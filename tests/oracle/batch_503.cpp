// Oracle batch 503 (stage 4 task G3, #149): the hand-written ports of Aridia's ship draws, background-ship glows, ocean and
// props helpers, generalised to serve every level that carries a copy of the same code: each check calls the original at the
// level's own copy and runs the one hand-written port. The level copies stay transliterated in their units until each level
// is re-glued (phase 2); here every copy is the original and the hand-written port the port:
//  - aridia_ship.cpp: the ship's shadow / glow 0x28e670, engine glow 0x28efc8 and exhaust trails 0x28f238 (Veldin's
//    addresses: every level's copy, where the level's overlay map pairs it; Aridia's 0x28dde0 / 0x28e738 / 0x28e9a8);
//  - aridia_ocean.cpp: the background ships' glows, class 1212's (levels 2, 9) and class 1213's / 1973's (levels 2, 9, 10),
//    and the packet's VIF tag 0x2a0b60 (dma_cnt_vif, Veldin's port the ocean now calls: every level's copy);
//  - aridia_props.cpp: class 331's update (levels 2, 8, 18), the escalator (levels 2, 12: Hoven's class 296), the draw-hook
//    appenders of the third list (levels 2, 5, 7, 9, 12, 14) and the fourth (levels 14, 15), the ocean's wave table
//    (levels 2, 3, 5, 7, 8, 9, 14) and Aridia's call of it (level 2);
//  - shared_race_camera.cpp: the hoverboard races' camera (camera type 8 of Rilgar and Kalebo III, levels 5 and 16): its start,
//    frame and every helper (the set-up, the targets, the basis, the board's point, the orbit, the follow).
// Subjects: the level after its first gameplay frame (the port's) with its view set up on a 640 x 448 screen; the camera moved
// so that the draws' quads are on the screen; the level's own mobys; records and lists in the test area; the packets at a cursor
// on a buffer of the test area (B503_INFO=1 prints how far each case's packets grew). No RAM dumps.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 503 for N in 0, 2, 3, 5, 7, 8, 9, 10, 12, 14, 15, 16 and 18; a function
// the level does not carry says "not on level N" and is not run.

#include "ovl_oracle.h"

#include <game/ovl/aridia_mobys.h>
#include <game/ovl/aridia_ocean.h>
#include <game/ovl/aridia_ship.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_race_camera.h>
#include <runtime/kernel/disc.h>
#include <runtime/vu/vu0.h>

#include <cmath>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <vector>

