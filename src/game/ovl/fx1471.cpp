// Ported overlay code: class-1471 effect manager's beams (the class-1440 shooting robot's beam).
#include <game/ovl/fx1471.h>

#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>

namespace rac1 {

// orig: 0x002e2250 — advance beam slot `p1`'s two 19-point local waves, update impact point,
// spawn muzzle/impact smoke particles, and update dynamic lights.
void fx1471_effect_start(uint32_t p0, uint32_t p1) {
    (void)p0;
    const uint32_t pos_buf = ee_stack_alloc(0x10);

    const uint32_t strip0 = p1 << 1;
    const uint32_t strip1 = strip0 + 1u;
    const uint32_t off64 = p1 << 6;
    const uint32_t off16 = p1 << 4;
    const uint32_t off4 = p1 << 2;

    Vec4 span{};
    vec3_sub(span, V(OV(0x001e6de0u) + off16), V(OV(0x001e6d50u) + off64));
    const float seg_step = fpu_div(vec3_length(span), 20.0f);

    const uint32_t state_slot = OV(0x00161bf8u) + off4;
    if (lw(state_slot) == 2) {
        const uint32_t pts_a = OV(0x001e6e40u) + strip0 * 0x140u;
        const uint32_t pts_b = OV(0x001e6e40u) + strip1 * 0x140u;
        sw(state_slot, 3);
        for (int32_t k = 0; k < 20; ++k) {
            const float x = fpu_mul(seg_step, static_cast<float>(k));
            const uint32_t pa = pts_a + static_cast<uint32_t>(k * 0x10);
            const uint32_t pb = pts_b + static_cast<uint32_t>(k * 0x10);
            swc1(pa + 8, 0.0f);
            swc1(pa + 4, 0.0f);
            swc1(pa + 0xc, 1.0f);
            swc1(pa + 0, x);
            swc1(pb + 0, x);
            swc1(pb + 8, 0.0f);
            swc1(pb + 4, 0.0f);
            swc1(pb + 0xc, 1.0f);
        }
    }

    const uint32_t ang0_slot = OV(0x00161c08u) + off4;
    const uint32_t ang1_slot = OV(0x00161c18u) + off4;
    const uint32_t ang2_slot = OV(0x00161c28u) + off4;
    swc1(ang0_slot, angle_add(lwc1(ang0_slot), u2f(0x3e32b8c2u)));
    swc1(ang1_slot, angle_sub(lwc1(ang1_slot), u2f(0x3f4d87acu)));
    swc1(ang2_slot, angle_add(lwc1(ang2_slot), u2f(0x3d567770u)));

    const float wave_freq = fpu_mul(fpu_div(360.0f, fpu_mul(int_to_float(20), 0.5f)), u2f(0x3c8efa35u));
    const uint32_t pts0 = OV(0x001e6e40u) + strip0 * 0x140u;
    for (int32_t i = 0; i < 19; ++i) {
        const float fi = static_cast<float>(i);
        const float t = fpu_div(int_to_float(i), int_to_float(20));
        const float ph = angle_wrap_mod(fpu_add(lwc1(ang0_slot), fpu_mul(fi, wave_freq)));
        const float amp = fpu_add(fpu_mul(t, u2f(0x3e99999au)), u2f(0x3dcccccdu));
        const float z = fpu_mul(amp, sin_vu(ph));
        const uint32_t pt = pts0 + static_cast<uint32_t>(i * 0x10);
        sw(pt + 4, 0);
        swc1(pt + 0xc, 1.0f);
        swc1(pt + 8, z);
        swc1(pt + 0, fpu_mul(seg_step, fi));
    }

    Mat4 basis{};
    basis.r[3] = V(OV(0x001e6d50u) + off64);
    Vec4 diff{};
    vec3_sub(diff, V(OV(0x001e6de0u) + off16), basis.r[3]);
    vec3_set_length(basis.r[0], diff, 1.0f);
    vec3_cross_ba(basis.r[1], basis.r[0], V(0x0013f5e0u));
    vec3_set_length(basis.r[1], basis.r[1], -1.0f);
    vec3_cross_ba(basis.r[2], basis.r[1], basis.r[0]);

    Vec4 impact_pos{};
    mat4_transform(impact_pos, V(OV(0x001e6f60u) + strip0 * 0x140u), basis);
    V(OV(0x001e6e10u) + off16) = impact_pos;

    const uint32_t pts1 = OV(0x001e6e40u) + strip1 * 0x140u;
    for (int32_t i = 0; i < 19; ++i) {
        const float fi = static_cast<float>(i);
        const uint32_t pt = pts1 + static_cast<uint32_t>(i * 0x10);
        sw(pt + 8, 0);
        const float ph1 = angle_wrap_mod(fpu_add(lwc1(ang1_slot), fpu_mul(fi, u2f(0x3f7eadaeu))));
        const float ph2 = angle_wrap_mod(fpu_add(lwc1(ang2_slot), fpu_mul(fi, u2f(0x3e567770u))));
        const float env = fpu_mul(sin_vu(ph2), 0.5f);
        const float s1 = sin_vu(ph1);
        const float r_jit = rand_range_signed_f(0.0f, u2f(0x3e4ccccdu));
        const float z = fpu_add(lwc1(pt + 8), fpu_add(fpu_mul(env, s1), r_jit));
        swc1(pt + 8, z);
        const float y = fpu_mul(cos_vu(ph1), u2f(0x3e99999au));
        swc1(pt + 0xc, 1.0f);
        swc1(pt + 4, y);
        swc1(pt + 0, fpu_mul(seg_step, fi));
    }

    const uint32_t muzzle_mat = OV(0x001e6d20u) + off64;
    const uint32_t moby_slot = OV(0x00161c88u) + off4;
    const float spd0 = rand_range_f(u2f(0x3c23d70au), u2f(0x3d4ccccdu));
    const float ang_m = rand_range_signed_f(u2f(0x3eb2b8c2u), u2f(0x3f32b8c2u));
    Vec4 vel{fpu_mul(spd0, cos_vu(ang_m)), fpu_mul(spd0, sin_vu(ang_m)), 0.0f, 1.0f};
    mat3_transform(vel, vel, M(muzzle_mat));

    const uint32_t muzzle_pos = muzzle_mat + 0x30;
    spawn_particle_45_v(V(muzzle_pos), vel, 0x7fu, lwu(moby_slot));
    if (rand_mod(3) == 0) {
        Vec4 fwd_pos{};
        vec3_scale(fwd_pos, V(muzzle_mat), u2f(0x3e99999au));
        vec3_add(fwd_pos, fwd_pos, V(muzzle_pos));
        const uint32_t p = spawn_particle_45_v(fwd_pos, V(OV(0x0015f580u)), 0x7fu, lwu(moby_slot));
        if (p != 0) {
            const uint32_t q = p + 0x20;
            swc1(p + 0xc, u2f(0x47ea6000u));
            const int32_t life = frame_scale_i(4);
            sh(p + 0xa, static_cast<uint32_t>(life));
            const float inv_life = fpu_div(1.0f, int_to_float(static_cast<int16_t>(life)));
            sw(q + 0x18, 0x007f7f7fu);
            swc1(q + 0x10, inv_life);
            const uint32_t kind = lwu(OV(0x00161c68u) + off4);
            if (kind == 1u) sh(q + 0x16, kind);
        }
    }

    const float k_lo = u2f(0x3c23d70au);
    const float k_hi = u2f(0x3d4ccccdu);
    (void)rand_range_f(k_lo, k_hi);  // original calls rand_range_f before rand_dir_scaled
    rand_dir_scaled_v(vel, k_lo, k_hi);
    spawn_particle_45_v(impact_pos, vel, 0x7fu, lwu(moby_slot));
    if (rand_mod(3) == 0) {
        const uint32_t p = spawn_particle_45_v(impact_pos, V(OV(0x0015f580u)), 0x7fu, lwu(moby_slot));
        if (p != 0) {
            const uint32_t q = p + 0x20;
            swc1(p + 0xc, u2f(0x47ea6000u));
            const int32_t life = frame_scale_i(4);
            sh(p + 0xa, static_cast<uint32_t>(life));
            const float inv_life = fpu_div(1.0f, int_to_float(static_cast<int16_t>(life)));
            sw(q + 0x18, 0x007f7f7fu);
            swc1(q + 0x10, inv_life);
            if (lw(OV(0x00161c68u) + off4) == 1) sh(q + 0x16, 2);
        }
    }

    const uint32_t flags = lwu(OV(0x00161c58u) + off4);
    if (flags != 0) {
        if ((flags & 1u) != 0) {
            V(pos_buf) = V(OV(0x001e6d50u) + off64);
            swc1(pos_buf + 8, fpu_sub(lwc1(pos_buf + 8), 0.25f));
            const uint32_t lslot_addr = OV(0x00161c38u) + off4;
            if (lw(lslot_addr) == -1) {
                const int32_t nl = light_add(pos_buf, lwc1(OV(0x00161bf0u)), 0.0f, 1.0f, 1.0f, 2.0f);
                sw(lslot_addr, static_cast<uint32_t>(nl));
            }
            const int32_t lslot = lw(lslot_addr);
            if (lslot >= 0) {
                const uint32_t entry = OV(0x001802c0u) + (static_cast<uint32_t>(lslot) << 5);
                V(entry + 0x10) = V(pos_buf);
                swc1(entry + 0x1c, lwc1(OV(0x00161bf0u)));
            }
        }
        if ((lwu(OV(0x00161c58u) + off4) & 2u) != 0) {
            impact_pos.z = fpu_add(impact_pos.z, 0.5f);
            const uint32_t lslot_addr = OV(0x00161c48u) + off4;
            if (lw(lslot_addr) == -1) {
                V(pos_buf) = impact_pos;
                const int32_t nl = light_add(pos_buf, lwc1(OV(0x00161bf0u)), 0.0f, 1.0f, 1.0f, 2.0f);
                sw(lslot_addr, static_cast<uint32_t>(nl));
            }
            const int32_t lslot = lw(lslot_addr);
            if (lslot >= 0) {
                const uint32_t entry = OV(0x001802c0u) + (static_cast<uint32_t>(lslot) << 5);
                V(entry + 0x10) = impact_pos;
                swc1(entry + 0x1c, lwc1(OV(0x00161bf0u)));
            }
        }
    }

    ee_stack_free(0x10);
}

// orig: 0x002e3388 — draw core + halo billboarded glow quads at both muzzle and impact of beam `p1`.
void fx1471_beam_prepare(uint32_t p0, uint32_t p1) {
    (void)p0;
    const uint32_t q = ee_stack_alloc(0x90);
    const uint32_t cam = OV(0x00166dc0u);
    const uint32_t tmpl = OV(0x001e6ce0u);

    Mat4 basis{};
    basis.r[3] = V(OV(0x001e6d50u) + (p1 << 6));
    basis.r[3].w = 1.0f;
    vec3_sub(basis.r[0], V(cam), basis.r[3]);
    vec3_set_length(basis.r[0], basis.r[0], 1.0f);
    vec3_cross_ba(basis.r[1], basis.r[0], V(0x0013f5e0u));
    vec3_set_length(basis.r[1], basis.r[1], -1.0f);
    vec3_cross_ba(basis.r[2], basis.r[1], basis.r[0]);

    const uint64_t tex = static_cast<uint64_t>(effect_tex0(0xb));
    sd(q + 0x78, tex);
    swc1(q + 0x6c, 1.0f);
    swc1(q + 0x5c, 1.0f);
    swc1(q + 0x60, 1.0f);
    swc1(q + 0x68, 1.0f);
    sd(q + 0x88, 0x000000ff00000044ull);
    sd(q + 0x80, 0x0000ff9000000260ull);
    sd(q + 0x70, 5ull);
    sw(q + 0x40, 0xffffffffu);
    sw(q + 0x4c, 0xffffffffu);
    sw(q + 0x48, 0xffffffffu);
    sw(q + 0x44, 0xffffffffu);
    swc1(q + 0x50, 0.0f);
    swc1(q + 0x54, 0.0f);
    swc1(q + 0x58, 0.0f);
    swc1(q + 0x64, 0.0f);

    const float sz_muz = rand_range_f(u2f(0x3e99999au), u2f(0x3ea66666u));
    const float jit_range = u2f(0x3d4ccccdu);
    Vec4 jitter{};
    jitter.x = rand_range_signed_f(0.0f, jit_range);
    jitter.y = rand_range_signed_f(0.0f, jit_range);
    jitter.z = rand_range_signed_f(0.0f, jit_range);

    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t vdst = q + k * 0x10;
        vec3_scale(V(vdst), V(tmpl + k * 0x10), sz_muz);
        mat4_transform(V(vdst), V(vdst), basis);
        vec3_add(V(vdst), V(vdst), jitter);
    }
    quad_draw(q, 0);

