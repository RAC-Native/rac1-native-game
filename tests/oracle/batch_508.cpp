// Oracle batch 508 (stage 4 task F5, #149): class code and helpers several levels carry a copy of, each family ported once as
// shared hand-written code (src/game/ovl/shared_*.cpp), against every level's own copy at that level's own address. The level
// copies stay transliterated in their units until each level is re-glued; here every copy is the original and the one shared
// port the port. Run it on each level that has a copy: ORACLE_LEVEL=<n> build/tests/ovl_oracle --batch 508 for n in 0, 2 .. 8
// and 10 .. 17 (needs the ISO; no dumps); a check prints "not on level N" on a level without its copy. The level snapshot is
// the level after its first gameplay frame (the port's) with the view set up (render_view_setup); the draws' packet cursor is
// put on a buffer of the test area so that their packets are compared, and each draw check says how many of its cases drew
// (B508_INFO=1 prints it per case); the port gets the VU0 registers the original started from (vu0_sync). Class updates run on
// the level's own mobys of the class (or mobys spawned for the check), through every state, perturbed; helpers on arguments
// built from the level's state; the frame scale (0x15ed68) at its 50 Hz value in a quarter of the cases. No broad list of
// left-out words: the one check that meets a known difference of existing code (the collision records, class 344's)
// leaves out its own words, with the reason.
//
// The shared ports and their copies (each level's address):
//   class1179_update     class 1179      L11:0x0030ee00 L15:0x002d9100
//   class933_update      class 933       L14:0x002ff640 L16:0x002ddde0
//   class933_spawn       (helper)        L14:0x002ffaf8 L16:0x002de298
//   class933_carrier_start / _hold / _sparks (helpers of class 923 on L14, 1401 on L16)
//                                        L14:0x002fe900 / 0x002fec28 / 0x002feca8, L16:0x002e3e20 / 0x002e4110 / 0x002e4190
//   class235_update      class 235       L3:0x002bae48 L16:0x002c3be8 (on trails spawned for the check)
//   class439_update      class 439       L5:0x002f87a8 L16:0x002c7788
//   hoverboard_boost_pads (helper of class 717 on L5, 556 on L16)  L5:0x00307010 L16:0x002cfc00
//   race_hud_start / _update / _reset / _draw (the race's HUD element)  L5:0x00262ae8 / 0x002661e8 / 0x002661a0 / 0x00266710,
//                                        L16:0x002351f8 / 0x002388f8 / 0x002388b0 / 0x00238e20 (on a HUD slot)
//   class344_update      class 344 .. (L8), 980 (L5)   L5:0x003186f8 L8:0x002dba40 (on pieces thrown for the check)
//   class344_throw       (helper)        L5:0x00318648 L8:0x002db990
//   floater_bob          (raft helper)   L5:0x00319740 L8:0x002dc4a0 L14:0x002f1490 L16:0x002e4d88
//   floater_ride / _lay_path             L5:0x003195f8 / 0x003198e8, L14:0x002f1360 / 0x002f1638, L16:0x002e4c40 / 0x002e4f30
//   floater_message / _find              L5:0x00319690 / 0x00319598, L16:0x002e4cd8 / 0x002e4be0
//   (the rafts: class 998 on L5, 424 on L8, 712 on L14, 1410 on L16; each level's pvars offsets passed as the levels' copies use them)
//   the small helpers of shared_little.cpp, each on its own copies (the addresses in the checks and in shared_little.h):
//   moby_is_in_state, moby_alive_in_state, moby_largest, moby_show_default, moby_rest_sparkles, cam_entry_mark,
//   cam_entry_mark_type19, camera_angles_set, class_anim_last_time / _first_time, particle_33_spawn, class848_spawn,
//   carry_keep_state_hook, carry_drop_hook, kerwan_class816_turn / _class1012_turn, blarg_class1051_switch
//   cutscene_l14_effect / _draw (Oltanis's arrival sparks and glow)  L0:0x0023e008 / 0x0023db30, L14:0x00245e98 / 0x002459c0
//   beam_particle_update (particle 0x50) L0:0x00275eb0 L5:0x002a00f0 L7:0x0029e4a0
//   particle_43_spawn    L0:0x002743a8 L5:0x0029e698 L16:0x0026f7e8
//   particle_3a_spawn    L0:0x002729c8 L7:0x0029ae58
//   particle_28_spawn    L0:0x0026fd28 L10:0x00262b08
//   path_point_even      L0:0x00262360 L10:0x002554e0
//   class1221_update     classes 1221 / 1222  L3:0x002de370 / 0x002de458 L7:0x0031d9f8 / 0x0031dae0 L12:0x00301d08 / 0x00301df0
//                                        L15:0x002e5e70 / 0x002e5f58 (on pieces spawned for the check)
//   class1371_update / _spawn / _throw   L12:0x00308918 / 0x00308670 / 0x00308708, L15:0x002ea5e0 / 0x002ea338 / 0x002ea3d0
//   class823_update / _draw              L5:0x0030c0a8 / 0x0030c220, L7:0x00304738 / 0x003048b0
//   class1511_draw       (draw callback) L5:0x0031cf58 L14:0x00308150
//   panel_lights         base 0: L2:0x002ea8b0 L6:0x002f4cd8 L15:0x002ebb50 L17:0x002f1568; base 0x10: L10:0x00298940 L15:0x002ea9b0
//   class819_launch      L10:0x002cc1b8 L15:0x002dd600
//   envmap_uvs4          L17:0x002f54d8 / 0x002f57f0 / 0x002f5b08 / 0x002f5e20 / 0x002f6450
//   path_mover_start     L14:0x002fc890 / 0x002fd808
//   quartu_waves_draw_a / _b             L15:0x002edbe0 / 0x002edfc0
//   fleet_waves_draw_a / _b              L17:0x002f0948 / 0x002f0d08
//   blarg_class1083_draw_a / _b / _c / _d  L6:0x003003f0 / 0x00300588 / 0x00300720 / 0x00300958
//   blarg_class1062_draw_b / _c            L6:0x002fd460 / 0x002fd748 (the camera put before one of the mesh's points)

#include "ovl_oracle.h"

#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_blarg_meshes.h>
#include <game/ovl/shared_class1179.h>
#include <game/ovl/shared_class1221.h>
#include <game/ovl/shared_class1371.h>
#include <game/ovl/shared_class1511.h>
#include <game/ovl/shared_class235.h>
#include <game/ovl/shared_class344.h>
#include <game/ovl/shared_class823.h>
#include <game/ovl/shared_class933.h>
#include <game/ovl/shared_cutscene_fx.h>
#include <game/ovl/shared_floaters.h>
#include <game/ovl/shared_hoverboard.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/shared_panel_lights.h>
#include <game/ovl/shared_particle_kinds.h>
#include <game/ovl/shared_waves.h>
#include <runtime/kernel/disc.h>
#include <runtime/vu/vu0.h>

#include <cmath>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <tuple>
#include <vector>

namespace rac1::oracle {

namespace {
// The VU0 the original started from (the interpreter's), given to the port before it runs (as batches 506 / 510)
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
        render_view_setup();
        g_snap.assign(ram, ram + 0x02000000);
        g_snap_level = g_oracle_level;
    } else {
        std::memcpy(ram, g_snap.data(), g_snap.size());
    }
}

uint32_t first_moby() { return ee_read<uint32_t>(OV(0x0015ffd8)); }

// the level's live mobys whose update function is `fn` (the level's address)
std::vector<uint32_t> mobys_with(uint32_t fn) {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
    return v;
}

// the level's live mobys of class `cls`
std::vector<uint32_t> mobys_of(uint32_t cls) {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint16_t>(m + 0xa6) == cls) v.push_back(m);
    return v;
}

// any live moby of the level
std::vector<uint32_t> live_mobys() {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < ee_read<uint32_t>(OV(0x0015ffe0)) && v.size() < 400; m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) < 0xfe) v.push_back(m);
    return v;
}

uint32_t pick(const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; }

// the subjects `fn` updates: the level's own (placed), or `k` mobys of class `cls` spawned at the hero
std::vector<uint32_t> placed_or_spawned(uint32_t fn, uint32_t cls, int k = 3) {
    std::vector<uint32_t> v = mobys_with(fn);
    if (!v.empty()) return v;
    for (int i = 0; i < k; ++i) {
        const uint32_t s = moby_spawn(cls);
        if (s == 0) break;
        ee_ref<Vec4>(s + 0x10) = ee_ref<Vec4>(0x0013f3d0);
        wf(s + 0x10, ee_read<float>(s + 0x10) + rf(-3, 3));
        wf(s + 0x14, ee_read<float>(s + 0x14) + rf(-3, 3));
        v.push_back(s);
    }
    return v;
}

// a hit on the moby: its slot in the hit table (0x178100, 64-byte entries) naming it, with a damage mask (the `kinds` bits
// mixed with random ones) and a damage at +0x2c (batch 506's)
void give_hit(uint32_t m, uint32_t kinds) {
    if (ri(0, 3) == 0) { w8(m + 0xa4, 0xff); return; }
    const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
    w8(m + 0xa4, idx);
    w32(e + 0x34, ri(0, 8) == 0 ? first_moby() : m);
    w32(e + 0x24, ri(0, 2) == 0 ? (R() & ~kinds) : (kinds | (R() & 0xff)));
    { static const float dm[] = {-1.0f, 0.0f, 0.5f, 5.0f, 12.0f}; wf(e + 0x2c, dm[ri(0, 4)]); }
    wfrand(e, 4, -1, 1);
    w32(e + 0x28, u(ri(0, 20)));
}

// the hero at `r0`..`r1` (flat) from the point
void hero_around(const Vec4& at, float r0, float r1) {
    Vec4 q = at;
    const float r = rf(r0, r1), a = rf(-3.1f, 3.1f);
    q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-1, 3);
    ee_ref<Vec4>(0x0013f3d0) = q;
}

