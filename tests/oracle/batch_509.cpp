// Oracle batch 509 (stage 4 task F6, #149): the class code several levels carry a copy of, each family ported once as shared
// hand-written code (src/game/ovl/shared_*.cpp), against every level's own copy at that level's own address. The level copies
// stay transliterated in their units until each level is re-glued; here every copy is the original and the one shared port
// the port:
//  - shared_class827.cpp: class 827, a walker with a rider, its helpers and its class hooks (levels 6, 10);
//  - shared_class302.cpp: class 302's draw (an environment-mapped mesh) and class 303's update (levels 6, 10);
//  - shared_class1302.cpp, shared_class857.cpp: class 1302's draw (a sign and its ring) and class 857's (glows; levels 6, 10);
//  - shared_swingshot_target.cpp: the swingshot's targets (classes 758 / 803), the camera's look toward them and its hook's
//    install (levels 2 .. 7, 9 .. 18);
//  - shared_class819.cpp: class 819, a homing shot (levels 10, 11, 15, 17), on shots spawned for the check;
//  - shared_class1021.cpp: class 1021, a flower of petals that opens on a trigger (levels 6, 17, 18);
//  - shared_class832.cpp: class 832, the Visibomb's range: its update (on a Visibomb missile spawned for the check) and its
//    static's draw callback (levels 1 .. 15, 18);
//  - shared_class604.cpp: class 604, an animated mechanism (levels 1 .. 6, 8, 11, 12, 14);
//  - shared_particle_4a.cpp: particle 0x4a's spawn (levels 1 .. 6, 8, 11 .. 14, 17); shared_path_links.cpp: the links of a
//    path's points (levels 5, 6, 7, 10, 11);
//  - shared_class830.cpp: class 830, a floor switch (levels 5, 11, 15, 17, 18);
//  - shared_class1038.cpp: classes 1038 (a crystal's holder, its glow) and 1040 (the crystal, its burst; levels 6, 10, 17);
//  - shared_class1378.cpp: class 1378 and its draw callback, a field of rippling quads (levels 10, 17);
//  - shared_class1108.cpp: class 1108's draw callback, a countdown on the screen (levels 6, 15, 17, 18);
// Subjects: the level's own mobys of the class after the first gameplay frame (the port's; the screen, the projection and the
// view set up), perturbed, or (a level that places none) live mobys with pvars; the helpers on the class's mobys with
// arguments built from the level's state; the draw callbacks' subjects at the hero (whom the view looks at), their packets
// into a buffer of the test area. No RAM dumps.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 509, N = each level a family names (1 .. 18); a family the level does
// not carry says so and is not run. B509_INFO=1 prints, per draw check, how far each case's packets grew.

#include "ovl_oracle.h"

#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class1021.h>
#include <game/ovl/shared_class1108.h>
#include <game/ovl/shared_class1038.h>
#include <game/ovl/shared_class1302.h>
#include <game/ovl/shared_class1378.h>
#include <game/ovl/shared_class302.h>
#include <game/ovl/shared_class604.h>
#include <game/ovl/shared_class819.h>
#include <game/ovl/shared_class827.h>
#include <game/ovl/shared_class830.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_class857.h>
#include <game/ovl/shared_particle_4a.h>
#include <game/ovl/shared_path_links.h>
#include <game/ovl/shared_swingshot_target.h>
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

// A check whose original is the level's own copy at `laddr` (the port given the original's VU0 first).
void fcheck(const char* name, uint32_t laddr, int n, Ret ret, const std::function<Args()>& gen,
            const std::function<uint64_t(const Args&)>& port) {
    if (!selected(name)) return;
    std::printf("%-26s L%d:0x%08x\n", name, g_oracle_level, laddr);
    const std::function<uint64_t(const Args&)> synced = [&port](const Args& a) { vu0_sync(); return port(a); };
    if (!check_fn(name, laddr, n, ret, gen, synced, Scope::FullRam, 0, 0)) ++g_fail;
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
        // the screen and projection constants, which the oracle's display (no size) leaves broken: a 640 x 448 screen, as the
        // game's (batch 501's); and the view's matrices (the draw callbacks project through them, as batch 401's)
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

uint32_t pvar(uint32_t m) { return ee_read<uint32_t>(m + 0x78); }
uint32_t first_moby() { return ee_read<uint32_t>(OV(0x0015ffd8)); }
uint32_t end_moby() { return ee_read<uint32_t>(OV(0x0015ffe0)); }
bool live(uint32_t m) { return ee_read<uint8_t>(m + 0x20) < 0xfe; }

// the level's live mobys whose update function is `fn` (the level's address)
std::vector<uint32_t> mobys_with(uint32_t fn) {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < end_moby(); m += 0x100)
        if (live(m) && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
    return v;
}
// the level's live mobys of class `cls`
std::vector<uint32_t> mobys_of(uint32_t cls) {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < end_moby(); m += 0x100)
        if (live(m) && ee_read<uint16_t>(m + 0xa6) == cls) v.push_back(m);
    return v;
}
// the level's live mobys (with a class)
std::vector<uint32_t> live_mobys() {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < end_moby(); m += 0x100)
        if (live(m) && ee_read<uint32_t>(m + 0x24) != 0) v.push_back(m);
    return v;
}

uint32_t pick(const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; }

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
    w32(e + 0x20, ri(0, 3) == 0 ? 0u : (coin() ? ee_read<uint32_t>(0x001413d0) : first_moby() + 0x100 * u(ri(0, 40))));
}

