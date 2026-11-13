// Oracle batch 506 (stage 4 task F3, #149): the class code several levels carry a copy of, each family ported once as shared
// hand-written code (src/game/ovl/shared_*.cpp), against every level's own copy at that level's own address. The level
// copies stay transliterated in their units until each level is re-glued; here every copy is the original and the one
// shared port the port:
//  - shared_spawner.cpp: classes 455 (the Spawner) and 545 (its pods) with their helpers (levels 3, 8, 14);
//  - shared_sandmouse.cpp: class 1633, the Sandmouse's shot, and the Sandmouse's glow (levels 1, 2, 3, 4, 5, 6, 8, 11, 12, 14);
//  - shared_class567.cpp: class 567, a falling fire bomb (levels 1, 8, 9, 15, 17);
//  - shared_class679.cpp: class 679, a current along paths that carries the hero (levels 1, 5, 8, 15);
//  - shared_rock.cpp: the rocks (classes 696..698, 1806 on level 10, 200 on level 15) and their spawn (levels 1, 2, 7, 10, 15, 18);
//  - shared_height_map.cpp: the height map's lookup and the ground's dust puffs (Veldin's 0x263468 / 0x2634f8 / 0x268f10;
//    levels 0, 8, 12, 14);
//  - shared_class258.cpp: class 258, the zones that show / wake / make solid mobys (levels 5, 7, 9, 10, 12, 13, 14, 15, 17);
//  - shared_class341.cpp: class 341, a pad the hero steps on, and its glow (levels 5, 7, 11, 12, 18);
//  - shared_class1135.cpp: class 1135's draw, the teleporter's ring (levels 1, 7, 8, 12, 13, 16, 17, 18);
//  - shared_class252.cpp: class 252's two draws, a beam and a glow (levels 8, 14);
//  - shared_boss_bar.cpp: the bosses' health bar on the HUD (levels 6, 7, 13, 18);
//  - shared_cam19.cpp: camera type 19, a scripted fly-through (levels 10, 13, 14, 15), on a camera of the test area whose
//    record (the level's first) names data of the test area: the level's own paths, zones and mobys, started from a camera
//    record of the test area with the view's rows and place (the matrix source 0x166e04, 0 after the first frame alone);
//  - shared_helpers.cpp: small helpers (each on its own levels: their checks name them), on random records in the test
//    area or the level's own data.
// Subjects: the level's own mobys of the class after the first gameplay frame (the port's; the screen, the projection and the
// view set up), perturbed; the pods (never
// placed) thrown by the shared port's own spawn from a Spawner of the level before the check; the shots (never placed)
// spawned (moby_spawn) at the hero or at a Sandmouse with their pvars dealt; the bombs (class 567) likewise, over the
// hero or a moby of the level; the rocks thrown by the shared spawn (checked first) about the camera; the draw callbacks'
// subjects at the hero (whom the view looks at), their packets into a buffer of the test area. No RAM dumps.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 506, N = each level a family names (0 .. 18); a family the level
// does not carry says so and is not run. B506_INFO=1 prints, per check, how many cases wrote packets beyond a state header
// and, for the cameras, how many turned, moved or changed the view's angle.

#include "ovl_oracle.h"

#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_boss_bar.h>
#include <game/ovl/shared_cam19.h>
#include <game/ovl/shared_class1135.h>
#include <game/ovl/shared_class252.h>
#include <game/ovl/shared_class258.h>
#include <game/ovl/shared_class341.h>
#include <game/ovl/shared_class567.h>
#include <game/ovl/shared_class679.h>
#include <game/ovl/shared_height_map.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_rock.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_spawner.h>
#include <runtime/kernel/disc.h>
#include <runtime/vu/vu0.h>

#include <cmath>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <vector>


