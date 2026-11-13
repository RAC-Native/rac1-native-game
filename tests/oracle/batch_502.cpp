// Oracle batch 502 (stage 4 task G2, #149): hand-written ports of one level made to serve every level that carries a copy of
// the same code, each against every level's own copy at that level's own address (the copies stay transliterated in their
// units until each level is re-glued; here every copy is the original and the one port the port):
//  - novalis_camera.cpp: camera type 3, the path camera (reset, init, update, exit; levels 1, 5, 8, 16, 17), on a camera of
//    the test area whose record (the level's first) names data of the test area: the level's own paths as the camera path,
//    waypoint lists and rails of the test area (the rail table 0x15f70c pointed at them), every hand-back kind (0, 2, 3,
//    others), the hero on a rail or not;
//  - kerwan_camera.cpp: camera type 1 (accept, init, update, exit and the init / update's callees; levels 3, 4) on the level's
//    live type-1 cameras (batch 126's setup); the follow camera's look-height hook and its callback, the callback queue (every
//    level but 0, 1 and 8), the distance rates 0x2e6f80's copies (levels 2, 3, 6, 7, 8, 12, 15, 18), the turn toward a
//    direction 0x24e6f8 (levels 3, 4);
//  - novalis_props.cpp: class 605, the zone marker (levels 1 .. 18), on the level's own markers (or live mobys given a
//    marker's pvars when the level has none placed);
//  - novalis_teleporter.cpp: the teleporter pad's pieces (levels 1, 7, 8, 11, 12, 13, 16, 17, 18) and the moby group's state
//    (levels 1, 5, 6, 7, 8, 11, 12, 13, 15, 16, 17, 18);
//  - novalis_hooks.cpp: the Peckbot's hook +8 (levels 1, 13) and the Amoeboid's hook +0 (levels 1, 11), on the mobys whose
//    class's hook table names the level's copy;
//  - novalis_platforms.cpp: whether the hero is on a moby (levels 1, 3, 7, 9, 10, 12, 13, 14: thirteen copies);
//  - kerwan_props.cpp: class 915 (levels 3, 10).
// Not checked here: the copies of the 7-word wrappers the plan grouped with camera type 1 (L11:0x3162d0, L11:0x3191a0,
// L16:0x2cb600, L16:0x2d0038, L18:0x2d6230): the same words, but they call other functions (a carry hook, moby_retire,
// moby_anim_set), not camera type 1's; they are not this port.
// No RAM dumps: states built from the level after its first gameplay frame.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 502, N = 1 .. 18; a family the level does not carry says so.

#include "ovl_oracle.h"

#include <game/ovl/carry_hooks.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/novalis_camera.h>
#include <game/ovl/novalis_hooks.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
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

// the level after its first gameplay frame (the port's), as batch 506
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
        g_snap.assign(ram, ram + 0x02000000);
        g_snap_level = g_oracle_level;
    } else {
        std::memcpy(ram, g_snap.data(), g_snap.size());
    }
}

constexpr uint32_t P = 0x0013f350;  // the hero (+0x80 his position)

uint32_t first_moby() { return ee_read<uint32_t>(OV(0x0015ffd8)); }

// the level's live mobys whose update function is `fn` (the level's address)
std::vector<uint32_t> mobys_with(uint32_t fn) {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
    return v;
}

// the level's live mobys (with a class)
std::vector<uint32_t> live_mobys() {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x24) != 0) v.push_back(m);
    return v;
}

uint32_t pick(const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; }

// a point `r0`..`r1` (horizontally) about `at`
Vec4 around(const Vec4& at, float r0, float r1, float z0, float z1) {
    const float r = rf(r0, r1), a = rf(-3.1f, 3.1f);
    return Vec4{at.x + r * std::cos(a), at.y + r * std::sin(a), at.z + rf(z0, z1), 1.0f};
}

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

// An update on the subjects `ms` (dealt again when the port's run throws, as batch 506's classcheck)
void classcheck(const char* name, uint32_t laddr, const std::function<std::vector<uint32_t>()>& subjects,
                const std::function<void(uint32_t)>& port, const std::function<void(uint32_t)>& vary, int n) {
    if (!selected(name)) return;
    level();
    const std::vector<uint32_t> probe = subjects();
    if (probe.empty()) { std::printf("%-26s L%d:0x%08x  no subject on this level\n", name, g_oracle_level, laddr); return; }
    std::printf("  (%s: %zu subjects)\n", name, probe.size());
    fcheck(name, laddr, n, Ret::None, [subjects, vary, port] {
        uint32_t m = 0;
        for (int attempt = 0; attempt < 40; ++attempt) {
            level();
            m = pick(subjects());
            vary(m);
            if (trial_ok([port, m] { port(m); })) break;
        }
        return Args{{m}, {}};
    }, [port](A a) { vu0_sync(); port(a.i[0]); return uint64_t{0}; });
}

// the level's paths (0x1b04b0: 2..64 points) by index
std::vector<uint32_t> level_paths() {
    std::vector<uint32_t> v;
    for (uint32_t i = 0; i < 64; ++i) {
        const uint32_t p = ee_read<uint32_t>(OV(0x001b04b0) + i * 4);
        if (p >= 0x00100000u && p < 0x02000000u && ee_read<int32_t>(p) >= 2 && ee_read<int32_t>(p) <= 64) v.push_back(i);
    }
    return v;
}
uint32_t path_ptr(uint32_t idx) { return ee_read<uint32_t>(OV(0x001b04b0) + idx * 4); }
Vec4 path_point(uint32_t path, int32_t i) { return ee_ref<Vec4>(path + 0x10 + u(i) * 0x10); }
// the path's w lanes: each point's distance to the next (the last to the first), as the cameras' inits measure them
void path_lengths(uint32_t path) {
    const int32_t n = ee_read<int32_t>(path);
    for (int32_t i = 0; i < n; ++i) {
        const Vec4 a = path_point(path, i), b = path_point(path, (i + 1) % n);
        wf(path + 0x1c + u(i) * 0x10, std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y) + (b.z - a.z) * (b.z - a.z)));
    }
}

