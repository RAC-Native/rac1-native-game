// Oracle batch 504 (stage 4 task F1, #149): class code several of the levels 5, 9, 11, 13, 16 and 17 carry a copy of, each
// family ported once as shared hand-written code (src/game/ovl/shared_*.cpp), against every level's own copy at that level's
// own address. The level copies stay transliterated in their units until each level is re-glued; here every copy is the
// original and the one shared port the port:
//  - shared_class1886.cpp: class 1886, a blob class 1885 throws: its update, the mark it leaves, its retirement (levels 9, 13);
//  - shared_path_ahead.cpp: the point a distance ahead along a path (levels 9, 13);
//  - shared_class1111.cpp: class 1111, the open sea, and its draw callback (levels 11, 16); "class1111_fix24" checks its 1.02
//    fork (1.02 Gameplay fix 24) when run with RAC1_GAMEPLAY102=24;
//  - shared_race_hud.cpp: the hoverboard races' trick meter (update and draw) and their bottom bar (levels 5, 16);
//  - shared_class224.cpp, shared_class295.cpp: the space levels' pickups (classes 224 / 228) and rocket (class 295)
//    (levels 13, 17);
//  - shared_ship_shots.cpp: the space combat's shots, classes 1009 (homing; and its target search) and 1017; class 1034,
//    the other build of the rocket (shared_class295.cpp) (levels 11, 13, 17);
//  - shared_class1319.cpp: Pokitaru's class 1319 / Drek's fleet's 1843: the trails' record and draw, the path pose, the hit
//    (levels 11, 17);
//  - shared_hud_shapes.cpp: the space combat HUD's line strips, turned quads, ring and screen point (levels 11, 13, 17);
//  - the launchers of the rocket 295 and the shots 1009 / 1017 (levels 11, 13, 17);
//  - shared_moby_helpers.cpp: a group's flags (13, 16, 17), a spawn at a place and an eased value (13, 17), a state test (11,
//    16); shared_class1952.cpp: the draw-queuing classes 1952 (16) and 1907, 1908, 1910, 1911, 1913, 1918 (17);
//@@HEADER
// Subjects: the level's own mobys of the class after the first gameplay frame (the port's, the view set up), or mobys of the
// class spawned for the check, perturbed; the helpers on arguments built from the level's state in the test area; the draw
// callbacks' packets into a buffer of the test area. No RAM dumps.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 504, N = 5, 9, 11, 13, 16 and 17; a family the level does not carry
// says so and is not run. B504_INFO=1 prints how many subjects each check had and how far the draws' packets grew.

#include "ovl_oracle.h"

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class1886.h>
#include <game/ovl/shared_path_ahead.h>
#include <game/enhance/advanced.h>
#include <game/ovl/shared_class1111.h>
#include <game/ovl/shared_race_hud.h>
#include <game/ovl/shared_class224.h>
#include <game/ovl/shared_class295.h>
#include <game/ovl/shared_ship_shots.h>
#include <game/ovl/shared_class1319.h>
#include <game/ovl/shared_hud_shapes.h>
#include <game/ovl/shared_moby_helpers.h>
#include <game/ovl/shared_class1952.h>
//@@INCLUDES
#include <runtime/kernel/disc.h>
#include <runtime/vu/vu0.h>

#include <cmath>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <unordered_set>
#include <vector>

namespace rac1::oracle {

namespace {
// The VU0 the original started from (the interpreter's), given to the port before it runs (as batches 128 / 137 / 506 / 507)
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

bool info() { return std::getenv("B504_INFO") != nullptr; }

// Arguments the original takes as whole 64-bit registers (a colour, a TEX0): the case's values, put in the interpreter's
// registers by the batch's pre-call (check_fn passes 32-bit words, sign-extended); the port reads them from here
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

// the level after its first gameplay frame (the port's) and its view set up, as batches 506 / 507
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
        render_view_setup();  // (the view's matrices: the draw callbacks project through them)
        g_snap.assign(ram, ram + 0x02000000);
        g_snap_level = g_oracle_level;
    } else {
        std::memcpy(ram, g_snap.data(), g_snap.size());
    }
}

// the level's live mobys whose update function is `fn` (the level's address)
std::vector<uint32_t> mobys_with(uint32_t fn) {
    std::vector<uint32_t> v;
    for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
    return v;
}

// the level's live mobys of class `cls`
std::vector<uint32_t> mobys_of(int32_t cls) {
    std::vector<uint32_t> v;
    for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<int16_t>(m + 0xa6) == cls) v.push_back(m);
    return v;
}

uint32_t pick(const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; }

Vec4 hero_pos() { return ee_ref<Vec4>(0x0013f3d0); }

// the hero at `r0`..`r1` from the point
void hero_around(const Vec4& at, float r0, float r1) {
    Vec4 q = at;
    const float r = rf(r0, r1), a = rf(-3.1f, 3.1f);
    q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-1, 3);
    ee_ref<Vec4>(0x0013f3d0) = q;
}

// the camera (0x166dc0) at `r0`..`r1` from the point
void camera_around(const Vec4& at, float r0, float r1) {
    Vec4 q = at;
    const float r = rf(r0, r1), a = rf(-3.1f, 3.1f);
    q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-2, 4);
    ee_ref<Vec4>(OV(0x00166dc0)) = q;
}

// the moby's place set to q (its w kept)
void put_at(uint32_t m, const Vec4& q) {
    Vec4 p = q;
    p.w = ee_ref<Vec4>(m + 0x10).w;
    ee_ref<Vec4>(m + 0x10) = p;
}

// a live moby of the level other than `not_m` (none: the first)
uint32_t any_moby(uint32_t not_m) {
    const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), n = (ee_read<uint32_t>(OV(0x0015ffe0)) - first) >> 8;
    for (int k = 0; k < 50; ++k) {
        const uint32_t m = first + u(ri(0, static_cast<int>(n) - 1)) * 0x100;
        if (m != not_m && ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x78) != 0) return m;
    }
    return first;
}

// a moby of class `cls` spawned by the game's moby_spawn about the point (0: the level's table has no such class), its
// update set to the level's `fn`
uint32_t spawn_at(int32_t cls, uint32_t fn, const Vec4& at, float spread) {
    const uint32_t m = moby_spawn(u(cls));
    if (m == 0) return 0;
    Vec4 q = at;
    q.x += rf(-spread, spread); q.y += rf(-spread, spread); q.z += rf(0, spread * 0.5f);
    put_at(m, q);
    w32(m + 0x74, fn);
    return m;
}

// An update (or a helper taking the moby) on the subjects (dealt again when the port's run throws on a pointer outside
// RAM: a random state that is not a game state, as batch 506's classcheck)
void classcheck(const char* name, uint32_t laddr, const std::function<std::vector<uint32_t>()>& subjects, void (*port)(uint32_t),
                const std::function<void(uint32_t)>& vary, int n) {
    if (!selected(name)) return;
    level();
    const std::vector<uint32_t> probe = subjects();
    if (probe.empty()) { std::printf("%-26s L%d:0x%08x  no subject on this level\n", name, g_oracle_level, laddr); return; }
    if (info()) std::printf("INFO %s: %zu subjects\n", name, probe.size());
    fcheck(name, laddr, n, Ret::None, [subjects, vary, port] {
        static std::vector<uint8_t> saved;
        uint32_t m = 0;
        for (int attempt = 0; attempt < 40; ++attempt) {
            level();
            const std::vector<uint32_t> ms = subjects();
            m = pick(ms);
            vary(m);
            uint8_t* ram = ee_get_ram_base();
            saved.assign(ram, ram + 0x02000000);
            bool ok = true;
            const uint32_t stack_top = ee_stack_ptr();
            try { port(m); } catch (const std::exception&) { ok = false; }
            ee_stack_ptr() = stack_top;
            std::memcpy(ram, saved.data(), saved.size());
            if (ok) break;
        }
        return Args{{m}, {}};
    }, [port](A a) { vu0_sync(); port(a.i[0]); return uint64_t{0}; });
}

// the frame's packet cursor (0x1611c0) on a buffer of the test area (after the first gameplay frame alone it is 0: the
// drawing would go to the null page, which these checks leave out, and compare nothing; batch 506's)
void packet_cursor() { w32(OV(0x001611c0), TEST_AREA + 0x8000 + u(ri(0, 0x40)) * 0x10); }

// B504_INFO=1: how far a draw moved the packet cursor (that the check compares something)
void draw_info(const char* name, uint32_t m, void (*port)(uint32_t)) {
    if (!info()) return;
    const uint32_t before = ee_read<uint32_t>(OV(0x001611c0));
    static std::vector<uint8_t> saved;
    uint8_t* ram = ee_get_ram_base();
    saved.assign(ram, ram + 0x02000000);
    const uint32_t sp0 = ee_stack_ptr();
    vu0_sync();
    try { port(m); } catch (const std::exception&) {}
    ee_stack_ptr() = sp0;
    std::printf("INFO %s: the list grew by 0x%x\n", name, ee_read<uint32_t>(OV(0x001611c0)) - before);
    std::memcpy(ram, saved.data(), saved.size());
}

