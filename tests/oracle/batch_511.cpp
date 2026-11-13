// Oracle batch 511 (stage 4 task G4, #149): hand-written ports of Novalis made to serve every level that carries a copy of the
// same code, each against every level's own copy at that level's own address (the copies stay transliterated in their units
// until each level is re-glued; here every copy is the original and the one port the port):
//  - novalis_amoeboid.cpp: classes 572 / 865 / 866, the Amoeboids (levels 1, 5, 11): the update through every state, its
//    frame before the state (the hit, the split, the water, the target), the split, the nearest class-0x26f moby of a group,
//    the splash and the drips; on the level's own Amoeboids past their first update, the hero, a hit, the water, the group
//    lists (a list of the test area naming the level's mobys made halves waiting to take a split's place or class-0x26f
//    mobys), the alert and the target's state varied;
//  - novalis_water.cpp: the water (levels 1, 5, 7, 11, 12, 13): the VU1 set-up, the grid's light and positions, the uv
//    scroll, a cell's packet, a cell, the tiles' draw and the class's draw callback, on each level's own tile records (the
//    callback's) with their moving fields varied and the camera over them or away; the packet cursor on a buffer of the test
//    area (`B511_INFO=1` prints the subjects and how far each case's packets grew).
// No RAM dumps: states built from the level after its first gameplay frame, the screen and projection set up (640 x 448).
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 511, N = 1, 5, 7, 11, 12, 13; a family the level does not carry says so.

#include "ovl_oracle.h"

#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/novalis_water.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/vu/vu0.h>

#include <cmath>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {
// The VU0 the original started from (the interpreter's), given to the port before it runs (as batches 501 / 503 / 513)
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

bool info() { return std::getenv("B511_INFO") != nullptr; }

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

// the frame's packet cursor (0x1611c0)
uint32_t cursor() { return ee_read<uint32_t>(OV(0x001611c0)); }

// A check whose original is the level's own copy at `laddr`; the port gets the interpreter's VU0 and, with B511_INFO, says how
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

// the level after its first gameplay frame (the port's) and its view set up, as batch 513
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

constexpr uint32_t P = 0x0013f350;  // the hero (+0x80 his position)
constexpr uint32_t SPR = 0x70000000u;

uint32_t first_moby() { return ee_read<uint32_t>(OV(0x0015ffd8)); }
uint32_t moby_end() { return ee_read<uint32_t>(OV(0x0015ffe0)); }

// the level's live mobys whose update function is `fn` (the level's address)
std::vector<uint32_t> mobys_with(uint32_t fn) {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < moby_end(); m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn && ee_read<uint32_t>(m + 0x78) != 0) v.push_back(m);
    return v;
}
// the level's live mobys with pvars (other than `not_m`)
std::vector<uint32_t> live_mobys(uint32_t not_m = 0) {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < moby_end(); m += 0x100)
        if (m != not_m && ee_read<uint8_t>(m + 0x20) < 0x7f && ee_read<uint32_t>(m + 0x24) != 0 && ee_read<uint32_t>(m + 0x78) != 0) v.push_back(m);
    return v;
}

uint32_t pick(const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; }
uint32_t index_of(uint32_t m) { return (m - first_moby()) >> 8; }

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

// A function taking one of the level's mobys `subjects()`, `vary` building the state (dealt again when the port's run throws);
// `args` the call's arguments for the moby (the moby alone when none), `ret` the kind of result compared
void classcheck(const char* name, uint32_t laddr, const std::function<std::vector<uint32_t>()>& subjects,
                const std::function<uint64_t(const Args&)>& port, const std::function<void(uint32_t)>& vary, int n,
                Ret ret = Ret::None, const std::function<Args(uint32_t)>& args = nullptr) {
    if (!selected(name)) return;
    level();
    const std::vector<uint32_t> probe = subjects();
    if (probe.empty()) { std::printf("%-26s L%d:0x%08x  no subject on this level\n", name, g_oracle_level, laddr); return; }
    if (info()) std::printf("  (%s: %zu subjects, class %d)\n", name, probe.size(), ee_read<int16_t>(probe[0] + 0xa6));
    fcheck(name, laddr, n, ret, [subjects, vary, port, args] {
        Args a;
        for (int attempt = 0; attempt < 40; ++attempt) {
            level();
            const uint32_t m = pick(subjects());
            vary(m);
            a = args ? args(m) : Args{{m}, {}};
            if (trial_ok([port, a] { port(a); })) break;
        }
        return a;
    }, port);
}
// a port taking the moby alone
std::function<uint64_t(const Args&)> on_moby(void (*fn)(uint32_t)) {
    return [fn](const Args& a) { fn(a.i[0]); return uint64_t{0}; };
}

