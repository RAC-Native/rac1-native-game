// The IOP side of 989snd's VAG streams on the host (see snd989_stream.h). What the IRX does
// (989snd.irx from the ISO's globals/irx.bin, analysed by disassembly):
//   - 0x2a (0xacc -> 0xe1f0): `n` streams, a buffer of `size` bytes each (a multiple of 0x1000,
//     read in halves of size / 2), the reader threads; answers non-zero when up.
//   - 0x2c (0xb18 -> 0xeacc): play the VAG at sector loc (+ offset): a stream record (0x50 bytes)
//     with the handle 0x04000000 | index << 16 | serial, volume vol * 127 >> 10, the volume group;
//     flag 1 starts it paused, 4 loops it, `queue` != 0 appends it to that stream (the answer 1);
//     the reader reads the 0x30-byte "VAGp" header (big endian: +0xc the data size, +0x10 the
//     rate; the voice's pitch rate * 4096 / 48000) and the data after it.
//   - 0x2d pause (0xf5bc: the voices' volume and pitch to 0), 0x2e continue (0xf7ec), 0x2f stop
//     (0xfb30), also 0x15 on a stream handle; 0x21 on one sets its volume / pan (0x14354).
//   - 0x30 / 0x31 / 0x32 (0x1070c / 0x10860 / 0x107e0): played / total / left time in 1/1024 s.
//   - 0x4f (0x108f8): the record's flag 8 (its first buffer in, keyed on), -1 when no stream.
//   - 0x38 (0xda8 -> 0x10da0): a disc read for the game (sector, count, EE address) by the
//     reader, then the status word given by command 0 cleared and its error at +0x10 (0x9768).
// The host model: the disc read at once (the ISO), the SPU buffer of each stream a host array
// played by an SPU voice of OpenGOAL's synth (its ADPCM decoder, interpolation, envelope), the
// halves refilled when the voice has left them (the IRX's SPU interrupt), on the EE's thread.
#if defined(__WIIU__)
#include "platform/per_thread.h"
#endif
#include <runtime/iop/snd989_stream.h>

#include <runtime/kernel/disc.h>
#include <runtime/kernel/ee_memory.h>

#include "game/sound/989snd/player.h"
#include "game/sound/989snd/util.h"
#include "game/sound/common/voice.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace rac1 {

extern bool g_snd989_trace;

namespace {
constexpr uint32_t kSector = 2048;
constexpr uint32_t kRunSectors = 32;

struct Part {
    uint32_t lsn = 0, offset = 0;
    bool loop = false;
};

struct Stream {
    uint32_t handle = 0;  // 0: free
    uint16_t serial = 0;
    bool paused = false;        // the record's flag 2 (0x2c flag 1, 0x2d / 0x2e, 0x13 / 0x14)
    bool group_paused = false;  // paused by group mask (0x16 / 0x17)
    bool started = false;       // flag 8
    int32_t vol = 0, pan = 0, group = 0;
    std::deque<Part> queue;
    Part part;
    uint32_t rate = 0, pitch = 0, size = 0;  // the playing part's header
    uint32_t cursor = 0;                     // its data bytes copied to the SPU buffer
    bool data_end = false;                   // the end block written
    std::vector<uint16_t> spu;               // the SPU buffer: two halves
    int half = 0;                            // the half the voice plays
    uint64_t played = 0;                     // ADPCM bytes played (whole halves)
    uint64_t copied = 0, decoded_samples = 0;
    std::shared_ptr<snd::Voice> voice;
};

}  // namespace

// One IOP's streams (ADR 0013: the game's, and the host's mirror on the audio thread).
struct Snd989Streams {
    bool mirror = false;  // the host's mirror: no EE writes, no log
    bool up = false;
    uint32_t size = 0;  // the buffer per stream
    std::vector<Stream> streams;
    uint32_t status_addr = 0;  // command 0: the EE word cleared when a game read is done
    std::array<int32_t, 16> master{};
    // disc_bytes' sectors: a run of up to kRunSectors read at once (one read where the IRX's sceCdRead asked sector by sector - the same
    // bytes; a disc read costs its own time on a slow medium, the Wii U's SD card: a stream's start read a sector at a time stalled the game)
    uint32_t cached = 0xffffffffu, cached_n = 0;
    uint8_t sec[kSector * kRunSectors];
    Snd989Streams() { master.fill(0x400); }
};

