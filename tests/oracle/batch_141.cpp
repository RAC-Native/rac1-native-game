// Oracle batch 141 (task 0390, Blarg's Nebula G34 part 2): what a play-through of Blarg's Nebula G34 still reached unported or unproved.
// Run with ORACLE_LEVEL=6 build/tests/ovl_oracle --batch 141 (Blarg's own original against the relocated port; needs the ISO).
//  - Blarg's 18 class draw callbacks (blarg_part2_tr.cpp): 302, 615, 832, 857, 1035, 1062 (three), 1068, 1083 (four), 1108, 1123, 1302,
//    1818, 1898, on a moby of the class moved about the camera, its pvars varied;
//  - the hero's item mode start 0x29a6b0 at Blarg's L6:0x2a77a8 (vendor_screen_start_tr, called by the regenerated hero files);
//  - the Gold Bolt 1134's states 0 / 1 / 2 (task 0350 skipped 1 / 2: the original polled the console clock / the disc through the IOP;
//    the host's answer them on both sides, as batch 138 does);
//  - on RAM dumps of the level (tools/oracle/blarg_tour.sh dumps: assets/dumps/oracle/blarg_0390/*.bin; skipped without them): the classes the level
//    spawns after its first frame (302 / 303, spawned by class 857) and the HUD element of class 1051, their updates and draw callbacks.

#include "ovl_oracle.h"

#include <game/ovl/shared_beam.h>
#include <game/ovl/shared_blarg_meshes.h>
#include <game/ovl/shared_boss_bar.h>
#include <game/ovl/shared_class1108.h>
#include <game/ovl/shared_class1302.h>
#include <game/ovl/shared_class302.h>
#include <game/ovl/shared_class615.h>
#include <game/ovl/shared_class827.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_class857.h>
#include <game/ovl/shared_flash.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/lib/libcdvd.h>
#include <game/ovl/blarg_mobys.h>
#include <game/ovl/blarg_part2.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/vendor_tr.h>

#include <cmath>
#include <cstring>
#include <map>
#include <string>

