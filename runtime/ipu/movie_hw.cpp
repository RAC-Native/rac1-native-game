// The movie hardware stand-in: PSS demux, MPEG-2 pictures (built-in mpeg2_dec or libavcodec), SPU2 ADPCM
// sound. See movie_hw.h.
#include <runtime/ipu/movie_hw.h>
#include <runtime/ipu/mpeg2_dec.h>

#include <runtime/kernel/disc.h>
#include <runtime/kernel/load_prof.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#if defined(__WIIU__)
#include <coreinit/thread.h>
#endif

#ifdef RAC1_HAVE_AVCODEC
extern "C" {
#include <libavcodec/avcodec.h>
}
#endif

namespace rac1 {

namespace {

struct Movie {
    bool open = false;
    // the video elementary stream (PES 0xe0 payloads): sized to the file's bytes at the open (never reallocated while the
    // loader writes it), its first `video_len` bytes demultiplexed so far
    std::vector<uint8_t> video;
    std::atomic<size_t> video_len{0};
    size_t video_pos = 0;
    bool video_ended = false;
    int width = 0, height = 0;
    double fps = 30.0;
    std::vector<int16_t> audio;  // the chosen track, 48 kHz stereo (g_audio_lock), decoded so far
    size_t audio_pos = 0;        // sample frames played
    // the file's loader: the rest of the file read, demultiplexed and its sound decoded on a worker thread after
    // the open; `video_done` / `audio_done`: all of it there (what a read of the whole file at the open gave)
    std::thread loader;
    std::atomic<bool> stop{false}, video_done{false}, audio_done{false};
    std::mutex lk;  // (with cv: the video's progress)
    std::condition_variable cv;
    bool use_builtin = true;
    mpeg2::Decoder mpeg2_dec;
#ifdef RAC1_HAVE_AVCODEC
    const AVCodec* codec = nullptr;
    AVCodecContext* ctx = nullptr;
    AVCodecParserContext* parser = nullptr;
    AVPacket* pkt = nullptr;
    AVFrame* frame = nullptr;
    bool flushed = false;
#endif
};

Movie g_movie;
bool g_enabled = true;
std::mutex g_audio_lock;
std::condition_variable g_audio_cv;  // (the sound decoded further, or all of it)
std::vector<uint8_t> g_shown;
int g_shown_w = 0, g_shown_h = 0;
uint64_t g_shown_serial = 0;

uint32_t be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
uint32_t le32(const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }

// The PSS's packets: MPEG-2 program stream packs (0x1ba), system headers (0x1bb), PES packets
// with a 16-bit length. Video 0xe0 to `video`; private stream 1 (0xbd) payloads start with
// 0xff 0xa1 0x00 <track>, each track a "SShd" header then "SSbd" and its ADPCM data.
// As the file's bytes come: `step` takes the packets whose bytes are all in the first `avail` bytes of `d` and
// stops at the first that is not (more to come); with `complete` the file's end is `avail` and the rest is taken as a demux
// of the whole file takes it (cut at the end). The packets and their payloads are those of a demux of the whole file.
struct Demux {
    size_t i = 0;
    bool ended = false;  // the program end, or the file's end
    template <class Video, class Track>
    void step(const uint8_t* d, size_t avail, bool complete, Video&& video, Track&& track_data) {
        while (!ended) {
            auto more = [&](size_t need) {  // false: wait for the bytes up to `need` (the file's end then ends)
                if (need <= avail) return true;
                if (complete) ended = true;
                return false;
            };
            if (!more(i + 4)) return;
            if (!(d[i] == 0 && d[i + 1] == 0 && d[i + 2] == 1)) {
                ++i;
                continue;
            }
            const uint8_t id = d[i + 3];
            if (id == 0xba) {  // pack header (MPEG-2: 14 bytes + stuffing)
                if (!more(i + 14)) return;
                const size_t pack_len = 14 + (d[i + 13] & 7);
                if (!more(i + pack_len)) return;
                i += pack_len;
                continue;
            }
            if (id == 0xb9) {  // program end
                ended = true;
                return;
            }
            if (!more(i + 6)) return;
            const size_t len = (size_t(d[i + 4]) << 8) | d[i + 5];
            if (!complete && std::max(i + 6 + len, i + 9) > avail) return;  // (the bytes the packet's parse reads: wait for them)
            const size_t end = std::min(avail, i + 6 + len);
            if (id == 0xe0 || id == 0xbd) {
                const size_t hl = i + 8 < avail ? d[i + 8] : 0;
                const size_t p = i + 9 + hl;
                if (p < end) {
                    if (id == 0xe0) {
                        video(d + p, end - p);
                    } else if (end - p > 4) {
                        const int track = d[p + 3];
                        if (track >= 0 && track < 16) track_data(track, d + p + 4, end - (p + 4));
                    }
                }
            }
            i = end;
        }
    }
};

// A track's "SShd" (size 0x18: type 0x10 ADPCM, rate, channels, interleave) and "SSbd" (size),
// then the data: `interleave` bytes of each channel in turn.
// Decoded as the track's bytes come: `step` decodes the interleave units all there, `complete` (the whole track
// there) the rest; the samples handed to `out` are those of a decode of the whole track (the resampling to 48 kHz of a
// sample waits for the one after it, the last ones for the track's end).
struct TrackDecoder {
    int phase = 0;  // 0 the header not there yet, 1 decoding, 2 done (or not decodable)
    uint32_t rate = 0, chans = 0, inter = 0;
    size_t b = 0, off = 0, out_i = 0;
    int32_t h[2][2] = {};
    std::vector<int16_t> pcm;  // at the track's rate (rate != 48000)
    // `t`: the track's bytes so far; `out(samples, n)`: n stereo frames more of the 48 kHz track
    template <class Out>
    void step(const std::vector<uint8_t>& t, bool complete, Out&& out) {
        if (phase == 0) {
            if (t.size() < 0x28) {
                if (complete) phase = 2;
                return;
            }
            if (std::memcmp(t.data(), "SShd", 4) != 0) {
                phase = 2;
                return;
            }
            const uint32_t hsize = le32(&t[4]);
            const uint32_t type = le32(&t[8]);
            rate = le32(&t[12]);
            chans = le32(&t[16]);
            inter = le32(&t[20]);
            b = 8 + hsize;
            if (b + 8 > t.size()) {
                if (complete) phase = 2;
                return;
            }
            if (std::memcmp(&t[b], "SSbd", 4) != 0) {
                phase = 2;
                return;
            }
            if (type != 0x10 || chans < 1 || chans > 2 || inter == 0 || (inter % 16) != 0) {
                std::fprintf(stderr, "[movie] sound track type %#x %u ch, interleave %#x: not decoded\n", type, chans, inter);
                phase = 2;
                return;
            }
            phase = 1;
        }
        if (phase != 1) return;
        const uint8_t* p = &t[b + 8];
        const size_t n = std::min<size_t>(le32(&t[b + 4]), t.size() - (b + 8));
        const size_t per = inter * chans;
        const size_t samples_per = inter / 16 * 28;
        int16_t blk[28];
        std::vector<int16_t> o48;
        for (; off + per <= n; off += per) {
            std::vector<int16_t>& out_ = rate == 48000 ? o48 : pcm;
            const size_t base = out_.size();
            out_.resize(base + samples_per * 2);
            for (uint32_t c = 0; c < chans; ++c) {
                for (size_t k = 0; k < inter / 16; ++k) {
                    spu2_adpcm_block(p + off + c * inter + k * 16, blk, h[c][0], h[c][1]);
                    for (int s = 0; s < 28; ++s) {
                        const size_t o = base + (k * 28 + static_cast<size_t>(s)) * 2;
                        out_[o + c] = blk[s];
                        if (chans == 1) out_[o + 1] = blk[s];
                    }
                }
            }
        }
        if (rate == 48000) {
            if (!o48.empty()) out(o48.data(), o48.size() / 2);
        } else if (rate != 0) {  // the SPU2 plays the stream at its rate: to the host's 48 kHz (linear)
            const size_t in = pcm.size() / 2;
            std::vector<int16_t> o;
            for (;; ++out_i) {
                const double x = static_cast<double>(out_i) * rate / 48000.0;
                if (complete) {
                    if (out_i >= in * 48000 / rate) break;
                } else if (static_cast<size_t>(x) + 1 >= in) {
                    break;  // (the sample after it not decoded yet)
                }
                const size_t a = std::min(static_cast<size_t>(x), in - 1), bb = std::min(a + 1, in - 1);
                const double tt = x - static_cast<double>(a);
                for (size_t c = 0; c < 2; ++c) o.push_back(static_cast<int16_t>(pcm[a * 2 + c] + (pcm[bb * 2 + c] - pcm[a * 2 + c]) * tt));
            }
            if (!o.empty()) out(o.data(), o.size() / 2);
        }
        if (complete) phase = 2;
    }
};

#ifdef RAC1_HAVE_AVCODEC
// The IPU's colour conversion (ITU-R BT.601, studio range) of a 4:2:0 picture, alpha 0x80 (the
// IPU's RGB32 output with its alpha threshold off).
void csc(const uint8_t* y, int ys, const uint8_t* cb, int cbs, const uint8_t* cr, int crs, int w, int h, std::vector<uint8_t>& out) {
    out.resize(static_cast<size_t>(w) * h * 4);
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            const int Y = (y[j * ys + i] - 16) * 149;
            const int U = cb[(j >> 1) * cbs + (i >> 1)] - 128;
            const int V = cr[(j >> 1) * crs + (i >> 1)] - 128;
            const int r = (Y + 204 * V + 64) >> 7;
            const int g = (Y - 50 * U - 104 * V + 64) >> 7;
            const int b = (Y + 258 * U + 64) >> 7;
            uint8_t* o = &out[(static_cast<size_t>(j) * w + i) * 4];
            o[0] = static_cast<uint8_t>(std::clamp(r, 0, 255));
            o[1] = static_cast<uint8_t>(std::clamp(g, 0, 255));
            o[2] = static_cast<uint8_t>(std::clamp(b, 0, 255));
            o[3] = 0x80;
        }
    }
}
#endif

}  // namespace

