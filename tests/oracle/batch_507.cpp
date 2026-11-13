// Oracle batch 507 (stage 4 task F4, #149): class code several of the levels 1, 3, 7, 14, 15 and 17 carry a copy of, each
// family ported once as shared hand-written code (src/game/ovl/shared_*.cpp), against every level's own copy at that level's
// own address. The level copies stay transliterated in their units until each level is re-glued; here every copy is the
// original and the one shared port the port:
//  - shared_class1224.cpp: class 1224, a bolt of lightning along a volume: its update, the new bolt, the strike sweeps, the
//    fade and the draw callback (levels 14, 15, 17);
//  - shared_class403.cpp: class 403, a rocket on a path and in free flight, and its trail (levels 14, 15);
//  - shared_class325.cpp: class 325, a burning shell (levels 14, 15), and its glow's draw (also Novalis's class 1510's; levels
//    1, 14, 15);
//  - shared_class250.cpp: class 250, animations in step with the camera of type 20 (levels 14, 15);
//  - shared_class1331.cpp: class 1331, the three effect slots' manager (levels 14, 15);
//  - shared_class28_draw.cpp: class 28's two draw callbacks, its five glows and its spot (levels 14, 15);
//  - shared_class77.cpp: class 77, a mine, and its group's glow (levels 15, 17);
//  - shared_class78_draw.cpp, shared_class123_draw.cpp: the draw callbacks of classes 78 and 123 (levels 15, 17);
//  - shared_three_glows.cpp: the three glows of classes 44 (levels 15, 17) and 1382 (level 17);
//  - shared_ripples.cpp: the rippling surfaces' update (classes 1408, 1409, 1565, 1567 on level 15; 1405, 1406, 1407, 1426 on
//    level 17) and two of their draws, the rings and the pool (levels 15, 17);
//  - shared_class196.cpp: classes 196 / 197, a lift (levels 15, 17);
//  - shared_class655_glows.cpp, shared_class408_tint.cpp: class 655's glows and class 408's tint (levels 15, 17);
//  - shared_class848.cpp: class 848, a helicopter's gunfire, and its bullet (levels 3, 7, 15);
//  - shared_level_helpers.cpp: a spark at a chain point (levels 7, 15, 17), the held moby shown (levels 1, 14);
//  - shared_class686.cpp: class 686, a shell that blows up at its target (levels 1, 14).
// Subjects: the level's own mobys of the class after the first gameplay frame (the port's, the view set up), perturbed; the
// draw callbacks' packets into a buffer of the test area. No RAM dumps.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 507, N = 1, 3, 7, 14, 15 and 17; a family the level does not carry
// says so and is not run.

#include "ovl_oracle.h"

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class1224.h>
#include <game/ovl/shared_class403.h>
#include <game/ovl/shared_class325.h>
#include <game/ovl/shared_class250.h>
#include <game/ovl/shared_class1331.h>
#include <game/ovl/shared_class28_draw.h>
#include <game/ovl/shared_class77.h>
#include <game/ovl/shared_class78_draw.h>
#include <game/ovl/shared_class123_draw.h>
#include <game/ovl/shared_three_glows.h>
#include <game/ovl/shared_ripples.h>
#include <game/ovl/shared_class196.h>
#include <game/ovl/shared_class655_glows.h>
#include <game/ovl/shared_class408_tint.h>
#include <game/ovl/shared_class848.h>
#include <game/ovl/shared_level_helpers.h>
#include <game/ovl/shared_class686.h>
#include <runtime/kernel/disc.h>
#include <runtime/vu/vu0.h>

#include <cmath>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <vector>