namespace rac1::oracle {

namespace {
// A check whose original is Blarg's own function at `l6addr`.
void b6check(const char* name, uint32_t l6addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (g_oracle_level != 6) {
        std::printf("%-26s L6:0x%08x  skipped (Blarg only: run with ORACLE_LEVEL=6)\n", name, l6addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L6:0x%08x\n", name, l6addr);
    if (!check_fn(name, l6addr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
// the point at `p` put about the camera, until the port's screen test (0x2002e0) sees it with radius r (as batch 140's); w = 1
void into_view(uint32_t p, float r, float spread) {
    const Vec4 c = VV(OV(0x00166dc0));
    for (int k = 0; k < 200; ++k) {
        VV(p) = Vec4{c.x + rf(-spread, spread), c.y + rf(-spread, spread), c.z + rf(-spread / 3, spread / 3), 1.0f};
        if (point_on_screen(p, r) != -1) return;
    }
}
// a RAM dump of a headless run (32 MB + the scratchpad), empty when the file is missing
const std::vector<uint8_t>& dump(const std::string& path) {
    static std::map<std::string, std::vector<uint8_t>> cache;
    std::vector<uint8_t>& d = cache[path];
    if (d.empty()) {
        if (FILE* f = std::fopen(path.c_str(), "rb")) {
            d.resize(0x02000000 + 0x4000);
            if (std::fread(d.data(), 1, d.size(), f) != d.size()) d.clear();
            std::fclose(f);
        }
    }
    return d;
}
void load_dump(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}
}  // namespace

void batch_141() {
    if (g_only_batch != 141) return;
    g_batch = 141;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // (the particle update's saved return address)
    g_ignored_words.push_back(OV(0x0016018c));
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    g_ignored_words.push_back(0x0017461c);  // (the w lane of segment_collide's hit point: an SQC2 of vf27 at L6 0x20885c whose w the VU0 ops leave stale)
    g_extra_noop_hooks.clear();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // the console clock / the disc reads the originals poll (IOP calls): the host's on both sides, as batch 138 does
    const std::time_t saved_clock = g_host_clock_fixed;
    g_host_clock_fixed = 1759658400;
    g_extra_hooks = {{0x00121780, [](EeCpu& c) { c.set32(2, cd_read_clock(c.r32(4))); }},
                     {0x0012d6d8, [](EeCpu& c) { clock_to_local(c.r32(4)); }},
                     {0x00121450, [](EeCpu& c) { c.set32(2, cd_read(c.r32(4), c.r32(5), c.r32(6), c.r32(7))); }}};

    // the level after its first gameplay frame (the port's)
    static std::vector<uint8_t> snap;
    static int snap_level = -1;
    auto level = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty() || snap_level != g_oracle_level) {
            veldin_boot();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            gameplay_frame();
            render_view_setup();  // (the view matrices of the render frame: zero after an update alone)
            snap.assign(ram, ram + 0x02000000);
            snap_level = g_oracle_level;
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    auto live = [](int cls) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && (cls < 0 || static_cast<int16_t>(ee_read<uint16_t>(m + 0xa6)) == cls)) v.push_back(m);
        return v;
    };
    auto pick = [](const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; };
    // a moby's pvars' floats / small counters varied (as batch 140's)
    auto vary_pvars = [](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (pv == 0 || !coin()) return;
        for (uint32_t o = 0; o < 0x80; o += 4) {
            const uint32_t w = ee_read<uint32_t>(pv + o);
            const uint32_t e = (w >> 23) & 0xff;
            if (e > 100 && e < 150 && coin()) wf(pv + o, ee_read<float>(pv + o) * rf(0.5f, 1.5f));
            else if (w < 0x100 && coin()) w32(pv + o, u(ri(0, 0x40)));
        }
    };
    if (g_oracle_level == 6) {  // the debris / spawns leave w lanes of a VU0 sin / cos in the original (as batch 117)
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
    }

    // (diagnostic, B141_GRID=1: the live mobys whose id is missing from a cell of their grid rect +0xa0 (cells 0x19b7e0 + 4 (x + 64 y),
    // blocks of u16 ids from the cells + 0x4000): the state in which moby_grid_move's search reaches the original's `teq`)
    if (g_oracle_level == 6 && std::getenv("B141_GRID") != nullptr) {
        auto grid_check = [live](const char* what) {
            int bad = 0, n = 0;
            const uint32_t cells = OV(0x0019b7e0), pool = cells + 0x4000;
            for (uint32_t m : live(-1)) {
                const uint32_t rect = ee_read<uint32_t>(m + 0xa0);
                if (static_cast<int32_t>(rect) < 0 || (rect == 0 && ee_read<uint32_t>(m + 0xac) == 0)) continue;  // (an all-zero slot)
                ++n;
                const uint32_t id = ee_read<uint32_t>(m + 0xac) & 0xffff;
                for (uint32_t y = (rect >> 8) & 0xff; y <= (rect >> 24); ++y)
                    for (uint32_t x = rect & 0xff; x <= ((rect >> 16) & 0xff); ++x) {
                        const uint32_t c = cells + x * 4 + (y << 8);
                        const uint32_t first = pool + (ee_read<uint16_t>(c) << 5);
                        bool found = false;
                        for (uint32_t k = 0; k < ee_read<uint8_t>(c + 2); ++k) found |= ee_read<uint16_t>(first + 2 * k) == id;
                        if (!found && bad++ < 5) std::printf("  grid: moby %08x class %d id %u missing from cell %u,%u\n", m, ee_read<int16_t>(m + 0xa6), id, x, y);
                    }
            }
            std::printf("grid check %s: %d mobys in the grid, %d missing cell entries\n", what, n, bad);
        };
        level();
        grid_check("first frame");
        for (const char* f : {"idle900", "c1051_1100"}) {
            const std::string path = dump_path("blarg_0390/") + f + ".bin";
            if (dump(path).empty()) continue;
            load_dump(dump(path));
            grid_check(f);
        }
    }

    // ---- Blarg's own draw callbacks: a moby of the class (any when none is live) moved about the camera, its pvars varied; on the level's
    // first frame and on the RAM dumps (where the spawned classes live)
    struct Cb {
        const char* name;
        uint32_t addr;
        void (*fn)(uint32_t);
        int cls;
    };
    const Cb cbs[] = {{"blg_class302_draw", 0x002d8738, class302_draw, 302},       {"blg_class615_draw", 0x002e2258, class615_puzzle_draw, 615},
                      {"blg_class832_draw", 0x002ea830, class832_draw, 832},       {"blg_class857_draw", 0x002f37d0, class857_draw, 857},
                      {"blg_class1035_draw", 0x002f6dd0, blg_class1035_draw_l6, 1035},    {"blg_class1062_draw_a", 0x002fd1a0, blg_class1062_draw_a_l6, 1062},
                      {"blg_class1062_draw_b", 0x002fd460, blarg_class1062_draw_b, 1062}, {"blg_class1062_draw_c", 0x002fd748, blarg_class1062_draw_c, 1062},
                      {"blg_class1068_draw", 0x002ff680, beam_draw, 1068},    {"blg_class1083_draw_a", 0x003003f0, blarg_class1083_draw_a, 1083},
                      {"blg_class1083_draw_b", 0x00300588, blarg_class1083_draw_b, 1083}, {"blg_class1083_draw_c", 0x00300720, blarg_class1083_draw_c, 1083},
                      {"blg_class1083_draw_d", 0x00300958, blarg_class1083_draw_d, 1083}, {"blg_class1108_draw", 0x003021d8, class1108_draw, 1108},
                      {"blg_class1123_draw", 0x00305170, blg_class1123_draw_l6, 1123},    {"blg_class1302_draw", 0x00307578, class1302_draw, 1302},
                      {"blg_class1818_draw", 0x0030adb0, sandmouse_glow, 1818},    {"blg_class1898_draw", 0x0030bfa8, flash_draw_quad, 1898}};
    std::vector<std::pair<std::string, std::function<void()>>> states = {{"", level}};
    for (const char* f : {"idle900", "c1051_1100"}) {
        const std::string path = dump_path("blarg_0390/") + f + ".bin";
        if (g_oracle_level == 6 && !dump(path).empty()) states.push_back({std::string("_") + f, [path] { load_dump(dump(path)); }});
        else if (g_oracle_level == 6) std::printf("(%s missing: tools/oracle/blarg_tour.sh dumps)\n", path.c_str());
    }
    for (const auto& [tag, load] : states)
    for (const Cb& cb : cbs) {
        const std::string nm = std::string(cb.name) + tag;
        if (g_oracle_level != 6) {
            if (tag.empty()) b6check(nm.c_str(), cb.addr, 0, Ret::None, nullptr, nullptr);
            continue;
        }
        load();
        std::vector<uint32_t> ms = live(cb.cls);
        if (ms.empty() && !tag.empty()) continue;  // (a dump: only the classes it holds)
        if (ms.empty()) ms = live(-1);
        const auto ld = load;
        b6check(nm.c_str(), cb.addr, tag.empty() ? 100 : 50, Ret::None, [ld, ms, pick, vary_pvars] {
            ld();
            const uint32_t m = pick(ms);
            if (coin()) into_view(m + 0x10, coin() ? 0.0f : 8.0f, coin() ? 20.0f : 150.0f);
            vary_pvars(m);
            if (coin()) w8(m + 0x20, u(ri(0, 4)));
            if (coin()) w8(m + 0x30, u(ri(0, 0xff)));
            w32(OV(0x0015f5cc), R());
            seed_rand();
            return Args{{m}, {}};
        }, [cb](A a) { cb.fn(a.i[0]); return uint64_t{0}; });
    }

    // ---- the classes the level spawns after its first frame (302 / 303: the parts of the boss 857, spawned by its update), on the dumps:
    // their state byte (as found / 0..8), the hero near or far, the pvars varied
    struct Upd {
        const char* name;
        uint32_t addr;
        void (*fn)(uint32_t);
        int cls;
    };
    const Upd upds[] = {{"blg_class302_update", 0x002d85a0, class302_update, 302}, {"blg_class303_update", 0x002d8c20, class303_update, 303},
                        {"blg_class1898_update", 0x0030be60, flash_update, 1898}};
    for (const auto& [tag, load] : states) {
        if (tag.empty()) continue;
        for (const Upd& up : upds) {
            load();
            const std::vector<uint32_t> ms = live(up.cls);
            if (ms.empty()) continue;
            const std::string nm = std::string(up.name) + tag;
            const auto ld = load;
            b6check(nm.c_str(), up.addr, 60, Ret::None, [ld, ms, pick, vary_pvars] {
                ld();
                const uint32_t m = pick(ms);
                if (coin()) w8(m + 0x20, u(ri(0, 8)));
                vary_pvars(m);
                Vec4 q = ee_ref<Vec4>(m + 0x10);
                const float r = coin() ? rf(0, 6) : rf(6, 60), a = rf(-3.1f, 3.1f);
                q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-2, 4);
                ee_ref<Vec4>(0x0013f3d0) = q;
                w32(OV(0x0015f5cc), R() & 0xffff);
                seed_rand();
                return Args{{m}, {}};
            }, [up](A a) { up.fn(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- class 827 (131 placed, task 0350 left its state 1 unproved: batch 117's random pvars made its debris spawn 0x2e9f30 retire a
    // moby that is not in the grid, where moby_grid_move's search ends in the original's `teq` trap): state 1 on the level's real
    // mobys of the class in state 1 (on the first frame and the dumps), the hero near or far, the frame counter varied; the pvars as the game
    // left them
    for (const auto& [tag, load] : states) {
        load();
        std::vector<uint32_t> ms;
        if (g_oracle_level == 6)
            for (uint32_t m : live(827))
                if (ee_read<uint8_t>(m + 0x20) == 1) ms.push_back(m);  // (the mobys the game left in state 1: forcing the state on another leaves its pointers unset)
        if (g_oracle_level == 6 && ms.empty()) continue;
        const std::string nm = std::string("blg_class827_state1") + tag;
        const auto ld = load;
        if (g_oracle_level != 6) { if (tag.empty()) b6check(nm.c_str(), 0x002e8678, 0, Ret::None, nullptr, nullptr); continue; }
        b6check(nm.c_str(), 0x002e8678, 60, Ret::None, [ld, ms, pick] {
            ld();
            const uint32_t m = pick(ms);
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            const float r = coin() ? rf(0, 4) : rf(4, 40), a = rf(-3.1f, 3.1f);
            q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-1, 3);
            ee_ref<Vec4>(0x0013f3d0) = q;
            ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10) = q;  // (the hero moby's own position too: within 4 the debris spawn 0x2e9f30 runs)
            w32(OV(0x0015f5cc), R() & 0xffff);
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { class827_update(a.i[0]); return uint64_t{0}; });
    }

    // ---- the HUD element class 1051 requests (0x2454c0): its draw function L6:0x249738 (+0x18 of the element, 0x17dc50 + 0x90 i), on the
    // dumps where it is up; its counter word +0x24 varied (as batch 140's race HUD)
    for (const auto& [tag, load] : states) {
        if (tag.empty()) continue;
        load();
        std::vector<uint32_t> es;
        for (uint32_t e = OV(0x0017dc50), i = 0; i < 13; ++i, e += 0x90)
            if (ee_read<uint32_t>(e + 0x18) == 0x00249738u) es.push_back(e);
        if (es.empty()) continue;
        const std::string nm = std::string("blg_class1051_hud_draw") + tag;
        const auto ld = load;
        b6check(nm.c_str(), 0x00249738, 60, Ret::None, [ld, es, pick] {
            ld();
            const uint32_t e = pick(es);
            if (coin()) w32(e + 0x24, u(ri(0, 0x40)));
            if (coin()) w32(e + 0x7c, u(ri(0, 0x40)));
            w32(OV(0x0015f5cc), R());
            return Args{{e}, {}};
        }, [](A a) { boss_health_bar_draw(a.i[0]); return uint64_t{0}; });
    }

    // ---- the hero's item mode start 0x29a6b0 (Blarg's L6:0x2a77a8, called by blg_hero_* through vendor_screen_start_tr): from a Vendor
    // or 0 (the hero's item 0x20), as batch 113 does on Veldin. The DMA kick, FlushCache and the vsync wait stubbed on both sides.
    if (g_oracle_level == 6) {
        g_extra_noop_hooks = {OVF(0x002a08c8), 0x00122298, 0x00118a80};
        check("vendor_screen_start_l6", 0x0029a6b0, 40, Ret::None, [level, live, pick] {
            level();
            const std::vector<uint32_t> vs = live(11);
            const uint32_t v = vs.empty() || coin() ? 0u : pick(vs);
            seed_rand();
            return Args{{v}, {}};
        }, [](A a) { vendor_screen_start_tr(a.i[0]); return uint64_t{0}; });
        g_extra_noop_hooks.clear();
    } else {
        b6check("vendor_screen_start_l6", 0x002a77a8, 0, Ret::None, nullptr, nullptr);
    }

    // ---- the Gold Bolt 1134: states 0 / 1 / 2, the hero right at it (the pickup) or not, alive (+0x22a8) and in a state that may collect
    // (+0x20a4), the bolt already collected or not (its byte of 0x14bec0), the kind (+0xc): batch 138's on Kerwan's
    if (g_oracle_level == 6) {
        level();
        std::vector<uint32_t> gb;
        for (uint32_t m : live(1134)) gb.push_back(m);
        for (int pass = 0; pass < 8 && !gb.empty(); ++pass)
            b6check("gold_bolt1134_states", 0x003053a8, 25, Ret::None, [level, gb, pick, pass] {
                level();
                const uint32_t m = pick(gb);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78), P = 0x0013f350;
                w8(m + 0x20, u(pass % 3));
                Vec4 q = ee_ref<Vec4>(m + 0x10);
                if (ri(0, 3) != 0) { q.x += rf(-2.0f, 2.0f); q.y += rf(-2.0f, 2.0f); q.z += rf(-2.5f, 2.5f); }
                else { q.x += rf(-10.0f, 10.0f); q.y += rf(-10.0f, 10.0f); }
                ee_ref<Vec4>(P + 0x80) = q;
                w32(P + 0x22a8, ri(0, 4) == 0 ? 0u : 1u);
                { static const uint8_t st[] = {0, 3, 3, 1, 7}; w8(P + 0x20a4, st[ri(0, 4)]); }
                if (coin()) w16(pv + 0xc, ri(0, 2) == 0 ? 0u : 1u);
                const int32_t idx = static_cast<int32_t>(ee_read<uint32_t>(pv));
                if (idx >= 0 && idx < 4) w8(0x0014bec0 + ee_read<uint32_t>(0x0015ed84) * 4 + static_cast<uint32_t>(idx), ri(0, 3) == 0 ? 1u : 0u);
                if (pass >= 6) w8(m + 0x53, R() & 3);
                w32(0x0015eea4, R() & 0xffff);
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { gold_bolt_update(a.i[0]); return uint64_t{0}; });
    } else {
        b6check("gold_bolt1134_states", 0x003053a8, 0, Ret::None, nullptr, nullptr);
    }

    g_extra_hooks.clear();
    g_host_clock_fixed = saved_clock;
    g_extra_noop_hooks.clear();
    g_ee_null_is_ram = null_ram;
    g_ignored_words = saved_ignored;
}

}  // namespace rac1::oracle
