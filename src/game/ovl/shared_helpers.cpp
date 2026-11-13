// Small helpers several levels carry copies of, each ported once: the copies of each are the same code relinked (their
// bodies compared with the relocated fields masked). What they name is the engine's (OV: the grid's data 0x1ca880.., the
// packet template 0x1609a0 and cursor 0x1611c0, the game mode 0x15f5c4, the list 0x16c860 and record 0x17c440) or the base
// ELF's (bare: the talkers 0x13d5b0, the frame time 0x15ed7c, the flag 0x15edb0); class_mates_mark_level's size is the
// level's own (a per-level address).
// Header shared_helpers.h.
#include <game/ovl/shared_helpers.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/carry_hooks.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/devastator.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l3.h>
#include <game/ovl/variants_l5.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

// orig: L0:0x00266858 L3:0x00254a10 L5:0x002907b8 L7:0x0028ec18 L10:0x00259700 L11:0x0028bc28 L13:0x00272260 L14:0x0026e1c0 L16:0x00261c98
void talker_word_set(uint32_t m, uint32_t value) {
    const int32_t idx = talker_index(m);
    if (idx == -1) return;
    sw(0x0013d5b0 + (static_cast<uint32_t>(idx) << 4) + 0xc, value);
}

// Each record: three blocks of 0x5c0 bytes, each with 0x400 bytes at +0x50, six of 0x40 from +0x450 and 0x18 at +0x5d0
// cleared; then its limits: +0x30 / +0x34 a quarter turn either way, +0x38 0.02, +0x3c -0.031, +0x40 0.03, +0x28 /
// +0x2c -0.0016, +0x20 / +0x24 / +0x44 0.
// orig: L1:0x002b7a48 L5:0x002cc428 L7:0x002ca980 L11:0x002c6bd0 L12:0x002ba560 L13:0x002ae368
void grid_records_reset(uint32_t base, int32_t count) {
    for (int32_t i = 0; i < count; ++i) {
        const uint32_t rec = base + static_cast<uint32_t>(i) * 0x1190;
        for (uint32_t blk = rec; blk < rec + 3 * 0x5c0; blk += 0x5c0) {
            libc_memset(blk + 0x50, 0, 0x400);
            for (uint32_t o = 0x450; o < 0x5d0; o += 0x40) libc_memset(blk + o, 0, 0x40);
            libc_memset(blk + 0x5d0, 0, 0x18);
        }
        swc1(rec + 0x30, u2f(0x3fc90fdbu));
        swc1(rec + 0x34, u2f(0xbfc90fdbu));
        swc1(rec + 0x38, u2f(0x3ca3d70au));
        swc1(rec + 0x3c, u2f(0xbcfdf3b6u));
        swc1(rec + 0x40, u2f(0x3cf5c28fu));
        sw(rec + 0x44, 0);
        sw(rec + 0x20, 0);
        sw(rec + 0x24, 0);
        swc1(rec + 0x28, u2f(0xbad1b717u));
        swc1(rec + 0x2c, u2f(0xbad1b717u));
    }
}

// The grid's origin (x +8, y +0xc) and cell size (+0): x / y from the origin, z less 1, all in 1024ths; the cell size
// twice (halves).
// orig: L1:0x002b7c68 L5:0x002cc648 L7:0x002caba0 L11:0x002c6df0 L12:0x002ba780 L13:0x002ae588
void grid_point_fixed(uint32_t p, uint32_t out) {
    const float one_k = 1024.0f;
    sw(out, static_cast<uint32_t>(float_to_int(fpu_mul(fpu_add(lwc1(p), lwc1(OV(0x001ca888))), one_k))));
    sw(out + 4, static_cast<uint32_t>(float_to_int(fpu_mul(fpu_add(lwc1(p + 4), lwc1(OV(0x001ca88c))), one_k))));
    sw(out + 8, static_cast<uint32_t>(float_to_int(fpu_mul(fpu_sub(lwc1(p + 8), 1.0f), one_k))));
    sh(out + 0xc, static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(OV(0x001ca880)), one_k))));
    sh(out + 0xe, static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(OV(0x001ca880)), one_k))));
}

// orig: L1:0x002b7ef0 L5:0x002cc8d0 L7:0x002cae28 L11:0x002c7078 L12:0x002baa08 L13:0x002ae810
void grid_weights_scale(float scale) {
    const float k = fpu_div(scale, lwc1(OV(0x001caa20)));
    const uint32_t weights = OV(0x001ca9e0);
    for (uint32_t i = 0; i < 92; ++i) swc1(weights + i * 4, fpu_mul(lwc1(weights + i * 4), k));
}

// orig: L3:0x00250a78 L4:0x00255150 L6:0x00271ac0 L10:0x00255a40 L15:0x00251c90
void jump_arc_set(uint32_t rec, float height, float g) {
    const float speed = sqrt_f(fpu_mul(fpu_add(g, g), lwc1(rec + 0x10)));
    const float twice = fpu_add(speed, speed);
    swc1(rec + 0x1c, speed);
    const float h2 = fpu_add(height, height);
    const float rate = fpu_div(twice, lwc1(rec + 0x10));
    swc1(rec + 0x18, fpu_div(h2, rate));
    swc1(rec + 0x14, fpu_div(h2, fpu_mul(rate, rate)));
}

