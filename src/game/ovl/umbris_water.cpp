// Umbris's own moby classes: class 902, the water of the Hydrodisplacer rooms (one moby a tile of the level's 36 water
// tiles; tile 0's sets up the grid, ripples it and draws it: the room's water level rising and falling with the
// Hydrodisplacer's group, the last room's flood), with 0x281ef8 (whether a moby leads its group) and 0x2cb1c0 (a splash
// on the grid), and class 1018 (the sea: its wave table set, its draw added while the camera is out of its volume) —
// hand-ported from Umbris's own overlay. Header umbris_mobys.h, registered in umbris_mobys.cpp. Addresses are Umbris's
// (`orig: L7:`); Umbris-only data is L7_ADDR, the base ELF's are bare (the frame time 0x15ed6c, the hero 0x13f3d0).
//
// Class 902's pvars: its levels +0x0 (high) and +0x4 (low), whether it starts high +0x8, starts flooding +0xc, the
// volume that sets the flood going +0x10, its tile +0x14 (-1: none), a timer +0x18. Its state +0x20, its group +0x21,
// the group's commands +0xbc (4: down, 8: up), its water level its z +0x18.
#include <game/math/vecmath.h>
#include <game/ovl/aridia_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/novalis_water.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_tiles.h>
#include <game/ovl/umbris_mobys.h>
#include <game/ovl/variants_l7.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;
constexpr uint32_t TILE_SIZE = 0x1190;
constexpr int32_t TILES = 36;

uint32_t tiles() { return L7_ADDR(0x001dce40); }
// tile `i`'s record (the grid's: +0x0 x, +0x4 y, +0x8 its level, +0x1c / +0x1d its colours, +0x1e its alpha)
uint32_t tile(int32_t i) { return tiles() + static_cast<uint32_t>(i * static_cast<int32_t>(TILE_SIZE)); }
// the tiles' alphas (0x204680, a halfword each)
uint32_t tile_alpha(int32_t i) { return L7_ADDR(0x00204680) + (static_cast<uint32_t>(i) << 1); }

// The grid set up (tile 0's moby, once): the records reset, the weights, the light and colours of the water, the tiles'
// points and alphas, two random splashes on each tile, their links.
void grid_setup(uint32_t pv) {
    const uint32_t base = tiles();
    grid_records_reset(base, TILES);
    grid_weights_scale(1.0f);
    const float lx = lwc1(L7_ADDR(0x0017ff50));
    const float ly = lwc1(L7_ADDR(0x0017ff54));
    swc1(L7_ADDR(0x001ca900), 16.0f);
    swc1(L7_ADDR(0x001ca90c), -8.0f);
    swc1(L7_ADDR(0x001ca918), u2f(0x3f666666u));
    swc1(L7_ADDR(0x001ca920), u2f(0x3dcccccdu));
    swc1(L7_ADDR(0x001ca908), -8.0f);
    swc1(L7_ADDR(0x001ca914), 1.0f);
    swc1(L7_ADDR(0x001ca910), 1.0f);
    swc1(L7_ADDR(0x001ca904), 16.0f);
    const float a = atan2_f(lx, ly);
    const float k = u2f(0x3f13cd36u);  // 1/sqrt(3)
    swc1(L7_ADDR(0x001ca930), fpu_mul(cos_vu(a), k));
    const float s = fpu_mul(sin_vu(a), k);
    sb(L7_ADDR(0x001ca93d), 0x40);
    swc1(L7_ADDR(0x001ca934), s);
    swc1(L7_ADDR(0x001ca938), u2f(0xbf13cd36u));
    sw(L7_ADDR(0x001ca928), 0x28);
    sw(L7_ADDR(0x001ca92c), 0x29);
    sw(L7_ADDR(0x001612d8), TILES);
    swc1(L7_ADDR(0x0016120c), 32768.0f);
    swc1(L7_ADDR(0x00161210), 255.0f);
    swc1(L7_ADDR(0x00161214), 48.0f);
    sb(L7_ADDR(0x001ca93c), 0x40);
    sw(L7_ADDR(0x001612d0), base);
    sw(L7_ADDR(0x001612d4), L7_ADDR(0x00204700));
    sw(L7_ADDR(0x00161208), 0);
    for (int32_t i = 0; i < TILES; ++i) {
        grid_point_fixed(tile(i), L7_ADDR(0x00204700) + (static_cast<uint32_t>(i) << 4));
        sh(tile(lw(pv + 0x14)) + 0x1e, lhu(tile_alpha(lw(pv + 0x14))));
    }
    for (int32_t i = 0; i < TILES; ++i) {
        const uint32_t t = tile(i);
        for (int k2 = 0; k2 < 2; ++k2) {
            const float rx = rand_range_f(-6.0f, 6.0f);
            const float x = fpu_add(lwc1(t + 0x0), rx);
            const float ry = rand_range_f(-6.0f, 6.0f);
            const float y = fpu_add(lwc1(t + 0x4), ry);
            umb_2cb1c0_l7(base, TILES, x, y, 2.0f, u2f(0x3e4ccccdu));
        }
    }
    for (int32_t i = 0; i < TILES; ++i) tile_links(i);
}
// the water level moved toward `target` at 4 a second
void level_approach(uint32_t m, float target) { approach_f(m + 0x18, target, fpu_mul(lwc1(0x0015ed6c), 4.0f)); }
// state 1 (up: `bit` 8, `want` 2, toward +0x0) or 2 (down: 4, 1, toward +0x4): the group told to go the other way when
// its command bit is set; else the group's leader sets the group's command +0xbc and both move the level
void level_follow(uint32_t m, uint32_t bit, uint32_t state, uint32_t want, float target) {
    if ((lbu(m + 0xbc) & bit) != 0) {
        moby_group_set_state(static_cast<int32_t>(lbu(m + 0x21)), state);
        return;
    }
    if (umb_281ef8_l7(m, static_cast<int32_t>(lbu(m + 0x21)), 1, 0)) {
        level_approach(m, target);
        if (lbu(m + 0xbc) != want) moby_group_set_bc_l1(lbu(m + 0x21), want);
    } else if (lbu(m + 0xbc) == want) {
        level_approach(m, target);
    }
}
}  // namespace

