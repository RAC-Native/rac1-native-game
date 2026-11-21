// Umbris's own moby classes in the lava: the floating platform the Snagglebeast's stomps sink and raise again (class 1069) and
// the stepping stone that sinks under the hero (class 1080) — hand-ported from Umbris's own overlay. Header umbris_mobys.h,
// registered in umbris_mobys.cpp. Addresses are Umbris's (`orig: L7:`); Umbris-only data is L7_ADDR, the base ELF's are
// bare (the hero 0x13f350 (+0x80 his place, +0xf0 his push, +0x2fc the moby he stands on, +0x30e in the air), the level's
// number 0x15ed84, the levels' saved flag bytes 0x14c050, the frame time 0x15ed6c, its square 0x15ed70, the speed 0x15ed60).
//
// Class 1069's pvars: its attach frame +0x60, the moby it reports to (+0xa0, an index: the Snagglebeast), its rest angles
// +0xa4 / +0xa8, a countdown +0xac, its rise speed +0xb0 and its pull +0xb4, the lava's height +0xb8, whether it comes back
// (+0xbc) and tells the Snagglebeast it sank (+0xbe: the value +0xbf for the Snagglebeast's +0x19d, and its state 12), the
// count +0xbd its sinking waits, its bob angles +0xc0 / +0xc4, its splash countdown +0xc8 and its last height +0xcc. Its
// +0xbc (the moby's) is a request: 1 shake, 2 sink.
// Class 1080's: its attach frame +0x60, its shake countdown +0xa0 (a halfword) with the sound +0xa2, its fall speed +0xa4,
// its angles at the hero's step +0xa8 / +0xac.
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_small.h>
#include <game/ovl/umbris_mobys.h>
#include <game/ovl/variants_l7.h>

