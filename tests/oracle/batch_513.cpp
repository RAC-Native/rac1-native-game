// Oracle batch 513 (stage 4 task G6, #149): hand-written ports of one level made to serve every level that carries a copy of
// the same code, each against every level's own copy at that level's own address (the copies stay transliterated in their
// units until each level is re-glued; here every copy is the original and the one port the port):
//  - novalis_teleporter.cpp: class 1135, the teleporter pad: its update through every state, the trigger, the hint, the
//    hide and the show (levels 1, 7, 8, 12, 13, 16, 17, 18), on the level's own pads past their first update (the pieces
//    spawned), the destination pad, the hero on the pad or not, the level number varied (the hint's tables of every level);
//  - novalis_platforms.cpp: the lift (class 726 on 1, 3, 13, class 1141 on 10), on the level's own lifts or live mobys given
//    a lift's pvars on one of the level's paths, at its ends and on its way, the hero riding it, near it or in its boxes;
//  - novalis_cases.cpp: the gold weapon case (classes 304, 1456 .. 1465; levels 1, 13), in its states but the scene start
//    (the case answered in state 1 starts gold_weapon_scene, which no batch compares: 989snd's SIF, batch 510) and state 4's
//    level change (run for real by the original: the IOP);
//  - aridia_ocean.cpp: the ocean's helpers (levels 2, 3, 5, 7, 8, 9, 14): the set-up, the transfer to the scratchpad, the
//    cells' test and draw, the texture blend, the end, and the one-record draw of the levels other than Aridia (3 .. 14), on
//    each level's own ocean records (Aridia's seven surfaces) with the camera over them or away; the packet cursor on a
//    buffer of the test area (`B513_INFO=1` prints how far each case's packets grew).
// No RAM dumps: states built from the level after its first gameplay frame, the screen and projection set up (640 x 448).
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 513, N = 1, 2, 3, 5, 7, 8, 9, 10, 12, 13, 14, 16, 17, 18; a family the
// level does not carry says so.

#include "ovl_oracle.h"

#include <game/ovl/aridia_ocean.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/vu/vu0.h>

#include <cmath>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {
// The VU0 the original started from (the interpreter's), given to the port before it runs (as batches 501 / 503)
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

bool info() { return std::getenv("B513_INFO") != nullptr; }

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

// the frame's packet cursor (0x1611c0)
uint32_t cursor() { return ee_read<uint32_t>(OV(0x001611c0)); }

// A check whose original is the level's own copy at `laddr`; the port gets the interpreter's VU0 and, with B513_INFO, says how
// far the packet cursor went
void fcheck(const char* name, uint32_t laddr, int n, Ret ret, const std::function<Args()>& gen,
            const std::function<uint64_t(const Args&)>& port) {
    if (!selected(name)) return;
    std::printf("%-26s L%d:0x%08x\n", name, g_oracle_level, laddr);
    const std::string nm = name;
    const std::function<uint64_t(const Args&)> synced = [&port, nm](const Args& a) {
        vu0_sync();
        const uint32_t c0 = cursor();
        const uint64_t r = port(a);
        if (info()) std::printf("INFO %s packets +0x%x\n", nm.c_str(), cursor() - c0);
        return r;
    };
    if (!check_fn(name, laddr, n, ret, gen, synced, Scope::FullRam, 0, 0)) ++g_fail;
}

void not_here(const char* name) {
    if (selected(name)) std::printf("%-26s not on level %d\n", name, g_oracle_level);
}

// the level after its first gameplay frame (the port's) and its view set up, as batch 503
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
        // the screen and projection constants, which the oracle's display (no size) leaves broken: a 640 x 448 screen, as the
        // game's (batch 501)
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

constexpr uint32_t P = 0x0013f350;  // the hero (+0x80 his position)
constexpr uint32_t PV = TEST_AREA + 0x6000;  // pvars given to a live moby made a subject

uint32_t first_moby() { return ee_read<uint32_t>(OV(0x0015ffd8)); }

// the level's live mobys (with a class), those whose update function is `fn` (the level's address), those of class `cls`
std::vector<uint32_t> live_mobys() {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x24) != 0) v.push_back(m);
    return v;
}
std::vector<uint32_t> mobys_with(uint32_t fn) {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn && ee_read<uint32_t>(m + 0x78) != 0) v.push_back(m);
    return v;
}
std::vector<uint32_t> mobys_of(int cls) {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<int16_t>(m + 0xa6) == cls && ee_read<uint32_t>(m + 0x78) != 0) v.push_back(m);
    return v;
}

uint32_t pick(const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; }

// whether the port runs on the state without throwing (a pointer outside RAM: the state is dealt again), RAM kept
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

