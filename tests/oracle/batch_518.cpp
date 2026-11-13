// Oracle batch 518 (stage 4 task F12, #149): the class code of Blarg (level 6) and Orxon (level 10) that both levels carry a
// copy of, each family ported once as shared hand-written code (src/game/ovl/shared_*.cpp) on top of the families earlier
// tasks ported, against each level's own copy at that level's own address. The level copies stay transliterated in their
// units until each level is re-glued; here every copy is the original and the one shared port the port:
//  - shared_class1302.cpp: class 1302, the Gadgebots' entrance (its update, and the opening of its mobys);
//  - shared_class1068.cpp: class 1068, a walking robot with an electric beam (its update, its frame's common part, the
//    beam's start and run); shared_beam.cpp: the shards' spawn; shared_class302.cpp: class 302's update and break, the
//    spawns of classes 302 / 303;
//  - shared_class857.cpp: class 857, the Gadgebot (its update and helpers).
// The shared ports and their copies (each level's address):
//   class1302_update        L6:0x00307d30 L10:0x002e7cd8
//   class1302_open          L6:0x00307c48 L10:0x002e7bf0
//   class1068_update        L6:0x002fdbd0 L10:0x002dabe8
//   class1068_think         L6:0x002feb40 L10:0x002dbb58
//   class1068_beam_run      L6:0x002ff2c8 L10:0x002dc2e0
//   class1068_beam_start    L6:0x002ff100 L10:0x002dc118
//   class302_update         L6:0x002d85a0 L10:0x002bd0e8
//   class302_break          L6:0x002d8178 L10:0x002bccc0
//   beam_shard_spawn        L6:0x00300c60 L10:0x002dd0e0
//   class302_spawn          L6:0x002d8510 L10:0x002bd058
//   class303_spawn          L6:0x002d8b98 L10:0x002bd6e0
//   class857_update         L6:0x002f0040 L10:0x002d4078
//   class857_assign         L6:0x002f2250 L10:0x002d6288
//   class857_arrive         L6:0x002f2c28 L10:0x002d6c60
//   class857_pick_dest      L6:0x002f2910 L10:0x002d6948
//   class857_find           L6:0x002f32f0 L10:0x002d7328
//   class857_score          L6:0x002f2148 L10:0x002d6180
//   class857_react          L6:0x002f3640 L10:0x002d7678
//   class857_glow           L6:0x002f38c8 L10:0x002d7900
// Subjects: the level's own mobys of the class after the first gameplay frame (the port's; the screen, the projection and the
// view set up, as batch 509), perturbed through every state, or (a level that places none awake) live mobys given the
// class's pvars; the helpers on the class's mobys with arguments built from the level's state. Blarg's Gadgebots sleep
// after the first frame: their cases mostly start from batch 151's Clank-section dumps (clank_0415/ring_3015.bin and
// ring_3815.bin: tools/oracle/blarg_tour.sh clankdumps), from the level alone without them.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 518, N = 6 and 10; on another level every check says "not on level".
// B518_INFO=1 prints the subjects found.

#include "ovl_oracle.h"

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_beam.h>
#include <game/ovl/shared_class1068.h>
#include <game/ovl/shared_class1302.h>
#include <game/ovl/shared_class302.h>
#include <game/ovl/shared_class857.h>
#include <runtime/kernel/disc.h>
#include <runtime/vu/vu0.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <vector>

namespace rac1::oracle {

namespace {
// The VU0 the original started from (the interpreter's), given to the port before it runs (as batches 506 / 509 / 510)
VUState g_vu0_in;
void vu0_sync() {
    VUState& v = vu0::global().st;
    v.vf = g_vu0_in.vf;
    v.vi = g_vu0_in.vi;
    v.acc = g_vu0_in.acc;
    v.q = g_vu0_in.q;
    v.i = g_vu0_in.i;
    v.r = g_vu0_in.r;
    v.p = g_vu0_in.p;
    v.macflag = g_vu0_in.macflag;
    v.statusflag = g_vu0_in.statusflag;
    v.clipflag = g_vu0_in.clipflag;
    std::memcpy(v.mem.data(), g_vu0_in.mem.data(), 0x1000);
}

bool info() { return std::getenv("B518_INFO") != nullptr; }

// the family's copy on the level being run (0: the level has none)
uint32_t copy_here(uint32_t l6, uint32_t l10) { return g_oracle_level == 6 ? l6 : (g_oracle_level == 10 ? l10 : 0u); }

bool selected(const char* name) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return false;
    const char* only = std::getenv("ORACLE_ONLY");
    return only == nullptr || std::strcmp(only, name) == 0;
}

// A check whose original is the level's own copy at `laddr` (the port given the original's VU0 first).
void fcheck(const char* name, uint32_t laddr, int n, Ret ret, const std::function<Args()>& gen,
            const std::function<uint64_t(const Args&)>& port) {
    if (!selected(name)) return;
    if (laddr == 0) {
        std::printf("%-26s not on level %d\n", name, g_oracle_level);
        return;
    }
    std::printf("%-26s L%d:0x%08x\n", name, g_oracle_level, laddr);
    const std::function<uint64_t(const Args&)> synced = [&port](const Args& a) { vu0_sync(); return port(a); };
    if (!check_fn(name, laddr, n, ret, gen, synced, Scope::FullRam, 0, 0)) ++g_fail;
}

// the level after its first gameplay frame (the port's) and its view set up, as batch 509
std::vector<uint8_t> g_snap;
int g_snap_level = -1;
void level() {
    uint8_t* ram = ee_get_ram_base();
    if (g_snap.empty() || g_snap_level != g_oracle_level) {
        veldin_boot();
        level_setup_body(1, 0);
        w32(OV(0x0016c060), 0xf);
        w32(0x0013f2e8, 0xffffffffu);
        gameplay_frame();
        if (ee_read<uint16_t>(0x001518d0) == 0) { w16(0x001518d0, 640); w16(0x001518d2, 448); }
        screen_setup();
        projection_setup();
        render_view_setup();
        g_snap.assign(ram, ram + 0x02000000);
        g_snap_level = g_oracle_level;
    } else {
        std::memcpy(ram, g_snap.data(), g_snap.size());
    }
}

uint32_t pvar(uint32_t m) { return ee_read<uint32_t>(m + 0x78); }
uint32_t first_moby() { return ee_read<uint32_t>(OV(0x0015ffd8)); }
uint32_t end_moby() { return ee_read<uint32_t>(OV(0x0015ffe0)); }
bool live(uint32_t m) { return ee_read<uint8_t>(m + 0x20) < 0xfe; }

// the level's live mobys whose update function is `fn` (the level's address)
std::vector<uint32_t> mobys_with(uint32_t fn) {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < end_moby(); m += 0x100)
        if (live(m) && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
    return v;
}
// the level's mobys of class `cls`, live or not (a class the level places asleep)
std::vector<uint32_t> placed_of(uint32_t cls) {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < end_moby(); m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) != 0xff && ee_read<uint16_t>(m + 0xa6) == cls) v.push_back(m);
    return v;
}
// the level's live mobys (with a class)
std::vector<uint32_t> live_mobys() {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < end_moby(); m += 0x100)
        if (live(m) && ee_read<uint32_t>(m + 0x24) != 0) v.push_back(m);
    return v;
}

uint32_t pick(const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; }
uint32_t index_of(uint32_t m) { return (m - first_moby()) >> 8; }

// a point `r0`..`r1` from `at` (horizontally), `z0`..`z1` above it
Vec4 around(const Vec4& at, float r0, float r1, float z0 = -1.0f, float z1 = 2.0f) {
    const float r = rf(r0, r1), a = rf(-3.1f, 3.1f);
    return Vec4{at.x + r * std::cos(a), at.y + r * std::sin(a), at.z + rf(z0, z1), at.w};
}

// a hit on the moby: its slot in the hit table (0x178100, 64-byte entries) naming it, with a damage mask (the `kinds` bits
// mixed with random ones), a damage at +0x2c and a hitter at +0x20 (batch 509's); or none (0xff)
void give_hit(uint32_t m, uint32_t kinds) {
    if (ri(0, 3) == 0) { w8(m + 0xa4, 0xff); return; }
    const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
    w8(m + 0xa4, idx);
    w32(e + 0x34, ri(0, 8) == 0 ? first_moby() : m);
    w32(e + 0x24, ri(0, 2) == 0 ? (R() & ~kinds) : (kinds | (R() & 0xff)));
    { static const float dm[] = {-1.0f, 0.0f, 0.5f, 5.0f, 12.0f}; wf(e + 0x2c, dm[ri(0, 4)]); }
    wfrand(e, 4, -1, 1);
    w32(e + 0x20, ri(0, 3) == 0 ? 0u : (coin() ? ee_read<uint32_t>(0x001413d0) : first_moby() + 0x100 * u(ri(0, 40))));
    // the hit's kind and strength (+0x28 / +0x29: the reaction moby_hit_react_r picks), its weapon (+0x2a), the push applied
    // or not (+0x1c the marker 5627.92: the kind kept, else 8)
    w8(e + 0x28, u(ri(0, 8)));
    w8(e + 0x29, u(ri(0, 4)));
    w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
    w32(e + 0x1c, coin() ? 0x45afdf66u : 0u);
}

