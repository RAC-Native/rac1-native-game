// Kerwan's (level 3) camera types 1, 14 and 18, and the camera code several levels carry a copy of: camera type 1 (Kerwan's
// and Eudora's), camera type 18 (Aridia's, Kerwan's, Blarg's, Umbris's, Batalia's, Hoven's and Veldin's finale's), the
// follow camera's look-height hook and its end-of-update callback (every level but Veldin, Novalis and Batalia), the
// callback queue they use, the follow camera's distance rates (0x2e6f80's copies) and word +0x218 (0x2e6f58's) and the turn
// toward a direction (0x24e6f8's). Those read the engine's data (OV) and the base ELF's; the levels' own words (the
// callback's address, the step the callback counts down by) come from per-level tables. Type 14 is Kerwan's alone (L3_ADDR).
#include <game/ovl/kerwan_camera.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l10.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/variants_l3.h>
#include <game/ovl/variants_l4.h>
#include <game/ovl/variants_l5.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/variants_l9.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {

constexpr uint32_t HERO_POS = 0x0013f3d0u;
constexpr uint32_t HERO_AXIS = 0x0013f5e0u;
constexpr uint32_t HERO_PLAT = 0x0013f64cu;
constexpr uint32_t HERO_MOBY = 0x001413d0u;
constexpr uint32_t HERO_STATE = 0x001413d4u;
constexpr uint32_t HERO_CAM_REQ = 0x001415d4u;
constexpr uint32_t PAD_RX = 0x0013ca40u;
constexpr uint32_t PAD_RY = 0x0013ca44u;

uint32_t entry_of(uint32_t cam) {
    return lwu(OV(0x0015ef50u)) + static_cast<uint32_t>(lh(cam + 0x84u) * 0x20);
}

// The level's own end-of-update callback of the look-height hook (cam0_level_callback's copy: the address the hook
// queues and camera_callback_call compares)
uint32_t level_callback_addr() {
    if (on_camera_level(KERWAN)) return L3_ADDR(0x002eb468u);
    if (on_camera_level(ARIDIA)) return L2_ADDR(0x002fa770u);
    if (on_camera_level(EUDORA)) return L4_ADDR(0x002f04a8u);
    if (on_camera_level(RILGAR)) return L5_ADDR(0x00328fe0u);
    if (on_camera_level(BLARG)) return L6_ADDR(0x00314810u);
    if (on_camera_level(UMBRIS)) return L7_ADDR(0x003280f0u);
    if (on_camera_level(GASPAR)) return L9_ADDR(0x00314280u);
    if (on_camera_level(ORXON)) return L10_ADDR(0x002f4b78u);
    if (on_camera_level(POKITARU)) return L11_ADDR(0x00327af0u);
    if (on_camera_level(HOVEN)) return L12_ADDR(0x00314950u);
    if (on_camera_level(GEMLIK)) return L13_ADDR(0x00316ba8u);
    if (on_camera_level(OLTANIS)) return L14_ADDR(0x00313240u);
    if (on_camera_level(QUARTU)) return L15_ADDR(0x002f69e0u);
    if (on_camera_level(KALEBO)) return L16_ADDR(0x002f3ae8u);
    if (on_camera_level(VELDIN_ORBIT)) return L17_ADDR(0x002fff98u);
    return L18_ADDR(0x00305378u);
}

// The level's own float the callback counts the look height down by each frame (a word of the level's small data)
uint32_t level_look_step() {
    if (on_camera_level(KERWAN)) return L3_ADDR(0x00161e54u);
    if (on_camera_level(ARIDIA)) return L2_ADDR(0x0016238cu);
    if (on_camera_level(EUDORA)) return L4_ADDR(0x00161f4cu);
    if (on_camera_level(RILGAR)) return L5_ADDR(0x001621a4u);
    if (on_camera_level(BLARG)) return L6_ADDR(0x0016256cu);
    if (on_camera_level(UMBRIS)) return L7_ADDR(0x00161e24u);
    if (on_camera_level(GASPAR)) return L9_ADDR(0x00161f3cu);
    if (on_camera_level(ORXON)) return L10_ADDR(0x0016220cu);
    if (on_camera_level(POKITARU)) return L11_ADDR(0x00162774u);
    if (on_camera_level(HOVEN)) return L12_ADDR(0x0016224cu);
    if (on_camera_level(GEMLIK)) return L13_ADDR(0x001620c4u);
    if (on_camera_level(OLTANIS)) return L14_ADDR(0x0016247cu);
    if (on_camera_level(QUARTU)) return L15_ADDR(0x001623ccu);
    if (on_camera_level(KALEBO)) return L16_ADDR(0x00162194u);
    if (on_camera_level(VELDIN_ORBIT)) return L17_ADDR(0x0016267cu);
    return L18_ADDR(0x00162794u);
}

}  // namespace

