// Oracle batch 113 (task 0341): Novalis on its own overlay by default: its camera type 0x315dd8, class 775
// (0x2ff810), its four sound emitter updates (0x319928 / 0x319f18 / 0x31a078 / 0x31a128, sound_emitter_types.cpp), and
// the shared code it reaches (Veldin's addresses: the particle types 0x06 / 0x08's updates 0x269d20 /
// 0x26a5b8, the Vendor's draw 0x2a5040). Run with ORACLE_LEVEL=1 build/tests/ovl_oracle --batch 113 (needs the
// ISO); level 0 runs the shared checks at Veldin's addresses on Novalis's data (as batch 69). The subjects are the level's own (its
// emitters, its Vendor, the particles live after a few hundred gameplay frames), perturbed.

#include "ovl_oracle.h"

#include <game/ovl/fx_26a5b8.h>
#include <game/ovl/novalis_level_fns.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_fx_tr.h>
#include <game/ovl/vendor_tr.h>
#include <game/ovl/weapons.h>

#include <runtime/kernel/disc.h>

#include <cstring>

namespace rac1::oracle {

namespace {
// A check whose original is Novalis's own function at `l1addr` (not a Veldin address to translate).
void l1check(const char* name, uint32_t l1addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
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
}  // namespace

void batch_113() {
    if (g_only_batch != 113) return;
    g_batch = 113;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime

    // 989snd's IOP server (as batch 103's): replies a function of the command, does the stream reads
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
                if (id == 0x38) {  // the stream system's disc reads (the cutscenes' blocks): done into EE RAM as the IRX does them
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
    // the level after `frames` gameplay frames (the port's), kept as a snapshot
    static std::vector<uint8_t> snap;
    static int snap_level = -1;
    const int frames = std::getenv("B113_FRAMES") != nullptr ? std::atoi(std::getenv("B113_FRAMES")) : 1;
    auto level = [frames] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty() || snap_level != g_oracle_level) {
            veldin_boot();
            level_setup_body(g_oracle_level == 0 ? 1 : g_oracle_level, 0);  // (level 0: Novalis's data on Veldin's image, as batch 69)
            w32(OV(0x0016c060), 0xf);
            for (int f = 0; f < frames; ++f) gameplay_frame();
            snap.assign(ram, ram + 0x02000000);
            snap_level = g_oracle_level;
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    // the live particles of a type
    auto particles_of = [](uint32_t type) {
        std::vector<uint32_t> v;
        const uint32_t base = ee_read<uint32_t>(OV(0x0016016c));
        const int32_t hi = ee_read<int32_t>(OV(0x00160174));
        for (int32_t i = 0; i <= hi && i < 0x800; ++i) {
            const uint32_t p = base + u(i) * 0x40;
            if (ee_read<int8_t>(p + 1) >= 0 && ee_read<uint8_t>(p) == type) v.push_back(p);
        }
        return v;
    };
    // the level's mobys of a class
    auto mobys_of = [](int cls) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<int16_t>(m + 0xa6) == cls) v.push_back(m);
        return v;
    };
    // the level's sound emitters (0x90 bytes at *(0x13f2e4), *(0x13f2e0) of them) whose update is `fn`
    auto emitters_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (int32_t i = 0; i < ee_read<int32_t>(0x0013f2e0); ++i) {
            const uint32_t em = ee_read<uint32_t>(0x0013f2e4) + u(i) * 0x90;
            if (ee_read<uint32_t>(em + 4) == fn) v.push_back(em);
        }
        return v;
    };
    const bool info = std::getenv("B113_INFO") != nullptr;

    // ---- Novalis's own: the camera type 0x315dd8 (`jr ra; move v0,zero`)
    l1check("camera_type_315dd8", 0x00315dd8, 20, Ret::Int32, [level] { level(); return Args{{R(), R()}, {}}; },
            [](A) { return ri32(camera_type_315dd8_l1()); });