void spu2_adpcm_block(const uint8_t* block, int16_t* out, int32_t& h1, int32_t& h2) {
    static const int32_t f[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};
    const int shift = block[0] & 0xf;
    const int filter = std::min(block[0] >> 4, 4);
    for (int i = 0; i < 28; ++i) {
        const int nib = (block[2 + i / 2] >> ((i & 1) * 4)) & 0xf;
        int32_t s = static_cast<int16_t>(static_cast<uint16_t>(nib << 12)) >> std::min(shift, 12);
        s += (h1 * f[filter][0] + h2 * f[filter][1] + 32) >> 6;
        s = std::clamp(s, -32768, 32767);
        h2 = h1;
        h1 = s;
        out[i] = static_cast<int16_t>(s);
    }
}

void movie_hw_enable(bool on) { g_enabled = on; }
bool movie_hw_enabled() { return g_enabled; }

bool movie_hw_available() {
    return g_enabled;
}

// The file's reading, its demultiplexing and its sound's decoding, in steps: the open runs it until the video's
// start is there (its first picture's header), a worker thread after it (load_rest). Before, the open read the whole file,
// demultiplexed it and decoded the whole sound track (0.1 .. 0.45 s on the game's thread). The pictures and samples handed
// to the game are the same: movie_hw_next_picture and movie_hw_audio_take wait for the bytes they need.
struct Load {
    uint32_t lba = 0, bytes = 0;
    int track = 0;
    std::vector<uint8_t> d;  // the file, read so far
    size_t got = 0;          // bytes read
    Demux dx;
    std::vector<std::vector<uint8_t>> tracks;
    int chosen = -1;  // the sound track decoded (the asked one when it has data, else the first that has; decided when known)
    TrackDecoder dec;
    bool ok = true;  // the disc read
    bool complete() const { return got >= bytes || !ok; }
};
Load g_load;

