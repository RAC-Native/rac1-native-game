// See rac2_enemies.h. RAC2's 3134, the arena's small enemy: a little hovering robot that comes through the gates in
// swarms, circles the arena on a ring while it waits for its turn, then flies at Ratchet and hits him with its body.
// Written for RAC1's engine after RAC2's level code (its update 0x40be08, set-up and hits 0x40caf0, turn keeping
// 0x40cf68, activate 0x40d080, death 0x40d180), read as the reference; RAC2's state numbers and sequences kept.
//
// What RAC2's 3134 does, and ours:
// - set up (0x40caf0, state 0): drawn at 0.75 of its class's size; its health from its pvars (1: one hit is enough); an
//   approach angle of its own, -10..10 degrees off the straight line (+0x190), so a swarm spreads as it closes in.
// - arriving (state 3, from the activate 0x40d080): slow (top 0.33 a frame) until it is in the arena, then it circles.
// - circling (state 7): along the ring its pvars name (RAC2's path 42: 60 points 13.6..22 from the centre), the nearest
//   point first, one way or the other at random, 1..3 above the floor (its mover 0.028 / 3.33). Each frame (0x40cf68) it
//   asks the controller for a turn to attack (one count per enemy class, 0x3fc0d0 / 0x3fc1b0: five for the 3134s); with one it chases.
// - chasing (state 8): towards Ratchet along its approach angle (its mover 0.0083 / 1.0), 0.7 above the floor; within 1.7 of him
//   and at its height it stops (state 10), 45 frames later winds up (state 9, sequence 2), then attacks (state 11,
//   sequence 3 at 1.75 x speed): closer than 1.7 it backs off, farther it creeps on (top 0.033); its first four
//   attachment points are damage spheres of 0.5 (0x320328, its pvars' damage: 1). Ratchet hit, it gloats (state 12, sequence 4) and
//   chases again; farther than 4 for 2 seconds it gives up (sequence 5, state 4) and chases again after 26 frames. Its
//   turn lost (another class's enemies take theirs), it goes back to circling.
// - hit (0x40caf0: RAC2's hit test 0x320d48; ours RAC1's, 0x25a420 with the enemies' mask and its hit reaction 0x25a478),
//   its health less the hit's damage, it is knocked along the hit's push, tumbling (state 13); it comes out of it on
//   touching something or at a 1-in-30 chance a frame: alive, it chases again; dead, it blows up (0x40d180: the arena's
//   explosion 0x41f230, its class's sound 0) and the controller counts the kill and drops its bolts (0x3fc248).
// - always: 0.3 over the floor at least (a puff where it scrapes it); kept 0.8 from the others of its class (0x3201e0's
//   push, mask 0x4000000); RAC2 recycles it once it has been stuck 3 seconds (+0xe0), ours lets the controller's own
//   checks handle it.
// RAC2's mover (0x331608, its settings 0x332710: an acceleration and a top speed, units a frame) wants the top speed or what
// lets it stop 0.5 short of its point, sqrt(2 a d), and changes its speed by at most a a frame: ours the same along a
// heading turned at most 0.105 a frame (its +0x1c), without RAC2's steering round obstacles: RAC1's walker_move stops it at
// the walls.
// Ours vs RAC2's, on purpose: its sounds are its sequences' sound events and its looping hum (sound 4 while it flies,
// rac2::sound_loop: RAC1's moby sound word); the bolts are ours (RAC2's come from its controller's kill count).
#include <host/rac2_enemies.h>
#include <host/rac2_compat.h>

#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/disc.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace rac1 {

