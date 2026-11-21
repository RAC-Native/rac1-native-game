// Oracle batch 75: the ship sequences of game mode 6 (the planet arrival / departure 0x290590, its
// start 0x28e9c8 and their callees) on Veldin's real level data (needs the ISO).

#include "ovl_oracle.h"

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/arrival.h>
#include <game/lib/libcdvd.h>
#include <game/lib/libvu0.h>
#include <runtime/kernel/disc.h>

namespace rac1::oracle {

void batch_75() {
    if (g_only_batch != 75) return;
    g_batch = 75;
    g_ignored_words = vcross_lanes();
    // the callees left TODO_PORT: the kernel's vsync wait, the ship's flight's drawing; the
    // console's clock (the level change request's) the host's on both sides
    g_extra_noop_hooks = {0x00122298, OVF(0x002900a0)};
    g_host_clock_fixed = 1759658400;
    g_extra_hooks = {{0x00121780, [](EeCpu& c) { c.set32(2, cd_read_clock(c.r32(4))); }},
                     {0x0012d6d8, [](EeCpu& c) { clock_to_local(c.r32(4)); }}};
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    // 989snd's IOP server as main()'s (replies a function of the command; when `none_zero`
    // every request succeeds and no sound plays any more (0x19: 0)), and the stream system's
    // disc reads done as the IRX does them (command 0x38: the sectors read into EE RAM, the
    // status words 0x137b10 / 0x137b00 cleared): the scene reads complete on both sides
    auto snd_server = [](bool reads, bool none_zero = false) {
        auto result = [none_zero](uint32_t id, uint32_t a0) -> uint32_t {
            const uint32_t h = (id * 0x9e3779b1u) ^ (a0 * 0x85ebca6bu);
            if (none_zero) return id == 0x19 ? 0u : ((h >> 3) | 1u);
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
    auto level = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty()) {
            veldin_boot();
            ee_write<uint16_t>(0x001518d0, 512);  // the display size (the host's, from the boot module)
            ee_write<uint16_t>(0x001518d2, 416);
            particle_table_init();
            level_setup_body(1, 0);
            hud_init();
            w32(OV(0x0016c060), 0xf);
            gameplay_frame();
            w32(0x0015ec8c, 1);  // the stream system up (the disc reads by command 0x38), the disc idle
            w32(0x00137b00, 0);
            sub_00264bf8();  // its callback 0x266168 (the streams' start, 0x264b68)
            snap.assign(ram, ram + 0x02000000);
            // the class update functions not ported yet: no-ops on both sides
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); ee_read<uint8_t>(m + 0x20) != 0xff; m += 0x100) {
                const uint32_t fn = ee_read<uint32_t>(m + 0x74);
                if (fn != 0 && !ovl_moby_fn_ported(fn)) g_extra_noop_hooks.push_back(fn);
            }
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    static const uint32_t SC = OV(0x0016c860), G = 0x0013e030, P = 0x0013f350, S = 0x001516d0;
    // the scene buffers 0x60000 below *(0x1611cc) (as 0x28e9c8 lays them out) and scene `k` read
    // from the disc into the first (the stream read the tests' 989snd server fakes: the same on
    // both sides); its blocks' table when `table`
    auto scene_load = [](int k, bool table) {
        const uint32_t base = ee_read<uint32_t>(OV(0x001611cc)) - 0x60000u;
        w32(SC + 0x5c, ee_read<uint32_t>(OV(0x00173e08)) + base);
        w32(SC + 0x58, ee_read<uint32_t>(OV(0x00173e04)) + base);
        w32(OV(0x001611cc), base);
        const uint32_t e = 0x00137b80 + static_cast<uint32_t>(k) * 8;
        const uint32_t buf = ee_read<uint32_t>(SC + 0x5c);
        disc_read_ee(buf, ee_read<uint32_t>(e + 0x12e8), ee_read<uint32_t>(e + 0x12ec));
        if (!table) return;
        for (uint32_t i = 0; i < 0x46 && ee_read<uint32_t>(buf + i * 8 + 4) != 0; ++i) w32(SC + 0x60 + i * 4, buf + ee_read<uint32_t>(buf + i * 8) + 0x800);
    };
    // ---- libvu0 (the base ELF): the unit matrix and the rotations about x / y / z
    auto rot = [level] {
        level();
        wfrand(M, 16, -3, 3);
        const int k = ri(0, 4);
        const float a = k == 0 ? 0.0f : k == 1 ? rf(-0.01f, 0.01f) : k == 2 ? rf(-7, 7) : rf(-3.2f, 3.2f);
        return Args{{coin() ? M : M2, M}, {a}};
    };
    check("vu0_unit_matrix", 0x00125218, 8, Ret::None, [level] { level(); wrand(M, 16); return Args{{M}, {}}; },
          [](A a) { vu0_unit_matrix(MM(a.i[0])); return uint64_t{0}; });
    check("vu0_rot_matrix_z", 0x001252b8, N, Ret::None, rot, [](A a) { vu0_rot_matrix_z(MM(a.i[0]), MM(a.i[1]), a.f[0]); return uint64_t{0}; });
    check("vu0_rot_matrix_x", 0x00125360, N, Ret::None, rot, [](A a) { vu0_rot_matrix_x(MM(a.i[0]), MM(a.i[1]), a.f[0]); return uint64_t{0}; });
    check("vu0_rot_matrix_y", 0x00125408, N, Ret::None, rot, [](A a) { vu0_rot_matrix_y(MM(a.i[0]), MM(a.i[1]), a.f[0]); return uint64_t{0}; });
    // ---- the scene file
    check("stream_reads_wait", 0x002660c0, 30, Ret::Int32, [level] {
        level();
        w16(S + 8, 0);
        return Args{{u(ri(0, 1))}, {}};
    }, [](A a) { return ri32(stream_reads_wait(static_cast<int32_t>(a.i[0]))); });
    auto blocks = [level, scene_load] {
        level();
        w16(G + 0x26, u(ri(0, 2)));
        w32(OV(0x0015f5c4), coin() ? 6u : 0u);
        scene_load(ri(0, 9), true);
        if (coin()) {  // a block decoded before: its mobys there
            w32(SC + 0x5c, ee_read<uint32_t>(SC + 0x60));
            scene_block_decode();
        }
    };
    auto nblocks = [] {
        int32_t n = 0;
        while (n < 0x46 && ee_read<uint32_t>(SC + 0x60 + u(n) * 4) != 0) ++n;
        return n;
    };
    check("scene_block_decode", 0x00245270, 30, Ret::None, [blocks, nblocks] {
        blocks();
        w32(SC + 0x5c, ee_read<uint32_t>(SC + 0x60 + u(ri(0, nblocks() - 1)) * 4));
        return Args{};
    }, [](A) { scene_block_decode(); return uint64_t{0}; });
    check("scene_block_load", 0x00245720, 30, Ret::None, [blocks, nblocks] {
        blocks();
        return Args{{u(ri(0, nblocks() - 1))}, {}};
    }, [](A a) { scene_block_load(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    check("scene_read", 0x00245610, 30, Ret::None, [level, scene_load] {
        level();
        const int k = ri(0, 9);
        scene_load(k, false);
        w16(S + 8, 0);
        return Args{{u(k), coin() ? 0u : u(ri(1, 8))}, {}};
    }, [](A a) { scene_read(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
    // ---- the mobys of the sequences
    auto ship = [level] {
        level();
        w16(G + 0x26, u(ri(0, 2)));
        return ee_read<uint32_t>(G) != 0 ? ee_read<uint32_t>(G) : ee_read<uint32_t>(0x001413d0);
    };
    check("ship_thrusters", 0x002903c8, 30, Ret::None, [ship] { const uint32_t m = ship(); seed_rand(); return Args{{m}, {rf(-3, 0)}}; },
          [](A a) { ship_thrusters(a.i[0], a.f[0]); return uint64_t{0}; });
    auto some_moby = [level] {  // a moby of the level, or the hero's
        level();
        std::vector<uint32_t> ms;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); ee_read<uint8_t>(m + 0x20) != 0xff; m += 0x100) ms.push_back(m);
        return coin() || ms.empty() ? ee_read<uint32_t>(0x001413d0) : ms[static_cast<size_t>(ri(0, static_cast<int32_t>(ms.size()) - 1))];
    };
    check("clank_ship_update", 0x00207430, 40, Ret::None, [some_moby] {
        some_moby();
        const uint32_t m = ee_read<uint32_t>(0x001413d0);
        const int k = ri(0, 3);
        w16(m + 0xa6, k == 0 ? 10u : k == 1 ? 0x1a3u : k == 2 ? 0x555u : 0u);
        w32(OV(0x0015f5cc), R() & 0xffff);
        w8(0x0015edb3, R() & 1);
        for (uint32_t n : {OV(0x0017c380u), OV(0x0017c3c0u), OV(0x0017c400u)}) w8(n + 1, ri(0, 3) == 0 ? 1u : 0u);
        if (coin()) wf(m + 0x18, ee_read<float>(m + 0x18) + rf(-4, 4));
        return Args{{m}, {}};
    }, [](A a) { clank_ship_update(a.i[0]); return uint64_t{0}; });
    {
        const CollisionScratchIgnored spr_ignored;
        check("moby_shadow_probe", 0x0025a1e0, 60, Ret::None, [some_moby] {
            const uint32_t m = some_moby();
            if (coin()) wf(m + 0xc, rf(0.5f, 8));
            return Args{{m}, {}};
        }, [](A a) { moby_shadow_probe(a.i[0]); return uint64_t{0}; });
    }
    check("hero_cam_target_set", 0x00233ee8, 30, Ret::None, [level] {
        level();
        wfrand(M, 4, -100, 100);
        w32(P + 0x2084, ri(0, 2) == 0 ? u(ri(0x64, 0x68)) : 0u);
        return Args{{M, R() & 3}, {rf(-2, 2)}};
    }, [](A a) { hero_cam_target_set(a.i[0], a.f[0], a.i[1]); return uint64_t{0}; });
    {
        const CollisionScratchIgnored spr_ignored;
        check("hero_ship_floor_fix", 0x00234d40, 40, Ret::None, [level] {
            level();
            wfrand(P, 3, -0.5f, 0.5f);
            if (coin()) wf(P + 0x88, ee_read<float>(P + 0x88) + rf(-1, 1));
            return Args{};
        }, [](A) { hero_ship_floor_fix(); return uint64_t{0}; });
    }
    auto scene_mobys = [level, scene_load] {  // a scene's mobys spawned (a block decoded)
        level();
        w16(G + 0x26, u(ri(0, 2)));
        w32(OV(0x0015f5c4), coin() ? 6u : 2u);
        scene_load(ri(0, 9), true);
        w32(SC + 0x5c, ee_read<uint32_t>(SC + 0x60));
        scene_block_decode();
        if (coin()) w32(0x0015ed84, ri(0, 3) == 0 ? 2u : ri(0, 2) == 0 ? 0xdu : 9u);
    };
    check("hero_ship_parts", 0x00234a50, 30, Ret::None, [scene_mobys] {
        scene_mobys();
        if (ri(0, 3) == 0) w32(OV(0x0015f5c4), 0);
        return Args{{ee_read<uint32_t>(0x001413d0)}, {}};
    }, [](A a) { hero_ship_parts(a.i[0]); return uint64_t{0}; });
    check("hero_ship_update", 0x00234b38, 30, Ret::None, [scene_mobys] {
        scene_mobys();
        wf(0x0015ee14, rf(0.5f, 2));
        w8(OV(0x0017c341), ri(0, 2) == 0 ? 1u : 0u);
        return Args{{ee_read<uint32_t>(0x001413d0)}, {}};
    }, [](A a) { hero_ship_update(a.i[0]); return uint64_t{0}; });
    check("hero_backpack_attach", 0x00234c78, 20, Ret::None, [level] {
        level();
        const uint32_t pack = moby_spawn(0x1b1);
        if (coin()) w16(pack + 0xa6, 0x509);
        return Args{{ee_read<uint32_t>(0x001413d0), pack}, {}};
    }, [](A a) { hero_backpack_attach(a.i[0], a.i[1]); return uint64_t{0}; });
    check("camera_scripted_set", 0x002eaaa0, 30, Ret::None, [level] {
        level();
        wfrand(M, 4, -200, 200);
        wfrand(M2, 4, -3, 3);
        return Args{{M, M2, u(ri(0, 4)), R(), R()}, {}};
    }, [](A a) { camera_scripted_set(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), a.i[3], a.i[4]); return uint64_t{0}; });
    check("hero_hold_clear", 0x002118b0, 4, Ret::None, [level] { level(); wrand(P + 0x190, 2); return Args{}; }, [](A) { hero_hold_clear(); return uint64_t{0}; });
    check("hero_anim_set", 0x002325a0, 20, Ret::None, [level] { level(); return Args{{u(ri(0, 20)), u(ri(0, 30))}, {}}; },
          [](A a) { hero_anim_set(a.i[0], a.i[1]); return uint64_t{0}; });
    check("moby_light_zone_pick", 0x002500b8, 40, Ret::None, [some_moby] {
        const uint32_t m = some_moby();
        if (coin() && ee_read<int32_t>(OV(0x00160014)) > 0) {  // near a zone
            const uint32_t z = ee_read<uint32_t>(OV(0x00160040)) + u(ri(0, ee_read<int32_t>(OV(0x00160014)) - 1)) * 0x30;
            for (uint32_t k = 0; k < 3; ++k) wf(m + 0x10 + k * 4, ee_read<float>(z + k * 4) + rf(-5, 5));
        }
        w16(S + 0x3e, ri(0, 3) == 0 ? 1u : 0u);
        if (coin()) w32(S + 0x20, 0x00ff0000u | (R() & 0xff000000u));
        return Args{{m}, {}};
    }, [](A a) { moby_light_zone_pick(a.i[0]); return uint64_t{0}; });
    {
        const CollisionScratchIgnored spr_ignored;
        check("hero_place", 0x00216f90, 30, Ret::None, [level] {
            level();
            for (uint32_t k = 0; k < 3; ++k) wf(M + k * 4, ee_read<float>(P + 0x80 + k * 4) + rf(-3, 3));
            wfrand(M2, 3, -3, 3);
            return Args{{M, M2, coin() ? 0xffffffffu : u(ri(0, 3)), u(ri(0, 1))}, {}};
        }, [](A a) { hero_place(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), static_cast<int32_t>(a.i[3])); return uint64_t{0}; });
    }
    // ---- the sequences
    // the class update functions of the mobys spawned since the snapshot (the scene's): no-ops on
    // both sides when not ported
    auto hook_new_mobys = [] {
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); ee_read<uint8_t>(m + 0x20) != 0xff; m += 0x100) {
            const uint32_t fn = ee_read<uint32_t>(m + 0x74);
            if (fn != 0 && !ovl_moby_fn_ported(fn) && std::find(g_extra_noop_hooks.begin(), g_extra_noop_hooks.end(), fn) == g_extra_noop_hooks.end())
                g_extra_noop_hooks.push_back(fn);
        }
    };
    // the sequence's sound ready (the dialog slot in state 3 with a handle: the start's wait loop
    // not entered, 989snd's replies being the tests' server's)
    auto sound_ready = [] {
        w16(S + 0x5a, 3);
        w32(S + 0x50, (R() & 0x7ffffffeu) | 1u);
        w16(S + 0x5c, 0);  // not paused
        w32(S + 0x1c, 0xffffffffu);  // no line asked for (as the streams' start leaves it)
    };
    auto setup = [level] {
        level();
        w16(G + 0x26, u(ri(0, 2)));
        w16(G + 0x2c, ri(0, 3) == 0 ? 1u : 0u);
        w16(G + 0x28, ri(0, 4) == 0 ? 1u : 0u);
        const int k = ri(0, 5);
        w32(0x0015ed84, k == 0 ? 0xau : k == 1 ? 0xeu : k == 2 ? 0xdu : 0u);
        w8(0x0013d4c6, R() & 1);
        w32(0x0015ed80, 0);
    };
    check("ship_sequence_start", 0x0028e9c8, 30, Ret::None, [setup, sound_ready] {
        setup();
        sound_ready();
        return Args{{coin() ? 8u : 0u}, {}};
    }, [](A a) { ship_sequence_start(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    // a landing / take-off under way: started, then at a random frame of its scene (its block
    // loaded as 0x28ed58 does), the fade, the buttons
    auto running = [setup, sound_ready, hook_new_mobys](uint32_t step) {
        setup();
        sound_ready();
        ship_sequence_start(static_cast<int32_t>(step));
        const int32_t total = ee_read<int16_t>(SC + 0x40);
        const int k = ri(0, 4);
        int32_t f = k == 0 ? ri(0, 40) : k == 1 ? ri(total - 40, total + 2) : ri(0, total + 2);
        if (f < 0) f = 0;
        const int32_t blk = f / 0x60;
        uint32_t n = 0;
        while (n < 0x46 && ee_read<uint32_t>(SC + 0x60 + n * 4) != 0) ++n;
        if (static_cast<uint32_t>(blk) < n) {
            w32(SC + 0x34, u(f));
            w32(SC + 0x3c, u(blk));
            scene_block_load(blk);
            w32(SC + 0x38, u(f % 0x60));
        }
        w16(G + 0x24, u(ri(0, 400)));
        wf(OV(0x0015f3fc), ri(0, 2) == 0 ? 0.0f : rf(0, 1));
        wf(OV(0x0015f400), rf(-0.1f, 1.1f));
        w32(0x0013cae4, ri(0, 3) == 0 ? 0x50u : 0u);
        w32(OV(0x0015f5d8), 0);
        hook_new_mobys();
    };
    {
        const CollisionScratchIgnored spr_ignored;
        check("ship_sequence_update_8", 0x00290590, 60, Ret::None, [running] { running(8); return Args{}; },
              [](A) { ship_sequence_update(); return uint64_t{0}; });
        check("ship_sequence_update_0", 0x00290590, 60, Ret::None, [running] { running(0); return Args{}; },
              [](A) { ship_sequence_update(); return uint64_t{0}; });
        // the flight (step 3): a path of 6..12 points at T for 0x1b04b0[0] (the table 0x2464a8 fills,
        // not ported), the level cameras' blend, the riders
        check("ship_sequence_update_3", 0x00290590, 60, Ret::None, [setup, sound_ready, hook_new_mobys] {
            setup();
            sound_ready();
            const int32_t n = ri(6, 12);
            w32(T, u(n));
            float x = rf(-50, 50), y = rf(-50, 50);
            for (int32_t k = 0; k < n; ++k) {
                x += rf(-20, 20);
                y += rf(-20, 20);
                wf(T + 0x10 + u(k) * 0x10, x);
                wf(T + 0x14 + u(k) * 0x10, y);
                wf(T + 0x18 + u(k) * 0x10, rf(0, 60));
                wf(T + 0x1c + u(k) * 0x10, 0.0f);
            }
            w32(OV(0x001b04b0), T);
            w32(G + 0x20, 3);
            w32(G + 0x30, ri(0, 3) == 0 ? 0xffffffffu : 0u);
            const int32_t first = ri(0, 2);
            w16(G + 0x24, first == 0 ? 0xffffu : u(ri(0, 300)));
            const int32_t ncam = ee_read<int32_t>(OV(0x001600f0));
            w32(G + 0x34, ri(0, 2) == 0 || ncam <= 0 ? 0xffffffffu : u(ri(0, ncam - 1)));
            w32(G + 0x38, ri(0, 2) == 0 || ncam <= 0 ? 0xffffffffu : u(ri(0, ncam - 1)));
            wf(G + 0x48, rf(0, 1));
            wf(G + 0x4c, rf(-0.1f, 0.1f));
            if (first != 0 && ee_read<int32_t>(G + 0x30) >= 0) {  // the riders and the path set up as the first frame does
                ee_write<int16_t>(G + 0x24, -1);
                ship_sequence_update();
                w16(G + 0x24, u(ri(0, 300)));
                wf(G + 0x3c, rf(0, static_cast<float>(n)));
                wf(G + 0x40, rf(0, 110));
            }
            wf(OV(0x0015f3fc), ri(0, 2) == 0 ? 0.0f : rf(0, 1));
            w32(0x0013cae4, ri(0, 3) == 0 ? 0x50u : 0u);
            w32(OV(0x0015f5d8), 0);
            w32(OV(0x0015f570), 0);
            hook_new_mobys();
            return Args{};
        }, [](A) { ship_sequence_update(); return uint64_t{0}; });
    }
    check("ship_sequence_update_x", 0x00290590, 20, Ret::None, [level] {  // the other steps: the count only
        level();
        static const uint32_t st[] = {1, 2, 5, 6, 7, 9, 0x20, 0xffffffffu};
        w32(G + 0x20, st[ri(0, 7)]);
        w16(G + 0x24, R());
        return Args{};
    }, [](A) { ship_sequence_update(); return uint64_t{0}; });
    // the drawing: its dispatch (a frame cut short, steps that draw nothing); the frame itself
    // (0x1f85b8, 0x200fd8) is not compared
    check("ship_sequence_draw", 0x00291d00, 20, Ret::None, [level] {
        level();
        const bool cut = coin();
        w32(OV(0x0015f5d8), cut ? 1u : 0u);
        static const uint32_t any[] = {0, 3, 4, 7, 8}, none[] = {1, 2, 5, 6, 9, 0x40};
        w32(G + 0x20, cut ? any[ri(0, 4)] : none[ri(0, 5)]);
        return Args{};
    }, [](A) { ship_sequence_draw(); return uint64_t{0}; });
    check("mode2_draw", 0x001f8e90, 4, Ret::None, [level] { level(); w32(OV(0x0015f5d8), 1); return Args{}; }, [](A) { mode2_draw(); return uint64_t{0}; });
    check("planet_travel", 0x0028ed58, 30, Ret::None, [setup, sound_ready, hook_new_mobys] {
        setup();
        sound_ready();
        const int32_t lv = ee_read<int32_t>(0x0015ed84);
        return Args{{coin() ? u(lv) : u(ri(0, 0x12))}, {}};
    }, [](A a) { planet_travel(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    snd_server(true, true);  // the take-off stops the dialog: the start's wait loop entered, the new one started
    check("ship_takeoff_start", 0x0028ecd8, 20, Ret::None, [setup, sound_ready] {
        setup();
        sound_ready();
        w32(P + 0x22a8, u(ri(-1, 2)));
        return Args{};
    }, [](A) { ship_takeoff_start(); return uint64_t{0}; });
    check("ship_sequence_start_wait", 0x0028e9c8, 20, Ret::None, [setup] {  // the sound awaited (started anew)
        setup();
        w16(S + 0x5c, 0);
        w32(S + 0x1c, 0xffffffffu);
        w16(S + 0x5a, 0);
        w32(S + 0x50, 0);
        return Args{{coin() ? 8u : 0u}, {}};
    }, [](A a) { ship_sequence_start(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    check("start_mode_select", 0x0028eed0, 30, Ret::None, [setup] {  // with the planet arrival's start
        setup();
        w16(S + 0x5c, 0);
        w32(S + 0x1c, 0xffffffffu);
        w16(S + 0x5a, 0);
        w32(S + 0x50, 0);
        w32(0x0015ed84, u(ri(0, 0x13)));
        w8(0x0013dd43, R() & 1);
        w8(0x0013d3f0, R() & 1);
        w16(0x0013e05a, coin() ? 0u : R());
        w16(0x0013e05e, R());
        return Args{};
    }, [](A) { start_mode_select(); return uint64_t{0}; });
    snd_server(false);
    g_extra_hooks.clear();
    g_host_clock_fixed = 0;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
