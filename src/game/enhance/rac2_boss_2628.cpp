// See rac2_enemies.h. RAC2's 2628, the boss B2 Brawler: a heavy four-limbed walker (2628) with a turret on its back (RAC2's
// 2635, its pvars +0x244: a second moby the walker carries on its attachment point 0 and keeps in step, each of its own
// sequences matched by one of the turret's) whose two arm guns fire shells (3336) at Ratchet. It jumps into the arena from
// its perch over the south gate, walks at Ratchet and shoots, now and then winds up and spins with its limbs out, sliding
// at him; hurt, it spins faster and its turret turns faster. Written for RAC1's engine after RAC2's level code (its update
// 0x3d7c20, set-up and hits 0x3d91a0, its moves by sequence 0x3d9728, the turret and its guns 0x3d9e68, the spin's hurt
// 0x3dab68, activate 0x3db310, park 0x3db6d8, the tag team's turns 0x3db880 / 0x3db950, the shell 0x4216f0 / 0x421860), read
// as the reference; RAC2's state numbers and sequences kept.
//
// What RAC2's B2 Brawler does, and ours:
// - set up (0x3d91a0 + the activate 0x3db310): its health from its pvars (+0x24: 100), the shell's and the spin's damage
//   (+0x34, +0x35: 1 each); placed on its perch (gp + 0x1400: 500, 558, 507.5, over the south gate), facing Ratchet, its
//   turret on it, the piece 2636 on the turret's attachment point 4; then state 2 (it comes in), or 4 (it waits on its
//   perch: the tag team's second, the controller's 0x3fac00).
// - coming in (state 2): 61 frames on the perch, then the jump (sequence 3, the turret's 6; from its time 23 it flies:
//   0x3329c8 at 0.333 a frame across, falling at 0.0139 a frame each frame, to the point 30 towards the arena's middle,
//   500, 588, 500), in the air (4 / 7), the landing (5 / 8 from its frame 3: its sound 0 at time 2; the camera shaken
//   0.35 when it ends), then state 5. Hits do nothing until then (states < 5).
// - walking (state 5): sequence 1 (the turret's 1) towards Ratchet (its mover 0.00167 / 0.0517 a frame, turning 0.0035 /
//   0.105; the camera shaken 0.05 at its footfalls, times 0..2, 15..17, 30..32, 45..47); him more than 45 degrees off its
//   facing: it turns on the spot (2 / 3, the turns at times 12..25 and 42..55, shaken 0.1 at 0..2 and 29..31) until he is
//   within 10; within 10 of him it stands (0), beyond 20 it walks again. Every 600..1200 frames (+0x288) its special
//   (state 6); Ratchet hurt (by anything: RAC2's 0x18c2bc): it gloats (state 7).
// - the turret (0x3d9e68): put on the walker's attachment point 0 each frame, turned towards Ratchet on its own (its yaw
//   eased at (360 - 330 h) x 4.85e-6 a frame each frame up to (240 - 225 h) x 2.9e-4 a frame, h its health's share:
//   faster when hurt); walking (or spinning below a fifth of its health), Ratchet within 45: every 20 + 40 h frames (60
//   while spinning) the turret fires (its sequence 5): at its times 3..6 a shell from each gun (its attachment points 0
//   and 1, 0x4216f0): 40 units a second, 100 units at most, blowing up on what it meets (a 0.8 blast: the shell's damage).
// - the special (state 6): wind-up (6 / 9: the camera shaken 0.2 at its times 20..22), from its time 28 the spin (7 / 10,
//   its looping sound 6): it turns (160 - 90 h) / 60 degrees a frame and, Ratchet farther than 12, slides at him at
//   (3 - 2.2 h) / 60 a frame; its four limbs (attachment points 1-2, 3-4, 5-6, 7-8) hurt him (0x3dab68: a 0.65 sphere at
//   each limb's point nearest him, a 0.95 at its end, the spin's damage, pushed round); after another 600..1200 frames
//   it stops (8 / 11, shaken 0.2 at 30..32), and from its time 45 walks again.
// - the gloat (state 7): the turret's spin pose (10) for 181 frames, then the special (spinning on if it already was).
// - hit (0x3d91a0): from state 5 on, not dying: its health less the damage (RAC2's own weapons 3209 and 3070 at 0.35);
//   more than 3 at once while spinning: the turret knocked round (state 9: its sequence 12, spinning freely, smoking), then
//   back to what it did. No health: it falls (state 10: 9 / 13, the turret spun round), lies still (its spin pose, 7),
//   and when the turret's falls ends (state 11) the controller counts it (KILL_COUNT), the piece is thrown off, its sound
//   5; 181 frames later explosions at it and its limbs (0x41f230), its sound 5 again, gone.
// - the tag team (challenge 9: Chainblade fights first, the B2 Brawler waits on its perch): a third more of its health
//   lost (0x3db950: below (3 - its turns) / 3 of its most) while its partner is up, it walks out (state 3: to 500, 588,
//   500), jumps back onto its perch (3 / 4 / 5) and waits (state 4) while its partner (0x3d4990: its turn, a third more)
//   fights; given its turn back (0x3db880), it comes in again (state 2). The controller's bar shows the one fighting.
// Ours vs RAC2's: no path-finding round the arena's middle (0x32f608: straight at him, RAC1's walker_move stops it at the
// walls); the turn on the spot turns in place (RAC2 pivots it about its planted foot, points 4 / 8); the guns' arm joints
// not aimed (RAC2 turns joints 2 and 3 of the turret, 0x3152d0: ours the turret's yaw only), each shell aimed straight at
// Ratchet from its gun; the turret's glows (0x3dad88) and its colour pulse not drawn; state 8 (a stagger back into a
// saved state: nothing in the code read sets it) not ported; the camera's swap and the announcer's line at a tag (0x3fc020,
// 0x400650) only the bar's swap.
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

