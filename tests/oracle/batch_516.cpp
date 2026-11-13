// Oracle batch 516 (stage 4 task F10, #149): the class code of Quartu (level 15) and Drek's fleet (17) that both levels carry a
// copy of, each family ported once as shared hand-written code (src/game/ovl/shared_*.cpp) on top of the families earlier tasks
// ported, against each level's own copy at its own address. The level copies stay transliterated in their units until each
// level is re-glued; here every copy is the original and the one shared port the port:
//  - shared_class44.cpp: class 44, a guard (its update through every state, its frame's common part, its look, and its
//    helpers: the ally search, the turns, the walk, the group's alert and calm, the path order, the look at another);
//  - shared_class408.cpp: class 408, the guards' alarm (its update, the frame's alarm, its raise, its wake);
//  - shared_class78.cpp: class 78, a turret the alarms wake (its update, its wake, its break), the sparks it spawns
//    (particle 0x47) and the guards' shot (class 934);
//  - shared_class655.cpp: class 655, an electric field (its update, its arc and the arc's reset, the hero's ground test,
//    class 1179's lift).
// The shared ports and their copies (each level's address):
//   class44_update        L15:0x002979d8 L17:0x0029f8d8
//   class44_think         L15:0x0029a570 L17:0x002a2470
//   class44_aim           L15:0x0029a2a8 L17:0x002a21a8
//   class44_find_ally     L15:0x00299dd8 L17:0x002a1cd8
//   class44_turn_to       L15:0x00299880 L17:0x002a1780
//   class44_turn_toward   L15:0x00299a68 L17:0x002a1968
//   class44_walk_to       L15:0x00299ae0 L17:0x002a19e0
//   class44_alert_group   L15:0x0029a0d0 L17:0x002a1fd0
//   class44_group_calm    L15:0x00299ca0 L17:0x002a1ba0
//   class44_path_first    L15:0x00299ff8 L17:0x002a1ef8
//   class44_look_at       L15:0x0029a248 L17:0x002a2148
//   class408_update       L15:0x002cb4c8 L17:0x002cbe80
//   class408_alarm        L15:0x002cbc30 L17:0x002cc5e8
//   class408_raise        L15:0x002cbac0 L17:0x002cc478
//   class408_wake         L15:0x002cbba0 L17:0x002cc558
//   class78_update        L15:0x002a2bf0 L17:0x002a82c0
//   class78_wake          L15:0x002a2868 L17:0x002a7f38
//   class78_break         L15:0x002a35d8 L17:0x002a8ca8
//   particle_47_spawn     L15:0x002647f0 L17:0x0026e978
//   class934_shot_spawn   L15:0x002e4580 L17:0x002e1e20
//   class655_update       L15:0x002d6810 L17:0x002d4e88
//   class655_arcs         L15:0x002d7bb0 L17:0x002d6228
//   class655_arcs_reset   L15:0x002d79e8 L17:0x002d6060
//   class655_hero_grounded L15:0x002d6770 L17:0x002d4de8
//   class1179_lift        L15:0x002d95a0 L17:0x002d77a8
// Subjects: the level's own mobys of the class after the first gameplay frame (the screen, the projection and the view set
// up, as batch 518), placed or live, their pvars dealt through every state from the level's own (paths, alarms, groups,
// boxes, other mobys swapped between the class's mobys); the helpers on those mobys with arguments built from the level.
// B516_INFO=1 prints the subjects found.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 516, N = 15 and 17; on another level every check says "skipped". No
// RAM dumps; the frame scale (0x15ed68) at its 50 Hz value in a quarter of the cases.

#include "ovl_oracle.h"

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class408.h>
#include <game/ovl/shared_class44.h>
#include <game/ovl/shared_class655.h>
#include <game/ovl/shared_class78.h>
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

bool info() { return std::getenv("B516_INFO") != nullptr; }

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
// original's VU0 first. With B516_INFO, the cases that wrote packets beyond a state header are counted.
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

