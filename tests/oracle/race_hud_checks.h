// Task H3: the hoverboard races' HUD draws (the trick meter's L5:0x262f50 / L16:0x235660, the bottom bar's L5:0x266320 /
// L16:0x238a30: one body relinked, shared_race_hud.cpp) checked at a level's own addresses, for batch 114 (Rilgar, level 5)
// and batch 250 (Kalebo III, level 16). Each case starts from the level after its first frame or, in half of them when the
// set is there, from a RAM dump of the level's race (Rilgar: rilgar_0387/race1200.bin, tools/oracle/rilgar_tour.sh dumps;
// Kalebo III: levels_15_16/race_1500/2500/3500_L16.bin, batch 242's header), on the HUD slot record the race uses (slot 0 for
// the meter, 5 for the bar: hud_element 0x17dc50 + 0x90 i; on a dump the live element whose draw +0x18 is the function), and
// deals the race's and the hero's words the two read: the meter's state 0 .. 3 (and 4), its timer (the fade), the message's
// frames, the tricks (none, one with a score of 360 or not, two, three with each pattern of flags, four, more), the score's tier,
// the history's repeats of the combo, the boost message's frames; the bar's fade, the lap -1 .. 5, the place 0 .. 12 (and
// negative), the race flag, the bolt, the level number (the bar's Kalebo III part), the widescreen flag; the frame scale at its
// 50 Hz value in a quarter; the packet cursor on the frame's own ring (the draws' packets compared).
//  - race_meter_draw_l<n> / race_bar_draw_l<n>: the port against the original.
//  - race_fix19_meter / race_fix19_bar (RAC1_FIXES102=19, skipped otherwise): 1.02 Fixes fix 19. The original runs with the
//    three statements tools/codegen/hooks.glue placed in its generated copies until H3, at the same addresses (interpreter probes:
//    fixes102_race_text after the poses' and the flips' sprintf, from the arguments latched at their jal, and at the join after the
//    place's sprintf), the port with the fix on.
//  - race_pal_meter / race_pal_bar: a PAL disc's state (the region flag 0x15ee90 set): the original runs with the PAL builds'
//    lines put in at the same points (their code: orch/w7/h3/pal.md; the poses, the flips and the place from the level's text
//    table as fix 19 writes them, the label " %s:  " with its two spaces), the port with its region fork.
// The HUD-width hooks (render_view_hud_begin / _end): none in these two functions (tools/codegen/hooks.glue has no line for
// levels 5 / 16; the race's elements are placed by the HUD's slots, hud.cpp's hud_race_bar keys on their EE addresses).
#ifndef RAC1_TESTS_ORACLE_RACE_HUD_CHECKS_H
#define RAC1_TESTS_ORACLE_RACE_HUD_CHECKS_H

#include "ovl_oracle.h"

#include <game/enhance/advanced.h>
#include <game/enhance/fixes102.h>
#include <game/lib/libc.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_hoverboard.h>
#include <game/ovl/shared_race_hud.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace rac1::oracle::race_hud {

// a level's own addresses (NTSC-U) of the two draws, their element's start / reset, and the points of the probes
struct Level {
    int level;
    uint32_t meter, bar, start, reset;
    uint32_t poses_jal, poses_at;  // the poses' sprintf (a2 the count) and its return address (hooks.glue's line)
    uint32_t flips_jal, flips_at;  // the flips' sprintf (a2 the trick's name, a3 the score) and its return address
    uint32_t place_at;             // the join after the place's sprintf (s1 the hero's block)
    uint32_t label_at;             // the return address of the label's sprintf (" %s:")
    std::vector<const char*> dumps;
};
inline const Level kRilgar{5, 0x00262f50, 0x00266320, 0x00262ae8, 0x002661a0, 0x002630d0, 0x002630d8, 0x00263348, 0x00263350,
                           0x00266528, 0x00266588, {"rilgar_0387/race1200.bin"}};
inline const Level kKalebo{16, 0x00235660, 0x00238a30, 0x002351f8, 0x002388b0, 0x002357e0, 0x002357e8, 0x00235a58, 0x00235a60,
                           0x00238c38, 0x00238c98, {"levels_15_16/race_1500_L16.bin", "levels_15_16/race_2500_L16.bin", "levels_15_16/race_3500_L16.bin"}};

using Check = std::function<void(const char*, uint32_t, int, Ret, const std::function<Args()>&, const std::function<uint64_t(const Args&)>&)>;
using Port = void (*)(uint32_t);