namespace rac1::oracle {

namespace {
// The VU0 the original started from (the interpreter's), given to the port before it runs (as batches 128 / 137): the
// collision's records keep a VU0 register's stale lanes
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

// B506_INFO=1: per check, how many of its cases compared real output (the port's run, which the original's matched):
// packets beyond a state header (a quad clipped away writes only its GS state, quad_gs_state's 5 quadwords, and
// sub_002a0ba8's register writes are state too), and for a camera of the test area whether its view turned / moved
bool info() { return std::getenv("B506_INFO") != nullptr; }
// the bytes of the packets between `c0` and `c1` that are not a state header (cnt DMA tags; what does not parse counts)
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
struct Tally {
    int cases = 0, grew = 0, drew = 0, turned = 0, moved = 0, fov = 0;
    uint32_t content = 0;
};
const uint32_t INFO_CAM = TEST_AREA + 0x2000;  // (the cam19 checks' camera)

// A check whose original is the level's own copy at `laddr`.
void fcheck(const char* name, uint32_t laddr, int n, Ret ret, const std::function<Args()>& gen,
            const std::function<uint64_t(const Args&)>& port) {
    if (!selected(name)) return;
    std::printf("%-26s L%d:0x%08x\n", name, g_oracle_level, laddr);
    Tally t;
    const std::function<uint64_t(const Args&)> measured = [&port, &t](const Args& a) {
        if (!info()) return port(a);
        const uint32_t c0 = ee_read<uint32_t>(OV(0x001611c0));
        const bool cam = !a.i.empty() && a.i[0] == INFO_CAM;
        uint8_t rows[0x40];
        if (cam) std::memcpy(rows, ee_get_ram_base() + INFO_CAM, 0x40);
        const uint32_t fov0 = ee_read<uint32_t>(OV(0x0016caf0));
        const uint64_t r = port(a);
        const uint32_t c1 = ee_read<uint32_t>(OV(0x001611c0));
        ++t.cases;
        if (c1 != c0) {
            ++t.grew;
            const uint32_t c = c0 >= 0x00100000u && c1 > c0 && c1 - c0 < 0x100000 ? packet_content(c0, c1) : 0;  // (a null cursor's: the null page)
            if (c != 0) ++t.drew;
            t.content += c;
        }
        if (cam) {
            if (std::memcmp(rows, ee_get_ram_base() + INFO_CAM, 0x30) != 0) ++t.turned;
            if (std::memcmp(rows + 0x30, ee_get_ram_base() + INFO_CAM + 0x30, 0x10) != 0) ++t.moved;
            if (ee_read<uint32_t>(OV(0x0016caf0)) != fov0) ++t.fov;
        }
        return r;
    };
    if (!check_fn(name, laddr, n, ret, gen, measured, Scope::FullRam, 0, 0)) ++g_fail;
    if (info() && (t.grew != 0 || t.turned != 0 || t.moved != 0 || std::strstr(name, "draw") != nullptr || std::strstr(name, "glow") != nullptr ||
                   std::strstr(name, "cam_") != nullptr))
        std::printf("INFO %s L%d: %d cases, packets in %d, beyond a state header in %d (0x%x bytes); camera turned in %d, moved in %d, its view's angle changed in %d\n",
                    name, g_oracle_level, t.cases, t.grew, t.drew, t.content, t.turned, t.moved, t.fov);
}

void not_here(const char* name) {
    if (selected(name)) std::printf("%-26s not on level %d\n", name, g_oracle_level);
}

// the level after its first gameplay frame (the port's) and its view set up, as batches 340 / 400 / 401
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
        // the screen and projection constants, which the oracle's display (no size) leaves broken (FLT_MAX: nothing projects
        // onto the screen): a 640 x 448 screen, as the game's (batch 501's); and the view's matrices (the draw callbacks
        // project through them, as batch 401's)
        if (ee_read<uint16_t>(0x001518d0) == 0) { w16(0x001518d0, 640); w16(0x001518d2, 448); }
        screen_setup();
        projection_setup();
        render_view_setup();
        if (info()) {  // (the view the draws and the cameras start from)
            const Vec4 c = ee_ref<Vec4>(OV(0x00166dc0)), r0 = ee_ref<Vec4>(OV(0x00166fd0)), h = ee_ref<Vec4>(0x0013f3d0);
            const uint32_t src = ee_read<uint32_t>(OV(0x00166e04));
            std::printf("INFO view L%d: camera %.2f %.2f %.2f forward %.3f %.3f %.3f hero %.2f %.2f %.2f; matrix source %08x row0 %.3f %.3f %.3f pos %.2f %.2f %.2f\n",
                        g_oracle_level, c.x, c.y, c.z, r0.x, r0.y, r0.z, h.x, h.y, h.z, src, ee_read<float>(src), ee_read<float>(src + 4),
                        ee_read<float>(src + 8), ee_read<float>(src + 0x30), ee_read<float>(src + 0x34), ee_read<float>(src + 0x38));
            std::printf("INFO words L%d: collision results %08x, segment record %08x, moby table %08x..%08x, particle pool %08x\n", g_oracle_level,
                        OV(0x00173e40), OV(0x00173e90), ee_read<uint32_t>(OV(0x0015ffd8)), ee_read<uint32_t>(OV(0x0015ffe0)),
                        ee_read<uint32_t>(OV(0x0016016c)));
        }
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

// a hit on the moby: its slot in the hit table (0x178100, 64-byte entries) naming it, with a damage mask (the `kinds` bits
// mixed with random ones) and a damage at +0x2c (batch 120's)
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

// The words where the native collision (segment_collide 0x1efc70 / sphere_collide, ovl_1e_1f.cpp, existing code) differs from
// the original: its result rows 0x173e60 .. 0x173eb0 store the w lane 0 where the original stores its VU0 registers' w lanes,
// and the segment record's x / y / z (0x173e90..) are the original's VU0 leftovers of another computation, which the native
// code does not keep (the levels' generated copies of class 1633, the rocks, the puffs and the pods fail the same words with
// these left in: tried on levels 1 .. 4, 12; batches 124 / 140 / 223 leave them out too). Left out in the checks of the
// functions that collide, only.
void collision_lanes() {
    for (uint32_t row = 0x20; row < 0x80; row += 0x10) g_ignored_words.push_back(OV(0x00173e40) + row + 0xc);
    for (uint32_t k = 0; k < 0xc; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
}
// The pvars of the debris pieces the native debris_spawn (existing code) makes: +0xc / +0x1c of each 0x80 from the moby
// table's end, where the original leaves the w lanes of a VU0 sin / cos and the native code 0 (the levels' generated copies
// of class 455 fail the same words: tried on levels 3, 8; batches 120 / 121 / 510 leave them out). For the Spawner, which
// breaks into pieces, only.
void debris_lanes() {
    for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) {
        g_ignored_words.push_back(a + 0xc);
        g_ignored_words.push_back(a + 0x1c);
    }
}

// the hero at `r0`..`r1` from the point
void hero_around(const Vec4& at, float r0, float r1) {
    Vec4 q = at;
    const float r = rf(r0, r1), a = rf(-3.1f, 3.1f);
    q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-1, 3);
    ee_ref<Vec4>(0x0013f3d0) = q;
}

// An update on the subjects `ms` (dealt again when the port's run throws on a pointer outside RAM: a random state that is
// not a game state, as batch 120's classcheck)
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

// the frame's packet cursor (0x1611c0) on a buffer of the test area: after the first gameplay frame alone it is 0 (the
// drawing would write over the null page, which these checks leave out; batch 160 sets the render frame's buffer)
void packet_cursor() { w32(OV(0x001611c0), TEST_AREA + 0x8000 + u(ri(0, 0x40)) * 0x10); }

// a draw callback's check: the display list cursor (0x1611c0) moved a little; B506_INFO=1 tells whether the port drew
void draw_info(const char* name, uint32_t m, void (*port)(uint32_t)) {
    if (std::getenv("B506_INFO") == nullptr) return;
    const uint32_t before = ee_read<uint32_t>(OV(0x001611c0));
    static std::vector<uint8_t> saved;
    uint8_t* ram = ee_get_ram_base();
    saved.assign(ram, ram + 0x02000000);
    port(m);
    std::printf("INFO %s: the list grew by 0x%x\n", name, ee_read<uint32_t>(OV(0x001611c0)) - before);
    std::memcpy(ram, saved.data(), saved.size());
}

// ---- classes 455 / 545 (shared_spawner.cpp)
void spawner_checks() {
    const uint32_t upd455 = copy_here({{3, 0x002bef68}, {8, 0x002e6530}, {14, 0x002e1b20}});
    const uint32_t box = copy_here({{3, 0x002beba0}, {8, 0x002e6168}, {14, 0x002e1758}});
    const uint32_t pod = copy_here({{3, 0x002c45d8}, {8, 0x002ee5b0}, {14, 0x002e7198}});
    const uint32_t upd545 = copy_here({{3, 0x002c4718}, {8, 0x002ee6f0}, {14, 0x002e72d8}});
    const uint32_t revive = copy_here({{3, 0x002bec60}, {8, 0x002e6228}, {14, 0x002e1818}});
    if (upd455 == 0) {
        for (const char* n : {"class455_box_point", "class455_pod_spawn", "class455_update", "class545_revive", "class545_update"}) not_here(n);
        return;
    }
    auto spawners = [upd455] { return mobys_with(upd455); };
    // the Spawner's list of mobys (+0xe8, 18 moby indices): some of its mobys dead (to be brought back), some entries none
    auto vary_list = [](uint32_t pv) {
        for (uint32_t i = 0; i < 18; ++i) {
            const int32_t idx = ee_read<int32_t>(pv + 0xe8 + i * 4);
            if (idx == -1) continue;
            const uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)) + (u(idx) << 8);
            if (ri(0, 3) == 0) w8(o + 0x20, coin() ? 0xfe : 0xfd);
            else if (ri(0, 7) == 0) w32(pv + 0xe8 + i * 4, 0xffffffffu);
        }
    };

    fcheck("class455_box_point", box, 60, Ret::Int32, [] {
        level();
        const uint32_t out = TEST_AREA;
        wrand(out, 4);
        const int32_t nbox = ri(0, 3) == 0 ? -1 : ri(0, 12);
        seed_rand();
        return Args{{u(nbox), out}, {}};
    }, [](A a) { return r32(class455_box_point(static_cast<int32_t>(a.i[0]), a.i[1])); });

    fcheck("class455_pod_spawn", pod, 40, Ret::Int32, [spawners] {
        level();
        const std::vector<uint32_t> ss = spawners();
        const uint32_t owner = ss.empty() ? ee_read<uint32_t>(OV(0x0015ffd8)) : pick(ss);
        const uint32_t pos = TEST_AREA, vel = TEST_AREA + 0x10;
        ee_ref<Vec4>(pos) = ee_ref<Vec4>(owner + 0x10);
        wfrand(vel, 4, -0.5f, 0.5f);
        if (ri(0, 4) == 0) wf(0x0015ed60, rf(0.5f, 3.0f));
        seed_rand();
        return Args{{owner, pos, vel, R() & 0xffff, R() & 0xff}, {}};
    }, [](A a) { return r32(class455_pod_spawn(a.i[0], a.i[1], a.i[2], static_cast<int16_t>(a.i[3]), a.i[4])); });

    // the Spawner's states 0..10 (its jump table), its next state +0x84 (0..7), the pattern +0x85 (0..6), its health, the
    // count / most, the timer and interval, the trigger mode and box, its list, the hero within / beyond its radii (broken,
    // it throws its pieces: the debris' w lanes left out)
    const size_t n_ign455 = g_ignored_words.size();
    if (selected("class455_update")) {
        level();
        debris_lanes();
    }
    for (int pass = 0; pass < 12; ++pass)
        classcheck("class455_update", upd455, spawners, class455_update, [pass, vary_list](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(pass < 11 ? pass : ri(0, 12)));
            w8(pv + 0x84, u(ri(0, 7)));
            w8(pv + 0x85, u(ri(0, 6)));
            if (coin()) wf(pv + 0x20, ri(0, 2) == 0 ? rf(-2.0f, 0.5f) : rf(0.5f, 20.0f));
            if (coin()) w8(pv + 0x87, u(ri(0, 18)));
            if (coin()) vary_list(pv);
            if (ri(0, 3) == 0) ee_ref<Vec4>(OV(0x00166dc0)) = ee_ref<Vec4>(m + 0x10);  // (the camera near: its floor band)
            if (coin()) w16(pv + 0x88, u(ri(0, 2)));
            if (coin()) w16(pv + 0x8a, u(ri(0, 2) == 0 ? 0 : ri(1, 40)));
            if (coin()) w8(pv + 0x82, u(ri(0, 2)));
            if (ri(0, 2) == 0) w8(pv + 0x83, R() & 0xff);
            if (ri(0, 3) == 0) w32(pv + 0xd0, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 20)));
            for (uint32_t i = 0; i < 4; ++i)
                if (ri(0, 3) == 0) w32(pv + 0xa0 + i * 4, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 12)));
            if (ri(0, 3) == 0) w8(m + 0x31, R() & 1);
            give_hit(m, 0xc30000u | (R() & 0x3ffffu));
            const float r = ee_read<float>(pv + 0x90) > 0.0f && ee_read<float>(pv + 0x90) < 200.0f ? ee_read<float>(pv + 0x90) : 10.0f;
            hero_around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? r : 80.0f);
            seed_rand();
        }, 25);

    // the spawns themselves: a spawning state, the hero within the outer radius (mode 0), the timer out, room for more (an
    // interval, the list emptied or kept, the most 18), each pattern 0..5, the boxes set or not, the spread flag
    for (int pattern = 0; pattern < 6; ++pattern)
        classcheck("class455_update", upd455, spawners, class455_update, [pattern, vary_list](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(ri(2, 7)));
            w8(pv + 0x85, u(pattern));
            w8(pv + 0x82, 0);
            w16(pv + 0x88, u(ri(0, 1)));
            w16(pv + 0x8a, ri(0, 3) == 0 ? 0u : u(ri(1, 40)));
            w8(pv + 0x87, 18);
            if (coin()) for (uint32_t i = 0; i < 18; ++i) w32(pv + 0xe8 + i * 4, 0xffffffffu);
            else vary_list(pv);
            w8(pv + 0x83, R() & 0xff);
            for (uint32_t i = 0; i < 4; ++i)
                if (ri(0, 2) != 0) w32(pv + 0xa0 + i * 4, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 12)));
            if (ee_read<float>(pv + 0x9c) < 2.0f || ee_read<float>(pv + 0x9c) > 200.0f) wf(pv + 0x9c, 30.0f);
            if (coin()) wf(pv + 0x90, rf(3.0f, 25.0f));
            give_hit(m, 0);
            w8(m + 0xa4, 0xff);
            hero_around(ee_ref<Vec4>(m + 0x10), 0.0f, ee_read<float>(pv + 0x9c) * 0.9f);
            seed_rand();
        }, 25);
    g_ignored_words.resize(n_ign455);

    // the pods: thrown from a Spawner by the shared spawn (class455_pod_spawn, checked above) at the check's start
    auto pods = [spawners] {
        std::vector<uint32_t> v;
        const std::vector<uint32_t> ss = spawners();
        if (ss.empty()) return v;
        const uint32_t buf = ee_stack_alloc(0x20);
        for (int k = 0; k < 3; ++k) {
            const uint32_t owner = ss[static_cast<size_t>(k) % ss.size()];
            ee_ref<Vec4>(buf) = ee_ref<Vec4>(owner + 0x10);
            wf(buf + 8, ee_read<float>(buf + 8) + 2.0f);
            ee_ref<Vec4>(buf + 0x10) = Vec4{0.1f * static_cast<float>(k), 0.05f, 0.2f, 0.0f};
            const uint32_t p = class455_pod_spawn(owner, buf, buf + 0x10, 300, k == 1 ? 4u : 0u);
            if (p != 0) v.push_back(p);
        }
        ee_stack_free(0x20);
        return v;
    };
    // (the native collision's words, collision_lanes; and the pods' own position w lanes (+0x1c), which take the hit point's
    // w: the original's VU0 leftover, the native collision's 0; the level's generated copy fails the same words: tried on
    // level 3)
    const size_t n_ign = g_ignored_words.size();
    collision_lanes();
    if (selected("class545_update") && upd545 != 0) {
        level();
        for (const uint32_t p : pods()) g_ignored_words.push_back(p + 0x1c);
    }
    for (int pass = 0; pass < 8; ++pass)
        classcheck("class545_update", upd545, pods, class545_update, [pass, vary_list](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const uint32_t owner = ee_read<uint32_t>(pv + 0x10);
            w8(m + 0x20, u(pass < 4 ? pass % 3 : ri(0, 3)));
            if (coin()) vary_list(ee_read<uint32_t>(owner + 0x78));
            Vec4& vel = ee_ref<Vec4>(pv);
            if (coin()) { vel.x = rf(-0.5f, 0.5f); vel.y = rf(-0.5f, 0.5f); vel.z = rf(-0.6f, 0.3f); }
            if (ri(0, 3) == 0) { vel.x *= 0.01f; vel.y *= 0.01f; vel.z *= 0.01f; }  // (slow: a stop on gentle ground)
            if (ri(0, 2) == 0) {  // (on the ground below it, or against a wall)
                Vec4& p = ee_ref<Vec4>(m + 0x10);
                p.z = ee_read<float>(owner + 0x18) + rf(-0.5f, 1.0f);
            }
            if (ri(0, 2) == 0) {  // (next to a live moby of the level's moby list: pushed away from it)
                std::vector<uint32_t> near;
                for (uint32_t o = ee_read<uint32_t>(OV(0x0015ffe4)); o != 0 && near.size() < 200; o = ee_read<uint32_t>(o + 0x28))
                    if (o != m && ee_read<uint8_t>(o + 0x20) < 0xfd) near.push_back(o);
                if (!near.empty()) {
                    const Vec4 q = ee_ref<Vec4>(pick(near) + 0x10);
                    ee_ref<Vec4>(m + 0x10) = Vec4{q.x + rf(-1.5f, 1.5f), q.y + rf(-1.5f, 1.5f), q.z + rf(-0.5f, 0.5f), q.w};
                }
            }
            if (coin()) w16(pv + 0x14, u(ri(0, 3)));
            if (coin()) w32(pv + 0x20, u(ri(0, 2)));
            if (coin()) wf(m + 0x2c, rf(0.0f, 1.2f) * ee_read<float>(ee_read<uint32_t>(m + 0x24) + 0x24));
            if (ri(0, 2) != 0) {  // (a moby it brought, growing)
                const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
                const uint32_t b = ri(0, 3) == 0 ? 0u : first + 0x100 * u(ri(0, 60));
                w32(pv + 0x18, b);
                if (b != 0 && coin()) wf(b + 0x2c, rf(0.0f, 1.0f) * ee_read<float>(ee_read<uint32_t>(b + 0x24) + 0x24));
                w32(pv + 0x1c, R() & 7);
            }
            if (ri(0, 4) == 0) wf(0x0015ed60, rf(0.5f, 3.0f));
            seed_rand();
        }, 25);
    g_ignored_words.resize(n_ign);

    fcheck("class545_revive", revive, 60, Ret::Int32, [spawners, vary_list] {
        level();
        const std::vector<uint32_t> ss = spawners();
        const uint32_t owner = ss.empty() ? 0u : pick(ss);
        if (owner != 0) {
            vary_list(ee_read<uint32_t>(owner + 0x78));
            if (coin()) w16(owner + 0xb4, u(ri(-3, 40)));
            w8(ee_read<uint32_t>(owner + 0x78) + 0x83, R() & 0xff);
        }
        const uint32_t pos = TEST_AREA;
        ee_ref<Vec4>(pos) = owner != 0 ? ee_ref<Vec4>(owner + 0x10) : Vec4{100, 100, 50, 1};
        wf(pos, ee_read<float>(pos) + rf(-10, 10));
        static const int32_t cls[] = {-1, -1, 0x221, 0x1ff, 0x100};
        return Args{{ri(0, 8) == 0 ? 0u : owner, pos, u(cls[ri(0, 4)])}, {}};
    }, [](A a) { return r32(class545_revive(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]))); });
}

