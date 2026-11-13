// Oracle batch 520 (stage 4 task F14, #149): class code and helpers of levels 1, 2, 3, 4, 6, 7, 8, 14, 15 and 17 that two or
// more copies carry, each family ported once as shared hand-written code (src/game/ovl/shared_*.cpp) on top of the families
// earlier tasks ported, against every copy at its own level's address. The level copies stay transliterated in their units
// until each level is re-glued; here every copy is the original and the one shared port the port:
//  - shared_class437.cpp: class 437, an older build of the Sandmouse (Novalis, Aridia, Eudora), its update and its six
//    helpers (its step, target search, shot, goal by the hero, fall home, and the moby's collision point);
//  - shared_gadget_pickups.cpp: the gadgets picked up before a panel (Aridia's class 1005, the Trespasser, Blarg's class
//    1016; Quartu's and Drek's fleet's class 1428);
//  - shared_class1629.cpp: class 1629, a shower of class 1400's drops (Batalia, Oltanis);
//  - shared_scroll.cpp: Batalia's two draws of three layers of scrolling meshes (classes 600 and 1649) and their third
//    layer's set (the twelfth mesh set);
//  - shared_sea_draw.cpp: the sea's draw callback (Kerwan, Umbris's two, Batalia, Oltanis).
// The shared ports and their copies (each level's address):
//   class437_update        L1:0x002e3f20 L2:0x002ccc08 L4:0x002c7e28
//   class437_move          L1:0x002e3830 L2:0x002cc518 L4:0x002c7738
//   class437_find_target   L1:0x002e32a8 L2:0x002cbf90 L4:0x002c71b0
//   moby_collision_point   L1:0x00263a28 L2:0x002512b8 L4:0x00241980
//   class437_shoot         L1:0x002e35a8 L2:0x002cc290 L4:0x002c74b0
//   class437_goal_by_hero  L1:0x002e3110 L2:0x002cbdf8 L4:0x002c7018
//   class437_sink          L1:0x002e3208 L2:0x002cbef0 L4:0x002c7110
//   gadget_pickup_update   L2:0x002ea210 L6:0x002f4638
//   class1428_update       L15:0x002ebd78 L17:0x002f1790
//   class1629_update       L8:0x003085f0 L14:0x003088b0
//   class600_draw          L8:0x002f0dc8
//   class1649_draw         L8:0x00309030
//   uv_scroll_meshes       (set 11) L8:0x00308f68
//   sea_draw               L3:0x002dc990 L7:0x002f8038 L8:0x002da0d0 L14:0x00303350
//   sea_draw_2             L7:0x0030d420
// Not checked: Veldin's finale's class 1750 update (L18:0x002fad08), the same shape as the sea's draw callback but another
// function (it writes the card's save image), not ported.
// Subjects: the level's own mobys of the class after the first gameplay frame (the screen, the projection and the view set
// up, as batch 519), perturbed through every state; mobys of the class spawned where the level places none. B520_INFO=1
// prints the subjects found and, per draw check, how many cases wrote packets beyond a state header.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 520, N = 1, 2, 3, 4, 6, 7, 8, 14, 15 and 17; a check prints "skipped"
// on a level without its copy. No RAM dumps; the frame scale (0x15ed68) at its 50 Hz value in a quarter of the cases.

#include "ovl_oracle.h"

#include <game/lib/libcdvd.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class1629.h>
#include <game/ovl/shared_class437.h>
#include <game/ovl/shared_gadget_pickups.h>
#include <game/ovl/shared_scroll.h>
#include <game/ovl/shared_sea_draw.h>
#include <runtime/kernel/disc.h>
#include <runtime/vu/vu0.h>

#include <cmath>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <vector>

namespace rac1::oracle {

namespace {
// The VU0 the original started from (the interpreter's), given to the port before it runs (as batches 506 / 510 / 519)
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

bool info() { return std::getenv("B520_INFO") != nullptr; }

// one level's copy of a shared function
struct Copy {
    int level;
    uint32_t addr;
};
// this level's `nth` copy among `copies`, 0 when the level has none
uint32_t copy_here(std::initializer_list<Copy> copies, int nth = 0) {
    for (const Copy& c : copies)
        if (c.level == g_oracle_level && nth-- == 0) return c.addr;
    return 0;
}

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
// original's VU0 first. With B520_INFO, the cases that wrote packets beyond a state header are counted.
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
    if (info() && std::strstr(name, "draw") != nullptr)
        std::printf("INFO %s L%d: %d cases, packets beyond a state header in %d\n", name, g_oracle_level, cases, drew);
}

// the level after its first gameplay frame (the port's) with the screen, the projection and the view set up (as batch 519)
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

// the frame's packet cursor (0x1611c0) on a buffer of the test area (batch 506's)
void packet_cursor() { w32(OV(0x001611c0), TEST_AREA + 0x8000 + u(ri(0, 0x40)) * 0x10); }
// the frame scale (0x15ed68) at the 50 Hz value sometimes (frame_scale_i's counts then differ from the 60 Hz ones)
void vary_frame_scale() {
    if (ri(0, 3) == 0) wf(0x0015ed68, 1.2f);
}

uint32_t pvar(uint32_t m) { return ee_read<uint32_t>(m + 0x78); }
uint32_t first_moby() { return ee_read<uint32_t>(OV(0x0015ffd8)); }
uint32_t end_moby() { return ee_read<uint32_t>(OV(0x0015ffe0)); }
bool live(uint32_t m) { return ee_read<uint8_t>(m + 0x20) < 0xfe; }
uint32_t moby_index(uint32_t m) { return (m - first_moby()) >> 8; }

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
// the mobys of the update list (0x15ffe4, linked by +0x28)
std::vector<uint32_t> listed_mobys() {
    std::vector<uint32_t> v;
    for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffe4)); m >= 0x00100000u && m < 0x02000000u && v.size() < 600; m = ee_read<uint32_t>(m + 0x28))
        v.push_back(m);
    return v;
}
uint32_t pick(const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; }

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

