// The IOP side of 989snd (see snd989_iop.h). The EE library's command ids are matched to the
// driver's calls by how the game uses them (their arguments and the callbacks of their replies):
//   0x11 play (bank, sound, vol, pan, pitch mod, pitch bend) -> the sound's handle, 0 if none
//   0x21 set (handle, flags 1 vol / 6 pan / 8 pitch mod / 0x10 pitch bend, vol, pan, pm, pb)
//        -> the handle while it plays, else 0
//   0x19 still playing (handle) -> the handle or 0;  0x15 stop (handle);  0x18 stop all
//   0x09 master volume (group, vol);  0x06 unload bank (handle);  0x08 resolve cross references
// The VAG streams' commands (0x00, 0x2a..0x38, 0x4f; 0x15 / 0x19 / 0x21 on a stream handle) go to
// snd989_stream.cpp. Commands not matched yet answer 0 and are reported once. Loader 0x57: load the bank at an EE
// address -> its handle.
//
// The host's mirror (ADR 0013): the game's IOP records what it is given (each command's argument words and its reply,
// each bank loaded, the end of each batch, each vsync) into a queue the audio thread replays on a second 989snd. Played
// in the same order at the same vsyncs, the mirror's output is the game's IOP's, sample for sample (RAC1_IOP_VERIFY=1
// compares them). When the game's thread stalls the audio thread renders the mirror alone (snd989_mirror_alone): its
// time then runs ahead of the game's, and the game's later commands reach it that much later, as they reached a PS2's
// IOP after a long EE frame. Its handles can then differ from the game's (a stream that ended while it ran alone frees
// its slot earlier): the commands' handles are translated (game -> mirror; equal until then).
#include <runtime/iop/snd989_iop.h>

#include <runtime/iop/snd989_stream.h>

#include <runtime/ipu/movie_hw.h>
#include <runtime/kernel/disc.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/sif/sif_rpc.h>

#include "game/sound/989snd/player.h"
#include "game/sound/989snd/util.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