namespace rac1::oracle {

namespace {
// The VU0 the original started from (the interpreter's), given to the port before it runs (as batches 128 / 137 / 506)
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

uint32_t pick(const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; }

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

// An update (or a helper taking the moby) on the subjects `ms` (dealt again when the port's run throws on a pointer
// outside RAM: a random state that is not a game state, as batch 506's classcheck)
void classcheck(const char* name, uint32_t laddr, const std::function<std::vector<uint32_t>()>& subjects, void (*port)(uint32_t),
                const std::function<void(uint32_t)>& vary, int n) {
    if (!selected(name)) return;
    level();
    const std::vector<uint32_t> probe = subjects();
    if (probe.empty()) { std::printf("%-26s L%d:0x%08x  no subject on this level\n", name, g_oracle_level, laddr); return; }
    std::printf("  (%s: %zu subjects)\n", name, probe.size());
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

// B507_INFO=1: how far a draw moved the packet cursor (that the check compares something)
void draw_info(const char* name, uint32_t m, void (*port)(uint32_t)) {
    if (std::getenv("B507_INFO") == nullptr) return;
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

// the state a check starts its moby from: its update run once by the port (state 0 sets it up), then `st`
void after_first_update(uint32_t m, void (*update)(uint32_t), int st) {
    w8(m + 0x20, 0);
    const uint32_t sp0 = ee_stack_ptr();
    try { update(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
    w8(m + 0x20, u(st));
}

// the level's live mobys of the update `fn`, or (none placed) one moby of class `cls` spawned by the game's moby_spawn near
// the hero
std::vector<uint32_t> placed_or_spawned(uint32_t fn, uint32_t cls) {
    std::vector<uint32_t> v = mobys_with(fn);
    if (!v.empty()) return v;
    const uint32_t m = moby_spawn(cls);
    if (m == 0) return v;
    Vec4 q = ee_ref<Vec4>(0x0013f3d0);
    q.x += rf(-4, 4); q.y += rf(-4, 4); q.z += rf(0, 2);
    ee_ref<Vec4>(m + 0x10) = q;
    w32(m + 0x74, fn);
    v.push_back(m);
    return v;
}

// a hit on the moby: its slot in the hit table (0x178100, 64-byte entries) naming it, with a damage mask (the `kinds` bits
// mixed with random ones) and a damage at +0x2c (batch 506's)
void give_hit(uint32_t m, uint32_t kinds) {
    if (ri(0, 3) == 0) { w8(m + 0xa4, 0xff); return; }
    const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
    w8(m + 0xa4, idx);
    w32(e + 0x34, ri(0, 8) == 0 ? ee_read<uint32_t>(OV(0x0015ffd8)) : m);
    w32(e + 0x24, ri(0, 2) == 0 ? (R() & ~kinds) : (kinds | (R() & 0xff)));
    { static const float dm[] = {-1.0f, 0.0f, 0.5f, 5.0f, 12.0f}; wf(e + 0x2c, dm[ri(0, 4)]); }
    wfrand(e, 4, -1, 1);
    w32(e + 0x28, u(ri(0, 20)));
}

// A draw's subject placed where its quads' corners reach the packet list (on screen and not through the clipper, whose
// triangles are staged in the scratchpad, which the checks do not compare), most of the time: `place(q)` puts it at q; the
// candidates are the hero's place and points about the view's camera (0x16ccf0), each tried by running the port on a copy
// of RAM twice, at q and a little off it: the first whose packets differ between the two is kept
void place_drawn(void (*port)(uint32_t), uint32_t m, const std::function<void(const Vec4&)>& place) {
    if (ri(0, 4) == 0) return;  // (now and then where it is)
    static std::vector<uint8_t> saved;
    uint8_t* ram = ee_get_ram_base();
    saved.assign(ram, ram + 0x02000000);
    const Vec4 view = ee_ref<Vec4>(OV(0x0016ccf0));  // (the view's camera, scaled by its w)
    const float vw = view.w != 0.0f ? view.w : 1.0f;
    const Vec4 cam{view.x / vw, view.y / vw, view.z / vw, 1.0f};
    // the packets the port writes with the subject at q (a hash; 0: none)
    auto packets = [&](const Vec4& q) {
        place(q);
        const uint32_t before = ee_read<uint32_t>(OV(0x001611c0));
        const uint32_t sp0 = ee_stack_ptr();
        vu0_sync();  // (the VU0 the check gives the port)
        try { port(m); } catch (const std::exception&) {}
        ee_stack_ptr() = sp0;
        uint32_t h = 0;
        for (uint32_t a = before; a < ee_read<uint32_t>(OV(0x001611c0)) && a < before + 0x10000; a += 4) h = h * 31 + ee_read<uint32_t>(a) + 1;
        std::memcpy(ram, saved.data(), saved.size());
        return h;
    };
    // (the view looks about the hero: the candidates lie about the line from its camera through him, near and far)
    const Vec4 hero = ee_ref<Vec4>(0x0013f3d0);
    Vec4 ahead{hero.x - cam.x, hero.y - cam.y, hero.z - cam.z, 0.0f};
    {
        const float l = std::sqrt(ahead.x * ahead.x + ahead.y * ahead.y + ahead.z * ahead.z);
        if (l > 0.001f) { ahead.x /= l; ahead.y /= l; ahead.z /= l; } else ahead = Vec4{1.0f, 0.0f, 0.0f, 0.0f};
    }
    bool have_drawn = false;
    Vec4 drawn{};
    for (int k = 0; k < 120; ++k) {
        Vec4 q = hero;
        if (k != 0) {
            const float r = rf(3.0f, 40.0f), j = k < 60 ? 0.3f : 0.6f;
            Vec4 d{ahead.x + rf(-j, j), ahead.y + rf(-j, j), ahead.z + rf(-j, j), 0.0f};
            const float l = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
            q = Vec4{cam.x + r * d.x / l, cam.y + r * d.y / l, cam.z + r * d.z / l, 1.0f};
        }
        const uint32_t h0 = packets(q);
        if (h0 == 0) continue;
        if (!have_drawn) { have_drawn = true; drawn = q; }
        const uint32_t h1 = packets(Vec4{q.x + 0.05f, q.y + 0.05f, q.z + 0.05f, q.w});
        if (h1 == h0) continue;  // (drawn, but its corners clipped away: only the GS state reaches the packets)
        if (std::getenv("B507_INFO") != nullptr) std::printf("INFO placed after %d tries\n", k + 1);
        place(q);
        return;
    }
    if (have_drawn) {  // (else where it draws at all: the GS state compared)
        if (std::getenv("B507_INFO") != nullptr) std::printf("INFO placed where drawn only\n");
        place(drawn);
        return;
    }
    if (std::getenv("B507_INFO") != nullptr) {
        const Vec4 c1 = ee_ref<Vec4>(OV(0x00166dc0)), h = ee_ref<Vec4>(0x0013f3d0);
        std::printf("INFO not placed (view %.1f %.1f %.1f %.2f, camera %.1f %.1f %.1f, hero %.1f %.1f %.1f)\n", cam.x, cam.y, cam.z, cam.w, c1.x, c1.y, c1.z, h.x, h.y, h.z);
    }
}

// the camera point (0x166dc0, which the quads face) raised or lowered off the view's height, most of the time (a quad
// facing a camera at its own height has its frame's up axis nearly level)
void camera_lift() {
    if (ri(0, 3) == 0) return;
    wf(OV(0x00166dc0) + 8, ee_read<float>(OV(0x00166dc0) + 8) + (coin() ? rf(0.5f, 2.5f) : -rf(0.5f, 2.5f)));
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

// ---- class 1224 (shared_class1224.cpp)
void class1224_checks() {
    const uint32_t upd = copy_here({{14, 0x003015d0}, {15, 0x002e6038}, {17, 0x002e6468}});
    if (upd == 0) {
        for (const char* n : {"class1224_update", "class1224_bolt", "class1224_strike", "class1224_fade", "class1224_draw"}) not_here(n);
        return;
    }
    const uint32_t tune = copy_here({{14, 0x001620b8}, {15, 0x00161f44}, {17, 0x00161f24}});
    // (Quartu and Drek's fleet place none: one spawned, on one of the level's first volumes, with its intervals and reach)
    auto subjects = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (!v.empty()) return v;
        v = placed_or_spawned(upd, 1224);
        for (uint32_t m : v) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w32(pv, u(ri(0, 3)));
            w16(pv + 4, u(ri(20, 90)));
            w16(pv + 6, coin() ? 0u : u(ri(5, 30)));
            w32(pv + 0xc, u(ri(20, 60)));
            wf(pv + 0x1c, rf(0.5f, 2.0f));
        }
        return v;
    };
    auto volume = [](uint32_t m) { return ee_read<uint32_t>(OV(0x001600ec)) + (ee_read<uint32_t>(ee_read<uint32_t>(m + 0x78)) << 7); };
    // the level's switches (+0x14 off, +0x18 no fade, +0x1c not drawn, +0x20 no sweeps), its count of strikes (+0), the fade's
    // marks (+0xc / +0x10) about the bolt's frames
    auto vary_tuning = [tune] {
        for (uint32_t o : {0x14u, 0x18u, 0x1cu, 0x20u}) w32(tune + o, ri(0, 3) == 0 ? 1u : 0u);
        if (coin()) w32(tune, ri(0, 2) == 0 ? 0u : u(ri(1, 5)));
        if (ri(0, 2) == 0) { w32(tune + 0xc, u(ri(0, 40))); w32(tune + 0x10, u(ri(0, 20))); }
    };
    // the bolt's state: frames left (+0x12: none, one, some), the timer (+0x10: running, about to end, ended), the intervals
    // (+4 / +6: a quick strike or none), the ends to clip (+8 / +0xa), the reach (+0x1c)
    auto vary_bolt = [](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        { const int k = ri(0, 3); w16(pv + 0x12, k == 0 ? 0u : k == 1 ? 1u : u(ri(2, 60))); }
        { const int k = ri(0, 3); w16(pv + 0x10, k == 0 ? 0u : k == 1 ? 1u : u(ri(2, 60))); }
        if (coin()) w16(pv + 4, u(ri(1, 90)));
        if (coin()) w16(pv + 6, coin() ? 0u : u(ri(1, 30)));
        if (coin()) { w16(pv + 8, u(ri(0, 1))); w16(pv + 0xa, u(ri(0, 1))); }
        if (ri(0, 3) == 0) wf(pv + 0x1c, coin() ? 0.0f : rf(0.2f, 3.0f));
        if (coin()) w32(pv + 0x620, u(ri(1, 90)));
    };
    // the camera off the volume's centre (the sparks' and the cull's distance) and the hero near the moby or not (the sweeps)
    auto vary_places = [volume](uint32_t m) {
        // (the moby where its cull test (point_on_screen with its volume's length in w, radius 52) passes, most of the time:
        // else the update returns at once)
        if (ri(0, 4) != 0) {
            const Vec4 view = ee_ref<Vec4>(OV(0x0016ccf0));  // (the view's camera, scaled by its w)
            const float vw = view.w != 0.0f ? view.w : 1.0f;
            const Vec4 cam{view.x / vw, view.y / vw, view.z / vw, 1.0f};
            const float len = ee_read<float>(ee_read<uint32_t>(m + 0x78) + 0x624);
            for (int k = 0; k < 60; ++k) {
                const float r = rf(2.0f, 30.0f), a = rf(-3.1f, 3.1f), b = rf(-0.7f, 0.7f);
                const Vec4 q{cam.x + r * std::cos(a) * std::cos(b), cam.y + r * std::sin(a) * std::cos(b), cam.z + r * std::sin(b), len};
                if (point_on_screen_v(q, 52.0f) >= 0) { put_at(m, q); break; }
            }
        }
        const Vec4 c = ee_ref<Vec4>(volume(m) + 0x30);
        if (coin()) camera_around(c, 0.0f, ri(0, 2) == 0 ? 4.0f : 30.0f);
        hero_around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 10.0f : 30.0f);
    };

    for (int st = 0; st < 5; ++st)
        for (int pass = 0; pass < 3; ++pass)
            classcheck("class1224_update", upd, subjects, class1224_update, [st, pass, vary_tuning, vary_bolt, vary_places](uint32_t m) {
                if (st != 0) after_first_update(m, class1224_update, st);
                if (pass >= 1) vary_bolt(m);
                if (pass == 2) vary_tuning();
                vary_places(m);
                seed_rand();
            }, 12);

    const uint32_t bolt = copy_here({{14, 0x003017f8}, {15, 0x002e6260}, {17, 0x002e6690}});
    classcheck("class1224_bolt", bolt, subjects, class1224_bolt, [vary_places](uint32_t m) {
        after_first_update(m, class1224_update, 2);
        vary_places(m);
        seed_rand();
    }, 40);

    // the sweeps hit what they cross: the hero inside the volume's reach or not, the moby turned
    const uint32_t strike = copy_here({{14, 0x00302538}, {15, 0x002e6fa0}, {17, 0x002e73d0}});
    classcheck("class1224_strike", strike, subjects, class1224_strike, [vary_bolt, volume](uint32_t m) {
        after_first_update(m, class1224_update, 2);
        vary_bolt(m);
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (ri(0, 4) != 0) wf(pv + 0x1c, rf(0.2f, 4.0f));
        if (coin()) { w16(pv + 8, 1); w16(pv + 0xa, 1); }
        wf(m + 0x48, rf(-3.1f, 3.1f));
        const Vec4 c = ee_ref<Vec4>(volume(m) + 0x30);
        const int k = ri(0, 3);
        if (k == 0) ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(m + 0x10);
        else if (k == 1) hero_around(c, 0.0f, coin() ? 2.0f : 8.0f);
        else {  // (on one of the four sweeps: the bolt's line moved out by the reach along the volume's x or z axis)
            const uint32_t vol = volume(m);
            Vec4 axis = ee_ref<Vec4>(vol + (coin() ? 0x00u : 0x20u));
            const float al = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
            const float reach = ee_read<float>(pv + 0x1c) * (coin() ? 1.0f : -1.0f) / (al > 0.0f ? al : 1.0f);
            const Vec4 y = ee_ref<Vec4>(vol + 0x10);
            const float t = rf(-0.8f, 0.8f);
            const Vec4 q{c.x + axis.x * reach + y.x * t, c.y + axis.y * reach + y.y * t, c.z + axis.z * reach + y.z * t, 1.0f};
            ee_ref<Vec4>(0x0013f3d0) = q;
            const uint32_t hero = ee_read<uint32_t>(0x001413d0);
            if (hero != 0) put_at(hero, q);
        }
        seed_rand();
    }, 60);

    const uint32_t fade = copy_here({{14, 0x00301de8}, {15, 0x002e6850}, {17, 0x002e6c80}});
    classcheck("class1224_fade", fade, subjects, class1224_fade, [vary_bolt, vary_tuning](uint32_t m) {
        after_first_update(m, class1224_update, 2);
        vary_bolt(m);
        if (coin()) vary_tuning();
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        for (uint32_t k = 0; k < 4; ++k) if (ri(0, 4) == 0) w8(pv + 0x18 + k, u(ri(0, 31)));
        seed_rand();
    }, 60);

    // the draw: a bolt made, the camera near or off, a fork past the bolt's end now and then, the colour
    const uint32_t draw = copy_here({{14, 0x00301fa8}, {15, 0x002e6a10}, {17, 0x002e6e40}});
    // (the volume moved where its quads are drawn most of the time: place_drawn)
    classcheck("class1224_draw", draw, subjects, class1224_draw, [volume](uint32_t m) {
        after_first_update(m, class1224_update, 2);
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        for (uint32_t k = 0; k < 4; ++k) if (ri(0, 5) == 0) w8(pv + 0x18 + k, u(ri(29, 40)));
        if (coin()) w32(pv + 0x14, R());
        const Vec4 c = ee_ref<Vec4>(volume(m) + 0x30);
        if (ri(0, 4) == 0) camera_around(c, 1.0f, 20.0f);
        packet_cursor();
        const uint32_t vol = volume(m);
        place_drawn(class1224_draw, m, [vol](const Vec4& q) { Vec4 p = q; p.w = ee_ref<Vec4>(vol + 0x30).w; ee_ref<Vec4>(vol + 0x30) = p; });
        draw_info("class1224_draw", m, class1224_draw);
        seed_rand();
    }, 40);
}

// ---- class 403 (shared_class403.cpp)
void class403_checks() {
    const uint32_t upd = copy_here({{14, 0x002dfd10}, {15, 0x002ca9f8}});
    const uint32_t trail = copy_here({{14, 0x002e03b8}, {15, 0x002cb0a0}});
    if (upd == 0) {
        for (const char* n : {"class403_update", "class403_trail"}) not_here(n);
        return;
    }
    auto subjects = [upd] { return placed_or_spawned(upd, 403); };
    // the rocket: on a path of the level (its index, place, speed) or in free flight (its velocity rising or falling, its
    // spin), its fuse (idle, ending, running), its owner (another moby, now and then one at its nose), near the level's box
    // edges now and then, its heading
    auto vary = [](uint32_t m, int st) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, u(st));
        const int32_t rails = ee_read<int32_t>(OV(0x0015f710));
        w32(pv + 0xc, rails > 0 ? u(ri(0, rails - 1)) : 0u);
        w32(pv + 0x10, u(ri(0, 3)));
        wf(pv + 0x14, rf(0.0f, 0.9f));
        wf(pv + 8, rf(0.05f, 1.5f));
        { const int k = ri(0, 3); w32(pv + 4, k == 0 ? 0u : k == 1 ? 1u : u(ri(2, 200))); }
        wfrand(pv + 0x18, 2, -0.3f, 0.3f);
        wfrand(pv + 0x20, 3, -0.2f, 0.2f);
        wfrand(pv + 0x30, 3, -0.6f, 0.6f);
        if (coin()) wf(pv + 0x38, rf(-0.1f, 0.6f));
        const uint32_t owner = any_moby(m);
        w32(pv, owner);
        if (ri(0, 3) == 0) { Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-0.6f, 0.6f); q.y += rf(-0.6f, 0.6f); ee_ref<Vec4>(any_moby(owner) + 0x10) = q; }
        if (ri(0, 5) == 0) wf(m + 0x10 + 4 * u(ri(0, 2)), coin() ? rf(1.0f, 2.5f) : rf(1020.0f, 1022.0f));
        if (coin()) { ee_ref<Vec4>(m + 0xe0) = Vec4{rf(-1, 1), rf(-1, 1), rf(-1, 1), 0.0f}; }
        wfrand(m + 0x40, 3, -3.1f, 3.1f);
        if (coin()) hero_around(ee_ref<Vec4>(m + 0x10), 0.0f, 1.5f);
        w32(OV(0x0015f5cc), R());
        seed_rand();
    };
    for (int st = 0; st < 3; ++st)
        classcheck("class403_update", upd, subjects, class403_update, [vary, st](uint32_t m) { vary(m, st); }, 40);
    for (int st = 0; st < 3; ++st)
        classcheck("class403_trail", trail, subjects, class403_trail, [vary, st](uint32_t m) { vary(m, st); }, 30);
}

// ---- class 325 (shared_class325.cpp)
void class325_checks() {
    const uint32_t upd = copy_here({{14, 0x002de670}, {15, 0x002ca240}});
    const uint32_t draw = copy_here({{1, 0x0030bfa8}, {14, 0x002dec40}, {15, 0x002ca810}});
    if (upd == 0) not_here("class325_update");
    else {
        auto subjects = [upd] { return placed_or_spawned(upd, 325); };
        // the shell: its velocity (rising or falling), spin, fuse (idle, ending, running), the moby it ignores (now and then
        // one at its place), near the level's box edges now and then
        classcheck("class325_update", upd, subjects, class325_update, [](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            wfrand(pv, 3, -0.6f, 0.6f);
            wfrand(pv + 0x10, 3, -0.2f, 0.2f);
            { const int k = ri(0, 3); w32(pv + 0x20, k == 0 ? 0u : k == 1 ? 1u : u(ri(2, 200))); }
            const uint32_t owner = any_moby(m);
            w32(pv + 0x24, owner);
            if (ri(0, 3) == 0) ee_ref<Vec4>(any_moby(owner) + 0x10) = ee_ref<Vec4>(m + 0x10);
            if (ri(0, 5) == 0) wf(m + 0x10 + 4 * u(ri(0, 2)), coin() ? rf(1.0f, 2.5f) : rf(1020.0f, 1022.0f));
            wfrand(m + 0x40, 3, -3.1f, 3.1f);
            if (coin()) hero_around(ee_ref<Vec4>(m + 0x10), 0.0f, 1.5f);
            seed_rand();
        }, 60);
    }
    if (draw == 0) { not_here("class325_draw"); return; }
    // the glow: on class 325's mobys (Novalis: class 1510's, its update 0x30ba18), their axes turned, the camera about them
    const uint32_t owner_upd = g_oracle_level == 1 ? 0x0030ba18u : upd;
    const uint32_t cls = g_oracle_level == 1 ? 1510u : 325u;
    classcheck("class325_draw", draw, [owner_upd, cls] { return placed_or_spawned(owner_upd, cls); }, class325_draw, [](uint32_t m) {
        if (coin()) {
            Mat4 r{};
            euler_to_mat4_fpu(r, Vec4{rf(-3, 3), rf(-3, 3), rf(-3, 3), 0.0f});
            for (uint32_t k = 0; k < 3; ++k) ee_ref<Vec4>(m + 0xc0 + k * 0x10) = r.r[k];
        }
        packet_cursor();
        place_drawn(class325_draw, m, [m](const Vec4& q) { put_at(m, q); });
        if (ri(0, 4) == 0) camera_around(ee_ref<Vec4>(m + 0x10), 1.0f, 15.0f);
        camera_lift();
        draw_info("class325_draw", m, class325_draw);
        seed_rand();
    }, 40);
}

// ---- classes 250 and 1331 (shared_class250.cpp, shared_class1331.cpp)
void class250_1331_checks() {
    const uint32_t upd250 = copy_here({{14, 0x002d96e0}, {15, 0x002c6f08}});
    if (upd250 == 0) not_here("class250_update");
    else {
        // every state of its table (and past it), armed or not, the timer (idle, ending, running), the animation ended or not,
        // the active camera of type 20 or not, the hero moby's byte +0x24
        for (int st = 0; st < 10; ++st)
            classcheck("class250_update", upd250, [upd250] { return placed_or_spawned(upd250, 250); }, class250_update, [st](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w8(m + 0x20, u(st));
                w32(pv, u(ri(0, 1)));
                w32(pv + 4, u(ri(0, 40)));
                { const int k = ri(0, 3); w32(pv + 8, k == 0 ? 0u : k == 1 ? 1u : u(ri(2, 40))); }
                if (coin()) w8(m + 0x70, ee_read<uint8_t>(m + 0x70) ^ 2u);
                const uint32_t cam = ee_read<uint32_t>(OV(0x00166e00));
                if (cam != 0 && ri(0, 2) != 0) w16(cam + 0x86, coin() ? 0x14u : u(ri(0, 0x20)));
                if (ri(0, 3) == 0) w8(0x001413f4, coin() ? 2u : u(ri(0, 4)));
                seed_rand();
            }, 12);
    }
    const uint32_t upd1331 = copy_here({{14, 0x003039e0}, {15, 0x002e8910}});
    if (upd1331 == 0) { not_here("class1331_update"); return; }
    // the slots: each idle (its moby alive or dead), asked to start (2 / 3), running (4) or none, their regions held or not
    for (int st = 0; st < 3; ++st)
        classcheck("class1331_update", upd1331, [upd1331] { return placed_or_spawned(upd1331, 1331); }, class1331_update, [st](uint32_t m) {
            w8(m + 0x20, u(st));
            for (uint32_t i = 0; i < 3; ++i) {
                w32(OV(0x00161bf8) + i * 4, u(ri(0, 4)));
                const uint32_t owner = any_moby(m);
                w32(OV(0x00161c88) + i * 4, owner);
                if (ri(0, 2) == 0) w8(owner + 0x20, coin() ? 0xfeu : u(ri(0x80, 0xfd)));
                if (coin()) w32(OV(0x00161c38) + i * 4, 0xffffffffu);
                if (coin()) w32(OV(0x00161c48) + i * 4, 0xffffffffu);
            }
            seed_rand();
        }, 40);
}

// ---- class 28's draw callbacks (shared_class28_draw.cpp)
void class28_draw_checks() {
    const uint32_t upd = copy_here({{14, 0x002b17d8}, {15, 0x002955c0}});
    const uint32_t glows = copy_here({{14, 0x002b2928}, {15, 0x00296710}});
    const uint32_t spot = copy_here({{14, 0x002b3a20}, {15, 0x00297808}});
    if (upd == 0) {
        for (const char* n : {"class28_draw_glows", "class28_draw_spot"}) not_here(n);
        return;
    }
    auto subjects = [upd] { return placed_or_spawned(upd, 28); };
    // the five glows' points about the moby (their w lanes too), the camera about them
    classcheck("class28_draw_glows", glows, subjects, class28_draw_glows, [](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        Vec4 off[5];
        for (Vec4& o : off) o = Vec4{rf(-0.6f, 0.6f), rf(-0.6f, 0.6f), rf(-0.3f, 0.8f), coin() ? 1.0f : rf(0, 2)};  // (close: all five on screen)
        auto place = [m, pv, off](const Vec4& q) {
            put_at(m, q);
            for (uint32_t k = 0; k < 5; ++k) ee_ref<Vec4>(pv + 0x1e0 + k * 0x10) = Vec4{q.x + off[k].x, q.y + off[k].y, q.z + off[k].z, off[k].w};
        };
        place(ee_ref<Vec4>(m + 0x10));
        packet_cursor();
        place_drawn(class28_draw_glows, m, place);
        if (ri(0, 4) == 0) camera_around(ee_ref<Vec4>(m + 0x10), 1.0f, 15.0f);
        camera_lift();
        draw_info("class28_draw_glows", m, class28_draw_glows);
        seed_rand();
    }, 40);
    // the spot: the camera at it, nearer than the pull back, within the reach and beyond it
    classcheck("class28_draw_spot", spot, subjects, class28_draw_spot, [](uint32_t m) {
        packet_cursor();
        place_drawn(class28_draw_spot, m, [m](const Vec4& q) { put_at(m, q); });
        const int k = ri(0, 3);
        if (k == 0) ee_ref<Vec4>(OV(0x00166dc0)) = ee_ref<Vec4>(m + 0x10);
        else if (k == 1) {  // (nearer than the pull back, 2, on the way to the view: no move; else the camera where it is)
            const Vec4 v = ee_ref<Vec4>(OV(0x0016ccf0)), p = ee_ref<Vec4>(m + 0x10);
            const float vw = v.w != 0.0f ? v.w : 1.0f;
            Vec4 d{v.x / vw - p.x, v.y / vw - p.y, v.z / vw - p.z, 0.0f};
            const float l = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z), r = rf(0.1f, 1.9f) / (l > 0.001f ? l : 1.0f);
            ee_ref<Vec4>(OV(0x00166dc0)) = Vec4{p.x + d.x * r, p.y + d.y * r, p.z + d.z * r, p.w};
        }
        draw_info("class28_draw_spot", m, class28_draw_spot);
        seed_rand();
    }, 40);
}