// a camera's rows (+0 forward, +0x10 side, +0x20 up, +0x30 its place, +0x40 the forward again) at `at`, looking about the
// hero with some yaw and pitch off
void set_view(uint32_t cam, const Vec4& at) {
    const Vec4 h = ee_ref<Vec4>(P + 0x80);
    float fx = h.x - at.x + rf(-2, 2), fy = h.y - at.y + rf(-2, 2), fz = h.z - at.z + rf(-1, 1);
    float l = std::sqrt(fx * fx + fy * fy + fz * fz);
    if (!(l > 0.001f)) { fx = 1.0f; fy = 0.0f; fz = 0.0f; l = 1.0f; }
    fx /= l; fy /= l; fz /= l;
    float sx = fy, sy = -fx;  // (forward x up: the side)
    const float sl = std::sqrt(sx * sx + sy * sy);
    if (sl > 0.001f) { sx /= sl; sy /= sl; } else { sx = 1.0f; sy = 0.0f; }
    const Vec4 f{fx, fy, fz, 0.0f}, sd{sx, sy, 0.0f, 0.0f}, up{sy * fz, -sx * fz, sx * fy - sy * fx, 0.0f};
    ee_ref<Vec4>(cam) = f;
    ee_ref<Vec4>(cam + 0x10) = sd;
    ee_ref<Vec4>(cam + 0x20) = up;
    ee_ref<Vec4>(cam + 0x30) = at;
    ee_ref<Vec4>(cam + 0x40) = f;
}

// the live cameras (48 records 0x167150, 0xa0 each, live while 0x169890's word is set)
uint32_t live_cam(const std::function<bool(uint32_t)>& pred) {
    for (uint32_t i = 0; i < 48; ++i) {
        const uint32_t c = OV(0x00167150) + i * 0xa0;
        if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0 && pred(c)) return c;
    }
    return 0;
}