// the frame scale (0x15ed68) at the 50 Hz value sometimes (frame_scale_i's counts then differ from the 60 Hz ones)
void vary_frame_scale() {
    if (ri(0, 3) == 0) wf(0x0015ed68, 1.2f);
}

// An update on the subjects `ms` (dealt again when the port's run throws on a pointer outside RAM: a random state that is
// not a game state, as batch 506's classcheck)
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
            vary_frame_scale();
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
// drawing would write over the null page, which these checks leave out)
void packet_cursor() { w32(OV(0x001611c0), TEST_AREA + 0x8000 + u(ri(0, 0x40)) * 0x10); }

// A draw's cases: how many drew more than `header` bytes (the port run on a copy of the state). Printed after the check.
struct DrawCount {
    int cases = 0, drawn = 0;
};
DrawCount g_draw;
void draw_probe(const char* name, uint32_t header, const std::function<void()>& run) {
    const uint32_t before = ee_read<uint32_t>(OV(0x001611c0));
    static std::vector<uint8_t> saved;
    uint8_t* ram = ee_get_ram_base();
    saved.assign(ram, ram + 0x02000000);
    const uint32_t stack_top = ee_stack_ptr();
    try { run(); } catch (const std::exception&) {}
    ee_stack_ptr() = stack_top;
    const uint32_t grew = ee_read<uint32_t>(OV(0x001611c0)) - before;
    ++g_draw.cases;
    if (grew > header) ++g_draw.drawn;
    if (std::getenv("B508_INFO") != nullptr) std::printf("INFO %s: the list grew by 0x%x\n", name, grew);
    std::memcpy(ram, saved.data(), saved.size());
}
void draw_report(const char* name) {
    if (selected(name) && g_draw.cases != 0) std::printf("  (%s: %d of %d cases drew)\n", name, g_draw.drawn, g_draw.cases);
    g_draw = DrawCount{};
}

// ---- class 1179 (shared_class1179.cpp)
void class1179_checks() {
    const uint32_t upd = copy_here({{11, 0x0030ee00}, {15, 0x002d9100}});
    if (upd == 0) { not_here("class1179_update"); return; }
    auto subjects = [upd] { return placed_or_spawned(upd, 1179); };
    // states 0, 1 (and one that is neither); the level number (Pokitaru or not: both copies test for 11), the uid and index,
    // the level state's / checkpoint's bytes and the table bit, the hero standing on it in state 0x22 with the flags, near or
    // not, his mode, the help records' counts and times, the game time
    for (int pass = 0; pass < 12; ++pass)
        classcheck("class1179_update", upd, subjects, class1179_update, [pass](uint32_t m) {
            const bool help = pass >= 8;  // (the help messages: state 1, a uid, the hero near in mode 0 / 1, the queue free)
            w8(m + 0x20, help ? 1u : u(pass < 6 ? pass % 2 : ri(0, 3)));
            if (coin()) w32(0x0015ed84, coin() ? 11u : 15u);
            w8(m + 0xb0, !help && ri(0, 3) == 0 ? 0xffu : u(ri(0, 0x7f)));
            const uint32_t idx = u(ri(0, 0x1ff));
            w16(m + 0xb2, idx);
            if (coin()) w8(OV(0x001bb230) + idx + 0x454, coin() ? 0u : u(ri(1, 4)));
            if (coin()) w8(OV(0x001ba5d0) + idx + 0x454, coin() ? 0u : u(ri(1, 4)));
            if (coin()) w32(0x0014c190 + (11u << 8) + ((idx >> 5) << 2), R());
            const uint32_t uid = ee_read<uint8_t>(m + 0xb0);
            if (uid != 0xff) {
                if (coin()) w8(OV(0x0015fc88) + uid, coin() ? 0xffu : uid);
                if (coin()) w8(0x0014c050 + uid + (11u << 4), coin() ? 0xffu : u(ri(0, 3)));
            }
            if (ri(0, 2) != 0) w32(0x0013f64c, m);
            if (ri(0, 3) != 0) w8(0x0013d4c3, u(ri(0, 1)));
            if (ri(0, 3) == 0) w16(0x0013f65e, u(ri(0, 2)));
            if (ri(0, 2) != 0) w32(0x001413d4, ri(0, 3) == 0 ? 0x21u : 0x22u);
            if (help || ri(0, 3) != 0) hero_around(ee_ref<Vec4>(m + 0x10), 0.0f, help || coin() ? 3.9f : 6.0f);
            w32(0x001413dc, u(help ? ri(0, 1) : ri(0, 3)));
            for (const uint32_t rec : {0x00141968u + 0x6c * 8, 0x00141968u + 0x71 * 8}) {
                if (coin()) w16(rec, u(ri(0, 2)));
                if (coin()) w16(rec + 2, u(ri(0, 3) == 0 ? 0 : ri(0, 40)));
                if (coin()) w32(rec + 4, R());
            }
            if (coin()) w32(0x0015eea4, u(ri(0, 30000)));
            if (help || ri(0, 2) == 0) {  // (the dialogue queue free and no stream on: sub_00203908 takes the message)
                w32(OV(0x00179410), 0);
                w32(OV(0x00179410) + 0x24, 0xffffffffu);
                w32(0x001516d0 + 0x50, 0);
                w32(0x001516d0 + 0x1c, 0xffffffffu);
            }
            seed_rand();
        }, 25);
}

// ---- class 933 and its carrier's helpers (shared_class933.cpp)
void class933_checks() {
    const uint32_t upd = copy_here({{14, 0x002ff640}, {16, 0x002ddde0}});
    const uint32_t spawn = copy_here({{14, 0x002ffaf8}, {16, 0x002de298}});
    const uint32_t start = copy_here({{14, 0x002fe900}, {16, 0x002e3e20}});
    const uint32_t hold = copy_here({{14, 0x002fec28}, {16, 0x002e4110}});
    const uint32_t sparks = copy_here({{14, 0x002feca8}, {16, 0x002e4190}});
    const uint32_t carrier = copy_here({{14, 0x002fe2a0}, {16, 0x002e37a0}});  // (class 923 / 1401's update)
    if (upd == 0) {
        for (const char* n : {"class933_update", "class933_spawn", "class933_carrier_start", "class933_carrier_hold", "class933_carrier_sparks"}) not_here(n);
        return;
    }
    auto subjects = [upd] { return placed_or_spawned(upd, 933); };
    // the carriers: the level's own, or mobys of the carrier's class spawned at the hero
    auto carriers = [carrier] { return placed_or_spawned(carrier, g_oracle_level == 14 ? 923u : 1401u, 2); };
    // states 0..3; a moby it waits on (gone or not); the level (Kalebo's rise or not), its float height unset; spawned or
    // placed (the hero's mode 15 or not); a hit of the kinds (its state word, the hitter of class 0x283 or not); the hero's
    // +0x23c naming it; its place on / off screen (state 2)
    for (int pass = 0; pass < 10; ++pass)
        classcheck("class933_update", upd, subjects, class933_update, [pass](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(pass < 8 ? pass % 4 : ri(0, 4)));
            if (coin()) {
                const std::vector<uint32_t> all = live_mobys();
                const uint32_t o = pick(all);
                w32(pv, ri(0, 2) == 0 ? 0u : o);
                if (coin()) w8(o + 0x20, coin() ? 0xffu : 0x80u);
            }
            if (ri(0, 2) == 0) w32(0x0015ed84, coin() ? 14u : 16u);
            if (coin()) wf(pv + 4, coin() ? -1.0f : rf(-2, 60));
            wfrand(pv + 8, 1, -0.5f, 0.5f);
            if (coin()) w32(0x0013f350 + 0x208c, coin() ? 0xfu : u(ri(0, 20)));
            if (coin()) w32(0x0013f350 + 0x23c, coin() ? m : 0u);
            give_hit(m, 0x230000u);
            if (ee_read<uint8_t>(m + 0xa4) != 0xff) {  // (the hitter: of class 0x283 or another)
                const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                const std::vector<uint32_t> all = live_mobys();
                const uint32_t h = pick(all);
                w32(e + 0x20, h);
                if (coin()) w16(h + 0xa6, 0x283);
            }
            if (pass % 4 == 2 && coin()) {  // (its place behind the camera or far: off screen)
                Vec4 c = ee_ref<Vec4>(OV(0x00166dc0));
                c.x += rf(-2, 2); c.y += rf(-2, 2); c.z += rf(-6, -3);
                ee_ref<Vec4>(m + 0x10) = coin() ? c : Vec4{c.x + 5000, c.y, c.z, 1};
            }
            wfrand(pv + 0xc, 1, -3, 3);
            seed_rand();
        }, 25);

    fcheck("class933_spawn", spawn, 40, Ret::Int32, [carriers] {
        level();
        const std::vector<uint32_t> cs = carriers();
        const uint32_t owner = cs.empty() ? first_moby() : pick(cs);
        ee_ref<Vec4>(TEST_AREA) = ee_ref<Vec4>(owner + 0x10);
        wfrand(TEST_AREA, 3, -50, 50);
        vary_frame_scale();
        seed_rand();
        return Args{{owner, TEST_AREA}, {}};
    }, [](A a) { return r32(class933_spawn(a.i[0], a.i[1])); });

    // the carrier: its paths (+0xd8 / +0xdc: the level's paths), +0xfc 1 or not; the one it holds (+0xf4: 0, or a class 933
    // spawned by it), the carrier turned at random
    auto vary_carrier = [](uint32_t c) {
        const uint32_t pv = ee_read<uint32_t>(c + 0x78);
        int32_t n = 0;
        while (n < 64 && ee_read<uint32_t>(OV(0x001b04b0) + u(n) * 4) != 0) ++n;
        w32(pv + 0xd8, u(ri(0, std::max(0, n - 1))));
        w32(pv + 0xdc, u(ri(0, std::max(0, n - 1))));
        w32(pv + 0xfc, u(ri(0, 2)));
        const uint32_t held = ri(0, 3) == 0 ? 0u : class933_spawn(c, c + 0x10);
        w32(pv + 0xf4, held);
        wf(c + 0x48, rf(-3, 3));
        for (uint32_t row = 0xc0; row <= 0xe0; row += 0x10) wfrand(c + row, 3, -1, 1);
        seed_rand();
    };
    for (int pass = 0; pass < 2; ++pass) {
        classcheck("class933_carrier_start", start, carriers, class933_carrier_start, vary_carrier, 25);
        classcheck("class933_carrier_hold", hold, carriers, class933_carrier_hold, vary_carrier, 25);
        classcheck("class933_carrier_sparks", sparks, carriers, class933_carrier_sparks, vary_carrier, 25);
    }
}

