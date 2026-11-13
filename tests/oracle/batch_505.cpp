// Oracle batch 505 (stage 4 task F2, #149): class code several of the levels 15, 16, 17 and 18 carry a copy of, each family
// ported once as shared hand-written code (src/game/ovl/shared_*.cpp), against every level's own copy at that level's own
// address. The level copies stay transliterated in their units until each level is re-glued; here every copy is the
// original and the one shared port the port:
//  - shared_class49.cpp: class 49, a shot that bursts into shards (its update, its trail, which class 50's copies on levels
//    16 and 18 share with a tuning record of their own, and its shards; levels 15, 16, 17, 18);
//  - shared_class256.cpp: class 256, a homing rocket, an earlier build of the Devastator's (levels 15, 18);
//  - shared_class638.cpp: class 638, a hovering gun drone (its update, hits, seek, turn, burst, aim, sparks and group timers;
//    levels 16, 18), and the class 49 shot it fires (shared_class49.cpp's class49_spawn);
//  - shared_class1356.cpp: class 1356, a carrier of four drones (its update, hits, path following, the paths' headings, its
//    joint, a drone attached and let go; levels 16, 18), and the class 50 shot it fires (shared_class49.cpp's class50_spawn);
//  - shared_class1523.cpp: class 1523, Giant Clank's charged shot, and its streak (levels 15, 18);
//  - shared_class123.cpp: class 123's update, a laser emitter that circles, swings or slides (levels 15, 17);
//  - shared_class1443.cpp: class 1443 (1890 on level 18), a target of spinning rings: its update, its group's helpers and its
//    draw callback (levels 16, 18);
// Subjects: the level's own mobys of the class after the first gameplay frame (the port's, the view set up), or one spawned
// by the game's moby_spawn when the level places none, perturbed; the draw callbacks' packets into a buffer of the test area.
// No RAM dumps. Harness: batch 507's.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 505, N = 15, 16, 17 and 18; a family the level does not carry says so
// and is not run.