// ---- camera type 3 (novalis_camera.cpp)
void cam3_checks() {
    const uint32_t reset = copy_here({{1, 0x00314e98}, {5, 0x00325340}, {8, 0x00311890}, {16, 0x002efe48}, {17, 0x002fc2c8}});
    const uint32_t init = copy_here({{1, 0x00315de0}, {5, 0x00326288}, {8, 0x003127d8}, {16, 0x002f0d90}, {17, 0x002fd210}});
    const uint32_t upd = copy_here({{1, 0x00315358}, {5, 0x00325800}, {8, 0x00311d50}, {16, 0x002f0308}, {17, 0x002fc788}});
    const uint32_t ext = copy_here({{1, 0x00316030}, {5, 0x003264d8}, {8, 0x00312a28}, {16, 0x002f0fe0}, {17, 0x002fd460}});
    if (upd == 0) {
        for (const char* n : {"cam_type3_reset", "cam_type3_init", "cam_type3_update", "cam_type3_exit"}) not_here(n);
        return;
    }
    level();
    const std::vector<uint32_t> paths = level_paths();
    std::printf("  (cam_type3: %zu paths)\n", paths.size());
    if (paths.empty()) return;
    // the camera CAM (the level's camera record 0, its data D), its work block ST; two waypoint lists WP0 / WP1 (in two of the
    // level's path slots), a rail table RT of four rails (the level's paths), the hero on a rail
    constexpr uint32_t CAM = TEST_AREA + 0x2000, ST = TEST_AREA + 0x2100, D = TEST_AREA + 0x2300, RT = TEST_AREA + 0x2380;
    constexpr uint32_t WP0 = TEST_AREA + 0x2400, WP1 = TEST_AREA + 0x2500;
    // `kind`: the hand-back kind (data +0x30; -1: dealt); `steps`: the port's init (and updates) run before
    auto setup = [paths](int kind, int steps) {
        level();
        for (uint32_t o = 0; o < 0x600; o += 4) w32(CAM + o, 0);
        w32(ee_read<uint32_t>(OV(0x0015ef50)) + 0x1c, D);
        w16(CAM + 0x84, 0);
        w16(CAM + 0x86, 3);
        w32(CAM + 0x70, ST);
        w32(CAM + 0x74, 4);
        // the camera path and the rails (their lengths measured), the waypoint lists along them
        const uint32_t pc = pick(paths);
        uint32_t s0 = pick(paths), s1 = pick(paths);
        for (int k = 0; k < 8 && paths.size() > 2 && (s0 == pc || s1 == pc || s0 == s1); ++k) { s0 = pick(paths); s1 = pick(paths); }
        const uint32_t cam_path = path_ptr(pc);
        uint32_t rails[4];
        for (uint32_t k = 0; k < 4; ++k) {
            rails[k] = path_ptr(coin() ? pc : pick(paths));
            path_lengths(rails[k]);
            for (uint32_t o = 0; o < 0x20; o += 4) w32(RT + k * 0x20 + o, R());
            w32(RT + k * 0x20 + 0x10, rails[k]);
        }
        w32(OV(0x0015f70c), RT);
        static const int kinds[] = {0, 0, 2, 2, 2, 3, 3, 1, 4};
        const int knd = kind >= 0 ? kind : kinds[ri(0, 8)];
        // (the rail entry: none, -1, only off kind 2, whose waypoint lists map onto it: the original divides by its count)
        const int32_t rk = knd != 2 && ri(0, 3) == 0 ? -1 : ri(0, 3);
        const uint32_t rail = rk >= 0 && ri(0, 3) != 0 ? rails[rk] : rails[ri(0, 3)];
        auto waypoints = [](uint32_t wp, uint32_t along, int32_t n) {
            const int32_t na = ee_read<int32_t>(along);
            w32(wp, u(n));
            for (int32_t i = 0; i < n; ++i) {
                const Vec4 q = path_point(along, std::min(na - 1, i * na / n + ri(0, 1)));
                ee_ref<Vec4>(wp + 0x10 + u(i) * 0x10) = Vec4{q.x + rf(-1, 1), q.y + rf(-1, 1), q.z + rf(-0.5f, 0.5f), ri(0, 2) == 0 ? 0.0f : rf(0.2f, 3.0f)};
            }
        };
        const int32_t nwp = ri(1, 5);  // (the two lists: as many waypoints each)
        waypoints(WP0, cam_path, nwp);
        waypoints(WP1, rk >= 0 ? rails[rk] : rail, nwp);
        w32(OV(0x001b04b0) + s0 * 4, WP0);
        w32(OV(0x001b04b0) + s1 * 4, WP1);
        // the data
        for (uint32_t o = 0; o < 0x40; o += 4) w32(D + o, 0);
        wf(D, ri(0, 3) == 0 ? 0.0f : rf(-2.0f, 12.0f));
        w32(D + 0x20, pc);
        w32(D + 0x24, u(rk));
        w32(D + 0x28, s0);
        w32(D + 0x2c, s1);
        w32(D + 0x30, u(knd));
        w16(D + 0x34, u(ri(0, 4) == 0 ? 1 : 0));
        w16(D + 0x36, u(ri(0, 2) == 0 ? 1 : 0));
        wf(D + 0x38, ri(0, 4) == 0 ? 0.0f : rf(-2.0f, 6.0f));
        // the hero: on the rail (its segment and offset), near a point of the camera path or of the rail
        const int32_t nr = ee_read<int32_t>(rail);
        const int32_t seg = ri(0, nr - 1);
        w32(P + 0x560, ri(0, 6) == 0 ? rails[ri(0, 3)] : rail);
        w32(P + 0x564, u(seg));
        wf(P + 0x568, rf(0.0f, std::max(0.01f, ee_read<float>(rail + 0x1c + u(seg) * 0x10))));
        w32(P + 0x570, u(ri(0, 3) == 0 ? 1 : 0));
        const uint32_t near = coin() ? cam_path : rail;
        ee_ref<Vec4>(P + 0x80) = around(path_point(near, ri(0, ee_read<int32_t>(near) - 1)), 0.0f, coin() ? 2.0f : 12.0f, -0.5f, 2.0f);
        w32(0x001415d4, coin() ? 3u : u(ri(0, 0x50)));  // (the hero's camera request: this camera's type or another)
        if (ri(0, 5) == 0) w32(0x0015ed84, 0xe);  // (the level number: 14 runs the kind-0 look without the wrap)
        // the previous camera (the init takes its matrix and place: after the first frame alone its rows are 0, and a camera
        // with no direction never turns) given a view of the hero, and the camera the same
        const uint32_t prev = ee_read<uint32_t>(OV(0x00166e04));
        set_view(prev, around(ee_ref<Vec4>(P + 0x80), 3.0f, 12.0f, 1.0f, 5.0f));
        for (uint32_t r = 0; r < 5; ++r) ee_ref<Vec4>(CAM + r * 0x10) = ee_ref<Vec4>(prev + (r < 4 ? r * 0x10 : 0u));
        for (int s = 0; s < steps; ++s) {
            if (s == 0) cam_type3_init(CAM);
            else cam_type3_update(CAM);
            Vec4 q = ee_ref<Vec4>(P + 0x80);
            q.x += rf(-0.6f, 0.6f); q.y += rf(-0.6f, 0.6f); q.z += rf(-0.3f, 0.1f);
            ee_ref<Vec4>(P + 0x80) = q;
            if (coin()) {  // (the hero moving along his rail)
                const int32_t sg = ee_read<int32_t>(P + 0x564) + ri(0, 1);
                w32(P + 0x564, u(sg % std::max(1, nr)));
                wf(P + 0x568, rf(0.0f, 2.0f));
            }
        }
        seed_rand();
    };
    // the reset and the init on every kind (kind 2 maps the waypoint lists: data +0x34 0)
    for (int pass = 0; pass < 3; ++pass)
        fcheck("cam_type3_reset", reset, 40, Ret::None, [setup, pass] {
            setup(pass == 0 ? 2 : -1, 0);
            if (pass == 0) w16(D + 0x34, 0);
            return Args{{CAM}, {}};
        }, [](A a) { vu0_sync(); cam_type3_reset(a.i[0]); return uint64_t{0}; });
    for (int pass = 0; pass < 3; ++pass)
        fcheck("cam_type3_init", init, 50, Ret::None, [setup, pass] {
            setup(pass == 0 ? 3 : (pass == 1 ? 2 : -1), 0);
            return Args{{CAM}, {}};
        }, [](A a) { vu0_sync(); cam_type3_init(a.i[0]); return uint64_t{0}; });
    // the update after the init and 0..12 updates, each kind. Kind 2 only on waypoint lists the init mapped into spans of a
    // length (a span of no length divides by zero: the original then walks the path without end, which no level's data
    // does). (The w lanes of the camera's side and up rows are VCROSS's stale lane in the original, as vcross_lanes says
    // of the live cameras: left out, as batch 506's camera type 19.)
    const size_t n_ign = g_ignored_words.size();
    g_ignored_words.push_back(CAM + 0x1c);
    g_ignored_words.push_back(CAM + 0x2c);
    // (and the camera path's span not far longer than the rail's: the step along the camera path is their ratio times the
    // hero's way along the rail)
    auto spans_ok = [] {
        for (uint32_t wp : {WP0, WP1})
            for (int32_t i = 0; i < ee_read<int32_t>(wp); ++i) {
                const float len = ee_read<float>(wp + 0x14 + u(i) * 0x10);
                if (!(len > 0.01f && len < 1.0e4f)) return false;
            }
        for (int32_t i = 0; i < std::min(ee_read<int32_t>(WP0), ee_read<int32_t>(WP1)); ++i)
            if (!(ee_read<float>(WP0 + 0x14 + u(i) * 0x10) < 50.0f * ee_read<float>(WP1 + 0x14 + u(i) * 0x10))) return false;
        return true;
    };
    for (int kind : {0, 2, 3, 1, -1})
        fcheck("cam_type3_update", upd, 50, Ret::None, [setup, kind, spans_ok] {
            for (int attempt = 0; attempt < 60; ++attempt) {
                setup(kind, ri(1, 13));
                if (ee_read<int32_t>(D + 0x30) != 2 || spans_ok()) break;
            }
            if (ee_read<int32_t>(D + 0x30) != 2 && ri(0, 2) == 0) {
                // (the look along the camera path, no rail entry: the hero near its last point, the look far ahead, past the
                // path's end, which level 14's kind 0 does not wrap)
                const uint32_t path = path_ptr(ee_read<uint32_t>(D + 0x20));
                w32(D + 0x24, 0xffffffffu);
                ee_ref<Vec4>(P + 0x80) = around(path_point(path, ee_read<int32_t>(path) - 1), 0.0f, 1.0f, 0.0f, 1.0f);
                wf(D, rf(5.0f, 40.0f));
                if (coin()) w32(0x0015ed84, 0xe);
            }
            return Args{{CAM}, {}};
        }, [](A a) { vu0_sync(); cam_type3_update(a.i[0]); return uint64_t{0}; });
    fcheck("cam_type3_exit", ext, 80, Ret::None, [setup] {
        setup(-1, ri(1, 4));
        if (coin()) {  // (the hero's rail the camera's: the hand-back held while he rides it forward)
            const int32_t rk = ee_read<int32_t>(D + 0x24);
            if (rk >= 0) w32(P + 0x560, ee_read<uint32_t>(RT + u(rk) * 0x20 + 0x10));
        }
        return Args{{CAM}, {}};
    }, [](A a) { vu0_sync(); cam_type3_exit(a.i[0]); return uint64_t{0}; });
    g_ignored_words.resize(n_ign);
}