namespace {

using rac2::Vec3;

constexpr int32_t kRac2Class = 3134;
constexpr float kPi = 3.14159265f;
constexpr float kFloor = 500.0f;          // (the arena's floor: RAC2's controller hands its height to its enemies, 0x3fc098)
constexpr float kHover = 0.7f;            // (over the floor while it chases and attacks)
constexpr float kLowest = 0.3f;           // (never lower: it scrapes the floor)
constexpr float kReach = 1.7f;            // (its attack's distance)
constexpr float kGiveUp = 4.0f;           // (farther than this in its attack for kGiveUpFrames: it gives up)
constexpr float kSpread = 0.8f;           // (kept this far from the others of its class)
constexpr float kHitRadius = 0.5f;        // (its four damage spheres)
constexpr float kScale = 0.75f;           // (of its class's size)
constexpr float kTurnRate = 0.1047f;      // (radians a frame it turns its heading at most: RAC2's mover +0x1c)
constexpr float kStop = 0.5f;             // (its mover stops this short of the point it flies to: its pvars +0x94)
constexpr uint32_t kReadyFrames = 45, kGiveUpFrames = 121, kRecoverFrames = 26, kArriveFrames = 40;
constexpr int32_t kHum = 4;               // (its class's looping sound while it flies)
constexpr float kKeepApart = 1.2f;        // (it steers round the other enemies nearer than this: rac2::steer_round)
constexpr int32_t kBolts = 4;             // (our share of bolts for one)

// RAC2's sequences
enum Seq : uint32_t { Hover = 0, WindUp = 2, Strike = 3, Gloat = 4, Recover = 5 };
// RAC2's states (its numbers)
enum State : int32_t { SetUp = 0, Arriving = 3, Recovering = 4, Holding = 5, Circling = 7, Chasing = 8, WindingUp = 9,
                       Ready = 10, Attacking = 11, Gloating = 12, Knocked = 13 };

// Our pvars (a block of the stand-ins' pool: rac2_compat.h), our own layout
namespace pv {
constexpr uint32_t kState = 0x00, kSince = 0x04, kEntered = 0x08;
constexpr uint32_t kHit = 0x10;           // RAC1's hit state block (0x20 bytes) the hit reaction keeps: +0 the health
constexpr uint32_t kVel = 0x30;           // its flight (a vector: units a frame)
constexpr uint32_t kSpeed = 0x40, kTop = 0x44, kAccel = 0x48, kHeading = 0x4c;  // (its mover: units a frame)
constexpr uint32_t kSpin = 0x50;          // (tumbling: radians a frame about x, y, z)
constexpr uint32_t kPoint = 0x60, kWay = 0x64, kCruise = 0x68, kAngle = 0x6c;
constexpr uint32_t kHeroWas = 0x70;       // (Ratchet's nanotech when it last hit him, -1 none)
constexpr uint32_t kSize = 0x74;          // (its size, kept: the engine sets +0x2c again)
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

// ---- planet.txt's "path 3134 <x y z ...>": the ring it circles ----
uint32_t g_gen = 0xffffffffu;
std::vector<Vec3> g_ring;

// orig: (none)
const std::vector<Vec3>& ring() {
    if (g_gen == custom_level_generation()) return g_ring;
    g_gen = custom_level_generation();
    g_ring.clear();
    std::ifstream f(custom_level_dir() + "/planet.txt");
    std::string line;
    while (f && std::getline(f, line)) {
        std::istringstream in(line);
        std::string key;
        int32_t cls = 0;
        if (!(in >> key) || key != "path" || !(in >> cls) || cls != kRac2Class) continue;
        Vec3 v;
        while (in >> v.x >> v.y >> v.z) g_ring.push_back(v);
    }
    return g_ring;
}

// ---- the others of its class (for the spread) ----
std::vector<uint32_t> g_seen, g_seen_before;
uint32_t g_seen_frame = 0;

// orig: (none) — this frame's list of the live ones, `m` added
const std::vector<uint32_t>& others(uint32_t m) {
    if (g_seen_frame != rac2::level_frame()) {
        g_seen_before.swap(g_seen);
        g_seen.clear();
        g_seen_frame = rac2::level_frame();
    }
    g_seen.push_back(m);
    return g_seen_before;
}

// ---- small helpers ----
// orig: (none)
float dist2d(const Vec3& a, const Vec3& b) { return std::hypot(a.x - b.x, a.y - b.y); }
// orig: (none)
float dist3d(const Vec3& a, const Vec3& b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)); }
// orig: (none) — a - b as an angle in -pi .. pi
float angle_to(float a, float b) {
    float d = std::fmod(a - b, 2.0f * kPi);
    if (d > kPi) d -= 2.0f * kPi;
    if (d < -kPi) d += 2.0f * kPi;
    return d;
}

// orig: (none) — a new state (its first frame next)
void set_state(uint32_t m, const Pv& p, int32_t s) {
    if (trace()) std::printf("[3134] frame %u: %#x state %d -> %d\n", rac2::level_frame(), m, p.i(pv::kState), s);
    p.i(pv::kState, s);
    p.i(pv::kSince, static_cast<int32_t>(rac2::level_frame()));
    p.i(pv::kEntered, 0);
}
// orig: (none) — frames in the state
uint32_t frames(const Pv& p) { return rac2::level_frame() - static_cast<uint32_t>(p.i(pv::kSince)); }

