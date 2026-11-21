// Ported overlay code: the Visibomb Gun (item 0x0d, hand class 0xad, update 0x2b3058),
// its HUD / fog / orbit-camera helpers, and rocket spawn (class 0xac).
#include <game/ovl/visibomb.h>

#include <game/enhance/advanced.h>
#include <game/enhance/fixes102.h>
#include <game/math/vecmath.h>
#include <game/ovl/camera_types.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350u;
constexpr uint32_t PAD = 0x0013c940u;
constexpr uint32_t STATS = 0x00141680u;

void cam6_snap_v(uint32_t cam, uint32_t ang, const Vec4& center, float yaw) {
    const uint32_t w = lwu(cam + 0x70);
    const Vec4 c = center;
    V(w + 0xb0) = c;
    V(w + 0x80) = c;
    swc1(w + 0xc8, yaw);
    cam_angles_copy_turned(w + 0x14, ang, yaw);
    cam_angles_copy_turned(cam + 0x50, ang, 0.0f);
    cam_springs_clear(w);
    vec3_clear_xyz(w + 0x90);
    cam6_place(cam);
}

int32_t visibomb_spawn_v(uint32_t hand, const Vec4& pos, float yaw, float pitch) {
    const uint32_t ball = moby_spawn(0xacu);
    if (ball == 0) return 0;

    const uint32_t pv = lwu(ball + 0x78);
    sw(P + 0x1fe0, ball);
    sw(OV(0x0017e508u), 1u);
    sb(ball + 0x30, 0xffu);
    sh(ball + 0x32, 0xffu);
    sb(ball + 0x20, 1u);
    swc1(ball + 0x48, yaw);
    swc1(ball + 0x44, pitch);
    sb(ball + 0x31, 1u);
    sw(pv + 0x24, P + 0x1f60);
    sw(pv + 0x28, P + 0x1fa0);
    sw(pv + 0x0, hand);
    V(ball + 0x10) = pos;
    sw(pv + 0x8, 0u);
    sw(pv + 0xc, 0u);

    const uint32_t ride = lwu(P + 0x2fc);
    if (ride != 0 && moby_pvar_word2(ride) != 0) {
        swc1(pv + 0x4, vec3_length(V(P + 0x100)));
    } else {
        sw(pv + 0x4, 0u);
    }

    const int32_t life = frame_scale_i(0xbb8);
    sw(pv + 0x18, 0u);
    sw(pv + 0x1c, 0u);
    sw(pv + 0x14, 0u);
    swc1(pv + 0x44, u2f(0x3ecccccdu));
    sw(pv + 0x10, static_cast<uint32_t>(life));

    visibomb_2b56d0(ball);
    moby_update_transform(ball);
    sw(pv + 0x14, static_cast<uint32_t>(visibomb_2eb930(ball)));
    hero_set_state(0x1du, 1);

    mem_fill32(lwu(pv + 0x24), 0u, 0x40);
    mem_fill32(lwu(pv + 0x28), 0u, 0x40);
    sub_0024f590(ball, 0u, lwu(pv + 0x24));
    sub_0024f590(ball, 1u, lwu(pv + 0x28));

    const uint32_t nozzle = pyrocitor_nozzle_spawn(ball);
    const float gs = lwc1(0x0015ed6cu);
    const float base_spd = lwc1(OV(0x0016149cu));
    sw(pv + 0x20, nozzle);
    swc1(pv + 0x48, fpu_mul(base_spd, gs));

    moby_copy_hero_color(ball);
    sw(pv + 0x2c, static_cast<uint32_t>(moby_sound_start(0, 4u, ball)));

    Vec4 seg_start = V(hand + 0x10);
    seg_start.z = pos.z;
    if (segment_collide_v(seg_start, pos, 0u, lwu(P + 0x2080), nullptr) != 0) {
        sb(ball + 0xbc, 1u);
        V(ball + 0x10) = V(OV(0x00173e60u));
    }

    sw(OV(0x0015f608u), 1u);
    hud_message_force(6, 0);
    return static_cast<int32_t>(ball);
}