// An update (or a helper taking the moby) on the subjects `ms`, `vary` building the state (dealt again when the port's run
// throws, as batch 502's classcheck)
void classcheck(const char* name, uint32_t laddr, const std::function<std::vector<uint32_t>()>& subjects,
                const std::function<void(uint32_t)>& port, const std::function<void(uint32_t)>& vary, int n) {
    if (!selected(name)) return;
    level();
    const std::vector<uint32_t> probe = subjects();
    if (probe.empty()) { std::printf("%-26s L%d:0x%08x  no subject on this level\n", name, g_oracle_level, laddr); return; }
    if (info()) std::printf("  (%s: %zu subjects, class %d)\n", name, probe.size(), ee_read<int16_t>(probe[0] + 0xa6));
    fcheck(name, laddr, n, Ret::None, [subjects, vary, port] {
        uint32_t m = 0;
        for (int attempt = 0; attempt < 40; ++attempt) {
            level();
            m = pick(subjects());
            vary(m);
            if (trial_ok([port, m] { port(m); })) break;
        }
        return Args{{m}, {}};
    }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
}

// a sound slot (0x13e550 + slot * 0x70) the moby owns and plays, or not, in pvar `at` (-1: none)
void sound_slot(uint32_t m, uint32_t at) {
    const int32_t slot = ri(0, 3) == 0 ? -1 : ri(0, 15);
    w32(at, u(slot));
    if (slot >= 0) {
        const uint32_t c = 0x0013e550 + u(slot) * 0x70;
        w32(c + 0x88, ri(0, 3) == 0 ? 0u : m);
        w8(c + 0x74, R() & 1);
    }
}

// the music / dialogue streams ready (batch 103's)
void sound_ready() {
    w16(0x001516d0 + 0x5a, 3);
    w32(0x001516d0 + 0x50, (R() & 0x7ffffffeu) | 1u);
    w16(0x001516d0 + 0x5c, 0);
    w32(0x001516d0 + 0x1c, 0xffffffffu);
}

// the hero near the moby (within 5, or 5 .. 15)
void hero_near(uint32_t m) {
    Vec4 q = ee_ref<Vec4>(m + 0x10);
    const float r = ri(0, 2) == 0 ? rf(0, 5) : rf(5, 15);
    const float a = rf(-3.1f, 3.1f);
    q.x += r * std::cos(a);
    q.y += r * std::sin(a);
    q.z += rf(-1, 3);
    ee_ref<Vec4>(0x0013f3d0) = q;
}

// the hero inside (or about) one of the level's boxes `b` (0x1600ec: 0x80 apart, their centre at +0x30)
void hero_in_box(int32_t b) {
    if (b < 0 || b >= 8) return;
    const uint32_t box = ee_read<uint32_t>(OV(0x001600ec)) + u(b << 7);
    for (uint32_t k = 0; k < 3; ++k) wf(0x0013f3d0 + 4 * k, ee_read<float>(box + 0x30 + 4 * k) + rf(-0.4f, 0.4f));
}

// ---- the teleporter pads (class 1135; novalis_teleporter.cpp)
struct Tele {
    uint32_t update, trigger, hint, hide, show;
};
Tele tele_here() {
    switch (g_oracle_level) {
    case 1: return {0x00308bd8, 0x00308b28, 0x00309430, 0x003097e8, 0x00309838};
    case 7: return {0x0031c528, 0x0031c478, 0x0031cd80, 0x0031d138, 0x0031d188};
    case 8: return {0x003042a8, 0x003041f8, 0x00304b00, 0x00304eb8, 0x00304f08};
    case 12: return {0x00300ce8, 0x00300c38, 0x00301540, 0x003018f8, 0x00301948};
    case 13: return {0x00304888, 0x003047d8, 0x003050e0, 0x00305498, 0x003054e8};
    case 16: return {0x002e0120, 0x002e0070, 0x002e0978, 0x002e0d30, 0x002e0d80};
    case 17: return {0x002e4f30, 0x002e4e80, 0x002e5788, 0x002e5b40, 0x002e5b90};
    case 18: return {0x002edb78, 0x002edac8, 0x002ee3d0, 0x002ee788, 0x002ee7d8};
    default: return {0, 0, 0, 0, 0};
    }
}

// the level's pads, or (none placed) live mobys given a pad's pvars
std::vector<uint32_t> pads() {
    std::vector<uint32_t> v = mobys_of(1135);
    if (!v.empty()) return v;
    for (uint32_t m : live_mobys()) {
        v.push_back(m);
        if (v.size() >= 6) break;
    }
    return v;
}

// a pad past its first update (its three pieces spawned: the port's state 0 run on it), its springs and flags perturbed
// (batch 103's)
void tele_setup(uint32_t m) {
    if (ee_read<int16_t>(m + 0xa6) != 1135) {
        w32(m + 0x78, PV);
        for (uint32_t o = 0; o < 0x50; o += 4) w32(PV + o, 0);
        w32(PV, 0xffffffffu); w32(PV + 4, 0xffffffffu); w32(PV + 8, 0xffffffffu);
        w32(PV + 0x40, 0xffffffffu); w32(PV + 0x44, 0xffffffffu);
    }
    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
    w8(m + 0x20, 0);
    teleporter_update_l1(m);
    w8(m + 0x20, u(ri(0, 8)));
    for (uint32_t o : {0x20u, 0x24u, 0x28u, 0x2cu, 0x34u}) wf(pv + o, ri(0, 3) == 0 ? 0.0f : (ri(0, 3) == 0 ? 1.0f : rf(-0.3f, 1.3f)));
    w32(pv + 0x10, u(ri(0, 5)));
    w32(pv + 0x3c, u(ri(0, 1)));
    w32(pv + 0x38, u(ri(0, 1)));
    w32(pv + 0x30, u(ri(-1, 3)));
    w32(0x0015ee20, u(ri(0, 1)));
    w32(0x0015ed84, ri(0, 2) == 0 ? u(ri(0, 19)) : u(g_oracle_level));  // (the level number: the hint's tables, the bits)
}

void teleporter_checks() {
    const Tele t = tele_here();
    if (t.update == 0) {
        for (const char* n : {"teleporter_trigger", "teleporter_hint", "teleporter_hide", "teleporter_show", "teleporter_update"}) not_here(n);
        return;
    }
    g_extra_noop_hooks = {0x00122298};  // (the kernel vsync wait)
    for (int pass = 0; pass < 2; ++pass)
        classcheck("teleporter_trigger", t.trigger, pads, teleporter_trigger_l1, [](uint32_t m) {
            tele_setup(m);
            w8(m + 0x21, ri(0, 2) == 0 ? 0xffu : u(ri(0, 12)));
            w16(m + 0xb2, u(ri(0, 600)));
            const uint32_t first = first_moby();
            for (int i = 0; i < 8; ++i) w8(first + 0x100 * u(ri(0, 300)) + 0x20, R());  // (the group's states, some negative)
        }, 30);
    classcheck("teleporter_hint", t.hint, pads, teleporter_hint_l1, [](uint32_t m) {
        tele_setup(m);
        w32(0x0015ed84, u(ri(0, 19)));  // (every level's table)
        w32(ee_read<uint32_t>(m + 0x78) + 0x10, u(ri(0, ri(0, 1) == 0 ? 1 : 5)));
    }, 60);
    classcheck("teleporter_hide", t.hide, pads, teleporter_hide_l1, [](uint32_t m) {
        tele_setup(m);
        if (ri(0, 3) == 0) w32(ee_read<uint32_t>(m + 0x78) + 0x14 + 4 * u(ri(0, 2)), 0);
    }, 20);
    classcheck("teleporter_show", t.show, pads, teleporter_show_l1, [](uint32_t m) {
        tele_setup(m);
        if (ri(0, 3) == 0) w32(ee_read<uint32_t>(m + 0x78) + 0x14 + 4 * u(ri(0, 2)), 0);
    }, 20);
    // the pad's update in each state (the destination pad pvar +0, the hero on it or not)
    for (int st = 0; st < 10; ++st)
        for (int pass = 0; pass < 2; ++pass)
            classcheck("teleporter_update", t.update, pads, teleporter_update_l1, [st](uint32_t m) {
                tele_setup(m);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w8(m + 0x20, st == 9 ? u(ri(9, 255)) : u(st));
                const uint32_t first = first_moby();
                w32(pv, ri(0, 4) == 0 ? u(ri(-1, 3)) : 0u);
                for (uint32_t o = first; o < ee_read<uint32_t>(OV(0x0015ffe0)); o += 0x100)  // the destination: another pad
                    if (o != m && ee_read<int16_t>(o + 0xa6) == 1135 && ee_read<uint32_t>(o + 0x78) != 0 && ri(0, 4) != 0) {
                        w32(pv, (o - first) >> 8);
                        break;
                    }
                if (ee_read<int32_t>(pv) >= 0) {  // (a destination that is no pad: give it a pad's pvars)
                    const uint32_t d = first + (ee_read<uint32_t>(pv) << 8);
                    if (ee_read<int16_t>(d + 0xa6) != 1135 || ee_read<uint32_t>(d + 0x78) == 0) w32(d + 0x78, PV + 0x100);
                }
                if (ri(0, 2) == 0) w32(pv + 4, u(ri(-1, 12)));
                if (ri(0, 2) == 0) w32(pv + 8, u(ri(-1, 4)));
                if (coin()) w8(m + 0x21, ri(0, 2) == 0 ? 0xffu : u(ri(0, 12)));
                w32(pv + 0x40, ri(0, 2) == 0 ? u(ri(0, 3)) : 0xffffffffu);
                w32(pv + 0x44, ri(0, 2) == 0 ? u(ri(0, 3)) : 0xffffffffu);
                w32(0x0013cae4, coin() ? 0x10u : 0u);
                w32(OV(0x0015f594), ri(0, 3) == 0 ? 4u : u(ri(0, 5)));
                if (ri(0, 2) == 0) {
                    Vec4 q = ee_ref<Vec4>(m + 0x10);
                    q.x += rf(-1, 1);
                    q.y += rf(-1, 1);
                    ee_ref<Vec4>(0x0013f3d0) = q;
                }
                if (ri(0, 3) == 0) hero_in_box(ee_read<int32_t>(pv + 4));
                if (coin()) {  // (its bit in the level's tables: used)
                    const uint32_t idx = ee_read<uint16_t>(m + 0xb2);
                    const uint32_t lv = ee_read<uint32_t>(0x0015ed84);
                    if (lv < 20) w32(0x0014c190 + (lv << 8) + ((idx >> 5) << 2), coin() ? 0xffffffffu : 0u);
                    w8(OV(0x001bb230) + idx + 0x454, ri(0, 3) == 0 ? 1u : 0u);
                }
                if (coin()) {  // (the group's other flag table 0x14c050)
                    const uint32_t lv = ee_read<uint32_t>(0x0015ed84);
                    if (lv < 20) for (uint32_t i = 0; i < 16; ++i) w8(0x0014c050 + (lv << 4) + i, coin() ? 0xffu : u(ri(0, 3)));
                }
                // the springs at their ends: open (2 / 7: both at 1 or past), closed to the wait (4 / 8: +0x34 at 1.95 or past)
                if ((st == 2 || st == 7) && coin()) {
                    wf(pv + 0x20, coin() ? 1.0f : rf(1.0f, 1.3f));
                    if (ri(0, 3) != 0) wf(pv + 0x24, coin() ? 1.0f : rf(1.0f, 1.3f));
                }
                if ((st == 3 || st == 4 || st == 8) && coin()) {
                    if (coin()) w32(pv + 0x34, 0x3ff9999au);  // (1.95)
                    else wf(pv + 0x34, rf(1.95f, 2.5f));
                    if (coin()) wf(pv + 0x24, coin() ? 0.0f : rf(-0.2f, 1.0f));
                    if (coin()) wf(pv + 0x20, coin() ? 0.0f : rf(-0.2f, 1.0f));
                }
                if ((st == 2 || st == 7) && coin()) {  // (the hero's state: 0x67 lets the pad send him, 0x65 / 0x66 wait)
                    static const uint32_t hs[] = {0x64, 0x65, 0x66, 0x67, 0x67};
                    w32(P + 0x2084, hs[ri(0, 4)]);
                }
                if (st == 3 && coin()) {  // (the count over: the jump, with the music pair or not)
                    w32(pv + 0x34, 0);
                    if (coin()) { w32(pv + 0x40, u(ri(0, 3))); w32(pv + 0x44, u(ri(0, 3))); }
                }
                if ((st == 1 || st == 5 || st == 6) && coin()) {  // (the hero in the pad's box, the pad not used: stepped on)
                    w32(pv + 4, u(ri(0, 3)));
                    hero_in_box(ee_read<int32_t>(pv + 4));
                    const uint32_t idx = ee_read<uint16_t>(m + 0xb2);
                    const uint32_t lv = ee_read<uint32_t>(0x0015ed84);
                    if (lv < 20) w32(0x0014c190 + (lv << 8) + ((idx >> 5) << 2), 0u);
                    w8(OV(0x001bb230) + idx + 0x454, 0u);
                }
                w32(P + 0x2fc, coin() ? m : 0u);
                w16(P + 0x30e, ri(0, 3) == 0 ? 1u : 0u);
                w8(P + 0x20a4, ri(0, 3) == 0 ? 2u : 0u);
                sound_ready();
                seed_rand();
            }, 30);
    g_extra_noop_hooks.clear();
}

// ---- the lift (class 726 / 1141; novalis_platforms.cpp)
// the level's paths (0x1b04b0: 2 .. 64 points) by index
std::vector<uint32_t> level_paths() {
    std::vector<uint32_t> v;
    for (uint32_t i = 0; i < 64; ++i) {
        const uint32_t p = ee_read<uint32_t>(OV(0x001b04b0) + i * 4);
        if (p >= 0x00100000u && p < 0x02000000u && ee_read<int32_t>(p) >= 2 && ee_read<int32_t>(p) <= 64) v.push_back(i);
    }
    return v;
}

void lift_checks() {
    const uint32_t lift = copy_here({{1, 0x002b9eb0}, {3, 0x00291dd0}, {10, 0x002de860}, {13, 0x002b0478}});
    if (lift == 0) { not_here("lift_update"); return; }
    level();
    const std::vector<uint32_t> paths = level_paths();
    if (paths.empty()) { std::printf("lift_update                L%d: no path on this level\n", g_oracle_level); return; }
    const uint32_t laddr = lift;
    auto subjects = [laddr] {
        std::vector<uint32_t> v = mobys_with(laddr);
        if (!v.empty()) return v;
        for (uint32_t m : live_mobys()) {
            v.push_back(m);
            if (v.size() >= 6) break;
        }
        return v;
    };
    for (int pass = 0; pass < 6; ++pass)
        classcheck("lift_update", lift, subjects, lift_update_l1, [pass, laddr, paths](uint32_t m) {
            if (ee_read<uint32_t>(m + 0x74) != laddr || ri(0, 4) == 0) {  // (a lift's pvars on one of the level's paths)
                w32(m + 0x78, PV);
                for (uint32_t o = 0; o < 0xd0; o += 4) w32(PV + o, 0);
                w32(PV + 0xb4, paths[static_cast<size_t>(ri(0, static_cast<int>(paths.size()) - 1))]);
                w32(PV + 0xcc, 0xffffffffu);
                w32(PV + 0xc0, 0xffffffffu);
                w32(PV + 0xc4, 0xffffffffu);
            }
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const uint32_t level_no = ee_read<uint32_t>(0x0015ed84);
            w8(m + 0x20, pass < 2 ? u(pass) : 1u);
            w8(m + 0xbc, pass < 2 ? u(ri(0, 3)) : u(ri(0, 2)));
            if (ri(0, 6) == 0) w32(pv + 0xb4, 0xffffffffu);
            if (ri(0, 3) == 0) w32(pv + 0xcc, u(ri(-1, 3)));
            if (ri(0, 3) == 0) w32(pv + 0xc0, u(ri(-1, 3)));
            w16(pv + 0xba, ri(0, 2) == 0 ? u(ri(0, 4)) : 0u);
            w16(pv + 0xb8, R() & 1);
            wf(pv + 0xb0, rf(0.05f, 1.0f));
            wf(pv + 0xa4, rf(-0.2f, 1.2f));
            wf(pv + 0xa8, rf(-0.05f, 0.05f));
            wf(pv + 0xac, rf(-0.05f, 0.05f));
            if (ri(0, 4) == 0) wf(pv + 0xac, 0);
            wf(pv + 0xbc, rf(-5, 5));
            w8(pv + 0xa0, R() & 1);
            w32(pv + 0xc8, R() & 1);
            if (pass >= 2 && ri(0, 2) == 0) wf(pv + 0xa4, coin() ? rf(0.97f, 1.01f) : rf(-0.01f, 0.03f));
            sound_slot(m, pv + 0xc4);
            hero_near(m);
            if (ri(0, 2) == 0) {  // (the hero on the lift, or just above / below it: a descending lift's hop from 0.9 .. 4 below)
                Vec4 q = ee_ref<Vec4>(m + 0x10);
                q.x += rf(-2, 2);
                q.y += rf(-2, 2);
                q.z += coin() ? rf(-0.5f, 3) : (ri(0, 2) != 0 ? -rf(0.5f, 4.5f) : (coin() ? rf(4, 6) : -rf(4, 6)));
                ee_ref<Vec4>(0x0013f3d0) = q;
            }
            if (ri(0, 4) == 0) {  // (the hero at one of the path's ends)
                const uint32_t path = ee_read<uint32_t>(OV(0x001b04b0) + ee_read<uint32_t>(pv + 0xb4) * 4);
                if (ee_read<int32_t>(pv + 0xb4) >= 0 && path != 0) {
                    const int32_t n = ee_read<int32_t>(path);
                    Vec4 q = ee_ref<Vec4>(path + 0x10 + (coin() ? 0u : u(n - 1) * 16));
                    q.x += rf(-3, 3);
                    q.y += rf(-3, 3);
                    ee_ref<Vec4>(0x0013f3d0) = q;
                }
            }
            static const uint32_t modes[] = {3, 0, 1, 2}, states[] = {0x1c, 0, 5, 0x13};
            w32(P + 0x208c, modes[ri(0, 3)]);
            w32(P + 0x2084, states[ri(0, 3)]);
            w32(P + 0x4f8, coin() ? m : 0u);
            w32(P + 0x2fc, coin() ? m : 0u);
            w16(P + 0x30e, ri(0, 3) == 0 ? 1u : 0u);
            if (coin()) hero_in_box(static_cast<int32_t>(ee_read<uint32_t>(pv + (coin() ? 0xccu : 0xc0u))));
            const uint32_t idx = u(ri(0, 0x7f));
            w16(m + 0xb2, idx);
            w8(OV(0x001bb230) + idx + 0x454, ri(0, 3) == 0 ? 1u : 0u);
            if (level_no < 20)
                for (uint32_t i = 0; i < 4; ++i) w32(0x0014c190 + level_no * 256 + i * 4, ri(0, 3) == 0 ? R() : 0u);
        }, 50);
}

// ---- the gold weapon case (classes 304, 1456 .. 1465; novalis_cases.cpp)
void case_checks() {
    const uint32_t gcase = copy_here({{1, 0x002e1ac0}, {13, 0x002e7238}});
    if (gcase == 0) { not_here("gold_case_update"); return; }
    // the level's case table (each case's weapon), read where the original reads it: the port's own word
    const uint32_t table = g_oracle_level == 13 ? ORACLE_LADDR(13, 0x001d3420u) : ORACLE_LADDR(1, 0x001deb10u);
    const uint32_t laddr = gcase;
    auto subjects = [laddr] {
        std::vector<uint32_t> v = mobys_with(laddr);
        if (!v.empty()) return v;
        for (uint32_t m : live_mobys()) {
            v.push_back(m);
            if (v.size() >= 6) break;
        }
        return v;
    };
    g_extra_noop_hooks = {0x00122298};  // (the kernel vsync wait)
    for (int pass = 0; pass < 6; ++pass)
        classcheck("gold_case_update", gcase, subjects, gold_case_update_l1, [pass, laddr, table](uint32_t m) {
            sound_ready();
            if (ee_read<uint32_t>(m + 0x74) != laddr) {  // (a case's pvars: a talk record)
                w32(m + 0x78, PV);
                for (uint32_t o = 0; o < 0x50; o += 4) w32(PV + o, 0);
            }
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, pass == 0 ? u(ri(0, 5)) : u(pass - 1));
            if (ri(0, 2) == 0 || ee_read<uint32_t>(m + 0x74) != laddr) w32(pv + 0x40, u(ri(-1, 9)));
            w32(0x0015ee20, u(ri(0, 1)));
            if (ri(0, 2) == 0) w16(m + 0xa6, coin() ? 0x130u : u(ri(1456, 1465)));
            if (ee_read<uint32_t>(pv + 0x40) < 12) {
                const uint32_t w = ee_read<uint32_t>(table + 4 * ee_read<uint32_t>(pv + 0x40));
                if (w < 0x100) w8(0x0013e520 + w, u(ri(0, 1)));
            }
            {  // (state 1 answered 2 starts the weapon scene: not run here, as batch 103)
                const uint32_t ans = u(ri(0, 3));
                w16(pv + 4, ee_read<uint8_t>(m + 0x20) == 1 && ans == 2 ? 3u : ans);
            }
            w32(0x0015ed98, R() & 0xffff);
            // (state 4 with no scripted scene (0x15f5c4 == 0) asks for the level change, which the original runs for real (the
            // IOP's replies: the interpreter's budget runs out); kept 1 as batch 103, that branch is not compared)
            w32(OV(0x0015f5c4), u(ri(1, 6)));
            seed_rand();
        }, 50);
    g_extra_noop_hooks.clear();
}