// ---- class 1633 (shared_sandmouse.cpp)
void sandmouse_checks() {
    const uint32_t upd1633 = copy_here({{1, 0x0030c9a8}, {2, 0x002efee0}, {3, 0x002e08c8}, {4, 0x002e5928}, {5, 0x0031d760},
                                        {6, 0x003094f0}, {8, 0x00308920}, {11, 0x0031b758}, {12, 0x00309648}, {14, 0x00308be0}});
    if (upd1633 == 0) { not_here("class1633_update"); not_here("sandmouse_glow"); return; }
    // the shots: spawned at a moby of the level (a Sandmouse, class 1818, when one is placed) or at the hero
    auto shots = [] {
        std::vector<uint32_t> v;
        std::vector<uint32_t> mice;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint16_t>(m + 0xa6) == 1818) mice.push_back(m);
        for (int k = 0; k < 3; ++k) {
            const uint32_t s = moby_spawn(1633);
            if (s == 0) break;
            const uint32_t from = mice.empty() ? 0u : mice[static_cast<size_t>(k) % mice.size()];
            ee_ref<Vec4>(s + 0x10) = from != 0 ? ee_ref<Vec4>(from + 0x10) : ee_ref<Vec4>(0x0013f3d0);
            wf(s + 0x18, ee_read<float>(s + 0x18) + 1.0f);
            const uint32_t pv = ee_read<uint32_t>(s + 0x78);
            w32(pv + 0x10, from);
            w16(pv + 0x14, 0x3c);
            v.push_back(s);
        }
        return v;
    };
    // the Sandmouse's glows (class 1818's draw callback): on the level's Sandmice (or any moby: the points are its pvars'),
    // the two points about the camera, in front of it or not
    const uint32_t glow = copy_here({{1, 0x0030de68}, {2, 0x002f0eb0}, {3, 0x002e1898}, {4, 0x002e68f8}, {5, 0x0031e730},
                                     {6, 0x0030adb0}, {8, 0x0030ab50}, {11, 0x0031c728}, {12, 0x0030a618}, {14, 0x00309bb0}});
    fcheck("sandmouse_glow", glow, 40, Ret::None, [] {
        level();
        std::vector<uint32_t> mice;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint16_t>(m + 0xa6) == 1818) mice.push_back(m);
        const uint32_t m = mice.empty() ? ee_read<uint32_t>(OV(0x0015ffd8)) : pick(mice);
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const Vec4 c = ee_ref<Vec4>(0x0013f3d0);  // (about the hero, whom the view looks at, or at the moby)
        for (const uint32_t b : {0x70u, 0x80u})
            ee_ref<Vec4>(pv + b) = ri(0, 3) != 0 ? Vec4{c.x + rf(-3, 3), c.y + rf(-3, 3), c.z + rf(-1, 2), 1.0f} : ee_ref<Vec4>(m + 0x10);
        packet_cursor();
        draw_info("sandmouse_glow", m, sandmouse_glow);
        return Args{{m}, {}};
    }, [](A a) { sandmouse_glow(a.i[0]); return uint64_t{0}; });

    const size_t n_ign = g_ignored_words.size();
    collision_lanes();  // (the native collision's words)
    for (int pass = 0; pass < 6; ++pass)
        classcheck("class1633_update", upd1633, shots, class1633_update, [pass](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            wfrand(pv, 2, -0.3f, 0.3f);  // (the turn rates)
            wf(pv + 8, rf(-1.2f, 1.2f));
            wf(pv + 0xc, rf(-3.1f, 3.1f));
            wf(m + 0x48, rf(-1.2f, 1.2f));
            wf(m + 0x44, rf(-3.1f, 3.1f));
            w16(pv + 0x14, u(pass == 0 ? ri(0, 3) : ri(0, 0x40)));
            if (coin()) wf(m + 0x2c, ee_read<float>(ee_read<uint32_t>(m + 0x24) + 0x24) * rf(0.0f, 1.2f));
            Vec4& p = ee_ref<Vec4>(m + 0x10);
            if (ri(0, 2) == 0) {  // (into the ground, at a wall or at the hero)
                p.z -= rf(0.0f, 2.0f);
                ee_ref<Vec4>(0x0013f3d0) = Vec4{p.x + rf(-1.0f, 1.0f), p.y + rf(-1.0f, 1.0f), p.z, 1.0f};
            }
            if (ri(0, 2) == 0) {  // (inside or next to a solid moby (+0x94 set): a hit on it)
                std::vector<uint32_t> solid;
                for (uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)); o < ee_read<uint32_t>(OV(0x0015ffe0)); o += 0x100)
                    if (o != m && ee_read<uint8_t>(o + 0x20) < 0xfe && ee_read<uint32_t>(o + 0x94) != 0) solid.push_back(o);
                if (!solid.empty()) {
                    const Vec4 q = ee_ref<Vec4>(pick(solid) + 0x10);
                    p = Vec4{q.x + rf(-0.7f, 0.7f), q.y + rf(-0.7f, 0.7f), q.z + rf(0.0f, 1.5f), p.w};
                    ee_ref<Vec4>(OV(0x00166dc0)) = Vec4{p.x + rf(-10, 10), p.y + rf(-10, 10), p.z, 1.0f};
                }
            }
            if (ri(0, 6) == 0) p.x = coin() ? rf(-5.0f, 3.0f) : rf(1019.0f, 1030.0f);
            if (ri(0, 6) == 0) ee_ref<Vec4>(OV(0x00166dc0)) = Vec4{p.x + rf(-90, 90), p.y + rf(-90, 90), p.z, 1.0f};
            if (ri(0, 4) == 0) wf(0x0015ed60, rf(0.5f, 3.0f));
            seed_rand();
        }, 30);
    g_ignored_words.resize(n_ign);
}

// ---- class 567 (shared_class567.cpp)
void class567_checks() {
    const uint32_t upd = copy_here({{1, 0x002eda00}, {8, 0x002efb60}, {9, 0x002f6678}, {15, 0x002d4748}, {17, 0x002d2dc0}});
    if (upd == 0) { not_here("class567_update"); return; }
    // the bombs: spawned over the hero or a live moby of the level (its thrower), falling
    auto bombs = [] {
        std::vector<uint32_t> v;
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        for (int k = 0; k < 3; ++k) {
            const uint32_t b = moby_spawn(567);
            if (b == 0) break;
            const uint32_t from = k == 0 ? 0u : first + 0x100 * u(k * 7);
            const Vec4 at = from != 0 && ee_read<uint8_t>(from + 0x20) < 0xfe ? ee_ref<Vec4>(from + 0x10) : ee_ref<Vec4>(0x0013f3d0);
            ee_ref<Vec4>(b + 0x10) = Vec4{at.x, at.y, at.z + 6.0f, at.w};
            const uint32_t pv = ee_read<uint32_t>(b + 0x78);
            ee_ref<Vec4>(pv) = Vec4{0.05f, -0.03f, -0.1f, 0.0f};
            w32(pv + 0x10, from);
            w32(pv + 0x14, 0x78);
            wf(pv + 0x18, 0.5f);
            w8(b + 0x20, 1);
            v.push_back(b);
        }
        return v;
    };
    for (int pass = 0; pass < 6; ++pass)
        classcheck("class567_update", upd, bombs, class567_update, [pass](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(pass == 0 ? 2 : (pass == 5 ? ri(0, 3) : 1)));
            Vec4& vel = ee_ref<Vec4>(pv);
            vel.x = rf(-0.3f, 0.3f); vel.y = rf(-0.3f, 0.3f); vel.z = rf(-0.6f, 0.1f);
            Vec4& p = ee_ref<Vec4>(m + 0x10);
            if (ri(0, 2) == 0) p.z -= rf(4.0f, 8.0f);  // (at or into the ground: the floor below, a hit)
            if (ri(0, 2) == 0) ee_ref<Vec4>(0x0013f3d0) = Vec4{p.x + rf(-1.0f, 1.0f), p.y + rf(-1.0f, 1.0f), p.z - rf(-1.0f, 2.0f), 1.0f};
            wf(pv + 0x18, rf(0.0f, 2.0f));
            w32(pv + 0x14, u(ri(0, 2) == 0 ? ri(0, 2) : ri(3, 200)));
            if (ri(0, 4) == 0) wf(0x0015ed60, rf(0.5f, 3.0f));
            seed_rand();
        }, 30);
}

