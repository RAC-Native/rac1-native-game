// See custom_classes.h. The behaviour of the custom planet's own classes (Maktar's arena: RAC2's crowd 3734, monitors 4697,
// jointed arms 2581, floor fittings 2563 / 3276, converted by tools/custom_level/rac2classes.py), written for RAC1's engine
// after what RAC2's level code does (its update functions read as the reference: their states, sequences, timings).
#include <host/custom_classes.h>
#include <host/custom_planet.h>
#include <host/custom_arena.h>
#include <host/rac2_compat.h>
#include <host/rac2_enemies.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/disc.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace rac1 {

namespace {

enum class Role { Crowd, Monitor, Arm, Prop, Tile, Lift, Enemy3134, Enemy3131, Enemy2850, Enemy2506, Boss2552, Part };

// planet.txt's "class <role> <id> <bank>"
struct ClassLine {
    Role role;
    int32_t id;
    int32_t bank;  // the index in custom_planet_banks() of its sounds' bank, -1 none
};

// Our update functions' addresses in the class update table: in the EE kernel's range, where no game code is, so no
// port of the game's answers to them (the dispatch's level-0 addresses are Veldin's own: identity).
constexpr uint32_t kFnBase = 0x0007ff00;

uint32_t g_read_gen = 0xffffffffu;
std::vector<ClassLine> g_lines;

// orig: (none) — planet.txt's class lines (read again when the planet's generation changes)
const std::vector<ClassLine>& lines() {
    if (g_read_gen == custom_level_generation()) return g_lines;
    g_read_gen = custom_level_generation();
    g_lines.clear();
    std::ifstream f(custom_level_dir() + "/planet.txt");
    std::string line;
    while (f && std::getline(f, line)) {
        std::istringstream in(line);
        std::string key, role;
        ClassLine c{Role::Prop, -1, -1};
        if (!(in >> key) || key != "class" || !(in >> role >> c.id >> c.bank)) continue;
        if (role == "crowd") c.role = Role::Crowd;
        else if (role == "monitor") c.role = Role::Monitor;
        else if (role == "arm") c.role = Role::Arm;
        else if (role == "tile") c.role = Role::Tile;
        else if (role == "lift") c.role = Role::Lift;
        else if (role == "enemy_3134") c.role = Role::Enemy3134;  // (P6: RAC2's enemies hand-ported, rac2_enemies.h)
        else if (role == "enemy_3131") c.role = Role::Enemy3131;
        else if (role == "enemy_2850") c.role = Role::Enemy2850;
        else if (role == "enemy_2506") c.role = Role::Enemy2506;
        else if (role == "enemy_2552") c.role = Role::Boss2552;  // (P7: RAC2's bosses hand-ported, rac2_enemies.h)
        else if (role == "part") c.role = Role::Part;  // (carried by an enemy: rac2_compat.h's parts)
        else if (role != "prop") continue;
        g_lines.push_back(c);
    }
    return g_lines;
}

// orig: (none) — the line of RAC1 class `cls`, nullptr when it is not one of ours
const ClassLine* line_of(int32_t cls) {
    for (const ClassLine& c : lines())
        if (c.id == cls) return &c;
    return nullptr;
}

// ---- the moby record (RAC1's 0x100 bytes) and the services our behaviours use ----
namespace moby {
constexpr uint32_t kPos = 0x10, kClass = 0x24, kSpeed = 0x58, kRot = 0x40, kAnimFlags = 0x70, kNextSeq = 0x53, kPvars = 0x78;
constexpr uint32_t kMatrix = 0xc0, kClassId = 0xa6, kUid = 0xb2;
}  // namespace moby

// A small random generator of our own (xorshift), so that the game's own random stream (and with it everything else on the
// level) stays as it is.
// orig: (none)
uint32_t rnd(uint32_t& s) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}
uint32_t g_rng = 0x2545f491u;
// orig: (none) — 0 .. n - 1
int32_t rand_below(int32_t n) { return n <= 1 ? 0 : static_cast<int32_t>(rnd(g_rng) % static_cast<uint32_t>(n)); }
// orig: (none) — 0 .. 1
float rand_unit() { return static_cast<float>(rnd(g_rng) & 0xffffff) / 16777216.0f; }
// orig: (none) — lo .. hi
float rand_range(float lo, float hi) { return lo + (hi - lo) * rand_unit(); }

