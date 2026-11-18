// Batalia (level 8) own moby classes, hand-ported: the fort's gates and bridges (classes 467 and 468, with 1.02 Fixes row
// 34 on class 467), the two controllers of the scrolling water and energy meshes (classes 600 and 1649), the sea mine
// (class 621), the grind-rail skill point and gadget-help triggers (class 1349), the cutscene ship-smoke helper (class
// 1553), and the steam vent with its particle spawner (class 1641). Registered in batalia_mobys.cpp. Addresses are level
// 8's (`orig: L8:`); its own data and draw callbacks are L8_ADDR, the base ELF's are bare.
#include <game/enhance/fixes102.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/batalia_own.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_scroll.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/variants_l8.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;
constexpr uint32_t SOUND_SLOTS = 0x0013e550;
constexpr uint32_t HELP_RECS = 0x00141968;

float dt() { return lwc1(0x0015ed6c); }
void zero_q(uint32_t a) { sd(a, 0); sd(a + 8, 0); }

// the heading from (x0, y0) to (x1, y1), as the original computes it
float heading(float x0, float y0, float x1, float y1) { return atan2_f(fpu_sub(x1, x0), fpu_sub(y1, y0)); }

// a path's point `i` (the points from +0x10, 0x10 each)
uint32_t path_pt(uint32_t path, int32_t i) { return path + 0x10 + (static_cast<uint32_t>(i) << 4); }

// the follower record `rec`'s next point (+0 the point's index, +4 the direction, a signed byte, +0x10 the path)
int32_t next_point(uint32_t rec) {
    const uint32_t path = lwu(rec + 0x10);
    const uint32_t dir = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(rec + 4))));
    return ee_rem(static_cast<int32_t>(lwu(rec) + lwu(path) + dir), lw(path));
}

// Batalia's two scrolling-layer updates (classes 600 and 1649): state 0 scales the three mesh sets' colours once and
// resets the scroll offsets and wave phase; state 1 scrolls the sets and waves while the centre is on screen (within 84
// of the camera for the first layer and its fade byte) and queues the draw callback.
struct ScrollLayerUpdate {
    uint32_t tinted_flag;
    uint32_t col0;
    int32_t cnt0_n;
    uint32_t cnt0;
    uint32_t col2;
    int32_t cnt2_n;
    uint32_t cnt2;
    uint32_t col1;
    int32_t cnt1_n;
    uint32_t cnt1;
    uint32_t off0, phase, off1, off2;
    uint32_t center, wave_out, fade, draw_cb;
    int32_t step0, step1, step2;
};