// orig: (none)
void play(uint32_t m, uint32_t seq, int32_t blend) { rac2::seq_play(m, seq, blend); }
// orig: (none)
bool seq_done(uint32_t m) { return rac2::seq_ended(m); }

// orig: (none) — its mover's speeds (RAC2's 0x332710: its acceleration and top speed, units a frame)
void mover(const Pv& p, float accel, float top) {
    p.f(pv::kAccel, accel);
    p.f(pv::kTop, top);
}

// orig: (none) — the floor under it (the arena's, or what is higher under it)
float floor_under(uint32_t m) { return std::max(kFloor, rac2::ground_under(m, kFloor - 2.0f)); }

// orig: (none) — a frame of flight towards `to` (its mover: speed up to its top, slower near it; its heading turned at most
// kTurnRate towards it): the walls stop it (RAC1's walker_move), its height eased to `height`
void fly_to(uint32_t m, const Pv& p, const Vec3& to, float height) {
    const Vec3 at = rac2::moby_pos(m);
    const float d = dist2d(at, to);
    // (RAC2's mover: the speed it wants, its top or what lets it stop kStop short of the point at its acceleration,
    // sqrt(2 a d); its speed changed by at most that acceleration a frame)
    const float acc = p.f(pv::kAccel);
    const float want = std::min(p.f(pv::kTop), std::sqrt(2.0f * acc * std::max(0.0f, d - kStop)));
    float speed = p.f(pv::kSpeed);
    speed = speed < want ? std::min(want, speed + acc) : std::max(want, speed - acc);
    p.f(pv::kSpeed, speed);
    float heading = p.f(pv::kHeading);
    if (d > 1e-3f) heading += std::clamp(angle_to(std::atan2(to.y - at.y, to.x - at.x), heading), -kTurnRate, kTurnRate);
    p.f(pv::kHeading, heading);
    const Vec3 away = rac2::steer_round(m, kKeepApart);
    Vec4 step{std::cos(heading) * speed + away.x * 0.02f, std::sin(heading) * speed + away.y * 0.02f, 0.0f, 0.0f};
    walker_move_v(m, step, 0, 0.0f, 0.3f, 0.0f);
    swc1(m + rac2::moby::kRot + 8, heading);
    // its height: eased towards the wanted one (RAC2's 0x321e78, a spring), never under the floor's kLowest
    const float lowest = floor_under(m) + kLowest;
    float z = lwc1(m + rac2::moby::kPos + 8);
    z += (std::max(height, lowest) - z) * 0.06f;
    swc1(m + rac2::moby::kPos + 8, std::max(z, lowest));
}

// orig: (none) — kept kSpread from the others of its class (RAC2: a damage sphere with the push-only mask 0x4000000)
void spread(uint32_t m) {
    const Vec3 at = rac2::moby_pos(m);
    for (uint32_t o : others(m)) {
        if (o == m || rac2::moby_gone(o, lh(m + rac2::moby::kClassId), lwu(o + rac2::moby::kPvars))) continue;
        const Vec3 q = rac2::moby_pos(o);
        const float dx = at.x - q.x, dy = at.y - q.y, d = std::hypot(dx, dy);
        if (d >= kSpread || std::fabs(at.z - q.z) > kSpread) continue;
        const float push = (kSpread - d) * 0.25f, ux = d > 1e-3f ? dx / d : 1.0f, uy = d > 1e-3f ? dy / d : 0.0f;
        Vec4 step{ux * push, uy * push, 0.0f, 0.0f};
        walker_move_v(m, step, 0, 0.0f, 0.3f, 0.0f);
    }
}

// orig: (none) — Ratchet within its four damage spheres (its class's attachment points 0..3, RAC2's 0x320328): hit (RAC1's
// hit protocol, its damage, pushed away from it)
void strike(uint32_t m, const Pv& p) {
    const Vec3 h = rac2::hero_pos();
    Vec3 j[4];
    rac2::joints_world(m, 0, 4, j);
    bool touched = false;
    for (const Vec3& q : j)  // (Ratchet's position is his feet: his body's middle is about 0.6 up)
        touched = touched || dist3d(q, Vec3{h.x, h.y, h.z + 0.6f}) < kHitRadius + 0.4f;
    if (!touched) return;
    const Vec3 at = rac2::moby_pos(m);
    float dx = h.x - at.x, dy = h.y - at.y;
    const float d = std::max(1e-3f, std::hypot(dx, dy));
    dx /= d;
    dy /= d;
    if (p.i(pv::kHeroWas) < 0) p.i(pv::kHeroWas, rac2::hero_health());
    rac2::hero_hit(m, Vec3{dx, dy, 0.5f}, 1.0f);
}

