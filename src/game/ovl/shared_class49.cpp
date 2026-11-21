// Class 49's update, its trail and its shards: one port of the copies Quartu, Kalebo III, Drek's fleet and Veldin carry (the
// same code relinked). Each level keeps its own tuning record (per level below: the trail's colours, counts, lives and
// spreads at +0 .. +0x3c, the shards' at +0x40 .. +0x5c); the trail's copy class 50 calls reads class 50's record, which its
// caller passes. The rest is the engine's (OV: the camera 0x166dc0, the collision's moby 0x173e58, the shards' velocity
// 0x15f580) or the base ELF's (bare: the frame time 0x15ed6c, the hero moby's byte 0x1413f4). Header shared_class49.h.
//
// The pvars: +0 the velocity, +0x10 the sound of its flight played, +0x14 its life, +0x18 its owner (not hit).
#include <game/ovl/shared_class49.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr float TWO_PI = 6.2831854820251465f;  // (0x40c90fdb)
constexpr float DEG = 0.01745329238474369f;    // (0x3c8efa35)
constexpr int32_t SOUND_SET = 0x27e;
}  // namespace

uint32_t class49_tuning() {
    if (on_level(QUARTU)) return L15_ADDR(0x00161418);
    if (on_level(KALEBO)) return L16_ADDR(0x00161388);
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x001613b8);
    return L18_ADDR(0x00161388);
}

// Two sparks' velocities, each the shot's velocity scaled (+0x30 / +0x34) plus a random point in a sphere whose radius is
// the frame time scaled (+0x28 / +0x2c), with the record's w (+8 / +0xc); their counts from three random ranges (+0x10 ..
// +0x24) scaled to the frame rate; one particle of type 2 spawned at the shot with the record's colours (+0 / +4) and
// texture (+0x3c).
// orig: L15:0x0029ae18 L16:0x002a07d0 L16:0x002a0f18 L17:0x002a2d18 L18:0x002a78c8 L18:0x002a8010
void class49_trail(uint32_t m, uint32_t tune) {
    const uint32_t sp = ee_stack_alloc(0xb0);
    const uint32_t jitter = sp + 0x10, vel_a = sp + 0x20, vel_b = sp + 0x30;
    const uint32_t pv = lwu(m + 0x78);
    float radius = fpu_mul(lwc1(tune + 0x28), lwc1(0x0015ed6c));
    vec3_scale(V(vel_a), V(pv), lwc1(tune + 0x30));
    rand_sphere_point(jitter, radius, radius);
    vec3_add(V(vel_a), V(vel_a), V(jitter));
    radius = fpu_mul(lwc1(tune + 0x2c), lwc1(0x0015ed6c));
    vec3_scale(V(vel_b), V(pv), lwc1(tune + 0x34));
    rand_sphere_point(jitter, radius, radius);
    vec3_add(V(vel_b), V(vel_b), V(jitter));
    swc1(vel_a + 0xc, lwc1(tune + 8));
    swc1(vel_b + 0xc, lwc1(tune + 0xc));
    const int32_t n1 = float_to_int(frame_scale_f(rand_range_f(lwc1(tune + 0x10), lwc1(tune + 0x14))));
    const int32_t n2 = float_to_int(frame_scale_f(rand_range_f(lwc1(tune + 0x18), lwc1(tune + 0x1c))));
    const int32_t n3 = float_to_int(frame_scale_f(rand_range_f(lwc1(tune + 0x20), lwc1(tune + 0x24))));
    spawn_particle_02(m + 0x10, vel_a, vel_b, lwu(tune), lwu(tune + 4), static_cast<uint32_t>(n1), static_cast<uint32_t>(n2),
                      static_cast<uint32_t>(n3), lw(tune + 0x3c));
    ee_stack_free(0xb0);
}

// The shards where it burst: their life (+0x44, frames), colour (+0x4c / +0x50 / +0x54), group (+0x48) and scale (+0x40)
// from the record, the engine's shard velocity.
// orig: L15:0x0029af88 L16:0x002a0940 L17:0x002a2e88 L18:0x002a7a38
void class49_shards(uint32_t m) {
    const uint32_t tune = class49_tuning() + 0x40;
    const int32_t life = frame_scale_i(lw(tune + 4));
    shard_spawn(m, m + 0x10, OV(0x0015f580), static_cast<uint32_t>(life), lbu(tune + 0xc), lbu(tune + 0x10), lbu(tune + 0x14),
                lbu(tune + 8), lwc1(tune));
}

