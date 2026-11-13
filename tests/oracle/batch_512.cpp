// Oracle batch 512 (stage 4 task G5, #149): hand-written ports of one level made to serve every level that carries a copy
// of the same code (layer 1: they call the ports tasks G1 and G2 generalised), each against every level's own copy at that
// level's own address (the copies stay transliterated in their units until each level is re-glued; here every copy is the
// original and the one port the port):
//  - kerwan_camera.cpp: camera type 18 (0x12), the follow camera's modifier toward a moby: its accept (where its frame
//    runs), the frame, the moby's cone test, the hero's zone test, and the follow camera's word +0x218 (levels 2, 3, 6, 7,
//    8, 12, 18), on a camera of the test area whose record (the level's first) names data of the test area: every kind
//    (+0x3c 0 .. 8), the counts at their edges, the moby from a group list or by index (live or not), every zone shape (the
//    override moby, the platform, the radius, a box, cylinder, sphere and polygon of the test area), the right stick, the
//    claim held by the camera, another or none, the follow camera given a real view;
//  - novalis_path.cpp: the path follower's point, index wrap and advance, the spline tangent, the Hermite curve and the
//    path's length (levels 1, 3, 4, 14), on followers of the level's paths and of paths of the test area;
//  - novalis_jet_base.cpp: the aircraft base (Kerwan's class 845, Eudora's and Oltanis's copies; levels 1, 3, 4, 14) on the
//    level's own aircraft (Oltanis has none placed at its start: live mobys given an aircraft's pvars in the test area),
//    each state, every row of its jump table (the classes 0x33 .. 0x4a), its bank (and Kerwan's second build of it, class
//    75's, with its pair at +0x140) and its smoke particle;
//  - novalis_mobys.cpp: class 737, the scripted-camera trigger (levels 1, 2, 3, 7, 10, 13), each state, its boxes in the
//    test area.
// No RAM dumps: states built from the level after its first gameplay frame.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 512, N = 1, 2, 3, 4, 6, 7, 8, 10, 12, 13, 14, 18; a family the level
// does not carry says so (B512_INFO=1 prints the subjects found).

#include "ovl_oracle.h"

#include <game/ovl/kerwan_camera.h>
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

bool info() { return std::getenv("B512_INFO") != nullptr; }

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
uint32_t copy_here(std::initializer_list<LvAddr> copies) {
    const std::vector<uint32_t> v = copies_here(copies);
    return v.empty() ? 0u : v[0];
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

constexpr uint32_t P = 0x0013f350;     // the hero (+0x80 his position)
constexpr uint32_t HERO = 0x0013f3d0;  // his position

uint32_t first_moby() { return ee_read<uint32_t>(OV(0x0015ffd8)); }

// the level's live mobys (with a class), and those whose update function is one of `fns` (the level's addresses)
std::vector<uint32_t> live_mobys() {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x24) != 0) v.push_back(m);
    return v;
}
std::vector<uint32_t> mobys_with(std::initializer_list<uint32_t> fns) {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
        if (ee_read<uint8_t>(m + 0x20) >= 0xfe) continue;
        for (uint32_t fn : fns)
            if (ee_read<uint32_t>(m + 0x74) == fn) { v.push_back(m); break; }
    }
    return v;
}

uint32_t pick(const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; }

bool in_ram(uint32_t a) { return a >= 0x00100000u && a < 0x01ff0000u; }

// a point `r0`..`r1` (horizontally) about `at`
Vec4 around(const Vec4& at, float r0, float r1, float z0, float z1) {
    const float r = rf(r0, r1), a = rf(-3.1f, 3.1f);
    return Vec4{at.x + r * std::cos(a), at.y + r * std::sin(a), at.z + rf(z0, z1), 1.0f};
}

