// Pokitaru's (level 11) fighter jet (class 1242) and its helpers, ported by hand from the level's own code
// (L11:0x313290, 0x3125a8, 0x309378, 0x3126f8, 0x313f60, 0x312838, 0x31a758, 0x31a7d8): its prompt and skill point
// clock on the pad (carrying 1.02 Gameplay, fix 10, always on the PAL 2.00 disc), its flight, camera and weapons
// (carrying 1.02 Fixes, fix 36, always on the PAL 2.00 disc), and the bomber waves of class 1319.
// The original's order of calls and float operations kept.
#include <game/ovl/pokitaru_own.h>

#include <game/collision/collision.h>
#include <game/enhance/fixes102.h>
#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ammo_pickup.h>
#include <game/ovl/arrival.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/hydro.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/region_core.h>
#include <game/ovl/shared_hud_shapes.h>
#include <game/ovl/shared_moby_helpers.h>
#include <game/ovl/shared_ship_exhaust.h>
#include <game/ovl/shared_ship_shots.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t kPlayer = 0x0013f350u;
constexpr uint32_t kHeroPos = 0x0013f3d0u;
float dt() { return lwc1(0x0015ed6cu); }
float dt2() { return lwc1(0x0015ed70u); }
uint32_t moby_at(uint32_t index) { return lwu(L11_ADDR(0x0015ffd8u)) + (index << 8); }

void stop_loop_sound(uint32_t m, uint32_t half_addr) {
    const int32_t slot = lh(half_addr);
    if (slot != -1) {
        const uint32_t ch = 0x0013e550u + static_cast<uint32_t>(slot * 0x70);
        if (lwu(ch + 0x88) == m && lbu(ch + 0x74) != 0) sub_0028d918(slot);
    }
    sh(half_addr, 0xffffu);
}
}  // namespace

// Fires the jet's twin machine guns (class 1009, alternating chain points 3 and 4) while X / Circle-mask 0x84 is held,
// and projects its aim crosshair onto the HUD (pvar +0xe0 / +0xe4).
// orig: L11:0x003125a8
void pok_3125a8_l11(uint32_t m, uint32_t pv, float yaw, float pitch) {
    const uint32_t sp = ee_stack_alloc(0x90);
    const float range = fpu_mul(lwc1(L11_ADDR(0x0016205cu)), lwc1(L11_ADDR(0x00162090u)));
    vec_from_spherical_v(V(sp), range, yaw, pitch);
    vec3_add(V(sp), V(sp), V(pv));
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x80))) != 0 && (lwu(0x0013caf0u) & 0x84u) != 0) {
        vec3_set_length(V(sp + 0x20), V(sp), u2f(0x400ccccdu));
        moby_chain_point_v(m, (lbu(pv + 0x61) & 1u) + 3u, V(sp + 0x10));
        vec3_add(V(sp + 0x10), V(sp + 0x10), V(sp + 0x20));
        class1009_launch(m, sp, sp + 0x10, 200.0f, -1.0f);
        const uint8_t alt = lbu(pv + 0x61) ^ 1u;
        sw(pv + 0x80, lwu(L11_ADDR(0x00162060u)));
        sb(pv + 0x61, alt);
        moby_sound_start(3, 0, m);
    }
    vec3_set_length(V(sp), V(sp), 23.0f);
    vec3_add(V(sp + 0x30), V(sp), V(m + 0x10));
    hud_screen_point(sp + 0x30, pv + 0xe0, pv + 0xe4, 0);
    ee_stack_free(0x90);
}

// Spawns a homing missile (class 0x40a = 1034) from jet `m` at `pos` toward `target`.
// orig: L11:0x00309378
uint32_t pok_309378_l11(uint32_t m, uint32_t pos, uint32_t target, uint32_t rot, uint32_t life, float speed) {
    const uint32_t s = moby_spawn(0x40a);
    if (s != 0) {
        const uint32_t spv = lwu(s + 0x78);
        sb(s + 0x30, 0xff);
        sh(s + 0x32, 0xff);
        sb(s + 0x31, 1);
        sb(s + 0x20, 0);
        sw(spv + 0x20, m);
        sw(spv + 0x24, target);
        lq_sq(s + 0x10, pos);
        lq_sq(s + 0x40, rot);
        if (target != 0) {
            const uint32_t pv0 = moby_pvar_word0(target);
            if (pv0 != 0) sh(pv0 + 0x1e, lhu(pv0 + 0x1e) | 0x80u);
        }
        sw(spv + 0x28, life);
        swc1(spv + 0x2c, speed);
        sw(spv + 0x30, 0);
        sw(spv + 0x34, 0);
        vec_from_spherical_v(V(spv), speed, lwc1(rot + 8), -lwc1(rot + 4));
        if (lbu(s + 0x53) != 1) moby_anim_set(s, 1, 0, 0xa);
        vec3_add(V(s + 0x10), V(s + 0x10), V(spv));
        const uint32_t locked = lwu(spv + 0x24);
        if (locked != 0) lq_sq(spv + 0x10, locked + 0x10);
        const float ratio = fpu_div(lwc1(m + 0x2c), lwc1(lwu(m + 0x24) + 0x24));
        swc1(s + 0x2c, fpu_mul(lwc1(lwu(s + 0x24) + 0x24), ratio));
        moby_update_transform(s);
    }
    return s;
}