// ---- class 77 (shared_class77.cpp)
void class77_checks() {
    const uint32_t upd = copy_here({{15, 0x002a2488}, {17, 0x002a7b58}});
    const uint32_t draw = copy_here({{15, 0x002a29b8}, {17, 0x002a8088}});
    const uint32_t wake = copy_here({{15, 0x00161ab8}, {17, 0x0016198c}});
    if (upd == 0) {
        for (const char* n : {"class77_update", "class77_draw"}) not_here(n);
        return;
    }
    auto subjects = [upd] { return placed_or_spawned(upd, 77); };
    // every state, the level's switch, the glow's frame word (none, this frame, another), the timer, the hero's point +0xd0
    // at the mine or not, its animation's set and end, a hit of the kinds 0x330000 or others
    for (int st = 0; st < 6; ++st)
        classcheck("class77_update", upd, subjects, class77_update, [st, wake](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(st));
            w32(wake, ri(0, 3) == 0 ? 0u : 1u);
            { const int k = ri(0, 3); w32(pv + 0x6c, k == 0 ? 0u : T); if (k == 1) w32(T, ee_read<uint32_t>(OV(0x0015f5cc))); else w32(T, R()); }
            { const int k = ri(0, 3); w32(pv + 0x68, k == 0 ? 0u : k == 1 ? 1u : u(ri(2, 200))); }
            wf(pv + 0x64, rf(-1.0f, 1.0f));
            hero_around(ee_ref<Vec4>(m + 0x10), 0.0f, 6.0f);
            if (ri(0, 3) == 0) ee_ref<Vec4>(0x0013f420) = ee_ref<Vec4>(m + 0x10);
            else ee_ref<Vec4>(0x0013f420) = ee_ref<Vec4>(0x0013f3d0);
            wf(0x0013f418, ee_read<float>(m + 0x18) - rf(0.0f, 1.0f));
            if (coin()) w8(m + 0x53, u(ri(0, 2)));
            if (coin()) w8(m + 0x70, ee_read<uint8_t>(m + 0x70) ^ 2u);
            give_hit(m, 0x330000u);
            w32(OV(0x0015f5cc), R() & 0xffff);
            seed_rand();
        }, 25);
    // the glow: the drawing moby's group, its members resting or not, about the hero
    classcheck("class77_draw", draw, subjects, class77_draw, [](uint32_t m) {
        const uint32_t list = ee_read<uint32_t>(OV(0x001ab840) + (ee_read<uint8_t>(m + 0x21) << 2));
        std::vector<std::pair<uint32_t, Vec4>> members;
        for (uint32_t e = list;; e += 2) {
            const uint32_t member = ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
            w8(member + 0x20, ri(0, 3) == 0 ? 1u : u(ri(2, 4)));
            members.push_back({member, Vec4{rf(-1.5f, 1.5f), rf(-1.5f, 1.5f), rf(-0.5f, 1.5f), 0.0f}});
            if (static_cast<int16_t>(ee_read<uint16_t>(e)) < 0) break;
        }
        packet_cursor();
        place_drawn(class77_draw, m, [members](const Vec4& q) {
            for (const auto& mo : members) put_at(mo.first, Vec4{q.x + mo.second.x, q.y + mo.second.y, q.z + mo.second.z, 1.0f});
        });
        if (ri(0, 4) == 0) camera_around(ee_ref<Vec4>(m + 0x10), 1.0f, 15.0f);
        draw_info("class77_draw", m, class77_draw);
        seed_rand();
    }, 40);
}