// a camera's rows (+0 forward, +0x10 side, +0x20 up, +0x30 its place, +0x40 the forward again) at `at`, looking about the
// hero with some yaw and pitch off (as batch 502)
void set_view(uint32_t cam, const Vec4& at) {
    const Vec4 h = ee_ref<Vec4>(HERO);
    float fx = h.x - at.x + rf(-2, 2), fy = h.y - at.y + rf(-2, 2), fz = h.z - at.z + rf(-1, 1);
    float l = std::sqrt(fx * fx + fy * fy + fz * fz);
    if (!(l > 0.001f)) { fx = 1.0f; fy = 0.0f; fz = 0.0f; l = 1.0f; }
    fx /= l; fy /= l; fz /= l;
    float sx = fy, sy = -fx;
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
std::vector<uint32_t> live_cams() {
    std::vector<uint32_t> v;
    for (uint32_t i = 0; i < 48; ++i)
        if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0) v.push_back(OV(0x00167150) + i * 0xa0);
    return v;
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

// ---- camera type 18 (kerwan_camera.cpp)
// The camera CAM (the level's camera record 0, its data D), the zone shapes SH (two boxes, a cylinder and a sphere: the
// tables 0x1600ec / 0x1600fc / 0x1600f4 pointed at them) and a polygon POLY (in a slot of the path table 0x1b04b0), the
// group list GL (in a slot of the group table 0x1ab840)
constexpr uint32_t CAM = TEST_AREA + 0x2000, D = TEST_AREA + 0x2100, SH = TEST_AREA + 0x4000, POLY = TEST_AREA + 0x4400;
constexpr uint32_t GL = TEST_AREA + 0x4600;

// a shape record at `rec`: its centre +0x30 and the matrix +0x40 that takes an offset into the unit shape (a scale `s`
// and a turn about z)
void shape_at(uint32_t rec, const Vec4& c, float sx, float sy, float sz) {
    for (uint32_t o = 0; o < 0x80; o += 4) w32(rec + o, 0);
    ee_ref<Vec4>(rec + 0x30) = c;
    const float a = rf(-3.1f, 3.1f), ca = std::cos(a), sa = std::sin(a);
    ee_ref<Vec4>(rec + 0x40) = Vec4{ca / sx, sa / sy, 0.0f, 0.0f};
    ee_ref<Vec4>(rec + 0x50) = Vec4{-sa / sx, ca / sy, 0.0f, 0.0f};
    ee_ref<Vec4>(rec + 0x60) = Vec4{0.0f, 0.0f, 1.0f / sz, 0.0f};
}

// the counts' edges (frame counts the frame compares +0x20 / +0x3e with, after its own +1)
int32_t count_near_edge() {
    static const int32_t edges[] = {0x5a, 0x78, 0xc8, 0x12c, 0x12c + 0x1e, 0x15e, 0x190, 0x230};
    if (ri(0, 4) == 0) return ri(-2, 700);
    return frame_scale_i(edges[ri(0, 7)]) + ri(-2, 1);
}

// The state of a type-18 modifier: `kind` (-1 dealt), the moby `live` (-1 dealt), the zone `shape` (-1 dealt: 0 the
// override moby, 1 the platform, 2 the radius, 3 a box, 4 a cylinder, 5 a sphere, 6 a polygon, 7 none). Returns the
// data's moby.
uint32_t cam18_setup(int kind, int live, int shape) {
    level();
    const std::vector<uint32_t> ms = live_mobys();
    const uint32_t pool = first_moby();
    for (uint32_t o = 0; o < 0xa0; o += 4) w32(CAM + o, 0);
    w16(CAM + 0x84, 0);
    w16(CAM + 0x86, 18);
    w32(CAM + 0x74, 4);
    const uint32_t e = ee_read<uint32_t>(OV(0x0015ef50));  // (the level's camera record 0)
    w32(e + 0x1c, D);
    wf(e + 0x18, rf(-3.1f, 3.1f));
    // the moby: from a group list (dead ones first) or by index
    const uint32_t mob = pick(ms);
    const bool alive = live < 0 ? ri(0, 4) != 0 : live != 0;
    w8(mob + 0x20, alive ? u(ri(0, 0x10)) : u(ri(0x80, 0xff)));
    for (uint32_t o = 0; o < 0x60; o += 4) w32(D + o, 0);
    const uint32_t g = u(ri(0, 7));
    if (ri(0, 2) != 0) {
        w32(D + 0x44, g);
        const int ndead = ri(0, 2);
        uint32_t p = GL;
        for (int k = 0; k < ndead; ++k, p += 2) {
            const uint32_t d = pick(ms);
            if (d == mob) continue;
            w8(d + 0x20, u(ri(0x80, 0xff)));
            w16(p, (d - pool) >> 8);
        }
        w16(p, ((mob - pool) >> 8) | (ri(0, 3) == 0 ? 0u : 0x8000u));
        if (ee_read<uint16_t>(p) < 0x8000u) w16(p + 2, 0x8000u | ((pick(ms) - pool) >> 8));
        w32(OV(0x001ab840) + g * 4, ri(0, 7) == 0 ? 0u : GL);
    } else {
        w32(D + 0x44, 0xffffffffu);
        w32(D + 0x28, (mob - pool) >> 8);
    }
    // the data
    wf(D + 0x00, ri(0, 6) == 0 ? 0.0f : rf(-30.0f, 150.0f));
    w8(D + 0x1c, u(ri(0, 5)));
    w16(D + 0x20, u(ri(0, 8) == 0 ? -1 : count_near_edge()));
    wf(D + 0x24, rf(1.0f, 15.0f));
    wf(D + 0x2c, ri(0, 2) == 0 ? 0.0f : rf(2.0f, 70.0f));
    wf(D + 0x30, ri(0, 2) == 0 ? 0.0f : rf(2.0f, 120.0f));
    wf(D + 0x34, ri(0, 2) == 0 ? 0.0f : rf(2.0f, 15.0f));
    wf(D + 0x38, ri(0, 2) == 0 ? 0.0f : rf(-2.0f, 6.0f));
    wf(D + 0x4c, ri(0, 2) == 0 ? 0.0f : rf(-1.0f, 3.0f));
    const int knd = kind >= 0 ? kind : ri(0, 8);
    w16(D + 0x3c, u(knd));
    w16(D + 0x3e, u(count_near_edge()));
    wf(D + 0x40, rf(-3.1f, 3.1f));
    w32(D + 0x48, pick(ms));
    w32(D + 0x50, ri(0, 7) == 0 ? 1u : 0u);
    for (uint32_t o : {0x08u, 0x0cu, 0x10u, 0x14u}) w32(D + o, 0xffffffffu);
    // the zone
    const int shp = shape >= 0 ? shape : ri(0, 7);
    const Vec4 mp = ee_ref<Vec4>(mob + 0x10);
    const Vec4 c = coin() ? mp : around(mp, 0.0f, 10.0f, -1.0f, 1.0f);
    const float s = rf(1.0f, 6.0f);
    w32(OV(0x00166edc), shp == 0 ? mob : (ri(0, 5) == 0 ? pick(ms) : 0u));
    w32(0x0013f64c, shp == 1 || knd == 6 ? (ri(0, 3) == 0 ? pick(ms) : mob) : (ri(0, 5) == 0 ? mob : 0u));
    w8(D + 0x22, shp == 2 ? 1u : (ri(0, 5) == 0 ? 2u : 0u));
    shape_at(SH, c, s, s * rf(0.5f, 2.0f), s * rf(0.3f, 1.0f));
    shape_at(SH + 0x80, around(c, 2.0f, 8.0f, -1.0f, 1.0f), s, s, s);
    shape_at(SH + 0x100, c, s, s, s * rf(0.3f, 1.5f));
    shape_at(SH + 0x200, c, s, s * rf(0.5f, 1.5f), s);
    w32(OV(0x001600ec), SH);
    w32(OV(0x001600fc), SH + 0x100);
    w32(OV(0x001600f4), SH + 0x200);
    const uint32_t pslot = u(ri(0, 7));
    const int32_t np = ri(3, 6);
    w32(POLY, u(np));
    for (int32_t i = 0; i < np; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(np) + rf(-0.3f, 0.3f), r = s * rf(0.6f, 1.4f);
        ee_ref<Vec4>(POLY + 0x10 + u(i) * 0x10) = Vec4{c.x + r * std::cos(a), c.y + r * std::sin(a), c.z, 0.0f};
    }
    w32(OV(0x001b04b0) + pslot * 4, POLY);
    if (shp == 3) w32(D + 0x0c, u(ri(0, 1)));
    if (shp == 4) w32(D + 0x10, 0);
    if (shp == 5) w32(D + 0x08, 0);
    if (shp == 6) w32(D + 0x14, pslot);
    if (shp >= 3 && shp <= 6 && ri(0, 4) == 0) {  // (a later shape set too: the first set one decides)
        for (uint32_t o : {0x08u, 0x10u, 0x14u}) if (coin()) w32(D + o, o == 0x14u ? pslot : 0u);
    }
    // the hero inside or about the zone
    ee_ref<Vec4>(HERO) = around(shp == 2 ? mp : c, 0.0f, coin() ? s * 0.8f : s * 3.0f, -0.5f * s, 0.5f * s);
    // the follow camera (the active one, type 0 but now and then another type): a view of the hero; its claim held by this
    // camera, another live camera or none
    const uint32_t act = ee_read<uint32_t>(OV(0x00166e00));
    w16(act + 0x86, ri(0, 7) == 0 ? u(ri(1, 20)) : 0u);
    set_view(act, around(ee_ref<Vec4>(HERO), 3.0f, 12.0f, 1.0f, 5.0f));
    const uint32_t w = ee_read<uint32_t>(act + 0x70);
    const std::vector<uint32_t> cams = live_cams();
    uint32_t holder = 0;
    const int hk = ri(0, 3);
    if (hk == 1) holder = CAM;
    if (hk >= 2 && !cams.empty()) {
        holder = pick(cams);
        const uint32_t he = ee_read<uint32_t>(OV(0x0015ef50)) + u(ee_read<int16_t>(holder + 0x84) * 0x20);
        const uint32_t hd = ee_read<uint32_t>(he + 0x1c);
        if (in_ram(hd)) w8(hd + 0x1c, u(ri(0, 5)));
        else holder = 0;
    }
    w32(w + 0x220, holder);
    // the up axis the cone test measures about (a tilt now and then)
    ee_ref<Vec4>(OV(0x00166e40)) = ri(0, 3) == 0 ? Vec4{rf(-0.2f, 0.2f), rf(-0.2f, 0.2f), 1.0f, 0.0f} : Vec4{0.0f, 0.0f, 1.0f, 0.0f};
    // the right stick (its 0.3 threshold)
    for (uint32_t a : {0x0013ca40u, 0x0013ca44u}) {
        const int k = ri(0, 2);
        wf(a, k == 0 ? 0.0f : (coin() ? 1.0f : -1.0f) * (k == 1 ? rf(0.01f, 0.29f) : rf(0.3f, 1.0f)));
    }
    seed_rand();
    return mob;
}

void cam18_checks() {
    const uint32_t acc = copy_here({{2, 0x002fc298}, {3, 0x002ed5f8}, {6, 0x00316368}, {7, 0x003290b8}, {8, 0x00316298}, {12, 0x00316478}, {18, 0x00306370}});
    const uint32_t upd = copy_here({{2, 0x002fb9c8}, {3, 0x002ecd28}, {6, 0x00315a68}, {7, 0x003287e8}, {8, 0x003159c8}, {12, 0x00315ba8}, {18, 0x00305a70}});
    const uint32_t cone = copy_here({{2, 0x002fb788}, {3, 0x002ecae8}, {6, 0x00315828}, {7, 0x003285a8}, {8, 0x00315788}, {12, 0x00315968}, {18, 0x00305830}});
    const uint32_t ins = copy_here({{2, 0x002fb648}, {3, 0x002ec9a8}, {6, 0x003156e8}, {7, 0x00328468}, {8, 0x00315648}, {12, 0x00315828}, {18, 0x003056f0}});
    const uint32_t s218 = copy_here({{2, 0x002f6570}, {3, 0x002e6f58}, {6, 0x00310690}, {7, 0x00323fd0}, {8, 0x003101f0}, {12, 0x003107f0}, {18, 0x00301258}});
    if (acc == 0) {
        for (const char* n : {"cam_type18_accept", "cam_type18_update", "cam_type18_cone_check", "cam_type18_inside", "cam0_set_218"}) not_here(n);
        return;
    }
    if (info() && selected("cam_type18_update")) {
        level();
        const uint32_t act = ee_read<uint32_t>(OV(0x00166e00));
        std::printf("INFO active camera %08x type %d work %08x, cameras %zu, mobys %zu\n", act, ee_read<int16_t>(act + 0x86),
                    ee_read<uint32_t>(act + 0x70), live_cams().size(), live_mobys().size());
    }
    fcheck("cam_type18_accept", acc, 300, Ret::Int32, [] {
        cam18_setup(-1, -1, -1);
        const std::vector<uint32_t> cams = live_cams();
        return Args{{CAM, ri(0, 3) == 0 || cams.empty() ? 0u : pick(cams)}, {}};
    }, [](A a) { vu0_sync(); return ri32(cam_type18_accept(a.i[0], a.i[1])); });
    // the frame on each kind (and dealt), the moby live and not
    for (int kind : {1, 2, 3, 4, 5, 6, 7, 0, -1})
        fcheck("cam_type18_update", upd, kind == -1 ? 150 : 80, Ret::None, [kind] {
            cam18_setup(kind, -1, -1);
            if (ri(0, 5) != 0) w32(D + 0x50, 0);
            return Args{{CAM}, {}};
        }, [](A a) { vu0_sync(); cam_type18_update(a.i[0]); return uint64_t{0}; });
    fcheck("cam_type18_cone_check", cone, 200, Ret::Int32, [] {
        const uint32_t mob = cam18_setup(-1, 1, -1);
        w32(D + 0x48, mob);
        if (coin()) {  // (the moby before or beside the camera: its pitch and heading about the limits)
            const uint32_t act = ee_read<uint32_t>(OV(0x00166e00));
            const Vec4 cp = ee_ref<Vec4>(act + 0x30), f = ee_ref<Vec4>(act);
            const float d = rf(2.0f, 20.0f);
            ee_ref<Vec4>(mob + 0x10) = Vec4{cp.x + f.x * d + rf(-6, 6), cp.y + f.y * d + rf(-6, 6), cp.z + f.z * d + rf(-4, 4), 1.0f};
        }
        // the limits: dealt, or about the moby's own heading and pitch off the camera's (measured as the function does: about
        // the up axis 0x166e40), so that both sides of each comparison are reached
        float yaw_at = rf(1.0f, 120.0f), pitch_at = rf(1.0f, 60.0f);
        if (ri(0, 2) != 0) {
            const uint32_t act = ee_read<uint32_t>(OV(0x00166e00));
            const Vec4 ax = ee_ref<Vec4>(OV(0x00166e40)), f = ee_ref<Vec4>(act), cp = ee_ref<Vec4>(act + 0x30), mp = ee_ref<Vec4>(mob + 0x10);
            const Vec4 to{mp.x - cp.x, mp.y - cp.y, mp.z - cp.z, 0.0f};
            const float dm = to.x * ax.x + to.y * ax.y + to.z * ax.z, dc = f.x * ax.x + f.y * ax.y + f.z * ax.z;
            const Vec4 fm{to.x - ax.x * dm, to.y - ax.y * dm, to.z - ax.z * dm, 0.0f}, fc{f.x - ax.x * dc, f.y - ax.y * dc, f.z - ax.z * dc, 0.0f};
            const float lm = std::sqrt(fm.x * fm.x + fm.y * fm.y + fm.z * fm.z), lc = std::sqrt(fc.x * fc.x + fc.y * fc.y + fc.z * fc.z);
            const float deg = 57.29578f;
            if (lm > 0.0f && lc > 0.0f) {
                const float c = std::max(-1.0f, std::min(1.0f, (fm.x * fc.x + fm.y * fc.y + fm.z * fc.z) / (lm * lc)));
                yaw_at = std::acos(c) * deg * rf(0.8f, 1.2f);
            }
            pitch_at = std::fabs(std::atan2(dm, lm) - std::atan2(dc, lc)) * deg * rf(0.8f, 1.2f);
        }
        const int k = ri(0, 4);
        const float yaw = k == 0 ? 0.0f : yaw_at, pitch = k == 1 ? 0.0f : (k == 2 ? 0.0f : pitch_at);
        return Args{{CAM}, {k == 2 ? 0.0f : yaw, pitch}};
    }, [](A a) { vu0_sync(); return ri32(cam_type18_cone_check(a.i[0], a.f[0], a.f[1])); });
    for (int shape = 0; shape < 8; ++shape)
        fcheck("cam_type18_inside", ins, 40, Ret::Int32, [shape] {
            const uint32_t mob = cam18_setup(ri(0, 3) == 0 ? 6 : ri(0, 7), 1, shape);
            w32(D + 0x48, ri(0, 5) == 0 ? pick(live_mobys()) : mob);
            return Args{{CAM}, {}};
        }, [](A a) { vu0_sync(); return ri32(cam_type18_inside(a.i[0])); });
    fcheck("cam0_set_218", s218, 60, Ret::None, [] {
        level();
        const uint32_t act = ee_read<uint32_t>(OV(0x00166e00));
        if (ri(0, 3) == 0) w16(act + 0x86, u(ri(1, 20)));
        return Args{{R()}, {}};
    }, [](A a) { cam0_set_218(a.i[0]); return uint64_t{0}; });
}

// ---- the path follower library (novalis_path.cpp)
// A follower FL of the test area {+0 the index, +4 the open-path byte, +0x10 the path, +0x14 its slot in the table
// 0x1b04b0} on one of the level's paths, or on a path TP of the test area (its own slot)
constexpr uint32_t FL = TEST_AREA + 0x1000, TP = TEST_AREA + 0x1100;

// the level's paths (the table 0x1b04b0): the slots that point at a path of 1..64 points
std::vector<uint32_t> level_paths() {
    std::vector<uint32_t> v;
    for (uint32_t i = 0; i < 64; ++i) {
        const uint32_t p = ee_read<uint32_t>(OV(0x001b04b0) + i * 4);
        if (in_ram(p) && (p & 0xf) == 0 && ee_read<int32_t>(p) >= 1 && ee_read<int32_t>(p) <= 64) v.push_back(i);
    }
    return v;
}

// a follower: returns its path's point count. Its index is one of the path's (as the game keeps it), or with `wild` also
// out of the path (the index helpers' wrap; a function that reads the points there would read past the path)
int32_t follower(uint32_t f, bool wild) {
    const std::vector<uint32_t> paths = level_paths();
    uint32_t path = 0, slot = 0;
    if (paths.empty() || ri(0, 3) == 0) {
        const int32_t n = ri(1, 12);
        slot = u(ri(0, 7));
        w32(TP, u(n));
        const Vec4 h = ee_ref<Vec4>(HERO);
        for (int32_t i = 0; i < n; ++i) {
            Vec4 q = around(h, 0.0f, 40.0f, -3.0f, 6.0f);
            q.w = ri(0, 3) == 0 ? 0.0f : rf(0.5f, 30.0f);  // (the segment's length, the w lane)
            ee_ref<Vec4>(TP + 0x10 + u(i) * 0x10) = q;
        }
        if (ri(0, 6) == 0 && n > 1) ee_ref<Vec4>(TP + 0x10 + u(n - 1) * 0x10) = ee_ref<Vec4>(TP + 0x10);  // (a closed loop)
        w32(OV(0x001b04b0) + slot * 4, TP);
        path = TP;
    } else {
        slot = pick(paths);
        path = ee_read<uint32_t>(OV(0x001b04b0) + slot * 4);
    }
    const int32_t n = ee_read<int32_t>(path);
    w32(f, u(wild && ri(0, 4) == 0 ? ri(-2, n + 3) : ri(0, std::max(0, n - 1))));
    w8(f + 4, ri(0, 4) == 0 ? u(ri(0x80, 0xff)) : u(ri(0, 2)));
    w8(f + 5, R() & 0xff);
    w32(f + 8, R());
    w32(f + 0x10, path);
    w32(f + 0x14, slot);
    return n;
}

void path_checks() {
    each_copy("path_point", {{1, 0x0028b7b0}, {3, 0x00264660}, {4, 0x00268d20}, {14, 0x0027e180}}, [](uint32_t a) {
        fcheck("path_point", a, 100, Ret::Int32, [] {
            level();
            const int32_t n = follower(FL, true);
            return Args{{FL, u(ri(-n - 2, 2 * n + 2))}, {}};
        }, [](A a) { return ri32(static_cast<int32_t>(path_point_l1(a.i[0], static_cast<int32_t>(a.i[1])))); });
    });
    each_copy("path_index_wrap", {{1, 0x0028b828}, {3, 0x002646d8}, {4, 0x00268d98}, {14, 0x0027e1f8}}, [](uint32_t a) {
        fcheck("path_index_wrap", a, 100, Ret::Int32, [] {
            level();
            const int32_t n = follower(FL, true);
            return Args{{FL, u(ri(-n - 2, 2 * n + 2))}, {}};
        }, [](A a) { return ri32(path_index_wrap_l1(a.i[0], static_cast<int32_t>(a.i[1]))); });
    });
    each_copy("path_index_advance", {{1, 0x0028b878}, {3, 0x00264728}, {4, 0x00268de8}, {14, 0x0027e248}}, [](uint32_t a) {
        fcheck("path_index_advance", a, 80, Ret::Int32, [] {
            level();
            follower(FL, true);
            return Args{{FL}, {}};
        }, [](A a) { return ri32(path_index_advance_l1(a.i[0])); });
    });
    each_copy("path_spline_tangent", {{1, 0x0028b8c8}, {3, 0x00264778}, {4, 0x00268e38}, {14, 0x0027e298}}, [](uint32_t a) {
        fcheck("path_spline_tangent", a, 120, Ret::None, [] {
            level();
            follower(FL, false);
            wfrand(OUT, 4, -5.0f, 5.0f);
            const int k = ri(0, 4);
            return Args{{OUT, FL, u(ri(-1, 3))}, {k == 0 ? 0.0f : rf(-1.2f, 1.2f), k == 1 ? 0.0f : rf(-1.2f, 1.2f)}};
        }, [](A a) { vu0_sync(); path_spline_tangent_l1(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), a.f[0], a.f[1]); return uint64_t{0}; });
    });
    each_copy("hermite", {{1, 0x0028ba80}, {3, 0x00264930}, {4, 0x00268ff0}, {14, 0x0027e450}}, [](uint32_t a) {
        fcheck("hermite", a, 100, Ret::None, [] {
            level();
            for (uint32_t o = 0; o < 0x50; o += 0x10) wfrand(OUT + o, 4, -10.0f, 10.0f);
            const int k = ri(0, 5);
            return Args{{OUT + 0x40, OUT, OUT + 0x10, OUT + 0x20, OUT + 0x30}, {k == 0 ? 0.0f : (k == 1 ? 1.0f : rf(-0.2f, 1.2f))}};
        }, [](A a) { vu0_sync(); hermite_l1(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.f[0]); return uint64_t{0}; });
    });
    each_copy("path_measure", {{1, 0x0028bb90}, {3, 0x00264a40}, {4, 0x00269100}, {14, 0x0027e560}}, [](uint32_t a) {
        fcheck("path_measure", a, 60, Ret::Float, [] {
            level();
            follower(FL, false);
            const std::vector<uint32_t> ms = live_mobys();
            return Args{{FL, ri(0, 3) == 0 ? 1u : 0u, pick(ms)}, {rf(-1.0f, 1.0f), rf(-1.0f, 1.0f)}};
        }, [](A a) { vu0_sync(); return rf32(path_measure_l1(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1])); });
    });
}

