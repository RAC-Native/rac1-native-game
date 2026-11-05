// See rac2_enemies.h. RAC2's 2552, the boss Chainblade: a big robot with a long blade in each hand, each on a chain. It comes
// in at the arena's north end, floats down to the floor and lands with a slam; on the floor it runs at Ratchet and swings both
// blades; after a while it roars and rises off the floor, keeps its distance and throws its blades at him on their chains
// (the blade slams into the floor where he stood and is pulled back); then it comes down again with a slam. Written for
// RAC1's engine after RAC2's level code (its update 0x3d2de0, set-up and hits 0x3d4308, after-update 0x3d46b8, activate
// 0x3d47b0, park 0x3d4900, taunts 0x3d4af0, the rounds 0x3d4d00 / 0x3d4d68, its floating 0x3d4e00 / 0x3d4ed0 / 0x3d4fd0, the
// swing's hurt 0x3d5118, the throw 0x3d52a8 / 0x3d5660 / 0x3d5748, the chain 0x3d5570 / 0x3d5b90), read as the reference;
// RAC2's state numbers and sequences kept.
//
// What RAC2's Chainblade does, and ours:
// - set up (0x3d4308): its health from its pvars (+0x24: 80), the swings' and the blade's damage (+0x34, +0x35: 1 each); its
//   chain's five mobys spawned and hidden (0x3d5570: classes 2585..2589, link 0 the grip in its hand, link 4 the blade).
// - activated (0x3d47b0, by the controller's boss column): at RAC2's entry point (gp + 0x11d0: 500, 642, 510, above the
//   north gate), facing Ratchet (state 2). It stands (sequence 0) for 121 frames, then floats (sequence 9) towards the
//   arena's middle (gp + 0x11c0: 500, 600, 500); within 18 of it it comes down (state 20): landed, the slam (sequence 18,
//   the camera shaken 0.2 at its times 8..10), then the fight on the floor. Hits do nothing while it comes in (states < 5).
// - on the floor (its substate 1): it runs at Ratchet (state 6, sequence 1; its mover 0.0044 / 0.133 a frame, turning
//   0.0084 / 0.21; the camera shaken 0.05 at each footfall, times 1..3 and 16..18); Ratchet farther than 23 from the
//   middle: it taunts (0x3d4af0) instead (ours: past its round it then rises, where RAC2 would taunt on); 1 in 400 a
//   frame it taunts anyway. Within 9 and facing him within 5 degrees (at
//   its run's times 3..15) it swings (state 7, sequence 2): the right blade at times 19..23, the left at 32..36, each a
//   5.3-long blade from its hand along the hand's axis, a damage sphere where it passes nearest him (0.3 to 0.5 round, its
//   damage). Swung: 1 in 3 a taunt; after 300 frames on the floor (then 1 in (500 - frames since it last hit him) a
//   frame, 0x3d4d00) it roars (state 19, sequence 17) and rises.
// - in the air (substate 2): it floats (state 11, sequence 9) about a point 13..45 from Ratchet (0x3d4e00 / 0x3d5e40, a
//   carrot 0.83 a frame ahead along its approach angle 20..40 degrees); 5..40 from him, when its throw is due, one of its
//   blades (state 12 right, sequence 10; 15 left, 13): it comes to its throwing range (0x3d52a8: its height and distance
//   such that the blade, thrown down at 1.15 radians, lands on him), and at its sequence's time 19 the blade goes
//   (0x3d5660): the chain runs out at 1.17 a frame (each link a quarter of the run later), the blade slams into the floor
//   (0x41f230's dust, a 0.5 damage sphere: its damage) and lies 5 frames, then the chain comes back at 1.5 a frame
//   (states 13 / 16 hold, sequences 11 / 14; 14 / 17 pull, 12 / 15); then it floats again. After 500 frames in the air
//   (then 1 in (900 - ...) a frame, 0x3d4d68), or farther than 18 from the middle, it comes down (state 20) and lands.
// - hit (0x3d4308 with RAC2's hit reaction 0x32bbf0): its health less the damage (RAC2's own weapons 3209 and 3070 at 0.3);
//   at each third of its health lost it is knocked (RAC2's hit block +0x10: state 8 on the floor, a stagger, sequence 16;
//   state 18 in the air: it falls, lies (sequence 4), gets up roaring (19)). No health: it falls (state 8, sequence 4; the
//   camera shaken 0.4 when it lands, its times 95..97), lies until its time 110 (state 9), and 61 frames later blows up
//   (0x40d7b8) and the controller counts it (0x3fc248).
// Ours vs RAC2's: no path-finding round the arena's middle (0x32f608: straight at him, RAC1's walker_move stops it at the
// walls); its floating moved by hand (no collision); the throw aimed at where Ratchet stands when the blade goes (RAC2 gets
// there by its height and facing: the same place); the chain's links set along the line from its hand to the blade (RAC2
// sets each from the hand's matrix the same way); its taunts, rises and slams by RAC2's numbers.
// - the tag team (challenge 9, its partner the B2 Brawler: rac2_enemies.h): a third more of its health lost while its
//   partner is up (0x3d4a60), it rises after its swing and, in the air, floats back to its entry point (state 3, sequence
//   9; its partner's turn, 0x3db880) and waits there (state 4, sequence 0) until its turn (0x3d4990), then comes in again;
//   dying, its partner's turn too (0x3d4658).
#include <host/custom_arena.h>
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