// ---- class 235 (shared_class235.cpp)
void class235_checks() {
    const uint32_t upd = copy_here({{3, 0x002bae48}, {16, 0x002c3be8}});
    if (upd == 0) { not_here("class235_update"); return; }
    // trails spawned for the check: following a live moby with pvars, their colours, life and what is left (0, 1: they
    // leave), distance and speed
    auto trails = [] {
        std::vector<uint32_t> v;
        std::vector<uint32_t> owners;
        for (uint32_t o : live_mobys())
            if (ee_read<uint32_t>(o + 0x78) != 0) owners.push_back(o);
        for (int k = 0; k < 3 && !owners.empty(); ++k) {
            const uint32_t t = moby_spawn(235);
            if (t == 0) break;
            const uint32_t pv = ee_read<uint32_t>(t + 0x78);
            w32(pv, owners[static_cast<size_t>(k * 7) % owners.size()]);
            v.push_back(t);
        }
        return v;
    };
    for (int pass = 0; pass < 3; ++pass)
        classcheck("class235_update", upd, trails, class235_update, [](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w32(pv + 4, R());
            w32(pv + 8, R());
            const int32_t life = ri(1, 120);
            w16(pv + 0xc, u(life));
            w16(pv + 0xe, u(ri(0, 4) == 0 ? ri(0, 1) : ri(0, life)));
            wf(pv + 0x10, rf(-2, 5));
            wf(pv + 0x14, rf(-0.5f, 0.5f));
            wfrand(m + 0x40, 3, -3, 3);
            wf(m + 0x2c, rf(0.1f, 3));
            seed_rand();
        }, 25);
}

// ---- the hoverboard: class 439 and the boost pads (shared_hoverboard.cpp)
void hoverboard_checks() {
    const uint32_t upd = copy_here({{5, 0x002f87a8}, {16, 0x002c7788}});
    const uint32_t pads = copy_here({{5, 0x00307010}, {16, 0x002cfc00}});
    const uint32_t ctl = copy_here({{5, 0x00307910}, {16, 0x002d04a0}});  // (the race controller: class 717 / 556)
    if (upd == 0) { not_here("class439_update"); not_here("hoverboard_boost_pads"); return; }
    auto boards = [upd] { return placed_or_spawned(upd, 439); };
    // states 0..2; the hero near (5.5) or not, the frame counter, the hero's mode (0x15 / 0x16 / another), in a race (+0x50)
    // or not; the help message's conditions; +0xbc 0 / 1 / 2; the board still or moving (its last position +0)
    for (int pass = 0; pass < 12; ++pass)
        classcheck("class439_update", upd, boards, class439_update, [pass](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(pass < 9 ? pass % 3 : ri(0, 3)));
            hero_around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 5.4f : 9.0f);
            w32(OV(0x0015f5cc), u(ri(0, 3) == 0 ? ri(0, 4) : ri(4, 1000)));
            { static const uint32_t modes[] = {0x15, 0x16, 0x16, 0, 1, 7}; w32(0x001413dc, modes[ri(0, 5)]); }
            w32(pv + 0x50, u(ri(0, 1)));
            wfrand(pv + 0x10, 3, -100, 100);
            if (coin()) {  // (the help message's conditions met, or one of them not)
                w8(0x0013d388, u(ri(0, 3) != 0));
                w16(0x00141938, u(ri(0, 3) == 0));
                w32(0x0015f6a8, u(ri(0, 3) != 0));
                w32(0x0013f350 + 0x19c, u(ri(0, 3000)));
                w16(0x00141c60, u(ri(0, 3) == 0));
                w32(OV(0x00179410), 0);
                w32(OV(0x00179410) + 0x24, 0xffffffffu);
                w32(0x001516d0 + 0x50, 0);
                w32(0x001516d0 + 0x1c, 0xffffffffu);
            }
            w8(m + 0xbc, u(ri(0, 2)));
            Vec4 last = ee_ref<Vec4>(m + 0x10);
            const float d = coin() ? 0.0f : (coin() ? 1.5f : 3.0f);
            last.x += rf(-d, d); last.y += rf(-d, d); last.z += rf(-d, d);
            ee_ref<Vec4>(pv) = last;
            seed_rand();
        }, 25);

    // the pads: the controllers (class 717 / 556, or spawned); the hero's board one of the level's boards, his race time
    // (+0x894) and the controller's wait; the controller's point at a pad of class 0x473 / 0x474 or near one
    const bool rilgar = g_oracle_level == 5;
    const uint32_t at = rilgar ? 0x200u : 0x260u, boost = rilgar ? 0x25au : 0x2f6u, wait = rilgar ? 0x264u : 0x2dcu;
    auto controllers = [ctl, rilgar] { return placed_or_spawned(ctl, rilgar ? 717u : 556u, 2); };
    for (int pass = 0; pass < 4; ++pass)
        classcheck("hoverboard_boost_pads", pads, controllers, rilgar ? +[](uint32_t m) { hoverboard_boost_pads(m, 0x200, 0x25a, 0x264); }
                                                                      : +[](uint32_t m) { hoverboard_boost_pads(m, 0x260, 0x2f6, 0x2dc); },
                   [upd, at, boost, wait](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const std::vector<uint32_t> bs = mobys_with(upd);
            w32(0x0013f350 + 0x86c, ri(0, 5) == 0 || bs.empty() ? 0u : pick(bs));
            w32(0x0013f350 + 0x894, u(ri(0, 4) == 0 ? ri(0, 0xb4) : ri(0xb4, 5000)));
            w32(pv + wait, u(ri(0, 2) == 0 ? ri(0, 1) : ri(0, 30)));
            w16(pv + boost, u(ri(0, 500)));
            std::vector<uint32_t> ps = mobys_of(0x473);
            for (uint32_t q : mobys_of(0x474)) ps.push_back(q);
            if (!ps.empty() && ri(0, 4) != 0) {
                const uint32_t q = pick(ps);
                Vec4 c = ee_ref<Vec4>(q + 0x10);
                const float r = coin() ? 0.4f : 3.0f;
                c.x += rf(-r, r); c.y += rf(-r, r); c.z += rf(-r, r) - (coin() ? 0.8f : 0.0f);
                ee_ref<Vec4>(pv + at) = c;
            }
            seed_rand();
        }, 25);
}

// ---- the race's HUD element (shared_hoverboard.cpp)
void race_hud_checks() {
    const uint32_t start = copy_here({{5, 0x00262ae8}, {16, 0x002351f8}});
    const uint32_t update = copy_here({{5, 0x002661e8}, {16, 0x002388f8}});
    const uint32_t reset = copy_here({{5, 0x002661a0}, {16, 0x002388b0}});
    const uint32_t draw = copy_here({{5, 0x00266710}, {16, 0x00238e20}});
    if (start == 0) {
        for (const char* n : {"race_hud_start", "race_hud_update", "race_hud_reset", "race_hud_draw"}) not_here(n);
        return;
    }
    // a HUD slot (as batch 137's): its fade +0x6c (0 / below: no draw), timer, size; the hero's mode, race time and lap, the
    // race flag, the region's frame rate
    auto slot = [] {
        level();
        const uint32_t e = OV(0x0017dc50u) + u(ri(0, 7)) * 0x90;
        w32(e + 0x6c, u(ri(0, 3) == 0 ? ri(-6, 0) : ri(1, 0x1e)));
        w32(e + 0x7c, u(ri(0, 300)));
        w32(e + 0x58, u(ri(0, 0x100)));
        w32(e + 0x5c, u(ri(0, 0x100)));
        w32(0x001413dc, ri(0, 2) == 0 ? 0x16u : u(ri(0, 0x20)));
        w32(0x0013f350 + 0x894, u(ri(0, 3) == 0 ? ri(0, 4000) : ri(0, 400000)));
        w32(0x0013f350 + 0x8a8, u(ri(0, 5)));
        w8(0x0013d388, u(ri(0, 1)));
        if (coin()) w32(0x0015ed80, u(ri(0, 1)));
        vary_frame_scale();
        return Args{{e}, {}};
    };
    fcheck("race_hud_start", start, 40, Ret::None, slot, [](A a) { race_hud_start(a.i[0]); return uint64_t{0}; });
    fcheck("race_hud_update", update, 40, Ret::None, slot, [](A a) { race_hud_update(a.i[0]); return uint64_t{0}; });
    fcheck("race_hud_reset", reset, 40, Ret::None, slot, [](A a) { race_hud_reset(a.i[0]); return uint64_t{0}; });
    fcheck("race_hud_draw", draw, 100, Ret::None, slot, [](A a) { race_hud_draw(a.i[0]); return uint64_t{0}; });
}