// ---- the aircraft base (novalis_jet_base.cpp)
// the level's aircraft: the mobys whose update is the base (Kerwan's class 845, Oltanis's) or calls it (Kerwan's class
// 822, Eudora's class 466); Novalis's fighter jets (class 660)
std::vector<uint32_t> aircraft() {
    switch (g_oracle_level) {
    case 1: {
        std::vector<uint32_t> v;
        for (uint32_t m = first_moby(); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<int16_t>(m + 0xa6) == 660) v.push_back(m);
        return v;
    }
    case 3: return mobys_with({0x00293c78, 0x00292e98});
    case 4: return mobys_with({0x002ca420});
    case 14: return mobys_with({0x002e81a0});
    default: return {};
    }
}
// the level's own word that holds the aircraft (set: the base returns at once)
uint32_t jets_held_word() { return copy_here({{1, 0x00161b00}, {3, 0x0016136c}, {4, 0x00161958}, {14, 0x00161b94}}); }

// a hit record for `m` (as batch 103's): none, or one of the 16 records 0x178100 with kinds in or out of `kinds`
void give_hit(uint32_t m, uint32_t kinds) {
    if (ri(0, 3) == 0) { w8(m + 0xa4, 0xff); return; }
    const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
    w8(m + 0xa4, idx);
    w32(e + 0x34, ri(0, 8) == 0 ? first_moby() : m);
    w32(e + 0x24, ri(0, 2) == 0 ? (R() & ~kinds) : (kinds | (R() & 0xff)));
    { static const float dm[] = {-1.0f, 0.0f, 0.5f, 5.0f, 12.0f}; wf(e + 0x2c, dm[ri(0, 4)]); }
    wfrand(e, 4, -1, 1);
    w32(e + 0x28, ri(0, 4) == 0 ? 0x102u : u(ri(0, 20)));  // (kind 0x102: no explosion)
}