// ---- class 1886 (shared_class1886.cpp)
void class1886_checks() {
    const uint32_t upd = copy_here({{9, 0x0030b3b0}, {13, 0x0030dd80}});
    if (upd == 0) {
        for (const char* n : {"class1886_update", "class1886_leave_mark", "class1886_retire"}) not_here(n);
        return;
    }
    // Blobs: the level's own, else three spawned about the hero; each given a thrower (a class-1885 moby of the level, else
    // another moby made one) whose list holds the blobs, a velocity, its timers
    auto subjects = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (!v.empty()) return v;
        std::vector<uint32_t> throwers = mobys_of(0x75d);
        uint32_t thrower = throwers.empty() ? 0u : pick(throwers);
        if (thrower == 0) {
            thrower = any_moby(0);
            w16(thrower + 0xa6, 0x75d);
        }
        const uint32_t tpv = ee_read<uint32_t>(thrower + 0x78);
        for (uint32_t k = 0; k < 20; ++k) w32(tpv + 0xb0 + 4 * k, 0);
        const Vec4 at = coin() ? hero_pos() : ee_ref<Vec4>(thrower + 0x10);
        for (int k = 0; k < 3; ++k) {
            const uint32_t m = spawn_at(1886, upd, at, coin() ? 1.0f : 3.0f);
            if (m == 0) break;
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const uint32_t slot = u(ri(0, 19));
            wf(pv, rf(-0.3f, 0.3f)); wf(pv + 4, rf(-0.3f, 0.3f)); wf(pv + 8, rf(-0.4f, 0.2f)); wf(pv + 0xc, 0.0f);
            w32(pv + 0x10, thrower);
            w32(pv + 0x14, 0);
            w16(pv + 0x18, u(ri(1, 90)));
            w8(pv + 0x1a, u(ri(0, 1)));
            w8(pv + 0x1b, 0);
            w8(pv + 0x1c, slot);
            w32(tpv + 0xb0 + slot * 4, m);
            wf(m + 0x2c, ee_read<float>(ee_read<uint32_t>(m + 0x24) + 0x24) * rf(0.2f, 1.0f));
            w8(m + 0x20, 0);
            v.push_back(m);
        }
        return v;
    };
    // the blob: its state, velocity (falling onto the floor below it, towards a neighbour, slow), timers, its scale, its
    // mark, where it is (near the floor, near another blob, off the level's bounds)
    auto vary = [](uint32_t m, int st) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t cls = ee_read<uint32_t>(m + 0x24);
        const float cs = ee_read<float>(cls + 0x24);
        w8(m + 0x20, u(st));
        {
            const int k = ri(0, 4);
            if (k == 0) { wf(pv, rf(-0.02f, 0.02f)); wf(pv + 4, rf(-0.02f, 0.02f)); wf(pv + 8, rf(-0.05f, 0.0f)); }
            else if (k == 1) { wf(pv, rf(-0.1f, 0.1f)); wf(pv + 4, rf(-0.1f, 0.1f)); wf(pv + 8, rf(-0.8f, -0.2f)); }
            else if (k == 2) { wfrand(pv, 3, -0.5f, 0.5f); }
        }
        if (ri(0, 2) == 0) w16(pv + 0x18, u(ri(0, 2)));
        if (coin()) w8(pv + 0x1b, u(ri(0, 2)));
        if (coin()) w8(pv + 0x1a, u(ri(0, 1)));
        {
            const int k = ri(0, 4);
            if (k == 0) wf(m + 0x2c, cs * rf(0.0f, 0.06f));
            else if (k == 1) wf(m + 0x2c, cs * rf(0.0f, 1.2f));
        }
        // its mark: none, a moby of the level (gone / going / growing / grown)
        if (st == 2 || coin()) {
            const int k = ri(0, 4);
            if (k == 0) w32(pv + 0x14, 0);
            else {
                const uint32_t mark = any_moby(m);
                w32(pv + 0x14, mark);
                const uint32_t mc = ee_read<uint32_t>(mark + 0x24);
                const float ms = mc != 0 ? ee_read<float>(mc + 0x24) : 1.0f;
                if (k == 1) w8(mark + 0x20, coin() ? 0xfeu : 0xfdu);
                else if (k == 2) wf(mark + 0x2c, ms - rf(0.0f, 0.02f));
                else wf(mark + 0x2c, ms * rf(0.0f, 1.0f));
            }
        }
        // where it is
        {
            const int k = ri(0, 9);
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            if (k <= 2) {  // (just above the floor the hero stands on)
                q = hero_pos();
                q.x += rf(-2, 2); q.y += rf(-2, 2); q.z += rf(0.05f, 0.6f);
            } else if (k == 3) {  // (beside another blob of the list)
                const uint32_t tpv = ee_read<uint32_t>(ee_read<uint32_t>(pv + 0x10) + 0x78);
                for (uint32_t s = 0; s < 20; ++s) {
                    const uint32_t o = ee_read<uint32_t>(tpv + 0xb0 + 4 * s);
                    if (o != 0 && o != m) { q = ee_ref<Vec4>(o + 0x10); q.x += rf(-1.5f, 1.5f); q.y += rf(-1.5f, 1.5f); break; }
                }
            } else if (k >= 5 && k <= 7) {  // (into another blob of the list, or into a moby of the level: what it meets is a moby)
                uint32_t o = 0;
                if (k == 5 && coin()) {
                    const uint32_t tpv = ee_read<uint32_t>(ee_read<uint32_t>(pv + 0x10) + 0x78);
                    for (uint32_t s = 0; s < 20; ++s) {
                        const uint32_t c = ee_read<uint32_t>(tpv + 0xb0 + 4 * s);
                        if (c != 0 && c != m) { o = c; break; }
                    }
                }
                Vec4 c{};
                if (o != 0) {
                    c = ee_ref<Vec4>(o + 0x10);
                    const float r = rf(0.05f, 0.5f), a = rf(-3.1f, 3.1f);
                    q = c;
                    q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(0.0f, 1.0f);
                } else {
                    // (a moby whose collision a sphere of 0.2 there meets: tried with the collision's port, its results kept as
                    // they were)
                    const std::vector<uint8_t> was(ee_get_ram_base() + OV(0x00173e40), ee_get_ram_base() + OV(0x00173fc0));
                    for (int t = 0; t < 60; ++t) {
                        const uint32_t c2 = any_moby(m);
                        c = ee_ref<Vec4>(c2 + 0x10);
                        const float r = rf(0.0f, 2.0f), a = rf(-3.1f, 3.1f);
                        q = c;
                        q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-0.5f, 2.0f);
                        if (sphere_collide_v(q, 0.2f, 4, m) != 0 && ee_read<uint32_t>(OV(0x00173e58)) != 0) {
                            c = ee_ref<Vec4>(OV(0x00173e60));
                            // (the moby met made a blob half the time: the spawned blobs have no collision of their own)
                            if (coin()) w16(ee_read<uint32_t>(OV(0x00173e58)) + 0xa6, 0x75e);
                            break;
                        }
                    }
                    std::memcpy(ee_get_ram_base() + OV(0x00173e40), was.data(), was.size());
                }
                const float s = rf(0.0f, 0.3f);
                wf(pv, (c.x - q.x) * s); wf(pv + 4, (c.y - q.y) * s); wf(pv + 8, (c.z - q.z) * s);
            } else if (k == 4) {  // (at the level's bounds)
                q.x = coin() ? rf(3.0f, 6.0f) : rf(1015.0f, 1020.0f);
                if (coin()) q.y = coin() ? rf(3.0f, 6.0f) : rf(1015.0f, 1020.0f);
                if (ri(0, 2) == 0) q.z = coin() ? rf(3.0f, 6.0f) : rf(498.0f, 502.0f);
            }
            put_at(m, q);
        }
        // a neighbour blob of the list where the push reaches it, now and then gone
        if (coin()) {
            const uint32_t tpv = ee_read<uint32_t>(ee_read<uint32_t>(pv + 0x10) + 0x78);
            for (uint32_t s = 0; s < 20; ++s) {
                const uint32_t o = ee_read<uint32_t>(tpv + 0xb0 + 4 * s);
                if (o == 0 || o == m) continue;
                const int k = ri(0, 5);
                if (k == 0) w8(o + 0x20, coin() ? 0xfeu : 0xfdu);
                else if (k <= 3) { Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-1.7f, 1.7f); q.y += rf(-1.7f, 1.7f); q.z += rf(-1, 1); put_at(o, q); }
            }
            if (ri(0, 3) == 0) w32(tpv + 0xb0 + 4 * u(ri(0, 19)), any_moby(m));  // (a moby of another class in a slot)
        }
        if (ri(0, 5) == 0) wf(0x0015ed60, rf(0.5f, 2.0f));
        seed_rand();
    };
    // (the blobs' place w (+0x1c): after a sphere hit the blob is put at the collision's point 0x173e70 plus a step, and
    // vec3_add keeps the point's w, a VU0 leftover in the original and 0 in the collision's port: the words the batch
    // leaves out of 0x173e40.. already; left out here for the blobs only)
    const size_t ign = g_ignored_words.size();
    level();
    for (uint32_t m : subjects()) g_ignored_words.push_back(m + 0x1c);
    for (int st = 0; st < 4; ++st)
        for (int pass = 0; pass < 2; ++pass)
            classcheck("class1886_update", upd, subjects, class1886_update, [st, vary](uint32_t m) { vary(m, st == 3 ? ri(3, 6) : st); }, st == 0 ? 60 : 30);
    g_ignored_words.resize(ign);

    // the mark: the blob's thrower (none now and then) and its group's state, the thrower's share (+0xb4) and its +0x93
    const uint32_t mark = copy_here({{9, 0x0030a778}, {13, 0x0030d148}});
    fcheck("class1886_leave_mark", mark, 80, Ret::Int32, [subjects] {
        level();
        const uint32_t m = pick(subjects());
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        uint32_t thrower = ee_read<uint32_t>(pv + 0x10);
        if (ri(0, 9) == 0) thrower = 0;
        if (thrower != 0) {
            const uint32_t tpv = ee_read<uint32_t>(thrower + 0x78);
            // (a group of the level's with a free member (one whose state byte is negative: dead) most of the time, else
            // the thrower's own or any)
            const int32_t groups = ee_read<int32_t>(OV(0x0015fff4));
            int32_t g = ee_read<int32_t>(tpv + 0x94);
            if (ri(0, 3) != 0 && groups >= 0) {
                for (int t = 0; t < 30; ++t) {
                    const int32_t c = ri(0, groups);
                    const uint32_t list = ee_read<uint32_t>(OV(0x001ab840) + u(c) * 4);
                    if (list == 0) continue;
                    g = c;
                    int n = 0;
                    while (n < 64 && (ee_read<uint16_t>(list + u(n) * 2) & 0x8000u) == 0) ++n;
                    const uint32_t id = ee_read<uint16_t>(list + u(ri(0, n)) * 2) & 0x7fffu;
                    w8(ee_read<uint32_t>(OV(0x0015ffd8)) + (id << 8) + 0x20, coin() ? 0xfeu : 0x80u);
                    break;
                }
            } else if (ri(0, 2) == 0) {
                g = ri(-1, 40);
            }
            w32(tpv + 0x94, u(g));
            if (coin()) w8(tpv + 0x93, u(ri(0, 1)));
            if (coin()) w8(tpv + 0x91, u(ri(0, 255)));
            w16(thrower + 0xb4, u(ri(-3, 12)));
        }
        const uint32_t pos = TEST_AREA + 0x100;
        Vec4 q = ee_ref<Vec4>(m + 0x10);
        q.x += rf(-1, 1); q.w = rf(-1, 1);
        ee_ref<Vec4>(pos) = q;
        seed_rand();
        return Args{{thrower, pos}, {}};
    }, [](A a) { vu0_sync(); return r32(class1886_leave_mark(a.i[0], a.i[1])); });

    const uint32_t ret = copy_here({{9, 0x0030b350}, {13, 0x0030dd20}});
    fcheck("class1886_retire", ret, 60, Ret::None, [subjects] {
        level();
        const uint32_t m = pick(subjects());
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const int k = ri(0, 5);
        const uint32_t thrower = ee_read<uint32_t>(pv + 0x10);
        if (k == 0) w32(pv + 0x10, 0);
        else if (k == 1 && thrower != 0) w8(thrower + 0x20, coin() ? 0xfeu : 0xfdu);
        else if (k == 2) w32(pv + 0x10, any_moby(m));
        w8(pv + 0x1c, u(ri(0, 19)));
        return Args{{m, pv}, {}};
    }, [](A a) { class1886_retire(a.i[0], a.i[1]); return uint64_t{0}; });
}

