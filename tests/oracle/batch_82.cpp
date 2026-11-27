// Oracle batch 82: the Pyrocitor (item 0x10, hand moby class 0xb0) on Veldin's real level state
// (needs the ISO): its hand moby's update 0x2b77f0 and helpers, the nozzle glow (class 0xb3),
// the flame particles (type 0xc); the Blaster (item 0x0f, hand class 0xa8): its hand update
// 0x2b49a8, the auto-aim, the bolt (class 0x131) and its glows / sparks.

#include "ovl_oracle.h"

#include <game/ovl/ovl_reloc.h>
namespace rac1::oracle {

void batch_82() {
    if (g_only_batch != 82) return;
    g_batch = 82;
    g_ignored_words = vcross_lanes();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime (null pointers read as RAM, as on the EE)
    static std::vector<uint8_t> snap;
    auto level1 = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty()) {
            veldin_boot();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            gameplay_frame();
            snap.assign(ram, ram + 0x02000000);
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    constexpr uint32_t P = 0x0013f350, PAD = 0x0013c940;
    // a hand moby of the item whose class runs `fn` (its class loaded with the hero's items)
    auto item_moby = [](uint32_t fn) -> uint32_t {
        for (uint32_t i = 1; i < 0x25; ++i) {
            const uint32_t c = ee_read<uint32_t>(OV(0x00179ac0) + i * 0x4c + 0x10);
            if (c >= 0x800) continue;
            const uint32_t ci = ee_read<uint8_t>(OV(0x00197bc0) + c);
            if (ci == 0xff || ee_read<uint32_t>(OV(0x00197680) + ci * 4) != fn) continue;
            return moby_spawn(c);
        }
        return 0;
    };
    auto pick = [](std::initializer_list<uint32_t> l) { return *(l.begin() + ri(0, static_cast<int>(l.size()) - 1)); };
    // the hero / pad / ammo state the fire reads
    auto hero_vary = [pick] {
        w32(PAD + 0x1a0, ri(0, 2) != 0 ? ee_read<uint32_t>(P + 0x10a0) | (R() & 0x9000u) : (R() & 0x9000u));
        w32(PAD + 0x1a4, coin() ? ee_read<uint32_t>(P + 0x10a0) : 0u);
        w8(P + 0x20ac, u(ri(0, 4) == 0));
        w8(P + 0x20b4, u(ri(0, 3)));
        w32(0x001413d4, pick({0u, 1u, 1u, 2u}));
        w8(0x0013e530, u(ri(0, 1)));
        w32(OV(0x0015f5cc), R());
        w32(P + 0x10b0, u(ri(0, 40)));
        wf(P + 0x98, rf(-3.1f, 3.1f));
        // the ammo of the item in hand (0 sometimes: the click without ammo)
        w32(0x00140408, 0x10);
        w32(0x0013d428 + 0x10 * 4, ri(0, 4) == 0 ? 0u : u(ri(1, 240)));
    };
    static uint32_t hand = 0;
    {
        level1();
        hand = item_moby(OVF(0x002b77f0));
        std::printf("  (pyrocitor hand moby %08x)\n", hand);
    }
    check("class_uv_scroll", 0x0024efb0, 100, Ret::None, [level1] {
        level1();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        uint32_t cls = 0;
        for (int k = 0; k < 50 && cls == 0; ++k) cls = ee_read<uint32_t>(first + 0x100 * u(ri(0, 200)) + 0x24);
        if (ri(0, 3) == 0) {
            const uint32_t m = moby_spawn(0xb3);
            if (m != 0) cls = ee_read<uint32_t>(m + 0x24);
        }
        return Args{{cls, u(ri(-0x2000, 0x2000)), u(ri(-0x2000, 0x2000))}, {}};
    }, [](A a) { class_uv_scroll(a.i[0], static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2])); return uint64_t{0}; });
    check("spawn_particle_0c", 0x0026b230, 200, Ret::Int32, [level1] {
        level1();
        wfrand(OUT, 4, 100, 200);
        wfrand(OUT + 0x10, 4, -1, 1);
        w8(0x0013e530, u(ri(0, 1)));
        if (ri(0, 8) == 0) w32(OV(0x00160170), 0x800);  // the pool full
        seed_rand();
        return Args{{OUT, OUT + 0x10, R() & 0x1ff}, {rf(0, 20)}};
    }, [](A a) { return r32(spawn_particle_0c(a.i[0], a.i[1], a.i[2], a.f[0])); });
    check("particle_0c_update", 0x0026b500, 300, Ret::None, [level1] {
        level1();
        Vec4 p = ee_ref<Vec4>(0x0013f3d0); p.x += rf(-4, 4); p.y += rf(-4, 4); p.z += rf(-1, 3);
        if (ri(0, 8) == 0) p.x = rf(-1, 3);
        if (ri(0, 8) == 0) p.z = rf(1019, 1023);
        ee_ref<Vec4>(OUT) = p;
        Vec4 v{rf(-0.5f, 0.5f), rf(-0.5f, 0.5f), rf(-0.2f, 0.3f), 0}; ee_ref<Vec4>(OUT + 0x10) = v;
        w8(0x0013e530, u(ri(0, 1)));
        seed_rand();
        const uint32_t q = spawn_particle_0c(OUT, OUT + 0x10, R() & 7, rf(0, 15));
        if (q == 0) return Args{{0u}, {}};
        w16(q + 0xa, u(ri(0, 3) == 0 ? ri(0, 2) : ri(0, 40)));
        if (coin()) wf(q + 0x30, rf(0, 0.02f));
        w8(q + 0x3d, u(ri(1, 40)));  // (0: a division by zero, the colour lerp's host floats then differ)
        return Args{{q}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_0c_update(a.i[0]); return uint64_t{0}; });
    check("pyrocitor_nozzle_spawn", 0x002bb360, 60, Ret::Int32, [level1] {
        level1();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        return Args{{first + 0x100 * u(ri(1, 200))}, {}};
    }, [](A a) { return r32(pyrocitor_nozzle_spawn(a.i[0])); });
    check("pyrocitor_nozzle_update", 0x002bb400, 300, Ret::None, [level1, pick] {
        level1();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        const uint32_t m = first + 0x100 * u(ri(1, 200));
        const uint32_t o = pyrocitor_nozzle_spawn(m);
        if (o == 0) return Args{{0u}, {}};
        const uint32_t pv = ee_read<uint32_t>(o + 0x78);
        w8(m + 0x20, pick({0u, 1u, 2u, 3u, 0xf0u, 0xfeu}));
        w16(m + 0x34, R() & 0xffff);
        w16(o + 0x34, R() & 0xffff);
        w32(pv + 4, u(ri(-0x200, 0x1200)));
        wf(pv + 0xc, rf(-0.5f, 0.5f));
        wf(o + 0x44, rf(-3.1f, 3.1f));
        w32(P + 0x10b0, u(ri(0, 40)));
        seed_rand();
        return Args{{o}, {}};
    }, [](A a) { if (a.i[0] != 0) pyrocitor_nozzle_update(a.i[0]); return uint64_t{0}; });
    check("pyrocitor_flame_keep", 0x002b8798, 100, Ret::None, [level1] {
        level1();
        if (hand == 0) return Args{{0u, 0u}, {}};
        const uint32_t m = moby_spawn(ee_read<uint16_t>(hand + 0xa6));
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        for (uint32_t i = 0; i < 0xf; ++i) w32(pv + 4 + i * 4, ri(0, 3) == 0 ? 0u : R());
        return Args{{m, R()}, {}};
    }, [](A a) { if (a.i[0] != 0) pyrocitor_flame_keep(a.i[0], a.i[1]); return uint64_t{0}; });
    check("pyrocitor_fire_stats", 0x002b8230, 300, Ret::None, [level1, pick] {
        level1();
        if (hand == 0) return Args{{0u}, {}};
        const uint32_t m = moby_spawn(ee_read<uint16_t>(hand + 0xa6));
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x53, pick({0u, 1u, 2u, 2u}));
        w8(m + 0x20, pick({1u, 2u, 3u, 3u}));
        w32(pv, coin() ? moby_spawn(0xb3) : 0u);
        w16(pv + 0x44, ri(0, 2) == 0 ? 0xffffu : u(ri(0, 7)));
        w16(pv + 0x4a, ri(0, 2) == 0 ? 0xffffu : u(ri(0, 15)));
        w16(pv + 0x50, u(ri(0, 3) == 0 ? 0 : ri(1, 300)));
        w16(0x00141bd8, u(ri(0, 6) == 0));
        w16(0x00141848 + 0x50, ri(0, 6) == 0 ? u(ri(1, 3)) : 0u);
        w16(0x00141848 + 0x52, u(ri(0, 5)));
        w32(0x0015eea4, u(ri(0, 6000)));
        w32(0x0015ed84, u(ri(0, 31)));
        w8(P + 0x20b4, u(ri(0, 3)));
        for (uint32_t i = 0; i < 16; ++i) { w32(0x0013e550 + i * 0x70 + 0x88, coin() ? m : 0u); w8(0x0013e550 + i * 0x70 + 0x74, u(coin())); }
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) pyrocitor_fire_stats(a.i[0]); return uint64_t{0}; });
    // the hit records' byte +0x19.. +0x1b are stack leftovers in the original (copied into the
    // contact ring 0x178100, word +0x28 of each entry)
    const std::vector<uint32_t> ign_saved = g_ignored_words;
    for (uint32_t i = 0; i < 0x100; ++i) g_ignored_words.push_back(OV(0x00178100) + i * 0x40 + 0x28);
    // moby_pose_matrices 0x2531e0 spills the caller's $t8 / $t9 with sq: their upper lanes are stale
    for (uint32_t a = 0x70003708; a < 0x70003710; a += 4) g_ignored_words.push_back(a);
    for (uint32_t a = 0x70003718; a < 0x70003720; a += 4) g_ignored_words.push_back(a);
    {
    // (stage 5, task SPR) pyrocitor_flames_hit calls mobys_in_sphere: the collision scratch words left out
    // (docs/oracle.md)
    const CollisionScratchIgnored spr_ignored;
    check("pyrocitor_flames_hit", 0x002b8438, 300, Ret::None, [level1] {
        level1();
        if (hand == 0) return Args{{0u}, {}};
        const uint32_t m = moby_spawn(ee_read<uint16_t>(hand + 0xa6));
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(0x0013e530, u(ri(0, 1)));
        seed_rand();
        for (uint32_t i = 0; i < 0xf; ++i) {
            if (ri(0, 3) == 0) { w32(pv + 4 + i * 4, 0); continue; }
            Vec4 p = ee_ref<Vec4>(0x0013f3d0);
            if (coin()) p = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(1, 200)) + 0x10);
            p.x += rf(-1.5f, 1.5f); p.y += rf(-1.5f, 1.5f); p.z += rf(-0.5f, 1.5f);
            ee_ref<Vec4>(OUT) = p;
            Vec4 v{rf(-0.5f, 0.5f), rf(-0.5f, 0.5f), rf(-0.2f, 0.3f), 0}; ee_ref<Vec4>(OUT + 0x10) = v;
            const uint32_t q = spawn_particle_0c(OUT, OUT + 0x10, R() & 7, rf(0, 12));
            w32(pv + 4 + i * 4, q);
            if (q == 0) continue;
            w16(q + 0xa, u(ri(0, 3) == 0 ? 10 : ri(0, 30)));
            if (ri(0, 6) == 0) w8(q, 0xb);
            if (ri(0, 6) == 0) w8(q + 1, 0x80);
            wf(q + 0xc, rf(0, 4) * 210000.0f);
        }
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) pyrocitor_flames_hit(a.i[0]); return uint64_t{0}; });
    }
    {
    // (stage 5, task SPR) pyrocitor_update calls segment_collide: the collision scratch words left out (docs/oracle.md)
    const CollisionScratchIgnored spr_ignored;
    check("pyrocitor_update", 0x002b77f0, 400, Ret::None, [level1, hero_vary, pick] {
        level1();
        if (hand == 0) return Args{{0u}, {}};
        hero_vary();
        const uint32_t m = moby_spawn(ee_read<uint16_t>(hand + 0xa6));
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        VV(m + 0x10) = ee_ref<Vec4>(0x0013f3d0);
        for (uint32_t k = 0; k < 0x30; k += 0x10) VV(m + 0xc0 + k) = VV(0x0013f990 + k);
        w8(m + 0x20, pick({0u, 1u, 2u, 2u, 3u, 3u, 3u, 3u, 4u}));
        w8(m + 0x52, pick({0u, 0u, 1u, 2u}));
        w8(m + 0x53, pick({0u, 0u, 1u, 2u}));
        w16(m + 0x34, R() & 0xffff);
        if (coin()) w32(pv, pyrocitor_nozzle_spawn(m)); else w32(pv, 0);
        for (uint32_t i = 0; i < 0xf; ++i) w32(pv + 4 + i * 4, 0);
        w16(pv + 0x44, ri(0, 2) == 0 ? 0xffffu : u(ri(0, 7)));
        if (ee_read<int16_t>(pv + 0x44) >= 0) {
            const uint32_t r = OV(0x001802c0) + u(ee_read<int16_t>(pv + 0x44)) * 0x20;
            wfrand(r, 3, coin() ? 0.0f : 0.0f, coin() ? 0.0005f : 2.0f);
        }
        w16(pv + 0x4a, ri(0, 2) == 0 ? 0xffffu : u(ri(0, 15)));
        w16(pv + 0x4c, u(ri(0, 12)));
        w16(pv + 0x4e, u(ri(0, 3)));
        w16(pv + 0x50, u(ri(0, 200)));
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) pyrocitor_update(a.i[0]); return uint64_t{0}; });
    }
    // --- the Blaster (item 0x0f, hand class 0xa8)
    static uint32_t bhand = 0;
    {
        level1();
        bhand = item_moby(OVF(0x002b49a8));
        std::printf("  (blaster hand moby %08x)\n", bhand);
    }
    // a spot near the hero, a direction
    auto near_hero = [](uint32_t out, float r, float zlo, float zhi) {
        Vec4 p = ee_ref<Vec4>(0x0013f3d0); p.x += rf(-r, r); p.y += rf(-r, r); p.z += rf(zlo, zhi); ee_ref<Vec4>(out) = p;
    };
    auto a_level_moby = [] { return ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(1, 200)); };
    check("spawn_particle_1b", 0x0026dd20, 150, Ret::Int32, [level1, near_hero] {
        level1(); near_hero(OUT, 4, 0, 2); wfrand(OUT + 0x10, 4, -1, 1);
        if (ri(0, 8) == 0) w32(OV(0x00160170), 0x800);
        seed_rand();
        return Args{{OUT, OUT + 0x10, R(), u(ri(1, 40))}, {rf(0, 50000)}};
    }, [](A a) { return r32(spawn_particle_1b(a.i[0], a.i[1], a.i[2], a.i[3], a.f[0])); });
    check("spawn_particle_1a", 0x0026daa0, 150, Ret::Int32, [level1, a_level_moby] {
        level1();
        const bool hero = coin();  // (joints only on the hero: a level moby may have none)
        const uint32_t m = hero ? ee_read<uint32_t>(0x0013f350 + 0x2080) : a_level_moby();
        if (ri(0, 8) == 0) w32(OV(0x00160170), 0x800);
        seed_rand();
        const uint32_t j = !hero || ri(0, 2) == 0 ? 0xffffu : u(ri(0, 3));
        return Args{{m, R(), u(ri(1, 60)), j}, {rf(0, 50000)}};
    }, [](A a) { return r32(spawn_particle_1a(a.i[0], a.i[1], a.i[2], a.i[3], a.f[0])); });
    check("spawn_particle_48", 0x00275158, 150, Ret::Int32, [level1, near_hero] {
        level1(); near_hero(OUT, 4, 0, 2); wfrand(OUT + 0x10, 4, -1, 1);
        if (ri(0, 8) == 0) w32(OV(0x00160170), 0x800);
        seed_rand();
        return Args{{OUT, OUT + 0x10, coin() ? 1u : 0xffffffffu}, {rf(-2, 2)}};
    }, [](A a) { return r32(spawn_particle_48(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), a.f[0])); });
    check("particle_1a_update", 0x0026dbe0, 200, Ret::None, [level1, a_level_moby] {
        level1();
        const bool hero = coin();
        const uint32_t m = hero ? ee_read<uint32_t>(0x0013f350 + 0x2080) : a_level_moby();
        seed_rand();
        const uint32_t p = spawn_particle_1a(m, R(), u(ri(1, 60)), !hero || ri(0, 2) == 0 ? 0xffffu : u(ri(0, 3)), rf(0, 50000));
        if (p == 0) return Args{{0u}, {}};
        w16(p + 0x24, u(ri(0, 60)));
        if (ri(0, 6) == 0) w8(m + 0x20, 0xff);
        if (ri(0, 8) == 0) w32(p + 0x20, 0);
        return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_1a_update(a.i[0]); return uint64_t{0}; });
    check("particle_1b_update", 0x0026de00, 200, Ret::None, [level1, near_hero] {
        level1(); near_hero(OUT, 4, 0, 2); wfrand(OUT + 0x10, 4, -1, 1);
        seed_rand();
        const uint32_t p = spawn_particle_1b(OUT, OUT + 0x10, R(), u(ri(1, 40)), rf(0, 50000));
        if (p == 0) return Args{{0u}, {}};
        w32(p + 0x30, u(ri(0, 40)));
        return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_1b_update(a.i[0]); return uint64_t{0}; });
    check("particle_48_update", 0x002752e0, 200, Ret::None, [level1, near_hero] {
        level1(); near_hero(OUT, 4, 0, 2); wfrand(OUT + 0x10, 4, -1, 1);
        seed_rand();
        const uint32_t p = spawn_particle_48(OUT, OUT + 0x10, coin() ? 1 : -1, 0.0f);
        if (p == 0) return Args{{0u}, {}};
        if (coin()) wf(p + 0xc, rf(-100, 100));
        return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_48_update(a.i[0]); return uint64_t{0}; });
    // a bolt in flight near the hero, varied (no bolt when the class or a particle is missing)
    auto bolt = [near_hero, a_level_moby](uint32_t& o) {
        o = 0;
        near_hero(OUT, 6, 0.3f, 3);
        seed_rand();
        const uint32_t from = ee_read<uint32_t>(0x0013f350 + 0x2080);
        o = moby_spawn(0x131);
        if (o == 0) return;
        const uint32_t pv = ee_read<uint32_t>(o + 0x78);
        w32(pv + 0x20, from);
        VV(o + 0x10) = VV(OUT);
        wf(o + 0x44, rf(-0.5f, 0.5f)); wf(o + 0x48, rf(-3.1f, 3.1f));
        if (coin()) { const uint32_t t = a_level_moby(); VV(pv) = VV(t + 0x10); } else VV(pv) = Vec4{0, 0, 0, 0};
        wf(pv + 0x10, rf(-0.2f, 0.2f)); wf(pv + 0x14, rf(-0.2f, 0.2f));
        wf(pv + 0x18, rf(-3.1f, 3.1f)); wf(pv + 0x1c, rf(-0.5f, 0.5f));
        const int32_t life = frame_scale_i(0x23);
        w16(pv + 0x26, u(life + ri(0, 2) * 10));
        w16(pv + 0x24, u(ri(0, 4) == 0 ? ri(0, 3) : ri(1, life)));
        w32(pv + 0x28, u(ri(0, 3) == 0 ? ri(1, 8) : 0));
        for (uint32_t i = 0; i < 0x11; ++i) {
            const uint32_t p = ri(0, 4) == 0 ? 0u : spawn_particle_48(o + 0x10, OV(0x0015f580), 1, 0.0f);
            w32(pv + 0x2c + i * 4, p);
        }
        w8(0x0013e52f, u(ri(0, 1)));
    };
    check("blaster_bolt_end", 0x002cbcc0, 150, Ret::None, [level1, bolt] {
        level1();
        uint32_t o; bolt(o);
        if (o == 0) return Args{{0u, 0u}, {}};
        return Args{{o, u(coin())}, {}};
    }, [](A a) { if (a.i[0] != 0) blaster_bolt_end(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
    {
    // (stage 5, task SPR) blaster_bolt_update calls segment_collide: the collision scratch words left out
    // (docs/oracle.md)
    const CollisionScratchIgnored spr_ignored;
    check("blaster_bolt_update", 0x002cb418, 400, Ret::None, [level1, bolt] {
        level1();
        uint32_t o; bolt(o);
        if (o == 0) return Args{{0u}, {}};
        if (ri(0, 6) == 0) w32(0x001403e0, bhand);
        return Args{{o}, {}};
    }, [](A a) { if (a.i[0] != 0) blaster_bolt_update(a.i[0]); return uint64_t{0}; });
    }
    // the targets list 0x15ffe4: some level mobys linked in (+0x28 the next), flagged 0x1000
    auto targets = [a_level_moby] {
        uint32_t head = 0;
        std::vector<uint32_t> in;
        for (int k = ri(0, 6); k > 0; --k) {
            const uint32_t t = a_level_moby();
            if (std::find(in.begin(), in.end(), t) != in.end()) continue;  // (no cycles)
            in.push_back(t);
            Vec4 p = ee_ref<Vec4>(0x0013f3d0); p.x += rf(-12, 12); p.y += rf(-12, 12); p.z += rf(0, 3);
            VV(t + 0x10) = p;
            w16(t + 0x34, ee_read<uint16_t>(t + 0x34) | (ri(0, 4) != 0 ? 0x1000u : 0u));
            w8(t + 0x20, ri(0, 6) == 0 ? 0xffu : 1u);
            w32(t + 0x28, head);
            head = t;
        }
        w32(OV(0x0015ffe4), head);
    };
    {
    // (stage 5, task SPR) blaster_auto_aim calls segment_collide: the collision scratch words left out (docs/oracle.md)
    const CollisionScratchIgnored spr_ignored;
    check("blaster_auto_aim", 0x002b46a8, 300, Ret::Int32, [level1, targets, near_hero] {
        level1(); targets();
        near_hero(OUT, 1, 0.5f, 1.5f);
        wf(OUT + 0x10, rf(-3.1f, 3.1f)); wf(OUT + 0x14, rf(-0.4f, 0.4f));
        if (const uint32_t t = ee_read<uint32_t>(OV(0x0015ffe4)); t != 0 && ri(0, 3) != 0) {  // aimed near the first target
            const Vec4 a = VV(t + 0x10), f = VV(OUT);
            wf(OUT + 0x10, atan2_f(a.x - f.x, a.y - f.y) + rf(-0.25f, 0.25f));
            wf(OUT + 0x14, -atan2_f(std::sqrt((a.x - f.x) * (a.x - f.x) + (a.y - f.y) * (a.y - f.y)), a.z - f.z) + rf(-0.2f, 0.2f));
        }
        return Args{{bhand, OUT, OUT + 0x10, OUT + 0x14}, {}};
    }, [](A a) { return r32(blaster_auto_aim(a.i[0], a.i[1], a.i[2], a.i[3])); });
    }
    // (stage 5, task SPR) not marked: after its collision calls the moby's pose capture (moby_anim_set) copies the
    // collision's leftover past the bind pose into a pose record, inside the collision scratch mask (docs/oracle.md)
    check("blaster_update", 0x002b49a8, 500, Ret::None, [level1, hero_vary, pick, targets] {
        level1();
        if (bhand == 0) return Args{{0u}, {}};
        hero_vary();
        targets();
        constexpr uint32_t P = 0x0013f350;
        w32(0x00140408, 0xf);
        w32(0x0013d428 + 0xf * 4, ri(0, 4) == 0 ? 0u : u(ri(1, 200)));
        w32(P + 0x2084, pick({0u, 1u, 0x1eu, 0x1eu, 0x72u}));
        w16(ee_read<uint32_t>(P + 0x2080) + 0x34, ee_read<uint16_t>(ee_read<uint32_t>(P + 0x2080) + 0x34) ^ (coin() ? 1u : 0u));
        w8(P + 0x20a8, u(ri(0, 3) != 0));
        w32(P + 0x1bc, u(ri(0, 20)));
        w32(P + 0x12b0, u(ri(0, 4) == 0));
        wf(P + 0x12bc, rf(-3.1f, 3.1f));
        w16(P + 0x22c8, u(ri(0, 4) == 0));
        wf(P + 0x2e4, rf(-0.3f, 0.3f));
        w32(OV(0x0015f5c4), ri(0, 8) == 0 ? 2u : 1u);
        w8(0x0013e52f, u(ri(0, 1)));
        const uint32_t m = moby_spawn(ee_read<uint16_t>(bhand + 0xa6));
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        VV(m + 0x10) = ee_ref<Vec4>(0x0013f3d0);
        for (uint32_t k = 0; k < 0x30; k += 0x10) VV(m + 0xc0 + k) = VV(0x0013f990 + k);
        w8(m + 0x20, pick({0u, 1u, 1u, 1u}));
        w8(m + 0x51, u(ri(0, 4)));
        w8(m + 0x52, pick({0u, 0u, 1u, 2u}));
        w8(m + 0x53, pick({0u, 1u, 4u, 4u}));
        w8(m + 0x70, R() & 2);
        w32(pv, ri(0, 2) == 0 ? 0u : u(ri(1, 6)));
        w16(pv + 4, u(ri(0, 2)));
        w32(pv + 0x10, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 7)));
        w32(pv + 0x14, u(ri(-1, 15)));
        wf(m + 0x48, rf(-3.1f, 3.1f));
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) blaster_update(a.i[0]); return uint64_t{0}; });
    g_ignored_words = ign_saved;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
