// The game's sound on the Wii U (src/host/orig_main.cpp's audio_out_*, as audio_out.cpp's on the PC): the game's IOP's
// output (989snd's, rendered every vsync on the game's thread: RAC1_IOP_MIRROR=0) written into two looping LPCM16
// buffers that two AX voices play, the left and the right channel, on the TV and the GamePad. The writer stays ~3 game
// frames ahead of the voices' play position, and starts again from there when it falls behind (the game slower than real
// time) or runs too far ahead. Host code.
#include <coreinit/cache.h>
#include <sndcore2/core.h>
#include <sndcore2/device.h>
#include <sndcore2/voice.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
constexpr uint32_t kRing = 9600;     // each channel's buffer: 0.2 s at 48 kHz (12 game frames)
constexpr uint32_t kAhead = 2400;    // the writer's lead: 3 game frames (50 ms)
int16_t* g_buf[2] = {};              // the left and right channels' buffers (the DSP reads them: flushed after each write)
AXVoice* g_voice[2] = {};
uint32_t g_w = 0;                    // where the next frame is written
bool g_on = false;
uint64_t g_resyncs = 0;

uint32_t play_pos() {
    AXVoiceOffsets o{};
    AXGetVoiceOffsets(g_voice[0], &o);
    return o.currentOffset % kRing;
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
    std::printf("[wiiu] audio: AX at 48 kHz, two voices, %u samples ahead\n", kAhead);
    return true;
}

// a game frame's stereo samples (interleaved) after the ones written last, or, behind the voices or too far ahead of
// them, kAhead past where they play
void audio_out_push(const int16_t* pcm, const int16_t*, int frames) {
    if (!g_on || frames <= 0) return;
    const uint32_t pos = play_pos(), lead = (g_w + kRing - pos) % kRing;
    if (lead < 400 || lead > kRing / 2) {
        g_w = (pos + kAhead) % kRing;
        ++g_resyncs;
    }
    uint32_t w = g_w;
    for (int i = 0; i < frames; ++i) {
        g_buf[0][w] = pcm[2 * i];
        g_buf[1][w] = pcm[2 * i + 1];
        w = (w + 1) % kRing;
    }
    // (the two stretches written, flushed for the DSP)
    const uint32_t n1 = static_cast<uint32_t>(frames) <= kRing - g_w ? static_cast<uint32_t>(frames) : kRing - g_w;
    for (int c = 0; c < 2; ++c) {
        DCFlushRange(g_buf[c] + g_w, n1 * 2);
        if (n1 < static_cast<uint32_t>(frames)) DCFlushRange(g_buf[c], (static_cast<uint32_t>(frames) - n1) * 2);
    }
    g_w = w;
}

void audio_out_set_channels(int) {}
void audio_out_set_alone(bool) {}
void audio_out_step(bool) {}
int audio_out_channels() { return 2; }
uint64_t audio_out_alone_total() { return 0; }

void audio_out_stop() {
    if (!g_on) return;
    for (int c = 0; c < 2; ++c) {
        AXSetVoiceState(g_voice[c], AX_VOICE_STATE_STOPPED);
        AXFreeVoice(g_voice[c]);
    }
    AXQuit();
    g_on = false;
    std::printf("[wiiu] audio: %llu resyncs\n", static_cast<unsigned long long>(g_resyncs));
}