// ---- the draw callbacks of classes 78, 123, 44 and 1382 (shared_class78_draw.cpp, shared_class123_draw.cpp,
// shared_three_glows.cpp): on the classes' mobys (placed, or spawned) at the hero, their points about him
void quartu_draw_checks() {
    struct Cb { const char* name; uint32_t addr; uint32_t cls_fn; uint32_t cls; void (*port)(uint32_t); };
    const Cb cbs[] = {
        {"class78_draw", copy_here({{15, 0x002a3138}, {17, 0x002a8808}}), copy_here({{15, 0x002a2bf0}, {17, 0x002a82c0}}), 78, class78_draw},
        {"class123_draw", copy_here({{15, 0x002a7628}, {17, 0x002acf00}}), copy_here({{15, 0x002a6e68}, {17, 0x002ac740}}), 123, class123_draw},
        {"class44_glows_draw", copy_here({{15, 0x0029aba8}, {17, 0x002a2aa8}}), copy_here({{15, 0x002979d8}, {17, 0x0029f8d8}}), 44, class44_glows_draw},
        {"class1382_glows_draw", copy_here({{17, 0x002f0448}}), copy_here({{17, 0x002eeb68}}), 1382, class1382_glows_draw},
    };
    for (const Cb& cb : cbs) {
        if (cb.addr == 0) { not_here(cb.name); continue; }
        const uint32_t fn = cb.cls_fn, cls = cb.cls;
        const char* name = cb.name;
        auto port = cb.port;
        classcheck(name, cb.addr, [fn, cls] { return placed_or_spawned(fn, cls); }, port, [name, port](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (coin()) {
                Mat4 r{};
                euler_to_mat4_fpu(r, Vec4{rf(-3, 3), rf(-3, 3), rf(-3, 3), 0.0f});
                for (uint32_t k = 0; k < 3; ++k) ee_ref<Vec4>(m + 0xc0 + k * 0x10) = r.r[k];
            }
            // (class 78: its fade, the frame counter; class 123: its far end; the glows: their points, the moby's colour)
            w32(pv + 0x14, coin() ? 0u : u(ri(1, 60)));
            w32(OV(0x0015f3f8), R() & 0xfffff);
            w32(m + 0x90, R());
            const Vec4 far{rf(-3, 3), rf(-3, 3), rf(-1, 2), 0.0f};
            Vec4 pts[6];
            for (Vec4& o : pts) o = Vec4{rf(-1, 1), rf(-1, 1), rf(0, 2), 0.0f};
            auto place = [m, pv, far, pts](const Vec4& q) {
                put_at(m, q);
                ee_ref<Vec4>(pv + 0x10) = Vec4{q.x + far.x, q.y + far.y, q.z + far.z, q.w};
                for (uint32_t k = 0; k < 3; ++k) {
                    ee_ref<Vec4>(pv + 0x200 + k * 0x10) = Vec4{q.x + pts[k].x, q.y + pts[k].y, q.z + pts[k].z, 1.0f};
                    ee_ref<Vec4>(pv + 0x2a0 + k * 0x10) = Vec4{q.x + pts[k + 3].x, q.y + pts[k + 3].y, q.z + pts[k + 3].z, 1.0f};
                }
            };
            place(ee_ref<Vec4>(m + 0x10));
            packet_cursor();
            place_drawn(port, m, place);
            if (ri(0, 4) == 0) camera_around(ee_ref<Vec4>(m + 0x10), 1.0f, 15.0f);
            camera_lift();
            draw_info(name, m, port);
            seed_rand();
        }, 40);
    }
}

