// Oracle batch 501 (stage 4 task G1, #149): the hand-written ports of Novalis's (and the hero's level-1) helpers that
// other levels carry copies of, generalised to serve every level of their family: each check calls the original at the
// level's own copy and runs the one hand-written port. The level copies stay transliterated in their units until each
// level is re-glued (phase 2); here every copy is the original and the hand-written port the port:
//  - novalis_water.cpp: the water grids (0x261c10 spread, 0x262038 wave step, 0x2b7f30 step, 0x261d78 blend, 0x2b7fe0 the
//    frame; levels 1, 5, 7, 11, 12, 13) and the strips' packet 0x21fa98 (levels 1, 9, 12, 14);
//  - novalis_amoeboid.cpp: the wall-path helpers 0x276c40 / 0x276a48 / 0x276fe8 (levels 1, 5, 6, 7, 10, 11) and particle
//    0x34's spawn 0x287158 (levels 1, 5, 10, 11, 12, 13);
//  - novalis_mobys.cpp: class 805's update (the spawn point, levels 1..18), the scripted camera's nodes 0x2fb4b8 (levels 1,
//    2, 3, 5, 7, 10, 13), the plants' bolts 0x2787a0 (levels 1, 4..17), a moby's clone 0x278e20 (levels 1, 4, 8, 9, 10, 11,
//    12, 17), a moby on the screen 0x275690 (levels 1, 3, 4, 5, 7, 11, 12, 13, 14);
//  - novalis_enemies.cpp: a group's +0xbc 0x26e090 (levels 1, 3, 7, 9, 12, 13, 15, 16), the walker record 0x26d930 (levels
//    1..16 but 7), a cooldown slot 0x2efbf8 (levels 1, 13), a path's next point 0x28b5e0 (levels 1, 9, 13, 15);
//  - novalis_npcs.cpp: a level-data note 0x29b0a0 (levels 1, 3, 4, 5, 8), the Plumber's camera nodes 0x2ff028 (levels 1, 8),
//    the hero walked to a moby 0x2783a8 (levels 1, 2, 3, 5, 6, 8, 11, 12, 14, 16);
//  - novalis_path.cpp: a path follower started 0x28b410 (levels 1, 3, 4, 12, 14), a path's nearest point 0x28b510 (levels 1,
//    3, 4, 5, 7, 8, 9, 12, 13, 14, 16; the hoverboard race's on 5 and 16 too), the debug particle 0x285768 (levels 1, 2, 3,
//    4, 6, 10, 14);
//  - novalis_hive.cpp: a group's count 0x26e008 (levels 1, 6, 9, 11, 12, 13, 14, 15, 16);
//  - hero_l1_state.cpp / hero_l1_misc.cpp: the special mode's exit 0x231450 and the mode's idle 0x2405a0 (levels 1, 2).
// Subjects: the level after its first gameplay frame (the port's) with its view set up; the level's own mobys of class 805;
// paths, groups and mobys of the level; water tiles, wall lists, followers, packets and records built in the test area
// (the packets at a cursor on a buffer of the test area). No RAM dumps.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 501, N = each level a family names (1 .. 18); a function the level
// does not carry says so and is not run.

#include "ovl_oracle.h"

#include <game/ovl/hero_l1.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/novalis_water.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/disc.h>
#include <runtime/vu/vu0.h>

#include <cmath>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <vector>

