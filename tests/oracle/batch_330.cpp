// Oracle batch 330 (fix/hydrodisplacer-rac3: on Hoven the Hydrodisplacer's animation on the first faucet is cut short and the faucet
// does nothing more until the item is put away and drawn again). The cause is the game's own code, not the port: in the faucet's
// states 0x38..0x3a the hero is pulled onto the faucet moby's origin (Veldin's ride update, 0x217970 block 0x21969c; Hoven's own build
// of hero_state_update), and coming from the faucet's +x / +y side the position, rounded toward zero, lands exactly on that origin.
// Hoven's first faucet (class 341, uid 38, at 401.269928 369.598297) has a collision triangle edge through its origin: the vertical
// ground probe (segment_collide 0x1efc70, the moby part's triangle test) through a point on that edge fails the edge test of both
// triangles (the edge value, ~-1e4 for values ~1e10, is negative on both sides), so the probe misses the faucet, the hero drops onto
// the level floor 0.06 below and leaves state 0x38, and the Hydrodisplacer's hand stays in its state 3 (it waits for the hero on a
// class-341 moby: 0x2df9a0, L_2dfbec).
// This batch shows that the original code does the same on the same memory (L12's own functions, ORACLE_LEVEL=12):
//   - hero_frame_update 0x2070d0 on the frame where the hero reaches the origin (edge_1254: one ulp from it in x and y, state 0x38);
//   - segment_collide 0x1efc70, the hero's ground probe (flags 2, the hero's moby ignored) from 2.2 above to 48 below (at least 0), through the
//     faucet's origin exactly and through points a few ulps from it (on the dumps edge_1254 / edge_1255 and off_1275, the faucet used
//     from the -x / -y side: the hero stops one ulp short of the origin and the faucet works).
// Run with ORACLE_LEVEL=12 build-oracle/tests/ovl_oracle --batch 330.
// The dumps (assets/dumps/oracle/hydro-hoven/, skipped without them), from the repository's root; every run with RAC1_HEADLESS=1
// RAC1_LEVEL=12 RAC1_GIVE_ITEMS=0x16 RAC1_GIVE_FRAME=1100 RAC1_EQUIP=0x16 RAC1_RAMHASH=/dev/null RAC1_FRAMES=<f>+2
// RAC1_RAMDUMP=<f>,assets/dumps/oracle/hydro-hoven/<name>.bin:
//   edge_<f>_L12.bin, f = 1254, 1255: RAC1_HERO_AT=341@1150,0,-3,1 RAC1_SIM_INPUT='seq:1170-1250:lx=1;1230+3:o' (Ratchet runs onto
//       the faucet from its +x / +y side and presses circle while still moving; 1254: one ulp from the origin, 1255: the faucet lost)
//   off_1275_L12.bin: RAC1_HERO_AT=341@1150,0,0,1 RAC1_SIM_INPUT='seq:1250+3:o' (placed on the faucet's -y side: it works)

// Audit 2 (2026-11-13): also the Hydrodisplacer's hand update 0x2df9a0 (hydro.cpp) on these dumps, the only state with its
// faucet: batch 106 (levels 0 .. 3) reaches 138 of its 516 instructions, this check 370.

#include "ovl_oracle.h"

#include <game/ovl/hydro.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {
const std::vector<uint8_t>& dump330(const std::string& path) {
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
void load330(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}
constexpr uint32_t FAUCET = 0x01eb8fc0;  // (Hoven's first class-341 moby in these dumps)
constexpr uint32_t SEG = 0x000c0000;     // (the probe's two end points: unused low RAM)
// a float `n` ulps from `f`
float ulps(float f, int32_t n) { return u2f(static_cast<uint32_t>(static_cast<int32_t>(f2u(f)) + n)); }
}  // namespace