// ---- the Amoeboids (classes 572 / 865 / 866; novalis_amoeboid.cpp)
struct Amo {
    uint32_t update, frame, split, target, splash, drips;
};
Amo amo_here() {
    switch (g_oracle_level) {
    case 1: return {0x002edca0, 0x002eec68, 0x002ef350, 0x002ef478, 0x002ef770, 0x002ef560};
    case 5: return {0x002fecf0, 0x002ffcb8, 0x003003a0, 0x003004c8, 0x003007c0, 0x003005b0};
    case 11: return {0x002f9998, 0x002fa960, 0x002fb048, 0x002fb170, 0x002fb468, 0x002fb258};
    default: return {0, 0, 0, 0, 0, 0};
    }
}

// the level's water tiles (the class's draw callback's records) and their count
struct Water {
    uint32_t tiles;
    int32_t n;
};
Water water_here() {
    switch (g_oracle_level) {
    case 1: return {ORACLE_LADDR(1, 0x001e34c0u), 0x15};
    case 5: return {ORACLE_LADDR(5, 0x001d6880u), 0x30};
    case 7: return {ORACLE_LADDR(7, 0x001dce40u), 0x24};
    case 11: return {ORACLE_LADDR(11, 0x001da880u), 0x14};
    case 12: return {ORACLE_LADDR(12, 0x001cbf40u), 0x11};
    case 13: return {ORACLE_LADDR(13, 0x001d9bc0u), 0x16};
    default: return {0, 0};
    }
}

std::vector<uint32_t> amoeboids() { return mobys_with(amo_here().update); }

// the class's tuning block (the level's own data; novalis_amoeboid.cpp)
uint32_t amo_tuning() { return g_oracle_level == 5 ? ORACLE_LADDR(5, 0x00161948u) : ORACLE_LADDR(1, 0x00161998u); }

// a hit on the moby: its slot in the hit table (0x178100, 64-byte entries) naming it, with a damage mask (the `kinds` bits
// mixed with random ones), a damage at +0x2c and a source +0x20: none, the hero, another moby (a class 0x47 / 0xba one, one
// whose class's +0x46 is 5) (batch 103's)
void give_hit(uint32_t m, uint32_t kinds) {
    if (ri(0, 3) == 0) { w8(m + 0xa4, 0xff); return; }
    const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
    w8(m + 0xa4, idx);
    w32(e + 0x34, ri(0, 8) == 0 ? first_moby() : m);
    w32(e + 0x24, ri(0, 2) == 0 ? (R() & ~kinds) : (kinds | (R() & 0xff)));
    { static const float dm[] = {-1.0f, 0.0f, 0.5f, 5.0f, 12.0f}; wf(e + 0x2c, dm[ri(0, 4)]); }
    wfrand(e, 4, -1, 1);
    for (uint32_t k = 0; k < 3; ++k) wf(e + 4 * k, ee_read<float>(m + 0x10 + 4 * k) + rf(-20, 20));  // (the hit's place)
    wfrand(e + 0x10, 3, -2, 2);  // (its direction)
    w32(e + 0x28, u(ri(0, 20)));
    w8(e + 0x28, u(ri(0, 11))); w8(e + 0x29, u(ri(0, 4)));
    w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
    wf(e + 0x1c, coin() ? 5627.95f : 0.0f);
    const int src = ri(0, 4);
    uint32_t s = 0;
    if (src == 1) s = ee_read<uint32_t>(P + 0x2080);
    else if (src >= 2) {
        const std::vector<uint32_t> others = live_mobys(m);
        if (!others.empty()) {
            s = pick(others);
            if (src == 3) w16(s + 0xa6, coin() ? 0x47u : 0xbau);
            if (src == 4 && ee_read<uint32_t>(s + 0x24) != 0) w16(ee_read<uint32_t>(s + 0x24) + 0x46, coin() ? 5u : u(ri(0, 8)));
            if (src == 2 && coin()) w32(s + 0x24, 0);  // (a source without a class)
        }
    }
    w32(e + 0x20, s);
}

// a group list of the test area (halves: moby indices, the last with bit 15) put in group slot `g` of the level's table
// (Veldin's 0x1ab840)
constexpr uint32_t GLIST = TEST_AREA + 0x7000;
void group_list(uint32_t g, const std::vector<uint32_t>& ms) {
    if (ms.empty()) { w32(OV(0x001ab840) + (g << 2), 0); return; }
    for (size_t i = 0; i < ms.size(); ++i) w16(GLIST + u(static_cast<int32_t>(i)) * 2, index_of(ms[i]) | (i + 1 == ms.size() ? 0x8000u : 0u));
    w32(OV(0x001ab840) + (g << 2), GLIST);
}