constexpr size_t kChunk = 256 * 1024;  // bytes read at a time

// orig: (host) the next part of the file read and demultiplexed, the sound decoded as far as it goes
void load_step(Load& L, Movie& m) {
    if (!L.complete()) {
        const size_t read_cap = (static_cast<size_t>(L.bytes) + 2047) / 2048 * 2048;
        const size_t n = std::min<size_t>(kChunk, read_cap - L.got);
        const size_t old_sz = L.d.size();
        L.d.resize(old_sz + n);
        if (!disc_read(L.lba + static_cast<uint32_t>(L.got / 2048), static_cast<uint32_t>(n / 2048), L.d.data() + old_sz)) {
            L.d.resize(old_sz);
            L.ok = false;
        } else {
            L.got += n;
            if (L.got > L.bytes) {
                const size_t excess = L.got - L.bytes;
                if (excess <= L.d.size()) L.d.resize(L.d.size() - excess);
            }
        }
    }
    const bool complete = L.complete();
    L.dx.step(L.d.data(), L.d.size(), complete,
              [&m](const uint8_t* p, size_t n) {
                  const size_t at = m.video_len.load(std::memory_order_relaxed);
                  if (at + n <= m.video.capacity()) {
                      m.video.insert(m.video.end(), p, p + n);
                      m.video_len.store(at + n, std::memory_order_release);
                  }
              },
              [&L](int t, const uint8_t* p, size_t n) {
                  if (L.tracks.size() <= static_cast<size_t>(t)) L.tracks.resize(static_cast<size_t>(t) + 1);
                  L.tracks[static_cast<size_t>(t)].insert(L.tracks[static_cast<size_t>(t)].end(), p, p + n);
              });
    if (L.dx.i > 0) {
        const size_t consumed = std::min(L.dx.i, L.d.size());
        L.d.erase(L.d.begin(), L.d.begin() + static_cast<std::ptrdiff_t>(consumed));
        L.dx.i -= consumed;
    }
    const bool all = complete && L.dx.ended;  // (the demux's end is the file's: every track is whole)
    if (L.chosen < 0) {
        if (static_cast<size_t>(L.track) < L.tracks.size() && !L.tracks[static_cast<size_t>(L.track)].empty()) L.chosen = L.track;
        else if (all)
            for (size_t k = 0; L.chosen < 0 && k < L.tracks.size(); ++k)
                if (!L.tracks[k].empty()) L.chosen = static_cast<int>(k);
    }
    if (L.chosen >= 0)
        L.dec.step(L.tracks[static_cast<size_t>(L.chosen)], all, [&m](const int16_t* pcm, size_t frames) {
            std::scoped_lock lock(g_audio_lock);
            m.audio.insert(m.audio.end(), pcm, pcm + frames * 2);
        });
    if (all) {
        const size_t num_tracks = L.tracks.size();
        L.d.clear();
        L.d.shrink_to_fit();
        L.tracks.clear();
        L.tracks.shrink_to_fit();
        m.video_done.store(true, std::memory_order_release);
        {
            std::scoped_lock lock(g_audio_lock);
            m.audio_done.store(true, std::memory_order_release);
            std::printf("[movie] sector %u read: video %zu bytes, %zu sound tracks, track %d %.1f s\n", L.lba, m.video_len.load(), num_tracks, L.chosen,
                        static_cast<double>(m.audio.size() / 2) / 48000.0);
        }
    }
    g_audio_cv.notify_all();
    {
        std::scoped_lock lock(m.lk);
    }
    m.cv.notify_all();
}