namespace rac1::oracle {

namespace {
// The VU0 the original started from (the interpreter's), given to the port before it runs (as batches 501 / 506)
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

bool info() { return std::getenv("B503_INFO") != nullptr; }

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

// the frame's packet cursor (0x1611c0)
uint32_t cursor() { return ee_read<uint32_t>(OV(0x001611c0)); }

// A check whose original is the level's own copy at `laddr`; the port gets the interpreter's VU0 and, with B503_INFO, says how
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

// Each copy of the family on this level checked (or "not on level N").
void each_copy(const char* name, std::initializer_list<LvAddr> copies, const std::function<void(uint32_t)>& run) {
    const std::vector<uint32_t> here = copies_here(copies);
    if (here.empty()) { not_here(name); return; }
    for (uint32_t a : here) run(a);
}

// The level's copy of Veldin's engine function `veldin` (the overlay map's pairing; Veldin's own on level 0), checked.
void engine_copy(const char* name, uint32_t veldin, const std::function<void(uint32_t)>& run) {
    const uint32_t a = g_oracle_level == 0 ? veldin : ovl_code_lookup(veldin);
    if (a == 0) { not_here(name); return; }
    run(a);
}

// the level after its first gameplay frame (the port's) and its view set up, as batch 501
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

// a point near `at` (xy within r, z within rz)
Vec4 near(const Vec4& at, float r, float rz) {
    Vec4 q = at;
    q.x += rf(-r, r);
    q.y += rf(-r, r);
    q.z += rf(-rz, rz);
    return q;
}

uint32_t pick(const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; }

// the frame's packet cursor (0x1611c0) on a buffer of the test area (after the first gameplay frame it is 0: the packets
// would go to the null page, which the comparison leaves out)
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

// the camera moved (and turned) about the point at `p` until the screen test (point_on_screen, 0x2002e0) sees it
void see(uint32_t p, float spread) {
    for (int k = 0; k < 80; ++k) {
        move_camera(ee_read<float>(p) + rf(-spread, spread), ee_read<float>(p + 4) + rf(-spread, spread), ee_read<float>(p + 8), spread / 2);
        if (point_on_screen(p, 0.0f) != -1) return;
    }
}

// a point the screen test sees (`want` 1: on the screen, 0: in the guard band, -1: not), near the camera
Vec4 screen_point(int64_t want, float r) {
    Vec4 c{};
    for (int k = 0; k < 4000; ++k) {
        Vec4 q = VV(OV(0x00166dc0));
        const float s = k < 2000 ? 30.0f : 120.0f;
        q.x += rf(-s, s); q.y += rf(-s, s); q.z += rf(-s / 3, s / 3);
        q.w = 1.0f;
        c = q;
        if (point_on_screen_v(c, r) == want) break;
    }
    return c;
}

// ---- the ship's draw callbacks (aridia_ship.cpp): the subject the ship *(0x13e030) when the level has one, else a live moby made
// the ship; it and its bounding point (+0..: the position x 1024) at a point the screen test with radius `r` sees (the shadow's
// test has 32), or one it does not
uint32_t ship_subject(float r) {
    level();
    packet_cursor();
    uint32_t m = ee_read<uint32_t>(0x0013e030);
    if (m == 0 || coin()) {
        m = pick(live_mobys());
        w32(0x0013e030, m);
    }
    const int64_t want = ri(0, 4) == 0 ? ri(-1, 0) : 1;
    const Vec4 c = screen_point(want, r);
    VV(m + 0x10) = c;
    wf(m, c.x * 1024.0f); wf(m + 4, c.y * 1024.0f); wf(m + 8, c.z * 1024.0f); wf(m + 0xc, (ri(0, 3) != 0 ? 1.0f : rf(0.5f, 2.0f)) * 1024.0f);
    if (coin()) {  // its matrix turned
        const float a = rf(-3.1f, 3.1f), cs = std::cos(a), sn = std::sin(a);
        wf(m + 0xc0, cs); wf(m + 0xc4, sn); wf(m + 0xc8, 0.0f);
        wf(m + 0xd0, -sn); wf(m + 0xd4, cs); wf(m + 0xd8, 0.0f);
        wf(m + 0xe0, 0.0f); wf(m + 0xe4, 0.0f); wf(m + 0xe8, 1.0f);
    }
    w16(0x0013e056, u(ri(0, 2)));  // (the ship's kind: its corners, glows, colours, widths)
    if (coin()) w16(m + 0xb2, u(ri(0, 3) == 0 ? ri(1, 40) : 0));  // (the engine glow's jitter: rand_mod)
    if (coin()) w8(m + 0xbc, R());  // (its intensity)
    if (ri(0, 3) == 0) w16(m + 0xa6, 0x215);  // (the colour of class 0x215)
    if (coin()) {  // the landing / take-off (game mode 6, its step and frame: the shadow fading, the trails' texture)
        w32(OV(0x0015f5c4), 6);
        w32(0x0013e050, coin() ? 3u : u(ri(0, 5)));
        w16(0x0013e054, u(ri(0, 600)));
    } else if (coin()) {
        w32(OV(0x0015f5c4), u(ri(0, 7)));
    }
    seed_rand();
    return m;
}

void ship_checks() {
    engine_copy("ship_shadow_draw", 0x0028e670, [](uint32_t a) {
        fcheck("ship_shadow_draw", a, 100, Ret::None, [] { return Args{{ship_subject(32.0f)}, {}}; },
               [](A a) { ari_ship_glow_draw_l2(a.i[0]); return uint64_t{0}; });
    });
    engine_copy("ship_flame_draw", 0x0028efc8, [](uint32_t a) {
        fcheck("ship_flame_draw", a, 100, Ret::None, [] { return Args{{ship_subject(0.0f)}, {}}; },
               [](A a) { ari_ship_engine_glow_draw_l2(a.i[0]); return uint64_t{0}; });
    });
    engine_copy("ship_trails_draw", 0x0028f238, [](uint32_t a) {
        fcheck("ship_trails_draw", a, 100, Ret::None, [] {
            const uint32_t m = ship_subject(0.0f);
            // the exhaust trails: two rings of 32 points (0x13e0f0 / 0x13e2f0, 16 bytes each) about the point, the head 0x13e080,
            // the count 0x13e084
            if (ri(0, 5) != 0) {
                w32(0x0013e084, u(ri(0, 5) == 0 ? ri(0, 2) : ri(0, 32)));
                w32(0x0013e080, u(ri(0, 63)));
                const Vec4 s = VV(m + 0x10);
                for (uint32_t k = 0; k < 64; ++k) {
                    Vec4& q = VV(0x0013e0f0 + k * 0x10);
                    q.x = s.x + rf(-8.0f, 8.0f);
                    q.y = s.y + rf(-8.0f, 8.0f);
                    q.z = s.z + rf(-3.0f, 6.0f);
                    q.w = 1.0f;
                }
            }
            return Args{{m}, {}};
        }, [](A a) { ari_ship_takeoff_draw_l2(a.i[0]); return uint64_t{0}; });
    });
}

// ---- the packet's VIF tag (dma_cnt_vif, Veldin's port of 0x2a0b60 the ocean calls in place of Aridia's 0x2a0368)
void vif_checks() {
    engine_copy("dma_cnt_vif", 0x002a0b60, [](uint32_t a) {
        fcheck("dma_cnt_vif", a, 20, Ret::None, [] {
            level();
            packet_cursor();
            return Args{{coin() ? 0x13000000u : R()}, {}};
        }, [](A a) { dma_cnt_vif(a.i[0]); return uint64_t{0}; });
    });
}

// ---- the background ships' glows (aridia_ocean.cpp): the ships of the class (their group +0x21), drawn this frame or not
// (+0x31), their glows' points (pvars +0x80..) about them and colours (pvar `col`) about the 0x80 threshold, a glow seen
void bg_glow_checks() {
    struct Ship {
        const char* name;
        std::initializer_list<LvAddr> copies;
        void (*fn)(uint32_t);
        uint32_t col;    // the glows' colour pvar (ship 1: +0xb0, three glows; ship 2: +0xe0, six)
        uint32_t upd[3];  // the class's update on levels 2, 9, 10 (its mobys the subjects)
    };
    const Ship ships[] = {
        {"bg_ship1_glow_draw", {{2, 0x002ec308}, {9, 0x00305c10}}, ari_bg_ship1_glow_draw_l2, 0xb0, {0x002ec4b8, 0x00305dc0, 0}},
        {"bg_ship2_glow_draw", {{2, 0x002ecc68}, {9, 0x00306570}, {10, 0x002eb958}}, ari_bg_ship2_glow_draw_l2, 0xe0, {0x002ece18, 0x00306720, 0x002ebb08}},
    };
    for (const Ship& s : ships) {
        each_copy(s.name, s.copies, [&s](uint32_t a) {
            level();
            const uint32_t upd = g_oracle_level == 2 ? s.upd[0] : g_oracle_level == 9 ? s.upd[1] : s.upd[2];
            const std::vector<uint32_t> ms = mobys_with(upd);
            std::printf("  (%s: %zu ships)\n", s.name, ms.size());
            if (info()) {
                for (uint32_t m : ms) {
                    const uint32_t g = ee_read<uint8_t>(m + 0x21);
                    const uint32_t list = g < 0x80 ? ee_read<uint32_t>(OV(0x001ab840) + g * 4) : 0u;
                    std::printf("INFO ship %08x class %u group %u list %08x:", m, ee_read<uint16_t>(m + 0xa6), g, list);
                    for (uint32_t k = 0; list != 0 && k < 16; ++k) {
                        const uint32_t id = ee_read<uint16_t>(list + 2 * k);
                        const uint32_t o = first_moby() + ((id & 0x7fffu) << 8);
                        std::printf(" %08x/%u", o, ee_read<uint16_t>(o + 0xa6));
                        if ((id & 0x8000u) != 0) break;
                    }
                    std::printf("\n");
                }
            }
            if (ms.empty()) { std::printf("%-26s no ship on this level\n", s.name); return; }
            const uint32_t col = s.col;
            fcheck(s.name, a, 80, Ret::None, [ms, col] {
                level();
                packet_cursor();
                for (uint32_t m : ms) {
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    w8(m + 0x31, ri(0, 3) != 0 ? 1u : 0u);  // (drawn this frame: its glows only then)
                    for (uint32_t g = 0; g < (col - 0x80) / 0x10; ++g) {  // its glows' points (+0x80..) about it
                        Vec4& q = VV(pv + 0x80 + g * 0x10);
                        q = VV(m + 0x10);
                        q.x += rf(-4.0f, 4.0f);
                        q.y += rf(-4.0f, 4.0f);
                        q.z += rf(0.0f, 6.0f);
                    }
                    if (ri(0, 3) != 0) w8(pv + col, u(coin() ? ri(0x81, 0xff) : ri(0x60, 0x81)));
                    if (coin()) w8(pv + col + 1, R());
                }
                if (g_oracle_level == 10 || ri(0, 3) == 0) {
                    // the ships in a group of their own (Orxon's have none, +0x21 0xff: their glows are never drawn): the list of
                    // their ids (the last with bit 15, a live moby among them) at T2, put in the place of the level's group 0..
                    const int32_t ngroups = ee_read<int32_t>(OV(0x0015fff4));
                    const uint32_t g = u(ri(0, ngroups < 0 ? 0 : (ngroups < 8 ? ngroups : 8)));
                    std::vector<uint32_t> ids;
                    for (uint32_t m : ms) ids.push_back((m - first_moby()) >> 8);
                    if (coin()) ids.push_back((pick(live_mobys()) - first_moby()) >> 8);
                    for (size_t k = 0; k < ids.size(); ++k) w16(T2 + 2 * u(static_cast<int32_t>(k)), ids[k] | (k + 1 == ids.size() ? 0x8000u : 0u));
                    w32(OV(0x001ab840) + g * 4, T2);
                    for (uint32_t m : ms) w8(m + 0x21, g);
                }
                const uint32_t m0 = pick(ms);
                if (ri(0, 4) == 0) w16(pick(ms) + 0xa6, R() & 0xffff);  // (a ship of another class: passed over)
                if (ri(0, 3) != 0) see(ee_read<uint32_t>(m0 + 0x78) + 0x80 + u(ri(0, 2)) * 0x10, coin() ? 10.0f : 30.0f);  // (a glow seen)
                else move_camera(ee_read<float>(m0 + 0x10), ee_read<float>(m0 + 0x14), ee_read<float>(m0 + 0x18), 40.0f);
                return Args{{m0}, {}};
            }, [s](A a) { s.fn(a.i[0]); return uint64_t{0}; });
        });
    }
}

// ---- the props (aridia_props.cpp)
void props_checks() {
    // class 331: a moby below or above the end of the level's initial array (0x15ffdc)
    each_copy("trackdrive_left_update", {{2, 0x002cb978}, {8, 0x002da3c0}, {18, 0x002ce7d0}}, [](uint32_t a) {
        fcheck("trackdrive_left_update", a, 40, Ret::None, [] {
            level();
            const uint32_t m = pick(live_mobys());
            w16(m + 0x34, R()); w32(m + 0x58, R());
            const int k = ri(0, 3);
            if (k != 0) w32(OV(0x0015ffdc), k == 1 ? m : k == 2 ? m + 0x100 : m - 0x100);  // (the array's end: at / above / below it)
            return Args{{m}, {}};
        }, [](A a) { trackdrive_left_update_l2(a.i[0]); return uint64_t{0}; });
    });

    // the escalators (Aridia's classes 652 / 653, Hoven's 296): every state, the belt's words (Aridia's 0x161c14.., Hoven's
    // 0x161960..), the offset along the belt short / long, the timer running / expired
    each_copy("escalator_update", {{2, 0x002dc6b0}, {12, 0x002e8c10}}, [](uint32_t a) {
        level();
        const std::vector<uint32_t> ms = mobys_with(a);
        std::printf("  (escalator_update: %zu mobys)\n", ms.size());
        if (ms.empty()) { std::printf("%-26s no moby on this level\n", "escalator_update"); return; }
        const uint32_t words = g_oracle_level == 12 ? ORACLE_LADDR(12, 0x00161960u) : ORACLE_LADDR(2, 0x00161c14u);
        for (int pass = 0; pass < 8; ++pass)
            fcheck("escalator_update", a, 40, Ret::None, [ms, words, pass] {
                level();
                const uint32_t m = pick(ms);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w8(m + 0x20, pass < 5 ? u(pass) : u(ri(0, 4)));
                wf(m + 0x48, rf(-3.1f, 3.1f));
                wf(words, ri(0, 8) == 0 ? 0.0f : rf(-3, 3));
                w32(words + 4, u(ri(0, 8) == 0 ? 0 : ri(1, 90)));
                wf(words + 8, rf(0, 2));
                const float len = ri(0, 3) == 0 ? 0.0f : rf(0, 6), yaw = ee_read<float>(m + 0x48);
                wf(pv + 0x40, len * std::cos(yaw)); wf(pv + 0x44, len * std::sin(yaw)); wf(pv + 0x48, coin() ? 0.0f : rf(-2, 2));
                w32(pv + 0xa0, ri(0, 2) == 0 ? 1u : u(ri(0, 4)));
                w32(pv + 0x9c, R()); w32(pv + 0xa4, ri(0, 1) ? u(R() & 3) : 0u);
                wrand(pv + 0x60, 16);
                if (ri(0, 10) == 0) w32(m + 0x78, 0);  // (no pvars: nothing)
                return Args{{m}, {}};
            }, [](A a) { escalator_update_l2(a.i[0]); return uint64_t{0}; });
    });

    // the draw-hook appenders: the list's count at its ends (63, 64, more, negative) or anywhere
    auto hook_gen = [](uint32_t count) {
        return [count] {
            level();
            const int k = ri(0, 4);
            w32(count, k == 0 ? 0x3fu : k == 1 ? 0x40u : k == 2 ? u(ri(-3, 0x50)) : u(ri(0, 0x3f)));
            return Args{{R(), R()}, {}};
        };
    };
    each_copy("draw_hook_add_dc80", {{2, 0x0020a390}, {5, 0x00228110}, {7, 0x00221290}, {9, 0x00218810}, {12, 0x0021bf18}, {14, 0x0020c698}},
              [hook_gen](uint32_t a) {
        fcheck("draw_hook_add_dc80", a, 30, Ret::None, hook_gen(OV(0x0015f42c)), [](A a) { draw_hook_add_dc80_l2(a.i[0], a.i[1]); return uint64_t{0}; });
    });
    each_copy("draw_hook_add_de80", {{14, 0x0020c760}, {15, 0x001f2c08}}, [hook_gen](uint32_t a) {
        fcheck("draw_hook_add_de80", a, 30, Ret::None, hook_gen(OV(0x0015f430)), [](A a) { draw_hook_add_de80_l14(a.i[0], a.i[1]); return uint64_t{0}; });
    });

    // the ocean's wave table: a record in the test area, the alpha and the scale; the source table as the level has it or
    // random (large / small / special values among them)
    struct Src {
        int level;
        uint32_t src;
    };
    static const Src srcs[] = {{2, ORACLE_LADDR(2, 0x001cb500u)}, {3, ORACLE_LADDR(3, 0x001cb000u)}, {5, ORACLE_LADDR(5, 0x001cb380u)},
                               {7, ORACLE_LADDR(7, 0x001caf80u)}, {8, ORACLE_LADDR(8, 0x001cb700u)}, {9, ORACLE_LADDR(9, 0x001cb080u)},
                               {14, ORACLE_LADDR(14, 0x001cb600u)}};
    uint32_t src = 0;
    for (const Src& s : srcs)
        if (s.level == g_oracle_level) src = s.src;
    auto wave_gen = [src] {
        level();
        if (coin()) {
            for (uint32_t i = 0; i < 0xf8; ++i) {
                const int k = ri(0, 14);
                const uint32_t a = src + 0x10 + i * 4;
                if (k == 0) w32(a, R());
                else if (k == 1) wf(a, rf(-1e30f, 1e30f));
                else if (k == 2) wf(a, rf(-1e-30f, 1e-30f));
                else if (k == 3) w32(a, R() & 0x807fffffu);  // (a denormal: the PS2's zero)
                else if (k == 4) w32(a, 0x7f800000u | (R() & 0x807fffffu));  // (exponent 255: the PS2's number)
                else wf(a, rf(-4.0f, 4.0f));
            }
        }
        static const float ks[] = {0.0f, 1.0f, 0.16666667f, -1.0f, 1e20f, 1e-20f, 1e-10f, 3e-8f};
        return ks[ri(0, 7)];
    };
    each_copy("ocean_wave_table_set", {{2, 0x002a4058}, {3, 0x00291198}, {5, 0x002ce098}, {7, 0x002cc608}, {8, 0x002acc70}, {9, 0x002c11a0}, {14, 0x002aabf8}},
              [wave_gen](uint32_t a) {
        fcheck("ocean_wave_table_set", a, 30, Ret::None, [wave_gen] {
            const float k = coin() ? wave_gen() : (level(), rf(-3.0f, 3.0f));
            wrand(T, 0x10);
            return Args{{T, R() & 0xff}, {k}};
        }, [](A a) { ocean_wave_table_set(a.i[0], a.i[1], a.f[0]); return uint64_t{0}; });
    });
    each_copy("ocean_wave_scale", {{2, 0x002a47f8}}, [wave_gen](uint32_t a) {
        fcheck("ocean_wave_scale", a, 20, Ret::None, [wave_gen] {
            const float k = coin() ? wave_gen() : (level(), rf(-3.0f, 3.0f));
            return Args{{}, {k}};
        }, [](A a) { ocean_wave_scale_l2(a.f[0]); return uint64_t{0}; });
    });
}
// ---- the hoverboard races' camera (shared_race_camera.cpp; levels 5, 16)
// a rotation's three rows at `a` (yaw, pitch, roll), w random
void unit_rows(uint32_t a) {
    const float y = rf(-3.1f, 3.1f), p = rf(-1.5f, 1.5f), r = rf(-3.1f, 3.1f);
    const float cy = std::cos(y), sy = std::sin(y), cp = std::cos(p), sp = std::sin(p), cr = std::cos(r), sr = std::sin(r);
    const float m[3][3] = {{cy * cp, sy * cp, -sp}, {cy * sp * sr - sy * cr, sy * sp * sr + cy * cr, cp * sr}, {cy * sp * cr + sy * sr, sy * sp * cr - cy * sr, cp * cr}};
    for (int i = 0; i < 3; ++i) { for (int j = 0; j < 3; ++j) wf(a + 16 * u(i) + 4 * u(j), m[i][j]); wf(a + 16 * u(i) + 12, rf(-1, 1)); }
}

// the camera record (one of the engine's, 0x167150..: their rows' w lanes are left out as VCROSS's), its block at T3, the level's
// camera table *(0x15ef50) at T2 (4 entries) with their parameters at T4, the current camera *(0x166e00) and the one the start
// copies *(0x166e04), the hero about the board's point: every field the camera reads, about the values a race gives them
uint32_t race_camera() {
    level();
    const uint32_t cams = OV(0x00167150);
    const uint32_t cam = cams + u(ri(0, 3)) * 0xa0, other = cams + u(ri(4, 7)) * 0xa0;
    const uint32_t blk = T3, blk2 = T3 + 0x200;
    VV(0x0013f3d0) = coin() ? Vec4{rf(226, 346), rf(387, 507), rf(55, 70), 1.0f} : near(VV(0x0013f3d0), 20.0f, 5.0f);  // (Rilgar's floor point: within 48)
    const Vec4 hero = VV(0x0013f3d0);
    for (uint32_t c : {cam, other}) {
        unit_rows(c);
        VV(c + 0x30) = near(hero, 12.0f, 4.0f);
        wfrand(c + 0x40, 4, -1, 1);
        wf(c + 0x50, rf(-3.1f, 3.1f)); wf(c + 0x54, rf(-1.5f, 1.5f)); wf(c + 0x58, rf(1, 12));
        const Vec4 from = coin() ? near(hero, 4.0f, 3.0f) : near(VV(c + 0x30), 3.0f, 2.0f);  // (the way from it: the ground in it or not)
        wf(c + 0x64, from.x); wf(c + 0x68, from.y); wf(c + 0x6c, coin() ? hero.z - rf(0.5f, 3.0f) : from.z);
        w16(c + 0x7e, R());
        w16(c + 0x84, u(ri(0, 3)));
    }
    w32(cam + 0x70, blk); w32(other + 0x70, blk2);
    for (uint32_t b : {blk, blk2}) {
        wfrand(b, 0x80, -2, 2);
        wfrand(b, 3, -1, 1);                                     // the position springs' speeds
        wf(b + 0x10, rf(0.005f, 0.2f)); wf(b + 0x14, rf(0.1f, 0.5f)); wf(b + 0x18, coin() ? 0.0f : rf(0.0f, 3.0f));
        w16(b + 0x1c, u(ri(0, 4) == 0 ? 2 : ri(0, 1)));         // the orbit flag
        VV(b + 0x80) = near(hero, 2.0f, 2.0f);                   // the board's point (its height about the tracks' floors)
        if (coin()) wf(b + 0x88, rf(55.0f, 85.0f));
        VV(b + 0xa0) = Vec4{rf(-0.2f, 0.2f), rf(-0.2f, 0.2f), 1.0f, 0.0f};
        unit_rows(b + 0xb0 - 0x20);                              // (the orbit's axis +0xb0 a unit row)
        wf(b + 0xcc, rf(0, 3)); wf(b + 0xe0, rf(2, 12)); wf(b + 0xf0, rf(0, 6));
        wfrand(b + 0x120, 3, -0.5f, 0.5f); wf(b + 0x134, rf(0.005f, 0.2f)); wf(b + 0x138, rf(0.1f, 0.5f)); wf(b + 0x13c, coin() ? 0.0f : rf(0, 2));
        wf(b + 0x140, ri(0, 3) == 0 ? rf(-1e-5f, 1e-5f) : rf(-1, 1));
        for (uint32_t k = 0; k < 0x30; k += 4) wf(b + 0x150 + k, rf(-3.1f, 3.1f));
        wf(b + 0x168, rf(0.001f, 0.1f)); wf(b + 0x16c, rf(0.001f, 0.1f)); wf(b + 0x170, rf(0.001f, 0.1f));
        wf(b + 0x174, rf(0.1f, 0.4f)); wf(b + 0x178, rf(0.1f, 0.4f)); wf(b + 0x17c, rf(0.1f, 0.4f));
        wf(b + 0x180, rf(0, 3)); wf(b + 0x184, rf(2, 12)); wf(b + 0x188, rf(0, 6)); wf(b + 0x18c, rf(0, 3)); wf(b + 0x190, rf(2, 12)); wf(b + 0x194, rf(0, 6));
        // (the distances on their targets, a step away, or far)
        if (ri(0, 3) == 0) wf(b + 0xe0, ee_read<float>(b + (coin() ? 0x184 : 0x190)) + (coin() ? rf(-0.02f, 0.02f) : rf(-0.05f, 0.05f)));
        if (ri(0, 3) == 0) wf(b + 0xf0, ee_read<float>(b + (coin() ? 0x188 : 0x194)) + (coin() ? rf(-0.06f, 0.06f) : rf(-0.15f, 0.15f)));
        if (ri(0, 2) == 0) wf(b + 0xcc, ee_read<float>(b + (coin() ? 0x180 : 0x18c)) + (coin() ? rf(-0.025f, 0.025f) : rf(-0.05f, 0.05f)));
        wfrand(b + 0x1b0, 3, -0.5f, 0.5f); wfrand(b + 0x1c0, 3, -0.1f, 0.1f);
        VV(b + 0x1d0) = near(hero, 2.0f, 2.0f);
    }
    w32(OV(0x0015ef50), T2);
    for (uint32_t e = 0; e < 4; ++e) {
        const uint32_t par = T4 + e * 0x60;
        w32(T2 + e * 0x20 + 0x1c, par);
        wfrand(par, 0x18, -1, 1);
        wf(par + 0x20, rf(0.005f, 0.2f)); wf(par + 0x24, rf(0.1f, 0.5f));
        wf(par + 0x28, rf(2, 12)); wf(par + 0x2c, rf(0, 6)); wf(par + 0x30, rf(0, 3)); wf(par + 0x34, rf(2, 12)); wf(par + 0x38, rf(0, 6)); wf(par + 0x3c, rf(0, 3));
        wf(par + 0x40, coin() ? rf(-1, -0.1f) : rf(0.005f, 0.2f)); wf(par + 0x44, coin() ? rf(-1, -0.1f) : rf(0.1f, 0.5f));
        wf(par + 0x48, rf(-0.5f, 0.5f));
    }
    // the camera on the board's point, or right above it (the orbit's start measures a zero length)
    if (ri(0, 6) == 0) {
        VV(cam + 0x30) = VV(blk + 0x80);
        if (coin()) wf(cam + 0x38, ee_read<float>(blk + 0x88) + rf(1, 5));
    }
    if (coin()) unit_rows(ee_read<uint32_t>(0x001413d0) + 0xc0);  // (the hero moby's rows: the board's facing)
    w32(OV(0x00166e00), coin() ? cam : other);
    w32(OV(0x00166e04), other);
    // the hero: the orbit's flags (+0x88d, +0x88e), the boost (+0x30e) and the speed (+0x2dc); the level number (the tracks' floors)
    w8(0x0013f350 + 0x88d, u(ri(0, 3) == 0 ? 1 : 0)); w8(0x0013f350 + 0x88e, u(ri(0, 2) == 0 ? 1 : 0));
    w16(0x0013f350 + 0x30e, u(ri(0, 1))); wf(0x0013f350 + 0x2dc, coin() ? rf(1.5f, 2.5f) : rf(0, 6));
    if (ri(0, 2) == 0) w32(0x0015ed84, coin() ? 5u : coin() ? 16u : 3u);
    return cam;
}

void race_camera_checks() {
    // for these checks only: the collision's contact vectors' w lanes (sphere_collide's, as batches 138 / 142), and the w lane of the
    // orbit's axis the orbit's start keeps (block +0xbc): the original's VCROSS leaves the stale vf3.w there, as in vcross_lanes()
    const size_t ign = g_ignored_words.size();
    for (uint32_t a = OV(0x00173e80); a < OV(0x00173fc0); a += 0x10) g_ignored_words.push_back(a + 0xc);
    g_ignored_words.push_back(T3 + 0xbc);
    g_ignored_words.push_back(T3 + 0x200 + 0xbc);
    using Fn = void (*)(uint32_t);
    struct C {
        const char* name;
        std::initializer_list<LvAddr> copies;
        Fn fn;
    };
    const C cs[] = {
        {"race_cam_init", {{5, 0x0032a6e0}, {16, 0x002f51e8}}, race_cam_init},
        {"race_cam_update", {{5, 0x0032a758}, {16, 0x002f5260}}, race_cam_update},
        {"race_cam_setup", {{5, 0x003296e0}, {16, 0x002f41e8}}, race_cam_setup},
        {"race_cam_basis", {{5, 0x00329540}, {16, 0x002f4048}}, race_cam_basis},
        {"race_cam_anchor", {{5, 0x00329358}, {16, 0x002f3e60}}, race_cam_anchor},
        {"race_cam_orbit", {{5, 0x003298d8}, {16, 0x002f43e0}}, race_cam_orbit},
        {"race_cam_follow", {{5, 0x00329ce8}, {16, 0x002f47f0}}, race_cam_follow},
    };
    for (const C& c : cs) {
        const Fn fn = c.fn;
        each_copy(c.name, c.copies, [&c, fn](uint32_t a) {
            fcheck(c.name, a, 200, Ret::None, [] { return Args{{race_camera()}, {}}; }, [fn](A a) { fn(a.i[0]); return uint64_t{0}; });
        });
    }
    each_copy("race_cam_params", {{5, 0x003294c0}, {16, 0x002f3fc8}}, [](uint32_t a) {
        fcheck("race_cam_params", a, 40, Ret::None, [] {
            race_camera();
            return Args{{}, {rf(2, 12), rf(0, 6), rf(0, 3), rf(2, 12), rf(0, 6), rf(0, 3), coin() ? rf(-1, 0) : rf(0, 0.2f), coin() ? rf(-1, 0) : rf(0, 0.5f)}};
        }, [](A a) { race_cam_params(a.f[0], a.f[1], a.f[2], a.f[3], a.f[4], a.f[5], a.f[6], a.f[7]); return uint64_t{0}; });
    });
    g_ignored_words.resize(ign);
}
}  // namespace

void batch_503() {
    if (g_only_batch != 503) return;
    g_batch = 503;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    // (the scratchpad words the pose function 0x245f18 spills the caller's $t8 / $t9 into: batch 120's)
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
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

    ship_checks();
    vif_checks();
    bg_glow_checks();
    props_checks();
    race_camera_checks();

    g_pre_call = nullptr;
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