// another live moby (made one that takes contact records: +0x34 bit 0x4000) with the subject `m` moved beside it, facing it
// (its matrix updated): a swing or a beam of `m` reaches it
uint32_t beside_moby(uint32_t m) {
    std::vector<uint32_t> others, mates;
    for (const uint32_t o : live_mobys()) {
        if (o == m || ee_read<uint32_t>(o + 0x78) == 0) continue;
        others.push_back(o);
        if (ee_read<uint32_t>(o + 0x74) == ee_read<uint32_t>(m + 0x74)) mates.push_back(o);  // (its class's: they take hits)
    }
    if (others.empty()) return 0;
    if (!mates.empty() && coin()) others = mates;
    const uint32_t o = pick(others);
    w16(o + 0x34, ee_read<uint16_t>(o + 0x34) | 0x4000u);
    w8(o + 0xa4, 0xff);
    const Vec4 at = ee_ref<Vec4>(o + 0x10);
    ee_ref<Vec4>(m + 0x10) = around(at, 0.6f, 1.2f, -0.3f, 0.0f);
    const Vec4 p = ee_ref<Vec4>(m + 0x10);
    wf(m + 0x48, std::atan2(at.y - p.y, at.x - p.x));
    moby_update_transform(m);
    return o;
}
// a hit-table entry (as sub_0025a420 returns one), its direction +0x10 random, or none
uint32_t a_hit() {
    if (ri(0, 3) == 0) return 0;
    const uint32_t e = OV(0x00178100) + (u(ri(0, 15)) << 6);
    wfrand(e, 8, -1, 1);
    w32(e + 0x20, ri(0, 3) == 0 ? 0u : ee_read<uint32_t>(0x001413d0));
    return e;
}

// a run of the port on the generator's state that throws (a pointer outside RAM) asks for another state; the RAM restored
bool trial(const std::function<void()>& port) {
    static std::vector<uint8_t> saved;
    uint8_t* ram = ee_get_ram_base();
    saved.assign(ram, ram + 0x02000000);
    bool ok = true;
    const uint32_t stack_top = ee_stack_ptr();
    try { port(); } catch (const std::exception&) { ok = false; }
    ee_stack_ptr() = stack_top;
    std::memcpy(ram, saved.data(), saved.size());
    return ok;
}

// An update on one of the subjects (dealt again when the port's run throws, as batch 509's classcheck)
void classcheck(const char* name, uint32_t laddr, const std::function<std::vector<uint32_t>()>& subjects, void (*port)(uint32_t),
                const std::function<void(uint32_t)>& vary, int n) {
    if (!selected(name)) return;
    if (laddr == 0) {
        std::printf("%-26s not on level %d\n", name, g_oracle_level);
        return;
    }
    level();
    const std::vector<uint32_t> probe = subjects();
    if (probe.empty()) { std::printf("%-26s L%d:0x%08x  no subject on this level\n", name, g_oracle_level, laddr); ++g_fail; return; }
    fcheck(name, laddr, n, Ret::None, [subjects, vary, port] {
        uint32_t m = 0;
        for (int attempt = 0; attempt < 40; ++attempt) {
            level();
            const std::vector<uint32_t> ms = subjects();
            m = pick(ms);
            vary(m);
            if (trial([port, m] { port(m); })) break;
        }
        return Args{{m}, {}};
    }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
}

// ---- class 1302, the Gadgebots' entrance (shared_class1302.cpp)
// the classes the entrance opens (class1302_open's cases) and one it leaves
constexpr uint32_t kOpened[] = {1028, 1015, 1282, 1346, 857};

// a moby for the entrance to open: the level's own of a class it opens, or a live one
uint32_t opened_moby() {
    std::vector<uint32_t> own;
    for (const uint32_t c : kOpened)
        for (const uint32_t o : placed_of(c)) own.push_back(o);
    const uint32_t o = !own.empty() && ri(0, 3) != 0 ? pick(own) : pick(live_mobys());
    // (the level places few of them: Blarg one class-1028 moby, no 1015 / 1282 / 1346; another moby given the class's number)
    if (ri(0, 2) == 0) w16(o + 0xa6, kOpened[ri(0, 4)]);
    w8(o + 0x20, ri(0, 2) == 0 ? u(ri(0, 6)) : u(ri(1, 2)));
    return o;
}

void class1302_checks() {
    const uint32_t upd = copy_here(0x00307d30, 0x002e7cd8);
    const uint32_t open = copy_here(0x00307c48, 0x002e7bf0);
    if (upd != 0 && info()) {
        level();
        std::printf("INFO class 1302: %zu awake (%zu placed); opened classes:", mobys_with(upd).size(), placed_of(1302).size());
        for (const uint32_t c : kOpened) std::printf(" %u: %zu", c, placed_of(c).size());
        std::printf("\n");
    }
    // the level's entrances, or (none awake) live mobys given the entrance's pvars in the test area
    auto entrances = [upd] {
        const std::vector<uint32_t> own = mobys_with(upd);
        return own.empty() ? live_mobys() : own;
    };
    for (int st = 0; st < 4; ++st)
        classcheck("class1302_update", upd, entrances, class1302_update, [st, upd](uint32_t m) {
            if (ee_read<uint32_t>(m + 0x74) != upd) {
                w32(m + 0x78, TEST_AREA + 0x800);
                w32(pvar(m) + 0x10, u(ri(0, 20)));
            }
            const uint32_t pv = pvar(m);
            // the mobys it opens: none (-1), the first none (the update does nothing), or the level's
            for (uint32_t k = 0; k < 4; ++k) w32(pv + k * 4, ri(0, 3) == 0 ? 0xffffffffu : index_of(opened_moby()));
            if (ri(0, 12) == 0) w32(pv, 0xffffffffu);
            // the count left: none, below ten (one digit), ten and up (two)
            const int k = ri(0, 3);
            w32(pv + 0x14, k == 0 ? u(ri(-2, 0)) : (k == 1 ? u(ri(1, 9)) : u(ri(10, 99))));
            wf(pv + 0x18, rf(-3.1f, 3.1f));
            wf(pv + 0x28, rf(-3.1f, 3.1f));
            const float full = ee_read<float>(g_oracle_level == 6 ? 0x001622ecu : 0x00161f34u);
            const int s = ri(0, 5);  // (hidden, full, about to vanish, about to be full, anywhere)
            wf(pv + 0x24, s == 0 ? 0.0f : (s == 1 ? full : (s == 2 ? rf(0.0f, 0.05f) : (s == 3 ? full - rf(0.0f, 0.1f) : rf(0.0f, full)))));
            wf(m + 0x48, rf(-3.1f, 3.1f));
            // the camera within 32 units, between 32 and 36, or past 36
            const int c = ri(0, 2);
            ee_ref<Vec4>(OV(0x00166dc0)) = around(ee_ref<Vec4>(m + 0x10), c == 0 ? 0.0f : (c == 1 ? 32.2f : 36.5f), c == 0 ? 31.5f : (c == 1 ? 35.5f : 80.0f), -0.5f, 0.5f);
            w8(0x001413f4, ri(0, 2) != 0 ? 1u : u(ri(0, 3)));
            w8(0x0015edb4, u(ri(0, 1)));
            w8(m + 0xb0, u(ri(0, 15)));
            const uint32_t mark = 0x0014c050 + (ee_read<uint32_t>(0x0015ed84) << 4) + ee_read<uint8_t>(m + 0xb0);
            w8(mark, coin() ? 0xffu : u(ri(0, 2)));
            w8(m + 0x20, u(st == 3 ? ri(3, 9) : st));
            seed_rand();
        }, st == 1 ? 120 : 40);

    fcheck("class1302_open", open, 80, Ret::None, [entrances] {
        level();
        const uint32_t m = pick(entrances());
        const uint32_t o = opened_moby();
        w8(m + 0xb0, u(ri(0, 15)));
        return Args{{m, o}, {}};
    }, [](A a) { class1302_open(a.i[0], a.i[1]); return uint64_t{0}; });
}
// ---- class 1068, its beam and its shards; class 302 (shared_class1068.cpp, shared_beam.cpp, shared_class302.cpp)
// the robot's beam dealt: its life (+0x21c, a frame count as a float: out, the last frame, at 20 / 21, running), its origin
// at the moby and its points about it with strengths (the ends 0, full, about 0.6, fading)
void beam_state(uint32_t m) {
    const uint32_t pv = pvar(m);
    static const float lives[] = {0.0f, 1.0f, 2.0f, 20.0f, 21.0f, 40.0f, 150.0f};
    wf(pv + 0x21c, lives[ri(0, 6)]);
    if (coin()) return;  // (the level's own beam words)
    const Vec4 at = ee_ref<Vec4>(m + 0x10);
    if (ri(0, 2) == 0) {  // (across another moby, which takes contact records: the strikes between strong points reach it)
        std::vector<uint32_t> others, mates;
        for (const uint32_t o : live_mobys()) {
            if (o == m) continue;
            others.push_back(o);
            if (ee_read<uint32_t>(o + 0x74) == ee_read<uint32_t>(m + 0x74)) mates.push_back(o);  // (its class's: they take hits)
        }
        const uint32_t o = pick(!mates.empty() && coin() ? mates : others);
        w16(o + 0x34, ee_read<uint16_t>(o + 0x34) | 0x4000u);
        w8(o + 0xa4, 0xff);
        const Vec4 c = ee_ref<Vec4>(o + 0x10);
        const float a = rf(-3.1f, 3.1f), z = rf(0.2f, 1.2f);
        for (uint32_t k = 0; k < 16; ++k) {
            const float d = (static_cast<float>(k) - 7.5f) * 0.15f;
            ee_ref<Vec4>(pv + 0x230 + k * 0x10) = Vec4{c.x + d * std::cos(a), c.y + d * std::sin(a), c.z + z, ri(0, 3) == 0 ? 0.6f : 1.0f};
        }
        return;
    }
    ee_ref<Vec4>(pv + 0x210) = Vec4{at.x, at.y, at.z + rf(0.5f, 2.0f), 1.0f};
    ee_ref<Vec4>(pv + 0x220) = coin() ? Vec4{0.0f, 0.0f, 1.0f, 0.0f} : Vec4{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), 0.9f, 0.0f};
    const float a0 = rf(-3.1f, 3.1f), spread = rf(0.5f, 6.0f);
    for (uint32_t k = 0; k < 16; ++k) {
        const float a = a0 + static_cast<float>(k) * 0.1f, r = coin() ? rf(0.1f, 1.0f) : rf(1.0f, spread);
        static const float ws[] = {0.0f, 1.0f, 1.0f, 0.98f, 0.6f, 0.59f, 0.3f};
        ee_ref<Vec4>(pv + 0x230 + k * 0x10) = Vec4{at.x + r * std::cos(a), at.y + r * std::sin(a), at.z + rf(0.0f, 1.5f), ws[ri(0, 6)]};
    }
}

