// Umbris's own moby classes: the Snagglebeast arena pillars (classes 1113..1116, shattered into rocks when struck by the
// Snagglebeast's charge beam), the driven sliding gate / bridge (class 1128), the driven hinged bridge (class 1133) and
// the breakable plant prop (class 1686) — hand-ported from Umbris's own overlay. Header umbris_mobys.h, registered in
// umbris_mobys.cpp. Addresses are Umbris's (`orig: L7:`); Umbris-only data is L7_ADDR, the base ELF's are bare (the level's
// number 0x15ed84, the levels' saved flag bytes 0x14c050, the frame time 0x15ed6c).
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_rock.h>
#include <game/ovl/umbris_mobys.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
// the state of driver moby `idx` (-1: none; null, retired 0xfe or dying 0xfd: `gone` set)
uint32_t driver_state_at(int32_t idx, bool& gone) {
    gone = true;
    if (idx == -1) return 0;
    const uint32_t d = lwu(L7_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8);
    if (d == 0) return 0;
    const uint32_t st = lbu(d + 0x20);
    if (st == 0xfe || st == 0xfd) return 0;
    gone = false;
    return st;
}

void set_level_switch(int32_t sw_idx, uint32_t val) {
    if (sw_idx != -1) {
        sw(L7_ADDR(0x00184528) + (static_cast<uint32_t>(sw_idx) << 2), val);
    }
}
}  // namespace