// a point `r0`..`r1` from `at` (horizontally), `z0`..`z1` above it
Vec4 around(const Vec4& at, float r0, float r1, float z0 = -1.0f, float z1 = 2.0f) {
    const float r = rf(r0, r1), a = rf(-3.1f, 3.1f);
    return Vec4{at.x + r * std::cos(a), at.y + r * std::sin(a), at.z + rf(z0, z1), at.w};
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

// An update on one of the subjects `ms` (dealt again when the port's run throws, as batch 506's classcheck)
void classcheck(const char* name, uint32_t laddr, const std::function<std::vector<uint32_t>()>& subjects, void (*port)(uint32_t),
                const std::function<void(uint32_t)>& vary, int n) {
    if (!selected(name)) return;
    level();
    const std::vector<uint32_t> probe = subjects();
    if (probe.empty()) { std::printf("%-26s L%d:0x%08x  no subject on this level\n", name, g_oracle_level, laddr); return; }
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

// the frame's packet cursor (0x1611c0) on a buffer of the test area: after the first gameplay frame alone it is 0 (the
// drawing would write over the null page, which these checks leave out)
void packet_cursor() { w32(OV(0x001611c0), TEST_AREA + 0x8000 + u(ri(0, 0x40)) * 0x10); }

// a draw callback's case: B509_INFO=1 prints how far the port's packets grew (0: nothing drawn, the case compares nothing)
void draw_info(const char* name, uint32_t m, void (*port)(uint32_t)) {
    if (std::getenv("B509_INFO") == nullptr) return;
    const uint32_t before = ee_read<uint32_t>(OV(0x001611c0));
    static std::vector<uint8_t> saved;
    uint8_t* ram = ee_get_ram_base();
    saved.assign(ram, ram + 0x02000000);
    port(m);
    std::printf("INFO %s: the list grew by 0x%x\n", name, ee_read<uint32_t>(OV(0x001611c0)) - before);
    std::memcpy(ram, saved.data(), saved.size());
}

// the level's path (an index of the path table 0x1b04b0) with 2 .. 200 points, or -1
int32_t a_path() {
    std::vector<int32_t> ok;
    for (uint32_t i = 0; i < 64; ++i) {
        const uint32_t p = ee_read<uint32_t>(OV(0x001b04b0) + i * 4);
        if (p >= 0x00100000u && p < 0x02000000u && ee_read<int32_t>(p) >= 2 && ee_read<int32_t>(p) <= 200) ok.push_back(static_cast<int32_t>(i));
    }
    return ok.empty() ? -1 : ok[static_cast<size_t>(ri(0, static_cast<int>(ok.size()) - 1))];
}

// ---- class 827 (shared_class827.cpp)
void class827_checks() {
    const uint32_t upd = copy_here({{6, 0x002e8678}, {10, 0x002cc8b8}});
    const uint32_t react = copy_here({{6, 0x002e94f0}, {10, 0x002cd730}});
    const uint32_t sparks = copy_here({{6, 0x002ea198}, {10, 0x002ce1d0}});
    const uint32_t debris = copy_here({{6, 0x002e9f30}, {10, 0x002cdf68}});
    const uint32_t move = copy_here({{6, 0x002e9b60}, {10, 0x002cdda0}});
    const uint32_t pick_t = copy_here({{6, 0x002f34d8}, {10, 0x002d7510}});
    const uint32_t score = copy_here({{6, 0x002f33e8}, {10, 0x002d7420}});
    const uint32_t glow = copy_here({{6, 0x002ea498}, {10, 0x002ce4d0}});
    const uint32_t turn = copy_here({{6, 0x002e9998}, {10, 0x002cdbd8}});
    const uint32_t ground = copy_here({{6, 0x002e9aa8}, {10, 0x002cdce8}});
    const uint32_t hook0 = copy_here({{6, 0x002ea5e0}, {10, 0x002ce618}});
    const uint32_t hook4 = copy_here({{6, 0x002ea6a8}, {10, 0x002ce6e0}});
    const uint32_t hook8 = copy_here({{6, 0x002ea738}, {10, 0x002ce770}});
    const uint32_t hookc = copy_here({{6, 0x002ea7c8}, {10, 0x002ce800}});
    const char* names[] = {"class827_update", "class827_react", "class827_sparks", "class827_rider_debris", "class827_move",
                           "class827_target_pick", "class827_target_score", "class827_glow_burst", "class827_turn",
                           "class827_ground", "class827_hook_carry0", "class827_hook_carry", "class827_hook_carryc"};
    if (upd == 0) {
        for (const char* n : names) not_here(n);
        return;
    }
    // the class's tunings block (its own data: Blarg 0x161bf0, Orxon 0x161ae0)
    const uint32_t tun = g_oracle_level == 6 ? 0x00161bf0u : 0x00161ae0u;
    auto walkers = [upd] { return mobys_with(upd); };
    // a rider of its own (class 500, as the state 0 spawns it) on the walker, or none
    auto rider = [](uint32_t m) {
        const uint32_t r = moby_spawn(0x1f4);
        w32(pvar(m) + 0x20c, r);
        if (r == 0) return;
        for (uint32_t k = 0x38; k < 0x40; k += 4) w32(r + k, ee_read<uint32_t>(m + k));
        w16(r + 0x34, ee_read<uint16_t>(r + 0x34) | 0x4002u);
        ee_ref<Vec4>(r + 0x10) = ee_ref<Vec4>(m + 0x10);
        ee_ref<Vec4>(r + 0x40) = ee_ref<Vec4>(m + 0x40);
        w16(r + 0x32, 0x40);
        w8(r + 0x31, 1);
        moby_update_transform(r);
    };
    // a target near the walker: the hero's moby, a class-857 moby or another live one
    auto target = [](uint32_t m) {
        const uint32_t pv = pvar(m);
        const std::vector<uint32_t> mates = mobys_of(857), all = live_mobys();
        uint32_t t = ee_read<uint32_t>(0x001413d0);
        const int k = ri(0, 3);
        if (k == 1 && !mates.empty()) t = pick(mates);
        else if (k == 2) t = pick(all);
        w32(pv + 0x1c0, t);
        const float r = ri(0, 2) == 0 ? rf(0.0f, 2.5f) : (coin() ? rf(2.5f, 5.0f) : rf(5.0f, 40.0f));
        ee_ref<Vec4>(t + 0x10) = around(ee_ref<Vec4>(m + 0x10), r, r, -1.0f, 1.0f);
        if (t == ee_read<uint32_t>(0x001413d0)) ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(t + 0x10);
    };
    // the walker's state words and its surroundings dealt
    auto vary = [tun, rider, target](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        w32(tun + 0x20, ri(0, 12) == 0 ? 1u : 0u);
        if (coin()) w32(tun + 0x24, ri(0, 3) == 0 ? R() : u(ri(0, 40)));  // (the spawn counter: the frame slot +0x20a it deals)
        w32(OV(0x0015f5c4), u(ri(0, 2)));
        w16(m + 0x34, (ee_read<uint16_t>(m + 0x34) & ~1u) | (R() & 1));
        w16(pv + 0x1ea, u(ri(0, 1)));
        const int run = ri(0, 7);  // (mostly the state runs: the mode word and the flag +0x242 agree)
        w32(OV(0x0015fdf8), u(ri(0, 1)));
        w16(pv + 0x242, run == 0 ? u(ri(0, 1)) : (ee_read<uint32_t>(OV(0x0015fdf8)) != 0 ? 1u : 0u));
        // (a rider, or none; in state 1 always one, as the state 0 leaves it: the rider's debris retires it, and moby_retire of
        // no moby reads the null page's grid rect, where the original's grid search ends in its `teq` trap and the port's not)
        if (coin() || st == 1) rider(m);
        else w32(pv + 0x20c, 0);
        w16(pv + 0x200, ri(0, 3) == 0 ? 1u : 0u);
        w16(pv + 0x202, ri(0, 4) == 0 ? 1u : 0u);
        const int32_t path = coin() || st == 4 ? a_path() : -1;
        w32(pv + 0x1f8, u(path));
        if (path >= 0) {  // (in state 4: often at its point, the path's last but one: the jump to its end)
            const uint32_t p = ee_read<uint32_t>(OV(0x001b04b0) + u(path) * 4);
            const int32_t n = ee_read<int32_t>(p);
            const int32_t idx = coin() ? ri(0, n - 1) : std::max(0, n - 2 - ri(0, 1));
            w16(pv + 0x208, u(idx));
            if (coin() || st == 4) ee_ref<Vec4>(m + 0x10) = around(ee_ref<Vec4>(p + 0x10 + u(idx) * 0x10), 0.0f, coin() ? 0.3f : 3.0f, -0.2f, 0.2f);
        }
        w32(pv + 0x204, ri(0, 5) == 0 ? u(ri(0, 60)) : 0xffffffffu);
        w32(pv + 0x1c4, u(ri(0, 2)));
        w16(pv + 0x1e8, ri(0, 2) == 0 ? 0u : u(ri(1, 60)));
        w32(pv + 0x38, ri(0, 3) == 0 ? 1u : 0u);
        w16(pv + 0x240, u(ri(0, 2)));
        w8(pv + 0x14e, u(ri(0, 1)));
        if (coin()) wf(pv + 0x20, rf(-1.0f, 3.0f));
        target(m);
        // the hero within its range or not
        if (coin()) ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 10.0f : 50.0f);
        w8(0x001413f4, u(ri(0, 1)));
        w32(OV(0x0015f5cc), R() & 0xffff);
        if (coin()) w32(OV(0x0015f5cc), (ee_read<uint32_t>(OV(0x0015f5cc)) & ~7u) | (ee_read<uint16_t>(pv + 0x20a) & 7u));
        w32(0x001413d4, ri(0, 4) == 0 ? 0x51u : ee_read<uint32_t>(0x001413d4));
        // its animation: the set (+0x53), the one shown (+0x52), its frame (10 .. 15 for the attack: the frame record's +4)
        w8(m + 0x53, u(ri(0, 6)));
        w8(m + 0x52, coin() ? ee_read<uint8_t>(m + 0x53) : u(ri(0, 6)));
        if (coin()) {
            wf(m + 0x54, 0.0f);
            const uint32_t cur = ee_read<uint8_t>(m + 0x52) == 0xff ? ee_read<uint32_t>(m + 0x6c) : ee_read<uint32_t>(m + 0x68);
            if (cur >= 0x00100000u && cur < 0x02000000u) w16(cur + 4, u(ri(140, 260)));
        }
        w8(m + 0x70, R() & 0xff);
        w8(m + 0x31, u(ri(0, 1)));
        if (ri(0, 3) == 0) ee_ref<Vec4>(OV(0x00166dc0)) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 20.0f);
        // the wander: at its goal (+0x230), its timer +0x1fc at its edge
        if (coin()) ee_ref<Vec4>(pv + 0x230) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 0.04f : 4.0f, 0.0f, 0.0f);
        w32(pv + 0x1fc, u(ri(0, 2)));
        if (coin()) ee_ref<Vec4>(pv + 0x210) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 1.5f : 8.0f, -0.5f, 0.5f);
        give_hit(m, 0x330000u);
        if (ee_read<uint32_t>(pv + 0x20c) != 0 && ri(0, 3) == 0) give_hit(ee_read<uint32_t>(pv + 0x20c), 0x330000u);
        // carried (16): the carry record (class hook 0x10) in its state 7, falling onto the ground (the carry ends: 1) or not
        if (st == 16) {
            const uint32_t r = class_hook_10(m);
            if (r != 0) {
                w16(r + 0x68, ri(0, 2) != 0 ? 7u : u(ri(0, 8)));
                ee_ref<Vec4>(r + 0x10) = Vec4{rf(-0.05f, 0.05f), rf(-0.05f, 0.05f), -rf(0.0f, 0.5f), 0.0f};
                wf(m + 0x18, ee_read<float>(m + 0x18) + rf(0.0f, 0.3f));
            }
        }
        w8(m + 0x20, u(st));
        seed_rand();
    };

    // the update: every state (16: carried, 17.. none), each from the port's state 0 first (its rider spawned or not)
    const int only_st = std::getenv("B509_ST") != nullptr ? std::atoi(std::getenv("B509_ST")) : -1;  // (one state only: debugging)
    for (int st = 0; st < 18; ++st)
        if (only_st < 0 || only_st == st) classcheck("class827_update", upd, walkers, class827_update, [st, vary](uint32_t m) {
            const uint32_t pv = pvar(m);
            if (st != 0 && coin()) {
                w8(m + 0x20, 0);
                w16(pv + 0x200, u(ri(0, 1)));
                const uint32_t sp0 = ee_stack_ptr();
                try { class827_update(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
            }
            vary(m, st);
        }, st == 4 || st == 8 || st == 13 || st == 16 ? 50 : 30);

    fcheck("class827_react", react, 120, Ret::None, [walkers, vary] {
        uint32_t m = 0;
        for (int k = 0; k < 40; ++k) {
            level();
            m = pick(walkers());
            vary(m, ri(0, 16));
            if (trial([m] { class827_react(m); })) break;
        }
        return Args{{m}, {}};
    }, [](A a) { class827_react(a.i[0]); return uint64_t{0}; });

    fcheck("class827_sparks", sparks, 30, Ret::None, [walkers, tun] {
        level();
        const uint32_t m = pick(walkers());
        wfrand(TEST_AREA, 4, -2.0f, 2.0f);
        // (the level's sparks tunings varied: the push's scale +0x74 is 0 in the level's data)
        if (coin()) wf(tun + 0x74, rf(0.5f, 4.0f));
        if (coin()) w32(tun + 0x28, u(ri(1, 3)));
        if (coin()) w32(tun + 0x2c, u(ri(0, 3)));
        seed_rand();
        return Args{{m, coin() ? TEST_AREA : OV(0x0015f580)}, {}};
    }, [](A a) { class827_sparks(a.i[0], a.i[1]); return uint64_t{0}; });

    fcheck("class827_rider_debris", debris, 30, Ret::None, [walkers, rider] {
        level();
        const uint32_t m = pick(walkers());
        rider(m);
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { class827_rider_debris(a.i[0]); return uint64_t{0}; });

    // a step: about the walker's own speed, or long (cut at a wall), up and radius about its own, on its path or not
    fcheck("class827_move", move, 80, Ret::None, [walkers] {
        level();
        const uint32_t m = pick(walkers());
        const float len = coin() ? rf(0.0f, 0.4f) : rf(0.4f, 6.0f);
        const float a = rf(-3.1f, 3.1f);
        ee_ref<Vec4>(TEST_AREA) = Vec4{len * std::cos(a), len * std::sin(a), rf(-0.5f, 0.5f), 0.0f};
        if (ri(0, 2) == 0) ee_ref<Vec4>(m + 0x10) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 3.0f, -1.5f, 0.3f);
        return Args{{m, TEST_AREA, u(ri(0, 1))}, {rf(0.1f, 1.0f), coin() ? 0.3f : rf(0.05f, 2.0f)}};
    }, [](A a) { class827_move(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1]); return uint64_t{0}; });

    // the pick: the class-857 mobys' states dealt (some 0 / 1 / 9), the frame's list stale or fresh, the range
    fcheck("class827_target_pick", pick_t, 80, Ret::None, [walkers, tun] {
        level();
        const uint32_t m = pick(walkers());
        for (const uint32_t o : mobys_of(857)) {
            if (coin()) w8(o + 0x20, u(ri(0, 10)));
            if (coin()) ee_ref<Vec4>(o + 0x10) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 30.0f);
        }
        w32(OV(0x0015f5cc), R() & 0xffff);
        if (ri(0, 3) == 0) w32(tun + 0xf8, ee_read<uint32_t>(OV(0x0015f5cc)));
        if (coin()) ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 30.0f);
        ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10) = ee_ref<Vec4>(0x0013f3d0);
        wf(m + 0x48, rf(-3.1f, 3.1f));
        const std::vector<uint32_t> mates = mobys_of(857);
        w32(pvar(m) + 0x1c0, ri(0, 2) == 0 || mates.empty() ? ee_read<uint32_t>(0x001413d0) : pick(mates));
        return Args{{m, pvar(m) + 0x1c0}, {rf(0.0f, 60.0f)}};
    }, [](A a) { class827_target_pick(a.i[0], a.i[1], a.f[0]); return uint64_t{0}; });

    fcheck("class827_target_score", score, 60, Ret::None, [walkers] {
        level();
        const uint32_t m = pick(walkers());
        const std::vector<uint32_t> all = live_mobys();
        const uint32_t cand = pick(all);
        ee_ref<Vec4>(cand + 0x10) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 30.0f);
        wf(m + 0x48, rf(-3.1f, 3.1f));
        w32(TEST_AREA, R());
        wf(TEST_AREA + 4, coin() ? 999999.0f : rf(0.0f, 40.0f));
        return Args{{m, coin() ? cand : pick(all), cand, TEST_AREA, TEST_AREA + 4}, {rf(0.0f, 40.0f)}};
    }, [](A a) { class827_target_score(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.f[0]); return uint64_t{0}; });

    fcheck("class827_glow_burst", glow, 30, Ret::None, [walkers] {
        level();
        seed_rand();
        return Args{{pick(walkers())}, {}};
    }, [](A a) { class827_glow_burst(a.i[0]); return uint64_t{0}; });

    // the turn: up along the ground's normal (after a ground probe), the unit z or its own up, toward a random direction
    fcheck("class827_turn", turn, 60, Ret::None, [walkers] {
        level();
        const uint32_t m = pick(walkers());
        const int k = ri(0, 2);
        uint32_t up = m + 0xe0;
        if (k == 0) { wfrand(TEST_AREA + 0x10, 3, -1.0f, 1.0f); wf(TEST_AREA + 0x18, rf(0.5f, 1.0f)); up = TEST_AREA + 0x10; }
        else if (k == 1) up = OV(0x00173e80);
        wfrand(TEST_AREA, 4, -1.0f, 1.0f);
        if (ri(0, 4) == 0) wf(TEST_AREA + 8, 0.0f);
        return Args{{m, up, TEST_AREA}, {}};
    }, [](A a) { class827_turn(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });

    fcheck("class827_ground", ground, 60, Ret::Int32, [walkers] {
        level();
        const uint32_t m = pick(walkers());
        if (coin()) wf(m + 0x18, ee_read<float>(m + 0x18) + rf(-1.5f, 1.5f));
        return Args{{m}, {}};
    }, [](A a) { return ri32(class827_ground(a.i[0])); });

    // the class hooks: on the walkers (their class's hook table names them), in any state (16: carried), the carry record
    // as the game left it (+0x120); a0 the moby, a1 a position, a2 a target moby, f12 a float (batch 122's)
    auto hook_case = [walkers] {
        uint32_t m = 0;
        level();
        m = pick(walkers());
        const uint32_t first = first_moby();
        w8(m + 0x20, coin() ? 0x10u : u(ri(0, 16)));
        w8(m + 0x53, u(ri(0, 3)));
        const uint32_t tgt = ri(0, 3) == 0 ? 0u : first + 0x100 * u(ri(0, 100));
        return Args{{m, ri(0, 2) == 0 ? 0x0013f3d0u : m + 0x10, tgt}, {rf(-1, 3)}};
    };
    g_extra_noop_hooks.push_back(0x002763f8);  // (batch 120's)
    fcheck("class827_hook_carry0", hook0, 60, Ret::Int32, hook_case,
           [](A a) { return ri32(class827_hook_carry0(a.i[0], a.i[1], a.i[2], a.f[0])); });
    fcheck("class827_hook_carry", hook4, 60, Ret::Int32, hook_case,
           [](A a) { return ri32(class827_hook_carry(a.i[0], a.i[1], a.i[2], a.f[0], 4)); });
    fcheck("class827_hook_carry", hook8, 60, Ret::Int32, hook_case,
           [](A a) { return ri32(class827_hook_carry(a.i[0], a.i[1], a.i[2], a.f[0], 8)); });
    fcheck("class827_hook_carryc", hookc, 60, Ret::Int32, hook_case,
           [](A a) { return ri32(class827_hook_carryc(a.i[0], a.i[1], a.i[2], a.f[0])); });
    g_extra_noop_hooks.pop_back();
}