namespace rac1::oracle {

namespace {
// The VU0 the original started from (the interpreter's), given to the port before it runs (as batch 506)
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

struct LvAddr {
    int level;
    uint32_t addr;
};

// the family's copies on the level being run (none: the level has none)
std::vector<uint32_t> copies_here(std::initializer_list<LvAddr> copies) {
    std::vector<uint32_t> v;
    for (const LvAddr& c : copies)
        if (c.level == g_oracle_level) v.push_back(c.addr);
    return v;
}

bool selected(const char* name) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return false;
    const char* only = std::getenv("ORACLE_ONLY");
    return only == nullptr || std::strcmp(only, name) == 0;
}

// A check whose original is the level's own copy at `laddr`.
void fcheck(const char* name, uint32_t laddr, int n, Ret ret, const std::function<Args()>& gen,
            const std::function<uint64_t(const Args&)>& port) {
    if (!selected(name)) return;
    std::printf("%-26s L%d:0x%08x\n", name, g_oracle_level, laddr);
    if (!check_fn(name, laddr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}

void not_here(const char* name) {
    if (selected(name)) std::printf("%-26s not on level %d\n", name, g_oracle_level);
}

// Each copy of the family on this level checked (or "not on level N").
void each_copy(const char* name, std::initializer_list<LvAddr> copies, const std::function<void(uint32_t)>& run) {
    const std::vector<uint32_t> here = copies_here(copies);
    if (here.empty()) { not_here(name); return; }
    for (uint32_t a : here) run(a);
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
        if (g_oracle_level == 14) w8(0x0013d3f0, 1);  // (Oltanis: the arrival scene seen, as batch 221's)
        gameplay_frame();
        // the screen and projection constants (the screen test's 0x16cd00..), which the oracle's display (no size) leaves
        // broken: a 640 x 448 screen, as the game's
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

uint32_t first_moby() { return ee_read<uint32_t>(OV(0x0015ffd8)); }

// the level's live mobys (with a class), and those whose update function is `fn` (the level's address)
std::vector<uint32_t> live_mobys() {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x24) != 0) v.push_back(m);
    return v;
}
std::vector<uint32_t> mobys_with(uint32_t fn) {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
    return v;
}

uint32_t pick(const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; }

bool in_ram(uint32_t a) { return a >= 0x00100000u && a < 0x01ff0000u; }

// the level's paths (the table Veldin's 0x1b04b0): the indices whose entry points at a path of 1..32 points
std::vector<uint32_t> level_paths() {
    std::vector<uint32_t> v;
    const uint32_t tbl = OV(0x001b04b0);
    for (uint32_t i = 0; i < 64; ++i) {
        const uint32_t p = ee_read<uint32_t>(tbl + i * 4);
        const bool ok = in_ram(p) && (p & 0xf) == 0 && ee_read<int32_t>(p) >= 1 && ee_read<int32_t>(p) <= 32;
        if (std::getenv("B501_INFO") != nullptr) std::printf("INFO path %u: %08x (%d)\n", i, p, in_ram(p) ? ee_read<int32_t>(p) : -1);
        if (ok) v.push_back(i);
    }
    return v;
}
uint32_t path_at(uint32_t i) { return ee_read<uint32_t>(OV(0x001b04b0) + i * 4); }

// the level's moby groups (the table Veldin's 0x1ab840, lists of halves, the last with bit 15): none, or a list in RAM
// that ends within 256 entries
std::vector<uint32_t> level_groups() {
    std::vector<uint32_t> v;
    const uint32_t tbl = OV(0x001ab840);
    const uint32_t nmob = (ee_read<uint32_t>(OV(0x0015ffe0)) - first_moby()) >> 8;
    for (uint32_t g = 0; g < 64; ++g) {
        const uint32_t p = ee_read<uint32_t>(tbl + g * 4);
        if (p == 0) { v.push_back(g); continue; }
        if (!in_ram(p) || (p & 1) != 0) continue;
        bool ok = false;
        for (uint32_t k = 0; k < 256; ++k) {
            const uint32_t id = ee_read<uint16_t>(p + 2 * k);
            if ((id & 0x7fffu) >= nmob) break;
            if ((id & 0x8000u) != 0) { ok = true; break; }
        }
        if (ok) v.push_back(g);
    }
    return v;
}

// a point near `at` (xy within r, z within rz)
Vec4 near(const Vec4& at, float r, float rz) {
    Vec4 q = at;
    q.x += rf(-r, r);
    q.y += rf(-r, r);
    q.z += rf(-rz, rz);
    return q;
}

// the hero at `r0`..`r1` from the point
void hero_around(const Vec4& at, float r0, float r1) {
    Vec4 q = at;
    const float r = rf(r0, r1), a = rf(-3.1f, 3.1f);
    q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-1, 3);
    ee_ref<Vec4>(0x0013f3d0) = q;
}

// the frame's packet cursor (0x1611c0) on a buffer of the test area (after the first gameplay frame it is 0: the packets
// would go to the null page, which the comparison leaves out)
void packet_cursor() { w32(OV(0x001611c0), TEST_AREA + 0x8000 + u(ri(0, 0x40)) * 0x10); }

// (a trial run of the port: the state is dealt again when it throws on a pointer outside RAM, as batch 506's)
bool trial_ok(const std::function<void()>& port) {
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

// ---- the water grids (novalis_water.cpp)
constexpr uint32_t SPR = 0x70000000u;
constexpr uint32_t TILES = TEST_AREA;  // (up to 6 records of 0x1190 bytes)
constexpr uint32_t TILE = 0x1190;

// `n` tile records: their three blocks' heights (small floats, some exactly the threshold's size), their neighbours
// (mostly tiles of the set, some none), live or not
void water_tiles(int32_t n) {
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t t = TILES + u(i) * TILE;
        wrand(t, 0x50 / 4);
        for (uint32_t a = t + 0x50; a < t + 0x50 + 3 * 0x5c0; a += 4) wf(a, ri(0, 7) == 0 ? 0.0f : rf(-0.5f, 0.5f));
        for (uint32_t k = 0; k < 8; ++k) w8(t + 0xc + k, ri(0, 3) == 0 ? 0xffu : u(ri(0, n - 1)));
        w16(t + 0x1e, ri(0, 4) == 0 ? 0u : u(ri(1, 3)));
    }
}
// the water's engine words: the current / previous / drawn blocks, the phase, the step and the threshold
void water_words() {
    w32(OV(0x00161218), u(ri(0, 2)));
    w32(OV(0x0016121c), u(ri(0, 2)));
    w32(OV(0x00161220), u(ri(0, 2)));
    wf(OV(0x00161224), ri(0, 3) == 0 ? rf(0.9f, 1.1f) : rf(0.0f, 1.0f));
    if (coin()) wf(OV(0x001ca8a0), rf(0.01f, 0.3f));
    if (coin()) wf(OV(0x001ca89c), rf(0.0001f, 0.05f));
}

void water_checks() {
    each_copy("water_grid_in", {{1, 0x00261c10}, {5, 0x00276bc0}, {7, 0x00275380}, {11, 0x00272fa8}, {12, 0x00265de8}, {13, 0x00258e78}},
              [](uint32_t a) {
        fcheck("water_grid_in", a, 40, Ret::None, [] {
            level();
            water_tiles(1);
            wrand(SPR, 0x800 / 4);
            return Args{{SPR, TILES + 0x50 + u(ri(0, 2)) * 0x5c0}, {}};
        }, [](A a) { nov_water_grid_in_l1(a.i[0], a.i[1]); return uint64_t{0}; });
    });
    each_copy("water_grid_wave", {{1, 0x00262038}, {5, 0x00276fe8}, {7, 0x002757a8}, {11, 0x002733d0}, {12, 0x00266210}, {13, 0x002592a0}},
              [](uint32_t a) {
        fcheck("water_grid_wave", a, 60, Ret::None, [] {
            level();
            const int32_t n = ri(1, 4);
            water_tiles(n);
            water_words();
            for (uint32_t g = SPR; g < SPR + 0x800; g += 4) wf(g, ri(0, 7) == 0 ? 0.0f : rf(-0.6f, 0.6f));
            if (coin()) nov_water_grid_in_l1(SPR, TILES + u(ri(0, n - 1)) * TILE + 0x50 + u(ri(0, 2)) * 0x5c0);
            return Args{{SPR, u(ri(0, n - 1)), u(ri(0, 2)), TILES}, {}};
        }, [](A a) { nov_water_grid_wave_l1(a.i[0], static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]), a.i[3]); return uint64_t{0}; });
    });
    each_copy("water_grid_step", {{1, 0x002b7f30}, {5, 0x002cc910}, {7, 0x002cae68}, {11, 0x002c70b8}, {12, 0x002baa48}, {13, 0x002ae850}},
              [](uint32_t a) {
        fcheck("water_grid_step", a, 40, Ret::None, [] {
            level();
            const int32_t n = ri(1, 4);
            water_tiles(n);
            water_words();
            return Args{{TILES, u(ri(0, 5) == 0 ? ri(-1, 0) : n)}, {}};
        }, [](A a) { nov_water_grid_step_l1(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
    });
    each_copy("water_grid_blend", {{1, 0x00261d78}, {5, 0x00276d28}, {7, 0x002754e8}, {11, 0x00273110}, {12, 0x00265f50}, {13, 0x00258fe0}},
              [](uint32_t a) {
        fcheck("water_grid_blend", a, 40, Ret::None, [] {
            level();
            water_tiles(1);
            static const float ts[] = {0.0f, 1.0f, 0.5f, -0.25f, 1.5f};
            const float t = coin() ? ts[ri(0, 4)] : rf(0.0f, 1.0f);
            const uint32_t b0 = TILES + 0x50;
            return Args{{b0 + u(ri(0, 2)) * 0x5c0, b0 + u(ri(0, 2)) * 0x5c0, b0 + u(ri(0, 2)) * 0x5c0}, {t}};
        }, [](A a) { vu0_sync(); nov_water_grid_blend_l1(a.i[0], a.i[1], a.i[2], a.f[0]); return uint64_t{0}; });
    });
    each_copy("water_grid_update", {{1, 0x002b7fe0}, {5, 0x002cc9c0}, {7, 0x002caf18}, {11, 0x002c7168}, {12, 0x002baaf8}, {13, 0x002ae900}},
              [](uint32_t a) {
        fcheck("water_grid_update", a, 60, Ret::None, [] {
            level();
            const int32_t n = ri(1, 4);
            water_tiles(n);
            water_words();
            return Args{{TILES, u(ri(0, 6) == 0 ? ri(-1, 0) : n)}, {}};
        }, [](A a) { vu0_sync(); nov_water_grid_update_l1(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
    });
    each_copy("prim_list2", {{1, 0x0021fa98}, {9, 0x0021d250}, {12, 0x00220a58}, {14, 0x00211128}}, [](uint32_t a) {
        fcheck("prim_list2", a, 60, Ret::None, [] {
            level();
            packet_cursor();
            const int32_t n = ri(0, 8) == 0 ? ri(1, 2) : ri(1, 120);  // (0 is no case: the original's loops then run 2^32 times)
            wrand(SPR, n * 3);
            wrand(SPR + 0x1000, n * 2);
            wrand(SPR + 0x2000, n * 2);
            wrand(SPR + 0x3000, n);
            return Args{{u(n), SPR, SPR + 0x3000, SPR + 0x1000, SPR + 0x2000, R()}, {}};
        }, [](A a) { nov_prim_list2_l1(static_cast<int32_t>(a.i[0]), a.i[1], a.i[2], a.i[3], a.i[4], a.i[5]); return uint64_t{0}; });
    });
}

// ---- the wall-path helpers and particle 0x34 (novalis_amoeboid.cpp)
// a list of `n` wall paths (the level's) at T, and a point near one of the level's paths' points
uint32_t wall_list(const std::vector<uint32_t>& paths, int32_t n) {
    for (int32_t k = 0; k < n; ++k) w32(T + u(k) * 4, pick(paths));
    return T;
}
Vec4 near_path(const std::vector<uint32_t>& paths, float r) {
    const uint32_t p = path_at(pick(paths));
    const int32_t cnt = ee_read<int32_t>(p);
    return near(ee_ref<Vec4>(p + 0x10 + u(ri(0, cnt - 1)) * 0x10), r, 2.0f);
}

void wall_path_checks() {
    const std::initializer_list<LvAddr> c40 = {{1, 0x00276c40}, {5, 0x0028be08}, {6, 0x00270af8}, {7, 0x0028a3d0}, {10, 0x00254a78}, {11, 0x00287340}};
    const std::initializer_list<LvAddr> a48 = {{1, 0x00276a48}, {5, 0x0028bc10}, {6, 0x00270900}, {7, 0x0028a1d8}, {10, 0x00254880}, {11, 0x00287148}};
    const std::initializer_list<LvAddr> fe8 = {{1, 0x00276fe8}, {5, 0x0028c5f8}, {6, 0x002712e8}, {7, 0x0028a910}, {10, 0x00255268}, {11, 0x00287b30}};
    std::vector<uint32_t> paths;
    if (!copies_here(c40).empty() && (selected("wall_path_meet") || selected("wall_path_seen") || selected("wall_path_way"))) {
        level();
        paths = level_paths();
        std::printf("  (wall paths: %zu paths on this level)\n", paths.size());
        if (paths.empty()) { std::printf("  no path on this level\n"); return; }
    }
    each_copy("wall_path_meet", c40, [&paths](uint32_t a) {
        fcheck("wall_path_meet", a, 60, Ret::Int32, [paths] {
            level();
            const uint32_t want = ri(0, 3) == 0 ? 0u : R(), have = ri(0, 5) == 0 ? 0u : (coin() ? R() : 1u << ri(0, 31));
            return Args{{pick(paths), want, have}, {}};
        }, [](A a) { return r32(nov_276c40_l1(static_cast<int32_t>(a.i[0]), a.i[1], a.i[2])); });
    });
    each_copy("wall_path_seen", a48, [&paths](uint32_t a) {
        fcheck("wall_path_seen", a, 60, Ret::Int32, [paths] {
            level();
            const int32_t n = ri(0, 4);
            const uint32_t walls = wall_list(paths, n);
            ee_ref<Vec4>(OUT) = near_path(paths, 15.0f);
            return Args{{walls, u(n), pick(paths), OUT}, {ri(0, 2) == 0 ? 0.0f : rf(0.1f, 2.0f)}};
        }, [](A a) { return r32(nov_276a48_l1(a.i[0], static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]), a.i[3], a.f[0])); });
    });
    each_copy("wall_path_way", fe8, [&paths](uint32_t a) {
        fcheck("wall_path_way", a, 80, Ret::Int32, [paths] {
            level();
            const int32_t n = ri(0, 4);
            const uint32_t walls = wall_list(paths, n);
            ee_ref<Vec4>(OUT) = near_path(paths, 15.0f);
            ee_ref<Vec4>(OUT + 0x10) = coin() ? near_path(paths, 15.0f) : near(ee_ref<Vec4>(OUT), 6.0f, 1.0f);
            wrand(OUT + 0x20, 4);
            return Args{{walls, u(n), pick(paths), OUT, OUT + 0x10, OUT + 0x20}, {ri(0, 2) == 0 ? 0.0f : rf(0.1f, 2.0f)}};
        }, [](A a) {
            return r32(nov_276fe8_l1(a.i[0], static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]), a.i[3], a.i[4], a.i[5], a.f[0]));
        });
    });
}

void particle34_checks() {
    each_copy("particle34_spawn", {{1, 0x00287158}, {5, 0x0029c180}, {10, 0x00264ed8}, {11, 0x002972c8}, {12, 0x0028a7d0}, {13, 0x0027da80}},
              [](uint32_t a) {
        fcheck("particle34_spawn", a, 40, Ret::Int32, [] {
            level();
            ee_ref<Vec4>(OUT) = near(ee_ref<Vec4>(0x0013f3d0), 5.0f, 2.0f);
            seed_rand();
            return Args{{OUT, R(), R(), u(ri(1, 90))}, {rf(0.0f, 2.0f), rf(-1.0f, 1.0f)}};
        }, [](A a) { return r32(nov_287158_l1(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]), a.f[0], a.f[1])); });
    });
}

