// Ported overlay code: the hero's per-frame update root (0x2070d0) and its direct helpers.
#include <game/ovl/hero_l1.h>
#include <game/ovl/hero_l2.h>
#include <game/ovl/hero_move_mode.h>
#include <game/ovl/variants_l10.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/screen_fade.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t P = g::PLAYER;
constexpr uint32_t PAD = 0x0013c940;

int32_t& i32_at(uint32_t a) { return *static_cast<int32_t*>(ee_ram_ptr(a)); }  // orig: (helper) EE word
int16_t& i16_at(uint32_t a) { return ee_ref<int16_t>(a); }  // orig: (helper) EE halfword

// orig: (helper) enter a state, forced (mode 1)
void set_state_forced(uint32_t st) { hero_set_state(st, 1); }

// the C remainder of a ring index by the 32-entry history length
// orig: (helper) the (i < 0 ? i + 31 : i) >> 5 idiom
int32_t ring32(int32_t i) { return i - ((i < 0 ? i + 31 : i) >> 5) * 32; }
}  // namespace

// The hero's per-frame timers and bookkeeping: the tilt (unless +0x20ad); the state / sub-state
// frame counters +0x198..+0x1a4; the countdown timers +0x1b0..+0x1f8; +0x1ec cleared without L1 /
// L2, +0x1bc counting the frames with an item in hand, +0x1a8 / +0x1ac the frames without / with
// +0x1d4 on the pad; the 0x53 set's countdown +0x22de: when it runs out with air left (and no
// cutscene 0x15f5c4) the mode 3 ends (the swing still queued on square), else the moby 0x27a is
// spawned at the hero and ridden in mode 3, state 0x53; mode 2's +0x1636; the 5 idle-event timers
// (0x1798f0, stride 0x70); +0x22b8 reset to 9999; +0x1e2 counts the airborne-and-rising frames whose
// average step over the last 8 positions (+0x1b00 ring) is under fs6c; the gait / camera offsets
// (+0xd0, +0x2288 / +0x228c) by mode; the camera focus +0xc0 (the feet + 0.5 when within 4 of the
// ground, else +0xd0); lost below z 1.
// orig: 0x002215c8
void hero_timers_update() {
    if (on_blarg()) { blg_call(blg_hero_timers_update_u); return; }  // (level 6's own: L6:0x002348d0)
    if (on_batalia()) { bat_call(bat_hero_timers_update_u); return; }  // (level 8's own: L8:0x0022fd98)
    if (on_pokitaru()) { pok_call(pok_hero_timers_update_u); return; }  // (level 11's own: L11:0x0024cdb0)
    if (on_hoven()) { hov_call(hov_hero_timers_update_u); return; }  // (level 12's own: L12:0x0023f330)
    if (on_oltanis()) { olt_call(olt_hero_timers_update_u); return; }  // (level 14's own: L14:0x0022f1b8)
    if (on_quartu()) { qua_call(qua_hero_timers_update_u); return; }  // (level 15's own: L15:0x00215e58)
    if (on_kalebo()) { kal_call(kal_hero_timers_update_u); return; }  // (level 16's own: L16:0x0021d5f8)
    if (on_veldin_finale()) { vf_call(vf_hero_timers_update_u); return; }  // (level 18's own: L18:0x00226ff0)
    if (on_veldin_orbit()) { vo_call(vo_hero_timers_update_u); return; }  // (level 17's own: L17:0x0021d760)
    if (on_l1_family() || on_rilgar()) return hero_timers_update_l1();
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
    if (lbu(P + 0x20a4) == 2) timer_tick_i16(i16_at(P + 0x1636));
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
    if (mode == 1) {
        sub_00233660(P + 0xd0, 0.0f, 0.0f, u2f(0x3ecccccdu));
        swc1(P + 0x2288, 2.125f);
        swc1(P + 0x228c, 1.25f);
    } else if (mode == 2) {
        sub_00233660(P + 0xd0, 0.0f, 0.0f, 4.0f);
        swc1(P + 0x2288, 15.0f);
        swc1(P + 0x228c, 3.0f);
    } else {
        sub_00233660(P + 0xd0, 0.0f, 0.0f, u2f(0x3f333333u));
        if (lbu(0x0013d4e2) != 0) {
            swc1(P + 0x2288, 12.0f);
            swc1(P + 0x228c, 4.5f);
        } else {
            swc1(P + 0x2288, 3.0f);
            swc1(P + 0x228c, 1.75f);
        }
    }
    if (lwc1(P + 0x88) - lwc1(P + 0x2d8) < 4.0f) {
        V(P + 0xc0) = V(P + 0x80);
        swc1(P + 0xc8, lwc1(P + 0x2d8) + 0.5f);
    } else {
        V(P + 0xc0) = V(P + 0xd0);
    }
    if (lwc1(P + 0x88) < 1.0f) hero_lost();
}

