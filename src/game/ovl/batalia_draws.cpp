// Batalia (level 8) own draw callbacks and banner / flame-sheet class, hand-ported: the anti-air turret's HUD draw
// callback (class 440, L8:0x002deee0), the Bolt Thief's spinning halo quad (class 452, L8:0x002e5188), and class 671's
// update, draw callback and two helpers (a waving cloth / flame curtain drawn with quad_draw, optional embers and sparks,
// and an optional dynamic light). Registered in batalia_mobys.cpp and batalia_part2.cpp. Addresses are level 8's (`orig:
// L8:`); its own data and callbacks are L8_ADDR, the base ELF's are bare.
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/batalia_own.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/nanotech_draw.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_16_spawn.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants_l8.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint64_t kHudZ = 0x00fffff000000000ull;

void zero_q(uint32_t a) { sd(a, 0); sd(a + 8, 0); }

uint64_t s32_to_u64(uint32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v))); }
uint64_t u32_to_u64(uint32_t v) { return static_cast<uint64_t>(v); }

// Packs a 2D GS XYZ2 vertex at kHudZ: `x_pix` in pixels (shifted by 4), `y16` already in 1/16ths.
uint64_t pack_hud_px(int32_t x_pix, int32_t y16) {
    const uint32_t yu = (static_cast<uint32_t>(y16) << 16) - 0x80000000u;
    const uint32_t xu = (static_cast<uint32_t>(x_pix) << 4) + 0x8000u;
    return kHudZ + u32_to_u64(yu) + s32_to_u64(xu);
}

// Packs a 2D GS XYZ2 vertex at kHudZ where both `x16` and `y16` are already in 1/16ths.
uint64_t pack_hud_16(int32_t x16, int32_t y16) {
    const uint32_t yu = (static_cast<uint32_t>(y16) << 16) - 0x80000000u;
    const uint32_t xu = static_cast<uint32_t>(x16) + 0x8000u;
    return kHudZ + u32_to_u64(yu) + s32_to_u64(xu);
}

void fill_quad_cols(uint32_t cols, uint32_t c) {
    sw(cols, c);
    sw(cols + 0xc, c);
    sw(cols + 8, c);
    sw(cols + 4, c);
}

// Draws a symmetric four-quadrant frame centred on `(cx, cy)` with half-extents `(hw, hh)`.
void draw_hud_frame(uint32_t pts, uint32_t uvs, uint32_t cols, uint64_t tex0, uint32_t cx_a, uint32_t cy_a,
                    uint32_t hw_a, uint32_t hh_a) {
    for (int32_t q = 0; q < 4; ++q) {
        const float sx = (q & 1) == 0 ? 1.0f : -1.0f;
        const float sy = q < 2 ? 1.0f : -1.0f;
        const int32_t y_out = float_to_int(fpu_mul(fpu_add(lwc1(cy_a), fpu_mul(lwc1(hh_a), sy)), 16.0f));
        const int32_t x_out = float_to_int(fpu_add(lwc1(cx_a), fpu_mul(lwc1(hw_a), sx)));
        const int32_t y_mid = float_to_int(fpu_mul(lwc1(cy_a), 16.0f));
        const int32_t x_out2 = float_to_int(fpu_add(lwc1(cx_a), fpu_mul(lwc1(hw_a), sx)));
        const int32_t y_out2 = float_to_int(fpu_mul(fpu_add(lwc1(cy_a), fpu_mul(lwc1(hh_a), sy)), 16.0f));
        const int32_t x_mid = float_to_int(lwc1(cx_a));
        const int32_t y_mid2 = float_to_int(fpu_mul(lwc1(cy_a), 16.0f));
        const int32_t x_mid2 = float_to_int(lwc1(cx_a));
        sd(pts, pack_hud_px(x_out, y_out));
        sd(pts + 8, pack_hud_px(x_out2, y_mid));
        sd(pts + 0x10, pack_hud_px(x_mid, y_out2));
        sd(pts + 0x18, pack_hud_px(x_mid2, y_mid2));
        gs_quad_packet(pts, uvs, cols, tex0, 1);
    }
}

// Draws one radar blip centred on `(cx + dx/16, cy + dy/16)` with half-extents `(bw, bh)`.
void draw_radar_blip(uint32_t pts, uint32_t uvs, uint32_t cols, uint64_t tex0, uint32_t cx_a, uint32_t cy_a,
                     uint32_t bw_a, uint32_t bh_a, float dx, float dy) {
    const int32_t y0 = float_to_int(fpu_add(fpu_mul(fpu_add(lwc1(cy_a), lwc1(bh_a)), 16.0f), dy));
    const int32_t x0 = float_to_int(fpu_add(fpu_mul(fpu_add(lwc1(cx_a), lwc1(bw_a)), 16.0f), dx));
    const int32_t y1 = float_to_int(fpu_add(fpu_mul(fpu_sub(lwc1(cy_a), lwc1(bh_a)), 16.0f), dy));
    const int32_t x1 = float_to_int(fpu_add(fpu_mul(fpu_add(lwc1(cx_a), lwc1(bw_a)), 16.0f), dx));
    const int32_t y2 = float_to_int(fpu_add(fpu_mul(fpu_add(lwc1(cy_a), lwc1(bh_a)), 16.0f), dy));
    const int32_t x2 = float_to_int(fpu_add(fpu_mul(fpu_sub(lwc1(cx_a), lwc1(bw_a)), 16.0f), dx));
    const int32_t y3 = float_to_int(fpu_add(fpu_mul(fpu_sub(lwc1(cy_a), lwc1(bh_a)), 16.0f), dy));
    const int32_t x3 = float_to_int(fpu_add(fpu_mul(fpu_sub(lwc1(cx_a), lwc1(bw_a)), 16.0f), dx));
    sd(pts, pack_hud_16(x0, y0));
    sd(pts + 8, pack_hud_16(x1, y1));
    sd(pts + 0x10, pack_hud_16(x2, y2));
    sd(pts + 0x18, pack_hud_16(x3, y3));
    gs_quad_packet(pts, uvs, cols, tex0, 1);
}