// ---- the rippling surfaces (shared_ripples.cpp)
void ripple_checks() {
    struct Cls { int level; uint32_t fn; uint32_t cls; void (*port)(uint32_t); };
    static const Cls cls[] = {{15, 0x002eb0a0, 1408, class1408_update}, {15, 0x002eb458, 1409, class1409_update}, {15, 0x002edf58, 1565, class1565_update},
                              {15, 0x002ee338, 1567, class1567_update}, {17, 0x002f08e0, 1405, class1405_update}, {17, 0x002f0ca0, 1406, class1406_update},
                              {17, 0x002f1060, 1407, class1407_update}, {17, 0x002f1418, 1426, class1426_update}};
    bool any = false;
    for (const Cls& c : cls) {
        if (c.level != g_oracle_level) continue;
        any = true;
        const uint32_t fn = c.fn, cl = c.cls;
        for (int st = 0; st < 2; ++st)
            classcheck("ripple_update", fn, [fn, cl] { return placed_or_spawned(fn, cl); }, c.port, [st](uint32_t m) {
                w8(m + 0x20, u(st));
                if (coin()) w16(m + 0x32, R());
                if (coin()) w16(m + 0x34, R());
                seed_rand();
            }, 12);
    }
    if (!any) not_here("ripple_update");
    // the draws: the phase, the frame counter, the moby where the meshes are drawn
    struct Dr { const char* name; uint32_t addr; uint32_t fn; uint32_t cls; void (*port)(uint32_t); };
    const Dr drs[] = {{"ripple_rings_draw", copy_here({{15, 0x002eac90}, {17, 0x002f04d0}}), copy_here({{15, 0x002eb0a0}, {17, 0x002f08e0}}),
                       g_oracle_level == 17 ? 1405u : 1408u, ripple_rings_draw},
                      {"ripple_pool_draw", copy_here({{15, 0x002eb108}, {17, 0x002f10c8}}), copy_here({{15, 0x002eb458}, {17, 0x002f1418}}),
                       g_oracle_level == 17 ? 1426u : 1409u, ripple_pool_draw}};
    for (const Dr& d : drs) {
        if (d.addr == 0) { not_here(d.name); continue; }
        const uint32_t fn = d.fn, cl = d.cls;
        const char* name = d.name;
        auto port = d.port;
        classcheck(name, d.addr, [fn, cl] { return placed_or_spawned(fn, cl); }, port, [name, port](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            wf(pv, rf(-3.1f, 3.1f));
            wf(pv + 4, rf(-3.1f, 3.1f));
            w32(OV(0x0015f5cc), R() & 0xfffff);
            packet_cursor();
            place_drawn(port, m, [m](const Vec4& q) { put_at(m, q); });
            draw_info(name, m, port);
            seed_rand();
        }, 40);
    }
}