void scroll_layer_update(uint32_t m, const ScrollLayerUpdate& d) {
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        if (lw(d.tinted_flag) == 0) {
            mesh_colours_scale(d.col0, d.cnt0_n, d.cnt0, 1, 0.5f, 0.5f, u2f(0x3f266666u), 0.5f);
            mesh_colours_scale(d.col2, d.cnt2_n, d.cnt2, 1, 0.5f, 0.5f, 0.5f, 1.0f);
            mesh_colours_scale(d.col1, d.cnt1_n, d.cnt1, 1, 0.25f, 0.25f, 0.25f, 1.0f);
            sw(d.tinted_flag, 1);
        }
        sb(m + 0x20, 1);
        for (uint32_t i = 0; i < 3; ++i) {
            sw(d.off0 + i * 8, 0);
            sw(d.off0 + i * 8 + 4, 0);
            sw(d.phase, 0);
            sw(d.phase + 4, 0);
        }
        const float step_v = u2f(0x3ecccccdu);
        for (int32_t i = 0; i < 2; ++i) {
            const uint32_t at = static_cast<uint32_t>(i) * 8;
            sw(d.off1 + at, 0);
            swc1(d.off1 + at + 4, fpu_mul(static_cast<float>(i), step_v));
        }
        sw(d.off2, 0);
        swc1(d.off2 + 4, fpu_mul(0.0f, step_v));
        return;
    }
    if (st != 1) return;

    const uint32_t sp = ee_stack_alloc(112);
    const uint32_t pt = sp, rel = sp + 0x10;
    swc1(pt, lwc1(d.center));
    swc1(pt + 4, lwc1(d.center + 4));
    swc1(pt + 8, lwc1(d.center + 8));
    swc1(pt + 0xc, lwc1(d.center + 0xc));
    if (point_on_screen(pt, 1000.0f) >= 0) {
        swc1(rel, lwc1(d.center));
        swc1(rel + 4, lwc1(d.center + 4));
        swc1(rel + 8, lwc1(d.center + 8));
        swc1(rel + 0xc, 1.0f);
        vec3_sub(V(rel), V(L8_ADDR(0x001675c0)), V(rel));
        const float dist = vec3_length(V(rel));
        if (dist <= 84.0f) {
            uv_scroll_step(d.step0, 0);
            wave_step(d.wave_out, d.phase, u2f(0x3f051eb8u), u2f(0x3f1eb852u), u2f(0x3d75c28fu));
            uv_scroll_step(d.step0, 2);
            if (dist < 20.0f) {
                sb(d.fade, 0xff);
            } else if (dist < 84.0f) {
                const float t = fpu_sub(1.0f, fpu_mul(fpu_sub(dist, 20.0f), 0.015625f));
                sb(d.fade, static_cast<uint32_t>(float_to_int(fpu_mul(t, 255.0f))));
            } else {
                sb(d.fade, 0);
            }
        }
        uv_scroll_step(d.step2, 0);
        uv_scroll_step(d.step1, 0);
        uv_scroll_step(d.step1, 1);
        sub_001f93e0(d.draw_cb, m);
    }
    ee_stack_free(112);
}

// Class 1349's help-voice timer check on record `rec` (0x141968 + voice * 8): speaks `txt` (`voice`) when never stamped or
// past 18 scaled seconds since `rec + 2` (in 600-frame ticks), else advances `rec + 2` to the play time's 600-frame tick.
void help_repeat(uint32_t rec, int32_t txt, int32_t voice) {
    const int32_t now = frame_scale_i(lw(0x0015eea4));
    const int32_t last = static_cast<int32_t>(lhu(rec + 2)) * 0x258;
    const int32_t elapsed = now - last;
    const int32_t wait = static_cast<int32_t>(f2u(eer::cvt_w(fpu_mul(int_to_float(frame_scale_i(0x12)), 60.0f))));
    if (wait < elapsed || static_cast<int32_t>(lhu(rec + 2)) * 0x258 == 0) {
        sub_00203908(txt, voice);
        return;
    }
    const int32_t tick = ee_div(frame_scale_i(lw(0x0015eea4)), 0x258);
    if (static_cast<int32_t>(lhu(rec + 2)) < tick) {
        sh(rec + 2, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 0x258)));
    }
}

// First visit (`rec + 0 == 0`): marks `rec` seen on this planet without playing the repeat line yet; later visits run
// help_repeat.
void help_owned(uint32_t rec, int32_t txt, int32_t voice) {
    const uint32_t count = lhu(rec);
    if (count != 0) {
        help_repeat(rec, txt, voice);
        return;
    }
    sh(rec, count + 1);
    const int32_t tick = ee_div(frame_scale_i(lw(0x0015eea4)), 0x258);
    if (static_cast<int32_t>(lhu(rec + 2)) < tick) {
        sh(rec + 2, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 0x258)));
    }
    const uint32_t level = lwu(0x0015ed84);
    sw(rec + 4, lwu(rec + 4) | (1u << (level & 31u)) | 0x80000000u);
}
}  // namespace