// ---- classes 302 / 303, 1302's and 857's draws (shared_class302.cpp, shared_class1302.cpp, shared_class857.cpp)
// a moby (of class `cls` when the level places one, else any live one) at the hero, whom the view looks at, or near him
uint32_t drawn_moby(uint32_t cls, float r) {
    std::vector<uint32_t> own = mobys_of(cls);
    const uint32_t m = own.empty() ? pick(live_mobys()) : pick(own);
    if (ri(0, 4) != 0) ee_ref<Vec4>(m + 0x10) = around(ee_ref<Vec4>(0x0013f3d0), 0.0f, r, -1.0f, 1.0f);
    return m;
}
void blarg_orxon_draw_checks() {
    const uint32_t d302 = copy_here({{6, 0x002d8738}, {10, 0x002bd280}});
    const uint32_t u303 = copy_here({{6, 0x002d8c20}, {10, 0x002bd768}});
    const uint32_t d1302 = copy_here({{6, 0x00307578}, {10, 0x002e7520}});
    const uint32_t d857 = copy_here({{6, 0x002f37d0}, {10, 0x002d7808}});
    if (d302 == 0) {
        for (const char* n : {"class302_draw", "class303_update", "class1302_draw", "class857_draw"}) not_here(n);
        return;
    }
    // class 302: its matrix the level's (turned at random sometimes), the camera within 16 units of it or not
    fcheck("class302_draw", d302, 60, Ret::None, [] {
        level();
        const uint32_t m = drawn_moby(302, 4.0f);
        if (coin()) {  // (or farther along the view: the mesh is a big one, the camera inside it at the hero's)
            const Vec4 c = ee_ref<Vec4>(OV(0x00166dc0)), h = ee_ref<Vec4>(0x0013f3d0);
            const float k = rf(1.0f, 6.0f);
            ee_ref<Vec4>(m + 0x10) = Vec4{c.x + (h.x - c.x) * k, c.y + (h.y - c.y) * k, c.z + (h.z - c.z) * k + rf(-3.0f, 1.0f), 1.0f};
        }
        if (std::getenv("B509_INFO") != nullptr) {
            const Vec4 p = ee_ref<Vec4>(m + 0x10), h = ee_ref<Vec4>(0x0013f3d0), c = ee_ref<Vec4>(OV(0x00166dc0));
            const Vec4 r0 = ee_ref<Vec4>(m + 0xc0), r2 = ee_ref<Vec4>(m + 0xe0);
            std::printf("INFO class302 subject class %u at %.1f %.1f %.1f hero %.1f %.1f %.1f cam %.1f %.1f %.1f rows %.2f %.2f %.2f / %.2f %.2f %.2f\n",
                        ee_read<uint16_t>(m + 0xa6), p.x, p.y, p.z, h.x, h.y, h.z, c.x, c.y, c.z, r0.x, r0.y, r0.z, r2.x, r2.y, r2.z);
        }
        if (ri(0, 3) != 0) {  // (turned about z, then tipped about x: its quads face the camera from inside or out)
            const float a = rf(-3.1f, 3.1f), b = rf(-3.1f, 3.1f), ca = std::cos(a), sa = std::sin(a), cb = std::cos(b), sb = std::sin(b);
            ee_ref<Vec4>(m + 0xc0) = Vec4{ca, sa, 0.0f, 0.0f};
            ee_ref<Vec4>(m + 0xd0) = Vec4{-sa * cb, ca * cb, sb, 0.0f};
            ee_ref<Vec4>(m + 0xe0) = Vec4{sa * sb, -ca * sb, cb, 0.0f};
        }
        if (ri(0, 2) != 0) ee_ref<Vec4>(OV(0x00166dc0)) = around(ee_ref<Vec4>(m + 0x10), 2.0f, coin() ? 14.0f : 30.0f, 0.0f, 6.0f);
        packet_cursor();
        draw_info("class302_draw", m, class302_draw);
        return Args{{m}, {}};
    }, [](A a) { class302_draw(a.i[0]); return uint64_t{0}; });
    // class 303: the moby it watches (none, in state 0 / 1, or later), the frame counter (negative too), its colour
    // (on the level's class-303 mobys; Blarg has none awake after the first frame: any live moby, the update reads +0xb8 / +0x90)
    classcheck("class303_update", u303, [u303] { const std::vector<uint32_t> v = mobys_with(u303); return v.empty() ? live_mobys() : v; }, class303_update, [](uint32_t m) {
        const int k = ri(0, 3);
        const std::vector<uint32_t> all = live_mobys();
        const uint32_t o = k == 0 ? 0u : pick(all);
        w32(m + 0xb8, o);
        if (o != 0) w8(o + 0x20, k == 1 ? u(ri(2, 12)) : u(ri(0, 1)));
        w32(OV(0x0015f5cc), coin() ? (R() & 0xffff) : R());
        w32(m + 0x90, R());
    }, 60);
    // class 1302: the scroll and the growth at their edges (about 1), the size
    fcheck("class1302_draw", d1302, 60, Ret::None, [] {
        level();
        const uint32_t m = drawn_moby(1302, 3.0f);
        if (ee_read<uint16_t>(m + 0xa6) != 1302) w32(m + 0x78, TEST_AREA + 0x800);
        const uint32_t pv = pvar(m);
        wf(pv + 0x1c, coin() ? rf(0.98f, 1.0f) : rf(0.0f, 1.0f));
        wf(pv + 0x20, coin() ? rf(0.98f, 1.0f) : rf(0.0f, 1.0f));
        wf(pv + 0x24, rf(0.3f, 3.0f));
        if (coin()) wf(OV(0x00166dd8), rf(-3.1f, 3.1f));
        packet_cursor();
        draw_info("class1302_draw", m, class1302_draw);
        return Args{{m}, {}};
    }, [](A a) { class1302_draw(a.i[0]); return uint64_t{0}; });
    // class 857: its group's mobys (shown or not, of its class or not), their three glow points about the hero
    fcheck("class857_draw", d857, 60, Ret::None, [] {
        level();
        const uint32_t m = drawn_moby(857, 2.0f);
        const uint32_t grp = ee_read<uint8_t>(m + 0x21);
        for (uint32_t o = first_moby(); o < end_moby(); o += 0x100) {
            if (!live(o) || ee_read<uint8_t>(o + 0x21) != grp || pvar(o) == 0) continue;
            w8(o + 0x31, ri(0, 3) == 0 ? 0u : 1u);
            for (uint32_t k = 0; k < 3; ++k)
                ee_ref<Vec4>(pvar(o) + 0x140 + k * 0x10) = around(ee_ref<Vec4>(0x0013f3d0), 0.0f, 3.0f, 0.0f, 2.0f);
        }
        w8(0x0015edb3, u(ri(0, 1)));
        packet_cursor();
        draw_info("class857_draw", m, class857_draw);
        return Args{{m}, {}};
    }, [](A a) { class857_draw(a.i[0]); return uint64_t{0}; });
}