// ---- classes 196 / 197, 655's glows, 408's tint (shared_class196.cpp, shared_class655_glows.cpp, shared_class408_tint.cpp)
void lift_glow_tint_checks() {
    const uint32_t lift = copy_here({{15, 0x002bddb0}, {17, 0x002c0f00}});
    if (lift == 0) not_here("class196_update");
    else {
        const uint32_t cls = g_oracle_level == 17 ? 197u : 196u;
        // every state of its table (and past it), the moby it waits for (none, one in state 4 or another, no such moby) and
        // its boxes (none, the hero in them or not), its speed about its top speed, its class (196 / 1958 go backwards)
        for (int st = 0; st < 8; ++st)
            classcheck("class196_update", lift, [lift, cls] { return placed_or_spawned(lift, cls); }, class196_update, [st](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w8(m + 0x20, u(st));
                const int k = ri(0, 3);
                if (k == 0) w32(pv + 0x10, 0xffffffffu);
                else if (k == 1) {
                    const uint32_t other = any_moby(m), first = ee_read<uint32_t>(OV(0x0015ffd8));
                    w32(pv + 0x10, (other - first) >> 8);
                    w8(other + 0x20, coin() ? 4u : u(ri(0, 6)));
                } else if (k == 2) w32(pv + 0x10, 0);
                for (uint32_t o : {0x18u, 0x1cu}) {
                    const int b = ri(0, 4);
                    if (b == 0) w32(pv + o, 0xffffffffu);
                    else if (b <= 2) w32(pv + o, u(ri(0, 3)));
                }
                for (uint32_t o : {0x18u, 0x1cu})
                    if (ee_read<int32_t>(pv + o) >= 0 && coin()) {  // (the hero at the box's centre)
                        const uint32_t box = ee_read<uint32_t>(OV(0x001600ec)) + (ee_read<uint32_t>(pv + o) << 7);
                        ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(box + 0x30);
                    }
                const float top = ee_read<float>(m + 0x2c) * 3.0f / ee_read<float>(ee_read<uint32_t>(m + 0x24) + 0x24);
                wf(pv + 0x14, ri(0, 3) == 0 ? 0.0f : rf(-1.2f, 1.2f) * (std::isfinite(top) ? top : 1.0f));
                if (ri(0, 2) == 0) w16(m + 0xa6, coin() ? 0xc4u : (coin() ? 0x7a6u : 0xc5u));
                seed_rand();
            }, 20);
    }
    const uint32_t glows = copy_here({{15, 0x002d8e48}, {17, 0x002d74c0}});
    if (glows == 0) not_here("class655_glows");
    else {
        const uint32_t owner = copy_here({{15, 0x002d6810}, {17, 0x002d4e88}});
        classcheck("class655_glows", glows, [owner] { return placed_or_spawned(owner, 655); }, class655_glows, [](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const Vec4 off{rf(-1, 1), rf(-1, 1), rf(-0.5f, 1.5f), 1.0f};
            auto place = [m, pv, off](const Vec4& q) { put_at(m, q); ee_ref<Vec4>(pv + 0xf0) = Vec4{q.x + off.x, q.y + off.y, q.z + off.z, 1.0f}; };
            place(ee_ref<Vec4>(m + 0x10));
            packet_cursor();
            place_drawn(class655_glows, m, place);
            camera_lift();
        draw_info("class655_glows", m, class655_glows);
            seed_rand();
        }, 40);
    }
    const uint32_t tint = copy_here({{15, 0x002cbd88}, {17, 0x002cc740}});
    if (tint == 0) { not_here("class408_tint"); return; }
    const uint32_t w = copy_here({{15, 0x00161abc}, {17, 0x00161990}});
    // the level's words: the colour word (its low byte past 8 / 16 or not, its high byte signed), the strength (below 0 now
    // and then), the period (0 now and then); the frame counter
    fcheck("class408_tint", tint, 60, Ret::None, [w] {
        level();
        {  // (the low byte's marks 9 and 17 and their neighbours often)
            static const uint32_t marks[] = {0, 7, 8, 9, 10, 15, 16, 17, 18, 24};
            const int k = ri(0, 3);
            const uint32_t low = coin() ? marks[ri(0, 9)] : u(ri(0, 24));
            w32(w + 0x10, k == 0 ? low : k == 1 ? low | (R() & 0xff000000u) : R());
        }
        wf(w, rf(-1.0f, 2.0f));
        wf(w + 0x14, rf(-2.0f, 2.0f));
        w32(w + 0x18, ri(0, 5) == 0 ? 0u : u(ri(1, 200)));
        w32(OV(0x0015f5cc), R() & 0xfffff);
        packet_cursor();  // (the register write goes into the frame's packets)
        return Args{{ee_read<uint32_t>(OV(0x0015ffd8))}, {}};
    }, [](A a) { vu0_sync(); class408_tint(a.i[0]); return uint64_t{0}; });
}

