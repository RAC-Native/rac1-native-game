// Novalis's (level 1) variants of the hero's timers, idle entry, landing and item use:
// Veldin's code minus the features Novalis's build does not have (movement modes 1 / 2, the
// swingshot, the hoverboard landing). Addresses are Novalis's.
#include <game/enhance/advanced.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/vendor_tr.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t P = g::PLAYER;
constexpr uint32_t PAD = 0x0013c940;

float fs6c() { return lwc1(0x0015ed6c); }
uint32_t buttons() { return lwu(PAD + 0x1a0); }
int32_t& i32_at(uint32_t a) { return *static_cast<int32_t*>(ee_ram_ptr(a)); }  // orig: (helper) EE word
int16_t& i16_at(uint32_t a) { return ee_ref<int16_t>(a); }  // orig: (helper) EE halfword
// the C remainder of a ring index by the 32-entry history length
// orig: (helper) the (i < 0 ? i + 31 : i) >> 5 idiom
int32_t ring32(int32_t i) { return i - ((i < 0 ? i + 31 : i) >> 5) * 32; }
// the item's button pressed within `back` frames (pad history), 0 when not
// orig: (helper) a call of pad_button_history (0x266d60 on Veldin, 0x27b940 on Novalis)
uint32_t item_pressed(uint32_t mask, int32_t back) { return pad_button_history(mask, back, nullptr); }
}  // namespace

// The hero's per-frame timers and bookkeeping (Veldin's 0x2215c8) in Novalis's build: without the ship
// mode's +0x1636 countdown and the camera offsets of the movement modes 1 / 2.
// orig: L1:0x0023c710 L2:0x0022a988
void hero_timers_update_l1() {
    if (lbu(P + 0x20ad) == 0) hero_tilt_update();
    sw(P + 0x198, lwu(P + 0x198) + 1);
    sw(P + 0x19c, lwu(P + 0x19c) + 1);
    sw(P + 0x1a0, lwu(P + 0x1a0) + 1);
    sw(P + 0x1a4, lwu(P + 0x1a4) + 1);
    timer_tick_i32(i32_at(P + 0x1c4));
    timer_tick_i32(i32_at(P + 0x1c0));
    timer_tick_i16(i16_at(P + 0x1b2));
    timer_tick_i32(i32_at(P + 0x1b8));
    timer_tick_i16(i16_at(P + 0x1d8));
    timer_tick_i16(i16_at(P + 0x1de));
    timer_tick_i32(i32_at(P + 0x1b4));
    for (uint32_t o : {0x1f0u, 0x1b0u, 0x1c8u, 0x1cau, 0x1e4u, 0x1e6u}) timer_tick_i16(i16_at(P + o));
    for (uint32_t o : {0x1ccu, 0x1d0u, 0x1d4u}) timer_tick_i32(i32_at(P + o));
    for (uint32_t o : {0x1e8u, 0x1e0u, 0x1eeu, 0x1eau, 0x1f4u, 0x1f2u, 0x1dcu, 0x1f6u, 0x1dau, 0x1f8u}) timer_tick_i16(i16_at(P + o));
    if ((lwu(PAD + 0x1a0) & 5) == 0) sh(P + 0x1ec, 0);
    if (lbu(P + 0x20a8) == 0) {
        sw(P + 0x1bc, 0);
    } else {
        sw(P + 0x1bc, lwu(P + 0x1bc) + 1);
    }
    if (lw(PAD + 0x1d4) != 0) {
        sw(P + 0x1a8, 0);
        sw(P + 0x1ac, lwu(P + 0x1ac) + 1);
    } else {
        sw(P + 0x1ac, 0);
        sw(P + 0x1a8, lwu(P + 0x1a8) + 1);
    }
    if (lh(P + 0x22de) != 0 && timer_tick_i16(i16_at(P + 0x22de)) != 0 && lw(P + 0x22a8) != 0 && lw(OV(0x0015f5c4)) == 0) {
        if (lbu(P + 0x20a4) == 3) {
            hero_mode_exit();
            if (pad_button_history(0x80, frame_scale_i(0x12), nullptr) != 0) sb(P + 0x20a6, 1);
        } else {
            const uint32_t m = moby_spawn(0x27a);
            sw(P + 0xa8c, m);
            if (m != 0) {
                V(m + 0x10) = V(P + 0x80);
                V(m + 0x40) = V(P + 0x90);
                sh(m + 0x32, 0x40);
                sb(lwu(P + 0xa8c) + 0x31, 1);
                sw(lwu(P + 0xa8c) + 0x90, 0);
                const uint32_t hm = lwu(P + 0x2080);
                sd(lwu(P + 0xa8c) + 0x38, ld(hm + 0x38));
                moby_update_transform(lwu(P + 0xa8c));
                hero_mode_enter(3, 0x53, lwu(P + 0xa8c));
            }
        }
    }
    for (uint32_t e = OV(0x001798f0); e < OV(0x001798f0) + 0x230; e += 0x70) timer_tick_i32(i32_at(e));
    swc1(P + 0x22b8, u2f(0x461c3c00u));
    bool rising = false;
    if (lh(P + 0x30e) != 0 && sub_00233a78(P + 0xe0) < 0.0f) {
        if (lw(P + 0x21b4) != 0x20) {
            rising = true;  // (the count is left as it is)
        } else {
            Vec4 sum;
            vec_clear(sum);
            for (int32_t i = 0; i < 8; ++i) {
                const int32_t v = lw(P + 0x21b0) - i;
                Vec4 d;
                vec3_sub(d, V(P + 0x1b00 + 16 * static_cast<uint32_t>(ring32(v + 0x1f))), V(P + 0x1b00 + 16 * static_cast<uint32_t>(ring32(v + 0x1e))));
                vec3_add(sum, sum, d);
            }
            vec3_scale(sum, sum, 1.0f / 8.0f);
            if (vec3_length(sum) < lwc1(0x0015ed6c)) {
                sh(P + 0x1e2, lhu(P + 0x1e2) + 1);
                rising = true;
            }
        }
    }
    if (!rising) sh(P + 0x1e2, 0);
    sub_00221b68();
    const uint32_t mode = lbu(P + 0x20a4);
    (void)mode;
    sub_00233660(P + 0xd0, 0.0f, 0.0f, u2f(0x3f333333u));
    if (lbu(0x0013d4e2) != 0) {
        swc1(P + 0x2288, 12.0f);
        swc1(P + 0x228c, 4.5f);
    } else {
        swc1(P + 0x2288, 3.0f);
        swc1(P + 0x228c, 1.75f);
    }
    if (lwc1(P + 0x88) - lwc1(P + 0x2d8) < 4.0f) {
        V(P + 0xc0) = V(P + 0x80);
        swc1(P + 0xc8, lwc1(P + 0x2d8) + 0.5f);
    } else {
        V(P + 0xc0) = V(P + 0xd0);
    }
    if (lwc1(P + 0x88) < 1.0f) hero_lost();
}