namespace {
// The instance the calls of this thread work on (set by the entry points below; the game's by default).
Snd989Streams& game_streams() {
    static Snd989Streams s;
    return s;
}
#if defined(__WIIU__)  // (no thread-local storage there: per OS thread all the same, platform/per_thread.h)
rac1::platform::PerThread<Snd989Streams*> g_cur;
Snd989Streams*& cur() { return g_cur.get(); }
#else
thread_local Snd989Streams* t_cur = nullptr;
Snd989Streams*& cur() { return t_cur; }
#endif
Snd989Streams& st() { return cur() != nullptr ? *cur() : game_streams(); }
struct Use {
    Snd989Streams* prev;
    explicit Use(Snd989Streams* s) : prev(cur()) { cur() = s; }
    ~Use() { cur() = prev; }
};
bool quiet() { return st().mirror; }

uint32_t be32(const uint8_t* p) { return (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | p[3]; }

// orig: (host) bytes of the disc from byte `pos`, read a sector at a time (the IRX: sceCdRead)
bool disc_bytes(uint64_t pos, uint8_t* dst, uint32_t n) {
    uint32_t& cached = st().cached;
    uint32_t& cached_n = st().cached_n;
    uint8_t* sec = st().sec;
    while (n != 0) {
        const uint32_t lba = static_cast<uint32_t>(pos / kSector), o = static_cast<uint32_t>(pos % kSector);
        if (cached == 0xffffffffu || lba < cached || lba >= cached + cached_n) {
            if (disc_read(lba, kRunSectors, sec)) {
                cached_n = kRunSectors;
            } else {  // (a run past the disc's end: the sector alone)
                if (!disc_read(lba, 1, sec)) return false;
                cached_n = 1;
            }
            cached = lba;
        }
        const uint32_t k = std::min(n, kSector - o);
        std::memcpy(dst, sec + static_cast<size_t>(lba - cached) * kSector + o, k);
        dst += k;
        pos += k;
        n -= k;
    }
    return true;
}

// The voice's volume pair: 989snd's snd_MakeVolume and the volume group's master volume, the
// formulas of OpenGOAL's VoiceManager::MakeVolume / AdjustVolToGroup (the SFX voices' too).
// orig: (host) OpenGOAL vagvoice.cpp, as 0xeacc's voices
snd::VolPair stream_volume(const Stream& s) {
    int32_t v = 127 * 258;
    v = v * s.vol / 0x7f;
    v = v * 127 / 0x7f;
    if (v == 0) return {0, 0};
    int pan = s.pan;
    while (pan >= 360) pan -= 360;
    while (pan < 0) pan += 360;
    pan = pan >= 270 ? pan - 270 : pan + 90;
    int32_t l, r;
    if (pan < 180) {
        l = snd::normalPanTable[pan].left * v / 0x3fff;
        r = snd::normalPanTable[pan].right * v / 0x3fff;
    } else {
        r = snd::normalPanTable[pan - 180].left * v / 0x3fff;
        l = snd::normalPanTable[pan - 180].right * v / 0x3fff;
    }
    auto group = [&](int32_t in) -> int16_t {
        int32_t x = in;
        if (s.group >= 15) return static_cast<int16_t>(x);
        if (x >= 0x7fff) x = 0x7ffe;
        x = x * st().master[static_cast<size_t>(s.group)] / 0x400;
        const int sign = x < 0 ? -1 : 1;
        return static_cast<int16_t>(x * x / 0x7ffe * sign);
    };
    return {static_cast<int16_t>(group(l) >> 1), static_cast<int16_t>(group(r) >> 1)};
}

// the voice's volume and pitch: 0 while paused (0xf5bc), else the stream's (0xf7ec)
// orig: (host) the IRX's sceSdSetParam of VOLL / VOLR / PITCH in 0xf5bc / 0xf7ec
void voice_apply(Stream& s) {
    if (!s.voice) return;
    if (s.paused || s.group_paused) {
        s.voice->SetVolume(0, 0);
        s.voice->SetPitch(0);
        return;
    }
    const snd::VolPair v = stream_volume(s);
    s.voice->SetVolume(static_cast<uint16_t>(v.left), static_cast<uint16_t>(v.right));
    // (host) in 5.1 / 7.1 the dialogue (group 2) on the centre speaker, its pan towards the front left / right; the
    // others (the music) on the front left / right
    s.voice->SetPlace(s.group == 2 ? snd::Voice::Place::Centre : snd::Voice::Place::Front, s.pan);
    s.voice->SetPitch(static_cast<uint16_t>(s.pitch));
}

// The part's 0x30-byte header: "VAGp", big endian size (+0xc) and rate (+0x10); the pitch as
// the IRX computes it (rate << 12, times 0x057619f1, >> 42: rate * 4096 / 48000).
// orig: (host) the header parse of the reader 0x116e8 (0x11b0c..0x11bdc)
bool part_open(Stream& s, const Part& p) {
    uint8_t h[0x30];
    if (!disc_bytes(uint64_t{p.lsn} * kSector + p.offset, h, sizeof h) || std::memcmp(h, "VAGp", 4) != 0) {
        if (!quiet()) std::fprintf(stderr, "[989snd] stream: no VAGp header at sector 0x%x + %u\n", p.lsn, p.offset);
        return false;
    }
    s.part = p;
    s.size = be32(h + 0xc);
    s.rate = be32(h + 0x10);
    s.pitch = static_cast<uint32_t>((uint64_t{s.rate << 12} * 0x057619f1u) >> 42);
    s.cursor = 0;
    if (!quiet()) {
        const uint64_t samples = uint64_t{s.size} / 16 * 28;
        std::fprintf(stderr, "[989snd] stream %08x: VAGp at sector 0x%x, %u bytes, %u Hz (pitch 0x%x), %llu samples (%.2f s)%s\n", s.handle,
                     p.lsn, s.size, s.rate, s.pitch, static_cast<unsigned long long>(samples),
                     s.rate != 0 ? static_cast<double>(samples) / s.rate : 0.0, p.loop ? ", looped" : "");
    }
    return true;
}

// One half of the SPU buffer filled with the next 16-byte ADPCM blocks: a part's end goes on to
// its loop or the queued part; with none an end block (flag 1) stops the voice. The data blocks'
// own flags cleared; the buffer's first block loop start (4), its last loop end + repeat (3).
// orig: (host) the reader's SPU transfers (0x116e8 / 0x1295c) and the buffer loop of 0xeacc
void fill_half(Stream& s, int h) {
    const uint32_t words = static_cast<uint32_t>(s.spu.size()) / 2;
    uint16_t* dst = s.spu.data() + static_cast<size_t>(h) * words;
    uint8_t blk[16];
    for (uint32_t w = 0; w < words; w += 8) {
        bool end_block = false;
        if (s.data_end) {
            std::memset(blk, 0, sizeof blk);
        } else {
            while (s.cursor + 16 > s.size) {
                if (s.part.loop) {
                    s.cursor = 0;
                    if (s.size < 16) break;
                } else if (!s.queue.empty()) {
                    const Part next = s.queue.front();
                    s.queue.pop_front();
                    if (!part_open(s, next)) break;
                } else {
                    break;
                }
            }
            if (s.cursor + 16 > s.size) {
                std::memset(blk, 0, sizeof blk);
                blk[1] = 1;
                s.data_end = true;
                end_block = true;
            } else if (!disc_bytes(uint64_t{s.part.lsn} * kSector + s.part.offset + 0x30 + s.cursor, blk, 16)) {
                std::memset(blk, 0, sizeof blk);
                blk[1] = 1;
                s.data_end = true;
                end_block = true;
            } else {
                blk[1] = 0;
                s.cursor += 16;
                s.copied += 16;
            }
        }
        if (!end_block) {
            if (h == 0 && w == 0) blk[1] |= 4;
            if (h == 1 && w + 8 >= words) blk[1] |= 3;
        }
        for (int k = 0; k < 8; ++k) dst[w + k] = static_cast<uint16_t>(blk[2 * k] | blk[2 * k + 1] << 8);  // (16-bit words, the host's order)
    }
}

// the stream record of a handle (0xcadc: type 4, index in bits 16..23, the record's handle equal)
// orig: (host) as the IRX's 0xcadc
Stream* find(uint32_t h) {
    if (!snd989_is_stream_handle(h)) return nullptr;
    const uint32_t i = (h >> 16) & 0xff;
    if (i >= st().streams.size()) return nullptr;
    Stream& s = st().streams[i];
    return s.handle == h ? &s : nullptr;
}

// the record freed, its voice stopped (the handle no longer found: 0x19 answers 0)
// orig: (host) the end of a stream in the IRX (0xfb30 / the reader at the data's end)
void stream_free(Stream& s, const char* why) {
    if (s.voice) s.voice->Stop();
    s.voice.reset();
    if (s.handle != 0 && !quiet())
        std::fprintf(stderr, "[989snd] stream %08x freed (%s): %llu ADPCM bytes queued, %llu played\n", s.handle, why,
                     static_cast<unsigned long long>(s.copied), static_cast<unsigned long long>(s.played));
    s.handle = 0;
    s.queue.clear();
}

// 0x2c: play / queue
// orig: (host) the IRX's 0xeacc
int32_t stream_play(snd::Player& p, const uint32_t* args) {
    const uint32_t loc1 = args[0], loc2 = args[1], w2 = args[2], w3 = args[3];
    const int32_t group = static_cast<int32_t>(args[4]);
    const uint32_t queue = args[5], flags = args[6];
    const int32_t vol = static_cast<int16_t>(w2 >> 16), pan = static_cast<int16_t>(w3 >> 16);
    const Part part = {loc1, w2 & 0xffff, (flags & 4) != 0};
    if (!st().up) return 0;
    if (loc2 != 0 && !quiet()) std::fprintf(stderr, "[989snd] stream: a second channel (sector 0x%x) not played (host: mono only)\n", loc2);
    if (queue != 0) {
        Stream* s = find(queue);
        if (s == nullptr && (flags & 0x10) != 0) return 0;  // 0xeb7c: else it starts as a new stream
    }
    if (queue != 0 && find(queue) != nullptr) {
        Stream* s = find(queue);
        s->queue.push_back(part);
        if (g_snd989_trace && !quiet()) std::fprintf(stderr, "[989snd] stream %08x: sector 0x%x queued (flags %x)\n", queue, loc1, flags);
        return 1;
    }
    Stream* s = nullptr;
    for (size_t i = 0; i < st().streams.size(); ++i) {
        if (st().streams[i].handle == 0) {
            s = &st().streams[i];
            s->handle = 0x04000000u | static_cast<uint32_t>(i) << 16 | ++s->serial;
            break;
        }
    }
    if (s == nullptr) return 0;
    s->vol = (vol * 128 - vol) >> 10;  // 0xed28
    s->pan = (static_cast<uint32_t>(pan + 2) < 2u) ? 0 : pan;
    s->group = group;
    s->paused = (flags & 1) != 0;
    s->group_paused = false;
    s->data_end = false;
    s->played = s->copied = 0;
    s->queue.clear();
    if (!part_open(*s, part)) {
        s->handle = 0;
        return 0;
    }
    s->spu.assign(st().size / 2 / 2, 0);  // half the stream's buffer, in 16-bit words
    fill_half(*s, 0);
    fill_half(*s, 1);
    s->half = 0;
    s->voice = std::make_shared<snd::Voice>();
    s->voice->SetAsdr1(0x000f);  // the IRX's stream voices (0xa3d8 / 0xa430)
    s->voice->SetAsdr2(0x1fc0);
    s->voice->SetSample(s->spu.data());
    s->voice->SetReverb((flags & 2) != 0);  // (the record's 0x100: the voice's VMIXE, 0xef8c / 0x1372c)
    p.AllocVoice(*s->voice, group, 0x7f);   // (its SPU2 voice, 0xe948: the group's range; its core)
    s->voice->KeyOn();
    voice_apply(*s);
    std::shared_ptr<snd::Voice> v = s->voice;
    p.SubmitVoice(v);
    s->started = true;  // the first buffer is in (host: at once)
    if (!quiet()) std::fprintf(stderr, "[989snd] stream %08x play: sector 0x%x vol %d pan %d group %d flags %x%s\n", s->handle, loc1, s->vol, s->pan, group,
                 flags, s->paused ? " (paused)" : "");
    return static_cast<int32_t>(s->handle);
}

// time in 1/1024 s of `samples` at the part's rate
// orig: (host) the arithmetic of the IRX's 0x1070c / 0x10860
int32_t time_of(uint64_t samples, uint32_t rate) {
    if (rate == 0) return 0;
    const uint32_t s = static_cast<uint32_t>(samples);
    return static_cast<int32_t>((s / rate << 10) + ((s % rate) << 10) / rate);
}
// the samples played of the part (host: whole SPU halves, an estimate of the IRX's +0x98 bytes)
// orig: (host)
uint64_t played_samples(const Stream& s) {
    const uint32_t b = static_cast<uint32_t>(s.played % (s.size == 0 ? 1 : s.size));
    return uint64_t{b >> 4} * 28;
}
}  // namespace

// orig: (host) the streams of an IOP: the game's (nullptr), or a new one for the host's mirror
Snd989Streams* snd989_streams(bool mirror) {
    if (!mirror) return &game_streams();
    Snd989Streams* s = new Snd989Streams;
    s->mirror = true;
    return s;
}

// orig: (host) the IRX's handle type (bits 24..28) of 0xcadc
bool snd989_is_stream_handle(uint32_t h) { return h != 0xffffffffu && ((h >> 24) & 0x1f) == 4; }

// The movie player's sound. The EE side 0x12f068 (command 0x3b) is called by the player 0x2ee540 with
// (0x400, 0x1000, volume 0x400, pan 0, group 5, mode 3): the IRX (0x9c90) keeps the volume as 0x400 * 127 >> 10 = 127,
// the pan 0 and the group 5; the start (0x3e -> 0xa2c0) sets, for a stereo track, the first voice's VOLL to the left
// pan volume and the second voice's VOLR to the right one, each through the group's master volume (0x16dcc) and >> 1,
// i.e. a stream's voice (stream_volume) with volume 127, pan 0, group 5.
// orig: (host) the IRX's 0xa2c0 volumes of the movie's voices
void snd989_movie_gain(float& left, float& right) {
    Stream s;
    s.vol = 127;
    s.pan = 0;
    s.group = 5;
    const snd::VolPair v = stream_volume(s);
    left = static_cast<float>(v.left) / 16384.0f;   // (the SPU2's non-sweep voice volume: register << 1, / 0x8000)
    right = static_cast<float>(v.right) / 16384.0f;
}

// orig: (host) 989snd's master volume of a group (command 0x09) applied to the stream voices
void snd989_stream_master_volume(Snd989Streams* io, uint32_t group, int32_t vol) {
    Use use(io);
    if (group >= st().master.size()) return;
    const int32_t clamped = vol < 0 ? 0 : (vol > 0x400 ? 0x400 : vol);
    if (st().master[group] == clamped) return;
    st().master[group] = clamped;
    for (Stream& s : st().streams)
        if (s.handle != 0 && s.group == static_cast<int32_t>(group)) voice_apply(s);
}

// orig: (host) 989snd's group pause / continue (commands 0x16 / 0x17, 0xcd2c / 0xce74) applied to the stream voices
void snd989_stream_group_pause(Snd989Streams* io, uint32_t mask, bool pause) {
    Use use(io);
    for (Stream& s : st().streams) {
        if (s.handle != 0 && s.group >= 0 && s.group < 32 && ((mask >> s.group) & 1u) != 0) {
            s.group_paused = pause;
            voice_apply(s);
        }
    }
}

// The stream commands, the IRX's handlers by the table 0x17bdc (see the file's head).
// orig: (host) the IRX's command dispatch 0x15d4 for the stream ids
bool snd989_stream_command(Snd989Streams* io, snd::Player& p, uint32_t id, const uint32_t* args, int32_t& result) {
    Use use(io);
    auto a = [&](int i) { return args[i]; };
    result = 0;
    switch (id) {
    case 0x00:  // the EE's stream status word
        st().status_addr = a(0);
        return true;
    case 0x2a: {  // 0xe1f0: streams, buffer size
        const uint32_t n = a(0);
        uint32_t size = a(1) / 0x1000;
        if (size < 2) size = 2;
        st().size = size * 0x1000;
        for (Stream& s : st().streams) stream_free(s, "init");
        st().streams.assign(n, Stream{});
        st().up = true;
        if (!quiet()) std::fprintf(stderr, "[989snd] streams up: %u of 0x%x bytes\n", n, st().size);
        result = 1;
        return true;
    }
    case 0x2c: result = stream_play(p, args); return true;
    case 0x13:
    case 0x14:
        if (!snd989_is_stream_handle(a(0))) return false;
        [[fallthrough]];
    case 0x2d:
    case 0x2e: {  // pause / continue
        if (Stream* s = find(a(0))) {
            s->paused = id == 0x2d || id == 0x13;
            voice_apply(*s);
            if (g_snd989_trace && !quiet()) std::fprintf(stderr, "[989snd] stream %08x %s\n", s->handle, s->paused ? "paused" : "continued");
        }
        return true;
    }
    case 0x2f:
    case 0x15:  // stop (0xfb30; 0x15 on a stream handle: 0xcfe8)
        if (id == 0x15 && !snd989_is_stream_handle(a(0))) return false;
        if (Stream* s = find(a(0))) stream_free(*s, id == 0x15 ? "0x15" : "0x2f");
        return true;
    case 0x19:  // still playing (0xb278): the handle while the record has it
        if (!snd989_is_stream_handle(a(0))) return false;
        result = find(a(0)) != nullptr ? static_cast<int32_t>(a(0)) : 0;
        return true;
    case 0x21: {  // set volume / pan (0xb428 -> 0x14354)
        if (!snd989_is_stream_handle(a(0))) return false;
        Stream* s = find(a(0));
        if (s == nullptr) return true;
        uint32_t f = a(1);
        if ((f & 6) == 6) f &= ~2u;
        if ((f & 1) != 0) {
            const int32_t v = static_cast<int32_t>(a(2));
            s->vol = v < 0 ? -v : ((v * 128 - v) >> 10);
            if (s->vol > 0x7f) s->vol = 0x7f;
        }
        if ((f & 2) != 0) s->pan = static_cast<int32_t>(a(3)) == -1 ? 0 : static_cast<int32_t>(a(3));
        voice_apply(*s);
        result = static_cast<int32_t>(s->handle);
        return true;
    }
    case 0x30:
    case 0x31:
    case 0x32: {  // played / length / left, 1/1024 s
        Stream* s = find(a(0));
        if (s == nullptr || s->rate == 0) return true;
        const int32_t played = time_of(played_samples(*s), s->rate), total = time_of(uint64_t{s->size} / 16 * 28, s->rate);
        result = id == 0x30 ? played : id == 0x31 ? total : total - played;
        return true;
    }
    case 0x34:  // stop all
        for (Stream& s : st().streams) stream_free(s, "0x34");
        return true;
    case 0x36: result = 1; return true;  // the reader idle (the host reads at once)
    case 0x37: return true;              // a read broken off (none in flight)
    case 0x38: {  // a disc read for the game: sector, count, EE address
        if (st().mirror) return true;  // (the EE's RAM is the game's IOP's to write)
        const bool ok = disc_read_ee(a(2), a(0), a(1));
        if (st().status_addr != 0) {
            ee_write<uint32_t>(st().status_addr + 0x10, ok ? 0u : 0xffffffffu);
            ee_write<uint32_t>(st().status_addr, 0);
        }
        return true;
    }
    case 0x4f: {  // started (0x108f8)
        Stream* s = find(a(0));
        result = s == nullptr ? -1 : (s->started ? 1 : 0);
        return true;
    }
    default: return false;
    }
}

// orig: (host) the IRX's SPU interrupt at a half's end waking the reader (0x116e8)
void snd989_stream_update(Snd989Streams* io) {
    Use use(io);
    for (Stream& s : st().streams) {
        if (s.handle == 0 || !s.voice) continue;
        if (s.voice->Dead()) {  // the end block played: the stream is over
            stream_free(s, "ended");
            continue;
        }
        const uint32_t half_words = static_cast<uint32_t>(s.spu.size()) / 2;
        const int h = s.voice->GetNax() >= half_words ? 1 : 0;
        if (h != s.half) {
            fill_half(s, s.half);
            s.played += half_words * 2;
            s.half = h;
        }
    }
}

// orig: (host)
void snd989_stream_reset(Snd989Streams* io) {
    Use use(io);
    for (Stream& s : st().streams) stream_free(s, "reset");
    st().streams.clear();
    st().up = false;
}

// (testing) the streams playing, as text: "group:sector[p]" per live stream (p: paused), in slot order
std::string snd989_stream_describe(Snd989Streams* io) {
    Use use(io);
    std::string out;
    char b[48];
    for (const Stream& s : st().streams) {
        if (s.handle == 0) continue;
        std::snprintf(b, sizeof b, "%s%d:%x%s", out.empty() ? "" : " ", s.group, s.part.lsn, (s.paused || s.group_paused) ? "p" : "");
        out += b;
    }
    return out;
}

}  // namespace rac1