// some of the level's other Amoeboids made halves waiting for a split (state 0xc, the next sizes), and other live mobys made
// class-0x26f mobys, in a group list of `m`'s own group +0x21 or its group pvar +0x248
void groups_about(uint32_t m) {
    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
    std::vector<uint32_t> list;
    for (uint32_t o : amoeboids()) {
        if (o == m || ri(0, 3) != 0 || list.size() >= 4) continue;
        w8(o + 0x20, ri(0, 3) == 0 ? u(ri(0, 0x10)) : 0xcu);
        static const int sizes[] = {0x23c, 0x361, 0x362};
        w16(o + 0xa6, u(sizes[ri(0, 2)]));
        list.push_back(o);
    }
    for (uint32_t o : live_mobys(m)) {
        if (ri(0, 6) != 0 || list.size() > 12) continue;
        if (ee_read<uint32_t>(o + 0x74) == amo_here().update) continue;
        w16(o + 0xa6, coin() ? 0x26fu : u(ri(0, 0x300)));
        w16(o + 0x34, coin() ? (ee_read<uint16_t>(o + 0x34) | 0x1000u) : (ee_read<uint16_t>(o + 0x34) & 0xefffu));
        w8(o + 0x20, ri(0, 4) == 0 ? u(ri(0x7f, 0xfd)) : u(ri(0, 0x7e)));
        Vec4& q = ee_ref<Vec4>(o + 0x10);  // (near the subject: within the 120 / the range)
        if (coin()) { const Vec4 p = ee_ref<Vec4>(m + 0x10); q.x = p.x + rf(-40, 40); q.y = p.y + rf(-40, 40); q.z = p.z + rf(-5, 5); }
        list.push_back(o);
    }
    for (int k = ri(0, 3); k > 0 && !list.empty(); --k) std::swap(list[static_cast<size_t>(ri(0, static_cast<int>(list.size()) - 1))], list[0]);
    const uint32_t own = ri(0, 4) == 0 ? u(ri(0, 63)) : u(ri(0, 20));
    w8(m + 0x21, own);
    group_list(own, list);
    w32(pv + 0x248, ri(0, 3) == 0 ? 0xffffffffu : (coin() ? own : u(ri(0, 20))));
    if (ee_read<int32_t>(pv + 0x248) >= 0 && ee_read<uint32_t>(pv + 0x248) != own && coin()) group_list(ee_read<uint32_t>(pv + 0x248), list);
}