    // ---- class 775 (0x2ff810): any live moby as the subject, its alpha byte +0x23 and scale +0x2c varied
    l1check("class775_update", 0x002ff810, 200, Ret::None, [level, mobys_of] {
        level();
        std::vector<uint32_t> ms = mobys_of(775);
        if (ms.empty()) ms = mobys_of(13);
        const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
        w8(m + 0x23, coin() ? u(ri(0, 8)) : R() & 0xff);
        wf(m + 0x2c, rf(0.0f, 4.0f));
        return Args{{m}, {}};
    }, [](A a) { class775_update_l1(a.i[0]); return uint64_t{0}; });

    // ---- the sound emitters: the camera put near / inside the emitter's box, its timers and channel varied
    struct Em {
        const char* name;
        uint32_t fn;
        void (*port)(uint32_t);
    };
    const Em ems[] = {{"emitter_319928", 0x00319928, emitter_fade_box_update}, {"emitter_319f18", 0x00319f18, emitter_reverb_box_update},
                      {"emitter_31a078", 0x0031a078, emitter_camera_bed_update}, {"emitter_31a128", 0x0031a128, emitter_music_box_update}};
    for (const Em& e : ems) {
        if (g_oracle_level != 1) {
            l1check(e.name, e.fn, 0, Ret::None, nullptr, nullptr);
            continue;
        }
        level();
        const std::vector<uint32_t> es = emitters_with(e.fn);
        if (info) std::printf("  %s: %zu emitters\n", e.name, es.size());
        if (es.empty()) {
            std::printf("%-26s L1:0x%08x  no emitter on this level\n", e.name, e.fn);
            continue;
        }
        for (int pass = 0; pass < 3; ++pass)
            l1check(e.name, e.fn, 60, Ret::None, [level, es, pass] {
                level();
                const uint32_t em = es[static_cast<size_t>(ri(0, static_cast<int>(es.size()) - 1))];
                const float r = pass == 0 ? 2.0f : pass == 1 ? ee_read<float>(em + 0xc) + 3.0f : 60.0f;
                for (int k = 0; k < 3; ++k) {
                    const float p = ee_read<float>(em + 0x40 + 4 * u(k)) + rf(-r, r);
                    wf(OV(0x00166dc0) + 4 * u(k), p);
                    wf(0x0013f3d0 + 4 * u(k), p);  // (the hero's position too)
                }
                const uint32_t prm = ee_read<uint32_t>(em + 0x80);
                if (prm >= 0x00100000u && prm < 0x02000000u) {
                    if (coin()) w32(prm + 0xc, u(ri(0, 4)));
                    if (coin()) w32(prm + 0x10, coin() ? 0xffffffffu : u(ri(0, 7)));
                }
                w32(OV(0x0015f5cc), R() & 0xffff);
                seed_rand();
                return Args{{em}, {}};
            }, [port = e.port](A a) { port(a.i[0]); return uint64_t{0}; });
    }

