// Hoven's (level 12) classes 1259 (electric arc hazard oscillating along a path toward its partner moby, queuing draw
// callback 0x3028c8; helper 0x302c58) and 1267 (the missile turret Ratchet mounts to shoot down the planetary defense
// gunships and dropship, queuing draw callbacks 0x304218 and 0x304d98; helpers 0x3032e8 and 0x303370). Level 12 only:
// `orig: L12:`, level data through lvd::*, Hoven-only data L12_ADDR, base ELF bare. Registered in hoven_mobys.cpp.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/hoven_mobys.h>
#include <game/ovl/level_change.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_homing_missile.h>
#include <game/ovl/shared_small.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};
}  // namespace

// Advances the four electric arc point chains of class 1259 (`pv + arc * 0x460`, velocities `+0x1190`) and queues draw
// callback L12:0x3028c8 when within 48 units of the camera.
// orig: L12:0x00302c58
void beam1259_arcs(uint32_t m) {
    const Frame f(240);
    const uint32_t pv = lwu(m + 0x78);
    const float t = fpu_add(lwc1(L12_ADDR(0x00161df8)), fpu_mul(lwc1(L12_ADDR(0x00161dfc)), lwc1(0x0015ed6c)));
    swc1(L12_ADDR(0x00161df8), t);
    if (1.0f < t) swc1(L12_ADDR(0x00161df8), fpu_sub(t, 1.0f));

    for (int32_t arc = 0; arc < 4; ++arc) {
        const uint32_t timer_addr = pv + 0x2300 + static_cast<uint32_t>(arc << 2);
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(timer_addr))) != 0) {
            const int32_t count = lw(pv + 0x232c);
            const uint32_t alloc_bytes = count > 0 ? ((static_cast<uint32_t>(count) << 7) >> 3) : 0u;
            const Frame dyn(alloc_bytes);
            const uint32_t pts = dyn.sp;
            if (count > 0) {
                uint32_t out = pv + static_cast<uint32_t>(arc * 0x460);
                uint32_t p = pts;
                for (int32_t k = 0; k < lw(pv + 0x232c); ++k, out += 0x10, p += 0x10) {
                    vec_clear(V(out));
                    const float denom = static_cast<float>(lw(pv + 0x232c));
                    const float step = fpu_div(lwc1(pv + 0x2324), denom);
                    swc1(out + 0xc, 1.0f);
                    const float r = fpu_mul(lwc1(L12_ADDR(0x00161e00)), lwc1(0x0015ed6c));
                    swc1(out + 0x0, fpu_mul(static_cast<float>(k), step));
                    rand_sphere_point(p, 0.0f, r);
                }
            }
            if (1 < lw(pv + 0x232c) - 1) {
                uint32_t vel = pv + static_cast<uint32_t>(arc * 0x460) + 0x1190;
                uint32_t p0 = pts, p1 = pts + 0x10, p2 = pts + 0x20;
                for (int32_t k = 1; k < lw(pv + 0x232c) - 1; ++k, vel += 0x10, p0 += 0x10, p1 += 0x10, p2 += 0x10) {
                    vec3_add(V(vel), V(p0), V(p1));
                    vec3_add(V(vel), V(vel), V(p2));
                    vec3_scale(V(vel), V(vel), u2f(0x3eaa7efau));
                }
            }
            sw(timer_addr, lwu(L12_ADDR(0x00161e04)));
        } else if (1 < lw(pv + 0x232c) - 1) {
            uint32_t out = pv + static_cast<uint32_t>(arc * 0x460) + 0x10;
            uint32_t vel = pv + static_cast<uint32_t>(arc * 0x460) + 0x1190;
            for (int32_t k = 1; k < lw(pv + 0x232c) - 1; ++k, out += 0x10, vel += 0x10) {
                vec3_add(V(out), V(out), V(vel));
            }
        }
    }

    if (vec3_distance(V(m + 0x10), V(L12_ADDR(0x00167240))) < 48.0f) {
        sub_001f93e0(L12_ADDR(0x003028c8), m);
    }
}

