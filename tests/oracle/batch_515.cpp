// Oracle batch 515 (stage 4 task F9, #149): class code several levels carry a copy of, each family ported once as shared
// hand-written code (src/game/ovl/shared_*.cpp), against every level's own copy at that level's own address. The level
// copies stay transliterated in their units until each level is re-glued; here every copy is the original and the one shared
// port the port:
//  - shared_class615.cpp: class 615, the Trespasser's pad (its update, the camera's goal, the hero pulled onto it) and its
//    puzzle's draw callback with the link, ring, marker and frame draws (levels 2, 4, 6, 8, 11, 13, 18);
//  - shared_class1844.cpp: class 1844, a breakable prop (Eudora's 1834, Batalia's 1844, Gaspar's 1849, Pokitaru's 1859,
//    Hoven's 1862; levels 4, 8, 9, 11, 12);
//  - aridia_props.cpp: the ocean's wave table with the alpha 0x80 (ocean_wave_table_set_80; levels 5, 7, 8);
//  - shared_class447.cpp: class 447 (Kerwan's and Rilgar's class 920), a creature that looks at the hero (levels 3, 5, 7, 13);
//  - shared_class1023.cpp: class 1023, a walker that patrols, leaps, fires and strikes, its every-frame part and its shot's
//    spawn (levels 8, 9);
//  - shared_class280.cpp: class 280, the pump the hero works with the Hydrodisplacer, Novalis's story flags of its pumps and
//    the scripted camera's angles (levels 1, 4, 8);
//  - shared_class631.cpp: class 631, a flying craft with a rider that drops class 848's charges, and its hits (levels 3, 7);
//@@HEADER
// Subjects: the level's own mobys of the class after the first gameplay frame (the port's; the screen, the projection and the
// view set up), perturbed through every state; the helpers on arguments built from the level's state; the draws' packets
// into a buffer of the test area (the packet cursor put there), the frame scale (0x15ed68) at its 50 Hz value in a quarter
// of the cases. No RAM dumps. No broad list of left-out words.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 515, N = each level a family names (1 .. 9, 11, 12, 13, 18); a family
// the level does not carry says so and is not run. B515_INFO=1 prints, per draw check, how many cases drew (the packet
// cursor moved) of the cases compared.

#include "ovl_oracle.h"

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/aridia_mobys.h>
#include <game/ovl/shared_class1023.h>
#include <game/ovl/shared_class1844.h>
#include <game/ovl/shared_class280.h>
#include <game/ovl/shared_class447.h>
#include <game/ovl/shared_class631.h>
#include <game/ovl/shared_class615.h>
#include <runtime/kernel/disc.h>
#include <runtime/vu/vu0.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <vector>

