// Gemlik Base's (level 13) class 1263: the tiles of a pool (the Novalis family's water grid: the grids' records at
// L13:0x1d9bc0, 0x1190 bytes each, 22 of them, their fixed points at L13:0x1f1e80, the light L13:0x1cab80;
// shared_helpers / novalis_water / shared_tiles do the work). Level 13 only: the addresses are Gemlik's
// (`orig: L13:`), the level's own data L13_ADDR, the base ELF's bare. Registered in gemlik_mobys.cpp.
//
// The pvars: +0 / +4 the highest / lowest height it bobs between, +8 the bob's angle, +0xc its speed (degrees a second
// at the set-up, then radians a frame), +0x10 its tile (its record; -1 none). The moby: +0x20 the state (0 set up, 1
// bobbing, others: the record only). Nothing at all while Ratchet flies his ship (a live class 0x45 moby at the hero's
// +0x15f0 and his state 0x32).
#include <game/math/vecmath.h>
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/gemlik_own.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_water.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_tiles.h>
#include <game/ovl/variants_l13.h>

namespace rac1 {

using namespace gml;

// A splash on the grids' records `regs` (`n` of them) at (`x`, `y`): region_height_splash with nothing added.
// orig: L13:0x002aeba8
void gml_2aeba8_l13(uint32_t regs, int32_t n, float x, float y, float radius, float amount) {
    region_height_splash(regs, n, 0, x, y, radius, amount);
}

// The update. The tile 0 sets the grids and the light up on its first frame (with a splash at random on each tile),
// updates the grids and queues the water's draw (L13:0x30a460) every frame; every tile bobs between its heights (the
// bob held while the hero is in his states 0x14 / 0x19 of +0x208c or 0x3d / 0x7b / 0x7c of +0x2084), splashes now and
// then, puts its height into its record, and is drawn while on screen (its record's +0x1e from the table L13:0x1f1e20;
// the tile L13:0x161ef0 given L13:0x161ef4 instead, into the table too).
// orig: L13:0x00309e88
void gml_class1263_update_l13(uint32_t m) {
    if (const uint32_t ship = lwu(HERO + 0x15f0); ship != 0 && lh(ship + 0xa6) == 0x45 && alive(ship) && lw(0x001413d4) == 0x32) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    const uint32_t grids = L13_ADDR(0x001d9bc0), looks = L13_ADDR(0x001f1e20);
    auto record = [pv, grids]() { return grids + static_cast<uint32_t>(lw(pv + 0x10) * 0x1190); };
    auto look = [pv, looks]() { return looks + (lwu(pv + 0x10) << 1); };
    switch (lbu(m + 0x20)) {
    case 0: {  // set up: the bob's angle and speed; the tile 0 also the grids, the light and the water's draw words
        sh(m + 0x34, lhu(m + 0x34) | 1u);
        const float deg = u2f(0x3c8efa35u);
        const float speed = fpu_mul(fpu_mul(lwc1(pv + 0xc), deg), dt());
        swc1(pv + 0x8, fpu_mul(lwc1(pv + 0x8), deg));
        swc1(pv + 0xc, speed);
        if (lwu(pv + 0x10) == 0) {
            const uint32_t points = L13_ADDR(0x001f1e80), light = L13_ADDR(0x001cab80);
            grid_records_reset(grids, 0x16);
            grid_weights_scale(1.0f);
            sb(m + 0x30, 0xff);
            const float k = u2f(0x3f13cd36u);  // 1/sqrt(3)
            swc1(light + 0x0, 16.0f);
            swc1(light + 0xc, -8.0f);
            swc1(light + 0x18, u2f(0x3f666666u));
            swc1(light + 0x20, u2f(0x3dcccccdu));
            swc1(light + 0x8, -8.0f);
            swc1(light + 0x14, 1.0f);
            swc1(light + 0x10, 1.0f);
            swc1(light + 0x4, 16.0f);
            const float yaw = atan2_f(lwc1(L13_ADDR(0x001801d0)), lwc1(L13_ADDR(0x001801d4)));
            swc1(light + 0x30, fpu_mul(cos_vu(yaw), k));
            const float s = fpu_mul(sin_vu(yaw), k);
            sb(light + 0x3d, 0x40);
            swc1(light + 0x34, s);
            swc1(light + 0x38, u2f(0xbf13cd36u));
            sw(light + 0x28, 0x2d);
            sw(light + 0x2c, 0x2e);
            sb(light + 0x3c, 0x60);
            sw(L13_ADDR(0x001612d8), 0x16);
            swc1(L13_ADDR(0x0016120c), 32768.0f);
            swc1(L13_ADDR(0x00161210), 255.0f);
            swc1(L13_ADDR(0x00161214), 48.0f);
            sw(L13_ADDR(0x001612d0), grids);
            sw(L13_ADDR(0x001612d4), points);
            sw(L13_ADDR(0x00161208), 0);
            for (uint32_t i = 0; i < 22; ++i) {
                grid_point_fixed(grids + i * 0x1190, points + (i << 4));
                sh(record() + 0x1e, lhu(look()));
            }
            for (uint32_t i = 0; i < 22; ++i) {  // two splashes at random on each tile
                const uint32_t rec = grids + i * 0x1190;
                for (int k2 = 0; k2 < 2; ++k2) {
                    const float dx = rand_range_f(-6.0f, 6.0f);
                    const float x = fpu_add(lwc1(rec + 0x0), dx);
                    const float dy = rand_range_f(-6.0f, 6.0f);
                    const float y = fpu_add(lwc1(rec + 0x4), dy);
                    gml_2aeba8_l13(grids, 0x16, x, y, 2.0f, u2f(0x3e4ccccdu));
                }
            }
            for (int32_t i = 0; i < 22; ++i) tile_links(i);
        }
        sh(m + 0x32, 0);
        sb(m + 0x20, 1);
        break;
    }
    case 1: {  // bobbing
        const float half = fpu_mul(fpu_sub(lwc1(pv + 0x0), lwc1(pv + 0x4)), 0.5f);
        swc1(m + 0x18, fpu_add(lwc1(pv + 0x4), fpu_add(fpu_mul(cos_vu(lwc1(pv + 0x8)), half), half)));
        const int32_t walk = lw(HERO + 0x208c), st = lw(HERO + 0x2084);
        if (walk != 0x14 && walk != 0x19 && st != 0x3d && st != 0x7b && st != 0x7c)
            swc1(pv + 0x8, angle_add(lwc1(pv + 0x8), lwc1(pv + 0xc)));
        sb(record() + 0x1c, lbu(L13_ADDR(0x001cabbc)));
        sb(record() + 0x1d, lbu(L13_ADDR(0x001cabbd)));
        if (rand_mod(0xc8) == 0) {
            const float dx = rand_range_f(-4.0f, 4.0f);
            const float x = fpu_add(lwc1(record() + 0x0), dx);
            const float dy = rand_range_f(-4.0f, 4.0f);
            const float y = fpu_add(lwc1(record() + 0x4), dy);
            region_height_splash(record(), 1, 1, x, y, 1.0f, u2f(0xbd4ccccdu));
        }
        break;
    }
    default:
        break;
    }
    // its height into its record, drawn while on screen; the tile 0 updates the grids and queues the water's draw
    if (lw(pv + 0x10) < 0) return;
    swc1(record() + 0x8, lwc1(m + 0x18));
    const float shown = moby_on_screen_l1(m, 48.0f) != -1 ? 1.0f : 0.0f;
    sh(record() + 0x1e, shown == 0.0f ? 0u : lhu(look()));
    if (lwu(pv + 0x10) == 0) {
        nov_water_grid_update_l1(grids, 0x16);
        draw_callback_add(L13_ADDR(0x0030a460), m);
    }
    if (lw(pv + 0x10) == lw(L13_ADDR(0x00161ef0))) {
        const uint32_t v = lhu(L13_ADDR(0x00161ef4));
        sh(record() + 0x1e, v);
        sh(look(), v);
    }
}

}  // namespace rac1