// orig: (none) — RAC1_CUSTOM_TRACE
bool trace() {
    static const bool t = std::getenv("RAC1_CUSTOM_TRACE") != nullptr;
    return t;
}

// orig: (none) — the level's frame count (0x15f5cc)
uint32_t level_frame() { return lwu(OV(0x0015f5cc)); }

// orig: (none) — the animation looped (or ended) in this frame's step (moby_anim_advance's flags +0x70, bit 1)
bool looped(uint32_t m) { return (lbu(m + moby::kAnimFlags) & 2u) != 0; }

// orig: (none) — sequence `seq` played from its first frame, blended in over `blend` frames (as RAC2's anim_seq_set; RAC1's
// moby_anim_set takes at least one), unless it is the one playing already
void play(uint32_t m, uint32_t seq, int32_t blend) {
    if (lbu(m + moby::kNextSeq) == seq) return;
    moby_anim_set(m, seq, 0, blend > 0 ? blend : 1);
}

// orig: (none) — the class's sound records (+0x28, 32 bytes, +0xd of them) pointed at the planet's RAC2 bank: the game's own
// class sound pass gave them RAC2's sound ids (the index's lists) and its level bank; ours is the RAC2 bank's handle
void bind_sounds(uint32_t m, const ClassLine& c) {
    const std::vector<uint32_t>& banks = custom_planet_banks();
    if (c.bank < 0 || static_cast<size_t>(c.bank) >= banks.size()) return;
    const uint32_t hdr = lwu(m + moby::kClass), recs = lwu(hdr + 0x28);
    if (hdr == 0 || recs == 0) return;
    for (uint32_t j = 0; j < lbu(hdr + 0xd); ++j)
        if (lwu(recs + j * 32 + 0x1c) != banks[static_cast<size_t>(c.bank)]) sw(recs + j * 32 + 0x1c, banks[static_cast<size_t>(c.bank)]);
}

// The pvars of each behaviour: our own layout (the tools give each moby a zeroed block of its size, maktar.py CLASS_PVARS)
struct Pvars {
    uint32_t base;
    int32_t i(uint32_t o) const { return lw(base + o); }
    void i(uint32_t o, int32_t v) const { sw(base + o, static_cast<uint32_t>(v)); }
    float f(uint32_t o) const { return lwc1(base + o); }
    void f(uint32_t o, float v) const { swc1(base + o, v); }
};

// ---- the crowd (RAC2 3734, its update 0x42e988): cards in the stands playing 8-frame loops ----
// Sequences: 0 / 1 idle, 2 / 3 cheering, 4 standing up as a wave goes round the stands. RAC2's crowd follows the challenge
// controller's excitement (its pvars +0x6c): above 4 it starts cheering, under 3 it calms down, under 2 it gets bored and
// does the wave, above 3 again it stops; each change at a 1-in-60 chance a frame, so the stands change gradually. Ours
// follows our arena controller (custom_arena.h): waiting 1, a wave on 2.5, a wave just cleared or a challenge won 5.
namespace crowd {
constexpr uint32_t kState = 0x0, kEntered = 0x4;
constexpr uint32_t kWavePeriod = 600;        // (frames for the wave to go round the stands once)
constexpr float kWaveWidth = 0.0698132f;      // (radians either side of the wave's front: 4 degrees)
constexpr float kCentreX = 500.0f, kCentreY = 600.0f;  // (the arena's centre, round which the wave goes)
constexpr float kPi = 3.14159265f;

// orig: (none) — RAC2's controller's excitement, from our challenge's mood
float excitement() {
    switch (custom_arena_mood()) {
    case ArenaMood::Cheering: return 5.0f;
    case ArenaMood::Fighting: return 2.5f;
    default: return 1.0f;
    }
}

// orig: (none) — |a - b| as an angle (0 .. pi)
float angle_diff(float a, float b) {
    float d = std::fmod(a - b, 2.0f * kPi);
    if (d > kPi) d -= 2.0f * kPi;
    if (d < -kPi) d += 2.0f * kPi;
    return std::fabs(d);
}

// orig: (none) — a new state (its first frame next)
void set_state(const Pvars& pv, int32_t s) {
    pv.i(kState, s);
    pv.i(kEntered, 0);
}

// orig: (none)
void update(uint32_t m) {
    const Pvars pv{lwu(m + moby::kPvars)};
    if (pv.base == 0) return;
    const float exc = excitement();
    const bool entering = pv.i(kEntered) == 0;
    pv.i(kEntered, 1);
    switch (pv.i(kState)) {
    case 0:  // (set up: an idle loop)
        set_state(pv, 1);
        play(m, static_cast<uint32_t>(rand_below(2)), 10);
        break;
    case 1:  // idle
        if (entering) play(m, static_cast<uint32_t>(rand_below(2)), 10);
        if (exc > 4.0f && rand_below(60) == 0) set_state(pv, 2);
        else if (exc < 2.0f && rand_below(60) == 0) set_state(pv, 3);
        break;
    case 2:  // cheering
        if (entering) play(m, static_cast<uint32_t>(2 + rand_below(2)), 10);
        if (exc < 3.0f && rand_below(60) == 0) set_state(pv, 1);
        break;
    case 3: {  // the wave: two fronts opposite each other going round the arena's centre
        const float front = -2.0f * kPi * static_cast<float>(level_frame() % kWavePeriod) / static_cast<float>(kWavePeriod) + kPi;
        const float here = std::atan2(lwc1(m + moby::kPos + 4) - kCentreY, lwc1(m + moby::kPos) - kCentreX);
        if (angle_diff(here, front) < kWaveWidth || angle_diff(here, front + kPi) < kWaveWidth) play(m, 4, 10);
        else if (looped(m)) play(m, static_cast<uint32_t>(rand_below(2)), 10);
        if (exc > 3.0f && rand_below(60) == 0) set_state(pv, 1);
        break;
    }
    default: set_state(pv, 0);
    }
}
}  // namespace crowd

