// Class 1511's draw callback: one port of the copies Rilgar and Oltanis carry (the same code relinked; compared with the
// relocated fields masked, and normalised). Its level-own data: two distances (below) and the quad's four corners; the
// camera 0x166dc0 is the engine's (OV). Header shared_class1511.h.
//
// The moby's pvars: its alpha +0x60 (halved at random while +0x67 is set), its colour +0x68, its size +0x70; the colours
// +0x64 .. +0x66 (the moby's) and +0x6c .. +0x6e (the glow's), the glow's full alpha +0x6f.
#include <game/ovl/shared_class1511.h>

#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l5.h>

namespace rac1 {

namespace {
// the glow's distance along the moby's y axis (+0) and toward the camera (+4); the update's pieces' push up (+8) and on (+0xc)
uint32_t glow_words() { return on_level(RILGAR) ? L5_ADDR(0x00161fc8) : L14_ADDR(0x0016229c); }
// the quad's four corners
uint32_t glow_corners() { return on_level(RILGAR) ? L5_ADDR(0x00215c70) : L14_ADDR(0x001f6200); }
}  // namespace

// The glow's centre (along the moby's y axis, then toward the camera) and a frame facing the camera (up 0, 0, 1); its
// corners scaled by the size and placed in that frame; effect texture 11, blended; its colour with the alpha in the top
// byte (while +0x67 is set, the alpha is halved one frame in eight first).
// orig: L5:0x0031cf58 L14:0x00308150
void class1511_draw(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x100);
    const uint32_t d = sp, up = sp + 0x90, mat = sp + 0xa0, ax = mat, ay = mat + 0x10, az = mat + 0x20, centre = mat + 0x30;
    const uint32_t along = sp + 0xe0, toward = sp + 0xf0;
    V(up) = Vec4{};
    swc1(up + 8, 1.0f);
    swc1(up + 0xc, 1.0f);
    const uint32_t pv = lwu(m + 0x78);
    const uint64_t tex = effect_tex0(0xb);
    const uint32_t w = glow_words();
    sd(d + 0x80, 0x0000ff9000000260ull);
    sd(d + 0x78, tex);
    sd(d + 0x70, 5);
    sd(d + 0x88, 0x8000000048ull);
    sw(d + 0x50, 0);
    sw(d + 0x54, 0);
    sw(d + 0x58, 0);
    swc1(d + 0x5c, 1.0f);
    swc1(d + 0x60, 1.0f);
    sw(d + 0x64, 0);
    swc1(d + 0x68, 1.0f);
    swc1(d + 0x6c, 1.0f);
    vec3_scale(V(along), V(m + 0xe0), lwc1(w));
    vec3_add(V(centre), V(m + 0x10), V(along));
    vec3_sub(V(ax), V(OV(0x00166dc0)), V(centre));
    vec3_set_length(V(ax), V(ax), 1.0f);
    vec3_cross_ba(V(ay), V(ax), V(up));
    vec3_set_length(V(ay), V(ay), 1.0f);
    vec3_cross_ba(V(az), V(ay), V(ax));
    vec3_scale(V(toward), V(ax), lwc1(w + 4));
    vec3_add(V(centre), V(centre), V(toward));
    if (lbu(pv + 0x67) != 0 && rand_mod(8) == 0) sw(pv + 0x60, static_cast<uint32_t>(lw(pv + 0x60) >> 1));
    const uint32_t colour = (lwu(pv + 0x60) << 24) | lwu(pv + 0x68);
    sw(d + 0x40, colour);
    sw(d + 0x4c, colour);
    sw(d + 0x48, colour);
    sw(d + 0x44, colour);
    const uint32_t corners = glow_corners();
    for (uint32_t k = 0; k < 0x40; k += 0x10) {
        vec3_scale(V(d + k), V(corners + k), lwc1(pv + 0x70));
        mat4_transform(V(d + k), V(d + k), M(mat));
    }
    quad_draw(d, 0);
    ee_stack_free(0x100);
}


