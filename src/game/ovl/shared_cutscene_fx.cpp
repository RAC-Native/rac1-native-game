// The arrival scene's sparks and glow on Oltanis: one port of Veldin's copies (which cutscene_level_extras runs for the
// scene on level 14) and Oltanis's own: the same code relinked (compared with the relocated fields masked, and
// normalised). Their callee particle 0x35's spawn is each level's copy of Veldin's 0x272060 (the overlay maps pair them),
// here the Veldin port; their one word is the engine's (OV: the glow's height 0x15fb08). Header shared_cutscene_fx.h.
#include <game/ovl/shared_cutscene_fx.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>

namespace rac1 {

// Four times: a size 0.15..0.5 and a spin (1 or -1); a velocity of 0.05..0.2 (frame-scaled) in a random direction 20..88
// degrees up; the point (the joint's, moved by up to 0.15 each way, the moves adding up) gets a warm spark (0x7f7f2020,
// size a tenth, a second) and a white one (0x7f7f7f7f, additive, 0.07 / 0.7 of the size) spinning the other way.
// orig: L0:0x0023e008 L14:0x00245e98
void cutscene_l14_effect(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t vel = sp, kept = sp + 0x10, at = sp + 0x20;
    moby_chain_point_v(m, 0, V(at));
    for (int k = 0; k < 4; ++k) {
        const float size = rand_range_f(u2f(0x3e19999au), 0.5f);
        int32_t spin = rand_mod(2);
        spin = spin != 0 ? spin : spin - 1;
        const float yaw = rand_angle();
        const float pitch = rand_range_f(u2f(0x3eb2b8c2u), u2f(0x3fc49809u));
        const float speed = rand_range_f(u2f(0x3d4ccccdu), u2f(0x3e4ccccdu));
        vec_from_spherical_v(V(vel), fpu_mul(speed, lwc1(0x0015ed60)), yaw, pitch);
        lq_sq(kept, at);  // (a copy nothing reads)
        for (uint32_t c = 0; c < 0xc; c += 4)
            swc1(at + c, fpu_add(lwc1(at + c), rand_range_signed_f(0.0f, u2f(0x3e19999au))));
        uint32_t p = spawn_particle_35(at, static_cast<uint32_t>(frame_scale_i(0x3c)), 0x7f7f2020u, 0, static_cast<uint32_t>(spin), vel,
                                       fpu_mul(size, u2f(0x3dcccccdu)), size, u2f(0x3c1374bcu));
        if (p != 0) sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x70));
        spin = -spin;
        p = spawn_particle_35(at, static_cast<uint32_t>(frame_scale_i(0x3c)), 0x7f7f7f7fu, 1, static_cast<uint32_t>(spin), vel,
                              fpu_mul(size, u2f(0x3d8f5c29u)), fpu_mul(size, u2f(0x3f333333u)), u2f(0x3c1374bcu));
        if (p != 0) sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x70));
    }
    ee_stack_free(0xc0);
}

namespace {
// a corner's (y, z) moved by (dy, dz)
void nudge(uint32_t p, float dy, float dz) {
    swc1(p + 4, fpu_add(lwc1(p + 4), dy));
    swc1(p + 8, fpu_add(lwc1(p + 8), dz));
}
}  // namespace