// ---- the monitors on posts (RAC2 4697, its update 0x43f4c8): screens swaying slowly on their posts, breaking when shot ----
// Sequences 0..2: three single-frame poses (RAC2 picks 1 or 2). Each sways about x and y by 0.05 radians at its own slow
// rates (RAC2: random phases, rates of 20..40 and 40..60 times 2.9e-4 a frame).
// Shot (RAC2: its hit test 0x320d48 with the mask 0x4210000; ours RAC1's: the class keeps its collision, rac2classes.py,
// the moby takes hits (flag 0x4000) and the weapons' hit record reaches it as a lightpost's does, mask 0x10000 with some
// damage), it comes off its post: it flies on along the shot's push at 0.25 a frame for 30..60 frames, turning by how far
// from its middle it was hit (about y: 0.0698 x the hit's height, about z: 0.0524 x its side, each clamped to 1), giving off
// sparks (one frame in two) with its sound 0. If the flight would end in a wall it breaks at once. It breaks: an explosion
// with its sound 1, debris, its bolts (RAC2: 15 x 3 - 1 = 44 on Maktar, thrown at the hero within 7 units), and it is gone
// until the level loads again (RAC2 keeps no record of it either).
namespace monitor {
constexpr uint32_t kState = 0x0, kPhaseX = 0x4, kPhaseY = 0x8, kRateX = 0xc, kRateY = 0x10, kBaseX = 0x14, kBaseY = 0x18;
constexpr uint32_t kTimer = 0x1c, kVel = 0x20, kSpinY = 0x30, kSpinZ = 0x34;  // (kVel: a vector, 16 bytes)
constexpr float kRateUnit = 2.908882e-4f, kAmplitude = 0.05f;
constexpr float kFlySpeed = 0.25f, kSpinYRate = 0.0697f, kSpinZRate = 0.0524f;
constexpr int32_t kBolts = 44;
constexpr uint32_t kTakesHits = 0x4000, kHitMask = 0x10000;
enum : int32_t { SetUp = 0, Swaying = 1, Flying = 2, Breaking = 3 };

// orig: (none) — (testing) RAC1_ARENA_MONITOR_HIT=<level frame>[:<monitor>]: at that frame a blast (a bomb's sphere test with
// the blaster's hit record) on the monitors (each, or the n-th to update in that frame): the hit through RAC1's own
// collision and hit records, as a weapon's
void test_blast(uint32_t m) {
    static const std::pair<uint32_t, int32_t> at = [] {
        const char* e = std::getenv("RAC1_ARENA_MONITOR_HIT");
        if (e == nullptr) return std::pair<uint32_t, int32_t>{0, -2};
        char* end = nullptr;
        const uint32_t f = static_cast<uint32_t>(std::strtoul(e, &end, 10));
        return std::pair<uint32_t, int32_t>{f, *end == ':' ? std::atoi(end + 1) : -1};
    }();
    static uint32_t frame = 0;
    static int32_t nth = 0;
    if (at.second == -2) return;
    if (frame != level_frame()) {
        frame = level_frame();
        nth = 0;
    }
    const int32_t k = nth++;
    if (frame != at.first || (at.second >= 0 && k != at.second)) return;
    uint32_t rec[9] = {};
    Vec4 push = V(m + moby::kPos);
    push.x -= lwc1(0x0013f3d0);  // (away from the hero, as the blaster's)
    push.y -= lwc1(0x0013f3d4);
    push.z = 0.0f;
    vec2_set_length(push, push, 1.0f);
    std::memcpy(rec, &push, 16);
    rec[5] = 0x00010001u;  // (+0x14: the hit's kinds, RAC1's mask word)
    const float damage = 0.25f;
    std::memcpy(&rec[7], &damage, 4);
    rec[8] = 1;
    const int32_t n = mobys_in_sphere_v(V(m + moby::kPos), 1.0f, 0x10, 0, rec);
    std::printf("[classes] frame %u: test blast on monitor %d (uid %d): %d moby(s) touched\n", level_frame(), k, lh(m + moby::kUid), n);
}

// orig: (none) — the shot that took it off its post: its flight (velocity, turn rates, frames)
void hit_taken(uint32_t m, const Pvars& pv, uint32_t hit) {
    Vec4 push = V(hit + 0x10);
    push.w = 0.0f;
    if (vec3_length(push) < 1e-4f) {  // (no push: away from the hero)
        push = V(m + moby::kPos);
        push.x -= lwc1(0x0013f3d0);
        push.y -= lwc1(0x0013f3d4);
        push.z = 0.0f;
    }
    vec3_set_length(push, push, kFlySpeed);
    push.w = 0.0f;
    V(pv.base + kVel) = push;
    // where it was hit, in its own axes (the contact point when the record has one, else its near side)
    Vec4 at = V(hit);
    Vec4 rel{at.x - lwc1(m + moby::kPos), at.y - lwc1(m + moby::kPos + 4), at.z - lwc1(m + moby::kPos + 8), 0.0f};
    if (vec3_length(rel) > 20.0f || (at.x == 0.0f && at.y == 0.0f && at.z == 0.0f)) {
        Vec4 back;
        vec3_set_length(back, push, -1.0f);
        rel = back;
    }
    float local[3];
    for (uint32_t r = 0; r < 3; ++r)
        local[r] = rel.x * lwc1(m + moby::kMatrix + 0x10 * r) + rel.y * lwc1(m + moby::kMatrix + 0x10 * r + 4) + rel.z * lwc1(m + moby::kMatrix + 0x10 * r + 8);
    auto clamp1 = [](float v) { return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v); };
    pv.f(kSpinY, -clamp1(local[2]) * kSpinYRate);
    pv.f(kSpinZ, clamp1(local[1]) * kSpinZRate);
    pv.i(kTimer, 30 + rand_below(31));
    sh(m + 0x34, lhu(m + 0x34) & ~kTakesHits);
    // (RAC2: a wall before the flight's end: it breaks at once)
    Vec4 end = V(m + moby::kPos);
    end.x += push.x * static_cast<float>(pv.i(kTimer));
    end.y += push.y * static_cast<float>(pv.i(kTimer));
    end.z += push.z * static_cast<float>(pv.i(kTimer));
    const bool wall = segment_collide_v(V(m + moby::kPos), end, 0, m, nullptr) != 0;
    pv.i(kState, wall ? Breaking : Flying);
    if (!wall) moby_sound_start(0, 0, m);
    if (trace()) std::printf("[classes] frame %u: monitor uid %d hit: flies %d frames%s\n", level_frame(), lh(m + moby::kUid), pv.i(kTimer), wall ? " (a wall: breaks)" : "");
}

