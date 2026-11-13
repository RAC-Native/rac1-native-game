// Oracle batch 132 (task 0374): the galactic map, the ship's planet menu (galactic_map.cpp by hand, galactic_map_tr.cpp
// transliterated at Veldin's addresses). The menu opened as the take-off's end opens it (0x276190(0), state 0xe: the galaxy
// page 0x1b6088, state 0xf) and run by the port's menu frames; the planet page 0x1b63f8 (0x10) reached by a cross. Then the
// pages' widget functions: the planet list's focus-in 0x27f448 (the unlocked planets 0x13d510 and the help planet varied),
// the galaxy view's update / draw 0x27f550 / 0x27f578 (and its body 0x2033c0 in random boxes), the planet page's command
// line 0x27f5b0 and its model 0x27cbc0, the title's focus-in 0x27f440. Run with ORACLE_LEVEL=1 too (Novalis's own original
// against the relocated port).

// Audit 2 (2026-11-13): the galaxy view's planet marks (0x13dd40 / 0x13dd58) and the 50 Hz flag varied (before, every planet
// unmarked: the view drew its two pictures alone), the help planet's gold bolts 0x14bec0 varied for the planet page's model.

#include "ovl_oracle.h"

#include <game/ovl/galactic_map.h>
#include <game/ovl/galactic_map_tr.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <runtime/kernel/disc.h>

#include <cstring>