// ---- the ocean's helpers (aridia_ocean.cpp)
constexpr uint32_t SPR = 0x70000000u;

// the frame's packet cursor (0x1611c0) on a buffer of the test area (after the first gameplay frame it is 0: the packets would go
// to the null page, which the comparison leaves out)
void packet_cursor() { w32(OV(0x001611c0), TEST_AREA + 0x8000 + u(ri(0, 0x40)) * 0x10); }

// the camera (Veldin's 0x166dc0) moved near a point, its view rows (0x166fd0) turned about z; the render frame's view made
// again (batch 137's)
void move_camera(float x, float y, float z, float spread) {
    Vec4& c = VV(OV(0x00166dc0));
    c.x = x + rf(-spread, spread);
    c.y = y + rf(-spread, spread);
    c.z = z + rf(0.5f, spread / 3);
    if (coin()) {
        const float a = rf(-3.14f, 3.14f), cs = std::cos(a), sn = std::sin(a);
        for (uint32_t r = OV(0x00166fd0); r < OV(0x00166fd0) + 0x30; r += 0x10) {
            Vec4& v = VV(r);
            const float px = v.x, py = v.y;
            v.x = cs * px - sn * py;
            v.y = sn * px + cs * py;
        }
    }
    render_view_setup();
}

// the camera over a random point of a record's grid (origin +0, step +0x10, counts +0x18 / +0x1a)
void camera_over(uint32_t rec, float spread) {
    const float ox = ee_read<float>(rec), oy = ee_read<float>(rec + 4), oz = ee_read<float>(rec + 8);
    const float st = ee_read<float>(rec + 0x10);
    const float w = st * ee_read<uint16_t>(rec + 0x18), h = st * ee_read<uint16_t>(rec + 0x1a);
    move_camera(ox + rf(0.0f, w), oy + rf(0.0f, h), oz + 2.0f, spread);
}