// `out`: `from` turned toward `to` by `frac` of the angle between them (`unit_flag` 0: the angle from their unit
// directions; `from` itself when either is zero), a rotation by a quaternion about their cross product.
// orig: L3:0x0024e6f8 L4:0x00252ce0
void slerp_vec_toward(Vec4& out, const Vec4& from, const Vec4& to, int32_t unit_flag, float frac) {
    Vec4 q{};
    vec3_cross_ba(q, to, from);
    vec3_set_length(q, q, 1.0f);
    float d = vec3_dot(from, to);
    if (unit_flag == 0) {
        const float denom = fpu_mul(vec3_length(from), vec3_length(to));
        if (denom == 0.0f) {
            out = from;
            return;
        }
        d = fpu_div(d, denom);
    }
    const float half_ang = fpu_mul(fpu_mul(fpu_sub(u2f(0x3fc90fdbu), asin_f(d)), 0.5f), frac);
    vec3_scale(q, q, sin_vu(half_ang));
    q.w = cos_vu(half_ang);
    quat_rotate(out, from, q);
}

namespace {

// orig: L3:0x002eb7e0
void type14_place_l3(uint32_t cam) {
    const uint32_t w = lwu(cam + 0x70u);
    Vec4 up_off{}, back_off{}, tmp{}, look_pt{}, dir{};
    vec3_set_length(up_off, V(HERO_AXIS), -lwc1(w + 0xe0u));
    vec3_set_length(back_off, V(w + 0xf0u), -lwc1(w + 0xd0u));
    vec3_add(tmp, V(HERO_POS), up_off);
    vec3_add(V(cam + 0x30u), tmp, back_off);
    vec3_set_length(look_pt, V(HERO_AXIS), -lwc1(w + 0xb0u + 0x14u));
    vec3_add(look_pt, V(HERO_POS), look_pt);
    vec3_sub(dir, look_pt, V(cam + 0x30u));
    vec3_set_length(V(w + 0x100u), dir, 1.0f);
    vec3_set_length(V(cam), V(w + 0x100u), 1.0f);
    vec3_set_length(V(cam + 0x40u), V(cam), 1.0f);
    lq_sq(cam, cam + 0x40u);
    vec3_cross_ba(V(cam + 0x10u), V(cam), V(HERO_AXIS));
    vec3_set_length(V(cam + 0x10u), V(cam + 0x10u), -1.0f);
    vec3_cross_ba(V(cam + 0x20u), V(cam + 0x10u), V(cam));
}

}  // namespace

// The follow camera's (the active camera 0x166e00 when it is type 0) word +0x218 of its work block set to `v`.
// (Shared: the same code on Aridia, Kerwan, Blarg, Umbris, Batalia, Hoven and Veldin's finale.)
// orig: L3:0x002e6f58 L2:0x002f6570 L6:0x00310690 L7:0x00323fd0 L8:0x003101f0 L12:0x003107f0 L18:0x00301258
void cam0_set_218(uint32_t v) {
    const uint32_t c = lwu(OV(0x00166e00u));
    if (lh(c + 0x86u) == 0) {
        sw(lwu(c + 0x70u) + 0x218u, v);
    }
}

// The follow camera's (the active camera 0x166e00 when it is type 0) three rates of its distance block (+0x1d0 of its work
// block: +0x3c / +0x40 / +0x44), each set only when it is not 0.
// orig: L3:0x002e6f80 L2:0x002f6598 L6:0x003106b8 L7:0x00323ff8 L8:0x00310218 L12:0x00310818 L15:0x002f28b8 L18:0x00301280
void cam0_set_20c(float a, float b, float g) {
    const uint32_t c = lwu(OV(0x00166e00u));
    if (lh(c + 0x86u) != 0) return;
    const uint32_t p = lwu(c + 0x70u) + 0x1d0u;
    if (lwc1(p + 0x3cu) != 0.0f) swc1(p + 0x3cu, a);
    if (lwc1(p + 0x40u) != 0.0f) swc1(p + 0x40u, b);
    if (lwc1(p + 0x44u) != 0.0f) swc1(p + 0x44u, g);
}

// Camera type 18 (0x12): a modifier of the follow camera while the hero is in its zone near a moby of its data (+0x48; the
// moby of group +0x44, or the moby +0x28), which turns the follow camera toward the moby or a direction by the kind +0x3c
// (1 / 2 after the hero leaves the right stick alone, 3 the moby with a slow start, 4 / 5 a yaw, 6 while the hero stands on
// the moby, 7 after the stick and a pause), and sets its distance, height and look height (+0x34 / +0x38 / +0x4c).
// (Shared: the same code on Aridia, Kerwan, Blarg, Umbris, Batalia, Hoven and Veldin's finale; it reads the engine's words
// through OV.)