namespace rac1 {

float g_snd989_gain = 1.0f;
bool g_snd989_trace = std::getenv("RAC1_SND_TRACE") != nullptr;

namespace {
constexpr uint32_t CMD_SERVER = 0x123456, LOADER_SERVER = 0x123457;
constexpr int kArgWords = 16;  // (the longest command, 0x2c, has 7)

// One IOP's 989snd: the game's, or the host's mirror.
struct Iop {
    explicit Iop(bool m) : mirror(m) {}
    bool mirror = false;
    std::unique_ptr<snd::Player> player;
    std::map<uint32_t, snd::BankHandle> banks;  // the handles the EE sees -> the driver's banks (the mirror: the game's handles)
    uint32_t next_bank = 0x00100000;
    std::set<uint32_t> reported;
    Snd989Streams* streams = nullptr;
    std::mutex render_lock;  // the player's lifetime against the renders
    std::map<uint32_t, uint32_t> handles;  // the mirror: the game's sound / stream handles -> its own
};
Iop& game() {
    static Iop i(false);
    return i;
}
Iop& mirror() {
    static Iop i(true);
    return i;
}

bool trace() { return g_snd989_trace; }

snd::BankHandle bank_of(Iop& io, uint32_t h) {
    const auto it = io.banks.find(h);
    return it == io.banks.end() ? nullptr : it->second;
}

// orig: (host) the IRX's command dispatch 0x15d4, on the argument words copied from the EE
int32_t command(Iop& io, uint32_t id, const uint32_t* a) {
    snd::Player& p = *io.player;
    if (trace() && !io.mirror && id != 0x09 && id != 0x19 && id != 0x21 && id != 0x32)
        std::fprintf(stderr, "[989snd] cmd 0x%x %08x %08x %08x %08x\n", id, a[0], a[1], a[2], a[3]);
    {  // the VAG streams (snd989_stream): their commands and those on a stream handle
        std::scoped_lock lock(io.render_lock);
        int32_t r = 0;
        if (snd989_stream_command(io.streams, p, id, a, r)) return r;
        if (id == 0x09) snd989_stream_master_volume(io.streams, a[0], static_cast<int32_t>(a[1]));
        if (id == 0x16 || id == 0x17) snd989_stream_group_pause(io.streams, a[0], id == 0x16);
    }
    auto s = [&](int i) { return static_cast<int32_t>(a[i]); };
    switch (id) {
    case 0x11: {
        snd::BankHandle b = bank_of(io, a[0]);
        if (b == nullptr) return 0;
        return static_cast<int32_t>(p.PlaySound(b, a[1], s(2), s(3), s(4), s(5)));
    }
    case 0x21: {
        const uint32_t h = a[0];
        uint32_t f = a[1];
        if (!p.SoundStillActive(h)) return 0;
        if ((f & 6) == 6) f &= ~2u;
        if ((f & 3) != 0) p.SetSoundVolPan(static_cast<s32>(h), (f & 1) ? s(2) : snd::VOLUME_DONT_CHANGE, (f & 2) ? s(3) : snd::PAN_DONT_CHANGE);
        if ((f & 4) != 0) p.AutoPan(static_cast<s32>(h), s(3), 0, 0x1e, 4);  // (0xb4c4: snd_AutoPan(h, pan, 0, 30, 4))
        if ((f & 8) != 0) p.SetSoundPmod(static_cast<s32>(h), s(4));
        if ((f & 0x10) != 0) p.SetSoundPBend(static_cast<s32>(h), s(5));
        return static_cast<int32_t>(h);
    }
    case 0x19: return p.SoundStillActive(a[0]) ? s(0) : 0;
    case 0x13: p.PauseSound(s(0)); return 0;
    case 0x14: p.ContinueSound(s(0)); return 0;
    case 0x15: p.StopSound(a[0]); return 0;
    case 0x16: p.PauseAllSoundsInGroup(static_cast<u8>(a[0])); return 0;
    case 0x17: p.ContinueAllSoundsInGroup(static_cast<u8>(a[0])); return 0;
    case 0x18: p.StopAllSounds(); return 0;
    case 0x09: p.SetMasterVolume(a[0], s(1)); return 0;
    case 0x0b: p.SetPlaybackMode(s(0)); return 0;
    case 0x0d: p.SetMixerMode(s(0), s(1)); return 0;  // snd_SetMixerMode (0x149e8): core 0 into core 1's reverb
    case 0x10:
        p.AutoReverb(static_cast<s16>(s(1)), s(2), s(3));
        return 0;
    case 0x4e: p.SetGroupVoiceRange(s(0), s(1), s(2)); return 0;  // snd_SetGroupVoiceRange (0x14ad4)
    case 0x50:
        p.SetReverbMode(s(1), s(3), s(4));
        p.SetReverbDepth(static_cast<s16>(s(2)), static_cast<s16>(s(2)));
        return 0;
    case 0x51: return 0;  // snd_SetPanMode
    case 0x06: {
        snd::BankHandle b = bank_of(io, a[0]);
        if (b != nullptr) p.UnloadBank(b);
        io.banks.erase(a[0]);
        return 0;
    }
    case 0x08: return 0;  // banks reference no others here
    default:
        if (!io.mirror && io.reported.insert(id).second) std::fprintf(stderr, "[989snd] command 0x%x not handled\n", id);
        return 0;
    }
}

// ---- the record for the mirror (written by the game's thread, read by the audio thread) ----

struct Op {
    enum Kind : uint8_t { Cmd, Bank, Batch, Vsync } kind = Cmd;
    uint32_t id = 0;
    uint32_t w[kArgWords] = {};
    int32_t reply = 0;
    std::shared_ptr<std::vector<u8>> bank;  // Bank: its bytes (w[0] its handle)
    std::vector<int16_t> movie;             // Vsync: the movie's raw samples
    float gl = 1.0f, gr = 1.0f;
    std::vector<int16_t> game_pcm;  // Vsync, RAC1_IOP_VERIFY: the game's mix
};
struct Record {
    std::atomic<bool> on{false};
    std::mutex m;
    std::condition_variable cv;
    std::deque<Op> ops;
    int vsyncs = 0;  // the Vsync ops queued
    // the audio thread's counters
    uint64_t rendered = 0, alone = 0, verify_equal = 0, verify_differ = 0, verify_first_differ = 0;
    bool verify = std::getenv("RAC1_IOP_VERIFY") != nullptr;
} g_rec;

void record(Op&& op) {
    {
        std::scoped_lock lock(g_rec.m);
        if (op.kind == Op::Vsync) ++g_rec.vsyncs;
        g_rec.ops.push_back(std::move(op));
    }
    g_rec.cv.notify_one();
}

// The mirror's command: the game's handles translated to its own (equal until it ran alone). The commands that only
// read (0x19, 0x30..0x32, 0x4f) and those whose effect is the EE's (0x00 the status word, 0x38 a disc read into the EE's
// RAM, 0x36 / 0x37 the reader's waits) change nothing in the mirror: skipped.
// orig: (host) ADR 0013
void mirror_command(Op& op) {
    Iop& m = mirror();
    auto tr = [&](uint32_t& h) {  // false: a handle the mirror has no sound for
        const auto it = m.handles.find(h);
        if (it != m.handles.end()) h = it->second;
        return h != 0;
    };
    uint32_t* w = op.w;
    const uint32_t queued_to = w[5];
    switch (op.id) {
    case 0x00: case 0x19: case 0x30: case 0x31: case 0x32: case 0x36: case 0x37: case 0x38: case 0x4f: return;
    case 0x13: case 0x14: case 0x15: case 0x21: case 0x2d: case 0x2e: case 0x2f:
        if (!tr(w[0])) return;
        break;
    case 0x2c:  // play / queue after a stream: the stream the mirror has for it (else one it cannot find: 0xeb7c's rule)
        if (w[5] != 0 && !tr(w[5])) w[5] = 0x04ff0000u;
        break;
    default: break;
    }
    const int32_t r = command(m, op.id, w);
    const uint32_t gr = static_cast<uint32_t>(op.reply), mr = static_cast<uint32_t>(r);
    if (op.id == 0x11 && gr != 0) m.handles[gr] = mr;
    if (op.id == 0x2c) {
        if (snd989_is_stream_handle(gr)) m.handles[gr] = mr;
        else if (gr == 1 && queued_to != 0 && snd989_is_stream_handle(mr)) m.handles[queued_to] = mr;  // (queued for the game, a new stream here)
    }
    while (m.handles.size() > 4096) m.handles.erase(m.handles.begin());  // (the sound handles only grow: the oldest long gone)
}

// orig: (host) one recorded op applied to the mirror (the audio thread)
void mirror_apply(Op& op) {
    Iop& m = mirror();
    switch (op.kind) {
    case Op::Cmd: mirror_command(op); break;
    case Op::Bank: {
        std::vector<u8> d = *op.bank;  // (the driver keeps its own copy)
        snd::BankHandle b = m.player->LoadBank(d);
        if (b != nullptr) m.banks[op.w[0]] = b;
        break;
    }
    case Op::Batch: {
        std::scoped_lock lock(m.render_lock);
        snd989_stream_update(m.streams);
        break;
    }
    case Op::Vsync: break;
    }
}

// `multi`: also the 5.1 / 7.1 speakers' output (`channels` 6 / 8 per frame; Synth::Tick), else nullptr
void render(Iop& io, int16_t* out, int frames, int16_t* multi = nullptr, int channels = 2) {
    std::scoped_lock lock(io.render_lock);
    if (multi == nullptr) channels = 2;
    if (!io.player) {
        std::memset(out, 0, static_cast<size_t>(frames) * 4);
        if (multi != nullptr) std::memset(multi, 0, static_cast<size_t>(frames * channels) * 2);
        return;
    }
    io.player->Tick(reinterpret_cast<snd::s16Output*>(out), frames, multi, channels);
    const float gain = g_snd989_gain;
    if (gain != 1.0f) {
        auto scale = [gain](int16_t* b, int n) {
            for (int i = 0; i < n; ++i) {
                const float v = static_cast<float>(b[i]) * gain;
                b[i] = static_cast<int16_t>(std::clamp(v, -32768.0f, 32767.0f));
            }
        };
        scale(out, frames * 2);
        if (multi != nullptr) scale(multi, frames * channels);
    }
}

// orig: (host) the movie's sound in a 5.1 / 7.1 output: on the front left / right (its stereo mix's samples)
void movie_add_multi(int16_t* multi, int channels, const int16_t* movie, int movie_frames, float gl, float gr) {
    int16_t st[800 * 2] = {};
    const int n = std::min(movie_frames, 800);
    movie_hw_audio_add(st, movie, n, gl, gr);
    for (int i = 0; i < n; ++i)
        for (int c = 0; c < 2; ++c) {
            int16_t& o = multi[i * channels + c];
            o = static_cast<int16_t>(std::clamp(o + st[i * 2 + c], -32768, 32767));
        }
}

// The command server: 0x4d a batch (word 0 the count, then u16 id, u16 size, the data padded
// to words), its replies [-1, results..., -1]; any other function one command, reply [-1, r, -1].
void cmd_server(uint32_t fno, uint32_t send, uint32_t ssize, uint32_t recv, uint32_t rsize) {
    (void)ssize;
    (void)rsize;
    Iop& io = game();
    const bool rec = g_rec.on.load(std::memory_order_relaxed);
    struct Refill {  // the streams' reader after the call (each frame's batch)
        Iop& io;
        bool rec;
        ~Refill() {
            {
                std::scoped_lock lock(io.render_lock);
                snd989_stream_update(io.streams);
            }
            if (rec) record(Op{Op::Batch});
        }
    } refill{io, rec};
    // one command: its argument words copied from the EE's buffer (the handlers read at most 7), run, recorded with its
    // reply
    auto one = [&](uint32_t id, uint32_t at) {
        Op op;
        op.id = id;
        for (uint32_t i = 0; i < kArgWords; ++i) op.w[i] = ee_read<uint32_t>(at + i * 4);
        op.reply = command(io, id, op.w);
        const int32_t r = op.reply;
        if (rec) record(std::move(op));
        return r;
    };
    if (fno == 0x4d) {
        const uint32_t n = ee_read<uint32_t>(send);
        uint32_t p = send + 4;
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t id = ee_read<uint16_t>(p), size = ee_read<uint16_t>(p + 2);
            ee_write<uint32_t>(recv + 4 + i * 4, static_cast<uint32_t>(one(id, p + 4)));
            p += 4 + ((size + 3) & ~3u);
        }
        ee_write<uint32_t>(recv, 0xffffffffu);
        ee_write<uint32_t>(recv + 4 + n * 4, 0xffffffffu);
        return;
    }
    ee_write<uint32_t>(recv + 4, static_cast<uint32_t>(one(fno, send)));
    ee_write<uint32_t>(recv, 0xffffffffu);
    ee_write<uint32_t>(recv + 8, 0xffffffffu);
}

// A bank handed to the driver (a 989snd file: type, chunk count, {offset, size} per chunk): its
// handle, 0 when it is not a bank. Recorded for the mirror.
uint32_t bank_load(std::vector<u8> d, const char* where, uint32_t at) {
    Iop& io = game();
    snd::BankHandle b = io.player->LoadBank(d);  // the driver keeps its own copy
    if (b == nullptr) {
        std::fprintf(stderr, "[989snd] no bank at %s 0x%08x\n", where, at);
        return 0;
    }
    const uint32_t handle = io.next_bank++;
    io.banks[handle] = b;
    if (g_rec.on.load(std::memory_order_relaxed)) {
        Op op{Op::Bank};
        op.w[0] = handle;
        op.bank = std::make_shared<std::vector<u8>>(std::move(d));
        record(std::move(op));
    }
    return handle;
}

// The loader: 0x57 the bank at the EE address in the send buffer; 3 the bank on the disc at the
// sector and byte offset in the send buffer (snd_BankLoadByLoc, the title's bank);
// its handle to the receive buffer, 0 when it is not a bank.
void loader_server(uint32_t fno, uint32_t send, uint32_t ssize, uint32_t recv, uint32_t rsize) {
    (void)ssize;
    (void)rsize;
    uint32_t handle = 0;
    auto word = [](uint32_t a) { return ee_read<uint32_t>(a); };
    if (fno == 3) {
        const uint32_t lsn = word(send), off = word(send + 4);
        std::vector<u8> head(2048 * 2);
        if (disc_read(lsn + off / 2048, 2, head.data())) {
            const uint32_t o = off % 2048;
            uint32_t chunks = 0;
            std::memcpy(&chunks, &head[o + 4], 4);
            uint32_t end = 8 + chunks * 8;
            for (uint32_t i = 0; i < chunks && i < 8 && o + 16 + i * 8 <= head.size(); ++i) {
                uint32_t co = 0, cs = 0;
                std::memcpy(&co, &head[o + 8 + i * 8], 4);
                std::memcpy(&cs, &head[o + 12 + i * 8], 4);
                end = std::max(end, co + cs);
            }
            std::vector<u8> all(((o + end + 2047) / 2048) * 2048);
            if (disc_read(lsn + off / 2048, static_cast<uint32_t>(all.size() / 2048), all.data())) {
                std::vector<u8> d(all.begin() + o, all.begin() + o + end);
                handle = bank_load(d, "sector", lsn);
            }
        }
    } else if (fno == 0x57) {
        const uint32_t addr = word(send);
        const uint32_t chunks = word(addr + 4);
        uint32_t end = 8 + chunks * 8;
        for (uint32_t i = 0; i < chunks && i < 8; ++i) end = std::max(end, word(addr + 8 + i * 8) + word(addr + 12 + i * 8));
        std::vector<u8> d(end);
        for (uint32_t i = 0; i < end; ++i) d[i] = ee_read<uint8_t>(addr + i);
        handle = bank_load(d, "EE", addr);
    } else if (game().reported.insert(0x10000 | fno).second) {
        std::fprintf(stderr, "[989snd] loader function 0x%x not handled\n", fno);
    }
    ee_write<uint32_t>(recv, handle);
}

void make(Iop& io) {
    std::scoped_lock lock(io.render_lock);
    if (!io.player) io.player = std::make_unique<snd::Player>();
    if (io.streams == nullptr) io.streams = snd989_streams(io.mirror);
}
}  // namespace

