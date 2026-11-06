// The high-precision mix's end (Options > Sound's Mix: High Precision): a look-ahead peak limiter on the float output.
//
// The high-precision mix sums the voices, the reverb and the master volume in float without the SPU2's 16-bit clamps
// (Synth::Tick), so a loud scene can go past full scale where the original clips. Before the device (miniaudio's f32,
// whose system mixer would clip it again) the sum goes through this limiter: below its threshold (-0.2 dBFS) it leaves
// the samples untouched; above, it lowers the gain smoothly, the channels together, so that no sample passes the
// threshold. The gain is the minimum of each frame's needed gain over a 1 ms window (held, then released over ~80 ms),
// averaged over the same window, and applied to the samples delayed by that window (1 ms of latency, in this mode only):
// it reaches the needed gain by the time the loud frame comes out, without a step, so without a click.
//
// Host only; it sees only the output samples, never anything the game reads.
#ifndef RAC1_HOST_AUDIO_LIMITER_H
#define RAC1_HOST_AUDIO_LIMITER_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>

namespace rac1 {

class AudioLimiter {
public:
    static constexpr int kLook = 48;                     // 1 ms at 48 kHz: the look-ahead, the delay + 1
    static constexpr int kMaxCh = 8;
    static constexpr float kThreshold = 32768.0f * 0.977f;  // -0.2 dBFS, on the s16 scale

    // `frames` frames of `channels` interleaved floats (s16 scale), limited in place (delayed by kLook - 1 frames)
    void process(float* x, int frames, int channels) {
        if (channels != ch_) reset(channels);
        for (int i = 0; i < frames; ++i) {
            float* f = x + i * channels;
            float peak = 0.0f;
            for (int c = 0; c < channels; ++c) peak = std::max(peak, std::fabs(f[c]));
            peak_in_ = std::max(peak_in_, peak);
            if (peak > 32767.0f) ++over_frames_;
            const float need = peak > kThreshold ? kThreshold / peak : 1.0f;
            need_[pos_] = need;
            float h = 1.0f;
            for (float n : need_) h = std::min(h, n);
            // held at the window's minimum; released towards it (up) over ~80 ms
            // (within 1e-5 of it, there: a float release never quite reaches 1, which would leave the gain a hair under it)
            rel_ = h < rel_ || h - rel_ < 1e-5f ? h : rel_ + (h - rel_) * kRelease;
            box_[pos_] = rel_;
            float sum = 0.0f;
            for (float b : box_) sum += b;
            const float g = std::min(sum / static_cast<float>(kLook), 1.0f);  // (48 / 48 exactly 1: untouched below the threshold)
            if (g < 1.0f) ++limited_frames_;
            for (int c = 0; c < channels; ++c) {
                float& d = delay_[pos_ * kMaxCh + c];
                const float in = f[c];
                d = in;
                f[c] = delay_[((pos_ + 1) % kLook) * kMaxCh + c] * g;  // (the frame of kLook - 1 frames ago)
            }
            pos_ = (pos_ + 1) % kLook;
        }
    }

    void reset(int channels) {
        ch_ = channels;
        pos_ = 0;
        rel_ = 1.0f;
        std::fill(std::begin(need_), std::end(need_), 1.0f);
        std::fill(std::begin(box_), std::end(box_), 1.0f);
        std::fill(std::begin(delay_), std::end(delay_), 0.0f);
    }

    // (the counters since the start: the loudest sample before the limiter, the frames past full scale before it, the
    // frames it lowered)
    float peak_in() const { return peak_in_; }
    uint64_t over_frames() const { return over_frames_; }
    uint64_t limited_frames() const { return limited_frames_; }

private:
    static constexpr float kRelease = 1.0f / (0.080f * 48000.0f);
    int ch_ = 0;
    int pos_ = 0;
    float rel_ = 1.0f;
    float need_[kLook] = {};
    float box_[kLook] = {};
    float delay_[kLook * kMaxCh] = {};
    float peak_in_ = 0.0f;
    uint64_t over_frames_ = 0, limited_frames_ = 0;
};

}  // namespace rac1

#endif