// an Amoeboid past its first update (state 0, the port's), then in state `st` (-1: a random one) with its pvars, the hero,
// a hit, its group and the water varied
void amo_prep(uint32_t m, int st) {
    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
    if (st != 0 && coin()) {
        w8(m + 0x20, 0);
        amoeboid_update_l1(m);
    }
    w8(m + 0x20, st < 0 ? u(ri(0, 0x10)) : u(st));
    w8(m + 0x53, u(ri(0, 8))); w8(m + 0x52, coin() ? ee_read<uint32_t>(m + 0x53) : u(ri(0, 8))); w8(m + 0xbc, u(ri(0, 2)));
    if (ri(0, 2) == 0) w8(m + 0x70, R());  // (the animation's end flag, bit 1)
    if (ri(0, 2) == 0) wf(m + 0x54, coin() ? 0.0f : rf(0, 1));  // (the animation's blend)
    w8(m + 0x31, u(ri(0, 3) == 0 ? 0 : 1));
    wf(m + 0x48, rf(-3.1f, 3.1f));
    if (st == 0 && ri(0, 2) == 0) {  // its class: one of the three sizes, or another
        static const uint16_t cls[] = {0x23c, 0x361, 0x362, 0x400};
        w16(m + 0xa6, cls[ri(0, 3)]);
    }
    if (ri(0, 3) == 0 || (st == 6 && coin())) {  // the animation's frame (the sets' frame number +4: the attack's steps in frames 10 .. 16)
        for (uint32_t k : {0x68u, 0x6cu}) {
            const uint32_t set = ee_read<uint32_t>(m + k);
            if (set >= 0x00100000u && set < 0x02000000u) w16(set + 4, u(ri(8 * 16, 18 * 16)));
        }
    }
    if (ri(0, 3) == 0) {  // the camera near (the floor band within 28)
        Vec4& c = ee_ref<Vec4>(OV(0x00166dc0));
        const Vec4 q = ee_ref<Vec4>(m + 0x10);
        c.x = q.x + rf(-20, 20); c.y = q.y + rf(-20, 20); c.z = q.z + rf(-5, 10);
    }
    if (ri(0, 3) == 0) {  // the moby moved off its home
        Vec4& q = ee_ref<Vec4>(m + 0x10);
        const float r = coin() ? rf(0, 1.2f) : rf(1, 12);
        const float a = rf(-3.1f, 3.1f);
        q.x += r * std::cos(a); q.y += r * std::sin(a);
    }
    wf(m + 0x18, ee_read<float>(m + 0x18) + (coin() ? 0.0f : rf(-3, 3)));
    for (uint32_t o = 0x20; o < 0x200; o += 4) {  // a few of the pvars kept as they are typed: floats scaled, small counters moved
        if (ri(0, 9) != 0) continue;
        const uint32_t v = ee_read<uint32_t>(pv + o), ex = v & 0x7f800000u;
        if (ex >= 0x30000000u && ex < 0x50000000u) wf(pv + o, ee_read<float>(pv + o) * rf(0.5f, 1.5f));
        else if (v < 16) w32(pv + o, u(ri(0, static_cast<int>(v) + 2)));
    }
    // the class's own pvars: the target and its loss, the alert, the range, the water, the box, the leader, the size
    const std::vector<uint32_t> others = live_mobys(m);
    const int tk = ri(0, 3);
    w32(pv + 0x200, tk == 0 ? 0u : (tk != 3 || others.empty() ? ee_read<uint32_t>(0x001413d0) : pick(others)));
    {  // the hero near or far (the target: the hero moby's place, the hero's own and his box-test point +0xd0) from the moby or
       // from its home
        Vec4 q = ri(0, 3) != 0 ? ee_ref<Vec4>(m + 0x10) : ee_ref<Vec4>(pv + 0x210);
        const int k = ri(0, 2);
        const float r = k == 0 ? rf(0, 2.5f) : k == 1 ? rf(2.5f, 25) : rf(25, 80);
        const float a = rf(-3.1f, 3.1f);
        q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += ri(0, 3) == 0 ? rf(-6, 6) : rf(-1, 1);
        ee_ref<Vec4>(P + 0x80) = q;
        ee_ref<Vec4>(P + 0xd0) = q;
        const uint32_t hm = ee_read<uint32_t>(0x001413d0);
        if (hm != 0 && ri(0, 4) != 0) ee_ref<Vec4>(hm + 0x10) = q;
        const uint32_t t = ee_read<uint32_t>(pv + 0x200);
        if (t != 0 && t != hm && coin()) ee_ref<Vec4>(t + 0x10) = q;  // (another moby targeted: there)
    }
    w32(pv + 0x204, u(ri(0, 2)));
    if (coin()) w32(pv + 0x22c, ri(0, 2) == 0 ? 0u : u(ri(1, 300)));
    if (ri(0, 3) == 0) w32(pv + 0x38, u(ri(0, 1)));
    if (ri(0, 3) == 0) wf(pv + 0x224, ri(0, 3) == 0 ? 0.0f : rf(5, 40));
    if (ri(0, 2) == 0) wf(pv + 0x20, ri(0, 3) == 0 ? rf(-0.5f, 1.0f) : rf(1.0f, 4.0f));
    if (ri(0, 3) == 0) w32(pv + 0x230, u(ri(0, 1)));
    if (ri(0, 3) == 0 || st == 0xd) w32(pv + 0x25c, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 7)));
    if (ee_read<int32_t>(pv + 0x25c) >= 0 && ee_read<int32_t>(pv + 0x25c) < 8 && coin()) {  // the hero in its box (0x1600ec: 0x80 apart, the centre +0x30)
        const uint32_t box = ee_read<uint32_t>(OV(0x001600ec)) + (ee_read<uint32_t>(pv + 0x25c) << 7);
        for (uint32_t k = 0; k < 3; ++k) wf(P + 0xd0 + 4 * k, ee_read<float>(box + 0x30 + 4 * k) + rf(-0.4f, 0.4f));
    }
    if (ri(0, 3) == 0) {
        const Water w = water_here();
        w32(pv + 0x258, w.n == 0 || coin() ? 0u : u(ri(1, w.n - 1)));
        const uint32_t tiles = ee_read<uint32_t>(OV(0x001612d0));
        const float h = ee_read<float>(tiles + ee_read<uint32_t>(pv + 0x258) * 0x1190 + 8);
        if (ee_read<int32_t>(pv + 0x258) != 0 && std::isfinite(h) && std::fabs(h) < 1e4f)  // (the moby about the tile's surface; the
            wf(m + 0x18, h + rf(-2, 2));                                                     // pointer is 0 on some levels)
    }
    if (ri(0, 3) == 0) {  // the leader: none, a moby (its +0xbc) or the moby itself
        const int32_t k = ri(0, 2);
        if (k == 0) w32(pv + 0x260, u(ri(-1, 0)));
        else {
            const uint32_t o = pick(live_mobys());
            w32(pv + 0x260, index_of(o));
            w8(o + 0xbc, u(ri(0, 1)));
        }
    }
    if (ri(0, 4) == 0) wf(pv + 0x250, rf(0.3f, 1.2f));
    if (ri(0, 3) == 0) groups_about(m);
    const uint32_t hm = ee_read<uint32_t>(0x001413d0);
    if (hm != 0 && st >= 1 && st <= 6 && coin()) {  // (the chase's states: the hero the target, near: a clear way, the reach)
        w32(pv + 0x200, hm);
        w32(pv + 0x204, u(ri(0, 1)));
        Vec4 q = ee_ref<Vec4>(m + 0x10);
        const float r = coin() ? rf(0.2f, 2.0f) : rf(2.0f, 12.0f), ang = rf(-3.1f, 3.1f);
        q.x += r * std::cos(ang); q.y += r * std::sin(ang); q.z += rf(-0.5f, 0.5f);
        ee_ref<Vec4>(hm + 0x10) = q;
        ee_ref<Vec4>(P + 0x80) = q;
        if (coin()) ee_ref<Vec4>(pv + 0x210) = ee_ref<Vec4>(m + 0x10);  // (home where it is)
    }
    if (st == 6 && coin()) {  // the strike: the frame just past 15.5 (no blend), the speed positive
        for (uint32_t k : {0x68u, 0x6cu}) {
            const uint32_t set = ee_read<uint32_t>(m + k);
            if (set >= 0x00100000u && set < 0x02000000u) w16(set + 4, u(ri(248, 250)));
        }
        wf(m + 0x54, 0.0f);
        wf(m + 0x58, rf(0.5f, 1.5f));
        wf(m + 0x5c, rf(0.2f, 1.0f));
    }
    if ((st == 0xf || st == 0x10) && coin()) {  // next to an edge of its wall path (+0x240), the target across it
        const uint32_t path = ee_read<uint32_t>(OV(0x001b04b0) + (ee_read<uint32_t>(pv + 0x240) << 2));
        const int32_t n = path != 0 ? ee_read<int32_t>(path) : 0;
        if (n >= 2 && n < 64) {
            const int32_t i = ri(1, n - 1);
            const Vec4 p0 = ee_ref<Vec4>(path + 0x10 + u(i - 1) * 16), p1 = ee_ref<Vec4>(path + 0x10 + u(i) * 16);
            const float dx = p1.x - p0.x, dy = p1.y - p0.y, len = std::hypot(dx, dy);
            if (len > 0.01f) {
                const float nx = -dy / len, ny = dx / len, f = rf(0.2f, 0.8f), near = rf(0.1f, 1.6f), far = rf(1.0f, 8.0f);
                Vec4& q = ee_ref<Vec4>(m + 0x10);
                q.x = p0.x + dx * f + nx * near; q.y = p0.y + dy * f + ny * near;
                const uint32_t t = ee_read<uint32_t>(pv + 0x200) != 0 ? ee_read<uint32_t>(pv + 0x200) : hm;
                if (t != 0) {
                    Vec4& tq = ee_ref<Vec4>(t + 0x10);
                    tq.x = p0.x + dx * f - nx * far; tq.y = p0.y + dy * f - ny * far; tq.z = q.z;
                    w32(pv + 0x200, t);
                }
                if (coin()) w32(pv + 0x22c, u(ri(1, 200)));
            }
        }
    }
    if (ri(0, 4) == 0) wf(amo_tuning() + 0xa0, coin() ? 0.0f : rf(0.1f, 2.0f));  // (the splash's share of the speed)
    if (st == 8 && ri(0, 2) == 0) {  // a home it may not get back to
        Vec4& h = ee_ref<Vec4>(pv + 0x210);
        h.x += rf(-120, 120); h.y += rf(-120, 120);
    }
    if (st < 0 || coin()) give_hit(m, ri(0, 1) ? 0x210000u : 0x330000u);  // (in a state's check, half the cases without a hit)
    else w8(m + 0xa4, 0xff);
    w32(OV(0x0015f5cc), R() & 0xffff);
    seed_rand();
}

