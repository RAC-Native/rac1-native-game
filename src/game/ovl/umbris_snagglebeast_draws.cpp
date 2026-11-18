// Umbris's part 2 draw callbacks for the Snagglebeast (class 1106) and its shockwave ring (class 1046): the sweeping ground
// beam & scorch trail (0x312420), the glowing shield sphere (0x312dc8 + 0x312948), the pillar-shattering charge beam
// (0x3131b0 + 0x312e70), the suction vortex mesh (0x317910) and the unused shockwave ring drawer (0x30dcf0) — hand-ported
// from Umbris's own overlay. Header umbris_part2.h, registered in umbris_part2.cpp.
#include <game/collision/collision.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/shared_particle_kinds.h>
#include <game/ovl/umbris_part2.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/veldin_mobys.h>

namespace rac1 {

namespace {
constexpr uint64_t GS_TEX1_LINEAR = (uint64_t{0xff90} << 32) | 0x260ull;
constexpr uint64_t GS_ALPHA_ADD = (uint64_t{0x8000} << 24) | 0x48ull;
constexpr uint64_t GS_ALPHA_SHIELD = (uint64_t{0xa000} << 24) | 0x29ull;

// copies the 0x90-byte quad descriptor at `src` to `dst`
void copy_quad_desc(uint32_t dst, uint32_t src) {
    for (uint32_t off = 0; off < 0x90; off += 8) {
        sd(dst + off, ld(src + off));
    }
}

// clamps `a` to [0, 64] and packs `(a << 24) | 0x00404040`
uint32_t pack_vortex_color(int32_t a) {
    if (a <= -1) a = 0;
    if (a >= 65) a = 0x40;
    return (static_cast<uint32_t>(a) << 24) | 0x00404040u;
}
}  // namespace

// Snagglebeast draw A: casts a beam from chain point 2 toward a wobbling target at pvars +0x140, spawns scorch particles
// (0x3a) along the ground contact trail (+0x150 / +0x15c) and draws the beam quad with effect texture 0x3b.
// orig: L7:0x00312420
void umb_class1106_draw_a_l7(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x210);
    const uint32_t end_pt = sp + 0x0, end_off = sp + 0x10, side = sp + 0x20, origin = sp + 0x30;
    const uint32_t beam_dir = sp + 0x40, aim = sp + 0x50, trail_pt = sp + 0x60, trail_step = sp + 0x70;
    const uint32_t ray_vec = sp + 0x80, ray_a = sp + 0x90, ray_b = sp + 0xa0, desc = sp + 0xb0;

    const uint32_t pv = lwu(m + 0x78);
    const float fc = int_to_float(lw(L7_ADDR(0x0015f5cc)));
    V(aim) = V(pv + 0x140);
    const float c11 = cos_vu(angle_wrap_mod(fpu_div(fc, 11.0f)));
    const float phase = fpu_add(fpu_div(fc, 10.0f), fpu_mul(c11, u2f(0x3fc90fdbu)));  // (pi / 2)
    const float wob = sin_vu(angle_wrap_mod(phase));
    swc1(aim + 0x0, fpu_add(lwc1(aim + 0x0), wob));

    moby_chain_point_v(m, 2, V(origin));
    vec3_sub(V(end_off), V(aim), V(origin));
    vec3_set_length(V(end_off), V(end_off), 30.0f);
    vec3_add(V(end_pt), V(aim), V(end_off));

    const bool hit = segment_collide_v(V(origin), V(end_pt), 2, m, nullptr) != 0;
    if (hit) {
        V(end_pt) = coll_hit_point();
    }

    vec3_sub(V(beam_dir), V(origin), V(end_pt));
    vec3_cross_ba(V(side), V(L7_ADDR(0x00167050)), V(beam_dir));
    vec3_set_length(V(side), V(side), 0.25f);
    vec3_add(V(end_off), V(side), V(end_pt));

