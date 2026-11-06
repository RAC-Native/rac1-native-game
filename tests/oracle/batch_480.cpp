// Oracle batch 480: the world passes' three culls written by hand (src/game/ovl/world_cull_tfrag.cpp, world_cull_tie.cpp,
// world_cull_shrub.cpp): the tfrag cull 0x294368, the tie cull 0x295fa0 and the shrub cull 0x286f10, against each level's own
// original (its overlay map), on RAM dumps of headless runs, the camera moved about its recorded place in each case (the same
// tables, other spheres in and out of the view, on its planes, at other LODs and fades).
// Run with ORACLE_LEVEL=<n> RAC1_ORACLE_DUMPS=<dir> build/tests/ovl_oracle --batch 480 on the levels whose dumps exist.
// The dumps: <dir>/world_cull/l<n>_<k>.bin (k = 0, 1, ...), each written by a headless run of the game:
//   RAC1_HEADLESS=1 RAC1_GOTO=<n>@60 RAC1_SIM_INPUT=forward RAC1_FRAMES=<f + 1> RAC1_RAMHASH=/dev/null RAC1_RAMDUMP=<f>,l<n>_<k>.bin
//   ./build/rac1-native
// with f = 1200 (k = 0) and 1800 (k = 1): the level's start and further along the forward walk. Skipped without them.
// The routines save the caller's s registers in the scratchpad (0x70003fa8..): the harness ignores those words.

#include "ovl_oracle.h"

#include <game/ovl/world_cull_hand.h>

#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {
const std::vector<uint8_t>& dump480(const std::string& path) {
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
}  // namespace

void batch_480() {
    if (g_only_batch != 480) return;
    g_batch = 480;
    std::vector<std::string> dumps;
    for (int k = 0; k < 4; ++k) {
        const std::string p = dump_path("world_cull/l" + std::to_string(g_oracle_level) + "_" + std::to_string(k) + ".bin");
        if (!dump480(p).empty()) dumps.push_back(p);
    }
    struct Cull {
        const char* name;
        uint32_t orig;
        bool (*port)();
    };
    const Cull culls[] = {{"world_tfrag_cull", 0x00294368, world_hand::tfrag_cull},
                          {"world_tie_cull", 0x00295fa0, world_hand::tie_cull},
                          {"world_shrub_cull", 0x00286f10, world_hand::shrub_cull}};
    if (dumps.empty()) {
        for (const Cull& c : culls) skip_check(c.name, c.orig, "no world_cull/ dumps for this level: see batch_480.cpp's header");
        return;
    }
    for (const Cull& c : culls) {
        int n = 0;
        check(c.name, c.orig, 12 * static_cast<int>(dumps.size()), Ret::None, [&] {
            const std::vector<uint8_t>& d = dump480(dumps[static_cast<size_t>(n++) % dumps.size()]);
            std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
            std::memcpy(ee_get_scratchpad_base(), d.data() + 0x02000000, 0x4000);
            // the camera (the view's +0x140, the passes' centre) moved about its recorded place, the first case of a dump as it is
            if (n > static_cast<int>(dumps.size())) {
                const uint32_t cam = OV(0x00166c80u) + 0x140;
                const float spread = coin() ? 4.0f : 40.0f;
                for (uint32_t i = 0; i < 3; ++i) wf(cam + 4 * i, ee_read<float>(cam + 4 * i) + rf(-spread, spread));
            }
            return Args{};
        }, [&](A) {
            if (!c.port()) std::printf("  [%s] the hand-written cull did not run (a VU0 program running?)\n", c.name);
            return uint64_t{0};
        });
    }
}

}  // namespace rac1::oracle
