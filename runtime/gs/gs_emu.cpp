// See gs_emu.h. The local memory layouts (page / block / column tables) are the GS's, as in the
// GS User's Manual ch. 8 (the tables as OpenGOAL's common/texture/texture_conversion.h has them:
// Copyright (c) the OpenGOAL team, ISC license, third_party/989snd/LICENSE).
#include <runtime/vu/vu1.h>
#include <runtime/gs/gs_emu.h>

#include <runtime/kernel/ee_memory.h>
#include <profiler/host_profiler.h>

#include <algorithm>
#include <condition_variable>
#include <exception>
#include <iterator>
#include <mutex>
#include <thread>
#include <cmath>
#include <cstring>
#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace rac1::gs {

namespace {
constexpr uint32_t VRAM = 4 * 1024 * 1024;

const uint8_t BLOCK32[4][8] = {{0, 1, 4, 5, 16, 17, 20, 21}, {2, 3, 6, 7, 18, 19, 22, 23}, {8, 9, 12, 13, 24, 25, 28, 29}, {10, 11, 14, 15, 26, 27, 30, 31}};
const uint8_t COL32[2][8] = {{0, 1, 4, 5, 8, 9, 12, 13}, {2, 3, 6, 7, 10, 11, 14, 15}};
const uint8_t BLOCK16[8][4] = {{0, 2, 8, 10}, {1, 3, 9, 11}, {4, 6, 12, 14}, {5, 7, 13, 15}, {16, 18, 24, 26}, {17, 19, 25, 27}, {20, 22, 28, 30}, {21, 23, 29, 31}};
const uint8_t BLOCK16S[8][4] = {{0, 2, 16, 18}, {1, 3, 17, 19}, {8, 10, 24, 26}, {9, 11, 25, 27}, {4, 6, 20, 22}, {5, 7, 21, 23}, {12, 14, 28, 30}, {13, 15, 29, 31}};
const uint8_t PIX16[8][16] = {
    {0, 2, 8, 10, 16, 18, 24, 26, 1, 3, 9, 11, 17, 19, 25, 27},         {4, 6, 12, 14, 20, 22, 28, 30, 5, 7, 13, 15, 21, 23, 29, 31},
    {32, 34, 40, 42, 48, 50, 56, 58, 33, 35, 41, 43, 49, 51, 57, 59},   {36, 38, 44, 46, 52, 54, 60, 62, 37, 39, 45, 47, 53, 55, 61, 63},
    {64, 66, 72, 74, 80, 82, 88, 90, 65, 67, 73, 75, 81, 83, 89, 91},   {68, 70, 76, 78, 84, 86, 92, 94, 69, 71, 77, 79, 85, 87, 93, 95},
    {96, 98, 104, 106, 112, 114, 120, 122, 97, 99, 105, 107, 113, 115, 121, 123},
    {100, 102, 108, 110, 116, 118, 124, 126, 101, 103, 109, 111, 117, 119, 125, 127}};
const uint8_t PIX8[16][16] = {
    {0, 4, 16, 20, 32, 36, 48, 52, 2, 6, 18, 22, 34, 38, 50, 54},
    {8, 12, 24, 28, 40, 44, 56, 60, 10, 14, 26, 30, 42, 46, 58, 62},
    {33, 37, 49, 53, 1, 5, 17, 21, 35, 39, 51, 55, 3, 7, 19, 23},
    {41, 45, 57, 61, 9, 13, 25, 29, 43, 47, 59, 63, 11, 15, 27, 31},
    {96, 100, 112, 116, 64, 68, 80, 84, 98, 102, 114, 118, 66, 70, 82, 86},
    {104, 108, 120, 124, 72, 76, 88, 92, 106, 110, 122, 126, 74, 78, 90, 94},
    {65, 69, 81, 85, 97, 101, 113, 117, 67, 71, 83, 87, 99, 103, 115, 119},
    {73, 77, 89, 93, 105, 109, 121, 125, 75, 79, 91, 95, 107, 111, 123, 127},
    {128, 132, 144, 148, 160, 164, 176, 180, 130, 134, 146, 150, 162, 166, 178, 182},
    {136, 140, 152, 156, 168, 172, 184, 188, 138, 142, 154, 158, 170, 174, 186, 190},
    {161, 165, 177, 181, 129, 133, 145, 149, 163, 167, 179, 183, 131, 135, 147, 151},
    {169, 173, 185, 189, 137, 141, 153, 157, 171, 175, 187, 191, 139, 143, 155, 159},
    {224, 228, 240, 244, 192, 196, 208, 212, 226, 230, 242, 246, 194, 198, 210, 214},
    {232, 236, 248, 252, 200, 204, 216, 220, 234, 238, 250, 254, 202, 206, 218, 222},
    {193, 197, 209, 213, 225, 229, 241, 245, 195, 199, 211, 215, 227, 231, 243, 247},
    {201, 205, 217, 221, 233, 237, 249, 253, 203, 207, 219, 223, 235, 239, 251, 255}};
const uint16_t PIX4[16][32] = {
    {0, 8, 32, 40, 64, 72, 96, 104, 2, 10, 34, 42, 66, 74, 98, 106, 4, 12, 36, 44, 68, 76, 100, 108, 6, 14, 38, 46, 70, 78, 102, 110},
    {16, 24, 48, 56, 80, 88, 112, 120, 18, 26, 50, 58, 82, 90, 114, 122, 20, 28, 52, 60, 84, 92, 116, 124, 22, 30, 54, 62, 86, 94, 118, 126},
    {65, 73, 97, 105, 1, 9, 33, 41, 67, 75, 99, 107, 3, 11, 35, 43, 69, 77, 101, 109, 5, 13, 37, 45, 71, 79, 103, 111, 7, 15, 39, 47},
    {81, 89, 113, 121, 17, 25, 49, 57, 83, 91, 115, 123, 19, 27, 51, 59, 85, 93, 117, 125, 21, 29, 53, 61, 87, 95, 119, 127, 23, 31, 55, 63},
    {192, 200, 224, 232, 128, 136, 160, 168, 194, 202, 226, 234, 130, 138, 162, 170, 196, 204, 228, 236, 132, 140, 164, 172, 198, 206, 230, 238, 134, 142, 166, 174},
    {208, 216, 240, 248, 144, 152, 176, 184, 210, 218, 242, 250, 146, 154, 178, 186, 212, 220, 244, 252, 148, 156, 180, 188, 214, 222, 246, 254, 150, 158, 182, 190},
    {129, 137, 161, 169, 193, 201, 225, 233, 131, 139, 163, 171, 195, 203, 227, 235, 133, 141, 165, 173, 197, 205, 229, 237, 135, 143, 167, 175, 199, 207, 231, 239},
    {145, 153, 177, 185, 209, 217, 241, 249, 147, 155, 179, 187, 211, 219, 243, 251, 149, 157, 181, 189, 213, 221, 245, 253, 151, 159, 183, 191, 215, 223, 247, 255},
    {256, 264, 288, 296, 320, 328, 352, 360, 258, 266, 290, 298, 322, 330, 354, 362, 260, 268, 292, 300, 324, 332, 356, 364, 262, 270, 294, 302, 326, 334, 358, 366},
    {272, 280, 304, 312, 336, 344, 368, 376, 274, 282, 306, 314, 338, 346, 370, 378, 276, 284, 308, 316, 340, 348, 372, 380, 278, 286, 310, 318, 342, 350, 374, 382},
    {321, 329, 353, 361, 257, 265, 289, 297, 323, 331, 355, 363, 259, 267, 291, 299, 325, 333, 357, 365, 261, 269, 293, 301, 327, 335, 359, 367, 263, 271, 295, 303},
    {337, 345, 369, 377, 273, 281, 305, 313, 339, 347, 371, 379, 275, 283, 307, 315, 341, 349, 373, 381, 277, 285, 309, 317, 343, 351, 375, 383, 279, 287, 311, 319},
    {448, 456, 480, 488, 384, 392, 416, 424, 450, 458, 482, 490, 386, 394, 418, 426, 452, 460, 484, 492, 388, 396, 420, 428, 454, 462, 486, 494, 390, 398, 422, 430},
    {464, 472, 496, 504, 400, 408, 432, 440, 466, 474, 498, 506, 402, 410, 434, 442, 468, 476, 500, 508, 404, 412, 436, 444, 470, 478, 502, 510, 406, 414, 438, 446},
    {385, 393, 417, 425, 449, 457, 481, 489, 387, 395, 419, 427, 451, 459, 483, 491, 389, 397, 421, 429, 453, 461, 485, 493, 391, 399, 423, 431, 455, 463, 487, 495},
    {401, 409, 433, 441, 465, 473, 497, 505, 403, 411, 435, 443, 467, 475, 499, 507, 405, 413, 437, 445, 469, 477, 501, 509, 407, 415, 439, 447, 471, 479, 503, 511}};

// (EE memory and the VUs' are host-native 32-bit words, ee_memory.h: a doubleword's low word first, the PS2's byte i of
// a word-aligned block at i ^ EE_BYTE_SWIZZLE, its halfword at i ^ EE_HALF_SWIZZLE; the same on a little-endian host)
uint64_t r64(const uint8_t* p) {
    uint32_t w[2];
    std::memcpy(w, p, 8);
    return (static_cast<uint64_t>(w[1]) << 32) | w[0];
}
uint8_t byte_at(const uint8_t* p, uint32_t i) { return p[i ^ EE_BYTE_SWIZZLE]; }
// (the GS's local memory, vram_, holds the PS2's bytes - its 16-bit, 8-bit and 4-bit pixels are read and written as bytes -:
// a 32-bit pixel is its four bytes little-endian, whatever the host's order)
uint32_t vram_r32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }
void vram_w32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
}
uint16_t half_at(const uint8_t* p, uint32_t i) {
    uint16_t v;
    std::memcpy(&v, p + (i ^ EE_HALF_SWIZZLE), 2);
    return v;
}
uint32_t r32(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}
float f32(uint32_t v) {
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}
uint32_t bits(uint64_t v, int lo, int n) { return static_cast<uint32_t>((v >> lo) & ((1ull << n) - 1)); }

