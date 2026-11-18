// Hoven's (level 12) classes 336 (flying bolt target on a path, with its hit / respawn helper 0x2ebca8), 339 (ambient
// creature that plays animations by distance to the hero), 384 (geyser / steam vent emitter queuing draw callback 0x2ecac0),
// and 422 (the level's group-clear and Hydro-Pack / Raritanium hint monitor). Level 12 only: `orig: L12:`, level data
// through lvd::*, Hoven-only data L12_ADDR, base ELF bare. Registered in hoven_mobys.cpp.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/hoven_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l12.h>
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

// Class 336's hit and respawn check: hit by weapon mask 0xa30000 (counting toward the 5-target skill / bonus 0x15ee04 ->
// 0x13d41d, message TXT(0x53d6), when the hero is not in turret state 0x32), drops bolts and explodes, hides for 500
// scaled frames, and reappears once off screen (`off_screen != 0`) while L12:0x1619b4 is zero.
// orig: L12:0x002ebca8
void target336_hits(uint32_t m, uint32_t off_screen) {
    const Frame f(160);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    if (sub_0025a420(m, 0x00a30000u, 0) != 0) {
        if (lw(0x001413d4) != 0x32) {
            const int32_t count = lw(0x0015ee04) + 1;
            sw(0x0015ee04, static_cast<uint32_t>(count));
            if (count >= 5 && lbu(0x0013d41d) == 0) {
                sb(0x0013d41d, 1);
                sound_play_indexed(1, 0, 0);
                sub_00263d40(TXT(0x53d6), -1);
            }
        }
        const uint32_t pos = sp + 0x20, zero = sp + 0x30;
        lq_sq(pos, m + 0x10);
        vec_clear(V(zero));
        bolts_drop(m, 0x1f, 0x4a, 2, -1);
        sw(pv + 0xa4, static_cast<uint32_t>(frame_scale_i(0x1f4)));
        sw(sp + 0x0, 1);
        sw(sp + 0x8, 0xffffffffu);
        sw(sp + 0x10, 0);
        explosion_effect(m, zero, pos, 0x14, 3, 4, -1, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 100000.0f, 3.0f, 15.0f);
    }
    sb(m + 0xa4, 0xff);
    if (lw(L12_ADDR(0x001619b4)) != 0 && off_screen != 0) {
        sw(m + 0x94, 0);
        sh(m + 0x34, (lhu(m + 0x34) | 0x41u) & 0xefffu);
    }
    if (lwu(pv + 0xa4) != 0 || (lhu(m + 0x34) & 1u) != 0) {
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xa4))) != 0 && off_screen != 0 &&
            lw(L12_ADDR(0x001619b4)) == 0) {
            const uint32_t cls = lwu(m + 0x24);
            sh(m + 0x34, (lhu(m + 0x34) & 0xffbeu) | 0x1000u);
            sw(m + 0x94, lwu(cls + 0x10));
        } else {
            sw(m + 0x94, 0);
            sh(m + 0x34, (lhu(m + 0x34) | 0x41u) & 0xefffu);
        }
    }
}