// ---- camera type 1 (kerwan_camera.cpp)
void cam1_checks() {
    const uint32_t acc = copy_here({{3, 0x002e8870}, {4, 0x002ed850}});
    const uint32_t init = copy_here({{3, 0x002e8940}, {4, 0x002ed920}});
    const uint32_t init2 = copy_here({{3, 0x002e8628}, {4, 0x002ed608}});
    const uint32_t upd = copy_here({{3, 0x002e8960}, {4, 0x002ed940}});
    const uint32_t upd2 = copy_here({{3, 0x002e8730}, {4, 0x002ed710}});
    const uint32_t ext = copy_here({{3, 0x002e8980}, {4, 0x002ed960}});
    if (acc == 0) {
        for (const char* n : {"cam_type1_accept", "cam_type1_init", "cam_type1_update", "cam_type1_exit"}) not_here(n);
        return;
    }
    level();
    if (live_cam([](uint32_t c) { return ee_read<int16_t>(c + 0x86) == 1; }) == 0) {
        std::printf("cam_type1                  no live camera of type 1 on level %d\n", g_oracle_level);
        return;
    }
    // a live camera of the type (batch 126's setup): the hero in or near its box, priorities / the current camera's request
    // varied; for init / update / exit made the active camera (the work block 0x169110), after 0..10 earlier frames
    auto cam_setup = [](uint32_t* other, int steps) -> uint32_t {
        level();
        std::vector<uint32_t> cams;
        for (uint32_t i = 0; i < 48; ++i) {
            const uint32_t c = OV(0x00167150) + i * 0xa0;
            if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0 && ee_read<int16_t>(c + 0x86) == 1) cams.push_back(c);
        }
        const uint32_t cam = pick(cams);
        const uint32_t e = ee_read<uint32_t>(OV(0x0015ef50)) + u(ee_read<int16_t>(cam + 0x84) * 0x20);
        const uint32_t d = ee_read<uint32_t>(e + 0x1c);
        Vec4 at = ee_ref<Vec4>(P + 0x80);
        if (ri(0, 3) != 0 && d != 0) {
            const int32_t idx = ee_read<int32_t>(d + 0xc);
            if (idx >= 0) at = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(idx) * 0x80 + 0x30);
        } else if (ri(0, 2) == 0) {
            at = ee_ref<Vec4>(e);
        }
        at.x += rf(-3, 3); at.y += rf(-3, 3); at.z += rf(-1, 1);
        ee_ref<Vec4>(P + 0x80) = at;
        if (ri(0, 2) == 0) ee_ref<Vec4>(OV(0x00166dc0)) = at;
        w8(OV(0x00166ef3), R() & 0xff);
        wf(OV(0x00166f08), rf(0.0f, 1.0f));
        wf(OV(0x00166f14), rf(0.0f, 1.0f));
        if (ri(0, 3) == 0) w8(cam + 0x7c, u(ri(0, 7)));
        if (ri(0, 4) == 0) w32(cam + 0x74, u(ri(0, 5)));
        if (other != nullptr) {
            *other = ri(0, 3) == 0 ? 0u : ee_read<uint32_t>(OV(0x00166e00));
            if (*other != 0) { w16(*other + 0x7e, u(ri(0, 2) == 0 ? ri(1, 5) : 0)); w8(*other + 0x7c, u(ri(0, 7))); }
            return cam;
        }
        w32(OV(0x00166e00), cam);
        w32(cam + 0x70, OV(0x00169110));
        w16(cam + 0x7e, 0);
        if (ri(0, 3) == 0) { wf(d + 0x18, rf(-1.0f, 3.0f)); wfrand(e + 0x10, 3, -3.1f, 3.1f); }
        for (int s = 0; s < steps; ++s) {
            if (s == 0) cam_type1_init(cam);
            else cam_type1_update(cam);
            Vec4 q = ee_ref<Vec4>(P + 0x80);
            q.x += rf(-0.5f, 0.5f); q.y += rf(-0.5f, 0.5f); q.z += rf(-0.2f, 0.2f);
            ee_ref<Vec4>(P + 0x80) = q;
        }
        seed_rand();
        return cam;
    };
    fcheck("cam_type1_accept", acc, 200, Ret::Int32, [cam_setup] { uint32_t other = 0; const uint32_t cam = cam_setup(&other, 0); return Args{{cam, other}, {}}; },
           [](A a) { vu0_sync(); return ri32(cam_type1_accept(a.i[0], a.i[1])); });
    for (uint32_t at : {init, init2})
        fcheck("cam_type1_init", at, 60, Ret::None, [cam_setup] { return Args{{cam_setup(nullptr, 0)}, {}}; },
               [](A a) { vu0_sync(); cam_type1_init(a.i[0]); return uint64_t{0}; });
    for (uint32_t at : {upd, upd2})
        fcheck("cam_type1_update", at, 100, Ret::None, [cam_setup] { return Args{{cam_setup(nullptr, ri(1, 10))}, {}}; },
               [](A a) { vu0_sync(); cam_type1_update(a.i[0]); return uint64_t{0}; });
    fcheck("cam_type1_exit", ext, 100, Ret::None, [cam_setup] { return Args{{cam_setup(nullptr, ri(1, 6))}, {}}; },
           [](A a) { vu0_sync(); cam_type1_exit(a.i[0]); return uint64_t{0}; });
}