// Enter the idle state of the movement mode PLAYER+0x20a4 (Veldin's 0x227638; Novalis has modes 0: 0 and 3: 0x53).
// orig: L1:0x002405a0 L2:0x0022ea90
// 2: 0x5a, 3: 0x53).
void hero_enter_idle_l1() {
    const uint32_t m = lbu(P + 0x20a4);
    if (m == 0) {
        hero_set_state(0, 1);
    } else if (m == 3) {
        hero_set_state(0x53, 1);
    }
}

// Landing (Veldin's 0x2293a8) without the hoverboard surface case (+0x12e2: state 0x2f): with R1 / L1
// the crouch 4; with the stick the run 2 (or a landing roll into it); fast or after a long fall the hard
// landing 3; else standing 0 (after a long jump, with the item's landing animation).
// orig: L1:0x00242198 L2:0x002301f0
// 0x2f; with R1 / L1 the crouch 4; with the stick the run 2 (or a landing roll into it); fast
// or after a long fall the hard landing 3; else standing 0 (after a long jump, with the item's
// landing animation).
void hero_land_l1() {
    sw(P + 0x2284, 0);
    hero_stick_move(0, 1.0f);
    if ((buttons() & 0xa) != 0) {
        hero_set_state(4, 0);
        hero_anim_set(0xd, 2, -2.0f);
        sub_002325e0(3, 0x12);
        return;
    }
    if (u2f(0x3e99999au) < lwc1(P + 0x229c) && lw(P + 0x1c4) == 0) {
        if (lbu(P + 0x12e2) != 0 || lbu(P + 0x20a9) != 0) {
            hero_set_state(2, 1);
            return;
        }
        if (hero_set_state(2, 0) == 0) return;
        sw(P + 0x2088, 1);
        int32_t blend = frame_scale_i(8);
        if (frame_scale_i(5) < lw(P + 0x418)) blend = frame_scale_i(0xe);
        hero_anim_set(4, lwu(OV(0x0015f6dc)), static_cast<float>(blend));
        sh(P + 0x3bc, 1);
        return;
    }
    const bool hard = (fs6c() * 3.0f < lwc1(P + 0x168) && frame_scale_i(3) < lw(P + 0x418)) || lw(P + 0x2084) == 0x10;
    if (hard) {
        if (hero_set_state(3, 0) == 0) return;
        hero_anim_set(5, 0, -1.0f);
        vec3_clamp_length(P + 0x150, fs6c() * 5.0f);
        return;
    }
    if (!(frame_scale_i(0xc) < lw(P + 0x418)) && (lw(P + 0xa98) & 2) == 0) return;
    if (hero_set_state(0, 0) == 0) return;
    if (sub_00205000(0) == 0x54) {
        const uint32_t set = sub_00205000(0);
        hero_anim_set(set, 0, static_cast<float>(frame_scale_i(0xf)));
    } else if ((lw(P + 0xa98) & 2) != 0) {
        hero_anim_set(sub_00205000(0), 0, -2.0f);
    }
}

