// Oracle batch 150 (task 0418; levels/gaspar-orxon keeps its Gaspar part only: Batalia's checks belong to the Batalia branch): the draw-list callbacks Batalia (8) and Gaspar (9) queue that the port had no function for, on RAM
// dumps of headless runs (assets/dumps/oracle/drawcb_0418/, skipped without them; ORACLE_LEVEL=8 / 9). Each dump is taken at a frame where
// the callback is in one of the three draw lists (Batalia's arrival cutscene ends at frame ~2700: RAC1_HERO_AT=<class>@2700):
//   scout_L8.bin         2800: RAC1_HERO_AT=252@2700 (the Scoutbots around the hero: 0x2d3878 queued, 0x2d4108 every frame);
//   flare_L8.bin         2800: RAC1_HERO_AT=600@2700,0,0,1,0 RAC1_SIM_INPUT=seq:2705-3100:rx=1 (class 600's point on screen);
//   flare1649_L8.bin     2760: RAC1_HERO_AT=1649@2700;
//   turret_L8.bin        2780: RAC1_HERO_AT=440@2700,0,0,0.5 RAC1_POKE8=2690:0x15c0d4=255,2705:0x1be75fc=1 (the turret manned:
//                        its +0xbc mode poked, see the task file);
//   teleport_out_L8.bin  2800 / teleport_in_L8.bin 2830: RAC1_HERO_AT=1135@2700,0,0,1 RAC1_SIM_INPUT=seq:2715+4:t;
//   visibomb_L8.bin      3050: RAC1_GIVE_FRAME=2690 RAC1_GIVE_ITEMS=0xd RAC1_EQUIP=0xd RAC1_SIM_INPUT=seq:2800+4:o;2810-3290:ly=1;
//   sandmouse_L8.bin     3100: RAC1_HERO_AT=604@2700 RAC1_GIVE_FRAME=2690 RAC1_GIVE_ITEMS=5 RAC1_POKE8=2690:0x14142c=5;
//   ships_L9.bin         1900: a plain run (class 317's list dc80 entry, both background ships' glows);
//   visibomb_L9.bin      1250: RAC1_GIVE_FRAME=1000 RAC1_GIVE_ITEMS=0xd RAC1_EQUIP=0xd RAC1_SIM_INPUT=seq:1100+4:o;1110-1490:ly=1.
// Every dump is RAC1_HEADLESS=1 RAC1_LEVEL=<n> RAC1_RAMHASH=/dev/null RAC1_RAMDUMP=<frame>,<file>. The argument of a check is one of
// the callback's own entries in the dumped lists; class 452's callback (L8:0x2e5188) is queued only by L8:0x2e3870, which nothing
// reaches (dead code in the shipped game), so it runs on a live class-452 moby of another dump.
#include "ovl_oracle.h"