// orig: (host) the rest of the file (the worker thread)
void load_rest(Movie& m) {
#if defined(__WIIU__)
    if (OSThread* self = OSGetCurrentThread()) {
        OSSetThreadAffinity(self, OS_THREAD_ATTRIB_AFFINITY_CPU2);
        OSSetThreadPriority(self, 24);
    }
#endif
    while (!m.stop.load(std::memory_order_relaxed) && !m.video_done.load(std::memory_order_relaxed)) load_step(g_load, m);
}

bool movie_hw_open(uint32_t lba, uint32_t bytes, int track) {
    LoadProf prof_all("movie_hw_open");
    movie_hw_close();
    if (!movie_hw_available()) {
        return false;
    }
    if (bytes == 0) return false;
    if (!disc_is_open() && !disc_open()) return false;
    Movie& m = g_movie;
    Load& L = g_load;
    L = Load{};
    L.lba = lba;
    L.bytes = bytes;
    L.track = track;
    L.d.reserve(kChunk + 65536);
    m.video.clear();
    m.video.reserve(bytes);
    m.video_len.store(0);
    m.stop.store(false);
    m.video_done.store(false);
    m.audio_done.store(false);
    // the start: until the video's first bytes are there and its sequence header (12-bit width / height, frame rate code)
    // found, or the whole file read (as a demux of the whole file would see them)
    bool header = false;
    for (;;) {
        load_step(L, m);
        if (!L.ok) return false;  // (the disc read failed: the open of the whole file failed)
        const size_t vl = m.video_len.load();
        for (size_t i = 0; !header && i + 8 < vl; ++i) {
            if (m.video[i] == 0 && m.video[i + 1] == 0 && m.video[i + 2] == 1 && m.video[i + 3] == 0xb3) {
                const uint32_t v = be32(&m.video[i + 4]);
                m.width = static_cast<int>(v >> 20);
                m.height = static_cast<int>((v >> 8) & 0xfff);
                static const double rates[16] = {30, 23.976, 24, 25, 29.97, 30, 50, 59.94, 60, 30, 30, 30, 30, 30, 30, 30};
                m.fps = rates[v & 0xf];
                header = true;
            }
        }
        if (m.video_done.load()) break;
        if (vl >= 12 && header) break;
    }
    if (m.video_len.load() < 12) return false;
    m.mpeg2_dec.reset();
    m.use_builtin = true;
#ifdef RAC1_HAVE_AVCODEC
    const char* env_builtin = std::getenv("RAC1_BUILTIN_MPEG2");
    m.use_builtin = (env_builtin != nullptr && std::atoi(env_builtin) != 0);
    if (!m.use_builtin) {
        m.codec = avcodec_find_decoder(AV_CODEC_ID_MPEG2VIDEO);
        m.ctx = m.codec ? avcodec_alloc_context3(m.codec) : nullptr;
        m.parser = av_parser_init(AV_CODEC_ID_MPEG2VIDEO);
        m.pkt = av_packet_alloc();
        m.frame = av_frame_alloc();
        if (m.ctx == nullptr || m.parser == nullptr || m.pkt == nullptr || m.frame == nullptr || avcodec_open2(m.ctx, m.codec, nullptr) < 0) {
            if (m.frame != nullptr) av_frame_free(&m.frame);
            if (m.pkt != nullptr) av_packet_free(&m.pkt);
            if (m.parser != nullptr) av_parser_close(m.parser);
            if (m.ctx != nullptr) avcodec_free_context(&m.ctx);
            m.parser = nullptr;
            m.codec = nullptr;
            m.use_builtin = true;
        }
        m.flushed = false;
    }
#endif
    m.video_pos = 0;
    m.video_ended = false;
    m.open = true;
    if (!m.video_done.load()) m.loader = std::thread([&m] { load_rest(m); });
    std::printf("[movie] sector %u, %u bytes: %d x %d, %.2f fps (%s, the rest read as it plays)\n",
                lba, bytes, m.width, m.height, m.fps, m.use_builtin ? "builtin-mpeg2" : "libavcodec");
    return true;
}