void visibomb_try_fire(uint32_t m, uint32_t pv) {
    if ((lwu(PAD + 0x1a4) & lwu(P + 0x10a0)) == 0) return;
    if (lw(pv + 0x0) != 0) return;
    if (lw(pv + 0x4) != 0) return;
    if (lbu(P + 0x20ac) != 0) return;

    const int32_t gstate = lw(P + 0x208c);
    if (gstate == 7 || gstate == 2 || gstate == 3 || gstate == 4 || gstate == 5 || gstate == 6 ||
        gstate == 0x10 || gstate == 0xe || gstate == 9 || gstate == 0xa || gstate == 0xb || gstate >= 13) {
        return;
    }

    const int32_t hstate = lw(P + 0x2084);
    if (hstate == 0x72 || hstate == 0x65) return;
    if (gameplay102_on(8) && static_cast<uint32_t>(hstate) == 0x32u) return;  // enhancement: 1.02 Gameplay, fix 8 (fixes102.h)

    const uint32_t cam_g = OV(0x00166c80u);
    const uint32_t active_cam = lwu(cam_g + 0x180);
    if (active_cam != 0 && lh(active_cam + 0x86) == 5) return;
    if (lwu(P + 0x300) == 0) return;

    const uint32_t ride = lwu(P + 0x2fc);
    if (ride != 0) {
        const int16_t rcls = lh(ride + 0xa6);
        if (rcls == 0x13e || rcls == 0x46f) return;
    }

    if (sub_00233db8(-1, 1) == 0) {
        if ((lwu(PAD + 0x1a4) & lwu(P + 0x10a0)) != 0) {
            moby_sound_start(0, 0u, m);
        }
        return;
    }

    float pitch = 0.0f;
    float yaw = lwc1(P + 0x688);
    Vec4 spawn_pos{};

    if (lw(P + 0x2084) == 1 && (lhu(lwu(P + 0x2080) + 0x34) & 1u) != 0) {
        yaw = atan2_f(lwc1(cam_g + 0x350), lwc1(cam_g + 0x354));
        const float xy_len = vec2_length(V(cam_g + 0x350));
        pitch = -atan2_f(xy_len, lwc1(cam_g + 0x358));
        vec3_set_length(spawn_pos, V(cam_g + 0x350), u2f(0x3e99999au));
        Vec4 down_off{};
        vec3_set_length(down_off, V(cam_g + 0x370), u2f(0xbeb33333u));
        vec3_add(spawn_pos, spawn_pos, down_off);
        vec3_add(spawn_pos, spawn_pos, V(cam_g + 0x140));
    } else if (lh(P + 0x22c8) == 1) {
        const float xy_len = vec2_length(V(m + 0xd0));
        pitch = atan2_f(xy_len, lwc1(m + 0xd8));
        vec_from_spherical_v(spawn_pos, u2f(0x3f19999au), yaw, -pitch);
        vec3_add(spawn_pos, spawn_pos, V(m + 0x10));
    } else {
        float launch_pitch = -pitch;
        if (advanced_aim_angles(m + 0x10, yaw, pitch)) launch_pitch = -pitch;  // enhancement: modern aim yaw / pitch (advanced.h)
        vec_from_spherical_v(spawn_pos, u2f(0x3f19999au), yaw, launch_pitch);
        vec3_add(spawn_pos, spawn_pos, V(m + 0x10));
    }

    if (lbu(m + 0x53) != 3u) {
        moby_anim_set(m, 3u, 0, 0);
    }

    visibomb_203b18();
    sw(pv + 0x4, static_cast<uint32_t>(visibomb_spawn_v(m, spawn_pos, yaw, pitch)));

    const uint16_t shots = lhu(STATS + 0x68);
    if (shots <= 0xfffeu) {
        sh(STATS + 0x68, shots + 1u);
    }
    const int32_t mins = ee_div(frame_scale_i(lw(0x0015eea4u)), 0x258);
    if (static_cast<int32_t>(lhu(STATS + 0x6a)) < mins) {
        const int32_t mins2 = ee_div(frame_scale_i(lw(0x0015eea4u)), 0x258);
        sh(STATS + 0x6a, static_cast<uint32_t>(mins2));
    }
    const uint32_t level = lwu(0x0015ed84u);
    sw(STATS + 0x6c, lwu(STATS + 0x6c) | (1u << (level & 31u)) | 0x80000000u);
    sw(pv + 0x0, static_cast<uint32_t>(frame_scale_i(0x3c)));
}
}  // namespace