// The wrench (item 8) in Novalis's build: as Veldin's, the slam 0x14 without the +0x5fc test.
// orig: (helper) L1:0x00240ed8 case 8, the wrench)
void item_wrench_l1(uint32_t mask) {
    const uint32_t mode = lwu(P + 0x208c);
    const bool ground = mode < 2 || (mode == 4 && lw(P + 0x418) != 0);
    if (ground) {
        if (lbu(P + 0x12ea) != 0) return;
        int32_t ago = 0;
        if (pad_button_history(mask, frame_scale_i(7), &ago) == 0) return;
        const uint32_t held = lwu(P + 0x1090);
        if (held == 0 || moby_is_class_0x47(held) == 0 || lbu(held + 0x20) != 0) return;
        const uint32_t st = lwu(P + 0x2084);
        if ((st == 1 || st == 0x1e) && frame_scale_i(0x14) < lw(P + 0x198)) {
            hero_hold_moby_start();
            return;
        }
        if ((buttons() & 0xa) != 0) {
            hero_set_state(0x15, 1);
        } else {
            hero_set_state(lh(P + 0x308) != 1 ? 0x13u : 0x70u, 1);
        }
        return;
    }
    const uint32_t st = lwu(P + 0x2084);
    bool air = st == 7 || st == 9 || st == 8 || st == 6 || st == 0x2d || st == 0x11;
    if (st == 0xe && 32.0f < lwc1(P + 0xaa8)) air = true;
    if (st == 0xb) {
        if (lw(P + 0x450) == 3) {
            if (20.0f < lwc1(P + 0xaa8)) air = true;
        } else if (18.0f < lwc1(P + 0xaa8)) {
            air = true;
        }
    }
    if (air && frame_scale_i(0xa) < lw(P + 0x198)) {
        int32_t back = frame_scale_i(0xa);
        if (lw(P + 0x2084) == 0xb || lw(P + 0x2084) == 0xe) back = frame_scale_i(0x12);
        if (item_pressed(mask, back) == 0) return;
        const float high = lh(P + 0x41e) != 0 ? u2f(0x3f11eb85u) : u2f(0x3dcccccdu);
        const bool slam = high < lwc1(P + 0x2dc) || (lw(P + 0x2084) == 8 && u2f(0x3f11eb85u) < lwc1(P + 0x2dc));
        if (slam) {
            hero_set_state(0x14, 1);
        } else if (lbu(P + 0x12ea) == 0) {
            hero_set_state(0x13, 1);
        }
        return;
    }
    if (lw(P + 0x208c) != 6) return;
    const uint32_t step = OV(0x0017bc28) + lwu(P + 0xa60) * 0x2c;  // the combo table
    if (lw(P + 0x2084) == 0x15 && frame_scale_i(0x1e) < lw(P + 0x198)) {
        if (lw(P + 0xa9c) != 0) return;
        if (static_cast<float>(lw(step + 0x10)) <= lwc1(P + 0xaa8) && (buttons() & 0xa) != 0 &&
            item_pressed(mask, frame_scale_i(0xf)) != 0) {
            hero_set_state(0x15, 1);
            return;
        }
    }
    const int32_t chained = lw(P + 0xa9c);
    if (chained != 0) return;
    if (!(static_cast<float>(lw(step + 0x10)) <= lwc1(P + 0xaa8))) return;
    int32_t back = (float_to_int(lwc1(P + 0xaa8)) - lw(step + 0xc)) * 2;
    if (!(chained < back)) back = 1;
    if (frame_scale_i(0xf) < back) back = frame_scale_i(0xf);
    if (lbu(P + 0x12ea) != 0) return;
    if (item_pressed(mask, back) == 0) return;
    if ((buttons() & 0xa) != 0) {
        hero_set_state(0x15, 1);
    } else {
        hero_set_state(lh(P + 0x308) == 1 ? 0x70u : 0x13u, 1);
    }
}

