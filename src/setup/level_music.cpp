// See level_music.h. Not a port: our own tool.
#include "level_music.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace rac1::setup {

namespace {

constexpr uint32_t kChunk = 0x800;  // a stereo VAG's interleave (runtime/iop/snd989_stream.cpp's kStereoChunk)
constexpr uint32_t kMinRate = 8000, kMaxRate = 48000;

std::string fmt(const char* f, ...) {
    char b[512];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(b, sizeof b, f, ap);
    va_end(ap);
    return b;
}

uint32_t be32(const std::vector<uint8_t>& b, size_t at) {
    return at + 4 <= b.size() ? static_cast<uint32_t>(b[at] << 24 | b[at + 1] << 16 | b[at + 2] << 8 | b[at + 3]) : 0;
}
uint32_t le32(const std::vector<uint8_t>& b, size_t at) {
    return at + 4 <= b.size() ? static_cast<uint32_t>(b[at] | b[at + 1] << 8 | b[at + 2] << 16 | static_cast<uint32_t>(b[at + 3]) << 24) : 0;
}
uint32_t le16(const std::vector<uint8_t>& b, size_t at) { return at + 2 <= b.size() ? static_cast<uint32_t>(b[at] | b[at + 1] << 8) : 0; }
void put_be32(std::vector<uint8_t>& b, size_t at, uint32_t v) {
    b[at] = static_cast<uint8_t>(v >> 24);
    b[at + 1] = static_cast<uint8_t>(v >> 16);
    b[at + 2] = static_cast<uint8_t>(v >> 8);
    b[at + 3] = static_cast<uint8_t>(v);
}

// a VAG of `channels` channels' ADPCM data (each `size` bytes): the header, then the data (stereo: interleaved)
std::vector<uint8_t> vag_write(const std::vector<std::vector<uint8_t>>& ch, uint32_t rate, const uint8_t* name_from = nullptr) {
    const uint32_t size = static_cast<uint32_t>(ch[0].size());
    std::vector<uint8_t> v(0x30, 0);
    if (name_from != nullptr) std::memcpy(v.data(), name_from, 0x30);
    std::memcpy(v.data(), "VAGp", 4);
    put_be32(v, 4, 0x20);
    put_be32(v, 12, size);
    put_be32(v, 16, rate);
    v[0x1e] = ch.size() == 2 ? 2 : 0;
    if (ch.size() == 1) {
        v.insert(v.end(), ch[0].begin(), ch[0].end());
        return v;
    }
    for (uint32_t x = 0; x < size; x += kChunk)
        for (const std::vector<uint8_t>& c : ch) {
            const uint32_t n = std::min(kChunk, size - x);
            v.insert(v.end(), c.begin() + x, c.begin() + x + n);
            v.insert(v.end(), kChunk - n, 0);
        }
    return v;
}

// ---- a VAG: as it is, or a mono one made stereo ----
bool from_vag(const std::vector<uint8_t>& in, int channels, MusicVag& out, std::string& why) {
    const uint32_t size = be32(in, 12), rate = be32(in, 16);
    const int ch = in[0x1e] == 2 ? 2 : 1;
    const uint64_t need = ch == 1 ? uint64_t{0x30} + size : uint64_t{0x30} + (uint64_t{(size + kChunk - 1) / kChunk} * 2 - 1) * kChunk + (size - 1) % kChunk + 1;
    if (size < 16 || need > in.size() || rate < kMinRate || rate > kMaxRate) {
        why = fmt("its VAG header's size %u or rate %u do not fit the file (%zu bytes%s)", size, rate, in.size(), ch == 2 ? ", stereo" : "");
        return false;
    }
    out.rate = rate;
    out.samples = uint64_t{size} / 16 * 28;
    if (ch == 2 || channels != 2) {
        out.file = in;
        out.channels = ch;
        out.how = ch == 2 ? "a stereo VAG" : "a VAG";
        return true;
    }
    const std::vector<uint8_t> d(in.begin() + 0x30, in.begin() + 0x30 + size);
    out.file = vag_write({d, d}, rate, in.data());
    out.channels = 2;
    out.how = "a VAG, mono made stereo (the same on both channels)";
    return true;
}

// ---- a WAV: its channels as samples (16-bit full scale), its rate ----
bool read_wav(const std::vector<uint8_t>& b, std::vector<std::vector<float>>& ch, uint32_t& rate, std::string& desc, std::string& why) {
    uint32_t tag = 0, channels = 0, bits = 0, block = 0;
    size_t data = 0, data_size = 0;
    for (size_t at = 12; at + 8 <= b.size();) {
        const uint32_t n = le32(b, at + 4);
        if (std::memcmp(b.data() + at, "fmt ", 4) == 0) {
            tag = le16(b, at + 8);
            channels = le16(b, at + 10);
            rate = le32(b, at + 12);
            block = le16(b, at + 20);
            bits = le16(b, at + 22);
            if (tag == 0xfffe && n >= 40) tag = le16(b, at + 8 + 24);  // WAVE_FORMAT_EXTENSIBLE: its sub-format
        } else if (std::memcmp(b.data() + at, "data", 4) == 0) {
            data = at + 8;
            data_size = std::min<size_t>(n, b.size() - data);
        }
        at += 8 + static_cast<size_t>(n) + (n & 1);
    }
    const bool pcm = tag == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32), flt = tag == 3 && bits == 32;
    if ((!pcm && !flt) || channels < 1 || channels > 2 || data == 0 || block != channels * bits / 8 || rate == 0) {
        why = "a WAV of another kind (PCM 8, 16, 24 or 32 bits or float, mono or stereo only)";
        return false;
    }
    const size_t frames = data_size / block;
    if (frames == 0) {
        why = "an empty WAV";
        return false;
    }
    ch.assign(channels, std::vector<float>(frames));
    for (size_t i = 0; i < frames; ++i)
        for (uint32_t c = 0; c < channels; ++c) {
            const size_t at = data + i * block + c * bits / 8;
            float x = 0;
            if (flt) {
                const uint32_t u = le32(b, at);
                float f;
                std::memcpy(&f, &u, 4);
                x = f * 32768.0f;
            } else if (bits == 8) {
                x = (static_cast<float>(b[at]) - 128.0f) * 256.0f;
            } else if (bits == 16) {
                x = static_cast<float>(static_cast<int16_t>(le16(b, at)));
            } else if (bits == 24) {
                x = static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(b[at]) << 8 | static_cast<uint32_t>(b[at + 1]) << 16 |
                                                            static_cast<uint32_t>(b[at + 2]) << 24)) / 65536.0f;
            } else {
                x = static_cast<float>(static_cast<int32_t>(le32(b, at))) / 65536.0f;
            }
            ch[c][i] = x;
        }
    desc = fmt("%u Hz, %u bits%s, %s", rate, bits, flt ? " float" : "", channels == 2 ? "stereo" : "mono");
    return true;
}