// ---- class 679 (shared_class679.cpp)
void class679_checks() {
    const uint32_t upd = copy_here({{1, 0x002f6328}, {5, 0x003067b8}, {8, 0x002f74c0}, {15, 0x002d95e8}});
    if (upd == 0) { not_here("class679_update"); return; }
    for (int pass = 0; pass < 8; ++pass)
        classcheck("class679_update", upd, [upd] { return mobys_with(upd); }, class679_update, [pass](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const uint32_t H = 0x0013f350;
            w32(H + 0x208c, ri(0, 5) == 0 ? u(ri(0, 0x20)) : 0x10u);
            if (pass < 2 || ri(0, 3) == 0) w32(pv + 0x68, 0);
            // the hero near a point of one of its paths (or far)
            const int32_t n = ee_read<int32_t>(pv + 0x70);
            if (n > 0 && ee_read<int32_t>(pv) != -1) {
                const uint32_t path = ee_read<uint32_t>(OV(0x001b04b0) + (ee_read<uint32_t>(pv + u(ri(0, n - 1)) * 4) << 2));
                const int32_t pts = ee_read<int32_t>(path);
                if (pts > 0 && pts < 200) {
                    const Vec4 q = ee_ref<Vec4>(path + 0x10 + u(ri(0, pts - 1)) * 0x10);
                    const float r = ri(0, 3) == 0 ? rf(5.0f, 40.0f) : rf(0.0f, 4.0f);
                    ee_ref<Vec4>(H + 0x80) = Vec4{q.x + rf(-r, r), q.y + rf(-r, r), q.z + rf(-2.0f, 2.0f), 1.0f};
                }
            }
            if (coin()) wf(pv + 0x64, rf(0.5f, 30.0f));
            if (coin()) wf(pv + 0x60, rf(0.0f, 0.3f));
            w32(0x0015ed84, u(ri(0, 2) == 0 ? 0 : 1));
            if (coin()) wf(H + 0x9dc, rf(0.0f, 0.4f));
            if (coin()) wf(H + 0x9e0, rf(0.0f, 3.0f));
            if (coin()) wfrand(H + 0x150, 3, -0.5f, 0.5f);
            if (coin()) wfrand(H + 0xf0, 4, -0.5f, 0.5f);
            if (coin()) w16(H + 0x1e0, u(ri(0, 40)));
            if (ri(0, 4) == 0) wf(0x0015ed60, rf(0.5f, 3.0f));
        }, 30);
}

// ---- the rocks (shared_rock.cpp)
void rock_checks() {
    const uint32_t spawn = copy_here({{1, 0x002f8530}, {2, 0x002dd4d8}, {7, 0x00300150}, {10, 0x002eafc8}, {15, 0x002be0b0}, {18, 0x002dfbc0}});
    const uint32_t upd = copy_here({{1, 0x002f8718}, {2, 0x002dd6c0}, {7, 0x00300338}, {10, 0x002eb1b0}, {15, 0x002be298}, {18, 0x002dfda8}});
    if (upd == 0) { not_here("rock_spawn"); not_here("rock_update"); return; }
    // the rock classes the level's moby_spawn makes with pvars (on level 18 one of the three: the others' spawn gives a moby
    // without pvars or throws)
    std::vector<uint32_t> classes;
    for (const uint32_t c : g_oracle_level == 10 ? std::vector<uint32_t>{1806} : g_oracle_level == 15 ? std::vector<uint32_t>{200} : std::vector<uint32_t>{696, 697, 698}) {
        level();
        uint32_t pv = 0;
        const uint32_t stack_top = ee_stack_ptr();
        try {
            const uint32_t r = moby_spawn(c);
            pv = r != 0 ? ee_read<uint32_t>(r + 0x78) : 0u;
        } catch (const std::exception&) {  // (a class the level does not load: its spawn reads past the class table)
            ee_stack_ptr() = stack_top;
        }
        if (pv >= 0x00100000u && pv < 0x02000000u) classes.push_back(c);
    }
    std::printf("  (rocks: %zu classes the level spawns)\n", classes.size());
    if (classes.empty()) return;
    auto any_class = [classes] { return classes[static_cast<size_t>(ri(0, static_cast<int>(classes.size()) - 1))]; };
    // a place about the camera (or the hero) and a throw
    auto throw_at = [](uint32_t buf) {
        const Vec4 c = coin() ? ee_ref<Vec4>(OV(0x00166dc0)) : ee_ref<Vec4>(0x0013f3d0);
        ee_ref<Vec4>(buf) = Vec4{c.x + rf(-12.0f, 12.0f), c.y + rf(-12.0f, 12.0f), c.z + rf(-2.0f, 4.0f), 1.0f};
        ee_ref<Vec4>(buf + 0x10) = Vec4{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(-0.2f, 0.4f), 0.0f};
    };
    fcheck("rock_spawn", spawn, 40, Ret::Int32, [throw_at, any_class] {
        level();
        throw_at(TEST_AREA);
        if (ri(0, 4) == 0) wf(0x0015ed6c, rf(0.01f, 0.05f));
        seed_rand();
        return Args{{TEST_AREA, TEST_AREA + 0x10, any_class(), u(ri(20, 300)), u(ri(0, 1))},
                    {rf(0.1f, 1.5f), rf(0.2f, 2.0f), rf(0.2f, 2.0f), rf(0.0f, 1.0f)}};
    }, [](A a) { return r32(rock_spawn(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]), a.i[4], a.f[0], a.f[1], a.f[2], a.f[3])); });
    // the rocks: three thrown by the shared spawn about the camera
    auto rocks = [throw_at, classes] {
        std::vector<uint32_t> v;
        const uint32_t buf = ee_stack_alloc(0x20);
        for (uint32_t k = 0; k < 3; ++k) {
            throw_at(buf);
            const uint32_t r = rock_spawn(buf, buf + 0x10, classes[k % classes.size()], 120, k == 2 ? 1u : 0u, 0.75f, 1.0f, 1.0f, 0.1f);
            if (r != 0) v.push_back(r);
        }
        ee_stack_free(0x20);
        return v;
    };
    const size_t n_ign = g_ignored_words.size();
    collision_lanes();  // (the native collision's words)
    for (int pass = 0; pass < 8; ++pass)
        classcheck("rock_update", upd, rocks, rock_update, [pass](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            Vec4& p = ee_ref<Vec4>(m + 0x10);
            if (pass >= 2 && ri(0, 2) != 0) {  // (on the ground below it, or into it: a bounce)
                const float floor = ee_read<float>(0x0013f3d8);
                p.z = floor + rf(-0.5f, 0.5f);
                ee_ref<Vec4>(pv) = Vec4{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(-0.6f, 0.05f), 0.0f};
            }
            if (ri(0, 2) == 0) { Vec4& vel = ee_ref<Vec4>(pv); vel.x *= 0.001f; vel.y *= 0.001f; vel.z *= 0.001f; }  // (at rest)
            w32(pv + 0x1c, u(ri(0, 2) == 0 ? 0 : ri(1, 8)));
            w32(pv + 0x10, u(ri(0, 3) == 0 ? ri(0, 2) : ri(3, 200)));
            w32(pv + 0x20, u(ri(0, 2) == 0 ? 1 : 0));
            if (coin()) w32(pv + 0x28, u(ri(0, 4) == 0 ? 0 : ri(1, 300)));
            wf(pv + 0x18, rf(-0.3f, 0.3f));
            if (ri(0, 6) == 0) p.x = coin() ? rf(-3.0f, 2.5f) : rf(1020.0f, 1025.0f);
            if (ri(0, 4) == 0) wf(0x0015ed6c, rf(0.01f, 0.05f));
            seed_rand();
        }, 30);
    g_ignored_words.resize(n_ign);
}

// ---- the height map and the ground's puffs (shared_height_map.cpp)
void height_map_checks() {
    const uint32_t height = copy_here({{0, 0x00263468}, {8, 0x0026d3e0}, {12, 0x0027bb20}, {14, 0x0026b008}});
    const uint32_t spawn = copy_here({{0, 0x00268f10}, {8, 0x00273080}, {12, 0x002817e8}, {14, 0x00270a78}});
    const uint32_t at = copy_here({{0, 0x002634f8}, {8, 0x0026d470}, {12, 0x0027bbb0}, {14, 0x0026b098}});
    if (height == 0) { not_here("ground_height_at"); not_here("ground_puff_spawn"); not_here("ground_puff_at"); return; }
    // a point about the camera or the hero (on the map's grid: x, y within its width), its height about the ground's
    auto point = [](uint32_t buf) {
        const Vec4 c = coin() ? ee_ref<Vec4>(OV(0x00166dc0)) : ee_ref<Vec4>(0x0013f3d0);
        const float w = static_cast<float>(ee_read<uint32_t>(OV(0x0015fc98)));
        auto inside = [w](float v) { return std::fmin(std::fmax(v, 0.0f), w > 1.0f ? w - 1.0f : 0.0f); };
        ee_ref<Vec4>(buf) = Vec4{inside(c.x + rf(-30.0f, 30.0f)), inside(c.y + rf(-30.0f, 30.0f)), c.z + rf(-15.0f, 8.0f), 1.0f};
    };
    fcheck("ground_height_at", height, 80, Ret::Float, [point] {
        level();
        point(TEST_AREA);
        return Args{{TEST_AREA}, {}};
    }, [](A a) { return rf32(ground_height_at(VV(a.i[0]))); });
    fcheck("ground_puff_spawn", spawn, 40, Ret::None, [point] {
        level();
        point(TEST_AREA);
        if (ri(0, 4) == 0) wf(0x0015ed60, rf(0.5f, 3.0f));
        return Args{{TEST_AREA, R() & 0xff}, {}};
    }, [](A a) { ground_puff_spawn(a.i[0], a.i[1]); return uint64_t{0}; });
    const size_t n_ign = g_ignored_words.size();
    collision_lanes();  // (the native collision's words)
    fcheck("ground_puff_at", at, 80, Ret::None, [point] {
        level();
        point(TEST_AREA);
        if (coin()) {  // (or at the hero's feet, on the world's ground: the segment down from the map's height meets it)
            const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
            ee_ref<Vec4>(TEST_AREA) = Vec4{h.x + rf(-3.0f, 3.0f), h.y + rf(-3.0f, 3.0f), h.z, 1.0f};
        }
        if (coin()) {  // (the camera above the point: the ground below its height + 5)
            Vec4& c = ee_ref<Vec4>(OV(0x00166dc0));
            c.z = ground_height_at(VV(TEST_AREA)) + rf(-6.0f, 20.0f);
        }
        return Args{{TEST_AREA}, {}};
    }, [](A a) { vu0_sync(); ground_puff_at(a.i[0]); return uint64_t{0}; });
    g_ignored_words.resize(n_ign);
}

