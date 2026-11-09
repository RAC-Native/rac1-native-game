// The game's sound on the Wii U (src/host/orig_main.cpp's audio_out_*, as audio_out.cpp's on the PC): two AX voices,
// the left and the right channel on the TV and the GamePad, play two looping LPCM16 buffers. What fills them, as on the
// PC (ADR 0013): the IOP's mirror on a thread of its own on core 2, which replays the game's recorded IOP vsync by vsync
// and, while the game's thread is late (a load, a long frame), plays on alone a game frame at a time (those frames are
// then not owed by the game: audio_out_alone_total). With RAC1_IOP_MIRROR=0 the game's thread pushes its IOP's output.
// The writer stays ~3 game frames ahead of the voices' play position and starts again from there when it falls behind
// or runs too far ahead. Host code.
#include <runtime/iop/snd989_iop.h>
#include <runtime/ipu/movie_hw.h>

#include <coreinit/cache.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <sndcore2/core.h>
#include <sndcore2/device.h>
#include <sndcore2/voice.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
constexpr uint32_t kRing = 24000;    // each channel's buffer: 0.5 s at 48 kHz (30 game frames; the lead kept at kAhead: a backlog fits)
constexpr uint32_t kAhead = 2400;    // the writer's lead: 3 game frames (50 ms)
constexpr int kFrame = 800;          // a game frame's samples
int16_t* g_buf[2] = {};              // the left and right channels' buffers (the DSP reads them: flushed after each write)
AXVoice* g_voice[2] = {};
uint32_t g_w = 0;                    // where the next frame is written (the writer's: the mirror's thread, or the game's)
bool g_on = false;
uint64_t g_resyncs = 0;

// the mirror's thread
OSThread g_iop;
int g_iop_core = 0;
alignas(16) unsigned char g_iop_stack[256 * 1024];
std::atomic<bool> g_iop_run{false};
std::atomic<bool> g_alone_ok{true};
std::atomic<uint64_t> g_alone_total{0};
std::atomic<double> g_step_t0_ms{0.0};  // (the game step the game's thread is in: its start, 0 between steps)
uint64_t g_alone_runs = 0;

double now_ms() { return static_cast<double>(OSTicksToMicroseconds(OSGetSystemTime())) / 1000.0; }

uint32_t play_pos() {
    AXVoiceOffsets o{};
    AXGetVoiceOffsets(g_voice[0], &o);
    return o.currentOffset % kRing;
}
// how far the writer is ahead of the voices (in samples)
uint32_t lead() { return (g_w + kRing - play_pos()) % kRing; }

// a game frame's stereo samples (interleaved) after the ones written last, or, behind the voices or too far ahead of
// them, kAhead past where they play
void write_frame(const int16_t* pcm, int frames) {
    const uint32_t pos = play_pos(), ahead = (g_w + kRing - pos) % kRing;
    if (ahead < 400 || ahead > kRing / 2) {
        g_w = (pos + kAhead) % kRing;
        ++g_resyncs;
        // (testing, RAC1_WIIU_AUDIO_LOG=1: each resync on the log - the voices ran dry, or the writer too far ahead)
        static const bool log = std::getenv("RAC1_WIIU_AUDIO_LOG") != nullptr;
        if (log) std::printf("[audio] resync at %.0f ms: %s (%u samples ahead)\n", now_ms(), ahead > kRing / 2 ? "the voices ran dry" : "short", ahead);
    }
    uint32_t w = g_w;
    for (int i = 0; i < frames; ++i) {
        g_buf[0][w] = pcm[2 * i];
        g_buf[1][w] = pcm[2 * i + 1];
        w = (w + 1) % kRing;
    }
    const uint32_t n1 = static_cast<uint32_t>(frames) <= kRing - g_w ? static_cast<uint32_t>(frames) : kRing - g_w;
    for (int c = 0; c < 2; ++c) {
        DCFlushRange(g_buf[c] + g_w, n1 * 2);
        if (n1 < static_cast<uint32_t>(frames)) DCFlushRange(g_buf[c], (static_cast<uint32_t>(frames) - n1) * 2);
    }
    g_w = w;
    // two frames of silence after it: voices that run past the writer (a stall the alone frames did not cover) play a gap, not
    // the ring's old samples again (the stutter heard around loads)
    for (int c = 0; c < 2; ++c)
        for (uint32_t i = 0, z = w; i < 2 * kFrame; ++i, z = (z + 1) % kRing) g_buf[c][z] = 0;
    const uint32_t z1 = 2 * kFrame <= kRing - w ? 2 * kFrame : kRing - w;
    for (int c = 0; c < 2; ++c) {
        DCFlushRange(g_buf[c] + w, z1 * 2);
        if (z1 < 2 * kFrame) DCFlushRange(g_buf[c], (2 * kFrame - z1) * 2);
    }
}

