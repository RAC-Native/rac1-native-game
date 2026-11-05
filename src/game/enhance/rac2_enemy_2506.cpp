// See rac2_enemies.h. RAC2's 2506, the arena's gladiator: an alien with a spiked flail and a round shield (RAC2's class 2520, which it
// spawns and carries on its attachment point 2). It lands from its gate, wanders while it waits for its turn, then comes at
// Ratchet guarded, squares up to him side-stepping as he moves round it, and swings the flail at him; its shield stops what hits it
// from the front while it stands guard. Hurt, it throws the shield off and fights on without it. Written for RAC1's engine
// after RAC2's level code (its update 0x3d0408, set-up and hits 0x3d10f0, its turn and partner 0x3d1998, the cut 0x3d1c38,
// the shield 0x3d1e40 / 0x3d1ed0 / 0x3d1fa0, the block test 0x3d1880, death 0x3d2488 / 0x3d2390), read as the reference;
// RAC2's state numbers and sequences kept. (PLANET_PLAN.md called 2520 its shot: it is its shield, which 0x32b468 throws.)
//
// What RAC2's 2506 does, and ours:
// - set up (0x3d10f0, state 0): its approach angle 10..30 degrees either side, one in two mirrored; its shield spawned
//   (0x3d1e40: class 2520 on attachment point 2, placed there each frame, 0x3d1430); health 2.0 from its pvars.
// - arriving (state 3): standing (sequence 3) for its controller's wait, then into the arena.
// - wandering (substate 2: state 4 with its shield, 5 without, 0x3d14f8): walks (3) to a spot of its controller's (ours on
//   its side of the arena, as 2850's), stands there (0 with the shield, 1 without), now and then the other one (after 300
//   frames, then every 0..100). With a turn to attack (0x3fc0d0) it comes at Ratchet. (Without one RAC2 pairs it with the
//   nearest idle 2506 and the two spar, 0x3d1998 mode 2: not ported, ours wait.)
// - approaching (state 6, substate 3; 0x3d1588): guarded (sequence 2; its mover 0.0056 / 0.032 a frame) along its
//   approach angle; farther than 14 it runs (state 7, sequence 3, top 0.103) until within 4 or 450 frames; within 4 it
//   squares up (0x3d1610).
// - squaring up (state 10, substate 4: sequence 6, top 0.067): shuffles at Ratchet; when he is off to a side (0x3d1820:
//   his direction against its side axis, +-0.01) it side-steps round to him (states 8 / 9, sequences 4 / 5, turning 1
//   degree a frame) until he is in front again. Within 4 and facing him within 15 degrees (0x3d1698): one time in five it
//   taunts (state 12, sequence 7), else it cuts (state 18, sequence 13 with the shield; 15, sequence 10 without; both
//   from their frame 8).
// - cutting (its swing): at the sequence's times 19..31 (RAC2's 0x3146d0) the flail (its attachment points 0 to 1) is a
//   damage sphere where it passes nearest Ratchet (0.15 at the grip to 0.5 at the head) and another of 0.5 on its head (its pvars'
//   damage, 1); its sound 5 when it lands. Over: when it hit him it taunts, else it goes round again (0x3d14f8).
// - hit (0x3d10f0): with the shield, standing guard (wandering with it, squaring up, its shield cut: 0x3d1880) it blocks:
//   no damage, sparks off the shield (0x3d1fa0, then not for 60 frames), a stagger (state 19). Else its health less the
//   damage, knocked back and up (its hit reaction: state 20, substate 7, physics 0x32c938); landed with half its health
//   or less, it throws the shield off (0x3d1ed0: RAC2's 0x32b468, rac2_compat's part_throw). At none it falls (state 21,
//   substate 7, sequence 16), the shield thrown off, and after it lands (or 60 frames) it blows up (the arena's
//   explosion, debris; the controller counts it, 0x3fc248).
// Ours vs RAC2's: the sparring between waiting 2506s, its steering round the others and its path-finding round the
// arena's middle (0x32f608) are left out; side-stepping turns it on the spot (RAC2's sequences 4 / 5 carry it, assumed).
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

