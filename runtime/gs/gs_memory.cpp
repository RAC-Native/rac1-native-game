// See gs_memory.h. The local memory layouts (page / block / column tables) are the GS's, as in the GS User's Manual ch. 8 (the
// tables as OpenGOAL's common/texture/texture_conversion.h has them: Copyright (c) the OpenGOAL team, ISC license,
// third_party/989snd/LICENSE).
#include <runtime/gs/gs_memory.h>
#include <runtime/kernel/ee_memory.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>

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

// (EE memory is host-native 32-bit words, ee_memory.h: the PS2's byte i of a word-aligned block at i ^ EE_BYTE_SWIZZLE, its
// halfword at i ^ EE_HALF_SWIZZLE - the same on a little-endian host; an EE word is read as it is)
uint32_t r32(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}
uint8_t byte_at(const uint8_t* p, uint32_t i) { return p[i ^ EE_BYTE_SWIZZLE]; }
uint16_t half_at(const uint8_t* p, uint32_t i) {
    uint16_t v;
    std::memcpy(&v, p + (i ^ EE_HALF_SWIZZLE), 2);
    return v;
}
// (the GS's local memory, vram_, holds the PS2's bytes - its 16-bit, 8-bit and 4-bit pixels are read and written as bytes -:
// a 32-bit pixel is its four bytes little-endian, whatever the host's order)
uint32_t vram_r32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }
void vram_w32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
}
uint32_t bits(uint64_t v, int lo, int n) { return static_cast<uint32_t>((v >> lo) & ((1ull << n) - 1)); }

enum Psm : uint32_t { CT32 = 0, CT24 = 1, CT16 = 2, CT16S = 0xa, T8 = 0x13, T4 = 0x14, T8H = 0x1b, T4HL = 0x24, T4HH = 0x2c };
}  // namespace

LocalMemory::LocalMemory() : vram_(VRAM) {}


