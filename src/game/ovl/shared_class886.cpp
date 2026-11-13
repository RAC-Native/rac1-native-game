// Class 886: one port of the copies Umbris (L7) and Hoven (L12) carry (the same code relinked; compared word by word: only
// relocated words differ). Nothing of it is the level's own: its words are the base ELF's (bare: the hero 0x13f350 (+0x80
// his place, +0x2084 his state, +0xaa8, +0x2fc the moby he stands on, +0x30e), the level's number 0x15ed84, the levels'
// collected flags 0x14c050) and its group's (the shared group helpers). Header shared_class886.h.
//
// The pvars: +0 the timer it stays lit, +4 the tick sound's timer, +8 whether it is lit, +0xc / +0xe the flash's timer and
// step, +0x10 its time in seconds (0: 15). The moby's +0xb0: its index in the level's collected flags (0xff none).
#include <game/ovl/shared_class886.h>

#include <game/math/vecmath.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350;  // (the hero)

// the level's collected flag `index`: 0xff when set
bool collected(uint32_t index) { return lbu(0x0014c050u + index + (lwu(0x0015ed84) << 4)) == 0xff; }
}  // namespace

// Lit by the hero standing on it (or, flat on the ground, by him crouched within its top: 0.6 across, 0.957 up), it lights
// its group in turn (2 / 4 / 6), ticks while lit and puts the group out (8) when its time runs out; at 6 it marks its flag
// collected and flashes. With its flag already collected, its group starts at 6.
// orig: L7:0x0030bf90 L12:0x002ff838
void class886_update(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    bool stood = false;
    const uint32_t index = lbu(m + 0xb0);
    if (lbu(m + 0x20) < 6 && index != 0xff && collected(index)) moby_group_set_state(lbu(m + 0x21), 6);
    if (lw(pv + 8) == 0) {
        if (lh(P + 0x30e) == 0) stood = lwu(P + 0x2fc) == m;
        if (u2f(0x3f490fdbu) < abs_f(lwc1(m + 0x44)) && vec2_distance(V(m + 0x10), V(P + 0x80)) < 2.0f && lw(P + 0x2084) == 0x11 &&
            lwc1(P + 0xaa8) < 7.0f) {
            // (tilted beyond pi / 4: the hero's place in its frame)
            const uint32_t sp = ee_stack_alloc(0xd0);
            const uint32_t rel = sp, frame = sp + 0x10;
            vec3_sub(V(rel), V(m + 0x10), V(P + 0x80));
            mat4_identity(M(frame));
            mat3_transpose(M(frame), M(m + 0xc0));
            mat4_transform(V(rel), V(rel), M(frame));
            const float across = u2f(0x3f19999au);  // (0.6)
            if (abs_f(lwc1(rel)) < across && abs_f(lwc1(rel + 4)) < across && abs_f(lwc1(rel + 8)) < u2f(0x3f74fdf4u)) stood = true;
            ee_stack_free(0xd0);
        }
    }
    const uint32_t lit = lbu(m + 0x20);
    if (lit - 2 < 4 && lbu(m + 0xbc) == 1) {
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv))) != 0) {
            moby_sound_start(2, 0, m);
            moby_group_set_state(lbu(m + 0x21), 8);
        } else if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 4))) != 0) {
            // the tick, quicker as the time runs out
            moby_sound_start(0, 0, m);
            const int32_t most = frame_scale_i(0x2d);
            if (most < frame_scale_i(5) + ee_div(lw(pv), frame_scale_i(0x28))) sw(pv + 4, static_cast<uint32_t>(frame_scale_i(0x2d)));
            else sw(pv + 4, static_cast<uint32_t>(frame_scale_i(5) + ee_div(lw(pv), frame_scale_i(0x28))));
        }
    }
    const uint32_t group = lbu(m + 0x21);
    // (the state table 0x20c900 on Hoven: 0 -> 0x002ffb18, 1 -> 0x002ffb4c, 2 -> 0x002ffbdc, 3 -> 0x002ffbe4, 4 -> 0x002ffc28,
    // 5 -> 0x002ffc30, 6 -> 0x002ffc74, 7 -> 0x002ffca0, 8 -> 0x002ffd64, 9 -> 0x002ffd84)
    switch (lbu(m + 0x20)) {
    case 0:
        moby_set_color_hi(m, 0x80, 0x80, 0x80);
        moby_group_set_bc_l1(group, 0);
        moby_group_set_state(static_cast<int32_t>(group), 1);
        break;
    case 1: {
        sw(pv + 8, 0);
        moby_set_color_hi(m, 0x80, 0x80, 0x80);
        if (!stood) break;
        const int32_t secs = lw(pv + 0x10);
        sw(pv, static_cast<uint32_t>(frame_scale_i(secs > 0 ? secs * 0x3c : 0x384)));
        moby_set_color_hi(m, 0, 0xff, 0);
        sw(pv + 8, 1);
        moby_group_set_bc_l1(lbu(m + 0x21), 0);
        sb(m + 0xbc, 1);
        moby_sound_start(3, 0, m);
        moby_group_set_state(lbu(m + 0x21), 2);
        break;
    }
    case 2:
        sb(m + 0x20, 3);
        break;
    case 3:
        if (!stood) break;
        sw(pv + 8, 1);
        moby_set_color_hi(m, 0, 0xff, 0);
        moby_sound_start(3, 0, m);
        moby_group_set_state(lbu(m + 0x21), 4);
        break;
    case 4:
        sb(m + 0x20, 5);
        break;
    case 5:
        if (!stood) break;
        sw(pv + 8, 1);
        moby_set_color_hi(m, 0, 0xff, 0);
        moby_group_set_state(lbu(m + 0x21), 6);
        moby_sound_start(1, 0, m);
        break;
    case 6:
        sh(pv + 0xe, 0);
        sh(pv + 0xc, static_cast<uint32_t>(frame_scale_i(0x14)));
        moby_set_color_hi(m, 0x80, 0xff, 0x80);
        sb(m + 0x20, 7);
        break;
    case 7: {
        const uint32_t idx = lbu(m + 0xb0);
        if (idx != 0xff && !collected(idx)) checkpoint_mark_collected(idx);
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0xc))) == 0) break;
        sh(pv + 0xc, static_cast<uint32_t>(frame_scale_i(0x14)));
        const int32_t step = lh(pv + 0xe);
        if (step == 1) {
            moby_set_color_hi(m, 0xff, 0xff, 0xff);
            sh(pv + 0xe, 0);
        } else if (step == 0) {
            moby_set_color_hi(m, 0, 0xff, 0);
            sh(pv + 0xe, 1);
        } else if (step >= 2 && step < 6) {
            sh(pv + 0xe, 0);
        }
        break;
    }
    case 8:
        moby_set_color_hi(m, 0x80, 0x80, 0x80);
        sb(m + 0xbc, 0);
        sb(m + 0x20, 1);
        break;
    default:
        break;
    }
}

}  // namespace rac1