void amoeboid_checks() {
    const Amo a = amo_here();
    if (a.update == 0) {
        for (const char* n : {"amoeboid_update", "amoeboid_frame", "amoeboid_split", "amoeboid_target", "amoeboid_splash", "amoeboid_drips"})
            not_here(n);
        return;
    }
    // (the class's carry / suck hooks belong to the weapons: no-ops on both sides, as batch 103; the explosion, the vsync wait)
    g_extra_noop_hooks = {OVF(0x0025e450), 0x00122298, OVF(0x002db868)};
    classcheck("amoeboid_split", a.split, amoeboids, [](const Args& x) { return r32(nov_2ef350_l1(x.i[0])); }, [](uint32_t m) {
        amo_prep(m, -1);
        groups_about(m);
        if (coin()) w16(m + 0xa6, coin() ? 0x23cu : 0x361u);
    }, 80, Ret::Int32);
    classcheck("amoeboid_target", a.target, amoeboids, [](const Args& x) { return r32(nov_2ef478_l1(x.i[0], x.i[1])); }, [](uint32_t m) {
        amo_prep(m, -1);
        groups_about(m);
    }, 80, Ret::Int32, [](uint32_t m) {  // (the group: the moby's group pvar +0x248 or its own group +0x21, as the frame passes it)
        const int32_t g = ee_read<int32_t>(ee_read<uint32_t>(m + 0x78) + 0x248);
        return Args{{m, g >= 0 && coin() ? u(g) : ee_read<uint8_t>(m + 0x21)}, {}};
    });
    classcheck("amoeboid_splash", a.splash, amoeboids, [](const Args& x) { nov_2ef770_l1(x.i[0], x.i[1]); return uint64_t{0}; }, [](uint32_t m) {
        amo_prep(m, -1);
        if (ri(0, 3) == 0) wf(0x0015ed6c, rf(0.5f, 2.0f));  // (the frame's time scale)
    }, 60, Ret::None, [](uint32_t m) {  // (the speed: a hit's direction, zero, or a test vector)
        const int k = ri(0, 2);
        wfrand(TEST_AREA + 0x6000, 4, -3, 3);
        return Args{{m, k == 0 ? TEST_AREA + 0x6000 : k == 1 ? OV(0x0015f580) : OV(0x00178100) + 0x10}, {}};
    });
    classcheck("amoeboid_drips", a.drips, amoeboids, on_moby(nov_2ef560_l1), [](uint32_t m) {
        amo_prep(m, -1);
        if (coin()) w32(amo_tuning() + 0x48, 2);  // (the first kind's odds: one in 1, every frame)
        if (coin()) w32(amo_tuning() + 0x4c, 2);
    }, 80);
    for (int pass = 0; pass < 4; ++pass)
        classcheck("amoeboid_frame", a.frame, amoeboids, on_moby(nov_2eec68_l1), [pass](uint32_t m) {
            amo_prep(m, pass == 0 ? 1 : -1);
            if (pass >= 2) groups_about(m);
            if (pass == 3) {  // a hit that counts (the frame's mask, a damage) on a moby with health to split
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                do give_hit(m, 0x330000u); while (ee_read<uint8_t>(m + 0xa4) == 0xff);
                const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                w32(e + 0x24, 0x330000u | (R() & 0xff));
                wf(e + 0x2c, coin() ? 5.0f : 0.5f);
                wf(pv + 0x20, rf(1.5f, 4.0f));
                if (ri(0, 2) != 0) w8(m + 0x20, u(ri(1, 7)));
            }
        }, 50);
    for (int st = 0; st <= 0x10; ++st)
        classcheck("amoeboid_update", a.update, amoeboids, on_moby(amoeboid_update_l1), [st](uint32_t m) { amo_prep(m, st); }, 60);
    g_extra_noop_hooks.clear();
}