constexpr int32_t kTurretClass = 2029, kShellClass = 2028, kPieceClass = 2027;  // (RAC2's 2635, 3336, 2636: maktar.py)
constexpr float kPi = 3.14159265f, kDeg = kPi / 180.0f;
constexpr float kFloor = 500.0f;
constexpr Vec3 kMiddle{500.0f, 600.0f, 500.0f};  // (the controller's middle, 0x3fc098)
constexpr Vec3 kPerch{500.0f, 558.0f, 507.5f};   // (gp + 0x1400)
constexpr Vec3 kOut{500.0f, 588.0f, 500.0f};     // (gp + 0x1410: where it walks to before jumping back onto its perch)
constexpr float kHealth = 100.0f;                // (its pvars +0x24)
constexpr float kShellDamage = 1.0f, kSpinDamage = 1.0f;  // (its pvars +0x34, +0x35)
constexpr float kJumpSpeed = 0.3333f, kGravity = 0.01389f, kJumpReach = 30.0f;  // (0x3329c8's speed, its pvars +0x7c)
constexpr float kFireRange = 45.0f;
constexpr float kShellSpeed = 40.0f / 60.0f, kShellRange = 100.0f, kShellBlast = 0.8f;  // (gp + 0x2fb0 / 0x2fb4)
constexpr uint32_t kComeIn = 61, kGloat = 181, kDyingGone = 181, kFirstShot = 60;

// RAC2's sequences of the walker, and of the turret
enum Seq : uint32_t { Stand = 0, Walk = 1, Turn = 2, Jump = 3, Air = 4, Land = 5, WindUp = 6, Spin = 7, SpinEnd = 8, Fall = 9 };
enum TurretSeq : uint32_t { TIdle = 0, TAim = 1, TFire = 5, TKnocked = 12, TFall = 13 };
// the turret's sequence for each of the walker's (RAC2 sets them together: 0 -> 0, 1 -> 1, 2 -> 3, 3 -> 6, ...)
constexpr uint32_t kTurretOf[10] = {0, 1, 3, 6, 7, 8, 9, 10, 11, 13};
// RAC2's states
enum State : int32_t { SetUp = 0, Entering = 2, TagOut = 3, Waiting = 4, Walking = 5, Special = 6, Gloating = 7, Knocked = 9, Falling = 10, Dying = 11 };