// ---- class 258 (shared_class258.cpp)
void class258_checks() {
    const uint32_t apply = copy_here({{5, 0x002f4f60}, {7, 0x002f23e8}, {9, 0x002ea098}, {10, 0x002bab98}, {12, 0x002e4a38},
                                      {13, 0x002e4930}, {14, 0x002db0d8}, {15, 0x002c7118}, {17, 0x002c7a10}});
    const uint32_t group = copy_here({{5, 0x002f5190}, {7, 0x002f2618}, {9, 0x002ea2c8}, {10, 0x002badc8}, {12, 0x002e4c68},
                                      {13, 0x002e4b60}, {14, 0x002db308}, {15, 0x002c7348}, {17, 0x002c7c40}});
    const uint32_t upd = copy_here({{5, 0x002f5200}, {7, 0x002f2688}, {9, 0x002ea338}, {10, 0x002bae38}, {12, 0x002e4cd8},
                                    {13, 0x002e4bd0}, {14, 0x002db378}, {15, 0x002c73b8}, {17, 0x002c7cb0}});
    if (upd == 0) { not_here("class258_apply"); not_here("class258_apply_group"); not_here("class258_update"); return; }
    auto zones = [upd] { return mobys_with(upd); };
    // the zone's effect flags dealt (signed bytes -1 / 0 / 1, or any)
    auto flags = [](uint32_t pv) {
        for (const uint32_t o : {0x7cu, 0x7du, 0x7fu}) w8(pv + o, ri(0, 3) == 0 ? (R() & 0xff) : u(ri(-1, 1)));
        if (ri(0, 3) == 0) w8(pv + 0x7e, u(ri(0, 1)));
    };
    // a live moby index of the level, or one of the classes the zone keeps
    auto subject_index = [] {
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), end = ee_read<uint32_t>(OV(0x0015ffe0));
        const int32_t n = static_cast<int32_t>((end - first) >> 8);
        const int32_t idx = ri(0, n > 1 ? n - 1 : 0);
        if (ri(0, 4) == 0) {
            static const uint16_t kept[] = {0x386, 0x3ae, 0x4ef, 0x571};
            w16(first + (u(idx) << 8) + 0xa6, kept[ri(0, 3)]);
        }
        if (ri(0, 6) == 0) w32(first + (u(idx) << 8) + 0x24, 0);
        return idx;
    };
    fcheck("class258_apply", apply, 80, Ret::None, [zones, flags, subject_index] {
        level();
        const std::vector<uint32_t> zs = zones();
        const uint32_t pv = zs.empty() ? TEST_AREA : ee_read<uint32_t>(pick(zs) + 0x78);
        flags(pv);
        return Args{{u(subject_index()), pv, u(ri(0, 1))}, {}};
    }, [](A a) { class258_apply(static_cast<int32_t>(a.i[0]), a.i[1], a.i[2]); return uint64_t{0}; });
    fcheck("class258_apply_group", group, 60, Ret::None, [zones, flags] {
        level();
        const std::vector<uint32_t> zs = zones();
        const uint32_t pv = zs.empty() ? TEST_AREA : ee_read<uint32_t>(pick(zs) + 0x78);
        flags(pv);
        int32_t g = ri(0, 40);
        for (int k = 0; k < 12; ++k) {  // (mostly one of the zone's own groups)
            const int32_t own = ee_read<int32_t>(pv + 0x20 + u(ri(0, 11)) * 4);
            if (own != -1 && coin()) { g = own; break; }
        }
        return Args{{0, pv, u(g), u(ri(0, 1))}, {}};
    }, [](A a) { class258_apply_group(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), a.i[3]); return uint64_t{0}; });
    for (int pass = 0; pass < 6; ++pass)
        classcheck("class258_update", upd, zones, class258_update, [pass, flags](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (coin()) flags(pv);
            w8(m + 0x20, u(pass == 0 ? 0 : ri(0, 2)));
            w8(m + 0xbc, u(ri(0, 1)));
            if (ri(0, 3) == 0) wf(pv + 4, ri(0, 2) == 0 ? 0.0f : rf(1.0f, 30.0f));
            // the hero (and the camera) at the zone, at one of its boxes, or away
            Vec4 at = ee_ref<Vec4>(m + 0x10);
            const int32_t box = ee_read<int32_t>(pv + 8 + u(ri(0, 5)) * 4);
            if (box != -1 && coin()) at = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(box) * 0x80 + 0x30);
            const float r = ri(0, 2) == 0 ? rf(20.0f, 120.0f) : rf(0.0f, 6.0f);
            ee_ref<Vec4>(0x0013f3d0) = Vec4{at.x + rf(-r, r), at.y + rf(-r, r), at.z + rf(-2.0f, 2.0f), 1.0f};
            if (coin()) ee_ref<Vec4>(OV(0x00166dc0)) = Vec4{at.x + rf(-r, r), at.y + rf(-r, r), at.z + rf(-2.0f, 4.0f), 1.0f};
        }, 30);
}

// ---- class 341 (shared_class341.cpp)
void class341_checks() {
    const uint32_t glow = copy_here({{5, 0x002f81b8}, {7, 0x002f5640}, {11, 0x002f31e8}, {12, 0x002ec5b0}, {18, 0x002ce938}});
    const uint32_t upd = copy_here({{5, 0x002f8080}, {7, 0x002f5508}, {11, 0x002f30b0}, {12, 0x002ec478}, {18, 0x002ce800}});
    if (upd == 0) { not_here("class341_glow"); not_here("class341_update"); return; }
    auto pads = [upd] { return mobys_with(upd); };
    auto vary = [](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t H = 0x0013f350;
        w8(m + 0x53, u(ri(0, 4)));
        w8(m + 0x70, R() & 0xff);
        w8(m + 0xbc, u(ri(0, 2)));
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        if (ri(0, 4) == 0) w32(pv, coin() ? 0xffffffffu : u(ri(0, 80)));
        const int32_t idx = ee_read<int32_t>(pv);
        if (idx >= 0 && idx < 0x400) w8(first + (u(idx) << 8) + 0xbc, u(ri(0, 3)));
        w32(H + 0x2fc, ri(0, 2) == 0 ? 0u : m);
        w32(0x001413dc, coin() ? 0x13u : u(ri(0, 0x20)));
        w32(0x001403e0, ri(0, 3) == 0 ? 0u : first);
        w32(0x00140408, coin() ? 0x16u : u(ri(0, 0x20)));
        wf(pv + 0x1c, ri(0, 3) == 0 ? 0.0f : rf(0.0f, 1.0f));
        wf(pv + 0x18, rf(-3.1f, 3.1f));
        if (ri(0, 4) == 0) wf(0x0015ed6c, rf(0.01f, 0.05f));
    };
    fcheck("class341_glow", glow, 60, Ret::None, [pads, vary] {
        level();
        const std::vector<uint32_t> ps = pads();
        const uint32_t m = ps.empty() ? ee_read<uint32_t>(OV(0x0015ffd8)) : pick(ps);
        vary(m);
        return Args{{m}, {}};
    }, [](A a) { class341_glow(a.i[0]); return uint64_t{0}; });
    for (int pass = 0; pass < 4; ++pass)
        classcheck("class341_update", upd, pads, class341_update, vary, 30);
}