// Class 336's update: flies along path `pv + 0x74`, banking into turns and looping from the last point back to point 0.
// orig: L12:0x002ebea8
void hov_class336_update_l12(uint32_t m) {
    const Frame f(224);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    if (pv == 0) return;
    const uint32_t off_screen = moby_on_screen_l1(m, int_to_float(lh(m + 0x32))) == -1 ? 1u : 0u;
    target336_hits(m, off_screen);
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        const int32_t path_idx = lw(pv + 0x74);
        if (path_idx == -1) return;
        path_follower_init_l1(pv + 0x60);
        const uint32_t path = lvd::path(lw(pv + 0x74));
        sw(pv + 0x70, path);
        int32_t idx = lw(pv + 0xa0);
        const int32_t last = lw(path) - 1;
        if (last < idx) idx = last;
        lq_sq(m + 0x10, path + 0x10 + (static_cast<uint32_t>(idx) << 4));
        sw(pv + 0x60, static_cast<uint32_t>(idx));
        sb(pv + 0x2a, 0x10);
        sb(pv + 0x28, 2);
        sb(pv + 0x2c, 0x2c);
        swc1(pv + 0x30, u2f(0x3fb33333u));
        sb(pv + 0x2b, 1);
        sb(m + 0x20, 1);
        sh(m + 0x32, 0xaa);
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
    } else if (st == 1) {
        const uint32_t fol = pv + 0x60;
        const int32_t cur = lw(pv + 0x60);
        const uint32_t path = lwu(fol + 0x10);
        const int32_t dir = static_cast<int8_t>(lbu(fol + 4));
        const int32_t n = lw(path);
        const int32_t prev = ee_rem(cur + n - dir, n);
        const int32_t next = ee_rem(cur + n + dir, n);
        lq_sq(sp + 0x0, path + 0x10 + (static_cast<uint32_t>(prev) << 4));
        lq_sq(sp + 0x10, path + 0x10 + (static_cast<uint32_t>(cur) << 4));
        lq_sq(sp + 0x20, path + 0x10 + (static_cast<uint32_t>(next) << 4));
        vec3_sub(V(sp + 0x0), V(sp + 0x10), V(sp + 0x0));
        vec3_sub(V(sp + 0x10), V(sp + 0x20), V(sp + 0x10));
        const float yaw0 = atan2_f(lwc1(sp + 0x0), lwc1(sp + 0x4));
        const float yaw1 = atan2_f(lwc1(sp + 0x10), lwc1(sp + 0x14));
        const float curv = fpu_div(angle_sub(yaw0, yaw1), vec3_length(V(sp + 0x0)));
        const float roll_goal = fpu_mul(curv, lwc1(L12_ADDR(0x001619ac)));
        const float roll_step = fpu_div(angle_sub(roll_goal, lwc1(m + 0x40)), lwc1(L12_ADDR(0x001619b0)));
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), roll_step));
        const float dist = vec3_distance(V(m + 0x10), V(sp + 0x20));
        const float speed = fpu_mul(lwc1(L12_ADDR(0x001619a8)), lwc1(0x0015ed6c));
        if (dist < speed) {
            sw(pv + 0x60, static_cast<uint32_t>(next));
            const uint32_t p2 = lwu(fol + 0x10);
            if (next == lw(p2) - 1) lq_sq(m + 0x10, p2 + 0x10);
        }
        const uint32_t vel = sp + 0x30;
        vec3_sub(V(vel), V(sp + 0x20), V(m + 0x10));
        vec3_set_length(V(vel), V(vel), fpu_mul(lwc1(L12_ADDR(0x001619a8)), lwc1(0x0015ed6c)));
        vec3_add(V(m + 0x10), V(m + 0x10), V(vel));
        const float move_yaw = atan2_f(lwc1(vel + 0), lwc1(vel + 4));
        const float yaw_step = fpu_div(angle_sub(move_yaw, lwc1(m + 0x48)), lwc1(L12_ADDR(0x001619b0)));
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), yaw_step));
        swc1(m + 0x44, u2f(0x3f060a92u));
    }
}

// Class 339's update: ambient prop cycling between idle animations (0, 1, 2) and transition animations (3..6) on timer
// `m + 0xbc`, reacting to the hero's distance.
// orig: L12:0x002ec1d0
void hov_class339_update_l12(uint32_t m) {
    const float dist = vec2_distance(V(m + 0x10), V(0x0013f3d0));
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 0);
        sb(m + 0x20, 1);
        sb(m + 0xbc, static_cast<uint8_t>(rand_mod(frame_scale_i(0xb4))));
    } else if (st == 1) {
        if (lbu(m + 0x52) == 0 && dist < 9.0f && lbu(m + 0xbc) < 2) sb(m + 0xbc, 2);
        if (dist < 12.0f) {
            const uint32_t req = lbu(m + 0x52);
            if (req == lbu(m + 0x53) && req != 0) sb(m + 0xbc, 0);
        }
        if (countdown_u8(m + 0xbc) != 0) {
            const uint32_t req = lbu(m + 0x52);
            if (req == 0) {
                const uint32_t set = (rand_mod(0x100) & 1) != 0 ? 3u : 5u;
                if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, 0);
            } else if (req == 1) {
                if (lbu(m + 0x53) != 4) moby_anim_set(m, 4, 0, 0);
            } else if (req == 2) {
                if (lbu(m + 0x53) != 6) moby_anim_set(m, 6, 0, 0);
            }
            sb(m + 0x20, 2);
        }
    } else if (st == 2) {
        if ((lbu(m + 0x70) & 2u) != 0) {
            const uint32_t req = lbu(m + 0x52);
            if (req == 3) {
                if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, 0);
            } else if (req == 4 || req == 6) {
                if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 0);
            } else if (req == 5) {
                if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, 0);
            }
            sb(m + 0x20, 1);
            sb(m + 0xbc, static_cast<uint8_t>(rand_mod(frame_scale_i(0xb4))));
        }
    }
}