// Draws one rotated reticle ring quad (`angle * mul` + 0, pi/2, -pi/2, pi) with radii `(rx_a, ry_a)` and texture `*tex_a`.
void draw_reticle_dial(uint32_t pts, uint32_t uvs, uint32_t cols, uint32_t angle_a, float mul, uint32_t rx_a,
                       uint32_t ry_a, uint32_t tex_a) {
    const float offs[4] = {0.0f, u2f(0x3fc90fdbu), u2f(0xbfc90fdbu), u2f(0x40490fdbu)};
    for (uint32_t k = 0; k < 4; ++k) {
        const float ang = angle_wrap(angle_add(fpu_mul(lwc1(angle_a), mul), offs[k]));
        const int32_t y16 = float_to_int(fpu_mul(fpu_mul(sin_vu(ang), lwc1(ry_a)), 16.0f));
        const int32_t x16 = float_to_int(fpu_mul(fpu_mul(cos_vu(ang), lwc1(rx_a)), 16.0f));
        sd(pts + k * 8, pack_hud_16(x16, y16));
    }
    gs_quad_packet(pts, uvs, cols, static_cast<uint64_t>(effect_tex0(lw(tex_a))), 1);
}
}  // namespace

// Class 440's HUD draw callback: draws the outer radar frame, incoming missile blips (clearing dead missile slots), live
// gunship blips, the inner reticle frame, the 32-segment health ring, and the yaw and pitch lock-on dials.
// orig: L8:0x002deee0
void bat_class440_draw_l8(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(336);
    const uint32_t pts = sp, cols = sp + 0x20, uvs = sp + 0x30;
    const uint32_t pv = lwu(m + 0x78);

    sub_002a0ba8(0x42, (0x8000ull << 24) | 0x44);
    uint64_t tex0 = static_cast<uint64_t>(effect_tex0(lw(L8_ADDR(0x00161ba0)) + 0x28));
    sw(uvs, lwu(L8_ADDR(0x00161bb0)));
    sw(uvs + 4, lwu(L8_ADDR(0x00161bb4)));
    sw(uvs + 8, lwu(L8_ADDR(0x00161bb8)));
    sw(uvs + 0xc, lwu(L8_ADDR(0x00161bbc)));
    fill_quad_cols(cols, lwu(L8_ADDR(0x00161ac8)));
    draw_hud_frame(pts, uvs, cols, tex0, L8_ADDR(0x00161b80), L8_ADDR(0x00161b84), L8_ADDR(0x00161b88),
                   L8_ADDR(0x00161b8c));

    // Incoming missiles (slots +0x60 .. +0x7c)
    tex0 = static_cast<uint64_t>(effect_tex0(lw(L8_ADDR(0x00161ba4))));
    sw(uvs, lwu(L8_ADDR(0x00161bc0)));
    sw(uvs + 4, lwu(L8_ADDR(0x00161bc4)));
    sw(uvs + 8, lwu(L8_ADDR(0x00161bc8)));
    sw(uvs + 0xc, lwu(L8_ADDR(0x00161bcc)));
    for (uint32_t i = 0; i < 8; ++i) {
        const uint32_t slot = pv + 0x60 + i * 4;
        const uint32_t shot = lwu(slot);
        if (shot == 0) continue;
        if (static_cast<int8_t>(lbu(shot + 0x20)) < 0) {
            sw(slot, 0);
            continue;
        }
        uint32_t col = lwu(L8_ADDR(0x00161adc));
        if (lbu(shot + 0x20) == 2) {
            col = (lwu(L8_ADDR(0x0015f5cc)) & 0x10u) != 0 ? lwu(L8_ADDR(0x00161ad4)) : 0x60000000u;
        }
        fill_quad_cols(cols, col);
        const float to_shot = atan2_f(fpu_sub(lwc1(shot + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(shot + 0x14), lwc1(m + 0x14)));
        const float ang = angle_sub(to_shot, lwc1(m + 0x48));
        const float dist = vec3_distance(V(shot + 0x10), V(m + 0x10));
        const float r = fpu_mul(dist, lwc1(L8_ADDR(0x00161ac0)));
        const float mirror = static_cast<float>(lbu(0x0015edb4) != 0 ? 1 : -1);
        const float dx = fpu_mul(fpu_mul(mirror, sin_vu(ang)), r);
        const float dy = fpu_mul(-cos_vu(ang), r);
        draw_radar_blip(pts, uvs, cols, tex0, L8_ADDR(0x00161b80), L8_ADDR(0x00161b84), L8_ADDR(0x00161b90),
                        L8_ADDR(0x00161b94), dx, dy);
    }

    // Live gunships (indices +0x80 .. +0x8c)
    for (uint32_t i = 0; i < 4; ++i) {
        const uint32_t idx = lwu(pv + 0x80 + i * 4);
        const uint32_t g = lwu(L8_ADDR(0x0015ffd8)) + (idx << 8);
        if (lbu(g + 0x20) >= 0x7fu) continue;
        fill_quad_cols(cols, lwu(L8_ADDR(0x00161ad8)));
        const float to_ship = atan2_f(fpu_sub(lwc1(g + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(g + 0x14), lwc1(m + 0x14)));
        const float ang = angle_sub(to_ship, lwc1(m + 0x48));
        const uint32_t g2 = lwu(L8_ADDR(0x0015ffd8)) + (lwu(pv + 0x80 + i * 4) << 8);
        const float r = fpu_mul(vec3_distance(V(g2 + 0x10), V(m + 0x10)), lwc1(L8_ADDR(0x00161ac0)));
        const float dx = fpu_mul(-sin_vu(ang), r);
        const float dy = fpu_mul(-cos_vu(ang), r);
        draw_radar_blip(pts, uvs, cols, tex0, L8_ADDR(0x00161b80), L8_ADDR(0x00161b84), L8_ADDR(0x00161b98),
                        L8_ADDR(0x00161b9c), dx, dy);
    }

    // Inner reticle frame
    fill_quad_cols(cols, lwu(L8_ADDR(0x00161ac8)));
    sw(uvs, lwu(L8_ADDR(0x00161bb0)));
    sw(uvs + 4, lwu(L8_ADDR(0x00161bb4)));
    sw(uvs + 8, lwu(L8_ADDR(0x00161bb8)));
    sw(uvs + 0xc, lwu(L8_ADDR(0x00161bbc)));
    tex0 = static_cast<uint64_t>(effect_tex0(lw(L8_ADDR(0x00161ba0)) + 0x28));
    draw_hud_frame(pts, uvs, cols, tex0, L8_ADDR(0x00161b60), L8_ADDR(0x00161b64), L8_ADDR(0x00161b68),
                   L8_ADDR(0x00161b6c));

    // 32-segment health ring
    tex0 = static_cast<uint64_t>(effect_tex0(lw(L8_ADDR(0x00161ba4))));
    sw(uvs, lwu(L8_ADDR(0x00161bd0)));
    sw(uvs + 4, lwu(L8_ADDR(0x00161bd4)));
    sw(uvs + 8, lwu(L8_ADDR(0x00161bd8)));
    sw(uvs + 0xc, lwu(L8_ADDR(0x00161bdc)));
    const float hp = static_cast<float>(lw(pv + 0x58));
    const uint32_t full_c =
        100.0f < hp ? rgba_lerp(0x6000ffffu, 0x6000ff00u, fpu_div(fpu_sub(hp, 100.0f), 100.0f))
                    : rgba_lerp(0x600000ffu, 0x6000ffffu, fpu_div(hp, 100.0f));
    const int32_t gone = float_to_int(fpu_mul(fpu_sub(1.0f, fpu_div(static_cast<float>(lw(pv + 0x58)), 200.0f)), 32.0f));
    const uint32_t dim_c = (full_c & 0xffffffu) + 0x20000000u;
    const float step_ang = u2f(0x3e490fdbu);  // (pi / 16)
    for (int32_t s = 0; s < 32; ++s) {
        fill_quad_cols(cols, s < gone ? dim_c : full_c);
        const float a0 = angle_wrap(fpu_mul(static_cast<float>(s), step_ang));
        const float a1 = angle_add(a0, step_ang);
        const int32_t x0 = float_to_int(fpu_mul(lwc1(L8_ADDR(0x00161b70)), sin_vu(a0)));
        const int32_t y0 = float_to_int(fpu_mul(lwc1(L8_ADDR(0x00161b74)), -cos_vu(a0)));
        const int32_t x1 = float_to_int(fpu_mul(lwc1(L8_ADDR(0x00161b70)), sin_vu(a1)));
        const int32_t y1 = float_to_int(fpu_mul(lwc1(L8_ADDR(0x00161b74)), -cos_vu(a1)));
        const int32_t py0 = float_to_int(fpu_mul(fpu_add(lwc1(L8_ADDR(0x00161b64)), static_cast<float>(y1)), 16.0f));
        const int32_t px0 = float_to_int(fpu_add(lwc1(L8_ADDR(0x00161b60)), static_cast<float>(x1)));
        const int32_t py1 = float_to_int(fpu_mul(lwc1(L8_ADDR(0x00161b64)), 16.0f));
        const int32_t px1 = float_to_int(lwc1(L8_ADDR(0x00161b60)));
        const int32_t py2 = float_to_int(fpu_mul(fpu_add(lwc1(L8_ADDR(0x00161b64)), static_cast<float>(y0)), 16.0f));
        const int32_t px2 = float_to_int(fpu_add(lwc1(L8_ADDR(0x00161b60)), static_cast<float>(x0)));
        const int32_t py3 = float_to_int(fpu_mul(lwc1(L8_ADDR(0x00161b64)), 16.0f));
        const int32_t px3 = float_to_int(lwc1(L8_ADDR(0x00161b60)));
        sd(pts, pack_hud_px(px0, py0));
        sd(pts + 8, pack_hud_px(px1, py1));
        sd(pts + 0x10, pack_hud_px(px2, py2));
        sd(pts + 0x18, pack_hud_px(px3, py3));
        gs_quad_packet(pts, uvs, cols, tex0, 1);
    }

    // Lock-on yaw and pitch dials
    uint32_t lc0 = 0x5000ff00u, lc1 = 0x5000ffffu;
    float lock_num = 0.0f;
    float lock_den = 1.0f;
    if (frame_scale_i(0xf) < lw(pv + 0xa0)) {
        const int32_t half = frame_scale_i(0xf);
        lock_num = int_to_float(lw(pv + 0xa0) - half);
        lock_den = static_cast<float>(frame_scale_i(0xf));
        lc0 = 0x5000ffffu;
        lc1 = 0x500000ffu;
    } else {
        lock_num = int_to_float(lw(pv + 0xa0));
        lock_den = static_cast<float>(frame_scale_i(0xf));
    }
    fill_quad_cols(cols, rgba_lerp(lc0, lc1, fpu_div(lock_num, lock_den)));
    sw(uvs, lwu(L8_ADDR(0x00161be0)));
    sw(uvs + 4, lwu(L8_ADDR(0x00161be4)));
    sw(uvs + 8, lwu(L8_ADDR(0x00161be8)));
    sw(uvs + 0xc, lwu(L8_ADDR(0x00161bec)));
    draw_reticle_dial(pts, uvs, cols, m + 0x48, 4.0f, L8_ADDR(0x00161b78), L8_ADDR(0x00161b7c), L8_ADDR(0x00161ba8));
    draw_reticle_dial(pts, uvs, cols, m + 0x44, 8.0f, L8_ADDR(0x00161b78), L8_ADDR(0x00161b7c), L8_ADDR(0x00161bac));

    ee_stack_free(336);
}

// Class 452's glow draw callback: projects the Bolt Thief's offset point into screen space and emits one additive quad
// (texture 0x13) spun by pvars +0x74 and coloured by +0x78 when +0x6c is non-zero.
// orig: L8:0x002e5188
void bat_class452_draw_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lh(pv + 0x6c) == 0) return;
    const uint32_t sp = ee_stack_alloc(272);
    const uint32_t pts = sp, cols = sp + 0x20, uvs = sp + 0x30, at = sp + 0x40, tmp = sp + 0x50;

    lq_sq(at, m + 0x10);
    vec3_set_length(V(tmp), V(m + 0xe0), lwc1(L8_ADDR(0x00161c5c)));
    vec3_add(V(at), V(at), V(tmp));
    vec3_set_length(V(tmp), V(m + 0xc0), lwc1(L8_ADDR(0x00161c60)));
    vec3_add(V(at), V(at), V(tmp));
    vec3_sub(V(at), V(at), V(L8_ADDR(0x001675c0)));
    float dist = vec3_length(V(at));
    swc1(at + 0xc, 1.0f);
    vec3_scale(V(at), V(at), 1024.0f);
    mat4_transform(V(at), V(at), M(L8_ADDR(0x001674c0)));
    vec3_scale(V(at), V(at), fpu_div(lwc1(L8_ADDR(0x0016d450)), lwc1(at + 0xc)));

    const uint32_t sx = static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(at), 16.0f))) + 0x8000u;
    const uint32_t sy_hi = (static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(at + 4), 16.0f))) + 0x8000u) << 16;
    const uint32_t sz_i = static_cast<uint32_t>(
        float_to_int(fpu_add(fpu_mul(lwc1(at + 8), u2f(0x3f7fec57u)), lwc1(L8_ADDR(0x0016d3e8)))));
    const uint64_t z_hi = static_cast<uint64_t>(sz_i) << 32;

    if (16.0f < dist) dist = 16.0f;
    else if (dist < 4.0f) dist = 4.0f;

    const int32_t raw_sz = static_cast<int32_t>(static_cast<int8_t>(lbu(pv + 0x7b))) + 0x10;
    const float radius = fpu_mul(fpu_mul(int_to_float(raw_sz), 0.015625f), fpu_sub(50.0f, fpu_mul(dist, 2.5f)));

    const float a0 = lwc1(pv + 0x74);
    const float dx0 = fpu_mul(radius, sin_vu(a0));
    const float dy0 = fpu_mul(radius, cos_vu(lwc1(pv + 0x74)));
    const float a1_s = angle_add(lwc1(pv + 0x74), u2f(0x3fc90fdbu));
    const float dx1 = fpu_mul(radius, sin_vu(a1_s));
    const float a1_c = angle_add(lwc1(pv + 0x74), u2f(0x3fc90fdbu));
    const float dy1 = fpu_mul(radius, cos_vu(a1_c));
    const float a2_s = angle_add(lwc1(pv + 0x74), u2f(0x4096cbe4u));
    const float dx2 = fpu_mul(radius, sin_vu(a2_s));
    const float a2_c = angle_add(lwc1(pv + 0x74), u2f(0x4096cbe4u));
    const float dy2 = fpu_mul(radius, cos_vu(a2_c));
    const float a3_s = angle_add(lwc1(pv + 0x74), u2f(0x40490fdbu));
    const float dx3 = fpu_mul(radius, sin_vu(a3_s));
    const float a3_c = angle_add(lwc1(pv + 0x74), u2f(0x40490fdbu));
    const float dy3 = fpu_mul(radius, cos_vu(a3_c));

    sw(uvs + 4, 0x200u);
    sw(uvs + 8, 0x02000000u);
    sw(uvs + 0xc, 0x02000200u);
    sw(uvs, 0);
    fill_quad_cols(cols, lwu(pv + 0x78));

    const float dxs[4] = {dx0, dx1, dx2, dx3};
    const float dys[4] = {dy0, dy1, dy2, dy3};
    for (uint32_t k = 0; k < 4; ++k) {
        uint64_t v = z_hi;
        v += s32_to_u64((static_cast<uint32_t>(float_to_int(dys[k])) << 20) + sy_hi);
        v += s32_to_u64((static_cast<uint32_t>(float_to_int(dxs[k])) << 4) + sx);
        sd(pts + k * 8, v);
    }

    sub_002a0ba8(0x42, (0x8000ull << 24) | 0x48);
    gs_quad_packet(pts, uvs, cols, static_cast<uint64_t>(effect_tex0(0x13)), 1);
    sub_002a0ba8(0x42, (0x8000ull << 24) | 0x44);
    ee_stack_free(272);
}

