// Oracle batch 517 (stage 4 task F11, #149): layer-1 families several levels carry a copy of, each ported once as shared
// hand-written code (src/game/ovl/shared_*.cpp) on top of the earlier tasks' shared ports, against every level's own copy
// at that level's own address. The level copies stay transliterated in their units until each level is re-glued; here every
// copy is the original and the one shared port the port:
//  - shared_class252_update.cpp: class 252's update, its move and its hits (levels 8, 14);
//  - shared_class52.cpp: class 52's update and its hits (levels 9, 16);
//  - shared_class1400.cpp: class 1400, the weather about the camera: its update, its wind and its two particles (levels 8, 12,
//    14);
//  - shared_class1885.cpp: class 1885, the blob thrower: its update, hits, free slot, throw and two random helpers (levels 9,
//    13); "class1885_fix25" checks its 1.02 fork (1.02 Gameplay fix 25) when run with RAC1_GAMEPLAY102=25;
//  - shared_class1511.cpp: class 1511's update and its remains (levels 5, 14);
//  - shared_floaters.cpp: the rafts' next stage (levels 5, 14, 16), each level's own build of the arrival it calls, and
//    Oltanis's build of floater_find;
//  - shared_seas.cpp: the sea of Umbris and Gaspar (classes 460 / 317; levels 7, 9), class 1139 (levels 5, 16), the puff of
//    particle 0x34 about a moby (levels 10, 12, 13);
//  - shared_class49.cpp: class 50's update, a homing shot (levels 16, 18);
//  - shared_class886.cpp: class 886, a checkpoint pad (levels 7, 12);
//  - shared_bg_ships.cpp: ship 1, class 1212 (levels 2, 9);
//  - shared_envmap_meshes.cpp: the meshes' draw callbacks of classes 1952 (level 16) and 1907 .. 1918 (level 17), and
//    shared_little.cpp's second build of envmap_uvs4 (class 1913's);
//  - shared_class655_draw.cpp: class 655's draw callback and its strips of quads (levels 15, 17, 18), and the finale's build of
//    its glows (shared_class655_glows.cpp);
//  - shared_env_objects.cpp: Hoven's and Oltanis's sky-reflecting objects: the callback, the draw and the VU0 mapping (levels
//    12, 14);
// Subjects: the level's own mobys of the class after the first gameplay frame (the port's; the screen, the projection and the
// view set up), perturbed through every state; the helpers on the class's mobys with arguments built from the level's state;
// the frame scale (0x15ed68) at its 50 Hz value in a quarter of the cases. No RAM dumps; no broad list of left-out words.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 517, N = each level a family names (2, 5, 7 .. 10, 12 .. 18); a family
// the level does not carry says so and is not run. B517_INFO=1 prints the subjects each family finds.

#include "ovl_oracle.h"

#include <game/enhance/advanced.h>
#include <game/lib/libcdvd.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class252.h>
#include <game/ovl/shared_class52.h>
#include <game/ovl/shared_class1400.h>
#include <game/ovl/shared_class1885.h>
#include <game/ovl/shared_class1511.h>
#include <game/ovl/shared_floaters.h>
#include <game/ovl/shared_seas.h>
#include <game/ovl/shared_class49.h>
#include <game/ovl/shared_class886.h>
#include <game/ovl/shared_bg_ships.h>
#include <game/ovl/shared_envmap_meshes.h>
#include <game/ovl/shared_class655_draw.h>
#include <game/ovl/shared_class655_glows.h>
#include <game/ovl/shared_env_objects.h>
#include <runtime/kernel/disc.h>
#include <runtime/vu/vu0.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <unordered_set>
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

// the middle of the path `idx`'s points (inside its area when the area is convex), or `fallback`
Vec4 path_middle(int32_t idx, const Vec4& fallback) {
    if (idx < 0) return fallback;
    if (idx >= 64) return fallback;
    const uint32_t p = path_ptr(idx);
    if (p < 0x00100000u || p >= 0x01ff0000u) return fallback;
    const int32_t n = ee_read<int32_t>(p);
    if (n < 1 || n > 200) return fallback;
    Vec4 c{0.0f, 0.0f, 0.0f, fallback.w};
    for (int32_t k = 0; k < n; ++k) {
        const Vec4 q = ee_ref<Vec4>(p + 0x10 + static_cast<uint32_t>(k) * 0x10);
        c.x += q.x / static_cast<float>(n);
        c.y += q.y / static_cast<float>(n);
        c.z += q.z / static_cast<float>(n);
    }
    return c;
}

// The debris records past the moby table (0x80 bytes each): the w lanes of their two vectors +0xc / +0x1c, which the
// original's debris_spawn fills from a VU0 sin / cos and its port leaves at 0 (as batch 120). Left out of the checks of
// updates that spawn debris (a known difference of debris_spawn's port, not of the ports here).
std::vector<uint32_t> debris_lanes() {
    level();
    std::vector<uint32_t> lanes;
    for (uint32_t a = end_moby(); a < end_moby() + 0x20000; a += 0x80) {
        lanes.push_back(a + 0xc);
        lanes.push_back(a + 0x1c);
    }
    return lanes;
}