    sd(q + 0x88, 0x000000ff00000048ull);
    const uint32_t halo_col = 0x607f4040u;
    sw(q + 0x40, halo_col);
    sw(q + 0x4c, halo_col);
    sw(q + 0x48, halo_col);
    sw(q + 0x44, halo_col);
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t vdst = q + k * 0x10;
        const float s = fpu_mul(sz_muz, 3.0f);
        vec3_scale(V(vdst), V(tmpl + k * 0x10), s);
        mat4_transform(V(vdst), V(vdst), basis);
    }
    quad_draw(q, 0);

    basis.r[3] = V(OV(0x001e6e10u) + (p1 << 4));
    vec3_sub(basis.r[0], V(cam), basis.r[3]);
    vec3_set_length(basis.r[0], basis.r[0], 1.0f);
    vec3_cross_ba(basis.r[1], basis.r[0], V(0x0013f5e0u));
    vec3_set_length(basis.r[1], basis.r[1], -1.0f);
    vec3_cross_ba(basis.r[2], basis.r[1], basis.r[0]);

    const float sz_imp = rand_range_f(u2f(0x3e4ccccdu), u2f(0x3e666666u));
    sd(q + 0x88, 0x000000ff00000044ull);
    sw(q + 0x40, 0xffffffffu);
    sw(q + 0x4c, 0xffffffffu);
    sw(q + 0x48, 0xffffffffu);
    sw(q + 0x44, 0xffffffffu);
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t vdst = q + k * 0x10;
        vec3_scale(V(vdst), V(tmpl + k * 0x10), sz_imp);
        mat4_transform(V(vdst), V(vdst), basis);
        vec3_add(V(vdst), V(vdst), jitter);
    }
    quad_draw(q, 0);

    sd(q + 0x88, 0x000000ff00000048ull);
    sw(q + 0x40, halo_col);
    sw(q + 0x4c, halo_col);
    sw(q + 0x48, halo_col);
    sw(q + 0x44, halo_col);
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t vdst = q + k * 0x10;
        const float s = fpu_mul(sz_imp, 3.0f);
        vec3_scale(V(vdst), V(tmpl + k * 0x10), s);
        mat4_transform(V(vdst), V(vdst), basis);
    }
    quad_draw(q, 0);

    ee_stack_free(0x90);
}

