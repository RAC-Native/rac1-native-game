// Ported overlay code: the R.Y.N.O. (item 0x17, hand update 0x002cc608), its target
// acquisition scan (0x002cc360), and rocket spawner (0x002ccee0).
#include <game/ovl/ryno.h>

#include <game/enhance/advanced.h>
#include <game/enhance/fixes102.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {

struct EeStackFrame {
    uint32_t addr;
    uint32_t size;
    explicit EeStackFrame(uint32_t sz) : addr(ee_stack_alloc(sz)), size(sz) {}
    ~EeStackFrame() { ee_stack_free(size); }
    EeStackFrame(const EeStackFrame&) = delete;
    EeStackFrame& operator=(const EeStackFrame&) = delete;
    operator uint32_t() const { return addr; }
};

constexpr uint32_t HERO = 0x0013f350u;
constexpr uint32_t PAD = 0x0013c940u;

// orig: (helper) shared fire-burst initiation in states 2 and 3 of 0x002cc608
void ryno_begin_burst(uint32_t m, uint32_t pv) {
    if (sub_00233db8(-1, 1) == 0) return;

    const uint32_t stats = 0x00141680u;
    const uint32_t uses = lhu(stats + 0xb8);
    if (!(0xfffeu < uses)) {
        sh(stats + 0xb8, uses + 1u);
    }
    const int32_t play_mins = ee_div(frame_scale_i(lw(0x0015eea4u)), 0x258);
    if (static_cast<int32_t>(lhu(stats + 0xba)) < play_mins) {
        const int32_t mins2 = ee_div(frame_scale_i(lw(0x0015eea4u)), 0x258);
        sh(stats + 0xba, static_cast<uint32_t>(mins2));
    }
    const uint32_t lvl_bit = 1u << (lwu(0x0015ed84u) & 31u);
    sb(HERO + 0x10ac, 2);
    sw(stats + 0xbc, lwu(stats + 0xbc) | lvl_bit | 0x80000000u);
    sb(m + 0x20, 4);
    sh(pv + 6, 0);
    sb(pv + 5, 0);
    sb(pv + 4, 0);
    hero_hold_start();
    for (int32_t i = 6; i >= 0; --i) {
        sw(pv + 0x18 + static_cast<uint32_t>(i) * 4u, 0);
    }
    sh(pv + 0x12, 0);
}

}  // namespace