// ---- the follow camera's level hook and callback, the callback queue, the distance rates, the turn (kerwan_camera.cpp)
void cam0_checks() {
    const uint32_t hook = copy_here({{2, 0x002fa710}, {3, 0x002eb408}, {4, 0x002f0448}, {5, 0x00328f80}, {6, 0x003147b0}, {7, 0x00328090},
                                     {9, 0x00314220}, {10, 0x002f4b18}, {11, 0x00327a90}, {12, 0x003148f0}, {13, 0x00316b48},
                                     {14, 0x003131e0}, {15, 0x002f6980}, {16, 0x002f3a88}, {17, 0x002fff38}, {18, 0x00305318}});
    const uint32_t cb = copy_here({{2, 0x002fa770}, {3, 0x002eb468}, {4, 0x002f04a8}, {5, 0x00328fe0}, {6, 0x00314810}, {7, 0x003280f0},
                                   {9, 0x00314280}, {10, 0x002f4b78}, {11, 0x00327af0}, {12, 0x00314950}, {13, 0x00316ba8},
                                   {14, 0x00313240}, {15, 0x002f69e0}, {16, 0x002f3ae8}, {17, 0x002fff98}, {18, 0x00305378}});
    const uint32_t queue = copy_here({{2, 0x001fc008}, {3, 0x001e4a88}, {4, 0x001dda88}, {5, 0x00219d88}, {6, 0x00202d08}, {7, 0x00212f08},
                                      {9, 0x0020a488}, {10, 0x001de788}, {11, 0x0021c988}, {12, 0x0020db88}, {13, 0x001f6b08},
                                      {14, 0x001fe308}, {15, 0x001e4808}, {16, 0x001e9988}, {17, 0x001e8188}, {18, 0x001f3e08}});
    const uint32_t rates = copy_here({{2, 0x002f6598}, {3, 0x002e6f80}, {6, 0x003106b8}, {7, 0x00323ff8}, {8, 0x00310218}, {12, 0x00310818},
                                      {15, 0x002f28b8}, {18, 0x00301280}});
    const uint32_t turn = copy_here({{3, 0x0024e6f8}, {4, 0x00252ce0}});
    // the look-height block 0x167000 (+0 / +4 flags, +0xc the height), the callback queue (its count 0x15ef4c, 0x168ff0)
    auto prep = [cb] {
        level();
        const uint32_t b = OV(0x00167000);
        wf(b + 0xc, ri(0, 3) == 0 ? 0.0f : (ri(0, 4) == 0 ? rf(0.0f, 0.02f) : rf(-1, 3)));
        w32(b + 4, ri(0, 2) == 0 ? 0u : 1u);
        w32(b, R() & 1);
        wf(b + 8, rf(0, 5));
        const int32_t n = ri(0, 3) == 0 ? ri(14, 16) : ri(0, 5);
        w32(OV(0x0015ef4c), u(n));
        for (int32_t i = 0; i < n; ++i) w32(OV(0x00168ff0) + u(i) * 4, ri(0, 3) == 0 ? cb : R());
        if (ri(0, 4) == 0) wf(0x0015ed6c, rf(0.01f, 0.05f));
        seed_rand();
    };
    if (hook == 0) not_here("cam0_level_hook");
    else fcheck("cam0_level_hook", hook, 150, Ret::None, [prep] { prep(); return Args{{0u}, {}}; },
                [](A) { vu0_sync(); cam0_level_hook(0); return uint64_t{0}; });
    if (cb == 0) not_here("cam0_level_callback");
    else fcheck("cam0_level_callback", cb, 150, Ret::None, [prep] { prep(); return Args{{0u}, {}}; },
                [](A) { cam0_level_callback(0); return uint64_t{0}; });
    if (queue == 0) not_here("cam_callback_queue");
    else fcheck("cam_callback_queue", queue, 100, Ret::None, [prep, cb] {
            prep();
            return Args{{coin() ? cb : (coin() ? ee_read<uint32_t>(OV(0x00168ff0)) : R())}, {}};
        }, [](A a) { cam_callback_queue(a.i[0]); return uint64_t{0}; });
    if (rates == 0) not_here("cam0_set_20c");
    else fcheck("cam0_set_20c", rates, 100, Ret::None, [] {
            level();
            const uint32_t c = ee_read<uint32_t>(OV(0x00166e00));
            if (ri(0, 3) == 0) w16(c + 0x86, u(ri(1, 20)));
            const uint32_t p = ee_read<uint32_t>(c + 0x70) + 0x1d0;
            for (uint32_t o : {0x3cu, 0x40u, 0x44u}) wf(p + o, coin() ? 0.0f : rf(-2.0f, 20.0f));
            return Args{{}, {rf(-2, 20), rf(-2, 20), rf(-1, 1)}};
        }, [](A a) { cam0_set_20c(a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
    if (turn == 0) not_here("slerp_vec_toward");
    else fcheck("slerp_vec_toward", turn, 150, Ret::None, [] {
            level();
            constexpr uint32_t OUTV = TEST_AREA + 0x2000, FROM = TEST_AREA + 0x2010, TO = TEST_AREA + 0x2020;
            wfrand(OUTV, 4, -3.0f, 3.0f);
            wfrand(FROM, 4, -2.0f, 2.0f);
            wfrand(TO, 4, -2.0f, 2.0f);
            const int k = ri(0, 7);
            if (k == 0) for (uint32_t o = 0; o < 0x10; o += 4) w32(FROM + o, 0);
            if (k == 1) for (uint32_t o = 0; o < 0x10; o += 4) w32(TO + o, 0);
            if (k == 2) ee_ref<Vec4>(TO) = ee_ref<Vec4>(FROM);
            if (k == 3) { Vec4 v = ee_ref<Vec4>(FROM); v.x = -v.x; v.y = -v.y; v.z = -v.z; ee_ref<Vec4>(TO) = v; }
            if (k == 4) {  // (unit vectors, as camera type 1 passes them)
                for (uint32_t a : {FROM, TO}) {
                    Vec4 v = ee_ref<Vec4>(a);
                    const float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
                    if (l > 0.0f) { v.x /= l; v.y /= l; v.z /= l; }
                    ee_ref<Vec4>(a) = v;
                }
            }
            return Args{{OUTV, FROM, TO, u(ri(0, 1))}, {ri(0, 4) == 0 ? (coin() ? 0.0f : 1.0f) : rf(-1.0f, 2.0f)}};
        }, [](A a) { vu0_sync(); slerp_vec_toward(VV(a.i[0]), VV(a.i[1]), VV(a.i[2]), static_cast<int32_t>(a.i[3]), a.f[0]); return uint64_t{0}; });
}

// ---- class 605, the zone marker (novalis_props.cpp)
void zone_marker_checks() {
    const uint32_t upd = copy_here({{1, 0x002f2eb8}, {2, 0x002d74e0}, {3, 0x002cbc38}, {4, 0x002d46b8}, {5, 0x00301cd8}, {6, 0x002e1518},
                                    {7, 0x002fd698}, {8, 0x002f1c08}, {9, 0x002f7658}, {10, 0x002c6470}, {11, 0x002fc980},
                                    {12, 0x002f5540}, {13, 0x002f51f0}, {14, 0x002ead20}, {15, 0x002d5728}, {16, 0x002d2a80},
                                    {17, 0x002d3da0}, {18, 0x002d8ba8}});
    if (upd == 0) { not_here("zone_marker_update"); return; }
    // the level's own count of the markers met (its small data: the counter the port picks per level)
    const uint32_t counter = copy_here({{1, 0x00161af0}, {2, 0x00161a60}, {3, 0x00161990}, {4, 0x001619e0}, {5, 0x00161a70}, {6, 0x00161960},
                                        {7, 0x00161930}, {8, 0x00161e30}, {9, 0x001619f0}, {10, 0x00161950}, {11, 0x00161ac0},
                                        {12, 0x00161b20}, {13, 0x00161a20}, {14, 0x00161c00}, {15, 0x00161bc0}, {16, 0x00161a70},
                                        {17, 0x00161a50}, {18, 0x00161ab8}});
    level();
    const bool placed = !mobys_with(upd).empty();
    const std::vector<uint32_t> paths = level_paths();
    constexpr uint32_t PV = TEST_AREA + 0x3000;
    // the level's markers, or (none placed) live mobys given a marker's pvars in the test area
    auto subjects = [upd, placed] { return placed ? mobys_with(upd) : live_mobys(); };
    for (int pass = 0; pass < 4; ++pass)
        classcheck("zone_marker_update", upd, subjects, zone_marker_update, [pass, placed, paths, counter](uint32_t m) {
            if (!placed) {
                for (uint32_t o = 0; o < 0x40; o += 4) w32(PV + o, 0);
                w32(m + 0x78, PV);
                w32(PV + 0x10, paths.empty() || coin() ? 0xffffffffu : pick(paths));
                w32(PV + 0x14, coin() ? 0xffffffffu : u(ri(0, 3)));
                w32(PV + 0x18, u(ri(0, 40)));
            }
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(pass == 0 ? 0 : (pass == 1 ? 1 : ri(0, 2))));
            w8(m + 0xbc, u(ri(0, 2) == 0 ? 0 : ri(1, 9)));
            if (pv != 0 && ri(0, 3) == 0) w32(pv + 0x18, u(ri(-5, 30)));
            if (pv != 0 && ri(0, 4) == 0) w32(pv + 0x10, 0xffffffffu);
            if (pv != 0 && ri(0, 4) == 0) w32(pv + 0x14, 0xffffffffu);
            if (ri(0, 5) == 0) w32(m + 0x78, 0);
            if (ri(0, 4) == 0) w32(0x0015ed84, u(ri(0, 24)));
            // the markers met so far (the nibble each takes: odd / even counts) and the level's row of hint nibbles
            if (ri(0, 3) != 0) w32(counter, u(ri(0, 12)));
            if (ri(0, 3) != 0) for (uint32_t k = 0; k < 0x10; ++k) w8(0x0014bf10 + u(g_oracle_level) * 0x10 + k, R() & 0xff);
            // the hero at the marker, in its polygon or box, or away; or riding a moby (state 0x1b) there
            Vec4 at = ee_ref<Vec4>(m + 0x10);
            if (pv != 0) {
                const int32_t poly = ee_read<int32_t>(pv + 0x10), box = ee_read<int32_t>(pv + 0x14);
                if (poly >= 0 && poly < 64 && coin()) {
                    const uint32_t path = path_ptr(u(poly));
                    if (path >= 0x00100000u && path < 0x02000000u && ee_read<int32_t>(path) > 0)
                        at = path_point(path, ri(0, std::min(64, ee_read<int32_t>(path)) - 1));
                }
                if (box >= 0 && box < 0x200 && coin()) at = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(box) * 0x80 + 0x30);
            }
            ee_ref<Vec4>(0x0013f3d0) = around(at, 0.0f, ri(0, 2) == 0 ? 60.0f : 4.0f, -1.0f, 1.0f);
            if (pv != 0 && ri(0, 3) == 0) {  // (no polygon or box: the hero 17..21 from it, about the "near" mark's 20)
                w32(pv + 0x10, 0xffffffffu);
                w32(pv + 0x14, 0xffffffffu);
                ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 17.0f, 21.0f, -1.0f, 1.0f);
            }
            w32(P + 0x1090, 0);
            if (ri(0, 3) == 0) {
                const std::vector<uint32_t> live = live_mobys();
                const uint32_t o = pick(live);
                w32(P + 0x1090, o);
                w32(P + 0x10b8, coin() ? 0x1bu : u(ri(0, 0x30)));
            }
            wf(P + 0x2044, ri(0, 2) == 0 ? 1000.0f : rf(0.0f, 40.0f));
            if (coin()) w32(P + 0x2048, 0);
            if (coin()) w32(ee_read<uint32_t>(OV(0x0015ffd8)) + 0x78 + 0x100 * u(ri(0, 4)), coin() ? 0u : PV);
        }, 40);
}

