// Two kinds of glowing round that fly straight on, whistle once as they pass the hero, and burst where they hit something
// (sparks off the surface) or when their fuse runs out: the shells of Gaspar (level 9) and Orxon (10), class 0x4ea (1258:
// their burst also explodes and blasts the mobys around), and the shots of Eudora (4) and Hoven (12), class 0xb8 (184: a
// launcher of class 0xff or 0x4b1 fires the strong kind). Each kind's levels have the same code, relinked: no level word; the
// camera 0x166dc0, the hit's point and normal 0x173e60 / 0x173e80 go through OV(), the base ELF's are bare (the hero 0x13f3d0,
// the frame time 0x15ed6c, the game's level number 0x15ed84). Header shared_shell.h.
#include <game/ovl/shared_shell.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t kShellClass = 0x4ea;
constexpr uint32_t kShotClass = 0xb8;
constexpr uint32_t HERO_POS = 0x0013f3d0;

float dt() { return lwc1(0x0015ed6c); }

// the hit's record (what a hit leaves in the hit table): a direction, the round, its kind, its damage
void hit_record(uint32_t* record, const Vec4& dir, uint32_t m, uint32_t kind, uint32_t bytes, float damage) {
    record[0] = f2u(dir.x);
    record[1] = f2u(dir.y);
    record[2] = f2u(1.0f);
    record[3] = 0x45afdf66u;  // (5627.92)
    record[4] = m;
    record[5] = kind;
    record[6] = bytes | (static_cast<uint32_t>(lhu(m + 0xa6)) << 16);
    record[7] = f2u(damage);
    record[8] = 1u;
}

// The whistle (once, going away from the hero after coming closer, within 3) and the round's move by its velocity; `from`
// gets where it was.
void fly(uint32_t m, uint32_t pv, Vec4& from) {
    const float dist = vec3_distance(V(m + 0x10), V(HERO_POS));
    from = V(m + 0x10);
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
    if (lw(pv + 0x28) == 0 && lw(pv + 0x24) != 0 && lwc1(pv + 0x20) < dist && dist < 3.0f) {
        moby_sound_start(0, 0, m);
        sw(pv + 0x28, 1);
    }
    if (dist < lwc1(pv + 0x20)) sw(pv + 0x24, 1);
    swc1(pv + 0x20, dist);
}

// The fuse: when it runs out the first time it is lit again for a quarter of its start (the burst's last frames); false
// when the round is spent (out a second time).
bool fuse_burns(uint32_t m, uint32_t pv) {
    int16_t fuse = static_cast<int16_t>(lh(pv + 0x1c));
    const int32_t ticked = timer_tick_i16(fuse);
    sh(pv + 0x1c, static_cast<uint32_t>(fuse));
    if (ticked != 0 && lbu(m + 0xbc) == 0) {
        sb(m + 0xbc, 1);
        sh(pv + 0x1c, static_cast<uint32_t>(static_cast<int32_t>(lh(pv + 0x1e)) / 4));
        return true;
    }
    return lh(pv + 0x1c) > 0 || lbu(m + 0xbc) == 0;
}

// five sparks (particle 0x1b) of size `size` off the surface hit (its normal 0x173e80), bounced from the velocity
void sparks(uint32_t m, uint32_t pv, float size) {
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t spark = sp + 0x60, bounce = sp + 0x70;
    for (int32_t i = 0; i < 5; ++i) {
        Vec4 r{};
        r.x = rand_range_f(-1.0f, 1.0f);
        r.y = rand_range_f(-1.0f, 1.0f);
        r.z = rand_range_f(-1.0f, 1.0f);
        V(spark) = r;
        vec3_reflect(V(bounce), V(pv), V(OV(0x00173e80)));
        vec3_set_length(V(spark), V(spark), fpu_mul(vec3_length(V(bounce)), 0.5f));
        vec3_add(V(spark), V(bounce), V(spark));
        vec3_set_length(V(spark), V(spark), rand_range_f(fpu_mul(dt(), 3.0f), fpu_mul(dt(), 6.0f)));
        const int32_t lo = frame_scale_i(10);
        const int32_t hi = frame_scale_i(15);
        spawn_particle_1b(m + 0x10, spark, 0x7f2f4f6fu, static_cast<uint32_t>(rand_range(lo, hi)), size);
    }
    ee_stack_free(0x80);
}
}  // namespace

