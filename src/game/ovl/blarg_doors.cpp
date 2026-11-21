// Blarg's Nebula G34 (level 6) own moby classes, hand-ported: the landing bay's doors (class 55) and a panel that slides
// out (class 367). Header blarg_own.h; registered in blarg_mobys.cpp under the names the generated code had. Addresses are
// level 6's (`orig: L6:`); its own data is L6_ADDR, the base ELF's are bare.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
}  // namespace

// Class 55, a half of the landing bay's doors: it swings about the centre of its box (pvar +0, a box of the level) on a
// circle of the radius it starts at (+0xc), by the angle +0x18 seeking +0x14 (yaw_seek, its rate +8). While the game is in
// its landing scene (mode 6 at 0x15f5c4) it opens by 20 degrees as the ship arrives (the scene's step 3 at 0x13e050) and
// closes as it leaves (step 8), once per scene (+0x10). State 0 measures the radius and keeps the yaw (+4); the moby turned
// about y (+0x44) is the half that swings the other way (state 2, else 1).
// orig: L6:0x002b4770
void blg_class55_update_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float open = u2f(0x3eb2b8c2u);  // 20 degrees
    if (lw(L6_ADDR(0x0015f5c4)) != 6) {
        sw(pv + 0x10, 0);
    } else if (lw(pv + 0x10) == 0) {
        const int32_t step = lw(0x0013e050);
        if (step == 8) {  // the ship leaves: from open to shut
            sw(pv + 0x10, 1);
            swc1(pv + 0x18, open);
            sw(pv + 0x14, 0);
        } else if (step == 3) {  // the ship arrives: from shut to open
            sw(pv + 0x10, 1);
            swc1(pv + 0x14, open);
            sw(pv + 0x18, 0);
        }
    }
    const float half_pi = u2f(0x3fc90fdbu);
    if (lwc1(pv + 0x18) != lwc1(pv + 0x14)) {
        const float accel = fpu_mul(dt_sq(), half_pi);
        yaw_seek(pv + 0x18, pv + 0x8, lwc1(pv + 0x14), accel, accel, fpu_mul(dt(), half_pi));
    }
    const float minus_half_pi = u2f(0xbfc90fd0u);
    // the moby on its circle at the angle `ang` about the box's centre (its height kept)
    auto place = [&](float ang) {
        const uint32_t pos = m + 0x10;
        const float z = lwc1(m + 0x18);
        swc1(pos, fpu_mul(cos_vu(ang), lwc1(pv + 0xc)));
        const float s = sin_vu(ang);
        sw(pos + 8, 0);
        swc1(pos + 4, fpu_mul(s, lwc1(pv + 0xc)));
        vec3_add(V(pos), V(pos), V(lvd::box(lvd::BOXES, lw(pv)) + 0x30));
        swc1(m + 0x18, z);
    };
    switch (lbu(m + 0x20)) {
    case 0:
        swc1(pv + 0xc, vec2_distance(V(m + 0x10), V(lvd::box(lvd::BOXES, lw(pv)) + 0x30)));
        swc1(pv + 0x4, lwc1(m + 0x48));
        sb(m + 0x20, lwc1(m + 0x44) == 0.0f ? 2 : 1);
        break;
    case 1:
        place(angle_add(lwc1(pv + 0x18), minus_half_pi));
        swc1(m + 0x48, angle_add(lwc1(pv + 0x4), lwc1(pv + 0x18)));
        break;
    case 2:
        place(angle_sub(minus_half_pi, lwc1(pv + 0x18)));
        swc1(m + 0x48, angle_sub(lwc1(pv + 0x4), lwc1(pv + 0x18)));
        break;
    default:
        break;
    }
}

// Class 367, a panel that slides out: state 0 notes its place (pvar +0) and the place 11 units along its forward axis
// (+0x10), then waits (state 1); in state 2 (set by another class) it moves between them by the fraction +0x20 seeking
// +0x28 (seek_1d, its rate +0x24), and waits again once there.
// orig: L6:0x002d9d10
void blg_class367_update_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10;
    switch (lbu(m + 0x20)) {
    case 0:
        lq_sq(pv, pos);
        vec3_set_length(V(pv + 0x10), V(m + 0xd0), 11.0f);
        vec3_add(V(pv + 0x10), V(pv + 0x10), V(pos));
        sb(m + 0x20, 1);
        break;
    case 2: {
        const float accel = fpu_mul(dt_sq(), 10.0f);
        seek_1d(pv + 0x20, pv + 0x24, lwc1(pv + 0x28), accel, accel, fpu_mul(dt(), 20.0f));
        vec3_lerp(V(pos), V(pv), V(pv + 0x10), lwc1(pv + 0x20));
        if (lwc1(pv + 0x20) == lwc1(pv + 0x28)) sb(m + 0x20, 1);
        break;
    }
    default:
        break;
    }
}

}  // namespace rac1