#include "ovl_oracle.h"

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class49.h>
#include <game/ovl/shared_class256.h>
#include <game/ovl/shared_class638.h>
#include <game/ovl/shared_class1356.h>
#include <game/ovl/shared_class1523.h>
#include <game/ovl/shared_class123.h>
#include <game/ovl/shared_class1443.h>
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
void classcheck(const char* name, uint32_t laddr, const std::function<std::vector<uint32_t>()>& subjects, const std::function<void(uint32_t)>& port,
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

// B505_INFO=1: how far a draw moved the packet cursor (that the check compares something)
void draw_info(const char* name, uint32_t m, const std::function<void(uint32_t)>& port) {
    if (std::getenv("B505_INFO") == nullptr) return;
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
void place_drawn(const std::function<void(uint32_t)>& port, uint32_t m, const std::function<void(const Vec4&)>& place) {
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
        if (std::getenv("B505_INFO") != nullptr) std::printf("INFO placed after %d tries\n", k + 1);
        place(q);
        return;
    }
    if (have_drawn) {  // (else where it draws at all: the GS state compared)
        if (std::getenv("B505_INFO") != nullptr) std::printf("INFO placed where drawn only\n");
        place(drawn);
        return;
    }
    if (std::getenv("B505_INFO") != nullptr) {
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

// ---- class 49 (shared_class49.cpp)
void class49_checks() {
    const uint32_t upd = copy_here({{15, 0x0029ac30}, {16, 0x002a05e8}, {17, 0x002a2b30}, {18, 0x002a76e0}});
    if (upd == 0) {
        for (const char* n : {"class49_update", "class49_trail", "class49_shards"}) not_here(n);
        return;
    }
    auto subjects = [upd] { return placed_or_spawned(upd, 49); };
    // the shot: a velocity (slow to fast, along the floor or down into it), its flight's sound played or not, its life
    // (idle, ending, running), its owner (none, another moby, the moby on its way), a moby put on its way now and then, the
    // last collision's moby (the owner or not), the camera near or far, the hero moby's byte +0x24 at 2 now and then
    auto vary = [](uint32_t m, int st) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, u(st));
        const float speed = coin() ? rf(0.05f, 0.6f) : rf(0.6f, 3.0f);
        const float a = rf(-3.1f, 3.1f), down = ri(0, 2) == 0 ? rf(-1.0f, -0.2f) : rf(-0.1f, 0.1f);
        wf(pv, speed * std::cos(a)); wf(pv + 4, speed * std::sin(a)); wf(pv + 8, speed * down);
        w32(pv + 0x10, u(ri(0, 1)));
        { const int k = ri(0, 3); w32(pv + 0x14, k == 0 ? 0u : k == 1 ? 1u : u(ri(2, 120))); }
        wf(m + 0x40, rf(-3.1f, 3.1f));
        if (ri(0, 2) == 0) {  // (near the hero, whose floor its path may cross)
            Vec4 q = ee_ref<Vec4>(0x0013f3d0);
            q.x += rf(-2, 2); q.y += rf(-2, 2); q.z += rf(0.2f, 1.5f);
            put_at(m, q);
        }
        const uint32_t other = any_moby(m);
        const int k = ri(0, 3);
        w32(pv + 0x18, k == 0 ? 0u : k == 1 ? other : any_moby(other));
        if (coin()) {  // (a moby on its way)
            const Vec4 p = ee_ref<Vec4>(m + 0x10);
            const float t = rf(-1.8f, 0.9f);
            put_at(other, Vec4{p.x + ee_read<float>(pv) * t, p.y + ee_read<float>(pv + 4) * t, p.z + ee_read<float>(pv + 8) * t, 1.0f});
        }
        if (ri(0, 2) == 0) w32(OV(0x00173e58), coin() ? ee_read<uint32_t>(pv + 0x18) : other);
        if (coin()) camera_around(ee_ref<Vec4>(m + 0x10), 0.0f, 25.0f);
        if (ri(0, 3) == 0) w8(0x001413f4, coin() ? 2u : u(ri(0, 4)));
        seed_rand();
    };
    for (int st = 0; st < 2; ++st)
        for (int pass = 0; pass < 3; ++pass)
            classcheck("class49_update", upd, subjects, class49_update, [vary, st](uint32_t m) { vary(m, st); }, st == 0 ? 40 : 10);

    // the trail: every copy of the level (class 49's and, on 16 / 18, class 50's) with its own record, on a shot of class 49
    struct TrailCopy { int level; uint32_t addr; uint32_t tune; };
    static const TrailCopy trails[] = {{15, 0x0029ae18, 0x00161418}, {16, 0x002a07d0, 0x00161388}, {16, 0x002a0f18, 0x001613e8},
                                       {17, 0x002a2d18, 0x001613b8}, {18, 0x002a78c8, 0x00161388}, {18, 0x002a8010, 0x001613e8}};
    for (const TrailCopy& t : trails) {
        if (t.level != g_oracle_level) continue;
        const uint32_t tune = t.tune;
        classcheck("class49_trail", t.addr, subjects, [tune](uint32_t m) { class49_trail(m, tune); }, [vary](uint32_t m) { vary(m, 0); }, 40);
    }
    const uint32_t shards = copy_here({{15, 0x0029af88}, {16, 0x002a0940}, {17, 0x002a2e88}, {18, 0x002a7a38}});
    classcheck("class49_shards", shards, subjects, class49_shards, [vary](uint32_t m) { vary(m, 0); }, 40);
}

// ---- class 256 (shared_class256.cpp)
void class256_checks() {
    const uint32_t upd = copy_here({{15, 0x0029d6c0}, {18, 0x002aa3b0}});
    if (upd == 0) { not_here("class256_update"); return; }
    auto subjects = [upd] { return placed_or_spawned(upd, 256); };
    // the rocket: its life about the homing's limit (150 frames) or ending, its target (none, the hero's moby, another moby,
    // one dying), leading it or not, its speeds, rates and kept heading, the hero's velocity share; flying into the floor
    // near the hero, out of the level's box, far from the hero, a moby (its owner or not) on its way, already bursting
    // (+0xbc 1 / 2); the camera near or far, the camera effects on or off, the particle load
    auto vary = [](uint32_t m, int pass) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        { const int k = ri(0, 4); w32(pv + 0x14, k == 0 ? 0u : k == 1 ? 1u : k == 2 ? u(ri(140, 170)) : u(ri(2, 139))); }
        const uint32_t other = any_moby(m);
        { const int k = ri(0, 3); w32(pv + 0x18, k == 0 ? 0u : k == 1 ? ee_read<uint32_t>(0x001413d0) : other); }
        if (ri(0, 5) == 0 && ee_read<uint32_t>(pv + 0x18) != 0) w8(ee_read<uint32_t>(pv + 0x18) + 0x20, coin() ? 0x80u : 0xfeu);
        wf(pv + 0x1c, rf(-1.0f, 3.0f));
        wf(pv + 0x20, rf(0.05f, 2.0f));
        wf(pv + 0x34, rf(0.05f, 2.5f));
        wfrand(pv + 0x24, 2, -0.3f, 0.3f);
        wf(pv + 0x2c, rf(-3.1f, 3.1f)); wf(pv + 0x30, rf(-1.5f, 1.5f));
        w32(pv + 0x38, u(ri(0, 1)));
        w16(pv + 0x3c, u(ri(0, 30))); w16(pv + 0x3e, u(ri(1, 30)));
        wfrand(pv, 3, -50.0f, 50.0f);
        if (ee_read<uint32_t>(pv + 0x18) != 0 && coin()) { Vec4 q = ee_ref<Vec4>(ee_read<uint32_t>(pv + 0x18) + 0x10); q.x += rf(-2, 2); q.y += rf(-2, 2); ee_ref<Vec4>(pv) = q; }
        wf(m + 0x48, rf(-3.1f, 3.1f)); wf(m + 0x44, rf(-1.5f, 1.5f));
        {
            Mat4 r{};
            euler_to_mat4_fpu(r, Vec4{rf(-3, 3), rf(-3, 3), rf(-3, 3), 0.0f});
            for (uint32_t k = 0; k < 3; ++k) ee_ref<Vec4>(m + 0xc0 + k * 0x10) = r.r[k];
        }
        const int where = ri(0, 5);
        Vec4 hero = ee_ref<Vec4>(0x0013f3d0);
        if (where <= 2) {  // (just above the hero's floor, heading down into it now and then)
            put_at(m, Vec4{hero.x + rf(-3, 3), hero.y + rf(-3, 3), hero.z + rf(0.0f, 1.0f), 1.0f});
            if (coin()) wf(m + 0x44, rf(0.6f, 1.5f));
        } else if (where == 3) {
            const uint32_t k = u(ri(0, 2));
            wf(m + 0x10 + 4 * k, rf(-0.5f, 0.5f));
        } else if (where == 4) {
            put_at(m, Vec4{hero.x + (coin() ? 255.0f : -262.0f), hero.y + rf(-5, 5), hero.z + rf(0, 5), 1.0f});
        }
        if (coin()) {  // (a moby on its way, its owner now and then; the hero's moby there now and then)
            const Vec4 p = ee_ref<Vec4>(m + 0x10);
            const uint32_t on_way = ri(0, 3) == 0 ? ee_read<uint32_t>(0x001413d0) : any_moby(m);
            if (on_way != 0) put_at(on_way, Vec4{p.x + rf(-0.6f, 0.6f), p.y + rf(-0.6f, 0.6f), p.z + rf(-0.6f, 0.6f), 1.0f});
            if (on_way != 0 && coin()) w16(on_way + 0x34, ee_read<uint16_t>(on_way + 0x34) | 0x4000u);  // (it takes the hit's contact record)
            w32(pv + 0x10, coin() ? on_way : other);
        } else {
            w32(pv + 0x10, other);
        }
        if (ee_read<uint32_t>(pv + 0x18) != 0 && ri(0, 2) == 0) {  // (a target faster than the rocket and coming at it: both lead times positive)
            const uint32_t t = ee_read<uint32_t>(pv + 0x18);
            Vec4 q = ee_ref<Vec4>(t + 0x10);
            q.z += ee_read<float>(pv + 0x1c);
            const Vec4 p = ee_ref<Vec4>(m + 0x10);
            Vec4 d{q.x - p.x, q.y - p.y, q.z - p.z, 0.0f};
            const float l = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
            if (l > 0.01f) {
                const float k = rf(1.0f, 8.0f) / l;
                ee_ref<Vec4>(pv) = Vec4{q.x + d.x * k + rf(-0.5f, 0.5f), q.y + d.y * k + rf(-0.5f, 0.5f), q.z + d.z * k, q.w};
            }
            wf(pv + 0x20, rf(0.05f, 0.6f));
            w32(pv + 0x38, 1);
            w16(pv + 0x3c, 0);
            if (coin()) {  // (headed about at the lead point it picks (the later meeting), its turn rates at rest: the turn short of its limit)
                w32(pv + 0x14, u(ri(2, 100)));
                wf(pv + 0x34, ee_read<float>(pv + 0x20));
                w32(pv + 0x24, 0); w32(pv + 0x28, 0);
                const Vec4 last = ee_ref<Vec4>(pv);
                const Vec4 tv{q.x - last.x, q.y - last.y, q.z - last.z, 0.0f};
                const float sp = ee_read<float>(pv + 0x20);
                const float a = sp * sp - (tv.x * tv.x + tv.y * tv.y + tv.z * tv.z);
                const float b = -2.0f * (tv.x * d.x + tv.y * d.y + tv.z * d.z), c = -(d.x * d.x + d.y * d.y + d.z * d.z);
                const float disc = b * b - 4.0f * a * c;
                Vec4 lead = d;
                if (disc >= 0.0f && a != 0.0f) {
                    const float t1 = (-b + std::sqrt(disc)) / (2.0f * a), t2 = (-b - std::sqrt(disc)) / (2.0f * a);
                    float t = -1.0f;
                    if (t1 > 0.0f && t2 > 0.0f) t = std::fmax(t1, t2); else if (t1 > 0.0f) t = t1; else if (t2 > 0.0f) t = t2;
                    if (t > 0.0f) lead = Vec4{d.x + tv.x * t, d.y + tv.y * t, d.z + tv.z * t, 0.0f};
                }
                // (the game's atan2_f(a, b) is atan2(b, a): its heading atan2(y, x), its pitch the elevation negated)
                wf(m + 0x48, std::atan2(lead.y, lead.x) + rf(-0.002f, 0.002f));
                wf(m + 0x44, -std::atan2(lead.z, std::sqrt(lead.x * lead.x + lead.y * lead.y)) + rf(-0.002f, 0.002f));
            }
        }
        if (ri(0, 3) == 0) {  // (aimed at a moby close by, flying straight on: a hit on a moby, its contact record now and then)
            const uint32_t target = any_moby(m);
            const Vec4 t = ee_ref<Vec4>(target + 0x10);
            const float r = rf(0.6f, 1.6f), ang = rf(-3.1f, 3.1f);
            const Vec4 p{t.x + r * std::cos(ang), t.y + r * std::sin(ang), t.z + rf(0.0f, 0.8f), 1.0f};
            put_at(m, p);
            const Vec4 d{t.x - p.x, t.y - p.y, t.z + 0.5f - p.z, 0.0f};
            wf(m + 0x48, std::atan2(d.y, d.x));
            wf(m + 0x44, -std::atan2(d.z, std::sqrt(d.x * d.x + d.y * d.y)));
            w32(pv + 0x24, 0); w32(pv + 0x28, 0);
            w32(pv + 0x18, 0);
            w32(pv + 0x14, u(ri(151, 170)));
            const float speed = rf(1.8f, 3.0f);
            wf(pv + 0x20, speed); wf(pv + 0x34, speed);
            w16(pv + 0x3c, 0);
            if (coin()) w16(target + 0x34, ee_read<uint16_t>(target + 0x34) | 0x4000u);
            if (ri(0, 3) == 0) w32(pv + 0x10, target);
        }
        w8(m + 0xbc, pass == 2 ? u(ri(1, 2)) : (ri(0, 6) == 0 ? u(ri(1, 2)) : 0u));
        w32(OV(0x0015f30c), u(ri(0, 1)));
        wf(OV(0x0015f5d0), coin() ? rf(0.0f, 0.9f) : rf(0.9f, 1.0f));
        if (coin()) camera_around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 8.0f : 40.0f);
        seed_rand();
    };
    for (int pass = 0; pass < 3; ++pass)
        classcheck("class256_update", upd, subjects, class256_update, [vary, pass](uint32_t m) { vary(m, pass); }, 40);
}