// A shell from `src` at `pos` with the velocity `vel` (EE vectors), its fuse `fuse` frames; the shell or 0.
// orig: L9:0x00307ba8 L10:0x002e68b8
uint32_t shell_launch(uint32_t src, uint32_t pos, uint32_t vel, int32_t fuse) {
    const Vec4 at = V(pos);
    const Vec4 speed = V(vel);
    const uint32_t m = moby_spawn(kShellClass);
    if (m == 0) return 0;
    const uint32_t pv = lwu(m + 0x78);
    sw(pv + 0x18, src);
    sw(m + 0x40, 0);
    sb(m + 0x30, 0xff);
    swc1(m + 0x2c, fpu_mul(lwc1(m + 0x2c), 4.0f));
    sh(m + 0x32, 0xff);
    sb(m + 0x31, 1);
    swc1(m + 0x44, -atan2_f(vec2_length(speed), speed.z));
    swc1(m + 0x48, atan2_f(speed.x, speed.y));
    V(m + 0x10) = at;
    V(pv) = speed;
    const float turn = fpu_mul(dt(), u2f(0x3fc90fdbu));  // (a quarter turn a second at most)
    const float spin = rand_range_f(-turn, turn);
    sw(pv + 0x14, 0);
    sh(pv + 0x1c, static_cast<uint32_t>(fuse));
    sh(pv + 0x1e, static_cast<uint32_t>(fuse));
    swc1(pv + 0x10, spin);
    swc1(pv + 0x20, vec3_distance(V(m + 0x10), V(HERO_POS)));
    sw(pv + 0x28, 0);
    sw(pv + 0x24, 0);
    const uint32_t glow = static_cast<uint32_t>(fuse * 5 / 4);
    sh(m + 0x34, lhu(m + 0x34) | 0x200u);
    sb(m + 0x23, 0x40);
    spawn_particle_1a(m, 0x2f7f4f4fu, glow, 0xffffffffu, 400000.0f);
    spawn_particle_1a(m, 0x4f7f7f7fu, glow, 0xffffffffu, 160000.0f);
    moby_update_transform(m);
    return m;
}

// The shell (pvars: its velocity +0, its spin +0x10, its source +0x18, its fuse +0x1c (and its start +0x1e), the hero's last
// distance +0x20, "it came closer" +0x24, "it whistled" +0x28; the moby's +0xbc: the fuse has run out once).
// orig: L9:0x00307d68 L10:0x002e6a78
void shell_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    Vec4 from{};
    fly(m, pv, from);
    const float x = lwc1(m + 0x10), y = lwc1(m + 0x14), z = lwc1(m + 0x18);
    const bool inside = !(x < 2.0f) && !(1021.0f < x) && !(y < 2.0f) && !(1021.0f < y) && !(z < 2.0f) && !(1021.0f < z);
    if (!inside || 255.0f < vec2_distance(V(m + 0x10), V(OV(0x00166dc0)))) {  // (out of the level, or far from the camera)
        moby_retire(m);
        return;
    }
    // while the fuse has not run out: the record, its direction the way from the hero
    uint32_t record[9];
    const bool armed = lbu(m + 0xbc) == 0;
    if (armed) {
        Vec4 away{};
        vec3_sub(away, V(m + 0x10), V(HERO_POS));
        vec2_set_length(away, away, 1.0f);
        hit_record(record, away, m, 0x70001u, 1u | (3u << 8), 3.0f);
    }
    if (!fuse_burns(m, pv)) {
        moby_retire(m);
        return;
    }
    if (segment_collide_v(from, V(m + 0x10), 0, lwu(pv + 0x18), armed ? record : nullptr) == 0) return;
    V(m + 0x10) = V(OV(0x00173e60));  // (the hit point)
    if (lbu(m + 0xbc) == 0) sparks(m, pv, 60000.0f);
    explosion_effect(m, pv, 0, 4, 3, 6, -1, 0, 1, -1, 0, 0.0f, 0.0f, 1.1f, 0.6f, 1.0f, 0.9f, 0.0f);
    mobys_in_sphere_v(V(m + 0x10), 1.1f, 0x10, m, armed ? record : nullptr);
    moby_retire(m);
}

