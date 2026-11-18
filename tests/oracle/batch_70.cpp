// Oracle batch 70: the camera modifiers (types 5 / 6) (needs the ISO).

#include "ovl_oracle.h"

#include <game/ovl/ovl_reloc.h>
namespace rac1::oracle {

void batch_70() {
    if (g_only_batch != 70) return;
    auto boot = [] { veldin_boot(); };
    g_batch = 70;
    g_ignored_words = vcross_lanes();
    for (uint32_t c = OV(0x00167150); c < OV(0x00167150) + 48 * 0xa0; c += 0xa0) {  // the look camera's VCROSS rows 0 / +0x40
        g_ignored_words.push_back(c + 0xc);
        g_ignored_words.push_back(c + 0x4c);
    }
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
    // the live camera of type `t`, the hero put in or near its volumes, its data varied
    auto setup = [](int t) -> uint32_t {
        uint32_t cam = 0;
        for (uint32_t i = 0; i < 48 && cam == 0; ++i)
            if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0 && ee_read<int16_t>(OV(0x00167150) + i * 0xa0 + 0x8c) == t) cam = OV(0x00167150) + i * 0xa0;
        if (cam == 0) return 0;
        const uint32_t e = ee_read<uint32_t>(OV(0x0015ef50)) + u(ee_read<int16_t>(cam + 0x84) * 0x20);
        const uint32_t d = ee_read<uint32_t>(e + 0x1c);
        Vec4 at = ee_ref<Vec4>(0x0013f3d0);
        const int vol = ri(0, 2);
        const uint32_t tables[3] = {OV(0x001600ec), OV(0x001600fc), OV(0x001600f4)};
        const uint32_t offs[3] = {0xc, 0x10, 0x8};
        const int32_t idx = ee_read<int32_t>(d + offs[vol]);
        if (idx >= 0 && ri(0, 3) != 0) at = ee_ref<Vec4>(ee_read<uint32_t>(tables[vol]) + u(idx) * 0x80 + 0x30);
        at.x += rf(-2, 2); at.y += rf(-2, 2); at.z += rf(-1, 1);
        ee_ref<Vec4>(0x0013f3d0) = at;
        if (t == 5) {
            if (ri(0, 2) == 0) w16(d + 0x2c, u(ri(1, 11)));
            w16(d + 0x2e, u(ri(0, 3) == 0 ? 0 : ri(1, 450)));
            w16(d + 0x48, u(ri(0, 1))); w16(d + 0x4a, u(ri(0, 1))); w16(d + 0x36, u(ri(0, 3) == 0));
            w16(d + 0x34, u(coin())); w16(d + 0x54, u(coin())); w16(d + 0x56, u(coin()));
        } else {
            w16(d + 0x26, u(ri(0, 3) == 0)); w32(d + 0x20, u(ri(0, 4)));
            w16(d + 0x44, u(ri(0, 3) == 0)); w16(d + 0x46, u(ri(0, 3) == 0)); w32(d + 0x34, u(coin()));
        }
        const uint32_t w = ee_read<uint32_t>(ee_read<uint32_t>(OV(0x00166e00)) + 0x70);
        w32(w + 0x220, ri(0, 2) == 0 ? 0u : (coin() ? cam : OV(0x00167150u)));
        w32(w + 0x230, u(ri(0, 2)));
        w32(0x0013cae0, ri(0, 4) == 0 ? (R() & 5) : 0u);
        wf(0x0013c940 + 0x100, coin() ? 0.0f : rf(-1, 1)); wf(0x0013c940 + 0x104, coin() ? 0.0f : rf(-1, 1));
        w8(0x001413f4, u(ri(0, 2))); w32(0x001413dc, ri(0, 2) == 0 ? 0x1au : 0u); w32(0x001415d4, u(ri(0, 3)));
        w32(OV(0x0015f5c4), ri(0, 4) == 0 ? 1u : 0u);
        return cam;
    };
    check("cam_mod_inside", 0x002ec7e8, 300, Ret::Int32, [level1, setup] { level1(); return Args{{setup(5)}, {}}; },
          [](A a) { return r32(static_cast<uint32_t>(a.i[0] != 0 ? cam_mod_inside(a.i[0]) : 0)); });
    check("cam_mod_claim", 0x002e83c0, 300, Ret::Int32, [level1, setup] { level1(); return Args{{setup(coin() ? 5 : 6)}, {}}; },
          [](A a) { return r32(static_cast<uint32_t>(cam_mod_claim(a.i[0]))); });
    check("cam_mod6_inside", 0x002ed348, 300, Ret::Int32, [level1, setup] { level1(); return Args{{setup(6)}, {}}; },
          [](A a) { return r32(static_cast<uint32_t>(a.i[0] != 0 ? cam_mod6_inside(a.i[0]) : 0)); });
    check("cam_mod6_update", 0x002ed498, 300, Ret::None, [level1, setup] { level1(); return Args{{setup(6)}, {}}; },
          [](A a) { if (a.i[0] != 0) cam_mod6_update(a.i[0]); return uint64_t{0}; });
    check("cam_mod_update", 0x002ec988, 400, Ret::None, [level1, setup] { level1(); return Args{{setup(5)}, {}}; },
          [](A a) { if (a.i[0] != 0) cam_mod_update(a.i[0]); return uint64_t{0}; });
    check("wind_update", 0x002a4890, 400, Ret::None, [level1] {
        level1();
        static constexpr OvConst<0x001cb7c0u> W{};
        wf(W, rf(-3.1f, 3.1f)); wf(W + 4, rf(-3.1f, 3.1f)); wf(W + 8, rf(-0.05f, 0.05f));
        w16(W + 0xc, u(ri(1, 4))); w16(W + 0xe, u(ri(0, 4)));
        wf(W + 0x10, rf(0, 0.5f)); wf(W + 0x14, rf(-0.6f, 0.6f)); wf(W + 0x18, rf(-0.05f, 0.05f));
        w16(W + 0x1c, u(ri(10, 200))); w16(W + 0x1e, u(ri(0, 3)));
        seed_rand();
        return Args{};
    }, [](A) { wind_update(); return uint64_t{0}; });
    auto lights = [] {
        for (uint32_t i = 0; i < 8; ++i) {
            if (ri(0, 2) == 0) continue;
            Vec4 p = ee_ref<Vec4>(0x0013f3d0); p.x += rf(-30, 30); p.y += rf(-30, 30); p.z += rf(-5, 5);
            ee_ref<Vec4>(OUT) = p;
            const int32_t s = light_add(OUT, rf(2, 20), rf(0, 2), rf(0, 1), rf(0, 1), rf(0, 1));
            if (s >= 0) {
                w32(OV(0x001804c0) + u(s) * 0x30 + 0x10, u(ri(1, 2)));
                if (coin()) ee_ref<Vec4>(OV(0x001804c0) + u(s) * 0x30 + 0x20) = p;
            }
        }
    };
    check("spheres_overlap", 0x001ff4b8, 300, Ret::Int32, [] { wfrand(OUT, 4, -5, 5); wfrand(OUT + 0x10, 4, -5, 5); return Args{{OUT, OUT + 0x10}, {}}; },
          [](A a) { return r32(static_cast<uint32_t>(spheres_overlap(a.i[0], a.i[1]))); });
    check("light_assign", 0x0023ea10, 200, Ret::None, [level1, lights] {
        level1(); wf(OV(0x0015f5d4), 0.5f); lights();
        const uint32_t i = u(ri(0, 7));
        if (ee_read<uint32_t>(OV(0x001804c0) + i * 0x30 + 0xc) == 0) w32(OV(0x001804c0) + i * 0x30 + 0xc, OV(0x00180640) + i * 0x400);
        return Args{{i}, {}};
    }, [](A a) { light_assign(a.i[0]); return uint64_t{0}; });
    check("level_lights_update", 0x0023e890, 200, Ret::None, [level1, lights] {
        level1(); wf(OV(0x0015f5d4), 0.5f); lights(); wf(OV(0x00166dd8), rf(-3.1f, 3.1f));
        return Args{};
    }, [](A) { level_lights_update(); return uint64_t{0}; });
    {
        const CollisionScratchIgnored spr_ignored;
        check("checkpoint_restore", 0x00285008, 200, Ret::None, [level1] {
            level1();
            static constexpr OvConst<0x001bb230u> C{};
            if (ri(0, 4) == 0) { w32(C, 0); return Args{}; }
            wrand(C, 0x320);
            w32(C, 1);
            Vec4 p = ee_ref<Vec4>(0x0013f3d0); p.x += rf(-20, 20); p.y += rf(-20, 20); ee_ref<Vec4>(C + 0x10) = p;
            wfrand(C + 0x20, 4, -3, 3); wfrand(C + 0x30, 3, -3, 3);
            w32(C + 0x44, u(ri(0, 3)));  // (4 and up: the original halts in its debug trap)
            // the saved moby data: entries for real mobys
            const uint32_t L = OV(0x001bb280);
            const int n = ri(0, 6);
            w32(L, u(n));
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
            for (int i = 0; i < n; ++i) {
                const uint32_t e = L + 4 + u(i) * 0x10;
                const uint32_t m = first + 0x100 * u(ri(1, 150));
                w32(e, u(ri(0, 0x8)) * 16); w16(e + 8, u(ri(1, 2)) * 16); w16(e + 0xa, u(ri(0, 1)));
                w16(e + 0xc, coin() ? ee_read<uint16_t>(m + 0xb2) : u(ri(0, 0x7fff)));
            }
            return Args{};
        }, [](A) { checkpoint_restore(); return uint64_t{0}; });
    }
    check("cam_mod5_accept", 0x002ed230, 400, Ret::Int32, [level1, setup] { level1(); return Args{{setup(5)}, {}}; },
          [](A a) { return r32(static_cast<uint32_t>(a.i[0] != 0 ? cam_mod5_accept(a.i[0]) : 0)); });
    check("cam_mod6_accept", 0x002ed8b0, 400, Ret::Int32, [level1, setup] { level1(); return Args{{setup(6)}, {}}; },
          [](A a) { return r32(static_cast<uint32_t>(a.i[0] != 0 ? cam_mod6_accept(a.i[0]) : 0)); });
    check("item_class_load", 0x00245770, 60, Ret::None, [level1] {
        level1();
        const int n = ee_read<int32_t>(OV(0x00160008));
        if (n <= 0) return Args{};
        w32(OV(0x0016000c), u(ri(-1, n - 1))); w32(OV(0x00160010), u(ri(0, 1)));
        for (uint32_t i = 0; i < 0x25; ++i) w8(0x0013e520 + i, u(coin()));
        const uint32_t cls = ee_read<uint32_t>(OV(0x001afbc0) + 4 * u(ri(0, n - 1)));
        return Args{{cls, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 1))}, {}};
    }, [](A a) { if (!a.i.empty()) item_class_load(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
    // the look camera (type row 1) with random springs, pad, hero state and view flags
    auto look = []() -> uint32_t {
        uint32_t cam = 0;
        for (uint32_t i = 0; i < 48 && cam == 0; ++i)
            if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0 && ee_read<int16_t>(OV(0x00167150) + i * 0xa0 + 0x8c) == 1) cam = OV(0x00167150) + i * 0xa0;
        if (cam == 0) return 0;
        const uint32_t w = ee_read<uint32_t>(cam + 0x70);
        for (uint32_t o = 0; o < 0x1c; o += 4) wf(w + o, coin() ? 0.0f : rf(-0.5f, 0.5f));
        wf(w + 0x10, rf(-1.5f, 1.5f));
        for (uint32_t o = 0x100; o < 0x110; o += 4) wf(0x0013c940 + o, coin() ? 0.0f : rf(-1, 1));
        w32(0x0013c940 + 0x1a0, ri(0, 2) == 0 ? (R() & 0xf000) : 0u);
        constexpr uint32_t P = 0x0013f350;
        w8(P + 0x20a4, u(ri(0, 2))); w8(P + 0x20a5, u(coin())); wf(P + 0x14c, coin() ? 0.0f : rf(-0.1f, 0.1f)); w16(P + 0x308, u(coin()));
        w32(0x0015eddc, u(coin())); w8(0x0015edb3, u(coin())); w16(OV(0x00166c80) + 0x270, u(ri(0, 2))); w32(P + 0x2090, u(ri(1, 2)));
        const int st[4] = {4, 0x51, 0, 7}; w32(P + 0x2284, u(st[ri(0, 3)])); wf(P + 0x98, rf(-3.1f, 3.1f));
        const uint32_t d = ee_read<uint32_t>(ee_read<uint32_t>(OV(0x0015ef50)) + u(ee_read<int16_t>(cam + 0x84) * 0x20) + 0x1c);
        w16(d + 0x20, u(ri(0, 12))); w16(d + 0x22, u(coin())); w32(d + 0x24, coin() ? 0u : u(ri(1, 40)));
        Vec4 r = {rf(-1, 1), rf(-1, 1), rf(-0.5f, 0.5f), 0}; ee_ref<Vec4>(cam) = r;
        return cam;
    };
    check("cam_look_accept", 0x002ea488, 400, Ret::Int32, [level1, look] {
        level1(); const uint32_t cam = look(); if (cam == 0) return Args{};
        uint32_t other = OV(0x00167150) + u(ri(0, 47)) * 0xa0; if (coin()) other = ee_read<uint32_t>(OV(0x00166e00));
        if (ri(0, 3) == 0) w16(other + 0x86, u(ri(0, 3) * 4)); if (ri(0, 4) == 0) w16(ee_read<uint32_t>(OV(0x00166e00)) + 0x86, 7);
        return Args{{cam, other}, {}};
    }, [](A a) { return r32(static_cast<uint32_t>(a.i.size() > 0 && a.i[0] != 0 ? cam_look_accept(a.i[0], a.i[1]) : 0)); });
    {
        const CollisionScratchIgnored spr_ignored;
        check("cam_look_update", 0x002e9f38, 400, Ret::None, [level1, look] {
            level1(); const uint32_t cam = look(); if (cam == 0) return Args{};
            return Args{{cam}, {}};
        }, [](A a) { if (a.i.size() > 0 && a.i[0] != 0) cam_look_update(a.i[0]); return uint64_t{0}; });
    }
    check("cam_look_init", 0x002ea7a0, 200, Ret::None, [level1, look] {
        level1(); const uint32_t cam = look(); if (cam == 0) return Args{};
        w32(OV(0x00166c80) + 0x254, 0); w16(OV(0x00167150) + 0x86 + 0xa0 * u(ri(0, 3)), 0);
        return Args{{cam}, {}};
    }, [](A a) { if (a.i.size() > 0 && a.i[0] != 0) cam_look_init(a.i[0]); return uint64_t{0}; });
    check("cam_look_exit", 0x002ea810, 300, Ret::None, [level1, look] {
        level1(); const uint32_t cam = look(); if (cam == 0) return Args{};
        return Args{{cam}, {}};
    }, [](A a) { if (a.i.size() > 0 && a.i[0] != 0) cam_look_exit(a.i[0]); return uint64_t{0}; });
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