// ---- the point ahead along a path (shared_path_ahead.cpp)
void path_ahead_checks() {
    const uint32_t fn = copy_here({{9, 0x00294ae0}, {13, 0x00281e40}});
    if (fn == 0) { not_here("path_point_ahead"); return; }
    // a path of 2 .. 8 points about the moby in the test area; the index at either end or between; the direction; the distance
    // short of, about and past the next point
    fcheck("path_point_ahead", fn, 200, Ret::Int32, [] {
        level();
        const uint32_t m = any_moby(0);
        const uint32_t path = TEST_AREA + 0x1000, out = TEST_AREA + 0x3000;
        const int32_t n = ri(2, 8);
        w32(path, u(n));
        Vec4 q = ee_ref<Vec4>(m + 0x10);
        for (int32_t i = 0; i < n; ++i) {
            q.x += rf(-6, 6); q.y += rf(-6, 6); q.z += rf(-2, 2); q.w = rf(-1, 1);
            ee_ref<Vec4>(path + 0x10 + u(i) * 0x10) = q;
        }
        const int k = ri(0, 3);
        const int32_t idx = k == 0 ? 0 : k == 1 ? n - 1 : ri(0, n - 1);
        const int32_t dir = coin() ? 1 : -1;
        wfrand(out, 4, -1, 1);
        return Args{{m, path, out, u(idx), u(dir)}, {coin() ? rf(-2.0f, 3.0f) : rf(3.0f, 15.0f)}};
    }, [](A a) { return r32(u(path_point_ahead(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]), static_cast<int32_t>(a.i[4]), a.f[0]))); });
}