constexpr int32_t kRac2Class = 2506;
constexpr int32_t kShieldClass = 2042;     // (RAC2's 2520 converted: maktar.py RAC2_CLASSES)
constexpr uint32_t kShieldPoint = 2;       // (its attachment point: 0x3d1430)
constexpr float kPi = 3.14159265f, kDeg = kPi / 180.0f;
constexpr float kFloor = 500.0f;           // (the arena's floor)
constexpr float kCentreX = 500.0f, kCentreY = 600.0f;  // (the arena's middle)
constexpr float kSpotRadius = 8.0f, kSpotSpread = 2.0f, kSpotSide = 0.7854f;
constexpr float kClose = 4.0f, kRun = 14.0f, kCutFacing = 0.2618f, kSide = 0.01f;
constexpr float kStop = 0.5f;
constexpr float kGravity = 0.012f;
constexpr float kStepUp = 1.0f;            // (walker_move's probe height: it steps over the floor's rims, as 2850's)
constexpr float kHealth = 2.0f, kDamage = 1.0f;
constexpr uint32_t kIdleFirst = 300, kIdleNext = 100, kArriveFrames = 40, kRunMost = 450, kSparkWait = 60;
constexpr int kTurns = 3;                  // (of its class attacking at once: ours, as for the others)
constexpr int32_t kBolts = 10;             // (our share of bolts for one)

// RAC2's sequences
enum Seq : uint32_t {
    StandShield = 0, Stand = 1, Guarded = 2, Walk = 3, StepLeft = 4, StepRight = 5, Squared = 6, Taunt = 7, Cut = 10,
    ShieldCut = 13, Fall = 16
};
// RAC2's states (its numbers: 4 / 5 its wandering with / without the shield, substate 2)
enum State : int32_t {
    SetUp = 0, Arriving = 3, WanderShield = 4, Wander = 5, Approaching = 6, Running = 7, SideLeft = 8, SideRight = 9, Squaring = 10,
    Taunting = 12, Cutting = 15, ShieldCutting = 18, Blocked = 19, Knocked = 20, Dying = 21
};

