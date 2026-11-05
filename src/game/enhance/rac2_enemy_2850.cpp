// See rac2_enemies.h. RAC2's 2850, the arena's heavy enemy: a blue four-legged walker with a big arm, a red blade spinning
// on it (RAC2's class 2722, two of them it spawns and carries: "wheels" below, after what they do). It lands from its gate,
// wanders while it waits for its turn, roars, stalks Ratchet and slams him with its arm, the blades spinning faster as it
// swings. Written for RAC1's engine after RAC2's level
// code (its update 0x3f0798, set-up and hits 0x3f1298, its turn 0x3f16f0, activate 0x3f17b0, death 0x3f18e8, wandering spot
// 0x3f1a88, the wheels 0x3f4c88 / 0x3f4d68), read as the reference; RAC2's state numbers and sequences kept.
//
// What RAC2's 2850 does, and ours:
// - set up (0x3f1298, state 0): its approach angle 15..40 degrees either side (0x31e808), the wheels' spin 8 degrees a frame,
//   one in two mirrored (+0x34 bit 0x8000: RAC1's moby flag does the same), its two wheels spawned (class 2722 at 0.75 of
//   its size) on its attachment points 0 and 1; activated (0x3f17b0): its health from its pvars (2.0).
// - arriving (state 3): standing (sequence 2) for the wait its controller gives it, then walking (11) into the arena.
// - wandering (state 4, substate 2): walks (sequence 11; its mover 0.0022 / 0.067 a frame) to a spot 6..10 from the arena's
//   middle on its own side (0x3f1a88: the bearing from the middle to it, +-45 degrees), stands there with an idle (2, 3,
//   4, 6 or 7) now and then (after 300 frames, then every 0..100). With a turn to attack (its controller's count of its
//   class, 0x3fc0d0: four for the 2850s; every 4th frame the controller hands the 2506s' and 2850s' turns to the two of
//   them nearest Ratchet, 0x3fbcf8) it roars (state 5: sequence 1 or 5), then chases.
// - chasing (state 7): walks at Ratchet along its approach angle (sequence 11, 0.0044 / 0.133); within 7 it stalks (8,
//   top speed 0.033), farther than 15 it walks again; within 2.8 and facing him within 22 degrees it slams (state 8).
//   Ratchet 160 degrees off its facing: it turns round first (state 6, sequence 10, turning fast), then goes on.
// - slamming (state 8): sequence 12 from its frame 4; at its times 17..25 its wheels spin at 25 degrees a frame, at 19..25
//   (RAC2's 0x3146d0: the frame records' own times, not their indices: rac2::seq_time) three damage spheres on its attachment points 0, 1, 2 (0.35, 0.15, 0.15; its pvars' damage, 1). Over: when it hit him
//   it gloats (state 9, sequence 13), else it chases again.
// - hit (RAC2's 0x320d48 with its hit reaction 0x32bbf0: state 10, substate 5, its physics 0x32c938): its health less the
//   damage, knocked back and up; landed, it chases again. At none it blows up (0x3f18e8: the arena's explosion, debris)
//   and the controller counts it (0x3fc248). The floor's tiles and vents do not hurt it (RAC2 zeroes their damage; ours
//   hurt only Ratchet anyway).
// Ours vs RAC2's: RAC2's steering round the others (0x3f1ba8) and its path-finding round the arena's middle (0x32f608) are
// left out (it walks straight; RAC1's walker_move stops it at walls); the arms' trail during the slam (0x3f1608 / 0x2eefb0)
// is not drawn; its body's bump (0x3201e0 with kind 0x4000000 each frame, 1.3) is RAC1's collision with its class. Its feet
// probe 1 up (walker_move): with RAC1's 0.5 it stopped at the rim round the floor's middle. The wheels: RAC2 copies the
// attachment point's matrix into theirs (0x328500); ours RAC1's "keep the rows" flag with the point's rows turned by the
// spin, their size RAC2's (0.75 of their class's): rac2_compat's parts.
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

