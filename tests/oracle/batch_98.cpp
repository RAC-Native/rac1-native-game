// Oracle batch 98 (task 0321): the weapons and gadgets of R&C1 on Veldin's real level state (needs
// the ISO): the Mine Glove (item 0x11, hand class 0xbe, mine class 0x4a) and the ports that follow
// in the same file as they are added.

#include "ovl_oracle.h"

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapons.h>
namespace rac1::oracle {

void batch_98() {
    if (g_only_batch != 98) return;
    g_batch = 98;
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
        render_view_setup();  // the camera's matrices in VU0 (point_on_screen)
        collide_vu_reset();  // the contact registers vf25..vf29 the collision leaves, the same on both sides
        oracle_vu_contact_reset();
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
    auto pickf = [](std::initializer_list<float> l) { return *(l.begin() + ri(0, static_cast<int>(l.size()) - 1)); };
    auto pick = [](std::initializer_list<uint32_t> l) { return *(l.begin() + ri(0, static_cast<int>(l.size()) - 1)); };
    // the hero / pad / ammo state a hand update reads, for the item `item`
    auto hero_vary = [pick](uint32_t item) {
        if (ee_read<uint32_t>(P + 0x10a0) == 0) w32(P + 0x10a0, 0x20u);  // the fire button's mask (circle)
        w32(PAD + 0x1a0, ri(0, 2) != 0 ? ee_read<uint32_t>(P + 0x10a0) | (R() & 0x9005u) : (R() & 0x9005u));
        w32(PAD + 0x1a4, coin() ? ee_read<uint32_t>(P + 0x10a0) : 0u);
        w32(0x0013cae4, coin() ? ee_read<uint32_t>(P + 0x10a0) : 0u);
        w8(P + 0x20ac, u(ri(0, 4) == 0));
        w8(P + 0x20a8, u(ri(0, 3) != 0));
        w32(P + 0x2084, pick({0u, 1u, 0x1eu, 0x1eu, 0x23u, 0x23u}));
        w32(P + 0x198, u(ri(0, 20)));
        w32(P + 0x1bc, u(ri(0, 20)));
        w32(0x001413d4, pick({0u, 1u, 1u, 2u}));
        w8(0x0013e530, u(ri(0, 1)));
        w32(OV(0x0015f5cc), R());
        w32(P + 0x10b0, u(ri(0, 40)));
        wf(P + 0x98, rf(-3.1f, 3.1f));
        w32(0x00140408, item);
        w32(0x0013d428 + item * 4, ri(0, 4) == 0 ? 0u : u(ri(1, 240)));
    };
    auto near_hero = [](uint32_t out, float r, float zlo, float zhi) {
        Vec4 p = ee_ref<Vec4>(0x0013f3d0); p.x += rf(-r, r); p.y += rf(-r, r); p.z += rf(zlo, zhi); ee_ref<Vec4>(out) = p;
    };
    auto a_level_moby = [] { return ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(1, 200)); };
    // the hit records' byte +0x19.. +0x1b are stack leftovers in the original (copied into the
    // contact ring 0x178100, word +0x28 of each entry); moby_pose_matrices 0x2531e0 spills the
    // caller's $t8 / $t9 with sq: their upper lanes are stale
    const std::vector<uint32_t> ign_saved = g_ignored_words;
    for (uint32_t i = 0; i < 0x100; ++i) g_ignored_words.push_back(OV(0x00178100) + i * 0x40 + 0x28);
    for (uint32_t a = 0x70003708; a < 0x70003710; a += 4) g_ignored_words.push_back(a);
    for (uint32_t a = 0x70003718; a < 0x70003720; a += 4) g_ignored_words.push_back(a);

