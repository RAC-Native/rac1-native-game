// Oracle batch 65: the visible zones from the camera on Veldin's real level data (needs the ISO).

#include "ovl_oracle.h"

#include <game/ovl/ovl_reloc.h>
namespace rac1::oracle {

void batch_65() {
    if (g_only_batch != 65) return;
    auto boot = [] { veldin_boot(); };
    g_batch = 65;
    g_ignored_words = vcross_lanes();
    static std::vector<uint8_t> snap;
    auto level = [boot] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty()) {
            boot();
            level_setup_body(1, 0);
            snap.assign(ram, ram + 0x02000000);
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    auto campos = [] {  // around the level's mobys, sometimes far outside
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        const uint32_t m = first + 0x100 * u(ri(0, 200));
        for (int k = 0; k < 3; ++k) wf(OV(0x00166dc0) + 4 * u(k), ee_read<float>(m + 0x10 + 4 * u(k)) + (ri(0, 5) == 0 ? rf(-400, 400) : rf(-6, 6)));
    };
    check("zone_grid_cell", 0x001f7278, 200, Ret::Int32, [level, campos] {
        level(); campos();
        return Args{{u(float_to_int(ee_read<float>(OV(0x00166dc0)) * 0.25f)), u(float_to_int(ee_read<float>(OV(0x00166dc4)) * 0.25f)), u(float_to_int(ee_read<float>(OV(0x00166dc8)) * 0.25f))}, {}};
    }, [](A a) { return r32(zone_grid_cell(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]))); });
    check("moby_visible_queue", 0x00250768, 100, Ret::Int32, [level, campos] {
        level(); campos();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        for (int k = 0; k < 20; ++k) {  // some mobys hidden, switched off, with or without a draw distance
            const uint32_t m = first + 0x100 * u(ri(0, 290));
            switch (ri(0, 3)) {
                case 0: w16(m + 0x34, ee_read<uint16_t>(m + 0x34) ^ 2u); break;
                case 1: w8(m + 0x20, 0xfe); break;
                case 2: w8(m + 0x31, R() & 1); w8(m + 0x30, u(ri(0, 255))); break;
                default: w16(m + 0x34, ee_read<uint16_t>(m + 0x34) ^ 0x1000u); break;
            }
        }
        return Args{};
    }, [](A) { return r32(moby_visible_queue()); });
    {  // the update pass: the class update functions not ported yet are no-ops on both sides
        level();
        std::vector<uint32_t> fns;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); ee_read<uint8_t>(m + 0x20) != 0xff; m += 0x100) {
            const uint32_t fn = ee_read<uint32_t>(m + 0x74);
            if (fn != 0 && !ovl_moby_fn_ported(fn) && std::find(fns.begin(), fns.end(), fn) == fns.end()) fns.push_back(fn);
        }
        std::printf("moby update functions not ported on Veldin: %zu\n", fns.size());
        for (uint32_t fn : fns) {
            int n = 0, cls = -1;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); ee_read<uint8_t>(m + 0x20) != 0xff; m += 0x100)
                if (ee_read<uint32_t>(m + 0x74) == fn) { ++n; cls = ee_read<int16_t>(m + 0xa6); }
            std::printf("  0x%06x: %d mobys (class %d)\n", fn, n, cls);
        }
        {  // the update functions of the level's classes (0x197680 by class index 0x197bc0[class]): what spawned mobys run
            std::printf("class update functions not ported:");
            for (uint32_t c = 0; c < 0x800; ++c) {
                const uint32_t ci = ee_read<uint8_t>(OV(0x00197bc0) + c);
                if (ci == 0xff || ee_read<uint32_t>(OV(0x00197300) + ci * 4) == 0) continue;
                const uint32_t fn = ee_read<uint32_t>(OV(0x00197680) + ci * 4);
                if (fn != 0 && !ovl_moby_fn_ported(fn)) std::printf(" %u:0x%06x", c, fn);
            }
            std::printf("\n");
        }
        for (uint32_t fn : {OVF(0x002a84c8u), OVF(0x002d89f0u)}) if (!ovl_moby_fn_ported(fn)) fns.push_back(fn);  // spawned mobys
        for (uint32_t fn : {OVF(0x002d5730u), OVF(0x002d57d0u)}) fns.push_back(fn);  // the class-749 hooks +4 / +0xc (TODO_PORT)
        g_extra_noop_hooks = fns;
        const std::vector<uint32_t> saved_ign = g_ignored_words;
        for (uint32_t i = 0; i < 8; ++i) g_ignored_words.push_back(OV(0x001e3d2c) + i * 0x10);  // nanotech orbit axes' w (VU register leftover)
        check("moby_update_visible", 0x00264848, 60, Ret::None, [level, campos] {
            level(); campos();
            w8(0x0013d390, 1);  // Veldin visited: the ship moby (class 834) does not start the opening cutscene (its disc reads)
            const int frames = ri(0, 3);
            for (int f = 0; f < frames; ++f) moby_update_visible();
            return Args{};
        }, [](A) { moby_update_visible(); return uint64_t{0}; });
        g_ignored_words = saved_ign;
        g_extra_noop_hooks.clear();
    }
    // the class update functions, on the level's own mobys of each class
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)  // spawned ones too
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // the level after its first gameplay frame (the port's), when the hero's equipment exists
    static std::vector<uint8_t> snap1;
    auto level1 = [level] {
        uint8_t* ram = ee_get_ram_base();
        if (snap1.empty()) {
            level();
            w32(OV(0x0016c060), 0xf);
            gameplay_frame();
            snap1.assign(ram, ram + 0x02000000);
        } else {
            std::memcpy(ram, snap1.data(), snap1.size());
        }
    };
    auto classcheck = [level1, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary) {
        level1();
        const std::vector<uint32_t> ms = mobys_with(ovl_xlate_code(fn));  // (the level's update function)
        if (ms.empty()) { std::printf("%-26s 0x%08x  no moby on this level\n", name, fn); return; }
        check(name, fn, 60, Ret::None, [level1, ms, vary] {
            level1();
            const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            vary(m);
            return Args{{m}, {}};
        }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
    };
    for (int pass = 0; pass < 2; ++pass)
    classcheck("smoke_vent_update", 0x002d6f28, smoke_vent_update, [pass](uint32_t m) {
        if (pass > 0) {  // set up first, then its frames (near the camera or not)
            smoke_vent_update(m);
            if (coin()) { Vec4 c = ee_ref<Vec4>(m + 0x10); c.x += rf(-20, 20); c.y += rf(-20, 20); c.z += rf(2, 10); ee_ref<Vec4>(OV(0x00166dc0)) = c; }
        }
    });
    check("free_particle", 0x00267a08, 100, Ret::None, [level1] {
        level1(); std::vector<uint32_t> ps;
        for (int i = ri(1, 12); i > 0; --i) { const uint32_t p = alloc_particle(u(ri(1, 0x40))); if (p != 0) ps.push_back(p); }
        for (uint32_t p : ps) if (ri(0, 3) == 0) w8(p + 1, 0x40);
        return Args{{ps[static_cast<size_t>(ri(0, static_cast<int>(ps.size()) - 1))]}, {}};
    }, [](A a) { free_particle(a.i[0]); return uint64_t{0}; });
    check("spawn_particle_3b", 0x00272f68, 60, Ret::Int32, [level1] {
        level1(); wfrand(OUT, 4, -50, 150);
        return Args{{OUT, R(), R(), u(ri(0, 3)), R() & 1, u(ri(1, 60)), u(ri(0, 2))}, {rf(0, 2)}};
    }, [](A a) { return r32(spawn_particle_3b(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5], a.i[6], a.f[0])); });
    check("spawn_particle_3c", 0x002730e0, 80, Ret::Int32, [level1] {
        level1(); wfrand(OUT, 4, -10, 1030); if (coin()) wfrand(OUT, 3, 50, 300); wfrand(OUT + 0x10, 4, -1, 1);
        return Args{{OUT, OUT + 0x10, R(), R(), R(), u(ri(0, 2))}, {rf(0, 2)}};
    }, [](A a) { return r32(spawn_particle_3c(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5], a.f[0])); });
    check("spawn_particle_3e", 0x00273708, 60, Ret::Int32, [level1] {
        level1(); wfrand(OUT, 4, -50, 150);
        return Args{{OUT, u(ri(1, 60)), u(ri(0, 3))}, {}};
    }, [](A a) { return r32(spawn_particle_3e(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2])); });
    check("spawn_particle_17", 0x0026d000, 60, Ret::Int32, [level1] {
        level1(); wfrand(OUT, 8, -50, 150);
        return Args{{OUT, u(ri(-50, 50)), OUT + 0x10, R()}, {rf(0, 1), rf(0, 1), rf(1, 2), rf(100, 2000)}};
    }, [](A a) { return r32(spawn_particle_17(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2], a.i[3], a.f[0], a.f[1], a.f[2], a.f[3])); });
    const std::vector<uint32_t> saved_ignored_e = g_ignored_words;
    for (uint32_t a = pvars_lo(); a < pvars_lo() + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }  // debris velocity / spin w: a stack leftover
    for (int pass = 0; pass < 3; ++pass)
    classcheck("e1440_update", 0x002e0b88, e1440_update, [pass](uint32_t m) {
        if (pass > 0) {  // set up first (state 0), then a state
            e1440_update(m);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(ri(1, 8)));
            if (ee_read<int32_t>(pv + 0x1b8) < 0 && ee_read<uint8_t>(m + 0x20) <= 2) w8(m + 0x20, 3);  // states 1 / 2 walk its path
            w8(m + 0x70, R() & 3);
            if (coin()) w32(pv + 0x114, u(ri(0, 2)));
            w16(pv + 0x1d8, u(ri(0, 3)));
            w32(pv + 0x110, ee_read<uint32_t>(0x001413d0));  // the target the states before set
            if (pass == 2) wf(pv + 0x20, rf(0, 0.5f));
        }
    });
    g_ignored_words = saved_ignored_e;
    for (int pass = 0; pass < 3; ++pass)
    classcheck("crate_update", 0x002cf218, crate_update, [pass](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        static const uint16_t cls[] = {0x1f4, 0x1f5, 0x1f6, 0x1ff};  // the crate kinds whose debris classes Veldin loads
        if (ri(0, 2) == 0) w16(m + 0xa6, cls[ri(0, 3)]);
        w8(m + 0x20, pass == 0 ? u(ri(0, 6)) : (pass == 1 ? (coin() ? 1u : 5u) : u(ri(2, 6))));
        w32(pv + 0xa0, 0); w32(pv + 0xa4, 0);
        w32(pv + 0xac, R() & 0x1f); w8(pv + 0xc8, coin() ? 0u : 1u); w8(pv + 0xc9, u(ri(0, 3))); w8(pv + 0xca, u(ri(0, 25)));
        w8(m + 0xbc, u(ri(0, 200))); w32(0x0015ed84, ri(0, 3) == 0 ? 0x12u : 0u); w32(0x001413dc, ri(0, 5) == 0 ? 0x16u : 0u);
        w32(pv + 0xb4, 0);
        if (coin()) {  // a hit
            const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
            w8(m + 0xa4, idx); w32(e + 0x34, m);
            static const uint32_t kinds[] = {0x10000, 0x20000, 0x1000000, 0x800000, 0x100000, 0x400000, 0x2000};
            w32(e + 0x24, kinds[ri(0, 6)]); wf(e + 0x2c, coin() ? 0.0f : rf(0.5f, 3)); w32(e + 0x20, coin() ? 0u : first + 0x100 * u(ri(0, 290)));
            w32(e + 0x28, coin() ? 0xb4u : u(ri(0, 20)));
            wfrand(e, 4, -1, 1);
        } else {
            w8(m + 0xa4, 0xff);
        }
        if (ri(0, 4) == 0) w32(0x0013f350 + 0x240, m);
        if (ri(0, 6) == 0) { w32(0x0013f350 + 0x2fc, m); w16(0x0013f350 + 0x30e, coin() ? 0u : 1u); }
    });
    // the level once the hero holds the wrench (P+0x1090, spawned a few frames in)
    static std::vector<uint8_t> snapw;
    auto levelw = [level1] {
        uint8_t* ram = ee_get_ram_base();
        if (snapw.empty()) {
            level1();
            const bool null_ram = g_ee_null_is_ram;
            g_ee_null_is_ram = true;  // (as on the EE: address 0 is RAM)
            for (int k = 0; k < 30 && ee_read<uint32_t>(0x0013f350 + 0x1090) == 0; ++k) { w32(OV(0x0015f3f8), ee_read<uint32_t>(OV(0x0015f3f8)) + 1); gameplay_frame(); }  // (the frame count: overlay_frame)
            g_ee_null_is_ram = null_ram;
            snapw.assign(ram, ram + 0x02000000);
        } else {
            std::memcpy(ram, snapw.data(), snapw.size());
        }
    };
    const bool null_ram_w = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // the wrench reads through null pointers (address 0 is RAM on the EE)
    for (int pass = 0; pass < 3; ++pass)
    check("wrench_update", 0x002a84c8, 60, Ret::None, [levelw, pass] {
        levelw();
        collide_vu_reset();
        const uint32_t m = ee_read<uint32_t>(0x0013f350 + 0x1090);
        constexpr uint32_t P = 0x0013f350;
        const uint32_t p = ee_read<uint32_t>(m + 0x78);
        static const uint32_t hs[] = {1, 0x13, 0x14, 0x2b, 0x70, 0x3b, 0x21, 0};
        w32(P + 0x2084, hs[ri(0, 7)]); w32(P + 0x208c, coin() ? 6u : (coin() ? 0xfu : u(ri(0, 20))));
        w32(P + 0x2090, coin() ? 0x3bu : 0u); w32(P + 0x10b4, ri(0, 8) == 0 ? 3u : 0u);
        w32(P + 0xa9c, ri(0, 3) == 0 ? 1u : 0u); w32(P + 0xa60, u(ri(0, 6))); wf(P + 0xaa8, rf(0, 30)); wf(P + 0xa90, rf(0, 2));
        w32(P + 0x198, u(ri(0, 40))); w32(P + 0xa64, coin() ? 1u : 0u); w8(P + 0x20ae, R() & 1); w8(P + 0x20b3, R() & 1);
        w32(P + 0x221c, coin() ? 0xffffffffu : u(ri(0, 8))); w32(P + 0xab0, coin() ? 0xffffffffu : 1u);
        w32(0x0013cae0, R() & 0xf);
        if (pass == 0) w8(m + 0x20, 0);
        else { w8(m + 0x20, pass == 1 ? 0xau : 0xbu); wfrand(p + 0x40, 3, -1, 1); wf(p + 0x60, rf(0, 1)); wf(p + 0x64, rf(-0.1f, 0.1f)); w32(p + 0x6c, u(ri(0, 3))); w16(p + 0x76, R() & 1);
               const Vec4 hp = ee_ref<Vec4>(0x0013f3d0); Vec4 q = hp; q.x += rf(-3, 3); q.y += rf(-3, 3); q.z += rf(0, 1.5f); ee_ref<Vec4>(m + 0x10) = q; }
        static const uint8_t sets[] = {0, 1, 0xe, 0xf, 0x10, 3, 4, 7};
        w8(m + 0x53, sets[ri(0, 7)]); w8(m + 0x70, R() & 2); w16(p + 0x74, R() & 3); w16(p + 0x7c, R() & 3);
        const uint32_t h = ee_read<uint32_t>(P + 0x2080);
        static const uint8_t ha[] = {0x17, 0x18, 0x19, 0x2b, 0x40, 0x41, 3};
        w8(h + 0x53, ha[ri(0, 6)]); w8(h + 0x52, coin() ? ee_read<uint8_t>(h + 0x53) : 3u);
        return Args{{m}, {}};
    }, [](A a) { wrench_update(a.i[0]); return uint64_t{0}; });
    g_ee_null_is_ram = null_ram_w;
    classcheck("moby_1007_update", 0x002a4e48, moby_1007_update, [](uint32_t m) {
        static const uint32_t C = OV(0x00166c80), P = 0x0013f350;
        const uint32_t cams[] = {0u, 0x01e03000u};
        w32(C + 0x180, cams[ri(0, 1)]);
        Vec4 q = ee_ref<Vec4>(0x0013f3d0); q.x += rf(-2, 2); q.y += rf(-2, 2); q.z += rf(-3, 3); ee_ref<Vec4>(0x01e03030) = q;
        wf(C + 0x158, rf(-3.1f, 3.1f)); wf(P + 0x98, rf(-3.1f, 3.1f)); wf(P + 0x164, coin() ? 0.0f : rf(0, 0.3f));
        w32(P + 0x208c, coin() ? 0x11u : 0u); (void)m;
    });
    classcheck("moby_1471_update", 0x002e1df0, moby_1471_update, [](uint32_t m) {  // (slots never requested: 0x2e2250 not reached)
        w8(m + 0x20, u(ri(0, 2)));
        for (uint32_t i = 0; i < 3; ++i) { static const uint32_t sv[] = {0, 1, 4}; w32(OV(0x00161bf8) + i * 4, sv[ri(0, 2)]); w32(OV(0x00161c88) + i * 4, R());
            w32(OV(0x00161c38) + i * 4, coin() ? 0xffffffffu : u(ri(0, 6))); w32(OV(0x00161c48) + i * 4, coin() ? 0xffffffffu : u(ri(0, 6))); }
    });
    classcheck("moby_607_update", 0x002d37d0, moby_607_update, [](uint32_t m) {
        w32(0x001413d4, coin() ? 8u : u(ri(0, 12))); w8(m + 0x53, coin() ? 6u : u(ri(0, 8)));
    });
    classcheck("moby_530_update", 0x002d1e80, moby_530_update, [](uint32_t m) {
        w8(m + 0x20, coin() ? 0u : 1u); w32(OV(0x0015f5c4), coin() ? 2u : u(ri(0, 6))); w32(OV(0x0016c890), u(ri(0, 6))); w32(OV(0x0015f428), u(ri(0, 0x41)));
    });
    classcheck("moby_plant_update", 0x002e3ca8, moby_plant_update, [](uint32_t m) {
        if (coin()) for (int k = 0; k < 3; ++k) wf(0x0013f3d0 + 4 * u(k), ee_read<float>(m + 0x10 + 4 * u(k)) + rf(-1.2f, 1.2f));
        wfrand(0x0013f450, 3, -0.3f, 0.3f); w8(m + 0x53, u(ri(0, 2))); w8(m + 0x70, R() & 3);
    });
    classcheck("moby_1060_update", 0x002df4f8, moby_1060_update, [](uint32_t m) {
        w8(m + 0x20, u(ri(0, 2))); wf(ee_read<uint32_t>(m + 0x78), rf(-3.1f, 3.1f)); wf(OV(0x00161b98), rf(-20, 20));
        w32(OV(0x00161b90), R()); w32(OV(0x00161b94), R()); w32(0x0015ed84, u(ri(0, 0x13))); w32(OV(0x0015f5cc), u(ri(0, 3)));
        w8(m + 0x21, coin() ? 0xffu : 1u); w32(ee_read<uint32_t>(ee_read<uint32_t>(m + 0x78) + 8), u(ri(0, 3))); w32(OV(0x0015f428), u(ri(0, 0x41)));
    });
    classcheck("moby_809_update", 0x002a4cd8, moby_809_update, [](uint32_t m) {
        w8(m + 0x20, u(ri(0, 2))); w32(OV(0x00161368), u(ri(1, 100))); w32(OV(0x0016136c), u(ri(1, 100))); w32(OV(0x00161370), u(ri(1, 100)));
        wf(OV(0x00161374), rf(-9, 1)); wf(OV(0x00161378), rf(-3, 3));
    });
    check("path_point", 0x00262e40, 100, Ret::None, [level1] {
        level1();
        uint32_t path = 0;
        while (path == 0) path = ee_read<uint32_t>(OV(0x001b04b0) + 4 * u(ri(0, 7)));
        const int n = static_cast<int>(ee_read<uint32_t>(path));
        return Args{{path, u(ri(0, 1)), OUT, OUT + 0x10, u(ri(0, 1))}, {rf(0, static_cast<float>(n) + 0.9f)}};
    }, [](A a) { path_point(a.f[0], a.i[0], static_cast<int32_t>(a.i[1]), a.i[2], a.i[3], a.i[4]); return uint64_t{0}; });
    check("countdown_u8", 0x001fefc8, N, Ret::Int32, [] { w8(M, coin() ? u(ri(0, 3)) : R()); return Args{{M}, {}}; },
          [](A a) { return r32(static_cast<uint32_t>(countdown_u8(a.i[0]))); });
    check("point_on_screen", 0x002002e0, 200, Ret::Int64, [level1] {
        level1(); render_view_setup();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        const uint32_t m = first + 0x100 * u(ri(0, 290));
        for (int k = 0; k < 3; ++k) wf(OUT + 4 * u(k), ee_read<float>(m + 0x10 + 4 * u(k)) + rf(-3, 3));
        wf(OUT + 12, coin() ? 1.0f : rf(0.5f, 2));
        return Args{{OUT}, {rf(0, 4)}};
    }, [](A a) { return static_cast<uint64_t>(point_on_screen(a.i[0], a.f[0])); });
    check("group_first", 0x002591d0, 200, Ret::Int32, [level1] {
        level1();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        for (int k = 0; k < 10; ++k) w8(first + 0x100 * u(ri(0, 290)) + 0x20, coin() ? 0xfeu : 1u);
        return Args{{OUT, u(ri(-2, static_cast<int>(ee_read<uint32_t>(OV(0x0015fff4))) + 2)), u(ri(0, 1)), u(ri(0, 1))}, {}};
    }, [](A a) { return r32(static_cast<uint32_t>(group_first(a.i[0], static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]), static_cast<int32_t>(a.i[3])))); });
    check("group_next", 0x002592b8, 200, Ret::Int32, [level1] {
        level1();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        for (int k = 0; k < 10; ++k) w8(first + 0x100 * u(ri(0, 290)) + 0x20, coin() ? 0xfeu : 1u);
        const int a = ri(0, 1), b = ri(0, 1);
        uint32_t cur = first + 0x100 * u(ri(0, 290));
        if (coin()) { group_first(OUT + 0x20, ri(0, static_cast<int>(ee_read<uint32_t>(OV(0x0015fff4)))), a, b); if (ee_read<uint32_t>(OUT + 0x20) != 0) cur = ee_read<uint32_t>(OUT + 0x20); }
        return Args{{OUT, cur, u(a), u(b)}, {}};
    }, [](A a) { return r32(static_cast<uint32_t>(group_next(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), static_cast<int32_t>(a.i[3])))); });
    check("crate_spawn_companion", 0x002d8138, 40, Ret::Int32, [level1] {
        level1();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        const uint32_t m = first + 0x100 * u(ri(0, 290));
        if (ri(0, 4) == 0) w8(m + 0x20, 0xfe);
        return Args{{m}, {}};
    }, [](A a) { return r32(crate_spawn_companion(a.i[0])); });
    {
        level();
        const std::vector<uint32_t> crates = mobys_with(OVF(0x002cf218));
        check("crate_settle", 0x002cfcb8, 60, Ret::None, [level, crates] {
            level();
            const uint32_t m = crates[static_cast<size_t>(ri(0, static_cast<int>(crates.size()) - 1))];
            if (ri(0, 3) == 0) wf(m + 0x18, ee_read<float>(m + 0x18) + rf(0, 2));
            return Args{{m}, {}};
        }, [](A a) { crate_settle(a.i[0]); return uint64_t{0}; });
    }
    check("moby_hit_react", 0x0025a478, 300, Ret::Int32, [level1] {
        level1();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        const uint32_t m = ri(0, 10) == 0 ? 0u : first + 0x100 * u(ri(0, 290));
        const uint32_t hs = M + 0x100, hit = M;
        wrand(hs, 0x20); wfrand(hs, 2, -4, 4); w8(hs + 8, u(ri(0, 7)));  // (+0/+4 floats in range) w16(hs + 0x1c, u(ri(0, 3))); w16(hs + 0x14, coin() ? 0x47u : R());
        wfrand(hit + 0x10, 4, -2, 2); wf(hit + 0x1c, coin() ? 5627.9248f : 0.0f);
        const uint32_t att = first + 0x100 * u(ri(0, 290));
        w32(hit + 0x20, ri(0, 3) == 0 ? 0u : att); w8(hit + 0x28, u(ri(0, 7)));  // (the push table 0x1b01c8 has 8 per kind) w8(hit + 0x29, u(ri(0, 4)));
        w16(hit + 0x2a, coin() ? 0x47u : u(ri(0, 3))); wf(hit + 0x2c, rf(0, 5));
        w32(OUT, R()); w32(OUT + 4, R()); w32(OUT + 8, R());
        const bool nohit = ri(0, 6) == 0, nohs = ri(0, 4) == 0;
        return Args{{m, nohit ? 0u : hit, nohs ? 0u : hs, R() & 0x3f, OUT, coin() ? OUT + 4 : 0u, coin() ? OUT + 8 : 0u, u(ri(0, 4))}, {}};
    }, [](A a) { return r32(static_cast<uint32_t>(moby_hit_react(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5], a.i[6], static_cast<int32_t>(a.i[7])))); });
    check("path_crossing", 0x00261b48, 100, Ret::Int32, [level1] {
        level1();
        int k = 0;
        uint32_t path = 0;
        while (path == 0) { k = ri(0, 7); path = ee_read<uint32_t>(OV(0x001b04b0) + 4 * u(k)); }
        const uint32_t pt = path + 0x10 + 0x10 * u(ri(0, static_cast<int>(ee_read<uint32_t>(path)) - 1));
        for (int c = 0; c < 3; ++c) { wf(OUT + 4 * u(c), ee_read<float>(pt + 4 * u(c)) + rf(-8, 8)); wf(OUT + 0x10 + 4 * u(c), ee_read<float>(pt + 4 * u(c)) + rf(-8, 8)); }
        if (coin()) for (int i = 0; i < 6; ++i) wf(path + 0x1c + 0x10 * u(i), coin() ? 1.0f : 0.0f);
        return Args{{u(k), OUT, OUT + 0x10, OUT + 0x20}, {}};
    }, [](A a) { return r32(static_cast<uint32_t>(path_crossing(static_cast<int32_t>(a.i[0]), a.i[1], a.i[2], a.i[3]))); });
    check("pickup_spawn", 0x002c52a8, 60, Ret::Int32, [level1] {
        level1();
        wfrand(OUT, 4, 100, 140);
        return Args{{OUT, u(ri(0, 0x24)), coin() ? 0xffffffffu : u(ri(0, 50)), u(ri(-3, 30))}, {}};
    }, [](A a) { return r32(pickup_spawn(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), static_cast<int32_t>(a.i[3]))); });
    check("crate_ammo_kind", 0x00257218, 200, Ret::Int32, [level1] {
        level1();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        for (uint32_t i = 0; i < 12; ++i) w8(0x0015edd0 + i, ri(0, 5) == 0 ? 0xffu : u(ri(0, 0x24)));
        for (uint32_t i = 0; i < 0x40; ++i) { w8(0x0013d4c0 + i, coin() ? 1u : 0u); w32(0x0013d428 + 4 * i, u(ri(0, 60))); }
        wrand(0x0014c190, 0x100);
        w32(OUT, ri(0, 2) == 0 ? u(ri(-1, 3)) : u(ri(1, 30)));
        return Args{{coin() ? first + 0x100 * u(ri(0, 290)) : 0u, OUT}, {}};
    }, [](A a) { return r32(static_cast<uint32_t>(crate_ammo_kind(a.i[0], a.i[1]))); });
    check("bolt_spawn", 0x002a7438, 100, Ret::Int32, [level1] {
        level1();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        const uint32_t src = first + 0x100 * u(ri(0, 290));
        if (coin()) { const uint32_t up = first + 0x100 * u(ri(0, 290)); if (up != src) { w32(src + 0xb8, up); w32(up + 0xb8, 0); } }
        wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -1, 1);
        static const int vals[] = {1, 5, 20, 50, 3, 7};
        return Args{{src, OUT, OUT + 0x10, R() & 0x12, u(vals[ri(0, 5)]), R() & 0xff}, {}};
    }, [](A a) { return r32(bolt_spawn(a.i[0], a.i[1], a.i[2], a.i[3], static_cast<int32_t>(a.i[4]), a.i[5])); });
    check("bolt_fly_to", 0x002a7210, 100, Ret::Int32, [level1] {
        level1();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -1, 1);
        const uint32_t b = bolt_spawn(first + 0x100 * u(ri(0, 290)), OUT, OUT + 0x10, 0, 1, 0);
        w8(b + 0x20, u(ri(1, 4)));
        w32(OV(0x001613ac), u(ri(0, 4))); w32(OV(0x0015f5cc), u(ri(0, 6)));
        return Args{{b, first + 0x100 * u(ri(0, 290))}, {coin() ? 0.0f : rf(-0.5f, 0.5f), coin() ? 0.0f : rf(0, 0.5f)}};
    }, [](A a) { return r32(static_cast<uint32_t>(bolt_fly_to(a.i[0], a.i[1], a.f[0], a.f[1]))); });
    check("bolts_drop", 0x00260a88, 200, Ret::None, [level1] {
        level1();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        const uint32_t m = first + 0x100 * u(ri(0, 290));
        if (coin()) w8(m + 0xb0, 0xff);
        w32(0x0015eea0, ri(0, 4) == 0 ? 1u : 0u); w32(0x0015ee20, ri(0, 4) == 0 ? 1u : 0u);
        w32(0x0015ee28, ri(0, 2) == 0 ? 0u : u(ri(1, 100))); w32(0x0015ee2c, u(ri(0, 400)));
        int path = -1;
        if (coin()) { for (int k = 0; k < 20 && path < 0; ++k) { const int c = ri(0, 7); if (ee_read<uint32_t>(OV(0x001b04b0) + 4 * u(c)) != 0) path = c; } }
        const int lo = ri(0, 40), hi = lo + ri(0, 300);
        return Args{{m, u(lo), u(hi), R() & 0x3f, u(path)}, {}};
    }, [](A a) { bolts_drop(a.i[0], static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]), a.i[3], static_cast<int32_t>(a.i[4])); return uint64_t{0}; });
    check("moby_drop_bolts", 0x00257470, 200, Ret::None, [level1] {
        level1();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        const uint32_t m = first + 0x100 * u(ri(0, 290));
        w16(m + 0xb4, u(ri(-2, 60))); w8(m + 0xb1, ri(0, 4) == 0 ? 0xfeu : (coin() ? 0xffu : u(ri(0, 8)))); w16(m + 0xb2, coin() ? u(ri(0, 200)) : R());
        w8(m + 0xb0, coin() ? 0xffu : u(ri(0, 10))); w16(m + 0xb6, u(ri(0, 80))); w32(OV(0x0015ffbc), u(ri(0, 200)));
        w32(0x001413d4, coin() ? 0x10u : 0u); w8(0x00141402, R() & 1);
        w32(0x0015ee28, 0);
        return Args{{m, R() & 0xf00, coin() ? 0xffffffffu : 0u}, {}};
    }, [](A a) { moby_drop_bolts(a.i[0], a.i[1], static_cast<int32_t>(a.i[2])); return uint64_t{0}; });
    // A spawned bolt's direction (pvars +0x20) has its w lane from an unset stack slot in
    // bolts_drop (0x260a88): stale data from the earlier calls (1.0 after 0x260400's matrices).
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    for (uint32_t a = pvars_lo(); a < pvars_lo() + 0x20000; a += 0x80) g_ignored_words.push_back(a + 0x2c);
    check("crate_break_drops", 0x002d0538, 200, Ret::None, [level1, mobys_with] {
        level1();
        const std::vector<uint32_t> crates = mobys_with(OVF(0x002cf218));
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        const uint32_t m = crates[static_cast<size_t>(ri(0, static_cast<int>(crates.size()) - 1))];
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (ri(0, 4) == 0) w16(m + 0xa6, 0x1f5);
        w16(pv + 0xc6, coin() ? 0u : 1u); w8(pv + 0xcb, u(ri(0, 13))); w16(pv + 0xc4, R() & 0xffff);
        int path = -1;
        if (coin()) { for (int k = 0; k < 20 && path < 0; ++k) { const int c = ri(0, 7); if (ee_read<uint32_t>(OV(0x001b04b0) + 4 * u(c)) != 0) path = c; } }
        w32(pv + 0xc0, u(path));
        w32(pv + 0xf0, ri(0, 4) == 0 ? first + 0x100 * u(ri(0, 290)) : 0u);
        if (ri(0, 4) == 0) { w32(0x0015ed84, 0xf); if (coin()) { wf(m + 0x10, 166.0f + rf(-7, 7)); wf(m + 0x14, 193.0f + rf(-7, 7)); } }
        w16(m + 0xb4, u(ri(0, 30))); w8(m + 0xb1, 0xff); w32(OV(0x0015ffbc), u(ri(0, 200))); w32(0x0015ee28, 0);
        w32(OV(0x0015f598), R());
        return Args{{m}, {}};
    }, [](A a) { crate_break_drops(a.i[0]); return uint64_t{0}; });
    g_ignored_words = saved_ignored;
    auto anymoby = [] { return ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(0, 290)); };
    auto colmoby = [] {  // a moby whose class has collision volumes
        for (int k = 0; k < 400; ++k) {
            const uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(0, 290)), c = ee_read<uint32_t>(m + 0x24);
            if (c != 0 && c < 0x02000000u && ee_read<uint32_t>(c + 0x10) != 0 && ee_read<uint32_t>(c + 0x10) < 0x02000000u) return m;
        }
        return 0u;
    };
    check("moby_volume_points", 0x0025aca0, 100, Ret::Int32, [level1, colmoby] {
        level1(); const uint32_t m = colmoby();
        return Args{{m, u(ri(0, 20)), OUT, coin() ? 0xffffffffu : R()}, {rf(0, 20)}};
    }, [](A a) { Vec4 pts[64]; const int32_t n = moby_volume_points(a.f[0], a.i[0], static_cast<int32_t>(a.i[1]), pts, a.i[3]);
                 for (int32_t i = 0; i < n; ++i) ee_ref<Vec4>(a.i[2] + 16 * u(i)) = pts[i]; return r32(static_cast<uint32_t>(n)); });
    check("enemy_physics", 0x0025c698, 300, Ret::Int32, [level1, mobys_with, colmoby] {
        level1();
        const std::vector<uint32_t> es = mobys_with(OVF(0x002d4610));
        const uint32_t m = coin() && !es.empty() ? es[static_cast<size_t>(ri(0, static_cast<int>(es.size()) - 1))] : colmoby();
        const uint32_t p = M;
        wfrand(p, 3, -0.3f, 0.3f); wf(p + 0xc, 0); wf(p + 0x10, rf(0, 0.03f)); wf(p + 0x14, rf(0, 0.05f)); wf(p + 0x1c, rf(0, 1));
        w32(p + 0x20, u(ri(0, 1500))); w32(p + 0x24, R() & 0x3f); wf(p + 0x28, rf(0, 1.5f)); w8(p + 0x2e, R() & 1); w8(p + 0x2f, coin() ? 0u : R() & 0xff);
        wf(p + 0x30, rf(0, 1)); w16(p + 0x3e, u(ri(0, 3))); wf(p + 0x40, rf(-3.1f, 3.1f)); wf(p + 0x44, rf(-0.1f, 0.1f)); wf(p + 0x48, rf(0, 0.2f));
        wf(p + 0x4c, rf(0, 0.2f)); wf(p + 0x50, ri(0, 3) == 0 ? -1.0f : rf(0, 20)); wf(p + 0x54, ri(0, 3) == 0 ? -1.0f : rf(0, 20)); wf(p + 0x5c, ri(0, 2) == 0 ? 0.0f : rf(1, 8));
        if (ri(0, 6) == 0) wf(m + 0x10, rf(0, 3));
        return Args{{m, p}, {}};
    }, [](A a) { return r32(enemy_physics(a.i[0], a.i[1])); }, 0x7000371c);  // (0x7000371c: a scratchpad temp whose stale lane depends on what earlier checks left; seen in the full batch only, task 0330)
    check("swap3_by_mask", 0x0025c088, 60, Ret::None, [] { wrand(OUT, 12); return Args{{OUT, OUT + 4, OUT + 8, R() & 7}, {}}; },
          [](A a) { uint32_t x = ee_read<uint32_t>(a.i[0]), y = ee_read<uint32_t>(a.i[1]), z = ee_read<uint32_t>(a.i[2]); swap3_by_mask(x, y, z, a.i[3]);
                    w32(a.i[0], x); w32(a.i[1], y); w32(a.i[2], z); return uint64_t{0}; });
    check("color_swap_channels", 0x0025c0e8, 60, Ret::Int32, [] { return Args{{R(), R() & 7}, {}}; },
          [](A a) { return r32(color_swap_channels(a.i[0], a.i[1])); });
    check("effect_spawn", 0x002d3838, 60, Ret::Int32, [level1] {
        level1(); wrand(T, 0x50); wfrand(T, 4, -1, 1); wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -1, 1);
        return Args{{T, OUT, R(), coin() ? OUT + 0x10 : 0u}, {}};
    }, [](A a) { return r32(effect_spawn(a.i[0], a.i[1], a.i[2], a.i[3])); });
    check("enemy_death_burst", 0x0025f3e8, 60, Ret::None, [level1, anymoby] {
        level1(); wfrand(OUT, 4, 100, 140);
        return Args{{coin() ? 0u : anymoby(), OUT}, {rf(0.3f, 2), ri(0, 2) == 0 ? 0.0f : rf(-3, 8)}};
    }, [](A a) { enemy_death_burst(a.i[0], a.i[1], a.f[0], a.f[1]); return uint64_t{0}; });
    check("explosion_burst", 0x0025f090, 60, Ret::None, [level1, anymoby] {
        level1(); wfrand(OUT, 4, 100, 140);
        if (coin()) { Vec4 h = ee_ref<Vec4>(0x0013f3d0); h.x += rf(-3, 3); h.y += rf(-3, 3); ee_ref<Vec4>(OUT) = h; }
        return Args{{coin() ? 0u : anymoby(), OUT, coin() ? 0xffffffffu : u(ri(0, 3))}, {rf(0.3f, 2), ri(0, 2) == 0 ? 0.0f : rf(-3, 8)}};
    }, [](A a) { explosion_burst(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), a.f[0], a.f[1]); return uint64_t{0}; });
    check("e1440_hits_and_target", 0x002e1678, 200, Ret::None, [level1, mobys_with] {
        level1(); const std::vector<uint32_t> es = mobys_with(OVF(0x002e0b88));
        const uint32_t m = es[static_cast<size_t>(ri(0, static_cast<int>(es.size()) - 1))];
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        w8(m + 0x20, u(ri(0, 9)));
        if (coin()) {  // a hit queued for it
            const uint32_t idx = u(ri(0, 7)), e = OV(0x00178100) + idx * 0x40;
            w8(m + 0xa4, idx); w32(e + 0x34, ri(0, 6) == 0 ? first : m); w32(e + 0x24, coin() ? 0x10000u << ri(0, 7) : R());
            wfrand(e + 0x10, 4, -2, 2); wf(e + 0x1c, coin() ? 5627.9248f : 0.0f);
            w32(e + 0x20, coin() ? 0u : first + 0x100 * u(ri(0, 290))); w8(e + 0x28, u(ri(0, 7))); w8(e + 0x29, u(ri(0, 4)));
            w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 3))); wf(e + 0x2c, rf(0, 3));
        }
        wf(pv + 0x20, rf(0, 4)); w32(pv + 0x38, R() & 1); w16(pv + 0x1da, coin() ? 0u : u(ri(1, 5)));
        w32(pv + 0x110, coin() ? 0u : ee_read<uint32_t>(0x001413d0)); w32(pv + 0x1b4, 0);  // (pvars not set up yet in the snapshot)
        w32(pv + 0x1bc, ri(0, 3) == 0 ? u(ri(0, 290)) : 0xffffffffu);
        return Args{{m}, {}};
    }, [](A a) { e1440_hits_and_target(a.i[0]); return uint64_t{0}; });
    check("angle_lerp", 0x0025b750, 100, Ret::Float, [] { return Args{{}, {rf(-3.1f, 3.1f), rf(-3.1f, 3.1f), rf(0, 1)}}; },
          [](A a) { return rf32(angle_lerp(a.f[0], a.f[1], a.f[2])); });
    check("round_decimals", 0x00261858, 200, Ret::Float, [] { return Args{{u(ri(-1, 5))}, {rf(-100, 100)}}; },
          [](A a) { return rf32(round_decimals(static_cast<int32_t>(a.i[0]), a.f[0])); });
    check("anim_crossed", 0x002618d8, 100, Ret::Int32, [level1, anymoby] {
        level1(); const uint32_t m = anymoby(); wf(m + 0x58, rf(0, 2)); wf(m + 0x5c, rf(0, 1));
        return Args{{m}, {rf(0, 30)}};
    }, [](A a) { return r32(static_cast<uint32_t>(anim_crossed(a.i[0], a.f[0]))); });
    check("joint_node_scale", 0x00263ac8, 60, Ret::None, [level1] {
        level1(); const uint32_t hero = ee_read<uint32_t>(0x001413d0);
        wrand(T, 0x30); w8(T + 1, R() & 1); w8(T, u(ri(0, 3))); w8(0x0015edb7, R() & 1);
        if (ee_read<uint8_t>(T + 1) != 0) { w32(T + 8, ee_read<uint32_t>(hero + 0x64)); w32(hero + 0x64, T); }
        return Args{{hero, u(ri(0, 3)), T}, {rf(0.5f, 2)}};
    }, [](A a) { joint_node_scale(a.i[0], a.i[1], a.i[2], a.f[0]); return uint64_t{0}; });
    check("walker_step", 0x00258490, 300, Ret::Int32, [level1, mobys_with, anymoby] {
        level1(); collide_vu_reset();
        const std::vector<uint32_t> es = mobys_with(OVF(0x002d4610));
        const uint32_t m = !es.empty() && coin() ? es[static_cast<size_t>(ri(0, static_cast<int>(es.size()) - 1))] : anymoby();
        Vec4 a = ee_ref<Vec4>(m + 0x10); ee_ref<Vec4>(OUT) = a; a.x += rf(-0.6f, 0.6f); a.y += rf(-0.6f, 0.6f); a.z += rf(-0.5f, 0.5f); ee_ref<Vec4>(OUT + 0x10) = a;
        return Args{{m, OUT, OUT + 0x10, R() & 3}, {rf(0, 1), rf(0.2f, 1), rf(0.1f, 1), rf(0.3f, 1.2f)}};
    }, [](A a) { return r32(static_cast<uint32_t>(walker_step(a.i[0], a.i[1], a.i[2], a.i[3], a.f[0], a.f[1], a.f[2], a.f[3]))); });
    check("wander_update", 0x00261630, 200, Ret::None, [level1, mobys_with, anymoby] {
        level1(); collide_vu_reset();
        const std::vector<uint32_t> es = mobys_with(OVF(0x002d4610));
        const uint32_t m = !es.empty() && coin() ? es[static_cast<size_t>(ri(0, static_cast<int>(es.size()) - 1))] : anymoby();
        const uint32_t r = T;
        Vec4 h = ee_ref<Vec4>(m + 0x10); h.x += rf(-5, 5); h.y += rf(-5, 5); ee_ref<Vec4>(r) = h;
        wf(r + 0x10, rf(0.2f, 1)); wf(r + 0x14, rf(0, 0.2f)); wf(r + 0x18, rf(0, 0.2f)); wf(r + 0x1c, rf(1, 10)); wf(r + 0x20, rf(0, 10)); wf(r + 0x24, rf(-3, 3));
        w16(r + 0x28, R() & 1); w16(r + 0x2a, u(ri(0, 4))); w16(r + 0x2c, u(ri(0, 20))); w16(r + 0x2e, u(ri(20, 60)));
        if (coin()) { Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-6, 6); q.y += rf(-6, 6); ee_ref<Vec4>(0x0013f3d0) = q; }
        return Args{{m, r}, {rf(0, 1), rf(0.1f, 1)}};
    }, [](A a) { wander_update(a.i[0], a.i[1], a.f[0], a.f[1]); return uint64_t{0}; });
    // the class-1440 enemy's chain
    check("moby_set_70", 0x00263b38, 40, Ret::None, [level1, anymoby] { level1(); w8(0x0015edb7, R() & 1); return Args{{anymoby()}, {rf(0, 3)}}; },
          [](A a) { moby_set_70(a.i[0], a.f[0]); return uint64_t{0}; });
    auto fxslots = [] {
        for (uint32_t o = 0; o < 0xa0; o += 4) w32(OV(0x00161bf8) + o, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 4)));
        for (uint32_t o = 0; o < 12; o += 4) w32(OV(0x00161c88) + o, coin() ? 0u : u(ri(1, 4)));
    };
    check("fx1471_release", 0x002e2040, 100, Ret::Int32, [level1, fxslots] { level1(); fxslots(); return Args{{u(ri(0, 4))}, {}}; },
          [](A a) { return r32(static_cast<uint32_t>(fx1471_release(a.i[0]))); });
    check("fx1471_set", 0x002e2190, 100, Ret::Int32, [level1, fxslots] { level1(); fxslots(); wrand(OUT, 0x50); return Args{{u(ri(0, 4)), OUT, OUT + 0x40}, {}}; },
          [](A a) { return r32(static_cast<uint32_t>(fx1471_set(a.i[0], a.i[1], a.i[2]))); });
    check("fx1471_claim", 0x002e1f28, 100, Ret::Int32, [level1, fxslots] { level1(); fxslots(); return Args{{u(ri(1, 5)), R(), R()}, {}}; },
          [](A a) { return r32(static_cast<uint32_t>(fx1471_claim(a.i[0], a.i[1], a.i[2]))); });
    check("rand_dir_scaled", 0x00257d00, 60, Ret::None, [level1] { level1(); wrand(OUT, 16); return Args{{OUT}, {rf(0, 1), rf(1, 3)}}; },
          [](A a) { rand_dir_scaled(a.i[0], a.f[0], a.f[1]); return uint64_t{0}; });
    check("spawn_particle_45", 0x00274948, 60, Ret::Int32, [level1] { level1(); wfrand(OUT, 8, -50, 150); return Args{{OUT, OUT + 0x10, R() & 0xff, R()}, {}}; },
          [](A a) { return r32(spawn_particle_45(a.i[0], a.i[1], a.i[2], a.i[3])); });
    check("e1440_flash", 0x002e1aa8, 60, Ret::None, [level1, mobys_with] {
        level1(); const std::vector<uint32_t> es = mobys_with(OVF(0x002e0b88));
        return Args{{es[static_cast<size_t>(ri(0, static_cast<int>(es.size()) - 1))]}, {}};
    }, [](A a) { e1440_flash(a.i[0]); return uint64_t{0}; });
    // the next checks' subjects are Veldin's class 1440 enemies (update 0x2e0b88): a level without
    // them skips those checks (--level N)
    level1();
    const bool no1440 = mobys_with(OVF(0x002e0b88u)).empty();
    if (no1440) skip_check("joint_node_spring", 0x002628d8, "no class 1440 moby on this level");
    else check("joint_node_spring", 0x002628d8, 150, Ret::None, [level1, mobys_with] {
        level1();
        const std::vector<uint32_t> es = mobys_with(OVF(0x002e0b88));
        const uint32_t hero = ee_read<uint32_t>(0x001413d0);
        const uint32_t m = coin() ? es[static_cast<size_t>(ri(0, static_cast<int>(es.size()) - 1))] : hero;
        wrand(T, 0x80); w8(T + 1, R() & 1); w8(T, u(ri(0, 3)));
        for (uint32_t o = 0x40; o < 0x70; o += 4) wf(T + o, ri(0, 2) == 0 ? 0.0f : rf(-0.5f, 0.5f));
        wf(T + 0x70, coin() ? 1.0f : rf(0.5f, 2));
        const uint32_t pick = u(ri(0, 3));
        w32(T + 0x78, pick == 0 ? 0u : pick == 1 ? m : (m == hero ? es[0] : hero));
        const uint32_t owner = ee_read<uint32_t>(T + 0x78) != 0 ? ee_read<uint32_t>(T + 0x78) : m;  // an attached node is in its moby's list
        if (ee_read<uint8_t>(T + 1) != 0) { w32(T + 8, ee_read<uint32_t>(owner + 0x64)); w32(owner + 0x64, T); }
        return Args{{m, T, u(ri(0, 3))}, {rf(0.05f, 0.5f), rf(0.1f, 0.9f)}};
    }, [](A a) { joint_node_spring(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1]); return uint64_t{0}; });
    check("point_in_polygon", 0x00259740, 200, Ret::Int32, [level1] {
        level1(); const int n = ri(0, 8);
        wfrand(OUT, 4, -2, 2); wfrand(T, 4 * n, -3, 3);
        if (ri(0, 4) == 0 && n > 1) wf(T + 0x14, ee_read<float>(OUT + 4));
        return Args{{OUT, T, u(n)}, {}};
    }, [](A a) { return r32(static_cast<uint32_t>(point_in_polygon(a.i[0], a.i[1], static_cast<int32_t>(a.i[2])))); });
    check("vec_yaw_marked", 0x0025ab48, 80, Ret::None, [level1] {
        level1(); wfrand(OUT, 4, -2, 2); if (coin()) w32(OUT + 0xc, 0x45afdf66u); wfrand(OUT + 0x10, 3, -2, 2);
        return Args{{OUT, OUT + 0x10, OUT + 0x14, OUT + 0x18}, {}};
    }, [](A a) { vec_yaw_marked(a.i[0], a.i[1], a.i[2], a.i[3]); return uint64_t{0}; });
    if (no1440) skip_check("moby_anim_set_flags", 0x002579c8, "no class 1440 moby on this level");
    else check("moby_anim_set_flags", 0x002579c8, 150, Ret::None, [level1, mobys_with] {
        level1(); const std::vector<uint32_t> es = mobys_with(OVF(0x002e0b88));
        const uint32_t hero = ee_read<uint32_t>(0x001413d0);
        const uint32_t m = coin() ? es[static_cast<size_t>(ri(0, static_cast<int>(es.size()) - 1))] : hero;
        if (coin()) wf(m + 0x54, rf(0, 1));
        const uint32_t cls = ee_read<uint32_t>(m + 0x24);
        const int sets = ee_read<uint8_t>(cls + 0xc);
        uint32_t set = u(ri(0, sets - 1));
        while (ee_read<uint32_t>(cls + 0x48 + set * 4) == 0) set = u(ri(0, sets - 1));  // a set the class has
        return Args{{m, set, u(ri(0, 5)), u(ri(1, 20)), R() & 7}, {}};
    }, [](A a) { moby_anim_set_flags(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]), a.i[4]); return uint64_t{0}; });
    if (no1440) skip_check("move_state_start", 0x0025c558, "no class 1440 moby on this level");
    else check("move_state_start", 0x0025c558, 120, Ret::None, [level1, mobys_with] {
        level1(); const std::vector<uint32_t> es = mobys_with(OVF(0x002e0b88));
        const uint32_t m = es[static_cast<size_t>(ri(0, static_cast<int>(es.size()) - 1))];
        wfrand(T, 0x14, -2, 2); w32(T + 0x24, R() & 0x3f);
        const uint32_t pw = ee_read<uint32_t>(ee_read<uint32_t>(m + 0x78));
        if (pw != 0 && coin()) wf(pw, 0.0f);
        const uint32_t cls = ee_read<uint32_t>(m + 0x24);
        const int sets = ee_read<uint8_t>(cls + 0xc);
        uint32_t set = u(ri(0, sets - 1));
        while (ee_read<uint32_t>(cls + 0x48 + set * 4) == 0) set = u(ri(0, sets - 1));
        return Args{{m, T, set, u(ri(1, 20)), u(ri(0, 3))}, {rf(-3.2f, 3.2f)}};
    }, [](A a) { move_state_start(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]), a.i[4], a.f[0]); return uint64_t{0}; });
    auto pulse = [] {
        w16(T, coin() ? 0u : u(ri(1, 30))); w16(T + 2, R() & 1);
        for (uint32_t o = 4; o < 10; ++o) w8(T + o, ri(0, 3) == 0 ? 0u : (R() & 0xff));
        w16(T + 0xc, u(ri(1, 30))); w16(T + 0xe, u(ri(1, 30)));
    };
    check("color_pulse_start", 0x0025d458, 100, Ret::None, [level1, anymoby, pulse] { level1(); pulse(); return Args{{anymoby(), T}, {}}; },
          [](A a) { color_pulse_start(a.i[0], a.i[1]); return uint64_t{0}; });
    check("color_pulse_update", 0x0025d538, 150, Ret::None, [level1, anymoby, pulse] { level1(); pulse(); return Args{{anymoby(), T}, {}}; },
          [](A a) { color_pulse_update(a.i[0], a.i[1]); return uint64_t{0}; });
    if (no1440) skip_check("target_select", 0x0025ff38, "no class 1440 moby on this level");
    else check("target_select", 0x0025ff38, 200, Ret::Int32, [level1, mobys_with] {
        level1(); const std::vector<uint32_t> es = mobys_with(OVF(0x002e0b88));
        const uint32_t m = es[static_cast<size_t>(ri(0, static_cast<int>(es.size()) - 1))];
        if (ri(0, 5) == 0) w32(0x0013f350 + 0x2084, 0x72);
        if (ri(0, 5) == 0) w32(0x0013f350 + 0x208c, 0x18);
        const int nbox = ri(0, 3) == 0 ? ri(1, 4) : 0;
        for (int i = 0; i < 4; ++i) w32(T + u(i) * 4, u(ri(-1, 4)));
        const int npoly = ri(0, 6);
        const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
        for (int i = 0; i < npoly; ++i) { wf(T + 0x40 + u(i) * 0x10, h.x + rf(-30, 30)); wf(T + 0x44 + u(i) * 0x10, h.y + rf(-30, 30)); }
        return Args{{m, OUT, T, u(nbox), coin() ? T + 0x40 : 0u, u(npoly)}, {rf(1, 200)}};
    }, [](A a) { return r32(static_cast<uint32_t>(target_select(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]), a.i[4], static_cast<int32_t>(a.i[5]), a.f[0]))); });
    if (no1440) skip_check("target_select_any", 0x0025fcb8, "no class 1440 moby on this level");
    else check("target_select_any", 0x0025fcb8, 100, Ret::Int32, [level1, mobys_with] {
        level1(); const std::vector<uint32_t> es = mobys_with(OVF(0x002e0b88));
        const uint32_t m = es[static_cast<size_t>(ri(0, static_cast<int>(es.size()) - 1))];
        if (ri(0, 4) == 0) w32(0x0013f350 + 0x2084, 0x72);
        if (ri(0, 4) == 0) w32(0x0013f350 + 0x208c, 0x18);
        return Args{{m, OUT}, {rf(1, 200)}};
    }, [](A a) { return r32(static_cast<uint32_t>(target_select_any(a.i[0], a.i[1], a.f[0]))); });
    check("seek_1d", 0x0025b970, 300, Ret::Float, [level1] {
        level1(); wf(OUT, rf(-5, 5)); wf(OUT + 4, ri(0, 4) == 0 ? 0.0f : rf(-1, 1));
        const float t = ri(0, 6) == 0 ? ee_read<float>(OUT) : rf(-5, 5);
        return Args{{OUT, OUT + 4}, {t, rf(0.01f, 0.3f), rf(0.01f, 0.3f), rf(0.1f, 2)}};
    }, [](A a) { return rf32(seek_1d(a.i[0], a.i[1], a.f[0], a.f[1], a.f[2], a.f[3])); });
    auto loadedcls = [] {  // a debris class of the level (the classes these spawn in the game)
        static const uint32_t cls[] = {0x78d, 0x78e, 0x78f, 0x7ab, 0x15c, 0x95};
        return cls[ri(0, 5)];
    };
    check("debris_spawn", 0x00263fd8, 120, Ret::Int32, [level1, mobys_with, loadedcls] {
        level1(); const std::vector<uint32_t> es = mobys_with(OVF(0x002e0b88));
        const uint32_t m = es[static_cast<size_t>(ri(0, static_cast<int>(es.size()) - 1))];
        for (uint32_t q = 0; q < 5; ++q) {
            if (coin()) wfrand(OUT + q * 0x10, 4, -2, 2); else for (uint32_t o = 0; o < 16; o += 4) w32(OUT + q * 0x10 + o, 0);
        }
        if (ri(0, 4) == 0) w32(OUT + 0x4c, 0x3f800000u);
        return Args{{m, loadedcls(), OUT, OUT + 0x10, coin() ? 0u : u(ri(1, 90)), R() & 3, OUT + 0x20, OUT + 0x30, OUT + 0x40}, {rf(0, 1)}};
    }, [](A a) { return r32(debris_spawn(a.i[0], a.i[1], a.i[2], a.i[3], static_cast<int32_t>(a.i[4]), a.i[5], a.i[6], a.i[7], a.i[8], a.f[0])); });
    const std::vector<uint32_t> saved_ignored_d = g_ignored_words;
    for (uint32_t a = pvars_lo(); a < pvars_lo() + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }  // debris velocity / spin w: a stack leftover
    check("debris_burst", 0x00263e30, 60, Ret::None, [level1, mobys_with, loadedcls] {
        level1(); const std::vector<uint32_t> es = mobys_with(OVF(0x002e0b88));
        const uint32_t m = es[static_cast<size_t>(ri(0, static_cast<int>(es.size()) - 1))];
        return Args{{m, coin() ? 0u : loadedcls(), u(ri(0, 1)), loadedcls(), u(ri(0, 1)), u(ri(0, 6)), R() & 1}, {}};
    }, [](A a) { debris_burst(a.i[0], static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]), static_cast<int32_t>(a.i[3]), static_cast<int32_t>(a.i[4]), static_cast<int32_t>(a.i[5]), a.i[6]); return uint64_t{0}; });
    g_ignored_words = saved_ignored_d;
    // the nanotech orbs (class 806) helpers, on a moby with its pvars made up
    auto nanomoby = [anymoby] {
        uint32_t m = anymoby();
        while (ee_read<uint32_t>(m + 0x78) == 0) m = anymoby();  // one with pvars
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        wfrand(pv, 4, -0.5f, 0.5f); ee_ref<Vec4>(pv + 0x10) = ee_ref<Vec4>(m + 0x10); wfrand(pv + 0x20, 4, -1, 1); wf(pv + 0x2c, rf(-3, 3));
        for (uint32_t i = 0; i < 8; ++i) { const uint32_t o = coin() ? alloc_particle(0x3c) : 0u; if (o) wf(o + 0x28, rf(0.2f, 1)); w32(pv + 0x30 + i * 4, o); }
        for (uint32_t i = 0; i < 8; ++i) { wfrand(OV(0x001e3ca0) + i * 16, 4, -1, 1); wfrand(OV(0x001e3d20) + i * 16, 4, -1, 1); }
        w16(pv + 0xa, R() & 1); wfrand(pv + 0x70, 3, -0.5f, 0.5f); wf(pv + 0x50, rf(0, 0.4f));
        for (uint32_t k = 0; k < 4; ++k) { w16(pv + 0x58 + k * 2, u(ri(0, 80))); wf(pv + 0x60 + k * 4, rf(0, 255)); }
        w8(m + 0x20, u(ri(0, 4)));
        return m;
    };
    check("nanotech_orb_pos", 0x002d8850, 100, Ret::None, [level1, nanomoby] { level1(); return Args{{nanomoby(), OUT, u(ri(0, 7))}, {}}; },
          [](A a) { nanotech_orb_pos_v(a.i[0], ee_ref<Vec4>(a.i[1]), static_cast<int32_t>(a.i[2])); return uint64_t{0}; });
    check("nanotech_orbs_spin", 0x002d8908, 40, Ret::None, [level1, nanomoby] { level1(); nanomoby(); return Args{{}, {}}; },
          [](A) { nanotech_orbs_spin(); return uint64_t{0}; });
    check("nanotech_sees_hero", 0x002d8980, 60, Ret::Int32, [level1, anymoby] { level1(); return Args{{anymoby()}, {}}; },
          [](A a) { return r32(static_cast<uint32_t>(nanotech_sees_hero(a.i[0]))); });
    check("nanotech_ground", 0x002d81b8, 150, Ret::None, [level1, nanomoby] {
        level1(); const uint32_t m = nanomoby(); wf(m + 0x18, ee_read<float>(m + 0x18) + rf(-0.5f, 1.5f));
        return Args{{m}, {}};
    }, [](A a) { nanotech_ground(a.i[0]); return uint64_t{0}; });
    check("nanotech_glints", 0x002d8418, 60, Ret::None, [level1, nanomoby] { level1(); return Args{{nanomoby()}, {}}; },
          [](A a) { nanotech_glints(a.i[0]); return uint64_t{0}; });
    check("nanotech_trail", 0x002d8510, 100, Ret::Int32, [level1, nanomoby] { level1(); return Args{{nanomoby()}, {}}; },
          [](A a) { return r32(static_cast<uint32_t>(nanotech_trail(a.i[0]))); });
    const std::vector<uint32_t> saved_ignored_n = g_ignored_words;
    for (uint32_t i = 0; i < 8; ++i) g_ignored_words.push_back(OV(0x001e3d2c) + i * 0x10);  // orbit axes' w: the cross product's VU register leftover
    for (int pass = 0; pass < 3; ++pass)
    check("nanotech_update", 0x002d89f0, 80, Ret::None, [level1, mobys_with, pass] {
        level1();
        const std::vector<uint32_t> crates = mobys_with(OVF(0x002cf218));
        const uint32_t crate = crates[static_cast<size_t>(ri(0, static_cast<int>(crates.size()) - 1))];
        if (ri(0, 2) == 0) w32(OV(0x0015f63c), 0);
        const uint32_t m = crate_spawn_companion(crate);
        if (pass > 0) {
            nanotech_update(m);
            for (int f = ri(0, 3); f > 0; --f) nanotech_update(m);
            w8(m + 0x20, u(ri(1, 4)));
            if (pass == 2) {  // near a hurt hero, or flying
                Vec4 h = ee_ref<Vec4>(0x0013f3d0); h.x += rf(-6, 6); h.y += rf(-6, 6); ee_ref<Vec4>(m + 0x10) = h;
                w32(0x0013f350 + 0x22a8, u(ri(0, 4))); w8(0x0015edb6, R() & 1); w32(0x0013f350 + 0x1c0, coin() ? 0u : 30u);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w16(pv + 0x56, R() & 1); w16(pv + 0x54, u(ri(0, 40)));
                for (uint32_t i = 0; i < 8; ++i) { const uint32_t o = ee_read<uint32_t>(pv + 0x30 + i * 4); if (o) { w16(o + 0x3c, 40); w16(o + 0x3e, u(ri(0, 40))); wfrand(o + 0x24, 3, -2, 2); } }
            }
        }
        return Args{{m}, {}};
    }, [](A a) { nanotech_update(a.i[0]); return uint64_t{0}; });
    g_ignored_words = saved_ignored_n;
    check("moby_hit_sparks", 0x0025c398, 60, Ret::None, [level1, colmoby] {
        level1(); const uint32_t m = colmoby(); wfrand(OUT, 4, -0.2f, 0.2f); w8(0x0013e530, R() & 1);
        return Args{{m, OUT, coin() ? 0xffffffffu : R()}, {}};
    }, [](A a) { moby_hit_sparks(a.i[0], ee_ref<Vec4>(a.i[1]), a.i[2]); return uint64_t{0}; });
    check("moby_floor_band", 0x0025a120, 100, Ret::None, [level1, anymoby] { level1(); const uint32_t m = anymoby(); if (coin()) wf(m + 0x18, rf(0, 20)); return Args{{m}, {}}; },
          [](A a) { moby_floor_band(a.i[0]); return uint64_t{0}; });
    // class 749 chain
    auto e749 = [mobys_with] { const std::vector<uint32_t> es = mobys_with(OVF(0x002d4610)); return es[static_cast<size_t>(ri(0, static_cast<int>(es.size()) - 1))]; };
    auto anypath = [] { uint32_t i = 0; while (ee_read<uint32_t>(OV(0x001b04b0) + 4 * (i = u(ri(0, 7)))) == 0) {} return i; };
    auto nearpath = [](uint32_t path, uint32_t out, float spread) {  // a point near one of the path's points
        const uint32_t pts = ee_read<uint32_t>(OV(0x001b04b0) + 4 * path);
        const uint32_t k = u(ri(0, static_cast<int>(ee_read<uint32_t>(pts)) - 1));
        for (uint32_t c = 0; c < 3; ++c) wf(out + 4 * c, ee_read<float>(pts + 0x10 + k * 0x10 + 4 * c) + rf(-spread, spread));
        wf(out + 0xc, rf(-1, 1));
    };
    check("vec3_length_sq_vu", 0x001ff430, 100, Ret::Float, [] { wfrand(OUT, 4, -20, 20); return Args{{OUT}, {}}; },
          [](A a) { return rf32(vec3_length_sq_vu(VV(a.i[0]))); });
    check("vec3_clamp_length_vu", 0x001ff598, 200, Ret::Int32, [] {
        wfrand(OUT, 4, -3, 3); wfrand(OUT + 0x10, 4, -3, 3); if (ri(0, 6) == 0) wfrand(OUT, 3, 0, 0);
        return Args{{coin() ? OUT : OUT + 0x10, OUT}, {ri(0, 4) == 0 ? 0.0f : rf(0, 6)}};
    }, [](A a) { return r32(static_cast<uint32_t>(vec3_clamp_length_vu(VV(a.i[0]), VV(a.i[1]), a.f[0]))); });
    check("angle_wrap_mod", 0x0025e310, 200, Ret::Float, [] { return Args{{}, {rf(-20, 20)}}; }, [](A a) { return rf32(angle_wrap_mod(a.f[0])); });
    {
        level1();
        const uint32_t m = mobys_with(OVF(0x002d4610))[0], tab = ee_read<uint32_t>(ee_read<uint32_t>(m + 0x24) + 0x2c);
        std::printf("class 749 hooks: +4 0x%x, +0xc 0x%x, +0x10 0x%x, +0x14 0x%x\n", ee_read<uint32_t>(tab + 4), ee_read<uint32_t>(tab + 0xc), ee_read<uint32_t>(tab + 0x10), ee_read<uint32_t>(tab + 0x14));
    }
    check("class_hook_4", 0x002da218, 40, Ret::Int32, [level1, e749] { level1(); wf(OUT, 1.5f); wf(OUT + 4, -2.25f); wf(OUT + 8, 0.5f); return Args{{e749(), OUT}, {}}; },
          [](A a) { return r32(static_cast<uint32_t>(class_hook_4(a.i[0], a.i[1]))); });  // (the class-749 hook +4 is ported: task 0330)
    {  // the hooks +0x10 of the level's other classes not ported yet
        level1();
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
            const uint32_t c = ee_read<uint32_t>(m + 0x24);
            if (c == 0 || ee_read<uint32_t>(c + 0x2c) == 0) continue;
            const uint32_t fn = ee_read<uint32_t>(ee_read<uint32_t>(c + 0x2c) + 0x10);
            if (fn != 0 && fn != OVF(0x002d5800) && fn != OVF(0x002db888) && fn != OVF(0x002caf40) && std::find(g_extra_noop_hooks.begin(), g_extra_noop_hooks.end(), fn) == g_extra_noop_hooks.end())
                g_extra_noop_hooks.push_back(fn);
        }
    }
    check("class_hook_10", 0x002db890, 60, Ret::Int32, [level1, e749, anymoby] {
        level1(); const uint32_t m = coin() ? e749() : anymoby();
        if (ri(0, 4) == 0) w8(m + 0x20, coin() ? 0xfeu : 0xfdu);
        return Args{{ri(0, 6) == 0 ? 0u : m}, {}};
    }, [](A a) { return r32(class_hook_10(a.i[0])); });
    g_extra_noop_hooks.clear();
    check("moby_chain_contact", 0x002599e8, 100, Ret::Int32, [level1, e749] {
        level1(); collide_vu_reset(); oracle_vu_contact_reset();
        return Args{{e749(), 0, u(ri(0, 3)), R() & 0xff, R() & 0xff, coin() ? 0xffffffffu : R()}, {rf(0.5f, 6), 0, rf(0, 2)}};
    }, [](A a) { return r32(static_cast<uint32_t>(moby_chain_contact(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), a.i[3], a.i[4], a.i[5], a.f[0], a.f[2]))); },
      0x70003718, 0x7000371c);  // (0x1f2868's spill of $t9 to the scratchpad: its upper half is a stale register)
    check("path_crossed", 0x00261968, 200, Ret::Int32, [level1, anypath, nearpath] {
        level1(); const uint32_t p = anypath(); nearpath(p, OUT, 6); nearpath(p, OUT + 0x10, 6);
        return Args{{p, OUT, OUT + 0x10}, {}};
    }, [](A a) { return r32(static_cast<uint32_t>(path_crossed(a.i[0], a.i[1], a.i[2]))); });
    check("path_push_out_2d", 0x00261d78, 200, Ret::Int32, [level1, anypath, nearpath] {
        level1(); const uint32_t p = anypath(); nearpath(p, OUT, 4); wrand(OUT + 0x10, 4);
        const uint32_t pts = ee_read<uint32_t>(OV(0x001b04b0) + 4 * p);
        if (coin()) wf(pts + 0x1c + 0x10 * u(ri(0, static_cast<int>(ee_read<uint32_t>(pts)) - 1)), 0.0f);
        return Args{{p, OUT, OUT + 0x10}, {rf(0.2f, 4)}};
    }, [](A a) { return r32(static_cast<uint32_t>(path_push_out_2d(a.i[0], a.i[1], a.i[2], a.f[0]))); });
    auto walkgen = [level1, e749, anymoby](bool fx) {
        level1(); collide_vu_reset(); oracle_vu_contact_reset();
        const uint32_t m = coin() ? e749() : anymoby();
        wfrand(OUT, 3, -0.4f, 0.4f); if (coin()) wfrand(OUT, 2, -2, 2); wf(OUT + 0xc, rf(-1, 1));
        if (fx) return Args{{m, OUT, u(ri(100, 1500)), R() & 0x1f}, {rf(0, 1), rf(0, 1)}};
        return Args{{m, OUT, R() & 0x1f}, {rf(0, 1), rf(0.1f, 1.5f), rf(0, 1)}};
    };
    check("walker_move", 0x00258830, 300, Ret::Int32, [walkgen] { return walkgen(false); },
          [](A a) { return r32(walker_move(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1], a.f[2])); });
    check("walker_move_fx", 0x00258ad0, 150, Ret::Int32, [walkgen] { return walkgen(true); },
          [](A a) { return r32(walker_move_fx(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), a.i[3], a.f[0], a.f[1])); });
    check("moby_list_hit", 0x0025a9f8, 100, Ret::None, [level1, e749, anymoby] {
        level1(); const int n = ri(0, 6);
        for (int i = 0; i < n; ++i) w32(T + 4 * u(i), coin() ? e749() : anymoby());
        const uint32_t m = e749(); ee_ref<Vec4>(OUT) = ee_ref<Vec4>(m + 0x10);
        for (int i = 0; i < 256; ++i) w8(ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(i) + 0xa4, ri(0, 3) == 0 ? 0xffu : u(ri(0, 63)));
        return Args{{m, OUT, T, u(n), n > 0 && coin() ? ee_read<uint32_t>(T) : 0u, R(), R(), R()}, {rf(0, 3), rf(0, 2), rf(-1, 1)}};
    }, [](A a) { moby_list_hit(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]), a.i[4], a.i[5], a.i[6], a.i[7], a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
    check("spawn_particle_12", 0x0026c3d0, 60, Ret::None, [level1] { level1(); wfrand(OUT, 4, -50, 150); wfrand(T, 12, -1, 1); seed_rand(); return Args{{OUT, T}, {}}; },
          [](A a) { spawn_particle_12(a.i[0], a.i[1]); return uint64_t{0}; });
    check("moby_33e_wake", 0x002d5830, 150, Ret::None, [level1, e749, anypath] {
        level1(); const uint32_t m = e749(), pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, coin() ? 1u : u(ri(0, 3)));
        if (ri(0, 3) == 0) w8(m + 0xb0, coin() ? 0xffu : u(ri(0, 15)));
        w32(0x0015ed84, u(ri(0, 0x13)));
        const uint32_t p = anypath(); w32(pv, ri(0, 4) == 0 ? 0xffffffffu : p);
        const uint32_t pts = ee_read<uint32_t>(OV(0x001b04b0) + 4 * p);
        if (coin()) wf(pv + 4, ee_read<float>(pts + 0x1c + 0x10 * u(ri(0, static_cast<int>(ee_read<uint32_t>(pts)) - 1))));
        return Args{{m}, {}};
    }, [](A a) { moby_33e_wake(a.i[0]); return uint64_t{0}; });
    check("debug_stub_1f6c28", 0x001f6c28, 10, Ret::None, [] { return Args{}; }, [](A) { debug_stub_1f6c28(); return uint64_t{0}; });
    check("debris_7a_spawn", 0x002af450, 60, Ret::Int32, [level1] {
        level1(); wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -1, 1); wf(0x0015ed6c, coin() ? 1.0f / 60 : rf(0, 0.1f));
        return Args{{OUT, OUT + 0x10, R() & 0xffff, coin() ? 0u : R() & 1, R() & 1}, {}};
    }, [](A a) { return r32(debris_7a_spawn(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4])); });
    check("fx_4a8_spawn", 0x002df808, 60, Ret::Int32, [level1] {
        level1(); wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -1, 1);
        return Args{{R(), OUT, OUT + 0x10, R(), R(), R(), R(), R()}, {rf(0, 3)}};
    }, [](A a) { return r32(fx_4a8_spawn(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5], a.i[6], a.i[7], a.f[0])); });
    check("e749_chase", 0x002d54c8, 300, Ret::Float, [level1, e749] {
        level1(); collide_vu_reset(); oracle_vu_contact_reset();
        const uint32_t m = e749(), pv = ee_read<uint32_t>(m + 0x78);
        Vec4 t = coin() ? ee_ref<Vec4>(0x0013f3d0) : ee_ref<Vec4>(m + 0x10);
        t.x += rf(-8, 8); t.y += rf(-8, 8); t.z += rf(-1, 1); ee_ref<Vec4>(OUT) = t;
        if (coin()) w8(m + 0x52, ee_read<uint8_t>(m + 0x53));
        wf(pv + 0x1f4, rf(-0.2f, 0.2f)); wf(pv + 0x1f8, rf(-0.1f, 0.4f)); wf(pv + 0x1fc, rf(-1, 1)); wf(pv + 0x270, rf(-0.3f, 0.1f));
        w8(m + 0x70, R() & 3);
        return Args{{m, OUT}, {rf(0, 10)}};
    }, [](A a) { return rf32(e749_chase(a.i[0], a.i[1], a.f[0])); });
    for (int pass = 0; pass < 2; ++pass)
    check("e749_hit_update", 0x002d5160, 200, Ret::None, [level1, e749, anypath, anymoby, pass] {
        level1();
        const uint32_t m = e749(), pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, u(ri(0, 12))); w32(OV(0x0015f5c4), coin() ? 2u : u(ri(0, 4)));
        w32(pv + 0x1e0, anypath()); w32(pv + 0x1e4, anypath());  // (set up by the moby's first update)
        w32(pv + 0x38, coin() ? 0u : 1u); w16(pv + 0x276, u(ri(0, 3))); w8(m + 0x53, u(ri(2, 5)));
        wf(pv + 0x20, rf(0, 10));
        if (pass == 1 || coin()) {  // a hit
            const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
            w8(m + 0xa4, idx); w32(e + 0x34, m);
            static const uint32_t kinds[] = {0x10000, 0x20000, 0x100000, NOT_ADDR(0x200000), 0x2000, 0x10};
            w32(e + 0x24, kinds[ri(0, 5)]); wf(e + 0x2c, coin() ? 0.0f : rf(0.5f, 3)); w32(e + 0x20, coin() ? 0u : anymoby());
            w32(e + 0x28, u(ri(0, 20)) | (u(ri(0, 3)) << 8) | (ri(0, 4) == 0 ? 0x470000u : 0u));
            wfrand(e + 0x10, 3, -1, 1); w32(e + 0x1c, coin() ? 0x45afdf66u : 0u);
        } else {
            w8(m + 0xa4, 0xff);
        }
        if (ri(0, 4) == 0) w32(0x0013f350 + 0x2080, 0);
        return Args{{m}, {}};
    }, [](A a) { e749_hit_update(a.i[0]); return uint64_t{0}; });
    auto blastcam = [](uint32_t m) {  // the blast flag and the camera near or far
        w8(0x0013e529, coin() ? 0u : u(ri(1, 3)));
        const float d = coin() ? rf(0, 10) : rf(5, 40);
        for (int k = 0; k < 3; ++k) wf(OV(0x00166dc0) + 4 * u(k), ee_read<float>(m + 0x10 + 4 * u(k)) + rf(-d, d) * 0.577f);
    };
    check("explosion_burst", 0x002dbf28, 100, Ret::None, [level1, e749, blastcam] {
        level1(); collide_vu_reset(); oracle_vu_contact_reset(); const uint32_t m = e749(); blastcam(m); return Args{{m}, {}};
    }, [](A a) { explosion_burst(a.i[0]); return uint64_t{0}; }, 0x70003718, 0x7000371c);  // (0x1f2868's stale $t9 half)
    check("explode_and_reset", 0x002dc938, 100, Ret::None, [level1, e749, blastcam] {
        level1(); collide_vu_reset(); oracle_vu_contact_reset(); const uint32_t m = e749(); blastcam(m); wrand(T, 0x30); wfrand(m + 0x40, 3, -3, 3);
        return Args{{m, T, R() & 1}, {}};
    }, [](A a) { explode_and_reset(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; }, 0x70003718, 0x7000371c);
    // (the class-749 hooks +4 / +0xc run on both sides: ported (carry_hooks.cpp), and since #145 carry_update calls the hook
    // +0xc through carry_hook_c; these checks had them as no-ops in the original, which hid that the port's call went nowhere)
    for (int pass = 0; pass < 2; ++pass)
    check("carry_update", 0x002dc9f0, 400, Ret::Int32, [level1, e749, anymoby, blastcam, pass] {
        level1(); collide_vu_reset(); oracle_vu_contact_reset();
        const uint32_t m = e749(), pv = ee_read<uint32_t>(m + 0x78), r = pv + 0xd0;
        constexpr uint32_t P = 0x0013f350;
        blastcam(m);
        if (ri(0, 20) == 0) w8(m + 0x20, coin() ? 0xfeu : 0xfdu);
        w16(r + 0x68, pass == 0 ? u(ri(0, 9)) : (coin() ? 5u : 6u));
        const uint32_t hero = ee_read<uint32_t>(P + 0x2080);
        w32(r + 0x60, coin() ? hero : e749()); wf(r + 0x64, rf(0.5f, 5));
        w16(r + 0x6a, u(ri(0, 3))); w32(P + 0x2050 + 4 * ee_read<uint16_t>(r + 0x6a), anymoby());
        const uint32_t cls = ee_read<uint32_t>(m + 0x24); const int sets = ee_read<uint8_t>(cls + 0xc);
        auto aset = [cls, sets] { uint32_t set = u(ri(0, sets - 1)); while (ee_read<uint32_t>(cls + 0x48 + set * 4) == 0) set = u(ri(0, sets - 1)); return set; };
        w8(T3 + 4, aset()); w8(T3 + 6, aset()); w32(r + 0x70, T3);
        w32(r + 0x74, ri(0, 2) == 0 ? 0u : (coin() ? hero : e749()));
        wf(r + 0x78, rf(0, 1)); wf(r + 0x7c, rf(0, 0.1f)); wf(r + 0x80, rf(0.3f, 1.5f)); w32(r + 0x88, R()); wf(r + 0x8c, rf(0.2f, 1));
        wf(r + 0x90, rf(0, 1)); w32(r + 0x94, u(ri(0, 400))); wf(r + 0x98, rf(0, 0.3f)); w16(r + 0x9e, coin() ? 0xffffu : u(ri(0, 2)));
        wf(r + 0xc, rf(-1, 2)); wf(r + 0x6c, rf(0, 1.2f));
        wfrand(r + 0x10, 3, -0.4f, 0.4f); if (ri(0, 5) == 0) wfrand(r + 0x10, 3, 0, 0);
        wfrand(r, 2, -5, 5); w32(r + 0x28, 0); for (int k = 0; k < 4; ++k) wf(r + 0x20 + 4 * u(k), rf(-1, 1));
        { Vec4& q = ee_ref<Vec4>(r + 0x20); const float l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w); q.x /= l; q.y /= l; q.z /= l; q.w /= l; }
        for (int k = 0; k < 2; ++k) wf(r + k * 4, ee_read<float>(m + 0x10 + 4 * u(k)) + rf(-5, 5));
        ee_ref<Vec4>(r + 0x40) = ee_ref<Vec4>(ee_read<uint32_t>(r + 0x74) != 0 ? ee_read<uint32_t>(r + 0x74) + 0x10 : m + 0x10);
        if (ri(0, 6) == 0) wf(m + 0x18, rf(0, 2.5f));
        if (coin()) { Vec4 q = ee_ref<Vec4>(0x0013f3d0); q.x += rf(-3, 3); q.y += rf(-3, 3); q.z += rf(0, 3); ee_ref<Vec4>(m + 0x10) = q; }
        wf(m + 0x48, rf(-3.1f, 3.1f)); w8(m + 0x70, R() & 3);
        return Args{{m, pv + 0x70}, {}};
    }, [](A a) { return r32(static_cast<uint32_t>(carry_update(a.i[0], a.i[1]))); }, 0x70003718, 0x7000371c);
    g_extra_noop_hooks.clear();
    for (int pass = 0; pass < 3; ++pass)
    check("e749_update", 0x002d4610, 300, Ret::None, [level1, e749, anymoby, anypath, blastcam, pass] {
        level1(); collide_vu_reset(); oracle_vu_contact_reset();
        const uint32_t m = e749(), pv = ee_read<uint32_t>(m + 0x78), r = pv + 0xd0;
        constexpr uint32_t P = 0x0013f350;
        blastcam(m);
        if (pass == 0) return Args{{m}, {}};  // as the level placed it
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        const uint32_t st = pass == 2 ? u(ri(0, 12)) : (coin() ? 0u : u(ri(1, 12)));
        w32(pv + 0x1e0, anypath());  // the patrol paths: -1 only before the set-up (states 2..4 read them)
        w32(pv + 0x1e4, st == 0 && coin() ? 0xffffffffu : anypath()); w32(pv + 0x1e8, st == 0 && coin() ? 0xffffffffu : anypath());
        w32(pv + 0x1ec, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 290))); w32(pv + 0x1c4, u(ri(0, 3)));
        { const uint32_t pts = ee_read<uint32_t>(OV(0x001b04b0) + 4 * (ee_read<int32_t>(pv + 0x1e8) >= 0 ? ee_read<uint32_t>(pv + 0x1e8) : ee_read<uint32_t>(pv + 0x1e0)));
          w16(pv + 0x274, u(ri(0, static_cast<int>(ee_read<uint32_t>(pts)) - 1))); }
        w16(pv + 0x276, u(ri(0, 2))); w32(pv + 0x38, R() & 1);
        if (ri(0, 3) == 0) {  // a moby to wake nearby: none of class 0x33e on Veldin, another class-749 one stands in
            const uint32_t o = e749(), po = ee_read<uint32_t>(o + 0x78); w32(pv + 0x1f0, (o - first) >> 8);
            Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-1.2f, 1.2f); q.y += rf(-1.2f, 1.2f); ee_ref<Vec4>(o + 0x10) = q;
            if (coin()) w16(o + 0xa6, 0x33e);
            w8(o + 0x20, coin() ? 1u : 3u); w32(po, coin() ? 0xffffffffu : anypath());
        } else {
            w32(pv + 0x1f0, 0xffffffffu);
        }
        const uint32_t tgt = coin() ? ee_read<uint32_t>(P + 0x2080) : e749();
        w32(pv + 0x1c0, ri(0, 5) == 0 ? 0u : tgt);
        { Vec4 q = ee_ref<Vec4>(tgt + 0x10); ee_ref<Vec4>(pv + 0x180) = q; if (coin()) { q.x += rf(-3, 3); q.y += rf(-3, 3); ee_ref<Vec4>(m + 0x10) = q; } }
        const uint32_t cls = ee_read<uint32_t>(m + 0x24); const int sets = ee_read<uint8_t>(cls + 0xc);
        auto aset = [cls, sets] { uint32_t set = u(ri(0, sets - 1)); while (ee_read<uint32_t>(cls + 0x48 + set * 4) == 0) set = u(ri(0, sets - 1)); return set; };
        if (coin()) w8(m + 0x53, aset());
        w8(m + 0x70, R() & 3); wf(m + 0x48, rf(-3.1f, 3.1f)); wf(m + 0x58, rf(0, 40)); wf(m + 0x5c, rf(0, 1));
        wf(pv + 0x1f4, rf(-0.2f, 0.2f)); wf(pv + 0x1f8, rf(0, 0.3f)); wf(pv + 0x1fc, rf(-0.3f, 0.3f));
        // the knock-back record (+0x70) and the carry record (+0xd0)
        wfrand(pv + 0x70, 3, -0.2f, 0.2f); wf(pv + 0x7c, 0); wf(pv + 0x80, rf(0, 0.03f)); wf(pv + 0x84, rf(0, 0.05f)); wf(pv + 0x8c, rf(0, 1));
        w32(pv + 0x90, u(ri(0, 1500))); w32(pv + 0x94, R() & 0x3f); wf(pv + 0x98, rf(0, 1.5f));
        w16(r + 0x68, u(ri(0, 8))); w8(T3 + 4, aset()); w8(T3 + 6, aset()); w32(r + 0x70, T3); w32(r + 0x74, 0); wf(r + 0x80, rf(0.3f, 1.5f));
        wf(r + 0xc, rf(-1, 2)); wf(r + 0x6c, rf(0, 1.2f)); wfrand(r + 0x10, 3, -0.4f, 0.4f); w32(r + 0x60, ee_read<uint32_t>(P + 0x2080)); wf(r + 0x64, rf(0.5f, 5));
        w16(r + 0x6a, 0); w32(P + 0x2050, anymoby()); w16(r + 0x9e, 0xffff); w32(r + 0x94, u(ri(0, 400)));
        { Vec4& q = ee_ref<Vec4>(r + 0x20); q = Vec4{0, 0, 0, 1}; }
        w8(m + 0x20, st);
        return Args{{m}, {}};
    }, [](A a) { e749_update(a.i[0]); return uint64_t{0}; }, 0x70003718, 0x7000371c);
    g_extra_noop_hooks.clear();
    check("floor_probe_20", 0x00259710, 100, Ret::Float, [level1, anymoby] { level1(); ee_ref<Vec4>(OUT) = ee_ref<Vec4>(anymoby() + 0x10); wf(OUT + 8, ee_read<float>(OUT + 8) + rf(-2, 4)); return Args{{OUT}, {}}; },
          [](A a) { return rf32(floor_probe_20(a.i[0])); });
    check("yaw_turn_damped", 0x00258278, 200, Ret::None, [level1, anymoby] { level1(); const uint32_t m = anymoby(); wf(m + 0x48, rf(-3.1f, 3.1f)); wf(OUT, rf(-0.3f, 0.3f));
        return Args{{m, OUT}, {rf(-3.1f, 3.1f), rf(0, 0.1f), rf(0, 0.5f), coin() ? 0.0f : rf(0, 0.2f)}}; },
          [](A a) { yaw_turn_damped(a.i[0], a.i[1], a.f[0], a.f[1], a.f[2], a.f[3]); return uint64_t{0}; });
    check("yaw_seek", 0x0025be00, 300, Ret::Float, [] { wf(OUT, rf(-3.1f, 3.1f)); wf(OUT + 4, coin() ? 0.0f : rf(-0.3f, 0.3f));
        return Args{{OUT, OUT + 4}, {rf(-3.1f, 3.1f), rf(0.001f, 0.05f), rf(0.001f, 0.05f), rf(0.01f, 0.3f)}}; },
          [](A a) { return rf32(yaw_seek(a.i[0], a.i[1], a.f[0], a.f[1], a.f[2], a.f[3])); });
    check("hud_message_show", 0x00263db0, 60, Ret::None, [level1] {
        level1();
        const uint32_t w = u(ri(0, 0x24));
        return Args{{u(ee_read<int16_t>(OV(0x00179ac0) + w * 0x4c + (coin() ? 0x34 : 0x36))), u(ri(0, 999)), coin() ? 0xffffffffu : u(ri(1, 300))}, {}};
    }, [](A a) { hud_message_show(static_cast<int32_t>(a.i[0]), a.i[1], static_cast<int32_t>(a.i[2])); return uint64_t{0}; });
    check("ammo_add", 0x00233e40, 100, Ret::Int32, [level1] {
        level1();
        const uint32_t w = u(ri(0, 0x24));
        w32(0x0013d428 + w * 4, u(ri(0, 60)));
        return Args{{w, u(ri(-5, 60))}, {}};
    }, [](A a) { return r32(static_cast<uint32_t>(ammo_add(a.i[0], static_cast<int32_t>(a.i[1])))); });
    check("pickup_collect", 0x002c5be8, 100, Ret::Int32, [level1, anymoby] {
        level1();
        wfrand(OUT, 4, 100, 140);
        auto loaded = [](uint32_t it) {  // the item's pickup class has its data on this level
            const int16_t c = ee_read<int16_t>(OV(0x00179ac0) + it * 0x4c + 0x3a);
            return c > 0 && ee_read<uint32_t>(OV(0x00197300) + 4u * ee_read<uint8_t>(OV(0x00197bc0) + u(c))) != 0;
        };
        uint32_t item = 2;
        for (int k = 0; k < 50; ++k) { const uint32_t it = u(ri(1, 0x24)); if (loaded(it)) { item = it; break; } }
        const uint32_t m = pickup_spawn(OUT, item, coin() ? -1 : ri(1, 30), 0);
        if (m != 0 && ri(0, 4) == 0) w8(m + 0x20, 2);
        w32(0x0013d428 + item * 4, u(ri(0, 60))); w8(0x0013d4c0 + item, R() & 1);
        w32(OV(0x0015f5cc), u(ri(0, 100))); w32(OV(0x00161838), u(ri(0, 100)));
        return Args{{m != 0 ? m : anymoby()}, {}};
    }, [](A a) { return r32(static_cast<uint32_t>(pickup_collect(a.i[0]))); });
    check("moby_is_pickup", 0x0025e3f8, 100, Ret::Int32, [level1, anymoby] {
        level1(); const uint32_t m = anymoby();
        static const uint16_t cs[] = {0xe2, 0xcc, 0xde, 0x3ee, 0xd6, 0xe1, 0xd5, 0xdf, 0xd4, 0x1f4};
        if (coin()) w16(m + 0xa6, cs[ri(0, 9)]);
        return Args{{m}, {}};
    }, [](A a) { return r32(static_cast<uint32_t>(moby_is_pickup(a.i[0]))); });
    check("wrench_pull_pickups", 0x002a82c0, 60, Ret::None, [level1, anymoby] {
        level1();
        const uint32_t hero = ee_read<uint32_t>(0x001413d0);
        Vec4 at = ee_ref<Vec4>(hero + 0x10);
        for (int k = 0; k < 6; ++k) {
            Vec4 q = at; q.x += rf(-3, 3); q.y += rf(-3, 3); ee_ref<Vec4>(OUT + 0x20) = q; wfrand(OUT + 0x30, 4, -0.1f, 0.1f);
            if (coin()) bolt_spawn(anymoby(), OUT + 0x20, OUT + 0x30, 0, 1, 0);
            else { const uint32_t it = u(ri(1, 0x24)); const int16_t c = ee_read<int16_t>(OV(0x00179ac0) + it * 0x4c + 0x3a);
                   if (c > 0 && ee_read<uint32_t>(OV(0x00197300) + 4u * ee_read<uint8_t>(OV(0x00197bc0) + u(c))) != 0) pickup_spawn(OUT + 0x20, it, -1, 0); }
        }
        const uint32_t m = anymoby(); ee_ref<Vec4>(m + 0x10) = at;
        return Args{{m}, {}};
    }, [](A a) { wrench_pull_pickups(a.i[0]); return uint64_t{0}; });
    check("spawn_particle_13", 0x0026c6e8, 60, Ret::None, [level1] { level1(); wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -1, 1); return Args{{OUT, OUT + 0x10}, {}}; },
          [](A a) { spawn_particle_13_v(ee_ref<Vec4>(a.i[0]), ee_ref<Vec4>(a.i[1])); return uint64_t{0}; });
    check("spawn_particle_15", 0x0026cbb0, 60, Ret::Int32, [level1] { level1(); wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -1, 1);
        return Args{{OUT, OUT + 0x10, R(), R(), coin() ? 0u : u(ri(1, 30)), R() & 0xffff}, {rf(0, 9)}}; },
          [](A a) { return r32(spawn_particle_15_v(ee_ref<Vec4>(a.i[0]), ee_ref<Vec4>(a.i[1]), a.i[2], a.i[3], a.i[4], a.i[5], a.f[0])); });
    check("wrench_hit_burst", 0x002a8028, 40, Ret::None, [level1, anymoby] {
        level1(); wfrand(OV(0x00173e60), 4, 100, 140); wfrand(OV(0x00166dc0), 4, 100, 140);
        const uint32_t m = anymoby(); if (coin()) w16(ee_read<uint32_t>(m + 0x78) + 0x78, 0x2b);
        return Args{{m}, {}};
    }, [](A a) { wrench_hit_burst(a.i[0]); return uint64_t{0}; });
    check("hit_moby_soft", 0x002a7d90, 60, Ret::Int32, [level1, anymoby] { level1(); w32(OV(0x00173e58), coin() ? 0u : anymoby()); return Args{}; },
          [](A) { return r32(static_cast<uint32_t>(hit_moby_soft())); });
    check("hit_moby_flag2", 0x002a7de0, 60, Ret::Int32, [level1, anymoby] { level1(); w32(OV(0x00173e58), coin() ? 0u : anymoby()); return Args{}; },
          [](A) { return r32(static_cast<uint32_t>(hit_moby_flag2())); });
    check("moby_not_crate_or_1f6", 0x0025e368, 60, Ret::Int32, [level1, anymoby] { level1(); const uint32_t m = coin() ? 0u : anymoby(); if (m && coin()) w16(m + 0xa6, coin() ? 0x1f6u : 0x1f4u); return Args{{m}, {}}; },
          [](A a) { return r32(static_cast<uint32_t>(moby_not_crate_or_1f6(a.i[0]))); });
    check("moby_rotate_by", 0x00262480, 60, Ret::None, [level1, anymoby] { level1(); const uint32_t m = anymoby(); wfrand(m + 0x40, 3, -3, 3); return Args{{m}, {rf(-3, 3), rf(-3, 3), rf(-3, 3)}}; },
          [](A a) { moby_rotate_by(a.i[0], a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
    check("swept_collide", 0x00259d08, 60, Ret::Int32, [level1, anymoby] {
        level1();
        const uint32_t hero = ee_read<uint32_t>(0x001413d0);
        for (int k = 0; k < 4; ++k) { Vec4 q = ee_ref<Vec4>(hero + 0x10); q.x += rf(-2, 2); q.y += rf(-2, 2); q.z += rf(-2, 3); ee_ref<Vec4>(OUT + 0x10 * u(k)) = q; }
        return Args{{OUT, OUT + 0x10, OUT + 0x20, OUT + 0x30, coin() ? hero : 0u, 0, u(ri(0, 6))}, {}};
    }, [](A a) { return r32(static_cast<uint32_t>(swept_collide(ee_ref<Vec4>(a.i[0]), ee_ref<Vec4>(a.i[1]), ee_ref<Vec4>(a.i[2]), ee_ref<Vec4>(a.i[3]), a.i[4], nullptr, static_cast<int32_t>(a.i[6])))); });  // (rec 0)
    check("throw_turn_scale", 0x002a8418, 60, Ret::None, [level1] { level1(); wfrand(OUT, 4, -3, 3); wf(0x0013f3e8, rf(-3, 3)); return Args{{OUT}, {}}; },
          [](A a) { throw_turn_scale(a.i[0]); return uint64_t{0}; });
    check("wrench_follow_anim", 0x002a7c70, 60, Ret::None, [level1, anymoby] {
        level1(); const uint32_t hero = ee_read<uint32_t>(0x001413d0);
        w32(0x0013f350 + 0x2084, coin() ? 0x3bu : 0u); w32(0x0013f350 + 0x2090, coin() ? 0x3bu : 0u); w32(0x0013f350 + 0x198, R() & 1);
        static const uint8_t an[] = {0x40, 0x41, 0x10};
        w8(hero + 0x53, an[ri(0, 2)]); w8(hero + 0x52, coin() ? ee_read<uint8_t>(hero + 0x53) : 0x40u);
        uint32_t m = hero;  // a moby with the animation sets 1, 0xb, 0x12, 0x13
        for (int k = 0; k < 200; ++k) {
            const uint32_t c = ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(0, 290)), cls = ee_read<uint32_t>(c + 0x24);
            if (cls == 0 || cls >= 0x02000000u || ee_read<uint8_t>(cls) <= 0x13) continue;
            bool ok = true;
            for (uint32_t st : {1u, 0xbu, 0x12u, 0x13u}) { const uint32_t sp = ee_read<uint32_t>(cls + 0x48 + st * 4); ok = ok && sp != 0 && sp < 0x02000000u; }
            if (ok) { m = c; break; }
        }
        return Args{{m}, {}};
    }, [](A a) { wrench_follow_anim(a.i[0]); return uint64_t{0}; });
    check("cam_effect_add", 0x001edf60, 100, Ret::None, [level1] {
        level1(); w32(OV(0x00169100), u(ri(0, 3))); w32(OV(0x0015f5c4), ri(0, 5) == 0 ? 2u : 0u); wfrand(OUT, 4, -9, 9);
        return Args{{R(), R(), coin() ? OUT : 0u, u(ri(0, 0x2a)), coin() ? 0xffffffffu : u(ri(0, 4))}, {rf(-3, 3), rf(-3, 3), rf(-3, 3)}};
    }, [](A a) { cam_effect_add(a.i[0], a.i[1], a.i[2] != 0 ? &ee_ref<Vec4>(a.i[2]) : nullptr, a.i[3], static_cast<int32_t>(a.i[4]), a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
    for (int st = 0; st < 5; ++st)
    check("bolt_update", 0x002a5dd8, 120, Ret::None, [level1, st] {
        level1();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        const uint32_t hero = ee_read<uint32_t>(0x001413d0);
        Vec4 at = ee_ref<Vec4>(hero + 0x10);
        at.x += rf(-4, 4); at.y += rf(-4, 4); at.z += rf(-1, 2.5f);
        ee_ref<Vec4>(OUT) = at; wfrand(OUT + 0x10, 4, -0.2f, 0.2f);
        static const int vals[] = {1, 5, 20, 50};
        const uint32_t b = bolt_spawn(first + 0x100 * u(ri(0, 290)), OUT, OUT + 0x10, 0, vals[ri(0, 3)], R() & 1);
        const uint32_t p = ee_read<uint32_t>(b + 0x78);
        w8(b + 0x20, u(st));
        ee_ref<Vec4>(p) = at; wfrand(p + 0x10, 4, -0.3f, 0.3f); wfrand(p + 0x20, 3, -0.3f, 0.3f);
        w8(p + 0x54, R() & 1); w8(p + 0x56, u(ri(5, 40))); w8(p + 0x57, u(ri(st == 2 ? 1 : 0, 5)));  // (state 2 never runs with 0 left)
        wf(p + 0x68, at.z + rf(-3, 3)); w16(p + 0x6c, coin() ? 0xffffu : u(ri(0, 15))); w16(p + 0x6e, u(ri(0, 3)));
        w32(p + 0x5c, 0); w32(p + 0x60, coin() ? hero : first + 0x100 * u(ri(0, 290)));
        wf(p + 0x64, rf(0, 1)); wf(p + 0x48, coin() ? 0.0f : rf(-1, 1)); wf(p + 0x4c, coin() ? 0.0f : rf(-1, 1));
        w32(OV(0x0015f5c4), ri(0, 5) == 0 ? 2u : 0u); w8(b + 0x31, R() & 1); w32(OV(0x0015ffbc), u(ri(150, 250)));
        w32(0x001415f8, R() & 1);
        return Args{{b}, {}};
    }, [](A a) { bolt_update(a.i[0]); return uint64_t{0}; });
    check("bolt_spin_place", 0x002a6de8, 150, Ret::None, [level1] {
        level1();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -1, 1);
        static const int vals[] = {1, 5, 20, 50};
        const uint32_t b = bolt_spawn(first + 0x100 * u(ri(0, 290)), OUT, OUT + 0x10, 0, vals[ri(0, 3)], R() & 1);
        const uint32_t p = ee_read<uint32_t>(b + 0x78);
        wfrand(p, 4, 100, 140); wfrand(p + 0x10, 4, -3, 3); wfrand(p + 0x50, 4, -3, 3); w8(p + 0x55, R() & 1); w8(p + 0x57, ri(0, 4) == 0 ? 1u : 0u);
        const uint32_t on = first + 0x100 * u(ri(0, 290));
        w32(p + 0x5c, coin() ? on : 0u);
        if (coin()) w8(on + 0x20, coin() ? 0xfeu : 0xfdu);
        w8(b + 0x20, coin() ? 3u : u(ri(0, 4)));
        return Args{{b, coin() ? OUT + 0x20 : 0u, coin() ? OUT + 0x30 : 0u}, {}};
    }, [](A a) { bolt_spin_place(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
    check("bolt_collect", 0x002a6b70, 100, Ret::None, [level1] {
        level1();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -1, 1);
        static const int vals[] = {1, 5, 20, 50};
        const uint32_t src = first + 0x100 * u(ri(0, 290));
        if (coin()) w8(src + 0xb1, coin() ? 0x80u : u(ri(0, 8)));
        const uint32_t b = bolt_spawn(src, OUT, OUT + 0x10, 0, vals[ri(0, 3)], R() & 1);
        w32(OV(0x0015f598), R() & 1); w32(0x0015ed98, u(ri(0, 5000))); w32(0x0015ee2c, u(ri(0, 100)));
        return Args{{b}, {}};
    }, [](A a) { bolt_collect(a.i[0]); return uint64_t{0}; });
    check("crate_stack_update", 0x002d1428, 200, Ret::None, [level1, mobys_with] {
        level1();
        const std::vector<uint32_t> crates = mobys_with(OVF(0x002cf218));
        auto pick = [&] { return crates[static_cast<size_t>(ri(0, static_cast<int>(crates.size()) - 1))]; };
        std::vector<uint32_t> chain;
        const int n = ri(1, 3);
        for (int k = 0; k < 20 && static_cast<int>(chain.size()) < n; ++k) { const uint32_t c = pick(); if (std::find(chain.begin(), chain.end(), c) == chain.end()) chain.push_back(c); }
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        const uint32_t plat = coin() ? 0u : first + 0x100 * u(ri(0, 290));
        for (size_t k = 0; k < chain.size(); ++k) {
            const uint32_t c = chain[k], pv = ee_read<uint32_t>(c + 0x78);
            w32(pv + 0xa4, k == 0 ? 0u : chain[k - 1]);   // the one below
            w32(pv + 0xa0, k + 1 < chain.size() ? chain[k + 1] : 0u);
            if (k > 0) { wf(c + 0x10, ee_read<float>(chain[0] + 0x10)); wf(c + 0x14, ee_read<float>(chain[0] + 0x14)); wf(c + 0x18, ee_read<float>(chain[k - 1] + 0x18) + rf(0.5f, 1.5f)); }
            w32(pv + 0xb4, ri(0, 6) == 0 ? ee_read<uint32_t>(OV(0x0015f5cc)) : 0u);
            w32(pv + 0xac, R() & (ri(0, 4) == 0 ? 0x16u : 0x14u));
            w8(c + 0x20, ri(0, 5) == 0 ? 0u : (coin() ? 1u : 5u));
            wf(pv + 0x40, rf(-0.1f, 0.1f)); wf(pv + 0x44, rf(-0.1f, 0.1f)); wf(pv + 0x48, rf(-0.4f, 0.4f)); wf(pv + 0x4c, rf(-1, 1));
            wf(pv + 0xb0, rf(0.5f, 1.5f)); wf(pv + 0xf4, rf(0, 2));
            w32(pv + 0xf0, plat);
            const bool nitro_chunks = ee_read<uint8_t>(OV(0x00197bc0) + 0x16b) != 0xff && ee_read<uint8_t>(OV(0x00197bc0) + 0x16c) != 0xff;
            if (ri(0, 3) == 0 && nitro_chunks) w16(c + 0xa6, 0x1f9);  // (only with its chunk classes loaded)
        }
        static bool lanes = false;  // the bolts' direction w (pvars +0x2c): an unset stack lane in the original
        if (!lanes) {
            for (uint32_t i = 0; i < 0x400; ++i) g_ignored_words.push_back(ee_read<uint32_t>(OV(0x0015ffe8)) + i * 0x80 + 0x2c);
            lanes = true;
        }
        if (coin()) { const uint32_t c = chain[static_cast<size_t>(ri(0, static_cast<int>(chain.size()) - 1))]; wf(0x0013f3d0, ee_read<float>(c + 0x10) + rf(-0.6f, 0.6f)); wf(0x0013f3d4, ee_read<float>(c + 0x14) + rf(-0.6f, 0.6f)); wf(0x0013f3d8, ee_read<float>(c + 0x18) + rf(0, 1.5f)); }
        return Args{{chain[static_cast<size_t>(ri(0, static_cast<int>(chain.size()) - 1))]}, {}};
    }, [](A a) { crate_stack_update(a.i[0]); return uint64_t{0}; });
    check("crate_shatter", 0x002d09b8, 150, Ret::None, [level1, mobys_with] {
        level1();
        const std::vector<uint32_t> crates = mobys_with(OVF(0x002cf218));
        auto pick = [&] { return crates[static_cast<size_t>(ri(0, static_cast<int>(crates.size()) - 1))]; };
        const uint32_t m = pick();
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w32(pv + 0xa0, 0); w32(pv + 0xa4, 0);
        if (coin()) { const uint32_t c = pick(); if (c != m) { w32(pv + 0xa0, c); } }
        if (coin()) { const uint32_t c = pick(); if (c != m) { w32(pv + 0xa4, c); w32(ee_read<uint32_t>(c + 0x78) + 0xa0, m); } }
        w32(OV(0x0015f5cc), u(ri(100, 200))); w32(OV(0x001618d4), u(ri(95, 205))); w32(OV(0x001618d8), u(ri(95, 205)));
        w32(OV(0x0015ffbc), u(ri(0, 260))); wf(OV(0x0015f5d0), rf(0.8f, 1.0f)); w32(0x001413dc, coin() ? 0x16u : 0u);
        const float d = rf(0, 14), a = rf(-3.1f, 3.1f);
        wf(OV(0x00166dc0), ee_read<float>(m + 0x10) + d * std::cos(a)); wf(OV(0x00166dc4), ee_read<float>(m + 0x14) + d * std::sin(a)); wf(OV(0x00166dc8), ee_read<float>(m + 0x18) + rf(-2, 2));
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        return Args{{m, coin() ? 1u : 0u, coin() ? 0u : first + 0x100 * u(ri(0, 290))}, {}};
    }, [](A a) { crate_shatter(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
    check("rand_sphere_point", 0x00257d78, 100, Ret::None, [level1] { level1(); wfrand(OUT, 4, -9, 9); return Args{{OUT}, {rf(0, 2), rf(2, 5)}}; },
          [](A a) { rand_sphere_point(a.i[0], a.f[0], a.f[1]); return uint64_t{0}; });
    check("spawn_particle_0b", 0x0026a9f0, 100, Ret::None, [level1] {
        level1();
        wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -1, 1);
        wf(OV(0x0015f5d0), rf(0.7f, 1.1f)); wf(OV(0x0015f5d4), rf(0.7f, 1.1f));
        return Args{{OUT, OUT + 0x10, R(), R(), ri(0, 5) == 0 ? 0u : u(ri(1, 40)), R() & 0xff, R() & 0xff, R() & 0xff}, {rf(-2, 2), rf(0, 2)}};
    }, [](A a) { spawn_particle_0b(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5], a.i[6], a.i[7], a.f[0], a.f[1]); return uint64_t{0}; });
    check("spawn_particle_0d", 0x0026b790, 100, Ret::Int32, [level1] {
        level1();
        wfrand(OUT, 4, 100, 140);
        return Args{{OUT, u(ri(-500, 500)), R()}, {rf(0, 2), rf(0, 1), rf(1, 2), rf(-1, 1), rf(0, 3)}};
    }, [](A a) { return r32(spawn_particle_0d(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2], a.f[0], a.f[1], a.f[2], a.f[3], a.f[4])); });
    check("moby_sound_play", 0x0028dc90, 100, Ret::Int32, [level1] {
        level1();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        const uint32_t m = first + 0x100 * u(ri(0, 290));
        return Args{{u(ri(0, 6)), R() & 0xff, m, ee_read<uint16_t>(m + 0xa6)}, {}};
    }, [](A a) { return r32(static_cast<uint32_t>(moby_sound_play(static_cast<int32_t>(a.i[0]), a.i[1], a.i[2], a.i[3]))); });
    check("shard_spawn", 0x002ac910, 100, Ret::Int32, [level1] {
        level1();
        wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -1, 1);
        return Args{{R(), OUT, OUT + 0x10, u(ri(5, 60)), R(), R(), R(), R()}, {rf(0.2f, 2)}};
    }, [](A a) { return r32(shard_spawn(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5], a.i[6], a.i[7], a.f[0])); });
    check("shard_update", 0x002acad8, 150, Ret::None, [level1] {
        level1();
        wfrand(OUT, 4, 100, 140); wfrand(OUT + 0x10, 4, -1, 1);
        const uint32_t m = shard_spawn(R(), OUT, OUT + 0x10, u(ri(5, 60)), R(), R(), R(), R(), rf(0.2f, 2));
        const uint32_t p = ee_read<uint32_t>(m + 0x78);
        if (coin()) w16(p + 0x1c, u(ri(0, 3)));
        else if (coin()) w16(p + 0x1c, u(ri(0, 60)));
        return Args{{m}, {}};
    }, [](A a) { shard_update(a.i[0]); return uint64_t{0}; });
    check("chunk_spawn", 0x002af8b0, 100, Ret::Int32, [level1] {
        level1();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        wfrand(OUT, 4, -1, 1); wfrand(OUT + 0x10, 4, 100, 140); wfrand(OUT + 0x20, 4, -3, 3);
        uint32_t cm = first + 0x100 * u(ri(0, 290));
        while (ee_read<uint32_t>(cm + 0x24) == 0) cm = first + 0x100 * u(ri(0, 290));  // a class with data
        const uint32_t cls = ee_read<uint16_t>(cm + 0xa6);
        return Args{{first + 0x100 * u(ri(0, 290)), OUT, OUT + 0x10, OUT + 0x20, cls, R() & 0xffff}, {rf(0.2f, 1), rf(1, 2)}};
    }, [](A a) { return r32(chunk_spawn(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5], a.f[0], a.f[1])); });
    check("chunk_update", 0x002afa48, 300, Ret::None, [level1] {
        level1();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        const uint32_t hero = ee_read<uint32_t>(0x001413d0);
        Vec4 at = ee_ref<Vec4>(hero + 0x10);
        at.x += rf(-3, 3); at.y += rf(-3, 3); at.z += rf(-0.5f, 1.5f);
        ee_ref<Vec4>(OUT + 0x10) = at;
        wfrand(OUT, 4, -0.2f, 0.2f); wfrand(OUT + 0x20, 4, -3, 3);
        static const uint32_t cls[] = {0x95, 0x15c, 0x15d, 0x162, 0x163};
        const uint32_t m = chunk_spawn(first + 0x100 * u(ri(0, 290)), OUT, OUT + 0x10, OUT + 0x20, cls[ri(0, 4)], R() & 1, rf(0.2f, 1), rf(1, 2));
        const uint32_t p = ee_read<uint32_t>(m + 0x78);
        if (ri(0, 3) == 0) w8(m + 0x20, 1);
        if (coin()) w16(p + 0xe, u(ri(0, 2)));
        if (coin()) w16(p + 0xc, u(ri(0, 3)));
        if (coin()) wf(p + 0x18, rf(-0.4f, 0.1f));
        if (ri(0, 4) == 0) w8(m + 0x23, u(ri(0, 20)));
        return Args{{m}, {}};
    }, [](A a) { chunk_update(a.i[0]); return uint64_t{0}; });
    classcheck("moby_1564_update", 0x002e3a88, moby_1564_update, [](uint32_t m) {
        w8(m + 0x20, u(ri(0, 2))); w8(m + 0x70, R() & 3); w8(m + 0x53, u(ri(0, 2)));
        const uint32_t p = ee_read<uint32_t>(m + 0x78);
        if (ri(0, 2) == 0) wf(p + 0x6c, rf(0, 12));
        wf(p + 0x64, rf(-0.05f, 0.05f));
    });
    classcheck("moby_1413_update", 0x002e0988, moby_1413_update, [](uint32_t m) {
        w8(m + 0x20, u(ri(0, 1)));
        for (uint32_t a : {0x00141980u, 0x00141d08u, 0x001419b0u, 0x00141850u, 0x001419a8u, 0x00141848u, 0x00141968u, 0x00141970u, 0x00141978u})
            w16(a, ri(0, 2) == 0 ? 1u : 0u);
        w32(0x001415f8, u(ri(0, 8)));
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        if (coin()) { const uint32_t o = ee_read<uint32_t>(ee_read<uint32_t>(m + 0x78) + 0x14 + 4 * u(ri(0, 3))); if (o != 0xffffffffu) w8(first + (o << 8) + 0x20, coin() ? 0xfeu : 0xfdu); }
        const uint32_t mi = first + 0x100 * u(ri(0, 200));  // the hero in one of the boxes, or anywhere near the level
        for (int k = 0; k < 3; ++k) wf(0x0013f3d0 + 4 * u(k), ee_read<float>(mi + 0x10 + 4 * u(k)));
    });
    check("zones_update", 0x001f77f8, 200, Ret::None, [level, campos] {
        level(); campos();
        w32(OV(0x0016c074), ri(0, 4) == 0 ? u(ri(0, 3)) : 2u); w32(OV(0x0015f608), ri(0, 3) == 0 ? u(ri(0, 3)) : 0u); w32(OV(0x0016c06c), ri(0, 5) == 0 ? 1u : 0u);
        w32(OV(0x0015f610), coin() ? OV(0x00173d80u) : 0u); wrand(OV(0x00173d80), 0x20);
        if (coin()) { w32(OV(0x0015f604), M); wfrand(M, 3, 100, 160); wrand(M + 0x10, 0x100); } else { w32(OV(0x0015f604), 0); }
        return Args{};
    }, [](A) { zones_update(); return uint64_t{0}; });
}

}  // namespace rac1::oracle
