// Quartu (level 15) class 1430, ported by hand from the level's own code: the pad where Clank becomes Giant Clank. It may
// ride along with a linked moby (its pvars +0xc: it moves by that moby's motion since the last frame). Its states:
//   0 set up: done already (its moby's saved bit, or the level's killed byte, set): to 4; else the level's flag 0x184aac
//     cleared, its 24 groups (the pvars +0x20..: -1 none) hidden (0x2ebf28), to 1
//   1 waiting: with Clank's item (0x13d38a) and the hero within 3, not Giant Clank: its level camera marked (+0: the camera
//     entry), to 2
//   2 the camera there (0x2f78e0): Clank (class 1428, 0x2eb9e0) put 2 to its right, the hero placed 3 to its right on the
//     floor facing along it, its first group shown, to 3
//   3 Clank swinging out and back (+8 the swing's angle, a turn every second at 50 and 60 Hz); back near the pad: its saved
//     bits set, to 4
//   4 done: the level's flag 0x184aac set, Clank kept (spawned again when missing) 0.5 to its right; once ten of its first
//     group's mobys are gone, the reward of 15000 bolts (once: 0x13d3f5)
// Its helpers: 0x2eb9e0 spawns the Clank (class 1428) at a place and a rotation, 0x2ebf28 hides or shows a group's mobys,
// 0x2ebfb0 counts a group's gone mobys, 0x2f78e0 tells whether the level camera of an index is the one running and settled.
// The original's order of calls and float operations is kept. Header quartu_mobys.h.
#include <game/ovl/quartu_mobys.h>

#include <game/lib/libgcc_fp.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quartu_own.h>
#include <game/ovl/variants_l15.h>