constexpr int32_t kRac2Class = 2850;
constexpr int32_t kWheelClass = 2041;      // (RAC2's 2722 converted: maktar.py RAC2_CLASSES)
constexpr float kPi = 3.14159265f, kDeg = kPi / 180.0f;
constexpr float kFloor = 500.0f;           // (the arena's floor)
constexpr float kCentreX = 500.0f, kCentreY = 601.55f;  // (the arena's middle: its controller's point 0x3fc098, the level's cuboid 50)
constexpr float kSpotRadius = 8.0f, kSpotSpread = 2.0f, kSpotSide = 0.7854f;  // (0x3f1a88: 8 +- 2, +-45 degrees)
constexpr float kSlamRange = 2.8f;         // (it slams within this, facing Ratchet within kSlamFacing)
constexpr float kSlamFacing = 0.3842f;
constexpr float kStalk = 7.0f, kWalkAgain = 15.0f;
constexpr float kTurnRound = 2.793f;       // (Ratchet this far off its facing: it turns round)
constexpr float kStop = 0.5f;              // (its mover stops this short)
constexpr float kGravity = 0.012f;         // (a frame, falling)
constexpr float kStepUp = 1.0f;            // (walker_move's probe height: it steps over the floor's rims)
constexpr float kSpinIdle = 8.0f, kSpinSlam = 25.0f;  // (its wheels: degrees a frame)
constexpr float kWheelScale = 0.75f;
constexpr float kHealth = 2.0f, kDamage = 1.0f;
constexpr uint32_t kIdleFirst = 300, kIdleNext = 100, kArriveFrames = 40;
constexpr float kKeepApart = 2.2f;         // (it steers round the other enemies nearer than this: rac2::steer_round)
constexpr int32_t kBolts = 12;             // (our share of bolts for one)

// RAC2's sequences
enum Seq : uint32_t {
    Roar1 = 1, Stand = 2, Idle3 = 3, Idle4 = 4, Roar5 = 5, Idle6 = 6, Idle7 = 7, Stalk = 8, TurnRound = 10, Walk = 11, Slam = 12,
    Gloat = 13
};
// RAC2's states (its numbers; 4 is its wandering, its substate 2; 10 knocked, RAC2's hit reaction's)
enum State : int32_t { SetUp = 0, Arriving = 3, Wandering = 4, Roaring = 5, Turning = 6, Chasing = 7, Slamming = 8, Gloating = 9, Knocked = 10 };

// Our pvars (a block of the stand-ins' pool), our own layout
namespace pv {
constexpr uint32_t kState = 0x00, kSince = 0x04, kEntered = 0x08, kIdleAt = 0x0c;
constexpr uint32_t kHit = 0x10;            // RAC1's hit state block (0x20 bytes): +0 the health
constexpr uint32_t kSpeed = 0x30, kTop = 0x34, kAccel = 0x38, kHeading = 0x3c;
constexpr uint32_t kSpot = 0x40;           // (its wandering spot: x, y)
constexpr uint32_t kAngle = 0x48, kVz = 0x4c, kAtSpot = 0x50, kBack = 0x54;  // (kBack: the state a turn round goes back to)
constexpr uint32_t kPush = 0x60;           // (a hit's push, x y, and frames left)
constexpr uint32_t kTurnW = 0x70, kTurnAcc = 0x74, kTurnTop = 0x78;  // (its turning: radians a frame, and its mover's two)
constexpr uint32_t kSpin = 0x80, kSpinRate = 0x84;  // (its wheels' angle, degrees a frame)
constexpr uint32_t kWheels = 0x88;         // (its two wheels: mobys)
constexpr uint32_t kLanded = 0x90, kHitHim = 0x94;
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
    if (trace()) std::printf("[2850] frame %u: %#x state %d -> %d\n", rac2::level_frame(), m, p.i(pv::kState), s);
    p.i(pv::kState, s);
    p.i(pv::kSince, static_cast<int32_t>(rac2::level_frame()));
    p.i(pv::kEntered, 0);
}
// orig: (none)
uint32_t frames(const Pv& p) { return rac2::level_frame() - static_cast<uint32_t>(p.i(pv::kSince)); }

// orig: (none) — its mover's settings (RAC2's 0x332710: acceleration, top speed, turning's acceleration and top; units a frame)
void mover(const Pv& p, float accel, float top, float turn_acc, float turn_top) {
    p.f(pv::kAccel, accel);
    p.f(pv::kTop, top);
    p.f(pv::kTurnAcc, turn_acc);
    p.f(pv::kTurnTop, turn_top);
}

