// Ported overlay code: quad-drawing callbacks (see effect_quads.h).
#include <game/ovl/effect_quads.h>

#include <game/ovl/ovl_reloc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/quad_draw.h>

namespace rac1 {

// The frame as the original lays it out: the descriptor at +0, the matrix at +0x90, the spin's
// matrix at +0xd0, the Euler angles at +0x110 (direction) and +0x120 (spin).
// orig: 0x002acbf0
void bomb_discs_draw(uint32_t m) {
    if (lw(OV(0x0015f5c4)) == 2) return;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x140);
    const uint32_t mat = sp + 0x90, spin = sp + 0xd0, dir = sp + 0x110, ang = sp + 0x120;
    int32_t i = 0;
    do {
        const uint32_t odd = static_cast<uint32_t>(i) & 1;
        const float x = lwc1(pv + 0x20), z = lwc1(pv + 0x28);
        const float h = sqrt_f(fpu_add(fpu_mul(x, x), fpu_mul(z, z)));
        swc1(dir, angle_add(-atan2_f(h, lwc1(pv + 0x24)), u2f(0x3fc90fdbu)));
        swc1(dir + 4, atan2_f(lwc1(pv + 0x28), lwc1(pv + 0x20)));
        sw(dir + 8, 0);
        sw(dir + 0xc, 0);
        sw(ang, 0);
        const int32_t period = frame_scale_i(0xb4);
        const float t = int_to_float(ee_rem(lw(OV(0x0015f5cc)), period));
        const float frames = frame_scale_f(180.0f);
        float a = angle_wrap(fpu_div(fpu_mul(t, u2f(0x40c90fdbu)), frames));
        if (odd != 0) a = -a;
        swc1(ang + 4, a);
        sw(ang + 8, 0);
        sw(ang + 0xc, 0);
        euler_to_mat4_vu(M(spin), V(ang));
        euler_to_mat4_vu(M(mat), V(dir));
        mat4_mul(M(mat), M(mat), M(spin));
        lq_sq(mat + 0x30, pv + 0x10);
        for (uint32_t k = 0; k < 4; ++k) sw(sp + 0x40 + 4 * k, 0x80808080u);
        ++i;
        const uint64_t tex0 = effect_tex0(odd == 0 ? 0x11 : 0x12);
        sd(sp + 0x88, 0x8000000048ull);
        sd(sp + 0x78, tex0);
        sd(sp + 0x80, 0x0000ff9000000260ull);
        sd(sp + 0x70, 0);
        static const float corner[16] = {-1, 0, 1, 1, 1, 0, 1, 1, -1, 0, -1, 1, 1, 0, -1, 1};
        for (uint32_t k = 0; k < 16; ++k) swc1(sp + 4 * k, corner[k]);
        static const float uv[8] = {0, 1, 1, 1, 0, 0, 1, 0};
        for (uint32_t k = 0; k < 8; ++k) swc1(sp + 0x50 + 4 * k, uv[k]);
        quad_draw(sp, mat);
    } while (i < 2);
    ee_stack_free(0x140);
}