// ---- class 805, the spawn point (novalis_mobys.cpp)
void spawn_point_checks() {
    const std::vector<uint32_t> here = copies_here({{1, 0x00300220}, {2, 0x002e28a0}, {3, 0x002d1140}, {4, 0x002d9fc0}, {5, 0x00309e40},
                                                    {6, 0x002e66e0}, {7, 0x003027a0}, {8, 0x002f99b0}, {9, 0x002f90e0}, {10, 0x002ca220},
                                                    {11, 0x00300578}, {12, 0x002f8090}, {13, 0x002fac70}, {14, 0x002f4190}, {15, 0x002db668},
                                                    {16, 0x002d69e0}, {17, 0x002d9b10}, {18, 0x002e1f40}});
    if (here.empty()) { not_here("spawn_point_update"); return; }
    if (!selected("spawn_point_update")) return;
    const uint32_t fn = here[0];
    level();
    const std::vector<uint32_t> ms = mobys_with(fn);
    if (ms.empty()) { std::printf("%-26s L%d:0x%08x  no moby of class 805 on this level\n", "spawn_point_update", g_oracle_level, fn); return; }
    std::printf("  (spawn_point_update: %zu subjects)\n", ms.size());
    for (int pass = 0; pass < 3; ++pass)
        fcheck("spawn_point_update", fn, 60, Ret::None, [ms, pass] {
            level();
            const uint32_t m = pick(ms);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const uint32_t level_no = ee_read<uint32_t>(0x0015ed84);
            w32(pv + 8, pass == 0 ? R() & 0x1e : (R() & 0x16));
            w8(m + 0xbc, ri(0, 5) == 0 ? 0u : 1u); w8(m + 0x20, R() & 1);
            w32(pv + 0xc, R() & 1); w32(pv + 0x10, R() & 1); w32(pv + 0x14, R() & 1);
            w32(pv + 0x18, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 15)));
            if (ri(0, 3) == 0) w8(m + 0xb0, ri(0, 2) == 0 ? 0xffu : u(ri(0, 15)));
            for (uint32_t i = 0; i < 16; ++i) w8(0x0014c050 + level_no * 16 + i, coin() ? 0xffu : 0u);
            const uint32_t idx = u(ri(0, 0x7f));
            w16(m + 0xb2, idx);
            w8(OV(0x001bb230) + idx + 0x454, R() & 1); w8(OV(0x001ba5d0) + idx + 0x454, R() & 1);
            for (uint32_t i = 0; i < 4; ++i) { w32(0x0014c190 + level_no * 256 + i * 4, R()); w32(OV(0x001ba4d0) + i * 4, R()); }
            static const uint32_t ks[] = {0, 1, 3, 5}, runs[] = {0, 1, 5, 9};
            w8(0x001413f4, ks[ri(0, 3)]); w32(0x001413dc, runs[ri(0, 3)]);
            if (pass == 2 || coin()) {  // the hero in the box of the flags' index (or elsewhere)
                const int32_t b = ri(0, 3) == 0 ? ri(-1, 3) : static_cast<int32_t>(ee_read<uint32_t>(pv));
                const uint32_t box = ee_read<uint32_t>(OV(0x001600ec)) + u(b << 7);
                if (b >= 0) for (uint32_t k = 0; k < 3; ++k) wf(0x0013f3d0 + 4 * k, ee_read<float>(box + 0x30 + 4 * k) + rf(-0.5f, 0.5f));
            }
            if (ri(0, 3) == 0) w32(pv, u(ri(-1, 3)));
            if (ri(0, 2) == 0) wf(0x0013f3d0 + 8, rf(-50, 500));
            return Args{{m}, {}};
        }, [](A a) { spawn_point_update_l1(a.i[0]); return uint64_t{0}; });
}