// Whether the hero is in the camera's zone: the moby the camera block's +0xdc names, the moby he stands on (kind 6), within
// the radius +0x24 of the moby (+0x22 == 1), or in the box / cylinder / sphere / polygon of the data (+0xc / +0x10 / +8 /
// +0x14, the first that is set).
// orig: L3:0x002ec9a8 L2:0x002fb648 L6:0x003156e8 L7:0x00328468 L8:0x00315648 L12:0x00315828 L18:0x003056f0
int32_t cam_type18_inside(uint32_t cam) {
    const uint32_t override_moby = lwu(OV(0x00166edcu));
    const uint32_t data = lwu(entry_of(cam) + 0x1cu);
    const uint32_t moby = lwu(data + 0x48u);
    if (override_moby != 0 && override_moby == moby) return 1;
    if (lh(data + 0x3cu) == 6) return lwu(HERO_PLAT) == moby ? 1 : 0;
    if (lbu(data + 0x22u) == 1) {
        return vec3_distance(V(HERO_POS), V(moby + 0x10u)) < lwc1(data + 0x24u) ? 1 : 0;
    }
    if (lw(data + 0xcu) >= 0) return point_in_box(HERO_POS, lw(data + 0xcu)) != 0 ? 1 : 0;
    if (lw(data + 0x10u) >= 0) return point_in_cylinder(HERO_POS, lw(data + 0x10u)) != 0 ? 1 : 0;
    if (lw(data + 0x8u) >= 0) return point_in_sphere(HERO_POS, lw(data + 0x8u)) != 0 ? 1 : 0;
    const int32_t poly = lw(data + 0x14u);
    if (poly < 0) return 0;
    const uint32_t p = lwu(OV(0x001b04b0u) + static_cast<uint32_t>(poly) * 4u);
    return point_in_polygon(HERO_POS, p + 0x10u, lw(p)) != 0 ? 1 : 0;
}

// Whether the moby of the data (+0x48) is within `max_pitch_deg` of the active camera's pitch and `max_yaw_deg` of its
// heading (both about the up axis 0x166e40; both 0: always).
// orig: L3:0x002ecae8 L2:0x002fb788 L6:0x00315828 L7:0x003285a8 L8:0x00315788 L12:0x00315968 L18:0x00305830
int32_t cam_type18_cone_check(uint32_t cam, float max_yaw_deg, float max_pitch_deg) {
    const uint32_t data = lwu(entry_of(cam) + 0x1cu);
    const uint32_t moby = lwu(data + 0x48u);
    if (max_yaw_deg == 0.0f && max_pitch_deg == 0.0f) return 1;

    const Vec4& axis = V(OV(0x00166e40u));
    const uint32_t act = lwu(OV(0x00166e00u));
    Vec4 to_moby{}, along_m{}, flat_m{};
    vec3_sub(to_moby, V(moby + 0x10u), V(act + 0x30u));
    const float dot_m = vec3_dot(to_moby, axis);
    vec3_scale(along_m, axis, dot_m);
    vec3_sub(flat_m, to_moby, along_m);
    const float len_m = vec3_length(flat_m);

    const float dot_c = vec3_dot(V(act), axis);
    Vec4 along_c{}, flat_c{};
    vec3_scale(along_c, axis, dot_c);
    vec3_sub(flat_c, V(act), along_c);
    const float len_c = vec3_length(flat_c);

    const float deg = u2f(0x3c8efa35u);
    if (max_pitch_deg != 0.0f) {
        const float pm = atan2_f(len_m, dot_m);
        const float pc = atan2_f(len_c, dot_c);
        if (fpu_mul(max_pitch_deg, deg) < angle_dist(pm, pc)) return 0;
    }
    if (max_yaw_deg == 0.0f) return 1;
    const float prod = fpu_mul(len_c, len_m);
    if (prod == 0.0f) return 0;
    const float ang = abs_f(fpu_sub(u2f(0x3fc90fdbu), asin_f(fpu_div(vec3_dot(flat_m, flat_c), prod))));
    return ang < fpu_mul(max_yaw_deg, deg) ? 1 : 0;
}

