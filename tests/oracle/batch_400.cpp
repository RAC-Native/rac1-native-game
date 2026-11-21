// Oracle batch 400 (issue #42, package P1: the Metal Detector's display on every level): HUD slot 7's draw, Veldin's 0x23a2d0 (with the
// large text box 0x1fb470 it calls), transliterated at Veldin's addresses (hud_slot7_tr.cpp, regen group veldin_hud7) and registered for
// every level's copy (the same code on the 19 overlays; Batalia, Drek's fleet and the Veldin finale keep their own transliterations,
// batches 160 / 271). Before it only those three levels drew the display: the others logged TODO_PORT for the element's draw once the
// Metal Detector (item 0x1b, the flag 0x13d4db) was owned and something buried was near.
// The original is each level's own copy (ORACLE_LEVEL=N: the overlay map's function). The element is the one the hero code (0x205ea8,
// its port) requests after the level's first gameplay frame, with the Metal Detector owned and the buried thing's timer under 20 (as
// batch 271 does on levels 17 / 18 with a frame), started and updated once by the ports; then its fields varied (the counters +0x70 / +0x71 the draw scales by, the positions +0x74 / +0x78, the timer +0x7c, the state
// +0x40, the word its argument points to), the bolt count 0x13e504 and the frame counter. HUD slot 7's start and update (0x235dc0 /
// 0x23a218, already registered for every level, proved on Batalia by batch 160) are checked on the same element too.
// No RAM dumps: the states are built from the level.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 400, N in 0 .. 18.

// Audit 2 (2026-11-13): the packets go to a test-area buffer (the cursor was 0 and the null page, the first sprites' packets,
// was left out); 0x13e504 is the screen's height (mostly a real one); the detector's signal 0x141398 and both counters at 0
// varied, so that the update's end of the element is compared.

#include "ovl_oracle.h"

#include <game/ovl/hud_slot7_tr.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

#include <cstring>
#include <vector>

namespace rac1::oracle {

void batch_400() {
    if (g_only_batch != 400) return;
    g_batch = 400;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    // (no null page left out: the draws' packets go to a buffer of the test area, packet_cursor below; before, the cursor was 0 after
    // the frame and the null page's first 0x100 bytes, the first sprites' packets, were not compared)
    g_extra_noop_hooks.clear();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime

    // the level after a gameplay frame played with the Metal Detector owned and something buried near (0x205ea8 sets PLAYER+0x2048
    // from +0x2044 < 20 with +0x2040 set, then requests the element)
    static std::vector<uint8_t> snap;
    static int snap_level = -1;
    auto level = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty() || snap_level != g_oracle_level) {
            veldin_boot();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            w32(0x0013f2e8, 0xffffffffu);  // (the sound listener's cached points refreshed on the first frame, as batch 270 does)
            if (g_oracle_level == 14) w8(0x0013d3f0, 1);  // (Oltanis: the arrival scene seen, as batch 221's: else its stream is read from the disc)
            gameplay_frame();
            render_view_setup();
            // (a second frame would read the arrival scenes' streams from the disc on levels 1, 7, 8, 12 and 13: the hero code's request
            // made by its port instead, then the element's start and one update, the HUD's order)
            w8(0x0013d4db, 1);
            wf(0x0013f350 + 0x2044, 10.0f);
            w32(0x0013f350 + 0x2040, 1);
            hero_camera_7_update();
            for (uint32_t e = OV(0x0017dc50), i = 0; i < 13; ++i, e += 0x90)
                if (ee_read<uint32_t>(e + 0x30) == ovl_code_lookup(0x00235dc0)) {
                    hud_slot7_start_tr(e);
                    hud_slot7_update_tr(e);
                }
            snap.assign(ram, ram + 0x02000000);
            snap_level = g_oracle_level;
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    level();
    std::vector<uint32_t> es;
    for (uint32_t e = OV(0x0017dc50), i = 0; i < 13; ++i, e += 0x90)
        if (ee_read<uint32_t>(e + 0x30) == ovl_code_lookup(0x00235dc0)) es.push_back(e);
    if (es.empty()) {
        std::printf("batch 400: no HUD slot 7 element on level %d after the frame\n", g_oracle_level);
        ++g_fail;
    }
    std::printf("batch 400: level %d, %zu slot 7 element(s)\n", g_oracle_level, es.size());
    auto pick = [](const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; };

    struct H {
        const char* name;
        uint32_t addr;
        void (*fn)(uint32_t);
        int n;
    };
    static const H hs[] = {{"hud_slot7_draw_tr", 0x0023a2d0, hud_slot7_draw_tr, 200},
                           {"hud_slot7_start_tr", 0x00235dc0, hud_slot7_start_tr, 40},
                           {"hud_slot7_update_tr", 0x0023a218, hud_slot7_update_tr, 60}};
    for (const H& h : hs) {
        if (es.empty()) { skip_check(h.name, h.addr, "no slot 7 element"); continue; }
        check(h.name, h.addr, h.n, Ret::None, [level, es, pick] {
            level();
            const uint32_t e = pick(es);
            const uint32_t P = 0x0013f350;
            if (coin()) w32(e + 0x7c, u(ri(-2, 300)));
            if (coin()) w32(e + 0x40, u(ri(0, 8)));
            if (ri(0, 3) != 0) w8(e + 0x70, u(coin() ? ri(0, 3) : ri(0, 0xff)));  // (0: the draw returns early)
            if (coin()) w8(e + 0x71, u(coin() ? ri(0, 3) : ri(0, 0xff)));
            // (the detector's signal 0x141398: the update's two ways, the counters up or down to the element's end)
            if (coin()) w32(0x00141398, coin() ? 0u : 1u);
            if (ri(0, 5) == 0) { w8(e + 0x70, 0); w8(e + 0x71, 0); }  // (both run out: the update's end of the element)
            if (coin()) w32(e + 0x74, u(ri(0, 0x200)));
            if (coin()) w32(e + 0x78, u(ri(0, 0x200)));
            // (0x13e504: the screen's height the display stands on, not the bolts: mostly a real one, the sprites on the screen)
            if (coin()) w32(0x0013e504, ri(0, 3) == 0 ? u(ri(0, 99999)) : coin() ? 448u : u(ri(0x60, 0x200)));
            if (ri(0, 5) == 0) w8(P + 0x20a4, 2);  // (the hero's state the draw skips)
            if (ri(0, 5) == 0) w32(P + 0x2084, 0x32);
            if (coin()) w32(0x0015ed80, ri(0, 3) == 0 ? 1u : 0u);
            const uint32_t a = ee_read<uint32_t>(e + 0x2c);
            if (a != 0 && coin()) w32(a, u(ri(0, 2)));
            w32(OV(0x0015f5cc), R());
            w32(OV(0x001611c0), TEST_AREA + 0x8000 + u(ri(0, 0x40)) * 0x10);  // (the packet cursor on a buffer of the test area)
            seed_rand();
            return Args{{e}, {}};
        }, [h](A a) { h.fn(a.i[0]); return uint64_t{0}; });
    }

    g_ee_null_is_ram = null_ram;
    g_ignored_words = saved_ignored;
}

}  // namespace rac1::oracle