constexpr int kLinks = 5;
// (RAC2's 2585..2589 converted: maktar.py RAC2_CLASSES; 2589's id below 2048, RAC1's class index table's size)
constexpr int32_t kLinkClass[kLinks] = {2044, 2045, 2046, 2047, 2031};
constexpr float kPi = 3.14159265f, kDeg = kPi / 180.0f;
constexpr float kFloor = 500.0f;
constexpr Vec3 kMiddle{500.0f, 600.0f, 500.0f};  // (gp + 0x11c0)
constexpr Vec3 kEntry{500.0f, 642.0f, 510.0f};   // (gp + 0x11d0)
constexpr float kHealth = 80.0f;          // (its pvars +0x24)
constexpr float kSwingDamage = 1.0f, kBladeDamage = 1.0f;  // (its pvars +0x34, +0x35)
constexpr float kBladeLength = 5.3f;      // (a swung blade's reach from its hand)
constexpr float kSwingRange = 9.0f, kSwingFacing = 0.0873f;
constexpr float kRingOut = 18.0f;         // (farther from the middle: it comes down / it lands within this coming in)
constexpr float kTauntOut = 23.0f;        // (Ratchet farther from the middle: it taunts instead of running at him)
constexpr float kThrowMin = 5.0f, kThrowMax = 40.0f;
constexpr float kThrowAngle = 1.15f;      // (the blade thrown down at this under the level, from its hand)
constexpr float kHandHeight = 4.2f;       // (its hands above its feet, 0x3d52a8)
constexpr float kHoverMax = 5.0f;         // (it floats at most this above the floor)
constexpr float kChainOut = 1.1667f, kChainBack = 1.5f;  // (units a frame)
constexpr float kLinkAt[kLinks] = {0.0f, 1.5f, 2.5f, 3.5f, 4.5f};  // (gp + 0x11a0: each link's place behind the run)
constexpr float kGravity = 0.012f, kStepUp = 1.0f;
constexpr float kStop = 0.5f;
constexpr uint32_t kStandFirst = 121, kGroundRounds = 300, kAirRounds = 500;
constexpr uint32_t kDyingDown = 110, kDyingGone = 61;

// RAC2's sequences
enum Seq : uint32_t {
    Stand = 0, Run = 1, Swing = 2, Down = 4, Taunt5 = 5, Taunt6 = 6, Taunt7 = 7, Taunt8 = 8, Float = 9, ThrowR = 10,
    HoldR = 11, PullR = 12, ThrowL = 13, HoldL = 14, PullL = 15, Stagger = 16, Roar = 17, Land = 18, AirTaunt19 = 19, AirTaunt20 = 20
};
// RAC2's states
enum State : int32_t {
    SetUp = 0, Entering = 2, TagOut = 3, Waiting = 4, Chasing = 6, Swinging = 7, Knocked = 8, Dying = 9, Taunting = 10, Floating = 11, ThrowingR = 12,
    HoldingR = 13, PullingR = 14, ThrowingL = 15, HoldingL = 16, PullingL = 17, Falling = 18, Roaring = 19, Landing = 20
};

// Our pvars (a block of the stand-ins' pool), our own layout
namespace pv {
constexpr uint32_t kState = 0x00, kSince = 0x04, kEntered = 0x08, kPhase = 0x0c;  // (kPhase: RAC2's substate, 1 floor, 2 air)
constexpr uint32_t kHit = 0x10;            // RAC1's hit state block (0x20 bytes): +0 the health
constexpr uint32_t kSpeed = 0x30, kTop = 0x34, kAccel = 0x38, kHeading = 0x3c;
constexpr uint32_t kVz = 0x40, kTurnW = 0x44, kTurnAcc = 0x48, kTurnTop = 0x4c;
constexpr uint32_t kThirds = 0x50;         // (thirds of its health lost: RAC2's +0x31a)
constexpr uint32_t kRounds = 0x54;         // (frames since it landed or rose: RAC2's +0x32e)
constexpr uint32_t kSinceHit = 0x58;       // (frames since it last hurt Ratchet: RAC2's +0x32a)
constexpr uint32_t kBack = 0x5c;           // (a taunt's state to go back to)
constexpr uint32_t kLinksAt = 0x60;        // (its chain's five mobys)
constexpr uint32_t kSide = 0x78, kCount = 0x7c, kLength = 0x80, kStruck = 0x84;  // (a blade out: its side, frames, length)
constexpr uint32_t kHand = 0x88, kDir = 0x98, kImpact = 0xa8;  // (x y z each: where it went from, its way, where it lands)
constexpr uint32_t kCarrot = 0xb8;         // (x y z: the point it floats towards)
constexpr uint32_t kAngle = 0xc8, kThrowAt = 0xcc, kSwingHit = 0xd0, kDownFlag = 0xd4, kVx = 0xd8, kVy = 0xdc;
// (P7, the tag team: its partner (+0x310) and how to know it is still that one; its flag to come in (+0x318), the thirds of
// its health counted at its turns (+0x31a); waiting first)
constexpr uint32_t kPartner = 0xe0, kPartnerCls = 0xe4, kPartnerPv = 0xe8, kGo = 0xec, kTurns = 0xf0, kWaitFirst = 0xf4;
}  // namespace pv

struct Pv {
    uint32_t b;
    int32_t i(uint32_t o) const { return lw(b + o); }
    void i(uint32_t o, int32_t v) const { sw(b + o, static_cast<uint32_t>(v)); }
    float f(uint32_t o) const { return lwc1(b + o); }
    void f(uint32_t o, float v) const { swc1(b + o, v); }
    Vec3 v(uint32_t o) const { return Vec3{f(o), f(o + 4), f(o + 8)}; }
    void v(uint32_t o, const Vec3& x) const {
        f(o, x.x);
        f(o + 4, x.y);
        f(o + 8, x.z);
    }
};

// orig: (none) — (testing) RAC1_ARENA_BOSS_HEALTH=<n>: the bosses' health at their set-up (RAC2's otherwise)
float health_most() {
    static const float h = [] {
        const char* e = std::getenv("RAC1_ARENA_BOSS_HEALTH");
        const float v = e != nullptr ? std::strtof(e, nullptr) : 0.0f;
        return v > 0.0f ? v : kHealth;
    }();
    return h;
}

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
    if (trace()) std::printf("[2552] frame %u: %#x state %d -> %d (health %.1f)\n", rac2::level_frame(), m, p.i(pv::kState), s, static_cast<double>(p.f(pv::kHit)));
    p.i(pv::kState, s);
    p.i(pv::kSince, static_cast<int32_t>(rac2::level_frame()));
    p.i(pv::kEntered, 0);
}
// orig: (none)
uint32_t frames(const Pv& p) { return rac2::level_frame() - static_cast<uint32_t>(p.i(pv::kSince)); }

