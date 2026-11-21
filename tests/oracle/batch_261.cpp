// Oracle batch 261 (levels 17 / 18 port, 0.3.0): the hero's own builds of Drek's fleet (level 17: veldin_orbit_hero_tr_*.cpp) and Veldin's
// finale (level 18: veldin_finale_hero_tr_*.cpp) on their real level data, as batch 91 checks Novalis's: the port plays a number of frames of
// the level from its start with scripted pad input (walking, jumping, attacking, crouching), then the one frame compared is the hero's update
// or a piece of it, run by the original (the level's own function, through the variants tables) and the port on the same memory. Run with
// ORACLE_LEVEL=17 / 18 build/tests/ovl_oracle --batch 261 (needs the ISO). Batch 91's own checks are this batch's (the level's first frame
// with the sound listener's cached points refreshed, as the game has it: level 18's first frame otherwise overruns the scratchpad, task 0323).
// The hero unit batches 44 .. 55 run on both levels too.

#include "ovl_oracle.h"

#include <game/ovl/ovl_reloc.h>
namespace rac1::oracle {

void batch_261() {
    if (g_only_batch != 261) return;
    auto boot = [] { veldin_boot(); };
    g_batch = 261;
    if (g_oracle_level != 17 && g_oracle_level != 18) {
        skip_check("hero/real (levels 17 / 18)", 0x002070d0, "levels 17 and 18 only (ORACLE_LEVEL=17 / 18): batch 91 checks the other levels");
        return;
    }
    g_ignored_words = vcross_lanes();
    g_extra_noop_hooks = {OVF(0x001f3aa0), OVF(0x001f4490)};  // (the screens, ported: task 0301)
    g_ignored_words.push_back(OV(0x00160188));                 // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime

    static std::vector<uint8_t> snap;
    auto level = [boot] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty()) {
            boot();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            w32(0x0013f2e8, 0xffffffffu);  // (the sound listener's cached points refreshed on the first frame, as in the game: task 0323)
            snap.assign(ram, ram + 0x02000000);
            // the class update functions not ported yet: no-ops on both sides
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); ee_read<uint8_t>(m + 0x20) != 0xff; m += 0x100) {
                const uint32_t fn = ee_read<uint32_t>(m + 0x74);
                if (fn != 0 && !ovl_moby_fn_ported(fn)) g_extra_noop_hooks.push_back(fn);
            }
            for (uint32_t fn : {OVF(0x002a84c8u), OVF(0x002d89f0u)}) if (!ovl_moby_fn_ported(fn)) g_extra_noop_hooks.push_back(fn);
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    // a pad packet: held buttons (high byte: select / start / d-pad, low byte: L2 R2 L1 R1 triangle
    // circle cross square), the left stick
    auto pad = [](uint16_t held, uint8_t lx, uint8_t ly) {
        const uint16_t low = static_cast<uint16_t>(~held);
        const uint8_t pk[6] = {static_cast<uint8_t>(low >> 8), static_cast<uint8_t>(low & 0xff), 0x7f, 0x7f, lx, ly};
        pad_read_packet(0x0013c940, pk, 6);
    };
    // a script: the frames from the start of the level, the input of each
    struct Script { int walk_from, walk_to; uint8_t lx, ly; int jump_every, jump_len; int attack_every; bool r1; };
    auto input = [pad](const Script& s, int f) {
        uint16_t held = 0;
        uint8_t lx = 0x7f, ly = 0x7f;
        if (f >= s.walk_from && f < s.walk_to) { lx = s.lx; ly = s.ly; }
        if (s.jump_every != 0 && f >= 20 && (f % s.jump_every) < s.jump_len) held |= 0x0040;
        if (s.attack_every != 0 && f >= 20 && (f % s.attack_every) < 3) held |= 0x0080;
        if (s.r1 && f >= 40) held |= 0x0008;
        pad(held, lx, ly);
    };
    auto script = []() -> Script {
        Script s{};
        s.walk_from = ri(0, 3) == 0 ? 0 : ri(5, 30);
        s.walk_to = s.walk_from + ri(20, 140);
        s.lx = ri(0, 2) == 0 ? 0x7f : static_cast<uint8_t>(ri(0, 255));
        s.ly = ri(0, 3) == 0 ? 0x7f : static_cast<uint8_t>(ri(0, 255));
        s.jump_every = ri(0, 2) == 0 ? 0 : ri(15, 70);
        s.jump_len = ri(2, 12);
        s.attack_every = ri(0, 3) == 0 ? ri(25, 90) : 0;
        s.r1 = ri(0, 7) == 0;
        return s;
    };
    auto play = [level, input, script] {
        level();
        const Script s = script();
        const int n = ri(30, 170);
        for (int f = 0; f < n; ++f) {
            input(s, f);
            gameplay_frame();
            w32(OV(0x0015f3f8), ee_read<uint32_t>(OV(0x0015f3f8)) + 1);
        }
        input(s, n);
    };
    {
    // (stage 5, task C2c) hero_frame_update/real reaches segment_collide: A2's collision scratch
    // ranges left out of this check (see batch 43's checks of it)
    const CollisionScratchIgnored spr_ignored;
    check("hero_frame_update/real", 0x002070d0, 60, Ret::None, [play] { play(); return Args{}; },
          [](A) { hero_frame_update(); return uint64_t{0}; }, OV(0x00173e7c));
    }
    {
    // (stage 5, task C2c) hero_input_update/real reaches segment_collide: A2's collision scratch
    // ranges left out of this check (see batch 43's checks of it)
    const CollisionScratchIgnored spr_ignored;
    check("hero_input_update/real", 0x00229b70, 40, Ret::None, [play] { play(); return Args{}; },
          [](A) { hero_input_update(); return uint64_t{0}; }, OV(0x00173e7c));
    }
    {
    // (stage 5, task C2b) hero_state_update/real reaches sphere_collide or capsule_collide: A2's collision scratch
    // ranges left out of this check (see batch 43's checks of it)
    const CollisionScratchIgnored spr_ignored;
    check("hero_state_update/real", 0x00217970, 40, Ret::None, [play] { play(); return Args{}; },
          [](A) { hero_state_update(); return uint64_t{0}; }, OV(0x00173e7c));
    }
    check("hero_timers_update/real", 0x002215c8, 40, Ret::None, [play] { play(); return Args{}; },
          [](A) { hero_timers_update(); return uint64_t{0}; }, OV(0x00173e7c));
    // the mode exit and the idle entry (no check of their own in the other batches): the hero in mode 0 or 3
    // after a few frames of play, the riding moby slots empty (mode 3's 0x27a moby needs a spawn the
    // real level has room for)
    check("hero_mode_exit/real", 0x00210b30, 30, Ret::None, [play] {
        play();
        w8(g::PLAYER + 0x20a4, ri(0, 1) ? 3u : 0u); w32(g::PLAYER + 0xa84, 0); w32(g::PLAYER + 0xa8c, 0);
        return Args{};
    }, [](A) { hero_mode_exit(); return uint64_t{0}; }, OV(0x00173e7c));
    check("hero_enter_idle/real", 0x00227638, 30, Ret::None, [play] {
        play();
        w8(g::PLAYER + 0x20a4, ri(0, 1) ? 3u : 0u);
        return Args{};
    }, [](A) { hero_enter_idle(); return uint64_t{0}; }, OV(0x00173e7c));

    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
