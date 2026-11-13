// Classes 302 and 303 of Blarg (L6) and Orxon (L10): one port of the two levels' copies, which are the same code relinked
// (the normalised texts compared): class 302's update, its break into shards and its spawn, class 303's update and spawn
// (the Gadgebots of class 857 spawn both). Class 302's draw reads the level's own mesh (one block of tables, per level below) and
// its drawing words (the level's own small data, per level below); the camera 0x166dc0 and the frame counter 0x15f5cc are
// the engine's (OV). Header shared_class302.h.
#include <game/ovl/shared_class302.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/shared_beam.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l10.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
// The level's mesh (its own data, one block): the 57 points +0, the 42 quads' corners +0x390 (four (point, texture
// coordinate) index pairs of halves each), their fixed texture coordinates +0x630, the quads' normals +0x800, the points'
// normals +0xaa0, the quads' centres +0xe30, and two buffers the draw fills: the points' mapped texture coordinates +0x10d0
// and the points in the world +0x12a0.
uint32_t mesh(uint32_t o) {
    if (on_level(ORXON)) return L10_ADDR(0x001d2f90) + o;
    return L6_ADDR(0x001d3290) + o;
}
// The draw's words (the level's own data): the blend's four selectors +0..+0xc and its fixed alpha +0x10, the texture
// +0x14, the colour +0x18, the normals' length +0x1c.
uint32_t words(uint32_t o) {
    if (on_level(ORXON)) return L10_ADDR(0x00161850) + o;
    return L6_ADDR(0x00161870) + o;
}
constexpr int32_t POINTS = 57;
constexpr int32_t QUADS = 42;
}  // namespace

// The moby's matrix (its rows +0xc0.., its position) places the mesh's points (into the level's buffer +0x12a0). With the
// camera within 16 units (x and y), each point's texture coordinates are the sphere map of the camera's view reflected off
// its normal; else the quads keep their fixed ones. Each quad facing the camera (its normal against the view of its centre)
// is drawn with the level's texture, colour and blend.
// orig: L6:0x002d8738 L10:0x002bd280
void class302_draw(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x1d0);
    const uint32_t quad = sp, colours = sp + 0x40, uvs = sp + 0x50, regs = sp + 0x70, matrix = sp + 0x90;
    const uint32_t view = sp + 0xd0, centre = sp + 0xe0, normal = sp + 0xf0, reflect = sp + 0x100, near_flag = sp + 0x110;
    const uint32_t cam = OV(0x00166dc0);
    lq_sq(matrix, m + 0xc0);
    lq_sq(matrix + 0x10, m + 0xd0);
    lq_sq(matrix + 0x20, m + 0xe0);
    swc1(matrix + 0x34, lwc1(m + 0x14));
    swc1(matrix + 0x38, lwc1(m + 0x18));
    swc1(matrix + 0x3c, 1.0f);
    swc1(matrix + 0x30, lwc1(m + 0x10));
    sw(near_flag, 0);
    if (abs_f(fpu_sub(lwc1(cam), lwc1(m + 0x10))) < 16.0f && abs_f(fpu_sub(lwc1(cam + 4), lwc1(m + 0x14))) < 16.0f) sw(near_flag, 1);
    const uint32_t world = mesh(0x12a0), mapped = mesh(0x10d0);
    if (lw(near_flag) != 0) {
        for (int32_t i = 0; i < POINTS; ++i) {
            const uint32_t o = static_cast<uint32_t>(i) << 4;
            mat4_transform(V(world + o), V(mesh(0) + o), M(matrix));
            vec3_sub(V(view), V(world + o), V(cam));
            vec3_set_length(V(view), V(view), 1.0f);
            mat4_transform(V(normal), V(mesh(0xaa0) + o), M(matrix));
            vec3_set_length(V(normal), V(normal), lwc1(words(0x1c)));
            const float d = vec3_dot(V(normal), V(view));
            vec3_scale(V(reflect), V(normal), fpu_add(d, d));
            vec3_sub(V(reflect), V(view), V(reflect));
            vec3_set_length(V(reflect), V(reflect), 1.0f);
            const float z = fpu_add(lwc1(reflect + 8), 1.0f);
            swc1(reflect + 8, z);
            const float root = sqrt_f(fpu_add(z, z));
            const float s = fpu_add(root, root);
            const uint32_t uv = mapped + static_cast<uint32_t>(i) * 8;
            swc1(uv + 4, fpu_add(fpu_div(lwc1(reflect + 4), s), 0.5f));
            swc1(uv, fpu_add(fpu_div(lwc1(reflect), s), 0.5f));
        }
    } else {
        for (int32_t i = 0; i < POINTS; ++i) {
            const uint32_t o = static_cast<uint32_t>(i) << 4;
            mat4_transform(V(world + o), V(mesh(0) + o), M(matrix));
        }
    }
    // (the selectors loaded sign-extended and shifted as doublewords, as the original does)
    auto word = [](uint32_t o) { return static_cast<uint64_t>(static_cast<int64_t>(lw(words(o)))); };
    const uint64_t tex0 = effect_tex0(lw(words(0x14)));
    const uint64_t blend = word(0) | word(4) << 2 | word(8) << 4 | word(0xc) << 6 | word(0x10) << 32;
    sd(regs + 8, tex0);
    sd(regs + 0x10, 0x0000ff9000000260ull);
    sd(regs + 0x18, blend);
    for (uint32_t k = 0; k < 4; ++k) sw(colours + k * 4, lwu(words(0x18)));
    sd(regs, 0);
    for (int32_t q = 0; q < QUADS; ++q) {
        const uint32_t o = static_cast<uint32_t>(q) << 4;
        mat4_transform(V(centre), V(mesh(0xe30) + o), M(matrix));
        vec3_sub(V(view), V(centre), V(cam));
        vec3_set_length(V(view), V(view), 1.0f);
        mat3_transform(V(centre), V(mesh(0x800) + o), M(matrix));
        if (0.0f < vec3_dot(V(view), V(centre))) continue;
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t corner = mesh(0x390) + o + k * 4;
            const uint32_t point = static_cast<uint32_t>(lh(corner));
            lq_sq(quad + k * 0x10, world + (point << 4));
            const uint32_t uv = lw(near_flag) != 0 ? mapped + (point << 3) : mesh(0x630) + (static_cast<uint32_t>(lh(corner + 2)) << 3);
            swc1(uvs + k * 8, lwc1(uv));
            swc1(uvs + k * 8 + 4, lwc1(uv + 4));
        }
        quad_draw(quad, 0);
    }
    ee_stack_free(0x1d0);
}

