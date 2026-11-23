// Quartu (level 15): the level's camera type 22 (Giant Clank's camera), ported by hand from the level's own code. Its init,
// update and exit are empty (quartu_hooks.cpp); its accept always refuses (-1), but while the current camera is the
// type's own (its +0x86 is 0), the type's data block's counter (+0x20) is not negative and the hero is Giant Clank (his
// movement mode 2), it first runs 0x2f88e8, which steers the running camera for him:
//   - claimed (cam_mod_claim) and the hero inside its volume (0x2f85a8: the data's cylinder +0x28 excluded, then its box
//     +0xc, cylinder +0x10, sphere +8 or polygon path +0x14), else the counter cleared (and the claim released);
//   - the counter +0x20 runs up to 5 seconds (reset to 5 while the right stick moves, 0x13ca40 / 0x13ca44); past 6.7 seconds
//     (+0x3c) with the camera not yet within 30 degrees of the data's box (0x2f8698) it starts again from 9.3 seconds;
//   - while it runs the camera turns around the box's centre (cam0_turn_point) at the data's rate (+0 degrees a second)
//     scaled by the counter's share of 3.3 seconds, and takes the data's distance (+0x34), height (+0x38) and look height
//     (+0x44) when they are set; the camera's frame (0x167400's +0x70) gets the mode 2 at +0x230 and cam0_set_20c its
//     1 / 12 / 0.11.
// No VU0 instruction of its own: the vector helpers are the engine's host ports, as the transliteration called them. The
// original's order of calls and float operations is kept. Header quartu_mobys.h.
#include <game/ovl/quartu_mobys.h>

#include <game/lib/libgcc_fp.h>
#include <game/math/vecmath.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quartu_own.h>
#include <game/ovl/variants_l15.h>