// ---- the debris pieces: class 344 / 980 (shared_class344.cpp)
void class344_checks() {
    const uint32_t upd = copy_here({{5, 0x003186f8}, {8, 0x002dba40}});
    const uint32_t thr = copy_here({{5, 0x00318648}, {8, 0x002db990}});
    if (upd == 0) { not_here("class344_update"); not_here("class344_throw"); return; }
    const uint32_t cls = g_oracle_level == 5 ? 980u : 344u;
    // pieces spawned about the hero, thrown by the shared throw (checked below)
    auto pieces = [cls] {
        std::vector<uint32_t> v;
        const uint32_t buf = ee_stack_alloc(0x20);
        for (int k = 0; k < 3; ++k) {
            const uint32_t p = moby_spawn(cls);
            if (p == 0) break;
            ee_ref<Vec4>(p + 0x10) = ee_ref<Vec4>(0x0013f3d0);
            wf(p + 0x18, ee_read<float>(p + 0x18) + 1.0f + static_cast<float>(k));
            ee_ref<Vec4>(buf) = Vec4{0.05f * static_cast<float>(k), -0.1f, 0.2f, 0.0f};
            ee_ref<Vec4>(buf + 0x10) = Vec4{0.0f, 0.5f, 0.25f, 0.0f};
            class344_throw(p, 0, buf, buf + 0x10, 60);
            v.push_back(p);
        }
        ee_stack_free(0x20);
        return v;
    };
    // states 0..2; mirrored or not; hanging from a joint (+4) or not; the moby it ignores; its radius; its life (running out),
    // alpha; its velocity (into the ground, sideways, slow); low (below 5)
    // (the collision's records 0x173e90..0x173e98, 0x173ea0..0x173ea8 and 0x173eb0..0x173eb8: sphere_collide's port leaves
    // other values there than the original, 1024 times smaller; the levels' transliterated copies fail the same words in
    // this check: tried; batch 506 leaves out the first of them for the same reason)
    const size_t n_ign = g_ignored_words.size();
    for (const uint32_t k : {0x0u, 0x4u, 0x8u, 0x10u, 0x14u, 0x18u, 0x20u, 0x24u, 0x28u}) g_ignored_words.push_back(OV(0x00173e90) + k);
    for (int pass = 0; pass < 8; ++pass)
        classcheck("class344_update", upd, pieces, class344_update, [pass](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(pass < 6 ? (pass < 2 ? 0 : 1) : ri(0, 2)));
            if (coin()) w16(m + 0x32, ee_read<uint16_t>(m + 0x32) ^ 1u);
            if (coin()) w16(m + 0x34, ee_read<uint16_t>(m + 0x34) ^ 0x8000u);
            w32(pv + 4, u(ri(0, 2) == 0 ? ri(1, 2) : ri(-1, 0)));
            const std::vector<uint32_t> all = live_mobys();
            w32(pv + 8, coin() ? 0u : pick(all));
            w32(pv + 0xc, u(ri(0, 3000)));
            w32(pv, u(ri(0, 3) == 0 ? ri(0, 1) : ri(0, 50)));
            w8(m + 0x23, u(ri(0, 0x80)));
            Vec4& vel = ee_ref<Vec4>(pv + 0x30);
            vel.x = rf(-0.5f, 0.5f); vel.y = rf(-0.5f, 0.5f); vel.z = rf(-0.8f, 0.4f);
            if (ri(0, 3) == 0) { vel.x *= 0.01f; vel.y *= 0.01f; vel.z *= 0.01f; }
            wfrand(pv + 0x10, 3, -1, 1);
            wfrand(pv + 0x20, 3, -0.2f, 0.2f);
            if (ri(0, 4) == 0) wf(m + 0x18, rf(-3, 6));
            seed_rand();
        }, 25);
    g_ignored_words.resize(n_ign);
    fcheck("class344_throw", thr, 40, Ret::None, [cls] {
        level();
        const uint32_t p = moby_spawn(cls);
        ee_ref<Vec4>(p + 0x10) = ee_ref<Vec4>(0x0013f3d0);
        wfrand(TEST_AREA, 8, -2, 2);
        if (coin()) wf(0x0015ed6c, rf(0.01f, 0.05f));
        vary_frame_scale();
        seed_rand();
        return Args{{p, R(), TEST_AREA, TEST_AREA + 0x10, u(ri(0, 200))}, {}};
    }, [](A a) { class344_throw(a.i[0], a.i[1], a.i[2], a.i[3], static_cast<int32_t>(a.i[4])); return uint64_t{0}; });
}

// ---- the path-riding rafts (shared_floaters.cpp)
void floater_checks() {
    const uint32_t bob = copy_here({{5, 0x00319740}, {8, 0x002dc4a0}, {14, 0x002f1490}, {16, 0x002e4d88}});
    const uint32_t ride = copy_here({{5, 0x003195f8}, {14, 0x002f1360}, {16, 0x002e4c40}});
    const uint32_t lay = copy_here({{5, 0x003198e8}, {14, 0x002f1638}, {16, 0x002e4f30}});
    const uint32_t msg = copy_here({{5, 0x00319690}, {16, 0x002e4cd8}});
    const uint32_t find = copy_here({{5, 0x00319598}, {16, 0x002e4be0}});
    if (bob == 0) {
        for (const char* n : {"floater_bob", "floater_ride", "floater_lay_path", "floater_message", "floater_find"}) not_here(n);
        return;
    }
    const int lv = g_oracle_level;
    const uint32_t cls = lv == 5 ? 998u : lv == 8 ? 424u : lv == 14 ? 712u : 1410u;
    auto rafts = [cls] {
        std::vector<uint32_t> v = mobys_of(cls);
        if (v.empty()) {
            const uint32_t s = moby_spawn(cls);
            if (s != 0) { ee_ref<Vec4>(s + 0x10) = ee_ref<Vec4>(0x0013f3d0); v.push_back(s); }
        }
        return v;
    };
    int npaths = 0;
    level();
    while (npaths < 200 && ee_read<uint32_t>(OV(0x001b04b0) + u(npaths) * 4) >= 0x00100000u &&
           ee_read<uint32_t>(OV(0x001b04b0) + u(npaths) * 4) < 0x02000000u)
        ++npaths;
    std::printf("  (the level's paths: %d)\n", npaths);
    const uint32_t phases = lv == 8 ? 0xecu : lv == 16 ? 0xb8u : 0xc8u;
    const uint32_t along = lv == 16 ? 0xb0u : 0xc0u, path = lv == 16 ? 0xa4u : 0xacu, lay_at = lv == 16 ? 0xa0u : 0xdcu;
    for (int pass = 0; pass < 2; ++pass)
        classcheck("floater_bob", bob, rafts, lv == 8 ? +[](uint32_t m) { floater_bob(m, 0xec); }
                                               : lv == 16 ? +[](uint32_t m) { floater_bob(m, 0xb8); } : +[](uint32_t m) { floater_bob(m, 0xc8); },
                   [phases](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            wfrand(pv + phases, 3, -3.2f, 3.2f);
            wfrand(pv + 0x70, 3, -0.3f, 0.3f);
            if (coin()) wf(0x0015ed6c, rf(0.01f, 2.0f));
            wfrand(m + 0x40, 2, -0.3f, 0.3f);
            seed_rand();
        }, 25);
    if (ride == 0 || npaths == 0) { not_here("floater_ride"); not_here("floater_lay_path"); }
    else {
        // (its result: at the path's end or not)
        fcheck("floater_ride", ride, 60, Ret::Int32, [rafts, along, path, npaths] {
            level();
            const std::vector<uint32_t> rs = rafts();
            const uint32_t m = pick(rs);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w32(pv + path, ee_read<uint32_t>(OV(0x001b04b0) + u(ri(0, npaths - 1)) * 4));
            wf(pv + along, coin() ? rf(0.0f, 1.0f) : rf(0.95f, 1.1f));
            wf(pv + along + 4, rf(-0.05f, 0.05f));
            wf(pv + along + 0x14, rf(0.001f, 0.05f));
            wf(pv + along + 0x18, rf(0.001f, 0.02f));
            seed_rand();
            return Args{{m}, {}};
        }, [along, path](A a) { vu0_sync(); return r32(u(floater_ride(a.i[0], along, path))); });
        auto port_lay = lv == 16 ? +[](uint32_t m) { floater_lay_path(m, 0xa0); } : +[](uint32_t m) { floater_lay_path(m, 0xdc); };
        for (int pass = 0; pass < 2; ++pass)
            classcheck("floater_lay_path", lay, rafts, port_lay, [lay_at, npaths](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                const int32_t i = ri(0, 4) == 0 ? -1 : ri(0, npaths - 1);
                w32(pv + lay_at, u(i));
                if (i >= 0 && ri(0, 3) != 0) w32(ee_read<uint32_t>(OV(0x001b04b0) + u(i) * 4), 7);  // (a path of 7 points)
                wfrand(m + 0xc0, 12, -1, 1);
                seed_rand();
            }, 25);
    }
    if (msg == 0) { not_here("floater_message"); not_here("floater_find"); return; }
    auto port_msg = lv == 16 ? +[](uint32_t m) { floater_message(m, 0xae, TXT(0x3e89), TXT(0x3e8a)); }
                             : +[](uint32_t m) { floater_message(m, 0xb6, TXT(0x13a5), TXT(0x1398)); };
    const uint32_t stage = lv == 16 ? 0xaeu : 0xb6u;
    for (int pass = 0; pass < 2; ++pass)
        classcheck("floater_message", msg, rafts, port_msg, [stage](uint32_t m) {
            w16(ee_read<uint32_t>(m + 0x78) + stage, u(ri(-2, 7)));
            w32(OV(0x0015f594), coin() ? 0u : (coin() ? 8u : 3u));
            seed_rand();
        }, 25);
    const uint32_t key = lv == 16 ? 0xacu : 0xb4u;
    fcheck("floater_find", find, 60, Ret::Int32, [rafts, key] {
        level();
        const std::vector<uint32_t> rs = rafts();
        const std::vector<uint32_t> all = live_mobys();
        const uint32_t m = coin() || rs.empty() ? pick(all) : pick(rs);
        // (a member of its group's key, or another value)
        const uint32_t list = ee_read<uint32_t>(OV(0x001ab840) + ee_read<uint8_t>(m + 0x21) * 4u);
        int32_t value = ri(-1, 40);
        if (list != 0 && coin()) {
            const uint32_t o = first_moby() + ((ee_read<uint16_t>(list + 2u * u(ri(0, 1))) & 0x7fffu) << 8);
            value = ee_read<int16_t>(ee_read<uint32_t>(o + 0x78) + key);
        }
        return Args{{m, u(value)}, {}};
    }, [key](A a) { return r32(floater_find(a.i[0], static_cast<int32_t>(a.i[1]), key)); });
}

