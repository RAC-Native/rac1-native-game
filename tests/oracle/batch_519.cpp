// Oracle batch 519 (stage 4 task F13, #149): class code and helpers of levels 3, 6, 7, 8, 11, 14, 15, 17 and 18 that two or
// more copies carry, each family ported once as shared hand-written code (src/game/ovl/shared_*.cpp) on top of the families
// earlier tasks ported, against every copy at its own level's address. The level copies stay transliterated in their units
// until each level is re-glued; here every copy is the original and the one shared port the port:
//  - shared_floor_switch.cpp: class 1209, a floor switch of a group (Quartu, Drek's fleet);
//  - shared_class934.cpp: class 934, a spinning shot (Quartu, Drek's fleet);
//  - shared_breakables.cpp: the breakable props of Blarg (classes 1655, 1660, 1664) and Umbris (1650, 1663);
//  - shared_ship_exhaust.cpp: the exhaust of the ships the hero rides (Pokitaru's class 1242, Drek's fleet's 1379);
//  - shared_pump_camera.cpp: Blarg's hydro pumps' camera (class 1051), its start and its follow;
//  - shared_flash.cpp: the flash's draw (class 1898 of Blarg and Veldin's finale);
//  - shared_scroll.cpp: three more sets of the scrolling textures' meshes (Batalia, Pokitaru);
//  - shared_class994.cpp: the sea's controller (Kerwan's class 994, Oltanis's class 1260).
// The shared ports and their copies (each level's address):
//   floor_switch_update   L15:0x002e5958 L17:0x002e5f50
//   class934_update       L15:0x002e4380 L17:0x002e1c20
//   class1655_update      L6:0x00309860
//   class1650_update      L7:0x0031f3d8
//   class1663_update      L7:0x0031f5b0
//   class1660_update      L6:0x00309e08
//   class1664_update      L6:0x00309fa8
//   ship_exhaust_spawn    L11:0x003112b8 L17:0x002ea688
//   pump_camera_start     L6:0x002fb148 / 0x002fb508 / 0x002fb8c8
//   pump_camera_follow    L6:0x002fb668 / 0x002fba28
//   flash_draw_quad       L6:0x0030bfa8 L18:0x002fb690
//   uv_scroll_meshes      (sets 8, 9, 10) L8:0x002f0b40 / 0x00308db8, L11:0x0031e6a8
//   class994_update       L3:0x002dc9b0 L14:0x00303370
// Subjects: the level's own mobys of the class after the first gameplay frame (the screen, the projection and the view set
// up, as batch 518), placed or live, perturbed through every state; mobys of the class spawned where the level places none
// awake. The draws write to a packet buffer of the test area; B519_INFO=1 prints the subjects found and, per check, how many
// cases wrote packets beyond a state header.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 519, N = 3, 6, 7, 8, 11, 14, 15, 17 and 18; a check prints "skipped"
// on a level without its copy. No RAM dumps; the frame scale (0x15ed68) at its 50 Hz value in a quarter of the cases.

#include "ovl_oracle.h"

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_breakables.h>
#include <game/ovl/shared_class934.h>
#include <game/ovl/shared_class994.h>
#include <game/ovl/shared_flash.h>
#include <game/ovl/shared_floor_switch.h>
#include <game/ovl/shared_pump_camera.h>
#include <game/ovl/shared_scroll.h>
#include <game/ovl/shared_ship_exhaust.h>
#include <runtime/kernel/disc.h>
#include <runtime/vu/vu0.h>

#include <cmath>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <vector>