// the level's ocean records: those its draw callback passes to its one-record draw (Aridia: its seven surfaces)
std::vector<uint32_t> ocean_records() {
    switch (g_oracle_level) {
    case 2: return {ORACLE_LADDR(2, 0x001f3c00u), ORACLE_LADDR(2, 0x001f3c60u), ORACLE_LADDR(2, 0x001f3cd0u), ORACLE_LADDR(2, 0x001f3d30u),
                    ORACLE_LADDR(2, 0x001f3d90u), ORACLE_LADDR(2, 0x001f3dd0u), ORACLE_LADDR(2, 0x001f3e10u)};
    case 3: return {ORACLE_LADDR(3, 0x001dc1a0u)};
    case 5: return {ORACLE_LADDR(5, 0x00211b20u)};
    case 7: return {ORACLE_LADDR(7, 0x001d3400u), ORACLE_LADDR(7, 0x00208560u)};
    case 8: return {ORACLE_LADDR(8, 0x001d7060u)};
    case 9: return {ORACLE_LADDR(9, 0x001fc740u)};
    case 14: return {ORACLE_LADDR(14, 0x001e9c60u)};
    default: return {};
    }
}

// a record of the ocean, the camera over it (or away: cells culled), the packet cursor on the test buffer
uint32_t ocean_record(bool far) {
    level();
    packet_cursor();
    const std::vector<uint32_t> recs = ocean_records();
    const uint32_t r = pick(recs);
    camera_over(r, far ? (ri(0, 3) == 0 ? 600.0f : 120.0f) : 15.0f);  // (600: past the records' cull distance)
    if (ri(0, 3) == 0) w32(OV(0x0015f5cc), R() & 0xffffu);  // (the frame counter: the texture's frames)
    return r;
}

