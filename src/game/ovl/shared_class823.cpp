// Class 823, a swaying plume: one port of the copies Rilgar and Umbris carry (the same code relinked; compared with the
// relocated fields masked, and normalised). Their level-own data: the draw's words (the blend's four fields, two colours,
// the effect texture: below) and its three tables (the texture coordinates and the two quads' corners), and the draw
// callback the update queues (each level's own); the camera 0x166dc0 is the engine's (OV). Header shared_class823.h.
//
// Its pvars: +0x10 its sway (a quaternion about y), +0x40 its home yaw, +0x44 / +0x48 two phases, +0x4c / +0x50 their
// rates.
#include <game/ovl/shared_class823.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l5.h>
#include <game/ovl/variants_l7.h>

namespace rac1 {

namespace {
// the draw's words: the blend's fields +0..+0xc, the colours +0x10 / +0x14, the effect texture +0x18
uint32_t draw_words() { return on_level(RILGAR) ? L5_ADDR(0x00161cf4) : L7_ADDR(0x00161a98); }
// the texture coordinates (4 pairs), then the first quad's corners +0x20 and the second's +0x60
uint32_t draw_tables() { return on_level(RILGAR) ? L5_ADDR(0x001d67b0) : L7_ADDR(0x001dac20); }
// the level's own draw callback (this class's draw, at the level's address)
uint32_t draw_callback() { return on_level(RILGAR) ? L5_ADDR(0x0030c220) : L7_ADDR(0x003048b0); }

const float kSway = u2f(0x3f060a92u);  // 30 degrees
}  // namespace

// State 0: its home yaw kept, its phases at random and their rates (27..30 and 12..15 degrees a second, either way), its
// colour 0x80 grey, its sway node set (sub_0024f590, joint 0); state 1. State 1: its draw queued; it swings about its
// home yaw (30 degrees by the first phase) and sways about y (30 degrees and 30 more by the second phase).
// orig: L5:0x0030c0a8 L7:0x00304738
void class823_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        swc1(pv + 0x40, lwc1(m + 0x48));
        swc1(pv + 0x48, rand_angle());
        const float a = rand_angle();
        float dt = lwc1(0x0015ed6c);
        const float lo1 = fpu_mul(dt, u2f(0x3ef14639u)), hi1 = fpu_mul(dt, u2f(0x3f060a92u));
        swc1(pv + 0x44, a);
        swc1(pv + 0x50, rand_range_signed_f(lo1, hi1));
        dt = lwc1(0x0015ed6c);
        const float lo2 = fpu_mul(dt, u2f(0x3e567750u)), hi2 = fpu_mul(dt, u2f(0x3e860a92u));
        swc1(pv + 0x4c, rand_range_signed_f(lo2, hi2));
        moby_set_color_hi(m, 0x80, 0x80, 0x80);
        sub_0024f590(m, 0, pv);
        sb(m + 0x20, 1);
        return;
    }
    if (st != 1) return;
    sub_001f93e0(draw_callback(), m);
    const float p1 = angle_add(lwc1(pv + 0x44), lwc1(pv + 0x4c));
    swc1(pv + 0x44, p1);
    swc1(m + 0x48, angle_add(lwc1(pv + 0x40), fpu_mul(sin_vu(p1), kSway)));
    const float p2 = angle_add(lwc1(pv + 0x48), lwc1(pv + 0x50));
    swc1(pv + 0x48, p2);
    quat_from_axis_angle(V(pv + 0x10), 1, fpu_add(fpu_mul(sin_vu(p2), kSway), kSway));
}

// From its joint 0, turned by its yaw and its sway's angle about y, the frame turned to face the camera: ten pairs of
// quads (the level's two quads' corners, moved out (8 / 6 a tenth along x, either side) and up (10 / 12 each)), each corner
// coloured between the level's two colours by its tenth, with the level's effect texture and blend.
// orig: L5:0x0030c220 L7:0x003048b0
void class823_draw(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x1a0);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t qa = sp, qb = sp + 0x90;  // (the two quads' descriptors: corners +0, colours +0x40, (s, t) +0x50, GS +0x70)
    const uint32_t angles = sp + 0x120, joint = sp + 0x130, mat = sp + 0x140;
    moby_chain_point_v(m, 0, V(joint));
    vec_clear(V(angles));
    swc1(angles + 8, lwc1(m + 0x48));
    swc1(angles + 4, fpu_add(fpu_mul(sin_vu(lwc1(pv + 0x48)), kSway), kSway));
    euler_to_mat4_vu(M(mat), V(angles));
    const uint32_t ax = mat, ay = mat + 0x10, az = mat + 0x20;
    vec3_sub(V(ay), V(OV(0x00166dc0)), V(joint));
    vec3_set_length(V(ay), V(ay), 1.0f);
    vec3_cross_ba(V(ax), V(ay), V(az));
    vec3_cross_ba(V(ay), V(az), V(ax));
    lq_sq(mat + 0x30, joint);
    const uint32_t w = draw_words(), t = draw_tables();
    const uint64_t tex = effect_tex0(lw(w + 0x18));
    auto sx = [](uint32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v))); };
    const uint64_t blend = sx(lwu(w)) | (sx(lwu(w + 4)) << 2) | (sx(lwu(w + 8)) << 4) | (sx(lwu(w + 0xc)) << 6) | 0x8000000000ull;
    for (const uint32_t q : {qa, qb}) {
        sd(q + 0x78, tex);
        sd(q + 0x80, 0x0000ff9000000260ull);
        sd(q + 0x88, blend);
        sd(q + 0x70, 0);
    }
    for (uint32_t k = 0; k < 0x20; k += 8) {
        const float s = lwc1(t + k), tt = lwc1(t + k + 4);
        swc1(qa + 0x50 + k, s);
        swc1(qa + 0x54 + k, tt);
        swc1(qb + 0x50 + k, s);
        swc1(qb + 0x54 + k, tt);
    }
    for (int32_t i = 0; i < 10; ++i) {
        const float fi = static_cast<float>(i);
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t a = qa + k * 0x10, b = qb + k * 0x10;
            lq_sq(a, t + 0x20 + k * 0x10);
            lq_sq(b, t + 0x60 + k * 0x10);
            const float tenth = fpu_mul(static_cast<float>(i + static_cast<int32_t>(k & 1)), u2f(0x3dcccccdu));
            const float side = k < 2 ? 1.0f : -1.0f;
            const uint32_t colour = rgba_lerp(lwu(w + 0x10), lwu(w + 0x14), tenth);
            sw(qb + 0x40 + k * 4, colour);
            sw(qa + 0x40 + k * 4, colour);
            swc1(a + 8, fpu_add(lwc1(a + 8), fpu_mul(fi, 10.0f)));
            swc1(a, fpu_add(lwc1(a), fpu_mul(fpu_mul(tenth, 8.0f), side)));
            swc1(b, fpu_add(lwc1(b), fpu_mul(fpu_mul(tenth, 6.0f), side)));
            swc1(b + 8, fpu_add(lwc1(b + 8), fpu_mul(fi, 12.0f)));
        }
        quad_draw(qa, mat);
        quad_draw(qb, mat);
    }
    ee_stack_free(0x1a0);
}

}  // namespace rac1