// The hero's control tick: the ground mode (+0x20b3), the stick tilt (+0x229c), the move input
// +0x1d20 (the left stick; the d-pad when the stick is under 0.25), the state update, the heading
// ring (+0x2128, 32 entries) and position ring (+0x1b00), the rotation matrix (+0x00) from the angles
// +0x90 and its transpose (+0x40), then the gait, the motion and the per-frame timers.
// orig: 0x00211670
void hero_tick() {
    const int32_t mode = sub_00233440();
    sb(P + 0x20b3, static_cast<uint32_t>(mode));
    const float tilt = sub_00211830();
    swc1(P + 0x229c, tilt);
    sw(P + 0x240, 0);
    swc1(P + 0x1d20, lwc1(PAD + 0x108));
    swc1(P + 0x1d24, lwc1(PAD + 0x10c));
    if (vec2_length(V(P + 0x1d20)) < 0.25f) {  // the d-pad: right - left, down - up
        const uint32_t b = lwu(PAD + 0x1b0);
        const int32_t x = static_cast<int32_t>((b >> 13) & 1) - static_cast<int32_t>((b >> 15) & 1);
        const int32_t y = static_cast<int32_t>((b >> 14) & 1) - static_cast<int32_t>((b >> 12) & 1);
        swc1(P + 0x1d20, static_cast<float>(x));
        swc1(P + 0x1d24, static_cast<float>(y));
    }
    hero_state_update();
    swc1(P + 0x2128 + 4 * static_cast<uint32_t>(lw(P + 0x21a8)), lwc1(P + 0x98));
    sw(P + 0x21a8, static_cast<uint32_t>(ring32(lw(P + 0x21a8) + 1)));
    sw(P + 0x21ac, static_cast<uint32_t>(lw(P + 0x21ac) + 1 < 0x21 ? lw(P + 0x21ac) + 1 : 0x20));
    V(P + 0x1b00 + 16 * static_cast<uint32_t>(lw(P + 0x21b0))) = V(P + 0x80);
    sw(P + 0x21b0, static_cast<uint32_t>(ring32(lw(P + 0x21b0) + 1)));
    sw(P + 0x21b4, static_cast<uint32_t>(lw(P + 0x21b4) + 1 < 0x21 ? lw(P + 0x21b4) + 1 : 0x20));
    euler_to_mat4_vu(M(P), V(P + 0x90));
    mat4_identity(M(P + 0x40));
    mat3_transpose(M(P + 0x40), M(P));
    hero_gait_update();
    hero_motion_update();
    hero_timers_update();
}

namespace {
// the hero's feet within 1 of the height h (0.25 above them) and below it, coming down
// orig: (helper) the hazard tests of hero_surface_update (blocks 0x20bcc8 / 0x20bd68 / 0x20be30)
bool sinking_into(float h) {
    if (!(abs_f(h - (lwc1(P + 0x88) + 0.25f)) < 1.0f)) return false;
    return 0.0f < h - lwc1(P + 0x88);
}
}  // namespace

