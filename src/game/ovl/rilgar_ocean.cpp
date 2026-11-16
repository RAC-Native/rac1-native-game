// Rilgar's (level 5) water: the open sea's controller (class 879: the ocean's swell height; its draw, 0x316070, stays generated:
// it is the only caller of the ocean's chain, rl2_2ce830_l5, which keeps the palette blend's hook) and the bay's water
// disc (class 982: the level's water disc, 0x1612dc, set over the hero while he is in its box), the waterfalls
// (class 855 and its drops) and the raft (class 998). Ported by hand from Rilgar's own code (L5:0x316110, 0x318a68,
// 0x3150f0, 0x29be70, 0x318c78, 0x3193a8), the original's order of calls and float operations kept.
#include <game/ovl/aridia_mobys.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/shared_floaters.h>
#include <game/ovl/shared_helpers.h>
#include <game/level/level_data.h>
#include <game/ovl/variants_l5.h>

namespace rac1 {

namespace {
constexpr uint32_t kHero = 0x0013f350;  // the hero's block (+0x80 his position, +0x1de, +0x208c his state: 0x16 racing)
float dt() { return lwc1(0x0015ed6c); }   // the frame's time scale
float dt2() { return lwc1(0x0015ed70); }  // its square
// the swell: 0.25 either side of `base`, one turn every 360 frames (the frame counter 0x15f5cc)
float swell(float base) {
    const int32_t phase = ee_rem(lw(L5_ADDR(0x0015f5ccu)), 0x168);
    const float a = fpu_sub(fpu_mul(static_cast<float>(phase), u2f(0x3c8ee7a7u)), u2f(0x4048f5c3u));  // (degrees, less 3.14)
    return fpu_add(fpu_mul(sin_vu(a), 0.25f), base);
}
}  // namespace

// Class 879, the open sea: its wave table set up once; every frame the swell's height (59.5, or 61.5 while the hero
// races) and its draw queued.
// orig: L5:0x00316110
void ril_class879_update_l5(uint32_t m) {
    switch (lbu(m + 0x20)) {
    case 0:  // (0x0031613c)
        ocean_wave_table_set_80(L5_ADDR(0x00211b20u), 1.0f);
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
        break;
    case 1:  // (0x00316164)
        swc1(L5_ADDR(0x00211b28u), swell(lw(kHero + 0x208c) == 0x16 ? 61.5f : 59.5f));
        draw_hook_add_dc80_l2(L5_ADDR(0x00316070u), m);
        break;
    default:
        break;
    }
}

// Class 982, the bay's water disc: while the hero is in its box (+0) the level's water disc (0x1612dc on, centred on him
// 0x1612e0, radius 48, at the swell's height about 59.5) and his +0x1de 5; else the disc off.
// orig: L5:0x00318a68
void ril_class982_update_l5(uint32_t m) {
    const uint32_t state = lbu(m + 0x20);
    switch (state) {
    case 0:  // (0x00318a9c) hidden, no collision
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        sh(m + 0x34, lhu(m + 0x34) | 1u);
        sw(m + 0x94, 0);
        break;
    case 1:  // (0x00318ac0)
        if (point_in_box(kHero + 0x80, lw(lwu(m + 0x78))) == 0) {
            sw(L5_ADDR(0x001612dcu), 0);
            break;
        }
        sw(L5_ADDR(0x001612dcu), state);
        lq_sq(L5_ADDR(0x001612e0u), kHero + 0x80);
        sh(kHero + 0x1de, 5);
        swc1(L5_ADDR(0x001612ecu), 48.0f);
        swc1(L5_ADDR(0x001612e8u), swell(59.5f));
        break;
    default:
        break;
    }
}

namespace {
// A drop of class 855's falls (particle kind 0x32) at `pos` moving `vel`, its colour byte `big`'s in the level's table
// 0x1b25c8, its texture word `tex`, its size `size` (as 210000ths), the water's height `floor` it splashes at and its
// fall `fall` a frame squared; its brightness and its spin at random, fading in 15 frames.
// orig: L5:0x0029be70
uint32_t fall_drop(uint32_t pos, uint32_t vel, uint32_t tex, uint32_t big, float size, float floor, float fall) {
    const uint32_t p = alloc_particle(0x32);
    if (p == 0) return 0;
    lq_sq(p + 0x10, pos);
    sb(p + 9, static_cast<uint32_t>(float_to_int(1.0f)) + 0x40);
    sb(p + 3, 0x44);
    sb(p + 1, 0);
    sw(p + 4, tex);
    sb(p + 2, lbu(lwu(L5_ADDR(0x001b25c8u)) + big));
    swc1(p + 0xc, fpu_mul(size, 210000.0f));
    sb(p + 8, f2u(eer::cvt_w(rand_range_f(0.0f, 255.0f))));
    sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0xf)));
    lq_sq(p + 0x20, vel);
    swc1(p + 0x30, floor);
    sw(p + 0x34, static_cast<uint32_t>(float_to_int(rand_range_f(-3.0f, 3.0f))));
    swc1(p + 0x38, fall);
    return p;
}
// the water body `i`'s height (the level's water list 0x1612d0, 0x1190 bytes each)
uint32_t water_at(int32_t i) { return lwu(L5_ADDR(0x001612d0u)) + static_cast<uint32_t>(i * 0x1190) + 8; }
}  // namespace