// orig: 0x002e2c88 — draw one 19-point local beam strip (`p2`, slot `p5`) with inner (`p0`) and outer (`p1`) textures.
void fx1471_beam_strip(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    const uint32_t frame = ee_stack_alloc(0x120);
    const uint32_t q0 = frame;
    const uint32_t q1 = frame + 0x90;
    const uint32_t off4 = p5 << 2;

    const uint64_t tex0 = static_cast<uint64_t>(effect_tex0(static_cast<int32_t>(p0)));
    const uint32_t col0 = (p3 << 24) | 0x007f7f7fu;
    sd(q0 + 0x88, 0x0000008000000048ull);
    sd(q0 + 0x80, 0x0000ff9000000260ull);
    sd(q0 + 0x78, tex0);
    sw(q0 + 0x40, col0);
    sd(q0 + 0x70, 0ull);
    sw(q0 + 0x4c, col0);
    sw(q0 + 0x48, col0);
    sw(q0 + 0x44, col0);

    const uint64_t tex1 = static_cast<uint64_t>(effect_tex0(static_cast<int32_t>(p1)));
    float base_half_w = u2f(0x3f19999au);
    sw(q1 + 0x54, 0);
    const uint32_t col1 = (p4 << 24) | 0x007f4040u;
    const float u_phase = lwc1(OV(0x00161c78u) + off4);
    const float u1_init = fpu_add(u_phase, 1.0f);
    const float u0_init = fpu_add(u_phase, 0.0f);
    const uint32_t owner = lwu(OV(0x00161c88u) + off4);

    sd(q1 + 0x78, tex1);
    sw(q1 + 0x40, col1);
    sd(q1 + 0x88, ld(q0 + 0x88));
    sd(q1 + 0x80, ld(q0 + 0x80));
    sd(q1 + 0x70, ld(q0 + 0x70));
    swc1(q0 + 0x58, u0_init);
    swc1(q0 + 0x68, u1_init);
    sw(q1 + 0x4c, col1);
    sw(q1 + 0x48, col1);
    sw(q1 + 0x44, col1);
    swc1(q1 + 0x50, 1.0f);
    swc1(q1 + 0x58, 1.0f);
    swc1(q1 + 0x5c, 1.0f);
    sw(q1 + 0x60, 0);
    sw(q1 + 0x64, 0);
    sw(q1 + 0x68, 0);
    swc1(q1 + 0x6c, 1.0f);
    swc1(q0 + 0x50, u0_init);
    swc1(q0 + 0x54, 0.0f);
    swc1(q0 + 0x5c, 1.0f);
    swc1(q0 + 0x60, u1_init);
    swc1(q0 + 0x64, 0.0f);
    swc1(q0 + 0x6c, 1.0f);

    if (lh(owner + 0xa6) == 0x1c) {
        base_half_w = u2f(0x3e99999au);
    }
    const float outer_half_w0 = fpu_add(base_half_w, base_half_w);

    Vec4 off_inner{0.0f, 0.0f, base_half_w, 1.0f};
    Vec4 off_outer{0.0f, 0.0f, outer_half_w0, 1.0f};

    const uint32_t cam = OV(0x00166dc0u);
    const uint32_t muz_base = OV(0x001e6d30u) + (p5 << 6);
    const uint32_t muz_pos = muz_base + 0x20;

    Mat4 beam_frame{};
    beam_frame.r[3] = V(muz_pos);
    Vec4 span{};
    vec3_sub(span, V(OV(0x001e6de0u) + (p5 << 4)), beam_frame.r[3]);
    vec3_set_length(beam_frame.r[0], span, 1.0f);
    vec3_cross_ba(beam_frame.r[1], beam_frame.r[0], V(0x0013f5e0u));
    vec3_set_length(beam_frame.r[1], beam_frame.r[1], -1.0f);
    vec3_cross_ba(beam_frame.r[2], beam_frame.r[1], beam_frame.r[0]);

    V(q0 + 0x00) = off_inner;
    vec3_scale(V(q0 + 0x10), V(q0 + 0x00), -1.0f);
    V(q1 + 0x00) = off_outer;
    vec3_scale(V(q1 + 0x10), V(q1 + 0x00), -1.0f);

    Vec4 w0{};
    Vec4 w1{};
    mat4_transform(w0, V(p2), beam_frame);
    mat4_transform(w1, V(p2 + 0x10), beam_frame);

    Mat4 seg_basis{};
    vec3_sub(seg_basis.r[0], w1, w0);
    vec3_set_length(seg_basis.r[0], seg_basis.r[0], 1.0f);
    vec3_sub(seg_basis.r[1], V(cam), w0);
    vec3_cross_ba(seg_basis.r[2], seg_basis.r[1], seg_basis.r[0]);
    vec3_set_length(seg_basis.r[2], seg_basis.r[2], 1.0f);
    vec3_cross_ba(seg_basis.r[1], seg_basis.r[0], seg_basis.r[2]);
    seg_basis.r[3] = w0;

    mat4_transform(V(q0 + 0x00), V(q0 + 0x00), seg_basis);
    mat4_transform(V(q0 + 0x10), V(q0 + 0x10), seg_basis);
    mat4_transform(V(q1 + 0x00), V(q1 + 0x00), seg_basis);
    mat4_transform(V(q1 + 0x10), V(q1 + 0x10), seg_basis);

    uint32_t pt_a = p2 + 0x10;
    uint32_t pt_b = p2 + 0x20;
    for (int32_t i = 1; i < 19; ++i) {
        V(q0 + 0x20) = off_inner;
        vec3_scale(V(q0 + 0x30), V(q0 + 0x20), -1.0f);
        V(q1 + 0x20) = off_outer;
        vec3_scale(V(q1 + 0x30), V(q1 + 0x20), -1.0f);

        mat4_transform(w0, V(pt_a), beam_frame);
        mat4_transform(w1, V(pt_b), beam_frame);

        vec3_sub(seg_basis.r[0], w1, w0);
        vec3_set_length(seg_basis.r[0], seg_basis.r[0], 1.0f);
        vec3_sub(seg_basis.r[1], V(cam), w0);
        vec3_cross_ba(seg_basis.r[2], seg_basis.r[1], seg_basis.r[0]);
        vec3_set_length(seg_basis.r[2], seg_basis.r[2], 1.0f);
        vec3_cross_ba(seg_basis.r[1], seg_basis.r[0], seg_basis.r[2]);
        seg_basis.r[3] = w0;

        mat4_transform(V(q0 + 0x20), V(q0 + 0x20), seg_basis);
        mat4_transform(V(q0 + 0x30), V(q0 + 0x30), seg_basis);
        mat4_transform(V(q1 + 0x20), V(q1 + 0x20), seg_basis);
        mat4_transform(V(q1 + 0x30), V(q1 + 0x30), seg_basis);

        if (i == 1) {
            const uint32_t save0 = lwu(q0 + 0x40);
            const uint32_t save1 = lwu(q1 + 0x40);
            sw(q0 + 0x40, 0x00ffffffu);
            sw(q1 + 0x40, 0x007f4040u);
            sw(q0 + 0x44, 0x00ffffffu);
            sw(q1 + 0x44, 0x007f4040u);
            quad_draw(q0, 0);
            quad_draw(q1, 0);
            sw(q0 + 0x40, save0);
            sw(q1 + 0x40, save1);
            sw(q0 + 0x4c, save0);
            sw(q0 + 0x48, save0);
            sw(q0 + 0x44, save0);
            sw(q1 + 0x4c, save1);
            sw(q1 + 0x48, save1);
            sw(q1 + 0x44, save1);
        } else {
            quad_draw(q0, 0);
            quad_draw(q1, 0);
        }

        V(q0 + 0x00) = V(q0 + 0x20);
        V(q0 + 0x10) = V(q0 + 0x30);
        V(q1 + 0x00) = V(q1 + 0x20);
        V(q1 + 0x10) = V(q1 + 0x30);

        pt_b += 0x10;
        const float t = fpu_div(int_to_float(i), int_to_float(20));
        pt_a += 0x10;
        const float curve = cubic_interp(-1.0f, 0.0f, 1.0f, 0.0f, t);
        const float cur_u = lwc1(OV(0x00161c78u) + off4);
        const float hw = fpu_add(base_half_w, fpu_mul(fpu_sub(u2f(0x3d4ccccdu), base_half_w), curve));
        const float u1 = fpu_add(cur_u, 1.0f);
        const float u0 = fpu_add(cur_u, 0.0f);
        swc1(q0 + 0x54, 0.0f);
        swc1(q0 + 0x5c, 1.0f);
        swc1(q0 + 0x64, 0.0f);
        swc1(q0 + 0x6c, 1.0f);
        swc1(q0 + 0x68, u1);
        swc1(q0 + 0x58, u0);
        swc1(q0 + 0x50, u0);
        swc1(q0 + 0x60, u1);
        off_inner.z = hw;
        off_outer.z = fpu_add(hw, hw);
    }

    ee_stack_free(0x120);
}

