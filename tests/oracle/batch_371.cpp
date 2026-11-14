// Oracle batch 371 (fix/gamebreaking, issue #49: the Kalebo III hoverboard race won in 2nd place): Kalebo III's own class updates
// (kalebo_classes_tr_*.cpp, level 16's addresses) on RAM dumps of the race, the racers in their laps and after their finish, as the
// game left them. Batch 250 proves the classes at the level's start and batch 242 the hero's race code on the race's first lap; the
// racers' and the race official's states at the race's end were never compared.
// Run with ORACLE_LEVEL=16 build/tests/ovl_oracle --batch 371. The dumps (skipped without them), from the repository's root, every run
// with RAC1_HEADLESS=1 RAC1_LEVEL=16 RAC1_GIVE_ITEMS=30 RAC1_HERO_AT=439@740 RAC1_RAMHASH=/dev/null RAC1_FRAMES=<f>+2
// RAC1_RAMDUMP=<f>,<file> and RAC1_SIM_INPUT=seq: batch 242's race script (circle at frames 760, 790, .. 1600 and cross 15 frames
// after each, then from 1700 the stick forward with cross every 60 frames):
//   levels_15_16/race_<f>_L16.bin, f = 1500, 2500, 3500: batch 242's (lap 1);
//   gamebreaking/racefinish_<f>_L16.bin, f = 3420, 4350: also RAC1_POKE8=2000:0x13faf0=2,2000:0x13faf2=2,2000:0x13faf4=2,
//     2000:0x13faf6=2,2000:0x13faf8=2 (the five racers' laps set to 2 at frame 2000: they finish the race from frame ~3400, the hero
//     still on his first lap).
// Each case loads a dump, takes one live moby of the class, moves the hero about it (or not), varies the frame counter and the seed.

#include "ovl_oracle.h"

#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>