namespace {
// the colour pulse of classes 302 and 303: a pulse every 150 frames between two greys, the sine of the frame counter's
// phase (0 .. 2 pi, wrapped to -pi .. pi)
uint32_t pulse_colour() {
    const float pi = u2f(0x40490fdbu);
    float a = fpu_div(int_to_float(lw(OV(0x0015f5cc)) % 150), 150.0f);
    a = fpu_mul(fpu_add(a, a), pi);
    if (pi < a) a = fpu_sub(a, u2f(0x40c90fdbu));
    return rgba_lerp(0x80464646u, 0x80828282u, fpu_mul(fpu_add(sin_vu(a), 1.0f), 0.5f));
}
// class 302's draw callback (the level's copy of class302_draw)
uint32_t class302_draw_callback() { return on_level(ORXON) ? L10_ADDR(0x002bd280) : L6_ADDR(0x002d8738); }
}  // namespace

// orig: L6:0x002d8c20 L10:0x002bd768
void class303_update(uint32_t m) {
    const uint32_t watched = lwu(m + 0xb8);
    if (watched != 0 && lbu(watched + 0x20) < 2) {
        sw(m + 0x90, pulse_colour());
        return;
    }
    sw(m + 0x90, rgba_lerp(lwu(m + 0x90), 0x80464646u, u2f(0x3dcccccdu)));
}