// Class 467, a vertical gate / barrier driven by a Trespasser socket or a crank. pvars: +0x00 target height, +0x04 initial
// height, +0x08 linked moby index (-1: none). States: 0 saves +0x18 in +0x04, state 1; 1 waits on a class-615 / 575 socket
// to reach state 4 (sets 0x13d3bd = 1, sound 0, state 2) or eases +0x18 toward +0x04 + (+0x00 - +0x04) * link's +0x00 at
// 10 a second; 2 eases +0x18 toward +0x00 at 10 a second.
// (1.02 Fixes, fix 34, L8:0x002eb770 on the PAL 2.00 disc: with 0x13d3b6 set and +0x08 negative, retires the live mobys of
// classes 0x24c..0x256 and 0x1d8 in its group +0x21 and itself.)
// orig: L8:0x002ea398
void bat_class467_update_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (fixes102_batalia_gate_group() && lbu(0x0013d3b6) != 0 && lw(pv + 8) < 0) {
        const uint32_t list = lvd::group(static_cast<int32_t>(lbu(m + 0x21)));
        if (list == 0) return;
        for (uint32_t entry = list;; entry += 2) {
            const uint32_t o = lwu(L8_ADDR(0x0015ffd8)) + ((lhu(entry) & 0x7fffu) << 8);
            if (lbu(o + 0x20) < 0x7fu && o != m) {
                const uint32_t cls = lhu(o + 0xa6);
                if (cls - 0x24cu < 0xbu || static_cast<int16_t>(cls) == 0x1d8) moby_retire(o);
            }
            if (lh(entry) < -1) break;
        }
        moby_retire(m);
        return;
    }

    switch (lbu(m + 0x20)) {
    case 0:
        swc1(pv + 4, lwc1(m + 0x18));
        sb(m + 0x20, 1);
        break;
    case 1: {
        const int32_t idx = lw(pv + 8);
        if (idx < 0) break;
        const uint32_t link = lwu(L8_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8);
        const int16_t cls = lh(link + 0xa6);
        if (cls == 0x267 || cls == 0x23f) {
            if (lbu(link + 0x20) == 4) {
                sb(0x0013d3bd, 1);
                moby_sound_start(0, 0, m);
                sb(m + 0x20, 2);
            }
            break;
        }
        const float base = lwc1(pv + 4);
        const float target = fpu_add(fpu_mul(fpu_sub(lwc1(pv), base), lwc1(lwu(link + 0x78))), base);
        approach_f(m + 0x18, target, fpu_mul(dt(), 10.0f));
        break;
    }
    case 2:
        approach_f(m + 0x18, lwc1(pv), fpu_mul(dt(), 10.0f));
        break;
    default:
        break;
    }
}