// ---- the scripted camera's nodes (novalis_mobys.cpp, novalis_npcs.cpp): the hero block's list (the engine's 0x16c860, +0x178
// its mobys, +0x44 their count) filled with live mobys, some of the classes the function marks
void node_list(uint32_t& m, std::initializer_list<int> classes) {
    level();
    const std::vector<uint32_t> live = live_mobys();
    m = pick(live);
    const uint32_t list = OV(0x0016c860);
    const int32_t n = ri(0, 3) == 0 ? ri(-1, 0) : ri(1, 6);
    w16(list + 0x44, u(n));
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t o = pick(live);
        if (ri(0, 2) != 0) {
            const int cls = classes.begin()[ri(0, static_cast<int>(classes.size()) - 1)];
            w16(o + 0xa6, cls < 0 ? ee_read<uint16_t>(m + 0xa6) : u(cls));
        }
        w32(list + 0x178 + u(i) * 4, o);
    }
    w32(OV(0x0015f5c4), ri(0, 4) == 0 ? u(ri(0, 3)) : 2u);
    w8(0x0015edb0, u(ri(0, 4) == 0 ? 0 : 1));
    w8(OV(0x0017c440) + 1, u(ri(0, 3) == 0 ? 1 : 0));
}

void camera_node_checks() {
    each_copy("scripted_camera_nodes", {{1, 0x002fb4b8}, {2, 0x002dfbd8}, {3, 0x002cf248}, {3, 0x002dc380}, {5, 0x003179a8}, {7, 0x002f60c8},
                                        {7, 0x00300878}, {10, 0x002c7c00}, {13, 0x002edaf0}, {13, 0x002f8d78}}, [](uint32_t a) {
        fcheck("scripted_camera_nodes", a, 40, Ret::None, [] {
            uint32_t m = 0;
            for (int k = 0; k < 40; ++k) {
                node_list(m, {0x398, 0x1bf});
                if (trial_ok([] { scripted_camera_nodes_l1(); })) break;
            }
            return Args{{}, {}};
        }, [](A) { scripted_camera_nodes_l1(); return uint64_t{0}; });
    });
    each_copy("plumber_camera_nodes", {{1, 0x002ff028}, {8, 0x002f8fd8}}, [](uint32_t a) {
        fcheck("plumber_camera_nodes", a, 40, Ret::None, [] {
            uint32_t m = 0;
            for (int k = 0; k < 40; ++k) {
                node_list(m, {-1, 0x32b});
                if (trial_ok([m] { plumber_camera_nodes_l1(m); })) break;
            }
            return Args{{m}, {}};
        }, [](A a) { plumber_camera_nodes_l1(a.i[0]); return uint64_t{0}; });
    });
}

