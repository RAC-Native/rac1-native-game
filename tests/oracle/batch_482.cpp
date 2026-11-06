// Oracle batch 482: the moby drop shadows' VU0 routines written by hand (src/game/ovl/moby_shadows_hand.cpp), against each level's
// own original (its overlay map), on the RAM dumps of batch 480 (world_cull/: levels 0, 3, 5, 6, 11, 12 and 18, see
// batch_480.cpp's header for the command lines). Skipped without the dumps.
//  - shadow_points 0x285a68 on the frame's deferred mobys (the list the draw loop left: their joints, their classes' shadow lists);
//  - drop_shadows_draw 0x285870 after it (its point / segment tubes 0x2860a8 / 0x2861f8, the clip 0x286848, the quads 0x285d30, the
//    packets 0x2868c0, all through the hand-written routines): the list as made, then with the mobys' light directions (+0x10) and
//    clip flags (+4) varied.
// Batch 79 checks shadow_points and drop_shadows_draw on Veldin's own mobys too.

#include "ovl_oracle.h"

#include <game/ovl/moby_shadows.h>
#include <runtime/vu/vu0.h>

#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {
const std::vector<uint8_t>& dump482(const std::string& path) {
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
    std::memcpy(ee_get_scratchpad_base(), d.data() + 0x02000000, 0x4000);
}
// the interpreter's VU0 as the port's (the routines read lanes their callers left)
void vu0_to_interpreter(EeInterpreter& ee) {
    const VUState& p = vu0::global().st;
    ee.vu0().vf = p.vf;
    ee.vu0().vi = p.vi;
    ee.vu0().acc = p.acc;
    ee.vu0().q = p.q;
    ee.vu0().i = p.i;
    ee.vu0().mem = p.mem;
}
}  // namespace

void batch_482() {
    if (g_only_batch != 482) return;
    g_batch = 482;
    std::vector<std::string> dumps;
    for (int k = 0; k < 4; ++k) {
        const std::string p = dump_path("world_cull/l" + std::to_string(g_oracle_level) + "_" + std::to_string(k) + ".bin");
        if (!dump482(p).empty()) dumps.push_back(p);
    }
    if (dumps.empty()) {
        const char* why = "no world_cull/ dumps for this level: see batch_480.cpp's header";
        skip_check("shadow_points", 0x00285a68, why);
        skip_check("drop_shadows_draw", 0x00285870, why);
        return;
    }
    const auto shadowed = [](const std::vector<uint8_t>& d) {
        uint32_t n;
        std::memcpy(&n, d.data() + (OV(0x0015fffcu) & 0x01ffffffu), 4);
        return n != 0;
    };
    std::vector<std::string> with;
    for (const std::string& p : dumps)
        if (shadowed(dump482(p))) with.push_back(p);
    vu0::vif0_send_chain(0x0010e4c0, true);  // (the shadows' VU0 programs 0x190 / 0x192)
    if (with.empty()) {
        skip_check("shadow_points", 0x00285a68, "no deferred moby with a shadow in this level's dumps");
        skip_check("drop_shadows_draw", 0x00285870, "no deferred moby with a shadow in this level's dumps");
    } else {
        int n = 0;
        g_pre_call = vu0_to_interpreter;
        check("shadow_points", 0x00285a68, 4 * static_cast<int>(with.size()), Ret::None, [&] {
            load_dump(dump482(with[static_cast<size_t>(n++) % with.size()]));
            return Args{};
        }, [](A) { shadow_points(); return uint64_t{0}; });
        g_ignored_words.push_back(OV(0x0016044c));  // 0x1ff3c0's w lane: a stale VU0 lane (as batch 79)
        n = 0;
        check("drop_shadows_draw", 0x00285870, 16 * static_cast<int>(with.size()), Ret::None, [&] {
            load_dump(dump482(with[static_cast<size_t>(n++) % with.size()]));
            shadow_points();
            // past the first round: the mobys' light directions and clip flags varied (the list's headers)
            if (n > static_cast<int>(with.size())) {
                for (uint32_t h = OV(0x001ac780u); ee_read<uint32_t>(h) != 0;) {
                    const uint32_t count = ee_read<uint32_t>(h);
                    if (coin()) {
                        const float x = rf(-1, 1), z = rf(-1, 1);
                        wf(h + 0x10, x);
                        wf(h + 0x14, rf(-1.0f, -0.2f));
                        wf(h + 0x18, z);
                    }
                    if (ri(0, 2) == 0) w32(h + 4, u(ri(0, 1)));
                    // the entries: 0x20 (one joint) or 0x30 (two)
                    uint32_t e = h + 0x20;
                    for (uint32_t i = 0; i < count && i < 64; ++i) e += ee_read<uint32_t>(e) == 0 ? 0x20u : 0x30u;
                    h = e;
                }
            }
            g_pre_call = vu0_to_interpreter;
            return Args{{OV(0x001ac780u)}, {}};
        }, [](A a) { drop_shadows_draw(a.i[0]); return uint64_t{0}; });
        g_ignored_words.pop_back();
    }
    g_pre_call = nullptr;
}

}  // namespace rac1::oracle