// Spawns one spark particle (kind 0x2b, texture *0x1b292c) for class 671.
// orig: L8:0x0027b450
uint32_t banner_spark_spawn_l8(uint32_t owner, uint32_t pos, uint32_t vel, int16_t life, uint8_t r, uint8_t g,
                               uint8_t b, uint8_t a, float size) {
    const uint32_t p = alloc_particle(0x2b);
    if (p == 0) return 0;
    lq_sq(p + 0x10, pos);
    lq_sq(p + 0x20, vel);
    sb(p + 0x37, a);
    sb(p + 0x34, r);
    sb(p + 0x35, g);
    sb(p + 0x36, b);
    sw(p + 4, (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(r));
    sb(p + 1, 0);
    sb(p + 3, 0x48);
    sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x40));
    sb(p + 8, static_cast<uint32_t>(libc_rand()));
    swc1(p + 0xc, size);
    sh(p + 0xa, 0);
    sb(p + 2, lbu(lwu(L8_ADDR(0x001b292c))));
    sh(p + 0x38, static_cast<uint32_t>(frame_scale_i(life)));
    sw(p + 0x30, owner);
    return p;
}

// Samples the 8-entry circular wind flutter table at `pv + 0x90` at parameter `t`.
// orig: L8:0x002f5d98
float banner_wind_sample_l8(uint32_t pv, float t) {
    const float scaled = fpu_mul(t, 8.0f);
    const int32_t idx = float_to_int(scaled);
    const float frac = fpu_sub(scaled, int_to_float(idx));
    const int32_t i0 = idx % 8;
    const int32_t i1 = (i0 + 1) % 8;
    const uint32_t tab = pv + 0x90;
    const float v0 = lwc1(tab + static_cast<uint32_t>(i0 << 2));
    const float v1 = lwc1(tab + static_cast<uint32_t>(i1 << 2));
    return fpu_add(fpu_mul(v0, fpu_sub(1.0f, frac)), fpu_mul(v1, frac));
}