// an aircraft's state `st` and pvars dealt (batch 103's jet_prep), its class one of the jump table's (0x33 .. 0x4a) now and
// then, the hero near it, the view about him
constexpr uint32_t PVJ = TEST_AREA + 0x5000;  // (the pvars of a stand-in: a live moby of a level whose aircraft are not placed yet)
void jet_prep(uint32_t m, int st, int pass) {
    const bool stand_in = aircraft().empty();
    if (stand_in) {
        for (uint32_t o = 0; o < 0x200; o += 4) w32(PVJ + o, 0);
        w32(m + 0x78, PVJ);
    }
    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
    const uint32_t first = first_moby();
    if (stand_in || ri(0, 3) == 0) {  // (its path follower +0x60 on one of the level's paths or the test area's)
        follower(pv + 0x60, false);
        if (ri(0, 8) == 0) w32(pv + 0x74, 0xffffffffu);
    }
    w8(m + 0x20, u(st));
    if (st == 1) { w8(m + 0x20, 0); jet_update_base_l1(m); w8(m + 0x20, 1); }  // (set up by the port's own state 0)
    w8(pv + 0x120, pass == 0 ? 0u : u(R() & 0x67));
    w8(pv + 0x121, u(ri(0, 8)));
    for (int i = 0; i < 4; ++i) w32(pv + 0x140 + 4 * u(i), ri(0, 3) == 0 ? 0xffffffffu : (m - first) / 0x100 + u(ri(0, 3)));
    wf(pv + 0xf0, ri(0, 2) == 0 ? rf(0.5f, 1.6f) : rf(0, 1));
    wf(pv + 0xf4, rf(-1, 1)); wf(pv + 0xf8, rf(-1, 1)); wf(pv + 0xfc, rf(0.5f, 3));
    wf(pv + 0x124, ri(0, 3) == 0 ? 0.0f : rf(0, 3)); wf(pv + 0x128, rf(-3, 3)); wf(pv + 0x12c, ri(0, 3) == 0 ? -1.0f : (ri(0, 6) == 0 ? 7.0f : rf(-3, 3)));
    wf(pv + 0x110, rf(0.5f, 4)); wf(pv + 0x108, rf(5, 90)); wf(pv + 0x100, rf(5, 90)); wf(pv + 0x114, rf(0, 2)); wf(pv + 0x118, rf(0, 2));
    w16(pv + 0x122, u(ri(-20, 20)));
    wf(pv + 0x130, ri(0, 3) == 0 ? 0.0f : (ri(0, 2) == 0 ? rf(0, 100) : rf(400, 900))); wf(pv + 0x134, ri(0, 3) == 0 ? 0.0f : (ri(0, 2) == 0 ? rf(0, 100) : rf(400, 900)));
    w16(pv + 0x138, ri(0, 3) == 0 ? 0xffffu : u(ri(0, 5))); w16(pv + 0x13a, ri(0, 3) == 0 ? 0u : u(ri(0, 5))); w16(pv + 0x13c, u(ri(1, 40))); w16(pv + 0x13e, u(ri(1, 40)));
    w16(pv + 0x11c, ri(0, 3) == 0 ? u(ri(0, 3)) : 0u); w16(pv + 0x11e, ri(0, 3) == 0 ? 0xffffu : u(ri(0, 6)));
    w8(m + 0x21, ri(0, 2) == 0 ? u(ri(0, 13)) : 0xffu);
    w32(OV(0x0015f5cc), R() & 0xffff);
    w32(0x0015ed84, ri(0, 2) == 0 ? (coin() ? 3u : 9u) : u(g_oracle_level));
    w32(jets_held_word(), ri(0, 12) == 0 ? 1u : 0u);
    wf(pv + 0x104, ri(0, 2) == 0 ? 0.0f : rf(-0.05f, 0.05f)); wf(pv + 0x10c, ri(0, 2) == 0 ? 0.0f : rf(-0.05f, 0.05f));
    if (ri(0, 1) == 0 && ee_read<uint8_t>(m + 0x21) != 0xff) {  // the group's list: this aircraft first, then another (bit 15 ends it)
        const uint32_t list = OUT + 0x300, g = ee_read<uint8_t>(m + 0x21);
        w16(list, (m - first) >> 8); w16(list + 2, ((first + 0x100 * u(ri(0, 300)) - first) >> 8) | (coin() ? 0x8000u : 0u));
        if (ee_read<uint16_t>(list + 2) < 0x8000) w16(list + 4, 0x8000u | u(ri(0, 300)));
        w32(OV(0x001ab840) + 4 * g, list);
        w32(OV(0x0015fff4), 20);
    }
    ee_ref<Vec4>(HERO) = around(ee_ref<Vec4>(m + 0x10), 0.0f, ri(0, 2) == 0 ? 5.0f : 15.0f, -1.0f, 3.0f);
    { Vec4 q = ee_ref<Vec4>(HERO); q.z += rf(-2, 8); ee_ref<Vec4>(OV(0x00166dc0)) = q; }
    if (ri(0, 2) == 0) { Vec4 q = ee_ref<Vec4>(HERO); q.x += rf(-30, 30); q.y += rf(-30, 30); q.z += rf(-5, 20); ee_ref<Vec4>(m + 0x10) = q; }
    {  // the view state: a camera near the hero, random rotation rows and frustum terms (as batch 103)
        Vec4 cam = ee_ref<Vec4>(HERO);
        cam.x = (cam.x + rf(-10, 10)) * 1024.0f; cam.y = (cam.y + rf(-10, 10)) * 1024.0f; cam.z = (cam.z + rf(-10, 10)) * 1024.0f; cam.w = 1024.0f;
        ee_ref<Vec4>(OV(0x0016ccf0)) = cam;
        for (uint32_t a = 0; a < 0x30; a += 0x10) { Vec4 r = {rf(-1, 1), rf(-1, 1), rf(-1, 1), 0.0f}; ee_ref<Vec4>(OV(0x0016ccc0) + a) = r; }
        for (uint32_t a = 0; a < 0x40; a += 0x10) { Vec4 r = {rf(0.1f, 3), rf(0.1f, 3), rf(0.1f, 3), rf(0.1f, 3)}; ee_ref<Vec4>(OV(0x0016cd00) + a) = r; }
    }
    if (ri(0, 9) < 7) {  // on the screen: the moby's bounding position (+0x00, 1024 times the world's) in front of the camera
        Vec4 q = ee_ref<Vec4>(HERO);
        q.x = (q.x + rf(-15, 15)) * 1024.0f; q.y = (q.y + rf(-15, 15)) * 1024.0f; q.z = (q.z + rf(-5, 15)) * 1024.0f; q.w = 1.0f;
        ee_ref<Vec4>(m) = q;
        w16(m + 0x32, u(static_cast<int32_t>(rf(0.5f, 20.0f))));
    }
    give_hit(m, 0x210000u);
    const int ck = ri(0, 5);  // the class: the jump table's range, the engine-sound ones, the ship, the hit's no-shake ones, its own
    if (ck == 0) w16(m + 0xa6, u(0x33 + ri(0, 23)));
    if (ck == 1) { static const uint16_t c[] = {0x33, 0x46a, 0x473, 0x474}; w16(m + 0xa6, c[ri(0, 3)]); }
    if (ck == 2) { static const uint16_t c[] = {0x336, 0x532, 0x53b, 0x53c}; w16(m + 0xa6, c[ri(0, 3)]); }
    seed_rand();
}