// The surface under the hero (+0x12e0, consumed and reset to -1 here; +0x12e1.. the flags it sets,
// +0x12ed kept): 2 deep water (+0x12e7; the 0xad moby +0x10e0 sets +0x308), 0xe shallow water
// (+0x12e4 at 0.2), 0 water (+0x12e4, the depth +0x22a4, +0x20a9 wading at 0.25..0.85), 3 lava
// (+0x12e6), 0xb quicksand (+0x12eb), 4 slippery (+0x12e3), 7 (+0x12e2), 0xd (+0x12ec), 1 deadly
// water (+0x12e5), 8 / 0xc (+0x12ea), 9 (+0x12ee); on level 0xd a type-0xb wall at the gait centre
// sinks the hero (0x7b). Then the states they force: 0x31 sliding on slippery ground (not sub-states
// 0x10 / 0x14 / 7), the deadly water's 0x7d (on the ship) or drowning 0x3c / 0x7c, 0x7f, the lava's
// 0x68 / 0x7b and the quicksand's 0x7b once sunk in and coming down.
// orig: 0x0020b960
void hero_surface_update() {
    if (on_blarg()) { blg_call(blg_hero_surface_update_u); return; }  // (level 6's own: L6:0x00222fb8)
    if (on_batalia()) { bat_call(bat_hero_surface_update_u); return; }  // (level 8's own: L8:0x0021d9c0)
    if (on_pokitaru()) { pok_call(pok_hero_surface_update_u); return; }  // (level 11's own: L11:0x0023c7a0)
    if (on_hoven()) { hov_call(hov_hero_surface_update_u); return; }  // (level 12's own: L12:0x0022de30)
    if (on_oltanis()) { olt_call(olt_hero_surface_update_u); return; }  // (level 14's own: L14:0x0021dd30)
    if (on_quartu()) { qua_call(qua_hero_surface_update_u); return; }  // (level 15's own: L15:0x002044c8)
    if (on_kalebo()) { kal_call(kal_hero_surface_update_u); return; }  // (level 16's own: L16:0x002097a0)
    if (on_veldin_finale()) { vf_call(vf_hero_surface_update_u); return; }  // (level 18's own: L18:0x00213b40)
    if (on_rilgar()) { ril_call(ril_hero_surface_update_u); return; }  // (Rilgar's own: L5:0x00239fc0)
    if (on_novalis() || on_kerwan()) return hero_surface_update_l1();
    if (on_aridia()) return hero_surface_update_l2();
    const uint32_t keep = lbu(P + 0x12ed);
    const int32_t kind = lh(P + 0x12e0);
    mem_clear128(P + 0x12e0, 0x10);
    sb(P + 0x12ed, keep);
    sh(P + 0x12e0, 0xffff);
    sb(P + 0x20a9, 0);
    sh(P + 0x308, 0);
    if (kind == -1) return;
    if (kind == 2 && (lh(P + 0x30c) == 0 || lwc1(P + 0x2dc) < u2f(0x3e99999au))) {
        sb(P + 0x12e7, 1);
        if (lbu(P + 0x20a4) == 0) {
            const uint32_t m = lwu(P + 0x10e0);
            if (m != 0 && lh(m + 0xa6) == 0xad) sh(P + 0x308, 1);
        }
    }
    if (kind == 0xe) {
        sb(P + 0x12e4, 1);
        swc1(P + 0x22a4, u2f(0x3e4ccccdu));
        swc1(P + 0x2f0, lwc1(P + 0x2d8) + u2f(0x3e4ccccdu));
    }
    if (kind == 0) {
        const float d = lwc1(P + 0x2f0) - lwc1(P + 0x2d8);
        swc1(P + 0x22a4, d);
        if (d < u2f(0x3f59999au) && 0.25f < d) sb(P + 0x20a9, 1);
        sb(P + 0x12e4, 1);
    }
    if (kind == 3) sb(P + 0x12e6, 1);
    if (lw(0x0015ed84) == 0xd && lw(P + 0x2084) != 0x7b) {
        const float r = lwc1(P + 0x234) + u2f(0x3cf5c28fu);
        if (sphere_collide_v(V(P + 0xd0), r, 2, 0) != 0 && sub_001f35d8() == 0xb) {
            set_state_forced(0x7b);
            return;
        }
    }
    if (kind == 0xb) sb(P + 0x12eb, 1);
    if (kind == 2) sb(P + 0x12e7, 1);
    if (kind == 4) sb(P + 0x12e3, 1);
    if (kind == 7) sb(P + 0x12e2, 1);
    if (kind == 0xd) sb(P + 0x12ec, 1);
    if (kind == 1) sb(P + 0x12e5, 1);
    if (kind == 8) sb(P + 0x12ea, 1);
    if (kind == 9) sb(P + 0x12ee, 1);
    if (kind == 0xc) sb(P + 0x12ea, 1);
    if (lbu(P + 0x12e3) != 0 && lw(P + 0x300) != 0) {  // sliding
        const uint32_t sub = lwu(P + 0x208c);
        if (sub != 0x10 && sub != 0x14 && sub != 7) {
            set_state_forced(0x31);
            return;
        }
    }
    if (lbu(P + 0x12e5) != 0) {  // deadly water
        if (lw(P + 0x300) != 0 && lbu(P + 0x20a4) == 1) {
            if (lw(P + 0x2084) != 0x7d) {
                set_state_forced(0x7d);
                return;
            }
        }
        if (lbu(P + 0x12e5) != 0 && lw(P + 0x300) != 0 && !(lw(P + 0x2084) == 0x3c && lh(P + 0x41e) == 0)) {
            hero_mode3_exit();
            set_state_forced(lw(P + 0x22a8) != 0 ? 0x3cu : 0x7cu);
            return;
        }
    }
    if (lbu(P + 0x12ec) != 0 && lw(P + 0x2084) != 0x7f && sinking_into(lwc1(P + 0x2f0)) && lwc1(P + 0x108) < 0.0f) {
        hero_mode3_exit();
        set_state_forced(0x7f);
        return;
    }
    if (lbu(P + 0x12e6) != 0 && lw(P + 0x2084) != 0x68 && lw(P + 0x2084) != 0x7b && sinking_into(lwc1(P + 0x2f4))) {  // lava
        const bool falling = lw(P + 0x2084) != 0x69 || lh(P + 0x41e) != 0;
        if (falling && lwc1(P + 0x108) < 0.0f) {
            hero_mode3_exit();
            set_state_forced(lw(P + 0x22a8) != 0 ? 0x68u : 0x7bu);
            return;
        }
    }
    if (lbu(P + 0x12eb) == 0 || lw(P + 0x2084) == 0x7b) return;  // quicksand
    if (!sinking_into(lwc1(P + 0x2f4)) || !(lwc1(P + 0x108) < 0.0f)) return;
    hero_mode3_exit();
    set_state_forced(0x7b);
}

