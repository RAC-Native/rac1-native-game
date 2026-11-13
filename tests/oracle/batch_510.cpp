// Oracle batch 510 (stage 4, task F7): the shared ports of the class code several levels carry a copy of
// (src/game/ovl/shared_*.cpp), each against every level's own copy at that level's own address. Run it on each level that
// has a copy: ORACLE_LEVEL=<n> build/tests/ovl_oracle --batch 510 for n in 0, 1, 2, 4 .. 13, 15, 16 and 18 (needs the ISO;
// no dumps), and the 1.02 fork's check with RAC1_GAMEPLAY102=23 on 15 and 18. A check prints "skipped" on a level without
// its copy. The level snapshot has the screen, the projection and the view set up (screen_setup, projection_setup,
// render_view_setup) and the draws' packet cursor is put on a test-area buffer, so that a draw's packets are compared (the
// beam and the finale's meshes placed before the view where their quads reach the packets; B510_INFO=1 prints, per check,
// how many cases wrote packets beyond a state header); the port gets the VU0 registers the original started from
// (vu0_sync). The class updates run on the level's own mobys of the
// class after the first gameplay frame (as the levels' class batches 109, 121, 122 ... do), through every state, perturbed;
// the helpers through their callers and on their own with arguments built from the level's state.
//
// The shared ports and their copies (each level's address):
//   bg_ship2_update      class 1213 / 1973   L2:0x002ece18 L9:0x00306720 L10:0x002ebb08
//   bg_ship_exhaust      (helper)            L2:0x002eca30 / 0x002ed420 / 0x002ed658, L9:0x00306338 / 0x00306d28 / 0x00306f60,
//                                            L10:0x002ec110 / 0x002ec348
//   bg_ship_glow_prep    (helper)            L2:0x002ec3f0 / 0x002ecd50, L9:0x00305cf8 / 0x00306658, L10:0x002eba40
//   mover_platform_update  class 1129 / 104 / 106     L7:0x0031aee0 L13:0x002c32d0
//   mover_door_update      class 1063 / 129 / 130 ..  L7:0x00310df0 L13:0x002c8390
//   puffer_update          class 648                  L8:0x002f5830 L10:0x002c74b8
//   homing_missile_launch  (helper)                   L8:0x002e7eb0 L12:0x002ef2e8
//   homing_missile_update  class 458                  L8:0x002e8210 L12:0x002ef648 (on missiles the launch spawns)
//   shell_launch           (helper)                   L9:0x00307ba8 L10:0x002e68b8
//   shell_update           class 1258                 L9:0x00307d68 L10:0x002e6a78 (on shells the launch spawns)
//   shot_launch            (helper)                   L4:0x002aee30 L12:0x002d4150
//   shot_update            class 184                  L4:0x002af058 L12:0x002d4378 (on shots the launch spawns)
//   tracer_update          class 1292                 L8:0x00307298 L9:0x00308f08 (on rounds spawned for the check)
//   flicker_light_update   class 1504                 L1:0x0030b618 L6:0x00308868
//   flame_spray / _clear / _burn (helpers)            L2:0x00264e70 / 0x00264e40 / 0x002651d0, L16:0x0025eab8 / 0x0025ea88 /
//                                                     0x0025ee18 (on records and flames built for the check)
//   envmap_uvs (helper)                               L16:0x002e8b98 / 0x002e90c8 / 0x002e95f8 / 0x002e9b38 / 0x002e9e40
//   envmap_1948_update / _1953_update                 L16:0x002e98e0 / 0x002ea128
//   envmap_1943_draw / _1948_draw / _1953_draw        L16:0x002e8d40 / 0x002e97a0 / 0x002e9fe8 (on the classes' mobys)
//   beam_sparks / beam_draw (class 1068)              L6:0x002ff978 / 0x002ff680, L10:0x002dc990 / 0x002dc698, L13:0x00309bc8 /
//                                                     0x003098d0 (on the level's class-1068 mobys, or any moby made one)
//   beam_shard_update      class 1085 ..              L6:0x00300b90 L10:0x002dd010 L13:0x00303878 (on shards spawned)
//   arcs_update / arc_between / arc_sparks (helpers)  L4:0x002c46d0 / 0x002c45b8 / 0x002c41e0 (Eudora's build), L8:0x002da8d8 /
//                                                     0x002da7c0 / 0x002da3f0 (Batalia's) (on any moby, near the hero)
//   gold_weapon_scene      (the cases' scene start)   L1:0x002aea70 L13:0x002a5390: not compared (989snd's SIF, the stream load)
//   tile_links             (helper)                   L7:0x0030cd48 L11:0x0030e730 L12:0x002bf770 L13:0x0030a488
//   tile_group_height      (helper)                   L11:0x0030e690 L12:0x002bf6d0 (on the level's grouped mobys)
//   raritanium_pickup_update  class 37                L11:0x002cb790 L12:0x002bf9b8
//   uv_offset_pairs / uv_scroll_meshes / uv_scroll_step / mesh_colours_scale / wave_step (helpers, every copy: the sets of
//                          shared_scroll.h)           L8, L11, L12 (on the level's own tables)
//   deadly_zones_update    class 1039                 L6:0x002f7930 L8:0x00302a78 L13:0x003036f0 L18:0x002ec858
//   attach_bind / attach_carry (helpers)              L12:0x0027b268 / 0x0027b370, L18:0x00265250 / 0x00265358 (records built
//                                                     in the test area on the level's mobys)
//   shockwave_update       class 1427                 L15:0x0029ead0 L18:0x002ab7c0 (on waves spawned near the hero)
//   finale_quads_draw      (draw callbacks)           L18:0x002f2620 / 0x002f27c8 / 0x002f2970 (the camera at the mesh)
//   finale_stop_hook       (class hooks)              L18:0x002d6280 / 0x002fcd00
//   flash_update / flash_spawn (class 1898)           L6:0x0030be60 / 0x0030bef8, L18:0x002fb548 / 0x002fb5e0
//   camera_type12_flag     (camera hook)              L6:0x00316330 L18:0x00306338
//   state_query_blarg / _finale (two builds)          L6:0x003047f8, L18:0x002dfba0
//   level_movie_play       (helper)                   L2:0x00298c68 L8:0x002a1880 L16:0x002923d0 (the paths without the movie:
//                                                     Ratchet lost, or no movie while +0x20b1 is set)
//   point_turned           (helper)                   L7:0x00288968 L9:0x0027fe88
//   camera_flag_88         (helper)                   L8:0x00313ba8 L9:0x00312888 L12:0x00312f58
//   push_without_direction (helper)                   L10:0x0024c7f0 L13:0x00265d68
//   spawn_like / spawn_like_gaspar / spawn_over_at_59 (helpers)  L4:0x002ce080 / 0x002d34d8, L12:0x002f1118; L9:0x002efda8;
//                                                     L10:0x002dd848 / 0x002eb8c0
//   group_wake             (helper)                   L10:0x002e0138 / 0x002e47d8 (on the level's grouped mobys)
//   bullet_particle_update (particle type 0x31)       Veldin 0x00271d20 (level 0), L5:0x0029bda0 (new records about the hero)
//   timed_particle_update  (particle types 0x21 / 0x3f)  Veldin 0x0026f048 / 0x002738b0 (level 0)
//   clank_pad_update       class 1451 / 1899          L15:0x002ed068 L18:0x002fb868 (and "clank_pad_fix23": its 1.02 fork,
//                                                     run with RAC1_GAMEPLAY102=23, against the fork-off port plus the
//                                                     1.02 build's two changes)

#include "ovl_oracle.h"

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/shared_bg_ships.h>
#include <game/ovl/shared_movers.h>
#include <game/ovl/shared_puffer.h>
#include <game/ovl/shared_shell.h>
#include <game/ovl/shared_tracer.h>
#include <game/ovl/shared_lights.h>
#include <game/ovl/shared_clank_pad.h>
#include <game/ovl/shared_flame_spray.h>
#include <game/ovl/shared_envmap.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/shared_beam.h>
#include <game/ovl/shared_arcs.h>
#include <game/ovl/shared_tiles.h>
#include <game/ovl/shared_scroll.h>
#include <game/ovl/shared_zones.h>
#include <game/ovl/shared_attach.h>
#include <game/ovl/shared_shockwave.h>
#include <game/ovl/shared_finale.h>
#include <game/ovl/shared_flash.h>
#include <game/ovl/shared_small.h>
#include <game/ovl/shared_spawns.h>
#include <game/ovl/shared_particles.h>
#include <game/enhance/advanced.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/shared_homing_missile.h>
#include <runtime/kernel/disc.h>
#include <runtime/vu/vu0.h>

#include <cmath>
#include <cstring>
#include <initializer_list>
#include <string>
#include <unordered_set>
#include <vector>