bool movie_hw_is_open() { return g_movie.open; }
int movie_hw_width() { return g_movie.width; }
int movie_hw_height() { return g_movie.height; }
double movie_hw_fps() { return g_movie.fps; }
bool movie_hw_video_ended() { return !g_movie.open || g_movie.video_ended; }

bool movie_hw_next_picture(std::vector<uint8_t>& rgba, bool render_rgba) {
    Movie& m = g_movie;
    if (!m.open || m.video_ended) return false;
    if (m.use_builtin) {
        for (;;) {
            size_t vlen = m.video_len.load(std::memory_order_acquire);
            const bool vdone = m.video_done.load(std::memory_order_acquire);
            if (vdone) vlen = m.video_len.load(std::memory_order_acquire);
            int out_w = m.width, out_h = m.height;
            const int res = m.mpeg2_dec.next_picture(m.video.data(), vlen, vdone, m.video_pos, rgba, out_w, out_h, render_rgba);
            if (res == 1) {
                if (out_w > 0 && out_h > 0) {
                    m.width = out_w;
                    m.height = out_h;
                }
                return true;
            }
            if (res < 0) {
                m.video_ended = true;
                return false;
            }
            std::unique_lock lock(m.lk);
            m.cv.wait(lock, [&m, vlen] {
                return m.video_len.load(std::memory_order_acquire) >= vlen + 0x4000 ||
                       m.video_done.load(std::memory_order_acquire);
            });
        }
    }
#ifdef RAC1_HAVE_AVCODEC
    for (;;) {
        const int r = avcodec_receive_frame(m.ctx, m.frame);
        if (r == 0) {
            const AVFrame* f = m.frame;
            if (f->format != AV_PIX_FMT_YUV420P) {
                av_frame_unref(m.frame);
                continue;
            }
            m.width = f->width;
            m.height = f->height;
            if (render_rgba) csc(f->data[0], f->linesize[0], f->data[1], f->linesize[1], f->data[2], f->linesize[2], f->width, f->height, rgba);
            av_frame_unref(m.frame);
            return true;
        }
        if (r == AVERROR_EOF) {
            m.video_ended = true;
            return false;
        }
        // more input: the next 0x10000 bytes there, or all of them
        size_t vlen = m.video_len.load(std::memory_order_acquire);
        if (vlen < m.video_pos + 0x10000 && !m.video_done.load(std::memory_order_acquire)) {
            std::unique_lock lock(m.lk);
            m.cv.wait(lock, [&m] { return m.video_len.load(std::memory_order_acquire) >= m.video_pos + 0x10000 || m.video_done.load(std::memory_order_acquire); });
            vlen = m.video_len.load(std::memory_order_acquire);
        }
        const bool vdone = m.video_done.load(std::memory_order_acquire);
        if (vdone) vlen = m.video_len.load(std::memory_order_acquire);
        if (m.video_pos >= vlen) {
            if (m.flushed) {
                m.video_ended = true;
                return false;
            }
            avcodec_send_packet(m.ctx, nullptr);
            m.flushed = true;
            continue;
        }
        uint8_t* data = nullptr;
        int size = 0;
        const int used = av_parser_parse2(m.parser, m.ctx, &data, &size, m.video.data() + m.video_pos,
                                          static_cast<int>(std::min<size_t>(vlen - m.video_pos, 0x10000)), AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        if (used < 0) {
            m.video_ended = true;
            return false;
        }
        m.video_pos += static_cast<size_t>(used);
        if (vdone && m.video_pos >= vlen && size == 0) {  // the parser's last picture
            av_parser_parse2(m.parser, m.ctx, &data, &size, nullptr, 0, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        }
        if (size > 0) {
            m.pkt->data = data;
            m.pkt->size = size;
            avcodec_send_packet(m.ctx, m.pkt);
        }
    }
#else
    (void)rgba;
    m.video_ended = true;
    return false;
#endif
}

float g_movie_volume = std::getenv("RAC1_MOVIE_VOLUME") != nullptr ? static_cast<float>(std::atof(std::getenv("RAC1_MOVIE_VOLUME"))) : 1.0f;

int movie_hw_audio_take(int16_t* dst, int frames) {
    std::unique_lock lock(g_audio_lock);
    Movie& m = g_movie;
    if (!m.open) return 0;
    // the samples decoded that far, or all of them (the loader decodes as it reads)
    g_audio_cv.wait(lock, [&m, frames] { return m.audio_done.load() || m.audio.size() / 2 >= m.audio_pos + static_cast<size_t>(frames); });
    const size_t total = m.audio.size() / 2;
    int i = 0;
    for (; i < frames && m.audio_pos < total; ++i, ++m.audio_pos) {
        dst[i * 2] = m.audio[m.audio_pos * 2];
        dst[i * 2 + 1] = m.audio[m.audio_pos * 2 + 1];
    }
    return i;
}

void movie_hw_audio_add(int16_t* out, const int16_t* src, int frames, float gain_left, float gain_right) {
    for (int i = 0; i < frames; ++i) {
        for (int c = 0; c < 2; ++c) {
            const float g = (c == 0 ? gain_left : gain_right) * g_movie_volume;
            const int v = out[i * 2 + c] + static_cast<int>(std::lround(static_cast<float>(src[i * 2 + c]) * g));
            out[i * 2 + c] = static_cast<int16_t>(std::clamp(v, -32768, 32767));
        }
    }
}

void movie_hw_close() {
    Movie& m = g_movie;
    if (m.loader.joinable()) {  // (the file's loader stopped first)
        m.stop.store(true);
        m.loader.join();
    }
#ifdef RAC1_HAVE_AVCODEC
    if (m.frame != nullptr) av_frame_free(&m.frame);
    if (m.pkt != nullptr) av_packet_free(&m.pkt);
    if (m.parser != nullptr) av_parser_close(m.parser);
    if (m.ctx != nullptr) avcodec_free_context(&m.ctx);
    m.parser = nullptr;
    m.codec = nullptr;
#endif
    m.mpeg2_dec.reset();
    g_load = Load{};
    std::scoped_lock lock(g_audio_lock);
    m.open = false;
    m.video.clear();
    m.video.shrink_to_fit();
    m.video_len.store(0);
    m.audio.clear();
    m.audio.shrink_to_fit();
    m.video_pos = 0;
    m.audio_pos = 0;
    m.video_ended = false;
}

void movie_hw_show(const std::vector<uint8_t>& rgba, int w, int h) {
    g_shown = rgba;
    g_shown_w = w;
    g_shown_h = h;
    ++g_shown_serial;
}

void movie_hw_show_swap(std::vector<uint8_t>& rgba, int w, int h) {
    g_shown.swap(rgba);
    g_shown_w = w;
    g_shown_h = h;
    ++g_shown_serial;
}

const std::vector<uint8_t>* movie_hw_shown(int& w, int& h) {
    if (g_shown.empty()) return nullptr;
    w = g_shown_w;
    h = g_shown_h;
    return &g_shown;
}

uint64_t movie_hw_shown_serial() { return g_shown_serial; }

void movie_hw_show_clear() {
    g_shown.clear();
    g_shown.shrink_to_fit();
    g_shown_w = g_shown_h = 0;
    g_shown_serial = 0;
}

}  // namespace rac1
