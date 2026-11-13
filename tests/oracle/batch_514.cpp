// Oracle batch 514 (stage 4 task F8, #149): class code several levels carry a copy of, each family ported once as shared
// hand-written code (src/game/ovl/shared_*.cpp), against every level's own copy at that level's own address. The level copies
// stay transliterated in their units until each level is re-glued; here every copy is the original and the one shared port
// the port:
//  - shared_class28.cpp: class 28, a path-riding turret, its update and its sixteen helpers (levels 14, 15);
//  - shared_class156.cpp: class 156, a shot that bursts on what it meets, its burst and its trail's particle (levels 1, 6, 8,
//    9, 15, 17), on shots spawned for the check;
//  - shared_class193.cpp: class 193, a walking creature that charges and strikes (levels 9, 15);
//  - shared_gold_bolt.cpp: class 1134, the gold bolt, its update and helpers, and the glitter's pvars set up (the gold bolt's
//    build and the other one; levels 1 .. 18);
//  - shared_sandmouse.cpp: class 1818, the Sandmouse, its update and helpers, the shot's spawn (class 1633), and the point
//    ahead of the hero other classes' copies use too (levels 1 .. 6, 8, 11, 12, 14);
// Subjects: the level's own mobys of the class after the first gameplay frame (the port's; the screen, the projection and the
// view set up), perturbed through every state; the helpers on the class's mobys with arguments built from the level's state;
// the frame scale (0x15ed68) at its 50 Hz value in a quarter of the cases. No RAM dumps; no broad list of left-out words.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 514, N = each level a family names (1 .. 18); a family the level does
// not carry says so and is not run.

#include "ovl_oracle.h"

#include <game/lib/libcdvd.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class156.h>
#include <game/ovl/shared_class193.h>
#include <game/ovl/shared_class28.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_sandmouse.h>
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
        if (g_oracle_level == 14) w8(0x0013d3f0, 1);  // (Oltanis: the arrival scene seen, as batch 221's)
        gameplay_frame();
        // the screen and projection constants (a 640 x 448 screen, as the game's) and the view's matrices (batch 509's)
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
uint32_t moby_index(uint32_t m) { return (m - first_moby()) >> 8; }

// the level's live mobys whose update function is `fn` (the level's address)
std::vector<uint32_t> mobys_with(uint32_t fn) {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < end_moby(); m += 0x100)
        if (live(m) && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
    return v;
}
// the level's live mobys (with a class and pvars)
std::vector<uint32_t> live_mobys() {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < end_moby() && v.size() < 400; m += 0x100)
        if (live(m) && ee_read<uint32_t>(m + 0x24) != 0 && ee_read<uint32_t>(m + 0x78) != 0) v.push_back(m);
    return v;
}

uint32_t pick(const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; }

// a hit on the moby: its slot in the hit table (0x178100, 64-byte entries) naming it, with a damage mask (the `kinds` bits
// mixed with random ones), a damage at +0x2c and its source at +0x20 (one of `sources`, or none; batch 509's)
void give_hit(uint32_t m, uint32_t kinds, const std::vector<uint32_t>& sources = {}) {
    if (ri(0, 3) == 0) { w8(m + 0xa4, 0xff); return; }
    const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
    w8(m + 0xa4, idx);
    w32(e + 0x34, ri(0, 8) == 0 ? first_moby() : m);
    w32(e + 0x24, ri(0, 2) == 0 ? (R() & ~kinds) : (kinds | (R() & 0xff)));
    { static const float dm[] = {-1.0f, 0.0f, 0.5f, 5.0f, 12.0f}; wf(e + 0x2c, dm[ri(0, 4)]); }
    wfrand(e, 4, -1, 1);
    w32(e + 0x28, u(ri(0, 20)));
    const int k = ri(0, 3);
    uint32_t src = 0;
    if (k == 1) src = ee_read<uint32_t>(0x001413d0);
    else if (k == 2 && !sources.empty()) src = pick(sources);
    else if (k == 3) src = first_moby() + 0x100 * u(ri(0, 40));
    w32(e + 0x20, src);
}

// a point `r0`..`r1` from `at` (horizontally), `z0`..`z1` above it
Vec4 around(const Vec4& at, float r0, float r1, float z0 = -1.0f, float z1 = 2.0f) {
    const float r = rf(r0, r1), a = rf(-3.1f, 3.1f);
    return Vec4{at.x + r * std::cos(a), at.y + r * std::sin(a), at.z + rf(z0, z1), at.w};
}

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
    try { port(); } catch (const std::exception&) { ok = false; }
    ee_stack_ptr() = stack_top;
    std::memcpy(ram, saved.data(), saved.size());
    return ok;
}

