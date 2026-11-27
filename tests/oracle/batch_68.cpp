// Oracle batch 68: the weapons: the bomb glove and its bombs (needs the ISO).

#include "ovl_oracle.h"

#include <game/ovl/ovl_reloc.h>
namespace rac1::oracle {

void batch_68() {
    if (g_only_batch != 68) return;
    auto boot = [] { veldin_boot(); };
    g_batch = 68;
    g_ignored_words = vcross_lanes();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime (null pointers read as RAM, as on the EE)
    static std::vector<uint8_t> snap;
    auto level1 = [boot] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty()) {
            boot();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            gameplay_frame();
            snap.assign(ram, ram + 0x02000000);
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    // a hand moby of the item whose class runs `fn` (its class loaded with the hero's items)
    auto item_moby = [](uint32_t fn) -> uint32_t {
        for (uint32_t i = 1; i < 0x25; ++i) {
            const uint32_t c = ee_read<uint32_t>(OV(0x00179ac0) + i * 0x4c + 0x10);
            if (c >= 0x800) continue;
            const uint32_t ci = ee_read<uint8_t>(OV(0x00197bc0) + c);
            if (ci == 0xff || ee_read<uint32_t>(OV(0x00197680) + ci * 4) != fn) continue;
            const uint32_t m = moby_spawn(c);
            return m;
        }
        return 0;
    };
    auto pick = [] (std::initializer_list<uint32_t> l) { return *(l.begin() + ri(0, static_cast<int>(l.size()) - 1)); };
    auto hero_vary = [pick] {
        constexpr uint32_t P = 0x0013f350;
        w32(P + 0x2084, pick({0u, 1u, 0x1eu, 0x23u, 0x23u, 0x32u}));
        w8(P + 0x20a8, u(ri(0, 2) == 0));
        w8(P + 0x20ac, u(ri(0, 4) == 0));
        w32(P + 0x1bc, u(frame_scale_i(0x11) + ri(-1, 1)));
        w32(P + 0x198, u(frame_scale_i(0x10) + ri(-1, 1)));
        w32(0x0013cae4, coin() ? ee_read<uint32_t>(P + 0x10a0) : 0u);
        w32(0x0013c940 + 0x1a0, ri(0, 2) == 0 ? (R() & 5) : 0u);
        w8(0x0013e52a, u(ri(0, 2)));
        wf(P + 0x98, rf(-3.1f, 3.1f));
        w32(P + 0x208c, ri(0, 6) == 0 ? 0xfu : 0u);
    };
    // hit-point leftovers of the collision 0x173e40.. are compared as they are
    static uint32_t glove = 0;
    check("bomb_throw_velocity", 0x002c2488, 200, Ret::None, [level1, hero_vary] {
        level1(); hero_vary();
        const uint32_t F = OUT, T = OUT + 0x10;
        Vec4 f{rf(100, 200), rf(100, 200), rf(20, 40), 0}; ee_ref<Vec4>(F) = f;
        Vec4 t{f.x + rf(-15, 15), f.y + rf(-15, 15), f.z + rf(-4, 4), 0}; ee_ref<Vec4>(T) = t;
        return Args{{F, T, OUT + 0x20}, {rf(5, 15), 1e-5f, rf(5, 12)}};
    }, [](A a) { bomb_throw_velocity(a.i[0], a.i[1], a.i[2], a.f[0], a.f[2]); return uint64_t{0}; });
    check("bomb_spawn", 0x002ace70, 60, Ret::Int32, [level1] {
        level1(); Vec4 p{rf(100, 200), rf(100, 200), rf(20, 40), 1}; ee_ref<Vec4>(OUT) = p; seed_rand();
        return Args{{R() & 0x1fffff0u, OUT}, {}};
    }, [](A a) { return r32(bomb_spawn(a.i[0], a.i[1])); });
    {
    // (stage 5, task SPR) bomb_throw_start calls segment_collide: the collision scratch words left out (docs/oracle.md)
    const CollisionScratchIgnored spr_ignored;
    check("bomb_throw_start", 0x002acfd8, 200, Ret::None, [level1, hero_vary] {
        level1(); hero_vary();
        const uint32_t b = bomb_spawn(0, 0x0013f3d0);
        const uint32_t pv = ee_read<uint32_t>(b + 0x78);
        Vec4 f = ee_ref<Vec4>(0x0013f3d0); f.x += rf(-1, 1); f.y += rf(-1, 1); f.z += rf(0, 1.5f); ee_ref<Vec4>(OUT) = f;
        Vec4 v{rf(-8, 8), rf(-8, 8), rf(-2, 6), 0}; ee_ref<Vec4>(OUT + 0x10) = v;
        // a lock-on target: a level moby, its motion +0x40
        if (ri(0, 2) != 0) {
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
            w32(pv + 0x60, first + 0x100 * u(ri(1, 200)));
            w32(pv + 0x64, u(ri(0, 3) != 0));
            Vec4 mv{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), 0, 0}; ee_ref<Vec4>(pv + 0x40) = mv;
        }
        if (ri(0, 4) == 0) w32(0x0013f350 + 0x2fc, 0);
        return Args{{OUT, b, OUT + 0x10}, {}};
    }, [](A a) { bomb_throw_start(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
    }
    {
    // (stage 5, task SPR) bomb_glove_update calls segment_collide: the collision scratch words left out
    // (docs/oracle.md)
    const CollisionScratchIgnored spr_ignored;
    check("bomb_glove_update", 0x002c26c8, 300, Ret::None, [level1, item_moby, hero_vary, pick] {
        level1();
        glove = item_moby(OVF(0x002c26c8));
        static bool told = false;
        if (!told) { told = true; std::printf("  (bomb glove moby %08x)\n", glove); }
        if (glove == 0) return Args{{0u}, {}};
        hero_vary();
        const uint32_t pv = ee_read<uint32_t>(glove + 0x78);
        w32(pv + 0x58, u(ri(0, 5) == 0 ? ri(0, 2) : 3));
        w32(pv + 0x54, u(ri(-1, 3)));
        w8(glove + 0x20, pick({0u, 1u, 2u, 3u, 3u, 3u, 4u, 5u, 6u}));
        w8(glove + 0x70, R() & 2);
        if (coin()) { seed_rand(); w32(pv + 0x50, bomb_spawn(glove, 0x0013f3d0)); }
        else w32(pv + 0x50, 0);
        return Args{{glove}, {}};
    }, [](A a) { bomb_glove_update(a.i[0]); return uint64_t{0}; });
    }
    {
    // (stage 5, task SPR) bomb_trace calls segment_collide: the collision scratch words left out (docs/oracle.md)
    const CollisionScratchIgnored spr_ignored;
    check("bomb_trace", 0x002ad410, 300, Ret::Int32, [level1, item_moby, hero_vary] {
        level1(); hero_vary();
        const uint32_t g = item_moby(OVF(0x002c26c8));
        const uint32_t b = bomb_spawn(g, 0x0013f3d0);
        const uint32_t pv = ee_read<uint32_t>(b + 0x78);
        Vec4 v{rf(-0.4f, 0.4f), rf(-0.4f, 0.4f), rf(-0.1f, 0.3f), 0}; ee_ref<Vec4>(pv) = v;
        if (coin()) { Vec4 p = ee_ref<Vec4>(0x0013f3d0); p.x += rf(-6, 6); p.y += rf(-6, 6); p.z += rf(-1, 4); ee_ref<Vec4>(b + 0x10) = p; }
        w8(b + 0x20, coin() ? 0u : u(ri(1, 4)));
        w32(pv + 0x50, g);
        w16(pv + 0x54, u(ri(-1, 300)));
        if (ri(0, 3) == 0) { const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)); w32(pv + 0x60, first + 0x100 * u(ri(1, 200))); w32(pv + 0x64, u(ri(0, 40))); }
        if (ri(0, 3) == 0) w32(ee_read<uint32_t>(g + 0x78) + 0x50, R());
        return Args{{pv, b, u(ri(0, 2))}, {}};
    }, [](A a) { return r32(static_cast<uint32_t>(bomb_trace(a.i[0], a.i[1], static_cast<int32_t>(a.i[2])))); });
    }
    {
    // (stage 5, task SPR) bomb_update reaches segment_collide (through bomb_trace): the collision scratch words
    // left out (docs/oracle.md)
    const CollisionScratchIgnored spr_ignored;
    check("bomb_update", 0x002adb30, 400, Ret::None, [level1, item_moby, hero_vary, pick] {
        level1(); hero_vary();
        const uint32_t g = item_moby(OVF(0x002c26c8));
        const uint32_t b = bomb_spawn(g, 0x0013f3d0);
        const uint32_t pv = ee_read<uint32_t>(b + 0x78);
        const int k = ri(0, 5);
        Vec4 p = ee_ref<Vec4>(0x0013f3d0);
        if (coin()) p = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(1, 200)) + 0x10);  // near a level moby
        p.x += rf(-8, 8); p.y += rf(-8, 8); p.z += k < 2 ? rf(-0.3f, 0.6f) : rf(0.5f, 4);
        ee_ref<Vec4>(b + 0x10) = p;
        Vec4 v{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), k < 2 ? rf(-0.4f, -0.05f) : rf(-0.2f, 0.3f), 0}; ee_ref<Vec4>(pv) = v;
        w8(b + 0x20, pick({0u, 1u, 1u, 1u, 1u, 2u}));
        w32(pv + 0x50, g);
        w16(pv + 0x54, u(ri(0, 3) == 0 ? ri(0, 2) : ri(0, 300)));
        w16(pv + 0x56, u(ri(0, 3) == 0));
        w16(pv + 0x68, u(ri(0, 2) == 0 ? 0 : ri(1, 30)));
        w16(pv + 0x6a, u(ri(0, 40)));
        w8(b + 0xbc, ri(0, 3) == 0 ? u(ri(0, 20)) : 0u);
        w32(OV(0x0015f5cc), R());
        if (ri(0, 4) == 0) w32(0x001403e0, 0);
        else if (ri(0, 3) != 0) w32(0x001403e0, g);
        w8(0x0013f350 + 0x20a4, u(ri(0, 5) == 0)); w8(0x0013f350 + 0x20a5, u(ri(0, 3) == 0)); w8(0x0013f350 + 0x20af, u(ri(0, 3) == 0));
        seed_rand();
        return Args{{b}, {}};
    }, [](A a) { bomb_update(a.i[0]); return uint64_t{0}; });
    }
    check("debris_7a_update", 0x002af5b8, 300, Ret::None, [level1] {
        level1();
        Vec4 p = ee_ref<Vec4>(0x0013f3d0); p.x += rf(-5, 5); p.y += rf(-5, 5); p.z += rf(0, 3);
        if (ri(0, 8) == 0) p.z = rf(-0.2f, 0.2f);
        Vec4 v{rf(-0.5f, 0.5f), rf(-0.5f, 0.5f), rf(-0.2f, 0.5f), 0};
        const uint32_t d = debris_7a_spawn_v(p, v, u(ri(1, 120)), u(coin()), u(ri(0, 1)));
        if (d == 0) return Args{{0u}, {}};
        w8(d + 0xbc, R() & 7);
        const uint32_t pv = ee_read<uint32_t>(d + 0x78);
        w16(pv + 0x18, u(ri(0, 60)));
        seed_rand();
        return Args{{d}, {}};
    }, [](A a) { if (a.i[0] != 0) debris_7a_update(a.i[0]); return uint64_t{0}; });
    const std::vector<uint32_t> ign_saved = g_ignored_words;
    for (uint32_t i = 0; i < 8; ++i) { g_ignored_words.push_back(OV(0x001802c0) + i * 0x20 + 4); g_ignored_words.push_back(OV(0x001802c0) + i * 0x20 + 8); }  // $f15 / $f16: not set by the harness
    check("light_add", 0x0023e738, 200, Ret::Int32, [level1] {
        level1(); wfrand(OUT, 4, 100, 140);
        for (uint32_t i = 0; i < 8; ++i) w32(OV(0x001804c0) + i * 0x30 + 0x10, ri(0, 2) == 0 ? 1u : 0u);
        wf(OV(0x0015f5d4), rf(0.5f, 0.9f));
        return Args{{OUT}, {rf(1, 20), rf(0, 2), rf(0, 2)}};
    }, [](A a) { return r32(static_cast<uint32_t>(light_add(a.i[0], a.f[0], a.f[1], a.f[2], 0.0f, 0.0f))); });
    g_ignored_words = ign_saved;
    check("effect_light_update", 0x002d3a10, 400, Ret::None, [level1] {
        level1();
        const uint32_t def = OUT + 0x40;
        wrand(def, 0x14);
        Vec4 p = ee_ref<Vec4>(0x0013f3d0); p.x += rf(-6, 6); p.y += rf(-6, 6); p.z += rf(0, 2); ee_ref<Vec4>(OUT) = p;
        const uint32_t m = effect_spawn(def, OUT, 0, 0);
        if (m == 0) return Args{{0u}, {}};
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, u(ri(0, 1)));
        w8(m + 0xbc, ri(0, 3) == 0 ? u(ri(1, 3)) : 0u);
        w8(pv + 0x1c, ri(0, 3) == 0 ? u(ri(1, 3)) : 0u);
        wfrand(pv, 3, -1, 1);
        w32(pv + 0x4c, R());
        w32(pv + 0x48, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 20)));
        w32(pv + 0x50, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 7)));
        w32(pv + 0x64, ri(0, 2) == 0 ? 0xffffffffu : u(ri(1, 150)));
        w32(pv + 0x20, ri(0, 2) == 0 ? 0u : ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(1, 150)));
        for (uint32_t k = 0x24; k < 0x34; ++k) w8(pv + k, R() & 0xff);
        wf(pv + 0x34, rf(0, 10)); wf(pv + 0x38, rf(5, 15)); wf(pv + 0x3c, rf(0, 5)); wf(pv + 0x40, rf(-1, 1)); wf(pv + 0x44, rf(-1, 1));
        for (uint32_t k = 0x54; k < 0x64; k += 4) wf(pv + k, rf(0, 3));
        for (uint32_t k = 0x68; k < 0x6c; ++k) w8(pv + k, u(ri(0, 4)));
        for (uint32_t k = 0x6c; k < 0x7c; k += 2) w16(pv + k, u(ri(0, 3) == 0 ? 0 : ri(1, 10)));
        for (uint32_t k = 0x7c; k < 0x80; ++k) w8(pv + k, u(ri(0, 3)));
        wf(OV(0x0015f5d4), rf(0.5f, 0.9f));
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) effect_light_update(a.i[0]); return uint64_t{0}; });
    g_extra_noop_hooks.clear();
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