namespace rac1::oracle {

namespace {
// The VU0 the original started from (the interpreter's), given to the port before it runs (as batches 506 / 508)
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

bool info() { return std::getenv("B515_INFO") != nullptr; }

// Arguments the original takes as whole 64-bit registers (a TEX0): the case's values, put in the interpreter's registers by
// the batch's pre-call (check_fn passes 32-bit words, sign-extended); the port reads them from here (batch 504's)
std::vector<std::pair<int, uint64_t>> g_wide_regs;
uint64_t wide_arg(int reg) {
    for (const auto& [r, v] : g_wide_regs)
        if (r == reg) return v;
    return 0;
}

struct LvAddr {
    int level;
    uint32_t addr;
};

// the family's copy on the level being run (0: the level has none)
uint32_t copy_here(std::initializer_list<LvAddr> copies) {
    for (const LvAddr& c : copies)
        if (c.level == g_oracle_level) return c.addr;
    return 0;
}

bool selected(const char* name) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return false;
    const char* only = std::getenv("ORACLE_ONLY");
    return only == nullptr || std::strcmp(only, name) == 0;
}

// A check whose original is the level's own copy at `laddr` (the port given the original's VU0 first).
void fcheck(const char* name, uint32_t laddr, int n, Ret ret, const std::function<Args()>& gen,
            const std::function<uint64_t(const Args&)>& port) {
    if (!selected(name)) return;
    std::printf("%-26s L%d:0x%08x\n", name, g_oracle_level, laddr);
    const std::function<uint64_t(const Args&)> synced = [&port](const Args& a) { vu0_sync(); return port(a); };
    if (!check_fn(name, laddr, n, ret, gen, synced, Scope::FullRam, 0, 0)) ++g_fail;
}

void not_here(const char* name) {
    if (selected(name)) std::printf("%-26s not on level %d\n", name, g_oracle_level);
}

// the level after its first gameplay frame (the port's) and its view set up, as batch 506
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
        // the screen and projection constants, which the oracle's display (no size) leaves broken: a 640 x 448 screen, as the
        // game's (batch 501's); and the view's matrices (the draw callbacks project through them, as batch 401's)
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

// the level's live mobys of class `cls`
std::vector<uint32_t> mobys_of(uint32_t cls) {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < end_moby(); m += 0x100)
        if (live(m) && ee_read<uint16_t>(m + 0xa6) == cls) v.push_back(m);
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

// the frame scale (0x15ed68) at the 50 Hz value sometimes (frame_scale_i's counts then differ from the 60 Hz ones)
void vary_frame_scale() {
    if (ri(0, 3) == 0) wf(0x0015ed68, 1.2f);
}

// a run of the port on the generator's state that throws (a pointer outside RAM) asks for another state; the RAM restored
bool trial(const std::function<void()>& port) {
    static std::vector<uint8_t> saved;
    uint8_t* ram = ee_get_ram_base();
    saved.assign(ram, ram + 0x02000000);
    bool ok = true;
    const uint32_t stack_top = ee_stack_ptr();
    try { port(); } catch (const std::exception& e) { ok = false; if (std::getenv("B515_DBG")) std::printf("DBG trial: %s\n", e.what()); }
    ee_stack_ptr() = stack_top;
    std::memcpy(ram, saved.data(), saved.size());
    return ok;
}

// the frame's packet cursor (0x1611c0) on a buffer of the test area: after the first gameplay frame alone it is 0 (the
// drawing would write over the null page, which these checks leave out)
void packet_cursor() { w32(OV(0x001611c0), TEST_AREA + 0x8000 + u(ri(0, 0x40)) * 0x10); }

// A draw check's tally: B515_INFO=1 prints, per case, how far the port's packets grew, and at the end how many cases drew
struct DrawTally {
    const char* name;
    int cases = 0, drawn = 0;
    void add(const std::function<void()>& port) {
        if (!info()) return;
        static std::vector<uint8_t> saved;
        uint8_t* ram = ee_get_ram_base();
        saved.assign(ram, ram + 0x02000000);
        const uint32_t before = ee_read<uint32_t>(OV(0x001611c0));
        const uint32_t stack_top = ee_stack_ptr();
        try { port(); } catch (const std::exception&) {}
        ee_stack_ptr() = stack_top;
        const uint32_t grew = ee_read<uint32_t>(OV(0x001611c0)) - before;
        ++cases;
        if (grew != 0) ++drawn;
        std::memcpy(ram, saved.data(), saved.size());
    }
    void report() const {
        if (info() && cases != 0) std::printf("INFO %s: %d of %d cases drew\n", name, drawn, cases);
    }
};

// An update on one of the subjects (dealt again when the port's run throws, as batch 506's classcheck)
void classcheck(const char* name, uint32_t laddr, const std::function<std::vector<uint32_t>()>& subjects, void (*port)(uint32_t),
                const std::function<void(uint32_t)>& vary, int n) {
    if (!selected(name)) return;
    level();
    const std::vector<uint32_t> probe = subjects();
    if (probe.empty()) { std::printf("%-26s L%d:0x%08x  no subject on this level\n", name, g_oracle_level, laddr); return; }
    if (info()) std::printf("INFO %s: %zu subjects\n", name, probe.size());
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

// a point `r0`..`r1` from `at` (horizontally), `z0`..`z1` above it
Vec4 around(const Vec4& at, float r0, float r1, float z0 = -1.0f, float z1 = 2.0f) {
    const float r = rf(r0, r1), a = rf(-3.1f, 3.1f);
    return Vec4{at.x + r * std::cos(a), at.y + r * std::sin(a), at.z + rf(z0, z1), at.w};
}

// a hit on the moby: its slot in the hit table (0x178100, 64-byte entries) naming it, with a damage mask (the `kinds` bits
// mixed with random ones) and a damage at +0x2c (batch 506's)
void give_hit(uint32_t m, uint32_t kinds) {
    if (ri(0, 3) == 0) { w8(m + 0xa4, 0xff); return; }
    const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
    w8(m + 0xa4, idx);
    w32(e + 0x34, ri(0, 8) == 0 ? first_moby() : m);
    w32(e + 0x24, ri(0, 2) == 0 ? (R() & ~kinds) : (kinds | (R() & 0xff)));
    { static const float dm[] = {-1.0f, 0.0f, 0.5f, 5.0f, 12.0f}; wf(e + 0x2c, dm[ri(0, 4)]); }
    wfrand(e, 4, -1, 1);
    w32(e + 0x28, u(ri(0, 20)));
    w32(e + 0x20, ri(0, 3) == 0 ? 0u : (coin() ? ee_read<uint32_t>(0x001413d0) : first_moby() + 0x100 * u(ri(0, 40))));
}

// the level's mobys of class `cls`, or (none placed) `k` spawned at the hero
std::vector<uint32_t> placed_or_spawned(uint32_t cls, int k = 3) {
    std::vector<uint32_t> v = mobys_of(cls);
    if (!v.empty()) return v;
    for (int i = 0; i < k; ++i) {
        const uint32_t s = moby_spawn(cls);
        if (s == 0) break;
        ee_ref<Vec4>(s + 0x10) = ee_ref<Vec4>(0x0013f3d0);
        wf(s + 0x10, ee_read<float>(s + 0x10) + rf(-3, 3));
        wf(s + 0x14, ee_read<float>(s + 0x14) + rf(-3, 3));
        v.push_back(s);
    }
    return v;
}

// the level's path (an index of the path table 0x1b04b0) with 2 .. 200 points, or -1
int32_t a_path() {
    std::vector<int32_t> ok;
    for (uint32_t i = 0; i < 64; ++i) {
        const uint32_t p = ee_read<uint32_t>(OV(0x001b04b0) + i * 4);
        if (p >= 0x00100000u && p < 0x02000000u && ee_read<int32_t>(p) >= 2 && ee_read<int32_t>(p) <= 200) ok.push_back(static_cast<int32_t>(i));
    }
    return ok.empty() ? -1 : ok[static_cast<size_t>(ri(0, static_cast<int>(ok.size()) - 1))];
}
uint32_t path_at(int32_t i) { return ee_read<uint32_t>(OV(0x001b04b0) + u(i) * 4); }

// ---- class 615, the Trespasser's pad and its puzzle (shared_class615.cpp)
void class615_checks() {
    const uint32_t upd = copy_here({{2, 0x002d8ad0}, {4, 0x002d4ae0}, {6, 0x002e1940}, {8, 0x002f2030}, {11, 0x002fcda8}, {13, 0x002f5618}, {18, 0x002d8fd0}});
    const uint32_t goal = copy_here({{2, 0x002d8910}, {4, 0x002d4920}, {6, 0x002e1780}, {8, 0x002f1e70}, {11, 0x002fcbe8}, {13, 0x002f5458}, {18, 0x002d8e10}});
    const uint32_t to_pad = copy_here({{2, 0x002d92f0}, {4, 0x002d5300}, {6, 0x002e2160}, {8, 0x002f2850}, {11, 0x002fd5c8}, {13, 0x002f5e38}, {18, 0x002d97f0}});
    const uint32_t draw = copy_here({{2, 0x002d93e8}, {4, 0x002d53f8}, {6, 0x002e2258}, {8, 0x002f2948}, {11, 0x002fd6c0}, {13, 0x002f5f30}, {18, 0x002d98e8}});
    const uint32_t link = copy_here({{2, 0x002da938}, {4, 0x002d6948}, {6, 0x002e37a8}, {8, 0x002f3e98}, {11, 0x002fec10}, {13, 0x002f7480}, {18, 0x002dae38}});
    const uint32_t ring = copy_here({{2, 0x002da3d8}, {4, 0x002d63e8}, {6, 0x002e3248}, {8, 0x002f3938}, {11, 0x002fe6b0}, {13, 0x002f6f20}, {18, 0x002da8d8}});
    const uint32_t marker = copy_here({{2, 0x002da6c0}, {4, 0x002d66d0}, {6, 0x002e3530}, {8, 0x002f3c20}, {11, 0x002fe998}, {13, 0x002f7208}, {18, 0x002dabc0}});
    const uint32_t frame = copy_here({{2, 0x002dae88}, {4, 0x002d6e98}, {6, 0x002e3cf8}, {8, 0x002f43e8}, {11, 0x002ff160}, {13, 0x002f79d0}, {18, 0x002db388}});
    if (upd == 0) {
        for (const char* n : {"class615_update", "class615_camera_goal", "class615_hero_to_pad", "class615_puzzle_draw", "class615_link_draw",
                              "class615_ring_draw", "class615_marker_draw", "class615_frame_draw"})
            not_here(n);
        return;
    }
    auto pads = [] { return mobys_of(615); };
    // the puzzle's state: the rings' turns (sometimes outside 0..11), their slots (mostly links), the lights, the active ring
    // and its turn in progress
    auto puzzle = [](uint32_t pv) {
        for (uint32_t k = 0; k < 3; ++k) w32(pv + 0x14 + 4 * k, ri(0, 7) == 0 ? u(ri(-14, 26)) : u(ri(0, 11)));
        const bool sparse = ri(0, 2) == 0;  // (few slots set: the links that reach no ring, toward a light)
        for (uint32_t s = 0; s < 36; ++s) {
            const int k = ri(0, 19);
            if (sparse) w32(pv + 0x20 + 4 * s, k < 3 ? 2u : k < 18 ? 0u : u(ri(1, 3)));
            else w32(pv + 0x20 + 4 * s, k < 8 ? 2u : k < 13 ? 0u : k < 16 ? 1u : k < 18 ? 3u : u(ri(-2, 5)));
        }
        const int lit = ri(0, 3);  // (0: all lit or off, no unlit one)
        for (uint32_t s = 0; s < 12; ++s) w32(pv + 0xb0 + 4 * s, lit == 0 ? u(ri(0, 1) * 2) : u(ri(0, 2)));
        w32(pv + 0xe0, ri(0, 7) == 0 ? u(ri(0, 5)) : u(ri(1, 3)));
        const int t = ri(0, 5);
        wf(pv + 0xe4, t < 2 ? 0.0f : t == 2 ? rf(-0.32f, 0.32f) : t == 3 ? rf(0.45f, 0.6f) : rf(-0.6f, -0.45f));
    };
    // the frame's block: its quads' corners and its matrix (the pad's state 0 sets the matrix; perturbed)
    auto frame_block = [] {
        const uint32_t b = copy_here({{2, 0x001d3310}, {4, 0x001d2f70}, {6, 0x001d4a20}, {8, 0x001e9300}, {11, 0x001d36b0}, {13, 0x001d36e0}, {18, 0x001d36d0}});
        if (coin()) for (uint32_t k = 0; k < 0x80; k += 4) w32(b + k, R());
        if (coin()) wfrand(b + 0xa0, 16, -20.0f, 20.0f);
    };

    // the update, each state: the hero on the pad or not, with the Trespasser in hand or not, the camera's goal reached or not,
    // the puzzle solved or not, the stick and the buttons
    classcheck("class615_update", upd, pads, class615_update, [puzzle](uint32_t m) {
        const uint32_t pv = pvar(m);
        const uint32_t hero = 0x0013f350;
        vary_frame_scale();
        const int st = ri(0, 9);
        w8(m + 0x20, st < 1 ? 0u : st < 3 ? 1u : st < 7 ? 2u : st < 9 ? 3u : u(ri(4, 6)));
        w8(m + 0xb0, ri(0, 2) == 0 ? 0xffu : u(ri(0, 0x40)));
        w16(m + 0xb2, u(ri(0, 0x7ff)));
        if (coin()) {  // (the moby's saved flags: its bit, or its byte)
            const uint32_t idx = ee_read<uint16_t>(m + 0xb2);
            const uint32_t word = 0x0014c190 + ((idx >> 5) << 2) + (ee_read<uint32_t>(0x0015ed84) << 8);
            w32(word, ee_read<uint32_t>(word) ^ (1u << (idx & 0x1f)));
            if (coin()) w8(OV(0x001bb230) + idx + 0x454, u(ri(0, 1)));
        }
        w16(pv + 0x10, u(ri(0, 3) == 0 ? ri(0, 2) : ri(0, 300)));
        w16(pv + 0x12, u(ri(0, 1)));
        w32(pv + 0xec, u(ri(0, 0x40)));
        puzzle(pv);
        if (coin()) w32(pv + 0xe0, u(ri(1, 3)));
        if (ri(0, 4) == 0) {  // (solved: every light lit, no turn, the timer at its end)
            for (uint32_t k = 0; k < 12; ++k) w32(pv + 0xb0 + 4 * k, 2);
            wf(pv + 0xe4, 0.0f);
            w16(pv + 0x10, u(ri(0, 1)));
        }
        // the hero on the pad (his ground moby), in the air or not, the Trespasser in hand or not, the item's moby
        w32(hero + 0x2fc, ri(0, 4) == 0 ? first_moby() : m);
        w16(hero + 0x30e, ri(0, 4) == 0 ? u(ri(1, 3)) : 0u);
        w32(hero + 0x10b8, ri(0, 3) == 0 ? u(ri(0, 0x30)) : 0x1au);
        if (ri(0, 4) == 0) {
            w32(hero + 0x1090, 0);
        } else {
            const uint32_t item = pick(live_mobys());
            w32(hero + 0x1090, item);
            w8(item + 0xbc, ri(0, 2) == 0 ? 0u : u(ri(1, 3)));
        }
        w32(0x001413d4, ri(0, 3) == 0 ? u(ri(0, 0x80)) : 0x72u);
        w32(0x0013fde8, R());
        // the buttons pressed (triangle 0x10, up 0x1000, down 0x4000), the sticks' bits, the save state's words
        const int b = ri(0, 5);
        w32(0x0013cae4, b == 0 ? 0u : b == 1 ? 0x1000u : b == 2 ? 0x4000u : b == 3 ? 0x10u : (R() & 0x5010u));
        w32(0x0013cb00, R());
        w32(0x0013cb04, ri(0, 2) == 0 ? 0u : (R() & 0xa000u));
        if (coin()) w32(0x0013cb00, 0);
        // (the inputs while played: a turn near its end, the stick alone, a button alone, or as dealt)
        switch (ri(0, 3)) {
            case 0: wf(pv + 0xe4, coin() ? rf(0.3f, 0.6f) : rf(-0.6f, -0.3f)); break;
            case 1: wf(pv + 0xe4, 0.0f); w32(0x0013cb04, ri(0, 2) == 0 ? 0xa000u : (coin() ? 0x2000u : 0x8000u)); break;
            case 2: {
                wf(pv + 0xe4, 0.0f);
                w32(0x0013cb04, 0);
                const int k = ri(0, 3);
                w32(0x0013cae4, k == 0 ? 0x1000u : k == 1 ? 0x4000u : k == 2 ? 0x10u : 0u);
                break;
            }
            default: break;
        }
        w32(OV(0x00179410), ri(0, 3) == 0 ? 1u : 0u);
        w32(OV(0x00179434), ri(0, 3) == 0 ? u(ri(0, 3)) : 0xffffffffu);
        // the camera's goal: the pad's camera record, or none (-1); the blend done or not; the camera at the goal's yaw or not
        const uint32_t recs = ee_read<uint32_t>(OV(0x001600ec));
        const bool playing = ri(0, 2) != 0;  // (mostly the camera at its goal: the puzzle played)
        const int32_t rec = recs != 0 && (playing || coin()) ? ri(0, 3) : -1;
        w32(pv + 0xe8, u(rec));
        wf(pv + 0x110, !playing && ri(0, 2) == 0 ? rf(0.0f, 1.0f) : 1.0f);
        wf(pv + 0x114, !playing && ri(0, 2) == 0 ? rf(-0.2f, 0.2f) : 0.0f);
        wfrand(pv + 0xf0, 8, -3.0f, 3.0f);
        const uint32_t cam = OV(0x00166c80);
        if (playing || ri(0, 2) != 0) {
            float yaw;
            if (rec >= 0) {
                yaw = ee_read<float>(recs + u(rec) * 0x80 + 0x78);
            } else {  // (the side of the pad nearer the camera's goal, looking back)
                const float m48 = ee_read<float>(m + 0x48);
                const float to = std::atan2(ee_read<float>(OV(0x00166dc4)) - ee_read<float>(m + 0x14), ee_read<float>(OV(0x00166dc0)) - ee_read<float>(m + 0x10));
                auto dist = [](float a, float b) { float d = std::fmod(std::fabs(a - b), 6.2831853f); return d > 3.1415927f ? 6.2831853f - d : d; };
                const float side = dist(to, m48 + 1.5707963f) < dist(to, m48 - 1.5707963f) ? m48 + 1.5707963f : m48 - 1.5707963f;
                yaw = std::remainder(side + 3.1415927f, 6.2831853f);
            }
            wf(cam + 0x158, std::remainder(yaw + (playing ? rf(-0.008f, 0.008f) : rf(-0.03f, 0.03f)), 6.2831853f));
        }
        if (const uint32_t c = ee_read<uint32_t>(OV(0x00166e00)); c != 0 && coin()) w16(c + 0x86, ri(0, 1) == 0 ? 5u : u(ri(0, 8)));
        // the hero near the pad
        if (coin()) ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 3.0f, 0.0f, 1.0f);
        wf(hero + 0x98, rf(-3.1f, 3.1f));
    }, 300);

    // the camera's goal: the pad's record or none, the camera's goal around the pad
    fcheck("class615_camera_goal", goal, 80, Ret::None, [pads] {
        level();
        const uint32_t m = pick(pads());
        const uint32_t recs = ee_read<uint32_t>(OV(0x001600ec));
        w32(pvar(m) + 0xe8, recs != 0 && coin() ? u(ri(0, 3)) : 0xffffffffu);
        ee_ref<Vec4>(OV(0x00166dc0)) = around(ee_ref<Vec4>(m + 0x10), 0.5f, 20.0f);
        wf(m + 0x48, rf(-3.1f, 3.1f));
        if (coin()) ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 3.0f);
        wrand(TEST_AREA + 0x100, 8);
        return Args{{TEST_AREA + 0x100, TEST_AREA + 0x110, m}, {}};
    }, [](A a) { class615_camera_goal(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });

    // the hero pulled onto the pad and turned (the turn within a frame's worth or clamped either way)
    fcheck("class615_hero_to_pad", to_pad, 80, Ret::None, [pads] {
        level();
        vary_frame_scale();
        const uint32_t m = pick(pads());
        ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 0.1f : 4.0f, -0.5f, 1.0f);
        wf(m + 0x48, rf(-3.1f, 3.1f));
        wf(0x0013f3e8, coin() ? std::remainder(ee_read<float>(m + 0x48) + 3.1415927f + rf(-0.15f, 0.15f), 6.2831853f) : rf(-3.1f, 3.1f));
        if (coin()) wf(0x0015ed6c, rf(0.005f, 0.05f));
        return Args{{m}, {}};
    }, [](A a) { class615_hero_to_pad(a.i[0]); return uint64_t{0}; });