uint32_t LocalMemory::addr32(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y) {
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
uint32_t LocalMemory::addr16(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, bool s) {
    const uint32_t ppr = std::max(1u, bw);
    const uint32_t page = x / 64 + (y / 64) * ppr;
    const uint32_t px = x % 64, py = y % 64;
    const uint32_t block = s ? BLOCK16S[py / 8][px / 16] : BLOCK16[py / 8][px / 16];
    const uint32_t pix = PIX16[py % 8][px % 16];
    return ((bp * 128 + page * 4096 + block * 128 + pix) * 2) & (VRAM - 1);
}
uint32_t LocalMemory::addr8(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y) {
    const uint32_t ppr = std::max(1u, bw / 2);
    const uint32_t page = x / 128 + (y / 64) * ppr;
    const uint32_t px = x % 128, py = y % 64;
    const uint32_t block = BLOCK32[py / 16][px / 16];
    const uint32_t pix = PIX8[py % 16][px % 16];
    return (bp * 256 + page * 8192 + block * 256 + pix) & (VRAM - 1);
}
uint32_t LocalMemory::addr4(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y) {
    const uint32_t ppr = std::max(1u, bw / 2);
    const uint32_t page = x / 128 + (y / 128) * ppr;
    const uint32_t px = x % 128, py = y % 128;
    const uint32_t block = BLOCK16[py / 16][px / 32];
    const uint32_t pix = PIX4[py % 16][px % 32];
    return (bp * 512 + page * 16384 + block * 512 + pix) & (VRAM * 2 - 1);
}

uint32_t LocalMemory::read_pixel(uint32_t psm, uint32_t bp, uint32_t bw, uint32_t x, uint32_t y) const {
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
void LocalMemory::note_change(uint32_t a, uint32_t n) {
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
bool LocalMemory::snap_stale(const TexSnap& s) const {
    if (s.seq < wr_floor_) return true;
    for (size_t i = wr_log_.size(); i-- > 0;) {
        const WrRange& r = wr_log_[i];
        if (r.seq <= s.seq) break;
        for (int k = 0; k < 2; ++k)
            if (r.lo < s.rd_hi[k] && r.hi > s.rd_lo[k]) return true;
    }
    return false;
}

// (the bytes a write of this format touches (a, n), compared before and after: a write of what is there is no change - the
// snapshots of textures stay valid; a change is kept for undo_restore while undo_begin's scope lasts.) Returns the changed bytes'
// address and count (count 0: unchanged), for the caller to note.
inline uint32_t LocalMemory::store(uint32_t psm, uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t v, uint32_t& n_out) {
    if (psm == CT32) {
        const uint32_t a0 = addr32(bp, bw, x, y);
        n_out = 0;
        if (vram_r32(&vram_[a0]) != v) {
            uint32_t raw;  // (the bytes as they were, for the undo's put-back)
            std::memcpy(&raw, &vram_[a0], 4);
            vram_w32(&vram_[a0], v);
            if (vundo_on_) vundo_.push_back({a0, 4, raw});
            n_out = 4;
        }
        return a0;
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
    n_out = 0;
    if (n0 != 0 && std::memcmp(before, &vram_[a0], n0) != 0) {
        if (vundo_on_) {
            uint32_t w = 0;
            std::memcpy(&w, before, 4);
            vundo_.push_back({a0, n0, w});
        }
        n_out = n0;
    }
    return a0;
}

void LocalMemory::write_pixel(uint32_t psm, uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t v) {
    uint32_t n = 0;
    const uint32_t a = store(psm, bp, bw, x, y, v, n);
    if (n != 0) note_change(a, n);
}

void LocalMemory::write_ct32_rect(uint32_t bp, uint32_t bw, uint32_t w, uint32_t h, const uint32_t* px) {
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

void LocalMemory::write_pixel_raw(uint32_t psm, uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t v) {
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

uint32_t LocalMemory::texel(uint64_t tex0, uint64_t texa, int x, int y) const {
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

// (the image's key: the TEX0 / TEXA bits the decode reads. TFX, TCC and CLD never; the CLUT's fields only for an indexed format;
// TEXA only for a 16 / 24-bit texture or a 16-bit CLUT. A texture drawn with another texture function is the same image.)
// orig: (host)
std::pair<uint64_t, uint64_t> LocalMemory::texture_key(uint64_t tex0, uint64_t texa) {
    const uint32_t psm = bits(tex0, 20, 6), cpsm = bits(tex0, 51, 4);
    const bool indexed = psm == T8 || psm == T8H || psm == T4 || psm == T4HL || psm == T4HH;
    uint64_t k = tex0 & 0x3ffffffffull;  // TBP0, TBW, PSM, TW, TH
    if (indexed) k |= tex0 & (0x1ffffffull << 37);  // CBP, CPSM, CSM, CSA
    const bool uses_texa = psm == CT24 || psm == CT16 || psm == CT16S || (indexed && cpsm != CT32);
    return {k, uses_texa ? texa : 0};
}

std::shared_ptr<const TexImage> LocalMemory::texture_at_kick(uint64_t tex0, uint64_t texa) {
    // (the snapshots' images bounded: the game uploads textures at addresses that keep changing, so the keys keep growing - over
    // minutes of play hundreds of MB, the Wii U's memory gone. Past the budget they are dropped, all at once: the images this
    // frame's primitives hold stay alive with them, the others are decoded again when met - the same pixels)
    constexpr size_t kSnapBudget = size_t{64} << 20;
    if (tex_snap_bytes_ > kSnapBudget) {
        tex_snaps_.clear();
        tex_snap_bytes_ = 0;
    }
    TexSnap& s = tex_snaps_[texture_key(tex0, texa)];
    if (s.img != nullptr) {
        if (s.gen == gen_) return s.img;
        if (!snap_stale(s)) {  // the memory it read is as it was: the same image
            s.gen = gen_;
            wr_open_ = false;  // (a change from here on is later than the snapshot)
            s.seq = wr_seq_;
            return s.img;
        }
    }
    const int w = 1 << std::min(10u, bits(tex0, 26, 4)), h = 1 << std::min(10u, bits(tex0, 30, 4));
    dec_.resize(static_cast<size_t>(w) * h);
    rd_track_ = true;
    for (int k = 0; k < 2; ++k) {
        rd_lo_[k] = 0xffffffffu;
        rd_hi_[k] = 0;
    }
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) dec_[static_cast<size_t>(y) * w + x] = texel(tex0, texa, x, y);
    rd_track_ = false;
    for (int k = 0; k < 2; ++k) {
        s.rd_lo[k] = rd_lo_[k];
        s.rd_hi[k] = rd_hi_[k];
    }
    s.gen = gen_;
    wr_open_ = false;
    s.seq = wr_seq_;
    // (the memory changed within the range of bytes it reads - its texels written back as they were, or bytes between them that it
    // does not read: the title's menus' textures, every frame - and the same pixels decoded: the image kept, its id with it, so that
    // the drawers' GPU textures made from it are kept too)
    if (s.img != nullptr && s.img->w == w && s.img->h == h && s.img->px == dec_) return s.img;
    auto img = std::make_shared<TexImage>();
    img->id = ++tex_ids_;
    img->w = w;
    img->h = h;
    img->px = dec_;
    if (s.img == nullptr) tex_snap_bytes_ += img->px.size() * 4;
    s.img = img;
    return img;
}

void LocalMemory::transfer_start() {
    xfer_ = true;
    xx_ = 0;
    xy_ = 0;
    xpend_.clear();
    wr_open_ = false;
}

void LocalMemory::set_transfer_state(const TransferState& t) {
    xfer_ = t.on;
    xx_ = t.xx;
    xy_ = t.xy;
    xpend_ = t.pend;
}

// (the pixels written in a row, their changes noted once: the same log entry the per-pixel notes made, nothing reads the log
// in between)
void LocalMemory::transfer_data(uint64_t bb, uint64_t pos, uint64_t rg, const uint8_t* p, uint32_t n) {
    if (!xfer_) return;
    const uint32_t dbp = bits(bb, 32, 14), dbw = bits(bb, 48, 6), dpsm = bits(bb, 56, 6);
    const uint32_t sx = bits(pos, 32, 11), sy = bits(pos, 48, 11);
    const uint32_t w = bits(rg, 0, 12), h = bits(rg, 32, 12);
    if (w == 0 || h == 0) return;
    uint32_t lo = 0xffffffffu, hi = 0;
    auto put = [&](uint32_t v) {
        if (xy_ >= h) return;
        uint32_t c = 0;
        const uint32_t a = store(dpsm, dbp, dbw, (sx + xx_) % 2048, (sy + xy_) % 2048, v, c);
        if (c != 0) {
            lo = std::min(lo, a);
            hi = std::max(hi, a + c);
        }
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
    if (hi > lo) note_change(lo, hi - lo);
    if (xy_ >= h) xfer_ = false;
}

void LocalMemory::undo_begin() {
    vundo_.clear();
    vundo_on_ = true;
}

void LocalMemory::undo_restore() {
    vundo_on_ = false;
    for (auto it = vundo_.rbegin(); it != vundo_.rend(); ++it) {
        std::memcpy(&vram_[it->a], &it->before, it->n);
        note_change(it->a, it->n);
    }
    vundo_.clear();
}

bool LocalMemory::changed_since(uint64_t seq, uint32_t lo, uint32_t hi) const {
    if (seq < wr_floor_) return true;
    for (size_t i = wr_log_.size(); i-- > 0;) {
        const WrRange& r = wr_log_[i];
        if (r.seq <= seq) break;
        if (r.lo < hi && r.hi > lo) return true;
    }
    return false;
}

std::shared_ptr<const TexImage> LocalMemory::picture_ct32(uint32_t bp, uint32_t bw, uint32_t w, uint32_t h) {
    const uint32_t lo = bp * 256, hi = lo + w * h * 4;
    if (pic_ != nullptr && pic_bp_ == bp && pic_bw_ == bw && pic_w_ == w && pic_h_ == h && !changed_since(pic_seq_, lo, hi)) return pic_;
    auto img = std::make_shared<TexImage>();
    img->id = ++tex_ids_;
    img->w = static_cast<int>(w);
    img->h = static_cast<int>(h);
    img->px.resize(static_cast<size_t>(w) * h);
    for (uint32_t y = 0; y < h; ++y) {
        uint32_t* row = img->px.data() + static_cast<size_t>(y) * w;
        for (uint32_t x = 0; x < w; ++x) row[x] = vram_r32(&vram_[addr32(bp, bw, x, y)]);
    }
    wr_open_ = false;
    pic_seq_ = wr_seq_;
    pic_bp_ = bp;
    pic_bw_ = bw;
    pic_w_ = w;
    pic_h_ = h;
    pic_ = img;
    return img;
}

size_t LocalMemory::clear_caches() {
    const size_t n = tex_snaps_.size();
    tex_snaps_.clear();
    tex_snap_bytes_ = 0;
    pic_.reset();
    pic_bp_ = ~0u;
    return n;
}

LocalMemory& local_memory() {
    static LocalMemory m;
    return m;
}

namespace {
// (a fixed table: registered from the main thread at a walker's first start, read from any thread: the game thread's libgraph)
std::array<std::atomic<MemoryJoin>, 4> g_joins{};
}  // namespace

void add_memory_join(MemoryJoin join) {
    for (auto& j : g_joins) {
        MemoryJoin cur = j.load(std::memory_order_acquire);
        if (cur == join) return;
        if (cur == nullptr && j.compare_exchange_strong(cur, join, std::memory_order_acq_rel)) return;
        if (cur == join) return;
    }
}

namespace {
StoreImageHook g_store_image_hook = nullptr;
}  // namespace

void set_store_image_hook(StoreImageHook hook) { g_store_image_hook = hook; }
StoreImageHook store_image_hook() { return g_store_image_hook; }

void memory_join() {
    for (auto& j : g_joins)
        if (MemoryJoin f = j.load(std::memory_order_acquire); f != nullptr) f();
}

}  // namespace rac1::gs