// ---- class 1111, the open sea (shared_class1111.cpp)
void class1111_checks() {
    const uint32_t upd = copy_here({{11, 0x0030b358}, {16, 0x002dee10}});
    const uint32_t draw = copy_here({{11, 0x0030a908}, {16, 0x002de3c0}});
    if (upd == 0) {
        for (const char* n : {"class1111_update", "class1111_draw", "class1111_fix24"}) not_here(n);
        return;
    }
    const uint32_t sea = copy_here({{11, 0x00161df8}, {16, 0x00161c50}});
    const uint32_t last_cam = copy_here({{11, 0x00162840}, {16, 0x00162240}});
    auto subjects = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (v.empty()) {
            const uint32_t m = spawn_at(1111, upd, hero_pos(), 2.0f);
            if (m != 0) v.push_back(m);
        }
        return v;
    };
    // the camera (0x166dc0) above or below the sea's height (+0x20) and the band's reach, moved since the last update by a
    // little or by a lot; the offsets about their wraps; the frame counter; the hero's state 0x1d, 0x16c780; the pvars
    auto vary = [sea, last_cam](uint32_t m, int st) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, u(st));
        const uint32_t cam = OV(0x00166dc0);
        if (ri(0, 2) != 0) {
            Vec4 c = ee_ref<Vec4>(cam);
            const float h = ee_read<float>(pv + 0x20);
            c.z = ri(0, 4) == 0 ? h - rf(0.0f, 5.0f) : h + rf(0.0f, 40.0f);
            ee_ref<Vec4>(cam) = c;
        }
        {
            Vec4 l = ee_ref<Vec4>(OV(0x00166dc0));
            const float r = coin() ? 0.5f : 30.0f;
            l.x += rf(-r, r); l.y += rf(-r, r); l.z += rf(-r, r); l.w = rf(-1, 1);
            ee_ref<Vec4>(last_cam) = l;
        }
        for (uint32_t k = 0; k < 4; ++k) {
            const int c = ri(0, 3);
            if (c == 0) wf(sea + 0x10 + 4 * k, coin() ? rf(0.9f, 1.0f) : rf(-1.0f, -0.9f));
            else if (c == 1) wf(sea + 0x10 + 4 * k, rf(-1.0f, 1.0f));
        }
        if (coin()) { wf(sea, rf(0.5f, 4.0f)); wf(sea + 4, rf(0.5f, 4.0f)); }
        if (ri(0, 3) == 0) wf(sea + 0x24, rf(-5.0f, 5.0f));
        w32(OV(0x0015f5cc), R() & 0xffff);
        if (ri(0, 4) == 0) w32(0x001413d4, 0x1d);
        if (ri(0, 4) == 0) w32(OV(0x0016c780), 1 + (R() & 0xff));
        if (ri(0, 3) == 0) wf(0x0015ed7c, rf(0.5f, 2.0f));
        if (ri(0, 3) == 0) wf(pv + 0x18, coin() ? rf(5.0f, 60.0f) : rf(0.5f, 3.0f));
        if (ri(0, 3) == 0) wf(pv + 0x14, ee_read<float>(OV(0x00166dc0) + 8) - rf(-10.0f, 40.0f));
        if (ri(0, 3) == 0) wf(pv + 0x24, rf(-20.0f, 40.0f));
        if (ri(0, 3) == 0) wf(pv + 0x2c, coin() ? rf(1.0f, 30.0f) : rf(30.0f, 200.0f));
        if (ri(0, 4) == 0) wf(pv + 0x28, rf(50.0f, 800.0f));
        if (coin()) { w32(pv, R()); w32(pv + 4, R()); w32(pv + 8, R()); w32(pv + 0xc, R()); w32(pv + 0x10, R()); }
    };
    for (int st = 0; st < 3; ++st)
        classcheck("class1111_update", upd, subjects, class1111_update, [st, vary](uint32_t m) { vary(m, st); seed_rand(); }, st == 0 ? 20 : 60);
    // the draw: after an update, the camera about the sea's height (the tiles fade out with it) and high (the curtain),
    // the camera turned (the view's matrix stays: the band follows the camera's matrix 0x166fd0)
    classcheck("class1111_draw", draw, subjects, class1111_draw, [vary](uint32_t m) {
        vary(m, 1);
        const uint32_t sp0 = ee_stack_ptr();
        try { class1111_update(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (coin()) {  // (the camera over the swell by part of the tiles' fade height)
            const float sw_h = ee_read<float>(ee_read<uint32_t>(m + 0x78) + 0x2c);
            wf(OV(0x00166dc0) + 8, ee_read<float>(OV(0x00166dc0) + 8) * 0.0f + rf(-0.2f, 1.2f) * sw_h);
        }
        if (ri(0, 3) == 0) wf(pv + 0x18, rf(0.5f, 40.0f));
        if (ri(0, 4) == 0) wf(pv + 0x18, coin() ? -rf(0.5f, 10.0f) : 0.0f);
        if (ri(0, 2) != 0) {  // (the curtain near enough to be seen whole or in part: its reach, its top under the camera)
            wf(pv + 0x28, rf(5.0f, 120.0f));
            wf(pv + 0x24, rf(-15.0f, 10.0f));
            wf(pv + 0x14, ee_read<float>(OV(0x00166dc0) + 8) - rf(20.0f, 60.0f));
            const int k = ri(0, 5);  // (the view open, or closed by the hero's state 0x1d or by 0x16c780)
            w32(0x001413d4, k == 4 ? 0x1du : 0u);
            w32(OV(0x0016c780), k == 5 ? 1u : 0u);
        }
        packet_cursor();
        draw_info("class1111_draw", m, class1111_draw);
        seed_rand();
    }, 100);

    // fix 24 (1.02 Gameplay): the port with the fix against the port without it plus the 1.02 build's stores by hand
    if (const char* only = std::getenv("ORACLE_ONLY"); only == nullptr || std::strcmp(only, "class1111_fix24") == 0) {
        if (std::getenv("RAC1_GAMEPLAY102") == nullptr) {
            std::printf("%-26s skipped (run with RAC1_GAMEPLAY102=24)\n", "class1111_fix24");
            ++g_skipped;
        } else {
            int bad = 0, n = 0, set = 0;
            static std::vector<uint8_t> start, with_fix;
            const std::unordered_set<uint32_t> ignored(g_ignored_words.begin(), g_ignored_words.end());
            for (int c = 0; c < 40; ++c) {
                level();
                const std::vector<uint32_t> ms = subjects();
                if (ms.empty()) break;
                const uint32_t m = pick(ms);
                vary(m, c % 4 == 0 ? 0 : 1);
                w32(OV(0x001612dc), 0);
                seed_rand();
                uint8_t* ram = ee_get_ram_base();
                start.assign(ram, ram + 0x02000000);
                g_advanced_disabled = false;
                class1111_update(m);
                g_advanced_disabled = true;
                with_fix.assign(ram, ram + 0x02000000);
                std::memcpy(ram, start.data(), start.size());
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                const bool drawn = ee_read<uint8_t>(m + 0x20) != 0 && ee_read<float>(pv + 0x20) < ee_read<float>(OV(0x00166dc0) + 8);
                class1111_update(m);  // (the fix off: the North American code)
                if (drawn) {  // (the 1.02 build's stores, by hand: L11:0x30c728's after the swell)
                    w32(OV(0x001612dc), 1);
                    wf(OV(0x001612e0), 512.0f);
                    wf(OV(0x001612e4), 512.0f);
                    wf(OV(0x001612e8), ee_read<float>(sea + 0x20));
                    wf(OV(0x001612ec), 1024.0f);
                    ++set;
                }
                ++n;
                for (uint32_t a = 0x00100000; a < 0x02000000; a += 4) {
                    if (ignored.count(a) != 0) continue;
                    if (a >= ee_stack_ptr() - 0x10000 && a < ee_stack_ptr() + 0x10000) continue;  // (the port's stack frames)
                    uint32_t x, y;
                    std::memcpy(&x, &with_fix[a], 4); std::memcpy(&y, &ram[a], 4);
                    if (x != y) { if (bad < 8) std::printf("  class1111_fix24 case %d: %08x fix=%08x expected=%08x\n", c, a, x, y); ++bad; break; }
                }
            }
            std::printf("%-26s %s (%d cases, %d set the water disc)\n", "class1111_fix24", bad == 0 && n > 0 && set > 0 ? "ok" : "FAIL", n, set);
            if (bad != 0 || n == 0 || set == 0) ++g_fail;
        }
    }
}


// ---- the hoverboard races' HUD (shared_race_hud.cpp)
void race_hud_checks() {
    const uint32_t upd = copy_here({{5, 0x00262b58}, {16, 0x00235268}});
    const uint32_t meter = copy_here({{5, 0x00262f50}, {16, 0x00235660}});
    const uint32_t bar = copy_here({{5, 0x00266320}, {16, 0x00238a30}});
    if (upd == 0) {
        for (const char* n : {"race_meter_update", "race_meter_draw", "race_bar_draw"}) not_here(n);
        return;
    }
    const uint32_t P = 0x0013f350;
    const uint32_t race = 0x0015fad4;  // (the race's trick state: the same address on both levels, no relocation on NTSC-U)
    // the element (in the test area): the meter's fill and full value, the boost it follows (the hero's +0x864), its place,
    // its state bytes; the hero's trick state (in the air or landed, the combo state 0x6b), his boost, the race's words
    auto element = [P, race](int st) {
        level();
        const uint32_t e = TEST_AREA + 0x400;
        for (uint32_t k = 0; k < 0x80; k += 4) w32(e + k, 0);
        w32(e + 8, coin() ? u(ri(1, 2000)) : 1020u);
        w32(e + 0xc, P + 0x864);
        w32(e + 0x50, u(ri(0, 400)));
        w32(e + 0x54, u(ri(0, 300)));
        w32(e + 0x6c, ri(0, 4) == 0 ? u(ri(-2, 0)) : u(ri(1, 50)));
        w8(e + 0x70, ri(0, 3) == 0 ? u(ri(0, 3)) : u(ri(0, 200)));
        w8(e + 0x71, u(ri(0, 60)));
        w8(e + 0x72, u(st));
        w32(e + 0x74, u(ri(0, 2000)));
        w32(P + 0x864, coin() ? u(ri(0, 1100)) : u(ri(-50, 3000)));
        w32(P + 0x8a8, u(ri(0, 500)));
        w16(P + 0x30e, u(ri(0, 10)));
        w32(P + 0x70c, ri(0, 3) == 0 ? 0u : u(ri(1, 6)));
        w32(P + 0x6f0, u(ri(0, 5)));
        for (uint32_t k = 0; k < 4; ++k) w32(P + 0x6e0 + 4 * k, coin() ? 0u : u(ri(1, 9)));
        for (uint32_t k = 0; k < 3; ++k) w32(P + 0x700 + 4 * k, R());
        w32(P + 0x300, coin() ? 0u : 1u);
        { const int k = ri(0, 5); w32(P + 0x2084, k == 0 ? 0x6bu : k == 1 ? 0x6du : k == 2 ? 0x6fu : k == 3 ? 0x70u : u(ri(0, 0x80))); }
        w32(P + 0x208c, ri(0, 4) == 0 ? u(ri(0, 0x20)) : 0x16u);
        w8(P + 0x8ae, u(ri(0, 255)));
        w16(P + 0x89a, u(ri(-1, 4)));
        w32(P + 0x8c0, u(ri(0, 6)));
        w8(P + 0x8ce, u(ri(0, 1) * ri(1, 3)));
        w8(0x0013d388, u(ri(0, 1)));
        w32(0x0015ed80, u(ri(0, 1)));
        if (ri(0, 3) == 0) w32(0x0015ed84, coin() ? 0x10u : 5u);
        // the race's words: the score, the count, the four tricks' flags, the combos' history, the points
        w32(race, u(ri(0, 6)));
        w32(race + 4, u(ri(0, 5)));
        for (uint32_t k = 0; k < 4; ++k) w32(race + 0xc + 4 * k, coin() ? 0u : u(ri(1, 3)));
        for (uint32_t k = 0; k < 4; ++k) w32(race + 0x1c + 4 * k, coin() ? u(ri(0, 14)) : 0xffffffffu);
        w32(race + 0x2c, u(ri(0, 300)));
        w32(OV(0x0015f5cc), R() & 0xffff);
        packet_cursor();
        seed_rand();
        return e;
    };
    if (info() && selected("race_meter_draw")) {  // (the trick names' texts: their addresses and first bytes)
        level();
        for (int32_t id : {0x517e, 0x517f, 0x5180, 0x5181, 0x5092, 0x5093, 0x531a, 0x523f}) {
            const uint32_t s = sub_00203ad8(TXT(id));
            std::printf("INFO text 0x%x: 0x%08x \"%.20s\"\n", id, s, s != 0 ? reinterpret_cast<const char*>(ee_ram_ptr(s)) : "");
        }
    }
    for (int st = 0; st < 5; ++st)
        fcheck("race_meter_update", upd, 60, Ret::None, [element, st, P] {
            const uint32_t e = element(st);
            if (st >= 2 && ri(0, 2) == 0) w8(e + 0x70, u(frame_scale_i(0x6e)));  // (the combo's points paid)
            if (st >= 2 && ri(0, 3) == 0) w8(e + 0x70, 0);
            if (st == 1) {  // (in the air; landed on the board, a combo or not; landed in the states 0x6d..0x6f)
                const int k = ri(0, 3);
                if (k == 0) { w32(P + 0x300, 0); w32(P + 0x2084, 0x6bu); }
                else if (k == 1) { w32(P + 0x300, 1); w32(P + 0x2084, coin() ? 0x6bu : 0x70u); }
                else if (k == 2) { w32(P + 0x300, 0); w32(P + 0x2084, u(ri(0x6d, 0x6f))); }
                if (coin()) w32(P + 0x6f0, u(ri(3, 5)));
                for (uint32_t k2 = 0; k2 < 4; ++k2) if (coin()) w32(0x0015faf0 + 4 * k2, u(ri(0, 14)));
                if (ri(0, 3) == 0) w32(0x0015fad4, ee_read<uint32_t>(P + 0x70c));
            }
            return Args{{e}, {}};
        }, [](A a) { race_meter_update(a.i[0]); return uint64_t{0}; });
    for (int st = 0; st < 5; ++st)
        fcheck("race_meter_draw", meter, 60, Ret::None, [element, st, P] {
            const uint32_t e = element(st);
            if (st == 3) {  // (the combos: three tricks with each pattern of flags, or four; one, two or more tricks without)
                const int k = ri(0, 5);
                w32(0x0015fad8, k == 4 ? 4u : k == 0 ? u(ri(1, 2)) : k == 5 ? u(ri(2, 7)) : 3u);
                if (k == 0 || k == 5) w32(P + 0x208c, 0x16u);
            }
            if (coin()) w8(P + 0x8ae, u(frame_scale_i(0x78) + ri(1, 50)));
            if (info()) {
                const uint32_t before = ee_read<uint32_t>(OV(0x001611c0));
                static std::vector<uint8_t> saved;
                uint8_t* ram = ee_get_ram_base();
                saved.assign(ram, ram + 0x02000000);
                const uint32_t sp0 = ee_stack_ptr();
                try { race_meter_draw(e); } catch (const std::exception&) {}
                ee_stack_ptr() = sp0;
                std::printf("INFO race_meter_draw: the list grew by 0x%x\n", ee_read<uint32_t>(OV(0x001611c0)) - before);
                std::memcpy(ram, saved.data(), saved.size());
            }
            return Args{{e}, {}};
        }, [](A a) { race_meter_draw(a.i[0]); return uint64_t{0}; });
    fcheck("race_bar_draw", bar, 120, Ret::None, [element] {
        const uint32_t e = element(0);
        return Args{{e}, {}};
    }, [](A a) { race_bar_draw(a.i[0]); return uint64_t{0}; });
}


// A moby whose collision a sphere of `r` about a point near `near` meets (the collision's port tried, its results kept as
// they were); the point in `at`; 0 when none found
uint32_t moby_met_near(const Vec4& near, float spread, uint32_t ignore, float r, Vec4& at) {
    const std::vector<uint8_t> was(ee_get_ram_base() + OV(0x00173e40), ee_get_ram_base() + OV(0x00173fc0));
    uint32_t met = 0;
    for (int t = 0; t < 60 && met == 0; ++t) {
        const uint32_t c = t < 30 ? any_moby(ignore) : 0;
        Vec4 q = c != 0 ? ee_ref<Vec4>(c + 0x10) : near;
        q.x += rf(-spread, spread); q.y += rf(-spread, spread); q.z += rf(-1.0f, 2.0f);
        if (sphere_collide_v(q, r, 1, ignore) != 0) {
            met = ee_read<uint32_t>(OV(0x00173e58));
            at = q;
        }
    }
    std::memcpy(ee_get_ram_base() + OV(0x00173e40), was.data(), was.size());
    return met;
}

// ---- classes 224 / 228, the space levels' pickups (shared_class224.cpp)
void class224_checks() {
    const uint32_t upd = copy_here({{13, 0x002e1cc8}, {17, 0x002c53d8}});
    if (upd == 0) { not_here("class224_update"); return; }
    const uint32_t P = 0x0013f350;
    auto subjects = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (v.empty()) {
            for (int cls : {224, 228}) {
                const uint32_t m = spawn_at(cls, upd, hero_pos(), 4.0f);
                if (m != 0) v.push_back(m);
            }
        }
        return v;
    };
    // the pickup's state and pvars (the moby it waits for and that moby's state, the frames, its flags); the hero's ship
    // (+0x15f0: a moby made a ship, near or far, alive or not; the hero flying it or not), its ammo / shield and their most;
    // a ship's collision where the pickup is
    auto vary = [P](uint32_t m, int st) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, u(st));
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), n = (ee_read<uint32_t>(OV(0x0015ffe0)) - first) >> 8;
        const uint32_t idx = u(ri(0, static_cast<int>(n) - 1));
        w32(pv, ri(0, 3) == 0 ? 0xffffffffu : idx);
        { const int k = ri(0, 3); w32(pv + 4, k == 0 ? 0xffffffffu : k == 1 ? ee_read<uint8_t>(first + (idx << 8) + 0x20) : u(ri(0, 8))); }
        if (ri(0, 4) == 0) w8(first + (idx << 8) + 0x20, coin() ? 0xfeu : 0xfdu);
        w32(pv + 8, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 30)));
        w32(pv + 0x10, u(ri(0, 3)));
        w8(pv + 0xc, u(ri(0, 1)));
        w8(pv + 0xd, u(ri(0, 1)));
        if (coin()) w8(m + 0x31, u(ri(0, 1)));
        if (coin()) wf(m + 0x2c, ee_read<float>(ee_read<uint32_t>(m + 0x24) + 0x24) * rf(0.0f, 0.1f));
        wf(m + 0x48, rf(-3.1f, 3.1f));
        // the ship
        const uint32_t ship = any_moby(m);
        w32(P + 0x15f0, ri(0, 5) == 0 ? 0u : ship);
        { const int k = ri(0, 3); w16(ship + 0xa6, k == 0 ? 0x4dau : k == 1 ? 0x45u : k == 2 ? 0x563u : u(ri(0, 0x600))); }
        if (ri(0, 5) == 0) w8(ship + 0x20, coin() ? 0xfeu : 0xfdu);
        w32(P + 0x2084, ri(0, 3) == 0 ? u(ri(0, 0x40)) : 0x32u);
        {
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            const float r = ri(0, 2) == 0 ? rf(0.0f, 4.0f) : rf(4.0f, 45.0f), a = rf(-3.1f, 3.1f);
            q.x += r * std::cos(a); q.y += r * std::sin(a);
            put_at(ship, q);
        }
        w8(P + 0x15f6, u(ri(0, 255))); w8(P + 0x15f7, u(ri(0, 255)));
        wf(P + 0x15fc, rf(0.0f, 100.0f)); wf(P + 0x1600, rf(0.0f, 100.0f));
        if (ri(0, 3) == 0) {  // (the pickup where a sphere meets a moby, made a ship or not)
            Vec4 at{};
            const uint32_t met = moby_met_near(ee_ref<Vec4>(m + 0x10), 2.0f, m, 2.0f, at);
            if (met != 0) {
                put_at(m, at);
                if (ri(0, 2) != 0) { const int k = ri(0, 2); w16(met + 0xa6, k == 0 ? 0x4dau : k == 1 ? 0x45u : 0x563u); }
            }
        }
        if (ri(0, 4) == 0) {  // (at the level's bounds)
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            q.x = coin() ? rf(18.0f, 22.0f) : rf(1001.0f, 1005.0f);
            if (coin()) q.z = coin() ? rf(18.0f, 22.0f) : rf(1001.0f, 1005.0f);
            put_at(m, q);
        }
        w32(OV(0x0015f5cc), R() & 0xffff);
        seed_rand();
    };
    for (int st = 0; st < 8; ++st)
        for (int pass = 0; pass < 2; ++pass)
            classcheck("class224_update", upd, subjects, class224_update, [st, vary](uint32_t m) { vary(m, st); }, st == 4 ? 60 : 20);
}