enum Psm : uint32_t { CT32 = 0, CT24 = 1, CT16 = 2, CT16S = 0xa, T8 = 0x13, T4 = 0x14, T8H = 0x1b, T4HL = 0x24, T4HH = 0x2c };
}  // namespace

Gs::Gs() : vram_(VRAM) {}

Gs& gs_global() {
    static Gs gs;
    return gs;
}

uint32_t Gs::addr32(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y) {
    static const std::array<uint32_t, 64 * 32> in_page = [] {
        std::array<uint32_t, 64 * 32> t{};
        for (uint32_t py = 0; py < 32; ++py) {
            for (uint32_t px = 0; px < 64; ++px) {
                const uint32_t block = BLOCK32[py / 8][px / 8];
                const uint32_t bx = px % 8, by = py % 8;
                const uint32_t pix = (by / 2) * 16 + COL32[by % 2][bx];
                t[py * 64 + px] = (block * 64 + pix) * 4;
            }
        }
        return t;
    }();
    const uint32_t ppr = std::max(1u, bw);
    const uint32_t page = (x >> 6) + (y >> 5) * ppr;
    return (bp * 256 + page * 8192 + in_page[((y & 31) << 6) | (x & 63)]) & (VRAM - 1);
}
uint32_t Gs::addr16(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, bool s) {
    const uint32_t ppr = std::max(1u, bw);
    const uint32_t page = x / 64 + (y / 64) * ppr;
    const uint32_t px = x % 64, py = y % 64;
    const uint32_t block = s ? BLOCK16S[py / 8][px / 16] : BLOCK16[py / 8][px / 16];
    const uint32_t pix = PIX16[py % 8][px % 16];
    return ((bp * 128 + page * 4096 + block * 128 + pix) * 2) & (VRAM - 1);
}
uint32_t Gs::addr8(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y) {
    const uint32_t ppr = std::max(1u, bw / 2);
    const uint32_t page = x / 128 + (y / 64) * ppr;
    const uint32_t px = x % 128, py = y % 64;
    const uint32_t block = BLOCK32[py / 16][px / 16];
    const uint32_t pix = PIX8[py % 16][px % 16];
    return (bp * 256 + page * 8192 + block * 256 + pix) & (VRAM - 1);
}
uint32_t Gs::addr4(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y) {
    const uint32_t ppr = std::max(1u, bw / 2);
    const uint32_t page = x / 128 + (y / 128) * ppr;
    const uint32_t px = x % 128, py = y % 128;
    const uint32_t block = BLOCK16[py / 16][px / 32];
    const uint32_t pix = PIX4[py % 16][px % 32];
    return (bp * 512 + page * 16384 + block * 512 + pix) & (VRAM * 2 - 1);
}

uint32_t Gs::read_pixel(uint32_t psm, uint32_t bp, uint32_t bw, uint32_t x, uint32_t y) const {
    // (the bytes read, for the texture decodes' validity: whole words at most)
    auto note = [this](uint32_t a, uint32_t n) {
        if (rd_track_) {
            rd_lo_[rd_slot_] = std::min(rd_lo_[rd_slot_], a);
            rd_hi_[rd_slot_] = std::max(rd_hi_[rd_slot_], a + n);
        }
    };
    switch (psm) {
    case CT32: { const uint32_t a = addr32(bp, bw, x, y); note(a, 4); return vram_r32(&vram_[a]); }
    case CT24: { const uint32_t a = addr32(bp, bw, x, y); note(a, 4); return vram_r32(&vram_[a]) & 0xffffff; }
    case CT16:
    case CT16S: {
        const uint32_t a = addr16(bp, bw, x, y, psm == CT16S);
        note(a, 2);
        return vram_[a] | (vram_[a + 1] << 8);
    }
    case T8: { const uint32_t a = addr8(bp, bw, x, y); note(a, 1); return vram_[a]; }
    case T4: {
        const uint32_t a = addr4(bp, bw, x, y);
        note(a >> 1, 1);
        return (vram_[a >> 1] >> ((a & 1) * 4)) & 0xf;
    }
    case T8H: { const uint32_t a = addr32(bp, bw, x, y) + 3; note(a, 1); return vram_[a]; }
    case T4HL: { const uint32_t a = addr32(bp, bw, x, y) + 3; note(a, 1); return vram_[a] & 0xf; }
    case T4HH: { const uint32_t a = addr32(bp, bw, x, y) + 3; note(a, 1); return vram_[a] >> 4; }
    default: { const uint32_t a = addr32(bp, bw, x, y); note(a, 4); return vram_r32(&vram_[a]); }
    }
}

// A write that changed bytes [a, a + n) of the local memory: logged (a run of changes in a transfer is one entry).
void Gs::note_change(uint32_t a, uint32_t n) {
    if (!wr_open_ || wr_log_.empty()) {
        if (wr_log_.size() >= 8192) {  // (the oldest half forgotten: the snapshots before it are decoded again)
            wr_floor_ = wr_log_[wr_log_.size() / 2 - 1].seq;
            wr_log_.erase(wr_log_.begin(), wr_log_.begin() + static_cast<long>(wr_log_.size() / 2));
        }
        wr_log_.push_back({++wr_seq_, a, a + n});
        wr_open_ = true;
        ++gen_;
        return;
    }
    WrRange& r = wr_log_.back();
    r.lo = std::min(r.lo, a);
    r.hi = std::max(r.hi, a + n);
}

// true when a change since the snapshot touched the memory it read
bool Gs::snap_stale(const TexSnap& s) const {
    if (s.seq < wr_floor_) return true;
    for (size_t i = wr_log_.size(); i-- > 0;) {
        const WrRange& r = wr_log_[i];
        if (r.seq <= s.seq) break;
        for (int k = 0; k < 2; ++k)
            if (r.lo < s.rd_hi[k] && r.hi > s.rd_lo[k]) return true;
    }
    return false;
}

void Gs::write_pixel(uint32_t psm, uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t v) {
    // the bytes a write of this format touches (a, n), compared before and after: a write of what is
    // there is no change (the snapshots of textures stay valid)
    if (psm == CT32) {
        const uint32_t a0 = addr32(bp, bw, x, y);
        if (vram_r32(&vram_[a0]) != v) {
            uint32_t raw;  // (the bytes as they were, for the undo's put-back)
            std::memcpy(&raw, &vram_[a0], 4);
            vram_w32(&vram_[a0], v);
            note_change(a0, 4);
            if (vundo_on_) vundo_.push_back({a0, 4, raw});
        }
        return;
    }
    uint32_t a0 = 0, n0 = 0;
    switch (psm) {
    case CT24: a0 = addr32(bp, bw, x, y); n0 = 3; break;
    case CT16: case CT16S: a0 = addr16(bp, bw, x, y, psm == CT16S); n0 = 2; break;
    case T8: a0 = addr8(bp, bw, x, y); n0 = 1; break;
    case T4: a0 = addr4(bp, bw, x, y) >> 1; n0 = 1; break;
    case T8H: case T4HL: case T4HH: a0 = addr32(bp, bw, x, y) + 3; n0 = 1; break;
    default: break;
    }
    uint8_t before[4] = {0, 0, 0, 0};
    if (n0 != 0) std::memcpy(before, &vram_[a0], n0);
    write_pixel_raw(psm, bp, bw, x, y, v);
    if (n0 != 0 && std::memcmp(before, &vram_[a0], n0) != 0) {
        note_change(a0, n0);
        if (vundo_on_) {  // (the substitute's uploads, put back before the real range)
            uint32_t w = 0;
            std::memcpy(&w, before, 4);
            vundo_.push_back({a0, n0, w});
        }
    }
}

