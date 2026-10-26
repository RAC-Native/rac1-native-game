// Novalis's own moby classes, part 2f: the fighter jet (660), the bomber jet (688) and the stationary ship
// (666). Header novalis_mobys2.h. Addresses are Novalis's (`orig: L1:`);
// Novalis-only data is L1_ADDR, the globals the engine shares with Veldin are the level's own (OV), the base ELF's
// are bare.
#include <game/math/vecmath.h>
#include <game/ovl/hydro.h>
#include <game/ovl/arrival.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/weapons.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/port_stubs.h>

namespace rac1 {

// Class 660, a fighter jet: its size (+0x2c) and speed (pvar +0xfc) scale with its distance from the camera (the
// level's range L1:0x161af4 / 0x161af8 / 0x161afc: 1 near, the minimum beyond), and
//  - when it is hit (mask 0x800000) it explodes: the first hit starts the level's hit sound once (flag 0x13d408) and
//    the message 0x53d6; state 0x65; the explosion (0x273310 = explosion_effect), the debris (jet_debris_spawn) flying along its
//    velocity, its four wingmen (pvar +0x140..) and itself retired;
//  - otherwise the aircraft base (jet_update_base), the flags +0x34 |= 0x1000, pvar +0x2c = 100 / +0x2b = 1, and its
//    fade (+0x23) by its height: none from 175 up, 128 * (175 - z) / 50 between 125 and 175, 0x80 below.
// orig: L1:0x002f4428
void jet_fighter_update_l1(uint32_t m) {
    const uint32_t buf = ee_stack_alloc(0x40);
    const uint32_t hit = sub_0025a420(m, 0x800000, 0);
    const uint32_t pv = lwu(m + 0x78);
    sb(m + 0xa4, 0xff);
    if (lbu(m + 0x20) == 0) {
        const float f0 = lwc1(m + 0x2c), f1 = lwc1(pv + 0xfc);
        swc1(pv + 0x170, f0);
        swc1(pv + 0x174, f1);
    }
    const uint32_t s3 = m + 0x10;
    const float d = vec3_distance(V(s3), V(OV(0x00166dc0)));
    const float f5 = u2f(0x3f800000u);
    const float f3 = lwc1(L1_ADDR(0x00161afc)), f4 = lwc1(L1_ADDR(0x00161af4)), f2 = lwc1(L1_ADDR(0x00161af8));
    float f1 = f5 - f4;
    const float f2b = f2 - f3;
    f1 = fpu_div(f1, f2b);
    f1 = -f1;
    float f3b = f1 * f3;
    f1 = f1 * d;
    f3b = f5 - f3b;
    float f20 = f1 + f3b;
    if (f5 < f20) f20 = f5;
    if (f20 < f4) f20 = f4;
    const float s0 = lwc1(pv + 0x170) * f20;
    swc1(m + 0x2c, s0);
    const float s1v = lwc1(pv + 0x174) * f20;
    swc1(pv + 0xfc, s1v);
    if (hit != 0) {
        if (lbu(m + 0x20) == 0x65) {
            ee_stack_free(0x40);
            return;
        }
        if (lbu(0x0013d408) == 0) {
            sb(0x0013d408, 1);
            sound_play_indexed(1, 0, 0);
            sub_00263d40(0x53d6, -1);
        }
        vec3_sub(V(buf + 0x30), V(pv + 0xd0), V(pv + 0xe0));
        lq_sq(buf + 0x20, buf + 0x30);
        sb(m + 0x20, 0x65);
        sw(buf, 1);
        sw(buf + 8, 0xffffffffu);
        sw(buf + 0x10, 0);
        explosion_effect(m, buf + 0x20, s3, 0x14, 3, 4, 1, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 100000.0f, 3.0f, 15.0f);  // jal L1:0x273310 (Veldin 0x25e450): the stack words 1 / -1 / 0, f12..f18 as loaded before the call
        jet_debris_spawn_l1(m, buf + 0x20, f20);
        uint32_t s2 = pv + 0x140;
        for (int32_t k = 3; k >= 0; --k, s2 += 4) {
            const uint32_t a0 = lwu(s2);
            if (static_cast<int32_t>(a0) != -1) moby_retire(lwu(OV(0x0015ffd8)) + (a0 << 8));
        }
        moby_retire(m);
        ee_stack_free(0x40);
        return;
    }
    if (lbu(m + 0x20) == 0x65) {
        ee_stack_free(0x40);
        return;
    }
    jet_update_base_l1(m);
    sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
    sb(pv + 0x2c, 0x64);
    sb(pv + 0x2b, 1);
    const float z = lwc1(m + 0x18);
    const float lim = u2f(0x432f0000u);
    if (lim <= z) {
        sb(m + 0x23, 0);
    } else {
        const float f0 = u2f(0x42fa0000u);
        if (f0 < z && z < lim) {
            float f12 = lim - z;
            f12 = f12 * u2f(0x43000000u);
            f12 = fpu_div(f12, u2f(0x42480000u));
            sb(m + 0x23, static_cast<uint32_t>(float_to_int(f12)));
        } else if (lwc1(m + 0x18) <= f0) {
            sb(m + 0x23, 0x80);
        }
    }
    ee_stack_free(0x40);
}

// Class 666, the stationary ship (the carrier that launches the fighters): its pvar +0x170 is its flight path number
// (-1: none, it just idles its animation 5); the stage +0x174: 0 hides it (flags |= 0x41) and goes to 1; 1 waits for
// the hero in the box +0x170 and then goes to 2, unhides, scales the cargo's timer (the carried moby +0x180: pvar
// +0x2c = frame_scale(+0x188 * 60), its +0xbc = 1) and finds the path point whose segment length is 1.0 (the stop,
// +0x17c); 2 flies by the aircraft base, turns the carried moby (pvar +0x10 / +0x14 from its box, +0x18 its yaw
// towards the camera, limited by +0x190 degrees), starts its animation 2 at the stop (+0x178 = 1, timer +0x176) and
// retires the ship at the second last path point once nothing is left (+0x184); every stage then keeps the eight
// cargo mobys (+0x150) at the offsets of the table L1:0x20b360 rotated by the ship's matrix, facing the same way + pi,
// and when the timer ends in the stop (animation 2 over) releases the cargo one at a time (the first listed: class
// 0x1ab to state 4, 0x154 to state 2, 0x1cb the fighter: state 1 with +0x1e8 / +0x1ec and its velocity).
// orig: L1:0x002f4960
void jet_ship_update_l1(uint32_t m) {
    const uint32_t buf = ee_stack_alloc(0x80);
    const uint32_t pv = lwu(m + 0x78);
    auto done = [&] { ee_stack_free(0x80); };
    const int32_t a1 = lw(pv + 0x170);
    if (a1 == -1) {
        if (lbu(m + 0x53) != 5) moby_anim_set(m, 5, 0, 0);
        done();
        return;
    }
    const int32_t stage = lh(pv + 0x174);
    if (stage == 0) {
        sw(pv + 0x194, 0xffffffffu);
        sh(pv + 0x174, 1);
        sw(pv + 0x184, 0);
        sw(pv + 0x178, 0);
        sb(m + 0x30, 0xff);
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        done();
        return;
    } else if (stage == 1) {
        if (point_in_box(0x0013f3d0, a1) != 0) {
            sh(pv + 0x174, 2);
            sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
            if (lw(pv + 0x180) != -1) {
                const uint32_t cargo = lwu(OV(0x0015ffd8)) + (lwu(pv + 0x180) << 8);
                const uint32_t s0 = lwu(cargo + 0x78);
                const float f12 = lwc1(pv + 0x188) * u2f(0x42700000u);
                sw(s0 + 0x2c, static_cast<uint32_t>(frame_scale_i(float_to_int(f12))));
                sb(lwu(OV(0x0015ffd8)) + (lwu(pv + 0x180) << 8) + 0xbc, 1);
            }
            if (lw(pv + 0x74) != -1) {
                const uint32_t path = lwu(OV(0x001b04b0) + (lwu(pv + 0x74) << 2));
                const int32_t count = lw(path);
                if (count > 0) {
                    int32_t i = 0;
                    if (lwc1(path + 0x1c) == u2f(0x3f800000u)) {
                        sw(pv + 0x17c, 0);
                    } else {
                        for (;;) {
                            ++i;
                            if (!(i < lw(path))) break;
                            if (lwc1(path + (static_cast<uint32_t>(i) << 4) + 0x1c) == u2f(0x3f800000u)) {
                                sw(pv + 0x17c, static_cast<uint32_t>(i));
                                break;
                            }
                        }
                    }
                }
            }
        }
    } else if (stage == 2) {
        jet_update_base_l1(m);
        const int32_t v1 = lw(pv + 0x180);
        if (v1 != -1) {
            const uint32_t s0 = lwu(lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(v1) << 8) + 0x78);
            const float cx = lwc1(OV(0x00166dc0)), cy = lwc1(OV(0x00166dc4));
            float f20 = atan2_f(lwc1(m + 0x10) - cx, lwc1(m + 0x14) - cy);
            const uint32_t boxes = lwu(OV(0x001600ec));
            const float f1 = angle_sub(lwc1((lwu(s0 + 0x24) << 7) + boxes + 0x78), f20);
            if (f1 < 0.0f) {
                f20 = lwc1((lwu(s0 + 0x24) << 7) + boxes + 0x78);
            } else {
                float f13 = lwc1(pv + 0x190);
                const float f2 = u2f(0x3c8efa35u);
                if (f13 * f2 < f1) {
                    f13 = -f13;
                    f13 = f13 * f2;
                    f20 = angle_add(lwc1((lwu(s0 + 0x24) << 7) + boxes + 0x78), f13);
                }
            }
            const uint32_t box = (lwu(s0 + 0x24) << 7) + boxes;
            const float fdt1 = lwc1(0x0015ed70), fdt2 = lwc1(0x0015ed6c);
            swc1(s0 + 0x10, lwc1(box + 0x70));
            const float f14 = fdt1 * u2f(0x41490fdbu);
            const float f15 = fdt1 * u2f(0x42fb53d1u);
            swc1(s0 + 0x14, lwc1(box + 0x74));
            const float f16 = fdt2 * u2f(0x40490fdbu);
            swc1(s0 + 0x18, angle_seek_rate(pv + 0x18c, lwc1(OV(0x00166dd8)), f20, f14, f15, f16));
        }
        if (lw(pv + 0x74) != -1) {
            const uint32_t path = lwu(OV(0x001b04b0) + (lwu(pv + 0x74) << 2));
            const int32_t f_idx = lw(pv + 0x60);
            const int32_t stop = lw(pv + 0x17c);
            bool at_stop = false;
            if (f_idx == stop) {
                if (lw(pv + 0x178) == 0) {
                    if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, frame_scale_i(0x14));
                    sh(pv + 0x176, 0);
                    sw(pv + 0x178, 1);
                    at_stop = true;
                }
            }
            if (!at_stop && f_idx == lw(path) - 2 && lw(pv + 0x184) == 0) {
                moby_retire(m);
                done();
                return;
            }
        }
    }
    // ---- 0x2f4d28
    timer_tick_i16(ee_ref<int16_t>(pv + 0x176));
    if (lw(pv + 0x178) != 0 && lh(pv + 0x176) == 0 && lbu(m + 0x52) == 2) {
        // the cargo released, one at a time
        uint32_t s1 = pv + 0x150;
        int32_t k = 0;
        for (; k < 8; ++k, s1 += 4) {
            const int32_t id = lw(s1);
            if (id == -1) continue;
            const uint32_t base = lwu(OV(0x0015ffd8));
            const uint32_t mm = (static_cast<uint32_t>(id) << 8) + base;
            sh(mm + 0x34, lhu(mm + 0x34) & 0xfff9u);
            const uint32_t mm2 = base + (lwu(s1) << 8);
            vec_clear(V(mm2 + 0x40));
            const float ang = atan2_f(lwc1(0x0013f350 + 0x80) - lwc1(m + 0x10), lwc1(0x0013f350 + 0x84) - lwc1(m + 0x14));
            const uint32_t mm3 = (lwu(s1) << 8) + base;
            swc1(mm3 + 0x48, ang);
            const uint32_t mm4 = (lwu(s1) << 8) + base;
            const int32_t cls = lh(mm4 + 0xa6);
            if (cls == 0x1ab) {
                sb(mm4 + 0x20, 4);
            } else if (cls >= 0x1ac) {
                if (cls == 0x1cb) {
                    const uint32_t s0 = lwu(mm4 + 0x78);
                    sw(s0 + 0x1e8, 1);
                    sh(s0 + 0x1ec, static_cast<uint32_t>(frame_scale_i(0x1e)));
                    vec3_sub(V(s0 + 0x1d0), V(pv + 0xd0), V(m + 0x10));
                }
            } else if (cls == 0x154) {
                sb(mm4 + 0x20, 2);
            }
            sw(s1, 0xffffffffu);
            sh(pv + 0x176, static_cast<uint32_t>(rand_range(0x14, 0x1e)));
            done();
            return;
        }
        if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0x14));
        done();
        return;
    }
    // the cargo kept at its place
    for (int32_t a3 = 0; a3 < 8; ++a3) {
        for (uint32_t o = 0; o < 0x80; o += 0x10) lq_sq(buf + o, L1_ADDR(0x0020b360) + o);
        const uint32_t s1 = pv + 0x150 + (static_cast<uint32_t>(a3) << 2);
        const int32_t id = lw(s1);
        if (id == -1) continue;
        if (lh(pv + 0x174) != 1) {
            const uint32_t mm = (static_cast<uint32_t>(id) << 8) + lwu(OV(0x0015ffd8));
            sh(mm + 0x34, lhu(mm + 0x34) & 0xffbeu);
        }
        const uint32_t s0 = buf + (static_cast<uint32_t>(a3) << 4);
        mat3_transform(V(s0), V(s0), M(m + 0xc0));
        uint32_t mm = (lwu(s1) << 8) + lwu(OV(0x0015ffd8));
        vec3_add(V(mm + 0x10), V(m + 0x10), V(s0));
        mm = (lwu(s1) << 8) + lwu(OV(0x0015ffd8));
        lq_sq(mm + 0x40, m + 0x40);
        swc1(mm + 0x48, angle_add(lwc1(mm + 0x48), u2f(0x40490fdbu)));
        mm = (lwu(s1) << 8) + lwu(OV(0x0015ffd8));
        moby_anim_advance(mm);
        moby_update_transform(mm);
        sh(mm + 0x34, lhu(mm + 0x34) | 6u);
    }
    done();
}