// the groups holding a moby of class `cls` whose whole list (to its last, with bit 15, within 64) names mobys of the table (a
// list the table pointer finds in other data would name mobys past its end: the original's stores wrap at 32 MB)
std::vector<uint32_t> whole_groups_with(uint32_t cls) {
    std::vector<uint32_t> v;
    for (uint32_t g : groups_with(cls)) {
        uint32_t e = ee_read<uint32_t>(OV(0x001ab840) + g * 4);
        bool ok = false;
        for (int k = 0; k < 64; ++k, e += 2) {
            const uint16_t h = ee_read<uint16_t>(e);
            if (first_moby() + ((h & 0x7fffu) << 8) >= end_moby()) break;
            if ((h & 0x8000u) != 0) { ok = true; break; }
        }
        if (ok) v.push_back(g);
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

// the animation's frame shown (the current animation record's +4, in 16ths) set to `frame`
void anim_frame(uint32_t m, uint32_t frame16) {
    wf(m + 0x54, 0.0f);
    const uint32_t cur = ee_read<uint8_t>(m + 0x52) == 0xff ? ee_read<uint32_t>(m + 0x6c) : ee_read<uint32_t>(m + 0x68);
    if (cur >= 0x00100000u && cur < 0x02000000u) w16(cur + 4, frame16);
}

bool info() { return std::getenv("B517_INFO") != nullptr; }

// Arguments the original takes as whole 64-bit registers (a TEX0): the case's values, put in the interpreter's registers by
// the batch's pre-call (check_fn passes 32-bit words, sign-extended); the port reads them from here (batch 504's)
std::vector<std::pair<int, uint64_t>> g_wide_regs;
uint64_t wide_arg(int reg) {
    for (const auto& [r, v] : g_wide_regs)
        if (r == reg) return v;
    return 0;
}

// ---- draw checks (as batches 506 / 510): the packet cursor on a test-area buffer; B517_INFO=1 prints per draw check how many
// cases wrote packets beyond a state header
void packet_cursor() { w32(OV(0x001611c0), TEST_AREA + 0x8000 + u(ri(0, 0x40)) * 0x10); }
uint32_t packet_content(uint32_t c0, uint32_t c1) {
    uint32_t content = 0;
    for (uint32_t p = c0; p < c1;) {
        const uint32_t w0 = ee_read<uint32_t>(p), size = ((w0 & 0xffffu) + 1) * 0x10;
        if (((w0 >> 28) & 7) != 1 || p + size > c1) return content + (c1 - p);
        const bool gs_state = (w0 & 0xffffu) == 5 && ee_read<uint64_t>(p + 0x10) == 0x2400000000000001ull;
        const bool reg_write = (w0 & 0xffffu) == 2 && ee_read<uint32_t>(p + 0x10) == 0x8001u && ee_read<uint32_t>(p + 0x14) == 0x10000000u;
        if (!gs_state && !reg_write) content += size;
        p += size;
    }
    return content;
}
// a draw check: as fcheck, counting the cases whose packets carry more than a state header
void dcheck(const char* name, uint32_t laddr, int n, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
            uint32_t skip = 0) {  // (`skip`: the bytes of a fixed header every run writes first, not counted)
    if (!selected(name)) return;
    std::printf("%-26s L%d:0x%08x\n", name, g_oracle_level, laddr);
    int cases = 0, drew = 0;
    uint32_t content = 0;
    const std::function<uint64_t(const Args&)> synced = [&](const Args& a) {
        vu0_sync();
        const uint32_t c0 = ee_read<uint32_t>(OV(0x001611c0));
        const uint64_t r = port(a);
        const uint32_t c1 = ee_read<uint32_t>(OV(0x001611c0));
        ++cases;
        const uint32_t c = c0 >= 0x00100000u && c1 > c0 + skip && c1 - c0 < 0x100000 ? packet_content(c0 + skip, c1) : 0;
        if (c != 0) ++drew;
        content += c;
        return r;
    };
    if (!check_fn(name, laddr, n, Ret::None, gen, synced, Scope::FullRam, 0, 0)) ++g_fail;
    std::printf("INFO %s L%d: %d port runs, beyond a state header in %d (0x%x bytes)\n", name, g_oracle_level, cases, drew, content);
}

// ---- the meshes' draw callbacks (shared_envmap_meshes.cpp)
void envmap_meshes_checks() {
    struct Cb { const char* name; int level; uint32_t addr; void (*fn)(uint32_t); };
    static const Cb cbs[] = {{"class1952_draw", 16, 0x002e9ce0, class1952_draw}, {"class1907_draw", 17, 0x002f5690, class1907_draw},
                             {"class1908_draw", 17, 0x002f59a8, class1908_draw}, {"class1910_draw", 17, 0x002f5cc0, class1910_draw},
                             {"class1911_draw", 17, 0x002f5fd8, class1911_draw}, {"class1913_draw", 17, 0x002f62f0, class1913_draw},
                             {"class1918_draw", 17, 0x002f6608, class1918_draw}};
    for (const Cb& c : cbs) {
        if (c.level != g_oracle_level) {
            not_here(c.name);
            continue;
        }
        // (the camera about the hero, looking about; the meshes are the level's, wherever they are)
        dcheck(c.name, c.addr, 30, [] {
            level();
            const uint32_t cam = OV(0x00166dc0);
            if (coin()) ee_ref<Vec4>(cam) = around(ee_ref<Vec4>(0x0013f3d0), 0.0f, coin() ? 5.0f : 60.0f, -2.0f, 20.0f);
            render_view_setup();
            packet_cursor();
            seed_rand();
            return Args{{pick(live_mobys())}, {}};
        }, [c](A a) { c.fn(a.i[0]); return uint64_t{0}; });
    }
}

// ---- class 252's update, move and hits (shared_class252_update.cpp)
void class252_checks() {
    const uint32_t upd = copy_here({{8, 0x002d2af0}, {14, 0x002d98f0}});
    const uint32_t move = copy_here({{8, 0x002d3d78}, {14, 0x002dab78}});
    const uint32_t hit = copy_here({{8, 0x002d36e0}, {14, 0x002da4e0}});
    if (upd == 0) {
        for (const char* n : {"class252_update", "class252_move", "class252_hit"}) not_here(n);
        return;
    }
    // the level's own (placed), or three spawned about the hero
    auto subjects = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (!v.empty()) return v;
        for (int k = 0; k < 3; ++k) {
            const uint32_t s = moby_spawn(252);
            if (s == 0) break;
            ee_ref<Vec4>(s + 0x10) = around(ee_ref<Vec4>(0x0013f3d0), 2.0f, 8.0f, 0.0f, 1.0f);
            v.push_back(s);
        }
        return v;
    };
    if (info()) {
        level();
        std::printf("INFO class 252: %zu placed, %zu subjects\n", mobys_with(upd).size(), subjects().size());
        for (uint32_t m : subjects())
            std::printf("INFO   0x%08x state %u path %d target %08x\n", m, ee_read<uint8_t>(m + 0x20), ee_read<int32_t>(pvar(m) + 0x21c),
                        ee_read<uint32_t>(pvar(m) + 0x1b0));
    }
    auto vary = [](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        const Vec4 at = ee_ref<Vec4>(m + 0x10);
        give_hit(m, 0x330000u);
        wf(pv + 0x20, ri(0, 2) == 0 ? rf(0.0f, 1.0f) : rf(1.0f, 20.0f));
        if (coin()) w32(pv + 0x21c, ri(0, 5) == 0 ? 0xffffffffu : u(std::max(0, a_path())));
        // its target: none, the hero, or a live moby (close by, level with it, half the time)
        const int tk = ri(0, 3);
        uint32_t target = tk == 0 ? 0u : (tk == 1 ? ee_read<uint32_t>(0x001413d0) : pick(live_mobys()));
        if (target == m) target = 0;
        w32(pv + 0x1b0, target);
        if (target != 0 && coin()) ee_ref<Vec4>(target + 0x10) = around(at, 0.0f, 3.5f, -3.0f, 3.0f);
        // (the hero within the search's 14 half the time, beside it or farther; or inside its path's area)
        if (coin()) ee_ref<Vec4>(0x0013f3d0) = around(at, 0.0f, coin() ? 4.0f : 16.0f, -2.0f, 4.0f);
        else if (coin()) ee_ref<Vec4>(0x0013f3d0) = around(path_middle(ee_read<int32_t>(pv + 0x21c), at), 0.0f, 2.0f, -4.0f, 4.0f);
        w32(pv + 0x1b4, coin() ? 2u : u(ri(0, 1)));
        ee_ref<Vec4>(pv + 0x170) = around(at, 0.0f, coin() ? 4.0f : 20.0f, -5.0f, 5.0f);
        if (coin()) ee_ref<Vec4>(pv + 0x1c0) = around(at, 0.0f, coin() ? 1.2f : 18.0f, -1.0f, 1.0f);
        if (coin()) wf(pv + 0x1c8, at.z + (coin() ? rf(-1.0f, 1.0f) : rf(-6.0f, 6.0f)));
        if (coin()) ee_ref<Vec4>(pv + 0x1f0) = around(at, coin() ? 0.45f : 0.0f, coin() ? 0.65f : 6.0f, -1.0f, 1.0f);
        w16(pv + 0x216, u(ri(0, 1)));
        if (coin()) wfrand(pv + 0x200, 4, -2.0f, 4.0f);
        if (coin()) wfrand(pv + 0x40, 3, -0.4f, 0.4f);
        w16(pv + 0x214, coin() ? 0xffffu : u(ri(0, 15)));
        if (coin()) wf(m + 0x48, rf(-3.1f, 3.1f));
        if (coin()) wf(pv + 0x210, rf(-0.3f, 0.3f));
        w8(m + 0x31, u(ri(0, 1)));
        if (ri(0, 3) == 0) ee_ref<Vec4>(OV(0x00166dc0)) = around(at, 0.0f, coin() ? 20.0f : 40.0f);
        w8(m + 0x70, R() & 0xff);
        w32(m + 0x90, R());
        w8(m + 0x53, u(ri(0, 3)));
        w8(m + 0x52, coin() ? ee_read<uint8_t>(m + 0x53) : u(ri(0, 3)));
        if (coin()) {  // (the strike's frames: the beam's 9 .. 14 and the blow's crossing of 14)
            const int k = ri(0, 2);
            anim_frame(m, k == 0 ? u(ri(9 * 16, 14 * 16)) : (k == 1 ? u(ri(13 * 16, 15 * 16)) : u(ri(0, 600))));
        }
        // (carried, state 5: its carry record left as the level has it: carry_update's port differs from the original in records
        // dealt at random (alone, on Oltanis, 14 of 200 cases with the record's states 1 .. 7: the moby grid's cells, the
        // state), a known difference of that port; so the carried state's release, carry_update's 1, is not reached here)
        vary_frame_scale();
        w8(m + 0x20, u(st));
        seed_rand();
    };
    auto set_up = [](uint32_t m) {
        w8(m + 0x20, 0);
        const uint32_t sp0 = ee_stack_ptr();
        try { class252_update(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
    };
    for (int st : {0, 1, 2, 3, 4, 5, 6, 7, 0x63, 9, 0xff})
        classcheck("class252_update", upd, subjects, class252_update, [st, vary, set_up](uint32_t m) {
            if (st != 0 && ri(0, 3) != 0) set_up(m);
            vary(m, st);
        }, st == 2 || st == 4 ? 80 : 40);
    auto a_subject = [subjects, vary, set_up] {
        const uint32_t m = pick(subjects());
        if (coin()) set_up(m);
        vary(m, ri(0, 7));
        return m;
    };
    helpercheck("class252_move", move, 60, Ret::None, [a_subject] { return Args{{a_subject()}, {}}; },
                [](A a) { class252_move(a.i[0]); return uint64_t{0}; });
    helpercheck("class252_hit", hit, 80, Ret::None, [a_subject] {
        const uint32_t m = a_subject();
        if (ri(0, 3) == 0) w8(m + 0x20, coin() ? 0x63u : 4u);
        return Args{{m}, {}};
    }, [](A a) { class252_hit(a.i[0]); return uint64_t{0}; });
}

// ---- class 52's update and hits (shared_class52.cpp)
void class52_checks() {
    const uint32_t upd = copy_here({{9, 0x002c5990}, {16, 0x002a1088}});
    const uint32_t hit = copy_here({{9, 0x002c6230}, {16, 0x002a1928}});
    if (upd == 0) {
        for (const char* n : {"class52_update", "class52_hit"}) not_here(n);
        return;
    }
    auto subjects = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (!v.empty()) return v;
        for (int k = 0; k < 3; ++k) {
            const uint32_t s = moby_spawn(52);
            if (s == 0) break;
            ee_ref<Vec4>(s + 0x10) = around(ee_ref<Vec4>(0x0013f3d0), 3.0f, 9.0f, 0.5f, 2.0f);
            w32(pvar(s) + 0x180, u(std::max(0, a_path())));
            v.push_back(s);
        }
        return v;
    };
    if (info()) {
        level();
        std::printf("INFO class 52: %zu placed, %zu subjects, groups %zu (%zu whole)\n", mobys_with(upd).size(), subjects().size(), groups_with(52).size(),
                    whole_groups_with(52).size());
        for (uint32_t m : subjects())
            std::printf("INFO   0x%08x state %u path %d group %u\n", m, ee_read<uint8_t>(m + 0x20), ee_read<int32_t>(pvar(m) + 0x180),
                        ee_read<uint8_t>(m + 0x21));
    }
    auto vary = [subjects](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        const Vec4 at = ee_ref<Vec4>(m + 0x10);
        give_hit(m, 0x330000u, subjects());
        wf(pv + 0x20, ri(0, 2) == 0 ? rf(0.0f, 1.0f) : rf(1.0f, 6.0f));
        vary_frame_scale();
        {  // (the life timer: about to run out, at a pulse's frame, or anywhere)
            static const int marks[] = {10, 20, 40, 60, 120, 179};
            const int k = ri(0, 3);
            w32(pv + 0x184, k == 0 ? u(ri(0, 2)) : (k == 1 ? u(frame_scale_i(marks[ri(0, 5)]) + 1) : u(ri(0, 400))));
        }
        const int tk = ri(0, 4);
        uint32_t target = tk == 0 ? 0u : (tk == 1 ? ee_read<uint32_t>(0x001413d0) : pick(live_mobys()));
        if (target == m) target = 0;
        if (target != 0 && tk == 4) w8(target + 0x20, coin() ? 0xfeu : 0xfdu);
        w32(pv + 0x160, target);
        const uint32_t tp = target != 0 ? target : ee_read<uint32_t>(0x001413d0);
        if (ri(0, 4) != 0) {  // (the target at the distances the closing in turns at: 1.4, 1.9, 6, or anywhere within 10)
            static const float edges[][2] = {{1.3f, 1.5f}, {1.85f, 2.05f}, {5.8f, 6.2f}, {0.0f, 10.0f}};
            const int k = ri(0, 3);
            ee_ref<Vec4>(tp + 0x10) = around(at, edges[k][0], edges[k][1], -2.0f, 2.0f);
        }
        w32(pv + 0x164, coin() ? 2u : 0u);
        if (coin()) w32(pv + 0x180, ri(0, 5) == 0 ? 0xffffffffu : u(std::max(0, a_path())));
        w32(pv + 0x190, ri(0, 2) == 0 ? 0u : u(ri(1, 200)));
        w32(pv + 0x38, ri(0, 3) == 0 ? 1u : 0u);
        w8(m + 0xbc, u(ri(0, 2)));
        w32(pv + 0x198, u(ri(0, 1)));
        {  // (its sound: none, or a channel still its own and playing, or another's)
            const int slot = ri(-1, 15);
            w32(pv + 0x194, u(slot));
            if (slot >= 0) {
                const uint32_t ch = 0x0013e550u + u(slot) * 0x70;
                w32(ch + 0x88, coin() ? m : R());
                w8(ch + 0x74, u(ri(0, 1)));
            }
        }
        w8(m + 0x70, R() & 0xff);
        if (coin()) {
            const std::vector<uint32_t> gs = whole_groups_with(52);
            w8(m + 0x21, gs.empty() || coin() ? 0xffu : pick(gs));
        }
        w8(m + 0x53, u(ri(0, 4)));
        if (coin()) wf(m + 0x48, rf(-3.1f, 3.1f));
        if (coin()) wf(pv + 0x18c, rf(-0.3f, 0.3f));
        if (coin()) wf(m + 0x18, at.z + rf(-2.0f, 2.0f));
        if (coin()) ee_ref<Vec4>(0x0013f3d0) = around(at, 0.0f, 10.0f, -3.0f, 3.0f);
        else if (coin()) ee_ref<Vec4>(0x0013f3d0) = around(at, 0.0f, 10.0f, coin() ? 2.5f : -4.5f, coin() ? 4.5f : -2.5f);
        else {  // (the hero inside the path's area (the search's target): within its 12 or not, level with it or not)
            const Vec4 mid = path_middle(ee_read<int32_t>(pv + 0x180), at);
            ee_ref<Vec4>(0x0013f3d0) = around(mid, 0.0f, 2.0f, -0.5f, 0.5f);
            if (coin()) ee_ref<Vec4>(m + 0x10) = around(mid, coin() ? 0.0f : 11.0f, coin() ? 11.0f : 18.0f, coin() ? -2.5f : 2.5f, coin() ? 4.5f : 2.5f);
        }
        // (knocked back: its move record's speed, often almost spent)
        if (coin()) ee_ref<Vec4>(pv + 0x70) = Vec4{rf(-0.1f, 0.1f), rf(-0.1f, 0.1f), rf(-0.1f, 0.1f), 0.0f};
        if (coin()) wf(pv + 0xf4, rf(0.0f, 0.3f));
        w8(m + 0x31, u(ri(0, 1)));
        if (ri(0, 3) == 0) ee_ref<Vec4>(OV(0x00166dc0)) = around(at, 0.0f, coin() ? 20.0f : 40.0f);
        w32(OV(0x0015f5c4), ri(0, 7) == 0 ? 2u : 0u);
        if (coin()) wf(m + 0x2c, rf(0.0f, 3.0f));
        w8(m + 0x20, u(st));
        seed_rand();
    };
    auto set_up = [](uint32_t m) {
        w8(m + 0x20, 0);
        const uint32_t sp0 = ee_stack_ptr();
        try { class52_update(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
    };
    for (int st : {0, 1, 2, 3, 4, 5, 6, 7, 8})
        classcheck("class52_update", upd, subjects, class52_update, [st, vary, set_up](uint32_t m) {
            if (st != 0 && ri(0, 3) != 0) set_up(m);
            vary(m, st);
        }, st == 2 || st == 4 || st == 1 ? 80 : 40);
    helpercheck("class52_hit", hit, 120, Ret::None, [subjects, vary, set_up] {
        const uint32_t m = pick(subjects());
        if (coin()) set_up(m);
        vary(m, ri(0, 7));
        if (coin()) {  // (the search's path test: its timer out, the hero inside the path's area, within 12 or not, level or not)
            const uint32_t pv = pvar(m);
            w32(pv + 0x190, 0);
            w32(pv + 0x38, 0);
            w8(m + 0xbc, 0);
            const Vec4 mid = path_middle(ee_read<int32_t>(pv + 0x180), ee_ref<Vec4>(m + 0x10));
            ee_ref<Vec4>(0x0013f3d0) = around(mid, 0.0f, 1.0f, -0.2f, 0.2f);
            const int k = ri(0, 2);
            if (k == 0) ee_ref<Vec4>(m + 0x10) = around(mid, 0.0f, 16.0f, -1.0f, 1.0f);
            else ee_ref<Vec4>(m + 0x10) = around(mid, 0.0f, 2.0f, k == 1 ? 2.5f : -4.5f, k == 1 ? 4.5f : -2.5f);
            seed_rand();
        }
        return Args{{m}, {}};
    }, [](A a) { class52_hit(a.i[0]); return uint64_t{0}; });
}

// ---- class 1400, the weather about the camera (shared_class1400.cpp)
void class1400_checks() {
    const uint32_t upd = copy_here({{8, 0x00307cf0}, {12, 0x00308be0}, {14, 0x00306390}});
    const uint32_t wind = copy_here({{8, 0x00307b90}, {12, 0x00308a80}, {14, 0x00306230}});
    const uint32_t drop = copy_here({{8, 0x00272cb8}, {12, 0x00281420}, {14, 0x002706b0}});
    const uint32_t mote = copy_here({{8, 0x0027f5f0}, {12, 0x0028db38}, {14, 0x0027cfb0}});
    const uint32_t rec = copy_here({{8, 0x00162340}, {12, 0x00162000}, {14, 0x00162210}});
    if (upd == 0) {
        for (const char* n : {"class1400_update", "class1400_wind", "class1400_drop", "class1400_mote"}) not_here(n);
        return;
    }
    auto subjects = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (!v.empty()) return v;
        const uint32_t s = moby_spawn(1400);
        if (s != 0) v.push_back(s);
        return v;
    };
    if (info()) {
        level();
        std::printf("INFO class 1400: %zu placed, %zu subjects, level word %u, count %d\n", mobys_with(upd).size(), subjects().size(),
                    ee_read<uint32_t>(0x0015ed84), ee_read<int32_t>(rec + 0x34));
        const Vec4 c = ee_ref<Vec4>(OV(0x00166dc0)), h = ee_ref<Vec4>(0x0013f3d0);
        std::printf("INFO   height map %08x width %u (+0x15fc9c %u) range %f .. %f; camera %f %f %f, hero %f %f %f\n",
                    ee_read<uint32_t>(OV(0x0015fca8)), ee_read<uint32_t>(OV(0x0015fc98)), ee_read<uint32_t>(OV(0x0015fc9c)),
                    ee_read<float>(OV(0x0015fca0)), ee_read<float>(OV(0x0015fca4)), c.x, c.y, c.z, h.x, h.y, h.z);
    }
    // the level's record dealt: the wind, its timer (often about to run out), the counts, the kinds and the limits either side
    // of 0; the level's number (Hoven's motes on every level now and then); the camera moved and turned
    auto vary_record = [rec] {
        if (coin()) wfrand(rec + 0x20, 2, -0.3f, 0.3f);
        wf(rec + 0x30, coin() ? rf(-0.1f, 0.05f) : rf(0.0f, 2.0f));
        w32(rec + 0x34, ri(0, 5) == 0 ? u(ri(-2, 0)) : u(ri(1, 9)));
        if (coin()) w32(rec + 0x18, u(ri(-1, 3)));
        for (uint32_t o : {0x0u, 0xcu, 0x10u, 0x14u}) if (coin()) wf(rec + o, coin() ? -1.0f : rf(0.1f, 9.0f));
        if (coin()) wf(rec + 4, rf(-2.0f, 10.0f));
        if (coin()) wf(rec + 8, rf(1.0f, 20.0f));
        if (coin()) wf(rec + 0x1c, rf(0.0f, 30.0f));
        if (ri(0, 3) == 0) w32(0x0015ed84, coin() ? 0xcu : 0x200u);
        const uint32_t cam = OV(0x00166dc0);
        if (coin()) ee_ref<Vec4>(cam) = around(ee_ref<Vec4>(0x0013f3d0), 0.0f, 12.0f, -3.0f, 15.0f);
        if (coin()) { wf(cam + 0x14, rf(-1.0f, 1.0f)); wf(cam + 0x18, rf(-3.1f, 3.1f)); }
        vary_frame_scale();
        seed_rand();
    };
    for (int st : {0, 2, 1})
        classcheck("class1400_update", upd, subjects, class1400_update, [st, vary_record](uint32_t m) {
            vary_record();
            if (coin()) ee_ref<Vec4>(pvar(m) + 0x30) = around(ee_ref<Vec4>(OV(0x00166dc0)), 0.0f, 2.0f);
            w8(m + 0x20, u(st));
        }, st == 2 ? 120 : 30);
    helpercheck("class1400_wind", wind, 60, Ret::None, [vary_record] {
        vary_record();
        return Args{{u(ri(0, 1))}, {}};
    }, [](A a) { class1400_wind(a.i[0]); return uint64_t{0}; });
    // the particles: at a point about the hero, with a velocity, to a height
    auto particle_args = [vary_record](int lo, int hi) {
        vary_record();
        ee_ref<Vec4>(T) = around(ee_ref<Vec4>(0x0013f3d0), 0.0f, 10.0f, 0.0f, 10.0f);
        ee_ref<Vec4>(T + 0x10) = Vec4{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(-1.0f, 0.0f), rf(-1.0f, 1.0f)};
        return Args{{T, u(ri(lo, hi)), T + 0x10}, {rf(-5.0f, 5.0f)}};
    };
    helpercheck("class1400_drop", drop, 60, Ret::None, [particle_args] { return particle_args(-1, 3); },
                [](A a) { class1400_drop(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2], a.f[0]); return uint64_t{0}; });
    helpercheck("class1400_mote", mote, 60, Ret::None, [particle_args] { return particle_args(-1, 2); },
                [](A a) { class1400_mote(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2], a.f[0]); return uint64_t{0}; });
}

// ---- class 1885, the blob thrower (shared_class1885.cpp)
void class1885_checks() {
    const uint32_t upd = copy_here({{9, 0x0030ab80}, {13, 0x0030d550}});
    const uint32_t hit = copy_here({{9, 0x0030a9e8}, {13, 0x0030d3b8}});
    const uint32_t slot = copy_here({{9, 0x0030aae0}, {13, 0x0030d4b0}});
    const uint32_t thr = copy_here({{9, 0x0030b218}, {13, 0x0030dbe8}});
    const uint32_t disc = copy_here({{9, 0x00277758}, {13, 0x00263a58}});
    const uint32_t radius = copy_here({{9, 0x00277688}, {13, 0x00263988}});
    if (upd == 0) {
        for (const char* n : {"class1885_update", "class1885_hit", "class1885_free_slot", "class1885_throw", "rand_disc_offset",
                              "rand_radius", "class1885_fix25"})
            not_here(n);
        return;
    }
    auto subjects = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (!v.empty()) return v;
        for (int k = 0; k < 2; ++k) {
            const uint32_t s = moby_spawn(1885);
            if (s == 0) break;
            ee_ref<Vec4>(s + 0x10) = around(ee_ref<Vec4>(0x0013f3d0), 3.0f, 10.0f, 0.0f, 1.0f);
            v.push_back(s);
        }
        return v;
    };
    const uint32_t cams = ee_read<uint32_t>(OV(0x001600ec));
    if (info()) {
        level();
        std::printf("INFO class 1885: %zu placed, %zu subjects, groups %zu (%zu whole), cameras %08x\n", mobys_with(upd).size(),
                    subjects().size(), groups_with(1885).size(), whole_groups_with(1885).size(), cams);
        for (uint32_t m : subjects())
            std::printf("INFO   0x%08x state %u group %d box %d path %d most %u aim %u cams %d %d %d %d\n", m, ee_read<uint8_t>(m + 0x20),
                        ee_read<int32_t>(pvar(m) + 0x94), ee_read<int32_t>(pvar(m) + 0x84), ee_read<int32_t>(pvar(m) + 0x88),
                        ee_read<uint8_t>(pvar(m) + 0x90), ee_read<uint8_t>(pvar(m) + 0x92), ee_read<int32_t>(pvar(m) + 0xa0),
                        ee_read<int32_t>(pvar(m) + 0xa4), ee_read<int32_t>(pvar(m) + 0xa8), ee_read<int32_t>(pvar(m) + 0xac));
    }
    // the blobs a thrower has out: its list's slots none, a blob of its (live, or gone), or another moby
    auto deal_blobs = [](uint32_t m) {
        const uint32_t pv = pvar(m);
        const int mode = ri(0, 3);
        for (uint32_t k = 0; k < 20; ++k) {
            uint32_t b = 0;
            if (mode == 1 || (mode == 2 && coin()) || (mode == 3 && k < 19)) {
                b = moby_spawn(1886);
                if (b != 0) {
                    ee_ref<Vec4>(b + 0x10) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 6.0f);
                    if (ri(0, 4) == 0) w8(b + 0x20, coin() ? 0xfeu : 0xfdu);
                    if (ri(0, 6) == 0) w16(b + 0xa6, 1884);
                }
            }
            w32(pv + 0xb0 + k * 4, b);
        }
    };
    auto vary = [subjects, deal_blobs, cams](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        const Vec4 at = ee_ref<Vec4>(m + 0x10);
        give_hit(m, 0xc30000u, subjects());
        if (ee_read<uint8_t>(m + 0xa4) != 0xff) wf(OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6) + 0x2c, rf(0.0f, 6.0f));
        wf(pv + 0x20, ri(0, 2) == 0 ? rf(0.0f, 1.0f) : rf(1.0f, 8.0f));
        w16(pv + 0x8e, ri(0, 2) == 0 ? 1u : u(ri(0, 30)));
        w16(pv + 0x8c, u(ri(0, 90)));
        const int tk = ri(0, 4);
        uint32_t target = tk == 0 ? 0u : (tk == 1 ? ee_read<uint32_t>(0x001413d0) : pick(live_mobys()));
        if (target == m) target = 0;
        if (target != 0 && tk == 4) w8(target + 0x20, coin() ? 0xfeu : 0xfdu);
        w32(pv + 0x140, target);
        w32(pv + 0x144, ri(0, 3) == 0 ? 2u : 0u);
        if (coin()) w32(pv + 0x84, coin() ? 0xffffffffu : u(ri(0, 8)));
        if (coin()) w32(pv + 0x88, coin() ? 0xffffffffu : u(std::max(0, a_path())));
        wf(pv + 0x80, coin() ? rf(1.0f, 4.0f) : rf(4.0f, 25.0f));
        w8(pv + 0x90, u(ri(0, 6)));
        w8(pv + 0x92, u(ri(0, 1)));
        w8(pv + 0x93, u(ri(0, 3)));
        if (coin()) {
            const std::vector<uint32_t> gs = whole_groups_with(1885);
            w32(pv + 0x94, gs.empty() || ri(0, 3) == 0 ? 0xffffffffu : pick(gs));
        }
        // its four camera points: none, or indices of the level's cameras' list
        if (coin()) {
            const bool none = cams == 0 || coin();
            for (uint32_t k = 0; k < 4; ++k) w32(pv + 0xa0 + k * 4, none || coin() ? 0xffffffffu : u(ri(0, 5)));
        }
        if (coin()) deal_blobs(m);
        if (coin()) ee_ref<Vec4>(0x0013f3d0) = around(at, 0.0f, coin() ? 6.0f : 30.0f, -3.0f, 6.0f);
        if (coin()) ee_ref<Vec4>(pv + 0x100) = around(at, 0.0f, 20.0f, -3.0f, 3.0f);
        if (coin()) wf(m + 0x48, rf(-3.1f, 3.1f));
        // (half the time in state 1 ready to throw: its timer out, a target, room for a blob)
        if (st == 1 && coin()) {
            w16(pv + 0x8e, 1);
            w32(pv + 0x144, 0);
            w32(pv + 0x140, ee_read<uint32_t>(0x001413d0));
            w8(pv + 0x90, 0xff);
            if (coin()) for (uint32_t k = 0; k < 20; ++k) w32(pv + 0xb0 + k * 4, 0);
        }
        vary_frame_scale();
        w8(m + 0x20, u(st));
        seed_rand();
    };
    auto set_up = [](uint32_t m) {
        w8(m + 0x20, 0);
        const uint32_t sp0 = ee_stack_ptr();
        try { class1885_update(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
    };
    // (the debris' w lanes, left out of the update's check, whose death spawns three pieces: debris_lanes)
    without(debris_lanes(), [&] {
        for (int st : {0, 1, 2, 3})
            classcheck("class1885_update", upd, subjects, class1885_update, [st, vary, set_up](uint32_t m) {
                if (st != 0 && ri(0, 3) == 0) set_up(m);
                vary(m, st);
            }, st == 1 ? 200 : 40);
    });
    auto a_subject = [subjects, vary] {
        const uint32_t m = pick(subjects());
        vary(m, ri(0, 2));
        return m;
    };
    helpercheck("class1885_hit", hit, 80, Ret::None, [a_subject] {
        const uint32_t m = a_subject();
        return Args{{m, pvar(m)}, {}};
    }, [](A a) { class1885_hit(a.i[0], a.i[1]); return uint64_t{0}; });
    helpercheck("class1885_free_slot", slot, 60, Ret::Int32, [a_subject, deal_blobs] {
        const uint32_t m = a_subject();
        deal_blobs(m);
        return Args{{m, pvar(m)}, {}};
    }, [](A a) { return ri32(class1885_free_slot(a.i[0], a.i[1])); });
    helpercheck("class1885_throw", thr, 40, Ret::Int32, [a_subject] {
        const uint32_t m = a_subject();
        ee_ref<Vec4>(T) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 2.0f, 0.0f, 2.0f);
        ee_ref<Vec4>(T + 0x10) = Vec4{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(-0.1f, 0.4f), rf(-1.0f, 1.0f)};
        return Args{{m, T, T + 0x10, R() & 0x3ff, R() & 0x3ff}, {}};
    }, [](A a) { return r32(class1885_throw(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4])); });
    helpercheck("rand_disc_offset", disc, 40, Ret::None, [] {
        level();
        wfrand(T, 4, -9.0f, 9.0f);
        seed_rand();
        return Args{{T}, {rf(0.0f, 20.0f)}};
    }, [](A a) { rand_disc_offset(a.i[0], a.f[0]); return uint64_t{0}; });
    helpercheck("rand_radius", radius, 40, Ret::Float, [] {
        level();
        seed_rand();
        return Args{{}, {rf(-5.0f, 20.0f)}};
    }, [](A a) { return rf32(rand_radius(a.f[0])); });

    // fix 25 (1.02 Gameplay): the port with the fix against the 1.02 build's death, by hand (L13:0x30e880 on the PAL disc 2.00:
    // on Gemlik Base (0x15ed84 13) with Ratchet's height within 4 of 250, moby_drop_bolts' flag 0x200), and against the port
    // without it elsewhere
    if (const char* only = std::getenv("ORACLE_ONLY"); only == nullptr || std::strcmp(only, "class1885_fix25") == 0) {
        if (std::getenv("RAC1_GAMEPLAY102") == nullptr) {
            std::printf("%-26s skipped (run with RAC1_GAMEPLAY102=25)\n", "class1885_fix25");
            ++g_skipped;
        } else {
            int bad = 0, n = 0, flagged = 0;
            static std::vector<uint8_t> start, with_fix;
            const std::unordered_set<uint32_t> ignored(g_ignored_words.begin(), g_ignored_words.end());
            for (int c = 0; c < 60; ++c) {
                level();
                const uint32_t m = a_subject();
                const uint32_t pv = pvar(m);
                // (a hit that kills it, from a moby; Ratchet at 250 or about it; the level's number Gemlik Base's or not)
                const uint32_t e = OV(0x00178100) + (5u << 6);
                w8(m + 0xa4, 5);
                w32(e + 0x34, m);
                w32(e + 0x24, 0xc30000u);
                w32(e + 0x20, pick(live_mobys()));
                wf(e + 0x2c, rf(1.0f, 10.0f));
                wf(pv + 0x20, coin() ? rf(0.0f, 1.0f) : rf(1.0f, 20.0f));
                wf(0x0013f3d8, 250.0f + (coin() ? rf(-3.9f, 3.9f) : rf(-10.0f, 10.0f)));
                w32(0x0015ed84, ri(0, 3) == 0 ? 9u : 13u);
                seed_rand();
                uint8_t* ram = ee_get_ram_base();
                start.assign(ram, ram + 0x02000000);
                g_advanced_disabled = false;
                class1885_hit(m, pv);
                g_advanced_disabled = true;
                with_fix.assign(ram, ram + 0x02000000);
                std::memcpy(ram, start.data(), start.size());
                // the 1.02 build's function, by hand
                {
                    const uint32_t h = sub_0025a420(m, 0xc30000u, 0);
                    int32_t st = 0;
                    moby_hit_react_r(m, h, pv + 0x20, 0, st, nullptr, nullptr, 4);
                    if (st >= 2) {
                        const float health = ee_read<float>(pv + 0x20) - ee_read<float>(h + 0x2c);
                        wf(pv + 0x20, health);
                        if (0.0f < health) {
                            w8(pv + 0x67, 0x5a);
                        } else {
                            const bool flag = ee_read<int32_t>(0x0015ed84) == 0xd && std::fabs(ee_read<float>(0x0013f3d8) - 250.0f) < 4.0f;
                            if (flag) ++flagged;
                            moby_drop_bolts(m, flag ? 0x200u : 0u, -1);
                            w8(m + 0x20, 2);
                            w8(pv + 0x67, 0x78);
                        }
                        color_pulse_start(m, pv + 0x60);
                    }
                    w8(m + 0xa4, 0xff);
                    color_pulse_update(m, pv + 0x60);
                }
                ++n;
                for (uint32_t a = 0x00100000; a < 0x02000000; a += 4) {
                    if (ignored.count(a) != 0) continue;
                    if (a >= ee_stack_ptr() - 0x10000 && a < ee_stack_ptr() + 0x10000) continue;  // (the port's stack frames)
                    uint32_t x, y;
                    std::memcpy(&x, &with_fix[a], 4); std::memcpy(&y, &ram[a], 4);
                    if (x != y) { if (bad < 8) std::printf("  class1885_fix25 case %d: %08x fix=%08x expected=%08x\n", c, a, x, y); ++bad; break; }
                }
            }
            std::printf("%-26s %s (%d cases, %d with the bolts' flag)\n", "class1885_fix25", bad == 0 && n > 0 && flagged > 0 ? "ok" : "FAIL", n, flagged);
            if (bad != 0 || n == 0 || flagged == 0) ++g_fail;
        }
    }
}