#include <game/ovl/aridia_ocean.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/gaspar_mobys.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l9.h>
#include <runtime/ee/ee_interpreter.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {
const std::vector<uint8_t>& dc_dump(const std::string& path) {
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
void dc_load(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}
// the arguments the dumped draw lists hold for `fn` (the three lists: counts 0x15f424 / 0x15f428 / 0x15f42c, the (fn, arg) arrays at
// the level's own 0x16d880 / 0x16da80 / 0x16dc80)
std::vector<uint32_t> queued_args(uint32_t fn) {
    std::vector<uint32_t> out;
    const uint32_t counts[3] = {0x0015f424u, 0x0015f428u, 0x0015f42cu};
    const uint32_t lists[3] = {OV(0x0016d880), OV(0x0016da80), OV(0x0016dc80)};
    for (int k = 0; k < 3; ++k) {
        const uint32_t n = ee_read<uint32_t>(counts[k]);
        for (uint32_t i = 0; i < n && i < 64; ++i)
            if (ee_read<uint32_t>(lists[k] + i * 4) == fn) out.push_back(ee_read<uint32_t>(lists[k] + 0x100 + i * 4));
    }
    return out;
}
std::vector<uint32_t> live_mobys(int16_t cls) {
    std::vector<uint32_t> out;
    const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
    for (uint32_t m = first; m < first + 0x100 * 3000; m += 0x100)
        if (ee_read<int16_t>(m + 0xa6) == cls && static_cast<int8_t>(ee_read<uint8_t>(m + 0x20)) >= 0) out.push_back(m);
    return out;
}
bool is_moby(uint32_t a) {
    const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
    return a >= first && a < first + 0x100 * 3000 && ((a - first) & 0xff) == 0;
}
void cursor_jitter() { w32(OV(0x001611c0), ee_read<uint32_t>(OV(0x001611c0)) + u(ri(0, 0x40)) * 0x10); }

struct Cb {
    const char* name;
    int level;
    uint32_t at;          // the callback (the level's own address)
    const char* dump;     // assets/dumps/oracle/drawcb_0418/<dump>.bin
    int16_t cls;          // the class whose live moby stands in when the lists hold none (class 452's)
    void (*port)(uint32_t);
};
const Cb kCbs[] = {
    {"gas_class317_draw", 9, 0x002ef750, "ships_L9", 317, gas_class317_draw_l9},
    {"gas_class832_draw", 9, 0x002fb078, "visibomb_L9", 832, class832_draw},
    {"gas_class1212_draw", 9, 0x00305c10, "ships_L9", 1212, ari_bg_ship1_glow_draw_l2},
    {"gas_class1213_draw", 9, 0x00306570, "ships_L9", 1213, ari_bg_ship2_glow_draw_l2},
};
}  // namespace

void batch_150() {
    if (g_only_batch != 150) return;
    g_batch = 150;
    const int lv = g_oracle_level;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // (as in the runtime)
    for (const Cb& c : kCbs) {
        if (c.level != lv) {
            skip_check(c.name, c.at, lv == 8 || lv == 9 ? "another level's callback" : "the draw-list callbacks of task 0418: ORACLE_LEVEL=8 / 9");
            continue;
        }
        const std::vector<uint8_t>& d = dc_dump(dump_path("drawcb_0418/") + c.dump + ".bin");
        if (d.empty()) {
            skip_check(c.name, c.at, "no RAM dump (assets/dumps/oracle/drawcb_0418)");
            continue;
        }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, c.name) != 0) continue;
        std::printf("%-26s L%d:0x%08x\n", c.name, lv, c.at);
        const Cb* cp = &c;
        auto gen = [cp, &d] {
            dc_load(d);
            std::vector<uint32_t> args = queued_args(cp->at);
            if (args.empty()) args = live_mobys(cp->cls);
            const uint32_t a = args.empty() ? 0u : args[static_cast<size_t>(ri(0, static_cast<int>(args.size()) - 1))];
            if (is_moby(a) && coin()) {  // (the moby moved a little: its draw's screen position)
                for (uint32_t k = 0; k < 3; ++k) wf(a + 0x10 + k * 4, ee_read<float>(a + 0x10 + k * 4) + rf(-0.5f, 0.5f));
            }
            if (cp->cls == 832 && is_moby(a) && coin()) w32(ee_read<uint32_t>(a + 0x78) + 0x140, u(ri(0, 0x80)));  // (the static's counter)
            if (cp->cls == 317 && coin()) w32(0x0015f5cc, u(ri(0, 0x10000)));  // (the frame counter: the animated texture's frame)
            if (cp->cls == 1212 || cp->cls == 1213) {  // (the ships' engine glows: drawn while their colour's first byte, pvars +0xb0 (1212) / +0xe0 (1213), is past 0x80)
                for (uint32_t m : live_mobys(cp->cls)) {
                    if (coin()) w8(ee_read<uint32_t>(m + 0x78) + (cp->cls == 1212 ? 0xb0u : 0xe0u), u(ri(0x40, 0xff)));
                    if (coin()) w8(m + 0x31, 1);  // (a ship the callback skips while its +0x31 is clear)
                }
            }
            cursor_jitter();
            return Args{{a}, {}};
        };
        if (!check_fn(c.name, c.at, 30, Ret::None, gen, [cp](A a) { cp->port(a.i[0]); return uint64_t{0}; }, Scope::FullRam, 0, 0)) ++g_fail;
    }
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