// Our pvars (a block of the stand-ins' pool), our own layout
namespace pv {
constexpr uint32_t kState = 0x00, kSince = 0x04, kEntered = 0x08, kSub = 0x0c;  // (kSub: RAC2's substate, moby +0xbc)
constexpr uint32_t kHit = 0x10, kTurretHit = 0x30;  // RAC1's hit state blocks (0x20 bytes each): the walker's, the turret's
constexpr uint32_t kHealthNow = 0x50, kSpeed = 0x54, kHeading = 0x58, kTurnW = 0x5c;
constexpr uint32_t kVel = 0x60;                      // (x y z: a jump's, RAC2's +0xf0)
constexpr uint32_t kYaw = 0x70, kYawW = 0x74;        // (the turret's yaw and its turning, RAC2's +0x248 / +0x24c)
constexpr uint32_t kSpecial = 0x78, kCooldown = 0x7c;  // (RAC2's +0x288, +0x286)
constexpr uint32_t kTurret = 0x80, kPiece = 0x84, kFired = 0x88;
constexpr uint32_t kSaved = 0x8c, kSavedSub = 0x90, kSavedTurret = 0x94;  // (RAC2's +0x28a, +0x28e, +0x28c)
constexpr uint32_t kSounded = 0x98, kHeroLast = 0x9c, kSpinHit = 0xa0;
constexpr uint32_t kPartner = 0xb0, kPartnerCls = 0xb4, kPartnerPv = 0xb8, kGo = 0xbc, kTurns = 0xc0, kWaitFirst = 0xc4;  // (+0x264, +0x280, +0x282)
constexpr uint32_t kScratched = 0xc8;  // (P8: Ratchet hurt by it, its turret or its shells: RAC2's +0x290, its skill point lost)
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
// orig: (none)
float dist3d(const Vec3& a, const Vec3& b) { return std::hypot(std::hypot(a.x - b.x, a.y - b.y), a.z - b.z); }
// orig: (none) — a - b as an angle in -pi .. pi
float angle_to(float a, float b) {
    float d = std::fmod(a - b, 2.0f * kPi);
    if (d > kPi) d -= 2.0f * kPi;
    if (d < -kPi) d += 2.0f * kPi;
    return d;
}

// orig: (none) — its health's share (RAC2's +0x20 / +0x24)
float share(const Pv& p) { return std::clamp(p.f(pv::kHealthNow) / health_most(), 0.0f, 1.0f); }

// orig: (none) — a new state (RAC2's STATE_SET: the substate 0, its first frame next)
void set_state(uint32_t m, const Pv& p, int32_t s) {
    if (trace()) std::printf("[2628] frame %u: %#x state %d -> %d (health %.1f)\n", rac2::level_frame(), m, p.i(pv::kState), s, static_cast<double>(p.f(pv::kHealthNow)));
    p.i(pv::kState, s);
    p.i(pv::kSince, static_cast<int32_t>(rac2::level_frame()));
    p.i(pv::kEntered, 0);
    p.i(pv::kSub, 0);
}
// orig: (none)
uint32_t frames(const Pv& p) { return rac2::level_frame() - static_cast<uint32_t>(p.i(pv::kSince)); }

// orig: (none) — its turret, while it is the one it spawned
uint32_t turret_of(uint32_t m, const Pv& p) {
    const uint32_t t = static_cast<uint32_t>(p.i(pv::kTurret));
    return rac2::part_of(t, m) ? t : 0u;
}

// orig: (none) — the walker's sequence and the turret's with it (RAC2 sets the turret's after each of its own)
void seqs(uint32_t m, const Pv& p, uint32_t s, int32_t blend, int32_t from = 0) {
    rac2::seq_play(m, s, blend, from);
    if (const uint32_t t = turret_of(m, p); t != 0 && s < 10) rac2::seq_play(t, kTurretOf[s], 10);
}

// orig: (none) — its facing turned towards `yaw` (the turning speeds up by 0.0035 a frame to 0.105: its mover's)
void face(uint32_t m, const Pv& p, float yaw) {
    const float d = angle_to(yaw, p.f(pv::kHeading));
    const float w = std::min({p.f(pv::kTurnW) + 0.00349f, 0.1047f, std::fabs(d)});
    p.f(pv::kTurnW, std::fabs(d) < 1e-3f ? 0.0f : w);
    p.f(pv::kHeading, p.f(pv::kHeading) + std::copysign(w, d));
    swc1(m + rac2::moby::kRot + 8, p.f(pv::kHeading));
}

// orig: (none) — a frame on its feet (RAC2's mover 0x3313e8 at 0.00167 / 0.0517 a frame), towards `to` or (`moving`
// false) slowing to a stop, the walls stopping it; on the floor
void walk(uint32_t m, const Pv& p, const Vec3& to, bool moving) {
    const Vec3 at = rac2::moby_pos(m);
    const float d = moving ? dist2d(at, to) : 0.0f;
    const float acc = 0.001667f;
    const float want = std::min(0.05167f, std::sqrt(2.0f * acc * std::max(0.0f, d - 0.3f)));
    float speed = p.f(pv::kSpeed);
    speed = speed < want ? std::min(want, speed + acc) : std::max(want, speed - acc);
    p.f(pv::kSpeed, speed);
    if (moving && d > 1e-3f) face(m, p, std::atan2(to.y - at.y, to.x - at.x));
    const float h = p.f(pv::kHeading);
    Vec4 step{std::cos(h) * speed, std::sin(h) * speed, 0.0f, 0.0f};
    if (speed > 1e-5f) walker_move_v(m, step, 0, 1.0f, 1.5f, 0.0f);
    const float ground = rac2::ground_under(m, kFloor - 30.0f);
    if (ground > kFloor - 30.0f) swc1(m + rac2::moby::kPos + 8, ground);
}

// orig: (none) — a jump to `to` (RAC2's 0x3329c8): across at 0.333 a frame, its rise such that it falls on `to`
void jump_at(const Pv& p, const Vec3& from, const Vec3& to) {
    const float d = dist2d(from, to);
    const float t = std::max(1.0f, d / kJumpSpeed);
    const float k = d > 1e-4f ? kJumpSpeed / d : 0.0f;
    p.v(pv::kVel, Vec3{(to.x - from.x) * k, (to.y - from.y) * k, (to.z - from.z) / t + 0.5f * kGravity * t});
}

// orig: (none) — a frame of flight (RAC2's 0x3d9b64: facing its way, falling, moved); not below the floor
void fly(uint32_t m, const Pv& p) {
    Vec3 v = p.v(pv::kVel);
    if (std::hypot(v.x, v.y) > 1e-4f) face(m, p, std::atan2(v.y, v.x));
    v.z -= kGravity;
    Vec3 at = rac2::moby_pos(m);
    at = Vec3{at.x + v.x, at.y + v.y, at.z + v.z};
    if (v.z < 0.0f && at.z <= kFloor) {
        at.z = kFloor;
        v = Vec3{};
    }
    p.v(pv::kVel, v);
    swc1(m + rac2::moby::kPos, at.x);
    swc1(m + rac2::moby::kPos + 4, at.y);
    swc1(m + rac2::moby::kPos + 8, at.z);
}

// orig: (none) — its four limbs' hurt while it spins (RAC2's 0x3dab68): each limb from attachment point 2k + 1 to 2k + 2; a
// 0.65 sphere at its point nearest Ratchet, a 0.95 one at its end; pushed round, the spin's way
void spin_hurt(uint32_t m, const Pv& p) {
    if (p.i(pv::kSpinHit) > 0) {
        p.i(pv::kSpinHit, p.i(pv::kSpinHit) - 1);
        return;
    }
    Vec3 pts[8];
    rac2::joints_world(m, 1, 8, pts);
    const Vec3 hero = rac2::hero_pos();
    const Vec3 h{hero.x, hero.y, hero.z + 0.6f};
    const Vec3 at = rac2::moby_pos(m);
    for (int k = 0; k < 4; ++k) {
        const Vec3 a = pts[2 * k], b = pts[2 * k + 1];
        const Vec3 ab{b.x - a.x, b.y - a.y, b.z - a.z};
        const float ll = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
        const float t = ll > 1e-6f ? std::clamp(((h.x - a.x) * ab.x + (h.y - a.y) * ab.y + (h.z - a.z) * ab.z) / ll, 0.0f, 1.0f) : 0.0f;
        const Vec3 q{a.x + ab.x * t, a.y + ab.y * t, a.z + ab.z * t};
        if (dist3d(h, q) >= 0.65f + 0.6f && dist3d(h, b) >= 0.95f + 0.6f) continue;
        // (the push: across the limb, round the spin: RAC2's (-y, x) of where it reaches from its middle, length 2)
        Vec4 push{-(q.y - at.y), q.x - at.x, 0.0f, 0.0f};
        if (vec3_length(push) < 1e-4f) push = Vec4{h.x - at.x, h.y - at.y, 0.0f, 0.0f};
        vec3_set_length(push, push, 2.0f);
        rac2::hero_hit(m, Vec3{push.x, push.y, 0.5f}, kSpinDamage);
        p.i(pv::kSpinHit, 30);
        if (trace()) std::printf("[2628] frame %u: %#x spins into Ratchet (nanotech %d)\n", rac2::level_frame(), m, rac2::hero_health());
        return;
    }
}

// orig: (none) — a shell from the turret's gun `gun` (its attachment point 0 or 1: RAC2's 0x4216f0) at Ratchet
void shell_fire(uint32_t m, uint32_t t, uint32_t gun) {
    Vec3 muzzle, x, y, z;
    rac2::point_frame(t, gun, muzzle, x, y, z);
    const Vec3 hero = rac2::hero_pos();
    Vec3 d{hero.x - muzzle.x, hero.y - muzzle.y, hero.z + 0.8f - muzzle.z};
    const float len = std::max(0.01f, std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z));
    d = Vec3{d.x / len, d.y / len, d.z / len};
    rac2::Shot s;
    s.cls = kShellClass;
    s.at = Vec3{muzzle.x + d.x * 0.5f, muzzle.y + d.y * 0.5f, muzzle.z + d.z * 0.5f};
    s.vel = Vec3{d.x * kShellSpeed, d.y * kShellSpeed, d.z * kShellSpeed};
    s.life = static_cast<uint32_t>(kShellRange / kShellSpeed);
    s.damage = kShellDamage;
    s.blast = kShellBlast;
    s.sound_fire = 1;  // (its class sounds: 1 fired, 0 blown up; RAC2's 139 / 140 of the level's bank)
    s.sound_hit = 0;
    const uint32_t w = rac2::shot_fire(m, s);
    if (trace()) std::printf("[2628] frame %u: %#x fires a shell from gun %u (%#x), %.1f to Ratchet\n", rac2::level_frame(), m, gun, w, static_cast<double>(len));
}