// orig: (none) — its mover's settings (RAC2's 0x332710: acceleration, top speed, turning's acceleration and top; a frame)
void mover(const Pv& p, float accel, float top, float turn_acc, float turn_top) {
    p.f(pv::kAccel, accel);
    p.f(pv::kTop, top);
    p.f(pv::kTurnAcc, turn_acc);
    p.f(pv::kTurnTop, turn_top);
}

// orig: (none) — its facing turned towards `yaw` (the turning speeds up by its acceleration to its top)
void face(uint32_t m, const Pv& p, float yaw) {
    const float d = angle_to(yaw, p.f(pv::kHeading));
    const float w = std::min({p.f(pv::kTurnW) + p.f(pv::kTurnAcc), p.f(pv::kTurnTop), std::fabs(d)});
    p.f(pv::kTurnW, std::fabs(d) < 1e-3f ? 0.0f : w);
    p.f(pv::kHeading, p.f(pv::kHeading) + std::copysign(w, d));
    swc1(m + rac2::moby::kRot + 8, p.f(pv::kHeading));
}

// orig: (none) — a frame on its feet towards `to` (RAC2's mover; `moving` false: standing), the walls stop it; in the air it
// falls onto the floor. True on the frame it lands
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
    Vec4 step{std::cos(h) * speed + p.f(pv::kVx), std::sin(h) * speed + p.f(pv::kVy), 0.0f, 0.0f};
    p.f(pv::kVx, p.f(pv::kVx) * 0.9f);  // (a knock's push, fading)
    p.f(pv::kVy, p.f(pv::kVy) * 0.9f);
    walker_move_v(m, step, 0, kStepUp, 1.2f, 0.0f);
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

// orig: (none) — a frame in the air towards `to` (RAC2's 0x3d4fd0: each axis eased towards it, 0x321e78), facing `yaw`;
// true when there
bool float_to(uint32_t m, const Pv& p, const Vec3& to, float yaw) {
    Vec3 at = rac2::moby_pos(m);
    const float dx = to.x - at.x, dy = to.y - at.y, dz = to.z - at.z;
    const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float acc = 0.006f, top = 0.15f;
    float speed = p.f(pv::kSpeed);
    const float want = std::min(top, std::sqrt(2.0f * acc * d));
    speed = speed < want ? std::min(want, speed + acc) : std::max(want, speed - acc);
    p.f(pv::kSpeed, speed);
    if (d > 1e-4f) {
        const float k = std::min(d, speed) / d;
        at.x += dx * k;
        at.y += dy * k;
        at.z += dz * k;
    }
    swc1(m + rac2::moby::kPos, at.x);
    swc1(m + rac2::moby::kPos + 4, at.y);
    swc1(m + rac2::moby::kPos + 8, at.z);
    p.f(pv::kVz, 0.0f);
    face(m, p, yaw);
    return d < 0.05f;
}

// orig: (none) — the carrot it floats after (RAC2's 0x3d4e00 / 0x3d4ed0): a point 13..45 from Ratchet, kept within 10 of
// the middle's sides, approached at 0.83 a frame along its approach angle
Vec3 carrot(uint32_t m, const Pv& p) {
    const Vec3 at = rac2::moby_pos(m), hero = rac2::hero_pos();
    float dx = at.x - hero.x, dy = at.y - hero.y;
    float d = std::max(0.01f, std::hypot(dx, dy));
    const float keep = std::clamp(d, 13.0f, 45.0f);
    const float a = std::atan2(dy, dx) + p.f(pv::kAngle) * 0.02f;
    Vec3 want{hero.x + std::cos(a) * keep, hero.y + std::sin(a) * keep, kFloor + 3.0f};
    if (dist2d(want, kMiddle) > 10.0f) {  // (RAC2: back to the middle, a unit higher)
        want = Vec3{kMiddle.x, kMiddle.y, kFloor + 3.0f};
    }
    Vec3 c = p.v(pv::kCarrot);
    dx = want.x - c.x;
    dy = want.y - c.y;
    float dz = want.z - c.z;
    d = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (d > 0.8333f) {
        dx *= 0.8333f / d;
        dy *= 0.8333f / d;
        dz *= 0.8333f / d;
    }
    c = Vec3{c.x + dx, c.y + dy, c.z + dz};
    p.v(pv::kCarrot, c);
    return c;
}

// orig: (none) — link k, while it is the one it spawned
uint32_t link_of(uint32_t m, const Pv& p, int k) {
    const uint32_t w = static_cast<uint32_t>(p.i(pv::kLinksAt + 4 * static_cast<uint32_t>(k)));
    return rac2::part_of(w, m) ? w : 0u;
}

// orig: (none) — its chain spawned, hidden (RAC2's 0x3d5570: classes 2585..2589)
void chain_spawn(uint32_t m, const Pv& p) {
    for (int k = 0; k < kLinks; ++k) {
        const uint32_t w = rac2::part_spawn(m, kLinkClass[k], 1.0f);
        p.i(pv::kLinksAt + 4 * static_cast<uint32_t>(k), static_cast<int32_t>(w));
        if (w != 0) rac2::part_put(w, rac2::moby_pos(m), Vec3{1.0f, 0.0f, 0.0f}, false);
    }
}

// orig: (none) — its chain hidden, no blade out (RAC2's 0x3d5b90)
void chain_stow(uint32_t m, const Pv& p) {
    p.i(pv::kSide, -1);
    for (int k = 0; k < kLinks; ++k)
        if (const uint32_t w = link_of(m, p, k)) rac2::part_put(w, rac2::moby_pos(m), Vec3{1.0f, 0.0f, 0.0f}, false);
}

// orig: (none) — its chain gone with it
void chain_kill(uint32_t m, const Pv& p) {
    for (int k = 0; k < kLinks; ++k)
        if (const uint32_t w = link_of(m, p, k)) rac2::moby_kill(w);
}