// a check of a helper on the generator's arguments, dealt again while the port's run throws
void helpercheck(const char* name, uint32_t laddr, int n, Ret ret, const std::function<Args()>& deal,
                 const std::function<uint64_t(const Args&)>& port) {
    if (!selected(name)) return;
    if (laddr == 0) {
        fcheck(name, 0, n, ret, nullptr, nullptr);
        return;
    }
    fcheck(name, laddr, n, ret, [deal, port] {
        Args a;
        for (int attempt = 0; attempt < 40; ++attempt) {
            level();
            a = deal();
            if (trial([&port, &a] { port(a); })) break;
        }
        return a;
    }, port);
}

// a check run with the words `words` left out besides the batch's (one check's known difference of existing code)
void without(const std::vector<uint32_t>& words, const std::function<void()>& run) {
    const std::vector<uint32_t> saved = g_ignored_words;
    for (uint32_t w : words) g_ignored_words.push_back(w);
    run();
    g_ignored_words = saved;
}
// the segment records 0x173e90 .. 0x173ebc that segment_collide / the walker's collision leave from VU0 registers in the
// original, which their hand ports do not reproduce (batches 240 / 242 / 372 / 514): the words left out of the checks of the
// functions that test lines against the world, only
std::vector<uint32_t> segment_words() {
    std::vector<uint32_t> seg;
    for (uint32_t k = 0; k < 0x30; k += 4) seg.push_back(OV(0x00173e90) + k);
    return seg;
}

// the level change a check's subject requests reads the console clock (and its dialogue stream polls the disc): the host's
// on both sides, a fixed clock (batch 514's)
void with_level_change(const std::function<void()>& run) {
    const std::time_t saved_clock = g_host_clock_fixed;
    g_host_clock_fixed = 1759658400;
    const auto saved_hooks = g_extra_hooks;
    g_extra_hooks = {{0x00121780, [](EeCpu& c) { c.set32(2, cd_read_clock(c.r32(4))); }},
                     {0x0012d6d8, [](EeCpu& c) { clock_to_local(c.r32(4)); }},
                     {0x00121450, [](EeCpu& c) { c.set32(2, cd_read(c.r32(4), c.r32(5), c.r32(6), c.r32(7))); }}};
    run();
    g_extra_hooks = saved_hooks;
    g_host_clock_fixed = saved_clock;
}