void Gs::write_ct32_rect(uint32_t bp, uint32_t bw, uint32_t w, uint32_t h, const uint32_t* px) {
    uint32_t lo = 0xffffffffu, hi = 0;
    for (uint32_t y = 0; y < h; ++y) {
        const uint32_t* row = px + static_cast<size_t>(y) * w;
        for (uint32_t x = 0; x < w; ++x) {
            const uint32_t a = addr32(bp, bw, x, y);
            if (vram_r32(&vram_[a]) != row[x]) {
                vram_w32(&vram_[a], row[x]);
                if (a < lo) lo = a;
                if (a + 4 > hi) hi = a + 4;
            }
        }
    }
    if (hi > lo) {
        wr_open_ = false;
        note_change(lo, hi - lo);
        wr_open_ = false;
    }
}

void Gs::write_pixel_raw(uint32_t psm, uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t v) {
    switch (psm) {
    case CT32: {
        const uint32_t a = addr32(bp, bw, x, y);
        vram_w32(&vram_[a], v);
        break;
    }
    case CT24: {
        const uint32_t a = addr32(bp, bw, x, y);
        vram_[a] = v & 0xff;
        vram_[a + 1] = (v >> 8) & 0xff;
        vram_[a + 2] = (v >> 16) & 0xff;
        break;
    }
    case CT16:
    case CT16S: {
        const uint32_t a = addr16(bp, bw, x, y, psm == CT16S);
        vram_[a] = v & 0xff;
        vram_[a + 1] = (v >> 8) & 0xff;
        break;
    }
    case T8: vram_[addr8(bp, bw, x, y)] = v & 0xff; break;
    case T4: {
        const uint32_t a = addr4(bp, bw, x, y);
        uint8_t& b = vram_[a >> 1];
        b = (a & 1) ? static_cast<uint8_t>((b & 0x0f) | ((v & 0xf) << 4)) : static_cast<uint8_t>((b & 0xf0) | (v & 0xf));
        break;
    }
    case T8H: vram_[addr32(bp, bw, x, y) + 3] = v & 0xff; break;
    case T4HL: {
        uint8_t& b = vram_[addr32(bp, bw, x, y) + 3];
        b = static_cast<uint8_t>((b & 0xf0) | (v & 0xf));
        break;
    }
    case T4HH: {
        uint8_t& b = vram_[addr32(bp, bw, x, y) + 3];
        b = static_cast<uint8_t>((b & 0x0f) | ((v & 0xf) << 4));
        break;
    }
    default: break;
    }
}

uint32_t Gs::texel(uint64_t tex0, uint64_t texa, int x, int y) const {
    const uint32_t tbp = bits(tex0, 0, 14), tbw = bits(tex0, 14, 6), psm = bits(tex0, 20, 6);
    const uint32_t cbp = bits(tex0, 37, 14), cpsm = bits(tex0, 51, 4), csa = bits(tex0, 56, 5);
    const uint32_t ta0 = bits(texa, 0, 8), aem = bits(texa, 15, 1), ta1 = bits(texa, 32, 8);
    auto c16 = [&](uint32_t v) {
        const uint32_t r = (v & 0x1f) << 3, g = ((v >> 5) & 0x1f) << 3, b = ((v >> 10) & 0x1f) << 3;
        uint32_t a = (v & 0x8000) ? ta1 : ta0;
        if (aem && (v & 0x7fff) == 0 && !(v & 0x8000)) a = 0;
        return r | (g << 8) | (b << 16) | (a << 24);
    };
    const uint32_t raw = read_pixel(psm, tbp, tbw, static_cast<uint32_t>(x), static_cast<uint32_t>(y));
    switch (psm) {
    case CT32: return raw;
    case CT24: {
        const uint32_t a = (aem && raw == 0) ? 0 : ta0;
        return raw | (a << 24);
    }
    case CT16:
    case CT16S: return c16(raw);
    default: break;
    }
    // indexed: the CLUT (CSM1) from cbp
    const bool eight = psm == T8 || psm == T8H;
    uint32_t cx, cy;
    if (eight) {
        const uint32_t j = (raw & 0xe7) | ((raw & 0x08) << 1) | ((raw & 0x10) >> 1);
        cx = j & 0xf;
        cy = j >> 4;
    } else {
        cx = raw & 7;
        cy = raw >> 3;
        (void)csa;
    }
    rd_slot_ = 1;  // (the CLUT's bytes are a range of their own)
    const uint32_t entry = cpsm == CT32 ? read_pixel(CT32, cbp, 1, cx, cy) : c16(read_pixel(cpsm == CT16S ? CT16S : CT16, cbp, 1, cx, cy));
    rd_slot_ = 0;
    return entry;
}

std::shared_ptr<const TexImage> Gs::texture_at_kick(uint64_t tex0, uint64_t texa) {
    // (the snapshots' images bounded: the game uploads textures at addresses that keep changing, so the keys keep growing - over
    // minutes of play hundreds of MB, the Wii U's memory gone. Past the budget they are dropped, all at once: the images this
    // frame's primitives hold stay alive with them, the others are decoded again when met - the same pixels)
    constexpr size_t kSnapBudget = size_t{64} << 20;
    if (tex_snap_bytes_ > kSnapBudget) {
        tex_snaps_.clear();
        tex_snap_bytes_ = 0;
    }
    TexSnap& s = tex_snaps_[{tex0 & 0x1fffffffffffffffull, texa}];  // CLD not part of the image
    if (s.img != nullptr) {
        if (s.gen == gen_) return s.img;
        if (!snap_stale(s)) {  // the memory it read is as it was: the same image
            s.gen = gen_;
            wr_open_ = false;  // (a change from here on is later than the snapshot)
            s.seq = wr_seq_;
            return s.img;
        }
    }
    auto img = std::make_shared<TexImage>();
    img->id = ++tex_ids_;
    img->w = 1 << std::min(10u, bits(tex0, 26, 4));
    img->h = 1 << std::min(10u, bits(tex0, 30, 4));
    img->px.resize(static_cast<size_t>(img->w) * img->h);
    tex_snap_bytes_ += img->px.size() * 4;
    rd_track_ = true;
    for (int k = 0; k < 2; ++k) {
        rd_lo_[k] = 0xffffffffu;
        rd_hi_[k] = 0;
    }
    for (int y = 0; y < img->h; ++y)
        for (int x = 0; x < img->w; ++x) img->px[static_cast<size_t>(y) * img->w + x] = texel(tex0, texa, x, y);
    rd_track_ = false;
    for (int k = 0; k < 2; ++k) {
        s.rd_lo[k] = rd_lo_[k];
        s.rd_hi[k] = rd_hi_[k];
    }
    s.gen = gen_;
    wr_open_ = false;
    s.seq = wr_seq_;
    s.img = img;
    return img;
}