// ---- the small helpers (shared_helpers.cpp)
void helper_checks() {
    {
        const uint32_t a = copy_here({{0, 0x00266858}, {3, 0x00254a10}, {5, 0x002907b8}, {7, 0x0028ec18}, {10, 0x00259700},
                                      {11, 0x0028bc28}, {13, 0x00272260}, {14, 0x0026e1c0}, {16, 0x00261c98}});
        if (a == 0) not_here("talker_word_set");
        else fcheck("talker_word_set", a, 40, Ret::None, [] {
            level();
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), end = ee_read<uint32_t>(OV(0x0015ffe0));
            const uint32_t m = first + 0x100 * u(ri(0, static_cast<int>((end - first) >> 8) - 1));
            return Args{{ri(0, 4) == 0 ? ee_read<uint32_t>(0x001413d0) : m, R()}, {}};
        }, [](A a) { talker_word_set(a.i[0], a.i[1]); return uint64_t{0}; });
    }
    {
        const uint32_t reset = copy_here({{1, 0x002b7a48}, {5, 0x002cc428}, {7, 0x002ca980}, {11, 0x002c6bd0}, {12, 0x002ba560}, {13, 0x002ae368}});
        const uint32_t fixed = copy_here({{1, 0x002b7c68}, {5, 0x002cc648}, {7, 0x002caba0}, {11, 0x002c6df0}, {12, 0x002ba780}, {13, 0x002ae588}});
        const uint32_t scale = copy_here({{1, 0x002b7ef0}, {5, 0x002cc8d0}, {7, 0x002cae28}, {11, 0x002c7078}, {12, 0x002baa08}, {13, 0x002ae810}});
        if (reset == 0) {
            not_here("grid_records_reset"); not_here("grid_point_fixed"); not_here("grid_weights_scale");
        } else {
            fcheck("grid_records_reset", reset, 20, Ret::None, [] {
                level();
                wrand(TEST_AREA, 0x3500 / 4);
                return Args{{TEST_AREA, u(ri(-1, 3))}, {}};
            }, [](A a) { grid_records_reset(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
            fcheck("grid_point_fixed", fixed, 60, Ret::None, [] {
                level();
                wfrand(TEST_AREA, 3, -300.0f, 1300.0f);
                wrand(TEST_AREA + 0x20, 4);
                if (coin()) { wf(OV(0x001ca880), rf(0.1f, 8.0f)); wf(OV(0x001ca888), rf(-500.0f, 500.0f)); wf(OV(0x001ca88c), rf(-500.0f, 500.0f)); }
                return Args{{TEST_AREA, TEST_AREA + 0x20}, {}};
            }, [](A a) { grid_point_fixed(a.i[0], a.i[1]); return uint64_t{0}; });
            fcheck("grid_weights_scale", scale, 40, Ret::None, [] {
                level();
                if (coin()) wfrand(OV(0x001ca9e0), 92, -2.0f, 2.0f);
                return Args{{}, {rf(-3.0f, 3.0f)}};
            }, [](A a) { grid_weights_scale(a.f[0]); return uint64_t{0}; });
        }
    }
    {
        const uint32_t a = copy_here({{3, 0x00250a78}, {4, 0x00255150}, {6, 0x00271ac0}, {10, 0x00255a40}, {15, 0x00251c90}});
        if (a == 0) not_here("jump_arc_set");
        else fcheck("jump_arc_set", a, 60, Ret::None, [] {
            level();
            wfrand(TEST_AREA, 0x20, -2.0f, 2.0f);
            wf(TEST_AREA + 0x10, ri(0, 6) == 0 ? 0.0f : rf(0.1f, 30.0f));
            return Args{{TEST_AREA}, {rf(0.0f, 10.0f), ri(0, 6) == 0 ? -1.0f : rf(0.0f, 2.0f)}};
        }, [](A a) { jump_arc_set(a.i[0], a.f[0], a.f[1]); return uint64_t{0}; });
    }
    {
        const uint32_t a = copy_here({{2, 0x0025c758}, {3, 0x00248b78}, {5, 0x002843a8}, {12, 0x00272fe0}, {15, 0x002499d8}});
        if (a == 0) not_here("band_set");
        else fcheck("band_set", a, 30, Ret::None, [] {
            level();
            wfrand(TEST_AREA, 0x24, -1000.0f, 1000.0f);
            return Args{{TEST_AREA}, {}};
        }, [](A a) { band_set(a.i[0]); return uint64_t{0}; });
    }
    {
        const uint32_t a = copy_here({{2, 0x0020bc88}, {4, 0x001ed6b8}, {6, 0x00212938}, {8, 0x0020d760}, {11, 0x0022c5b8},
                                      {12, 0x0021d810}, {13, 0x00206738}, {18, 0x00203a38}});
        if (a == 0) not_here("gs_quad_packet");
        else fcheck("gs_quad_packet", a, 40, Ret::None, [] {
            level();
            wrand(TEST_AREA, 0x30);
            w32(OV(0x001611c0), TEST_AREA + 0x1000 + u(ri(0, 15)) * 0x10);
            return Args{{TEST_AREA, TEST_AREA + 0x40, TEST_AREA + 0x80, R(), u(ri(0, 1))}, {}};
        }, [](A a) { gs_quad_packet(a.i[0], a.i[1], a.i[2], static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(a.i[3]))), a.i[4]); return uint64_t{0}; });
    }
    {
        const uint32_t a = copy_here({{8, 0x00211b08}, {12, 0x00221ec8}, {14, 0x00212598}, {15, 0x001f8730}, {17, 0x001fc060}});
        if (a == 0) not_here("float_timer_tick");
        else fcheck("float_timer_tick", a, 60, Ret::Int32, [] {
            level();
            static const float v[] = {0.0f, -0.0f, -0.5f, 0.01f, 0.016f, 0.017f, 1.0f, 5.0f};
            wf(TEST_AREA, coin() ? v[ri(0, 7)] : rf(-1.0f, 1.0f));
            if (coin()) wf(0x0015ed7c, rf(0.0f, 0.05f));
            return Args{{TEST_AREA}, {}};
        }, [](A a) { return ri32(float_timer_tick(a.i[0])); });
    }
    {
        const uint32_t a = copy_here({{8, 0x002d4050}, {14, 0x002dae50}});
        if (a == 0) { not_here("class252_hook_c"); return; }
        if (!selected("class252_hook_c")) return;
        level();
        std::vector<uint32_t> own;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
            const uint32_t cls = ee_read<uint32_t>(m + 0x24);
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + 0xc) == a) own.push_back(m);
        }
        if (own.empty()) { std::printf("%-26s L%d:0x%08x  no moby owns it on this level\n", "class252_hook_c", g_oracle_level, a); return; }
        std::printf("  (class252_hook_c: %zu owners, class %u)\n", own.size(), ee_read<uint16_t>(own[0] + 0xa6));
        g_extra_noop_hooks.push_back(0x002763f8);  // (batch 120's)
        fcheck("class252_hook_c", a, 40, Ret::Int32, [own] {
            level();
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
            const uint32_t m = pick(own);
            if (ri(0, 4) == 0) w8(m + 0x20, coin() ? 0xfe : 0xfd);
            w8(m + 0x53, u(ri(0, 3)));
            const uint32_t tgt = ri(0, 3) == 0 ? 0u : first + 0x100 * u(ri(0, 100));
            return Args{{m, ri(0, 2) == 0 ? 0x0013f3d0u : m + 0x10, tgt}, {rf(-1, 3)}};
        }, [](A a) { return ri32(class252_hook_c(a.i[0], a.i[1], a.i[2], a.f[0])); });
        g_extra_noop_hooks.pop_back();
    }
}

// ---- the small helpers, part 2 (shared_helpers.cpp): the stub, the class mates' marks, the carry state hooks
// (a trial run of the port deals the state again when it throws on a pointer outside RAM, as classcheck does)
uint32_t trial_ok(const std::function<void()>& port) {
    static std::vector<uint8_t> saved;
    uint8_t* ram = ee_get_ram_base();
    saved.assign(ram, ram + 0x02000000);
    bool ok = true;
    const uint32_t stack_top = ee_stack_ptr();
    try { port(); } catch (const std::exception&) { ok = false; }
    ee_stack_ptr() = stack_top;
    std::memcpy(ram, saved.data(), saved.size());
    return ok ? 1u : 0u;
}
void helper_checks2() {
    struct Lv { int level; uint32_t addr; uint32_t arg; };  // (arg: the copy's joint, or state)
    for (const LvAddr& c : {LvAddr{1, 0x002c72c0}, LvAddr{5, 0x00326280}, LvAddr{5, 0x0032a6d8}, LvAddr{6, 0x002be860}, LvAddr{8, 0x002025d0},
                            LvAddr{8, 0x002025d8}, LvAddr{8, 0x002ba660}, LvAddr{8, 0x003127d0}, LvAddr{9, 0x002d0b18}, LvAddr{12, 0x00212638},
                            LvAddr{14, 0x00202db8}, LvAddr{15, 0x002aa948}, LvAddr{16, 0x002f0d88}, LvAddr{16, 0x002f51e0}, LvAddr{17, 0x002af398},
                            LvAddr{17, 0x002fd208}})
        if (c.level == g_oracle_level)
            fcheck("zero_stub", c.addr, 5, Ret::Int32, [] { level(); return Args{{R(), R()}, {}}; }, [](A) { return ri32(zero_stub()); });
    // the list 0x16c860 (+0x178 the mobys, +0x44 their count) filled with `m` and live mobys of its class or others
    auto mates = [](uint32_t& m) {
        level();
        std::vector<uint32_t> live;
        for (uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)); o < ee_read<uint32_t>(OV(0x0015ffe0)); o += 0x100)
            if (ee_read<uint8_t>(o + 0x20) < 0xfe && ee_read<uint32_t>(o + 0x24) != 0) live.push_back(o);
        m = pick(live);
        const uint32_t list = OV(0x0016c860);
        const int32_t n = ri(0, 6);
        w16(list + 0x44, u(n));
        for (int32_t i = 0; i < n; ++i) {
            uint32_t o = pick(live);
            if (coin()) o = m;
            for (int k = 0; k < 20 && ri(0, 2) != 0 && ee_read<uint16_t>(o + 0xa6) != ee_read<uint16_t>(m + 0xa6); ++k) o = pick(live);
            w32(list + 0x178 + u(i) * 4, o);
        }
        w32(OV(0x0015f5c4), ri(0, 4) == 0 ? u(ri(0, 3)) : 2u);
        w8(0x0015edb0, u(ri(0, 4) == 0 ? 0 : 1));
        w8(OV(0x0017c440) + 1, u(ri(0, 3) == 0 ? 1 : 0));
    };
    auto mark_check = [mates](const char* name, uint32_t addr, uint32_t joint, void (*port)(uint32_t, uint32_t)) {
        fcheck(name, addr, 30, Ret::None, [mates, joint, port] {
            uint32_t m = 0;
            for (int k = 0; k < 40; ++k) {
                mates(m);
                if (trial_ok([port, m, joint] { port(m, joint); }) != 0) break;
            }
            return Args{{m}, {}};
        }, [port, joint](A a) { port(a.i[0], joint); return uint64_t{0}; });
    };
    for (const Lv& c : {Lv{2, 0x002e0cd8, 0}, Lv{2, 0x002e1868, 0}, Lv{4, 0x002e2f90, 0}, Lv{5, 0x00317fb8, 0}, Lv{8, 0x00302c00, 0},
                        Lv{11, 0x002d0ec0, 0}, Lv{11, 0x002f0d58, 0}, Lv{12, 0x002e6b60, 1}, Lv{14, 0x002fb938, 0}, Lv{14, 0x002feef8, 0},
                        Lv{15, 0x002ec678, 1}, Lv{16, 0x002e30a8, 1}, Lv{16, 0x002e6720, 0}})
        if (c.level == g_oracle_level) mark_check("class_mates_mark_fixed", c.addr, c.arg, class_mates_mark_fixed);
    for (const Lv& c : {Lv{3, 0x002db480, 1}, Lv{5, 0x00317398, 0}, Lv{6, 0x00300f98, 1}, Lv{11, 0x002d0638, 0}, Lv{12, 0x002eb498, 1}})
        if (c.level == g_oracle_level) mark_check("class_mates_mark_level", c.addr, c.arg, class_mates_mark_level);

    {
        const uint32_t a = copy_here({{3, 0x00250ae8}, {10, 0x00255ab0}, {15, 0x00251d00}, {18, 0x00264c10}});
        if (a == 0) not_here("spark_burst");
        else fcheck("spark_burst", a, 30, Ret::None, [] {
            level();
            std::vector<uint32_t> live;
            for (uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)); o < ee_read<uint32_t>(OV(0x0015ffe0)); o += 0x100)
                if (ee_read<uint8_t>(o + 0x20) < 0xfe) live.push_back(o);
            const uint32_t m = pick(live);
            ee_ref<Vec4>(TEST_AREA) = ee_ref<Vec4>(m + 0x10);
            wfrand(TEST_AREA + 0x10, 4, -0.2f, 0.2f);
            if (ri(0, 4) == 0) wf(0x0015ed60, rf(0.5f, 3.0f));
            seed_rand();
            return Args{{m, TEST_AREA, coin() ? 0u : TEST_AREA + 0x10}, {}};
        }, [](A a) { spark_burst(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
    }

    for (const Lv& c : {Lv{3, 0x002c6c90, 0xe}, Lv{6, 0x002fc630, 3}, Lv{6, 0x00303150, 2}, Lv{6, 0x00304088, 5}, Lv{14, 0x002fe228, 2},
                        Lv{14, 0x002ffc08, 1}, Lv{16, 0x002de3a8, 1}})
        if (c.level == g_oracle_level)
            fcheck("moby_state_hook", c.addr, 10, Ret::Int32, [] {
                level();
                return Args{{ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(0, 40))}, {}};
            }, [c](A a) { return r32(moby_state_hook(a.i[0], c.arg)); });

    struct Hk { int level; uint32_t addr; uint32_t which, carried; int32_t released; };
    for (const Hk& h : {Hk{8, 0x002d4000, 8, 5, 1}, Hk{12, 0x002e1758, 4, 8, 1}, Hk{12, 0x002e17a8, 8, 8, 1}, Hk{14, 0x002dae00, 8, 5, 1},
                        Hk{18, 0x002d6190, 4, 4, -1}, Hk{18, 0x002d61e0, 8, 4, -1}, Hk{18, 0x002fcbb0, 0, 7, 2}, Hk{18, 0x002fcc00, 4, 7, 2},
                        Hk{18, 0x002fcc50, 8, 7, 2}}) {
        if (h.level != g_oracle_level || !selected("carry_state_hook")) continue;
        level();
        std::vector<uint32_t> own;  // (the mobys whose class's hook table names it)
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
            const uint32_t cls = ee_read<uint32_t>(m + 0x24);
            if (ee_read<uint8_t>(m + 0x20) >= 0xfe || cls == 0 || ee_read<uint32_t>(cls + 0x2c) == 0) continue;
            for (uint32_t s = 0; s < 0x18; s += 4)
                if (ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + s) == h.addr) { own.push_back(m); break; }
        }
        if (own.empty()) { std::printf("%-26s L%d:0x%08x  no moby owns it on this level\n", "carry_state_hook", g_oracle_level, h.addr); continue; }
        g_extra_noop_hooks.push_back(0x002763f8);
        fcheck("carry_state_hook", h.addr, 40, Ret::Int32, [own, h] {
            level();
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
            const uint32_t m = pick(own);
            w8(m + 0x20, coin() ? h.carried : u(ri(0, 9)));
            if (ri(0, 2) == 0) {  // (the carry record's state 8: the carry hook lets go, returns 0)
                const uint32_t r = class_hook_10(m);
                if (r != 0) w16(r + 0x68, 8);
            }
            w8(m + 0xbc, u(ri(0, 9)));
            const uint32_t tgt = ri(0, 3) == 0 ? 0u : first + 0x100 * u(ri(0, 100));
            return Args{{m, ri(0, 2) == 0 ? 0x0013f3d0u : m + 0x10, tgt}, {rf(-1, 3)}};
        }, [h](A a) { return ri32(carry_state_hook(a.i[0], a.i[1], a.i[2], a.f[0], h.which, h.carried, h.released)); });
        g_extra_noop_hooks.pop_back();
    }
}

