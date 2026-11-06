// Oracle batch 481: the tfrag entries' static lighting written by hand (src/game/ovl/world_lighting_tfrag.cpp), 0x295350, against
// each level's own original (its overlay map), on the RAM dumps of batch 480 (world_cull/: levels 0, 3, 5, 6, 11, 12 and 18, see
// batch_480.cpp's header for the command lines). Per case a list of entries in the scratchpad (0x70003000, as the level load
// passes it): the frame's own relight list (the tfrag pass's 0x1c5400, as the render pass relights them; copied, since a list
// in RAM is not the level's own address on the original's side), or random entries, some given another light set (+0x34 >= 0:
// the one-set loop) or point lights (the nibbles of +0x36) placed about them. Batch 101 checks the same routine on Veldin
// started from the disc and on the title. Skipped without the dumps.

#include "ovl_oracle.h"

#include <game/ovl/world_cull_hand.h>

#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {
const std::vector<uint8_t>& dump481(const std::string& path) {
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

void batch_481() {
    if (g_only_batch != 481) return;
    g_batch = 481;
    std::vector<std::string> dumps;
    for (int k = 0; k < 4; ++k) {
        const std::string p = dump_path("world_cull/l" + std::to_string(g_oracle_level) + "_" + std::to_string(k) + ".bin");
        if (!dump481(p).empty()) dumps.push_back(p);
    }
    if (dumps.empty()) {
        skip_check("world_tfrag_lighting", 0x00295350, "no world_cull/ dumps for this level: see batch_480.cpp's header");
        return;
    }
    int n = 0;
    check("world_tfrag_lighting", 0x00295350, 16 * static_cast<int>(dumps.size()), Ret::None, [&] {
        const std::vector<uint8_t>& d = dump481(dumps[static_cast<size_t>(n++) % dumps.size()]);
        std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
        std::memcpy(ee_get_scratchpad_base(), d.data() + 0x02000000, 0x4000);
        const uint32_t table = ee_read<uint32_t>(OV(0x00160f4cu));
        const int32_t count = ee_read<int32_t>(OV(0x00160f50u));
        // the first case of each dump: the frame's own relight list (the tfrag pass's 0x1c5400), copied to the scratchpad's list
        const uint32_t list = 0x70003000u;
        const uint32_t frame_list = OV(0x001c5400u);
        if (n <= static_cast<int>(dumps.size()) && ee_read<uint16_t>(frame_list) != 0xffffu) {
            uint32_t p = frame_list, q = list;
            for (; ee_read<uint16_t>(p) != 0xffffu && q < 0x700033feu; p += 2, q += 2) w16(q, ee_read<uint16_t>(p));
            w16(q, 0xffff);
            return Args{{list}, {}};
        }
        std::vector<uint32_t> named;
        const int k = ri(1, 8);
        for (int i = 0; i < k && count > 0; ++i) {
            const uint32_t e = u(ri(0, count - 1));
            w16(list + 2 * u(i), e);
            named.push_back(e);
        }
        w16(list + 2 * u(static_cast<int32_t>(named.size())), 0xffff);
        for (uint32_t e : named) {
            const uint32_t a = table + e * 0x40;
            if (ri(0, 3) == 0) w8(a + 0x34, u(ri(0, 11)));         // one light set for the entry
            if (ri(0, 2) == 0) w8(a + 0x35, u(ri(0, 1)));          // the relight flag
            if (ri(0, 1) == 0) {                                     // point lights about the entry (its sphere: 1/1024 units)
                uint32_t w = 0xffffu;
                for (int i = ri(1, 3) - 1; i >= 0; --i) w = (w & ~(0xfu << (4 * i))) | u(ri(0, 7)) << (4 * i);
                w16(a + 0x36, w);
                const float x = ee_read<float>(a) / 1024, y = ee_read<float>(a + 4) / 1024, z = ee_read<float>(a + 8) / 1024;
                const float r = ee_read<float>(a + 12) / 1024;
                for (uint32_t p = 0; p < 8; ++p) {
                    const uint32_t l = OV(0x001802c0u) + p * 0x20;
                    wfrand(l, 3, 0, 1.2f);
                    wf(l + 0xc, rf(0, 1));
                    wf(l + 0x10, x + rf(-1, 1) * r);
                    wf(l + 0x14, y + rf(-1, 1) * r);
                    wf(l + 0x18, z + rf(-1, 1) * r);
                    wf(l + 0x1c, rf(0.3f, 2.0f) * r + 0.5f);
                }
            }
        }
        return Args{{list}, {}};
    }, [](A a) {
        if (!world_hand::tfrag_lighting(a.i[0])) std::printf("  [world_tfrag_lighting] the hand-written routine did not run\n");
        return uint64_t{0};
    });
}

}  // namespace rac1::oracle