    if (hit) {
        const uint32_t b1 = static_cast<uint32_t>(frame_scale_i((libc_rand() & 1) + 0x2d)) & 0xffu;
        const uint32_t b2 = static_cast<uint32_t>(frame_scale_i((libc_rand() & 7) + 0x37)) & 0xffu;
        const uint32_t b5 = static_cast<uint32_t>(frame_scale_i((libc_rand() & 3) + 0xd)) & 0xffu;
        particle_3a_spawn(end_pt, 0, b1, b2, 1, b5, m, 1.0f, 1.0f);

        if (!(lwc1(pv + 0x15c) == 0.0f)) {
            const uint32_t prev = pv + 0x150;
            vec3_sub(V(trail_step), V(end_pt), V(prev));
            vec3_set_length(V(trail_step), V(trail_step), u2f(0x3e4ccccdu));  // (0.2)
            V(trail_pt) = V(prev);
            for (int32_t i = 0; i < 4; ++i) {
                vec3_add(V(trail_pt), V(trail_pt), V(trail_step));
                vec3_sub(V(ray_vec), V(trail_pt), V(origin));
                vec3_set_length(V(ray_vec), V(ray_vec), 5.0f);
                vec3_sub(V(ray_a), V(trail_pt), V(ray_vec));
                vec3_add(V(ray_b), V(trail_pt), V(ray_vec));
                if (segment_collide_v(V(ray_a), V(ray_b), 2, m, nullptr) != 0) {
                    V(trail_pt) = coll_hit_point();
                }
                if (i == 2) {
                    const uint32_t sb1 = static_cast<uint32_t>(frame_scale_i((libc_rand() & 1) + 0x2d)) & 0xffu;
                    const uint32_t sb2 = static_cast<uint32_t>(frame_scale_i((libc_rand() & 7) + 0x37)) & 0xffu;
                    const uint32_t sb5 = static_cast<uint32_t>(frame_scale_i((libc_rand() & 3) + 0xd)) & 0xffu;
                    particle_3a_spawn(trail_pt, 0, sb1, sb2, 1, sb5, m, 1.0f, 1.0f);
                } else {
                    const uint32_t sb1 = static_cast<uint32_t>(frame_scale_i(0x2d)) & 0xffu;
                    const uint32_t sb2 = static_cast<uint32_t>(frame_scale_i(0x37)) & 0xffu;
                    const uint32_t sb5 = static_cast<uint32_t>(frame_scale_i(0xd)) & 0xffu;
                    particle_3a_spawn(trail_pt, 1, sb1, sb2, 1, sb5, m, 1.0f, 1.0f);
                }
            }
        }
        V(pv + 0x150) = V(end_pt);
        swc1(pv + 0x15c, 1.0f);
    } else {
        sw(pv + 0x15c, 0);
    }

    const uint64_t tex0 = effect_tex0(0x3b);
    sw(desc + 0x40, 0x80808080u);
    sw(desc + 0x44, 0x80808080u);
    sw(desc + 0x48, 0x80808080u);
    sw(desc + 0x4c, 0x80808080u);
    swc1(desc + 0x50, 0.0f);
    swc1(desc + 0x54, 0.0f);
    swc1(desc + 0x58, 1.0f);
    swc1(desc + 0x5c, 0.0f);
    swc1(desc + 0x60, 0.0f);
    swc1(desc + 0x64, 1.0f);
    swc1(desc + 0x68, 1.0f);
    swc1(desc + 0x6c, 1.0f);
    sd(desc + 0x70, 4ull);
    sd(desc + 0x78, tex0);
    sd(desc + 0x80, GS_TEX1_LINEAR);
    sd(desc + 0x88, GS_ALPHA_ADD);

    V(desc + 0x0) = V(origin);
    vec3_add(V(desc + 0x10), V(desc + 0x0), V(side));
    V(desc + 0x20) = V(end_pt);
    V(desc + 0x30) = V(end_off);
    quad_draw(desc, 0);

    ee_stack_free(0x210);
}

