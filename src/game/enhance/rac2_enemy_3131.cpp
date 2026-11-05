// See rac2_enemies.h. RAC2's 3131, the arena's mid-sized enemy: a walking robot with a flamethrower. It lands from its gate,
// wanders the floor while it waits for its turn, roars, then walks at Ratchet and burns him from close by. Written for
// RAC1's engine after RAC2's level code (its update 0x40a4a8, set-up and hits 0x40b0f8, activate 0x40b868, death
// 0x40b980), read as the reference; RAC2's state numbers and sequences kept. 3134 (rac2_enemy_3134.cpp) shares half of
// its code with it in RAC2 (the turns, the activate, the death), not its body: 3134 flies, 3131 walks.
//
// What RAC2's 3131 does, and ours:
// - set up (0x40b0f8, state 0): its class's size; its health from its pvars (2.0); its approach angle -40..40 degrees.
// - arriving (state 3, from the activate 0x40b868): standing (sequence 2), then walking in (sequence 10).
// - wandering (state 4, substate 2): walks (sequence 10; its mover 0.0044 / 0.133 a frame, turning 0.105) to a spot its
//   controller gives it (0x40bab8: ours a random spot on the floor), stands there with an idle of its own (sequences 2,
//   3, 5 or 6) now and then (after 300 frames, then every 0..100); it asks for its turn to attack (the controller's
//   count of its class, 0x3fc0d0); with one it roars (state 5: sequence 1 or 4) and then chases.
// - chasing (state 7): walks at Ratchet along its approach angle (to 2.8 of him); within 15 of him and facing him within
//   90 degrees it fires (sequence 13, its mover slowed to 0.033): a jet of flames from its attachment point 4 along its facing, and
//   while he is within 3, three damage spheres of 0.3 along the jet (at the point, 0.85 and 1.6 out: its pvars'
//   damage, 1). Ratchet behind it (160 degrees off), it turns round first (state 6, sequence 9).
// - hit (RAC2's 0x320d48 / its mover's hit reaction 0x32bbf0; ours RAC1's 0x25a420 / 0x25a478 with the enemies' mask): its
//   health less the damage, a short push; at none it blows up (0x40b980: the arena's explosion, a flash) and the
//   controller counts it (0x3fc248).
// - falling (substate 4: RAC2's physics 0x32c938): it falls to the floor; landing in a pit (the death height) it is gone.
// Ours vs RAC2's: RAC2's mover's obstacle steering and its path-finding round the arena's middle (0x32f608, the inner
// ring its pvars name) are left out (it walks straight; RAC1's walker_move stops it at walls); its death sound is the
// small enemy's (RAC2 plays 3134's there; ours the explosion's own); the flames are RAC1's particles.
#include <host/rac2_enemies.h>
#include <host/rac2_compat.h>

#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace rac1 {