// Class 384's update: steam / geyser emitter spinning `pv + 0`, spawning particles of type 2 while unpaused, ramping height
// `pv + 8` by `m + 0xbc` flags, and queuing draw callback L12:0x2ecac0.
// orig: L12:0x002ec720
void hov_class384_update_l12(uint32_t m) {
    const Frame f(256);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sb(m + 0x20, 1);
        sb(m + 0xbc, 2);
        swc1(pv + 0x0, rand_angle());
        swc1(pv + 0x8, 5.0f);
    } else if (st == 1) {
        const uint32_t bc = lbu(m + 0xbc);
        if ((bc & 8u) != 0) {
            sb(m + 0xbc, 1);
            sw(pv + 0x4, static_cast<uint32_t>(frame_scale_i(lw(L12_ADDR(0x001619d0)))));
        } else if ((bc & 4u) != 0) {
            sb(m + 0xbc, 2);
            sw(pv + 0x4, static_cast<uint32_t>(frame_scale_i(lw(L12_ADDR(0x001619d0)))));
        }
        const float step = fpu_mul(fpu_mul(lwc1(L12_ADDR(0x001619cc)), u2f(0x3c8efa35u)), lwc1(0x0015ed6c));
        swc1(pv + 0x0, angle_add(lwc1(pv + 0x0), step));
    }

    if (lwc1(L12_ADDR(0x0015f3fc)) == 0.0f) {
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x4))) == 0) {
            float prob = fpu_div(int_to_float(lw(pv + 0x4)), int_to_float(lw(L12_ADDR(0x001619d0))));
            if ((lbu(m + 0xbc) & 1u) != 0) prob = fpu_sub(1.0f, prob);
            if (rand_range_f(0.0f, 1.0f) < prob) {
                const float r = lwc1(L12_ADDR(0x001619d4));
                rand_sphere_point(sp + 0x10, 0.0f, r);
                rand_sphere_point(sp + 0x20, 0.0f, r);
                rand_sphere_point(sp + 0x30, 0.0f, r);
                const uint32_t v1 = sp + 0x40, v2 = sp + 0x50;
                vec3_sub(V(v1), V(sp + 0x10), V(sp + 0x20));
                swc1(v1 + 8, fpu_add(lwc1(v1 + 8), fpu_sub(lwc1(L12_ADDR(0x00161a00)), lwc1(L12_ADDR(0x001619fc)))));
                vec3_sub(V(v2), V(sp + 0x20), V(sp + 0x30));
                const float life_f = static_cast<float>(lw(L12_ADDR(0x001619ec)));
                const float inv = fpu_div(2.0f, life_f);
                swc1(v2 + 8, fpu_add(lwc1(v2 + 8), fpu_sub(lwc1(L12_ADDR(0x00161a04)), lwc1(L12_ADDR(0x00161a00)))));
                vec3_scale(V(v1), V(v1), inv);
                vec3_scale(V(v2), V(v2), inv);
                vec3_add(V(sp + 0x10), V(sp + 0x10), V(m + 0x10));
                if ((lbu(m + 0xbc) & 1u) != 0) {
                    const float z0 = lwc1(sp + 0x18);
                    swc1(v1 + 8, -lwc1(v1 + 8));
                    swc1(v2 + 8, -lwc1(v2 + 8));
                    swc1(sp + 0x18, fpu_add(z0, lwc1(L12_ADDR(0x00161a04))));
                } else {
                    swc1(sp + 0x18, fpu_add(lwc1(sp + 0x18), lwc1(L12_ADDR(0x001619fc))));
                }
                swc1(v1 + 0xc, rand_range_f(lwc1(L12_ADDR(0x001619e0)), lwc1(L12_ADDR(0x001619e4))));
                swc1(v2 + 0xc, rand_range_f(lwc1(L12_ADDR(0x001619e0)), lwc1(L12_ADDR(0x001619e4))));
                const uint32_t life = static_cast<uint32_t>(frame_scale_i(lw(L12_ADDR(0x001619ec))));
                const int32_t tex = lw(L12_ADDR(0x001619f8));
                const uint32_t col = lwu(L12_ADDR(0x001619f4));
                sw(sp + 0x0, static_cast<uint32_t>(tex));
                spawn_particle_02(sp + 0x10, v1, v2, col, col, lwu(L12_ADDR(0x001619e8)), life, lwu(L12_ADDR(0x001619f0)), tex);
            }
        }
        const uint32_t bc = lbu(m + 0xbc);
        if ((bc & 1u) != 0 && lwu(pv + 0x4) == 0) {
            approach_f(pv + 0x8, -1.0f, fpu_mul(lwc1(L12_ADDR(0x00161a08)), lwc1(0x0015ed6c)));
        } else if ((bc & 2u) != 0) {
            approach_f(pv + 0x8, 3.0f, fpu_mul(lwc1(L12_ADDR(0x00161a08)), lwc1(0x0015ed6c)));
        }
    }
    sub_001f93e0(L12_ADDR(0x002ecac0), m);
}