// ---- class 1511's update and its remains (shared_class1511.cpp)
void class1511_checks() {
    const uint32_t upd = copy_here({{5, 0x0031c8e0}, {14, 0x00307ad8}});
    const uint32_t rem = copy_here({{5, 0x0031d160}, {14, 0x00308358}});
    const uint32_t words = copy_here({{5, 0x00161fc8}, {14, 0x0016229c}});
    if (upd == 0) {
        for (const char* n : {"class1511_update", "class1511_remains"}) not_here(n);
        return;
    }
    auto subjects = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (!v.empty()) return v;
        for (int k = 0; k < 3; ++k) {
            const uint32_t s = moby_spawn(1511);
            if (s == 0) break;
            ee_ref<Vec4>(s + 0x10) = around(ee_ref<Vec4>(0x0013f3d0), 2.0f, 10.0f, 0.0f, 1.0f);
            v.push_back(s);
        }
        return v;
    };
    if (info()) {
        level();
        std::printf("INFO class 1511: %zu placed, %zu subjects\n", mobys_with(upd).size(), subjects().size());
    }
    // the camera about it (in the fade's 32 .. 48 or not); a hit: a blast, at its glow's point, or the wrench's from by it
    auto vary = [words](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        const Vec4 at = ee_ref<Vec4>(m + 0x10);
        const int ck = ri(0, 3);
        ee_ref<Vec4>(OV(0x00166dc0)) = around(at, ck == 0 ? 30.0f : (ck == 1 ? 45.0f : 0.0f), ck == 0 ? 50.0f : (ck == 1 ? 50.0f : 32.0f));
        wrand(pv + 0x64, 3);
        w8(pv + 0x6f, R() & 0xff);
        if (ri(0, 4) != 0) {
            const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
            w8(m + 0xa4, idx);
            w32(e + 0x34, m);
            const int hk = ri(0, 3);
            w32(e + 0x24, (hk == 0 ? 0x800000u : 0x10000u) | (coin() ? 0u : (R() & 0x7f00ffu)));
            Vec4 glow = ee_ref<Vec4>(m + 0xe0);
            const float k = ee_read<float>(words);
            glow = Vec4{at.x + glow.x * k, at.y + glow.y * k, at.z + glow.z * k, at.w};
            ee_ref<Vec4>(e) = hk == 1 ? around(glow, 0.0f, 1.0f, -0.4f, 0.4f) : around(at, 0.0f, 5.0f);
            uint32_t src = pick(live_mobys());
            if (src == m) src = ee_read<uint32_t>(0x001413d0);
            if (hk >= 2 && coin()) w16(src + 0xa6, 0x47);
            if (hk == 3) ee_ref<Vec4>(src + 0x10) = around(glow, 0.0f, 1.0f, -0.4f, 0.4f);
            w32(e + 0x20, src);
        } else {
            w8(m + 0xa4, 0xff);
        }
        vary_frame_scale();
        w8(m + 0x20, u(st));
        seed_rand();
    };
    without(debris_lanes(), [&] {  // (its pieces: debris_lanes)
        for (int st : {0, 1})
            classcheck("class1511_update", upd, subjects, class1511_update, [st, vary](uint32_t m) { vary(m, st); }, 80);
    });
    helpercheck("class1511_remains", rem, 30, Ret::Int32, [subjects, vary] {
        const uint32_t m = pick(subjects());
        vary(m, 1);
        w32(m + 0x30, R());
        return Args{{m}, {}};
    }, [](A a) { return r32(class1511_remains(a.i[0])); });
}