// a RAM dump (32 MB + the scratchpad), empty when missing
inline const std::vector<uint8_t>& rh_dump(const std::string& path) {
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
inline void rh_load(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}

// the port run with the PC version's settings on (fixes102_on reads RAC1_FIXES102 only then), off again after it
inline void with_settings(const std::function<void()>& f) {
    g_advanced_disabled = false;
    try { f(); } catch (...) { g_advanced_disabled = true; throw; }
    g_advanced_disabled = true;
}

// The state of one case: the element (its address returned) and the words the draw reads. `meter`: the meter's element and
// state `st` (0 .. 4), else the bar's.
inline uint32_t deal(const Level& L, const std::function<void()>& level, bool meter, int st) {
    const uint32_t P = 0x0013f350;
    const uint32_t race = 0x0015fad4;  // (the race's trick state: the same NTSC-U address on both levels)
    const uint32_t fn = meter ? L.meter : L.bar;
    uint32_t e = OV(0x0017dc50) + (meter ? 0u : 5u) * 0x90;
    bool from_dump = false;
    if (coin()) {
        const char* set = L.dumps[static_cast<size_t>(ri(0, static_cast<int>(L.dumps.size()) - 1))];
        const std::string path = dump_path(set);
        const std::vector<uint8_t>& d = rh_dump(path);
        if (!d.empty()) {
            rh_load(d);
            from_dump = true;
            for (uint32_t i = 0; i < 13; ++i)
                if (ee_read<uint32_t>(OV(0x0017dc50) + i * 0x90 + 0x18) == fn) e = OV(0x0017dc50) + i * 0x90;
        }
    }
    if (!from_dump) {
        level();
        (meter ? race_hud_start : race_hud_reset)(e);  // (the element set up as the race's request does)
        if (coin()) race_hud_update(e);
        if (ri(0, 3) == 0) w32(e + 0x60, ri(0, 2) == 0 ? 0u : coin() ? 4u : 8u);  // (its layout: centred, left of x, right of x)
    }
    // the element: the meter's fill and full value, its place, its fade, its state bytes
    if (!from_dump || coin()) {
        w32(e + 8, coin() ? u(ri(1, 2000)) : 1020u);
        w32(e + 0x74, u(ri(0, 2000)));
        if (coin()) { w32(e + 0x50, u(ri(0, 500))); w32(e + 0x54, u(ri(0, 400))); }
    }
    w32(e + 0x6c, ri(0, 4) == 0 ? u(ri(-6, 0)) : u(ri(1, 50)));
    if (meter) {
        w8(e + 0x70, ri(0, 3) == 0 ? u(ri(0, 12)) : u(ri(0, 255)));
        w8(e + 0x71, u(ri(0, 70)));
        w8(e + 0x72, u(st));
    }
    // the hero: on the board (his +0x208c 0x16) or not, the boost's message frames, the lap, the place, the bolt
    w32(P + 0x208c, ri(0, 5) == 0 ? u(ri(0, 0x20)) : 0x16u);
    w8(P + 0x8ae, coin() ? u(frame_scale_i(0x78) + ri(-2, 60)) : u(ri(0, 255)));
    w16(P + 0x89a, u(ri(-1, 5)));
    {
        const int k = ri(0, 9);
        w32(P + 0x8c0, k < 6 ? u(k + 1) : k == 6 ? 0u : k == 7 ? u(ri(-3, -1)) : k == 8 ? u(ri(7, 12)) : R());
    }
    w8(P + 0x8ce, coin() ? 0u : u(ri(1, 9)));
    if (ri(0, 3) == 0) w8(0x0013d388, u(ri(0, 1)));
    if (ri(0, 3) == 0) w32(0x0015ed80, u(ri(0, 1)));
    if (ri(0, 5) == 0) w32(0x0015ed84, coin() ? 0x10u : 5u);
    // the race's words: the score (in the tricks' units; 360 a flip's), the count, the four tricks' flags, the combos' history,
    // the points
    {
        const int k = ri(0, 7);
        w32(race, k == 0 ? 0x168u : k == 1 ? u(ri(0x166, 0x16a)) : k == 2 ? 1u : k == 3 ? u(ri(-2, 0)) : u(ri(0, 6)));
    }
    {
        const int k = ri(0, 9);
        w32(race + 4, k < 2 ? 1u : k < 4 ? 3u : k == 4 ? 4u : k == 5 ? 2u : k == 6 ? 0u : u(ri(5, 7)));
    }
    for (uint32_t k = 0; k < 4; ++k) w32(race + 0xc + 4 * k, coin() ? 0u : u(ri(1, 3)));
    for (uint32_t k = 0; k < 4; ++k) w32(race + 0x1c + 4 * k, coin() ? u(ri(0, 14)) : 0xffffffffu);
    w32(race + 0x2c, u(ri(0, 300)));
    if (meter && st == 3 && coin()) {  // (the history holding the combo the tricks give: shared points)
        const uint32_t c = ee_read<uint32_t>(race + 0x1c + 4 * u(ri(1, 3)));
        for (uint32_t k = 1; k < 4; ++k) if (coin()) w32(race + 0x1c + 4 * k, c);
    }
    w32(OV(0x0015f5cc), R() & 0xffff);
    if (ri(0, 3) == 0) { wf(0x0015ed68, 1.2f); wf(0x0015ed6c, 0.02f); wf(0x0015ed70, 0.0004f); }  // (50 Hz)
    // the packet cursor on the frame's own ring, below the skinning ring
    const uint32_t ring = ee_read<uint32_t>(OV(0x00173e04));
    w32(OV(0x001611c0), (ring != 0 ? ring : 0x00800000u) + u(ri(0, 0x40)) * 0x10);
    seed_rand();
    return e;
}

// the two draws against the original
inline void draws(const Level& L, const Check& check, const std::function<void()>& level, Port meter_port, Port bar_port) {
    const std::string mn = "race_meter_draw_l" + std::to_string(L.level), bn = "race_bar_draw_l" + std::to_string(L.level);
    check(mn.c_str(), L.meter, 400, Ret::None, [&L, level] {
        const int st = ri(0, 9) < 8 ? ri(0, 3) : 4;
        return Args{{deal(L, level, true, st)}, {}};
    }, [meter_port](A a) { meter_port(a.i[0]); return uint64_t{0}; });
    check(bn.c_str(), L.bar, 300, Ret::None, [&L, level] { return Args{{deal(L, level, false, 0)}, {}}; },
          [bar_port](A a) { bar_port(a.i[0]); return uint64_t{0}; });
}

// the probes that put fix 19's statements (hooks.glue's, as the generated copies ran them) into the original's run
inline std::vector<std::pair<uint32_t, std::function<void(EeCpu&)>>> fix19_probes(const Level& L, bool meter, bool settings) {
    static uint32_t a2 = 0, a3 = 0;
    auto run = [settings](const std::function<void()>& f) { if (settings) with_settings(f); else f(); };
    if (meter)
        return {{L.poses_jal, [](EeCpu& c) { a2 = c.r32(6); }},
                {L.poses_at, [run](EeCpu& c) { run([&c] { fixes102_race_text(c.r32(29), 0, 0, static_cast<int32_t>(a2)); }); }},
                {L.flips_jal, [](EeCpu& c) { a2 = c.r32(6); a3 = c.r32(7); }},
                {L.flips_at, [run](EeCpu& c) { run([&c] { fixes102_race_text(c.r32(29), 1, a2, static_cast<int32_t>(a3)); }); }}};
    return {{L.place_at, [run](EeCpu& c) {
        run([&c] { fixes102_race_text(c.r32(29), 2, 0, static_cast<int32_t>(ee_read<uint32_t>(c.r32(17) + 0x8c0))); });
    }}};
}

// fix 19 (RAC1_FIXES102=19): the original with the hooks.glue statements, the port with the fix on
inline void fix19(const Level& L, const Check& check, const std::function<void()>& level, Port meter_port, Port bar_port) {
    for (const bool meter : {true, false}) {
        const char* name = meter ? "race_fix19_meter" : "race_fix19_bar";
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) continue;
        if (std::getenv("RAC1_FIXES102") == nullptr) {
            std::printf("%-26s skipped (run with RAC1_FIXES102=19)\n", name);
            ++g_skipped;
            continue;
        }
        const ExtraProbes probes(fix19_probes(L, meter, true));
        const Port port = meter ? meter_port : bar_port;
        check(name, meter ? L.meter : L.bar, 300, Ret::None, [&L, level, meter] {
            return Args{{deal(L, level, meter, meter ? (ri(0, 2) == 0 ? 3 : ri(1, 2)) : 0)}, {}};
        }, [port](A a) { with_settings([&] { port(a.i[0]); }); return uint64_t{0}; });
    }
}