namespace {
constexpr OvConst<0x00179ac0u> ITEMS{};  // the item table, 0x4c per item (+8 slot, +0x10 / +0x14 moby classes, +0x18 hand)

// a freshly spawned hand moby: drawn (+0x32 0x20, +0x31 1), lit like the hero moby (+0x38), +0x73
// 0x18 when its class has +6
// orig: (helper) the spawn tail repeated in hero_equipment_spawn
void item_moby_init(uint32_t m, bool depth) {
    sh(m + 0x32, 0x20);
    sb(m + 0x31, 1);
    const uint32_t cls = lwu(m + 0x24);
    sd(m + 0x38, ld(lwu(P + 0x2080) + 0x38));
    if (depth && lbu(cls + 6) != 0) sb(m + 0x73, 0x18);
}
}  // namespace

// Spawns the hero's equipment mobys that are missing: the item in hand (slot by the item table's
// +8; the item +0x20d4, else 8 on 0x15ed90, else the last item 0x141660, else 8 the wrench; its
// textures uploaded and the three hand-bone blocks 0x140c40 cleared; the wrench starts animation 1
// and is held by square 0x80, others by circle 0x20), only within the item budget 0x15f3f8 and with
// no override +0x20b8 (but 0x24); the pack +0x1180 (+0x20e0, else 0x14166c, else 3 / 2 by
// 0x15ed94; the hidden flags with +0x22d8; the helipack's two rotors 0xa7) and its second part
// +0x1184; the helmet +0x1130 (+0x20dc or 0x141668); the boots +0x10e0 / +0x10e4 (0x141664); and
// the three gadget extras +0x11d0 / +0x11d4 / +0x1220 (items 0x21..0x23) when owned (0x13d4e1..3).
// orig: 0x0020ea80
void hero_equipment_spawn() {
    if (lwu(P + 0x1090) == 0 && lw(P + 0x10a4) < lw(OV(0x0015f3f8)) && (lw(P + 0x20b8) == 0 || lw(P + 0x20b8) == 0x24)) {
        uint32_t item = lwu(P + 0x20d4);
        if (item == 0) {
            if (lw(0x0015ed90) != 0) {
                item = 8;
            } else {
                item = lwu(0x00141660);
                if (item == 0) item = 8;
            }
        }
        item_class_load(lwu(ITEMS + item * 0x4c + 0x10), -1);
        for (uint32_t b = 0x00140c40; b < 0x00140c40 + 0x210; b += 0xb0) {
            mem_fill32(b, 0, 0xb0);
            sh(b + 0xa0, 0xffff);
        }
        sw(P + 0x10b8, item);
        const uint32_t slot = lwu(ITEMS + item * 0x4c + 8);
        const uint32_t sp = P + slot * 0x50;
        const uint32_t m = moby_spawn(lwu(ITEMS + lwu(sp + 0x10b8) * 0x4c + 0x10));
        if (m != 0) {
            item_moby_init(m, true);
            sw(sp + 0x1090, m);
            sw(sp + 0x10b4, 2);
            if (item == 8) {
                sw(sp + 0x10a0, 0x80);
                moby_anim_set(m, 1, 0, 1);
            } else {
                sw(sp + 0x10a0, 0x20);
            }
            sb(P + 0x20ab, lbu(ITEMS + lwu(P + slot * 0x50 + 0x10b8) * 0x4c + 0x18));
        }
    }
    if (lwu(P + 0x1180) == 0) {  // the pack
        uint32_t pack = lwu(P + 0x20e0);
        if (pack == 0) {
            pack = lwu(0x0014166c);
            if (pack == 0) pack = lw(0x0015ed94) != 0 ? 3u : 2u;
        }
        sw(P + 0x11a8, pack);
        const uint32_t m = moby_spawn(lwu(ITEMS + pack * 0x4c + 0x10));
        if (m != 0) {
            item_moby_init(m, true);
            sw(P + 0x11a4, 2);
            sw(P + 0x1180, m);
            if (lh(P + 0x22d8) != 0) {
                sh(m + 0x34, lhu(m + 0x34) | 0x41u);
            } else if (lw(P + 0x11a8) == 3) {
                spawn_moby_a7(0);
                spawn_moby_a7(1);
            }
        }
    }
    if (lwu(P + 0x1184) == 0) {  // the pack's second part
        const uint32_t m = moby_spawn(lwu(OV(0x00179b1c)));
        if (m != 0) {
            item_moby_init(m, true);
            sw(P + 0x11a4, 2);
            sw(P + 0x1184, m);
            if (lh(P + 0x22d8) != 0) sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        }
    }
    if (lwu(P + 0x1130) == 0 && (lwu(0x00141668) != 0 || lwu(P + 0x20dc) != 0)) {  // the helmet
        sw(P + 0x1158, lwu(P + 0x20dc) != 0 ? lwu(P + 0x20dc) : lwu(0x00141668));
        const uint32_t m = moby_spawn(lwu(ITEMS + lwu(P + 0x1158) * 0x4c + 0x10));
        if (m != 0) {
            sh(m + 0x32, 0x20);
            sb(m + 0x31, 1);
            const uint32_t f = lhu(m + 0x34) | 2u;
            const uint32_t cls = lwu(m + 0x24);
            const uint64_t lit = ld(lwu(P + 0x2080) + 0x38);
            sh(m + 0x34, f);
            sd(m + 0x38, lit);
            sw(m + 0x74, 0);
            if (lbu(cls + 6) != 0) sb(m + 0x73, 0x18);
            sh(m + 0x34, lhu(m + 0x34) | 0x800u);
            sw(P + 0x1154, 2);
            sw(P + 0x1130, m);
        }
    }
    if (lwu(P + 0x10e0) == 0 && lwu(0x00141664) != 0) {  // the boots
        const uint32_t boots = lwu(0x00141664);
        sw(P + 0x1108, boots);
        const uint32_t m = moby_spawn(lwu(ITEMS + boots * 0x4c + 0x10));
        if (m != 0) {
            item_moby_init(m, true);
            sw(P + 0x10e0, m);
            sw(P + 0x1104, 2);
        }
        const uint32_t m2 = moby_spawn(lwu(ITEMS + lwu(0x00141664) * 0x4c + 0x14));
        if (m2 != 0) {
            item_moby_init(m2, true);
            sw(P + 0x10e4, m2);
            sw(P + 0x1104, 2);
        }
    }
    if (lwu(P + 0x11d0) == 0 && lbu(0x0013d4e1) != 0) {
        const uint32_t m = moby_spawn(lwu(OV(0x0017a49c)));
        sw(P + 0x11f8, 0x21);
        if (m != 0) {
            item_moby_init(m, true);
            sw(P + 0x11d0, m);
            sw(P + 0x11f4, 2);
        }
    }
    if (lwu(P + 0x11d4) == 0 && lbu(0x0013d4e2) != 0) {
        const uint32_t m = moby_spawn(lwu(OV(0x0017a4e8)));
        sw(P + 0x11f8, 0x22);
        if (m != 0) {
            item_moby_init(m, false);
            sw(P + 0x11f4, 2);
            sw(P + 0x11d4, m);
        }
    }
    if (lwu(P + 0x1220) != 0 || lbu(0x0013d4e3) == 0) return;
    const uint32_t m = moby_spawn(lwu(OV(0x0017a534)));
    sw(P + 0x1248, 0x23);
    if (m == 0) return;
    item_moby_init(m, false);
    sw(P + 0x1244, 2);
    sw(P + 0x1220, m);
}

