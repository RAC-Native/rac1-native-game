// Oracle batch 85 (task 0301): the story's picture cards and movies. The card's images
// (0x292690: the card file read from the disc, unpacked, uploaded to the GS; the three TEX0s),
// its sprite (0x292500), the whole card (0x2929f0: its frames run to the end on both sides, with
// the next level's load), the movie by number (0x292420) and the player's entry (0x2edd28), with
// the movie player itself (0x2ee130 / 0x2eddd0 / 0x2ee3d8: Sony's sample player, threads and IPU)
// the host's stand-in on both sides, movies off (a movie that cannot be opened). The memory card's
// machines (0x24b690, 0x24ac20 and its handlers; libmc the host's stand-in, no card), a new game's
// state (0x24b628: the disc's default save read into the game), game mode 4's screens (0x201730,
// 0x202aa0, 0x2018c8, 0x1faea0).

// Audit 2 (2026-11-13): screen_draw draws every kind (0, 2, 5 and 6 were never drawn: the race's result, the message, the
// disc's spinner, the choice's steps), the race's state, place and records varied; game mode 4's drawing 0x1f8ec8 checked (its
// skip flag, the dim, the screen; not its gameplay / menu arms, see its check).

#include "ovl_oracle.h"

#include <game/ovl/ovl_reloc.h>
#include <game/lib/libcdvd.h>
#include <game/lib/libmc.h>
#include <game/lib/movie_player.h>
#include <game/ovl/memory_card.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/level_change.h>
#include <game/ovl/movies.h>
#include <runtime/ipu/movie_hw.h>
#include <runtime/kernel/disc.h>

