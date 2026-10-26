// Ported overlay code: the Taunter (item 0x0e, hand moby class 0xaf). Its hand moby's update
// 0x2b6f10 plays the taunt (a sound per step of the item's sound table, the "taunt" animation) while
// the button is pressed or held and the hit sphere 0x2b6bc8 finds the enemies in front of the hero
// (classes 0x1f4 / 0x1f5 / 0x1f9 / 0x1ff within 12; the nearest one is set up as the taunted one
// through their pvars' +0x18, one of them picked at random is hit) and the mines in front of him
// (the glove's list 0x1b07b0: their pvars' +0x78 set, which triples their arming radius). The hand's
// pvars: +0 the sound step, +4 the sound slot, +8 the taunt's reach (30), +0xc the frames the
// button is held, +0x10 the next step's wait, +0x14 the enemy taken in this round, +0x18 the frames
// since the taunt started. 0x2b7398 keeps the expanding rings (records at 0x1db440, the draw
// callback 0x2b7568).
#include <game/math/vecmath.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350;
constexpr uint32_t PAD = 0x0013c940;
}  // namespace

// The taunt's sphere: of the targets list 0x1aba00 those with a pvars word (0x25c338) of the classes
// 0x1f4 / 0x1f5 / 0x1f9 / 0x1ff within 12 of the hand and within 55 degrees of the hero's facing:
// the `n`-th of them (n picked at random, pvars +0x14 counts them) is hit (0x259bc8, from the hero,
// pushed 1.5 along the line from the hand), its next-step wait (pvars +0x10) set; those within
// `range` (and the facing) whose taunter (+0x18 of their pvars word) is none or farther get the hand
// as their taunter. Then the mines of the list 0x1b07b0 within `range` and the facing get the hand in
// their pvars +0x78. Returns the count of taunters set.
// orig: 0x002b6bc8
int32_t taunter_hit(uint32_t m, float range) {
    int32_t s7 = 0;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t frame = ee_stack_alloc(0x30);
    const uint32_t sp = frame;
    V(sp) = V(m + 0x10);
    {
        const int32_t cnt = lw(pv + 0x14);
        const int32_t pick = rand_mod(0 < cnt ? cnt : 1);
        sw(sp + 0x20, static_cast<uint32_t>(pick));
    }
    sw(pv + 0x14, 0);
    const float cone = u2f(0x3f75be0bu);
    uint32_t list = OV(0x001aba00);
    uint32_t s0 = lwu(list);
    while (s0 != 0) {
        const uint32_t s2 = moby_pvar_word0(s0);
        if (s2 != 0) {
            const float f20 = vec2_distance(V(sp), V(s0 + 0x10));
            if (f20 < u2f(0x41400000u)) {
                const uint32_t cls = lhu(s0 + 0xa6);
                if ((cls - 0x1f4u) < 2u || static_cast<int16_t>(cls) == 0x1f9 || static_cast<int16_t>(cls) == 0x1ff) {
                    const float bearing = atan2_f(lwc1(s0 + 0x10) - lwc1(P + 0x80), lwc1(s0 + 0x14) - lwc1(P + 0x84));
                    if (angle_dist(lwc1(P + 0x98), bearing) < cone) {
                        if (lw(pv + 0x14) == lw(sp + 0x20)) {
                            vec3_sub(V(sp + 0x10), V(s0 + 0x10), V(m + 0x10));
                            vec3_set_length(V(sp + 0x10), V(sp + 0x10), u2f(0x3fc00000u));
                            sub_00259bc8(s0, m, 0x10000, P + 0x80, sp + 0x10, 1.0f);
                            const int32_t r = rand_range(5, 0x2d);
                            sw(pv + 0x10, static_cast<uint32_t>(frame_scale_i(r)));
                        }
                        sw(pv + 0x14, static_cast<uint32_t>(lw(pv + 0x14) + 1));
                    }
                }
            }
            if (f20 < range) {
                const float bearing = atan2_f(lwc1(s0 + 0x10) - lwc1(P + 0x80), lwc1(s0 + 0x14) - lwc1(P + 0x84));
                if (angle_dist(lwc1(P + 0x98), bearing) < cone) {
                    const uint32_t taunter = lwu(s2 + 0x18);
                    if (taunter == 0 || f20 < vec2_distance(V(taunter + 0x10), V(s0 + 0x10))) {
                        sw(s2 + 0x18, m);
                        ++s7;
                    }
                }
            }
        }
        list += 4;
        s0 = lwu(list);
    }
    const uint32_t mines = OV(0x001b07b0);
    if (lh(mines) > 0) {
        uint32_t s2p = mines + 2;
        for (int32_t s4 = 1;; ++s4, s2p += 2) {
            const uint32_t mo = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(static_cast<int32_t>(lh(s2p))) << 8);
            if (mo != 0 && static_cast<int8_t>(lbu(mo + 0x20)) >= 0) {
                const uint32_t mpv = lwu(mo + 0x78);
                if (vec2_distance(V(sp), V(mo + 0x10)) < range) {
                    const float bearing = atan2_f(lwc1(mo + 0x10) - lwc1(P + 0x80), lwc1(mo + 0x14) - lwc1(P + 0x84));
                    if (angle_dist(lwc1(P + 0x98), bearing) < cone) {
                        sw(mpv + 0x78, m);
                        ++s7;
                    }
                }
            }
            if (lh(mines) < s4 + 1) break;
        }
    }
    ee_stack_free(0x30);
    return s7;
}