// ---- the water (novalis_water.cpp)
// the frame's packet cursor (0x1611c0) on a buffer of the test area (after the first gameplay frame it is 0: the packets would go
// to the null page, which the comparison leaves out)
void packet_cursor() { w32(OV(0x001611c0), TEST_AREA + 0x10000 + u(ri(0, 0x40)) * 0x10); }

// the camera (Veldin's 0x166dc0) moved near a point, its view rows (0x166fd0) turned about z; the render frame's view made
// again (batches 128 / 513)
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

// a water tile's moving fields varied (batch 128's)
void vary_tile(uint32_t t) {
    if (coin()) wfrand(t + 0x20, 2, -1.2f, 1.2f);   // the uv scroll
    if (coin()) wfrand(t + 0x28, 2, -0.2f, 0.2f);   // its rates
    if (coin()) wfrand(t + 0x30, 2, -7.0f, 7.0f);   // the swell's angles
    if (coin()) wfrand(t + 0x38, 2, -0.3f, 0.3f);   // their rates
    if (coin()) wf(t + 0x40, rf(0.0f, 0.2f));        // the swell's size
    if (ri(0, 3) == 0) w16(t + 0x1e, R());           // the cells drawn
    if (ri(0, 3) == 0) w32(t + 0x44, R() & 0xfu);    // the flat edges
    if (ri(0, 4) == 0) w8(t + 0x1c, R());            // the alphas
    if (ri(0, 4) == 0) w8(t + 0x1d, R());
    if (ri(0, 3) == 0) wfrand(t + 0x50 + u(ri(0, 2)) * 0x5c0, 0x5c0 / 4, -0.6f, 0.6f);  // a frame's heights (and its borders)
}

