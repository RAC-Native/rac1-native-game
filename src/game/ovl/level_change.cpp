// Ported overlay code: the level change request 0x24d430 (the level's state written into the
// save game before another level is loaded: the clock, the level's talkers' positions, the
// planet map's revealed area, the save slot's record and its two blocks with their checksums,
// the memory card save asked for) and its callees; the level exit 0x292e08 (the overlay's main
// loop left: the next level made current, its header and files read, back to the base ELF's
// overlay load).
#include <game/lib/libcdvd.h>
#include <host/level_slots.h>
#include <game/ovl/ovl_reloc.h>
#include <game/math/vecmath.h>
#include <game/ovl/level_change.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/planet_map.h>
#include <game/port_stubs.h>

namespace rac1 {

// The save slots' block 0x13d290: the slots (0xb8 bytes each, +0x14 the slot's state), +0xc0
// the slot in use (-1: none), +0xc8 the level saved, +0xd4 the memory card's state, +0xdc the
// card operation asked for (-1: none), +0xe0 its slot, +0xf4 a save pending.
namespace {
constexpr uint32_t SAVE = 0x0013d290;
}  // namespace

// The checksum of `n` bytes at `p` (0 when longer than 0x1800): a 16-bit CRC (polynomial
// 0x1f45, the register starting at 0xedb88320, the bytes shifted in from bit 8).
// orig: 0x0024cf78
uint32_t save_checksum(uint32_t p, int32_t n) {
    if (!(n < 0x1801)) return 0;
    const uint32_t end = p + static_cast<uint32_t>(n);
    uint32_t c = 0xedb88320u;
    while (p < end) {
        c ^= static_cast<uint32_t>(lbu(p)) << 8;
        ++p;
        for (int32_t k = 7; k >= 0; --k) c = (c & 0x8000) != 0 ? (c << 1) ^ 0x1f45 : c << 1;
    }
    return c & 0xffff;
}

// A save block at `dst`: for each entry of the list `list` (16 bytes: source, size, id; 0 ends
// it) a record (id, size, then `size` bytes from source + index * size, 4-byte aligned); then a
// (-1, 0) record; the block's length (from +8) at +0 and its checksum at +4. Returns the length
// + 8.
// orig: 0x0024d030
int32_t save_block_write(uint32_t dst, int32_t index, uint32_t list) {
    int32_t len = 0;
    uint32_t out = dst + 8;
    if (lw(list) != 0) {
        uint32_t e = list;
        do {
            len += 8;
            const uint32_t src = lwu(e);
            const int32_t size = lw(e + 4);
            sw(out, lwu(e + 8));
            sw(out + 4, lwu(e + 4));
            out += 8;
            if (!level_slots_save_record(list, index, e, out))  // enhancement: the base level's records while a level mod's are in memory
                mem_copy(out, src + static_cast<uint32_t>(index * size), lw(e + 4));
            const uint32_t n = lwu(e + 4);
            e += 0x10;
            out += n;
            len += static_cast<int32_t>(n);
            out = (out + 3) & ~3u;
            len = (len + 3) & ~3;
        } while (lw(e) != 0);
    }
    len += 8;
    sw(out + 4, 0);
    sw(out, 0xffffffffu);
    const uint32_t sum = save_checksum(dst + 8, len);
    sw(dst + 4, sum);
    sw(dst, static_cast<uint32_t>(len));
    return len + 8;
}

// The positions of the level's talkers (the level's range of the table 0x1c44b8) kept in
// 0x13d5b0 (16 bytes each: x, y and +0x48 of the moby 0x1791b8[i]; skipped when it is gone).
// orig: 0x0024ab50
void talkers_save() {
    const uint32_t lv = lwu(0x0015ed84);
    if (!(lv < 0x13)) return;
    const int32_t last = lw(OV(0x001c44b8) + (lv + 1) * 4);
    int32_t first = lw(OV(0x001c44b8) + lv * 4);
    if (!(first < last)) return;
    uint32_t out = static_cast<uint32_t>(first) * 16 + 0x0013d5b0;
    uint32_t m = static_cast<uint32_t>(first) * 4 + OV(0x001791b8);
    for (int32_t n = last - first; n != 0; --n, m += 4) {
        const uint32_t moby = lwu(m);
        if (moby == 0) {
            out += 0x10;
            continue;
        }
        swc1(out, lwc1(moby + 0x10));
        swc1(out + 4, lwc1(moby + 0x14));
        swc1(out + 8, lwc1(moby + 0x48));
        out += 0x10;
    }
}

// The planet map's revealed area run-length coded into `dst` (at most `size` bytes): the bits of
// the bitmap `bits` (512 x 512) taken along the spans `spans` (skip, count byte pairs, up to bit
// 0x40000), each byte the length of a run of equal bits (at most 0x7f for the first, 0xff after;
// a full run is followed by an empty one); the first byte doubled + 1 at the end. Returns the
// length, or -1 when it does not fit.
// orig: 0x00200418
int32_t map_runs_encode(uint32_t dst, int32_t size, uint32_t spans, uint32_t bits) {
    const uint32_t end = dst + static_cast<uint32_t>(size);
    const uint32_t start = dst;
    uint32_t limit = 0x7f;
    uint32_t pos = 0, cur = 1, run = 0, left = 0;
    for (;;) {
        if (left == 0) {
            const uint32_t skip = lbu(spans);
            left = lbu(spans + 1);
            pos += skip;
            spans += 2;
            if (pos == 0x40000) break;
            continue;
        }
        const uint32_t b = (lbu((pos >> 3) + bits) >> (pos & 7)) & 1;
        ++pos;
        --left;
        if (cur != b) {
            cur = b;
            sb(dst, run);
            ++dst;
            limit = 0xff;
            if (dst == end) return -1;
            run = 1;
            continue;
        }
        cur = b;
        if (run != limit) {
            ++run;
            continue;
        }
        sb(dst, run);
        dst += 2;
        run = 1;
        limit = 0xff;
        if (dst == end) return -1;
        sb(dst - 1, 0);
    }
    if (run != 0) {
        sb(dst, run);
        ++dst;
    }
    sb(start, static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(start))) * 2 + 1));
    return static_cast<int32_t>(dst - start);
}