// The expanding taunt rings: 10 records at 0x1db440 (0xc each: +0 the size, +4 the life timer,
// +6 / +8 the ring's colour index / active flag). A reset (0x1614cc) clears them and derives the
// step (0x1614d8) and its inverse (0x1614dc) from 0x1612b4 / 0x1612b8 and the frame scale; while the
// taunt runs (0x1614d4 clear) a ring is started when the wait 0x1614d0 is over (the first free
// record); the live ones grow by 0x1612b8 a frame scaled and are freed when their timer ends; the
// hand's chain point 0 into 0x1614e0, the draw callback 0x2b7568 registered.
// orig: 0x002b7398
void taunter_rings(uint32_t m) {
    const uint32_t ring = OV(0x001db440);
    if (lwu(OV(0x001614cc)) != 0) {
        for (uint32_t i = 0; i < 10; ++i) sh(ring + 8 + i * 0xc, 0);
        const float f0 = lwc1(0x0015ed6c);
        float f12 = lwc1(OV(0x001614b8));
        const float f1 = lwc1(OV(0x001614b4));
        f12 = f12 * f0;
        sw(OV(0x001614cc), 0);
        sw(OV(0x001614d0), 0);
        const int32_t v = float_to_int(fpu_div(f1, f12));
        sw(OV(0x001614d8), static_cast<uint32_t>(v));
        swc1(OV(0x001614dc), fpu_div(1.0f, int_to_float(v)));
    }
    if (lwu(OV(0x001614d4)) == 0) {
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(OV(0x001614d0)))) != 0) {
            uint32_t s0 = ring;
            for (;;) {
                if (lh(s0 + 8) != 0) {
                    s0 += 0xc;
                    if (s0 < ring + 0x78) continue;
                    break;
                }
                const int32_t v = frame_scale_i(lw(OV(0x001614b0)));
                const uint32_t a0 = lhu(OV(0x001614d8));
                const uint32_t v1 = lhu(OV(0x001614bc));
                sw(OV(0x001614d0), static_cast<uint32_t>(v));
                sh(s0 + 6, v1);
                sh(s0 + 8, 1);
                sh(s0 + 4, a0);
                sw(s0, 0);
                break;
            }
        }
    }
    for (uint32_t s0 = ring; s0 < ring + 0x78; s0 += 0xc) {
        if (lh(s0 + 8) == 0) continue;
        if (timer_tick_i16(ee_ref<int16_t>(s0 + 4)) != 0) {
            sh(s0 + 8, 0);
        } else {
            const float f0 = lwc1(OV(0x001614b8));
            const float f2 = lwc1(0x0015ed6c);
            swc1(s0, lwc1(s0) + f0 * f2);
        }
    }
    moby_chain_point(m, 0, OV(0x001614e0));
    sub_001f93e0(OVF(0x002b7568), m);
}