// The bomber's shot (class 0x2ae): spawned at `pos` heading along `dir` (its yaw / pitch from it, its length at
// the pvar +0x38), `target` at pvar +0x10 (and the vector at +0), the owner `m` at +0x30, the flag `flag` at +0x34;
// two glow particles (spawn_particle_1a), the launch sound.
// orig: L1:0x002f68a0
uint32_t bomber_shot_spawn_l1(uint32_t m, uint32_t pos, uint32_t dir, uint32_t target, uint32_t flag) {
    const uint32_t buf = ee_stack_alloc(0x30);
    lq_sq(buf, pos);
    lq_sq(buf + 0x10, dir);
    lq_sq(buf + 0x20, target);
    const uint32_t s1 = moby_spawn(0x2ae);
    if (s1 == 0) {
        ee_stack_free(0x30);
        return s1;
    }
    const uint32_t s0 = lwu(s1 + 0x78);
    sw(s0 + 0x30, m);
    sw(s1 + 0x40, 0);
    const float f0 = lwc1(s1 + 0x2c) * u2f(0x3e800000u);
    sb(s1 + 0x30, 0xff);
    sh(s1 + 0x32, 0xff);
    sb(s1 + 0x31, 1);
    swc1(s1 + 0x2c, f0);
    const float len2 = vec2_length(V(buf + 0x10));
    const float ang = atan2_f(len2, lwc1(buf + 0x18));
    swc1(s1 + 0x44, -ang);
    swc1(s1 + 0x48, atan2_f(lwc1(buf + 0x10), lwc1(buf + 0x14)));
    lq_sq(s1 + 0x10, buf);
    lq_sq(s0, buf + 0x10);
    lq_sq(s0 + 0x10, buf + 0x20);
    sw(s0 + 0x34, flag);
    swc1(s0 + 0x38, vec3_length(V(buf + 0x10)));
    spawn_particle_1a(s1, 0x2f4f7f7fu, static_cast<uint32_t>(frame_scale_i(0x78)), 0xffffffffu, u2f(0x48cd1400u));
    spawn_particle_1a(s1, 0x4f6f7f7fu, static_cast<uint32_t>(frame_scale_i(0x78)), 0xffffffffu, u2f(0x484d1400u));
    moby_sound_start(0, 0, s1);
    moby_update_transform(s1);
    ee_stack_free(0x30);
    return s1;
}