// orig: (none) — a spark off the falling monitor (RAC2: one frame in two, drifting up from half a unit below it)
void spark(uint32_t m) {
    if (rand_below(2) != 0) return;
    Vec4 at = V(m + moby::kPos);
    at.z -= 0.5f;
    const Vec4 up{0.0f, 0.0f, 0.0667f, 0.0f};
    spawn_particle_0b_v(at, up, 0x40f0f0f0u, 0x40f0f0f0u, 15, 25, 0, 0, 60000.0f, 1.0f);
}

// orig: (none) — it breaks: the explosion and its sound 1, debris, the bolts; gone
void break_up(uint32_t m) {
    explosion_burst(m, m + moby::kPos, 1, 1.0f, 8.0f);
    debris_burst(m, 0x7ab, 1, 0x7ab, 1, 4, 0);  // (RAC1's: the debris of Veldin's walkers, 0x7ab, in place of RAC2's own pieces)
    const uint32_t near = vec3_distance(V(m + moby::kPos), V(0x0013f3d0)) < 7.0f ? 2u : 0u;
    bolts_drop(m, kBolts, kBolts, near, -1);
    if (trace()) std::printf("[classes] frame %u: monitor uid %d breaks\n", level_frame(), lh(m + moby::kUid));
    moby_retire(m);
}

