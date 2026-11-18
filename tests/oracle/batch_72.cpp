// Oracle batch 72: camera types 2 / 3 / 4 (ids 5 / 6 / 7) and their helpers, on Veldin's real
// level data (its level cameras 4 / 5 / 6 are of these types) (needs the ISO).

#include "ovl_oracle.h"

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/camera_types.h>

namespace rac1::oracle {

void batch_72() {
    if (g_only_batch != 72) return;
    auto boot = [] { veldin_boot(); };
    g_batch = 72;
    g_ignored_words = vcross_lanes();
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
        collide_vu_reset();  // the contact registers vf25..vf29 the collision leaves, the same on both sides
        oracle_vu_contact_reset();
    };
    constexpr uint32_t P = 0x0013f350, HERO = 0x0013f3d0;
    auto moby = [] { return ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(1, 150)); };
    auto near_hero = [](uint32_t a, float r, float rz) {
        Vec4 p = ee_ref<Vec4>(HERO);
        p.x += rf(-r, r); p.y += rf(-r, r); p.z += rf(-rz, rz); p.w = coin() ? 0.0f : rf(-1, 1);
        ee_ref<Vec4>(a) = p;
    };
    // the live camera of type row `row`, made active (or not) with the work area 0x169110, its
    // view near the hero, the drawn position +0x64, the view flags
    auto setup = [near_hero](int row) -> uint32_t {
        uint32_t cam = 0;
        for (uint32_t i = 0; i < 48 && cam == 0; ++i)
            if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0 && ee_read<int16_t>(OV(0x00167150) + i * 0xa0 + 0x8c) == row) cam = OV(0x00167150) + i * 0xa0;
        if (cam == 0) return 0;
        if (ri(0, 3) != 0) {
            const uint32_t old = ee_read<uint32_t>(OV(0x00166e00));
            if (old != cam) w32(OV(0x00166e04), old);
            w32(OV(0x00166e00), cam);
        }
        w32(cam + 0x70, OV(0x00169110));
        near_hero(cam + 0x30, 6, 3);
        wfrand(cam + 0x40, 3, -3.1f, 3.1f);
        wfrand(cam + 0x50, 5, -3, 3); wf(cam + 0x58, rf(1, 8));
        wfrand(cam + 0x64, 3, -1, 1);
        for (uint32_t o = 0; o < 0xc; o += 4) wf(cam + 0x64 + o, ee_read<float>(cam + 0x30 + o) + rf(-2, 2));
        w8(cam + 0x89, u(coin())); w16(cam + 0x8a, ri(0, 3) == 0 ? 1u : 0u);
        w32(OV(0x0016c06c), ri(0, 3) == 0 ? 1u : 0u);
        return cam;
    };
    auto rnd_work = [](uint32_t w, uint32_t from, uint32_t to, float lo, float hi) {
        for (uint32_t o = from; o < to; o += 4) wf(w + o, coin() ? 0.0f : rf(lo, hi));
    };

    // ---- helpers ----
    check("cam_springs_clear", 0x001eb0b0, 50, Ret::None, [] { wrand(T, 8); return Args{{T}, {}}; },
          [](A a) { cam_springs_clear(a.i[0]); return uint64_t{0}; });
    check("vec3_clear_xyz", 0x001ff250, 50, Ret::None, [] { wrand(T, 8); return Args{{T}, {}}; },
          [](A a) { vec3_clear_xyz(a.i[0]); return uint64_t{0}; });
    check("cam_angles_copy_turned", 0x001eb0c8, 200, Ret::None, [] {
        wfrand(T, 5, -4, 4); wrand(T + 0x20, 5);
        return Args{{coin() ? T : T + 0x20, T}, {rf(-4, 4)}};
    }, [](A a) { cam_angles_copy_turned(a.i[0], a.i[1], a.f[0]); return uint64_t{0}; });
    check("camera_polar_point", 0x001ed580, 300, Ret::None, [] {
        wfrand(T, 3, -4, 4); wf(T + 8, rf(0, 20)); wfrand(T + 0x10, 4, -50, 50); wrand(T + 0x20, 4);
        return Args{{coin() ? T + 0x20 : T + 0x10, T, T + 0x10}, {}};
    }, [](A a) { camera_polar_point(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
    check("angle_cos_ease", 0x00257ef0, 300, Ret::Float, [] {
        const float t = ri(0, 5) == 0 ? (coin() ? 0.0f : 1.0f) : rf(-0.2f, 1.2f);
        return Args{{}, {rf(-3.2f, 3.2f), rf(-3.2f, 3.2f), t}};
    }, [](A a) { return rf32(angle_cos_ease(a.f[0], a.f[1], a.f[2])); });
    check("vec3_cos_ease", 0x00257f70, 300, Ret::None, [] {
        wfrand(T, 4, -50, 50); wfrand(T + 0x10, 4, -50, 50); wrand(T + 0x20, 4);
        const float t = ri(0, 3) == 0 ? (coin() ? 0.0f : 1.0f) : rf(-0.2f, 1.2f);
        return Args{{coin() ? T : T + 0x20, T, T + 0x10}, {t}};
    }, [](A a) { vec3_cos_ease(a.i[0], a.i[1], a.i[2], a.f[0]); return uint64_t{0}; });
    check("angles_cos_ease", 0x00258050, 300, Ret::None, [] {
        wfrand(T, 4, -3.2f, 3.2f); wfrand(T + 0x10, 4, -3.2f, 3.2f); wrand(T + 0x20, 4);
        const float t = ri(0, 5) == 0 ? (coin() ? 0.0f : 1.0f) : rf(-0.2f, 1.2f);
        return Args{{coin() ? T : T + 0x20, T, T + 0x10}, {t}};
    }, [](A a) { angles_cos_ease(a.i[0], a.i[1], a.i[2], a.f[0]); return uint64_t{0}; });
    {
        const CollisionScratchIgnored spr_ignored;
        check("camera_collide_sweep", 0x001ed6a8, 300, Ret::None, [level1, setup, near_hero] {
            level1();
            const uint32_t cam = setup(ri(2, 4));
            if (cam == 0) return Args{};
            const uint32_t pos = coin() ? cam + 0x30 : OUT;
            near_hero(pos, 8, 4);
            if (ri(0, 4) == 0) { const uint32_t a = ee_read<uint32_t>(OV(0x00166e00)); for (uint32_t o = 0; o < 0xc; o += 4) wf(pos + o, ee_read<float>(a + 0x64 + o)); }
            return Args{{pos}, {ri(0, 4) == 0 ? rf(0.05f, 0.3f) : rf(0.3f, 2.0f)}};
        }, [](A a) { if (a.i.size() > 0) camera_collide_sweep(a.i[0], a.f[0]); return uint64_t{0}; });
    }

    // ---- type 2 (id 5) ----
    auto cam5 = [setup, rnd_work, near_hero]() -> uint32_t {
        const uint32_t cam = setup(2);
        if (cam == 0) return 0;
        const uint32_t w = OV(0x00169110);
        rnd_work(w, 0, 0x80, -0.5f, 0.5f);
        wf(w + 0x10, rf(0, 0.5f)); wf(w + 0x14, rf(0, 0.5f)); wf(w + 0x18, coin() ? 0.0f : rf(0.1f, 2));
        wf(w + 0x50, rf(0, 0.5f)); wf(w + 0x54, rf(0, 0.5f)); wf(w + 0x58, coin() ? 0.0f : rf(0.1f, 2));
        w16(w + 0x1c, u(ri(0, 2))); w16(w + 0x5c, u(ri(0, 2)));
        near_hero(w + 0x80, 10, 4);
        wfrand(w + 0x90, 4, -3.1f, 3.1f);
        // (never 0: the step's fraction is then +-FLT_MAX, an angle euler_to_mat4_vu's port does
        // not reproduce out of range)
        const int32_t len = ri(0, 4) == 0 ? (coin() ? -1 : 1) : ri(1, 90);
        w32(w + 0xe4, u(len)); w32(w + 0xe0, u(ri(0, 5) == 0 ? ri(-2, 2) : ri(0, len > 0 ? len : 1)));
        wfrand(w + 0xe8, 4, -3.1f, 3.1f); wf(w + 0xf0, rf(1, 10));
        wfrand(w + 0xfc, 2, -3.1f, 3.1f); wf(w + 0x104, rf(1, 10));
        near_hero(w + 0x110, 3, 2);
        wf(w + 0x124, rf(-1, 2)); wf(w + 0x128, rf(-1, 2));
        w32(w + 0x120, u(coin()));
        wf(OV(0x00166dd8), rf(-3.1f, 3.1f));
        w8(cam + 0x88, u(ri(0, 4)));
        return cam;
    };
    check("cam5_init", 0x002ead60, 200, Ret::None, [level1, cam5] { level1(); return Args{{cam5()}, {}}; },
          [](A a) { if (a.i[0] != 0) cam5_init(a.i[0]); return uint64_t{0}; });
    check("cam5_step", 0x002eae20, 800, Ret::None, [level1, cam5] { level1(); return Args{{cam5()}, {}}; },
          [](A a) { if (a.i[0] != 0) cam5_step(a.i[0]); return uint64_t{0}; });
    {
        const CollisionScratchIgnored spr_ignored;
        check("cam5_update", 0x002eb218, 400, Ret::None, [level1, cam5] { level1(); return Args{{cam5()}, {}}; },
              [](A a) { if (a.i[0] != 0) cam5_update(a.i[0]); return uint64_t{0}; });
    }

    // ---- type 3 (id 6) ----
    auto cam6 = [setup, rnd_work, near_hero]() -> uint32_t {
        const uint32_t cam = setup(3);
        if (cam == 0) return 0;
        const uint32_t w = OV(0x00169110);
        rnd_work(w, 0, 0x14, -0.5f, 0.5f);
        wfrand(w + 0x14, 5, -3.1f, 3.1f); wf(w + 0x1c, rf(1, 10));
        wfrand(w + 0x28, 15, 0, 0.5f);
        for (uint32_t o = 0x50; o < 0x64; o += 4) wf(w + o, coin() ? 0.0f : rf(0.1f, 2));
        near_hero(w + 0x80, 5, 3);
        rnd_work(w, 0x90, 0x9c, -0.5f, 0.5f);
        w32(w + 0x9c, u(coin()));
        wf(w + 0xa0, rf(0, 0.5f)); wf(w + 0xa4, rf(0, 0.5f)); wf(w + 0xa8, coin() ? 0.0f : rf(0.1f, 2)); wf(w + 0xac, rf(-1, 1));
        near_hero(w + 0xb0, 5, 3);
        wfrand(w + 0xc0, 4, -3.1f, 3.1f);
        return cam;
    };
    check("cam6_place", 0x002eb278, 200, Ret::None, [level1, cam6] { level1(); return Args{{cam6()}, {}}; },
          [](A a) { if (a.i[0] != 0) cam6_place(a.i[0]); return uint64_t{0}; });
    check("cam6_reset", 0x002eb3b0, 200, Ret::None, [level1, cam6] { level1(); return Args{{cam6()}, {}}; },
          [](A a) { if (a.i[0] != 0) cam6_reset(a.i[0]); return uint64_t{0}; });
    check("cam6_step", 0x002eb480, 400, Ret::None, [level1, cam6] { level1(); return Args{{cam6()}, {}}; },
          [](A a) { if (a.i[0] != 0) cam6_step(a.i[0]); return uint64_t{0}; });
    check("cam6_init", 0x002ebb00, 200, Ret::None, [level1, cam6] { level1(); return Args{{cam6()}, {}}; },
          [](A a) { if (a.i[0] != 0) cam6_init(a.i[0]); return uint64_t{0}; });
    check("cam6_update", 0x002ebbb0, 200, Ret::None, [level1, cam6] { level1(); return Args{{cam6()}, {}}; },
          [](A a) { if (a.i[0] != 0) cam6_update(a.i[0]); return uint64_t{0}; });

    // ---- type 4 (id 7) ----
    auto cam7 = [setup, rnd_work, near_hero, moby]() -> uint32_t {
        const uint32_t cam = setup(4);
        if (cam == 0) return 0;
        const uint32_t w = OV(0x00169110);
        const int st[7] = {0x24, 0x25, 0x26, 0x2c, 0x2d, 6, 0};
        w32(P + 0x2084, u(st[ri(0, 6)]));
        w32(P + 0x2284, coin() ? u(ee_read<int16_t>(cam + 0x86)) : u(ri(0, 8)));
        const int lv[5] = {0, 0xe, 7, 9, 3};
        w32(0x0015ed84, u(lv[ri(0, 4)]));
        wf(0x0015ed6c, rf(0.5f, 2));
        const uint32_t ride = ri(0, 4) == 0 ? 0u : moby();
        w32(P + 0x994, ride);
        w32(P + 0x964, ri(0, 5) == 0 ? 0u : moby());
        w32(P + 0x2080, coin() ? 0u : moby());
        wf(P + 0x9a0, rf(0.5f, 4)); wf(P + 0x9ac, rf(-3.1f, 3.1f)); wf(P + 0x98, rf(-3.1f, 3.1f));
        if (ride != 0) {
            w8(ride + 0x21, coin() ? 0xffu : u(ri(0, 3)));
            wf(ride + 0x18, ee_read<float>(HERO + 8) + rf(-1, 5));
            // the list of the ridden moby: mobys around it, some of class 0x323 / 0x2f6, the
            // last entry with bit 15
            const uint32_t idx = ee_read<uint8_t>(ride + 0x21);
            if (idx != 0xff) {
                const int n = ri(0, 6);
                w32(OV(0x001ab840) + idx * 4, n == 0 && coin() ? 0u : T2);
                const uint32_t base = ee_read<uint32_t>(OV(0x0015ffd8));
                for (int i = 0; i < (n == 0 ? 1 : n); ++i) {
                    const uint32_t m = ri(0, 6) == 0 ? ride : moby();
                    if (ri(0, 3) != 0) w16(m + 0xa6, coin() ? 0x323u : 0x2f6u);
                    Vec4 p = ee_ref<Vec4>(ride + 0x10); p.x += rf(-30, 30); p.y += rf(-30, 30);
                    ee_ref<Vec4>(m + 0x10) = p;
                    w16(T2 + u(i) * 2, ((m - base) >> 8) | (i == (n == 0 ? 1 : n) - 1 ? 0x8000u : 0u));
                }
            }
        }
        w32(w + 0x80, u(coin()));
        const uint32_t m = moby(); w32(w + 0x84, m); wf(m + 0x48, rf(-3.1f, 3.1f));
        wf(w + 0x8c, rf(0.5f, 3)); wf(w + 0xa0, rf(2, 6)); wf(w + 0xb0, rf(0, 3));
        wf(w + 0x30, coin() ? rf(0, 1.2f) : rf(-0.2f, 0.2f));
        wf(w + 0xb8, rf(-3.1f, 3.1f)); wf(w + 0xbc, rf(-0.1f, 0.1f));
        w32(w + 0xc0, coin() ? ee_read<uint32_t>(P + 0x994) : (coin() ? ee_read<uint32_t>(P + 0x964) : moby()));
        wf(OV(0x00161dd8), rf(0.001f, 0.05f));
        rnd_work(w, 0x70, 0x7c, -1, 1);
        // a direction for the rows (unit-ish), the view row toward the hero sometimes
        Vec4 r = {rf(-1, 1), rf(-1, 1), rf(-0.5f, 0.5f), 0}; ee_ref<Vec4>(cam) = r; ee_ref<Vec4>(cam + 0x40) = r;
        return cam;
    };
    check("cam7_init", 0x002ebbd8, 300, Ret::None, [level1, cam7] { level1(); return Args{{cam7()}, {}}; },
          [](A a) { if (a.i[0] != 0) cam7_init(a.i[0]); return uint64_t{0}; });
    {
        const CollisionScratchIgnored spr_ignored;
        check("cam7_track", 0x002ebe70, 800, Ret::None, [level1, cam7] { level1(); return Args{{cam7()}, {}}; },
              [](A a) { if (a.i[0] != 0) cam7_track(a.i[0]); return uint64_t{0}; });
    }
    check("cam7_aim", 0x002ec4a8, 400, Ret::None, [level1, cam7] { level1(); return Args{{cam7()}, {}}; },
          [](A a) { if (a.i[0] != 0) cam7_aim(a.i[0]); return uint64_t{0}; });
    {
        const CollisionScratchIgnored spr_ignored;
        check("cam7_step", 0x002ec678, 300, Ret::None, [level1, cam7] { level1(); return Args{{cam7()}, {}}; },
              [](A a) { if (a.i[0] != 0) cam7_step(a.i[0]); return uint64_t{0}; });
    }
    {
        const CollisionScratchIgnored spr_ignored;
        check("cam7_update", 0x002ec6c0, 500, Ret::None, [level1, cam7] { level1(); return Args{{cam7()}, {}}; },
              [](A a) { if (a.i[0] != 0) cam7_update(a.i[0]); return uint64_t{0}; });
    }
    check("cam7_exit", 0x002ec7b0, 200, Ret::None, [level1, cam7] { level1(); return Args{{cam7()}, {}}; },
          [](A a) { if (a.i[0] != 0) cam7_exit(a.i[0]); return uint64_t{0}; });

    // the camera update of a frame with the camera of each type made active
    {
        const CollisionScratchIgnored spr_ignored;
        check("camera_select", 0x001eba20, 300, Ret::Int32, [level1, cam5, cam6, cam7] {
            level1();
            const int k = ri(0, 2);
            const uint32_t cam = k == 0 ? cam5() : (k == 1 ? cam6() : cam7());
            if (cam != 0) w32(OV(0x00166e00), cam);
            return Args{};
        }, [](A) { return r32(static_cast<uint32_t>(camera_select())); });
    }
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
