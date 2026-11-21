// Oracle batch 146 (task 0411): the Trespasser pad (class 615: Aridia, Eudora, Blarg, Pokitaru) and its puzzle, on RAM dumps of
// headless runs (assets/dumps/oracle/trespasser_0411/, skipped without them; ORACLE_LEVEL=2 / 4 / 6 / 11):
//   pad_L<n>.bin     the hero standing on the pad with the Trespasser in hand (RAC1_HERO_AT=615@1500,<dx>,<dy>,1.5: the pad's
//                    collision centre, Aridia 0,-0.84, Eudora 0.28,-0.78, Blarg 0.42,-0.72, Pokitaru -0.42,-0.72;
//                    RAC1_GIVE_ITEMS=0x1a RAC1_EQUIP=0x1a), the pad waiting in its state 1;
//   puzzle_L<n>.bin  the same run with circle held 1600-1640 (the Trespasser plugged in), frame 2000: the puzzle running.
// Checked: the hero's ground probe (the moby he stands on, 0x13f64c: what the pad's state 1 waits for), the pad's update (its
// states 1..3 with the buttons held / pressed varied) and the puzzle's draw-list callback (the rings' screen quads into the frame
// packet), each the level's own function.
#include "ovl_oracle.h"

#include <game/ovl/shared_class615.h>
#include <game/ovl/blarg_mobys.h>
#include <game/ovl/blarg_part2.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/batalia_mobys.h>
#include <game/ovl/variants_l8.h>
#include <runtime/ee/ee_interpreter.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <string>

namespace rac1 {
void hero_ground_update();
}

namespace rac1::oracle {

namespace {
const std::vector<uint8_t>& tp_dump(const std::string& path) {
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
// a check of the level's own function at `at` (not a Veldin address to translate)
void lvcheck(const char* name, uint32_t at, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    std::printf("%-26s L%d:0x%08x\n", name, g_oracle_level, at);
    if (!check_fn(name, at, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
}  // namespace

void batch_146() {
    if (g_only_batch != 146) return;
    g_batch = 146;
    const int lv = g_oracle_level;
    if (lv != 2 && lv != 4 && lv != 6 && lv != 8 && lv != 11) {
        for (const char* n : {"hero_ground_update", "trespasser_pad_update", "trespasser_puzzle_draw"})
            skip_check(n, 0x002127b8, "the Trespasser pad: Aridia / Eudora / Blarg / Batalia / Pokitaru only (ORACLE_LEVEL=2 / 4 / 6 / 8 / 11)");
        return;
    }
    const std::string sfx = "_L" + std::to_string(lv) + ".bin";
    const std::string dir = dump_path("trespasser_0411");
    const std::vector<uint8_t>* dumps[2] = {&tp_dump(dir + "/pad" + sfx), &tp_dump(dir + "/puzzle" + sfx)};
    if (dumps[0]->empty() || dumps[1]->empty()) {
        for (const char* n : {"hero_ground_update", "trespasser_pad_update", "trespasser_puzzle_draw"})
            skip_check(n, 0x002127b8, "no Trespasser pad RAM dumps (assets/dumps/oracle/trespasser_0411)");
        return;
    }
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // (as in the runtime)
    auto load = [&dumps](int which) {
        const std::vector<uint8_t>& d = *dumps[which];
        std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
        std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
    };
    const uint32_t upd = lv == 2 ? L2_ADDR(0x002d8ad0u) : lv == 4 ? L4_ADDR(0x002d4ae0u) : lv == 6 ? L6_ADDR(0x002e1940u) : lv == 8 ? L8_ADDR(0x002f2030u) : L11_ADDR(0x002fcda8u);
    const uint32_t draw = lv == 2 ? L2_ADDR(0x002d93e8u) : lv == 4 ? L4_ADDR(0x002d53f8u) : lv == 6 ? L6_ADDR(0x002e2258u) : lv == 8 ? L8_ADDR(0x002f2948u) : L11_ADDR(0x002fd6c0u);
    // the pad: the first moby of class 615 (live) in the dump; on Batalia (six of the class) the one the hero stands on (0x13f64c)
    auto pad = [lv] {
        if (lv == 8) return ee_read<uint32_t>(0x0013f64c);
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffdc)); m += 0x100)
            if (ee_read<int16_t>(m + 0xa6) == 615 && static_cast<int8_t>(ee_read<uint8_t>(m + 0x20)) >= 0) return m;
        return 0u;
    };
    // the hero a little off / on the pad (its collision is ~0.9 x 1.7: the hero at its edge stands on the floor around it)
    {
    // (stage 5, task C2c) hero_ground_update reaches segment_collide: A2's collision scratch
    // ranges left out of this check (see batch 43's checks of it)
    const CollisionScratchIgnored spr_ignored;
    check("hero_ground_update", 0x002127b8, 20, Ret::None, [load] {
        load(0);
        constexpr uint32_t P = g::PLAYER;
        for (uint32_t k = 0; k < 2; ++k) wf(P + 0x80 + 4 * k, ee_read<float>(P + 0x80 + 4 * k) + rf(-0.6f, 0.6f));
        wf(P + 0x88, ee_read<float>(P + 0x88) + rf(-0.05f, 0.5f));
        return Args{};
    }, [](A) { hero_ground_update(); return uint64_t{0}; });
    }
    // the pad's update: on the pad (state 1) or in the puzzle, the buttons held / pressed (0x13cae0 / 0x13cae4) varied
    lvcheck("trespasser_pad_update", upd, 60, Ret::None, [load, pad] {
        load(ri(0, 3) == 0 ? 0 : 1);
        if (coin()) w32(0x0013cae0, u(ri(0, 0xffff)));
        if (coin()) w32(0x0013cae4, u(ri(0, 0xffff)) & (ri(0, 1) ? 0xffffu : 0x00f0u));
        return Args{{pad()}, {}};
    }, [lv](A a) {
        if (lv == 2) class615_update(a.i[0]);
        else if (lv == 4) class615_update(a.i[0]);  // (Eudora: the shared port it is re-glued onto)
        else if (lv == 6) class615_update(a.i[0]);
        else if (lv == 8) class615_update(a.i[0]);
        else class615_update(a.i[0]);
        return uint64_t{0};
    });
    // the puzzle's draw (the pad's draw-list entry), the frame packet's cursor moved on a little (a draw later in the frame)
    lvcheck("trespasser_puzzle_draw", draw, 30, Ret::None, [load, pad] {
        load(1);
        w32(OV(0x001611c0), ee_read<uint32_t>(OV(0x001611c0)) + u(ri(0, 0x40)) * 0x10);
        return Args{{pad()}, {}};
    }, [lv](A a) {
        if (lv == 2) class615_puzzle_draw(a.i[0]);
        else if (lv == 4) class615_puzzle_draw(a.i[0]);  // (Eudora: the shared port it is re-glued onto)
        else if (lv == 6) class615_puzzle_draw(a.i[0]);
        else if (lv == 8) class615_puzzle_draw(a.i[0]);
        else class615_puzzle_draw(a.i[0]);
        return uint64_t{0};
    });
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