// a point `r0`..`r1` from `at` (horizontally), `z0`..`z1` above it
Vec4 around(const Vec4& at, float r0, float r1, float z0 = -1.0f, float z1 = 2.0f) {
    const float r = rf(r0, r1), a = rf(-3.1f, 3.1f);
    return Vec4{at.x + r * std::cos(a), at.y + r * std::sin(a), at.z + rf(z0, z1), at.w};
}
void hero_at(const Vec4& q) {
    ee_ref<Vec4>(0x0013f3d0) = q;
    ee_ref<Vec4>(hero_moby() + 0x10) = q;
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

// A check whose generator is dealt again when the port's run on its state throws (a pointer outside RAM, which the
// interpreter reads through), as classcheck does
void tcheck(const char* name, uint32_t laddr, int n, Ret ret, const std::function<Args()>& gen,
            const std::function<uint64_t(const Args&)>& port) {
    fcheck(name, laddr, n, ret, [gen, port] {
        Args a;
        for (int attempt = 0; attempt < 40; ++attempt) {
            a = gen();
            if (trial([port, a] { port(a); })) break;
        }
        return a;
    }, port);
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


// ---- the level's mobys of the families
uint32_t index_of(uint32_t m) { return (m - first_moby()) >> 8; }
uint32_t box_centre(int32_t box) { return ee_read<uint32_t>(OV(0x001600ec)) + u(box) * 0x80 + 0x30; }
// the moby's animation dealt: its set (+0x53) one of `sets` (or any below 16), the next (+0x52) the same or not, its flags
// (+0x70: bit 1 the end reached), its frame (the current record's +4, in 16ths) at `frames` sometimes
void deal_anim(uint32_t m, std::initializer_list<uint32_t> sets, std::initializer_list<float> frames = {}) {
    std::vector<uint32_t> s(sets);
    const uint32_t set = !s.empty() && ri(0, 3) != 0 ? s[static_cast<size_t>(ri(0, static_cast<int>(s.size()) - 1))] : u(ri(0, 15));
    w8(m + 0x53, set);
    w8(m + 0x52, ri(0, 3) != 0 ? set : u(ri(0, 15)));
    w8(m + 0x70, coin() ? 2u : (R() & 0xfd));
    if (coin()) {
        wf(m + 0x54, 0.0f);
        const uint32_t cur = ee_read<uint8_t>(m + 0x52) == 0xff ? ee_read<uint32_t>(m + 0x6c) : ee_read<uint32_t>(m + 0x68);
        std::vector<float> f(frames);
        const float t = !f.empty() && coin() ? f[static_cast<size_t>(ri(0, static_cast<int>(f.size()) - 1))] : rf(0.0f, 20.0f);
        if (cur >= 0x00100000u && cur < 0x02000000u) w16(cur + 4, u(static_cast<int32_t>(t * 16.0f)));
    }
}
// a hit on the moby (batch 518's): its slot in the hit table naming it, a damage mask, a damage, a hitter (the wrench's class
// 0x47 sometimes); or none
void give_hit(uint32_t m, uint32_t kinds) {
    if (ri(0, 2) == 0) { w8(m + 0xa4, 0xff); return; }
    const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
    w8(m + 0xa4, idx);
    w32(e + 0x34, ri(0, 8) == 0 ? first_moby() : m);
    w32(e + 0x24, ri(0, 3) == 0 ? (R() & ~kinds) : (kinds | (R() & 0xff)));
    { static const float dm[] = {-1.0f, 0.0f, 0.5f, 1.0f, 2.0f, 5.0f}; wf(e + 0x2c, dm[ri(0, 5)]); }
    wfrand(e, 4, -1, 1);
    wfrand(e + 0x10, 3, -1, 1);
    const std::vector<uint32_t> lives = live_mobys();
    const uint32_t by = ri(0, 3) == 0 || lives.empty() ? hero_moby() : pick(lives);
    w32(e + 0x20, by);
    if (ri(0, 3) == 0) w16(by + 0xa6, 0x47);  // (the wrench)
    w8(e + 0x28, u(ri(0, 8)));
    w8(e + 0x29, u(ri(0, 4)));
    w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
    w32(e + 0x1c, coin() ? 0x45afdf66u : 0u);
}

struct Lv {
    uint32_t c44, think, aim, find, turn, toward, walk, alert, calm, first, look;
    uint32_t c408, alarm, raise, wake408, c78, wake78, brk, p47, shot, c655, arcs, reset, grounded, lift;
};
Lv lv() {
    const bool l15 = g_oracle_level == 15, l17 = g_oracle_level == 17;
    auto a = [l15, l17](uint32_t x, uint32_t y) { return l15 ? x : (l17 ? y : 0u); };
    return Lv{a(0x002979d8, 0x0029f8d8), a(0x0029a570, 0x002a2470), a(0x0029a2a8, 0x002a21a8), a(0x00299dd8, 0x002a1cd8),
              a(0x00299880, 0x002a1780), a(0x00299a68, 0x002a1968), a(0x00299ae0, 0x002a19e0), a(0x0029a0d0, 0x002a1fd0),
              a(0x00299ca0, 0x002a1ba0), a(0x00299ff8, 0x002a1ef8), a(0x0029a248, 0x002a2148), a(0x002cb4c8, 0x002cbe80),
              a(0x002cbc30, 0x002cc5e8), a(0x002cbac0, 0x002cc478), a(0x002cbba0, 0x002cc558), a(0x002a2bf0, 0x002a82c0),
              a(0x002a2868, 0x002a7f38), a(0x002a35d8, 0x002a8ca8), a(0x002647f0, 0x0026e978), a(0x002e4580, 0x002e1e20),
              a(0x002d6810, 0x002d4e88), a(0x002d7bb0, 0x002d6228), a(0x002d79e8, 0x002d6060), a(0x002d6770, 0x002d4de8),
              a(0x002d95a0, 0x002d77a8)};
}
// the level's words of the guards (+0: the frame's alert, +0x2c: held) and of the alarm (+0 .. +0x14), of the field (+0 ..)
uint32_t guard_words() { return g_oracle_level == 17 ? 0x00161388u : 0x001613e8u; }
uint32_t alarm_words() { return g_oracle_level == 17 ? 0x00161988u : 0x00161ab4u; }
uint32_t field_words() { return g_oracle_level == 17 ? 0x00161a58u : 0x00161bc8u; }

std::vector<uint32_t> of_class(uint32_t cls) { return placed_of(cls); }
// a moby index of the class, or -1, or (rarely) any moby
uint32_t some_index(uint32_t cls, int none_in = 3) {
    if (ri(0, none_in) == 0) return 0xffffffffu;
    const std::vector<uint32_t> v = of_class(cls);
    if (v.empty() || ri(0, 6) == 0) return index_of(pick(live_mobys()));
    return index_of(pick(v));
}

// the hero placed: near the moby (in front or not), at its target, far, or left
void place_hero(uint32_t m) {
    const Vec4 at = ee_ref<Vec4>(m + 0x10);
    const int k = ri(0, 4);
    if (k == 0) hero_at(around(at, 1.0f, 8.0f, -0.5f, 1.0f));
    else if (k == 1) {  // (in front of it)
        const float a = ee_read<float>(m + 0x48) + rf(-0.6f, 0.6f), r = rf(2.0f, 30.0f);
        hero_at(Vec4{at.x + r * std::cos(a), at.y + r * std::sin(a), at.z + rf(-1.0f, 3.0f), at.w});
    } else if (k == 2) hero_at(around(at, 20.0f, 80.0f));
    ee_ref<Vec4>(0x0013f420) = ee_ref<Vec4>(0x0013f3d0);
    wf(0x0013f428, ee_read<float>(0x0013f3d8) + 1.0f);
}
// the hero's state: dead (0x65), 0x59, 0x18 / 0x19, or its own
void deal_hero_state() {
    const int k = ri(0, 9);
    if (k == 0) w32(0x001413d4, 0x65);
    else if (k == 1) w32(0x001413d4, 0x59);
    else if (k == 2) w32(0x001413d4, u(ri(0x18, 0x19)));
    else if (k == 3) { static const uint32_t sc[] = {0x6a, 0x82, 0x76, 0x75}; w32(0x001413d4, sc[ri(0, 3)]); }  // (sub_0020d568's)
    else if (k == 4) w32(0x001413dc, u(ri(0x11, 0x12)));
    w8(0x001413f4, ri(0, 2) == 0 ? u(ri(0, 3)) : 0u);
}

// ---- class 44, the guard (shared_class44.cpp)
// the guard's state dealt (`st`: the state the case runs, -1 any)
void deal_guard(uint32_t m, int st) {
    const uint32_t pv = pvar(m);
    const std::vector<uint32_t> guards = of_class(44);
    if (guards.size() > 1 && ri(0, 3) == 0) {  // (another guard's path, alarm, mobys and boxes)
        const uint32_t o = pvar(pick(guards));
        for (const uint32_t k : {0x120u, 0x124u, 0x128u, 0x12cu, 0x150u, 0x17cu, 0x180u, 0x184u, 0x188u, 0x18cu, 0x2d0u, 0x2d4u, 0x2d8u, 0x2dcu})
            w32(pv + k, ee_read<uint32_t>(o + k));
    }
    if (ri(0, 4) == 0) w32(pv + 0x124, 0xffffffffu);
    if (ri(0, 12) == 0) w32(pv + 0x120, ri(0, 2) == 0 ? 0xffffffffu : ee_read<uint32_t>(pv + 0x124));
    const int32_t path = ee_read<int32_t>(pv + 0x124);
    if (path >= 0 && path < 0x400) {
        const uint32_t p = ee_read<uint32_t>(OV(0x001b04b0) + u(path) * 4);
        const int32_t n = p >= 0x00100000u && p < 0x02000000u ? ee_read<int32_t>(p) : 0;
        if (n > 0) {
            w32(pv + 0x13c, u(ri(0, n - 1)));
            if (coin()) ee_ref<Vec4>(m + 0x10) = around(ee_ref<Vec4>(p + 0x10 + ee_read<uint32_t>(pv + 0x13c) * 0x10), 0.0f, coin() ? 0.19f : 3.0f, 0.0f, 0.0f);
        }
    }
    w32(pv + 0x144, u(ri(0, 1)));
    w32(pv + 0x148, u(ri(-2, 300)));
    { static const int32_t ts[] = {0, 1, 2, 100}; w32(pv + 0x14c, u(ts[ri(0, 3)])); }
    { static const uint32_t vs[] = {0, 1, 2, 2, 3}; w32(pv + 0x114, vs[ri(0, 4)]); }
    static const int16_t t16[] = {0, 0, 1, 2, 30};
    for (const uint32_t k : {0x154u, 0x156u, 0x158u, 0x194u, 0x196u, 0x19cu}) w16(pv + k, u(t16[ri(0, 4)]));
    for (const uint32_t k : {0x15au, 0x15bu, 0x15fu, 0x19eu, 0x19fu}) w8(pv + k, u(ri(0, 1)));
    w8(pv + 0x15e, u(ri(0, 3)));
    { static const uint32_t ss[] = {2, 4, 5, 0xb, 0xd, 0x10, 0xff}; w8(pv + 0x15c, ss[ri(0, 6)]); }
    { static const uint32_t as[] = {0, 3, 0xd, 0xe, 0xff}; w8(pv + 0x15d, as[ri(0, 4)]); }
    if (ri(0, 3) == 0) w32(pv + 0x128, some_index(408));
    {  // (its alarm raised or not)
        const int32_t a = ee_read<int32_t>(pv + 0x128);
        if (a >= 0 && a < 0x800) {
            const uint32_t apv = ee_read<uint32_t>(first_moby() + u(a) * 0x100 + 0x78);
            if (apv >= 0x00100000u && apv < 0x02000000u) w32(apv + 8, u(ri(0, 1)));
        }
    }
    w32(pv + 0x12c, ri(0, 2) == 0 ? some_index(78, 1) : 0xffffffffu);
    w32(pv + 0x150, ri(0, 2) == 0 ? some_index(78, 1) : 0xffffffffu);
    if (st == 1 || ri(0, 3) == 0)
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t i = ri(0, 2) == 0 ? 0xffffffffu : index_of(pick(live_mobys()));
            w32(pv + 0x180 + k * 4, i);
            if (i != 0xffffffffu) w8(first_moby() + (i << 8) + 0x20, u(ri(0, 3)));
        }
    if (ri(0, 3) == 0) w32(pv + 0x17c, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 40)));
    // allies (the levels place no class-635 moby): the members of a group made allies, their states 4 .. 16 (6 .. 15 count,
    // 7 / 15 call), some about the guard; that group the guard's ally group
    if (ri(0, 2) != 0) {
        const uint32_t g = coin() && ee_read<uint8_t>(m + 0x21) != 0xff ? ee_read<uint8_t>(m + 0x21) : u(ri(0, 63));
        uint32_t e = ee_read<uint32_t>(OV(0x001ab840) + g * 4);
        if (e >= 0x00100000u && e < 0x02000000u) {
            w32(pv + 0x17c, g);
            for (int n = 0; n < 64; ++n, e += 2) {
                const uint32_t o = first_moby() + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                if (o != m && ri(0, 3) != 0) {
                    w16(o + 0xa6, 635);
                    w8(o + 0x20, ri(0, 3) == 0 ? (coin() ? 7u : 0xfu) : u(ri(4, 16)));
                    if (coin()) ee_ref<Vec4>(o + 0x10) = around(ee_ref<Vec4>(m + 0x10), 1.0f, coin() ? 7.0f : 14.0f, -0.5f, 0.5f);
                }
                if (ee_read<int16_t>(e) < 0) break;
            }
        }
    }
    const std::vector<uint32_t> allies = of_class(635);
    w32(pv + 0x190, ri(0, 2) == 0 || allies.empty() ? 0u : pick(allies));
    w32(pv + 0x198, coin() || guards.empty() ? 0u : pick(guards));
    for (const uint32_t a : allies) if (coin()) w8(a + 0x20, u(ri(4, 17)));
    // the camera boxes: none mostly; the guard's own boxes with the camera in one or not
    if (ri(0, 3) != 0) for (uint32_t k = 0; k < 4; ++k) w32(pv + 0x2d0 + k * 4, 0xffffffffu);
    else if (coin()) {
        const int32_t b = ee_read<int32_t>(pv + 0x2d0 + u(ri(0, 3)) * 4);
        if (b >= 0 && b < 0x200) ee_ref<Vec4>(OV(0x00166dc0)) = ee_ref<Vec4>(box_centre(b));
    }
    const std::vector<uint32_t> lives = live_mobys();
    w32(pv + 0x110, ri(0, 3) == 0 ? 0u : (coin() ? hero_moby() : pick(lives)));
    { static const float hs[] = {0.0f, 0.5f, 1.0f, 3.0f}; wf(pv + 0x20, hs[ri(0, 3)]); }
    w32(pv + 0x38, u(ri(0, 1)));
    { static const float ws[] = {0.0f, 1.0f, 100.0f}; wf(pv + 0x130, ws[ri(0, 2)]); }
    if (coin()) ee_ref<Vec4>(pv + 0x160) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 0.19f : 5.0f, 0.0f, 0.0f);
    wf(pv + 0x168, ee_read<float>(m + 0x18) + rf(-1.0f, 8.0f));
    wf(pv + 0x16c, rf(-3.1f, 3.1f));
    wf(pv + 0x170, rf(-3.1f, 3.1f));
    wf(m + 0x48, rf(-3.1f, 3.1f));
    if (coin()) ee_ref<Vec4>(pv + 0xd0) = around(ee_ref<Vec4>(m + 0x10), 1.0f, 20.0f, -5.0f, 5.0f);
    if (coin()) ee_ref<Vec4>(pv + 0x100) = around(ee_ref<Vec4>(m + 0x10), 2.0f, 20.0f, -2.0f, 3.0f);
    wf(pv + 0x134, rf(0.0f, 0.3f));
    deal_anim(m, {0, 1, 2, 3, 4, 6, 9, 0xa, 0xb, 0xc, 0xd, 0xe}, {0.5f, 0.9f, 2.0f, 2.9f, 3.1f, 11.0f, 12.5f});
    w8(m + 0x31, u(ri(0, 1)));
    w8(m + 0xbc, u(ri(0, 2)));
    give_hit(m, 0x330000u);
    if (coin()) w8(pv + 0x28, u(ri(0, 3)));  // (the hit's reactions' row)
    place_hero(m);
    const int32_t home = ee_read<int32_t>(pv + 0x120);
    if (home >= 0 && home < 0x400 && ri(0, 2) != 0) {  // (the hero at a point of its home polygon: a target picked)
        const uint32_t p = ee_read<uint32_t>(OV(0x001b04b0) + u(home) * 4);
        const int32_t n = p >= 0x00100000u && p < 0x02000000u ? ee_read<int32_t>(p) : 0;
        if (n > 0) {
            // (inside it: toward its points' centre from one of them)
            Vec4 c{0.0f, 0.0f, 0.0f, 0.0f};
            for (int32_t i = 0; i < n; ++i) {
                const Vec4 pi = ee_ref<Vec4>(p + 0x10 + u(i) * 0x10);
                c.x += pi.x / static_cast<float>(n);
                c.y += pi.y / static_cast<float>(n);
                c.z += pi.z / static_cast<float>(n);
            }
            const Vec4 v = ee_ref<Vec4>(p + 0x10 + u(ri(0, n - 1)) * 0x10);
            const float t = rf(0.3f, 1.0f);
            const Vec4 q{v.x + (c.x - v.x) * t, v.y + (c.y - v.y) * t, v.z + (c.z - v.z) * t, v.w};
            hero_at(around(q, 0.0f, 0.3f, 0.0f, 0.5f));
            ee_ref<Vec4>(0x0013f420) = ee_ref<Vec4>(0x0013f3d0);
            wf(0x0013f428, ee_read<float>(0x0013f3d8) + 1.0f);
            if (coin()) {  // (the guard near it, facing it)
                ee_ref<Vec4>(m + 0x10) = around(q, 2.0f, 8.0f, 0.0f, 0.0f);
                const Vec4 g = ee_ref<Vec4>(m + 0x10);
                wf(m + 0x48, std::atan2(q.y - g.y, q.x - g.x) + rf(-0.5f, 0.5f));
            }
        }
    }
    // (one case in three a sighting that keeps it alerted: in front, in its polygon, the hero's alert word 3, its count 1 or
    // none, alerted by another or not, the hero waiting (0x59) or not; the think's lose path and the states' alerted ones)
    const bool sighting = ri(0, 2) == 0;
    if (sighting || ri(0, 2) == 0) {  // (the hero close in front of it, its head in sight: seen, its target picked)
        const float a = ee_read<float>(m + 0x48) + rf(-0.3f, 0.3f), r = rf(1.5f, 5.0f);
        if (coin()) {  // (the guard moved behind the hero, facing it)
            const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
            ee_ref<Vec4>(m + 0x10) = Vec4{h.x - r * std::cos(a), h.y - r * std::sin(a), h.z, ee_ref<Vec4>(m + 0x10).w};
        }
        const Vec4 g = ee_ref<Vec4>(m + 0x10);
        hero_at(Vec4{g.x + r * std::cos(a), g.y + r * std::sin(a), g.z, g.w});
        ee_ref<Vec4>(0x0013f420) = ee_ref<Vec4>(0x0013f3d0);
        wf(0x0013f428, g.z + 1.0f);
        if (coin()) w8(pv + 0x15e, 0);
    }
    // (its home path's polygon, which picks its target, made a square about the hero half the time: the levels' homes are
    // lines, where the hero is never inside)
    const int32_t home2 = ee_read<int32_t>(pv + 0x120);
    if (home2 >= 0 && home2 < 0x400 && (sighting || coin())) {
        const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
        const float r = coin() ? rf(2.0f, 6.0f) : rf(10.0f, 40.0f);
        w32(T4, 4);
        const float xs[] = {-r, r, r, -r}, ys[] = {-r, -r, r, r};
        for (uint32_t i = 0; i < 4; ++i) ee_ref<Vec4>(T4 + 0x10 + i * 0x10) = Vec4{h.x + xs[i], h.y + ys[i], h.z, 1.0f};
        w32(OV(0x001b04b0) + u(home2) * 4, T4);
    }
    if (ri(0, 3) == 0) {  // (another target than the hero: a class-203 moby in state 3 in the level's list 0x1b0830, near it)
        const std::vector<uint32_t> lives = live_mobys();
        const uint32_t c = pick(lives);
        if (c != m) {
            w16(c + 0xa6, 0xcb);
            w8(c + 0x20, 3);
            const bool far = ri(0, 3) == 0;
            ee_ref<Vec4>(c + 0x10) = far ? around(ee_ref<Vec4>(m + 0x10), 120.5f, 127.5f, 0.0f, 0.0f)
                                         : around(ee_ref<Vec4>(0x0013f3d0), 0.0f, 2.0f, 0.0f, 0.5f);
            const int32_t h3 = ee_read<int32_t>(pv + 0x120);
            if (far && h3 >= 0 && h3 < 0x400) {  // (a polygon wide enough to hold it: the range decides)
                const Vec4 g = ee_ref<Vec4>(m + 0x10);
                w32(T4, 4);
                const float xs[] = {-200.0f, 200.0f, 200.0f, -200.0f}, ys[] = {-200.0f, -200.0f, 200.0f, 200.0f};
                for (uint32_t i = 0; i < 4; ++i) ee_ref<Vec4>(T4 + 0x10 + i * 0x10) = Vec4{g.x + xs[i], g.y + ys[i], g.z, 1.0f};
                w32(OV(0x001b04b0) + u(h3) * 4, T4);
            }
            w16(OV(0x001b0830), 1);
            w16(OV(0x001b0830) + 2, index_of(c));
        }
    }
    if (ri(0, 4) == 0) w32(pv + 0x2d4, u(ri(0, 80)));  // (a second camera box)
    if (ri(0, 4) == 0) ee_ref<Vec4>(OV(0x00166dc0)) = around(ee_ref<Vec4>(m + 0x10), 2.0f, 30.0f, 1.0f, 4.0f);
    deal_hero_state();
    if (sighting) {
        w8(m + 0xa4, 0xff);  // (not hit: a hit's reaction takes it out of its state)
        w8(0x001413f4, 3);
        w8(pv + 0x15e, coin() ? 1u : 0u);
        if (coin()) {
            w16(pv + 0x156, 30);
            w8(pv + 0x15b, 0);
        }
        if (coin()) w32(0x001413d4, 0x59);
        w16(0x0013f658, 0);
        ee_ref<Vec4>(pv + 0xd0) = ee_ref<Vec4>(0x0013f3d0);
    }
    if (ri(0, 9) == 0) w32(OV(0x0015f5c4), 2);
    if (ri(0, 15) == 0) w32(guard_words() + 0x2c, 1);
    if (coin()) w32(guard_words(), ee_read<uint32_t>(OV(0x0015f5cc)));
    w16(0x00141630, u(ri(-1, 2)));
    w16(0x0013f658, u(ri(0, 1)));
    w32(OV(0x0015f5cc), R() & 0xffff);
    vary_frame_scale();
    seed_rand();
}

