// Batalia (level 8) own moby classes, hand-ported: class 440, the anti-air turret Ratchet mans to defend the fort from
// the four Blarg gunships, and its helpers (the missile under the sights, the outcome handed to the Commander). Its HUD
// draw callback L8:0x002deee0 is in batalia_draws.cpp. Header batalia_own.h; registered in batalia_mobys.cpp.
// Addresses are level 8's (`orig: L8:`); its own data and code are L8_ADDR (tuning and offsets at 0x161a90 .. 0x161bf0,
// the camera 0x1675c0 and shake 0x1675e0 .. 0x1675f8), the base ELF's are bare.
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/batalia_own.h>
#include <game/ovl/hydro.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_homing_missile.h>
#include <game/ovl/shared_small.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

namespace {
constexpr uint32_t PLAYER = 0x0013f350;      // the hero (+0x80 position, +0x90 rotation, +0x160f dismount flag)
constexpr uint32_t HERO_POS = PLAYER + 0x80;
constexpr uint32_t HERO_ROT = PLAYER + 0x90;
constexpr uint32_t HERO_MOBY = 0x001413d0;

const float kDegToRad = u2f(0x3c8efa35u);
const float kFourDegrees = u2f(0x3d8efa35u);
const float kPi = u2f(0x40490fdbu);
const float kHalfPi = u2f(0x3fc90fdbu);
const float kMinPitch = u2f(0xbf490fdbu);   // -45 degrees
const float kMaxPitch = u2f(0x3eb2b8c2u);   // 20 degrees

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

// the moby of the level's moby table at index `i`
uint32_t table_moby(uint32_t i) { return lwu(L8_ADDR(0x0015ffd8)) + (i << 8); }

// the turret's story byte in the level flags row (0xff once the gunship battle is won)
bool battle_won(uint32_t m) {
    return lbu(0x0014c050u + (lwu(0x0015ed84) << 4) + lbu(m + 0xb0)) == 0xff;
}

// the heading from (x0, y0) to (x1, y1), as the original computes it
float heading(float x0, float y0, float x1, float y1) { return atan2_f(fpu_sub(x1, x0), fpu_sub(y1, y0)); }
}  // namespace

// The first active missile in the turret `m`'s eight incoming slots (pvars +0x60 .. +0x7c) whose direction from the
// camera (0x1675c0) lies within 4 degrees of the turret's aim (+0x48 yaw, -+0x44 pitch); 0 when none is under the sights.
// orig: L8:0x002e0228
uint32_t turret_aimed_missile_l8(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(144);
    const uint32_t aim = sp, to_shot = sp + 0x10;
    const uint32_t pv = lwu(m + 0x78);
    uint32_t found = 0;
    for (uint32_t i = 0; i < 8; ++i) {
        const uint32_t shot = lwu(pv + 0x60 + i * 4);
        if (shot == 0) continue;
        vec_from_spherical_v(V(aim), 1.0f, lwc1(m + 0x48), -lwc1(m + 0x44));
        vec3_sub(V(to_shot), V(shot + 0x10), V(L8_ADDR(0x001675c0)));
        vec3_set_length(V(to_shot), V(to_shot), 1.0f);
        const float dot = vec3_dot(V(to_shot), V(aim));
        if (cos_vu(kFourDegrees) < dot) {
            found = lwu(pv + 0x60 + i * 4);
            break;
        }
    }
    ee_stack_free(144);
    return found;
}

// The turret's outcome handed to the Commander `commander` (class 1283): state 1; `outcome` 0 (the turret's health gone)
// asks for his scene 4 (+0x8 1, +0x36 4), 1 (all four gunships down) for his scene 3 (+0x8 1, +0x36 3), -1 loses Ratchet.
// orig: L8:0x00306b30
void turret_commander_end_l8(uint32_t commander, int32_t outcome) {
    sb(commander + 0x20, 1);
    const uint32_t cpv = lwu(commander + 0x78);
    if (outcome == 0) {
        sb(cpv + 8, 1);
        sh(cpv + 0x36, 4);
    } else if (outcome > 0) {
        if (outcome == 1) {
            sb(cpv + 8, static_cast<uint32_t>(outcome));
            sh(cpv + 0x36, 3);
        }
    } else if (outcome == -1) {
        hero_lost();
    }
}