// a PAL disc's state (0x15ee90 set): the original with the PAL builds' lines at the same points, the port with its region fork
inline void pal(const Level& L, const Check& check, const std::function<void()>& level, Port meter_port, Port bar_port) {
    for (const bool meter : {true, false}) {
        const char* name = meter ? "race_pal_meter" : "race_pal_bar";
        auto probes = fix19_probes(L, meter, false);  // (fixes102_race_text writes on a PAL disc whatever the fix's row)
        if (!meter)  // the label: the PAL builds' " %s:  " where NTSC-U's is " %s:"
            probes.push_back({L.label_at, [](EeCpu& c) {
                const uint32_t b = c.r32(29);
                const uint32_t end = b + static_cast<uint32_t>(libc_strlen(b));
                w8(end, ' '); w8(end + 1, ' '); w8(end + 2, 0);
            }});
        const ExtraProbes on(std::move(probes));
        const Port port = meter ? meter_port : bar_port;
        check(name, meter ? L.meter : L.bar, 300, Ret::None, [&L, level, meter] {
            const uint32_t e = deal(L, level, meter, meter ? (ri(0, 2) == 0 ? 3 : ri(0, 3)) : 0);
            w32(0x0015ee90, 1u);
            return Args{{e}, {}};
        }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
    }
}

}  // namespace rac1::oracle::race_hud

#endif