// ---- the rafts' next stage and their arrivals (shared_floaters.cpp)
void floater_stage_checks() {
    const uint32_t next = copy_here({{5, 0x00319510}, {14, 0x002f1268}, {16, 0x002e4b58}});
    const uint32_t arrive = copy_here({{5, 0x00319208}, {14, 0x002f1040}, {16, 0x002e4900}});
    const uint32_t findc = copy_here({{14, 0x002f12f0}});
    if (next == 0) {
        for (const char* n : {"floater_next_stage", "floater_arrive", "floater_find_checked"}) not_here(n);
        return;
    }
    const int lv = g_oracle_level;
    const uint32_t cls = lv == 5 ? 998u : lv == 14 ? 712u : 1410u;
    const uint32_t stage = lv == 16 ? 0xacu : 0xb4u;
    const uint32_t points = lv == 16 ? 3u : 5u;
    auto rafts = [cls] {
        std::vector<uint32_t> v;
        for (uint32_t m = first_moby(); m < end_moby(); m += 0x100)
            if (live(m) && ee_read<uint16_t>(m + 0xa6) == cls) v.push_back(m);
        if (v.empty()) {
            const uint32_t s = moby_spawn(cls);
            if (s != 0) { ee_ref<Vec4>(s + 0x10) = ee_ref<Vec4>(0x0013f3d0); v.push_back(s); }
        }
        return v;
    };
    const uint32_t cams = ee_read<uint32_t>(OV(0x001600ec));
    if (info()) {
        level();
        std::printf("INFO rafts (class %u): %zu, cameras %08x\n", cls, rafts().size(), cams);
        for (uint32_t m : rafts())
            std::printf("INFO   0x%08x group %u stage %d next %d points %d %d %d %d %d\n", m, ee_read<uint8_t>(m + 0x21),
                        ee_read<int16_t>(pvar(m) + stage), ee_read<int16_t>(pvar(m) + stage + 2), ee_read<int32_t>(pvar(m) + 0x80),
                        ee_read<int32_t>(pvar(m) + 0x84), ee_read<int32_t>(pvar(m) + 0x88), ee_read<int32_t>(pvar(m) + 0x8c),
                        ee_read<int32_t>(pvar(m) + 0x90));
    }
    // a raft's camera points (indices 0 .. 5 of the level's list, so that the rafts share some), its stages, the progress
    // flags Rilgar's arrival reads, the flag 0x13d4e1 Kalebo III's reads
    auto deal_raft = [stage, points](uint32_t m) {
        const uint32_t pv = pvar(m);
        for (uint32_t k = 0; k < points; ++k) w32(pv + 0x80 + k * 4, u(ri(0, 5)));
        w16(pv + stage, u(ri(0, 2) == 0 ? -1 : ri(0, static_cast<int>(points) - 1)));
        w32(pv + 0xb8, u(ri(0, 15)));
        w32(pv + 0xbc, u(ri(0, 15)));
    };
    auto vary_world = [] {
        w8(0x0013d4e1, u(ri(0, 1)));
        for (uint32_t k = 0; k < 16; ++k) w8(0x0014c050u + (ee_read<uint32_t>(0x0015ed84) << 4) + k, coin() ? 0xffu : u(ri(0, 3)));
    };
    // its group's members dealt too, one of them the next stage's (its key the raft's next), at a camera point or about it
    // (Oltanis's raft has no group: there it is given one of the level's, half the time)
    std::vector<uint32_t> groups;
    level();
    for (uint32_t g = 0; g < 0x100 && static_cast<int32_t>(g) < ee_read<int32_t>(OV(0x0015fff4)); ++g) {
        uint32_t e = ee_read<uint32_t>(OV(0x001ab840) + g * 4);
        if (e < 0x00100000u || e >= 0x02000000u) continue;
        for (int k = 0; k < 64; ++k, e += 2) {
            const uint16_t h = ee_read<uint16_t>(e);
            const uint32_t o = first_moby() + ((h & 0x7fffu) << 8);
            if (o >= end_moby() || ee_read<uint32_t>(o + 0x78) == 0) break;
            if ((h & 0x8000u) != 0) { groups.push_back(g); break; }
        }
    }
    auto vary = [rafts, deal_raft, vary_world, stage, cams, groups](uint32_t m) {
        const uint32_t pv = pvar(m);
        vary_world();
        deal_raft(m);
        if (ee_read<uint8_t>(m + 0x21) == 0xff && !groups.empty() && coin()) w8(m + 0x21, groups[static_cast<size_t>(ri(0, static_cast<int>(groups.size()) - 1))]);
        const uint32_t list = ee_read<uint32_t>(OV(0x001ab840) + ee_read<uint8_t>(m + 0x21) * 4u);
        w16(pv + stage + 2, u(ri(-1, 4)));
        if (list >= 0x00100000u && list < 0x02000000u) {
            for (uint32_t e = list, k = 0; k < 32; e += 2, ++k) {
                const uint16_t h = ee_read<uint16_t>(e);
                const uint32_t o = first_moby() + ((h & 0x7fffu) << 8);
                if (o >= end_moby()) break;
                if (o != m && ee_read<uint32_t>(o + 0x78) != 0) {
                    deal_raft(o);
                    if (coin()) w16(pvar(o) + stage, ee_read<uint16_t>(pv + stage + 2));
                    if (cams != 0 && coin()) ee_ref<Vec4>(o + 0x10) = around(ee_ref<Vec4>(cams + (u(ri(0, 5)) << 7) + 0x30), 0.0f, 1.5f, -0.5f, 0.5f);
                }
                if ((h & 0x8000u) != 0) break;
            }
        }
        seed_rand();
    };
    for (int pass = 0; pass < 2; ++pass)
        classcheck("floater_next_stage", next, rafts, lv == 5 ? +[](uint32_t m) { floater_next_stage(m, 0xb4); }
                                                    : lv == 14 ? +[](uint32_t m) { floater_next_stage(m, 0xb4); }
                                                               : +[](uint32_t m) { floater_next_stage(m, 0xac); },
                   vary, 60);
    // the arrival on a raft put at one of its camera points or about one
    classcheck("floater_arrive", arrive, rafts, lv == 5 ? floater_arrive_l5 : lv == 14 ? floater_arrive_l14 : floater_arrive_l16,
               [deal_raft, vary_world, cams](uint32_t m) {
        vary_world();
        deal_raft(m);
        if (cams != 0 && ri(0, 3) != 0) {
            const uint32_t idx = ee_read<uint32_t>(pvar(m) + 0x80 + u(ri(0, 2)) * 4);
            ee_ref<Vec4>(m + 0x10) = around(ee_ref<Vec4>(cams + (idx << 7) + 0x30), 0.0f, 1.5f, -0.5f, 0.5f);
        }
        seed_rand();
    }, 80);
    if (findc != 0)
        fcheck("floater_find_checked", findc, 60, Ret::Int32, [rafts] {
            level();
            const std::vector<uint32_t> rs = rafts();
            const uint32_t m = coin() ? pick(live_mobys()) : pick(rs);
            if (ri(0, 4) == 0) w8(m + 0x21, coin() ? 0xffu : u(ri(0, 80)));
            if (ri(0, 3) == 0) w32(OV(0x0015fff4), u(ri(0, std::max(0, ee_read<int32_t>(OV(0x0015fff4))))));  // (fewer groups)
            const uint32_t list = ee_read<uint32_t>(OV(0x001ab840) + ee_read<uint8_t>(m + 0x21) * 4u);
            int32_t value = ri(-1, 6);
            if (list >= 0x00100000u && list < 0x02000000u && coin()) {
                const uint32_t o = first_moby() + ((ee_read<uint16_t>(list + 2u * u(ri(0, 1))) & 0x7fffu) << 8);
                if (o < end_moby() && ee_read<uint32_t>(o + 0x78) != 0) value = ee_read<int16_t>(ee_read<uint32_t>(o + 0x78) + 0xb4);
            }
            return Args{{m, u(value)}, {}};
        }, [](A a) { return r32(floater_find_checked(a.i[0], static_cast<int32_t>(a.i[1]), 0xb4)); });
    else
        not_here("floater_find_checked");
}