namespace rac1 {

using namespace quartu;

namespace {

constexpr float HALF_PI = 1.5707963705062866f;  // 0x3fc90fdb
constexpr float DEG = 0.01745329238474369f;     // 0x3c8efa35: one degree in radians

// the moby's saved bit (0x14c190: the levels' words of bits by uid; 0x1baad0: the level's own copy) and the level's killed
// byte (0x1bb830 + 0x454 by uid)
uint32_t saved_word(int32_t uid) { return 0x0014c190 + (static_cast<uint32_t>(uid >> 5) << 2) + (lwu(0x0015ed84) << 8); }

// (x, y) = `len` along the moby's yaw - 90 degrees, z = 0, at `at` (an EE vector: its w lane is left as it is)
void to_its_right(uint32_t m, uint32_t at, float len, bool doubled) {
    const float c = cos_vu(angle_add(lwc1(m + 0x48), -HALF_PI));
    swc1(at + 0x0, doubled ? fpu_add(c, c) : fpu_mul(c, len));
    const float s = sin_vu(angle_add(lwc1(m + 0x48), -HALF_PI));
    sw(at + 0x8, 0);
    swc1(at + 0x4, doubled ? fpu_add(s, s) : fpu_mul(s, len));
}

// the rotation of a moby facing along the pad: (0, pi / 2 if `tilt`, yaw + 90 degrees), at `at`
void along_pad(uint32_t m, uint32_t at, bool tilt) {
    vec_clear(V(at));
    swc1(at + 0x8, angle_add(lwc1(m + 0x48), HALF_PI));
    if (tilt) swc1(at + 0x4, HALF_PI);
}

}  // namespace

// orig: L15:0x002eb9e0
uint32_t qua_2eb9e0_l15(uint32_t pos, uint32_t rot) {
    const Vec4 at = V(pos), turn = V(rot);
    const uint32_t c = moby_spawn(0x594);  // class 1428
    if (c == 0) return 0;
    moby_copy_hero_color(c);
    sb(c + 0x30, 0xff);
    sh(c + 0x32, 0xff);
    sb(c + 0x31, 1);
    V(c + 0x10) = at;
    V(c + 0x40) = turn;
    moby_update_transform(c);
    return c;
}

// orig: L15:0x002ebf28
void qua_2ebf28_l15(int32_t group, bool show) {
    for_each_group_moby(group, [show](uint32_t g) {
        if (show) {
            sb(g + 0x31, 1);
            sh(g + 0x34, lhu(g + 0x34) & 0xfffc);
            sw(g + 0x94, lwu(lwu(g + 0x24) + 0x10));
        } else {
            sw(g + 0x94, 0);
            sh(g + 0x34, lhu(g + 0x34) | 0x3);
            sb(g + 0x31, 0);
        }
    });
}

// orig: L15:0x002ebfb0
int32_t qua_2ebfb0_l15(int32_t group) {
    int32_t gone = 0;
    for_each_group_moby(group, [&gone](uint32_t g) {
        if (g == 0 || lbu(g + 0x20) == 0xfe || lbu(g + 0x20) == 0xfd) ++gone;
    });
    return gone;
}

// orig: L15:0x002f78e0
bool qua_2f78e0_l15(int32_t camera) {
    if (lh(lwu(L15_ADDR(0x00167400u)) + 0x86) != 0x13) return true;  // (not the level's camera type: nothing to wait for)
    const int16_t mode = lh(lwu(lwu(lvd::camera_data(camera) + 0x4) + 0x70) + 0x54);  // (the camera record's +0x40 block, its +0x14)
    if (mode == 1 && lwc1(L15_ADDR(0x0015f3fcu)) != 0.0f) return true;
    return mode == 3;
}

// orig: L15:0x002ec030
void qua_class1430_update_l15(uint32_t m) {
    const Frame frame(0xb0);
    const uint32_t sp = frame.sp;  // +0 a place, +0x10 a rotation, +0x20 the pad's place (the original's frame)
    const uint32_t pv = lwu(m + 0x78);  // +0 its level camera, +4 the Clank, +8 the swing, +0xc the linked moby, +0x10 its place
    const int32_t uid = lh(m + 0xb2);
    if (lw(pv + 0xc) != -1) {
        if (lbu(m + 0x20) != 0) {  // moved along with the linked moby
            vec3_sub(V(sp + 0x10), V(lwu(OV(0x0015ffd8)) + (lwu(pv + 0xc) << 8) + 0x10), V(pv + 0x10));
            V(sp) = V(sp + 0x10);
            vec3_add_inplace(V(m + 0x10), V(sp));
        }
        V(pv + 0x10) = V(lwu(OV(0x0015ffd8)) + (lwu(pv + 0xc) << 8) + 0x10);
    }
    switch (lbu(m + 0x20)) {  // (the table L15:0x1e35a0)
    case 0: {  // (0x002ec0f4)
        if (lbu(L15_ADDR(0x001bb830u) + 0x454 + static_cast<uint32_t>(uid)) != 0 ||
            ((lw(saved_word(uid)) >> (lhu(m + 0xb2) & 0x1f)) & 1) != 0) {
            sb(m + 0x20, 4);
            return;
        }
        sw(L15_ADDR(0x00184aacu), 0);
        sb(m + 0x20, 1);
        for (uint32_t k = 0; k < 24; ++k) {
            const int32_t group = lw(pv + 0x20 + (k << 2));
            if (group != -1) qua_2ebf28_l15(group, false);
        }
        return;
    }
    case 1:  // (0x002ec1a0)
        if (lbu(0x0013d38a) == 0) return;
        if (!(vec2_distance(V(HERO_POS), V(m + 0x10)) < 3.0f)) return;
        if (lbu(HERO_FORM) != 0) return;
        cam_entry_mark(lw(pv + 0x0));
        sb(m + 0x20, 2);
        return;
    case 2: {  // (0x002ec1fc)
        if (!qua_2f78e0_l15(lw(pv + 0x0))) return;
        to_its_right(m, sp, 0.0f, true);
        vec3_add_inplace(V(sp), V(m + 0x10));
        along_pad(m, sp + 0x10, true);
        sw(pv + 0x4, qua_2eb9e0_l15(sp, sp + 0x10));
        sw(pv + 0x8, 0);
        to_its_right(m, sp, 3.0f, false);
        vec3_add_inplace(V(sp), V(m + 0x10));
        swc1(sp + 0x8, floor_probe_v(0.5f, V(sp), 0));
        along_pad(m, sp + 0x10, false);
        hero_place(sp, sp + 0x10, 0x72, 0);
        if (const int32_t group = lw(pv + 0x20); group != -1) qua_2ebf28_l15(group, true);
        sb(m + 0x20, 3);
        return;
    }
    case 3: {  // (0x002ec364)
        if (lwu(pv + 0x4) == 0) return;
        const float rate = fpu_div(360.0f, frame_scale_f(360.0f));
        const float swing = angle_add(lwc1(pv + 0x8), fpu_mul(rate, DEG));
        swc1(pv + 0x8, swing);
        const float out = fpu_add(cos_vu(swing), 1.0f);
        to_its_right(m, sp, out, false);
        const uint32_t clank = lwu(pv + 0x4);
        vec3_add(V(sp + 0x10), V(m + 0x10), V(sp));
        V(clank + 0x10) = V(sp + 0x10);
        if (!(out < 0.5f)) return;
        sw(saved_word(uid), lwu(saved_word(uid)) | (1u << (lhu(m + 0xb2) & 0x1f)));
        const uint32_t own = L15_ADDR(0x001baad0u) + (static_cast<uint32_t>(lh(m + 0xb2) >> 5) << 2);
        sw(own, lwu(own) | (1u << (lhu(m + 0xb2) & 0x1f)));
        sb(m + 0x20, 4);
        return;
    }
    case 4: {  // (0x002ec4ac)
        sw(L15_ADDR(0x00184aacu), 1);
        if (lwu(pv + 0x4) == 0) {  // (the place 0.5 to its right is computed, then the pad's own place is used)
            to_its_right(m, sp, 0.5f, false);
            vec3_add_inplace(V(sp), V(m + 0x10));
            along_pad(m, sp + 0x10, true);
            V(sp + 0x20) = V(m + 0x10);
            sw(pv + 0x4, qua_2eb9e0_l15(sp + 0x20, sp + 0x10));
        } else {
            to_its_right(m, sp, 0.5f, false);
            vec3_add_inplace(V(sp), V(m + 0x10));
            along_pad(m, sp + 0x10, true);
            V(lwu(pv + 0x4) + 0x10) = V(sp);
            V(lwu(pv + 0x4) + 0x40) = V(sp + 0x10);
        }
        if (lbu(0x0013d3f5) != 0) return;
        if (qua_2ebfb0_l15(lw(pv + 0x20)) < 10) return;
        sb(0x0013d3f5, 1);
        sw(0x0015ed98, lwu(0x0015ed98) + 0x3a98);
        return;
    }
    default:
        return;
    }
}

}  // namespace rac1