// Class 855, a waterfall: set up when its slot (+0xb0) in the level's table of things done (0x14c050, 16 a level) is
// not done (state 1), else as at the bottom (3). In state 1 it waits for its switch moby (+8 of the pvars) to set its
// group (+0x21) to 2. Falling (state 2) it counts its frames (+0x10), keeps its roar (the sound 0xd, its slot in the
// sound table 0x13e550 at +0xc's word; 2.5 out along its facing) while within 64 of the point 0x1671c0 and nearer than
// the class mate holding the slot; at its water body's height (+4) it is at the bottom (3). Each frame it lets go of
// the level's number of drops (0x161eb8), big or small (0x161ebc percent big), across its width (0x161eb4) at the
// fastest of three speeds (0x161eac / 0x161eb0 .. 0x161ea8), each a drop (ril_29be70) and, every other one, its splash
// where it will meet the water when that is within the frames fallen so far.
// orig: L5:0x003150f0
void ril_class855_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {  // (0x00315174)
        const uint32_t done = lbu(0x0014c050 + lbu(m + 0xb0) + (lwu(0x0015ed84) << 4));
        sb(m + 0x20, done == 0xff ? 3u : 1u);
        return;
    }
    if (state == 1) {  // (0x003151ac)
        const int32_t s = lw(pv + 8);
        if (s < 0) return;
        if (lbu(lwu(L5_ADDR(0x0015ffd8u)) + (static_cast<uint32_t>(s) << 8) + 0xbc) != 0) moby_group_set_state(lbu(m + 0x21), 2);
        return;
    }
    if (state != 2) return;
    const uint32_t sp = ee_stack_alloc(0x30);
    const uint32_t ear = L5_ADDR(0x001671c0u);
    constexpr uint32_t kSlots = 0x0013e550;  // the sound slots (0x70 bytes: +0x88 the moby, +0x90 the place)
    sw(pv + 0x10, lwu(pv + 0x10) + 1);
    if (vec3_distance(V(m + 0x10), V(ear)) < 64.0f) {  // (0x0031520c) the roar
        const int32_t slot = lw(lwu(pv + 0xc));
        bool start = true, take = false;
        if (slot >= 0) {
            const uint32_t owner = lwu(kSlots + static_cast<uint32_t>(slot * 0x70) + 0x88);
            if (owner != 0 && lh(owner + 0xa6) == 0x357) {
                start = false;
                if (owner != m) {
                    if (lbu(owner + 0x20) != state) {
                        take = true;
                    } else {
                        const float mine = vec3_distance(V(m + 0x10), V(ear));
                        take = mine < vec3_distance(V(owner + 0x10), V(ear));
                    }
                }
            }
        }
        if (take) {  // (0x00315294) the slot is this one's
            vec3_set_length(V(sp), V(m + 0xc0), 2.5f);
            const int32_t i = lw(lwu(pv + 0xc));
            vec3_add(V(kSlots + 0x90 + static_cast<uint32_t>(i * 0x70)), V(sp), V(m + 0x10));
            sw(kSlots + static_cast<uint32_t>(lw(lwu(pv + 0xc)) * 0x70) + 0x88, m);
        } else if (start) {  // (0x003152e4)
            sw(lwu(pv + 0xc), static_cast<uint32_t>(moby_sound_start(0, 0xd, m)));
            if (lw(lwu(pv + 0xc)) >= 0) {
                vec3_set_length(V(sp), V(m + 0xc0), 2.5f);
                vec3_add(V(0x0013e5e0 + static_cast<uint32_t>(lw(lwu(pv + 0xc)) * 0x70)), V(sp), V(m + 0x10));
            }
        }
    }
    const float floor = lwc1(water_at(lw(pv + 4)));
    if (lwc1(m + 0x18) < floor) sb(m + 0x20, 3);
    for (int32_t n = 0; static_cast<float>(n) < lwc1(L5_ADDR(0x00161eb8u)); ++n) {  // (0x00315398) the drops
        const uint32_t big = lwc1(L5_ADDR(0x00161ebcu)) < rand_range_f(0.0f, 100.0f) ? 0u : 1u;
        const float slow = big != 0 ? lwc1(L5_ADDR(0x00161eacu)) : lwc1(L5_ADDR(0x00161eb0u));
        lq_sq(sp, m + 0x10);
        const float w = lwc1(L5_ADDR(0x00161eb4u));
        vec3_set_length(V(sp + 0x20), V(m + 0xd0), rand_range_f(-w, w));
        vec3_add(V(sp), V(sp), V(sp + 0x20));
        float speed = rand_range_f(slow, lwc1(L5_ADDR(0x00161ea8u)));
        const float s1 = rand_range_f(slow, lwc1(L5_ADDR(0x00161ea8u)));
        const float s2 = rand_range_f(slow, lwc1(L5_ADDR(0x00161ea8u)));
        if (speed < s1) speed = s1;
        if (speed < s2) speed = s2;
        vec3_set_length(V(sp + 0x10), V(m + 0xc0), fpu_mul(speed, dt()));
        float size;
        uint32_t tex;
        if (big != 0) {
            size = lwc1(L5_ADDR(0x00161ea0u));
            tex = lwu(L5_ADDR(0x00161e98u));
            const float r = rand_range_f(-0.4f, 0.4f);
            swc1(sp + 8, fpu_add(lwc1(sp + 8), fpu_add(fpu_sub(lwc1(L5_ADDR(0x00161eb4u)), size), r)));
        } else {
            size = lwc1(L5_ADDR(0x00161ea4u));
            tex = lwu(L5_ADDR(0x00161e9cu));
            swc1(sp + 8, fpu_add(lwc1(sp + 8), fpu_add(fpu_sub(lwc1(L5_ADDR(0x00161eb4u)), size), 0.4f)));
        }
        const float k = rand_range_f(0.8f, 1.2f);
        fall_drop(sp, sp + 0x10, tex, big, fpu_mul(size, k), floor, fpu_mul(dt2(), 10.0f));
        if (rand_mod(2) == 0) continue;
        // (0x00315564) the splash: the frames the drop takes to the water, if fewer than this fall's frames so far
        const float t = sqrt_f(fpu_div(fpu_add(fpu_sub(lwc1(m + 0x18), floor), fpu_sub(lwc1(m + 0x18), floor)),
                                       fpu_mul(dt2(), 10.0f)));
        if (!(t < static_cast<float>(lw(pv + 0x10)))) continue;
        const float out = fpu_mul(t, fpu_mul(dt(), 4.0f));
        const float a = rand_angle();
        const float r = rand_range_f(0.0f, 1.25f);
        vec3_scale(V(sp), V(m + 0xc0), out);
        vec3_add(V(sp), V(sp), V(m + 0x10));
        swc1(sp, fpu_add(lwc1(sp), fpu_mul(cos_vu(a), r)));
        swc1(sp + 4, fpu_add(lwc1(sp + 4), fpu_mul(sin_vu(a), r)));
        swc1(sp + 8, fpu_add(floor, u2f(0x3d4ccccdu)));
        const float sz = rand_range_f(1.0f, 1.5f);
        const float spin = rand_mod(2) == 0 ? -2.0f : 2.0f;
        spawn_particle_2e(sp, L5_ADDR(0x0015f580u), water_at(lw(pv + 4)), sz, spin);
    }
    ee_stack_free(0x30);
}

