// Oracle batch 147 (task 0413): the draw-list callbacks the levels queued and the port had no function for, on RAM dumps of
// headless runs (assets/dumps/oracle/drawcb_0413/, skipped without them; ORACLE_LEVEL=2 / 3 / 4):
//   visibomb_L<n>.bin   frame 1040: a Visibomb (0xd) fired at 900 and pulled up from 910 (RAC1_SIM_INPUT=seq:900+4:o;910-1290:ly=1),
//                       the hero in state 0x1d; on Aridia / Eudora the missile is out of class 832's volumes (its counter
//                       pvars +0x140 running: the static drawn), on Kerwan still inside them (counter 0: varied below);
//   sandmouse_L<n>.bin  the Sandmouse (1818) out of its house and flying (state 5): the hero in its box wearing item 5
//                       (RAC1_GIVE_ITEMS=5, RAC1_POKE8=<f>:0x14142c=5; Aridia frame 1990, Kerwan / Eudora frame 1300);
//   thruster_L<n>.bin   (n = 2, 4) the hero with the Thruster-Pack (pack 3: RAC1_POKE8 0x141430 = 3 and the pack moby pointer
//                       0x1404d0 cleared, so hero_update spawns pack 3 and its two class-167 jets) jumping, x held.
// Checked: class 832's update (the level's own) and its draw-list callback (the static over the Visibomb's view), the
// Sandmouse's draw-list callback (its two glows), class 167's update and draw-list callback (Veldin's code at 0x2b4198 /
// 0x2b3628, every level's copy: the Thruster-Pack's jets).
#include "ovl_oracle.h"