// orig: 0x00203880
void visibomb_203880() {
    const uint32_t hud = OV(0x00179410u);
    const uint32_t state = lwu(hud);
    if (state >= 8u) return;

    // (the state's case by its index: the original's switch over the state, compiled to the jump table 0x1e7b80; the
    // disc's table is not read, so the states work on a build whose own code moved the case labels)
    switch (state) {
    case 0:  // (0x002038b0)
        sw(hud + 0x24, 0xffffffffu);
        break;
    case 1: case 2:  // (0x002038c0)
    case 3:          // (0x002038c8)
        sw(hud, 7u);
        sw(hud + 0x4, 0u);
        break;
    case 4: {  // (0x002038d0)
        const uint32_t t = lwu(hud + 0x4);
        sw(hud, 6u);
        sw(hud + 0x4, 4u - t);
        break;
    }
    case 5:  // (0x002038f0)
        sw(hud, 6u);
        sw(hud + 0x4, 0u);
        break;
    case 6: case 7:  // (0x00203900)
        break;
    }
}

// orig: 0x00203b18
void visibomb_203b18() {
    const uint32_t hud = OV(0x00179410u);
    if (lh(hud + 0x38) != 0) return;

    sh(hud + 0x38, 1u);
    sw(hud + 0x3c, static_cast<uint32_t>(frame_scale_i(0x3c)));
    visibomb_203880();
    if (lw(OV(0x00179410u)) == 0) {
        sw(OV(0x00179410u), 8u);
    }
}

// orig: 0x002b56d0
void visibomb_2b56d0(uint32_t p0) {
    const uint32_t f0 = lwu(OV(0x0015f448u));
    const uint32_t f1 = lwu(OV(0x0015f44cu));
    const uint32_t pv = lwu(p0 + 0x78);
    const uint32_t f2 = lwu(OV(0x0015f450u));
    const uint32_t f3 = lwu(OV(0x0015f454u));

    sw(OV(0x0015f30cu), 1u);
    sw(pv + 0x30, f0);
    sw(pv + 0x34, f1);
    sw(pv + 0x38, f2);
    sw(pv + 0x3c, f3);
    sb(pv + 0x40, lbu(OV(0x0015f444u)));
    sb(pv + 0x41, lbu(OV(0x0015f445u)));
    sb(pv + 0x42, lbu(OV(0x0015f446u)));

    sb(OV(0x0015f445u), 0x60u);
    sb(OV(0x0015f446u), 0x40u);
    swc1(OV(0x0015f44cu), u2f(0x48000000u));
    swc1(OV(0x0015f450u), u2f(0x437f0000u));
    sw(OV(0x00169ff8u), 0u);
    swc1(OV(0x00160f80u), u2f(0x48100000u));
    swc1(OV(0x001604a4u), u2f(0x43100000u));
    sw(OV(0x0015fff0u), 0x90u);
    sw(OV(0x0016017cu), 0x50000u);
    sb(OV(0x0015f444u), 0x40u);
    swc1(OV(0x00160fe0u), u2f(0x43100000u));
    sw(OV(0x0015f448u), 0u);
    sw(OV(0x0015f454u), 0u);
    bg_color_set(0x40u, 0x60u, 0x40u);
}

// orig: 0x002eb320
void visibomb_2eb320(uint32_t p0, uint32_t p1, uint32_t p2, float pf12) {
    cam6_snap_v(p0, p1, V(p2), pf12);
}