// A throwable / placeable item: the same code as Veldin's.
// orig: (helper) L1:0x00240ed8 cases 0xa, 0x11, 0x14, 0x18, 0x19)
void item_throw_l1(uint32_t mask) {
    const uint32_t mode = lwu(P + 0x208c);
    bool go;
    if (mode == 0) {
        go = lw(P + 0x2084) != 0x1e;
        if (!go) return;
    } else if (mode == 1 || mode == 0xc || mode == 5 || mode == 0xf || (mode == 4 && lh(P + 0x4ac) != 0)) {
        go = true;
    } else {
        if (lw(P + 0x2084) != 0x23) return;
        if (!(25.0f < lwc1(P + 0xaa8))) return;
    }
    if (lbu(P + 0x20a8) != 0) return;
    const int32_t hold = lw(P + 0x10b0);
    const int32_t f7 = frame_scale_i(7);
    int32_t ago = 0;
    if (pad_button_history(mask, hold < f7 ? hold : f7, &ago) == 0) {
        if ((buttons() & 0x20) == 0) return;
        if (!(frame_scale_i(8) < hold)) return;
        if (!(hold < frame_scale_i(0x11))) return;
    }
    if (sub_00233e98(-1) != 0) {
        const uint32_t m = lwu(P + 0x208c);
        if (m == 0xc || m == 0 || lw(P + 0x2084) == 3 || (m == 1 && lwc1(P + 0x229c) < u2f(0x3f333333u))) {
            hero_set_state(0x23, 1);
        } else {
            hero_hold_start();
        }
        return;
    }
    if (lwu(P + 0x1090) != 0) moby_sound_start(0, 0, lwu(P + 0x1090));
}