    // --- the Mine Glove (item 0x11, hand class 0xbe)
    static uint32_t mhand = 0;
    {
        level1();
        mhand = item_moby(OVF(0x002c1ad0));
        std::printf("  (mine glove hand moby %08x, mine class 0x4a %s)\n", mhand, moby_spawn(0x4a) != 0 ? "present" : "absent");
    }
    check("mine_throw_velocity", 0x002c1928, 300, Ret::None, [level1, near_hero] {
        level1();
        near_hero(OUT, 0.5f, 0.5f, 1.5f);
        near_hero(OUT + 0x10, ri(0, 3) == 0 ? 3.0f : 14.0f, ri(0, 3) == 0 ? -1.0f : -3.0f, 4.0f);
        wfrand(OUT + 0x20, 4, 0, 0);
        const float g = rf(0.0f, 2.0f) * 9.8f;
        return Args{{OUT, OUT + 0x10, OUT + 0x20}, {g, rf(0.1f, 3.0f), rf(2.0f, 14.0f)}};
    }, [](A a) { mine_throw_velocity(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
    check("mine_spawn", 0x002a9ed0, 100, Ret::Int32, [level1, near_hero] {
        level1();
        near_hero(OUT, 3, 0, 2);
        if (ri(0, 8) == 0) w32(OV(0x00160170), 0x800);
        seed_rand();
        const uint32_t h = mhand != 0 ? mhand : ee_read<uint32_t>(P + 0x2080);
        return Args{{h, OUT, OUT + 0x20}, {}};
    }, [](A a) { return r32(mine_spawn(a.i[0], a.i[1])); });
    {
    // (stage 5, task SPR) mine_throw_start calls segment_collide: the collision scratch words left out (docs/oracle.md)
    const CollisionScratchIgnored spr_ignored;
    check("mine_throw_start", 0x002aa008, 300, Ret::None, [level1, near_hero, pick, a_level_moby] {
        level1();
        near_hero(OUT, ri(0, 3) == 0 ? 6.0f : 1.0f, 0.2f, 2.0f);
        seed_rand();
        const uint32_t mine = mine_spawn(ee_read<uint32_t>(P + 0x2080), OUT);
        if (mine == 0) return Args{{0u}, {}};
        const uint32_t pv = ee_read<uint32_t>(mine + 0x78);
        wfrand(pv, 3, -8, 8);
        // some other mines of the same class alive (with their timestamps)
        uint32_t head = 0;  // (the moby list 0x15ffe4, linked by +0x28)
        for (int k = ri(0, 1) == 0 ? ri(6, 10) : ri(0, 5); k > 0; --k) {
            const uint32_t o = mine_spawn(ee_read<uint32_t>(P + 0x2080), OUT);
            if (o == 0) continue;
            w32(ee_read<uint32_t>(o + 0x78) + 0x5c, R());
            if (ri(0, 7) == 0) w8(o + 0x20, pick({0xfeu, 0xfdu}));
            w32(o + 0x28, head);
            head = o;
        }
        if (ri(0, 2) == 0) { const uint32_t o = a_level_moby(); w32(o + 0x28, head); head = o; }
        w32(OV(0x0015ffe4), head);
        if (ri(0, 3) == 0) w32(P + 0x2fc, ee_read<uint32_t>(P + 0x2080));
        near_hero(OUT + 0x40, ri(0, 3) == 0 ? 8.0f : 1.0f, 0.2f, 2.0f);
        return Args{{OUT + 0x40, mine, pv}, {}};
    }, [](A a) { if (a.i[1] != 0) mine_throw_start(VV(a.i[0]), a.i[1], a.i[2]); return uint64_t{0}; });
    }
    {
    // (stage 5, task SPR) mine_glove_update calls segment_collide: the collision scratch words left out
    // (docs/oracle.md)
    const CollisionScratchIgnored spr_ignored;
    check("mine_glove_update", 0x002c1ad0, 500, Ret::None, [level1, hero_vary, pick, near_hero] {
        level1();
        if (mhand == 0) return Args{{0u}, {}};
        hero_vary(0x11);
        w8(0x0013e531, u(ri(0, 1)));
        if (ri(0, 3) != 0) {  // the camera: a random frame (0x166c80 rows) and place (0x166dc0) near the hero
            wfrand(OV(0x00166c80), 3, -1.0f, 1.0f); wfrand(OV(0x00166c90), 3, -1.0f, 1.0f); wfrand(OV(0x00166ca0), 3, -1.0f, 1.0f);
            near_hero(OV(0x00166dc0), 8.0f, -1.0f, 4.0f);
        }
        const uint32_t m = moby_spawn(ee_read<uint16_t>(mhand + 0xa6));
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        VV(m + 0x10) = ee_ref<Vec4>(0x0013f3d0);
        for (uint32_t k = 0; k < 0x30; k += 0x10) VV(m + 0xc0 + k) = VV(0x0013f990 + k);
        w8(m + 0x20, pick({0u, 1u, 2u, 3u, 3u, 3u, 4u, 5u, 6u}));
        w8(m + 0x70, R() & 2);
        w16(m + 0x34, R() & 0xffff);
        w32(pv + 0x5c, ri(0, 6) == 0 ? u(ri(0, 2)) : 3u);
        w32(pv + 0x54, ri(0, 2) == 0 ? 0u : u(ri(0, 20)));
        w32(pv + 0x58, ri(0, 2) == 0 ? 0u : u(ri(0, 20)));
        seed_rand();
        if (ri(0, 3) != 0) {
            near_hero(OUT, 0.5f, 0.5f, 1.5f);
            w32(pv + 0x50, mine_spawn(m, OUT));
        } else {
            w32(pv + 0x50, 0);
        }
        w32(0x00141708, R());
        w32(0x0014170c, R());
        w32(0x0015eea4, u(ri(0, 6000)));
        w32(0x0015ed84, u(ri(0, 31)));
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) mine_glove_update(a.i[0]); return uint64_t{0}; });
    }
    // --- the Trespasser (item 0x1a, hand / pickup class 0xbc)
    static uint32_t thand = 0;
    {
        level1();
        thand = item_moby(OVF(0x002c1718));
        std::printf("  (trespasser moby %08x)\n", thand);
    }
    {
    // (stage 5, task SPR) trespasser_pickup_update reaches segment_collide (through floor_probe_v): the
    // collision scratch words left out (docs/oracle.md)
    const CollisionScratchIgnored spr_ignored;
    check("trespasser_pickup_update", 0x002c1778, 400, Ret::None, [level1, pick, near_hero] {
        level1();
        if (thand == 0) return Args{{0u}, {}};
        const uint32_t m = moby_spawn(ee_read<uint16_t>(thand + 0xa6));
        w8(m + 0x20, pick({0u, 1u, 1u, 1u, 2u, 2u, 3u}));
        near_hero(m + 0x10, ri(0, 3) == 0 ? 3.0f : 0.5f, -1.0f, 3.0f);
        wf(m + 0x2c, ri(0, 4) == 0 ? 0.0f : rf(0.0f, 1.0f));
        w32(OV(0x0015f5cc), ri(0, 2) == 0 ? u(10 * ri(0, 500)) : R());
        w32(P + 0x22a8, u(ri(0, 4) != 0));
        wf(m + 0x48, rf(-3.1f, 3.1f));
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) trespasser_pickup_update(a.i[0]); return uint64_t{0}; });
    }
    {
    // (stage 5, task SPR) trespasser_update reaches segment_collide (through trespasser_pickup_update,
    // floor_probe_v): the collision scratch words left out (docs/oracle.md)
    const CollisionScratchIgnored spr_ignored;
    check("trespasser_update", 0x002c1718, 200, Ret::None, [level1, pick, near_hero] {
        level1();
        if (thand == 0) return Args{{0u}, {}};
        const uint32_t m = moby_spawn(ee_read<uint16_t>(thand + 0xa6));
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w32(pv + 4, ri(0, 2) == 0 ? R() | 1u : 0u);
        w8(m + 0x20, pick({0u, 1u, 2u}));
        near_hero(m + 0x10, 0.5f, -1.0f, 3.0f);
        w32(P + 0x22a8, u(ri(0, 4) != 0));
        w32(OV(0x0015f5cc), 10u * u(ri(0, 500)));
        w32(0x0013cae0, R()); w32(0x001403f0, R());
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) trespasser_update(a.i[0]); return uint64_t{0}; });
    }
    // --- the shared effect helpers
    check("angle_seek_limited", 0x00200510, 300, Ret::Float, [] {
        return Args{{}, {rf(-3.1f, 3.1f), rf(-3.1f, 3.1f), ri(0, 3) == 0 ? rf(0, 0.05f) : rf(0, 4.0f)}};
    }, [](A a) { return rf32(angle_seek_limited(a.f[0], a.f[1], a.f[2])); });
    check("moby_list_remove", 0x0025f800, 200, Ret::None, [level1, a_level_moby] {
        level1();
        const int n = ri(0, 8);
        w16(T, u(n));
        uint32_t want = 0;
        for (int i = 0; i < 10; ++i) {
            const uint32_t t = a_level_moby();
            w16(T + 2 + i * 2, (t - ee_read<uint32_t>(OV(0x0015ffd8))) >> 8);
            if (i == ri(0, 9)) want = t;
        }
        if (want == 0) want = a_level_moby();
        return Args{{want, T}, {}};
    }, [](A a) { moby_list_remove(a.i[0], a.i[1]); return uint64_t{0}; });
    check("hit_rec_init", 0x00259858, 50, Ret::None, [] {
        wrand(OUT, 12);
        return Args{{OUT, R(), R()}, {rf(-5, 5)}};
    }, [](A a) { hit_rec_init(a.i[0], a.i[1], a.i[2], a.f[0]); return uint64_t{0}; });
    check("spawn_particle_08", 0x0026a4c8, 200, Ret::None, [level1, near_hero] {
        level1(); near_hero(OUT, 4, 0, 2); wfrand(OUT + 0x10, 4, -1, 1);
        if (ri(0, 8) == 0) w32(OV(0x00160170), 0x800);
        seed_rand();
        return Args{{OUT, OUT + 0x10, R(), R(), ri(0, 5) == 0 ? 0u : u(ri(1, 100))}, {rf(0, 300000)}};
    }, [](A a) { spawn_particle_08(a.i[0], a.i[1], a.i[2], a.i[3], static_cast<int32_t>(a.i[4]), a.f[0]); return uint64_t{0}; });
    {
    // (stage 5, task SPR) floor_light_add reaches segment_collide (through floor_probe_v): the collision scratch
    // words left out (docs/oracle.md)
    const CollisionScratchIgnored spr_ignored;
    check("floor_light_add", 0x00259fe8, 200, Ret::None, [level1, near_hero] {
        level1();
        const uint32_t m = moby_spawn(0x4a);
        if (m == 0) return Args{{0u}, {}};
        w8(m + 0x31, u(ri(0, 4) != 0));
        near_hero(m + 0x10, 3, -1, 4);
        w32(OV(0x0015f434), ri(0, 8) == 0 ? 0x20u : u(ri(0, 0x1f)));
        return Args{{m}, {rf(0, 2)}};
    }, [](A a) { if (a.i[0] != 0) floor_light_add(a.i[0], a.f[0]); return uint64_t{0}; });
    }
    {
    // (stage 5, task SPR) explosion_effect reaches mobys_in_sphere (through explosion_body): the collision
    // scratch words left out (docs/oracle.md)
    const CollisionScratchIgnored spr_ignored;
    check("explosion_effect", 0x0025e450, 400, Ret::None, [level1, near_hero, a_level_moby, pick, pickf] {
        level1();
        const uint32_t src = ri(0, 6) == 0 ? 0u : a_level_moby();
        near_hero(OUT, ri(0, 3) == 0 ? 30.0f : 3.0f, 0.0f, 3.0f);
        if (src != 0) w8(src + 0xa6, 1);
        seed_rand();
        w32(OV(0x0015f5d0), u(ri(0, 1) == 0 ? 0x3e800000 : 0x3f7a0000));
        w32(OV(0x0015f5d4), u(ri(0, 1) == 0 ? 0x3e800000 : 0x3f7a0000));
        if (ri(0, 8) == 0) w32(OV(0x00160170), 0x800);
        const uint32_t pos = ri(0, 3) == 0 ? 0u : OUT;
        const uint32_t a9 = u(ri(0, 4)), a10 = ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 2)), a11 = u(ri(0, 1));
        Args r{{src, OUT + 0x20, pos, u(ri(0, 4)), u(ri(0, 6)), u(ri(0, 4)), pick({0xffffffffu, 2u, 5u}), u(ri(0, 1)), a9, a10, a11},
                    {pickf({0.0f, 0.5f, 2.0f}), rf(0, 1), pickf({0.0f, 1.0f, 2.0f}), pickf({0.0f, 1.0f}), rf(1, 5), rf(0.5f, 1.5f), pickf({0.0f, 3.0f, 7.0f})}};
        if (const char* xm = std::getenv("XMASK")) { const int x = std::atoi(xm); if (x & 1) r.i[3] = 0; if (x & 2) r.i[4] = 0; if (x & 4) r.i[5] = 0; if (x & 8) r.i[7] = 0; if (x & 16) r.i[8] = 0; if (x & 32) r.i[0] = 0; if (x & 64) r.f[0] = 0; if (x & 128) { r.f[2] = 0; r.f[3] = 0; } if (x & 256) r.f[6] = 0; }
        return r;
    }, [](A a) {
        explosion_effect(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]), static_cast<int32_t>(a.i[4]), static_cast<int32_t>(a.i[5]), static_cast<int32_t>(a.i[6]),
                         static_cast<int32_t>(a.i[7]), static_cast<int32_t>(a.i[8]), static_cast<int32_t>(a.i[9]), a.i[10], a.f[0], a.f[1], a.f[2], a.f[3], a.f[4], a.f[5], a.f[6]);
        return uint64_t{0};
    });
    }
    // --- the mine (class 0x4a)
    auto make_mine = [near_hero, pick, a_level_moby](uint32_t state) -> uint32_t {
        const uint32_t m = moby_spawn(0x4a);
        if (m == 0) return 0;
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        near_hero(m + 0x10, 3.0f, 0.2f, 3.0f);
        if (ri(0, 3) == 0) { Vec4 p = ee_ref<Vec4>(0x0013f3d0); p.x += rf(-0.9f, 0.9f); p.y += rf(-0.9f, 0.9f); p.z = ee_read<float>(P + 0x88) + rf(-0.35f, 0.35f); ee_ref<Vec4>(m + 0x10) = p; }
        w8(m + 0x20, state);
        w8(m + 0x70, R() & 2);
        w16(m + 0x34, R() & 0xffff);
        wfrand(pv, 3, -0.3f, 0.3f);
        wf(pv + 8, rf(-0.6f, 0.4f));
        const uint32_t hm = mhand != 0 ? moby_spawn(ee_read<uint16_t>(mhand + 0xa6)) : 0u;
        w32(pv + 0x30, ri(0, 6) == 0 ? 0u : hm);
        w16(pv + 0x34, u(ri(0, 4) == 0 ? 0 : ri(1, 120)));
        w16(pv + 0x36, u(ri(0, 5) == 0));
        wfrand(pv + 0x40, 3, -3.1f, 3.1f);
        w32(pv + 0x4c, ri(0, 3) == 0 ? a_level_moby() : 0u);
        w16(pv + 0x50, u(ri(0, 8) == 0));
        w16(pv + 0x52, u(ri(0, 2)));
        w32(pv + 0x54, ri(0, 3) == 0 ? a_level_moby() : 0u);
        w32(pv + 0x58, u(ri(0, 3) == 0 ? ri(0, 3) : ri(0, 600)));
        w32(pv + 0x5c, ri(0, 8) == 0 ? 0xffffffffu : R() >> 1);
        w32(pv + 0x68, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 15)));
        wf(pv + 0x6c, ri(0, 3) == 0 ? 0.0f : rf(0, 3.0f));
        wf(pv + 0x70, rf(-3.1f, 3.1f));
        w32(pv + 0x74, u(ri(-5, 50)));
        w32(pv + 0x78, u(ri(0, 4) == 0));
        w8(m + 0xbc, u(ri(0, 4) == 0));
        wf(m + 0x48, rf(-3.1f, 3.1f));
        return m;
    };
    auto mine_world = [level1, hero_vary, near_hero, a_level_moby, pick] {
        level1();
        if (mhand == 0) return;
        hero_vary(0x11);
        w8(0x0013e531, u(ri(0, 1)));
        w32(P + 0x1090, ri(0, 6) == 0 ? 0u : mhand);
        w8(P + 0x20a4, u(ri(0, 8) == 0)); w8(P + 0x20a5, u(ri(0, 3) == 0)); w8(P + 0x20af, u(ri(0, 3) == 0));
        w32(P + 0x2fc, ri(0, 4) == 0 ? ee_read<uint32_t>(P + 0x2080) : 0u);
        w32(0x001613d0, u(ri(0, 12) == 0));
        w32(OV(0x0015f5cc), u(ri(40, 400)));
        // the target list 0x1aba00 (zero-terminated), mobys near the hero
        const int n = ri(0, 4);
        for (int i = 0; i < n; ++i) {
            const uint32_t t = a_level_moby();
            near_hero(t + 0x10, ri(0, 2) == 0 ? 35.0f : 5.0f, -1.0f, 3.0f);
            w32(OV(0x001aba00) + i * 4, t);
        }
        w32(OV(0x001aba00) + n * 4, 0);
        if (ri(0, 2) == 0) { wfrand(OV(0x00166c80), 3, -1.0f, 1.0f); near_hero(OV(0x00166dc0), 8.0f, -1.0f, 4.0f); }
    };
    {
    // (stage 5, task SPR) mine_predict calls segment_collide: the collision scratch words left out (docs/oracle.md)
    const CollisionScratchIgnored spr_ignored;
    check("mine_predict", 0x002aa2a8, 300, Ret::Int32, [mine_world, make_mine, pick] {
        mine_world();
        const uint32_t m = make_mine(pick({0u, 0u, 1u, 1u, 2u, 6u}));
        if (m == 0) return Args{{0u, 0u, 0u}, {}};
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w32(OV(0x0015f5c4), u(ri(0, 9) == 0));
        return Args{{pv, m, u(ri(0, 2))}, {}};
    }, [](A a) { if (a.i[1] == 0) return r32(0); return r32(static_cast<uint32_t>(mine_predict(a.i[0], a.i[1], static_cast<int32_t>(a.i[2])))); });
    }
    {
    // (stage 5, task SPR) mine_update calls segment_collide: the collision scratch words left out (docs/oracle.md)
    const CollisionScratchIgnored spr_ignored;
    check("mine_update", 0x002aa670, 2500, Ret::None, [mine_world, make_mine, pick] {
        mine_world();
        const uint32_t m = make_mine(pick({0u, 1u, 1u, 1u, 1u, 2u, 6u, 6u, 6u, 0x16u, 0x14u, 5u, 0x12u}));
        if (m == 0) return Args{{0u}, {}};
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) mine_update(a.i[0]); return uint64_t{0}; });
    }
    // --- the Taunter (item 0x0e, hand class 0xaf)
    static uint32_t thand2 = 0;
    {
        level1();
        thand2 = item_moby(OVF(0x002b6f10));
        std::printf("  (taunter hand moby %08x)\n", thand2);
    }
    // enemies for the taunt: level mobys of the classes it takes, with a pvars word (+0x18 the taunter)
    auto taunt_world = [level1, hero_vary, near_hero, a_level_moby, pick] {
        level1();
        hero_vary(0xe);
        wf(P + 0x98, rf(-3.1f, 3.1f));
        const int n = ri(0, 5);
        for (int i = 0; i < n; ++i) {
            const uint32_t t = a_level_moby();
            near_hero(t + 0x10, ri(0, 2) == 0 ? 20.0f : 8.0f, -1.0f, 3.0f);
            w16(t + 0xa6, pick({0x1f4u, 0x1f5u, 0x1f9u, 0x1ffu, 0x100u}));
            w16(t + 0x34, ee_read<uint16_t>(t + 0x34) | (ri(0, 6) == 0 ? 0u : 0x20u));
            const uint32_t pvv = ee_read<uint32_t>(t + 0x78);
            const uint32_t st = T + 0x40 * u(i);
            w32(pvv, st);
            wrand(st, 8);
            w32(st + 0x18, ri(0, 2) == 0 ? 0u : a_level_moby());
            w32(OV(0x001aba00) + i * 4, t);
        }
        w32(OV(0x001aba00) + n * 4, 0);
        // the mines list 0x1b07b0
        const int k = ri(0, 4);
        w16(OV(0x001b07b0), u(k));
        for (int i = 0; i < k; ++i) {
            const uint32_t mm = moby_spawn(0x4a);
            if (mm == 0) continue;
            near_hero(mm + 0x10, 8.0f, -1.0f, 3.0f);
            w8(mm + 0x20, ri(0, 6) == 0 ? 0xfeu : 1u);
            w16(OV(0x001b07b0) + 2 + i * 2, (mm - ee_read<uint32_t>(OV(0x0015ffd8))) >> 8);
        }
    };
    check("taunter_hit", 0x002b6bc8, 300, Ret::Int32, [taunt_world, near_hero, thand2_ = &thand2] {
        taunt_world();
        const uint32_t m = moby_spawn(0xaf);
        if (m == 0) return Args{{0u}, {0.0f}};
        near_hero(OUT, 0.5f, 0.5f, 1.5f);
        VV(m + 0x10) = VV(OUT);
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w32(pv + 0x14, u(ri(0, 4)));
        seed_rand();
        (void)thand2_;
        return Args{{m}, {rf(2.0f, 30.0f)}};
    }, [](A a) { if (a.i[0] == 0) return r32(0); return r32(static_cast<uint32_t>(taunter_hit(a.i[0], a.f[0]))); });
    check("taunter_rings", 0x002b7398, 300, Ret::None, [level1, pick] {
        level1();
        const uint32_t m = moby_spawn(0xaf);
        if (m == 0) return Args{{0u}, {}};
        w32(OV(0x001614cc), u(ri(0, 3) == 0));
        w32(OV(0x001614d4), u(ri(0, 2) == 0));
        w32(OV(0x001614d0), u(ri(0, 4) == 0 ? 0 : ri(0, 20)));
        for (uint32_t i = 0; i < 10; ++i) { const uint32_t r = OV(0x001db440) + i * 0xc; wf(r, rf(0, 3)); w16(r + 4, u(ri(0, 8))); w16(r + 6, u(ri(0, 4))); w16(r + 8, u(ri(0, 1))); }
        wf(OV(0x001614b8), rf(0.01f, 0.3f)); wf(OV(0x001614b4), rf(1, 6)); w32(OV(0x001614b0), u(ri(5, 30))); w16(OV(0x001614bc), u(ri(0, 4)));
        w16(OV(0x001614d8), u(ri(1, 30)));
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) taunter_rings(a.i[0]); return uint64_t{0}; });
    check("taunter_update", 0x002b6f10, 800, Ret::None, [taunt_world, pick, near_hero] {
        taunt_world();
        if (thand2 == 0) return Args{{0u}, {}};
        const uint32_t m = moby_spawn(ee_read<uint16_t>(thand2 + 0xa6));
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        VV(m + 0x10) = ee_ref<Vec4>(0x0013f3d0);
        w8(m + 0x20, pick({0u, 1u, 2u, 2u, 2u, 2u, 3u}));
        w8(m + 0x53, pick({0u, 1u, 2u, 3u, 3u}));
        w8(m + 0x70, R() & 2);
        w32(pv, u(ri(0, 3)));
        w32(pv + 4, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 14)));
        wf(pv + 8, ri(0, 3) == 0 ? 0.0f : rf(5.0f, 30.0f));
        w32(pv + 0xc, ri(0, 2) == 0 ? pick({15u, 16u, 17u}) : u(ri(0, 30)));
        w32(pv + 0x10, u(ri(0, 40)));
        w32(pv + 0x14, u(ri(0, 3)));
        w32(pv + 0x18, ri(0, 2) == 0 ? u(ri(28, 32)) : u(ri(0, 40)));
        w32(P + 0x10b4, u(ri(0, 4)));
        w32(OV(0x0015f5cc), R());
        w32(OV(0x001614cc), u(ri(0, 3) == 0)); w32(OV(0x001614d4), u(ri(0, 2)));
        w32(OV(0x001614d0), u(ri(0, 20)));
        wf(OV(0x001614b8), rf(0.01f, 0.3f)); wf(OV(0x001614b4), rf(1, 6)); w32(OV(0x001614b0), u(ri(5, 30))); w16(OV(0x001614d8), u(ri(1, 30)));
        for (uint32_t i = 0; i < 16; ++i) { w32(0x0013e550 + i * 0x70 + 0x88, coin() ? m : 0u); w8(0x0013e550 + i * 0x70 + 0x74, u(coin())); }
        w32(0x00141680 + 0x70, R()); w32(0x00141680 + 0x74, R());
        w32(0x0015eea4, u(ri(0, 6000))); w32(0x0015ed84, u(ri(0, 31)));
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) taunter_update(a.i[0]); return uint64_t{0}; });
    // --- the Walloper (item 0x12, hand class 0xb4)
    static uint32_t whand = 0;
    {
        level1();
        whand = item_moby(OVF(0x002bb558));
        std::printf("  (walloper hand moby %08x)\n", whand);
    }
    check("walloper_update", 0x002bb558, 600, Ret::None, [level1, hero_vary, pick, pickf, near_hero] {
        level1();
        if (whand == 0) return Args{{0u}, {}};
        hero_vary(0x12);
        const uint32_t m = moby_spawn(ee_read<uint16_t>(whand + 0xa6));
        VV(m + 0x10) = ee_ref<Vec4>(0x0013f3d0);
        wfrand(m + 0xd0, 3, -1.0f, 1.0f);
        w8(m + 0x20, pick({0u, 2u, 2u, 2u, 2u, 1u}));
        w8(m + 0x52, u(ri(0, 2))); w8(m + 0x53, u(ri(0, 2)));
        wf(m + 0x54, ri(0, 2) == 0 ? 0.0f : rf(0.0f, 1.0f));
        if (ee_read<uint32_t>(m + 0x68) != 0) w16(ee_read<uint32_t>(m + 0x68) + 4, u(ri(0, 300)));
        if (ee_read<uint32_t>(m + 0x6c) != 0) w16(ee_read<uint32_t>(m + 0x6c) + 4, u(ri(0, 300)));
        w32(P + 0x2084, pick({0x20u, 0x20u, 0x20u, 5u}));
        w8(ee_read<uint32_t>(P + 0x2080) + 0x53, pick({0x82u, 0x10u}));
        for (uint32_t i = 0; i < 10; ++i) {
            w16(OV(0x001dbec0) + i * 2, ri(0, 2) == 0 ? 0u : u(ri(0, 4)));
            w16(OV(0x001dbed8) + i * 2, u(ri(0, 0x40)));
            for (uint32_t k = 0; k < 5; ++k) wfrand(OV(0x001dbba0) + i * 0x50 + k * 0x10, 3, -2.0f, 2.0f);
        }
        w32(OV(0x001616a8), u(ri(5, 40))); w32(OV(0x001616ac), u(ri(5, 40)));
        w32(OV(0x001616cc), u(ri(0, 3) == 0 ? 0 : ri(0, 40))); w32(OV(0x001616d0), u(ri(0, 3) == 0 ? 0 : ri(0, 40)));
        wf(OV(0x001616a4), rf(0.05f, 0.6f));
        near_hero(OV(0x00166dc0), 8.0f, -1.0f, 4.0f);
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) walloper_update(a.i[0]); return uint64_t{0}; });
    // --- the weapons' draw callbacks
    check("taunter_rings_draw", 0x002b7568, 300, Ret::None, [level1, near_hero] {
        level1();
        const uint32_t m = M2;
        wfrand(m + 0xd0, 3, -1.0f, 1.0f); wf(m + 0xdc, 0.0f);
        near_hero(OV(0x001614e0), 4.0f, 0.0f, 3.0f);
        wfrand(0x0013f5e0, 3, -1.0f, 1.0f);
        for (uint32_t i = 0; i < 10; ++i) { const uint32_t r = OV(0x001db440) + i * 0xc; wf(r, rf(0, 8)); w16(r + 4, u(ri(0, 40))); w16(r + 8, u(ri(0, 1))); }
        wf(OV(0x001614c8), rf(0.3f, 2)); wf(OV(0x001614c4), rf(2, 9)); w32(OV(0x001614bc), u(ri(0, 255))); w32(OV(0x001614c0), R());
        wf(OV(0x001614dc), rf(0.01f, 0.2f));
        if (coin()) for (uint32_t t : {8u}) { w32(OV(0x0016d180) + t * 0x10, 0); w32(OV(0x0016d184) + t * 0x10, 0); }
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x100)));
        return Args{{m}, {}};
    }, [](A a) { taunter_rings_draw(a.i[0]); return uint64_t{0}; });
    check("mine_marker_draw", 0x002a9c50, 300, Ret::None, [level1, near_hero] {
        level1();
        const uint32_t m = M2;
        w32(m + 0x78, OUT);
        near_hero(OUT + 0x10, 3.0f, 0.0f, 3.0f);
        wfrand(OUT + 0x20, 3, -1.0f, 1.0f);
        w32(OV(0x0015f5c4), ri(0, 4) == 0 ? 2u : u(ri(0, 1)));
        w32(OV(0x0015f5cc), R() & 0xffff);
        if (coin()) for (uint32_t t : {0x11u, 0x12u}) { w32(OV(0x0016d180) + t * 0x10, 0); w32(OV(0x0016d184) + t * 0x10, 0); }
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x100)));
        return Args{{m}, {}};
    }, [](A a) { mine_marker_draw(a.i[0]); return uint64_t{0}; });
    check("walloper_ribbon", 0x002bbbc8, 300, Ret::None, [level1, near_hero] {
        level1();
        for (uint32_t k = 0; k < 5; ++k) { near_hero(OUT + k * 0x10, 3.0f, 0.0f, 3.0f); }
        near_hero(OV(0x00166dc0), 8.0f, -1.0f, 4.0f);
        wfrand(0x0013f5e0, 3, -1.0f, 1.0f);
        w32(OV(0x00161680), R() & 0xffffffu); w32(OV(0x00161684), R() & 0xffffffu);
        if (coin()) for (uint32_t t : {0u, 1u, 2u}) { w32(OV(0x0016d180) + t * 0x10, 0); w32(OV(0x0016d184) + t * 0x10, 0); }
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x100)));
        return Args{{OUT, u(ri(0, 255)), u(ri(0, 255)), 5u}, {rf(0.01f, 0.5f), rf(0.01f, 0.5f), rf(0.0f, 0.5f)}};
    }, [](A a) { walloper_ribbon(a.i[0], static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]), static_cast<int32_t>(a.i[3]), a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
    check("walloper_glow", 0x002bc1a0, 300, Ret::None, [level1, near_hero] {
        level1();
        const uint32_t m = M2;
        near_hero(m + 0x10, 2.0f, 0.0f, 2.0f);
        wfrand(m + 0xd0, 3, -1.0f, 1.0f);
        near_hero(OV(0x00166dc0), 8.0f, -1.0f, 4.0f);
        wfrand(0x0013f5e0, 3, -1.0f, 1.0f);
        wf(OV(0x001616bc), rf(0.0f, 1.0f)); wf(OV(0x001616b8), rf(0.1f, 2.0f)); wf(OV(0x001616d4), rf(0.0f, 1.0f));
        w32(OV(0x001616c0), R()); w32(OV(0x001616c4), R());
        w32(OV(0x001616c8), u(ri(0, 3)));
        seed_rand();
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x100)));
        return Args{{m}, {}};
    }, [](A a) { walloper_glow(a.i[0]); return uint64_t{0}; });
    check("walloper_trail_draw", 0x002bbb00, 200, Ret::None, [level1, near_hero] {
        level1();
        const uint32_t m = M2;
        near_hero(m + 0x10, 2.0f, 0.0f, 2.0f);
        wfrand(m + 0xd0, 3, -1.0f, 1.0f);
        near_hero(OV(0x00166dc0), 8.0f, -1.0f, 4.0f);
        wfrand(0x0013f5e0, 3, -1.0f, 1.0f);
        for (uint32_t i = 0; i < 10; ++i) {
            w16(OV(0x001dbec0) + i * 2, ri(0, 2) == 0 ? 0u : u(ri(1, 3)));
            w16(OV(0x001dbed8) + i * 2, u(ri(0, 0x40)));
            for (uint32_t k = 0; k < 5; ++k) near_hero(OV(0x001dbba0) + i * 0x50 + k * 0x10, 3.0f, 0.0f, 3.0f);
        }
        wf(OV(0x00161690), rf(0.01f, 0.5f)); wf(OV(0x00161694), rf(0.01f, 0.5f)); wf(OV(0x001616d4), rf(0.0f, 1.0f));
        w32(OV(0x00161680), R() & 0xffffffu); w32(OV(0x00161684), R() & 0xffffffu);
        wf(OV(0x001616bc), rf(0.0f, 1.0f)); wf(OV(0x001616b8), rf(0.1f, 2.0f));
        w32(OV(0x001616c0), R()); w32(OV(0x001616c4), R());
        seed_rand();
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x100)));
        return Args{{m}, {}};
    }, [](A a) { walloper_trail_draw(a.i[0]); return uint64_t{0}; });
    // --- the Glove of Doom (item 0x14, hand class 0xe5, agent class 0xe6)
    static uint32_t dhand = 0;
    {
        level1();
        dhand = item_moby(OVF(0x002c7a58));
        std::printf("  (glove of doom hand moby %08x, agent class 0xe6 %s)\n", dhand, moby_spawn(0xe6) != 0 ? "present" : "absent");
    }
    check("doom_throw_velocity", 0x002c7320, 300, Ret::None, [level1, near_hero, hero_vary] {
        level1();
        hero_vary(0x14);
        near_hero(OUT, 0.5f, 0.5f, 1.5f);
        near_hero(OUT + 0x10, ri(0, 3) == 0 ? 3.0f : 14.0f, ri(0, 3) == 0 ? -1.0f : -3.0f, 4.0f);
        wfrand(OUT + 0x20, 4, 0, 0);
        const float g = rf(0.0f, 2.0f) * 9.0f;
        return Args{{OUT, OUT + 0x10, OUT + 0x20}, {g, 1e-5f, coin() ? 2.5f : 3.5f}};
    }, [](A a) { doom_throw_velocity(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
    check("doom_agent_spawn", 0x002c8218, 100, Ret::Int32, [level1, near_hero, hero_vary] {
        level1();
        hero_vary(0x14);
        w8(P + 0x20a5, u(ri(0, 1))); w8(P + 0x20af, u(ri(0, 1)));
        near_hero(OUT, 3.0f, 0.0f, 2.0f);
        wfrand(OUT + 0x20, 3, -1.0f, 1.0f);
        if (ri(0, 8) == 0) w32(OV(0x00160170), 0x800);
        seed_rand();
        const uint32_t h = dhand != 0 ? dhand : ee_read<uint32_t>(P + 0x2080);
        return Args{{h, OUT, OUT + 0x20}, {}};
    }, [](A a) { return r32(doom_agent_spawn(a.i[0], a.i[1], a.i[2])); });
    {
    // (stage 5, task SPR) doom_agent_launch calls segment_collide: the collision scratch words left out
    // (docs/oracle.md)
    const CollisionScratchIgnored spr_ignored;
    check("doom_agent_launch", 0x002c82f0, 300, Ret::None, [level1, near_hero] {
        level1();
        const uint32_t ag = moby_spawn(0xe6);
        if (ag == 0) return Args{{0u, 0u, 0u}, {}};
        const uint32_t apv = ee_read<uint32_t>(ag + 0x78);
        near_hero(OUT, ri(0, 3) == 0 ? 6.0f : 1.0f, 0.2f, 2.0f);
        wfrand(apv, 3, -8, 8);
        w32(P + 0x2fc, ri(0, 4) == 0 ? ee_read<uint32_t>(P + 0x2080) : 0u);
        return Args{{OUT, ag, apv}, {}};
    }, [](A a) { if (a.i[1] != 0) doom_agent_launch(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
    }
    auto doom_hand = [pick, near_hero](uint32_t state) -> uint32_t {
        if (dhand == 0) return 0;
        const uint32_t m = moby_spawn(ee_read<uint16_t>(dhand + 0xa6));
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        VV(m + 0x10) = ee_ref<Vec4>(0x0013f3d0);
        for (uint32_t k = 0; k < 0x30; k += 0x10) VV(m + 0xc0 + k) = VV(0x0013f990 + k);
        w8(m + 0x20, state);
        w8(m + 0x70, R() & 2);
        w16(m + 0x34, R() & 0x0f00);
        w32(pv + 0x54, ri(0, 2) == 0 ? 0u : u(ri(0, 20)));
        if (ri(0, 4) != 0) {
            const uint32_t ag = doom_agent_spawn(m, pv + 0x40, OUT + 0x80);
            w32(pv + 0x50, ag);
        } else {
            w32(pv + 0x50, 0);
        }
        w32(0x00141680 + 0xa0, R()); w32(0x00141680 + 0xa4, R());
        w32(0x0015eea4, u(ri(0, 6000))); w32(0x0015ed84, u(ri(0, 31)));
        return m;
    };
    auto doom_world = [level1, hero_vary, near_hero, pick] {
        level1();
        hero_vary(0x14);
        if (ri(0, 2) == 0) { wfrand(OV(0x00166c80), 3, -1.0f, 1.0f); wfrand(OV(0x00166c90), 3, -1.0f, 1.0f); wfrand(OV(0x00166ca0), 3, -1.0f, 1.0f); near_hero(OV(0x00166dc0), 8.0f, -1.0f, 4.0f); }
        w32(P + 0x2fc, ri(0, 4) == 0 ? ee_read<uint32_t>(P + 0x2080) : 0u);
        if (ee_read<int32_t>(P + 0x2084) == 1) w32(P + 0x2084, 0x1eu);  // (hero_set_state(0x1e, 1) waits for a VU job the interpreter never finishes here; the same call is checked in the other hands)
    };
    {
    // (stage 5, task SPR) doom_aim calls segment_collide: the collision scratch words left out (docs/oracle.md)
    const CollisionScratchIgnored spr_ignored;
    check("doom_aim", 0x002c7548, 400, Ret::None, [doom_world, doom_hand] {
        doom_world();
        const uint32_t m = doom_hand(2);
        if (m == 0) return Args{{0u, 0u, 0u}, {}};
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        seed_rand();
        return Args{{m, pv, OUT}, {}};
    }, [](A a) { if (a.i[0] != 0) doom_aim(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
    }
    {
    // (stage 5, task SPR) doom_glove_update reaches segment_collide (through doom_aim): the collision scratch
    // words left out (docs/oracle.md)
    const CollisionScratchIgnored spr_ignored;
    check("doom_glove_update", 0x002c7a58, 1000, Ret::None, [doom_world, doom_hand, pick] {
        doom_world();
        const uint32_t m = doom_hand(pick({0u, 1u, 2u, 2u, 3u, 3u, 4u, 5u, 6u}));
        if (m == 0) return Args{{0u}, {}};
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) doom_glove_update(a.i[0]); return uint64_t{0}; });
    }
    g_ignored_words = ign_saved;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
