// See custom_classes.h. The behaviour of the custom planet's own classes (Maktar's arena: RAC2's crowd 3734, monitors 4697,
// jointed arms 2581, floor fittings 2563 / 3276, converted by tools/custom_level/rac2classes.py), written for RAC1's engine
// after what RAC2's level code does (its update functions read as the reference: their states, sequences, timings).
#include <host/custom_classes.h>
#include <host/custom_planet.h>
#include <host/custom_waves.h>

#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/disc.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace rac1 {

namespace {

enum class Role { Crowd, Monitor, Arm, Prop };

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
// follows our wave challenge (custom_waves.h): waiting 1, a wave on 2.5, a wave just cleared 5.
namespace crowd {
constexpr uint32_t kState = 0x0, kEntered = 0x4;
constexpr uint32_t kWavePeriod = 600;        // (frames for the wave to go round the stands once)
constexpr float kWaveWidth = 0.0698132f;      // (radians either side of the wave's front: 4 degrees)
constexpr float kCentreX = 500.0f, kCentreY = 600.0f;  // (the arena's centre, round which the wave goes)
constexpr float kPi = 3.14159265f;

// orig: (none) — RAC2's controller's excitement, from our challenge's mood
float excitement() {
    switch (custom_waves_mood()) {
    case WavesMood::Cheering: return 5.0f;
    case WavesMood::Fighting: return 2.5f;
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

// ---- the monitors on posts (RAC2 4697, its update 0x43f4c8): screens swaying slowly on their posts ----
// Sequences 0..2: three single-frame poses (RAC2 picks 1 or 2). Each sways about x and y by 0.05 radians at its own slow
// rates (RAC2: random phases, rates of 20..40 and 40..60 times 2.9e-4 a frame). RAC2's monitors also break when shot (a
// hit through its damage test, a fall with sparks and their sound 0, an explosion with debris, bolts and sound 1):
// that needs RAC1's hit protocol for these mobys, the engine layer's (PLANET_PLAN.md P4), not here.
namespace monitor {
constexpr uint32_t kState = 0x0, kPhaseX = 0x4, kPhaseY = 0x8, kRateX = 0xc, kRateY = 0x10, kBaseX = 0x14, kBaseY = 0x18;
constexpr float kRateUnit = 2.908882e-4f, kAmplitude = 0.05f;

// orig: (none)
void update(uint32_t m) {
    const Pvars pv{lwu(m + moby::kPvars)};
    if (pv.base == 0) return;
    if (pv.i(kState) == 0) {
        pv.f(kPhaseX, rand_unit() * 6.2831853f);
        pv.f(kPhaseY, rand_unit() * 6.2831853f);
        pv.f(kRateX, rand_range(20.0f, 40.0f) * kRateUnit);
        pv.f(kRateY, rand_range(40.0f, 60.0f) * kRateUnit);
        pv.f(kBaseX, lwc1(m + moby::kRot));
        pv.f(kBaseY, lwc1(m + moby::kRot + 4));
        play(m, static_cast<uint32_t>(1 + rand_below(2)), 5);
        pv.i(kState, 1);
    }
    const float px = std::fmod(pv.f(kPhaseX) + pv.f(kRateX), 6.2831853f), py = std::fmod(pv.f(kPhaseY) + pv.f(kRateY), 6.2831853f);
    pv.f(kPhaseX, px);
    pv.f(kPhaseY, py);
    swc1(m + moby::kRot, pv.f(kBaseX) + kAmplitude * std::sin(px));
    swc1(m + moby::kRot + 4, pv.f(kBaseY) + kAmplitude * std::sin(py));
}
}  // namespace monitor

// ---- the jointed arms at the floor's edge (RAC2 2581, its update 0x3d7330) ----
// Sequences: 0 lowering, 1 dropping (letting go of what it holds), 2 pulling back, 3 rising. Set up, an arm stands raised
// 2 units along its own up axis (its matrix's second row), its animation stopped. Told to drop (RAC2: its controller, with
// the enemies to drop; ours: each wave of our challenge), it comes down those 2 units over 19 frames while it plays its
// lowering, drops, waits 90 frames, pulls back and rises again. Its sounds are its sequences' own sound events.
namespace arm {
constexpr uint32_t kState = 0x0, kTimer = 0x4, kEntered = 0x8, kWaves = 0xc, kHome = 0x10;  // (kHome: a vector, 16 bytes)
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
    const uint32_t waves = custom_waves_started();
    switch (pv.i(kState)) {
    case 0:  // set up: raised, still
        play(m, 0, 0);
        swc1(m + moby::kSpeed, 0.0f);
        move_up(m, kTravel);
        for (uint32_t k = 0; k < 4; ++k) sw(pv.base + kHome + 4 * k, lwu(m + moby::kPos + 4 * k));
        pv.i(kWaves, static_cast<int32_t>(waves));
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
    case 2:  // raised, waiting for a wave
        if (static_cast<int32_t>(waves) != pv.i(kWaves)) {
            pv.i(kWaves, static_cast<int32_t>(waves));
            if (waves != 0) {
                swc1(m + moby::kSpeed, 1.0f);
                play(m, 0, 0);
                set_state(pv, 3);
            }
        }
        break;
    case 3:  // lowering
        travel(m, pv, entering, -kTravel / static_cast<float>(kTravelFrames));
        if (looped(m)) {
            swc1(m + moby::kSpeed, 0.0f);
            set_state(pv, 4);
        }
        break;
    case 4:  // the drop
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

// ---- the floor's fittings (RAC2 2563, 3276): the challenges' hazards and props, idle until the challenges come (P5) ----
// orig: (none)
void prop_update(uint32_t) {}

// orig: (none)
const ClassLine* line_of_moby(uint32_t m) { return line_of(lh(m + moby::kClassId)); }

// orig: (none) — every class's update: its sounds' bank, then its behaviour
// orig: (none) — RAC1_CUSTOM_TRACE
bool trace() {
    static const bool t = std::getenv("RAC1_CUSTOM_TRACE") != nullptr;
    return t;
}

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