// ---- class 848 and the small helpers (shared_class848.cpp, shared_level_helpers.cpp)
void class848_helper_checks() {
    const uint32_t upd = copy_here({{3, 0x002d47c0}, {7, 0x00304ff0}, {15, 0x002de430}});
    const uint32_t bullet = copy_here({{3, 0x00260130}, {7, 0x0029a018}, {15, 0x00261560}});
    if (upd == 0) {
        for (const char* n : {"class848_update", "class848_bullet"}) not_here(n);
    } else {
        const uint32_t heli_fn = copy_here({{3, 0x002cca00}, {7, 0x002fd908}, {15, 0x002c5630}});
        // the burst: spawned near the hero, its owner the level's helicopter (or another moby), its states 0 .. 3; its
        // velocity, height and timer (on the sixth frames: a bullet), its gun; the owner's sound slot (none, the owner's,
        // another's), the owner's state (Quartu's 0x40 / 1 end it), dying or gone now and then; the hero at the aim or not
        for (int st = 0; st < 4; ++st)
            classcheck("class848_update", upd, [upd] { return placed_or_spawned(upd, 848); }, class848_update, [st, heli_fn](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w8(m + 0x20, u(st));
                const std::vector<uint32_t> helis = mobys_with(heli_fn);
                uint32_t owner = !helis.empty() && ri(0, 4) != 0 ? pick(helis) : any_moby(m);
                if (ri(0, 9) == 0) owner = 0;
                w32(pv + 0x10, owner);
                if (owner != 0) {
                    const int k = ri(0, 5);
                    if (k == 0) w8(owner + 0x20, coin() ? 0xfeu : 0xfdu);
                    else if (k == 1) w8(owner + 0x20, coin() ? 0x40u : 1u);
                    const uint32_t opv = ee_read<uint32_t>(owner + 0x78);
                    if (opv != 0) {
                        for (uint32_t o : {0x250u, 0x1b8u}) {
                            const int32_t id = ri(0, 3) == 0 ? -1 : ri(0, 7);
                            w32(opv + o, u(id));
                            if (id >= 0) {
                                const uint32_t e = 0x0013e550 + u(id) * 0x70;
                                if (coin()) w32(e + 0x88, owner);
                                w8(e + 0x74, u(ri(0, 1)));
                            }
                        }
                    }
                }
                wf(pv, rf(-0.4f, 0.4f)); wf(pv + 4, rf(-0.4f, 0.4f)); wf(pv + 8, rf(-0.1f, 0.1f));
                wf(pv + 0xc, ee_read<float>(m + 0x18) + rf(-3.0f, 3.0f));
                { const int k = ri(0, 3); w32(pv + 0x14, k == 0 ? u(ri(0, 1)) : k == 1 ? u(6 * ri(1, 10)) : u(ri(2, 90))); }
                w32(pv + 0x18, u(ri(0, 1)));
                if (owner != 0 && coin()) { Vec4 q = ee_ref<Vec4>(m + 0x10); q.z += rf(2.0f, 8.0f); put_at(owner, q); }
                if (coin()) hero_around(ee_ref<Vec4>(m + 0x10), 0.0f, 3.0f);
                const uint32_t hero = ee_read<uint32_t>(0x001413d0);
                if (hero != 0 && coin()) put_at(hero, ee_ref<Vec4>(0x0013f3d0));
                seed_rand();
            }, 25);
        // the bullet's spawn: an owner, the three vectors from the test area, the life
        fcheck("class848_bullet", bullet, 40, Ret::None, [] {
            level();
            for (uint32_t k = 0; k < 3; ++k) wfrand(T + k * 0x10, 4, -50.0f, 50.0f);
            return Args{{any_moby(0), T, T + 0x10, T + 0x20, u(ri(-2, 100))}, {}};
        }, [](A a) { vu0_sync(); class848_bullet(a.i[0], a.i[1], a.i[2], a.i[3], static_cast<int32_t>(a.i[4])); return uint64_t{0}; });
    }
    // the spark at a chain point: a moby of the level (its joints 0 .. 7), a size and a pull
    const uint32_t spark = copy_here({{7, 0x0029b8d8}, {15, 0x00262d68}, {17, 0x0026cf30}});
    if (spark == 0) not_here("chain_spark_spawn");
    else
        fcheck("chain_spark_spawn", spark, 60, Ret::Int32, [] {
            // (a moby and joint whose chain point the port can read: a state that sends it through a pointer outside RAM is
            // dealt again, as classcheck does)
            static std::vector<uint8_t> saved;
            uint8_t* ram = ee_get_ram_base();
            uint32_t m = 0, joint = 0;
            const float size = rf(0.05f, 2.0f), pull = rf(-1.0f, 3.0f);
            // (the mobys of the classes that spawn it: 934 on Quartu and Drek's fleet, 1049 / 1106 on Umbris)
            const uint32_t users[] = {copy_here({{15, 0x002e4380}, {17, 0x002e1c20}, {7, 0x0030e458}}), copy_here({{7, 0x00314150}})};
            for (int attempt = 0; attempt < 40; ++attempt) {
                level();
                std::vector<uint32_t> ms;
                for (uint32_t f : users)
                    if (f != 0) for (uint32_t x : mobys_with(f)) ms.push_back(x);
                m = ms.empty() ? any_moby(0) : pick(ms);
                // (the joints the classes pass: class 1106 0 .. 3, the others 0)
                joint = ee_read<uint32_t>(m + 0x74) == copy_here({{7, 0x00314150}}) ? u(ri(0, 3)) : 0u;
                seed_rand();
                saved.assign(ram, ram + 0x02000000);
                bool ok = true;
                const uint32_t sp0 = ee_stack_ptr();
                try { chain_spark_spawn(m, static_cast<int16_t>(joint), size, pull); } catch (const std::exception&) { ok = false; }
                ee_stack_ptr() = sp0;
                std::memcpy(ram, saved.data(), saved.size());
                if (ok) break;
            }
            return Args{{m, joint}, {size, pull}};
        }, [](A a) { vu0_sync(); return r32(chain_spark_spawn(a.i[0], static_cast<int16_t>(a.i[1]), a.f[0], a.f[1])); });
    // the held moby: none, or a moby of the level (its +0x34 bits set)
    const uint32_t held = copy_here({{1, 0x002a2480}, {14, 0x00294d38}});
    if (held == 0) not_here("held_moby_show");
    else
        fcheck("held_moby_show", held, 30, Ret::None, [] {
            level();
            const uint32_t m = ri(0, 3) == 0 ? 0u : any_moby(0);
            w32(0x0013e030, m);
            if (m != 0) w16(m + 0x34, R());
            return Args{{0}, {}};
        }, [](A) { vu0_sync(); held_moby_show(); return uint64_t{0}; });
}

