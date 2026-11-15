// Blarg's Nebula G34 (level 6) own moby classes, hand-ported: class 1061, the lift on a shaft's water, worked from a
// console (Ratchet at the console raises or lowers the water and the lift rides with it between its two door sets; the
// first use with the Hydrodisplacer plays the cutscene 3). Header blarg_own.h; registered in blarg_mobys.cpp. Addresses
// are level 6's (`orig: L6:`); its own data is L6_ADDR, the base ELF's are bare.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/screen_fade.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
constexpr uint32_t PLAYER = 0x0013f350;  // the hero (+0x80 his position, +0x2080 his moby, +0x20a4 his movement mode)
constexpr uint32_t HERO_POS = PLAYER + 0x80;

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

// the moby of the level's moby table at index `i`
uint32_t table_moby(int32_t i) { return lwu(L6_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8); }
// the door set whose index the pvar `word` holds
uint32_t door_at(uint32_t word) { return table_moby(lw(word)); }

bool hydrodisplacer_owned() { return lbu(0x0013d4c0 + 0x16) != 0; }
bool item6_owned() { return lbu(0x0013d4c0 + 0x6) != 0; }  // (the hero's item 6)

// Ratchet put at the box `box`'s centre and rotation
void hero_to_box(int32_t box) {
    const uint32_t b = lvd::box(lvd::BOXES, box);
    hero_place(b + 0x30, b + 0x70, 0, 1);
}
}  // namespace