// ---- class 437, an older build of the Sandmouse (shared_class437.cpp)
void class437_checks() {
    const uint32_t upd = copy_here({{1, 0x002e3f20}, {2, 0x002ccc08}, {4, 0x002c7e28}});
    const uint32_t move = copy_here({{1, 0x002e3830}, {2, 0x002cc518}, {4, 0x002c7738}});
    const uint32_t find = copy_here({{1, 0x002e32a8}, {2, 0x002cbf90}, {4, 0x002c71b0}});
    const uint32_t point = copy_here({{1, 0x00263a28}, {2, 0x002512b8}, {4, 0x00241980}});
    const uint32_t shoot = copy_here({{1, 0x002e35a8}, {2, 0x002cc290}, {4, 0x002c74b0}});
    const uint32_t goal = copy_here({{1, 0x002e3110}, {2, 0x002cbdf8}, {4, 0x002c7018}});
    const uint32_t sink = copy_here({{1, 0x002e3208}, {2, 0x002cbef0}, {4, 0x002c7110}});
    const uint32_t tun = copy_here({{1, 0x00161900}, {2, 0x00161898}, {4, 0x00161908}});
    const uint32_t rec = copy_here({{1, 0x001deb38}, {2, 0x001d3110}, {4, 0x001d2e10}});
    // the class's mobys: the level's own (placed), or three spawned with a house (a live moby near)
    auto mice = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (!v.empty()) return v;
        const std::vector<uint32_t> all = live_mobys();
        for (int k = 0; k < 3; ++k) {
            const uint32_t s = moby_spawn(437);
            if (s == 0) break;
            if (pvar(s) == 0) w32(s + 0x78, TEST_AREA + 0x6000 + u(k) * 0x100);  // (no spawner gives it pvars: zeros)
            for (uint32_t q = 0; q < 0x80; q += 4) w32(pvar(s) + q, 0);
            const uint32_t house = all[static_cast<size_t>(k * 7) % all.size()];
            ee_ref<Vec4>(s + 0x10) = ee_ref<Vec4>(house + 0x10);
            w32(pvar(s) + 0x50, moby_index(house));
            w32(pvar(s) + 0x40, 0xffffffffu);
            v.push_back(s);
        }
        return v;
    };
    if (upd != 0 && info()) {
        level();
        std::printf("INFO class 437: %zu awake, %zu placed, %zu listed\n", mobys_with(upd).size(), placed_of(437).size(), listed_mobys().size());
        for (uint32_t m : mice())
            std::printf("INFO   %08x state %u house %d zone %d bc %u\n", m, ee_read<uint8_t>(m + 0x20), ee_read<int32_t>(pvar(m) + 0x50),
                        ee_read<int32_t>(pvar(m) + 0x40), ee_read<uint8_t>(m + 0xbc));
    }
    // targets: mobys of the update list made shown, flagged 0x1020, of kind 5 with a word of health (some not: each test of
    // the search's), put before the subject (the subject moved: a moby moved in RAM is not moved in the collision grid)
    auto targets = [](uint32_t m, int n) {
        std::vector<uint32_t> listed = listed_mobys(), out;
        if (listed.empty()) return out;
        for (int k = 0; k < n; ++k) {
            const uint32_t t = pick(listed);
            if (t == m) continue;
            const uint32_t cls = ee_read<uint32_t>(t + 0x24);
            if (cls == 0 || ee_read<uint32_t>(t + 0x78) == 0) continue;
            w16(t + 0x34, ee_read<uint16_t>(t + 0x34) | (ri(0, 5) == 0 ? 0x20u : 0x1020u));
            if (ri(0, 6) != 0) w16(t + 0x32, 0xff);
            if (ri(0, 6) != 0) w16(cls + 0x46, 5);
            const uint32_t word = TEST_AREA + 0x6800 + u(k) * 0x20;
            w32(pvar(t), ri(0, 6) == 0 ? 0u : word);
            wf(word, ri(0, 4) == 0 ? (coin() ? 0.0f : -1.0f) : rf(0.5f, 5.0f));
            wf(word + 0x10, rf(0.0f, 1.5f));
            out.push_back(t);
        }
        if (!out.empty() && coin()) {  // (the subject before one of them, near or far)
            const uint32_t t = out[0];
            const float yaw = ee_read<float>(m + 0x48) + rf(-0.6f, 0.6f), r = coin() ? rf(1.0f, 8.0f) : rf(8.0f, 30.0f);
            const Vec4 p = ee_ref<Vec4>(t + 0x10);
            ee_ref<Vec4>(m + 0x10) = Vec4{p.x - r * std::cos(yaw), p.y - r * std::sin(yaw), p.z - rf(-2.0f, 3.0f), ee_read<float>(m + 0x1c)};
        }
        return out;
    };
    // a zone of the test area (a box of half size 2 about `at`, its index into the level's box list 0x1600ec), or -1
    auto test_zone = [](const Vec4& at) -> int32_t {
        const uint32_t boxes = ee_read<uint32_t>(OV(0x001600ec));
        if (boxes < 0x00100000u || boxes >= TEST_AREA) return -1;
        const uint32_t box = TEST_AREA + 0x7000 + ((boxes - (TEST_AREA + 0x7000)) & 0x7fu);
        for (uint32_t k = 0; k < 0x80; k += 4) w32(box + k, 0);
        ee_ref<Vec4>(box + 0x30) = at;
        for (uint32_t r = 0; r < 3; ++r) wf(box + 0x40 + r * 0x10 + r * 4, 0.5f);
        return static_cast<int32_t>((box - boxes) >> 7);
    };
    // the hero: by the subject or away, in the subject's zone (its box's centre) or not, on foot (mode 5) or not, holding a
    // moby (of class 0x1b1 or not) or not, in the states 0x14 / 0x16 or not
    auto hero = [](uint32_t m) {
        const uint32_t pv = pvar(m);
        const uint32_t boxes = ee_read<uint32_t>(OV(0x001600ec));
        const int32_t zone = ee_read<int32_t>(pv + 0x40);
        if (zone >= 0 && zone < 64 && boxes >= 0x00100000u && boxes < 0x02000000u && coin()) {
            ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(boxes + (u(zone) << 7) + 0x30);
            if (coin()) ee_ref<Vec4>(m + 0x10) = around(ee_ref<Vec4>(0x0013f3d0), 0.5f, 4.0f, -0.5f, 0.5f);
        } else if (coin()) {
            ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 1.5f : 12.0f, -0.5f, 1.5f);
        }
        w8(0x0013d4c5, ri(0, 4) == 0 ? 0u : 1u);
        w32(0x001404a8, ri(0, 3) == 0 ? u(ri(0, 8)) : 5u);
        const int k = ri(0, 3);
        w32(0x00140480, k == 0 ? 0u : (k == 1 ? pick(live_mobys()) : ee_read<uint32_t>(0x00140480)));
        const uint32_t held = ee_read<uint32_t>(0x00140480);
        if (held != 0 && coin()) {
            w16(held + 0xa6, 0x1b1);
            w8(held + 0x20, ri(0, 3) == 0 ? (coin() ? 0xfeu : 0xfdu) : u(ri(0, 4)));
        }
        w32(0x001413dc, ri(0, 3) == 0 ? (coin() ? 0x16u : 0x14u) : ee_read<uint32_t>(0x001413dc));
        w8(0x001413f4, ri(0, 2) == 0 ? 1u : 0u);
    };
    // the hero taken (state 1's way through): in a zone of the test area, on foot, holding a moby, the house beside it
    auto taken = [test_zone](uint32_t m) {
        const uint32_t pv = pvar(m);
        const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
        w32(pv + 0x40, u(test_zone(h)));
        w8(0x0013d4c5, 1);
        w32(0x001404a8, 5);
        if (ee_read<uint32_t>(0x00140480) == 0 || coin()) w32(0x00140480, pick(live_mobys()));
        if (ee_read<int32_t>(pv + 0x50) != -1) {
            const uint32_t house = first_moby() + (ee_read<uint32_t>(pv + 0x50) << 8);
            ee_ref<Vec4>(house + 0x10) = around(h, 0.3f, 1.5f, -0.2f, 0.2f);
        }
        if (coin()) ee_ref<Vec4>(m + 0x10) = around(h, 0.5f, 3.0f, -0.2f, 0.2f);
    };
    // the subject's state words dealt
    auto vary = [tun, targets, hero, taken](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        // (its house: its own, none, or another moby (one in state 5: open))
        const int hk = ri(0, 5);
        if (hk == 0) w32(pv + 0x50, 0xffffffffu);
        else if (hk == 1) w32(pv + 0x50, moby_index(pick(live_mobys())));
        if (ee_read<int32_t>(pv + 0x50) != -1 && coin())
            w8(first_moby() + (ee_read<uint32_t>(pv + 0x50) << 8) + 0x20, coin() ? 5u : u(ri(0, 6)));
        if (ri(0, 3) == 0) w32(pv + 0x40, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 8)));
        hero(m);
        if (coin()) {
            const std::vector<uint32_t> ts = targets(m, ri(1, 4));
            w32(pv + 0x68, ts.empty() || coin() ? 0u : ts[0]);
        }
        if (st == 1 && coin()) taken(m);
        w8(m + 0xbc, ri(0, 3) == 0 ? u(ri(0, 5)) : (coin() ? 0u : 2u));
        w8(m + 0x31, u(ri(0, 1)));
        w8(m + 0x53, ri(0, 2) == 0 ? (coin() ? 0u : 5u) : u(ri(0, 5)));
        w8(m + 0x52, coin() ? ee_read<uint8_t>(m + 0x53) : u(ri(0, 5)));
        w8(m + 0x70, R() & 0xff);
        w8(m + 0x23, u(ri(0, 0xff)));
        w32(OV(0x0015f5c4), ri(0, 4) == 0 ? 2u : 0u);
        // (timers at their edges, counters about the level's limits)
        for (uint32_t k : {0x4cu, 0x4eu}) w16(pv + k, ri(0, 2) == 0 ? u(ri(0, 2)) : u(ri(0, 130)));
        w32(pv + 0x60, ri(0, 2) == 0 ? u(ri(0, 2)) : u(ri(0, 100)));
        if (st == 3 && coin()) w16(pv + 0x4c, 0);
        if (ri(0, 3) == 0) w32(tun + 0x24, u(ri(1, 12)));  // (the out time, which divides the burrow's fading: small too)
        w32(pv + 0x64, coin() ? u(ri(0, 3)) : u(ee_read<int32_t>(tun + 0x1c) + ri(-3, 1)));
        w16(pv + 0x4a, ri(0, 2) == 0 ? u(ri(15, 30)) : u(ri(0, 3)));
        // (its goal: near it or at the hero; its home)
        if (coin()) ee_ref<Vec4>(pv) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 0.3f : 6.0f, -0.3f, 0.3f);
        if (coin()) ee_ref<Vec4>(pv + 0x30) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 0.5f : 4.0f, -0.3f, 0.3f);
        if (coin()) ee_ref<Vec4>(pv + 0x10) = Vec4{rf(-0.2f, 0.2f), rf(-0.2f, 0.2f), rf(-0.1f, 0.1f), 0.0f};
        if (coin()) for (uint32_t k : {0x40u, 0x44u, 0x48u}) wf(m + k, rf(-3.1f, 3.1f));
        if (ri(0, 3) == 0) wf(m + 0x18, ee_read<float>(m + 0x18) + rf(-1.0f, 1.0f));
        // (once collected: the level's byte or bit)
        if (st == 0 && ri(0, 3) == 0) {
            const int32_t idx = ee_read<int16_t>(m + 0xb2);
            if (coin()) w8(OV(0x001bb230) + u(idx) + 0x454, 1);
            else {
                const uint32_t w = 0x0014c190 + (ee_read<uint32_t>(0x0015ed84) << 8) + (u(idx >> 5) << 2);
                w32(w, ee_read<uint32_t>(w) | (1u << (idx & 0x1f)));
            }
        }
        vary_frame_scale();
        w8(m + 0x20, u(st));
        seed_rand();
    };
    // the walker record and the subject set up first by its state 0 (the port's) in most cases
    auto set_up = [](uint32_t m) {
        const uint8_t st = ee_read<uint8_t>(m + 0x20);
        w8(m + 0x20, 0);
        const uint32_t sp0 = ee_stack_ptr();
        try { class437_update(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
        if (ee_read<uint8_t>(m + 0x20) < 0xfe) w8(m + 0x20, st);
    };
    const std::vector<uint32_t> seg = segment_words();
    const int only_st = std::getenv("B520_ST") != nullptr ? std::atoi(std::getenv("B520_ST")) : -1;
    without(seg, [&] {
        for (int st : {0, 1, 2, 3, 4, 5, 6, 7, 0x13, 0x14})
            if (only_st < 0 || only_st == st)
                classcheck("class437_update", upd, mice, class437_update, [st, vary, set_up](uint32_t m) {
                    if (st != 0 && ri(0, 3) != 0) set_up(m);
                    vary(m, st);
                }, st == 1 || st == 3 || st == 5 || st == 0x14 ? 60 : 30);
    });
    auto any_mouse = [mice, vary, set_up] {
        const uint32_t m = pick(mice());
        if (coin()) set_up(m);
        static const int states[] = {1, 2, 3, 5, 6, 0x13, 0x14};
        vary(m, states[ri(0, 6)]);
        return m;
    };
    without(seg, [&] {
        helpercheck("class437_move", move, 120, Ret::None, [any_mouse, tun] {
            const uint32_t m = any_mouse();
            if (coin()) w8(m + 0x20, 6);
            if (ri(0, 2) == 0) {  // (stuck: hardly moving toward a goal at its place, about its 20 frames, the walk's height)
                ee_ref<Vec4>(pvar(m)) = ee_ref<Vec4>(m + 0x10);
                ee_ref<Vec4>(pvar(m) + 0x10) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
                w16(pvar(m) + 0x4a, u(frame_scale_i(0x14) + ri(-1, 2)));
                if (coin()) wf(tun + 0x20, rf(0.0f, 2.0f));
            }
            return Args{{m, pvar(m)}, {}};
        }, [](A a) { class437_move(a.i[0], a.i[1]); return uint64_t{0}; });
        // the search from a point by the subject (its angles dealt, toward a target or not), a range of 1 .. 60
        helpercheck("class437_find_target", find, 120, Ret::Int32, [mice, targets] {
            const uint32_t m = pick(mice());
            const std::vector<uint32_t> ts = targets(m, ri(1, 6));
            ee_ref<Vec4>(TEST_AREA) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 0.5f, 0.0f, 1.0f);
            const Vec4 q = ee_ref<Vec4>(TEST_AREA);
            float range = coin() ? rf(1.0f, 12.0f) : rf(12.0f, 60.0f);
            if (!ts.empty() && ri(0, 3) != 0) {  // (turned toward one of them, about its pitch, it within the range or not)
                const Vec4 p = ee_ref<Vec4>(pick(ts) + 0x10);
                const float d = std::sqrt((p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y) + (p.z - q.z) * (p.z - q.z));
                wf(m + 0x48, std::atan2(p.y - q.y, p.x - q.x) + (coin() ? rf(-0.1f, 0.1f) : rf(-0.6f, 0.6f)));
                wf(m + 0x44, std::atan2(std::hypot(p.x - q.x, p.y - q.y), p.z + 0.4f - q.z) + rf(-0.2f, 0.2f));
                range = coin() ? d + rf(0.1f, 5.0f) : rf(1.0f, d + 5.0f);
            } else {
                wf(m + 0x48, rf(-3.1f, 3.1f));
                wf(m + 0x44, rf(-0.8f, 0.8f));
            }
            const float near = coin() ? 2.0f : rf(0.0f, 30.0f);
            return Args{{m, TEST_AREA, m + 0x40}, {rf(0.1f, 1.5f), rf(0.1f, 1.5f), range, near, rf(0.1f, 3.2f), rf(0.1f, 3.2f)}};
        }, [](A a) {
            return uint64_t{class437_find_target(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1], a.f[2], a.f[3], a.f[4], a.f[5])};
        });
        helpercheck("class437_shoot", shoot, 120, Ret::None, [any_mouse, targets, tun] {
            const uint32_t m = any_mouse();
            const uint32_t pv = pvar(m);
            if (coin()) w16(pv + 0x4e, 1);
            if (coin()) targets(m, ri(1, 4));
            if (coin()) wf(0x0013f3d8, ee_read<float>(m + 0x18) - rf(0.0f, 3.0f));
            if (coin()) wf(tun + 0x14, rf(0.0f, 2.0f));
            if (coin()) {  // (wide reaches: the targets found more often)
                wf(tun + 8, rf(30.0f, 180.0f));
                wf(tun + 0xc, rf(30.0f, 180.0f));
                wf(tun + 4, rf(10.0f, 60.0f));
            }
            return Args{{m, pv}, {}};
        }, [](A a) { class437_shoot(a.i[0], a.i[1]); return uint64_t{0}; });
        helpercheck("class437_goal_by_hero", goal, 60, Ret::None, [any_mouse] {
            const uint32_t m = any_mouse();
            const uint32_t pv = coin() ? pvar(m) : TEST_AREA + 0x100;
            if (pv != pvar(m)) w32(pv + 0x68, coin() ? 0u : m);
            return Args{{m, pv}, {}};
        }, [](A a) { class437_goal_by_hero(a.i[0], a.i[1]); return uint64_t{0}; });
        helpercheck("class437_sink", sink, 60, Ret::Int32, [any_mouse] {
            const uint32_t m = any_mouse();
            if (coin()) ee_ref<Vec4>(pvar(m) + 0x10) = Vec4{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(-0.5f, 0.2f), 0.0f};
            return Args{{m, pvar(m)}, {}};
        }, [](A a) { return uint64_t{class437_sink(a.i[0], a.i[1])}; });
    });
    // the collision point: of a live moby with a collision (its primitives of every kind: the subject's or another moby's),
    // the outputs on the test area
    helpercheck("moby_collision_point", point, 80, Ret::Int32, [mice] {
        std::vector<uint32_t> with;
        for (const uint32_t o : live_mobys())
            if (ee_read<uint32_t>(o + 0x94) >= 0x00100000u && ee_read<uint32_t>(o + 0x94) < 0x02000000u) with.push_back(o);
        const uint32_t m = with.empty() || ri(0, 4) == 0 ? pick(mice()) : pick(with);
        const uint32_t coll = ee_read<uint32_t>(m + 0x94);
        const uint32_t joint = u(ri(0, 3));
        if (coll >= 0x00100000u && coll < 0x02000000u && ri(0, 2) != 0) {  // (the primitive's kind dealt: 0 .. 5)
            const uint32_t e = coll + (joint << 5) + 0x10;
            w8(e, u(ri(0, 5)));
            if (coin()) w32(e + 4, u(ri(0, 20)));
            if (coin()) { w16(e + 4, u(ri(0, 20))); w16(e + 6, u(ri(0, 20))); }
            if (ri(0, 3) == 0) w16(coll + 2, 0);  // (the pose's joints: the moby's own, or none)
        }
        if (coin()) wfrand(m + 0x40, 3, -3.1f, 3.1f);
        for (uint32_t k = 0; k < 0x40; k += 4) w32(TEST_AREA + 0x200 + k, R());
        return Args{{m, joint, TEST_AREA + 0x200, TEST_AREA + 0x210, TEST_AREA + 0x220, TEST_AREA + 0x224}, {}};
    }, [](A a) { return uint64_t{moby_collision_point(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5])}; });
    (void)rec;
}

