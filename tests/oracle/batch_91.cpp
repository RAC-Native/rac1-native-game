// Oracle batch 91 (task 0314): Novalis's own hero controller on its real level data (needs the ISO;
// run with ORACLE_LEVEL=1 build/tests/ovl_oracle --batch 91). The port plays a number of frames of
// the level from its start with scripted pad input (walking, jumping, attacking, crouching), then
// the one frame compared is the hero's update (hero_frame_update, Novalis's 0x228870) or a piece of
// it run by the original and the port on the same memory: the hero is in whatever state the
// frames before left it in, on the level's own collision, with its own camera and mobys around.
// On level 0 the same checks run on Veldin's code (the oracle's own proof that the dispatch to
// Novalis's variants leaves Veldin alone).

#include "ovl_oracle.h"

#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/disc.h>
namespace rac1::oracle {

void batch_91() {
    if (g_only_batch != 91) return;
    auto boot = [] { veldin_boot(); };
    g_batch = 91;
    g_ignored_words = vcross_lanes();
    g_extra_noop_hooks = {OVF(0x001f3aa0), OVF(0x001f4490)};  // (the screens, ported: task 0301)
    g_ignored_words.push_back(OV(0x00160188));                 // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // 989snd's IOP server (batch 80's) with the stream system's disc reads (command 0x38) done as the IRX does them: the
    // frames played may start a scene (Novalis's class 730, ported since the batch was written, starts scene 5), whose
    // blocks are read through the stream system, as in the game
    auto snd_server = [](bool reads) {
        auto result = [](uint32_t id, uint32_t a0) -> uint32_t {
            const uint32_t h = (id * 0x9e3779b1u) ^ (a0 * 0x85ebca6bu);
            return ((h >> 7) % 4 == 0) ? 0u : ((h >> 3) | 1u);
        };
        sif_rpc_register(0x123456, [result, reads](uint32_t fno, uint32_t send, uint32_t, uint32_t recv, uint32_t) {
            if (fno == 0x4d) {
                const uint32_t n = ee_read<uint32_t>(send);
                uint32_t p = send + 4;
                for (uint32_t i = 0; i < n; ++i) {
                    const uint32_t id = ee_read<uint16_t>(p), size = ee_read<uint16_t>(p + 2);
                    ee_write<uint32_t>(recv + 4 + i * 4, result(id, size != 0 ? ee_read<uint32_t>(p + 4) : 0u));
                    if (reads && id == 0x38) {
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
    };
    snd_server(true);

    static std::vector<uint8_t> snap;
    auto level = [boot] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty()) {
            boot();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            w32(0x0015ec8c, 1);  // the stream system up (the disc reads by command 0x38), the disc idle (batch 80's)
            w32(0x00137b00, 0);
            sub_00264bf8();
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
    check("hero_state_update/real", 0x00217970, 40, Ret::None, [play] { play(); return Args{}; },
          [](A) { hero_state_update(); return uint64_t{0}; }, OV(0x00173e7c));
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

    snd_server(false);
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