// Class 468 (and its silent partner leaf), a drawbridge leaf tilted by its linked crank. pvars: +0x00 the crank's moby
// index (its pvars +0x00 is the progress 0 .. 1), +0x04 / +0x08 the end and start pitches in degrees, +0x0c the flag index
// in 0x13d3b8 once lowered, +0x10 its pivot position, +0x20 its looping sound slot (class 468 only). Hidden in game mode 2.
// orig: L8:0x002ea4c0
void bat_class468_update_l8(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(176);
    const uint32_t mat = sp, off = sp + 0x40;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t link = lwu(L8_ADDR(0x0015ffd8)) + (lwu(pv) << 8);
    const float frac = lwc1(lwu(link + 0x78));
    zero_q(off);
    swc1(off + 8, 30.0f);
    if (lw(L8_ADDR(0x0015f5c4)) == 2) {
        sb(m + 0x31, 0);
        sh(m + 0x34, lhu(m + 0x34) | 1u);
    } else {
        sb(m + 0x31, 1);
        sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
    }

    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sb(m + 0x20, 1);
        lq_sq(pv + 0x10, m + 0x10);
    } else if (st == 1) {
        const float deg = u2f(0x3c8efa35u);
        const float span = angle_sub(fpu_mul(lwc1(pv + 4), deg), fpu_mul(lwc1(pv + 8), deg));
        const float pitch = angle_add(fpu_mul(span, frac), fpu_mul(lwc1(pv + 8), deg));
        if (angle_dist(lwc1(m + 0x44), pitch) != 0.0f) {
            const int16_t cls = lh(m + 0xa6);
            swc1(m + 0x44, pitch);
            if (cls == 0x1d4) {
                if (sub_0028d8c0(m, lw(pv + 0x20)) == 0) {
                    const int32_t s = moby_sound_start(0, 4, m);
                    sw(pv + 0x20, static_cast<uint32_t>(s));
                    sound_slot_volume_set(s, 1);
                } else {
                    const int32_t s = lw(pv + 0x20);
                    const uint32_t slot = SOUND_SLOTS + static_cast<uint32_t>(s * 0x70);
                    if (lw(slot + 0x80) < 1024) {
                        const int32_t step = ee_div(0x400, frame_scale_i(0x3c));
                        const int32_t s2 = lw(pv + 0x20);
                        const uint32_t slot2 = SOUND_SLOTS + static_cast<uint32_t>(s2 * 0x70);
                        sound_slot_volume_set(s2, lwu(slot2 + 0x80) + static_cast<uint32_t>(step));
                    }
                }
            }
        } else if (lh(m + 0xa6) == 0x1d4 && sub_0028d8c0(m, lw(pv + 0x20)) != 0) {
            const int32_t s = lw(pv + 0x20);
            const uint32_t slot = SOUND_SLOTS + static_cast<uint32_t>(s * 0x70);
            if (lw(slot + 0x80) < 256) {
                sub_0028d918(s);
                sw(pv + 0x20, 0xffffffffu);
            } else {
                const int32_t step = ee_div(0x400, frame_scale_i(0x3c));
                const int32_t s2 = lw(pv + 0x20);
                const uint32_t slot2 = SOUND_SLOTS + static_cast<uint32_t>(s2 * 0x70);
                sound_slot_volume_set(s2, lwu(slot2 + 0x80) - static_cast<uint32_t>(step));
            }
        }
        euler_to_mat4_vu(M(mat), V(m + 0x40));
        mat4_transform(V(off), V(off), M(mat));
        vec3_add(V(m + 0x10), V(pv + 0x10), V(off));
        if (frac == 1.0f && lh(m + 0xa6) == 0x1d4) {
            sb(0x0013d388u + lwu(pv + 0xc) + 0x30u, 1);
            sb(m + 0x20, 2);
            moby_sound_start(1, 0, m);
            if (lh(m + 0xa6) == 0x1d4 && sub_0028d8c0(m, lw(pv + 0x20)) != 0) {
                sub_0028d918(lw(pv + 0x20));
            }
        }
    }
    ee_stack_free(176);
}

// Class 600, the first scrolling-layer controller (draw callback L8:0x002f0dc8).
// orig: L8:0x002f1548
void bat_class600_update_l8(uint32_t m) {
    scroll_layer_update(m, {L8_ADDR(0x00161e04), L8_ADDR(0x001e8310), 13, L8_ADDR(0x001e2858), L8_ADDR(0x001ddf20), 8,
                            L8_ADDR(0x001db518), L8_ADDR(0x001e2828), 11, L8_ADDR(0x001ddf40), L8_ADDR(0x001e8348),
                            L8_ADDR(0x00161e28), L8_ADDR(0x00161e10), L8_ADDR(0x00161e20), L8_ADDR(0x00161dc0),
                            L8_ADDR(0x001e8350), L8_ADDR(0x00161e08), L8_ADDR(0x002f0dc8), 8, 0, 1});
}

// Class 1649, the second scrolling-layer controller (draw callback L8:0x00309030).
// orig: L8:0x003097b0
void bat_class1649_update_l8(uint32_t m) {
    scroll_layer_update(m, {L8_ADDR(0x001623c4), L8_ADDR(0x001f8528), 8, L8_ADDR(0x001f4c60), L8_ADDR(0x001623e8), 2,
                            L8_ADDR(0x001623c8), L8_ADDR(0x001f4c48), 6, L8_ADDR(0x001f01c8), L8_ADDR(0x001f8548),
                            L8_ADDR(0x00162410), L8_ADDR(0x001623f8), L8_ADDR(0x00162408), L8_ADDR(0x00162380),
                            L8_ADDR(0x001f8550), L8_ADDR(0x001623f0), L8_ADDR(0x00309030), 9, 2, 3});
}