// Class 440, the anti-air turret. pvars: +0x40 its fire cooldown, +0x42 its mount cooldown, +0x48 / +0x4c its pitch and
// yaw rates, +0x52 its camera settle timer, +0x54 which barrel fires next (1: left +0x90, else right +0x94), +0x58 its
// health (200 at setup), +0x5c the gunships' volley timer, +0x5e the gunship next ordered to fire, +0x5f a countdown to
// pick a new one, +0x60 .. +0x7c eight incoming missile slots, +0x80 .. +0x8c the four gunships' moby indices, +0x90 /
// +0x94 its two barrels (class 0x236), +0x98 the missiles' speed cap (10 .. 17.5 as gunships fall), +0x9c the Commander's
// moby index, +0xa0 its lock-on counter, +0xa4 its stand (class 0x710) and +0xa8 the stand's pitch offset (pi/2 once won).
// Mount flag +0xbc: 1 manned (Ratchet held at the seat, camera scripted, gunships sent from state 2 to 3; health 0 places
// Ratchet before the Commander with outcome 0; once won, triangle 0x13cae4 bit 4 or +0x160f bit 0 dismounts him to 0x161a90),
// 2 stepping off (clears to 0 with a 60-frame cooldown once Ratchet is over 1 away), 0 idle (once won, Ratchet within 1
// across and 7 above mounts it, hero state 0x32). States: 0 setup -> 4; 4 idle; 5 manned (aims from stick 0x141070/74,
// locks onto missiles, fires homing missiles from alternating barrels on 0x13cae0 bit 5, updates camera and HUD draw
// 0x2deee0, orders gunship volleys and on all four dead places Ratchet before the Commander with outcome 1); 6 retires
// both barrels and itself. Every frame except the two Commander exits and state 6 keeps the barrels and stand placed.
// orig: L8:0x002e0328
void bat_class440_update_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(320);
    const uint32_t pos = m + 0x10, rot = m + 0x40, seat = sp;

    lq_sq(seat, pos);
    swc1(seat + 8, fpu_add(lwc1(seat + 8), 0.4f));
    const int32_t ready = timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x42)));
    const uint32_t ride = lbu(m + 0xbc);

    if (ready != 0 && ride == 1) {
        lq_sq(HERO_POS, seat);
        lq_sq(HERO_ROT, rot);
        swc1(sp + 0x10, fpu_mul(cos_vu(lwc1(HERO_ROT + 8)), -0.7f));
        const float back_y = fpu_mul(sin_vu(lwc1(HERO_ROT + 8)), -0.7f);
        sw(sp + 0x18, 0);
        swc1(sp + 0x14, back_y);
        vec3_add(V(HERO_POS), V(HERO_POS), V(sp + 0x10));
        mem_clear128(PLAYER + 0xe0, 0x90);
        if (lw(pv + 0x58) == 0) {
            // turret health depleted: place Ratchet in front of the Commander, outcome 0
            sw(m + 0x44, 0);
            sb(m + 0xbc, 2);
            hydro_2eac18(0);
            sb(m + 0x20, 4);
            const uint32_t commander = table_moby(lwu(pv + 0x9c));
            lq_sq(sp + 0x30, commander + 0x40);
            swc1(sp + 0x38, angle_add(lwc1(sp + 0x38), kPi));
            vec3_set_length(V(sp + 0x20), V(commander + 0xc0), 2.5f);
            vec3_add(V(sp + 0x20), V(sp + 0x20), V(commander + 0x10));
            hero_place(sp + 0x20, sp + 0x30, 0, 1);
            turret_commander_end_l8(commander, 0);
            ee_stack_free(320);
            return;
        }
        if (!battle_won(m) && (lbu(PLAYER + 0x160f) & 1u) != 0) hero_lost();
        if (((lwu(0x0013cae4) & 0x10u) != 0 || (lbu(PLAYER + 0x160f) & 1u) != 0) && battle_won(m)) {
            // dismount after the battle is won
            sb(PLAYER + 0x160f, 0);
            hero_set_state(0, 1);
            sw(m + 0x44, 0);
            sb(m + 0xbc, 2);
            euler_to_mat3_vu(M(m + 0xc0), V(rot));
            vec_clear(V(sp + 0x30));
            swc1(sp + 0x38, lwc1(m + 0x48));
            mat3_transform(V(sp + 0x20), V(L8_ADDR(0x00161a90)), M(m + 0xc0));
            vec3_add(V(sp + 0x20), V(sp + 0x20), V(pos));
            hero_place(sp + 0x20, sp + 0x30, 0, 1);
            hydro_2eac18(0);
            swc1(HERO_ROT + 8, angle_add(lwc1(HERO_ROT + 8), kPi));
            sb(m + 0x20, 4);
        } else {
            if (lbu(m + 0x20) != 5) {
                sh(pv + 0x52, 2);
                camera_scripted_set(pos, rot, 1, 0, 0);
            }
            sb(m + 0x20, 5);
            for (uint32_t i = 0; i < 4; ++i) {
                const uint32_t g = table_moby(lwu(pv + 0x80 + i * 4));
                if (lbu(g + 0x20) == 2) sb(g + 0x20, 3);
            }
        }
    } else if (ride == 2) {
        if (1.0f < vec2_distance(V(seat), V(lwu(HERO_MOBY) + 0x10))) {
            sb(m + 0xbc, 0);
            sh(pv + 0x42, static_cast<uint32_t>(frame_scale_i(0x3c)));
        }
    } else if (battle_won(m) && vec2_distance(V(seat), V(lwu(HERO_MOBY) + 0x10)) < 1.0f &&
               fpu_sub(lwc1(HERO_POS + 8), lwc1(m + 0x18)) < 7.0f && lh(pv + 0x42) == 0 && lw(pv + 0x58) > 0) {
        sb(m + 0xbc, 1);
        hero_set_state(0x32, 1);
    }

    switch (lbu(m + 0x20)) {  // (a compare tree on 0, 4, 5, 6)
    case 0: {
        sb(m + 0x20, 4);
        sw(pv + 0x58, 0xc8);
        swc1(pv + 0x98, 10.0f);
        const uint32_t stand = moby_spawn(0x710);
        sw(pv + 0xa4, stand);
        if (stand != 0) {
            lq_sq(stand + 0x10, pos);
            lq_sq(stand + 0x40, rot);
            sh(stand + 0x32, 0x30);
            sb(lwu(pv + 0xa4) + 0x31, 1);
            sd(lwu(pv + 0xa4) + 0x38, ld(m + 0x38));
        }
        sw(m + 0x44, 0);
        sh(pv + 0x40, static_cast<uint32_t>(frame_scale_i(0x1e)));
        sh(pv + 0x42, 0);
        sw(pv + 0x48, 0);
        sw(pv + 0x4c, 0);
        for (int32_t i = 7; i >= 0; --i) sw(pv + 0x60 + static_cast<uint32_t>(i) * 4, 0);
        const int32_t volley = frame_scale_i(0xb4);
        sb(pv + 0x5f, 0);
        sb(pv + 0x5e, 0);
        sh(pv + 0x5c, static_cast<uint32_t>(volley));
        if (battle_won(m))
            swc1(pv + 0xa8, kHalfPi);
        else
            sw(pv + 0xa8, 0);
        break;
    }
    case 5: {
        if (lw(pv + 0x58) < 0) sw(pv + 0x58, 0);
        const float sx = lwc1(0x00141070);
        float sy = lwc1(0x00141074);
        if (lw(0x0015eddc) == 0) sy = -sy;
        swc1(sp + 0x20, sx);
        swc1(sp + 0x24, sy);
        const float stick_len = vec2_length(V(sp + 0x20));
        if (1.0f < stick_len) vec3_div(V(sp + 0x20), V(sp + 0x20), stick_len);

        const float max_pitch = -fpu_mul(fpu_mul(lwc1(L8_ADDR(0x00161aec)), kDegToRad), dt());
        const float max_yaw = fpu_mul(fpu_mul(lwc1(L8_ADDR(0x00161ae8)), kDegToRad), dt());
        const float cur_yaw = lwc1(pv + 0x4c);
        const float want_yaw = fpu_mul(max_yaw, -lwc1(sp + 0x20));
        const float want_pitch = fpu_mul(max_pitch, lwc1(sp + 0x24));
        const float yaw_acc = ((0.0f <= cur_yaw && cur_yaw < want_yaw) || (cur_yaw <= 0.0f && want_yaw < cur_yaw))
                                  ? lwc1(L8_ADDR(0x00161af0))
                                  : lwc1(L8_ADDR(0x00161af8));
        approach_f(pv + 0x4c, want_yaw, fpu_mul(fpu_mul(yaw_acc, kDegToRad), dt_sq()));

        const float cur_pitch = lwc1(pv + 0x48);
        const float pitch_acc =
            ((0.0f <= cur_pitch && cur_pitch < want_pitch) || (cur_pitch <= 0.0f && want_pitch < cur_pitch))
                ? lwc1(L8_ADDR(0x00161af4))
                : lwc1(L8_ADDR(0x00161afc));
        approach_f(pv + 0x48, want_pitch, fpu_mul(fpu_mul(pitch_acc, kDegToRad), dt_sq()));

        swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x4c)));
        const float pitch = angle_add(lwc1(m + 0x44), lwc1(pv + 0x48));
        swc1(m + 0x44, pitch);
        if (pitch < kMinPitch)
            swc1(m + 0x44, kMinPitch);
        else if (kMaxPitch < pitch)
            swc1(m + 0x44, kMaxPitch);

        const uint32_t aimed = turret_aimed_missile_l8(m);
        if (aimed != 0) {
            const int32_t cap = frame_scale_i(0x1e);
            const int32_t lock = lw(pv + 0xa0);
            if (lock < cap) sw(pv + 0xa0, static_cast<uint32_t>(lock + 1));
        } else {
            const uint32_t lock = lwu(pv + 0xa0);
            if (lock != 0) sw(pv + 0xa0, lock - 1u);
        }

        timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x40)));
        if ((lwu(0x0013cae0) & 0x20u) != 0 && lh(pv + 0x40) == 0) {
            const bool left = lh(pv + 0x54) == 1;
            const uint32_t shot_vel = sp + 0x30, muzzle = sp + 0x40, spark_vel = sp + 0x50, rnd = sp + 0x60;
            const float speed = fpu_mul(lwc1(L8_ADDR(0x00161b00)), dt());
            vec_from_spherical_v(V(shot_vel), speed, lwc1(m + 0x48), -lwc1(m + 0x44));
            moby_anim_set(lwu(pv + (left ? 0x90u : 0x94u)), 1, 0, 0);
            mat3_transform(V(muzzle), V(left ? L8_ADDR(0x00161b30) : L8_ADDR(0x00161b40)), M(m + 0xc0));
            vec3_add(V(muzzle), V(pos), V(muzzle));
            const uint32_t target = left ? aimed : turret_aimed_missile_l8(m);
            homing_missile_launch(m, muzzle, shot_vel, target);
            moby_sound_start(0, 0, m);
            swc1(L8_ADDR(0x001675e0), 0.03f);
            sw(L8_ADDR(0x001675e8), static_cast<uint32_t>(frame_scale_i(0xa)));
            swc1(L8_ADDR(0x001675f0), 0.03f);
            sw(L8_ADDR(0x001675f8), static_cast<uint32_t>(frame_scale_i(7)));
            sh(pv + 0x54, left ? 0 : 1);
            sh(pv + 0x40, static_cast<uint32_t>(frame_scale_i(0x14)));
            for (int32_t k = 4; k >= 0; --k) {
                sd(rnd, 0);
                sd(rnd + 8, 0);
                for (uint32_t lane = 0; lane < 12; lane += 4) swc1(rnd + lane, rand_range_f(-1.0f, 1.0f));
                lq_sq(spark_vel, rnd);
                const float len = vec3_length(V(shot_vel));
                const float scale = fpu_add(fpu_mul(lwc1(0x0015ed60), u2f(0xbeb33334u)), 1.0f);  // (-0.35)
                vec3_set_length(V(spark_vel), V(spark_vel), fpu_mul(len, scale));
                vec3_add(V(spark_vel), V(shot_vel), V(spark_vel));
                vec3_set_length(V(spark_vel), V(spark_vel), rand_range_f(fpu_mul(dt(), 3.0f), fpu_mul(dt(), 6.0f)));
                const int32_t life = rand_range(frame_scale_i(5), frame_scale_i(0xa));
                spawn_particle_1b(muzzle, spark_vel, 0x5f2f4f6fu, static_cast<uint32_t>(life), 50000.0f);
            }
            vec3_set_length(V(shot_vel), V(shot_vel), fpu_mul(dt(), 0.5f));
            const int32_t flash_life = rand_range(frame_scale_i(4), frame_scale_i(7));
            spawn_particle_1b(muzzle, shot_vel, 0x2f4f7f7fu, static_cast<uint32_t>(flash_life), 500000.0f);
        }

        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x52))) != 0) {
            camera_flag_88();
            mat3_transform(V(sp + 0x30), V(L8_ADDR(0x00161b50)), M(m + 0xc0));
            vec3_add(V(sp + 0x30), V(sp + 0x30), V(pos));
            uid5_set_pvar_80(sp + 0x30);
            uid5_set_pvar_90(rot);
        }
        draw_callback_add(L8_ADDR(0x002deee0), m);

        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x5c))) != 0) {
            bool clear = true;
            for (uint32_t i = 0; i < 8; ++i) {
                const uint32_t shot = lwu(pv + 0x60 + i * 4);
                if (shot != 0 && static_cast<int8_t>(lbu(shot + 0x20)) >= 0 && lbu(shot + 0x20) != 2) clear = false;
            }
            if (clear) {
                uint32_t dead = 0;
                for (uint32_t i = 0; i < 4; ++i) {
                    const uint32_t g = table_moby(lwu(pv + 0x80 + i * 4));
                    if (static_cast<int8_t>(lbu(g + 0x20)) < 0) ++dead;
                }
                // (the table 0x1fbf90: 0 .. 4 gunships down)
                switch (dead) {
                case 0:  // (0x002e115c)
                    swc1(pv + 0x98, 10.0f);
                    sh(pv + 0x5c, static_cast<uint32_t>(frame_scale_i(0x11d)));
                    break;
                case 1:  // (0x002e116c)
                    swc1(pv + 0x98, 12.5f);
                    sh(pv + 0x5c, static_cast<uint32_t>(frame_scale_i(0xf0)));
                    break;
                case 2:  // (0x002e117c)
                    swc1(pv + 0x98, 15.0f);
                    sh(pv + 0x5c, static_cast<uint32_t>(frame_scale_i(0xc3)));
                    break;
                case 3:  // (0x002e118c)
                    swc1(pv + 0x98, 17.5f);
                    sh(pv + 0x5c, static_cast<uint32_t>(frame_scale_i(0x96)));
                    break;
                case 4:  // (0x002e11a8) all four gunships destroyed
                    if (!battle_won(m)) {
                        sw(m + 0x44, 0);
                        sb(m + 0xbc, 2);
                        hydro_2eac18(0);
                        sb(m + 0x20, 4);
                        const uint32_t commander = table_moby(lwu(pv + 0x9c));
                        lq_sq(sp + 0x40, commander + 0x40);
                        swc1(sp + 0x48, angle_add(lwc1(sp + 0x48), kPi));
                        vec3_set_length(V(sp + 0x30), V(commander + 0xc0), 2.5f);
                        vec3_add(V(sp + 0x30), V(sp + 0x30), V(commander + 0x10));
                        hero_place(sp + 0x30, sp + 0x40, 0, 1);
                        turret_commander_end_l8(commander, 1);
                        swc1(pv + 0xa8, kHalfPi);
                        ee_stack_free(320);
                        return;
                    }
                    break;
                default:
                    break;
                }
                if (countdown_u8(pv + 0x5f) != 0 ||
                    static_cast<int8_t>(lbu(table_moby(lwu(pv + 0x80 + lbu(pv + 0x5e) * 4)) + 0x20)) < 0) {
                    sb(pv + 0x5f, 1);
                    float best = kPi;
                    int32_t pick = -1;
                    for (int32_t i = 0; i < 4; ++i) {
                        const uint32_t g = table_moby(lwu(pv + 0x80 + static_cast<uint32_t>(i) * 4));
                        const float d = angle_dist(
                            heading(lwc1(HERO_POS), lwc1(HERO_POS + 4), lwc1(g + 0x10), lwc1(g + 0x14)),
                            lwc1(HERO_ROT + 8));
                        if (d < best && lbu(g + 0x20) < 0x7f) {
                            pick = i;
                            best = angle_dist(
                                heading(lwc1(HERO_POS), lwc1(HERO_POS + 4), lwc1(g + 0x10), lwc1(g + 0x14)),
                                lwc1(HERO_ROT + 8));
                        }
                    }
                    sb(pv + 0x5e, static_cast<uint32_t>(pick));
                }
                sb(table_moby(lwu(pv + 0x80 + lbu(pv + 0x5e) * 4)) + 0xbc, 1);
            }
        }
        break;
    }
    case 6:
        moby_retire(lwu(pv + 0x90));
        moby_retire(lwu(pv + 0x94));
        moby_retire(m);
        ee_stack_free(320);
        return;
    default:
        break;
    }

    // the two gun barrels (+0x90 at 0x161b10, +0x94 at 0x161b20) and the stand (+0xa4 at 0x161bf0)
    const auto sync_barrel = [&](uint32_t slot, uint32_t local_ofs) {
        if (lwu(slot) == 0) {
            const uint32_t b = moby_spawn(0x236);
            sw(slot, b);
            if (b == 0) return;
            sb(b + 0x30, 0xff);
            sh(lwu(slot) + 0x32, 0x7e);
            sb(lwu(slot) + 0x31, 1);
            sd(lwu(slot) + 0x38, ld(m + 0x38));
            mat3_transform(V(sp + 0x20), V(local_ofs), M(m + 0xc0));
            vec3_add(V(sp + 0x20), V(pos), V(sp + 0x20));
            const uint32_t g = lwu(slot);
            lq_sq(g + 0x10, sp + 0x20);
            lq_sq(g + 0x40, rot);
            moby_update_transform(g);
        } else {
            mat3_transform(V(sp + 0x20), V(local_ofs), M(m + 0xc0));
            vec3_add(V(sp + 0x20), V(pos), V(sp + 0x20));
            const uint32_t g = lwu(slot);
            lq_sq(g + 0x10, sp + 0x20);
            lq_sq(g + 0x40, rot);
            if ((lbu(g + 0x70) & 2u) != 0 && lbu(g + 0x53) == 1) moby_anim_set(g, 0, 0, 0);
        }
    };
    sync_barrel(pv + 0x90, L8_ADDR(0x00161b10));
    sync_barrel(pv + 0x94, L8_ADDR(0x00161b20));
    if (lwu(pv + 0xa4) != 0) {
        mat3_transform(V(sp + 0x20), V(L8_ADDR(0x00161bf0)), M(m + 0xc0));
        vec3_add(V(lwu(pv + 0xa4) + 0x10), V(sp + 0x20), V(pos));
        const uint32_t stand = lwu(pv + 0xa4);
        lq_sq(stand + 0x40, rot);
        const float pitch = angle_add(lwc1(stand + 0x44), lwc1(pv + 0xa8));
        swc1(lwu(pv + 0xa4) + 0x44, pitch);
        moby_update_transform(lwu(pv + 0xa4));
    }
    ee_stack_free(320);
}

}  // namespace rac1
