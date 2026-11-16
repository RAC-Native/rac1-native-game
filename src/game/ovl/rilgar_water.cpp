// Rilgar's (level 5) water (class 831): the 48 patches of Blackwater City's water surface (the water grid at 0x1d6880,
// 0x1190 bytes a cell), each a moby of the class that holds its cell's height: some rise and fall between two heights
// when their group is switched, the flooded areas rise in stages while Ratchet works the valve, the first cell sets the
// whole grid up and drives its waves. Ported by hand from Rilgar's own code (L5:0x30ca80 and its helpers 0x30d5c0,
// 0x30d6a0), the original's order of calls and float operations kept; with the 1.02 build's two changes (1.02 Gameplay
// fix 21, always on the PAL 2.00 disc: fixes102.h).
#include <game/ovl/rilgar_own.h>

#include <game/enhance/fixes102.h>
#include <game/level/level_data.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/novalis_water.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants_l5.h>

namespace rac1 {

namespace {
constexpr uint32_t kHero = 0x0013f350;  // the hero's block (+0x80 his position, +0x2f0 his water height, +0x2084 /
                                        // +0x208c his state and the previous one)
constexpr uint32_t kCellSize = 0x1190;
float dt() { return lwc1(0x0015ed6c); }   // the frame's time scale
float dt2() { return lwc1(0x0015ed70); }  // its square
uint32_t grid() { return L5_ADDR(0x001d6880u); }  // the water grid's 48 cells
uint32_t cell(int32_t i) { return grid() + static_cast<uint32_t>(i) * kCellSize; }
// the cells' draw flags (halves, one a cell: +0x1e of the cell while it is shown)
uint32_t cell_flags(int32_t i) { return L5_ADDR(0x0020b380u) + (static_cast<uint32_t>(i) << 1); }
}  // namespace

// The water's group (+0x21) kept level: every water moby of it (class 0x33f) to this one's height, offset by the two's
// low heights (+4) when they differ (none when this one is a stage of the flood, +0xc), its cell's height too.
// orig: L5:0x0030d5c0
void ril_30d5c0_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    for (uint32_t e = lvd::group(lbu(m + 0x21));; e += 2) {
        const uint32_t o = lwu(L5_ADDR(0x0015ffd8u)) + ((lhu(e) & 0x7fffu) << 8);
        if (lh(o + 0xa6) == 0x33f) {
            const uint32_t opv = lwu(o + 0x78);
            float z;
            if (lw(pv + 0xc) != 0 || lwc1(opv + 4) == lwc1(pv + 4)) z = lwc1(m + 0x18);
            else z = fpu_add(lwc1(opv + 4), fpu_sub(lwc1(m + 0x18), lwc1(pv + 4)));
            swc1(o + 0x18, z);
            swc1(cell(lw(opv + 0x14)) + 8, lwc1(o + 0x18));
        }
        if (lh(e) < 0) break;
    }
}