// ---- the sea, class 1139 and the puff of particle 0x34 (shared_seas.cpp)
void seas_checks() {
    const uint32_t sea = copy_here({{7, 0x002f8058}, {9, 0x002ef7f8}});
    const uint32_t c1139 = copy_here({{5, 0x0031abe0}, {16, 0x002e0f78}});
    const uint32_t puff1 = copy_here({{10, 0x002e3de8}, {12, 0x002e3420}, {13, 0x003072c0}});
    const uint32_t puff2 = copy_here({{13, 0x0030c2b0}});
    if (sea != 0) {
        for (int st : {0, 1, 2})
            classcheck("sea_update", sea, [sea] {
                std::vector<uint32_t> v = mobys_with(sea);
                if (v.empty()) v.push_back(pick(live_mobys()));
                return v;
            }, sea_update, [st](uint32_t m) {
                w8(m + 0x30, R() & 0xff);
                w8(m + 0x20, u(st));
                seed_rand();
            }, 20);
    } else {
        not_here("sea_update");
    }
    if (c1139 != 0) {
        auto subjects = [c1139] {
            std::vector<uint32_t> v = mobys_with(c1139);
            if (!v.empty()) return v;
            const uint32_t s = moby_spawn(1139);
            if (s != 0) { ee_ref<Vec4>(s + 0x10) = around(ee_ref<Vec4>(0x0013f3d0), 2.0f, 8.0f, 0.0f, 2.0f); v.push_back(s); }
            return v;
        };
        // (on screen or not: the camera by it or turned away; the hero's state 0x16 or another)
        for (int st : {0, 1, 2})
            classcheck("class1139_update", c1139, subjects, class1139_update, [st](uint32_t m) {
                if (coin()) ee_ref<Vec4>(m + 0x10) = around(ee_ref<Vec4>(OV(0x00166dc0)), 2.0f, coin() ? 10.0f : 80.0f, -3.0f, 3.0f);
                w32(0x001413dc, ri(0, 2) == 0 ? u(ri(0, 0x30)) : 0x16u);
                w16(m + 0x34, R() & 0xffff);
                vary_frame_scale();
                w8(m + 0x20, u(st));
                seed_rand();
            }, st == 1 ? 60 : 20);
    } else {
        not_here("class1139_update");
    }
    auto puff_args = [] {
        level();
        const uint32_t m = pick(live_mobys());
        seed_rand();
        return Args{{m}, {}};
    };
    if (puff1 != 0)
        helpercheck("particle34_puff_1", puff1, 40, Ret::Int32, puff_args, [](A a) { return r32(particle34_puff_1(a.i[0])); });
    else
        not_here("particle34_puff_1");
    if (puff2 != 0)
        helpercheck("particle34_puff_2", puff2, 40, Ret::Int32, puff_args, [](A a) { return r32(particle34_puff_2(a.i[0])); });
    else
        not_here("particle34_puff_2");
}