void guard_checks() {
    const Lv l = lv();
    if (l.c44 != 0 && info()) {
        level();
        std::printf("INFO class 44: %zu awake, %zu placed; 408: %zu; 78: %zu; 635: %zu; 655: %zu; 1179: %zu; 830: %zu\n", mobys_with(l.c44).size(),
                    placed_of(44).size(), placed_of(408).size(), placed_of(78).size(), placed_of(635).size(), placed_of(655).size(),
                    placed_of(1179).size(), placed_of(830).size());
        std::printf("INFO hero state %x substate %x\n", ee_read<uint32_t>(0x001413d4), ee_read<uint32_t>(0x001413dc));
        for (const uint32_t m : placed_of(44)) {
            const uint32_t pv = pvar(m);
            std::printf("INFO   44 %08x st %u home %d path %d alarm %d group %u allies %d boxes %d %d %d %d\n", m, ee_read<uint8_t>(m + 0x20),
                        ee_read<int32_t>(pv + 0x120), ee_read<int32_t>(pv + 0x124), ee_read<int32_t>(pv + 0x128), ee_read<uint8_t>(m + 0x21),
                        ee_read<int32_t>(pv + 0x17c), ee_read<int32_t>(pv + 0x2d0), ee_read<int32_t>(pv + 0x2d4), ee_read<int32_t>(pv + 0x2d8),
                        ee_read<int32_t>(pv + 0x2dc));
        }
    }
    // (the scratchpad words the pose function spills the caller's $t8 / $t9 into: batch 120 / 518's)
    const size_t ign = g_ignored_words.size();
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);
    collision_lanes();
    auto guards = [] { return of_class(44); };
    for (int st = 0; st < 25; ++st) {
        const size_t ign2 = g_ignored_words.size();
        if ((st == 21 || st == 23) && l.c44 != 0) {
            level();
            debris_lanes();
        }
        classcheck("class44_update", l.c44, guards, class44_update, [st](uint32_t m) {
            deal_guard(m, st);
            const uint32_t pv = pvar(m);
            // (arrived: at its home for states 9 / 20, at its path's point for 4 / 13; a shot's frame for 17 / 19)
            if ((st == 9 || st == 20) && coin()) ee_ref<Vec4>(pv + 0x160) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 0.19f, 0.0f, 0.0f);
            const int32_t path = ee_read<int32_t>(pv + 0x124);
            if ((st == 4 || st == 13) && coin() && path >= 0 && path < 0x400) {
                const uint32_t p = ee_read<uint32_t>(OV(0x001b04b0) + u(path) * 4);
                if (p >= 0x00100000u && p < 0x02000000u && ee_read<int32_t>(p) > 0)
                    ee_ref<Vec4>(m + 0x10) = around(ee_ref<Vec4>(p + 0x10 + ee_read<uint32_t>(pv + 0x13c) * 0x10), 0.0f, 0.15f, 0.0f, 0.0f);
            }
            if ((st == 17 || st == 19) && coin()) {
                w8(pv + 0x19e, 0);
                wf(m + 0x54, 0.0f);
                const uint32_t cur = ee_read<uint8_t>(m + 0x52) == 0xff ? ee_read<uint32_t>(m + 0x6c) : ee_read<uint32_t>(m + 0x68);
                if (cur >= 0x00100000u && cur < 0x02000000u) w16(cur + 4, u(static_cast<int32_t>(rf(0.9f, 2.9f) * 16.0f)));
            }
            if (st == 0 && coin()) w32(pv + 0x148, u(ri(-2, 6)));  // (its stop below 5 frames: raised to 5)
            if (st == 11 && coin()) w8(pv + 0x15c, 0xff);
            if ((st == 2 || st == 3 || st == 4) && coin()) w16(pv + 0x196, 1);  // (the ally search's time out)
            if (st == 2 && ri(0, 2) == 0) {  // (watching at home: no path, no class-78 moby, the hero waiting)
                w32(pv + 0x124, 0xffffffffu);
                w32(pv + 0x150, 0xffffffffu);
                w32(pv + 0x12c, 0xffffffffu);
                w16(pv + 0x154, 0);
                w32(0x001413d4, 0x59);
            }
            if (st == 14 && coin()) {  // (about 1.5 from its alarm: arriving or not)
                const int32_t a = ee_read<int32_t>(pv + 0x128);
                if (a >= 0 && a < 0x800) {
                    const Vec4 c = ee_ref<Vec4>(first_moby() + u(a) * 0x100 + 0x10);
                    const float ang = rf(-3.1f, 3.1f), d = coin() ? rf(1.21f, 1.29f) : rf(1.6f, 1.75f);
                    wf(pv + 0x134, 0.0f);  // (standing: it barely moves this frame)
                    ee_ref<Vec4>(m + 0x10) = Vec4{c.x + d * std::cos(ang), c.y + d * std::sin(ang), c.z, ee_ref<Vec4>(m + 0x10).w};
                }
            }
            if (st == 15 && coin()) {  // (its animation 11 .. 13 frames in, the same set)
                w8(m + 0x52, ee_read<uint8_t>(m + 0x53));
                wf(m + 0x54, 0.0f);
                const uint32_t cur = ee_read<uint32_t>(m + 0x68);
                if (cur >= 0x00100000u && cur < 0x02000000u) w16(cur + 4, u(static_cast<int32_t>(rf(11.0f, 13.0f) * 16.0f)));
            }
            w8(m + 0x20, u(st == 24 ? ri(24, 30) : st));
        }, st == 2 || st == 3 || st == 4 || st == 11 || st == 12 || st == 17 || st == 19 || st == 21 || st == 23 ? 60 : 30);
        g_ignored_words.resize(ign2);
    }
    for (const uint32_t quiet : {0u, 1u})
        tcheck("class44_think", l.think, quiet == 0 ? 200 : 60, Ret::None, [guards, quiet] {
            level();
            const uint32_t m = pick(guards());
            deal_guard(m, -1);
            w8(m + 0x20, ri(0, 6) == 0 ? 0u : (ri(0, 4) == 0 ? 0x17u : u(ri(1, 23))));
            return Args{{m, quiet}, {}};
        }, [](A a) { class44_think(a.i[0], a.i[1]); return uint64_t{0}; });
    tcheck("class44_aim", l.aim, 60, Ret::None, [guards] {
        level();
        const uint32_t m = pick(guards());
        deal_guard(m, -1);
        w8(m + 0x20, coin() ? u(ri(0x10, 0x11)) : u(ri(0, 23)));
        return Args{{m}, {}};
    }, [](A a) { class44_aim(a.i[0]); return uint64_t{0}; });
    tcheck("class44_find_ally", l.find, 60, Ret::Int32, [guards] {
        level();
        const uint32_t m = pick(guards());
        deal_guard(m, -1);
        // its ally group: its own, or one that holds allies; the allies about it
        const std::vector<uint32_t> allies = of_class(635);
        if (!allies.empty() && coin()) {
            const uint32_t a = pick(allies);
            w32(pvar(m) + 0x17c, ee_read<uint8_t>(a + 0x21));
            const bool far = coin();  // (all 7.2 .. 7.9 away: the nearest within 8 or not)
            for (const uint32_t o : allies)
                if (far || coin()) ee_ref<Vec4>(o + 0x10) = around(ee_ref<Vec4>(m + 0x10), far ? 7.2f : 1.0f, far ? 8.3f : 12.0f, 0.0f, 0.0f);
            if (far) w16(pvar(m) + 0x194, 0);
        }
        return Args{{m}, {}};
    }, [](A a) { return r32(class44_find_ally(a.i[0])); });
    tcheck("class44_turn_to", l.turn, 60, Ret::Int32, [guards] {
        level();
        const uint32_t m = pick(guards());
        deal_guard(m, -1);
        static const uint32_t ss[] = {2, 4, 0xc, 0x10, 0x14};
        return Args{{m, pvar(m), ss[ri(0, 4)], u(ri(0, 0xf))}, {ee_read<float>(m + 0x48) + rf(-3.1f, 3.1f)}};
    }, [](A a) { return r32(class44_turn_to(a.i[0], a.i[1], a.i[2], a.i[3], a.f[0])); });
    tcheck("class44_turn_toward", l.toward, 40, Ret::Int32, [guards] {
        level();
        const uint32_t m = pick(guards());
        deal_guard(m, -1);
        ee_ref<Vec4>(T) = around(ee_ref<Vec4>(m + 0x10), 0.5f, 10.0f);
        return Args{{m, pvar(m), T, u(ri(2, 0x14)), u(ri(0, 0xf))}, {}};
    }, [](A a) { return r32(class44_turn_toward(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4])); });
    // (the collision records' rows 0x173ea0 / 0x173eb0: VU0 leftovers of the original's sphere test that the native collision,
    // existing code, does not keep (COMMON's known difference of 0x173e40 .. 0x173ec0); seen once in 60 walks on level 17)
    const size_t ign_walk = g_ignored_words.size();
    for (uint32_t k = 0; k < 0xc; k += 4) {
        g_ignored_words.push_back(OV(0x00173ea0) + k);
        g_ignored_words.push_back(OV(0x00173eb0) + k);
    }
    tcheck("class44_walk_to", l.walk, 60, Ret::Int32, [guards] {
        level();
        const uint32_t m = pick(guards());
        deal_guard(m, -1);
        const float a = ee_read<float>(m + 0x48) + (coin() ? rf(-0.7f, 0.7f) : rf(-3.1f, 3.1f)), r = coin() ? rf(0.0f, 0.25f) : rf(0.25f, 8.0f);
        const Vec4 at = ee_ref<Vec4>(m + 0x10);
        ee_ref<Vec4>(T) = Vec4{at.x + r * std::cos(a), at.y + r * std::sin(a), at.z, at.w};
        if (ri(0, 3) == 0) {  // (standing still just about 0.2 from it: arrived or not)
            wf(pvar(m) + 0x134, 0.0f);
            const float d = rf(0.195f, 0.215f);
            ee_ref<Vec4>(T) = Vec4{at.x + d * std::cos(a), at.y + d * std::sin(a), at.z, at.w};
            return Args{{m, T}, {0.0f}};
        }
        return Args{{m, T}, {rf(0.0f, 0.2f)}};
    }, [](A a) { return r32(class44_walk_to(a.i[0], a.i[1], a.f[0])); });
    g_ignored_words.resize(ign_walk);
    tcheck("class44_alert_group", l.alert, 40, Ret::None, [guards] {
        level();
        const uint32_t m = pick(guards());
        deal_guard(m, -1);
        if (coin()) for (const uint32_t o : guards()) if (o != m && coin()) ee_ref<Vec4>(o + 0x10) = around(ee_ref<Vec4>(m + 0x10), 1.0f, 10.0f, -0.5f, 0.5f);
        if (ri(0, 4) == 0) w8(m + 0x21, 0xff);
        return Args{{m}, {}};
    }, [](A a) { class44_alert_group(a.i[0]); return uint64_t{0}; });
    tcheck("class44_group_calm", l.calm, 60, Ret::Int32, [guards] {
        level();
        const uint32_t m = pick(guards());
        deal_guard(m, -1);
        if (ri(0, 4) == 0) w8(m + 0x21, 0xff);
        for (const uint32_t o : guards()) {
            w8(o + 0xbc, u(ri(0, 2)));
            if (ri(0, 5) == 0) w8(o + 0x20, 0xe);
            if (coin()) w32(pvar(o) + 0x128, some_index(408, 4));
        }
        for (const uint32_t a : of_class(408)) w32(pvar(a) + 8, ri(0, 2) == 0 ? 1u : 0u);
        return Args{{m}, {}};
    }, [](A a) { return r32(class44_group_calm(a.i[0])); });
    tcheck("class44_path_first", l.first, 60, Ret::Int32, [guards] {
        level();
        const uint32_t m = pick(guards());
        deal_guard(m, -1);
        if (ri(0, 4) == 0) w8(m + 0x21, 0xff);
        for (const uint32_t o : guards()) {
            const uint32_t opv = pvar(o);
            w8(o + 0xbc, ri(0, 2) == 0 ? 1u : 0u);
            w32(opv + 0x144, ri(0, 2) == 0 ? 1u : 0u);
            if (ri(0, 4) == 0) w32(opv + 0x124, 0xffffffffu);
            w32(opv + 0x13c, u(ri(0, 6)));
            if (ri(0, 6) == 0) w8(o + 0x20, coin() ? 0xfeu : 0xfdu);
        }
        return Args{{m}, {}};
    }, [](A a) { return r32(class44_path_first(a.i[0])); });
    tcheck("class44_look_at", l.look, 40, Ret::None, [guards] {
        level();
        const std::vector<uint32_t> g = guards();
        const uint32_t m = pick(g), o = ri(0, 4) == 0 ? m : pick(g);
        w16(pvar(o) + 0x19c, ri(0, 1) == 0 ? 0u : u(ri(1, 50)));
        seed_rand();
        return Args{{m, o}, {}};
    }, [](A a) { class44_look_at(a.i[0], a.i[1]); return uint64_t{0}; });
    g_ignored_words.resize(ign);
}