// Draws the Snagglebeast's spherical shield shell around chain point 9 with effect texture 0x3c and scrolling UVs.
// orig: L7:0x00312948
void um2_312948_l7(uint32_t m, uint32_t frame_off, float rate, float radius, float span, float alpha_lo, float alpha_hi) {
    const uint32_t sp = ee_stack_alloc(0x1c0);
    const uint32_t zbuf_base = 0x01000000u | static_cast<uint32_t>(lw(0x0015ee88) >> 13);
    sub_002a0ba8(0x4e, static_cast<uint64_t>(zbuf_base) | (uint64_t{0x8000} << 17));

    const uint32_t glow_b = lbu(lwu(m + 0x78) + 0x1c6);
    const uint32_t glow_rep = (glow_b << 24) | (glow_b << 16) | (glow_b << 8) | glow_b;
    const uint32_t rgb = glow_rep | 0x00404040u;

    const uint64_t tex0 = effect_tex0(0x3c);
    const uint32_t c0 = (static_cast<uint32_t>(float_to_int(fpu_mul(int_to_float(0), alpha_lo))) << 24) | rgb;
    sw(sp + 0x40, c0);
    sw(sp + 0x44, c0);
    sw(sp + 0x48, c0);
    sw(sp + 0x4c, c0);
    sd(sp + 0x70, 0);
    sd(sp + 0x78, tex0);
    sd(sp + 0x80, GS_TEX1_LINEAR);
    sd(sp + 0x88, GS_ALPHA_SHIELD);

    const float t = fpu_mul(int_to_float(static_cast<int32_t>(lwu(L7_ADDR(0x0015f5cc)) + frame_off)), rate);
    const uint32_t center = sp + 0x90, pole = sp + 0xa0, int_tmp = sp + 0xb0;
    moby_chain_point_v(m, 9, V(center));
    V(pole) = V(center);
    swc1(pole + 0x8, fpu_sub(lwc1(pole + 0x8), fpu_sub(radius, u2f(0x3f4ccccdu))));  // (0.8)

    float ip = lwc1(int_tmp);
    const float u0 = split_int_frac(&ip, fpu_mul(t, u2f(0xbc23d70au)));  // (-0.01)
    swc1(int_tmp, ip);
    const float u1 = fpu_add(u0, 1.0f);
    swc1(sp + 0x50, u0);
    swc1(sp + 0x60, u0);
    swc1(sp + 0x68, u1);
    swc1(sp + 0x58, u1);

    const float v0 = split_int_frac(&ip, fpu_mul(t, u2f(0xbca3d70au)));  // (-0.02)
    swc1(int_tmp, ip);
    const float v1 = fpu_add(v0, 1.0f);
    swc1(sp + 0x54, v0);
    swc1(sp + 0x5c, v0);
    swc1(sp + 0x6c, v1);
    swc1(sp + 0x64, v1);

    const float alpha_step = fpu_mul(fpu_sub(alpha_hi, alpha_lo), 0.0625f);
    const float lat_step = fpu_mul(span, 0.0625f);

    float lon = u2f(0xc02fede0u);  // (-7 * pi / 8)
    for (;;) {
        V(sp + 0x20) = V(pole);
        V(sp + 0x30) = V(pole);
        const float next_lon = fpu_add(lon, u2f(0x3ec90fdbu));  // (pi / 8)
        float alpha = alpha_lo;
        const uint32_t cpole = (static_cast<uint32_t>(float_to_int(fpu_mul(int_to_float(0), alpha))) << 24) | rgb;
        sw(sp + 0x48, cpole);
        sw(sp + 0x4c, cpole);

        const float max_lat = fpu_sub(span, u2f(0x3fc90fdbu));  // (pi / 2)
        float lat = fpu_add(lat_step, u2f(0xbfc90fdbu));        // (-pi / 2)
        if (lat <= max_lat) {
            do {
                V(sp + 0x0) = V(sp + 0x20);
                V(sp + 0x10) = V(sp + 0x30);
                sw(sp + 0x40, lwu(sp + 0x48));
                sw(sp + 0x44, lwu(sp + 0x4c));
                alpha = fpu_add(alpha, alpha_step);
                const uint32_t c_ring =
                    (static_cast<uint32_t>(float_to_int(fpu_mul(int_to_float(0), alpha))) << 24) | rgb;
                sw(sp + 0x48, c_ring);
                sw(sp + 0x4c, c_ring);

                const float a0 = angle_wrap_mod(fpu_add(lon, fpu_mul(t, u2f(0x3ca3d70au))));  // (0.02)
                vec_from_spherical_v(V(sp + 0x20), radius, a0, lat);
                const float a1 = angle_add(a0, u2f(0x3ec90fdbu));
                vec_from_spherical_v(V(sp + 0x30), radius, a1, lat);
                if (0.0f < lat) {
                    const float dz = u2f(0x3f19999au);  // (0.6)
                    swc1(sp + 0x28, fpu_add(lwc1(sp + 0x28), dz));
                    swc1(sp + 0x38, fpu_add(lwc1(sp + 0x38), dz));
                } else {
                    swc1(sp + 0x28, fpu_sub(lwc1(sp + 0x28), 0.375f));
                    swc1(sp + 0x38, fpu_sub(lwc1(sp + 0x38), 0.375f));
                }
                lat = fpu_add(lat, lat_step);
                vec3_add(V(sp + 0x20), V(sp + 0x20), V(center));
                vec3_add(V(sp + 0x30), V(sp + 0x30), V(center));
                quad_draw(sp, 0);
            } while (lat <= max_lat);
        }
        lon = next_lon;
        if (!(lon < u2f(0x40490fdbu))) break;  // (pi)
    }

    sub_002a0ba8(0x4e, 0x01000000ull | static_cast<uint32_t>(lw(0x0015ee88) >> 13));
    ee_stack_free(0x1c0);
}