// Our pvars (a block of the stand-ins' pool), our own layout
namespace pv {
constexpr uint32_t kState = 0x00, kSince = 0x04, kEntered = 0x08, kIdleAt = 0x0c;
constexpr uint32_t kHit = 0x10;            // RAC1's hit state block (0x20 bytes): +0 the health
constexpr uint32_t kSpeed = 0x30, kTop = 0x34, kAccel = 0x38, kHeading = 0x3c;
constexpr uint32_t kSpot = 0x40;
constexpr uint32_t kAngle = 0x48, kVz = 0x4c, kAtSpot = 0x50;
constexpr uint32_t kPush = 0x60;           // (a hit's push, x y, and frames left)
constexpr uint32_t kTurnW = 0x70, kTurnAcc = 0x74, kTurnTop = 0x78;
constexpr uint32_t kShield = 0x80, kSparkAt = 0x84, kHitHim = 0x88, kSideWas = 0x8c, kSound = 0x90;
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
    if (trace()) std::printf("[2506] frame %u: %#x state %d -> %d\n", rac2::level_frame(), m, p.i(pv::kState), s);
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

// orig: (none) — a new wandering spot (RAC2's controller hands one out; ours as 2850's: on its own side of the middle)
void new_spot(uint32_t m, const Pv& p) {
    const Vec3 at = rac2::moby_pos(m);
    const float a = std::atan2(at.y - kCentreY, at.x - kCentreX) + (rac2::rand_unit() * 2.0f - 1.0f) * kSpotSide;
    const float r = kSpotRadius + (rac2::rand_unit() * 2.0f - 1.0f) * kSpotSpread;
    p.f(pv::kSpot, kCentreX + r * std::cos(a));
    p.f(pv::kSpot + 4, kCentreY + r * std::sin(a));
    p.i(pv::kAtSpot, 0);
}

// orig: (none) — its shield, while it carries it
uint32_t shield_of(uint32_t m, const Pv& p) {
    const uint32_t s = static_cast<uint32_t>(p.i(pv::kShield));
    return rac2::part_of(s, m) ? s : 0u;
}

// orig: (none) — the shield thrown off (RAC2's 0x3d1ed0: up and a little away, 0x31ea58's random spread)
void shield_throw(uint32_t m, const Pv& p) {
    const uint32_t s = shield_of(m, p);
    if (s == 0) return;
    const float a = rac2::rand_unit() * 2.0f * kPi, out = 0.05f + rac2::rand_unit() * 0.05f;
    rac2::part_throw(s, Vec3{std::cos(a) * out, std::sin(a) * out, 0.15f + rac2::rand_unit() * 0.05f}, kFloor, 120);
    p.i(pv::kShield, 0);
    if (trace()) std::printf("[2506] frame %u: %#x throws its shield off\n", rac2::level_frame(), m);
}

// orig: (none) — Ratchet's side of it (RAC2's 0x3d1820: his direction against its side axis, the matrix's second row)
float side_of(uint32_t m, const Vec3& at, const Vec3& hero) {
    float dx = hero.x - at.x, dy = hero.y - at.y;
    const float l = std::hypot(dx, dy);
    if (l < 1e-4f) return 0.0f;
    dx /= l;
    dy /= l;
    return dx * lwc1(m + 0xd0) + dy * lwc1(m + 0xd4);
}

// orig: (none) — the swing's damage spheres (RAC2's 0x3d1c38): along the flail (attachment points 0 to 1) where it passes
// nearest Ratchet, 0.15 at the grip to 0.5 at the head, and 0.5 on the head; his middle, his own 0.6 round it
void cut_spheres(uint32_t m, const Pv& p, const Vec3& hero) {
    Vec3 b[2];
    rac2::joints_world(m, 0, 2, b);
    const Vec3 mid{hero.x, hero.y, hero.z + 0.6f};
    const Vec3 d{b[1].x - b[0].x, b[1].y - b[0].y, b[1].z - b[0].z};
    const float dd = d.x * d.x + d.y * d.y + d.z * d.z;
    float t = dd > 1e-6f ? ((mid.x - b[0].x) * d.x + (mid.y - b[0].y) * d.y + (mid.z - b[0].z) * d.z) / dd : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    const Vec3 near{b[0].x + d.x * t, b[0].y + d.y * t, b[0].z + d.z * t};
    auto within = [&](const Vec3& c, float r) {
        return std::sqrt((mid.x - c.x) * (mid.x - c.x) + (mid.y - c.y) * (mid.y - c.y) + (mid.z - c.z) * (mid.z - c.z)) < r + 0.6f;
    };
    if (!within(near, 0.15f + 0.35f * t) && !within(b[1], 0.5f)) return;
    const float h = p.f(pv::kHeading);
    rac2::hero_hit(m, Vec3{std::cos(h) * 1.2f, std::sin(h) * 1.2f, 0.8f}, kDamage);
    if (p.i(pv::kHitHim) == 0) {
        moby_sound_start(5, 0, m);  // (its sound 5, once a swing)
        if (trace()) std::printf("[2506] frame %u: %#x cuts Ratchet (nanotech %d)\n", rac2::level_frame(), m, rac2::hero_health());
    }
    p.i(pv::kHitHim, 1);
}

// orig: (none) — it blows up (RAC2's 0x3d2488 with the controller's 0x3fc248): the explosion, its bolts, its turn given back
void die(uint32_t m, const Pv& p) {
    shield_throw(m, p);
    explosion_burst(m, m + rac2::moby::kPos, -1, 1.2f, 7.0f);
    const uint32_t near = dist2d(rac2::moby_pos(m), rac2::hero_pos()) < 7.0f ? 2u : 0u;
    bolts_drop(m, kBolts, kBolts, near, -1);
    rac2::attack_turn_give(m);
    if (trace()) std::printf("[2506] frame %u: %#x blows up\n", rac2::level_frame(), m);
    rac2::moby_kill(m);
}

// orig: (none) — standing guard with its shield (RAC2's 0x3d1880: wandering with it, squaring up, its shield cut, staggered)
bool guarding(uint32_t m, const Pv& p) {
    if (shield_of(m, p) == 0) return false;
    switch (p.i(pv::kState)) {
    case WanderShield: case SideLeft: case SideRight: case Squaring: case ShieldCutting: case Blocked: return true;
    default: return false;
    }
}

// orig: (none) — a hit taken (RAC1's hit record through its reaction): blocked by its shield, or its health less the
// damage and knocked (RAC2's state 20, substate 7) or falling (21)
void hit_taken(uint32_t m, const Pv& p) {
    Vec3 from{};
    const float dmg = rac2::hit_receive(m, p.b + pv::kHit, &from);
    if (dmg <= 0.0f || p.i(pv::kState) == Dying) return;
    Vec4 push{from.x, from.y, 0.0f, 0.0f};
    if (vec3_length(push) < 1e-4f) push = Vec4{-std::cos(p.f(pv::kHeading)), -std::sin(p.f(pv::kHeading)), 0.0f, 0.0f};
    if (guarding(m, p)) {  // (the shield takes it: sparks, a stagger)
        if (rac2::level_frame() >= static_cast<uint32_t>(p.i(pv::kSparkAt))) {
            const Vec3 s = rac2::moby_pos(shield_of(m, p));
            for (int k = 0; k < 6; ++k)
                spawn_particle_3c_v(Vec4{s.x, s.y, s.z, 1.0f}, Vec4{(rac2::rand_unit() - 0.5f) * 0.1f, (rac2::rand_unit() - 0.5f) * 0.1f, rac2::rand_unit() * 0.08f, 0.0f},
                                    0x8040c0f0u, 14, 0, 0, 0.12f);
            p.i(pv::kSparkAt, static_cast<int32_t>(rac2::level_frame() + kSparkWait));
        }
        vec3_set_length(push, push, 0.05f);
        p.f(pv::kPush, push.x);
        p.f(pv::kPush + 4, push.y);
        p.i(pv::kPush + 8, 8);
        if (trace()) std::printf("[2506] frame %u: %#x blocks a hit with its shield\n", rac2::level_frame(), m);
        set_state(m, p, Blocked);
        return;
    }
    p.f(pv::kHit, p.f(pv::kHit) - dmg);
    if (trace()) std::printf("[2506] frame %u: %#x hit (damage %.2f, health %.2f)\n", rac2::level_frame(), m, static_cast<double>(dmg), static_cast<double>(p.f(pv::kHit)));
    vec3_set_length(push, push, 0.1f);
    p.f(pv::kPush, push.x);
    p.f(pv::kPush + 4, push.y);
    p.i(pv::kPush + 8, 16);
    p.f(pv::kVz, 0.12f);
    p.f(pv::kSpeed, 0.0f);
    rac2::attack_turn_give(m);
    if (p.f(pv::kHit) <= 0.0f) {
        rac2::seq_play(m, Fall, 1);
        shield_throw(m, p);
        set_state(m, p, Dying);
    } else {
        set_state(m, p, Knocked);
    }
}

// orig: (none) — back to wandering (RAC2's 0x3d14f8: state 4 with the shield, 5 without)
void wander(uint32_t m, const Pv& p) { set_state(m, p, shield_of(m, p) != 0 ? WanderShield : Wander); }

// orig: (none) — squaring up (RAC2's 0x3d1610: sequence 6, its mover 0.0056 / 0.067, turning 0.0035 / 0.42)
void square_up(uint32_t m, const Pv& p) {
    mover(p, 0.00556f, 0.0667f, 0.00349f, 0.419f);
    rac2::seq_play(m, Squared, 5);
    set_state(m, p, Squaring);
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
        const float a = (10.0f + rac2::rand_unit() * 20.0f) * kDeg;
        p.f(pv::kAngle, rac2::rand_below(2) == 0 ? a : -a);
        p.f(pv::kHeading, lwc1(m + rac2::moby::kRot + 8));
        p.f(pv::kSpeed, 0.0f);
        p.f(pv::kVz, 0.0f);
        p.f(pv::kTurnW, 0.0f);
        p.i(pv::kPush + 8, 0);
        p.i(pv::kHitHim, 0);
        p.i(pv::kSparkAt, 0);
        if (rac2::rand_below(2) == 0) sh(m + rac2::moby::kFlags, lhu(m + rac2::moby::kFlags) | 0x8000u);  // (mirrored)
        if (lwu(m + 0x94) != 0) sh(m + rac2::moby::kFlags, lhu(m + rac2::moby::kFlags) | 0x4000u);  // (weapons reach it)
        p.i(pv::kShield, static_cast<int32_t>(rac2::part_spawn(m, kShieldClass, 1.0f)));
        rac2::seq_play(m, Walk, 1);
        mover(p, 0.00556f, 0.1033f, 0.00349f, 0.419f);
        set_state(m, p, Arriving);
    }
    const bool entering = p.i(pv::kEntered) == 0;
    p.i(pv::kEntered, 1);
    const Vec3 at = rac2::moby_pos(m), hero = rac2::hero_pos();
    const float to_hero = dist2d(at, hero);
    const float bearing = std::atan2(hero.y - at.y, hero.x - at.x);
    rac2::test_hit(m);
    hit_taken(m, p);
    const int32_t s = p.i(pv::kState);
    const bool fighting = s >= Approaching && s <= ShieldCutting;
    const bool turn = (s == WanderShield || s == Wander || fighting) && rac2::attack_turn_take(kRac2Class, m, kTurns);
    if (!turn && fighting) wander(m, p);
    // within reach and facing him: a cut, or one time in five a taunt (RAC2's 0x3d1698)
    auto try_cut = [&]() {
        if (to_hero >= kClose || std::fabs(angle_to(bearing, p.f(pv::kHeading))) >= kCutFacing) return false;
        p.i(pv::kHitHim, 0);
        if (rac2::rand_below(5) == 0) {
            rac2::seq_play(m, Taunt, 10);
            set_state(m, p, Taunting);
        } else if (shield_of(m, p) != 0) {
            rac2::seq_play(m, ShieldCut, 15, 8);
            set_state(m, p, ShieldCutting);
        } else {
            rac2::seq_play(m, Cut, 15, 8);
            set_state(m, p, Cutting);
        }
        return true;
    };
    switch (p.i(pv::kState)) {
    case Arriving:
        walk_to(m, p, at, false);
        if (frames(p) >= kArriveFrames) wander(m, p);
        break;
    case WanderShield:
    case Wander: {
        if (entering) {
            mover(p, 0.00556f, 0.1033f, 0.00349f, 0.419f);
            rac2::seq_play(m, Walk, 10);
            new_spot(m, p);
            p.i(pv::kIdleAt, static_cast<int32_t>(rac2::level_frame() + kIdleFirst));
        }
        const uint32_t idle = shield_of(m, p) != 0 ? StandShield : Stand;
        const Vec3 spot{p.f(pv::kSpot), p.f(pv::kSpot + 4), at.z};
        if (p.i(pv::kAtSpot) == 0 && dist2d(at, spot) < kStop + 0.1f) {
            p.i(pv::kAtSpot, 1);
            rac2::seq_play(m, idle, 10);
        }
        walk_to(m, p, spot, p.i(pv::kAtSpot) == 0);
        if (p.i(pv::kAtSpot) != 0 && rac2::level_frame() >= static_cast<uint32_t>(p.i(pv::kIdleAt))) {
            rac2::seq_play(m, rac2::rand_below(2) == 0 ? StandShield : Stand, 10);
            p.i(pv::kIdleAt, static_cast<int32_t>(rac2::level_frame()) + rac2::rand_below(static_cast<int32_t>(kIdleNext)) + 60);
        }
        if (turn) {  // (0x3d1588: guarded, at him)
            mover(p, 0.00556f, 0.0317f, 0.00698f, 0.838f);
            rac2::seq_play(m, Guarded, 10);
            set_state(m, p, Approaching);
        }
        break;
    }
    case Approaching:
    case Running: {
        const bool running = p.i(pv::kState) == Running;
        if (!running && to_hero > kRun) {
            mover(p, 0.00556f, 0.1033f, 0.00349f, 0.419f);
            rac2::seq_play(m, Walk, 10);
            set_state(m, p, Running);
        } else if (running && (to_hero < kClose || frames(p) > kRunMost)) {
            mover(p, 0.00556f, 0.0317f, 0.00698f, 0.838f);
            rac2::seq_play(m, Guarded, 10);
            set_state(m, p, Approaching);
        }
        if (!running && to_hero < kClose) {
            square_up(m, p);
            walk_to(m, p, at, false);
            break;
        }
        const float c = std::cos(p.f(pv::kAngle)), sn = std::sin(p.f(pv::kAngle)), dx = hero.x - at.x, dy = hero.y - at.y;
        const float k = std::min(1.0f, std::max(0.0f, (to_hero - kClose) / 6.0f));  // (straight at him at the end)
        walk_to(m, p, Vec3{at.x + (dx * c - dy * sn) * k + dx * (1.0f - k), at.y + (dx * sn + dy * c) * k + dy * (1.0f - k), at.z}, true);
        break;
    }
    case Squaring: {
        if (to_hero > kRun) {
            mover(p, 0.00556f, 0.0317f, 0.00698f, 0.838f);
            rac2::seq_play(m, Guarded, 10);
            set_state(m, p, Approaching);
            break;
        }
        if (try_cut()) break;
        const float side = side_of(m, at, hero);
        if (side > kSide || side < -kSide) {
            rac2::seq_play(m, side > 0.0f ? StepRight : StepLeft, 10);
            p.i(pv::kSideWas, side > 0.0f ? 1 : -1);
            set_state(m, p, side > 0.0f ? SideRight : SideLeft);
        }
        walk_to(m, p, hero, to_hero > kClose - 1.0f);
        face(m, p, bearing);
        break;
    }
    case SideLeft:
    case SideRight: {
        walk_to(m, p, at, false);
        const float was = p.f(pv::kTurnTop);
        p.f(pv::kTurnTop, kDeg);  // (RAC2 turns it a degree a frame while it side-steps: its +0x130 for the while)
        face(m, p, bearing);
        p.f(pv::kTurnTop, was);
        const float side = side_of(m, at, hero);
        if ((p.i(pv::kSideWas) > 0 && side < kSide) || (p.i(pv::kSideWas) < 0 && side > -kSide) || to_hero > kRun) square_up(m, p);
        else try_cut();
        break;
    }
    case Taunting:
        walk_to(m, p, at, false);
        if (frames(p) > 3 && rac2::seq_ended(m)) square_up(m, p);
        break;
    case Cutting:
    case ShieldCutting: {
        walk_to(m, p, at, false);
        const float t = rac2::seq_time(m);
        if (t >= 19.0f && t <= 31.0f) cut_spheres(m, p, hero);
        if (frames(p) > 3 && rac2::seq_ended(m)) {
            if (p.i(pv::kHitHim) != 0) {
                rac2::seq_play(m, Taunt, 10);
                set_state(m, p, Taunting);
            } else {
                wander(m, p);
            }
        }
        break;
    }
    case Blocked:
        walk_to(m, p, at, false);
        if (frames(p) > 12) square_up(m, p);
        break;
    case Knocked:
        if ((walk_to(m, p, at, false) || frames(p) > 90) && frames(p) > 4) {
            if (p.f(pv::kHit) <= kHealth * 0.5f) shield_throw(m, p);  // (0x3d07f4: health 1 or less, the shield off)
            wander(m, p);
        }
        break;
    case Dying:
        if ((walk_to(m, p, at, false) && frames(p) > 4) || frames(p) > 60) {
            die(m, p);
            return;
        }
        break;
    default: wander(m, p);
    }
    if (const uint32_t sh = shield_of(m, p)) rac2::part_place(sh, m, kShieldPoint, 0.0f);
    if (trace() && (rac2::level_frame() % 60) == 0)
        std::printf("[2506] frame %u: %#x state %d seq %u time %.1f at %.1f %.1f %.1f, Ratchet %.1f away, health %.2f, turn %d, shield %d\n",
                    rac2::level_frame(), m, p.i(pv::kState), rac2::seq_now(m), static_cast<double>(rac2::seq_time(m)), static_cast<double>(at.x),
                    static_cast<double>(at.y), static_cast<double>(at.z), static_cast<double>(to_hero), static_cast<double>(p.f(pv::kHit)), turn ? 1 : 0,
                    shield_of(m, p) != 0 ? 1 : 0);
}

}  // namespace

// orig: (none) — see rac2_enemies.h
void rac2_enemy_2506_update(uint32_t m) { update(m); }

}  // namespace rac1