// ---- class 408, the alarm (shared_class408.cpp)
void deal_alarm(uint32_t m) {
    const uint32_t pv = pvar(m);
    w32(pv, ri(0, 2) == 0 ? 2u : u(ri(0, 3)));
    if (ri(0, 3) == 0) w32(pv + 4, u(ri(0, 40)));
    w32(pv + 8, u(ri(0, 1)));
    { static const int32_t ts[] = {0, 1, 2, 50}; w32(pv + 0xc, u(ts[ri(0, 3)])); w32(pv + 0x18, u(ts[ri(0, 3)])); }
    w32(pv + 0x10, u(ri(0, 100)));
    wf(pv + 0x14, rf(-3.1f, 3.1f));
    deal_anim(m, {0, 1, 2, 3, 4, 5, 6, 7, 8});
    const uint32_t B = alarm_words();
    if (coin()) w32(B, ee_read<uint32_t>(OV(0x0015f5cc)));
    w32(B + 4, u(ri(0, 1)));
    { static const float lv[] = {0.0f, 0.01f, 0.5f, 1.0f}; wf(B + 8, lv[ri(0, 3)]); }
    w32(B + 0xc, u(ri(0, 1)));
    w32(B + 0x10, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 15)));
    w32(B + 0x14, ri(0, 2) == 0 ? 0u : m);
    if (ee_read<int32_t>(B + 0x10) >= 0 && coin()) {  // (its sound playing on its moby, or not)
        const uint32_t ch = 0x0013e550 + ee_read<uint32_t>(B + 0x10) * 0x70;
        w32(ch + 0x88, ee_read<uint32_t>(B + 0x14));
        w8(ch + 0x74, u(ri(0, 3)));
    }
    // its turrets: a class-78 group with sleeping turrets, the hero near it in sight
    const std::vector<uint32_t> turrets = of_class(78);
    if (!turrets.empty() && coin()) {
        const uint32_t t = pick(turrets);
        if (ee_read<uint8_t>(t + 0x21) != 0xff) w32(pv + 4, ee_read<uint8_t>(t + 0x21));
        for (const uint32_t o : turrets) w8(o + 0x20, coin() ? 1u : u(ri(0, 3)));
    }
    place_hero(m);
    if (coin()) hero_at(around(ee_ref<Vec4>(m + 0x10), coin() ? 15.0f : 1.0f, 16.0f, 0.0f, 1.0f));
    deal_hero_state();
    if (coin()) wf(0x0013f3d4, coin() ? rf(200.0f, 280.0f) : ee_read<float>(0x0013f3d4));
    if (ri(0, 5) == 0) w32(OV(0x0015f5c4), 2);
    if (ri(0, 6) == 0) w8(m + 0x21, 0xff);
    w32(OV(0x0015f5cc), R() & 0xffff);
    vary_frame_scale();
    seed_rand();
}
void alarm_checks() {
    const Lv l = lv();
    const size_t ign = g_ignored_words.size();
    collision_lanes();
    auto alarms = [] { return of_class(408); };
    for (int st = 0; st < 12; ++st)
        classcheck("class408_update", l.c408, alarms, class408_update, [st](uint32_t m) {
            deal_alarm(m);
            w8(m + 0x20, u(st == 11 ? ri(11, 20) : st));
        }, st == 8 ? 60 : 30);
    tcheck("class408_alarm", l.alarm, 60, Ret::None, [alarms] {
        level();
        const uint32_t m = pick(alarms());
        deal_alarm(m);
        return Args{{m}, {}};
    }, [](A a) { class408_alarm(a.i[0]); return uint64_t{0}; });
    tcheck("class408_raise", l.raise, 60, Ret::None, [alarms] {
        level();
        const uint32_t m = pick(alarms());
        deal_alarm(m);
        static const int32_t fs[] = {-1, 0, 1, 30, 0xb4};
        return Args{{m, u(fs[ri(0, 4)])}, {}};
    }, [](A a) { class408_raise(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
    tcheck("class408_wake", l.wake408, 40, Ret::None, [alarms] {
        level();
        const uint32_t m = pick(alarms());
        deal_alarm(m);
        w8(m + 0x20, u(ri(0, 4)));
        return Args{{m}, {}};
    }, [](A a) { class408_wake(a.i[0]); return uint64_t{0}; });
    g_ignored_words.resize(ign);
}

// ---- class 78 (shared_class78.cpp)
void deal_turret(uint32_t m) {
    const uint32_t pv = pvar(m);
    w32(pv + 0x10, some_index(44, 2));
    if (ee_read<int32_t>(pv + 0x10) != -1) w8(first_moby() + ee_read<uint32_t>(pv + 0x10) * 0x100 + 0x20, u(ri(1, 3)));
    w32(pv + 0x14, u(ri(55, 65)));
    const int32_t idx = ee_read<int16_t>(m + 0xb2);
    if (idx >= 0 && idx < 0x400) {
        w8(OV(0x001bb230) + u(idx) + 0x454, ri(0, 3) == 0 ? u(ri(1, 3)) : 0u);
        const uint32_t word = 0x0014c190 + (u(idx >> 5) << 2) + (ee_read<uint32_t>(0x0015ed84) << 8);
        const uint32_t bit = 1u << (idx & 0x1f);
        w32(word, ri(0, 3) == 0 ? (ee_read<uint32_t>(word) | bit) : (ee_read<uint32_t>(word) & ~bit));
    }
    w8(m + 0xb0, ri(0, 4) == 0 ? 0xffu : u(ri(0, 15)));
    const uint32_t flag = ee_read<uint8_t>(m + 0xb0);
    if (flag != 0xff) {
        w8(0x0014c050 + flag + (ee_read<uint32_t>(0x0015ed84) << 4), ri(0, 3) == 0 ? 0xffu : u(ri(0, 3)));
        w8(OV(0x0015fc88) + flag, ri(0, 3) == 0 ? 0xffu : u(ri(0, 3)));
    }
    // its sound channel: none, one playing on it, another's
    const int k = ri(0, 2);
    w8(m + 0xbc, k == 0 ? 0xffu : u(ri(0, 15)));
    if (k == 1) {
        const uint32_t ch = 0x0013e550 + ee_read<uint8_t>(m + 0xbc) * 0x70;
        w32(ch + 0x88, m);
        w8(ch + 0x74, u(ri(0, 1)));
    }
    // the hero's head in front of it (within 5 along its local y / z) or not
    const Vec4 at = ee_ref<Vec4>(m + 0x10);
    hero_at(around(at, 0.0f, coin() ? 4.0f : 20.0f, -1.0f, 6.0f));
    ee_ref<Vec4>(0x0013f420) = ee_ref<Vec4>(0x0013f3d0);
    wf(0x0013f428, ee_read<float>(0x0013f3d8) + rf(0.0f, 2.0f));
    wf(m + 0x48, rf(-3.1f, 3.1f));
    if (coin()) moby_update_transform(m);
    vary_frame_scale();
    seed_rand();
}
void turret_checks() {
    const Lv l = lv();
    auto turrets = [] { return of_class(78); };
    for (int st = 0; st < 4; ++st)
        classcheck("class78_update", l.c78, turrets, class78_update, [st](uint32_t m) {
            deal_turret(m);
            w8(m + 0x20, u(st == 3 ? ri(3, 9) : st));
        }, st == 1 ? 60 : 30);
    tcheck("class78_wake", l.wake78, 60, Ret::Int32, [turrets] {
        level();
        const std::vector<uint32_t> t = turrets();
        const uint32_t m = pick(t);
        for (const uint32_t o : t) {
            w8(o + 0x20, ri(0, 2) == 0 ? 1u : u(ri(0, 3)));
            w8(o + 0x53, coin() ? 0u : u(ri(1, 4)));
        }
        ee_ref<Vec4>(T) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 5.0f);
        place_hero(m);
        seed_rand();
        // the group: a turret's own (or another group of the level)
        return Args{{coin() ? ee_read<uint8_t>(m + 0x21) : u(ri(0, 40)), T}, {}};
    }, [](A a) { return r32(class78_wake(static_cast<int32_t>(a.i[0]), a.i[1])); });
    tcheck("class78_break", l.brk, 60, Ret::None, [turrets] {
        level();
        const std::vector<uint32_t> t = turrets();
        const uint32_t m = t.empty() || ri(0, 4) == 0 ? pick(live_mobys()) : pick(t);
        deal_turret(m);
        return Args{{m}, {}};
    }, [](A a) { class78_break(a.i[0]); return uint64_t{0}; });
    tcheck("particle_47_spawn", l.p47, 40, Ret::Int32, [turrets] {
        level();
        const std::vector<uint32_t> t = turrets();
        const uint32_t m = t.empty() ? pick(live_mobys()) : pick(t);
        ee_ref<Vec4>(T) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 3.0f);
        wfrand(T + 0x10, 4, -0.1f, 0.2f);
        wfrand(T + 0x20, 4, -0.1f, 0.2f);
        seed_rand();
        // (the ninth argument, the moby, on the caller's stack: the turret's update passes itself)
        return Args{{T, T + 0x10, T + 0x20, R(), R(), u(ri(0, 20)), u(ri(0, 20)), u(ri(0, 20)), m}, {}};
    }, [](A a) { return r32(particle_47_spawn(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5], a.i[6], a.i[7], a.i[8])); });
    tcheck("class934_shot_spawn", l.shot, 40, Ret::Int32, [] {
        level();
        const uint32_t m = pick(of_class(44).empty() ? live_mobys() : of_class(44));
        ee_ref<Vec4>(T) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 3.0f);
        wfrand(T + 0x10, 3, -0.3f, 0.3f);
        return Args{{m, T + 0x10, T}, {}};
    }, [](A a) { return r32(class934_shot_spawn(a.i[0], a.i[1], a.i[2])); });
}

