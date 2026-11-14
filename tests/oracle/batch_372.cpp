// Oracle batch 372 (fix/gamebreaking, issue #54: Veldin's finale, Ratchet cannot surface at the far end of the underwater pipe): the
// hero's frame (hero_frame_update 0x2070d0, level 18's own build L18:0x20f898 and every callee: the ground probe, the water entry,
// the swim states) on RAM dumps of Ratchet diving and swimming back up in level 18's water.
// Level 18's water is no part of the level's collision: it is the collision of class 1432's mobys (flat planes of kind 0, met by the
// hero's ground probe through segment_collide 0x1efc70's moby part): one pool at z 88 (x 398..447 / y 724..767, its floor at 79), a
// shallow one at z 91.5 (x 554..618 / y 724..778, its floor at 91).
// Run with ORACLE_LEVEL=18 build/tests/ovl_oracle --batch 372. The dumps (skipped without them), from the repository's root, every run
// with RAC1_HEADLESS=1 RAC1_LEVEL=18 RAC1_RAMHASH=/dev/null RAC1_FRAMES=<f>+2 RAC1_RAMDUMP=<f>,assets/dumps/oracle/gamebreaking/
// swim_<f>_L18.bin, RAC1_HERO_AT='-1@800,413.8,740,96' (dropped into the deep pool) and RAC1_SIM_INPUT=seq: square every 12 frames
// from 860 to 999 (diving), then cross held from 1050 (swimming up), f = 980 (under water), 1100 (rising), 1160 (reaching the
// surface), 1200 (swimming on it).
// Each case loads a dump and varies the pad (the stick, cross, square, triangle, circle held / pressed), the frame counter, the seed,
// and in a third of the cases the water height the hero keeps (+0x2f0) about the pool's.
// The water's height is moved by the Hydrodisplacer (the hand moby's update, Veldin's 0x2df9a0 = L18:0x2eeba0, hydro_asm.cpp) used on
// the faucets (class 341): the deep pool drains (88 to 79.2), the shallow one fills (91.5 to 103.5). On RAM dumps of that sequence the
// Hydrodisplacer's update and every level-18 class alive are compared too:
//   hydro_<f>_L18.bin, f = 1050 (the deep pool draining), 1650 / 1750 (the shallow one filling): as above with RAC1_GIVE_FRAME=780
//     RAC1_GIVE_ITEMS=0x16,0xc,0xf,0x2,0x4 RAC1_EQUIP=0x16 RAC1_HERO_AT='341@800,0,0,1,3.14/-1@1400,558.5,743.3,105' (on the first
//     faucet, then on the second) and RAC1_SIM_INPUT=seq: circle at 900, 1500, 1560 and 1620.

#include "ovl_oracle.h"

#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/hydro.h>
#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/empty_class_updates.h>