// orig: (host) the IOP's own processor (audio_out.cpp's iop_loop): the mirror's frames written as the game's vsyncs come;
// alone while the game's thread is late (inside a step for over 20 ms since its last vsync, or no vsync for 100 ms) and
// the voices are about to run dry
int iop_main(int, const char**) {
    int16_t buf[kFrame * 2];
    double last = 0.0;
    int run = 0;
    while (g_iop_run.load(std::memory_order_relaxed)) {
        // paced: past kAhead + half a frame ahead of the voices (and not past half the ring: there the voices ran past the writer),
        // the next vsync waits - with a backlog after a stall (the game's frames caught up at once) written as it came, the writer
        // went past half the ring and was put back (the sound skipped, around every load); a backlog of 16 vsyncs or more not waited for
        if (const uint32_t l = lead(); l > kAhead + kFrame / 2 && l < kRing / 2 && rac1::snd989_mirror_queued() < 16) {
            OSSleepTicks(OSMillisecondsToTicks(2));
            continue;
        }
        bool did = false;
        if (rac1::snd989_mirror_next(buf, nullptr, 2)) {
            write_frame(buf, kFrame);
            last = now_ms();
            did = true;
        }
        if (did) {
            if (run != 0) {
                ++g_alone_runs;
                run = 0;
            }
            continue;
        }
        const double t = now_ms(), step_t0 = g_step_t0_ms.load(std::memory_order_relaxed);
        // While a .pss movie is open (30 fps: 2 vsyncs every 33.3 ms, with movie_hw_audio_take samples recorded per vsync for
        // snd989_mirror_next), do not trigger the 20 ms stall heuristic in the middle of a movie frame decode.
        const bool movie_open = rac1::movie_hw_is_open();
        // Between steps (a load's host work: step_t0 0) after 34 ms without a vsync - two frames; the voices hold kAhead, 50 ms: at
        // 100 ms they ran dry (the sound stalled right before a load, a landing's); with a movie open still 100 ms
        const bool stalled = (!movie_open && step_t0 != 0.0 && t - step_t0 > 20.0 && t - last > 20.0) || t - last > (movie_open ? 100.0 : 34.0);
        if (g_alone_ok.load(std::memory_order_relaxed) && last != 0.0 && stalled && lead() < static_cast<uint32_t>(kFrame + kFrame / 2)) {
            ++run;
            rac1::snd989_mirror_alone(buf, nullptr, 2);
            write_frame(buf, kFrame);
            g_alone_total.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        rac1::snd989_mirror_wait(2.0);
    }
    return 0;
}
}  // namespace