// orig: (none) — the turret's frame (RAC2's 0x3d9e68): on the walker's attachment point 0, its yaw towards Ratchet (not
// while gloating, knocked or falling: then it spins freely, slowing), its guns
void turret_step(uint32_t m, const Pv& p, float to_hero) {
    const uint32_t t = turret_of(m, p);
    if (t == 0) return;
    const int32_t s = p.i(pv::kState);
    Vec3 at, x, y, z;
    rac2::point_frame(m, 0, at, x, y, z);
    const Vec3 hero = rac2::hero_pos();
    const float bearing = std::atan2(hero.y - at.y, hero.x - at.x);
    float yaw = p.f(pv::kYaw), w = p.f(pv::kYawW);
    if (s == Gloating || s == Knocked || s == Falling || s == Dying) {
        if (s != Dying) {  // (0x3daa88 / 0x3da9cc: spun round, slowing)
            yaw += w;
            w *= 0.97f;
        }
    } else {  // (0x322330: eased towards him, faster when hurt)
        const float h = share(p);
        const float acc = (360.0f - 330.0f * h) * 4.848e-6f, top = (240.0f - 225.0f * h) * 2.909e-4f;
        const float d = angle_to(bearing, yaw);
        const float stop = w * w / (2.0f * acc);  // (how far it turns while slowing down)
        const float want = std::fabs(d) <= stop ? 0.0f : std::copysign(top, d);
        w = w < want ? std::min(want, w + acc) : std::max(want, w - acc);
        if (std::fabs(d) < std::fabs(w)) w = d;
        yaw += w;
    }
    yaw = angle_to(yaw, 0.0f);
    p.f(pv::kYaw, yaw);
    p.f(pv::kYawW, w);
    rac2::part_put(t, at, Vec3{std::cos(yaw), std::sin(yaw), 0.0f}, (lhu(m + rac2::moby::kFlags) & 1u) == 0);
    if (const uint32_t piece = static_cast<uint32_t>(p.i(pv::kPiece)); rac2::part_of(piece, m)) rac2::part_place(piece, t, 4, 0.0f);
    // the guns: walking, or spinning below a fifth of its health
    const bool firing = s == Walking || (s == Special && share(p) < 0.2f);
    if (firing) {
        int32_t c = p.i(pv::kCooldown);
        if (c > 0) p.i(pv::kCooldown, --c);
        if (c <= 0 && to_hero > 0.0f && to_hero < kFireRange && rac2::seq_now(t) != TFire) {
            rac2::seq_play(t, TFire, 3);
            p.i(pv::kFired, 0);
            p.i(pv::kCooldown, s == Walking ? static_cast<int32_t>(20.0f + 40.0f * share(p)) : 60);
        }
    }
    // the turret's own sequences (the table 0x2a5230): aiming within range while walking, its shots at its times 3..6
    const uint32_t ts = rac2::seq_now(t);
    if (ts == TFire) {
        const float tt = rac2::seq_time(t);
        if (p.i(pv::kFired) == 0 && tt > 3.0f && tt < 6.0f) {
            shell_fire(m, t, 0);
            shell_fire(m, t, 1);
            p.i(pv::kFired, 1);
        }
        if (rac2::seq_ended(t) && p.i(pv::kFired) != 0) rac2::seq_play(t, TIdle, 3);
    } else if (s == Walking && (ts == TIdle || ts == TAim)) {
        rac2::seq_play(t, to_hero < kFireRange ? TAim : TIdle, 10);
    }
}

// orig: (none) — Ratchet hurt this frame (RAC2 reads the moby that hurt him, 0x18c2bc: ours his nanotech going down)
bool hero_hurt(const Pv& p) {
    const int32_t now = rac2::hero_health(), last = p.i(pv::kHeroLast);
    p.i(pv::kHeroLast, now);
    return now < last;
}

// orig: (none) — the partner (the tag team's other boss) still in it (RAC2: alive, its state 3 or more)
bool partner_up(const Pv& p) {
    const uint32_t q = static_cast<uint32_t>(p.i(pv::kPartner));
    if (q == 0 || rac2::moby_gone(q, p.i(pv::kPartnerCls), static_cast<uint32_t>(p.i(pv::kPartnerPv)))) return false;
    return rac2_boss_fighting(q);
}

