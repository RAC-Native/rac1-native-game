// Ported overlay code: the hero's per-frame input -> state transitions (0x229b70) and the
// checks it runs before the per-state part.
#include <game/enhance/advanced.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/screen_fade.h>
#include <game/ovl/vendor_tr.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t P = g::PLAYER;
constexpr uint32_t PAD = 0x0013c940;

float fs6c() { return lwc1(0x0015ed6c); }
uint32_t buttons() { return lwu(PAD + 0x1a0); }
}  // namespace

// orig: 0x002276c0 — with an item held (+0x20a8, not +0x20ab) and standing (state 0, 2, 3)
// for 6 frames with the stick under 0.7: release the hold and enter the throw stance 0x23.
int32_t hero_try_item_stance() {
    const uint32_t st = lwu(P + 0x2084);
    if (!(st - 2u < 2u) && st != 0) return 0;
    if (lbu(P + 0x20a8) == 0) return 0;
    if (!(lw(P + 0x1bc) < frame_scale_i(6))) return 0;
    if (!(lwc1(P + 0x229c) < u2f(0x3f333333u))) return 0;
    if (lbu(P + 0x20ab) != 0) return 0;
    hero_hold_release();
    sw(P + 0xd14, 2);
    hero_set_state(0x23, 1);
    return 1;
}

// orig: 0x00228b78 — jump pressed within the last `back` frames: with R1 / L1 (0xa) held and a
// stick flick together with it, the long jump 0xb; else, unless `no_plain`, the jump 7.
int32_t hero_try_jump(int32_t back, int32_t no_plain) {
    int32_t ago = 0;
    if (pad_button_history(0x40, back, &ago) == 0) return 0;
    const int32_t t = hero_stick_turn_frames();
    if ((buttons() & 0xa) != 0 && 0 < t && pad_buttons_together(0x40, 0x1f000, back + t, t) != 0) {
        hero_set_state(0xb, 1);
        return 1;
    }
    if (no_plain != 0) return 0;
    hero_set_state(7, 1);
    return 1;
}

// orig: 0x00227778 — the air meter under water (+0x22a0, 10000 full; drained over 60 s unless
// 0x13d4c6, shown through camera slot 4) and the way out: drowning 0x6a when empty and not near
// the surface heading up, climbing out (6) once above the surface; on the surface (mode 0x12) a
// clear dive line starts swimming 0x33 (0x35 with the hydro-pack and R1 / L1).
int32_t hero_water_air() {
    if (lw(P + 0x208c) == 0x11) {
        int32_t used = 0;
        if (lbu(0x0013d4c6) == 0) {
            const int32_t span = frame_scale_i(0x3c) * 15;
            const int32_t per = span != 0 ? 10000 / span : 0;
            camera_request(4, 0x753f, OVF(0x00236268), OVF(0x00236458), OVF(0x002365a8), P + 0x22a0, 10000);
            used = per;
        }
        sw(P + 0x22a0, static_cast<uint32_t>(lw(P + 0x22a0) - used));
        if (lw(P + 0x22a0) < 0) sw(P + 0x22a0, 0);
        if (lw(P + 0x22a0) == 0) {
            const bool rising = lwc1(P + 0x2f0) - 2.0f < lwc1(P + 0x88) && lwc1(P + 0x94) < u2f(0xbf5f66f3u) &&
                                fs6c() < lwc1(P + 0x108);
            if (!rising) {
                hero_set_state(0x6a, 1);
                return 1;
            }
        }
        if (lwc1(P + 0x2f0) + u2f(0x3ecccccdu) < lwc1(P + 0x88)) {
            sw(OV(0x00167014), 0);
            hero_set_state(6, 1);
            return 1;
        }
    } else if (lw(P + 0x2084) != 0x6a && lw(P + 0x2084) != 0x76) {
        sw(P + 0x22a0, 10000);
    }
    if (lw(P + 0x208c) != 0x12) return 0;
    Vec4 a = V(P + 0x80);
    a.z = lwc1(P + 0x2f0) + u2f(0x3c23d70au);
    Vec4 b = a;
    b.z = b.z + u2f(0x3e99999au);
    if (segment_collide_v(a, b, 2, lwu(P + 0x2080), nullptr) == 0) return 0;
    hero_set_state(lbu(0x0013d4c4) != 0 && (buttons() & 0xa) != 0 ? 0x35u : 0x33u, 1);
    return 1;
}