// Class 621, the sea mine. States: 0 raises +0x18 by 0.4, state 1; 1 explodes on contact (Ratchet within 2 in state 0x42,
// or touching it at 0x13f58c) or hit (mask 0x330000); 2 patrols its path (+0x00 / +0x04 / +0x10, path id +0x14; pitch spun
// by 0x161f70 degrees a second) at 3 a second, exploding near Ratchet at the path's end (within 24) or entering state 3;
// 3 fades its alpha +0x23 out at 128 over 30 scaled frames and retires at 0.
// orig: L8:0x002f44c0
void bat_class621_update_l8(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(240);
    const uint32_t goal = sp + 0x20, step = sp + 0x30, zero_v = sp + 0x40, alpha_f = sp + 0x50;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10;
    const float lift = u2f(0x3ecccccdu);  // (0.4)

    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x20, 1);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), lift));
        break;
    case 1: {
        const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
        if ((vec2_distance(V(pos), V(HERO_POS)) < 2.0f && lw(0x001413d4) == 0x42) || lwu(0x0013f58c) == m) {
            moby_sound_start(0, 0, m);
            explosion_effect(m, L8_ADDR(0x0015f580), pos, 0xa, 0x14, 0x3c, -1, 1, 1, -1, 0, 3.0f, 1.0f, 3.0f, 5.0f,
                             9.0f, 1.0f, 15.0f);
            moby_retire(m);
        } else if (hit != 0) {
            moby_sound_start(0, 0, m);
            explosion_effect(m, L8_ADDR(0x0015f580), pos, 0xa, 0x14, 0x3c, -1, 1, 1, -1, 0, 0.0f, 0.0f, 1.0f, 3.0f,
                             9.0f, 1.0f, 15.0f);
            moby_retire(m);
        }
        break;
    }
    case 2: {
        swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), lift));
        if (lw(pv + 0x14) == -1 || lw(lwu(pv + 0x10)) == 0) {
            moby_retire(m);
            break;
        }
        const float spin = fpu_mul(fpu_mul(lwc1(L8_ADDR(0x00161f70)), u2f(0x3c8efa35u)), dt());
        swc1(m + 0x44, angle_add(lwc1(m + 0x44), spin));
        if (vec2_distance(V(pos), V(HERO_POS)) < 2.0f && lw(0x001413d4) == 0x42) {
            vec_clear(V(zero_v));
            explosion_effect(m, zero_v, pos, 0xa, 0x14, 0x3c, -1, 1, 1, -1, 0, 0.0f, 0.0f, 3.0f, 5.0f, 9.0f, 1.0f, 15.0f);
            moby_retire(m);
            break;
        }
        const uint32_t path = lwu(pv + 0x10);
        const int32_t next = next_point(pv);
        const uint32_t p = path_pt(path, next);
        yaw_turn_damped(m, pv + 0x34, heading(lwc1(pos), lwc1(pos + 4), lwc1(p), lwc1(p + 4)), 0.01f, 0.3f, 0.1f);
        lq_sq(goal, path_pt(lwu(pv + 0x10), next));
        if (next != 0) {
            vec3_sub(V(step), V(goal), V(pos));
            vec3_set_length(V(step), V(step), fpu_mul(dt(), 3.0f));
            vec3_add(V(pos), V(pos), V(step));
        } else if (vec2_distance(V(pos), V(HERO_POS)) < 24.0f) {
            vec_clear(V(zero_v));
            explosion_effect(m, zero_v, pos, 0xa, 0x14, 0x3c, -1, 1, 1, -1, 0, 0.0f, 0.0f, 3.0f, 5.0f, 9.0f, 1.0f, 15.0f);
            moby_sound_start(0, 0, m);
            moby_retire(m);
            break;
        } else {
            sb(m + 0x20, 3);
        }
        if (vec3_distance(V(pos), V(goal)) < 0.1f) sw(pv, static_cast<uint32_t>(next));
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), lift));
        break;
    }
    case 3: {
        swc1(alpha_f, static_cast<float>(static_cast<int32_t>(lbu(m + 0x23))));
        const float step_a = fpu_div(128.0f, static_cast<float>(frame_scale_i(0x1e)));
        approach_f(alpha_f, 0.0f, step_a);
        const uint32_t na = f2u(eer::cvt_w(lwc1(alpha_f)));
        sb(m + 0x23, na);
        if ((na & 0xffu) == 0) moby_retire(m);
        break;
    }
    default:
        break;
    }
    ee_stack_free(240);
}