// ---- the swingshot's targets (shared_swingshot_target.cpp)
void swingshot_target_checks() {
    const uint32_t upd = copy_here({{2, 0x002e0450}, {3, 0x002d0bd8}, {4, 0x002d9a58}, {5, 0x003098d8}, {6, 0x002e6270}, {7, 0x00302238},
                                    {9, 0x002f8c70}, {10, 0x002c9590}, {11, 0x00300010}, {12, 0x002f7b28}, {13, 0x002fa708}, {14, 0x002f2cb8},
                                    {15, 0x002db100}, {16, 0x002d6378}, {17, 0x002d95a8}, {18, 0x002e19d8}});
    const uint32_t look = copy_here({{2, 0x002fa7c8}, {3, 0x002eb4c0}, {4, 0x002f0500}, {5, 0x00329038}, {6, 0x00314868}, {7, 0x00328148},
                                     {9, 0x003142d8}, {10, 0x002f4bd0}, {11, 0x00327b48}, {12, 0x003149a8}, {13, 0x00316c00}, {14, 0x00313298},
                                     {15, 0x002f6a38}, {16, 0x002f3b40}, {17, 0x002ffff0}, {18, 0x003053d0}});
    const uint32_t install = copy_here({{2, 0x002fa6d8}, {3, 0x002eb3d0}, {4, 0x002f0410}, {5, 0x00328f48}, {6, 0x00314778}, {7, 0x00328058},
                                        {9, 0x003141e8}, {10, 0x002f4ae0}, {11, 0x00327a58}, {12, 0x003148b8}, {13, 0x00316b10}, {14, 0x003131a8},
                                        {15, 0x002f6948}, {16, 0x002f3a50}, {17, 0x002fff00}, {18, 0x003052e0}});
    if (upd == 0) {
        for (const char* n : {"swingshot_target_update", "swingshot_target_look", "cam0_level_hook_install"}) not_here(n);
        return;
    }
    // the targets: the level's own (classes 758 / 803), or (none placed) live mobys with pvars made targets of either class
    auto targets = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (v.empty())
            for (const uint32_t m : live_mobys())
                if (pvar(m) != 0 && v.size() < 8) v.push_back(m);
        return v;
    };
    // the target in the active camera's view (or not), the hero below it within 22 units (or not), the look block's best
    auto in_view = [](uint32_t m) {
        const uint32_t cam = ee_read<uint32_t>(OV(0x00166e00));
        if (cam < 0x00100000u || cam >= 0x02000000u) return;
        w16(cam + 0x86, ri(0, 5) == 0 ? 1u : 0u);
        const Vec4 t = ee_ref<Vec4>(m + 0x10);
        Vec4 f = ee_ref<Vec4>(cam);
        if (ri(0, 2) != 0) {  // (the camera's forward toward the target, a little off)
            f = Vec4{rf(-1.0f, 1.0f), rf(-1.0f, 1.0f), ri(0, 3) == 0 ? rf(-0.4f, 0.05f) : rf(0.05f, 0.4f), 0.0f};
            const float l = std::sqrt(f.x * f.x + f.y * f.y + f.z * f.z);
            if (l > 0.01f) { f.x /= l; f.y /= l; f.z /= l; }
            ee_ref<Vec4>(cam) = f;
            // the camera and the hero back from the target, off the forward by angles about the look's limits (36 and 64
            // degrees: 0.63 / 1.12)
            Vec4 side{-f.y, f.x, 0.0f, 0.0f};
            const float sl = std::sqrt(side.x * side.x + side.y * side.y);
            if (sl > 0.01f) { side.x /= sl; side.y /= sl; }
            auto back = [&](float dist, float ang) {
                const float c = std::cos(ang), s2 = std::sin(ang) * (coin() ? 1.0f : -1.0f);
                return Vec4{t.x - (f.x * c + side.x * s2) * dist, t.y - (f.y * c + side.y * s2) * dist, t.z - (f.z * c) * dist, 1.0f};
            };
            ee_ref<Vec4>(cam + 0x30) = back(rf(4.0f, 25.0f), coin() ? rf(0.45f, 0.7f) : rf(0.0f, 0.55f));
            Vec4 h = back(rf(0.5f, 24.0f), coin() ? rf(0.9f, 1.25f) : rf(0.0f, 1.0f));
            if (ri(0, 3) == 0) h.z = t.z - rf(-1.0f, 4.0f);
            ee_ref<Vec4>(0x0013f3d0) = h;
        }
        wf(OV(0x00167008), ri(0, 3) == 0 ? rf(0.0f, 20.0f) : 10000.0f);
        wf(OV(0x0016700c), rf(0.0f, 1.2f));
        wf(0x0013ca44, ri(0, 4) == 0 ? rf(-1.0f, 1.0f) : rf(-0.06f, 0.06f));
    };
    for (int st = 0; st < 4; ++st)
        classcheck("swingshot_target_update", upd, targets, swingshot_target_update, [st, in_view, upd](uint32_t m) {
            const uint32_t pv = pvar(m);
            // (its class: the level's own, or (one in three, or a moby made a target) 803, which turns and sparkles, or 758)
            if (ee_read<uint32_t>(m + 0x74) != upd || ri(0, 2) == 0) w16(m + 0xa6, coin() ? 0x323u : 0x2f6u);
            w8(m + 0x20, u(st < 3 ? st : ri(0, 5)));
            const uint32_t flag = u(ri(0, 15));
            w32(pv + 0x30, ri(0, 2) == 0 ? 0xffffffffu : flag);
            w8(0x0014c050 + flag + (ee_read<uint32_t>(0x0015ed84) << 4), u(ri(0, 1)));
            if (coin()) {
                w32(pv + 0x3c, 0xffffffffu);
            } else {
                const uint32_t idx = u(ri(0, 40));
                w32(pv + 0x3c, idx);
                static const uint8_t sts[] = {0, 1, 3, 0xfe, 0xfd};
                w8(first_moby() + (idx << 8) + 0x20, sts[ri(0, 4)]);
            }
            w32(OV(0x0015f5cc), R() & 0xffff);
            w8(m + 0x31, u(ri(0, 3) == 0 ? 0 : 1));
            wf(m + 0x48, rf(-3.1f, 3.1f));
            wf(pv + 0x38, rf(-0.2f, 0.2f));
            w32(0x0013f350 + 0x2084, ri(0, 4) == 0 ? 0x2cu : u(ri(0, 0x40)));
            in_view(m);
            seed_rand();
        }, 40);
    fcheck("swingshot_target_look", look, 120, Ret::None, [targets, in_view] {
        level();
        const uint32_t m = pick(targets());
        in_view(m);
        if (std::getenv("B509_INFO") != nullptr) {  // (whether the look takes this target: the block's +4 names it)
            static std::vector<uint8_t> saved;
            uint8_t* ram = ee_get_ram_base();
            saved.assign(ram, ram + 0x02000000);
            swingshot_target_look(m);
            const uint32_t cam = ee_read<uint32_t>(OV(0x00166e00));
            const Vec4 t = ee_ref<Vec4>(m + 0x10), h = ee_ref<Vec4>(0x0013f3d0), c = ee_ref<Vec4>(cam + 0x30), f = ee_ref<Vec4>(cam);
            const float rx = t.x - h.x, ry = t.y - h.y, rz = t.z - h.z, d1 = std::sqrt(rx * rx + ry * ry + rz * rz);
            const float sx = t.x - c.x, sy = t.y - c.y, sz = t.z - c.z, d2 = std::sqrt(sx * sx + sy * sy + sz * sz);
            std::printf("INFO swingshot_target_look: %s (86 %d stick %.3f d1 %.1f z %.1f a1 %.2f a2 %.2f best %.1f)\n",
                        ee_read<uint32_t>(OV(0x00167004)) == m ? "taken" : "not taken", ee_read<int16_t>(cam + 0x86), ee_read<float>(0x0013ca44),
                        d1, rz, std::acos((f.x * rx + f.y * ry + f.z * rz) / d1), std::acos((f.x * sx + f.y * sy + f.z * sz) / d2), ee_read<float>(OV(0x00167008)));
            std::memcpy(ram, saved.data(), saved.size());
        }
        return Args{{m}, {}};
    }, [](A a) { swingshot_target_look(a.i[0]); return uint64_t{0}; });
    fcheck("cam0_level_hook_install", install, 10, Ret::None, [] {
        level();
        wrand(OV(0x00167000), 5);
        return Args{{}, {}};
    }, [](A) { cam0_level_hook_install(); return uint64_t{0}; });
}