// The modifier's frame: its moby found (none or not live: +0x20 -1 and the modifier released), claimed, the hero in its zone;
// the count +0x20 / +0x3e of the frames the turn waits, then the follow camera turned and set.
// orig: L3:0x002ecd28 L2:0x002fb9c8 L6:0x00315a68 L7:0x003287e8 L8:0x003159c8 L12:0x00315ba8 L18:0x00305a70
void cam_type18_update(uint32_t cam) {
    const uint32_t data = lwu(entry_of(cam) + 0x1cu);
    if (lw(data + 0x50u) != 0) {
        sh(data + 0x20u, 0);
        cam_mod_release(cam);
        return;
    }

    uint32_t moby = 0;
    if (lw(data + 0x44u) >= 0) {
        uint32_t list = lwu(OV(0x001ab840u) + static_cast<uint32_t>(lw(data + 0x44u)) * 4u);
        const uint32_t pool = lwu(OV(0x0015ffd8u));
        if (list == 0) {
            sh(data + 0x20u, 0xffffu);
            cam_mod_release(cam);
            return;
        }
        for (;;) {
            const uint16_t raw = lhu(list);
            const uint32_t m = pool + ((raw & 0x7fffu) << 8);
            if (static_cast<int8_t>(lbu(m + 0x20u)) >= 0) {
                moby = m;
                break;
            }
            if (static_cast<int16_t>(raw) < 0) {
                sh(data + 0x20u, 0xffffu);
                cam_mod_release(cam);
                return;
            }
            list += 2u;
        }
    } else {
        moby = lwu(OV(0x0015ffd8u)) + (lwu(data + 0x28u) << 8);
        if (static_cast<int8_t>(lbu(moby + 0x20u)) < 0) {
            sh(data + 0x20u, 0xffffu);
            cam_mod_release(cam);
            return;
        }
    }

    sw(data + 0x48u, moby);
    if (lh(data + 0x3cu) == 6 && lwu(HERO_PLAT) != moby) {
        sh(data + 0x20u, 0);
        cam_mod_release(cam);
        return;
    }
    if (cam_mod_claim(cam) <= 0) {
        sh(data + 0x20u, 0);
        return;
    }
    if (cam_type18_inside(cam) == 0) {
        sh(data + 0x20u, 0);
        cam_mod_release(cam);
        return;
    }

    if (lh(data + 0x3cu) == 5) {
        const uint32_t act = lwu(OV(0x00166e00u));
        const float cur_yaw = atan2_f(lwc1(act + 0x0u), lwc1(act + 0x4u));
        if (lh(data + 0x20u) < 2) {
            swc1(data + 0x40u, lwc1(entry_of(cam) + 0x18u));
        } else if (u2f(0x3e99999au) <= abs_f(lwc1(PAD_RX)) || u2f(0x3e99999au) <= abs_f(lwc1(PAD_RY))) {
            swc1(data + 0x40u, cur_yaw);
        }
        const float saved = lwc1(data + 0x40u);
        if (u2f(0x3fc90fdbu) < angle_dist(saved, cur_yaw)) {
            (void)angle_add(saved, u2f(0x40490fdbu));
        }
    }

    if (lh(data + 0x3cu) == 4) {
        const uint32_t act = lwu(OV(0x00166e00u));
        const uint32_t e = entry_of(cam);
        float yaw = lwc1(e + 0x18u);
        const float cur_yaw = atan2_f(lwc1(act + 0x0u), lwc1(act + 0x4u));
        if (u2f(0x3fc90fdbu) < angle_dist(yaw, cur_yaw)) {
            yaw = angle_add(yaw, u2f(0x40490fdbu));
        }
        swc1(data + 0x40u, yaw);
    }

    bool turn_active = true;
    float speed_scale = 1.0f;
    const int32_t kind0 = lh(data + 0x3cu);
    sh(data + 0x20u, lhu(data + 0x20u) + 1u);

    const int32_t t300 = frame_scale_i(0x12c);
    const int32_t t_mid = frame_scale_i(kind0 == 1 ? 0x190 : 0x15e);
    const int32_t t_end = frame_scale_i(kind0 == 1 ? 0x230 : 0x190);
    const int32_t t200 = frame_scale_i(0xc8);

    if (lh(data + 0x3cu) == 7) {
        if (lwc1(PAD_RX) != 0.0f) sh(data + 0x20u, static_cast<uint32_t>(t300));
        int32_t n = lh(data + 0x20u);
        if (!(n < t300)) {
            turn_active = false;
            sh(data + 0x3eu, lhu(data + 0x3eu) + 1u);
            if (t300 + frame_scale_i(0x1e) < n) {
                speed_scale = u2f(0x3dcccccdu);
                turn_active = true;
            }
            n = lh(data + 0x20u);
        }
        if (!(n < t_end)) {
            turn_active = true;
            sh(data + 0x3eu, 0);
            sh(data + 0x20u, 1u);
        }
        const int32_t n2 = lh(data + 0x20u);
        if (t200 < n2 && n2 < t300) {
            sh(data + 0x3eu, static_cast<uint32_t>(t300));
            sh(data + 0x20u, static_cast<uint32_t>(t200));
        }
    } else if (static_cast<uint16_t>(lhu(data + 0x3cu) - 1u) < 2u) {
        cam0_set_218(0xb0u);
        cam0_set_20c(1.0f, 12.0f, u2f(0x3de147aeu));
        if (lwc1(PAD_RX) != 0.0f || lwc1(PAD_RY) != 0.0f) sh(data + 0x20u, static_cast<uint32_t>(t300));
        int32_t n = lh(data + 0x20u);
        if (!(n < t300)) {
            const uint32_t next3e = lhu(data + 0x3eu) + 1u;
            sh(data + 0x3eu, next3e);
            if (static_cast<int16_t>(next3e) < t_mid) {
                turn_active = false;
            } else if (cam_type18_cone_check(cam, 30.0f, 0.0f) == 0) {
                turn_active = false;
            } else {
                sh(data + 0x20u, static_cast<uint32_t>(t_end));
            }
            n = lh(data + 0x20u);
        }
        if (!(n < t_end)) {
            turn_active = true;
            sh(data + 0x3eu, 0);
            sh(data + 0x20u, 1u);
        }
        const int32_t n2 = lh(data + 0x20u);
        if (t200 < n2 && n2 < t300) {
            sh(data + 0x3eu, static_cast<uint32_t>(t300));
            sh(data + 0x20u, static_cast<uint32_t>(t200));
        }
    } else if (t200 < lh(data + 0x20u)) {
        sh(data + 0x20u, static_cast<uint32_t>(t200));
    }

    const float span = int_to_float(t200);
    bool in_cone = false;
    const float count_f = static_cast<float>(static_cast<int32_t>(lh(data + 0x20u)));
    const float speed = fpu_mul(fpu_mul(fpu_mul(lwc1(data + 0x0u), speed_scale), u2f(0x3c8efa35u)), fpu_div(count_f, span));

    if (turn_active && cam_type18_cone_check(cam, lwc1(data + 0x30u), lwc1(data + 0x2cu)) != 0) {
        in_cone = true;
        const uint16_t k16 = lhu(data + 0x3cu);
        if (static_cast<uint32_t>(k16 - 4u) < 2u) {
            const Vec4 dir = {cos_vu(lwc1(data + 0x40u)), sin_vu(lwc1(data + 0x40u)), 0.0f, 0.0f};
            const uint32_t act = lwu(OV(0x00166e00u));
            if (lh(act + 0x86u) == 0 && speed != 0.0f) {
                swc1(lwu(act + 0x70u) + 0x1bcu, speed);
                cam0_turn_toward(act, dir, 0.0f);
            }
        } else {
            float spd = speed;
            if (static_cast<int16_t>(k16) == 3 && lh(data + 0x20u) < frame_scale_i(0x5a)) {
                const float denom = int_to_float(frame_scale_i(0x5a));
                const float num = static_cast<float>(static_cast<int32_t>(lh(data + 0x20u)));
                spd = fpu_add(fpu_mul(fpu_sub(speed, 0.0f), fpu_div(num, denom)), 0.0f);
            }
            cam0_turn_point(moby + 0x10u, spd, 0.0f);
        }
    }

    if (lwc1(data + 0x34u) != 0.0f) {
        cam0_set_distance(0, lwc1(data + 0x34u), u2f(0x3b449ba6u));
        cam0_set_flag226();
    }
    if (lwc1(data + 0x38u) != 0.0f) {
        cam0_set_height(lwc1(data + 0x38u), u2f(0x3b449ba6u));
    }
    if (lwc1(data + 0x4cu) != 0.0f) {
        cam0_set_look_height(0, lwc1(data + 0x4cu), u2f(0x3ba3d70au));
    }

    if (lh(data + 0x3cu) == 3) {
        cam0_set_mode(0);
        cam0_set_flag226();
        cam0_set_flag116();
        cam0_set_124(u2f(0x3c23d70au), u2f(0x3e4ccccdu));
        float r0 = u2f(0x3cf5c28fu);
        if (lh(data + 0x20u) < frame_scale_i(0x78)) {
            const float denom = int_to_float(frame_scale_i(0x78));
            const float num = static_cast<float>(static_cast<int32_t>(lh(data + 0x20u)));
            r0 = fpu_add(fpu_mul(fpu_div(num, denom), u2f(0x3ca3d70au)), u2f(0x3c23d70au));
        }
        cam0_set_rates(r0, u2f(0x3e4ccccdu));
    }

    if (static_cast<uint32_t>(lhu(data + 0x3cu) - 4u) < 2u) {
        const float rate_dist = u2f(0x3b449ba6u);
        cam0_set_mode(0);
        cam0_set_flag226();
        cam0_set_flag116();
        cam0_set_124(u2f(0x3ca3d70au), u2f(0x3e4ccccdu));
        cam0_set_distance(0, u2f(0x40bae148u), rate_dist);
        if (lh(data + 0x3cu) == 4) {
            Vec4 d{};
            vec3_sub(d, V(moby + 0x10u), V(HERO_POS));
            const float a = atan2_f(d.x, d.y);
            if (in_cone && u2f(0x3ff5be0bu) < angle_dist(a, lwc1(data + 0x40u))) {
                const float base = lwc1(data + 0x34u);
                cam0_set_distance(0, base == 0.0f ? 8.0f : fpu_add(base, u2f(0x40570a3du)), rate_dist);
            }
        }
        cam0_set_218(0xb0u);
    }
}