void Gs::vertex_kick(const Vertex& vin, bool draw) {
    if (vu7_drop_ && vu_kick_ && !vu7_batches_.empty()) {  // (host: a kept batch's strip as the program kicked it, its ADC flags)
        const uint32_t t = bits(regs_[0x00], 0, 3);
        if (t == 3 || t == 4) vu7_batches_.back().kicked.push_back(draw ? 1 : 0);
    }
    const uint64_t prim = bits(regs_[0x1a], 0, 1) ? regs_[0x00] : (regs_[0x1b] & ~7ull) | (regs_[0x00] & 7);
    const int ctx = bits(prim, 9, 1);
    const uint64_t off = regs_[0x18 + ctx];
    Vertex v = vin;
    v.x = (v.x - static_cast<float>(bits(off, 0, 16))) / 16.0f;
    v.y = (v.y - static_cast<float>(bits(off, 32, 16))) / 16.0f;
    queue_.push_back(v);
    const int type = bits(regs_[0x00], 0, 3);
    int need = 0, assembled = 0;
    switch (type) {
    case 0: need = 1; assembled = 0; break;
    case 1: case 2: need = 2; assembled = 1; break;
    case 3: case 4: case 5: need = 3; assembled = 3; break;
    case 6: need = 2; assembled = 6; break;
    default: queue_.clear(); return;
    }
    if (static_cast<int>(queue_.size()) < need) return;
    if (draw && type != 0) {
        if (scr_prim_ >= 0) screen_picture();  // (the frame's picture put back before this primitive)
        Primitive p;
        p.type = assembled;
        DrawState& s = p.st;
        s.prim = prim;
        s.tex0 = regs_[0x06 + ctx];
        s.clamp = regs_[0x08 + ctx];
        s.tex1 = regs_[0x14 + ctx];
        s.miptbp1 = regs_[0x34 + ctx];
        s.alpha = regs_[0x42 + ctx];
        s.test = regs_[0x47 + ctx];
        s.scissor = regs_[0x40 + ctx];
        s.fba = regs_[0x4a + ctx];
        s.frame = regs_[0x4c + ctx];
        s.zbuf = regs_[0x4e + ctx];
        s.texa = regs_[0x3b];
        s.colclamp = regs_[0x46];
        s.pabe = regs_[0x49];
        s.fogcol = regs_[0x3d];
        s.dthe = regs_[0x45];
        s.xyoffset = off;
        p.v.assign(queue_.end() - need, queue_.end());
        if (bits(prim, 4, 1) != 0) {
            // mipmapping (TEX1 MXL > 0, MMIN *_MIPMAP_*): the GS picks the level per pixel from
            // its LOD (LCM 0: log2(1 / |Q|) << L + K, Q interpolated; LCM 1: K), rounded, clamped
            // to MXL; level n's base / width from MIPTBP1 (levels 1..3), its size halved n times.
            // The levels the primitive's vertices span are read at the kick; the draw picks per pixel.
            const uint32_t mxl = std::min(bits(s.tex1, 2, 3), 3u);
            if (mxl != 0 && !bits(prim, 8, 1) && bits(s.tex1, 6, 3) >= 2) {
                const float k = static_cast<float>(static_cast<int32_t>(bits(s.tex1, 32, 12) << 20) >> 20) / 16.0f;
                auto level_of = [&](float q) {
                    if (std::isnan(q)) return 0;
                    const float lod = bits(s.tex1, 0, 1) != 0 ? k : std::log2(1.0f / std::max(std::fabs(q), 1e-20f)) * static_cast<float>(1u << bits(s.tex1, 19, 2)) + k;
                    if (std::isnan(lod) || lod <= 0.0f) return 0;
                    return std::min(static_cast<int>(std::floor(lod + 0.5f)), static_cast<int>(mxl));
                };
                int lo = 3, hi = 0;
                for (auto it = queue_.end() - need; it != queue_.end(); ++it) {
                    lo = std::min(lo, level_of(it->q));
                    hi = std::max(hi, level_of(it->q));
                }
                lo = std::clamp(lo, 0, static_cast<int>(mxl));
                hi = std::clamp(hi, lo, static_cast<int>(mxl));
                auto level_tex0 = [&](int level) -> uint64_t {
                    if (level == 0) return s.tex0;
                    const uint32_t lv = static_cast<uint32_t>(level - 1);
                    const uint64_t tbp = bits(s.miptbp1, static_cast<int>(20 * lv), 14), tbw = bits(s.miptbp1, static_cast<int>(20 * lv + 14), 6);
                    const uint64_t tw = bits(s.tex0, 26, 4), th = bits(s.tex0, 30, 4);
                    const uint64_t ntw = tw > lv + 1 ? tw - lv - 1 : 0, nth = th > lv + 1 ? th - lv - 1 : 0;
                    return (s.tex0 & ~((0x3fffull) | (0x3full << 14) | (0xfull << 26) | (0xfull << 30))) | tbp | (tbw << 14) | (ntw << 26) | (nth << 30);
                };
                p.mip_lo = lo;
                p.mip_n = hi - lo;
                for (int l = lo + 1; l <= hi; ++l) {
                    int idx = l - lo - 1;
                    if (idx >= 0 && idx < 3) {
                        p.mip_tex0[idx] = level_tex0(l);
                        p.mip_tex[idx] = texture_at_kick(p.mip_tex0[idx], s.texa);
                    }
                }
                s.tex0 = level_tex0(lo);
            }
            p.tex = texture_at_kick(s.tex0, s.texa);
        }
        if (!bits(prim, 3, 1))  // flat: the last vertex's colour
            for (Vertex& w : p.v) { w.r = p.v.back().r; w.g = p.v.back().g; w.b = p.v.back().b; w.a = p.v.back().a; }
        p.vu = vu_kick_;
        p.vu_start = vu_kick_ ? vu1::g_last_start : 0;
        p.moby = moby_kick_;
        // a substituted range: both kept, marked (the drawing takes the projected 3D from the substitute, the 2D from the
        // real range: gs_draw.cpp)
        p.sub = sub_wide_ ? 1 : sub_drop_ ? 2 : 0;
        p.wide = sub_wide_;
        if (sub_drop_) ++sub_dropped_;
        p.fx = fx_now_;
        p.fx_id = fx_id_;
        p.layer = layer_now_;
        p.hud = hud_now_;
        if (vu_kick_ && vu7_drop_) {  // (a batch the MRP draws itself: set_chain_vu7)
            p.fx = vu7_kind_;
            p.fx_id = kVu7Kicked;
        }
        prims_.push_back(std::move(p));
    }
    switch (type) {
    case 2: queue_.erase(queue_.begin(), queue_.end() - 1); break;
    case 4: queue_.erase(queue_.begin()); break;
    case 5: queue_.erase(queue_.begin() + 1); break;
    default: queue_.clear(); break;
    }
}

void Gs::write_reg(uint32_t reg, uint64_t v) {
    reg &= 0x7f;
    regs_[reg] = v;
    switch (reg) {
    case 0x00: queue_.clear(); break;  // PRIM
    case 0x01:
        cur_.r = bits(v, 0, 8); cur_.g = bits(v, 8, 8); cur_.b = bits(v, 16, 8); cur_.a = bits(v, 24, 8);
        cur_.q = f32(bits(v, 32, 32));
        break;
    case 0x02: cur_.s = f32(bits(v, 0, 32)); cur_.t = f32(bits(v, 32, 32)); break;
    case 0x03: cur_.u = bits(v, 0, 14) / 16.0f; cur_.v = bits(v, 16, 14) / 16.0f; break;
    case 0x0a: cur_.fog = bits(v, 56, 8); break;
    case 0x04: case 0x0c: {  // XYZF2 / XYZF3
        Vertex k = cur_;
        k.x = static_cast<float>(bits(v, 0, 16)); k.y = static_cast<float>(bits(v, 16, 16)); k.z = bits(v, 32, 24); k.fog = bits(v, 56, 8);
        vertex_kick(k, reg == 0x04);
        break;
    }
    case 0x05: case 0x0d: {  // XYZ2 / XYZ3
        Vertex k = cur_;
        k.x = static_cast<float>(bits(v, 0, 16)); k.y = static_cast<float>(bits(v, 16, 16)); k.z = bits(v, 32, 32);
        vertex_kick(k, reg == 0x05);
        break;
    }
    case 0x53:  // TRXDIR: host -> local
        if (bits(v, 0, 2) == 0) {
            xfer_ = true; xx_ = 0; xy_ = 0; xpend_.clear(); wr_open_ = false;
            if (scr_bw_ != 0) screen_transfer(bits(regs_[0x50], 32, 14));
        }
        else xfer_ = false;
        break;
    default: break;
    }
}

void Gs::image_data(const uint8_t* p, uint32_t n) {
    if (!xfer_) return;
    const uint64_t bb = regs_[0x50], pos = regs_[0x51], rg = regs_[0x52];
    const uint32_t dbp = bits(bb, 32, 14), dbw = bits(bb, 48, 6), dpsm = bits(bb, 56, 6);
    const uint32_t sx = bits(pos, 32, 11), sy = bits(pos, 48, 11);
    const uint32_t w = bits(rg, 0, 12), h = bits(rg, 32, 12);
    if (w == 0 || h == 0) return;
    auto put = [&](uint32_t v) {
        if (xy_ >= h) return;
        write_pixel(dpsm, dbp, dbw, (sx + xx_) % 2048, (sy + xy_) % 2048, v);
        if (++xx_ == w) { xx_ = 0; ++xy_; }
    };
    if (dpsm == CT32) { for (uint32_t i = 0; i + 4 <= n; i += 4) put(r32(p + i)); }
    else if (dpsm == CT16 || dpsm == CT16S) { for (uint32_t i = 0; i + 2 <= n; i += 2) put(half_at(p, i)); }
    else if (dpsm == T8 || dpsm == T8H) { for (uint32_t i = 0; i < n; ++i) put(byte_at(p, i)); }
    else if (dpsm == T4 || dpsm == T4HL || dpsm == T4HH) { for (uint32_t i = 0; i < n; ++i) { put(byte_at(p, i) & 0xf); put(byte_at(p, i) >> 4); } }
    else if (dpsm == CT24) {
        for (uint32_t i = 0; i < n; ++i) xpend_.push_back(byte_at(p, i));
        size_t i = 0;
        for (; i + 3 <= xpend_.size(); i += 3) put(xpend_[i] | (xpend_[i + 1] << 8) | (xpend_[i + 2] << 16));
        xpend_.erase(xpend_.begin(), xpend_.begin() + static_cast<long>(i));
    }
    if (xy_ >= h) xfer_ = false;
}

void Gs::packed(const uint8_t* q, uint32_t d) {
    const uint32_t w0 = r32(q), w1 = r32(q + 4), w2 = r32(q + 8), w3 = r32(q + 12);
    switch (d) {
    case 0x0: write_reg(0x00, w0 & 0x7ff); break;
    case 0x1:
        cur_.r = w0 & 0xff; cur_.g = w1 & 0xff; cur_.b = w2 & 0xff; cur_.a = w3 & 0xff;
        regs_[0x01] = (w0 & 0xff) | ((w1 & 0xff) << 8) | ((w2 & 0xff) << 16) | (static_cast<uint64_t>(w3 & 0xff) << 24);
        break;
    case 0x2: cur_.s = f32(w0); cur_.t = f32(w1); cur_.q = f32(w2); break;
    case 0x3: cur_.u = (w0 & 0x3fff) / 16.0f; cur_.v = (w1 & 0x3fff) / 16.0f; break;
    case 0x4: case 0xc: {
        Vertex k = cur_;
        k.x = static_cast<float>(w0 & 0xffff); k.y = static_cast<float>(w1 & 0xffff); k.z = (w2 >> 4) & 0xffffff; k.fog = (w3 >> 4) & 0xff;
        vertex_kick(k, d == 0x4 && !((w3 >> 15) & 1));
        break;
    }
    case 0x5: case 0xd: {
        Vertex k = cur_;
        k.x = static_cast<float>(w0 & 0xffff); k.y = static_cast<float>(w1 & 0xffff); k.z = w2;
        vertex_kick(k, d == 0x5 && !((w3 >> 15) & 1));
        break;
    }
    case 0xa: cur_.fog = (w3 >> 4) & 0xff; break;
    case 0xe: write_reg(byte_at(q, 8), r64(q)); break;
    case 0xf: break;
    default: write_reg(d, r64(q)); break;  // TEX0 / CLAMP: the register's own layout
    }
}