    // ---- shared (Veldin's addresses, the level's equivalent): the particle types 0x06 / 0x08 (type 6: the ones class 27's
    // update spawns on Novalis (its spawner L1 0x27e900, batch 103), a few updates of the level's class-27 mobys)
    {
        auto with_particles = [level, mobys_of] {
            level();
            if (g_oracle_level == 1)  // class 27's spawn call (L1 0x2bd420: a0 its pvars, a2 / a3 / t0 its +0xc0 / +0xc6 / +0xc7, t1 0xff,
                                      // f12..f14 the point, f15 / f16 random, f17..f19 its +0xb0..+0xb8)
                for (const uint32_t m : mobys_of(27)) {
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    for (int k = 0; k < 4; ++k) {
                        for (int c = 0; c < 3; ++c) wf(OUT + 4 * u(c), ee_read<float>(m + 0x10 + 4 * u(c)) + rf(-2.0f, 2.0f));
                        seed_rand();
                        nov_27e900_l1(pv, OUT, ee_read<int32_t>(pv + 0xc0), ee_read<uint8_t>(pv + 0xc6), ee_read<uint8_t>(pv + 0xc7), 0xff,
                                      ee_read<float>(OUT), ee_read<float>(OUT + 4), ee_read<float>(OUT + 8), rf(0.0f, 1.0f), rf(0.0f, 1.0f),
                                      ee_read<float>(pv + 0xb0), ee_read<float>(pv + 0xb4), ee_read<float>(pv + 0xb8), ee_read<float>(pv + 0xbc));
                    }
                }
        };
        with_particles();
        const std::vector<uint32_t> p6 = particles_of(6), p8 = particles_of(8);
        if (info) std::printf("  particles: type 6 %zu, type 8 %zu\n", p6.size(), p8.size());
        if (p6.empty()) skip_check("particle_06_update", 0x00269d20, "no live type-6 particle in the snapshot");
        else
            check("particle_06_update", 0x00269d20, 300, Ret::None, [with_particles, p6] {
                with_particles();
                const uint32_t p = p6[static_cast<size_t>(ri(0, static_cast<int>(p6.size()) - 1))];
                if (coin()) w16(p + 0xa, u(ri(0, 3)));                       // the frames left: near the end
                if (coin()) wfrand(p + 0x20, 3, -0.3f, 0.3f);                 // the velocity
                if (ri(0, 3) == 0) w8(p + 0x3b, R() & 0xff);
                w32(OV(0x0015f5cc), R() & 0xffff);
                seed_rand();
                return Args{{p}, {}};
            }, [](A a) { particle_06_update(a.i[0]); return uint64_t{0}; });
        if (p8.empty()) {  // spawned by the ported spawner (0x26a4c8) when the level has none live
            check("particle_08_update", 0x0026a5b8, 300, Ret::None, [level] {
                level();
                wfrand(OUT, 4, 100, 140);
                wfrand(OUT + 0x10, 4, -1, 1);
                seed_rand();
                spawn_particle_08(OUT, OUT + 0x10, R(), R(), ri(1, 40), rf(0.1f, 3.0f));
                const uint32_t base = ee_read<uint32_t>(OV(0x0016016c));
                const int32_t hi = ee_read<int32_t>(OV(0x00160174));
                uint32_t p = 0;
                for (int32_t i = hi; i >= 0 && p == 0; --i)
                    if (ee_read<uint8_t>(base + u(i) * 64) == 8 && ee_read<int8_t>(base + u(i) * 64 + 1) >= 0) p = base + u(i) * 64;
                if (p != 0 && coin()) w16(p + 0xa, u(ri(0, 3)));
                return Args{{p}, {}};
            }, [](A a) { if (a.i[0] != 0) fx_26a5b8_update(a.i[0]); return uint64_t{0}; });
        } else {
            check("particle_08_update", 0x0026a5b8, 300, Ret::None, [with_particles, p8] {
                with_particles();
                const uint32_t p = p8[static_cast<size_t>(ri(0, static_cast<int>(p8.size()) - 1))];
                if (coin()) w16(p + 0xa, u(ri(0, 3)));
                seed_rand();
                return Args{{p}, {}};
            }, [](A a) { fx_26a5b8_update(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- shared: the particle types 0x38 / 0x39 (class 751's, its spawners L1 0x287b00 / 0x287c80 (batch 103) called as
    // its update calls them: a0 / a1 two vectors, a2 a count, f12 / f13 sizes)
    struct PT {
        const char* name;
        uint32_t vaddr, type;
        uint32_t (*spawn)(uint32_t, uint32_t, float, float);
        void (*port)(uint32_t);
    };
    const PT pts[] = {{"particle_38_update", 0x00272838, 0x38, [](uint32_t p, uint32_t v, float s, float) { return nov_287b00_l1(p, v, s); },
                       particle_38_update},
                      {"particle_39_update", 0x002728e8, 0x39, nov_287c80_l1, particle_39_update}};
    for (const PT& t : pts) {
        if (g_oracle_level != 1) {
            skip_check(t.name, t.vaddr, "its spawner is Novalis's own (ORACLE_LEVEL=1)");
            continue;
        }
        check(t.name, t.vaddr, 300, Ret::None, [level, particles_of, t] {
            level();
            wfrand(OUT, 4, 100, 140);
            wfrand(OUT + 0x10, 4, -1, 1);
            seed_rand();
            (void)ri(1, 8);  // (the count the spawners never read, still drawn: the same cases as before)
            t.spawn(OUT, OUT + 0x10, rf(0.2f, 2.0f), rf(0.0f, 1.0f));
            const std::vector<uint32_t> ps = particles_of(t.type);
            if (ps.empty()) return Args{{0u}, {}};
            const uint32_t p = ps[static_cast<size_t>(ri(0, static_cast<int>(ps.size()) - 1))];
            if (coin()) w16(p + 0xa, u(ri(0, 3)));
            seed_rand();
            return Args{{p}, {}};
        }, [port = t.port](A a) { if (a.i[0] != 0) port(a.i[0]); return uint64_t{0}; });
    }

    // ---- shared: the Vendor's draw callback (on the level's Vendors, class 11), the camera near it
    {
        level();
        const std::vector<uint32_t> vs = mobys_of(11);
        if (vs.empty()) skip_check("vendor_draw", 0x002a5040, "no Vendor on this level");
        else
            for (int pass = 0; pass < 2; ++pass)
                check("vendor_draw", 0x002a5040, 100, Ret::None, [level, vs, pass] {
                    level();
                    const uint32_t m = vs[static_cast<size_t>(ri(0, static_cast<int>(vs.size()) - 1))];
                    if (pass == 1) {  // its state and timers varied
                        w8(m + 0x20, u(ri(0, 6)));
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        if (pv >= 0x00100000u && pv < 0x02000000u)
                            for (uint32_t o = 0; o < 0x40; o += 4)
                                if (ri(0, 3) == 0) w32(pv + o, coin() ? u(ri(0, 60)) : static_cast<uint32_t>(fbits(rf(0.0f, 2.0f))));
                    }
                    for (int k = 0; k < 3; ++k) wf(OV(0x00166dc0) + 4 * u(k), ee_read<float>(m + 0x10 + 4 * u(k)) + rf(-8.0f, 8.0f));
                    wf(OV(0x00161398), rf(0.0f, 1.2f));  // (the draw's pulse phase, $gp - 0x5868)
                    w32(OV(0x0015f5cc), R() & 0xffff);
                    seed_rand();
                    return Args{{m}, {}};
                }, [](A a) { vendor_draw_tr(a.i[0]); return uint64_t{0}; });
        check("vendor_draw_packets", 0x002a5040, 100, Ret::None, [level, vs] {
            level();
            if (ee_read<uint16_t>(0x001518d0) == 0) {
                w16(0x001518d0, 640);
                w16(0x001518d2, 448);
            }
            screen_setup();
            projection_setup();
            render_view_setup();
            w32(OV(0x001611c0), TEST_AREA + 0x1000);
            w32(OV(0x0015f5f8), TEST_AREA + 0x7000);
            uint32_t m;
            if (!vs.empty()) {
                m = vs[static_cast<size_t>(ri(0, static_cast<int>(vs.size()) - 1))];
            } else {
                // Veldin's intro has no placed class-11 Vendor: copy the hero moby's pose/matrix into TEST_AREA.
                m = TEST_AREA + 0x200;
                const uint32_t hero = ee_read<uint32_t>(OV(0x0015ffd8));
                std::memcpy(ee_ram_ptr(m), ee_ram_ptr(hero), 0x100);
                w32(m + 0x78, TEST_AREA + 0x300);
            }
            for (int k = 0; k < 3; ++k) wf(m + 0x10 + 4 * u(k), ee_read<float>(0x0013f3d0 + 4 * u(k)) + rf(-1.2f, 1.2f));
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (pv >= 0x00100000u && pv < 0x02000000u) wf(pv + 0x90, rf(0.4f, 2.0f));
            w32(m + 0x90, (R() & 0xffff0000u) | 0x40000000u);
            wf(OV(0x00161394), coin() ? rf(0.992f, 1.05f) : rf(0.0f, 0.99f));
            wf(OV(0x00161398), coin() ? rf(0.992f, 1.05f) : rf(0.0f, 0.99f));
            if (ri(0, 3) == 0) {
                wf(0x0015ed60, 1.2f);
                wf(0x0015ed68, 1.2f);
                wf(0x0015ed6c, 0.02f);
                wf(0x0015ed70, 0.0004f);
            }
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { vendor_draw_tr(a.i[0]); return uint64_t{0}; });
    }

    // ---- shared (P0.1): direct L0/L1 checks of vec_damp_along (0x25c2b0) and particle types 0x06, 0x38, 0x39
    {
        check("p01_vec_damp_along", 0x0025c2b0, 100, Ret::None, [level] {
            level();
            wfrand(OUT, 4, -5.0f, 5.0f);
            wfrand(OUT + 0x10, 4, -10.0f, 10.0f);
            Vec4 dir{rf(-4.0f, 4.0f), rf(-4.0f, 4.0f), rf(-4.0f, 4.0f), 0.0f};
            if (dir.x * dir.x + dir.y * dir.y + dir.z * dir.z < 0.01f) dir.z = 1.0f;
            ee_ref<Vec4>(OUT + 0x20) = dir;
            return Args{{OUT, OUT + 0x10, OUT + 0x20}, {rf(-2.0f, 2.5f)}};
        }, [](A a) { vec_damp_along(ee_ref<Vec4>(a.i[0]), ee_ref<Vec4>(a.i[1]), ee_ref<Vec4>(a.i[2]), a.f[0]); return uint64_t{0}; });

        {
            const CollisionScratchIgnored spr_ignored;
            check("p01_particle_06", 0x00269d20, 240, Ret::None, [level] {
                level();
                const uint32_t p = alloc_particle(6);
                if (p == 0) return Args{{0u}, {}};
                const uint32_t cfg = TEST_AREA + 0x2000;
                const uint32_t hero = ee_read<uint32_t>(OV(0x0015ffd8));
                const float hx = ee_read<float>(0x0013f3d0), hy = ee_read<float>(0x0013f3d4), hz = ee_read<float>(0x0013f3d8);
                ee_ref<Vec4>(cfg + 0x00) = Vec4{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), 0.0f};
                const float box = ri(0, 6) == 0 ? 0.05f : 50.0f;
                ee_ref<Vec4>(cfg + 0x10) = Vec4{hx - box, hy - box, hz - box, 0.0f};
                ee_ref<Vec4>(cfg + 0x20) = Vec4{hx + box, hy + box, hz + box, 0.0f};
                wf(cfg + 0x30, rf(-600.0f, 600.0f));
                wf(cfg + 0x34, rf(-0.15f, 0.15f));
                wf(cfg + 0x38, rf(0.05f, 0.4f));
                wf(cfg + 0x3c, rf(1.0f, 2.0f));
                wf(cfg + 0x40, rf(0.05f, 2.0f));
                wf(cfg + 0x44, rf(0.1f, 0.3f));
                wf(cfg + 0x48, rf(0.6f, 0.85f));
                for (uint32_t k = 0; k < 3; ++k) {
                    wf(cfg + 0x4c + k * 4, rf(0.05f, 0.4f));
                    wf(cfg + 0x58 + k * 4, rf(0.15f, 0.35f));
                    wf(cfg + 0x64 + k * 4, rf(0.65f, 0.85f));
                }
                uint32_t fl = R() & 0x8fffu;
                if (coin()) fl |= 1u;  // segment_collide bounce
                w32(cfg + 0x70, fl);
                wf(cfg + 0x74, rf(500.0f, 2000.0f));
                wf(cfg + 0x78, rf(3000.0f, 12000.0f));
                w32(cfg + 0x7c, hero);

                ee_ref<Vec4>(p + 0x10) = Vec4{hx + rf(-1.0f, 1.0f), hy + rf(-1.0f, 1.0f), hz + rf(-0.2f, 0.8f), 1.0f};
                if (coin()) {
                    ee_ref<Vec4>(p + 0x20) = Vec4{rf(-0.2f, 0.2f), rf(-0.2f, 0.2f), rf(-2.5f, -0.4f), 0.0f};
                } else {
                    ee_ref<Vec4>(p + 0x20) = Vec4{rf(-0.5f, 0.5f), rf(-0.5f, 0.5f), rf(-0.5f, 0.5f), 0.0f};
                }
                w32(p + 4, R());
                w8(p + 8, R() & 0xff);
                w16(p + 0xa, u(ri(0, 5) == 0 ? ri(0, 2) : ri(3, 60)));
                wf(p + 0xc, rf(100.0f, 15000.0f));
                w32(p + 0x2c, cfg);
                wf(p + 0x30, ri(0, 6) == 0 ? rf(-0.05f, 0.0f) : rf(0.0f, 1.05f));
                wf(p + 0x34, rf(0.0f, 1.0f));
                w32(p + 0x38, R());
                if (ri(0, 3) == 0) {
                    wf(0x0015ed60, 1.2f);
                    wf(0x0015ed68, 1.2f);
                    wf(0x0015ed6c, 0.02f);
                }
                seed_rand();
                return Args{{p}, {}};
            }, [](A a) { if (a.i[0] != 0) particle_06_update(a.i[0]); return uint64_t{0}; }, OV(0x00173e8c), OV(0x00173e9c));
        }

        check("p01_particle_38", 0x00272838, 150, Ret::None, [level] {
            level();
            const uint32_t p = alloc_particle(0x38);
            if (p == 0) return Args{{0u}, {}};
            wfrand(p + 0x10, 4, 100.0f, 140.0f);
            wfrand(p + 0x20, 4, -1.0f, 1.0f);
            wf(p + 0xc, rf(0.0f, 30000.0f));
            w32(p + 4, (R() & 0xffffff00u) | u(ri(0, 3) == 0 ? ri(0, 4) : ri(5, 255)));
            return Args{{p}, {}};
        }, [](A a) { if (a.i[0] != 0) particle_38_update(a.i[0]); return uint64_t{0}; });

        check("p01_particle_39", 0x002728e8, 150, Ret::None, [level] {
            level();
            const uint32_t p = alloc_particle(0x39);
            if (p == 0) return Args{{0u}, {}};
            wfrand(p + 0x10, 4, 100.0f, 140.0f);
            wfrand(p + 0x20, 4, -1.0f, 1.0f);
            wf(p + 0xc, rf(0.0f, 30000.0f));
            wf(p + 0x30, rf(-50.0f, 50.0f));
            wf(p + 0x34, rf(-5.0f, 5.0f));
            const int k = ri(0, 2);
            w16(p + 0xa, u(k == 0 ? ri(0, 2) : k == 1 ? ri(3, 65) : ri(66, 127)));
            return Args{{p}, {}};
        }, [](A a) { if (a.i[0] != 0) particle_39_update(a.i[0]); return uint64_t{0}; });
    }

    // ---- the vendor's screens (game mode 5, Veldin's addresses): the start 0x29a6b0 from a Vendor (or 0: the hero's
    // item 0x20), then the mode's update 0x29c648 / draw 0x2a02b0 after k port updates with random buttons (+0x1a0 / +0x1a4
    // of the pad block 0x13c940). The DMA kick, FlushCache and the vsync wait are stubbed on both sides (the fade 0x1f9838,
    // a loop of frames, runs on both: task 0351).
    {
        g_extra_noop_hooks = {OVF(0x002a08c8), 0x00122298, 0x00118a80};
        g_ignored_words.push_back(OV(0x001e3d5c));  // (a VCROSS result's stale w lane: the vector 0x1e3d50 of the mode's camera rows)
        auto pad = [] {
            const uint32_t b = coin() ? 0u : (R() & 0xf0f0u);
            w32(0x0013c940 + 0x1a4, b);
            w32(0x0013c940 + 0x1a0, b | (coin() ? 0u : (R() & 0xf0f0u)));
        };
        auto vendor_or_0 = [mobys_of] {
            const std::vector<uint32_t> vs = mobys_of(11);
            return vs.empty() || coin() ? 0u : vs[static_cast<size_t>(ri(0, static_cast<int>(vs.size()) - 1))];
        };
        check("vendor_screen_start", 0x0029a6b0, 40, Ret::None, [level, vendor_or_0] {
            level();
            const uint32_t v = vendor_or_0();
            seed_rand();
            return Args{{v}, {}};
        }, [](A a) { vendor_screen_start_tr(a.i[0]); return uint64_t{0}; });
        auto in_mode5 = [level, vendor_or_0, pad, mobys_of](int maxk) {
            level();
            for (const int cls : {730, 731, 790})  // (the Agnogg ship starts a scene: its stream read needs the IOP, not modelled here)
                for (const uint32_t m : mobys_of(cls)) w32(m + 0x74, 0);
            vendor_screen_start_tr(vendor_or_0());
            const int k = ri(0, maxk);
            for (int i = 0; i < k; ++i) {
                pad();
                vendor_mode_update_tr(0);
                vendor_mode_draw_tr(0);
            }
            if (coin()) {  // a state the headless start does not reach by itself (state 1 waits on the Vendor's animation): put
                // directly, k more frames run in it
                w32(OV(0x001ca4c0), u(ri(1, 3)));
                const int k2 = ri(0, 20);
                for (int i = 0; i < k2; ++i) {
                    pad();
                    vendor_mode_update_tr(0);
                    vendor_mode_draw_tr(0);
                }
            }
            pad();
            w32(OV(0x0015f5cc), R() & 0xffff);
            seed_rand();
            if (std::getenv("B113_INFO") != nullptr)
                std::printf("  mode 5 after %d frames: state %u, +0x14 %08x, +0x20 %08x, game mode %d\n", k, ee_read<uint32_t>(OV(0x001ca4c0)),
                            ee_read<uint32_t>(OV(0x001ca4c0) + 0x14), ee_read<uint32_t>(OV(0x001ca4c0) + 0x20), ee_read<int32_t>(OV(0x0015f5c4)));
        };
        {
        // (stage 5, task C2c) vendor_mode_update reaches segment_collide: A2's collision scratch
        // ranges left out of this check (see batch 43's checks of it)
        const CollisionScratchIgnored spr_ignored;
        check("vendor_mode_update", 0x0029c648, 120, Ret::None, [in_mode5] { in_mode5(240); return Args{{0u}, {}}; },
              [](A) { vendor_mode_update_tr(0); return uint64_t{0}; }, OV(0x00173e9c));
        }
        check("vendor_mode_draw", 0x002a02b0, 120, Ret::None, [in_mode5] { in_mode5(240); return Args{{0u}, {}}; },
              [](A) { vendor_mode_draw_tr(0); return uint64_t{0}; });
        g_extra_noop_hooks.clear();
    }

    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