// Classes 1113..1116, the four pillars in the Snagglebeast's arena. Ignores hits unless dealt by class 0x415 in state 8
// or the Snagglebeast (0x452) in state 9 with the charge beam's signature damage (0x3f800408); then scatters +0x0 rocks
// (classes 0x2b8..0x2ba) across its rotated box (+0x4..+0xc), bursts and retires.
// orig: L7:0x00319f48
void umb_class1113_update_l7(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t hit = sub_0025a420(m, 0x30000u, 0);
    if (hit == 0) {
        sb(m + 0xa4, 0xff);
        return;
    }
    const uint32_t by = lwu(hit + 0x20);
    if (by == 0) {
        sb(m + 0xa4, 0xff);
        return;
    }
    const int32_t by_cls = lh(by + 0xa6);
    const uint32_t by_st = lbu(by + 0x20);
    const bool valid_src = (by_cls == 0x415 && by_st == 8) || (by_cls == 0x452 && by_st == 9);
    if (!valid_src || !(lwc1(hit + 0x2c) == u2f(0x3f800408u))) {  // (1.000123: the charge beam's marker)
        sb(m + 0xa4, 0xff);
        return;
    }

    const uint32_t sp = ee_stack_alloc(0x30);
    const uint32_t ext = sp + 0x0, rpos = sp + 0x10, rvel = sp + 0x20;
    swc1(ext + 0x0, lwc1(pv + 0x4));
    swc1(ext + 0x4, lwc1(pv + 0x8));
    swc1(ext + 0x8, lwc1(pv + 0xc));
    mat3_transform(V(ext), V(ext), M(m + 0xc0));

    for (int32_t i = 0; i < lw(pv + 0x0); ++i) {
        const float ex = lwc1(ext + 0x0);
        swc1(rpos + 0x0, fpu_mul(rand_range_f(-ex, ex), 0.5f));
        const float ey = lwc1(ext + 0x4);
        swc1(rpos + 0x4, fpu_mul(rand_range_f(-ey, ey), 0.5f));
        swc1(rpos + 0x8, rand_range_f(1.5f, lwc1(ext + 0x8)));
        vec3_add(V(rpos), V(rpos), V(m + 0x10));

        const float dt0 = lwc1(0x0015ed6c);
        rand_sphere_point(rvel, fpu_mul(dt0, 5.0f), fpu_mul(dt0, 10.0f));
        if (lwc1(rvel + 0x8) < 0.0f) {
            swc1(rvel + 0x8, -lwc1(rvel + 0x8));
        }
        const float dt1 = lwc1(0x0015ed6c);
        const float min_vz = fpu_add(dt1, dt1);
        if (lwc1(rvel + 0x8) < min_vz) {
            swc1(rvel + 0x8, fpu_add(lwc1(rvel + 0x8), min_vz));
        }

        const uint32_t rock_cls = static_cast<uint32_t>(ee_rem(i, 3)) + 0x2b8u;
        const float scale = rand_range_f(u2f(0x3ed1eb85u), 0.5f);  // (0.41 .. 0.5)
        const int32_t life = rand_range(0x32, 0x50);
        rock_spawn(rpos, rvel, rock_cls, life, 0, scale, 1.0f, 1.0f, 0.75f);
    }

    explosion_effect(m, hit + 0x10, hit, 10, 3, 0x10, 0, 1, 1, -1, 0, 3.0f, 1.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
    moby_retire(m);
    ee_stack_free(0x30);
}

// Class 1128, the sliding gate / bridge driven by moby +0x10 (opening at state +0x14, closing at +0x18) or by the live
// level flag 0x15fc88[+0xb0] == 0xff (+0x3c == 0 holds it closed, != 0 forces it open). Moves along its rotated offset
// +0x1c..+0x24 at +0x28 opening and +0x2c closing, updating level switch +0x38 in 0x184528.
// orig: L7:0x0031a9e8
void umb_class1128_update_l7(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0xb0) != 0xff && lbu(0x0014c050 + (lwu(0x0015ed84) << 4) + lbu(m + 0xb0)) == 0xff) {
        sw(pv + 0x10, 0xffffffffu);
    }
    bool hold_closed = false, force_open = false;
    if (lbu(L7_ADDR(0x0015fc88) + lbu(m + 0xb0)) == 0xff) {
        if (lw(pv + 0x3c) == 0) {
            hold_closed = true;
        } else {
            force_open = true;
        }
    }

    switch (lbu(m + 0x20)) {  // (jump table L7:0x00211a10)
        case 0: {  // (0x0031aaa4)
            V(pv + 0x0) = V(m + 0x10);
            set_level_switch(lw(pv + 0x38), 0);
            sb(m + 0x20, 1);
            sw(pv + 0xc, 0);
            return;
        }
        case 1: {  // (0x0031aae0)
            if (hold_closed) return;
            if (!force_open) {
                bool gone = false;
                const uint32_t ds = driver_state_at(lw(pv + 0x10), gone);
                if (!gone && ds != lwu(pv + 0x14)) return;
            }
            Vec4 off{lwc1(pv + 0x1c), lwc1(pv + 0x20), lwc1(pv + 0x24), 0.0f};
            const float len = vec3_length(off);
            const int32_t snd = lw(pv + 0x30);
            swc1(pv + 0xc, len);
            if (snd != -1) moby_sound_start(snd, 0, m);
            sb(m + 0x20, 2);
            return;
        }
        case 2: {  // (0x0031ab84)
            bool to_close = hold_closed;
            if (!to_close && !force_open) {
                bool gone = false;
                const uint32_t ds = driver_state_at(lw(pv + 0x10), gone);
                if (!gone && ds == lwu(pv + 0x18)) to_close = true;
            }
            if (to_close) {
                const int32_t snd = lw(pv + 0x30);
                if (snd != -1) moby_sound_start(snd, 0, m);
                sb(m + 0x20, 4);
                return;
            }
            Vec4 off{lwc1(pv + 0x1c), lwc1(pv + 0x20), lwc1(pv + 0x24), 0.0f};
            mat3_transform(off, off, M(m + 0xc0));
            Vec4 step{};
            vec3_set_length(step, off, fpu_mul(lwc1(pv + 0x28), lwc1(0x0015ed6c)));
            vec3_add(V(m + 0x10), V(m + 0x10), step);
            const float rem = fpu_sub(lwc1(pv + 0xc), fpu_mul(lwc1(pv + 0x28), lwc1(0x0015ed6c)));
            swc1(pv + 0xc, rem);
            if (!(rem < 0.0f)) return;
            vec3_add(V(m + 0x10), V(pv + 0x0), off);
            swc1(pv + 0xc, 0.0f);
            set_level_switch(lw(pv + 0x38), 1);
            sb(m + 0x20, 3);
            if (lw(pv + 0x34) != -1) {
                moby_sound_start(lw(pv + 0x30), 0, m);
            }
            return;
        }
        case 3: {  // (0x0031acf0)
            if (!hold_closed) {
                if (force_open) return;
                bool gone = false;
                const uint32_t ds = driver_state_at(lw(pv + 0x10), gone);
                if (gone || ds != lwu(pv + 0x18)) return;
            }
            const int32_t snd = lw(pv + 0x30);
            sw(pv + 0xc, 0);
            if (snd != -1) moby_sound_start(snd, 0, m);
            sb(m + 0x20, 4);
            return;
        }
        case 4: {  // (0x0031ad64)
            if (!hold_closed) {
                bool to_open = force_open;
                if (!to_open) {
                    bool gone = false;
                    const uint32_t ds = driver_state_at(lw(pv + 0x10), gone);
                    if (gone || ds == lwu(pv + 0x14)) to_open = true;
                }
                if (to_open) {
                    const int32_t snd = lw(pv + 0x30);
                    if (snd != -1) moby_sound_start(snd, 0, m);
                    sb(m + 0x20, 2);
                    return;
                }
            }
            Vec4 off{lwc1(pv + 0x1c), lwc1(pv + 0x20), lwc1(pv + 0x24), 0.0f};
            const float total = vec3_length(off);
            vec3_set_length(off, off, -fpu_mul(lwc1(pv + 0x2c), lwc1(0x0015ed6c)));
            Vec4 step{};
            mat3_transform(step, off, M(m + 0xc0));
            vec3_add(V(m + 0x10), V(m + 0x10), step);
            const float moved = fpu_add(lwc1(pv + 0xc), fpu_mul(lwc1(pv + 0x2c), lwc1(0x0015ed6c)));
            swc1(pv + 0xc, moved);
            if (!(total < moved)) return;
            V(m + 0x10) = V(pv + 0x0);
            if (lw(pv + 0x30) != -1) {
                moby_sound_start(lw(pv + 0x34), 0, m);
            }
            set_level_switch(lw(pv + 0x38), 0);
            sb(m + 0x20, 1);
            return;
        }
        default:  // (5 and >= 6: 0x0031aebc)
            return;
    }
}