// orig: (host) see snd989_iop.h
void snd989_rand_use_game() { snd::RandUseGame(); }

void snd989_iop_start() {
    make(game());
    sif_rpc_register(CMD_SERVER, cmd_server);
    sif_rpc_register(LOADER_SERVER, loader_server);
}

void snd989_iop_stop() {
    for (Iop* io : {&game(), &mirror()}) {
        std::scoped_lock lock(io->render_lock);
        if (io->streams != nullptr) snd989_stream_reset(io->streams);
        io->player.reset();
        io->banks.clear();
    }
}

void snd989_iop_render(int16_t* out, int frames, int16_t* multi, int channels) { render(game(), out, frames, multi, channels); }

void snd989_movie_add_multi(int16_t* multi, int channels, const int16_t* movie, int movie_frames, float gl, float gr) {
    movie_add_multi(multi, channels, movie, movie_frames, gl, gr);
}

int snd989_iop_active_sounds() {
    Iop& io = game();
    std::scoped_lock lock(io.render_lock);
    return io.player ? io.player->ActiveSounds() : 0;
}

// ---- the mirror ----

// orig: (host) ADR 0013
void snd989_mirror_enable() {
    make(mirror());
    g_rec.on.store(true);
}

void snd989_mirror_disable() {
    g_rec.on.store(false);
    std::scoped_lock lock(g_rec.m);
    g_rec.ops.clear();
    g_rec.vsyncs = 0;
}

