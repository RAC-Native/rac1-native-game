// Oracle batch 115 (task 0343): the Vendor on its own overlay. (1) The vendor's screens (game mode 5, vendor_*.cpp,
// transliterated at Veldin's addresses) in the states that read the state block 0x1ca4c0 through a register holding its
// `lui` page on every path (relocated by tools/codegen/hi_flow.py since task 0343; before, Veldin's 0x1ca4c0 on every level):
// the item panel 0x29b4d8 (its mode 2 / 3 path) and the item strip 0x29dea0 with 8 items or more (the draw 0x2a02b0's
// own case is not compared: see below). (2) The help message panel (help_panel_tr.cpp): the three callbacks of
// help_hint_update's request (slot 0xc: init 0x237190, update 0x2371e0, draw 0x237200), on the slot as the game sets it
// up (a message requested, then the request made), its counters varied. Run with ORACLE_LEVEL=1 build/tests/ovl_oracle
// --batch 115 (Novalis's own original against the relocated port, the case that matters); level 0 runs Veldin's
// original on Novalis's data (as batch 113).

#include "ovl_oracle.h"

#include <game/ovl/help_panel_tr.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/vendor_screens.h>
#include <game/ovl/vendor_tr.h>

#include <runtime/kernel/disc.h>

#include <cstring>