void class1068_checks() {
    const uint32_t upd = copy_here(0x002fdbd0, 0x002dabe8);
    const uint32_t think = copy_here(0x002feb40, 0x002dbb58);
    const uint32_t run = copy_here(0x002ff2c8, 0x002dc2e0);
    const uint32_t start = copy_here(0x002ff100, 0x002dc118);
    const uint32_t u302 = copy_here(0x002d85a0, 0x002bd0e8);
    const uint32_t brk = copy_here(0x002d8178, 0x002bccc0);
    const uint32_t shard = copy_here(0x00300c60, 0x002dd0e0);
    if (upd != 0 && info()) {
        level();
        std::printf("INFO class 1068: %zu awake (%zu placed); class 302: %zu awake (%zu placed)\n", mobys_with(upd).size(),
                    placed_of(1068).size(), mobys_with(u302).size(), placed_of(302).size());
        for (const uint32_t m : mobys_with(upd)) {
            const uint32_t pv = pvar(m);
            std::printf("INFO   1068 at %08x: path %d back %d box %d walls %08x target %08x\n", m, ee_read<int32_t>(pv + 0x180),
                        ee_read<int32_t>(pv + 0x1a8), ee_read<int32_t>(pv + 0x1ac), ee_read<uint32_t>(pv + 0x1a4), ee_read<uint32_t>(pv + 0x160));
        }
    }
    // (the segment record 0x173e90 .. 0x173e9c that a moby's collision leaves: VU0 leftovers in the original that the port of
    // segment_collide does not reproduce, batch 506's difference; the beam and the arm reach mobys)
    const size_t ign0 = g_ignored_words.size();
    for (uint32_t k = 0; k < 0x10; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
    auto robots = [upd] { return mobys_with(upd); };
    // the robot's surroundings and state words dealt (the level's paths, boxes and walls kept: swapped between the robots)
    // (`st`: the state the case runs, -1 any; the tests of its state dealt more often)
    auto vary = [robots](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        const std::vector<uint32_t> all = robots(), lives = live_mobys();
        if (all.size() > 1 && ri(0, 3) == 0) {
            const uint32_t o = pvar(pick(all));
            for (const uint32_t k : {0x180u, 0x1a4u, 0x1a8u, 0x1acu}) w32(pv + k, ee_read<uint32_t>(o + k));
        }
        if (st == 1 ? ri(0, 3) != 0 : ri(0, 3) == 0) w32(pv + 0x1ac, 0xffffffffu);
        if (st != 14 && ri(0, 4) == 0) w32(pv + 0x1a8, 0xffffffffu);
        const int t = ri(0, 3);
        const uint32_t target = t == 0 ? 0u : (t == 1 ? pick(lives) : ee_read<uint32_t>(0x001413d0));
        w32(pv + 0x160, target);
        w32(pv + 0x164, ri(0, 2) == 0 ? 2u : u(ri(0, 1)));
        w32(pv + 0x188, ri(0, st == 11 ? 4 : 2) == 0 ? 0u : u(ri(1, 200)));
        if (coin()) w8(pv + 0x28, u(ri(0, 3)));  // (the hit record's kind: the reactions' row)
        w32(pv + 0x38, ri(0, 5) == 0 ? 1u : 0u);
        w32(pv + 0x19c, ri(0, 2) == 0 ? u(ri(0, 2)) : u(ri(3, 100)));
        static const float hs[] = {0.0f, 0.4f, 2.0f, 6.0f};
        if (st < 0 ? coin() : ri(0, 3) == 0) wf(pv + 0x20, hs[ri(0, 3)]);
        w8(0x001413f4, ri(0, 2) == 0 ? 1u : u(ri(0, 3)));
        // the hero (and the target) near or far, in the guard box or not
        const Vec4 at = ee_ref<Vec4>(m + 0x10);
        ee_ref<Vec4>(0x0013f3d0) = around(at, 0.0f, coin() ? 6.0f : 40.0f, -1.0f, 2.0f);
        if (ri(0, 3) == 0) ee_ref<Vec4>(0x0013f3d0).z += coin() ? rf(3.0f, 6.0f) : -rf(3.0f, 6.0f);  // (the target's height out of reach)
        const int32_t box = ee_read<int32_t>(pv + 0x1ac);
        if (box >= 0 && box < 0x400 && coin()) ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(box) * 0x80 + 0x30);
        ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10) = ee_ref<Vec4>(0x0013f3d0);
        if (target != 0 && target != ee_read<uint32_t>(0x001413d0) && coin()) ee_ref<Vec4>(target + 0x10) = around(at, 0.0f, coin() ? 3.0f : 30.0f, -0.5f, 0.5f);
        // back at its path's end, or home, or near either
        if (st == 14 && ee_read<int32_t>(pv + 0x1a8) < 0) {  // (a path back of another robot's)
            for (const uint32_t o : all)
                if (ee_read<int32_t>(pvar(o) + 0x1a8) >= 0) w32(pv + 0x1a8, ee_read<uint32_t>(pvar(o) + 0x1a8));
        }
        const int32_t back = ee_read<int32_t>(pv + 0x1a8);
        if (back >= 0 && back < 512 && (st == 14 ? ri(0, 3) != 0 : ri(0, 2) == 0)) {
            const uint32_t p = ee_read<uint32_t>(OV(0x001b04b0) + u(back) * 4);
            if (p >= 0x00100000u && p < 0x02000000u) {
                const int k = ri(0, 2);  // (within its 0.25, about it, or a walk away)
                ee_ref<Vec4>(m + 0x10) = around(ee_ref<Vec4>(p + ee_read<uint32_t>(p) * 0x10), k == 0 ? 0.0f : (k == 1 ? 0.2f : 0.3f), k == 0 ? 0.25f : (k == 1 ? 0.3f : 3.0f), 0.0f, 0.0f);
            }
        }
        if (ri(0, 3) == 0) ee_ref<Vec4>(pv + 0x170) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 1.2f : 8.0f, -0.3f, 0.3f);
        wf(m + 0x48, rf(-3.1f, 3.1f));
        wf(pv + 0x18c, rf(-0.2f, 0.2f));
        // its animation: the set (+0x53) and its frame (the swing's 9 .. 15 often)
        w8(m + 0x53, u(ri(0, 6)));
        w8(m + 0x52, coin() ? ee_read<uint8_t>(m + 0x53) : u(ri(0, 6)));
        if (coin()) {
            wf(m + 0x54, 0.0f);
            const uint32_t cur = ee_read<uint8_t>(m + 0x52) == 0xff ? ee_read<uint32_t>(m + 0x6c) : ee_read<uint32_t>(m + 0x68);
            if (cur >= 0x00100000u && cur < 0x02000000u) w16(cur + 4, u(ri(120, 300)));
        }
        w8(m + 0x70, R() & 0xff);
        w8(m + 0xbc, u(ri(0, 3)));
        w8(m + 0x31, u(ri(0, 1)));
        if (coin()) ee_ref<Vec4>(OV(0x00166dc0)) = around(at, 0.0f, coin() ? 26.0f : 60.0f);
        ee_ref<Vec4>(pv + 0x200) = around(at, 0.0f, 1.5f, 0.0f, 2.0f);
        const bool keep_place = (st == 1 || st == 14) && coin();  // (the state's own placing kept)
        if (!keep_place && ri(0, 3) == 0) {
            const uint32_t o = beside_moby(m);
            if (o != 0 && ri(0, 2) != 0) w32(pv + 0x160, o);
            ee_ref<Vec4>(pv + 0x200) = around(ee_ref<Vec4>(m + 0x10), 0.5f, 1.5f, 0.3f, 1.5f);
        }
        const int32_t walls = ee_read<int32_t>(pv + 0x1a4);
        if (!keep_place && walls >= 0 && walls < 512 && (st == 11 ? coin() : ri(0, 3) == 0)) {  // (at a point of its walls' path, the target beyond: the chase meets the wall)
            const uint32_t p = ee_read<uint32_t>(OV(0x001b04b0) + u(walls) * 4);
            if (p >= 0x00100000u && p < 0x02000000u && ee_read<int32_t>(p) > 0) {
                bool continue_walls = true;
                const int32_t n = ee_read<int32_t>(p);
                const uint32_t k = u(ri(0, n - 1));
                ee_ref<Vec4>(m + 0x10) = around(ee_ref<Vec4>(p + 0x10 + k * 0x10), 0.1f, 0.7f, 0.0f, 0.0f);
                if (n >= 2 && coin()) {  // (just off the middle of a segment, the hero straight across it)
                    const uint32_t j = u(ri(0, n - 2));
                    const Vec4 a = ee_ref<Vec4>(p + 0x10 + j * 0x10), b = ee_ref<Vec4>(p + 0x20 + j * 0x10);
                    float nx = a.y - b.y, ny = b.x - a.x;
                    const float l = std::sqrt(nx * nx + ny * ny);
                    if (l > 0.01f) {
                        nx /= l;
                        ny /= l;
                        const float side = coin() ? 1.0f : -1.0f, off = rf(0.2f, 0.7f), far = rf(3.0f, 8.0f), t = rf(0.3f, 0.7f);
                        const Vec4 c{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, 1.0f};
                        ee_ref<Vec4>(m + 0x10) = Vec4{c.x + side * off * nx, c.y + side * off * ny, c.z, ee_ref<Vec4>(m + 0x10).w};
                        ee_ref<Vec4>(0x0013f3d0) = Vec4{c.x - side * far * nx, c.y - side * far * ny, c.z, ee_ref<Vec4>(0x0013f3d0).w};
                        ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10) = ee_ref<Vec4>(0x0013f3d0);
                    }
                    continue_walls = false;
                }
                if (continue_walls) {
                    const uint32_t tg = ee_read<uint32_t>(pv + 0x160);
                    if (tg != 0 && tg != ee_read<uint32_t>(0x001413d0)) ee_ref<Vec4>(tg + 0x10) = around(ee_ref<Vec4>(m + 0x10), 2.0f, 10.0f, -0.5f, 0.5f);
                    ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 2.0f, 10.0f, -0.5f, 0.5f);  // (the hero too: the pick's)
                    ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10) = ee_ref<Vec4>(0x0013f3d0);
                }
            }
        }
        // turned toward its target or the hero (the target the pick most often finds), about the 10 / 15 / 45 degrees of its
        // tests
        const uint32_t tg = ee_read<uint32_t>(pv + 0x160) != 0 && coin() ? ee_read<uint32_t>(pv + 0x160) : ee_read<uint32_t>(0x001413d0);
        if (st == 1 || coin()) {
            const Vec4 q = ee_ref<Vec4>(tg + 0x10), p = ee_ref<Vec4>(m + 0x10);
            wf(m + 0x48, std::atan2(q.y - p.y, q.x - p.x) + rf(-0.9f, 0.9f) * (coin() ? 0.3f : 1.0f));
            if (coin()) wf(pv + 0x18c, 0.0f);
        }
        beam_state(m);
        // a hit (in a state's own run less often: a hit's reaction takes the robot out of its state)
        if (st < 0 || ri(0, 2) == 0) give_hit(m, 0x330000u);
        else w8(m + 0xa4, 0xff);
        if (g_oracle_level == 6 && ri(0, 5) == 0) w32(0x0015ed84, 10);  // (Orxon's hint: the planet word)
        w8(0x0013d419, u(ri(0, 1)));
        w32(OV(0x0015f5cc), R() & 0xffff);
    };
    for (int st = 0; st < 18; ++st) {
        // (the dying state's three pieces: debris_spawn leaves in each debris record's +0xc / +0x1c the w lanes of a VU0 sin /
        // cos in the original, which its port does not; the records follow the moby table: batches 117 / 506's difference)
        const size_t ign = g_ignored_words.size();
        // (the scratchpad words the pose function 0x245f18 spills the caller's $t8 / $t9 into: the original's stale registers,
        // which the ports have not; batch 120's)
        for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);
        if (st == 16) {
            level();
            for (uint32_t a = end_moby(); a < end_moby() + 0x20000; a += 0x80) {
                g_ignored_words.push_back(a + 0xc);
                g_ignored_words.push_back(a + 0x1c);
            }
        }
        classcheck("class1068_update", upd, robots, class1068_update, [st, vary](uint32_t m) {
            if (st != 0 && coin()) {  // (the state 0 first, the port's)
                w8(m + 0x20, 0);
                const uint32_t sp0 = ee_stack_ptr();
                try { class1068_update(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
            }
            vary(m, st);
            w8(m + 0x20, u(st));
            seed_rand();
        }, st == 0 || st >= 17 ? 20 : 40);
        g_ignored_words.resize(ign);
    }

    // the frame's common part: the hit reactions (the hit's slot, the damage against the health), dying or not
    classcheck("class1068_think", think, robots, class1068_think, [vary](uint32_t m) {
        vary(m, -1);
        if (ri(0, 3) == 0) w8(m + 0x20, 0x10);
        if (ee_read<uint8_t>(m + 0xa4) != 0xff) {  // (the hitter: the hero, a class-794 moby for Orxon's hint, any)
            const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
            const uint32_t h = ee_read<uint32_t>(e + 0x20);
            if (h != 0 && coin()) w16(h + 0xa6, 0x31a);
        }
        seed_rand();
    }, 120);

    classcheck("class1068_beam_run", run, robots, class1068_beam_run, [vary](uint32_t m) {
        vary(m, -1);
        beam_state(m);
        seed_rand();
    }, 80);

    classcheck("class1068_beam_start", start, robots, class1068_beam_start, [vary](uint32_t m) {
        vary(m, -1);
        if (coin()) wf(m + 0x18, ee_read<float>(m + 0x18) + rf(-1.0f, 1.0f));
        seed_rand();
    }, 40);

    g_ignored_words.resize(ign0);

    // class 302: its lights (the level's, or live mobys), each state, hit or not (mask 0x10000), shown or not
    auto lights = [u302] {
        const std::vector<uint32_t> own = mobys_with(u302);
        return own.empty() ? live_mobys() : own;
    };
    for (int st = 0; st < 4; ++st)
        classcheck("class302_update", u302, lights, class302_update, [st](uint32_t m) {
            give_hit(m, 0x10000u);
            w8(m + 0x31, u(ri(0, 1)));
            w8(m + 0xbc, u(ri(0, 2) == 0 ? ri(0, 1) : ri(2, 30)));
            w32(OV(0x0015f5cc), coin() ? (R() & 0xffff) : R());
            w8(m + 0x20, u(st == 3 ? ri(3, 9) : st));
            seed_rand();
        }, st == 1 ? 60 : 20);
    fcheck("class302_break", brk, 40, Ret::None, [lights] {
        level();
        const uint32_t m = pick(lights());
        const uint32_t hit = a_hit();
        if (coin()) ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 0.5f, 10.0f);
        seed_rand();
        return Args{{m, hit}, {}};
    }, [](A a) { class302_break(a.i[0], a.i[1]); return uint64_t{0}; });

    // a shard spawned at a point near the hero with a speed (the original's a0, a moby, unread)
    fcheck("beam_shard_spawn", shard, 40, Ret::Int32, [] {
        level();
        ee_ref<Vec4>(TEST_AREA) = around(ee_ref<Vec4>(0x0013f3d0), 0.0f, 3.0f);
        wfrand(TEST_AREA + 0x10, 4, -0.3f, 0.3f);
        seed_rand();
        return Args{{first_moby(), TEST_AREA, TEST_AREA + 0x10}, {}};
    }, [](A a) { return r32(beam_shard_spawn(a.i[1], a.i[2])); });
}
// ---- class 857, the Gadgebot (shared_class857.cpp), and the lights it spawns (shared_class302.cpp)
// the level's mobys whose class's target kind (+0x46 of the class) is 5 (what the Gadgebots attack)
std::vector<uint32_t> targets_of_kind5() {
    std::vector<uint32_t> v;
    for (const uint32_t o : live_mobys()) {
        const uint32_t cls = ee_read<uint32_t>(o + 0x24);
        if (cls != 0 && ee_read<int16_t>(cls + 0x46) == 5) v.push_back(o);
    }
    return v;
}
// the level's list 0x1aba00 (null-ended)
std::vector<uint32_t> level_list() {
    std::vector<uint32_t> v;
    for (uint32_t p = OV(0x001aba00); ee_read<uint32_t>(p) != 0 && v.size() < 4000; p += 4) v.push_back(ee_read<uint32_t>(p));
    return v;
}