// Snagglebeast draw B: maps the shield glow progress (+0x1c8) to the sphere's latitude span and invokes 0x312948.
// orig: L7:0x00312dc8
void umb_class1106_draw_b_l7(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float glow = lwc1(pv + 0x1c8);
    const float knee = u2f(0x3f4ccccdu);  // (0.8)
    const float pi = u2f(0x40490fdbu);
    float span = pi;
    if (glow < knee) {
        const float s = fpu_add(fpu_div(fpu_mul(glow, u2f(0x4042c75cu)), knee), u2f(0x3dc90fdbu));
        if (!(pi < s)) span = s;
    }
    um2_312948_l7(m, 0, 1.0f, u2f(0x40b9999au), span, 1.0f, lwc1(pv + 0x1c8));
}

// Draws one spinning tapered beam cone + cap from pvars +0x110 -> +0x140 -> +0x120 with effect texture 0x3d.
// orig: L7:0x00312e70
void um2_312e70_l7(uint32_t m, uint32_t frame_off, uint32_t color, float radius, float spin_rate) {
    const uint32_t pv = lwu(m + 0x78);
    float spin = fpu_mul(int_to_float(static_cast<int32_t>(lwu(L7_ADDR(0x0015f5cc)) + frame_off)), spin_rate);
    const int32_t sub = lw(pv + 0x164);
    if (sub == 4) {
        const float base_a = int_to_float(static_cast<int32_t>(color) >> 24);
        const float rem_f = int_to_float(lw(pv + 0x160));
        const float denom = static_cast<float>(frame_scale_i(0x14));
        float a = fpu_mul(base_a, fpu_div(rem_f, denom));
        if (a < 0.0f) a = 0.0f;
        color = (color & 0x00ffffffu) | (static_cast<uint32_t>(float_to_int(a)) << 24);
    } else if (!(sub < 5)) {
        return;
    }

    const uint32_t sp = ee_stack_alloc(0x210);
    const uint32_t desc_a = sp + 0x0, desc_b = sp + 0x90, ring_off = sp + 0x120;
    const uint64_t tex0 = effect_tex0(0x3d);

    sw(desc_a + 0x40, color);
    sw(desc_a + 0x44, color);
    sw(desc_a + 0x48, color);
    sw(desc_a + 0x4c, color);
    swc1(desc_a + 0x50, 0.0f);
    swc1(desc_a + 0x54, 0.0f);
    swc1(desc_a + 0x58, 1.0f);
    swc1(desc_a + 0x5c, 0.0f);
    swc1(desc_a + 0x60, 0.0f);
    swc1(desc_a + 0x64, 0.5f);
    swc1(desc_a + 0x68, 1.0f);
    swc1(desc_a + 0x6c, 0.5f);
    sd(desc_a + 0x70, 4ull);
    sd(desc_a + 0x78, tex0);
    sd(desc_a + 0x80, GS_TEX1_LINEAR);
    sd(desc_a + 0x88, GS_ALPHA_ADD);
    V(desc_a + 0x10) = V(pv + 0x110);
    V(desc_a + 0x30) = V(pv + 0x140);

    copy_quad_desc(desc_b, desc_a);
    V(desc_b + 0x0) = V(pv + 0x120);
    V(desc_b + 0x10) = V(pv + 0x120);

    const float step_a = u2f(0x3f060a92u);  // (pi / 6)
    const float start_a = u2f(0xc0490fdbu);
    const float end_a = fpu_add(step_a, u2f(0x40490fdbu));
    float ang = start_a;
    if (ang < end_a) {
        spin = fpu_mul(spin, u2f(0x3ca3d70au));  // (0.02)
        do {
            V(desc_a + 0x0) = V(desc_a + 0x10);
            V(desc_a + 0x20) = V(desc_a + 0x30);
            const float a = angle_wrap_mod(fpu_add(ang, spin));
            swc1(ring_off + 0x4, fpu_mul(radius, cos_vu(a)));
            swc1(ring_off + 0x8, fpu_mul(radius, sin_vu(a)));
            sw(ring_off + 0x0, 0);
            mat3_transform(V(ring_off), V(ring_off), M(m + 0xc0));
            vec3_add(V(desc_a + 0x30), V(pv + 0x140), V(ring_off));
            if (!(ang == start_a)) {
                quad_draw(desc_a, 0);
                V(desc_b + 0x20) = V(desc_a + 0x20);
                V(desc_b + 0x30) = V(desc_a + 0x30);
                quad_draw(desc_b, 0);
            }
            ang = fpu_add(ang, step_a);
        } while (ang < end_a);
    }

    ee_stack_free(0x210);
}