// ---- resampling: `in` to `n_out` samples over the same time (windowed sinc, Kaiser); `periodic`: `in` is a loop (its
// end runs on into its start), else silence round it ----
double bessel_i0(double x) {
    double s = 1, t = 1;
    for (int k = 1; k < 40; ++k) {
        t *= (x / (2 * k)) * (x / (2 * k));
        s += t;
    }
    return s;
}

std::vector<float> resample(const std::vector<float>& in, size_t n_out, bool periodic) {
    const size_t n_in = in.size();
    const double ratio = static_cast<double>(n_out) / static_cast<double>(n_in);  // output samples per input sample
    const double cut = std::min(1.0, ratio) * 0.97;                                 // the pass band, of the input's Nyquist
    const int half = static_cast<int>(std::ceil(24.0 / cut));                       // taps each side
    constexpr int kPhases = 1024;
    const double beta = 9.0, i0b = bessel_i0(beta);
    // the kernel at t = k / kPhases, k = 0 .. half * kPhases (+1 for the interpolation)
    std::vector<float> table(static_cast<size_t>(half) * kPhases + 2);
    for (size_t k = 0; k < table.size(); ++k) {
        const double t = static_cast<double>(k) / kPhases;
        const double r = t / half;
        const double w = r >= 1 ? 0 : bessel_i0(beta * std::sqrt(1 - r * r)) / i0b;
        const double x = 3.14159265358979323846 * cut * t;
        table[k] = static_cast<float>(cut * (t == 0 ? 1.0 : std::sin(x) / x) * w);
    }
    auto kernel = [&](double t) {
        const double a = std::abs(t) * kPhases;
        const size_t i = static_cast<size_t>(a);
        if (i + 1 >= table.size()) return 0.0f;
        const float f = static_cast<float>(a - static_cast<double>(i));
        return table[i] + (table[i + 1] - table[i]) * f;
    };
    std::vector<float> out(n_out);
    for (size_t j = 0; j < n_out; ++j) {
        const double pos = static_cast<double>(j) / ratio;
        const long long c = static_cast<long long>(std::floor(pos));
        double acc = 0;
        for (long long i = c - half + 1; i <= c + half; ++i) {
            long long k = i;
            if (periodic) {
                k %= static_cast<long long>(n_in);
                if (k < 0) k += static_cast<long long>(n_in);
            } else if (k < 0 || k >= static_cast<long long>(n_in)) {
                continue;
            }
            acc += static_cast<double>(in[static_cast<size_t>(k)]) * kernel(pos - static_cast<double>(i));
        }
        out[j] = static_cast<float>(acc);
    }
    return out;
}

