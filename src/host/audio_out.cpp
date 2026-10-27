// The host's audio device (the SPU2's output): 48 kHz stereo, or (Options > Sound: 5.1 / 7.1, a host setting) 6 / 8
// channels: the IOP's mirror renders its voices on the speakers too (Synth::Tick), the queue carries that many channels.
//
// The sound is made on the game's time, not the device's. The IOP's 989snd advances
// 1/60 s (800 sample frames) at each vsync the game waits for (orig_main.cpp's sound_vsync, on the
// game's thread), as on a PS2 that never misses a vsync; what it reports back to the game (a sound
// still playing, a stream's time left) then depends on the game's frames only, never on how fast
// the host runs them. The samples made go into a queue the device plays from. Had the
// device's thread run 989snd itself, pulled at the wall clock's pace, the game would see its sounds end
// sooner when the host's frames were slow, later when it ran several game frames in one host frame.
//
// The queue made to hold under a loaded CPU. The game's thread pushes 800 frames per vsync in bursts (a host
// frame late, then two game frames in the next one); the device pulls at its own clock. Nothing of this reaches the
// game: only how the samples already made are played.
// - A lock-free ring (one producer: the game's thread; one consumer: the device's thread). The device's callback takes
//   no lock, so a game frame holding the IOP's locks (a stream's disc read) can never make it late.
// - A target fill (the latency): starts at 3 game frames (50 ms), raised by one game frame (up to 8) when the queue ran
//   dry while the game was running (the host's jitter was larger than the queue), lowered back by half a frame after
//   20 s without running dry (never under 2 frames, the earlier policy's start).
// - Rate control: the device reads the queue at 1 +- 0.5 % of its rate (linear interpolation) to keep the fill at the
//   target. This absorbs the drift between the host's 60 Hz and the device's 48 kHz clocks and the host's catch-up
//   bursts without dropping samples (0.5 % is 9 cents of pitch at most, inaudible; the steady state is ~1.0000).
// - Running dry (an underrun: the host stalled longer than the queue): what is left is played, then the last sample
//   ramped to 0 over 2.5 ms (no click, no stale samples), then silence until the queue is back at the target; the sound
//   resumes with a 2.5 ms fade in. Counted.
// - Too much queued (a fast screen fade's n + 2 vsyncs in one host frame: over 12 game frames past the target at once, or 4
//   frames past it on average for ~150 ms): the excess over the target skipped with a 2.5 ms crossfade. Counted.
// - The device's buffer: 10 ms periods x 3 (RAC1_AUDIO_PERIOD_MS / RAC1_AUDIO_PERIODS; PulseAudio / PipeWire take it as one
//   30 ms buffer). Its thread made real-time through rtkit (host_priority.h; RAC1_PRIORITY=0: not): under a loaded CPU a
//   normal thread was measured waking up to 48 ms late, past the device's buffer (PipeWire counted the stream's xruns).
// - `[audio]` lines: every underrun (the push gap that caused it), and a summary every 10 s when something happened
//   and at exit. RAC1_AUDIO_LOG=1: the summary every 10 s always (the fill's min / mean, the rate, the callbacks' gaps).
//
// (ADR 0013) The queue's producer is the IOP's mirror on a thread of its own (the "IOP thread"), not the game's
// thread. The game's IOP (runtime/iop/snd989_iop.h) records what it is sent and each vsync; the IOP thread replays them on
// the mirror, a second 989snd, and queues its output: sample for sample the game's IOP's while the game keeps up. When the
// game's thread stalls (a load, a long frame: inside a game step 20 ms after its last vsync, or no vsync for longer than the
// host's usual gaps) and the queue runs low, the IOP thread
// renders the mirror alone, a game frame's worth at a time, until the game's vsyncs come back: the music and the sounds
// already playing go on, as on the PS2, whose IOP is a processor of its own. The game's IOP (what the game reads back) does
// not move: only the mirror's time runs ahead. Not while the host holds the game (the debug menu's pause or its slow
// speeds: audio_out_set_alone). RAC1_IOP_MIRROR=0: the game's thread pushes the game's IOP's output as before.
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#include <miniaudio.h>

#include "host_priority.h"

#include <runtime/iop/snd989_iop.h>

#include <platform/platform.h>
#include <profiler/host_profiler.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace {
ma_context g_context;
ma_device g_device;
bool g_started = false;

using Clock = std::chrono::steady_clock;
double now_ms() { return std::chrono::duration<double, std::milli>(Clock::now().time_since_epoch()).count(); }