// Fires a homing missile from jet `m` (cycling chain points 7 .. 10) while Circle / R1 (0x28) is held and ammo (0x140946)
// remains.
// orig: L11:0x003126f8
void pok_3126f8_l11(uint32_t m, uint32_t pv) {
    const uint32_t sp = ee_stack_alloc(0x90);
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x84))) != 0 && (lwu(0x0013caf0u) & 0x28u) != 0 &&
        lbu(0x00140946u) != 0) {
        const uint32_t pod = (lhu(pv + 0x6a) + 1u) & 3u;
        sh(pv + 0x6a, pod);
        moby_chain_point_v(m, pod + 7u, V(sp));
        vec3_set_length(V(sp + 0x10), V(pv), u2f(0x3e99999au));
        vec3_add(V(sp), V(sp), V(sp + 0x10));
        uint32_t target = 0;
        const uint32_t cand = lwu(pv + 0x88);
        if (cand != 0 && !(500 < lw(pv + 0x8c))) target = cand;
        const float speed = fpu_mul(lwc1(L11_ADDR(0x00162064u)), lwc1(L11_ADDR(0x00162090u)));
        const uint32_t life = static_cast<uint32_t>(frame_scale_i(0x12c));
        if (pok_309378_l11(m, sp, target, m + 0x40, life, speed) != 0) {
            moby_sound_start(2, 0, m);
            sb(0x00140946u, static_cast<uint8_t>(lbu(0x00140946u) - 1u));
            sw(pv + 0x84, lwu(L11_ADDR(0x00162068u)));
        }
    }
    ee_stack_free(0x90);
}

// Blends `steer_x` / `steer_y` back inside the jet's flight cylinder and height band (box pvar +0x104, floor 250),
// setting bit 1 of 0x14095f (the HUD boundary warning) whenever it overrides the player's steering.
// orig: L11:0x00313f60
void pok_313f60_l11(uint32_t m, uint32_t steer_x, uint32_t steer_y) {
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t pv = lwu(m + 0x78);
    const float look = fpu_mul(lwc1(pv + 0x64), int_to_float(frame_scale_i(0x3c)));
    vec3_scale(V(sp), V(pv + 0x10), look);
    vec3_add(V(sp), V(sp), V(m + 0x10));
    const uint32_t b0 = lvd::box(lvd::BOXES, lw(pv + 0x104));
    const float z = lwc1(sp + 8);
    vec3_sub(V(sp), V(sp), V(b0 + 0x30));
    const float dist_xy = vec2_length(V(sp));
    const float radius = vec2_length(V(lvd::box(lvd::BOXES, lw(pv + 0x104))));
    const uint32_t b2 = lvd::box(lvd::BOXES, lw(pv + 0x104));
    const float ceil_z = fpu_add(lwc1(b2 + 0x28), lwc1(b2 + 0x38));
    const float inward = angle_add(atan2_f(lwc1(sp), lwc1(sp + 4)), u2f(0x40490fd0u));

    if (fpu_sub(radius, 10.0f) < dist_xy) {
        float w = fpu_div(fpu_add(fpu_sub(dist_xy, radius), 10.0f), 10.0f);
        const float turn = angle_sub(inward, lwc1(pv + 0x38));
        if (1.0f < w) w = 1.0f;
        else if (w < 0.0f) w = 0.0f;
        const float cur = lwc1(steer_x);
        const float goal = 0.0f < turn ? -1.0f : 1.0f;
        swc1(steer_x, fpu_add(fpu_mul(fpu_sub(goal, cur), w), cur));
        sb(0x0014095fu, lbu(0x0014095fu) | 2u);
    }

    const float sixth_pi = u2f(0x3f060a92u);
    if (fpu_sub(ceil_z, 10.0f) < z) {
        float wz = fpu_div(fpu_add(fpu_sub(z, ceil_z), 10.0f), 10.0f);
        float wp = fpu_div(fpu_sub(sixth_pi, lwc1(pv + 0x34)), sixth_pi);
        if (1.0f < wz) wz = 1.0f;
        else if (wz < 0.0f) wz = 0.0f;
        if (1.0f < wp) wp = 1.0f;
        else if (wp < 0.0f) wp = 0.0f;
        const float w = fpu_mul(wz, wp);
        const float cur = lwc1(steer_y);
        swc1(steer_y, fpu_add(fpu_mul(fpu_sub(-1.0f, cur), w), cur));
        sb(0x0014095fu, lbu(0x0014095fu) | 2u);
    } else if (z < 260.0f) {
        float wp = fpu_div(fpu_add(lwc1(pv + 0x34), sixth_pi), sixth_pi);
        float wz = fpu_div(fpu_add(fpu_sub(250.0f, z), 10.0f), 10.0f);
        if (1.0f < wz) wz = 1.0f;
        else if (wz < 0.0f) wz = 0.0f;
        if (1.0f < wp) wp = 1.0f;
        else if (wp < 0.0f) wp = 0.0f;
        const float cur = lwc1(steer_y);
        const float w = fpu_mul(wz, wp);
        swc1(steer_y, fpu_add(fpu_mul(fpu_sub(1.0f, cur), w), cur));
        sb(0x0014095fu, lbu(0x0014095fu) | 2u);
    }
    ee_stack_free(0x80);
}