// orig: (none) — time to hand over (RAC2's 0x3db950): the partner up and a third more of its health lost
bool tag_due(const Pv& p) {
    return partner_up(p) && p.f(pv::kHealthNow) < health_most() * static_cast<float>(3 - p.i(pv::kTurns)) / 3.0f;
}

// orig: (none) — the partner's turn (RAC2's 0x3d4990 on it, then 0x3fc020: the controller's one fighting)
void hand_over(const Pv& p) {
    const uint32_t q = static_cast<uint32_t>(p.i(pv::kPartner));
    if (q == 0 || rac2::moby_gone(q, p.i(pv::kPartnerCls), static_cast<uint32_t>(p.i(pv::kPartnerPv)))) return;
    rac2_boss_turn(q);
    custom_arena_boss_active(q);
}

// orig: (none) — a hit taken (RAC1's hit records of the walker and its turret through their reactions; RAC2's 0x3d91a0):
// none before state 5 or while it falls; its health less the damage; more than 3 at once while spinning: the turret knocked
// round; none left: it falls
void hit_taken(uint32_t m, const Pv& p) {
    float dmg = rac2::hit_receive(m, p.b + pv::kHit, nullptr);
    if (const uint32_t t = turret_of(m, p); t != 0) dmg += rac2::hit_receive(t, p.b + pv::kTurretHit, nullptr);
    const int32_t s = p.i(pv::kState);
    if (dmg <= 0.0f || s < Walking || s == Falling || s == Dying) return;
    p.f(pv::kHealthNow, p.f(pv::kHealthNow) - dmg);
    if (trace()) std::printf("[2628] frame %u: %#x hit (damage %.2f, health %.2f)\n", rac2::level_frame(), m, static_cast<double>(dmg), static_cast<double>(p.f(pv::kHealthNow)));
    if (dmg > 3.0f && s == Special && rac2::seq_now(m) == Spin) {
        const uint32_t t = turret_of(m, p);
        p.i(pv::kSaved, s);
        p.i(pv::kSavedSub, p.i(pv::kSub));
        p.i(pv::kSavedTurret, t != 0 ? static_cast<int32_t>(rac2::seq_now(t)) : 0);
        set_state(m, p, Knocked);
        p.f(pv::kYawW, 0.1939f);
    }
    if (p.f(pv::kHealthNow) <= 0.0f) {
        rac2::sound_loop(m, -1);
        p.f(pv::kYawW, 0.3879f);
        set_state(m, p, Falling);
    }
}

// orig: (none) — its set-up and activate (0x3d91a0 state 0, 0x3db310): health, its perch, its turret and piece; it comes in,
// or waits on its perch for its turn (the tag team's second)
void set_up(uint32_t m, const Pv& p) {
    for (uint32_t o = 0; o < 0x20; o += 4) {
        sw(p.b + pv::kHit + o, 0);
        sw(p.b + pv::kTurretHit + o, 0);
    }
    for (uint32_t o : {pv::kHit, pv::kTurretHit}) {
        sh(p.b + o + 0x14, 0xffff);
        sh(p.b + o + 0x16, 0xffff);
    }
    p.f(pv::kHealthNow, health_most());
    p.i(pv::kScratched, 0);
    p.f(pv::kSpeed, 0.0f);
    p.f(pv::kTurnW, 0.0f);
    p.v(pv::kVel, Vec3{});
    p.f(pv::kYawW, 0.0f);
    p.i(pv::kSpecial, 600 + rac2::rand_below(601));
    p.i(pv::kCooldown, static_cast<int32_t>(kFirstShot));
    p.i(pv::kSpinHit, 0);
    p.i(pv::kHeroLast, rac2::hero_health());
    swc1(m + rac2::moby::kPos, kPerch.x);
    swc1(m + rac2::moby::kPos + 4, kPerch.y);
    swc1(m + rac2::moby::kPos + 8, kPerch.z);
    const Vec3 hero = rac2::hero_pos();
    p.f(pv::kHeading, std::atan2(hero.y - kPerch.y, hero.x - kPerch.x));
    p.f(pv::kYaw, p.f(pv::kHeading));
    swc1(m + rac2::moby::kRot + 8, p.f(pv::kHeading));
    if (lwu(m + 0x94) != 0) sh(m + rac2::moby::kFlags, lhu(m + rac2::moby::kFlags) | 0x4000u);  // (weapons reach it)
    const uint32_t t = rac2::part_spawn(m, kTurretClass, 1.0f);
    p.i(pv::kTurret, static_cast<int32_t>(t));
    if (t != 0 && lwu(t + 0x94) != 0) sh(t + rac2::moby::kFlags, lhu(t + rac2::moby::kFlags) | 0x4000u);
    p.i(pv::kPiece, static_cast<int32_t>(t != 0 ? rac2::part_spawn(m, kPieceClass, 1.0f) : 0u));
    if (trace()) std::printf("[2628] frame %u: %#x set up: turret %#x, piece %#x, partner %#x%s\n", rac2::level_frame(), m, t, static_cast<uint32_t>(p.i(pv::kPiece)),
                             static_cast<uint32_t>(p.i(pv::kPartner)), p.i(pv::kWaitFirst) != 0 ? " (waits first)" : "");
    if (p.i(pv::kWaitFirst) != 0) {
        set_state(m, p, Waiting);
    } else {
        p.i(pv::kPartner, 0);
        set_state(m, p, Entering);
    }
    seqs(m, p, Stand, 3);
}

