// Class 1111, the open sea about the camera, and its draw callback: one port of the copies Pokitaru and Kalebo III carry
// (the same code relinked). Each level keeps the sea's state, the camera's last place and the draw's vertex buffers at
// addresses of its own (per level below, the same layout on both); the rest is the engine's (OV: the camera 0x166dc0 and its
// matrix 0x166fd0, the view's fog 0x16ca40 + 0x228 / + 0x22c, the frame counter 0x15f5cc, 0x16c780, the water disc
// 0x1612dc) or the base ELF's (bare: the hero's state 0x1413d4, the frame's scale 0x15ed7c). The VU1 set-up is Veldin's
// (shadows_vu1_setup, 0x1fc740). Header shared_class1111.h.
//
// The pvars: +0 / +4 the band's near and far colours (+4 / +8 the curtain's bottom and top), +0xc / +0x10 the tiles' near
// and far colours, +0x14 the sea's level, +0x18 a tile's size, +0x1c the layers' first texture (less 0x28), +0x20 the
// camera's height above which the sea is drawn, +0x24 the curtain's top below the camera, +0x28 the band's reach ahead,
// +0x2c the height over which the tiles fade out.
//
// The 1.02 build (the PAL disc 2.00) changed the update: see class1111_update.
#include <game/ovl/shared_class1111.h>

#include <game/enhance/fixes102.h>
#include <game/math/vecmath.h>
#include <game/ovl/nanotech_draw.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/shadows.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l16.h>