// ---- class 1135's draw (shared_class1135.cpp)
void class1135_checks() {
    const uint32_t drw = copy_here({{1, 0x003094f0}, {7, 0x0031ce40}, {8, 0x00304bc0}, {12, 0x00301600}, {13, 0x003051a0},
                                    {16, 0x002e0a38}, {17, 0x002e5848}, {18, 0x002ee490}});
    if (drw == 0) { not_here("class1135_draw"); return; }
    fcheck("class1135_draw", drw, 60, Ret::None, [] {
        level();
        std::vector<uint32_t> rings, live;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
            if (ee_read<uint8_t>(m + 0x20) >= 0xfe) continue;
            live.push_back(m);
            if (ee_read<uint16_t>(m + 0xa6) == 1135) rings.push_back(m);
        }
        const uint32_t m = !rings.empty() ? pick(rings) : pick(live);
        if (rings.empty()) w32(m + 0x78, TEST_AREA + 0x800);  // (no ring placed: pvars of its own)
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        wf(pv + 0x34, ri(0, 2) == 0 ? rf(0.0f, 0.7f) : rf(0.5f, 3.0f));
        w8(m + 0x20, u(ri(0, 9)));
        if (ri(0, 3) != 0) {  // (the ring at the hero, whom the view looks at: drawn)
            const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
            ee_ref<Vec4>(m + 0x10) = Vec4{h.x + rf(-2, 2), h.y + rf(-2, 2), h.z + rf(-1, 1), 1.0f};
        }
        packet_cursor();
        draw_info("class1135_draw", m, class1135_draw);
        return Args{{m}, {}};
    }, [](A a) { vu0_sync(); class1135_draw(a.i[0]); return uint64_t{0}; });
}

// ---- class 252's draws (shared_class252.cpp)
void class252_checks() {
    const uint32_t beam = copy_here({{8, 0x002d3878}, {14, 0x002da678}});
    const uint32_t glow = copy_here({{8, 0x002d4108}, {14, 0x002daf08}});
    if (beam == 0) { not_here("class252_draw_beam"); not_here("class252_draw_glow"); return; }
    auto subject = [] {
        level();
        std::vector<uint32_t> own, live;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
            if (ee_read<uint8_t>(m + 0x20) >= 0xfe) continue;
            live.push_back(m);
            if (ee_read<uint16_t>(m + 0xa6) == 252) own.push_back(m);
        }
        const uint32_t m = pick(own.empty() ? live : own);
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (ri(0, 3) != 0) {  // (at the hero, whom the view looks at)
            const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
            ee_ref<Vec4>(m + 0x10) = Vec4{h.x + rf(-2, 2), h.y + rf(-2, 2), h.z + rf(-1, 1), 1.0f};
        }
        const Vec4 p = ee_ref<Vec4>(m + 0x10);
        w32(pv + 0x218, pick(live));
        ee_ref<Vec4>(pv + 0x1d0) = Vec4{rf(-1, 1), rf(-1, 1), rf(-1, 1), rf(0.1f, 1.5f)};
        ee_ref<Vec4>(pv + 0x1e0) = Vec4{rf(-1, 1), rf(-1, 1), rf(-1, 1), rf(0.1f, 1.5f)};
        ee_ref<Vec4>(pv + 0x220) = Vec4{p.x + rf(-2, 2), p.y + rf(-2, 2), p.z + rf(0, 3), 1.0f};
        w32(m + 0x90, R());
        packet_cursor();
        return m;
    };
    fcheck("class252_draw_beam", beam, 50, Ret::None, [subject] {
        const uint32_t m = subject();
        draw_info("class252_draw_beam", m, class252_draw_beam);
        return Args{{m}, {}};
    }, [](A a) { vu0_sync(); class252_draw_beam(a.i[0]); return uint64_t{0}; });
    fcheck("class252_draw_glow", glow, 50, Ret::None, [subject] {
        const uint32_t m = subject();
        draw_info("class252_draw_glow", m, class252_draw_glow);
        return Args{{m}, {}};
    }, [](A a) { vu0_sync(); class252_draw_glow(a.i[0]); return uint64_t{0}; });
}

// ---- the bosses' health bar (shared_boss_bar.cpp)
void boss_bar_checks() {
    const uint32_t drw = copy_here({{6, 0x00249738}, {7, 0x00262078}, {13, 0x00245cf8}, {18, 0x0023cd60}});
    if (drw == 0) { not_here("boss_health_bar_draw"); return; }
    fcheck("boss_health_bar_draw", drw, 80, Ret::None, [] {
        level();
        const uint32_t m = TEST_AREA + 0x400;  // (the bar reads the boss's +8, +0x70, +0x74 only)
        w32(m + 8, u(ri(0, 6) == 0 ? 0 : ri(1, 400)));
        w32(m + 0x74, u(ri(0, 4) == 0 ? 0 : ri(-5, 420)));
        w8(m + 0x70, u(ri(0, 4) == 0 ? 0 : ri(1, 12)));
        static const uint32_t layouts[] = {0, 1, 7, 0xd, 0x12, 3};
        w32(0x0015ed84, layouts[ri(0, 5)]);
        w32(0x0015ed80, u(ri(0, 1)));
        if (coin()) w32(OV(0x0015f8e8), u(ri(100, 600)));
        packet_cursor();
        draw_info("boss_health_bar_draw", m, boss_health_bar_draw);
        return Args{{m}, {}};
    }, [](A a) { boss_health_bar_draw(a.i[0]); return uint64_t{0}; });
}

