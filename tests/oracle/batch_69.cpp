// Oracle batch 69: the particles' update and their types (needs the ISO).

#include "ovl_oracle.h"

#include <game/ovl/ovl_reloc.h>
namespace rac1::oracle {

void batch_69() {
    if (g_only_batch != 69) return;
    auto boot = [] { veldin_boot(); };
    g_batch = 69;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // the update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;
    static std::vector<uint8_t> snap;
    auto level1 = [boot] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty()) {
            boot();
            particle_table_init();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            gameplay_frame();
            snap.assign(ram, ram + 0x02000000);
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    // a level moby with pvars
    auto pv_moby = [] {
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        for (;;) {
            const uint32_t m = first + 0x100 * u(ri(0, 200));
            if (ee_read<int8_t>(m + 0x20) >= 0 && ee_read<uint32_t>(m + 0x78) >= 0x00100000) return m;
        }
    };
    auto p13 = [] {
        const uint32_t p = alloc_particle(0x13);
        if (p == 0) return p;
        wfrand(p + 0x10, 4, 100, 140); wfrand(p + 0x20, 4, -2, 2); wfrand(p + 0x30, 4, -0.5f, 0.5f);
        w32(p + 4, R()); w32(p + 0xc, R());
        return p;
    };
    auto p15 = [] {
        Vec4 pos{rf(100, 140), rf(100, 140), rf(20, 40), 1}, vel{rf(-1, 1), rf(-1, 1), rf(-1, 1), 0};
        const uint32_t p = spawn_particle_15_v(pos, vel, R(), R(), u(ri(1, 30)), coin() ? 0u : (R() & 0xffff), rf(0, 3));
        if (p != 0) w16(p + 0xa, u(ri(0, 30)));
        return p;
    };
    auto p45 = [pv_moby] {
        const uint32_t p = alloc_particle(0x45);
        if (p == 0) return p;
        const float lo = ri(0, 8) == 0 ? -10.0f : 2.5f, hi = ri(0, 8) == 0 ? 1040.0f : 1020.0f;
        wfrand(p + 0x10, 4, lo, hi); wfrand(p + 0x20, 4, -3, 3);
        w16(p + 0x36, u(ri(0, 5)));
        const uint32_t m = pv_moby();
        w32(p + 0x3c, m);
        if (ri(0, 6) == 0) { Vec4 out{rf(-50, 1100), rf(2.5f, 1020), rf(2.5f, 1020), 1}; ee_ref<Vec4>(m + 0x10) = out; }
        w8(p + 8, R()); w16(p + 0xa, u(ri(0, 40)));
        wf(p + 0x30, rf(0, 0.2f)); w16(p + 0x34, u(ri(0, 255))); w32(p + 0x38, R() & 0xffffff);
        return p;
    };
    check("particle_13_update", 0x0026c7f0, 100, Ret::None, [level1, p13] { level1(); return Args{{p13()}, {}}; },
          [](A a) { particle_13_update(a.i[0]); return uint64_t{0}; });
    check("particle_15_update", 0x0026ccb0, 200, Ret::None, [level1, p15] {
        level1(); const uint32_t p = p15(); w32(OV(0x0015f5cc), R()); seed_rand(); return Args{{p}, {}};
    }, [](A a) { particle_15_update(a.i[0]); return uint64_t{0}; });
    check("particle_45_update", 0x00274a70, 200, Ret::None, [level1, p45] { level1(); const uint32_t p = p45(); seed_rand(); return Args{{p}, {}}; },
          [](A a) { particle_45_update(a.i[0]); return uint64_t{0}; });
    // the whole doubleword: the original's v0 is the PPACB-packed colour, its upper word zero (not bit 31's sign; task 0388)
    check("rgba_lerp_ovl", 0x0025a400, 100, Ret::Int64, [] { return Args{{R(), R()}, {rf(0, 1)}}; },
          [](A a) { return r32(rgba_lerp_ovl(a.i[0], a.i[1], a.f[0])); });
    check("rgba_lerp_ovl alpha >= 0x80", 0x0025a400, 100, Ret::Int64, [] { return Args{{R() | 0x80000000u, R() | 0x80000000u}, {rf(0, 1)}}; },
          [](A a) { return r32(rgba_lerp_ovl(a.i[0], a.i[1], a.f[0])); });
    check("particles_update", 0x00267c08, 100, Ret::None, [level1, p13, p15, p45] {
        level1();
        // only the ported types stay live
        const uint32_t base = ee_read<uint32_t>(OV(0x0016016c));
        const int32_t hi = ee_read<int32_t>(OV(0x00160174));
        for (int32_t i = 0; i <= hi; ++i) {
            const uint32_t p = base + u(i) * 0x40;
            const uint8_t t = ee_read<uint8_t>(p);
            if (ee_read<int8_t>(p + 1) >= 0 && t != 0x13 && t != 0x15 && t != 0x45) free_particle(p);
        }
        const int n = ri(0, 30);
        for (int i = 0; i < n; ++i) { const int k = ri(0, 2); if (k == 0) p13(); else if (k == 1) p15(); else p45(); }
        w32(OV(0x0015f5cc), R()); seed_rand();
        return Args{};
    }, [](A) { particles_update(); return uint64_t{0}; });
    check("rgba_to_vec", 0x001ff728, 100, Ret::None, [] { return Args{{OUT, R()}, {}}; },
          [](A a) { rgba_to_vec(ee_ref<Vec4>(a.i[0]), a.i[1]); return uint64_t{0}; });
    check("vec_to_rgba", 0x001ff740, 200, Ret::Int64, [] { wfrand(OUT, 4, coin() ? -300.0f : 0.0f, 300); return Args{{OUT}, {}}; },
          [](A a) { return uint64_t{vec_to_rgba(ee_ref<Vec4>(a.i[0]))}; });
    check("dir_pack", 0x00261480, 200, Ret::None, [] { const float r = coin() ? 0.01f : rf(0, 3); wfrand(OUT, 4, -r, r); return Args{{OUT, OUT + 0x10}, {}}; },
          [](A a) { dir_pack(a.i[0], a.i[1]); return uint64_t{0}; });
    check("dir_unpack", 0x002615a8, 200, Ret::None, [] { w32(OUT + 0x20, R()); return Args{{OUT, OUT + 0x20}, {}}; },
          [](A a) { dir_unpack(ee_ref<Vec4>(a.i[0]), a.i[1]); return uint64_t{0}; });
    check("spawn_particle_02", 0x002690b8, 100, Ret::Int32, [level1] {
        level1(); wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -2, 2); wfrand(OUT + 0x20, 4, -2, 2); seed_rand();
        const int32_t tex = ri(0, 3) == 0 ? -1 : ri(0, 20) | (coin() ? 0x10000 : 0);
        return Args{{OUT, OUT + 0x10, OUT + 0x20, R(), R(), u(ri(0, 40)), R() & 0xffff, R() & 0xffff, u(tex)}, {}};
    }, [](A a) { return r32(spawn_particle_02(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5], a.i[6], a.i[7], static_cast<int32_t>(a.i[8]))); });
    check("spawn_particle_0f", 0x0026bb70, 100, Ret::Int32, [level1] {
        level1(); wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -2, 2); seed_rand();
        return Args{{OUT, OUT + 0x10, R(), R(), coin() ? 0u : u(ri(1, 40)), R() & 0xffff, coin() ? 0xffffffffu : R() & 0xff, coin() ? 0xffffffffu : R() & 0xff}, {rf(0, 3)}};
    }, [](A a) { return r32(spawn_particle_0f(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5], static_cast<int32_t>(a.i[6]), static_cast<int32_t>(a.i[7]), a.f[0])); });
    check("spawn_particle_40", 0x002738e8, 100, Ret::Int32, [level1] {
        level1(); Vec4 h = ee_ref<Vec4>(0x0013f3d0); h.x += rf(-120, 120); h.y += rf(-120, 120); ee_ref<Vec4>(OUT) = h;
        wfrand(OUT + 0x10, 4, -2, 2); seed_rand();
        return Args{{OUT, OUT + 0x10, u(ri(0, 60)), R(), R(), u(coin())}, {rf(0, 0.001f), rf(-1, 1), rf(-1, 1)}};
    }, [](A a) { return r32(spawn_particle_40(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5], a.f[0], a.f[1], a.f[2])); });
    check("particle_02_update", 0x00269290, 300, Ret::None, [level1] {
        level1(); wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -0.2f, 0.2f); wfrand(OUT + 0x20, 4, -0.2f, 0.2f); seed_rand();
        const uint32_t p = spawn_particle_02(OUT, OUT + 0x10, OUT + 0x20, R(), R(), u(ri(1, 20)), u(ri(1, 20)), u(ri(1, 20)), -1);
        const int ph = ri(0, 2);
        if (ph >= 1) w16(p + 0x30, 0);
        if (ph >= 2) w16(p + 0x32, 0);
        w16(p + 0xa, u(ri(0, 20)));
        w16(p + 0x36, R() & 0xff); w16(p + 0x38, R() & 0xff);
        w32(OV(0x0015f5cc), R());
        return Args{{p}, {}};
    }, [](A a) { particle_02_update(a.i[0]); return uint64_t{0}; });
    check("particle_0b_update", 0x0026ac78, 400, Ret::None, [level1] {
        level1(); wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -1, 1); seed_rand();
        w32(OV(0x0015f5d0), 0); w32(OV(0x0015f5d4), 0);
        const uint32_t fl = R() & 0xf;
        spawn_particle_0b(OUT, OUT + 0x10, R(), R(), u(ri(1, 40)), R() & 0xff, u(ri(0, 2)), fl, rf(0, 3), rf(0, 2));
        const uint32_t base = ee_read<uint32_t>(OV(0x0016016c));
        const int32_t hi = ee_read<int32_t>(OV(0x00160174));
        uint32_t p = 0;
        for (int32_t i = hi; i >= 0 && p == 0; --i) if (ee_read<uint8_t>(base + u(i) * 64) == 0xb && ee_read<int8_t>(base + u(i) * 64 + 1) >= 0) p = base + u(i) * 64;
        if (p == 0) return Args{{0u}, {}};
        const uint32_t life = ee_read<uint8_t>(p + 0x3b);
        w16(p + 0xa, coin() ? u(float_to_int(static_cast<float>(life) * 0.85f)) : u(ri(0, 40)));
        if (ri(0, 4) == 0) w8(p + 3, 0x48);
        w8(p + 0x38, R() & 0xff);
        wf(OV(0x0015f5d0), ri(0, 3) == 0 ? 1.5f : 0.0f);
        return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_0b_update(a.i[0]); return uint64_t{0}; });
    check("particle_0f_update", 0x0026bca8, 300, Ret::None, [level1] {
        level1(); wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -1, 1); seed_rand();
        const uint32_t p = spawn_particle_0f(OUT, OUT + 0x10, R(), R(), u(ri(1, 30)), coin() ? 0u : (R() & 0xffff), coin() ? -1 : ri(0, 255), coin() ? -1 : ri(0, 255), rf(0, 3));
        if (p != 0) w16(p + 0xa, u(ri(0, 30)));
        w32(OV(0x0015f5cc), R());
        return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_0f_update(a.i[0]); return uint64_t{0}; });
    check("particle_22_update", 0x0026f280, 300, Ret::None, [level1] {
        level1(); wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -0.3f, 0.3f); seed_rand();
        const uint32_t p = spawn_particle_22(OUT, OUT + 0x10, rf(5000, 30000), rf(90, 150));
        if (p != 0) { w16(p + 0xa, u(ri(0, 40))); wf(p + 0x30, ee_read<float>(p + 0x18) + rf(-2, 60)); }
        return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_22_update(a.i[0]); return uint64_t{0}; });
    check("spawn_particle_42", 0x002741a0, 100, Ret::Int32, [level1] {
        level1(); wfrand(OUT, 4, 100, 140); seed_rand();
        return Args{{OUT, coin() ? 0xffffffffu : R()}, {rf(0, 1), rf(0, 5000)}};
    }, [](A a) { return r32(spawn_particle_42(a.i[0], static_cast<int32_t>(a.i[1]), a.f[0], a.f[1])); });
    check("spawn_particle_10", 0x0026bed0, 150, Ret::Int32, [level1] {
        level1(); wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -1, 1); seed_rand();
        wf(OV(0x0015f5d0), rf(0.5f, 1.2f));
        return Args{{OUT, OUT + 0x10, R(), R(), coin() ? 0u : u(ri(1, 30)), u(ri(0, 4))}, {rf(0, 3)}};
    }, [](A a) { return r32(spawn_particle_10(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5], a.f[0])); });
    {
        const CollisionScratchIgnored spr_ignored;
        check("particle_23_update", 0x0026f660, 300, Ret::None, [level1] {
            level1(); Vec4 h = ee_ref<Vec4>(0x0013f3d0); h.x += rf(-10, 10); h.y += rf(-10, 10); h.z += rf(-0.5f, 3); ee_ref<Vec4>(OUT) = h;
            wfrand(OUT + 0x10, 4, -0.5f, 0.5f); seed_rand();
            const uint32_t p = alloc_particle(0x23);
            if (p == 0) return Args{{0u}, {}};
            ee_ref<Vec4>(p + 0x10) = h; ee_ref<Vec4>(p + 0x20) = ee_ref<Vec4>(OUT + 0x10);
            w32(p + 4, R()); w32(p + 0x30, u(ri(0, 4))); w32(p + 0x34, u(ri(-1, 30))); wf(p + 0x38, h.z + rf(-2, 2)); wf(p + 0x3c, rf(0, 0.05f));
            wf(0x0013f640, h.z + rf(-1, 1));
            return Args{{p}, {}};
        }, [](A a) { if (a.i[0] != 0) particle_23_update(a.i[0]); return uint64_t{0}; });
    }
    check("particle_40_update", 0x00273a68, 300, Ret::None, [level1] {
        level1(); wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -0.3f, 0.3f); seed_rand();
        const uint32_t p = spawn_particle_40(OUT, OUT + 0x10, u(ri(0, 5)), R(), R(), u(coin()), rf(0, 0.0001f), rf(-1, 1), rf(0, 0.1f));
        if (p == 0) return Args{{0u}, {}};
        wf(p + 0x30, ee_read<float>(p + 0x18) + rf(-0.5f, 0.5f));
        if (coin()) w32(0x0015ed84, 10);
        w32(OV(0x00160228), u(ri(1, 4)));
        return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_40_update(a.i[0]); return uint64_t{0}; });
    check("particle_1c_update", 0x0026e150, 300, Ret::None, [level1] {
        level1();
        const uint32_t p = alloc_particle(0x1c);
        if (p == 0) return Args{{0u}, {}};
        wfrand(p + 0x10, 4, 100, 140); w32(p + 4, R()); wf(p + 0xc, rf(0, 50000)); w16(p + 0xa, u(ri(0, 30)));
        wf(p + 0x24, rf(0, 0.2f)); wf(p + 0x28, rf(0, 1)); wf(p + 0x2c, rf(-3, 3)); wfrand(p + 0x30, 4, -0.1f, 0.1f);
        return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_1c_update(a.i[0]); return uint64_t{0}; });
    check("particle_35_update", 0x002721f0, 300, Ret::None, [level1] {
        level1();
        const uint32_t p = alloc_particle(0x35);
        if (p == 0) return Args{{0u}, {}};
        const float lo = ri(0, 8) == 0 ? -5.0f : 100.0f;
        wfrand(p + 0x10, 4, lo, 140); w32(p + 4, R()); w16(p + 0xa, u(ri(0, 30)));
        wf(p + 0x20, rf(0, 3)); wf(p + 0x24, rf(0, 3)); w16(p + 0x28, u(ri(1, 36))); w8(p + 0x2a, R() & 0xff); w8(p + 0x2b, R() & 0xff);
        wfrand(p + 0x2c, 3, -0.2f, 0.2f); wf(p + 0x38, rf(-0.01f, 0.01f));
        return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_35_update(a.i[0]); return uint64_t{0}; });
    check("particle_04_update", 0x00269a70, 300, Ret::None, [level1] {
        level1(); wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -0.5f, 0.5f); seed_rand();
        spawn_particle_4(OUT, OUT + 0x10, R(), R(), u(ri(1, 60)), u(ri(0, 200)), u(ri(0, 200)), u(coin()));
        const uint32_t base = ee_read<uint32_t>(OV(0x0016016c));
        const int32_t hi = ee_read<int32_t>(OV(0x00160174));
        uint32_t p = 0;
        for (int32_t i = hi; i >= 0 && p == 0; --i) if (ee_read<uint8_t>(base + u(i) * 64) == 4 && ee_read<int8_t>(base + u(i) * 64 + 1) >= 0) p = base + u(i) * 64;
        if (p != 0) w16(p + 0xa, u(ri(0, ee_read<int32_t>(p + 0x3c))));
        return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_04_update(a.i[0]); return uint64_t{0}; });
    // a particle of `kind` with random fields (floats in its extension; pointer fields set per type)
    auto rpart = [](uint32_t kind) -> uint32_t {
        const uint32_t p = alloc_particle(kind);
        if (p == 0) return 0;
        wfrand(p + 0x10, 4, 100, 140); w32(p + 4, R()); w8(p + 8, R() & 0xff); w16(p + 0xa, u(ri(0, 40))); wf(p + 0xc, rf(0, 30000));
        wfrand(p + 0x20, 8, -1, 1);
        return p;
    };
    check("particle_0d_update", 0x0026b910, 200, Ret::None, [level1, rpart] {
        level1(); const uint32_t p = rpart(0xd); if (p == 0) return Args{{0u}, {}};
        w8(p + 0x28, R() & 0xff); w32(p + 0x24, u(coin())); w32(p + 0x34, R() & 0xffffff); wf(p + 0x2c, rf(0.9f, 1.1f));
        return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_0d_update(a.i[0]); return uint64_t{0}; });
    check("particle_2e_update", 0x002719f0, 200, Ret::None, [level1, rpart] {
        level1(); const uint32_t p = rpart(0x2e); if (p == 0) return Args{{0u}, {}};
        wf(OUT, ee_read<float>(p + 0x18) + rf(-0.3f, 0.3f)); w32(p + 0x38, coin() ? OUT : 0u); w16(p + 0xa, u(ri(0, 3)));
        return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_2e_update(a.i[0]); return uint64_t{0}; });
    check("particle_2f_update", 0x00271be8, 200, Ret::None, [level1, rpart] {
        level1(); const uint32_t p = rpart(0x2f); if (p == 0) return Args{{0u}, {}};
        w8(p + 0x30, R() & 0xff); return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_2f_update(a.i[0]); return uint64_t{0}; });
    check("particle_29_update", 0x00270710, 100, Ret::None, [level1, rpart] {  // (task 0415)
        level1(); const uint32_t p = rpart(0x29); if (p == 0) return Args{{0u}, {}};
        w16(p + 0xa, ri(0, 3) == 0 ? 0xffffu : u(ri(0, 2))); return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_29_update(a.i[0]); return uint64_t{0}; });
    check("particle_3b_update", 0x00273088, 200, Ret::None, [level1, rpart] {
        level1(); const uint32_t p = rpart(0x3b); if (p == 0) return Args{{0u}, {}};
        w32(p + 0x30, u(ri(0, 2))); return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_3b_update(a.i[0]); return uint64_t{0}; });
    check("particle_3e_update", 0x00273800, 200, Ret::None, [level1, rpart] {
        level1(); const uint32_t p = rpart(0x3e); if (p == 0) return Args{{0u}, {}};
        w16(p + 0x22, u(ri(0, 3))); w32(p + 0x24, ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(0, 100))); w16(p + 0x20, u(ri(0, 40)));
        return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_3e_update(a.i[0]); return uint64_t{0}; });
    check("particle_42_update", 0x00274308, 200, Ret::None, [level1, rpart] {
        level1(); const uint32_t p = rpart(0x42); if (p == 0) return Args{{0u}, {}};
        w16(p + 0xa, u(ri(0, 3))); if (coin()) w32(p + 4, R() & 0x01ffffff); return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_42_update(a.i[0]); return uint64_t{0}; });
    check("particle_46_update", 0x00274de0, 200, Ret::None, [level1, rpart] {
        level1(); const uint32_t p = rpart(0x46); if (p == 0) return Args{{0u}, {}};
        w16(p + 0xa, u(ri(0, 3))); if (coin()) w32(p + 4, R() & 0x01ffffff); return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_46_update(a.i[0]); return uint64_t{0}; });
    check("particle_12_update", 0x0026c558, 200, Ret::None, [level1, rpart] {
        level1(); const uint32_t p = rpart(0x12); if (p == 0) return Args{{0u}, {}};
        wf(p + 0x24, rf(-0.05f, 0.3f)); wf(p + 0x28, rf(0, 1.1f)); wf(p + 0x2c, rf(-3, 3)); return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_12_update(a.i[0]); return uint64_t{0}; });
    check("particle_2d_update", 0x00271748, 200, Ret::None, [level1, rpart] {
        level1(); const uint32_t p = rpart(0x2d); if (p == 0) return Args{{0u}, {}};
        if (ri(0, 8) == 0) wf(p + 0x10, rf(-5, 2.5f));
        wf(OUT, ee_read<float>(p + 0x18) + rf(-0.3f, 0.3f)); w32(p + 0x34, coin() ? OUT : 0u); w16(p + 0xa, u(ri(0, 5)));
        if (coin()) w32(p + 4, R() & 0x01ffffff);
        return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_2d_update(a.i[0]); return uint64_t{0}; });
    check("particle_3c_update", 0x00273298, 200, Ret::None, [level1, rpart] {
        level1(); const uint32_t p = rpart(0x3c); if (p == 0) return Args{{0u}, {}};
        w32(p + 0x3c, u(ri(0, 2))); if (ri(0, 8) == 0) wf(p + 0x10, rf(-5, 2.5f)); return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_3c_update(a.i[0]); return uint64_t{0}; });
    check("particle_17_update", 0x0026d1b0, 300, Ret::None, [level1, rpart] {
        level1(); const uint32_t p = rpart(0x17); if (p == 0) return Args{{0u}, {}};
        w32(p + 0x24, u(ri(0, 4))); w8(p + 0x28, R() & 7); wf(p + 0x2c, rf(0.9f, 1.1f)); w32(p + 0x20, R());
        w8(p + 0x2a, R() & 0xff); w8(p + 0x2b, u(ri(1, 255))); return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_17_update(a.i[0]); return uint64_t{0}; });
    {
        const CollisionScratchIgnored spr_ignored;
        check("particle_19_update", 0x0026d700, 300, Ret::None, [level1, rpart] {
            level1(); const uint32_t p = rpart(0x19); if (p == 0) return Args{{0u}, {}};
            Vec4 c = ee_ref<Vec4>(OV(0x00166dc0)); c.x += rf(-6, 6); c.y += rf(-6, 6); c.z += rf(-1, 3); ee_ref<Vec4>(p + 0x10) = c;
            if (ri(0, 6) == 0) wf(p + 0x10 + 4 * u(ri(0, 2)), rf(-1, 0.05f));
            if (ri(0, 4) == 0) wfrand(p + 0x20, 3, 0, 0); else wfrand(p + 0x20, 3, -3, 3);
            wf(p + 0x2c, coin() ? 0.0f : rf(-0.2f, 0.1f)); wfrand(p + 0x30, 3, -0.2f, 1); wf(p + 0x3c, rf(-0.05f, 1));
            wf(p + 0xc, rf(-10, 30000)); return Args{{p}, {}};
        }, [](A a) { particle_19_update(a.i[0]); return uint64_t{0}; });
    }
    check("particle_10_update", 0x0026c088, 300, Ret::None, [level1] {
        level1(); seed_rand();
        Vec4 c = ee_ref<Vec4>(OV(0x00166dc0)); c.x += rf(-12, 12); c.y += rf(-12, 12); ee_ref<Vec4>(OUT) = c;
        wfrand(OUT + 0x10, 4, -0.3f, 0.3f); wf(OV(0x0015f5d0), 0.5f);
        const uint32_t p = spawn_particle_10(OUT, OUT + 0x10, R(), R(), u(ri(1, 40)), u(ri(0, 3)), rf(0, 3));
        wf(OV(0x0015f5d0), rf(0.5f, 1.2f));
        w16(p + 0xa, u(ri(0, 40))); wf(p + 0x2c, ee_read<float>(p + 0x18) + rf(-0.5f, 0.5f));
        seed_rand();
        return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_10_update(a.i[0]); return uint64_t{0}; });
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
