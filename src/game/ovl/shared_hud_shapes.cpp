// The space combat HUD's screen-space shapes: one port of the copies Pokitaru, Gemlik Base and Drek's fleet carry (the same
// code relinked; the ring's effect texture differs per level). The packet cursor 0x1611c0 and the GIF tags' templates
// 0x1608e0 / 0x1608f0 are the engine's (OV), the screen's offsets 0x13e510 / 0x13e514 the base ELF's (bare). The coordinates
// go out in 12.4 fixed point about the screen's corner. Header shared_hud_shapes.h.
#include <game/ovl/shared_hud_shapes.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/screen_effects.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
uint32_t cursor() { return lwu(OV(0x001611c0)); }
void cursor_set(uint32_t at) { sw(OV(0x001611c0), at); }

uint64_t sx(int32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(v)); }

// An XYZ2 word: (x, y) (in 16ths of a pixel) about the screen's offsets (less 8), the depth in the upper word; each half
// sign-extended as the original's 32-bit sums are
uint64_t screen_xyz(float x, float y, uint64_t depth) {
    const int32_t sx16 = float_to_int(fpu_mul(x, 16.0f));
    const int32_t sy16 = float_to_int(fpu_mul(y, 16.0f));
    const int32_t px = sx16 + lw(0x0013e510) - 8;
    const int32_t py = sy16 + lw(0x0013e514) - 8;
    return sx(px) | (sx(py) << 16) | depth;
}

// The strip's head: a DMA tag of the strip's quadwords, the GIF tag (the template 0x1608e0, the prim's register), the prim
// and the colour, the XYZ2 tag (0x1608f0) for `n` points; the cursor left at the points
void strip_head(int32_t n, uint64_t rgba) {
    const int32_t qw = (n + 1) / 2 + 3;
    const uint32_t at = cursor();
    sw(at, static_cast<uint32_t>(qw) | 0x10000000u);
    sw(at + 4, 0);
    sw(at + 8, 0);
    sw(at + 0xc, static_cast<uint32_t>(qw) | 0x50000000u);
    lq_sq(at + 0x10, OV(0x001608e0));
    sh(at + 0x10, 0x8001);
    sd(at + 0x20, 0x144);
    sd(at + 0x28, rgba);
    lq_sq(at + 0x30, OV(0x001608f0));
    sh(at + 0x30, static_cast<uint32_t>(n - 0x8000));
    cursor_set(at + 0x40);
}

// the cursor past the points (two to a quadword)
void strip_end(int32_t n) { cursor_set(cursor() + static_cast<uint32_t>((n + 1) / 2) * 0x10); }
}  // namespace

// orig: L11:0x00310ad0 L13:0x002b7b30 L17:0x002e9c28
void hud_line_strip(uint32_t points, int32_t n, uint32_t z, uint64_t rgba, float x, float y, float scale) {
    strip_head(n, rgba);
    const uint64_t depth = static_cast<uint64_t>(z) << 32;
    const uint32_t out = cursor();
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t p = points + static_cast<uint32_t>(i) * 4;
        const float px = fpu_add(fpu_mul(int_to_float(lh(p)), scale), x);
        const float py = fpu_add(fpu_mul(int_to_float(lh(p + 2)), scale), y);
        sd(out + static_cast<uint32_t>(i) * 8, screen_xyz(px, py, depth));
    }
    strip_end(n);
}

// orig: L13:0x002b7d50 L17:0x002e9e48
void hud_line_strip_turned(uint32_t points, int32_t n, uint32_t z, uint64_t rgba, float x, float y, float scale, float angle) {
    const uint32_t sp = ee_stack_alloc(0x120);
    const uint32_t turn = sp, angles = sp + 0x40, pt = sp + 0x50, raw = sp + 0x60;
    strip_head(n, rgba);
    V(angles) = Vec4{};
    swc1(angles + 8, angle);
    euler_to_mat4_vu(M(turn), V(angles));
    const uint64_t depth = static_cast<uint64_t>(z) << 32;
    const uint32_t out = cursor();
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t p = points + static_cast<uint32_t>(i) * 4;
        V(raw) = Vec4{};
        swc1(raw, int_to_float(lh(p)));
        swc1(raw + 4, int_to_float(lh(p + 2)));
        lq_sq(pt, raw);
        mat4_transform(V(pt), V(pt), M(turn));
        vec3_scale(V(pt), V(pt), scale);
        swc1(pt, fpu_add(lwc1(pt), x));
        swc1(pt + 4, fpu_add(lwc1(pt + 4), y));
        sd(out + static_cast<uint32_t>(i) * 8, screen_xyz(lwc1(pt), lwc1(pt + 4), depth));
    }
    strip_end(n);
    ee_stack_free(0x120);
}

