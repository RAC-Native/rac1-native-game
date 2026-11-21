// Oltanis (level 14), class 8's draws: the light of the searchlight drones (oltanis_searchlight.cpp).
// - 0x2ae260, queued by each drone: the spot its beam casts, a quad on the ground at pvars +0x90, pulled towards the
//   camera, red while the drone's alarm (+0x202) is on.
// - 0x2aee28, queued by the master of the beams (state 8): for each of its +0x20a beams the drone of its group farthest
//   from the camera that is not drawn yet (0x2aef28, the drawn ones' uids in the list L14:0x1d8680), then that drone's
//   glow (0x2add48: sixteen pairs of fading quads along its turning direction, when its +0x20c has bit 8) and its cone
//   (0x2ada18: the lens quad 0x2ae0a8, VU1 program 7 set up by shadows_vu1_setup, then the cone's triangle list drawn
//   twice, once from each side, when its +0x20c has bit 2).
// The quads go through quad_draw, the cone through prim_list_packet; every local the callees read or write lives in an
// EE frame of the original's size at the original's offsets.
//
// VU0: none of these functions has a VU0 macro instruction of its own. The cone's 0x20f9d0 (the level's copy of
// shadows_vu1_setup, Veldin's 0x1fc740) is the only VU0 writer they reach: through its copy of mat4_scale_diag (L14:0x213028)
// it leaves vf1..vf3 = the diagonal 1024, vf4 = vf0, vf5 = (1024, 0, 0, 0); the engine's shadows_vu1_setup leaves the same
// (mat4_scale_diag_vu0, batch 506's check of L14:0x213028). The rest (mat4_mul, mat4_transform, euler_to_mat3_vu, vec3_*,
// quad_draw, prim_list_packet) are the host helpers the generated code already called: the port leaves VU0 as it was left.
// Level 14 only: `orig: L14:`, Oltanis data L14_ADDR, base ELF bare. Registered in oltanis_mobys.cpp.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/nanotech_draw.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/shadows.h>
#include <game/ovl/variants_l14.h>

#include <cstdint>