// orig: 0x002a7780
void wrench_trail_draw(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    const float t0 = lwc1(pv + 0x70);
    if (t0 == 0.0f) return;
    const uint32_t hero = lwu(0x001413d0);
    if (t0 == 1.0f) {
        const uint32_t a = lbu(hero + 0x52);
        if (a != lbu(hero + 0x53) || a == 0xff) {
            swc1(pv + 0x70, 0.0f);
            return;
        }
    }
    float scale = 1.0f, alpha = 1.0f;
    uint32_t tex = 0x14, rgb = 0x808080, blend = 1;
    const uint32_t anim = lbu(hero + 0x52);
    const uint32_t seq = (anim - 0x17 < 3 || anim == 0x2b) ? lbu(hero + 0x52) : static_cast<uint32_t>(static_cast<int32_t>(lh(pv + 0x78)));
    int32_t step;
    if (seq == 0x17) {
        sh(pv + 0x78, seq);
        step = 0;
    } else if (seq == 0x18) {
        sh(pv + 0x78, seq);
        step = 1;
    } else if (seq == 0x19) {
        sh(pv + 0x78, seq);
        step = 2;
    } else {
        sh(pv + 0x78, 0x2b);
        alpha = u2f(0x3f333333u);
        step = 3;
        scale = 1.25f;
        tex = 0xc;
        rgb = 0x408080;
        blend = 2;
    }
    const uint32_t r = OV(0x001d92e8) + static_cast<uint32_t>(step) * 0x60;
    const float t = lwc1(pv + 0x70);
    const int32_t lit[4] = {lh(r), lh(r + 2), lh(r + 4), lh(r + 6)};
    if (t == 1.0f) {  // the hero's matrix at the start
        lq_sq(pv, hero + 0xc0);
        lq_sq(pv + 0x10, hero + 0xd0);
        lq_sq(pv + 0x20, hero + 0xe0);
        sw(OV(0x001613c0), lwu(OV(0x0015f5cc)));
    }
    const uint32_t now = lwu(OV(0x0015f5cc));
    int32_t frames = static_cast<int32_t>(now - lwu(OV(0x001613c0)));
    if (!(0 < frames)) frames = 1;
    const float time = fpu_add(t, fpu_mul(static_cast<float>(frames), lwc1(r + 0x48)));
    swc1(pv + 0x70, time);
    sw(OV(0x001613c0), now);
    if (static_cast<float>(lh(r + 0x58) << 1) <= time) {  // over: kept while the swing goes on
        const uint32_t a = lbu(hero + 0x52);
        if (a - 0x17 < 3 || a == 0x2b) return;
        sw(pv + 0x70, 0);
        return;
    }
    const uint64_t tex0 = effect_tex0(static_cast<int32_t>(tex));
    const uint32_t sp = ee_stack_alloc(0x90);
    sd(sp + 0x78, tex0);
    sd(sp + 0x80, 0x0000ff9000000260ull);
    sd(sp + 0x70, 5);
    sd(sp + 0x88, 0x8000000040ull | (static_cast<uint64_t>(blend) << 2));
    lq_sq(pv + 0x30, hero + 0x10);
    for (int32_t q = 0; q < lh(r + 0x58);) {
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t pair = lwu(r + 0x50) + (static_cast<uint32_t>(q) << 4) + 4 * k;
            const int32_t vtx = lh(pair);
            uint32_t col = 0x50808080u;
            for (int j = 0; j < 4; ++j)
                if (q == lit[j]) {
                    col = lwu(r + 4 * k + 8 + 0x10 * static_cast<uint32_t>(j));
                    break;
                }
            sw(sp + 0x40 + 4 * k, col);
            const int32_t a = float_to_int(fpu_mul(static_cast<float>(static_cast<int8_t>(lbu(sp + 0x40 + 4 * k + 3))), alpha));
            sw(sp + 0x40 + 4 * k, (static_cast<uint32_t>(a) << 24) | rgb);
            vec3_scale(V(sp + 0x10 * k), V(lwu(r + 0x4c) + (static_cast<uint32_t>(vtx) << 4)), scale);
            const float k4 = lwc1(OV(0x001613bc));
            const uint32_t uv = (static_cast<uint32_t>(lh(pair + 2)) << 3) + lwu(r + 0x54);
            const float fade = fpu_sub(1.0f, fpu_mul(lwc1(pv + 0x70), lwc1(r + 0x5c)));
            swc1(sp + 0x50 + 8 * k, fpu_add(fpu_mul(fpu_sub(lwc1(uv), 0.5f), k4), 0.5f));
            swc1(sp + 0x54 + 8 * k, fpu_add(fpu_add(fpu_mul(fpu_sub(lwc1(uv + 4), 0.5f), k4), 0.5f), fade));
        }
        ++q;
        quad_draw(sp, pv);
    }
    ee_stack_free(0x90);
}