// ---- the gadgets picked up before a panel (shared_gadget_pickups.cpp)
void gadget_pickup_checks() {
    const uint32_t upd = copy_here({{2, 0x002ea210}, {6, 0x002f4638}});
    const uint32_t cls = g_oracle_level == 2 ? 1005u : 1016u;
    auto subjects = [upd, cls] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (v.empty()) v = placed_of(cls);
        if (v.empty()) {
            const uint32_t m = moby_spawn(cls);
            if (m != 0 && pvar(m) != 0) v.push_back(m);
        }
        return v;
    };
    if (upd != 0 && info()) {
        level();
        std::printf("INFO class %u: %zu awake, %zu placed\n", cls, mobys_with(upd).size(), placed_of(cls).size());
        for (const uint32_t m : subjects()) std::printf("INFO   %08x state %u\n", m, ee_read<uint8_t>(m + 0x20));
    }
    auto vary = [](uint32_t m, int st) {
        const uint32_t pv = pvar(m);
        // (the class number tested: the Trespasser's or not, on both levels)
        if (ri(0, 2) == 0) w16(m + 0xa6, coin() ? 1005u : 1016u);
        const uint32_t item = ee_read<uint16_t>(m + 0xa6) == 1005 ? 0x1au : 0x16u;
        w8(0x0013d4e8 + item, ri(0, 3) == 0 ? 1u : 0u);
        // (the hero by it, within the reach and height or not; the frame count on a tenth or not)
        if (coin()) ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 0.0f, coin() ? 1.0f : 3.0f, coin() ? -1.5f : -3.0f, coin() ? 1.5f : 3.0f);
        w32(OV(0x0015f5cc), coin() ? u(ri(0, 1000)) * 10 : R() & 0xffffu);
        w32(OV(0x0015f5c4), ri(0, 3) == 0 ? 2u : u(ri(0, 1)));
        w8(0x001413f4, coin() ? 0u : u(ri(1, 3)));
        w32(0x0013f350 + 0x22a8, ri(0, 3) == 0 ? 0u : 1u);  // (the scene started, or refused: the hero lost)
        // (the camera's move: its way, its speed, the camera's kind, the scripted camera running or not, the fade near its ends)
        wf(pv + 0x40, ri(0, 3) == 0 ? (coin() ? 0.0f : 1.0f) : rf(0.0f, 1.0f));
        wf(pv + 0x44, rf(-0.05f, 0.05f));
        const uint32_t cam = ee_read<uint32_t>(OV(0x00166e00));
        if (cam >= 0x00100000u && cam < 0x02000000u && ri(0, 3) == 0) w16(cam + 0x86, coin() ? 5u : 0u);
        if (ri(0, 3) == 0) w32(OV(0x00166e00), 0);
        w32(OV(0x00179410), ri(0, 3) == 0 ? 1u : 0u);
        w32(OV(0x00179434), ri(0, 3) == 0 ? u(ri(0, 3)) : 0xffffffffu);
        {
            const int k = ri(0, 3);
            wf(OV(0x0015f3fc), k == 0 ? rf(0.0f, 1.0f) : (k == 1 ? rf(0.9f, 1.0f) : (k == 2 ? rf(0.0f, 0.1f) : 1.0f)));
        }
        if (coin()) wf(m + 0x48, rf(-3.1f, 3.1f));
        vary_frame_scale();
        w8(m + 0x20, u(st));
        seed_rand();
    };
    with_level_change([&] {
        for (int st = 0; st < 7; ++st)
            classcheck("gadget_pickup_update", upd, subjects, gadget_pickup_update, [st, vary](uint32_t m) { vary(m, st); },
                       st == 1 || st == 3 || st == 4 ? 60 : 20);
    });
}