// ---- small moby helpers (novalis_mobys.cpp, novalis_npcs.cpp, novalis_enemies.cpp, novalis_hive.cpp)
void moby_helper_checks() {
    each_copy("plant_bolts_drop", {{1, 0x002787a0}, {4, 0x00256460}, {5, 0x0028dd80}, {6, 0x00272a68}, {7, 0x0028c398}, {8, 0x0026db60},
                                   {9, 0x00282b10}, {10, 0x00256e20}, {11, 0x00288fc0}, {12, 0x0027c2c8}, {13, 0x0026f828}, {14, 0x0026b788},
                                   {15, 0x00253268}, {16, 0x0025eef0}, {17, 0x0025d9d0}}, [](uint32_t a) {
        fcheck("plant_bolts_drop", a, 40, Ret::None, [] {
            level();
            const uint32_t m = pick(live_mobys());
            hero_around(ee_ref<Vec4>(m + 0x10), coin() ? 0.0f : 6.5f, coin() ? 7.5f : 20.0f);
            if (ri(0, 4) == 0) wf(0x0015ed60, rf(0.5f, 3.0f));
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { plant_bolts_drop_l1(a.i[0]); return uint64_t{0}; });
    });
    each_copy("moby_clone", {{1, 0x00278e20}, {4, 0x00256ab0}, {8, 0x0026e1b0}, {9, 0x00283160}, {10, 0x00257470}, {11, 0x00289610},
                             {12, 0x0027c918}, {17, 0x0025e020}}, [](uint32_t a) {
        fcheck("moby_clone", a, 40, Ret::None, [] {
            level();
            const std::vector<uint32_t> live = live_mobys();
            const uint32_t m = pick(live);
            if (coin()) wf(m + 0x2c, rf(0.1f, 3.0f));
            wrand(m + 0x38, 2);
            return Args{{m, ee_read<uint16_t>(pick(live) + 0xa6)}, {}};
        }, [](A a) { moby_clone_l1(a.i[0], a.i[1]); return uint64_t{0}; });
    });
    each_copy("moby_on_screen", {{1, 0x00275690}, {3, 0x0024eec0}, {4, 0x002534a8}, {5, 0x0028a858}, {7, 0x00288e20}, {11, 0x00285d90},
                                 {12, 0x00279238}, {13, 0x0026c818}, {14, 0x00268b50}}, [](uint32_t a) {
        fcheck("moby_on_screen", a, 60, Ret::Int64, [] {
            level();
            const uint32_t m = pick(live_mobys());
            const float r = ri(0, 3) == 0 ? 0.0f : rf(0.0f, 30.0f);
            if (ri(0, 3) != 0) {  // its first quad (the bounding position x 1024) about the camera: seen (1), beside (0), behind (-1)
                const int64_t want = ri(-1, 1);
                Vec4 c{};
                for (int k = 0; k < 4000; ++k) {
                    c = near(ee_ref<Vec4>(OV(0x00166dc0)), k < 2000 ? 30.0f : 120.0f, k < 2000 ? 10.0f : 40.0f);
                    c.w = ri(0, 4) == 0 ? rf(-1.0f, 1.0f) : 1.0f;
                    if (point_on_screen_v(c, r) == want) break;
                }
                wf(m, c.x * 1024.0f); wf(m + 4, c.y * 1024.0f); wf(m + 8, c.z * 1024.0f); wf(m + 0xc, c.w * 1024.0f);
            }
            return Args{{m}, {r}};
        }, [](A a) {
            const int64_t r = moby_on_screen_l1(a.i[0], a.f[0]);
            if (std::getenv("B501_INFO") != nullptr) std::printf("INFO moby_on_screen %lld\n", static_cast<long long>(r));
            return static_cast<uint64_t>(r);
        });
    });
    each_copy("moby_data_note", {{1, 0x0029b0a0}, {3, 0x00273f50}, {4, 0x00278610}, {5, 0x002af5e8}, {8, 0x0028fa30}}, [](uint32_t a) {
        fcheck("moby_data_note", a, 60, Ret::None, [] {
            level();
            const uint32_t m = pick(live_mobys());
            const uint32_t list = OUT + 0x100;
            const int32_t n = ri(0, 3);
            w32(list, u(n));
            const uint32_t at = ri(0, 1) ? m + 0x20 : ee_read<uint32_t>(m + 0x78) + 0x44;
            const int32_t kind = ri(0, 1);
            const int16_t off = static_cast<int16_t>(kind == 1 ? at - m : at - (ee_read<uint32_t>(m + 0x78) & 0xffff));
            const int32_t size = ri(0, 3) == 0 ? 1 : ri(1, 12);
            for (int32_t i = 0; i < 4; ++i) {
                const uint32_t e = list + 4 + 0x10 * u(i);
                w32(e, ri(0, 1) ? u(static_cast<int32_t>(off)) : R()); w16(e + 8, ri(0, 1) ? u(size) : u(ri(0, 3)));
                w16(e + 0xa, u(ri(0, 1))); w16(e + 0xc, ri(0, 1) ? ee_read<uint16_t>(m + 0xb2) : u(ri(0, 99)));
                w16(e + 0xe, ri(0, 1) ? ee_read<uint16_t>(m + 0xa6) : u(ri(0, 99)));
            }
            return Args{{at, u(size), m, u(kind), list}, {}};
        }, [](A a) { moby_data_note_l1(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2], static_cast<int32_t>(a.i[3]), a.i[4]); return uint64_t{0}; });
    });
    each_copy("hero_walk_to", {{1, 0x002783a8}, {2, 0x00264d28}, {3, 0x00251988}, {5, 0x0028d960}, {6, 0x00272670}, {8, 0x0026d768},
                               {11, 0x00288e98}, {12, 0x0027bea8}, {14, 0x0026b390}, {16, 0x0025e948}}, [](uint32_t a) {
        fcheck("hero_walk_to", a, 40, Ret::None, [] {
            level();
            const uint32_t m = pick(live_mobys());
            wf(m + 0x48, rf(-3.1f, 3.1f));
            wrand(OV(0x0016c860) + 0x10, 12);
            return Args{{m}, {ri(0, 4) == 0 ? 0.0f : rf(-2.0f, 5.0f)}};
        }, [](A a) { hero_walk_to_l1(a.f[0], a.i[0]); return uint64_t{0}; });
    });
    each_copy("walker_record_init", {{1, 0x0026d930}, {2, 0x0025b1c0}, {3, 0x002476c0}, {4, 0x0024b888}, {5, 0x00282db8}, {6, 0x00267810},
                                     {8, 0x00263050}, {9, 0x002785a0}, {10, 0x0024b5c8}, {11, 0x0027e980}, {12, 0x00271890}, {13, 0x002648a0},
                                     {14, 0x00261050}, {15, 0x00248318}, {16, 0x00254228}}, [](uint32_t a) {
        fcheck("walker_record_init", a, 6, Ret::None, [] {
            level();
            wrand(OUT, 16);
            return Args{{OUT}, {}};
        }, [](A a) { walker_record_init_l1(a.i[0]); return uint64_t{0}; });
    });
    each_copy("cooldown_slot_claim", {{1, 0x002efbf8}, {13, 0x002b4ec0}}, [](uint32_t a) {
        fcheck("cooldown_slot_claim", a, 60, Ret::Int32, [] {
            level();
            const uint32_t arr = OUT;
            const int32_t now = ee_read<int32_t>(OV(0x0015f5cc));
            for (uint32_t i = 0; i < 6; ++i) w32(arr + 4 * i, u(now + ri(-3, 3)));
            return Args{{arr, u(ri(-1, 6)), u(ri(-5, 100))}, {}};
        }, [](A a) { return ri32(cooldown_slot_claim_l1(a.i[0], static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]))); });
    });

    std::vector<uint32_t> groups;
    auto need_groups = [&groups](const char* name) {
        if (!groups.empty() || !selected(name)) return;
        level();
        groups = level_groups();
        std::printf("  (moby groups: %zu usable on this level)\n", groups.size());
    };
    const std::initializer_list<LvAddr> bc = {{1, 0x0026e090}, {3, 0x00247da0}, {7, 0x00281be0}, {9, 0x00278d00}, {12, 0x00272080},
                                              {13, 0x00265000}, {15, 0x00248a78}, {16, 0x00254988}};
    if (!copies_here(bc).empty()) need_groups("moby_group_set_bc");
    each_copy("moby_group_set_bc", bc, [&groups](uint32_t a) {
        fcheck("moby_group_set_bc", a, 40, Ret::None, [groups] {
            level();
            return Args{{pick(groups), R() & 0xffu}, {}};
        }, [](A a) { moby_group_set_bc_l1(a.i[0], a.i[1]); return uint64_t{0}; });
    });
    const std::initializer_list<LvAddr> cnt = {{1, 0x0026e008}, {6, 0x00267ee8}, {9, 0x00278c78}, {11, 0x0027f058}, {12, 0x00271ff8},
                                               {13, 0x00264f78}, {14, 0x00261728}, {15, 0x002489f0}, {16, 0x00254900}};
    if (!copies_here(cnt).empty()) need_groups("moby_group_count");
    each_copy("moby_group_count", cnt, [&groups](uint32_t a) {
        fcheck("moby_group_count", a, 60, Ret::Int32, [groups] {
            level();
            const uint32_t g = pick(groups);
            const uint32_t p = ee_read<uint32_t>(OV(0x001ab840) + g * 4);
            if (p != 0) {  // some of its mobys dead (negative states) or in other states
                for (uint32_t k = 0; k < 256; ++k) {
                    const uint32_t id = ee_read<uint16_t>(p + 2 * k);
                    if (ri(0, 2) == 0) w8(first_moby() + ((id & 0x7fffu) << 8) + 0x20, coin() ? u(ri(0x80, 0xff)) : u(ri(0, 6)));
                    if ((id & 0x8000u) != 0) break;
                }
            }
            return Args{{g, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 6))}, {}};
        }, [](A a) { return ri32(moby_group_count_l1(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]))); });
    });
}