// The planet map's revealed area when its runs do not fit: the 512 x 512 map *(0x1841fc), 4 bits
// a pixel, reduced to 128 x 128 bits into `dst` (0x800 bytes): each 4 x 4 block's pixels summed
// through the weights 0x1e8b30 (16 words), a bit set where the sum is 8 or more; the first byte's
// low bits then 1 0 (the reduced form).
// orig: 0x0024a3c8
void map_area_reduce(uint32_t dst) {
    uint32_t sums[128] = {};
    uint32_t weight[16];
    uint32_t src = lwu(OV(0x001841fc));
    for (uint32_t i = 0; i < 16; ++i) weight[i] = lwu(OV(0x001e8b30) + i * 4);
    uint32_t out = dst;
    for (int32_t row = 0; row < 0x200; ++row) {
        if ((row & 3) == 0)
            for (uint32_t& s : sums) s = 0;
        for (uint32_t i = 0; i < 128; i += 2) {
            const uint32_t v = lbu(src);
            ++src;
            sums[i] += weight[v & 0xf];
            sums[i + 1] += weight[v >> 4];
        }
        if ((row & 3) != 3) continue;
        for (uint32_t i = 0; i < 128; ++i) sums[i] = static_cast<int32_t>(sums[i]) < 8 ? 0u : 1u << (i & 7);
        for (uint32_t g = 0; g < 128; g += 8) {
            uint32_t v = 0;
            for (uint32_t k = 0; k < 8; ++k) v |= sums[g + k] & 0xff;
            sb(out, v);
            ++out;
        }
    }
    sb(dst, (lbu(dst) & 0xfe) | 2);
}