// The joint's matrix (moby_chain_world_one), its x axis turned 20 degrees down about its y axis, its y axis made square
// to x and z (unit length) and z square to both; the descriptor's colours (0x3ff08080 at the first two corners,
// 0x3ff08030 at the others), its texture coordinates (0, 0) .. (1, 1), effect texture 14, blended. In the joint's frame:
// a quad along x (a quarter wide), the same turned a quarter, a pair out to twice the x axis, and a pair out to three
// times along x at the glow's height (0x15fb08).
// orig: L0:0x0023db30 L14:0x002459c0
void cutscene_l14_effect_draw(uint32_t m) {
    const uint64_t tex = effect_tex0(14);
    const uint32_t sp = ee_stack_alloc(0x120);
    const uint32_t d = sp;  // (the quad's descriptor: corners +0..+0x30, colours +0x40, (s, t) +0x50, GS registers +0x70)
    const uint32_t mat = sp + 0x90, ax = mat, ay = mat + 0x10, az = mat + 0x20;
    const uint32_t tip = sp + 0xd0, end = sp + 0xe0, x1 = sp + 0xf0, x2 = sp + 0x100;
    const float one = 1.0f, four = 4.0f, q = 0.25f;
    sw(d + 0x44, 0x3ff08080u);
    sw(d + 0x4c, 0x3ff08030u);
    sd(d + 0x78, tex);
    sd(d + 0x80, 0x0000ff9000000260ull);
    sd(d + 0x70, 5);
    sd(d + 0x88, 0x8000000048ull);
    sw(d + 0x40, 0x3ff08080u);
    sw(d + 0x48, 0x3ff08030u);
    sw(d + 0x50, 0);
    sw(d + 0x54, 0);
    swc1(d + 0x58, one);
    sw(d + 0x5c, 0);
    sw(d + 0x60, 0);
    swc1(d + 0x64, one);
    swc1(d + 0x68, one);
    swc1(d + 0x6c, one);
    moby_chain_world_one(m, 0, mat);
    vec_rotate_axis(V(ax), V(ax), V(ay), u2f(0xbeb2b8c2u));
    vec3_cross_ba(V(ay), V(ax), V(az));
    vec3_set_length(V(ay), V(ay), one);
    vec3_cross_ba(V(az), V(ay), V(ax));
    swc1(mat + 0x3c, one);
    vec3_scale(V(tip), V(ax), four);
    swc1(tip + 0xc, one);
    vec3_add(V(end), V(tip), V(mat + 0x30));  // (unused below)
    lq_sq(x1, ax);
    vec3_add(V(x2), V(x1), V(ax));
    swc1(x1 + 0xc, q);
    swc1(x2 + 0xc, 0.5f);

    // the first pair: from the joint along x, a quarter wide, then turned a quarter about x
    vec_clear(V(d));
    sw(d + 8, 0);
    swc1(d + 0xc, one);
    lq_sq(d + 0x10, x1);
    {
        const float z = lwc1(d + 0x18);
        const float x = fpu_mul(lwc1(x1 + 0xc), four);
        swc1(d + 0x1c, one);
        swc1(d + 0x18, fpu_add(z, -q));
        swc1(d + 0x10, x);
    }
    vec_clear(V(d + 0x20));
    sw(d + 0x28, 0);
    swc1(d + 0x2c, one);
    lq_sq(d + 0x30, x1);
    {
        const float z = fpu_add(lwc1(d + 0x38), q);
        swc1(d + 0x3c, one);
        swc1(d + 0x38, z);
        swc1(d + 0x30, fpu_mul(lwc1(x1 + 0xc), four));
    }
    quad_draw(d, mat);
    nudge(d + 0x10, -q, q);
    nudge(d + 0x30, q, -q);
    quad_draw(d, mat);

    // the second pair: from x out to twice x
    {
        lq_sq(d, x1);
        const float z0 = lwc1(d + 8);
        const float near = fpu_mul(four, lwc1(x1 + 0xc));
        swc1(d + 0xc, one);
        swc1(d, near);
        swc1(d + 8, fpu_add(z0, -q));
        lq_sq(d + 0x10, x2);
        const float z1 = fpu_add(lwc1(d + 0x18), -q);
        const float far = fpu_mul(four, lwc1(x2 + 0xc));
        swc1(d + 0x1c, one);
        swc1(d + 0x18, z1);
        swc1(d + 0x10, far);
        lq_sq(d + 0x20, x1);
        swc1(d + 0x20, near);
        swc1(d + 0x28, fpu_add(lwc1(d + 0x28), q));
        swc1(d + 0x2c, one);
        lq_sq(d + 0x30, x2);
        swc1(d + 0x30, far);
        swc1(d + 0x3c, one);
        swc1(d + 0x38, fpu_add(lwc1(d + 0x38), q));
        quad_draw(d, mat);
        nudge(d, -q, q);
        nudge(d + 0x10, -q, q);
        nudge(d + 0x20, q, -q);
        nudge(d + 0x30, q, -q);
        quad_draw(d, mat);
    }

    // the third pair: from twice x out to three times along x, at the glow's height
    {
        lq_sq(d, x2);
        const float z0 = lwc1(d + 8);
        const float near = fpu_mul(four, lwc1(x2 + 0xc));
        swc1(d + 0xc, one);
        swc1(d, near);
        swc1(d + 8, fpu_add(z0, -q));
        vec_clear(V(d + 0x10));
        const float y = lwc1(d + 0x18);
        const float far = fpu_mul(fpu_mul(four, lwc1(x1 + 0xc)), 3.0f);
        swc1(d + 0x1c, one);
        swc1(d + 0x18, fpu_add(y, lwc1(OV(0x0015fb08))));
        swc1(d + 0x10, far);
        lq_sq(d + 0x20, x2);
        const float z2 = fpu_add(lwc1(d + 0x28), q);
        swc1(d + 0x2c, one);
        swc1(d + 0x20, fpu_mul(four, lwc1(x2 + 0xc)));
        swc1(d + 0x28, z2);
        vec_clear(V(d + 0x30));
        swc1(d + 0x3c, one);
        sw(d + 0x38, 0);
        swc1(d + 0x30, fpu_mul(fpu_mul(four, lwc1(x1 + 0xc)), 3.0f));
        quad_draw(d, mat);
        nudge(d, -q, q);
        nudge(d + 0x20, q, -q);
        quad_draw(d, mat);
    }
    ee_stack_free(0x120);
}

}  // namespace rac1
