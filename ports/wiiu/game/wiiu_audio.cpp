// The game's sound on the Wii U (src/host/orig_main.cpp's audio_out_*, as audio_out.cpp's on the PC): two AX voices,
// the left and the right channel on the TV and the GamePad, play two looping LPCM16 buffers. What fills them, as on the
// PC (ADR 0013): the IOP's mirror on a thread of its own on core 2, which replays the game's recorded IOP vsync by vsync
// and, while the game's thread is late (a load, a long frame), plays on alone a game frame at a time (those frames are
// then not owed by the game: audio_out_alone_total). With RAC1_IOP_MIRROR=0 the game's thread pushes its IOP's output.
// The writer stays ~3 game frames ahead of the voices' play position and starts again from there when it falls behind
// or runs too far ahead. Host code.
#include <runtime/iop/snd989_iop.h>

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
constexpr uint32_t kRing = 9600;     // each channel's buffer: 0.2 s at 48 kHz (12 game frames)
constexpr uint32_t kAhead = 2400;    // the writer's lead: 3 game frames (50 ms)
constexpr int kFrame = 800;          // a game frame's samples
int16_t* g_buf[2] = {};              // the left and right channels' buffers (the DSP reads them: flushed after each write)
AXVoice* g_voice[2] = {};
uint32_t g_w = 0;                    // where the next frame is written (the writer's: the mirror's thread, or the game's)
bool g_on = false;
uint64_t g_resyncs = 0;

// the mirror's thread
OSThread g_iop;
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
}

// orig: (host) the IOP's own processor (audio_out.cpp's iop_loop): the mirror's frames written as the game's vsyncs come;
// alone while the game's thread is late (inside a step for over 20 ms since its last vsync, or no vsync for 100 ms) and
// the voices are about to run dry
int iop_main(int, const char**) {
    int16_t buf[kFrame * 2];
    double last = 0.0;
    int run = 0;
    while (g_iop_run.load(std::memory_order_relaxed)) {
        bool did = false;
        while (rac1::snd989_mirror_next(buf, nullptr, 2)) {
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
        const bool stalled = (step_t0 != 0.0 && t - step_t0 > 20.0 && t - last > 20.0) || t - last > 100.0;
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

bool audio_out_start(int) {
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
        if (OSCreateThread(&g_iop, iop_main, 0, nullptr, g_iop_stack + sizeof g_iop_stack, sizeof g_iop_stack, 10, OS_THREAD_ATTRIB_AFFINITY_CPU2)) {
            OSSetThreadName(&g_iop, "rac1 IOP mirror");
            OSResumeThread(&g_iop);
        } else {
            g_iop_run.store(false);
        }
    }
    std::printf("[wiiu] audio: AX at 48 kHz, two voices, %u samples ahead; %s\n", kAhead,
                g_iop_run.load() ? "the IOP's mirror on core 2" : "the game's thread pushes its IOP's output");
    return true;
}

void audio_out_push(const int16_t* pcm, const int16_t*, int frames) {
    if (!g_on || frames <= 0 || g_iop_run.load(std::memory_order_relaxed)) return;
    write_frame(pcm, frames);
}

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