bool audio_out_start(int, bool) {  // (stereo s16 alone: the high-precision mix not offered here)
    if (g_on) return true;
    AXInitParams p{};
    p.renderer = AX_INIT_RENDERER_48KHZ;
    p.pipeline = AX_INIT_PIPELINE_SINGLE;
    AXInitWithParams(&p);
    for (int c = 0; c < 2; ++c) {
        g_buf[c] = static_cast<int16_t*>(std::aligned_alloc(0x40, kRing * 2));
        if (g_buf[c] == nullptr) return false;
        std::memset(g_buf[c], 0, kRing * 2);
        DCFlushRange(g_buf[c], kRing * 2);
        g_voice[c] = AXAcquireVoice(31, nullptr, nullptr);
        if (g_voice[c] == nullptr) return false;
    }
    for (int c = 0; c < 2; ++c) {
        AXVoice* v = g_voice[c];
        AXVoiceBegin(v);
        AXSetVoiceType(v, 0);
        AXVoiceVeData ve{0x8000, 0};
        AXSetVoiceVe(v, &ve);
        AXVoiceDeviceMixData mix[6] = {};
        mix[c].bus[0].volume = 0x8000;  // (channel c of the device: left 0, right 1)
        AXSetVoiceDeviceMix(v, AX_DEVICE_TYPE_TV, 0, mix);
        AXSetVoiceDeviceMix(v, AX_DEVICE_TYPE_DRC, 0, mix);
        AXSetVoiceSrcType(v, AX_VOICE_SRC_TYPE_NONE);
        AXSetVoiceSrcRatio(v, 1.0f);
        AXVoiceOffsets o{};
        o.dataType = AX_VOICE_FORMAT_LPCM16;
        o.loopingEnabled = AX_VOICE_LOOP_ENABLED;
        o.loopOffset = 0;
        o.endOffset = kRing - 1;
        o.currentOffset = 0;
        o.data = g_buf[c];
        AXSetVoiceOffsets(v, &o);
        AXVoiceEnd(v);
    }
    for (int c = 0; c < 2; ++c) AXSetVoiceState(g_voice[c], AX_VOICE_STATE_PLAYING);
    g_w = kAhead;
    g_on = true;
    if (rac1::snd989_mirror_enabled()) {
        g_iop_run.store(true);
        // (its core: RAC1_WIIU_IOP_CORE, default 0 - the workers' core, idle most of a frame; 2 was the game thread's, 13% of it on
        // Kerwan)
        const char* ce = std::getenv("RAC1_WIIU_IOP_CORE");
        g_iop_core = ce != nullptr ? std::atoi(ce) : 0;
        if (g_iop_core < 0 || g_iop_core > 2) g_iop_core = 0;
        if (OSCreateThread(&g_iop, iop_main, 0, nullptr, g_iop_stack + sizeof g_iop_stack, sizeof g_iop_stack, 10,
                           static_cast<OSThreadAttributes>(OS_THREAD_ATTRIB_AFFINITY_CPU0 << g_iop_core))) {
            OSSetThreadName(&g_iop, "rac1 IOP mirror");
            OSResumeThread(&g_iop);
        } else {
            g_iop_run.store(false);
        }
    }
    if (g_iop_run.load())
        std::printf("[wiiu] audio: AX at 48 kHz, two voices, %u samples ahead; the IOP's mirror on core %d\n", kAhead, g_iop_core);
    else
        std::printf("[wiiu] audio: AX at 48 kHz, two voices, %u samples ahead; the game's thread pushes its IOP's output\n", kAhead);
    return true;
}

void audio_out_push(const int16_t* pcm, const int16_t*, int frames, int, const float*, const float*) {
    if (!g_on || frames <= 0 || g_iop_run.load(std::memory_order_relaxed)) return;
    write_frame(pcm, frames);
}

// (Options > Sound's Mix: the Wii U's output is the s16 mix alone - High Precision changes nothing here)
bool audio_out_hp() { return false; }
void audio_out_set_hp(bool) {}

void audio_out_set_channels(int) {}
void audio_out_set_alone(bool ok) { g_alone_ok.store(ok, std::memory_order_relaxed); }
void audio_out_step(bool in_step) { g_step_t0_ms.store(in_step ? now_ms() : 0.0, std::memory_order_relaxed); }
int audio_out_channels() { return 2; }
uint64_t audio_out_alone_total() { return g_alone_total.load(std::memory_order_relaxed); }

void audio_out_stop() {
    if (!g_on) return;
    if (g_iop_run.exchange(false)) {
        int rc = 0;
        OSJoinThread(&g_iop, &rc);
        rac1::snd989_mirror_report();
    }
    for (int c = 0; c < 2; ++c) {
        AXSetVoiceState(g_voice[c], AX_VOICE_STATE_STOPPED);
        AXFreeVoice(g_voice[c]);
    }
    AXQuit();
    g_on = false;
    std::printf("[wiiu] audio: %llu resyncs, %llu frames alone in %llu runs\n", static_cast<unsigned long long>(g_resyncs),
                static_cast<unsigned long long>(g_alone_total.load()), static_cast<unsigned long long>(g_alone_runs));
}