// orig: (none) — a blade thrown (RAC2's 0x3d5660): from its hand on `side` (attachment point 0 its right, 1 its left) down
// at where Ratchet stands
void blade_throw(uint32_t m, const Pv& p, int side) {
    Vec3 hand, x, y, z;
    rac2::point_frame(m, static_cast<uint32_t>(side), hand, x, y, z);
    const Vec3 hero = rac2::hero_pos();
    const Vec3 impact{hero.x, hero.y, std::max(kFloor, hero.z)};
    Vec3 dir{impact.x - hand.x, impact.y - hand.y, impact.z - hand.z};
    const float len = std::max(1.0f, std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z));
    dir = Vec3{dir.x / len, dir.y / len, dir.z / len};
    p.i(pv::kSide, side);
    p.i(pv::kCount, 0);
    p.i(pv::kStruck, 0);
    p.f(pv::kLength, len);
    p.v(pv::kHand, hand);
    p.v(pv::kDir, dir);
    p.v(pv::kImpact, impact);
    if (trace()) std::printf("[2552] frame %u: %#x throws its %s blade %.1f\n", rac2::level_frame(), m, side == 0 ? "right" : "left", static_cast<double>(len));
}

// orig: (none) — the blade's frame (RAC2's 0x3d5748): the chain run out (each link a quarter of the run later), the blade's
// slam into the floor (dust, a damage sphere) when it is out, held 5 frames, pulled back. 1: being pulled back, 2: back,
// 0: going out or held
int blade_step(uint32_t m, const Pv& p) {
    const int side = p.i(pv::kSide);
    if (side < 0) return 2;
    Vec3 hand, x, y, z;
    rac2::point_frame(m, static_cast<uint32_t>(side), hand, x, y, z);  // (the chain goes from where its hand is now)
    const Vec3 dir = p.v(pv::kDir);
    const float run = std::max(0.0f, p.f(pv::kLength) - kLinkAt[kLinks - 1]);
    const float out = run / kChainOut;   // (frames going out)
    const float back = run / kChainBack; // (frames coming back)
    const int32_t n = p.i(pv::kCount) + 1;
    p.i(pv::kCount, n);
    const float t = static_cast<float>(n);
    for (int k = 0; k < kLinks; ++k) {
        float reach;
        const float share = static_cast<float>(k) / static_cast<float>(kLinks - 1);
        if (t < out) reach = (t - (1.0f - share) * out * 0.25f) * kChainOut * share;
        else if (t < out + 5.0f) reach = run * share;
        else reach = run * share - (t - out - 5.0f) * kChainBack;
        reach = std::max(0.0f, std::min(reach, run * share)) + kLinkAt[k];
        if (const uint32_t w = link_of(m, p, k)) rac2::part_put(w, Vec3{hand.x + dir.x * reach, hand.y + dir.y * reach, hand.z + dir.z * reach}, dir, true);
        if (trace() && k == kLinks - 1 && n % 3 == 0)
            std::printf("[2552] frame %u: %#x blade %d out %.1f (link %#x flags %04x), hand %.1f %.1f %.1f\n", rac2::level_frame(), m, n,
                        static_cast<double>(reach), link_of(m, p, k), link_of(m, p, k) != 0 ? lhu(link_of(m, p, k) + rac2::moby::kFlags) : 0u,
                        static_cast<double>(hand.x), static_cast<double>(hand.y), static_cast<double>(hand.z));
    }
    if (p.i(pv::kStruck) == 0 && t >= out + 1.0f) {  // (the slam: RAC2's 0x41f230 dust and a 0.5 damage sphere)
        p.i(pv::kStruck, 1);
        const Vec3 at = p.v(pv::kImpact), hero = rac2::hero_pos();
        for (int k = 0; k < 10; ++k) {
            const float a = static_cast<float>(k) * 0.628f;
            spawn_particle_3c_v(Vec4{at.x, at.y, at.z + 0.2f, 1.0f}, Vec4{std::cos(a) * 0.05f, std::sin(a) * 0.05f, 0.03f, 0.0f}, 0x40a0a0a0u, 24, 0, 0, 0.9f);
        }
        rac2::camera_shake(0.1f, 12);
        const float dx = hero.x - at.x, dy = hero.y - at.y, dz = hero.z + 0.6f - at.z;
        if (std::sqrt(dx * dx + dy * dy + dz * dz) < 0.5f + 0.6f) {
            rac2::hero_hit(m, Vec3{dir.x * 1.5f, dir.y * 1.5f, 1.0f}, kBladeDamage);
            p.i(pv::kSinceHit, 0);
            if (trace()) std::printf("[2552] frame %u: %#x blade hits Ratchet (nanotech %d)\n", rac2::level_frame(), m, rac2::hero_health());
        }
    }
    if (t > out + back + 7.0f) return 2;
    if (t > out + 6.0f) return 1;
    return 0;
}

// orig: (none) — a swung blade's hurt (RAC2's 0x3d5118): the blade 5.3 from its hand on `side` along the hand's axis, a
// damage sphere at its point nearest Ratchet, 0.3 at the hand to 0.5 at the tip
void swing_hurt(uint32_t m, const Pv& p, int side) {
    Vec3 hand, x, y, z;
    rac2::point_frame(m, static_cast<uint32_t>(side), hand, x, y, z);
    const float lx = std::sqrt(x.x * x.x + x.y * x.y + x.z * x.z);
    if (lx < 1e-5f) return;
    x = Vec3{x.x / lx, x.y / lx, x.z / lx};
    const Vec3 hero = rac2::hero_pos();
    const Vec3 h{hero.x, hero.y, hero.z + 0.6f};
    const float along = std::clamp((h.x - hand.x) * x.x + (h.y - hand.y) * x.y + (h.z - hand.z) * x.z, 0.0f, kBladeLength);
    const Vec3 q{hand.x + x.x * along, hand.y + x.y * along, hand.z + x.z * along};
    const float r = 0.3f + 0.2f * along / kBladeLength;
    const float dx = h.x - q.x, dy = h.y - q.y, dz = h.z - q.z;
    if (std::sqrt(dx * dx + dy * dy + dz * dz) >= r + 0.6f) return;
    const uint32_t bit = 1u << side;
    if ((static_cast<uint32_t>(p.i(pv::kSwingHit)) & bit) != 0) return;  // (a blade hurts once a swing)
    p.i(pv::kSwingHit, static_cast<int32_t>(static_cast<uint32_t>(p.i(pv::kSwingHit)) | bit));
    const float hd = p.f(pv::kHeading);
    rac2::hero_hit(m, Vec3{std::cos(hd) * 2.0f, std::sin(hd) * 2.0f, 1.0f}, kSwingDamage);
    p.i(pv::kSinceHit, 0);
    if (trace()) std::printf("[2552] frame %u: %#x swings into Ratchet (nanotech %d)\n", rac2::level_frame(), m, rac2::hero_health());
}