#include <game/ovl/aridia_classes_tr.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/kerwan_classes_tr.h>
#include <game/ovl/shared_classes_tr.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l3.h>
#include <game/ovl/variants_l4.h>
#include <runtime/ee/ee_interpreter.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {
const std::vector<uint8_t>& dl_dump(const std::string& path) {
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
void dl_load(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}
// the live mobys of a class (the placed ones and those spawned after them: from the first moby on)
std::vector<uint32_t> mobys_of(int16_t cls) {
    std::vector<uint32_t> out;
    const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
    for (uint32_t m = first; m < first + 0x100 * 3000; m += 0x100)
        if (ee_read<int16_t>(m + 0xa6) == cls && static_cast<int8_t>(ee_read<uint8_t>(m + 0x20)) >= 0) out.push_back(m);
    return out;
}
// a check of the level's own function at `at` (not a Veldin address to translate)
void lvcheck(const char* name, uint32_t at, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    std::printf("%-26s L%d:0x%08x\n", name, g_oracle_level, at);
    if (!check_fn(name, at, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
// the frame packet's cursor moved on a little (a draw later in the frame)
void cursor_jitter() { w32(OV(0x001611c0), ee_read<uint32_t>(OV(0x001611c0)) + u(ri(0, 0x40)) * 0x10); }
}  // namespace

void batch_147() {
    if (g_only_batch != 147) return;
    g_batch = 147;
    const int lv = g_oracle_level;
    const char* names[] = {"class832_update", "class832_draw", "sandmouse_glow", "class167_update", "class167_draw"};
    if (lv != 2 && lv != 3 && lv != 4) {
        for (const char* n : names) skip_check(n, 0x002b3628, "the draw-list callbacks of task 0413: ORACLE_LEVEL=2 / 3 / 4");
        return;
    }
    const std::string dir = dump_path("drawcb_0413/"), sfx = "_L" + std::to_string(lv) + ".bin";
    const std::vector<uint8_t>& vb = dl_dump(dir + "visibomb" + sfx);
    const std::vector<uint8_t>& sm = dl_dump(dir + "sandmouse" + sfx);
    const std::vector<uint8_t>& th = dl_dump(dir + "thruster" + sfx);  // (Aridia, Eudora)
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // (as in the runtime)

    // class 832: its update (the Visibomb's volumes, the counter, the draw queued, the missile blown at the end) and its draw
    if (vb.empty()) {
        for (int k = 0; k < 2; ++k) skip_check(names[k], 0x002b3628, "no Visibomb RAM dump (assets/dumps/oracle/drawcb_0413)");
    } else {
        const uint32_t upd = lv == 2 ? L2_ADDR(0x002e4a48u) : lv == 3 ? L3_ADDR(0x002d40e8u) : L4_ADDR(0x002dc168u);
        const uint32_t drw = lv == 2 ? L2_ADDR(0x002e4838u) : lv == 3 ? L3_ADDR(0x002d3ed8u) : L4_ADDR(0x002dbf58u);
        auto gen = [&vb] {
            dl_load(vb);
            const std::vector<uint32_t> ms = mobys_of(832);
            const uint32_t m = ms.empty() ? 0u : ms[0];
            if (m != 0 && coin()) w32(ee_read<uint32_t>(m + 0x78) + 0x140, u(ri(0, 0x80)));  // (the out-of-range counter)
            cursor_jitter();
            return Args{{m}, {}};
        };
        lvcheck("class832_update", upd, 30, Ret::None, gen, [lv](A a) {
            if (lv == 2) class832_update(a.i[0]);
            else if (lv == 3) class832_update(a.i[0]);
            else class832_update(a.i[0]);  // (Eudora: the shared port it is re-glued onto)
            return uint64_t{0};
        });
        lvcheck("class832_draw", drw, 40, Ret::None, gen, [lv](A a) {
            if (lv == 2) class832_draw(a.i[0]);
            else if (lv == 3) class832_draw(a.i[0]);
            else class832_draw(a.i[0]);  // (Eudora: the shared port it is re-glued onto)
            return uint64_t{0};
        });
    }

    // the Sandmouse's two glows (chain points 3 / 4 into its pvars +0x70 by its update), the moby moved a little
    if (sm.empty()) {
        skip_check(names[2], 0x002b3628, "no Sandmouse RAM dump (assets/dumps/oracle/drawcb_0413)");
    } else {
        const uint32_t drw = lv == 2 ? L2_ADDR(0x002f0eb0u) : lv == 3 ? L3_ADDR(0x002e1898u) : L4_ADDR(0x002e68f8u);
        lvcheck("sandmouse_glow", drw, 30, Ret::None, [&sm] {
            dl_load(sm);
            const std::vector<uint32_t> ms = mobys_of(1818);
            const uint32_t m = ms.empty() ? 0u : ms[0];
            if (m != 0 && coin()) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                for (uint32_t b : {0x70u, 0x80u})  // (the two glows' points)
                    for (uint32_t k = 0; k < 3; ++k) wf(pv + b + k * 4, ee_read<float>(pv + b + k * 4) + rf(-0.5f, 0.5f));
            }
            cursor_jitter();
            return Args{{m}, {}};
        }, [lv](A a) {
            if (lv == 2) sandmouse_glow(a.i[0]);
            else if (lv == 3) sandmouse_glow(a.i[0]);
            else sandmouse_glow(a.i[0]);  // (Eudora: the shared port it is re-glued onto)
            return uint64_t{0};
        });
    }

    // class 167 (Veldin's code: the level's copy run, the port at Veldin's address), either jet, the buttons held varied
    if (th.empty()) {
        for (int k = 3; k < 5; ++k) skip_check(names[k], 0x002b3628, "no Thruster-Pack RAM dump (assets/dumps/oracle/drawcb_0413: Aridia, Eudora)");
    } else {
        auto gen = [&th] {
            dl_load(th);
            const std::vector<uint32_t> ms = mobys_of(167);
            const uint32_t m = ms.empty() ? 0u : ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            if (coin()) w32(0x0013cae0, u(ri(0, 0xffff)));
            // the hero's state (P+0x2084: 0xb..0xe, 0x10, 0x1c, 0x22, 0x81 the values the update tests), its flag +0x41e and the
            // float +0xaa8 (6..16 a range it tests), the jet's state byte +0x20 (1..3), the pack (P+0x11a8: 3, or 2: the jet idle)
            constexpr uint32_t P = g::PLAYER;
            static const uint32_t states[] = {0xb, 0xc, 0xd, 0xe, 0x10, 0x1c, 0x22, 0x81, 0, 1, 2, 3};
            if (ri(0, 3) != 0) w32(P + 0x2084, states[ri(0, 11)]);
            if (coin()) w16(P + 0x41e, u(ri(0, 1)));
            if (coin()) wf(P + 0xaa8, rf(0.0f, 20.0f));
            if (m != 0 && coin()) w8(m + 0x20, u(ri(0, 3)));
            if (ri(0, 7) == 0) w32(P + 0x11a8, 2);
            cursor_jitter();
            return Args{{m}, {}};
        };
        check("class167_update", 0x002b4198, 80, Ret::None, gen, [](A a) { class167_update_tr(a.i[0]); return uint64_t{0}; });
        check("class167_draw", 0x002b3628, 40, Ret::None, gen, [](A a) { class167_draw_tr(a.i[0]); return uint64_t{0}; });
    }
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