namespace rac1::oracle {

void batch_85() {
    if (g_only_batch != 85) return;
    g_batch = 85;
    g_ignored_words = vcross_lanes();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;
    movie_hw_enable(false);
    // the disc's reads (libcdvd) the host's on both sides
    g_host_clock_fixed = 1759658400;  // (the level change request's clock, batch 84's)
    g_extra_hooks = {{0x00121780, [](EeCpu& c) { c.set32(2, cd_read_clock(c.r32(4))); }},
                     {0x0012d6d8, [](EeCpu& c) { clock_to_local(c.r32(4)); }},
                     {0x00123d68, [](EeCpu& c) { c.set32(2, mc_get_info(static_cast<int32_t>(c.r32(4)), static_cast<int32_t>(c.r32(5)), c.r32(6), c.r32(7), c.r32(8))); }},
                     {0x00123c30, [](EeCpu& c) { c.set32(2, mc_sync(static_cast<int32_t>(c.r32(4)), c.r32(5), c.r32(6))); }},
                     {OVF(0x00266040), [](EeCpu& c) { c.set32(2, disc_read_wait(c.r32(4), c.r32(5), c.r32(6))); }},  // (the host's disc read on both sides)
                     {0x00121450, [](EeCpu& c) { c.set32(2, cd_read(c.r32(4), c.r32(5), c.r32(6), c.r32(7))); }},
                     {0x00121630, [](EeCpu& c) { c.set32(2, cd_get_error()); }},
                     {0x001216c8, [](EeCpu& c) { c.set32(2, cd_break()); }},
                     {OVF(0x00267898), [](EeCpu&) { pad_update(); }},
                     // the movie player's entry points: the host's stand-ins
                     {OVF(0x002ee130), [](EeCpu& c) { c.set32(2, movie_player_open(c.r32(4), c.r32(5), static_cast<int32_t>(c.r32(6))) ? 1 : 0); }},
                     {OVF(0x002eddd0), [](EeCpu& c) {
                          movie_player_loop_start(c.r32(4), c.r32(5), c.r32(6));
                          while (!movie_player_loop_frame()) {
                          }
                          c.set32(2, 0);
                      }},
                     {OVF(0x002ee3d8), [](EeCpu&) { movie_player_close(); }},
                     // the player as the port runs it (0x292420's callee)
                     {OVF(0x002edd28), [](EeCpu& c) {
                          movie_stream_start(c.r32(4), c.r32(5), c.r32(6), c.r32(7), static_cast<int32_t>(c.r32(8)));
                          while (!movie_stream_frame()) {
                          }
                          c.set32(2, 0);
                      }}};
    // the callees left TODO_PORT: the kernel's vsync wait, thread priority and vsync callback, the
    // DMA kick, the card's machine, the state table, the GS display registers, the fade
    // (the level exit's: DMA handlers; its boot module read back 0x243d38 and the flight's drawing 0x2900a0 run on both
    // sides, ported: batches 420 and 143)
    g_extra_noop_hooks = {0x00122298, 0x001188e0, 0x001188a0, 0x00122e68, OVF(0x002a08c8), OVF(0x001f8540), OVF(0x0023af30),
                          0x001187d0, 0x001190f8};
    // 989snd's IOP server (batch 84's), the stream system's disc reads done
    auto result = [](uint32_t id, uint32_t a0) -> uint32_t {
        const uint32_t h = (id * 0x9e3779b1u) ^ (a0 * 0x85ebca6bu);
        return ((h >> 7) % 4 == 0) ? 0u : ((h >> 3) | 1u);
    };
    sif_rpc_register(0x123456, [result](uint32_t fno, uint32_t send, uint32_t, uint32_t recv, uint32_t) {
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
    // Veldin's level with the display size and the stream system up (batch 84's)
    static std::vector<uint8_t> snap;
    static const auto level = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty()) {
            veldin_boot();
            level_setup_body(1, 0);
            ee_write<uint16_t>(0x001518d0, 512);
            ee_write<uint16_t>(0x001518d2, 416);
            particle_table_init();
            hud_init();
            w32(OV(0x0016c060), 0xf);
            w32(0x0015ec8c, 1);
            w32(0x00137b00, 0);
            sub_00264bf8();
            sub_002a07f8();
            snap.assign(ram, ram + 0x02000000);
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    auto pick = [](std::initializer_list<uint32_t> l) { return *(l.begin() + ri(0, static_cast<int>(l.size()) - 1)); };

    check("card_images_load", 0x00292690, 6, Ret::None, [] {
        level();
        mc_reset();
        w32(0x0015ee8c, NOT_ADDR(0x280000u) + u(ri(0, 8)) * 0x2000u);
        const int32_t first = ri(0, 0x10), last = coin() ? first : ri(0, 0x10);
        return Args{{u(ri(0, 5)), u(first), u(last), OUT, OUT + 8, OUT + 0x10}};
    }, [](A a) {
        mc_reset();
        card_images_load(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]), a.i[3], a.i[4], a.i[5]);
        return uint64_t{0};
    });

    check("card_sprite", 0x00292500, N, Ret::None, [pick] {
        level();
        w32(0x0013e510, u(ri(0x6c00, 0x7400)));
        w32(0x0013e514, u(ri(0x6c00, 0x7400)));
        return Args{{u(ri(-8, 0x200)), u(ri(-0x40, 0x1c0)), u(ri(0, 0x200)), u(ri(0, 0x80)), coin() ? R() : u(ri(0, 0x80)) << 24 | 0x808080u, R()},
                    {rf(-1.0f, 1.0f), rf(0.0f, 4.0f), rf(0.0f, 2.0f), rf(0.0f, 2.0f)}};
    }, [](A a) {
        const uint64_t tex0 = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(a.i[5])));  // (the register as passed: sign-extended)
        card_sprite(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]), static_cast<int32_t>(a.i[3]),
                    static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(a.i[4]))), tex0, a.f[0], a.f[1], a.f[2], a.f[3]);
        return uint64_t{0};
    });

    // the whole card: its frames to the end (a short one), the card's state idle or busy (the
    // loop left at once), with the next level's load on the way (load 1: the card held until the
    // level's files and bank are in)
    check("card", 0x002929f0, 8, Ret::None, [pick] {
        level();
        mc_reset();
        const int32_t first = ri(0, 0x10), last = coin() ? first : ri(0, 0x10);
        w32(0x0013d290 + 0xd4, pick({2u, 2u, 2u, 3u}));
        w32(0x0013d290 + 0xdc, ri(0, 3) != 0 ? 0xffffffffu : u(ri(-1, 2)));
        w32(0x0015ed84, u(ri(0, 3)));
        w16(0x0013e056, 0);
        w32(0x0015ed5c, 0);
        return Args{{u(ri(0, 4)), u(first), u(last), u(ri(-2, 0x70)), u(ri(0, 2) == 0)}};
    }, [](A a) {
        mc_reset();
        card_start(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]), static_cast<int32_t>(a.i[3]),
                   static_cast<int32_t>(a.i[4]));
        for (int f = 0; f < 100000 && !card_frame(); ++f) {
        }
        return uint64_t{0};
    });

    check("movie", 0x00292420, 8, Ret::None, [] {
        level();
        mc_reset();
        w32(0x0015ed80, u(ri(0, 3) == 0));
        w32(0x0015ed88, u(ri(0, 5)));
        w8(0x0013e550 + 0x6b, R());
        return Args{{u(ri(0, 11))}};
    }, [](A a) {
        mc_reset();
        movie_start(static_cast<int32_t>(a.i[0]));
        for (int f = 0; f < 100000 && !movie_frame(); ++f) {
        }
        return uint64_t{0};
    });

    // the level exit (0x292e08, task 0300) on its story's paths: the cards and movies run to their
    // end on both sides (a new game's Veldin, Veldin -> Novalis: the last card loads the level; to
    // Eudora, from Umbris, to Oltanis Orbit, from Oltanis, to Kalebo III: then the loading screen)
    check("level_exit_story", 0x00292e08, 7, Ret::None, [] {
        level();
        mc_reset();
        for (uint32_t i = 0; i < 0x20; ++i) w8(0x0013dd40 + i, 0);
        static int k = 0;
        int32_t cur = ri(0, 18), next = ri(1, 18);
        switch (k++ % 7) {
            case 0: next = 0; break;
            case 1: cur = 0; next = 1; break;
            case 2: next = 4; break;
            case 3: cur = 7; w8(0x0013dd48, 1); break;
            case 4: next = 0xd; break;
            case 5: cur = 0xe; w8(0x0013dd4f, 1); break;
            default: next = 0x10; break;
        }
        w32(0x0015ed84, u(cur));
        w32(OV(0x0015f5c0), u(next));
        w32(0x0015ed88, u(ri(0, 5)));
        w32(0x0013d290 + 0xd4, 2);
        w32(0x0013d290 + 0xdc, 0xffffffffu);
        w32(OV(0x0015f5d8), 1);
        return Args{};
    }, [](A) {
        mc_reset();
        if (level_exit_start() != LevelExit::Quit)
            for (int f = 0; f < 40000 && !level_exit_frame(); ++f) {
            }
        return uint64_t{0};
    });
    // the segment records' w lanes (Veldin 0x1efc70's sqc2 of VU0 registers into 0x173e80..): the flight's drawing the exits
    // above ran leaves other VU0 registers in the interpreter than the port's, which the screens' segment tests below store
    // (as batches 142 / 160)
    for (uint32_t a = OV(0x00173e80); a < OV(0x00173fc0); a += 0x10) g_ignored_words.push_back(a + 0xc);


    // ---- the memory card (task 0301) ----
    constexpr uint32_t SAVE = 0x0013d290;
    static bool pend_on = false;
    static int32_t pend_cmd = 0, pend_res = 0;
    auto card_state = [pick] {
        w32(0x0015eeb0, u(ri(0, 0x18)));
        w32(0x0015eeb4, R() & 0x3ff);
        w32(OV(0x0015f5a8), u(ri(0, 1)));
        w32(SAVE + 0x1c, pick({0u, 0xffffffffu, u(-2), u(-10), u(-3), R()}));
        w32(SAVE + 0x14, pick({0u, u(-2), u(-3), u(-1), u(1)}));
        w32(SAVE + 0xc, u(ri(0, 0x300)));
        w32(SAVE + 0xd4, u(ri(0, 4)));
        w32(SAVE + 0xdc, coin() ? 0xffffffffu : u(ri(0, 9)));
        w32(SAVE + 0xe0, coin() ? 0xffffffffu : u(ri(0, 1)));
        w32(SAVE + 0xe4, u(ri(0, 1)) * R());
        w32(SAVE + 0xf4, u(ri(0, 1)));
        w32(SAVE + 0xbc, pick({0u, u(-10), u(-2), R()}));
        w32(OV(0x00161e00), u(ri(0, 100)));
        w32(OV(0x0015f5c4), pick({0u, 3u, 4u}));
    };
    check("card_ui_machine", 0x0024ac20, 200, Ret::None, [card_state] {
        level();
        mc_reset();
        card_state();
        return Args{};
    }, [](A) {
        mc_reset();
        card_ui_machine();
        return uint64_t{0};
    });
    check("card_machine", 0x0024b690, 120, Ret::None, [pick] {
        level();
        pend_on = coin();
        pend_cmd = ri(0, 3);
        pend_res = static_cast<int32_t>(pick({0u, u(-10), u(-2), u(-1)}));
        mc_set_pending(pend_on, pend_cmd, pend_res);
        w32(SAVE + 0xcc, u(ri(0, 3) == 0));
        w32(SAVE + 0xd4, pick({0u, 1u, 2u, 2u, 0x18u, 0x20u}));
        w32(SAVE + 0xc4, u(coin() ? ri(-2, 0) : ri(0x1c, 0x1f)));
        w32(SAVE + 0xdc, coin() ? 0xffffffffu : u(ri(3, 9)));
        w32(SAVE + 0xe0, coin() ? 0xffffffffu : u(ri(0, 1)));
        w32(SAVE + 0x1c, pick({0u, u(-10)}));
        w32(SAVE + 0xbc, pick({0u, u(-10), u(-2)}));
        for (int32_t k = 0; k < 2; ++k) {  // the two ports' records
            w32(SAVE + k * 0xb8, u(k));
            w32(SAVE + k * 0xb8 + 4, 0);
        }
        return Args{};
    }, [](A) {
        mc_set_pending(pend_on, pend_cmd, pend_res);
        card_machine();
        return uint64_t{0};
    });

    check("level_buffer_room", 0x00203358, 20, Ret::Int32, [] {
        level();
        return Args{{u(ri(0, 3) == 0 ? ri(0x20001, 0x40000) : ri(0, 0x20000)), OUT, OUT + 4}};
    }, [](A a) { return ri32(level_buffer_room(a.i[0], a.i[1], a.i[2])); });
    check("save_list_size", 0x0024cf40, 4, Ret::Int32, [pick] {
        level();
        return Args{{pick({OV(0x001845c0u), OV(0x001848c0u)})}};
    }, [](A a) { return r32(save_list_size(a.i[0])); });

    // a save block: the length and the checksum, the items (id, size, data) up to the id -1, as the
    // game writes them (0x24d030, ported: batch 84), then changed at random (sizes, ids, a word)
    static const auto block = [pick](uint32_t list, int32_t index) {
        const uint32_t dst = T3;
        save_block_write(dst, index, list);
        const int32_t len = static_cast<int32_t>(lwu(dst));
        // an item's id or data changed with the checksum made right again (a size changed would walk
        // the block past its end: the original does not check it); or a word changed (a wrong
        // checksum: rejected), or no checksum
        const int k = ri(0, 3);
        if (k == 0) w32(dst + 8, lwu(dst + 8) + 1);
        if (k == 1 && len > 24) w32(dst + 8 + 8 + u(ri(0, 1)) * 4, R());
        if (k <= 1) w32(dst + 4, save_checksum(dst + 8, len));
        if (k == 2 && len > 16) w32(dst + 8 + u(ri(0, len / 4 - 1)) * 4, R());
        if (k == 3 && ri(0, 1) == 0) w32(dst + 4, 0);
        (void)pick;
        return dst;
    };
    check("save_block_check", 0x0024cff0, 20, Ret::Int32, [] {
        level();
        return Args{{block(OV(0x001845c0), 0)}};
    }, [](A a) { return r32(save_block_check(a.i[0]) ? 1u : 0u); });
    check("save_block_read", 0x0024d1d8, 30, Ret::Int32, [pick] {
        level();
        const uint32_t list = pick({OV(0x001845c0u), OV(0x001848c0u)});
        const int32_t index = list == OV(0x001845c0u) ? 0 : ri(0, 19);
        const uint32_t src = block(list, index);
        w32(SAVE + 0xc4, u(ri(0, 1)));
        return Args{{src, u(index), list}};
    }, [](A a) { return ri32(save_block_read(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2])); });
    check("new_game_state", 0x0024b628, 2, Ret::None, [] {
        level();
        return Args{};
    }, [](A) {
        new_game_state();
        return uint64_t{0};
    });

    // ---- game mode 4's screens ----
    check("screen_open", 0x00201730, 30, Ret::None, [pick] {
        level();
        w32(OV(0x0015f5c4), pick({0u, 3u, 2u}));
        return Args{{u(ri(-1, 8)), R()}};
    }, [](A a) {
        screen_open(static_cast<int32_t>(a.i[0]), a.i[1]);
        return uint64_t{0};
    });
    auto screen_state = [pick, card_state] {
        card_state();
        static constexpr OvConst<0x00173040u> SB{};
        w32(SB, pick({3u, 3u, 3u, 0u, 1u, 2u, 4u, 5u, 6u, 9u}));
        w32(SB + 4, u(ri(0, 2)));
        w32(SB + 0x14, pick({0u, 3u, 2u}));
        w32(SB + 0x18, R());
        w32(SB + 0x1c, u(ri(-1, 4)));
        w32(SB + 0x20, u(ri(0, 3) == 0 ? ri(0, 0x80) : ri(0, 9)));
        w32(SB + 0x24, u(ri(0, 9)));
        w32(0x0013cae4, pick({0u, 0x40u, 0x10u, 0x20u, 0x800u, 0x50u}));
        w32(0x0015ed84, pick({0u, 1u, 5u, 0x10u, 3u}));
        w32(0x0013f350 + 0x880, coin() ? 0xffffffffu : u(ri(0, 3)));
        w32(0x0013f350 + 0x19c, u(ri(0, 0x2000)));
        w16(0x0013f350 + 0x89a, u(ri(0, 5)));
    };
    check("screen_update", 0x00202aa0, 120, Ret::None, [screen_state] {
        level();
        mc_reset();
        screen_state();
        return Args{};
    }, [](A) {
        mc_reset();
        screen_update();
        return uint64_t{0};
    });
    check("screen_frame", 0x001faea0, 20, Ret::None, [] {
        level();
        return Args{{u(ri(0, 0x100)), u(ri(0, 0x100)), u(ri(0, 0x200)), u(ri(0, 0x200)), R()}};
    }, [](A a) {
        screen_frame(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]), static_cast<int32_t>(a.i[3]),
                     static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(a.i[4]))));
        return uint64_t{0};
    });
    check("screen_draw", 0x002018c8, 200, Ret::None, [pick, screen_state] {
        level();
        screen_state();
        // (every kind: 0 the race's result (Aridia / Kalebo III), 2 a message, 3 the card's, 5 the disc's spinner, 6 the steps of
        // a choice, 1 / 4 / >= 7 the framed lines; before only 3, 1, 4 and 9 were drawn)
        w32(OV(0x00173040), pick({3u, 3u, 0u, 0u, 0u, 1u, 2u, 4u, 5u, 5u, 6u, 6u, 9u}));
        // the race's result: the HUD's race state 0x13fbea (below 3 the prompts, else the place and records), the place, the
        // best time / score (+4 on Kalebo III) equal to the race's or not
        w16(0x0013fbea, u(ri(0, 5)));
        w32(0x0013f350 + 0x8c0, u(ri(0, 6)));
        {
            const uint32_t off = ee_read<uint32_t>(0x0015ed84) == 0x10 ? 4u : 0u;
            const uint32_t t = u(ri(1, 40000)), sc = coin() ? 0u : u(ri(1, 9999));
            w32(0x0015ee58 + off, t);
            w32(0x0013fbe4, coin() ? t : u(ri(1, 40000)));
            w32(0x0015ee68 + off, sc);
            w32(0x0013fbf8, coin() ? sc : u(ri(0, 9999)));
            w32(0x0015ed80, coin() ? 1u : 0u);  // (the 50 Hz flag: the frames per minute)
        }
        w32(OV(0x00173040) + 0x1c, u(ri(-2, 5)));  // (kind 6's step)
        w32(OV(0x00173040) + 8, coin() ? 0u : static_cast<uint32_t>(sub_00203ad8(0x5229)));
        w32(OV(0x00173040) + 0xc, coin() ? 0u : static_cast<uint32_t>(sub_00203ad8(0x4ee0)));
        w32(OV(0x00173040) + 0x10, coin() ? 0u : static_cast<uint32_t>(sub_00203ad8(0x524a)));
        w32(OV(0x0015f3f8), u(ri(0, 1000)));
        return Args{};
    }, [](A) {
        screen_draw();
        return uint64_t{0};
    });

    // game mode 4's drawing 0x1f8ec8 (no check before): its skip flag, the dim and the screen over a mode that is not gameplay
    // (0: the whole frame's render 0x1f85b8, whose passes need the main loop's set-up, as batch 115 says) nor the full-screen
    // menu (3: menu_draw, its pages not open here): those two arms are not compared
    check("screen_draw_mode4", 0x001f8ec8, 60, Ret::None, [pick, screen_state] {
        level();
        screen_state();
        w32(OV(0x00173040), pick({3u, 0u, 1u, 2u, 4u, 5u, 6u}));
        w32(OV(0x00173040) + 0x14, pick({1u, 2u, 4u, 5u, 7u}));
        w32(OV(0x0015f5d8), ri(0, 4) == 0 ? 1u : 0u);
        w32(OV(0x0015f3f8), u(ri(0, 1000)));
        return Args{};
    }, [](A) {
        screen_draw_mode4();
        return uint64_t{0};
    });

    {  // the player's entry itself (its callee 0x2edd28 not hooked)
        auto hooks = g_extra_hooks;
        g_extra_hooks.pop_back();
        check("movie_stream", 0x002edd28, 6, Ret::Int32, [] {
            level();
            return Args{{u(ri(500000, 700000)), u(ri(1, 0x1000000)), T, T2, u(ri(0, 5))}};
        }, [](A a) {
            movie_stream_start(a.i[0], a.i[1], a.i[2], a.i[3], static_cast<int32_t>(a.i[4]));
            for (int f = 0; f < 100000 && !movie_stream_frame(); ++f) {
            }
            return uint64_t{0};
        });
        g_extra_hooks = hooks;
    }

    movie_hw_enable(true);
    mc_reset();
    g_host_clock_fixed = 0;
    g_extra_noop_hooks.clear();
    g_extra_hooks.clear();
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