// Steps the fighter jet `m` in flight (states 4 and 8): throttle and boost, stick steering and flight bounds, chase
// camera and FOV, world and moby collisions, skill point 0x55, the HUD callback 0x311d50 and the guns and missiles.
// orig: L11:0x00312838
void pok_312838_l11(uint32_t m, uint32_t pv) {
    const uint32_t sp = ee_stack_alloc(0x1c0);
    const float d0 = dt();
    eer::sq(sp + 0x40, eer::Q128{});
    sh(m + 0x34, lhu(m + 0x34) & 0xfeffu);
    swc1(sp + 0x48, fpu_mul(fpu_mul(d0, 8.0f), lwc1(L11_ADDR(0x00162090u))));
    lq_sq(sp + 0x30, sp + 0x40);
    eer::sq(sp + 0x40, eer::Q128{});
    swc1(sp + 0x48, 1.0f);
    if (!fixes102_pokitaru_jet_camera() && (lwu(0x0013caf4u) & 0x8000u) != 0) {  // (1.02 Fixes, fix 36: PAL 2.00 0x313d78)
        const uint8_t next = static_cast<uint8_t>(lbu(pv + 0x60) + 1u);
        sb(pv + 0x60, next < 2u ? next : 0u);
    }

    if (lw(L11_ADDR(0x00161ff0u)) < lh(pv + 0x68)) {
        if ((lwu(0x0013caf0u) & 0x40u) != 0) {
            if (!(0xfffeu < lhu(0x00141928u))) sh(0x00141928u, lhu(0x00141928u) + 1u);
            const int32_t now = ee_div(frame_scale_i(lw(0x0015eea4u)), 0x258);
            if (static_cast<int32_t>(lhu(0x0014192au)) < now)
                sh(0x0014192au, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4u)), 0x258)));
            const uint32_t bit = 1u << (lwu(0x0015ed84u) & 31u);
            sw(0x0014192cu, (lwu(0x0014192cu) | bit) | 0x80000000u);
            const float scale = lwc1(L11_ADDR(0x00162090u));
            const float add = fpu_mul(lwc1(L11_ADDR(0x00161ffcu)), scale);
            const float vmax = fpu_mul(lwc1(L11_ADDR(0x00161ff8u)), scale);
            const float spd = fpu_add(lwc1(pv + 0x64), add);
            swc1(pv + 0x64, vmax < spd ? vmax : spd);
        } else {
            const float scale = lwc1(L11_ADDR(0x00162090u));
            const float cruise = fpu_mul(lwc1(L11_ADDR(0x00161ff4u)), scale);
            const float cur = lwc1(pv + 0x64);
            const float accel = fpu_mul(lwc1(L11_ADDR(0x00161fecu)), scale);
            if (cur < fpu_sub(cruise, accel)) {
                swc1(pv + 0x64, fpu_add(cur, accel));
            } else if (cruise < cur) {
                const float k = lwc1(L11_ADDR(0x00162000u));
                swc1(pv + 0x64, fpu_add(cur, fpu_mul(fpu_sub(cruise, cur), k)));
            } else {
                swc1(pv + 0x64, cruise);
            }
        }
    } else {
        const uint32_t next_t = lhu(pv + 0x68) + 1u;
        const float add = fpu_mul(lwc1(L11_ADDR(0x00161fe8u)), lwc1(L11_ADDR(0x00162090u)));
        sh(pv + 0x68, next_t);
        swc1(pv + 0x64, fpu_add(lwc1(pv + 0x64), add));
    }

    if (lw(L11_ADDR(0x00162004u)) < lh(pv + 0x68)) {
        const float sy_raw = lwc1(0x00141074u);
        const float sx_raw = lwc1(0x00141070u);
        float damp_x = lwc1(L11_ADDR(0x001620a4u));
        float damp_y = lwc1(L11_ADDR(0x001620b4u));
        swc1(sp + 0x110, sx_raw);
        swc1(sp + 0x114, sy_raw);
        if ((lwu(0x0013cae0u) & 0x200u) != 0) {
            const float qx = fpu_div(sx_raw, lwc1(L11_ADDR(0x00162018u)));
            const float qy = fpu_div(sy_raw, lwc1(L11_ADDR(0x00162014u)));
            damp_x = lwc1(L11_ADDR(0x001620a8u));
            damp_y = lwc1(L11_ADDR(0x001620b8u));
            swc1(sp + 0x110, fpu_mul(qx, lwc1(L11_ADDR(0x00162020u))));
            swc1(sp + 0x114, fpu_mul(qy, lwc1(L11_ADDR(0x0016201cu))));
        }
        pok_313f60_l11(m, sp + 0x110, sp + 0x114);
        spring_to(pv + 0x6c, pv + 0x90, lwc1(sp + 0x110), lwc1(L11_ADDR(0x0016209cu)), lwc1(L11_ADDR(0x001620a0u)), damp_x);
        spring_to(pv + 0x70, pv + 0x94, lwc1(sp + 0x114), lwc1(L11_ADDR(0x001620acu)), lwc1(L11_ADDR(0x001620b0u)), damp_y);
        spring_angle(pv + 0x30, pv + 0x98, 0, fpu_mul(lwc1(pv + 0x6c), lwc1(L11_ADDR(0x0016207cu))),
                     lwc1(L11_ADDR(0x001620bcu)), lwc1(L11_ADDR(0x001620c0u)), lwc1(L11_ADDR(0x001620c4u)));
        float pitch = angle_sub(lwc1(pv + 0x34), fpu_mul(lwc1(pv + 0x70), lwc1(L11_ADDR(0x00162014u))));
        const float max_pitch = u2f(0x3fb2b8c2u);
        const float min_pitch = u2f(0xbfb2b8c2u);
        swc1(pv + 0x34, pitch);
        if (max_pitch < pitch) swc1(pv + 0x34, max_pitch);
        else if (pitch < min_pitch) swc1(pv + 0x34, min_pitch);
        swc1(pv + 0x38, angle_sub(lwc1(pv + 0x38), fpu_mul(lwc1(pv + 0x6c), lwc1(L11_ADDR(0x00162018u)))));
    }

    vec_from_spherical_v(V(sp + 0x50), lwc1(pv + 0x64), lwc1(pv + 0x38), -lwc1(pv + 0x34));
    vec3_set_length(V(pv + 0x10), V(sp + 0x50), 1.0f);
    lq_sq(sp + 0x20, m + 0x10);
    vec3_add(V(m + 0x10), V(m + 0x10), V(sp + 0x50));

    const float base_spd = lwc1(L11_ADDR(0x00161ff4u));
    const float diff_spd = fpu_sub(lwc1(pv + 0x64), base_spd);
    const float span_spd = fpu_sub(lwc1(L11_ADDR(0x00161ff8u)), base_spd);
    const float boost_t = diff_spd < 0.0f ? fpu_div(0.0f, span_spd) : fpu_div(diff_spd, span_spd);
    const float pitch_in = lwc1(pv + 0x70);
    const float px = fpu_mul(pitch_in, lwc1(L11_ADDR(0x00162128u)));
    const float pz = fpu_mul(pitch_in, lwc1(L11_ADDR(0x0016212cu)));
    eer::sq(sp + 0xc0, eer::Q128{});
    eer::sq(sp + 0xd0, eer::Q128{});
    const float bx = fpu_mul(boost_t, lwc1(L11_ADDR(0x00162130u)));
    const float x0 = fpu_sub(fpu_sub(lwc1(L11_ADDR(0x00162024u)), px), bx);
    const float y0 = fpu_mul(lwc1(pv + 0x6c), lwc1(L11_ADDR(0x00162080u)));
    const float cam_scale = lwc1(L11_ADDR(0x0016208cu));
    const float z0 = fpu_add(lwc1(L11_ADDR(0x00162028u)), pz);
    swc1(sp + 0xc4, fpu_mul(y0, cam_scale));
    swc1(sp + 0xc8, fpu_mul(z0, cam_scale));
    swc1(sp + 0xd0, fpu_mul(lwc1(L11_ADDR(0x0016202cu)), cam_scale));
    swc1(sp + 0xd8, fpu_mul(lwc1(L11_ADDR(0x00162030u)), cam_scale));
    swc1(sp + 0xc0, fpu_mul(x0, cam_scale));
    lq_sq(sp + 0xb0, sp + 0xd0);
    lq_sq(sp + 0xa0, sp + 0xc0);
    const uint32_t cam_slot = fixes102_pokitaru_jet_camera() ? sp + 0xa0 : sp + 0xa0 + (static_cast<uint32_t>(lbu(pv + 0x60)) << 4);  // (1.02 Fixes, fix 36)
    swc1(pv + 0xc4, boost_t);
    spring_to(pv + 0xb4, pv + 0xc8, lwc1(cam_slot), lwc1(L11_ADDR(0x00162104u)), lwc1(L11_ADDR(0x00162108u)), lwc1(L11_ADDR(0x0016210cu)));
    spring_to(pv + 0xb8, pv + 0xcc, lwc1(cam_slot + 4), lwc1(L11_ADDR(0x00162110u)), lwc1(L11_ADDR(0x00162114u)), lwc1(L11_ADDR(0x00162118u)));
    spring_to(pv + 0xbc, pv + 0xd0, lwc1(cam_slot + 8), lwc1(L11_ADDR(0x0016211cu)), lwc1(L11_ADDR(0x00162120u)), lwc1(L11_ADDR(0x00162124u)));

    swc1(sp + 0x94, lwc1(pv + 0xb8));
    swc1(sp + 0x98, lwc1(pv + 0xbc));
    swc1(sp + 0x90, lwc1(pv + 0xb4));
    sw(sp + 0x9c, 0);
    euler_to_mat3_vu(M(sp + 0xe0), V(pv + 0x30));
    mat3_transform(V(sp + 0x80), V(sp + 0x90), M(sp + 0xe0));
    vec3_add(V(sp + 0xc0), V(m + 0x10), V(sp + 0x80));
    uid5_set_pvar_80(sp + 0xc0);

    const float roll_goal = angle_add(fpu_mul(lwc1(pv + 0x30), lwc1(L11_ADDR(0x00162038u))),
                                      fpu_mul(lwc1(pv + 0x6c), lwc1(L11_ADDR(0x00162034u))));
    swc1(sp + 0xd0, roll_goal);
    const float pitch_goal = angle_sub(angle_add(lwc1(pv + 0x34), lwc1(L11_ADDR(0x0016203cu))),
                                       fpu_mul(lwc1(pv + 0x70), lwc1(L11_ADDR(0x00162014u))));
    swc1(sp + 0xd8, lwc1(pv + 0x38));
    swc1(sp + 0xd4, pitch_goal);
    spring_angle(pv + 0x20, pv + 0xa0, 0, lwc1(sp + 0xd0), lwc1(L11_ADDR(0x001620e0u)), lwc1(L11_ADDR(0x001620e4u)), lwc1(L11_ADDR(0x001620e8u)));
    spring_angle(pv + 0x24, pv + 0xa4, 0, lwc1(sp + 0xd4), lwc1(L11_ADDR(0x001620ecu)), lwc1(L11_ADDR(0x001620f0u)), lwc1(L11_ADDR(0x001620f4u)));
    spring_angle(pv + 0x28, pv + 0xa8, 0, lwc1(sp + 0xd8), lwc1(L11_ADDR(0x001620f8u)), lwc1(L11_ADDR(0x001620fcu)), lwc1(L11_ADDR(0x00162100u)));

    swc1(sp + 0x70, lwc1(pv + 0x20));
    swc1(sp + 0x78, angle_sub(lwc1(pv + 0x28), lwc1(pv + 0xd4)));
    swc1(sp + 0x74, angle_sub(lwc1(pv + 0x24), lwc1(pv + 0xd8)));
    uid5_set_pvar_90(sp + 0x70);

    const float fov_goal = (lwu(0x0013caf0u) & 0x40u) == 0 ? lwc1(L11_ADDR(0x00162134u)) : lwc1(L11_ADDR(0x00162138u));
    const float d2 = dt2(), d = dt();
    const float fov_acc = fpu_add(d2, d2);
    seek_1d(pv + 0xb0, pv + 0xc0, fov_goal, fov_acc, fov_acc, fpu_add(d, d));
    const float half_fov = fpu_mul(lwc1(pv + 0xb0), 0.5f);
    swc1(L11_ADDR(0x0016d4f0u), fpu_div(sin_vu(half_fov), cos_vu(half_fov)));
    projection_setup();

    swc1(m + 0x40, angle_add(lwc1(pv + 0x30), fpu_mul(lwc1(pv + 0x6c), lwc1(L11_ADDR(0x00162150u)))));
    swc1(m + 0x44, angle_add(lwc1(pv + 0x34), fpu_mul(-lwc1(pv + 0x70), lwc1(L11_ADDR(0x00162154u)))));
    swc1(m + 0x48, angle_add(lwc1(pv + 0x38), fpu_mul(-lwc1(pv + 0x6c), lwc1(L11_ADDR(0x00162158u)))));

    if (sphere_collide(m + 0x10, 2.0f, 0, m) != 0) {
        const uint32_t hit = coll_hit_moby();
        if (hit == 0) {
            sw(sp, 1);
            sw(sp + 8, 0xffffffffu);
            sw(sp + 0x10, 0);
            explosion_effect(m, L11_ADDR(0x0015f580u), 0, 0x1e, 0xa, 0x18, -1, 1, 1, -1, 0, 8.0f, 100.0f, 16.0f, 8.0f, 9.0f, 1.0f, 50.0f);
            const uint16_t fl = lhu(m + 0x34);
            sb(m + 0x31, 0);
            sb(m + 0x20, 7);
            sh(m + 0x34, fl | 1u);
            sw(pv + 0xf8, static_cast<uint32_t>(frame_scale_i(0x5a)));
        } else {
            const int16_t hcls = lh(hit + 0xa6);
            if (hcls != 0 && hcls != 0x192 && hcls != 0x4c2 && hcls != 0x4c3) {
                V(m + 0x10) = coll_hit_pushed();
                const Vec4 nrm = coll_hit_normal();
                vec3_reflect(V(sp + 0x50), V(sp + 0x50), nrm);
                const float refl_yaw = atan2_f(lwc1(sp + 0x50), lwc1(sp + 0x54));
                swc1(pv + 0x38, angle_add(lwc1(pv + 0x38), fpu_mul(angle_sub(refl_yaw, lwc1(pv + 0x38)), lwc1(L11_ADDR(0x00162008u)))));
                const float refl_pitch = -atan2_f(vec2_length(V(sp + 0x50)), lwc1(sp + 0x58));
                const float new_pitch = angle_add(lwc1(pv + 0x34), fpu_mul(angle_sub(refl_pitch, lwc1(pv + 0x34)), lwc1(L11_ADDR(0x0016200cu))));
                const float new_spd = fpu_mul(lwc1(pv + 0x64), lwc1(L11_ADDR(0x0016204cu)));
                const uint32_t cool = lwu(pv + 0xdc);
                swc1(pv + 0x34, new_pitch);
                swc1(pv + 0x64, new_spd);
                if (cool == 0) {
                    const float hp = fpu_sub(lwc1(0x0014094cu), lwc1(L11_ADDR(0x00162050u)));
                    swc1(0x0014094cu, hp);
                    if (hp < 0.0f) {
                        vec3_sub(V(sp + 0x60), coll_hit_tri(0), coll_hit_tri(1));
                        vec3_set_length(V(sp + 0x60), V(sp + 0x60), 1.0f);
                        vec3_reflect(V(sp + 0x30), V(sp + 0x50), nrm);
                        const float step = dt();
                        vec3_set_length(V(sp + 0x30), V(sp + 0x30), fpu_add(step, step));
                        vec3_set_length(V(sp + 0x40), nrm, 1.0f);
                        sw(sp, 1);
                        sw(sp + 8, 0xffffffffu);
                        sw(sp + 0x10, 0);
                        explosion_effect(m, sp + 0x30, 0, 0x1e, 0xa, 0x18, -1, 1, 1, -1, 0, 8.0f, 100.0f, 16.0f, 8.0f, 9.0f, 1.0f, 50.0f);
                        sb(m + 0x20, 7);
                    }
                    sw(pv + 0xdc, lwu(L11_ADDR(0x0016214cu)));
                }
            }
        }
    }

    if (lhu(0x00141c10u) == 0) sub_00203908(TXT(0x2afc), 0x55);
    sub_001f93e0(L11_ADDR(0x00311d50u), m);
    pok_3126f8_l11(m, pv);
    const float aim_yaw = angle_add(lwc1(pv + 0x28), fpu_div(-lwc1(pv + 0x6c), 7.0f));
    const float aim_pitch = -angle_add(lwc1(pv + 0x24), fpu_sub(-fpu_div(lwc1(pv + 0x70), 7.0f), u2f(0x3dcccccdu)));
    pok_3125a8_l11(m, pv, aim_yaw, aim_pitch);
    ee_stack_free(0x1c0);
}

