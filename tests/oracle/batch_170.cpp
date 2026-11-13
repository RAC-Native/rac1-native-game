// Oracle batch 170 (levels/gaspar-orxon, release 0.2.0 levels): class 1290's update (Wrench's "Pilot's Helmet": the item pickup
// that gives item 7 on Gaspar), Veldin 0x2e03a0 (L9:0x308818), ported at Veldin's addresses (assets/generated/src/game/ovl/helmet_tr.cpp,
// generated from tools/codegen/gen/mkall/veldin_helmet.json in rac1-native-game). ORACLE_LEVEL=9, on RAM dumps of headless runs
// of the port (assets/dumps/oracle/gaspar_orxon/, skipped without them):
//   helmet762_L9.bin  RAC1_HEADLESS=1 RAC1_LEVEL=9 RAC1_HERO_AT=1290@760,2,0,1 RAC1_RAMHASH=/dev/null RAC1_RAMDUMP=762,<file>
//                     (the hero next to the helmet, before the pickup);
//   helmet900_L9.bin  the same, RAC1_RAMDUMP=900 (the pickup's scene running, game mode 2).
// Each case takes one live class-1290 moby and varies what the update reads: its state byte (+0x20: 0 .. 3 and beyond), the game
// mode 0x15f5c4, the hero's position against the moby's (the 2D distance < 3 and the height < 2 tests), the hero's health 0x1415f8
// (the pickup only when non-zero; the scene's start falls to hero_lost without it), the item-7 byte 0x13d4c7, 0x141b08, the pvars'
// two moby indices (+0 / +4, -1 or a moby), the moby's +0xb0 byte / +0xb2 index.
// Audit 2 (2026-11-13): the placed pickup chosen three times in four, mostly in its state 1, at the edges of its reach, its
// sparkles' angles near their wrap and their timers about to run out (state 1 was 5 cases of 160 before).

#include "ovl_oracle.h"

#include <game/lib/libcdvd.h>
#include <game/ovl/helmet_tr.h>
#include <game/ovl/level_change.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/ee/ee_interpreter.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {
const std::vector<uint8_t>& go_dump(const std::string& path) {
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
void go_load(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}
std::vector<uint32_t> go_live_mobys(int16_t cls) {
    std::vector<uint32_t> out;
    const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
    for (uint32_t m = first; m < first + 0x100 * 3000; m += 0x100)
        if (ee_read<int16_t>(m + 0xa6) == cls && static_cast<int8_t>(ee_read<uint8_t>(m + 0x20)) >= 0) out.push_back(m);
    return out;
}
}  // namespace