namespace rac1::oracle {

void batch_115() {
    if (g_only_batch != 115) return;
    g_batch = 115;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x001e3d5c));  // (a VCROSS result's stale w lane: the vector 0x1e3d50 of the mode's camera rows, as batch 113)
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime

    // 989snd's IOP server (as batch 113's): replies a function of the command, does the stream reads
    sif_rpc_register(0x123456, [](uint32_t fno, uint32_t send, uint32_t, uint32_t recv, uint32_t) {
        auto result = [](uint32_t id, uint32_t a0) -> uint32_t {
            const uint32_t h = (id * 0x9e3779b1u) ^ (a0 * 0x85ebca6bu);
            return ((h >> 7) % 4 == 0) ? 0u : ((h >> 3) | 1u);
        };
        if (fno == 0x4d) {
            const uint32_t n = ee_read<uint32_t>(send);
            uint32_t p = send + 4;
            for (uint32_t i = 0; i < n; ++i) {
                const uint32_t id = ee_read<uint16_t>(p), size = ee_read<uint16_t>(p + 2);
                ee_write<uint32_t>(recv + 4 + i * 4, result(id, size != 0 ? ee_read<uint32_t>(p + 4) : 0u));
                if (id == 0x38) {
                    disc_read_ee(ee_read<uint32_t>(p + 0xc), ee_read<uint32_t>(p + 4), ee_read<uint32_t>(p + 8));
                    ee_write<uint32_t>(0x00137b10, 0);
                    ee_write<uint32_t>(0x00137b00, 0);
                }
                p += 4 + ((size + 3) & ~3u);
            }
            ee_write<uint32_t>(recv, 0xffffffffu);
            ee_write<uint32_t>(recv + 4 + n * 4, 0xffffffffu);
            return;
        }
        ee_write<uint32_t>(recv + 4, result(fno, send != 0 ? ee_read<uint32_t>(send) : 0u));
        ee_write<uint32_t>(recv, 0xffffffffu);
        ee_write<uint32_t>(recv + 8, 0xffffffffu);
    });
    // the level after one gameplay frame (the port's), kept as a snapshot
    static std::vector<uint8_t> snap;
    static int snap_level = -1;
    auto level = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty() || snap_level != g_oracle_level) {
            veldin_boot();
            level_setup_body(g_oracle_level == 0 ? 1 : g_oracle_level, 0);  // (level 0: Novalis's data on Veldin's image, as batch 113)
            w32(OV(0x0016c060), 0xf);
            gameplay_frame();
            snap.assign(ram, ram + 0x02000000);
            snap_level = g_oracle_level;
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    auto mobys_of = [](int cls) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<int16_t>(m + 0xa6) == cls) v.push_back(m);
        return v;
    };

    // ---- the vendor's screens: started from the level's Vendor (or 0: the hero's item 0x20), the block put in the states
    // the checks need. The DMA kick, FlushCache and the vsync wait are stubbed on both sides (as batch 113; the fade 0x1f9838 runs).
    {
        g_extra_noop_hooks = {OVF(0x002a08c8), 0x00122298, 0x00118a80};
        constexpr OvConst<0x001ca4c0u> S{};
        auto start = [level, mobys_of] {
            level();
            for (const int cls : {730, 731, 790})  // (the Agnogg ship starts a scene: its stream read needs the IOP, as batch 113)
                for (const uint32_t m : mobys_of(cls)) w32(m + 0x74, 0);
            // (an item owned, as on any real visit (the Bomb Glove from Veldin): with an empty list the item panel loads
            // class 0, whose WAD the original's decompressor traps on, as the port's does)
            item_give(0xa, 0);
            if (coin()) item_give(0xf, 0);
            w32(0x0015ed98, u(ri(0, 20000)));  // (the bolts)
            const std::vector<uint32_t> vs = mobys_of(11);
            vendor_screen_start_tr(vs.empty() || coin() ? 0u : vs[static_cast<size_t>(ri(0, static_cast<int>(vs.size()) - 1))]);
            const uint32_t b = coin() ? 0u : (R() & 0xf0f0u);  // the pad (+0x1a0 / +0x1a4 of the pad block 0x13c940)
            w32(0x0013c940 + 0x1a4, b);
            w32(0x0013c940 + 0x1a0, b | (coin() ? 0u : (R() & 0xf0f0u)));
            w32(OV(0x0015f5cc), R() & 0xffff);
        };
        // (The draw 0x2a02b0 itself is not compared here: every path past its skip flag runs the frame's render 0x1f85b8, whose
        // passes, in this one-frame snapshot, write through packet / list pointers the main loop has not set up yet, and
        // differ there (batch 113's draw check runs with the start's skip flag 0x15f5d8 still set: the draw returns at
        // once). Its `s2` uses are the same relocation as 0x29b4d8's below; the runtime run of task 0343 exercises it.)
        // the item panel's mode switch 0x29b4d8 (a0 0..4; modes 2 / 3 reach 0x29b644 through `move v1,v0`, the page)
        check("vendor_29b4d8", 0x0029b4d8, 160, Ret::None, [start, S] {
            start();
            w32(OV(0x00161188), coin() ? 0u : 1u);
            const uint32_t m = ee_read<uint32_t>(S + 0x24);
            if (m >= 0x00100000u && m < 0x02000000u) w8(m + 0x20, u(ri(0, 12)));
            seed_rand();
            return Args{{u(ri(0, 4))}, {}};
        }, [](A a) { vendor::vendor_remark(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
        // the item strip 0x29dea0: 8 items or more take the path through s0 = the page
        check("vendor_29dea0", 0x0029dea0, 120, Ret::None, [start, S] {
            start();
            const int n = ri(0, 12);
            w32(S + 0x210, u(n));
            for (int i = 0; i < n; ++i) {
                w32(S + 0xd0 + u(i) * 0x14, u(ri(0, 0x24)));
                w32(S + 0xd4 + u(i) * 0x14, u(ri(0, 1)));
            }
            w32(S + 0x58, u(n > 0 ? ri(0, n - 1) : 0));
            w32(S + 4, u(ri(0, 200)));
            seed_rand();
            return Args{{0u}, {}};
        }, [](A) { vendor::panel_list(); return uint64_t{0}; });
        g_extra_noop_hooks.clear();
    }

    // ---- the help message panel: a message requested (hud_message_request 0x2643c8, the Vendor's 0x53e3 or another),
    // help_hint_update 0x264328 makes the request (slot 0xc) and the slot is started; then its counters varied
    {
        const uint32_t E = OV(0x0017dc50) + 0xc * 0x90;
        auto slot = [level, E] {
            level();
            static const int32_t ids[] = {0x53e3, 0x53e3, 0x53e4, 0x53e0};
            w32(OV(0x0015f594), 0);
            hud_message_request(ri(1, 3), ids[ri(0, 3)]);
            help_hint_update();
            if (ee_read<uint32_t>(E + 0x30) == OVF(0x00237190)) camera_slot_start(E);
            w32(E + 0x58, u(ri(0, 0x40)));
            w32(E + 0x5c, u(ri(0, 0x40)));
            w32(E + 0x7c, u(ri(0, 60)));
            w8(E + 0x70, u(ri(0, 40)));
            w16(E + 0x48, u(ri(-20, 20)));
            w16(E + 0x4a, u(ri(-20, 20)));
            w32(0x0015ed80, coin() ? 0u : 1u);
            w32(OV(0x0015f770), u(ri(0, 300)));
            seed_rand();
        };
        check("help_panel_init", 0x00237190, 60, Ret::None, [slot, E] { slot(); return Args{{E}, {}}; },
              [](A a) { help_panel_init_tr(a.i[0]); return uint64_t{0}; });
        check("help_panel_update", 0x002371e0, 60, Ret::None, [slot, E] { slot(); return Args{{E}, {}}; },
              [](A a) { help_panel_update_tr(a.i[0]); return uint64_t{0}; });
        check("help_panel_draw", 0x00237200, 200, Ret::None, [slot, E] { slot(); return Args{{E}, {}}; },
              [](A a) { help_panel_draw_tr(a.i[0]); return uint64_t{0}; });
    }

    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