// orig: (none) — it blows up (RAC2's 0x40d180 with the controller's 0x3fc248): the explosion and its sound 0, its bolts, its
// turn given back; gone (the controller counts it)
void die(uint32_t m) {
    explosion_burst(m, m + rac2::moby::kPos, 0, 0.6f, 4.0f);
    const uint32_t near = dist2d(rac2::moby_pos(m), rac2::hero_pos()) < 7.0f ? 2u : 0u;
    bolts_drop(m, kBolts, kBolts, near, -1);
    rac2::attack_turn_give(m);
    rac2::sound_loop(m, -1);  // (its hum stopped: 0x40d1d0)
    if (trace()) std::printf("[3134] frame %u: %#x blows up\n", rac2::level_frame(), m);
    rac2::moby_kill(m);
}

// orig: (none) — a hit taken (RAC1's hit record through its reaction, as Veldin's walker reads its own): knocked and
// tumbling (state 13); true when one came
bool hit_taken(uint32_t m, const Pv& p) {
    Vec3 from{};
    const float dmg = rac2::hit_receive(m, p.b + pv::kHit, &from);
    if (dmg <= 0.0f) return false;
    const float before = p.f(pv::kHit);
    p.f(pv::kHit, before - dmg);
    if (rac2::hit_log()) std::printf("[3134] frame %u: %#x health %.2f -> %.2f\n", rac2::level_frame(), m, static_cast<double>(before), static_cast<double>(p.f(pv::kHit)));
    // (P9: a hit while it tumbles counts - before, it was dropped, the damage lost - and does not knock it again; out of health,
    // it blows up where the tumble ends, as knocked() has it)
    if (p.i(pv::kState) == Knocked) return false;
    Vec4 push{from.x, from.y, 0.0f, 0.0f};
    if (vec3_length(push) < 1e-4f) {  // (no push: away from Ratchet)
        const Vec3 at = rac2::moby_pos(m), h = rac2::hero_pos();
        push = Vec4{at.x - h.x, at.y - h.y, 0.0f, 0.0f};
    }
    vec3_set_length(push, push, 0.15f);
    push.z = 0.06f;
    push.w = 0.0f;
    V(p.b + pv::kVel) = push;
    p.f(pv::kSpin, rac2::rand_unit() * 0.4f - 0.2f);
    p.f(pv::kSpin + 4, rac2::rand_unit() * 0.4f - 0.2f);
    p.f(pv::kSpin + 8, rac2::rand_unit() * 0.6f - 0.3f);
    if (trace()) std::printf("[3134] frame %u: %#x hit (damage %.2f, health %.2f)\n", rac2::level_frame(), m, static_cast<double>(dmg), static_cast<double>(p.f(pv::kHit)));
    set_state(m, p, Knocked);
    return true;
}

// orig: (none) — a frame knocked: along its push, falling, tumbling, until it touches something (or a 1 in 30 chance)
void knocked(uint32_t m, const Pv& p) {
    Vec4 v = V(p.b + pv::kVel);
    v.z -= 0.006f;
    V(p.b + pv::kVel) = v;
    const Vec4 from = V(m + rac2::moby::kPos);
    Vec4 to = from;
    vec3_add(to, to, v);
    const bool touch = segment_collide_v(from, to, 0, m, nullptr) != 0 || to.z < floor_under(m) + kLowest * 0.5f;
    if (!touch) {
        V(m + rac2::moby::kPos) = Vec4{to.x, to.y, to.z, from.w};
        for (uint32_t k = 0; k < 3; ++k) swc1(m + rac2::moby::kRot + 4 * k, lwc1(m + rac2::moby::kRot + 4 * k) + p.f(pv::kSpin + 4 * k));
    }
    if (!touch && rac2::rand_below(30) != 0 && frames(p) < 90) return;
    if (p.f(pv::kHit) <= 0.0f) {
        die(m);
        return;
    }
    swc1(m + rac2::moby::kRot, 0.0f);
    swc1(m + rac2::moby::kRot + 4, 0.0f);
    p.f(pv::kSpeed, 0.0f);
    play(m, Hover, 5);
    set_state(m, p, Chasing);
}