// The shot spawned (class 49) at `pos`, flying at `vel` (its heading and pitch along it), shown at once (+0x31 set, then
// cleared with the moby's +0x34 bit 0 set), three times its class's size, with its owner's +0x38 word pair; its life 240
// frames.
// orig: L16:0x002a09a0 L18:0x002a7a98
uint32_t class49_spawn(uint32_t owner, uint32_t vel, uint32_t pos) {
    const uint32_t shot = moby_spawn(0x31);
    if (shot == 0) return 0;
    const float size = lwc1(shot + 0x2c);
    sb(shot + 0x30, 0xff);
    sh(shot + 0x32, 0xff);
    sb(shot + 0x31, 1);
    sb(shot + 0x20, 0);
    sd(shot + 0x38, ld(owner + 0x38));
    swc1(shot + 0x2c, fpu_mul(size, 3.0f));
    lq_sq(shot + 0x10, pos);
    const uint32_t pv = lwu(shot + 0x78);
    lq_sq(pv, vel);
    sw(pv + 0x14, static_cast<uint32_t>(frame_scale_i(0xf0)));
    sw(pv + 0x18, owner);
    sw(pv + 0x10, 0);
    swc1(shot + 0x48, atan2_f(lwc1(vel), lwc1(vel + 4)));
    swc1(shot + 0x44, -atan2_f(vec2_length(V(vel)), lwc1(vel + 8)));
    sb(shot + 0x31, 0);
    sh(shot + 0x34, lhu(shot + 0x34) | 1u);
    return shot;
}

// A shot of class 50 spawned at `pos`, flying at `vel` (its heading and pitch along it) towards `target`, as class 49's (shown at
// once, three times its size, its owner's +0x38 pair); its pvars: the velocity +0, the target +0x10, its life +0x20 / +0x30,
// its speed +0x24, a random turn (the level's class 50 record: +0 the yaw spread, +4 the pitch's, degrees) +0x28 / +0x2c.
// orig: L16:0x002a0dc0 L18:0x002a7eb8
uint32_t class50_spawn(uint32_t owner, uint32_t vel, uint32_t pos, uint32_t target, int32_t life) {
    const uint32_t shot = moby_spawn(0x32);
    if (shot == 0) return 0;
    const uint32_t spread = on_level(VELDIN_FINALE) ? L18_ADDR(0x001613e0) : L16_ADDR(0x001613e0);
    const float size = lwc1(shot + 0x2c);
    sb(shot + 0x30, 0xff);
    sh(shot + 0x32, 0xff);
    sb(shot + 0x31, 1);
    sb(shot + 0x20, 0);
    sd(shot + 0x38, ld(owner + 0x38));
    swc1(shot + 0x2c, fpu_mul(size, 3.0f));
    lq_sq(shot + 0x10, pos);
    const uint32_t pv = lwu(shot + 0x78);
    lq_sq(pv, vel);
    lq_sq(pv + 0x10, target);
    sw(pv + 0x20, static_cast<uint32_t>(life));
    sw(pv + 0x30, static_cast<uint32_t>(life));
    swc1(pv + 0x24, vec3_length(V(vel)));
    {
        const float yaw = lwc1(spread);
        swc1(pv + 0x28, fpu_mul(rand_range_f(-yaw, yaw), DEG));
    }
    swc1(pv + 0x2c, fpu_mul(rand_range_f(0.0f, lwc1(spread + 4)), DEG));
    sw(pv + 0x34, 0);
    swc1(shot + 0x48, atan2_f(lwc1(vel), lwc1(vel + 4)));
    swc1(shot + 0x44, -atan2_f(vec2_length(V(vel)), lwc1(vel + 8)));
    sb(shot + 0x31, 0);
    sh(shot + 0x34, lhu(shot + 0x34) | 1u);
    return shot;
}