constexpr int kFrame = 800;                       // a game frame's samples (1/60 s at 48 kHz)
constexpr uint64_t kCap = uint64_t{1} << 16;      // the ring: 65536 sample frames (1.37 s)
constexpr int kRamp = 120;                        // 2.5 ms: the fades at an underrun / resume / skip
constexpr double kMaxSkew = 0.005;                // the rate control's limit (+- 0.5 %)
constexpr int kMinTarget = 2 * kFrame, kMaxTarget = 8 * kFrame, kStartTarget = 3 * kFrame;
// RAC1_AUDIO_QUEUE=0354 (testing; the value is just a key): the earlier, simple policy, for comparison: a fixed 2-frame start, the excess past 6 frames
// dropped back to 2 without a crossfade, no rate control, no fades at running dry (counted the same way).
const bool g_simple = std::getenv("RAC1_AUDIO_QUEUE") != nullptr && std::strcmp(std::getenv("RAC1_AUDIO_QUEUE"), "0354") == 0;
const int kRampUsed = g_simple ? 0 : kRamp;

// The ring: written by the game's thread only (w), read by the device's thread only (r). g_ch samples a frame (the
// device's channels: 2, 6 or 8; changed only with the device and the IOP thread stopped, audio_out_set_channels).
constexpr int kMaxCh = 8;
std::vector<int16_t> g_ring(kCap * kMaxCh);
int g_ch = 2;
std::atomic<uint64_t> g_w{0}, g_r{0};
std::atomic<double> g_last_push_ms{0.0};
std::atomic<bool> g_producing{false};  // (pushes seen recently: an underrun while the game was running, not a pause)

// The device side's state (its thread only).
struct Reader {
    bool primed = false;
    double frac = 0.0;      // the read position's fraction past g_r
    double fill_avg = -1;   // the fill seen by the callbacks, smoothed
    int target = g_simple ? kMinTarget : kStartTarget;
    int ramp_in = 0;        // frames of the fade in left
    int16_t last[kMaxCh] = {};
    double last_dry_ms = 0;  // the last underrun (for the target's lowering)
    double last_cb_ms = 0;
    // the game's production measured over ~1 s windows: below 95 % of the device's rate the game runs slower than real time
    // (a host that cannot keep 60 game frames a second even with its catch-up; the debug menu's slow speeds; a headless run
    // paced at 60 that cannot keep up): then running dry is not the host's jitter, the target is not raised, and the queue
    // restarts at its minimum (short gaps rather than long silences waiting for a large queue that never fills)
    double win_t0 = 0;
    uint64_t win_w0 = 0;
    bool deficit = false;
} g_rd;

// Counters (written by the device's thread, read at the summaries).
std::atomic<uint64_t> g_deficit_underruns{0};
std::atomic<uint64_t> g_underruns{0}, g_dry_frames{0}, g_skips{0}, g_skipped_frames{0}, g_overflow_frames{0}, g_callbacks{0};
std::atomic<double> g_max_cb_gap_ms{0}, g_min_fill_ms{1e9}, g_fill_sum_ms{0}, g_rate_sum{0};
std::atomic<int> g_target_now{kStartTarget};
std::atomic<uint64_t> g_rate_n{0};
std::atomic<uint64_t> g_device_tid{0};
std::atomic<uint64_t> g_alone_frames{0}, g_alone_runs{0};  // (the IOP's mirror played alone; since the last summary)
// the longest recent gap between the game's bursts of vsyncs (the IOP thread's measure, see iop_loop): the queue's target is not
// lowered under it (with a quarter more) plus the device's period (at 30 frames a second, or VSync at 75 Hz, a 33 ms target ran dry at each gap)
std::atomic<double> g_burst_gap_ms{0.0};
std::atomic<uint64_t> g_alone_total{0};                    // (the whole run's)  // (the device's thread, seen by its first callback)

inline int16_t at(uint64_t i, int c) { return g_ring[(i & (kCap - 1)) * static_cast<uint64_t>(g_ch) + static_cast<uint64_t>(c)]; }

template <class T>
void atomic_max(std::atomic<T>& a, T v) {
    T o = a.load(std::memory_order_relaxed);
    while (v > o && !a.compare_exchange_weak(o, v, std::memory_order_relaxed)) {
    }
}
template <class T>
void atomic_min(std::atomic<T>& a, T v) {
    T o = a.load(std::memory_order_relaxed);
    while (v < o && !a.compare_exchange_weak(o, v, std::memory_order_relaxed)) {
    }
}
void atomic_add(std::atomic<double>& a, double v) {
    double o = a.load(std::memory_order_relaxed);
    while (!a.compare_exchange_weak(o, o + v, std::memory_order_relaxed)) {
    }
}