// Class 671's draw callback: steps the 8 wind weights, builds the `(rows + 1) x (cols + 1)` displaced vertex grid and UV
// arrays on the EE stack, draws each cell with quad_draw, and when flag bit 2 of +0x0a is set updates its ember / spark
// emitters and dynamic light before clearing bit 2.
// orig: L8:0x002f5e58
void bat_class671_draw_l8(uint32_t m) {
    const uint32_t s8 = ee_stack_alloc(976);
    if (m == 0) {
        ee_stack_free(976);
        return;
    }
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) {
        ee_stack_free(976);
        return;
    }

    const uint32_t cols = lbu(pv + 8), rows = lbu(pv + 9);
    const uint32_t row_stride = (cols + 1) << 4;
    const uint32_t grid_bytes = (((cols + 1) << 7) * (rows + 1)) >> 3;
    const uint32_t grid = ee_stack_alloc(grid_bytes);
    const float inv_cols = fpu_div(1.0f, int_to_float(static_cast<int32_t>(cols)));
    const float inv_rows = fpu_div(1.0f, int_to_float(static_cast<int32_t>(rows)));
    const uint32_t u_bytes = ((cols << 2) + 0x13u) & 0x7f0u;
    const uint32_t v_bytes = ((rows << 2) + 0x13u) & 0x7f0u;
    const uint32_t u_arr = ee_stack_alloc(u_bytes);
    const uint32_t v_arr = ee_stack_alloc(v_bytes);
    const uint32_t total_alloc = 976u + grid_bytes + u_bytes + v_bytes;
    const float frame_f = int_to_float(lw(L8_ADDR(0x0015f3f8)));

    // 1. Step the 8 wind flutter entries
    for (uint32_t i = 0; i < 8; ++i) {
        const uint32_t cur_a = pv + 0x90 + i * 4, goal_a = pv + 0xb0 + i * 4;
        const float cur = lwc1(cur_a), goal = lwc1(goal_a);
        const float step = fpu_mul(lwc1(pv + 0x88), lwc1(pv + 0x8c));
        bool reached = false;
        if (cur < goal) {
            const float next = fpu_add(cur, step);
            swc1(cur_a, next);
            reached = lwc1(goal_a) <= next;
        } else {
            const float next = fpu_sub(cur, step);
            swc1(cur_a, next);
            reached = next <= lwc1(goal_a);
        }
        if (reached) swc1(goal_a, rand_range_f(fpu_sub(1.0f, lwc1(pv + 0x88)), 1.0f));
    }

    // 2. Alpha and quad descriptor state
    const float a_pvar = int_to_float(lbu(pv + 0x13));
    const float a_moby = int_to_float(lbu(m + 0x23));
    const uint32_t base_alpha =
        static_cast<uint32_t>(float_to_int(fpu_mul(fpu_mul(fpu_mul(lwc1(pv + 0x154), a_pvar), a_moby), 0.0078125f))) &
        0xffu;

    uint32_t tex_idx = lbu(pv + 0xc);
    if (lbu(pv + 0xd) != 0) {
        const int32_t q = ee_div(lw(L8_ADDR(0x0015f3f8)), static_cast<int32_t>(lbu(pv + 0xe) + 1));
        tex_idx = lbu(pv + 0xc) + static_cast<uint32_t>(ee_rem(q, static_cast<int32_t>(lbu(pv + 0xd))));
    }
    sb(pv + 0xf, tex_idx);
    const int32_t tex_arg =
        (lhu(pv + 0xa) & 1u) != 0 ? static_cast<int32_t>(lbu(pv + 0xf)) : static_cast<int32_t>(lbu(pv + 0xf) + 0x28);

    const uint32_t desc = s8 + 0x100;
    sd(desc + 0x78, static_cast<uint64_t>(effect_tex0(tex_arg)));
    sd(desc + 0x70, 0);
    sd(desc + 0x80, (0xff90ull << 32) | 0x260ull);
    const uint64_t blend = static_cast<uint64_t>(lbu(pv + 0x3c) | (lbu(pv + 0x3d) << 2) | (lbu(pv + 0x3e) << 4) |
                                                 (lbu(pv + 0x3f) << 6)) |
                           (static_cast<uint64_t>(lbu(pv + 0x13)) << 32);
    sd(desc + 0x88, blend);
    const uint32_t col0 = (static_cast<uint32_t>(lbu(pv + 0x13)) << 24) | (static_cast<uint32_t>(lbu(pv + 0x11)) << 16) |
                          (static_cast<uint32_t>(lbu(pv + 0x12)) << 8) | static_cast<uint32_t>(lbu(pv + 0x10));
    sw(desc + 0x4c, col0);
    sw(desc + 0x40, col0);
    sw(desc + 0x44, col0);
    sw(desc + 0x48, col0);

    // 3. Banner corner basis vectors
    const uint32_t c0 = s8, c1 = s8 + 0x10, c2 = s8 + 0x20, c3 = s8 + 0x30;
    const uint32_t step_col = s8 + 0x40, down_l = s8 + 0x50, down_r = s8 + 0x60;
    const uint32_t row_l = s8 + 0x70, row_r = s8 + 0x80, pt_cur = s8 + 0x90;
    const uint32_t across_top = s8 + 0xa0, normal = s8 + 0xb0, rot_m = s8 + 0xc0;
    zero_q(c0);
    zero_q(c1);
    zero_q(c2);
    zero_q(c3);
    const float half_w = fpu_mul(lwc1(pv), 0.5f);
    swc1(c3, half_w);
    swc1(c1, half_w);
    swc1(c0, -half_w);
    swc1(c2, -half_w);
    swc1(c0 + 8, fpu_add(lwc1(pv + 0x80), u2f(0x3c23d70au)));
    swc1(c1 + 8, fpu_add(lwc1(pv + 0x84), u2f(0x3c23d70au)));
    euler_to_mat4_vu(M(rot_m), V(m + 0x40));
    mat4_transform(V(c0), V(c0), M(rot_m));
    mat4_transform(V(c1), V(c1), M(rot_m));
    mat4_transform(V(c2), V(c2), M(rot_m));
    mat4_transform(V(c3), V(c3), M(rot_m));

    vec3_sub(V(down_l), V(c2), V(c0));
    vec3_scale(V(down_l), V(down_l), inv_rows);
    vec3_sub(V(down_r), V(c3), V(c1));
    vec3_scale(V(down_r), V(down_r), inv_rows);

    const float d14 = fpu_mul(fpu_sub(lwc1(pv + 0x18), lwc1(pv + 0x14)), inv_rows);
    const float d1c = fpu_mul(fpu_sub(lwc1(pv + 0x24), lwc1(pv + 0x1c)), inv_rows);
    const float d20 = fpu_mul(fpu_sub(lwc1(pv + 0x28), lwc1(pv + 0x20)), inv_rows);
    const float d60 = fpu_mul(fpu_sub(lwc1(pv + 0x68), lwc1(pv + 0x60)), inv_cols);
    const float sag_l = fpu_sub(lwc1(pv + 4), lwc1(pv + 0x80));
    const float sag_r = fpu_sub(lwc1(pv + 4), lwc1(pv + 0x84));
    const float d64 = fpu_mul(fpu_sub(lwc1(pv + 0x6c), lwc1(pv + 0x64)), inv_rows);
    const float wind0 = lwc1(pv + 0x90);

    vec3_sub(V(across_top), V(c1), V(c0));
    vec3_cross_ba(V(normal), V(across_top), V(down_l));
    vec3_set_length(V(normal), V(normal), 1.0f);
    lq_sq(row_l, c0);
    lq_sq(row_r, c1);

    // 4. Displaced vertex grid
    float p14 = lwc1(pv + 0x14), p1c = lwc1(pv + 0x1c), p20 = lwc1(pv + 0x20);
    float p60 = lwc1(pv + 0x60), p64 = lwc1(pv + 0x64);
    const float pi = u2f(0x40490fdbu);
    const uint32_t disp_x = s8 + 0x190, disp_y = s8 + 0x1a0, disp_sum = s8 + 0x1b0;
    const uint32_t disp_n = s8 + 0x1c0, disp_sag = s8 + 0x1d0;

    for (uint32_t r = 0; r <= rows; ++r) {
        const float rv = fpu_div(int_to_float(static_cast<int32_t>(r)), int_to_float(static_cast<int32_t>(rows)));
        lq_sq(pt_cur, row_l);
        const float next_p14 = fpu_add(p14, d14);
        const float next_p20 = fpu_add(p20, d20);
        const float next_p60 = fpu_add(p60, d60);
        const float next_p1c = fpu_add(p1c, d1c);
        const float next_p64 = fpu_add(p64, d64);
        vec3_sub(V(step_col), V(row_r), V(row_l));
        vec3_scale(V(step_col), V(step_col), inv_cols);
        const float wind_r = banner_wind_sample_l8(pv, rv);
        const uint32_t row_off = r * row_stride;

        for (uint32_t c = 0; c <= cols; ++c) {
            const float cu = fpu_div(int_to_float(static_cast<int32_t>(c)), int_to_float(static_cast<int32_t>(cols)));
            const float sw_ang = angle_wrap_mod(fpu_mul(lwc1(pv + 0x5c), frame_f));
            const float u_pi = fpu_mul(cu, pi);
            const float sw_s = sin_vu(sw_ang);
            float base_ph = fpu_add(fpu_mul(frame_f, p14), fpu_mul(lwc1(pv + 0x58), sw_s));
            const float wind_c = banner_wind_sample_l8(pv, cu);

            const float w_sum = fpu_add(fpu_add(fpu_mul(wind_c, lwc1(pv + 0xd0)), fpu_mul(wind_r, lwc1(pv + 0xd4))),
                                        fpu_mul(wind0, lwc1(pv + 0xd8)));
            base_ph = fpu_add(base_ph, w_sum);
            float sag = fpu_add(fpu_mul(sag_l, cu), fpu_mul(sag_r, fpu_sub(1.0f, cu)));

            const float ph_x = angle_wrap_mod(
                fpu_add(fpu_add(fpu_add(base_ph, p60), fpu_mul(lwc1(pv + 0x2c), cu)), fpu_mul(lwc1(pv + 0x34), rv)));
            const float ph_y = angle_wrap_mod(
                fpu_add(fpu_add(fpu_add(base_ph, p64), fpu_mul(lwc1(pv + 0x30), rv)), fpu_mul(lwc1(pv + 0x38), cu)));

            const float arch = sqrt_f(sin_vu(u_pi));
            sag = fpu_mul(fpu_mul(sag, arch), fpu_sub(1.0f, rv));
            vec3_set_length(V(disp_x), V(step_col), fpu_mul(fpu_mul(fpu_mul(p1c, cos_vu(ph_x)), arch), wind_c));
            vec3_set_length(V(disp_y), V(down_l), fpu_mul(fpu_mul(fpu_mul(p20, sin_vu(ph_y)), arch), wind_c));

            const float env = fpu_mul(sin_vu(fpu_mul(rv, pi)), sin_vu(u_pi));
            const float n_u = sin_vu(angle_wrap_mod(fpu_add(base_ph, fpu_mul(lwc1(pv + 0x2c), cu))));
            const float n_v = sin_vu(angle_wrap_mod(fpu_add(base_ph, fpu_mul(lwc1(pv + 0x34), rv))));
            const float n_amt = fpu_mul(env, fpu_add(fpu_mul(lwc1(pv + 0x78), n_u), fpu_mul(lwc1(pv + 0x7c), n_v)));
            vec3_set_length(V(disp_n), V(normal), n_amt);
            vec3_set_length(V(disp_sag), V(down_l), sag);

            vec3_add(V(disp_sum), V(disp_x), V(disp_y));
            vec3_add(V(disp_sum), V(disp_sum), V(disp_n));
            vec3_sub(V(disp_sum), V(disp_sum), V(disp_sag));

            const uint32_t vtx = grid + row_off + (c << 4);
            vec3_add(V(vtx), V(pt_cur), V(disp_sum));

            const float k0 = lwc1(pv + 0x158), k1 = lwc1(pv + 0x15c);
            const float a_term = fpu_add(fpu_sub(1.0f, k0), fpu_mul(fpu_mul(fpu_mul(k0, arch), arch), arch));
            const float b_term = fpu_add(fpu_sub(1.0f, k1), fpu_mul(k1, rv));
            swc1(vtx + 0xc, fpu_div(1.0f, fpu_mul(a_term, b_term)));
            vec3_add(V(pt_cur), V(pt_cur), V(step_col));
        }

        vec3_add(V(row_l), V(row_l), V(down_l));
        vec3_add(V(row_r), V(row_r), V(down_r));
        p14 = next_p14;
        p1c = next_p1c;
        p20 = next_p20;
        p60 = next_p60;
        p64 = next_p64;
    }

    // 5. Texture coordinate arrays
    float int_part = 0.0f;
    float u_val = split_int_frac(
        &int_part, fpu_add(fpu_mul(frame_f, lwc1(pv + 0x40)),
                           fpu_mul(lwc1(pv + 0x44), sin_vu(angle_wrap_mod(fpu_mul(frame_f, lwc1(pv + 0x48)))))));
    for (uint32_t c = 0; c <= cols; ++c) {
        swc1(u_arr + c * 4, u_val);
        u_val = fpu_add(u_val, fpu_div(inv_cols, lwc1(pv + 0x70)));
    }
    float v_val = split_int_frac(
        &int_part, fpu_add(fpu_mul(frame_f, lwc1(pv + 0x4c)),
                           fpu_mul(lwc1(pv + 0x50), sin_vu(angle_wrap_mod(fpu_mul(frame_f, lwc1(pv + 0x54)))))));
    for (uint32_t r = 0; r <= rows; ++r) {
        swc1(v_arr + r * 4, v_val);
        v_val = fpu_add(v_val, fpu_div(inv_rows, lwc1(pv + 0x74)));
    }

    // 6. Emit cell quads
    const uint32_t rgb = (static_cast<uint32_t>(lbu(pv + 0x11)) << 16) |
                         (static_cast<uint32_t>(lbu(pv + 0x12)) << 8) | static_cast<uint32_t>(lbu(pv + 0x10));
    for (uint32_t r = 0; r < rows; ++r) {
        const uint32_t row0 = grid + r * row_stride;
        const uint32_t row1 = grid + (r + 1) * row_stride;
        const float v0 = lwc1(v_arr + r * 4), v1 = lwc1(v_arr + (r + 1) * 4);
        for (uint32_t c = 0; c < cols; ++c) {
            const float u0 = lwc1(u_arr + c * 4), u1 = lwc1(u_arr + (c + 1) * 4);
            swc1(desc + 0x50, u0);
            swc1(desc + 0x54, v0);
            swc1(desc + 0x58, u1);
            swc1(desc + 0x5c, v0);
            swc1(desc + 0x60, u0);
            swc1(desc + 0x64, v1);
            swc1(desc + 0x68, u1);
            swc1(desc + 0x6c, v1);

            const uint32_t p00 = row0 + (c << 4), p01 = row0 + ((c + 1) << 4);
            const uint32_t p10 = row1 + (c << 4), p11 = row1 + ((c + 1) << 4);
            vec3_add(V(desc), V(p00), V(m + 0x10));
            vec3_add(V(desc + 0x10), V(p01), V(m + 0x10));
            vec3_add(V(desc + 0x20), V(p10), V(m + 0x10));
            vec3_add(V(desc + 0x30), V(p11), V(m + 0x10));

            auto corner_col = [&](uint32_t pt) {
                const int32_t denom = float_to_int(lwc1(pt + 0xc));
                const uint32_t a = static_cast<uint32_t>(ee_div(static_cast<int32_t>(base_alpha), denom));
                return (a << 24) | rgb;
            };
            sw(desc + 0x40, corner_col(p00));
            sw(desc + 0x44, corner_col(p01));
            sw(desc + 0x48, corner_col(p10));
            sw(desc + 0x4c, corner_col(p11));
            quad_draw(desc, 0);
        }
    }

    // 7. Sparks, embers and light drift (once per gameplay update: bit 2 of +0x0a)
    if ((lhu(pv + 0xa) & 2u) != 0) {
        const uint32_t p_vel = s8 + 0x190, p_pos = s8 + 0x1a0, p_off = s8 + 0x1b0;
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xf8))) != 0) {
            sw(pv + 0xf8, static_cast<uint32_t>(frame_scale_i(lbu(pv + 0xf3))));
            for (uint32_t i = 0; i < lbu(pv + 0xf2); ++i) {
                const float half_width = fpu_mul(lwc1(pv), 0.5f);
                const float r_span = fpu_mul(rand_range_f(-1.0f, 1.0f), lwc1(pv + 0xf4));
                const float u_pos = rand_range_f(0.0f, r_span);
                const float dome = sqrt_f(cos_vu(fpu_mul(u_pos, pi)));
                const float drop = -fpu_add(lwc1(pv + 0xfc), fpu_mul(dome, lwc1(pv + 0x100)));
                vec3_set_length(V(p_off), V(down_l), drop);
                vec3_set_length(V(p_pos), V(step_col), fpu_mul(u_pos, half_width));
                vec3_add(V(p_pos), V(p_pos), V(m + 0x10));
                vec3_add(V(p_pos), V(p_pos), V(p_off));

                const float spread = lwc1(pv + 0xdc);
                swc1(p_vel, fpu_mul(rand_range_f(-spread, spread), lwc1(0x0015ed60)));
                const float spread2 = lwc1(pv + 0xdc);
                swc1(p_vel + 4, fpu_mul(rand_range_f(-spread2, spread2), lwc1(0x0015ed60)));
                swc1(p_vel + 8, fpu_mul(rand_range_f(0.0f, lwc1(pv + 0xe0)), lwc1(0x0015ed60)));
                const float sz = rand_range_f(lwc1(pv + 0xe4), lwc1(pv + 0xe8));
                const int16_t life = static_cast<int16_t>(frame_scale_i(lh(pv + 0xf0)));
                const uint8_t alpha = static_cast<uint8_t>(float_to_int(fpu_mul(int_to_float(lbu(pv + 0xef)), u_pos)));
                banner_spark_spawn_l8(m, p_pos, p_vel, life, static_cast<uint8_t>(lbu(pv + 0xec)),
                                      static_cast<uint8_t>(lbu(pv + 0xed)), static_cast<uint8_t>(lbu(pv + 0xee)),
                                      alpha, sz);
            }
        }

        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x122))) != 0) {
            sh(pv + 0x122, static_cast<uint32_t>(frame_scale_i(lbu(pv + 0x121))));
            for (uint32_t i = 0; i < lbu(pv + 0x120); ++i) {
                const uint32_t c_a = (static_cast<uint32_t>(lbu(pv + 0x117)) << 24) |
                                     (static_cast<uint32_t>(lbu(pv + 0x116)) << 16) |
                                     (static_cast<uint32_t>(lbu(pv + 0x115)) << 8) | static_cast<uint32_t>(lbu(pv + 0x114));
                const uint32_t c_b = (static_cast<uint32_t>(lbu(pv + 0x11b)) << 24) |
                                     (static_cast<uint32_t>(lbu(pv + 0x11a)) << 16) |
                                     (static_cast<uint32_t>(lbu(pv + 0x119)) << 8) | static_cast<uint32_t>(lbu(pv + 0x118));
                const float half_width = fpu_mul(lwc1(pv), 0.5f);
                const float sz = rand_range_f(lwc1(pv + 0x10c), lwc1(pv + 0x110));
                const int32_t life = frame_scale_i(rand_range(lh(pv + 0x11c), lh(pv + 0x11e)));
                const float bound = fpu_mul(rand_range_f(-1.0f, 1.0f), lwc1(pv + 0x124));
                const float u_pos = rand_range_f(-bound, bound);
                const float dome = sqrt_f(cos_vu(fpu_mul(u_pos, pi)));
                const float drop = -fpu_add(lwc1(pv + 0x12c), fpu_mul(dome, lwc1(pv + 0x130)));
                vec3_set_length(V(p_off), V(down_l), drop);
                vec3_set_length(V(p_pos), V(step_col), fpu_mul(u_pos, half_width));
                vec3_add(V(p_pos), V(p_pos), V(m + 0x10));
                vec3_add(V(p_pos), V(p_pos), V(p_off));

                const float spread = lwc1(pv + 0x104);
                swc1(p_vel, fpu_mul(rand_range_f(-spread, spread), lwc1(0x0015ed60)));
                const float spread2 = lwc1(pv + 0x104);
                swc1(p_vel + 4, fpu_mul(rand_range_f(-spread2, spread2), lwc1(0x0015ed60)));
                swc1(p_vel + 8, fpu_mul(rand_range_f(0.0f, lwc1(pv + 0x108)), lwc1(0x0015ed60)));
                const float k_dome = lwc1(pv + 0x128);
                vec3_scale(V(p_vel), V(p_vel), fpu_add(fpu_sub(1.0f, k_dome), fpu_mul(dome, k_dome)));
                particle_16_spawn(p_pos, p_vel, c_a, c_b, life, sz);
            }
        }

        const int32_t lslot = static_cast<int8_t>(lbu(pv + 0x13b));
        if (lslot != -1) {
            const uint32_t lrec = L8_ADDR(0x00180ac0) + static_cast<uint32_t>(lslot << 5);
            const uint32_t lgoal = pv + 0x140, lpos = lrec + 0x10;
            const float max_step = fpu_mul(lwc1(pv + 0x13c), lwc1(0x0015ed60));
            if (vec3_distance(V(lgoal), V(lpos)) < max_step) {
                const int32_t rx = static_cast<int32_t>(lbu(pv + 0x139));
                const float jx = rand_range_f(static_cast<float>(-rx), static_cast<float>(rx));
                const float u_w = fpu_mul(fpu_add(jx, lwc1(pv)), 0.5f);
                const int32_t ry = static_cast<int32_t>(lbu(pv + 0x13a));
                const float jy = rand_range_f(static_cast<float>(-ry), static_cast<float>(ry));
                const float v_h = fpu_mul(fpu_add(jy, lwc1(pv + 4)), 0.5f);
                vec3_set_length(V(p_vel), V(step_col), u_w);
                vec3_set_length(V(p_pos), V(down_l), -v_h);
                vec3_add(V(lgoal), V(p_vel), V(p_pos));
                vec3_add(V(lgoal), V(lgoal), V(c0));
                vec3_add(V(lgoal), V(lgoal), V(m + 0x10));
            } else {
                vec3_sub(V(p_vel), V(lgoal), V(lpos));
                vec3_set_length(V(p_vel), V(p_vel), fpu_mul(lwc1(pv + 0x13c), lwc1(0x0015ed60)));
                vec3_add(V(lpos), V(lpos), V(p_vel));
                swc1(lrec + 0x1c, int_to_float(lbu(pv + 0x137)));
            }
        }
        sh(pv + 0xa, lhu(pv + 0xa) ^ 2u);
    }

    ee_stack_free(total_alloc);
}