namespace {
// The raft's next ride (class 998): by its stage (+0xb4 of the pvars) the path to ride (the level's path at +0x94 /
// +0x98 / +0x9c or, the slot +0xb8 done, +0xa4 / +0xa0 / +0xa8) and the stage it ends at (+0xb6); its progress
// (+0xc0, +0xc4) back to 0 and its pace (+0xd4 a frame, +0xd8 a frame squared) one over the path's length.
// orig: L5:0x003193a8
void raft_route(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t stage = static_cast<uint32_t>(lh(pv + 0xb4));
    if (stage < 5) {
        uint32_t word = 0, ends = 0;
        switch (stage) {  // (the table 0x218ac0)
        case 0: word = 0x94; ends = 1; break;
        case 1: word = 0x98; ends = 0; break;
        case 2:
            if (lbu(0x0014c050 + lwu(pv + 0xb8) + (lwu(0x0015ed84) << 4)) == 0xff) {
                word = 0xa4; ends = 4;
            } else {
                word = 0x9c; ends = 3;
            }
            break;
        case 3: word = 0xa0; ends = 2; break;
        default: word = 0xa8; ends = 2; break;
        }
        const uint32_t path = lvd::path(lw(pv + word));
        sh(pv + 0xb6, ends);
        sw(pv + 0xac, path);
    }
    const uint32_t path = lwu(pv + 0xac);
    const float len = fpu_mul(vec3_distance(V(path + 0x10), V(path + 0x20)), static_cast<float>(lw(path)));
    const float a = fpu_mul(dt2(), 5.0f), v = fpu_mul(dt(), 20.0f);
    sw(pv + 0xc4, 0);
    sw(pv + 0xc0, 0);
    const float inv = fpu_div(1.0f, len);
    swc1(pv + 0xd8, fpu_mul(a, inv));
    swc1(pv + 0xd4, fpu_mul(v, inv));
}
bool slot_done(uint32_t pv, uint32_t word) { return lbu(0x0014c050 + lwu(pv + word) + (lwu(0x0015ed84) << 4)) == 0xff; }
}  // namespace