// orig: (none)
void update(uint32_t m) {
    const Pvars pv{lwu(m + moby::kPvars)};
    if (pv.base == 0) return;
    switch (pv.i(kState)) {
    case SetUp:
        pv.f(kPhaseX, rand_unit() * 6.2831853f);
        pv.f(kPhaseY, rand_unit() * 6.2831853f);
        pv.f(kRateX, rand_range(20.0f, 40.0f) * kRateUnit);
        pv.f(kRateY, rand_range(40.0f, 60.0f) * kRateUnit);
        pv.f(kBaseX, lwc1(m + moby::kRot));
        pv.f(kBaseY, lwc1(m + moby::kRot + 4));
        play(m, static_cast<uint32_t>(1 + rand_below(2)), 5);
        if (lwu(m + 0x94) != 0) sh(m + 0x34, lhu(m + 0x34) | kTakesHits);  // (with its collision: weapons reach it)
        pv.i(kState, Swaying);
        break;
    case Swaying: {
        const float px = std::fmod(pv.f(kPhaseX) + pv.f(kRateX), 6.2831853f), py = std::fmod(pv.f(kPhaseY) + pv.f(kRateY), 6.2831853f);
        pv.f(kPhaseX, px);
        pv.f(kPhaseY, py);
        swc1(m + moby::kRot, pv.f(kBaseX) + kAmplitude * std::sin(px));
        swc1(m + moby::kRot + 4, pv.f(kBaseY) + kAmplitude * std::sin(py));
        test_blast(m);
        const uint32_t hit = sub_0025a420(m, kHitMask, 0);
        if (hit != 0 && 0.0f < lwc1(hit + 0x2c)) hit_taken(m, pv, hit);
        break;
    }
    case Flying: {
        spark(m);
        swc1(m + moby::kRot + 4, lwc1(m + moby::kRot + 4) + pv.f(kSpinY));
        swc1(m + moby::kRot + 8, lwc1(m + moby::kRot + 8) + pv.f(kSpinZ));
        for (uint32_t k = 0; k < 3; ++k) swc1(m + moby::kPos + 4 * k, lwc1(m + moby::kPos + 4 * k) + lwc1(pv.base + kVel + 4 * k));
        pv.i(kTimer, pv.i(kTimer) - 1);
        if (pv.i(kTimer) <= 0) pv.i(kState, Breaking);
        break;
    }
    case Breaking: break_up(m); break;
    default: pv.i(kState, SetUp);
    }
}
}  // namespace monitor