// ---- camera type 19 (shared_cam19.cpp)
void cam19_checks() {
    const uint32_t zone = copy_here({{10, 0x002f5a78}, {13, 0x00316f80}, {14, 0x003141c8}, {15, 0x002f7958}});
    const uint32_t zone2 = copy_here({{14, 0x00314e20}});
    const uint32_t accept = copy_here({{10, 0x002f65f8}, {13, 0x00317b00}, {14, 0x00314d48}, {15, 0x002f84d8}});
    const uint32_t init = copy_here({{10, 0x002f5b58}, {13, 0x00317060}, {14, 0x003142a8}, {15, 0x002f7a38}});
    const uint32_t upd = copy_here({{10, 0x002f5f18}, {13, 0x00317420}, {14, 0x00314668}, {15, 0x002f7df8}});
    if (upd == 0) {
        for (const char* n : {"cam_type19_zone", "cam_type19_accept", "cam_type19_init", "cam_type19_update"}) not_here(n);
        return;
    }
    // the level's paths (2..64 points)
    level();
    std::vector<uint32_t> paths;
    for (uint32_t i = 0; i < 64; ++i) {
        const uint32_t p = ee_read<uint32_t>(OV(0x001b04b0) + i * 4);
        if (p >= 0x00100000u && p < 0x02000000u && ee_read<int32_t>(p) >= 2 && ee_read<int32_t>(p) <= 64) paths.push_back(i);
    }
    std::printf("  (cam_type19: %zu paths)\n", paths.size());
    if (paths.empty()) return;
    const uint32_t CAM = TEST_AREA + 0x2000, ST = TEST_AREA + 0x2100, D = TEST_AREA + 0x2200, OTHER = TEST_AREA + 0x2300, SRC = TEST_AREA + 0x2400;
    // the camera (record 0 of the level, its data D), its state block ST, its data dealt
    auto setup = [paths, CAM, ST, D, OTHER, SRC] {
        level();
        for (uint32_t o = 0; o < 0x400; o += 4) w32(CAM + o, 0);
        w32(ee_read<uint32_t>(OV(0x0015ef50)) + 0x1c, D);
        w16(CAM + 0x84, 0);
        w32(CAM + 0x70, ST);
        w8(CAM + 0x7c, u(ri(0, 3)));
        // the camera's matrix source (the camera a type 19 starts from, which its init copies): after the first frame alone
        // it is 0, the null page's zero rows; a camera record of the test area with the view's rows (0x166fd0, turned about z
        // at random) and its place (0x166dc0, moved a little), which the test camera starts from or not (its rows zero or
        // random: the init's copy seen)
        {
            const uint32_t src = SRC;
            const float a = rf(-3.1f, 3.1f), ca = std::cos(a), sa = std::sin(a);
            for (uint32_t r = 0; r < 3; ++r) {
                const Vec4 v = ee_ref<Vec4>(OV(0x00166fd0) + r * 0x10);
                ee_ref<Vec4>(src + r * 0x10) = coin() ? v : Vec4{ca * v.x - sa * v.y, sa * v.x + ca * v.y, v.z, v.w};
            }
            const Vec4 c = ee_ref<Vec4>(OV(0x00166dc0));
            ee_ref<Vec4>(src + 0x30) = Vec4{c.x + rf(-2, 2), c.y + rf(-2, 2), c.z + rf(-1, 1), c.w};
            w32(OV(0x00166e04), src);
        }
        const int start = ri(0, 2);
        for (uint32_t r = 0; r < 4; ++r) {
            if (start == 0) ee_ref<Vec4>(CAM + r * 0x10) = ee_ref<Vec4>(SRC + r * 0x10);
            else if (start == 1) wfrand(CAM + r * 0x10, 3, -1.0f, 1.0f);
        }
        const int kind = ri(0, 4);
        w32(D + 0xc, kind == 0 ? u(ri(0, 10)) : 0xffffffffu);
        w32(D + 0x10, kind == 1 ? u(ri(0, 4)) : 0xffffffffu);
        w32(D + 8, kind == 2 ? u(ri(0, 4)) : 0xffffffffu);
        w32(D + 0x14, kind == 3 ? pick(paths) : 0xffffffffu);
        w32(D + 0x20, pick(paths));
        w32(D + 0x24, pick(paths));
        if (coin()) {  // (the camera path's view angles, its points' w: none, an angle in degrees, or above 180 the starting view)
            const uint32_t path = ee_read<uint32_t>(OV(0x001b04b0) + ee_read<uint32_t>(D + 0x20) * 4);
            for (int32_t k = 0; k < ee_read<int32_t>(path); ++k)
                wf(path + 0x1c + u(k) * 0x10, ri(0, 2) == 0 ? 0.0f : coin() ? rf(20.0f, 170.0f) : rf(181.0f, 400.0f));
        }
        w32(D + 0x28, coin() ? 0xffffffffu : u(ri(0, 40)));
        wfrand(D + 0x2c, 3, -2.0f, 2.0f);
        w8(D + 0x38, u(ri(0, 1)));
        w8(D + 0x39, u(ri(0, 4) == 0 ? 1 : 0));
        w16(D + 0x3a, u(ri(0, 3) == 0 ? 0 : ri(1, 300)));
        wf(D + 0x3c, rf(0.5f, 20.0f));
        w32(D + 0x40, u(ri(0, 1)));
        w32(D + 0x44, coin() ? 0xffffffffu : ee_read<uint32_t>(0x001413d4));
        w32(D + 0x48, coin() ? 0xffffffffu : (coin() ? ee_read<uint32_t>(0x001413dc) : 7u));
        w32(D + 0x4c, u(ri(0, 1)));
        // the hero on the camera path's points or the zone polygon's, or away
        const uint32_t path = ee_read<uint32_t>(OV(0x001b04b0) + ee_read<uint32_t>(kind == 3 && coin() ? D + 0x14 : D + 0x20) * 4);
        const Vec4 q = ee_ref<Vec4>(path + 0x10 + u(ri(0, ee_read<int32_t>(path) - 1)) * 0x10);
        const float r = coin() ? rf(0.0f, 3.0f) : rf(10.0f, 60.0f);
        ee_ref<Vec4>(0x0013f3d0) = Vec4{q.x + rf(-r, r), q.y + rf(-r, r), q.z + rf(-1, 2), 1.0f};
        // (or, half the time, at the centre of the camera's zone, a box, cylinder or sphere of the level's tables, or near
        // it: inside, on its edge, out)
        if (kind < 3 && coin()) {
            const uint32_t table = ee_read<uint32_t>(kind == 0 ? OV(0x001600ec) : kind == 1 ? OV(0x001600fc) : OV(0x001600f4));
            const uint32_t idx = ee_read<uint32_t>(kind == 0 ? D + 0xc : kind == 1 ? D + 0x10 : D + 8);
            if (table >= 0x00100000u && table < 0x02000000u) {
                const Vec4 c = ee_ref<Vec4>(table + (idx << 7) + 0x30);
                const float e = coin() ? 0.2f : rf(0.5f, 6.0f);
                ee_ref<Vec4>(0x0013f3d0) = Vec4{c.x + rf(-e, e), c.y + rf(-e, e), c.z + rf(-e, e), 1.0f};
            }
        }
        for (uint32_t o = 0; o < 0xa0; o += 4) w32(OTHER + o, 0);
        w16(OTHER + 0x86, coin() ? 0x13u : u(ri(0, 0x20)));
        w16(OTHER + 0x7e, u(ri(0, 1)));
        w8(OTHER + 0x7c, u(ri(0, 3)));
        seed_rand();
    };
    fcheck("cam_type19_zone", zone, 60, Ret::Int32, [setup, CAM] { setup(); return Args{{CAM}, {}}; },
           [](A a) { return ri32(cam_type19_zone(a.i[0])); });
    if (zone2 != 0)
        fcheck("cam_type19_zone", zone2, 60, Ret::Int32, [setup, CAM] { setup(); return Args{{CAM}, {}}; },
               [](A a) { return ri32(cam_type19_zone(a.i[0])); });
    fcheck("cam_type19_accept", accept, 100, Ret::Int32, [setup, CAM, OTHER] { setup(); return Args{{CAM, coin() ? OTHER : 0u}, {}}; },
           [](A a) { return ri32(cam_type19_accept(a.i[0], a.i[1])); });
    fcheck("cam_type19_init", init, 60, Ret::None, [setup, CAM] { setup(); return Args{{CAM}, {}}; },
           [](A a) { cam_type19_init(a.i[0]); return uint64_t{0}; });
    // the update: the camera started by the shared init (checked above), then its phase, fade, timer, leg varied (the w lanes
    // of the camera's side and up rows: VCROSS's stale lane in the original, which the native vec3_cross_ba (existing code)
    // does not keep, as vcross_lanes says; the levels' generated copies fail the same two words: tried on levels 10, 13)
    const size_t n_ign = g_ignored_words.size();
    g_ignored_words.push_back(CAM + 0x1c);
    g_ignored_words.push_back(CAM + 0x2c);
    for (int pass = 0; pass < 6; ++pass)
        fcheck("cam_type19_update", upd, 40, Ret::None, [setup, CAM, ST, pass] {
            setup();
            cam_type19_init(CAM);
            const uint32_t blk = ST + 0x40;
            w16(blk + 0x14, u(pass < 4 ? pass : ri(0, 5)));
            wf(OV(0x0015f3fc), ri(0, 3) == 0 ? rf(0.9f, 1.0f) : rf(0.0f, 1.0f));
            if (coin()) w16(blk + 0x16, u(ri(0, 30)));
            const uint32_t d = TEST_AREA + 0x2200;
            const uint32_t path = ee_read<uint32_t>(OV(0x001b04b0) + ee_read<uint32_t>(d + 0x20) * 4);
            const int32_t n = ee_read<int32_t>(path);
            w32(blk, u(ri(0, n - 1)));
            wf(blk + 4, rf(0.0f, 1.0f));
            ee_ref<Vec4>(CAM + 0x30) = ee_ref<Vec4>(path + 0x10 + ee_read<uint32_t>(blk) * 0x10);
            if (coin()) wf(blk + 0x10, rf(0.0f, ee_read<float>(blk + 8)));
            if (coin()) wf(0x0015ed6c, rf(0.01f, 0.05f));
            for (uint32_t k = 0; k < 8; ++k)  // (view points of the path, some)
                if (ri(0, 2) == 0) { w16(ST + 0x58 + k * 2, u(std::min(n - 1, static_cast<int32_t>(k) * 2))); wf(ST + 0x68 + k * 4, rf(20.0f, 120.0f)); }
            seed_rand();
            return Args{{CAM}, {}};
        }, [](A a) { vu0_sync(); cam_type19_update(a.i[0]); return uint64_t{0}; });
    g_ignored_words.resize(n_ign);
}
}  // namespace

void batch_506() {
    if (g_only_batch != 506) return;
    g_batch = 506;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
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
    // (no global list of words left out beyond the cameras' VCROSS lanes and the pose function's spills: the few words of
    // existing code's known differences are left out inside the checks that reach them, collision_lanes / debris_lanes)
    // (the kernel's calls the original would make for real: the CD / movie path of a level change request; batch 120's)
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80};
    // the port's VU0 micro memory as the game leaves it, and the interpreter's VU0 kept for the port (as batch 137)
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

    spawner_checks();
    sandmouse_checks();
    class567_checks();
    class679_checks();
    rock_checks();
    height_map_checks();
    class258_checks();
    class341_checks();
    helper_checks();
    helper_checks2();
    class1135_checks();
    class252_checks();
    boss_bar_checks();
    cam19_checks();

    //@@NEXT
    g_pre_call = nullptr;
    g_extra_noop_hooks.clear();
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