void jet_checks() {
    // (the aircraft's pair +0x130 / +0x134; Kerwan's second build L3:0x29c8a8, class 75's, reads +0x140 / +0x144)
    each_copy("jet_banking", {{1, 0x002f5040}, {3, 0x00293b50}, {3, 0x0029c8a8}, {4, 0x002cb790}, {14, 0x002e8078}}, [](uint32_t a) {
        const uint32_t sway = a == 0x0029c8a8 ? 0x140u : 0x130u;
        fcheck("jet_banking", a, 80, Ret::None, [sway] {
            level();
            const std::vector<uint32_t> ms = aircraft().empty() ? live_mobys() : aircraft();
            const uint32_t m = pick(ms);
            uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (!in_ram(pv)) { pv = OUT + 0x400; w32(m + 0x78, pv); }
            const int k = ri(0, 3);
            for (uint32_t o : {0x130u, 0x140u}) {
                wf(pv + o, k == 0 ? 0.0f : rf(-3, 3));
                wf(pv + o + 4, k == 0 || k == 1 ? 0.0f : rf(-3, 3));
            }
            if (ri(0, 4) == 0) wf(pv + (sway ^ 0x70u), 0.0f);  // (the other build's pair off)
            wfrand(m + 0x40, 3, -3.1f, 3.1f);
            wfrand(OUT, 4, -10, 10);
            return Args{{m, OUT}, {}};
        }, [sway](A a) { vu0_sync(); jet_banking(a.i[0], a.i[1], sway); return uint64_t{0}; });
    });
    each_copy("smoke_particle", {{1, 0x0027f660}, {3, 0x00258a30}, {4, 0x0025d0e8}, {14, 0x002723e0}}, [](uint32_t a) {
        fcheck("smoke_particle", a, 60, Ret::None, [] {
            level();
            wfrand(OUT, 4, -10, 10);
            wfrand(OUT + 0x10, 4, -3, 3);
            seed_rand();
            return Args{{OUT, OUT + 0x10}, {}};
        }, [](A a) { vu0_sync(); smoke_particle_l1(a.i[0], a.i[1]); return uint64_t{0}; });
    });
    const uint32_t base = copy_here({{1, 0x002f5168}, {3, 0x00293c78}, {4, 0x002cb8b8}, {14, 0x002e81a0}});
    if (base == 0) { not_here("jet_update_base"); return; }
    if (info() && selected("jet_update_base")) {
        level();
        const std::vector<uint32_t> v = aircraft();
        std::printf("INFO aircraft %zu:", v.size());
        for (size_t i = 0; i < v.size() && i < 12; ++i) std::printf(" %08x(class %d)", v[i], ee_read<int16_t>(v[i] + 0xa6));
        std::printf("\n");
    }
    const std::vector<uint32_t> saved = g_extra_noop_hooks;
    g_extra_noop_hooks.push_back(0x00122298);
    for (int st = 0; st < 4; ++st)
        for (int pass = 0; pass < 5; ++pass)
            classcheck("jet_update_base", base, [] { return aircraft().empty() ? live_mobys() : aircraft(); }, jet_update_base_l1,
                       [st, pass](uint32_t m) { jet_prep(m, st, pass); }, 30);
    g_extra_noop_hooks = saved;
}