    // the puzzle's draw on the pad's state perturbed, the frame counter anywhere
    DrawTally t_draw{"class615_puzzle_draw"};
    fcheck("class615_puzzle_draw", draw, 80, Ret::None, [pads, puzzle, frame_block, &t_draw] {
        level();
        const uint32_t m = pick(pads());
        puzzle(pvar(m));
        frame_block();
        w32(OV(0x0015f5cc), ri(0, 3) == 0 ? R() : u(ri(0, 5000)));
        packet_cursor();
        t_draw.add([m] { class615_puzzle_draw(m); });
        return Args{{m}, {}};
    }, [](A a) { class615_puzzle_draw(a.i[0]); return uint64_t{0}; });
    t_draw.report();

    // the helpers on their own: a link (the colour; the angle; the radii, across or not), a ring's segment (the TEX0 a whole
    // register), a marker of each kind (and others), the frame
    DrawTally t_link{"class615_link_draw"}, t_ring{"class615_ring_draw"}, t_marker{"class615_marker_draw"}, t_frame{"class615_frame_draw"};
    fcheck("class615_link_draw", link, 60, Ret::None, [&t_link] {
        level();
        w32(OV(0x0015f5cc), ri(0, 3) == 0 ? R() : u(ri(0, 5000)));
        packet_cursor();
        const uint32_t c = R();
        const float a = rf(-3.2f, 3.2f), r0 = rf(-20.0f, 60.0f), r1 = rf(-60.0f, 60.0f);
        t_link.add([c, a, r0, r1] { class615_link_draw(c, a, r0, r1); });
        return Args{{R(), c}, {a, r0, r1}};
    }, [](A a) { class615_link_draw(a.i[1], a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
    t_link.report();
    fcheck("class615_ring_draw", ring, 60, Ret::None, [&t_ring] {
        level();
        packet_cursor();
        const uint64_t tex0 = (static_cast<uint64_t>(R()) << 32) | R();
        g_wide_regs = {{4, tex0}};
        const uint32_t c = R();
        const float a0 = rf(-3.2f, 3.2f), a1 = coin() ? a0 + rf(0.1f, 0.6f) : rf(-3.2f, 3.2f), w0 = rf(0.0f, 60.0f), w1 = rf(0.0f, 60.0f);
        t_ring.add([tex0, c, a0, a1, w0, w1] { class615_ring_draw(tex0, c, a0, a1, w0, w1); });
        return Args{{static_cast<uint32_t>(tex0), c}, {a0, a1, w0, w1}};
    }, [](A a) { class615_ring_draw(wide_arg(4), a.i[1], a.f[0], a.f[1], a.f[2], a.f[3]); return uint64_t{0}; });
    t_ring.report();
    g_wide_regs.clear();
    fcheck("class615_marker_draw", marker, 60, Ret::None, [frame_block, &t_marker] {
        g_wide_regs.clear();
        level();
        frame_block();
        packet_cursor();
        const uint32_t kind = ri(0, 5) == 0 ? u(ri(-2, 6)) : u(ri(0, 2));
        const float a = rf(-3.2f, 3.2f), r = rf(-10.0f, 60.0f);
        t_marker.add([kind, a, r] { class615_marker_draw(kind, a, r); });
        return Args{{R(), R(), kind}, {a, r}};
    }, [](A a) { class615_marker_draw(a.i[2], a.f[0], a.f[1]); return uint64_t{0}; });
    t_marker.report();
    fcheck("class615_frame_draw", frame, 30, Ret::None, [frame_block, &t_frame] {
        level();
        frame_block();
        packet_cursor();
        t_frame.add([] { class615_frame_draw(); });
        return Args{{R(), R(), R(), R()}, {}};
    }, [](A) { class615_frame_draw(); return uint64_t{0}; });
    t_frame.report();
}

// ---- class 1844, a breakable prop (shared_class1844.cpp)
void class1844_checks() {
    const uint32_t upd = copy_here({{4, 0x002e79a8}, {8, 0x0030bc00}, {9, 0x0030a678}, {11, 0x0031d7d8}, {12, 0x0030b6c8}});
    const uint32_t cls = copy_here({{4, 1834}, {8, 1844}, {9, 1849}, {11, 1859}, {12, 1862}});
    if (upd == 0) { not_here("class1844_update"); return; }
    // (the debris: debris_spawn's pieces keep two w lanes of a VU0 sin / cos in the original's pvars (+0xc / +0x1c of each 0x80
    // from *0x15ffe0) that the native debris_spawn does not make, as batches 109 / 121 / 510 leave out; for this check only)
    const size_t ign = g_ignored_words.size();
    level();
    for (uint32_t a = end_moby(); a < end_moby() + 0x80000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
    classcheck("class1844_update", upd, [cls] { return placed_or_spawned(cls); }, class1844_update, [](uint32_t m) {
        vary_frame_scale();
        w8(m + 0x20, coin() ? 1u : u(ri(0, 3)));
        give_hit(m, 0x10000);
        if (ri(0, 2) == 0) wf(OV(0x00178100) + (u(ee_read<uint8_t>(m + 0xa4) & 0xf) << 6) + 0x2c, 0.0f);  // (a hit of no damage)
        if (coin()) ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 6.5f);  // (the hero near: the bolts drop)
        seed_rand();
    }, 80);
    g_ignored_words.resize(ign);
}

// ---- the ocean's wave table with the alpha 0x80 (aridia_props.cpp)
void ocean_checks() {
    const uint32_t f = copy_here({{5, 0x002ce110}, {7, 0x002cc680}, {8, 0x002acce8}});
    if (f == 0) { not_here("ocean_wave_table_set_80"); return; }
    fcheck("ocean_wave_table_set_80", f, 30, Ret::None, [] {
        level();
        wrand(TEST_AREA + 0x100, 0x14);
        return Args{{TEST_AREA + 0x100, R()}, {coin() ? rf(-2.0f, 2.0f) : rf(0.1f, 0.3f)}};
    }, [](A a) { ocean_wave_table_set_80(a.i[0], a.f[0]); return uint64_t{0}; });
}

// ---- class 447 / 920 (shared_class447.cpp)
void class447_checks() {
    const uint32_t upd = copy_here({{3, 0x002dc478}, {5, 0x00317aa0}, {7, 0x002f61c0}, {13, 0x002edbe8}});
    const uint32_t cls = g_oracle_level == 3 || g_oracle_level == 5 ? 920u : 447u;
    if (upd == 0) { not_here("class447_update"); return; }
    classcheck("class447_update", upd, [cls] { return placed_or_spawned(cls); }, class447_update, [](uint32_t m) {
        const uint32_t pv = pvar(m);
        vary_frame_scale();
        seed_rand();
        w8(m + 0x20, u(ri(0, 4) == 0 ? ri(0, 6) : ri(0, 3)));
        w8(m + 0x31, u(ri(0, 1)));
        w8(m + 0x53, ri(0, 3) == 0 ? u(ri(1, 20)) : 0u);
        if (coin()) ee_ref<Vec4>(OV(0x00166dc0)) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 25.0f : 40.0f);
        // its zone (the hero in it or not) and the mobys it waits on / sets (by index, in their start state or not)
        const uint32_t boxes = ee_read<uint32_t>(OV(0x001600ec));
        const int32_t zone = boxes != 0 && ri(0, 3) != 0 ? ri(0, 4) : -1;
        w32(pv + 0x120, u(zone));
        if (zone >= 0 && coin()) {
            Vec4 c = ee_ref<Vec4>(boxes + u(zone) * 0x80 + 0x30);
            c.x += rf(-0.3f, 0.3f); c.y += rf(-0.3f, 0.3f); c.z += rf(-0.3f, 0.3f);
            ee_ref<Vec4>(0x0013f3d0) = c;
        }
        const std::vector<uint32_t> all = live_mobys();
        auto index_of = [](uint32_t mo) { return (mo - first_moby()) >> 8; };
        const uint32_t wait = pick(all), other = pick(all);
        w32(pv + 0x124, ri(0, 3) == 0 ? 0xffffffffu : index_of(wait));
        w8(wait + 0x20, ri(0, 1) == 0 ? 0u : u(ri(1, 3)));
        w32(pv + 0x128, ri(0, 3) == 0 ? 0xffffffffu : index_of(other));
        // the hero near and in front of it or not, moving or not; the timers at their ends or running
        if (coin()) {
            const float a = ee_read<float>(m + 0x48) + (coin() ? rf(-1.2f, 1.2f) : rf(-3.1f, 3.1f));
            const float r = coin() ? rf(1.0f, 7.5f) : rf(7.0f, 12.0f);
            Vec4 h = ee_ref<Vec4>(m + 0x10);
            h.x += r * std::sin(a); h.y += r * std::cos(a); h.z += rf(-1.0f, 2.0f);
            ee_ref<Vec4>(0x0013f3d0) = h;
            Vec4 head = h;
            head.z += 1.0f;
            ee_ref<Vec4>(0x0013f420) = head;
        }
        if (coin()) wfrand(0x0013f450, 3, -0.004f, 0.004f);
        else wfrand(0x0013f450, 3, -5.0f, 5.0f);
        w32(pv + 0x118, ri(0, 2) == 0 ? 0u : u(ri(0, 3) == 0 ? ri(1, 2) : ri(1, 200)));
        w32(pv + 0x11c, ri(0, 2) == 0 ? u(ri(0, 1)) : u(ri(1, 300)));
        if (coin()) ee_ref<Vec4>(pv + 0x100) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 8.0f, -3.0f, 5.0f);
        w8(0x0015edb0, u(ri(0, 1)));
    }, 160);
}

// ---- class 1023 (shared_class1023.cpp)
void class1023_checks() {
    const uint32_t upd = copy_here({{8, 0x00301158}, {9, 0x003009d0}});
    const uint32_t frm = copy_here({{8, 0x003024d8}, {9, 0x00301d50}});
    const uint32_t shot = copy_here({{8, 0x00307190}, {9, 0x00308e00}});
    if (upd == 0) {
        for (const char* n : {"class1023_update", "class1023_frame", "class1023_shot_spawn"}) not_here(n);
        return;
    }
    const uint32_t tun = g_oracle_level == 9 ? 0x00161bd0u : 0x00162180u;  // (the class's numbers)
    // (the debris of its death: debris_spawn's pieces keep two w lanes of a VU0 sin / cos in the original's pvars, as batch
    // 510 leaves out; for these checks only)
    const size_t ign = g_ignored_words.size();
    level();
    for (uint32_t a = end_moby(); a < end_moby() + 0x80000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
    auto walkers = [] { return placed_or_spawned(1023); };
    // the walker's words dealt: its paths and point, the timers at their ends or running, the target near or far, its
    // animation, the hero's spin, the difficulty, a hit
    auto vary = [tun](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        vary_frame_scale();
        seed_rand();
        w8(m + 0x20, u(st < 0 ? 0 : st));
        w8(m + 0x31, ri(0, 3) == 0 ? 0u : 1u);
        if (ri(0, 4) == 0) ee_ref<Vec4>(OV(0x00166dc0)) = around(ee_ref<Vec4>(m + 0x10), 20.0f, 50.0f);
        else ee_ref<Vec4>(OV(0x00166dc0)) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 25.0f);
        w32(OV(0x0015f5c4), ri(0, 9) == 0 ? 2u : u(ri(0, 1)));
        const int32_t home = a_path(), patrol = ri(0, 3) == 0 ? -1 : a_path();
        const int32_t poly = a_path();
        w32(pv + 0x1d0, ri(0, 15) == 0 ? 0xffffffffu : u(poly));
        w32(pv + 0x1d4, ri(0, 9) == 0 ? 0xffffffffu : u(home));
        w32(pv + 0x1fc, u(patrol));
        const int32_t n = patrol >= 0 ? ee_read<int32_t>(path_at(patrol)) : 4;
        const int k = ri(0, 4);
        w32(pv + 0x1f4, k == 0 ? 0u : k == 1 ? u(n - 1) : k == 2 ? u(ri(-1, n + 1)) : u(ri(0, 1)));
        w16(pv + 0x20c, coin() ? 1u : 0xffffu);
        if (patrol >= 0 && coin()) {  // (at its patrol point, or the home path's)
            const int32_t at = std::clamp(ee_read<int32_t>(pv + 0x1f4), 0, n - 1);
            ee_ref<Vec4>(m + 0x10) = around(ee_ref<Vec4>(path_at(patrol) + 0x10 + u(at) * 0x10), 0.0f, 1.5f, -0.2f, 0.2f);
        } else if (home >= 0 && coin()) {
            const int32_t at = ri(0, 1);
            ee_ref<Vec4>(m + 0x10) = around(ee_ref<Vec4>(path_at(home) + 0x10 + u(at) * 0x10), 0.3f, 1.2f, -0.2f, 0.2f);
        }
        wf(pv + 0xe0, coin() ? 0.0f : rf(0.0f, 3.0f));
        w32(pv + 0x1f0, coin() ? u(ri(0, 1)) : u(ri(2, 90)));
        w32(pv + 0x1ec, coin() ? u(ri(0, 1)) : u(ri(2, 60)));
        wf(pv + 0x1e8, ri(0, 3) == 0 ? rf(19.0f, 25.0f) : rf(0.5f, 20.0f));
        w16(pv + 0x20e, coin() ? 4u : u(ri(0, 5)));
        w16(pv + 0x1da, u(ri(0, ee_read<int32_t>(tun + 0x4c) + 1)));
        w32(pv + 0x164, coin() ? 2u : 0u);
        w8(pv + 0x2e, ri(0, 7) == 0 ? 2u : 1u);
        w32(pv + 0x38, ri(0, 4) == 0 ? 1u : 0u);
        w16(pv + 0x1d8, ri(0, 2) == 0 ? 0u : u(ri(1, 100)));
        if (coin()) w32(pv + 0x1f8, 0);
        else if (coin()) w32(pv + 0x1f8, moby_spawn(0x401));  // (its glow moby there: placed by its joint)
        // the target: the hero, another moby, or none (the frame part may pick it again); near (a strike), in sight or far
        const std::vector<uint32_t> all = live_mobys();
        const uint32_t hero_m = ee_read<uint32_t>(0x001413d0);
        const int t = ri(0, 4);
        const uint32_t target = t == 0 ? 0u : t == 1 ? pick(all) : hero_m;
        w32(pv + 0x160, target);
        if (target != 0) {
            const float strike = ee_read<float>(tun + 0x48);
            const float r = coin() ? rf(0.0f, strike) : rf(strike, 30.0f);
            ee_ref<Vec4>(target + 0x10) = around(ee_ref<Vec4>(m + 0x10), r, r, -0.5f, 1.0f);
            if (target == hero_m) ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(target + 0x10);
        }
        if (coin()) w32(OV(0x00173e58), target);
        if (poly >= 0 && ri(0, 2) == 0) {  // (the hero inside the patrol path's polygon: the target picked)
            const uint32_t pp = path_at(poly);
            const int32_t np = ee_read<int32_t>(pp);
            Vec4 c{};
            for (int32_t i = 0; i < np; ++i) { c.x += ee_read<float>(pp + 0x10 + u(i) * 0x10) / np; c.y += ee_read<float>(pp + 0x14 + u(i) * 0x10) / np; c.z += ee_read<float>(pp + 0x18 + u(i) * 0x10) / np; }
            c.z += rf(-3.5f, 3.5f);
            ee_ref<Vec4>(0x0013f3d0) = c;
            ee_ref<Vec4>(hero_m + 0x10) = c;
            ee_ref<Vec4>(m + 0x10) = around(c, 0.0f, 15.0f, -1.0f, 1.0f);
            if (coin()) wf(m + 0x18, c.z + (coin() ? 1.0f : -1.0f) * rf(2.3f, 3.2f));  // (the height's limit, 3)
        }
        if (st == 2 && home >= 0 && coin()) {  // (back home: near its point, reached or not)
            ee_ref<Vec4>(m + 0x10) = around(ee_ref<Vec4>(path_at(home) + 0x10 + u(ri(0, 1)) * 0x10), 0.4f, 1.1f, -0.2f, 0.2f);
            wf(pv + 0xe0, 0.0f);
        }
        // its animation (set, the next one, the frame), its animation flags
        static const uint8_t sets[] = {0, 1, 2, 6, 0xa, 0xd, 0xe, 0x10, 0x12};
        w8(m + 0x53, sets[ri(0, 8)]);
        w8(m + 0x52, coin() ? ee_read<uint8_t>(m + 0x53) : sets[ri(0, 8)]);
        wf(m + 0x54, ri(0, 3) == 0 ? 0.0f : rf(0.0f, 30.0f));
        if (ri(0, 2) != 0) {  // (the animation's time in its windows 12..23, 16.5..20 or not: the frame record's time, or the blend)
            const int k2 = ri(0, 3);
            const float t = k2 == 0 ? rf(0.0f, 12.0f) : k2 == 1 ? rf(12.0f, 23.0f) : k2 == 2 ? rf(16.5f, 20.0f) : rf(23.0f, 30.0f);
            const uint32_t cur = ee_read<uint8_t>(m + 0x52) == 0xff ? ee_read<uint32_t>(m + 0x6c) : ee_read<uint32_t>(m + 0x68);
            if (cur >= 0x00100000u && cur < 0x01f00000u) {
                if (coin()) {
                    wf(m + 0x54, 0.0f);
                    w16(cur + 4, u(static_cast<int32_t>(t * 16.0f)));
                } else {
                    w8(m + 0x52, ee_read<uint8_t>(m + 0x53));
                    w8(m + 0x50, 1);
                    w8(m + 0x51, 0);
                    const float base = static_cast<float>(ee_read<int16_t>(ee_read<uint32_t>(m + 0x68) + 4)) * 0.0625f;
                    wf(m + 0x54, t - base == 0.0f ? 0.01f : t - base);
                }
            }
        }
        w8(m + 0x70, u(ri(0, 3)));
        // the hero spinning near; the difficulty
        w32(0x001413d4, coin() ? 0x3du : u(ri(0, 0x40)));
        if (coin()) ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 24.0f);
        w32(0x0013f4e8, u(ri(0, 60)));
        wf(OV(0x0015f5d4), rf(0.5f, 1.2f));
        wf(OV(0x0015f5d0), rf(0.5f, 1.2f));
        // a hit (in the update's checks a third of the cases, so that the states run; always in the frame part's)
        if (st < 0 || st >= 10 || ri(0, 2) == 0) give_hit(m, 0x330000);
        else w8(m + 0xa4, 0xff);
        if (ee_read<uint8_t>(m + 0xa4) != 0xff) {  // (the hit's kind and strength: each reaction; a source of class 71 or not)
            const uint32_t e = OV(0x00178100) + (u(ee_read<uint8_t>(m + 0xa4)) << 6);
            w8(e + 0x28, u(ri(0, 6)));
            w8(e + 0x29, u(ri(0, 4)));
            w16(e + 0x2a, u(ri(0, 3)));
            const uint32_t src = ee_read<uint32_t>(e + 0x20);
            if (src != 0 && ri(0, 3) == 0) w16(src + 0xa6, 0x47);
        }
        if (coin()) wf(pv + 0x20, ri(0, 2) == 0 ? rf(-1.0f, 0.5f) : rf(0.5f, 3.0f));
        // the knock-backs' landing (states 10, 11): moving, on the floor or not
        if (st >= 10 || st < 0) {
            wfrand(pv + 0x70, 8, -0.3f, 0.3f);
            if (coin()) wf(m + 0x18, ee_read<float>(m + 0x18) + rf(0.0f, 2.0f));
        }
    };
    for (int st = 0; st < 12; ++st) {
        classcheck("class1023_update", upd, walkers, class1023_update, [vary, st](uint32_t m) { vary(m, st); }, 40);
    }
    classcheck("class1023_frame", frm, walkers, class1023_frame, [vary](uint32_t m) { vary(m, -1); w8(m + 0x20, u(ri(0, 11))); }, 120);
    g_ignored_words.resize(ign);
    // the shot: a velocity, at a point near the walker, its owner and target, its life and index
    fcheck("class1023_shot_spawn", shot, 40, Ret::Int32, [walkers] {
        level();
        const uint32_t m = pick(walkers());
        wfrand(TEST_AREA + 0x100, 4, -0.5f, 0.5f);
        ee_ref<Vec4>(TEST_AREA + 0x110) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 2.0f);
        return Args{{TEST_AREA + 0x100, TEST_AREA + 0x110, m, ee_read<uint32_t>(0x001413d0), u(ri(0, 300)), u(ri(-2, 6))}, {}};
    }, [](A a) { return uint64_t{class1023_shot_spawn(a.i[0], a.i[1], a.i[2], a.i[3], static_cast<int32_t>(a.i[4]), a.i[5])}; });
}