// A shot of class 0xb8 from `src` at `pos` with the velocity `vel` (EE vectors), its fuse `fuse` frames; the shot or 0. A
// launcher of class 0xff fires a full-size, red-glowing one; the others' are half size and blue.
// orig: L4:0x002aee30 L12:0x002d4150
uint32_t shot_launch(uint32_t src, uint32_t pos, uint32_t vel, int32_t fuse) {
    const Vec4 at = V(pos);
    const Vec4 speed = V(vel);
    const uint32_t m = moby_spawn(kShotClass);
    if (m == 0) return 0;
    const uint32_t pv = lwu(m + 0x78);
    sw(pv + 0x18, src);
    sb(m + 0x30, 0xff);
    sh(m + 0x32, 0xff);
    sb(m + 0x31, 1);
    const bool red = lh(src + 0xa6) == 0xff;
    if (!red) swc1(m + 0x2c, fpu_mul(lwc1(m + 0x2c), 0.5f));
    sw(m + 0x40, 0);
    sw(m + 0x44, 0);
    swc1(m + 0x48, atan2_f(speed.x, speed.y));
    V(m + 0x10) = at;
    V(pv) = speed;
    const float turn = fpu_mul(dt(), u2f(0x3fc90fdbu));
    const float spin = rand_range_f(-turn, turn);
    sw(pv + 0x14, 0);
    sh(pv + 0x1c, static_cast<uint32_t>(fuse));
    sh(pv + 0x1e, static_cast<uint32_t>(fuse));
    swc1(pv + 0x10, spin);
    swc1(pv + 0x20, vec3_distance(V(m + 0x10), V(HERO_POS)));
    sw(pv + 0x28, 0);
    sw(pv + 0x24, 0);
    const uint32_t glow = static_cast<uint32_t>(fuse * 5 / 4);
    spawn_particle_1a(m, red ? 0x2f7f4f4fu : 0x2f4f7f7fu, glow, 0xffffffffu, 100000.0f);
    spawn_particle_1a(m, red ? 0x4f7f7f7fu : 0x4f6f7f7fu, glow, 0xffffffffu, 40000.0f);
    moby_update_transform(m);
    return m;
}

// The shot (pvars as the shell's). It is gone below the level's floor (any coordinate under 0) or past its range from the
// camera: 64, 128 from a launcher of class 0xff, 100 on Hoven.
// orig: L4:0x002af058 L12:0x002d4378
void shot_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    float range = 64.0f;
    if (lh(lwu(pv + 0x18) + 0xa6) == 0xff) range = 128.0f;
    if (lw(0x0015ed84) == 12) range = 100.0f;
    Vec4 from{};
    fly(m, pv, from);
    if (lwc1(m + 0x10) < 0.0f || lwc1(m + 0x14) < 0.0f || lwc1(m + 0x18) < 0.0f || range < vec2_distance(V(m + 0x10), V(OV(0x00166dc0)))) {
        moby_retire(m);
        return;
    }
    // while the fuse has not run out: the record, its direction the shot's own; a launcher of class 0xff or 0x4b1 hits harder
    uint32_t record[9];
    const bool armed = lbu(m + 0xbc) == 0;
    if (armed) {
        const int32_t cls = lh(lwu(pv + 0x18) + 0xa6);
        const bool strong = cls == 0xff || cls == 0x4b1;
        Vec4 dir = V(pv);
        vec2_set_length(dir, dir, 1.0f);
        hit_record(record, dir, m, strong ? 0x50001u : 0x10001u, 1u | (1u << 8), strong ? 3.0f : 1.0f);
    }
    if (!fuse_burns(m, pv)) {
        moby_retire(m);
        return;
    }
    if (segment_collide_v(from, V(m + 0x10), 0, lwu(pv + 0x18), armed ? record : nullptr) == 0) return;
    V(m + 0x10) = V(OV(0x00173e60));  // (the hit point)
    if (lbu(m + 0xbc) == 0) sparks(m, pv, 30000.0f);
    moby_retire(m);
}

}  // namespace rac1
