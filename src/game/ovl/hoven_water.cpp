// Hoven's (level 12) class 19: the tiles of the level's water (the Novalis family's water grid: the grids' records at
// L12:0x1cbf40, 0x1190 bytes each, their fixed points at 0x1cbe00, the light 0x1cad00; shared_helpers / novalis_water /
// shared_tiles do the work). Level 12 only: the addresses are Hoven's (`orig: L12:`), the level's own data L12_ADDR, the base
// ELF's bare. Registered in hoven_mobys.cpp.
//
// The moby: +0x20 the state (0 set up, 1 linked, 2 running), +0xbc bit 2 a raise asked; the pvars: +0 / +4 the records'
// texture words less 0x28, +8 its height's speed, +0xc its tile (the order the tiles were set up in, the counter
// L12:0x161388), +0x10 the height it seeks, +0x14 seeking, +0x18 the box it is drawn in (-1 always).
#include <game/math/vecmath.h>
#include <game/ovl/hoven_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_water.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_tiles.h>
#include <game/ovl/variants_l12.h>

namespace rac1 {

// The update. The tile 0 sets the grids and the light up on its first frame, updates the grids and queues the water's draw
// (L12:0x2bf748) every frame; every tile puts its place into its record, seeks the height it is raised to, drops the
// hero's water line when he is over it, splashes now and then, and is drawn while on screen (and in its box).
// orig: L12:0x002bf140
void hov_class19_update_l12(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t grids = L12_ADDR(0x001cbf40), points = L12_ADDR(0x001cbe00);
    auto record = [pv, grids]() { return grids + static_cast<uint32_t>(lw(pv + 0xc) * 0x1190); };
    switch (lbu(m + 0x20)) {
    case 0: {  // set up: its tile, the moby's look; the tile 0 also the grids, the light and the water's draw words
        const int32_t tile = lw(L12_ADDR(0x00161388));
        sw(pv + 0xc, static_cast<uint32_t>(tile));
        sw(L12_ADDR(0x00161388), static_cast<uint32_t>(tile + 1));
        if (!(tile + 1 < 128)) debug_printf_stub();  // (the original prints a message: too many tiles)
        const float scale = lwc1(lwu(m + 0x24) + 0x24);
        swc1(m + 0x2c, fpu_add(scale, scale));
        sw(pv + 0, 3);
        sb(m + 0x30, 0xff);
        sh(m + 0x34, lhu(m + 0x34) | 1);
        if (lw(pv + 0xc) == 0) {
            for (int32_t i = 16; i >= 0; --i) sh(grids + static_cast<uint32_t>(i) * 0x1190 + 0x1e, 0);
            grid_records_reset(grids, 0x11);
            grid_weights_scale(1.0f);
            const uint32_t light = L12_ADDR(0x001cad00);
            const float k = u2f(0x3f13cd36u);  // 1/sqrt(3)
            swc1(light + 0x0, 16.0f);
            swc1(light + 0xc, -8.0f);
            swc1(light + 0x18, u2f(0x3f666666u));
            swc1(light + 0x8, -8.0f);
            swc1(light + 0x20, u2f(0x3dcccccdu));
            swc1(light + 0x14, 1.0f);
            swc1(light + 0x10, 1.0f);
            swc1(light + 0x4, 16.0f);
            const float yaw = atan2_f(lwc1(L12_ADDR(0x00180350)), lwc1(L12_ADDR(0x00180354)));
            swc1(light + 0x30, fpu_mul(cos_vu(yaw), k));
            const float s = fpu_mul(sin_vu(yaw), k);
            swc1(light + 0x38, u2f(0xbf13cd36u));
            swc1(light + 0x34, s);
            sw(light + 0x28, lwu(pv + 0) + 0x28);
            sb(light + 0x3d, 0x40);
            sb(light + 0x3c, 0x1c);
            sw(light + 0x2c, lwu(pv + 4) + 0x28);
            sw(L12_ADDR(0x001612d0), grids);
            sw(L12_ADDR(0x001612d4), points);
            sw(L12_ADDR(0x001612d8), 0x11);
            swc1(L12_ADDR(0x0016120c), 32768.0f);
            swc1(L12_ADDR(0x00161210), 255.0f);
            swc1(L12_ADDR(0x00161214), 48.0f);
            sw(L12_ADDR(0x00161208), 0);
        }
        // its record: its place, the texture words, its colour, drawn
        swc1(record() + 0x0, lwc1(m + 0x10));
        swc1(record() + 0x4, lwc1(m + 0x14));
        swc1(record() + 0x8, lwc1(m + 0x18));
        sw(record() + 0x14, lwu(pv + 0) + 0x28);
        sw(record() + 0x18, lwu(pv + 4) + 0x28);
        sb(record() + 0x1c, 0x1c);
        sb(record() + 0x1d, 0x44);
        sh(record() + 0x1e, 0xffff);
        grid_point_fixed(record(), points + (lwu(pv + 0xc) << 4));
        sb(m + 0x20, 1);
        break;
    }
    case 1:  // the tiles' links (the counter back to 0 for the next set-up)
        sw(L12_ADDR(0x00161388), 0);
        tile_links(lw(pv + 0xc));
        sb(m + 0x20, 2);
        swc1(pv + 0x10, lwc1(m + 0x18));
        break;
    case 2: {  // running: a raise of 5 asked (+0xbc bit 2), the height sought; the hero's water line; a splash at random
        if ((lbu(m + 0xbc) & 4) != 0) {
            sw(pv + 0x14, 1);
            swc1(pv + 0x10, fpu_add(lwc1(pv + 0x10), 5.0f));
        }
        sb(m + 0xbc, 1);
        if (lwu(pv + 0x14) != 0) {
            const float accel = fpu_mul(lwc1(0x0015ed70), 4.0f);
            seek_1d(m + 0x18, pv + 8, lwc1(pv + 0x10), accel, accel, fpu_mul(lwc1(0x0015ed6c), 4.0f));
            tile_group_height(m);
        }
        if (abs_f(fpu_sub(lwc1(0x0013f3d0), lwc1(m + 0x10))) <= 8.0f || abs_f(fpu_sub(lwc1(0x0013f3d4), lwc1(m + 0x14))) <= 8.0f)
            swc1(0x0013f350 + 0x2f0, lwc1(m + 0x18));
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
    // drawn while on screen and in its box; the tile 0 updates the grids and queues the water's draw
    if (lw(pv + 0xc) < 0) return;
    float shown = moby_on_screen_l1(m, 64.0f) != -1 ? 1.0f : 0.0f;
    const int32_t box = lw(pv + 0x18);
    if (box != -1 && point_in_box(L12_ADDR(0x00167240), box) == 0) shown = 0.0f;
    sh(record() + 0x1e, shown == 0.0f ? 0u : 0xffffu);
    if (lw(pv + 0xc) != 0) return;
    nov_water_grid_update_l1(grids, 0x11);
    sub_001f93e0(L12_ADDR(0x002bf748), m);
}

}  // namespace rac1