// ---- PS-ADPCM, as the SPU2 decodes it (third_party/989snd's Voice::DecodeSamples): 16-byte blocks of 28 samples, each
// the filter and shift of least error. `independent_first`: the first block's filter 0 (it does not use the samples
// before it: a loop's start sounds the same after the loop's end as after an intro). `loop`: the first block marks the
// loop's start, the last its end (and repeat), else the last ends the voice (the streams set their own flags). ----
std::vector<uint8_t> adpcm_encode(const std::vector<int16_t>& pcm, bool loop, bool independent_first) {
    static const int coefs[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};
    std::vector<uint8_t> out;
    out.reserve(pcm.size() / 28 * 16);
    int32_t s1 = 0, s2 = 0;
    for (size_t b = 0; b + 28 <= pcm.size(); b += 28) {
        int64_t best_err = -1;
        int best_f = 0, best_shift = 0;
        int32_t best_s1 = 0, best_s2 = 0;
        uint8_t best_nib[28] = {};
        const int filters = independent_first && b == 0 ? 1 : 5;
        for (int f = 0; f < filters; ++f) {
            const int c1 = coefs[f][0], c2 = coefs[f][1];
            int32_t p1 = s1, p2 = s2, peak = 0;
            for (size_t i = 0; i < 28; ++i) {
                const int32_t x = pcm[b + i];
                peak = std::max(peak, std::abs(x - (((p1 * c1) >> 6) + ((p2 * c2) >> 6))));
                p2 = p1;
                p1 = x;
            }
            int est = 12;
            while (est > 0 && (7 << (12 - est)) < peak) --est;
            for (int shift = std::max(0, est - 1); shift <= std::min(12, est + 1); ++shift) {
                p1 = s1;
                p2 = s2;
                int64_t err = 0;
                uint8_t nib[28];
                for (size_t i = 0; i < 28; ++i) {
                    const int32_t x = pcm[b + i];
                    const int32_t pred = ((p1 * c1) >> 6) + ((p2 * c2) >> 6);
                    const double qd = std::round(static_cast<double>(x - pred) * (1 << shift) / 4096.0);
                    const int32_t q = std::clamp(static_cast<int32_t>(qd), -8, 7);
                    const int32_t y = std::clamp(((q * 4096) >> shift) + pred, -32768, 32767);
                    err += static_cast<int64_t>(x - y) * (x - y);
                    nib[i] = static_cast<uint8_t>(q & 15);
                    p2 = p1;
                    p1 = y;
                }
                if (best_err < 0 || err < best_err) {
                    best_err = err;
                    best_f = f;
                    best_shift = shift;
                    best_s1 = p1;
                    best_s2 = p2;
                    std::memcpy(best_nib, nib, sizeof nib);
                }
            }
        }
        s1 = best_s1;
        s2 = best_s2;
        out.push_back(static_cast<uint8_t>(best_f << 4 | best_shift));
        out.push_back(0);
        for (size_t i = 0; i < 28; i += 2) out.push_back(static_cast<uint8_t>(best_nib[i] | best_nib[i + 1] << 4));
    }
    if (loop) {
        out[1] |= 4;
        out[out.size() - 15] |= 3;
    } else {
        out[out.size() - 15] |= 1;
    }
    return out;
}

