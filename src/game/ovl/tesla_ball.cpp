// Ported overlay code: Tesla Claw beam strip and muzzle glow draw callback (0x002ba970).
#include <game/ovl/tesla_ball.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>

namespace rac1 {

// orig: 0x002ba970 — scroll UV phase, draw both 20-point beams and all 4 5-point side arcs, then muzzle glow.
void tesla_2ba970_update(uint32_t m) {
    const float du = fpu_mul(lwc1(OV(0x001615f0u)), lwc1(0x0015ed60u));
    const float u_next = fpu_sub(lwc1(OV(0x001615ecu)), du);
    const uint32_t pv = lwu(m + 0x78);
    swc1(OV(0x001615ecu), u_next);
    if (u_next <= -8.0f) {
        swc1(OV(0x001615ecu), fpu_add(u_next, 8.0f));
    }

    const uint32_t pts = OV(0x001db5c0u);
    const float k02 = u2f(0x3e4ccccdu);
    tesla_ball_2baae0(pts, lwu(OV(0x001615d4u)), lwu(OV(0x001615dcu)),
                      static_cast<uint32_t>(lh(pv + 0x3e)),
                      lwc1(OV(0x001615fcu)), lwc1(OV(0x00161600u)), lwc1(OV(0x00161630u)));
    tesla_ball_2baae0(pts + 0x140, lwu(OV(0x00161604u)), lwu(OV(0x00161608u)),
                      static_cast<uint32_t>(lh(pv + 0x3e)),
                      lwc1(OV(0x0016160cu)), lwc1(OV(0x00161610u)), lwc1(OV(0x00161634u)));

    const uint32_t arcs = OV(0x001dba20u);
    for (uint32_t i = 0; i < 4; ++i) {
        const uint32_t a = static_cast<uint32_t>(lh(OV(0x00161660u) + i * 2));
        tesla_ball_2baae0(arcs + i * 0x50, a, a, 5u,
                          lwc1(OV(0x0016160cu)), lwc1(OV(0x00161610u)), k02);
    }

    tesla_ball_2bb0b0(m);
}

// orig: 0x002baae0 — draw a polyline `p0` (`p3` points) as a core quad strip (`q0`) and outer glow quads (`q1`).
void tesla_ball_2baae0(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, float pf12, float pf13, float pf14) {
    (void)pf12;
    const uint32_t frame = ee_stack_alloc(0x120);
    const uint32_t q0 = frame;
    const uint32_t q1 = frame + 0x90;

    const uint64_t tex0 = static_cast<uint64_t>(effect_tex0(lw(OV(0x00161638u))));
    const uint32_t col0 = lwu(OV(0x001615f4u)) | (p1 << 24);
    sd(q0 + 0x88, 0x0000008000000048ull);
    sd(q0 + 0x80, 0x0000ff9000000260ull);
    sd(q0 + 0x78, tex0);
    sd(q0 + 0x70, 0ull);
    sw(q0 + 0x4c, col0);
    sw(q0 + 0x48, col0);
    sw(q0 + 0x44, col0);
    sw(q0 + 0x40, col0);

    const uint64_t tex1 = static_cast<uint64_t>(effect_tex0(lw(OV(0x0016163cu))));
    sw(q1 + 0x50, 0);
    const uint32_t col1 = lwu(OV(0x001615f8u)) | (p2 << 24);
    const float u_raw = lwc1(OV(0x001615ecu));
    const float u1 = fpu_add(u_raw, 1.0f);
    const float u0 = fpu_add(u_raw, 0.0f);
    sd(q1 + 0x78, tex1);
    sd(q1 + 0x88, ld(q0 + 0x88));
    sd(q1 + 0x80, ld(q0 + 0x80));
    sd(q1 + 0x70, ld(q0 + 0x70));
    swc1(q0 + 0x58, u0);
    swc1(q0 + 0x68, u1);
    sw(q1 + 0x4c, col1);
    sw(q1 + 0x48, col1);
    sw(q1 + 0x44, col1);
    sw(q1 + 0x40, col1);
    sw(q1 + 0x54, 0);
    sw(q1 + 0x58, 0);
    swc1(q1 + 0x5c, 1.0f);
    swc1(q1 + 0x60, 1.0f);
    sw(q1 + 0x64, 0);
    swc1(q1 + 0x68, 1.0f);
    swc1(q1 + 0x6c, 1.0f);
    swc1(q0 + 0x50, u0);
    swc1(q0 + 0x54, 0.0f);
    swc1(q0 + 0x5c, 1.0f);
    swc1(q0 + 0x60, u1);
    swc1(q0 + 0x64, 0.0f);
    swc1(q0 + 0x6c, 1.0f);

    const uint32_t cam = OV(0x00166dc0u);
    Vec4 to_cam{};
    vec3_sub(to_cam, V(p0), V(cam));
    Vec4 seg{};
    vec3_sub(seg, V(p0 + 0x10), V(p0));
    Vec4 side{};
    vec3_cross_ba(side, seg, to_cam);
    float side_len = vec3_length(side);
    if (side_len != 0.0f) side_len = fpu_div(1.0f, side_len);
    vec3_scale(side, side, fpu_mul(lwc1(OV(0x001615fcu)), side_len));
    vec3_add(V(q0 + 0x00), V(p0), side);
    vec3_sub(V(q0 + 0x10), V(p0), side);

    const int32_t last_seg = static_cast<int32_t>(p3) - 1;
    for (int32_t i = 1; i < last_seg; ++i) {
        const uint32_t cur = p0 + static_cast<uint32_t>(i * 0x10);
        const uint32_t nxt = cur + 0x10;
        vec3_sub(to_cam, V(cur), V(cam));
        vec3_sub(seg, V(nxt), V(cur));
        vec3_cross_ba(side, seg, to_cam);
        float len = vec3_length(side);
        if (len != 0.0f) len = fpu_div(1.0f, len);
        vec3_scale(side, side, fpu_mul(lwc1(OV(0x001615fcu)), len));
        vec3_add(V(q0 + 0x20), V(cur), side);
        vec3_sub(V(q0 + 0x30), V(cur), side);

        if (i == 1) {
            const uint32_t fade = lwu(OV(0x001615f4u));
            sw(q0 + 0x40, fade);
            sw(q0 + 0x44, fade);
        } else if (i == static_cast<int32_t>(p3) - 2) {
            const uint32_t fade = lwu(OV(0x001615f4u));
            sw(q0 + 0x48, fade);
            sw(q0 + 0x4c, fade);
        } else if (i == 2) {
            sw(q0 + 0x44, col0);
            sw(q0 + 0x40, col0);
        }

        quad_draw(q0, 0);
        V(q0 + 0x00) = V(q0 + 0x20);
        V(q0 + 0x10) = V(q0 + 0x30);
    }

    for (int32_t i = 0; i < last_seg; ++i) {
        const uint32_t pa = p0 + static_cast<uint32_t>(i * 0x10);
        const uint32_t pb = pa + 0x10;
        Vec4 da{};
        Vec4 db{};
        vec3_sub(da, V(pa), V(cam));
        vec3_sub(db, V(pb), V(cam));
        const float la = vec3_length(da);
        const float lb = vec3_length(db);
        if (lb < la) {
            if (lb != 0.0f) {
                vec3_scale(db, db, fpu_div(la, lb));
            } else {
                db = da;
            }
        } else {
            if (la != 0.0f) {
                vec3_scale(da, da, fpu_div(lb, la));
            } else {
                da = db;
            }
        }

        Vec4 wa{};
        Vec4 wb{};
        vec3_add(wa, V(cam), da);
        vec3_add(wb, V(cam), db);
        vec3_sub(seg, wb, wa);
        Vec4 ext{};
        vec3_scale(ext, seg, pf14);
        vec3_sub(wa, wa, ext);
        vec3_add(wb, wb, ext);

        Vec4 cam_rel{};
        vec3_sub(cam_rel, V(cam), wa);
        vec3_cross_ba(side, cam_rel, seg);
        float len = vec3_length(side);
        if (len != 0.0f) len = fpu_div(1.0f, len);
        Vec4 half_w{};
        vec3_scale(half_w, side, fpu_mul(pf13, len));

        vec3_add(V(q1 + 0x00), wa, half_w);
        vec3_sub(V(q1 + 0x10), wa, half_w);
        vec3_add(V(q1 + 0x20), wb, half_w);
        vec3_sub(V(q1 + 0x30), wb, half_w);

        if (i == 0) {
            const uint32_t fade = lwu(OV(0x001615f8u));
            sw(q1 + 0x40, fade);
            sw(q1 + 0x44, fade);
        } else if (i == static_cast<int32_t>(p3) - 2) {
            const uint32_t fade = lwu(OV(0x001615f8u));
            sw(q1 + 0x48, fade);
            sw(q1 + 0x4c, fade);
        } else if (i == 1) {
            sw(q1 + 0x44, col1);
            sw(q1 + 0x40, col1);
        }

        quad_draw(q1, 0);
    }

    sub_002a0ba8(0x47u, 0x5360bull);
    ee_stack_free(0x120);
}

// orig: 0x002bb0b0 — draw the two billboarded muzzle glow quads for the Tesla Claw.
void tesla_ball_2bb0b0(uint32_t p0) {
    const uint32_t q = ee_stack_alloc(0x90);
    const uint32_t pv = lwu(p0 + 0x78);

    const uint64_t tex = static_cast<uint64_t>(effect_tex0(0xb));
    sd(q + 0x80, 0x0000ff9000000260ull);
    sd(q + 0x78, tex);
    sd(q + 0x88, 0x0000008000000048ull);
    sd(q + 0x70, 0ull);

    const uint32_t cam = OV(0x00166dc0u);
    Mat4 basis{};
    vec3_sub(basis.r[3], V(cam), V(pv));
    vec3_set_length(basis.r[3], basis.r[3], u2f(0x3e99999au));
    vec3_add(basis.r[3], basis.r[3], V(pv));
    basis.r[3].w = 1.0f;

    vec3_sub(basis.r[0], V(cam), basis.r[3]);
    vec3_set_length(basis.r[0], basis.r[0], 1.0f);
    vec3_cross_ba(basis.r[1], basis.r[0], V(0x0013f5e0u));
    vec3_set_length(basis.r[1], basis.r[1], -1.0f);
    vec3_cross_ba(basis.r[2], basis.r[1], basis.r[0]);

    const uint32_t col_inner = lwu(OV(0x00161640u));
    sw(q + 0x40, col_inner);
    sw(q + 0x4c, col_inner);
    sw(q + 0x48, col_inner);
    sw(q + 0x44, col_inner);

    const float sz_inner = fpu_add(lwc1(OV(0x00161648u)), rand_range_signed_f(0.0f, u2f(0x3ccccccdu)));
    const uint32_t tmpl_v = OV(0x001db560u);
    const uint32_t tmpl_uv = OV(0x001db5a0u);
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t vdst = q + k * 0x10;
        vec3_scale(V(vdst), V(tmpl_v + k * 0x10), sz_inner);
        mat4_transform(V(vdst), V(vdst), basis);
        swc1(q + 0x50 + k * 8, lwc1(tmpl_uv + k * 8));
        swc1(q + 0x54 + k * 8, lwc1(tmpl_uv + k * 8 + 4));
    }
    quad_draw(q, 0);

    const uint32_t col_outer = lwu(OV(0x001615f8u));
    sw(q + 0x40, col_outer);
    sw(q + 0x4c, col_outer);
    sw(q + 0x48, col_outer);
    sw(q + 0x44, col_outer);

    const float sz_outer = fpu_add(lwc1(OV(0x0016164cu)), rand_range_signed_f(0.0f, u2f(0x3d4ccccdu)));
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t vdst = q + k * 0x10;
        vec3_scale(V(vdst), V(tmpl_v + k * 0x10), sz_outer);
        mat4_transform(V(vdst), V(vdst), basis);
        swc1(q + 0x50 + k * 8, lwc1(tmpl_uv + k * 8));
        swc1(q + 0x54 + k * 8, lwc1(tmpl_uv + k * 8 + 4));
    }
    quad_draw(q, 0);

    ee_stack_free(0x90);
}

// orig: (none) -- the ports for the engine's indirect calls (a moby class's update, a draw callback)
void tesla_ball_fns_register() {
    ovl_register_moby_fn(0x002ba970, tesla_2ba970_update);
}

}  // namespace rac1