// GIF data, a quadword at a time: a tag and its data may arrive in separate transfers (an
// IMAGE tag in a CNT, its pixels in the next REF), so the tag in progress is kept.
void Gs::gif(const uint8_t* p, uint32_t qwc) {
    for (uint32_t i = 0; i < qwc; ++i) {
        const uint8_t* q = p + i * 16;
        if (tag_left_ == 0) {
            const uint64_t lo = r64(q), hi = r64(q + 8);
            const uint32_t nloop = bits(lo, 0, 15);
            tag_flg_ = bits(lo, 58, 2);
            tag_nreg_ = bits(lo, 60, 4);
            if (tag_nreg_ == 0) tag_nreg_ = 16;
            tag_regs_ = hi;
            tag_reg_ = 0;
            if (tag_flg_ == 0 && bits(lo, 46, 1)) write_reg(0x00, bits(lo, 47, 11));
            // the data's size: PACKED nloop x nreg quadwords, REGLIST nloop x nreg double words, IMAGE nloop quadwords
            tag_left_ = tag_flg_ == 0 ? nloop * tag_nreg_ : tag_flg_ == 1 ? nloop * tag_nreg_ : nloop;
            continue;
        }
        if (tag_flg_ == 0) {
            packed(q, bits(tag_regs_, static_cast<int>(4 * tag_reg_), 4));
            tag_reg_ = (tag_reg_ + 1) % tag_nreg_;
            --tag_left_;
        } else if (tag_flg_ == 1) {
            for (int h = 0; h < 2 && tag_left_ != 0; ++h) {
                const uint32_t d = bits(tag_regs_, static_cast<int>(4 * tag_reg_), 4);
                if (d != 0xe && d != 0xf) write_reg(d, r64(q + 8 * h));
                tag_reg_ = (tag_reg_ + 1) % tag_nreg_;
                --tag_left_;
            }
        } else {
            const uint32_t take = std::min(tag_left_, qwc - i);
            image_data(q, take * 16);
            tag_left_ -= take;
            i += take - 1;
        }
    }
}

void Gs::vif_stream(const uint8_t* p, uint32_t n, bool) {
    uint32_t i = 0;
    while (i + 4 <= n) {
        const uint32_t code = r32(p + i);
        i += 4;
        const uint32_t cmd = (code >> 24) & 0x7f, imm = code & 0xffff;
        static const bool vif_trace = std::getenv("RAC1_VIF_TRACE") != nullptr;  // testing: the VIF1 codes
        if (vif_trace) std::fprintf(stderr, "[VIF1] %08x\n", code);
        static const bool mark_trace = std::getenv("RAC1_VU1_TRACE") != nullptr;  // testing: the passes' MARKs in the VU1 trace
        if (mark_trace && cmd == 0x07) std::fprintf(stderr, "[VU1] MARK %04x\n", imm);
        if (dry_) return;  // (host, a skipped tag: its VIF codes and data are not processed)
        if (cmd == 0x50 || cmd == 0x51) {  // DIRECT / DIRECTHL: GIF data (qwords)
            const uint32_t q = imm == 0 ? 65536 : imm;
            i = (i + 15) & ~15u;
            const uint32_t take = std::min(q * 16, n - std::min(n, i));
            gif(p + i, take / 16);
            i += q * 16;
        } else {  // the VU1 side (cycle, masks, UNPACK, MPG, double buffer, program starts)
            vu1::Vu1& vu = vu1::vu1_global();
            const uint32_t nb = vu1::vif_data_bytes(vu, code);
            if (nb == 0 || i + nb <= n) vu1::vif1_command(vu, code, p + i, [this](const uint8_t* d, uint32_t q) {
                vu_kick_ = true;
                gif(d, q);
                vu_kick_ = false;
            });
            i += nb;
        }
        // NOP, MSKPATH3, MARK, FLUSH*: nothing to do
    }
}

void Gs::add_chain_substitute(uint32_t first, uint32_t end, std::vector<uint8_t> chain) {
    Sub u;
    u.first = first & 0x01fffff0u;
    u.end = end & 0x01fffff0u;
    u.chain = std::move(chain);
    if (u.chain.empty() || u.first == 0 || static_cast<int>(subs_.size()) >= kMaxSubs) return;
    subs_.push_back(std::move(u));
}
uint32_t Gs::sub_size_of(int k) const {
    if (k < 0 || k >= static_cast<int>(subs_.size())) return 0;
    const Sub& u = subs_[static_cast<size_t>(k)];
    return u.replay_size != 0xffffffffu ? u.replay_size : static_cast<uint32_t>(u.chain.size());
}
// (host) EE memory, or substitute k's bytes for an address that carries kSubFlag (and k)
const uint8_t* Gs::sub_mem(uint32_t a) const {
    if ((a & kSubFlag) == 0) return static_cast<const uint8_t*>(ee_ram_ptr_read(a));
    const size_t k = (a >> kSubShift) & 7u;
    if (k >= subs_.size()) return nullptr;
    const Sub& u = subs_[k];
    const uint32_t o = (a & 0x01ffffffu) - u.first;
    return o < u.chain.size() ? u.chain.data() + o : nullptr;
}

// (host) see gs_emu.h
void Gs::set_chain_fx(std::vector<FxRange> ranges, bool external) {
    std::sort(ranges.begin(), ranges.end(), [](const FxRange& a, const FxRange& b) { return a.sub != b.sub ? a.sub < b.sub : a.first < b.first; });
    fx_ranges_ = std::move(ranges);
    fx_external_ = external && !fx_ranges_.empty();
}
// (the range holding the tag at `addr`, by a binary search: a frame has hundreds of them, one per effect quad)
const Gs::FxRange* Gs::fx_tag(uint32_t addr, uint8_t sub) const {
    auto it = std::upper_bound(fx_ranges_.begin(), fx_ranges_.end(), std::make_pair(sub, addr), [](const std::pair<uint8_t, uint32_t>& k, const FxRange& r) {
        return k.first != r.sub ? k.first < r.sub : k.second < r.first;
    });
    if (it == fx_ranges_.begin()) return nullptr;
    --it;
    return it->sub == sub && addr >= it->first && addr <= it->last ? &*it : nullptr;
}

// (host) see gs_emu.h
void Gs::set_chain_layers(std::vector<std::pair<uint32_t, uint32_t>> ranges) {
    std::sort(ranges.begin(), ranges.end());
    layer_ranges_ = std::move(ranges);
}

// (host) see gs_emu.h
void Gs::set_chain_vu7(std::vector<Vu7Range> ranges, uint64_t hash, uint8_t kind) {
    std::sort(ranges.begin(), ranges.end(), [](const Vu7Range& a, const Vu7Range& b) { return a.sub != b.sub ? a.sub < b.sub : a.first < b.first; });
    vu7_ranges_ = std::move(ranges);
    vu7_hash_ = hash;
    vu7_kind_ = kind;
    if (!vu7_ranges_.empty() && kind != 0) fx_external_ = true;
}