// orig: (none) — a taunt (RAC2's 0x3d4af0: on the floor 5 or 6, or 7 or 8; in the air 19 or 20), then back to `back`
void taunt(uint32_t m, const Pv& p, int32_t back) {
    uint32_t s;
    if (p.i(pv::kPhase) == 2) s = rac2::rand_below(2) == 0 ? AirTaunt19 : AirTaunt20;
    else if (rac2::rand_below(2) == 0) s = rac2::rand_below(2) == 0 ? Taunt5 : Taunt6;
    else s = rac2::rand_below(2) == 0 ? Taunt7 : Taunt8;
    rac2::seq_play(m, s, 10);
    p.i(pv::kBack, back);
    set_state(m, p, Taunting);
}

// orig: (none) — the rounds (RAC2's 0x3d4d00 on the floor, 0x3d4d68 in the air): time to rise / to come down
bool round_over(uint32_t m, const Pv& p) {
    const int32_t n = p.i(pv::kRounds);
    if (p.i(pv::kPhase) == 2 && dist2d(rac2::moby_pos(m), kMiddle) > kRingOut) return true;
    const int32_t first = p.i(pv::kPhase) == 2 ? static_cast<int32_t>(kAirRounds) : static_cast<int32_t>(kGroundRounds);
    const int32_t last = p.i(pv::kPhase) == 2 ? 900 : 500;
    if (n < first) return false;
    const int32_t chance = last - p.i(pv::kSinceHit);
    return n > last || chance <= 0 || rac2::rand_below(chance) == 0;
}

// orig: (none) — the partner (the tag team's other boss) still in it (RAC2: alive, its state 3 or more)
bool partner_up(const Pv& p) {
    const uint32_t q = static_cast<uint32_t>(p.i(pv::kPartner));
    if (q == 0 || rac2::moby_gone(q, p.i(pv::kPartnerCls), static_cast<uint32_t>(p.i(pv::kPartnerPv)))) return false;
    return rac2_boss_fighting(q);
}

// orig: (none) — time to hand over (RAC2's 0x3d4a60): the partner up and a third more of its health lost
bool tag_due(const Pv& p) {
    return partner_up(p) && p.f(pv::kHit) < health_most() * static_cast<float>(3 - p.i(pv::kTurns)) / 3.0f;
}

// orig: (none) — the partner's turn (RAC2's 0x3db880 on it, then 0x3fc020: the controller's one fighting)
void hand_over(const Pv& p) {
    const uint32_t q = static_cast<uint32_t>(p.i(pv::kPartner));
    if (q == 0 || rac2::moby_gone(q, p.i(pv::kPartnerCls), static_cast<uint32_t>(p.i(pv::kPartnerPv)))) return;
    rac2_boss_turn(q);
    custom_arena_boss_active(q);
}

// orig: (none) — it blows up (RAC2's 0x40d7b8): explosions round it, its chain gone, counted by the controller
void die(uint32_t m, const Pv& p) {
    for (int k = 0; k < 4; ++k) explosion_burst(m, m + rac2::moby::kPos, -1, 2.5f, 12.0f);
    rac2::camera_shake(0.4f, 25);
    chain_kill(m, p);
    if (trace()) std::printf("[2552] frame %u: %#x blows up\n", rac2::level_frame(), m);
    rac2::moby_kill(m);
}

// orig: (none) — a hit taken (RAC1's hit record through its reaction; RAC2's 0x4308 / 0x32bbf0): none while it comes in;
// its health less the damage; a third of it lost: knocked; none left: down
void hit_taken(uint32_t m, const Pv& p) {
    Vec3 from{};
    const float dmg = rac2::hit_receive(m, p.b + pv::kHit, &from);
    const int32_t s = p.i(pv::kState);
    if (dmg <= 0.0f || s < 5 || s == Dying || (s == Knocked && p.f(pv::kHit) <= 0.0f)) return;
    p.f(pv::kHit, p.f(pv::kHit) - dmg);
    if (trace()) std::printf("[2552] frame %u: %#x hit (damage %.2f, health %.2f)\n", rac2::level_frame(), m, static_cast<double>(dmg), static_cast<double>(p.f(pv::kHit)));
    const bool air = p.i(pv::kPhase) == 2;
    if (p.f(pv::kHit) <= 0.0f) {  // (down: RAC2's substate 3)
        chain_stow(m, p);
        rac2::seq_play(m, Down, 3);
        p.i(pv::kDownFlag, 1);
        set_state(m, p, air ? Falling : Knocked);
        return;
    }
    // (RAC2's 0x3d4990: a third more of its health lost)
    const int32_t thirds = p.i(pv::kThirds);
    if (thirds >= 2 || p.f(pv::kHit) >= health_most() * static_cast<float>(2 - thirds) / 3.0f) return;
    p.i(pv::kThirds, thirds + 1);
    Vec4 push{from.x, from.y, 0.0f, 0.0f};
    if (vec3_length(push) < 1e-4f) push = Vec4{-std::cos(p.f(pv::kHeading)), -std::sin(p.f(pv::kHeading)), 0.0f, 0.0f};
    vec3_set_length(push, push, 0.12f);
    p.f(pv::kVx, push.x);
    p.f(pv::kVy, push.y);
    chain_stow(m, p);
    if (air) {
        p.i(pv::kDownFlag, 0);
        set_state(m, p, Falling);
    } else {
        rac2::seq_play(m, Stagger, 3);
        p.i(pv::kDownFlag, 0);
        set_state(m, p, Knocked);
    }
}