namespace rac1 {

namespace {
constexpr float DEG = 0.01745329238474369f;  // (0x3c8efa35: one degree)

// the shake about the rest angles: `t` the countdown, the angles `base` + a degree's sin / cos of its two rates
void shake(uint32_t m, int32_t t, float base_x, float base_y, bool add_base) {
    const float sx = fpu_mul(sin_vu(angle_wrap_mod(fpu_mul(int_to_float(t), 0.75f))), DEG);
    swc1(m + 0x40, add_base ? angle_add(base_x, sx) : sx);
    const float cy = fpu_mul(cos_vu(angle_wrap_mod(fpu_mul(int_to_float(t), u2f(0x3f63d70au)))), DEG);  // (0.89)
    swc1(m + 0x44, add_base ? angle_add(base_y, cy) : cy);
}

// the tilt toward 80 degrees a sinking platform takes: the angle +0x40 a twentieth of the way there; the step
float tilt_down(uint32_t m) {
    const float a = lwc1(m + 0x40);
    const float step = fpu_mul(fpu_sub(u2f(0x3fb2b8c2u), a), u2f(0x3d4ccccdu));  // (80 degrees, 0.05)
    swc1(m + 0x40, fpu_add(a, step));
    return step;
}

// the angle at `at` clamped to +-30 degrees
void clamp30(uint32_t at) {
    const float hi = u2f(0x3f060a92u), lo = u2f(0xbf060a92u);
    if (hi < lwc1(at)) swc1(at, hi);
    if (lwc1(at) < lo) swc1(at, lo);
}
}  // namespace

// Class 1069, the floating platform in the Snagglebeast's lava. 0: set up (turned half round one time in two; already sunk
// for good when its saved flag of the level is set). 1: rests; shakes while asked to (+0xbc 1), its group told to shake.
// A request +0xbc 2 starts 2: it shakes for its count (+0xbd) frames, then tells the Snagglebeast (+0xbe) and sinks (3):
// tilting, dropping; under the lava (15 below its height) it is gone, or (+0xbc) waits in 4 until the Snagglebeast is out of
// its attacks (+0xef below 2, or its state below 12), and comes up in 5 at a random speed. 5: it floats: pulled up to the
// lava's height, held down a moment when the hero lands on it (a sound; the hero carried and tilted with it), bobbing, its
// angles kept within 30 degrees; a splash sound at a random interval as it crosses the surface. It carries what stands
// on it by its attach frame.
// orig: L7:0x003112c8
void umb_class1069_update_l7(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10;
    Vec4 moved;
    vec3_scale(moved, V(pos), -1.0f);
    const Vec4 old_ang = V(m + 0x40);
    uint32_t boss = 0, boss_pv = 0;
    if (lw(pv + 0xa0) != -1) {
        boss = lwu(L7_ADDR(0x0015ffd8)) + (lwu(pv + 0xa0) << 8);
        boss_pv = lwu(boss + 0x78);
    }
    if (lbu(m + 0xbc) == 2) {  // asked to sink
        sb(m + 0x20, 2);
        sb(m + 0xbc, 3);
        sw(pv + 0xac, lbu(pv + 0xbd));
    }
    switch (lbu(m + 0x20)) {
        case 0: {  // (0x003113ac)
            sw(pv + 0x20, 0);
            sh(pv + 0x24, 0);
            sb(pv + 0x28, 4);
            sh(pv + 0x3e, 0xd);
            if ((libc_rand() & 1) != 0) swc1(m + 0x48, angle_add(lwc1(m + 0x48), u2f(0x40490fdbu)));  // (pi)
            swc1(pv + 0xa4, lwc1(m + 0x40));
            sw(pv + 0xc8, 0xffffffffu);
            swc1(pv + 0xa8, lwc1(m + 0x44));
            sb(m + 0x20, 1);
            const uint32_t flag = lbu(m + 0xb0);
            sb(m + 0xbc, 0);
            if (flag == 0xff) break;
            if (lbu(0x0014c050 + (lwu(0x0015ed84) << 4) + flag) != 0xff) break;
            swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 5.0f));  // sunk for good
            if (lbu(pv + 0xbc) == 0) {
                moby_retire(m);
                return;
            }
            sb(m + 0x20, 5);
            break;
        }
        case 1: {  // (0x00311470)
            if (lbu(m + 0xbc) != 1) break;
            if (lw(pv + 0xac) == 0) sw(pv + 0xac, static_cast<uint32_t>(ee_rem(libc_rand(), 10)));
            if (lbu(pv + 0xbe) != 0) moby_group_set_bc_l1(lbu(m + 0x21), 2);
            shake(m, lw(pv + 0xac), lwc1(pv + 0xa4), lwc1(pv + 0xa8), true);
            break;
        }
        case 2: {  // (0x00311538)
            shake(m, lw(pv + 0xac), lwc1(pv + 0xa4), lwc1(pv + 0xa8), true);
            if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xac))) == 0) break;
            if (lbu(pv + 0xbe) != 0) {  // the Snagglebeast told
                sb(boss_pv + 0x19d, lbu(pv + 0xbf));
                sb(boss + 0x20, 0xc);
            }
            sb(m + 0x20, 3);
            break;
        }
        case 3: {  // (0x003115dc) sinking
            const float step = tilt_down(m);
            swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), fpu_mul(sin_vu(step), u2f(0x40466666u))));  // (3.1)
            const float rise = fpu_sub(lwc1(pv + 0xb0), fpu_mul(lwc1(pv + 0xb4), lwc1(0x0015ed70)));
            swc1(pv + 0xb0, rise);
            const float z = fpu_add(lwc1(m + 0x18), rise);
            swc1(m + 0x18, z);
            if (!(z < fpu_sub(lwc1(pv + 0xb8), 15.0f))) break;
            if (lbu(pv + 0xbc) == 0) {
                moby_retire(m);
                return;
            }
            sw(pv + 0xb0, 0);
            sb(m + 0x20, 4);
            break;
        }
        case 4: {  // (0x00311698) under the lava, waiting for the Snagglebeast
            const float dt = lwc1(0x0015ed6c);
            if (lbu(boss_pv + 0xef) < 2 && !(lbu(boss + 0x20) < 12)) break;
            swc1(pv + 0xb0, rand_range_f(dt, fpu_mul(dt, 4.0f)));
            swc1(pv + 0xc0, rand_angle());
            swc1(pv + 0xc4, rand_angle());
            sb(m + 0x20, 5);
            sw(m + 0x40, 0);
            sw(m + 0x44, 0);
            sw(pv + 0xac, 0);
            break;
        }
        case 5: {  // (0x003116f4) floating
            const float next_z = fpu_add(lwc1(m + 0x18), lwc1(pv + 0xb0));
            const float surface = lwc1(pv + 0xb8);
            if (lh(0x0013f65e) == 0 && lwu(0x0013f64c) == m) {  // the hero landed on it: held down 90 frames at least
                if (lw(pv + 0xac) == 0) moby_sound_start(0, 0, m);
                if (lw(pv + 0xac) < frame_scale_i(0x5a)) sw(pv + 0xac, static_cast<uint32_t>(frame_scale_i(0x5a)));
            }
            if (lw(pv + 0xac) != 0) {
                if (fpu_sub(lwc1(pv + 0xb8), u2f(0x40266666u)) < lwc1(m + 0x18))  // (2.6 below the surface at most)
                    swc1(pv + 0xb0, -fpu_mul(lwc1(0x0015ed6c), 0.25f));
                else
                    sw(pv + 0xb0, 0);
            } else {
                if (next_z < surface) {  // pulled up toward the surface
                    float pull = fpu_mul(fpu_sub(surface, next_z), u2f(0x3e4ccccdu));  // (0.2)
                    const float cap = u2f(0x3f666666u);  // (0.9)
                    if (cap < pull) pull = cap;
                    else if (pull < u2f(0xbf666666u)) pull = u2f(0xbf666666u);
                    swc1(pv + 0xb0, fpu_add(lwc1(pv + 0xb0), pull));
                }
                const float v = fpu_sub(lwc1(pv + 0xb0), fpu_mul(lwc1(0x0015ed70), u2f(0x411ccccdu)));  // (gravity 9.8)
                const float vmax = fpu_mul(lwc1(0x0015ed6c), 5.0f);
                swc1(pv + 0xb0, v);
                if (vmax < v) swc1(pv + 0xb0, vmax);
            }
            swc1(m + 0x18, fpu_add(lwc1(m + 0x18), lwc1(pv + 0xb0)));
            // the bob: two angles going round (the second taken from the first's new value, as the original does)
            const float a = angle_add(lwc1(pv + 0xc0), u2f(0x3c23d70au));  // (0.01)
            swc1(pv + 0xc0, a);
            swc1(pv + 0xc4, angle_add(a, u2f(0x3c8b4396u)));  // (0.017)
            float goal_x = fpu_mul(sin_vu(lwc1(pv + 0xc0)), u2f(0x3ca3d70au));  // (0.02)
            const float goal_y = fpu_mul(sin_vu(lwc1(pv + 0xc4)), u2f(0x3ca3d70au));
            if (lw(pv + 0xac) != 0) {  // held down: tilted toward the hero, who is carried round with it
                timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xac)));
                Vec4 to_hero;
                vec3_sub(to_hero, V(0x0013f3d0), V(pos));
                const float along = vec3_dot(to_hero, V(m + 0xc0));
                const float across = vec3_dot(to_hero, V(m + 0xd0));
                goal_x = angle_add(angle_add(goal_x, fpu_mul(across, u2f(0xbcf5c28fu))), fpu_mul(along, u2f(0x3cf5c28fu)));  // (-0.03, 0.03)
                const float z0 = lwc1(m + 0x18);
                // (point_turned takes its angles and writes its point by address: the original's stack)
                const uint32_t sp = ee_stack_alloc(0x20);
                V(sp) = V(m + 0x40);
                point_turned(0x0013f3d0, sp + 0x10, pos, sp, m + 0x40);
                vec3_sub(V(0x0013f440), V(sp + 0x10), V(0x0013f3d0));
                ee_stack_free(0x20);
                swc1(0x0013f448, fpu_add(lwc1(0x0013f448), fpu_sub(lwc1(m + 0x18), z0)));
            }
            const float rate = fpu_mul(lwc1(0x0015ed6c), u2f(0x3e32b8c2u));  // (10 degrees a second)
            turn_towards(m + 0x40, 0, goal_x, rate);
            turn_towards(m + 0x44, 0, goal_y, fpu_mul(lwc1(0x0015ed6c), u2f(0x3e32b8c2u)));
            clamp30(m + 0x40);
            clamp30(m + 0x44);
            break;
        }
        default:
            break;
    }
    if (lbu(m + 0x20) == 5 && timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xc8))) != 0) {
        // the splash: as it crosses the lava's height, then again after 20 .. 40 frames
        const float last = lwc1(pv + 0xcc), surface = lwc1(pv + 0xb8), z = lwc1(m + 0x18);
        if ((surface <= last && z < surface) || (last <= surface && surface < z)) {
            moby_sound_start(0, 0, m);
            const int32_t r = libc_rand();
            const int32_t span = frame_scale_i(0x14);
            sw(pv + 0xc8, static_cast<uint32_t>(ee_rem(r, span) + frame_scale_i(0x14)));
        }
    }
    swc1(pv + 0xcc, lwc1(m + 0x18));
    vec3_add(moved, moved, V(pos));
    attach_frame_set_v(pv + 0x60, moved, old_ang, V(m + 0x40));
}