// orig: (none) — its facing turned towards `yaw`: its turning speeds up by the mover's turning acceleration to its top,
// and is never more than what is left of the turn
void face(uint32_t m, const Pv& p, float yaw) {
    const float d = angle_to(yaw, p.f(pv::kHeading));
    const float w = std::min({p.f(pv::kTurnW) + p.f(pv::kTurnAcc), p.f(pv::kTurnTop), std::fabs(d)});
    p.f(pv::kTurnW, std::fabs(d) < 1e-3f ? 0.0f : w);
    p.f(pv::kHeading, p.f(pv::kHeading) + std::copysign(w, d));
    swc1(m + rac2::moby::kRot + 8, p.f(pv::kHeading));
}

// orig: (none) — a frame on its feet: towards `to` (RAC2's mover: sqrt(2 a d) braking; `moving` false: standing), the walls
// stop it; airborne it falls onto the floor. True on the frame it lands
bool walk_to(uint32_t m, const Pv& p, const Vec3& to, bool moving) {
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
        p.f(pv::kPush, p.f(pv::kPush) * 0.85f);
        p.f(pv::kPush + 4, p.f(pv::kPush + 4) * 0.85f);
        p.i(pv::kPush + 8, p.i(pv::kPush + 8) - 1);
    }
    const Vec3 away = rac2::steer_round(m, kKeepApart);  // (round the other enemies)
    step.x += away.x * 0.02f;
    step.y += away.y * 0.02f;
    walker_move_v(m, step, 0, kStepUp, 0.7f, 0.0f);
    const float ground = rac2::ground_under(m, kFloor - 30.0f);
    float z = lwc1(m + rac2::moby::kPos + 8), vz = p.f(pv::kVz) - kGravity;
    z += vz;
    bool landed = false;
    if (z <= ground) {
        landed = vz < -kGravity * 1.5f;
        z = ground;
        vz = 0.0f;
    }
    p.f(pv::kVz, vz);
    swc1(m + rac2::moby::kPos + 8, z);
    return landed;
}

// orig: (none) — a new wandering spot (RAC2's 0x3f1a88): 8 +- 2 from the arena's middle, on its own side +- 45 degrees
void new_spot(uint32_t m, const Pv& p) {
    const Vec3 at = rac2::moby_pos(m);
    const float a = std::atan2(at.y - kCentreY, at.x - kCentreX) + (rac2::rand_unit() * 2.0f - 1.0f) * kSpotSide;
    const float r = kSpotRadius + (rac2::rand_unit() * 2.0f - 1.0f) * kSpotSpread;
    p.f(pv::kSpot, kCentreX + r * std::cos(a));
    p.f(pv::kSpot + 4, kCentreY + r * std::sin(a));
    p.i(pv::kAtSpot, 0);
}

// orig: (none) — wheel k, while it is the one it spawned
uint32_t wheel_of(uint32_t m, const Pv& p, int k) {
    const uint32_t w = static_cast<uint32_t>(p.i(pv::kWheels + 4 * static_cast<uint32_t>(k)));
    return rac2::part_of(w, m) ? w : 0u;
}

// orig: (none) — its two wheels spawned (RAC2's 0x3f4c88: class 2722 at 0.75 of the class's size, on attachment points 0, 1)
void wheels_spawn(uint32_t m, const Pv& p) {
    for (int k = 0; k < 2; ++k) {
        const uint32_t w = rac2::part_spawn(m, kWheelClass, kWheelScale);
        p.i(pv::kWheels + 4 * static_cast<uint32_t>(k), static_cast<int32_t>(w));
        if (trace()) std::printf("[2850] frame %u: %#x wheel %d spawned: %#x\n", rac2::level_frame(), m, k, w);
    }
}

// orig: (none) — its wheels on its attachment points 0 and 1, spun about their own z by its spin (RAC2's 0x3f4d68: the
// point's matrix after a turn about z; a spark off one now and then, 1 in 6)
void wheels_place(uint32_t m, const Pv& p) {
    p.f(pv::kSpin, std::fmod(p.f(pv::kSpin) + p.f(pv::kSpinRate) * kDeg, 2.0f * kPi));
    for (int k = 0; k < 2; ++k) {
        const uint32_t w = wheel_of(m, p, k);
        if (w == 0) continue;
        rac2::part_place(w, m, static_cast<uint32_t>(k), p.f(pv::kSpin));
        if (rac2::rand_below(6) == 0) {  // (RAC2's 0x3f49d8: ours a small red glow drifting up)
            const Vec3 at = rac2::moby_pos(w);
            spawn_particle_3c_v(Vec4{at.x, at.y, at.z, 1.0f}, Vec4{0.0f, 0.0f, 0.02f, 0.0f}, 0x803040f0u, 12, 0, 0, 0.15f);
        }
    }
}