// orig: (none)
void update(uint32_t m) {
    const Pv p{lwu(m + rac2::moby::kPvars)};
    if (p.b == 0 || static_cast<int8_t>(lbu(m + rac2::moby::kState)) < 0) return;
    if (p.i(pv::kState) == SetUp) {  // (0x3d4308 + the activate 0x3d47b0)
        for (uint32_t o = 0; o < 0x20; o += 4) sw(p.b + pv::kHit + o, 0);
        sh(p.b + pv::kHit + 0x14, 0xffff);
        sh(p.b + pv::kHit + 0x16, 0xffff);
        p.f(pv::kHit, health_most());
        p.i(pv::kThirds, 0);
        p.i(pv::kRounds, 0);
        p.i(pv::kSinceHit, 0);
        p.i(pv::kSide, -1);
        p.i(pv::kSwingHit, 0);
        p.i(pv::kDownFlag, 0);
        p.f(pv::kSpeed, 0.0f);
        p.f(pv::kVz, 0.0f);
        p.f(pv::kVx, 0.0f);
        p.f(pv::kVy, 0.0f);
        p.f(pv::kTurnW, 0.0f);
        const float a = (20.0f + rac2::rand_unit() * 20.0f) * kDeg;
        p.f(pv::kAngle, rac2::rand_below(2) == 0 ? a : -a);
        swc1(m + rac2::moby::kPos, kEntry.x);
        swc1(m + rac2::moby::kPos + 4, kEntry.y);
        swc1(m + rac2::moby::kPos + 8, kEntry.z);
        const Vec3 hero = rac2::hero_pos();
        p.f(pv::kHeading, std::atan2(hero.y - kEntry.y, hero.x - kEntry.x));
        swc1(m + rac2::moby::kRot + 8, p.f(pv::kHeading));
        p.v(pv::kCarrot, kEntry);
        if (lwu(m + 0x94) != 0) sh(m + rac2::moby::kFlags, lhu(m + rac2::moby::kFlags) | 0x4000u);  // (weapons reach it)
        chain_spawn(m, p);
        rac2::seq_play(m, Stand, 1);
        mover(p, 0.00444f, 0.1333f, 0.00844f, 0.2094f);
        p.i(pv::kPhase, 1);
        if (p.i(pv::kWaitFirst) != 0) {
            set_state(m, p, Waiting);
        } else {
            set_state(m, p, Entering);
        }
    }
    const bool entering = p.i(pv::kEntered) == 0;
    p.i(pv::kEntered, 1);
    const Vec3 at = rac2::moby_pos(m), hero = rac2::hero_pos();
    const float to_hero = dist2d(at, hero);
    const float bearing = std::atan2(hero.y - at.y, hero.x - at.x);
    const float t = rac2::seq_time(m);
    rac2::test_hit(m);
    hit_taken(m, p);
    p.i(pv::kRounds, p.i(pv::kRounds) + 1);
    p.i(pv::kSinceHit, std::min(p.i(pv::kSinceHit) + 1, 100000));
    switch (p.i(pv::kState)) {
    case Entering:  // (state 2: it stands at the entry, then floats in; within 18 of the middle it comes down)
        if (frames(p) < kStandFirst) {
            face(m, p, bearing);
            break;
        }
        if (rac2::seq_now(m) != Float) rac2::seq_play(m, Float, 10);
        float_to(m, p, Vec3{kMiddle.x, kMiddle.y, kEntry.z}, std::atan2(kMiddle.y - at.y, kMiddle.x - at.x));
        if (dist2d(at, kMiddle) < kRingOut) set_state(m, p, Landing);
        break;
    case TagOut:  // (state 3: its partner's turn; it floats back to its entry point, facing Ratchet)
        if (entering) {
            rac2::seq_play(m, Float, 10);
            p.v(pv::kCarrot, at);
            p.f(pv::kSpeed, 0.0f);
            p.i(pv::kGo, 0);
            p.i(pv::kPhase, 2);
            chain_stow(m, p);
            hand_over(p);
        }
        float_to(m, p, kEntry, bearing);
        if (dist2d(rac2::moby_pos(m), kEntry) < 0.5f) set_state(m, p, Waiting);
        break;
    case Waiting:  // (state 4: at its entry point until its turn, then it comes in again)
        if (entering) rac2::seq_play(m, Stand, 15);
        float_to(m, p, kEntry, bearing);
        if (p.i(pv::kGo) != 0) {
            p.i(pv::kRounds, 0);
            set_state(m, p, Entering);
        }
        break;
    case Landing:  // (state 20: down onto the floor, the slam)
        if (entering) {
            p.f(pv::kSpeed, 0.0f);
            chain_stow(m, p);
        }
        if (rac2::seq_now(m) != Land) {
            const bool landed = walk_to(m, p, at, false) || at.z <= rac2::ground_under(m, kFloor - 30.0f) + 0.01f;
            if (landed) {
                rac2::seq_play(m, Land, 4);
                p.i(pv::kThrowAt, static_cast<int32_t>(rac2::level_frame()));
            }
            face(m, p, bearing);
            break;
        }
        walk_to(m, p, at, false);
        if (t >= 8.0f && t <= 10.0f) rac2::camera_shake(0.2f, 25);
        if (frames(p) > 3 && rac2::seq_ended(m)) {
            p.i(pv::kPhase, 1);
            p.i(pv::kRounds, 0);
            mover(p, 0.00444f, 0.1333f, 0.00844f, 0.2094f);
            if (p.i(pv::kDownFlag) != 0) {  // (knocked out of the air: down a while first)
                rac2::seq_play(m, Down, 3);
                set_state(m, p, Knocked);
            } else {
                set_state(m, p, Chasing);
            }
        }
        break;
    case Chasing: {  // (state 6)
        if (entering) {
            rac2::seq_play(m, Run, 10);
            mover(p, 0.00444f, 0.1333f, 0.00844f, 0.2094f);
        }
        if (dist2d(hero, kMiddle) < kTauntOut) walk_to(m, p, hero, true);
        else walk_to(m, p, at, false), face(m, p, bearing);
        if (rac2::seq_now(m) == Run && ((t > 1.0f && t < 3.0f) || (t > 16.0f && t < 18.0f))) rac2::camera_shake(0.05f, 25);
        if (rac2::rand_below(400) == 0 || (dist2d(hero, kMiddle) >= kTauntOut && rac2::rand_below(60) == 0)) {
            taunt(m, p, Chasing);
            break;
        }
        if (to_hero < kSwingRange && std::fabs(angle_to(bearing, p.f(pv::kHeading))) < kSwingFacing && t > 3.0f && t < 15.0f) {
            rac2::seq_play(m, Swing, 10);
            p.i(pv::kSwingHit, 0);
            set_state(m, p, Swinging);
        }
        break;
    }
    case Swinging:  // (state 7: the right blade at times 19..23, the left at 32..36)
        walk_to(m, p, at, false);
        if (rac2::seq_now(m) == Swing) {
            if (t < 19.0f) face(m, p, bearing);
            if (t > 19.0f && t < 23.0f) swing_hurt(m, p, 0);
            if (t > 32.0f && t < 36.0f) swing_hurt(m, p, 1);
        }
        if (frames(p) > 3 && rac2::seq_ended(m)) {
            if (round_over(m, p) || tag_due(p)) {  // (the tag team: it rises first, then leaves from the air)
                rac2::seq_play(m, Roar, 10);
                set_state(m, p, Roaring);
            } else if (rac2::rand_below(3) == 0) {
                taunt(m, p, Chasing);
            } else {
                set_state(m, p, Chasing);
            }
        }
        break;
    case Taunting:  // (state 10: back to what it did)
        if (p.i(pv::kPhase) == 2) float_to(m, p, at, bearing);
        else walk_to(m, p, at, false), face(m, p, bearing);
        if (frames(p) > 3 && rac2::seq_ended(m)) {
            // (ours: Ratchet out of its reach on the floor past its round, it rises and throws; RAC2 taunts on, a stand-off)
            if (p.i(pv::kPhase) == 1 && dist2d(hero, kMiddle) >= kTauntOut && p.i(pv::kRounds) > static_cast<int32_t>(kGroundRounds)) {
                rac2::seq_play(m, Roar, 10);
                set_state(m, p, Roaring);
            } else {
                set_state(m, p, p.i(pv::kBack));
            }
        }
        break;
    case Roaring:  // (state 19: from its time 9 it rises)
        if (entering) rac2::seq_play(m, Roar, 10);
        if (t > 9.0f && p.i(pv::kPhase) != 2) {
            p.i(pv::kPhase, 2);
            p.i(pv::kRounds, 0);
            p.v(pv::kCarrot, at);
        }
        if (p.i(pv::kPhase) == 2) float_to(m, p, Vec3{at.x, at.y, kFloor + 3.0f}, bearing);
        else walk_to(m, p, at, false), face(m, p, bearing);
        if (frames(p) > 3 && rac2::seq_ended(m)) {
            p.i(pv::kThrowAt, static_cast<int32_t>(rac2::level_frame()) + 60);
            set_state(m, p, Floating);
        }
        break;
    case Floating: {  // (state 11)
        if (entering) {
            rac2::seq_play(m, Float, 10);
            p.f(pv::kSpeed, 0.0f);
        }
        float_to(m, p, carrot(m, p), bearing);
        if (tag_due(p)) {  // (0x3d3a68: its partner's turn)
            set_state(m, p, TagOut);
            break;
        }
        if (round_over(m, p)) {
            set_state(m, p, Landing);
            break;
        }
        if (rac2::rand_below(400) == 0) {
            taunt(m, p, Floating);
            break;
        }
        if (to_hero > kThrowMin && to_hero < kThrowMax && rac2::level_frame() >= static_cast<uint32_t>(p.i(pv::kThrowAt))) {
            const bool right = rac2::rand_below(2) == 0;
            rac2::seq_play(m, right ? ThrowR : ThrowL, 7);
            set_state(m, p, right ? ThrowingR : ThrowingL);
        }
        break;
    }
    case ThrowingR:
    case ThrowingL: {  // (states 12 / 15: to its throwing range, the wind-up, at time 19 the blade goes)
        const bool right = p.i(pv::kState) == ThrowingR;
        // (0x3d52a8: the height and distance at which the blade, thrown down at 1.15 rad from its hands, lands on him)
        const float reach = (kHoverMax + kHandHeight) / std::tan(kThrowAngle);
        const float h = std::clamp(to_hero * std::tan(kThrowAngle) - kHandHeight, 0.0f, kHoverMax);
        Vec3 want{at.x, at.y, kFloor + h};
        if (to_hero > reach + 1.0f) {
            const float k = (to_hero - reach) / to_hero;
            want.x += (hero.x - at.x) * std::min(k, 1.0f);
            want.y += (hero.y - at.y) * std::min(k, 1.0f);
        }
        float_to(m, p, want, bearing);
        if (p.i(pv::kSide) < 0 && rac2::seq_now(m) == (right ? ThrowR : ThrowL) && t > 19.0f) blade_throw(m, p, right ? 0 : 1);
        const int r = blade_step(m, p);
        if (p.i(pv::kSide) >= 0 && r == 1) set_state(m, p, right ? PullingR : PullingL);
        else if (p.i(pv::kSide) >= 0 && rac2::seq_ended(m) && t > 27.0f) set_state(m, p, right ? HoldingR : HoldingL);
        else if (p.i(pv::kSide) < 0 && frames(p) > 3 && rac2::seq_ended(m)) set_state(m, p, Floating);  // (it never threw)
        break;
    }
    case HoldingR:
    case HoldingL: {  // (states 13 / 16)
        const bool right = p.i(pv::kState) == HoldingR;
        if (entering) rac2::seq_play(m, right ? HoldR : HoldL, 7);
        float_to(m, p, at, bearing);
        if (blade_step(m, p) >= 1) set_state(m, p, right ? PullingR : PullingL);
        break;
    }
    case PullingR:
    case PullingL: {  // (states 14 / 17: the chain back, then floating again)
        const bool right = p.i(pv::kState) == PullingR;
        if (entering) rac2::seq_play(m, right ? PullR : PullL, 7);
        float_to(m, p, at, bearing);
        if (p.i(pv::kSide) >= 0 && blade_step(m, p) == 2) chain_stow(m, p);
        if (p.i(pv::kSide) < 0 && frames(p) > 3 && rac2::seq_ended(m) && t > 6.0f) {
            p.i(pv::kThrowAt, static_cast<int32_t>(rac2::level_frame()) + 60 + rac2::rand_below(90));
            set_state(m, p, Floating);
        }
        break;
    }
    case Falling:  // (state 18: knocked out of the air, down onto the floor, then lands as it comes down)
        if (walk_to(m, p, at, false) || at.z <= rac2::ground_under(m, kFloor - 30.0f) + 0.01f) {
            if (p.f(pv::kHit) <= 0.0f) {
                rac2::seq_play(m, Down, 3);
                set_state(m, p, Knocked);
            } else {
                p.i(pv::kDownFlag, 1);
                set_state(m, p, Landing);
            }
        }
        break;
    case Knocked:  // (state 8: a stagger, or down; no health: it lies, the floor shaken, then state 9)
        walk_to(m, p, at, false);
        if (p.f(pv::kHit) <= 0.0f) {
            if (t > 95.0f && t < 97.0f) rac2::camera_shake(0.4f, 25);
            if (t > kDyingDown || (frames(p) > 3 && rac2::seq_ended(m))) set_state(m, p, Dying);
            break;
        }
        if (frames(p) > 3 && rac2::seq_ended(m)) {
            p.i(pv::kDownFlag, 0);
            p.i(pv::kPhase, 1);
            if (round_over(m, p)) {
                rac2::seq_play(m, Roar, 10);
                set_state(m, p, Roaring);
            } else {
                set_state(m, p, Chasing);
            }
        }
        break;
    case Dying:  // (state 9: 61 frames, then it blows up)
        if (entering) hand_over(p);  // (0x3d4658: its partner's turn)
        walk_to(m, p, at, false);
        if (frames(p) >= kDyingGone) {
            die(m, p);
            return;
        }
        break;
    default: set_state(m, p, Chasing);
    }
    // (its chain out: the blade's frame where a state above did not step it)
    if (p.i(pv::kSide) >= 0) {
        const int32_t s = p.i(pv::kState);
        if (s != ThrowingR && s != ThrowingL && s != HoldingR && s != HoldingL && s != PullingR && s != PullingL) chain_stow(m, p);
    }
    if (trace() && (rac2::level_frame() % 60) == 0)
        std::printf("[2552] frame %u: %#x state %d phase %d seq %u time %.1f at %.1f %.1f %.1f, Ratchet %.1f away, health %.1f\n", rac2::level_frame(), m,
                    p.i(pv::kState), p.i(pv::kPhase), rac2::seq_now(m), static_cast<double>(rac2::seq_time(m)), static_cast<double>(at.x),
                    static_cast<double>(at.y), static_cast<double>(at.z), static_cast<double>(to_hero), static_cast<double>(p.f(pv::kHit)));
}

}  // namespace