// the moby's animation time (moby_anim_time) just about `t`: its blend fraction +0x54 set over its current frame's time (the
// set +0x52 made to differ from +0x53, so the time is the frame's plus the fraction), its step +0x58 x +0x5c about one frame
void anim_about(uint32_t m, float t) {
    const uint32_t cur = ee_read<uint32_t>(m + 0x68);
    if (cur == 0) return;
    w8(m + 0x52, (ee_read<uint8_t>(m + 0x53) + 1u) & 0x7fu);
    wf(m + 0x58, 1.0f);
    wf(m + 0x5c, rf(0.5f, 1.5f));
    const float base = static_cast<float>(static_cast<int16_t>(ee_read<uint16_t>(cur + 4))) * 0.0625f;
    float f = t - base + rf(-0.3f, 1.6f);
    if (f == 0.0f) f = 0.01f;
    wf(m + 0x54, f);
}

// ---- class 638 (shared_class638.cpp) and the shot it fires (shared_class49.cpp's class49_spawn)
void class638_checks() {
    const uint32_t upd = copy_here({{16, 0x002d2cf0}, {18, 0x002dc918}});
    if (upd == 0) {
        for (const char* n : {"class638_update", "class638_hits", "class638_seek", "class638_turn", "class638_burst", "class638_aim",
                              "class638_sparks", "class638_group_timers", "class49_spawn"})
            not_here(n);
        return;
    }
    const uint32_t switches = copy_here({{16, 0x00161a74}, {18, 0x00161cdc}});
    auto subjects = [upd] { return placed_or_spawned(upd, 638); };
    // a drone in state `st`: its target (the hero's moby, another moby, one of class 0x1a3), the target's state (none: 2), its
    // path (its own, none), the path point (0 / 1), the timers at their edges, the seek speed about two frames' worth, its
    // post and hover point near it, attached or not (and to what), the animation set and its end bit, the bob's size, the
    // level's three switches, the hero's mode 0x16, the level number 18 and the scene (Veldin's leave), the hero near or far
    auto vary = [switches](uint32_t m, int st, int pass) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (st != 0 && pass != 1) after_first_update(m, class638_update, st);
        w8(m + 0x20, u(st));
        const uint32_t other = any_moby(m);
        { const int k = ri(0, 4); w32(pv + 0x110, k <= 1 ? ee_read<uint32_t>(0x001413d0) : other); if (k == 2) w16(other + 0xa6, 0x1a3); }
        w32(pv + 0x114, coin() ? 2u : 0u);
        if (ri(0, 3) == 0) w32(pv + 0x120, 0xffffffffu);  // (else its own path, as placed)
        w16(pv + 0x300, u(ri(0, 1)));
        { const int k = ri(0, 3); w16(pv + 0x2dc, k == 0 ? 0u : k == 1 ? 1u : u(ri(2, 60))); }
        w16(pv + 0x2de, u(ri(0, 3)));
        { const int k = ri(0, 3); w16(pv + 0x302, k == 0 ? 0u : k == 1 ? 1u : u(ri(2, 60))); }
        wf(pv + 0x2d4, rf(0.0f, 0.08f));
        { Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-2, 2); q.y += rf(-2, 2); q.z += rf(-1, 1); ee_ref<Vec4>(pv + 0x2b0) = q; }
        { Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-0.3f, 0.3f); q.y += rf(-0.3f, 0.3f); ee_ref<Vec4>(pv + 0x2c0) = q; }
        if (ee_read<int32_t>(pv + 0x120) >= 0 && ri(0, 3) == 0) {  // (on its path point now and then)
            const uint32_t p = ee_read<uint32_t>(OV(0x001b04b0) + ee_read<uint32_t>(pv + 0x120) * 4);
            Vec4 q = ee_ref<Vec4>(p + 0x10 + u(static_cast<int16_t>(ee_read<uint16_t>(pv + 0x300))) * 0x10);
            q.x += rf(-0.005f, 0.005f);
            ee_ref<Vec4>(pv + 0x2c0) = q;
        }
        w32(pv + 0x128, ri(0, 3) == 0 ? 1u : 0u);
        w32(pv + 0x2d0, ri(0, 3) == 0 ? 0u : other);
        wf(pv + 0x2cc, rf(-3.1f, 3.1f));
        w8(m + 0x53, u(ri(0, 3)));
        if (coin()) w8(m + 0x70, ee_read<uint8_t>(m + 0x70) ^ 2u);
        if (st == 5 && ri(0, 4) != 0) {  // (about the frames it fires at, and the end of a burst's (10))
            static const float marks[] = {2.0f, 10.0f, 10.0f, 17.0f, 32.0f};
            anim_about(m, marks[ri(0, 4)]);
            if (coin()) w16(pv + 0x2de, 2);
        }
        wf(pv + 0x2ec, rf(0.0f, 0.4f));
        for (uint32_t k = 0; k < 3; ++k) w32(switches + 4 * k, ri(0, 3) == 0 ? 0u : 1u);
        w32(pv + 0x304, u(ri(-1, 7)));
        if (ri(0, 5) == 0) w32(0x0013f350 + 0x208c, 0x16);
        if (ri(0, 5) == 0) w32(0x001413dc, 0x16);
        if (ri(0, 4) == 0) { w32(0x0015ed84, 0x12); w32(OV(0x0015f5c4), u(ri(1, 2))); w32(OV(0x0016c890), u(ri(2, 6))); }
        if (ri(0, 4) == 0) w8(0x0015edb7, u(ri(0, 1)));
        hero_around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 10.0f : 40.0f);
        if (coin()) camera_around(ee_ref<Vec4>(m + 0x10), 0.0f, 40.0f);
        w32(OV(0x0015f5cc), R() & 0xffff);
    };
    // the hits: a hit of a kind it takes (or none, or its own shot's), its health (dying now and then), the hit reaction's
    // inputs, the look-round timer and its asking, the target path, its range, the kill counter (Veldin's skill point)
    auto vary_hit = [](uint32_t m, bool own_shot) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        give_hit(m, 0x330000);
        if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
            const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
            w8(e + 0x28, u(ri(0, 8))); w8(e + 0x29, u(ri(0, 4))); w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
            w32(e + 0x20, ri(0, 3) == 0 ? 0u : (own_shot ? any_moby(m) : ee_read<uint32_t>(0x0013f350 + 0x2080)));
            if (coin()) w32(e + 0x1c, 0x45afdf66u);  // (the hit's push applied: its kind +0x28 / strength +0x29 decide the reaction)
            wf(e + 0x2c, coin() ? rf(0.1f, 3.0f) : 0.0f);
            if (own_shot && ee_read<uint32_t>(e + 0x20) != 0) w16(ee_read<uint32_t>(e + 0x20) + 0xa6, coin() ? 0x31u : 0x47u);
        }
        wf(pv + 0x20, coin() ? rf(0.1f, 2.0f) : rf(2.0f, 20.0f));
        { const int k = ri(0, 3); w32(pv + 0x2e0, k == 0 ? 0u : k == 1 ? 1u : u(ri(2, 200))); }
        w32(pv + 0x38, u(ri(0, 1)));
        { const int k = ri(0, 2); if (k == 0) w32(pv + 0x12c, 0xffffffffu); else if (k == 1 && ee_read<int32_t>(pv + 0x120) >= 0) w32(pv + 0x12c, ee_read<uint32_t>(pv + 0x120)); }  // (else its own, as placed; its path as the area now and then)
        wf(pv + 0x124, rf(5.0f, 60.0f));
        w32(pv + 0x110, ri(0, 4) == 0 ? 0u : ee_read<uint32_t>(pv + 0x110));
        w32(pv + 0x308, u(ri(0, 1)));
        w32(0x0015ee10, u(ri(7, 11)));
        w8(0x0013d425, u(ri(0, 1)));
        if (coin()) w8(0x001413f4, coin() ? 2u : u(ri(0, 4)));
    };
    for (int st = 0; st < 12; ++st)
        for (int pass = 0; pass < 3; ++pass) {
            // (B505_ST / B505_PASS: one state / pass only, while debugging)
            if (std::getenv("B505_ST") != nullptr && std::atoi(std::getenv("B505_ST")) != st) continue;
            if (std::getenv("B505_PASS") != nullptr && std::atoi(std::getenv("B505_PASS")) != pass) continue;
            classcheck("class638_update", upd, subjects, class638_update, [vary, vary_hit, st, pass](uint32_t m) {
                vary(m, st, pass);
                if (std::getenv("B505_INFO") != nullptr) std::printf("INFO m %08x pv %08x\n", m, ee_read<uint32_t>(m + 0x78));
                if (pass == 2) vary_hit(m, coin());
                seed_rand();
            }, 12);
        }
    const uint32_t hits = copy_here({{16, 0x002d38b0}, {18, 0x002dd4d8}});
    for (int pass = 0; pass < 5; ++pass)
        classcheck("class638_hits", hits, subjects, class638_hits, [vary, vary_hit, pass](uint32_t m) {
            vary(m, pass == 2 ? 9 : ri(0, 8), 1);
            vary_hit(m, pass == 1);
            if (pass == 4 && ee_read<uint8_t>(m + 0xa4) != 0xff) {  // (a knock-back (kind 5) with a little health left)
                const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                w8(e + 0x28, 5); w8(e + 0x29, u(ri(0, 3)));
                w32(e + 0x1c, 0x45afdf66u);
                wf(e + 0x2c, 0.0f);
                wf(ee_read<uint32_t>(m + 0x78) + 0x20, rf(0.05f, 0.95f));
            }
            seed_rand();
        }, 40);
    // the helpers on the drones in a random state
    auto vary_any = [vary](uint32_t m) { vary(m, ri(0, 10), ri(0, 2)); seed_rand(); };
    for (const auto& h : std::initializer_list<std::pair<const char*, std::pair<uint32_t, void (*)(uint32_t)>>>{
             {"class638_turn", {copy_here({{16, 0x002d4070}, {18, 0x002ddc98}}), class638_turn}},
             {"class638_burst", {copy_here({{16, 0x002d41f8}, {18, 0x002dde20}}), class638_burst}},
             {"class638_aim", {copy_here({{16, 0x002d4590}, {18, 0x002de1b8}}), class638_aim}},
             {"class638_sparks", {copy_here({{16, 0x002d4908}, {18, 0x002de530}}), class638_sparks}}})
        classcheck(h.first, h.second.first, subjects, h.second.second, [vary_any](uint32_t m) {
            vary_any(m);
            if (coin()) w8(m + 0x31, u(ri(0, 1)));
            if (coin()) wf(OV(0x0015f5d0), rf(0.8f, 1.0f));
            if (coin()) w32(ee_read<uint32_t>(m + 0x78) + 0x110, ee_read<uint32_t>(0x001413d0));
            wf(m + 0x48, rf(-3.1f, 3.1f));  // (any heading: the turn's sides)
            if (ri(0, 3) != 0) w32(ee_read<uint32_t>(m + 0x78) + 0x114, 0);  // (a target: the joints aimed)
            if (coin()) w8(m + 0x20, u(ri(4, 5)));  // (the gun aimed in states 4 / 5)
        }, 40);
    // the seek: towards a point near its hover point or far off, at a seek speed about the frame's
    const uint32_t seek = copy_here({{16, 0x002d3eb8}, {18, 0x002ddae0}});
    fcheck("class638_seek", seek, 40, Ret::Float, [subjects, vary_any] {
        level();
        const std::vector<uint32_t> ms = subjects();
        const uint32_t m = pick(ms);
        vary_any(m);
        const Vec4 h = ee_ref<Vec4>(ee_read<uint32_t>(m + 0x78) + 0x2c0);
        const float r = coin() ? rf(0.0f, 0.5f) : rf(0.5f, 30.0f);
        ee_ref<Vec4>(T) = Vec4{h.x + rf(-r, r), h.y + rf(-r, r), h.z + rf(-r, r), rf(-1, 1)};
        return Args{{m, T}, {}};
    }, [](A a) { vu0_sync(); return rf32(class638_seek(a.i[0], a.i[1])); });
    // the group timers: the drone's group (or none: 0xff), the members' states, a range of frames
    const uint32_t timers = copy_here({{16, 0x002d3f78}, {18, 0x002ddba0}});
    fcheck("class638_group_timers", timers, 40, Ret::None, [subjects] {
        level();
        const uint32_t m = pick(subjects());
        if (ri(0, 3) == 0) w8(m + 0x21, 0xff);
        const int lo = ri(10, 300), hi = lo + ri(0, 100);
        seed_rand();
        return Args{{m, u(lo), u(hi)}, {}};
    }, [](A a) { vu0_sync(); class638_group_timers(a.i[0], static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2])); return uint64_t{0}; });
    // the shot: an owner (a drone), its velocity and start in the test area
    const uint32_t spawn = copy_here({{16, 0x002a09a0}, {18, 0x002a7a98}});
    fcheck("class49_spawn", spawn, 40, Ret::Int32, [subjects] {
        level();
        const uint32_t m = pick(subjects());
        ee_ref<Vec4>(T) = Vec4{rf(-0.5f, 0.5f), rf(-0.5f, 0.5f), rf(-0.3f, 0.3f), rf(-1, 1)};
        const Vec4 p = ee_ref<Vec4>(m + 0x10);
        ee_ref<Vec4>(T + 0x10) = Vec4{p.x + rf(-1, 1), p.y + rf(-1, 1), p.z + rf(0, 2), 1.0f};
        seed_rand();
        return Args{{m, T, T + 0x10}, {}};
    }, [](A a) { vu0_sync(); return r32(class49_spawn(a.i[0], a.i[1], a.i[2])); });
}