// ---- class 819 (shared_class819.cpp)
void class819_checks() {
    const uint32_t upd = copy_here({{10, 0x002cc2e8}, {11, 0x00302510}, {15, 0x002dd730}, {17, 0x002dbaa8}});
    if (upd == 0) { not_here("class819_update"); return; }
    // the shots: spawned near the hero (or before a solid moby, flying at it), aimed at the hero's moby mostly
    auto shots = [] {
        std::vector<uint32_t> v;
        std::vector<uint32_t> solid;
        for (const uint32_t o : live_mobys())
            if (ee_read<uint32_t>(o + 0x94) != 0) solid.push_back(o);
        for (int k = 0; k < 3; ++k) {
            const uint32_t s = moby_spawn(819);
            if (s == 0 || pvar(s) < 0x00100000u || pvar(s) >= 0x02000000u) break;
            const uint32_t pv = pvar(s);
            const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
            Vec4 at = around(h, 1.0f, 12.0f, 0.0f, 3.0f);
            Vec4 vel{rf(-0.5f, 0.5f), rf(-0.5f, 0.5f), rf(-0.2f, 0.2f), 0.0f};
            if (k == 2 && !solid.empty()) {  // (just before a solid moby, flying into it)
                const Vec4 q = ee_ref<Vec4>(pick(solid) + 0x10);
                at = around(q, 0.3f, 1.0f, 0.0f, 1.0f);
                vel = Vec4{(q.x - at.x) * 0.8f, (q.y - at.y) * 0.8f, (q.z + 0.5f - at.z) * 0.8f, 0.0f};
            }
            ee_ref<Vec4>(s + 0x10) = at;
            ee_ref<Vec4>(pv) = vel;
            w32(pv + 0x10, ri(0, 2) == 0 ? 0u : pick(live_mobys()));
            w32(pv + 0x14, ee_read<uint32_t>(0x001413d0));
            w32(pv + 0x18, u(ri(1, 200)));
            w8(s + 0x20, 1);
            v.push_back(s);
        }
        return v;
    };
    for (int pass = 0; pass < 8; ++pass)
        classcheck("class819_update", upd, shots, class819_update, [pass](uint32_t m) {
            const uint32_t pv = pvar(m);
            const uint32_t H = 0x0013f350;
            w8(m + 0x20, u(pass == 0 ? 2 : (pass == 7 ? ri(0, 3) : 1)));
            const int t = ri(0, 5);  // (its target: the hero's moby, another, none, or a retired one)
            if (t == 1) w32(pv + 0x14, pick(live_mobys()));
            else if (t == 2) w32(pv + 0x14, 0);
            else if (t == 3) { const uint32_t o = pick(live_mobys()); w8(o + 0x20, coin() ? 0xfeu : 0xfdu); w32(pv + 0x14, o); }
            w8(H + 0x20a4, u(ri(0, 4) == 0 ? ri(0, 3) : ri(1, 2)));
            ee_ref<Vec4>(H + 0x100) = Vec4{rf(-0.4f, 0.4f), rf(-0.4f, 0.4f), rf(-0.1f, 0.1f), rf(-1.0f, 1.0f)};
            if (ri(0, 3) == 0) ee_ref<Vec4>(H + 0x100) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
            wf(pv + 0x28, rf(-0.2f, 0.2f));
            wf(pv + 0x2c, rf(-0.2f, 0.2f));
            wf(m + 0x48, rf(-3.1f, 3.1f));
            wf(m + 0x44, rf(-1.0f, 1.0f));
            w32(pv + 0x30, u(ri(0, 1)));
            w32(pv + 0x34, ri(0, 3) == 0 ? 1u : 0u);
            const float d = std::sqrt(std::pow(ee_read<float>(m + 0x10) - ee_read<float>(0x0013f3d0), 2.0f) +
                                      std::pow(ee_read<float>(m + 0x14) - ee_read<float>(0x0013f3d4), 2.0f));
            wf(pv + 0x38, d + rf(-1.0f, 1.0f));
            if (ri(0, 3) == 0) ee_ref<Vec4>(m + 0x10) = around(ee_ref<Vec4>(0x0013f3d0), 0.5f, 5.0f, -0.5f, 2.0f);  // (passing him)
            w32(pv + 0x18, u(ri(0, 4) == 0 ? ri(0, 2) : ri(3, 200)));
            if (ri(0, 4) == 0) wf(0x0015ed6c, rf(0.01f, 0.05f));
            seed_rand();
        }, 30);
}

// ---- class 1021 (shared_class1021.cpp)
void class1021_checks() {
    const uint32_t upd = copy_here({{6, 0x002f4f00}, {17, 0x002e2c58}, {18, 0x002ec3e8}});
    if (upd == 0) { not_here("class1021_update"); return; }
    const uint32_t tun = g_oracle_level == 6 ? 0x00161d38u : g_oracle_level == 17 ? 0x00161dd0u : 0x001620ecu;
    if (std::getenv("B509_INFO") != nullptr) {
        level();
        for (const uint32_t m : mobys_with(upd))
            std::printf("INFO class1021 %08x state %u class %u petal0 %08x petal6 %08x\n", m, ee_read<uint8_t>(m + 0x20), ee_read<uint16_t>(m + 0xa6),
                        ee_read<uint32_t>(pvar(m) + 0x24), ee_read<uint32_t>(pvar(m) + 0x3c));
        std::printf("INFO class1021 spawn %08x\n", moby_spawn(0x3fd));
    }
    // the flowers and petals of the level (each state from the table's every entry and beyond); the trigger moby (none: the
    // hero; or a moby made class 0x5a8 / 0x24a / another), the hero near, the petals' states, the opening at its ends
    for (int pass = 0; pass < 14; ++pass) {
        const int st = pass < 11 ? pass : 2;  // (the trigger's state 2 four times)
        classcheck("class1021_update", upd, [upd, st] {
            // (waiting for the trigger, 2: the flowers, whose petals the state 0 spawned, when the level has them)
            std::vector<uint32_t> all = mobys_with(upd), flowers;
            for (const uint32_t m : all)
                if (ee_read<uint32_t>(pvar(m) + 0x24) >= 0x00100000u && ee_read<uint32_t>(pvar(m) + 0x24) < 0x02000000u) flowers.push_back(m);
            return st == 2 && !flowers.empty() ? flowers : all;
        }, class1021_update, [st, tun](uint32_t m) {
            const uint32_t pv = pvar(m);
            // (a moby without petals (the finale's only one waits in state 0 after the first frame): its petals spawned by
            // the port's state 0 first)
            const uint32_t p0 = ee_read<uint32_t>(pv + 0x24);
            if (st >= 2 && (p0 < 0x00100000u || p0 >= 0x02000000u) && ri(0, 3) != 0) {
                w8(m + 0x20, 0);
                const uint32_t sp0 = ee_stack_ptr();
                try { class1021_update(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
            }
            w8(m + 0x20, u(st));
            w8(m + 0xbc, ri(0, 5) == 0 ? 1u : 0u);
            const int k = ri(0, 3);
            if (k == 0) {
                w32(pv + 0x1c, 0xffffffffu);
            } else {
                const uint32_t idx = u(ri(0, 60));
                const uint32_t o = first_moby() + (idx << 8);
                w32(pv + 0x1c, idx);
                if (k == 1) w16(o + 0xa6, 0x5a8);
                else if (k == 2) w16(o + 0xa6, 0x24a);
                w8(o + 0xbc, u(ri(0, 1)));
                w8(o + 0x20, u(ri(1, 4)));
                if (ri(0, 3) != 0) ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(o + 0x10), 0.0f, 25.0f);
            }
            if (k == 0 && coin()) ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 6.5f : 20.0f);
            w32(tun + 0xc, ri(0, 3) == 0 ? 0x3f800000u : 0u);
            for (uint32_t i = 0; i < 7; ++i) {
                const uint32_t o = ee_read<uint32_t>(pv + 0x24 + i * 4);
                if (o >= 0x00100000u && o < 0x02000000u) w8(o + 0x20, ri(0, 24) == 0 ? u(ri(0, 9)) : 3u);
            }
            const float open = 1.0471976f;
            wf(pv + 0x14, coin() ? rf(open - 0.05f, open + 0.02f) : rf(-0.02f, 0.05f));
            wf(pv + 0x20, rf(-0.1f, 0.1f));
            wf(pv + 0x10, rf(-3.1f, 3.1f));
            if (ri(0, 4) == 0) wf(0x0015ed6c, rf(0.01f, 0.05f));
            seed_rand();
            if (std::getenv("B509_INFO") != nullptr && st == 0) {
                static std::vector<uint8_t> saved;
                uint8_t* ram = ee_get_ram_base();
                saved.assign(ram, ram + 0x02000000);
                class1021_update(m);
                const uint32_t p0 = ee_read<uint32_t>(pv + 0x24);
                std::printf("INFO class1021 st0: petal0 %08x angle %f state %u\n", p0, p0 ? ee_read<float>(p0 + 0x40) : 0.0f, ee_read<uint8_t>(m + 0x20));
                std::memcpy(ram, saved.data(), saved.size());
            }
        }, 30);
    }
}