// The camera's end-of-update callback `fn` queued (0x168ff0, 16 at most; the count 0x15ef4c), unless it is there already.
// orig: L3:0x001e4a88 L2:0x001fc008 L4:0x001dda88 L5:0x00219d88 L6:0x00202d08 L7:0x00212f08 L9:0x0020a488 L10:0x001de788 L11:0x0021c988 L12:0x0020db88 L13:0x001f6b08 L14:0x001fe308 L15:0x001e4808 L16:0x001e9988 L17:0x001e8188 L18:0x001f3e08
void cam_callback_queue(uint32_t fn) {
    const int32_t n = lw(OV(0x0015ef4cu));
    if (n >= 16) return;
    const uint32_t q = OV(0x00168ff0u);
    for (int32_t i = 0; i < n; ++i) {
        if (lwu(q + static_cast<uint32_t>(i) * 4u) == fn) return;
    }
    sw(q + static_cast<uint32_t>(n) * 4u, fn);
    sw(OV(0x0015ef4cu), static_cast<uint32_t>(n + 1));
}

// Camera type 1 (a fixed camera that turns to the hero while he is in its box, data +0xc): Kerwan's and Eudora's.
// The accept: an active camera (+0x74 4), not below another's priority, the hero in the box; the camera already in it,
// or else a quick cut (the follow camera's +0xf3 cleared, the cut kind 2, the rates 0x166f08 / 0x166f14).
// orig: L3:0x002e8870 L4:0x002ed850
int32_t cam_type1_accept(uint32_t cam, uint32_t other) {
    const uint32_t data = lwu(entry_of(cam) + 0x1cu);
    if (lwu(cam + 0x74u) != 4u) return 0;
    if (other != 0 && lh(other + 0x7eu) == 0 && !(lbu(other + 0x7cu) < lbu(cam + 0x7cu))) {
        return 0;
    }
    if (point_in_box(HERO_POS, lw(data + 0xcu)) == 0) return 0;
    if (point_in_box(OV(0x00166dc0u), lw(data + 0xcu)) != 0) return 1;

    const float spd = u2f(0x3c9374bcu);
    const uint32_t act = lwu(OV(0x00166e00u));
    sb(OV(0x00166ef3u), 0);
    sh(act + 0x7eu, 2u);
    swc1(OV(0x00166f08u), spd);
    swc1(OV(0x00166f14u), spd);
    return 1;
}