// Class 1259's update: oscillates between the first two points of path `pv + 0x2310`, faces partner moby `pv + 0x2314`,
// updates the electric arcs when visible and active (`pv + 0x2320 != 0`), and tests segment collision between the pair.
// orig: L12:0x00302f30
void hov_class1259_update_l12(uint32_t m) {
    const Frame f(192);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    const int32_t mate_idx = lw(pv + 0x2314);
    if (mate_idx == -1) return;
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sw(L12_ADDR(0x00161e04), 0x2d);
        sw(L12_ADDR(0x00161e08), 0x16);
        sw(pv + 0x231c, 0xffffffffu);
        sw(pv + 0x2304, 0xb);
        sw(pv + 0x2308, 0x17);
        sw(pv + 0x230c, 0x22);
        sw(pv + 0x2300, 0);
        sb(m + 0x20, 1);
    } else if (st == 1) {
        const uint32_t mate0 = lwu(L12_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(mate_idx) << 8);
        swc1(m + 0x48, atan2_f(fpu_sub(lwc1(mate0 + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(mate0 + 0x14), lwc1(m + 0x14))));
        const uint32_t mate1 = lwu(L12_ADDR(0x0015ffd8)) + (lwu(pv + 0x2314) << 8);
        swc1(pv + 0x2324, vec3_distance(V(m + 0x10), V(mate1 + 0x10)));
        const float step = fpu_mul(fpu_mul(lwc1(pv + 0x2328), u2f(0x3c8efa35u)), lwc1(0x0015ed6c));
        const float ang = angle_add(lwc1(pv + 0x2318), step);
        swc1(pv + 0x2318, ang);
        const uint32_t path = lvd::path(lw(pv + 0x2310));
        float s = sin_vu(ang);
        const float a = lwc1(pv + 0x2318);
        if (u2f(0x3fc90fdbu) < a && a < u2f(0x40490fdbu)) s = fpu_sub(2.0f, s);
        if (u2f(0xc0490fdbu) < a && a < u2f(0xbfc90fdbu)) s = fpu_sub(2.0f, s);
        s = fpu_add(fpu_mul(s, 0.5f), 0.5f);
        if (1.0f < s) s = fpu_sub(s, 2.0f);
        const float t = abs_f(s);
        lq_sq(sp + 0x0, path + 0x10);
        lq_sq(sp + 0x10, path + 0x20);
        vec4_lerp(m + 0x10, sp + 0x0, sp + 0x10, t);
        if (64.0f < vec2_distance(V(m + 0x10), V(0x0013f3d0))) return;
        if (lbu(m + 0x31) == 0) {
            const uint32_t mate2 = lwu(L12_ADDR(0x0015ffd8)) + (lwu(pv + 0x2314) << 8);
            if (lbu(mate2 + 0x31) == 0) {
                vec3_add(V(sp), V(m + 0x10), V(mate2 + 0x10));
                vec3_scale(V(sp), V(sp), 0.5f);
                swc1(sp + 0xc, 16.0f);
                if (point_on_screen(sp, 32.0f) == -1) return;
            }
        }
        const int32_t box = lw(pv + 0x2330);
        if (box != -1 && point_in_box(L12_ADDR(0x00167240), box) != 0) return;
        if (lwu(pv + 0x2320) == 0) return;
        beam1259_arcs(m);
        const int32_t mate_end = lw(pv + 0x2314);
        if (mate_end != -1) {
            lq_sq(sp + 0x0, m + 0x18);  // (lq aligns m + 0x18 down to m + 0x10)
            const uint32_t mate_m = lwu(L12_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(mate_end) << 8);
            lq_sq(sp + 0x10, mate_m + 0x10);
            sub_00259888(1.0f, sp + 0x20, static_cast<int32_t>(m), 0x10001, L12_ADDR(0x0015f580));
            segment_collide_v(V(sp + 0x0), V(sp + 0x10), 0, 0, static_cast<const uint32_t*>(ee_ram_ptr(sp + 0x20)));
        }
    }
}

// Places the turret camera / hero pose from `m`'s transform and turret pitch `pv + 0x74`.
// orig: L12:0x003032e8
void turret_hero_pose(uint32_t m) {
    const Frame f(80);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    camera_flag_88();
    mat3_transform(V(sp), V(L12_ADDR(0x00161ef0)), M(m + 0xc0));
    vec3_add(V(sp), V(sp), V(m + 0x10));
    uid5_set_pvar_80(sp);
    sw(sp + 0x10, 0);
    swc1(sp + 0x14, -lwc1(pv + 0x74));
    swc1(sp + 0x18, angle_add(u2f(0x3fc90fdbu), lwc1(m + 0x48)));
    uid5_set_pvar_90(sp + 0x10);
}

