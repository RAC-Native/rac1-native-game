// Class 295, a rocket of the space levels, and class 1034, Pokitaru's, Gemlik Base's and Drek's fleet's other build of it:
// one port of the levels' copies of each (the same code relinked), the two builds one function with their differences as
// a RocketBuild. Nothing of it is the level's own: the effects' table 0x1b2080 and the collision's hit point 0x173e60 are the
// engine's (OV), the frame time 0x15ed6c and its square 0x15ed70 the base ELF's (bare). The smoke is Veldin's Devastator
// trail particle (dev_2712b8). Header shared_class295.h.
//
// The pvars: +0 the velocity, +0x10 the point the target was last seen at, +0x20 the moby the rocket's line of flight
// ignores (its owner), +0x24 the target (cleared each frame), +0x28 the frames it flies, +0x2c its speed, +0x30 / +0x34
// the yaw's and the pitch's turning rates, +0x3c the target's class.
#include <game/ovl/shared_class295.h>

#include <game/math/vecmath.h>
#include <game/ovl/devastator.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
// What the two builds do differently
struct RocketBuild {
    uint32_t frame;          // (its stack frame)
    bool tint_first;         // the first puff tinted 0x48 too
    uint32_t puff_tint;      // the trail's puffs' tint (their +3)
    bool tint_sparks;        // the sparks tinted 0x48
    bool drop_hidden;        // a hidden target without collision dropped
    bool class_check;        // the target must be of the class +0x3c
    uint32_t pitch_rate;     // the pvar of the pitch's turning rate (+0x34, or the yaw's +0x30)
    bool blast_at_copy;      // the blast at a copy of the hit point (else at the collision's own)
    int32_t blast_c;         // the blast's argument C
};
constexpr RocketBuild CLASS295{0x130, true, 0x48, true, true, true, 0x34, true, 0};
constexpr RocketBuild CLASS1034{0x140, false, 0x44, false, false, false, 0x30, false, -1};

constexpr uint32_t SPARK_TINT = 0x48;

// the target still homed on: alive, (of the class the rocket wants,) within 3 of where it was last seen
bool target_held(uint32_t pv, const RocketBuild& b) {
    const uint32_t t = lwu(pv + 0x24);
    if (t == 0 || (b.class_check && lh(t + 0xa6) != lw(pv + 0x3c))) return false;
    const uint32_t st = lbu(t + 0x20);
    if (st == 0xfe || st == 0xfd) return false;
    return vec3_distance(V(pv + 0x10), V(t + 0x10)) < 3.0f;
}