// Blarg's Clank section on RAM dumps (batch 151's: its Gadgebots awake and following, the ring open; skipped without them)
const std::vector<uint8_t>& clank_dump(int k) {
    static std::vector<uint8_t> d[2];
    static bool read[2] = {false, false};
    if (!read[k]) {
        read[k] = true;
        if (FILE* fp = std::fopen(dump_path(k == 0 ? "clank_0415/ring_3015.bin" : "clank_0415/ring_3815.bin").c_str(), "rb")) {
            d[k].resize(0x02000000 + 0x4000);
            if (std::fread(d[k].data(), 1, d[k].size(), fp) != d[k].size()) d[k].clear();
            std::fclose(fp);
        }
    }
    return d[k];
}
// the state the Gadgebots' cases start from: on Blarg mostly a Clank-section dump (the level's own are asleep in state 0
// after the first frame), else the level after its first frame
void gadgebot_base() {
    if (g_oracle_level == 6 && ri(0, 3) != 0) {
        const std::vector<uint8_t>& d = clank_dump(ri(0, 1));
        if (!d.empty()) {
            std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
            std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
            return;
        }
    }
    level();
}
std::vector<uint32_t> gadgebots() {
    std::vector<uint32_t> v;
    for (const uint32_t m : placed_of(857))
        if (ee_read<uint8_t>(m + 0x20) < 0xfe && pvar(m) != 0) v.push_back(m);
    return v;
}
// the mobys a Gadgebot goes to (classes 945, 1024, 1301, 351, 1302) of its group or list; some made one (a live moby with
// pvars given the class: the levels place few or none of 945 / 1024)
std::vector<uint32_t> gadgebot_spots(uint32_t m) {
    std::vector<uint32_t> v;
    for (const uint32_t c : {945u, 1024u, 1301u, 351u, 1302u})
        for (const uint32_t o : placed_of(c)) v.push_back(o);
    const std::vector<uint32_t> list = level_list();
    if (v.empty() || coin()) {
        std::vector<uint32_t> others;
        for (const uint32_t o : list.empty() ? live_mobys() : list)
            if (o != m && pvar(o) != 0 && ee_read<int16_t>(o + 0xa6) != 857) others.push_back(o);
        if (!others.empty()) {
            const uint32_t o = pick(others);
            static const uint16_t cls[] = {945, 1024, 1301, 351, 1302};
            w16(o + 0xa6, cls[ri(0, 4)]);
            w32(pvar(o), ri(0, 3) == 0 ? 0xffffffffu : index_of(pick(live_mobys())));  // (a slot's linked moby, or none)
            v.push_back(o);
        }
    }
    return v;
}
// a spot dealt: its state (2 or not), its pvars' first word (the linked moby's index, or -1) and its place count, the
// Gadgebot `m` at its goal (within 1) or not, level with it or not
void spot_state(uint32_t m, uint32_t o) {
    const uint32_t opv = pvar(o);
    w8(o + 0x20, ri(0, 2) == 0 ? 2u : u(ri(0, 4)));
    const std::vector<uint32_t> lives = live_mobys();
    if (opv != 0) {
        w32(opv, ri(0, 5) == 0 ? 0xffffffffu : index_of(pick(lives)));
        w32(opv + 4, u(ri(0, 5)));
        w32(opv + 0x14, u(ri(0, 3)));  // (an entrance's count of Gadgebots still to come)
    }
    const int16_t cls = ee_read<int16_t>(o + 0xa6);
    Vec4 goal = ee_ref<Vec4>(o + 0x10);
    if (cls != 1301 && cls != 351) {
        const Vec4 x = ee_ref<Vec4>(o + 0xc0);
        goal = Vec4{goal.x + x.x, goal.y + x.y, goal.z + x.z, goal.w};
    }
    const int k = ri(0, 3);
    Vec4 at = around(goal, k == 0 ? 0.0f : (k == 1 ? 0.8f : 1.1f), k == 0 ? 0.8f : (k == 1 ? 1.1f : 14.0f), 0.0f, 0.0f);
    if (ri(0, 3) == 0) at.z += coin() ? rf(0.2f, 1.4f) : rf(1.5f, 3.0f);
    ee_ref<Vec4>(m + 0x10) = Vec4{at.x, at.y, at.z, ee_ref<Vec4>(m + 0x10).w};
}