// ---- class 295, a rocket of the space levels (shared_class295.cpp)
void class295_checks() {
    const uint32_t upd = copy_here({{13, 0x002e6c08}, {17, 0x002c9ce8}});
    if (upd == 0) { not_here("class295_update"); return; }
    auto subjects = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (v.empty()) {
            const uint32_t m = spawn_at(295, upd, hero_pos(), 4.0f);
            if (m != 0) v.push_back(m);
        }
        return v;
    };
    // the rocket: its speed, its yaw / pitch and their rates, its owner, its frames; its target (a moby of the class it
    // wants or not, hidden or not, near where it was last seen or not); where it flies (into the ground under the hero, at
    // the level's bounds, in the open)
    auto vary = [](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, u(ri(0, 3)));
        wf(pv + 0x2c, coin() ? rf(0.2f, 1.0f) : rf(1.0f, 4.0f));
        wf(m + 0x48, rf(-3.1f, 3.1f)); wf(m + 0x44, rf(-1.4f, 1.4f));
        wfrand(m + 0xc0, 12, -1.0f, 1.0f);  // (its matrix: a spawned moby has none until the moby loop runs; the sparks turn by it)
        wfrand(pv + 0x30, 2, -0.3f, 0.3f);
        wfrand(pv, 3, -1.0f, 1.0f);
        w32(pv + 0x20, coin() ? ee_read<uint32_t>(0x001413d0) : any_moby(m));
        w32(pv + 0x28, u(ri(0, 3) == 0 ? ri(0, 2) : ri(3, 200)));
        if (ri(0, 4) != 0) {
            const uint32_t t = any_moby(m);
            w32(pv + 0x24, t);
            w32(pv + 0x3c, coin() ? u(ee_read<int16_t>(t + 0xa6)) : u(ri(0, 0x600)));
            Vec4 q = ee_ref<Vec4>(t + 0x10);
            if (coin()) { q.x += rf(-2.0f, 2.0f); q.y += rf(-2.0f, 2.0f); } else { q.x += rf(-30.0f, 30.0f); }
            ee_ref<Vec4>(pv + 0x10) = q;
            if (ri(0, 4) == 0) w8(t + 0x20, coin() ? 0xfeu : 0xfdu);
            if (ri(0, 3) == 0) { w16(t + 0x34, u(ri(0, 1) | (coin() ? 0x1000 : 0))); w32(t + 0x94, coin() ? 0u : 1u); }
            if (coin()) { Vec4 r = ee_ref<Vec4>(t + 0x10); r.x += rf(-10.0f, 10.0f); r.y += rf(-10.0f, 10.0f); r.z += rf(-5.0f, 5.0f); put_at(m, r); }
        } else {
            w32(pv + 0x24, 0);
        }
        const int k = ri(0, 5);
        if (k <= 1) {  // (just over the ground the hero stands on, heading down)
            Vec4 q = hero_pos();
            q.x += rf(-3.0f, 3.0f); q.y += rf(-3.0f, 3.0f); q.z += rf(0.2f, 2.0f);
            put_at(m, q);
            wf(m + 0x44, rf(0.5f, 1.4f));
        } else if (k == 2) {  // (at the level's bounds)
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            const float c = coin() ? rf(0.0f, 5.0f) : rf(1018.0f, 1024.0f);
            { const int a = ri(0, 2); if (a == 0) q.x = c; else if (a == 1) q.y = c; else q.z = c; }
            put_at(m, q);
        }
        if (ri(0, 4) == 0) wf(0x0015ed6c, rf(0.01f, 0.05f));
        w32(OV(0x0015f5cc), R() & 0xffff);
        seed_rand();
    };
    for (int pass = 0; pass < 4; ++pass)
        classcheck("class295_update", upd, subjects, class295_update, vary, 40);
}


// the level's target list (0x15ffe4, linked by +0x28)
std::vector<uint32_t> target_list() {
    std::vector<uint32_t> v;
    for (uint32_t t = ee_read<uint32_t>(OV(0x0015ffe4)); t != 0 && v.size() < 200; t = ee_read<uint32_t>(t + 0x28)) v.push_back(t);
    return v;
}

// a moby of the target list made one the search takes: flagged 0x1000, its class of type 5
void qualify(uint32_t t) {
    w16(t + 0x34, ee_read<uint16_t>(t + 0x34) | 0x1000u);
    const uint32_t cls = ee_read<uint32_t>(t + 0x24);
    if (cls != 0) w16(cls + 0x46, 5);
}

// a shot's angles (+0x44 pitch, +0x48 yaw) turned at the point `at` from its place, off by up to `off`
void aim_at(uint32_t m, const Vec4& at, float off) {
    const Vec4 p = ee_ref<Vec4>(m + 0x10);
    const float dx = at.x - p.x, dy = at.y - p.y, dz = at.z - p.z;
    wf(m + 0x48, atan2_f(dx, dy) + rf(-off, off));  // (the game's own atan2, as the shots use it)
    wf(m + 0x44, -atan2_f(std::sqrt(dx * dx + dy * dy), dz) + rf(-off, off));
}