// ---- the small helpers (shared_little.cpp)
// the mobys whose class's hook table (+0x2c, six words) names `addr` (batch 506's)
std::vector<uint32_t> hook_owners(uint32_t addr) {
    std::vector<uint32_t> own;
    for (uint32_t m = first_moby(); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
        const uint32_t cls = ee_read<uint32_t>(m + 0x24);
        if (ee_read<uint8_t>(m + 0x20) >= 0xfe || cls == 0 || ee_read<uint32_t>(cls + 0x2c) == 0) continue;
        for (uint32_t k = 0; k < 0x18; k += 4)
            if (ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + k) == addr) { own.push_back(m); break; }
    }
    return own;
}

void little_checks() {
    const int lv = g_oracle_level;
    // moby_is_in_state: any moby, made of the class or not, in the state or not
    if (const uint32_t a = copy_here({{3, 0x002c6c60}, {11, 0x0030f270}, {15, 0x002d9570}, {17, 0x002d7778}}); a == 0) not_here("moby_is_in_state");
    else {
        const int32_t cls = lv == 3 ? 0x23e : 0x49b;
        const uint32_t st = lv == 3 ? 0xeu : 2u;
        fcheck("moby_is_in_state", a, 60, Ret::Int32, [cls, st] {
            level();
            const uint32_t m = pick(live_mobys());
            if (coin()) w16(m + 0xa6, u(cls));
            w8(m + 0x20, coin() ? st : u(ri(0, 0x10)));
            return Args{{m}, {}};
        }, [cls, st](A a) { return r32(moby_is_in_state(a.i[0], cls, st)); });
    }
    // moby_alive_in_state: Kerwan's two copies; also 0 and gone mobys
    for (const auto& c : {std::tuple<uint32_t, int32_t, uint32_t>{0x002c5b78, 0x23d, 5}, std::tuple<uint32_t, int32_t, uint32_t>{0x002c6d58, 0x23e, 9}}) {
        if (lv != 3) { not_here("moby_alive_in_state"); break; }
        const int32_t cls = std::get<1>(c);
        const uint32_t st = std::get<2>(c);
        fcheck("moby_alive_in_state", std::get<0>(c), 60, Ret::Int32, [cls, st] {
            level();
            const uint32_t m = ri(0, 5) == 0 ? 0u : pick(live_mobys());
            if (m != 0) {
                if (coin()) w16(m + 0xa6, u(cls));
                static const uint32_t sts[] = {0xfe, 0xfd, 0, 1};
                w8(m + 0x20, coin() ? st : sts[ri(0, 3)]);
            }
            return Args{{m}, {}};
        }, [cls, st](A a) { return r32(moby_alive_in_state(a.i[0], cls, st)); });
    }
    // moby_largest: four mobys (some 0), their scales (ties)
    if (const uint32_t a = copy_here({{5, 0x002899f8}, {8, 0x00269c70}}); a == 0) not_here("moby_largest");
    else
        fcheck("moby_largest", a, 60, Ret::Int32, [] {
            level();
            const std::vector<uint32_t> all = live_mobys();
            uint32_t ms[4];
            for (uint32_t& m : ms) {
                m = ri(0, 3) == 0 ? 0u : pick(all);
                if (m != 0) wf(m + 0x2c, ri(0, 3) == 0 ? 1.0f : rf(0.1f, 3.0f));
            }
            return Args{{ms[0], ms[1], ms[2], ms[3]}, {}};
        }, [](A a) { return r32(moby_largest(a.i[0], a.i[1], a.i[2], a.i[3])); });
    // moby_show_default
    if (const uint32_t a = copy_here({{14, 0x002b3330}, {15, 0x00297118}}); a == 0) not_here("moby_show_default");
    else
        fcheck("moby_show_default", a, 40, Ret::None, [] {
            level();
            const uint32_t m = pick(live_mobys());
            w16(m + 0x34, R() & 0xffff);
            return Args{{m}, {}};
        }, [](A a) { moby_show_default(a.i[0]); return uint64_t{0}; });
    // moby_rest_sparkles: on the level's class 1109 (Blarg) / 1106 (Umbris) mobys, or spawned ones
    if (const uint32_t a = copy_here({{6, 0x002e6148}, {7, 0x00302208}}); a == 0) not_here("moby_rest_sparkles");
    else {
        const uint32_t cls = lv == 6 ? 1109u : 1106u;
        fcheck("moby_rest_sparkles", a, 40, Ret::None, [cls] {
            level();
            std::vector<uint32_t> v = mobys_of(cls);
            const uint32_t m = v.empty() ? moby_spawn(cls) : pick(v);
            w16(m + 0x34, R() & 0xffff);
            vary_frame_scale();
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { moby_rest_sparkles(a.i[0]); return uint64_t{0}; });
    }
    // cam_entry_mark / _type19: the level's camera entries (their records), the active camera of type 19 or not
    {
        const uint32_t a = copy_here({{10, 0x002f5a50}, {13, 0x00316f20}, {14, 0x00314168}, {15, 0x002f78b8}});
        const uint32_t b = copy_here({{13, 0x00316f48}, {14, 0x00314190}});
        auto entry = [] {
            level();
            int32_t n = 0;
            const uint32_t t = ee_read<uint32_t>(OV(0x0015ef50));
            while (n < 64 && t != 0 && ee_read<uint32_t>(t + u(n) * 0x20 + 0x1c) >= 0x00100000u && ee_read<uint32_t>(t + u(n) * 0x20 + 0x1c) < 0x02000000u) ++n;
            const int32_t i = n == 0 ? 0 : ri(0, n - 1);
            const uint32_t r = ee_read<uint32_t>(t + u(i) * 0x20 + 0x1c);
            w8(r + 0x38, R() & 0xff);
            w8(r + 0x39, R() & 0xff);
            const uint32_t cam = ee_read<uint32_t>(OV(0x00166e00));
            if (cam != 0 && coin()) w16(cam + 0x86, coin() ? 0x13u : u(ri(0, 20)));
            return Args{{u(i)}, {}};
        };
        if (a == 0) not_here("cam_entry_mark");
        else fcheck("cam_entry_mark", a, 40, Ret::None, entry, [](A x) { cam_entry_mark(static_cast<int32_t>(x.i[0])); return uint64_t{0}; });
        if (b == 0) not_here("cam_entry_mark_type19");
        else fcheck("cam_entry_mark_type19", b, 40, Ret::None, entry, [](A x) { cam_entry_mark_type19(static_cast<int32_t>(x.i[0])); return uint64_t{0}; });
    }
    if (const uint32_t a = copy_here({{2, 0x002f8aa0}, {6, 0x00312b40}, {17, 0x002fe2c8}}); a == 0) not_here("camera_angles_set");
    else
        fcheck("camera_angles_set", a, 40, Ret::None, [] {
            level();
            return Args{{}, {rf(-3, 3), rf(-3, 3), rf(-3, 3), rf(-3, 3), rf(-3, 3), rf(-3, 3)}};
        }, [](A x) { camera_angles_set(x.f[0], x.f[1], x.f[2], x.f[3], x.f[4], x.f[5]); return uint64_t{0}; });
    // the classes' animation times: a class of a live moby (with animations), a set within its count or not, 255 and above
    {
        const uint32_t a = copy_here({{4, 0x00241910}, {15, 0x0023e4c8}, {17, 0x00249618}});
        const uint32_t b = copy_here({{4, 0x002418b0}, {17, 0x002495b8}});
        auto args = [] {
            level();
            // (a class of a live moby and one of its sets whose frames are in RAM; else 255 and above)
            auto ram = [](uint32_t a) { return a >= 0x00100000u && a < 0x02000000u; };
            uint32_t cls = 0, set = 0;
            for (int k = 0; k < 200; ++k) {
                cls = ee_read<uint16_t>(pick(live_mobys()) + 0xa6);
                const uint32_t c = ee_read<uint32_t>(OV(0x00197300) + cls * 4);
                if (!ram(c)) continue;
                set = u(ri(0, 15));
                const uint32_t st = ee_read<uint32_t>(c + 0x48 + set * 4);
                if (!ram(st)) continue;
                const uint32_t n = ee_read<uint8_t>(st + 0x10);
                if (n == 0 || !ram(ee_read<uint32_t>(st + 0x1c)) || !ram(ee_read<uint32_t>(st + 0x1c + (n - 1) * 4))) continue;
                break;
            }
            if (ri(0, 5) == 0) set = u(ri(255, 300));
            return Args{{cls, set}, {}};
        };
        if (a == 0) not_here("class_anim_last_time");
        else fcheck("class_anim_last_time", a, 60, Ret::Float, args, [](A x) { return rf32(class_anim_last_time(x.i[0], x.i[1])); });
        if (b == 0) not_here("class_anim_first_time");
        else fcheck("class_anim_first_time", b, 60, Ret::Float, args, [](A x) { return rf32(class_anim_first_time(x.i[0], x.i[1])); });
    }
    if (const uint32_t a = copy_here({{7, 0x0029a2a0}, {13, 0x0027d8d0}, {14, 0x00279aa0}}); a == 0) not_here("particle_33_spawn");
    else
        fcheck("particle_33_spawn", a, 40, Ret::Int32, [] {
            level();
            wfrand(TEST_AREA, 8, -5, 5);
            seed_rand();
            return Args{{TEST_AREA, TEST_AREA + 0x10}, {}};
        }, [](A x) { return r32(particle_33_spawn(x.i[0], x.i[1])); });
    if (const uint32_t a = copy_here({{3, 0x002d5208}, {7, 0x00305a38}, {15, 0x002dee78}}); a == 0) not_here("class848_spawn");
    else
        fcheck("class848_spawn", a, 40, Ret::Int32, [] {
            level();
            wfrand(TEST_AREA, 8, -50, 50);
            seed_rand();
            return Args{{TEST_AREA, TEST_AREA + 0x10, R(), R()}, {rf(-2, 2)}};
        }, [](A x) { return r32(class848_spawn(x.i[0], x.i[1], x.i[2], x.i[3], x.f[0])); });
    // the carry hooks: on the mobys whose class's hook table names the copy; their state (the carried one or not), the carry
    // record's state 8 (it lets go), +0xbc, the target
    struct Hook { int level; uint32_t addr; uint32_t which; bool keep; };
    static const Hook hooks[] = {{11, 0x00316210, 4, true}, {11, 0x00316270, 8, true},
                                 {17, 0x002efea8, 0, false}, {17, 0x002eff30, 4, false}, {17, 0x002effb8, 8, false}};
    for (const Hook& h : hooks) {
        const char* name = h.keep ? "carry_keep_state_hook" : "carry_drop_hook";
        if (h.level != lv) { if (lv != 11 && lv != 17) not_here(name); continue; }
        if (!selected(name)) continue;
        level();
        const std::vector<uint32_t> own = hook_owners(h.addr);
        if (own.empty()) { std::printf("%-26s L%d:0x%08x  no moby owns it on this level\n", name, lv, h.addr); continue; }
        g_extra_noop_hooks.push_back(0x002763f8);
        fcheck(name, h.addr, 40, Ret::Int32, [own, h] {
            level();
            const uint32_t m = pick(own);
            const uint32_t carried = h.keep ? 0x14u : 0xcu;
            w8(m + 0x20, coin() ? carried : u(ri(0, 0x16)));
            if (ri(0, 2) == 0) {
                const uint32_t r = class_hook_10(m);
                if (r != 0) w16(r + 0x68, 8);
            }
            w8(m + 0xbc, u(ri(0, 9)));
            if (coin()) w8(m + 0x53, coin() ? 7u : u(ri(0, 9)));
            const uint32_t tgt = ri(0, 3) == 0 ? 0u : first_moby() + 0x100 * u(ri(0, 100));
            vary_frame_scale();
            return Args{{m, ri(0, 2) == 0 ? 0x0013f3d0u : m + 0x10, tgt}, {rf(-1, 3)}};
        }, [h](A a) {
            return ri32(h.keep ? carry_keep_state_hook(a.i[0], a.i[1], a.i[2], a.f[0], h.which) : carry_drop_hook(a.i[0], a.i[1], a.i[2], a.f[0], h.which));
        });
        g_extra_noop_hooks.pop_back();
    }
    // Kerwan's turning objects: their classes' mobys (or spawned), their angles and pvars
    if (lv != 3) { not_here("kerwan_class816_turn"); not_here("kerwan_class1012_turn"); }
    else {
        for (const auto& c : {std::tuple<uint32_t, uint32_t, void (*)(uint32_t)>{0x002d3c40, 816, kerwan_class816_turn},
                              std::tuple<uint32_t, uint32_t, void (*)(uint32_t)>{0x002dd0f0, 1012, kerwan_class1012_turn}}) {
            const uint32_t cls = std::get<1>(c);
            classcheck(cls == 816 ? "kerwan_class816_turn" : "kerwan_class1012_turn", std::get<0>(c),
                       [cls] { std::vector<uint32_t> v = mobys_of(cls); if (v.empty()) { const uint32_t s = moby_spawn(cls); if (s != 0) v.push_back(s); } return v; },
                       std::get<2>(c), [](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                wfrand(pv + 0xa8, 4, -3, 3);
                wfrand(pv + 0xe8, 4, -3, 3);
                wfrand(m + 0x40, 3, -3, 3);
                if (coin()) wf(0x0015ed6c, rf(0.01f, 0.05f));
            }, 30);
        }
    }
    // Blarg's class 1051: its mobys (or spawned), the path index (+0xec) and count (+0x114)
    if (lv != 6) not_here("blarg_class1051_switch");
    else {
        int32_t npaths = 0;
        level();
        while (npaths < 200 && ee_read<uint32_t>(OV(0x001b04b0) + u(npaths) * 4) >= 0x00100000u && ee_read<uint32_t>(OV(0x001b04b0) + u(npaths) * 4) < 0x02000000u) ++npaths;
        for (const uint32_t a : {0x002fb4b0u, 0x002fb870u, 0x002fbc30u})
            classcheck("blarg_class1051_switch", a,
                       [] { std::vector<uint32_t> v = mobys_of(1051); if (v.empty()) { const uint32_t s = moby_spawn(1051); if (s != 0) v.push_back(s); } return v; },
                       blarg_class1051_switch, [npaths](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w32(pv + 0xec, u(ri(0, std::max(0, npaths - 1))));
                w32(pv + 0x114, u(ri(0, 12)));
                w32(0x001413dc, u(ri(0, 3)));
            }, 20);
    }
}

// ---- the arrival scene's sparks and glow, the particle types, the even path (shared_cutscene_fx.cpp,
// shared_particle_kinds.cpp, shared_little.cpp)
// a live moby with a joint chain (its class's joint count +0xf, some), moved about the hero (whom the view looks at) or
// not, its matrix made again
uint32_t jointed_moby(bool near_hero) {
    std::vector<uint32_t> v;
    for (uint32_t m : live_mobys()) {
        const uint32_t c = ee_read<uint32_t>(m + 0x24);
        if (c != 0 && ee_read<uint8_t>(c + 0xf) != 0 && ee_read<uint32_t>(m + 0x7c) != 0) v.push_back(m);
    }
    const uint32_t m = v.empty() ? first_moby() : pick(v);
    if (near_hero) {
        const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
        const float r = coin() ? 2.0f : 8.0f;
        ee_ref<Vec4>(m + 0x10) = Vec4{h.x + rf(-r, r), h.y + rf(-r, r), h.z + rf(-1, 3), 1.0f};
        wfrand(m + 0x40, 3, -3.1f, 3.1f);
        moby_update_transform(m);
    }
    return m;
}

void fx_checks() {
    const uint32_t eff = copy_here({{0, 0x0023e008}, {14, 0x00245e98}});
    const uint32_t drw = copy_here({{0, 0x0023db30}, {14, 0x002459c0}});
    if (eff == 0) { not_here("cutscene_l14_effect"); not_here("cutscene_l14_effect_draw"); }
    else {
        fcheck("cutscene_l14_effect", eff, 40, Ret::None, [] {
            level();
            const uint32_t m = jointed_moby(coin());
            if (coin()) wf(0x0015ed60, rf(0.5f, 1.5f));
            vary_frame_scale();
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { vu0_sync(); cutscene_l14_effect(a.i[0]); return uint64_t{0}; });
        fcheck("cutscene_l14_effect_draw", drw, 60, Ret::None, [] {
            level();
            const uint32_t m = jointed_moby(ri(0, 3) != 0);
            if (coin()) wf(OV(0x0015fb08), rf(-2, 2));
            packet_cursor();
            draw_probe("cutscene_l14_effect_draw", 0, [m] { cutscene_l14_effect_draw(m); });
            return Args{{m}, {}};
        }, [](A a) { vu0_sync(); cutscene_l14_effect_draw(a.i[0]); return uint64_t{0}; });
        draw_report("cutscene_l14_effect_draw");
    }
    // particle 0x50: allocated for the check, in the level's box or out of it, its life (out at 0 / 1), its +0x20 word,
    // its size and colour
    if (const uint32_t a = copy_here({{0, 0x00275eb0}, {5, 0x002a00f0}, {7, 0x0029e4a0}}); a == 0) not_here("beam_particle_update");
    else
        fcheck("beam_particle_update", a, 100, Ret::None, [] {
            level();
            uint32_t p = alloc_particle(0x50);
            if (p == 0) p = TEST_AREA + 0x400;
            const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
            ee_ref<Vec4>(p + 0x10) = Vec4{h.x + rf(-5, 5), h.y + rf(-5, 5), h.z + rf(-5, 5), 1.0f};
            if (ri(0, 4) == 0) wf(p + 0x10 + 4u * u(ri(0, 2)), coin() ? rf(-5, 1.9f) : rf(1021.1f, 1100));
            w16(p + 0xa, u(ri(0, 4) == 0 ? ri(0, 1) : ri(2, 200)));
            w32(p + 0x20, coin() ? 0u : R());
            wf(p + 0xc, rf(100, 50000));
            w32(p + 4, R());
            return Args{{p}, {}};
        }, [](A a) { beam_particle_update(a.i[0]); return uint64_t{0}; });
    // the spawns: their arguments at random (a velocity or none; the modes / kinds about their branches)
    if (const uint32_t a = copy_here({{0, 0x002743a8}, {5, 0x0029e698}, {16, 0x0026f7e8}}); a == 0) not_here("particle_43_spawn");
    else
        fcheck("particle_43_spawn", a, 60, Ret::Int32, [] {
            level();
            wfrand(TEST_AREA, 8, -5, 5);
            seed_rand();
            return Args{{R(), TEST_AREA, u(ri(1, 200)), u(ri(0, 2) == 0 ? 0 : ri(1, 3)), R() & 0xff, coin() ? 0u : TEST_AREA + 0x10},
                        {rf(0, 2), rf(0, 2)}};
        }, [](A x) {
            return r32(particle_43_spawn(x.i[0], x.i[1], static_cast<int32_t>(x.i[2]), x.i[3], x.i[4], x.i[5], x.f[0], x.f[1]));
        });
    if (const uint32_t a = copy_here({{0, 0x002729c8}, {7, 0x0029ae58}}); a == 0) not_here("particle_3a_spawn");
    else
        fcheck("particle_3a_spawn", a, 60, Ret::Int32, [] {
            level();
            wfrand(TEST_AREA, 4, -5, 5);
            vary_frame_scale();
            seed_rand();
            const uint32_t kind = ri(0, 4) == 0 ? R() : u(ri(0, 3));
            return Args{{TEST_AREA, kind, R(), R(), R(), R(), R()}, {rf(0, 3), rf(0, 3)}};
        }, [](A x) {
            return r32(particle_3a_spawn(x.i[0], x.i[1], x.i[2], x.i[3], x.i[4], x.i[5], x.i[6], x.f[0], x.f[1]));
        });
    if (const uint32_t a = copy_here({{0, 0x0026fd28}, {10, 0x00262b08}}); a == 0) not_here("particle_28_spawn");
    else
        fcheck("particle_28_spawn", a, 60, Ret::Int32, [] {
            level();
            wfrand(TEST_AREA, 8, -5, 5);
            if (coin()) wf(0x0015ed60, rf(0.5f, 1.5f));
            seed_rand();
            const uint32_t kind = ri(0, 3) == 0 ? R() : u(ri(0, 7));
            return Args{{TEST_AREA, TEST_AREA + 0x10, kind, R()}, {}};
        }, [](A x) { return r32(particle_28_spawn(x.i[0], x.i[1], x.i[2], x.i[3])); });
    // the even path: a path of 1..8 points built in the test area, a distance before / along / past it, its legs' length
    if (const uint32_t a = copy_here({{0, 0x00262360}, {10, 0x002554e0}}); a == 0) not_here("path_point_even");
    else
        fcheck("path_point_even", a, 80, Ret::None, [] {
            level();
            const uint32_t path = TEST_AREA + 0x200;
            const int32_t n = ri(1, 8);
            w32(path, u(n));
            wfrand(path + 0x10, 4 * n + 4, -20, 20);
            wrand(TEST_AREA, 2);
            const float leg = rf(0.5f, 10);
            const float dist = ri(0, 4) == 0 ? rf(-10, 0) : rf(0, leg * static_cast<float>(n + 1));
            return Args{{path, TEST_AREA, TEST_AREA + 4, TEST_AREA + 0x10}, {dist, leg}};
        }, [](A x) { path_point_even(x.i[0], x.i[1], x.i[2], x.i[3], x.f[0], x.f[1]); return uint64_t{0}; });
}

// ---- classes 1221 / 1222 and 1371 (shared_class1221.cpp, shared_class1371.cpp)
void hoven_piece_checks() {
    const int lv = g_oracle_level;
    const uint32_t u1221 = copy_here({{3, 0x002de370}, {7, 0x0031d9f8}, {12, 0x00301d08}, {15, 0x002e5e70}});
    const uint32_t u1222 = copy_here({{3, 0x002de458}, {7, 0x0031dae0}, {12, 0x00301df0}, {15, 0x002e5f58}});
    if (u1221 == 0) not_here("class1221_update");
    else
        for (const uint32_t cls : {1221u, 1222u}) {
            const uint32_t upd = cls == 1221 ? u1221 : u1222;
            classcheck("class1221_update", upd, [upd, cls] { return placed_or_spawned(upd, cls); }, class1221_update, [](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                wfrand(pv, 3, -0.3f, 0.3f);
                wfrand(pv + 0x10, 3, -0.2f, 0.2f);
                const int32_t life = ri(1, 120);
                w32(pv + 0x20, u(life));
                w32(pv + 0x24, u(ri(0, 4) == 0 ? ri(0, 1) : ri(0, life)));
                wfrand(m + 0x40, 3, -3.1f, 3.1f);
                if (coin()) wf(0x0015ed70, rf(0.0002f, 0.002f));
            }, 25);
        }
    const uint32_t upd = copy_here({{12, 0x00308918}, {15, 0x002ea5e0}});
    const uint32_t spawn = copy_here({{12, 0x00308670}, {15, 0x002ea338}});
    const uint32_t thr = copy_here({{12, 0x00308708}, {15, 0x002ea3d0}});
    if (upd == 0) { for (const char* n : {"class1371_update", "class1371_spawn", "class1371_throw"}) not_here(n); return; }
    (void)lv;
    fcheck("class1371_spawn", spawn, 40, Ret::Int32, [] {
        level();
        return Args{{pick(live_mobys())}, {}};
    }, [](A a) { return r32(class1371_spawn(a.i[0])); });
    // pieces spawned by the shared spawn at a live moby near the hero
    auto pieces = [] {
        std::vector<uint32_t> v;
        const std::vector<uint32_t> all = live_mobys();
        for (int k = 0; k < 3; ++k) {
            const uint32_t r = class1371_spawn(all[static_cast<size_t>(k * 5) % all.size()]);
            if (r == 0) break;
            ee_ref<Vec4>(r + 0x10) = ee_ref<Vec4>(0x0013f3d0);
            wf(r + 0x18, ee_read<float>(r + 0x18) + 1.0f + static_cast<float>(k));
            v.push_back(r);
        }
        return v;
    };
    // (the throw takes the piece as its second argument)
    fcheck("class1371_throw", thr, 50, Ret::None, [pieces] {
        level();
        const std::vector<uint32_t> ps = pieces();
        const uint32_t m = pick(ps);
        wf(m + 0x48, rf(-3.1f, 3.1f));
        if (coin()) wf(0x0015ed70, rf(0.0002f, 0.002f));
        seed_rand();
        return Args{{R(), m}, {}};
    }, [](A a) { vu0_sync(); class1371_throw(a.i[0], a.i[1]); return uint64_t{0}; });
    // the update: thrown (the shared throw) or not (its animation set), then moved about: into the ground, up, low
    for (int pass = 0; pass < 4; ++pass)
        classcheck("class1371_update", upd, pieces, class1371_update, [pass](uint32_t m) {
            if (pass != 3) class1371_throw(0, m);
            if (pass < 2) {  // (some frames of its flight first, the port's: rising, falling, about to land)
                const int frames = ri(0, 90);
                for (int k = 0; k < frames && ee_read<uint8_t>(m + 0x20) < 0xfe; ++k) class1371_update(m);
                if (ee_read<uint8_t>(m + 0x20) >= 0xfe) w8(m + 0x20, 0);
            }
            if (pass == 2) w8(m + 0x53, u(ri(0, 2)));
            Vec4& p = ee_ref<Vec4>(m + 0x10);
            if (pass >= 2 && coin()) p.z = ee_read<float>(0x0013f3d8) + rf(-1.0f, 0.5f);
            if (ri(0, 4) == 0) p.z = rf(-2, 6);
            wfrand(m + 0x40, 2, -3.1f, 3.1f);
            seed_rand();
        }, 25);
}

// ---- class 823 and class 1511's draw (shared_class823.cpp, shared_class1511.cpp)
// a moby of class `cls` (the level's, or spawned) at the hero (whom the view looks at) or further, its pvars from `vary`
uint32_t drawn_subject(uint32_t cls, bool near_hero) {
    std::vector<uint32_t> v = mobys_of(cls);
    const uint32_t m = v.empty() ? moby_spawn(cls) : pick(v);
    if (near_hero) {
        const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
        const float r = coin() ? 2.0f : 6.0f;
        ee_ref<Vec4>(m + 0x10) = Vec4{h.x + rf(-r, r), h.y + rf(-r, r), h.z + rf(-2, 2), 1.0f};
        moby_update_transform(m);
    }
    return m;
}

void plume_checks() {
    const uint32_t upd = copy_here({{5, 0x0030c0a8}, {7, 0x00304738}});
    const uint32_t drw = copy_here({{5, 0x0030c220}, {7, 0x003048b0}});
    if (upd == 0) { not_here("class823_update"); not_here("class823_draw"); }
    else {
        for (int pass = 0; pass < 3; ++pass)
            classcheck("class823_update", upd, [upd] { return placed_or_spawned(upd, 823); }, class823_update, [pass](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w8(m + 0x20, u(pass < 2 ? pass : ri(0, 3)));
                wfrand(pv + 0x40, 5, -3.1f, 3.1f);
                wf(m + 0x48, rf(-3.1f, 3.1f));
            }, 25);
        fcheck("class823_draw", drw, 60, Ret::None, [] {
            level();
            const uint32_t m = drawn_subject(823, ri(0, 3) != 0);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            wf(pv + 0x48, rf(-3.1f, 3.1f));
            wf(m + 0x48, rf(-3.1f, 3.1f));
            packet_cursor();
            draw_probe("class823_draw", 0, [m] { class823_draw(m); });
            return Args{{m}, {}};
        }, [](A a) { vu0_sync(); class823_draw(a.i[0]); return uint64_t{0}; });
        draw_report("class823_draw");
    }
    const uint32_t glow = copy_here({{5, 0x0031cf58}, {14, 0x00308150}});
    if (glow == 0) { not_here("class1511_draw"); return; }
    fcheck("class1511_draw", glow, 60, Ret::None, [] {
        level();
        const uint32_t m = drawn_subject(1511, ri(0, 3) != 0);
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w32(pv + 0x60, u(ri(0, 0x80)));
        w8(pv + 0x67, u(ri(0, 1)));
        w32(pv + 0x68, R() & 0xffffff);
        wf(pv + 0x70, rf(0.2f, 4));
        wfrand(m + 0xe0, 3, -1, 1);
        if (ri(0, 2) == 0) {  // (close to the camera, on the way to the hero: its depth tells)
            const Vec4 c = ee_ref<Vec4>(OV(0x00166dc0)), h = ee_ref<Vec4>(0x0013f3d0);
            const float t = rf(0.1f, 0.4f);
            ee_ref<Vec4>(m + 0x10) = Vec4{c.x + (h.x - c.x) * t, c.y + (h.y - c.y) * t, c.z + (h.z - c.z) * t, 1.0f};
            wfrand(m + 0xe0, 3, -0.05f, 0.05f);
        }
        packet_cursor();
        draw_probe("class1511_draw", 0, [m] { class1511_draw(m); });
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { vu0_sync(); class1511_draw(a.i[0]); return uint64_t{0}; });
    draw_report("class1511_draw");
}

// ---- the panels' lights, class 819's launch, the fleet's environment map, the path mover (shared_panel_lights.cpp,
// shared_little.cpp)
void lights_checks() {
    const int lv = g_oracle_level;
    struct Lights { int level; uint32_t addr; uint32_t cls; uint32_t base; };
    static const Lights ls[] = {{2, 0x002ea8b0, 1005, 0}, {6, 0x002f4cd8, 1016, 0}, {15, 0x002ebb50, 1428, 0}, {17, 0x002f1568, 1428, 0},
                                {10, 0x00298940, 18, 0x10}, {15, 0x002ea9b0, 1388, 0x10}};
    bool any = false;
    for (const Lights& l : ls) {
        if (l.level != lv) continue;
        any = true;
        const uint32_t cls = l.cls, base = l.base;
        auto subjects = [cls] { std::vector<uint32_t> v = mobys_of(cls); if (v.empty()) { const uint32_t s = moby_spawn(cls); if (s != 0) v.push_back(s); } return v; };
        // the spins about their wraps (0 / 255), the rates either way, the timers out or not, the sizes
        classcheck("panel_lights", l.addr, subjects, base == 0 ? +[](uint32_t m) { panel_lights(m, 0); } : +[](uint32_t m) { panel_lights(m, 0x10); },
                   [base](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78) + base;
            for (uint32_t k = 0; k < 0x10; k += 4) {
                wf(pv + k, ri(0, 3) == 0 ? rf(250, 256) : rf(-1, 255));
                wf(pv + 0x10 + k, rf(-12, 12));
                w32(pv + 0x20 + k, u(ri(0, 3) == 0 ? ri(0, 1) : ri(0, 300)));
                wf(pv + 0x30 + k, rf(0.1f, 2));
            }
        }, 30);
    }
    if (!any) not_here("panel_lights");
    if (const uint32_t a = copy_here({{10, 0x002cc1b8}, {15, 0x002dd600}}); a == 0) not_here("class819_launch");
    else
        fcheck("class819_launch", a, 40, Ret::Int32, [] {
            level();
            ee_ref<Vec4>(TEST_AREA + 0x10) = ee_ref<Vec4>(0x0013f3d0);
            wfrand(TEST_AREA, 4, -1, 1);
            wfrand(TEST_AREA + 0x10, 3, -20, 20);
            vary_frame_scale();
            return Args{{TEST_AREA, TEST_AREA + 0x10, R(), R(), u(ri(0, 300))}, {rf(0.2f, 3)}};
        }, [](A x) { return r32(class819_launch(x.i[0], x.i[1], x.i[2], x.i[3], static_cast<int32_t>(x.i[4]), x.f[0])); });
    // the fleet's environment map: points and normals built in the test area about the camera
    if (lv != 17) not_here("envmap_uvs4");
    else
        for (const uint32_t a : {0x002f54d8u, 0x002f57f0u, 0x002f5b08u, 0x002f5e20u, 0x002f6450u})
            fcheck("envmap_uvs4", a, 20, Ret::None, [] {
                level();
                const int32_t n = ri(0, 12);
                const Vec4 c = ee_ref<Vec4>(OV(0x00166dc0));
                for (int32_t i = 0; i < n; ++i) {
                    const uint32_t k = u(i) * 0xc;
                    wf(TEST_AREA + 0x100 + k, c.x + rf(-10, 10));
                    wf(TEST_AREA + 0x104 + k, c.y + rf(-10, 10));
                    wf(TEST_AREA + 0x108 + k, c.z + rf(-10, 10));
                    wfrand(TEST_AREA + 0x200 + k, 3, -1, 1);
                }
                return Args{{u(n), TEST_AREA + 0x200, TEST_AREA + 0x100, R(), TEST_AREA + 0x400}, {}};
            }, [](A x) { envmap_uvs4(static_cast<int32_t>(x.i[0]), x.i[1], x.i[2], x.i[3], x.i[4]); return uint64_t{0}; });
    // the path mover: Oltanis's classes 908 / 921 (or spawned), their path a level's path (with its count)
    if (lv != 14) not_here("path_mover_start");
    else {
        int32_t npaths = 0;
        level();
        while (npaths < 200 && ee_read<uint32_t>(OV(0x001b04b0) + u(npaths) * 4) >= 0x00100000u && ee_read<uint32_t>(OV(0x001b04b0) + u(npaths) * 4) < 0x02000000u) ++npaths;
        for (const auto& c : {std::pair<uint32_t, uint32_t>{0x002fc890, 908}, std::pair<uint32_t, uint32_t>{0x002fd808, 921}}) {
            const uint32_t cls = c.second;
            classcheck("path_mover_start", c.first, [cls] { std::vector<uint32_t> v = mobys_of(cls); if (v.empty()) { const uint32_t s = moby_spawn(cls); if (s != 0) v.push_back(s); } return v; },
                       path_mover_start, [npaths](uint32_t m) {
                w32(ee_read<uint32_t>(m + 0x78) + 0x68, u(ri(0, std::max(0, npaths - 1))));
                wfrand(ee_read<uint32_t>(m + 0x78) + 0x70, 8, -2, 2);
            }, 30);
        }
    }
}

// ---- the ripple meshes' draws (shared_waves.cpp): on a live moby moved about the hero, its phase, colour, the frame counter
void waves_checks() {
    struct Wave { int level; uint32_t addr; void (*fn)(uint32_t); const char* name; };
    static const Wave ws[] = {{15, 0x002edbe0, quartu_waves_draw_a, "quartu_waves_draw"}, {15, 0x002edfc0, quartu_waves_draw_b, "quartu_waves_draw"},
                              {17, 0x002f0948, fleet_waves_draw_a, "fleet_waves_draw"}, {17, 0x002f0d08, fleet_waves_draw_b, "fleet_waves_draw"}};
    if (g_oracle_level != 15) not_here("quartu_waves_draw");
    if (g_oracle_level != 17) not_here("fleet_waves_draw");
    for (const Wave& w : ws) {
        if (w.level != g_oracle_level) continue;
        fcheck(w.name, w.addr, 30, Ret::None, [w] {
            level();
            const uint32_t m = jointed_moby(ri(0, 3) != 0);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            wfrand(pv, 2, -3.1f, 3.1f);
            w32(m + 0x90, R());
            w32(OV(0x0015f5cc), R() & 0xffff);
            vary_frame_scale();
            packet_cursor();
            draw_probe(w.name, 0x400, [w, m] { w.fn(m); });
            return Args{{m}, {}};
        }, [w](A a) { vu0_sync(); w.fn(a.i[0]); return uint64_t{0}; });
        draw_report(w.name);
    }
}

// ---- Blarg's mesh draws (shared_blarg_meshes.cpp): the camera moved to look at one of the mesh's points (its forward row
// 0x166fd0 kept, 3..25 back), the view set up again
void look_at(const Vec4& p) {
    const Vec4 f = ee_ref<Vec4>(OV(0x00166fd0));
    const float d = rf(3, 25);
    ee_ref<Vec4>(OV(0x00166dc0)) = Vec4{p.x - f.x * d + rf(-2, 2), p.y - f.y * d + rf(-2, 2), p.z - f.z * d + rf(-2, 2), 1.0f};
    render_view_setup();
}

void blarg_mesh_checks() {
    if (g_oracle_level != 6) { not_here("blarg_class1083_draw"); not_here("blarg_class1062_draw"); return; }
    struct D { uint32_t addr; void (*fn)(uint32_t); const char* name; uint32_t verts; uint32_t scale; uint32_t lift; };
    // (a point of the mesh: a vertex, scaled by the words' scale, raised by +0 for class 1083)
    static const D ds[] = {{0x003003f0, blarg_class1083_draw_a, "blarg_class1083_draw", 0x001f2eb0, 0x00162074, 0x0016204c},
                           {0x00300588, blarg_class1083_draw_b, "blarg_class1083_draw", 0x001f5330, 0x00162074, 0x0016204c},
                           {0x00300720, blarg_class1083_draw_c, "blarg_class1083_draw", 0x001f77b0, 0x00162074, 0x0016204c},
                           {0x00300958, blarg_class1083_draw_d, "blarg_class1083_draw", 0x001fb5f0, 0x00162074, 0x0016204c},
                           {0x002fd460, blarg_class1062_draw_b, "blarg_class1062_draw", 0x001eed30, 0x00161f9c, 0},
                           {0x002fd748, blarg_class1062_draw_c, "blarg_class1062_draw", 0x001dbff0, 0x00161f9c, 0}};
    for (const D& d : ds) {
        fcheck(d.name, d.addr, 25, Ret::None, [d] {
            level();
            if (ri(0, 4) != 0) {
                const float k = ee_read<float>(ORACLE_LADDR(6, d.scale));
                Vec4 p = ee_ref<Vec4>(ORACLE_LADDR(6, d.verts) + u(ri(0, 40)) * 0x10);
                p.x *= k; p.y *= k; p.z *= k;
                if (d.lift != 0) p.z += ee_read<float>(ORACLE_LADDR(6, d.lift));
                look_at(p);
            }
            if (coin()) wfrand(OV(0x00166dd0), 3, -3.1f, 3.1f);
            packet_cursor();
            draw_probe(d.name, 0, [d] { d.fn(0); });
            return Args{{first_moby()}, {}};
        }, [d](A a) { vu0_sync(); d.fn(a.i[0]); return uint64_t{0}; });
        draw_report(d.name);
    }
}

}  // namespace

void batch_508() {
    if (g_only_batch != 508) return;
    g_batch = 508;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    // (the scratchpad words the pose function 0x245f18 spills the caller's $t8 / $t9 into: batch 120's)
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // 989snd's IOP server (as batches 120 / 506): replies a function of the command
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
    // (no broad lists of left-out words: each check that meets a known difference of existing code leaves out its own words)
    // (the kernel's calls the original would make for real: the CD / movie path of a level change request; batch 120's)
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80};
    // the port's VU0 micro memory as the game leaves it, and the interpreter's VU0 kept for the port (as batches 137 / 506)
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

    class1179_checks();
    class933_checks();
    class235_checks();
    hoverboard_checks();
    race_hud_checks();
    class344_checks();
    floater_checks();
    little_checks();
    fx_checks();
    hoven_piece_checks();
    plume_checks();
    lights_checks();
    waves_checks();
    blarg_mesh_checks();

    g_pre_call = nullptr;
    g_extra_noop_hooks.clear();
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
