// Ported overlay code: the level setup (0x244110) and the loaders it calls.
#include <game/enhance/advanced.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/lib/libgraph.h>
#include <game/math/vecmath.h>
#include <game/port_stubs.h>

#include <cstdlib>

namespace rac1 {

// The level memory map: the core header buffer and the area base at 0x2f4000 (0x173e04 /
// 0x173e00), then *(0x1611cc) bytes twice (0x173e08, 0x173e0c), 0x64000 (0x173e10) and 0x30000
// more: the core data / allocation start 0x173e14 / 0x173e18; 0x173e20.. the VU / SPR windows.
// orig: 0x0023d520
void memory_layout() {
    // (the image's end symbol 0x2f4ccf rounded down to 16 KB: relocated, each image ends elsewhere)
    const uint32_t a = OV(0x002f4ccfu) & 0xffffc000u, n = lwu(OV(0x001611cc));
    const uint32_t b = a + n, c = b + n, d = c + 0x64000, e = d + 0x30000;
    sw(OV(0x00173e28), 0x07200000);
    sw(OV(0x00173e00), a);
    sw(OV(0x00173e18), e);
    sw(OV(0x00173e20), 0x07000000);
    sw(OV(0x00173e24), 0x07100000);
    sw(OV(0x00173e04), a);
    sw(OV(0x00173e08), b);
    sw(OV(0x00173e0c), c);
    sw(OV(0x00173e10), d);
    sw(OV(0x00173e14), e);
}

// Four words of the core (the fog / light block): 0x15fc98, 0x15fc9c and the floats 0x15fca0,
// 0x15fca4; *(0x15fca8) points past them.
// orig: 0x0023f0d8
void sub_0023f0d8(uint32_t p) {
    sw(OV(0x0015fc98), lwu(p));
    sw(OV(0x0015fc9c), lwu(p + 4));
    sw(OV(0x0015fca0), lwu(p + 8));
    sw(OV(0x0015fca4), lwu(p + 0xc));
    sw(OV(0x0015fca8), p + 0x10);
}

// A core list (count, then from +0x10 count 32-byte records and a table after them): the count
// to 0x15f438, the records to 0x15f43c, the table to 0x15f440; each table word is relocated
// (offset from the table -> address), then the word +4 of the record it points at.
// orig: 0x0023f120
void sub_0023f120(uint32_t p) {
    const int32_t n = lw(p);
    sw(OV(0x0015f43c), p + 0x10);
    sw(OV(0x0015f438), static_cast<uint32_t>(n));
    sw(OV(0x0015f440), p + 0x10 + static_cast<uint32_t>(n) * 32);
    for (int32_t i = 0; i < lw(OV(0x0015f438)); ++i) {
        const uint32_t t = lwu(OV(0x0015f440));
        const uint32_t a = static_cast<uint32_t>(i) * 4 + t;
        sw(a, lwu(a) + t);
    }
    for (int32_t i = 0; i < lw(OV(0x0015f438)); ++i) {
        const uint32_t t = lwu(OV(0x0015f440));
        const uint32_t r = lwu(static_cast<uint32_t>(i) * 4 + t);
        sw(r + 4, lwu(r + 4) + t);
    }
}

// The texture-page list: count words from +0x10 become addresses in the 0x1b2280 copy of the
// data block (+8 offset, +0xc size; 0 -> the block start) at 0x1b2080; the block is copied to
// 0x1b2280; then n entries of (4 words) at `list` give two GS addresses each, (base + w0) * 16 +
// w1 and (base + w2) * 16 + log2(w3), at 0x1b1a80 + i * 8; the count goes to 0x160180.
// orig: 0x0023f630
void sub_0023f630(uint32_t p, int32_t base, uint32_t list, int32_t n) {
    int32_t c = lw(p);
    const int32_t off = lw(p + 8), size = lw(p + 0xc);
    uint32_t src = p + 0x10, dst = OV(0x001b2080);
    const uint32_t delta = static_cast<uint32_t>(off) - OV(0x001b2280u);
    while (c > 0) {
        const uint32_t v = lwu(src);
        sw(dst, v == 0 ? OV(0x001b2280u) : v - delta);
        dst += 4;
        src += 4;
        --c;
    }
    mem_copy(OV(0x001b2280), p + static_cast<uint32_t>(off), size);
    sw(OV(0x00160180), 0);
    if (n <= 0) return;
    int32_t i = 0;
    do {
        const int32_t w0 = lw(list), w1 = lw(list + 4), w2 = lw(list + 8), w3 = lw(list + 0xc);
        list += 0x10;
        const uint32_t k = lwu(OV(0x00160180));
        sw(OV(0x001b1a80) + k * 8, (static_cast<uint32_t>(base) + static_cast<uint32_t>(w0)) * 16 + static_cast<uint32_t>(w1));
        const int32_t b = highest_bit_index(w3);
        const uint32_t k2 = lwu(OV(0x00160180));
        sw(OV(0x001b1a84) + k2 * 8, (static_cast<uint32_t>(base) + static_cast<uint32_t>(w2)) * 16 + static_cast<uint32_t>(b));
        i = static_cast<int32_t>(k2) + 1;
        sw(OV(0x00160180), static_cast<uint32_t>(i));
    } while (i < n);
}

// n 16-byte entries (x, y, a, b) -> 0x16d180 + i * 16: +8 / +0xa the coordinates >> 4, +0xc /
// +0xe log2 of a / b, +0 cleared; the count at 0x15f41c.
// orig: 0x0023f768
void sub_0023f768(uint32_t p, int32_t n) {
    sw(OV(0x0015f41c), 0);
    if (n <= 0) return;
    int32_t i;
    do {
        const int32_t x = lw(p), y = lw(p + 4), a = lw(p + 8), b = lw(p + 0xc);
        p += 0x10;
        const uint32_t e = lwu(OV(0x0015f41c)) * 16 + OV(0x0016d180);
        sh(e + 0xa, static_cast<uint32_t>(x >> 4));
        sh(e + 8, static_cast<uint32_t>(y >> 4));
        sh(e + 0xc, static_cast<uint32_t>(highest_bit_index(a)));
        const uint32_t e2 = lwu(OV(0x0015f41c)) * 16 + OV(0x0016d180);
        sh(e2 + 0xe, static_cast<uint32_t>(highest_bit_index(b)));
        i = lw(OV(0x0015f41c)) + 1;
        sd(e2, 0);
        sw(OV(0x0015f41c), static_cast<uint32_t>(i));
    } while (i < n);
}

// The class's update function and its +0x2c word from the class table 0x1ea300 (12-byte rows:
// class, function, word; -1 ends it), for the class being registered (index *(0x15ffc0)): the
// function to 0x197680[index], the word to the class header +0x2c when the class has one.
// orig: 0x00264688
void class_update_fn_set(int32_t cls) {
    const uint32_t idx = lwu(OV(0x0015ffc0));
    const uint32_t hdr = lwu(OV(0x00197300) + idx * 4);
    uint32_t k = 0;
    int32_t c = lw(OV(0x001ea300));
    if (c != -1 && c != cls) {
        uint32_t row = OV(0x001ea30c);
        for (;;) {
            c = lw(row);
            ++k;
            if (c == -1 || c == cls) break;
            row += 0xc;
        }
    }
    const uint32_t row = OV(0x001ea300) + k * 0xc;
    sw(OV(0x00197680) + idx * 4, lwu(row + 4));
    if (hdr != 0) sw(lwu(OV(0x00197300) + idx * 4) + 0x2c, lwu(row + 8));
}

// Register a moby class: the next class index *(0x15ffc0) is given to class `cls` (0x197bc0[cls],
// 0x197a00[index] = cls), its header goes to 0x197300[index] (0: a class without data), its +0x2c
// word to 0x19a280[index]; the update function is set and the header relocated (0x2406b0).
// orig: 0x002409b8
void moby_class_register(uint32_t hdr, uint32_t tex, uint32_t remap, uint32_t cls) {
    const uint32_t idx = lwu(OV(0x0015ffc0));
    sb(OV(0x00197bc0) + cls, lbu(OV(0x0015ffc0)));
    sh(OV(0x00197a00) + idx * 2, cls);
    sw(OV(0x00197300) + idx * 4, hdr);
    if (hdr == 0) {
        class_update_fn_set(static_cast<int32_t>(cls));
        sw(OV(0x0015ffc0), lwu(OV(0x0015ffc0)) + 1);
        return;
    }
    sw(OV(0x0019a280) + idx * 4, lwu(hdr + 0x2c));
    class_update_fn_set(static_cast<int32_t>(cls));
    sw(OV(0x0015ffc0), lwu(OV(0x0015ffc0)) + 1);
    moby_class_relocate(hdr, tex, remap, cls);
}

// The collision block: the tree (+0 offset) to 0x173e40; the list at +4 (count, then 16-byte
// records whose +0x1c word is relocated) to 0x15f618.
// orig: 0x00241728
void level_collision_set(uint32_t p) {
    if (lw(p) != 0) sw(OV(0x00173e40), p + lwu(p));
    if (lw(p + 4) == 0) return;
    const uint32_t l = p + lwu(p + 4);
    sw(OV(0x0015f618), l);
    if (lw(l) <= 0) return;
    uint32_t e = l + 0x1c;
    for (int32_t i = 0; i < lw(l); ++i, e += 0x10) sw(e, l + lwu(e));
}

// (empty)
// orig: 0x00241938
void sub_00241938(uint32_t, int32_t) {}

// The level moby of unique id `uid` (+0xb2), 0 when none. (`unused`: the caller's second id.)
// orig: 0x002851c0
uint32_t moby_find_uid(int32_t unused, int32_t uid) {
    (void)unused;
    const uint32_t end = lwu(OV(0x0015ffdc));
    for (uint32_t m = lwu(OV(0x0015ffd8)); m != end; m += 0x100) {
        if (lh(m + 0xb2) == uid) return m;
    }
    return 0;
}

// The moby data saved in the list `list` (a count, then 0x10-byte entries: +0 the offset, +8
// the size, +0xa 1: in the moby (else in its pvars), +0xc the moby's id; the bytes from +4)
// copied back into the mobys still there.
// orig: 0x00285208
void moby_data_restore(uint32_t list) {
    uint32_t e = list + 4, src = list + 8;
    for (int32_t i = 0; i < lw(list); ++i, e += 0x10, src += 0x10) {
        const uint32_t m = moby_find_uid(lh(e + 0xe), lh(e + 0xc));
        if (m == 0) continue;
        const uint32_t dst = lh(e + 0xa) == 1 ? lwu(e) + m : lwu(e) + lwu(m + 0x78);
        mem_copy(dst, src, lhu(e + 8));
    }
}

// The checkpoint's moby data (0x1bb280) restored.
// orig: 0x002852c0
void checkpoint_mobys_restore() { moby_data_restore(OV(0x001bb280)); }

// Back at the checkpoint (0x1bb230, when one was taken): the level state 0x1ba5d0 from it, the
// hero's position, angles and moby fields, the HUD's state bits and values, the music track;
// the vehicle the hero was in (+0x44: 1 class 0x57, 2 class 0x1a3) entered again; the hero's
// matrix, the first camera, the saved moby data. Without one, the level state is cleared.
// orig: 0x00285008
void checkpoint_restore() {
    const uint32_t C = OV(0x001bb230), P = 0x0013f350, HUD = 0x0013e550;
    if (lw(C) == 0) {
        mem_fill32(OV(0x001ba5d0), 0, 0xc60);
        return;
    }
    mem_copy(OV(0x001ba5d0), C, 0xc60);
    lq_sq(0x0013f3d0, C + 0x10);
    lq_sq(0x0013f3e0, C + 0x20);
    sw(lwu(P + 0x2080) + 0x38, lwu(C + 0x30));
    sw(lwu(P + 0x2080) + 0x3c, lwu(C + 0x34));
    sw(lwu(P + 0x2080) + 0x80, lwu(C + 0x38));
    const int32_t vehicle = lw(C + 0x44);
    sb(HUD + 0x6b, lbu(HUD + 0x6b) | 7u);
    sh(0x00151708, lhu(C + 0xc54));
    sw(HUD + 0x64, lwu(C + 0x3c));
    sb(HUD + 0x68, lbu(C + 0x40));
    sb(HUD + 0x69, lbu(C + 0x41));
    sb(HUD + 0x6a, lbu(C + 0x42));
    if (vehicle != 0 && vehicle != 3) {
        int32_t cls = 0;
        if (vehicle == 1) cls = 0x57;
        else if (vehicle == 2) cls = 0x1a3;
        else debug_trap();
        const uint32_t first = lwu(OV(0x0015ffd8)), end = lwu(OV(0x0015ffdc));
        for (uint32_t k = 0;; ++k) {
            const uint32_t m = first + (k << 8);
            if (!(m < end)) break;
            if (lh(m + 0xa6) == cls) {
                hero_mode_enter(lwu(C + 0x44), lwu(C + 0x4c), m);
                break;
            }
        }
    }
    moby_update_transform(lwu(P + 0x2080));
    euler_to_mat3_vu(*static_cast<Mat4*>(ee_ram_ptr(lwu(P + 0x2080) + 0xc0)), V(P + 0x90));
    camera_select_first();
    checkpoint_mobys_restore();
}

// Clears 0x1ba5d0 and 0x1bb230 (0xc60 bytes each) and 0x1ba4d0 (0x100).
// orig: 0x00284e00
void sub_00284e00() {
    mem_fill32(OV(0x001ba5d0), 0, 0xc60);
    mem_fill32(OV(0x001bb230), 0, 0xc60);
    mem_fill32(OV(0x001ba4d0), 0, 0x100);
}

namespace {
int64_t sxw(uint32_t v) { return static_cast<int64_t>(static_cast<int32_t>(v)); }  // orig: (helper) lw sign extension
}  // namespace

// Register a tie class: the next tie index *(0x160fbc) for class `cls` (0x1c5a00[index] = cls,
// 0x1c5b00[cls] = index, 0x1c5800[index] = header, header +0x46 = index, 0x1c6f00[index] =
// int(header +0x48 * 1024)); header +0x28 / +0x26 cleared; the three packet lists (+0 / +4 / +8,
// counts +0x20..+0x22, 16-byte entries whose first word is an offset) and +0xc / +0x2c
// relocated; the palette `remap` (16 bytes) to 0x1c7700 + index * 16; then each of the
// header +0x23 texture packets (0x50 bytes from +0x2c) gets its GS registers: from the
// texture table `tex` (16 bytes per texture) when given, else from the defaults at 0x182640.
// orig: 0x00240aa8
void tie_class_register(uint32_t hdr, uint32_t tex, uint32_t remap, uint32_t cls) {
    const uint32_t idx = lwu(OV(0x00160fbc));
    sh(OV(0x001c5a00) + idx * 2, cls);
    sb(OV(0x001c5b00) + cls, lbu(OV(0x00160fbc)));
    sw(OV(0x001c5800) + idx * 4, hdr);
    sh(hdr + 0x46, lhu(OV(0x00160fbc)));
    const int32_t r = float_to_int(lwc1(hdr + 0x48) * 1024.0f);
    const uint32_t k = lwu(OV(0x00160fbc));
    sw(OV(0x00160fbc), k + 1);
    sw(OV(0x001c6f00) + k * 4, static_cast<uint32_t>(r));
    sw(hdr + 0x28, 0);
    sh(hdr + 0x26, 0);
    for (uint32_t i = 0; i < 3; ++i) {
        const uint32_t a = hdr + i * 4;
        if (lw(a) == 0) continue;
        sw(a, lwu(a) + hdr);
        const uint32_t cnt = hdr + 0x20 + i;
        if (lbu(cnt) == 0) continue;
        for (uint32_t j = 0; j < lbu(cnt); ++j) {
            const uint32_t l = lwu(a);
            const uint32_t e = j * 16 + l;
            sw(e, lwu(e) + l);
        }
    }
    sw(hdr + 0xc, lwu(hdr + 0xc) + hdr);
    const uint32_t pk0 = lwu(hdr + 0x2c) + hdr;
    sw(hdr + 0x2c, pk0);
    const uint32_t pal = static_cast<uint32_t>(lbu(OV(0x001c5b00) + cls)) * 16 + OV(0x001c7700);
    lq_sq(pal, remap);
    uint32_t pk = pk0;
    for (uint32_t t = 0; t < lbu(hdr + 0x23); ++t, pk += 0x50) {
        const int64_t w10 = sxw(lwu(pk + 0x10)), w14 = sxw(lwu(pk + 0x14)), w30 = sxw(lwu(pk + 0x30)), w34 = sxw(lwu(pk + 0x34));
        const uint32_t ti = lbu(pal + t);
        const uint64_t lo = static_cast<uint64_t>(w14) << 6 | 0x20u;
        const uint64_t w3 = static_cast<uint64_t>(w30) | static_cast<uint64_t>(w34) << 2 | static_cast<uint64_t>(ti) << 24;
        if (tex != 0) {
            const uint32_t e = tex + ti * 16;
            const int32_t hw = static_cast<int32_t>(static_cast<uint32_t>(lhu(e + 4)) << 16);
            int64_t a = hw >> 22, b = hw >> 23;
            if (!(0 < a)) a = 1;
            if (!(0 < b)) b = 1;
            const int64_t lw_ = highest_bit_index(hw >> 16);
            const int64_t lh_ = highest_bit_index(lh(e + 6));
            const int32_t vb = lw(0x0015ee8c) >> 8;
            const int64_t f = sxw(static_cast<uint32_t>(lh(e + 0xe) + vb)), c = sxw(static_cast<uint32_t>(lh(e + 0xc) + vb));
            const int64_t d = sxw(static_cast<uint32_t>(lh(e + 0xa) + vb));
            const uint64_t q0 = static_cast<uint64_t>(a) << 14 | static_cast<uint64_t>(lw_) << 26 | 0x1300000u | static_cast<uint64_t>(lh_) << 30 |
                                static_cast<uint64_t>(d) << 37 | 0x400000000ull | 0x8000000000000000ull;
            const uint64_t q1 = static_cast<uint64_t>(sxw(static_cast<uint32_t>(lh(e + 8) - 1))) << 2 | lo | static_cast<uint64_t>(w10) << 32;
            const uint64_t q2 = static_cast<uint64_t>(b) << 14 | static_cast<uint64_t>(c) << 20 | static_cast<uint64_t>(f) << 40 | 0x400000000ull |
                                0x40000000000000ull;
            sd(pk, q0);
            sd(pk + 0x10, q1);
            sd(pk + 0x20, q2);
            sd(pk + 0x30, w3);
        } else {
            const uint32_t g = OV(0x00182640) + ti * 24;
            sd(pk, ld(g));
            sd(pk + 0x10, (ld(g + 8) & 0x1c) | lo | static_cast<uint64_t>(w10) << 32);
            sd(pk + 0x20, ld(g + 16));
            sd(pk + 0x30, w3);
        }
        sd(pk + 0x40, 0);
    }
}

// The GS registers of a texture packet from the texture table entry `e` (16 bytes): returns
// the TEX0-like word (a) and the second word (b) of the pair the shrub / tie packets take.
// orig: (helper) the identical sequence in 0x240e80 / 0x240aa8 / 0x241458
namespace {
struct TexWords { uint64_t q0, q2; };
TexWords tex_words(uint32_t e) {
    const int32_t hw = static_cast<int32_t>(static_cast<uint32_t>(lhu(e + 4)) << 16);
    int64_t a = hw >> 22, b = hw >> 23;
    if (!(0 < a)) a = 1;
    if (!(0 < b)) b = 1;
    const int64_t lw_ = highest_bit_index(hw >> 16);
    const int64_t lh_ = highest_bit_index(lh(e + 6));
    const int32_t vb = lw(0x0015ee8c) >> 8;
    const int64_t f = sxw(static_cast<uint32_t>(lh(e + 0xe) + vb)), c = sxw(static_cast<uint32_t>(lh(e + 0xc) + vb));
    const int64_t d = sxw(static_cast<uint32_t>(lh(e + 0xa) + vb));
    TexWords t;
    t.q0 = static_cast<uint64_t>(a) << 14 | static_cast<uint64_t>(lw_) << 26 | 0x1300000u | static_cast<uint64_t>(lh_) << 30 |
           static_cast<uint64_t>(d) << 37 | 0x400000000ull | 0x8000000000000000ull;
    t.q2 = static_cast<uint64_t>(b) << 14 | static_cast<uint64_t>(c) << 20 | static_cast<uint64_t>(f) << 40 | 0x400000000ull | 0x40000000000000ull;
    return t;
}
}  // namespace

// Register a shrub class: the next shrub index *(0x16048c) for class `cls` (0x1bc1b0[cls] =
// index, 0x1bc130[index] = cls, 0x1bc030[index] = header, header +0x26 = index, 0x1bcdb0[index]
// = int(header +0x10 * 1024)); header +0x18 / +0x16 cleared, +0x2c and the +0x28 words at +0x40
// (stride 8) relocated; the billboard packet +0x1c (relocated) gets its GS words from `def`
// (a texture record) when given, else from the first free row of 0x182640; the palette
// `remap` to 0x1bd3b0 + index * 16; each model's texture packets (0x40 bytes) get theirs from
// `tex` when given, else from the defaults 0x182640.
// orig: 0x00240e80
void shrub_class_register(uint32_t hdr, uint32_t tex, uint32_t remap, uint32_t def, uint32_t cls) {
    const uint32_t idx = lwu(OV(0x0016048c));
    sb(OV(0x001bc1b0) + cls, lbu(OV(0x0016048c)));
    sh(OV(0x001bc130) + idx * 2, cls);
    sw(OV(0x001bc030) + idx * 4, hdr);
    sh(hdr + 0x26, lhu(OV(0x0016048c)));
    const int32_t r = float_to_int(lwc1(hdr + 0x10) * 1024.0f);
    const uint32_t k = lwu(OV(0x0016048c));
    sw(OV(0x0016048c), k + 1);
    sw(OV(0x001bcdb0) + k * 4, static_cast<uint32_t>(r));
    sw(hdr + 0x18, 0);
    sh(hdr + 0x16, 0);
    if (lw(hdr + 0x2c) != 0) sw(hdr + 0x2c, lwu(hdr + 0x2c) + hdr);
    for (int32_t i = 0; i < lh(hdr + 0x28); ++i) {
        const uint32_t a = hdr + 0x40 + static_cast<uint32_t>(i) * 8;
        sw(a, lwu(a) + hdr);
    }
    if (lw(hdr + 0x1c) != 0) {
        const uint32_t b = lwu(hdr + 0x1c) + hdr;
        sw(hdr + 0x1c, b);
        const int64_t w10 = sxw(lwu(b + 0x10)), w14 = sxw(lwu(b + 0x14));
        const uint64_t lo = static_cast<uint64_t>(w14) << 6 | 0x20u;
        if (def != 0) {
            const int32_t h0 = lh(def);
            int64_t sh_[4];
            for (int32_t i = 0; i < 4; ++i) {
                const int32_t v = h0 >> ((i + 6) & 31);
                sh_[i] = 0 < v ? v : 1;
            }
            const int64_t b0 = highest_bit_index(lh(def));
            const int64_t b1 = highest_bit_index(lh(def + 2));
            sd(b + 0x10, static_cast<uint64_t>(sxw(static_cast<uint32_t>(lh(def + 4) - 1))) << 2 | lo | static_cast<uint64_t>(w10) << 32);
            const int32_t vb = lw(0x0015ee8c) >> 8;
            const auto hv = [def, vb](uint32_t o) { return static_cast<uint64_t>(sxw(static_cast<uint32_t>(lh(def + o) + vb))); };
            sd(b + 0x20, hv(8) | static_cast<uint64_t>(sh_[0]) << 14 | static_cast<uint64_t>(b0) << 26 | 0x1300000u | static_cast<uint64_t>(b1) << 30 |
                             hv(6) << 37 | 0x400000000ull | 0x8000000000000000ull);
            sd(b + 0x30, hv(0xa) | static_cast<uint64_t>(sh_[1]) << 14 | hv(0xc) << 20 | static_cast<uint64_t>(sh_[2]) << 34 | hv(0xe) << 40 |
                             static_cast<uint64_t>(sh_[3]) << 54);
        } else {
            uint32_t row = 0;
            if (ld(OV(0x00182658)) != 0) {
                uint32_t a = OV(0x00182658);
                do {
                    a += 0x18;
                    ++row;
                } while (ld(a) != 0);
            }
            const uint32_t g = OV(0x00182640) + row * 0x18;
            sd(b + 0x20, ld(g));
            sd(b + 0x30, ld(g + 16));
            sd(b + 0x10, (ld(g + 8) & 0x1c) | lo | static_cast<uint64_t>(w10) << 32);
        }
    }
    const uint32_t pal = static_cast<uint32_t>(lbu(OV(0x001bc1b0) + cls)) * 16 + OV(0x001bd3b0);
    lq_sq(pal, remap);
    for (int32_t i = 0; i < lh(hdr + 0x28); ++i) {
        const uint32_t m = lwu(hdr + static_cast<uint32_t>(i) * 8 + 0x40);
        const uint32_t t5 = m + 0x10;
        if (lw(m + 0x10) <= 0) continue;
        uint32_t pk = t5 + lwu(t5 + 4) * 16 + 0x10;
        for (int32_t t = 0; t < lw(t5); ++t, pk += 0x40) {
            const uint32_t ti = lbu(pal + lwu(pk + 0x20));
            const int64_t w0 = sxw(lwu(pk)), w4 = sxw(lwu(pk + 4)), w10 = sxw(lwu(pk + 0x10)), w14 = sxw(lwu(pk + 0x14));
            const uint64_t lo = static_cast<uint64_t>(w4) << 6 | 0x20u;
            const uint64_t w1 = static_cast<uint64_t>(w10) | static_cast<uint64_t>(w14) << 2 | static_cast<uint64_t>(ti) << 24;
            if (tex != 0) {
                const uint32_t e = tex + ti * 16;
                const TexWords tw = tex_words(e);
                sd(pk, static_cast<uint64_t>(sxw(static_cast<uint32_t>(lh(e + 8) - 1))) << 2 | lo | static_cast<uint64_t>(w0) << 32);
                sd(pk + 0x10, w1);
                sd(pk + 0x20, tw.q2);
                sd(pk + 0x30, tw.q0);
            } else {
                const uint32_t g = OV(0x00182640) + ti * 24;
                sd(pk, (ld(g + 8) & 0x1c) | lo | static_cast<uint64_t>(w0) << 32);
                sd(pk + 0x10, w1);
                sd(pk + 0x20, ld(g + 16));
                sd(pk + 0x30, ld(g));
            }
        }
    }
}

// The three LOD distances 0x160f60.. (x 1024, as integers) to 0x160f70..; the blend factors of
// the two LOD bands (distances scaled by *(0x16cc60)) to the 0x40 bytes at 0x1c28f0.
// orig: 0x00293f00
void lod_bands_set() {
    sw(OV(0x00160f70), static_cast<uint32_t>(float_to_int(lwc1(OV(0x00160f60)) * 1024.0f)));
    sw(OV(0x00160f74), static_cast<uint32_t>(float_to_int(lwc1(OV(0x00160f64)) * 1024.0f)));
    const int32_t i2 = float_to_int(lwc1(OV(0x00160f68)) * 1024.0f);
    const float k = lwc1(OV(0x0016cc60));
    const float d0 = lwc1(OV(0x00160f60)) * k, d1 = lwc1(OV(0x00160f64)) * k, d2 = lwc1(OV(0x00160f68)) * k;
    sw(OV(0x00160f78), static_cast<uint32_t>(i2));
    const float r0 = fpu_div(1.0f, d0 - d1), r1 = fpu_div(1.0f, d1 - d2);
    mem_clear128(OV(0x001c28f0), 0x40);
    constexpr OvConst<0x001c28f0u> B{};
    swc1(B + 0xc, d0);
    swc1(B + 0x1c, d1);
    swc1(B + 0x10, r1 * 0.5f);
    swc1(B + 0x24, d0 * r0);
    swc1(B + 0x30, (d2 * r1) * -0.5f);
    swc1(B + 0x34, d1 * r1);
    swc1(B + 0x20, (d1 * r0) * -0.5f);
    swc1(B, r0 * 0.5f);
    swc1(B + 4, -r0);
    swc1(B + 0x14, -r1);
}

// The moby LOD block: count (+4) to 0x160f50, the distances from +8 (x 6, x 4, x 2) to
// 0x160f60.. (then lod_bands_set); the 0x40-byte entries from the offset +0 (to 0x160f4c) get
// +0x10 relocated, and each entry's +0x28 packets (0x50 bytes from +0x10 + +0x1c) their GS
// words from the texture table `tex`.
// orig: 0x00241458
void moby_lod_set(uint32_t p, uint32_t tex) {
    sw(OV(0x00160f50), lwu(p + 4));
    const float d = lwc1(p + 8);
    swc1(OV(0x00160f60), d * 6.0f);
    swc1(OV(0x00160f64), d * 4.0f);
    swc1(OV(0x00160f68), d + d);
    lod_bands_set();
    const uint32_t base = p + lwu(p);
    sw(OV(0x00160f4c), base);
    for (int32_t n = lw(OV(0x00160f50)), i = 0; i < n; ++i) {
        const uint32_t e = base + static_cast<uint32_t>(i) * 0x40;
        sw(e + 0x10, base + lwu(e + 0x10));
    }
    for (int32_t i = 0; i < lw(OV(0x00160f50)); ++i) {
        const uint32_t eo = static_cast<uint32_t>(i) * 0x40;
        for (uint32_t t = 0; t < lbu(eo + lwu(OV(0x00160f4c)) + 0x28); ++t) {
            const uint32_t e = eo + lwu(OV(0x00160f4c));
            uint32_t pk = lwu(e + 0x10) + lhu(e + 0x1c) + t * 0x50;
            const uint32_t ti = lwu(pk);
            const int64_t w10 = sxw(lwu(pk + 0x10)), w14 = sxw(lwu(pk + 0x14)), w20 = sxw(lwu(pk + 0x20)), w24 = sxw(lwu(pk + 0x24));
            const uint32_t te = tex + ti * 16;
            const TexWords tw = tex_words(te);
            sd(pk, tw.q0);
            pk += 0x10;
            sd(pk, static_cast<uint64_t>(sxw(static_cast<uint32_t>(lh(te + 8) - 1))) << 2 | (static_cast<uint64_t>(w14) << 6 | 0x20u) |
                       static_cast<uint64_t>(w10) << 32);
            pk += 0x10;
            sd(pk, static_cast<uint64_t>(w20) | static_cast<uint64_t>(w24) << 2 | static_cast<uint64_t>(sxw(ti)) << 24);
            pk += 0x10;
            sd(pk, tw.q2);
            sd(pk + 0x10, 0);
        }
    }
}

// Prints the class memory list sorted by size (debug output only: no effect on memory).
// orig: 0x00241788
void sub_00241788(uint32_t, int32_t) {}

// Decompress a "WAD" block (LZ with the magic "WAD" at +0, the compressed size at +3, data from
// +0x10) to `dst`; returns the bytes written. The input is streamed through the scratchpad by
// DMA in 0x2000-byte halves (0x70000000 / 0x70002000), a 0x11 code with distance 0 marking the
// end of each half. A bad magic or an invalid code traps (teq) in the original.
// orig: 0x0024d8d0
int32_t wad_decompress(uint32_t src, uint32_t dst) {
    if ((lhu(src) ^ 0x4157u) != (lbu(src + 2) ^ 0x44u)) std::abort();  // teq: trap
    uint32_t size = static_cast<uint32_t>(lbu(src + 3)) | static_cast<uint32_t>(lbu(src + 4)) << 8 | static_cast<uint32_t>(lbu(src + 5)) << 16 |
                    static_cast<uint32_t>(lbu(src + 6)) << 24;
    uint32_t madr = src + 0x10, half = 0;
    auto dma = [&] {
        spr_dma_to(half, madr, 0x200);
        half ^= 0x2000;
    };
    dma();
    madr += 0x2000;
    dma();
    madr += 0x2000;
    uint32_t in = 0x70000000u, end = 0x80000000u, out = dst;
    size -= 0x10;
    if (!(0x1fffu < size)) end = in + size;
    auto rd = [&in] { return static_cast<uint32_t>(lbu(in++)); };
    auto put = [&out](uint32_t v) { sb(out++, v); };
    auto copy_match = [&](uint32_t from, int32_t n) {
        sb(out, lbu(from));
        sb(out + 1, lbu(from + 1));
        out += 2;
        from += 2;
        do {
            sb(out++, lbu(from++));
            --n;
        } while (n > 0);
    };
    enum class St { First, Top, Next } st = St::First;
    uint32_t v = 0;
    for (;;) {
        if (st == St::First) {
            const int32_t lit = static_cast<int32_t>(lbu(in)) - 0x11;
            if (0 < lit) {
                ++in;
                for (int32_t c = lit; c > 0; --c) put(rd());
                st = St::Next;
                continue;
            }
            st = St::Top;
            continue;
        }
        if (st == St::Top) {  // 0x24d988: a literal run may follow
            const bool more = in < end;
            v = lbu(in);
            ++in;
            if (!more) break;
            if (v < 0x10) {
                if (v == 0) {
                    v = lbu(in) + 0xf;
                    ++in;
                }
                put(lbu(in));
                put(lbu(in + 1));
                put(lbu(in + 2));
                in += 3;
                int32_t c = static_cast<int32_t>(v);
                do {
                    put(rd());
                    --c;
                } while (c > 0);
                st = St::Next;
                continue;
            }
        } else {  // 0x24d9f8
            const bool more = in < end;
            v = lbu(in);
            ++in;
            if (!more) break;
        }
        // 0x24da08: a match
        uint32_t from;
        int32_t n;
        bool copy = true;
        if (0x40 <= v) {
            from = out - 1 - ((v >> 2) & 7) - lbu(in) * 8u;
            ++in;
            n = static_cast<int32_t>(v >> 5) - 1;
        } else if (0x20 <= v) {
            n = static_cast<int32_t>(v & 0x1f);
            if (n == 0) {
                n = static_cast<int32_t>(lbu(in)) + 0x1f;
                ++in;
            }
            const uint32_t d = (static_cast<uint32_t>(lbu(in)) >> 2) + lbu(in + 1) * 64u;
            in += 2;
            from = out - 1 - d;
        } else if (0x10 <= v) {
            from = out - ((v & 8) << 11);
            n = static_cast<int32_t>(v & 7);
            if (n == 0) {
                n = static_cast<int32_t>(lbu(in)) + 7;
                ++in;
            }
            const uint32_t d = (static_cast<uint32_t>(lbu(in)) >> 2) + lbu(in + 1) * 64u;
            in += 2;
            from -= d;
            if (from == out) {
                if (n != 1) {  // the end of a half: the next one, the one after it requested
                    dma();
                    size -= 0x2000;
                    madr += 0x2000;
                    in = 0x70000000u + half;
                    end = 0x1fffu < size ? 0x80000000u : in + size;
                    st = St::Top;
                    continue;
                }
                copy = false;  // no copy: straight to the literals
            } else {
                from -= 0x4000;
            }
        } else {
            std::abort();  // teq: trap
        }
        if (copy) copy_match(from, n);
        // 0x24db60: up to 3 literals after a match (low bits of its last byte)
        const uint32_t k = lbu(in - 2) & 3u;
        if (k == 0) {
            st = St::Top;
            continue;
        }
        sb(out, lbu(in));
        sb(out + 1, lbu(in + 1));
        sb(out + 2, lbu(in + 2));
        out += k;
        in += k;
        st = St::Next;
    }
    return static_cast<int32_t>(out - dst);
}

// The frame-rate constants: NTSC (pal == 0): scales 1.0 (0x15ed60 / 64 / 68), 0x15ed6c = 1/60,
// gravity factors 0x15ed70 / 0x15ed74, 5 (0x15ed78), 1/60 (0x15ed7c), 0x15ed80 = 0; PAL: the 50 Hz
// set and 0x15ed80 = 1.
// orig: 0x00261358
void frame_rate_set(int32_t pal) {
    if (pal == 0) {
        swc1(0x0015ed68, 1.0f);
        sw(0x0015ed70, 0x3991a2b4u);
        sw(0x0015ed74, 0x369b5837u);
        sw(0x0015ed78, 5);
        sw(0x0015ed7c, 0x3c888889u);
        sw(0x0015ed80, 0);
        swc1(0x0015ed60, 1.0f);
        swc1(0x0015ed64, 1.0f);
        sw(0x0015ed6c, 0x3c888889u);
        return;
    }
    sw(0x0015ed80, 1);
    sw(0x0015ed60, 0x3f99999au);
    sw(0x0015ed64, 0x3fb851ecu);
    sw(0x0015ed68, 0x3f555555u);
    sw(0x0015ed6c, 0x3ca3d70bu);
    sw(0x0015ed70, 0x39d1b718u);
    sw(0x0015ed74, 0x370637bdu);
    sw(0x0015ed78, 6);
    sw(0x0015ed7c, 0x3ca3d70au);
}

// The fog colour / range from the level (0x15f444..0x15f454) or, with 0x167014 set, from
// 0x161204..0x161214, into the projection block (0x16cc58.. range, 0x16cc70.. colour), the far
// clip 0x16017c (0x1f4000 / 0x40000); then the projection (0x1f7980); 0x15f458 cleared.
// orig: 0x001f7170
void fog_setup() {
    constexpr OvConst<0x0016ca40u> S{};
    if (lw(OV(0x00167014)) != 0) {
        sw(S + 0x230, lbu(OV(0x00161204)));
        sw(S + 0x22c, lwu(OV(0x00161214)));
        sw(S + 0x234, lbu(OV(0x00161205)));
        sw(S + 0x238, lbu(OV(0x00161206)));
        sw(S + 0x218, lwu(OV(0x00161208)));
        sw(S + 0x21c, lwu(OV(0x0016120c)));
        sw(S + 0x228, lwu(OV(0x00161210)));
        sw(OV(0x0016017c), 0x40000);
    } else {
        sw(S + 0x230, lbu(OV(0x0015f444)));
        sw(S + 0x22c, lwu(OV(0x0015f454)));
        sw(S + 0x234, lbu(OV(0x0015f445)));
        sw(S + 0x238, lbu(OV(0x0015f446)));
        sw(S + 0x218, lwu(OV(0x0015f448)));
        sw(S + 0x21c, lwu(OV(0x0015f44c)));
        sw(S + 0x228, lwu(OV(0x0015f450)));
        sw(OV(0x0016017c), NOT_ADDR(0x1f4000));
    }
    projection_setup();
    sw(OV(0x0015f458), 0);
}

// The background colour register 0x152078 (r | g << 8 | b << 16 | 0x80000000).
// orig: 0x00200ef0
void bg_color_set(uint32_t r, uint32_t g, uint32_t b) { sw(0x00152078, r | g << 8 | b << 16 | 0x80000000u); }

// Marks `n + 1` in the 64 slots (halfwords, stride 4, from +0xfc down) of the 0x100-byte record
// `idx` at 0x14d590: the slot already holding it or the first empty one from the top. Returns the
// slot, or -1 (n < 0, or no room).
// orig: 0x00284d90
int32_t sub_00284d90(uint32_t idx, int32_t n) {
    if (n < 0) return -1;
    const uint32_t v = static_cast<uint32_t>(n + 1);
    uint32_t a = idx * 0x100 + 0x0014d590 + 0xfc;
    int32_t k = 0x3f;
    for (;;) {
        const int32_t c = lh(a);
        if (c == n + 1 || c == 0) {
            sh(a, v);
            return k;
        }
        --k;
        if (k < 0) return k;
        a -= 4;
    }
}

// Clears the frame's effect counters (0x15f424.., 0x15f2f0.., 0x15f404.., 0x15f474 / 88 / 98,
// 0x15f608, 0x1612d0..d8, 0x1612dc).
// orig: 0x001f83d0
void sub_001f83d0() {
    for (uint32_t a : {OV(0x0015f424u), OV(0x0015f42cu), OV(0x0015f430u), OV(0x0015f428u), OV(0x0015f434u), OV(0x0015f2f0u), OV(0x0015f2f4u), OV(0x0015f404u), OV(0x0015f408u),
                       OV(0x0015f30cu), OV(0x0015f320u), OV(0x0015f330u), OV(0x0015f608u), OV(0x001612d0u), OV(0x001612d4u), OV(0x001612d8u), OV(0x001612dcu)})
        sw(a, 0);
}

// 0x15f45c = 1.
// orig: 0x001fb028
void sub_001fb028() { sw(OV(0x0015f45c), 1); }

// Store `v` in the level's slot `i` of 0x1791b8 (levels below 0x13 only; 0x1c44b8 gives each
// level's first slot).
// orig: 0x0024a798
void level_slot_set(uint32_t v, int32_t i) {
    const int32_t lv = lw(0x0015ed84);
    if (!(lv < 0x13)) return;
    const int32_t first = lw(OV(0x001c44b8) + static_cast<uint32_t>(lv) * 4);
    sw(OV(0x001791b8) + static_cast<uint32_t>(i + first) * 4, v);
}

// 0x13cad0 / 0x13cace cleared.
// orig: 0x00266d48
void sub_00266d48() {
    sw(0x0013cad0, 0);
    sh(0x0013cace, 0);
}

// The handler of the record `p` (s16 id at +0, index to +2, function to +4) from the table
// 0x1ea980 (8-byte rows: id, function; -1 ends it).
// orig: 0x0028df58
void sub_0028df58(uint32_t p) {
    uint32_t k = 0;
    int32_t c = lw(OV(0x001ea980));
    if (c != -1) {
        const int32_t id = lh(p);
        if (c != id) {
            uint32_t row = OV(0x001ea988);
            for (;;) {
                c = lw(row);
                ++k;
                if (c == -1 || c == id) break;
                row += 8;
            }
        }
    }
    sh(p + 2, k);
    sw(p + 4, lwu(OV(0x001ea980) + k * 8 + 4));
}

// The ship moby *(0x13e030) hidden (flags | 3) without collision.
// orig: 0x0028e990
void ship_hide() {
    const uint32_t m = lwu(0x0013e030);
    if (m == 0) return;
    sh(m + 0x34, lhu(m + 0x34) | 3u);
    sw(m + 0x94, 0);
}

// The ship moby `m` at the level start: 0x160540 = 1, the old one hidden; on level 0xd the
// position 0x13e090.. is set; m becomes *(0x13e030) with the armor index (class - 0x213) at
// 0x13e056, the update function 0x28e180, visible with collision, animation 1; its pvars'
// first three words to 0x13e060.
// orig: 0x0028e8a0
void ship_init(uint32_t m) {
    sw(OV(0x00160540), 1);
    ship_hide();
    if (lw(0x0015ed84) == 0xd) {
        sw(0x0013e090, 0x43e8547bu);
        sw(0x0013e0a8, 0x403147aeu);
        sw(0x0013e094, 0x44112b85u);
        sw(0x0013e098, 0x439e5c29u);
    }
    sw(0x0013e030, m);
    sh(0x0013e056, lhu(m + 0xa6) - 0x213u);
    sw(m + 0x74, OVF(0x0028e180));
    sh(m + 0x34, lhu(m + 0x34) & 0xfffcu);
    sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
    moby_set_anim(m, 1, 0);
    const uint32_t pv = lwu(m + 0x78);
    sw(0x0013e060, lwu(pv));
    sw(0x0013e064, lwu(pv + 4));
    sw(0x0013e068, lwu(pv + 8));
}

// The hero's reset at a level start: camera fields (+0x234, +0xa94, +0x220..+0x22c), 0x141668,
// +0x22cb, the state 0 (0x232628, hero_set_state(0, 1)).
// orig: 0x00204f80
void hero_level_reset() {
    constexpr uint32_t P = 0x0013f350;
    sw(P + 0x234, 0x3ee66666u);
    swc1(P + 0xa94, 1.0f);
    sw(P + 0x228, 0x3f4ccccdu);
    sw(P + 0x22c, 0x3f333333u);
    sw(0x00141668, 0);
    sw(P + 0x220, 0x3f4ccccdu);
    sw(P + 0x224, 0x3f333333u);
    sb(P + 0x22cb, 0);
    sw(P + 0x2084, 0);
    sub_00232628();
    hero_set_state(0, 1);
}

// The sky / scene block of the core (+0x10 offsets relocated, the 16-byte entries at +0x10:
// coordinates >> 4 and log2 sizes, the +0x20 lists of 0x20-byte records relocated); its address
// to 0x16051c, +4 = 1.
// orig: 0x0023f848
void sub_0023f848(uint32_t p) {
    sw(OV(0x0016051c), p);
    sh(p + 4, 1);
    sw(p + 0x10, lwu(p + 0x10) + p);
    sw(p + 0x14, lwu(p + 0x14) + p);
    sw(p + 0x18, lwu(p + 0x18) + p);
    if (lw(p + 0x1c) != 0) sw(p + 0x1c, lwu(p + 0x1c) + p);
    uint32_t src = lwu(lwu(OV(0x0016051c)) + 0x10);
    for (int32_t i = 0; i < lh(lwu(OV(0x0016051c)) + 0xc); ++i) {
        const int32_t x = lw(src), y = lw(src + 4), a = lw(src + 8), b = lw(src + 0xc);
        src += 0x10;
        const uint32_t o = static_cast<uint32_t>(i) * 16;
        sh(lwu(lwu(OV(0x0016051c)) + 0x10) + o + 0xa, static_cast<uint32_t>(x >> 4));
        sh(lwu(lwu(OV(0x0016051c)) + 0x10) + o + 8, static_cast<uint32_t>(y >> 4));
        sh(lwu(lwu(OV(0x0016051c)) + 0x10) + o + 0xc, static_cast<uint32_t>(highest_bit_index(a)));
        sh(lwu(lwu(OV(0x0016051c)) + 0x10) + o + 0xe, static_cast<uint32_t>(highest_bit_index(b)));
        sd(lwu(lwu(OV(0x0016051c)) + 0x10) + o, 0);
    }
    const uint32_t q = lwu(OV(0x0016051c));
    for (int32_t i = 0; i < lh(q + 6); ++i) {
        const uint32_t slot = q + 0x20 + static_cast<uint32_t>(i) * 4;
        const uint32_t l = lwu(slot) + p;
        sw(slot, l);
        for (int32_t k = 0; k < lw(l); ++k) {
            const uint32_t w = l + 0x20 + static_cast<uint32_t>(k) * 0x20;
            sw(w, lwu(w) + p);
        }
    }
}

// The level's GS RAM: `n` texture blocks (16-byte records at `list`: format 0x13 PSMT8 w x h,
// 2 a 16 x 16 PSMCT16 palette, 0 a 16 x 16 PSMCT32 palette, +4 w | h << 16, +0xc the data's
// offset from `data`) uploaded one after the other from 0x15ee8c (0x15ee74 moved past each: w x h
// bytes, 0x100 at least / 0x200 / 0x400); the frame's own uploads then start past them (0x15ee78).
// orig: 0x00240498
void gs_ram_upload(uint32_t data, int32_t n, uint32_t list) {
    const uint32_t lp = ee_stack_alloc(0xf0);  // (the load image packet at the frame's base)
    const uint32_t base = lwu(0x0015ee8c);
    sw(0x0015ee78, base);
    sw(0x0015ee74, base);
    for (int32_t left = n; left > 0;) {
        const uint32_t src = data + lwu(list + 0xc);
        const uint32_t wh = lwu(list + 4);
        const int32_t h = static_cast<int32_t>(wh) >> 16, w = static_cast<int32_t>(wh & 0xffff);
        const int32_t dbp = static_cast<int32_t>(lwu(0x0015ee74) << 8) >> 16;
        const uint32_t fmt = lwu(list);
        if (fmt == 0x13) {
            const int32_t bw = w >> 6;
            gs_set_def_load_image(lp, dbp, bw == 0 ? 1 : bw, 0x13, 0, 0, static_cast<int16_t>(w), h);
            int32_t size = static_cast<int32_t>(static_cast<uint32_t>(w) * static_cast<uint32_t>(h));
            if (!(0xff < size)) size = 0x100;
            sw(0x0015ee74, lwu(0x0015ee74) + static_cast<uint32_t>(size));
        } else if (fmt == 2) {
            gs_set_def_load_image(lp, dbp, 1, 2, 0, 0, 0x10, 0x10);
            sw(0x0015ee74, lwu(0x0015ee74) + 0x200);
        } else if (fmt == 0) {
            gs_set_def_load_image(lp, dbp, 1, 0, 0, 0, 0x10, 0x10);
            sw(0x0015ee74, lwu(0x0015ee74) + 0x400);
        }
        // FlushCache(0) (0x118a80): no memory effect on the host
        --left;
        list += 0x10;
        gs_exec_load_image(lp, src);  // (another format: the previous packet again)
        gs_sync_path(0, 0);
    }
    sw(0x0015ee78, lwu(0x0015ee74));
    ee_stack_free(0xf0);
}

// The armor (the hero's two armor classes): the armor file (*(0x15ee54), size from the table of
// contents) copied to `dst`; its texture levels and palettes uploaded to the GS (four sceGsExecLoadImage);
// the GS registers of the two palettes (0x15fc00 / 0x15fc10, VRAM addresses from the core header
// +0xac..+0xb8) as texture records 0x1aebc0[n] / [n + 1] with their data copied after `dst`
// (0x14000 and 0x4000 bytes, 0x199a80[n] / [n + 1]); each armor class registered unless already;
// 0x160564 / 0x160568 = 1, 0x16056c = 0. Returns the end of the copy.
// orig: 0x0023fa00
uint32_t armor_load(uint32_t dst, uint32_t core) {
    const int32_t armor = lh(0x0013e056);
    // (enhancement, #94: the file read is the chosen ship's, its size that file's; its classes registered as the game's ship's)
    mem_copy128(dst, lwu(0x0015ee54), lw(0x00137b80 + static_cast<uint32_t>(advanced_ship_armor(armor) + 1) * 8 + 0x12cc) << 11);
    const uint32_t w0 = lwu(dst), s3 = lwu(dst + 4), w8 = lwu(dst + 8), s7 = lwu(dst + 0xc);
    // the four GS uploads (sceGsSetDefLoadImage 0x122330 / FlushCache 0x118a80 / sceGsExecLoadImage 0x122658 / sceGsSyncPath
    // 0x120558, four times at the loop 0x23fa88): the armor's 64 x 64 and 32 x 32 texture levels (PSMT8) of block 0, and
    // the two 16 x 16 palettes (PSMCT32), each at its VRAM address (the core header's +0xac / +0xb0 / +0xb4 / +0xb8 words
    // from 0x15ee8c). The load image packet is on the frame's stack (0x110 bytes: the packet at +0, the words w0 / w8 at +0x60 / +0x64).
    const uint32_t lp = ee_stack_alloc(0x110);
    sw(lp + 0x60, w0);
    sw(lp + 0x64, w8);
    for (int32_t i = 0; i < 4; ++i) {
        const uint32_t sp = lwu(0x0015ee8c) + lwu(core + (i == 0 ? 0xacu : i == 1 ? 0xb0u : i == 2 ? 0xb4u : 0xb8u));
        const int32_t dbp = static_cast<int32_t>(sp << 8) >> 16;
        uint32_t src;
        if (i == 0) {
            gs_set_def_load_image(lp, dbp, 1, 0x13, 0, 0, 0x40, 0x40);
            src = dst + s3 + 0x14430;
        } else if (i == 1) {
            gs_set_def_load_image(lp, dbp, 1, 0x13, 0, 0, 0x20, 0x20);
            src = dst + s3 + 0x15430;
        } else if (i == 2) {
            gs_set_def_load_image(lp, dbp, 1, 0, 0, 0, 0x10, 0x10);
            src = dst + s3 + 0x30;
        } else {
            gs_set_def_load_image(lp, dbp, 1, 0, 0, 0, 0x10, 0x10);
            src = dst + s7 + 0x30;
        }
        // FlushCache(0) (0x118a80): no memory effect on the host
        gs_exec_load_image(lp, src);
        gs_sync_path(0, 0);
    }
    ee_stack_free(0x110);
    sh(OV(0x0015fc0a), static_cast<uint32_t>(lw(core + 0xb4) >> 8));
    uint32_t n = lwu(OV(0x0015ffc8));
    sh(OV(0x0015fc0c), static_cast<uint32_t>(lw(core + 0xac) >> 8));
    sh(OV(0x0015fc0e), static_cast<uint32_t>(lw(core + 0xb0) >> 8));
    lq_sq(OV(0x001aebc0) + n * 16, OV(0x0015fc00));
    uint32_t out = dst + s3;
    sw(OV(0x00199a80) + n * 4, out + 0x80000000u);
    mem_copy128(out, dst + s3 + 0x430, 0x14000);
    uint32_t cls = lwu(OV(0x00160548) + static_cast<uint32_t>(armor) * 4);
    out += 0x14000;
    sb(OV(0x0015fc20), lbu(OV(0x0015ffc8)));
    if (lbu(OV(0x00197bc0) + cls) == 0xff) moby_class_register(dst + w0, OV(0x0015fc00) - lwu(OV(0x0015ffc8)) * 16, OV(0x0015fc20), cls);
    sh(OV(0x0015fc1a), static_cast<uint32_t>(lw(core + 0xb8) >> 8));
    n = lwu(OV(0x0015ffc8)) + 1;
    sw(OV(0x0015ffc8), n);
    lq_sq(OV(0x001aebc0) + n * 16, OV(0x0015fc10));
    sw(OV(0x00199a80) + n * 4, out + 0x70000000u);
    mem_copy128(out, dst + s7 + 0x430, 0x4000);
    out += 0x4000;
    sb(OV(0x0015fc20), lbu(OV(0x0015ffc8)));
    cls = lwu(OV(0x00160558) + static_cast<uint32_t>(lh(0x0013e056)) * 4);
    if (lbu(OV(0x00197bc0) + cls) == 0xff) moby_class_register(dst + w8, OV(0x0015fc10) - lwu(OV(0x0015ffc8)) * 16, OV(0x0015fc20), cls);
    sw(OV(0x00160568), 1);
    sw(OV(0x00160564), 1);
    sw(OV(0x0015ffc8), lwu(OV(0x0015ffc8)) + 1);
    sw(OV(0x0016056c), 0);
    return out;
}

// The level start's moby pass: the live mobys (+0x20 >= 0 and not flag 2) linked through +0x28
// from *(0x15ffe4); each one's animation advanced (unless flag 0x40), its update function
// (+0x74) run, and its transform updated (unless flag 4).
// orig: 0x00264740
void moby_update_pass() {
    sw(OV(0x001aba00), 0);
    sw(OV(0x0015ffe4), 0);
    uint32_t prev = 0;
    for (uint32_t m = lwu(OV(0x0015ffd8)); lbu(m + 0x20) != 0xff; m += 0x100) {
        if ((lbu(m + 0x20) & 0x80) != 0 || (lhu(m + 0x34) & 2) != 0) continue;
        if (prev == 0) {
            sw(OV(0x0015ffe4), m);
        } else {
            sw(prev + 0x28, m);
        }
        prev = m;
    }
    if (prev != 0) sw(prev + 0x28, 0);
    for (uint32_t m = lwu(OV(0x0015ffe4)); m != 0; m = lwu(m + 0x28)) {
        if (static_cast<int8_t>(lbu(m + 0x20)) < 0) continue;
        if ((lhu(m + 0x34) & 0x40) == 0) moby_anim_advance(m);
        if (lwu(m + 0x74) != 0) ovl_call_moby_fn(lwu(m + 0x74), m);
        if ((lhu(m + 0x34) & 4) == 0) moby_update_transform(m);
    }
}

namespace {
// orig: (helper) the 64-byte alignment of the setup's allocation pointer
uint32_t align64(uint32_t a) { return (a + 0x3f) & 0xffffffc0u; }
// orig: (helper) a texture table: each entry's GS address (base + +0) with log2(+4) in the top bits
void tex_table(uint32_t dst, uint32_t src, uint32_t count_addr) {
    for (int32_t i = 0; i < lw(count_addr); ++i, src += 0x10, dst += 4) {
        const int32_t b = highest_bit_index(lhu(src + 4));
        sw(dst, lwu(OV(0x0015f40c)) + lwu(src) + (static_cast<uint32_t>(b) << 28));
    }
}
// orig: (helper) a GS TEX0-style word of the chrome / glass maps: (vram + tex) >> 8 | (vram + pal) >> 8 << 37
uint64_t map_word(uint32_t tex_off, uint32_t pal_off, uint64_t lo, uint64_t hi) {
    const int32_t vram = lw(0x0015ee8c);
    const int64_t t = static_cast<int32_t>(static_cast<uint32_t>(vram) + tex_off) >> 8, pl = static_cast<int32_t>(static_cast<uint32_t>(vram) + pal_off) >> 8;
    return (static_cast<uint64_t>(t) | lo) | (static_cast<uint64_t>(pl) << 37 | hi) | 0x8000000000000000ull;
}
}  // namespace

// The level setup body (all of 0x244110 before its moby pass): with `full` the HUD and sound
// banks, the core (index copied to *(0x173e04), data decompressed to *(0x173e14)), the GS RAM
// textures, the chrome / glass map registers, the texture tables, the moby LOD, the sky, the
// collision, the moby / tie / shrub classes and the hero's sequences, the gadgets, the fog
// cuboids, the paths, the texture pages, the class texture remaps, the armor; then (always) the
// gameplay data; finally the checkpoint (`restore`) or the save slots cleared.
// orig: 0x00244110 (up to its moby pass)
void level_setup_body(int32_t full, int32_t restore) {
    const uint32_t s3 = lwu(OV(0x00173e04));
    const uint32_t lvl = lwu(0x0015ee4c);
    const uint32_t s4 = lwu(OV(0x00173e14));
    if (full != 0) {
        hud_data_load();
        mem_copy128(s3, lwu(lvl + 0x10) + lvl, lw(lvl + 0x14));
        sw(OV(0x0015f40c), 0);
        mem_copy(OV(0x0015fc88), static_cast<uint32_t>(lw(0x0015ed84)) * 16 + 0x0014c050, 0x10);
        gs_ram_upload(lvl + lwu(lvl + 0x18), lw(s3), s3 + lwu(s3 + 4));
        sw(OV(0x00160004), lwu(0x0015ed5c));
        const int32_t size = wad_decompress(lvl + lwu(lvl + 0x50), s4);
        constexpr uint64_t T1 = 0xffa0000000e0ull, T0 = 0x40000400004000ull;
        sd(OV(0x001827c0), map_word(lwu(s3 + 0x90), lwu(s3 + 0x94), 0x1d308000u, 0x5c0000000ull));
        sd(OV(0x001827c8), T1);
        sd(OV(0x001827d0), T0);
        sd(OV(0x001827e8), T0);
        sd(OV(0x001827e0), T1);
        sd(OV(0x001827d8), map_word(lwu(s3 + 0x98), lwu(s3 + 0x9c), 0x19304000u, 0x580000000ull));
        sd(OV(0x00182800), T0);
        sd(OV(0x001827f8), T1);
        sw(OV(0x0015f40c), s4 + lwu(s3 + 0x60));
        sd(OV(0x001827f0), map_word(lwu(s3 + 0xc0), lwu(s3 + 0xc4), 0x1d308000u, 0x5c0000000ull));
        sw(OV(0x00160f54), lwu(s3 + 0x30));
        tex_table(OV(0x001c4a00), s3 + lwu(s3 + 0x34), OV(0x00160f54));
        const uint32_t end = s4 + static_cast<uint32_t>(size);
        const uint32_t mtex = s3 + lwu(s3 + 0x3c);
        sw(OV(0x0015ffc8), lwu(s3 + 0x38));
        tex_table(OV(0x00199a80), mtex, OV(0x0015ffc8));
        mem_copy128(OV(0x001aebc0), mtex, lw(OV(0x0015ffc8)) << 4);
        sw(OV(0x00160fd4), lwu(s3 + 0x40));
        tex_table(OV(0x001c6700), s3 + lwu(s3 + 0x44), OV(0x00160fd4));
        sw(OV(0x001604ac), lwu(s3 + 0x48));
        tex_table(OV(0x001bc5b0), s3 + lwu(s3 + 0x4c), OV(0x001604ac));
        moby_lod_set(s4 + lwu(s3 + 8), s3 + lwu(s3 + 0x34));
        sw(OV(0x0015f600), lw(s3 + 0xc) != 0 ? s4 + lwu(s3 + 0xc) : 0u);
        if (lw(s3 + 0x10) == 0) {
            sw(OV(0x0016051c), 0);
        } else {
            sub_0023f848(s4 + lwu(s3 + 0x10));
        }
        level_collision_set(s4 + lwu(s3 + 0x14));
        uint32_t e = s3 + lwu(s3 + 0x1c);
        for (int32_t i = 0; i < lw(s3 + 0x18); ++i, e += 0x20) {
            const uint32_t off = lwu(e);
            moby_class_register(off == 0 ? 0u : s4 + off, s3 + lwu(s3 + 0x3c), e + 0x10, lwu(e + 4));
        }
        sub_00241788(s3 + lwu(s3 + 0x1c), lw(s3 + 0x1c));
        uint32_t seq = s3 + lwu(s3 + 0x78);
        for (uint32_t i = 0; i < lbu(lwu(OV(0x00197300)) + 0xc); ++i, seq += 4) {
            if (lw(seq) == 0) continue;
            sw(lwu(OV(0x00197300)) + i * 4 + 0x48, s4 + lwu(seq));
            moby_seq_relocate(lwu(OV(0x00197300)), i);
        }
        // the gadgets
        uint32_t g = s3 + lwu(s3 + 0x84);
        sw(OV(0x0016000c), 0xffffffffu);
        sw(OV(0x00160010), 0);
        sw(OV(0x00160008), lwu(s3 + 0x80));
        for (int32_t i = 0; i < lw(s3 + 0x80); ++i, g += 0x10) {
            const uint32_t k = static_cast<uint32_t>(i);
            sw(OV(0x001afbc0) + k * 4, lwu(g + 4));
            sw(OV(0x001afc20) + k * 4, s4 + lwu(g));
            sw(OV(0x001afc80) + k * 4, lwu(g + 8));
            const uint32_t ci = lbu(OV(0x00197bc0) + lwu(g + 4));
            lq_sq(OV(0x001afce0) + k * 16, s3 + lwu(s3 + 0x1c) + ci * 32 + 0x10);
        }
        e = s3 + lwu(s3 + 0x24);
        for (int32_t i = 0; i < lw(s3 + 0x20); ++i, e += 0x20) tie_class_register(s4 + lwu(e), s3 + lwu(s3 + 0x44), e + 0x10, lwu(e + 4));
        sub_00241938(s3 + lwu(s3 + 0x24), lw(s3 + 0x20));
        e = s3 + lwu(s3 + 0x2c);
        for (int32_t i = 0; i < lw(s3 + 0x28); ++i, e += 0x30) shrub_class_register(s4 + lwu(e), s3 + lwu(s3 + 0x4c), e + 0x10, e + 0x20, lwu(e + 4));
        sw(OV(0x0015f420), s4 + lwu(s3 + 0x68));
        sub_0023f768(s3 + lwu(s3 + 0x5c), lw(s3 + 0x58));
        if (lw(s3 + 0xa0) == 0) {
            sw(OV(0x0015f438), 0);
            sw(OV(0x0015f43c), 0);
            sw(OV(0x0015f440), 0);
        } else {
            sub_0023f120(s4 + lwu(s3 + 0xa0));
        }
        if (lw(s3 + 0xa4) == 0) {
            sw(OV(0x0015fc98), 0);
            sw(OV(0x0015fc9c), 0);
            sw(OV(0x0015fca4), 0);
            sw(OV(0x0015fca0), 0);
            sw(OV(0x0015fca8), 0);
        } else {
            sub_0023f0d8(s4 + lwu(s3 + 0xa4));
        }
        sw(OV(0x0015f604), lw(s3 + 0xa8) != 0 ? s4 + lwu(s3 + 0xa8) : 0u);
        sub_0023f630(s3 + lwu(s3 + 0x6c), static_cast<int32_t>(s4 + lwu(s3 + 0x64)), s3 + lwu(s3 + 0x54), lw(s3 + 0x50));
        // the texture remaps of the moby classes' packets
        const uint32_t s6 = s3 + lwu(s3 + 0x70);
        uint32_t rm = align64(end);
        const int32_t nrm = lh(s6 + 2);
        sw(OV(0x0015f5f4), rm);
        sw(OV(0x0015f5f0), static_cast<uint32_t>(nrm));
        mem_copy(rm, s6 + static_cast<uint32_t>(lh(s6)), nrm << 5);
        uint32_t s2 = s6 + 4;
        rm += static_cast<uint32_t>(nrm) << 5;
        const uint32_t tab = s6 + static_cast<uint32_t>(lh(s2));
        for (int32_t i = 0; i < lw(OV(0x0015f5f0)); ++i) {
            const uint32_t a = lwu(OV(0x0015f5f4)) + static_cast<uint32_t>(i) * 0x20;
            const int32_t t = lh(a + 0x1a);
            if (!(t < lh(s2 + 2))) {
                debug_printf_stub();
                sh(lwu(OV(0x0015f5f4)) + static_cast<uint32_t>(i) * 0x20 + 0x1a, 0xffff);
            } else {
                sh(a + 0x1a, lhu(tab + static_cast<uint32_t>(t) * 4));
            }
            sw(lwu(OV(0x0015f5f4)) + static_cast<uint32_t>(i) * 0x20 + 0x1c, lwu(0x0015ed5c));
        }
        const uint32_t after = rm + 0x3f;
        s2 += 4;
        for (int32_t c = 0; c < lw(OV(0x0015ffc0)); ++c) {
            const uint32_t slot = OV(0x00197300) + static_cast<uint32_t>(c) * 4;
            const uint32_t s5 = s2 + 4;
            if (lw(slot) == 0) {
                const int32_t ng = lw(OV(0x00160008));
                int32_t k = 0;
                if (0 < ng) {
                    const int32_t id = lh(OV(0x00197a00) + static_cast<uint32_t>(c) * 2);
                    if (lw(OV(0x001afbc0)) != id) {
                        uint32_t a = OV(0x001afbc0);
                        for (;;) {
                            ++k;
                            if (!(k < ng)) break;
                            a += 4;
                            if (lw(a) == lh(OV(0x00197a00) + static_cast<uint32_t>(c) * 2)) break;
                        }
                    }
                    if (k < ng) {
                        const uint32_t d = OV(0x001afe60) + static_cast<uint32_t>(k) * 32;
                        mem_fill32(d, 0xffffffffu, 0x20);
                        uint32_t src = s6 + static_cast<uint32_t>(lh(s2));
                        if (lh(s2 + 2) > 0) {
                            uint32_t o = d;
                            int32_t j = 0;
                            uint32_t v = lhu(src);
                            for (;;) {
                                ++j;
                                src += 4;
                                sh(o, v);
                                if (!(j < lh(s2 + 2))) break;
                                o += 2;
                                if (!(j < 0xf)) break;
                                v = lhu(src);
                            }
                        }
                    }
                }
            } else {
                if (lh(s2 + 2) != static_cast<int32_t>(lbu(lwu(slot) + 0xd))) debug_printf_stub();
                uint32_t src = s6 + static_cast<uint32_t>(lh(s2));
                if (lbu(lwu(slot) + 0xd) != 0) {
                    const uint32_t t0 = lwu(0x0015ed5c);
                    uint32_t j = 0;
                    do {
                        const uint32_t o = j * 32;
                        const uint32_t v = lhu(src);
                        ++j;
                        sh(lwu(lwu(slot) + 0x28) + o + 0x1a, v);
                        sw(lwu(lwu(slot) + 0x28) + o + 0x1c, t0);
                        src += 4;
                    } while (j < lbu(lwu(slot) + 0xd));
                }
            }
            s2 = s5;
        }
        sw(OV(0x00173e18), armor_load(after & 0xffffffc0u, s3));
    }
    sw(OV(0x00173e1c), gameplay_load(full));
    // (full: memory statistics printed; no effect on memory)
    if (restore != 0) {
        checkpoint_restore();
    } else {
        sub_00284e00();
        for (uint32_t a = 0x0014eecc; a >= 0x0014ee90; a -= 4) sw(a, 0);
    }
    sw(OV(0x00179110), 0);
}

// The level setup: its body, the moby pass, *(0x15f5cc) + 1. Returns 0.
// orig: 0x00244110
int32_t level_setup(int32_t full, int32_t restore) {
    level_setup_body(full, restore);
    moby_update_pass();
    sw(OV(0x0015f5cc), lwu(OV(0x0015f5cc)) + 1);
    return 0;
}

}  // namespace rac1