// (testing) RAC1_DEVICE_DUMP=file: what the device is given, as played (48 kHz stereo s16): the queue's skips, underruns and
// the IOP's frames alone in it, to look for clicks
FILE* g_device_dump = std::getenv("RAC1_DEVICE_DUMP") != nullptr ? std::fopen(std::getenv("RAC1_DEVICE_DUMP"), "wb") : nullptr;
void pull_queue(ma_device* d, void* out, const void* in, ma_uint32 frames_u);
void pull(ma_device* d, void* out, const void* in, ma_uint32 frames_u) {
    pull_queue(d, out, in, frames_u);
    if (g_device_dump != nullptr) std::fwrite(out, static_cast<size_t>(2 * g_ch), frames_u, g_device_dump);
}
void pull_queue(ma_device*, void* out, const void*, ma_uint32 frames_u) {
    int16_t* o = static_cast<int16_t*>(out);
    const int frames = static_cast<int>(frames_u);
    Reader& rd = g_rd;
    if (g_device_tid.load(std::memory_order_relaxed) == 0) {
        g_device_tid.store(rac1::platform::thread_id(), std::memory_order_relaxed);
        rac1::prof::thread_name("audio device");  // (the debug menu's profiler: the Threads list)
    }
    const double t = now_ms();
    if (rd.last_cb_ms != 0) atomic_max(g_max_cb_gap_ms, t - rd.last_cb_ms);
    rd.last_cb_ms = t;
    g_callbacks.fetch_add(1, std::memory_order_relaxed);

    const uint64_t w = g_w.load(std::memory_order_acquire);
    uint64_t r = g_r.load(std::memory_order_relaxed);
    int64_t fill = static_cast<int64_t>(w - r);
    if (rd.win_t0 == 0) {
        rd.win_t0 = t;
        rd.win_w0 = w;
    } else if (t - rd.win_t0 >= 1000.0) {
        const double made = static_cast<double>(w - rd.win_w0) / ((t - rd.win_t0) * 48.0);  // (1: the device's rate)
        rd.deficit = g_producing.load(std::memory_order_relaxed) && made < 0.95;
        rd.win_t0 = t;
        rd.win_w0 = w;
    }

    // the target lowered after 20 s without running dry
    const int floor_target = std::max(kMinTarget, static_cast<int>(g_burst_gap_ms.load(std::memory_order_relaxed) * 1.25 * 48.0) +
                                                      static_cast<int>(g_device.playback.internalPeriodSizeInFrames));
    if (!g_simple && rd.target - kFrame / 2 >= floor_target && t - rd.last_dry_ms > 20000.0) {
        rd.target = rd.target - kFrame / 2;
        rd.last_dry_ms = t;
        g_target_now.store(rd.target, std::memory_order_relaxed);
    }
    if (!rd.primed) {
        if (fill >= (rd.deficit ? kMinTarget : rd.target)) {
            rd.primed = true;
            rd.frac = 0.0;
            rd.fill_avg = static_cast<double>(fill);
            rd.ramp_in = kRampUsed;
            if (!g_simple && fill > rd.target && !rd.deficit) {  // (only the target's worth kept: the latency stays the target's)
                g_skipped_frames.fetch_add(static_cast<uint64_t>(fill - rd.target), std::memory_order_relaxed);
                r = w - static_cast<uint64_t>(rd.target);
                fill = rd.target;
            }
        } else {
            std::memset(o, 0, static_cast<size_t>(frames * g_ch) * 2);
            return;
        }
    }
    // too much queued: the excess skipped, crossfaded. Either a burst far over the target (a fast screen fade's n + 2 vsyncs in one
    // host frame: 8 or 14 game frames), or an excess that lasts (the smoothed fill: a device's thread late for a few callbacks
    // fills the queue for a moment, then takes it back - skipping then would only run it dry after)
    rd.fill_avg = rd.fill_avg < 0 ? static_cast<double>(fill) : rd.fill_avg * 0.95 + static_cast<double>(fill) * 0.05;
    int xfade = 0;
    uint64_t r_old = r;
    const bool burst = fill > rd.target + 6 * kFrame, lasting = rd.fill_avg > rd.target + 4 * kFrame;
    if (g_simple ? fill > rd.target + 4 * kFrame : (burst || lasting) && fill > rd.target) {
        const uint64_t skip = static_cast<uint64_t>(fill - rd.target);
        g_skips.fetch_add(1, std::memory_order_relaxed);
        g_skipped_frames.fetch_add(skip, std::memory_order_relaxed);
        static int logged = 0;  // (each one while a few, as the underruns)
        if (logged++ < 20)
            std::fprintf(stderr, "[audio] skip: %.1f ms queued (%s), the target %.1f ms: %.1f ms skipped\n", fill / 48.0, burst ? "a burst" : "lasting",
                         rd.target / 48.0, static_cast<double>(skip) / 48.0);
        r += skip;
        fill = rd.target;
        rd.fill_avg = static_cast<double>(fill);
        xfade = kRampUsed;
    }
    atomic_min(g_min_fill_ms, static_cast<double>(fill) / 48.0);
    atomic_add(g_fill_sum_ms, static_cast<double>(fill) / 48.0);
    // the rate: above the target read faster, below slower
    const double skew = std::clamp((rd.fill_avg - rd.target) / static_cast<double>(rd.target) * 0.01, -kMaxSkew, kMaxSkew);
    const double step = g_simple ? 1.0 : 1.0 + skew;
    atomic_add(g_rate_sum, step);
    g_rate_n.fetch_add(1, std::memory_order_relaxed);

    double pos = rd.frac;  // relative to r
    int i = 0;
    for (; i < frames; ++i) {
        const uint64_t k = static_cast<uint64_t>(pos);
        if (static_cast<int64_t>(k + 1) >= fill) break;  // (the next two samples needed)
        const double f = pos - static_cast<double>(k);
        for (int c = 0; c < g_ch; ++c) {
            double v = at(r + k, c) * (1.0 - f) + at(r + k + 1, c) * f;
            if (xfade > 0) {  // (the skipped position faded out, the new one in)
                const double a = static_cast<double>(kRamp - xfade) / kRamp;
                const double vo = at(r_old + k, c) * (1.0 - f) + at(r_old + k + 1, c) * f;
                v = vo * (1.0 - a) + v * a;
            }
            if (rd.ramp_in > 0) v *= static_cast<double>(kRamp - rd.ramp_in) / kRamp;
            const int16_t s = static_cast<int16_t>(std::clamp(std::lround(v), -32768L, 32767L));
            o[i * g_ch + c] = s;
            rd.last[c] = s;
        }
        if (xfade > 0) --xfade;
        if (rd.ramp_in > 0) --rd.ramp_in;
        pos += step;
    }
    const uint64_t used = static_cast<uint64_t>(pos);
    rd.frac = pos - static_cast<double>(used);
    g_r.store(r + std::min<uint64_t>(used, static_cast<uint64_t>(std::max<int64_t>(fill, 0))), std::memory_order_release);
    if (i < frames) {  // ran dry: the last sample ramped to 0, then silence
        const int n = frames - i;
        for (int j = 0; j < n; ++j) {
            const double a = j < kRampUsed ? 1.0 - static_cast<double>(j + 1) / kRampUsed : 0.0;
            for (int c = 0; c < g_ch; ++c) o[(i + j) * g_ch + c] = static_cast<int16_t>(rd.last[c] * a);
        }
        for (int16_t& l : rd.last) l = 0;
        rd.primed = false;
        rd.frac = 0.0;
        // a pause / a long load (no push for 250 ms) is not the host's jitter: counted apart, the target kept
        const double gap = t - g_last_push_ms.load(std::memory_order_relaxed);
        if (g_producing.load(std::memory_order_relaxed) && gap < 250.0) {
            g_underruns.fetch_add(1, std::memory_order_relaxed);
            g_dry_frames.fetch_add(static_cast<uint64_t>(n), std::memory_order_relaxed);
            rd.last_dry_ms = t;
            if (!g_simple && !rd.deficit) rd.target = std::min(kMaxTarget, rd.target + kFrame);
            g_target_now.store(rd.target, std::memory_order_relaxed);
            if (rd.deficit) g_deficit_underruns.fetch_add(1, std::memory_order_relaxed);
            static int logged = 0;  // (each one while a few; then the summaries)
            if (logged++ < 20)
                std::fprintf(stderr, "[audio] underrun: %d frames short (%.1f ms since the game's last push)%s, the queue's target now %.1f ms\n", n, gap,
                             rd.deficit ? " - the game slower than real time" : "", rd.target / 48.0);
        }
        g_producing.store(false, std::memory_order_relaxed);
    }
}