// An update on one of the subjects (dealt again when the port's run throws, as batch 509's classcheck)
void classcheck(const char* name, uint32_t laddr, const std::function<std::vector<uint32_t>()>& subjects, void (*port)(uint32_t),
                const std::function<void(uint32_t)>& vary, int n) {
    if (!selected(name)) return;
    level();
    const std::vector<uint32_t> probe = subjects();
    if (probe.empty()) { std::printf("%-26s L%d:0x%08x  no subject on this level\n", name, g_oracle_level, laddr); return; }
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

// a check of a helper on the generator's arguments, dealt again while the port's run throws
void helpercheck(const char* name, uint32_t laddr, int n, Ret ret, const std::function<Args()>& deal,
                 const std::function<uint64_t(const Args&)>& port) {
    if (!selected(name)) return;
    fcheck(name, laddr, n, ret, [deal, port] {
        Args a;
        for (int attempt = 0; attempt < 40; ++attempt) {
            level();
            a = deal();
            if (trial([&port, &a] { port(a); })) break;
        }
        return a;
    }, port);
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
uint32_t path_ptr(int32_t idx) { return ee_read<uint32_t>(OV(0x001b04b0) + u(idx) * 4); }

// the groups (0x1ab840: a list of moby indices per group, the last with bit 15) holding a moby of class `cls`
std::vector<uint32_t> groups_with(uint32_t cls) {
    std::vector<uint32_t> v;
    for (uint32_t g = 0; g < 0x100; ++g) {
        uint32_t e = ee_read<uint32_t>(OV(0x001ab840) + g * 4);
        if (e < 0x00100000u || e >= 0x02000000u) continue;
        for (int k = 0; k < 64; ++k, e += 2) {
            const uint16_t h = ee_read<uint16_t>(e);
            const uint32_t m = first_moby() + ((h & 0x7fffu) << 8);
            if (m < end_moby() && ee_read<uint16_t>(m + 0xa6) == cls) { v.push_back(g); break; }
            if ((h & 0x8000u) != 0) break;
        }
    }
    return v;
}

// the damage the moby's hit (the hit table's, masked by `kinds`) would deal it (moby_hit_react_r on a copy of the state)
float hit_damage(uint32_t m, uint32_t kinds) {
    static std::vector<uint8_t> saved;
    uint8_t* ram = ee_get_ram_base();
    saved.assign(ram, ram + 0x02000000);
    float d = 0.0f;
    const uint32_t stack_top = ee_stack_ptr();
    try {
        const uint32_t hit = sub_0025a420(m, kinds, 0);
        int32_t st = 0;
        moby_hit_react_r(m, hit, pvar(m) + 0x20, 0, st, &d, nullptr, 4);
    } catch (const std::exception&) {
    }
    ee_stack_ptr() = stack_top;
    std::memcpy(ram, saved.data(), saved.size());
    return d;
}

// the zones of the level's zone list (*0x15f70c, 32-byte entries, +0x10 a path) whose path is a path of 1 .. 200 points
std::vector<uint32_t> zones_with_paths() {
    std::vector<uint32_t> v;
    const uint32_t list = ee_read<uint32_t>(OV(0x0015f70c));
    if (list < 0x00100000u || list >= 0x02000000u) return v;
    for (uint32_t z = 0; z < 128; ++z) {
        const uint32_t p = ee_read<uint32_t>(list + (z << 5) + 0x10);
        if (p >= 0x00100000u && p < 0x01ff0000u && ee_read<int32_t>(p) >= 1 && ee_read<int32_t>(p) <= 200) v.push_back(z);
    }
    return v;
}

// a check run with the words `words` left out besides the batch's (one check's known difference of existing code)
void without(const std::vector<uint32_t>& words, const std::function<void()>& run) {
    const std::vector<uint32_t> saved = g_ignored_words;
    for (uint32_t w : words) g_ignored_words.push_back(w);
    run();
    g_ignored_words = saved;
}

// ---- class 193 (shared_class193.cpp)
void class193_checks() {
    const uint32_t upd = copy_here({{9, 0x002e27d8}, {15, 0x002bc608}});
    if (upd == 0) {
        not_here("class193_update");
        return;
    }
    auto walkers = [upd] { return mobys_with(upd); };
    if (std::getenv("B514_INFO") != nullptr) {
        level();
        std::printf("INFO class 193: %zu placed\n", walkers().size());
        for (uint32_t m : walkers())
            std::printf("INFO   0x%08x state %u group %u path %d\n", m, ee_read<uint8_t>(m + 0x20), ee_read<uint8_t>(m + 0x21),
                        ee_read<int32_t>(pvar(m) + 0x21c));
    }
    auto vary = [walkers](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        // (a hit, some from a moby of class 0xb0 / 0xb1, some by the weapon 0xb4; the hit timer about its 45 frames)
        give_hit(m, 0x330000u, walkers());
        if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
            const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
            if (coin()) {
                const uint32_t src = pick(live_mobys());
                w32(e + 0x20, src);
                if (coin()) w16(src + 0xa6, coin() ? 0xb0u : 0xb1u);
            }
            if (coin()) w16(e + 0x2a, 0xb4);
        }
        w16(pv + 0x26, ri(0, 2) == 0 ? 0u : u(ri(0, 70)));
        if (coin()) wf(pv + 0x20, ri(0, 2) == 0 ? rf(0.1f, 3.0f) : (coin() ? 0.5f : 1.0f));
        w8(m + 0x31, u(ri(0, 1)));
        if (ri(0, 3) == 0) ee_ref<Vec4>(OV(0x00166dc0)) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 30.0f : 60.0f);
        // (the hero: near (a target in sight), in range or out of the 40)
        const int hk = ri(0, 3);
        if (hk != 0) ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), hk == 1 ? 0.0f : 3.0f, hk == 1 ? 3.0f : (hk == 2 ? 25.0f : 60.0f), -1.0f, 2.0f);
        w32(pv + 0x228, ri(0, 5) == 0 ? 1u : 0u);
        w8(0x001413f4, ri(0, 3) == 0 ? 2u : 0u);
        w32(pv + 0x38, ri(0, 3) == 0 ? 1u : 0u);
        for (uint32_t k : {0x208u, 0x210u, 0x224u}) w32(pv + k, ri(0, 2) == 0 ? u(ri(0, 2)) : u(ri(0, 60)));
        w16(pv + 0x214, ri(0, 2) == 0 ? u(ri(0, 2)) : u(ri(0, 60)));
        if (coin()) w32(pv + 0x21c, ri(0, 2) == 0 ? 0xffffffffu : u(a_path()));
        if (coin()) ee_ref<Vec4>(pv + 0x1d0) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 2.0f : 10.0f, -0.5f, 0.5f);
        if (coin()) w8(m + 0x21, coin() ? 0xffu : u(ri(0, 40)));
        w8(m + 0xbc, ri(0, 2) == 0 ? 1u : u(ri(0, 2)));
        w8(m + 0x53, u(ri(0, 4)));
        w8(m + 0x52, coin() ? ee_read<uint8_t>(m + 0x53) : u(ri(0, 4)));
        w8(m + 0x70, coin() ? (R() & 0xff) : 2u);
        if (coin()) {  // (the animation's time: at 13 (the blow), past 29 (up again) or about)
            wf(m + 0x54, 0.0f);
            const uint32_t cur = ee_read<uint8_t>(m + 0x52) == 0xff ? ee_read<uint32_t>(m + 0x6c) : ee_read<uint32_t>(m + 0x68);
            const int k = ri(0, 2);
            if (cur >= 0x00100000u && cur < 0x02000000u) w16(cur + 4, k == 0 ? 13u * 16u : (k == 1 ? u(ri(29, 40)) * 16u : u(ri(0, 600))));
        }
        if (coin()) wf(pv + 0x190, coin() ? 0.0f : rf(0.0f, 0.5f));
        if (coin()) wf(pv + 0x198, rf(-1.0f, 0.5f));
        if (coin()) wf(pv + 0x1f8, rf(-40.0f, 40.0f));
        if (coin()) wf(pv + 0x204, rf(1.0f, 8.0f));
        if (coin()) {  // (an anchor moby and the reach about it)
            w32(pv + 0x218, coin() ? 0u : pick(live_mobys()));
            w16(pv + 0x216, u(ri(1, 20)));
        }
        if (coin()) {  // (another of its class close by)
            const std::vector<uint32_t> ws = walkers();
            const uint32_t o = pick(ws);
            if (o != m) ee_ref<Vec4>(o + 0x10) = around(ee_ref<Vec4>(m + 0x10), 0.5f, 2.5f, -0.5f, 0.5f);
        }
        if (coin()) wf(m + 0x48, rf(-3.1f, 3.1f));
        if (st == 0x63 && coin()) wf(m + 0x18, ee_read<float>(m + 0x18) + rf(-1.0f, 3.0f));
        // (striking: half the time the blow's moment: its animation shown at frame 13, the hero close before it, level)
        if (st == 4 && coin()) {
            w8(m + 0x52, ee_read<uint8_t>(m + 0x53));
            wf(m + 0x54, 0.0f);
            const uint32_t cur = ee_read<uint8_t>(m + 0x52) == 0xff ? ee_read<uint32_t>(m + 0x6c) : ee_read<uint32_t>(m + 0x68);
            if (cur >= 0x00100000u && cur < 0x02000000u) w16(cur + 4, 13u * 16u);
            const float a = ee_read<float>(m + 0x48) + rf(-0.2f, 0.2f), r = rf(0.3f, 2.2f);
            const Vec4 p = ee_ref<Vec4>(m + 0x10);
            ee_ref<Vec4>(0x0013f3d0) = Vec4{p.x + r * std::cos(a), p.y + r * std::sin(a), p.z + rf(-0.6f, 0.6f), p.w};
        }
        // (backing off: half the time the hero steeply above or below it, close)
        if (st == 12 && coin()) {
            const Vec4 p = ee_ref<Vec4>(m + 0x10);
            const float a = rf(-3.1f, 3.1f), r = rf(0.2f, 3.0f);
            ee_ref<Vec4>(0x0013f3d0) = Vec4{p.x + r * std::cos(a), p.y + r * std::sin(a), p.z + (coin() ? 1.0f : -1.0f) * rf(r, r + 5.0f), p.w};
            w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u);
        }
        vary_frame_scale();
        w8(m + 0x20, u(st));
        seed_rand();
    };
    // (the segment records 0x173e90 .. 0x173ebc the walker's collision leaves from VU0 registers in the original, which its
    // hand port does not reproduce (as batches 240 / 242 / 372): left out of the update's check)
    std::vector<uint32_t> seg;
    for (uint32_t k = 0; k < 0x30; k += 4) seg.push_back(OV(0x00173e90) + k);
    const int only_st = std::getenv("B514_ST") != nullptr ? std::atoi(std::getenv("B514_ST")) : -1;
    without(seg, [&] {
        for (int st : {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0x63, 20})
            if (only_st < 0 || only_st == st)
                classcheck("class193_update", upd, walkers, class193_update, [st, vary](uint32_t m) {
                    if (st != 0 && coin()) {
                        w8(m + 0x20, 0);
                        const uint32_t sp0 = ee_stack_ptr();
                        try { class193_update(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
                    }
                    vary(m, st);
                }, st == 1 || st == 2 || st == 8 || st == 10 || st == 11 ? 120 : (st == 3 || st == 4 || st == 5 || st == 12 ? 100 : 40));
    });
}

// ---- class 156 (shared_class156.cpp)
void class156_checks() {
    const uint32_t upd = copy_here({{1, 0x002c7530}, {6, 0x002bead0}, {8, 0x002ba8d0}, {9, 0x002d0d88}, {15, 0x002aabb8}, {17, 0x002af608}});
    const uint32_t burst = copy_here({{1, 0x002c72c8}, {6, 0x002be868}, {8, 0x002ba668}, {9, 0x002d0b20}, {15, 0x002aa950}, {17, 0x002af3a0}});
    const uint32_t trail = copy_here({{1, 0x00280970}, {6, 0x0027a850}, {8, 0x00275bd8}, {9, 0x0028a428}, {15, 0x0025b050}, {17, 0x002653f8}});
    if (upd == 0) {
        for (const char* n : {"class156_update", "class156_burst", "class156_trail"}) not_here(n);
        return;
    }
    // shots spawned (three), each put by a live moby (or by the hero) and flying toward it, or away
    auto shots = [] {
        std::vector<uint32_t> v;
        const std::vector<uint32_t> all = live_mobys();
        for (int k = 0; k < 3; ++k) {
            const uint32_t s = moby_spawn(156);
            if (s == 0) break;
            const uint32_t near = k == 0 ? ee_read<uint32_t>(0x001413d0) : all[static_cast<size_t>(k * 37) % all.size()];
            ee_ref<Vec4>(s + 0x10) = around(ee_ref<Vec4>(near + 0x10), 0.0f, 2.0f, 0.0f, 1.5f);
            v.push_back(s);
        }
        return v;
    };
    auto vary = [](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        const float a = rf(-3.1f, 3.1f), speed = coin() ? rf(0.0f, 0.3f) : rf(0.3f, 2.0f);
        ee_ref<Vec4>(pv + 0x20) = Vec4{speed * std::cos(a), speed * std::sin(a), rf(-0.2f, 0.2f), 0.0f};
        w32(pv, ri(0, 2) == 0 ? u(ri(0, 2)) : u(ri(0, 40)));
        wf(pv + 0xc, rf(-0.3f, 0.3f));
        w32(pv + 0x10, u(ri(0, 1)));
        w32(pv + 0x18, ri(0, 3) == 0 ? 0u : (coin() ? ee_read<uint32_t>(0x001413d0) : pick(live_mobys())));
        if (ri(0, 3) == 0) ee_ref<Vec4>(OV(0x00166dc0)) = around(ee_ref<Vec4>(m + 0x10), 1.0f, 20.0f);
        if (coin()) wf(m + 0x40, rf(-3.1f, 3.1f));
        vary_frame_scale();
        w8(m + 0x20, u(st));
        seed_rand();
    };
    for (int st = 0; st < 4; ++st)
        classcheck("class156_update", upd, shots, class156_update, [st, vary](uint32_t m) { vary(m, st); }, st == 1 ? 80 : 30);
    // the burst: the hit point by the hero (whom the camera looks at) or by the shot
    helpercheck("class156_burst", burst, 40, Ret::None, [shots] {
        const uint32_t m = pick(shots());
        ee_ref<Vec4>(OV(0x00173e60)) = around(coin() ? ee_ref<Vec4>(0x0013f3d0) : ee_ref<Vec4>(m + 0x10), 0.0f, 3.0f);
        vary_frame_scale();
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { class156_burst(a.i[0]); return uint64_t{0}; });
    helpercheck("class156_trail", trail, 40, Ret::Int32, [shots] {
        const uint32_t m = coin() ? pick(shots()) : pick(live_mobys());
        vary_frame_scale();
        seed_rand();
        return Args{{ri(0, 2) == 0 ? 0u : (coin() ? u(ri(1, 8)) : R()), m}, {}};
    }, [](A a) { return uint64_t{class156_trail(a.i[0], a.i[1])}; });
}

// ---- class 1134, the gold bolt (shared_gold_bolt.cpp)
void gold_bolt_checks() {
    const uint32_t upd = copy_here({{1, 0x00307ca0}, {2, 0x002ead38}, {3, 0x002dd188}, {4, 0x002e1f38}, {5, 0x00319c60}, {6, 0x003053a8},
                                    {7, 0x0031b5f0}, {8, 0x00303370}, {9, 0x003022f0}, {10, 0x002dd8e0}, {11, 0x0030b730}, {12, 0x002ffdb0},
                                    {13, 0x00303950}, {14, 0x002ffc18}, {15, 0x002e4770}, {16, 0x002df1e8}, {17, 0x002e3ff8}, {18, 0x002ecc40}});
    const uint32_t glitter = copy_here({{1, 0x00308550}, {2, 0x002eb5e8}, {3, 0x002dda38}, {4, 0x002e27e8}, {5, 0x0031a510}, {6, 0x00305c58},
                                        {7, 0x0031bea0}, {8, 0x00303c20}, {9, 0x00302ba0}, {10, 0x002de190}, {11, 0x0030bfe0}, {12, 0x00300660},
                                        {13, 0x00304200}, {14, 0x003004c8}, {15, 0x002e5020}, {16, 0x002dfa98}, {17, 0x002e48a8}, {18, 0x002ed4f0}});
    const uint32_t camera = copy_here({{1, 0x003087e0}, {2, 0x002eb878}, {3, 0x002ddcc8}, {4, 0x002e2a78}, {5, 0x0031a7a0}, {6, 0x00305ee8},
                                       {7, 0x0031c130}, {8, 0x00303eb0}, {9, 0x00302e30}, {10, 0x002de420}, {11, 0x0030c270}, {12, 0x003008f0},
                                       {13, 0x00304490}, {14, 0x00300758}, {15, 0x002e52b0}, {16, 0x002dfd28}, {17, 0x002e4b38}, {18, 0x002ed780}});
    const uint32_t sparkles = copy_here({{1, 0x003089f0}, {2, 0x002eba88}, {3, 0x002dded8}, {4, 0x002e2c88}, {5, 0x0031a9b0}, {6, 0x003060f8},
                                         {7, 0x0031c340}, {8, 0x003040c0}, {9, 0x00303040}, {10, 0x002de630}, {11, 0x0030c480}, {12, 0x00300b00},
                                         {13, 0x003046a0}, {14, 0x00300968}, {15, 0x002e54c0}, {16, 0x002dff38}, {17, 0x002e4d48}, {18, 0x002ed990}});
    const uint32_t spin = copy_here({{1, 0x00308380}, {2, 0x002eb418}, {3, 0x002dd868}, {4, 0x002e2618}, {5, 0x0031a340}, {6, 0x00305a88},
                                     {7, 0x0031bcd0}, {8, 0x00303a50}, {9, 0x003029d0}, {10, 0x002ddfc0}, {11, 0x0030be10}, {12, 0x00300490},
                                     {13, 0x00304030}, {14, 0x003002f8}, {15, 0x002e4e50}, {16, 0x002df8c8}, {17, 0x002e46d8}, {18, 0x002ed320}});
    const uint32_t init20 = copy_here({{1, 0x00308470}, {2, 0x002eb508}, {3, 0x002dd958}, {4, 0x002e2708}, {5, 0x0031a430}, {6, 0x00305b78},
                                       {7, 0x0031bdc0}, {8, 0x00303b40}, {9, 0x00302ac0}, {10, 0x002de0b0}, {11, 0x0030bf00}, {12, 0x00300580},
                                       {13, 0x00304120}, {14, 0x003003e8}, {15, 0x002e4f40}, {16, 0x002df9b8}, {17, 0x002e47c8}, {18, 0x002ed410}});
    const uint32_t init0 = copy_here({{2, 0x002ea7d0}, {6, 0x002f4bf8}, {15, 0x002eba70}, {17, 0x002f1488}});
    if (upd == 0) {
        for (const char* n : {"gold_bolt_update", "gold_bolt_glitter", "gold_bolt_camera", "gold_bolt_sparkles", "gold_bolt_spin",
                              "glitter_pvars_init", "glitter_pvars_init_0"})
            not_here(n);
        return;
    }
    // the gold bolts: the level's own (placed), or two spawned at the hero (bolt 0 and 1)
    auto bolts = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (!v.empty()) return v;
        for (int k = 0; k < 2; ++k) {
            const uint32_t s = moby_spawn(1134);
            if (s == 0) break;
            ee_ref<Vec4>(s + 0x10) = around(ee_ref<Vec4>(0x0013f3d0), 2.0f, 6.0f, 0.5f, 1.5f);
            w32(pvar(s), u(k));
            w32(pvar(s) + 4, 0xffffffffu);
            w32(pvar(s) + 8, 0xffffffffu);
            v.push_back(s);
        }
        return v;
    };
    if (std::getenv("B514_INFO") != nullptr) {
        level();
        std::printf("INFO gold bolt: %zu placed, camera list %08x\n", mobys_with(upd).size(), ee_read<uint32_t>(OV(0x001600ec)));
        for (uint32_t m : bolts())
            std::printf("INFO   0x%08x state %u bolt %d cam %d %d skip %d\n", m, ee_read<uint8_t>(m + 0x20), ee_read<int32_t>(pvar(m)),
                        ee_read<int32_t>(pvar(m) + 4), ee_read<int32_t>(pvar(m) + 8), ee_read<int16_t>(pvar(m) + 0xc));
    }
    // a bolt set up by its state 0 (the port's) and dealt: its index taken or not, the hero by it or not (on foot, in the
    // mode 3, or in another), its scene's camera path, timers and animation
    auto vary = [](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        const uint32_t P = 0x0013f350;
        if (coin()) w32(pv, ri(0, 5) == 0 ? 0xffffffffu : u(ri(0, 3)));
        if (ee_read<int32_t>(pv) >= 0 && ri(0, 3) == 0) w8(0x0014bec0 + ee_read<uint32_t>(pv) + (ee_read<uint32_t>(0x0015ed84) << 2), 1);
        if (coin()) ee_ref<Vec4>(P + 0x80) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 2.9f : 6.0f, -2.5f, 2.5f);
        w32(P + 0x22a8, ri(0, 3) == 0 ? 0u : 1u);
        w8(P + 0x20a4, ri(0, 2) == 0 ? 3u : (coin() ? 0u : u(ri(1, 5))));
        w16(pv + 0xc, ri(0, 3) == 0 ? 1u : 0u);
        if (coin()) {
            const uint32_t list = ee_read<uint32_t>(OV(0x001600ec));
            const bool path = list >= 0x00100000u && list < 0x02000000u && coin();
            w32(pv + 4, path ? u(ri(0, 3)) : 0xffffffffu);
            w32(pv + 8, path || coin() ? u(ri(0, 3)) : 0xffffffffu);
        }
        w32(pv + 0x6c, u(ri(0, 3)));
        w8(m + 0x52, u(ri(0, 1)));
        w8(m + 0x53, u(ri(0, 1)));
        w8(m + 0x70, R() & 0xff);
        if (coin()) {
            wf(m + 0x54, 0.0f);
            const uint32_t cur = ee_read<uint8_t>(m + 0x52) == 0xff ? ee_read<uint32_t>(m + 0x6c) : ee_read<uint32_t>(m + 0x68);
            if (cur >= 0x00100000u && cur < 0x02000000u) w16(cur + 4, u(ri(0, 3000)));
        }
        w32(P + 0xa98, R());
        w8(0x0015edb5, u(ri(0, 1)));
        // (the scene's camera moby (uid 5, at 0x166e00): its pvars in the test area, so that the camera's place and angles the
        // bolt hands it are compared; the level's own has none)
        if (ri(0, 3) != 0) {
            w32(OV(0x00166e00), M3);
            w16(M3 + 0x86, 5);
            w32(M3 + 0x70, T4);
        }
        vary_frame_scale();
        w8(m + 0x20, u(st));
        seed_rand();
    };
    auto set_up = [](uint32_t m) {
        w8(m + 0x20, 0);
        const uint32_t sp0 = ee_stack_ptr();
        try { gold_bolt_update(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
    };
    // the level change the taken bolt requests reads the console clock (and its dialogue stream polls the disc): the host's on
    // both sides, a fixed clock (as batch 119)
    const std::time_t saved_clock = g_host_clock_fixed;
    g_host_clock_fixed = 1759658400;
    const auto saved_hooks = g_extra_hooks;
    g_extra_hooks = {{0x00121780, [](EeCpu& c) { c.set32(2, cd_read_clock(c.r32(4))); }},
                     {0x0012d6d8, [](EeCpu& c) { clock_to_local(c.r32(4)); }},
                     {0x00121450, [](EeCpu& c) { c.set32(2, cd_read(c.r32(4), c.r32(5), c.r32(6), c.r32(7))); }}};
    for (int st = 0; st < 4; ++st)
        classcheck("gold_bolt_update", upd, bolts, gold_bolt_update, [st, vary, set_up](uint32_t m) {
            if (st != 0 && ri(0, 4) != 0) set_up(m);
            vary(m, st);
        }, st == 1 || st == 2 ? 60 : 30);
    g_extra_hooks = saved_hooks;
    g_host_clock_fixed = saved_clock;
    auto a_bolt = [bolts, vary, set_up] {
        const uint32_t m = pick(bolts());
        if (ri(0, 4) != 0) set_up(m);
        vary(m, ri(1, 2));
        return m;
    };
    helpercheck("gold_bolt_glitter", glitter, 60, Ret::None, [a_bolt] { return Args{{a_bolt()}, {}}; },
                [](A a) { gold_bolt_glitter(a.i[0]); return uint64_t{0}; });
    helpercheck("gold_bolt_camera", camera, 60, Ret::None, [a_bolt] { return Args{{a_bolt()}, {}}; },
                [](A a) { gold_bolt_camera(a.i[0]); return uint64_t{0}; });
    helpercheck("gold_bolt_sparkles", sparkles, 40, Ret::None, [a_bolt] { return Args{{a_bolt()}, {}}; },
                [](A a) { gold_bolt_sparkles(a.i[0]); return uint64_t{0}; });
    helpercheck("gold_bolt_spin", spin, 60, Ret::None, [a_bolt] { return Args{{a_bolt()}, {}}; },
                [](A a) { gold_bolt_spin(a.i[0]); return uint64_t{0}; });
    helpercheck("glitter_pvars_init", init20, 20, Ret::None, [a_bolt] {
        const uint32_t m = a_bolt();
        vary_frame_scale();
        return Args{{m}, {}};
    }, [](A a) { glitter_pvars_init(a.i[0], 0x20); return uint64_t{0}; });
    if (init0 != 0)
        helpercheck("glitter_pvars_init_0", init0, 20, Ret::None, [] {
            const uint32_t m = pick(live_mobys());
            vary_frame_scale();
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { glitter_pvars_init(a.i[0], 0); return uint64_t{0}; });
    else
        not_here("glitter_pvars_init_0");
}

// ---- class 1818, the Sandmouse (shared_sandmouse.cpp)
void sandmouse_checks() {
    const uint32_t upd = copy_here({{1, 0x0030df40}, {2, 0x002f0f88}, {3, 0x002e1970}, {4, 0x002e69d0}, {5, 0x0031e808},
                                    {6, 0x0030ae88}, {8, 0x0030ac28}, {11, 0x0031c800}, {12, 0x0030a6f0}, {14, 0x00309c88}});
    const uint32_t move = copy_here({{1, 0x0030d880}, {2, 0x002f08c8}, {3, 0x002e12b0}, {4, 0x002e6310}, {5, 0x0031e148},
                                     {6, 0x0030a7c8}, {8, 0x0030a568}, {11, 0x0031c140}, {12, 0x0030a030}, {14, 0x003095c8}});
    const uint32_t find = copy_here({{1, 0x0030d308}, {2, 0x002f0350}, {3, 0x002e0d38}, {4, 0x002e5d98}, {5, 0x0031dbd0},
                                     {6, 0x0030a250}, {8, 0x00309ff0}, {11, 0x0031bbc8}, {12, 0x00309ab8}, {14, 0x00309050}});
    const uint32_t shoot = copy_here({{1, 0x0030d5f0}, {2, 0x002f0638}, {3, 0x002e1020}, {4, 0x002e6080}, {5, 0x0031deb8},
                                      {6, 0x0030a538}, {8, 0x0030a2d8}, {11, 0x0031beb0}, {12, 0x00309da0}, {14, 0x00309338}});
    const uint32_t spawn = copy_here({{1, 0x0030c898}, {2, 0x002efdd0}, {3, 0x002e07b8}, {4, 0x002e5818}, {5, 0x0031d650},
                                      {6, 0x003093e0}, {8, 0x00308810}, {11, 0x0031b648}, {12, 0x00309538}, {14, 0x00308ad0}});
    const uint32_t sparks = copy_here({{1, 0x0030dda0}, {2, 0x002f0de8}, {3, 0x002e17d0}, {4, 0x002e6830}, {5, 0x0031e668},
                                       {6, 0x0030ace8}, {8, 0x0030aa88}, {11, 0x0031c660}, {12, 0x0030a550}, {14, 0x00309ae8}});
    const uint32_t goal = copy_here({{1, 0x0030d260}, {2, 0x002f02a8}, {3, 0x002e0c90}, {4, 0x002e5cf0}, {5, 0x0031db28},
                                     {6, 0x0030a1a8}, {8, 0x00309f48}, {11, 0x0031bb20}, {12, 0x00309a10}, {14, 0x00308fa8}});
    const uint32_t points = copy_here({{1, 0x0030ded0}, {2, 0x002f0f18}, {3, 0x002e1900}, {4, 0x002e6960}, {5, 0x0031e798},
                                       {6, 0x0030ae18}, {8, 0x0030abb8}, {11, 0x0031c790}, {12, 0x0030a680}, {14, 0x00309c18}});
    // the point ahead of the hero: every copy on the level (Novalis, Aridia and Eudora carry two)
    std::vector<uint32_t> ahead;
    for (const LvAddr& c : {LvAddr{1, 0x002e30b8}, LvAddr{1, 0x0030d208}, LvAddr{2, 0x002cbda0}, LvAddr{2, 0x002f0250},
                            LvAddr{3, 0x002e0c38}, LvAddr{4, 0x002c6fc0}, LvAddr{4, 0x002e5c98}, LvAddr{5, 0x0031dad0},
                            LvAddr{6, 0x0030a150}, LvAddr{8, 0x00309ef0}, LvAddr{11, 0x0031bac8}, LvAddr{12, 0x003099b8},
                            LvAddr{14, 0x00308f50}})
        if (c.level == g_oracle_level) ahead.push_back(c.addr);
    const char* names[] = {"sandmouse_update", "sandmouse_move", "sandmouse_find_target", "sandmouse_shoot", "class1633_spawn",
                           "sandmouse_sparks", "sandmouse_goal_by_hero", "sandmouse_glow_points", "point_ahead_of_hero"};
    if (upd == 0) {
        for (const char* n : names) not_here(n);
        return;
    }
    const uint32_t tun = copy_here({{1, 0x00162080}, {2, 0x001621c0}, {3, 0x00161c88}, {4, 0x00161d80}, {5, 0x00161fd8},
                                    {6, 0x00162380}, {8, 0x00162418}, {11, 0x00162458}, {12, 0x00162040}, {14, 0x001622b0}});
    // the Sandmice: the level's own (placed), or three spawned at the hero with a house (a live moby near)
    auto mice = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (!v.empty()) return v;
        const std::vector<uint32_t> all = live_mobys();
        for (int k = 0; k < 3; ++k) {
            const uint32_t s = moby_spawn(1818);
            if (s == 0) break;
            const uint32_t house = all[static_cast<size_t>(k) % all.size()];
            ee_ref<Vec4>(s + 0x10) = ee_ref<Vec4>(house + 0x10);
            w32(pvar(s) + 0x48, moby_index(house));
            w32(pvar(s) + 0x64, 0xffffffffu);
            v.push_back(s);
        }
        return v;
    };
    // the targets' list (0x1aba00): its mobys (the Sandmouse's shots look for those of kind 5)
    auto listed = [] {
        std::vector<uint32_t> v;
        for (uint32_t e = OV(0x001aba00); v.size() < 256; e += 4) {
            const uint32_t o = ee_read<uint32_t>(e);
            if (o == 0) break;
            v.push_back(o);
        }
        return v;
    };
    if (std::getenv("B514_INFO") != nullptr) {
        level();
        std::printf("INFO sandmouse: %zu placed, %zu listed targets\n", mobys_with(upd).size(), listed().size());
        for (uint32_t m : mice())
            std::printf("INFO   0x%08x state %u house %d zone %d slot %d\n", m, ee_read<uint8_t>(m + 0x20), ee_read<int32_t>(pvar(m) + 0x48),
                        ee_read<int32_t>(pvar(m) + 0x40), ee_read<int32_t>(pvar(m) + 0x64));
    }
    // a target of kind 5 from the list put before the Sandmouse (or none moved)
    auto target_near = [listed](uint32_t m) {
        const std::vector<uint32_t> ts = listed();
        if (ts.empty() || ri(0, 3) == 0) return 0u;
        const uint32_t t = pick(ts);
        // (the Sandmouse put before it: a moby moved in RAM is not moved in the collision grid, so the target stays put)
        const float yaw = ee_read<float>(m + 0x48) + rf(-0.6f, 0.6f), r = coin() ? rf(1.0f, 8.0f) : rf(8.0f, 30.0f);
        const Vec4 p = ee_ref<Vec4>(t + 0x10);
        ee_ref<Vec4>(m + 0x10) = Vec4{p.x - r * std::cos(yaw), p.y - r * std::sin(yaw), p.z - rf(-2.0f, 3.0f), ee_read<float>(m + 0x1c)};
        const uint32_t w = moby_pvar_word0(t);
        if (w != 0 && coin()) wf(w, coin() ? rf(0.5f, 5.0f) : 0.0f);
        if (ee_read<uint16_t>(t + 0x32) == 0 && coin()) w16(t + 0x32, 0xff);
        return t;
    };
    // the hero: in the Sandmouse's zone (at it) or away, on foot (state 5) or not, holding a moby or not
    auto hero = [](uint32_t m) {
        if (coin()) ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 1.5f : 12.0f, -0.5f, 1.5f);
        w8(0x0013d4c5, ri(0, 4) == 0 ? 0u : 1u);
        w32(0x001404a8, ri(0, 3) == 0 ? u(ri(0, 8)) : 5u);
        const int k = ri(0, 3);
        w32(0x00140480, k == 0 ? 0u : (k == 1 ? pick(live_mobys()) : ee_read<uint32_t>(0x00140480)));
        if (k == 1 && coin()) w16(ee_read<uint32_t>(0x00140480) + 0xa6, 0x1b1);
        w32(0x001413dc, ri(0, 3) == 0 ? (coin() ? 0x16u : 0x14u) : ee_read<uint32_t>(0x001413dc));
        w8(0x001413f4, ri(0, 4) == 0 ? 1u : 0u);
    };
    // the Sandmouse's state words dealt
    auto vary = [mice, tun, target_near, hero](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        // (its house: its own, none, or another moby (one in state 5: open))
        const int hk = ri(0, 5);
        if (hk == 0) w32(pv + 0x48, 0xffffffffu);
        else if (hk == 1) w32(pv + 0x48, moby_index(pick(live_mobys())));
        if (ee_read<int32_t>(pv + 0x48) != -1 && coin())
            w8(first_moby() + (ee_read<uint32_t>(pv + 0x48) << 8) + 0x20, coin() ? 5u : u(ri(0, 6)));
        hero(m);
        if (coin()) {
            const uint32_t t = target_near(m);
            w32(pv + 0x60, t);
        }
        w8(m + 0xbc, ri(0, 3) == 0 ? u(ri(0, 5)) : (coin() ? 0u : 2u));
        w8(m + 0x31, u(ri(0, 1)));
        w8(m + 0x53, u(ri(0, 3)));
        w8(m + 0x52, coin() ? ee_read<uint8_t>(m + 0x53) : u(ri(0, 3)));
        w8(m + 0x70, R() & 0xff);
        w8(m + 0x23, u(ri(0, 0xff)));
        w8(0x0015edb0, u(ri(0, 1)));
        w8(pv + 0x91, u(ri(0, 1)));
        w32(OV(0x0015f5c4), ri(0, 4) == 0 ? 2u : 0u);
        // (timers at their edges, counters about the level's limit)
        for (uint32_t k : {0x52u, 0x54u}) w16(pv + k, ri(0, 2) == 0 ? u(ri(0, 2)) : u(ri(0, 130)));
        if (coin()) w16(pv + 0x52, u(frame_scale_i(0x14)));
        if (ri(0, 3) == 0) w32(tun + 0x24, u(ri(1, 12)));  // (the level's out time, which divides the burrow's fading: small too)
        w32(pv + 0x58, u(ri(0, 3)));
        w32(pv + 0x5c, coin() ? u(ri(0, 3)) : u(ee_read<int32_t>(tun + 0x1c) + ri(-6, 1)));
        w16(pv + 0x50, ri(0, 2) == 0 ? u(ri(15, 30)) : u(ri(0, 3)));
        w8(pv + 0x56, u(ri(0, 1)));
        // (its sound slot: none, or one of the slots (its own playing or not))
        const int sk = ri(0, 3);
        if (sk == 0) w32(pv + 0x64, 0xffffffffu);
        else {
            const uint32_t slot = u(ri(0, 7)), e = 0x0013e550 + slot * 0x70;
            w32(pv + 0x64, slot);
            if (sk != 3) w32(e + 0x88, m);
            w8(e + 0x74, u(ri(0, 1)));
        }
        // (its goal: near it or at the hero; its home)
        if (coin()) ee_ref<Vec4>(pv) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 0.3f : 6.0f, -0.3f, 0.3f);
        if (coin()) ee_ref<Vec4>(pv + 0x30) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 0.5f : 4.0f, -0.3f, 0.3f);
        if (coin()) ee_ref<Vec4>(pv + 0x10) = Vec4{rf(-0.2f, 0.2f), rf(-0.2f, 0.2f), rf(-0.1f, 0.1f), 0.0f};
        if (coin()) for (uint32_t k : {0x40u, 0x44u, 0x48u}) wf(m + k, rf(-3.1f, 3.1f));
        // (once collected: the level's byte or bit)
        if (st == 0 && ri(0, 3) == 0) {
            const int32_t idx = ee_read<int16_t>(m + 0xb2);
            if (coin()) w8(OV(0x001bb230) + u(idx) + 0x454, 1);
            else {
                const uint32_t w = 0x0014c190 + (ee_read<uint32_t>(0x0015ed84) << 8) + (u(idx >> 5) << 2);
                w32(w, ee_read<uint32_t>(w) | (1u << (idx & 0x1f)));
            }
        }
        vary_frame_scale();
        w8(m + 0x20, u(st));
        seed_rand();
    };
    // the walker record and the Sandmouse set up first by its state 0 (the port's) in most cases
    auto set_up = [](uint32_t m) {
        const uint8_t st = ee_read<uint8_t>(m + 0x20);
        w8(m + 0x20, 0);
        const uint32_t sp0 = ee_stack_ptr();
        try { sandmouse_update(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
        if (ee_read<uint8_t>(m + 0x20) < 0xfe) w8(m + 0x20, st);
    };

    // Left out of the checks that test lines and steps against the world (the update, the step, the search, the shot): the
    // segment records 0x173e90 .. 0x173ebc that segment_collide / the walker's collision leave from VU0 registers in the
    // original, which their hand ports do not reproduce (as batches 240 / 242 / 372)
    std::vector<uint32_t> seg;
    for (uint32_t k = 0; k < 0x30; k += 4) seg.push_back(OV(0x00173e90) + k);
    const int only_st = std::getenv("B514_ST") != nullptr ? std::atoi(std::getenv("B514_ST")) : -1;
    without(seg, [&] {
        for (int st = 0; st < 10; ++st)
            if (only_st < 0 || only_st == st)
                classcheck("sandmouse_update", upd, mice, sandmouse_update, [st, vary, set_up](uint32_t m) {
                    if (st != 0 && ri(0, 3) != 0) set_up(m);
                    vary(m, st);
                }, st == 3 || st == 5 || st == 6 ? 60 : 30);
    });

    auto any_mouse = [mice, vary, set_up] {
        const uint32_t m = pick(mice());
        if (coin()) set_up(m);
        vary(m, ri(0, 9));
        return m;
    };
    without(seg, [&] {
        helpercheck("sandmouse_move", move, 120, Ret::None, [any_mouse] {
            const uint32_t m = any_mouse();
            if (coin()) w8(m + 0x20, 6);
            return Args{{m, pvar(m)}, {}};
        }, [](A a) { sandmouse_move(a.i[0], a.i[1]); return uint64_t{0}; });
        // the search: from a point at the Sandmouse, its angles, reaches about the level's (radians), a range, near reaches
        // (the search from a point by the Sandmouse, its angles dealt, a range of 1 .. 60; no moby moved. Searches from points
        // elsewhere (before a listed moby, across the world) meet a known difference of the hand port of segment_collide: on
        // Aridia, Hoven and Oltanis its hit (a moby where the original's reports the world at a fixed point), hit point and
        // counts differ, so the target found does; the generated copies of this function differ alike, case for case)
        helpercheck("sandmouse_find_target", find, 120, Ret::Int32, [mice, listed] {
            const uint32_t m = pick(mice());
            ee_ref<Vec4>(TEST_AREA) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 0.5f, 0.0f, 1.0f);
            const Vec4 q = ee_ref<Vec4>(TEST_AREA);
            std::vector<uint32_t> near;  // (the listed mobys within 25)
            for (uint32_t t : listed()) {  // (of kind 5, shown)
                const Vec4 p = ee_ref<Vec4>(t + 0x10);
                const uint32_t cls = ee_read<uint32_t>(t + 0x24);
                if (std::hypot(p.x - q.x, p.y - q.y) < 25.0f && cls != 0 && ee_read<int16_t>(cls + 0x46) == 5 &&
                    ee_read<uint16_t>(t + 0x32) != 0)
                    near.push_back(t);
            }
            float range = coin() ? rf(1.0f, 12.0f) : rf(12.0f, 60.0f);
            if (!near.empty() && ri(0, 3) != 0) {  // (turned toward one of them, about its pitch, it within the range or not)
                const uint32_t t = pick(near);
                const Vec4 p = ee_ref<Vec4>(t + 0x10);
                const float d = std::sqrt((p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y) + (p.z - q.z) * (p.z - q.z));
                wf(m + 0x48, std::atan2(p.y - q.y, p.x - q.x) + (coin() ? rf(-0.1f, 0.1f) : rf(-0.6f, 0.6f)));
                wf(m + 0x44, std::atan2(std::hypot(p.x - q.x, p.y - q.y), p.z + 0.4f - q.z) + rf(-0.2f, 0.2f));
                range = coin() ? d + rf(0.1f, 5.0f) : rf(1.0f, d + 5.0f);
                const uint32_t w = moby_pvar_word0(t);
                if (w != 0) wf(w, ri(0, 3) != 0 ? rf(0.5f, 5.0f) : 0.0f);
            } else {
                wf(m + 0x48, rf(-3.1f, 3.1f));
                wf(m + 0x44, rf(-0.8f, 0.8f));
            }
            return Args{{m, TEST_AREA, m + 0x40}, {rf(0.1f, 1.5f), rf(0.1f, 1.5f), range}};
        }, [](A a) {
            const float pi = 3.14159274f;
            return uint64_t{sandmouse_find_target(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1], a.f[2], 2.0f, pi, pi)};
        });
        helpercheck("sandmouse_shoot", shoot, 120, Ret::None, [any_mouse, target_near, tun] {
            const uint32_t m = any_mouse();
            const uint32_t pv = pvar(m);
            if (coin()) w16(pv + 0x54, 1);
            if (coin()) { target_near(m); target_near(m); }
            if (coin()) wf(0x0013f3d8, ee_read<float>(m + 0x18) - rf(0.0f, 3.0f));
            if (coin()) wf(tun + 0x14, rf(0.0f, 2.0f));
            return Args{{m, pv}, {}};
        }, [](A a) { sandmouse_shoot(a.i[0], a.i[1]); return uint64_t{0}; });
    });
    helpercheck("class1633_spawn", spawn, 40, Ret::Int32, [mice] {
        const uint32_t m = coin() ? pick(mice()) : pick(live_mobys());
        ee_ref<Vec4>(TEST_AREA) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 1.0f);
        vary_frame_scale();
        return Args{{m, TEST_AREA}, {rf(-3.1f, 3.1f), rf(-1.5f, 1.5f), rf(-3.1f, 3.1f), rf(-1.5f, 1.5f)}};
    }, [](A a) { return uint64_t{class1633_spawn(a.i[0], a.i[1], a.f[0], a.f[1], a.f[2], a.f[3])}; });
    helpercheck("sandmouse_sparks", sparks, 40, Ret::None, [any_mouse] {
        const uint32_t m = any_mouse();
        if (coin()) w8(m + 0x31, 1);
        return Args{{m, pvar(m)}, {}};
    }, [](A a) { sandmouse_sparks(a.i[0], a.i[1]); return uint64_t{0}; });
    helpercheck("sandmouse_goal_by_hero", goal, 30, Ret::None, [any_mouse] {
        const uint32_t m = any_mouse();
        return Args{{m, coin() ? pvar(m) : TEST_AREA}, {}};
    }, [](A a) { sandmouse_goal_by_hero(a.i[0], a.i[1]); return uint64_t{0}; });
    helpercheck("sandmouse_glow_points", points, 30, Ret::None, [any_mouse] { return Args{{any_mouse()}, {}}; },
                [](A a) { sandmouse_glow_points(a.i[0]); return uint64_t{0}; });
    for (const uint32_t copy : ahead)
        helpercheck("point_ahead_of_hero", copy, 20, Ret::None, [mice] {
            const uint32_t m = pick(mice());
            if (coin()) ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 10.0f);
            return Args{{m, coin() ? pvar(m) : TEST_AREA}, {}};
        }, [](A a) { point_ahead_of_hero(a.i[0], a.i[1]); return uint64_t{0}; });
}