// orig: 0x002e2af0 — class-1471 effect manager draw/update callback (iterates over the 3 beam slots).
void fx1471_draw(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    sub_002a0ba8(0x47u, 0x51001ull);

    for (uint32_t i = 0; i < 3; ++i) {
        const uint32_t off4 = i * 4u;
        const uint32_t owner = lwu(OV(0x00161c88u) + off4);
        if (owner == 0) continue;
        if (lw(OV(0x00161bf8u) + off4) < 2) continue;

        const uint32_t u_slot = OV(0x00161c78u) + off4;
        const float du = fpu_mul(lwc1(0x0015ed60u), u2f(0x3e4ccccdu));
        float u_next = fpu_sub(lwc1(u_slot), du);
        swc1(u_slot, u_next);
        if (u_next <= -8.0f) {
            u_next = fpu_add(u_next, 8.0f);
            swc1(u_slot, u_next);
        }

        fx1471_beam_prepare(owner, i);
        const uint32_t strip0 = i << 1;
        const uint32_t strip1 = strip0 + 1u;
        const uint32_t tex_in = lwu(pv + 4) + 0x28u;
        const uint32_t tex_out = lwu(pv + 0) + 0x28u;
        fx1471_beam_strip(tex_in, tex_out, OV(0x001e6e40u) + strip0 * 0x140u, 0x80u, 0x40u, i);
        fx1471_beam_strip(tex_in, tex_out, OV(0x001e6e40u) + strip1 * 0x140u, 0x30u, 0x10u, i);
    }
}

// orig: (none) -- the ports for the engine's indirect calls (a moby class's update, a draw callback)
void fx1471_fns_register() {
    ovl_register_moby_fn(0x002e2af0, fx1471_draw);
}

}  // namespace rac1
