// Oracle batch 71: the particles' draw 0x267c98 (culling, depth sort, the VU1 packets) on
// Veldin's real level data (needs the ISO).

#include "ovl_oracle.h"

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particles_draw.h>
#include <game/ovl/particles_vu1.h>
#include <game/ovl/sparkles_draw.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/glows.h>
#include <game/ovl/effect_quads.h>
#include <game/ovl/screen_effects.h>
#include <runtime/gs/gs_emu.h>
#include <runtime/vu/vu1.h>

#include <cmath>

namespace rac1::oracle {

void batch_71() {
    if (g_only_batch != 71) return;
    auto boot = [] { veldin_boot(); };
    g_batch = 71;
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
            w16(0x001518d0, 512);  // the display as the frame sets it (512 x 416)
            w16(0x001518d2, 416);
            screen_setup();
            projection_setup();
            render_view_setup();
            snap.assign(ram, ram + 0x02000000);
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    if (std::getenv("ORACLE_PDRAW_INFO") != nullptr) {  // the inputs the draw reads, as Veldin sets them
        level1();
        for (uint32_t a : {OV(0x0016cb00u), OV(0x0016cb10u), OV(0x0016cb20u), OV(0x0016cb30u), OV(0x0016cb40u), OV(0x0016cb50u), OV(0x0016cb60u), OV(0x0016cb70u), OV(0x0016cb80u), OV(0x0016cb90u), OV(0x0016cba0u), OV(0x0016cbb0u), OV(0x0016cc40u), OV(0x0016cc60u), OV(0x0016caa0u), OV(0x0016cab0u), OV(0x0016cae0u), OV(0x0016cc10u),
                           OV(0x00166dc0u), OV(0x00166c80u), OV(0x00166c90u), OV(0x00166ca0u), OV(0x00166d40u), OV(0x00166d50u), OV(0x00166d60u), OV(0x00166d70u), OV(0x0016cc00u),
                           OV(0x0016caf0u), OV(0x0016cc20u), OV(0x001607e0u), OV(0x0016cc50u), OV(0x0016cbe0u)})
            std::printf("%08x: %g %g %g %g\n", a, ee_read<float>(a), ee_read<float>(a + 4), ee_read<float>(a + 8), ee_read<float>(a + 12));
        std::printf("pool %08x last %d s3 %08x ring %08x tex %08x vram %08x\n", ee_read<uint32_t>(OV(0x0016016c)), ee_read<int32_t>(OV(0x00160174)),
                    ee_read<uint32_t>(OV(0x0016017c)), ee_read<uint32_t>(OV(0x001611c0)), ee_read<uint32_t>(OV(0x0015f418)), ee_read<uint32_t>(0x0015ee74));
        for (uint32_t i = 0; i < 8; ++i) std::printf("tex0[%u] %016llx\n", i, static_cast<unsigned long long>(ee_read<uint64_t>(OV(0x001b1a80) + 8 * i)));
    }
    // n random particles of every shape around the view: most in front within the depth ranges,
    // some behind, beside, too near / too far, dead, already drawn last frame
    // `many`: a full pool (as Batalia's rain fills it: up to 2048 live records), most of them
    // streaks in view, so that the packet's 0x800-byte chunks fill with 6-quad streaks
    auto particles = [](bool many = false) {
        const uint32_t base = ee_read<uint32_t>(OV(0x0016016c));
        const int n = many ? ri(900, 2048) : ri(0, 8) == 0 ? 0 : (ri(0, 3) == 0 ? ri(1, 600) : ri(1, 80));
        w32(OV(0x00160174), u(n - 1));
        const float cx = ee_read<float>(OV(0x00166dc0)), cy = ee_read<float>(OV(0x00166dc4)), cz = ee_read<float>(OV(0x00166dc8));
        float fw[3], rt[3], up[3];  // the view's forward / right / down axes in the world (columns of 0x166c80)
        for (int k = 0; k < 3; ++k) {
            rt[k] = ee_read<float>(OV(0x00166c80) + 0x10 * u(k));
            up[k] = ee_read<float>(OV(0x00166c84) + 0x10 * u(k));
            fw[k] = ee_read<float>(OV(0x00166c88) + 0x10 * u(k));
        }
        auto place = [&](uint32_t a, float d, float side) {
            const float x = rf(-side, side), y = rf(-side, side);
            wf(a, cx + fw[0] * d + rt[0] * x + up[0] * y);
            wf(a + 4, cy + fw[1] * d + rt[1] * x + up[1] * y);
            wf(a + 8, cz + fw[2] * d + rt[2] * x + up[2] * y);
        };
        for (int i = 0; i < n; ++i) {
            const uint32_t p = base + u(i) * 0x40;
            wrand(p, 0x10);
            const uint32_t shape = many && ri(0, 3) != 0 ? 3u : u(ri(0, 3));
            uint32_t fl = shape | (coin() ? 0x20u : 0u) | (ri(0, 3) == 0 ? 1u : 0u) | (R() & 0x1c);
            if (ri(0, 10) == 0) fl |= 0x80;
            w8(p + 1, fl);
            w8(p + 2, u(ri(0, 127)));
            w8(p + 9, ri(0, many ? 30 : 6) == 0 ? R() : (u(ri(4, 15)) << 4) | u(ri(0, 1)));
            const int where = many && ri(0, 3) != 0 ? 9 : ri(0, 9);
            const float d = where == 0 ? rf(-30, 0) : where == 1 ? rf(0, 1.5f) : where == 2 ? rf(150, 400) : rf(1.5f, 60);
            const float side = where == 3 ? rf(10, 80) : rf(0.1f, 0.7f) * std::fabs(d) + 0.5f;
            place(p + 0x10, d, side);
            wf(p + 0x1c, rf(0.02f, 2.0f));
            if (shape != 2) wf(p + 0xc, ri(0, 10) == 0 ? rf(0, 40) : rf(0.05f, 3.0f));
            if (shape >= 2) {
                if (coin()) place(p + 0x20, d + rf(-3, 3), side);
                else for (uint32_t k = 0; k < 3; ++k) wf(p + 0x20 + 4 * k, ee_read<float>(p + 0x10 + 4 * k) + rf(-2, 2));
            }
        }
        w32(OV(0x0016017c), ri(0, 4) == 0 ? R() : NOT_ADDR(0x1e0000u) + u(ri(-0x10000, 0x10000)));
        w32(OV(0x0015f418), u(ri(0, 8)));
        w32(0x0015ee74, u(ri(0x1000, 0x40000)) & ~0xffu);
        for (uint32_t i = 0; i < 0x80; ++i)  // some textures not yet placed (bit 63 clear)
            if (ri(0, 4) == 0) w32(OV(0x001b1a84) + 8 * i, ee_read<uint32_t>(OV(0x001b1a84) + 8 * i) & 0x7fffffffu);
    };
    if (std::getenv("ORACLE_PDRAW_STATS") != nullptr) {  // coverage: what the cases draw, by shape
        int tot[4] = {}, drawn[4] = {}, bytes = 0, allocs = 0;
        for (int c = 0; c < 100; ++c) {
            level1();
            particles();
            w32(OV(0x001611c0), 0x01e10000u); w32(OV(0x0015f5f8), 0x01f80000u);
            const uint32_t base = ee_read<uint32_t>(OV(0x0016016c));
            const int32_t n = ee_read<int32_t>(OV(0x00160174)) + 1;
            for (int32_t i = 0; i < n; ++i) w8(base + u(i) * 0x40 + 1, ee_read<uint8_t>(base + u(i) * 0x40 + 1) & ~0x20u);
            const uint32_t t0 = ee_read<uint32_t>(OV(0x0015f418));
            particles_draw();
            allocs += static_cast<int>(ee_read<uint32_t>(OV(0x0015f418)) - t0);
            bytes += static_cast<int>(ee_read<uint32_t>(OV(0x001611c0)) - 0x01e10000u);
            for (int32_t i = 0; i < n; ++i) {
                const uint8_t f = ee_read<uint8_t>(base + u(i) * 0x40 + 1);
                if (f & 0x80) continue;
                ++tot[f & 3];
                if (f & 0x20) ++drawn[f & 3];
            }
        }
        std::printf("drawn/live by shape: %d/%d %d/%d %d/%d %d/%d, packet bytes %d, texture allocs %d\n", drawn[0], tot[0], drawn[1], tot[1],
                    drawn[2], tot[2], drawn[3], tot[3], bytes, allocs);
    }
    check("particles_draw", 0x00267c98, 300, Ret::None, [level1, particles] {
        level1();
        particles();
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x100))); w32(OV(0x0015f5f8), 0x01f80000u);  // the packet buffer
        return Args{};
    }, [](A) { particles_draw(); return uint64_t{0}; });
    check("particles_draw_many", 0x00267c98, 100, Ret::None, [level1, particles] {
        level1();
        particles(true);
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x100))); w32(OV(0x0015f5f8), 0x01f80000u);
        return Args{};
    }, [](A) { particles_draw(); return uint64_t{0}; });
    auto sparkles = [level1] {
        level1();
        const float cx = ee_read<float>(OV(0x00166dc0)), cy = ee_read<float>(OV(0x00166dc4)), cz = ee_read<float>(OV(0x00166dc8));
        float fw[3];
        for (int k = 0; k < 3; ++k) fw[k] = ee_read<float>(OV(0x00166c88) + 0x10 * u(k));
        for (uint32_t i = 0; i < 16; ++i) {
            const uint32_t e = OV(0x0016ea40) + i * 0x20;
            wrand(e, 8);
            const float d = ri(0, 5) == 0 ? rf(-20, 1) : rf(0.5f, 40);
            for (uint32_t k = 0; k < 3; ++k) wf(e + 4 * k, (k == 0 ? cx : k == 1 ? cy : cz) + fw[k] * d + rf(-0.4f, 0.4f) * d);
            w16(e + 0x10, ri(0, 3) == 0 ? u(ri(-3, 0)) : u(ri(1, 30)));
            w16(e + 0x12, u(ri(0, 4) == 0 ? ri(-0x100, 0x100) : ri(0, 0x80)));
            wf(e + 0x18, rf(-7, 7));
            wf(e + 0x1c, rf(0, 3));
        }
        if (coin()) { w32(OV(0x0016d180) + 0x13 * 0x10, 0); w32(OV(0x0016d184) + 0x13 * 0x10, 0); }
        w32(OV(0x0015f418), u(ri(0, 0x40)));
        w32(0x0015ee74, u(ri(0x1000, 0x40000)) & ~0xffu);
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x100))); w32(OV(0x0015f5f8), 0x01f80000u);
    };
    if (std::getenv("ORACLE_SPARK_STATS") != nullptr) {  // coverage: sparkles drawn / culled
        int drawn = 0, culled = 0;
        for (int c = 0; c < 100; ++c) {
            sparkles();
            int live = 0;
            for (uint32_t i = 0; i < 16; ++i) live += ee_read<int16_t>(OV(0x0016ea40) + i * 0x20 + 0x10) > 0;
            const uint32_t r0 = ee_read<uint32_t>(OV(0x001611c0));
            sparkles_draw();
            const int n = static_cast<int>((ee_read<uint32_t>(OV(0x001611c0)) - r0) / 0xa0);
            drawn += n;
            culled += live - n;
        }
        std::printf("sparkles drawn %d culled %d\n", drawn, culled);
    }
    check("sparkles_draw", 0x001fead8, 300, Ret::None, [sparkles] { sparkles(); return Args{}; }, [](A) { sparkles_draw(); return uint64_t{0}; });
    // the quad drawer: a quad in front of the view (on screen, partly off, or out), sometimes
    // through a matrix
    auto quad = [](uint32_t d) {
        const float cx = ee_read<float>(OV(0x00166dc0)), cy = ee_read<float>(OV(0x00166dc4)), cz = ee_read<float>(OV(0x00166dc8));
        float fw[3], rt[3], up[3];
        for (int k = 0; k < 3; ++k) {
            rt[k] = ee_read<float>(OV(0x00166c80) + 0x10 * u(k));
            up[k] = ee_read<float>(OV(0x00166c84) + 0x10 * u(k));
            fw[k] = ee_read<float>(OV(0x00166c88) + 0x10 * u(k));
        }
        const int mode = ri(0, 3);
        const float dist = mode == 3 ? rf(-20, 0.5f) : rf(2, 40);
        const float off = mode == 1 ? rf(0.5f, 1.5f) * dist : mode == 2 ? rf(1.5f, 4.0f) * dist : rf(0, 0.3f) * dist;
        const float ox = rf(-1, 1) * off, oy = rf(-1, 1) * off * 0.8f, size = mode == 1 ? rf(0.3f, 2.0f) * dist : rf(0.05f, 0.3f) * dist;
        for (uint32_t k = 0; k < 4; ++k) {
            const float sx = (k == 1 || k == 2 ? 1.0f : -1.0f) * size, sy = (k >= 2 ? 1.0f : -1.0f) * size;
            for (uint32_t i = 0; i < 3; ++i)
                wf(d + 0x10 * k + 4 * i, (i == 0 ? cx : i == 1 ? cy : cz) + fw[i] * dist + rt[i] * (ox + sx) + up[i] * (oy + sy));
            wf(d + 0x10 * k + 0xc, ri(0, 3) == 0 ? rf(0.5f, 2.0f) : 1.0f);
        }
        wrand(d + 0x40, 4);
        wfrand(d + 0x50, 8, 0, 1);
        wrand(d + 0x70, 8);
    };
    if (std::getenv("ORACLE_QUAD_STATS") != nullptr) {  // coverage: on screen / clipped / rejected
        int on = 0, clipped = 0, out = 0;
        for (int c = 0; c < 300; ++c) {
            level1();
            quad(M);
            w32(OV(0x001611c0), 0x01e10000u); w32(OV(0x0015f5f8), 0x01f80000u);
            quad_draw(M, 0);
            const uint32_t n = ee_read<uint32_t>(OV(0x001611c0)) - 0x01e10000u;
            (n == 0xd0 ? on : n == 0x60 ? clipped : out) += 1;
        }
        std::printf("quads on screen %d clipped %d rejected %d\n", on, clipped, out);
    }
    check("quad_draw", 0x001fd228, 400, Ret::None, [level1, quad] {
        level1();
        quad(M);
        uint32_t mat = 0;
        if (ri(0, 3) == 0) {  // a matrix: a turn about z and a small move (the points relative to it)
            mat = T;
            const float a = rf(-3.1f, 3.1f);
            const float m[16] = {std::cos(a), std::sin(a), 0, 0, -std::sin(a), std::cos(a), 0, 0, 0, 0, 1, 0, rf(-1, 1), rf(-1, 1), rf(-1, 1), 1};
            for (uint32_t i = 0; i < 16; ++i) wf(T + 4 * i, m[i]);
        }
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x100))); w32(OV(0x0015f5f8), 0x01f80000u);
        return Args{{M, mat, 0}, {}};
    }, [](A a) { quad_draw(a.i[0], a.i[1]); return uint64_t{0}; });
    g_extra_noop_hooks.clear();
    // quads across the screen's edges (or the near plane) only: the triangle clipper's paths
    check("quad_draw_clipped", 0x001fd228, 600, Ret::None, [level1] {
        level1();
        const float cx = ee_read<float>(OV(0x00166dc0)), cy = ee_read<float>(OV(0x00166dc4)), cz = ee_read<float>(OV(0x00166dc8));
        float fw[3], rt[3], up[3];
        for (int k = 0; k < 3; ++k) {
            rt[k] = ee_read<float>(OV(0x00166c80) + 0x10 * u(k));
            up[k] = ee_read<float>(OV(0x00166c84) + 0x10 * u(k));
            fw[k] = ee_read<float>(OV(0x00166c88) + 0x10 * u(k));
        }
        const float dist = ri(0, 4) == 0 ? rf(0.05f, 1.0f) : rf(1, 30);
        const float ox = rf(-1.2f, 1.2f) * dist, oy = rf(-1.0f, 1.0f) * dist, size = rf(0.2f, 1.5f) * dist;
        const float tilt = rf(-1, 1);
        for (uint32_t k = 0; k < 4; ++k) {
            const float sx = (k == 1 || k == 2 ? 1.0f : -1.0f) * size, sy = (k >= 2 ? 1.0f : -1.0f) * size;
            const float dz = sy * tilt;
            for (uint32_t i = 0; i < 3; ++i)
                wf(M + 0x10 * k + 4 * i, (i == 0 ? cx : i == 1 ? cy : cz) + fw[i] * (dist + dz) + rt[i] * (ox + sx) + up[i] * (oy + sy));
            wf(M + 0x10 * k + 0xc, 1.0f);
        }
        wrand(M + 0x40, 4);
        wfrand(M + 0x50, 8, 0, 1);
        wrand(M + 0x70, 8);
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x100))); w32(OV(0x0015f5f8), 0x01f80000u);
        return Args{{M, 0, 0}, {}};
    }, [](A a) { quad_draw(a.i[0], a.i[1]); return uint64_t{0}; });
    check("decals_draw", 0x001f9660, 300, Ret::None, [level1] {
        level1();
        const float cx = ee_read<float>(OV(0x00166dc0)), cy = ee_read<float>(OV(0x00166dc4)), cz = ee_read<float>(OV(0x00166dc8));
        float fw[3];
        for (int k = 0; k < 3; ++k) fw[k] = ee_read<float>(OV(0x00166c88) + 0x10 * u(k));
        const int n = ri(-1, 8);
        w32(OV(0x0015f434), u(n));
        for (int i = 0; i < 8; ++i) {
            const uint32_t e = OV(0x0016e080) + u(i) * 0x20;
            const float d = ri(0, 4) == 0 ? rf(-10, 2) : rf(2, 30);
            for (uint32_t k = 0; k < 3; ++k) wf(e + 4 * k, (k == 0 ? cx : k == 1 ? cy : cz) + fw[k] * d + rf(-0.3f, 0.3f) * d);
            wf(e + 0xc, ri(0, 5) == 0 ? rf(2, 20) : rf(0.2f, 2));
            wfrand(e + 0x10, 4, -1, 1);
            if (coin()) { wf(e + 0x10, rf(-0.2f, 0.2f)); wf(e + 0x14, rf(-0.2f, 0.2f)); wf(e + 0x18, 1.0f); }
        }
        if (coin()) wfrand(OV(0x0016c7e0), 16, -1, 1);
        if (coin()) { w32(OV(0x0016d180), 0); w32(OV(0x0016d184), 0); }
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x100))); w32(OV(0x0015f5f8), 0x01f80000u);
        return Args{};
    }, [](A) { decals_draw(); return uint64_t{0}; });
    g_extra_noop_hooks.clear();
    auto near_point = [](uint32_t a, float lo, float hi) {  // a point in front of the view
        const float d = rf(lo, hi);
        for (uint32_t k = 0; k < 3; ++k)
            wf(a + 4 * k, ee_read<float>(OV(0x00166dc0) + 4 * k) + ee_read<float>(OV(0x00166c88) + 0x10 * k) * d + rf(-0.3f, 0.3f) * d);
        wf(a + 0xc, coin() ? 1.0f : rf(-2, 2));
    };
    check("glow_draw", 0x00263618, 300, Ret::None, [level1, near_point] {
        level1();
        near_point(OUT, ri(0, 5) == 0 ? -5.0f : 0.5f, 30);
        if (coin()) { w32(OV(0x0016d180) + 0xb * 0x10, 0); w32(OV(0x0016d184) + 0xb * 0x10, 0); }
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x100))); w32(OV(0x0015f5f8), 0x01f80000u);
        return Args{{OUT, R()}, {rf(0, ri(0, 4) == 0 ? 20.0f : 1.0f), rf(-0.5f, 0.5f)}};
    }, [](A a) { glow_draw(a.i[0], a.i[1], a.f[0], a.f[1]); return uint64_t{0}; });
    check("hero_glows_draw", 0x00207d40, 400, Ret::None, [level1, near_point] {
        level1();
        constexpr uint32_t P = 0x0013f350;
        static const int32_t modes[] = {0, 1, 2, 3, 5, 6};
        w32(OV(0x0015f5c4), u(modes[ri(0, 5)]));
        w8(P + 0x20a4, u(ri(0, 4)));
        w16(P + 0x1eb0, u(ri(-1, 8)));
        for (uint32_t k = 0; k < 8; ++k) {
            near_point(P + 0x1dd0 + 0x10 * k, 1, 25);
            w32(P + 0x1e50 + 4 * k, R());
            wf(P + 0x1e70 + 4 * k, rf(0, 1.5f));
            wf(P + 0x1e90 + 4 * k, rf(-0.3f, 0.3f));
        }
        for (uint32_t k = 0; k < 4; ++k) near_point(P + 0x1d50 + 0x10 * k, 1, 25);
        w32(P + 0x22e4, R()); w32(P + 0x2300, R());
        w16(P + 0x22d8, coin() ? 0u : R()); w16(P + 0x22fc, R() & 1);
        w8(0x0015edb3, R() & 1); wf(0x0015ed60, rf(0, 3)); wf(0x0014097c, rf(0, 0.3f));
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x100))); w32(OV(0x0015f5f8), 0x01f80000u);
        return Args{{ee_read<uint32_t>(0x001413d0)}, {}};
    }, [](A a) { hero_glows_draw(a.i[0]); return uint64_t{0}; });
    // class 1060's glows on Veldin's own lights (and some other moby given the group / class)
    auto with_class = [](int32_t cls) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); ee_read<uint8_t>(m + 0x20) != 0xff && m < 0x01fe0000u; m += 0x100)
            if (ee_read<int16_t>(m + 0xa6) == cls) v.push_back(m);
        return v;
    };
    check("light_group_glows_draw", 0x002df3d8, 300, Ret::None, [level1, with_class] {
        level1();
        const std::vector<uint32_t> lights = with_class(1060);
        uint32_t m = lights.empty() || ri(0, 5) == 0 ? ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(0, 100)) : lights[u(ri(0, static_cast<int32_t>(lights.size()) - 1))];
        for (uint32_t l : lights) w8(l + 0x31, R() & 1);
        static const int32_t levels[] = {0, 1, 0x12, 3};
        w32(0x0015ed84, u(levels[ri(0, 3)]));
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x100))); w32(OV(0x0015f5f8), 0x01f80000u);
        return Args{{m}, {}};
    }, [](A a) { light_group_glows_draw(a.i[0]); return uint64_t{0}; });
    check("bomb_discs_draw", 0x002acbf0, 300, Ret::None, [level1, near_point] {
        level1();
        const uint32_t m = M2;
        w32(m + 0x78, OUT);
        near_point(OUT + 0x10, ri(0, 5) == 0 ? -5.0f : 1, 25);
        wfrand(OUT + 0x20, 4, -1, 1);
        if (ri(0, 6) == 0) { wf(OUT + 0x20, 0); wf(OUT + 0x28, 0); }
        w32(OV(0x0015f5c4), ri(0, 4) == 0 ? 2u : u(ri(0, 1)));
        w32(OV(0x0015f5cc), R() & 0xffff);
        if (coin()) for (uint32_t t : {0x11u, 0x12u}) { w32(OV(0x0016d180) + t * 0x10, 0); w32(OV(0x0016d184) + t * 0x10, 0); }
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x100))); w32(OV(0x0015f5f8), 0x01f80000u);
        return Args{{m}, {}};
    }, [](A a) { bomb_discs_draw(a.i[0]); return uint64_t{0}; });
    check("wrench_trail_draw", 0x002a7780, 400, Ret::None, [level1] {
        level1();
        const uint32_t m = M2, hero = ee_read<uint32_t>(0x001413d0);
        w32(m + 0x78, ri(0, 10) == 0 ? 0u : OUT);
        wrand(OUT, 0x20);
        const int tm = ri(0, 3);
        wf(OUT + 0x70, tm == 0 ? 0.0f : tm == 1 ? 1.0f : rf(1, 40));
        static const uint32_t anims[] = {0x17, 0x18, 0x19, 0x2b, 0x05, 0xff};
        const uint32_t a = anims[ri(0, 5)];
        w8(hero + 0x52, a);
        w8(hero + 0x53, coin() ? a : anims[ri(0, 5)]);
        w16(OUT + 0x78, coin() ? anims[ri(0, 4)] : R());
        const uint32_t now = R() & 0xffff;
        w32(OV(0x0015f5cc), now);
        w32(OV(0x001613c0), coin() ? now - u(ri(0, 3)) : R());
        wf(OV(0x001613bc), rf(0.5f, 1.5f));
        if (coin()) for (uint32_t t : {0xcu, 0x14u}) { w32(OV(0x0016d180) + t * 0x10, 0); w32(OV(0x0016d184) + t * 0x10, 0); }
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x100))); w32(OV(0x0015f5f8), 0x01f80000u);
        return Args{{m}, {}};
    }, [](A a) { wrench_trail_draw(a.i[0]); return uint64_t{0}; });
    check("mirror_mesh_draw", 0x002d19b0, 200, Ret::None, [level1, with_class] {
        level1();
        const std::vector<uint32_t> ms = with_class(530);
        const uint32_t m = ms.empty() ? M2 : ms[0];
        if (ms.empty() || ri(0, 4) == 0) {  // the matrix: the camera's front, a turn about z
            w32(m + 0x78, OUT);
            const float a = rf(-3.1f, 3.1f), d = rf(3, 40);
            const float r[16] = {std::cos(a), std::sin(a), 0, 0, -std::sin(a), std::cos(a), 0, 0, 0, 0, 1, 0,
                                 ee_read<float>(OV(0x00166dc0)) + ee_read<float>(OV(0x00166c88)) * d, ee_read<float>(OV(0x00166dc4)) + ee_read<float>(OV(0x00166c98)) * d,
                                 ee_read<float>(OV(0x00166dc8)) + ee_read<float>(OV(0x00166ca8)) * d, 1};
            for (uint32_t i = 0; i < 16; ++i) wf(OUT + 4 * i, r[i]);
        }
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w32(pv + 0x40, u(ri(-2, 70)));
        w8(m + 0x20, u(ri(0, 3)));
        w8(m + 0xbc, R() & 1);
        if (coin()) for (uint32_t k = 0; k < 3; ++k) wf(m + 0x10 + 4 * k, ee_read<float>(OV(0x00166dc0) + 4 * k) + rf(-20, 20));
        wfrand(OV(0x001e0e30), 0xcc, 0, 1);
        w32(OV(0x001618f0), u(ri(0, 0x20)));
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x80))); w32(OV(0x0015f5f8), 0x01f80000u);
        return Args{{m}, {}};
    }, [](A a) { mirror_mesh_draw(a.i[0]); return uint64_t{0}; });
    check("smoke_wisps_draw", 0x002d73e8, 200, Ret::None, [level1, with_class] {
        level1();
        const std::vector<uint32_t> vents = with_class(760);
        if (vents.empty()) return Args{{0u}, {}};
        const uint32_t m = vents[u(ri(0, static_cast<int32_t>(vents.size()) - 1))];
        for (int k = 0; k < 2 && ee_read<uint8_t>(m + 0x20) == 0; ++k) smoke_vent_update(m);
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (pv == 0 || ee_read<int8_t>(m + 0x20) < 0) return Args{{0u}, {}};
        const int32_t first = ee_read<int16_t>(pv), end = first + ee_read<int16_t>(pv + 2) + ee_read<int16_t>(pv + 0x40);
        for (int32_t i = first; i < end && i < first + 64; ++i) {
            const uint32_t q = u(i);
            wf(OV(0x001d31c0) + 4 * q, rf(-8, 2));
            wf(OV(0x001d4480) + 4 * q, rf(0, 0.2f));
            w8(OV(0x001d2860) + q, R() & 1);
            w8(OV(0x001d2d10) + q, ri(0, 2) == 0 ? 1u : 0u);
            w16(OV(0x001d5740) + 2 * q, ri(0, 2) == 0 ? u(ri(1, 40)) : 0u);
            w16(OV(0x001d60a0) + 2 * q, ri(0, 2) == 0 ? u(ri(1, 40)) : 0u);
            w16(OV(0x001d6a00) + 2 * q, u(ri(0, 3)));
        }
        w16(pv + 0x42, ri(0, 3) == 0 ? 1u : 0u);
        wf(0x0015ed60, rf(0.5f, 2));
        w32(OV(0x00161998), u(ri(0, 0x80)));
        seed_rand();
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x40))); w32(OV(0x0015f5f8), 0x01f80000u);
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) smoke_wisps_draw(a.i[0]); return uint64_t{0}; });
    g_extra_noop_hooks.clear();
    // the screen-space effects
    check("screen_project", 0x001f6c38, 200, Ret::None, [level1, near_point] {
        level1(); near_point(OUT + 0x10, -10, 40); return Args{{OUT, OUT + 0x10}, {}};
    }, [](A a) { screen_project(a.i[0], a.i[1]); return uint64_t{0}; });
    auto effect_entry = [near_point](uint32_t e) {
        wrand(e, 12);
        near_point(e, 1, 30);
        wf(e + 0x10, rf(0, 2));
        w32(e + 0x18, u(ri(0, 0x20)));
        wf(e + 0x1c, rf(-3, 3));
        w16(e + 0x24, R() & 1);
        w16(e + 0x26, u(ri(-1, 6)));
        wf(e + 0x28, rf(-1, 1));
        w32(e + 0x2c, u(ri(0, 3)));
    };
    check("screen_effect_draw", 0x001edc38, 300, Ret::None, [level1, effect_entry] {
        level1(); effect_entry(OUT);
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x100))); w32(OV(0x0015f5f8), 0x01f80000u);
        return Args{{OUT}, {rf(-100, 600), rf(-100, 500)}};
    }, [](A a) { screen_effect_draw(a.i[0], a.f[0], a.f[1]); return uint64_t{0}; });
    check("screen_effects_draw", 0x001ee040, 300, Ret::None, [level1, effect_entry] {
        level1();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        const int n = ri(0, 4);
        for (int i = 0; i < 4; ++i) {
            const uint32_t e = OV(0x00169040) + u(i) * 0x30;
            effect_entry(e);
            const uint32_t m = first + 0x100 * u(ri(0, 60));
            w32(e + 0x20, ri(0, 5) == 0 ? 0u : m);
            if (ri(0, 6) == 0) w8(m + 0x20, coin() ? 0xfeu : 0xfdu);
        }
        w32(OV(0x00169100), u(n));
        w32(0x001413d4, ri(0, 8) == 0 ? 0x72u : 0u);
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x100))); w32(OV(0x0015f5f8), 0x01f80000u);
        return Args{};
    }, [](A) { screen_effects_draw(); return uint64_t{0}; });
    check("lens_flare_draw", 0x001ed880, 300, Ret::None, [level1, near_point] {
        level1();
        static constexpr OvConst<0x00167040u> S{};
        const uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(0, 60));
        near_point(m + 0x10, 1, 60);
        if (ri(0, 6) == 0) w8(m + 0x20, 0x80 | R());
        w32(S, m);
        w32(S + 4, ri(0, 5) == 0 ? 0u : 1u);
        wf(S + 8, ri(0, 5) == 0 ? 0.0f : rf(0, 1));
        for (uint32_t i = 0; i < 16; ++i) {
            w16(S + 0x10 + 2 * i, u(ri(0, 0x80)));
            w16(S + 0x30 + 2 * i, u(ri(0, 0x18)));
            w32(S + 0x50 + 4 * i, R());
            wf(S + 0x90 + 4 * i, rf(-1, 2));
            wf(S + 0xd0 + 4 * i, rf(0, 2));
        }
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x100))); w32(OV(0x0015f5f8), 0x01f80000u);
        return Args{};
    }, [](A) { lens_flare_draw(); return uint64_t{0}; });
    // The VU1 program: the chain the draw wrote, walked from the same random VU1 state once with
    // the VU interpreter running the uploaded original and once with the port; the kicked GIF
    // packets, the VU1 data memory and registers (vf, vi, ACC, Q; not the flags) compared.
    if (std::getenv("ORACLE_ONLY") == nullptr || std::string(std::getenv("ORACLE_ONLY")) == "particles_vu1") {
        particles_vu1_register();
        const int cases = 400;
        int ok = 0, shown = 0, kicks = 0;
        for (int c = 0; c < cases; ++c) {
            level1();
            particles(c >= 300);
            const uint32_t ring0 = 0x01e10000u + 0x10 * u(ri(0, 0x100));
            w32(OV(0x001611c0), ring0);
            particles_draw();
            const uint32_t ring1 = ee_read<uint32_t>(OV(0x001611c0));
            vu1::Vu1 init;
            for (auto& v : init.st.vf) for (uint32_t& w : v.u) w = R();
            for (auto& v : init.st.vi) v = static_cast<int16_t>(R());
            for (uint32_t& w : init.st.acc.u) w = R();
            for (size_t k = 0; k < init.st.mem.size(); k += 4) { const uint32_t r = R(); std::memcpy(&init.st.mem[k], &r, 4); }
            init.st.vf[0].u[0] = init.st.vf[0].u[1] = init.st.vf[0].u[2] = 0;
            init.st.vf[0].f[3] = 1.0f;
            init.st.vi[0] = 0;
            init.dbf = coin();
            auto run = [&](bool interp, std::vector<uint8_t>& out) {
                vu1::vu1_global() = init;
                vu1::g_force_interpreter = interp;
                vu1::g_kick_tap = [&out](const uint8_t* d, uint32_t q) { out.insert(out.end(), d, d + q * 16); };
                if (ring1 != ring0) gs::gs_global().vif1_chain(ring0, ring1);
                vu1::g_kick_tap = nullptr;
                vu1::g_force_interpreter = false;
                return vu1::vu1_global();
            };
            std::vector<uint8_t> ka, kb;
            const vu1::Vu1 a = run(true, ka);
            const vu1::Vu1 b = run(false, kb);
            if (!ka.empty()) ++kicks;
            std::string why;
            if (ka != kb) why = "kicked packets (" + std::to_string(ka.size()) + " / " + std::to_string(kb.size()) + " bytes)";
            else if (a.st.mem != b.st.mem) why = "data memory";
            else if (std::memcmp(a.st.vf.data(), b.st.vf.data(), sizeof(a.st.vf)) != 0) why = "vf";
            else if (a.st.vi != b.st.vi) why = "vi";
            else if (std::memcmp(a.st.acc.u, b.st.acc.u, 16) != 0) why = "acc";
            else if (std::memcmp(&a.st.q, &b.st.q, 4) != 0) why = "q";
            if (why.empty()) { ++ok; continue; }
            if (shown++ < 3) {
                std::printf("  [particles_vu1] case %d: %s differ\n", c, why.c_str());
                if (ka != kb)
                    for (size_t k = 0; k < std::min(ka.size(), kb.size()); k += 4)
                        if (std::memcmp(&ka[k], &kb[k], 4) != 0) { std::printf("    first at byte %zu: %08x vs %08x\n", k, r32(ka[k] | ka[k+1] << 8 | ka[k+2] << 16 | static_cast<uint32_t>(ka[k+3]) << 24), r32(kb[k] | kb[k+1] << 8 | kb[k+2] << 16 | static_cast<uint32_t>(kb[k+3]) << 24)); break; }
                for (int r = 0; r < 32 && why == "vf"; ++r)
                    for (int l = 0; l < 4; ++l)
                        if (a.st.vf[r].u[l] != b.st.vf[r].u[l]) std::printf("    vf%d.%c %08x vs %08x\n", r, "xyzw"[l], a.st.vf[r].u[l], b.st.vf[r].u[l]);
                for (int r = 0; r < 16 && why == "vi"; ++r)
                    if (a.st.vi[r] != b.st.vi[r]) std::printf("    vi%d %04x vs %04x\n", r, static_cast<uint16_t>(a.st.vi[r]), static_cast<uint16_t>(b.st.vi[r]));
                for (size_t k = 0; k < a.st.mem.size() && why == "data memory"; k += 4)
                    if (std::memcmp(&a.st.mem[k], &b.st.mem[k], 4) != 0) { std::printf("    first at %zx\n", k); break; }
            }
        }
        std::printf("%-26s 0x%08x  %s (%d/%d, %d with kicks)\n", "particles_vu1", 0x00101088u, ok == cases ? "ok" : "FAIL", ok, cases, kicks);
        if (ok != cases) ++g_fail;
    }
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