// ---- class 1356 (shared_class1356.cpp) and the shot it fires (shared_class49.cpp's class50_spawn)
void class1356_checks() {
    const uint32_t upd = copy_here({{16, 0x002e22d8}, {18, 0x002f0920}});
    if (upd == 0) {
        for (const char* n : {"class1356_update", "class1356_hits", "class1356_follow", "class1356_path_headings", "class1356_joint",
                              "class1356_attach", "class1356_release", "class50_spawn"})
            not_here(n);
        return;
    }
    const uint32_t drone_fn = copy_here({{16, 0x002d2cf0}, {18, 0x002dc918}});
    auto subjects = [upd] { return placed_or_spawned(upd, 1356); };
    auto path_of = [](int32_t i) { return ee_read<uint32_t>(OV(0x001b04b0) + u(i) * 4); };
    // the carrier in state `st`: its paths (as placed, or none), its drones (as placed, gone, attached: state 10), the point
    // along its path (the start, a few points before the end, the end) and the fraction, its speed and step, its timers at
    // their edges, its tilt and rates, the hero in its start area or not, the scene, the hero's modes, its target state
    auto vary = [path_of, drone_fn](uint32_t m, int st, int pass) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (st != 0 && pass != 1) after_first_update(m, class1356_update, st);
        w8(m + 0x20, u(st));
        for (uint32_t o : {0xd0u, 0xd4u, 0xdcu}) if (ri(0, 4) == 0) w32(pv + o, 0xffffffffu);
        const std::vector<uint32_t> drones = mobys_with(drone_fn);
        for (uint32_t k = 0; k < 4; ++k) {
            const int r = ri(0, 3);
            if (r == 0) w32(pv + 0xc0 + k * 4, 0xffffffffu);
            else if (r == 1 && !drones.empty()) w32(pv + 0xc0 + k * 4, (pick(drones) - ee_read<uint32_t>(OV(0x0015ffd8))) >> 8);
            const int32_t d = ee_read<int32_t>(pv + 0xc0 + k * 4);
            if (d >= 0 && coin()) w8(ee_read<uint32_t>(OV(0x0015ffd8)) + u(d) * 0x100 + 0x20, coin() ? 10u : u(ri(0, 9)));
        }
        const int32_t in = ee_read<int32_t>(pv + (st >= 5 ? 0xdcu : 0xd8u));
        if (in >= 0) {
            const int32_t n = ee_read<int32_t>(path_of(in));
            const int k = ri(0, 3);
            w32(pv + 0x168, u(k == 0 ? 0 : k == 1 ? std::max(0, n - ri(1, 4)) : ri(0, std::max(0, n - 1))));
        }
        wf(pv + 0x16c, rf(0.0f, 0.99f));
        wf(pv + 0x170, rf(0.0f, 0.5f));
        wf(pv + 0x174, rf(0.5f, 8.0f));
        for (uint32_t o : {0x160u, 0x164u, 0x184u}) { const int k = ri(0, 3); w32(pv + o, k == 0 ? 0u : k == 1 ? 1u : u(ri(2, 90))); }
        wfrand(pv + 0x178, 5, -0.3f, 0.3f);
        wf(pv + 0x18c, rf(-3.1f, 3.1f));
        if (coin()) wf(m + 0x48, rf(-3.1f, 3.1f));
        wf(pv + 0x194, ee_read<float>(m + 0x18) + rf(-1, 1));
        if (ee_read<int32_t>(pv + 0xd4) >= 0 && coin()) {  // (the hero in its start area: at one of the area's points)
            const uint32_t a = path_of(ee_read<int32_t>(pv + 0xd4));
            const int32_t n = ee_read<int32_t>(a);
            if (n > 0) {  // (the middle of the area's points, or one of them)
                Vec4 q{};
                for (int32_t i = 0; i < n; ++i) { const Vec4 c = ee_ref<Vec4>(a + 0x10 + u(i) * 0x10); q.x += c.x / n; q.y += c.y / n; q.z += c.z / n; }
                if (coin()) q = ee_ref<Vec4>(a + 0x10 + u(ri(0, n - 1)) * 0x10);
                q.x += rf(-0.5f, 0.5f); q.y += rf(-0.5f, 0.5f); q.w = 1.0f;
                ee_ref<Vec4>(0x0013f3d0) = q;
            }
        } else {
            hero_around(ee_ref<Vec4>(m + 0x10), 0.0f, 60.0f);
        }
        if (ri(0, 3) == 0) w32(OV(0x0015f5c4), u(ri(0, 2)));
        if (ri(0, 4) == 0) w8(0x001413f4, coin() ? 2u : u(ri(0, 4)));
        if (ri(0, 5) == 0) w32(0x001413dc, 0x16);
        w32(pv + 0xb4, coin() ? 2u : 0u);
        if (ri(0, 4) == 0) w32(pv + 0xb0, 0);
        w32(pv + 0x190, u(ri(-1, 7)));
        if (coin() && ee_read<int32_t>(pv + 0x190) >= 0) {  // (its sound playing in the slot)
            const uint32_t e = 0x0013e550 + ee_read<uint32_t>(pv + 0x190) * 0x70;
            w32(e + 0x88, m);
            w8(e + 0x74, 1);
        }
        if (ri(0, 3) != 0) w32(OV(0x0015f5c4), 0);
        if (ee_read<int32_t>(pv + 0xd0) >= 0 && coin()) {  // (the hero in its target area)
            const uint32_t a = path_of(ee_read<int32_t>(pv + 0xd0));
            const int32_t n = ee_read<int32_t>(a);
            if (n > 0) {
                Vec4 q{};
                for (int32_t i = 0; i < n; ++i) { const Vec4 c = ee_ref<Vec4>(a + 0x10 + u(i) * 0x10); q.x += c.x / n; q.y += c.y / n; q.z += c.z / n; }
                q.x += rf(-0.5f, 0.5f); q.y += rf(-0.5f, 0.5f); q.w = 1.0f;
                ee_ref<Vec4>(0x0013f3d0) = q;
            }
        }
        if ((st == 2 || st == 3) && ri(0, 2) == 0) {  // (its drones gone, its out path none now and then)
            for (uint32_t k = 0; k < 4; ++k) if (k < 3 || coin()) w32(pv + 0xc0 + k * 4, 0xffffffffu);
            if (coin()) w32(pv + 0xdc, 0xffffffffu);
        }
        if (st == 3 && coin()) w32(pv + 0x184, 1);
        if ((st == 2 || st == 5) && ri(0, 2) == 0) { wf(pv + 0x16c, rf(0.9f, 0.99f)); wf(pv + 0x170, rf(1.0f, 4.0f)); wf(pv + 0x174, 1.0f); }
        if (st == 4 && coin() && ee_read<int32_t>(pv + 0xdc) >= 0) wf(m + 0x48, ee_read<float>(path_of(ee_read<int32_t>(pv + 0xdc)) + 0x1c) + rf(-0.02f, 0.02f));
        if (coin()) camera_around(ee_ref<Vec4>(m + 0x10), 0.0f, 40.0f);
        w32(OV(0x0015f5cc), R() & 0xffff);
    };
    auto vary_hit = [](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        give_hit(m, 0x330000);
        if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
            const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
            w8(e + 0x28, u(ri(0, 8))); w8(e + 0x29, u(ri(0, 4))); w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
            w32(e + 0x20, ri(0, 3) == 0 ? 0u : ee_read<uint32_t>(0x0013f350 + 0x2080));
            if (coin()) w32(e + 0x1c, 0x45afdf66u);
            wf(e + 0x2c, coin() ? rf(0.1f, 3.0f) : 0.0f);
        }
        wf(pv + 0x20, coin() ? rf(0.1f, 2.5f) : rf(2.5f, 15.0f));
        if (coin()) w8(0x001413f4, coin() ? 2u : u(ri(0, 4)));
        if (ri(0, 3) == 0 && ee_read<uint8_t>(m + 0xa4) != 0xff) {  // (Giant Clank's hit of 1 .. 2: the carrier downed at 2 or more)
            const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
            w8(0x001413f4, 2);
            wf(e + 0x2c, rf(1.0f, 2.5f));
            wf(pv + 0x20, rf(5.0f, 10.0f));
        }
    };
    // (not compared, in this check only: the w lane of the spin the downed carrier's debris get (the debris' pvars +0x2c, 0x80
    // apart from the start of the pvars): debris_spawn's port leaves it 0 where the original leaves a stack leftover, the
    // same difference as the +0xc / +0x1c lanes the batch leaves out)
    const size_t ignored = g_ignored_words.size();
    for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) g_ignored_words.push_back(a + 0x2c);
    for (int st = 0; st < 10; ++st)
        for (int pass = 0; pass < 3; ++pass)
            classcheck("class1356_update", upd, subjects, class1356_update, [vary, vary_hit, st, pass](uint32_t m) {
                vary(m, st, pass);
                if (pass == 2) vary_hit(m);
                seed_rand();
            }, 12);
    g_ignored_words.resize(ignored);
    const uint32_t hits = copy_here({{16, 0x002e2b60}, {18, 0x002f11a8}});
    for (int pass = 0; pass < 3; ++pass)
        classcheck("class1356_hits", hits, subjects, class1356_hits, [vary, vary_hit, pass](uint32_t m) {
            vary(m, pass == 2 ? 8 : ri(0, 7), 1);
            vary_hit(m);
            seed_rand();
        }, 40);
    const uint32_t joint = copy_here({{16, 0x002e3050}, {18, 0x002f1698}});
    classcheck("class1356_joint", joint, subjects, class1356_joint, [vary](uint32_t m) { vary(m, ri(0, 8), 1); seed_rand(); }, 30);
    // the path following: on its in or out path, at a point along it
    const uint32_t follow = copy_here({{16, 0x002e2e88}, {18, 0x002f14d0}});
    fcheck("class1356_follow", follow, 60, Ret::Int32, [subjects, vary, path_of] {
        level();
        const uint32_t m = pick(subjects());
        vary(m, 2, 1);
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const int32_t p = ee_read<int32_t>(pv + (coin() ? 0xd8 : 0xdc));
        seed_rand();
        return Args{{m, p >= 0 ? path_of(p) : path_of(0)}, {}};
    }, [](A a) { vu0_sync(); return ri32(class1356_follow(a.i[0], a.i[1])); });
    // the headings: each of the level's paths
    const uint32_t headings = copy_here({{16, 0x002e2fa0}, {18, 0x002f15e8}});
    fcheck("class1356_path_headings", headings, 40, Ret::None, [path_of, subjects] {
        level();
        const uint32_t pv = ee_read<uint32_t>(pick(subjects()) + 0x78);
        int32_t p = ee_read<int32_t>(pv + 0xd0 + u(ri(0, 3)) * 4);  // (one of a carrier's paths)
        if (p < 0) p = ee_read<int32_t>(pv + 0xd8);
        return Args{{path_of(p < 0 ? 0 : p)}, {}};
    }, [](A a) { vu0_sync(); class1356_path_headings(a.i[0]); return uint64_t{0}; });
    // a drone attached (its offset and turn from the test area) and let go (attached or not, a timer)
    const uint32_t attach = copy_here({{16, 0x002d3dc0}, {18, 0x002dd9e8}});
    const uint32_t release = copy_here({{16, 0x002d3de0}, {18, 0x002dda08}});
    auto a_drone = [drone_fn] {
        const std::vector<uint32_t> d = mobys_with(drone_fn);
        return d.empty() ? any_moby(0) : pick(d);
    };
    fcheck("class1356_attach", attach, 30, Ret::None, [subjects, a_drone] {
        level();
        wfrand(T, 4, -5.0f, 5.0f);
        wfrand(T + 0x10, 4, -3.0f, 3.0f);
        return Args{{a_drone(), pick(subjects()), T, T + 0x10}, {}};
    }, [](A a) { vu0_sync(); class1356_attach(a.i[0], a.i[1], a.i[2], a.i[3]); return uint64_t{0}; });
    fcheck("class1356_release", release, 40, Ret::None, [a_drone] {
        level();
        const uint32_t d = a_drone();
        w8(d + 0x20, coin() ? 10u : u(ri(0, 9)));
        w8(d + 0x53, u(ri(0, 2)));
        seed_rand();
        return Args{{d, u(ri(0, 200))}, {}};
    }, [](A a) { vu0_sync(); class1356_release(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
    // the shot: an owner (a carrier), its velocity, start and target point in the test area, its life
    const uint32_t spawn = copy_here({{16, 0x002a0dc0}, {18, 0x002a7eb8}});
    fcheck("class50_spawn", spawn, 40, Ret::Int32, [subjects] {
        level();
        const uint32_t m = pick(subjects());
        ee_ref<Vec4>(T) = Vec4{rf(-0.5f, 0.5f), rf(-0.5f, 0.5f), rf(-0.3f, 0.3f), rf(-1, 1)};
        const Vec4 p = ee_ref<Vec4>(m + 0x10);
        ee_ref<Vec4>(T + 0x10) = Vec4{p.x + rf(-1, 1), p.y + rf(-1, 1), p.z + rf(0, 2), 1.0f};
        ee_ref<Vec4>(T + 0x20) = ee_ref<Vec4>(0x0013f3d0);
        seed_rand();
        return Args{{m, T, T + 0x10, T + 0x20, u(ri(0, 100))}, {}};
    }, [](A a) { vu0_sync(); return r32(class50_spawn(a.i[0], a.i[1], a.i[2], a.i[3], static_cast<int32_t>(a.i[4]))); });
}

// ---- class 1523 (shared_class1523.cpp)
void class1523_checks() {
    const uint32_t upd = copy_here({{15, 0x0029edb0}, {18, 0x002abaa0}});
    if (upd == 0) {
        for (const char* n : {"class1523_update", "class1523_streak"}) not_here(n);
        return;
    }
    auto subjects = [upd] { return placed_or_spawned(upd, 1523); };
    // the shot: charging (the button held or not, as Giant Clank or not: the hero moby's byte +0x24), its frames charged (none,
    // a few, about 50), its light region (none, one of the first), the frame counter's low bits, the charge light's alpha and
    // blend word; or fired: its frames flown about 60, its velocity, near the level's box edges now and then, its light about
    // to fade out, the shot moby word; other states; the hero's heading
    auto vary = [](uint32_t m, int st) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, u(st));
        w16(pv + 0x2e, ri(0, 3) == 0 ? 0u : 1u);
        w8(0x001413f4, ri(0, 4) == 0 ? u(ri(0, 4)) : 2u);
        { const int k = ri(0, 3); w32(pv + 0x10, k == 0 ? 0u : k == 1 ? u(ri(1, 30)) : u(ri(45, 60))); }
        { const int k = ri(0, 3); w32(pv + 0x14, k == 0 ? 0u : k == 1 ? u(ri(1, 50)) : u(ri(55, 65))); }
        const int16_t r = static_cast<int16_t>(ri(0, 2) == 0 ? -1 : ri(0, 3));
        w16(pv + 0x2c, static_cast<uint16_t>(r));
        if (r >= 0) {
            const uint32_t e = OV(0x001802c0) + u(r) * 0x20;
            wf(e, rf(0.0f, 2.0f)); wf(e + 4, rf(0.0f, 2.0f));
            wf(e + 0x1c, coin() ? rf(0.00005f, 0.0002f) : rf(0.1f, 7.0f));
        }
        w32(OV(0x0015f5cc), R());
        if (coin()) w8(OV(0x0015f334) + 3, coin() ? 0xffu : u(ri(0, 0x30)));
        if (coin()) { w32(OV(0x0015f338), 0); w32(OV(0x0015f33c), 0); }
        w32(OV(0x001613c8), coin() ? m : any_moby(m));
        wf(pv, rf(-1.0f, 1.0f)); wf(pv + 4, rf(-1.0f, 1.0f)); wf(pv + 8, rf(-0.2f, 0.2f));
        if (ri(0, 4) == 0) wf(m + 0x10 + 4 * u(ri(0, 2)), coin() ? rf(1.0f, 2.5f) : rf(1020.0f, 1022.0f));
        wf(0x0013f350 + 0x98, rf(-3.1f, 3.1f));
        if (coin()) camera_around(ee_ref<Vec4>(m + 0x10), 0.0f, 30.0f);
        seed_rand();
    };
    for (int st = 0; st < 3; ++st)
        for (int pass = 0; pass < 2; ++pass)
            classcheck("class1523_update", upd, subjects, class1523_update, [vary, st](uint32_t m) { vary(m, st); }, st == 2 ? 10 : 40);
    // the streak: a colour, a point about the shot
    const uint32_t streak = copy_here({{15, 0x0025e2c0}, {18, 0x00270af0}});
    fcheck("class1523_streak", streak, 40, Ret::None, [subjects] {
        level();
        const uint32_t m = pick(subjects());
        const Vec4 p = ee_ref<Vec4>(m + 0x10);
        ee_ref<Vec4>(T) = Vec4{p.x + rf(-40, 40), p.y + rf(-40, 40), p.z + rf(-40, 40), rf(-1, 1)};
        seed_rand();
        return Args{{R(), T, m}, {}};
    }, [](A a) { vu0_sync(); class1523_streak(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
}

// ---- class 123 (shared_class123.cpp)
void class123_checks() {
    const uint32_t upd = copy_here({{15, 0x002a6e68}, {17, 0x002ac740}});
    if (upd == 0) { not_here("class123_update"); return; }
    const uint32_t tune = copy_here({{15, 0x00161558}, {17, 0x001614f0}});
    auto subjects = [upd] { return placed_or_spawned(upd, 123); };
    // the emitter in each state: its motion (0 .. 4), radius, phase, period, plane heading; its post near it; the level's spark
    // count (none, a few), its frame turned at random; a moby across its beam now and then
    for (int st = 0; st < 5; ++st)
        for (int pass = 0; pass < 2; ++pass)
            classcheck("class123_update", upd, subjects, class123_update, [st, pass, tune](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                if (pass == 1) {
                    w32(pv + 0x20, u(ri(0, 4)));
                    wf(pv + 0x24, rf(0.5f, 6.0f));
                    wf(pv + 0x28, rf(-3.1f, 3.1f));
                    wf(pv + 0x30, rf(0.5f, 8.0f));
                    wf(pv + 0x2c, rf(-3.1f, 3.1f));
                    Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-3, 3); q.y += rf(-3, 3); q.z += rf(-2, 2); ee_ref<Vec4>(pv) = q;
                    Mat4 r{};
                    euler_to_mat4_fpu(r, Vec4{rf(-3, 3), rf(-3, 3), rf(-3, 3), 0.0f});
                    for (uint32_t k = 0; k < 3; ++k) ee_ref<Vec4>(m + 0xc0 + k * 0x10) = r.r[k];
                }
                w8(m + 0x20, u(st));
                // (a radius of 0 makes the phase's step infinite: the original's angle_add then wraps for ever, as batch 260 has it)
                if (ee_read<float>(pv + 0x24) == 0.0f) wf(pv + 0x24, rf(0.5f, 4.0f));
                if (ri(0, 3) == 0) w32(tune, u(ri(0, 4)));
                if (coin()) {  // (the emitter put so its beam (12 along its frame's z) runs through a moby of the level)
                    const Vec4 z{ee_read<float>(m + 0xe0), ee_read<float>(m + 0xe4), ee_read<float>(m + 0xe8), 0.0f};
                    const uint32_t o = any_moby(m);
                    const Vec4 q = ee_ref<Vec4>(o + 0x10);
                    const float t = rf(1.0f, 11.0f);
                    if (st == 0 || st == 4) put_at(m, Vec4{q.x - z.x * t, q.y - z.y * t, q.z + 0.5f - z.z * t, 1.0f});
                    if (coin()) w16(o + 0x34, ee_read<uint16_t>(o + 0x34) | 0x4000u);
                }
                if (coin()) hero_around(ee_ref<Vec4>(m + 0x10), 0.0f, 12.0f);
                seed_rand();
            }, 30);
}

// ---- class 1443 / 1890 (shared_class1443.cpp)
void class1443_checks() {
    const uint32_t upd = copy_here({{16, 0x002e5708}, {18, 0x002fae48}});
    if (upd == 0) {
        for (const char* n : {"class1443_update", "class1443_draw", "class1443_group_all_hit", "class1443_group_sound"}) not_here(n);
        return;
    }
    const uint32_t tune = copy_here({{16, 0x00161e00}, {18, 0x00162588}});
    const uint32_t cls = g_oracle_level == 18 ? 1890u : 1443u;
    auto subjects = [upd, cls] { return placed_or_spawned(upd, cls); };
    // a group of the level's (a moby's own, or the subject's) and its members' +0xbc flags: all set, all but one, none
    auto a_group = [](uint32_t m) {
        int32_t g = ee_read<int32_t>(ee_read<uint32_t>(m + 0x78) + 0x20);
        if (g < 0 || ri(0, 3) == 0) {
            const uint32_t o = any_moby(m);
            g = ee_read<uint8_t>(o + 0x21) != 0xff ? ee_read<uint8_t>(o + 0x21) : 0;
        }
        return g;
    };
    auto set_flags = [](int32_t g) {
        uint32_t e = ee_read<uint32_t>(OV(0x001ab840) + u(g) * 4);
        if (e == 0) return;
        const int k = ri(0, 2);
        int i = 0;
        for (;; e += 2, ++i) {
            const uint32_t mm = ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
            w8(mm + 0xbc, k == 0 ? 1u : k == 1 ? (i == 0 ? 0u : 1u) : 0u);
            if (coin()) w16(mm + 0xa6, coin() ? 0x5a3u : ee_read<uint16_t>(mm + 0xa6));
            if (static_cast<int16_t>(ee_read<uint16_t>(e)) < 0) break;
        }
    };
    for (int st = 0; st < 4; ++st)
        classcheck("class1443_update", upd, subjects, class1443_update, [st, a_group, set_flags](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(st));
            wf(pv + 0xc, rf(0.5f, 1.2f));
            const int32_t g = ri(0, 3) == 0 ? -1 : a_group(m);
            w32(pv + 0x20, u(g));
            if (g >= 0) set_flags(g);
            w32(pv + 0x24, u(ri(-1, 7)));
            w8(m + 0x31, u(ri(0, 1)));
            hero_around(ee_ref<Vec4>(m + 0x10), 0.0f, 30.0f);
            seed_rand();
        }, 30);
    const uint32_t all_hit = copy_here({{16, 0x002e5cd0}, {18, 0x002fb410}});
    fcheck("class1443_group_all_hit", all_hit, 40, Ret::Int32, [subjects, a_group, set_flags] {
        level();
        const int32_t g = a_group(pick(subjects()));
        set_flags(g);
        return Args{{u(g)}, {}};
    }, [](A a) { vu0_sync(); return ri32(class1443_group_all_hit(static_cast<int32_t>(a.i[0]))); });
    const uint32_t sound = copy_here({{16, 0x002e5d30}, {18, 0x002fb470}});
    fcheck("class1443_group_sound", sound, 40, Ret::None, [subjects, a_group, set_flags] {
        level();
        const uint32_t m = pick(subjects());
        const int32_t g = a_group(m);
        set_flags(g);
        hero_around(ee_ref<Vec4>(m + 0x10), 0.0f, 40.0f);
        seed_rand();
        return Args{{u(g)}, {}};
    }, [](A a) { vu0_sync(); class1443_group_sound(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    // the draw: its colour cycle (below 0, above 1, between), phase and rings' angles, the count of cycling quads, its size;
    // placed where its quads reach the packets, the camera lifted now and then
    const uint32_t draw = copy_here({{16, 0x002e58d8}, {18, 0x002fb018}});
    classcheck("class1443_draw", draw, subjects, class1443_draw, [tune](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        { const int k = ri(0, 3); wf(pv + 0xc, k == 0 ? rf(-2.5f, -0.01f) : k == 1 ? rf(1.01f, 3.0f) : rf(0.0f, 1.0f)); }
        wfrand(pv + 8, 1, -3.1f, 3.1f);
        wfrand(pv + 0x10, 4, -3.1f, 3.1f);
        if (ri(0, 3) == 0) w32(tune + 0x40, u(ri(1, 6)));
        if (coin()) wf(m + 0x2c, ee_read<float>(ee_read<uint32_t>(m + 0x24) + 0x24) * rf(0.5f, 2.0f));
        packet_cursor();
        place_drawn(class1443_draw, m, [m](const Vec4& q) { put_at(m, q); });
        camera_lift();
        draw_info("class1443_draw", m, class1443_draw);
        seed_rand();
    }, 40);
}

}  // namespace

void batch_505() {
    if (g_only_batch != 505) return;
    g_batch = 505;
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

    class49_checks();
    class256_checks();
    class638_checks();
    class1356_checks();
    class1523_checks();
    class123_checks();
    class1443_checks();

    //@@NEXT
    g_pre_call = nullptr;
    g_extra_noop_hooks.clear();
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