// Class 1133, the hinged bridge driven by moby +0x0 (+0x4 drops it into a damped swing in state 3; +0x8 raises it back to
// pitch 0 in state 4).
// orig: L7:0x0031b3f0
void umb_class1133_update_l7(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;

    switch (lbu(m + 0x20)) {  // (jump table L7:0x00211a50)
        case 0: {  // (0x0031b438)
            sw(m + 0x44, 0);
            sb(m + 0x20, 1);
            sw(pv + 0x18, 0);
            return;
        }
        case 1: {  // (0x0031b44c)
            bool gone = false;
            const uint32_t ds = driver_state_at(lw(pv + 0x0), gone);
            if (!gone && ds != lwu(pv + 0x4)) return;
            const float rate = fpu_mul(lwc1(pv + 0xc), lwc1(0x0015ed6c));
            swc1(pv + 0x18, rate);
            swc1(m + 0x44, rate);
            moby_sound_start(0, 0, m);
            sb(m + 0x20, 3);
            return;
        }
        case 3: {  // (0x0031b4c0)
            const float c = cos_vu(lwc1(m + 0x44));
            const float dv = fpu_mul(c, fpu_mul(lwc1(pv + 0xc), lwc1(0x0015ed6c)));
            const float rate = angle_add(lwc1(pv + 0x18), dv);
            swc1(pv + 0x18, rate);
            swc1(m + 0x44, angle_add(lwc1(m + 0x44), rate));
            const float damped = fpu_mul(lwc1(pv + 0x18), lwc1(pv + 0x10));
            const uint32_t d = lwu(L7_ADDR(0x0015ffd8)) + (lwu(pv + 0x0) << 8);
            swc1(pv + 0x18, damped);
            if (d == 0) return;
            const uint32_t ds = lbu(d + 0x20);
            if (ds == 0xfe || ds == 0xfd || ds != lwu(pv + 0x8)) return;
            sb(m + 0x20, 4);
            return;
        }
        case 4: {  // (0x0031b54c)
            bool gone = false;
            const uint32_t ds = driver_state_at(lw(pv + 0x0), gone);
            if (gone || ds == lwu(pv + 0x4)) {
                moby_sound_start(0, 0, m);
                sb(m + 0x20, 2);
                return;
            }
            const float next = angle_sub(lwc1(m + 0x44), lwc1(pv + 0x14));
            if (!(0.0f < next)) {
                swc1(m + 0x44, next);
                return;
            }
            swc1(m + 0x44, 0.0f);
            swc1(pv + 0x18, 0.0f);
            sb(m + 0x20, 1);
            return;
        }
        default:  // (2, 5 and >= 6: 0x0031b5d8)
            return;
    }
}

// Class 1686, the breakable plant prop: waits in state 1 until hit with positive damage, then plays a sound, drops bolts and
// spawns debris 0x698..0x69a in state 2.
// orig: L7:0x0031f780
void umb_class1686_update_l7(uint32_t m) {
    const uint32_t hit = sub_0025a420(m, 0x10000u, 0);
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sb(m + 0x20, 1);
    } else if (st == 1) {
        if (hit != 0 && 0.0f < lwc1(hit + 0x2c)) {
            sb(m + 0x20, 2);
        }
    } else if (st == 2) {
        moby_sound_start(0, 0, m);
        const uint32_t zero_vec = L7_ADDR(0x0015f580);
        plant_bolts_drop_l1(m);
        debris_spawn(m, 0x698, m + 0x10, m + 0x40, 0, 0, zero_vec, zero_vec, zero_vec, 0.0f);
        debris_spawn(m, 0x699, m + 0x10, m + 0x40, 0, 0, zero_vec, zero_vec, zero_vec, 0.0f);
        debris_spawn(m, 0x69a, m + 0x10, m + 0x40, 0, 0, zero_vec, zero_vec, zero_vec, 0.0f);
        moby_retire(m);
    }
}

}  // namespace rac1