// orig: 0x002d19b0
void mirror_mesh_draw(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t view = sp + 0xb0, nrm = sp + 0x90, refl = sp + 0xa0;
    const uint32_t VTX = OV(0x001de7b0), NRM = OV(0x001df9a0), OUTV = OV(0x001e04a0), UV = OV(0x001e0b00), KEPT = OV(0x001e0e30), QUADS = OV(0x001dee10);
    const uint64_t tex0 = effect_tex0(lw(OV(0x001618f0)));
    const uint64_t alpha = static_cast<uint64_t>(static_cast<int64_t>(lw(OV(0x001618dc)))) | (static_cast<uint64_t>(static_cast<int64_t>(lw(OV(0x001618e0)))) << 2) |
                           (static_cast<uint64_t>(static_cast<int64_t>(lw(OV(0x001618e4)))) << 4) | (static_cast<uint64_t>(static_cast<int64_t>(lw(OV(0x001618e8)))) << 6) |
                           (static_cast<uint64_t>(static_cast<int64_t>(lw(OV(0x001618ec)))) << 32);
    const uint32_t rgba = lwu(OV(0x001618f4));
    sd(sp + 0x78, tex0);
    sd(sp + 0x80, 0x0000ff9000000260ull);
    sd(sp + 0x88, alpha);
    sd(sp + 0x70, 0);
    for (uint32_t k = 0; k < 4; ++k) sw(sp + 0x40 + 4 * k, rgba);
    bool near = false;
    if (abs_f(fpu_sub(lwc1(OV(0x00166dc0)), lwc1(m + 0x10))) < 16.0f && abs_f(fpu_sub(lwc1(OV(0x00166dc4)), lwc1(m + 0x14))) < 16.0f) near = true;
    if (near || lbu(m + 0x20) == 1) {
        sb(m + 0xbc, 1);
        timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x40)));
        const float ease = fpu_div(int_to_float(lw(pv + 0x40)), int_to_float(frame_scale_i(0x3c)));
        for (uint32_t i = 0; i < 0x66; ++i) {
            mat4_transform(V(OUTV + 0x10 * i), V(VTX + 0x10 * i), M(pv));
            vec3_sub(V(view), V(OUTV + 0x10 * i), V(OV(0x00166dc0)));
            vec3_set_length(V(view), V(view), 1.0f);
            mat4_transform(V(nrm), V(NRM + 0x10 * i), M(pv));
            vec3_set_length(V(nrm), V(nrm), u2f(0x3dcccccdu));
            const float d = vec3_dot(V(nrm), V(view));
            vec3_scale(V(refl), V(nrm), fpu_add(d, d));
            vec3_sub(V(refl), V(view), V(refl));
            vec3_set_length(V(refl), V(refl), 1.0f);
            const float z1 = fpu_add(lwc1(refl + 8), 1.0f);
            swc1(refl + 8, z1);
            const float r = sqrt_f(fpu_add(z1, z1));
            const float k2 = fpu_add(r, r);
            float u, v;
            if (lbu(m + 0x20) == 1 || lw(pv + 0x40) == 0) {
                u = fpu_add(fpu_div(lwc1(refl), k2), 0.5f);
                v = fpu_add(fpu_div(lwc1(refl + 4), k2), 0.5f);
            } else {
                const float su = fpu_add(fpu_div(lwc1(refl), k2), 0.5f);
                u = fpu_add(su, fpu_mul(fpu_sub(lwc1(KEPT + 8 * i), su), ease));
                const float sv = fpu_add(fpu_div(lwc1(refl + 4), k2), 0.5f);
                v = fpu_add(sv, fpu_mul(fpu_sub(lwc1(KEPT + 8 * i + 4), sv), ease));
            }
            swc1(UV + 8 * i, u);
            swc1(UV + 8 * i + 4, v);
        }
        if (lbu(m + 0x20) == 1) sb(m + 0x20, 2);
    } else {
        if (lbu(m + 0xbc) == 1) {  // the camera just left: the uvs kept
            sb(m + 0xbc, 0);
            for (uint32_t i = 0; i < 0x66; ++i) {
                swc1(KEPT + 8 * i, lwc1(UV + 8 * i));
                swc1(KEPT + 8 * i + 4, lwc1(UV + 8 * i + 4));
                mat4_transform(V(OUTV + 0x10 * i), V(VTX + 0x10 * i), M(pv));
            }
        } else {
            for (uint32_t i = 0; i < 0x66; ++i) mat4_transform(V(OUTV + 0x10 * i), V(VTX + 0x10 * i), M(pv));
        }
        sw(pv + 0x40, static_cast<uint32_t>(frame_scale_i(0x3c)));
    }
    for (uint32_t q = 0; q < 0x4a; ++q) {
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t idx = static_cast<uint32_t>(static_cast<int32_t>(lh(QUADS + q * 0x18 + 6 * k)));
            lq_sq(sp + 0x10 * k, OUTV + (idx << 4));
            swc1(sp + 0x50 + 8 * k, lwc1(UV + (idx << 3)));
            swc1(sp + 0x54 + 8 * k, lwc1(UV + 4 + (idx << 3)));
        }
        quad_draw(sp, 0);
    }
    ee_stack_free(0xc0);
}

