// Oracle batch 136 (task 0383, Novalis part 2): what a play-through of Novalis still reached unported. Run with ORACLE_LEVEL=1
// (Novalis's own original against the relocated port; needs the ISO); the shared functions also on level 0.
//  - particle_16_spawn 0x26ced0 (Novalis 0x281f30): the sparks' spawner (the Birdbot's, the rocks' 696..698, classes 27 / 751 ...);
//  - particle_20_update 0x271fc0: particle type 0x20's update;
//  - point_on_screen_depth 0x2003d8 and the ship's draw callbacks 0x28e670 / 0x28efc8 / 0x28f238 (every level's landing / take-off);
//  - Novalis's own draw callbacks L1:0x302438 / 0x3094f0 / 0x30bfa8 / 0x30de68 / 0x30f0e0 (with 0x30ef18);
//  - the gold weapon cases' scene start L1:0x2aea70: batch 510 checks its shared port (see below).

#include "ovl_oracle.h"

#include <game/ovl/aridia_ship.h>
#include <game/ovl/empty_class_updates.h>
#include <game/ovl/novalis_draw_cbs.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_16_spawn.h>
#include <game/ovl/shared_class1135.h>
#include <game/ovl/shared_class325.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/ship_draw_cbs.h>
#include <game/ovl/variants_l1.h>

#include <cstring>

