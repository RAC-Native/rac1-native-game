// Quartu (level 15): small classes of the Giant Clank part, ported by hand from the level's own code.
//   class 937   a wheel that turns while a linked moby (its pvars +0) is in its state 2 or 4, one way or the other by that
//               moby's pvar flag +0x10, at the level's rate (degrees a second)
//   class 1250  a conveyor piece: moves what stands on it at 2.5 a second along its yaw + 90 degrees (attach_frame_set)
//   class 1560  a scene's smoke: in a scene (0x15f5c4 == 2) the smoke of one of the scene's mobys, by the scene's shot
//   class 1394  a dropper: every few seconds (its pvars +4 the first time, the level's 0x16207c after) a class 1257 ball
//               at its place (0x2e7ed8)
//   class 1257  the ball: falls under the level's gravity, rides a moby it lands on (one whose pvar word 2 is set), and
//               bursts (blast, sound, hits around it) on anything else, on a hit, or when hitting the floor fast
//   class 1425  a steam vent: now and then (one frame in the level's 0x16219c - 1) a puff of particle 0x22 upward
// The original's order of calls and float operations is kept. Header quartu_mobys.h.
#include <game/ovl/quartu_mobys.h>

#include <game/collision/collision.h>
#include <game/lib/libgcc_fp.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quartu_own.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/weapons.h>

namespace rac1 {

using namespace quartu;

namespace {
constexpr float HALF_PI = 1.5707963705062866f;  // 0x3fc90fdb
constexpr float DEG = 0.01745329238474369f;     // 0x3c8efa35: one degree in radians
}  // namespace

// orig: L15:0x002e46b0
void qua_class937_update_l15(uint32_t m) {
    const int32_t link = lw(lwu(m + 0x78) + 0x0);
    if (link == -1) return;
    const uint32_t other = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(link) << 8);
    const uint32_t state = lbu(other + 0x20);
    if (state != 2 && state != 4) return;
    const bool flag = lw(lwu(other + 0x78) + 0x10) != 0;
    float step = fpu_mul(fpu_mul(lwc1(L15_ADDR(0x00161ec0u)), DEG), frame_time());
    if (state == 2 ? flag : !flag) step = -step;  // (state 2: backward with the flag, state 4: backward without it)
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), step));
}

// orig: L15:0x002e73c0
void qua_class1250_update_l15(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        vec_clear(V(pv + 0x40));
        sw(pv + 0x9c, lwu(pv + 0x9c) | 4);
        sb(m + 0x20, 3);
    } else if (state == 3) {
        swc1(m + 0x58, u2f(0x3fb1c71d));  // (1.3889)
        const float c = cos_vu(angle_add(lwc1(m + 0x48), HALF_PI));
        swc1(pv + 0x40, fpu_mul(c, fpu_mul(frame_time(), 2.5f)));
        const float s = sin_vu(angle_add(lwc1(m + 0x48), HALF_PI));
        sw(pv + 0x48, 0);
        swc1(pv + 0x44, fpu_mul(s, fpu_mul(frame_time(), 2.5f)));
    }
    attach_frame_set(pv + 0x60, pv + 0x40, m + 0x40, m + 0x40);
}

// orig: L15:0x002edb20
void qua_class1560_update_l15(uint32_t m) {
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        return;
    }
    if (state != 1 || lw(L15_ADDR(0x0015f5c4u)) != 2) return;
    const int32_t shot = lw(L15_ADDR(0x0016ce90u));
    uint32_t which;
    if (shot == 1) which = 4;
    else if (shot >= 2 && shot <= 4) which = 2;
    else if (shot == 5) which = 0;
    else if (shot == 6) which = 1;
    else return;
    scene_moby_smoke(lwu(L15_ADDR(0x0016cfd8u) + (which << 2)));  // (the scene's moby list: 0x16ce60 + 0x178)
}

// orig: L15:0x002e7ed8 (its a0, the dropper, is not read)
uint32_t qua_2e7ed8_l15(uint32_t pos) {
    const uint32_t b = moby_spawn(0x4e9);  // class 1257
    if (b == 0) return 0;
    const uint32_t bpv = lwu(b + 0x78);
    sh(b + 0x32, 0x7f);
    sb(b + 0x30, 0xff);
    sb(b + 0x31, 1);
    sb(b + 0x20, 0);
    sb(b + 0xbc, 0);
    sw(bpv + 0x10, 0);
    vec_clear(V(bpv));
    V(b + 0x10) = V(pos);
    sw(b + 0x94, lwu(lwu(b + 0x24) + 0x10));
    moby_update_transform(b);
    return b;
}

// orig: L15:0x002eabd8
void qua_class1394_update_l15(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        sw(pv + 0x0, static_cast<uint32_t>(float_to_int(frame_scale_f(fpu_mul(lwc1(pv + 0x4), 60.0f)))));
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
    } else if (state == 1) {
        if (tick32(pv + 0x0) == 0) return;
        sw(pv + 0x0, static_cast<uint32_t>(float_to_int(frame_scale_f(fpu_mul(lwc1(L15_ADDR(0x0016207cu)), 60.0f)))));
        qua_2e7ed8_l15(m + 0x10);
    }
}