// A level's bit set for the moby (its index +0xb2) in the collected table (0x14c190 + level * 0x100) and in the saved
// one (0x1ba950 / Veldin 0x1ba4d0).
// orig: (helper) repeated in 0x00308b28 / 0x002f7728
static void level_bit_set_l1(uint32_t m) {
    const uint32_t level = static_cast<uint32_t>(lw(0x0015ed84));
    uint32_t idx = lhu(m + 0xb2);
    uint32_t a = (static_cast<uint32_t>(static_cast<int32_t>(idx << 16) >> 21) << 2) + (level << 8) + 0x0014c190;
    sw(a, lwu(a) | (1u << (idx & 0x1f)));
    idx = lhu(m + 0xb2);
    a = (static_cast<uint32_t>(static_cast<int32_t>(idx << 16) >> 21) << 2) + OV(0x001ba4d0);
    sw(a, lwu(a) | (1u << (idx & 0x1f)));
}

// Class 688, the bomber jet (a boss of the jet level): the fighter's size / speed scaling by the distance (L1:0x161bd0 /
// bd4 / bd8) and the hit explosion (mask 0x800000: the debris, the wingmen and itself gone); state 0 doubles its
// speed, trims the closed path, and records the path points whose segment length is 1.0 (its stops, +0x180..); the
// aircraft base moves it; when the hero is in its trigger box (pvar +0x170) the first time it starts a scene (hero state
// 0x72, the scripted camera at the box +0x174, the timer +0x186 = frame_scale(0x1fe)); the timer's end records the kill
// (the level's bit) and removes it; at a stop (the follower's index equal to a marker) it picks a random clear box of
// the stop's list (+0x190) and fires a shot (bomber_shot_spawn) towards it, handing the shot the turrets (class 0x2bc,
// pvar +0x1d0) that can see the hero.
// orig: L1:0x002f7728
void jet_bomber_update_l1(uint32_t m) {
    const uint32_t buf = ee_stack_alloc(0x90);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10;
    const float f20 = u2f(0x3f800000u);
    const uint32_t fol = pv + 0x60;
    const uint32_t p50 = buf + 0x50, p40 = buf + 0x40, p60 = buf + 0x60;
    const float d = vec3_distance(V(pos), V(OV(0x00166dc0)));
    const float f3 = lwc1(L1_ADDR(0x00161bd8));
    float f1 = lwc1(L1_ADDR(0x00161bd0));
    float f2 = lwc1(L1_ADDR(0x00161bd4));
    f1 = f20 - f1;
    f2 = f2 - f3;
    f1 = fpu_div(f1, f2);
    f1 = -f1;
    float f3b = f1 * f3;
    f1 = f1 * d;
    f3b = f20 - f3b;
    const uint32_t hit = sub_0025a420(m, 0x800000, 0);
    float f21 = f1 + f3b;
    sb(m + 0xa4, 0xff);
    auto done = [&] { ee_stack_free(0x90); };
    if (hit != 0) {
        if (lbu(0x0013d408) == 0) {
            sb(0x0013d408, 1);
            sound_play_indexed(1, 0, 0);
            sub_00263d40(0x53d6, -1);
        }
        vec3_sub(V(buf + 0x30), V(pv + 0xd0), V(pv + 0xe0));
        lq_sq(buf + 0x20, buf + 0x30);
        sw(buf, 1);
        sw(buf + 8, 0xffffffffu);
        sw(buf + 0x10, 0);
        explosion_effect(m, buf + 0x20, pos, 0x14, 3, 4, 1, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 100000.0f, 3.0f, 15.0f);  // jal L1:0x273310 (Veldin 0x25e450): the stack words 1 / -1 / 0, f12..f18 as loaded before the call
        jet_debris_spawn_l1(m, buf + 0x20, f21 + f21);
        uint32_t s1 = pv + 0x140;
        for (int32_t s0 = 3; s0 >= 0; --s0, s1 += 4) {
            const uint32_t a0 = lwu(s1);
            if (static_cast<int32_t>(a0) != -1) moby_retire(lwu(OV(0x0015ffd8)) + (a0 << 8));
        }
        moby_retire(m);
        done();
        return;
    }
    for (;;) {  // 0x2f7990
        if (lbu(m + 0x20) == 0) {
            const int32_t pn = lw(fol + 0x14);
            if (pn != -1) {
                const uint32_t path = lwu(OV(0x001b04b0) + (static_cast<uint32_t>(pn) << 2));
                swc1(pv + 0xfc, lwc1(pv + 0xfc) + lwc1(pv + 0xfc));
                int32_t s2 = 0;
                const float dd = vec3_distance(V(path + 0x10), V(path + (static_cast<uint32_t>(lw(path)) << 4)));
                if (dd < u2f(0x3f000000u)) sw(path, static_cast<uint32_t>(lw(path) - 1));
                if (lw(path) > 0) {
                    int32_t s0 = 0;
                    if (lwc1(path + 0x1c) == f20) {
                        sb(pv + 0x180, 0);
                        s2 = 1;
                    }
                    for (;;) {
                        ++s0;
                        if (!(s0 < lw(path))) break;
                        if (lwc1(path + (static_cast<uint32_t>(s0) << 4) + 0x1c) == f20) {
                            sb(pv + 0x180 + static_cast<uint32_t>(s2), static_cast<uint32_t>(s0));
                            ++s2;
                            if (!(s2 < 4)) break;
                        }
                    }
                }
                sh(pv + 0x184, 0);
                sw(pv + 0x16c, 0xffffffffu);
                sh(pv + 0x186, 0xffffu);
                swc1(pv + 0x17c, lwc1(pv + 0xfc));
                swc1(pv + 0x178, lwc1(m + 0x2c));
            } else {
                debug_printf_stub();
            }
            sb(m + 0x30, 0xff);
            sh(m + 0x32, 0xff);
        }
        // the size / speed by the distance, the height fade
        {
            const float f0 = lwc1(L1_ADDR(0x00161bd0));
            if (f20 < f21) f21 = u2f(0x3f800000u);
            if (f21 < f0) f21 = f0;
            swc1(m + 0x2c, lwc1(pv + 0x178) * f21);
            swc1(pv + 0xfc, lwc1(pv + 0x17c) * f21);
            const float z = lwc1(m + 0x18);
            const float lim = u2f(0x432f0000u);
            if (lim <= z) {
                sb(m + 0x23, 0);
            } else if (u2f(0x42fa0000u) < z && z < lim) {
                float f12 = lim - z;
                f12 = f12 * u2f(0x43000000u);
                f12 = fpu_div(f12, u2f(0x42480000u));
                sb(m + 0x23, static_cast<uint32_t>(float_to_int(f12)));
            } else if (lwc1(m + 0x18) <= u2f(0x42fa0000u)) {
                sb(m + 0x23, 0x80);
            }
        }
        if (lw(pv + 0x170) == -1) {
            jet_update_base_l1(m);
        } else {
            if (lbu(m + 0x20) == 0) jet_update_base_l1(m);
            int32_t s0 = lh(pv + 0x186);
            bool countdown = (s0 != -1);
            if (!countdown) {
                if (point_in_box(0x0013f3d0, lw(pv + 0x170)) == 0) {
                    if (lh(pv + 0x186) == -1) {
                        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
                        done();
                        return;
                    }
                    countdown = true;
                } else {
                    const uint32_t idx16 = lhu(m + 0xb2);
                    const int32_t idx = static_cast<int16_t>(idx16);
                    bool collected = lbu(OV(0x001bb230) + static_cast<uint32_t>(idx) + 0x454) != 0;
                    if (!collected) {
                        const uint32_t level = static_cast<uint32_t>(lw(0x0015ed84));
                        collected = ((lwu(0x0014c190 + (level << 8) + (static_cast<uint32_t>(idx >> 5) << 2)) >> (idx16 & 0x1f)) & 1u) != 0;
                    }
                    if (collected) {
                        uint32_t s1 = pv + 0x1f0;
                        for (int32_t s2 = 3; s2 >= 0; --s2, s1 += 4) {
                            const int32_t v0 = lw(s1);
                            if (v0 == -1) continue;
                            const uint32_t v1 = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(v0) << 8);
                            if (v1 == 0) continue;
                            if (lh(v1 + 0xa6) != 0x1cb) continue;
                            const uint32_t st = lbu(v1 + 0x20);
                            if (st == 0xfe || st == 0xfd) continue;
                            const uint32_t s0p = lwu(v1 + 0x78);
                            sw(s0p + 0x1e8, 1);
                            sh(s0p + 0x1ec, static_cast<uint32_t>(frame_scale_i(2)));
                        }
                        moby_retire(m);
                        done();
                        return;
                    }
                    sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
                    if (lw(pv + 0x174) != -1) {
                        hero_set_state_l1(0x72, 0);
                        const uint32_t box = lwu(OV(0x001600ec)) + (lwu(pv + 0x174) << 7);
                        camera_scripted_set(box + 0x30, box + 0x70, 0, 0, 0);
                        sw(OV(0x0015f404), 1);
                        sw(m + 0x94, 0);
                        sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
                        sh(pv + 0x186, static_cast<uint32_t>(frame_scale_i(0x1fe)));
                    }
                }
            }
            if (countdown) {
                if (timer_tick_i16(ee_ref<int16_t>(pv + 0x186)) != 0) {
                    hydro_2eac18(0);  // jal L1:0x317070 (Veldin's 0x2eac18, the scripted camera's end)
                    hero_set_state_l1(0, 1);
                    level_bit_set_l1(m);
                    sw(OV(0x0015f404), 0);
                    moby_retire(m);
                    done();
                    return;
                }
                const int32_t t = lh(pv + 0x186);
                if (t == frame_scale_i(0x3c)) {
                    uint32_t s1 = pv + 0x1f0;
                    for (int32_t s2 = 3; s2 >= 0; --s2, s1 += 4) {
                        const int32_t v1 = lw(s1);
                        if (v1 == -1) continue;
                        const uint32_t mm = (static_cast<uint32_t>(v1) << 8) + lwu(OV(0x0015ffd8));
                        if (lh(mm + 0xa6) != 0x1cb) continue;
                        const uint32_t s0p = lwu(mm + 0x78);
                        sw(s0p + 0x1e8, 1);
                        sh(s0p + 0x1ec, static_cast<uint32_t>(frame_scale_i(2)));
                    }
                }
                jet_update_base_l1(m);
            }
        }
        // ---- 0x2f7de8: the stops
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        sb(pv + 0x2b, 1);
        sb(pv + 0x2c, 0x2f);
        const int32_t fidx = lw(pv + 0x60);
        if (lw(pv + 0x16c) != fidx) {
            int32_t s8 = -1;
            for (int32_t s0 = 0; s0 < 4; ++s0) {
                if (static_cast<uint32_t>(fidx) == lbu(pv + 0x180 + static_cast<uint32_t>(s0))) s8 = s0;
            }
            if (s8 != -1) {
                const uint32_t a0 = (static_cast<uint32_t>(s8) << 4) + pv + 0x190;
                lq_sq(buf + 0x20, L1_ADDR(0x00161be0));
                lq_sq(buf + 0x30, L1_ADDR(0x00161be0));
                int32_t s2 = 0, s4 = 0;
                uint32_t a1 = buf + 0x30, v1 = buf + 0x20, p = a0;
                for (int32_t s0 = 3; s0 >= 0; --s0, p += 4) {
                    if (lw(p) != -1) {
                        sw(v1, lwu(p));
                        ++s2;
                        v1 += 4;
                        ++s4;
                        sw(a1, lwu(p));
                        a1 += 4;
                    }
                }
                if (s2 == 0) {
                    done();
                    return;
                }
                if (point_in_box(OV(0x00166dc0), lw(pv + 0x188)) != 0) {
                    done();
                    return;
                }
                int32_t s0 = s2 - 1;
                if (s0 >= 0) {
                    uint32_t s1 = buf + 0x30 + (static_cast<uint32_t>(s0) << 2);
                    for (;;) {
                        const int32_t v0 = lw(s1 - 0x10);
                        const uint32_t box = lwu(OV(0x001600ec)) + (static_cast<uint32_t>(v0) << 7);
                        const int32_t r = segment_collide_v(V(OV(0x00166dc0)), V(box + 0x30), 0x86, m, nullptr);
                        if (r == 0) {
                            --s0;
                        } else {
                            const uint32_t who = lwu(OV(0x00173e58));
                            if (who == 0) {
                                sw(s1, 0xffffffffu);
                                --s4;
                                break;
                            }
                            if (who == m) {
                                --s0;
                            } else {
                                sw(s1, 0xffffffffu);
                                --s4;
                                break;
                            }
                        }
                        if (!(s0 >= 0)) break;
                        s1 -= 4;
                    }
                }
                if (s4 != 0) {
                    if (s2 > 0) {
                        int32_t k = s2;
                        uint32_t pa = buf + 0x30, pv2 = buf + 0x20;
                        do {
                            const int32_t v0 = lw(pa);
                            --k;
                            if (v0 != -1) {
                                sw(pv2, static_cast<uint32_t>(v0));
                                pv2 += 4;
                            }
                            pa += 4;
                        } while (k != 0);
                    }
                    s2 = s4;
                }
                if (s2 != 0) {
                    const int32_t pick = rand_mod(s2);
                    const uint32_t a1b = buf + (static_cast<uint32_t>(pick) << 2);
                    uint32_t v1b = lhu(pv + 0x184) ^ 1u;
                    uint32_t boxi = lwu(a1b + 0x20);
                    sh(pv + 0x184, v1b);
                    moby_chain_point_v(m, static_cast<uint32_t>(static_cast<int16_t>(v1b)), V(p50));
                    boxi <<= 7;
                    vec3_sub(V(p40), V(lwu(OV(0x001600ec)) + boxi + 0x30), V(p50));
                    vec3_set_length(V(p40), V(p40), lwc1(0x0015ed6c) * u2f(0x43200000u));
                    lq_sq(p60, lwu(OV(0x001600ec)) + boxi + 0x30);
                    const int32_t inbox = point_in_box(0x0013f3d0, lw(pv + 0x18c));
                    const uint32_t shot = bomber_shot_spawn_l1(m, p50, p40, p60, static_cast<uint32_t>(inbox));
                    if (shot != 0) {
                        int32_t s6 = 0;
                        uint32_t s4p = lwu(shot + 0x78) + 0x20;
                        sw(pv + 0x16c, lbu(pv + 0x180 + static_cast<uint32_t>(s8)));
                        uint32_t s2p = pv + 0x1d0;
                        for (int32_t s0b = 0; s0b < 8; ++s0b, s2p += 4) {
                            const int32_t vv = lw(s2p);
                            if (vv == -1) continue;
                            const uint32_t mm = (static_cast<uint32_t>(vv) << 8) + lwu(OV(0x0015ffd8));
                            if (lh(mm + 0xa6) != 0x2bc) continue;
                            const uint32_t s1p = lwu(mm + 0x78);
                            if (lh(s1p + 4) == 0 && lw(s1p) == 0) {
                                if (rand_mod(100) < 100) {
                                    if (point_in_box(0x0013f3d0, lw(s1p + 8)) != 0 && point_in_box(OV(0x00166dc0), lw(s1p + 8)) != 0) {
                                        ++s6;
                                        sw(s4p, lwu(s2p));
                                        s4p += 4;
                                    }
                                }
                            }
                            if (!(s6 < 4)) break;
                        }
                    }
                }
            }
        }
        // ---- 0x2f8164: the loop's end
        const int32_t s1 = lh(pv + 0x186);
        const int32_t s0 = frame_scale_i(0x1fe);
        const int32_t v0 = frame_scale_i(0x78);
        if (!((s0 - v0) < s1)) break;
    }
    done();
}

}  // namespace rac1