// The init: the turn's spring, the look height from the data (+0x18), the place and the matrix from the camera's record.
// orig: L3:0x002e8940 (via L3:0x002e8628) L4:0x002ed920 (via L4:0x002ed608)
void cam_type1_init(uint32_t cam) {
    const uint32_t w = lwu(cam + 0x70u);
    swc1(w + 0x10u + 0x4cu, 1.5f);
    sw(w + 0x10u + 0x40u, 0);
    sw(lwu(cam + 0x70u) + 0x80u, 0);

    const uint32_t w0 = lwu(cam + 0x70u);
    swc1(w0 + 0x4u, u2f(0x3ca3d70au));
    swc1(w0 + 0x8u, u2f(0x3dcccccdu));
    sw(w0 + 0xcu, 0);

    const int32_t idx = lh(cam + 0x84u);
    const uint32_t e = lwu(OV(0x0015ef50u)) + static_cast<uint32_t>(idx * 0x20);
    const uint32_t data = lwu(e + 0x1cu);
    if (idx < 0) {
        debug_trap();
    } else {
        swc1(w + 0x10u + 0x4cu, lwc1(data + 0x18u));
    }

    lq_sq(cam + 0x30u, e);
    const Vec4 ang = V(e + 0x10u);
    Mat4 m{};
    euler_to_mat4_vu(m, ang);
    V(cam + 0x00u) = m.r[0];
    V(cam + 0x10u) = m.r[1];
    V(cam + 0x20u) = m.r[2];
    lq_sq(cam + 0x40u, cam + 0x00u);
    sh(cam + 0x7eu, 0);
}

// The update: turned toward the hero's head (his position + the look height) by the spring, the up axis 0x166e30.
// orig: L3:0x002e8960 (via L3:0x002e8730) L4:0x002ed940 (via L4:0x002ed710)
void cam_type1_update(uint32_t cam) {
    const uint32_t w = lwu(cam + 0x70u);
    Vec4 at = V(HERO_POS);
    at.z = fpu_add(at.z, lwc1(w + 0x10u + 0x4cu));

    Vec4 dir{};
    vec3_sub(dir, at, V(cam + 0x30u));
    vec3_set_length(dir, dir, 1.0f);

    const float off = fpu_sub(u2f(0x3fc90fdbu), asin_f(vec3_dot(dir, V(cam))));
    const float turn = camera_angle_spring(0.0f, off, lwc1(w + 0x4u), lwc1(w + 0x8u), lwc1(w + 0xcu), w);
    slerp_vec_toward(V(cam), V(cam + 0x40u), dir, 1, turn);
    vec3_set_length(V(cam + 0x40u), V(cam), 1.0f);
    lq_sq(cam, cam + 0x40u);
    vec3_cross_ba(V(cam + 0x10u), V(cam), V(OV(0x00166e30u)));
    vec3_set_length(V(cam + 0x10u), V(cam + 0x10u), 1.0f);
    vec3_cross_ba(V(cam + 0x20u), V(cam + 0x10u), V(cam));
}