void batch_170() {
    if (g_only_batch != 170) return;
    g_batch = 170;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // (as in the runtime)
    // state 3's autosave (level_change_request 0x24d430): the console clock and its conversion to local time (sceCdReadClock 0x121780,
    // 0x12d6d8: IOP calls whose wait the interpreter's original polls forever at 0x11ba40) the host's on both sides, as batches 75 / 138
    const std::time_t saved_clock = g_host_clock_fixed;
    g_host_clock_fixed = 1759658400;
    g_extra_hooks = {{0x00121780, [](EeCpu& c) { c.set32(2, cd_read_clock(c.r32(4))); }},
                     {0x0012d6d8, [](EeCpu& c) { clock_to_local(c.r32(4)); }},
                     {0x00121450, [](EeCpu& c) { c.set32(2, cd_read(c.r32(4), c.r32(5), c.r32(6), c.r32(7))); }}};
    const char* dumps[] = {"helmet762_L9", "helmet900_L9"};
    for (const char* dn : dumps) {
        const std::string name = std::string("helmet_update ") + dn;
        if (g_oracle_level != 9) {
            skip_check(name.c_str(), 0x002e03a0, "class 1290's dumps are Gaspar's: ORACLE_LEVEL=9");
            continue;
        }
        const std::vector<uint8_t>& d = go_dump(dump_path("gaspar_orxon/") + dn + ".bin");
        if (d.empty()) {
            skip_check(name.c_str(), 0x002e03a0, "no RAM dump (assets/dumps/oracle/gaspar_orxon)");
            continue;
        }
        auto gen = [&d] {
            go_load(d);
            const std::vector<uint32_t> ms = go_live_mobys(1290);
            // the subjects: the placed pickup (its pvars in the level's pvars block) in any state; a class-1290 moby whose pvars pointer is
            // not one (the helmet the hero wears, spawned by hero_update from the item table: its "pvars" are another struct's words) in
            // its own state only
            uint32_t m = ms.empty() ? 0u : ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            // (the placed pickup three times in four: before, half of the cases were the worn helmet, in its state 0 alone)
            if (ri(0, 3) != 0)
                for (const uint32_t c : ms)
                    if (ee_read<uint32_t>(c + 0x78) >= 0x01000000u && ee_read<uint32_t>(c + 0x78) < 0x02000000u) m = c;
            if (m == 0) return Args{{0}, {}};
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const bool placed = pv >= 0x01000000u && pv < 0x02000000u;
            if (placed && coin()) w8(m + 0x20, ri(0, 2) == 0 ? u(ri(0, 4)) : 1u);  // (state 1, the sparkles and the reach, most)
            const int gm = ri(0, 3);
            if (gm == 1) w32(OV(0x0015f5c4), 0); else if (gm == 2) w32(OV(0x0015f5c4), 2); else if (gm == 3) w32(OV(0x0015f5c4), u(ri(-1, 7)));
            if (coin()) {  // (the moby moved around the hero: inside / outside the pickup's reach)
                for (uint32_t k = 0; k < 2; ++k) wf(m + 0x10 + k * 4, ee_read<float>(0x0013f3d0 + k * 4) + rf(-4.0f, 4.0f));
                wf(m + 0x18, ee_read<float>(0x0013f3d8) + rf(-3.0f, 3.0f));
                if (coin()) {  // (or at the reach's edges: 3 across, 2 up / down)
                    const float r = rf(2.6f, 3.4f), a = rf(-3.1f, 3.1f);
                    wf(m + 0x10, ee_read<float>(0x0013f3d0) + r * std::cos(a));
                    wf(m + 0x14, ee_read<float>(0x0013f3d4) + r * std::sin(a));
                    wf(m + 0x18, ee_read<float>(0x0013f3d8) + (coin() ? rf(-1.9f, 1.9f) : rf(-2.4f, 2.4f)));
                }
            }
            if (placed && coin()) {  // (the sparkles' four angles near the wrap at 0 / 255 and their timers about to run out)
                for (uint32_t k = 0; k < 4; ++k) {
                    if (coin()) wf(pv + 0x10 + 4 * k, coin() ? rf(252.0f, 255.0f) : rf(0.0f, 3.0f));
                    if (coin()) w32(pv + 0x30 + 4 * k, u(ri(0, 2)));
                }
            }
            if (ri(0, 3) == 0) w32(0x001415f8, 0);
            if (coin()) w8(0x0013d4c7, u(ri(0, 1)));
            if (coin()) w16(0x00141b08, u(ri(0, 1)));
            if (placed && coin()) {
                const uint32_t n = static_cast<uint32_t>(ee_read<int32_t>(OV(0x0015ffe0)) - static_cast<int32_t>(ee_read<uint32_t>(OV(0x0015ffd8)))) >> 8;
                if (coin()) w32(pv, 0xffffffffu);  // (+0: the pickup's scene, -1 = none; else the class's own: an index of the scene table)
                w32(pv + 4, coin() ? 0xffffffffu : u(ri(0, static_cast<int>(n > 0 ? n - 1 : 0))));  // (+4: a moby index)
            }
            if (placed && coin()) w8(m + 0xb0, coin() ? 0xffu : u(ri(0, 15)));
            if (coin()) wf(m + 0x48, rf(-7.0f, 7.0f));
            // (state 1 with the hero alive starts the pickup's scene (cutscene_start, batch 80) when in reach: its stream read and the music
            // streams' frame wait for the IOP, which the oracle's host SIF does not answer -- the original would wait forever. Such a case
            // gets no scene (+0 = -1: the same path without the scene start); the scene's start was checked in the game, helmet2 tour)
            if (ee_read<uint8_t>(m + 0x20) == 1 && ee_read<uint32_t>(0x001415f8) != 0 && ee_read<int32_t>(placed ? pv : 0x0013f350u) != -1) {
                if (placed) w32(pv, 0xffffffffu);
            }
            return Args{{m}, {}};
        };
        check(name.c_str(), 0x002e03a0, 80, Ret::None, gen, [](A a) { if (a.i[0] != 0) helmet_update_tr(a.i[0]); return uint64_t{0}; });
    }
    g_extra_hooks.clear();
    g_host_clock_fixed = saved_clock;
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