namespace {
// orig: (helper) repeated in 0x002d73e8 — a timer halfword in EE memory as the timer helpers take it
int16_t& h16ref(uint32_t a) { return ee_ref<int16_t>(a); }
}  // namespace

// The frame as the original lays it out: +0 the vent's position, +0x10 the z axis, +0x20.. the
// camera-facing basis (d, right, up, position), +0x60.. the reversed one, +0xa0 the descriptor.
// orig: 0x002d73e8
void smoke_wisps_draw(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x130);
    const uint32_t q = sp + 0xa0;
    const uint32_t alpha24 = static_cast<uint32_t>(lbu(pv + 0x5c)) << 24;
    const uint32_t rgb = (static_cast<uint32_t>(lbu(pv + 0x5f)) << 16) | (static_cast<uint32_t>(lbu(pv + 0x5e)) << 8) | lbu(pv + 0x5d);
    const uint32_t vent = lwu(OV(0x001600ec)) + (lwu(pv + 4) << 7);
    const int32_t end = lh(pv) + lh(pv + 2) + lh(pv + 0x40);
    lq_sq(sp, m + 0x10);
    sd(sp + 0x10, 0);
    sd(sp + 0x18, 0);
    swc1(sp + 0xc, 1.0f);
    swc1(sp + 0x18, 1.0f);
    vec3_sub(V(sp + 0x20), V(OV(0x00166dc0)), V(sp));
    vec3_set_length(V(sp + 0x20), V(sp + 0x20), 1.0f);
    vec3_cross_ba(V(sp + 0x30), V(sp + 0x20), V(sp + 0x10));
    vec3_set_length(V(sp + 0x30), V(sp + 0x30), 1.0f);
    vec3_cross_ba(V(sp + 0x40), V(sp + 0x30), V(sp + 0x20));
    lq_sq(sp + 0x50, sp);
    vec3_scale(V(sp + 0x60), V(sp + 0x20), -1.0f);
    vec3_cross_ba(V(sp + 0x70), V(sp + 0x60), V(sp + 0x10));
    vec3_set_length(V(sp + 0x70), V(sp + 0x70), 1.0f);
    vec3_cross_ba(V(sp + 0x80), V(sp + 0x70), V(sp + 0x60));
    lq_sq(sp + 0x90, sp + 0x50);
    const uint64_t tex0 = effect_tex0(lw(pv + 0x54) + 0x28);
    sd(q + 0x78, tex0);
    sd(q + 0x80, 0x0000ff9000000260ull);
    sd(q + 0x88, 0x8000000048ull);
    sd(q + 0x70, 0);
    sub_002a0ba8(0x4a, 0);
    sub_002a0ba8(0x47, 0x51001);
    // corner k of a quad: the vertex (x, y fy, z fz, 1) on the basis, its uv and colour
    auto corner = [q](uint32_t k, uint32_t vtx, float fy, float fz, uint32_t basis, float s, float t, uint32_t col) {
        const uint32_t c = q + 0x10 * k;
        swc1(c, lwc1(vtx));
        swc1(c + 4, fpu_mul(lwc1(vtx + 4), fy));
        swc1(c + 0xc, 1.0f);
        swc1(c + 8, fpu_mul(lwc1(vtx + 8), fz));
        swc1(q + 0x50 + 8 * k, s);
        swc1(q + 0x54 + 8 * k, t);
        sw(q + 0x40 + 4 * k, col);
        mat4_transform(V(c), V(c), M(basis));
    };
    for (int32_t i = lh(pv); i < end;) {
        const uint32_t u = static_cast<uint32_t>(i);
        const uint32_t hp = OV(0x001d31c0) + 4 * u;
        float h = fpu_sub(lwc1(hp), fpu_mul(lwc1(OV(0x001d4480) + 4 * u), lwc1(0x0015ed60)));
        const uint32_t basis = lbu(OV(0x001d2860) + u) == 0 ? sp + 0x20 : sp + 0x60;
        swc1(hp, h);
        if (h <= -8.0f) swc1(hp, fpu_add(h, 8.0f));
        mat4_transform(V(basis + 0x30), V(OV(0x001cb7e0) + (u << 4)), M(vent));
        const float fy = lwc1(OV(0x001d02e0) + 4 * u), fz = lwc1(OV(0x001d15a0) + 4 * u);
        if (lbu(OV(0x001d2d10) + u) != 0) {  // rising: two quads
            sd(q + 0x88, 0x8000000044ull);
            for (uint32_t j = 2; j < 4; ++j) {
                for (uint32_t k = 0; k < 4; ++k) {
                    const uint32_t idx = lwu(OV(0x001e1380) + 0x10 * j + 4 * k);
                    const uint32_t col = static_cast<uint32_t>(alpha24 * lbu(OV(0x001619a8) + idx)) | rgb;
                    corner(k, OV(0x001e12c0) + (idx << 4), fy, fz, basis, fpu_add(lwc1(OV(0x001e1444) + (idx << 3)), lwc1(hp)), lwc1(OV(0x001e1440) + (idx << 3)), col);
                }
                quad_draw(q, 0);
            }
            i = static_cast<int32_t>(u + 1);
            continue;
        }
        int32_t fade;
        const uint32_t t1 = OV(0x001d5740) + 2 * u, t2 = OV(0x001d60a0) + 2 * u;
        if (lh(t1) != 0) {
            timer_tick_i16(h16ref(t1));
            const float f = fpu_mul(int_to_float(lh(t1)), lwc1(OV(0x00161384)));
            fade = float_to_int(fpu_mul(fpu_sub(1.0f, f), lwc1(OV(0x0016199c))));
        } else if (lh(t2) != 0) {
            if (timer_tick_i16(h16ref(t2)) != 0) {
                wisp_init_rising(m, i);
                sb(OV(0x001d2860) + u, static_cast<uint32_t>(rand_mod(2)));
                fade = 0;
            } else {
                fade = float_to_int(fpu_mul(fpu_mul(int_to_float(lh(t2)), lwc1(OV(0x0016138c))), lwc1(OV(0x0016199c))));
            }
        } else {
            if (timer_tick_i16(h16ref(OV(0x001d6a00) + 2 * u)) != 0) sh(t2, lhu(OV(0x00161388)));
            fade = lw(OV(0x00161998));
        }
        sd(q + 0x88, 0x8000000048ull);
        const uint32_t a = static_cast<uint32_t>(fade) << 24;
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t col = static_cast<uint32_t>(a * lbu(OV(0x001619a0) + k)) | lwu(pv + 0x38);
            corner(k, OV(0x001e11a0) + 0x10 * k, fy, fz, basis, fpu_sub(lwc1(OV(0x001e11e0) + 8 * k), lwc1(hp)), lwc1(OV(0x001e11e4) + 8 * k), col);
        }
        quad_draw(q, 0);
        i = static_cast<int32_t>(u + 1);
    }
    // the base: six quads on the camera-facing basis at the vent, lowered by pvars +0x50
    const uint64_t tex1 = effect_tex0(lw(pv + 0x58) + 0x28);
    sd(q + 0x78, tex1);
    sd(q + 0x88, 0x8000000044ull);
    lq_sq(sp + 0x50, vent + 0x30);
    swc1(sp + 0x58, fpu_sub(lwc1(sp + 0x58), lwc1(pv + 0x50)));
    if (lh(pv + 0x42) == 0) {
        for (uint32_t j = 0; j < 6; ++j) {
            for (uint32_t k = 0; k < 4; ++k) {
                const uint32_t idx = lwu(OV(0x001e1380) + 0x10 * j + 4 * k);
                const uint32_t col = static_cast<uint32_t>(lbu(OV(0x001619a8) + idx) * 0xff000000u) | rgb;
                corner(k, OV(0x001e1200) + (idx << 4), lwc1(pv + 0x48), lwc1(pv + 0x4c), sp + 0x20, fpu_add(lwc1(OV(0x001e13e4) + (idx << 3)), lwc1(OV(0x00161374))),
                       lwc1(OV(0x001e13e0) + (idx << 3)), col);
            }
            quad_draw(q, 0);
        }
    }
    ee_stack_free(0x130);
}

// orig: (none)
void effect_quad_fns_register() {
    ovl_register_moby_fn(0x002acbf0, bomb_discs_draw);
    ovl_register_moby_fn(0x002a7780, wrench_trail_draw);
    ovl_register_moby_fn(0x002d19b0, mirror_mesh_draw);
    ovl_register_moby_fn(0x002d73e8, smoke_wisps_draw);
}

}  // namespace rac1