// The Taunter's hand frame (see the file's head). State 0 starts it (pvars +4 = -1, state 1), state
// 1 waits for the hand to be drawn (+0x70 bit 2) then state 2; in state 2 the taunt: its sound slot
// (+4, 0x28d8c0 tells whether it still plays) and the button (mask P+0x10a0 against the pressed /
// held words) start a step (the sound step +0 of the item's table, sounds of 10 times the table's
// two floats; the hit sphere 0x2b6bc8 each step), a held button keeps it going for 0x1e frames,
// let go (or the taunt over) the sound is freed and the animation back to 1; every fourth frame of
// the counter the sphere also runs while no step is playing. The rings 0x2b7398 run unless P+0x10b4
// is 3.
// orig: 0x002b6f10
void taunter_update(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    if (lwc1(pv + 8) == 0.0f) swc1(pv + 8, u2f(0x41f00000u));
    if (lbu(m + 0x53) == 2) sb(m + 0x20, 3);
    sw(OV(0x001614d4), 1);
    if (lbu(m + 0x20) == 0) {
        sw(pv + 4, 0xffffffffu);
        sb(m + 0x20, lbu(m + 0x20) + 1u);
        sw(OV(0x001614cc), 1);
    }
    const uint32_t st = lbu(m + 0x20);
    if (st == 1) {
        if ((lbu(m + 0x70) & 2) != 0) {
            sb(m + 0x20, st + 1u);
            sw(pv + 0xc, 0);
        }
    } else if (st == 2) {
        if (lbu(m + 0x53) == 3 && lbu(P + 0x20ac) == 0) {
            sw(OV(0x001614d4), 0);
            sw(pv + 0x18, lwu(pv + 0x18) + 1u);
        } else {
            sw(pv + 0x18, 0);
        }
        if (sub_0028d8c0(m, lw(pv + 4)) != 0) {
            // the step's sound is still playing
            sw(pv + 0xc, lwu(pv + 0xc) + 1u);
            uint32_t v0;
            bool release = false;
            if ((lwu(0x0013cae0) & lwu(P + 0x10a0)) != 0) {
                v0 = lbu(P + 0x20ac);
            } else if (frame_scale_i(0x1e) < lw(pv + 0x18)) {
                release = true;
                v0 = 0;
            } else {
                v0 = lbu(P + 0x20ac);
            }
            if (!release) {
                if (v0 == 0) {
                    if ((lwu(OV(0x0015f5cc)) & 3) == 0) (void)taunter_hit(m, lwc1(pv + 8));
                    goto L_735c;
                }
            }
            if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(5));
            {
                const int32_t slot = lw(pv + 4);
                if (slot != -1) {
                    const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot) * 0x70u;
                    if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
                }
                sw(pv + 0xc, 0);
                sw(pv + 4, 0xffffffffu);
            }
            goto L_735c;
        }
        {
            const uint32_t held_mask = lwu(P + 0x10a0);
            const uint32_t pressed = lwu(PAD + 0x1a4) & held_mask;
            bool fire = false;
            if (pressed != 0) {
                fire = lbu(P + 0x20ac) == 0;
            } else if ((lwu(PAD + 0x1a0) & held_mask) != 0 && !(lw(pv + 0xc) < 0x11)) {
                fire = lbu(P + 0x20ac) == 0;
            }
            if (fire) {
                if (pressed != 0) {
                    const int32_t r = rand_range(0xf, 0x2d);
                    sw(pv + 0x10, static_cast<uint32_t>(frame_scale_i(r)));
                }
                // the stats: the glove 0x141680 + 0x70 (count, longest, the bits of the items)
                do {
                    const uint32_t c = lhu(0x00141680 + 0x70);
                    if (!(0xfffe < c)) sh(0x00141680 + 0x70, c + 1);
                    const int32_t q = ee_div(frame_scale_i(lw(0x0015eea4)), 0x258);
                    if (static_cast<int32_t>(lhu(0x00141680 + 0x72)) < q) sh(0x00141680 + 0x72, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 0x258)));
                    sw(0x00141680 + 0x74, lwu(0x00141680 + 0x74) | (1u << (lwu(0x0015ed84) & 31)) | 0x80000000u);
                } while (false);
                if (lbu(m + 0x53) != 3) moby_anim_set(m, 3, 0, frame_scale_i(3));
                const int32_t snd = moby_sound_start(lw(pv), 1, m);
                int32_t idx = lw(pv);
                sw(pv + 4, static_cast<uint32_t>(snd));
                ++idx;
                sw(pv, static_cast<uint32_t>(idx));
                const uint32_t cls = lwu(m + 0x24);
                const uint32_t e = static_cast<uint32_t>(idx) << 5;
                const int32_t s3 = float_to_int(lwc1(lwu(cls + 0x28) + e) * 10.0f);
                const int32_t a1 = float_to_int(lwc1(lwu(lwu(m + 0x24) + 0x28) + e + 4) * 10.0f);
                if (!(idx < static_cast<int32_t>(lbu(lwu(m + 0x24) + 0xd)))) {
                    sw(pv, 0);
                } else if (s3 != 0x1e) {
                    sw(pv, 0);
                } else if (a1 != s3) {
                    sw(pv, 0);
                }
                (void)taunter_hit(m, lwc1(pv + 8));
                sw(pv + 0xc, 0);
                goto L_735c;
            }
            // not firing
            if (lw(pv + 4) >= 0) {
                if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(5));
                const int32_t slot = lw(pv + 4);
                if (slot != -1) {
                    const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot) * 0x70u;
                    if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
                }
                sw(pv + 0xc, 0);
                sw(pv + 4, 0xffffffffu);
                goto L_735c;
            }
            if ((lwu(PAD + 0x1a0) & lwu(P + 0x10a0)) == 0) sw(pv + 0xc, 0);
            else if (lbu(P + 0x20ac) != 0) sw(pv + 0xc, 0);
            else sw(pv + 0xc, lwu(pv + 0xc) + 1u);
            if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(5));
        }
    }
L_735c:
    if (lw(P + 0x10b4) != 3) taunter_rings(m);
}

// orig: (none) — the Taunter's update for the hand moby's indirect call.
void taunter_fns_register() {
    ovl_register_moby_fn(0x002b6f10, taunter_update);
}

}  // namespace rac1