namespace rac1 {

using namespace quartu;

namespace {
constexpr float DEG = 0.01745329238474369f;  // 0x3c8efa35: one degree in radians
uint32_t current_camera() { return lwu(L15_ADDR(0x00167400u)); }
uint32_t data_of(uint32_t cam) { return lvd::camera_data(lh(cam + 0x84)); }
}  // namespace

// orig: L15:0x002f85a8
bool qua_2f85a8_l15(uint32_t cam) {
    const uint32_t d = data_of(cam);
    if (lw(d + 0x28) >= 0 && point_in_cylinder(HERO_POS, lw(d + 0x28)) != 0) return false;  // (its excluded cylinder)
    if (lw(d + 0xc) >= 0) return point_in_box(HERO_POS, lw(d + 0xc)) != 0;
    if (lw(d + 0x10) >= 0) return point_in_cylinder(HERO_POS, lw(d + 0x10)) != 0;
    if (lw(d + 0x8) >= 0) return point_in_sphere(HERO_POS, lw(d + 0x8)) != 0;
    if (lw(d + 0x14) < 0) return false;
    const uint32_t polygon = lvd::path(lw(d + 0x14));
    return point_in_polygon(HERO_POS, polygon + 0x10, lw(polygon + 0x0)) != 0;
}

// orig: L15:0x002f8698
bool qua_2f8698_l15(uint32_t cam, float turn_tol, float tilt_tol) {
    const uint32_t box = lvd::box(lvd::BOXES, lw(data_of(cam) + 0x48));
    if (turn_tol == 0.0f && tilt_tol == 0.0f) return true;
    const Vec4& axis = V(L15_ADDR(0x00167410u) + 0x30);  // (the camera globals' up axis)
    Vec4 to_box, along, box_off, cam_off;
    vec3_sub(to_box, V(box + 0x30), V(current_camera() + 0x30));
    const float box_up = vec3_dot(to_box, axis);
    vec3_scale(along, axis, box_up);
    vec3_sub(box_off, to_box, along);
    const float box_out = vec3_length(box_off);
    const float cam_up = vec3_dot(V(current_camera()), axis);
    vec3_scale(along, axis, cam_up);
    vec3_sub(cam_off, V(current_camera()), along);
    const float cam_out = vec3_length(cam_off);
    if (tilt_tol != 0.0f) {
        const float a = atan2_f(box_out, box_up);
        const float b = atan2_f(cam_out, cam_up);
        if (fpu_mul(tilt_tol, DEG) < angle_dist(a, b)) return false;
    }
    if (turn_tol == 0.0f) return true;
    const float dot = vec3_dot(box_off, cam_off);
    const float lengths = fpu_mul(cam_out, box_out);
    if (lengths == 0.0f) return false;
    const float off = abs_f(fpu_sub(u2f(0x3fc90fdb), asin_f(fpu_div(dot, lengths))));  // (pi / 2 - the angle's asin)
    return off < fpu_mul(turn_tol, DEG);
}

// orig: L15:0x002f88e8
void qua_2f88e8_l15(uint32_t cam) {
    const uint32_t d = data_of(cam);
    const uint32_t box = lvd::box(lvd::BOXES, lw(d + 0x48));
    if (cam_mod_claim(cam) <= 0) {
        sh(d + 0x20, 0);
        return;
    }
    if (!qua_2f85a8_l15(cam)) {
        sh(d + 0x20, 0);
        cam_mod_release(cam);
        return;
    }
    sw(lwu(current_camera() + 0x70) + 0x230, 2);
    sh(d + 0x20, lhu(d + 0x20) + 1);
    const int32_t hold = frame_scale_i(0x12c), retry = frame_scale_i(0x190), restart = frame_scale_i(0x230), ramp = frame_scale_i(0xc8);
    cam0_set_20c(1.0f, 12.0f, u2f(0x3de147ae));  // (0.11)
    if (lwc1(0x0013ca40) != 0.0f || lwc1(0x0013ca44) != 0.0f) sh(d + 0x20, static_cast<uint32_t>(hold));  // (the right stick moved)
    bool turning = true;
    if (!(lh(d + 0x20) < hold)) {
        const int32_t n = lw(d + 0x3c) + 1;
        sw(d + 0x3c, static_cast<uint32_t>(n));
        if (n < retry) turning = false;
        else if (!qua_2f8698_l15(cam, 30.0f, 0.0f)) turning = false;
        else sh(d + 0x20, static_cast<uint32_t>(restart));
    }
    if (!(lh(d + 0x20) < restart)) {
        turning = true;
        sw(d + 0x3c, 0);
        sh(d + 0x20, 1);
    }
    if (ramp < lh(d + 0x20) && lh(d + 0x20) < hold) {
        sw(d + 0x3c, static_cast<uint32_t>(hold));
        sh(d + 0x20, static_cast<uint32_t>(ramp));
    }
    const float share = fpu_div(int_to_float(lh(d + 0x20)), int_to_float(ramp));
    const float rate = fpu_mul(fpu_mul(lwc1(d + 0x0), DEG), share);
    if (turning) cam0_turn_point(box + 0x30, rate, 0.0f);
    if (lwc1(d + 0x34) != 0.0f) {
        cam0_set_distance(0, lwc1(d + 0x34), u2f(0x3b449ba6));  // (0.003)
        cam0_set_flag226();
    }
    if (lwc1(d + 0x38) != 0.0f) cam0_set_height(lwc1(d + 0x38), u2f(0x3b449ba6));
    if (lwc1(d + 0x44) != 0.0f) cam0_set_look_height(0, lwc1(d + 0x44), u2f(0x3ba3d70a));  // (0.005)
}

// orig: L15:0x002f8ba8
int32_t qua_cam_type22_accept_l15(uint32_t cam) {
    if (lh(current_camera() + 0x86) == 0 && lh(data_of(cam) + 0x20) >= 0 && lbu(HERO_FORM) == 2) qua_2f88e8_l15(cam);
    return -1;
}

}  // namespace rac1