namespace rac1::oracle {

namespace {
// A check whose original is Novalis's own function at `l1addr` (as batch 103's).
void l1check136(const char* name, uint32_t l1addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (g_oracle_level != 1) {
        std::printf("%-26s L1:0x%08x  skipped (Novalis only: run with ORACLE_LEVEL=1)\n", name, l1addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L1:0x%08x\n", name, l1addr);
    if (!check_fn(name, l1addr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
// the point at `p` put about the camera, until the port's screen test (0x2002e0) sees it with radius r (as batch 128's); w = 1
void into_view(uint32_t p, float r, float spread) {
    const Vec4 c = VV(OV(0x00166dc0));
    for (int k = 0; k < 200; ++k) {
        VV(p) = Vec4{c.x + rf(-spread, spread), c.y + rf(-spread, spread), c.z + rf(-spread / 3, spread / 3), 1.0f};
        if (point_on_screen(p, r) != -1) return;
    }
}
}  // namespace

void batch_136() {
    if (g_only_batch != 136) return;
    g_batch = 136;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // (the particle update's saved return address)
    g_ignored_words.push_back(OV(0x0016018c));
    g_extra_noop_hooks.clear();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime

    // the level after its first gameplay frame (the port's)
    static std::vector<uint8_t> snap;
    static int snap_level = -1;
    auto level = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty() || snap_level != g_oracle_level) {
            veldin_boot();
            level_setup_body(g_oracle_level == 0 ? 1 : g_oracle_level, 0);
            w32(OV(0x0016c060), 0xf);
            gameplay_frame();
            render_view_setup();  // (the view matrices of the render frame: zero after an update alone)
            snap.assign(ram, ram + 0x02000000);
            snap_level = g_oracle_level;
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    // the level's live mobys (of class `cls`, any when -1)
    auto mobys_of = [level](int cls) {
        level();
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && (cls < 0 || ee_read<int16_t>(m + 0xa6) == cls)) v.push_back(m);
        return v;
    };
    auto pick = [](const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; };

    // ---- the sparks' spawner: a position / velocity, two colours, the frames (0 and negative too: the division by the
    // frames), the size; the pool sometimes full (its next index 0x160170 at the end) or nearly
    check("particle_16_spawn", 0x0026ced0, 200, Ret::None, [level] {
        level();
        wfrand(OUT, 4, -300, 300);
        wfrand(OUT + 0x10, 4, -2, 2);
        const int k = ri(0, 9);
        if (k == 0) w32(OV(0x00160170), 0x800);
        else if (k == 1) w32(OV(0x00160170), u(ri(0x7f0, 0x7ff)));
        wf(0x0015ed70, ri(0, 3) == 0 ? rf(0.5f, 3.0f) : 1.0f);
        seed_rand();
        const int32_t frames = ri(0, 7) == 0 ? ri(-3, 1) : ri(1, 300);
        const float size = ri(0, 7) == 0 ? rf(-2, 0) : rf(0, 8);
        return Args{{OUT, OUT + 0x10, R(), R(), u(frames)}, {size}};
    }, [](A a) { particle_16_spawn(a.i[0], a.i[1], a.i[2], a.i[3], static_cast<int32_t>(a.i[4]), a.f[0]); return uint64_t{0}; });

    // ---- particle type 0x20's update: a particle of the pool with its frames (the last one too: freed), rate, colours and sizes varied
    check("particle_20_update", 0x00271fc0, 200, Ret::None, [level] {
        level();
        const uint32_t p = alloc_particle(0x20);
        w16(p + 0xa, u(ri(0, 5) == 0 ? ri(-1, 1) : ri(2, 200)));
        wfrand(p + 0x20, 2, -3, 3);
        w32(p + 0x28, R());
        w32(p + 0x2c, R());
        wf(p + 0x30, rf(-0.05f, 0.05f));
        wf(0x0015ed64, ri(0, 3) == 0 ? rf(0.5f, 3.0f) : 1.0f);
        return Args{{p}, {}};
    }, [](A a) { particle_20_update(a.i[0]); return uint64_t{0}; });

    // ---- the screen test with its depth byte: a point about the camera (seen, near the edges, behind), a radius
    check("point_on_screen_depth", 0x002003d8, 300, Ret::Int64, [level] {
        level();
        const float r = ri(0, 3) == 0 ? 0.0f : rf(0, coin() ? 4.0f : 400.0f);
        const Vec4 c = VV(OV(0x00166dc0));
        const float s = coin() ? 30.0f : 300.0f;
        if (ri(0, 2) != 0) into_view(OUT, r, s);  // (mostly seen: the depth byte's path)
        else VV(OUT) = Vec4{c.x + rf(-s, s), c.y + rf(-s, s), c.z + rf(-s / 3, s / 3), 1.0f};
        if (ri(0, 3) == 0) wf(OUT + 0xc, rf(0, 2));
        w32(OUT + 0x10, R());
        return Args{{OUT, OUT + 0x10}, {r}};
    }, [](A a) { return static_cast<uint64_t>(point_on_screen_depth(a.i[0], a.i[1], a.f[0])); });

    // ---- the ship's draw callbacks: the level's ship (*(0x13e030)) or another moby, moved about the camera; the take-off's / flight's
    // state (the arrival block 0x13e030.., its trails) left as the level start makes it or varied; game mode 6 or not
    auto ship_gen = [level, mobys_of, pick] {
        const std::vector<uint32_t> any = mobys_of(-1);
        level();
        const uint32_t ship = ee_read<uint32_t>(0x0013e030);
        const uint32_t m = ship != 0 && coin() ? ship : pick(any);
        into_view(m + 0x10, coin() ? 0.0f : 32.0f, coin() ? 20.0f : 120.0f);
        if (coin()) {  // (its +0 quadword, x 1024: the point the shadow's screen test scales back)
            const Vec4 p = VV(m + 0x10);
            VV(m) = Vec4{p.x * 1024.0f, p.y * 1024.0f, p.z * 1024.0f, coin() ? 1024.0f : rf(0, 2048)};
        }
        if (coin()) w32(OV(0x0015f5c4), 6);
        if (coin()) {
            wfrand(0x0013e030 + 0x3c, 3, 0, 40);
            w32(0x0013e030 + 0x50, R() & 0x1fu);
            w32(0x0013e030 + 0x54, u(ri(0, 0x20)));
            wfrand(0x0013e030 + 0xc0, 0x100, -400, 400);
        }
        if (coin()) w8(m + 0xbc, R());
        if (coin()) w16(m + 0xb2, u(ri(0, 0x20)));
        w16(0x0013e030 + 0x26, u(ri(0, 2)));
        w16(0x0013e030 + 0x24, u(ri(0, 400)));
        w32(0x0013e030 + 0x20, coin() ? 3u : u(ri(0, 4)));
        seed_rand();
        return Args{{m}, {}};
    };
    {
    // (stage 5, task C2c) ship_shadow_draw reaches segment_collide: A2's collision scratch
    // ranges left out of this check (see batch 43's checks of it)
    const CollisionScratchIgnored spr_ignored;
    check("ship_shadow_draw", 0x0028e670, 100, Ret::None, ship_gen, [](A a) { ari_ship_glow_draw_l2(a.i[0]); return uint64_t{0}; });
    }
    check("ship_flame_draw", 0x0028efc8, 100, Ret::None, ship_gen, [](A a) { ari_ship_engine_glow_draw_l2(a.i[0]); return uint64_t{0}; });
    check("ship_trails_draw", 0x0028f238, 100, Ret::None, ship_gen, [](A a) { ari_ship_takeoff_draw_l2(a.i[0]); return uint64_t{0}; });

    // ---- Novalis's own draw callbacks: a moby of the class (any when the level places none) moved about the camera, its
    // pvars' floats / counters and the level's scroll state varied
    struct Cb {
        const char* name;
        uint32_t addr;
        void (*fn)(uint32_t);
        int cls;
    };
    // (classes 832, 1135, 1510 and 1818: the shared ports, shared_*.cpp, registered for them since the level's re-glue)
    const Cb cbs[] = {{"nov_class832_draw", 0x00302438, class832_draw, 832}, {"nov_teleporter_draw", 0x003094f0, class1135_draw, 1135},
                      {"nov_class1510_draw", 0x0030bfa8, class325_draw, 1510}, {"nov_class1818_glow", 0x0030de68, sandmouse_glow, 1818},
                      {"nov_class1848_draw", 0x0030f0e0, nov_class1848_draw_l1, 1848}};
    for (const Cb& cb : cbs) {
        if (g_oracle_level != 1) {
            l1check136(cb.name, cb.addr, 0, Ret::None, nullptr, nullptr);
            continue;
        }
        std::vector<uint32_t> ms = mobys_of(cb.cls);
        if (ms.empty()) ms = mobys_of(-1);
        l1check136(cb.name, cb.addr, 100, Ret::None, [level, ms, pick, cb] {
            level();
            const uint32_t m = pick(ms);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (coin()) into_view(m + 0x10, coin() ? 0.0f : 8.0f, coin() ? 20.0f : 150.0f);
            if (pv != 0 && coin()) {
                for (uint32_t o = 0; o < 0x40; o += 4) {
                    const uint32_t w = ee_read<uint32_t>(pv + o);
                    const uint32_t e = (w >> 23) & 0xff;
                    if (e > 100 && e < 150 && coin()) wf(pv + o, ee_read<float>(pv + o) * rf(0.5f, 1.5f));  // (a float)
                    else if (w < 0x100 && coin()) w32(pv + o, u(ri(0, 0x40)));                                // (a small counter)
                }
            }
            if (coin()) wfrand(L1_ADDR(0x00162110u), 2, -1.0f, 1.0f);  // (class 1848's scroll)
            if (coin()) w8(m + 0x20, u(ri(0, 4)));
            w32(OV(0x0015f5cc), R());
            seed_rand();
            return Args{{m}, {}};
        }, [cb](A a) { cb.fn(a.i[0]); return uint64_t{0}; });
    }

    // ---- class 1848's draw (stage 4 task P1.1, novalis_reflections.cpp) with its packets compared: the frame's packet cursor
    // (0x1611c0, 0 after an update alone: the null page, which checks leave out) on a buffer of the test area, the skinning
    // ring's start (0x15f5f8) past it as the game lays them out; the camera about the meshes, the scroll varied
    if (g_oracle_level == 1) {
        const std::vector<uint32_t> ms = mobys_of(-1);
        l1check136("nov_class1848_draw_pk", 0x0030f0e0, 60, Ret::None, [level, ms, pick] {
            level();
            const uint32_t m = pick(ms);
            w32(OV(0x001611c0), TEST_AREA + 0x10000 + u(ri(0, 0x40)) * 0x10);
            w32(OV(0x0015f5f8), 0x01f80000u);
            if (coin()) wfrand(L1_ADDR(0x00162110u), 2, -1.0f, 1.0f);
            if (coin()) {  // (the camera at one of the meshes' vertices, off it)
                const uint32_t k = u(ri(0, 4));
                const int32_t n = ee_read<int32_t>(L1_ADDR(0x00202ea0u) + 4 * k);
                const uint32_t pos = ee_read<uint32_t>(L1_ADDR(0x00208108u) + 4 * k);
                if (n > 0) {
                    const uint32_t v = pos + u(ri(0, n - 1)) * 0xc;
                    wf(L1_ADDR(0x00167240u), ee_read<float>(v) + rf(-20, 20));
                    wf(L1_ADDR(0x00167244u), ee_read<float>(v + 4) + rf(-20, 20));
                    wf(L1_ADDR(0x00167248u), ee_read<float>(v + 8) + rf(-5, 10));
                }
            }
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { nov_class1848_draw_l1(a.i[0]); return uint64_t{0}; });
    }

    // ---- the two empty class updates (jr ra): 531 (the ship *(0x13e030)) and 1816 (none placed), on any of the level's mobys
    for (const Cb& cb : {Cb{"nov_empty_update_531", 0x002ece90, empty_update_2ece90_l1, 531}, Cb{"nov_empty_update_1816", 0x0030d200, empty_update_30d200_l1, 1816}}) {
        if (g_oracle_level != 1) {
            l1check136(cb.name, cb.addr, 0, Ret::None, nullptr, nullptr);
            continue;
        }
        const std::vector<uint32_t> ms = mobys_of(-1);
        l1check136(cb.name, cb.addr, 10, Ret::None, [level, ms, pick] {
            level();
            return Args{{pick(ms)}, {}};
        }, [cb](A a) { cb.fn(a.i[0]); return uint64_t{0}; });
    }

    // ---- the gadgets' empty class updates (433 Sonic Summoner, 618 Armor Magnetizer, 1289 O2 Mask), on any of the level's mobys
    {
        struct E {
            const char* name;
            uint32_t addr;
            void (*fn)(uint32_t);
        };
        for (const E& e : {E{"sonic_summoner_update", 0x002cc358, sonic_summoner_update}, E{"armor_magnetizer_update", 0x002d3830, armor_magnetizer_update},
                           E{"o2_mask_update", 0x002e0398, o2_mask_update}}) {
            const std::vector<uint32_t> ms = mobys_of(-1);
            check(e.name, e.addr, 10, Ret::None, [level, ms, pick] {
                level();
                return Args{{pick(ms)}, {}};
            }, [e](A a) { e.fn(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- the gold weapon cases' scene start L1:0x2aea70: the shared port gold_weapon_scene (shared_gold_scene.cpp), checked by
    // batch 510 at Novalis's and Gemlik Base's addresses (989snd's IOP server answering, the stream's wait answered in vsyncs)

    g_extra_noop_hooks.clear();
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