// ---- class 832 (shared_class832.cpp)
void class832_checks() {
    const uint32_t upd = copy_here({{1, 0x00302648}, {2, 0x002e4a48}, {3, 0x002d40e8}, {4, 0x002dc168}, {5, 0x0030dac8}, {6, 0x002eaa40},
                                    {7, 0x00304e50}, {8, 0x002fbb58}, {9, 0x002fb288}, {10, 0x002cea78}, {11, 0x00303070}, {12, 0x002fa238},
                                    {13, 0x002fce18}, {14, 0x002f6338}, {15, 0x002de290}, {18, 0x002e4168}});
    const uint32_t drw = copy_here({{1, 0x00302438}, {2, 0x002e4838}, {3, 0x002d3ed8}, {4, 0x002dbf58}, {5, 0x0030d8b8}, {6, 0x002ea830},
                                    {7, 0x00304c40}, {8, 0x002fb948}, {9, 0x002fb078}, {10, 0x002ce868}, {11, 0x00302e60}, {12, 0x002fa028},
                                    {13, 0x002fcc08}, {14, 0x002f6128}, {15, 0x002de080}, {18, 0x002e3f58}});
    if (upd == 0) { not_here("class832_update"); not_here("class832_draw"); return; }
    if (std::getenv("B509_INFO") != nullptr) {
        level();
        for (const uint32_t m : mobys_with(upd))
            for (uint32_t i = 0; i < 20; ++i)
                if (ee_read<int32_t>(pvar(m) + i * 0x10) != -1 || ee_read<int32_t>(pvar(m) + i * 0x10 + 0xc) != -1)
                    std::printf("INFO class832 %08x rec %u poly %d z %.1f .. %.1f box %d\n", m, i, ee_read<int32_t>(pvar(m) + i * 0x10),
                                ee_read<float>(pvar(m) + i * 0x10 + 4), ee_read<float>(pvar(m) + i * 0x10 + 8), ee_read<int32_t>(pvar(m) + i * 0x10 + 0xc));
    }
    // the level's class-832 mobys (or, none placed, live mobys with pvars: the update reads its zone records and counter only)
    auto ranges = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (v.empty())
            for (const uint32_t m : live_mobys())
                if (pvar(m) != 0 && v.size() < 6) v.push_back(m);
        return v;
    };
    // the Visibomb's missile (class 172) the hero is flying: spawned (the level's class table has it), else a moby made one
    // (then the counter stays below the end: retiring a made-up class 172 would free it by its real class, batch 109's note)
    auto bomb = [](uint32_t m, bool& real) {
        uint32_t b = 0;
        const uint32_t sp0 = ee_stack_ptr();
        try { b = moby_spawn(0xac); } catch (const std::exception&) { ee_stack_ptr() = sp0; b = 0; }
        real = b != 0 && pvar(b) >= 0x00100000u && pvar(b) < 0x02000000u;
        if (!real) {
            const std::vector<uint32_t> all = live_mobys();
            b = pick(all);
            if (b == m) b = all[0];
            w16(b + 0xa6, 0xac);
        }
        w32(b + 0x28, 0);
        w32(OV(0x0015ffe4), b);
        return b;
    };
    for (int pass = 0; pass < 6; ++pass)
        classcheck("class832_update", upd, ranges, class832_update, [bomb, pass](uint32_t m) {
            const uint32_t pv = pvar(m);
            w32(0x001413d4, ri(0, 3) != 0 ? 0x1du : u(ri(0, 0x40)));
            bool real = false;
            const uint32_t b = ri(0, 5) != 0 ? bomb(m, real) : 0u;
            if (b == 0) w32(OV(0x0015ffe4), ri(0, 2) == 0 ? 0u : ee_read<uint32_t>(OV(0x0015ffe4)));
            // the zone records: the level's own (some), or dealt: a polygon (a path of the level) or none, a floor and a
            // ceiling about the missile, a box
            const bool own = coin() && ee_read<uint32_t>(m + 0x74) != 0;
            for (uint32_t i = 0; i < 20 && !own; ++i) {
                const uint32_t rec = pv + 0x10 * i;
                const int32_t path = ri(0, 3) == 0 ? a_path() : -1;
                w32(rec, u(path));
                wf(rec + 4, rf(-50, 50));
                wf(rec + 8, rf(-20, 100));
                w32(rec + 0xc, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 6)));
            }
            bool squared = false;
            if (b != 0 && !own && coin()) {  // (a square polygon of the test area about the missile, at path index 40, in a record)
                squared = true;
                const uint32_t sq = TEST_AREA + 0x3000;
                const Vec4 c = ee_ref<Vec4>(b + 0x10);
                const float h = rf(2.0f, 8.0f), off = coin() ? 0.0f : rf(-12.0f, 12.0f);
                w32(sq, 4);
                const float xs[4] = {-h, h, h, -h}, ys[4] = {-h, -h, h, h};
                for (uint32_t k = 0; k < 4; ++k) ee_ref<Vec4>(sq + 0x10 + k * 0x10) = Vec4{c.x + off + xs[k], c.y + ys[k], c.z, 1.0f};
                w32(OV(0x001b04b0) + 40 * 4, sq);
                const uint32_t rec = pv + 0x10 * u(ri(0, 19));
                w32(rec, 40);
                const int zk = ri(0, 2);  // (the missile between the floor and the ceiling, below, or above)
                wf(rec + 4, zk == 1 ? c.z + rf(0.1f, 3.0f) : c.z - rf(0.1f, 3.0f));
                wf(rec + 8, zk == 2 ? c.z - rf(0.0f, 0.05f) : c.z + rf(0.1f, 3.0f));
            }
            if (b != 0 && !squared) {  // (the missile in one of the records' polygons or boxes, or away)
                const uint32_t rec = pv + 0x10 * u(ri(0, 19));
                const int32_t poly = ee_read<int32_t>(rec), box = ee_read<int32_t>(rec + 0xc);
                Vec4 at = ee_ref<Vec4>(b + 0x10);
                if (poly >= 0 && poly < 64 && coin()) {
                    const uint32_t p = ee_read<uint32_t>(OV(0x001b04b0) + u(poly) * 4);
                    if (p >= 0x00100000u && p < 0x02000000u && ee_read<int32_t>(p) > 0 && ee_read<int32_t>(p) < 400) {
                        const int32_t n = ee_read<int32_t>(p);
                        if (coin()) {  // (the polygon's middle: inside it when it is convex)
                            Vec4 c{0.0f, 0.0f, 0.0f, 1.0f};
                            for (int32_t k = 0; k < n; ++k) { c.x += ee_read<float>(p + 0x10 + u(k) * 0x10) / n; c.y += ee_read<float>(p + 0x14 + u(k) * 0x10) / n; }
                            at.x = c.x; at.y = c.y;
                        } else {
                            at = around(ee_ref<Vec4>(p + 0x10 + u(ri(0, n - 1)) * 0x10), 0.0f, 3.0f, -1.0f, 1.0f);
                        }
                    }
                    const float lo = ee_read<float>(rec + 4), hi = ee_read<float>(rec + 8);
                    const int zk = ri(0, 3);  // (between its floor and ceiling, below, above, or anywhere)
                    at.z = zk == 0 ? (lo + hi) * 0.5f : zk == 1 ? lo - rf(0.1f, 5.0f) : zk == 2 ? hi + rf(0.1f, 5.0f) : rf(-60.0f, 110.0f);
                } else if (box >= 0 && box < 400 && coin()) {
                    const uint32_t bx = ee_read<uint32_t>(OV(0x001600ec)) + u(box << 7);
                    at = around(ee_ref<Vec4>(bx + 0x30), 0.0f, coin() ? 0.5f : 6.0f, -0.5f, 0.5f);
                } else {
                    at = around(ee_ref<Vec4>(0x0013f3d0), 0.0f, 40.0f, -5.0f, 20.0f);
                }
                ee_ref<Vec4>(b + 0x10) = at;
            }
            const int32_t end = 90;  // (frame_scale_i(0x5a) at the normal frame time)
            int32_t count = pass < 2 ? ri(0, 5) : ri(end - 3, end + 2);
            if (!real && count >= end - 1) count = ri(0, end - 3);
            w32(pv + 0x140, u(count));
            w32(OV(0x0015f30c), u(ri(0, 3) == 0 ? 0 : 1));
            seed_rand();
        }, 30);
    // the static: its counter (0 .. past the end), the screen's size, the cursor on a test buffer
    fcheck("class832_draw", drw, 40, Ret::None, [ranges] {
        level();
        const uint32_t m = pick(ranges());
        w32(pvar(m) + 0x140, u(ri(0, 100)));
        if (ri(0, 4) == 0) { w32(0x0013e500, u(ri(-40, 700))); w32(0x0013e504, u(ri(-40, 500))); }
        packet_cursor();
        draw_info("class832_draw", m, class832_draw);
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { class832_draw(a.i[0]); return uint64_t{0}; });
}