// ---- the space combat's shots, classes 1009 and 1017, and the rockets 1034 (shared_ship_shots.cpp, shared_class295.cpp)
void ship_shot_checks() {
    const uint32_t c1009 = copy_here({{11, 0x003089d0}, {13, 0x00302780}, {17, 0x002e22b0}});
    if (c1009 == 0) {
        for (const char* n : {"class1009_update", "class1009_target", "class1017_update", "class1034_update"}) not_here(n);
        return;
    }
    const uint32_t c1017 = copy_here({{11, 0x00309098}, {13, 0x00302e48}, {17, 0x002e2978}});
    const uint32_t c1034 = copy_here({{11, 0x003094f8}, {13, 0x00303128}, {17, 0x002e30c8}});
    const uint32_t ctarget = copy_here({{11, 0x00308670}, {13, 0x00302420}, {17, 0x002e1f50}});
    auto spawned = [](int cls, uint32_t fn) {
        return [cls, fn] {
            std::vector<uint32_t> v = mobys_with(fn);
            if (v.empty()) {
                const uint32_t m = spawn_at(cls, fn, hero_pos(), 4.0f);
                if (m != 0) v.push_back(m);
            }
            return v;
        };
    };
    // a shot: its velocity (into the ground under the hero, at a target, in the open), glow, timer, owner; for a homing
    // shot a target of the list (near where it was last seen or not, gone or not) and its frame on the 6-frame cycle
    auto vary_shot = [](uint32_t m, bool homing) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, u(ri(0, 2)));
        wf(pv + 0x10, ri(0, 2) == 0 ? rf(0.02f, 0.045f) : rf(0.0f, 1.0f));
        w32(pv + 0x18, u(ri(0, 2) == 0 ? 1 : ri(0, 100)));
        w32(pv + 0x1c, coin() ? ee_read<uint32_t>(0x001413d0) : any_moby(m));
        wf(pv + 0x20, rf(0.5f, 3.0f));
        wf(m + 0x48, rf(-3.1f, 3.1f)); wf(m + 0x44, rf(-1.4f, 1.4f));
        const float speed = coin() ? rf(0.3f, 1.5f) : rf(1.5f, 4.0f);
        const bool hold = homing && coin();  // (a homing shot flying at the target it holds, within its cones)
        const int k = hold ? 2 : ri(0, 4);
        Vec4 q = ee_ref<Vec4>(m + 0x10);
        const std::vector<uint32_t> targets = target_list();
        const uint32_t chosen = targets.empty() ? 0u : pick(targets);  // (the target flown at and homed on)
        if (chosen != 0 && ri(0, 3) != 0) qualify(chosen);
        if (k <= 1) {  // (just over the ground the hero stands on, heading down)
            q = hero_pos(); q.x += rf(-3.0f, 3.0f); q.y += rf(-3.0f, 3.0f); q.z += rf(0.3f, 2.5f);
            put_at(m, q);
            wf(pv, rf(-0.3f, 0.3f) * speed); wf(pv + 4, rf(-0.3f, 0.3f) * speed); wf(pv + 8, -speed);
        } else if (k == 2 && chosen != 0) {  // (behind a target of the list, flying at it)
            const Vec4 tp = ee_ref<Vec4>(chosen + 0x10);
            const float a = rf(-3.1f, 3.1f), r = rf(3.0f, 40.0f);
            q = tp; q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-3.0f, 5.0f);
            put_at(m, q);
            aim_at(m, tp, hold ? 0.15f : coin() ? 0.1f : 0.5f);
            const float yaw = ee_read<float>(m + 0x48), pitch = ee_read<float>(m + 0x44);
            wf(pv, speed * std::sin(yaw) * std::cos(pitch)); wf(pv + 4, speed * std::cos(yaw) * std::cos(pitch)); wf(pv + 8, -speed * std::sin(pitch));
        } else if (k == 4 && coin()) {  // (into a moby with collision, flagged 0x4000: its hit entry gets the shot's record)
            Vec4 at{};
            const uint32_t met = moby_met_near(hero_pos(), 6.0f, m, 0.5f, at);
            if (met != 0) {
                w16(met + 0x34, ee_read<uint16_t>(met + 0x34) | 0x4000u);
                const float a = rf(-3.1f, 3.1f);
                const Vec4 d{std::cos(a), std::sin(a), rf(-0.3f, 0.3f), 0.0f};
                put_at(m, Vec4{at.x - d.x * 1.5f, at.y - d.y * 1.5f, at.z - d.z * 1.5f, 1.0f});
                wf(pv, d.x * 2.0f); wf(pv + 4, d.y * 2.0f); wf(pv + 8, d.z * 2.0f);
            } else {
                wfrand(pv, 3, -speed, speed);
            }
        } else if (k == 3) {  // (at the level's bounds)
            const float c = coin() ? rf(0.0f, 5.0f) : rf(1018.0f, 1024.0f);
            { const int a = ri(0, 2); if (a == 0) q.x = c; else if (a == 1) q.y = c; else q.z = c; }
            put_at(m, q);
            wfrand(pv, 3, -speed, speed);
        } else {
            wfrand(pv, 3, -speed, speed);
        }
        if (homing) {
            const int j = hold ? 3 : ri(0, 4);
            if (j == 0 || chosen == 0) w32(pv + 0x2c, 0);
            else {
                const uint32_t t = chosen;
                w32(pv + 0x2c, t);
                Vec4 s = ee_ref<Vec4>(t + 0x10);
                if (j == 1) { s.x += rf(-5.0f, 5.0f); }
                wf(pv + 0x20, s.x); wf(pv + 0x24, s.y); wf(pv + 0x28, s.z);  // (where it was last seen; +0x2c is the target)
                if (j == 2) w8(t + 0x20, coin() ? 0xfeu : 0xfdu);
            }
            const uint32_t slot6 = u((static_cast<int32_t>(m - ee_read<uint32_t>(OV(0x0015ffd8))) >> 8) % 6);
            w32(OV(0x0015f5cc), hold ? slot6 + 1 + u(ri(0, 4)) + 6 * u(ri(0, 1000)) : ri(0, 2) == 0 ? slot6 : (R() & 0xffff));
        } else {
            w32(OV(0x0015f5cc), R() & 0xffff);
        }
        if (ri(0, 4) == 0) wf(0x0015ed6c, rf(0.01f, 0.05f));
        seed_rand();
    };
    for (int pass = 0; pass < 4; ++pass)
        classcheck("class1009_update", c1009, spawned(1009, c1009), class1009_update, [vary_shot](uint32_t m) {
            vary_shot(m, true);
            if (info()) {  // (whether the shot holds its target through the frame)
                static std::vector<uint8_t> saved;
                uint8_t* ram = ee_get_ram_base();
                saved.assign(ram, ram + 0x02000000);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78), before = ee_read<uint32_t>(pv + 0x2c);
                const uint32_t sp0 = ee_stack_ptr();
                const float yaw0 = ee_read<float>(m + 0x48);
                bool threw = false;
                try { class1009_update(m); } catch (const std::exception& e) { threw = true; std::printf("INFO threw %s\n", e.what()); }
                ee_stack_ptr() = sp0;
                std::printf("INFO class1009 target %08x -> %08x%s%s\n", before, ee_read<uint32_t>(pv + 0x2c), threw ? " threw" : "",
                            ee_read<float>(m + 0x48) != yaw0 ? " turned" : "");
                std::memcpy(ram, saved.data(), saved.size());
            }
        }, 40);
    for (int pass = 0; pass < 3; ++pass)
        classcheck("class1017_update", c1017, spawned(1017, c1017), class1017_update, [vary_shot](uint32_t m) { vary_shot(m, false); }, 40);
    // the rocket 1034: as class 295's
    for (int pass = 0; pass < 4; ++pass)
        classcheck("class1034_update", c1034, spawned(1034, c1034), class1034_update, [vary_shot](uint32_t m) {
            vary_shot(m, true);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            wfrand(m + 0xc0, 12, -1.0f, 1.0f);
            wf(pv + 0x2c, rf(0.2f, 3.0f));
            wfrand(pv + 0x30, 2, -0.3f, 0.3f);
            w32(pv + 0x28, u(ri(0, 3) == 0 ? ri(0, 2) : ri(3, 200)));
            w32(pv + 0x20, coin() ? ee_read<uint32_t>(0x001413d0) : any_moby(m));
            if (ri(0, 4) != 0) {
                const uint32_t t = any_moby(m);
                w32(pv + 0x24, t);
                Vec4 s = ee_ref<Vec4>(t + 0x10);
                if (coin()) s.x += rf(-6.0f, 6.0f);
                ee_ref<Vec4>(pv + 0x10) = s;
                if (ri(0, 4) == 0) w8(t + 0x20, coin() ? 0xfeu : 0xfdu);
            } else {
                w32(pv + 0x24, 0);
            }
            seed_rand();
        }, 40);
    // the target search from points about the list's targets, its flight turned at one or not, the cones and the range
    fcheck("class1009_target", ctarget, 200, Ret::Int32, [] {
        level();
        const std::vector<uint32_t> targets = target_list();
        const uint32_t pos = TEST_AREA + 0x100, rot = TEST_AREA + 0x140;
        const uint32_t near_t = targets.empty() ? 0u : pick(targets);
        if (near_t != 0) qualify(near_t);
        Vec4 q = near_t == 0 ? hero_pos() : ee_ref<Vec4>(near_t + 0x10);
        const Vec4 aim = q;
        const float a = rf(-3.1f, 3.1f), r = coin() ? rf(1.0f, 20.0f) : rf(20.0f, 80.0f);
        q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-5.0f, 5.0f); q.w = rf(-1, 1);
        ee_ref<Vec4>(pos) = q;
        wfrand(rot, 4, -3.1f, 3.1f);
        if (ri(0, 3) != 0) {
            const float dx = aim.x - q.x, dy = aim.y - q.y, dz = aim.z + 0.4f - q.z;
            wf(rot + 8, std::atan2(dx, dy) + rf(-0.3f, 0.3f));
            wf(rot + 4, std::atan2(std::sqrt(dx * dx + dy * dy), dz) + rf(-0.3f, 0.3f));
        }
        for (uint32_t t : targets) {
            const int k = ri(0, 3);
            if (k == 0) qualify(t);
            else if (k == 1) w16(t + 0x34, ee_read<uint16_t>(t + 0x34) ^ 0x1000u);
        }
        const float cone = coin() ? 0.2617994f : rf(0.05f, 1.5f);
        return Args{{pos, rot}, {cone, coin() ? cone : rf(0.05f, 1.5f), coin() ? 60.0f : rf(5.0f, 100.0f)}};
    }, [](A a) { return r32(class1009_target(a.i[0], a.i[1], a.f[0], a.f[1], a.f[2])); });
    if (info()) {
        level();
        int n = 0;
        for (uint32_t t : target_list()) n += (ee_read<uint16_t>(t + 0x34) & 0x1000u) != 0 && ee_read<uint32_t>(t + 0x24) != 0 && ee_read<int16_t>(ee_read<uint32_t>(t + 0x24) + 0x46) == 5;
        const uint32_t tr = copy_here({{11, 0x00161dc0}, {13, 0x00161d20}, {17, 0x00161db0}});
        std::printf("INFO target list: %zu mobys, %d targets; class 1017's trail %08x %08x %d %d %g\n", target_list().size(), n,
                    ee_read<uint32_t>(tr), ee_read<uint32_t>(tr + 4), ee_read<int32_t>(tr + 8), ee_read<int32_t>(tr + 0xc), ee_read<float>(tr + 0x10));
    }
}