// The packet: a DMA tag of 7 quadwords, the GIF tag (one loop of 0xb registers), TEX0, the prim 0x154, the colour, the
// texture's scale 0x100010, then each corner's UV and XYZ2: (x, y), along h, across w, and both.
// orig: L11:0x00310cf8 L13:0x002b7fd0 L17:0x002ea0c8
void hud_quad_turned(uint32_t u, uint32_t v, uint64_t tex0, uint32_t z, uint32_t r, uint32_t g, uint32_t b, uint32_t a,
                     float x, float y, float w, float h, float angle) {
    const uint32_t sp = ee_stack_alloc(0x150);
    const uint32_t up = sp, across = sp + 0x10, corner = sp + 0x20, c0 = sp + 0x30, c1 = sp + 0x40, c2 = sp + 0x50, c3 = sp + 0x60;
    swc1(corner, x);
    swc1(corner + 4, y);
    swc1(up, fpu_mul(h, sin_vu(angle)));
    swc1(up + 4, fpu_mul(h, cos_vu(angle)));
    swc1(across, fpu_mul(w, cos_vu(angle)));
    swc1(across + 4, fpu_mul(-w, sin_vu(angle)));
    vec3_add(V(c0), V(corner), V(up));
    vec3_sub(V(c0), V(c0), V(across));
    vec3_add(V(c1), V(corner), V(up));
    vec3_sub(V(c2), V(corner), V(across));
    lq_sq(c3, corner);
    const uint64_t colour = (r & 0xffu) | ((g & 0xffu) << 8) | ((b & 0xffu) << 16) | (static_cast<uint64_t>(a & 0xffu) << 24);
    const uint32_t at = cursor();
    sw(at, 0x10000007u);
    sw(at + 4, 0);
    sw(at + 8, 0);
    sw(at + 0xc, 0x50000007u);
    const uint32_t q = at + 0x10;
    cursor_set(q);
    sd(q, 0xb400000000008001ull);
    sd(q + 8, 0x53535353106ull);  // (the registers: TEX0, PRIM, RGBAQ, TEX1?, then UV / XYZ2 four times)
    sd(q + 0x18, 0x154);
    sd(q + 0x28, 0x100010);
    sd(q + 0x20, colour);
    sd(q + 0x10, tex0);
    const uint64_t depth = static_cast<uint64_t>(z) << 32;
    const uint64_t su = sx(static_cast<int32_t>(u << 4)), tv = sx(static_cast<int32_t>(v << 20));
    sd(q + 0x38, su);
    sd(q + 0x30, screen_xyz(lwc1(c0), lwc1(c0 + 4), depth));
    sd(q + 0x48, tv);
    sd(q + 0x40, screen_xyz(lwc1(c1), lwc1(c1 + 4), depth));
    sd(q + 0x58, sx(static_cast<int32_t>((v << 20) + (u << 4))));
    sd(q + 0x50, screen_xyz(lwc1(c2), lwc1(c2 + 4), depth));
    sd(q + 0x68, 0);
    sd(q + 0x60, screen_xyz(lwc1(c3), lwc1(c3 + 4), depth));
    cursor_set(cursor() + 0x70);
    ee_stack_free(0x150);
}

// orig: L11:0x00311048 L13:0x002b8320 L17:0x002ea418
void hud_ring(uint32_t r, uint32_t g, uint32_t b, uint32_t a, float x, float y, float size, float angle) {
    const int32_t texture = on_level(GEMLIK) ? 0x37 : on_level(VELDIN_ORBIT) ? 0x2e : 0x42;  // (the level's effect texture)
    const uint64_t tex0 = effect_tex0(texture);
    const float side = fpu_mul(size, 20.0f);
    const float quarter = u2f(0x3fc90fdbu);  // (pi / 2)
    for (int32_t k = 0; k < 4; ++k) {
        hud_quad_turned(0x3f, 0x3f, tex0, 0xfffff3, r & 0xffu, g & 0xffu, b & 0xffu, a & 0xffu, x, y, side, side, angle);
        angle = angle_add(angle, quarter);
    }
}

// orig: L11:0x00311210 L13:0x002b84e8 L17:0x002ea5e0
void hud_screen_point(uint32_t pos, uint32_t out_x, uint32_t out_y, int32_t add_y) {
    const uint32_t sp = ee_stack_alloc(0x70);
    screen_project(sp, pos);
    sw(out_x, static_cast<uint32_t>(float_to_int(fpu_mul(fpu_sub(lwc1(sp), int_to_float(lw(0x0013e510))), 0.0625f))));
    sw(out_y, static_cast<uint32_t>(float_to_int(fpu_mul(fpu_sub(lwc1(sp + 4), int_to_float(lw(0x0013e514))), 0.0625f)) + add_y));
    ee_stack_free(0x70);
}

}  // namespace rac1