// ---- class 50's update (shared_class49.cpp)
void class50_checks() {
    const uint32_t upd = copy_here({{16, 0x002a0a98}, {18, 0x002a7b90}});
    if (upd == 0) {
        not_here("class50_update");
        return;
    }
    // shots spawned (class50_spawn) by a live moby, flying at speeds about 0.1 .. 1 toward a target about them
    auto shots = [] {
        std::vector<uint32_t> v;
        const std::vector<uint32_t> all = live_mobys();
        for (int k = 0; k < 3; ++k) {
            const uint32_t owner = pick(all);
            ee_ref<Vec4>(T) = around(ee_ref<Vec4>(0x0013f3d0), 1.0f, 8.0f, 0.0f, 2.0f);
            const float a = rf(-3.1f, 3.1f), sp = rf(0.1f, 1.0f);
            ee_ref<Vec4>(T + 0x10) = Vec4{sp * std::cos(a), sp * std::sin(a), rf(-0.2f, 0.2f), 0.0f};
            ee_ref<Vec4>(T + 0x20) = around(ee_ref<Vec4>(T), 0.5f, 12.0f, -2.0f, 2.0f);
            const uint32_t s = class50_spawn(owner, T + 0x10, T, T + 0x20, ri(1, 60));
            if (s == 0) break;
            v.push_back(s);
        }
        return v;
    };
    auto vary = [](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        const Vec4 at = ee_ref<Vec4>(m + 0x10);
        w32(pv + 0x34, u(ri(0, 1)));
        if (coin()) ee_ref<Vec4>(OV(0x00166dc0)) = around(at, 0.0f, coin() ? 15.0f : 40.0f);
        const int tk = ri(0, 3);
        w32(pv + 0x20, tk == 0 ? 0u : (tk == 1 ? 1u : u(ri(2, 60))));
        w32(pv + 0x30, u(ri(1, 60)));
        if (coin()) wf(pv + 0x24, rf(0.05f, 2.0f));
        if (coin()) ee_ref<Vec4>(pv + 0x10) = around(at, coin() ? 0.0f : 1.0f, coin() ? 1.5f : 20.0f, -3.0f, 3.0f);
        if (coin()) wfrand(pv + 0x28, 2, -0.6f, 0.6f);
        w8(0x001413f4, ri(0, 2) == 0 ? 2u : u(ri(0, 1)));
        vary_frame_scale();
        w8(m + 0x20, u(st));
        seed_rand();
    };
    // (the segment record 0x173e90 .. 0x173ebc the line test leaves from VU0 registers in the original, which segment_collide's
    // port does not reproduce (as batches 240 / 242 / 372 / 514): left out of the update's check)
    std::vector<uint32_t> seg;
    for (uint32_t k = 0; k < 0x30; k += 4) seg.push_back(OV(0x00173e90) + k);
    without(seg, [&] {
        for (int st : {0, 1})
            classcheck("class50_update", upd, shots, class50_update, [st, vary](uint32_t m) { vary(m, st); }, st == 0 ? 100 : 20);
    });
}