// one of the level's tiles (the others varied too), the camera over it, near or away (cells clipped or culled), the
// texture / alpha cache words and the block drawn varied, the packet cursor on the test buffer
uint32_t tile_at() {
    level();
    packet_cursor();
    const Water w = water_here();
    const uint32_t t = w.tiles + u(ri(0, w.n - 1)) * 0x1190;
    for (int32_t i = 0; i < w.n; ++i) vary_tile(w.tiles + u(i) * 0x1190);
    const float spread = ri(0, 3) == 0 ? 400.0f : (coin() ? 60.0f : 15.0f);
    move_camera(ee_read<float>(t), ee_read<float>(t + 4), ee_read<float>(t + 8) + 2.0f, spread);
    if (ri(0, 3) == 0) w32(OV(0x00161220), u(ri(0, 2)));  // the block drawn
    if (ri(0, 3) == 0) {  // the GS state cache: unset, or the tile's own textures / alphas
        if (coin()) for (uint32_t k = 0; k < 4; ++k) w32(OV(0x00161228) + 4 * k, 0xffffffffu);
        else {
            w32(OV(0x00161228), ee_read<uint32_t>(t + 0x14)); w32(OV(0x0016122c), ee_read<uint32_t>(t + 0x18));
            w32(OV(0x00161230), ee_read<uint8_t>(t + 0x1c)); w32(OV(0x00161234), ee_read<uint8_t>(t + 0x1d));
        }
    }
    return t;
}