// ---- paths (novalis_enemies.cpp, novalis_path.cpp) and the debug particle
void path_checks() {
    const std::initializer_list<LvAddr> nxt = {{1, 0x0028b5e0}, {9, 0x00294cb0}, {13, 0x00282010}, {15, 0x00265b38}};
    each_copy("path_next_point", nxt, [](uint32_t a) {
        fcheck("path_next_point", a, 80, Ret::Int32, [] {
            level();
            const uint32_t m = pick(live_mobys());
            const uint32_t path = OUT, n = u(ri(0, 5));
            w32(path, n);
            const Vec4 base = ee_ref<Vec4>(m + 0x10);
            for (uint32_t i = 0; i < 6; ++i) ee_ref<Vec4>(path + 0x10 + 0x10 * i) = near(base, 20.0f, 3.0f);
            ee_ref<Vec4>(OUT + 0x200) = near(base, 30.0f, 0.0f);
            wrand(OUT + 0x300, 4);
            return Args{{m, path, OUT + 0x200, OUT + 0x300}, {}};
        }, [](A a) { return ri32(path_next_point_l1(a.i[0], a.i[1], a.i[2], a.i[3])); });
    });

    const std::initializer_list<LvAddr> fi = {{1, 0x0028b410}, {3, 0x00264458}, {4, 0x00268b18}, {12, 0x0028eb00}, {14, 0x0027df78}};
    const std::initializer_list<LvAddr> nr = {{1, 0x0028b510}, {3, 0x00264558}, {4, 0x00268c18}, {5, 0x002a0260}, {7, 0x0029e610}, {8, 0x002806a8},
                                              {9, 0x00294a10}, {12, 0x0028ec00}, {13, 0x00281d70}, {14, 0x0027e078}, {16, 0x00271250}};
    std::vector<uint32_t> paths;
    if ((!copies_here(fi).empty() && selected("path_follower_init")) || (!copies_here(nr).empty() && selected("path_nearest"))) {
        level();
        paths = level_paths();
        std::printf("  (paths: %zu on this level)\n", paths.size());
    }
    each_copy("path_follower_init", fi, [&paths](uint32_t a) {
        if (paths.empty()) { std::printf("%-26s no path on this level\n", "path_follower_init"); return; }
        fcheck("path_follower_init", a, 40, Ret::None, [paths] {
            level();
            const uint32_t f = OUT;
            wrand(f, 8);
            w32(f + 0x14, pick(paths));
            if (coin()) {  // the path's segment fields perturbed
                const uint32_t p = path_at(ee_read<uint32_t>(f + 0x14));
                for (int32_t i = 0; i < ee_read<int32_t>(p); ++i) wf(p + 0x18 + u(i) * 0x10, rf(-10.0f, 10.0f));
            }
            return Args{{f}, {}};
        }, [](A a) { path_follower_init_l1(a.i[0]); return uint64_t{0}; });
    });
    each_copy("path_nearest", nr, [&paths](uint32_t a) {
        fcheck("path_nearest", a, 60, Ret::Int32, [paths] {
            level();
            uint32_t path = OUT + 0x100;
            if (!paths.empty() && coin()) {
                path = path_at(pick(paths));
            } else {  // a path of the test area (0..6 points)
                const int32_t n = ri(0, 6);
                w32(path, u(n));
                const Vec4 c = ee_ref<Vec4>(0x0013f3d0);
                for (int32_t i = 0; i < 6; ++i) ee_ref<Vec4>(path + 0x10 + u(i) * 0x10) = near(c, 30.0f, 5.0f);
            }
            const int32_t cnt = ee_read<int32_t>(path);
            ee_ref<Vec4>(OUT) = near(cnt > 0 ? ee_ref<Vec4>(path + 0x10 + u(ri(0, cnt - 1)) * 0x10) : ee_ref<Vec4>(0x0013f3d0), 6.0f, 3.0f);
            return Args{{OUT, path}, {ri(0, 2) == 0 ? 0.0f : rf(-5.0f, 40.0f)}};
        }, [](A a) { return ri32(path_nearest_l1(a.i[0], a.i[1], a.f[0])); });
    });

    each_copy("debug_particle", {{1, 0x00285768}, {2, 0x00271d80}, {3, 0x0025e9e0}, {4, 0x00263098}, {6, 0x0027f648}, {10, 0x002634e8},
                                 {14, 0x00278390}}, [](uint32_t a) {
        fcheck("debug_particle", a, 40, Ret::Int32, [] {
            level();
            ee_ref<Vec4>(OUT) = near(ee_ref<Vec4>(0x0013f3d0), 10.0f, 3.0f);
            return Args{{OUT, u(ri(0, 5)), u(ri(0, 20)), R(), R(), coin() ? 0xffffffffu : R(), ri(0, 1) ? 0xffu : R()}, {rf(0, 2)}};
        }, [](A a) {
            return ri32(static_cast<int32_t>(debug_particle_l1(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], static_cast<int32_t>(a.i[5]), a.i[6], a.f[0])));
        });
    });
}