// orig: L5:0x0031d160 L14:0x00308358
uint32_t class1511_remains(uint32_t m) {
    const uint32_t c = moby_spawn(0x5ea);  // (1514)
    if (c == 0) return 0;
    sb(c + 0x30, lbu(m + 0x30));
    sb(c + 0x31, 1);
    sh(c + 0x32, lhu(m + 0x32));
    swc1(c + 0x40, lwc1(m + 0x40));
    swc1(c + 0x44, lwc1(m + 0x44));
    swc1(c + 0x48, lwc1(m + 0x48));
    sd(c + 0x38, ld(m + 0x38));
    lq_sq(c + 0x10, m + 0x10);
    moby_update_transform(c);
    return c;
}

// State 0 sets its colours up (then 1). Each frame: the glow fades from 32 to 48 from the camera (gone beyond), drawn
// (class1511_draw); a hit of mask 0x810000 breaks it when it is a blast, or lands within 0.8 of the glow's point, or comes
// from the wrench (class 0x47) within 0.8 of it: its sound, its remains, a piece of class 1517 and two of 1518 pushed
// apart, the bolts, 24 sparks and smoke puffs (particles 0x35 and 0x17) about the glow, then it goes.
// orig: L5:0x0031c8e0 L14:0x00307ad8
void class1511_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) {
        sb(m + 0x20, 1);
        sw(m + 0x90, 0x7f000000u | (lbu(pv + 0x66) << 16) | (lbu(pv + 0x65) << 8) | lbu(pv + 0x64));
        sw(pv + 0x68, (lbu(pv + 0x6e) << 16) | (lbu(pv + 0x6d) << 8) | lbu(pv + 0x6c));
    }
    const uint32_t pos = m + 0x10;
    const float dist = vec2_distance(V(OV(0x00166dc0)), V(pos));
    if (48.0f <= dist) return;
    if (32.0f <= dist) {
        const float fade = fpu_sub(1.0f, fpu_mul(fpu_sub(dist, 32.0f), 0.0625f));
        sw(pv + 0x60, static_cast<uint32_t>(float_to_int(fpu_mul(int_to_float(static_cast<int32_t>(lbu(pv + 0x6f))), fade))));
    } else {
        sw(pv + 0x60, lbu(pv + 0x6f));
    }
    sub_001f93e0(on_level(RILGAR) ? L5_ADDR(0x0031cf58) : L14_ADDR(0x00308150), m);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x810000u), 0);
    sb(m + 0xa4, 0xff);
    if (hit == 0) return;
    const uint32_t words = glow_words();
    const uint32_t sp = ee_stack_alloc(0x130);
    const uint32_t glow = sp + 0x10, push = sp + 0x20, vel = sp + 0x30, at = sp + 0x40;
    vec3_scale(V(glow), V(m + 0xe0), lwc1(words));
    vec3_add(V(glow), V(pos), V(glow));
    bool breaks = (lwu(hit + 0x24) & NOT_ADDR(0x800000u)) != 0;
    if (!breaks) {
        const float near = u2f(0x3f4ccccdu);  // (0.8)
        if (vec3_distance(V(glow), V(hit)) <= near) {
            breaks = true;
        } else {
            const uint32_t src = lwu(hit + 0x20);
            if (lh(src + 0xa6) != 0x47) {
                ee_stack_free(0x130);
                return;
            }
            breaks = vec3_distance(V(glow), V(src + 0x10)) <= near;
        }
    }
    if (!breaks) {
        ee_stack_free(0x130);
        return;
    }
    const float grav = 12.0f;
    const uint32_t none = OV(0x0015f580), ang = m + 0x40;
    moby_sound_start(0, 0, m);
    class1511_remains(m);
    uint32_t d = debris_spawn(m, 0x5ed, pos, ang, frame_scale_i(0x5a), 0, none, none, none, fpu_mul(lwc1(0x0015ed70), grav));
    if (d != 0) sd(d + 0x38, ld(m + 0x38));
    plant_bolts_drop_l1(m);
    // two pieces pushed apart along the moby's x axis (and up)
    vec3_scale(V(push), V(m + 0xc0), fpu_mul(lwc1(words + 8), lwc1(0x0015ed60)));
    swc1(push + 8, fpu_add(lwc1(push + 8), fpu_mul(lwc1(words + 0xc), lwc1(0x0015ed60))));
    d = debris_spawn(m, 0x5ee, pos, ang, frame_scale_i(0x5a), 0, push, none, none, fpu_mul(lwc1(0x0015ed70), grav));
    if (d != 0) sd(d + 0x38, ld(m + 0x38));
    swc1(push, -lwc1(push));
    swc1(push + 4, -lwc1(push + 4));
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), u2f(0x40490fdbu)));  // (pi)
    d = debris_spawn(m, 0x5ee, pos, ang, frame_scale_i(0x5a), 0, push, none, none, fpu_mul(lwc1(0x0015ed70), grav));
    if (d != 0) sd(d + 0x38, ld(m + 0x38));
    for (int32_t k = 0; k < 24; ++k) {
        // a spark pair (particle 0x35, the glow's colour then white) flying off at a random angle, then a puff (0x17)
        const float size = rand_range_f(u2f(0x3e99999au), 4.0f);  // (0.3 .. 4)
        int32_t spin = rand_mod(2);
        if (spin == 0) spin = -1;
        const float yaw = rand_angle();
        const float pitch = rand_range_f(u2f(0x3eb2b8c2u), u2f(0x3fc49809u));  // (0.35 .. 1.54)
        const float speed = rand_range_f(u2f(0x3d4ccccdu), u2f(0x3dcccccdu));  // (0.05 .. 0.1)
        vec_from_spherical_v(V(vel), fpu_mul(speed, lwc1(0x0015ed60)), yaw, pitch);
        lq_sq(at, glow);
        for (uint32_t c = 0; c < 0xc; c += 4)
            swc1(at + c, fpu_add(lwc1(at + c), rand_range_signed_f(0.0f, u2f(0x3e19999au))));  // (0.15)
        const float grow = u2f(0x3b449ba6u);  // (0.003)
        spawn_particle_35(at, static_cast<uint32_t>(frame_scale_i(0x3c)), lwu(pv + 0x68) | 0x7f000000u, 0, static_cast<uint32_t>(spin), vel,
                          fpu_mul(size, u2f(0x3dcccccdu)), size, grow);
        spawn_particle_35(at, static_cast<uint32_t>(frame_scale_i(0x3c)), 0x7f7f7f7fu, 1, static_cast<uint32_t>(-spin), vel,
                          fpu_mul(size, u2f(0x3d8f5c29u)), fpu_mul(size, u2f(0x3f333333u)), grow);  // (0.07, 0.7)
        swc1(vel + 4, 0.0f);
        swc1(vel, 0.0f);
        swc1(vel + 8, fpu_mul(rand_range_f(u2f(0x3c23d70au), u2f(0x3d4ccccdu)), lwc1(0x0015ed60)));  // (0.01 .. 0.05)
        const float puff = rand_range_f(60000.0f, 200000.0f);
        const int32_t a = rand_mod(2);
        const int32_t side = rand_mod(2) == 0 ? a : -a;
        const uint32_t p = spawn_particle_17(glow, side, vel, 0x407f7f7fu, u2f(0x3e99999au), 1.0f, u2f(0x3f828f5cu), puff);  // (0.3, 1, 1.02)
        if (p != 0) {
            sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(rand_range(0x5a, 0x78))));
            sw(p + 0x24, 2);
            sb(p + 0x2a, static_cast<uint32_t>(rand_range(0x40, 0x60)));
            sb(p + 0x2b, lbu(p + 0xa));
            if (rand_mod(2) != 0) sb(p + 3, 0x44);
        }
    }
    moby_retire(m);
    ee_stack_free(0x130);
}

}  // namespace rac1