// orig: (none) — RAC2's 0x40cf68: its turn to attack asked for each frame; without one, back to circling
void keep_turn(uint32_t m, const Pv& p) {
    const int32_t s = p.i(pv::kState);
    if (s < Recovering || s == Knocked) return;
    if (rac2::attack_turn_take(kRac2Class, m)) return;
    if (s == Chasing || s == Attacking) set_state(m, p, ring().empty() ? Holding : Circling);
}

// orig: (none)
void update(uint32_t m) {
    const Pv p{lwu(m + rac2::moby::kPvars)};
    if (p.b == 0 || static_cast<int8_t>(lbu(m + rac2::moby::kState)) < 0) return;
    if (p.i(pv::kState) == SetUp) {
        // (RAC2's 0x40caf0 state 0 and its activate 0x40d080: its size, its health, its approach angle, arriving)
        p.f(pv::kSize, lwc1(m + 0x2c) * kScale);
        for (uint32_t o = 0; o < 0x20; o += 4) sw(p.b + pv::kHit + o, 0);
        sh(p.b + pv::kHit + 0x14, 0xffff);
        sh(p.b + pv::kHit + 0x16, 0xffff);
        p.f(pv::kHit, 1.0f);
        p.f(pv::kAngle, (rac2::rand_unit() * 20.0f - 10.0f) * kPi / 180.0f);
        p.f(pv::kHeading, lwc1(m + rac2::moby::kRot + 8));
        p.f(pv::kSpeed, 0.0f);
        p.i(pv::kHeroWas, -1);
        if (lwu(m + 0x94) != 0) sh(m + rac2::moby::kFlags, lhu(m + rac2::moby::kFlags) | 0x4000u);  // (with its collision: weapons reach it)
        play(m, Hover, 10);
        mover(p, 0.0167f, 0.333f);
        set_state(m, p, Arriving);
    }
    swc1(m + 0x2c, p.f(pv::kSize));
    const bool entering = p.i(pv::kEntered) == 0;
    p.i(pv::kEntered, 1);
    const Vec3 at = rac2::moby_pos(m), hero = rac2::hero_pos();
    const float to_hero = dist3d(at, hero);
    const float base = floor_under(m);
    // its hit on Ratchet landed (his nanotech went down since): it gloats
    if (p.i(pv::kHeroWas) >= 0) {
        if (rac2::hero_health() < p.i(pv::kHeroWas) && p.i(pv::kState) == Attacking) {
            play(m, Gloat, 5);
            set_state(m, p, Gloating);
        }
        p.i(pv::kHeroWas, -1);
    }
    rac2::test_hit(m);
    hit_taken(m, p);  // (each frame, knocked too: its repeat timer runs down)
    keep_turn(m, p);
    // its hum (RAC2's 0x40ccc8: its sound 4 looping, flag 1, while it is up; stopped when it is not, 0x40cd1c): on while it
    // flies, off while it tumbles
    rac2::sound_loop(m, p.i(pv::kState) == Knocked ? -1 : kHum);
    switch (p.i(pv::kState)) {
    case Arriving:  // (from the gate, rising, then circling)
        fly_to(m, p, at, base + 1.5f);
        if (frames(p) >= kArriveFrames) set_state(m, p, ring().empty() ? Holding : Circling);
        break;
    case Holding:  // (RAC2's state 5: no ring, waiting for a turn)
        fly_to(m, p, at, base + 1.5f);
        if (rac2::attack_turn_held(m)) set_state(m, p, Chasing);
        break;
    case Circling: {
        const std::vector<Vec3>& r = ring();
        if (r.empty()) {
            set_state(m, p, Holding);
            break;
        }
        if (entering) {  // (the nearest point, one way or the other, its own height on the ring)
            mover(p, 0.0278f, 3.333f);
            size_t best = 0;
            for (size_t k = 1; k < r.size(); ++k)
                if (dist2d(at, r[k]) < dist2d(at, r[best])) best = k;
            p.i(pv::kPoint, static_cast<int32_t>(best));
            p.i(pv::kWay, rac2::rand_below(2) == 0 ? 1 : -1);
            p.f(pv::kCruise, kFloor + 1.0f + rac2::rand_unit() * 2.0f);
        }
        const int32_t n = static_cast<int32_t>(r.size());
        int32_t k = ((p.i(pv::kPoint) % n) + n) % n;
        if (dist2d(at, r[static_cast<size_t>(k)]) < 2.0f) k = ((k + p.i(pv::kWay)) % n + n) % n;
        p.i(pv::kPoint, k);
        fly_to(m, p, r[static_cast<size_t>(k)], p.f(pv::kCruise));
        if (rac2::attack_turn_held(m)) set_state(m, p, Chasing);
        break;
    }
    case Chasing: {
        if (entering) mover(p, 0.00833f, 1.0f);
        // (towards Ratchet along its approach angle: RAC2's 0x331518 heads at his bearing plus the angle)
        const float dx = hero.x - at.x, dy = hero.y - at.y, c = std::cos(p.f(pv::kAngle)), s = std::sin(p.f(pv::kAngle));
        const Vec3 aim{at.x + dx * c - dy * s, at.y + dx * s + dy * c, hero.z};
        if (to_hero >= kReach) fly_to(m, p, aim, base + kHover);
        else fly_to(m, p, at, base + kHover);
        if (to_hero < kReach && std::fabs(lwc1(m + rac2::moby::kPos + 8) - (base + kHover)) < 0.1f) set_state(m, p, Ready);
        break;
    }
    case Ready:
        fly_to(m, p, at, base + kHover);
        p.f(pv::kHeading, p.f(pv::kHeading) + std::clamp(angle_to(std::atan2(hero.y - at.y, hero.x - at.x), p.f(pv::kHeading)), -kTurnRate, kTurnRate));
        swc1(m + rac2::moby::kRot + 8, p.f(pv::kHeading));
        if (frames(p) >= kReadyFrames) {
            play(m, WindUp, 2);
            swc1(m + rac2::moby::kSpeed, 1.0f);
            set_state(m, p, WindingUp);
        }
        break;
    case WindingUp:
        fly_to(m, p, at, base + kHover);
        if (seq_done(m) && lbu(m + rac2::moby::kNextSeq) == WindUp) {
            play(m, Strike, 5);
            set_state(m, p, Attacking);
        }
        break;
    case Attacking: {
        if (entering) mover(p, 0.00111f, 0.0333f);
        swc1(m + rac2::moby::kSpeed, 1.75f);
        if (to_hero >= kReach) {
            fly_to(m, p, hero, base + kHover);
        } else {  // (too close: it backs off, 2 away from where it is)
            const float d = std::max(1e-3f, dist2d(at, hero));
            const Vec3 back{at.x + (at.x - hero.x) / d * 2.0f, at.y + (at.y - hero.y) / d * 2.0f, at.z};
            fly_to(m, p, back, base + kHover);
            p.f(pv::kHeading, std::atan2(hero.y - at.y, hero.x - at.x));  // (still facing him)
            swc1(m + rac2::moby::kRot + 8, p.f(pv::kHeading));
        }
        strike(m, p);
        if (to_hero > kGiveUp && frames(p) >= kGiveUpFrames) {
            play(m, Recover, 15);
            swc1(m + rac2::moby::kSpeed, 1.0f);
            set_state(m, p, Recovering);
        }
        break;
    }
    case Gloating:
        fly_to(m, p, at, base + kHover);
        if (seq_done(m) && lbu(m + rac2::moby::kNextSeq) == Gloat) {
            play(m, Hover, 15);
            swc1(m + rac2::moby::kSpeed, 1.0f);
            set_state(m, p, Chasing);
        }
        break;
    case Recovering:
        fly_to(m, p, at, base + kHover);
        if (frames(p) >= kRecoverFrames) {
            play(m, Hover, 5);
            set_state(m, p, Chasing);
        }
        break;
    case Knocked:
        knocked(m, p);
        return;
    default: set_state(m, p, Chasing);
    }
    spread(m);
    if (trace() && (rac2::level_frame() % 60) == 0)
        std::printf("[3134] frame %u: %#x state %d at %.1f %.1f %.1f, Ratchet %.1f away, health %.2f, turn %d\n", rac2::level_frame(), m,
                    p.i(pv::kState), static_cast<double>(at.x), static_cast<double>(at.y), static_cast<double>(at.z), static_cast<double>(to_hero),
                    static_cast<double>(p.f(pv::kHit)), rac2::attack_turn_held(m) ? 1 : 0);
}

}  // namespace

// orig: (none) — see rac2_enemies.h
void rac2_enemy_3134_update(uint32_t m) { update(m); }

}  // namespace rac1