// (host) see gs_emu.h: before a program start on the walk's VU1 - a batch of program 7 in a vu7 range kept as the program finds it
// (its GIF tag, then per vertex ST, RGBA, XYZ at XTOP; the constants the start-up loaded: vf18..vf21, vf30, vf31.w, vf7), with the
// GS state its kick would have, and its place in the primitives' order
void Gs::vu1_before(void* vu_p, uint32_t start, uint64_t hash) {
    vu1::Vu1& vu = *static_cast<vu1::Vu1*>(vu_p);
    if (!vu7_now_ || hash != vu7_hash_ || vu7_kind_ == 0 || (start != 0xc && start != 0xe)) return;
    const VUState& st = vu.st;
    auto q = [&st](uint32_t a) { return &st.mem[(a & 0x3ffu) << 4]; };
    const uint32_t x = st.xtop;
    const uint64_t tag = r64(q(x));
    const uint32_t n = static_cast<uint32_t>(tag & 0x7fff);
    if (n < 3 || n > 0x150) return;  // (a batch holds at most (0x1d2 - 1) / 3 vertices)
    Vu7Batch b;
    // the GS state a kick under a GIF tag `t` would have now (its PRIM when PRE, the context's registers)
    auto state_of = [this](uint64_t t, DrawState& s, std::shared_ptr<const TexImage>& tex, bool* mip) {
        const uint64_t prim0 = bits(t, 46, 1) != 0 ? bits(t, 47, 11) : regs_[0x00];
        const uint64_t prim = bits(regs_[0x1a], 0, 1) ? prim0 : (regs_[0x1b] & ~7ull) | (prim0 & 7);
        const int ctx = bits(prim, 9, 1);
        s.prim = prim;
        s.tex0 = regs_[0x06 + ctx];
        s.clamp = regs_[0x08 + ctx];
        s.tex1 = regs_[0x14 + ctx];
        s.miptbp1 = regs_[0x34 + ctx];
        s.alpha = regs_[0x42 + ctx];
        s.test = regs_[0x47 + ctx];
        s.scissor = regs_[0x40 + ctx];
        s.fba = regs_[0x4a + ctx];
        s.frame = regs_[0x4c + ctx];
        s.zbuf = regs_[0x4e + ctx];
        s.texa = regs_[0x3b];
        s.colclamp = regs_[0x46];
        s.pabe = regs_[0x49];
        s.fogcol = regs_[0x3d];
        s.dthe = regs_[0x45];
        s.xyoffset = regs_[0x18 + ctx];
        if (bits(prim, 4, 1) != 0) {
            tex = texture_at_kick(s.tex0, s.texa);
            if (mip != nullptr) *mip = std::min(bits(s.tex1, 2, 3), 3u) != 0 && bits(s.tex1, 6, 3) >= 2;
        }
    };
    state_of(tag, b.st, b.tex, &b.mip);
    if (start == 0xe) {
        const uint64_t ft = r64(reinterpret_cast<const uint8_t*>(&st.vf[31].u[0]));
        state_of(ft, b.st_fan, b.tex_fan, nullptr);
    }
    const DrawState& s = b.st;
    for (int c = 0; c < 4; ++c)
        for (int l = 0; l < 4; ++l) b.m[4 * c + l] = st.vf[static_cast<size_t>(18 + c)].f[l];
    for (int c = 0; c < 4; ++c)
        for (int l = 0; l < 4; ++l) b.clip[4 * c + l] = st.vf[static_cast<size_t>(22 + c)].f[l];
    for (int l = 0; l < 4; ++l) b.off[l] = st.vf[30].f[l];
    b.k = st.vf[31].f[3];
    b.fog[0] = st.vf[7].f[0];
    b.fog[1] = st.vf[7].f[1];
    b.first = static_cast<uint32_t>(vu7_verts_.size());
    b.n = n;
    b.start = start;
    b.tag = vu7_tag_;
    b.sub = sub_wide_ ? 1 : sub_drop_ ? 2 : 0;
    for (uint32_t i = 0; i < n; ++i) {
        Vu7Vertex v;
        float f[4];
        uint32_t u[4];
        std::memcpy(f, q(x + 1 + 3 * i), 16);
        v.st[0] = f[0];
        v.st[1] = f[1];
        v.st[2] = f[2];
        std::memcpy(u, q(x + 2 + 3 * i), 16);
        for (int l = 0; l < 4; ++l) v.rgba[l] = static_cast<uint8_t>(u[l]);
        std::memcpy(f, q(x + 3 + 3 * i), 16);
        v.xyz[0] = f[0];
        v.xyz[1] = f[1];
        v.xyz[2] = f[2];
        vu7_verts_.push_back(v);
    }
    if (scr_prim_ >= 0) screen_picture();
    Primitive p;  // (its place in the primitives' order)
    p.type = 9;
    p.st = s;
    p.fx = vu7_kind_;
    p.fx_id = static_cast<uint32_t>(vu7_batches_.size());
    p.sub = b.sub;
    p.wide = sub_wide_;
    p.vu = true;
    p.vu_start = start;
    prims_.push_back(std::move(p));
    vu7_batches_.push_back(std::move(b));
    vu7_drop_ = true;
}

// (host) see gs_emu.h
void Gs::vu1_after(void*, uint32_t, uint64_t) { vu7_drop_ = false; }

// (host) see gs_emu.h
void Gs::set_chain_hud(std::vector<HudRange> ranges) {
    std::sort(ranges.begin(), ranges.end(), [](const HudRange& a, const HudRange& b) { return a.first < b.first; });
    hud_ranges_ = std::move(ranges);
}

void Gs::set_chain_skips(std::vector<SkipRange> ranges) {
    std::sort(ranges.begin(), ranges.end(), [](const SkipRange& a, const SkipRange& b) { return a.first < b.first; });
    skip_ranges_ = std::move(ranges);
    skip_started_.assign(skip_ranges_.size(), false);
}

// (host) whether the tag at `addr` is in a skip range (the last range starting at or before it)
bool Gs::tag_skipped(uint32_t addr, size_t* range) const {
    if (skip_ranges_.empty()) return false;
    auto it = std::upper_bound(skip_ranges_.begin(), skip_ranges_.end(), addr, [](uint32_t a, const SkipRange& r) { return a < r.first; });
    while (it != skip_ranges_.begin()) {
        --it;
        if (addr <= it->last) {
            if (range != nullptr) *range = static_cast<size_t>(it - skip_ranges_.begin());
            return true;
        }
        if (addr - it->first > 0x100000u) break;  // (ranges are short: no need to look further back)
    }
    return false;
}