// ---- class 1319 / 1843's trails, pose and hit (shared_class1319.cpp)
void class1319_checks() {
    const uint32_t upd = copy_here({{11, 0x00319838}, {17, 0x002f40d8}});
    if (upd == 0) {
        for (const char* n : {"class1319_trails_draw", "class1319_trails_record", "class1319_path_pose", "class1319_hit"}) not_here(n);
        return;
    }
    const int cls = g_oracle_level == 11 ? 1319 : 1843;
    const uint32_t draw = copy_here({{11, 0x003192c8}, {17, 0x002f3b68}});
    const uint32_t record = copy_here({{11, 0x00319670}, {17, 0x002f3f10}});
    const uint32_t pose = copy_here({{11, 0x0031a438}, {17, 0x002f4c90}});
    const uint32_t hit = copy_here({{11, 0x0031a1c0}, {17, 0x002f4a18}});
    const uint32_t length = copy_here({{11, 0x001623f0}, {17, 0x001623ec}});
    // two flyers (the level's or spawned ahead of the view's camera), each with its trails' 32 points along a line about the
    // view, its count and head, a word for the frame the draw was queued; a group of the two (a list in the test area put in
    // a group's slot) with the first's +0x21
    auto subjects = [upd, cls] {
        std::vector<uint32_t> v = mobys_with(upd);
        const Vec4 view = ee_ref<Vec4>(OV(0x0016ccf0));
        const float vw = view.w != 0.0f ? view.w : 1.0f;
        const Vec4 cam{view.x / vw, view.y / vw, view.z / vw, 1.0f};
        const Vec4 h = hero_pos();
        const Vec4 mid{(cam.x + h.x) * 0.5f, (cam.y + h.y) * 0.5f, (cam.z + h.z) * 0.5f, 1.0f};
        while (v.size() < 2) {
            const uint32_t m = spawn_at(cls, upd, mid, 3.0f);
            if (m == 0) break;
            v.push_back(m);
        }
        return v;
    };
    auto prepare = [length](const std::vector<uint32_t>& all, uint32_t m) {
        std::vector<uint32_t> ms{m};  // (the subject and up to two more of its class)
        for (uint32_t o : all) if (o != m && ms.size() < 3 && coin()) ms.push_back(o);
        const Vec4 view = ee_ref<Vec4>(OV(0x0016ccf0));
        const float vw = view.w != 0.0f ? view.w : 1.0f;
        const Vec4 cam{view.x / vw, view.y / vw, view.z / vw, 1.0f};
        const Vec4 h = hero_pos();
        for (uint32_t o : ms) {
            const uint32_t pv = ee_read<uint32_t>(o + 0x78);
            const float t0 = rf(0.2f, 0.8f);
            const Vec4 c{cam.x + (h.x - cam.x) * t0, cam.y + (h.y - cam.y) * t0, cam.z + (h.z - cam.z) * t0 + rf(-1.0f, 2.0f), 1.0f};
            const float a = rf(-3.1f, 3.1f);
            for (uint32_t side = 0; side < 2; ++side)
                for (uint32_t i = 0; i < 16; ++i) {
                    const float s = static_cast<float>(i) * rf(0.2f, 0.6f);
                    ee_ref<Vec4>(pv + 0xc0 + side * 0x100 + i * 0x10) =
                        Vec4{c.x + std::cos(a) * s + (side != 0 ? 0.7f : -0.7f), c.y + std::sin(a) * s, c.z + rf(-0.2f, 0.2f), coin() ? 1.0f : rf(-1, 1)};
                }
            w32(pv + 0x2c0, u(ri(0, 3) == 0 ? ri(-1, 3) : ri(3, 16)));
            w32(pv + 0x2c4, u(ri(0, 4) == 0 ? ri(-3, -1) : ri(0, 15)));
            w32(pv + 0x2c8, TEST_AREA + 0x700);
            w8(o + 0x31, ri(0, 4) == 0 ? 0u : 1u);
            wfrand(o + 0xc0, 12, -1.0f, 1.0f);
        }
        // the group: a list of the flyers (and another moby of the level now and then) put in group 1's slot
        const uint32_t list = TEST_AREA + 0x780, first = ee_read<uint32_t>(OV(0x0015ffd8));
        uint32_t n = 0;
        for (uint32_t o : ms) w16(list + 2 * n++, (o - first) >> 8);
        if (coin()) w16(list + 2 * n++, (any_moby(m) - first) >> 8);
        w16(list + 2 * (n - 1), ee_read<uint16_t>(list + 2 * (n - 1)) | 0x8000u);
        w32(OV(0x001ab840) + 4, list);
        w8(m + 0x21, ri(0, 5) == 0 ? 0xffu : 1u);
        if (ri(0, 4) == 0) w16(ms[0] + 0xa6, u(ri(0, 0x700)));
        wf(length, coin() ? 5.0f : 16.0f);
        w8(0x0015edb2, u(ri(0, 1)));
        w32(TEST_AREA + 0x700, u(ri(0, 3)));
        w32(OV(0x0015f5cc), u(ri(0, 3) == 0 ? ri(0, 3) : ri(0, 0xffff)));
        seed_rand();
    };
    classcheck("class1319_trails_draw", draw, subjects, class1319_trails_draw, [subjects, prepare](uint32_t m) {
        prepare(subjects(), m);
        packet_cursor();
        draw_info("class1319_trails_draw", m, class1319_trails_draw);
    }, 60);
    classcheck("class1319_trails_record", record, subjects, class1319_trails_record, [subjects, prepare](uint32_t m) {
        prepare(subjects(), m);
    }, 60);
    // the pose: a path of the level (a slot of the paths' table 0x1b04b0 with points), a point and the way along, turned round
    // or not, the swing, the speed
    classcheck("class1319_path_pose", pose, subjects, class1319_path_pose, [](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        std::vector<uint32_t> paths;
        for (uint32_t i = 0; i < 64; ++i) {
            const uint32_t p = ee_read<uint32_t>(OV(0x001b04b0) + i * 4);
            if (p != 0 && p < 0x02000000u && ee_read<int32_t>(p) > 0 && ee_read<int32_t>(p) < 500) paths.push_back(i);
        }
        if (!paths.empty()) {
            const uint32_t i = pick(paths), p = ee_read<uint32_t>(OV(0x001b04b0) + i * 4);
            w32(pv + 0x60, i);
            w32(pv + 0x64, u(ri(0, ee_read<int32_t>(p) - 1)));
        }
        wf(pv + 0x68, rf(0.0f, 1.0f));
        wf(pv + 0x6c, coin() ? rf(0.1f, 1.0f) : rf(-1.0f, 0.0f));
        wf(pv + 0x70, rf(-3.1f, 3.1f));
        wf(pv + 0x78, rf(0.0f, 5.0f));
        wf(pv + 0x80, rf(0.0f, 2.0f));
        wfrand(m + 0x40, 3, -3.1f, 3.1f);
        if (ri(0, 4) == 0) wf(0x0015ed6c, rf(0.01f, 0.05f));
        seed_rand();
    }, 60);
    // the hit: an entry of the hit table naming it (mask 0x330000 or not), by a moby of class 0xac or not; its state (5 or
    // the one the hit sends it to), its animation set; the hero flying or not; the level Pokitaru or not; the reward's count
    // and flag; the game's records about their limits; a sphere about it touching something or not
    classcheck("class1319_hit", hit, subjects, class1319_hit, [](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (ri(0, 3) == 0) w8(m + 0xa4, 0xff);
        else {
            const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
            w8(m + 0xa4, idx);
            w32(e + 0x34, ri(0, 8) == 0 ? ee_read<uint32_t>(OV(0x0015ffd8)) : m);
            w32(e + 0x24, ri(0, 3) == 0 ? (R() & ~0x330000u) : (0x330000u & R()) | 0x10000u);
            const uint32_t by = any_moby(m);
            w32(e + 0x20, ri(0, 4) == 0 ? 0u : by);
            if (coin()) w16(by + 0xa6, 0xac);
        }
        w8(m + 0x20, u(ri(0, 3) == 0 ? ri(0, 7) : coin() ? 5 : (coin() ? 2 : 6)));
        w8(m + 0x53, u(ri(0, 2)));
        w32(pv + 0x88, u(ri(0, 1)));
        w32(0x001413d4, coin() ? 0x32u : u(ri(0, 0x40)));
        if (coin()) w32(0x0015ed84, coin() ? 0xbu : 0x11u);
        w32(0x0015ee00, u(ri(0, 4)));
        w8(0x0013d41a, u(ri(0, 1)));
        w16(0x00157150, coin() ? u(ri(0xfffd, 0xffff)) : u(ri(0, 100)));
        w16(0x00157152, u(ri(0, 200)));
        w32(0x0015eea4, u(ri(0, 200000)));
        if (coin()) {
            Vec4 at{};
            const uint32_t met = moby_met_near(ee_ref<Vec4>(m + 0x10), 3.0f, m, 1.0f, at);
            if (met != 0) put_at(m, at);
        } else if (coin()) {
            Vec4 q = hero_pos(); q.z += rf(0.2f, 1.5f); put_at(m, q);
        }
        seed_rand();
    }, 80);
}


// ---- the space combat HUD's shapes (shared_hud_shapes.cpp): arguments built in the test area, the packet cursor on a
// test-area buffer
void hud_shape_checks() {
    const uint32_t strip = copy_here({{11, 0x00310ad0}, {13, 0x002b7b30}, {17, 0x002e9c28}});
    if (strip == 0) {
        for (const char* n : {"hud_line_strip", "hud_line_strip_turned", "hud_quad_turned", "hud_ring", "hud_screen_point"}) not_here(n);
        return;
    }
    const uint32_t turned = copy_here({{13, 0x002b7d50}, {17, 0x002e9e48}});
    const uint32_t quad = copy_here({{11, 0x00310cf8}, {13, 0x002b7fd0}, {17, 0x002ea0c8}});
    const uint32_t ring = copy_here({{11, 0x00311048}, {13, 0x002b8320}, {17, 0x002ea418}});
    const uint32_t point = copy_here({{11, 0x00311210}, {13, 0x002b84e8}, {17, 0x002ea5e0}});
    const uint32_t pts = TEST_AREA + 0x1000;
    auto points = [pts](int n) {
        for (int i = 0; i < n; ++i) { w16(pts + u(i) * 4, u(ri(-300, 300))); w16(pts + u(i) * 4 + 2, u(ri(-300, 300))); }
        w32(0x0013e510, u(ri(0x6c00, 0x7000))); w32(0x0013e514, u(ri(0x7400, 0x7a00)));
    };
    auto rgba64 = [] { return static_cast<uint64_t>(R()) | (coin() ? (static_cast<uint64_t>(R()) << 32) : 0ull); };
    fcheck("hud_line_strip", strip, 120, Ret::None, [points, pts, rgba64] {
        level();
        packet_cursor();
        const int n = ri(0, 4) == 0 ? ri(-1, 1) : ri(2, 24);
        points(n < 0 ? 0 : n);
        const uint64_t c = rgba64();
        g_wide_regs = {{7, c}};
        return Args{{pts, u(n), coin() ? 0xfffff0u : R(), static_cast<uint32_t>(c)},
                    {rf(-200.0f, 200.0f), rf(-200.0f, 200.0f), coin() ? rf(0.05f, 2.0f) : rf(-2.0f, 2.0f)}};
    }, [](A a) { hud_line_strip(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2], wide_arg(7), a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
    if (turned != 0) {
        fcheck("hud_line_strip_turned", turned, 120, Ret::None, [points, pts, rgba64] {
            level();
            packet_cursor();
            const int n = ri(0, 4) == 0 ? ri(-1, 1) : ri(2, 24);
            points(n < 0 ? 0 : n);
            const uint64_t c = rgba64();
            g_wide_regs = {{7, c}};
            return Args{{pts, u(n), coin() ? 0xfffff0u : R(), static_cast<uint32_t>(c)},
                        {rf(-200.0f, 200.0f), rf(-200.0f, 200.0f), rf(0.05f, 2.0f), rf(-3.2f, 3.2f)}};
        }, [](A a) { hud_line_strip_turned(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2], wide_arg(7), a.f[0], a.f[1], a.f[2], a.f[3]); return uint64_t{0}; });
    } else {
        not_here("hud_line_strip_turned");
    }
    fcheck("hud_quad_turned", quad, 120, Ret::None, [points] {
        level();
        packet_cursor();
        points(0);
        const uint64_t tex0 = (static_cast<uint64_t>(R()) << 32) | R();
        g_wide_regs = {{6, tex0}};
        return Args{{u(ri(0, 0xff)), u(ri(0, 0xff)), static_cast<uint32_t>(tex0), coin() ? 0xfffff3u : R(), R(), R(), R(), R()},
                    {rf(-200.0f, 200.0f), rf(-200.0f, 200.0f), rf(-50.0f, 50.0f), rf(-50.0f, 50.0f), rf(-3.2f, 3.2f)}};
    }, [](A a) {
        hud_quad_turned(a.i[0], a.i[1], wide_arg(6), a.i[3], a.i[4], a.i[5], a.i[6], a.i[7], a.f[0], a.f[1], a.f[2], a.f[3], a.f[4]);
        return uint64_t{0};
    });
    g_wide_regs.clear();
    fcheck("hud_ring", ring, 80, Ret::None, [points] {
        g_wide_regs.clear();
        level();
        packet_cursor();
        points(0);
        return Args{{R(), R(), R(), R()}, {rf(-200.0f, 200.0f), rf(-200.0f, 200.0f), rf(0.1f, 3.0f), rf(-3.2f, 3.2f)}};
    }, [](A a) { hud_ring(a.i[0], a.i[1], a.i[2], a.i[3], a.f[0], a.f[1], a.f[2], a.f[3]); return uint64_t{0}; });
    fcheck("hud_screen_point", point, 120, Ret::None, [] {
        g_wide_regs.clear();
        level();
        const uint32_t pos = TEST_AREA + 0x100, ox = TEST_AREA + 0x200, oy = TEST_AREA + 0x204;
        Vec4 q = coin() ? hero_pos() : ee_ref<Vec4>(any_moby(0) + 0x10);
        q.x += rf(-5.0f, 5.0f); q.y += rf(-5.0f, 5.0f); q.z += rf(-2.0f, 4.0f); q.w = 1.0f;
        ee_ref<Vec4>(pos) = q;
        w32(ox, R()); w32(oy, R());
        if (ri(0, 3) != 0) { w32(0x0013e510, u(ri(0x6c00, 0x7000))); w32(0x0013e514, u(ri(0x7400, 0x7a00))); }  // (the screen's offsets)
        return Args{{pos, ox, oy, u(ri(-40, 40))}, {}};
    }, [](A a) { hud_screen_point(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3])); return uint64_t{0}; });
    g_wide_regs.clear();
}