// orig: (none) — see rac2_enemies.h
void rac2_boss_2552_update(uint32_t m) { update(m); }

// orig: (none) — see rac2_enemies.h (RAC2's 0x3d4980)
void rac2_boss_2552_pair(uint32_t m, uint32_t partner, bool waits) {
    const uint32_t b = lwu(m + rac2::moby::kPvars);
    if (b == 0) return;
    sw(b + pv::kPartner, partner);
    sw(b + pv::kPartnerCls, partner != 0 ? static_cast<uint32_t>(lh(partner + rac2::moby::kClassId)) : 0u);
    sw(b + pv::kPartnerPv, partner != 0 ? lwu(partner + rac2::moby::kPvars) : 0u);
    sw(b + pv::kWaitFirst, waits ? 1u : 0u);
}

// orig: (none) — see rac2_enemies.h (RAC2's 0x3d4990: its thirds of health lost counted up to now, its flag to come in)
void rac2_boss_2552_turn(uint32_t m) {
    const uint32_t b = lwu(m + rac2::moby::kPvars);
    if (b == 0 || lw(b + pv::kGo) != 0) return;
    const float health = lw(b + pv::kState) == SetUp ? health_most() : lwc1(b + pv::kHit);
    int32_t n = lw(b + pv::kTurns) + 1;
    while (n < 3 && health < health_most() * static_cast<float>(3 - n) / 3.0f) ++n;
    sw(b + pv::kTurns, static_cast<uint32_t>(n));
    sw(b + pv::kGo, 1);
    if (trace()) std::printf("[2552] frame %u: %#x its turn (thirds %d)\n", rac2::level_frame(), m, n);
}

// orig: (none) — see rac2_enemies.h
bool rac2_boss_2552_fighting(uint32_t m) {
    const uint32_t b = lwu(m + rac2::moby::kPvars);
    return b != 0 && lw(b + pv::kState) >= TagOut;
}

// orig: (none) — see rac2_enemies.h
void rac2_boss_2552_health(uint32_t m, float& now, float& most) {
    const uint32_t b = m != 0 ? lwu(m + rac2::moby::kPvars) : 0u;
    now = b != 0 && lw(b + pv::kState) != SetUp ? std::max(0.0f, lwc1(b + pv::kHit)) : health_most();
    most = health_most();
}

}  // namespace rac1