// Class 1061, the lift. pvars: the attach frame +0x20, the door sets +0x60 (the console's) / +0x64 (above) / +0x68
// (below), the console's moby +0x6c, the console's box +0x70, the water's box +0x78 (its height: the box's z, plus its
// height +0x28 for the high level), the boxes Ratchet is put at +0x7c (leaving the console) / +0x80 (at it), +0x84 the
// water up, +0x88, +0x8c the lift's rate, +0x90 a wait, +0x94 Ratchet riding (0x14161b), +0x9c its glow's phase. The
// level's word 0x161f78 says whether the Hydrodisplacer's first use was shown (a cutscene, 3). States: 0 set up (the
// water's high level, the console's moby hidden or the console's door set shut by the hero's mode, the two others shut);
// 1 waits for Ratchet at the console (0x13d3a8: its first visit) or the hero moby of class 0x57; 2 waits for a cutscene's end;
// 3 / 4 up / down: at the console, the help text (TXT 0x177e without the item 6, 0x1781 in the movement mode 1, 0x1785 /
// 0x1789 the water up / down) and the button (0x13cae4 bit 4) take Ratchet to the console (without the item 6: the
// movement mode 1, state 0x43, the music 2; leaving it with the Hydrodisplacer the first time, the cutscene 3) or back out,
// raising or lowering the water; riding it (+0x2fc) or under / over it within 24 it moves (5 / 6), the door
// set it leaves shut; 5 / 6 move to the low / high level (seek_1d, 0x161f64 / 0x161f68), then open the door set it comes
// to (8 / 7); 7 / 8 wait until Ratchet steps off (1) (3 / 4). Every frame the lift's glow (0x161f6c .. 0x161f74), Ratchet
// riding (0x14161b) and its attach frame (the move and turn of the frame).
// orig: L6:0x002fc640
void blg_class1061_update_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xb0);
    const uint32_t moved = sp, turn = sp + 0x10;
    vec3_scale(V(moved), V(m + 0x10), -1.0f);
    lq_sq(turn, m + 0x40);
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(L6_ADDR(0x00161f60))));
    // the lift starts moving from the state `st`: its door set `door` shut, a wait of 120 frames
    auto start_move = [&](uint32_t st, uint32_t door) {
        sb(m + 0x20, st);
        door_set_shut_l6(door_at(door));
        sw(pv + 0x90, static_cast<uint32_t>(frame_scale_i(0x78)));
    };
    switch (lbu(m + 0x20)) {  // (the table 0x2017b0: 0, 1, 2, 3 / 4, 5 / 6, 7 / 8)
    case 0: {
        if (lw(L6_ADDR(0x00161f78)) == -1) sw(L6_ADDR(0x00161f78), hydrodisplacer_owned() ? 1 : 0);
        sw(pv + 0x94, 0);
        sw(pv + 0x88, 0xffffffffu);
        sb(m + 0x20, lbu(0x0013d3a8) != 0 ? 3 : 1);
        const uint32_t water = lvd::box(lvd::BOXES, lw(pv + 0x78));
        swc1(m + 0x18, fpu_add(lwc1(water + 0x38), lwc1(water + 0x28)));
        const uint32_t mode = lbu(PLAYER + 0x20a4);
        if (mode == 0) {
            const uint32_t console = door_at(pv + 0x6c);
            sh(console + 0x34, lhu(console + 0x34) | 1u);
            sh(door_at(pv + 0x6c) + 0x34, lhu(door_at(pv + 0x6c) + 0x34) | 2u);
            sw(door_at(pv + 0x6c) + 0x94, 0);
        } else if (mode == 1) {
            door_set_shut_l6(door_at(pv + 0x60));
        }
        door_set_shut_l6(door_at(pv + 0x64));
        door_set_shut_l6(door_at(pv + 0x68));
        break;
    }
    case 1:
        if (point_in_box(HERO_POS, lw(pv + 0x70)) != 0) {
            if (lbu(0x0013d3a8) == 0) sb(0x0013d3a8, 1);
            sb(m + 0x20, 3);
            sw(pv + 0x90, 0);
        } else if (lh(lwu(PLAYER + 0x2080) + 0xa6) == 0x57) {
            sb(m + 0x20, 3);
            sw(pv + 0x84, 1);
        }
        break;
    case 2:
        if (lw(L6_ADDR(0x0015f5c4)) == 0) sb(m + 0x20, 3);
        break;
    case 3:
    case 4: {
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x90))) != 0 && point_in_box(HERO_POS, lw(pv + 0x70)) != 0) {
            int32_t text;
            if (lbu(PLAYER + 0x20a4) == 1) text = 0x1781;
            else if (!item6_owned()) text = 0x177e;
            else text = lw(pv + 0x84) != 0 ? 0x1785 : 0x1789;
            const bool shown = 0 < hud_message_request(9, TXT(text));
            if ((lwu(0x0013cae4) & 0x10u) != 0 && lw(PLAYER + 0x2084) != 0x64 && shown) {
                sh(PLAYER + 0x1f2, static_cast<uint32_t>(frame_scale_i(0x3c)));
                sw(pv + 0x90, static_cast<uint32_t>(frame_scale_i(0x78)));
                if (lbu(PLAYER + 0x20a4) == 1) {
                    // Ratchet leaves the console
                    screen_fade(frame_scale_i(0x10));
                    door_set_reset_l6(door_at(pv + 0x64));
                    door_set_reset_l6(door_at(pv + 0x68));
                    door_set_open_l6(door_at(pv + 0x60));
                    sw(pv + 0x84, 0);
                    sw(door_at(pv + 0x6c) + 0x94, 0);
                    hero_mode_exit();
                    hero_to_box(lw(pv + 0x7c));
                    music_track_request(0, 0xa);
                    sh(door_at(pv + 0x6c) + 0x34, lhu(door_at(pv + 0x6c) + 0x34) | 1u);
                    sh(door_at(pv + 0x6c) + 0x34, lhu(door_at(pv + 0x6c) + 0x34) | 2u);
                    if (hydrodisplacer_owned() && lw(L6_ADDR(0x00161f78)) == 0) {
                        sw(L6_ADDR(0x00161f78), 1);
                        cutscene_start(3);
                        sb(m + 0x20, 2);
                        break;
                    }
                } else if (!item6_owned()) {
                    // Ratchet takes the console (without the item 6: in the movement mode 1)
                    screen_fade(frame_scale_i(0x10));
                    door_set_open_l6(door_at(pv + 0x64));
                    door_set_shut_l6(door_at(pv + 0x68));
                    door_set_reset_l6(door_at(pv + 0x60));
                    sw(pv + 0x84, 1);
                    const uint32_t console = door_at(pv + 0x6c);
                    sw(console + 0x94, lwu(lwu(console + 0x24) + 0x10));
                    sh(door_at(pv + 0x6c) + 0x34, lhu(door_at(pv + 0x6c) + 0x34) & 0xfffeu);
                    hero_to_box(lw(pv + 0x80));
                    hero_mode_enter(1, 0x43, door_at(pv + 0x6c));
                    music_track_request(2, 7);
                } else if (lw(pv + 0x84) == 0) {
                    // the water raised
                    screen_fade(frame_scale_i(0x10));
                    door_set_open_l6(door_at(pv + 0x64));
                    door_set_shut_l6(door_at(pv + 0x68));
                    door_set_reset_l6(door_at(pv + 0x60));
                    sw(pv + 0x94, 1);
                    sw(pv + 0x84, 1);
                    hero_to_box(lw(pv + 0x80));
                } else {
                    // the water lowered
                    screen_fade(frame_scale_i(0x10));
                    door_set_reset_l6(door_at(pv + 0x64));
                    door_set_shut_l6(door_at(pv + 0x68));
                    door_set_open_l6(door_at(pv + 0x60));
                    sw(pv + 0x84, 0);
                    sw(pv + 0x94, 0);
                    hero_to_box(lw(pv + 0x7c));
                }
            }
        }
        // Ratchet riding it, above it and on it (within 1): it moves
        const uint32_t pos = m + 0x10;
        bool moving = false;
        if (lwu(PLAYER + 0x2fc) == m && lh(PLAYER + 0x30e) == 0 &&
            timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x90))) != 0 &&
            fpu_add(lwc1(m + 0x18), u2f(0x3e947ae1u)) < lwc1(PLAYER + 0x88) &&  // (0.29)
            vec2_distance(V(HERO_POS), V(pos)) < 1.0f) {
            moby_sound_start(0, 0, m);
            if (lbu(m + 0x20) == 3) start_move(5, pv + 0x64);
            else start_move(6, pv + 0x68);
            moving = true;
        }
        if (!moving && vec2_distance(V(pos), V(HERO_POS)) < 24.0f && lbu(m + 0x20) == 3 &&
            fpu_add(lwc1(HERO_POS + 8), 5.0f) < lwc1(m + 0x18)) {
            // Ratchet below it: it comes down
            sw(pv + 0x94, 1);
            sw(pv + 0x84, 1);
            sb(m + 0x20, 5);
            door_set_shut_l6(door_at(pv + 0x64));
            door_set_shut_l6(door_at(pv + 0x60));
            sw(pv + 0x90, static_cast<uint32_t>(frame_scale_i(0x78)));
            moving = true;
        }
        if (!moving && vec2_distance(V(pos), V(HERO_POS)) < 24.0f && lbu(m + 0x20) == 4 &&
            lwc1(m + 0x18) < fpu_sub(lwc1(HERO_POS + 8), 5.0f))
            start_move(6, pv + 0x68);  // (Ratchet above it: it goes up)
        break;
    }
    case 5:
    case 6: {
        const uint32_t water = lvd::box(lvd::BOXES, lw(pv + 0x78));
        float level = lwc1(water + 0x38);
        if (lbu(m + 0x20) == 6) level = fpu_add(level, lwc1(water + 0x28));
        const float accel = fpu_mul(lwc1(L6_ADDR(0x00161f68)), dt_sq());
        seek_1d(m + 0x18, pv + 0x8c, level, accel, accel, fpu_mul(lwc1(L6_ADDR(0x00161f64)), dt()));
        if (abs_f(fpu_sub(lwc1(m + 0x18), level)) < u2f(0x3c23d70au)) {  // (there: within 0.01)
            if (lbu(m + 0x20) == 6) {
                sb(m + 0x20, 7);
                door_set_open_l6(door_at(pv + 0x64));
            } else {
                door_set_open_l6(door_at(pv + 0x68));
                sb(m + 0x20, 8);
            }
        }
        break;
    }
    case 7:
    case 8:
        if (1.0f <= vec2_distance(V(HERO_POS), V(m + 0x10))) sb(m + 0x20, lbu(m + 0x20) == 7 ? 3 : 4);
        break;
    default:
        break;
    }
    sb(PLAYER + 0x22cb, lw(pv + 0x94) != 0 ? 1 : 0);
    uint32_t colour;
    if (static_cast<uint32_t>(lbu(m + 0x20) - 5) < 4u) {  // (moving or just arrived: its steady colour)
        sw(pv + 0x9c, 0);
        colour = lwu(L6_ADDR(0x00161f74));
    } else {
        const float phase = angle_add(lwc1(pv + 0x9c), fpu_mul(dt(), u2f(0x40a78d36u)));  // (300 degrees a second)
        swc1(pv + 0x9c, phase);
        float t = fpu_sub(fpu_mul(sin_vu(phase), 4.0f), 3.0f);
        if (1.0f < t) t = 1.0f;
        else if (t < 0.0f) t = 0.0f;
        colour = rgba_lerp(lwu(L6_ADDR(0x00161f6c)), lwu(L6_ADDR(0x00161f70)), t);
    }
    sw(m + 0x90, colour);
    vec3_add(V(moved), V(moved), V(m + 0x10));
    attach_frame_set(pv + 0x20, moved, turn, m + 0x40);
    ee_stack_free(0xb0);
}

}  // namespace rac1