// orig: L2:0x0025c758 L3:0x00248b78 L5:0x002843a8 L12:0x00272fe0 L15:0x002499d8
void band_set(uint32_t rec) {
    const float mid = lwc1(rec + 0x18);
    swc1(rec + 0x88, fpu_add(mid, 0.2f));
    swc1(rec + 0x84, fpu_sub(mid, 0.2f));
}

// orig: L2:0x0020bc88 L4:0x001ed6b8 L6:0x00212938 L8:0x0020d760 L11:0x0022c5b8 L12:0x0021d810 L13:0x00206738 L18:0x00203a38
void gs_quad_packet(uint32_t xyz, uint32_t colour, uint32_t uv, uint64_t regs, uint32_t textured) {
    const uint32_t cursor = OV(0x001611c0);
    sw(lwu(cursor), 0x10000009u);
    sw(lwu(cursor) + 4, 0);
    sw(lwu(cursor) + 8, 0);
    sw(lwu(cursor) + 0xc, 0x50000009u);
    const uint32_t start = lwu(cursor);
    sw(cursor, start + 0x10);
    lq_sq(start + 0x10, OV(0x001609a0));
    const uint32_t q = start + 0x20;
    sw(cursor, q);
    sd(start + 0x20, textured != 0 ? 5u : 0u);
    // (the words are loaded as the original's lw: sign-extended into the doublewords)
    auto word = [](uint32_t a) { return static_cast<uint64_t>(static_cast<int64_t>(lw(a))); };
    sd(q + 8, regs);
    sd(q + 0x10, 0x154);
    for (uint32_t k = 0; k < 4; ++k) {
        sd(q + 0x18 + k * 0x18, word(uv + k * 4));
        sd(q + 0x20 + k * 0x18, word(colour + k * 4));
        if (k == 3) sd(q + 0x78, 0);
        sd(q + 0x28 + k * 0x18, ld(xyz + k * 8));
    }
    sw(cursor, lwu(cursor) + 0x80);
}

// orig: L8:0x00211b08 L12:0x00221ec8 L14:0x00212598 L15:0x001f8730 L17:0x001fc060
int32_t float_timer_tick(uint32_t t) {
    const float dt = lwc1(0x0015ed7c);
    const float left = lwc1(t);
    if (!(left <= dt)) {
        swc1(t, fpu_sub(left, dt));
        return 0;
    }
    sw(t, 0);
    return static_cast<int32_t>(f2u(left)) <= 0 ? -1 : -2;
}

// orig: L8:0x002d4050 L14:0x002dae50
int32_t class252_hook_c(uint32_t m, uint32_t a1, uint32_t a2, float f12) {
    const int32_t r = carry_hookc(m, a1, a2, f12);
    const uint32_t pv = lwu(m + 0x78);
    sb(m + 0x20, 3);
    swc1(pv + 0x200, 4.0f);
    swc1(pv + 0x208, 0.5f);
    if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, 3);
    sh(pv + 0xc8, 0);
    return r;
}

// orig: L1:0x002c72c0 L5:0x00326280 L5:0x0032a6d8 L6:0x002be860 L8:0x002025d0 L8:0x002025d8 L8:0x002ba660 L8:0x003127d0 L9:0x002d0b18 L12:0x00212638 L14:0x00202db8 L15:0x002aa948 L16:0x002f0d88 L16:0x002f51e0 L17:0x002af398 L17:0x002fd208
int32_t zero_stub() { return 0; }

// (the core of the two families below: the same code but for the joint and where the size comes from)
void class_mates_mark(uint32_t m, uint32_t joint, float size) {
    if (lw(OV(0x0015f5c4)) != 2 || lbu(0x0015edb0) == 0) return;
    const uint32_t list = OV(0x0016c860);
    const uint32_t rec = OV(0x0017c440);
    for (int32_t i = 0; i < lh(list + 0x44); ++i) {
        const uint32_t o = lwu(list + 0x178 + static_cast<uint32_t>(i) * 4);
        if (lh(o + 0xa6) != lh(m + 0xa6) || lbu(rec + 1) != 0) continue;
        sub_0024f590(o, joint, rec);
        swc1(rec + 0x28, size);
        swc1(rec + 0x20, size);
        swc1(rec + 0x24, size);
    }
}

// orig: L2:0x002e0cd8 L2:0x002e1868 L4:0x002e2f90 L5:0x00317fb8 L8:0x00302c00 L11:0x002d0ec0 L11:0x002f0d58 L12:0x002e6b60 L14:0x002fb938 L14:0x002feef8 L15:0x002ec678 L16:0x002e30a8 L16:0x002e6720
void class_mates_mark_fixed(uint32_t m, uint32_t joint) { class_mates_mark(m, joint, 2.75f); }