// Class 1349, Batalia's grind-rail skill point ("Jumper", flag 0x13d413, text 0x53d6: start box +0x08 while grinding, end
// box +0x0c without entering state 0x42) and its four gadget help prompts (boxes +0x00 for item 0x1d: voices 0x30 / 0x31,
// texts 0x1f40 / 0x1f41; +0x04 for item 0x1c with 0x13dd4a set: voices 0x32 / 0x70, texts 0x1f42 / 0x1f45).
// orig: L8:0x00307540
void bat_class1349_update_l8(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(144);
    const uint32_t pv = lwu(m + 0x78);
    const int32_t st = lw(pv + 0x10);
    if (st == 0) {
        if (point_in_box(HERO_POS, lw(pv + 8)) != 0 && lw(0x001413dc) == 0xf) {
            sw(pv + 0x10, 1);
        }
    } else if (st == 1) {
        if (lw(0x001413d4) == 0x42) {
            sw(pv + 0x10, 0);
        } else if (point_in_box(HERO_POS, lw(pv + 0xc)) != 0 && lbu(0x0013d413) == 0) {
            sb(0x0013d413, 1);
            sound_play_indexed(1, 0, 0);
            sub_00263d40(TXT(0x53d6), -1);
        }
    }

    if (lbu(0x0013d4dd) == 0) {
        if (point_in_box(HERO_POS, lw(pv)) != 0) {
            help_repeat(HELP_RECS + 0x30 * 8, TXT(0x1f40), 0x30);
        }
    }
    if (lbu(0x0013d4dd) != 0) {
        if (point_in_box(HERO_POS, lw(pv)) != 0 && lbu(0x0013d3b7) == 0) {
            help_owned(HELP_RECS + 0x31 * 8, TXT(0x1f41), 0x31);
        }
    }

    if (lbu(0x0013d4dc) == 0) {
        if (point_in_box(HERO_POS, lw(pv + 4)) != 0 && lbu(0x0013dd4a) != 0) {
            help_repeat(HELP_RECS + 0x32 * 8, TXT(0x1f42), 0x32);
        } else if (lbu(0x0013d4dc) != 0 && point_in_box(HERO_POS, lw(pv + 4)) != 0 && lbu(0x0013dd4a) != 0) {
            help_owned(HELP_RECS + 0x70 * 8, TXT(0x1f45), 0x70);
        }
    } else if (point_in_box(HERO_POS, lw(pv + 4)) != 0 && lbu(0x0013dd4a) != 0) {
        help_owned(HELP_RECS + 0x70 * 8, TXT(0x1f45), 0x70);
    }

    zero_q(sp);
    swc1(sp + 8, -1.0f);
    if (vec3_dot(V(0x0013f5e0), V(sp)) < u2f(0x3dcccccdu)) {
        sh(HELP_RECS + 0x70 * 8, 0xffffu);
    }
    if (lbu(0x0013d3b7) == 0 && lw(0x001413dc) == 0xf) {
        sb(0x0013d3b7, 1);
    }
    ee_stack_free(144);
}