bool snd989_mirror_enabled() { return g_rec.on.load(std::memory_order_relaxed); }

// orig: (host) ADR 0013: a vsync of the game, recorded
void snd989_mirror_vsync(const int16_t* movie, int movie_frames, float movie_gl, float movie_gr, const int16_t* game_pcm) {
    if (!g_rec.on.load(std::memory_order_relaxed)) return;
    Op op{Op::Vsync};
    if (movie != nullptr && movie_frames > 0) op.movie.assign(movie, movie + movie_frames * 2);
    op.gl = movie_gl;
    op.gr = movie_gr;
    if (g_rec.verify && game_pcm != nullptr) op.game_pcm.assign(game_pcm, game_pcm + 800 * 2);
    record(std::move(op));
}

int snd989_mirror_queued() {
    std::scoped_lock lock(g_rec.m);
    return g_rec.vsyncs;
}

bool snd989_mirror_wait(double ms) {
    std::unique_lock lock(g_rec.m);
    return g_rec.cv.wait_for(lock, std::chrono::duration<double, std::milli>(ms), [] { return g_rec.vsyncs > 0; });
}

// orig: (host) ADR 0013: the mirror's next vsync
bool snd989_mirror_next(int16_t* out, int16_t* multi, int channels) {
    std::deque<Op> ops;
    {
        std::scoped_lock lock(g_rec.m);
        if (g_rec.vsyncs == 0) return false;
        while (!g_rec.ops.empty()) {
            const bool v = g_rec.ops.front().kind == Op::Vsync;
            ops.push_back(std::move(g_rec.ops.front()));
            g_rec.ops.pop_front();
            if (v) break;
        }
        --g_rec.vsyncs;
    }
    for (Op& op : ops) mirror_apply(op);
    Op& v = ops.back();
    render(mirror(), out, 800, multi, channels);
    if (!v.movie.empty()) {
        movie_hw_audio_add(out, v.movie.data(), static_cast<int>(v.movie.size() / 2), v.gl, v.gr);
        if (multi != nullptr) movie_add_multi(multi, channels, v.movie.data(), static_cast<int>(v.movie.size() / 2), v.gl, v.gr);
    }
    ++g_rec.rendered;
    if (!v.game_pcm.empty() && g_rec.alone == 0) {
        if (std::memcmp(v.game_pcm.data(), out, 800 * 4) == 0) {
            ++g_rec.verify_equal;
        } else if (g_rec.verify_differ++ == 0) {
            g_rec.verify_first_differ = g_rec.rendered;
        }
    }
    return true;
}

