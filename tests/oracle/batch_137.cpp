// Oracle batch 137 (task 0384): Aridia's ocean (the sand sea) and the background ships' glows (aridia_ocean_asm.cpp), Aridia-only code:
// run with ORACLE_LEVEL=2 build/tests/ovl_oracle --batch 137 (needs the ISO); on other levels every check is skipped.
// On Aridia's own data after 1 / 40 / 150 gameplay frames (the render frame's view set up): the five surface records 0x1f3c00..
// with their cells' visibility bytes and the camera moved over / about them (cells seen, clipped, culled), the animated texture's frame
// counter and record varied, the gameplay flag 0x15f608 (the ocean's boxes tested or not), the glows' ships and their colours.

// Audit 2 (2026-11-13): Skid's HUD draw with its packets in a test-area buffer, the bar's width +8 and the screen's height
// 0x13e504 varied.

#include "ovl_oracle.h"

#include <game/ovl/particle_16_spawn.h>
#include <game/ovl/aridia_classes_tr.h>
#include <game/ovl/aridia_fx.h>
#include <game/ovl/aridia_hud.h>
#include <game/ovl/aridia_ocean.h>
#include <game/ovl/aridia_ship.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l2.h>

#include <runtime/vu/vu0.h>

#include <cmath>
#include <cstring>