void batch_330() {
    if (g_only_batch != 330) return;
    g_batch = 330;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // (the particle update's saved return address)
    g_ignored_words.push_back(OV(0x0016018c));
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    for (uint32_t k = 0; k < 0x30; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);  // (the segment records' VU0 registers, as batch 242)
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
    struct Dump { const char* name; const char* file; };
    static const Dump dumps[] = {{"edge_1254", "hydro-hoven/edge_1254_L12.bin"}, {"edge_1255", "hydro-hoven/edge_1255_L12.bin"},
                                 {"off_1275", "hydro-hoven/off_1275_L12.bin"}};
    for (const Dump& dm : dumps) {
        if (g_oracle_level != 12) { skip_check(dm.name, 0x002070d0, "Hoven's dumps: ORACLE_LEVEL=12"); continue; }
        const std::vector<uint8_t>& d = dump330(dump_path(dm.file));
        if (d.empty()) { skip_check(dm.name, 0x002070d0, "no RAM dump (see the header)"); continue; }
        // the hero's frame as recorded (only the frame counter and the seed varied: the pad as the run had it)
        std::string nm = std::string("frame_") + dm.name;
        check(nm.c_str(), 0x002070d0, 8, Ret::None, [&d] {
            load330(d);
            w32(OV(0x0015f5cc), ee_read<uint32_t>(OV(0x0015f5cc)) + u(ri(0, 3)));
            seed_rand();
            return Args{};
        }, [](A) { hero_frame_update(); return uint64_t{0}; });
        // the ground probe through the faucet's origin, exactly (a third of the cases) or a few ulps from it in x / y
        nm = std::string("probe_") + dm.name;
        check(nm.c_str(), 0x001efc70, 24, Ret::Int32, [&d] {
            load330(d);
            const float x = ee_read<float>(FAUCET + 0x10), y = ee_read<float>(FAUCET + 0x14), z = ee_read<float>(FAUCET + 0x18) + 0.062f;
            const int k = ri(0, 2);
            const float px = k == 0 ? x : ulps(x, ri(-3, 3)), py = k == 0 ? y : ulps(y, ri(-3, 3));
            wf(SEG, px); wf(SEG + 4, py); wf(SEG + 8, z + 2.2f); wf(SEG + 12, 0.0f);
            wf(SEG + 16, px); wf(SEG + 20, py); wf(SEG + 24, std::max(z - 48.0f, 0.0f)); wf(SEG + 28, 0.0f);
            return Args{{SEG, SEG + 16, 2u, ee_read<uint32_t>(0x0013f350 + 0x2080), 0u}, {}};
        }, [](A a) { return ri32(segment_collide(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4])); });
        // what the code (the original's, as the checks above show) makes of it: the hero's frame, then the probe through the origin
        // and one ulp from it (the moby hit, 0 = the level's floor)
        load330(d);
        hero_frame_update();
        std::printf("  %s: after the hero's frame: state 0x%x, on moby %08x\n", dm.name, ee_read<uint32_t>(0x0013f350 + 0x2084),
                    ee_read<uint32_t>(0x0013f350 + 0x2fc));
        for (const int n : {0, 1, -1}) {
            load330(d);
            const float x = ulps(ee_read<float>(FAUCET + 0x10), n), y = ulps(ee_read<float>(FAUCET + 0x14), n), z = ee_read<float>(FAUCET + 0x18) + 0.062f;
            wf(SEG, x); wf(SEG + 4, y); wf(SEG + 8, z + 2.2f); wf(SEG + 12, 0.0f);
            wf(SEG + 16, x); wf(SEG + 20, y); wf(SEG + 24, std::max(z - 48.0f, 0.0f)); wf(SEG + 28, 0.0f);
            const int32_t r = segment_collide(SEG, SEG + 16, 2, ee_read<uint32_t>(0x0013f350 + 0x2080), 0);
            std::printf("  %s: probe at the faucet's origin %+d ulp: hit %d, moby %08x, z %.5f\n", dm.name, n, r, ee_read<uint32_t>(OV(0x00173e40) + 0x18),
                        ee_read<float>(OV(0x00173e40) + 0x28));
        }
    }
    // (audit 2) the Hydrodisplacer's hand update 0x2df9a0 (hydro.cpp, a stage-3 hand port) on Hoven's own copy, on these dumps: the
    // only level state with its faucet (class 341), which batch 106 (levels 0 .. 3) cannot reach: there the update ran its first 138 of
    // 516 instructions. The hand's state +0x20 (0 .. 5), the hero on the faucet or not (+0x2fc), the water flag +0x20b0, the other
    // mode +0x10b4 = 3, the fire button, the faucet's turn +0xbc / the fade 0x15f3fc / the turn's progress pvar +0x10 varied; the
    // segment records' VU0 words (batch 242's) left out, not the null page.
    {
        std::vector<uint32_t> ign = vcross_lanes();
        ign.push_back(OV(0x00160188));
        ign.push_back(OV(0x0016018c));
        for (uint32_t k = 0; k < 0x30; k += 4) ign.push_back(OV(0x00173e90) + k);
        const std::vector<uint32_t> kept = g_ignored_words;
        g_ignored_words = ign;
        constexpr uint32_t P = 0x0013f350;
        for (const Dump& dm : dumps) {
            const std::string nm = std::string("hydro_update_") + dm.name;
            if (g_oracle_level != 12) { skip_check(nm.c_str(), 0x002df9a0, "Hoven's dumps: ORACLE_LEVEL=12"); continue; }
            const std::string file = dump_path(dm.file);
            const std::vector<uint8_t>& d = dump330(file);
            if (d.empty()) { skip_check(nm.c_str(), 0x002df9a0, "no RAM dump (see the header)"); continue; }
            load330(d);
            const uint32_t hand_cls = ee_read<uint32_t>(OV(0x00179ac0) + 0x16 * 0x4c + 0x10);
            uint32_t hand = 0;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                if (ee_read<uint16_t>(m + 0xa6) == hand_cls && ee_read<uint8_t>(m + 0x20) < 0xfd) { hand = m; break; }
            if (hand == 0) { skip_check(nm.c_str(), 0x002df9a0, "no Hydrodisplacer hand moby in the dump"); continue; }
            check(nm.c_str(), 0x002df9a0, 300, Ret::None, [&d, hand] {
                load330(d);
                const uint32_t pv = ee_read<uint32_t>(hand + 0x78);
                if (coin()) w8(hand + 0x20, u(ri(0, 6)));
                const int on = ri(0, 3);
                if (on == 0) w32(P + 0x2fc, 0);
                else if (on == 1) w32(P + 0x2fc, FAUCET);
                w8(P + 0x20b0, u(ri(0, 1)));
                if (ri(0, 4) == 0) w32(P + 0x10b4, 3);
                if (coin()) w32(0x0013cae0, coin() ? ee_read<uint32_t>(P + 0x10a0) : 0u);
                if (coin()) w32(P + 0x208c, u(ri(0, 3)));
                if (ri(0, 3) == 0) w16(P + 0x30e, 1);
                if (coin()) w32(P + 0xa98, ee_read<uint32_t>(P + 0xa98) ^ 2u);
                w8(hand + 0xbc, u(ri(0, 2)));
                if (coin()) wf(OV(0x0015f3fc), coin() ? rf(0.0f, 1.0f) : (coin() ? 0.0f : 1.0f));
                if (pv != 0 && coin()) { wf(pv + 0x10, coin() ? 0.0f : rf(0.0f, 1.2f)); wf(pv + 0x14, rf(-0.1f, 0.1f)); }
                if (coin()) w32(OV(0x0015f5c4), 0);
                if (coin()) wfrand(P + 0x100, 3, -0.3f, 0.3f);  // (the hero's velocity the spray takes on)
                if (pv != 0 && coin()) w32(pv + 4, u(ri(0, 2)));  // (the spray's timer about to run out)
                seed_rand();
                return Args{{hand}, {}};
            }, [](A a) { hydro_update(a.i[0]); return uint64_t{0}; });
        }
        g_ignored_words = kept;
    }
    g_extra_noop_hooks.clear();
    g_ee_null_is_ram = null_ram;
    g_ignored_words = saved_ignored;
}

}  // namespace rac1::oracle