namespace rac1 {

namespace {

struct Frame {  // an EE stack frame of the original's size (its locals' offsets kept)
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

constexpr uint64_t kTex1 = (0xff90ull << 32) | 0x260ull;  // TEX1: MMAG / MMIN linear, L 0, K -7
constexpr uint64_t kAlpha = (0x80ull << 32) | 0x48ull;    // ALPHA: (Cs - 0) * As + Cd, FIX 0x80

// quad_draw's descriptor (quad_draw.h): the GS words at +0x70 (CLAMP 0, TEX0, TEX1, ALPHA)
void quad_gs(uint32_t q, uint64_t tex0) {
    sd(q + 0x70, 0);
    sd(q + 0x78, tex0);
    sd(q + 0x80, kTex1);
    sd(q + 0x88, kAlpha);
}
// its (s, t) pairs: (1, 1), (0, 1), (1, 0), (0, 0)
void quad_st(uint32_t q) {
    swc1(q + 0x50, 1.0f);
    swc1(q + 0x54, 1.0f);
    sw(q + 0x58, 0);
    swc1(q + 0x5c, 1.0f);
    swc1(q + 0x60, 1.0f);
    sw(q + 0x64, 0);
    sw(q + 0x68, 0);
    sw(q + 0x6c, 0);
}
// its four colours
void quad_rgba(uint32_t q, uint32_t rgba) {
    for (uint32_t k = 0; k < 4; ++k) sw(q + 0x40 + k * 4, rgba);
}

// The lens at the beam's mouth: a quad of the corners L14:0x1d8600 (scaled by L14:0x161474) facing the camera, at the
// beam's origin +0xd0 moved along its direction +0xa0 by L14:0x161478; red while the alarm is on.
// orig: L14:0x002ae0a8
void beam_lens(uint32_t m) {
    const Frame f(0x160);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    const uint32_t mat = sp, centre = sp + 0x30, push = sp + 0x40, quad = sp + 0x50, to_cam = sp + 0xe0, ang = sp + 0xf0;
    lq_sq(centre, pv + 0xd0);  // (the matrix's row 3)
    swc1(centre + 0xc, 1.0f);
    vec3_scale(V(push), V(pv + 0xa0), lwc1(L14_ADDR(0x00161478)));
    vec3_add(V(centre), V(centre), V(push));
    quad_gs(quad, effect_tex0(0xb));
    sub_002a0ba8(0x47, 0x51001);  // TEST
    vec3_sub(V(to_cam), V(L14_ADDR(0x001674c0)), V(centre));
    swc1(ang + 8, atan2_f(lwc1(to_cam), lwc1(to_cam + 4)));
    const float pitch = -atan2_f(vec2_length(V(to_cam)), lwc1(to_cam + 8));
    sw(ang, 0);
    swc1(ang + 4, pitch);
    euler_to_mat3_vu(M(mat), V(ang));
    quad_st(quad);
    quad_rgba(quad, lh(pv + 0x202) != 0 ? 0x7f40407fu : 0x7f7f7f7fu);
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t c = quad + k * 0x10;
        vec3_scale(V(c), V(L14_ADDR(0x001d8600) + k * 0x10), lwc1(L14_ADDR(0x00161474)));
        mat4_transform(V(c), V(c), M(mat));
    }
    quad_draw(quad, 0);
}

// The cone: a frame along the beam (row 0 its direction +0xa0, rows 1 / 2 across it and towards the camera, row 3 its origin
// +0xd0), VU1 program 7 set up, then the level's cone (L14:0x161418 points at L14:0x1d7a60, 12 bytes each, the strips'
// (s, t) at L14:0x1d7e38) transformed into the frame and drawn as a triangle list, then once more with rows 1 / 2 turned
// round (the far side). The colours are the table L14:0x1d80c8, while the alarm is on its red and alpha only.
// orig: L14:0x002ada18
void beam_cone(uint32_t m) {
    const Frame f(0x630);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    const uint32_t mat = sp, side = sp + 0x10, up = sp + 0x20, verts = sp + 0x40, tinted = sp + 0x420, pt = sp + 0x570;
    beam_lens(m);
    lq_sq(mat, pv + 0xa0);
    vec3_sub(V(up), V(pv + 0xd0), V(L14_ADDR(0x001674c0)));
    vec3_cross_ba(V(side), V(mat), V(up));
    vec3_set_length(V(side), V(side), 1.0f);
    vec3_cross_ba(V(up), V(side), V(mat));
    lq_sq(sp + 0x30, pv + 0xd0);
    swc1(sp + 0x3c, 1.0f);
    sub_002a0ba8(6, effect_tex0(0x16));  // TEX0_1
    sub_002a0ba8(0x42, kAlpha);          // ALPHA_1
    sub_002a0ba8(8, 5);                  // CLAMP_1
    sub_002a0ba8(0x14, kTex1);           // TEX1_1
    sub_002a0ba8(0x47, 0x50000);         // TEST_1
    sub_002a0ba8(0x4a, 0);               // FBA_1
    shadows_vu1_setup();                 // (the level's copy L14:0x20f9d0)
    // the cone's points into the frame (the vertex list at sp+0x40, 12 bytes a point)
    const auto lay = [&](bool colours) {
        for (int32_t i = 0; i < lw(L14_ADDR(0x00161418)); ++i) {
            const uint32_t src = L14_ADDR(0x001d7a60) + static_cast<uint32_t>(i) * 0xc;
            const uint32_t dst = verts + static_cast<uint32_t>(i) * 0xc;
            swc1(pt + 4, lwc1(src + 4));
            swc1(pt + 8, lwc1(src + 8));
            swc1(pt, lwc1(src));
            swc1(pt + 0xc, 1.0f);
            mat4_transform(V(pt), V(pt), M(mat));
            swc1(dst, lwc1(pt));
            swc1(dst + 4, lwc1(pt + 4));
            swc1(dst + 8, lwc1(pt + 8));
            if (colours && lh(pv + 0x202) != 0)
                sw(tinted + static_cast<uint32_t>(i) * 4, lwu(L14_ADDR(0x001d80c8) + static_cast<uint32_t>(i) * 4) & 0xff0000ffu);
        }
    };
    const auto draw = [&] {
        const uint32_t rgba = lh(pv + 0x202) != 0 ? tinted : L14_ADDR(0x001d80c8);
        prim_list_packet(lw(L14_ADDR(0x00161418)), verts, rgba, L14_ADDR(0x001d7e38), 1);
    };
    lay(true);
    draw();
    vec3_scale(V(side), V(side), -1.0f);
    vec3_scale(V(up), V(up), -1.0f);
    lay(false);
    draw();
}

// The glow: sixteen rings of two quads each (the corners L14:0x1d8600 facing the camera) at the drone's +0x1d4 (raised
// 0.125), one pushed along the turning direction (the angle +0x160, turned 4 degrees a frame), one pulled back as far;
// the rings grow (L14:0x16143c by L14:0x161440), spread (L14:0x161434 by L14:0x161438) and fade (the colour
// L14:0x161444..0x161450 less L14:0x161454..0x161460 per ring, each byte at least 0).
// orig: L14:0x002add48
void beam_glow(uint32_t m) {
    const Frame f(0x240);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    const uint32_t dir = sp, pos = sp + 0x30, quad = sp + 0x40, mat = sp + 0xd0, step = sp + 0x110, along = sp + 0x120,
                   to_cam = sp + 0x130, ang = sp + 0x140, rot = sp + 0x150;
    swc1(pos, lwc1(pv + 0x1d4));
    swc1(pos + 4, lwc1(pv + 0x1d8));
    swc1(pos + 0xc, 1.0f);
    swc1(pos + 8, fpu_add(lwc1(pv + 0x1dc), 0.125f));
    swc1(dir, cos_vu(lwc1(pv + 0x160)));
    swc1(dir + 4, sin_vu(lwc1(pv + 0x160)));
    sw(dir + 8, 0);
    swc1(pv + 0x160, angle_add(lwc1(pv + 0x160), u2f(0xbd8efa35u)));  // -4 degrees
    quad_gs(quad, effect_tex0(0xb));
    lq_sq(along, dir);
    lq_sq(mat + 0x30, pos);
    swc1(mat + 0x3c, 1.0f);
    vec3_sub(V(to_cam), V(L14_ADDR(0x001674c0)), V(mat + 0x30));
    swc1(ang + 8, atan2_f(lwc1(to_cam), lwc1(to_cam + 4)));
    const float pitch = -atan2_f(vec2_length(V(to_cam)), lwc1(to_cam + 8));
    sw(ang, 0);
    swc1(ang + 4, pitch);
    euler_to_mat3_vu(M(rot), V(ang));
    lq_sq(mat, rot);
    lq_sq(mat + 0x10, rot + 0x10);
    lq_sq(mat + 0x20, rot + 0x20);
    quad_st(quad);
    float size = lwc1(L14_ADDR(0x0016143c));
    float reach = lwc1(L14_ADDR(0x00161434));
    int32_t r = lw(L14_ADDR(0x00161444)), g = lw(L14_ADDR(0x00161448)), b = lw(L14_ADDR(0x0016144c)), a = lw(L14_ADDR(0x00161450));
    sub_002a0ba8(0x47, 0x51001);  // TEST
    for (int32_t ring = 1; ring <= 16; ++ring) {
        const auto u = [](int32_t x) { return static_cast<uint32_t>(x); };
        quad_rgba(quad, (u(a) << 24) | (u(b) << 16) | (u(g) << 8) | u(r));
        vec3_scale(V(step), V(along), reach);
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t c = quad + k * 0x10;
            vec3_scale(V(c), V(L14_ADDR(0x001d8600) + k * 0x10), size);
            mat4_transform(V(c), V(c), M(mat));
            vec3_add(V(c), V(c), V(step));
        }
        quad_draw(quad, 0);
        vec3_scale(V(step), V(along), fpu_mul(reach, -2.0f));
        for (uint32_t k = 0; k < 4; ++k) vec3_add(V(quad + k * 0x10), V(quad + k * 0x10), V(step));
        quad_draw(quad, 0);
        r = static_cast<int32_t>(u(r) - lwu(L14_ADDR(0x00161454)));
        g = static_cast<int32_t>(u(g) - lwu(L14_ADDR(0x00161458)));
        b = static_cast<int32_t>(u(b) - lwu(L14_ADDR(0x0016145c)));
        a = static_cast<int32_t>(u(a) - lwu(L14_ADDR(0x00161460)));
        size = fpu_add(size, lwc1(L14_ADDR(0x00161440)));
        reach = fpu_add(reach, lwc1(L14_ADDR(0x00161438)));
        if (r < 0) r = 0;
        if (g < 0) g = 0;
        if (b < 0) b = 0;
        if (a < 0) a = 0;
    }
}

// Of the moby group `group`, the member farthest from the camera across the ground (vec2_distance) whose uid +0xb2 is not
// in the list of drawn beams (L14:0x1d8680, 20 words); 0 when every member is listed (or the group is empty).
// orig: L14:0x002aef28
uint32_t farthest_undrawn(uint32_t group) {
    uint32_t e = lvd::group(static_cast<int32_t>(group));
    if (e == 0) return 0;
    uint32_t best = 0;
    float best_d = 0.0f;
    for (;; e += 2) {
        const uint32_t mob = lwu(L14_ADDR(0x0015ffd8)) + ((lhu(e) & 0x7fffu) << 8);
        bool drawn = false;
        for (uint32_t i = 0; i < 20 && !drawn; ++i) drawn = lh(mob + 0xb2) == lw(L14_ADDR(0x001d8680) + i * 4);
        if (!drawn) {
            const float d = vec2_distance(V(L14_ADDR(0x001674c0)), V(mob + 0x10));
            if (best_d < d) {
                best_d = d;
                best = mob;
            }
        }
        if (lh(e) < 0) break;  // (bit 15: the group's last member)
    }
    return best;
}

}  // namespace

