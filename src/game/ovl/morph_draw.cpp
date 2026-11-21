// Ported overlay code: the Morph-o-Ray's beam draw callback (0x2bdc50), helix ribbons (0x2be078),
// travelling rings (0x2be650), and muzzle glow (0x2be890).
#include <game/ovl/morph_draw.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>

namespace rac1 {

namespace {
constexpr uint32_t CAM_UP = 0x0013f5e0u;
constexpr uint32_t GOLD_MORPH_FLAG = 0x0013e535u;
}  // namespace

// orig: 0x002bdc50
void morph_2bdc50_update(uint32_t m) {
    const float next8c = fpu_sub(lwc1(OV(0x0016178cu)), fpu_mul(lwc1(OV(0x00161790u)), lwc1(0x0015ed60u)));
    swc1(OV(0x0016178cu), next8c);
    if (next8c <= -8.0f) {
        swc1(OV(0x0016178cu), fpu_add(next8c, 8.0f));
    }

    morph_draw_2be078(OV(0x001dc020u), lwu(OV(0x0016176cu)), lwu(OV(0x00161770u)), 0xcu, lwu(OV(0x00161784u)),
                      lwc1(OV(0x00161774u)), lwc1(OV(0x00161778u)), lwc1(OV(0x0016177cu)));
    morph_draw_2be078(OV(0x001dc0e0u), lwu(OV(0x0016176cu)), lwu(OV(0x00161770u)), 0xcu, lwu(OV(0x00161788u)),
                      lwc1(OV(0x00161774u)), lwc1(OV(0x00161778u)), lwc1(OV(0x0016177cu)));
    morph_draw_2be650();
    morph_draw_2be890(m);

    const uint32_t base_col = lbu(GOLD_MORPH_FLAG) != 0u ? lwu(OV(0x00161798u)) : lwu(OV(0x00161794u));
    const uint64_t tex0 = effect_tex0(lw(OV(0x0016179cu)));
    const float dt60 = lwc1(0x0015ed60u);
    const float step18 = fpu_mul(lwc1(OV(0x0016171cu)), dt60);
    float f22 = lwc1(OV(0x00161718u));
    const float next18 = fpu_sub(f22, step18);

    const uint32_t desc = ee_stack_alloc(0x90);
    sd(desc + 0x78u, tex0);
    sd(desc + 0x80u, 0x0000ff9000000260ull);
    sd(desc + 0x88u, 0x0000008000000048ull);
    sw(desc + 0x40u, base_col);
    sd(desc + 0x70u, 0ull);
    sw(desc + 0x4cu, base_col);
    sw(desc + 0x48u, base_col);
    sw(desc + 0x44u, base_col);

    swc1(OV(0x00161718u), next18);
    if (next18 <= -8.0f) {
        swc1(OV(0x00161718u), fpu_add(next18, 8.0f));
    }

    const float step28 = fpu_mul(lwc1(OV(0x00161728u)), dt60);
    const float next20 = fpu_sub(lwc1(OV(0x00161720u)), step28);
    swc1(OV(0x00161720u), next20);
    if (next20 <= -7.0f) {
        swc1(OV(0x00161720u), fpu_add(next20, 7.0f));
    }

    const float next24 = fpu_add(lwc1(OV(0x00161724u)), step28);
    swc1(OV(0x00161724u), next24);
    if (5.0f <= next24) {
        swc1(OV(0x00161724u), fpu_sub(next24, 5.0f));
    }

    for (int32_t ring = 2; ring < 12; ++ring) {
        const uint32_t ring_off = static_cast<uint32_t>(ring) * 0x140u;
        float f21 = lwc1(OV(0x00161720u));
        float f20 = lwc1(OV(0x00161724u));
        for (int32_t i = 0; i < 20; ++i) {
            const uint32_t cur_off = ring_off + (static_cast<uint32_t>(i) << 4);
            const int32_t next_i = ee_rem(i + 1, 20);
            const uint32_t next_off = ring_off + (static_cast<uint32_t>(next_i) << 4);

            V(desc + 0x00u) = V(OV(0x001dc1a0u) + cur_off);
            V(desc + 0x10u) = V(OV(0x001dc1a0u) + next_off);
            V(desc + 0x20u) = V(OV(0x001dc060u) + cur_off);
            V(desc + 0x30u) = V(OV(0x001dc060u) + next_off);

            Vec4 e1{}, e2{}, nrm{}, view{};
            vec3_sub(e1, V(desc + 0x00u), V(desc + 0x10u));
            vec3_sub(e2, V(desc + 0x20u), V(desc + 0x10u));
            vec3_cross_ba(nrm, e2, e1);
            vec3_sub(view, V(desc + 0x10u), V(OV(0x00166dc0u)));
            const float dot = vec3_dot(nrm, view);
            const float step14 = lwc1(OV(0x00161714u));

            if (0.0f <= dot) {
                f20 = fpu_add(f20, step14);
                f21 = fpu_add(f21, step14);
            } else {
                if (ring == 11) {
                    const uint32_t fade_col = lwu(OV(0x00161794u)) & 0x00ffffffu;
                    sw(desc + 0x40u, fade_col);
                    sw(desc + 0x44u, fade_col);
                }
                {
                    const float next_v = fpu_add(f21, lwc1(OV(0x00161714u)));
                    const float prev_u = fpu_sub(f22, lwc1(OV(0x00161710u)));
                    swc1(desc + 0x64u, f21);
                    swc1(desc + 0x54u, f21);
                    swc1(desc + 0x50u, f22);
                    f21 = next_v;
                    swc1(desc + 0x5cu, next_v);
                    swc1(desc + 0x68u, prev_u);
                    swc1(desc + 0x60u, prev_u);
                    swc1(desc + 0x6cu, next_v);
                    swc1(desc + 0x58u, f22);
                    quad_draw(desc, 0);
                }
                {
                    const float next_v = fpu_add(f20, lwc1(OV(0x00161714u)));
                    const float prev_u = fpu_sub(f22, lwc1(OV(0x00161710u)));
                    swc1(desc + 0x64u, f20);
                    swc1(desc + 0x54u, f20);
                    swc1(desc + 0x50u, f22);
                    f20 = next_v;
                    swc1(desc + 0x58u, f22);
                    swc1(desc + 0x68u, prev_u);
                    swc1(desc + 0x5cu, next_v);
                    swc1(desc + 0x60u, prev_u);
                    swc1(desc + 0x6cu, next_v);
                    quad_draw(desc, 0);
                }
            }
        }
        f22 = fpu_add(f22, lwc1(OV(0x00161710u)));
    }

    ee_stack_free(0x90);
}

// orig: 0x002be078
void morph_draw_2be078(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, float pf12, float pf13, float pf14) {
    (void)pf12;
    const uint32_t desc = ee_stack_alloc(0x120);
    const uint32_t desc2 = desc + 0x90u;
    const uint32_t cam = OV(0x00166dc0u);

    const uint64_t tex_a = effect_tex0(lw(OV(0x001617a4u)));
    const uint32_t col_a = lwu(OV(0x00161780u)) | (p1 << 24);
    sd(desc + 0x88u, 0x0000008000000048ull);
    sd(desc + 0x80u, 0x0000ff9000000260ull);
    sd(desc + 0x78u, tex_a);
    sd(desc + 0x70u, 0ull);
    sw(desc + 0x4cu, col_a);
    sw(desc + 0x48u, col_a);
    sw(desc + 0x44u, col_a);
    sw(desc + 0x40u, col_a);

    const uint64_t tex_b = effect_tex0(lw(OV(0x001617a8u)));
    const uint32_t col_b = p4 | (p2 << 24);
    const float u_base = lwc1(OV(0x0016178cu));
    const float u1 = fpu_add(u_base, 1.0f);
    const float u0 = fpu_add(u_base, 0.0f);

    sd(desc2 + 0x78u, tex_b);
    sd(desc2 + 0x88u, 0x0000008000000048ull);
    sd(desc2 + 0x80u, 0x0000ff9000000260ull);
    sd(desc2 + 0x70u, 0ull);
    sw(desc2 + 0x4cu, col_b);
    sw(desc2 + 0x48u, col_b);
    sw(desc2 + 0x44u, col_b);
    sw(desc2 + 0x40u, col_b);
    swc1(desc2 + 0x50u, 0.0f);
    swc1(desc2 + 0x54u, 0.0f);
    swc1(desc2 + 0x58u, 0.0f);
    swc1(desc2 + 0x5cu, 1.0f);
    swc1(desc2 + 0x60u, 1.0f);
    swc1(desc2 + 0x64u, 0.0f);
    swc1(desc2 + 0x68u, 1.0f);
    swc1(desc2 + 0x6cu, 1.0f);

    swc1(desc + 0x50u, u0);
    swc1(desc + 0x54u, 0.0f);
    swc1(desc + 0x58u, u0);
    swc1(desc + 0x5cu, 1.0f);
    swc1(desc + 0x60u, u1);
    swc1(desc + 0x64u, 0.0f);
    swc1(desc + 0x68u, u1);
    swc1(desc + 0x6cu, 1.0f);

    Vec4 view{}, seg{}, side{};
    vec3_sub(view, V(p0), V(cam));
    vec3_sub(seg, V(p0 + 0x10u), V(p0));
    vec3_cross_ba(side, seg, view);
    float side_len = vec3_length(side);
    if (side_len != 0.0f) {
        side_len = fpu_div(1.0f, side_len);
    }
    vec3_scale(side, side, fpu_mul(lwc1(OV(0x00161774u)), side_len));
    vec3_add(V(desc + 0x00u), V(p0), side);
    vec3_sub(V(desc + 0x10u), V(p0), side);

    const int32_t count = static_cast<int32_t>(p3);
    const int32_t last = count - 1;
    if (1 < last) {
        uint32_t cur = p0 + 0x10u;
        uint32_t nxt = p0 + 0x20u;
        for (int32_t s5 = 1; s5 < last; ++s5, cur += 0x10u, nxt += 0x10u) {
            vec3_sub(view, V(cur), V(cam));
            vec3_sub(seg, V(nxt), V(cur));
            vec3_cross_ba(side, seg, view);
            float sl = vec3_length(side);
            const float w1 = lwc1(OV(0x00161774u));
            if (sl != 0.0f) {
                sl = fpu_div(1.0f, sl);
            }
            vec3_scale(side, side, fpu_mul(w1, sl));
            vec3_add(V(desc + 0x20u), V(cur), side);
            vec3_sub(V(desc + 0x30u), V(cur), side);

            if (s5 == 1) {
                const uint32_t plain = lwu(OV(0x00161780u));
                sw(desc + 0x40u, plain);
                sw(desc + 0x44u, plain);
            } else if (s5 == count - 2) {
                const uint32_t plain = lwu(OV(0x00161780u));
                sw(desc + 0x48u, plain);
                sw(desc + 0x4cu, plain);
            } else if (s5 == 2) {
                sw(desc + 0x44u, col_a);
                sw(desc + 0x40u, col_a);
            }
            quad_draw(desc, 0);
            V(desc + 0x00u) = V(desc + 0x20u);
            V(desc + 0x10u) = V(desc + 0x30u);
        }
    }

    if (last > 0) {
        uint32_t pk0 = p0;
        uint32_t pk1 = p0 + 0x10u;
        for (int32_t s5 = 0; s5 < last; ++s5, pk0 += 0x10u, pk1 += 0x10u) {
            Vec4 v0{}, v1{};
            vec3_sub(v0, V(pk0), V(cam));
            vec3_sub(v1, V(pk1), V(cam));
            const float l0 = vec3_length(v0);
            const float l1 = vec3_length(v1);
            if (l1 < l0) {
                if (l1 == 0.0f) {
                    v1 = v0;
                } else {
                    vec3_scale(v1, v1, fpu_div(l0, l1));
                }
            } else {
                if (l0 == 0.0f) {
                    v0 = v1;
                } else {
                    vec3_scale(v0, v0, fpu_div(l1, l0));
                }
            }

            Vec4 q0{}, q1{}, push{}, to_cam{}, wide{};
            vec3_add(q0, V(cam), v0);
            vec3_add(q1, V(cam), v1);
            vec3_sub(seg, q1, q0);
            vec3_scale(push, seg, pf14);
            vec3_sub(q0, q0, push);
            vec3_add(q1, q1, push);
            vec3_sub(to_cam, V(cam), q0);
            vec3_cross_ba(side, to_cam, seg);
            float sl = vec3_length(side);
            float scale_w2;
            if (sl == 0.0f) {
                scale_w2 = fpu_mul(pf13, sl);
            } else {
                sl = fpu_div(1.0f, sl);
                scale_w2 = fpu_mul(pf13, sl);
            }
            vec3_scale(wide, side, scale_w2);
            vec3_add(V(desc2 + 0x00u), q0, wide);
            vec3_sub(V(desc2 + 0x10u), q0, wide);
            vec3_add(V(desc2 + 0x20u), q1, wide);
            vec3_sub(V(desc2 + 0x30u), q1, wide);

            if (s5 == 0) {
                sw(desc2 + 0x40u, p4);
                sw(desc2 + 0x44u, p4);
            } else if (s5 == count - 2) {
                sw(desc2 + 0x48u, p4);
                sw(desc2 + 0x4cu, p4);
            } else if (s5 == 1) {
                sw(desc2 + 0x44u, col_b);
                sw(desc2 + 0x40u, col_b);
            }
            quad_draw(desc2, 0);
        }
    }

    sub_002a0ba8(0x47u, 0x5360bull);
    ee_stack_free(0x120);
}

// orig: 0x002be650
void morph_draw_2be650() {
    const uint32_t desc = ee_stack_alloc(0x90);
    const uint64_t tex0 = effect_tex0(8);
    const uint32_t gold = lbu(GOLD_MORPH_FLAG);

    sd(desc + 0x78u, tex0);
    sd(desc + 0x88u, 0x0000008000000048ull);
    sd(desc + 0x80u, 0x0000ff9000000260ull);
    swc1(desc + 0x6cu, 1.0f);
    sd(desc + 0x70u, 0ull);
    sw(desc + 0x50u, 0u);
    sw(desc + 0x54u, 0u);
    sw(desc + 0x58u, 0u);
    swc1(desc + 0x5cu, 1.0f);
    swc1(desc + 0x60u, 1.0f);
    sw(desc + 0x64u, 0u);
    swc1(desc + 0x68u, 1.0f);

    const uint32_t rgb = (gold != 0u ? lwu(OV(0x00161764u)) : lwu(OV(0x00161760u))) & 0x00ffffffu;
    for (uint32_t i = 0; i < 3; ++i) {
        if (lw(OV(0x00161818u) + i * 4u) == 0) continue;

        const uint32_t alpha = static_cast<uint32_t>(float_to_int(lwc1(OV(0x001617e8u) + i * 4u)));
        const uint32_t col = rgb | (alpha << 24);
        sw(desc + 0x40u, col);
        sw(desc + 0x4cu, col);
        sw(desc + 0x48u, col);
        sw(desc + 0x44u, col);

        Mat4 basis{};
        basis.r[3] = V(OV(0x001dd0a0u) + i * 0x10u);
        basis.r[0] = V(OV(0x001dd0d0u) + i * 0x10u);
        vec3_scale(basis.r[2], V(CAM_UP), -1.0f);
        vec3_cross_ba(basis.r[1], basis.r[0], basis.r[2]);
        vec3_set_length(basis.r[1], basis.r[1], 1.0f);
        vec3_cross_ba(basis.r[2], basis.r[1], basis.r[0]);

        const uint32_t sz_addr = OV(0x00161808u) + i * 4u;
        for (uint32_t k = 0; k < 4; ++k) {
            Vec4 scaled{};
            vec3_scale(scaled, V(OV(0x001dbf20u) + k * 0x10u), lwc1(sz_addr));
            mat4_transform(V(desc + k * 0x10u), scaled, basis);
        }
        quad_draw(desc, 0);
    }

    ee_stack_free(0x90);
}

// orig: 0x002be890
void morph_draw_2be890(uint32_t p0) {
    const uint32_t desc = ee_stack_alloc(0x90);
    const uint64_t tex0 = effect_tex0(0xb);
    const uint32_t col0 = lwu(OV(0x001617b8u));

    sd(desc + 0x80u, 0x0000ff9000000260ull);
    sd(desc + 0x78u, tex0);
    sw(desc + 0x40u, col0);
    sw(desc + 0x4cu, col0);
    sw(desc + 0x48u, col0);
    sw(desc + 0x44u, col0);
    sd(desc + 0x88u, 0x0000008000000048ull);
    sd(desc + 0x70u, 0ull);
    sw(desc + 0x50u, 0u);
    sw(desc + 0x54u, 0u);
    sw(desc + 0x58u, 0u);
    swc1(desc + 0x5cu, 1.0f);
    swc1(desc + 0x60u, 1.0f);
    sw(desc + 0x64u, 0u);
    swc1(desc + 0x68u, 1.0f);
    swc1(desc + 0x6cu, 1.0f);

    Vec4 off{};
    vec3_set_length(off, V(p0 + 0xd0u), lwc1(OV(0x001617b4u)));

    Mat4 basis{};
    vec3_add(basis.r[3], V(OV(0x001dbf60u)), off);
    vec3_sub(off, V(OV(0x00166dc0u)), basis.r[3]);
    vec3_set_length(basis.r[0], off, 1.0f);
    vec3_cross_ba(basis.r[1], basis.r[0], V(CAM_UP));
    vec3_set_length(basis.r[1], basis.r[1], -1.0f);
    vec3_cross_ba(basis.r[2], basis.r[1], basis.r[0]);

    for (uint32_t k = 0; k < 4; ++k) {
        Vec4 scaled{};
        vec3_scale(scaled, V(OV(0x001dbf20u) + k * 0x10u), lwc1(OV(0x001617acu)));
        mat4_transform(V(desc + k * 0x10u), scaled, basis);
    }
    quad_draw(desc, 0);

    sw(desc + 0x40u, 0xffffffffu);
    sw(desc + 0x4cu, 0xffffffffu);
    sw(desc + 0x48u, 0xffffffffu);
    sw(desc + 0x44u, 0xffffffffu);
    for (uint32_t k = 0; k < 4; ++k) {
        Vec4 scaled{};
        vec3_scale(scaled, V(OV(0x001dbf20u) + k * 0x10u), lwc1(OV(0x001617b0u)));
        mat4_transform(V(desc + k * 0x10u), scaled, basis);
    }
    quad_draw(desc, 0);

    ee_stack_free(0x90);
}

// orig: (none)
void morph_draw_fns_register() {
    ovl_register_moby_fn(0x002bdc50, morph_2bdc50_update);
}

}  // namespace rac1