// Class 671, a waving banner / flame curtain: state 0 initializes its wind weights and UV tiling defaults; every frame
// updates its bounding sphere and distance fade (+0x154), queues bat_class671_draw_l8 when visible, manages its dynamic
// light (+0x13b in 0x180ac0) when +0x0a bit 2 (0x4) is set, and arms bit 1 (0x2) of +0x0a.
// orig: L8:0x002f70a0
void bat_class671_update_l8(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;

    if (lbu(m + 0x20) == 0) {
        sb(pv + 0x13b, 0xff);
        sw(pv + 0xf8, lbu(pv + 0xf3));
        lq_sq(pv + 0x140, m + 0x10);
        if (lwc1(pv + 0x70) == 0.0f) {
            debug_printf_stub();
            swc1(pv + 0x70, 1.0f);
        } else if (8.0f < lwc1(pv + 0x74)) {
            debug_printf_stub();
        }
        if (lwc1(pv + 0x74) == 0.0f) {
            debug_printf_stub();
            swc1(pv + 0x74, 1.0f);
        } else if (8.0f < lwc1(pv + 0x70)) {
            debug_printf_stub();
        }
        for (uint32_t i = 0; i < 8; ++i) {
            swc1(pv + 0x90 + i * 4, rand_range_f(lwc1(pv + 0x88), 1.0f));
            swc1(pv + 0xb0 + i * 4, rand_range_f(lwc1(pv + 0x88), 1.0f));
        }
        sb(m + 0x20, 1);
    }

    const float w = lwc1(pv), h = lwc1(pv + 4);
    const float span = h < w ? w : h;
    swc1(m + 0x2c, fpu_mul(fpu_mul(lwc1(lwu(m + 0x24) + 0x24), span), 0.25f));

    const float max_d = int_to_float(lh(m + 0x32));
    const float dist = vec3_distance(V(m + 0x10), V(L8_ADDR(0x001675c0)));
    const float fade_len = lwc1(pv + 0x150);
    float near_d = fpu_sub(max_d, fade_len);
    if (near_d < 0.0f) near_d = 0.0f;

    bool culled = false;
    if (max_d < dist) {
        swc1(pv + 0x154, 0.0f);
        culled = true;
    } else if (near_d < dist) {
        swc1(pv + 0x154, fpu_sub(1.0f, fpu_div(fpu_sub(dist, near_d), fade_len)));
    } else {
        swc1(pv + 0x154, 1.0f);
    }

    const uint32_t sp = ee_stack_alloc(208);
    lq_sq(sp, m + 0x10);
    swc1(sp + 0xc, int_to_float(lbu(pv + 0x137)));
    const bool visible = lbu(m + 0x31) != 0 || point_on_screen(sp, 255.0f) != -1;
    if (!visible || culled) {
        const int32_t slot = static_cast<int8_t>(lbu(pv + 0x13b));
        if (slot != -1) {
            region_clear(static_cast<uint32_t>(slot));
            sb(pv + 0x13b, 0xff);
        }
    } else {
        sub_001f93e0(L8_ADDR(0x002f5e58), m);
        if ((lhu(pv + 0xa) & 4u) != 0) {
            const int32_t slot = static_cast<int8_t>(lbu(pv + 0x13b));
            if (slot == -1) {
                const float rad = static_cast<float>(lbu(pv + 0x137));
                const float fall = fpu_div(int_to_float(lbu(pv + 0x138)), 100.0f);
                const float cr = fpu_div(fpu_mul(lwc1(pv + 0x154), int_to_float(lbu(pv + 0x134))), 100.0f);
                const float cg = fpu_div(fpu_mul(lwc1(pv + 0x154), int_to_float(lbu(pv + 0x135))), 100.0f);
                const float cb = fpu_div(fpu_mul(lwc1(pv + 0x154), int_to_float(lbu(pv + 0x136))), 100.0f);
                sb(pv + 0x13b, static_cast<uint32_t>(light_add(pv + 0x140, rad, fall, cr, cg, cb)));
            } else {
                const uint32_t lrec = L8_ADDR(0x00180ac0) + static_cast<uint32_t>(slot << 5);
                swc1(lrec, fpu_div(fpu_mul(lwc1(pv + 0x154), int_to_float(lbu(pv + 0x134))), 100.0f));
                swc1(lrec + 4, fpu_div(fpu_mul(lwc1(pv + 0x154), int_to_float(lbu(pv + 0x135))), 100.0f));
                swc1(lrec + 8, fpu_div(fpu_mul(lwc1(pv + 0x154), int_to_float(lbu(pv + 0x136))), 100.0f));
            }
        }
    }
    sh(pv + 0xa, lhu(pv + 0xa) | 2u);
    ee_stack_free(208);
}

}  // namespace rac1