// Updates the turret's lock-on charge (`pv + 0x88`, up to 30 scaled frames) over the 10 target moby slots at `pv + 0x40`.
// orig: L12:0x00303370
void turret_lock_on(uint32_t m) {
    const Frame f(144);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    bool locked = false;
    for (int32_t i = 0; i < 10; ++i) {
        const int32_t idx = lw(pv + 0x40 + static_cast<uint32_t>(i << 2));
        if (idx == -1) continue;
        const uint32_t tm = lwu(L12_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8);
        lq_sq(sp, tm + 0x10);
        float pitch_tol = u2f(0x3dfa35ddu), yaw_tol = pitch_tol;
        if (lh(tm + 0xa6) == 0x146) swc1(sp + 8, fpu_add(lwc1(sp + 8), 1.5f));
        if (lh(tm + 0xa6) == 0x4fe) {
            swc1(sp + 8, fpu_add(lwc1(sp + 8), 1.5f));
            yaw_tol = u2f(0x3d0efa35u);
            pitch_tol = u2f(0x3d567750u);
        }
        const float yaw =
            atan2_f(fpu_sub(lwc1(sp + 0), lwc1(L12_ADDR(0x00167240))), fpu_sub(lwc1(sp + 4), lwc1(L12_ADDR(0x00167244))));
        const float d2d = vec2_distance(V(L12_ADDR(0x00167240)), V(sp));
        const float pitch = atan2_f(d2d, fpu_sub(lwc1(sp + 8), lwc1(L12_ADDR(0x00167248))));
        if (angle_dist(yaw, lwc1(L12_ADDR(0x00167258))) < yaw_tol &&
            angle_dist(pitch, -lwc1(L12_ADDR(0x00167254))) < pitch_tol) {
            locked = true;
        }
    }
    if (locked) {
        sw(pv + 0x88, lwu(pv + 0x88) + 2u);
        if (frame_scale_i(0x1e) < lw(pv + 0x88)) sw(pv + 0x88, static_cast<uint32_t>(frame_scale_i(0x1e)));
    } else {
        timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x88)));
    }
}