// Class 1080, the stepping stone in the lava. 0: set up (its colour, its angles and height a little off at random). 1: the
// hero on it starts its shake (30 frames, a sound). 2: it shakes, sinking slowly; then 3: it tilts and drops, and is gone
// under the lava (its height below 2). It carries what stands on it by its attach frame.
// orig: L7:0x00311bc8
void umb_class1080_update_l7(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    const uint32_t pos = m + 0x10;
    const Vec4 old = V(pos);
    switch (lbu(m + 0x20)) {
        case 0: {  // (0x00311c30)
            sw(pv + 0x20, 0);
            sh(pv + 0x24, 0);
            sb(pv + 0x28, 4);
            sh(pv + 0x3e, 0xd);
            moby_set_color_hi(m, 0x50, 0x40, 0x10);
            const float lo = u2f(0xbd0efa35u), hi = u2f(0x3d0efa35u);  // (+-2 degrees)
            swc1(m + 0x40, fpu_add(lwc1(m + 0x40), rand_range_f(lo, hi)));
            swc1(m + 0x44, fpu_add(lwc1(m + 0x44), rand_range_f(lo, hi)));
            const float dz = rand_range_f(u2f(0xbca3d70au), u2f(0x3ca3d70au));  // (+-0.02)
            sb(m + 0x20, 1);
            swc1(m + 0x18, fpu_add(lwc1(m + 0x18), dz));
            break;
        }
        case 1: {  // (0x00311cd8)
            if (moby_hero_on_l1(m) == 0) break;
            swc1(pv + 0xa8, lwc1(m + 0x40));
            swc1(pv + 0xac, lwc1(m + 0x44));
            const int32_t t = frame_scale_i(0x1e);
            const int16_t snd = static_cast<int16_t>(lh(pv + 0xa2));
            sh(pv + 0xa0, static_cast<uint32_t>(t));
            if (snd != -1) moby_sound_start(snd, 0, m);
            sb(m + 0x20, 2);
            break;
        }
        case 2: {  // (0x00311d24)
            shake(m, lh(pv + 0xa0), 0.0f, 0.0f, false);
            const float sink = u2f(0x3b03126fu);  // (0.002)
            swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), fpu_mul(lwc1(0x0015ed60), sink)));
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0xa0))) == 0) break;
            swc1(pv + 0xa4, fpu_mul(lwc1(0x0015ed60), sink));
            sb(m + 0x20, 3);
            break;
        }
        case 3: {  // (0x00311dcc)
            const float step = tilt_down(m);
            const float s = sin_vu(step);
            const float z = fpu_sub(lwc1(m + 0x18), fpu_add(s, s));
            const float g = fpu_mul(lwc1(0x0015ed70), u2f(0x411ccccdu));  // (9.8)
            swc1(m + 0x18, z);
            const float fall = fpu_add(lwc1(pv + 0xa4), g);
            swc1(pv + 0xa4, fall);
            const float nz = fpu_sub(lwc1(m + 0x18), fall);
            swc1(m + 0x18, nz);
            if (nz < 2.0f) {
                moby_retire(m);
                return;
            }
            break;
        }
        default:  // (4: 0x00311e60)
            break;
    }
    Vec4 moved;
    vec3_sub(moved, V(pos), old);
    attach_frame_set_v(pv + 0x60, moved, V(m + 0x40), V(m + 0x40));
}

}  // namespace rac1