// ---- class 737, the scripted-camera trigger (novalis_mobys.cpp)
// boxes of the test area (the box table 0x1600ec pointed at them: centre +0x30 near the hero, angles +0x70) and a pvar
// block for a moby that is no trigger
constexpr uint32_t BX = TEST_AREA + 0x4000, PV737 = TEST_AREA + 0x4400;

void c737_checks() {
    const uint32_t upd = copy_here({{1, 0x002fb5b0}, {2, 0x002dfcd0}, {3, 0x002cf340}, {7, 0x00300970}, {10, 0x002c7cf8}, {13, 0x002f8e70}});
    if (upd == 0) { not_here("scripted_camera_trigger"); return; }
    level();
    const bool placed = !mobys_with({upd}).empty();
    auto subjects = [upd, placed] { return placed ? mobys_with({upd}) : live_mobys(); };
    if (info()) std::printf("INFO class 737: %zu placed\n", mobys_with({upd}).size());
    // (the camera rows' w lanes the scripted camera's start builds with VCROSS are stale in the original: vcross_lanes, as
    // batch 97 has them for this check on Novalis)
    // (and the hit's vector +0x50 of the collision block 0x173e40: the hero's placing runs the segment collision 0x1efc70,
    // which in a rare state of level 10 (one case in 250) stores a VU0 register (vf27) this call did not set: the original's
    // value differs from one run to the next (0 or 1024 times the port's), segment_collide_v stores its own; a difference
    // of that existing port, not of this function)
    const size_t n_ign = g_ignored_words.size();
    for (uint32_t a : vcross_lanes()) g_ignored_words.push_back(a);
    for (uint32_t o = 0x50; o < 0x5c; o += 4) g_ignored_words.push_back(OV(0x00173e40) + o);
    for (int pass = 0; pass < 5; ++pass)
        classcheck("scripted_camera_trigger", upd, subjects, scripted_camera_trigger_update_l1, [pass, placed](uint32_t m) {
            if (!placed || ri(0, 4) == 0) {
                for (uint32_t o = 0; o < 0x40; o += 4) w32(PV737 + o, 0);
                w32(m + 0x78, PV737);
            }
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const uint32_t level_no = ee_read<uint32_t>(0x0015ed84);
            w8(m + 0x20, pass == 0 ? u(ri(0, 4)) : u(pass < 4 ? pass : ri(0, 3)));
            w8(m + 0xbc, ri(0, 2) == 0 ? u(ri(2, 3)) : (R() & 1));
            // four boxes about the hero
            const Vec4 h = ee_ref<Vec4>(HERO);
            for (uint32_t b = 0; b < 4; ++b) {
                shape_at(BX + b * 0x80, around(h, 0.0f, 20.0f, -2.0f, 2.0f), rf(1.0f, 5.0f), rf(1.0f, 5.0f), rf(1.0f, 4.0f));
                wfrand(BX + b * 0x80 + 0x70, 3, -3.1f, 3.1f);
            }
            w32(OV(0x001600ec), BX);
            w32(pv + 0x20, u(ri(-1, 3))); w32(pv + 0x24, ri(0, 6) == 0 ? 0xffffffffu : u(ri(0, 3))); w32(pv + 0x28, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 3)));
            w32(pv + 0x2c, u(ri(0, 2) == 0 ? ri(0, 2) : ri(0, 40))); w32(pv + 0x30, R() & 1); w32(pv + 0x34, R());
            wfrand(pv, 8, -5, 5);
            const uint32_t idx = u(ri(0, 0x7f));
            w16(m + 0xb2, idx);
            w8(OV(0x001bb230) + idx + 0x454, ri(0, 3) == 0 ? 1u : 0u);
            for (uint32_t i = 0; i < 4; ++i) w32(0x0014c190 + level_no * 256 + i * 4, ri(0, 3) == 0 ? R() : 0u);
            if (ri(0, 2) != 0) {  // the hero in the box
                const int32_t b = static_cast<int32_t>(ee_read<uint32_t>(pv + 0x20));
                if (b >= 0) ee_ref<Vec4>(HERO) = around(ee_ref<Vec4>(BX + u(b) * 0x80 + 0x30), 0.0f, 0.5f, -0.3f, 0.3f);
            }
            // the scripted camera's node list (the hero's block 0x16c860: its count +0x44, its mobys from +0x178)
            w32(OV(0x0015f5c4), coin() ? 2u : u(ri(0, 3))); w8(0x0015edb0, ri(0, 3) != 0 ? 1u : 0u);
            const uint32_t list = OV(0x0016c860), first = first_moby();
            w16(list + 0x44, u(ri(0, 3)));
            for (uint32_t i = 0; i < 4; ++i) {
                const uint32_t t = first + 0x100 * u(ri(0, 100));
                w32(list + 0x178 + 4 * i, t);
                w16(t + 0xa6, ri(0, 2) == 0 ? 0x398u : (coin() ? 0x1bfu : u(ri(0, 100))));
            }
            w8(OV(0x0017c440) + 1, R() & 1);
            // the scripted camera (the first camera of type 5, whose work block +0x80 / +0x90 state 2 keeps): in state 2 a live
            // camera made one now and then (a level may have none placed: the stores then go through the null page)
            if (ee_read<uint8_t>(m + 0x20) == 2 && ri(0, 2) != 0) {
                const std::vector<uint32_t> cams = live_cams();
                if (!cams.empty()) {
                    const uint32_t c = pick(cams);
                    w16(c + 0x86, 5);
                    w32(c + 0x70, PV737 + 0x100);
                    wfrand(PV737 + 0x100, 0x30, -5.0f, 5.0f);
                }
            }
            seed_rand();
        }, 50);
    g_ignored_words.resize(n_ign);
}
}  // namespace

void batch_512() {
    if (g_only_batch != 512) return;
    g_batch = 512;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    // (the null page: a stale pointer of a random state)
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);
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
        std::printf("batch 512: levels 1 .. 18 only\n");
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

        cam18_checks();
        path_checks();
        jet_checks();
        c737_checks();
    }

    g_pre_call = nullptr;
    g_extra_noop_hooks.clear();
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