// ---- class 280 (shared_class280.cpp)
void class280_checks() {
    const uint32_t upd = copy_here({{1, 0x002e0c68}, {4, 0x002bfd30}, {8, 0x002d8270}});
    const uint32_t flg = copy_here({{1, 0x002e0bd8}, {4, 0x002bfca0}, {8, 0x002d81e0}});
    const uint32_t cam = copy_here({{1, 0x00316e88}, {4, 0x002ee798}, {8, 0x00313880}});
    if (upd == 0) {
        for (const char* n : {"class280_update", "class280_level_flags", "camera5_angles_set"}) not_here(n);
        return;
    }
    const uint32_t hero = 0x0013f350, pad = 0x0013c940;
    auto pumps = [] { return placed_or_spawned(280); };
    // the pump's words, the hero at it (facing it, at its height, the Hydrodisplacer in hand, the button's history), his
    // stick and animations while he works it, its flags and checkpoint
    auto vary = [hero, pad](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        vary_frame_scale();
        w8(m + 0x20, u(st));
        w32(0x001413d4, coin() ? 0x3bu : u(ri(0, 0x40)));
        if (ri(0, 5) == 0) w32(OV(0x00166e00), 0);
        w8(m + 0xb0, ri(0, 2) == 0 ? 0xffu : u(ri(0, 0x3f)));
        w16(m + 0xb2, coin() && g_oracle_level == 1 ? u(ri(0x34, 0x35)) : u(ri(0, 0x7ff)));
        if (ri(0, 3) == 0) {
            const uint32_t idx = ee_read<uint16_t>(m + 0xb2);
            const uint32_t word = 0x0014c190 + ((idx >> 5) << 2) + (ee_read<uint32_t>(0x0015ed84) << 8);
            w32(word, ee_read<uint32_t>(word) ^ (1u << (idx & 0x1f)));
            if (coin()) w8(OV(0x001bb230) + idx + 0x454, u(ri(0, 1)));
        }
        w8(0x0014c050 + ee_read<uint8_t>(m + 0xb0) + (ee_read<uint32_t>(0x0015ed84) << 4), coin() ? 0xffu : u(ri(0, 3)));
        // its words
        const int k = ri(0, 3);
        wf(pv, k == 0 ? 0.0f : k == 1 ? 1.0f : rf(0.0f, 1.0f));
        w32(pv + 0x14, u(ri(0, 60)));
        wf(pv + 0x18, rf(0.0f, 20.0f));
        wf(pv + 0x1c, coin() ? 0.0f : rf(0.0f, 0.1f));
        w32(pv + 0x20, u(ri(0, 3)));
        wf(pv + 0x24, rf(-0.5f, 0.5f));
        wfrand(pv + 0x28, 2, -3.0f, 3.0f);
        wf(pv + 0x30, rf(0.5f, 4.0f));
        w32(pv + 0x34, coin() ? 0xffffffffu : u(ri(0, 3)));
        w32(pv + 0x38, u(ri(0, 1)));
        wf(pv + 0x3c, ri(0, 3) == 0 ? ee_read<float>(m + 0x18) + rf(-1.0f, 1.0f) : ee_read<float>(m + 0x18));
        if (coin()) wf(m + 0x18, ee_read<float>(pv + 0x3c) + rf(-0.6f, 0.3f));
        // the hero at it or not
        Vec4 h = around(ee_ref<Vec4>(m + 0x10), coin() ? 0.0f : 1.0f, coin() ? 1.7f : 3.0f, 0.0f, 0.0f);
        h.z = ee_read<float>(m + 0x18) + 0.3f + (coin() ? rf(-0.15f, 0.15f) : rf(-1.0f, 1.0f));
        ee_ref<Vec4>(0x0013f3d0) = h;
        const float to = std::atan2(ee_read<float>(m + 0x10) - h.x, ee_read<float>(m + 0x14) - h.y);
        wf(hero + 0x98, std::remainder(to + (coin() ? rf(-1.4f, 1.4f) : rf(-3.1f, 3.1f)), 6.2831853f));
        w8(hero + 0x20a4, ri(0, 4) == 0 ? 1u : 0u);
        w32(hero + 0x10b8, ri(0, 4) == 0 ? u(ri(0, 0x20)) : 8u);
        w32(hero + 0x1090, ri(0, 4) == 0 ? 0u : ee_read<uint32_t>(0x001413d0));
        w32(pad + 0x190, u(ri(0, 30)));
        w16(pad + 0x18e, u(ri(0, 29)));
        const bool pressed = coin();
        for (uint32_t i = 0; i < 30; ++i) w32(pad + 0x1e0 + 4 * i, pressed && ri(0, 4) == 0 ? (R() | 0x80u) : (R() & ~0x80u));
        w32(hero + 0x198, u(ri(0, 120)));
        // working it: the stick, the hero's animations (his moby's +0x52 / +0x53), the button flags, the cutscene side
        wfrand(hero + 0x1d20, 2, -1.5f, 1.5f);
        w32(hero + 0xa98, R() & 3);
        w32(hero + 0xa9c, ri(0, 3) == 0 ? 1u : 0u);
        static const uint8_t anims[] = {0x3f, 0x40, 0x41, 0x44, 0x10};
        const uint32_t hm = ee_read<uint32_t>(hero + 0x2080);
        if (hm != 0) { w8(hm + 0x52, anims[ri(0, 4)]); w8(hm + 0x53, anims[ri(0, 4)]); wf(hm + 0x48, rf(-3.1f, 3.1f)); }
        w8(0x0015edb5, u(ri(0, 1)));
        if (coin()) wf(m + 0x48, rf(-3.1f, 3.1f));
        if (coin()) w32(0x0015ed84, 1);  // (Novalis's flags on any level)
        // a third of the cases: the hero ready to work it (every condition met but at most one)
        if (st == 1 && coin()) {
            const int off = ri(0, 9);
            Vec4 r = around(ee_ref<Vec4>(m + 0x10), 0.3f, off == 0 ? 2.0f : 1.6f, 0.0f, 0.0f);
            wf(pv + 0x3c, ee_read<float>(m + 0x18));
            if (off == 1) wf(pv + 0x3c, ee_read<float>(m + 0x18) + 0.1f);
            r.z = ee_read<float>(m + 0x18) + (off == 2 ? rf(0.0f, 0.6f) : 0.3f + rf(-0.1f, 0.1f));  // (the height's limits)
            ee_ref<Vec4>(0x0013f3d0) = r;
            const float a = std::atan2(ee_read<float>(m + 0x10) - r.x, ee_read<float>(m + 0x14) - r.y);
            wf(hero + 0x98, std::remainder(a + (off == 3 ? 2.0f : rf(-1.0f, 1.0f)), 6.2831853f));
            w8(hero + 0x20a4, off == 4 ? 1u : 0u);
            w32(hero + 0x10b8, off == 5 ? 7u : 8u);
            w32(hero + 0x1090, ee_read<uint32_t>(0x001413d0));
            w32(pad + 0x190, 30);
            for (uint32_t i = 0; i < 30; ++i) w32(pad + 0x1e0 + 4 * i, off == 6 ? 0u : 0x80u);
            w32(pv + 0x14, off == 7 ? u(frame_scale_i(30) + ri(0, 1)) : u(ri(40, 90)));  // (its limit, frame_scale_i(30))
            if (off == 9 || ri(0, 3) == 0) {  // (a moby of the list 0x15ffe4 near its limit, 3.25)
                const uint32_t o = ee_read<uint32_t>(OV(0x0015ffe4));
                if (o != 0) {
                    w16(o + 0x34, ee_read<uint16_t>(o + 0x34) | 0x1000u);
                    const float d = rf(2.9f, 3.4f);
                    Vec4 q = ee_ref<Vec4>(m + 0x10);
                    q.x += d;
                    ee_ref<Vec4>(o + 0x10) = q;
                }
            }
            wf(pv, off == 8 ? 1.0f : rf(0.0f, 0.9f));
        }
        if (st == 3) {  // (working it: the button held near its limit, the speed at the animations' and the dead zone's limits)
            if (ri(0, 3) != 0) w32(0x001413d4, 0x3b);
            if (ri(0, 2) != 0) { w32(pad + 0x190, 0); wf(pv, rf(0.0f, 0.95f)); }  // (worked on: not released)
            if (coin()) w32(hero + 0x198, u(ri(45, 65)));
            const float ft = ee_read<float>(0x0015ed6c), ft2 = ee_read<float>(0x0015ed70);
            const int k3 = ri(0, 3);
            if (k3 == 0) {
                wf(pv + 0x1c, ft * 3.7f * rf(0.35f, 0.7f));
                const uint32_t hm2 = ee_read<uint32_t>(hero + 0x2080);
                if (hm2 != 0 && coin()) { w8(hm2 + 0x52, coin() ? 0x41u : 0x40u); w8(hm2 + 0x53, 0x10); w32(hero + 0xa9c, 0); }
            }
            else if (k3 == 1) { wf(pv + 0x1c, -ft2 * 5.5f + ft2 * 5.5f * rf(0.2f, 0.35f)); wfrand(hero + 0x1d20, 2, -1.5f, 1.5f); }
            wrand(hero + 0x160, 4);  // (the words its clear reaches last)
        }
        if (ri(0, 9) == 0) w32(m + 0x78, 0);  // (a pump without pvars)
    };
    for (int st = 0; st < 7; ++st)
        classcheck("class280_update", upd, pumps, class280_update, [vary, st](uint32_t m) { vary(m, st); }, st == 1 || st == 3 ? 120 : 30);
    classcheck("class280_level_flags", flg, pumps, class280_level_flags, [](uint32_t m) {
        w32(0x0015ed84, coin() ? 1u : u(ri(0, 18)));
        w16(m + 0xb2, coin() ? u(ri(0x33, 0x36)) : u(ri(0, 0x100)));
        w16(m + 0xa6, coin() ? 0x118u : u(ri(0, 0x200)));
        w8(m + 0x20, coin() ? 5u : u(ri(0, 6)));
    }, 60);
    fcheck("camera5_angles_set", cam, 40, Ret::None, [] {
        level();
        if (const uint32_t c = ee_read<uint32_t>(OV(0x00166e00)); c != 0 && coin()) w16(c + 0x86, coin() ? 5u : u(ri(0, 8)));
        return Args{{}, {rf(-3.2f, 3.2f), rf(-3.2f, 3.2f)}};
    }, [](A a) { camera5_angles_set(a.f[0], a.f[1]); return uint64_t{0}; });
}