namespace rac1::oracle {

namespace {

// The VU0 the original started from (the interpreter's), given to the port before it runs (as batches 128 / 506): some ports
// read a VU0 register's stale lanes (the texture scroll's vf1.w, the collision's records)
VUState g_vu0_in;
void vu0_sync() {
    VUState& v = vu0::global().st;
    v.vf = g_vu0_in.vf;
    v.vi = g_vu0_in.vi;
    v.acc = g_vu0_in.acc;
    v.q = g_vu0_in.q;
    v.i = g_vu0_in.i;
    v.r = g_vu0_in.r;
    v.p = g_vu0_in.p;
    v.macflag = g_vu0_in.macflag;
    v.statusflag = g_vu0_in.statusflag;
    v.clipflag = g_vu0_in.clipflag;
    std::memcpy(v.mem.data(), g_vu0_in.mem.data(), 0x1000);
}

// the frame's packet cursor (0x1611c0) on a buffer of the test area (after the first gameplay frame it is 0: a draw would
// write over the null page, which the checks leave out, and compare nothing; batch 506)
void packet_cursor() { w32(OV(0x001611c0), TEST_AREA + 0x8000 + u(ri(0, 0x40)) * 0x10); }

// one level's copy of a shared function: the level, the copy's address there
struct Copy {
    int level;
    uint32_t addr;
};
// this level's copy among `copies` (the first one listed for the level), 0 when the level has none
uint32_t copy_here(std::initializer_list<Copy> copies, int nth = 0) {
    for (const Copy& c : copies)
        if (c.level == g_oracle_level && nth-- == 0) return c.addr;
    return 0;
}

// B510_INFO=1: per check, how many of its cases compared real output (the port's run, which the original's matched):
// packets beyond a state header (a quad clipped away writes only its GS state, quad_gs_state's 5 quadwords, and
// sub_002a0ba8's register writes are state too)
bool info() { return std::getenv("B510_INFO") != nullptr; }
// the bytes of the packets between `c0` and `c1` that are not a state header (cnt DMA tags; what does not parse counts)
uint32_t packet_content(uint32_t c0, uint32_t c1) {
    uint32_t content = 0;
    for (uint32_t p = c0; p < c1;) {
        const uint32_t w0 = ee_read<uint32_t>(p), size = ((w0 & 0xffffu) + 1) * 0x10;
        if (((w0 >> 28) & 7) != 1 || p + size > c1) return content + (c1 - p);
        const bool gs_state = (w0 & 0xffffu) == 5 && ee_read<uint64_t>(p + 0x10) == 0x2400000000000001ull;
        const bool reg_write = (w0 & 0xffffu) == 2 && ee_read<uint32_t>(p + 0x10) == 0x8001u && ee_read<uint32_t>(p + 0x14) == 0x10000000u;
        if (!gs_state && !reg_write) content += size;
        p += size;
    }
    return content;
}

// The words where the native collision (segment_collide 0x1efc70 / sphere_collide, ovl_1e_1f.cpp, existing code) differs from
// the original: its result rows 0x173e60 .. 0x173eb0 store the w lane 0 where the original stores its VU0 registers' w lanes,
// and the segment record's x / y / z (0x173e90..) are the original's VU0 leftovers of another computation, which the native
// code does not keep. Left out in the checks of the functions that collide, only.
void collision_lanes() {
    for (uint32_t row = 0x20; row < 0x80; row += 0x10) g_ignored_words.push_back(OV(0x00173e40) + row + 0xc);
    for (uint32_t k = 0; k < 0xc; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
}

// A draw's subject placed where the port's packets carry more than a state header (a quad on the screen, or through the
// clipper with something left), three times in four (else where the generator put it): `place(q)` puts the subject at q;
// the candidates lie about the view's line from its camera (0x166dc0) along its forward row (0x166fd0), near and far, each
// tried by running the port on a copy of RAM (as batch 507's place_drawn); the first that draws is kept. `candidate(k)`, when
// given, makes the k-th candidate instead
void place_drawn(const std::function<void()>& run, const std::function<void(const Vec4&)>& place,
                 const std::function<Vec4(int)>& candidate = nullptr) {
    if (ri(0, 3) == 0) return;
    static std::vector<uint8_t> saved;
    uint8_t* ram = ee_get_ram_base();
    saved.assign(ram, ram + 0x02000000);
    auto drawn = [&](const Vec4& q) {
        place(q);
        const uint32_t c0 = ee_read<uint32_t>(OV(0x001611c0)), sp0 = ee_stack_ptr();
        uint32_t c = 0;
        try { run(); c = packet_content(c0, ee_read<uint32_t>(OV(0x001611c0))); } catch (const std::exception&) {}
        ee_stack_ptr() = sp0;
        std::memcpy(ram, saved.data(), saved.size());
        return c != 0;
    };
    const Vec4 cam = ee_ref<Vec4>(OV(0x00166dc0)), fwd = ee_ref<Vec4>(OV(0x00166fd0));
    for (int k = 0; k < 60; ++k) {
        const float r = rf(2.0f, k < 30 ? 15.0f : 40.0f), j = k < 30 ? 0.25f : 0.6f;
        const Vec4 q = candidate ? candidate(k) : Vec4{cam.x + r * (fwd.x + rf(-j, j)), cam.y + r * (fwd.y + rf(-j, j)), cam.z + r * (fwd.z + rf(-j, j)), 1.0f};
        if (drawn(q)) {
            place(q);
            return;
        }
    }
    if (info()) std::printf("INFO not placed\n");
}

// A check of a shared port against this level's copy at `addr` (0: the level has none, skipped).
void fcheck(const char* name, uint32_t addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (addr == 0) {
        std::printf("%-26s skipped (no copy on level %d)\n", name, g_oracle_level);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L%d:0x%08x\n", name, g_oracle_level, addr);
    int cases = 0, grew = 0, drew = 0;
    uint32_t content = 0;
    const std::function<uint64_t(const Args&)> synced = [&port, &cases, &grew, &drew, &content](const Args& a) {
        vu0_sync();
        if (!info()) return port(a);
        const uint32_t c0 = ee_read<uint32_t>(OV(0x001611c0));
        const uint64_t r = port(a);
        const uint32_t c1 = ee_read<uint32_t>(OV(0x001611c0));
        ++cases;
        if (c1 != c0) {
            ++grew;
            const uint32_t c = c0 >= 0x00100000u && c1 > c0 && c1 - c0 < 0x100000 ? packet_content(c0, c1) : 0;  // (a null cursor's: the null page)
            if (c != 0) ++drew;
            content += c;
        }
        return r;
    };
    if (!check_fn(name, addr, n, ret, gen, synced, Scope::FullRam, 0, 0)) ++g_fail;
    if (info() && (grew != 0 || std::strstr(name, "draw") != nullptr || std::strstr(name, "scroll_meshes") != nullptr))
        std::printf("INFO %s L%d: %d cases, packets in %d, beyond a state header in %d (0x%x bytes)\n", name, g_oracle_level, cases, grew, drew, content);
}
}  // namespace

void batch_510() {
    if (g_only_batch != 510) return;
    g_batch = 510;
    g_ignored_words = vcross_lanes();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // not compared: the scratchpad words the pose function spills the caller's $t8 / $t9 into (batch 121); the few words of
    // existing code's known differences are left out inside the checks that reach them (collision_lanes, the ship's debris)
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);

    // 989snd's IOP server (as batch 121's): replies a function of the command; the stream system's disc reads done into EE RAM
    {
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
    }
    // the level after its first gameplay frame (the port's), the hero's equipment existing
    static std::vector<uint8_t> snap;
    snap.clear();
    auto level = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty()) {
            veldin_boot();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            if (g_oracle_level == 18) w32(0x0013f2e8, 0xffffffffu);  // (the sound listener's cached points refreshed on the first frame: batch 270)
            gameplay_frame();
            if (g_oracle_level == 15 || g_oracle_level == 18) {  // the stream system up and the disc idle (batch 270): the cutscenes' stream reads
                w32(0x0015ec8c, 1);
                w32(0x00137b00, 0);
                sub_00264bf8();
            }
            // the screen and projection constants, which the oracle's display (no size) leaves broken (FLT_MAX: nothing
            // projects onto the screen): a 640 x 448 screen, as the game's (batch 501's); and the view's matrices (the draws
            // project through them, as batches 401 / 509)
            if (ee_read<uint16_t>(0x001518d0) == 0) { w16(0x001518d0, 640); w16(0x001518d2, 448); }
            screen_setup();
            projection_setup();
            render_view_setup();
            snap.assign(ram, ram + 0x02000000);
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    // the level's live mobys whose update function is `fn` (the level's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // a class update on the level's own mobys, perturbed by `vary` (a state that sends the port through a pointer outside RAM
    // is dealt again, as in batch 121)
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 12,
                                          uint32_t subjects_fn = 0) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (fn == 0) { fcheck(name, 0, 0, Ret::None, nullptr, nullptr); return; }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
        level();
        const std::vector<uint32_t> ms = mobys_with(subjects_fn != 0 ? subjects_fn : fn);  // (a callback's: its class's mobys)
        if (ms.empty()) { std::printf("%-26s L%d:0x%08x  no moby of the class on this level\n", name, g_oracle_level, fn); ++g_skipped; return; }
        fcheck(name, fn, n, Ret::None, [level, ms, vary, port] {
            static std::vector<uint8_t> saved;
            uint32_t m = 0;
            for (int attempt = 0; attempt < 40; ++attempt) {
                level();
                m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                vary(m);
                uint8_t* ram = ee_get_ram_base();
                saved.assign(ram, ram + 0x02000000);
                bool ok = true;
                const uint32_t stack_top = ee_stack_ptr();
                try { port(m); } catch (const std::exception&) { ok = false; }
                ee_stack_ptr() = stack_top;
                std::memcpy(ram, saved.data(), saved.size());
                if (ok) break;
            }
            return Args{{m}, {}};
        }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
    };
    // a hit on the moby: its slot in the hit table (0x178100, 64-byte entries) naming it, with a damage mask and a damage +0x2c
    auto give_hit = [](uint32_t m, uint32_t kinds) {
        if (ri(0, 3) == 0) { w8(m + 0xa4, 0xff); return; }
        const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
        w8(m + 0xa4, idx);
        w32(e + 0x34, ri(0, 8) == 0 ? ee_read<uint32_t>(OV(0x0015ffd8)) : m);
        w32(e + 0x24, ri(0, 2) == 0 ? (R() & ~kinds) : (kinds | (R() & 0xff)));
        { static const float dm[] = {-1.0f, 0.0f, 0.5f, 5.0f, 12.0f}; wf(e + 0x2c, dm[ri(0, 4)]); }
        wfrand(e, 4, -1, 1);
        w32(e + 0x28, u(ri(0, 20)));
    };
    // the hero near the moby or far from it
    auto hero_near = [](uint32_t m) {
        Vec4 q = ee_ref<Vec4>(m + 0x10);
        const float r = ri(0, 2) == 0 ? rf(0, 5) : rf(5, 15);
        const float a = rf(-3.1f, 3.1f);
        q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-1, 3);
        ee_ref<Vec4>(0x0013f3d0) = q;
    };
    // the generic preparation of batch 121: state `st` (after the port's state 0), pass 1 / 2 a hit and the pvars moved as typed,
    // the hero near (passes 0 and 2), the frame counter and the random seed varied
    auto generic_prep = [give_hit, hero_near](uint32_t m, int st, int pass, void (*fn)(uint32_t)) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t P = 0x0013f350;
        if (st != 0) {
            w8(m + 0x20, 0);
            const uint32_t sp0 = ee_stack_ptr();
            try { fn(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
        }
        w8(m + 0x20, u(st));
        if (pass >= 1) {
            if (coin()) { w8(m + 0x53, 0); w8(m + 0x52, 0); } w8(m + 0xbc, u(ri(0, 2)));
            wf(m + 0x48, rf(-3.1f, 3.1f));
            { Vec4 q = ee_ref<Vec4>(m + 0x10); const float r = ri(0, 3) == 0 ? rf(0, 6) : (coin() ? rf(6, 30) : rf(30, 80)); const float a = rf(-3.1f, 3.1f);
              q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-4, 12); ee_ref<Vec4>(P + 0x80) = q; }
            for (uint32_t o = 0x20; o < 0x200; o += 4) {
                if (ri(0, 9) != 0) continue;
                const uint32_t v = ee_read<uint32_t>(pv + o), ex = v & 0x7f800000u;
                if (ex >= 0x30000000u && ex < 0x50000000u) wf(pv + o, ee_read<float>(pv + o) * rf(0.5f, 1.5f));
                else if (v >= 1 && v < 16) w32(pv + o, u(ri(1, static_cast<int>(v) + 2)));
            }
            give_hit(m, ri(0, 1) ? 0x210000u : 0x330000u);
            if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
                const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                w8(e + 0x28, u(ri(0, 11))); w8(e + 0x29, u(ri(0, 4))); w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
                wf(e + 0x1c, coin() ? 5627.95f : 0.0f);
                w32(e + 0x20, ri(0, 3) == 0 ? 0u : ee_read<uint32_t>(P + 0x2080));
                wf(e + 0x2c, coin() ? rf(0.1f, 6) : 0.0f);
            }
        }
        if (pass != 1) hero_near(m);
        w32(OV(0x0015f5cc), R() & 0xffff);
        seed_rand();
    };
    // every state 0 .. `states`-1, three passes each
    auto states_check = [classcheck, generic_prep](const char* name, uint32_t fn, void (*port)(uint32_t), int states, int n = 12,
                                                   std::function<void(uint32_t)> extra = nullptr) {
        if (fn == 0) { classcheck(name, 0, port, nullptr, n); return; }
        for (int st = 0; st < states; ++st)
            for (int pass = 0; pass < 3; ++pass)
                classcheck(name, fn, port, [st, pass, port, generic_prep, extra](uint32_t m) {
                    generic_prep(m, st, pass, port);
                    if (extra) extra(m);
                }, n);
    };

    // (no moby or pvar words are left out: the VU0 the original started from is given to the port, vu0_sync)
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80};
    // the port's VU0 micro memory as the game leaves it, and the interpreter's VU0 kept for the port (as batches 137 / 506)
    vu0::vif0_send_chain(0x00100080, true);
    vu0::vif0_send_chain(0x0010e4c0, true);
    g_pre_call = [](EeInterpreter& ee) {
        const VUState& o = ee.vu0();
        g_vu0_in.vf = o.vf;
        g_vu0_in.vi = o.vi;
        g_vu0_in.acc = o.acc;
        g_vu0_in.q = o.q;
        g_vu0_in.i = o.i;
        g_vu0_in.r = o.r;
        g_vu0_in.p = o.p;
        g_vu0_in.macflag = o.macflag;
        g_vu0_in.statusflag = o.statusflag;
        g_vu0_in.clipflag = o.clipflag;
        std::memcpy(g_vu0_in.mem.data(), o.mem.data(), 0x1000);
    };

    // a live moby of the level with pvars (any)
    auto any_moby = []() -> uint32_t {
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), n = (ee_read<uint32_t>(OV(0x0015ffe0)) - first) >> 8;
        for (int k = 0; k < 50; ++k) {
            const uint32_t m = first + u(ri(0, static_cast<int>(n) - 1)) * 0x100;
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x78) != 0) return m;
        }
        return first;
    };
    // ---- the background ships (Aridia, Gaspar, Orxon)
    {
        const uint32_t upd = copy_here({{2, 0x002ece18}, {9, 0x00306720}, {10, 0x002ebb08}});
        // (the ship's debris: debris_spawn's pieces keep two w lanes of a VU0 sin / cos in the original's pvars (+0xc / +0x1c of
        // each 0x80 from *0x15ffe0) that the native debris_spawn does not make, as batches 109 / 121 leave out (the levels'
        // generated copies fail the same words: tried on levels 2, 9); for this check only)
        const size_t ign_ship = g_ignored_words.size();
        if (upd != 0) {
            level();
            for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x80000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
        }
        states_check("bg_ship2", upd, bg_ship2_update, 4, 12, [](uint32_t m) {
            if (ri(0, 3) != 0) w8(m + 0x31, u(ri(1, 3)));  // (drawn last frame: the exhaust and the glow run)
            if (ri(0, 3) == 0) w32(0x0015ed84, u(ri(0, 3) == 0 ? 10 : ri(0, 2) == 0 ? 9 : 2));  // (the game's level number: the branches of Aridia, Gaspar, Orxon)
            if (coin()) { w32(0x0015edf4, u(ri(0, 4))); w32(0x0015edfc, u(ri(0, 6))); w8(0x0013d40a, u(ri(0, 1))); w8(0x0013d417, u(ri(0, 1))); w32(0x001413d4, coin() ? 0x32u : 0u); w8(0x001413f4, u(ri(0, 1))); }
        });
        g_ignored_words.resize(ign_ship);
        // the tuning bases and the copies of the helpers (ship 1's and ship 2's), per level
        struct Ex { int level; uint32_t addr; uint32_t base; int count; bool slowed; int layout; };  // layout 0: the plain block, 1: Orxon's slowed one
        static const Ex exs[] = {
            {2, 0x002eca30, 0x00161f84, 3, false, 0}, {2, 0x002ed420, 0x00161fd8, 2, false, 0}, {2, 0x002ed658, 0x00161fd8, 2, true, 1},
            {9, 0x00306338, 0x00161cf0, 3, false, 0}, {9, 0x00306d28, 0x00161d44, 2, false, 0}, {9, 0x00306f60, 0x00161d44, 2, true, 1},
            {10, 0x002ec110, 0x00162074, 2, false, 0}, {10, 0x002ec348, 0x00162074, 2, true, 1},
        };
        // a moby of the ship classes (any of the level's) moved, its chain points the original reads, the way it came from random
        auto ship_moby = [level, mobys_with](uint32_t cls_fn) -> uint32_t {
            level();
            std::vector<uint32_t> ms = mobys_with(cls_fn);
            if (ms.empty()) {  // (any live moby with pvars)
                for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                    if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x78) != 0) { ms.push_back(m); break; }
            }
            return ms.empty() ? 0u : ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
        };
        for (const Ex& e : exs) {
            if (e.level != g_oracle_level) continue;
            const Ex x = e;
            fcheck(e.slowed ? "bg_ship_exhaust_slow" : "bg_ship_exhaust", x.addr, 40, Ret::None, [ship_moby, upd, x] {
                const uint32_t m = ship_moby(upd);
                const uint32_t from = T;
                Vec4 q = ee_ref<Vec4>(m + 0x10);
                q.x += rf(-3, 3); q.y += rf(-3, 3); q.z += rf(-1, 1);
                ee_ref<Vec4>(from) = q;
                w32(OV(0x0015f5cc), R() & 0xffff);
                seed_rand();
                return Args{{m, from}, {}};
            }, [x](A a) {
                if (x.layout == 0) bg_ship_exhaust(a.i[0], a.i[1], x.count, x.base, x.base + 0xc, x.base + 0x2c, false);
                else bg_ship_exhaust(a.i[0], a.i[1], x.count, x.base + 0x3c, x.base + 0xc, x.base + 0x48, true);
                return uint64_t{0};
            });
        }
        struct Gp { int level; uint32_t addr; uint32_t first, points, draw; };
        static const Gp gps[] = {{2, 0x002ec3f0, 3, 3, 0x002ec308}, {2, 0x002ecd50, 2, 6, 0x002ecc68}, {9, 0x00305cf8, 3, 3, 0x00305c10}, {9, 0x00306658, 2, 6, 0x00306570}, {10, 0x002eba40, 2, 6, 0x002eb958}};
        for (const Gp& g : gps) {
            if (g.level != g_oracle_level) continue;
            const Gp x = g;
            fcheck("bg_ship_glow_prep", x.addr, 40, Ret::None, [ship_moby, upd, x] {
                const uint32_t m = ship_moby(upd);
                if (ri(0, 4) == 0) w8(m + 0x21, 0xff);
                w8(m + 0x90, R()); w8(m + 0x91, R()); w8(m + 0x92, R());
                w32(OV(0x0015f5cc), ri(0, 3) == 0 ? 0u : (R() & 0xffff));
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                const uint32_t last = pv + 0x84 + x.points * 0x10;  // (where the frame of the last request is kept)
                if (ri(0, 2) == 0 || ee_read<uint32_t>(last) < 0x100000) w32(last, T2);
                if (coin()) w32(ee_read<uint32_t>(last), ee_read<uint32_t>(OV(0x0015f5cc)));
                return Args{{m}, {}};
            }, [x](A a) { bg_ship_glow_prep(a.i[0], x.first, x.points, x.draw); return uint64_t{0}; });
        }
    }

    // ---- the linked movers (Umbris, Gemlik Base): every state, the link's moby (its index, its state against the two the
    // record waits for) and the broken-moby byte varied
    {
        auto link_vary = [](uint32_t rec) {
            return [rec](uint32_t m) {
                const uint32_t r = ee_read<uint32_t>(m + 0x78) + rec;
                const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), n = (ee_read<uint32_t>(OV(0x0015ffe0)) - first) >> 8;
                if (ri(0, 4) == 0) w32(r + 0x10, 0xffffffffu);
                else if (ri(0, 2) == 0) w32(r + 0x10, u(ri(0, static_cast<int>(n) - 1)));
                const int32_t idx = ee_read<int32_t>(r + 0x10);
                if (idx >= 0 && ri(0, 3) != 0) {
                    const uint32_t link = first + (u(idx) << 8);
                    const int k = ri(0, 4);
                    w8(link + 0x20, k == 0 ? ee_read<uint32_t>(r + 0x14) : k == 1 ? ee_read<uint32_t>(r + 0x18) : k == 2 ? 0xfeu : u(ri(0, 8)));
                }
                if (ri(0, 3) == 0) w32(r + 0x38, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 7)));
                if (ri(0, 3) == 0) { w32(r + 0x30, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 3))); w32(r + 0x34, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 3))); }
                if (ri(0, 3) == 0) wf(r + 0xc, rf(-1, 4));
                if (ri(0, 5) == 0) { w8(m + 0xb0, u(ri(0, 15))); if (coin()) w8(0x0014c050 + (ee_read<uint32_t>(0x0015ed84) << 4) + ee_read<uint8_t>(m + 0xb0), 0xff); }
            };
        };
        states_check("mover_platform", copy_here({{7, 0x0031aee0}, {13, 0x002c32d0}}), mover_platform_update, 7, 12, link_vary(0xa0));
        states_check("mover_door", copy_here({{7, 0x00310df0}, {13, 0x002c8390}}), mover_door_update, 7, 12, link_vary(0));
    }

    // ---- the puffer (Batalia, Orxon): its timer about to expire, the defaults or its own tuning
    states_check("puffer", copy_here({{8, 0x002f5830}, {10, 0x002c74b8}}), puffer_update, 1, 40, [](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w32(pv + 0x3c, ri(0, 3) == 0 ? u(ri(2, 9)) : u(ri(0, 1)));
        w32(pv + 0x38, coin() ? 1u : 0u);  // (the class's defaults or its own tuning)
        wf(m + 0x40, rf(-3.1f, 3.1f)); wf(m + 0x48, rf(-3.1f, 3.1f));
    });

    // ---- the homing missiles (Batalia, Hoven): the launch from a random moby, its target any live moby (of the target class
    // or not); the flight on a missile the launch spawned, its velocity, place, life and target varied
    {
        auto target_for = [any_moby]() -> uint32_t {
            if (ri(0, 4) == 0) return 0;
            const uint32_t t = any_moby();
            if (ri(0, 3) != 0) w16(t + 0xa6, 0x1b3);
            if (ri(0, 5) == 0) w8(t + 0x20, coin() ? 0xfeu : 0xfdu);
            wfrand(ee_read<uint32_t>(t + 0x78) + 0x20, 3, -0.5f, 0.5f);
            return t;
        };
        auto launch_args = [level, any_moby, target_for]() {
            level();
            const uint32_t src = any_moby();
            Vec4 p = ee_ref<Vec4>(src + 0x10);
            p.x += rf(-3, 3); p.y += rf(-3, 3); p.z += rf(0, 3);
            ee_ref<Vec4>(T) = p;
            ee_ref<Vec4>(T + 0x10) = Vec4{rf(-1, 1), rf(-1, 1), rf(-0.5f, 1), rf(-1, 1)};
            const uint32_t target = target_for();
            w32(OV(0x0015f5cc), R());
            if (ri(0, 3) == 0) w32(0x0015ed84, coin() ? 12u : 8u);
            seed_rand();
            return Args{{src, T, T + 0x10, target}, {}};
        };
        if (std::getenv("B510_INFO") != nullptr && copy_here({{8, 1}, {12, 1}}) != 0) {
            const Args a = launch_args();
            const uint32_t m = homing_missile_launch(a.i[0], a.i[1], a.i[2], a.i[3]);
            std::printf("INFO missile %08x pvars %08x mobys end %08x pvars area %08x\n", m, m ? ee_read<uint32_t>(m + 0x78) : 0u, ee_read<uint32_t>(OV(0x0015ffe0)), pvars_lo());
        }
        fcheck("homing_missile_launch", copy_here({{8, 0x002e7eb0}, {12, 0x002ef2e8}}), 60, Ret::Int32, launch_args,
               [](A a) { return uint64_t{homing_missile_launch(a.i[0], a.i[1], a.i[2], a.i[3])}; });
        // (not compared: the native collision's words, collision_lanes, as batches 242 / 330: the generated copies differ from
        // the original there the same way: tried on level 8)
        const std::vector<uint32_t> before = g_ignored_words;
        collision_lanes();
        fcheck("homing_missile", copy_here({{8, 0x002e8210}, {12, 0x002ef648}}), 120, Ret::None, [launch_args, target_for] {
            uint32_t m = 0;
            for (int k = 0; k < 20 && m == 0; ++k) {
                const Args a = launch_args();
                m = homing_missile_launch(a.i[0], a.i[1], a.i[2], a.i[3]);
            }
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const int kind = ri(0, 5);
            if (kind == 0) wf(m + 0x10 + 4 * u(ri(0, 2)), coin() ? rf(0, 12) : rf(495, 520));  // (out of the level's box, or nearly)
            if (kind == 1) wfrand(m + 0x10, 3, 14, 20);  // (near the floor: a small explosion)
            if (ri(0, 2) == 0) ee_ref<Vec4>(pv) = Vec4{rf(-2, 2), rf(-2, 2), rf(-2, 2), 0.0f};
            w32(pv + 0x18, ri(0, 3) == 0 ? u(ri(0, 1)) : u(ri(2, 200)));
            if (ri(0, 2) == 0) w32(pv + 0x10, target_for());
            w32(pv + 0x1c, coin() ? 1u : 0xffffffffu);
            w32(OV(0x0015f5cc), R());
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { homing_missile_update(a.i[0]); return uint64_t{0}; });
        g_ignored_words = before;

        // ---- the shells (Gaspar, Orxon): the launch near the hero; the flight on a shell the launch spawned, its way, its fuse
        // (running, running out, out), its whistle's flags and the hero's distance varied
        auto shell_args = [level, any_moby]() {
            level();
            const uint32_t src = any_moby();
            Vec4 p = ee_ref<Vec4>(0x0013f3d0);
            p.x += rf(-6, 6); p.y += rf(-6, 6); p.z += rf(-1, 4);
            ee_ref<Vec4>(T) = p;
            ee_ref<Vec4>(T + 0x10) = Vec4{rf(-1, 1), rf(-1, 1), rf(-0.6f, 0.6f), rf(-1, 1)};
            seed_rand();
            return Args{{src, T, T + 0x10, u(ri(0, 3) == 0 ? ri(-4, 4) : ri(1, 150))}, {}};
        };
        fcheck("shell_launch", copy_here({{9, 0x00307ba8}, {10, 0x002e68b8}}), 60, Ret::Int32, shell_args,
               [](A a) { return uint64_t{shell_launch(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]))}; });
        fcheck("shell", copy_here({{9, 0x00307d68}, {10, 0x002e6a78}}), 120, Ret::None, [shell_args] {
            uint32_t m = 0;
            for (int k = 0; k < 20 && m == 0; ++k) {
                const Args a = shell_args();
                m = shell_launch(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]));
            }
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 2) == 0) { w32(pv + 0x24, u(ri(0, 1))); w32(pv + 0x28, u(ri(0, 1))); wf(pv + 0x20, rf(0, 8)); }
            if (ri(0, 2) == 0) { const int f = ri(0, 4); w16(pv + 0x1c, u(f == 0 ? 0 : f == 1 ? 1 : f == 2 ? -3 : ri(2, 60))); w16(pv + 0x1e, u(ri(-9, 90))); }
            if (ri(0, 3) == 0) w8(m + 0xbc, 1);
            if (ri(0, 4) == 0) wf(m + 0x10 + 4 * u(ri(0, 2)), coin() ? rf(0, 3) : rf(1019, 1025));
            if (ri(0, 2) == 0) ee_ref<Vec4>(pv) = Vec4{rf(-3, 3), rf(-3, 3), rf(-3, 1), 0.0f};
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { shell_update(a.i[0]); return uint64_t{0}; });

        // ---- the shots (Eudora, Hoven): as the shells, the launcher's class (0xff, 0x4b1, other) and the game's level varied
        auto shot_args = [shell_args]() {
            Args a = shell_args();
            const int k = ri(0, 3);
            if (k < 2) w16(a.i[0] + 0xa6, k == 0 ? 0xffu : 0x4b1u);
            if (ri(0, 3) == 0) w32(0x0015ed84, coin() ? 12u : 4u);
            seed_rand();
            return a;
        };
        fcheck("shot_launch", copy_here({{4, 0x002aee30}, {12, 0x002d4150}}), 60, Ret::Int32, shot_args,
               [](A a) { return uint64_t{shot_launch(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]))}; });
        fcheck("shot", copy_here({{4, 0x002af058}, {12, 0x002d4378}}), 120, Ret::None, [shot_args] {
            uint32_t m = 0;
            for (int k = 0; k < 20 && m == 0; ++k) {
                const Args a = shot_args();
                m = shot_launch(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]));
            }
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 2) == 0) { w32(pv + 0x24, u(ri(0, 1))); w32(pv + 0x28, u(ri(0, 1))); wf(pv + 0x20, rf(0, 8)); }
            if (ri(0, 2) == 0) { const int f = ri(0, 4); w16(pv + 0x1c, u(f == 0 ? 0 : f == 1 ? 1 : f == 2 ? -3 : ri(2, 60))); w16(pv + 0x1e, u(ri(-9, 90))); }
            if (ri(0, 3) == 0) w8(m + 0xbc, 1);
            if (ri(0, 4) == 0) wf(m + 0x10 + 4 * u(ri(0, 2)), rf(-2, 1));
            if (ri(0, 4) == 0) { Vec4& q = ee_ref<Vec4>(m + 0x10); q.x += coin() ? 70.0f : 110.0f; }
            if (ri(0, 2) == 0) ee_ref<Vec4>(pv) = Vec4{rf(-3, 3), rf(-3, 3), rf(-3, 1), 0.0f};
            if (ri(0, 3) == 0) w32(0x0015ed84, coin() ? 12u : 4u);
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { shot_update(a.i[0]); return uint64_t{0}; });

        // ---- the tracer rounds (Batalia, Gaspar): a round of class 1292 spawned near the hero in state 1 or 2, its velocity,
        // shooter, owner (live, dying, none), life and the screen's fade levels varied
        collision_lanes();  // (the native collision's words; the generated copy differs there the same way: tried on level 9)
        fcheck("tracer", copy_here({{8, 0x00307298}, {9, 0x00308f08}}), 120, Ret::None, [level, any_moby] {
            level();
            const uint32_t m = moby_spawn(1292);
            if (m == 0) { std::printf("  tracer: no round spawned (the class is not on this level)\n"); return Args{{0}, {}}; }
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            Vec4 p = ee_ref<Vec4>(0x0013f3d0);
            p.x += rf(-4, 4); p.y += rf(-4, 4); p.z += rf(0, 3);
            ee_ref<Vec4>(m + 0x10) = p;
            ee_ref<Vec4>(pv) = Vec4{rf(-2, 2), rf(-2, 2), rf(-2, 0.5f), 0.0f};
            w8(m + 0x20, u(ri(0, 4) == 0 ? 2 : ri(0, 5) == 0 ? 3 : 1));
            w32(pv + 0x10, ri(0, 2) == 0 ? 0u : any_moby());
            const uint32_t owner = ri(0, 4) == 0 ? 0u : any_moby();
            if (owner != 0 && ri(0, 4) == 0) w8(owner + 0x20, coin() ? 0xfeu : 0xfdu);
            w32(pv + 0x14, owner);
            w32(pv + 0x18, ri(0, 3) == 0 ? u(ri(0, 1)) : u(ri(2, 90)));
            w32(pv + 0x2c, u(ri(0, 1)));
            wf(OV(0x0015f5d4), coin() ? rf(0, 0.95f) : 1.0f);
            wf(OV(0x0015f5d0), coin() ? rf(0, 0.95f) : 1.0f);
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { if (a.i[0] != 0) tracer_update(a.i[0]); return uint64_t{0}; });
        g_ignored_words = before;
    }

    // ---- the flickering lights (Novalis, Blarg): the camera near / far / in the fade, its box, the moby it dies with, the
    // change's timer and ease varied
    states_check("flicker_light", copy_here({{1, 0x0030b618}, {6, 0x00308868}}), flicker_light_update, 3, 12, [](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (ri(0, 3) != 0) {
            Vec4 c = ee_ref<Vec4>(m + 0x10);
            const float r = ee_read<float>(pv + 0x5c) * (ri(0, 2) == 0 ? rf(0.72f, 0.78f) : rf(0.0f, 1.2f)), a = rf(-3.1f, 3.1f);  // (often about the fade's start, 3/4 of its range)
            c.x += r * std::cos(a); c.y += r * std::sin(a);
            ee_ref<Vec4>(OV(0x00166dc0)) = c;
        }
        if (ri(0, 3) == 0) w32(pv + 0x80, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 6)));
        if (ri(0, 3) == 0) {
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), n = (ee_read<uint32_t>(OV(0x0015ffe0)) - first) >> 8;
            const uint32_t idx = u(ri(0, static_cast<int>(n) - 1));
            w32(pv + 0x84, coin() ? 0xffffffffu : idx);
            if (coin()) w8(first + (idx << 8) + 0x20, coin() ? 0xfeu : u(ri(0, 5)));
        }
        if (ri(0, 2) == 0) w32(pv + 0x6c, u(ri(0, 1)));
        if (ri(0, 2) == 0) w32(pv + 0x7c, u(ri(0, 1)));
        if (ri(0, 4) == 0) w32(pv + 0x68, coin() ? 0xffffffffu : u(ri(0, 3)));
        // (its moby dead while it holds a light slot, now and then: the slot cleared; its draw distance +0x30 dealt: the
        // state 0 widens it to the range)
        if (ri(0, 5) == 0) {
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), n = (ee_read<uint32_t>(OV(0x0015ffe0)) - first) >> 8;
            const uint32_t idx = u(ri(0, static_cast<int>(n) - 1));
            w32(pv + 0x84, idx);
            w8(first + (idx << 8) + 0x20, 0xfe);
            w32(pv + 0x68, u(ri(0, 3)));
        }
        if (ri(0, 2) == 0) w8(m + 0x30, R() & 0xff);
        if (coin()) {  // (the first light slots in use: their clearing seen)
            wrand(OV(0x001802c0), 4 * 8);
            wrand(OV(0x001804c0), 4 * 12);
        }
    });

    // ---- Giant Clank's pad (Quartu, Veldin's finale): Ratchet on it or not, his mode (0, Giant Clank 2, other), state, the
    // game mode, the press, the game's level number (15 / 18), Giant Clank broken, a moby left linked at +0x70
    {
        const uint32_t pad_fn = copy_here({{15, 0x002ed068}, {18, 0x002fb868}});
        auto pad_vary = [](uint32_t m) {
            const uint32_t P = 0x0013f350, pv = ee_read<uint32_t>(m + 0x78);
            w32(P + 0x2fc, ri(0, 4) == 0 ? 0u : m);
            w16(P + 0x30e, ri(0, 5) == 0 ? 1u : 0u);
            w32(OV(0x0015f5c4), ri(0, 5) == 0 ? 2u : 0u);
            if (ri(0, 5) == 0) w32(P + 0x2084, 0x1d);
            { const int k = ri(0, 4); w8(P + 0x20a4, k < 2 ? 0u : k < 4 ? 2u : 1u); }
            w32(0x0013cae4, ri(0, 3) == 0 ? 0u : 0x10u);
            w32(OV(0x0015f594), ri(0, 3) == 0 ? 3u : 4u);
            if (ri(0, 3) == 0) w32(0x0015ed84, coin() ? 15u : 18u);
            if (ri(0, 3) == 0) w8(0x0014c050 + ee_read<uint8_t>(m + 0xb0) + 0x120, coin() ? 0xffu : 0u);
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), n = (ee_read<uint32_t>(OV(0x0015ffe0)) - first) >> 8;
            w32(pv + 0x70, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, static_cast<int>(n) - 1)));
        };
        states_check("clank_pad", pad_fn, clank_pad_update, 4, 20, pad_vary);
        // the fork: on a case where the press boards (the hero on the pad, mode 0, the press), the port with fix 23 against
        // the port without it plus what the 1.02 build adds (L15:0x2ee458's code): the linked moby retired and its index
        // cleared before, and on Quartu checkpoint_save(the pad 0.1 over its floor, its turn) before Ratchet is placed
        if (const char* only = std::getenv("ORACLE_ONLY"); pad_fn != 0 && (only == nullptr || std::strcmp(only, "clank_pad_fix23") == 0)) {
            if (std::getenv("RAC1_GAMEPLAY102") == nullptr) {
                std::printf("%-26s skipped (run with RAC1_GAMEPLAY102=23)\n", "clank_pad_fix23");
                ++g_skipped;
            } else {
                level();
                const std::vector<uint32_t> pads = mobys_with(pad_fn);
                int bad = 0, n = 0, retired = 0, saved = 0;
                static std::vector<uint8_t> start, with_fix;
                const std::unordered_set<uint32_t> ignored(g_ignored_words.begin(), g_ignored_words.end());
                for (int c = 0; c < 40 && !pads.empty(); ++c) {
                    level();
                    const uint32_t m = pads[static_cast<size_t>(ri(0, static_cast<int>(pads.size()) - 1))];
                    const uint32_t P = 0x0013f350, pv = ee_read<uint32_t>(m + 0x78);
                    w8(m + 0x20, 2); w32(P + 0x2fc, m); w16(P + 0x30e, 0); w32(OV(0x0015f5c4), 0); w32(P + 0x2084, 0); w8(P + 0x20a4, 0);
                    w32(0x0013cae4, 0x10); w32(OV(0x0015f594), 4); w32(0x0015ed84, g_oracle_level == 18 ? 0u : c % 3 == 0 ? 18u : 15u);  // (on level 18 no scene: its scene 6 waits a vsync the native run has not)
                    const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), cnt = (ee_read<uint32_t>(OV(0x0015ffe0)) - first) >> 8;
                    const uint32_t idx = u(ri(0, static_cast<int>(cnt) - 1));
                    w32(pv + 0x70, c % 4 == 0 ? 0xffffffffu : idx);
                    if (c % 5 == 1) w8(first + (idx << 8) + 0x20, 0xfe);
                    seed_rand();
                    uint8_t* ram = ee_get_ram_base();
                    start.assign(ram, ram + 0x02000000);
                    g_advanced_disabled = false;
                    clank_pad_update(m);
                    g_advanced_disabled = true;
                    with_fix.assign(ram, ram + 0x02000000);
                    std::memcpy(ram, start.data(), start.size());
                    const int32_t left = ee_read<int32_t>(pv + 0x70);  // (the 1.02 build's first change, by hand)
                    if (left != -1) {
                        const uint32_t other = first + (u(left) << 8);
                        if (other != 0 && ee_read<uint8_t>(other + 0x20) != 0xfe && ee_read<uint8_t>(other + 0x20) != 0xfd) {
                            moby_retire(other);
                            w32(pv + 0x70, 0xffffffffu);
                            ++retired;
                        }
                    }
                    Vec4 at = ee_ref<Vec4>(m + 0x10);  // (its second, by hand: the checkpoint before the place)
                    {  // (the floor under it, as the port finds it; the probe's own counters put back: the port's run probes once)
                        static std::vector<uint8_t> before_probe;
                        before_probe.assign(ram, ram + 0x02000000);
                        at.z = fpu_add(floor_probe_v(0.5f, at, 0), u2f(0x3dcccccdu));
                        std::memcpy(ram, before_probe.data(), before_probe.size());
                    }
                    if (ee_read<uint32_t>(0x0015ed84) == 15) { checkpoint_save_v(at, m + 0x40); ++saved; }
                    clank_pad_update(m);  // (the fix off: the North American code)
                    ++n;
                    for (uint32_t a = 0x00100000; a < 0x02000000; a += 4) {
                        if (ignored.count(a) != 0) continue;
                        if (a >= ee_stack_ptr() - 0x10000 && a < ee_stack_ptr() + 0x10000) continue;  // (the port's stack frames)
                        uint32_t x, y;
                        std::memcpy(&x, &with_fix[a], 4); std::memcpy(&y, &ram[a], 4);
                        if (x != y) { if (bad < 8) std::printf("  clank_pad_fix23 case %d: %08x fix=%08x expected=%08x\n", c, a, x, y); ++bad; break; }
                    }
                }
                std::printf("%-26s %s (%d cases, %d retired a linked moby, %d saved the checkpoint)\n", "clank_pad_fix23", bad == 0 && n > 0 ? "ok" : "FAIL", n, retired, saved);
                if (bad != 0 || n == 0) ++g_fail;
            }
        }
    }

    // ---- the flame spray (Aridia, Kalebo III): a record in the test area (its slots empty, taken, its smoke timer), from near
    // the hero along a random way, a random length; the burn on flames spawned for it, some spent or replaced
    {
        auto spray_args = [level]() {
            level();
            const uint32_t rec = T2;
            for (uint32_t k = 0; k < 7; ++k) w32(rec + 4 * k, ri(0, 2) == 0 ? 0x01000000u + u(ri(0, 0xffff)) * 0x40 : 0u);
            w16(rec + 0x1c, u(ri(0, 3) == 0 ? ri(0, 2) : ri(3, 40)));
            Vec4 p = ee_ref<Vec4>(0x0013f3d0);
            p.x += rf(-3, 3); p.y += rf(-3, 3); p.z += rf(0, 2);
            ee_ref<Vec4>(T) = p;
            ee_ref<Vec4>(T + 0x10) = Vec4{rf(-1, 1), rf(-1, 1), rf(-0.5f, 0.5f), rf(-1, 1)};
            w32(OV(0x0015f5cc), R());
            seed_rand();
            return Args{{rec, T, T + 0x10}, {coin() ? rf(0.5f, 4) : rf(4, 14)}};
        };
        const uint32_t spray_fn = copy_here({{2, 0x00264e70}, {16, 0x0025eab8}});
        fcheck("flame_spray", spray_fn, 80, Ret::None, spray_args, [](A a) { flame_spray(a.i[0], a.i[1], a.i[2], a.f[0]); return uint64_t{0}; });
        fcheck("flame_spray_clear", copy_here({{2, 0x00264e40}, {16, 0x0025ea88}}), 20, Ret::None, [] {
            wrand(T2, 8);
            return Args{{T2}, {}};
        }, [](A a) { flame_spray_clear(a.i[0]); return uint64_t{0}; });
        fcheck("flame_spray_burn", copy_here({{2, 0x002651d0}, {16, 0x0025ee18}}), 80, Ret::None, [spray_args, any_moby] {
            const Args a = spray_args();
            const uint32_t rec = a.i[0];
            for (uint32_t k = 0; k < 7; ++k) w32(rec + 4 * k, 0);
            for (int f = 0; f < 6; ++f) {  // (flames of the spray's own)
                w32(OV(0x0015f5cc), 0);
                w16(rec + 0x1c, 9);
                flame_spray(rec, a.i[1], a.i[2], a.f[0]);
            }
            for (uint32_t k = 0; k < 7; ++k) {
                const uint32_t p = ee_read<uint32_t>(rec + 4 * k);
                if (p == 0) continue;
                const int r = ri(0, 5);
                if (r == 0) w8(p, 0x19);
                else if (r == 1) w8(p + 1, 0x80);
                else if (r == 2) w16(p + 0xa, u(ri(0, 6)));
            }
            const uint32_t hit = T3;
            wrand(hit, 9); w32(hit + 0x10, any_moby()); w32(hit + 0x20, 1);
            seed_rand();
            return Args{{rec, ri(0, 2) == 0 ? 0u : any_moby(), coin() ? 0u : hit}, {}};
        }, [](A a) { flame_spray_burn(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
    }

    // ---- Kalebo III's environment-mapped objects: the classes' two states; their draw callbacks on their mobys (the display
    // packet cursor given a buffer, as batch 250 does), the camera moved; the mapping on each class's own meshes
    if (g_oracle_level == 16 || std::getenv("ORACLE_ONLY") == nullptr) {
        const bool l16 = g_oracle_level == 16;
        auto cam_vary = [](uint32_t m) {
            Vec4 c = ee_ref<Vec4>(m + 0x10);
            c.x += rf(-30, 30); c.y += rf(-30, 30); c.z += rf(-5, 15);
            if (ri(0, 3) != 0) ee_ref<Vec4>(OV(0x00166dc0)) = c;
        };
        states_check("envmap_1948", l16 ? 0x002e98e0u : 0u, envmap_1948_update, 2, 12, cam_vary);
        states_check("envmap_1953", l16 ? 0x002ea128u : 0u, envmap_1953_update, 2, 12, cam_vary);
        struct Cb { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t cls; };
        static const Cb cbs[] = {{"envmap_1943_draw", 0x002e8d40, envmap_1943_draw, 0x002e8e80}, {"envmap_1948_draw", 0x002e97a0, envmap_1948_draw, 0x002e98e0},
                                 {"envmap_1953_draw", 0x002e9fe8, envmap_1953_draw, 0x002ea128}};
        for (const Cb& cb : cbs) {
            const Cb c = cb;
            classcheck(c.name, l16 ? c.addr : 0u, c.fn, [cam_vary, c](uint32_t m) {
                w8(m + 0x20, 0);  // (the class's own update first, from its start: its meshes' bounding spheres)
                if (c.cls == 0x002e8e80) kal_class1943_update_l16(m);
                else if (c.cls == 0x002e98e0) kal_class1948_update_l16(m);
                else kal_class1953_update_l16(m);
                cam_vary(m);
                {  // (the camera before one of the class's meshes, its bounding sphere ahead: the mesh drawn)
                    const uint32_t spheres = c.cls == 0x002e8e80 ? 0x001dc600u : c.cls == 0x002e98e0 ? 0x001df2b0u : 0x001e5390u;
                    const uint32_t sph = spheres + u(ri(0, 1)) * 0x10;
                    Vec4 cam = ee_ref<Vec4>(OV(0x00166dc0));
                    if (ri(0, 4) != 0) {
                        const float r = std::fmax(ee_read<float>(sph + 0xc), 4.0f) * rf(1.2f, 3.0f), a = rf(-3.1f, 3.1f);
                        cam.x = ee_read<float>(sph) + r * std::cos(a); cam.y = ee_read<float>(sph + 4) + r * std::sin(a); cam.z = ee_read<float>(sph + 8) + rf(-2, 6);
                        if (ri(0, 2) == 0) {  // (or far back along the view, the mesh ahead at hundreds of units: about the on-screen test's depth 512)
                            const Vec4 f = ee_ref<Vec4>(OV(0x00166fd0));
                            const float d = rf(300.0f, 700.0f);
                            cam = Vec4{ee_read<float>(sph) - d * f.x, ee_read<float>(sph + 4) - d * f.y, ee_read<float>(sph + 8) - d * f.z, cam.w};
                        }
                        ee_ref<Vec4>(OV(0x00166dc0)) = cam;
                    }
                }
                render_view_setup();  // (the view from the moved camera)
                packet_cursor();
                seed_rand();
                if (std::getenv("B510_INFO") != nullptr) {  // (whether the port drew: the packet cursor moved)
                    static std::vector<uint8_t> keep;
                    uint8_t* ram = ee_get_ram_base();
                    keep.assign(ram, ram + 0x02000000);
                    const uint32_t before = ee_read<uint32_t>(OV(0x001611c0));
                    c.fn(m);
                    std::printf("INFO %s: the list grew by 0x%x\n", c.name, ee_read<uint32_t>(OV(0x001611c0)) - before);
                    std::memcpy(ram, keep.data(), keep.size());
                }
            }, 24, c.cls);
        }
        // the mapping: each class's meshes (the level's tables), the camera moved
        struct Uv { uint32_t addr, counts, normals, points; };
        static const Uv uvs[] = {{0x002e8b98, 0x00161ef0, 0x00161f20, 0x00161f00}, {0x002e90c8, 0x00161ef0, 0x00161f20, 0x00161f00},
                                 {0x002e95f8, 0x00161f90, 0x00161fa8, 0x00161f98}, {0x002e9b38, 0x00162008, 0x00162038, 0x00162018},
                                 {0x002e9e40, 0x00162008, 0x00162038, 0x00162018}};
        for (const Uv& x : uvs) {
            const Uv c = x;
            fcheck("envmap_uvs", l16 ? c.addr : 0u, 20, Ret::None, [level, c] {
                level();
                const uint32_t k = u(ri(0, 1)) * 4;
                Vec4 cam = ee_ref<Vec4>(0x0013f3d0);
                cam.x += rf(-20, 20); cam.y += rf(-20, 20); cam.z += rf(-3, 10);
                ee_ref<Vec4>(OV(0x00166dc0)) = cam;
                return Args{{ee_read<uint32_t>(c.counts + k), ee_read<uint32_t>(c.normals + k), ee_read<uint32_t>(c.points + k), 0, 0x01800000u}, {}};  // (a3 unread; t0 the out)
            }, [](A a) { envmap_uvs(static_cast<int32_t>(a.i[0]), a.i[1], a.i[2], a.i[4]); return uint64_t{0}; });
            if (!l16) break;
        }
    }

    // ---- the beams (Blarg, Orxon, Gemlik): a class-1068 moby of the level (or another moby given the pvars), its 16 points
    // around it with random strengths; the draw with the packet cursor given a buffer. The shards: spawned, their pvars varied
    {
        const bool gem = g_oracle_level == 13;
        auto beam_moby = [level, any_moby, gem]() -> uint32_t {
            level();
            uint32_t m = 0;
            std::vector<uint32_t> ms;
            for (uint32_t x = ee_read<uint32_t>(OV(0x0015ffd8)); x < ee_read<uint32_t>(OV(0x0015ffe0)); x += 0x100)
                if (ee_read<uint8_t>(x + 0x20) < 0xfe && ee_read<uint16_t>(x + 0xa6) == 1068 && ee_read<uint32_t>(x + 0x78) != 0) ms.push_back(x);
            m = ms.empty() ? any_moby() : ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            const uint32_t pts = ee_read<uint32_t>(m + 0x78) + (gem ? 0x210u : 0x230u);
            if (ms.empty() || ri(0, 2) == 0) {
                Vec4 q = ee_ref<Vec4>(m + 0x10);
                ee_ref<Vec4>(pts - 0x20) = q;
                for (uint32_t k = 0; k < 16; ++k) {
                    Vec4 p{q.x + rf(-2, 2) + 0.5f * static_cast<float>(k), q.y + rf(-2, 2), q.z + rf(-1, 3), coin() ? rf(0.9f, 1.0f) : rf(0, 1)};
                    ee_ref<Vec4>(pts + k * 0x10) = p;
                }
            }
            ee_ref<Vec4>(OV(0x00166dc0)) = ee_ref<Vec4>(m + 0x10);
            render_view_setup();  // (the view from the moved camera)
            packet_cursor();
            seed_rand();
            return m;
        };
        fcheck("beam_sparks", copy_here({{6, 0x002ff978}, {10, 0x002dc990}, {13, 0x00309bc8}}), 60, Ret::None, [beam_moby] { return Args{{beam_moby()}, {}}; },
               [](A a) { beam_sparks(a.i[0]); return uint64_t{0}; });
        // (the draw: the beam, its origin and points moved together, placed before the view where its quads reach the packets)
        fcheck("beam_draw", copy_here({{6, 0x002ff680}, {10, 0x002dc698}, {13, 0x003098d0}}), 40, Ret::None, [beam_moby, gem] {
            const uint32_t m = beam_moby();
            const uint32_t pts = ee_read<uint32_t>(m + 0x78) + (gem ? 0x210u : 0x230u);
            static std::vector<uint8_t> at0;  // (the beam as dealt: each candidate moves it from there)
            at0.assign(ee_get_ram_base() + (pts - 0x20), ee_get_ram_base() + (pts + 0x100));
            const Vec4 m0 = ee_ref<Vec4>(pts);  // (its first point put at the candidate)
            place_drawn([m] { beam_draw(m); }, [m, pts, m0](const Vec4& q) {
                std::memcpy(ee_get_ram_base() + (pts - 0x20), at0.data(), at0.size());
                const float dx = q.x - m0.x, dy = q.y - m0.y, dz = q.z - m0.z;
                for (uint32_t p = pts - 0x20; p < pts + 0x100; p += 0x10) {
                    if (p == pts - 0x10) continue;
                    wf(p, ee_read<float>(p) + dx); wf(p + 4, ee_read<float>(p + 4) + dy); wf(p + 8, ee_read<float>(p + 8) + dz);
                }
                ee_ref<Vec4>(m + 0x10) = Vec4{q.x, q.y, q.z, ee_read<float>(m + 0x1c)};
            });
            return Args{{m}, {}};
        },
               [](A a) { beam_draw(a.i[0]); return uint64_t{0}; });
        fcheck("beam_shard", copy_here({{6, 0x00300b90}, {10, 0x002dd010}, {13, 0x00303878}}), 60, Ret::None, [level] {
            level();
            const uint32_t m = moby_spawn(static_cast<uint32_t>(ri(1085, 1089)));
            if (m == 0) { std::printf("  beam_shard: no shard spawned (the class is not on this level)\n"); return Args{{0}, {}}; }
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            ee_ref<Vec4>(m + 0x10) = ee_ref<Vec4>(0x0013f3d0);
            wfrand(pv, 3, -1, 1); wfrand(pv + 0x10, 3, -0.5f, 0.5f);
            wfrand(m + 0x40, 3, -3.1f, 3.1f);
            w8(m + 0xbc, u(ri(0, 3) == 0 ? ri(0, 2) : ri(3, 200)));
            return Args{{m}, {}};
        }, [](A a) { if (a.i[0] != 0) beam_shard_update(a.i[0]); return uint64_t{0}; });
    }

    // ---- the electric arcs (Eudora, Batalia): a moby near the hero turned at random; the arcs' counts as the level has them
    {
        auto arc_moby = [level, any_moby]() -> uint32_t {
            level();
            const uint32_t m = any_moby();
            Vec4 q = ee_ref<Vec4>(0x0013f3d0); q.x += rf(-4, 4); q.y += rf(-4, 4);
            ee_ref<Vec4>(m + 0x10) = q;
            euler_to_mat4_vu(MM(m + 0xc0), Vec4{rf(-3, 3), rf(-3, 3), rf(-3, 3), 0.0f});
            seed_rand();
            return m;
        };
        fcheck("arcs_update", copy_here({{4, 0x002c46d0}, {8, 0x002da8d8}}), 30, Ret::None, [arc_moby] { return Args{{arc_moby()}, {}}; },
               [](A a) { arcs_update(a.i[0]); return uint64_t{0}; });
        fcheck("arc_between", copy_here({{4, 0x002c45b8}, {8, 0x002da7c0}}), 60, Ret::None, [arc_moby] {
            const uint32_t m = arc_moby();
            return Args{{m, u(ri(0, 23)), u(ri(0, 23)), u(ri(0, 4) == 0 ? ri(-1, 1) : ri(2, 12))}, {}};
        }, [](A a) { arc_between(a.i[0], static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]), static_cast<int32_t>(a.i[3])); return uint64_t{0}; });
        fcheck("arc_sparks", copy_here({{4, 0x002c41e0}, {8, 0x002da3f0}}), 60, Ret::None, [arc_moby] {
            const uint32_t m = arc_moby();
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            ee_ref<Vec4>(T) = Vec4{q.x, q.y, q.z + 1, 1.0f};
            ee_ref<Vec4>(T + 0x10) = Vec4{q.x + rf(-3, 3), q.y + rf(-3, 3), q.z + rf(0, 3), 1.0f};
            return Args{{m, T, T + 0x10, u(ri(0, 4) == 0 ? ri(-1, 1) : ri(2, 12))}, {rf(0, 1e-5f), rf(0, 1e-5f)}};
        }, [](A a) { arc_sparks(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]), a.f[0], a.f[1]); return uint64_t{0}; });
    }

    // ---- the gold weapons' scene start (Novalis, Gemlik): not compared. The original's first 989snd call (snd_cmd_16 /
    // snd_flush) spins on the SIF in the interpreter at every case, and the scene's stream load (0x2454c8's disc read, then its
    // vsync wait) never completes in a native run here either (batch 136 skips the generated copy for the same reason); the
    // port was read against both levels' generated copies line by line
    skip_check("gold_weapon_scene", copy_here({{1, 0x002aea70}, {13, 0x002a5390}}), "989snd's SIF and the scene's stream load: not in the oracle (batch 136)");

    // ---- the tile grids (Umbris, Pokitaru, Hoven, Gemlik): a tile's links with some of the others moved to 16 from it (and a
    // little off); a grouped moby's height given to its group; the Raritanium pickup with Ratchet near or not
    {
        const uint32_t links = copy_here({{7, 0x0030cd48}, {11, 0x0030e730}, {12, 0x002bf770}, {13, 0x0030a488}});
        const uint32_t tbase = copy_here({{7, 0x001dce40}, {11, 0x001da880}, {12, 0x001cbf40}, {13, 0x001d9bc0}});
        const int tcount = g_oracle_level == 7 ? 36 : g_oracle_level == 11 ? 20 : g_oracle_level == 13 ? 22 : 17;
        fcheck("tile_links", links, 60, Ret::None, [level, tbase, tcount] {
            level();
            const int i = ri(0, tcount - 1);
            const uint32_t me = tbase + u(i) * 0x1190;
            for (int j = 0; j < tcount; ++j) {
                if (j == i || ri(0, 2) == 0) continue;
                const uint32_t o = tbase + u(j) * 0x1190;
                const float dx = 16.0f * static_cast<float>(ri(-1, 1)), dy = 16.0f * static_cast<float>(ri(-1, 1));
                wf(o, ee_read<float>(me) + dx + (ri(0, 5) == 0 ? rf(-0.2f, 0.2f) : 0.0f));
                wf(o + 4, ee_read<float>(me + 4) + dy + (ri(0, 5) == 0 ? rf(-0.2f, 0.2f) : 0.0f));
            }
            return Args{{u(i)}, {}};
        }, [](A a) { tile_links(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
        const uint32_t height = copy_here({{11, 0x0030e690}, {12, 0x002bf6d0}});
        fcheck("tile_group_height", height, 40, Ret::None, [level, tcount] {
            level();
            std::vector<uint32_t> ms;
            const uint32_t list = ee_read<uint32_t>(OV(0x0015ffd8));
            const uint32_t at = g_oracle_level == 11 ? 4u : 0xcu;
            for (uint32_t m = list; m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
                if (ee_read<uint8_t>(m + 0x20) >= 0xfe || ee_read<uint8_t>(m + 0x21) == 0xff) continue;
                const uint32_t g = ee_read<uint32_t>(OV(0x001ab840) + (ee_read<uint8_t>(m + 0x21) << 2));
                if (g == 0) continue;
                bool fine = true;  // (the members' tiles in the buffer: the classes that call it)
                for (uint32_t e = g;; e += 2) {
                    const uint32_t x = list + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                    const uint32_t pv = ee_read<uint32_t>(x + 0x78);
                    const int32_t t = pv == 0 ? -1 : ee_read<int32_t>(pv + at);
                    if (t < 0 || t >= tcount) { fine = false; break; }
                    if (ee_read<int16_t>(e) < 0) break;
                }
                if (fine) ms.push_back(m);
            }
            if (ms.empty()) { std::printf("  tile_group_height: no grouped moby on this level\n"); return Args{{0}, {}}; }
            const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            wf(m + 0x18, ee_read<float>(m + 0x18) + rf(-5, 5));
            return Args{{m}, {}};
        }, [](A a) { if (a.i[0] != 0) tile_group_height(a.i[0]); return uint64_t{0}; });
        fcheck("raritanium_pickup", copy_here({{11, 0x002cb790}, {12, 0x002bf9b8}}), 40, Ret::None, [level] {  // (on a pickup spawned near the hero)
            level();
            const uint32_t m = moby_spawn(37);
            if (m == 0) { std::printf("  raritanium_pickup: no pickup spawned (the class is not on this level)\n"); return Args{{0}, {}}; }
            Vec4 q = ee_ref<Vec4>(0x0013f3d0); q.x += rf(-3, 3); q.y += rf(-3, 3);
            ee_ref<Vec4>(m + 0x10) = q;
            return Args{{m}, {}};
        }, [](A a) { if (a.i[0] != 0) raritanium_pickup_update(a.i[0]); return uint64_t{0}; });
    }

    // ---- the scrolling textures (Batalia, Pokitaru, Hoven): every copy (set) on its level's tables; the offsets' speeds and
    // places varied, the packet cursor given a buffer
    {
        struct Sc { int level; uint32_t addr; int set; };
        static const Sc meshes[] = {{8, 0x002f0c18, 0}, {8, 0x002f0cf0, 1}, {8, 0x00308e90, 2}, {11, 0x0031d9a0, 3}, {11, 0x0031da78, 4},
                                    {11, 0x0031e780, 5}, {11, 0x0031e858, 6}, {12, 0x0030bd90, 7}};
        static const Sc steps[] = {{8, 0x002f1378, 0}, {8, 0x002f1460, 1}, {8, 0x003095e0, 2}, {8, 0x003096c8, 3}, {11, 0x0031e120, 4},
                                   {11, 0x0031e208, 5}, {11, 0x0031ef80, 6}, {11, 0x0031f068, 7}, {8, 0x002f12a0, 8}, {8, 0x00309508, 9},
                                   {11, 0x0031e048, 10}, {11, 0x0031eea8, 11}};
        static const uint32_t offs[] = {0x00161e10, 0x00161e20, 0x001623f8, 0x00162598, 0x001625a8, 0x00162618, 0x00162628, 0x001620f8};
        bool any = false;
        for (const Sc& x : meshes) {
            if (x.level != g_oracle_level) continue;
            any = true;
            const Sc c = x;
            fcheck("uv_scroll_meshes", c.addr, 12, Ret::None, [level, c] {
                level();
                const uint32_t idx = u(ri(0, 1));
                wfrand(offs[c.set] + idx * 8, 2, -1, 1);
                packet_cursor();
                return Args{{idx}, {}};
            }, [c](A a) { uv_scroll_meshes(c.set, a.i[0]); return uint64_t{0}; });
        }
        for (const Sc& x : steps) {
            if (x.level != g_oracle_level) continue;
            any = true;
            const Sc c = x;
            fcheck("uv_scroll_step", c.addr, 30, Ret::None, [level] {
                level();
                wf(0x0015ed7c, rf(0.5f, 2.0f));
                return Args{{u(ri(0, 1))}, {}};
            }, [c](A a) { uv_scroll_step(c.set, a.i[0]); return uint64_t{0}; });
        }
        if (!any) { fcheck("uv_scroll_meshes", 0, 0, Ret::None, nullptr, nullptr); fcheck("uv_scroll_step", 0, 0, Ret::None, nullptr, nullptr); }
        fcheck("uv_offset_pairs", copy_here({{8, 0x00258090}, {11, 0x002739c0}, {12, 0x002668d4}}), 40, Ret::None, [level] {
            level();
            wfrand(T, 64, -2, 2);
            wfrand(T3, 2, -1, 1);
            return Args{{T2, T, u(ri(2, 30)), T3}, {}};
        }, [](A a) { uv_offset_pairs(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), a.i[3]); return uint64_t{0}; });
        const bool scroll_level = g_oracle_level == 8 || g_oracle_level == 11 || g_oracle_level == 12;
        for (uint32_t addr : {copy_here({{8, 0x002f0f68}, {11, 0x0031dd10}, {12, 0x0030bba0}}), copy_here({{8, 0x003091d0}, {11, 0x0031eb70}})}) {
            if (addr == 0 && scroll_level) continue;
            fcheck("mesh_colours_scale", addr, 30, Ret::None, [level] {
                level();
                const uint32_t tabs = T3, cnts = T3 + 0x40;  // (three meshes of the test area's colours)
                for (uint32_t i = 0; i < 3; ++i) { w32(tabs + 4 * i, T + i * 0x100); w32(cnts + 4 * i, u(ri(0, 20))); wrand(T + i * 0x100, 32); }
                return Args{{tabs, u(ri(0, 3)), cnts, u(ri(0, 1))}, {rf(0, 1.2f), rf(0, 1.2f), rf(0, 1.2f), rf(0, 1.2f)}};
            }, [](A a) { mesh_colours_scale(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2], a.i[3], a.f[0], a.f[1], a.f[2], a.f[3]); return uint64_t{0}; });
            if (addr == 0) break;
        }
        for (uint32_t addr : {copy_here({{8, 0x002f1158}, {11, 0x0031df00}}), copy_here({{8, 0x003093c0}, {11, 0x0031ed60}})}) {
            fcheck("wave_step", addr, 30, Ret::None, [level] {
                level();
                wfrand(T + 8, 2, -1.2f, 1.2f);
                wf(0x0015ed7c, rf(0.5f, 2.0f));
                return Args{{T, T + 8}, {rf(-0.5f, 0.5f), rf(-0.5f, 0.5f), rf(0, 3)}};
            }, [](A a) { wave_step(a.i[0], a.i[1], a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
            if (addr == 0) break;
        }
    }

    // ---- the deadly zones (Blarg, Batalia, Gemlik, Veldin's finale): Ratchet put in one of the zone's boxes (of either list)
    // or near; his state, his vehicle mode and his ride varied
    states_check("deadly_zones", copy_here({{6, 0x002f7930}, {8, 0x00302a78}, {13, 0x003036f0}, {18, 0x002ec858}}), deadly_zones_update, 3, 20,
                 [any_moby](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78), P = 0x0013f350;
        std::vector<int32_t> boxes;
        for (uint32_t k = 0; k < 64; ++k) { const int32_t b = ee_read<int32_t>(pv + 4 * k); if (b >= 0 && b < 400) boxes.push_back(b); }
        if (!boxes.empty() && ri(0, 3) != 0) {
            const int32_t b = boxes[static_cast<size_t>(ri(0, static_cast<int>(boxes.size()) - 1))];
            if (ri(0, 2) == 0) w32(pv + (coin() ? 0x80u : 0u) + 4 * u(ri(0, 31)), u(b));  // (the box in either list: a level whose zones have one list only)
            const uint32_t box = ee_read<uint32_t>(OV(0x001600ec)) + u(b << 7);
            Vec4 q = ee_ref<Vec4>(P + 0x80);
            for (uint32_t k = 0; k < 3; ++k) (&q.x)[k] = ee_read<float>(box + 0x30 + 4 * k) + rf(-0.3f, 0.3f);
            ee_ref<Vec4>(P + 0x80) = q;
        }
        if (ri(0, 3) == 0) w32(P + 0x2084, coin() ? 0x32u : 0x77u);
        if (ri(0, 4) == 0) w8(P + 0x20a4, u(ri(1, 2)));
        if (ri(0, 2) == 0) { w32(P + 0x15f0, coin() ? 0u : any_moby()); w16(P + 0x15f4, coin() ? 0x45u : 0u); }
        w32(OV(0x0015f5cc), R() & 0xffff);
    });

    // ---- the carried mobys (Hoven, Veldin's finale): a carrier and up to six records of other mobys of the level (some -1)
    {
        auto attach_args = [level, any_moby]() {
            level();
            const uint32_t carrier = any_moby();
            wfrand(carrier + 0x40, 3, -3, 3);
            const int n = ri(0, 6);
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), cnt = (ee_read<uint32_t>(OV(0x0015ffe0)) - first) >> 8;
            for (int i = 0; i < n; ++i) {
                const uint32_t rec = T + u(i) * 0x30;
                wfrand(rec, 3, -4, 4); w32(rec + 0xc, 0); wfrand(rec + 0x10, 3, -3, 3); w32(rec + 0x1c, 0);
                w32(rec + 0x20, ri(0, 4) == 0 ? 0xffffffffu : u(ri(0, static_cast<int>(cnt) - 1)));
            }
            return Args{{carrier, T, u(n)}, {}};
        };
        fcheck("attach_bind", copy_here({{12, 0x0027b268}, {18, 0x00265250}}), 40, Ret::None, attach_args,
               [](A a) { attach_bind(a.i[0], a.i[1], static_cast<int32_t>(a.i[2])); return uint64_t{0}; });
        fcheck("attach_carry", copy_here({{12, 0x0027b370}, {18, 0x00265358}}), 40, Ret::None, attach_args,
               [](A a) { attach_carry(a.i[0], a.i[1], static_cast<int32_t>(a.i[2])); return uint64_t{0}; });
    }

    // ---- the shock waves (Quartu, Veldin's finale): a wave spawned near Ratchet, its growth, life (fading or not, ending) and
    // damage varied
    fcheck("shockwave", copy_here({{15, 0x0029ead0}, {18, 0x002ab7c0}}), 60, Ret::None, [level] {
        level();
        const uint32_t m = moby_spawn(1427);
        if (m == 0) { std::printf("  shockwave: no wave spawned (the class is not on this level)\n"); return Args{{0}, {}}; }
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        Vec4 q = ee_ref<Vec4>(0x0013f3d0); q.x += rf(-4, 4); q.y += rf(-4, 4);
        ee_ref<Vec4>(m + 0x10) = q;
        wf(m + 0x2c, rf(0.1f, 3)); wf(pv, rf(0, 0.5f)); wf(pv + 0xc, rf(0, 4));
        w32(pv + 8, u(ri(0, 3) == 0 ? ri(0, 1) : ri(2, 80)));
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) shockwave_update(a.i[0]); return uint64_t{0}; });

    // ---- Veldin's finale's quad meshes (the camera put near the mesh, the view from it, the packet cursor on a buffer) and its
    // stop hooks
    {
        struct Qd { uint32_t addr; int set; uint32_t points; };
        static const Qd qds[] = {{0x002f2620, 0, 0x001da3c0}, {0x002f27c8, 1, 0x001da8d0}, {0x002f2970, 2, 0x001db030}};
        for (const Qd& x : qds) {
            const Qd c = x;
            fcheck("finale_quads_draw", g_oracle_level == 18 ? c.addr : 0u, 30, Ret::None, [level, any_moby, c] {
                level();
                const uint32_t m = any_moby();
                Vec4 cam = ee_ref<Vec4>(c.points + u(ri(0, 20)) * 0x10);
                const float a = rf(-3.1f, 3.1f), r = rf(4, 20);
                cam.x += r * std::cos(a); cam.y += r * std::sin(a); cam.z += rf(0, 6);
                if (ri(0, 4) != 0) ee_ref<Vec4>(OV(0x00166dc0)) = cam;
                render_view_setup();
                packet_cursor();
                wf(L18_ADDR(0x00162308), rf(-1, 1));
                // (most of the time the camera put back from one of the mesh's points along the view's forward row, the
                // mesh before it: its quads reach the packets)
                const Vec4 fwd = ee_ref<Vec4>(OV(0x00166fd0));
                place_drawn([c, m] { finale_quads_draw(c.set, m); }, [](const Vec4& q) {
                    ee_ref<Vec4>(OV(0x00166dc0)) = q;
                    render_view_setup();
                }, [c, fwd, m](int k) {
                    Vec4 p = ee_ref<Vec4>(c.points + u(ri(0, 20)) * 0x10);
                    p.z += ee_read<float>(m + 0x18) - ee_read<float>(ee_read<uint32_t>(m + 0x78));  // (the mesh's lift)
                    const float r = rf(3.0f, k < 30 ? 15.0f : 40.0f);
                    return Vec4{p.x - r * fwd.x + rf(-2, 2), p.y - r * fwd.y + rf(-2, 2), p.z - r * fwd.z + rf(-1, 3), 1.0f};
                });
                if (std::getenv("B510_INFO") != nullptr) {
                    static std::vector<uint8_t> keep;
                    uint8_t* ram = ee_get_ram_base();
                    keep.assign(ram, ram + 0x02000000);
                    const uint32_t before = ee_read<uint32_t>(OV(0x001611c0));
                    finale_quads_draw(c.set, m);
                    std::printf("INFO finale_quads_draw: the list grew by 0x%x\n", ee_read<uint32_t>(OV(0x001611c0)) - before);
                    std::memcpy(ram, keep.data(), keep.size());
                }
                return Args{{m}, {}};
            }, [c](A a) { finale_quads_draw(c.set, a.i[0]); return uint64_t{0}; });
            if (g_oracle_level != 18) break;
        }
        for (const auto& h : {std::pair<uint32_t, uint32_t>{0x002d6280, 5}, std::pair<uint32_t, uint32_t>{0x002fcd00, 8}}) {
            const auto c = h;
            fcheck("finale_stop_hook", g_oracle_level == 18 ? c.first : 0u, 30, Ret::None, [level, any_moby] {
                level();
                const uint32_t m = any_moby();
                w8(m + 0x53, u(ri(0, 1)));
                w16(m + 0x34, R());
                return Args{{m}, {}};
            }, [c](A a) { finale_stop_hook(a.i[0], c.second); return uint64_t{0}; });
            if (g_oracle_level != 18) break;
        }
    }

    // ---- the flash (Blarg, Veldin's finale): spawned by its own spawn, then updated in its states; the camera flag on the
    // level's cameras (type 0x12 or not); the state queries on a moby in each state
    {
        auto flash_args = [level]() {
            level();
            ee_ref<Vec4>(T) = ee_ref<Vec4>(0x0013f3d0);
            return Args{{T, u(ri(-2, 60)), R(), R()}, {}};
        };
        fcheck("flash_spawn", copy_here({{6, 0x0030bef8}, {18, 0x002fb5e0}}), 30, Ret::Int32, flash_args,
               [](A a) { return uint64_t{flash_spawn(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2], a.i[3])}; });
        fcheck("flash", copy_here({{6, 0x0030be60}, {18, 0x002fb548}}), 60, Ret::None, [flash_args] {
            const Args a = flash_args();
            if (flash_spawn(a.i[0], static_cast<int32_t>(a.i[1]) == 0 ? 1 : static_cast<int32_t>(a.i[1]), a.i[2], a.i[3]) == 0) {
                std::printf("  flash: no flash spawned (the class is not on this level)\n");
                return Args{{0}, {}};
            }
            uint32_t m = 0;
            for (uint32_t x = ee_read<uint32_t>(OV(0x0015ffd8)); x < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x100 * 64; x += 0x100)
                if (ee_read<uint16_t>(x + 0xa6) == 0x76a && ee_read<uint8_t>(x + 0x20) < 0xfe) m = x;
            if (m == 0) return Args{{0}, {}};
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(ri(0, 4) == 0 ? 0 : ri(0, 5) == 0 ? 2 : 1));
            wf(pv, rf(0, 2.2f));
            return Args{{m}, {}};
        }, [](A a) { if (a.i[0] != 0) flash_update(a.i[0]); return uint64_t{0}; });
        fcheck("camera_type12_flag", copy_here({{6, 0x00316330}, {18, 0x00306338}}), 40, Ret::None, [level] {
            level();
            const uint32_t cam = OV(0x00167150) + u(ri(0, 47)) * 0xa0;  // (a camera record; its type and record index made)
            w16(cam + 0x86, coin() ? 0x12u : u(ri(0, 0x20)));
            w16(cam + 0x84, u(ri(0, 3)));
            return Args{{cam}, {}};
        }, [](A a) { camera_type12_flag(a.i[0]); return uint64_t{0}; });
        auto state_args = [level, any_moby]() {
            level();
            const uint32_t m = any_moby();
            w8(m + 0x20, u(ri(0, 10)));
            return Args{{m}, {}};
        };
        fcheck("state_query_blarg", copy_here({{6, 0x003047f8}}), 40, Ret::Int32, state_args, [](A a) { return uint64_t{state_query_blarg(a.i[0])}; });
        fcheck("state_query_finale", copy_here({{18, 0x002dfba0}}), 40, Ret::Int32, state_args, [](A a) { return uint64_t{state_query_finale(a.i[0])}; });
    }

    // ---- small helpers
    fcheck("level_movie_play", copy_here({{2, 0x00298c68}, {8, 0x002a1880}, {16, 0x002923d0}}), 30, Ret::None, [level] {
        level();
        w32(0x0015ed80, u(ri(0, 1)));
        if (coin()) w32(0x001415f8, 0); else { w32(0x001415f8, 1); w8(0x00141401, u(ri(1, 3))); }  // (never the movie itself)
        return Args{{u(ri(-2, 20))}, {}};
    }, [](A a) { level_movie_play(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    fcheck("point_turned", copy_here({{7, 0x00288968}, {9, 0x0027fe88}}), 40, Ret::None, [level] {
        level();
        wfrand(T, 4, -20, 20); wfrand(T + 0x20, 4, -20, 20); wfrand(T + 0x30, 3, -3.1f, 3.1f); wfrand(T + 0x40, 3, -3.1f, 3.1f);
        return Args{{T, T + 0x10, T + 0x20, T + 0x30, T + 0x40}, {}};
    }, [](A a) { point_turned(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4]); return uint64_t{0}; });
    fcheck("camera_flag_88", copy_here({{8, 0x00313ba8}, {9, 0x00312888}, {12, 0x00312f58}}), 10, Ret::None, [level] {
        level();
        if (coin()) w32(OV(0x00166e00), T3);
        return Args{{}, {}};
    }, [](A) { camera_flag_88(); return uint64_t{0}; });
    fcheck("push_without_direction", copy_here({{10, 0x0024c7f0}, {13, 0x00265d68}}), 40, Ret::None, [level, any_moby] {
        level();
        const uint32_t m = any_moby(), src = any_moby();
        ee_ref<Vec4>(T) = ee_ref<Vec4>(m + 0x10);
        return Args{{m, src, coin() ? 0x10001u : R(), T}, {rf(0, 5)}};
    }, [](A a) { push_without_direction(a.i[0], a.i[1], a.i[2], a.i[3], a.f[0]); return uint64_t{0}; });

    // ---- small spawn and group helpers: from a random moby of the level, at a place near it
    {
        auto src_args = [level, any_moby]() {
            level();
            const uint32_t src = any_moby();
            w32(src + 0x30, R()); wfrand(src + 0x40, 3, -3, 3); w32(src + 0x38, R()); w32(src + 0x3c, R());
            ee_ref<Vec4>(T) = ee_ref<Vec4>(src + 0x10);
            return Args{{src, T}, {}};
        };
        struct Sp { int level; uint32_t addr; uint32_t cls; };
        static const Sp sps[] = {{4, 0x002ce080, 0x1f3}, {4, 0x002d34d8, 0x23a}, {12, 0x002f1118, 0x1f3}};
        bool any = false;
        for (const Sp& x : sps) {
            if (x.level != g_oracle_level) continue;
            any = true;
            const Sp c = x;
            fcheck("spawn_like", c.addr, 30, Ret::Int32, src_args, [c](A a) { return uint64_t{spawn_like(a.i[0], a.i[1], c.cls)}; });
        }
        if (!any) fcheck("spawn_like", 0, 0, Ret::None, nullptr, nullptr);
        fcheck("spawn_like_gaspar", copy_here({{9, 0x002efda8}}), 30, Ret::Int32, src_args, [](A a) { return uint64_t{spawn_like_gaspar(a.i[0], a.i[1])}; });
        for (const auto& x : {std::pair<uint32_t, uint32_t>{0x002dd848, 0x462}, std::pair<uint32_t, uint32_t>{0x002eb8c0, 0x781}}) {
            const auto c = x;
            fcheck("spawn_over_at_59", g_oracle_level == 10 ? c.first : 0u, 30, Ret::Int32, src_args, [c](A a) { return uint64_t{spawn_over_at_59(a.i[0], c.second)}; });
            if (g_oracle_level != 10) break;
        }
        for (uint32_t addr : {0x002e0138u, 0x002e47d8u}) {
            fcheck("group_wake", g_oracle_level == 10 ? addr : 0u, 30, Ret::None, [level] {
                level();
                std::vector<uint32_t> ms;
                for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                    if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint8_t>(m + 0x21) != 0xff && ee_read<uint32_t>(OV(0x001ab840) + (ee_read<uint8_t>(m + 0x21) << 2)) != 0) ms.push_back(m);
                if (ms.empty()) { std::printf("  group_wake: no grouped moby\n"); return Args{{0}, {}}; }
                const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                for (uint32_t e = ee_read<uint32_t>(OV(0x001ab840) + (ee_read<uint8_t>(m + 0x21) << 2));; e += 2) {  // (its members in state 1 or not)
                    w8(ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8) + 0x20, u(ri(0, 3)));
                    if (ee_read<int16_t>(e) < 0) break;
                }
                return Args{{m}, {}};
            }, [](A a) { if (a.i[0] != 0) group_wake(a.i[0]); return uint64_t{0}; });
            if (g_oracle_level != 10) break;
        }
    }

    // ---- the particle types 0x31 (Veldin, Rilgar's copy) and 0x21 / 0x3f (Veldin): new records about the hero, their life,
    // place, tail and velocity varied (as batch 191 / 340)
    {
        auto bullet_args = [level]() {
            level();
            const uint32_t q = alloc_particle(0x31);
            const Vec4 h = VV(0x0013f3d0);
            VV(q + 0x10) = Vec4{h.x + rf(-6, 6), h.y + rf(-6, 6), h.z + rf(-2, 4), 1.0f};
            VV(q + 0x20) = VV(q + 0x10);
            VV(q + 0x20).x += rf(-2, 2);
            VV(q + 0x20).z += rf(-2, 2);
            wfrand(q + 0x30, 3, -2.0f, 2.0f);
            w32(q + 0x3c, ri(0, 2) == 0 ? 0u : ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100u * u(ri(0, 40)));
            w16(q + 0xa, u(ri(0, 4) == 0 ? ri(-1, 2) : ri(2, 45)));
            seed_rand();
            return Args{{q}, {}};
        };
        fcheck("bullet_particle", copy_here({{0, 0x00271d20}, {5, 0x0029bda0}}), 80, Ret::None, bullet_args,
               [](A a) { bullet_particle_update(a.i[0]); return uint64_t{0}; });
        for (const auto& x : {std::pair<uint32_t, uint32_t>{0x0026f048, 0x21}, std::pair<uint32_t, uint32_t>{0x002738b0, 0x3f}}) {
            const auto c = x;
            fcheck("timed_particle", g_oracle_level == 0 ? c.first : 0u, 40, Ret::None, [level, c] {
                level();
                const uint32_t q = alloc_particle(c.second);
                w16(q + 0xa, u(ri(0, 3) == 0 ? ri(-1, 2) : ri(2, 45)));
                return Args{{q}, {}};
            }, [](A a) { timed_particle_update(a.i[0]); return uint64_t{0}; });
            if (g_oracle_level != 0) break;
        }
    }

    g_extra_noop_hooks.clear();
    g_pre_call = nullptr;
    g_ignored_words.clear();
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