// The exit: handed back (+0x7e 3) when the active camera leaves the hero outside its box.
// orig: L3:0x002e8980 L4:0x002ed960
void cam_type1_exit(uint32_t cam) {
    const uint32_t data = lwu(entry_of(cam) + 0x1cu);
    if (lwu(cam + 0x74u) == 4u && point_in_box(HERO_POS, lw(data + 0xcu)) == 0) {
        sh(cam + 0x7eu, 3u);
    }
}

// orig: L3:0x002ebd70
int32_t ker_cam_type14_accept_l3(uint32_t, uint32_t) {
    const int32_t req = lw(HERO_CAM_REQ);
    const int32_t st = lw(HERO_STATE);
    if ((req == 0x50 && st != 0x11) || st == 6) return -1;
    return 0;
}

// orig: L3:0x002eb978
void ker_cam_type14_init_l3(uint32_t cam) {
    const uint32_t data = lwu(entry_of(cam) + 0x1cu);
    const uint32_t b = lwu(cam + 0x70u) + 0xb0u;
    swc1(b + 0x14u, 1.5f);
    sw(b + 0x10u, lwu(HERO_MOBY));
    lq_sq(b, HERO_POS);
    sw(b + 0xcu, 0);

    const uint32_t w2 = lwu(cam + 0x70u);
    const float dist = lwc1(data + 0x20u);
    swc1(w2 + 0xd8u, dist);
    swc1(w2 + 0xd0u, dist);
    swc1(w2 + 0xe0u, lwc1(data + 0x24u));

    const uint32_t w3 = lwu(cam + 0x70u);
    sw(w3 + 0x28u, 0);
    swc1(w3 + 0x20u, u2f(0x3c23d70au));
    swc1(w3 + 0x24u, u2f(0x3e4ccccdu));
    vec_clear(V(w3 + 0x10u));
    lq_sq(w3, HERO_POS);

    sw(lwu(cam + 0x70u) + 0x30u, static_cast<uint32_t>(frame_scale_i(0x78)));

    const uint32_t f = lwu(cam + 0x70u) + 0xf0u;
    sw(f + 0x30u, 0);
    sw(f + 0x28u, 0);
    sw(f + 0x2cu, 0);
    sw(f + 0x20u, 0);
    swc1(f + 0x34u, lwc1(HERO_POS + 0x8u));
    sw(f + 0x24u, 0);

    const uint32_t e2 = entry_of(cam);
    const Vec4 ang = {lwc1(e2 + 0x10u), lwc1(e2 + 0x14u), lwc1(e2 + 0x18u), 0.0f};
    Mat4 m{};
    euler_to_mat3_vu(m, ang);
    V(f) = m.r[0];

    type14_place_l3(cam);
    sh(cam + 0x7eu, 0);
    sw(L3_ADDR(0x00167074u), 0x78u);
}