#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {
const std::vector<uint8_t>& dump372(const std::string& path) {
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
// a check whose original is level 18's own function at `laddr` (as batch 271's)
void l18check372(const char* name, uint32_t laddr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    std::printf("%-26s L18:0x%08x\n", name, laddr);
    if (!check_fn(name, laddr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
void load372(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}
}  // namespace

void batch_372() {
    if (g_only_batch != 372) return;
    g_batch = 372;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // (the particle update's saved return address)
    g_ignored_words.push_back(OV(0x0016018c));
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    for (uint32_t k = 0; k < 0x30; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);  // (the segment records' VU0 registers, as batch 242)
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);  // (the pose function's spills, as batch 270)
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
    for (const char* f : {"980", "1100", "1160", "1200"}) {
        const std::string nm = std::string("hero_frame_swim_") + f;
        if (g_oracle_level != 18) { skip_check(nm.c_str(), 0x002070d0, "level 18's dumps: ORACLE_LEVEL=18"); continue; }
        const std::vector<uint8_t>& d = dump372(dump_path(std::string("gamebreaking/swim_") + f + "_L18.bin"));
        if (d.empty()) { skip_check(nm.c_str(), 0x002070d0, "no RAM dump (see the header)"); continue; }
        auto vary = [] {
            const uint32_t P = 0x0013c940;  // (the pad: held +0x1a0, pressed +0x1c4; cross 0x40, square 0x80, triangle 0x10, circle 0x20)
            static const uint32_t bt[] = {0x40, 0x80, 0x10, 0x20, 0x08, 0x04};
            if (ri(0, 3) == 0) { w32(P + 0x1a0, 0); w32(P + 0x1c4, 0); }
            for (int j = 0; j < 2; ++j)
                if (coin()) {
                    const uint32_t b = bt[ri(0, 5)];
                    w32(P + 0x1a0, ee_read<uint32_t>(P + 0x1a0) | b);
                    if (coin()) w32(P + 0x1c4, ee_read<uint32_t>(P + 0x1c4) | b);
                }
            if (coin()) { wf(P + 0x108, rf(-1, 1)); wf(P + 0x10c, rf(-1, 1)); }
            w32(OV(0x0015f5cc), ee_read<uint32_t>(OV(0x0015f5cc)) + u(ri(0, 63)));
            if (ri(0, 2) == 0) wf(0x0013f350 + 0x2f0, rf(84.0f, 93.0f));  // (the water height the hero keeps)
            seed_rand();
        };
        check(nm.c_str(), 0x002070d0, 40, Ret::None, [&d, vary] { load372(d); vary(); return Args{}; },
              [](A) { hero_frame_update(); return uint64_t{0}; });
    }
    // the Hydrodisplacer moving the pools' water, and every level-18 class alive there
    struct Cl { const char* name; uint32_t addr; void (*fn)(uint32_t); };
    static const Cl cls[] = {
            {"class41", 0x002a7220, vf_class41_update_l18},
            {"class49", 0x002a76e0, vf_class49_update_l18},
            {"class50", 0x002a7b90, vf_class50_update_l18},
            {"class256", 0x002aa3b0, vf_class256_update_l18},
            {"class1427", 0x002ab7c0, vf_class1427_update_l18},
            {"class1523", 0x002abaa0, vf_class1523_update_l18},
            {"class331", 0x002ce7d0, vf_class331_update_l18},
            {"class341", 0x002ce800, vf_class341_update_l18},
            {"class419", 0x002ceaa0, empty_update},
            {"class564", 0x002d5050, vf_class564_update_l18},
            {"class568", 0x002d5918, vf_class568_update_l18},
            {"class582", 0x002d62e8, vf_class582_update_l18},
            {"class583", 0x002d6600, vf_class583_update_l18},
            {"class586", 0x002d7b20, vf_class586_update_l18},
            {"class587", 0x002d82c0, vf_class587_update_l18},
            {"class605", 0x002d8ba8, vf_class605_update_l18},
            {"class615", 0x002d8fd0, vf_class615_update_l18},
            {"class624", 0x002db460, vf_class624_update_l18},
            {"class628", 0x002dc260, vf_class628_update_l18},
            {"class638", 0x002dc918, vf_class638_update_l18},
            {"class644", 0x002df608, vf_class644_update_l18},
            {"class696", 0x002dfda8, vf_class696_update_l18},
            {"class830", 0x002e1508, vf_class830_update_l18},
            {"class758", 0x002e19d8, vf_class758_update_l18},
            {"class805", 0x002e1f40, vf_class805_update_l18},
            {"class832", 0x002e4168, vf_class832_update_l18},
            {"class885", 0x002e9768, vf_class885_update_l18},
            {"class983", 0x002e9e70, vf_class983_update_l18},
            {"class1021", 0x002ec3e8, vf_class1021_update_l18},
            {"class1039", 0x002ec858, vf_class1039_update_l18},
            {"class1134", 0x002ecc40, vf_class1134_update_l18},
            {"class1135", 0x002edb78, vf_class1135_update_l18},
            {"class1355", 0x002efb88, vf_class1355_update_l18},
            {"class1356", 0x002f0920, vf_class1356_update_l18},
            {"class1381", 0x002f16f0, vf_class1381_update_l18},
            {"class1392", 0x002f1e68, vf_class1392_update_l18},
            {"class1402", 0x002f2310, vf_class1402_update_l18},
            {"class1422", 0x002f2bf0, vf_class1422_update_l18},
            {"class1432", 0x002f7ab0, vf_class1432_update_l18},
            {"class1434", 0x002f7ad8, vf_class1434_update_l18},
            {"class1454", 0x002f7c40, vf_class1454_update_l18},
            {"class1563", 0x002f88e8, vf_class1563_update_l18},
            {"class1568", 0x002fa640, vf_class1568_update_l18},
            {"class1584", 0x002fa728, vf_class1584_update_l18},
            {"class1750", 0x002fad08, vf_class1750_update_l18},
            {"class1799", 0x002fad28, vf_class1799_update_l18},
            {"class1890", 0x002fae48, vf_class1890_update_l18},
            {"class1898", 0x002fb548, vf_class1898_update_l18},
            {"class1899", 0x002fb868, vf_class1899_update_l18},
            {"class1906", 0x002fbb98, vf_class1906_update_l18},
    };
    for (const char* f : {"1050", "1650", "1750"}) {
        const std::string tag = std::string("hydro_") + f;
        if (g_oracle_level != 18) { skip_check(tag.c_str(), 0x002df9a0, "level 18's dumps: ORACLE_LEVEL=18"); continue; }
        const std::vector<uint8_t>& d = dump372(dump_path(std::string("gamebreaking/hydro_") + f + "_L18.bin"));
        if (d.empty()) { skip_check(tag.c_str(), 0x002df9a0, "no RAM dump (see the header)"); continue; }
        load372(d);
        std::vector<uint32_t> hand;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<int16_t>(m + 0xa6) == 1251) hand.push_back(m);
        if (!hand.empty()) {
            const std::string nm = "hydro_update_" + std::string(f);
            check(nm.c_str(), 0x002df9a0, 40, Ret::None, [&d, hand] {
                load372(d);
                const uint32_t P = 0x0013c940;
                if (coin()) { w32(P + 0x1a0, ee_read<uint32_t>(P + 0x1a0) | 0x20); if (coin()) w32(P + 0x1c4, ee_read<uint32_t>(P + 0x1c4) | 0x20); }
                w32(OV(0x0015f5cc), ee_read<uint32_t>(OV(0x0015f5cc)) + u(ri(0, 63)));
                seed_rand();
                return Args{{hand[0]}, {}};
            }, [](A a) { hydro_update(a.i[0]); return uint64_t{0}; });
        }
        for (const Cl& c : cls) {
            std::vector<uint32_t> ms;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == c.addr) ms.push_back(m);
            if (ms.empty() || c.addr == 0x002f2bf0) continue;  // (the boss: batch 271)
            const std::string nm = std::string(c.name) + "_" + tag;
            l18check372(nm.c_str(), c.addr, 8, Ret::None, [&d, ms] {
                load372(d);
                const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                w32(OV(0x0015f5cc), ee_read<uint32_t>(OV(0x0015f5cc)) + u(ri(0, 63)));
                seed_rand();
                return Args{{m}, {}};
            }, [c](A a) { c.fn(a.i[0]); return uint64_t{0}; });
            load372(d);
        }
    }
    g_extra_noop_hooks.clear();
    g_ee_null_is_ram = null_ram;
    g_ignored_words = saved_ignored;
}

}  // namespace rac1::oracle