// orig: (none) — it blows up (RAC2's state 11 at 181: explosions at it and its limbs 0x41f230, its sound 5, parked)
void blow_up(uint32_t m, const Pv& p) {
    Vec3 pts[8];
    rac2::joints_world(m, 1, 8, pts);
    explosion_burst(m, m + rac2::moby::kPos, -1, 2.5f, 12.0f);
    for (int k : {1, 5, 3, 7}) {  // (its points 2, 6, 4, 8)
        const uint32_t at = ee_stack_alloc(0x10);
        sw(at, 0);
        swc1(at, pts[k].x);
        swc1(at + 4, pts[k].y);
        swc1(at + 8, pts[k].z);
        swc1(at + 12, 1.0f);
        explosion_burst(m, at, -1, 1.5f, 6.0f);
        ee_stack_free(0x10);
    }
    rac2::camera_shake(0.4f, 25);
    rac2::sound_of_class(m, lh(m + rac2::moby::kClassId), 5);
    if (const uint32_t t = turret_of(m, p)) rac2::moby_kill(t);
    if (trace()) std::printf("[2628] frame %u: %#x blows up\n", rac2::level_frame(), m);
    rac2::moby_kill(m);
}

// orig: (none)
void update(uint32_t m) {
    const Pv p{lwu(m + rac2::moby::kPvars)};
    if (p.b == 0 || static_cast<int8_t>(lbu(m + rac2::moby::kState)) < 0) return;
    if (p.i(pv::kState) == SetUp) set_up(m, p);
    const bool first = p.i(pv::kEntered) == 0;
    p.i(pv::kEntered, 1);
    const Vec3 at = rac2::moby_pos(m), hero = rac2::hero_pos();
    const float to_hero = dist3d(at, hero);
    const float bearing = std::atan2(hero.y - at.y, hero.x - at.x);
    const float t = rac2::seq_time(m);
    const uint32_t seq = rac2::seq_now(m);
    rac2::test_hit(m);
    hit_taken(m, p);
    const bool hurt = hero_hurt(p);
    // (RAC2's 0x3d7cc4: the moby that last hurt Ratchet itself or its shell 3336: no skill point; ours its turret too)
    if (hurt && (rac2::hero_hit_by(m) || rac2::hero_hit_by(turret_of(m, p)))) p.i(pv::kScratched, 1);
    const int32_t sub = p.i(pv::kSub);
    switch (p.i(pv::kState)) {
    case Entering:  // (state 2: on its perch, the jump, in the air, the landing)
        if (first) seqs(m, p, Stand, 3);
        if (sub == 0) {
            face(m, p, bearing);
            if (frames(p) >= kComeIn) {  // (the jump: 30 towards the middle, down on the floor)
                p.i(pv::kSub, 1);
                seqs(m, p, Jump, 10);
                const float d = std::max(0.01f, dist2d(at, kMiddle));
                jump_at(p, at, Vec3{at.x + (kMiddle.x - at.x) * kJumpReach / d, at.y + (kMiddle.y - at.y) * kJumpReach / d, kFloor});
            }
        } else if (sub == 1) {
            if (t >= 23.0f) fly(m, p);
            if (frames(p) > 3 && rac2::seq_ended(m)) {
                p.i(pv::kSub, 2);
                seqs(m, p, Air, 7);
            }
        } else if (sub == 2) {
            fly(m, p);
            if (rac2::moby_pos(m).z < kFloor + 0.05f) {
                p.v(pv::kVel, Vec3{});
                p.i(pv::kSub, 3);
                p.i(pv::kSounded, 0);
                seqs(m, p, Land, 2, 3);
            }
        } else {
            if (t > 2.0f && p.i(pv::kSounded) == 0) {
                p.i(pv::kSounded, 1);
                rac2::sound_of_class(m, lh(m + rac2::moby::kClassId), 0);
            }
            if (t > 3.0f && t < 5.0f) rac2::camera_shake(0.4f, 25);
            if (frames(p) > 3 && rac2::seq_ended(m)) {
                rac2::camera_shake(0.35f, 25);
                seqs(m, p, Stand, 1);
                p.i(pv::kSpecial, 600 + rac2::rand_below(601));
                set_state(m, p, Walking);
                p.i(pv::kSub, 1);
            }
        }
        break;
    case TagOut:  // (state 3: walk out, jump back onto its perch, then wait)
        if (first) {
            seqs(m, p, Walk, 10);
            p.i(pv::kGo, 0);
            p.v(pv::kVel, Vec3{});
            hand_over(p);
        }
        if (sub == 0) {
            walk(m, p, kOut, true);
            if (dist2d(at, kOut) < 0.6f) {
                p.i(pv::kSub, 1);
                p.f(pv::kSpeed, 0.0f);
                seqs(m, p, Jump, 10);
                jump_at(p, at, kPerch);
            }
        } else if (sub == 1) {
            if (t >= 23.0f) fly(m, p);
            if (frames(p) > 3 && rac2::seq_ended(m)) {
                p.i(pv::kSub, 2);
                p.i(pv::kSince, static_cast<int32_t>(rac2::level_frame()));
                seqs(m, p, Air, 10);
            }
        } else if (sub == 2) {
            fly(m, p);
            const Vec3 now = rac2::moby_pos(m);
            if (dist2d(now, kPerch) < 1.0f || frames(p) >= 301 || (p.f(pv::kVel + 8) < 0.0f && now.z <= kPerch.z)) {
                p.v(pv::kVel, Vec3{});
                p.i(pv::kSub, 3);
                seqs(m, p, Land, 10);
            }
        } else if (frames(p) > 3 && rac2::seq_ended(m)) {
            set_state(m, p, Waiting);
        }
        break;
    case Waiting: {  // (state 4: on its perch until its turn: 0x3db880's flag)
        if (first) seqs(m, p, Stand, 10);
        Vec3 now = at;  // (0x321c30: eased onto its perch at 0.1667 a frame)
        const float d = dist3d(now, kPerch);
        const float k = d > 0.1667f ? 0.1667f / d : 1.0f;
        now = Vec3{now.x + (kPerch.x - now.x) * k, now.y + (kPerch.y - now.y) * k, now.z + (kPerch.z - now.z) * k};
        swc1(m + rac2::moby::kPos, now.x);
        swc1(m + rac2::moby::kPos + 4, now.y);
        swc1(m + rac2::moby::kPos + 8, now.z);
        face(m, p, bearing);
        if (p.i(pv::kGo) != 0 && d < 0.001f) {
            set_state(m, p, Entering);
            p.i(pv::kSub, 0);
        }
        break;
    }
    case Walking: {  // (state 5: substate 0 standing, 1 walking, 2 turning on the spot)
        if (first) {
            seqs(m, p, Walk, 15);
            p.i(pv::kSub, 1);
        }
        const float off = std::fabs(angle_to(bearing, p.f(pv::kHeading)));
        int32_t s = p.i(pv::kSub);
        if (s == 1 && off > 45.0f * kDeg) {
            s = 2;
            seqs(m, p, Turn, 15);
        }
        if (s == 2 && off < 10.0f * kDeg) {
            s = 1;
            seqs(m, p, Walk, 15);
        }
        if (s == 1 && to_hero < 10.0f) {
            s = 0;
            rac2::seq_play(m, Stand, 15);
        }
        if (s == 0 && to_hero > 20.0f) {
            s = 1;
            seqs(m, p, Walk, 10);
        }
        p.i(pv::kSub, s);
        if (seq == Walk) {
            walk(m, p, hero, true);
            if ((t > 0.0f && t < 2.0f) || (t > 15.0f && t < 17.0f) || (t > 30.0f && t < 32.0f) || (t > 45.0f && t < 47.0f)) rac2::camera_shake(0.05f, 15);
        } else {
            walk(m, p, at, false);
            if (seq == Turn) {
                if ((t > 12.0f && t < 25.0f) || (t > 42.0f && t < 55.0f)) face(m, p, bearing);
                if ((t > 0.0f && t < 2.0f) || (t > 29.0f && t < 31.0f)) rac2::camera_shake(0.1f, 15);
            }
        }
        if (hurt) {
            set_state(m, p, Gloating);
            break;
        }
        const int32_t left = p.i(pv::kSpecial) - 1;
        p.i(pv::kSpecial, std::max(0, left));
        if (left <= 0) {
            p.i(pv::kSpecial, 600 + rac2::rand_below(601));
            set_state(m, p, Special);
            break;
        }
        if (tag_due(p)) set_state(m, p, TagOut);
        break;
    }
    case Special: {  // (state 6: the wind-up, the spin, the stop)
        if (first) {
            if (seq == Spin) {
                p.i(pv::kSub, 1);
                if (const uint32_t tt = turret_of(m, p)) rac2::seq_play(tt, kTurretOf[Spin], 15);
            } else {
                seqs(m, p, WindUp, 1);
                p.i(pv::kSub, 0);
            }
        }
        const int32_t s = p.i(pv::kSub);
        if (s == 0) {
            walk(m, p, at, false);
            if (t > 20.0f && t < 22.0f) rac2::camera_shake(0.2f, 25);
            if (seq == WindUp && t > 28.0f) {
                seqs(m, p, Spin, 1);
                p.i(pv::kSub, 1);
            }
        } else if (s == 1) {
            rac2::sound_loop(m, 6);
            const int32_t left = p.i(pv::kSpecial) - 1;
            p.i(pv::kSpecial, std::max(0, left));
            if (left <= 0 || tag_due(p)) {
                seqs(m, p, SpinEnd, 1);
                p.i(pv::kSub, 2);
                rac2::sound_loop(m, -1);
            } else if (hurt) {
                set_state(m, p, Gloating);
            }
        } else {
            walk(m, p, at, false);
            if (t > 30.0f && t < 32.0f) rac2::camera_shake(0.2f, 25);
            if (seq == SpinEnd && t > 45.0f) {
                p.i(pv::kSpecial, 600 + rac2::rand_below(601));
                set_state(m, p, Walking);
            }
        }
        break;
    }
    case Gloating:  // (state 7: the turret's spin pose 181 frames, then the special)
        if (first) {
            if (const uint32_t tt = turret_of(m, p)) rac2::seq_play(tt, kTurretOf[Spin], 15);
            p.f(pv::kYawW, p.f(pv::kYawW) + 0.1939f);
        }
        if (seq == Walk) walk(m, p, hero, true);
        else if (seq != Spin) walk(m, p, at, false);
        if (seq == Spin) rac2::sound_loop(m, 6);
        if (frames(p) >= kGloat) set_state(m, p, Special);
        break;
    case Knocked: {  // (state 9: the turret knocked round, smoking; back to what it did when its sequence ends)
        const uint32_t tt = turret_of(m, p);
        if (first && tt != 0) rac2::seq_play(tt, TKnocked, 15);
        if (seq != Spin) walk(m, p, at, false);
        Vec3 smoke[1];
        if (tt != 0) {
            rac2::joints_world(tt, 4, 1, smoke);
            spawn_particle_3c_v(Vec4{smoke[0].x, smoke[0].y, smoke[0].z, 1.0f}, Vec4{0.0f, 0.0f, 0.03f, 0.0f}, 0x40505050u, 30, 0, 0, 0.8f);
        }
        if (tt == 0 || (frames(p) > 3 && rac2::seq_ended(tt))) {
            const int32_t back = p.i(pv::kSaved), back_sub = p.i(pv::kSavedSub);
            set_state(m, p, back);
            p.i(pv::kEntered, 1);
            p.i(pv::kSub, back_sub);
            if (tt != 0) rac2::seq_play(tt, static_cast<uint32_t>(p.i(pv::kSavedTurret)), 15);
        }
        break;
    }
    case Falling: {  // (state 10: it falls (9), then lies in its spin pose; when the turret's fall ends, state 11)
        const uint32_t tt = turret_of(m, p);
        if (first) {
            rac2::sound_loop(m, -1);
            if (seq == Spin) {
                p.i(pv::kSub, 1);
            } else {
                rac2::seq_play(m, Fall, 15);
                p.i(pv::kSub, 0);
            }
            if (tt != 0) rac2::seq_play(tt, TFall, 15);
        }
        walk(m, p, at, false);
        if (p.i(pv::kSub) == 0) {
            if (frames(p) > 3 && rac2::seq_ended(m)) {
                p.i(pv::kSub, 1);
                rac2::seq_play(m, Spin, 10);
            }
        } else if (tt == 0 || (frames(p) > 3 && rac2::seq_ended(tt))) {
            set_state(m, p, Dying);
        }
        break;
    }
    case Dying:  // (state 11: counted, the piece thrown off, its sound 5; 181 frames later it blows up)
        walk(m, p, at, false);
        if (first) {
            if (const uint32_t piece = static_cast<uint32_t>(p.i(pv::kPiece)); rac2::part_of(piece, m)) {
                const Vec3 pa = rac2::moby_pos(piece);
                Vec3 d{pa.x - at.x, pa.y - at.y, 0.0f};
                const float l = std::max(0.01f, std::hypot(d.x, d.y));
                const float sp = 0.0833f + rac2::rand_unit() * 0.0333f;
                rac2::part_throw(piece, Vec3{d.x / l * sp, d.y / l * sp, 0.0667f}, kFloor, 300);
            }
            rac2::sound_of_class(m, lh(m + rac2::moby::kClassId), 5);
            hand_over(p);
            // (RAC2's 0x3d8cbc: beaten without a scratch, "2B or not 2B hit")
            if (p.i(pv::kScratched) == 0) custom_arena_skill_point(kSkillBrawler);
        }
        if (frames(p) >= kDyingGone) {
            blow_up(m, p);
            return;
        }
        break;
    default: set_state(m, p, Walking);
    }
    // (its spin: turning, sliding at him, its limbs' hurt: RAC2's 0x3d9c1c, any state but falling)
    if (rac2::seq_now(m) == Spin && p.i(pv::kState) != Falling && p.i(pv::kState) != Dying) {
        const float h = share(p);
        p.f(pv::kHeading, angle_to(p.f(pv::kHeading) + (160.0f - 90.0f * h) * (kDeg / 60.0f), 0.0f));
        swc1(m + rac2::moby::kRot + 8, p.f(pv::kHeading));
        spin_hurt(m, p);
        if (dist3d(rac2::moby_pos(m), hero) > 12.0f) {
            const float step = (3.0f - 2.2f * h) / 60.0f;
            Vec4 d{std::clamp(hero.x - at.x, -step, step), std::clamp(hero.y - at.y, -step, step), 0.0f, 0.0f};
            walker_move_v(m, d, 0, 1.0f, 1.5f, 0.0f);
        }
    }
    turret_step(m, p, to_hero);
    if (trace() && (rac2::level_frame() % 60) == 0)
        std::printf("[2628] frame %u: %#x state %d sub %d seq %u time %.1f at %.1f %.1f %.1f, Ratchet %.1f away, health %.1f, turret %#x seq %u\n",
                    rac2::level_frame(), m, p.i(pv::kState), p.i(pv::kSub), rac2::seq_now(m), static_cast<double>(t), static_cast<double>(at.x),
                    static_cast<double>(at.y), static_cast<double>(at.z), static_cast<double>(to_hero), static_cast<double>(p.f(pv::kHealthNow)),
                    turret_of(m, p), turret_of(m, p) != 0 ? rac2::seq_now(turret_of(m, p)) : 0u);
}

}  // namespace