namespace rac1::oracle {

void batch_132() {
    if (g_only_batch != 132) return;
    g_batch = 132;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // the callees left TODO_PORT (as batch 73): the kernel's vsync wait, the frame capture, the packet kick, the mode-3
    // screen 7, the menu's VU moby pass (menu_draw's; the planet model's own 0x24ff08 runs on both sides)
    g_extra_noop_hooks = {0x00122298, OVF(0x002a0f18), OVF(0x002a08c8), OVF(0x00299a68), 0x00118a80};

    // 989snd's IOP server (as batch 73's): replies a function of the command, does the stream reads
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
    static const uint32_t MS = OV(0x001b9cf0), PAD = 0x0013c940;
    static const uint32_t UNLOCKED = 0x0013d510;
    auto packet = [] {  // the frame packet: buffer 0, the cursor at a random point of it (as batch 73)
        w32(OV(0x001611b8), ee_read<uint32_t>(OV(0x00173e08)));
        w32(OV(0x001611bc), ee_read<uint32_t>(OV(0x00173e08)) + 0x30000);
        w32(OV(0x001611d0), 0);
        w32(OV(0x001611c0), ee_read<uint32_t>(OV(0x001611b8)) + u(ri(0, 0x100)) * 0x10);
    };
    // the menu on the galaxy page (state 0xe as the take-off's end asks it, 16 frames), then (page 0x10) on the planet page
    static std::vector<uint8_t> snap15, snap16;
    static int snap_level = -1;
    auto menu = [packet](int page) {
        uint8_t* ram = ee_get_ram_base();
        if (snap15.empty() || snap_level != g_oracle_level) {
            veldin_boot();
            ee_write<uint16_t>(0x001518d0, 512);  // the display size (the host's, from the boot module)
            ee_write<uint16_t>(0x001518d2, 416);
            particle_table_init();
            level_setup_body(g_oracle_level == 0 ? 1 : g_oracle_level, 0);  // (level 0: Novalis's data on Veldin's image, as batch 115)
            hud_init();  // (the overlay's start: the menu's moby list 0x15fa70)
            w32(OV(0x0016c060), 0xf);
            gameplay_frame();
            // the class update functions not ported yet: no-ops on both sides (as batch 73)
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); ee_read<uint8_t>(m + 0x20) != 0xff; m += 0x100) {
                const uint32_t fn = ee_read<uint32_t>(m + 0x74);
                if (fn != 0 && !ovl_moby_fn_ported(fn)) g_extra_noop_hooks.push_back(fn);
            }
            // the planets the ship flies to (as after Novalis's Blarg ship: Veldin's two, Novalis, Kerwan)
            w32(UNLOCKED, 1);
            w32(UNLOCKED + 4, 3);
            w32(UNLOCKED + 8, 0);
            w32(0x0015ec8c, 1);  // (989snd's streams up, as in the runtime: the pages' pictures read through its command 0x38)
            w32(OV(0x0016c06c), 0);
            menu_open(0);
            w32(MS, 0xe);
            menu_page_start();  // (the galaxy page laid out: its mobys, buffers; its widgets' disc reads are not run here: the
                                // planet map's data comes from the IOP's stash, not modelled by this batch)
            snap15.assign(ram, ram + 0x02000000);
            const uint32_t p15 = ee_read<uint32_t>(MS + 4);
            w32(MS + 4, OV(0x001b63f8));  // the planet page shown (as a cross on the galaxy page asks it)
            w32(MS, 0x10);
            snap16.assign(ram, ram + 0x02000000);
            std::printf("galactic map: pages shown %08x (galaxy) / %08x (planet)\n", p15, ee_read<uint32_t>(MS + 4));
            snap_level = g_oracle_level;
        }
        const std::vector<uint8_t>& s = page == 0x10 ? snap16 : snap15;
        std::memcpy(ram, s.data(), s.size());
        packet();
    };
    auto buttons = [] {  // the pad: +0x1c4 pressed, +0x1b4 repeated
        static const uint32_t b[] = {0x1000, 0x4000, 0x40, 0x10, 0x800, 0x100, 0x20, 4, 8, 0};
        const uint32_t k = b[ri(0, 9)] | (ri(0, 4) == 0 ? b[ri(0, 9)] : 0u);
        w32(PAD + 0x1c4, k);
        w32(PAD + 0x1b4, coin() ? k : b[ri(0, 9)]);
    };
    auto planets = [] {  // the unlocked planets (0 ends; at most 0x14 read) and the help planet
        const int n = ri(0, 22);
        for (int i = 0; i < n; ++i) w32(UNLOCKED + u(i) * 4, u(ri(1, 18)));
        w32(UNLOCKED + u(n) * 4, 0);
        w32(OV(0x00184414), u(ri(0, 4) == 0 ? ri(0, 18) : static_cast<int>(ee_read<uint32_t>(UNLOCKED + u(ri(0, n)) * 4))));
    };

    // the planets' marks on the galaxy view (0x13dd40 + planet: seen, a blinking dot; 0x13dd58 + planet: visited, a dot) and
    // the 50 Hz flag (the rows' heights scaled): before, every planet unmarked, the view drew its two pictures alone
    auto marks = [] {
        for (uint32_t p = 0; p < 20; ++p) {
            w8(0x0013dd40 + p, ri(0, 2) == 0 ? 0u : 1u);
            w8(0x0013dd58 + p, ri(0, 2) == 0 ? 1u : 0u);
        }
        w32(0x0015ed80, ri(0, 3) == 0 ? 1u : 0u);
    };
    // the gold bolts found on the help planet's level (0x14bec0 + 4 planet + k), which the planet page's model counts
    auto bolts = [] {
        for (uint32_t k = 0; k < 20 * 4; ++k) w8(0x0014bec0 + k, coin() ? 1u : 0u);
    };
    check("galmap_nop_focus", 0x0027f440, 5, Ret::Int32, [menu] { menu(0xf); return Args{{OV(0x001b6178)}, {}}; },
          [](A a) { return ri32(galmap_nop_focus(a.i[0])); });
    check("galmap_list_focus_in", 0x0027f448, 160, Ret::Int32, [menu, planets] {
        menu(0xf);
        planets();
        if (ri(0, 4) == 0) w32(OV(0x001b62b8) + 0x30, R());
        return Args{{OV(0x001b62b8)}, {}};
    }, [](A a) { return ri32(galmap_list_focus_in(a.i[0])); });
    check("galmap_view_update", 0x0027f550, 40, Ret::Int32, [menu, buttons] {
        menu(0xf);
        buttons();
        return Args{{OV(0x001b6360)}, {}};
    }, [](A a) { return ri32(galmap_view_update(a.i[0])); });
    check("galmap_planet_update", 0x0027f5b0, 160, Ret::Int32, [menu, buttons] {
        menu(coin() ? 0x10 : 0xf);
        buttons();
        w32(MS + 0x124, ri(0, 3) == 0 ? 1u : 0u);
        w32(OV(0x00184414), u(ri(0, 18)));
        w32(0x0015ed84, u(ri(0, 18)));
        // (the widget's moby: the sound's source; one of the menu's 14 mobys 0x1b9e90 or none)
        w32(OV(0x001b6480) + 0x14, coin() ? ee_read<uint32_t>(OV(0x001b9e90) + u(ri(0, 13)) * 4) : 0u);
        return Args{{OV(0x001b6480)}, {}};
    }, [](A a) { return ri32(galmap_planet_update(a.i[0])); });
    check("galmap_view_draw", 0x0027f578, 80, Ret::Int32, [menu, planets, marks] {
        menu(0xf);
        if (coin()) planets();
        if (ri(0, 3) != 0) marks();
        w32(OV(0x00184414), u(ri(0, 18)));
        w32(OV(0x0015f3f8), R());
        if (ri(0, 2) != 0) {  // (its box: x, y, width, height)
            w32(OV(0x001b6360) + 0x18, u(ri(0, 300)));
            w32(OV(0x001b6360) + 0x1c, u(ri(0, 300)));
            w32(OV(0x001b6360) + 0x20, u(ri(1, 300)));
            w32(OV(0x001b6360) + 0x24, u(ri(1, 200)));
        }
        return Args{{OV(0x001b6360)}, {}};
    }, [](A a) { return ri32(galmap_view_draw(a.i[0])); });
    check("galaxy_view_draw", 0x002033c0, 60, Ret::None, [menu, marks] {
        menu(0xf);
        if (ri(0, 3) != 0) marks();
        w32(OV(0x00184414), u(ri(0, 18)));
        w32(OV(0x0015f3f8), R());
        const int x = ri(0, 300), y = ri(0, 300);
        return Args{{u(x), u(x + ri(1, 300)), u(y), u(y + ri(1, 200))}, {}};
    }, [](A a) { (void)galaxy_view_draw_u(a.i[0], a.i[1], a.i[2], a.i[3], 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f); return uint64_t{0}; });
    check("galmap_planet_draw", 0x0027cbc0, 80, Ret::Int32, [menu, bolts] {
        menu(0x10);
        if (ri(0, 3) != 0) bolts();
        w32(OV(0x00184414), u(ri(0, 18)));
        w32(OV(0x0015f3f8), R());
        return Args{{OV(0x001b65b8)}, {}};
    }, [](A a) { return ri32(galmap_planet_draw(a.i[0])); });

    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