// orig: 0x002279b0 — entering water: in water (+0x12e4) and close to its surface +0x2f0 for the
// fall speed, or rising out of the depth (mode 0x11), the hero goes to the surface state 0x37
// with a splash sound; every 16 frames a deep-enough spot below a non-water mode starts the dive
// 0x34. Returns whether the state changed.
int32_t hero_water_entry() {
    if (on_rilgar()) return static_cast<int32_t>(ril_call(ril_hero_water_entry_u).v0);  // (Rilgar's own: L5:0x002515d0)
    if (on_kalebo()) return static_cast<int32_t>(kal_call(kal_hero_water_entry_u).v0);  // (level 16's own: L16:0x00222d38)
    if (lw(P + 0x2084) == 0x6a) return 0;
    int32_t hit = 0;
    if (lbu(P + 0x12e4) != 0) {
        const uint32_t mode = lwu(P + 0x208c);
        if (mode == 0x11) {
            if (lw(P + 0x2088) != 1 && 0.0f < lwc1(P + 0x108) && lwc1(P + 0x2f0) - u2f(0x3ecccccdu) < lwc1(P + 0x88)) hit = 1;
        } else if (mode != 0x12 && mode != 3) {
            const float k = u2f(0x3d8f5c29u);
            float reach = 0.2f;
            if (0.2f < abs_f(lwc1(P + 0xe8)) + k) reach = abs_f(lwc1(P + 0xe8)) + k;
            const float d = abs_f(lwc1(P + 0x2f0) - (lwc1(P + 0x88) + u2f(0x3ee66666u)));
            const bool landing = !(lw(P + 0x208c) == 4 && lh(P + 0x41e) == 0) && u2f(0x3f4ccccdu) < lwc1(P + 0x22a4);
            if (d < reach && landing && lwc1(P + 0x108) < 0.0f) hit = 1;
            if (lwc1(P + 0x88) < lwc1(P + 0x2f0) - u2f(0x3f4ccccdu)) {
                if (lw(P + 0x208c) != 4 || lh(P + 0x41e) != 0) hit = 1;
            }
        }
    }
    // (the original then measures |+0xe8| + 0.07 against 0.27 and discards the result)
    if (lw(P + 0x2084) == 0x12 && lh(P + 0x41e) != 0 && lwc1(P + 0x88) < lwc1(P + 0x2f0) - u2f(0x3f333333u)) hit = 1;
    const uint32_t mode = lwu(P + 0x208c);
    if (!(mode - 0x11u < 2u) && mode != 7 && mode != 0x14 && lw(P + 0x2084) != 0x12) {
        const uint32_t t = lwu(OV(0x0015f5cc));
        if ((t & 0xf) == 0) {  // a dive check every 16 frames, a longer one every 64
            Vec4 a, b;
            sub_002126b8_v(a, b, 0, (t & 0x3f) == 0 ? 16.0f : 4.0f, u2f(0x3fa66666u));
            if (segment_collide_v(a, b, 2, lwu(P + 0x2080), nullptr) != 0) {
                const int32_t kind = sub_001f35d8();
                sh(P + 0x12e0, static_cast<uint32_t>(kind));
                if ((kind & 0xffff) == 0) {
                    a.z = lwc1(OV(0x00173e68)) - u2f(0x3c23d70au);
                    if (segment_collide_v(a, b, 2, lwu(P + 0x2080), nullptr) == 0) {
                        hero_set_state(0x34, 1);
                        return 1;
                    }
                }
            }
        }
    }
    if (hit != 0) {
        hero_set_state(0x37, 1);
        hero_sound_start(3, 0);
    }
    return hit;
}