// Each frame: it grows to its scale; a puff of smoke at its tail drifting back along its flight, then three more spread
// along the last frame's flight with a spark each; a target that is hidden and has no collision (flags 1 without 0x1000)
// is dropped; it turns straight at a held target (and notes where it is), else its yaw and pitch seek their rates; it moves
// (its speed along its yaw / pitch) and is gone outside 2 .. 1021 on an axis; where the line from where it was to its nose
// (chain point 1) meets something it blows up there; else it goes when its frames run out.
void rocket_update(uint32_t m, const RocketBuild& b) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10;
    const uint32_t sp = ee_stack_alloc(b.frame);
    const uint32_t from = sp + 0x20, nose = sp + 0x30, puff = sp + 0x40, drift = sp + 0x50, dir = sp + 0x60;
    lq_sq(from, pos);
    {
        const float sc = lwc1(m + 0x2c);
        swc1(m + 0x2c, fpu_add(sc, fpu_mul(fpu_sub(lwc1(lwu(m + 0x24) + 0x24), sc), 0.1f)));
    }
    // the smoke's drift: a random direction at 0.1 .. 0.2 a frame, less a part of the flight
    V(dir) = Vec4{};
    swc1(dir, rand_range_f(-1.0f, 1.0f));
    swc1(dir + 4, rand_range_f(-1.0f, 1.0f));
    swc1(dir + 8, rand_range_f(-1.0f, 1.0f));
    lq_sq(drift, dir);
    vec3_set_length(V(drift), V(drift), fpu_mul(rand_range_f(0.1f, 0.2f), lwc1(0x0015ed6c)));
    {
        const float ft = lwc1(0x0015ed6c);
        vec3_set_length(V(puff), V(pv), -rand_range_f(fpu_mul(ft, 0.1f), ft));
    }
    vec3_add(V(drift), V(drift), V(puff));
    {
        const uint32_t p = dev_2712b8(pos, drift, static_cast<uint32_t>(frame_scale_i(6)), 0x7f, 0xb0b0b0, 3, 40000.0f, 1000.0f, 1.0f,
                                      u2f(0xb9d1b717u), 0.0f);  // (-0.0004)
        if (b.tint_first && p != 0) sb(p + 3, SPARK_TINT);
    }
    for (float t0 = 0.0f; t0 < 1.0f;) {
        const float t1 = fpu_add(t0, u2f(0x3eaaaaabu));  // (a third)
        vec3_set_length(V(puff), V(pv), fpu_mul(rand_range_f(t0, t1), lwc1(pv + 0x2c)));
        vec3_add(V(puff), V(puff), V(pos));
        const uint32_t p = dev_2712b8(puff, drift, static_cast<uint32_t>(frame_scale_i(0x3c)), 0x7f, 0x606060, 3, 40000.0f, 1000.0f, 1.0f,
                                      u2f(0xb951b717u), 0.0f);  // (-0.0002)
        if (p != 0) {
            sb(p + 3, b.puff_tint);
            sb(p + 2, lbu(lwu(OV(0x001b2080) + 0x5c)));
        }
        V(dir) = Vec4{};
        swc1(dir + 8, 0.02f);
        mat3_transform(V(dir), V(dir), M(m + 0xc0));
        vec_rotate_axis(V(dir), V(dir), V(m + 0xc0), rand_angle());
        const uint32_t s = spawn_particle_15_v(V(puff), V(dir), 0x4f007fff, 0x1fffffff, static_cast<uint32_t>(frame_scale_i(5)), 1, 20000.0f);
        if (b.tint_sparks && s != 0) sb(s + 3, SPARK_TINT);
        t0 = t1;
    }
    if (b.drop_hidden) {
        const uint32_t t = lwu(pv + 0x24);
        if (t != 0) {
            const uint32_t flags = lhu(t + 0x34);
            if ((flags & 0x1000u) == 0 && (flags & 1u) != 0 && lwu(t + 0x94) == 0) sw(pv + 0x24, 0);
        }
    }
    if (target_held(pv, b)) {
        const uint32_t t = lwu(pv + 0x24);
        swc1(m + 0x48, atan2_f(fpu_sub(lwc1(t + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(t + 0x14), lwc1(m + 0x14))));
        const float across = vec2_distance(V(pos), V(t + 0x10));
        swc1(m + 0x44, -atan2_f(across, fpu_sub(lwc1(t + 0x18), lwc1(m + 0x18))));
        lq_sq(pv + 0x10, t + 0x10);
    } else {
        const float accel = u2f(0x437b53d1u), most = u2f(0x451d1463u);  // (80 pi, 800 pi)
        yaw_seek(m + 0x48, pv + 0x30, lwc1(m + 0x48), fpu_mul(lwc1(0x0015ed70), accel), fpu_mul(lwc1(0x0015ed70), accel),
                 fpu_mul(lwc1(0x0015ed6c), most));
        yaw_seek(m + 0x44, pv + b.pitch_rate, lwc1(m + 0x44), fpu_mul(lwc1(0x0015ed70), accel), fpu_mul(lwc1(0x0015ed70), accel),
                 fpu_mul(lwc1(0x0015ed6c), most));
        sw(pv + 0x24, 0);
    }
    vec_from_spherical_v(V(pv), lwc1(pv + 0x2c), lwc1(m + 0x48), -lwc1(m + 0x44));
    vec3_add(V(pos), V(pos), V(pv));
    bool out = false;
    for (uint32_t axis = 0; axis < 0xc && !out; axis += 4) {
        const float c = lwc1(pos + axis);
        out = c < 2.0f || 1021.0f < c;
    }
    if (out) {
        moby_retire(m);
        ee_stack_free(b.frame);
        return;
    }
    moby_chain_point_v(m, 1, V(nose));
    if (segment_collide_v(V(from), V(nose), 0, lwu(pv + 0x20), nullptr) != 0) {
        uint32_t at = OV(0x00173e60);
        if (b.blast_at_copy) {
            lq_sq(puff, at);
            at = puff;
        }
        explosion_effect(m, pv, at, 0xa, 3, 0x10, b.blast_c, 0, 1, -1, 0, 2.0f, 10.0f, 4.0f, 2.0f, 9.0f, 1.0f, 20.0f);
        moby_retire(m);
    } else if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x28))) != 0) {
        moby_retire(m);
    }
    ee_stack_free(b.frame);
}
}  // namespace

// orig: L13:0x002e6c08 L17:0x002c9ce8
void class295_update(uint32_t m) { rocket_update(m, CLASS295); }

// orig: L13:0x002e6a58 L17:0x002c9b38
uint32_t class295_launch(uint32_t owner, uint32_t pos, uint32_t target, uint32_t rot, int32_t frames, float speed) {
    const uint32_t r = moby_spawn(0x127);
    if (r == 0) return 0;
    const uint32_t pv = lwu(r + 0x78);
    sb(r + 0x30, 0xff);
    sh(r + 0x32, 0xff);
    sb(r + 0x31, 1);
    sb(r + 0x20, 0);
    sw(pv + 0x20, owner);
    sw(pv + 0x24, target);
    lq_sq(r + 0x10, pos);
    lq_sq(r + 0x40, rot);
    if (target != 0) {
        sw(pv + 0x3c, static_cast<uint32_t>(lh(target + 0xa6)));
        const uint32_t word = moby_pvar_word0(target);
        if (word != 0) sh(word + 0x1e, lhu(word + 0x1e) | 0x80u);
    }
    sw(pv + 0x28, static_cast<uint32_t>(frames));
    swc1(pv + 0x2c, speed);
    sw(pv + 0x30, 0);
    sw(pv + 0x34, 0);
    vec_from_spherical_v(V(pv), speed, lwc1(rot + 8), -lwc1(rot + 4));
    if (lbu(r + 0x53) != 1) moby_anim_set(r, 1, 0, 0xa);
    vec3_add(V(r + 0x10), V(r + 0x10), V(pv));
    if (const uint32_t t = lwu(pv + 0x24); t != 0) lq_sq(pv + 0x10, t + 0x10);
    swc1(r + 0x2c, fpu_mul(lwc1(lwu(r + 0x24) + 0x24), fpu_div(lwc1(owner + 0x2c), lwc1(lwu(owner + 0x24) + 0x24))));
    moby_update_transform(r);
    spawn_particle_1a(r, 0x60808080, static_cast<uint32_t>(frames), 0, 500000.0f);
    return r;
}

// Class 1034: the same rocket without the first puff's and the sparks' tint, its puffs tinted 0x44, a target homed on
// whatever its class (never dropped for being hidden), the pitch turning at the yaw's rate, the blast (C = -1) at the
// collision's hit point itself.
// orig: L11:0x003094f8 L13:0x00303128 L17:0x002e30c8
void class1034_update(uint32_t m) { rocket_update(m, CLASS1034); }

}  // namespace rac1