// orig: (none) — see rac2_enemies.h
void rac2_boss_2628_update(uint32_t m) { update(m); }

// orig: (none) — see rac2_enemies.h
void rac2_boss_2628_health(uint32_t m, float& now, float& most) {
    const uint32_t b = m != 0 ? lwu(m + rac2::moby::kPvars) : 0u;
    now = b != 0 && lw(b + pv::kState) != SetUp ? std::max(0.0f, lwc1(b + pv::kHealthNow)) : health_most();
    most = health_most();
}

// orig: (none) — see rac2_enemies.h (RAC2's 0x3db870 and the activate's second argument)
void rac2_boss_2628_pair(uint32_t m, uint32_t partner, bool waits) {
    const uint32_t b = lwu(m + rac2::moby::kPvars);
    if (b == 0) return;
    sw(b + pv::kPartner, partner);
    sw(b + pv::kPartnerCls, partner != 0 ? static_cast<uint32_t>(lh(partner + rac2::moby::kClassId)) : 0u);
    sw(b + pv::kPartnerPv, partner != 0 ? lwu(partner + rac2::moby::kPvars) : 0u);
    sw(b + pv::kWaitFirst, waits ? 1u : 0u);
}

// orig: (none) — see rac2_enemies.h (RAC2's 0x3db880: its thirds of health lost counted up to now, its flag to come in)
void rac2_boss_2628_turn(uint32_t m) {
    const uint32_t b = lwu(m + rac2::moby::kPvars);
    if (b == 0 || lw(b + pv::kGo) != 0) return;
    const float health = lw(b + pv::kState) == SetUp ? health_most() : lwc1(b + pv::kHealthNow);
    int32_t n = lw(b + pv::kTurns) + 1;
    while (n < 3 && health < health_most() * static_cast<float>(3 - n) / 3.0f) ++n;
    sw(b + pv::kTurns, static_cast<uint32_t>(n));
    sw(b + pv::kGo, 1);
    if (trace()) std::printf("[2628] frame %u: %#x its turn (thirds %d)\n", rac2::level_frame(), m, n);
}

// orig: (none) — see rac2_enemies.h
bool rac2_boss_2628_fighting(uint32_t m) {
    const uint32_t b = lwu(m + rac2::moby::kPvars);
    return b != 0 && lw(b + pv::kState) >= TagOut;
}

}  // namespace rac1