// The summary of the counters since the last one (every 10 s from the producer's side, and at exit).
struct Summary {
    double last_ms = 0;
    uint64_t pushes = 0, pushed = 0;
    double max_push_gap = 0;
} g_sum;

void summary(bool force) {
    static const bool always = std::getenv("RAC1_AUDIO_LOG") != nullptr;
    const uint64_t def = g_deficit_underruns.exchange(0);
    const uint64_t un = g_underruns.exchange(0), dry = g_dry_frames.exchange(0), sk = g_skips.exchange(0), skf = g_skipped_frames.exchange(0),
                   ov = g_overflow_frames.exchange(0), cb = g_callbacks.exchange(0), rn = g_rate_n.exchange(0);
    const uint64_t alone = g_alone_frames.exchange(0), runs = g_alone_runs.exchange(0);
    const double gap = g_max_cb_gap_ms.exchange(0), minf = g_min_fill_ms.exchange(1e9), fsum = g_fill_sum_ms.exchange(0), rsum = g_rate_sum.exchange(0);
    if (!(always || force || un != 0 || sk != 0 || ov != 0 || alone != 0)) {
        g_sum.pushes = g_sum.pushed = 0;
        g_sum.max_push_gap = 0;
        return;
    }
    std::fprintf(stderr,
                 "[audio] %s: %llu underruns (%llu of them with the game slower than real time; %llu frames of silence), %llu skips (%llu frames skipped), %llu frames lost to a full queue; "
                 "pushes %llu (%llu frames, longest gap %.1f ms); device callbacks %llu (longest gap %.1f ms); fill min %.1f ms mean %.1f ms, "
                 "target %.1f ms, rate %.5f; %llu frames played by the IOP alone in %llu stalls of the game\n",
                 force ? "at exit" : "last 10 s", static_cast<unsigned long long>(un), static_cast<unsigned long long>(def), static_cast<unsigned long long>(dry), static_cast<unsigned long long>(sk),
                 static_cast<unsigned long long>(skf), static_cast<unsigned long long>(ov), static_cast<unsigned long long>(g_sum.pushes),
                 static_cast<unsigned long long>(g_sum.pushed), g_sum.max_push_gap, static_cast<unsigned long long>(cb), gap, minf > 1e8 ? 0.0 : minf,
                 cb != 0 ? fsum / static_cast<double>(cb) : 0.0, g_target_now.load() / 48.0, rn != 0 ? rsum / static_cast<double>(rn) : 1.0,
                 static_cast<unsigned long long>(alone), static_cast<unsigned long long>(runs));
    g_sum.pushes = g_sum.pushed = 0;
    g_sum.max_push_gap = 0;
}
}  // namespace