namespace rac1 {

namespace {
// The sea's state (0x28 bytes): +0 / +4 the two layers' texture scales, +0x10 + 8 i / +0x14 + 8 i layer i's texture
// offsets (u, v), +0x20 the swell's height now, +0x24 the sea's level
uint32_t sea() {
    if (on_level(KALEBO)) return L16_ADDR(0x00161c50);
    return L11_ADDR(0x00161df8);
}
// the camera's place at the last update
uint32_t last_camera() {
    if (on_level(KALEBO)) return L16_ADDR(0x00162240);
    return L11_ADDR(0x00162840);
}
// The draw's buffers (0xc20 bytes): +0 the layers' scroll rates (u, v; 8 bytes a layer), +0x18 the band's 12 points
// (0xc each), +0x318 their colours (4 each), +0x418 a row of tiles' 64 points, +0x718 their colours, +0x820 the band's
// texture coordinates (8 each), +0xa20 the tiles'
uint32_t buffers() {
    if (on_level(KALEBO)) return L16_ADDR(0x001d8a10);
    return L11_ADDR(0x001d9af0);
}
// the level's copy of the draw callback (queued by the update: the levels still register their copies)
uint32_t draw_callback() {
    if (on_level(KALEBO)) return L16_ADDR(0x002de3c0);
    return L11_ADDR(0x0030a908);
}

constexpr int32_t LAYERS = 2;
constexpr int32_t BAND_POINTS = 12;
constexpr int32_t ROW_POINTS = 64;
constexpr uint32_t HERO_STATE = 0x001413d4u;

uint32_t camera() { return OV(0x00166dc0); }
uint32_t view_fog() { return OV(0x0016ca40) + 0x228; }  // (+0 / +4: the fog's two values)

// a texture offset kept inside -1 .. 1 (one wrap each way)
void wrap(uint32_t a) {
    if (1.0f < lwc1(a)) swc1(a, fpu_sub(lwc1(a), 1.0f));
    if (lwc1(a) < -1.0f) swc1(a, fpu_add(lwc1(a), 1.0f));
}

// the fractional part of a coordinate (left alone when it is exactly 1)
void fraction(uint32_t a) {
    const float v = lwc1(a);
    if (1.0f < v || v < 1.0f) swc1(a, fpu_sub(v, int_to_float(float_to_int(v))));
}

// the sea is not seen up close: the hero not in his state 0x1d and 0x16c780 clear
bool open_view() { return lw(HERO_STATE) != 0x1d && lw(OV(0x0016c780)) == 0; }

// One vertex of a row of tiles: at the camera's (x, y) + (`x`, `y`) + 1 at the swell's height, its texture coordinates
// (`x`, `y`) / 2 tiles scaled by the layer's scale and moved by its offsets.
void tile_point(uint32_t buf, int32_t k, uint32_t pv, uint32_t layer_scale, uint32_t layer_u, uint32_t layer_v, float x, float y,
                bool step_x, bool step_y) {
    const float size = lwc1(pv + 0x18);
    const float scale = lwc1(layer_scale);
    const float ux = step_x ? fpu_add(x, size) : x;
    const float uy = step_y ? fpu_add(y, size) : y;
    const uint32_t st = buf + 0xa20 + static_cast<uint32_t>(k) * 8;
    const uint32_t xyz = buf + 0x418 + static_cast<uint32_t>(k) * 0xc;
    swc1(st, fpu_add(fpu_mul(fpu_div(ux, fpu_add(size, size)), scale), lwc1(layer_u)));
    swc1(st + 4, fpu_add(fpu_mul(fpu_div(uy, fpu_add(size, size)), scale), lwc1(layer_v)));
    swc1(xyz, fpu_add(fpu_add(ux, lwc1(camera())), 1.0f));
    swc1(xyz + 4, fpu_add(fpu_add(uy, lwc1(camera() + 4)), 1.0f));
    swc1(xyz + 8, lwc1(sea() + 0x20));
}
}  // namespace

// State 0 sets the sea up: shown, the layers' offsets cleared, the camera noted, the swell at the sea's level. Then each
// frame: the layers scroll against the camera's motion (each at its scale over five tiles), only while the camera is
// above the pvar's height; the swell rises and falls a quarter about the level once every 360 frames; the layers drift by
// their rates; and the draw is queued.
// The 1.02 build (the PAL disc 2.00) adds two things while the camera is above that height (data/regions/
// SCES-50916-2.00.txt; its L11:0x30c728 / L16:0x2e01d8): with the camera above 180 the underwater colours (0x161200,
// seven bytes) are set each frame (a look, not forked; docs/advanced.md), and after the swell the level's water disc
// (0x1612dc: on, centre (512, 512), height the swell's, radius 1024) so that the open sea is water to the hero's movement,
// the bombs and the mines (surface_height): 1.02 Gameplay fix 24 (fixes102.h), off by default.
// orig: L11:0x0030b358 L16:0x002dee10
void class1111_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t s = sea();
    if (lbu(m + 0x20) == 0) {
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
        for (int32_t i = 0; i < LAYERS; ++i) {
            sw(s + 0x10 + static_cast<uint32_t>(i) * 8, 0);
            sw(s + 0x14 + static_cast<uint32_t>(i) * 8, 0);
        }
        lq_sq(last_camera(), camera());
        swc1(s + 0x24, lwc1(pv + 0x14));
        return;
    }
    const uint32_t sp = ee_stack_alloc(0xb0);
    const uint32_t moved = sp, step = sp + 0x10;
    vec3_sub(V(moved), V(camera()), V(last_camera()));
    lq_sq(last_camera(), camera());
    if (!(lwc1(pv + 0x20) < lwc1(camera() + 8))) {
        ee_stack_free(0xb0);
        return;
    }
    for (int32_t i = 0; i < LAYERS; ++i) {
        const uint32_t u = s + 0x10 + static_cast<uint32_t>(i) * 8, v = u + 4;
        const float scale = lwc1(s + static_cast<uint32_t>(i) * 4);
        vec3_scale(V(step), V(moved), fpu_div(fpu_add(scale, scale), fpu_mul(lwc1(pv + 0x18), 5.0f)));
        fraction(step);
        fraction(step + 4);
        swc1(u, fpu_add(lwc1(u), lwc1(step)));
        swc1(v, fpu_add(lwc1(v), lwc1(step + 4)));
        wrap(u);
        wrap(v);
    }
    {
        const int32_t frame = ee_rem(lw(OV(0x0015f5cc)), 360);
        const float angle = fpu_sub(fpu_mul(int_to_float(frame), u2f(0x3c8ee7a7u)), u2f(0x4048f5c3u));  // (degrees to radians, - 3.14)
        const float swell = fpu_add(lwc1(s + 0x24), fpu_mul(sin_vu(angle), 0.25f));
        swc1(s + 0x20, swell);
        if (gameplay102_on(24)) {  // enhancement: 1.02 Gameplay, fix 24 (fixes102.h): the open sea is water
            sw(OV(0x001612dc), 1);
            swc1(OV(0x001612e4), 512.0f);
            swc1(OV(0x001612ec), 1024.0f);
            swc1(OV(0x001612e0), 512.0f);
            swc1(OV(0x001612e8), swell);
        }
    }
    const float fs = lwc1(0x0015ed7c);
    const uint32_t rates = buffers();
    for (int32_t i = 0; i < LAYERS; ++i) {
        const uint32_t u = s + 0x10 + static_cast<uint32_t>(i) * 8, v = u + 4;
        const uint32_t rate = rates + static_cast<uint32_t>(i) * 8;
        swc1(u, fpu_add(lwc1(u), fpu_mul(lwc1(rate), fs)));
        wrap(u);
        swc1(v, fpu_add(lwc1(v), fpu_mul(lwc1(rate + 4), fs)));
        wrap(v);
    }
    sub_001f93e0(draw_callback(), m);
    ee_stack_free(0xb0);
}