// Class 302 broken by a hit from `hit` (a moby, or 0: the hero's): its sound, a puff of six smoke particles, ten shards
// thrown out (away from the hitter, or from the hero) at a third of their size; then state 2 for 15 frames, its collision
// off (+0x34 bit 0) and +0x94 cleared.
// orig: L6:0x002d8178 L10:0x002bccc0
void class302_break(uint32_t m, uint32_t hit) {
    const uint32_t sp = ee_stack_alloc(0x40);
    const uint32_t puff = sp, at = sp + 0x20, vel = sp + 0x30;
    moby_sound_start(0, 0, m);
    lq_sq(puff, m + 0x10);
    swc1(puff + 8, fpu_add(lwc1(puff + 8), u2f(0x3e99999au)));  // (0.3 up)
    const float jitter = u2f(0x3e99999au), lo = u2f(0x3f8147aeu), spread = u2f(0x3d4ccccdu);  // (0.3, 1.01, 0.05)
    spawn_particle_0d(puff, 1, 0x40808080u, jitter, lo, u2f(0x3f89999au), spread, 50000.0f);  // (to 1.075)
    for (int32_t k = 0; k < 5; ++k)
        spawn_particle_0d(puff, 1, 0x40808080u, jitter, lo, u2f(0x3f88f5c3u), spread, 50000.0f);  // (to 1.07)
    // (the original also takes the hitter's +0x10, or the moby's way from the hero, into a stack vector it never reads)
    const float ft = lwc1(0x0015ed6c);
    for (int32_t k = 0; k < 10; ++k) {
        lq_sq(at, m + 0x10);
        swc1(at, fpu_add(lwc1(at), rand_range_signed_f(0.0f, u2f(0x3e4ccccdu))));  // (0.2)
        swc1(at + 4, fpu_add(lwc1(at + 4), rand_range_signed_f(0.0f, u2f(0x3e4ccccdu))));
        swc1(at + 8, fpu_add(lwc1(at + 8), rand_range_f(u2f(0x3e19999au), u2f(0x3f0ccccdu))));  // (0.15 .. 0.55)
        const float a = rand_angle();
        const float speed = fpu_mul(rand_range_f(2.0f, 4.5f), ft);
        swc1(vel, fpu_mul(cos_vu(a), speed));
        swc1(vel + 4, fpu_mul(sin_vu(a), speed));
        swc1(vel + 8, 0.0f);
        const float push = rand_range_f(fpu_mul(ft, u2f(0x3f333333u)), fpu_mul(ft, 1.5f));  // (0.7 .. 1.5 a second)
        const float away = hit != 0 ? atan2_f(lwc1(hit + 0x10), lwc1(hit + 0x14))
                                    : atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(0x0013f3d0)),
                                              fpu_sub(lwc1(m + 0x14), lwc1(0x0013f3d4)));
        swc1(vel, fpu_add(lwc1(vel), fpu_mul(cos_vu(away), push)));
        swc1(vel + 4, fpu_add(lwc1(vel + 4), fpu_mul(sin_vu(away), push)));
        swc1(vel + 8, fpu_mul(rand_range_f(2.0f, 7.0f), ft));
        const uint32_t shard = beam_shard_spawn(at, vel);
        if (shard != 0) swc1(shard + 0x2c, fpu_mul(lwc1(shard + 0x2c), u2f(0x3e99999au)));
    }
    sw(m + 0x94, 0);
    sb(m + 0x20, 2);
    sh(m + 0x34, lhu(m + 0x34) | 1u);
    sb(m + 0xbc, static_cast<uint32_t>(frame_scale_i(15)));
    ee_stack_free(0x40);
}

// Class 302, a breakable light: state 0 goes to 1; in state 1 its colour pulses, a hit (mask 0x10000) breaks it
// (class302_break), and it is drawn while shown (+0x31); in state 2 it is gone when its timer (+0xbc) runs out. Its hit
// slot (+0xa4) is cleared every frame.
// orig: L6:0x002d85a0 L10:0x002bd0e8
void class302_update(uint32_t m) {
    const uint32_t hit = sub_0025a420(m, 0x10000, 0);
    sb(m + 0xa4, 0xff);
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x20, 1);
        break;
    case 1:
        sw(m + 0x90, pulse_colour());
        if (hit != 0) class302_break(m, hit);
        if (lbu(m + 0x31) != 0) draw_callback_add(class302_draw_callback(), m);
        break;
    case 2:
        // (the original also copies its position, 0.3 up, into a stack vector it never reads)
        if (countdown_u8(m + 0xbc) != 0) moby_retire(m);
        break;
    default:
        break;
    }
}

// A class-302 light spawned at the moby (class 857 spawns its own): its place, its yaw and its +0x38 words; shown (+0x30 /
// +0x32 0x60, +0x31 1), its collision flag 0x1000 set. Returns it (0: none).
// orig: L6:0x002d8510 L10:0x002bd058
uint32_t class302_spawn(uint32_t m) {
    const uint32_t o = moby_spawn(0x12e);
    if (o == 0) return 0;
    sb(o + 0x30, 0x60);
    sh(o + 0x32, 0x60);
    sb(o + 0x31, 1);
    sw(o + 0x40, 0);
    sw(o + 0x44, 0);
    swc1(o + 0x48, lwc1(m + 0x48));
    sh(o + 0x34, lhu(o + 0x34) | 0x1000u);
    sd(o + 0x38, ld(m + 0x38));
    lq_sq(o + 0x10, m + 0x10);
    moby_update_transform(o);
    return o;
}

// A class-303 light spawned at the moby, watching it (+0xb8): its place, its yaw and its +0x38 words; shown (+0x32 0x60,
// +0x31 1, +0x30 0). Returns it (0: none).
// orig: L6:0x002d8b98 L10:0x002bd6e0
uint32_t class303_spawn(uint32_t m) {
    const uint32_t o = moby_spawn(0x12f);
    if (o == 0) return 0;
    sh(o + 0x32, 0x60);
    sb(o + 0x31, 1);
    sb(o + 0x30, 0);
    sw(o + 0x40, 0);
    sw(o + 0x44, 0);
    swc1(o + 0x48, lwc1(m + 0x48));
    sw(o + 0xb8, m);
    sd(o + 0x38, ld(m + 0x38));
    lq_sq(o + 0x10, m + 0x10);
    moby_update_transform(o);
    return o;
}

}  // namespace rac1