// Snagglebeast draw C: updates the pillar-charge beam endpoints (+0x140 at 0.8 * len, +0x120 at len), draws three layered
// cones via 0x312e70 and tests the beam segment / tip sphere with damage 1.000123 (0x3f800408) to shatter pillars.
// orig: L7:0x003131b0
void umb_class1106_draw_c_l7(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t origin = pv + 0x110, tip = pv + 0x120;

    vec_clear(V(sp));
    vec3_set_length(V(sp), V(pv + 0x130), fpu_mul(lwc1(pv + 0x16c), u2f(0x3f4ccccdu)));  // (0.8)
    vec3_add(V(pv + 0x140), V(sp), V(origin));
    vec3_set_length(V(sp), V(sp), lwc1(pv + 0x16c));
    vec3_add(V(tip), V(sp), V(origin));

    um2_312e70_l7(m, 0, 0x20000080u, 1.0f, 1.0f);
    um2_312e70_l7(m, 0xa, 0x28008080u, u2f(0x3f333333u), 2.0f);
    um2_312e70_l7(m, 0x14, 0x30b0ffffu, u2f(0x3ecccccdu), 3.0f);

    const uint32_t hit_desc = sp + 0x10, dir = sp + 0x40;
    vec3_sub(V(dir), V(tip), V(origin));
    vec3_set_length(V(dir), V(dir), 1.0f);
    sub_00259888(u2f(0x3f800408u), hit_desc, static_cast<int32_t>(m), 0x30000, dir);
    if (segment_collide_v(V(origin), V(tip), 0, m, static_cast<const uint32_t*>(ee_ram_ptr(hit_desc))) == 0) {
        mobys_in_sphere(tip, 0.5f, 0, m, hit_desc);
    }

    ee_stack_free(0xc0);
}