// ---- class 686 (shared_class686.cpp)
void class686_checks() {
    const uint32_t upd = copy_here({{1, 0x002f6a30}, {14, 0x002ef840}});
    if (upd == 0) { not_here("class686_update"); return; }
    // the shell: flying (state 0: its target out of reach, within it now and then) or arrived (1, 2); out of the level's box
    // now and then; the low-detail flag; the camera near (below 7, 10 .. 30) or far (past 100); the screen's fade; the
    // moby at it (low detail: classes 709 .. 711 choose the sound); its four mobys to wake (class 700 resting with the hero
    // and the camera in their box, or not, or none)
    for (int st = 0; st < 3; ++st)
        classcheck("class686_update", upd, [upd] { return placed_or_spawned(upd, 686); }, class686_update, [st](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(st));
            const Vec4 p = ee_ref<Vec4>(m + 0x10);
            ee_ref<Vec4>(pv) = Vec4{rf(-0.5f, 0.5f), rf(-0.5f, 0.5f), rf(-0.6f, 0.2f), 0.0f};
            ee_ref<Vec4>(pv + 0x10) = Vec4{p.x + rf(-2, 2), p.y + rf(-2, 2), p.z + rf(-2, 1), 1.0f};
            wf(pv + 0x38, ri(0, 2) == 0 ? rf(2.0f, 4.0f) : rf(0.1f, 1.0f));
            if (ri(0, 6) == 0) wf(m + 0x10 + 4 * u(ri(0, 2)), rf(-0.5f, 0.3f));
            w32(OV(0x0015f404), ri(0, 2) == 0 ? 1u : 0u);
            { const int k = ri(0, 3); if (k == 0) camera_around(p, 0.5f, 6.5f); else if (k == 1) camera_around(p, 10.0f, 30.0f); else if (k == 2) camera_around(p, 110.0f, 150.0f); }
            wf(OV(0x0015f5d0), coin() ? rf(0.0f, 0.9f) : 1.0f);
            if (coin()) {
                const uint32_t o = any_moby(m);
                put_at(o, ee_ref<Vec4>(m + 0x10));
                w16(o + 0xa6, u(ri(708, 712)));
            }
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
            for (uint32_t k = 0; k < 4; ++k) {
                if (ri(0, 2) == 0) { w32(pv + 0x20 + k * 4, 0xffffffffu); continue; }
                const uint32_t o = any_moby(m);
                w32(pv + 0x20 + k * 4, (o - first) >> 8);
                const uint32_t opv = ee_read<uint32_t>(o + 0x78);
                if (opv == 0 || ri(0, 3) == 0) continue;
                w16(o + 0xa6, 0x2bc);
                w16(opv + 4, ri(0, 3) == 0 ? 1u : 0u);
                w32(opv, ri(0, 3) == 0 ? 5u : 0u);
                const uint32_t box = u(ri(0, 3));
                w32(opv + 8, box);
                if (coin()) {
                    const uint32_t b = ee_read<uint32_t>(OV(0x001600ec)) + (box << 7);
                    ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(b + 0x30);
                    if (coin()) ee_ref<Vec4>(OV(0x00166dc0)) = ee_ref<Vec4>(b + 0x30);
                }
            }
            packet_cursor();
            seed_rand();
        }, 30);
}
}  // namespace

void batch_507() {
    if (g_only_batch != 507) return;
    g_batch = 507;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    // (the scratchpad words the pose function 0x245f18 spills the caller's $t8 / $t9 into: batch 120's)
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // 989snd's IOP server (as batch 120's): replies a function of the command
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
    // the segment record 0x173e90..0x173e9c: segment_collide 0x1efc70 stores a VU0 register there whose lanes are another
    // computation's leftovers, which the port of segment_collide does not reproduce (batches 124 / 200 / 260 / 506)
    // (Quartu's 0x173ea0 .. 0x173ebc as well: the sphere test leaves a VU0 register's stale lanes there too, as batch 240 has it)
    for (uint32_t k = 0; k < 0x30; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
    // and the collision results' w lanes (0x173e40 .. 0x173fc0, + 0xc of each row: the contact vectors' VU0 leftovers in the
    // original, 0 in the port of the collision; batches 140 / 223 / 506)
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

    class1224_checks();
    class403_checks();
    class325_checks();
    class250_1331_checks();
    class28_draw_checks();
    class77_checks();
    quartu_draw_checks();
    ripple_checks();
    lift_glow_tint_checks();
    class848_helper_checks();
    class686_checks();

    //@@NEXT
    g_pre_call = nullptr;
    g_extra_noop_hooks.clear();
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