// Class 1267's update: states 0..8 (jump table L12:0x20c9d0: 0 set up, 1/2 idle, 3 waiting for the hero to mount, 4 manned
// turret aiming and firing homing missiles until `pv + 0x94` dies or health `pv + 0x8c` runs out, 5 victory movie ->
// cutscene 1, 6 completed, 7 destroyed -> hero_lost, 8 level restart after cutscene 1).
// orig: L12:0x00303540
void hov_class1267_update_l12(uint32_t m) {
    const Frame f(288);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    sw(0x0013e5bc, m);
    switch (lbu(m + 0x20)) {  // jump table L12:0x0020c9d0 (9 entries)
    case 0: {                 // (0x003035b8)
        sw(L12_ADDR(0x00161f00), 0);
        sw(L12_ADDR(0x00161f04), 0);
        const uint32_t cp = 0x0014c050u + (lwu(0x0015ed84) << 4) + lbu(m + 0xb0);
        if (lbu(cp) == 0xff) {
            sb(m + 0x20, 6);
        } else {
            sub_0024f590(m, 0, pv);
            sb(m + 0x20, 3);
            sw(pv + 0x84, 0xc8);
            sw(m + 0x44, 0);
            sh(pv + 0x70, static_cast<uint32_t>(frame_scale_i(0x1e)));
            sh(pv + 0x72, 0);
            sw(pv + 0x78, 0);
            sw(pv + 0x7c, 0);
            sw(pv + 0x98, 0xffffffffu);
            swc1(pv + 0xa0, lwc1(m + 0x18));
            swc1(m + 0x48, u2f(0x40490fdbu));
            sw(0x0013e5bc, m);
        }
        break;
    }
    case 1:
    case 2:  // (0x003041d8)
        break;
    case 3: {  // (0x00303660)
        moby_chain_point_v(m, 3, V(sp + 0x20));
        if (vec2_distance(V(m + 0x10), V(0x0013f3d0)) < 7.0f || lwu(0x0013f64c) == m) {
            const float target_z = fpu_sub(lwc1(pv + 0xa0), u2f(0x3f99999au));
            const float z = lwc1(m + 0x18);
            swc1(m + 0x18, fpu_add(z, fpu_mul(fpu_sub(target_z, z), u2f(0x3de147aeu))));
            const float to_hero =
                atan2_f(fpu_sub(lwc1(0x0013f3d0), lwc1(m + 0x10)), fpu_sub(lwc1(0x0013f3d4), lwc1(m + 0x14)));
            const float diff = angle_add(angle_sub(to_hero, lwc1(m + 0x48)), u2f(0x3fc90fdbu));
            const float vel_yaw = fpu_mul(angle_add(lwc1(pv + 0xa4), fpu_mul(diff, u2f(0x3c23d70au))), u2f(0x3f666666u));
            swc1(pv + 0xa4, vel_yaw);
            swc1(m + 0x48, angle_add(lwc1(m + 0x48), vel_yaw));
        } else {
            const float z = lwc1(m + 0x18), z0 = lwc1(pv + 0xa0);
            if (z < z0) swc1(m + 0x18, fpu_add(z, lwc1(0x0015ed6c)));
            else swc1(m + 0x18, z0);
            if (lwu(0x0013f64c) != m && lw(pv + 0x98) != -1) moby_sound_start(2, 0, m);
        }
        const uint32_t hm = lwu(0x001413d0);
        if (vec2_distance(V(m + 0x10), V(hm + 0x10)) < u2f(0x3fa66666u) && frame_scale_i(0xf) < lw(0x0013f650)) {
            camera_scripted_set(m + 0x10, m + 0x40, 1, 0, 0);
            if (lbu(m + 0x53) != 5) moby_set_anim(m, 5, 0);
            hero_set_state(0x32, 1);
            turret_hero_pose(m);
            swc1(pv + 0x8c, 200.0f);
            sb(m + 0xa4, 0xff);
            sw(L12_ADDR(0x00161f20), lwu(L12_ADDR(0x00161f20)) + 1u);
            sb(m + 0x20, 4);
        }
        break;
    }
    case 4: {  // (0x00303884)
        sb(0x001413f5, 1);
        sw(L12_ADDR(0x00161f00), 1);
        sw(0x0013f51c, static_cast<uint32_t>(frame_scale_i(0x78)));
        const uint32_t hide_t = static_cast<uint32_t>(frame_scale_i(0x78));
        const int32_t sc = lw(L12_ADDR(0x0015f5c4));
        sw(0x0013f510, hide_t);
        if (sc != 2 && lw(L12_ADDR(0x001619a0)) == 0) {
            draw_callback_add(L12_ADDR(0x00304218), m);
        }
        turret_lock_on(m);
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x90))) != 0) {
            const uint32_t hit = sub_0025a420(m, 0x00010000u, 0);
            if (hit != 0) {
                sw(pv + 0x90, static_cast<uint32_t>(frame_scale_i(4)));
                swc1(pv + 0x8c, fpu_sub(lwc1(pv + 0x8c), lwc1(hit + 0x2c)));
            }
            sb(m + 0xa4, 0xff);
        }
        if (lw(L12_ADDR(0x00161f24)) == 0) {
            if ((lbu(0x0014095f) & 1u) != 0) {
                if (100.0f < lwc1(pv + 0x8c)) sw(L12_ADDR(0x00161f20), lwu(L12_ADDR(0x00161f20)) - 1u);
                sb(0x0014095f, 0);
                hero_lost();
            }
            if (lwc1(pv + 0x8c) <= 0.0f) {
                sw(L12_ADDR(0x00161f04), 1);
                sh(pv + 0xac, static_cast<uint32_t>(frame_scale_i(0x78)));
                sb(m + 0x20, 7);
                return;
            }
        }
        const int32_t boss_idx = lw(pv + 0x94);
        if (boss_idx != -1) {
            const uint32_t boss = lwu(L12_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(boss_idx) << 8);
            if (static_cast<int8_t>(lbu(boss + 0x20)) < 0) {
                if (lw(L12_ADDR(0x0015f5c4)) == 2) {
                    if (lbu(pv + 0x1) != 0) sub_0024f608(m, pv);
                    if (lbu(m + 0x53) != 0) moby_set_anim(m, 0, 0);
                    swc1(m + 0x18, lwc1(pv + 0xa0));
                    swc1(m + 0x48, u2f(0x40490fdbu));
                } else {
                    checkpoint_mark_collected(lbu(m + 0xb0));
                    movie_play(0xd);
                    level_mark_visited(0xd);
                    vec_clear(V(sp + 0x20));
                    sw(sp + 0x2c, 0);
                    vec_clear(V(sp + 0x30));
                    sw(sp + 0x3c, 0);
                    swc1(sp + 0x20, u2f(0x43a43333u));
                    swc1(sp + 0x24, u2f(0x43a9599au));
                    swc1(sp + 0x28, u2f(0x4273eb85u));
                    swc1(sp + 0x38, u2f(0xbfd9999au));
                    sw(L12_ADDR(0x00161f00), 0);
                    hero_place(sp + 0x20, sp + 0x30, 0, 1);
                    sb(m + 0x20, 5);
                    return;
                }
            }
        }
        if (lw(L12_ADDR(0x001619a0)) != 0) return;

        float sx = lwc1(0x00141070), sy = lwc1(0x00141074);
        swc1(sp + 0x20, sx);
        if (lw(0x0015eddc) == 0) sy = -sy;
        swc1(sp + 0x24, sy);
        sx = -sx;
        const float deg = u2f(0x3c8efa35u);
        const float max_vyaw = fpu_mul(fpu_mul(lwc1(L12_ADDR(0x00161ed0)), deg), lwc1(0x0015ed6c));
        const float goal_vyaw = fpu_mul(max_vyaw, sx);
        const float cur_vyaw = lwc1(pv + 0x7c);
        const float accel_yaw =
            ((0.0f <= cur_vyaw && cur_vyaw < goal_vyaw) || (cur_vyaw <= 0.0f && goal_vyaw < cur_vyaw))
                ? lwc1(L12_ADDR(0x00161ed8))
                : lwc1(L12_ADDR(0x00161ee0));
        approach_f(pv + 0x7c, goal_vyaw, fpu_mul(fpu_mul(accel_yaw, deg), lwc1(0x0015ed70)));
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x7c)));

        const float max_vpitch = -fpu_mul(fpu_mul(lwc1(L12_ADDR(0x00161ed4)), deg), lwc1(0x0015ed6c));
        const float goal_vpitch = fpu_mul(max_vpitch, -lwc1(sp + 0x24));
        const float cur_vpitch = lwc1(pv + 0x78);
        const float accel_pitch =
            ((0.0f <= cur_vpitch && cur_vpitch < goal_vpitch) || (cur_vpitch <= 0.0f && goal_vpitch < cur_vpitch))
                ? lwc1(L12_ADDR(0x00161edc))
                : lwc1(L12_ADDR(0x00161ee4));
        approach_f(pv + 0x78, goal_vpitch, fpu_mul(fpu_mul(accel_pitch, deg), lwc1(0x0015ed70)));
        const float next_pitch = angle_add(lwc1(pv + 0x74), lwc1(pv + 0x78));
        const float max_p = lwc1(L12_ADDR(0x00161eac));
        swc1(pv + 0x74, max_p < next_pitch ? max_p : next_pitch);
        const float min_p = lwc1(L12_ADDR(0x00161ea8));
        if (lwc1(pv + 0x74) < min_p) swc1(pv + 0x74, min_p);
        quat_from_axis_angle(V(pv + 0x10), 1, -lwc1(pv + 0x74));
        if ((lbu(m + 0x70) & 2u) != 0) moby_anim_set(m, 5, 0, 1);
        timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x70)));

        if ((lwu(0x0013cae0) & 0xa0u) != 0 && lh(pv + 0x70) == 0) {
            moby_anim_set(m, (lwu(pv + 0x80) ^ 1u) != 0 ? 3u : 4u, 0, 1);
            const float fire_yaw = angle_add(lwc1(m + 0x48), u2f(0x3fc90fdbu));
            const uint32_t dir = sp + 0x30, muzzle = sp + 0x40, pvel = sp + 0x50;
            vec_from_spherical_v(V(dir), fpu_mul(lwc1(L12_ADDR(0x00161ee8)), lwc1(0x0015ed6c)), fire_yaw,
                                 lwc1(pv + 0x74));
            if (lwu(pv + 0x80) == 1) {
                sw(pv + 0x80, 0);
                moby_chain_point_v(m, 2, V(muzzle));
            } else {
                moby_chain_point_v(m, 1, V(muzzle));
                sw(pv + 0x80, 1);
            }
            vec3_add(V(muzzle), V(muzzle), V(dir));
            vec3_add(V(muzzle), V(muzzle), V(dir));
            vec3_add(V(muzzle), V(muzzle), V(dir));
            homing_missile_launch(m, muzzle, dir, 0);
            moby_sound_start(0, 0, m);
            swc1(L12_ADDR(0x00167260), u2f(0x3cf5c28fu));
            sw(L12_ADDR(0x00167268), static_cast<uint32_t>(frame_scale_i(0xa)));
            swc1(L12_ADDR(0x00167270), u2f(0x3cf5c28fu));
            sw(L12_ADDR(0x00167278), static_cast<uint32_t>(frame_scale_i(7)));
            sh(pv + 0x70, static_cast<uint32_t>(frame_scale_i(0x14)));
            for (int k = 0; k < 5; ++k) {
                vec_clear(V(sp + 0x60));
                sw(sp + 0x6c, 0);
                swc1(sp + 0x60, rand_range_f(-1.0f, 1.0f));
                swc1(sp + 0x64, rand_range_f(-1.0f, 1.0f));
                swc1(sp + 0x68, rand_range_f(-1.0f, 1.0f));
                lq_sq(pvel, sp + 0x60);
                const float len = vec3_length(V(dir));
                const float spread = fpu_mul(len, fpu_add(fpu_mul(lwc1(0x0015ed60), u2f(0xbeb33334u)), 1.0f));
                vec3_set_length(V(pvel), V(pvel), spread);
                vec3_add(V(pvel), V(dir), V(pvel));
                const float dt = lwc1(0x0015ed6c);
                const float spd = rand_range_f(fpu_mul(dt, 3.0f), fpu_mul(dt, 6.0f));
                vec3_set_length(V(pvel), V(pvel), spd);
                const int32_t lo_f = frame_scale_i(5), hi_f = frame_scale_i(0xa);
                const uint32_t n = static_cast<uint32_t>(rand_range(lo_f, hi_f));
                spawn_particle_1b(muzzle, pvel, 0x5f2f4f6fu, n, 50000.0f);
            }
            vec3_set_length(V(dir), V(dir), fpu_mul(lwc1(0x0015ed6c), 0.5f));
            const int32_t lo_f = frame_scale_i(4), hi_f = frame_scale_i(7);
            const uint32_t n = static_cast<uint32_t>(rand_range(lo_f, hi_f));
            spawn_particle_1b(muzzle, dir, 0x2f4f7f7fu, n, 500000.0f);
        }
        if (lw(L12_ADDR(0x001619a0)) == 0) turret_hero_pose(m);
        break;
    }
    case 5:  // (0x00304170)
        if (lw(L12_ADDR(0x0015f5c4)) != 2) {
            cutscene_start(1);
            sb(m + 0x20, 8);
        }
        break;
    case 6:  // (0x003041c0)
        if (lbu(m + 0x53) != 0) moby_set_anim(m, 0, 0);
        break;
    case 7: {  // (0x00304010)
        draw_callback_add(L12_ADDR(0x00304d98), m);
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0xae))) != 0) {
            const float r = rand_range_f(lwc1(L12_ADDR(0x00161e60)), lwc1(L12_ADDR(0x00161e64)));
            const float half_sixth = u2f(0x3f060a92u);
            const float yaw = angle_add(lwc1(L12_ADDR(0x00167258)), rand_range_signed_f(0.0f, half_sixth));
            const float pitch = angle_add(lwc1(L12_ADDR(0x00167254)), rand_range_signed_f(0.0f, half_sixth));
            const uint32_t pt = sp + 0x20;
            lq_sq(pt, L12_ADDR(0x00167240));
            sub_00262cf0(pt, pt, r, yaw, pitch);
            const float sc = rand_range_f(0.5f, u2f(0x3fa66666u));
            const float sc2 = fpu_mul(sc, u2f(0x3f11eb85u));
            sw(sp + 0x8, 0xffffffffu);
            sw(sp + 0x0, 0);
            sw(sp + 0x10, 0);
            explosion_effect(m, L12_ADDR(0x0015f580), pt, 7, 0xf, 0x19, 3, 0, 0, -1, 0, 0.0f, 0.0f, sc, sc2, 10.0f,
                             u2f(0x3f333333u), 0.0f);
            sh(pv + 0xae, static_cast<uint32_t>(frame_scale_i(rand_range(5, 0xd))));
        }
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0xac))) != 0) hero_lost();
        break;
    }
    case 8:  // (0x00304198)
        if (lw(L12_ADDR(0x0015f5c4)) != 2) {
            level_change_request(0, -1);
            sb(m + 0x20, 6);
        }
        break;
    default:
        break;
    }
}

}  // namespace rac1