// Aridia's set-up record as its draw callback builds it (batch 137's), the other levels' own records
uint32_t setup_record() {
    if (g_oracle_level != 2) {
        const uint32_t r = ocean_record(coin());
        if (ri(0, 2) == 0) w32(OV(0x0015f458), u(ri(0, 2)));  // (the fog's mode the set-up tests)
        return r;
    }
    level();
    packet_cursor();
    camera_over(ocean_records()[static_cast<size_t>(ri(0, 6))], 100.0f);
    const uint32_t r = T + 0x400;
    for (uint32_t k = 0; k < 3; ++k) w8(r + 0x30 + k, static_cast<uint8_t>(R()));
    w8(r + 0x33, static_cast<uint8_t>(ri(1, 60)));
    wf(r + 0x20, coin() ? 0.0f : rf(0.0f, 50.0f));
    wf(r + 0x24, rf(1000.0f, 300000.0f));
    wf(r + 0x28, coin() ? 255.0f : rf(0.0f, 255.0f));
    wf(r + 0x2c, coin() ? 0.0f : rf(0.0f, 255.0f));
    w16(r + 0x3c, 0x28);
    w8(r + 0x3e, static_cast<uint8_t>(ri(1, 0x40)));
    w8(r + 0x3f, static_cast<uint8_t>(ri(0x40, 0xff)));
    w32(r + 0x1c, ORACLE_LADDR(2, 0x001f3e80u));
    if (coin()) w32(OV(0x0015f5cc), R() & 0xffffu);
    if (ri(0, 2) == 0) w32(OV(0x0015f458), u(ri(0, 2)));
    return r;
}