void water_checks() {
    const Water w = water_here();
    const uint32_t vu7 = copy_here({{1, 0x002b8f98}, {5, 0x002cd960}, {7, 0x002cbed0}, {11, 0x002c8108}, {12, 0x002bba98}, {13, 0x002af8b8}});
    if (w.n == 0 || vu7 == 0) {
        for (const char* n : {"water_vu7_setup", "water_grid_light", "water_grid_xyz", "water_uv_scroll", "water_cell_packet", "water_cell",
                              "water_draw", "water_cb_draw"})
            not_here(n);
        return;
    }
    const uint32_t light = copy_here({{1, 0x00261df8}, {5, 0x00276da8}, {7, 0x00275568}, {11, 0x00273190}, {12, 0x00265fd0}, {13, 0x00259060}});
    const uint32_t xyz = copy_here({{1, 0x002b8c08}, {5, 0x002cd5d0}, {7, 0x002cbb40}, {11, 0x002c7d78}, {12, 0x002bb708}, {13, 0x002af528}});
    const uint32_t uv = copy_here({{1, 0x002b80d0}, {5, 0x002ccab0}, {7, 0x002cb008}, {11, 0x002c7258}, {12, 0x002babe8}, {13, 0x002ae9f0}});
    const uint32_t pkt = copy_here({{1, 0x00262388}, {5, 0x00277338}, {7, 0x00275af8}, {11, 0x00273720}, {12, 0x00266560}, {13, 0x002595f0}});
    const uint32_t cell = copy_here({{1, 0x002b8cb0}, {5, 0x002cd678}, {7, 0x002cbbe8}, {11, 0x002c7e20}, {12, 0x002bb7b0}, {13, 0x002af5d0}});
    const uint32_t draw = copy_here({{1, 0x002b91c8}, {5, 0x002cdb90}, {7, 0x002cc100}, {11, 0x002c8338}, {12, 0x002bbcc8}, {13, 0x002afae8}});
    const uint32_t cb = copy_here({{1, 0x002fd0c0}, {5, 0x0030d678}, {7, 0x0030cd20}, {11, 0x0030e708}, {12, 0x002bf748}, {13, 0x0030a460}});
    if (info()) {
        level();
        std::printf("  (water: %d tiles at %08x, the engine's pointer 0x1612d0 %08x)\n", w.n, w.tiles, ee_read<uint32_t>(OV(0x001612d0)));
    }

    fcheck("water_vu7_setup", vu7, 80, Ret::None, [] {
        tile_at();
        if (coin()) w32(OV(0x0015f5e0), u(ri(0, 8)));
        return Args{};
    }, [](A) { nov_water_vu7_setup_l1(); return uint64_t{0}; });
    fcheck("water_grid_light", light, 120, Ret::None, [] {
        const uint32_t t = tile_at();
        if (coin()) nov_water_grid_in_l1(SPR, t + 0x50 + u(ri(0, 2)) * 0x5c0);  // (the grid as the original's caller leaves it)
        else for (uint32_t a = SPR; a < SPR + 0x780; a += 4) wf(a, rf(-2.0f, 2.0f));
        return Args{{t}, {}};
    }, [](A a) { nov_water_grid_light_l1(a.i[0]); return uint64_t{0}; });
    fcheck("water_grid_xyz", xyz, 80, Ret::None, [] {
        const uint32_t t = tile_at();
        for (uint32_t a = SPR; a < SPR + 0x780; a += 4) wf(a, rf(-2.0f, 2.0f));
        return Args{{t}, {}};
    }, [](A a) { nov_water_grid_xyz_l1(a.i[0]); return uint64_t{0}; });
    fcheck("water_uv_scroll", uv, 120, Ret::None, [] { return Args{{tile_at()}, {}}; },
           [](A a) { nov_water_uv_scroll_l1(a.i[0]); return uint64_t{0}; });
    fcheck("water_cell_packet", pkt, 120, Ret::None, [] {
        tile_at();
        wrand(SPR, 0x3000 / 4);
        const int32_t c = ri(0, 15);
        return Args{{coin() ? 0xcu : 0xeu, u((c >> 2) * 0x44 + ((c & 3) << 2))}, {}};
    }, [](A a) { nov_water_cell_packet_l1(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
    fcheck("water_cell", cell, 240, Ret::None, [] {
        const uint32_t t = tile_at();
        wrand(SPR, 0x3000 / 4);
        return Args{{t, u(ri(0, 15)), u(ri(0, 2))}, {}};
    }, [](A a) { nov_water_cell_l1(a.i[0], static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2])); return uint64_t{0}; });
    fcheck("water_draw", draw, 80, Ret::None, [w] {
        tile_at();
        const int32_t first = ri(0, 3) == 0 ? ri(0, w.n - 1) : 0;
        const int32_t n = ri(0, 3) == 0 ? ri(0, w.n - first) : w.n - first;
        return Args{{w.tiles + u(first) * 0x1190, u(n)}, {}};
    }, [](A a) { nov_water_draw_l1(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
    fcheck("water_cb_draw", cb, 40, Ret::None, [] {
        tile_at();
        return Args{{M}, {}};
    }, [](A a) { nov_water_751_draw_l1(a.i[0]); return uint64_t{0}; });
}
}  // namespace

void batch_511() {
    if (g_only_batch != 511) return;
    g_batch = 511;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    if (g_oracle_level < 1 || g_oracle_level > 18) {
        std::printf("batch 511: levels 1 .. 18 only\n");
    } else {
        level();
        // the port's VU0 micro memory as the game leaves it, and the interpreter's VU0 kept for the port (as batch 513)
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
        if (info()) {
            std::printf("  (mobys %08x .. %08x; runs of one repeated word, 64 KB or more, from 0x1000000:", first_moby(), moby_end());
            uint32_t run = 0, word = 0;
            for (uint32_t a = 0x01000000; a <= 0x02000000; a += 0x100) {
                bool same = a < 0x02000000;
                const uint32_t w0 = same ? ee_read<uint32_t>(a) : 0;
                for (uint32_t k = 0; same && k < 0x100; k += 4) same = ee_read<uint32_t>(a + k) == w0;
                if (same && run != 0 && w0 == word) continue;
                if (run != 0 && a - run >= 0x10000) std::printf(" %08x..%08x(%08x)", run, a, word);
                run = same ? a : 0;
                word = w0;
            }
            std::printf(")\n");
            std::printf("  (amoeboids: %zu", amoeboids().size());
            for (uint32_t m : amoeboids()) std::printf(" %08x/%d", m, ee_read<int16_t>(m + 0xa6));
            std::printf(")\n");
        }

        amoeboid_checks();
        water_checks();
    }

    g_pre_call = nullptr;
    g_extra_noop_hooks.clear();
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