// The neighbours of cell `k` among the 48 (16 apart): north +0xc, south +0xe (x within 0.1, y 16 off), east +0xd,
// west +0xf (y within 0.1, x 16 off); else by x alone (the original's test of the corners looks at x only): +0x11 when
// x is 16 off one way, +0x13 / +0x12 when 16 off the other.
// orig: L5:0x0030d6a0
void ril_30d6a0_l5(int32_t k) {
    const uint32_t c = cell(k);
    const float near = u2f(0x3dcccccdu);  // (0.1)
    for (int32_t i = 0; i < 48; ++i) {
        if (i == k) continue;
        const uint32_t o = cell(i);
        const float dx = fpu_sub(lwc1(c), lwc1(o));
        const float dy = fpu_sub(lwc1(c + 4), lwc1(o + 4));
        if (abs_f(dx) < near) {
            if (abs_f(fpu_sub(dy, 16.0f)) < near) sb(c + 0xc, static_cast<uint32_t>(i));
            if (abs_f(fpu_add(dy, 16.0f)) < near) sb(c + 0xe, static_cast<uint32_t>(i));
        } else if (abs_f(dy) < near) {
            if (abs_f(fpu_sub(dx, 16.0f)) < near) sb(c + 0xd, static_cast<uint32_t>(i));
            if (abs_f(fpu_add(dx, 16.0f)) < near) sb(c + 0xf, static_cast<uint32_t>(i));
        } else {
            const float east = fpu_sub(dx, 16.0f);
            if (abs_f(east) < near) {
                sb(c + 0x11, static_cast<uint32_t>(i));  // (the original tests the same value twice)
                if (abs_f(fpu_add(dx, 16.0f)) < near) sb(c + 0x13, static_cast<uint32_t>(i));
            } else {
                const float west = fpu_add(dx, 16.0f);
                if (abs_f(west) < near) {
                    if (abs_f(east) < near) sb(c + 0x10, static_cast<uint32_t>(i));
                    if (abs_f(west) < near) sb(c + 0x12, static_cast<uint32_t>(i));
                }
            }
        }
    }
}