void ocean_checks() {
    struct Fam {
        const char* name;
        std::initializer_list<LvAddr> copies;
    };
    const uint32_t vu7 = copy_here({{2, 0x002a40f0}, {3, 0x00291218}, {5, 0x002ce130}, {7, 0x002cc6a0}, {8, 0x002acd08}, {9, 0x002c1220}, {14, 0x002aac78}});
    if (vu7 == 0) {
        for (const char* n : {"ocean_vu7_setup", "ocean_spr_dma_to", "ocean_end_body", "ocean_cells_clip", "ocean_cells_draw", "ocean_tex_upload", "ocean_draw_one"})
            not_here(n);
        return;
    }
    const uint32_t spr = copy_here({{2, 0x002101b8}, {3, 0x001f8ae8}, {5, 0x0022de38}, {7, 0x00226fb8}, {8, 0x00211cd0}, {9, 0x0021ee30}, {14, 0x00212760}});
    const uint32_t endb = copy_here({{2, 0x002a46e0}, {3, 0x00291808}, {5, 0x002ce720}, {7, 0x002ccc90}, {8, 0x002ad2f8}, {9, 0x002c1810}, {14, 0x002ab268}});
    const uint32_t clip = copy_here({{2, 0x0024f748}, {3, 0x0023bed0}, {5, 0x002775c8}, {7, 0x00275d88}, {8, 0x002577e8}, {9, 0x0026cce8}, {14, 0x00255788}});
    const uint32_t draw = copy_here({{2, 0x0024fa80}, {3, 0x0023c208}, {5, 0x00277900}, {7, 0x002760c0}, {8, 0x00257b20}, {9, 0x0026d020}, {14, 0x00255ac0}});
    const uint32_t tex = copy_here({{2, 0x0024fca0}, {3, 0x0023c428}, {5, 0x00277b20}, {7, 0x002762e0}, {8, 0x00257d40}, {9, 0x0026d240}, {14, 0x00255ce0}});
    const uint32_t one = copy_here({{3, 0x00291918}, {5, 0x002ce830}, {7, 0x002ccda0}, {8, 0x002ad408}, {9, 0x002c1920}, {14, 0x002ab378}});

    fcheck("ocean_vu7_setup", vu7, 120, Ret::None, [] { return Args{{setup_record()}, {}}; },
           [](A a) { ari_ocean_vu7_setup_l2(a.i[0]); return uint64_t{0}; });
    fcheck("ocean_spr_dma_to", spr, 30, Ret::None, [] {
        level();
        wrand(T, 0x400);
        return Args{{T + u(ri(0, 15)) * 16, SPR + u(ri(0, 0x300)) * 16, u(ri(1, 0xf8))}, {}};
    }, [](A a) { ari_spr_dma_to_l2(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
    fcheck("ocean_end_body", endb, 40, Ret::None, [] {
        level();
        packet_cursor();
        wfrand(OV(0x001612f8), 4, 0.0f, 300000.0f);  // (the saved fog / far)
        if (coin()) wfrand(OV(0x0016ca40) + 0x230, 3, 0.0f, 255.0f);  // (the fog colour)
        return Args{{R()}, {}};
    }, [](A a) { ari_ocean_end_body_l2(a.i[0]); return uint64_t{0}; });
    fcheck("ocean_cells_clip", clip, 120, Ret::None, [] { return Args{{ocean_record(coin())}, {}}; },
           [](A a) { ari_ocean_cells_clip_l2(a.i[0]); return uint64_t{0}; });
    fcheck("ocean_cells_draw", draw, 120, Ret::None, [] {
        const uint32_t r = ocean_record(coin());
        const int n = ee_read<uint16_t>(r + 0x18) / 7 * (ee_read<uint16_t>(r + 0x1a) / 7) + 1;
        for (int i = 0; i < n && i < 0x800; ++i) w8(SPR + 0x2000 + u(i), static_cast<uint8_t>(ri(0, 2) != 0 ? 0 : ri(1, 2)));  // the cells' bytes
        for (uint32_t a = SPR; a < SPR + 0x200; a += 4) wf(a, rf(-300.0f, 300.0f));
        for (uint32_t a = SPR + 0x2800; a < SPR + 0x2800 + 0xf8; a += 2) w16(a, u(ri(0, 63)) * 8);  // (the uv table's point offsets)
        return Args{{r}, {}};
    }, [](A a) { ari_ocean_cells_draw_l2(a.i[0]); return uint64_t{0}; });
    fcheck("ocean_tex_upload", tex, 60, Ret::None, [] {
        level();
        const uint32_t src = T2;
        for (uint32_t a = src; a < src + 0x2000; a += 4) w32(a, R());
        const uint32_t count = u(ri(1, 0x100)) * 16;
        return Args{{TEST_AREA + 0x8000, src + u(ri(0, 0x40)) * 16, src + 0x1400, src + u(ri(0, 0x40)) * 16, src + 0x1800, count},
                    {ri(0, 4) == 0 ? (coin() ? 0.0f : 1.0f) : rf(0.0f, 1.0f)}};
    }, [](A a) { ari_ocean_tex_upload_l2(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5], a.f[0]); return uint64_t{0}; });
    if (one == 0) not_here("ocean_draw_one");
    else
        fcheck("ocean_draw_one", one, 120, Ret::None, [] {
            const uint32_t r = ocean_record(ri(0, 3) == 0);
            if (ri(0, 2) == 0) w32(OV(0x0015f458), u(ri(0, 2)));
            return Args{{r}, {}};
        }, [](A a) { ocean_draw_one(a.i[0]); return uint64_t{0}; });
}
}  // namespace

void batch_513() {
    if (g_only_batch != 513) return;
    g_batch = 513;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    if (g_oracle_level < 1 || g_oracle_level > 18) {
        std::printf("batch 513: levels 1 .. 18 only\n");
    } else {
        level();
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

        teleporter_checks();
        lift_checks();
        case_checks();
        ocean_checks();
    }

    g_pre_call = nullptr;
    g_extra_noop_hooks.clear();
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