// Counts active bombers (class 0x527 = 1319 in state 5) in moby group `group`.
// orig: L11:0x0031a758
int32_t pok_31a758_l11(int32_t group) {
    uint32_t e = lvd::group(group);
    int32_t count = 0;
    for (;; e += 2) {
        const uint16_t raw = lhu(e);
        const uint32_t o = moby_at(raw & 0x7fffu);
        if (lh(o + 0xa6) == 0x527 && lbu(o + 0x20) == 5) ++count;
        if (static_cast<int16_t>(raw) < 0) break;
    }
    return count;
}

// Picks an idle bomber (class 0x527 = 1319 in state 4, 1 in 10 chance per candidate) in `group`, places it in front of
// the camera in its quadrant (`pvar +0x88`) and wakes it in state 5; returns its EE address or 0.
// orig: L11:0x0031a7d8
uint32_t pok_31a7d8_l11(int32_t group) {
    const uint32_t sp = ee_stack_alloc(0x110);
    uint32_t e = lvd::group(group);
    for (;; e += 2) {
        const uint16_t raw = lhu(e);
        const uint32_t idx = raw & 0x7fffu;
        const uint32_t o = moby_at(idx);
        if (lh(o + 0xa6) == 0x527 && lbu(o + 0x20) == 4 && rand_mod(0xa) == 0) {
            const uint32_t b = moby_at(idx);
            const uint32_t bpv = lwu(b + 0x78);
            lq_sq(sp + 0x20, L11_ADDR(0x001677d0u));
            const float pitch = lwc1(sp + 0x24);
            const float max_p = u2f(0x3e32b8c2u);
            const float min_p = u2f(0xbe32b8c2u);
            sw(sp + 0x20, 0);
            if (max_p < pitch) swc1(sp + 0x24, max_p);
            else if (pitch < min_p) swc1(sp + 0x24, min_p);
            euler_to_mat3_vu(M(sp + 0x30), V(sp + 0x20));
            const float qang = angle_add(u2f(0x3f490fdbu), fpu_mul(int_to_float(lw(bpv + 0x88)), u2f(0x3fc90fdbu)));
            vec3_set_length(V(sp), V(sp + 0x40), fpu_mul(sin_vu(qang), 12.0f));
            vec3_add(V(sp + 0x10), V(L11_ADDR(0x001677c0u)), V(sp));
            vec3_set_length(V(sp), V(sp + 0x50), fpu_mul(cos_vu(qang), 12.0f));
            vec3_add(V(sp + 0x10), V(sp + 0x10), V(sp));
            vec3_set_length(V(sp), V(sp + 0x30), 6.0f);
            vec3_add(V(sp + 0x10), V(sp + 0x10), V(sp));
            lq_sq(bpv + 0xb0, kHeroPos);
            lq_sq(b + 0x10, sp + 0x10);
            sb(b + 0xbc, 1);
            sb(b + 0x20, 5);
            const float spd = fpu_mul(dt(), 20.0f);
            sw(bpv + 0x98, 0);
            swc1(bpv + 0x94, 5.0f);
            swc1(bpv + 0x6c, spd);
            if (lbu(b + 0x53) != 0) moby_anim_set(b, 0, 0, frame_scale_i(0xa));
            sh(b + 0x34, lhu(b + 0x34) & 0xfffeu);
            sw(b + 0x94, lwu(lwu(b + 0x24) + 0x10));
            ee_stack_free(0x110);
            return b;
        }
        if (static_cast<int16_t>(raw) < 0) break;
    }
    ee_stack_free(0x110);
    return 0;
}