#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {
// A check whose original is level `lvl`'s own function at `laddr` (as batch 271's)
void lcheck371(int lvl, const char* name, uint32_t laddr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    std::printf("%-26s L%d:0x%08x\n", name, lvl, laddr);
    if (!check_fn(name, laddr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
const std::vector<uint8_t>& dump371(const std::string& path) {
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
void load371(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}
}  // namespace

void batch_371() {
    if (g_only_batch != 371) return;
    g_batch = 371;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // (the particle update's saved return address)
    g_ignored_words.push_back(OV(0x0016018c));
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);  // (the pose function's spills, as batch 270)
    g_extra_noop_hooks.clear();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    struct Cl { const char* name; uint32_t addr; void (*fn)(uint32_t); };
    static const Cl cls[] = {
            {"class49", 0x002a05e8, kal_class49_update_l16},
            {"class50", 0x002a0a98, kal_class50_update_l16},
            {"class52", 0x002a1088, kal_class52_update_l16},
            {"class1475", 0x002a3f38, kal_class1475_update_l16},
            {"class235", 0x002c3be8, kal_class235_update_l16},
            {"class257", 0x002c3d38, kal_class257_update_l16},
            {"class281", 0x002c5eb0, kal_class281_update_l16},
            {"class439", 0x002c7788, kal_class439_update_l16},
            {"class470", 0x002c9480, kal_class470_update_l16},
            {"class471", 0x002c9878, kal_class471_update_l16},
            {"class482", 0x002cb600, kal_class482_update_l16},
            {"class541", 0x002cddb8, kal_class541_update_l16},
            {"class546", 0x002cf198, kal_class546_update_l16},
            {"class552", 0x002cf4a8, kal_class552_update_l16},
            {"class556", 0x002d04a0, kal_class556_update_l16},
            {"class605", 0x002d2a80, kal_class605_update_l16},
            {"class638", 0x002d2cf0, kal_class638_update_l16},
            {"class647", 0x002d59e0, kal_class647_update_l16},
            {"class650", 0x002d5cb8, kal_class650_update_l16},
            {"class654", 0x002d5ef8, kal_class654_update_l16},
            {"class758", 0x002d6378, kal_class758_update_l16},
            {"class800", 0x002d6740, kal_class800_update_l16},
            {"class805", 0x002d69e0, kal_class805_update_l16},
            {"class876", 0x002dddd8, empty_update},
            {"class933", 0x002ddde0, kal_class933_update_l16},
            {"class1111", 0x002dee10, kal_class1111_update_l16},
            {"class1134", 0x002df1e8, kal_class1134_update_l16},
            {"class1135", 0x002e0120, kal_class1135_update_l16},
            {"class1139", 0x002e0f78, kal_class1139_update_l16},
            {"class1140", 0x002e1080, empty_update},
            {"class1356", 0x002e22d8, kal_class1356_update_l16},
            {"class1377", 0x002e3190, kal_class1377_update_l16},
            {"class1387", 0x002e36e8, kal_class1387_update_l16},
            {"class1401", 0x002e37a0, kal_class1401_update_l16},
            {"class1410", 0x002e43e0, kal_class1410_update_l16},
            {"class1439", 0x002e5010, kal_class1439_update_l16},
            {"class1441", 0x002e5258, kal_class1441_update_l16},
            {"class1442", 0x002e54a0, kal_class1442_update_l16},
            {"class1443", 0x002e5708, kal_class1443_update_l16},
            {"class1445", 0x002e5e08, kal_class1445_update_l16},
            {"class1455", 0x002e6808, kal_class1455_update_l16},
            {"class1561", 0x002e7208, kal_class1561_update_l16},
            {"class1667", 0x002e76b8, kal_class1667_update_l16},
            {"class1812", 0x002e7e00, kal_class1812_update_l16},
            {"class1826", 0x002e7f80, kal_class1826_update_l16},
            {"class1891", 0x002e88d0, kal_class1891_update_l16},
            {"class1923", 0x002e8970, kal_class1923_update_l16},
            {"class1943", 0x002e8e80, kal_class1943_update_l16},
            {"class1947", 0x002e93b0, kal_class1947_update_l16},
            {"class1948", 0x002e98e0, kal_class1948_update_l16},
            {"class1952", 0x002e9df0, kal_class1952_update_l16},
            {"class1953", 0x002ea128, kal_class1953_update_l16},
    };
    auto pick = [](const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; };
    const char* dumps[] = {"levels_15_16/race_1500_L16", "levels_15_16/race_2500_L16", "levels_15_16/race_3500_L16",
                           "gamebreaking/racefinish_3420_L16", "gamebreaking/racefinish_4350_L16"};
    for (const char* f : dumps) {
        const std::string tag = std::string(std::strrchr(f, '/') + 1);
        if (g_oracle_level != 16) { skip_check(("kalebo_classes " + tag).c_str(), 0, "level 16's dumps: ORACLE_LEVEL=16"); continue; }
        const std::vector<uint8_t>& d = dump371(dump_path(std::string(f) + ".bin"));
        if (d.empty()) { skip_check(("kalebo_classes " + tag).c_str(), 0, "no RAM dump (see the header)"); continue; }
        load371(d);
        for (const Cl& c : cls) {
            std::vector<uint32_t> ms;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == c.addr) ms.push_back(m);
            if (ms.empty()) continue;
            const std::string nm = std::string(c.name) + "_" + tag;
            lcheck371(16, nm.c_str(), c.addr, 12, Ret::None, [&d, ms, pick] {
                load371(d);
                const uint32_t m = pick(ms);
                if (ri(0, 3) == 0) {
                    Vec4 q = ee_ref<Vec4>(m + 0x10);
                    const float r = coin() ? rf(0, 6) : rf(6, 40), a = rf(-3.1f, 3.1f);
                    q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-2, 4);
                    ee_ref<Vec4>(0x0013f3d0) = q;
                }
                if (coin()) w32(OV(0x0015f5cc), ee_read<uint32_t>(OV(0x0015f5cc)) + u(ri(0, 63)));
                seed_rand();
                return Args{{m}, {}};
            }, [c](A a) { c.fn(a.i[0]); return uint64_t{0}; });
            load371(d);
        }
    }
    g_ee_null_is_ram = null_ram;
    g_ignored_words = saved_ignored;
}

}  // namespace rac1::oracle