// ---- the jointed arms at the floor's edge (RAC2 2581, its update 0x3d7330) ----
// Sequences: 0 lowering, 1 dropping (letting go of what it holds), 2 pulling back, 3 rising. Set up, an arm stands raised
// 2 units along its own up axis (its matrix's second row), its animation stopped. Told to drop (RAC2: its controller, with
// the enemies to drop; ours: the arena controller, custom_arena.h, with the enemies it hands out), it comes down those 2 units over 19 frames while it plays its
// lowering, drops, waits 90 frames, pulls back and rises again. Its sounds are its sequences' own sound events.
namespace arm {
constexpr uint32_t kState = 0x0, kTimer = 0x4, kEntered = 0x8, kGate = 0xc, kHome = 0x10;  // (kHome: a vector, 16 bytes)
constexpr float kTravel = 2.0f;
constexpr int32_t kTravelFrames = 19, kWait = 90;

// orig: (none) — a new state (its first frame next)
void set_state(const Pvars& pv, int32_t s) {
    pv.i(kState, s);
    pv.i(kEntered, 0);
}

// orig: (none) — the arm moved `d` along its up axis (its matrix's second row)
void move_up(uint32_t m, float d) {
    for (uint32_t k = 0; k < 3; ++k) swc1(m + moby::kPos + 4 * k, lwc1(m + moby::kPos + 4 * k) + d * lwc1(m + moby::kMatrix + 0x10 + 4 * k));
}

// orig: (none) — the 19 frames' move of states 1 and 3: true while it still moves
bool travel(uint32_t m, const Pvars& pv, bool entering, float step) {
    if (entering) pv.i(kTimer, kTravelFrames);
    if (pv.i(kTimer) <= 0) return false;
    pv.i(kTimer, pv.i(kTimer) - 1);
    move_up(m, step);
    return true;
}

// orig: (none)
void update(uint32_t m) {
    const Pvars pv{lwu(m + moby::kPvars)};
    if (pv.base == 0) return;
    const bool entering = pv.i(kEntered) == 0;
    pv.i(kEntered, 1);
    switch (pv.i(kState)) {
    case 0:  // set up: raised, still
        play(m, 0, 0);
        swc1(m + moby::kSpeed, 0.0f);
        move_up(m, kTravel);
        for (uint32_t k = 0; k < 4; ++k) sw(pv.base + kHome + 4 * k, lwu(m + moby::kPos + 4 * k));
        set_state(pv, 2);
        break;
    case 1:  // rising
        travel(m, pv, entering, kTravel / static_cast<float>(kTravelFrames));
        if (looped(m)) {
            for (uint32_t k = 0; k < 4; ++k) sw(m + moby::kPos + 4 * k, lwu(pv.base + kHome + 4 * k));
            swc1(m + moby::kSpeed, 0.0f);
            set_state(pv, 2);
        }
        break;
    case 2:  // raised, waiting for the controller to give it enemies (RAC2: 0x3d7868 assigns them, 0x3d78a8 lowers it)
        pv.i(kGate, custom_arena_gate_register(m));
        if (custom_arena_gate_wanted(pv.i(kGate))) {
            swc1(m + moby::kSpeed, 1.0f);
            play(m, 0, 0);
            set_state(pv, 3);
        }
        break;
    case 3:  // lowering
        travel(m, pv, entering, -kTravel / static_cast<float>(kTravelFrames));
        if (looped(m)) {
            swc1(m + moby::kSpeed, 0.0f);
            set_state(pv, 4);
        }
        break;
    case 4:  // the drop (RAC2: 0x3d7900 lets its enemies go)
        custom_arena_gate_drop(pv.i(kGate), m);
        swc1(m + moby::kSpeed, 1.0f);
        play(m, 1, 0);
        set_state(pv, 5);
        break;
    case 5:
        if (looped(m)) {
            pv.i(kTimer, kWait);
            swc1(m + moby::kSpeed, 0.0f);
            set_state(pv, 6);
        }
        break;
    case 6:  // waiting down
        if (pv.i(kTimer) > 0) {
            pv.i(kTimer, pv.i(kTimer) - 1);
        } else {
            swc1(m + moby::kSpeed, 1.0f);
            play(m, 2, 0);
            set_state(pv, 7);
        }
        break;
    case 7:  // pulling back
        if (looped(m)) {
            play(m, 3, 0);
            set_state(pv, 1);
        }
        break;
    default: set_state(pv, 0);
    }
}
}  // namespace arm

// ---- a prop: idle ----
// orig: (none)
void prop_update(uint32_t) {}