// The hero's item mobys each frame: +0x20ac / +0x20a7 (hands busy) on a 0xad-moby ride (+0x308 1), in
// sub-states 0x15 / 0x16 or while sub_0020d568 holds; +0x20ae (item hidden) likewise, or in
// sub-state 3 or state 0x12; then the missing equipment spawned, the slot mobys and gadgets
// updated and the slots stepped.
// orig: 0x00210928
void hero_items_update() {
    const bool riding = lh(P + 0x308) == 1 || lwu(P + 0x208c) - 0x15u < 2u || sub_0020d568() != 0;
    if (riding) {
        sb(P + 0x20ac, 1);
        sb(P + 0x20a7, 1);
    }
    if (sub_0020d568() != 0) {
        sb(P + 0x20ae, 1);
    } else {
        const uint32_t sub = lwu(P + 0x208c);
        const bool hide = sub == 0x15 || sub == 0x16 || sub == 3 || lw(P + 0x2084) == 0x12;
        sb(P + 0x20ae, hide ? 1u : 0u);
    }
    hero_equipment_spawn();
    hero_slot_mobys_update();
    sub_00234808();
    hero_gadget_glow();
    hero_slots_update(0x00140000);  // (the caller hero_update's $s1, handed through)
}

// The hero's per-frame update: off the map (x or y outside 2..1022) the screen fades and the hero
// is flagged lost (+0x20b1); in a movement mode (+0x20a4) the mode's own update; otherwise the
// animation, the control tick, the surface, the ledge / rail checks and the state transitions, then
// (still on foot) sounds, look and head motion, bones, the hero moby, items, layers, camera 7,
// footsteps and cheats; the map revealed around the hero (not in sub-state 0x16 / state 0x32), the
// hero moby's lighting, the clank glow and the shadow.
// orig: 0x002070d0
void hero_frame_update() {
    if (on_blarg()) { blg_call(blg_hero_frame_update_u); return; }  // (level 6's own: L6:0x0021ef38)
    if (on_batalia()) { bat_call(bat_hero_frame_update_u); return; }  // (level 8's own: L8:0x002194e0)
    if (on_pokitaru()) { pok_call(pok_hero_frame_update_u); return; }  // (level 11's own: L11:0x002382c8)
    if (on_hoven()) { hov_call(hov_hero_frame_update_u); return; }  // (level 12's own: L12:0x00229958)
    if (on_oltanis()) { olt_call(olt_hero_frame_update_u); return; }  // (level 14's own: L14:0x0021a058)
    if (on_quartu()) { qua_call(qua_hero_frame_update_u); return; }  // (level 15's own: L15:0x00200228)
    if (on_kalebo()) { kal_call(kal_hero_frame_update_u); return; }  // (level 16's own: L16:0x002051c8)
    if (on_veldin_finale()) { vf_call(vf_hero_frame_update_u); return; }  // (level 18's own: L18:0x0020f898)
    if (on_l1_family() || on_rilgar()) return hero_frame_update_l1();
    const float x = lwc1(P + 0x80);
    const float y = lwc1(P + 0x84);
    if (x < 2.0f || y < 2.0f || 1022.0f < x || 1022.0f < y) {
        screen_fade(frame_scale_i(0x10));
        sb(P + 0x20b1, 1);
        return;
    }
    sh(P + 0x1eb2, 0);
    sh(P + 0x1eb0, 0);
    if (lbu(P + 0x20a4) != 0) {
        // (the same code on Veldin, Eudora, Umbris, Gaspar and Orxon: one transliteration, OV addresses; Novalis's family, Blarg and Batalia dispatched above)
        if (on_veldin_orbit()) { vo_call(vo_hero_move_mode_u); return; }  // (level 17's own: L17:0x00203290, 0x8d0 bytes as Blarg's)
        hero_move_mode_u(0, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }
    hero_anim_advance();
    hero_tick();
    hero_surface_update();
    hero_ledge_grab_check();
    hero_rail_land_check();
    hero_rail_grab_check();
    hero_input_update();
    if (lbu(P + 0x20a4) != 0) return;
    hero_delayed_sounds();
    hero_look_fx();
    hero_wall_lean();
    hero_head_motion();
    hero_look_at_update();
    sub_0020a9e8();
    hero_head_look();
    clank_glow_blink();
    hero_aim_bones();
    bone_nodes_update();
    hero_moby_update();
    hero_chain_world(0);
    hero_items_update();
    hero_layers_tick();
    sub_00207c48();
    sub_00207a58();
    sub_00206cf8();
    hero_camera_7_update();
    hero_footsteps_update();
    cheat_input();
    sub_001f6c20();  // (empty; the original hands it the hero moby)
    if (lw(P + 0x208c) != 0x16 && lw(P + 0x2084) != 0x32) map_reveal(P + 0x80);
    moby_update_light(lwu(P + 0x2080));
    sub_002076a8();
    hero_shadow_update();
}

// The hero at the level start: the player block cleared, the hero moby (the first moby of class 0
// in 0x15ffd8..0x15ffdc) dropped onto the floor below it (0.5 up), taken as the hero moby (+0x2080,
// +0xa88, flag 2) with its position and facing, the ground and the hero moby updated, the respawn
// point 0x13e090 set (unless 0x160540); the last item 0x141660 defaults to 10; the 31 bone nodes
// (0x17a680) reset; the 8 slot targets +0x2218.. set to -1; the health +0x22a8 from 0x15eda0 (and
// its saved copies), the camera offsets +0x2288 / +0x228c, the idle-event timer +0x1010 (180..300
// frames), +0xd20 / +0x1014 / +0x1018.
// orig: 0x00204c60
void hero_init() {
    mem_fill32(P, 0, 0x2310);
    const uint32_t start = lwu(OV(0x0015ffd8)), end = lwu(OV(0x0015ffdc));
    for (uint32_t m = start; m < end; m += 0x100) {
        if (lh(m + 0xa6) != 0) continue;
        sw(P + 0x2080, m);
        const float z = floor_probe(0.5f, m + 0x10, 0);
        if (0.0f < z) swc1(lwu(P + 0x2080) + 0x18, z);
        const uint32_t hm = lwu(P + 0x2080);
        sh(hm + 0x34, lhu(hm + 0x34) | 2u);
        sw(P + 0xa88, hm);
        swc1(P + 0x98, lwc1(hm + 0x48));
        V(P + 0x80) = V(hm + 0x10);
        hero_ground_update();
        hero_moby_update();
        if (lw(OV(0x00160540)) == 0) {
            V(0x0013e090) = V(P + 0x80);
            V(0x0013e0a0) = V(P + 0x90);
        }
        break;
    }
    if (lw(0x00141660) == 0) sw(0x00141660, 10);
    for (uint32_t b = OV(0x0017a680); b < OV(0x0017a680) + 31 * 0xb0; b += 0xb0) {
        sb(b + 1, 0);
        for (uint32_t o : {0x60u, 0x40u, 0x50u, 0x90u, 0x70u, 0x80u}) vec_clear(V(b + o));
    }
    for (uint32_t o = 0x2218; o <= 0x2234; o += 4) sw(P + o, 0xffffffffu);
    sw(P + 0x22a8, lwu(0x0015eda0));
    swc1(P + 0x2288, 2.125f);
    swc1(P + 0x228c, 1.25f);
    sw(P + 0x22ac, 4);
    sh(P + 0x22b0, lhu(P + 0x22a8));
    const int32_t lo = frame_scale_i(0xb4);
    const int32_t hi = frame_scale_i(0x12c);
    sw(P + 0x1010, static_cast<uint32_t>(rand_range(lo, hi)));
    swc1(P + 0xd20, u2f(0x3f7851ecu));
    swc1(P + 0x1014, u2f(0x3be56042u));
    swc1(P + 0x1018, u2f(0x3e99999au));
}

}  // namespace rac1