// ---- class 631 (shared_class631.cpp)
void class631_checks() {
    const uint32_t upd = copy_here({{3, 0x002cca00}, {7, 0x002fd908}});
    const uint32_t hits = copy_here({{3, 0x002ce238}, {7, 0x002ff140}});
    if (upd == 0) {
        for (const char* n : {"class631_update", "class631_hits"}) not_here(n);
        return;
    }
    const uint32_t tun = g_oracle_level == 7 ? 0x00161934u : 0x00161994u;  // (the class's numbers)
    // (its death's pieces: debris_spawn's two w lanes, as batch 510 leaves out; for these checks only)
    const size_t ign = g_ignored_words.size();
    level();
    for (uint32_t a = end_moby(); a < end_moby() + 0x80000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
    auto crafts = [] { return placed_or_spawned(631); };
    // a path's segment lengths (its points' w), as the craft's state 0 / 2 set them
    auto lengths = [](int32_t pi) {
        if (pi < 0) return;
        const uint32_t p = path_at(pi);
        for (int32_t i = 0; i < ee_read<int32_t>(p) - 2; ++i) {
            const uint32_t q = p + 0x10 + u(i) * 0x10;
            const float dx = ee_read<float>(q + 0x10) - ee_read<float>(q), dy = ee_read<float>(q + 0x14) - ee_read<float>(q + 4),
                        dz = ee_read<float>(q + 0x18) - ee_read<float>(q + 8);
            wf(q + 0xc, std::sqrt(dx * dx + dy * dy + dz * dz));
        }
    };
    auto vary = [tun, lengths](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        vary_frame_scale();
        seed_rand();
        w8(m + 0x20, u(st));
        w8(m + 0x31, u(ri(0, 1)));
        if (coin()) ee_ref<Vec4>(OV(0x00166dc0)) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 60.0f);
        w8(pv + 0x2e, ri(0, 7) == 0 ? 2u : 1u);
        w16(m + 0xb2, u(ri(0, 0x7ff)));
        if (ri(0, 9) == 0) {
            const uint32_t idx = ee_read<uint16_t>(m + 0xb2), w = OV(0x001ba4d0) + ((idx >> 5) << 2);
            w32(w, ee_read<uint32_t>(w) | (1u << (idx & 0x1f)));
        }
        w8(m + 0xb0, u(ri(0, 0x3f)));
        w8(0x0014c050 + ee_read<uint8_t>(m + 0xb0) + (ee_read<uint32_t>(0x0015ed84) << 4), ri(0, 3) == 0 ? 0xffu : 0u);
        // its paths as placed (start, first, the route's two, the second follower's; now and then none), their segments'
        // lengths as its states 0 / 2 set them; the followers' points
        auto own = [pv](uint32_t o, int none) {
            const int32_t i = lw(pv + o);
            const bool ok = i >= 0 && i < 64 && path_at(i) >= 0x00100000u && path_at(i) < 0x02000000u && ee_read<int32_t>(path_at(i)) >= 2 &&
                            ee_read<int32_t>(path_at(i)) <= 200;
            if (!ok && i != -1) w32(pv + o, u(a_path()));  // (a spawned craft's blank words: one of the level's paths)
            if (ri(0, none) == 0) w32(pv + o, 0xffffffffu);
            return lw(pv + o);
        };
        const int32_t start = own(0x134, 12), second = own(0x164, 5), first = own(0x24c, 6), r1 = own(0x230, 5), r2 = own(0x234, 5);
        for (int32_t pi : {start, first, r1, r2, second}) lengths(pi);
        if (start >= 0) w32(pv + 0x130, path_at(start));
        if (second >= 0) w32(pv + 0x160, path_at(second));
        w32(pv + 0x23c, u(ri(0, 1)));
        for (uint32_t f : {pv + 0x120, pv + 0x150}) {
            const int32_t pi = f == pv + 0x120 ? start : second;
            if (pi < 0) continue;
            w32(f + 0x10, path_at(pi));
            const int32_t n = ee_read<int32_t>(path_at(pi));
            w32(f, u(ri(0, 3) == 0 ? ri(-1, n) : ri(1, std::max(1, n - 2))));
        }
        if (st == 2) {  // (the route's paths' lengths not set yet: its switch sets them)
            for (int32_t pi : {r1, r2})
                if (pi >= 0 && coin()) for (int32_t i = 0; i < ee_read<int32_t>(path_at(pi)) - 2; ++i) wf(path_at(pi) + 0x1c + u(i) * 0x10, rf(0.5f, 9.0f));
        }
        if (st == 0) w16(m + 0x32, u(ri(90, 140)));
        if (st == 0) {  // (set up again: its turret's joints unlinked first and cleared, as before its first state 0)
            sub_0024f608(m, pv + 0x180);
            sub_0024f608(m, pv + 0x1c0);
            for (uint32_t k = 0x180; k < 0x200; k += 4) w32(pv + k, 0);
        }
        // its zones: start, route, fire (the hero in them or not)
        const uint32_t boxes = ee_read<uint32_t>(OV(0x001600ec));
        auto zone = [boxes]() { return boxes != 0 && ri(0, 3) != 0 ? ri(0, 4) : -1; };
        const int32_t z248 = zone(), z20c = zone(), z244 = zone(), z208 = zone();
        w32(pv + 0x248, u(z248));
        w32(pv + 0x20c, u(z20c));
        w32(pv + 0x244, u(z244));
        w32(pv + 0x208, u(z208));
        const int32_t in = std::array<int32_t, 4>{z248, z20c, z244, z208}[static_cast<size_t>(ri(0, 3))];
        if (in >= 0 && coin()) {
            Vec4 c = ee_ref<Vec4>(boxes + u(in) * 0x80 + 0x30);
            c.x += rf(-0.3f, 0.3f); c.y += rf(-0.3f, 0.3f);
            ee_ref<Vec4>(0x0013f3d0) = c;
            ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10) = c;
        } else if (coin()) {
            const Vec4 c = around(ee_ref<Vec4>(m + 0x10), 2.0f, 40.0f, -10.0f, 10.0f);
            ee_ref<Vec4>(0x0013f3d0) = c;
            ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10) = c;
        }
        // near its path's end, slow or fast (the arrivals)
        const int32_t on = st == 1 ? first : (ee_read<uint32_t>(pv + 0x23c) != 0 ? r2 : r1);
        if (on >= 0 && (st == 1 || st == 3) && coin()) {  // (at its path's last point: the arrival)
            const uint32_t p = path_at(on);
            ee_ref<Vec4>(m + 0x10) = ee_ref<Vec4>(p + 0x10 + u(ee_read<int32_t>(p) - 1) * 0x10);
        } else if (on >= 0 && coin()) {
            const uint32_t p = path_at(on);
            const int32_t n = ee_read<int32_t>(p);
            ee_ref<Vec4>(m + 0x10) = around(ee_ref<Vec4>(p + 0x10 + u(coin() ? n - 1 : ri(0, n - 1)) * 0x10), 0.0f, 3.0f, -0.5f, 0.5f);
        }
        const float ft = ee_read<float>(0x0015ed6c);
        wf(pv + 0x214, coin() ? ee_read<float>(tun + 8) * ft * rf(0.5f, 1.1f) : rf(0.0f, ft * 20.0f));
        wfrand(pv + 0x218, 2, -0.2f, 0.2f);
        wf(pv + 0x210, rf(-3.1f, 3.1f));
        wfrand(pv + 0x220, 4, -1.0f, 1.0f);
        // the rider (by index; of its class or not, living or not), the charge in flight, the sound slot
        const std::vector<uint32_t> all = live_mobys();
        auto index_of = [](uint32_t mo) { return (mo - first_moby()) >> 8; };
        // (the rider and the charge: the level's mobys of their classes (574, 848) or others; their class ids left alone)
        const std::vector<uint32_t> riders = mobys_of(0x23e), charges = mobys_of(0x350);
        // (the rider: its own (as placed), another of its class, or none; never any moby: the rider's transform reads its
        // class's data)
        {  // (a craft whose words were never set (the level's fill): no rider)
            const int32_t r0 = lw(pv + 0x238);
            if (r0 != -1 && (r0 < 0 || first_moby() + (u(r0) << 8) >= end_moby())) w32(pv + 0x238, 0xffffffffu);
        }
        const int kr = ri(0, 3);
        if (kr == 1 && !riders.empty()) w32(pv + 0x238, index_of(pick(riders)));
        else if (kr == 2) w32(pv + 0x238, 0xffffffffu);
        if (lw(pv + 0x238) != -1 && ri(0, 4) == 0) w8(first_moby() + (ee_read<uint32_t>(pv + 0x238) << 8) + 0x20, 0x80);
        if (coin()) {
            const uint32_t c = !charges.empty() && coin() ? pick(charges) : pick(all);
            w32(pv + 0x240, c);
            if (ri(0, 4) == 0) w8(c + 0x20, coin() ? 0xfeu : 0xfdu);
        } else {
            w32(pv + 0x240, 0);
        }
        const int32_t slot = ri(0, 2) == 0 ? -1 : ri(0, 7);
        w32(pv + 0x250, u(slot));
        if (slot >= 0) { const uint32_t e = 0x0013e550 + u(slot) * 0x70; w32(e + 0x88, coin() ? m : 0u); w8(e + 0x74, u(ri(0, 1))); }
        w32(pv + 0x38, ri(0, 4) == 0 ? 1u : 0u);
        w32(pv + 0x254, ri(0, 2) == 0 ? 0u : u(ri(1, 200)));
        w16(pv + 0x204, ri(0, 2) == 0 ? u(ri(0, 1)) : u(ri(2, 300)));
        w16(pv + 0x206, ri(0, 2) == 0 ? u(ri(0, 1)) : u(ri(2, 60)));
        w8(m + 0xbc, u(ri(1, 4)));
        // a hit (its record applied or not)
        if (st == 5 || ri(0, 2) == 0) give_hit(m, 0x330000);
        else w8(m + 0xa4, 0xff);
        if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
            const uint32_t e = OV(0x00178100) + (u(ee_read<uint8_t>(m + 0xa4)) << 6);
            w8(e + 0x28, u(ri(0, 6)));
            w8(e + 0x29, u(ri(0, 4)));
            if (coin()) w32(e + 0x1c, 0x45afdf66u);
            if (coin()) {  // (a reaction of its own: an applied record of a damage kind with a reaction, the hit timer running
                           // for the same weapon, health left)
                static const uint8_t kinds[] = {1, 2, 3, 4, 7};
                w32(e + 0x1c, 0x45afdf66u);
                w8(e + 0x28, kinds[ri(0, 4)]);
                w8(e + 0x29, u(ri(0, 3)));
                const uint32_t weap = u(ri(3, 0x40));
                w16(e + 0x2a, weap);
                w16(pv + 0x34, weap);
                w16(pv + 0x3c, u(ri(2, 100)));
                wf(e + 0x2c, 0.5f);
                wf(pv + 0x20, rf(1.0f, 4.0f));
            }
        }
        if (coin()) wf(pv + 0x20, ri(0, 2) == 0 ? rf(-1.0f, 0.5f) : rf(0.5f, 4.0f));
        if (ee_read<uint8_t>(m + 0xa4) != 0xff && ri(0, 3) == 0) {  // (the health left at the edge of 0)
            const uint32_t e = OV(0x00178100) + (u(ee_read<uint8_t>(m + 0xa4)) << 6);
            wf(pv + 0x20, ee_read<float>(e + 0x2c) + rf(-0.15f, 0.05f));
        }
        if (coin()) wf(m + 0x40, rf(-0.4f, 0.4f));
        if (coin()) wf(m + 0x44, rf(-0.4f, 0.4f));
    };
    for (int st = 0; st < 7; ++st)
        classcheck("class631_update", upd, crafts, class631_update, [vary, st](uint32_t m) { vary(m, st); }, st == 2 ? 80 : 40);
    classcheck("class631_hits", hits, crafts, class631_hits, [vary](uint32_t m) { vary(m, ri(0, 5)); }, 80);
    g_ignored_words.resize(ign);
}

}  // namespace

void batch_515() {
    if (g_only_batch != 515) return;
    g_batch = 515;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    // (the scratchpad words the pose function 0x245f18 spills the caller's $t8 / $t9 into: batch 120's)
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // 989snd's IOP server (as batches 120 / 506 / 508): replies a function of the command
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
        for (const auto& [r, v] : g_wide_regs) ee.cpu().gpr[r].lo = v;
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

    class615_checks();
    class1844_checks();
    ocean_checks();
    class447_checks();
    class1023_checks();
    class280_checks();
    class631_checks();
    //@@NEXT

    g_wide_regs.clear();
    g_pre_call = nullptr;
    g_extra_noop_hooks.clear();
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