// Draws the Snagglebeast's 9-ring x 20-segment suction vortex tube (rings at 0x20daa0 / 0x20dbe0, per-ring alphas at
// 0x20dbc8, UV scroll at 0x161b74..0x161b7c) with effect texture 0x15.
// orig: L7:0x00317910
void umb_draw_317910_l7(uint32_t) {
    const uint32_t sp = ee_stack_alloc(0x1e0);
    sd(sp + 0x70, 0);
    sd(sp + 0x78, effect_tex0(0x15));
    sd(sp + 0x80, GS_TEX1_LINEAR);
    sd(sp + 0x88, GS_ALPHA_ADD);

    float u_scroll = lwc1(L7_ADDR(0x00161b74));
    float next_u = fpu_add(u_scroll, u2f(0x3a83126fu));  // (0.001)
    if (7.0f < next_u) next_u = fpu_sub(next_u, 7.0f);
    swc1(L7_ADDR(0x00161b74), next_u);

    sub_002a0ba8(0x47, 0x5301bu);
    const uint32_t zbuf_base = 0x01000000u | static_cast<uint32_t>(lw(0x0015ee88) >> 13);
    sub_002a0ba8(0x4e, static_cast<uint64_t>(zbuf_base) | (uint64_t{0x8000} << 17));

    const float u_step = u2f(0x3e99999au);  // (0.3)
    const float v_step = u2f(0x3d4ccccdu);  // (0.05)
    u_scroll = fpu_add(u_scroll, u_step);

    for (int32_t ring = 1; ring < 10; ++ring) {
        float v_scroll = lwc1(L7_ADDR(0x00161b78));
        float next_v = fpu_add(v_scroll, lwc1(L7_ADDR(0x00161b7c)));
        if (7.0f < next_v) next_v = fpu_sub(next_v, 7.0f);
        swc1(L7_ADDR(0x00161b78), next_v);

        const uint32_t ring_off = static_cast<uint32_t>(ring * 0x140);
        const float u_next_ring = fpu_add(u_scroll, u_step);
        const float u_prev_ring = fpu_sub(u_scroll, u_step);
        const uint32_t alpha_prev_addr = L7_ADDR(0x0020dbc8) + static_cast<uint32_t>((ring - 1) << 1);
        const uint32_t alpha_curr_addr = L7_ADDR(0x0020dbc8) + static_cast<uint32_t>(ring << 1);

        for (int32_t seg = 0; seg < 20; ++seg) {
            const int32_t next_seg = ee_rem(seg + 1, 20);
            const uint32_t off0 = ring_off + static_cast<uint32_t>(seg << 4);
            const uint32_t off1 = ring_off + static_cast<uint32_t>(next_seg << 4);
            V(sp + 0x0) = V(L7_ADDR(0x0020dbe0) + off0);
            V(sp + 0x10) = V(L7_ADDR(0x0020dbe0) + off1);
            V(sp + 0x20) = V(L7_ADDR(0x0020daa0) + off0);
            V(sp + 0x30) = V(L7_ADDR(0x0020daa0) + off1);

            const int32_t a_prev = lh(alpha_prev_addr);
            const int32_t a_curr = lh(alpha_curr_addr);
            const uint32_t c_curr = pack_vortex_color(a_curr);
            const uint32_t c_prev = pack_vortex_color(a_prev);
            sw(sp + 0x40, c_curr);
            sw(sp + 0x44, c_curr);
            sw(sp + 0x48, c_prev);
            sw(sp + 0x4c, c_prev);

            if (ring == 1) {
                sw(sp + 0x48, 0x20404040u);
                sw(sp + 0x4c, 0x20404040u);
            } else if (ring == 9) {
                sw(sp + 0x40, 0x00404040u);
                sw(sp + 0x44, 0x00404040u);
            }

            const float v_next = fpu_add(v_scroll, v_step);
            swc1(sp + 0x50, u_scroll);
            swc1(sp + 0x54, v_scroll);
            swc1(sp + 0x58, u_scroll);
            swc1(sp + 0x5c, v_next);
            swc1(sp + 0x60, u_prev_ring);
            swc1(sp + 0x64, v_scroll);
            swc1(sp + 0x68, u_prev_ring);
            swc1(sp + 0x6c, v_next);
            v_scroll = v_next;

            quad_draw(sp, 0);
        }
        u_scroll = u_next_ring;
    }

    sub_002a0ba8(0x4e, 0x01000000ull | static_cast<uint32_t>(lw(0x0015ee88) >> 13));
    sub_002a0ba8(0x47, 0x5360bu);
    ee_stack_free(0x1e0);
}