// orig: 0x002eb648
void visibomb_2eb648(uint32_t p0, uint32_t p1) {
    const uint32_t w = lwu(p0 + 0x70);
    const uint32_t ang = w + 0x64;
    if (p1 == 0) return;

    Vec4 off{lwc1(OV(0x00161dd4u)), 0.0f, u2f(0x3df5c28fu), 0.0f};
    const uint32_t pv = lwu(p1 + 0x78);
    mat3_transform(off, off, M(p1 + 0xc0));

    Vec4 center{};
    vec3_add(center, V(p1 + 0x10), off);

    if (lw(pv + 0x10) == -1) {
        cam_angles_copy_turned(ang, p0 + 0x50, 0.0f);
        const float old_c = lwc1(ang + 0xc);
        swc1(ang + 0xc, 0.0f);
        swc1(ang + 0x8, 6.0f);
        swc1(ang + 0x4, old_c);
        cam6_snap_v(p0, ang, center, 0.0f);
    }

    if (lw(pv + 0x10) > 0) {
        const float roll = lwc1(p1 + 0x40);
        const float cur_pitch = lwc1(w + 0x20);
        swc1(w + 0xac, fpu_mul(roll, 0.5f));
        const float dp = fpu_mul(fpu_sub(lwc1(p1 + 0x44), cur_pitch), u2f(0x3d4ccccdu));
        swc1(w + 0x20, fpu_add(cur_pitch, dp));
        V(w + 0x80) = center;
        V(w + 0xc0) = V(p1 + 0x40);
        return;
    }

    swc1(w + 0xac, angle_add(lwc1(w + 0xac), lwc1(pv + 0x18)));
    const uint32_t cam_g = OV(0x00166c80u);
    swc1(pv + 0x18, fpu_mul(lwc1(pv + 0x18), u2f(0x3f7851ecu)));

    const float dx = fpu_sub(lwc1(cam_g + 0x140), lwc1(P + 0x80));
    const float dy = fpu_sub(lwc1(cam_g + 0x144), lwc1(P + 0x84));
    const float yaw_to = atan2_f(dx, dy);
    const float adist = angle_dist(yaw_to, lwc1(cam_g + 0x158));

    const float d_hero = vec2_distance(V(P + 0x80), V(cam_g + 0x140));
    const float d_hand = vec2_distance(V(lwu(P + 0x1090) + 0x10), V(cam_g + 0x140));
    const uint32_t near_pos = (d_hero < d_hand) ? (P + 0x80) : (lwu(P + 0x1090) + 0x10);
    const float min_d = vec2_distance(V(near_pos), V(cam_g + 0x140));

    if (min_d < u2f(0x3f451eb8u)) {
        const float cur_r = lwc1(w + 0x1c);
        const float delta = fpu_sub(u2f(0x3f333333u), min_d);
        if (adist < u2f(0x3fc90fdbu)) {
            swc1(w + 0x1c, fpu_sub(cur_r, delta));
            sw(pv + 0x4, 0u);
        } else {
            swc1(w + 0x1c, fpu_add(cur_r, delta));
        }
    } else {
        const float pv4 = lwc1(pv + 0x4);
        const float cur_r = lwc1(w + 0x1c);
        swc1(w + 0x1c, fpu_add(cur_r, pv4));
        swc1(pv + 0x4, fpu_mul(lwc1(pv + 0x4), u2f(0x3f7851ecu)));
    }

    V(w + 0x80) = center;
}

// orig: 0x002eb930
int32_t visibomb_2eb930(uint32_t p0) {
    const uint32_t cam = moby_table_find_by_uid(6);
    sh(OV(0x00166ef0u), 0u);
    camera_switch(cam);
    visibomb_2eb648(cam, p0);

    const uint32_t w = lwu(cam + 0x70);
    V(w + 0xb0) = V(w + 0x80);
    cam_angles_copy_turned(w + 0x14, OV(0x001e77c0u), 0.0f);
    cam_angles_copy_turned(cam + 0x50, w + 0x14, 0.0f);
    cam6_reset(cam);
    sw(w + 0x9c, 1u);
    cam_angles_copy_turned(w + 0x28, OV(0x001e77d8u), 0.0f);
    cam_angles_copy_turned(w + 0x3c, OV(0x001e77f0u), 0.0f);
    cam_springs_clear(w + 0x50);
    return static_cast<int32_t>(cam);
}

// orig: 0x002b58d8
int32_t visibomb_2b58d8(uint32_t p0, uint32_t p1, float pf12, float pf13) {
    return visibomb_spawn_v(p0, V(p1), pf12, pf13);
}

// orig: 0x002b3058
void visibomb_update(uint32_t m) {
    if (m == 0) return;

    const uint32_t pv = lwu(m + 0x78);
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv)));

    visibomb_try_fire(m, pv);

    if ((lbu(m + 0x70) & 2u) != 0 && lbu(m + 0x53) != 1u) {
        moby_anim_set(m, 1u, 0, 0);
    }
    sh(m + 0x34, lhu(m + 0x34) | 4u);
}

// orig: (none) -- the ports for the engine's indirect calls (a moby class's update, a draw callback)
void visibomb_fns_register() {
    ovl_register_moby_fn(0x002b3058, visibomb_update);
}

}  // namespace rac1