// The chain walked (its DMA tags followed) by `mem` (the tag's or the data's EE address and length -> the bytes, nullptr: none).
template <class Mem>
void Gs::chain_walk(uint32_t tag, uint32_t end, Mem&& mem) {
    struct ClearSkips {  // (host) the skip ranges last for one chain
        Gs& g;
        ~ClearSkips() {
            g.skip_ranges_.clear();
            g.fx_ranges_.clear();
            g.fx_now_ = 0;
            g.layer_ranges_.clear();
            g.layer_now_ = false;
            g.hud_ranges_.clear();
            g.hud_now_ = 0;
            g.vu7_ranges_.clear();
            g.vu7_now_ = false;
            g.vu7_drop_ = false;
        }
    } clear_skips{*this};
    struct Observe : vu1::StartObserver {  // (host) the program starts of this walk (vu1_before / vu1_after), on this thread
        Gs& g;
        vu1::StartObserver* before_obs;
        explicit Observe(Gs& gs) : g(gs), before_obs(vu1::g_start_observer) { vu1::g_start_observer = this; }
        ~Observe() override { vu1::g_start_observer = before_obs; }
        void before(vu1::Vu1& vu, uint32_t start, uint64_t hash) override { g.vu1_before(&vu, start, hash); }
        void after(vu1::Vu1& vu, uint32_t start, uint64_t hash) override { g.vu1_after(&vu, start, hash); }
    } observe{*this};
    struct ClearSub {  // (host) the substitutes last for one chain
        Gs& g;
        ~ClearSub() {
            g.subs_.clear();
            g.sub_wide_ = g.sub_drop_ = g.vundo_on_ = false;
        }
    } clear_sub{*this};
    struct SideWiden {  // (host) the view's widening for the VU1 programs this walk runs (set_side_widen), on this thread
        float before[2];
        explicit SideWiden(const float w[2]) : before{vu1::g_side_widen[0], vu1::g_side_widen[1]} {
            vu1::g_side_widen[0] = w[0] > 1.0f ? w[0] : 1.0f;
            vu1::g_side_widen[1] = w[1] > 1.0f ? w[1] : 1.0f;
        }
        ~SideWiden() {
            vu1::g_side_widen[0] = before[0];
            vu1::g_side_widen[1] = before[1];
        }
    } side_widen{side_widen_};
    std::vector<uint32_t> stack;
    std::vector<uint8_t> buf;
    uint32_t addr = tag & 0x01fffff0u;
    // inside substitute `cur`: an address in [first, first + size) is the substitute's (kSubFlag)
    bool in_sub = false;
    int cur = -1;
    uint32_t sub_lo = 0, sub_hi = 0;
    uint32_t done = 0;  // (the substitutes walked)
    auto sub_addr = [&](uint32_t a) { return in_sub && a >= sub_lo && a < sub_hi ? sub_flagged(a, cur) : a; };
    vu1::Vu1 vu_saved;
    struct GsSaved {  // the GS / VIF / GIF state the substitute's walk changes (its uploads into the local memory: vundo_)
        uint64_t regs[0x80];
        Vertex cur;
        std::vector<Vertex> queue;
        bool xfer;
        uint32_t xx, xy;
        std::vector<uint8_t> xpend;
        uint32_t tag_left, tag_flg, tag_nreg, tag_reg;
        uint64_t tag_regs;
        uint32_t cl, wl;
    };
    std::unique_ptr<GsSaved> gs_saved;
    for (int guard = 0; guard < 200000; ++guard) {
        if (!in_sub && !sub_drop_)  // a substitute first, from the real range's first tag
            for (int k = 0; k < static_cast<int>(subs_.size()); ++k)
                if ((done >> k & 1u) == 0 && addr == subs_[static_cast<size_t>(k)].first) {
                    cur = k;
                    sub_lo = subs_[static_cast<size_t>(k)].first;
                    sub_hi = sub_lo + sub_size_of(k);
                    break;
                }
        if (cur >= 0 && !in_sub && !sub_drop_ && (done >> cur & 1u) == 0 && addr == sub_lo) {
            vu_saved = vu1::vu1_global();
            vundo_.clear();
            vundo_on_ = true;
            gs_saved.reset(new GsSaved{{}, cur_, queue_, xfer_, xx_, xy_, xpend_, tag_left_, tag_flg_, tag_nreg_, tag_reg_, tag_regs_, vif_cl_, vif_wl_});
            std::memcpy(gs_saved->regs, regs_, sizeof regs_);
            in_sub = sub_wide_ = true;
            sub_prims0_ = prims_.size();
            sub_dropped_ = 0;
            stack.clear();
        }
        if (in_sub && addr == sub_hi) {  // its end: the state put back, the real range walked (its primitives marked sub 2)
            vu1::vu1_global() = vu_saved;
            vundo_on_ = false;  // (the local memory as before the substitute: the real range's textures are its own)
            for (auto it = vundo_.rbegin(); it != vundo_.rend(); ++it) {
                std::memcpy(&vram_[it->a], &it->before, it->n);
                note_change(it->a, it->n);
            }
            vundo_.clear();
            const GsSaved& g = *gs_saved;
            std::memcpy(regs_, g.regs, sizeof regs_);
            cur_ = g.cur;
            queue_ = g.queue;
            xfer_ = g.xfer;
            xx_ = g.xx;
            xy_ = g.xy;
            xpend_ = g.xpend;
            tag_left_ = g.tag_left;
            tag_flg_ = g.tag_flg;
            tag_nreg_ = g.tag_nreg;
            tag_reg_ = g.tag_reg;
            tag_regs_ = g.tag_regs;
            vif_cl_ = g.cl;
            vif_wl_ = g.wl;
            in_sub = sub_wide_ = false;
            sub_drop_ = true;
            addr = sub_lo;
            stack.clear();
        }
        if (sub_drop_ && addr == subs_[static_cast<size_t>(cur)].end) {
            sub_drop_ = false;
            done |= 1u << cur;
            static const bool trace = std::getenv("RAC1_SUB_TRACE") != nullptr;  // (testing)
            if (trace) std::fprintf(stderr, "[sub] %d %08x..%08x: substitute + real %zu prims, real %llu\n", cur, sub_lo, subs_[static_cast<size_t>(cur)].end, prims_.size() - sub_prims0_, static_cast<unsigned long long>(sub_dropped_));
            cur = -1;
        }
        if (end != 0 && !in_sub && addr == (end & 0x01fffff0u)) return;  // (the substitute may reach past the real chain's end)
        const uint8_t* t = mem(sub_addr(addr), 16u);
        if (t == nullptr) return;
        const uint64_t lo = r64(t);
        const uint32_t qwc = bits(lo, 0, 16), id = bits(lo, 28, 3);
        const uint32_t a = bits(lo, 32, 31) & 0x01fffff0u;
        uint32_t data = addr + 16, next = addr + 16 + qwc * 16;
        bool stop = false;
        switch (id) {
        case 0: data = a; stop = true; break;               // REFE
        case 1: break;                                      // CNT
        case 2: next = a; break;                            // NEXT
        case 3: case 4: data = a; next = addr + 16; break;  // REF / REFS
        case 5: stack.push_back(next); next = a; break;     // CALL
        case 6:                                             // RET
            if (stack.empty()) stop = true;
            else { next = stack.back(); stack.pop_back(); }
            break;
        case 7: stop = true; break;  // END
        }
        static const bool vif_trace = std::getenv("RAC1_VIF_TRACE") != nullptr;
        if (vif_trace) std::fprintf(stderr, "[DMA] tag @%08x id %u qwc %u data %08x\n", addr, id, qwc, data);
        static const bool vu1_trace = std::getenv("RAC1_VU1_TRACE") != nullptr;  // testing: where in the chain the VU1 events are
        if (vu1_trace) std::fprintf(stderr, "[VU1] tag @%08x data %08x qwc %u\n", addr, data, qwc);
        // the tag's VIF codes (its upper 64 bits, stream offsets 8 and 12; its lower half, not
        // sent to the VIF, as two NOPs) then its data from the next quadword, one stream
        size_t k = 0;
        dry_ = !in_sub && tag_skipped(addr, &k);  // (the skip ranges are the real chain's)
        {  // (the MRP's effects)
            const FxRange* fr = fx_ranges_.empty() ? nullptr : fx_tag(addr, in_sub ? static_cast<uint8_t>(cur + 1) : 0);
            fx_now_ = fr != nullptr ? fr->kind : 0;
            fx_id_ = fr != nullptr ? fr->id : 0;
        }
        if (fx_now_ != 0 && fx_mark_ == SIZE_MAX) fx_mark_ = prims_.size();
        layer_now_ = false;
        if (!layer_ranges_.empty() && !in_sub) {  // (a screen layer's: the real chain's tags)
            auto it = std::upper_bound(layer_ranges_.begin(), layer_ranges_.end(), addr,
                                       [](uint32_t a, const std::pair<uint32_t, uint32_t>& r) { return a < r.first; });
            layer_now_ = it != layer_ranges_.begin() && addr <= std::prev(it)->second;
        }
        vu7_now_ = false;
        if (!vu7_ranges_.empty()) {  // (the MRP's program-7 surfaces)
            const uint8_t sb = in_sub ? static_cast<uint8_t>(cur + 1) : 0;
            auto it = std::upper_bound(vu7_ranges_.begin(), vu7_ranges_.end(), std::make_pair(sb, addr), [](const std::pair<uint8_t, uint32_t>& k, const Vu7Range& r) {
                return k.first != r.sub ? k.first < r.sub : k.second < r.first;
            });
            if (it != vu7_ranges_.begin()) {
                --it;
                vu7_now_ = it->sub == sb && addr >= it->first && addr <= it->last;
                vu7_tag_ = it->tag;
            }
        }
        hud_now_ = 0;
        if (!hud_ranges_.empty() && !in_sub) {  // (a HUD element's: the real chain's tags)
            auto it = std::upper_bound(hud_ranges_.begin(), hud_ranges_.end(), addr, [](uint32_t a, const HudRange& r) { return a < r.first; });
            if (it != hud_ranges_.begin() && addr <= std::prev(it)->last) hud_now_ = std::prev(it)->side;
        }
        if (dry_) {
            ++skipped_tags_;
            if (!skip_started_[k]) {  // the range's VU1 program starts, once
                skip_started_[k] = true;
                if (skip_ranges_[k].moby && moby_mark_ == SIZE_MAX) moby_mark_ = prims_.size();
                if (skip_ranges_[k].host_3d != 0) {  // the host's 3D in the primitives' order, with context 1's state now
                    if (scr_prim_ >= 0) screen_picture();
                    Primitive p;
                    p.type = 8;
                    DrawState& s = p.st;
                    s.prim = regs_[0x00];
                    s.alpha = regs_[0x42];
                    s.test = regs_[0x47];
                    s.scissor = regs_[0x40];
                    s.fba = regs_[0x4a];
                    s.frame = regs_[0x4c];
                    s.zbuf = regs_[0x4e];
                    s.xyoffset = regs_[0x18];
                    p.fx_id = skip_ranges_[k].host_3d;
                    prims_.push_back(std::move(p));
                }
                vu1::Vu1& vu = vu1::vu1_global();
                for (uint16_t m : skip_ranges_[k].mscals)
                    vu1::vif1_command(vu, 0x14000000u | m, nullptr, [this](const uint8_t* d, uint32_t q) {
                        vu_kick_ = true;
                        gif(d, q);
                        vu_kick_ = false;
                    });
            }
        }
        buf.assign(16, 0);
        std::memcpy(buf.data() + 8, t + 8, 8);
        if (qwc != 0 && !dry_) {  // (a skipped tag's data is not read: vif_stream stops at its first code)
            const uint8_t* d = mem(sub_addr(data), qwc * 16);
            if (d != nullptr) buf.insert(buf.end(), d, d + qwc * 16);
        }
        vif_stream(buf.data(), static_cast<uint32_t>(buf.size()), true);
        dry_ = false;
        if (stop) return;
        addr = next;
    }
}

void Gs::vif1_chain(uint32_t tag, uint32_t end) {
    chain_walk(tag, end, [this](uint32_t a, uint32_t) { return sub_mem(a); });
    if (scr_prim_ >= 0) screen_picture();
}

// an IMAGE transfer into the local memory at block `dbp` begins: into the frame, a type-7 primitive (one for transfers in a row)
void Gs::screen_transfer(uint32_t dbp) {
    const uint32_t blocks = scr_bw_ * 64 * scr_h_ * 4 / 256;
    if (dbp < scr_bp_ || dbp >= scr_bp_ + blocks || scr_prim_ >= 0) return;
    Primitive p;
    p.type = 7;
    p.v.resize(2);
    p.v[1].x = static_cast<float>(scr_bw_ * 64);
    p.v[1].y = static_cast<float>(scr_h_);
    p.st.frame = (scr_bp_ / 32) | (static_cast<uint64_t>(scr_bw_) << 16);
    scr_prim_ = static_cast<long>(prims_.size());
    prims_.push_back(std::move(p));
}