// The item in hand (+0x10b8, its moby +0x1090, button mask +0x10a0) used from the input (Veldin's
// 0x227fa0) without the swingshot (item 0xc); Aridia's build (L2:0x0022ee00) has it (Veldin's case 0xc).
// orig: L1:0x00240ed8 L2:0x0022ee00
int32_t hero_item_use_l1() {
    if (lbu(P + 0x20ac) != 0 && (lw(P + 0x10b8) != 8 || lh(P + 0x308) != 1)) return 0;
    if (lbu(P + 0x20a4) != 0) return 0;
    if (lw(P + 0x1090) == 0) return 0;
    if (advanced_rac3_wrench_out()) return 0;  // enhancement: R&C 3 takes no item input while the thrown wrench is out
    if (lw(P + 0x10b4) != 2) return 0;
    const int32_t t0 = lw(P + 0x198);
    const uint32_t mask = lwu(P + 0x10a0);
    switch (lwu(P + 0x10b8)) {
    case 8:
        item_wrench_l1(mask);
        break;
    case 0xc:
        if (on_aridia()) hero_item_swingshot(mask);
        break;
    case 0xa: case 0x11: case 0x14: case 0x18: case 0x19:
        item_throw_l1(mask);
        break;
    case 0xf: {  // a hold-to-use gadget
        const uint32_t mode = lwu(P + 0x208c);
        if (!(mode < 2 || mode == 0xc || mode == 2 || mode == 4 || mode == 5 || mode == 0xf || lw(P + 0x2084) == 0x1e)) break;
        if (lw(P + 0x1090) == 0) break;
        const uint32_t pv = lwu(lwu(P + 0x1090) + 0x78);
        bool start = ((buttons() & mask) != 0 && frame_scale_i(5) < lw(P + 0x10b0)) || (lwu(PAD + 0x1a4) & mask) != 0;
        if (start && sub_00233e98(-1) != 0) {
            if (lbu(P + 0x20a8) == 0) hero_hold_start();
            break;
        }
        if ((buttons() & mask) == 0 || sub_00233e98(-1) != 0) break;
        if (sub_0028d8c0(lwu(P + 0x1090), lw(pv + 0xc)) != 0) break;
        sw(pv + 0xc, static_cast<uint32_t>(moby_sound_start(0, 0, lwu(P + 0x1090))));
        break;
    }
    case 0x12: {  // the thruster
        const uint32_t mode = lwu(P + 0x208c);
        if (!(mode < 2 || (mode == 4 && lw(P + 0x418) != 0))) break;
        if (lw(P + 0x2084) == 1) break;
        const int32_t hold = lw(P + 0x10b0), f = frame_scale_i(0xa);
        int32_t ago = 0;
        if (pad_button_history(mask, hold < f ? hold : f, &ago) == 0) break;
        if (lw(P + 0x1090) == 0) break;
        hero_set_state(0x20, 1);
        if (lwu(P + 0x1090) != 0) moby_sound_start(0, 0, lwu(P + 0x1090));
        break;
    }
    case 0x15:
        if (lw(P + 0x1090) != 0 && (lwu(PAD + 0x1a4) & mask) != 0) hero_hold_start();
        break;
    case 0x1f:
        if (lbu(P + 0x20a4) != 0 || sub_0020fe30() == 0) break;
        if (item_pressed(mask, frame_scale_i(7)) == 0) break;
        if (lw(0x00141660) != 0x1f || lh(lwu(P + 0x1090) + 0xa6) != 0x1e3 || lh(P + 0x22de) != 0) break;
        hero_sound_start(0x18, 0);
        sh(P + 0x22dc, 1);
        sh(P + 0x22de, 0x12);
        break;
    case 0x20: {
        const uint32_t mode = lwu(P + 0x208c);
        if ((mode < 3 || mode == 4 || mode == 5) && item_pressed(mask, frame_scale_i(8)) != 0) {
            vendor_screen_start_tr(0);  // (0x29a6b0: the vendor screens with no Vendor moby)
        }
        break;
    }
    default:
        break;
    }
    if (lw(P + 0x198) == t0) {  // no state change: crouch on R1 over a low ceiling
        constexpr OvConst<0x0015f674u> TIMER{};
        const bool can = sub_0020d498(3) == 3 && lw(P + 0x2084) != 0x81 && (lh(P + 0x308) == 0 || lbu(0x0013d4dc) == 0) &&
                         lh(P + 0x22d8) == 0;
        if (can) {
            timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(TIMER)));
            if (lw(TIMER) != 0) {
                const uint32_t mode = lwu(P + 0x208c);
                const bool stand = mode < 2 || mode == 0xc || ((mode == 2 || mode == 4) && lw(P + 0x300) != 0);
                constexpr uint64_t BOTH = 0x0000000800000008ull;  // R1 held and pressed
                if (stand && (ld(PAD + 0x1a0) & BOTH) == BOTH) hero_set_state(0x81, 1);
            }
            if ((lwu(PAD + 0x1a4) & 8) != 0 && lw(P + 0x2084) != 0x81) sw(TIMER, static_cast<uint32_t>(frame_scale_i(0x1e)));
        } else {
            const uint32_t st = lwu(P + 0x2084);
            if (st != 0x13 && st != 0x81 && st != 0x15 && st != 0x16) sb(P + 0x22ca, 0);
        }
    }
    return lw(P + 0x198) < t0 ? 1 : 0;
}

}  // namespace rac1