namespace {
// the level's own size of class_mates_mark_level (a float of its data)
float level_mark_size() {
    if (on_level(KERWAN)) return lwc1(L3_ADDR(0x00161bc4));
    if (on_level(RILGAR)) return lwc1(L5_ADDR(0x00161ee8));
    if (on_level(POKITARU)) return lwc1(L11_ADDR(0x001613c0));
    if (on_level(HOVEN)) return lwc1(L12_ADDR(0x001619a4));
    return lwc1(L6_ADDR(0x00162094));
}
}  // namespace

// (the size read after each mark, as the original does)
// orig: L3:0x002db480 L5:0x00317398 L6:0x00300f98 L11:0x002d0638 L12:0x002eb498
void class_mates_mark_level(uint32_t m, uint32_t joint) {
    if (lw(OV(0x0015f5c4)) != 2 || lbu(0x0015edb0) == 0) return;
    const uint32_t list = OV(0x0016c860);
    const uint32_t rec = OV(0x0017c440);
    for (int32_t i = 0; i < lh(list + 0x44); ++i) {
        const uint32_t o = lwu(list + 0x178 + static_cast<uint32_t>(i) * 4);
        if (lh(o + 0xa6) != lh(m + 0xa6) || lbu(rec + 1) != 0) continue;
        sub_0024f590(o, joint, rec);
        const float size = level_mark_size();
        swc1(rec + 0x28, size);
        swc1(rec + 0x20, size);
        swc1(rec + 0x24, size);
    }
}

// orig: L8:0x002d4000 L12:0x002e1758 L12:0x002e17a8 L14:0x002dae00 L18:0x002d6190 L18:0x002d61e0 L18:0x002fcbb0 L18:0x002fcc00 L18:0x002fcc50
int32_t carry_state_hook(uint32_t m, uint32_t a1, uint32_t a2, float f12, uint32_t which, uint32_t carried, int32_t released) {
    const int32_t r = which == 0 ? carry_hook0(m, a1, a2, f12) : which == 4 ? carry_hook4(m, a1, a2, f12) : carry_hook8(m, a1, a2, f12);
    if (r != 0) sb(m + 0x20, carried);
    else if (lbu(m + 0x20) == carried) sb(m + 0x20, released < 0 ? lbu(m + 0xbc) : static_cast<uint32_t>(released));
    return r;
}

// (Kerwan's copy calls its own copy of Veldin's 0x2712b8, which the overlay map pairs with it: the Veldin port here.)
// orig: L3:0x00250ae8 L10:0x00255ab0 L15:0x00251d00 L18:0x00264c10
void spark_burst(uint32_t m, uint32_t pos, uint32_t offset) {
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t at = sp, dir = sp + 0x10;
    lq_sq(at, pos);
    for (int32_t i = 0; i < float_to_int(24.0f); ++i) {
        const float jitter = rand_range_signed_f(0.0f, u2f(0x3e060a92u));
        const float angle = fpu_add(fpu_mul(int_to_float(i), u2f(0x3e860a92u)), jitter);
        vec_rotate_axis(V(dir), V(m + 0xe0), V(m + 0xc0), angle);
        vec3_set_length(V(dir), V(dir), rand_range_f(0.05f, 0.1f));
        int32_t spin = rand_range(0, 3);
        if (rand_mod(2) != 0) spin = -spin;
        const int32_t lo = frame_scale_i(0x1e);
        const int32_t life = rand_range(lo, frame_scale_i(0x5a));
        if (offset != 0) vec3_add_inplace(V(dir), V(offset));
        dev_2712b8(at, dir, static_cast<uint32_t>(life), 0x1e, 0xffffffu, static_cast<uint32_t>(spin), 60000.0f, 3000.0f,
                   u2f(0x3f59999au), u2f(0xba83126fu), u2f(0x3f59999au));
    }
    for (int32_t i = 0; i < float_to_int(6.0f); ++i) {
        const float jitter = rand_range_f(0.0f, u2f(0x3f490fdbu));
        const float angle = fpu_add(fpu_mul(int_to_float(i), u2f(0x3f860a92u)), jitter);
        vec_rotate_axis(V(dir), V(m + 0xe0), V(m + 0xc0), angle);
        vec3_set_length(V(dir), V(dir), rand_range_f(0.05f, 0.1f));
        const int32_t lo = frame_scale_i(0x14);
        const int32_t life = rand_range(lo, frame_scale_i(0x3c));
        if (offset != 0) vec3_add_inplace(V(dir), V(offset));
        spawn_particle_15_v(V(at), V(dir), 0x4f007fffu, 0x1fffffffu, static_cast<uint32_t>(life), 1, 10000.0f);
    }
    ee_stack_free(0xc0);
}

// orig: L3:0x002c6c90 L6:0x002fc630 L6:0x00303150 L6:0x00304088 L14:0x002fe228 L14:0x002ffc08 L16:0x002de3a8
uint32_t moby_state_hook(uint32_t m, uint32_t state) {
    sb(m + 0x20, state);
    return state;
}

}  // namespace rac1