// ---- the launchers (shared_class295.cpp, shared_ship_shots.cpp): an owner of the level, a place, a direction / turn, a
// target, the speed (0 or less: the velocity's own), the range
void launcher_checks() {
    const uint32_t r295 = copy_here({{13, 0x002e6a58}, {17, 0x002c9b38}});
    const uint32_t s1009 = copy_here({{11, 0x00308848}, {13, 0x003025f8}, {17, 0x002e2128}});
    const uint32_t s1017 = copy_here({{11, 0x00308f48}, {13, 0x00302cf8}, {17, 0x002e2828}});
    auto place = [](uint32_t pos, uint32_t dir) {
        Vec4 q = hero_pos();
        q.x += rf(-5.0f, 5.0f); q.y += rf(-5.0f, 5.0f); q.z += rf(0.0f, 3.0f); q.w = rf(-1, 1);
        ee_ref<Vec4>(pos) = q;
        ee_ref<Vec4>(dir) = Vec4{rf(-2.0f, 2.0f), rf(-2.0f, 2.0f), rf(-1.0f, 1.0f), rf(-1, 1)};
    };
    auto owner = [] {
        const uint32_t o = any_moby(0);
        if (coin()) wf(o + 0x2c, rf(0.2f, 3.0f));
        return o;
    };
    if (r295 != 0) {
        fcheck("class295_launch", r295, 60, Ret::Int32, [place, owner] {
            level();
            const uint32_t pos = TEST_AREA + 0x100, rot = TEST_AREA + 0x110;
            place(pos, rot);
            const uint32_t o = owner(), target = ri(0, 3) == 0 ? 0u : any_moby(o);
            seed_rand();
            return Args{{o, pos, target, rot, u(ri(1, 300))}, {coin() ? rf(0.2f, 2.0f) : rf(2.0f, 6.0f)}};
        }, [](A a) { return r32(class295_launch(a.i[0], a.i[1], a.i[2], a.i[3], static_cast<int32_t>(a.i[4]), a.f[0])); });
    } else {
        not_here("class295_launch");
    }
    if (s1009 == 0) { not_here("class1009_launch"); not_here("class1017_launch"); return; }
    fcheck("class1009_launch", s1009, 60, Ret::Int32, [place, owner] {
        level();
        const uint32_t pos = TEST_AREA + 0x100, vel = TEST_AREA + 0x110;
        place(pos, vel);
        const uint32_t o = owner();
        seed_rand();
        return Args{{o, vel, pos}, {rf(5.0f, 200.0f), ri(0, 3) == 0 ? rf(-1.0f, 0.0f) : rf(0.2f, 4.0f)}};
    }, [](A a) { return r32(class1009_launch(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1])); });
    fcheck("class1017_launch", s1017, 60, Ret::Int32, [place, owner] {
        level();
        const uint32_t pos = TEST_AREA + 0x100, vel = TEST_AREA + 0x110;
        place(pos, vel);
        const uint32_t o = owner();
        seed_rand();
        return Args{{o, vel, pos}, {rf(5.0f, 200.0f), ri(0, 3) == 0 ? rf(-1.0f, 0.0f) : rf(0.2f, 4.0f), rf(0.2f, 3.0f)}};
    }, [](A a) { return r32(class1017_launch(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1], a.f[2])); });
}

// ---- the small moby helpers (shared_moby_helpers.cpp) and the draw-queuing classes (shared_class1952.cpp)
void moby_helper_checks() {
    if (const uint32_t fn = copy_here({{13, 0x002650a8}, {16, 0x00254a30}, {17, 0x002539c8}}); fn != 0) {
        // a group of the level (some members dead), each switch -1 / 0 / 1
        fcheck("group_set_flags", fn, 80, Ret::None, [] {
            level();
            const int32_t groups = ee_read<int32_t>(OV(0x0015fff4));
            int32_t g = ri(0, groups < 0 ? 0 : groups);
            for (int k = 0; k < 20; ++k) {
                if (ee_read<uint32_t>(OV(0x001ab840) + u(g) * 4) != 0) break;
                g = ri(0, groups < 0 ? 0 : groups);
            }
            const uint32_t list = ee_read<uint32_t>(OV(0x001ab840) + u(g) * 4);
            if (list != 0)
                for (uint32_t e = list; e < list + 64; e += 2) {
                    const uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                    if (ri(0, 3) == 0) w8(m + 0x20, coin() ? 0xfeu : 0x80u);
                    if (coin()) w16(m + 0x34, R());
                    if (ee_read<int16_t>(e) < 0) break;
                }
            return Args{{u(g), u(ri(-1, 1)), u(ri(-1, 1)), u(ri(-1, 1))}, {}};
        }, [](A a) { group_set_flags(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]), static_cast<int32_t>(a.i[3])); return uint64_t{0}; });
    } else {
        not_here("group_set_flags");
    }
    if (const uint32_t fn = copy_here({{13, 0x002e1c50}, {17, 0x002c5360}}); fn != 0) {
        fcheck("spawn_at_place", fn, 60, Ret::Int32, [] {
            level();
            const uint32_t pos = TEST_AREA + 0x100;
            Vec4 q = hero_pos(); q.x += rf(-5.0f, 5.0f); q.w = rf(-1, 1);
            ee_ref<Vec4>(pos) = q;
            // (a class of the level's: one of its mobys'; moby_spawn's port and the original part on a class the level lacks, so
            // none of those)
            const uint32_t cls = u(ee_read<int16_t>(any_moby(0) + 0xa6));
            return Args{{pos, cls}, {}};
        }, [](A a) { return r32(spawn_at_place(a.i[0], a.i[1])); });
    } else {
        not_here("spawn_at_place");
    }
    if (const uint32_t fn = copy_here({{13, 0x002ba088}, {17, 0x002ec270}}); fn != 0) {
        fcheck("ease_between", fn, 300, Ret::Float, [] {
            const float t0 = rf(-5.0f, 5.0f), t1 = t0 + (coin() ? rf(0.1f, 10.0f) : rf(-2.0f, 0.0f));
            const float t = ri(0, 4) == 0 ? t0 - rf(0.0f, 2.0f) : rf(t0 - 1.0f, t1 + 2.0f);
            return Args{{u(ri(0, 1)), u(ri(-1, 4))}, {t, rf(-3.1f, 3.1f), rf(-3.1f, 3.1f), t0, t1}};
        }, [](A a) { return static_cast<uint64_t>(f2u(ease_between(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]), a.f[0], a.f[1], a.f[2], a.f[3], a.f[4]))); });
    } else {
        not_here("ease_between");
    }
    {
        struct StateTest { int level; uint32_t addr; uint32_t state; };
        static const StateTest tests[] = {{11, 0x0030a830, 5}, {11, 0x0030e078, 5}, {16, 0x002c4710, 6}};
        bool any = false;
        for (const StateTest& s : tests) {
            if (s.level != g_oracle_level) continue;
            any = true;
            const uint32_t st = s.state;
            fcheck("moby_in_state", s.addr, 40, Ret::Int32, [st] {
                level();
                const uint32_t m = any_moby(0);
                w8(m + 0x20, coin() ? st : u(ri(0, 255)));
                return Args{{m}, {}};
            }, [st](A a) { return r32(moby_in_state(a.i[0], st) ? 1u : 0u); });
        }
        if (!any) not_here("moby_in_state");
    }
    {
        struct Queue { int level; uint32_t addr; int cls; void (*fn)(uint32_t); };
        static const Queue queues[] = {{16, 0x002e9df0, 1952, class1952_update}, {17, 0x002f57a0, 1907, class1907_update},
                                       {17, 0x002f5ab8, 1908, class1908_update}, {17, 0x002f5dd0, 1910, class1910_update},
                                       {17, 0x002f60e8, 1911, class1911_update}, {17, 0x002f6400, 1913, class1913_update},
                                       {17, 0x002f6718, 1918, class1918_update}};
        bool any = false;
        for (const Queue& q : queues) {
            if (q.level != g_oracle_level) continue;
            any = true;
            const uint32_t fn = q.addr;
            const int cls = q.cls;
            classcheck("class1952_update", fn, [fn, cls] {
                std::vector<uint32_t> v = mobys_with(fn);
                if (v.empty()) { const uint32_t m = spawn_at(cls, fn, hero_pos(), 3.0f); if (m != 0) v.push_back(m); }
                if (v.empty()) v.push_back(any_moby(0));  // (the class not in the level's table: another moby runs it)
                return v;
            }, q.fn, [](uint32_t m) { w8(m + 0x20, u(ri(0, 3))); }, 20);
        }
        if (!any) not_here("class1952_update");
    }
}

//@@CHECKS
}  // namespace

void batch_504() {
    if (g_only_batch != 504) return;
    g_batch = 504;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    // (the scratchpad words the pose function 0x245f18 spills the caller's $t8 / $t9 into: batch 120's)
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // 989snd's IOP server (as batches 120 / 507): replies a function of the command
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
    // the debris / spawns leave w lanes of a VU0 sin / cos in the original (the port has none); the particle pool's velocity
    // w lanes (a stale lane of vec_from_spherical in the original): batch 120's
    for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) {
        g_ignored_words.push_back(a + 0xc);
        g_ignored_words.push_back(a + 0x1c);
    }
    // the segment record 0x173e90..0x173ebc: segment_collide 0x1efc70 and the sphere test store VU0 registers there whose
    // lanes are another computation's leftovers, which the ports of the collision do not reproduce (batches 124 / 240 / 507)
    for (uint32_t k = 0; k < 0x30; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
    // and the collision results' w lanes (0x173e40 .. 0x173fc0, + 0xc of each row: the contact vectors' VU0 leftovers in the
    // original, 0 in the port of the collision; batches 140 / 223 / 506 / 507)
    for (uint32_t a = OV(0x00173e40); a < OV(0x00173fc0); a += 0x10) g_ignored_words.push_back(a + 0xc);
    {
        const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
        for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c);
    }
    // (the kernel's calls the original would make for real: the CD / movie path of a level change request; batch 120's)
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80};
    // the port's VU0 micro memory as the game leaves it, and the interpreter's VU0 kept for the port (as batch 137 / 506)
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

    class1886_checks();
    path_ahead_checks();
    class1111_checks();
    race_hud_checks();
    class224_checks();
    class295_checks();
    ship_shot_checks();
    class1319_checks();
    hud_shape_checks();
    launcher_checks();
    moby_helper_checks();
    //@@CALLS

    g_pre_call = nullptr;
    g_extra_noop_hooks.clear();
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