void class1428_checks() {
    const uint32_t upd = copy_here({{15, 0x002ebd78}, {17, 0x002f1790}});
    auto subjects = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (v.empty()) v = placed_of(1428);
        if (v.empty()) {
            const uint32_t m = moby_spawn(1428);
            if (m != 0 && pvar(m) != 0) v.push_back(m);
        }
        return v;
    };
    if (upd != 0 && info()) {
        level();
        std::printf("INFO class 1428: %zu awake, %zu placed\n", mobys_with(upd).size(), placed_of(1428).size());
    }
    with_level_change([&] {
    for (int st = 0; st < 5; ++st)
        classcheck("class1428_update", upd, subjects, class1428_update, [st](uint32_t m) {
            // (the level's number: on Quartu itself nothing runs, so Quartu's copy is run as if elsewhere mostly)
            w32(0x0015ed84, ri(0, 5) == 0 ? 0xfu : (coin() ? 0x11u : u(ri(1, 18))));
            w8(0x0013d38a, ri(0, 3) == 0 ? 1u : 0u);
            w32(0x0013f350 + 0x22a8, ri(0, 3) == 0 ? 0u : 1u);  // (the scene started, or refused: the hero lost)
            if (coin()) ee_ref<Vec4>(0x0013f3d0) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 4.0f, -1.5f, 1.5f);
            w32(0x001413d4, ri(0, 3) == 0 ? 0x32u : ee_read<uint32_t>(0x001413d4));
            w32(OV(0x0015f5c4), ri(0, 3) == 0 ? 2u : u(ri(0, 1)));
            if (coin()) wf(m + 0x48, rf(-3.1f, 3.1f));
            vary_frame_scale();
            w8(m + 0x20, u(st));
            seed_rand();
        }, st == 1 || st == 2 ? 40 : 20);
    });
}