void class857_checks() {
    const uint32_t upd = copy_here(0x002f0040, 0x002d4078);
    if (upd != 0 && info()) {
        level();
        std::printf("INFO class 857: %zu awake (%zu placed); list %zu; kind-5 targets %zu;", mobys_with(upd).size(), placed_of(857).size(),
                    level_list().size(), targets_of_kind5().size());
        for (const uint32_t c : {945u, 1024u, 1301u, 351u, 1015u, 1033u, 1302u, 302u, 303u}) std::printf(" %u: %zu", c, placed_of(c).size());
        std::printf("\n");
        for (int k = 0; k < 2; ++k) {
            const std::vector<uint8_t>& d = clank_dump(k);
            if (g_oracle_level != 6 || d.empty()) continue;
            std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
            std::printf("INFO dump %d: Gadgebots", k);
            for (const uint32_t m : gadgebots()) std::printf(" %08x st %u sub %u", m, ee_read<uint8_t>(m + 0x20), ee_read<uint8_t>(m + 0xbc));
            std::printf("\n");
        }
    }
    if (g_oracle_level == 6 && (clank_dump(0).empty() || clank_dump(1).empty()))
        std::printf("class857: no Clank-section dumps (tools/oracle/blarg_tour.sh clankdumps: clank_0415): Blarg's cases from the level only\n");
    // (the segment record 0x173e90 .. 0x173e9c that a moby's collision leaves, and the collision results' w lanes: VU0
    // leftovers in the original that the ports of segment_collide / sphere_collide do not reproduce, batch 506's difference)
    const size_t ign0 = g_ignored_words.size();
    for (uint32_t k = 0; k < 0x10; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
    // (the scratchpad words the pose function 0x245f18 spills the caller's $t8 / $t9 into: batch 120's)
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);

    // (a Gadgebot reaching a slot sparkles out with ten particles whose speed is a stack vector the original never sets: its
    // x / y and the sparkles' places' w lanes are what an earlier call left on the stack (the ports' frames are fresh); then
    // it moves to its place by that vector, its position's w lane (and its pvars' copies of the position, +0x2e0 / +0x2f0)
    // that stack lane too; the knocked-away landing's sparkles likewise. Left out: those words of the particles (+0x1c,
    // +0x2c, +0x30 of each record of the pool) and of the Gadgebots)
    gadgebot_base();
    {
        const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
        for (uint32_t k = 0; k < 0x800; ++k)
            for (const uint32_t o : {0x1cu, 0x2cu, 0x30u}) g_ignored_words.push_back(pool + k * 0x40 + o);
        for (const uint32_t m : placed_of(857))
            for (const uint32_t a : {m + 0x1c, pvar(m) + 0x2ec, pvar(m) + 0x2fc}) g_ignored_words.push_back(a);
    }
    // the Gadgebot's state and surroundings dealt (`st` the state its case runs)
    auto vary = [](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        const std::vector<uint32_t> lives = live_mobys(), bots = gadgebots(), kind5 = targets_of_kind5();
        const Vec4 at = ee_ref<Vec4>(m + 0x10);
        // the hero in the command ring's range (16, 1.5 in height) or not, and his command
        Vec4 h = around(at, coin() ? 0.0f : 2.0f, coin() ? 15.0f : 40.0f, -1.0f, 1.0f);
        if (ri(0, 4) == 0) h.z += coin() ? rf(1.5f, 4.0f) : -rf(1.5f, 4.0f);
        ee_ref<Vec4>(0x0013f3d0) = h;
        wf(0x0013f5f8, h.z + (coin() ? 0.0f : rf(-2.0f, 2.0f)));
        ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10) = h;
        w32(0x00141610, u(ri(0, 5)));
        w8(0x001413f4, u(ri(0, 1)));
        w8(0x0015edb3, ri(0, 3) == 0 ? 1u : 0u);
        // the light it waits by (state 1): alive, broken, gone
        const uint32_t light = ee_read<uint32_t>(pv + 0x170);
        if (light != 0 && ri(0, 4) == 0) w32(pv + 0x170, 0);
        else if (light != 0) w8(light + 0x20, ri(0, 2) == 0 ? u(ri(2, 3)) : u(ri(0, 1)));
        // the walls it is in, their count
        if (coin()) w32(pv + 0x27c, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, std::max(0, ee_read<int32_t>(pv + 0x310) - 1))));
        if (ri(0, 4) == 0) w32(pv + 0x310, u(ri(0, 3)));
        // its target: none, an attackable moby (kind 5), a spot, a gone one
        const int t = ri(0, 4);
        uint32_t tg = 0;
        if (t == 1 && !kind5.empty()) tg = pick(kind5);
        else if (t == 2) { const std::vector<uint32_t> sp = gadgebot_spots(m); if (!sp.empty()) tg = pick(sp); }
        else if (t == 3) tg = pick(lives);
        w32(pv + 0x174, tg);
        if (tg != 0) {
            if (ri(0, 5) == 0) w8(tg + 0x20, coin() ? 0xfeu : 0xfdu);
            const int k = ri(0, 2);  // (in its reach 1.5, near, far)
            ee_ref<Vec4>(tg + 0x10) = around(at, k == 0 ? 0.0f : 1.5f, k == 0 ? 1.5f : (k == 1 ? 6.0f : 20.0f), -0.3f, 0.3f);
            if (coin()) wf(m + 0x48, std::atan2(ee_ref<Vec4>(tg + 0x10).y - at.y, ee_ref<Vec4>(tg + 0x10).x - at.x) + rf(-0.3f, 0.3f));
        }
        // the deal's frame (a new deal when it is behind the frame counter), the flags and timers
        const int32_t frame = static_cast<int32_t>(R() & 0xffff);
        w32(OV(0x0015f5cc), u(frame));
        w32(pv + 0x18c, u(coin() ? frame - ri(1, 5) : frame + ri(0, 5)));
        w16(pv + 0x19a, u(ri(0, 1)));
        w16(pv + 0x198, u(ri(0, 1)));
        w32(pv + 0x194, ri(0, 2) == 0 ? u(ri(0, 2)) : u(ri(3, 60)));
        w16(pv + 0x190, u(ri(0, 1)));
        wf(pv + 0x188, rf(0.0f, 3.0f));
        w32(pv + 0x184, coin() ? u(14 * ri(0, 5)) : u(ri(0, 80)));
        if (coin()) ee_ref<Vec4>(pv + 0xf0) = around(at, 0.0f, coin() ? 0.3f : 3.0f, 0.0f, 0.0f);
        if (coin()) ee_ref<Vec4>(pv) = around(at, 0.0f, 3.0f, 0.0f, 0.0f);
        // going in (state 8): an entrance or a pad, near
        if (st == 8 || ri(0, 5) == 0) {
            const std::vector<uint32_t> sp = gadgebot_spots(m);
            const uint32_t e = sp.empty() ? pick(lives) : pick(sp);
            w32(pv + 0x304, e);
            if (pvar(e) != 0) w32(pvar(e) + 0x14, u(ri(0, 3)));
            if (st == 8) ee_ref<Vec4>(m + 0x10) = around(ee_ref<Vec4>(e + 0x10), 0.0f, coin() ? 0.15f : 1.6f, -0.2f, 0.2f);
        }
        // a switch's box with the hero in it (state 3, the Gadgebots that have them; others given a Gadgebot's switches)
        if (st == 3 && ee_read<int32_t>(pv + 0x2c0) < 0 && coin())
            for (const uint32_t o : bots)
                if (ee_read<int32_t>(pvar(o) + 0x2c0) >= 0)
                    for (uint32_t k = 0; k < 0x40; k += 4) w32(pv + 0x2a0 + k, ee_read<uint32_t>(pvar(o) + 0x2a0 + k));
        const int32_t box = ee_read<int32_t>(pv + 0x2c0);
        if (st == 3 && box >= 0 && box < 0x400 && coin()) {
            ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(box) * 0x80 + 0x30);
            ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10) = ee_ref<Vec4>(0x0013f3d0);
            const uint32_t s = first_moby() + (ee_read<uint32_t>(pv + 0x2a0) << 8);
            if (ri(0, 2) != 0) w8(s + 0x20, coin() ? 3u : u(ri(0, 5)));
            if (ri(0, 3) == 0)  // (all its switches in state 3)
                for (uint32_t k = 0; k < 8 && ee_read<int32_t>(pv + 0x2a0 + k * 4) >= 0; ++k)
                    w8(first_moby() + (ee_read<uint32_t>(pv + 0x2a0 + k * 4) << 8) + 0x20, 3);
        }
        // a spot near (state 7, the tail's arrivals)
        if (st == 7 || ri(0, 3) == 0) {
            const std::vector<uint32_t> sp = gadgebot_spots(m);
            if (!sp.empty()) spot_state(m, pick(sp));
        }
        // its animation, its frame (the strike's 12 .. 15), the flags
        w8(m + 0x53, u(ri(0, 6)));
        w8(m + 0x52, coin() ? ee_read<uint8_t>(m + 0x53) : u(ri(0, 6)));
        if (coin()) {
            wf(m + 0x54, 0.0f);
            const uint32_t cur = ee_read<uint8_t>(m + 0x52) == 0xff ? ee_read<uint32_t>(m + 0x6c) : ee_read<uint32_t>(m + 0x68);
            if (cur >= 0x00100000u && cur < 0x02000000u) w16(cur + 4, u(ri(160, 260)));
        }
        if (st == 5 && ri(0, 2) != 0) {  // (striking: its animation 2, its frames 12 .. 15 often)
            w8(m + 0x53, 2);
            w8(m + 0x52, 2);
            wf(m + 0x54, 0.0f);
            const uint32_t cur = ee_read<uint32_t>(m + 0x68);
            if (cur >= 0x00100000u && cur < 0x02000000u) w16(cur + 4, u(ri(184, 248)));
        }
        w8(m + 0x70, R() & 0xff);
        w8(m + 0x31, ri(0, 3) == 0 ? 0u : 1u);
        if (coin()) ee_ref<Vec4>(OV(0x00166dc0)) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 16.0f : 40.0f);
        // its height: fallen below home, out of the level's box, its fall speed, its springs
        wf(pv + 0x48, coin() ? 0.0f : rf(-0.3f, 0.1f));
        wf(pv + 0x284, rf(0.0f, 0.3f));
        if (coin()) wf(m + 0x18, ee_read<float>(m + 0x18) + rf(-0.6f, 0.6f));
        if (ri(0, 6) == 0) wf(pv + 0xe8, ee_read<float>(m + 0x18) + rf(4.0f, 8.0f));
        if (ri(0, 12) == 0) wf(m + 0x10, coin() ? 1.5f : 1021.5f);
        if (ri(0, 4) == 0) {  // (the walls it may be in: inside or out of the first walls' polygon)
            const int32_t w = ee_read<int32_t>(pv + 0x1e0);
            if (w >= 0 && w < 512) {
                const uint32_t p = ee_read<uint32_t>(OV(0x001b04b0) + u(w) * 4);
                if (p >= 0x00100000u && p < 0x02000000u && ee_read<int32_t>(p) > 0) {
                    ee_ref<Vec4>(m + 0x10) = around(ee_ref<Vec4>(p + 0x10 + u(ri(0, ee_read<int32_t>(p) - 1)) * 0x10), 0.0f, 1.0f, 0.0f, 0.0f);
                    if (coin()) {  // (the hero by it: inside the same walls or not)
                        ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 1.5f, -0.3f, 0.3f);
                        ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10) = ee_ref<Vec4>(0x0013f3d0);
                    }
                }
            }
        }
        if (bots.size() > 1 && ri(0, 5) == 0) {  // (just above another Gadgebot: the way down meets it)
            uint32_t o = pick(bots);
            if (o == m) o = bots[0] == m ? bots[1] : bots[0];
            const Vec4 q = ee_ref<Vec4>(o + 0x10);
            ee_ref<Vec4>(m + 0x10) = Vec4{q.x + rf(-0.1f, 0.1f), q.y + rf(-0.1f, 0.1f), q.z + rf(0.4f, 1.6f), ee_ref<Vec4>(m + 0x10).w};
        }
        if (st == 3 && coin()) {  // (waiting: the hero about 1.8 from its spot)
            ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(pv), 1.5f, 2.6f, -0.3f, 0.3f);
            ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10) = ee_ref<Vec4>(0x0013f3d0);
            wf(0x0013f5f8, ee_read<float>(0x0013f3d8));
        }
        // a hit (mask 0x330001) from the hero, a Gadgebot or another moby
        if (ri(0, 2) == 0) {
            give_hit(m, 0x330001u);
            if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
                const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                const int k = ri(0, 2);
                w32(e + 0x20, k == 0 ? ee_read<uint32_t>(0x001413d0) : (k == 1 && !bots.empty() ? pick(bots) : pick(lives)));
            }
        } else {
            w8(m + 0xa4, 0xff);
        }
        // its checkpoint (state 0: gone when collected)
        const uint32_t mark = 0x0014c050 + (ee_read<uint32_t>(0x0015ed84) << 4) + ee_read<uint8_t>(m + 0xb0);
        if (st == 0) w8(mark, ri(0, 3) == 0 ? 0xffu : 0u);
    };
    // the sub-states each state runs through
    auto subs = [](int st) { return st == 3 ? 4 : (st == 5 ? 6 : (st == 7 ? 3 : 1)); };
    for (int st = 0; st < 11; ++st)
        for (int sub = 0; sub < subs(st); ++sub)
            classcheck("class857_update", upd, [] { gadgebot_base(); return gadgebots(); }, class857_update, [st, sub, vary, subs](uint32_t m) {
                gadgebot_base();
                vary(m, st);
                if (st == 9) {  // knocked away first (the port's react sets the jump up), then near the floor or not
                    const uint32_t pv = pvar(m);
                    w8(m + 0x20, 3);
                    w8(m + 0x52, 0);
                    w8(m + 0x53, 0);
                    give_hit(m, 0x330001u);
                    if (ee_read<uint8_t>(m + 0xa4) != 0xff) w32(OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6) + 0x20, first_moby());
                    class857_react(m);
                    if (coin()) wf(pv + 0x88, -rf(0.0f, 0.5f));
                }
                w8(m + 0x20, u(st));
                if (subs(st) > 1) w8(m + 0xbc, u(sub));
                seed_rand();
            }, st == 3 || st == 5 || st == 7 ? 40 : 50);

    // the deal of targets: some Gadgebots attacking (state 5, not striking) without a live target, the level's attackable
    // mobys (kind 5) about them, near and far, level or not, in sight or not, some taking two or three Gadgebots
    fcheck("class857_assign", copy_here(0x002f2250, 0x002d6288), 60, Ret::None, [] {
        gadgebot_base();
        std::vector<uint32_t> bots;
        for (const uint32_t p : level_list())
            if (ee_read<int16_t>(p + 0xa6) == 857 && pvar(p) != 0) bots.push_back(p);
        const std::vector<uint32_t> kind5 = targets_of_kind5();
        for (const uint32_t m : bots) {
            const uint32_t pv = pvar(m);
            w8(m + 0x20, ri(0, 4) == 0 ? u(ri(0, 9)) : 5u);
            w8(m + 0xbc, ri(0, 5) == 0 ? 2u : u(ri(0, 4)));
            const int t = ri(0, 3);
            const uint32_t tg = t == 0 || kind5.empty() ? 0u : pick(kind5);
            w32(pv + 0x174, tg);
            if (tg != 0 && t == 1) w8(tg + 0x20, coin() ? 0xfeu : 0xfdu);
            w16(pv + 0x198, u(ri(0, 1)));
            wf(m + 0x48, rf(-3.1f, 3.1f));
        }
        if (!bots.empty() && !kind5.empty()) {
            const int n = ri(1, 12);
            for (int k = 0; k < n; ++k) {
                const uint32_t c = pick(kind5), near = pick(bots);
                const float r = ri(0, 3) == 0 ? rf(4.5f, 14.0f) : rf(0.5f, 5.0f);
                ee_ref<Vec4>(c + 0x10) = around(ee_ref<Vec4>(near + 0x10), r, r, ri(0, 4) == 0 ? -2.0f : -0.5f, ri(0, 4) == 0 ? 2.0f : 0.5f);
                w8(c + 0x31, u(ri(0, 1)));
                if (ri(0, 3) == 0) {  // (one taking more Gadgebots: its pvars' first word's +4, the moby's flag 0x20)
                    w16(c + 0x34, ee_read<uint16_t>(c + 0x34) | 0x20u);
                    if (pvar(c) != 0) w32(pvar(c), TEST_AREA + 0x600 + u(k) * 0x10);
                    w16(TEST_AREA + 0x604 + u(k) * 0x10, u(ri(0, 3)));
                }
            }
        }
        if (coin()) wf(OV(0x00166dd8), rf(-3.1f, 3.1f));
        w32(OV(0x0015f5cc), R() & 0xffff);
        return Args{{}, {}};
    }, [](A) { class857_assign(); return uint64_t{0}; });

    // the arrival at a spot: a slot (945, 1301, 351), a pad (1024) or an entrance (1302), or another moby; at it or not
    fcheck("class857_arrive", copy_here(0x002f2c28, 0x002d6c60), 120, Ret::None, [] {
        uint32_t m = 0, o = 0;
        for (int k = 0; k < 20 && o == 0; ++k) {
            gadgebot_base();
            m = pick(gadgebots());
            const std::vector<uint32_t> sp = gadgebot_spots(m);
            o = ri(0, 6) == 0 || sp.empty() ? pick(live_mobys()) : pick(sp);
            if (pvar(o) == 0) o = 0;
        }
        spot_state(m, o);
        if (coin()) w32(pvar(m) + 0x310, u(ri(0, 3)));
        w32(TEST_AREA + 0x100, 0);
        seed_rand();
        return Args{{m, pvar(m), o, u(ri(0, 1)), TEST_AREA + 0x100}, {}};
    }, [](A a) { class857_arrive(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4]); return uint64_t{0}; });

    // the destination's pick: of its group or of the level's list, the spots about it near or far, level or not
    auto with_spots = [] {
        gadgebot_base();
        const uint32_t m = pick(gadgebots());
        const uint32_t pv = pvar(m);
        if (ri(0, 2) == 0) w32(pv + 0x30c, 0xffffffffu);
        const int n = ri(0, 6);
        for (int k = 0; k < n; ++k) {
            const std::vector<uint32_t> sp = gadgebot_spots(m);
            if (sp.empty()) break;
            const uint32_t o = pick(sp);
            const Vec4 at = ee_ref<Vec4>(m + 0x10);
            const float r = coin() ? rf(0.0f, 1.2f) : rf(0.0f, 14.0f);
            ee_ref<Vec4>(o + 0x10) = around(at, r, r, 0.0f, 0.0f);
            const int h = ri(0, 3);  // (level, about the 0.2 of the pick, or out of reach)
            if (h == 1) wf(o + 0x18, ee_read<float>(o + 0x18) + (coin() ? rf(0.1f, 0.25f) : -rf(0.1f, 0.25f)));
            else if (h == 2) wf(o + 0x18, ee_read<float>(o + 0x18) + (coin() ? rf(-0.3f, 0.3f) : rf(-2.0f, 2.0f)));
            w8(o + 0x20, ri(0, 2) == 0 ? 2u : u(ri(0, 4)));
            if (pvar(o) != 0) w32(pvar(o), ri(0, 3) == 0 ? 0xffffffffu : index_of(pick(live_mobys())));
            if (pvar(o) != 0) w32(pvar(o) + 4, u(ri(0, 5)));
        }
        return m;
    };
    fcheck("class857_pick_dest", copy_here(0x002f2910, 0x002d6948), 80, Ret::None, [with_spots] {
        const uint32_t m = with_spots();
        if (ri(0, 2) == 0) {  // (the nearest a slot of class 1301 or 351 of the level's list, its linked moby -1 or not)
            w32(pvar(m) + 0x30c, 0xffffffffu);
            std::vector<uint32_t> list;
            for (const uint32_t o : level_list())
                if (o != m && pvar(o) != 0 && ee_read<int16_t>(o + 0xa6) != 857) list.push_back(o);
            if (!list.empty()) {
                const uint32_t o = pick(list);
                w16(o + 0xa6, coin() ? 1301u : 351u);
                w32(pvar(o), coin() ? 0xffffffffu : index_of(pick(live_mobys())));
                ee_ref<Vec4>(o + 0x10) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 0.5f, 0.0f, 0.0f);
            }
        }
        return Args{{m}, {}};
    }, [](A a) { class857_pick_dest(a.i[0]); return uint64_t{0}; });
    fcheck("class857_find", copy_here(0x002f32f0, 0x002d7328), 80, Ret::Int32, [with_spots] {
        const uint32_t m = with_spots();
        const std::vector<uint32_t> sp = gadgebot_spots(m);
        if (!sp.empty() && ri(0, 2) != 0) spot_state(m, pick(sp));  // (at a spot's goal often)
        if (coin()) w32(pvar(m) + 0x310, u(ri(0, 3)));
        seed_rand();
        return Args{{m, u(ri(0, 1))}, {}};
    }, [](A a) { return ri32(class857_find(a.i[0], a.i[1])); });

    // the score of a target: shown or hidden, state 9 or not, turned toward it or not, the camera's yaw
    fcheck("class857_score", copy_here(0x002f2148, 0x002d6180), 60, Ret::Float, [] {
        gadgebot_base();
        const uint32_t m = pick(gadgebots()), o = pick(live_mobys());
        wf(m + 0x48, rf(-3.1f, 3.1f));
        wf(OV(0x00166dd8), rf(-3.1f, 3.1f));
        w8(o + 0x31, u(ri(0, 1)));
        if (coin()) ee_ref<Vec4>(o + 0x10) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 12.0f);
        return Args{{m, o, ri(0, 2) == 0 ? 9u : u(ri(0, 10))}, {}};
    }, [](A a) { return rf32(class857_score(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]))); });

    // the hit reaction: in state 9 or not, its animation 2 shown or set or not, hit (mask 0x330001) from the hero, a Gadgebot
    // or another moby, or not
    classcheck("class857_react", copy_here(0x002f3640, 0x002d7678), [] { gadgebot_base(); return gadgebots(); }, class857_react, [](uint32_t m) {
        gadgebot_base();
        w8(m + 0x20, ri(0, 3) == 0 ? 9u : u(ri(0, 8)));
        w8(m + 0x52, ri(0, 3) == 0 ? 2u : u(ri(0, 6)));
        w8(m + 0x53, ri(0, 3) == 0 ? 2u : u(ri(0, 6)));
        give_hit(m, 0x330001u);
        if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
            const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
            const int k = ri(0, 2);
            const std::vector<uint32_t> bots = gadgebots();
            w32(e + 0x20, k == 0 ? ee_read<uint32_t>(0x001413d0) : (k == 1 ? pick(bots) : pick(live_mobys())));
        }
        seed_rand();
    }, 80);

    // the glow: each state's colour, no group (+0x21 0xff), the frame counter 0 or not, the group's frame word this frame's
    classcheck("class857_glow", copy_here(0x002f38c8, 0x002d7900), [] { gadgebot_base(); return gadgebots(); }, class857_glow, [](uint32_t m) {
        gadgebot_base();
        const uint32_t pv = pvar(m);
        w8(m + 0x20, u(ri(0, 10)));
        if (ri(0, 5) == 0) w8(m + 0x21, 0xff);
        w32(m + 0x90, R());
        wf(pv + 0x28c, rf(-3.1f, 3.1f));
        const int32_t frame = ri(0, 4) == 0 ? 0 : static_cast<int32_t>(R() & 0xffff);
        w32(OV(0x0015f5cc), u(frame));
        const uint32_t word = ee_read<uint32_t>(pv + 0x294);
        if (word >= 0x00100000u && word < 0x02000000u) w32(word, coin() ? u(frame) : R());
    }, 60);

    // the lights spawned at a Gadgebot
    fcheck("class302_spawn", copy_here(0x002d8510, 0x002bd058), 20, Ret::Int32, [] {
        gadgebot_base();
        return Args{{pick(gadgebots())}, {}};
    }, [](A a) { return r32(class302_spawn(a.i[0])); });
    fcheck("class303_spawn", copy_here(0x002d8b98, 0x002bd6e0), 20, Ret::Int32, [] {
        gadgebot_base();
        return Args{{pick(gadgebots())}, {}};
    }, [](A a) { return r32(class303_spawn(a.i[0])); });
    g_ignored_words.resize(ign0);
}
}  // namespace