// Whether moby `m` is the first of group `group` (group_first's flags `a`, `b`).
// orig: L7:0x00281ef8
bool umb_281ef8_l7(uint32_t m, int32_t group, int32_t a, int32_t b) {
    const uint32_t sp = ee_stack_alloc(0x10);  // (group_first's iterator: the original's stack)
    group_first(sp, group, a, b);
    const bool first = lwu(sp) == m;
    ee_stack_free(0x10);
    return first;
}

// A splash on the grid `tiles` (`n` tiles) at x, y: radius `radius`, depth `amount` (set, not added).
// orig: L7:0x002cb1c0
void umb_2cb1c0_l7(uint32_t tiles_at, int32_t n, float x, float y, float radius, float amount) {
    region_height_splash(tiles_at, n, 0, x, y, radius, amount);
}

// Class 902 (+0x20, the table L7:0x211680: 0 set up, 1 high (following the group up), 2 low (down), 3 waiting for the
// hero in the flood's volume, 4 the flood's first wait, 5 rising slowly, 6 rising fast to 54, 7 risen: retired past its
// high level). After each: its tile's colours, level, alpha (on screen, between its levels while flooding), tile 0's
// grid moved and drawn, the tile 0x161b30's alpha 0x161b34.
// orig: L7:0x0030c508
void umb_class902_update_l7(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    if (lw(L7_ADDR(0x00161b38)) != 0) {
        sb(0x0013d4c4, 0);
        sb(0x0013d4ec, 0);
    }
    switch (lbu(m + 0x20)) {
        case 0:  // L7:0x30c58c
            sh(m + 0x34, lhu(m + 0x34) | 0x81);
            if (lw(pv + 0x14) == 0) grid_setup(pv);
            if (lw(pv + 0xc) != 0) {
                sb(m + 0x20, 3);
                sw(pv + 0x18, static_cast<uint32_t>(frame_scale_i(60)));
                if (lwc1(m + 0x18) < lwc1(pv + 0x4)) sh(tile(lw(pv + 0x14)) + 0x1e, 0);
            } else {
                sb(m + 0x20, lw(pv + 0x8) != 0 ? 1 : 2);
            }
            break;
        case 1:  // L7:0x30c900
        case 2:  // L7:0x30c864
            if (lbu(m + 0x20) == 2)
                level_follow(m, 4, 1, 1, lwc1(pv + 0x4));
            else
                level_follow(m, 8, 2, 2, lwc1(pv + 0x0));
            // now and then a ripple on its tile
            if (rand_mod(200) == 0) {
                const float rx = rand_range_f(-4.0f, 4.0f);
                const float x = fpu_add(lwc1(tile(lw(pv + 0x14)) + 0x0), rx);
                const float ry = rand_range_f(-4.0f, 4.0f);
                const uint32_t t = tile(lw(pv + 0x14));
                region_height_splash(t, 1, 1, x, fpu_add(lwc1(t + 0x4), ry), 1.0f, u2f(0xbd4ccccdu));
            }
            break;
        case 3: {  // L7:0x30ca38
            const int32_t vol = lw(pv + 0x10);
            if (vol >= 0 && point_in_box(HERO_POS, vol) != 0) {
                sw(pv + 0x1c, 0);
                moby_group_set_state(static_cast<int32_t>(lbu(m + 0x21)), 4);
            }
            break;
        }
        case 4:  // L7:0x30ca68
            if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x18))) != 0) {
                sw(pv + 0x18, static_cast<uint32_t>(frame_scale_i(0x708)));
                sb(m + 0x20, 5);
            }
            break;
        case 5:  // L7:0x30ca90
            swc1(m + 0x18, fpu_add(lwc1(m + 0x18), fpu_mul(lwc1(0x0015ed6c), u2f(0x3ea3d70au))));
            if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x18))) != 0) sb(m + 0x20, 6);
            break;
        case 6: {  // L7:0x30cacc
            const float z = fpu_add(lwc1(m + 0x18), fpu_mul(lwc1(0x0015ed6c), u2f(0x3ed70a3du)));
            swc1(m + 0x18, z);
            if (54.0f <= z) sb(m + 0x20, 7);
            break;
        }
        case 7:  // L7:0x30cb10
            if (lwc1(pv + 0x0) < lwc1(m + 0x18)) {
                sh(tile(lw(pv + 0x14)) + 0x1e, 0);
                moby_retire(m);
                return;
            }
            break;
        default:
            break;
    }
    if (lw(pv + 0x14) < 0) return;
    sb(tile(lw(pv + 0x14)) + 0x1c, lbu(L7_ADDR(0x001ca93c)));
    sb(tile(lw(pv + 0x14)) + 0x1d, lbu(L7_ADDR(0x001ca93d)));
    swc1(tile(lw(pv + 0x14)) + 0x8, lwc1(m + 0x18));
    bool shown = false;
    if (lw(m + 0x94) != 0 && moby_on_screen_l1(m, 48.0f) != -1) {
        if (lbu(m + 0x20) - 3u < 5u)
            shown = lwc1(pv + 0x4) < lwc1(m + 0x18) && lwc1(m + 0x18) < lwc1(pv + 0x0);
        else
            shown = true;
    }
    if (!shown)
        sh(tile(lw(pv + 0x14)) + 0x1e, 0);
    else
        sh(tile(lw(pv + 0x14)) + 0x1e, lhu(tile_alpha(lw(pv + 0x14))));
    if (lw(pv + 0x14) == 0) {
        nov_water_grid_update_l1(tiles(), TILES);
        sub_001f93e0(L7_ADDR(0x0030cd20), m);
    }
    if (lw(pv + 0x14) == lw(L7_ADDR(0x00161b30))) {
        const uint32_t a = lhu(L7_ADDR(0x00161b34));
        sh(tile(lw(pv + 0x14)) + 0x1e, a);
        sh(tile_alpha(lw(pv + 0x14)), a);
    }
}

// Class 1018, the sea: its wave table (0x208560) set once (2/3); while the camera is out of its volume (pvar +0x0) its
// draw (L7:0x30d420, the seas' draw chain) added.
// orig: L7:0x0030d440
void umb_class1018_update_l7(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        ocean_wave_table_set_80(L7_ADDR(0x00208560), u2f(0x3f2aaaabu));
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
    } else if (st == 1) {
        if (point_in_box(L7_ADDR(0x00166e40), lw(lwu(m + 0x78))) == 0) draw_hook_add_dc80_l2(L7_ADDR(0x0030d420), m);
    }
}

}  // namespace rac1