// ---- class 604 (shared_class604.cpp)
void class604_checks() {
    const uint32_t upd = copy_here({{1, 0x002f2b68}, {2, 0x002d7190}, {3, 0x002cb8e8}, {4, 0x002d4368}, {5, 0x00301988}, {6, 0x002e11c8},
                                    {8, 0x002f18b8}, {11, 0x002fc630}, {12, 0x002f51f0}, {14, 0x002ea9d0}});
    if (upd == 0) { not_here("class604_update"); return; }
    auto subjects = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (v.empty())
            for (const uint32_t m : live_mobys())
                if (pvar(m) != 0 && v.size() < 6) v.push_back(m);
        return v;
    };
    // each state of the table and beyond, what its owner asks (+0xbc), the animation playing and its end, the turn's flags
    for (int st = 0; st < 10; ++st)
        classcheck("class604_update", upd, subjects, class604_update, [st](uint32_t m) {
            const uint32_t pv = pvar(m);
            w8(m + 0x20, u(st));
            w8(m + 0xbc, u(ri(0, 8)));
            w8(m + 0x53, u(coin() ? ri(0, 6) : st > 0 ? st - 1 : 0));
            w8(m + 0x70, R() & 0xff);
            w8(0x0013d4c5, u(ri(0, 1)));
            w32(0x001404a8, coin() ? 5u : u(ri(0, 8)));
            wf(pv, rf(-3.1f, 3.1f));
            wf(pv + 4, rf(-0.3f, 0.3f));
            wf(m + 0x48, rf(-3.1f, 3.1f));
            if (ri(0, 4) == 0) wf(0x0015ed70, rf(0.0f, 0.002f));
        }, 30);
}

// ---- particle 0x4a's spawn, the path links (shared_particle_4a.cpp, shared_path_links.cpp)
void small_helper_checks() {
    const uint32_t spawn = copy_here({{1, 0x0028a7a8}, {2, 0x002768c8}, {3, 0x002637f0}, {4, 0x00267eb0}, {5, 0x0029f5f0}, {6, 0x00284190},
                                      {8, 0x0027f950}, {11, 0x0029a738}, {12, 0x0028de98}, {13, 0x00281100}, {14, 0x0027d310}, {17, 0x0026f1a8}});
    if (spawn == 0) {
        not_here("particle_4a_spawn");
    } else {
        // (the particle's +0x2c is the moby here, not a velocity's w lane: the batch's ignored pool words are put back for
        // this check)
        const std::vector<uint32_t> ign = g_ignored_words;
        const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
        std::vector<uint32_t> kept;
        for (const uint32_t a : ign)
            if (!(a >= pool && a < pool + 0x800 * 0x40 && (a - pool) % 0x40 == 0x2c)) kept.push_back(a);
        g_ignored_words = kept;
        fcheck("particle_4a_spawn", spawn, 60, Ret::Int32, [] {
            level();
            const uint32_t m = pick(live_mobys());
            ee_ref<Vec4>(TEST_AREA) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 3.0f, -1.0f, 2.0f);
            wfrand(TEST_AREA + 0x10, 4, -0.5f, 0.5f);
            wrand(TEST_AREA + 0x20, 4);
            w16(TEST_AREA + 0x2c, u(ri(1, 200)));
            seed_rand();
            return Args{{m, TEST_AREA, TEST_AREA + 0x10, TEST_AREA + 0x20, u(ri(0, 1))}, {}};
        }, [](A a) { return r32(particle_4a_spawn(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4])); });
        g_ignored_words = ign;
    }
    const uint32_t links = copy_here({{5, 0x0028be78}, {6, 0x00270b68}, {7, 0x0028a440}, {10, 0x00254ae8}, {11, 0x002873b0}});
    if (links == 0) { not_here("path_links_build"); return; }
    // a path of the level (up to 40 points), up to four wall paths (the level's own, or the path itself)
    fcheck("path_links_build", links, 40, Ret::None, [] {
        level();
        int32_t path = a_path();
        for (int k = 0; k < 20 && path >= 0 && ee_read<int32_t>(ee_read<uint32_t>(OV(0x001b04b0) + u(path) * 4)) > 40; ++k) path = a_path();
        const int32_t n = ri(0, 4);
        for (int32_t i = 0; i < n; ++i) w32(TEST_AREA + u(i) * 4, u(ri(0, 3) == 0 ? path : a_path()));
        return Args{{TEST_AREA, u(n), u(path < 0 ? 0 : path)}, {}};
    }, [](A a) { path_links_build(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2]); return uint64_t{0}; });
}

// ---- class 830 (shared_class830.cpp)
void class830_checks() {
    const uint32_t upd = copy_here({{5, 0x0030c5b0}, {11, 0x00302ae0}, {15, 0x002ddd00}, {17, 0x002dc078}, {18, 0x002e1508}});
    if (upd == 0) { not_here("class830_update"); return; }
    auto subjects = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (v.empty())
            for (const uint32_t m : live_mobys())
                if (pvar(m) != 0 && v.size() < 6) v.push_back(m);
        return v;
    };
    // its flags (the index +0xb2 into the tables 0x1ba5d0 / 0x1bb230 and the level's words 0x14c190, the flag +0xb0 in the
    // level's list 0x15fc88 and bytes 0x14c050), the hero on it or not, its path's points marked as it
    for (int st = 0; st < 3; ++st)
        for (int pass = 0; pass < 2; ++pass)
            classcheck("class830_update", upd, subjects, class830_update, [st](uint32_t m) {
                const uint32_t pv = pvar(m);
                const uint32_t level = ee_read<uint32_t>(0x0015ed84);
                w8(m + 0x20, u(st));
                const int32_t idx = ri(0, 3) == 0 ? ri(-8, 8) : ri(0, 120);
                w16(m + 0xb2, u(idx));
                const uint32_t flag = ri(0, 3) == 0 ? 0xffu : u(ri(0, 15));
                w8(m + 0xb0, flag);
                w8(OV(0x001bb230) + u(idx) + 0x454, ri(0, 3) == 0 ? u(ri(1, 5)) : 0u);
                w8(OV(0x001ba5d0) + u(idx) + 0x454, R() & 0xff);
                const uint32_t wa = 0x0014c190 + (u(idx >> 5) << 2) + (level << 8);
                if (coin()) w32(wa, R() & R());
                if (flag != 0xff) {
                    w8(OV(0x0015fc88) + flag, coin() ? 0xffu : R() & 0xff);
                    w8(0x0014c050 + flag + (level << 4), coin() ? 0xffu : R() & 0xff);
                }
                w32(pv + 8, u(ri(0, 1)));
                w32(0x0013f350 + 0x2fc, ri(0, 3) != 0 ? m : 0u);
                w16(0x0013f350 + 0x30e, ri(0, 3) == 0 ? 1u : 0u);
                const int32_t path = ri(0, 3) == 0 ? -1 : a_path();
                w32(pv, u(path));
                const float mark = rf(1.0f, 9.0f);
                wf(pv + 4, mark);
                if (path >= 0) {
                    const uint32_t p = ee_read<uint32_t>(OV(0x001b04b0) + u(path) * 4);
                    const int32_t n = ee_read<int32_t>(p);
                    for (int32_t i = 0; i < n && i < 200; ++i) if (coin()) wf(p + 0x1c + u(i) * 0x10, coin() ? mark : rf(0.0f, 9.0f));
                }
                wf(pv + 0xc, rf(-3.1f, 3.1f));
                seed_rand();
            }, 30);
}