// ---- class 1629, a shower of drops (shared_class1629.cpp)
void class1629_checks() {
    const uint32_t upd = copy_here({{8, 0x003085f0}, {14, 0x003088b0}});
    auto subjects = [upd] {
        std::vector<uint32_t> v = mobys_with(upd);
        if (v.empty()) v = placed_of(1629);
        if (v.empty()) {
            const uint32_t m = moby_spawn(1629);
            if (m != 0 && pvar(m) != 0) v.push_back(m);
        }
        return v;
    };
    if (upd != 0 && info()) {
        level();
        std::printf("INFO class 1629: %zu awake, %zu placed\n", mobys_with(upd).size(), placed_of(1629).size());
        for (const uint32_t m : subjects()) {
            const uint32_t pv = pvar(m);
            std::printf("INFO   %08x kind %d reach %g radii %g %g near %g count %d fall %g %g below %g gust %g angles %g %g\n", m,
                        ee_read<int32_t>(pv), ee_read<float>(pv + 4), ee_read<float>(pv + 0xc), ee_read<float>(pv + 8),
                        ee_read<float>(pv + 0x10), ee_read<int32_t>(pv + 0x14), ee_read<float>(pv + 0x18), ee_read<float>(pv + 0x1c),
                        ee_read<float>(pv + 0x20), ee_read<float>(pv + 0x24), ee_read<float>(pv + 0x28), ee_read<float>(pv + 0x2c));
        }
    }
    classcheck("class1629_update", upd, subjects, class1629_update, [](uint32_t m) {
        const uint32_t pv = pvar(m);
        if (ri(0, 2) == 0) {  // (its record dealt: a small count, reaches about the camera's)
            w32(pv, u(ri(0, 2)));
            wf(pv + 4, rf(5.0f, 60.0f));
            wf(pv + 8, rf(2.0f, 20.0f));
            wf(pv + 0xc, rf(0.0f, 2.0f));
            wf(pv + 0x10, rf(0.0f, 10.0f));
            wf(pv + 0x18, rf(1.0f, 10.0f));
            wf(pv + 0x1c, rf(10.0f, 30.0f));
            wf(pv + 0x20, rf(0.0f, 20.0f));
            wf(pv + 0x24, rf(0.0f, 3.0f));
            wf(pv + 0x28, rf(0.0f, 180.0f));
            wf(pv + 0x2c, rf(180.0f, 360.0f));
        }
        w32(pv + 0x14, ri(0, 4) == 0 ? u(ri(-1, 0)) : u(ri(1, 6)));
        // (the camera within its reach, or out of it)
        const float reach = ee_read<float>(pv + 4);
        ee_ref<Vec4>(OV(0x00166dc0)) = around(ee_ref<Vec4>(m + 0x10), 0.0f, std::fabs(reach) * (coin() ? 0.9f : 1.5f), -5.0f, 5.0f);
        vary_frame_scale();
        seed_rand();
    }, 60);
}