// orig: (host) ADR 0013: the IOP running while the EE is busy
void snd989_mirror_alone(int16_t* out, int16_t* multi, int channels) {
    std::deque<Op> ops;
    {
        std::scoped_lock lock(g_rec.m);
        while (!g_rec.ops.empty() && g_rec.ops.front().kind != Op::Vsync) {  // (a vsync queued meanwhile: the game is back)
            ops.push_back(std::move(g_rec.ops.front()));
            g_rec.ops.pop_front();
        }
    }
    for (Op& op : ops) mirror_apply(op);
    render(mirror(), out, 800, multi, channels);
    {
        Iop& m = mirror();
        std::scoped_lock lock(m.render_lock);
        snd989_stream_update(m.streams);
    }
    ++g_rec.alone;
}

std::string snd989_streams_describe(bool of_mirror) {
    Iop& io = of_mirror ? mirror() : game();
    std::scoped_lock lock(io.render_lock);
    return io.streams != nullptr ? snd989_stream_describe(io.streams) : std::string();
}

void snd989_mirror_report() {
    if (!g_rec.on.load()) return;
    std::fprintf(stderr, "[audio] the IOP's mirror: %llu vsyncs of the game played, %llu frames played alone (the game's thread stalled)",
                 static_cast<unsigned long long>(g_rec.rendered), static_cast<unsigned long long>(g_rec.alone));
    if (g_rec.verify)
        std::fprintf(stderr, "; RAC1_IOP_VERIFY: %llu vsyncs equal to the game's IOP, %llu not (the first at vsync %llu)", static_cast<unsigned long long>(g_rec.verify_equal),
                     static_cast<unsigned long long>(g_rec.verify_differ), static_cast<unsigned long long>(g_rec.verify_first_differ));
    std::fprintf(stderr, "\n");
}

}  // namespace rac1