// ---- classes 1038 / 1040 (shared_class1038.cpp)
void class1038_checks() {
    const uint32_t u1038 = copy_here({{6, 0x002f7288}, {10, 0x002d9550}, {17, 0x002e3690}});
    const uint32_t ginit = copy_here({{6, 0x002f7548}, {10, 0x002d9810}, {17, 0x002e3950}});
    const uint32_t glow = copy_here({{6, 0x002f7628}, {10, 0x002d98f0}, {17, 0x002e3a30}});
    const uint32_t u1040 = copy_here({{6, 0x002f7ab8}, {10, 0x002d9bf8}, {17, 0x002e3d38}});
    const uint32_t burst = copy_here({{6, 0x002f7c68}, {10, 0x002d9da8}, {17, 0x002e3ee8}});
    if (u1038 == 0) {
        for (const char* n : {"class1038_update", "class1038_glow_init", "class1038_glow", "class1040_update", "class1040_burst"}) not_here(n);
        return;
    }
    auto holders = [u1038] {
        std::vector<uint32_t> v = mobys_with(u1038);
        if (v.empty())
            for (const uint32_t m : live_mobys())
                if (pvar(m) != 0 && v.size() < 6) v.push_back(m);
        return v;
    };
    // a holder with its crystal: its own (the state 0 spawned it) or one spawned by the port's state 0 now
    auto with_crystal = [](uint32_t m) {
        const uint32_t pv = pvar(m);
        const uint32_t c = ee_read<uint32_t>(pv + 0x40);
        if (c >= 0x00100000u && c < 0x02000000u && ((c - first_moby()) & 0xff) == 0) return;
        w8(m + 0x20, 0);
        w16(m + 0xb2, u(ri(0, 60)));
        const uint32_t word = 0x0014c190 + (u(static_cast<int16_t>(ee_read<uint16_t>(m + 0xb2)) >> 5) << 2) + (ee_read<uint32_t>(0x0015ed84) << 8);
        w32(word, 0);
        const uint32_t sp0 = ee_stack_ptr();
        try { class1038_update(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
    };
    auto holder_state = [with_crystal](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        if (st != 0) with_crystal(m);
        w8(m + 0x20, u(st));
        const int32_t idx = ri(0, 4) == 0 ? ri(-40, 0) : ri(0, 120);
        w16(m + 0xb2, u(idx));
        const uint32_t level = ee_read<uint32_t>(0x0015ed84);
        const uint32_t word = 0x0014c190 + (u(idx >> 5) << 2) + (level << 8);
        w32(word, coin() ? (R() & R()) : R());
        w32(OV(0x001ba4d0) + (u(idx >> 5) << 2), R());
        w8(m + 0xb0, u(ri(0, 15)));
        w8(0x0014c050 + ee_read<uint8_t>(m + 0xb0) + 0xa0, coin() ? 0xffu : R() & 0xff);
        const uint32_t c = ee_read<uint32_t>(pv + 0x40);
        if (st == 1 && c >= 0x00100000u && c < 0x02000000u) w8(c + 0x20, ri(0, 3) == 0 ? (coin() ? 0xfeu : 0xfdu) : u(ri(0, 2)));
        give_hit(m, 0x330000u);
        if (std::getenv("B509_INFO") != nullptr && st == 1)
            std::printf("INFO class1038 st1: a4 %02x hit %08x crystal %08x state %02x\n", ee_read<uint8_t>(m + 0xa4), sub_0025a420(m, 0x330000u, 1),
                        ee_read<uint32_t>(pv + 0x40), ee_read<uint8_t>(ee_read<uint32_t>(pv + 0x40) + 0x20));
        for (uint32_t i = 0; i < 4; ++i) {
            wf(pv + i * 4, coin() ? rf(-5.0f, 5.0f) : rf(250.0f, 260.0f));
            w32(pv + 0x20 + i * 4, u(ri(0, 3) == 0 ? ri(0, 2) : ri(3, 300)));
        }
        w32(OV(0x0015f5cc), R() & 0xffff);
        w8(m + 0x31, u(ri(0, 3) == 0 ? 0 : 1));
        seed_rand();
    };
    for (int pass = 0; pass < 9; ++pass)
        classcheck("class1038_update", u1038, holders, class1038_update, [holder_state, pass](uint32_t m) { holder_state(m, pass % 3); }, 30);
    fcheck("class1038_glow_init", ginit, 30, Ret::None, [holders] {
        level();
        seed_rand();
        return Args{{pick(holders())}, {}};
    }, [](A a) { class1038_glow_init(a.i[0]); return uint64_t{0}; });
    fcheck("class1038_glow", glow, 60, Ret::None, [holders, holder_state] {
        uint32_t m = 0;
        for (int k = 0; k < 40; ++k) {
            level();
            m = pick(holders());
            holder_state(m, 1);
            if (trial([m] { class1038_glow(m); })) break;
        }
        return Args{{m}, {}};
    }, [](A a) { class1038_glow(a.i[0]); return uint64_t{0}; });
    // the crystals: the level's own, else spawned by a holder's state 0 (the port's)
    auto crystals = [u1040, holders, with_crystal] {
        std::vector<uint32_t> v = mobys_with(u1040);
        if (v.empty()) {
            const std::vector<uint32_t> hs = holders();
            for (size_t k = 0; k < hs.size() && k < 3; ++k) {
                with_crystal(hs[k]);
                const uint32_t c = ee_read<uint32_t>(pvar(hs[k]) + 0x40);
                if (c >= 0x00100000u && c < 0x02000000u) v.push_back(c);
            }
        }
        return v;
    };
    for (int pass = 0; pass < 6; ++pass)
        classcheck("class1040_update", u1040, crystals, class1040_update, [pass](uint32_t m) {
            const uint32_t pv = pvar(m);
            w8(m + 0x20, u(pass % 3));
            give_hit(m, 0x330000u);
            const int32_t ch = ri(0, 2) == 0 ? -1 : ri(0, 15);
            w32(pv + 0x60, u(ch));
            if (ch >= 0) {
                const uint32_t slot = 0x0013e550 + u(ch) * 0x70;
                w32(slot + 0x88, coin() ? m : 0u);
                w8(slot + 0x74, u(ri(0, 1)));
            }
            wfrand(m + 0x40, 3, -3.1f, 3.1f);
            seed_rand();
        }, 30);
    fcheck("class1040_burst", burst, 30, Ret::None, [crystals] {
        level();
        const std::vector<uint32_t> cs = crystals();
        const uint32_t m = cs.empty() ? pick(live_mobys()) : pick(cs);
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { class1040_burst(a.i[0]); return uint64_t{0}; });
}

// ---- class 1378 (shared_class1378.cpp)
void class1378_checks() {
    const uint32_t upd = copy_here({{10, 0x002e9028}, {17, 0x002e9608}});
    const uint32_t drw = copy_here({{10, 0x002e91b8}, {17, 0x002e9798}});
    if (upd == 0) { not_here("class1378_update"); not_here("class1378_draw"); return; }
    auto fields = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (v.empty())
            for (const uint32_t m : live_mobys())
                if (pvar(m) != 0 && v.size() < 6) v.push_back(m);
        return v;
    };
    // the hero within 8 units or beyond, his centre (0x13f420) about the moby: in its box or out of it, in front or behind
    for (int st = 0; st < 3; ++st)
        for (int pass = 0; pass < 2; ++pass)
            classcheck("class1378_update", upd, fields, class1378_update, [st](uint32_t m) {
                w8(m + 0x20, u(st));
                const Vec4 p = ee_ref<Vec4>(m + 0x10);
                ee_ref<Vec4>(0x0013f3d0) = around(p, 0.0f, coin() ? 7.5f : 12.0f, -1.0f, 3.0f);
                // (his centre placed in the moby's own coordinates: about the box's edges and the 0.5 of y)
                const float lx = rf(-6.0f, 6.0f), ly = coin() ? rf(0.3f, 0.6f) : rf(-1.5f, 2.0f), lz = rf(-1.0f, 9.0f);
                const Vec4 r0 = ee_ref<Vec4>(m + 0xc0), r1 = ee_ref<Vec4>(m + 0xd0), r2 = ee_ref<Vec4>(m + 0xe0);
                ee_ref<Vec4>(0x0013f420) = Vec4{p.x + lx * r0.x + ly * r1.x + lz * r2.x, p.y + lx * r0.y + ly * r1.y + lz * r2.y,
                                                p.z + lx * r0.z + ly * r1.z + lz * r2.z, 1.0f};
                w8(0x0014161b, u(ri(0, 2)));
            }, 40);
    fcheck("class1378_draw", drw, 60, Ret::None, [fields] {
        level();
        const uint32_t m = pick(fields());
        if (ri(0, 4) != 0) ee_ref<Vec4>(m + 0x10) = around(ee_ref<Vec4>(0x0013f3d0), 0.0f, 3.0f, -1.0f, 1.0f);
        if (ee_read<uint32_t>(m + 0x74) == 0 || coin()) { wf(pvar(m), rf(0.3f, 3.0f)); wf(pvar(m) + 4, rf(0.3f, 3.0f)); }
        // (the level's corners lie flat: some given a height, so that their scale in z shows)
        const uint32_t tab = g_oracle_level == 17 ? 0x001d98a0u : 0x001da250u;
        if (coin()) for (uint32_t k = 0; k < 8; ++k) wf(tab + 0xc0 + k * 0x10 + 8, rf(-0.5f, 0.5f));
        w32(OV(0x0015f3f8), R() & 0xffff);
        packet_cursor();
        draw_info("class1378_draw", m, class1378_draw);
        return Args{{m}, {}};
    }, [](A a) { class1378_draw(a.i[0]); return uint64_t{0}; });
}

// ---- class 1108's draw (shared_class1108.cpp)
void class1108_checks() {
    const uint32_t drw = copy_here({{6, 0x003021d8}, {15, 0x002d77c0}, {17, 0x002d5e38}, {18, 0x002d8098}});
    if (drw == 0) { not_here("class1108_draw"); return; }
    // on the level's class-1108 moby (or another with pvars), its time left (each level's word: Blarg +0xb4, Quartu and the
    // fleet +0x12, the finale +0x6c) dealt, the frame counter
    fcheck("class1108_draw", drw, 60, Ret::None, [] {
        level();
        std::vector<uint32_t> own = mobys_of(1108);
        if (own.empty())
            for (const uint32_t m : live_mobys())
                if (pvar(m) != 0 && own.size() < 6) own.push_back(m);
        const uint32_t m = pick(own), pv = pvar(m);
        const int32_t steps[] = {0xe10, 0x258, 0x3c, 6};  // (often at a digit's edge)
        const int32_t t = ri(0, 4) == 0 ? ri(-500, 100) : coin() ? ri(0, 40000) : steps[ri(0, 3)] * ri(1, 12) + ri(-2, 2);
        w32(pv + 0xb4, u(t));
        w16(pv + 0x12, u(t));
        w32(pv + 0x6c, u(t));
        w32(OV(0x0015f5cc), R() & 0xffff);
        if (ri(0, 4) == 0) wf(0x0015ed60, rf(0.5f, 3.0f));
        packet_cursor();
        draw_info("class1108_draw", m, class1108_draw);
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { class1108_draw(a.i[0]); return uint64_t{0}; });
}
}  // namespace

void batch_509() {
    if (g_only_batch != 509) return;
    g_batch = 509;
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
    // the debris / spawns leave w lanes of a VU0 sin / cos in the original (the port has none); the particle pool's velocity
    // w lanes (a stale lane of vec_from_spherical in the original): batches 120 / 506
    for (uint32_t a = end_moby(); a < end_moby() + 0x20000; a += 0x80) {
        g_ignored_words.push_back(a + 0xc);
        g_ignored_words.push_back(a + 0x1c);
    }
    // the segment record 0x173e90..0x173e9c and the collision results' w lanes (0x173e40 .. 0x173fc0, + 0xc of each row): VU0
    // leftovers in the original that the ports of segment_collide / sphere_collide do not reproduce (batch 506's)
    for (uint32_t k = 0; k < 0x10; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
    for (uint32_t a = OV(0x00173e40); a < OV(0x00173fc0); a += 0x10) g_ignored_words.push_back(a + 0xc);
    {
        const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
        for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c);
    }
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

    class827_checks();
    blarg_orxon_draw_checks();
    swingshot_target_checks();
    class819_checks();
    class1021_checks();
    class832_checks();
    class604_checks();
    small_helper_checks();
    class830_checks();
    class1038_checks();
    class1378_checks();
    class1108_checks();

    //@@NEXT
    g_pre_call = nullptr;
    g_extra_noop_hooks.clear();
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