// ---- the floor's tiles (RAC2 2563, its update 0x3d6bc0) ----
// Set up off (state 2). Turned on by the controller (its pvars +0xbc, the wave's column c4): it turns over about its own x
// axis, 180 degrees in 60 frames (3 a frame: RAC2's 180 a second, 0x1390), its sound 1 as it starts and 0 while it turns,
// then stays over (state 1) hurting what comes within 1.5 of it (0x3d7140: the hero and the mobys that take hits, knocked
// along its turn: a push of 2 across, 1 up; its pvars' damage), sparking now and then (0x3d7040); turned off it turns back
// (state 4) and is off again. It hurts while it turns too.
namespace tile {
constexpr uint32_t kState = 0x0, kTimer = 0x4, kIndex = 0x8, kAngle = 0xc, kBaseX = 0x10;
enum : int32_t { SetUp, Off, Turning, Over, TurningBack };
constexpr int32_t kTurnFrames = 60;
constexpr float kPi = 3.14159265f, kHurtRadius = 1.5f;

// orig: (none) — RAC2's 0x3d7140 for the hero: within its radius, knocked across (the push at right angles to the line
// from the tile to the hero, 2 long, 1 up)
void hurt(uint32_t m) {
    const rac2::Vec3 h = rac2::hero_pos();
    const float dx = h.x - lwc1(m + moby::kPos), dy = h.y - lwc1(m + moby::kPos + 4);
    const float d = std::hypot(dx, dy);
    if (d >= kHurtRadius || std::fabs(h.z - lwc1(m + moby::kPos + 8)) > 2.0f) return;
    const float ux = d > 1e-3f ? dx / d : 1.0f, uy = d > 1e-3f ? dy / d : 0.0f;
    rac2::hero_hit(m, rac2::Vec3{-uy * 2.0f, ux * 2.0f, 1.0f}, static_cast<float>(custom_arena_tile_damage()));
    if (trace()) std::printf("[classes] frame %u: tile uid %d hits the hero (nanotech %d)\n", level_frame(), lh(m + moby::kUid), rac2::hero_health());
}

// orig: (none) — a spark off it now and then (RAC2's 0x3d7040: a 1-in-2 pick of its effect, near its top)
void spark(uint32_t m) {
    if (rand_below(8) != 0) return;
    const Vec4 at{lwc1(m + moby::kPos) + rand_range(-0.5f, 0.5f), lwc1(m + moby::kPos + 4) + rand_range(-0.5f, 0.5f), lwc1(m + moby::kPos + 8) + 0.5f, 1.0f};
    spawn_particle_0b_v(at, Vec4{0.0f, 0.0f, 0.0667f, 0.0f}, 0x40f0f0f0u, 0x40f0f0f0u, 15, 25, 0, 0, 60000.0f, 1.0f);
}

// orig: (none)
void update(uint32_t m) {
    const Pvars pv{lwu(m + moby::kPvars)};
    if (pv.base == 0) return;
    const int32_t me = pv.i(kIndex);
    switch (pv.i(kState)) {
    case SetUp:
        pv.f(kBaseX, lwc1(m + moby::kRot));
        pv.f(kAngle, 0.0f);
        pv.i(kIndex, custom_arena_tile_register(m));
        pv.i(kState, Off);
        return;
    case Off:
        if (!custom_arena_tile_on(me)) return;
        pv.i(kState, Turning);
        pv.i(kTimer, kTurnFrames);
        moby_sound_start(1, 0, m);
        moby_sound_start(0, 0, m);
        return;
    case Turning:
    case TurningBack: {
        const bool back = pv.i(kState) == TurningBack;
        pv.f(kAngle, pv.f(kAngle) + (back ? -kPi : kPi) / static_cast<float>(kTurnFrames));
        pv.i(kTimer, pv.i(kTimer) - 1);
        if (pv.i(kTimer) <= 0) {
            pv.f(kAngle, back ? 0.0f : kPi);
            pv.i(kState, back ? Off : Over);
        }
        hurt(m);
        break;
    }
    case Over:
        hurt(m);
        spark(m);
        if (!custom_arena_tile_on(me)) {
            pv.i(kState, TurningBack);
            pv.i(kTimer, kTurnFrames);
            moby_sound_start(1, 0, m);
            moby_sound_start(0, 0, m);
        }
        break;
    default: pv.i(kState, SetUp); return;
    }
    swc1(m + moby::kRot, pv.f(kBaseX) + pv.f(kAngle));
}
}  // namespace tile

