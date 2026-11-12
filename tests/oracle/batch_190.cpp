// Oracle batch 190 (rac1-native-game fix/graphics, issue #27): two loads the transliterator folded to a constant address. Its text pass
// (tools/codegen/mips2cpp.py, the `lui` pages) carried a `lui` from a branch-likely's delay slot - which runs only when the branch is
// taken - onto the fall-through path:
//   - the Vendor's start 0x29a6b0, at 0x29a884: `bnel v1,s2` / `lui v0,0x17` in the slot / `lw a1,0xd0(v0)`: the centre item's id
//     (the item list 0x1ca4c0 + index x 0x14 + 0xd0) read from 0x1700d0 instead: the ammo element requested for item 0 (the "no
//     chickens" icon, 0/0) when the centre item is owned;
//   - the drones' update 0x2ce358 (class 479), at 0x2ceeb4..: the moby the drone's sphere hit (*(0x173e58)) read as 0x170000.
// Both compared here on paths batches 131 / 180 never ran (the Vendor's start is set-up there; the drone's hit path is uncovered:
// ORACLE_COVER_EE=2cf100 ORACLE_COVER_FROM=2cee80).
// Needs the generator fix of branch fix/graphics (tools/codegen/mips2cpp.py, commit 88cfc07) in the generated sources: without it
// both checks fail (0/80 before the fix, 80/80 after, levels 0, 1, 3, 5). The drones' checks read batch 180's dumps
// (assets/dumps/oracle/weapons_dumps/, ORACLE_LEVEL=0).
#include "ovl_oracle.h"

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/vendor_tr.h>
#include <game/ovl/weapon_classes.h>
#include <runtime/ee/ee_interpreter.h>
#include <runtime/kernel/disc.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {
const std::vector<uint8_t>& b190_dump(const std::string& path) {
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
void b190_load(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}
std::vector<uint32_t> b190_live(int16_t cls) {
    std::vector<uint32_t> out;
    const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
    const uint32_t n = (static_cast<uint32_t>(ee_read<int32_t>(OV(0x0015ffe0)) - static_cast<int32_t>(first)) >> 8) + 1;
    for (uint32_t k = 0; k < n && k < 3000; ++k) {
        const uint32_t m = first + k * 0x100;
        if (ee_read<int16_t>(m + 0xa6) == cls && ee_read<uint8_t>(m + 0x20) < 0xfd) out.push_back(m);
    }
    return out;
}
uint32_t b190_pick(const std::vector<uint32_t>& v) { return v.empty() ? 0u : v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; }

// the drones of the dump with an enemy (749) next to them: the drone put on the enemy (or near it), so its sphere test meets it
void drones_hit() {
    for (const char* dn : {"drone_L0_enemy", "drone_L0_330"}) {
        const std::string name = std::string("drone_update_hit ") + dn;
        if (g_oracle_level != 0) {
            skip_check(name.c_str(), 0x002ce358, "the dump is Veldin's: ORACLE_LEVEL=0");
            continue;
        }
        const std::vector<uint8_t>& d = b190_dump(dump_path("weapons_dumps/") + dn + ".bin");
        if (d.empty()) {
            skip_check(name.c_str(), 0x002ce358, "no RAM dump (assets/dumps/oracle/weapons_dumps: batch 180's header)");
            continue;
        }
        b190_load(d);
        if (b190_live(479).empty()) {
            skip_check(name.c_str(), 0x002ce358, "no drone in the dump");
            continue;
        }
        auto gen = [&d] {
            b190_load(d);
            const uint32_t m = b190_pick(b190_live(479));
            seed_rand();
            std::vector<uint32_t> targets;  // (any live moby with a class: those with a collision mesh are hit)
            {
                const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
                for (uint32_t t = first; t < ee_read<uint32_t>(OV(0x0015ffe0)) && targets.size() < 2000; t += 0x100)
                    if (ee_read<uint8_t>(t + 0x20) < 0xfd && ee_read<uint32_t>(t + 0x24) != 0 && t != m) targets.push_back(t);
            }
            const uint32_t t = b190_pick(targets);
            if (m != 0 && t != 0 && ri(0, 4) != 0) {
                for (uint32_t k = 0; k < 3; ++k) wf(m + 0x10 + 4 * k, ee_read<float>(t + 0x10 + 4 * k) + rf(-1.0f, 1.0f) + (k == 2 ? rf(-0.5f, 2.0f) : 0.0f));
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                if (coin()) wfrand(pv, 3, -0.4f, 0.4f);
                // the attack state 3 (0x2ced0c): its target +0x30 (a moby whose class is +0x48), the sphere test 0x2cee88 toward it
                if (ri(0, 3) != 0) {
                    w8(m + 0x20, 3);
                    w32(pv + 0x30, t);
                    w32(pv + 0x48, static_cast<uint32_t>(static_cast<int32_t>(ee_read<int16_t>(t + 0xa6))));
                } else if (coin()) {
                    w8(m + 0x20, u(ri(0, 6)));
                }
            }
            return Args{{m}, {}};
        };
        check(name.c_str(), 0x002ce358, 400, Ret::None, gen, [](A a) { if (a.i[0] != 0) drone_update(a.i[0]); return uint64_t{0}; });
    }
}

// the Vendor's start 0x29a6b0 on the level after a gameplay frame (as batch 131's), with owned items (random, the quick select's slots
// among them) so the centre item of its list is owned or not
void vendor_start() {
    g_extra_noop_hooks = {OVF(0x002a08c8), 0x00122298, 0x00118a80};
    sif_rpc_register(0x123456, [](uint32_t, uint32_t send, uint32_t, uint32_t recv, uint32_t) {
        ee_write<uint32_t>(recv + 4, send != 0 ? (ee_read<uint32_t>(send) * 0x9e3779b1u) | 1u : 1u);
        ee_write<uint32_t>(recv, 0xffffffffu);
        ee_write<uint32_t>(recv + 8, 0xffffffffu);
    });
    static std::vector<uint8_t> snap;
    static int snap_level = -1;
    auto level = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty() || snap_level != g_oracle_level) {
            veldin_boot();
            level_setup_body(g_oracle_level == 0 ? 1 : g_oracle_level, 0);
            w32(OV(0x0016c060), 0xf);
            gameplay_frame();
            snap.assign(ram, ram + 0x02000000);
            snap_level = g_oracle_level;
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    check("vendor_screen_start", 0x0029a6b0, 80, Ret::None, [level] {
        level();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        std::vector<uint32_t> vs;
        for (uint32_t m = first; m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<int16_t>(m + 0xa6) == 11) vs.push_back(m);
        for (const int32_t it : {0xa, 0xb, 0xd, 0xf, 0x10, 0x13, 0x15, 0x17, 0x18, 0x19})
            if (coin()) item_give(it, 0);
        for (uint32_t s = 0; s < 12; ++s) {  // the quick select's slots (0xff ends; bit 0x40 flagged)
            const int k = ri(0, 3);
            w8(0x0015edd0 + s, k == 0 ? 0xffu : u(ri(0x8, 0x19)) | (coin() ? 0x40u : 0u));
        }
        if (coin()) w32(0x0013f350 + 0x10b8, u(ri(0, 0x19)));  // (the item in hand)
        for (uint32_t i = 0; i < 0x25; ++i)
            if (coin()) w32(0x0013d428 + 4 * i, u(ri(0, 300)));  // (the ammo)
        w32(0x0015ed98, u(ri(0, 20000)));
        seed_rand();
        return Args{{vs.empty() || coin() ? 0u : b190_pick(vs)}, {}};
    }, [](A a) { vendor_screen_start_tr(a.i[0]); return uint64_t{0}; });
    g_extra_noop_hooks.clear();
}
}  // namespace

void batch_190() {
    if (g_only_batch != 190) return;
    g_batch = 190;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // (as in the runtime)
    drones_hit();
    vendor_start();
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