// The planet map's revealed area saved into `dst` (0x800 bytes, the level's block of 0x141ec0):
// cleared when the map has none (*(0x184218) 0), else run-length coded (0x200418), reduced when
// that does not fit; the longest code of the level kept in 0x13d560[level].
// orig: 0x00249ea0
void map_area_save(uint32_t dst) {
    map_nop();
    constexpr OvConst<0x001841f0u> MAP{};
    if (lw(MAP + 0x28) == 0) {
        mem_fill32(dst, 0, 0x800);
        return;
    }
    const int32_t n = map_runs_encode(dst, 0x800, lwu(MAP + 0x14), lwu(MAP + 0xc));
    if (n == -1) map_area_reduce(dst);
    const uint32_t best = 0x0013d560 + lwu(0x0015ed84) * 4;
    if (lw(best) < n) sw(best, static_cast<uint32_t>(n));
}

// The level change request: the clock read, the level's talkers and map area kept; with a save
// slot in use (and its slot valid) a save made pending (`force`, or the flag 0x200 of
// 0x15eeb4), and when the card is idle: the slot's record (bolts, level, the clock, the play
// time) and the two save blocks written for `level` (the visited flag and the current level
// left as they were), the card asked to save (+0xdc 0xf). True when that save is asked for
// (without a slot: when not forced).
// orig: 0x0024d430
bool level_change_request(int32_t force, int32_t level) {
    cd_read_clock(0x0015ee98);
    clock_to_local(0x0015ee98);
    talkers_save();
    map_area_save(0x00141ec0 + (lwu(0x0015ed84) << 11));
    const int32_t slot = lw(SAVE + 0xc0);
    if (slot == -1 || lw(SAVE + static_cast<uint32_t>(slot) * 0xb8 + 0x14) < 0) return force == 0;
    const uint32_t pending = lwu(SAVE + 0xf4) | static_cast<uint32_t>(force);
    sw(SAVE + 0xf4, pending);
    if (pending == 0) return lw(SAVE + 0xdc) == 0xf;
    if (force == 0) sw(0x0015eeb4, lwu(0x0015eeb4) | 0x200);
    if (!(lw(SAVE + 0xd4) < 3)) return lw(SAVE + 0xdc) == 0xf;
    if (lw(SAVE + 0xdc) >= 0) return lw(SAVE + 0xdc) == 0xf;
    sw(SAVE + 0xc8, lwu(0x0015ed84));
    uint32_t visited = 0;
    if (level >= 0) {
        sw(0x0015ed84, static_cast<uint32_t>(level));
        visited = lbu(0x0013dd58 + static_cast<uint32_t>(level));
        if (visited == 0) sb(0x0013dd58 + static_cast<uint32_t>(level), 1);
    }
    const uint32_t rec = SAVE + lwu(SAVE + 0x14) * 0x1c;
    sw(rec + 0x24, lwu(0x0015ed98));
    sw(SAVE + lwu(SAVE + 0x14) * 0x1c + 0x20, lwu(0x0015ed84));
    sw(SAVE + lwu(SAVE + 0x14) * 0x1c + 0x2c, lwu(0x0015ee24));
    sd(SAVE + 0x30 + lwu(SAVE + 0x14) * 0x1c, ld(0x0015ee98));
    sw(SAVE + lwu(SAVE + 0x14) * 0x1c + 0x28, lwu(0x0015ee20));
    save_block_write(0x0014eed0, 0, OV(0x001845c0));
    save_block_write(0x001506d0, lw(SAVE + 0xc8), OV(0x001848c0));
    if (level >= 0) {
        sb(0x0013dd58 + lwu(0x0015ed84), visited);
        sw(0x0015ed84, lwu(SAVE + 0xc8));
    }
    if (lw(SAVE + 0xdc) < 0) {
        sw(SAVE + 0xdc, 0xf);
        sw(SAVE + 0xe0, lwu(SAVE + 0xc0));
    }
    return lw(SAVE + 0xdc) == 0xf;
}

}  // namespace rac1