// ---- class 655, the electric field (shared_class655.cpp)
void deal_field(uint32_t m, int st) {
    const uint32_t pv = pvar(m);
    // the hero: in the field's area (its box +0x124) mostly, on one of its ground boxes (+0x20 ..), its light boxes, or away
    const int32_t area = ee_read<int32_t>(pv + 0x124);
    const int k = ri(0, 5);
    if (k <= 2 && area >= 0 && area < 0x400) hero_at(ee_ref<Vec4>(box_centre(area)));
    if (k <= 1) {
        const int32_t g = ee_read<int32_t>(pv + 0x20 + u(ri(0, 11)) * 4);
        if (g >= 0 && g < 0x400) hero_at(ee_ref<Vec4>(box_centre(g)));
    } else if (k == 3) {
        const int32_t g = ee_read<int32_t>(pv + 0x60 + u(ri(0, 3)) * 4);
        if (g >= 0 && g < 0x400) hero_at(ee_ref<Vec4>(box_centre(g)));
    } else if (k == 4) {
        const int32_t g = ee_read<int32_t>(pv + (coin() ? 0x70u : 0x128u));
        if (g >= 0 && g < 0x400) hero_at(ee_ref<Vec4>(box_centre(g)));
    }
    ee_ref<Vec4>(0x0013f420) = ee_ref<Vec4>(0x0013f3d0);
    wf(0x0013f428, ee_read<float>(0x0013f3d8) + 1.0f);
    deal_hero_state();
    if (k <= 1 && coin()) w32(0x001413dc, 0x11);  // (on its ground in a scene's state: struck)
    static const int16_t ts[] = {0, 1, 2, 30, 200};
    w16(pv + 0x10, u(ts[ri(0, 4)]));
    w16(pv + 0x12, u(ts[ri(0, 4)]));
    w32(pv + 0x1c, u(ri(0, 2)));
    wf(pv + 0x18, rf(0.5f, 4.0f));
    w32(pv + 0x8c, u(ri(0, 1)));
    // its sounds: none, its own playing on the moby +0x84, others
    const std::vector<uint32_t> lives = live_mobys();
    w32(pv + 0x84, ri(0, 2) == 0 ? 0u : (coin() ? hero_moby() : pick(lives)));
    for (const uint32_t s : {0x14u, 0x80u}) {
        const int j = ri(0, 2);
        w32(pv + s, j == 0 ? 0xffffffffu : u(ri(0, 15)));
        if (j == 1) {
            const uint32_t ch = 0x0013e550 + ee_read<uint32_t>(pv + s) * 0x70;
            w32(ch + 0x88, ee_read<uint32_t>(pv + 0x84));
            w8(ch + 0x74, u(ri(0, 1)));
        }
    }
    w32(pv + 0x120, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 3)));
    // its switches (class 830) and lifts (class 1179) in state 2 or not, the hero on a switch or not
    for (uint32_t i = 0; i < 4; ++i) {
        const int32_t idx = ee_read<int32_t>(pv + i * 4);
        if (idx < 0 || idx > 0x800) continue;
        const uint32_t o = first_moby() + u(idx) * 0x100;
        w8(o + 0x20, ri(0, 2) == 0 ? 2u : u(ri(0, 3)));
        if (ri(0, 3) == 0) w32(0x0013f64c, o);
    }
    if (ri(0, 4) == 0) {  // (a switch made a lift)
        const int32_t idx = ee_read<int32_t>(pv + u(ri(0, 3)) * 4);
        if (idx >= 0 && idx < 0x800) w16(first_moby() + u(idx) * 0x100 + 0xa6, 1179);
    }
    w16(0x0013f65e, u(ri(0, 1)));
    w8(0x0013e533, u(ri(0, 1)));
    // the arc's words: its reset asked, its rates and offsets
    const uint32_t W = field_words();
    w32(W + 0x30, u(ri(0, 1)));
    wf(W + 8, ri(0, 3) == 0 ? 1.0f : 0.0f);
    for (uint32_t i = 0; i < 4; ++i) w16(W + 0xf8 + i * 2, u(ri(0, 3)));
    if (st >= 0) ee_ref<Vec4>(pv + 0xf0) = ee_ref<Vec4>(0x0013f420);
    if (coin()) ee_ref<Vec4>(pv + 0x110) = around(ee_ref<Vec4>(pv + 0xf0), 2.0f, 10.0f);
    if (coin()) ee_ref<Vec4>(pv + 0x100) = ee_ref<Vec4>(pv + 0x110);
    vary_frame_scale();
    w32(OV(0x0015f5cc), R() & 0xffff);
    seed_rand();
}
void field_checks() {
    const Lv l = lv();
    if (l.c655 != 0 && info()) {
        level();
        for (const uint32_t m : placed_of(655)) {
            const uint32_t pv = pvar(m);
            std::printf("INFO   655 %08x st %u switches %d %d %d %d area %d boxes %d %d emitters %d %d %d\n", m, ee_read<uint8_t>(m + 0x20),
                        ee_read<int32_t>(pv), ee_read<int32_t>(pv + 4), ee_read<int32_t>(pv + 8), ee_read<int32_t>(pv + 0xc),
                        ee_read<int32_t>(pv + 0x124), ee_read<int32_t>(pv + 0x20), ee_read<int32_t>(pv + 0x24), ee_read<int32_t>(pv + 0x74),
                        ee_read<int32_t>(pv + 0x78), ee_read<int32_t>(pv + 0x7c));
        }
    }
    const size_t ign = g_ignored_words.size();
    collision_lanes();
    auto fields = [] { return of_class(655); };
    for (int st = 0; st < 5; ++st)
        classcheck("class655_update", l.c655, fields, class655_update, [st](uint32_t m) {
            deal_field(m, st);
            if (st == 3 && coin()) {  // (the hero on its switch, holding it: its down time at the re-arm's edge)
                const uint32_t pv = pvar(m);
                const int32_t idx = ee_read<int32_t>(pv + u(ri(0, 3)) * 4);
                if (idx >= 0 && idx < 0x800) {
                    const uint32_t o = first_moby() + u(idx) * 0x100;
                    w16(o + 0xa6, 830);
                    w32(0x0013f64c, o);
                    w16(0x0013f65e, 0);
                    w32(pv + 0x8c, 1);
                    const int32_t full = float_to_int(frame_scale_f(fpu_mul(ee_read<float>(pv + 0x18), 60.0f)));
                    w16(pv + 0x12, u(full - frame_scale_i(0x3c) + ri(-1, 1)));
                }
            }
            w8(m + 0x20, u(st == 4 ? ri(4, 9) : st));
        }, st == 1 ? 120 : 40);
    tcheck("class655_arcs", l.arcs, 60, Ret::None, [fields] {
        level();
        const uint32_t m = pick(fields());
        deal_field(m, -1);
        return Args{{m}, {}};
    }, [](A a) { class655_arcs(a.i[0]); return uint64_t{0}; });
    tcheck("class655_arcs_reset", l.reset, 40, Ret::None, [fields] {
        level();
        const uint32_t m = pick(fields());
        deal_field(m, -1);
        return Args{{m}, {}};
    }, [](A a) { class655_arcs_reset(a.i[0]); return uint64_t{0}; });
    tcheck("class655_hero_grounded", l.grounded, 40, Ret::Int32, [fields] {
        level();
        const uint32_t m = pick(fields());
        deal_field(m, -1);
        if (coin()) w32(0x001413d4, u(ri(0x18, 0x19)));
        wf(0x0013f3d8, ee_read<float>(0x0013f3d8) + rf(-1.0f, 1.0f));
        return Args{{}, {}};
    }, [](A) { return r32(class655_hero_grounded()); });
    tcheck("class1179_lift", l.lift, 40, Ret::Int32, [] {
        level();
        const std::vector<uint32_t> lifts = of_class(1179);
        const uint32_t o = lifts.empty() || coin() ? pick(live_mobys()) : pick(lifts);
        if (coin()) w16(o + 0xa6, 1179);
        w8(o + 0x20, u(ri(0, 3)));
        return Args{{o}, {}};
    }, [](A a) { return r32(class1179_lift(a.i[0])); });
    g_ignored_words.resize(ign);
}
}  // namespace

void batch_516() {
    if (g_only_batch != 516) return;
    g_batch = 516;
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

    guard_checks();
    alarm_checks();
    turret_checks();
    field_checks();

    g_pre_call = nullptr;
    g_extra_noop_hooks.clear();
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