// Class 422's update: checks group `pv + 0xc` for completion (flag 0x13d41c, message TXT(0x53d6)), and triggers HelpDesk
// hints for underwater swimming (0x2ee3, 0x2ee4) and the Raritanium man (0x2ee0 once the Hydro-Pack is obtained).
// orig: L12:0x002ed280
void hov_class422_update_l12(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (moby_group_count_l1(lw(pv + 0xc), -1) == 0 && lbu(0x0013d41c) == 0) {
        sb(0x0013d41c, 1);
        sound_play_indexed(1, 0, 0);
        sub_00263d40(TXT(0x53d6), -1);
    }

    const uint32_t c0 = lhu(0x00141cd0);
    if (c0 == 0) {
        if (lw(0x001413d4) == 0x81) sh(0x00141cd0, c0 + 1u);
        const int32_t mins = ee_div(frame_scale_i(lw(0x0015eea4)), 0x258);
        if (static_cast<int32_t>(lhu(0x00141cd2)) < mins) {
            sh(0x00141cd2, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 0x258)));
        }
        const uint32_t lvl = lwu(0x0015ed84);
        sw(0x00141cd4, lwu(0x00141cd4) | (1u << (lvl & 31u)) | 0x80000000u);
        if (point_in_box(0x0013f3d0, lw(pv + 0x0)) != 0) sub_00203908(TXT(0x2ee3), 0x6d);
    }

    const uint32_t h1 = lhu(0x00141cd8);
    const bool check_swim =
        h1 == 0 || (h1 < 2 && frame_scale_i(0x00011940) < frame_scale_i(lw(0x0015eea4)) - static_cast<int32_t>(lhu(0x00141cda)) * 0x258);
    if (check_swim && lw(0x001413d4) == 0x81) {
        sw(pv + 0x4, lwu(pv + 0x4) + 1u);
        if ((lwu(0x0013cae0) & 3u) != 0 && u2f(0x3e99999au) < lwc1(0x001415ec)) {
            sw(pv + 0x8, lwu(pv + 0x8) + 1u);
        } else {
            sw(pv + 0x8, 0);
        }
        if (frame_scale_i(0x28) < lw(pv + 0x8)) {
            sw(pv + 0x4, 0);
            const uint32_t cur = lhu(0x00141cd8);
            if (cur <= 0xfffeu) sh(0x00141cd8, cur + 1u);
            const int32_t mins = ee_div(frame_scale_i(lw(0x0015eea4)), 0x258);
            if (static_cast<int32_t>(lhu(0x00141cda)) < mins) {
                sh(0x00141cda, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 0x258)));
            }
            const uint32_t lvl = lwu(0x0015ed84);
            sw(0x00141cdc, lwu(0x00141cdc) | (1u << (lvl & 31u)) | 0x80000000u);
        }
        const int32_t step = frame_scale_i(0x1c20);
        const int32_t base = frame_scale_i(0x708);
        if (static_cast<int32_t>(lhu(0x00141cd8)) * step + base < lw(pv + 0x4)) {
            sub_00203908(TXT(0x2ee4), 0x6e);
        }
    }

    if (lbu(0x0013d4c4) != 0 && lhu(0x00141b58) == 0) {
        sub_00203908(TXT(0x2ee0), 0x3e);
    }
}

}  // namespace rac1