namespace rac1::oracle {

namespace {
// The VU0 the original started from (the interpreter's), given to the port before it runs (as batches 506 / 510 / 518)
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

bool info() { return std::getenv("B519_INFO") != nullptr; }

// one level's copy of a shared function
struct Copy {
    int level;
    uint32_t addr;
};
// this level's `nth` copy among `copies`, 0 when the level has none
uint32_t copy_here(std::initializer_list<Copy> copies, int nth = 0) {
    for (const Copy& c : copies)
        if (c.level == g_oracle_level && nth-- == 0) return c.addr;
    return 0;
}

bool selected(const char* name) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return false;
    const char* only = std::getenv("ORACLE_ONLY");
    return only == nullptr || std::strcmp(only, name) == 0;
}

// the bytes of the packets between `c0` and `c1` that are not a state header (batch 510's)
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

// A check of a shared port against this level's copy at `laddr` (0: the level has none, skipped); the port given the
// original's VU0 first. With B519_INFO, the cases that wrote packets beyond a state header are counted.
void fcheck(const char* name, uint32_t laddr, int n, Ret ret, const std::function<Args()>& gen,
            const std::function<uint64_t(const Args&)>& port) {
    if (!selected(name)) return;
    if (laddr == 0) {
        std::printf("%-26s skipped (no copy on level %d)\n", name, g_oracle_level);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L%d:0x%08x\n", name, g_oracle_level, laddr);
    int cases = 0, drew = 0;
    const std::function<uint64_t(const Args&)> synced = [&port, &cases, &drew](const Args& a) {
        vu0_sync();
        if (!info()) return port(a);
        const uint32_t c0 = ee_read<uint32_t>(OV(0x001611c0));
        const uint64_t r = port(a);
        const uint32_t c1 = ee_read<uint32_t>(OV(0x001611c0));
        ++cases;
        if (c0 >= 0x00100000u && c1 > c0 && c1 - c0 < 0x100000 && packet_content(c0, c1) != 0) ++drew;
        return r;
    };
    if (!check_fn(name, laddr, n, ret, gen, synced, Scope::FullRam, 0, 0)) ++g_fail;
    if (info() && (std::strstr(name, "draw") != nullptr || std::strstr(name, "scroll") != nullptr))
        std::printf("INFO %s L%d: %d cases, packets beyond a state header in %d\n", name, g_oracle_level, cases, drew);
}

// the level after its first gameplay frame (the port's) with the screen, the projection and the view set up (as batch 518)
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

// the frame's packet cursor (0x1611c0) on a buffer of the test area (batch 506's)
void packet_cursor() { w32(OV(0x001611c0), TEST_AREA + 0x8000 + u(ri(0, 0x40)) * 0x10); }
// the frame scale (0x15ed68) at the 50 Hz value sometimes (frame_scale_i's counts then differ from the 60 Hz ones)
void vary_frame_scale() {
    if (ri(0, 3) == 0) wf(0x0015ed68, 1.2f);
}

uint32_t pvar(uint32_t m) { return ee_read<uint32_t>(m + 0x78); }
uint32_t first_moby() { return ee_read<uint32_t>(OV(0x0015ffd8)); }
uint32_t end_moby() { return ee_read<uint32_t>(OV(0x0015ffe0)); }
bool live(uint32_t m) { return ee_read<uint8_t>(m + 0x20) < 0xfe; }
uint32_t hero_moby() { return ee_read<uint32_t>(0x001413d0); }

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
        if (ee_read<uint8_t>(m + 0x20) != 0xff && ee_read<uint16_t>(m + 0xa6) == cls && ee_read<uint32_t>(m + 0x78) != 0) v.push_back(m);
    return v;
}
// the level's live mobys (with a class and pvars)
std::vector<uint32_t> live_mobys() {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < end_moby(); m += 0x100)
        if (live(m) && ee_read<uint32_t>(m + 0x24) != 0 && ee_read<uint32_t>(m + 0x78) != 0) v.push_back(m);
    return v;
}
uint32_t pick(const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; }