namespace rac1::oracle {

namespace {
// A check whose original is Aridia's own function at `l2addr`.
void l2check(const char* name, uint32_t l2addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (g_oracle_level != 2) {
        std::printf("%-26s L2:0x%08x  skipped (Aridia only: run with ORACLE_LEVEL=2)\n", name, l2addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L2:0x%08x\n", name, l2addr);
    if (!check_fn(name, l2addr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}

// The VU0 the original started from (the interpreter's), given to the port before it runs (as batch 128)
VUState g_vu0_in;
void vu0_sync() {
    VUState& v = vu0::global().st;
    v.vf = g_vu0_in.vf;
    v.vi = g_vu0_in.vi;
    v.acc = g_vu0_in.acc;
    v.q = g_vu0_in.q;
    v.i = g_vu0_in.i;
    v.r = g_vu0_in.r;
    v.p = g_vu0_in.p;
    v.macflag = g_vu0_in.macflag;
    v.statusflag = g_vu0_in.statusflag;
    v.clipflag = g_vu0_in.clipflag;
    std::memcpy(v.mem.data(), g_vu0_in.mem.data(), 0x1000);
}

constexpr uint32_t SPR = 0x70000000u;
constexpr OvConst<0x00166dc0u> CAM{};  // the camera's position (Aridia's 0x1673c0)
// the surface records (0x60 / 0x70 apart: 0x1f3c00 .. 0x1f3e10) the draw callback passes to L2:0x2a48a0
constexpr uint32_t RECS[] = {ORACLE_LADDR(2, 0x001f3c00u), ORACLE_LADDR(2, 0x001f3c60u), ORACLE_LADDR(2, 0x001f3cd0u), ORACLE_LADDR(2, 0x001f3d30u),
                             ORACLE_LADDR(2, 0x001f3d90u), ORACLE_LADDR(2, 0x001f3dd0u), ORACLE_LADDR(2, 0x001f3e10u)};
constexpr uint32_t FRAME = ORACLE_LADDR(2, 0x0015f5ccu);  // the frame counter (the texture's animation)
constexpr uint32_t GFLAG = ORACLE_LADDR(2, 0x0015f608u);  // the gameplay flag the draw tests (0: the boxes decide)

// the camera moved over / near a point, its view rows turned about z; the render frame's view made again
void move_camera(float x, float y, float z, float spread) {
    Vec4& c = VV(CAM);
    c.x = x + rf(-spread, spread);
    c.y = y + rf(-spread, spread);
    c.z = z + rf(0.5f, spread / 3);
    if (coin()) {
        const float a = rf(-3.14f, 3.14f), cs = std::cos(a), sn = std::sin(a);
        for (uint32_t r = OV(0x00166fd0u); r < OV(0x00166fd0u) + 0x30; r += 0x10) {
            Vec4& v = VV(r);
            const float px = v.x, py = v.y;
            v.x = cs * px - sn * py;
            v.y = sn * px + cs * py;
        }
    }
    render_view_setup();
}

// the camera moved (and turned) about a point until the port's screen test (point_on_screen, 0x2002e0) sees it: the glows are drawn
void see(uint32_t p, float spread) {
    for (int k = 0; k < 80; ++k) {
        move_camera(ee_read<float>(p) + rf(-spread, spread), ee_read<float>(p + 4) + rf(-spread, spread), ee_read<float>(p + 8), spread / 2);
        if (point_on_screen(p, 0.0f) != -1) return;
    }
}

// a stub the original runs a class update through N times (a0 = the moby, a1 = N, a2 = the function: batch 109's), in unused RAM below
// the stack
constexpr uint32_t kStub = 0x01fe0000;
void write_stub() {
    static const uint32_t stub[] = {0x27bdffd0, 0xafbf0010, 0xafb00014, 0xafb10018, 0xafb2001c, 0x00808021, 0x00a08821, 0x00c09021,
                                    0x02002021, 0x0240f809, 0x00000000, 0x2631ffff, 0x1e20fffb, 0x00000000, 0x8fb2001c, 0x8fb10018,
                                    0x8fb00014, 0x8fbf0010, 0x27bd0030, 0x03e00008, 0x00000000};
    for (size_t i = 0; i < sizeof stub / sizeof stub[0]; ++i) ee_write<uint32_t>(kStub + 4 * static_cast<uint32_t>(i), stub[i]);
}

// the camera over a random point of a record's grid (origin +0, step +0x10, counts +0x18 / +0x1a)
void camera_over(uint32_t rec, float spread) {
    const float ox = ee_read<float>(rec), oy = ee_read<float>(rec + 4), oz = ee_read<float>(rec + 8);
    const float st = ee_read<float>(rec + 0x10);
    const float w = st * ee_read<uint16_t>(rec + 0x18), h = st * ee_read<uint16_t>(rec + 0x1a);
    move_camera(ox + rf(0.0f, w), oy + rf(0.0f, h), oz + 2.0f, spread);
}
}  // namespace

void batch_137() {
    if (g_only_batch != 137) return;
    g_batch = 137;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    // the w lanes of the segment collision's results (Veldin 0x1efc70's end: vf25..vf28 x 1/1024 stored at 0x174460 / 0x174480 / 0x174490 on
    // Aridia): stale VU0 lanes the port does not carry (-0 against 0)
    for (uint32_t a : {0x0017446cu, 0x0017448cu, 0x0017449cu}) g_ignored_words.push_back(ORACLE_LADDR(2, a));
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime

    static std::vector<std::vector<uint8_t>> snaps;
    static int snap_level = -1;
    auto level = [](int which = -1) {
        uint8_t* ram = ee_get_ram_base();
        if (snaps.empty() || snap_level != g_oracle_level) {
            snaps.clear();
            veldin_boot();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            int frame = 0;
            for (int target : {1, 40, 150}) {
                while (frame < target) { gameplay_frame(); ++frame; }
                render_view_setup();  // (the view matrices of the render frame: zero after an update alone)
                snaps.emplace_back(ram, ram + 0x02000000);
            }
            snap_level = g_oracle_level;
        }
        const size_t k = which >= 0 ? static_cast<size_t>(which) : static_cast<size_t>(ri(0, static_cast<int>(snaps.size()) - 1));
        std::memcpy(ram, snaps[k].data(), snaps[k].size());
    };
    if (g_oracle_level != 2) {  // (no snapshot: every check below is skipped)
        l2check("ari_ocean_draw", 0x002ea048, 0, Ret::None, [] { return Args{}; }, [](A) { return uint64_t{0}; });
        g_ee_null_is_ram = null_ram;
        g_ignored_words = saved_ignored;
        return;
    }
    level(0);
    // the level's mobys whose update function is `fn` (Aridia's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    const std::vector<uint32_t> oceans = mobys_with(L2_ADDR(0x002ea198u));
    const std::vector<uint32_t> ships1 = mobys_with(L2_ADDR(0x002ec4b8u)), ships2 = mobys_with(L2_ADDR(0x002ece18u));
    std::vector<uint32_t> c1208;
    for (int s = 0; s < 3 && c1208.empty(); ++s) {
        level(s);
        c1208 = mobys_with(L2_ADDR(0x002ebf20u));
    }
    std::printf("  ocean mobys %zu, ships %zu / %zu, class 1208 %zu\n", oceans.size(), ships1.size(), ships2.size(), c1208.size());
    // the port's VU0 micro memory as the game leaves it (as batch 128)
    vu0::vif0_send_chain(0x00100080, true);
    vu0::vif0_send_chain(0x0010e4c0, true);
    g_pre_call = [](EeInterpreter& ee) {
        const VUState& o = ee.vu0();
        g_vu0_in.vf = o.vf;
        g_vu0_in.vi = o.vi;
        g_vu0_in.acc = o.acc;
        g_vu0_in.q = o.q;
        g_vu0_in.i = o.i;
        g_vu0_in.r = o.r;
        g_vu0_in.p = o.p;
        g_vu0_in.macflag = o.macflag;
        g_vu0_in.statusflag = o.statusflag;
        g_vu0_in.clipflag = o.clipflag;
        std::memcpy(g_vu0_in.mem.data(), o.mem.data(), 0x1000);
    };
    // a record of the ocean, the camera over it (or near: some cells culled)
    auto record = [level](bool far) -> uint32_t {
        level();
        const uint32_t r = RECS[ri(0, 6)];
        camera_over(r, far ? 120.0f : 15.0f);
        return r;
    };

    l2check("ari_ocean_cells_clip", 0x0024f748, 150, Ret::None, [record] { return Args{{record(coin())}, {}}; },
            [](A a) { vu0_sync(); ari_ocean_cells_clip_l2(a.i[0]); return uint64_t{0}; });
    l2check("ari_ocean_cells_draw", 0x0024fa80, 150, Ret::None, [record] {
        const uint32_t r = record(coin());
        const int n = ee_read<uint16_t>(r + 0x18) / 7 * (ee_read<uint16_t>(r + 0x1a) / 7) + 1;
        for (int i = 0; i < n && i < 0x800; ++i) w8(SPR + 0x2000 + u(i), static_cast<uint8_t>(ri(0, 3) == 0 ? 0 : ri(1, 2)));  // the cells' bytes
        for (uint32_t a = SPR; a < SPR + 0x200; a += 4) wf(a, rf(-300.0f, 300.0f));
        return Args{{r}, {}};
    }, [](A a) { vu0_sync(); ari_ocean_cells_draw_l2(a.i[0]); return uint64_t{0}; });
    l2check("ari_ocean_record", 0x002a48a0, 150, Ret::None, [record] { return Args{{record(coin())}, {}}; },
            [](A a) { vu0_sync(); ari_ocean_record_l2(a.i[0]); return uint64_t{0}; });
    l2check("ari_ocean_end", 0x002a4880, 40, Ret::None, [level] {
        level();
        wfrand(L2_ADDR(0x001612f8u), 4, 0.0f, 300000.0f);  // the saved fog / far
        return Args{};
    }, [](A) { vu0_sync(); ari_ocean_end_l2(); return uint64_t{0}; });
    // the set-up record as L2:0x2a4818 builds it (colour bytes +0x30.., period +0x33, the texture frames +0x3c / +0x3e / +0x3f, the
    // texture record +0x1c, the fog / far floats +0x20..), its values about the draw callback's (15 / 15 / 25 / 30, 0x28 / 0x40)
    l2check("ari_ocean_vu7_setup", 0x002a40f0, 150, Ret::None, [level] {
        level();
        camera_over(RECS[ri(0, 6)], 100.0f);
        const uint32_t r = T + 0x400;
        for (uint32_t k = 0; k < 3; ++k) w8(r + 0x30 + k, static_cast<uint8_t>(R()));
        w8(r + 0x33, static_cast<uint8_t>(ri(1, 60)));
        wf(r + 0x20, coin() ? 0.0f : rf(0.0f, 50.0f));
        wf(r + 0x24, rf(1000.0f, 300000.0f));
        wf(r + 0x28, coin() ? 255.0f : rf(0.0f, 255.0f));
        wf(r + 0x2c, coin() ? 0.0f : rf(0.0f, 255.0f));
        w16(r + 0x3c, 0x28);
        w8(r + 0x3e, static_cast<uint8_t>(ri(1, 0x40)));
        w8(r + 0x3f, static_cast<uint8_t>(ri(0x40, 0xff)));
        w32(r + 0x1c, L2_ADDR(0x001f3e80u));
        if (coin()) w32(FRAME, R() & 0xffffu);
        if (ri(0, 2) == 0) w32(L2_ADDR(0x0015f458u), u(ri(0, 2)));  // (the fog's mode the set-up tests)
        return Args{{r}, {}};
    }, [](A a) { vu0_sync(); ari_ocean_vu7_setup_l2(a.i[0]); return uint64_t{0}; });
    l2check("ari_spr_dma_to",0x002101b8, 40, Ret::None, [level] {
        level();
        wrand(T, 0x400);
        return Args{{T + u(ri(0, 15)) * 16, SPR + u(ri(0, 0x300)) * 16, u(ri(1, 0xf8))}, {}};
    }, [](A a) { ari_spr_dma_to_l2(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
    if (!oceans.empty()) {
        l2check("ari_ocean_draw", 0x002ea048, 150, Ret::None, [level, &oceans] {
            level();
            camera_over(RECS[ri(0, 6)], coin() ? 15.0f : 150.0f);
            if (coin()) w32(FRAME, R() & 0xffffu);
            if (ri(0, 2) == 0) w32(GFLAG, coin() ? 0u : 1u);
            return Args{{oceans[0]}, {}};
        }, [](A a) { vu0_sync(); ari_ocean_draw_tr_l2(a.i[0]); return uint64_t{0}; });
    }

    // ---- the sparks (Aridia's copy of Veldin's 0x26ced0: since the regeneration of task 0411 the files call the port
    // particle_16_spawn, task 0383, as the generator retires the transliterated copy; the Rocks spawn them):
    // a0 the position, a1 the velocity, a2 / a3 (the Rocks pass *(0x161cc4) / *(0x161cc8)), t0 the life, f12 the size
    l2check("ari_sparks", 0x0026e548, 150, Ret::None, [level] {
        level();
        const Vec4 c = VV(CAM);
        Vec4& p = VV(T);
        p.x = c.x + rf(-20.0f, 20.0f);
        p.y = c.y + rf(-20.0f, 20.0f);
        p.z = c.z + rf(-5.0f, 5.0f);
        wfrand(T + 0x10, 4, -1.0f, 1.0f);
        if (ri(0, 3) == 0) w32(OV(0x00160170u), u(ri(0x7f0, 0x800)));  // (the particle pool nearly full / full)
        return Args{{T, T + 0x10, ee_read<uint32_t>(L2_ADDR(0x00161cc4u)), ee_read<uint32_t>(L2_ADDR(0x00161cc8u)), u(ri(1, 400))}, {rf(0.05f, 3.0f)}};
    }, [](A a) { particle_16_spawn(a.i[0], a.i[1], a.i[2], a.i[3], static_cast<int32_t>(a.i[4]), a.f[0]); return uint64_t{0}; });

    // ---- class 1633 (spawned by L2:0x2efdd0: a parent moby, a position, f12..f15 its pvar +8 / +0xc and angles +0x48 / +0x44): one
    // spawned by the port's spawner in the generator (the same moby for both sides), its frame counter / life varied, updated once
    {
        level(0);
        const std::vector<uint32_t> parents = mobys_with(L2_ADDR(0x002f0f88u));  // (the Sandmouse, whose code spawns them)
        l2check("ari_class1633_update", 0x002efee0, 120, Ret::None, [level, parents] {
            level();
            const uint32_t par = parents.empty() ? ee_read<uint32_t>(OV(0x0015ffd8)) : parents[0];
            Vec4& p = VV(T);
            p = coin() ? VV(par + 0x10) : VV(CAM);  // (near the camera: within its 64 the moby's ray test to the hero runs)
            p.x += rf(-20.0f, 20.0f);
            p.y += rf(-20.0f, 20.0f);
            if (coin()) {  // the hero near it
                Vec4& h = VV(0x0013f3d0u);
                h.x = p.x + rf(-4.0f, 4.0f);
                h.y = p.y + rf(-4.0f, 4.0f);
                h.z = p.z + rf(-1.0f, 2.0f);
            }
            const uint32_t m = lv2_2efdd0_l2(par, T, 0, 0, 0, 0, 0, 0, rf(-2.0f, 2.0f), rf(-2.0f, 2.0f), rf(-3.1f, 3.1f), rf(-3.1f, 3.1f), 0, 0, 0, 0).v0;
            if (m != 0) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                if (coin()) w16(pv + 0x14, static_cast<uint16_t>(ri(0, 80)));  // its life
                if (coin()) wf(m + 0x2c, ee_read<float>(ee_read<uint32_t>(m + 0x24) + 0x24) * rf(0.2f, 1.3f));  // its scale about the class's limit
            }
            if (coin()) w32(FRAME, R() & 0xffffu);
            return Args{{m != 0 ? m : par}, {}};
        }, [](A a) { vu0_sync(); class1633_update_l2(a.i[0]); return uint64_t{0}; });
    }

    // ---- Skid's HUD element (aridia_hud_asm.cpp): a HUD slot (5 or 7) as Skid's request leaves it, its fades (+0x70 / +0x71), timer
    // (+0x7c), place (+0x50 / +0x54) and counter (*(+0xc) -> +0x74) varied
    {
        auto slot = [level]() -> uint32_t {
            level();
            const uint32_t e = OV(0x0017dc50u) + (coin() ? 5u : 7u) * 0x90;
            w32(T + 0x40, u(ri(0, 120)));  // the counter it shows
            w32(e + 0xc, T + 0x40);
            w8(e + 0x70, static_cast<uint8_t>(ri(0, 9)));
            w8(e + 0x71, static_cast<uint8_t>(ri(0, 9)));
            w32(e + 0x7c, u(ri(0, 40)));
            w32(e + 0x74, u(ri(0, 120)));
            w32(e + 0x50, u(ri(0, 0x200)));
            w32(e + 0x54, u(ri(0, 0x1c0)));
            if (coin()) w32(e + 0x6c, u(ri(-6, 0x1e)));
            if (coin()) w32(OV(0x0015ed80u), u(ri(0, 1)));  // (the widescreen flag its place reads)
            if (coin()) w32(e + 8, u(ri(0, 20)));  // (the bar's width: below 10 the short one)
            if (coin()) w32(0x0013e504, coin() ? 448u : u(ri(0x60, 0x200)));  // (the screen's height the bar stands on)
            w32(OV(0x001611c0), TEST_AREA + 0x8000 + u(ri(0, 0x40)) * 0x10);  // (the packets on a buffer of the test area)
            return e;
        };
        l2check("ari_skid_hud_start", 0x0023c990, 60, Ret::None, [slot] { return Args{{slot()}, {}}; },
                [](A a) { ari_skid_hud_start_l2(a.i[0]); return uint64_t{0}; });
        l2check("ari_skid_hud_update", 0x0023ccc8, 100, Ret::None, [slot] { return Args{{slot()}, {}}; },
                [](A a) { ari_skid_hud_update_l2(a.i[0]); return uint64_t{0}; });
        l2check("ari_skid_hud_draw", 0x0023cda8, 150, Ret::None, [slot] { return Args{{slot()}, {}}; },
                [](A a) { ari_skid_hud_draw_l2(a.i[0]); return uint64_t{0}; });
    }

    // ---- the ship's glows (Aridia's copies of Veldin's 0x28e670 / 0x28efc8, aridia_ship_asm.cpp): the ship *(0x13e030), the camera
    // near it, its glow fields (+0xb2 / +0xbc, its pvars) and colour (+0x90) varied
    {
        level(0);
        const uint32_t ship = ee_read<uint32_t>(0x0013e030u);
        std::printf("  ship %08x class %d\n", ship, ship != 0 ? ee_read<int16_t>(ship + 0xa6) : -1);
        if (ship != 0) {
            auto prep = [level, ship]() -> uint32_t {
                level();
                if (ri(0, 3) != 0) see(ship + 0x10, coin() ? 12.0f : 30.0f);  // (the ship seen: its glows drawn)
                else move_camera(ee_read<float>(ship + 0x10) + rf(-30.0f, 30.0f), ee_read<float>(ship + 0x14) + rf(-30.0f, 30.0f), ee_read<float>(ship + 0x18), 10.0f);
                if (coin()) w8(ship + 0xb2, static_cast<uint8_t>(R()));
                if (coin()) w8(ship + 0xbc, static_cast<uint8_t>(R()));
                if (coin()) w32(ship + 0x90, R());
                const uint32_t pv = ee_read<uint32_t>(ship + 0x78);
                if (pv != 0 && coin()) wfrand(pv, 16, -2.0f, 2.0f);
                if (coin()) w32(FRAME, R() & 0xffffu);
                if (coin()) wf(L2_ADDR(0x0015f400u), rf(0.0f, 1.0f));  // (the glow's pulse)
                if (coin()) {  // the landing / take-off (game mode 6, its step 3 and frame: the glow fading)
                    w32(L2_ADDR(0x0015f5c4u), 6);
                    w32(0x0013e050u, coin() ? 3u : u(ri(0, 5)));
                    w16(0x0013e054u, static_cast<uint16_t>(ri(0, 600)));
                }
                return ship;
            };
            l2check("ari_ship_glow_draw", 0x0028dde0, 120, Ret::None, [prep] { return Args{{prep()}, {}}; },
                    [](A a) { vu0_sync(); ari_ship_glow_draw_l2(a.i[0]); return uint64_t{0}; });
            l2check("ari_ship_engine_glow_draw", 0x0028e738, 120, Ret::None, [prep] { return Args{{prep()}, {}}; },
                    [](A a) { vu0_sync(); ari_ship_engine_glow_draw_l2(a.i[0]); return uint64_t{0}; });
            l2check("ari_ship_takeoff_draw", 0x0028e9a8, 150, Ret::None, [prep, ship] {
                prep();
                // the exhaust trails: two rings of 32 points (0x13e0f0 / 0x13e2f0, 16 bytes each) behind the ship, the head
                // 0x13e080, the count 0x13e084
                if (ri(0, 4) != 0) {
                    w32(0x0013e084u, u(ri(0, 32)));
                    w32(0x0013e080u, u(ri(0, 63)));
                    const Vec4 s = VV(ship + 0x10);
                    for (uint32_t k = 0; k < 64; ++k) {
                        Vec4& q = VV(0x0013e0f0u + k * 0x10);
                        q.x = s.x + rf(-8.0f, 8.0f);
                        q.y = s.y + rf(-8.0f, 8.0f);
                        q.z = s.z + rf(-3.0f, 6.0f);
                        q.w = 1.0f;
                    }
                }
                return Args{{ship}, {}};
            },
                    [](A a) { vu0_sync(); ari_ship_takeoff_draw_l2(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- class 1005, the Trespasser pickup (aridia_classes_tr.cpp; batch 109 runs it too): one update from each state but the item scene's
    // start (states 1 / 2 with the hero within 1) and the level change (state 5), the item owned or not, the hero about it, the frame counter
    {
        level(0);
        const std::vector<uint32_t> ms = mobys_with(L2_ADDR(0x002ea210u));
        if (!ms.empty()) {
            l2check("trespasser_gadget_1", 0x002ea210, 200, Ret::None, [level, ms] {
                level();
                const uint32_t m = ms[0];
                const int st = ri(0, 4);
                w8(m + 0x20, static_cast<uint8_t>(st));
                w8(0x0013d4e8u + 0x1a, ri(0, 2) == 0 ? 1u : 0u);  // (the item owned)
                Vec4 q = VV(m + 0x10);
                const float r = (st == 1 || st == 2) ? rf(1.02f, 4.0f) : rf(0.0f, 4.0f), a = rf(-3.1f, 3.1f);  // (states 1 / 2 within 1: the item scene)
                q.x += r * std::cos(a);
                q.y += r * std::sin(a);
                q.z += rf(-2.5f, 2.5f);
                VV(0x0013f3d0u) = q;
                if (coin()) wf(0x00174468u, q.z);  // (the height it rises to)
                if (coin()) wf(m + 0x48, rf(-3.1f, 3.1f));
                // states 3 / 4: the screen fade 0x15f3fc (to 1, then back to 0), the flag 0x1413f4, the game mode (2: none of it)
                if (ri(0, 3) != 0) wf(L2_ADDR(0x0015f3fcu), coin() ? (coin() ? 1.0f : 0.0f) : rf(0.0f, 1.0f));
                if (coin()) w8(0x001413f4u, static_cast<uint8_t>(ri(0, 1)));
                if (ri(0, 4) == 0) w32(L2_ADDR(0x0015f5c4u), 2);
                // state 4: the help message's record 0x179a10 (+0 none / +0x24 -1: done) and its pvar +0x40 (> 0.2: still turning)
                if (coin()) w32(L2_ADDR(0x00179a10u), coin() ? 0u : 1u);
                if (coin()) w32(L2_ADDR(0x00179a34u), coin() ? 0xffffffffu : 0u);
                if (coin()) wf(ee_read<uint32_t>(m + 0x78) + 0x40, rf(0.0f, 1.0f));
                w32(L2_ADDR(0x0015f5ccu), coin() ? u(ri(0, 100) * 10) : (R() & 0xffffu));
                return Args{{m}, {}};
            }, [](A a) { trespasser_gadget_update_l2(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- classes nothing in the snapshots holds: the Rocks' pieces 696..698 (spawned by the port's L2:0x2dd4d8 as the Rocks do: a
    // position, a velocity, a life 60..180, f12 0.15.., f13 / f14, f15 0.75), and classes 437 (Sand Mouse) / 755 (Sandshark Egg), which
    // nothing on Aridia spawns (a bare moby_spawn): one update from a varied state, the camera / hero about them
    {
        struct Sp {
            const char* name;
            uint32_t addr;
            void (*fn)(uint32_t);
            int cls;
        };
        for (const Sp& sp : {Sp{"ari_rock_update", 0x002dd6c0, rock_update_l2, 696}, Sp{"ari_sand_mouse_update", 0x002ccc08, sand_mouse_update_l2, 437},
                             Sp{"ari_shark_egg_update", 0x002e0280, shark_egg_update_l2, 755}}) {
            l2check(sp.name, kStub, 120, Ret::None, [level, sp] {
                level();
                const Vec4 c = VV(CAM);
                Vec4& p = VV(T);
                p.x = c.x + rf(-15.0f, 15.0f);
                p.y = c.y + rf(-15.0f, 15.0f);
                p.z = c.z + rf(-3.0f, 3.0f);
                p.w = 1.0f;
                uint32_t m = 0;
                if (sp.cls == 696) {
                    wfrand(T + 0x10, 3, -0.3f, 0.3f);
                    m = lv2_2dd4d8_l2(T, T + 0x10, u(696 + ri(0, 2)), u(ri(60, 180)), 0, 0, 0, 0, rf(0.15f, 0.3f), rf(0.5f, 1.5f), rf(0.5f, 1.5f), 0.75f,
                                      0, 0, 0, 0).v0;
                } else {
                    m = moby_spawn(u(sp.cls));
                    if (m != 0) {
                        VV(m + 0x10) = p;
                        if (ee_read<uint32_t>(m + 0x78) == 0) w32(m + 0x78, T + 0x200);  // (no spawner gives it pvars: a block of zeros,
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);                   // not a slot's leftovers)
                        for (uint32_t k = 0; k < 0x80; k += 4) w32(pv + k, 0);
                        // (the egg's state 2 animates a moby its pvars name, which only a spawner would set: states 0 / 1)
                        w8(m + 0x20, static_cast<uint8_t>(sp.cls == 755 ? ri(0, 1) : ri(0, 3)));
                    }
                }
                if (m == 0) m = ee_read<uint32_t>(OV(0x0015ffd8));
                if (coin()) {  // the hero about it
                    Vec4& h = VV(0x0013f3d0u);
                    h = VV(m + 0x10);
                    h.x += rf(-6.0f, 6.0f);
                    h.y += rf(-6.0f, 6.0f);
                }
                if (coin()) w32(FRAME, R() & 0xffffu);
                write_stub();
                return Args{{m, u(ri(1, sp.cls == 696 ? 40 : 6)), sp.addr}, {}};  // (the update 1..N times, the original through the stub)
            }, [sp](A a) { vu0_sync(); for (uint32_t i = 0; i < a.i[1]; ++i) sp.fn(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- the Elevator's draw callback (aridia_fx_asm.cpp): the level's elevators, the camera seeing them, their height / glow fields
    // (pvar +0xb0 / +0xb8) varied
    {
        level(0);
        const std::vector<uint32_t> els = mobys_with(L2_ADDR(0x002dbd38u));
        if (!els.empty()) {
            l2check("ari_elevator_draw", 0x002dc2c8, 150, Ret::None, [level, els] {
                level();
                const uint32_t m = els[static_cast<size_t>(ri(0, static_cast<int>(els.size()) - 1))];
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                if (coin()) wf(pv + 0xb0, rf(-3.1f, 3.1f));
                if (coin()) wf(pv + 0xb8, rf(-2.0f, 6.0f));
                if (ri(0, 3) != 0) see(m + 0x10, coin() ? 8.0f : 25.0f);
                if (coin()) w32(FRAME, R() & 0xffffu);
                return Args{{m}, {}};
            }, [](A a) { vu0_sync(); ari_elevator_draw_l2(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- class 1143, the Vendor Orb (Aridia's copy of Veldin's unported 0x2df730, aridia_fx_asm.cpp): the orbs the Vendor holds, their
    // state / timer and the camera / hero about them
    {
        level(0);
        const std::vector<uint32_t> orbs = mobys_with(L2_ADDR(0x002ebcb8u));
        std::printf("  vendor orbs %zu\n", orbs.size());
        if (!orbs.empty()) {
            l2check("ari_vendor_orb_update", 0x002ebcb8, 120, Ret::None, [level, orbs] {
                level();
                const uint32_t m = orbs[static_cast<size_t>(ri(0, static_cast<int>(orbs.size()) - 1))];
                if (coin()) w8(m + 0x20, static_cast<uint8_t>(ri(0, 3)));
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                if (pv != 0 && coin()) w32(pv, u(ri(0, 60)));
                if (coin()) {
                    Vec4& h = VV(0x0013f3d0u);
                    h = VV(m + 0x10);
                    h.x += rf(-5.0f, 5.0f);
                    h.y += rf(-5.0f, 5.0f);
                }
                if (coin()) w32(FRAME, R() & 0xffffu);
                return Args{{m}, {}};
            }, [](A a) { vu0_sync(); ari_vendor_orb_update_l2(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- the glows
    l2check("ari_glow_quad", 0x00264b50, 150, Ret::None, [level] {
        level();
        const Vec4 c = VV(CAM);
        Vec4& p = VV(T);
        p.x = c.x + rf(-60.0f, 60.0f);
        p.y = c.y + rf(-60.0f, 60.0f);
        p.z = c.z + rf(-10.0f, 10.0f);
        p.w = 1.0f;
        return Args{{T, R()}, {rf(0.1f, 4.0f), rf(0.0f, 1.0f)}};
    }, [](A a) { vu0_sync(); ari_glow_quad_l2(a.i[0], a.i[1], a.f[0], a.f[1]); return uint64_t{0}; });
    struct Ship {
        const char* name;
        uint32_t addr;
        void (*fn)(uint32_t);
        const std::vector<uint32_t>* ms;
        uint32_t col;  // the glows' colour pvar (ship 1: +0xb0, three glows; ship 2: +0xe0, six)
    };
    for (const Ship& s : {Ship{"ari_bg_ship1_glow_draw", 0x002ec308, ari_bg_ship1_glow_draw_l2, &ships1, 0xb0},
                          Ship{"ari_bg_ship2_glow_draw", 0x002ecc68, ari_bg_ship2_glow_draw_l2, &ships2, 0xe0}}) {
        if (s.ms->empty()) continue;
        l2check(s.name, s.addr, 80, Ret::None, [level, s] {
            level();
            for (uint32_t m : *s.ms) {  // their glows' colours (pvar +0xb0) about the 0x80 threshold, the camera near them
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w8(m + 0x31, ri(0, 3) != 0 ? 1u : 0u);  // (drawn this frame: the glows only then)
                for (uint32_t g = 0; g < (s.col - 0x80) / 0x10; ++g) {  // its glows' positions (+0x80.., the ship's update makes them;
                    Vec4& q = VV(pv + 0x80 + g * 0x10);                  // zero in the snapshots) about it
                    q = VV(m + 0x10);
                    q.x += rf(-4.0f, 4.0f);
                    q.y += rf(-4.0f, 4.0f);
                    q.z += rf(0.0f, 6.0f);
                }
                if (ri(0, 3) != 0) w8(pv + s.col, static_cast<uint8_t>(coin() ? ri(0x81, 0xff) : ri(0x60, 0x80)));
            }
            const uint32_t m0 = (*s.ms)[static_cast<size_t>(ri(0, static_cast<int>(s.ms->size()) - 1))];
            if (ri(0, 3) != 0) see(ee_read<uint32_t>(m0 + 0x78) + 0x80 + u(ri(0, 2)) * 0x10, coin() ? 10.0f : 30.0f);  // (a glow seen)
            else move_camera(ee_read<float>(m0 + 0x10), ee_read<float>(m0 + 0x14), ee_read<float>(m0 + 0x18), 40.0f);
            return Args{{m0}, {}};
        }, [s](A a) { vu0_sync(); s.fn(a.i[0]); return uint64_t{0}; });
    }
    // class 1208: the Rocket Launcher's rocket (733's update spawns it by L2:0x2ec228: the launcher, a position, a direction), spawned
    // by the port's spawner in the generator (or one the snapshots hold), flown 1 frame per case, the hero / camera near its path
    {
        level(0);
        const std::vector<uint32_t> launchers = mobys_with(L2_ADDR(0x002df3d8u));
        l2check("ari_class1208_update", 0x002ebf20, 150, Ret::None, [level, launchers, &c1208, mobys_with] {
            level();
            const std::vector<uint32_t> have = mobys_with(L2_ADDR(0x002ebf20u));
            uint32_t m = 0;
            if (!have.empty() && coin()) {
                m = have[static_cast<size_t>(ri(0, static_cast<int>(have.size()) - 1))];
            } else {
                const uint32_t par = launchers.empty() ? ee_read<uint32_t>(OV(0x0015ffd8)) : launchers[0];
                Vec4& p = VV(T);
                p = VV(par + 0x10);
                p.z += rf(0.0f, 3.0f);
                Vec4& d = VV(T + 0x10);
                d.x = rf(-1.0f, 1.0f);
                d.y = rf(-1.0f, 1.0f);
                d.z = rf(-0.3f, 0.6f);
                d.w = 0.0f;
                m = lv2_2ec228_l2(par, T, T + 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).v0;
                if (m == 0) m = par;
            }
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (coin()) w32(pv + 0x10, u(ri(0, 120)));  // its life
            if (coin()) {  // the hero on its path
                Vec4& h = VV(0x0013f3d0u);
                const Vec4 q = VV(m + 0x10);
                h.x = q.x + rf(-6.0f, 6.0f);
                h.y = q.y + rf(-6.0f, 6.0f);
                h.z = q.z + rf(-2.0f, 2.0f);
            }
            if (coin()) w8(m + 0x20, static_cast<uint8_t>(ri(0, 3)));
            return Args{{m}, {}};
        }, [](A a) { vu0_sync(); ari_class1208_update_l2(a.i[0]); return uint64_t{0}; });
        (void)c1208;
    }

    g_pre_call = nullptr;
    g_ee_null_is_ram = null_ram;
    g_ignored_words = saved_ignored;
}

}  // namespace rac1::oracle