// ---- Batalia's layers of scrolling meshes (shared_scroll.cpp)
void layers_draw_checks() {
    const bool batalia = g_oracle_level == 8;
    struct Lw {
        const char* name;
        uint32_t addr;
        void (*fn)(uint32_t);
        uint32_t words, fade, offsets[3];
    };
    static const Lw draws[] = {{"class600_draw", 0x002f0dc8, class600_draw, 0x00161dd0, 0x00161e08, {0x001e8348, 0x00161e10, 0x00161e20}},
                               {"class1649_draw", 0x00309030, class1649_draw, 0x00162390, 0x001623f0, {0x001f8548, 0x001623f8, 0x00162408}}};
    for (const Lw& x : draws) {
        const Lw d = x;
        fcheck(d.name, batalia ? d.addr : 0u, 40, Ret::None, [d] {
            level();
            packet_cursor();
            if (coin()) for (uint32_t k = 0; k < 0x34; k += 4) w32(d.words + k, coin() ? u(ri(0, 0x80)) : R());  // (the alphas)
            w8(d.fade, coin() ? 0x80u : (R() & 0xffu));
            for (const uint32_t o : d.offsets) wfrand(o, 6, -1.0f, 1.0f);
            return Args{{R()}, {}};
        }, [d](A a) { d.fn(a.i[0]); return uint64_t{0}; });
    }
    fcheck("uv_scroll_meshes", batalia ? 0x00308f68u : 0u, 24, Ret::None, [] {
        level();
        packet_cursor();
        const uint32_t idx = u(ri(0, 2));
        wfrand(0x00162408 + idx * 8, 2, -1, 1);
        return Args{{idx}, {}};
    }, [](A a) { uv_scroll_meshes(11, a.i[0]); return uint64_t{0}; });
}