// Class 1553, cutscene ship smoke: in game mode 2, emits scene_moby_smoke on slot 3 of scenes 1 (past frame 2254) and 4,
// and on slot 2 of scenes 2 and 5.
// orig: L8:0x003084d8
void bat_class1553_update_l8(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        return;
    }
    if (st != 1 || lw(L8_ADDR(0x0015f5c4)) != 2) return;
    const uint32_t scene = L8_ADDR(0x0016d060);
    const uint32_t id = lwu(L8_ADDR(0x0016d090));
    if (id - 1u >= 2u && id != 4u && id != 5u) return;
    if (lw(scene + 0x30) == 1 && !(frame_scale_i(0x8ce) < lw(scene + 0x34))) return;
    const uint32_t cur = lwu(scene + 0x30);
    uint32_t slot = 0;
    if (cur == 1 || cur == 4) slot = 3;
    else if (cur == 2 || cur == 5) slot = 2;
    scene_moby_smoke(lwu(scene + 0x178 + slot * 4));
}

// Spawns one steam puff (particle kind 0x4d, texture *0x1b29b4) at `pos` with velocity `vel`, lifetime `life` scaled
// frames, and size `scale * 210000`.
// orig: L8:0x0027fe00
void steam_puff_spawn_l8(uint32_t pos, uint32_t vel, int32_t life, float scale) {
    const uint32_t p = alloc_particle(0x4d);
    if (p == 0) return;
    lq_sq(p + 0x10, pos);
    sw(p + 4, 0x50504040u);
    sb(p + 9, static_cast<uint32_t>(float_to_int(2.0f) + 0x40));
    sb(p + 3, 0x48);
    sb(p + 1, 0);
    sb(p + 8, 0xa0);
    sb(p + 2, lbu(lwu(L8_ADDR(0x001b29b4))));
    swc1(p + 0xc, fpu_mul(scale, 210000.0f));
    const int16_t frames = static_cast<int16_t>(frame_scale_i(life));
    sh(p + 0xa, static_cast<uint32_t>(frames));
    lq_sq(p + 0x20, vel);
    swc1(p + 0x2c, int_to_float(frames));
}

// Class 1641, a steam vent: spawns +0x14 steam puffs each frame along (-1, -+0x04 .. +0x04, 0) in its local frame, with
// velocity (0, +0x18 .. +0x1c, +0x20 .. +0x24) * dt, lifetime +0x10, and scale +0x0c.
// orig: L8:0x00308c90
void bat_class1641_update_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(pv + 0x14) <= 0) return;
    const uint32_t sp = ee_stack_alloc(176);
    const uint32_t pos = sp, mat = sp + 0x10, vel = sp + 0x40;
    for (int32_t i = 0; i < lw(pv + 0x14); ++i) {
        swc1(pos, -1.0f);
        const float spread = lwc1(pv + 4);
        swc1(pos + 4, rand_range_f(-spread, spread));
        sw(pos + 8, 0);
        swc1(pos + 0xc, 1.0f);
        euler_to_mat3_vu(M(mat), V(m + 0x40));
        mat3_transform(V(pos), V(pos), M(mat));
        vec3_add(V(pos), V(pos), V(m + 0x10));
        sw(vel, 0);
        const float dt1 = dt();
        swc1(vel + 4, rand_range_f(fpu_mul(lwc1(pv + 0x18), dt1), fpu_mul(lwc1(pv + 0x1c), dt1)));
        const float dt2 = dt();
        swc1(vel + 8, rand_range_f(fpu_mul(lwc1(pv + 0x20), dt2), fpu_mul(lwc1(pv + 0x24), dt2)));
        sw(vel + 0xc, 0);
        steam_puff_spawn_l8(pos, vel, lw(pv + 0x10), lwc1(pv + 0xc));
    }
    ee_stack_free(176);
}

}  // namespace rac1