// orig: L15:0x002e7f68
void qua_class1257_update_l15(uint32_t m) {
    const Frame frame(0xd0);
    const uint32_t pv = lwu(m + 0x78);  // +0 the velocity, +0x10 the moby it rides
    const Vec4 from = V(m + 0x10);
    Vec4 bounce;  // (the bounce off the floor: computed, then the ball bursts; nothing reads it)
    vec_clear(bounce);
    // touching something that is not a moby to ride: burst
    if (sphere_collide(m + 0x10, 0.5f, 1, m) != 0) {
        const uint32_t met = coll_hit_moby();
        if (met == 0 || moby_pvar_word2(met) == 0) sb(m + 0xbc, 1);
    }
    // a hit from a moby of another class: burst
    if (const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00830000u), 0); hit != 0) {
        const uint32_t src = lwu(hit + 0x20);
        if (src != 0 && lh(src + 0xa6) != lh(m + 0xa6)) sb(m + 0xbc, 1);
    }
    sb(m + 0xa4, 0xff);

    switch (lbu(m + 0x20)) {
    case 0:  // falling
        swc1(pv + 0x8, fpu_sub(lwc1(pv + 0x8), fpu_mul(lwc1(L15_ADDR(0x00161fbcu)), frame_time2())));
        vec3_add_inplace(V(m + 0x10), V(pv));
        if (segment_collide_v(from, V(m + 0x10), 0x10, m, nullptr) == 0) break;
        if (coll_hit_moby() != 0 && moby_pvar_word2(coll_hit_moby()) != 0) {  // landed on a moby to ride
            sw(pv + 0x10, coll_hit_moby());
            sb(m + 0x20, 1);
            sw(pv + 0x8, 0);
            V(m + 0x10) = coll_hit_point();
            break;
        }
        if (coll_hit_moby() == lwu(HERO_MOBY) || lwc1(pv + 0x8) < fpu_mul(frame_time(), u2f(0xc11ccccd))) {  // (-9.8)
            sb(m + 0xbc, 1);
            V(m + 0x10) = coll_hit_point();
            vec3_reflect(bounce, V(pv), coll_hit_normal());
            vec3_set_length(bounce, bounce, fpu_add(frame_time(), frame_time()));
        }
        break;
    case 1: {  // riding a moby
        floor_probe_v(0.5f, V(m + 0x10), 0);
        uint32_t riding = 0;
        if (const uint32_t mount = lwu(pv + 0x10); mount != 0) {
            const uint32_t st = lbu(mount + 0x20);
            if (st == 0xfe || st == 0xfd) {  // the mount is gone
                sb(m + 0xbc, 1);
                sw(pv + 0x10, 0);
            } else if (coll_hit_moby() != 0) {
                riding = moby_pvar_word2(coll_hit_moby());
                if (riding != 0) {
                    sub_00260400(m, lwu(pv + 0x10), m + 0x10, m + 0x40, m + 0x10, m + 0x40);
                    vec3_sub(V(pv), V(m + 0x10), from);
                }
            }
        }
        if (riding == 0) {  // falling again
            sb(m + 0x20, 0);
            sw(pv + 0x8, 0);
            sw(pv + 0x10, 0);
        }
        break;
    }
    default:
        break;
    }

    if (lbu(m + 0xbc) == 0) return;
    explosion_effect(m, pv, 0, 3, 3, 5, -1, 0, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 4.0f, 1.0f, 7.0f);
    moby_sound_play(0, 0, m, 0x79);
    const int32_t n = mobys_in_sphere(m + 0x10, 2.0f, 0x10, m, 0);
    const uint32_t at = frame.sp + 0x40;
    V(at) = V(m + 0x10);
    moby_list_hit(m, at, coll_hit_list(), n, 0, NOT_ADDR(0x00830001u), 2, 1, 1.0f, 1.0f, 1.0f);
    moby_retire(m);
}

// orig: L15:0x002eb928
void qua_class1425_update_l15(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (rand_mod(lw(L15_ADDR(0x0016219cu)) - 1) != 0) return;
    const Frame frame(0x40);
    const uint32_t vel = frame.sp;
    vec_clear(V(vel));
    const float up = rand_range_f(0.75f, 1.5f);
    swc1(vel + 0x8, fpu_mul(fpu_mul(up, lwc1(L15_ADDR(0x00162198u))), frame_time()));
    const float size = rand_range_f(u2f(0x3d75c28f), u2f(0x3df5c28f));  // (0.06 .. 0.12)
    spawn_particle_22(m + 0x10, vel, fpu_mul(size, 210000.0f), lwc1(pv + 0x0));
}

}  // namespace rac1