namespace {

using rac2::Vec3;

constexpr int32_t kRac2Class = 3131;
constexpr float kPi = 3.14159265f;
constexpr float kFloor = 500.0f;           // (the arena's floor, as for 3134)
constexpr float kCentreX = 500.0f, kCentreY = 600.0f, kWander = 12.0f;  // (where it wanders: our controller's spots)
constexpr float kFireRange = 15.0f;        // (it fires within this, facing Ratchet within 90 degrees)
constexpr float kCloseIn = 2.8f;           // (it walks no closer)
constexpr float kBurnRange = 3.0f;         // (the flames hurt within this)
constexpr float kBurnRadius = 0.3f;        // (its three damage spheres)
constexpr float kTurnRound = 2.79f;        // (Ratchet this far off its facing: it turns round)
constexpr float kTurnRate = 0.105f;        // (radians a frame: its mover's +0x1c)
constexpr float kStop = 0.5f;              // (its mover stops this short)
constexpr float kGravity = 0.012f;         // (a frame, falling)
constexpr uint32_t kIdleFirst = 300, kIdleNext = 100, kArriveFrames = 40;
constexpr int kTurns = 3;                  // (of its class attacking at once: ours, as for 3134)
constexpr int32_t kBolts = 8;              // (our share of bolts for one)
constexpr uint32_t kNozzle = 4;            // (its flamethrower's attachment point)

// RAC2's sequences
enum Seq : uint32_t { Roar1 = 1, Stand = 2, Idle3 = 3, Roar4 = 4, Idle5 = 5, Idle6 = 6, TurnRound = 9, Walk = 10, Fire = 13 };
// RAC2's states (its numbers; 4 is its wandering, its substate 2)
enum State : int32_t { SetUp = 0, Arriving = 3, Wandering = 4, Roaring = 5, Turning = 6, Chasing = 7 };

// Our pvars (a block of the stand-ins' pool), our own layout
namespace pv {
constexpr uint32_t kState = 0x00, kSince = 0x04, kEntered = 0x08, kIdleAt = 0x0c;
constexpr uint32_t kHit = 0x10;            // RAC1's hit state block (0x20 bytes): +0 the health
constexpr uint32_t kSpeed = 0x30, kTop = 0x34, kAccel = 0x38, kHeading = 0x3c;
constexpr uint32_t kSpot = 0x40;           // (its wandering spot: x, y)
constexpr uint32_t kAngle = 0x48, kVz = 0x4c, kAtSpot = 0x50, kHeroWas = 0x54;
constexpr uint32_t kPush = 0x60;           // (a hit's push, x y, and frames left)
}  // namespace pv

struct Pv {
    uint32_t b;
    int32_t i(uint32_t o) const { return lw(b + o); }
    void i(uint32_t o, int32_t v) const { sw(b + o, static_cast<uint32_t>(v)); }
    float f(uint32_t o) const { return lwc1(b + o); }
    void f(uint32_t o, float v) const { swc1(b + o, v); }
};

// orig: (none) — RAC1_CUSTOM_TRACE
bool trace() {
    static const bool t = std::getenv("RAC1_CUSTOM_TRACE") != nullptr;
    return t;
}

// orig: (none)
float dist2d(const Vec3& a, const Vec3& b) { return std::hypot(a.x - b.x, a.y - b.y); }
// orig: (none) — a - b as an angle in -pi .. pi
float angle_to(float a, float b) {
    float d = std::fmod(a - b, 2.0f * kPi);
    if (d > kPi) d -= 2.0f * kPi;
    if (d < -kPi) d += 2.0f * kPi;
    return d;
}

// orig: (none) — a new state (its first frame next)
void set_state(uint32_t m, const Pv& p, int32_t s) {
    if (trace()) std::printf("[3131] frame %u: %#x state %d -> %d\n", rac2::level_frame(), m, p.i(pv::kState), s);
    p.i(pv::kState, s);
    p.i(pv::kSince, static_cast<int32_t>(rac2::level_frame()));
    p.i(pv::kEntered, 0);
}
// orig: (none)
uint32_t frames(const Pv& p) { return rac2::level_frame() - static_cast<uint32_t>(p.i(pv::kSince)); }

// orig: (none) — its mover's speeds (RAC2's 0x332710: acceleration, top speed, units a frame)
void mover(const Pv& p, float accel, float top) {
    p.f(pv::kAccel, accel);
    p.f(pv::kTop, top);
}

// orig: (none) — its facing turned towards `yaw`, at most kTurnRate
void face(uint32_t m, const Pv& p, float yaw) {
    p.f(pv::kHeading, p.f(pv::kHeading) + std::clamp(angle_to(yaw, p.f(pv::kHeading)), -kTurnRate, kTurnRate));
    swc1(m + rac2::moby::kRot + 8, p.f(pv::kHeading));
}

// orig: (none) — a frame on its feet: towards `to` (its mover, as 3134's: sqrt(2 a d) braking; `to` itself: standing),
// the walls stop it, and it falls onto the floor
void walk_to(uint32_t m, const Pv& p, const Vec3& to, bool moving) {
    const Vec3 at = rac2::moby_pos(m);
    const float d = moving ? dist2d(at, to) : 0.0f;
    const float acc = p.f(pv::kAccel);
    const float want = std::min(p.f(pv::kTop), std::sqrt(2.0f * acc * std::max(0.0f, d - kStop)));
    float speed = p.f(pv::kSpeed);
    speed = speed < want ? std::min(want, speed + acc) : std::max(want, speed - acc);
    p.f(pv::kSpeed, speed);
    if (d > 1e-3f) face(m, p, std::atan2(to.y - at.y, to.x - at.x));
    const float h = p.f(pv::kHeading);
    Vec4 step{std::cos(h) * speed, std::sin(h) * speed, 0.0f, 0.0f};
    if (p.i(pv::kPush + 8) > 0) {  // (a hit's push, fading)
        step.x += p.f(pv::kPush);
        step.y += p.f(pv::kPush + 4);
        p.f(pv::kPush, p.f(pv::kPush) * 0.8f);
        p.f(pv::kPush + 4, p.f(pv::kPush + 4) * 0.8f);
        p.i(pv::kPush + 8, p.i(pv::kPush + 8) - 1);
    }
    walker_move_v(m, step, 0, 0.5f, 0.5f, 0.0f);
    // (falling onto the floor; RAC2's physics 0x32c938 for its substate 4)
    const float ground = rac2::ground_under(m, kFloor - 30.0f);
    float z = lwc1(m + rac2::moby::kPos + 8), vz = p.f(pv::kVz) - kGravity;
    z += vz;
    if (z <= ground) {
        z = ground;
        vz = 0.0f;
    }
    p.f(pv::kVz, vz);
    swc1(m + rac2::moby::kPos + 8, z);
}

// orig: (none) — a new wandering spot (RAC2's controller hands one out, 0x40bab8; ours a random one on the floor)
void new_spot(const Pv& p) {
    const float a = rac2::rand_unit() * 2.0f * kPi, r = 3.0f + rac2::rand_unit() * (kWander - 3.0f);
    p.f(pv::kSpot, kCentreX + r * std::cos(a));
    p.f(pv::kSpot + 4, kCentreY + r * std::sin(a));
    p.i(pv::kAtSpot, 0);
}

// orig: (none) — the flames (RAC2: from attachment point 4 along its facing, the effect 0x344710; ours RAC1's glows) and their three
// damage spheres while Ratchet is within kBurnRange
void flames(uint32_t m, const Pv& p, float to_hero) {
    Vec3 nozzle;
    rac2::joints_world(m, kNozzle, 1, &nozzle);
    const float h = p.f(pv::kHeading), cx = std::cos(h), sy = std::sin(h);
    for (int n = 0; n < 2; ++n) {
        const float out = rac2::rand_unit() * 0.6f;
        const Vec4 at{nozzle.x + cx * out, nozzle.y + sy * out, nozzle.z - 0.1f, 1.0f};
        const Vec4 v{cx * (0.08f + rac2::rand_unit() * 0.05f), sy * (0.08f + rac2::rand_unit() * 0.05f), 0.005f, 0.0f};
        spawn_particle_3c_v(at, v, 0x802070f0u, 22, 0, 0, 0.6f + rac2::rand_unit() * 0.5f);
    }
    if (to_hero >= kBurnRange) return;
    const Vec3 hero = rac2::hero_pos();
    for (float out : {0.0f, 0.85f, 1.6f}) {
        const Vec3 c{nozzle.x + cx * out, nozzle.y + sy * out, nozzle.z - 0.1f};
        const float dz = hero.z + 0.6f - c.z;  // (Ratchet's middle)
        if (std::sqrt((hero.x - c.x) * (hero.x - c.x) + (hero.y - c.y) * (hero.y - c.y) + dz * dz) >= kBurnRadius + 0.5f) continue;
        rac2::hero_hit(m, Vec3{cx, sy, 0.3f}, 1.0f);
        if (trace() && rac2::level_frame() % 10 == 0)
            std::printf("[3131] frame %u: %#x burns Ratchet (nanotech %d)\n", rac2::level_frame(), m, rac2::hero_health());
        return;
    }
}

// orig: (none) — it blows up (RAC2's 0x40b980 with the controller's 0x3fc248): the explosion, its bolts, its turn given back
void die(uint32_t m) {
    explosion_burst(m, m + rac2::moby::kPos, -1, 1.0f, 6.0f);
    const uint32_t near = dist2d(rac2::moby_pos(m), rac2::hero_pos()) < 7.0f ? 2u : 0u;
    bolts_drop(m, kBolts, kBolts, near, -1);
    rac2::attack_turn_give(m);
    if (trace()) std::printf("[3131] frame %u: %#x blows up\n", rac2::level_frame(), m);
    rac2::moby_kill(m);
}

// orig: (none) — a hit taken (RAC1's hit record through its reaction): its health less the damage, pushed; true: dead
bool hit_taken(uint32_t m, const Pv& p) {
    Vec3 from{};
    const float dmg = rac2::hit_receive(m, p.b + pv::kHit, &from);
    if (dmg <= 0.0f) return false;
    p.f(pv::kHit, p.f(pv::kHit) - dmg);
    Vec4 push{from.x, from.y, 0.0f, 0.0f};
    if (vec3_length(push) > 1e-4f) {
        vec3_set_length(push, push, 0.12f);
        p.f(pv::kPush, push.x);
        p.f(pv::kPush + 4, push.y);
        p.i(pv::kPush + 8, 12);
    }
    if (trace()) std::printf("[3131] frame %u: %#x hit (damage %.2f, health %.2f)\n", rac2::level_frame(), m, static_cast<double>(dmg), static_cast<double>(p.f(pv::kHit)));
    return p.f(pv::kHit) <= 0.0f;
}

// orig: (none)
void update(uint32_t m) {
    const Pv p{lwu(m + rac2::moby::kPvars)};
    if (p.b == 0 || static_cast<int8_t>(lbu(m + rac2::moby::kState)) < 0) return;
    if (p.i(pv::kState) == SetUp) {
        for (uint32_t o = 0; o < 0x20; o += 4) sw(p.b + pv::kHit + o, 0);
        sh(p.b + pv::kHit + 0x14, 0xffff);
        sh(p.b + pv::kHit + 0x16, 0xffff);
        p.f(pv::kHit, 2.0f);
        p.f(pv::kAngle, (rac2::rand_unit() * 80.0f - 40.0f) * kPi / 180.0f);
        p.f(pv::kHeading, lwc1(m + rac2::moby::kRot + 8));
        p.f(pv::kSpeed, 0.0f);
        p.f(pv::kVz, 0.0f);
        p.i(pv::kPush + 8, 0);
        if (lwu(m + 0x94) != 0) sh(m + rac2::moby::kFlags, lhu(m + rac2::moby::kFlags) | 0x4000u);  // (weapons reach it)
        rac2::seq_play(m, Stand, 1);
        mover(p, 0.00444f, 0.133f);
        new_spot(p);
        set_state(m, p, Arriving);
    }
    const bool entering = p.i(pv::kEntered) == 0;
    p.i(pv::kEntered, 1);
    const Vec3 at = rac2::moby_pos(m), hero = rac2::hero_pos();
    const float to_hero = dist2d(at, hero);
    const float bearing = std::atan2(hero.y - at.y, hero.x - at.x);
    rac2::test_hit(m);
    if (hit_taken(m, p)) {
        die(m);
        return;
    }
    // its turn to attack, asked for each frame (RAC2's 0x40cf68 as 3134's); lost while chasing: wandering again
    const int32_t s = p.i(pv::kState);
    const bool turn = s >= Wandering && rac2::attack_turn_take(kRac2Class, m, kTurns);
    if (!turn && (s == Chasing || s == Turning)) set_state(m, p, Wandering);
    switch (p.i(pv::kState)) {
    case Arriving:
        walk_to(m, p, at, false);
        if (frames(p) >= kArriveFrames) set_state(m, p, Wandering);
        break;
    case Wandering: {
        if (entering) {
            mover(p, 0.00444f, 0.133f);
            rac2::seq_play(m, Walk, 10);
            p.i(pv::kIdleAt, static_cast<int32_t>(rac2::level_frame() + kIdleFirst));
        }
        const Vec3 spot{p.f(pv::kSpot), p.f(pv::kSpot + 4), at.z};
        if (p.i(pv::kAtSpot) == 0 && dist2d(at, spot) < kStop + 0.1f) {
            p.i(pv::kAtSpot, 1);
            rac2::seq_play(m, Stand, 10);
        }
        walk_to(m, p, spot, p.i(pv::kAtSpot) == 0);
        if (p.i(pv::kAtSpot) != 0 && rac2::level_frame() >= static_cast<uint32_t>(p.i(pv::kIdleAt))) {
            static constexpr uint32_t kIdles[] = {Stand, Idle3, Idle5, Idle6};
            rac2::seq_play(m, kIdles[rac2::rand_below(4)], 10);
            p.i(pv::kIdleAt, static_cast<int32_t>(rac2::level_frame()) + rac2::rand_below(static_cast<int32_t>(kIdleNext)) + 60);
            if (rac2::rand_below(3) == 0) {  // (and on to another spot)
                new_spot(p);
                rac2::seq_play(m, Walk, 10);
            }
        }
        if (turn) {
            rac2::seq_play(m, rac2::rand_below(2) == 0 ? Roar1 : Roar4, 10);
            set_state(m, p, Roaring);
        }
        break;
    }
    case Roaring:
        walk_to(m, p, at, false);
        face(m, p, bearing);
        if (frames(p) > 2 && rac2::seq_ended(m)) set_state(m, p, Chasing);
        break;
    case Turning:
        if (entering) rac2::seq_play(m, TurnRound, 10);
        walk_to(m, p, at, false);
        face(m, p, bearing);
        if (frames(p) > 2 && rac2::seq_ended(m)) set_state(m, p, Chasing);
        break;
    case Chasing: {
        if (entering) {
            mover(p, 0.00444f, 0.133f);
            rac2::seq_play(m, Walk, 15);
        }
        const bool firing = rac2::seq_now(m) == Fire;
        if (firing && rac2::seq_ended(m)) rac2::seq_play(m, Walk, 10);  // (a burst over: walking again, then another)
        if (!firing && to_hero < kFireRange && std::fabs(angle_to(bearing, p.f(pv::kHeading))) < kPi / 2.0f) {
            rac2::seq_play(m, Fire, 10);
            mover(p, 0.00111f, 0.0333f);
        } else if (!firing) {
            mover(p, 0.00444f, 0.133f);
        }
        if (to_hero > kCloseIn) {  // (towards him along its approach angle)
            const float c = std::cos(p.f(pv::kAngle)), sn = std::sin(p.f(pv::kAngle)), dx = hero.x - at.x, dy = hero.y - at.y;
            walk_to(m, p, Vec3{at.x + dx * c - dy * sn, at.y + dx * sn + dy * c, at.z}, true);
        } else {
            walk_to(m, p, at, false);
            face(m, p, bearing);
        }
        if (rac2::seq_now(m) == Fire) flames(m, p, to_hero);
        if (std::fabs(angle_to(bearing, p.f(pv::kHeading))) > kTurnRound) set_state(m, p, Turning);
        break;
    }
    default: set_state(m, p, Wandering);
    }
    if (trace() && (rac2::level_frame() % 60) == 0)
        std::printf("[3131] frame %u: %#x state %d seq %u at %.1f %.1f %.1f, Ratchet %.1f away, health %.2f, turn %d\n", rac2::level_frame(), m,
                    p.i(pv::kState), rac2::seq_now(m), static_cast<double>(at.x), static_cast<double>(at.y), static_cast<double>(at.z),
                    static_cast<double>(to_hero), static_cast<double>(p.f(pv::kHit)), turn ? 1 : 0);
}

}  // namespace

// orig: (none) — see rac2_enemies.h
void rac2_enemy_3131_update(uint32_t m) { update(m); }

}  // namespace rac1