// ---- the floor's lifts (RAC2 3276, its update 0x4164a8) ----
// Each holds a crate below the floor and brings it up when the controller hands it one (0x3fd228 -> 0x416720: the wave's
// columns c6 / c7, a lift ready at a 1-in-6 chance a frame): its sequence 1 as it rises, the crate riding up with it, then
// 2 as it goes down again, the crate left on the floor. Ours: the crate a copy of the planet's hidden template
// (custom_arena.cpp) spawned on the lift below the floor and carried up to it.
namespace lift {
constexpr uint32_t kState = 0x0, kTimer = 0x4, kIndex = 0x8, kCrate = 0xc;
enum : int32_t { SetUp, Down, Rising, Lowering };
constexpr int32_t kRiseFrames = 40, kLowerFrames = 40;
constexpr float kBelow = 1.9f, kAbove = 0.15f;  // (the crate's start under the lift's top, and its end over it: RAC2's crates
                                                //  wait at z 498 in lifts whose top is at 499.86)

// orig: (none)
void update(uint32_t m) {
    const Pvars pv{lwu(m + moby::kPvars)};
    if (pv.base == 0) return;
    const float x = lwc1(m + moby::kPos), y = lwc1(m + moby::kPos + 4), z = lwc1(m + moby::kPos + 8);
    switch (pv.i(kState)) {
    case SetUp:
        pv.i(kIndex, custom_arena_lift_register(m));
        play(m, 0, 1);
        pv.i(kState, Down);
        return;
    case Down: {
        const uint32_t c = custom_arena_lift_crate(pv.i(kIndex), x, y, z - kBelow);
        if (c == 0) return;
        pv.i(kCrate, static_cast<int32_t>(c));
        pv.i(kTimer, kRiseFrames);
        pv.i(kState, Rising);
        play(m, 1, 4);
        return;
    }
    case Rising: {
        const uint32_t c = static_cast<uint32_t>(pv.i(kCrate));
        pv.i(kTimer, pv.i(kTimer) - 1);
        const float t = 1.0f - static_cast<float>(pv.i(kTimer)) / static_cast<float>(kRiseFrames);
        if (c != 0 && static_cast<int8_t>(lbu(c + 0x20)) >= 0) swc1(c + moby::kPos + 8, z - kBelow + (kBelow + kAbove) * t);
        if (pv.i(kTimer) <= 0) {
            pv.i(kTimer, kLowerFrames);
            pv.i(kState, Lowering);
            play(m, 2, 4);
        }
        return;
    }
    case Lowering:
        pv.i(kTimer, pv.i(kTimer) - 1);
        if (pv.i(kTimer) <= 0) {
            pv.i(kState, Down);
            play(m, 0, 4);
        }
        return;
    default: pv.i(kState, SetUp); return;
    }
}
}  // namespace lift

// orig: (none)
const ClassLine* line_of_moby(uint32_t m) { return line_of(lh(m + moby::kClassId)); }

// orig: (none) — every class's update: its sounds' bank, then its behaviour

template <void (*Behaviour)(uint32_t)>
void with_sounds(uint32_t m) {
    if (!custom_level_active(0)) return;
    if (trace() && (level_frame() % 120) == 0)
        std::printf("[classes] frame %u: class %d uid %d at %.1f %.1f %.1f seq %u state %d flags %04x\n", level_frame(), lh(m + moby::kClassId), lh(m + moby::kUid),
                    static_cast<double>(lwc1(m + moby::kPos)), static_cast<double>(lwc1(m + moby::kPos + 4)), static_cast<double>(lwc1(m + moby::kPos + 8)),
                    lbu(m + moby::kNextSeq), lw(lwu(m + moby::kPvars)), lhu(m + 0x34));
    if (const ClassLine* c = line_of_moby(m)) bind_sounds(m, *c);
    Behaviour(m);
}

struct Registered {
    Role role;
    MobyFn fn;
};
constexpr Registered kFns[] = {
    {Role::Crowd, with_sounds<crowd::update>},
    {Role::Monitor, with_sounds<monitor::update>},
    {Role::Arm, with_sounds<arm::update>},
    {Role::Prop, with_sounds<prop_update>},
    {Role::Tile, with_sounds<tile::update>},
    {Role::Lift, with_sounds<lift::update>},
    {Role::Enemy3134, with_sounds<rac2_enemy_3134_update>},
    {Role::Enemy3131, with_sounds<rac2_enemy_3131_update>},
    {Role::Enemy2850, with_sounds<rac2_enemy_2850_update>},
    {Role::Enemy2506, with_sounds<rac2_enemy_2506_update>},
    {Role::Boss2552, with_sounds<rac2_boss_2552_update>},
    {Role::Part, with_sounds<rac2::part_update>},
};

}  // namespace

// orig: (none) — see custom_classes.h
uint32_t custom_classes_update_fn(int32_t cls) {
    if (!custom_level_active(0) || lw(0x0015ed84) != 0) return 0;
    const ClassLine* c = line_of(cls);
    if (c == nullptr) return 0;
    static bool registered = false;
    if (!registered) {
        registered = true;
        for (uint32_t k = 0; k < sizeof kFns / sizeof kFns[0]; ++k) ovl_register_moby_fn(kFnBase + 8 * k, kFns[k].fn);
    }
    for (uint32_t k = 0; k < sizeof kFns / sizeof kFns[0]; ++k)
        if (kFns[k].role == c->role) {
            if (trace()) std::printf("[classes] class %d: our update %#x\n", cls, kFnBase + 8 * k);
            return kFnBase + 8 * k;
        }
    return 0;
}

}  // namespace rac1