// The draw, in the camera's flattened frame (its x and y axes level): a band of 12 points from 75 behind / 87 aside to the
// pvar's reach ahead / 750 aside, its colours from near to far; under an open view and with the camera high enough, a
// curtain on the band's far edge up to the pvar's height below the camera; then, under 1 + its height, the two layers'
// 4 x 4 tiles about the camera, faded from near to far and out with the height. The fog is raised to 255 / 249.9 for it
// (in an open view) and put back.
// orig: L11:0x0030a908 L16:0x002de3c0
void class1111_draw(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t s = sea(), buf = buffers();
    const uint32_t sp = ee_stack_alloc(0x240);
    const uint32_t tmp = sp, aside = sp + 0x10, ax = sp + 0x20, ay = sp + 0x30;
    const uint32_t near_l = sp + 0x40, far_l = sp + 0x50, near_r = sp + 0x60, far_r = sp + 0x70;
    const uint32_t left = sp + 0x80, right = sp + 0x90, quad = sp + 0xa0;
    const uint32_t c_near = sp + 0x134, c_far = sp + 0x138;
    const float fog_a = lwc1(view_fog()), fog_b = lwc1(view_fog() + 4);
    if (open_view()) {
        swc1(view_fog(), 255.0f);
        swc1(view_fog() + 4, u2f(0x4379e666u));  // (249.9)
    }
    sub_002a0ba8(0x42, 0x3200000044ull);
    sub_002a0ba8(0x8, 0);
    sub_002a0ba8(0x14, 0xff9000000260ull);
    sub_002a0ba8(0x47, 0x513f1);
    sub_002a0ba8(0x6, effect_tex0(0x16));
    shadows_vu1_setup();
    // the camera's x and y axes, level and of unit length
    swc1(ax, 1.0f);
    sw(ax + 4, 0);
    sw(ax + 8, 0);
    swc1(ax + 0xc, 1.0f);
    mat3_transform(V(ax), V(ax), M(OV(0x00166fd0)));
    sw(ay, 0);
    swc1(ay + 4, 1.0f);
    sw(ay + 8, 0);
    swc1(ay + 0xc, 1.0f);
    mat3_transform(V(ay), V(ay), M(OV(0x00166fd0)));
    sw(ax + 8, 0);
    vec3_set_length(V(ax), V(ax), 1.0f);
    sw(ay + 8, 0);
    vec3_set_length(V(ay), V(ay), 1.0f);
    // the band's corners
    vec3_set_length(V(tmp), V(ax), -75.0f);
    vec3_set_length(V(aside), V(ay), -87.0f);
    vec3_add(V(near_l), V(tmp), V(aside));
    vec3_set_length(V(aside), V(ay), 87.0f);
    vec3_add(V(near_r), V(tmp), V(aside));
    vec3_set_length(V(tmp), V(ax), lwc1(pv + 0x28));
    vec3_set_length(V(aside), V(ay), -750.0f);
    vec3_add(V(far_l), V(tmp), V(aside));
    vec3_set_length(V(aside), V(ay), 750.0f);
    vec3_add(V(far_r), V(tmp), V(aside));
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t c = near_l + k * 0x10;
        vec3_add(V(c), V(c), V(camera()));
        swc1(c + 8, lwc1(s + 0x20));
    }
    vec3_sub(V(left), V(far_l), V(near_l));
    vec3_sub(V(right), V(far_r), V(near_r));
    for (int32_t i = 0; i < BAND_POINTS; i += 2) {
        const float t = fpu_div(int_to_float(i), 10.0f);
        const uint32_t colour = rgba_lerp(lw(pv), lw(pv + 4), fpu_div(int_to_float(i), 10.0f));
        const uint32_t xyz = buf + 0x18 + static_cast<uint32_t>(i) * 0xc, rgba = buf + 0x318 + static_cast<uint32_t>(i) * 4;
        const uint32_t st = buf + 0x820 + static_cast<uint32_t>(i) * 8;
        vec3_scale(V(tmp), V(left), t);
        sw(rgba, colour);
        sw(st, 0);
        sw(st + 4, 0);
        vec3_add(V(tmp), V(tmp), V(near_l));
        swc1(xyz, lwc1(tmp));
        swc1(xyz + 4, lwc1(tmp + 4));
        swc1(xyz + 8, lwc1(tmp + 8));
        vec3_scale(V(tmp), V(right), t);
        sw(rgba + 4, colour);
        sw(st + 8, 0);
        sw(st + 0xc, 0);
        vec3_add(V(tmp), V(tmp), V(near_r));
        swc1(xyz + 0xc, lwc1(tmp));
        swc1(xyz + 0x10, lwc1(tmp + 4));
        swc1(xyz + 0x14, lwc1(tmp + 8));
    }
    prim_list_packet(BAND_POINTS, buf + 0x18, buf + 0x318, buf + 0x820, 1);
    // the curtain: a quad on the band's far edge from the swell up to the pvar's height below the camera
    sd(quad + 0x78, effect_tex0(0x16));
    sd(quad + 0x80, 0xff9000000260ull);
    sd(quad + 0x88, 0x8000000044ull);
    sd(quad + 0x70, 0);
    const float cam_z = lwc1(camera() + 8);
    if (lwc1(pv + 0x14) < fpu_sub(cam_z, lwc1(pv + 0x24)) && open_view()) {
        for (uint32_t k = 0x50; k < 0x70; k += 4) sw(quad + k, 0);  // (the texture coordinates)
        sw(quad + 0x40, lwu(pv + 4));
        sw(quad + 0x48, lwu(pv + 4));
        sw(quad + 0x4c, lwu(pv + 8));
        sw(quad + 0x44, lwu(pv + 8));
        lq_sq(quad + 0x10, far_l);
        lq_sq(quad, far_l);
        lq_sq(quad + 0x30, far_r);
        lq_sq(quad + 0x20, far_r);
        swc1(quad + 0x18, fpu_sub(cam_z, lwc1(pv + 0x24)));
        swc1(quad + 0x38, fpu_sub(cam_z, lwc1(pv + 0x24)));
        quad_draw(quad, 0);
    }
    sub_002a0ba8(0x42, 0x8000000048ull);
    sub_002a0ba8(0x8, 0);
    sub_002a0ba8(0x14, 0xff9000000260ull);
    sub_002a0ba8(0x47, 0x513f1);
    // the tiles: faded by the camera's height over the swell
    {
        sw(c_near, lwu(pv + 0xc));
        float fade = fpu_sub(1.0f, fpu_div(fpu_sub(lwc1(camera() + 8), lwc1(s + 0x20)), lwc1(pv + 0x2c)));
        if (1.0f < fade) fade = 1.0f;
        sw(c_far, lwu(pv + 0x10));
        if (!(0.0f <= fade)) {
            swc1(view_fog() + 4, fog_b);
            swc1(view_fog(), fog_a);
            sub_002a0ba8(0x47, 0x5360b);
            ee_stack_free(0x240);
            return;
        }
        sw(c_near, rgba_lerp(0, lwu(c_near), fade));
        sw(c_far, rgba_lerp(0, lwu(c_far), fade));
    }
    for (int32_t layer = 0; layer < LAYERS; ++layer) {
        sub_002a0ba8(0x6, effect_tex0(lw(pv + 0x1c) + layer + 0x28));
        const uint32_t layer_scale = s + static_cast<uint32_t>(layer) * 4;
        const uint32_t layer_u = s + 0x10 + static_cast<uint32_t>(layer) * 8, layer_v = layer_u + 4;
        for (float y = fpu_mul(lwc1(pv + 0x18), -2.0f); y < fpu_add(lwc1(pv + 0x18), lwc1(pv + 0x18)); y = fpu_add(y, lwc1(pv + 0x18))) {
            int32_t n = 0;  // (the row's points)
            for (float x = fpu_mul(lwc1(pv + 0x18), -2.0f); x < fpu_add(lwc1(pv + 0x18), lwc1(pv + 0x18)); x = fpu_add(x, lwc1(pv + 0x18))) {
                tile_point(buf, n, pv, layer_scale, layer_u, layer_v, x, y, false, false);
                tile_point(buf, n + 1, pv, layer_scale, layer_u, layer_v, x, y, false, true);
                tile_point(buf, n + 2, pv, layer_scale, layer_u, layer_v, x, y, true, false);
                tile_point(buf, n + 3, pv, layer_scale, layer_u, layer_v, x, y, true, true);
                // each point's colour from near to far by its distance across from the camera over two tiles
                for (int32_t k = n; k < n + 4; ++k) {
                    const uint32_t xyz = buf + 0x418 + static_cast<uint32_t>(k) * 0xc;
                    swc1(tmp, fpu_sub(lwc1(xyz), lwc1(camera())));
                    swc1(tmp + 4, fpu_sub(lwc1(xyz + 4), lwc1(camera() + 4)));
                    swc1(tmp + 8, fpu_sub(lwc1(xyz + 8), lwc1(camera() + 8)));
                    swc1(tmp + 0xc, 1.0f);
                    float far = fpu_div(vec2_length(V(tmp)), fpu_add(lwc1(pv + 0x18), lwc1(pv + 0x18)));
                    if (1.0f < far) far = 1.0f;
                    sw(buf + 0x718 + static_cast<uint32_t>(k) * 4, rgba_lerp(lwu(c_near), lwu(c_far), far));
                }
                n += 4;
                if (!(n < ROW_POINTS)) debug_trap();
            }
            prim_list_packet(n, buf + 0x418, buf + 0x718, buf + 0xa20, 1);
        }
    }
    swc1(view_fog() + 4, fog_b);
    swc1(view_fog(), fog_a);
    sub_002a0ba8(0x47, 0x5360b);
    ee_stack_free(0x240);
}

}  // namespace rac1
