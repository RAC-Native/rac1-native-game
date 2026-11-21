// Oracle batch 80: game mode 2 (the level's cutscenes: 0x298840 start, 0x298f90 frame, 0x298de8
// camera, 0x298b18 end, 0x2451b8 block read, 0x23e268 level extras), the talk scripts that start
// them (0x2667d0, 0x2668a0, 0x266448, 0x266970), the Veldin ship moby whose script opens the first
// visit with scene 4 (0x2d9dc8), the music's pause timers (0x265880), the level exit (0x28eeb0)
// and newlib strstr (0x1169c8), on Veldin's real level data (needs the ISO).

#include "ovl_oracle.h"

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/arrival.h>
#include <game/ovl/cutscene.h>
#include <game/lib/libc.h>
#include <runtime/kernel/disc.h>

#include <cmath>

namespace rac1::oracle {

void batch_80() {
    if (g_only_batch != 80) return;
    g_batch = 80;
    g_ignored_words = vcross_lanes();
    // the callees left TODO_PORT: the kernel's vsync wait, the movie player, the level change request (the two effects of
    // other levels' scenes, Kerwan's sparks 0x26ced0 and level 14's effect 0x23e008, ported since: they run on both sides,
    // batches 410 and 401)
    g_extra_noop_hooks = {0x00122298, OVF(0x0024d430)};
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    // 989snd's IOP server (as batch 75's): replies a function of the command; the stream system's
    // disc reads (command 0x38) done into EE RAM as the IRX does them
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
    static const uint32_t SC = OV(0x0016c860), P = 0x0013f350, S = 0x001516d0, TK = OV(0x00179100);
    static std::vector<uint8_t> snap;
    static uint32_t ship = 0;  // the Veldin ship moby (class 834)
    auto level = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty()) {
            veldin_boot();
            ee_write<uint16_t>(0x001518d0, 512);
            ee_write<uint16_t>(0x001518d2, 416);
            particle_table_init();
            level_setup_body(1, 0);
            hud_init();
            w32(OV(0x0016c060), 0xf);
            w32(0x0015ec8c, 1);  // the stream system up, the disc idle
            w32(0x00137b00, 0);
            sub_00264bf8();
            snap.assign(ram, ram + 0x02000000);
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); ee_read<uint8_t>(m + 0x20) != 0xff; m += 0x100) {
                const uint32_t fn = ee_read<uint32_t>(m + 0x74);
                if (ee_read<int16_t>(m + 0xa6) == 834) ship = m;
                if (fn != 0 && !ovl_moby_fn_ported(fn)) g_extra_noop_hooks.push_back(fn);
            }
            std::printf("batch 80: the ship moby %08x, talkers %d..%d\n", ship, ee_read<int32_t>(OV(0x001c44b8)), ee_read<int32_t>(OV(0x001c44bc)));
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    auto hook_new_mobys = [] {
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); ee_read<uint8_t>(m + 0x20) != 0xff; m += 0x100) {
            const uint32_t fn = ee_read<uint32_t>(m + 0x74);
            if (fn != 0 && !ovl_moby_fn_ported(fn) && std::find(g_extra_noop_hooks.begin(), g_extra_noop_hooks.end(), fn) == g_extra_noop_hooks.end())
                g_extra_noop_hooks.push_back(fn);
        }
    };
    // the scene's sound ready (the dialog slot in state 3 with a handle: the start's wait loop not
    // entered)
    auto sound_ready = [] {
        w16(S + 0x5a, 3);
        w32(S + 0x50, (R() & 0x7ffffffeu) | 1u);
        w16(S + 0x5c, 0);
        w32(S + 0x1c, 0xffffffffu);
    };
    // Veldin's scenes: those with blocks in the level header's table (sector rows of 0x250 bytes)
    auto scene_blocks = [](int32_t k) {
        const uint32_t row = 0x00137b80 + 0x2afc + u(k) * 0x250;
        int32_t n = 0;
        while (n < 0x93 && ee_read<int32_t>(row + u(n + 1) * 4) > ee_read<int32_t>(row + u(n) * 4)) ++n;
        return n;
    };
    // ---- newlib strstr, the music's pause timers, the level exit
    check("libc_strstr", 0x001169c8, N, Ret::Int32, [level] {
        level();
        const int ls = ri(0, 12), lf = ri(0, 4);
        for (int i = 0; i < ls; ++i) w8(T + u(i), u(ri('a', 'c')));
        w8(T + u(ls), 0);
        if (coin() && lf <= ls && ls > 0) {  // a part of it
            const int at = ri(0, ls - lf);
            for (int i = 0; i < lf; ++i) w8(T2 + u(i), ee_read<uint8_t>(T + u(at + i)));
        } else {
            for (int i = 0; i < lf; ++i) w8(T2 + u(i), u(ri('a', 'c')));
        }
        w8(T2 + u(lf), 0);
        if (ri(0, 5) == 0) w8(T2, 0x80u | (R() & 0x7f));
        return Args{{T, T2}, {}};
    }, [](A a) { return ri32(static_cast<int32_t>(libc_strstr(a.i[0], a.i[1]))); });
    check("music_pause_timer", 0x00265880, 30, Ret::None, [level] {
        level();
        for (uint32_t o : {0x40u, 0x5cu, 0x78u}) {
            w16(S + o, coin() ? 0x8000u : u(ri(0, 4)));
            w16(S + o + 2, R());
        }
        return Args{{R()}, {}};
    }, [](A a) { music_pause_timer(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    check("level_exit_to", 0x0028eeb0, 8, Ret::None, [level] { level(); return Args{{u(ri(-1, 0x12))}, {}}; },
          [](A a) { level_exit_to(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    // ---- the talk scripts
    auto any_moby = [] {
        std::vector<uint32_t> ms;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); ee_read<uint8_t>(m + 0x20) != 0xff; m += 0x100) ms.push_back(m);
        return ms[static_cast<size_t>(ri(0, static_cast<int32_t>(ms.size()) - 1))];
    };
    check("talker_index", 0x002667d0, N, Ret::Int32, [level, any_moby] {
        level();
        if (ri(0, 3) == 0) w32(0x0015ed84, u(ri(0, 0x14)));
        return Args{{coin() ? ship : any_moby()}, {}};
    }, [](A a) { return ri32(talker_index(a.i[0])); });
    // a script of `n` records at T for the ship's slot (records lead forward only: no endless
    // condition loop), or (unless `synth`) its own; its talk struct at M3
    auto script = [](int32_t n, bool scenes, bool synth = false) {
        const int32_t i = talker_index(ship);
        if (synth || coin()) {
            for (int32_t r = 0; r < n; ++r) {
                const uint32_t e = T + u(r) * 0x1c;
                wrand(e, 7);
                static const uint32_t texts[] = {0, 0x4ee2, 0x4ee5, 0x4f1d};
                w32(e, texts[ri(0, 3)]);
                const int k = ri(0, 3);
                w16(e + 4, k == 0 ? 0xffffu : k == 1 ? (0x4000u | u(ri(0, 3))) : scenes ? 4u : 0xffffu);
                w16(e + 6, ri(0, 4) == 0 ? u(r) : u(ri(r, n - 1)));
                w16(e + 8, u(ri(0, 7)));
                w16(e + 0xa, u(ri(0, 6)));
                w16(e + 0xc, u(ri(r, n - 1)));
                w16(e + 0xe, u(ri(r, n - 1)));
                w16(e + 0x10, R() & (scenes ? 0xfu : 0xbu));
            }
            if (i >= 0) w32(OV(0x001b1678) + u(i) * 4, T);
        }
        if (i >= 0) w32(0x0013d5b0 + u(i) * 0x10 + 0xc, R() & 1);
        wrand(M3, 0x10);
        w32(0x0015ed98, u(ri(0, 4000)));
        for (int32_t k = 0; k < 0x30; ++k) w8(0x0013d4c0 + u(k), R() & 1);
        for (int32_t k = 0; k < 0x30; ++k) w8(0x0013d4e8 + u(k), R() & 1);
        for (int32_t k = 0; k < 0x10; ++k) w8(0x0013d388 + u(k), R() & 1);
        w32(OV(0x00160130), coin() ? 0xffffffffu : u(ri(0, 3)));
        w32(0x0015ed88, u(ri(0, 5)));
    };
    check("talk_init", 0x002668a0, N, Ret::Int32, [level, any_moby, script] {
        level();
        script(ri(1, 6), false);
        return Args{{ri(0, 3) == 0 ? any_moby() : ship, M3}, {}};
    }, [](A a) { return ri32(talk_init(a.i[0], a.i[1])); });
    check("talk_step", 0x00266970, 60, Ret::None, [level, script] {
        level();
        const int32_t n = ri(1, 6);
        script(n, false, true);  // (the ship's own script leads past its last record)
        talk_init(ship, M3);
        w16(M3 + 0x36, u(ri(0, n - 1)));
        if (ri(0, 5) == 0) w32(M3 + 0x3c, 0);
        if (ri(0, 6) == 0) w16(M3 + 0x36, 0xffffu);
        return Args{{ship, M3, u(ri(0, 1))}, {}};
    }, [](A a) { talk_step(a.i[0], a.i[1], static_cast<int32_t>(a.i[2])); return uint64_t{0}; });
    // the hero near the ship (or not), facing it (or not), the buttons, the frame
    auto talk_setup = [level, script, sound_ready, hook_new_mobys](bool scenes) {
        level();
        sound_ready();
        script(ri(1, 6), scenes);
        const uint32_t pv = ee_read<uint32_t>(ship + 0x78);
        const uint32_t t = coin() ? M3 : pv + 0x20;
        if (coin()) talk_init(ship, t);
        else w8(t + 9, 0);
        wf(t + 0xc, coin() ? rf(1, 6) : 255.0f);
        if (ri(0, 4) == 0) w8(t + 8, coin() ? 0xffu : 1u);
        const float a = rf(-3.1f, 3.1f), d = coin() ? rf(0, 6) : rf(0, 30);
        wf(P + 0x80, ee_read<float>(ship + 0x10) + d * std::cos(a));
        wf(P + 0x84, ee_read<float>(ship + 0x14) + d * std::sin(a));
        wf(P + 0x98, coin() ? rf(-3.1f, 3.1f) : a + 3.14159f);
        wf(ship + 0x48, rf(-3.1f, 3.1f));
        w32(P + 0x2084, ri(0, 6) == 0 ? 0x1du : 0u);
        w32(P + 0x22a8, ri(0, 6) == 0 ? 0u : 1u);
        w32(OV(0x0015f5c4), ri(0, 4) == 0 ? u(ri(1, 6)) : 0u);
        w32(0x0013cae4, coin() ? 0x10u : 0u);
        w32(OV(0x0015f5cc), u(ri(0, 100)));
        w32(TK + 0x10, u(ri(0, 100)));
        hook_new_mobys();
        return t;
    };
    check("talk_update", 0x00266448, 60, Ret::Int32, [talk_setup] {
        const uint32_t t = talk_setup(false);
        return Args{{ship, t}, {}};
    }, [](A a) { return ri32(talk_update(a.i[0], a.i[1])); });
    check("talk_update_scene", 0x00266448, 20, Ret::Int32, [talk_setup] {  // its scene (4 or 0) started
        const uint32_t t = talk_setup(true);
        return Args{{ship, t}, {}};
    }, [](A a) { return ri32(talk_update(a.i[0], a.i[1])); });
    check("veldin_ship_update", 0x002d9dc8, 60, Ret::None, [talk_setup] {
        talk_setup(coin());
        const uint32_t pv = ee_read<uint32_t>(ship + 0x78);
        w8(ship + 0x20, u(ri(0, 3)));
        w8(0x0013d390, R() & 1);
        w16(pv + 0x56, R() & 1);
        w16(pv + 0x24, u(ri(-1, 5)));
        w32(pv + 0x60, u(ri(-1, 3)));
        w32(OV(0x0015f5c4), ri(0, 3) == 0 ? u(ri(0, 6)) : 0u);
        return Args{{ship}, {}};
    }, [](A a) { veldin_ship_update(a.i[0]); return uint64_t{0}; });
    // ---- game mode 2
    auto scene_pick = [scene_blocks] {
        std::vector<int32_t> ks;
        for (int32_t k = 0; k < 0x20 && ee_read<int32_t>(0x00137b80 + 0x2afc + u(k) * 0x250) != 0; ++k)  // the level's scenes
            if (scene_blocks(k) >= 2) ks.push_back(k);
        return ks.empty() ? 4 : ks[static_cast<size_t>(ri(0, static_cast<int32_t>(ks.size()) - 1))];
    };
    check("cutscene_block_read", 0x002451b8, 30, Ret::Int32, [level, scene_pick, scene_blocks] {
        level();
        const int32_t k = scene_pick();
        w32(SC + 0x30, u(k));
        w32(SC + 0x5c, ee_read<uint32_t>(OV(0x00173e08)) + ee_read<uint32_t>(OV(0x001611cc)) - 0x3c000u);
        w16(S + 8, 0);
        return Args{{u(ri(0, scene_blocks(k)))}, {}};
    }, [](A a) { return ri32(cutscene_block_read(static_cast<int32_t>(a.i[0]))); });
    auto start_setup = [level, sound_ready, scene_pick] {
        level();
        sound_ready();
        w32(P + 0x22a8, ri(0, 8) == 0 ? 0u : 1u);
        w32(TK + 8, coin() ? ship : 0u);
        return scene_pick();
    };
    check("cutscene_start", 0x00298840, 20, Ret::None, [start_setup] {
        const int32_t k = start_setup();
        return Args{{ri(0, 8) == 0 ? 0xffffffffu : u(k)}, {}};
    }, [](A a) { cutscene_start(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    // a cutscene under way: started, then at a random frame of its current block
    auto running = [start_setup, hook_new_mobys] {
        const int32_t k = start_setup();
        w32(P + 0x22a8, 1);
        cutscene_start(k);
        hook_new_mobys();
        const int32_t per = 0x60;
        const int32_t kk = ri(0, 3);
        w32(SC + 0x38, u(kk == 0 ? per - 1 : kk == 1 ? per : ri(0, per - 2)));
        w32(SC + 0x34, u(ri(0, 2) == 0 ? ee_read<int16_t>(SC + 0x40) - ri(0, 2) : ri(0, 200)));
        w16(SC + 0x48, u(ri(0, 300)));
        wf(OV(0x0015f3fc), ri(0, 2) == 0 ? 0.0f : rf(0, 1));
        const int b = ri(0, 4);
        w32(0x0013cae4, b == 0 ? 0x800u : b == 1 ? 0x220u : 0u);
        w32(0x0013cae0, ri(0, 3) == 0 ? 0xfu : R() & 0xffu);
        w32(0x0015eea0, ri(0, 4) == 0 ? 1u : 0u);
        w32(OV(0x0016c060), ri(0, 3) == 0 ? (R() & 0x1fu) : 0xfu);
        w8(0x0015edb0, R() & 1);
        w8(0x0015edb4, R() & 1);
        wf(OV(0x0015f5a0), coin() ? 0.0f : rf(-3, 3));
        wf(OV(0x0015f59c), rf(0.5f, 1.5f));
        w32(OV(0x0015f5d8), 0);
        if (ri(0, 4) == 0) w32(0x0015ed84, u(ri(1, 3)));
    };
    check("cutscene_camera", 0x00298de8, 40, Ret::Int32, [running] { running(); return Args{}; },
          [](A) { return ri32(cutscene_camera()); });
    {
    // (stage 5, task C2c) cutscene_update reaches segment_collide: A2's collision scratch
    // ranges left out of this check (see batch 43's checks of it)
    const CollisionScratchIgnored spr_ignored;
    check("cutscene_update", 0x00298f90, 60, Ret::None, [running] { running(); return Args{}; },
          [](A) { cutscene_update(); return uint64_t{0}; });
    }
    {
    // (stage 5, task C2b) cutscene_end reaches sphere_collide / capsule_collide: A2's collision scratch
    // ranges left out of this check (see batch 43's checks of it)
    const CollisionScratchIgnored spr_ignored;
    check("cutscene_end", 0x00298b18, 40, Ret::None, [running] {
        running();
        w16(SC + 0x46, R() & 1);
        for (uint32_t k = 0; k < 3; ++k) wf(SC + 0x10 + k * 4, ee_read<float>(P + 0x80 + k * 4) + rf(-2, 2));
        wfrand(SC + 0x20, 3, -3, 3);
        w16(S + 0x5a, u(ri(0, 7)));
        if (coin()) {  // the ship's talk to be continued
            const uint32_t pv = ee_read<uint32_t>(ship + 0x78);
            talk_init(ship, pv + 0x20);
            w32(TK + 8, ship);
            w32(TK + 0xc, pv + 0x20);
        }
        w32(OV(0x001b9cf0) + 0xec, coin() ? ship : 0u);
        for (uint32_t o : {0x40u, 0x5cu, 0x78u}) w16(S + o, coin() ? 0x8000u : 4u);
        return Args{};
    }, [](A) { cutscene_end(); return uint64_t{0}; });
    }
    check("cutscene_level_extras", 0x0023e268, 60, Ret::None, [running, hook_new_mobys] {
        running();
        w32(0x0013cae4, 0);  // a frame of it played first: the scene's mobys posed
        w32(0x0013cae0, 0);
        w32(SC + 0x34, 10);
        w32(OV(0x0016c060), 0xf);
        cutscene_update();
        hook_new_mobys();
        const int32_t n = ee_read<int16_t>(SC + 0x44);
        for (int32_t i = 0; i < n; ++i) {
            const uint32_t m = ee_read<uint32_t>(SC + 0x178 + u(i) * 4);
            const int k = ri(0, 4);
            if (k < 3) w16(m + 0xa6, k == 0 ? 0x212u : k == 1 ? 0x1b1u : 0x214u);
        }
        // each level's case in turn, its scene and frames in range (or near)
        static const uint32_t lv[] = {0, 2, 3, 7, 0xe, 0xf, 1};
        const int c = ri(0, 6);
        w32(0x0015ed84, lv[c]);
        const bool in = ri(0, 3) != 0;
        w32(SC + 0x30, in ? (c == 2 ? 5u : c == 3 ? 4u : c == 5 ? u(ri(3, 4)) : 0u) : u(ri(0, 6)));
        w32(SC + 0x34, in ? (c == 2 ? u(ri(895, 1105)) : c == 4 ? u(ri(0x1a, 0x23)) : u(ri(0, 1200))) : u(ri(0, 1200)));
        for (uint32_t o : {0x17cu, 0x184u, 0x188u}) {
            const int k = ri(0, 3);
            if (k == 0) w32(SC + o, 0);
            else if (k == 1) w32(SC + o, o == 0x184 ? ee_read<uint32_t>(0x001413d0) : ship);  // (the sparks' chain points: the hero's)
            else if (o == 0x184) w32(SC + o, 0);  // (Veldin's scene mobys: no chain points to take)
        }
        if (coin() && ee_read<uint32_t>(SC + 0x188) != 0) w16(ee_read<uint32_t>(SC + 0x188) + 0xa6, 0x214);
        seed_rand();
        return Args{};
    }, [](A) { cutscene_level_extras(); return uint64_t{0}; });
    snd_server(false);
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