// ---- the sea's draw callback (shared_sea_draw.cpp)
// the camera (Veldin's 0x166dc0) moved near a point, its view rows (0x166fd0) turned about z; the render frame's view made
// again (batch 513's)
void move_camera(float x, float y, float z, float spread) {
    Vec4& c = ee_ref<Vec4>(OV(0x00166dc0));
    c.x = x + rf(-spread, spread);
    c.y = y + rf(-spread, spread);
    c.z = z + rf(0.5f, spread / 3);
    if (coin()) {
        const float a = rf(-3.14f, 3.14f), cs = std::cos(a), sn = std::sin(a);
        for (uint32_t r = OV(0x00166fd0); r < OV(0x00166fd0) + 0x30; r += 0x10) {
            Vec4& v = ee_ref<Vec4>(r);
            const float px = v.x, py = v.y;
            v.x = cs * px - sn * py;
            v.y = sn * px + cs * py;
        }
    }
    render_view_setup();
}
void sea_draw_checks() {
    struct Sd {
        const char* name;
        int level;
        uint32_t addr, rec;
        void (*fn)(uint32_t);
    };
    static const Sd seas[] = {{"sea_draw", 3, 0x002dc990, 0x001dc1a0, sea_draw},   {"sea_draw", 7, 0x002f8038, 0x001d3400, sea_draw},
                              {"sea_draw", 8, 0x002da0d0, 0x001d7060, sea_draw},   {"sea_draw", 14, 0x00303350, 0x001e9c60, sea_draw},
                              {"sea_draw_2", 7, 0x0030d420, 0x00208560, sea_draw_2}};
    bool any = false, any2 = false;
    for (const Sd& x : seas) {
        if (x.level != g_oracle_level) continue;
        (std::strcmp(x.name, "sea_draw") == 0 ? any : any2) = true;
        const Sd d = x;
        fcheck(d.name, d.addr, 60, Ret::None, [d] {
            level();
            packet_cursor();
            // the camera over a random point of the record's grid (origin +0, step +0x10, counts +0x18 / +0x1a), or away
            const float ox = ee_read<float>(d.rec), oy = ee_read<float>(d.rec + 4), oz = ee_read<float>(d.rec + 8);
            const float st = ee_read<float>(d.rec + 0x10);
            const float w = st * ee_read<uint16_t>(d.rec + 0x18), h = st * ee_read<uint16_t>(d.rec + 0x1a);
            move_camera(ox + rf(0.0f, w), oy + rf(0.0f, h), oz + 2.0f, ri(0, 3) == 0 ? 600.0f : (coin() ? 120.0f : 15.0f));
            if (ri(0, 3) == 0) w32(OV(0x0015f5cc), R() & 0xffffu);
            if (ri(0, 2) == 0) w32(OV(0x0015f458), u(ri(0, 2)));
            return Args{{R()}, {}};
        }, [d](A a) { d.fn(a.i[0]); return uint64_t{0}; });
    }
    if (!any) fcheck("sea_draw", 0, 0, Ret::None, nullptr, nullptr);
    if (!any2) fcheck("sea_draw_2", 0, 0, Ret::None, nullptr, nullptr);
}
}  // namespace

void batch_520() {
    if (g_only_batch != 520) return;
    g_batch = 520;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // 989snd's IOP server (as batches 120 / 506 / 518 / 519): replies a function of the command
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

    class437_checks();
    gadget_pickup_checks();
    class1428_checks();
    class1629_checks();
    layers_draw_checks();
    sea_draw_checks();

    g_pre_call = nullptr;
    g_extra_noop_hooks.clear();
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