// the class's mobys: the level's own (live, else placed), else one spawned (0 when the class is not on the level)
std::vector<uint32_t> subjects_of(uint32_t cls) {
    std::vector<uint32_t> v;
    for (const uint32_t m : placed_of(cls))
        if (live(m)) v.push_back(m);
    if (v.empty()) v = placed_of(cls);
    if (v.empty()) {
        const uint32_t m = moby_spawn(cls);
        if (m != 0 && ee_read<uint32_t>(m + 0x78) != 0) v.push_back(m);
    }
    return v;
}

// a point `r0`..`r1` from `at` (horizontally), `z0`..`z1` above it
Vec4 around(const Vec4& at, float r0, float r1, float z0 = -1.0f, float z1 = 2.0f) {
    const float r = rf(r0, r1), a = rf(-3.1f, 3.1f);
    return Vec4{at.x + r * std::cos(a), at.y + r * std::sin(a), at.z + rf(z0, z1), at.w};
}
void hero_at(const Vec4& q) {
    ee_ref<Vec4>(0x0013f3d0) = q;
    ee_ref<Vec4>(hero_moby() + 0x10) = q;
}

// a hit on the moby: its slot in the hit table (0x178100, 64-byte entries) naming it, with a damage mask (the `kinds` bits
// mixed with random ones) and a damage at +0x2c (batch 518's); or none (0xff)
void give_hit(uint32_t m, uint32_t kinds) {
    if (ri(0, 3) == 0) { w8(m + 0xa4, 0xff); return; }
    const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
    w8(m + 0xa4, idx);
    w32(e + 0x34, ri(0, 8) == 0 ? first_moby() : m);
    w32(e + 0x24, ri(0, 2) == 0 ? (R() & ~kinds) : (kinds | (R() & 0xff)));
    { static const float dm[] = {-1.0f, 0.0f, 0.5f, 5.0f, 12.0f}; wf(e + 0x2c, dm[ri(0, 4)]); }
    wfrand(e, 4, -1, 1);
    w32(e + 0x20, ri(0, 3) == 0 ? 0u : hero_moby());
    w8(e + 0x28, u(ri(0, 8)));
    w8(e + 0x29, u(ri(0, 4)));
    w16(e + 0x2a, u(ri(0, 0x60)));
    w32(e + 0x1c, coin() ? 0x45afdf66u : 0u);
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

// An update on one of the subjects (dealt again when the port's run throws)
void classcheck(const char* name, uint32_t laddr, const std::function<std::vector<uint32_t>()>& subjects, void (*port)(uint32_t),
                const std::function<void(uint32_t)>& vary, int n) {
    if (!selected(name)) return;
    if (laddr == 0) {
        std::printf("%-26s skipped (no copy on level %d)\n", name, g_oracle_level);
        ++g_skipped;
        return;
    }
    level();
    if (subjects().empty()) {
        std::printf("%-26s L%d:0x%08x  no subject on this level\n", name, g_oracle_level, laddr);
        ++g_fail;
        return;
    }
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

// The words where the native collision (existing code) differs from the original: the result rows' w lanes and the segment
// record's VU0 leftovers (batch 510's collision_lanes), left out in the checks of the functions that collide, only.
void collision_lanes() {
    for (uint32_t row = 0x20; row < 0x80; row += 0x10) g_ignored_words.push_back(OV(0x00173e40) + row + 0xc);
    for (uint32_t k = 0; k < 0xc; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
}
// The debris' records (after the moby table): debris_spawn leaves in each record's +0xc / +0x1c the w lanes of a VU0 sin / cos
// in the original, which its port does not (batches 117 / 506 / 518's difference). Left out in the breaking state, only.
void debris_lanes() {
    for (uint32_t a = end_moby(); a < end_moby() + 0x20000; a += 0x80) {
        g_ignored_words.push_back(a + 0xc);
        g_ignored_words.push_back(a + 0x1c);
    }
}

// ---- class 1209, the floor switch (shared_floor_switch.cpp)
void floor_switch_checks() {
    const uint32_t upd = copy_here({{15, 0x002e5958}, {17, 0x002e5f50}});
    if (upd != 0 && info()) {
        level();
        std::printf("INFO class 1209: %zu awake, %zu placed\n", mobys_with(upd).size(), placed_of(1209).size());
        for (const uint32_t m : placed_of(1209)) {
            const uint32_t pv = pvar(m);
            std::printf("INFO   %08x state %u group %u flag %d lead %08x led %u count %u radius %g time %d\n", m, ee_read<uint8_t>(m + 0x20),
                        ee_read<uint8_t>(m + 0x21), ee_read<int16_t>(m + 0xb2), ee_read<uint32_t>(pv + 0x10), ee_read<uint8_t>(pv + 9),
                        ee_read<uint8_t>(pv + 0xb), ee_read<float>(pv + 0x14), ee_read<int32_t>(pv + 0xc));
        }
    }
    auto switches = [] { return placed_of(1209); };
    for (int st = 0; st < 6; ++st)
        classcheck("floor_switch_update", upd, switches, floor_switch_update, [st, switches](uint32_t m) {
            const uint32_t pv = pvar(m);
            const std::vector<uint32_t> all = switches();
            // the lead: itself, another switch of the level (its state 1 or not), led or not
            const uint32_t lead = ri(0, 2) == 0 ? m : pick(all);
            w32(pv + 0x10, lead);
            w8(pv + 9, u(ri(0, 1)));
            const uint32_t lpv = pvar(lead);
            if (lead != m) w8(lead + 0x20, ri(0, 2) != 0 ? 1u : u(ri(0, 4)));
            // the lead's time (none, or some) and the switch's own (the time the lead's start takes)
            w32(lpv + 0xc, ri(0, 2) == 0 ? u(ri(-5, 0)) : u(ri(1, 900)));
            if (lead != m && coin()) w32(pv + 0xc, u(ri(-3, 900)));
            // the counts: lit below, at or past the switches
            const uint32_t total = u(ri(1, 5));
            w8(pv + 0xb, total);
            const int k = ri(0, 3);
            w8(pv + 0xa, k == 0 ? total : (k == 1 ? total + 1 : u(ri(0, static_cast<int>(total) - 1))));
            w8(lpv + 0xa, u(ri(0, 5)));
            // the timers at their edges
            // (the lead's time also past 1600 frames: the tick's wait then capped at 45 frames)
            static const int16_t ts[] = {0, 1, 2, 3, 40, 300, -1, 900, 1599, 1640, 2000, 5000};
            w16(pv, u(ts[ri(0, 11)]));
            w16(pv + 2, u(ts[ri(0, 6)]));
            w16(pv + 6, u(ts[ri(0, 6)]));
            w16(pv + 4, ri(0, 2) == 0 ? 1u : 0u);
            w8(pv + 8, u(ri(0, 1)));
            w16(pv + 0x18, u(ri(0, 600)));
            w32(pv + 0x1c, ri(0, 2) == 0 ? 0u : u(ri(1, 3)));
            if (ri(0, 3) == 0) wf(pv + 0x14, rf(0.5f, 4.0f));
            // done before: the switch byte (0x1bb230 + 0x454) or the level's flag word, set or not
            const int32_t idx = ee_read<int16_t>(m + 0xb2);
            if (idx >= 0 && idx < 0x400) {
                const uint32_t byte = OV(0x001bb230) + u(idx) + 0x454;
                w8(byte, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
                const uint32_t word = 0x0014c190 + (u(idx >> 5) << 2) + (ee_read<uint32_t>(0x0015ed84) << 8);
                const uint32_t bit = 1u << (idx & 0x1f);
                w32(word, ri(0, 2) == 0 ? (ee_read<uint32_t>(word) | bit) : (ee_read<uint32_t>(word) & ~bit));
                if (coin()) w32(word, ee_read<uint32_t>(word) ^ (R() & ~bit));
                const uint32_t own = OV(0x001ba4d0) + (u(idx >> 5) << 2);
                if (coin()) w32(own, ee_read<uint32_t>(own) ^ R());
            }
            // the hero on it (near its centre, along its local y below or above 0.2), near it, or away
            const Vec4 at = ee_ref<Vec4>(m + 0x10);
            const int h = ri(0, 5);
            if (h <= 2) hero_at(around(at, 0.0f, 0.4f, -0.4f, 0.4f));
            else if (h == 3) hero_at(around(at, 0.0f, ee_read<float>(pv + 0x14) * 1.2f, -0.5f, 0.5f));
            else if (h == 4) hero_at(around(at, 5.0f, 40.0f));
            vary_frame_scale();
            w8(m + 0x20, u(st == 5 ? ri(5, 9) : st));
        }, st == 1 || st == 2 ? 120 : 40);
}

// ---- class 934, the spinning shot (shared_class934.cpp)
void class934_checks() {
    const uint32_t upd = copy_here({{15, 0x002e4380}, {17, 0x002e1c20}});
    if (upd != 0 && info()) {
        level();
        std::printf("INFO class 934: %zu awake, %zu placed\n", mobys_with(upd).size(), placed_of(934).size());
    }
    const size_t ign = g_ignored_words.size();
    collision_lanes();
    // shots: the class spawned (or a live moby given the shot's pvars in the test area)
    auto shots = [] {
        const uint32_t m = moby_spawn(934);
        if (m != 0) {
            if (ee_read<uint32_t>(m + 0x78) == 0) w32(m + 0x78, TEST_AREA + 0x800);
            return std::vector<uint32_t>{m};
        }
        return live_mobys();
    };
    for (int st = 0; st < 3; ++st)
        classcheck("class934_update", upd, shots, class934_update, [st](uint32_t m) {
            if (ee_read<uint32_t>(m + 0x74) != 0 && ee_read<uint16_t>(m + 0xa6) != 934) w32(m + 0x78, TEST_AREA + 0x800);
            uint32_t pv = pvar(m);
            if (st == 2) {  // (no pvars: retired)
                w32(m + 0x78, 0);
                w8(m + 0x20, 0);
                return;
            }
            // toward a live moby (reached this frame or not), at the hero, or into the open; its velocity a frame's flight
            const std::vector<uint32_t> others = live_mobys();
            uint32_t target = 0;
            const int k = ri(0, 3);
            Vec4 from = ee_ref<Vec4>(m + 0x10);
            Vec4 to{};
            if (k <= 1 && !others.empty()) {
                target = pick(others);
                if (ri(0, 3) == 0) w16(target + 0xa6, 0x4e);  // (a class it pushes no contact into)
                to = ee_ref<Vec4>(target + 0x10);
                to.z += rf(0.0f, 1.0f);
                from = around(to, k == 0 ? 0.2f : 1.5f, k == 0 ? 1.2f : 4.0f, -0.2f, 1.0f);
            } else if (k == 2) {
                to = ee_ref<Vec4>(0x0013f3d0);
                to.z += 0.8f;
                from = around(to, 0.2f, 3.0f, -0.2f, 1.0f);
            } else {
                from = around(ee_ref<Vec4>(0x0013f3d0), 3.0f, 20.0f, 1.0f, 6.0f);
                to = around(from, 0.5f, 3.0f, -1.0f, 1.0f);
            }
            from.w = 1.0f;
            ee_ref<Vec4>(m + 0x10) = from;
            const float s = rf(0.2f, 1.2f);
            ee_ref<Vec4>(pv) = Vec4{(to.x - from.x) * s, (to.y - from.y) * s, (to.z - from.z) * s, 0.0f};
            // its shooter: none, the moby it may meet, another
            const int sh = ri(0, 2);
            w32(pv + 0x3c, sh == 0 ? 0u : (sh == 1 ? target : hero_moby()));
            static const int32_t ts[] = {0, 1, 2, 3, 50};
            w32(pv + 0x38, u(ts[ri(0, 4)]));
            wf(m + 0x40, rf(-3.1f, 3.1f));
            w32(OV(0x0015f5cc), R() & 0xffff);
            w8(m + 0x20, st == 0 ? 0u : u(ri(1, 4)));
            seed_rand();
        }, st == 0 ? 160 : 20);
    g_ignored_words.resize(ign);
}

// ---- the breakables of Blarg and Umbris (shared_breakables.cpp)
void breakable_checks() {
    struct B {
        const char* name;
        int level;
        uint32_t addr;
        uint32_t cls;
        void (*port)(uint32_t);
    };
    static const B bs[] = {{"class1655_update", 6, 0x00309860, 1655, class1655_update}, {"class1660_update", 6, 0x00309e08, 1660, class1660_update},
                           {"class1664_update", 6, 0x00309fa8, 1664, class1664_update}, {"class1650_update", 7, 0x0031f3d8, 1650, class1650_update},
                           {"class1663_update", 7, 0x0031f5b0, 1663, class1663_update}};
    for (const B& b : bs) {
        const uint32_t addr = b.level == g_oracle_level ? b.addr : 0u;
        if (addr != 0 && info()) {
            level();
            std::printf("INFO class %u: %zu awake, %zu placed\n", b.cls, mobys_with(addr).size(), placed_of(b.cls).size());
        }
        const uint32_t cls = b.cls;
        for (int st = 0; st < 4; ++st) {
            const size_t ign = g_ignored_words.size();
            if (st == 2 && addr != 0) {
                level();
                debris_lanes();
            }
            // (the level's own, awake: the props have no pvars; else one spawned)
            classcheck(b.name, addr, [cls, addr] {
                const std::vector<uint32_t> own = mobys_with(addr);
                return own.empty() ? subjects_of(cls) : own;
            }, b.port, [st](uint32_t m) {
                give_hit(m, 0x10000u);
                // the hero near (its bolts' extra flags) or away
                if (coin()) hero_at(around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 6.5f : 30.0f, -0.5f, 0.5f));
                w8(m + 0x20, u(st == 3 ? ri(3, 9) : st));
                seed_rand();
            }, st == 1 ? 60 : (st == 2 ? 40 : 15));
            g_ignored_words.resize(ign);
        }
    }
}

// ---- the ships' exhaust (shared_ship_exhaust.cpp)
void ship_exhaust_checks() {
    const uint32_t addr = copy_here({{11, 0x003112b8}, {17, 0x002ea688}});
    const uint32_t cls = g_oracle_level == 11 ? 1242u : 1379u;
    if (addr != 0 && info()) {
        level();
        std::printf("INFO class %u: %zu placed; full speed %g\n", cls, placed_of(cls).size(), ee_read<float>(g_oracle_level == 11 ? 0x00161ff4u : 0x00162004u));
    }
    // (the full speed: the level's word the speed is measured against)
    const uint32_t full = g_oracle_level == 11 ? 0x00161ff4u : 0x00162004u;
    fcheck("ship_exhaust_spawn", addr, 120, Ret::None, [cls, full] {
        level();
        const std::vector<uint32_t> ms = subjects_of(cls);
        if (ms.empty()) return Args{{0, 0}, {}};
        const uint32_t m = pick(ms);
        const uint32_t pv = pvar(m);
        const float top = ee_read<float>(full);
        // (its share of the full speed: none, below, at and just past the floor 0.35, between, full, past)
        static const float speeds[] = {0.0f, 0.2f, 0.35f, 0.352f, 0.356f, 0.5f, 1.0f, 1.5f};
        wf(pv + 0x64, top * speeds[ri(0, 7)] * (coin() ? rf(0.99f, 1.01f) : 1.0f));
        static const float thrusts[] = {0.0f, 0.3f, 0.5f, 0.51f, 0.8f, 1.0f, 2.0f};
        wf(pv + 0xc4, coin() ? thrusts[ri(0, 6)] : rf(0.0f, 1.2f));
        if (coin()) wf(0x0015ed6c, rf(0.01f, 0.04f));
        vary_frame_scale();
        seed_rand();
        return Args{{m, pv}, {}};
    }, [](A a) { if (a.i[0] != 0) ship_exhaust_spawn(a.i[0], a.i[1]); return uint64_t{0}; });
}

// ---- Blarg's pumps' camera (shared_pump_camera.cpp)
void pump_camera_checks() {
    const bool blarg = g_oracle_level == 6;
    if (blarg && info()) {
        level();
        std::printf("INFO class 1051: %zu placed\n", placed_of(1051).size());
        for (const uint32_t m : placed_of(1051)) {
            const uint32_t pv = pvar(m);
            std::printf("INFO   %08x state %u anim %u spot %d path %d blend %g\n", m, ee_read<uint8_t>(m + 0x20), ee_read<uint8_t>(m + 0x53),
                        ee_read<int32_t>(pv + 0xf8), ee_read<int32_t>(pv + 0xec), ee_read<float>(pv + 0x118));
        }
    }
    // the switch's state dealt: its spot and path (its own, or another switch's), its blend at its edges and its rate, its
    // animation 6, 1 or another, the hero about
    auto deal = [] {
        level();
        const std::vector<uint32_t> ms = placed_of(1051);
        if (ms.empty()) return uint32_t{0};
        const uint32_t m = pick(ms);
        const uint32_t pv = pvar(m);
        if (ri(0, 3) == 0) {
            const uint32_t o = pvar(pick(ms));
            w32(pv + 0xf8, ee_read<uint32_t>(o + 0xf8));
            w32(pv + 0xec, ee_read<uint32_t>(o + 0xec));
        }
        static const float blends[] = {0.0f, 0.5f, 0.99f, 1.0f, 1.2f};
        wf(pv + 0x118, coin() ? blends[ri(0, 4)] : rf(0.0f, 1.0f));
        wf(pv + 0x11c, rf(-0.1f, 0.1f));
        const int a = ri(0, 3);
        w8(m + 0x53, a == 0 ? 6u : (a == 1 ? 1u : u(ri(0, 9))));
        wf(m + 0x48, rf(-3.1f, 3.1f));
        moby_update_transform(m);
        if (coin()) hero_at(around(ee_ref<Vec4>(m + 0x10), 1.0f, 10.0f, -0.5f, 1.0f));
        if (coin()) wf(0x0015ed6c, rf(0.01f, 0.04f));
        if (coin()) wf(0x0015ed70, rf(0.0001f, 0.002f));
        return m;
    };
    for (int k = 0; k < 3; ++k)
        fcheck("pump_camera_start", blarg ? (k == 0 ? 0x002fb148u : (k == 1 ? 0x002fb508u : 0x002fb8c8u)) : 0u, 30, Ret::None, [deal] {
            return Args{{deal()}, {}};
        }, [](A a) { if (a.i[0] != 0) pump_camera_start(a.i[0]); return uint64_t{0}; });
    for (int k = 0; k < 2; ++k)
        fcheck("pump_camera_follow", blarg ? (k == 0 ? 0x002fb668u : 0x002fba28u) : 0u, 60, Ret::Int32, [deal] {
            return Args{{deal()}, {}};
        }, [](A a) { return a.i[0] != 0 ? r32(pump_camera_follow(a.i[0])) : uint64_t{0}; });
}

// ---- class 1898's draw (shared_flash.cpp)
void flash_draw_checks() {
    const uint32_t addr = copy_here({{6, 0x0030bfa8}, {18, 0x002fb690}});
    if (addr != 0 && info()) {
        level();
        std::printf("INFO class 1898: %zu placed\n", placed_of(1898).size());
    }
    fcheck("flash_draw_quad", addr, 80, Ret::None, [] {
        level();
        packet_cursor();
        uint32_t m = moby_spawn(1898);
        if (m == 0) m = pick(live_mobys());
        if (ee_read<uint32_t>(m + 0x78) == 0) w32(m + 0x78, TEST_AREA + 0x800);
        const uint32_t pv = pvar(m);
        static const float phases[] = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, -0.5f};
        wf(pv, coin() ? phases[ri(0, 6)] : rf(0.0f, 2.0f));
        w32(pv + 8, R());
        w32(pv + 0xc, R());
        // in front of the camera, near or far, or anywhere about the hero
        const Vec4 cam = ee_ref<Vec4>(OV(0x00166dc0)), fwd = ee_ref<Vec4>(OV(0x00166fd0));
        const int k = ri(0, 3);
        if (k < 3) {
            const float r = rf(2.0f, k == 0 ? 8.0f : 60.0f), j = 0.3f;
            ee_ref<Vec4>(m + 0x10) = Vec4{cam.x + r * (fwd.x + rf(-j, j)), cam.y + r * (fwd.y + rf(-j, j)), cam.z + r * (fwd.z + rf(-j, j)), 1.0f};
        } else {
            ee_ref<Vec4>(m + 0x10) = around(ee_ref<Vec4>(0x0013f3d0), 0.0f, 30.0f);
        }
        return Args{{m}, {}};
    }, [](A a) { flash_draw_quad(a.i[0]); return uint64_t{0}; });
}

// ---- the scrolling textures' three more sets (shared_scroll.cpp)
void scroll_checks() {
    struct Sc {
        int level;
        uint32_t addr;
        int set;
        uint32_t offsets;
    };
    static const Sc sets[] = {{8, 0x002f0b40, 8, 0x001e8348}, {8, 0x00308db8, 9, 0x001f8548}, {11, 0x0031e6a8, 10, 0x00217758}};
    bool any = false;
    for (const Sc& x : sets) {
        if (x.level != g_oracle_level) continue;
        any = true;
        const Sc c = x;
        fcheck("uv_scroll_meshes", c.addr, 24, Ret::None, [c] {
            level();
            const uint32_t idx = u(ri(0, 2));
            wfrand(c.offsets + idx * 8, 2, -1, 1);
            packet_cursor();
            return Args{{idx}, {}};
        }, [c](A a) { uv_scroll_meshes(c.set, a.i[0]); return uint64_t{0}; });
    }
    if (!any) fcheck("uv_scroll_meshes", 0, 0, Ret::None, nullptr, nullptr);
}

// ---- the sea's controller (shared_class994.cpp)
void class994_checks() {
    const uint32_t upd = copy_here({{3, 0x002dc9b0}, {14, 0x00303370}});
    const uint32_t cls = g_oracle_level == 3 ? 994u : 1260u;
    if (upd != 0 && info()) {
        level();
        std::printf("INFO class %u: %zu awake, %zu placed\n", cls, mobys_with(upd).size(), placed_of(cls).size());
    }
    for (int st = 0; st < 3; ++st)
        classcheck("class994_update", upd, [upd] {  // (the level's own controller: it has no pvars)
            const std::vector<uint32_t> v = mobys_with(upd);
            return v.empty() ? live_mobys() : v;
        }, class994_update, [st](uint32_t m) {
            w8(m + 0x30, R() & 0xff);
            w8(m + 0x20, u(st == 2 ? ri(2, 9) : st));
        }, 20);
}
}  // namespace

void batch_519() {
    if (g_only_batch != 519) return;
    g_batch = 519;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // 989snd's IOP server (as batches 120 / 506 / 518): replies a function of the command
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

    floor_switch_checks();
    class934_checks();
    breakable_checks();
    ship_exhaust_checks();
    pump_camera_checks();
    flash_draw_checks();
    scroll_checks();
    class994_checks();

    g_pre_call = nullptr;
    g_extra_noop_hooks.clear();
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
