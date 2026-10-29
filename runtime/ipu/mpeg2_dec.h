#pragma once
// Self-contained ISO/IEC 13818-2 (MPEG-2 Main Profile) video elementary stream
// decoder for PS2 .pss FMV streams (0xE0 video track).
//
// Zero external dependencies (no libavcodec required). Supports I, P, and B
// pictures, frame/field motion compensation, dual-prime, 16x16 & 16x8 MC,
// frame/field DCT coding, custom quantiser matrices, and both Table B.14 and
// Table B.15 VLC tables.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace rac1::mpeg2 {

inline uint8_t clamp_u8(int v) {
    return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

struct FrameBuf {
    int w = 0;
    int h = 0;
    int disp_w = 0;
    int disp_h = 0;
    std::vector<uint8_t> y;
    std::vector<uint8_t> cb;
    std::vector<uint8_t> cr;

    void ensure_alloc(int mw, int mh, int dw, int dh) {
        disp_w = dw;
        disp_h = dh;
        if (w == mw && h == mh && y.size() == static_cast<size_t>(mw) * mh) {
            return;
        }
        w = mw;
        h = mh;
        y.assign(static_cast<size_t>(mw) * mh, 16);
        cb.assign(static_cast<size_t>(mw / 2) * (mh / 2), 128);
        cr.assign(static_cast<size_t>(mw / 2) * (mh / 2), 128);
    }

    // Fast 2x2-grouped ITU-R BT.601 (studio range, matching PS2 IPU CSC) 4:2:0 -> RGBA8 conversion.
    void to_rgba(std::vector<uint8_t>& rgba, int& out_w, int& out_h) const {
        const int width = (disp_w > 0 && disp_w <= w) ? disp_w : w;
        const int height = (disp_h > 0 && disp_h <= h) ? disp_h : h;
        out_w = width;
        out_h = height;
        if (width <= 0 || height <= 0) {
            rgba.clear();
            return;
        }
        rgba.resize(static_cast<size_t>(width) * height * 4);
        const int cw = w >> 1;
        const int w_even = width & ~1;
        const int h_even = height & ~1;

        for (int py = 0; py < h_even; py += 2) {
            const uint8_t* y_row0 = y.data() + py * w;
            const uint8_t* y_row1 = y_row0 + w;
            const uint8_t* cb_row = cb.data() + (py >> 1) * cw;
            const uint8_t* cr_row = cr.data() + (py >> 1) * cw;
            uint8_t* dst0 = rgba.data() + static_cast<size_t>(py) * width * 4;
            uint8_t* dst1 = dst0 + static_cast<size_t>(width) * 4;

            for (int px = 0; px < w_even; px += 2) {
                const int U = static_cast<int>(cb_row[px >> 1]) - 128;
                const int V = static_cast<int>(cr_row[px >> 1]) - 128;
                const int r_off = 204 * V + 64;
                const int g_off = -50 * U - 104 * V + 64;
                const int b_off = 258 * U + 64;

                const int Y00 = (static_cast<int>(y_row0[px]) - 16) * 149;
                dst0[0] = clamp_u8((Y00 + r_off) >> 7);
                dst0[1] = clamp_u8((Y00 + g_off) >> 7);
                dst0[2] = clamp_u8((Y00 + b_off) >> 7);
                dst0[3] = 0x80;

                const int Y01 = (static_cast<int>(y_row0[px + 1]) - 16) * 149;
                dst0[4] = clamp_u8((Y01 + r_off) >> 7);
                dst0[5] = clamp_u8((Y01 + g_off) >> 7);
                dst0[6] = clamp_u8((Y01 + b_off) >> 7);
                dst0[7] = 0x80;

                const int Y10 = (static_cast<int>(y_row1[px]) - 16) * 149;
                dst1[0] = clamp_u8((Y10 + r_off) >> 7);
                dst1[1] = clamp_u8((Y10 + g_off) >> 7);
                dst1[2] = clamp_u8((Y10 + b_off) >> 7);
                dst1[3] = 0x80;

                const int Y11 = (static_cast<int>(y_row1[px + 1]) - 16) * 149;
                dst1[4] = clamp_u8((Y11 + r_off) >> 7);
                dst1[5] = clamp_u8((Y11 + g_off) >> 7);
                dst1[6] = clamp_u8((Y11 + b_off) >> 7);
                dst1[7] = 0x80;

                dst0 += 8;
                dst1 += 8;
            }
            if (w_even < width) {
                const int px = w_even;
                const int U = static_cast<int>(cb_row[px >> 1]) - 128;
                const int V = static_cast<int>(cr_row[px >> 1]) - 128;
                const int r_off = 204 * V + 64;
                const int g_off = -50 * U - 104 * V + 64;
                const int b_off = 258 * U + 64;
                const int Y00 = (static_cast<int>(y_row0[px]) - 16) * 149;
                dst0[0] = clamp_u8((Y00 + r_off) >> 7);
                dst0[1] = clamp_u8((Y00 + g_off) >> 7);
                dst0[2] = clamp_u8((Y00 + b_off) >> 7);
                dst0[3] = 0x80;
                const int Y10 = (static_cast<int>(y_row1[px]) - 16) * 149;
                dst1[0] = clamp_u8((Y10 + r_off) >> 7);
                dst1[1] = clamp_u8((Y10 + g_off) >> 7);
                dst1[2] = clamp_u8((Y10 + b_off) >> 7);
                dst1[3] = 0x80;
            }
        }
        if (h_even < height) {
            const int py = h_even;
            const uint8_t* y_row0 = y.data() + py * w;
            const uint8_t* cb_row = cb.data() + (py >> 1) * cw;
            const uint8_t* cr_row = cr.data() + (py >> 1) * cw;
            uint8_t* dst0 = rgba.data() + static_cast<size_t>(py) * width * 4;
            for (int px = 0; px < width; ++px) {
                const int U = static_cast<int>(cb_row[px >> 1]) - 128;
                const int V = static_cast<int>(cr_row[px >> 1]) - 128;
                const int Y00 = (static_cast<int>(y_row0[px]) - 16) * 149;
                dst0[0] = clamp_u8((Y00 + 204 * V + 64) >> 7);
                dst0[1] = clamp_u8((Y00 - 50 * U - 104 * V + 64) >> 7);
                dst0[2] = clamp_u8((Y00 + 258 * U + 64) >> 7);
                dst0[3] = 0x80;
                dst0 += 4;
            }
        }
    }
};

namespace detail {

struct BitReader {
    const uint8_t* data = nullptr;
    size_t size = 0;
    size_t bit_pos = 0;

    bool eof() const { return (bit_pos >> 3) >= size; }
    size_t bits_left() const { return (size << 3) > bit_pos ? (size << 3) - bit_pos : 0; }

    uint32_t show(int n) const {
        if (n <= 0) return 0;
        size_t byte_idx = bit_pos >> 3;
        int bit_off = static_cast<int>(bit_pos & 7);
        uint64_t val = 0;
        for (int i = 0; i < 5; ++i) {
            val = (val << 8) | (byte_idx + i < size ? data[byte_idx + i] : 0);
        }
        return static_cast<uint32_t>((val >> (40 - bit_off - n)) & ((1ULL << n) - 1));
    }

    void skip(int n) { bit_pos += static_cast<size_t>(n); }

    uint32_t get(int n) {
        uint32_t v = show(n);
        skip(n);
        return v;
    }

    int get_signed(int n) {
        uint32_t v = get(n);
        if (v & (1u << (n - 1))) {
            return static_cast<int>(v) - (1 << n);
        }
        return static_cast<int>(v);
    }
};

static constexpr uint8_t kZigZag[64] = {
    0,  1,  8, 16,  9,  2,  3, 10,
   17, 24, 32, 25, 18, 11,  4,  5,
   12, 19, 26, 33, 40, 48, 41, 34,
   27, 20, 13,  6,  7, 14, 21, 28,
   35, 42, 49, 56, 57, 50, 43, 36,
   29, 22, 15, 23, 30, 37, 44, 51,
   58, 59, 52, 45, 38, 31, 39, 46,
   53, 60, 61, 54, 47, 55, 62, 63
};

static constexpr uint8_t kAltScan[64] = {
    0,  8, 16, 24,  1,  9,  2, 10,
   17, 25, 32, 40, 48, 56, 57, 49,
   41, 33, 26, 18,  3, 11,  4, 12,
   19, 27, 34, 42, 50, 58, 35, 43,
   51, 59, 20, 28,  5, 13,  6, 14,
   21, 29, 36, 44, 52, 60, 37, 45,
   53, 61, 22, 30,  7, 15, 23, 31,
   38, 46, 54, 62, 39, 47, 55, 63
};

static constexpr uint8_t kDefaultIntraMatrix[64] = {
     8, 16, 19, 22, 26, 27, 29, 34,
    16, 16, 22, 24, 27, 29, 34, 37,
    19, 22, 26, 27, 29, 34, 34, 38,
    22, 22, 26, 27, 29, 34, 37, 40,
    22, 26, 27, 29, 32, 35, 40, 48,
    26, 27, 29, 32, 35, 40, 48, 58,
    26, 27, 29, 34, 38, 46, 56, 69,
    27, 29, 35, 38, 46, 56, 69, 83
};

static constexpr uint8_t kQScaleLinear[32] = {
     0,  2,  4,  6,  8, 10, 12, 14,
    16, 18, 20, 22, 24, 26, 28, 30,
    32, 34, 36, 38, 40, 42, 44, 46,
    48, 50, 52, 54, 56, 58, 60, 62
};

static constexpr uint8_t kQScaleNonLinear[32] = {
     0,  1,  2,  3,  4,  5,  6,  7,
     8, 10, 12, 14, 16, 18, 20, 22,
    24, 28, 32, 36, 40, 44, 48, 52,
    56, 64, 72, 80, 88, 96, 104, 112
};

enum MbFlags : uint8_t {
    MB_QUANT   = 0x01,
    MB_FORWARD = 0x02,
    MB_BACK    = 0x04,
    MB_PATTERN = 0x08,
    MB_INTRA   = 0x10
};

struct CoeffVlc {
    uint8_t run;   // 0..63, 0xFF = EOB, 0xFE = Escape, 0xFD = invalid
    uint8_t level; // 1..40
    int8_t len;    // >0: complete VLC length + 1 sign bit; <0: subtable offset
};

static constexpr uint8_t kCoeffRun[111] = {
     0,  1,  0,  2,  0,  1,  3,  4,  0,  5,  6,  7,  0,  1,  2,  3,
     8,  9,  0,  0,  1,  2, 10, 11, 12, 13,  0,  1,  3,  4,  5, 14,
    15, 16,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  2,  2,  6,
     7,  8,  9, 10, 17, 18, 19, 20, 21,  0,  0,  0,  0,  1,  1,  1,
     2,  2,  3,  3,  4,  4,  5, 22, 23, 24, 25, 26,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,
     1,  1,  2,  3,  5,  6, 11, 12, 13, 14, 15, 16, 27, 28, 29
};

static constexpr uint8_t kCoeffLevel[111] = {
     1,  1,  2,  1,  3,  2,  1,  1,  4,  1,  1,  1,  5,  3,  2,  2,
     1,  1,  6,  7,  4,  3,  1,  1,  1,  1,  8,  5,  3,  2,  2,  1,
     1,  1,  9, 10, 11, 12, 13, 14, 15, 16,  6,  7,  8,  4,  5,  2,
     2,  2,  2,  2,  1,  1,  1,  1,  1, 17, 18, 19, 20,  9, 10, 11,
     6,  7,  4,  5,  3,  4,  3,  1,  1,  1,  1,  1, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 12, 13, 14, 15, 16,
    17, 18,  8,  6,  4,  3,  2,  2,  2,  2,  2,  2,  1,  1,  1
};

static constexpr uint16_t kB14Code[111] = {
     0x03, 0x03, 0x04, 0x05, 0x05, 0x06, 0x07, 0x04, 0x0b, 0x0d, 0x0a, 0x08, 0x13, 0x1d, 0x0e, 0x1c,
     0x0f, 0x09, 0x15, 0x14, 0x1c, 0x19, 0x1b, 0x1a, 0x18, 0x17, 0x1d, 0x1c, 0x13, 0x1f, 0x12, 0x16,
     0x15, 0x14, 0x18, 0x17, 0x16, 0x1a, 0x19, 0x18, 0x17, 0x1f, 0x15, 0x14, 0x1e, 0x16, 0x1d, 0x13,
     0x11, 0x11, 0x1c, 0x1b, 0x12, 0x10, 0x1f, 0x1a, 0x19, 0x1f, 0x1e, 0x1d, 0x1c, 0x1b, 0x1a, 0x19,
     0x18, 0x17, 0x14, 0x13, 0x12, 0x16, 0x15, 0x11, 0x10, 0x1f, 0x1e, 0x1d, 0x1c, 0x1b, 0x1a, 0x19,
     0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x1a, 0x19,
     0x18, 0x17, 0x11, 0x10, 0x1f, 0x1e, 0x1d, 0x1c, 0x1b, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11
};

static constexpr uint8_t kB14Len[111] = {
     2,  3,  4,  4,  5,  5,  5,  6,  6,  6,  6,  6,  7,  7,  7,  7,
     7,  7,  8,  8,  8,  8,  8,  8,  8,  8, 10, 10, 10, 10, 10, 10,
    10, 10, 12, 12, 12, 13, 13, 13, 13, 14, 12, 12, 14, 12, 14, 12,
    12, 12, 14, 14, 12, 12, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 15, 15,
    15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16
};

static constexpr uint16_t kB15Code[111] = {
     0x02, 0x06, 0x05, 0x07, 0x09, 0x10, 0x0d, 0x0a, 0x19, 0x0b, 0x13, 0x12, 0x1b, 0x11, 0x28, 0x15,
     0x10, 0x0f, 0x21, 0x20, 0x3b, 0x3a, 0x26, 0x25, 0x24, 0x27, 0x62, 0x60, 0x38, 0x39, 0x12, 0x63,
     0x61, 0x13, 0x23, 0x22, 0x20, 0x21, 0x14, 0x15, 0x16, 0x17, 0x3f, 0x0a, 0x0b, 0x3c, 0x18, 0x3d,
     0x11, 0x10, 0x1c, 0x1b, 0x1f, 0x1f, 0x1f, 0x1a, 0x19, 0x18, 0x19, 0x1a, 0x1b, 0x10, 0x11, 0x12,
     0x19, 0x1a, 0x14, 0x13, 0x12, 0x16, 0x15, 0x11, 0x10, 0x1f, 0x1e, 0x1d, 0x1c, 0x1d, 0x1e, 0x1f,
     0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x13, 0x14, 0x15, 0x16, 0x17,
     0x1b, 0x1c, 0x11, 0x10, 0x1f, 0x1e, 0x1d, 0x1c, 0x1b, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11
};

static constexpr uint8_t kB15Len[111] = {
     2,  3,  3,  4,  4,  5,  5,  5,  5,  5,  6,  6,  5,  6,  6,  7,
     6,  6,  6,  6,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  8,  7,
     7,  8,  8,  8,  8,  8,  9,  9,  9,  9,  7,  8,  8,  7, 10,  7,
     8,  8, 14, 14,  8,  9, 10, 14, 14,  9,  9,  9,  9, 10, 10, 10,
    10, 10, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16,  9,  9,  9,  9,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 10, 10, 10, 10, 10,
    11, 11, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16
};

struct VlcTables {
    CoeffVlc b14_short[1024];
    CoeffVlc b14_long[4096];
    CoeffVlc b15_short[1024];
    CoeffVlc b15_long[4096];

    VlcTables() {
        for (int i = 0; i < 1024; ++i) {
            b14_short[i] = {0xFD, 0, 0};
            b15_short[i] = {0xFD, 0, 0};
        }
        for (int i = 0; i < 4096; ++i) {
            b14_long[i] = {0xFD, 0, 0};
            b15_long[i] = {0xFD, 0, 0};
        }

        // EOB in B.14: '10' (2 bits)
        for (int s = 0; s < 256; ++s) b14_short[(0b10 << 8) | s] = {0xFF, 0, 2};
        // Escape in B.14 & B.15: '000001' (6 bits)
        for (int s = 0; s < 16; ++s) {
            b14_short[(0b000001 << 4) | s] = {0xFE, 0, 6};
            b15_short[(0b000001 << 4) | s] = {0xFE, 0, 6};
        }
        // EOB in B.15: '0110' (4 bits)
        for (int s = 0; s < 64; ++s) b15_short[(0b0110 << 6) | s] = {0xFF, 0, 4};

        for (int i = 0; i < 111; ++i) {
            // Table B.14
            {
                const int total_len = kB14Len[i] + 1;
                if (total_len <= 10) {
                    const int shift = 10 - kB14Len[i];
                    const int base = static_cast<int>(kB14Code[i]) << shift;
                    for (int s = 0; s < (1 << shift); ++s) {
                        b14_short[base | s] = {kCoeffRun[i], kCoeffLevel[i], static_cast<int8_t>(total_len)};
                    }
                } else {
                    const int shift = 17 - kB14Len[i];
                    const int base = static_cast<int>(kB14Code[i]) << shift;
                    for (int s = 0; s < (1 << shift); ++s) {
                        b14_long[base | s] = {kCoeffRun[i], kCoeffLevel[i], static_cast<int8_t>(total_len)};
                    }
                }
            }
            // Table B.15
            {
                const int total_len = kB15Len[i] + 1;
                if (total_len <= 10) {
                    const int shift = 10 - kB15Len[i];
                    const int base = static_cast<int>(kB15Code[i]) << shift;
                    for (int s = 0; s < (1 << shift); ++s) {
                        b15_short[base | s] = {kCoeffRun[i], kCoeffLevel[i], static_cast<int8_t>(total_len)};
                    }
                } else {
                    const int shift = 17 - kB15Len[i];
                    const int base = static_cast<int>(kB15Code[i]) << shift;
                    for (int s = 0; s < (1 << shift); ++s) {
                        b15_long[base | s] = {kCoeffRun[i], kCoeffLevel[i], static_cast<int8_t>(total_len)};
                    }
                }
            }
        }
    }
};

// Loeffler/LLM 8x8 integer IDCT (matches ISO/IEC 13818-2 precision).
inline void idct_row(int16_t* blk) {
    const int x0 = blk[0], x1 = blk[4], x2 = blk[6], x3 = blk[2];
    const int x4 = blk[1], x5 = blk[7], x6 = blk[5], x7 = blk[3];
    if ((x1 | x2 | x3 | x4 | x5 | x6 | x7) == 0) {
        const int16_t v = static_cast<int16_t>(x0 << 3);
        for (int i = 0; i < 8; ++i) blk[i] = v;
        return;
    }
    constexpr int W1 = 2841, W2 = 2676, W3 = 2408, W5 = 1609, W6 = 1108, W7 = 565;
    int a0 = (x0 << 11) + 128;
    int a1 = x1 << 11;
    int a2 = W6 * (x2 + x3);
    int a3 = a2 + (W2 - W6) * x3;
    a2 = a2 - (W2 + W6) * x2;
    int b0 = a0 + a1;
    int b1 = a0 - a1;
    int s0 = b0 + a3;
    int s3 = b0 - a3;
    int s1 = b1 + a2;
    int s2 = b1 - a2;

    int c4 = W7 * (x4 + x5);
    int c5 = c4 + (W1 - W7) * x4;
    c4 = c4 - (W1 + W7) * x5;
    int c6 = W3 * (x6 + x7);
    int c7 = c6 - (W3 - W5) * x6;
    c6 = c6 - (W3 + W5) * x7;

    int d0 = c5 + c7;
    int d1 = c5 - c7;
    int d2 = c4 + c6;
    int d3 = c4 - c6;
    int e1 = (181 * (d1 + d2) + 128) >> 8;
    int e2 = (181 * (d1 - d2) + 128) >> 8;

    blk[0] = static_cast<int16_t>((s0 + d0) >> 8);
    blk[7] = static_cast<int16_t>((s0 - d0) >> 8);
    blk[1] = static_cast<int16_t>((s1 + e1) >> 8);
    blk[6] = static_cast<int16_t>((s1 - e1) >> 8);
    blk[2] = static_cast<int16_t>((s2 + e2) >> 8);
    blk[5] = static_cast<int16_t>((s2 - e2) >> 8);
    blk[3] = static_cast<int16_t>((s3 + d3) >> 8);
    blk[4] = static_cast<int16_t>((s3 - d3) >> 8);
}

inline void idct_col(int16_t* blk) {
    const int x0 = blk[0 * 8], x1 = blk[4 * 8], x2 = blk[6 * 8], x3 = blk[2 * 8];
    const int x4 = blk[1 * 8], x5 = blk[7 * 8], x6 = blk[5 * 8], x7 = blk[3 * 8];
    if ((x1 | x2 | x3 | x4 | x5 | x6 | x7) == 0) {
        const int16_t v = static_cast<int16_t>((x0 + 32) >> 6);
        for (int i = 0; i < 8; ++i) blk[i * 8] = v;
        return;
    }
    constexpr int W1 = 2841, W2 = 2676, W3 = 2408, W5 = 1609, W6 = 1108, W7 = 565;
    int a0 = (x0 << 11) + 65536;
    int a1 = x1 << 11;
    int a2 = W6 * (x2 + x3);
    int a3 = a2 + (W2 - W6) * x3;
    a2 = a2 - (W2 + W6) * x2;
    int b0 = a0 + a1;
    int b1 = a0 - a1;
    int s0 = b0 + a3;
    int s3 = b0 - a3;
    int s1 = b1 + a2;
    int s2 = b1 - a2;

    int c4 = W7 * (x4 + x5);
    int c5 = c4 + (W1 - W7) * x4;
    c4 = c4 - (W1 + W7) * x5;
    int c6 = W3 * (x6 + x7);
    int c7 = c6 - (W3 - W5) * x6;
    c6 = c6 - (W3 + W5) * x7;

    int d0 = c5 + c7;
    int d1 = c5 - c7;
    int d2 = c4 + c6;
    int d3 = c4 - c6;
    int e1 = (181 * (d1 + d2) + 128) >> 8;
    int e2 = (181 * (d1 - d2) + 128) >> 8;

    blk[0 * 8] = static_cast<int16_t>((s0 + d0) >> 17);
    blk[7 * 8] = static_cast<int16_t>((s0 - d0) >> 17);
    blk[1 * 8] = static_cast<int16_t>((s1 + e1) >> 17);
    blk[6 * 8] = static_cast<int16_t>((s1 - e1) >> 17);
    blk[2 * 8] = static_cast<int16_t>((s2 + e2) >> 17);
    blk[5 * 8] = static_cast<int16_t>((s2 - e2) >> 17);
    blk[3 * 8] = static_cast<int16_t>((s3 + d3) >> 17);
    blk[4 * 8] = static_cast<int16_t>((s3 - d3) >> 17);
}

inline void idct8x8(int16_t block[64]) {
    for (int i = 0; i < 8; ++i) idct_row(block + i * 8);
    for (int i = 0; i < 8; ++i) idct_col(block + i);
}

inline int decode_mb_address_increment(BitReader& br) {
    int inc = 0;
    while (!br.eof()) {
        uint32_t v = br.show(11);
        if (v == 0x00F) { // macroblock_stuffing
            br.skip(11);
            continue;
        }
        if (v == 0x008) { // macroblock_escape (+33)
            br.skip(11);
            inc += 33;
            continue;
        }
        if (v >= 1024) { br.skip(1); return inc + 1; }
        if (v >= 512) {
            uint32_t c = v >> 8;
            br.skip(3);
            return inc + (c == 3 ? 2 : 3);
        }
        if (v >= 256) {
            uint32_t c = v >> 7;
            br.skip(4);
            return inc + (c == 3 ? 4 : 5);
        }
        if (v >= 128) {
            uint32_t c = v >> 6;
            br.skip(5);
            return inc + (c == 3 ? 6 : 7);
        }
        if (v >= 96) {
            uint32_t c = v >> 4;
            br.skip(7);
            return inc + (c == 7 ? 8 : 9);
        }
        if (v >= 36) {
            uint32_t c = v >> 3;
            br.skip(8);
            return inc + (10 + (11 - static_cast<int>(c)));
        }
        if (v >= 24) {
            uint32_t c = v >> 1;
            br.skip(10);
            return inc + (16 + (17 - static_cast<int>(c)));
        }
        if (v >= 15) {
            br.skip(11);
            return inc + (22 + (23 - static_cast<int>(v)));
        }
        return -1;
    }
    return -1;
}

inline uint8_t decode_mb_type(BitReader& br, int pic_type) {
    using namespace detail;
    if (pic_type == 1) { // I-picture
        if (br.get(1) == 1) return MB_INTRA;
        if (br.get(1) == 1) return MB_INTRA | MB_QUANT;
        return 0;
    } else if (pic_type == 2) { // P-picture
        uint32_t v = br.show(6);
        if (v >= 32) { br.skip(1); return MB_FORWARD | MB_PATTERN; }
        if (v >= 16) { br.skip(2); return MB_PATTERN; }
        if (v >= 8)  { br.skip(3); return MB_FORWARD; }
        if (v == 6 || v == 7) { br.skip(5); return MB_INTRA; }
        if (v == 4 || v == 5) { br.skip(5); return MB_QUANT | MB_FORWARD | MB_PATTERN; }
        if (v == 2 || v == 3) { br.skip(5); return MB_QUANT | MB_PATTERN; }
        if (v == 1) { br.skip(6); return MB_QUANT | MB_INTRA; }
        return 0;
    } else if (pic_type == 3) { // B-picture
        uint32_t v = br.show(6);
        if (v >= 32) { br.skip(2); return (v >= 48) ? (MB_FORWARD | MB_BACK | MB_PATTERN) : (MB_FORWARD | MB_BACK); }
        if (v >= 16) { br.skip(3); return (v >= 24) ? (MB_BACK | MB_PATTERN) : MB_BACK; }
        if (v >= 8)  { br.skip(4); return (v >= 12) ? (MB_FORWARD | MB_PATTERN) : MB_FORWARD; }
        if (v == 6 || v == 7) { br.skip(5); return MB_INTRA; }
        if (v == 4 || v == 5) { br.skip(5); return MB_QUANT | MB_FORWARD | MB_BACK | MB_PATTERN; }
        if (v == 3) { br.skip(6); return MB_QUANT | MB_FORWARD | MB_PATTERN; }
        if (v == 2) { br.skip(6); return MB_QUANT | MB_BACK | MB_PATTERN; }
        if (v == 1) { br.skip(6); return MB_QUANT | MB_INTRA; }
        return 0;
    }
    return 0;
}

inline int decode_cbp(BitReader& br) {
    struct Entry { uint16_t code; uint8_t len; uint8_t cbp; };
    static constexpr Entry kCbp[64] = {
        {0b111, 3, 60},       {0b1101, 4, 4},       {0b1100, 4, 8},       {0b1011, 4, 16},
        {0b1010, 4, 32},      {0b10011, 5, 12},     {0b10010, 5, 48},     {0b10001, 5, 20},
        {0b10000, 5, 40},     {0b01111, 5, 28},     {0b01110, 5, 44},     {0b01101, 5, 52},
        {0b01100, 5, 56},     {0b01011, 5, 1},      {0b01010, 5, 61},     {0b01001, 5, 2},
        {0b01000, 5, 62},     {0b001111, 6, 24},    {0b001110, 6, 36},    {0b001101, 6, 3},
        {0b001100, 6, 63},    {0b0010111, 7, 5},    {0b0010110, 7, 9},    {0b0010101, 7, 17},
        {0b0010100, 7, 33},   {0b0010011, 7, 6},    {0b0010010, 7, 10},   {0b0010001, 7, 18},
        {0b0010000, 7, 34},   {0b00011111, 8, 7},   {0b00011110, 8, 11},  {0b00011101, 8, 19},
        {0b00011100, 8, 35},  {0b00011011, 8, 13},  {0b00011010, 8, 49},  {0b00011001, 8, 21},
        {0b00011000, 8, 41},  {0b00010111, 8, 14},  {0b00010110, 8, 50},  {0b00010101, 8, 22},
        {0b00010100, 8, 42},  {0b00010011, 8, 15},  {0b00010010, 8, 51},  {0b00010001, 8, 23},
        {0b00010000, 8, 43},  {0b00001111, 8, 25},  {0b00001110, 8, 37},  {0b00001101, 8, 26},
        {0b00001100, 8, 38},  {0b00001011, 8, 29},  {0b00001010, 8, 45},  {0b00001001, 8, 53},
        {0b00001000, 8, 57},  {0b00000111, 8, 30},  {0b00000110, 8, 46},  {0b00000101, 8, 54},
        {0b00000100, 8, 58},  {0b000000111, 9, 31}, {0b000000110, 9, 47}, {0b000000101, 9, 55},
        {0b000000100, 9, 59}, {0b000000011, 9, 27}, {0b000000010, 9, 39}, {0b000000001, 9, 0}
    };
    uint32_t v = br.show(9);
    for (const auto& e : kCbp) {
        if ((v >> (9 - e.len)) == e.code) {
            br.skip(e.len);
            return e.cbp;
        }
    }
    return -1;
}

inline int decode_mv_code(BitReader& br) {
    if (br.get(1) == 1) return 0;
    struct Entry { uint16_t code; uint8_t len; uint8_t val; };
    static constexpr Entry kMv[16] = {
        {0b01, 2, 1},          {0b001, 3, 2},         {0b0001, 4, 3},
        {0b000011, 6, 4},      {0b0000101, 7, 5},     {0b0000100, 7, 6},
        {0b0000011, 7, 7},     {0b000001011, 9, 8},   {0b000001010, 9, 9},
        {0b000001001, 9, 10},  {0b0000010001, 10, 11},{0b0000010000, 10, 12},
        {0b0000001111, 10, 13},{0b0000001110, 10, 14},{0b0000001101, 10, 15},
        {0b0000001100, 10, 16}
    };
    uint32_t v = br.show(9);
    for (const auto& e : kMv) {
        if ((v >> (10 - e.len)) == e.code) {
            br.skip(e.len - 1);
            int sign = br.get(1);
            return sign ? -static_cast<int>(e.val) : static_cast<int>(e.val);
        }
    }
    return 0;
}

inline int decode_motion_vector(BitReader& br, int r_size, int pred) {
    int code = decode_mv_code(br);
    int residual = (r_size > 0 && code != 0) ? static_cast<int>(br.get(r_size)) : 0;
    int r_max = 16 << r_size;
    int r_min = -(16 << r_size);
    int range = 32 << r_size;
    int delta = 0;
    if (code > 0) {
        delta = ((code - 1) << r_size) + residual + 1;
    } else if (code < 0) {
        delta = -(((-code - 1) << r_size) + residual + 1);
    }
    int mv = pred + delta;
    if (mv >= r_max) mv -= range;
    else if (mv < r_min) mv += range;
    return mv;
}

inline int decode_dc_size_luma(BitReader& br) {
    uint32_t v = br.show(9);
    if ((v >> 6) == 0b100)       { br.skip(3); return 0; }
    if ((v >> 7) == 0b00)        { br.skip(2); return 1; }
    if ((v >> 7) == 0b01)        { br.skip(2); return 2; }
    if ((v >> 6) == 0b101)       { br.skip(3); return 3; }
    if ((v >> 6) == 0b110)       { br.skip(3); return 4; }
    if ((v >> 5) == 0b1110)      { br.skip(4); return 5; }
    if ((v >> 4) == 0b11110)     { br.skip(5); return 6; }
    if ((v >> 3) == 0b111110)    { br.skip(6); return 7; }
    if ((v >> 2) == 0b1111110)   { br.skip(7); return 8; }
    if ((v >> 1) == 0b11111110)  { br.skip(8); return 9; }
    if (v == 0b111111110)        { br.skip(9); return 10; }
    if (v == 0b111111111)        { br.skip(9); return 11; }
    return 0;
}

inline int decode_dc_size_chroma(BitReader& br) {
    uint32_t v = br.show(10);
    if ((v >> 8) == 0b00)          { br.skip(2); return 0; }
    if ((v >> 8) == 0b01)          { br.skip(2); return 1; }
    if ((v >> 8) == 0b10)          { br.skip(2); return 2; }
    if ((v >> 7) == 0b110)         { br.skip(3); return 3; }
    if ((v >> 6) == 0b1110)        { br.skip(4); return 4; }
    if ((v >> 5) == 0b11110)       { br.skip(5); return 5; }
    if ((v >> 4) == 0b111110)      { br.skip(6); return 6; }
    if ((v >> 3) == 0b1111110)     { br.skip(7); return 7; }
    if ((v >> 2) == 0b11111110)    { br.skip(8); return 8; }
    if ((v >> 1) == 0b111111110)   { br.skip(9); return 9; }
    if (v == 0b1111111110)         { br.skip(10); return 10; }
    if (v == 0b1111111111)         { br.skip(10); return 11; }
    return 0;
}

inline int decode_dc_diff(BitReader& br, int size) {
    if (size <= 0) return 0;
    uint32_t v = br.get(size);
    if ((v & (1u << (size - 1))) == 0) {
        return static_cast<int>(v) - ((1 << size) - 1);
    }
    return static_cast<int>(v);
}

inline void mc_block(uint8_t* dst, int dst_stride, int dst_step_y,
                     const uint8_t* ref, int ref_stride, int ref_step_y,
                     int w, int h, int x, int y, int mv_x, int mv_y,
                     int ref_w, int ref_h, bool avg) {
    const int src_x = x + (mv_x >> 1);
    const int src_y = y + (mv_y >> 1);
    const int hx = mv_x & 1;
    const int hy = mv_y & 1;

    for (int py = 0; py < h; ++py) {
        const int ry0 = std::clamp(src_y + py * ref_step_y, 0, ref_h - 1);
        const int ry1 = std::clamp(src_y + (py + hy) * ref_step_y, 0, ref_h - 1);
        const uint8_t* r0 = ref + ry0 * ref_stride;
        const uint8_t* r1 = ref + ry1 * ref_stride;
        uint8_t* d = dst + (y + py * dst_step_y) * dst_stride + x;
        for (int px = 0; px < w; ++px) {
            const int rx0 = std::clamp(src_x + px, 0, ref_w - 1);
            const int rx1 = std::clamp(src_x + px + hx, 0, ref_w - 1);
            int val = 0;
            if (!hx && !hy) {
                val = r0[rx0];
            } else if (hx && !hy) {
                val = (r0[rx0] + r0[rx1] + 1) >> 1;
            } else if (!hx && hy) {
                val = (r0[rx0] + r1[rx0] + 1) >> 1;
            } else {
                val = (r0[rx0] + r0[rx1] + r1[rx0] + r1[rx1] + 2) >> 2;
            }
            if (avg) {
                d[px] = static_cast<uint8_t>((d[px] + val + 1) >> 1);
            } else {
                d[px] = static_cast<uint8_t>(val);
            }
        }
    }
}

} // namespace detail

struct Decoder {
    detail::VlcTables vlc;
    int seq_width = 0;
    int seq_height = 0;
    int mb_width = 0;
    int mb_height = 0;
    bool is_mpeg2 = true;

    uint8_t intra_qmat[64];
    uint8_t non_intra_qmat[64];
    uint8_t chroma_intra_qmat[64];
    uint8_t chroma_non_intra_qmat[64];

    int pic_type = 1; // 1=I, 2=P, 3=B
    int f_code[2][2] = {{1, 1}, {1, 1}};
    int intra_dc_precision = 0;
    int pic_struct = 3; // 3 = Frame picture
    int frame_pred_frame_dct = 1;
    int concealment_mv = 0;
    int q_scale_type = 0;
    int intra_vlc_format = 0;
    int alternate_scan = 0;

    FrameBuf ref[2];
    FrameBuf b_buf;
    int ref_count = 0;
    bool in_pic = false;
    bool seq_ended = false;
    std::vector<uint8_t> es_buf;

    Decoder() { reset(); }

    void reset() {
        seq_width = 0;
        seq_height = 0;
        mb_width = 0;
        mb_height = 0;
        is_mpeg2 = true;
        std::memcpy(intra_qmat, detail::kDefaultIntraMatrix, 64);
        std::memset(non_intra_qmat, 16, 64);
        std::memcpy(chroma_intra_qmat, detail::kDefaultIntraMatrix, 64);
        std::memset(chroma_non_intra_qmat, 16, 64);
        ref_count = 0;
        in_pic = false;
        seq_ended = false;
        es_buf.clear();
        es_buf.shrink_to_fit();
        ref[0] = FrameBuf{};
        ref[1] = FrameBuf{};
        b_buf = FrameBuf{};
    }

    void parse_sequence_header(const uint8_t* p, size_t len) {
        using namespace detail;
        BitReader br{p, len, 0};
        seq_width = static_cast<int>(br.get(12));
        seq_height = static_cast<int>(br.get(12));
        mb_width = (seq_width + 15) / 16;
        mb_height = (seq_height + 15) / 16;
        br.skip(4 + 4 + 18 + 1 + 10 + 1);
        if (br.get(1)) {
            for (int i = 0; i < 64; ++i) intra_qmat[kZigZag[i]] = static_cast<uint8_t>(br.get(8));
        } else {
            std::memcpy(intra_qmat, kDefaultIntraMatrix, 64);
        }
        if (br.get(1)) {
            for (int i = 0; i < 64; ++i) non_intra_qmat[kZigZag[i]] = static_cast<uint8_t>(br.get(8));
        } else {
            std::memset(non_intra_qmat, 16, 64);
        }
        std::memcpy(chroma_intra_qmat, intra_qmat, 64);
        std::memcpy(chroma_non_intra_qmat, non_intra_qmat, 64);
        const int mw = mb_width * 16;
        const int mh = mb_height * 16;
        ref[0].ensure_alloc(mw, mh, seq_width, seq_height);
        ref[1].ensure_alloc(mw, mh, seq_width, seq_height);
        b_buf.ensure_alloc(mw, mh, seq_width, seq_height);
    }

    void parse_extension(const uint8_t* p, size_t len) {
        using namespace detail;
        BitReader br{p, len, 0};
        int ext_id = static_cast<int>(br.get(4));
        if (ext_id == 1) {
            is_mpeg2 = true;
        } else if (ext_id == 3) {
            if (br.get(1)) {
                for (int i = 0; i < 64; ++i) intra_qmat[kZigZag[i]] = static_cast<uint8_t>(br.get(8));
                std::memcpy(chroma_intra_qmat, intra_qmat, 64);
            }
            if (br.get(1)) {
                for (int i = 0; i < 64; ++i) non_intra_qmat[kZigZag[i]] = static_cast<uint8_t>(br.get(8));
                std::memcpy(chroma_non_intra_qmat, non_intra_qmat, 64);
            }
            if (br.get(1)) {
                for (int i = 0; i < 64; ++i) chroma_intra_qmat[kZigZag[i]] = static_cast<uint8_t>(br.get(8));
            }
            if (br.get(1)) {
                for (int i = 0; i < 64; ++i) chroma_non_intra_qmat[kZigZag[i]] = static_cast<uint8_t>(br.get(8));
            }
        } else if (ext_id == 8) {
            f_code[0][0] = static_cast<int>(br.get(4));
            f_code[0][1] = static_cast<int>(br.get(4));
            f_code[1][0] = static_cast<int>(br.get(4));
            f_code[1][1] = static_cast<int>(br.get(4));
            intra_dc_precision = static_cast<int>(br.get(2));
            pic_struct = static_cast<int>(br.get(2));
            br.skip(1); // top_field_first
            frame_pred_frame_dct = static_cast<int>(br.get(1));
            concealment_mv = static_cast<int>(br.get(1));
            q_scale_type = static_cast<int>(br.get(1));
            intra_vlc_format = static_cast<int>(br.get(1));
            alternate_scan = static_cast<int>(br.get(1));
        }
    }

    void parse_pic_header(const uint8_t* p, size_t len) {
        using namespace detail;
        BitReader br{p, len, 0};
        br.skip(10); // temporal_reference
        pic_type = static_cast<int>(br.get(3));
        br.skip(16); // vbv_delay
        if (pic_type == 2 || pic_type == 3) {
            int fp_f = static_cast<int>(br.get(1));
            int f_f = static_cast<int>(br.get(3));
            if (!is_mpeg2) {
                f_code[0][0] = f_code[0][1] = f_f;
                (void)fp_f;
            }
        }
        if (pic_type == 3) {
            int fp_b = static_cast<int>(br.get(1));
            int f_b = static_cast<int>(br.get(3));
            if (!is_mpeg2) {
                f_code[1][0] = f_code[1][1] = f_b;
                (void)fp_b;
            }
        }
    }

    bool decode_block(detail::BitReader& br, int16_t block[64], bool intra, int comp,
                      int q_scale, int dc_pred[3]) {
        using namespace detail;
        std::memset(block, 0, sizeof(int16_t) * 64);
        const uint8_t* scan = alternate_scan ? kAltScan : kZigZag;
        const uint8_t* qmat = intra
            ? (comp == 0 ? intra_qmat : chroma_intra_qmat)
            : (comp == 0 ? non_intra_qmat : chroma_non_intra_qmat);
        const bool use_b15 = (intra && intra_vlc_format == 1);
        const CoeffVlc* short_lut = use_b15 ? vlc.b15_short : vlc.b14_short;
        const CoeffVlc* long_lut = use_b15 ? vlc.b15_long : vlc.b14_long;

        int idx = 0;
        int sum = 0;
        if (intra) {
            int dc_size = (comp == 0) ? decode_dc_size_luma(br) : decode_dc_size_chroma(br);
            int dc_diff = decode_dc_diff(br, dc_size);
            dc_pred[comp] += dc_diff;
            int dc_mult = 8 >> intra_dc_precision;
            block[0] = static_cast<int16_t>(dc_pred[comp] * dc_mult);
            sum ^= block[0];
            idx = 1;
        }

        for (;;) {
            if (br.eof()) return false;
            if (!intra && idx == 0) {
                uint32_t b2 = br.show(2);
                if (b2 >= 2) {
                    br.skip(2);
                    int sign = (b2 & 1);
                    int val = ((3 * q_scale * qmat[0]) >> 5);
                    if (val > 2047) val = 2047;
                    if (sign) val = -val;
                    block[0] = static_cast<int16_t>(val);
                    sum ^= block[0];
                    idx = 1;
                    continue;
                }
            }

            uint32_t peek10 = br.show(10);
            CoeffVlc e = short_lut[peek10];
            if (e.run == 0xFD) {
                uint32_t peek17 = br.show(17);
                if (peek17 < 4096) {
                    e = long_lut[peek17];
                }
            }
            if (e.run == 0xFD) {
                return false;
            }
            if (e.run == 0xFF) { // EOB
                br.skip(e.len);
                break;
            }
            int run = 0;
            int signed_level = 0;
            if (e.run == 0xFE) { // Escape: 6 bits + 6-bit run + 12-bit signed level
                br.skip(6);
                run = static_cast<int>(br.get(6));
                signed_level = br.get_signed(12);
            } else {
                br.skip(e.len - 1);
                int sign = static_cast<int>(br.get(1));
                run = e.run;
                signed_level = sign ? -static_cast<int>(e.level) : static_cast<int>(e.level);
            }

            idx += run;
            if (idx >= 64) {
                return false;
            }
            int pos = scan[idx];
            int abs_lvl = signed_level < 0 ? -signed_level : signed_level;
            int k = intra ? 0 : 1;
            int val = ((2 * abs_lvl + k) * qmat[pos] * q_scale) >> 4;
            if (signed_level < 0) val = -val;
            if (val > 2047) val = 2047;
            else if (val < -2048) val = -2048;
            sum ^= (val - block[pos]);
            block[pos] = static_cast<int16_t>(val);
            ++idx;
        }

        if ((sum & 1) == 0) {
            block[63] ^= 1;
        }
        idct8x8(block);
        return true;
    }

    void apply_mc(FrameBuf& dst, int mb_x, int mb_y, uint8_t mb_flags, int motion_type,
                  const int mv[2][2][2], const int mv_field_sel[2][2]) {
        using namespace detail;
        bool has_fwd = (mb_flags & MB_FORWARD) != 0;
        bool has_back = (mb_flags & MB_BACK) != 0;
        bool avg = false;

        auto do_dir = [&](int s, const FrameBuf& rbuf) {
            const int rw = rbuf.w;
            const int rh = rbuf.h;
            const int x = mb_x * 16;
            const int y = mb_y * 16;
            const int cx = mb_x * 8;
            const int cy = mb_y * 8;

            if (motion_type == 2) { // Frame-based MC
                mc_block(dst.y.data(), rw, 1, rbuf.y.data(), rw, 1,
                         16, 16, x, y, mv[0][s][0], mv[0][s][1], rw, rh, avg);
                mc_block(dst.cb.data(), rw / 2, 1, rbuf.cb.data(), rw / 2, 1,
                         8, 8, cx, cy, mv[0][s][0] / 2, mv[0][s][1] / 2, rw / 2, rh / 2, avg);
                mc_block(dst.cr.data(), rw / 2, 1, rbuf.cr.data(), rw / 2, 1,
                         8, 8, cx, cy, mv[0][s][0] / 2, mv[0][s][1] / 2, rw / 2, rh / 2, avg);
            } else if (motion_type == 1) { // Field-based MC in Frame picture
                for (int fld = 0; fld < 2; ++fld) {
                    int r_fld = mv_field_sel[fld][s];
                    int my = mv[fld][s][1] + (r_fld - fld) * 2;
                    int c_my = (mv[fld][s][1] / 2) + (r_fld - fld) * 2;
                    mc_block(dst.y.data(), rw, 2, rbuf.y.data(), rw, 2,
                             16, 8, x, y + fld, mv[fld][s][0], my, rw, rh, avg);
                    mc_block(dst.cb.data(), rw / 2, 2, rbuf.cb.data(), rw / 2, 2,
                             8, 4, cx, cy + fld, mv[fld][s][0] / 2, c_my, rw / 2, rh / 2, avg);
                    mc_block(dst.cr.data(), rw / 2, 2, rbuf.cr.data(), rw / 2, 2,
                             8, 4, cx, cy + fld, mv[fld][s][0] / 2, c_my, rw / 2, rh / 2, avg);
                }
            }
        };

        if (has_fwd) {
            const FrameBuf& fref = (pic_type == 3) ? ref[0] : ref[1];
            do_dir(0, fref);
            avg = true;
        }
        if (has_back) {
            do_dir(1, ref[1]);
        }
    }

    void decode_slice(int slice_row, const uint8_t* p, size_t len) {
        using namespace detail;
        if (slice_row < 0 || slice_row >= mb_height) return;
        BitReader br{p, len, 0};
        int q_code = static_cast<int>(br.get(5));
        int q_scale = q_scale_type ? kQScaleNonLinear[q_code] : kQScaleLinear[q_code];
        if (br.get(1) == 1) {
            while (!br.eof() && br.get(1) == 1) {
                br.skip(8);
            }
        }

        FrameBuf& dst = (pic_type == 3) ? b_buf : ref[1];
        int mb_x = -1;
        int dc_pred[3] = {
            128 << intra_dc_precision,
            128 << intra_dc_precision,
            128 << intra_dc_precision
        };
        int pmv[2][2][2] = {};
        uint8_t last_mb_flags = 0;
        int last_motion_type = 2;
        int last_mv_field_sel[2][2] = {};

        while (!br.eof()) {
            if (br.show(23) == 0) break;
            int inc = decode_mb_address_increment(br);
            if (inc <= 0) break;

            if (mb_x < 0) {
                for (int sk = 0; sk < inc - 1 && sk < mb_width; ++sk) {
                    if (pic_type == 2) {
                        int zero_mv[2][2][2] = {};
                        int zero_fs[2][2] = {};
                        apply_mc(dst, sk, slice_row, MB_FORWARD, 2, zero_mv, zero_fs);
                    }
                }
                mb_x = inc - 1;
            } else {
                if (inc > 1) {
                    dc_pred[0] = dc_pred[1] = dc_pred[2] = 128 << intra_dc_precision;
                    if (pic_type == 2) {
                        std::memset(pmv, 0, sizeof(pmv));
                    }
                }
                for (int sk = 1; sk < inc; ++sk) {
                    ++mb_x;
                    if (mb_x >= mb_width) break;
                    if (pic_type == 2) {
                        int zero_mv[2][2][2] = {};
                        int zero_fs[2][2] = {};
                        apply_mc(dst, mb_x, slice_row, MB_FORWARD, 2, zero_mv, zero_fs);
                    } else if (pic_type == 3) {
                        apply_mc(dst, mb_x, slice_row,
                                 last_mb_flags & (MB_FORWARD | MB_BACK),
                                 last_motion_type, pmv, last_mv_field_sel);
                    }
                }
                ++mb_x;
            }
            if (mb_x >= mb_width) break;

            uint8_t mb_flags = decode_mb_type(br, pic_type);
            if (mb_flags == 0) break;

            int motion_type = 2; // Frame-based default
            if ((mb_flags & (MB_FORWARD | MB_BACK)) != 0) {
                if (pic_struct == 3 && frame_pred_frame_dct == 0) {
                    motion_type = static_cast<int>(br.get(2));
                }
            }

            int dct_type = 0;
            if (pic_struct == 3 && frame_pred_frame_dct == 0 &&
                ((mb_flags & (MB_INTRA | MB_PATTERN)) != 0)) {
                dct_type = static_cast<int>(br.get(1));
            }

            if (mb_flags & MB_QUANT) {
                q_code = static_cast<int>(br.get(5));
                q_scale = q_scale_type ? kQScaleNonLinear[q_code] : kQScaleLinear[q_code];
            }

            int mv[2][2][2] = {};
            int mv_field_sel[2][2] = {};
            int mv_count = (motion_type == 1) ? 2 : 1;
            int mv_format = (motion_type == 1) ? 0 : 1; // 0=Field, 1=Frame

            auto read_mvs = [&](int s) {
                if (mv_count == 1) {
                    if (mv_format == 0) {
                        mv_field_sel[0][s] = static_cast<int>(br.get(1));
                    }
                    for (int t = 0; t < 2; ++t) {
                        int r_size = f_code[s][t] - 1;
                        int pred = pmv[0][s][t];
                        if (mv_format == 0 && t == 1 && pic_struct == 3) pred >>= 1;
                        int v = decode_motion_vector(br, r_size, pred);
                        if (mv_format == 0 && t == 1 && pic_struct == 3) {
                            pmv[0][s][t] = v * 2;
                        } else {
                            pmv[0][s][t] = v;
                        }
                        pmv[1][s][t] = pmv[0][s][t];
                        mv[0][s][t] = v;
                    }
                    if (motion_type == 3) { // Dual-prime
                        br.skip(1);
                        if (br.get(1)) br.skip(1);
                    }
                } else {
                    for (int r = 0; r < 2; ++r) {
                        mv_field_sel[r][s] = static_cast<int>(br.get(1));
                        for (int t = 0; t < 2; ++t) {
                            int r_size = f_code[s][t] - 1;
                            int pred = pmv[r][s][t];
                            if (t == 1 && pic_struct == 3) pred >>= 1;
                            int v = decode_motion_vector(br, r_size, pred);
                            if (t == 1 && pic_struct == 3) {
                                pmv[r][s][t] = v * 2;
                            } else {
                                pmv[r][s][t] = v;
                            }
                            mv[r][s][t] = v;
                        }
                    }
                }
            };

            if ((mb_flags & MB_FORWARD) || ((mb_flags & MB_INTRA) && concealment_mv)) {
                read_mvs(0);
            }
            if (mb_flags & MB_BACK) {
                read_mvs(1);
            }
            if ((mb_flags & MB_INTRA) && concealment_mv) {
                br.skip(1); // marker_bit
            }

            if (!(mb_flags & MB_INTRA)) {
                dc_pred[0] = dc_pred[1] = dc_pred[2] = 128 << intra_dc_precision;
                if (pic_type == 2 && !(mb_flags & MB_FORWARD)) {
                    std::memset(pmv, 0, sizeof(pmv));
                    std::memset(mv, 0, sizeof(mv));
                    apply_mc(dst, mb_x, slice_row, MB_FORWARD, 2, mv, mv_field_sel);
                } else {
                    apply_mc(dst, mb_x, slice_row, mb_flags, motion_type, mv, mv_field_sel);
                }
            } else if (!concealment_mv) {
                std::memset(pmv, 0, sizeof(pmv));
            }

            last_mb_flags = mb_flags;
            last_motion_type = motion_type;
            std::memcpy(last_mv_field_sel, mv_field_sel, sizeof(mv_field_sel));

            int cbp = (mb_flags & MB_INTRA) ? 0x3F : 0;
            if (mb_flags & MB_PATTERN) {
                cbp = decode_cbp(br);
                if (cbp < 0) break;
            }

            const bool intra = (mb_flags & MB_INTRA) != 0;
            const int stride = dst.w;
            for (int b = 0; b < 6; ++b) {
                if ((cbp & (1 << (5 - b))) == 0) continue;
                int16_t blk[64];
                int comp = (b < 4) ? 0 : (b - 3);
                if (!decode_block(br, blk, intra, comp, q_scale, dc_pred)) {
                    return;
                }
                if (b < 4) {
                    int bx = mb_x * 16 + (b & 1) * 8;
                    int by = slice_row * 16;
                    int step_y = 1;
                    if (dct_type == 1) {
                        by += (b >> 1);
                        step_y = 2;
                    } else {
                        by += (b >> 1) * 8;
                    }
                    for (int py = 0; py < 8; ++py) {
                        uint8_t* row = dst.y.data() + (by + py * step_y) * stride + bx;
                        for (int px = 0; px < 8; ++px) {
                            int v = intra ? blk[py * 8 + px] : (row[px] + blk[py * 8 + px]);
                            row[px] = clamp_u8(v);
                        }
                    }
                } else {
                    uint8_t* plane = (b == 4) ? dst.cb.data() : dst.cr.data();
                    int cstride = stride / 2;
                    int bx = mb_x * 8;
                    int by = slice_row * 8;
                    for (int py = 0; py < 8; ++py) {
                        uint8_t* row = plane + (by + py) * cstride + bx;
                        for (int px = 0; px < 8; ++px) {
                            int v = intra ? blk[py * 8 + px] : (row[px] + blk[py * 8 + px]);
                            row[px] = clamp_u8(v);
                        }
                    }
                }
            }
        }
    }

    template <typename OnFrame>
    void finish_picture(OnFrame&& on_frame) {
        if (!in_pic) return;
        in_pic = false;
        if (pic_type == 3) {
            on_frame(b_buf);
        } else {
            ++ref_count;
            if (ref_count >= 2) {
                on_frame(ref[0]);
            }
        }
    }

    template <typename OnFrame>
    void process_unit(uint8_t code, const uint8_t* payload, size_t payload_len, OnFrame&& on_frame) {
        if (code == 0xB3 || code == 0xB8 || code == 0x00 || code == 0xB7) {
            finish_picture(on_frame);
        }
        if (code == 0xB3) {
            parse_sequence_header(payload, payload_len);
        } else if (code == 0xB5) {
            parse_extension(payload, payload_len);
        } else if (code == 0x00) {
            parse_pic_header(payload, payload_len);
            if (pic_type == 1 || pic_type == 2) {
                std::swap(ref[0], ref[1]);
            }
            in_pic = true;
        } else if (code >= 0x01 && code <= 0xAF) {
            if (in_pic && mb_width > 0 && mb_height > 0) {
                decode_slice(static_cast<int>(code) - 1, payload, payload_len);
            }
        }
    }

    template <typename OnFrame>
    void push(const uint8_t* data, size_t len, OnFrame&& on_frame) {
        if (!data || len == 0) return;
        es_buf.insert(es_buf.end(), data, data + len);

        size_t i = 0;
        const size_t n = es_buf.size();
        // Find first start code
        while (i + 3 < n && !(es_buf[i] == 0 && es_buf[i + 1] == 0 && es_buf[i + 2] == 1)) {
            ++i;
        }
        if (i + 3 >= n) {
            if (n > 3) {
                es_buf.erase(es_buf.begin(), es_buf.begin() + static_cast<std::ptrdiff_t>(n - 3));
            }
            return;
        }

        size_t last_sc = i;
        while (i + 3 < n) {
            uint8_t code = es_buf[i + 3];
            size_t next = i + 4;
            while (next + 2 < n && !(es_buf[next] == 0 && es_buf[next + 1] == 0 && es_buf[next + 2] == 1)) {
                ++next;
            }
            if (next + 2 >= n) {
                // This unit is not yet terminated by a following start code
                break;
            }
            process_unit(code, es_buf.data() + i + 4, next - (i + 4), on_frame);
            i = next;
            last_sc = next;
        }

        if (last_sc > 0) {
            es_buf.erase(es_buf.begin(), es_buf.begin() + static_cast<std::ptrdiff_t>(last_sc));
        }
    }

    template <typename OnFrame>
    void flush(OnFrame&& on_frame) {
        const size_t n = es_buf.size();
        if (n >= 4 && es_buf[0] == 0 && es_buf[1] == 0 && es_buf[2] == 1) {
            process_unit(es_buf[3], es_buf.data() + 4, n - 4, on_frame);
        }
        es_buf.clear();
        finish_picture(on_frame);
        if (ref_count >= 1) {
            on_frame(ref[1]);
            ref_count = 0;
        }
    }

    // Step through `es[pos..vlen)` until one display-order picture is ready in `rgba`.
    // Returns: 1 = picture ready, 0 = need more bytes (when !vdone), -1 = EOF.
    int next_picture(const uint8_t* es, size_t vlen, bool vdone, size_t& pos,
                     std::vector<uint8_t>& rgba, int& out_w, int& out_h) {
        if (seq_ended) {
            if (ref_count >= 1) {
                ref_count = 0;
                ref[1].to_rgba(rgba, out_w, out_h);
                return 1;
            }
            return -1;
        }
        for (;;) {
            while (pos + 3 < vlen && !(es[pos] == 0 && es[pos + 1] == 0 && es[pos + 2] == 1)) {
                ++pos;
            }
            if (pos + 3 >= vlen) {
                if (!vdone) return 0;
                if (in_pic) {
                    in_pic = false;
                    if (pic_type == 3) {
                        b_buf.to_rgba(rgba, out_w, out_h);
                        return 1;
                    }
                    ++ref_count;
                    if (ref_count >= 2) {
                        ref[0].to_rgba(rgba, out_w, out_h);
                        return 1;
                    }
                }
                if (ref_count >= 1) {
                    ref_count = 0;
                    ref[1].to_rgba(rgba, out_w, out_h);
                    return 1;
                }
                return -1;
            }

            const uint8_t code = es[pos + 3];
            if (in_pic && (code == 0xB3 || code == 0xB8 || code == 0x00 || code == 0xB7)) {
                in_pic = false;
                if (pic_type == 3) {
                    b_buf.to_rgba(rgba, out_w, out_h);
                    return 1;
                }
                ++ref_count;
                if (ref_count >= 2) {
                    ref[0].to_rgba(rgba, out_w, out_h);
                    return 1;
                }
            }

            if (code == 0xB7) {
                pos += 4;
                seq_ended = true;
                if (ref_count >= 1) {
                    ref_count = 0;
                    ref[1].to_rgba(rgba, out_w, out_h);
                    return 1;
                }
                return -1;
            }

            size_t next = pos + 4;
            while (next + 2 < vlen && !(es[next] == 0 && es[next + 1] == 0 && es[next + 2] == 1)) {
                ++next;
            }
            if (next + 2 >= vlen) {
                if (!vdone) return 0;
                next = vlen;
            }

            const uint8_t* payload = es + pos + 4;
            const size_t payload_len = next - (pos + 4);
            if (code == 0xB3) {
                parse_sequence_header(payload, payload_len);
            } else if (code == 0xB5) {
                parse_extension(payload, payload_len);
            } else if (code == 0x00) {
                parse_pic_header(payload, payload_len);
                if (pic_type == 1 || pic_type == 2) {
                    std::swap(ref[0], ref[1]);
                }
                in_pic = true;
            } else if (code >= 0x01 && code <= 0xAF) {
                if (in_pic && mb_width > 0 && mb_height > 0) {
                    decode_slice(static_cast<int>(code) - 1, payload, payload_len);
                }
            }
            pos = next;
        }
    }
};

} // namespace rac1::mpeg2