// Class 998, the raft: it bobs (its phases +0xc8) and carries what stands on it. Waiting at a stage (state 1) it shows
// its message (the stage's word +0xb6: 0x13a5 / 0x1398) while the hero stands on it within its deck (1.2 across, -0.75
// to 1 along) and, with the button (0x13cae4 bit 0x10), sets off (3): its next ride picked (raft_route), the raft at the
// other end told (2, hidden). It glides from its stage's camera point (+0x80 by stage) to the path's start (3), rides
// the path (4: floater_ride) and glides from its end to the next stage's point (5), then waits there (1, the next
// stage). While it moves the hero is held to it (0x13f542 / 0x13f544) and its path laid out, and its sound (4, the
// slot +0xb2) kept; stopped, its sound stops. State 6 waits for one of its two slots (+0xb8, +0xbc) in the level's table
// of things done (0x14c050) and then comes back (shown, its first stage's place).
// orig: L5:0x00318c78
void ril_class998_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x80);
    lq_sq(sp, m + 0x10);
    lq_sq(sp + 0x10, m + 0x40);
    talker_word_set(m, (lhu(m + 0x34) ^ 1u) & 1u);
    // the glide between a stage's camera point (`at` its place, `at` + 0x20 its turn) and the path's end (`from` + 0 /
    // + 0x20): the raft's ride point and turn (+0x60, +0x74 / +0x78), done at 1
    auto glide = [&](uint32_t stage_word, bool to_path) {
        seek_1d(pv + 0xc0, pv + 0xc4, 1.0f, dt2(), dt2(), fpu_mul(dt(), 0.5f));
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + static_cast<uint32_t>(lh(pv + stage_word) << 2) + 0x80));
        const uint32_t pt = to_path ? sp + 0x30 : sp + 0x40;  // (the camera point's place and turn: sp + 0x30 / 0x50, or 0x40 / 0x60)
        lq_sq(pt, box + 0x30);
        lq_sq(pt + 0x20, box + 0x70);
        const uint32_t path = lwu(pv + 0xac);
        if (to_path) path_point(0.0f, path, 0, sp + 0x40, sp + 0x60, 0);
        else path_point(static_cast<float>(lw(path)), path, 0, sp + 0x30, sp + 0x50, 0);
        vec3_lerp(V(pv + 0x60), V(sp + 0x30), V(sp + 0x40), lwc1(pv + 0xc0));
        swc1(pv + 0x74, angle_add(fpu_mul(angle_sub(lwc1(sp + 0x64), lwc1(sp + 0x54)), lwc1(pv + 0xc0)), lwc1(sp + 0x54)));
        swc1(pv + 0x78, angle_add(fpu_mul(angle_sub(lwc1(sp + 0x68), lwc1(sp + 0x58)), lwc1(pv + 0xc0)), lwc1(sp + 0x58)));
        return 1.0f <= lwc1(pv + 0xc0);
    };
    switch (lbu(m + 0x20)) {  // (the table 0x218a80)
    case 0:  // (0x00318d04)
        sh(pv + 0xb2, 0xffff);
        sh(pv + 0xb0, 0xffff);
        floater_arrive_l5(m);
        break;
    case 1: {  // (0x00318d20) waits at its stage
        if (lwu(0x0013f64c) != m || lh(0x0013f65e) != 0) break;
        mat3_transpose(M(sp + 0x30), M(m + 0xc0));
        vec3_sub(V(sp + 0x70), V(0x0013f3d0), V(m + 0x10));
        sw(sp + 0x7c, 0);
        mat4_transform(V(sp + 0x70), V(sp + 0x70), M(sp + 0x30));
        if (!(abs_f(lwc1(sp + 0x74)) < u2f(0x3f99999au))) break;
        const float along = lwc1(sp + 0x70);
        if (!(along < 1.0f) || !(-0.75f < along)) break;
        raft_route(m);
        floater_message(m, 0xb6, TXT(0x13a5), TXT(0x1398));
        if ((lwu(0x0013cae4) & 0x10u) == 0) break;
        sb(m + 0x20, 3);
        const uint32_t far = floater_find(m, lh(pv + 0xb6), 0xb4);
        if (far == 0) break;
        sb(far + 0x20, 2);
        sw(far + 0x94, 0);
        sh(far + 0x34, lhu(far + 0x34) | 1u);
        sb(far + 0x31, 0);
        break;
    }
    case 3:  // (0x00318e30) to the path's start
        if (glide(0xb4, true)) {
            swc1(pv + 0xc4, 0.0f);
            swc1(pv + 0xc0, 0.0f);
            sb(m + 0x20, 4);
        }
        break;
    case 4:  // (0x00318f48) rides the path
        if (floater_ride(m, 0xc0, 0xac) != 0) {
            sw(pv + 0xc0, 0);
            sw(pv + 0xc4, 0);
            sb(m + 0x20, 5);
        }
        break;
    case 5:  // (0x00318f68) to the next stage's point
        if (glide(0xb6, false)) {
            sw(pv + 0xc0, 0);
            sw(pv + 0xc4, 0);
            sb(m + 0x20, 1);
            floater_next_stage(m, 0xb4);
        }
        break;
    case 6:  // (0x0031908c) gone until one of its slots is done
        if (slot_done(pv, 0xb8) || slot_done(pv, 0xbc)) {
            const uint32_t a = lwu(lwu(m + 0x24) + 0x10);
            const uint32_t f = lhu(m + 0x34) & 0xfffeu;
            sb(m + 0x20, 1);
            sw(m + 0x94, a);
            sh(m + 0x34, f);
            sb(m + 0x31, 1);
        }
        break;
    default:
        break;
    }
    floater_bob(m, 0xc8);
    const uint32_t st = lbu(m + 0x20);
    if (st - 3u < 3u) {  // (0x00319110) moving
        sh(0x0013f544, 2);
        sh(0x0013f542, 2);
        floater_lay_path(m, 0xdc);
        if (sub_0028d8c0(m, lh(pv + 0xb2)) == 0) sh(pv + 0xb2, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
    } else if (sub_0028d8c0(m, lh(pv + 0xb2)) != 0) {  // (0x00319158) stopped: its sound stopped
        const int32_t slot = lh(pv + 0xb2);
        if (slot != -1) {
            const uint32_t e = 0x0013e550 + static_cast<uint32_t>(slot * 0x70);
            if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
        }
        sh(pv + 0xb2, 0xffff);
    }
    vec3_sub(V(sp + 0x20), V(m + 0x10), V(sp));
    attach_frame_set(pv + 0x20, sp + 0x20, sp + 0x10, m + 0x40);
    ee_stack_free(0x80);
}

}  // namespace rac1