// ---- class 886, a checkpoint pad (shared_class886.cpp)
void class886_checks() {
    const uint32_t upd = copy_here({{7, 0x0030bf90}, {12, 0x002ff838}});
    if (upd == 0) {
        not_here("class886_update");
        return;
    }
    auto pads = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (!v.empty()) return v;
        const uint32_t s = moby_spawn(886);
        if (s != 0) { ee_ref<Vec4>(s + 0x10) = around(ee_ref<Vec4>(0x0013f3d0), 1.0f, 4.0f, 0.0f, 0.5f); v.push_back(s); }
        return v;
    };
    if (info()) {
        level();
        std::printf("INFO class 886: %zu placed, groups %zu (%zu whole)\n", mobys_with(upd).size(), groups_with(886).size(),
                    whole_groups_with(886).size());
    }
    auto vary = [](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        const uint32_t P = 0x0013f350;
        w32(pv + 8, ri(0, 2) == 0 ? 1u : 0u);
        // (the hero on it, or about it crouched (state 0x11) with the pad tilted, or elsewhere)
        const int hk = ri(0, 3);
        w16(P + 0x30e, hk == 1 ? 1u : 0u);
        w32(P + 0x2fc, hk == 0 ? m : (coin() ? 0u : pick(live_mobys())));
        if (hk >= 2) {
            wf(m + 0x44, coin() ? rf(0.8f, 3.0f) * (coin() ? 1.0f : -1.0f) : rf(-0.7f, 0.7f));
            ee_ref<Vec4>(P + 0x80) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 1.0f : 2.5f, -1.2f, 1.2f);
            w32(P + 0x2084, coin() ? 0x11u : u(ri(0, 0x20)));
            wf(P + 0xaa8, rf(0.0f, 10.0f));
        }
        w8(m + 0xbc, u(ri(0, 1)));
        w32(pv, ri(0, 2) == 0 ? u(ri(0, 1)) : u(ri(2, 3000)));
        w32(pv + 4, ri(0, 2) == 0 ? 1u : u(ri(0, 40)));
        w32(pv + 0x10, u(ri(-1, 30)));
        w16(pv + 0xc, ri(0, 2) == 0 ? 1u : u(ri(0, 20)));
        w16(pv + 0xe, u(ri(-1, 7)));
        // (its collected flag: none, set or not)
        const uint32_t idx = coin() ? 0xffu : u(ri(0, 15));
        w8(m + 0xb0, idx);
        if (idx != 0xff) w8(0x0014c050u + idx + (ee_read<uint32_t>(0x0015ed84) << 4), coin() ? 0xffu : 0u);
        if (coin()) {
            const std::vector<uint32_t> gs = whole_groups_with(886);
            w8(m + 0x21, gs.empty() || ri(0, 3) == 0 ? 0xffu : pick(gs));
        }
        vary_frame_scale();
        w8(m + 0x20, u(st));
        seed_rand();
    };
    for (int st = 0; st < 10; ++st)
        classcheck("class886_update", upd, pads, class886_update, [st, vary](uint32_t m) { vary(m, st); }, st == 1 || st == 7 ? 60 : 30);
}

// ---- ship 1 of Aridia and Gaspar, class 1212 (shared_bg_ships.cpp)
void bg_ship1_checks() {
    const uint32_t upd = copy_here({{2, 0x002ec4b8}, {9, 0x00305dc0}});
    if (upd == 0) {
        not_here("bg_ship1_update");
        return;
    }
    auto ships = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (!v.empty()) return v;
        const uint32_t s = moby_spawn(1212);
        if (s != 0) { w32(pvar(s) + 0x60, u(std::max(0, a_path()))); v.push_back(s); }
        return v;
    };
    if (info()) {
        level();
        std::printf("INFO ship 1: %zu placed\n", mobys_with(upd).size());
    }
    auto vary = [](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        if (ri(0, 4) == 0) w32(pv + 0x60, coin() ? 0xffffffffu : u(std::max(0, a_path())));
        if (coin()) wf(pv + 0x64, rf(0.0f, 12.0f));
        if (coin()) wf(pv + 0x70, rf(0.5f, 40.0f));
        if (coin()) wf(pv + 0x74, rf(0.0f, 1.0f));
        w32(pv + 0x6c, coin() ? 0xffffffffu : u(ri(0, 15)));
        if (coin()) wf(pv + 0x68, rf(-3.1f, 3.1f));
        w8(m + 0x31, u(ri(0, 1)));
        if (ri(0, 2) == 0) {  // (a hit with damage, or none)
            const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
            w8(m + 0xa4, idx);
            w32(e + 0x34, m);
            w32(e + 0x24, 0x10000u | (R() & 0xff));
            wf(e + 0x2c, coin() ? 0.0f : rf(0.1f, 5.0f));
        } else {
            w8(m + 0xa4, 0xff);
        }
        // (the explosions' counters and their sounds' flags, about the third / fifth; the hero's state 0x32 or not)
        w32(0x0015edf4, u(ri(0, 4)));
        w32(0x0015edfc, u(ri(2, 6)));
        w8(0x0013d40a, u(ri(0, 1)));
        w8(0x0013d417, u(ri(0, 1)));
        w32(0x001413d4, coin() ? 0x32u : u(ri(0, 0x40)));
        if (coin()) ee_ref<Vec4>(m + 0x10) = Vec4{rf(0.0f, 16.0f), rf(0.0f, 16.0f), rf(0.0f, 50.0f), 1.0f};
        vary_frame_scale();
        w8(m + 0x20, u(st));
        seed_rand();
    };
    auto set_up = [](uint32_t m) {
        w8(m + 0x20, 0);
        const uint32_t sp0 = ee_stack_ptr();
        try { bg_ship1_update(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
    };
    without(debris_lanes(), [&] {  // (its pieces: debris_lanes)
        for (int st : {0, 1, 2, 3})
            classcheck("bg_ship1_update", upd, ships, bg_ship1_update, [st, vary, set_up](uint32_t m) {
                if (st != 0 && ri(0, 3) != 0) set_up(m);
                vary(m, st);
            }, st == 1 ? 80 : 40);
    });
}

// ---- class 655's draw and its strips (shared_class655_draw.cpp), the finale's glows (shared_class655_glows.cpp)
void class655_draw_checks() {
    const uint32_t cb = copy_here({{15, 0x002d8710}, {17, 0x002d6d88}, {18, 0x002eb9f8}});
    const uint32_t strip = copy_here({{15, 0x002d8878}, {17, 0x002d6ef0}, {18, 0x002ebb60}});
    const uint32_t glows18 = copy_here({{18, 0x002ec130}});
    const uint32_t buf = copy_here({{15, 0x001d3750}, {17, 0x001d39d0}, {18, 0x001d9d00}});
    const uint32_t rec = copy_here({{15, 0x00161c54}, {17, 0x00161ae4}, {18, 0x00162048}});
    if (cb == 0) {
        for (const char* n : {"class655_draw", "class655_strip", "class655_glows_l18"}) not_here(n);
        return;
    }
    auto subjects = [] {
        std::vector<uint32_t> v;
        for (uint32_t m = first_moby(); m < end_moby(); m += 0x100)
            if (live(m) && ee_read<uint16_t>(m + 0xa6) == 655) v.push_back(m);
        if (v.empty() && g_oracle_level != 18) {
            const uint32_t s = moby_spawn(655);
            if (s != 0) v.push_back(s);
        }
        if (v.empty()) v = live_mobys();  // (Veldin's finale has none and no class 655 to spawn: its glows read only the place)
        return v;
    };
    if (info()) {
        level();
        std::printf("INFO class 655: %zu, first strip point %f %f %f\n", subjects().size(), ee_read<float>(buf), ee_read<float>(buf + 4),
                    ee_read<float>(buf + 8));
    }
    // the view: the camera before the strips' points (looking at one), near or far; the scroll about its wrap
    auto view = [buf, rec](uint32_t at) {
        const Vec4 p = ee_ref<Vec4>(at);
        const int k = ri(0, 3);
        if (k != 0) {
            Vec4& cam = ee_ref<Vec4>(OV(0x00166dc0));
            cam = around(p, k == 1 ? 1.0f : 5.0f, k == 1 ? 5.0f : 40.0f, -3.0f, 6.0f);
            const float yaw = std::atan2(p.y - cam.y, p.x - cam.x);
            wf(OV(0x00166dc0) + 0x18, yaw);
            wf(OV(0x00166dc0) + 0x14, rf(-0.3f, 0.3f));
        }
        if (coin()) wf(rec, coin() ? rf(-8.5f, -7.5f) : rf(-8.0f, 8.0f));
        if (coin()) wf(rec + 4, rf(-0.5f, 0.5f));
        render_view_setup();
        packet_cursor();
        vary_frame_scale();
        seed_rand();
        (void)buf;
    };
    // (the strips' points: the level's buffer is empty until its class runs, so half the time it gets lines of points in
    // front of the camera, each run its own line)
    auto fill_lines = [](uint32_t at, uint32_t n) {
        const Vec4 c = ee_ref<Vec4>(OV(0x00166dc0)), f = ee_ref<Vec4>(OV(0x00166fd0));
        Vec4 p{c.x + f.x * rf(3.0f, 15.0f), c.y + f.y * rf(3.0f, 15.0f), c.z + f.z * rf(3.0f, 15.0f) + rf(-1.0f, 1.0f), 1.0f};
        const Vec4 d{rf(-0.6f, 0.6f), rf(-0.6f, 0.6f), rf(-0.3f, 0.3f), 0.0f};
        for (uint32_t k = 0; k < n; ++k, at += 0x10) {
            ee_ref<Vec4>(at) = p;
            p = Vec4{p.x + d.x + rf(-0.1f, 0.1f), p.y + d.y + rf(-0.1f, 0.1f), p.z + d.z + rf(-0.1f, 0.1f), 1.0f};
        }
    };
    dcheck("class655_draw", cb, 40, [subjects, view, buf, fill_lines] {
        level();
        const uint32_t m = pick(subjects());
        view(buf + u(ri(0, 0x5f)) * 0x10);
        if (coin()) {
            fill_lines(buf, 20);
            fill_lines(buf + 0x140, 20);
            for (uint32_t r : {0x460u, 0x4b0u, 0x500u, 0x550u}) fill_lines(buf + r, 5);
            render_view_setup();
        }
        if (coin()) ee_ref<Vec4>(pvar(m) + 0xf0) = around(ee_ref<Vec4>(OV(0x00166dc0)), 2.0f, 10.0f);
        return Args{{m}, {}};
    }, [](A a) { class655_draw(a.i[0]); return uint64_t{0}; });
    // a strip: the level's points (one of the six runs) or points made about the camera; counts from 0 to 20
    dcheck("class655_strip", strip, 80, [view, buf] {
        level();
        static const uint32_t runs[] = {0, 0x140, 0x460, 0x4b0, 0x500, 0x550};
        uint32_t pts = buf + runs[ri(0, 5)];
        if (ri(0, 2) != 0) {
            pts = T;
            const Vec4 c = ee_ref<Vec4>(OV(0x00166dc0));
            for (uint32_t k = 0; k < 20; ++k) ee_ref<Vec4>(T + k * 0x10) = around(c, 2.0f, 12.0f, -3.0f, 3.0f);
            if (coin()) ee_ref<Vec4>(T + 0x10) = ee_ref<Vec4>(T);  // (a leg of length 0)
        }
        view(pts + u(ri(0, 3)) * 0x10);
        const int k = ri(0, 3);
        const int32_t count = k == 0 ? ri(0, 3) : (k == 1 ? 5 : 20);
        return Args{{pts, R() & 0xff, R() & 0xff, u(count)}, {rf(0.0f, 1.0f), rf(0.01f, 1.5f), rf(0.0f, 0.5f)}};
    }, [](A a) { class655_strip(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]), a.f[1], a.f[2]); return uint64_t{0}; });
    if (glows18 != 0)
        dcheck("class655_glows_l18", glows18, 40, [subjects] {
            level();
            const uint32_t m = pick(subjects());
            const Vec4 c = ee_ref<Vec4>(OV(0x00166dc0)), f = ee_ref<Vec4>(OV(0x00166fd0));
            if (ri(0, 3) != 0) {
                const float r = rf(2.0f, 20.0f);
                ee_ref<Vec4>(m + 0x10) = Vec4{c.x + r * f.x, c.y + r * f.y, c.z + r * f.z, 1.0f};
            }
            render_view_setup();
            packet_cursor();
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { class655_glows_l18(a.i[0]); return uint64_t{0}; });
    else
        not_here("class655_glows_l18");
}