// ---- the hero's special mode (hero_l1_state.cpp, hero_l1_misc.cpp; levels 1 and 2)
void hero_mode_checks() {
    const uint32_t P = 0x0013f350;
    each_copy("hero_mode_exit", {{1, 0x00231450}, {2, 0x0021fe40}}, [P](uint32_t a) {
        fcheck("hero_mode_exit", a, 40, Ret::None, [P] {
            level();
            const std::vector<uint32_t> live = live_mobys();
            w32(P + 0xa84, ri(0, 2) == 0 ? 0u : pick(live));
            w32(P + 0xa8c, ri(0, 2) == 0 ? 0u : pick(live));
            w32(ee_read<uint32_t>(P + 0xa88) + 0x98, R());
            w8(P + 0x20a4, u(ri(0, 3)));
            w32(P + 0x2084, ri(0, 2) == 0 ? 0x64u : u(ri(0, 0x80)));
            w32(OV(0x0015f5c4), coin() ? (coin() ? 2u : 6u) : u(ri(0, 7)));
            return Args{{}, {}};
        }, [](A) { hero_mode_exit_l1(); return uint64_t{0}; });
    });
    each_copy("hero_enter_idle", {{1, 0x002405a0}, {2, 0x0022ea90}}, [P](uint32_t a) {
        fcheck("hero_enter_idle", a, 40, Ret::None, [P] {
            level();
            w8(P + 0x20a4, u(ri(0, 4)));
            if (coin()) w32(P + 0x2084, u(ri(0, 0x80)));
            return Args{{}, {}};
        }, [](A) { hero_enter_idle_l1(); return uint64_t{0}; });
    });
}
}  // namespace

void batch_501() {
    if (g_only_batch != 501) return;
    g_batch = 501;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    // (the scratchpad words the pose function 0x245f18 spills the caller's $t8 / $t9 into: batch 120's)
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // 989snd's IOP server (as batch 506's): replies a function of the command
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
    // the debris / spawns leave w lanes of a VU0 sin / cos in the original (the port has none); the particle pool's w lanes
    // (batch 506's)
    for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) {
        g_ignored_words.push_back(a + 0xc);
        g_ignored_words.push_back(a + 0x1c);
    }
    {
        const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
        for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c);
    }
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

    water_checks();
    wall_path_checks();
    particle34_checks();
    spawn_point_checks();
    camera_node_checks();
    moby_helper_checks();
    path_checks();
    hero_mode_checks();

    g_pre_call = nullptr;
    g_extra_noop_hooks.clear();
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