// Class 831, a patch of water. pvars: its high height +0, its low height +4, it starts high +8, a stage of the flood
// +0xc, its switch kind +0x10 (-1: none), its cell +0x14 (0: the one that sets the grid up and drives its waves), the
// stage's timer +0x18, +0x1c, its speed +0x20, moving +0x24. States: 0 set up; 1 high / 2 low (moving on their
// group's switch, bits 8 / 4 of +0xbc; a ripple now and then); 3 a flood stage waiting for the valve (Ratchet on the
// valve, class 0x33e, within 10): 8; it carries Ratchet swimming within 16 up with it; 7 drained (gone when it is
// above its high height); 8 rises in stages (the level's 0x20b3e0 / 0x20b3f8: frames and speeds, scaled up by the
// level's tries 0x161d1c) to 44, Ratchet near the valve (211, 338, 25) fixing the stage at 5; 4..6 nothing.
// Every frame: its cell's height and draw flag (shown while on screen within 48 and, in states 3..7, between its
// heights); the first cell updates the grid and queues the water's draw (0x30d678).
// orig: L5:0x0030ca80
void ril_class831_update_l5(uint32_t m) {
    if (gameplay102_water_held()) return;  // (1.02 Gameplay fix 21, and the PAL 2.00 disc: held during a cutscene)
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t state = lbu(m + 0x20);
    // the level's flag of this water (the progress table 0x14c050: 16 bytes a level, the moby's +0xb0): 0xff unset
    auto flag = [m] { return lbu(0x0014c050 + (lwu(0x0015ed84) << 4) + lbu(m + 0xb0)); };
    // now and then a ripple of strength `amount` in its cell, within 4 of the cell's centre
    auto ripple = [pv](int32_t odds, float amount) {
        if (rand_mod(odds) != 0) return;
        const float rx = rand_range_f(-4.0f, 4.0f);
        const float x = fpu_add(lwc1(cell(lw(pv + 0x14))), rx);
        const float ry = rand_range_f(-4.0f, 4.0f);
        const uint32_t c = cell(lw(pv + 0x14));
        region_height_splash(c, 1, 1, x, fpu_add(lwc1(c + 4), ry), 1.0f, amount);
    };
    // the hero on the valve: Ratchet's state 0x6a (+0x198 2) counts a try (0x161d1c)
    auto count_try = [] {
        if (lw(kHero + 0x2084) == 0x6a && lw(kHero + 0x198) == 2) sw(L5_ADDR(0x00161d1cu), lwu(L5_ADDR(0x00161d1cu)) + 1);
    };
    // its height toward `target` (the level's rates 0x161d30 / 0x161d34), its group with it
    auto move_to = [m, pv](float target, bool carry) {
        const float r = fpu_mul(lwc1(L5_ADDR(0x00161d30u)), dt2());
        seek_1d(m + 0x18, pv + 0x20, target, r, r, fpu_mul(lwc1(L5_ADDR(0x00161d34u)), dt()));
        if (carry) gameplay102_water_carry(m, lwc1(pv));  // (1.02 Gameplay fix 21, and the PAL 2.00 disc: the swimmer carried)
        ril_30d5c0_l5(m);
    };
    bool tail = true;
    switch (state) {  // (the state's jump table L5:0x00218770, 9 entries, by its index)
    case 0: {  // (0x0030cae4) set up
        if (lw(pv + 0x14) == 0) {  // the first cell: the grid, its look, its draw records and its cells' neighbours
            swc1(L5_ADDR(0x00161d28u), lwc1(m + 0x18));
            sw(L5_ADDR(0x00161d20u), 0);
            sw(L5_ADDR(0x00161d2cu), 0);
            grid_records_reset(grid(), 0x30);
            grid_weights_scale(1.0f);
            const float a = atan2_f(lwc1(L5_ADDR(0x00180350u)), lwc1(L5_ADDR(0x00180354u)));
            swc1(L5_ADDR(0x001cad00u), 16.0f);
            swc1(L5_ADDR(0x001cad0cu), -8.0f);
            swc1(L5_ADDR(0x001cad18u), u2f(0x3f666666u));  // (0.9)
            swc1(L5_ADDR(0x001cad20u), u2f(0x3dcccccdu));  // (0.1)
            swc1(L5_ADDR(0x001cad08u), -8.0f);
            swc1(L5_ADDR(0x001cad14u), 1.0f);
            swc1(L5_ADDR(0x001cad10u), 1.0f);
            swc1(L5_ADDR(0x001cad04u), 16.0f);
            const float k = u2f(0x3f13cd36u);  // (1 / sqrt 3)
            swc1(L5_ADDR(0x001cad30u), fpu_mul(cos_vu(a), k));
            swc1(L5_ADDR(0x001cad34u), fpu_mul(sin_vu(a), k));
            swc1(L5_ADDR(0x001cad38u), u2f(0xbf13cd36u));
            sb(L5_ADDR(0x001cad3du), 0x40);
            sw(L5_ADDR(0x001cad28u), 0x29);
            sw(L5_ADDR(0x001cad2cu), 0x2a);
            sb(L5_ADDR(0x001cad3cu), 0x1c);
            sw(L5_ADDR(0x001612d8u), 0x30);
            sb(L5_ADDR(0x00161205u), 0xb);
            sb(L5_ADDR(0x00161206u), 0x3e);
            swc1(L5_ADDR(0x0016120cu), 20480.0f);
            swc1(L5_ADDR(0x00161210u), 255.0f);
            swc1(L5_ADDR(0x00161214u), 26.0f);
            sw(L5_ADDR(0x001612d0u), grid());
            sw(L5_ADDR(0x001612d4u), L5_ADDR(0x0020b440u));
            sb(L5_ADDR(0x00161204u), 0);
            sw(L5_ADDR(0x00161208u), 0);
            for (int32_t i = 0; i < 48; ++i) {
                grid_point_fixed(cell(i), L5_ADDR(0x0020b440u) + static_cast<uint32_t>(i) * 16);
                sh(cell(lw(pv + 0x14)) + 0x1e, lhu(cell_flags(lw(pv + 0x14))));  // (the original stores its own cell's, 48 times)
            }
            for (int32_t i = 0; i < 48; ++i) ril_30d6a0_l5(i);
        }
        sh(m + 0x32, 0);
        sh(m + 0x34, lhu(m + 0x34) | 1u);
        if (lw(pv + 0xc) != 0) {  // a flood stage: drained already (the level's flag set) or waiting
            if (lw(pv + 0x10) >= 0) {
                sb(m + 0x30, 0xff);
                const int32_t tries = lw(L5_ADDR(0x00161d1cu));
                if (tries >= 2) {  // the stages faster by the tries: 1 + (tries - 1) / 70, at most 1.4
                    float f = fpu_add(fpu_mul(static_cast<float>(tries - 1), u2f(0x3c6a0ea1u)), 1.0f);
                    if (u2f(0x3fb33333u) < f) f = u2f(0x3fb33333u);
                    for (uint32_t i = 0; i < 6; ++i) {
                        sw(L5_ADDR(0x0020b3e0u) + i * 4,
                           static_cast<uint32_t>(float_to_int(fpu_mul(static_cast<float>(lw(L5_ADDR(0x0020b410u) + i * 4)), f))));
                        swc1(L5_ADDR(0x0020b3f8u) + i * 4, fpu_div(lwc1(L5_ADDR(0x0020b428u) + i * 4), f));
                    }
                }
            }
            if (flag() != 0xff) {  // waiting (3)
                sb(m + 0x20, 3);
                sw(pv + 0x18, static_cast<uint32_t>(frame_scale_i(0x3c)));
                if (lwc1(m + 0x18) < lwc1(pv + 4)) sh(cell(lw(pv + 0x14)) + 0x1e, 0);
            } else {  // drained: 44 (7)
                sb(m + 0x20, 7);
                swc1(m + 0x18, 44.0f);
            }
        } else if (lw(pv + 0x10) >= 0) {  // a switched water: low (2)
            sb(m + 0x20, 2);
            swc1(m + 0x18, lwc1(pv + 4));
        } else {  // its heights from where it is placed: high 4.5 above
            const float z = lwc1(m + 0x18);
            swc1(pv + 4, z);
            swc1(pv, fpu_add(z, 4.5f));
            if (lw(pv + 8) != 0) {
                sb(m + 0x20, 1);
                swc1(m + 0x18, lwc1(pv));
            } else {
                sb(m + 0x20, 2);
                swc1(m + 0x18, lwc1(pv + 4));
            }
        }
        if (lw(pv + 0x14) == 0x2f) sw(m + 0x94, 0);
        break;
    }
    case 1:  // (0x0030cfdc) high: on its group's switch (bit 8) it moves (+0x24)
        if ((lbu(m + 0xbc) & 8) != 0) {
            sw(pv + 0x24, 1);
            moby_group_set_state(lbu(m + 0x21), 2);
        } else {
            sb(m + 0xbc, 2);
        }
        if (lw(pv + 0x24) != 0) move_to(lwc1(pv), true);
        ripple(0xc8, u2f(0xbd4ccccdu));  // (-0.05)
        break;
    case 2:  // (0x0030cea4) low: on its group's switch (bit 4) it moves (+0x24); the level's flag set: high (1)
        if ((lbu(m + 0xbc) & 4) != 0) {
            sw(pv + 0x24, 1);
            moby_group_set_state(lbu(m + 0x21), 1);
        } else if (lw(pv + 0x10) >= 0 && flag() == 0xff) {
            sb(m + 0x20, 1);
            swc1(m + 0x18, lwc1(pv));
        }
        if (lw(pv + 0x24) != 0) move_to(lwc1(pv + 4), false);
        sb(m + 0xbc, 1);
        ripple(0xc8, u2f(0xbd4ccccdu));  // (-0.05)
        break;
    case 3: {  // (0x0030d0d0) a flood stage waiting: carries Ratchet swimming within 16 below its surface; Ratchet on the
               // valve within 10: rises (8)
        const uint32_t pos = m + 0x10;
        if (vec2_distance(V(pos), V(kHero + 0x80)) < 16.0f && lwc1(kHero + 0x88) < lwc1(m + 0x18) && lw(kHero + 0x208c) == 0x11 &&
            lwc1(kHero + 0x2f0) < lwc1(m + 0x18))
            swc1(kHero + 0x2f0, lwc1(m + 0x18));
        if (lw(pv + 0x10) >= 0 && vec2_distance(V(pos), V(kHero + 0x80)) < 10.0f) {
            const uint32_t on = lwu(kHero + 0x2fc);  // (the moby he stands on)
            if (on != 0 && lh(kHero + 0x30e) == 0 && lh(on + 0xa6) == 0x33e) {
                sw(pv + 0x1c, 0);
                sb(m + 0x20, 8);
                sw(L5_ADDR(0x00161d20u), 0);
                sw(pv + 0x18, static_cast<uint32_t>(frame_scale_i(lw(L5_ADDR(0x0020b3e0u)))));
            }
        }
        ripple(0x32, u2f(0xbc23d70au));  // (-0.01)
        break;
    }
    case 7:  // (0x0030d248) drained: a try counted at the valve; with no switch, gone once above its high height
        if (lw(pv + 0x10) >= 0) {
            count_try();
        } else if (lwc1(pv) < lwc1(m + 0x18)) {
            sh(cell(lw(pv + 0x14)) + 0x1e, 0);
            moby_retire(m);
            tail = false;
        }
        break;
    case 8: {  // (0x0030d2c8) rises in stages
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x18))) != 0) {
            const int32_t stage = lw(L5_ADDR(0x00161d20u));
            if (stage < 5) {
                sw(L5_ADDR(0x00161d20u), static_cast<uint32_t>(stage + 1));
                sw(pv + 0x18, static_cast<uint32_t>(frame_scale_i(lw(L5_ADDR(0x0020b3e0u) + (static_cast<uint32_t>(stage + 1) << 2)))));
            }
        }
        const int32_t stage = lw(L5_ADDR(0x00161d20u));
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), fpu_mul(lwc1(L5_ADDR(0x0020b3f8u) + (static_cast<uint32_t>(stage) << 2)), dt())));
        ril_30d5c0_l5(m);
        swc1(L5_ADDR(0x00161d28u), lwc1(m + 0x18));
        sw(sp, 0);
        sw(sp + 0xc, 0);
        swc1(sp + 4, 338.0f);
        swc1(sp, 211.0f);
        swc1(sp + 8, 25.0f);
        if (vec3_distance(V(sp), V(kHero + 0x80)) < 6.0f && lw(L5_ADDR(0x00161d2cu)) == 0) {  // at the valve, once
            const bool above = lwc1(L5_ADDR(0x00161d24u)) < lwc1(m + 0x18);
            sw(L5_ADDR(0x00161d2cu), 1);
            if (above) sw(L5_ADDR(0x00161d20u), 5);
        }
        if (lw(pv + 0x10) >= 0) count_try();
        if (44.0f <= lwc1(m + 0x18)) moby_group_set_state(lbu(m + 0x21), 7);
        break;
    }
    default:  // (4 .. 6 and beyond: 0x0030d430)
        break;
    }
    const int32_t idx = lw(pv + 0x14);
    if (tail && idx >= 0) {
        swc1(cell(idx) + 8, lwc1(m + 0x18));
        bool shown = false;
        if (moby_on_screen_l1(m, 48.0f) != -1) {
            if (lbu(m + 0x20) - 3u < 5u) shown = lwc1(pv + 4) < lwc1(m + 0x18) && lwc1(m + 0x18) < lwc1(pv);
            else shown = true;
        }
        if (!shown) sh(cell(lw(pv + 0x14)) + 0x1e, 0);
        else sh(cell(lw(pv + 0x14)) + 0x1e, lhu(cell_flags(lw(pv + 0x14))));
        if (lw(pv + 0x14) == 0) {
            nov_water_grid_update_l1(grid(), 0x30);
            sub_001f93e0(L5_ADDR(0x0030d678u), m);
        }
        if (lw(pv + 0x14) == lw(L5_ADDR(0x00161d10u))) {
            const uint32_t v = lhu(L5_ADDR(0x00161d14u));
            sh(cell(lw(pv + 0x14)) + 0x1e, v);
            sh(cell_flags(lw(pv + 0x14)), v);
        }
    }
    ee_stack_free(0xc0);
}

}  // namespace rac1