// ---- the teleporter's pieces and the moby group's state (novalis_teleporter.cpp)
void teleporter_checks() {
    const uint32_t pieces = copy_here({{1, 0x003092d0}, {7, 0x0031cc20}, {8, 0x003049a0}, {11, 0x002f2b70}, {12, 0x003013e0},
                                       {13, 0x00304f80}, {16, 0x002e0818}, {17, 0x002e5628}, {18, 0x002ee270}});
    const uint32_t grp = copy_here({{1, 0x0026e0e0}, {5, 0x00283498}, {6, 0x00267f70}, {7, 0x00281c30}, {8, 0x00263730}, {11, 0x0027f0e0},
                                    {12, 0x002720d0}, {13, 0x00265050}, {15, 0x00248ac8}, {16, 0x002549d8}, {17, 0x00253970},
                                    {18, 0x0025b4a0}});
    if (pieces == 0) not_here("teleporter_pieces_update");
    else {
        constexpr uint32_t PV = TEST_AREA + 0x3000;
        auto pads = [] {
            std::vector<uint32_t> v;
            for (uint32_t m = first_moby(); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<int16_t>(m + 0xa6) == 1135 && ee_read<uint32_t>(m + 0x78) != 0) v.push_back(m);
            return v.empty() ? live_mobys() : v;
        };
        classcheck("teleporter_pieces_update", pieces, pads, teleporter_pieces_update, [](uint32_t m) {
            const std::vector<uint32_t> live = live_mobys();
            if (ee_read<int16_t>(m + 0xa6) != 1135 || ri(0, 3) == 0) w32(m + 0x78, PV);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            // (three other live mobys as the pieces; not one whose 0x100 bytes hold the pvars PV .. PV + 0x28: on Kalebo III
            // the moby table reaches over the test area, and the pvars dealt here would overwrite such a moby's grid rect
            // +0xa0, a state the game never has: the original's grid move then meets its `teq` trap, which the interpreter
            // runs through into the next function)
            auto overlaps_pv = [](uint32_t o) { return o < PV + 0x28 && PV < o + 0x100; };
            for (uint32_t k = 0; k < 3; ++k) {
                uint32_t o = pick(live);
                for (int t = 0; t < 16 && (o == m || overlaps_pv(o)); ++t) o = pick(live);
                w32(pv + 0x14 + k * 4, o);
            }
            for (uint32_t o : {0x20u, 0x24u}) wf(pv + o, ri(0, 3) == 0 ? (coin() ? 0.0f : 1.0f) : rf(-0.3f, 1.3f));
            if (coin()) { Vec4 q = ee_ref<Vec4>(m + 0x10); q.z += rf(-3, 3); ee_ref<Vec4>(m + 0x10) = q; }
        }, 40);
    }
    if (grp == 0) not_here("moby_group_set_state");
    else fcheck("moby_group_set_state", grp, 60, Ret::None, [] {
            level();
            const uint32_t first = first_moby();
            for (int i = 0; i < 8; ++i) w8(first + 0x100 * u(ri(0, 300)) + 0x20, R());  // (states, some negative)
            int32_t g = ri(0, 40);
            for (int k = 0; k < 40 && ri(0, 4) != 0; ++k) {  // (mostly a group the level has)
                if (ee_read<uint32_t>(OV(0x001ab840) + u(g) * 4) != 0) break;
                g = ri(0, 40);
            }
            return Args{{u(g), R() & 0xffu}, {}};
        }, [](A a) { moby_group_set_state(static_cast<int32_t>(a.i[0]), a.i[1]); return uint64_t{0}; });
}