bool from_wav(const std::vector<uint8_t>& in, bool loop, int channels, uint32_t want_rate, MusicVag& out, std::string& why) {
    std::vector<std::vector<float>> ch;
    uint32_t rate = 0;
    std::string desc;
    if (!read_wav(in, ch, rate, desc, why)) return false;
    if (channels == 2 && ch.size() == 1) ch.push_back(ch[0]);
    // the rate: the WAV's (8000 .. 48000 Hz: the stream's pitch is rate * 4096 / 48000), else the nearest of those; or
    // the one asked for (an intro's and its loop's the same: the stream keeps the pitch it started with)
    const uint32_t out_rate = std::clamp(want_rate != 0 ? want_rate : rate, kMinRate, kMaxRate);
    const size_t n_in = ch[0].size();
    // the length a whole number of 28-sample ADPCM blocks: the loop's end then meets its start exactly (an intro's its
    // loop's), made so by resampling it to that length (a stretch of at most 14 samples: no audible change of pitch)
    const double exact = static_cast<double>(n_in) * out_rate / rate;
    const size_t n_out = std::max<size_t>(28, static_cast<size_t>(std::llround(exact / 28.0)) * 28);
    std::vector<std::vector<uint8_t>> data;
    for (std::vector<float>& c : ch) {
        if (n_out != n_in) c = resample(c, n_out, loop);
        std::vector<int16_t> s(n_out);
        for (size_t i = 0; i < n_out; ++i) s[i] = static_cast<int16_t>(std::clamp(std::lround(c[i]), -32768L, 32767L));
        data.push_back(adpcm_encode(s, loop, true));
    }
    out.file = vag_write(data, out_rate);
    out.rate = out_rate;
    out.channels = static_cast<int>(data.size());
    out.samples = n_out;
    out.encoded = true;
    out.how = fmt("a WAV (%s, %zu samples), encoded%s: %u Hz, %zu samples%s", desc.c_str(), n_in,
                  rate != out_rate ? " and resampled" : n_out != n_in ? " (its length stretched to whole ADPCM blocks)" : "", out_rate, n_out,
                  channels == 2 && desc.find("mono") != std::string::npos ? ", mono made stereo" : "");
    return true;
}

}  // namespace

bool music_vag_make(const std::vector<uint8_t>& in, bool loop, int channels, uint32_t rate, MusicVag& out, std::string& why) {
    out = MusicVag{};
    if (in.size() >= 0x30 && std::memcmp(in.data(), "VAGp", 4) == 0) return from_vag(in, channels, out, why);
    if (in.size() >= 12 && std::memcmp(in.data(), "RIFF", 4) == 0 && std::memcmp(in.data() + 8, "WAVE", 4) == 0) return from_wav(in, loop, channels, rate, out, why);
    why = "neither a VAG (\"VAGp\" header) nor a WAV (RIFF / WAVE)";
    return false;
}

}  // namespace rac1::setup