// The spot on the ground: a quad of the corners L14:0x1d8640 scaled by +0x214 about the spot +0x90 (raised 0.01), the spot
// pulled towards the camera by its distance less L14:0x16147c, at most +0x218; red while the alarm is on, else the colour
// L14:0x161480.
// orig: L14:0x002ae260
void olt_cb_2ae260_l14(uint32_t m) {
    const Frame f(0x120);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    const uint32_t spot = sp, quad = sp + 0x10, to_cam = sp + 0xa0, pull = sp + 0xb0;
    lq_sq(spot, pv + 0x90);
    const float z = fpu_add(lwc1(spot + 8), u2f(0x3c23d70au));  // 0.01
    swc1(spot + 0xc, 1.0f);
    swc1(spot + 8, z);
    quad_gs(quad, effect_tex0(0xb));
    sub_002a0ba8(0x4a, 0);        // FBA
    sub_002a0ba8(0x47, 0x51001);  // TEST
    vec3_sub(V(to_cam), V(L14_ADDR(0x001674c0)), V(spot));
    const float dist = vec3_length(V(to_cam));
    if (0.0f < dist) {
        const float d = fpu_sub(dist, lwc1(L14_ADDR(0x0016147c)));
        float len = lwc1(pv + 0x218);
        if (d < len) len = d < 0.0f ? 0.0f : d;
        vec3_set_length(V(pull), V(to_cam), len);
        vec3_add(V(spot), V(spot), V(pull));
    }
    quad_st(quad);
    quad_rgba(quad, lh(pv + 0x202) != 0 ? 0x7f40407fu : lwu(L14_ADDR(0x00161480)));
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t c = quad + k * 0x10;
        vec3_scale(V(c), V(L14_ADDR(0x001d8640) + k * 0x10), lwc1(pv + 0x214));
        vec3_add(V(c), V(c), V(spot));
    }
    quad_draw(quad, 0);
}

// The beams: the drawn list cleared, then for each of the master's +0x20a beams the farthest drone of its group not drawn
// yet, listed, its glow and its cone drawn as its +0x20c asks.
// orig: L14:0x002aee28
void olt_cb_2aee28_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    for (uint32_t i = 20; i-- > 0;) sw(L14_ADDR(0x001d8680) + i * 4, 0xffffffffu);
    for (int32_t i = 0; i < lh(pv + 0x20a); ++i) {
        const uint32_t d = farthest_undrawn(lbu(m + 0x21));
        sw(L14_ADDR(0x001d8680) + static_cast<uint32_t>(i) * 4, static_cast<uint32_t>(static_cast<int32_t>(lh(d + 0xb2))));
        const uint32_t dpv = lwu(d + 0x78);
        if ((lwu(dpv + 0x20c) & 8) != 0) beam_glow(d);
        if ((lwu(dpv + 0x20c) & 2) != 0) beam_cone(d);
    }
}

}  // namespace rac1