// ---- the class hooks (novalis_hooks.cpp): on the mobys whose class's hook table (class +0x2c) names the level's copy
void hook_checks() {
    struct Hk { const char* name; int level; uint32_t addr; int32_t (*fn)(uint32_t, uint32_t, uint32_t, float); };
    for (const Hk& h : {Hk{"peckbot_hook8", 1, 0x002f1d18, peckbot_hook8_l1}, Hk{"peckbot_hook8", 13, 0x002b7a20, peckbot_hook8_l1},
                        Hk{"amoeboid_hook0", 1, 0x002efa88, amoeboid_hook0_l1}, Hk{"amoeboid_hook0", 11, 0x002fb780, amoeboid_hook0_l1}}) {
        if (!selected(h.name)) continue;
        if (h.level != g_oracle_level) continue;
        level();
        std::vector<uint32_t> own;
        for (uint32_t m = first_moby(); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
            const uint32_t cls = ee_read<uint32_t>(m + 0x24);
            if (ee_read<uint8_t>(m + 0x20) >= 0xfe || cls == 0 || ee_read<uint32_t>(cls + 0x2c) == 0) continue;
            for (uint32_t s = 0; s < 0x18; s += 4)
                if (ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + s) == h.addr) { own.push_back(m); break; }
        }
        if (own.empty()) { std::printf("%-26s L%d:0x%08x  no moby owns it on this level\n", h.name, g_oracle_level, h.addr); continue; }
        std::printf("  (%s: %zu owners, class %u)\n", h.name, own.size(), ee_read<uint16_t>(own[0] + 0xa6));
        g_extra_noop_hooks.push_back(0x002763f8);  // (batch 120's)
        auto fn = h.fn;
        fcheck(h.name, h.addr, 80, Ret::Int32, [own] {
            level();
            const uint32_t m = pick(own);
            w8(m + 0x20, coin() ? u(ri(0, 15)) : (coin() ? 7u : 0xeu));
            if (ri(0, 3) == 0) w16(m + 0xa6, coin() ? 0x362u : 0x23cu);
            const uint32_t r = class_hook_10(m);
            if (r != 0) {  // (the carry record: its state, the other moby)
                static const uint16_t st[] = {0, 1, 2, 3, 4, 5, 6, 6, 7, 8};
                w16(r + 0x68, st[ri(0, 9)]);
                if (coin()) wf(r + 0x6c, rf(0.0f, 2.0f));
            }
            w8(m + 0xbc, u(ri(0, 9)));
            const uint32_t tgt = ri(0, 3) == 0 ? 0u : first_moby() + 0x100 * u(ri(0, 100));
            wfrand(OUT, 8, -3.0f, 3.0f);
            seed_rand();
            return Args{{m, ri(0, 2) == 0 ? OUT : m + 0x10, tgt}, {rf(-1, 3)}};
        }, [fn](A a) { return ri32(fn(a.i[0], a.i[1], a.i[2], a.f[0])); });
        g_extra_noop_hooks.pop_back();
    }
}