// ---- class 28 (shared_class28.cpp)
void class28_checks() {
    const uint32_t upd = copy_here({{14, 0x002b17d8}, {15, 0x002955c0}});
    const uint32_t hits = copy_here({{14, 0x002b2f60}, {15, 0x00296d48}});
    const uint32_t visuals = copy_here({{14, 0x002b2600}, {15, 0x002963e8}});
    const uint32_t rocket = copy_here({{14, 0x002e0170}, {15, 0x002cae58}});
    const uint32_t aim = copy_here({{14, 0x002b35d8}, {15, 0x002973c0}});
    const uint32_t shell = copy_here({{14, 0x002dea98}, {15, 0x002ca668}});
    const uint32_t step = copy_here({{14, 0x002b3418}, {15, 0x00297200}});
    const uint32_t lengths = copy_here({{14, 0x002b3358}, {15, 0x00297140}});
    const uint32_t head = copy_here({{14, 0x002b37b0}, {15, 0x00297598}});
    const uint32_t target = copy_here({{14, 0x002b3268}, {15, 0x00297050}});
    const uint32_t pause = copy_here({{14, 0x002b3890}, {15, 0x00297678}});
    const uint32_t settled = copy_here({{14, 0x002b39a0}, {15, 0x00297788}});
    const uint32_t firing = copy_here({{14, 0x002b3920}, {15, 0x00297708}});
    const uint32_t range = copy_here({{14, 0x002b2ed8}, {15, 0x00296cc0}});
    const uint32_t call = copy_here({{14, 0x002d9870}, {15, 0x002c7098}});
    const uint32_t drift = copy_here({{14, 0x002b3578}, {15, 0x00297360}});
    const uint32_t hide = copy_here({{14, 0x002b3318}, {15, 0x00297100}});
    const char* names[] = {"class28_update", "class28_hits", "class28_visuals", "class28_spawn_rocket", "class28_aim",
                           "class28_spawn_shell", "class28_path_step", "class28_path_lengths", "class28_head_turn",
                           "class28_pick_target", "class28_group_pause", "class28_group_settled", "class28_group_firing",
                           "class28_range", "class28_call", "class28_drift", "class28_hide"};
    if (upd == 0) {
        for (const char* n : names) not_here(n);
        return;
    }
    const uint32_t tun = g_oracle_level == 15 ? 0x00161388u : 0x001614a8u;  // (the level's tunings: shared_class28.cpp)
    auto turrets = [upd] { return mobys_with(upd); };
    if (std::getenv("B514_INFO") != nullptr) {
        level();
        std::printf("INFO class 28: %zu turrets, %zu groups with one\n", turrets().size(), groups_with(0x1c).size());
        for (uint32_t m : turrets())
            std::printf("INFO   0x%08x state %u group %u path %d zone %d rides %u call %d\n", m, ee_read<uint8_t>(m + 0x20),
                        ee_read<uint8_t>(m + 0x21), ee_read<int32_t>(pvar(m) + 0x240), ee_read<int32_t>(pvar(m) + 0x244),
                        ee_read<uint8_t>(pvar(m) + 0x24a), ee_read<int32_t>(pvar(m) + 0x264));
    }
    // a target for the turret: the hero's moby (the hero moved near), another live moby, another turret or none
    auto aim_target = [turrets](uint32_t m) {
        const uint32_t pv = pvar(m);
        const int k = ri(0, 5);
        uint32_t t = ee_read<uint32_t>(0x001413d0);
        if (k == 1) t = pick(live_mobys());
        else if (k == 2) t = pick(turrets());
        else if (k == 3) t = 0;
        w32(pv + 0x110, t);
        if (t == 0) return;
        const float r = ri(0, 2) == 0 ? rf(0.0f, 6.0f) : rf(6.0f, 60.0f);
        ee_ref<Vec4>(t + 0x10) = around(ee_ref<Vec4>(m + 0x10), r, r, -3.0f, 3.0f);
        if (t == ee_read<uint32_t>(0x001413d0)) ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(t + 0x10);
        // (often facing it already: the fire's angle tests pass)
        if (coin()) {
            const Vec4 d = ee_ref<Vec4>(t + 0x10), p = ee_ref<Vec4>(m + 0x10);
            wf(m + 0x48, std::atan2(d.y - p.y, d.x - p.x));
            if (coin()) { wf(pv + 0x268, 0.0f); w32(pv + 0x26c, 0); }
        }
    };
    // the turret's paths: a path to ride (or none), a zone (a path), the point on it, the moby it calls
    auto paths = [turrets](uint32_t m) {
        const uint32_t pv = pvar(m);
        if (ri(0, 2) != 0) {
            const int32_t p = ri(0, 5) == 0 ? -1 : a_path();
            w32(pv + 0x240, u(p));
            if (p >= 0) {
                const int32_t n = ee_read<int32_t>(path_ptr(p));
                const int32_t idx = coin() ? ri(0, n - 1) : (coin() ? n - 1 : 0);
                w32(pv + 0x24c, u(idx));
                wf(pv + 0x250, coin() ? 0.0f : rf(0.0f, 3.0f));
                if (coin()) ee_ref<Vec4>(m + 0x10) = around(ee_ref<Vec4>(path_ptr(p) + 0x10 + u(idx) * 0x10), 0.0f, coin() ? 0.2f : 3.0f, -0.2f, 0.2f);
            }
        }
        if (ri(0, 3) == 0) w32(pv + 0x244, ri(0, 5) == 0 ? 0xffffffffu : u(a_path()));
        if (ri(0, 2) == 0) {
            const int k = ri(0, 4);
            w32(pv + 0x264, k == 0 ? 0xffffffffu : moby_index(k == 1 ? pick(live_mobys()) : pick(turrets())));
        }
        if (ri(0, 2) == 0) w8(pv + 0x24a, u(ri(0, 1)));
        if (ri(0, 2) == 0) w8(pv + 0x24b, u(ri(0, 1)));
    };
    // the group: its own, none, or one holding turrets (their states dealt: some waking, some firing)
    auto group = [turrets](uint32_t m) {
        const std::vector<uint32_t> gs = groups_with(0x1c);
        const int k = ri(0, 3);
        if (k == 1) w8(m + 0x21, 0xff);
        else if (k == 2 && !gs.empty()) w8(m + 0x21, pick(gs));
        for (uint32_t o : turrets())
            if (o != m && ri(0, 2) == 0) w8(o + 0x20, coin() ? u(ri(0, 13)) : u(ri(5, 6)));
    };
    // the animation: the set (+0x53), the one shown (+0x52), done or not (+0x70), its time (the frame record's +4)
    auto animation = [](uint32_t m) {
        w8(m + 0x53, coin() ? u(ri(0, 7)) : ee_read<uint8_t>(m + 0x53));
        w8(m + 0x52, coin() ? ee_read<uint8_t>(m + 0x53) : u(ri(0, 7)));
        w8(m + 0x70, coin() ? (R() & 0xff) : (ee_read<uint8_t>(m + 0x70) | 2u));
        if (coin()) {
            wf(m + 0x54, coin() ? 0.0f : rf(0.0f, 1.0f));
            const uint32_t cur = ee_read<uint8_t>(m + 0x52) == 0xff ? ee_read<uint32_t>(m + 0x6c) : ee_read<uint32_t>(m + 0x68);
            if (cur >= 0x00100000u && cur < 0x02000000u) w16(cur + 4, u(ri(0, 100)));
        }
    };
    // the turret's state words and its surroundings dealt
    auto vary = [turrets, tun, aim_target, paths, group, animation](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        w32(pv + 0x288, ri(0, 6) == 0 ? 1u : 0u);
        w8(0x001413f4, ri(0, 3) == 0 ? 2u : u(ri(0, 1)));
        w32(OV(0x0015f5c4), ri(0, 4) == 0 ? 2u : u(ri(0, 1)));
        aim_target(m);
        paths(m);
        group(m);
        animation(m);
        w32(pv + 0x114, ri(0, 3) == 0 ? 2u : 0u);
        for (uint32_t k : {0x248u, 0x260u, 0x262u}) w16(pv + k, ri(0, 2) == 0 ? u(ri(0, 2)) : u(ri(0, 60)));
        w32(pv + 0x238, u(ri(0, 3)));
        w32(pv + 0x38, ri(0, 3) == 0 ? 1u : 0u);
        if (coin()) wf(pv + 0x234, rf(4.0f, 40.0f));
        if (coin()) wf(pv + 0x20, ri(0, 2) == 0 ? rf(-0.5f, 4.0f) : (coin() ? 0.0f : (coin() ? 0.5f : 5.0f)));  // (the health: some at none or at a hit's damage)
        if (coin()) wf(pv + 0x278, rf(-3.1f, 3.1f));
        if (coin()) wf(pv + 0x27c, coin() ? 1.0f : -1.0f);
        if (coin()) for (uint32_t k : {0x254u, 0x258u}) wf(pv + k, rf(-0.3f, 0.3f));
        if (coin()) for (uint32_t k : {0x268u, 0x270u, 0x280u}) wf(pv + k, rf(-1.5f, 1.5f));
        if (coin()) wf(m + 0x48, rf(-3.1f, 3.1f));
        if (coin()) w8(pv + 0x121, u(ri(0, 1)));
        if (coin()) w8(pv + 0x161, u(ri(0, 1)));
        if (ri(0, 3) == 0) wf(tun + 0x34, rf(0.5f, 1.0f));  // (the drift's drag)
        give_hit(m, 0x330000u, turrets());
        if (coin()) ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 10.0f : 50.0f);
        vary_frame_scale();
        w8(m + 0x20, u(st));
        seed_rand();
    };

    const int only_st = std::getenv("B514_ST") != nullptr ? std::atoi(std::getenv("B514_ST")) : -1;  // (one state only: debugging)
    // Left out of the update's check: (state 6 tests the muzzle's line with segment_collide) the segment record 0x173e90 ..
    // 0x173e9c it leaves holds VU0 leftovers in the original that the hand port of segment_collide does not reproduce
    // (batches 506 / 509); and (a turret killed by its hit falls in the same frame: state 11, enemy_physics) the w lanes
    // enemy_physics stores from a VU0 register into the collision record's row 0x173e70 and the moby's position (its pc
    // 0x2650c0 and its callee's 0x212898 on Oltanis, ORACLE_WATCH), which the hand port of enemy_physics does not
    // reproduce: they differ only after earlier checks left the interpreter's VU0 so
    std::vector<uint32_t> left_out = {OV(0x00173e90), OV(0x00173e94), OV(0x00173e98), OV(0x00173e9c), OV(0x00173e7c)};
    level();
    for (uint32_t m : turrets()) left_out.push_back(m + 0x1c);
    for (int st = 0; st < 15; ++st)
        if (only_st < 0 || only_st == st)
            without(left_out, [&] {
                classcheck("class28_update", upd, turrets, class28_update, [st, vary](uint32_t m) {
                    vary(m, st);
                },
                           st == 4 || st == 6 ? 60 : 30);
            });

    auto any_turret = [turrets, vary] {
        const uint32_t m = pick(turrets());
        vary(m, ri(0, 14));
        return m;
    };
    // (in a third of the cases the health is the hit's damage: the reaction's damage found on a copy of the state first,
    // so that the health ends at 0 exactly)
    helpercheck("class28_hits", hits, 120, Ret::None, [any_turret] {
        const uint32_t m = any_turret();
        if (ee_read<uint8_t>(m + 0xa4) != 0xff && coin()) {  // (a reaction other than the plain one: kinds 2 .. 7, strengths 0 .. 3)
            const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
            w8(e + 0x28, u(ri(2, 7)));
            w8(e + 0x29, u(ri(0, 3)));
            w32(e + 0x20, ee_read<uint32_t>(0x001413d0));  // (from the hero, of the kinds the turret takes)
            w32(e + 0x24, 0x330000u | (R() & 0xff));
            if (coin()) w8(pvar(m) + 0x28, u(ri(0, 3)));  // (the reactions' row of the class)
        }
        if (ri(0, 2) == 0) wf(pvar(m) + 0x20, hit_damage(m, NOT_ADDR(0x330000)));
        return Args{{m}, {}};
    }, [](A a) { class28_hits(a.i[0]); return uint64_t{0}; });
    helpercheck("class28_visuals", visuals, 40, Ret::None, [any_turret] { return Args{{any_turret()}, {}}; },
                [](A a) { class28_visuals(a.i[0]); return uint64_t{0}; });
    // a rocket: thrown (state 1 ..) or along the zone list's path (state 0) of a moby with pvars (+0x60 its zone)
    helpercheck("class28_spawn_rocket", rocket, 60, Ret::Int32, [turrets] {
        const uint32_t m = coin() ? pick(turrets()) : pick(live_mobys());
        wf(TEST_AREA, rf(0.0f, 30.0f));
        uint32_t state = ri(0, 2) == 0 ? 0u : u(ri(1, 3));
        if (state == 0) {  // (a zone of the level's zone list 0x15f70c whose path is one: else thrown)
            const std::vector<uint32_t> zs = zones_with_paths();
            if (zs.empty()) state = 1;
            else w32(pvar(m) + 0x60, pick(zs));
        }
        if (coin()) ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 20.0f);
        vary_frame_scale();
        seed_rand();
        return Args{{m, TEST_AREA, state}, {}};
    }, [](A a) { return uint64_t{class28_spawn_rocket(a.i[0], a.i[1], a.i[2])}; });
    helpercheck("class28_spawn_shell", shell, 40, Ret::Int32, [turrets] {
        const uint32_t m = coin() ? pick(turrets()) : pick(live_mobys());
        ee_ref<Vec4>(TEST_AREA) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 3.0f);
        if (coin()) ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 20.0f);
        vary_frame_scale();
        seed_rand();
        return Args{{m, TEST_AREA}, {}};
    }, [](A a) { return uint64_t{class28_spawn_shell(a.i[0], a.i[1])}; });
    helpercheck("class28_aim", aim, 80, Ret::Float, [any_turret] {
        const uint32_t m = any_turret();
        return Args{{m, u(ri(0, 2) == 0 ? 0 : 1)}, {rf(-3.1f, 3.1f)}};
    }, [](A a) { return uint64_t{f2u(class28_aim(a.i[0], static_cast<int32_t>(a.i[1]), a.f[0]))}; });
    helpercheck("class28_path_step", step, 80, Ret::Int32, [any_turret] {
        const uint32_t m = any_turret();
        const uint32_t pv = pvar(m);
        const int32_t p = a_path();
        w32(pv + 0x240, u(p));
        if (p >= 0) {
            const int32_t n = ee_read<int32_t>(path_ptr(p));
            w32(pv + 0x24c, u(coin() ? ri(0, n - 1) : (coin() ? n - 1 : n - 2)));
            wf(pv + 0x250, coin() ? 0.0f : rf(0.0f, 5.0f));
            if (coin()) wf(pv + 0x27c, coin() ? 1.0f : -1.0f);
        }
        return Args{{m}, {}};
    }, [](A a) { return uint64_t{u(class28_path_step(a.i[0]))}; });
    helpercheck("class28_path_lengths", lengths, 30, Ret::None, [any_turret] {
        const uint32_t m = any_turret();
        w32(pvar(m) + 0x240, ri(0, 4) == 0 ? 0xffffffffu : u(a_path()));
        return Args{{m}, {}};
    }, [](A a) { class28_path_lengths(a.i[0]); return uint64_t{0}; });
    helpercheck("class28_head_turn", head, 40, Ret::None, [any_turret] {
        return Args{{any_turret()}, {rf(-3.1f, 3.1f)}};
    }, [](A a) { class28_head_turn(a.i[0], a.f[0]); return uint64_t{0}; });
    helpercheck("class28_pick_target", target, 60, Ret::None, [any_turret] {
        const uint32_t m = any_turret();
        if (coin()) w32(pvar(m) + 0x110, 0);
        return Args{{m}, {}};
    }, [](A a) { class28_pick_target(a.i[0]); return uint64_t{0}; });
    auto a_group = [turrets, group] {
        const std::vector<uint32_t> gs = groups_with(0x1c);
        group(pick(turrets()));
        vary_frame_scale();
        return gs.empty() || ri(0, 4) == 0 ? u(ri(0, 0xff)) : pick(gs);
    };
    helpercheck("class28_group_pause", pause, 30, Ret::None, [a_group, tun] {
        if (coin()) w32(tun + 0x38, u(ri(0, 90)));
        return Args{{a_group()}, {}};
    }, [](A a) { class28_group_pause(a.i[0]); return uint64_t{0}; });
    helpercheck("class28_group_settled", settled, 40, Ret::Int32, [any_turret] { return Args{{any_turret()}, {}}; },
                [](A a) { return uint64_t{u(class28_group_settled(a.i[0]))}; });
    helpercheck("class28_group_firing", firing, 40, Ret::Int32, [a_group] { return Args{{a_group()}, {}}; },
                [](A a) { return uint64_t{u(class28_group_firing(a.i[0]))}; });
    helpercheck("class28_range", range, 40, Ret::None, [any_turret] { return Args{{any_turret()}, {}}; },
                [](A a) { class28_range(a.i[0]); return uint64_t{0}; });
    helpercheck("class28_call", call, 40, Ret::Int32, [turrets] {
        const uint32_t m = coin() ? pick(turrets()) : pick(live_mobys());
        w8(m + 0x20, ri(0, 2) == 0 ? 0u : (coin() ? 6u : u(ri(1, 12))));
        vary_frame_scale();
        return Args{{moby_index(m)}, {}};
    }, [](A a) { return uint64_t{u(class28_call(a.i[0]))}; });
    helpercheck("class28_drift", drift, 30, Ret::None, [any_turret] { return Args{{any_turret()}, {}}; },
                [](A a) { class28_drift(a.i[0]); return uint64_t{0}; });
    helpercheck("class28_hide", hide, 20, Ret::None, [any_turret] { return Args{{any_turret()}, {}}; },
                [](A a) { class28_hide(a.i[0]); return uint64_t{0}; });
}
}  // namespace

void batch_514() {
    if (g_only_batch != 514) return;
    g_batch = 514;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    // (the scratchpad words the pose function 0x245f18 spills the caller's $t8 / $t9 into: batch 120's)
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);
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
    // the port's VU0 micro memory as the game leaves it, and the interpreter's VU0 kept for the port (as batch 509)
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

    class28_checks();
    sandmouse_checks();
    class193_checks();
    gold_bolt_checks();
    class156_checks();

    //@@NEXT
    g_pre_call = nullptr;
    g_extra_noop_hooks.clear();
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
