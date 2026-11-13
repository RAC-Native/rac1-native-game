// The camera of Blarg's (level 6) class 1051, the hydro pumps' switch, while its pump runs. The level carries two copies of
// the follow and three of the start (one a pump), the same code word for word. Nothing of it is the level's own: the hero
// (0x13f350, its position 0x13f3d0), the frame time and its square (0x15ed6c / 0x15ed70) are the base ELF's, the camera
// 0x166dc0, the spots 0x1600ec and the paths 0x1b04b0 the engine's (OV). Header shared_pump_camera.h.
//
// The pvars: +0xec the pump's path, +0xf8 the hero's spot, +0x118 the camera's blend (0 behind the switch, 1 behind the
// hero) and +0x11c its rate. The moby's animation +0x53.
#include <game/ovl/shared_pump_camera.h>

#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;
constexpr uint32_t HERO_POS = 0x0013f3d0;

// the camera at `from` turned to look at the switch: its yaw (rot +8) and its pitch (rot +4) less `tilt`
float pitch_to(uint32_t m, uint32_t from, uint32_t target) {
    return -atan2_f(vec2_distance(V(from), V(target)), fpu_sub(lwc1(m + 0x18), lwc1(from + 8)));
}
void yaw_to(uint32_t m, uint32_t from, uint32_t rot) {
    swc1(rot + 8, atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(from)), fpu_sub(lwc1(m + 0x14), lwc1(from + 4))));
}
}  // namespace

// orig: L6:0x002fb148 L6:0x002fb508 L6:0x002fb8c8
void pump_camera_start(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t cam = sp, rot = sp + 0x10, back = sp + 0x20;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t spot = lwu(OV(0x001600ec)) + (lwu(pv + 0xf8) << 7);
    sw(pv + 0x118, 0);
    hero_place(spot + 0x30, spot + 0x70, 0x72, 0);
    // 8 behind the switch, 2 above the hero
    lq_sq(cam, m + 0x10);
    swc1(cam + 8, fpu_add(lwc1(HERO_POS + 8), 2.0f));
    vec3_set_length(V(back), V(m + 0xc0), 8.0f);
    vec3_add(V(cam), V(cam), V(back));
    vec_clear(V(rot));
    yaw_to(m, cam, rot);
    swc1(rot + 4, fpu_sub(pitch_to(m, cam, m + 0x10), u2f(0x3f060a92u)));  // (30 degrees down)
    camera_scripted_set(cam, rot, 1, 0, 0);
    uid5_set_pvar_80(cam);
    uid5_set_pvar_90(rot);
    sw(lwu(OV(0x001b04b0) + (lwu(pv + 0xec) << 2)), 0);
    ee_stack_free(0x80);
}

// orig: L6:0x002fb668 L6:0x002fba28
uint32_t pump_camera_follow(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xd0);
    const uint32_t cam = sp, rot = sp + 0x10, behind_switch = sp + 0x20, behind_hero = sp + 0x30;
    const uint32_t pv = lwu(m + 0x78);
    vec_clear(V(rot));
    if (lbu(m + 0x53) == 6) {
        lq_sq(cam, OV(0x00166dc0));
    } else {
        const float half_sq = fpu_mul(lwc1(0x0015ed70), 0.5f);
        seek_1d(pv + 0x118, pv + 0x11c, 1.0f, half_sq, half_sq, fpu_mul(lwc1(0x0015ed6c), 0.5f));
        // 8 behind the switch, 2 above the hero; 7 behind the hero, 4 above it
        vec3_set_length(V(behind_switch), V(m + 0xc0), 8.0f);
        vec3_add(V(behind_switch), V(behind_switch), V(m + 0x10));
        swc1(behind_switch + 8, fpu_add(lwc1(HERO_POS + 8), 2.0f));
        vec3_set_length(V(behind_hero), V(HERO), -7.0f);
        vec3_add(V(behind_hero), V(behind_hero), V(HERO_POS));
        swc1(behind_hero + 8, fpu_add(lwc1(behind_hero + 8), 4.0f));
        vec3_lerp(V(cam), V(behind_switch), V(behind_hero), lwc1(pv + 0x118));
    }
    yaw_to(m, cam, rot);
    // the pitch less up to 30 degrees as the blend is short of 1
    const float lift = fpu_mul(fpu_mul(fpu_sub(1.0f, lwc1(pv + 0x118)), 30.0f), u2f(0x3c8efa35u));
    swc1(rot + 4, fpu_sub(pitch_to(m, cam, m + 0x10), lift));
    uid5_set_pvar_80(cam);
    uid5_set_pvar_90(rot);
    const uint32_t done = 1.0f <= lwc1(pv + 0x118) && lbu(m + 0x53) == 1 ? 1u : 0u;
    ee_stack_free(0xd0);
    return done;
}

}  // namespace rac1
