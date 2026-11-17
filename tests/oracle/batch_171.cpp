// Oracle batch 171 (levels/gaspar-orxon, release 0.2.0 levels): particle type 0x28's update (Veldin 0x26ff10, L10:0x262cf0; with its
// helper 0x26fd28), ported at Veldin's addresses (assets/generated/src/game/ovl/particle_28_tr.cpp, generated from
// tools/codegen/gen/mkall/veldin_particle28.json in rac1-native-game). First reached on Orxon (a TODO_PORT moby callback when Clank
// falls into the toxic goo, e.g. by class 258). ORACLE_LEVEL=10, on RAM dumps of headless runs of the port (assets/dumps/oracle/gaspar_orxon/,
// skipped without them):
//   goo{935,990}_L10.bin     RAC1_HEADLESS=1 RAC1_LEVEL=10 RAC1_HERO_AT=258@900,3,0,1,3.14 RAC1_SIM_INPUT=seq:920-1300:ly=-0.2
//                              RAC1_RAMHASH=/dev/null RAC1_RAMDUMP=<frame>,<file> (Clank sinking in the goo, state 0x7d: the type-0x28
//                              particles live).
// Each case takes one live type-0x28 particle of the dump and varies what the update reads: its timer (+0xa: 0, about to expire, running:
// the expiry spawns the next one through 0x26fd28), its size (+0xc), its velocity (+0x20..), its flag byte (+0x38), the random seed, and in a third of the cases
// its position and velocity (falling around the hero: the segment test hits the world, the bounce branch).
#include "ovl_oracle.h"

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_28_tr.h>
#include <runtime/ee/ee_interpreter.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {
const std::vector<uint8_t>& p28_dump(const std::string& path) {
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
void p28_load(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}
std::vector<uint32_t> live_particles(uint8_t type) {
    std::vector<uint32_t> out;
    const uint32_t base = ee_read<uint32_t>(OV(0x0016016c));
    const int32_t hi = ee_read<int32_t>(OV(0x00160174));
    for (int32_t i = 0; i <= hi && i < 4096; ++i) {
        const uint32_t p = base + u(i) * 64;
        if (ee_read<uint8_t>(p) == type && ee_read<int8_t>(p + 1) >= 0) out.push_back(p);
    }
    return out;
}
}  // namespace

void batch_171() {
    if (g_only_batch != 171) return;
    g_batch = 171;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // (as in the runtime)
    for (const char* dn : {"goo935_L10", "goo990_L10"}) {
        const std::string name = std::string("particle_28_update ") + dn;
        if (g_oracle_level != 10) {
            skip_check(name.c_str(), 0x0026ff10, "the type-0x28 particles' dumps are Orxon's: ORACLE_LEVEL=10");
            continue;
        }
        const std::vector<uint8_t>& d = p28_dump(dump_path("gaspar_orxon/") + dn + ".bin");
        if (d.empty()) {
            skip_check(name.c_str(), 0x0026ff10, "no RAM dump (assets/dumps/oracle/gaspar_orxon)");
            continue;
        }
        p28_load(d);
        std::printf("%s: %zu live type-0x28 particles\n", dn, live_particles(0x28).size());
        if (live_particles(0x28).empty()) {
            skip_check(name.c_str(), 0x0026ff10, "no live type-0x28 particle in the dump");
            continue;
        }
        auto gen = [&d] {
            p28_load(d);
            const std::vector<uint32_t> ps = live_particles(0x28);
            if (ps.empty()) return Args{{0}, {}};
            const uint32_t p = ps[static_cast<size_t>(ri(0, static_cast<int>(ps.size()) - 1))];
            seed_rand();
            { const int k = ri(0, 3); if (k == 0) w16(p + 0xa, 0); else if (k == 1) w16(p + 0xa, u(ri(1, 2))); else if (k == 2) w16(p + 0xa, u(ri(3, 60))); }
            if (coin()) wf(p + 0xc, rf(0.05f, 3.0f));
            if (coin()) wfrand(p + 0x20, 3, -0.5f, 0.5f);
            if (coin()) w8(p + 0x38, R() & 0xff);
            if (ri(0, 2) == 0) {  // (the particle moved above the floor the hero stood on, falling: its segment test hits the world)
                const float hz = ee_read<float>(0x0013f3d8);
                wf(p + 0x10, ee_read<float>(0x0013f3d0) + rf(-2, 2));
                wf(p + 0x14, ee_read<float>(0x0013f3d4) + rf(-2, 2));
                wf(p + 0x18, hz + rf(-1.0f, 1.5f));
                wf(p + 0x20, rf(-0.3f, 0.3f)); wf(p + 0x24, rf(-0.3f, 0.3f)); wf(p + 0x28, rf(-3.0f, -0.2f));
            }
            return Args{{p}, {}};
        };
        // (stage 5, task C2c) particle_28_update reaches segment_collide: A2's collision scratch ranges left out of this check
        const CollisionScratchIgnored spr_ignored;
        check(name.c_str(), 0x0026ff10, 60, Ret::None, gen, [](A a) { if (a.i[0] != 0) particle_28_update(a.i[0]); return uint64_t{0}; });
    }
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