// orig: 0x002293a8 — landing: onto a hoverboard surface (+0x12e2) at speed the board state
// 0x2f; with R1 / L1 the crouch 4; with the stick the run 2 (or a landing roll into it); fast
// or after a long fall the hard landing 3; else standing 0 (after a long jump, with the item's
// landing animation).
void hero_land() {
    if (on_quartu()) { qua_call(qua_hero_land_u); return; }  // (level 15's own: L15:0x0021c8a8)
    if (on_kalebo()) { kal_call(kal_hero_land_u); return; }  // (level 16's own: L16:0x00224a80)
    if (on_veldin_finale()) { vf_call(vf_hero_land_u); return; }  // (level 18's own: L18:0x0022e160)
    if (on_l1_family() || on_rilgar()) return hero_land_l1();
    sw(P + 0x2284, 0);
    if (hero_has_skate() && lbu(P + 0x12e2) != 0 && fs6c() * 0.5f < lwc1(P + 0x164)) {
        hero_set_state(0x2f, 1);
        return;
    }
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

// orig: 0x00229660 — the jump button pressed within 9 frames, 6 frames into the state: a flick
// with R1 / L1 gives the long jump 0xb; slot 3's ground kind (sub_0020d498) picks the wall jump
// 0xa or 0xf, the flip 0x10 (not while +0x1b8) or 0xd; else the plain jump 7.
int32_t hero_jump_input() {
    if (lw(P + 0x2084) == 0xe) return 0;
    const int32_t t = hero_stick_turn_frames();
    const int32_t back = frame_scale_i(5);
    if ((buttons() & 0xa) != 0 && 0 < t && pad_buttons_together(0x40, 0x1f000, back + t, t) != 0) {
        hero_set_state(0xb, 1);
        return 1;
    }
    const int32_t win = frame_scale_i(9);
    if (!(frame_scale_i(6) < lw(P + 0x198))) return 0;
    if (pad_button_history(0x40, win, nullptr) == 0) return 0;
    const float k = fs6c() * u2f(0x3f666666u);
    const bool wall = sub_0020d498(3) == 2 && lbu(0x0013d4c2) != 0 && lh(P + 0x22d8) == 0 &&
                      (2.5f < lwc1(P + 0x248) || lwc1(P + 0x250) < u2f(0x3f490fdbu)) &&
                      u2f(0x3f333333u) < lwc1(P + 0x229c) && k < lwc1(P + 0x168);
    if (wall) {
        hero_set_state(0xa, 1);
        return 1;
    }
    if (sub_0020d498(3) == 3 && lh(P + 0x22d8) == 0 && u2f(0x3f333333u) < lwc1(P + 0x229c) &&
        (2.5f < lwc1(P + 0x248) || lbu(P + 0x254) != 0) && fs6c() * u2f(0x3f666666u) < lwc1(P + 0x168)) {
        if (lw(P + 0x1b8) != 0) return 1;
        hero_set_state(0x10, 1);
        return 1;
    }
    if (sub_0020d498(3) == 3 && lh(P + 0x22d8) == 0) {
        hero_set_state(0xd, 1);
        return 1;
    }
    if (sub_0020d498(3) == 2 && lh(P + 0x22d8) == 0 && lbu(0x0013d4c2) != 0) {
        hero_set_state(0xf, 1);
        return 1;
    }
    hero_set_state(7, 1);
    return 1;
}

// A hit on the hero: the hero moby's contact record (ring 0x178100, flag 1, or
// 2 in mode 2) when not already hurting (+0x1c0), not invulnerable (0x15f5c4) and not in the
// modes / state that ignore hits. Counts the hit, keeps the attacker in +0x2280, takes the
// damage from the ship's shield +0x1630 in mode 2. With `knock`, the hurt state of the mode
// (special attackers: the 0x80 / 0x82 grabs; under water 0x76 / 0x75; on a ledge 0x6d) and a
// push from the record's direction, the attacker or the facing is added to the velocities.
// orig: 0x00210ce8
int32_t hero_hurt(int32_t knock) {
    if (on_quartu()) return static_cast<int32_t>(qua_call(qua_hero_hurt_u, static_cast<uint32_t>(knock)).v0);  // (level 15's own: L15:0x00208e10)
    if (on_kalebo()) return static_cast<int32_t>(kal_call(kal_hero_hurt_u, static_cast<uint32_t>(knock)).v0);  // (level 16's own: L16:0x0020e690)
    if (on_veldin_finale()) return static_cast<int32_t>(vf_call(vf_hero_hurt_u, static_cast<uint32_t>(knock)).v0);  // (level 18's own: L18:0x00218a90)
    if (on_l1_family() || on_rilgar()) return hero_hurt_l1(knock);
    if (lw(P + 0x208c) == 0x14 || lw(P + 0x208c) == 7) return 0;
    if (lw(P + 0x2084) == 0x32) return 0;
    if (lw(P + 0x1c0) != 0) return 0;
    if (lw(OV(0x0015f5c4)) != 0) return 0;
    sw(P + 0x2280, 0);
    const uint32_t hm = lwu(P + 0x2080);
    const uint32_t idx = lbu(hm + 0xa4);
    if (idx == 0xff) return 0;
    const uint32_t rec = OV(0x00178100) + idx * 64;
    if (lwu(rec + 0x34) != hm) return 0;
    if (hero_has_giant_clank() && lbu(P + 0x20a4) == 2) {
        if ((lw(rec + 0x24) & 2) == 0) return 0;
    } else if ((lw(rec + 0x24) & 1) == 0) {
        return 0;
    }
    const uint32_t lvl = lwu(0x0015ed84);
    sw(0x0013df88 + lvl * 4, lwu(0x0013df88 + lvl * 4) + 1);
    sw(0x0015eea8, lwu(0x0015eea8) + 1);
    sw(P + 0x2280, lwu(rec + 0x20));
    if (hero_has_grind() && lw(P + 0x208c) == 0xf) {
        sh(P + 0x5be, 1);
        return 0;
    }
    if (hero_has_giant_clank() && lbu(P + 0x20a4) == 2) {  // the ship's shield takes it
        sw(P + 0x1630, static_cast<uint32_t>(lw(P + 0x1630) - float_to_int(lwc1(rec + 0x2c))));
        if (lw(P + 0x1630) < 0) sw(P + 0x1630, 0);
    }
    if (knock == 0) return 1;
    sub_00206c08();
    int32_t scaled = 0;
    Vec4 push;  // (lanes the original leaves unset are stack garbage there)
    push.w = 0.0f;
    if ((lw(rec + 0x30) & 1) != 0) {
        push = V(rec + 0x10);
        if (lwc1(rec + 0x1c) == u2f(0x45afdf66u)) scaled = 1;
    } else if (lwu(rec + 0x20) != 0) {
        vec3_sub(push, V(P + 0x80), V(lwu(rec + 0x20) + 0x10));
    } else {
        push.x = cos_vu(angle_add(lwc1(P + 0x98), u2f(0x40490fdbu)));
        push.y = sin_vu(angle_add(lwc1(P + 0x98), u2f(0x40490fdbu)));
        push.z = 0.0f;
    }
    const uint32_t mode = lbu(P + 0x20a4);
    if (hero_has_clank() && mode == 1) {
        hero_set_state(0x46, 1);
        sub_00210c80_v(push, scaled, fs6c() * 5.0f, fs6c() * 3.0f);
    } else if (mode == 0) {
        const uint32_t att = lwu(P + 0x2280);
        const int32_t cls = att != 0 ? lh(att + 0xa6) : -1;
        if (att != 0 && (cls == 0x4eb || cls == 0x558)) {
            hero_set_state(0x80, 1);
            return 1;
        }
        const uint32_t sub = lwu(P + 0x208c);
        uint32_t st = 0x16;
        if (sub == 0x16) {  // hanging: let go
            hero_set_state(0x6d, 1);
            swc1(P + 0x128, fs6c() * 7.0f);
            return 1;
        }
        if (hero_has_water() && (sub == 0x12 || sub == 0x11 || sub == 3)) {
            if (att != 0 && cls == 0x28f) {
                hero_set_state(0x82, 1);
                return 1;
            }
            if (sub == 0x12) st = 0x75;
            if (sub == 0x11) {
                st = 0x76;
                const uint32_t l = lwu(0x0015ed84);
                if ((l == 0xf || l == 0x11) && att != 0 && (cls == 0x28f || cls == 0x7b || cls == 0x29d)) hero_sound_start(0x1c, 0);
            }
        }
        hero_set_state(st, 1);
        float s = fs6c() * u2f(0x40b66666u), z = fs6c() * u2f(0x4019999au);
        if (lbu(P + 0x12e7) != 0) {
            s = 0.0f;
            z = fs6c() * u2f(0x3fd9999au);
        }
        sub_00210c80_v(push, scaled, s, z);
        if (scaled != 0 && lbu(rec + 0x28) == 4) push.z = push.z + push.z;
    } else if (hero_has_giant_clank() && mode == 2) {
        if ((lw(rec + 0x24) & 4) != 0 || !(0 < lw(P + 0x1630))) {
            hero_set_state(0x5d, 1);
            remove_moby_5f3();
            if (lh(P + 0x30e) == 0) {
                sub_00210c80_v(push, scaled, fs6c() * 7.0f, fs6c() * 3.5f);
            } else {
                vec_clear(push);
            }
        } else {
            vec_clear(push);
        }
    } else if (mode == 3) {
        hero_set_state(0x56, 1);
        sub_00210c80_v(push, scaled, fs6c() * 5.0f, fs6c() * u2f(0x4019999au));
    }
    vec3_add(V(P + 0xe0), V(P + 0xe0), push);
    vec3_add(V(P + 0x100), V(P + 0x100), push);
    return 1;
}

// orig: 0x00211250 — the hero is lost (fallen out, drowned...): count it per level (0x13dfd8)
// and in total (0x15eeac), flag 0x15f5a4 when moby class 0x4d did it on level 15, count the
// attacker's kind (+0xb0) per level and in total, fade the screen out over 16 frames
// (0x1f9838, a render loop) and set the respawn flag +0x20b1.
void hero_lost() {
    const uint32_t lvl = lwu(0x0015ed84);
    sw(0x0013dfd8 + lvl * 4, lwu(0x0013dfd8 + lvl * 4) + 1);
    sw(0x0015eeac, lwu(0x0015eeac) + 1);
    const uint32_t att = lwu(P + 0x2280);
    if (att != 0 && lh(att + 0xa6) == 0x4d && lvl == 0xf) sw(OV(0x0015f5a4), 1);
    sub_00203a28();
    const uint32_t a = lwu(P + 0x2280);
    if (a != 0 && lbu(a + 0xb0) != 0xff) {
        const uint32_t kind = lbu(a + 0xb0);
        const uint32_t per = 0x0014e990 + kind * 4 + lwu(0x0015ed84) * 0x40;
        sw(per, lwu(per) + 1);
        sw(0x0014ee90 + kind * 4, lwu(0x0014ee90 + kind * 4) + 1);
    }
    screen_fade(frame_scale_i(0x10));
    sb(P + 0x20b1, 1);
}

namespace {
// the item's button pressed within `back` frames (pad history), 0 when not
uint32_t item_pressed(uint32_t mask, int32_t back) { return pad_button_history(mask, back, nullptr); }

// the wrench (item 8): swing 0x13 (0x70 when +0x308 is 1), with R1 / L1 the spin 0x15, out of
// the jump family the air swing 0xa (the slam 0x14 high above the ground), the combo
// continuation on the ground (mode 6) once the current step's window opened; standing in state
// 1 / 0x1e a while it picks up the held moby instead.
// orig: 0x00227fa0 (case 8)
void item_wrench(uint32_t mask) {
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
            if (!hero_has_state_74() || lw(P + 0x5fc) == 0) hero_set_state(0x14, 1);
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

// the swingshot (item 0xc): pick a target; on the ground (or state 0x1e) a press, or the button
// held while the hold window +0x10b0 is long enough, fires: at a target in reach (+0x99e) the
// pull 0x2c (unless already pulling or +0x1e6), else with a hook point (+0x968) the swing 0x24.
// orig: 0x00227fa0 (case 0xc)
void item_swingshot(uint32_t mask) {
    hero_pick_target_323();
    hero_pick_target_2f6();
    int32_t back = frame_scale_i(5);
    if (lh(P + 0x30e) != 0) back = frame_scale_i(8);
    const uint32_t mode = lwu(P + 0x208c);
    const bool ok = mode < 2 || mode == 4 || mode == 2 || mode == 0xf || mode == 5 || lw(P + 0x2084) == 0x1e;
    if (!ok) return;
    if (item_pressed(mask, back) == 0) {
        if ((buttons() & mask) == 0) return;
        if (!(frame_scale_i(0xf) < lw(P + 0x10b0))) return;
    }
    if (lh(P + 0x99e) != 0) {
        if (lw(P + 0x968) != 0) {
            if (lwc1(P + 0x998) < lwc1(P + 0x984)) {
                sw(P + 0x968, 0);
            } else {
                sh(P + 0x99e, 0);
            }
        }
        if (lh(P + 0x99e) != 0 && lh(P + 0x1e6) == 0 && lw(P + 0x2084) != 0x2c) {
            hero_set_state(0x2c, 1);
            return;
        }
    }
    if (lw(P + 0x968) == 0 || lh(P + 0x1e6) != 0) return;
    hero_set_state(0x24, 1);
}

// a throwable / placeable item (0xa, 0x11, 0x14, 0x18, 0x19): on the ground (or in the throw
// stance 0x23 after 25 frames of it), with nothing held, a press within min(7, +0x10b0) frames,
// or the button (0x20) let go after a hold of 8..17 frames: with the item's moby free
// (sub_00233e98) the throw stance 0x23 (on the move: the throw at once via hero_hold_start);
// else its use sound.
// orig: 0x00227fa0 (cases 0xa, 0x11, 0x14, 0x18, 0x19)
void item_throw(uint32_t mask) {
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
}  // namespace

// orig: (helper) the swingshot's case 0xc of hero_item_use, for Aridia's build (hero_l1_misc.cpp)
void hero_item_swingshot(uint32_t mask) { item_swingshot(mask); }

// The item in hand (+0x10b8, its moby +0x1090, button mask +0x10a0) used from the input: the
// per-item cases above; the hoverboots (0xf) toggle on a press (or a long enough hold), the
// glove (0x15) and the thruster (0x12, the jet 0x20) act on their button, the decoy (0x1f) on
// item 0x1e3's moby, item 0x20 starts its own mode (0x29a6b0). Then, when the state did not
// change, crouching 0x81 on R1 with the slot-3 kind 3 (once per 30 frames). Returns whether
// the state changed.
// orig: 0x00227fa0
int32_t hero_item_use() {
    if (on_quartu()) return static_cast<int32_t>(qua_call(qua_hero_item_use_u).v0);  // (level 15's own: L15:0x0021b4b8)
    if (on_kalebo()) return static_cast<int32_t>(kal_call(kal_hero_item_use_u).v0);  // (level 16's own: L16:0x00223350)
    if (on_veldin_finale()) return static_cast<int32_t>(vf_call(vf_hero_item_use_u).v0);  // (level 18's own: L18:0x0022cd70)
    if (on_rilgar()) return static_cast<int32_t>(ril_call(ril_hero_item_use_u).v0);  // (Rilgar's own: L5:0x00251be8)
    if (on_novalis() || on_aridia()) return hero_item_use_l1();
    if (lbu(P + 0x20ac) != 0 && (lw(P + 0x10b8) != 8 || lh(P + 0x308) != 1)) return 0;
    if (lbu(P + 0x20a4) != 0) return 0;
    if (lw(P + 0x1090) == 0) return 0;
    if (advanced_rac3_wrench_out()) return 0;  // enhancement: R&C 3 takes no item input while the thrown wrench is out
    if (lw(P + 0x10b4) != 2) return 0;
    const int32_t t0 = lw(P + 0x198);
    const uint32_t mask = lwu(P + 0x10a0);
    switch (lwu(P + 0x10b8)) {
    case 8:
        item_wrench(mask);
        break;
    case 0xc:
        if (hero_has_swingshot()) item_swingshot(mask);
        break;
    case 0xa: case 0x11: case 0x14: case 0x18: case 0x19:
        item_throw(mask);
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