// orig: (none) — its wheels gone with it
void wheels_kill(uint32_t m, const Pv& p) {
    for (int k = 0; k < 2; ++k)
        if (const uint32_t w = wheel_of(m, p, k)) rac2::moby_kill(w);
    p.i(pv::kWheels, 0);
    p.i(pv::kWheels + 4, 0);
}

// orig: (none) — the slam's damage spheres (RAC2's 0x320328 on attachment points 0, 1, 2: 0.35, 0.15, 0.15) against Ratchet
// (his middle, his own 0.5 round it)
void slam_spheres(uint32_t m, const Pv& p) {
    Vec3 pt[3];
    rac2::joints_world(m, 0, 3, pt);
    const Vec3 hero = rac2::hero_pos();
    static constexpr float kRadius[3] = {0.35f, 0.15f, 0.15f};
    for (int k = 0; k < 3; ++k) {
        const float dx = hero.x - pt[k].x, dy = hero.y - pt[k].y, dz = hero.z + 0.6f - pt[k].z;
        if (std::sqrt(dx * dx + dy * dy + dz * dz) >= kRadius[k] + 0.6f) continue;
        const float h = p.f(pv::kHeading);
        rac2::hero_hit(m, Vec3{std::cos(h) * 1.5f, std::sin(h) * 1.5f, 1.0f}, kDamage);
        if (p.i(pv::kHitHim) == 0 && trace())
            std::printf("[2850] frame %u: %#x slams Ratchet (nanotech %d)\n", rac2::level_frame(), m, rac2::hero_health());
        p.i(pv::kHitHim, 1);
        return;
    }
}

// orig: (none) — it blows up (RAC2's 0x3f18e8 with the controller's 0x3fc248): the explosion, its bolts, its turn given back,
// its wheels gone
void die(uint32_t m, const Pv& p) {
    explosion_burst(m, m + rac2::moby::kPos, -1, 1.4f, 8.0f);
    const uint32_t near = dist2d(rac2::moby_pos(m), rac2::hero_pos()) < 7.0f ? 2u : 0u;
    bolts_drop(m, kBolts, kBolts, near, -1);
    rac2::attack_turn_give(m);
    wheels_kill(m, p);
    if (trace()) std::printf("[2850] frame %u: %#x blows up\n", rac2::level_frame(), m);
    rac2::moby_kill(m);
}

// orig: (none) — a hit taken (RAC1's hit record through its reaction): its health less the damage, knocked back and up
// (RAC2's reaction: state 10, substate 5); true: dead
bool hit_taken(uint32_t m, const Pv& p) {
    Vec3 from{};
    const float dmg = rac2::hit_receive(m, p.b + pv::kHit, &from);
    if (dmg <= 0.0f) return false;
    p.f(pv::kHit, p.f(pv::kHit) - dmg);
    if (trace()) std::printf("[2850] frame %u: %#x hit (damage %.2f, health %.2f)\n", rac2::level_frame(), m, static_cast<double>(dmg), static_cast<double>(p.f(pv::kHit)));
    if (p.f(pv::kHit) <= 0.0f) return true;
    Vec4 push{from.x, from.y, 0.0f, 0.0f};
    if (vec3_length(push) < 1e-4f) push = Vec4{-std::cos(p.f(pv::kHeading)), -std::sin(p.f(pv::kHeading)), 0.0f, 0.0f};
    vec3_set_length(push, push, 0.1f);
    p.f(pv::kPush, push.x);
    p.f(pv::kPush + 4, push.y);
    p.i(pv::kPush + 8, 16);
    p.f(pv::kVz, 0.12f);
    p.f(pv::kSpeed, 0.0f);
    rac2::attack_turn_give(m);
    set_state(m, p, Knocked);
    return false;
}