// ---- Hoven's and Oltanis's sky-reflecting objects (shared_env_objects.cpp)
void env_objects_checks() {
    const uint32_t cb = copy_here({{12, 0x002e71b0}, {14, 0x00307a28}});
    const uint32_t draw = copy_here({{12, 0x002bc210}, {14, 0x002ab3e8}});
    const uint32_t uvs = copy_here({{12, 0x002667fc}, {14, 0x0025602c}});
    const uint32_t recs = copy_here({{12, 0x001f5480}, {14, 0x001f5f80}});
    const int32_t count = g_oracle_level == 14 ? 0x13 : 0x2f;
    if (cb == 0) {
        for (const char* n : {"env_objects_callback", "env_objects_draw", "env_objects_uvs"}) not_here(n);
        return;
    }
    // the view: the camera before one of the records' spheres (near or far), or where it is
    auto view = [recs, count] {
        if (ri(0, 3) != 0) {
            const Vec4 c = ee_ref<Vec4>(recs + u(ri(0, count - 1)) * 0x20);
            Vec4& cam = ee_ref<Vec4>(OV(0x00166dc0));
            cam = around(c, coin() ? 2.0f : 10.0f, coin() ? 10.0f : 60.0f, -3.0f, 10.0f);
            wf(OV(0x00166dc0) + 0x18, std::atan2(c.y - cam.y, c.x - cam.x));
            wf(OV(0x00166dc0) + 0x14, rf(-0.4f, 0.4f));
        }
        render_view_setup();
        packet_cursor();
        seed_rand();
    };
    if (info()) {
        level();
        const Vec4 c = ee_ref<Vec4>(recs);
        std::printf("INFO env objects: %d records, the first at %f %f %f (r %f), %d points\n", count, c.x, c.y, c.z, c.w,
                    ee_read<int32_t>(recs + 0x1c));
    }
    // (B517_INFO: the cases with a record on screen, so that its meshes reach the packets past the matrices' set-up)
    static int seen_cases = 0, all_cases = 0;
    auto count_seen = [](uint32_t first, int32_t n) {
        bool any = false;
        for (int32_t k = 0; k < n; ++k) any = any || point_on_screen(first + u(k) * 0x20, 256.0f) != -1;
        ++all_cases;
        if (any) ++seen_cases;
    };
    seen_cases = all_cases = 0;
    dcheck("env_objects_callback", cb, 40, [view, count_seen, recs, count] {
        level();
        view();
        count_seen(recs, count);
        return Args{{pick(live_mobys())}, {}};
    }, [](A a) { env_objects_callback(a.i[0]); return uint64_t{0}; }, 0x10);  // (its first tag pair, always written)
    dcheck("env_objects_draw", draw, 40, [view, recs, count, count_seen] {
        level();
        view();
        const uint32_t first = u(ri(0, count - 1));
        const int32_t n = ri(0, count - static_cast<int32_t>(first));
        count_seen(recs + first * 0x20, n);
        const uint64_t ta = coin() ? effect_tex0(ri(0, 0x40)) : (static_cast<uint64_t>(R()) << 32 | R()), tb = effect_tex0(ri(0, 0x40));
        g_wide_regs = {{6, ta}, {7, tb}};
        return Args{{recs + first * 0x20, u(n), static_cast<uint32_t>(ta), static_cast<uint32_t>(tb)}, {}};
    }, [](A a) { env_objects_draw(a.i[0], static_cast<int32_t>(a.i[1]), wide_arg(6), wide_arg(7)); return uint64_t{0}; }, 0x10);
    g_wide_regs.clear();
    if (info()) std::printf("INFO env objects L%d: %d of %d generated cases with a record on screen\n", g_oracle_level, seen_cases, all_cases);
    // the mapping on points and normals of the level's records (their own), or made up about the camera
    helpercheck("env_objects_uvs", uvs, 60, Ret::None, [recs, count] {
        level();
        const uint32_t rec = recs + u(ri(0, count - 1)) * 0x20;
        uint32_t pts = ee_read<uint32_t>(rec + 0x10), nrm = ee_read<uint32_t>(rec + 0x18);
        int32_t n = std::min(ee_read<int32_t>(rec + 0x1c), 0x80);
        if (coin() || n <= 0) {
            pts = T;
            nrm = T + 0x600;
            n = ri(1, 0x40);
            const Vec4 c = ee_ref<Vec4>(OV(0x00166dc0));
            for (int32_t k = 0; k < n; ++k) {
                const Vec4 p = around(c, 0.0f, 20.0f, -5.0f, 5.0f);
                wf(T + u(k) * 0xc, p.x); wf(T + u(k) * 0xc + 4, p.y); wf(T + u(k) * 0xc + 8, ri(0, 9) == 0 ? c.z : p.z);
                wfrand(T + 0x600 + u(k) * 0xc, 3, -1.0f, 1.0f);
            }
            if (coin()) { wf(T, c.x); wf(T + 4, c.y); wf(T + 8, c.z); }  // (a point at the camera: a 0 length)
        }
        return Args{{u(n), pts, nrm, T2}, {}};
    }, [](A a) { vu0_sync(); env_objects_uvs(static_cast<int32_t>(a.i[0]), a.i[1], a.i[2], a.i[3]); return uint64_t{0}; });
}

}  // namespace

void batch_517() {
    if (g_only_batch != 517) return;
    g_batch = 517;
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

    class252_checks();
    class52_checks();
    class1400_checks();
    class1885_checks();
    class1511_checks();
    floater_stage_checks();
    seas_checks();
    class50_checks();
    class886_checks();
    bg_ship1_checks();
    envmap_meshes_checks();
    class655_draw_checks();
    env_objects_checks();
    g_pre_call = nullptr;
    g_extra_noop_hooks.clear();
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