// Class 1046 draw (unreferenced by the game's updates, registered for completeness): draws ten concentric 20-segment upper
// and lower shockwave ring skirts using effect texture pvars +0x18, radius +0x8 and height +0x10.
// orig: L7:0x0030dcf0
void umb_class1046_draw_l7(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x220);
    const uint32_t desc_a = sp + 0x0, desc_b = sp + 0x90, int_tmp = sp + 0x120;
    const uint32_t pv = lwu(m + 0x78);

    const uint32_t alpha_hi = ((lbu(m + 0x23) / 5u) & 0xffu) << 24;
    const uint32_t shade_step = (0x80u / 0xbu) & 0xffu;
    sd(desc_a + 0x70, 4ull);
    sd(desc_a + 0x78, effect_tex0(lw(pv + 0x18)));
    sd(desc_a + 0x80, GS_TEX1_LINEAR);
    sd(desc_a + 0x88, GS_ALPHA_ADD);

    uint32_t shade = 0x80;
    int32_t layer = 0;
    float t = 0.0f;
    const float knee = u2f(0x3f666666u);        // (0.9)
    const float inner_base = u2f(0x3f75c28fu);  // (0.96)
    const float inner_tap = u2f(0x3cf5c28fu);   // (0.03)
    const float ang_step = u2f(0x3ea0d97cu);    // (pi / 10)
    const float pi = u2f(0x40490fdbu);

    do {
        const uint32_t col = alpha_hi | (shade << 16) | (shade << 8) | 0x80u;
        sw(desc_a + 0x40, col);
        sw(desc_a + 0x44, col);
        sw(desc_a + 0x48, col);
        sw(desc_a + 0x4c, col);

        const float cur_r = lwc1(pv + 0x8);
        const float r = cur_r < knee ? fpu_mul(cur_r, t) : fpu_sub(cur_r, fpu_mul(t, knee));
        const float h = fpu_mul(fpu_sub(1.0f, fpu_mul(t, 0.5f)), lwc1(pv + 0x10));

        const float fc = int_to_float(lw(L7_ADDR(0x0015f5cc)));
        float ip = lwc1(int_tmp);
        float u0 = split_int_frac(&ip, fpu_div(fc, fpu_add(fpu_mul(t, 40.0f), 40.0f)));
        swc1(int_tmp, ip);
        if ((layer & 1) == 0) u0 = -u0;

        int32_t reps = layer + 1;
        if (reps >= 6) reps = 5;
        swc1(desc_a + 0x60, u0);
        swc1(desc_a + 0x50, u0);
        const float u1 = fpu_add(u0, int_to_float(reps));
        layer += 1;
        shade -= shade_step;

        const float mx = lwc1(m + 0x10);
        const float my = lwc1(m + 0x14);
        const float mz = lwc1(m + 0x18);
        const float inner_r0 = fpu_mul(r, fpu_sub(inner_base, fpu_mul(t, inner_tap)));
        const float top_z = fpu_add(h, mz);

        swc1(desc_a + 0x8, top_z);
        swc1(desc_a + 0x10, fpu_sub(mx, inner_r0));
        swc1(desc_a + 0x14, my);
        swc1(desc_a + 0x18, top_z);
        swc1(desc_a + 0x28, mz);
        swc1(desc_a + 0x30, fpu_sub(mx, r));
        swc1(desc_a + 0x34, my);
        swc1(desc_a + 0x38, mz);
        swc1(desc_a + 0x54, 0.0f);
        swc1(desc_a + 0x58, u1);
        swc1(desc_a + 0x5c, 0.0f);
        swc1(desc_a + 0x64, 1.0f);
        swc1(desc_a + 0x68, u1);
        swc1(desc_a + 0x6c, 1.0f);

        copy_quad_desc(desc_b, desc_a);
        const float bot_z = fpu_sub(mz, 5.0f);
        swc1(desc_b + 0x8, bot_z);
        swc1(desc_b + 0x18, bot_z);

        float ang = u2f(0xc034f4acu);  // (-9 * pi / 10)
        do {
            V(desc_a + 0x0) = V(desc_a + 0x10);
            V(desc_a + 0x20) = V(desc_a + 0x30);
            const float ca0 = cos_vu(ang);
            const float ir = fpu_mul(r, fpu_sub(inner_base, fpu_mul(t, inner_tap)));
            swc1(desc_a + 0x10, fpu_add(lwc1(m + 0x10), fpu_mul(ir, ca0)));
            const float ca1 = cos_vu(ang);
            swc1(desc_a + 0x30, fpu_add(lwc1(m + 0x10), fpu_mul(r, ca1)));
            const float sa0 = sin_vu(ang);
            const float cur_ang = ang;
            ang = fpu_add(ang, ang_step);
            swc1(desc_a + 0x14, fpu_add(lwc1(m + 0x14), fpu_mul(ir, sa0)));
            const float sa1 = sin_vu(cur_ang);
            swc1(desc_a + 0x34, fpu_add(lwc1(m + 0x14), fpu_mul(r, sa1)));
            quad_draw(desc_a, 0);

            V(desc_b + 0x0) = V(desc_b + 0x10);
            V(desc_b + 0x20) = V(desc_b + 0x30);
            const float x3 = lwc1(desc_a + 0x30);
            const float y3 = lwc1(desc_a + 0x34);
            const float x1 = lwc1(desc_a + 0x10);
            const float y1 = lwc1(desc_a + 0x14);
            swc1(desc_b + 0x30, x3);
            swc1(desc_b + 0x34, y3);
            swc1(desc_b + 0x10, x1);
            swc1(desc_b + 0x14, y1);
            quad_draw(desc_b, 0);
        } while (ang < pi);

        t = fpu_add(t, u2f(0x3dcccccdu));  // (0.1)
        shade &= 0xffu;
    } while (t < 1.0f);

    ee_stack_free(0x220);
}

}  // namespace rac1