// the type-7 primitive's picture: the frame's memory now
void Gs::screen_picture() {
    const uint32_t w = scr_bw_ * 64, h = scr_h_;
    const uint32_t lo = scr_bp_ * 256, hi = lo + w * h * 4;
    if (scr_snap_ != nullptr && scr_snap_bp_ == scr_bp_ && scr_snap_bw_ == scr_bw_ && scr_snap_h_ == scr_h_ && scr_seq_ >= wr_floor_) {
        bool stale = false;
        for (size_t i = wr_log_.size(); i-- > 0;) {
            const WrRange& r = wr_log_[i];
            if (r.seq <= scr_seq_) break;
            if (r.lo < hi && r.hi > lo) { stale = true; break; }
        }
        if (!stale) {
            prims_[static_cast<size_t>(scr_prim_)].tex = scr_snap_;
            scr_prim_ = -1;
            return;
        }
    }
    auto img = std::make_shared<TexImage>();
    img->id = ++tex_ids_;
    img->w = static_cast<int>(w);
    img->h = static_cast<int>(h);
    img->px.resize(static_cast<size_t>(w) * h);
    for (uint32_t y = 0; y < h; ++y) {
        uint32_t* row = img->px.data() + static_cast<size_t>(y) * w;
        for (uint32_t x = 0; x < w; ++x) row[x] = vram_r32(&vram_[addr32(scr_bp_, scr_bw_, x, y)]);
    }
    wr_open_ = false;
    scr_seq_ = wr_seq_;
    scr_snap_bp_ = scr_bp_;
    scr_snap_bw_ = scr_bw_;
    scr_snap_h_ = scr_h_;
    scr_snap_ = img;
    prims_[static_cast<size_t>(scr_prim_)].tex = std::move(img);
    scr_prim_ = -1;
}

// the tags and the data a vif1_chain(tag, end) would read, in its order (the skipped tags' data not)
void Gs::capture_chain(uint32_t tag, uint32_t end, ChainCapture& c) const {
    c.tag = tag;
    c.end = end;
    c.size = 0;
    c.spans.clear();
    c.subs.clear();
    c.side_widen[0] = side_widen_[0];
    c.side_widen[1] = side_widen_[1];
    for (int k = 0; k < static_cast<int>(subs_.size()); ++k) c.subs.push_back({subs_[static_cast<size_t>(k)].first, subs_[static_cast<size_t>(k)].end, sub_size_of(k)});
    auto take = [&c, this](uint32_t a, uint32_t len) -> const uint8_t* {
        const uint8_t* src = sub_mem(a);
        const size_t at = c.size;
        c.spans.push_back({a, static_cast<uint32_t>(at), src == nullptr ? 0xffffffffu : len});
        if (src == nullptr) return nullptr;
        if (at + len > c.cap) {
            const size_t ncap = std::max<size_t>(at + len, c.cap * 2 + 65536);
            std::unique_ptr<uint8_t[]> nb(new uint8_t[ncap]);
            if (at != 0) std::memcpy(nb.get(), c.bytes.get(), at);
            c.bytes = std::move(nb);
            c.cap = ncap;
        }
        std::memcpy(c.bytes.get() + at, src, len);
        c.size = at + len;
        return c.bytes.get() + at;
    };
    std::vector<uint32_t> stack;
    uint32_t addr = tag & 0x01fffff0u;
    // the substitute walked as chain_walk walks it (the same addresses asked for, in the same order)
    bool in_sub = false, drop = false;
    int cur = -1;
    uint32_t sub_lo = 0, sub_hi = 0, done = 0;
    auto sub_addr = [&](uint32_t a) { return in_sub && a >= sub_lo && a < sub_hi ? sub_flagged(a, cur) : a; };
    for (int guard = 0; guard < 200000; ++guard) {
        if (!in_sub && !drop)
            for (int k = 0; k < static_cast<int>(subs_.size()); ++k)
                if ((done >> k & 1u) == 0 && addr == subs_[static_cast<size_t>(k)].first) {
                    cur = k;
                    sub_lo = subs_[static_cast<size_t>(k)].first;
                    sub_hi = sub_lo + sub_size_of(k);
                    in_sub = true;
                    stack.clear();
                    break;
                }
        if (in_sub && addr == sub_hi) {
            in_sub = false;
            drop = true;
            addr = sub_lo;
            stack.clear();
        }
        if (drop && addr == subs_[static_cast<size_t>(cur)].end) {
            drop = false;
            done |= 1u << cur;
            cur = -1;
        }
        if (end != 0 && !in_sub && addr == (end & 0x01fffff0u)) return;  // (the substitute may reach past the real chain's end)
        const uint8_t* t = take(sub_addr(addr), 16u);
        if (t == nullptr) return;
        const uint64_t lo = r64(t);
        const uint32_t qwc = bits(lo, 0, 16), id = bits(lo, 28, 3);
        const uint32_t a = bits(lo, 32, 31) & 0x01fffff0u;
        uint32_t data = addr + 16, next = addr + 16 + qwc * 16;
        bool stop = false;
        switch (id) {
        case 0: data = a; stop = true; break;
        case 1: break;
        case 2: next = a; break;
        case 3: case 4: data = a; next = addr + 16; break;
        case 5: stack.push_back(next); next = a; break;
        case 6:
            if (stack.empty()) stop = true;
            else { next = stack.back(); stack.pop_back(); }
            break;
        case 7: stop = true; break;
        }
        if (qwc != 0 && (in_sub || !tag_skipped(addr, nullptr))) take(sub_addr(data), qwc * 16);
        if (stop) return;
        addr = next;
    }
}

// vif1_chain on a capture (any thread: no EE memory read)
void Gs::vif1_chain(const ChainCapture& c) {
    size_t at = 0;
    subs_.clear();  // (the substitutes' addresses as the capture saw them; their bytes are in the capture)
    for (const auto& u : c.subs) {
        Sub v;
        v.first = u[0];
        v.end = u[1];
        v.replay_size = u[2];
        subs_.push_back(std::move(v));
    }
    side_widen_[0] = c.side_widen[0];
    side_widen_[1] = c.side_widen[1];
    chain_walk(c.tag, c.end, [&c, &at](uint32_t a, uint32_t) -> const uint8_t* {
        if (at < c.spans.size() && c.spans[at][0] == a) {
            const auto& sp = c.spans[at++];
            return sp[2] == 0xffffffffu ? nullptr : c.bytes.get() + sp[1];
        }
        return static_cast<const uint8_t*>(ee_ram_ptr_read(a));  // (never: the capture follows the same tags)
    });
    if (scr_prim_ >= 0) screen_picture();
}

namespace {
StoreImageHook g_store_image_hook = nullptr;
}  // namespace

void set_store_image_hook(StoreImageHook hook) { g_store_image_hook = hook; }
StoreImageHook store_image_hook() { return g_store_image_hook; }

namespace {
// the chain's run on a worker thread
struct ChainWorker {
    std::mutex m;
    std::condition_variable cv;
    std::thread th;
    bool job = false, busy = false, quit = false;
    const Gs::ChainCapture* cap = nullptr;
    std::exception_ptr error;
    void loop() {
        std::unique_lock<std::mutex> lk(m);
        for (;;) {
            cv.wait(lk, [this] { return job || quit; });
            if (quit) return;
            job = false;
            lk.unlock();
            try {
                const rac1::prof::Job prof_job(rac1::prof::kWorkerChain);  // (the debug menu's profiler: the job's time)
                gs_global().vif1_chain(*cap);
            } catch (...) {
                error = std::current_exception();
            }
            lk.lock();
            busy = false;
            cv.notify_all();
        }
    }
    ~ChainWorker() {
        {
            std::lock_guard<std::mutex> lk(m);
            quit = true;
        }
        cv.notify_all();
        if (th.joinable()) th.join();
    }
} g_chain_worker;
}  // namespace

void chain_start_async(const Gs::ChainCapture& c) {
    ChainWorker& w = g_chain_worker;
    chain_join();
    std::lock_guard<std::mutex> lk(w.m);
    if (!w.th.joinable()) w.th = std::thread([&w] {
        rac1::prof::thread_name("GS chain worker");
        w.loop();
    });
    w.cap = &c;
    w.error = nullptr;
    w.busy = true;
    w.job = true;
    w.cv.notify_all();
}

void chain_join() {
    RAC1_PROF_ZONE(kChainJoinWait);
    ChainWorker& w = g_chain_worker;
    std::unique_lock<std::mutex> lk(w.m);
    w.cv.wait(lk, [&w] { return !w.busy; });
    if (w.error) {
        std::exception_ptr e = w.error;
        w.error = nullptr;
        std::rethrow_exception(e);
    }
}

}  // namespace rac1::gs