void batch_518() {
    if (g_only_batch != 518) return;
    g_batch = 518;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // 989snd's IOP server (as batches 120 / 506 / 509): replies a function of the command
    {
        auto result = [](uint32_t id, uint32_t a0) -> uint32_t {
            const uint32_t h = (id * 0x9e3779b1u) ^ (a0 * 0x85ebca6bu);
            return ((h >> 7) % 4 == 0) ? 0u : ((h >> 3) | 1u);
        };
        sif_rpc_register(0x123456, [result](uint32_t fno, uint32_t send, uint32_t, uint32_t recv, uint32_t) {
            if (fno == 0x4d) {
                const uint32_t n = ee_read<uint32_t>(send);
                uint32_t p = send + 4;
                for (uint32_t i = 0; i < n; ++i) {
                    const uint32_t id = ee_read<uint16_t>(p), size = ee_read<uint16_t>(p + 2);
                    ee_write<uint32_t>(recv + 4 + i * 4, result(id, size != 0 ? ee_read<uint32_t>(p + 4) : 0u));
                    if (id == 0x38) {
                        disc_read_ee(ee_read<uint32_t>(p + 0xc), ee_read<uint32_t>(p + 4), ee_read<uint32_t>(p + 8));
                        ee_write<uint32_t>(0x00137b10, 0);
                        ee_write<uint32_t>(0x00137b00, 0);
                    }
                    p += 4 + ((size + 3) & ~3u);
                }
                ee_write<uint32_t>(recv, 0xffffffffu);
                ee_write<uint32_t>(recv + 4 + n * 4, 0xffffffffu);
                return;
            }
            ee_write<uint32_t>(recv + 4, result(fno, send != 0 ? ee_read<uint32_t>(send) : 0u));
            ee_write<uint32_t>(recv, 0xffffffffu);
            ee_write<uint32_t>(recv + 8, 0xffffffffu);
        });
    }
    level();
    // (the kernel's calls the original would make for real: the CD / movie path of a level change request; batch 120's)
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80};
    // the port's VU0 micro memory as the game leaves it, and the interpreter's VU0 kept for the port (as batch 506)
    vu0::vif0_send_chain(0x00100080, true);
    vu0::vif0_send_chain(0x0010e4c0, true);
    g_pre_call = [](EeInterpreter& ee) {
        const VUState& o = ee.vu0();
        g_vu0_in.vf = o.vf;
        g_vu0_in.vi = o.vi;
        g_vu0_in.acc = o.acc;
        g_vu0_in.q = o.q;
        g_vu0_in.i = o.i;
        g_vu0_in.r = o.r;
        g_vu0_in.p = o.p;
        g_vu0_in.macflag = o.macflag;
        g_vu0_in.statusflag = o.statusflag;
        g_vu0_in.clipflag = o.clipflag;
        std::memcpy(g_vu0_in.mem.data(), o.mem.data(), 0x1000);
    };

    class1302_checks();
    class1068_checks();
    class857_checks();

    g_pre_call = nullptr;
    g_extra_noop_hooks.clear();
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