// orig: (none)
void update(uint32_t m) {
    const Pv p{lwu(m + rac2::moby::kPvars)};
    if (p.b == 0 || static_cast<int8_t>(lbu(m + rac2::moby::kState)) < 0) return;
    if (p.i(pv::kState) == SetUp) {
        for (uint32_t o = 0; o < 0x20; o += 4) sw(p.b + pv::kHit + o, 0);
        sh(p.b + pv::kHit + 0x14, 0xffff);
        sh(p.b + pv::kHit + 0x16, 0xffff);
        p.f(pv::kHit, kHealth);
        const float a = (15.0f + rac2::rand_unit() * 25.0f) * kDeg;
        p.f(pv::kAngle, rac2::rand_below(2) == 0 ? a : -a);
        p.f(pv::kHeading, lwc1(m + rac2::moby::kRot + 8));
        p.f(pv::kSpeed, 0.0f);
        p.f(pv::kVz, 0.0f);
        p.f(pv::kTurnW, 0.0f);
        p.i(pv::kPush + 8, 0);
        p.f(pv::kSpin, 0.0f);
        p.f(pv::kSpinRate, kSpinIdle);
        p.i(pv::kHitHim, 0);
        if (rac2::rand_below(2) == 1) sh(m + rac2::moby::kFlags, lhu(m + rac2::moby::kFlags) | 0x8000u);  // (mirrored)
        if (lwu(m + 0x94) != 0) sh(m + rac2::moby::kFlags, lhu(m + rac2::moby::kFlags) | 0x4000u);  // (weapons reach it)
        wheels_spawn(m, p);
        rac2::seq_play(m, Stand, 1);
        mover(p, 0.00222f, 0.0667f, 0.001164f, 0.1047f);
        set_state(m, p, Arriving);
    }
    const bool entering = p.i(pv::kEntered) == 0;
    p.i(pv::kEntered, 1);
    const Vec3 at = rac2::moby_pos(m), hero = rac2::hero_pos();
    const float to_hero = dist2d(at, hero);
    const float bearing = std::atan2(hero.y - at.y, hero.x - at.x);
    rac2::test_hit(m);
    if (hit_taken(m, p)) {
        die(m, p);
        return;
    }
    // its turn to attack, asked for each frame (RAC2's 0x3f16f0); lost while chasing: wandering again (RAC2: substate 4 -> 2)
    const int32_t s = p.i(pv::kState);
    const bool turn = s >= Wandering && s != Knocked && rac2::attack_turn_take(kRac2Class, m);
    if (!turn && (s == Chasing || s == Roaring || (s == Turning && p.i(pv::kBack) == Chasing))) {
        rac2::seq_play(m, rac2::rand_below(2) == 0 ? Stand : Idle3, 10);
        set_state(m, p, Wandering);
    }
    // Ratchet well behind it while it goes somewhere: it turns round first (0x3f1b70: state 6, then back)
    auto turn_round = [&](float yaw) {
        if (std::fabs(angle_to(yaw, p.f(pv::kHeading))) <= kTurnRound) return false;
        p.i(pv::kBack, p.i(pv::kState));
        set_state(m, p, Turning);
        return true;
    };
    switch (p.i(pv::kState)) {
    case Arriving:
        walk_to(m, p, at, false);
        if (frames(p) >= kArriveFrames) set_state(m, p, Wandering);
        break;
    case Wandering: {
        if (entering) {
            mover(p, 0.00222f, 0.0667f, 0.001164f, 0.1047f);
            rac2::seq_play(m, Walk, 10);
            new_spot(m, p);
            p.i(pv::kIdleAt, static_cast<int32_t>(rac2::level_frame() + kIdleFirst));
        }
        const Vec3 spot{p.f(pv::kSpot), p.f(pv::kSpot + 4), at.z};
        if (p.i(pv::kAtSpot) == 0 && dist2d(at, spot) < kStop + 0.1f) {
            p.i(pv::kAtSpot, 1);
            rac2::seq_play(m, Stand, 10);
        }
        walk_to(m, p, spot, p.i(pv::kAtSpot) == 0);
        if (p.i(pv::kAtSpot) != 0 && rac2::level_frame() >= static_cast<uint32_t>(p.i(pv::kIdleAt))) {
            static constexpr uint32_t kIdles[] = {Stand, Idle3, Idle4, Idle6, Idle7};
            rac2::seq_play(m, kIdles[rac2::rand_below(5)], 10);
            p.i(pv::kIdleAt, static_cast<int32_t>(rac2::level_frame()) + rac2::rand_below(static_cast<int32_t>(kIdleNext)) + 60);
            if (rac2::rand_below(3) == 0) {  // (and on to another spot)
                new_spot(m, p);
                rac2::seq_play(m, Walk, 10);
            }
        }
        if (turn) {
            rac2::seq_play(m, rac2::rand_below(2) == 0 ? Roar1 : Roar5, 10);
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
        if (entering) {
            rac2::seq_play(m, TurnRound, 10);
            mover(p, 0.00444f, 0.1333f, 0.00698f, 0.8378f);
            p.f(pv::kTurnW, 0.0f);
        }
        walk_to(m, p, at, false);
        face(m, p, bearing);
        if (frames(p) > 2 && rac2::seq_ended(m)) {
            mover(p, 0.00444f, 0.1333f, 0.001164f, 0.1047f);
            set_state(m, p, p.i(pv::kBack) == Wandering ? Wandering : Chasing);
        }
        break;
    case Chasing: {
        if (entering) {
            rac2::seq_play(m, Walk, 15);
            mover(p, 0.00444f, 0.1333f, 0.001164f, 0.1047f);
        }
        if (rac2::seq_now(m) == Stalk && to_hero > kWalkAgain) {
            rac2::seq_play(m, Walk, 4);
            mover(p, 0.00444f, 0.1333f, 0.001164f, 0.1047f);
        } else if (rac2::seq_now(m) == Walk && to_hero < kStalk) {
            rac2::seq_play(m, Stalk, 4);
            mover(p, 0.00444f, 0.0333f, 0.001164f, 0.1047f);
        }
        if (to_hero < kSlamRange) {
            walk_to(m, p, at, false);
            face(m, p, bearing);
            if (std::fabs(angle_to(bearing, p.f(pv::kHeading))) < kSlamFacing) {
                set_state(m, p, Slamming);
                rac2::seq_play(m, Slam, 3, 4);
                p.i(pv::kHitHim, 0);
                break;
            }
        } else {  // (towards him along its approach angle: RAC2's 0x331518)
            const float c = std::cos(p.f(pv::kAngle)), sn = std::sin(p.f(pv::kAngle)), dx = hero.x - at.x, dy = hero.y - at.y;
            const float k = std::min(1.0f, std::max(0.0f, (to_hero - kSlamRange) / 6.0f));  // (straight at him at the end)
            const float ax = dx * c - dy * sn, ay = dx * sn + dy * c;
            walk_to(m, p, Vec3{at.x + ax * k + dx * (1.0f - k), at.y + ay * k + dy * (1.0f - k), at.z}, true);
        }
        turn_round(bearing);
        break;
    }
    case Slamming: {
        walk_to(m, p, at, false);
        face(m, p, bearing);
        const float f = rac2::seq_time(m);  // (RAC2's 0x3146d0: the sequence's own time, not its frame index)
        const bool swinging = rac2::seq_now(m) == Slam && f >= 17.0f && f <= 25.0f;
        p.f(pv::kSpinRate, swinging ? kSpinSlam : kSpinIdle);
        if (rac2::seq_now(m) == Slam && f >= 19.0f && f <= 25.0f) slam_spheres(m, p);
        if (frames(p) > 3 && rac2::seq_ended(m)) {
            p.f(pv::kSpinRate, kSpinIdle);
            if (p.i(pv::kHitHim) != 0) {
                set_state(m, p, Gloating);
                rac2::seq_play(m, Gloat, 7);
            } else {
                set_state(m, p, Chasing);
            }
        }
        break;
    }
    case Gloating:
        walk_to(m, p, at, false);
        if (frames(p) > 3 && rac2::seq_ended(m)) set_state(m, p, Chasing);
        break;
    case Knocked:
        p.f(pv::kSpinRate, kSpinIdle);
        if ((walk_to(m, p, at, false) || frames(p) > 90) && frames(p) > 4) {
            p.i(pv::kLanded, 1);
            set_state(m, p, Chasing);
        }
        break;
    default: set_state(m, p, Wandering);
    }
    wheels_place(m, p);
    if (trace() && (rac2::level_frame() % 60) == 0)
        std::printf("[2850] frame %u: %#x state %d seq %u time %.1f at %.1f %.1f %.1f, Ratchet %.1f away, health %.2f, turn %d\n", rac2::level_frame(), m,
                    p.i(pv::kState), rac2::seq_now(m), static_cast<double>(rac2::seq_time(m)), static_cast<double>(at.x), static_cast<double>(at.y),
                    static_cast<double>(at.z), static_cast<double>(to_hero), static_cast<double>(p.f(pv::kHit)), turn ? 1 : 0);
}

}  // namespace

// orig: (none) — see rac2_enemies.h
void rac2_enemy_2850_update(uint32_t m) { update(m); }

}  // namespace rac1