// Once the camera comes within 15 it plays its flight's sound (once). State 0 flies: it spins (a turn a second), moves by its
// velocity and tests the segment from where it would be to twice its velocity behind; on a hit of anything but its owner,
// or when its life runs out, it plays the burst's sound, bursts into shards and leaves; else it moves and trails sparks.
// The hit it would give is built at full strength, or (the hero moby's byte +0x24 at 2) mostly at none.
// orig: L15:0x0029ac30 L16:0x002a05e8 L17:0x002a2b30 L18:0x002a76e0
void class49_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(pv + 0x10) == 0 && vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 15.0f) {
        moby_sound_play(9, 0, m, SOUND_SET);
        sw(pv + 0x10, 1);
    }
    if (lbu(m + 0x20) != 0) return;
    const uint32_t sp = ee_stack_alloc(0xd0);
    const uint32_t next = sp, back = sp + 0x10, rec = sp + 0x20;
    const uint32_t pos = m + 0x10;
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), fpu_mul(lwc1(0x0015ed6c), TWO_PI)));
    vec3_add(V(next), V(pos), V(pv));
    vec3_scale(V(back), V(pv), -2.0f);
    vec3_add(V(back), V(back), V(pos));
    float strength = 1.0f;
    if (lbu(0x001413f4) == 2 && rand_mod(5) != 0) strength = 0.0f;
    sub_00259888(strength, rec, static_cast<int32_t>(m), 0x10003, pv);
    sh(rec + 0x1a, lhu(m + 0xa6));
    bool burst = segment_collide(back, next, 0x10, m, rec) != 0;
    if (!burst) burst = timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x14))) != 0;
    if (burst && lwu(pv + 0x18) != lwu(OV(0x00173e58))) {
        moby_sound_play(8, 0, m, SOUND_SET);
        class49_shards(m);
        moby_retire(m);
    } else {
        lq_sq(pos, next);
        class49_trail(m, class49_tuning());
    }
    ee_stack_free(0xd0);
}


// It starts its sound once the camera comes within 15; while flying (state 0) it trails, aims at its target (pvars +0x10)
// turned by its random turn (+0x28 / +0x2c; the pitch within 70 degrees) at its speed (+0x24), its velocity (+0) eased
// toward that over its life (+0x20 of +0x30) and its heading along it; a line to its next place that meets something, or its
// target within a step, bursts it (a contact blast when the hero's +0x13f4 is 2), else it moves on.
// orig: L16:0x002a0a98 L18:0x002a7b90
void class50_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10;
    if (lw(pv + 0x34) == 0 && vec3_distance(V(pos), V(OV(0x00166dc0))) < 15.0f) {
        sw(pv + 0x34, 1);
        moby_sound_play(5, 0, m, 0x54c);
    }
    if (lbu(m + 0x20) != 0) return;
    class49_trail(m, on_level(VELDIN_FINALE) ? L18_ADDR(0x001613e8) : L16_ADDR(0x001613e8));
    const uint32_t sp = ee_stack_alloc(0xe0);
    const uint32_t aim = sp + 0x20, next = sp + 0x30, to = sp + 0x40, eased = sp + 0x50;
    vec3_sub(V(to), V(pv + 0x10), V(pos));
    float yaw = atan2_f(lwc1(to), lwc1(to + 4));
    float pitch = atan2_f(vec2_length(V(to)), lwc1(to + 8));
    yaw = angle_add(yaw, lwc1(pv + 0x28));
    pitch = angle_add(pitch, lwc1(pv + 0x2c));
    const float most = u2f(0x3f9c61aau);  // (70 degrees)
    if (most < pitch) pitch = most;
    else if (pitch < u2f(0xbf9c61aau)) pitch = u2f(0xbf9c61aau);
    vec_from_spherical_v(V(aim), lwc1(pv + 0x24), yaw, pitch);
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x20))) == 0) {
        const float t = fpu_div(int_to_float(lw(pv + 0x20)), int_to_float(lw(pv + 0x30)));
        vec3_lerp(V(eased), V(aim), V(pv), t);
        vec3_add(V(next), V(pos), V(eased));
        swc1(m + 0x48, atan2_f(lwc1(eased), lwc1(eased + 4)));
        swc1(m + 0x44, -atan2_f(vec2_length(V(eased)), lwc1(eased + 8)));
    } else {
        vec3_add(V(next), V(pos), V(aim));
        swc1(m + 0x48, yaw);
        swc1(m + 0x44, pitch);
    }
    if (segment_collide_v(V(pos), V(next), 0, m, nullptr) != 0 || vec3_length(V(to)) < lwc1(pv + 0x24)) {
        if (lbu(0x001413f4) == 2) moby_sphere_contact(m, pos, 3, 0, 1, 0, 2.0f, 2.0f, 1.0f);
        moby_sound_play(4, 0, m, 0x54c);
        explosion_effect(m, pv, pos, 0xa, 3, 0x10, -1, 1, 1, -1, 0, 4.0f, 4.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
        moby_retire(m);
    } else {
        lq_sq(pos, next);
    }
    ee_stack_free(0xe0);
}

}  // namespace rac1