// ---- whether the hero is on a moby (novalis_platforms.cpp) and class 915 (kerwan_props.cpp)
void small_checks() {
    struct Lv { int level; uint32_t addr; };
    bool any = false;
    for (const Lv& c : {Lv{1, 0x00277fb8}, Lv{3, 0x00251598}, Lv{3, 0x002d3140}, Lv{3, 0x002dcb70}, Lv{7, 0x002cd258}, Lv{7, 0x002cdad0},
                        Lv{7, 0x00311b70}, Lv{9, 0x00303348}, Lv{10, 0x00256338}, Lv{10, 0x00295bc8}, Lv{12, 0x002e38f0},
                        Lv{13, 0x0026ed40}, Lv{14, 0x0026afa0}}) {
        if (c.level != g_oracle_level) continue;
        any = true;
        fcheck("moby_hero_on", c.addr, 60, Ret::Int32, [] {
            level();
            const std::vector<uint32_t> live = live_mobys();
            const uint32_t m = pick(live);
            w32(P + 0x208c, ri(0, 3) == 0 ? 3u : u(ri(0, 6)));
            w32(P + 0x2084, ri(0, 3) == 0 ? 0x1cu : u(ri(0, 0x30)));
            w32(P + 0x4f8, coin() ? m : pick(live));
            w16(P + 0x30e, u(ri(0, 2) == 0 ? ri(1, 3) : 0));
            w32(P + 0x2fc, coin() ? m : (coin() ? 0u : pick(live)));
            return Args{{m}, {}};
        }, [](A a) { return ri32(moby_hero_on_l1(a.i[0])); });
    }
    if (!any) not_here("moby_hero_on");
    const uint32_t fred = copy_here({{3, 0x002dc310}, {10, 0x002d7fe8}});
    if (fred == 0) not_here("fred_update");
    else classcheck("fred_update", fred, [] { return live_mobys(); }, fred_update_l3, [](uint32_t m) {
            ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 30.0f, -2.0f, 2.0f);
            wf(m + 0x48, rf(-3.1f, 3.1f));
        }, 30);
}
}  // namespace

void batch_502() {
    if (g_only_batch != 502) return;
    g_batch = 502;
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
    if (g_oracle_level < 1 || g_oracle_level > 18) {
        std::printf("batch 502: levels 1 .. 18 only\n");
    } else {
        level();
        // (the kernel's calls the original would make for real: batch 506's)
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

        cam3_checks();
        cam1_checks();
        cam0_checks();
        zone_marker_checks();
        teleporter_checks();
        hook_checks();
        small_checks();
    }

    g_pre_call = nullptr;
    g_extra_noop_hooks.clear();
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