// Class 1242, Pokitaru's fighter jet: state 0 initializes its HUD palette and hides it (state 9) if its checkpoint
// (+0xb0) was already completed; 1 offers its Triangle prompt (0x2b09; or runs skill point 0x3c's clock before 0x13d4c7
// is set) and boards on Triangle (carrying 1.02 Gameplay, fix 10); 2 fades into flight (4); 4 flies and spawns bombers
// until +0xe8 <= 0 (8, victory) or Triangle exits (5, return to pad); 7 crashes -> hero_lost; 8 lands and marks +0xb0 ->
// 9 (completed).
// orig: L11:0x00313290
void pok_class1242_update_l11(uint32_t m) {
    if (m == 0) return;
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t pv = lwu(m + 0x78);
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xdc)));

    const uint8_t st0 = lbu(m + 0x20);
    if (st0 != 7 && st0 != 1 && st0 != 2) {
        const uint32_t hit = sub_0025a420(m, 0x30001u, 0);
        if (hit != 0 && lbu(m + 0x20) == 4) {
            const float dmg = lwc1(hit + 0x2c);
            const float hp = lwc1(0x0014094cu);
            if (dmg < hp) {
                swc1(0x0014094cu, fpu_sub(hp, dmg));
                if ((lwu(hit + 0x30) & 1u) != 0) {
                    const float push = fpu_mul(lwc1(hit + 0x2c), lwc1(L11_ADDR(0x00162078u)));
                    vec_from_spherical_v(V(sp + 0x20), lwc1(pv + 0x64), lwc1(pv + 0x38), -lwc1(pv + 0x34));
                    vec3_add(V(m + 0x10), V(m + 0x10), V(hit + 0x10));
                    vec3_reflect(V(sp + 0x20), V(sp + 0x20), V(hit + 0x10));
                    const float ry = atan2_f(lwc1(sp + 0x20), lwc1(sp + 0x24));
                    const float dy = fpu_mul(fpu_mul(angle_sub(ry, lwc1(pv + 0x38)), lwc1(L11_ADDR(0x00162008u))), push);
                    swc1(pv + 0x38, angle_add(lwc1(pv + 0x38), dy));
                    const float rp = atan2_f(vec2_length(V(sp + 0x20)), lwc1(sp + 0x28));
                    const float dp = fpu_mul(fpu_mul(angle_sub(rp, lwc1(pv + 0x34)), lwc1(L11_ADDR(0x0016200cu))), push);
                    swc1(pv + 0x34, angle_add(lwc1(pv + 0x34), dp));
                }
            } else {
                vec_from_spherical_v(V(sp + 0x20), lwc1(pv + 0x64), lwc1(pv + 0x38), -lwc1(pv + 0x34));
                sw(0x0014094cu, 0);
                sb(m + 0x20, 7);
                sh(pv + 0x62, 0xb4);
                const uint32_t hm = lwu(0x001413d0u);
                swc1(0x0013f3d8u, fpu_add(lwc1(0x0013f3d8u), u2f(0x3e4ccccdu)));
                sw(hm + 0x98, 0);
                hero_set_state(0, 1);
                sh(0x00140944u, 0xffffu);
                sw(sp, 1);
                sw(sp + 8, 0xffffffffu);
                sw(0x00140940u, 0);
                sw(sp + 0x10, 0);
                explosion_effect(m, sp + 0x20, 0, 0xa, 3, 0x10, -1, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
            }
        }
    }

    sb(m + 0xa4, 0xff);
    if (lwu(0x00140940u) == m) {
        const float pct = fpu_div(lwc1(0x0014094cu), lwc1(L11_ADDR(0x00162074u)));
        sb(0x0014095fu, lbu(0x0014095fu) & 0xfdu);
        swc1(0x00140958u, eer::cvt_w(fpu_mul(pct, 200.0f)));
    }

    const bool fix10 = gameplay102_on(10) || region_core_moved();
    auto fade_in_step = [&] {
        if (lwc1(pv + 0x130) != 0.0f) {
            approach_f(pv + 0x130, 0.0f, fpu_mul(dt(), 4.0f));
            swc1(L11_ADDR(0x0015f3fcu), lwc1(pv + 0x130));
        }
    };

    switch (lbu(m + 0x20)) {  // (jump table L11:0x21b680)
    case 0: {  // (0x00313528)
        sb(m + 0x20, 1);
        sb(pv + 0x60, 0);
        swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24));
        const float r0 = lwc1(pv + 0x30), p0 = lwc1(pv + 0x34);
        swc1(pv + 0x20, r0);
        swc1(pv + 0x24, p0);
        const float fov0 = atan2_f(1.0f, lwc1(L11_ADDR(0x0016d4f0u)));
        const float fov = fpu_add(fov0, fov0);
        sw(pv + 0xc0, 0);
        swc1(pv + 0xb0, fov);
        swc1(pv + 0xac, fov);
        lq_sq(pv + 0x30, m + 0x40);
        lq_sq(pv + 0x50, m + 0x40);
        lq_sq(pv + 0x40, m + 0x10);
        const uint32_t pal = lwu(L11_ADDR(0x0015f420u)) + (static_cast<uint32_t>(lhu(L11_ADDR(0x0016df4au))) << 4);
        const int32_t copied = lw(L11_ADDR(0x00162170u));
        sw(L11_ADDR(0x0016216cu), pal);
        if (copied == 0) {
            const uint32_t dst = L11_ADDR(0x001f1088u);
            for (uint32_t k = 0; k < 0x400u; ++k) sb(dst + k, lbu(pal + k));
        }
        sw(L11_ADDR(0x00162170u), 1);
        sh(pv + 0xe8, 1);
        sw(pv + 0xf8, 1);
        sw(pv + 0x130, 0);
        const uint8_t cp = lbu(m + 0xb0);
        if (cp != 0xff && lbu(0x0014c050u + cp + (static_cast<uint32_t>(lw(0x0015ed84u)) << 4)) == 0xff) {
            const uint16_t fl = lhu(m + 0x34);
            sb(m + 0x20, 9);
            sw(m + 0x94, 0);
            sh(m + 0x34, fl | 1u);
            sb(m + 0x31, 0);
        }
        break;
    }
    case 1: {  // (0x003136c4)
        if (moby_sphere_gap(m, lwu(0x001413d0u)) < lwc1(L11_ADDR(0x00161fd0u)) && !gameplay102_talk_blocked()) {  // (1.02 Gameplay, fix 10)
            if (lbu(0x0013d4c7u) != 0) {
                hud_message_request(7, TXT(0x2b09));
            } else {
                const uint32_t kept = static_cast<uint32_t>(lhu(0x00141b4au)) * 0x258u;
                const int32_t since = frame_scale_i(lw(0x0015eea4u)) - static_cast<int32_t>(kept);
                const int32_t limit = static_cast<int32_t>(f2u(eer::cvt_w(fpu_mul(int_to_float(frame_scale_i(0x12)), 60.0f))));
                if (limit < since || static_cast<uint32_t>(lhu(0x00141b4au)) * 0x258u == 0) {
                    sub_00203908(TXT(0x2afa), 0x3c);
                } else {
                    const int32_t now = ee_div(frame_scale_i(lw(0x0015eea4u)), 0x258);
                    if (static_cast<int32_t>(lhu(0x00141b4au)) < now)
                        sh(0x00141b4au, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4u)), 0x258)));
                }
            }
            if (lbu(0x0013d4c7u) != 0 && (lwu(0x0013cae4u) & 0x10u) != 0) {
                sb(m + 0x20, 2);
                if (fix10) hero_set_state(0x32, 1);  // (1.02 Gameplay, fix 10: 0x32 at the press)
                sw(pv + 0x118, 0xffffffffu);
            }
        } else {
            sw(pv + 0x118, 0xffffffffu);
            fade_in_step();
        }
        break;
    }
    case 2: {  // (0x00313818)
        approach_f(pv + 0x130, 1.0f, fpu_mul(dt(), 4.0f));
        const float f = lwc1(pv + 0x130);
        swc1(L11_ADDR(0x0015f3fcu), f);
        if (f == 1.0f) {
            if (!fix10) hero_set_state(0x32, 1);  // (1.02 Gameplay, fix 10: skipped when set at the press)
            swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 30.0f));
            music_track_request(2, 6);
            vec_clear(V(pv + 0x30));
            vec_clear(V(pv + 0x20));
            const float yaw = lwc1(m + 0x48);
            swc1(pv + 0x38, yaw);
            swc1(pv + 0x28, yaw);
            sw(pv + 0x114, static_cast<uint32_t>(frame_scale_i(0x708)));
            if (lhu(0x00141c10u) == 0) sub_00203908(TXT(0x2afc), 0x55);
            sb(m + 0x20, 4);
            sw(0x00140940u, m);
            const float hp0 = lwc1(L11_ADDR(0x00162048u));
            const uint8_t ammo0 = lbu(L11_ADDR(0x00162054u));
            const uint8_t ammo_max = lbu(L11_ADDR(0x00162058u));
            const uint16_t cls_id = lhu(m + 0xa6);
            sb(0x00140946u, ammo0);
            sb(0x00140948u, ammo_max);
            sb(0x0014095eu, 1);
            sb(0x00140947u, ammo_max);
            swc1(0x0014094cu, hp0);
            const float hp_max = lwc1(L11_ADDR(0x00162074u));
            sh(0x00140944u, cls_id);
            sw(pv + 0xf0, 0);
            sw(pv + 0xf4, 0xff);
            swc1(0x00140950u, hp_max);
            swc1(0x00140954u, 100.0f);
            sb(0x0014094bu, 3);
            sb(0x00140949u, 3);
            sb(0x0014094au, 3);
            sb(0x0014095fu, 0);
            camera_scripted_set(L11_ADDR(0x001677c0u), L11_ADDR(0x001677d0u), 1, 0, 0);
            const float spd0 = lwc1(L11_ADDR(0x00161fd4u));
            sh(pv + 0x68, 0);
            swc1(pv + 0x64, spd0);
            sw(pv + 0x6c, 0);
            sw(pv + 0x70, 0);
            sw(pv + 0x84, 0);
            sw(pv + 0x80, 0);
            sw(pv + 0xd4, 0);
            sw(pv + 0xd8, 0);
            sb(m + 0x30, 0xff);
            sh(m + 0x32, 0xff);
            const float scale = lwc1(L11_ADDR(0x0016208cu));
            const float cx = fpu_mul(lwc1(L11_ADDR(0x00162024u)), scale);
            sw(pv + 0xb8, 0);
            const float cz = fpu_mul(lwc1(L11_ADDR(0x00162028u)), scale);
            swc1(pv + 0xb4, cx);
            swc1(pv + 0xbc, cz);
            sw(lwu(0x001413d0u) + 0x98, 0xffffffffu);
            swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), scale));
            sb(0x001413f5u, 1);
        }
        break;
    }
    case 4: {  // (0x003139c4)
        fade_in_step();
        sw(L11_ADDR(0x0015f608u), 2);
        pok_312838_l11(m, pv);
        const int32_t active = pok_31a758_l11(lw(pv + 0x10c));
        if (lh(pv + 0xea) != 0 && active < 5 - lh(pv + 0xe8) &&
            timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x114))) != 0 && pok_31a7d8_l11(lw(pv + 0x10c)) != 0) {
            sw(pv + 0x114, static_cast<uint32_t>(frame_scale_i(0xf0)));
        }
        if (lh(pv + 0xe8) <= 0) {
            sw(pv + 0xf8, lwu(L11_ADDR(0x00162168u)));
            sb(m + 0x20, 8);
        }
        if ((lbu(0x0014095fu) & 1u) != 0) sb(m + 0x20, 5);
        break;
    }
    case 5: {  // (0x00313ab0)
        const uint32_t hm = lwu(0x001413d0u);
        sb(0x001413f5u, 0);
        sw(hm + 0x98, 0);
        hero_set_state(0, 1);
        hydro_2eac18(0);
        sub_002359f8(lh(0x0014095cu), 0);
        music_track_request(0, 8);
        lq_sq(m + 0x10, pv + 0x40);
        lq_sq(m + 0x40, pv + 0x50);
        swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24));
        moby_update_transform_kept(m);
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0xfc));
        hero_place(box + 0x30, box + 0x70, 0, 1);
        stop_loop_sound(m, pv + 0x12c);
        stop_loop_sound(m, pv + 0x12e);
        const float f = u2f(0x3f7d70a4u);
        sb(m + 0x20, 1);
        swc1(pv + 0x130, f);
        swc1(L11_ADDR(0x0015f3fcu), f);
        break;
    }
    case 7:  // (0x00313bf8)
        sw(L11_ADDR(0x0015f608u), 2);
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xf8))) != 0) hero_lost();
        break;
    case 8: {  // (0x00313c20)
        sw(L11_ADDR(0x0015f608u), 2);
        pok_312838_l11(m, pv);
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xf8))) != 0) {
            approach_f(pv + 0x130, 1.0f, fpu_mul(dt(), 4.0f));
            const float f = lwc1(pv + 0x130);
            swc1(L11_ADDR(0x0015f3fcu), f);
            if (1.0f <= f) {
                const uint32_t hm = lwu(0x001413d0u);
                sb(0x001413f5u, 0);
                sw(hm + 0x98, 0);
                hero_set_state(0, 1);
                hydro_2eac18(0);
                music_track_request(0, 8);
                const uint16_t fl = lhu(m + 0x34);
                sb(m + 0x31, 0);
                sw(m + 0x94, 0);
                sh(m + 0x34, fl | 1u);
                const int32_t dst_box = lw(pv + 0x100);
                if (dst_box == -1) {
                    debug_printf_stub();
                    lq_sq(m + 0x10, pv + 0x40);
                    lq_sq(m + 0x40, pv + 0x50);
                } else {
                    const uint32_t b = lvd::box(lvd::BOXES, dst_box);
                    lq_sq(m + 0x10, b + 0x30);
                    lq_sq(m + 0x40, b + 0x70);
                }
                moby_update_transform_kept(m);
                if (lw(pv + 0xfc) == -1) {
                    debug_printf_stub();
                } else {
                    const uint32_t hb = lvd::box(lvd::BOXES, lw(pv + 0xfc));
                    hero_place(hb + 0x30, hb + 0x70, 0, 1);
                }
                const uint8_t cp = lbu(m + 0xb0);
                if (cp != 0xff) checkpoint_mark_collected(cp);
                sb(m + 0x20, 9);
            }
        }
        break;
    }
    case 9:  // (0x00313d9c)
        fade_in_step();
        break;
    default:  // (3, 6, 10: 0x00313de0)
        break;
    }

    const float lo = 15.0f, hi = 1008.0f;
    for (uint32_t off = 0x10; off <= 0x18; off += 4) {
        if (lwc1(m + off) < lo) swc1(m + off, lo);
        if (hi < lwc1(m + off)) swc1(m + off, hi);
    }
    const uint32_t st = lbu(m + 0x20);
    if (st == 4) {
        lq_sq(kHeroPos, m + 0x10);
        lq_sq(kHeroPos + 0x10, pv + 0x30);
    }
    if (st - 3u < 2u) {
        ship_exhaust_spawn(m, pv);
        if (sub_0028d8c0(m, lh(pv + 0x12c)) == 0) {
            sh(pv + 0x12c, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        }
    } else {
        stop_loop_sound(m, pv + 0x12c);
    }
    ee_stack_free(0xc0);
}

}  // namespace rac1