namespace {
// The IOP thread: the mirror's output queued; the mirror alone while the game's thread stalls.
std::thread g_iop;
std::atomic<bool> g_iop_run{false};
std::atomic<bool> g_alone_ok{true};
// (testing) RAC1_IOP_VERIFY: the mirror never alone, so that each of its vsyncs can be compared with the game's IOP's mix
const bool g_iop_verify = std::getenv("RAC1_IOP_VERIFY") != nullptr;
std::atomic<double> g_last_vsync_ms{0.0};  // (the game's last vsync recorded, as the IOP thread saw it)
// The game's thread stalled: inside one game step (a game frame, or a blocking loop's frame) for more than kAloneGapMs since
// its last vsync (a load, a long frame of the game's code), or no vsync for longer than the host's usual gap between its
// bursts of vsyncs allows (a long draw or swap: a shader compiled, a texture upload). Not the gaps of a host that runs below
// 60 frames a second at its pace (the 30 FPS cap: two game frames every 33 ms; VSync at 75 Hz: a host frame of 27 ms every
// fifth; a heavy scene): those vsyncs come, the game is not late, and the queue absorbs them. Played alone then (on any gap
// over 20 ms), the mirror's frames were taken off the game's owed frames (orig_main) and the game ran slower than real time
// (issues #4, #8, #22).
constexpr double kAloneGapMs = 20.0, kAloneMaxGapMs = 100.0;
std::atomic<double> g_step_t0_ms{0.0};  // (the game step the game's thread is in: its start, 0 between steps)
double g_gap_max_ms = 0.0;  // (the IOP thread's: the longest recent gap between the game's vsyncs, decaying)
int g_alone_fill = 2 * kFrame;              // (the fill under which a stalled game's gap is filled: the device's period + half a frame)
void ring_push(const int16_t* pcm, int frames);

// orig: (host) the IOP's own processor (ADR 0013): the mirror's frames queued as the game's vsyncs come, alone when they do not
void iop_loop() {
    int16_t buf[kFrame * 2];
    int16_t multi[kFrame * kMaxCh];  // (5.1 / 7.1: the speakers' output, what is queued)
    const int ch = g_ch;
    int16_t* const mp = ch > 2 ? multi : nullptr;
    int16_t* const queued = ch > 2 ? multi : buf;
    int run = 0;          // the frames of the current run alone
    double run_t0 = 0.0;  // the game's last vsync before it
    while (g_iop_run.load(std::memory_order_relaxed)) {
        bool did = false;
        const double last_before = g_last_vsync_ms.load(std::memory_order_relaxed);
        while (rac1::snd989_mirror_next(buf, mp, ch)) {
            ring_push(queued, kFrame);
            g_last_vsync_ms.store(now_ms(), std::memory_order_relaxed);
            did = true;
        }
        if (did) {
            if (run == 0 && last_before != 0.0)  // (the host's own rhythm, the stalls left out; ~2.3 s half-life at 60 bursts a second)
                g_gap_max_ms = std::max(std::min(now_ms() - last_before, kAloneMaxGapMs), g_gap_max_ms * 0.995);
            g_burst_gap_ms.store(g_gap_max_ms, std::memory_order_relaxed);
            if (run != 0) {
                g_alone_runs.fetch_add(1, std::memory_order_relaxed);
                static int logged = 0;
                if (logged++ < 20)
                    std::fprintf(stderr, "[audio] the game's thread stalled %.0f ms: %d frames (%.0f ms) played by the IOP alone\n", now_ms() - run_t0, run,
                                 run * 1000.0 / 60.0);
                run = 0;
            }
            continue;
        }
        const double t = now_ms(), last = g_last_vsync_ms.load(std::memory_order_relaxed);
        const int64_t fill = static_cast<int64_t>(g_w.load(std::memory_order_relaxed) - g_r.load(std::memory_order_acquire));
        const double step_t0 = g_step_t0_ms.load(std::memory_order_relaxed);
        const double gap_limit = std::clamp(g_gap_max_ms * 1.25 + 4.0, kAloneGapMs, kAloneMaxGapMs);
        const bool stalled = (step_t0 != 0.0 && t - step_t0 > kAloneGapMs && t - last > kAloneGapMs) || t - last > gap_limit;
        if (g_alone_ok.load(std::memory_order_relaxed) && !g_iop_verify && last != 0.0 && stalled && fill < g_alone_fill) {
            if (run++ == 0) run_t0 = last;
            rac1::snd989_mirror_alone(buf, mp, ch);
            ring_push(queued, kFrame);
            g_alone_frames.fetch_add(1, std::memory_order_relaxed);
            g_alone_total.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        rac1::snd989_mirror_wait(2.0);
    }
}

// orig: (host) the device opened with `channels` (2, 6: 5.1, 8: 7.1), its channel map the WAVE order; a device that
// does not take them (fewer channels of its own) opened in stereo, said once
bool device_open(int channels) {
    ma_device_config c = ma_device_config_init(ma_device_type_playback);
    c.playback.format = ma_format_s16;
    c.playback.channels = static_cast<ma_uint32>(channels);
    ma_channel map[kMaxCh] = {MA_CHANNEL_FRONT_LEFT, MA_CHANNEL_FRONT_RIGHT, MA_CHANNEL_FRONT_CENTER, MA_CHANNEL_LFE,
                              MA_CHANNEL_SIDE_LEFT,  MA_CHANNEL_SIDE_RIGHT,  MA_CHANNEL_NONE,         MA_CHANNEL_NONE};
    if (channels == 8) {
        map[4] = MA_CHANNEL_BACK_LEFT;
        map[5] = MA_CHANNEL_BACK_RIGHT;
        map[6] = MA_CHANNEL_SIDE_LEFT;
        map[7] = MA_CHANNEL_SIDE_RIGHT;
    }
    if (channels > 2) c.playback.pChannelMap = map;
    c.sampleRate = 48000;
    c.dataCallback = pull;
    c.periodSizeInMilliseconds = std::getenv("RAC1_AUDIO_PERIOD_MS") != nullptr ? static_cast<ma_uint32>(std::atoi(std::getenv("RAC1_AUDIO_PERIOD_MS"))) : 10;
    c.periods = std::getenv("RAC1_AUDIO_PERIODS") != nullptr ? static_cast<ma_uint32>(std::atoi(std::getenv("RAC1_AUDIO_PERIODS"))) : 3;
    g_ch = channels;
    if (ma_device_init(&g_context, &c, &g_device) != MA_SUCCESS) {
        if (channels == 2) return false;
        std::fprintf(stderr, "[audio] the device cannot be opened with %d channels: stereo\n", channels);
        return device_open(2);
    }
    if (channels > 2 && static_cast<int>(g_device.playback.internalChannels) < channels) {
        std::fprintf(stderr, "[audio] the device has %u channels, not %d: stereo\n", g_device.playback.internalChannels, channels);
        ma_device_uninit(&g_device);
        return device_open(2);
    }
    if (ma_device_start(&g_device) != MA_SUCCESS) {
        ma_device_uninit(&g_device);
        return false;
    }
    std::fprintf(stderr, "[audio] device: %s, %u Hz, %d channels%s, period %u frames x %u\n", ma_get_backend_name(g_context.backend), g_device.sampleRate, g_ch,
                 g_ch == 8 ? " (7.1)" : g_ch == 6 ? " (5.1)" : "", g_device.playback.internalPeriodSizeInFrames, g_device.playback.internalPeriods);
    return true;
}

// orig: (host) the device's thread made real-time once its first callback named it (the D-Bus call off the game's thread);
// again for the new thread when the device is opened again (Options > Sound's 5.1 / 7.1)
void device_thread_realtime() {
    if (!rac1::host_priority_enabled()) return;
    std::thread([] {
        for (int i = 0; i < 200 && g_device_tid.load() == 0; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        const uint64_t tid = g_device_tid.load();
        if (tid != 0)
            std::fprintf(stderr, "[audio] the device's thread %llu real-time: %s\n", static_cast<unsigned long long>(tid), rac1::host_priority_realtime(tid, 10) ? "yes" : "no (rtkit refused or absent)");
    }).detach();
}

// orig: (host) the IOP thread started (the mirror's frames queued)
void iop_thread_start() {
    if (!rac1::snd989_mirror_enabled()) return;
    g_iop_run.store(true);
    g_iop = std::thread([] {
        rac1::prof::thread_name("IOP mirror (audio)");
        if (rac1::host_priority_enabled()) {  // (the queue's producer now: real-time as the device's thread, one step under it)
            const uint64_t tid = rac1::platform::thread_id();
            std::fprintf(stderr, "[audio] the IOP thread %llu real-time: %s\n", static_cast<unsigned long long>(tid), rac1::host_priority_realtime(tid, 9) ? "yes" : "no (rtkit refused or absent)");
        }
        iop_loop();
    });
}
}  // namespace

// orig: (host) the SPU2's output: the device opened (`channels`: 2, or 6 / 8 for 5.1 / 7.1)
bool audio_out_start(int channels) {
    if (channels != 6 && channels != 8) channels = 2;
    ma_context_config cc = ma_context_config_init();
    cc.threadPriority = ma_thread_priority_realtime;  // (granted where the system allows it, else the default)
    // (testing) RAC1_AUDIO_DEVICE=null: miniaudio's null backend (a timer pulls the queue at 48 kHz, nothing is heard): a
    // headless run measures the queue, the IOP thread and the game's pacing as a player's would, without a sound device
    const char* dev = std::getenv("RAC1_AUDIO_DEVICE");
    const ma_backend null_backend[] = {ma_backend_null};
    const bool null_dev = dev != nullptr && std::strcmp(dev, "null") == 0;
    if (ma_context_init(null_dev ? null_backend : nullptr, null_dev ? 1 : 0, &cc, &g_context) != MA_SUCCESS) {
        std::fprintf(stderr, "rac1-native: no audio context\n");
        return false;
    }
    if (!device_open(channels)) {
        std::fprintf(stderr, "rac1-native: no audio device\n");
        ma_context_uninit(&g_context);
        return false;
    }
    g_sum.last_ms = now_ms();
    g_rd.last_dry_ms = g_sum.last_ms;
    g_target_now.store(g_rd.target);
    g_started = true;
    g_alone_fill = std::max<int>(2 * kFrame, static_cast<int>(g_device.playback.internalPeriodSizeInFrames) + kFrame / 2);
    iop_thread_start();
    device_thread_realtime();
    return true;
}

// orig: (host) the SPU2's output: `frames` stereo sample frames made on the game's time (`multi`: the same on the
// device's 5.1 / 7.1 speakers, audio_out_channels), queued for the device (the game's thread, without the IOP's mirror)
void audio_out_push(const int16_t* pcm, const int16_t* multi, int frames) {
    if (!g_started || g_iop_run.load(std::memory_order_relaxed)) return;
    if (g_ch > 2 && multi == nullptr) return;  // (the device just reopened with another layout: the next vsync's)
    ring_push(g_ch > 2 ? multi : pcm, frames);
}

// orig: (host) the device's channels (2, 6, 8), 2 without a device
int audio_out_channels() { return g_started ? g_ch : 2; }

// orig: (host) Options > Sound's 5.1 / 7.1 / stereo (the host's choice): the device opened again with that many
// channels (the IOP thread stopped meanwhile, the queue emptied: a moment of silence); the game's thread
void audio_out_set_channels(int channels) {
    if (channels != 6 && channels != 8) channels = 2;
    if (!g_started || channels == g_ch) return;
    const bool iop = g_iop_run.exchange(false);
    if (iop) g_iop.join();
    ma_device_uninit(&g_device);
    g_w.store(0);
    g_r.store(0);
    const int target = g_rd.target;
    g_rd = Reader{};
    g_rd.target = target;
    g_device_tid.store(0);  // (the new device's thread: named and made real-time again by its first callback)
    if (!device_open(channels)) {
        std::fprintf(stderr, "rac1-native: no audio device\n");
        ma_context_uninit(&g_context);
        g_started = false;
        if (iop) rac1::snd989_mirror_disable();
        return;
    }
    g_alone_fill = std::max<int>(2 * kFrame, static_cast<int>(g_device.playback.internalPeriodSizeInFrames) + kFrame / 2);
    if (iop) iop_thread_start();
    device_thread_realtime();
}

// orig: (host) the host lets the IOP run alone while the game's thread stalls (not while it holds the game: a pause, a slow speed)
void audio_out_set_alone(bool ok) { g_alone_ok.store(ok, std::memory_order_relaxed); }

// orig: (host) the game's thread enters / leaves a game step (a game frame, or a blocking loop's frame up to its vsync)
void audio_out_step(bool in_step) { g_step_t0_ms.store(in_step ? now_ms() : 0.0, std::memory_order_relaxed); }

// orig: (host) the game frames the IOP's mirror played alone so far (the host does not run them again as owed game frames)
uint64_t audio_out_alone_total() { return g_alone_total.load(std::memory_order_relaxed); }

namespace {
// orig: (host) the queue's producer: the IOP thread (the mirror), or the game's thread
void ring_push(const int16_t* pcm, int frames) {
    const double t = now_ms();
    const double prev = g_last_push_ms.exchange(t, std::memory_order_relaxed);
    if (prev != 0) g_sum.max_push_gap = std::max(g_sum.max_push_gap, t - prev);
    g_producing.store(true, std::memory_order_relaxed);
    ++g_sum.pushes;
    g_sum.pushed += static_cast<uint64_t>(frames);
    const uint64_t w = g_w.load(std::memory_order_relaxed), r = g_r.load(std::memory_order_acquire);
    const uint64_t room = kCap - (w - r);
    const uint64_t n = std::min<uint64_t>(static_cast<uint64_t>(frames), room);
    if (n < static_cast<uint64_t>(frames)) g_overflow_frames.fetch_add(static_cast<uint64_t>(frames) - n, std::memory_order_relaxed);
    const uint64_t ch = static_cast<uint64_t>(g_ch);
    for (uint64_t i = 0; i < n; ++i) {
        const uint64_t k = ((w + i) & (kCap - 1)) * ch;
        for (uint64_t c = 0; c < ch; ++c) g_ring[k + c] = pcm[i * ch + c];
    }
    g_w.store(w + n, std::memory_order_release);
    if (t - g_sum.last_ms >= 10000.0) {
        g_sum.last_ms = t;
        summary(false);
    }
}
}  // namespace

// orig: (host) the SPU2's output: the device closed
void audio_out_stop() {
    if (g_iop_run.exchange(false)) {
        g_iop.join();
        rac1::snd989_mirror_report();
    }
    if (g_started) {
        ma_device_uninit(&g_device);
        ma_context_uninit(&g_context);
        summary(true);
    }
    g_started = false;
}