// Spawns a R.Y.N.O. homing rocket (class 0x1c9) at `p1` owned by `p0` and targeting `p2`.
// orig: 0x002ccee0
void ryno_2ccee0(uint32_t p0, uint32_t p1, uint32_t p2, float pf12, float pf13, float pf14) {
    const uint32_t r = moby_spawn(0x1c9u);
    if (r == 0) return;

    const uint32_t pv = lwu(r + 0x78);
    sb(r + 0x30, 0xff);
    sh(r + 0x32, 0xff);
    sb(r + 0x31, 1);
    swc1(r + 0x48, pf13);
    swc1(r + 0x44, pf14);
    sb(r + 0x20, 0);
    sw(pv + 0x20, p0);

    const Vec4 start_pos = V(p1 & ~15u);
    V(r + 0x10) = start_pos;
    V(pv + 0x10) = start_pos;
    sw(pv + 0x34, 0);
    sw(pv + 0x38, 0);
    sw(pv + 0x30, 0);
    sw(pv + 0x28, p2);
    swc1(r + 0x2c, fpu_mul(lwc1(lwu(r + 0x24) + 0x24), 0.25f));

    if (p2 != 0) {
        sw(pv + 0x58, static_cast<uint32_t>(lh(p2 + 0xa6)));
        const uint32_t w0 = moby_pvar_word0(p2);
        if (w0 != 0) {
            sh(w0 + 0x1e, lhu(w0 + 0x1e) | 0x80u);
        }
        V(pv) = V(p2 + 0x10);
        const float tz = lwc1(pv + 8);
        swc1(pv + 0x2c, pf12);
        swc1(pv + 8, fpu_add(tz, pf12));
    }

    const float NPI = u2f(0xc0490fdbu);
    const float PI = u2f(0x40490fdbu);
    swc1(pv + 0x44, 0.0f);
    sw(pv + 0x24, static_cast<uint32_t>(frame_scale_i(0xfa)));
    swc1(pv + 0x48, rand_range_f(NPI, PI));
    swc1(pv + 0x4c, rand_range_f(NPI, PI));

    const float lim50 = fpu_mul(lwc1(0x0015ed60u), u2f(0x3c1e6eebu));
    swc1(pv + 0x50, rand_range_f(-lim50, lim50));
    const float lim54 = fpu_mul(lwc1(0x0015ed60u), u2f(0x3cc5197au));
    swc1(pv + 0x54, rand_range_f(-lim54, lim54));

    moby_update_transform(r);
    (void)moby_sound_start(1, 0, r);

    const uint32_t hero_moby = lwu(0x001413d0u);
    Vec4 from = V(hero_moby + 0x10);
    from.z = lwc1(p1 + 8);
    if (segment_collide_v(from, V(p1), 0u, hero_moby, nullptr) != 0) {
        sb(r + 0xbc, 2);
        V(r + 0x10) = V(OV(0x00173e60u));
        explosion_effect(r, 0u, 0u, 0xa, 3, 9, 0, 1, 1, -1, 0u, 0.0f, 0.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
        moby_retire(r);
    }
}

// Selects the best target moby from `0x15ffe4` within range `pf14` and angular tolerances `pf12`/`pf13`.
// orig: 0x002cc360
int32_t ryno_2cc360(uint32_t p0, uint32_t p1, uint32_t p2, float pf12, float pf13, float pf14) {
    if (gameplay102_on(4)) return fixes102_ryno_targets(p0, p1, p2, pf12, pf13, pf14);

    uint32_t best = 0;
    float best_score = u2f(0x4e6e6b28u);

    for (uint32_t cand = lwu(OV(0x0015ffe4u)); cand != 0; cand = lwu(cand + 0x28)) {
        if (lh(cand + 0x32) == 0) continue;

        if (p2 != 0) {
            bool skipped = false;
            for (uint32_t i = 0; i < 7u; ++i) {
                if (lwu(p2 + (i << 2)) == cand) {
                    skipped = true;
                    break;
                }
            }
            if (skipped) continue;
        } else {
            if (lbu(cand + 0x31) == 0) continue;
        }

        const uint32_t w0 = moby_pvar_word0(cand);
        if (w0 == 0) continue;
        if (lwc1(w0) < 0.0f) continue;
        if ((lhu(cand + 0x34) & 0x1000u) == 0) continue;

        const uint32_t cls_info = lwu(cand + 0x24);
        if (cls_info == 0 || lh(cls_info + 0x46) != 5) continue;

        const float dist2d = vec2_distance(V(p0), V(cand + 0x10));
        if (!(dist2d < pf14)) continue;

        Vec4 tpos = V(cand + 0x10);
        tpos.z = fpu_add(tpos.z, fpu_add(lwc1(w0 + 0x10), u2f(0x3dcccccd)));

        const float yaw_to = atan2_f(fpu_sub(tpos.x, lwc1(p0)), fpu_sub(tpos.y, lwc1(p0 + 4)));
        const float yaw_d = angle_dist(lwc1(p1 + 8), yaw_to);
        const float yaw_d2 = fpu_mul(yaw_d, yaw_d);
        if (!(yaw_d2 < fpu_mul(pf12, pf12))) continue;

        const float pitch_to = atan2_f(dist2d, fpu_sub(tpos.z, lwc1(p0 + 8)));
        const float pitch_d = angle_dist(lwc1(p1 + 4), pitch_to);
        const float pitch_d2 = fpu_mul(pitch_d, pitch_d);
        if (!(pitch_d2 < fpu_mul(pf13, pf13))) continue;

        const float score = !(5.0f < dist2d) ? fpu_div(dist2d, 5.0f)
                                             : fpu_add(fpu_mul(fpu_mul(yaw_d2, pitch_d2), dist2d), dist2d);
        if (!(score < best_score)) continue;
        if (segment_collide_v(V(p0), tpos, 2u, cand, nullptr) != 0) continue;

        best_score = score;
        best = cand;
    }
    return static_cast<int32_t>(best);
}

// R.Y.N.O. hand moby update.
// orig: 0x002cc608
void ryno_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);

    Mat4 cam_m{}, cam_t{};
    mat3_copy(cam_m, M(OV(0x00166c80u)));
    mat3_transpose(cam_t, cam_m);

    if (lbu(m + 0x20) - 2u < 2u) {
        bool clear_tgt = false;
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 8))) != 0) {
            clear_tgt = true;
        } else {
            const uint32_t cur = lwu(pv);
            if (cur == 0) {
                clear_tgt = true;
            } else {
                const uint32_t cst = lbu(cur + 0x20);
                if (cst == 0xfe || cst == 0xfd) clear_tgt = true;
            }
        }
        if (clear_tgt) {
            sw(pv, 0);
            sw(pv + 0xc, 0);
        }

        uint32_t found;
        {
            EeStackFrame sf(0x20);
            const uint32_t aim_addr = sf, pt_addr = sf + 0x10;
            V(aim_addr) = V(OV(0x00166dd0u));
            swc1(aim_addr + 4, -lwc1(aim_addr + 4));
            moby_chain_point_v(m, lbu(pv + 5), V(pt_addr));
            found = static_cast<uint32_t>(
                ryno_2cc360(pt_addr, aim_addr, 0u, u2f(0x3fd95d40u), u2f(0x3fd95d40u), 80.0f));
        }

        const uint32_t cur = lwu(pv);
        bool keep_cur = false;
        if (cur != 0) {
            const uint32_t cst = lbu(cur + 0x20);
            if (cst != 0xfe && cst != 0xfd && lbu(cur + 0x31) != 0) keep_cur = true;
        }
        if (!keep_cur) {
            sw(pv + 0xc, 0);
            sw(pv, found);
            sw(pv + 8, static_cast<uint32_t>(frame_scale_i(0x14)));
            swc1(pv + 0x14, 1.0f);
            const uint32_t w0 = moby_pvar_word0(lwu(pv));
            if (w0 != 0) {
                swc1(pv + 0xc, lwc1(w0 + 0x10));
                swc1(pv + 0x14, lwc1(w0));
            }
        }

        if (lwu(pv) != 0 && timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x10))) != 0 &&
            sub_00233e98(-1) != 0) {
            const uint32_t tgt = lwu(pv);
            if (tgt == 0 || lbu(tgt + 0x20) == 0xfe || lbu(tgt + 0x20) == 0xfd) {
                sw(pv, 0);
                sw(pv + 0xc, 0);
            } else {
                const float d = vec3_distance(V(OV(0x00166dc0u)), V(tgt + 0x10));
                const uint32_t tgt2 = lwu(pv);
                const float ratio = fpu_div(d, 150.0f);
                Vec4 rpos = V(tgt2 + 0x10);
                rpos.z = fpu_add(rpos.z, lwc1(pv + 0xc));
                const float scale = abs_f(fpu_sub(u2f(0x3f666666u), fpu_mul(ratio, u2f(0x3f266666u))));
                cam_effect_add(m, 0xff0fff0fu, &rpos, 0x23u, -1, scale, 0.0f, 90.0f);
            }
        }
    }

    const uint32_t st = lbu(m + 0x20);
    if (!(st < 9u)) return;

    const uint32_t target_fn = lwu(OV(0x001e9de0u) + (st << 2));
    switch (ovl_code_to_veldin(target_fn)) {
    case 0x002cc89c:  // state 0: init
        sw(pv, 0);
        sb(0x001403fcu, 0);
        sb(m + 0x20, 1);
        sh(pv + 0x10, 0);
        return;

    case 0x002cc8b8:  // state 1: equip wait
        if ((lbu(m + 0x70) & 2u) != 0) {
            sb(m + 0x20, 2);
        }
        return;

    case 0x002cc8d0:  // state 2: third-person ready
        if (lwu(HERO + 0x2084) == 1u) {
            (void)hero_set_state(0x1eu, 1);
        }
        if ((lwu(0x0013cae4u) & lwu(HERO + 0x10a0)) == 0) return;
        if (lbu(HERO + 0x20ac) != 0) return;
        ryno_begin_burst(m, pv);
        return;

    case 0x002cca0c:  // state 3: first-person scope ready
        cam_effect_add(m, 0xff0f0fffu, nullptr, 0x23u, -1, 1.0f, 0.0f, 90.0f);
        if ((lwu(PAD + 0x1a0) & 5u) == 0) {
            sb(m + 0x20, 2);
            return;
        }
        if ((lwu(PAD + 0x1a4) & lwu(HERO + 0x10a0)) == 0) return;
        if (lbu(HERO + 0x20ac) != 0) return;
        ryno_begin_burst(m, pv);
        return;

    case 0x002ccb7c: {  // state 4: firing 7-rocket burst
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 6))) == 0) return;

        sh(pv + 6, static_cast<uint32_t>(frame_scale_i(9)));
        const uint32_t barrel_inc = lbu(pv + 5) + 1u;
        sb(pv + 5, barrel_inc);
        const uint32_t barrel = (barrel_inc & 0xffu) % 9u;
        sb(pv + 4, lbu(pv + 4) + 1u);
        sb(pv + 5, barrel);

        EeStackFrame sf(0x20);
        const uint32_t pt_addr = sf, dir_addr = sf + 0x10;
        V(dir_addr) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
        if (lwu(HERO + 0x2084) == 0x1eu) {
            V(dir_addr).z = 1.0f;
            mat4_transform(V(dir_addr), V(dir_addr), cam_t);
        } else {
            V(dir_addr).x = 1.0f;
            mat3_transform(V(dir_addr), V(dir_addr), M(HERO + 0x640));
            advanced_aim_shot_hand(m, dir_addr);
        }

        Vec4 hvel{};
        vec3_add(hvel, V(0x0013f450u), V(0x0013f490u));
        vec3_scale(hvel, hvel, 3.0f);
        vec3_add(V(dir_addr), V(dir_addr), hvel);

        const float yaw =
            (lwu(HERO + 0x12b0) != 0) ? lwc1(HERO + 0x12bc) : atan2_f(lwc1(dir_addr), lwc1(dir_addr + 4));
        const float pitch = -atan2_f(vec2_length(V(dir_addr)), lwc1(dir_addr + 8));

        moby_chain_point_v(m, lbu(pv + 5), V(pt_addr));
        hero_hold_start();
        ryno_2ccee0(m, pt_addr, lwu(pv), lwc1(pv + 0xc), yaw, pitch);

        if (!(lbu(pv + 4) < 7u)) {
            sb(HERO + 0x10ac, 0);
            hero_hold_release();
            sb(m + 0x20, 5);
            sh(pv + 6, static_cast<uint32_t>(frame_scale_i(0x3c)));
            sh(pv + 0x10, static_cast<uint32_t>(frame_scale_i(0x3c)));
        }

        const float rem_hp = fpu_sub(lwc1(pv + 0x14), 1.0f);
        swc1(pv + 0x14, rem_hp);
        if (!(rem_hp <= 0.0f)) {
            const uint32_t cur = lwu(pv);
            if (cur != 0) {
                const uint32_t cst = lbu(cur + 0x20);
                if (cst != 0xfe && cst != 0xfd) return;
            }
        }

        const int32_t slot = lh(pv + 0x12);
        sw(pv + 0x18 + (static_cast<uint32_t>(slot) << 2), lwu(pv));
        const int16_t next_slot16 = static_cast<int16_t>(lhu(pv + 0x12) + 1u);
        sh(pv + 0x12, static_cast<uint32_t>(next_slot16));
        sh(pv + 0x12, static_cast<uint32_t>(ee_rem(static_cast<int32_t>(next_slot16), 7)));

        V(pt_addr) = V(OV(0x00166dd0u));
        swc1(pt_addr + 4, -lwc1(pt_addr + 4));
        const float PI = u2f(0x40490fdbu);
        const uint32_t next_tgt =
            static_cast<uint32_t>(ryno_2cc360(m + 0x10, pt_addr, pv + 0x18, PI, PI, 100.0f));
        sw(pv, next_tgt);

        if (next_tgt == 0 && lh(pv + 0x12) > 0) {
            const uint32_t r1 = static_cast<uint32_t>(libc_rand());
            if ((r1 & static_cast<uint32_t>(static_cast<int32_t>(lh(pv + 0x12)) + 1)) == 0) {
                const int32_t r2 = libc_rand();
                const int32_t n = lh(pv + 0x12);
                const int32_t pick = ee_rem(r2, n);
                sw(pv, lwu(pv + 0x18 + (static_cast<uint32_t>(pick) << 2)));
            }
        }

        sw(pv + 0xc, 0);
        sw(pv + 8, static_cast<uint32_t>(frame_scale_i(0x14)));
        swc1(pv + 0x14, 1.0f);
        const uint32_t w0 = moby_pvar_word0(lwu(pv));
        if (w0 != 0) {
            swc1(pv + 0xc, lwc1(w0 + 0x10));
            swc1(pv + 0x14, lwc1(w0));
        }
        return;
    }

    case 0x002cce94:  // state 5: post-burst cooldown
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 6))) != 0) {
            sw(pv, 0);
            sb(pv + 4, 0);
            sb(m + 0x20, 2);
        }
        return;

    case 0x002cceb0:
        return;

    default:
        TODO_PORT(0, "jr to an unknown address")();
        return;
    }
}

// orig: (none)
void ryno_fns_register() {
    ovl_register_moby_fn(0x002cc608, ryno_update);
}

}  // namespace rac1