// orig: L3:0x002ebdb0 (via L3:0x002ebb00, L3:0x002ebaf0, L3:0x002ebc58)
void ker_cam_type14_update_l3(uint32_t cam) {
    {
        const uint32_t w = lwu(cam + 0x70u);
        const uint32_t d = w + 0xd0u;
        const uint32_t f = w + 0xf0u;
        if (!(lw(HERO_CAM_REQ) == 0x50 && lw(HERO_STATE) != 0x11)) {
            swc1(f + 0x34u, lwc1(HERO_POS + 0x8u));
        }

        Vec4 up_off{}, back_off{};
        vec3_set_length(up_off, V(HERO_AXIS), -lwc1(d + 0x10u));
        vec3_set_length(back_off, V(f), -lwc1(d));

        swc1(w + 0x0u, camera_spring(lwc1(w + 0x0u), lwc1(HERO_POS + 0x0u), lwc1(w + 0x20u), lwc1(w + 0x24u), lwc1(w + 0x28u), w + 0x10u));
        swc1(w + 0x4u, camera_spring(lwc1(w + 0x4u), lwc1(HERO_POS + 0x4u), lwc1(w + 0x20u), lwc1(w + 0x24u), lwc1(w + 0x28u), w + 0x14u));
        swc1(w + 0x8u, camera_spring(lwc1(w + 0x8u), lwc1(f + 0x34u),        lwc1(w + 0x20u), lwc1(w + 0x24u), lwc1(w + 0x28u), w + 0x18u));

        Vec4 pos{};
        vec3_add(pos, V(w), up_off);
        vec3_add(pos, pos, back_off);
        swc1(cam + 0x30u, pos.x);
        swc1(cam + 0x38u, pos.z);
        swc1(cam + 0x34u, pos.y);
    }
    {
        const uint32_t w = lwu(cam + 0x70u);
        const uint32_t b = w + 0xb0u;
        Vec4 target = V(w);
        swc1(b + 0x8u, camera_spring(lwc1(b + 0x8u), lwc1(HERO_POS + 0x8u), lwc1(L3_ADDR(0x00161e70u)), lwc1(L3_ADDR(0x00161e74u)), 0.0f, w + 0xbcu));

        Vec4 off{};
        vec3_set_length(off, V(L3_ADDR(0x00166f40u)), lwc1(b + 0x14u));
        vec3_add(target, off, target);

        Vec4 dir{};
        vec3_sub(dir, target, V(cam + 0x30u));
        vec3_set_length(V(cam), dir, 1.0f);
        lq_sq(cam + 0x40u, cam);
        vec3_cross_ba(V(cam + 0x10u), V(cam), V(L3_ADDR(0x00166f30u)));
        vec3_set_length(V(cam + 0x10u), V(cam + 0x10u), 1.0f);
        vec3_cross_ba(V(cam + 0x20u), V(cam + 0x10u), V(cam));
    }
}

// orig: L3:0x002ebde8
void ker_cam_type14_exit_l3(uint32_t cam) {
    const uint32_t data = lwu(entry_of(cam) + 0x1cu);
    const int32_t box = lw(data + 0xcu);
    if (box >= 0 && point_in_box(HERO_POS, box) == 0) {
        sh(cam + 0x7eu, 3u);
    }
}

// Camera type 18's accept, where the modifier's whole frame runs: while the follow camera is the active one and the
// modifier is not off (+0x20 < 0), the frame; its request +0x50 cleared. Never takes the view (0).
// orig: L3:0x002ed5f8 L2:0x002fc298 L6:0x00316368 L7:0x003290b8 L8:0x00316298 L12:0x00316478 L18:0x00306370
int32_t cam_type18_accept(uint32_t cam, uint32_t) {
    const uint32_t act = lwu(OV(0x00166e00u));
    const uint32_t data = lwu(entry_of(cam) + 0x1cu);
    if (lh(act + 0x86u) == 0 && lh(data + 0x20u) >= 0) {
        cam_type18_update(cam);
    }
    sw(data + 0x50u, 0);
    return 0;
}

// The follow camera's per-frame hook *(0x167010) of the levels that have one (all but Veldin, Novalis and Batalia): a look
// height asked for in the block 0x167000 (+0xc) raises the follow camera's look (0.35 of it), and the level's callback is
// queued to count it down.
// orig: L3:0x002eb408 L2:0x002fa710 L4:0x002f0448 L5:0x00328f80 L6:0x003147b0 L7:0x00328090 L9:0x00314220 L10:0x002f4b18 L11:0x00327a90 L12:0x003148f0 L13:0x00316b48 L14:0x003131e0 L15:0x002f6980 L16:0x002f3a88 L17:0x002fff38 L18:0x00305318
void cam0_level_hook(uint32_t) {
    const float f = lwc1(OV(0x0016700cu));
    if (f == 0.0f) return;
    cam0_set_look_height(1, fpu_add(fpu_mul(f, u2f(0x3eb33333u)), 0.0f), u2f(0x3ba3d70au));
    cam_callback_queue(level_callback_addr());
}

// The hook's end-of-update callback: the block 0x167000's +8 set to 10000; its flag +4 cleared when set, else the look height
// +0xc counted down by the level's step (not below 0); +0 cleared.
// orig: L3:0x002eb468 L2:0x002fa770 L4:0x002f04a8 L5:0x00328fe0 L6:0x00314810 L7:0x003280f0 L9:0x00314280 L10:0x002f4b78 L11:0x00327af0 L12:0x00314950 L13:0x00316ba8 L14:0x00313240 L15:0x002f69e0 L16:0x002f3ae8 L17:0x002fff98 L18:0x00305378
void cam0_level_callback(uint32_t) {
    const uint32_t b = OV(0x00167000u);
    const uint32_t flag = lwu(OV(0x00167004u));
    swc1(OV(0x00167008u), 10000.0f);
    if (flag != 0) {
        sw(b + 0x4u, 0);
    } else {
        const float next = fpu_sub(lwc1(OV(0x0016700cu)), lwc1(level_look_step()));
        swc1(OV(0x0016700cu), next < 0.0f ? 0.0f : next);
    }
    sw(b, 0);
}

}  // namespace rac1
