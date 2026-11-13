// Ported overlay code: the Devastator (item 0x0b, hand class 0x9d), its smoke particle
// spawner (0x002712b8), rocket spawner (0x002afc70), and muzzle/exhaust effects.
#include <game/ovl/devastator.h>

#include <game/enhance/advanced.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapons.h>

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

// orig: (helper) host-Vec4 form of 0x002712b8
int32_t dev_2712b8_v(const Vec4& pos, const Vec4& vel, uint32_t life, uint32_t alpha, uint32_t rgb, uint32_t rot_rate,
                     float size, float grow_rate, float damp_rate, float grav, float carry) {
    const uint32_t p = alloc_particle(0x2cu);
    if (p == 0) return 0;

    V(p + 0x10) = pos;
    swc1(p + 0x1c, carry);
    sw(p + 4, (alpha << 24) | rgb);

    const uint32_t ext = p + 0x20;
    const uint32_t flags9 = static_cast<uint32_t>(float_to_int(4.0f)) + 0x40u;
    swc1(p + 0xc, size);
    sb(p + 1, 0);
    sb(p + 3, 0x48);
    sb(p + 9, flags9);
    sb(p + 8, static_cast<uint32_t>(rand_mod(0xff)));

    const float rot_f = static_cast<float>(static_cast<int32_t>(rot_rate));
    const uint32_t tex = lbu(lwu(OV(0x001b2130u)));
    sh(p + 0xa, life);
    sb(p + 2, tex);

    const float dt = lwc1(0x0015ed60u);
    swc1(p + 0x20, vel.x);
    const float rot_scaled = fpu_mul(rot_f, dt);
    swc1(ext + 4, vel.y);
    swc1(ext + 8, vel.z);
    sb(ext + 0x10, static_cast<uint32_t>(float_to_int(rot_scaled)));

    const float dt2 = lwc1(0x0015ed60u);
    swc1(ext + 0xc, grav);
    const float damp = fpu_add(fpu_mul(fpu_sub(damp_rate, 1.0f), dt2), 1.0f);
    const float grow = fpu_mul(grow_rate, dt2);
    sb(ext + 0x12, static_cast<uint32_t>(static_cast<int32_t>(rgb) >> 8));
    sb(ext + 0x13, static_cast<uint32_t>(static_cast<int32_t>(rgb) >> 16));
    sh(ext + 0x1c, life);
    sh(ext + 0x1e, alpha);
    swc1(ext + 0x14, grow);
    sb(ext + 0x11, rgb);
    swc1(ext + 0x18, damp);
    return static_cast<int32_t>(p);
}

// orig: (helper) host-Vec4 form of 0x002afc70
void dev_2afc70_v(uint32_t owner, const Vec4& pos, uint32_t target, float aim_yaw, float aim_pitch, float z_off,
                  float rot_yaw, float rot_pitch) {
    const uint32_t r = moby_spawn(0x99u);
    if (r == 0) return;

    const uint32_t pv = lwu(r + 0x78);
    sb(r + 0x30, 0xff);
    sh(r + 0x32, 0xff);
    sb(r + 0x31, 1);
    swc1(r + 0x48, rot_yaw);
    swc1(r + 0x44, rot_pitch);
    sb(r + 0x20, 0);
    sw(pv + 0x10, owner);
    V(r + 0x10) = pos;

    const float max_spd = fpu_mul(lwc1(0x0015ed6cu), 20.0f);
    sw(pv + 0x38, 1u);
    swc1(pv + 0x48, 70.0f);
    const float init_spd = fpu_div(max_spd, 3.0f);
    swc1(pv + 0x34, max_spd);
    sw(pv + 0x24, 0);
    sw(pv + 0x28, 0);
    sw(pv + 0x18, target);
    swc1(pv + 0x20, init_spd);

    if (target != 0) {
        const uint32_t w0 = moby_pvar_word0(target);
        if (w0 != 0) {
            const uint32_t flags = lhu(w0 + 0x1e) | 0x80u;
            const uint32_t no_lead = lbu(w0 + 0xb);
            sh(w0 + 0x1e, flags);
            if (no_lead != 0) sw(pv + 0x38, 0);

            const uint32_t custom_spd = lbu(w0 + 0xc);
            const float gs = lwc1(0x0015ed6cu);
            if (custom_spd != 0) {
                swc1(pv + 0x34, fpu_mul(static_cast<float>(static_cast<int32_t>(custom_spd)), gs));
            }
            if (fpu_mul(lwc1(0x0015ed6cu), 20.0f) < lwc1(pv + 0x34)) {
                swc1(pv + 0x48, 140.0f);
            }
        }
    }
    swc1(pv + 0x2c, aim_yaw);
    swc1(pv + 0x30, aim_pitch);
    sw(pv + 0x14, static_cast<uint32_t>(frame_scale_i(0x12c)));

    if (target != 0) {
        V(pv) = V(target + 0x10);
        const float tz = lwc1(pv + 8);
        swc1(pv + 0x1c, z_off);
        swc1(pv + 8, fpu_add(tz, z_off));
    }

    const int32_t trail_base =
        (fpu_mul(lwc1(0x0015ed6cu), u2f(0x3dcccccd)) < vec3_length(V(0x0013f490u))) ? 0x3c : 0x0f;
    const uint32_t trail_frames = static_cast<uint32_t>(frame_scale_i(trail_base));
    sh(pv + 0x3c, trail_frames);
    sh(pv + 0x3e, trail_frames);

    Vec4 origin{};
    bool owner_alive = false;
    if (owner != 0) {
        const uint32_t ost = lbu(owner + 0x20);
        if (ost != 0xfe && ost != 0xfd) owner_alive = true;
    }
    if (owner_alive) {
        origin = V(owner + 0x10);
    } else {
        origin = V(HERO + 0x80);
    }
    origin.z = pos.z;

    if (segment_collide_v(origin, pos, 0u, lwu(0x001413d0u), nullptr) != 0) {
        sb(r + 0xbc, 1);
        V(r + 0x10) = V(OV(0x00173e60u));
    }

    moby_update_transform(r);
    sub_0025f780(r, OV(0x001b0770u), 0x1f);
    const float sc = lwc1(r + 0x2c);
    swc1(r + 0x2c, fpu_add(sc, sc));
}

}  // namespace

// Returns the active Devastator rocket targeting moby `p0` from list 0x1b0770, or 0 if none.
// orig: 0x0025f878
int32_t dev_25f878(uint32_t p0) {
    const int32_t count = lh(OV(0x001b0770u));
    if (count <= 0) return 0;

    const uint32_t list = OV(0x001b0770u);
    const uint32_t moby_base = lwu(OV(0x0015ffd8u));
    for (int32_t i = 1; !(count < i); ++i) {
        const int32_t idx = lh(list + static_cast<uint32_t>(i) * 2u);
        const uint32_t r = moby_base + (static_cast<uint32_t>(idx) << 8);
        const uint32_t rpv = lwu(r + 0x78);
        if (lwu(rpv + 0x18) == p0) return static_cast<int32_t>(r);
    }
    return 0;
}

// Integrates `p0` frames of initial rocket acceleration to estimate lead distance.
// orig: 0x002aff50
float dev_2aff50(uint32_t p0) {
    int32_t n = static_cast<int32_t>(p0);
    if (n <= 0) return 0.0f;

    float dist = 0.0f;
    float spd = 0.0f;
    const float max_spd = fpu_mul(lwc1(0x0015ed6cu), 20.0f);
    const float dt = lwc1(0x0015ed64u);
    do {
        const float step = fpu_mul(fpu_div(fpu_sub(max_spd, spd), 10.0f), dt);
        spd = fpu_add(spd, step);
        --n;
        dist = fpu_add(dist, spd);
    } while (n != 0);
    return dist;
}

// Spawns a type-0x2c smoke particle at `p0` with velocity `p1`.
// orig: 0x002712b8
int32_t dev_2712b8(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, float pf12, float pf13,
                   float pf14, float pf15, float pf16) {
    const Vec4 pos = V(p0 & ~15u);
    const Vec4 vel{lwc1(p1), lwc1(p1 + 4), lwc1(p1 + 8), 0.0f};
    return dev_2712b8_v(pos, vel, p2, p3, p4, p5, pf12, pf13, pf14, pf15, pf16);
}

// Spawns a Devastator rocket (class 0x99) at `p1` owned by `p0` and targeting `p2`.
// orig: 0x002afc70
void dev_2afc70(uint32_t p0, uint32_t p1, uint32_t p2, float pf12, float pf13, float pf14, float pf15, float pf16) {
    dev_2afc70_v(p0, V(p1 & ~15u), p2, pf12, pf13, pf14, pf15, pf16);
}

// Post-shot exhaust smoke & spark vent while `m + 0xbc` counts down.
// orig: 0x002b1af0
void dev_2b1af0(uint32_t p0) {
    if (lbu(p0 + 0xbc) == 0) return;

    Vec4 off{u2f(0x3e6b851fu), u2f(0x3e3851ecu), 0.0f, 1.0f};
    Vec4 sp0{0.0f, 1.0f, 0.0f, 1.0f};
    (void)countdown_u8(p0 + 0xbc);

    mat3_transform(off, off, M(p0 + 0xc0));
    Vec4 emit_pos{};
    vec3_add(emit_pos, V(p0 + 0x10), off);
    mat3_transform(sp0, sp0, M(p0 + 0xc0));

    // Note: sp0 is overwritten inside iteration 0 and reused in iteration 1 and the spark below.
    for (int32_t k = 1; k >= 0; --k) {
        const float rlen = rand_range_f(u2f(0x3c23d70au), u2f(0x3d75c28fu));
        Vec4 vel{};
        vec3_set_length(vel, sp0, rlen);
        vec3_scale(sp0, V(0x0013f450u), u2f(0x3f333333u));
        sp0.z = 0.0f;
        vec3_add(vel, vel, sp0);
        vec3_scale(sp0, V(0x0013f490u), u2f(0x3f333333u));
        sp0.z = 0.0f;
        vec3_add(vel, vel, sp0);

        const uint32_t rot = static_cast<uint32_t>(float_to_int(rand_range_signed_f(0.0f, 6.0f)));
        const uint32_t life = static_cast<uint32_t>(frame_scale_i(0x1e));
        (void)dev_2712b8_v(emit_pos, vel, life, 0x7fu, 0x00ffffffu, rot, u2f(0x46c35000u), u2f(0x43960000u),
                           u2f(0x3f7ae148u), 0.0f, u2f(0x3f59999au));
    }

    for (int32_t k = 0; k >= 0; --k) {
        const float rlen = rand_range_f(u2f(0x3c23d70au), u2f(0x3d75c28fu));
        Vec4 vel{};
        vec3_set_length(vel, sp0, rlen);
        vec3_scale(sp0, V(0x0013f450u), u2f(0x3f0ccccdu));
        sp0.z = 0.0f;
        vec3_add(vel, vel, sp0);
        vec3_scale(sp0, V(0x0013f490u), u2f(0x3f0ccccdu));
        sp0.z = 0.0f;
        vec3_add(vel, vel, sp0);

        const int32_t lo_life = frame_scale_i(0x14);
        const int32_t hi_life = frame_scale_i(0x28);
        const uint32_t life = static_cast<uint32_t>(rand_range(lo_life, hi_life));
        (void)spawn_particle_15_v(emit_pos, vel, 0x4f007fffu, 0x1fffffffu, life, 1u, u2f(0x461c4000u));
    }
}

// Backblast / muzzle ring burst on hip-fire shot; starts the 5-frame vent timer at `m + 0xbc`.
// orig: 0x002b1de0
void dev_2b1de0(uint32_t p0) {
    Vec4 off{u2f(0x3e99999au), u2f(0xbf99999au), 0.0f, 1.0f};
    mat3_transform(off, off, M(p0 + 0xc0));
    Vec4 emit_pos{};
    vec3_add(emit_pos, V(p0 + 0x10), off);

    Vec4 vel{};
    for (int32_t i = 0; i < float_to_int(24.0f); ++i) {
        const float jitter = rand_range_signed_f(0.0f, u2f(0x3e060a92u));
        const float ang = fpu_add(fpu_mul(static_cast<float>(i), u2f(0x3e860a92u)), jitter);
        vec_rotate_axis(vel, V(0x0013f9b0u), V(0x0013f990u), ang);
        vec3_set_length(vel, vel, rand_range_f(u2f(0x3d4ccccdu), u2f(0x3dcccccd)));

        int32_t rot = rand_range(0, 3);
        if (rand_mod(2) != 0) rot = -rot;
        if (lwu(0x0013f650u) != 0) {
            Vec4 hvel{};
            vec3_scale(hvel, V(0x0013f450u), u2f(0x3fd9999au));
            vec3_add(vel, vel, hvel);
        }

        const int32_t lo_life = frame_scale_i(0x1e);
        const int32_t hi_life = frame_scale_i(0x5a);
        const uint32_t life = static_cast<uint32_t>(rand_range(lo_life, hi_life));
        (void)dev_2712b8_v(emit_pos, vel, life, 0x1eu, 0x00ffffffu, static_cast<uint32_t>(rot), u2f(0x476a6000u),
                           u2f(0x453b8000u), u2f(0x3f59999au), u2f(0xba83126fu), 0.0f);
    }

    const float step6 = u2f(0x3f860a92u);
    for (int32_t i = 0; i < float_to_int(6.0f); ++i) {
        const float jitter = rand_range_f(0.0f, u2f(0x3f490fdbu));
        const float ang = fpu_add(fpu_mul(static_cast<float>(i), step6), jitter);
        vec_rotate_axis(vel, V(0x0013f9b0u), V(0x0013f990u), ang);
        vec3_set_length(vel, vel, rand_range_f(u2f(0x3d4ccccdu), u2f(0x3dcccccd)));

        const int32_t lo_life = frame_scale_i(0x14);
        const int32_t hi_life = frame_scale_i(0x3c);
        const uint32_t life = static_cast<uint32_t>(rand_range(lo_life, hi_life));
        (void)spawn_particle_15_v(emit_pos, vel, 0x4f007fffu, 0x1fffffffu, life, 1u, u2f(0x461c4000u));
    }

    sb(p0 + 0xbc, static_cast<uint32_t>(frame_scale_i(5)));
}

// Devastator hand moby update (class 0x9d).
// orig: 0x002b2100
void devastator_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    dev_2b1af0(m);

    float z_off = 0.75f;
    float best_dist = u2f(0x461c4000u);

    if (lbu(m + 0x20) == 0) {
        sb(m + 0x20, 1);
        sh(pv + 0x1c, 0xffffu);
        sh(pv + 0x1e, 0);
    }

    Mat4 cam_m{}, cam_t{};
    mat3_copy(cam_m, M(OV(0x00166c80u)));
    mat3_transpose(cam_t, cam_m);

    (void)timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv)));
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x14))) != 0) {
        sw(pv + 0x10, 0);
    } else {
        const uint32_t locked = lwu(pv + 0x10);
        if (locked != 0 && (lbu(locked + 0x20) & 0x80u) != 0) {
            sw(pv + 0x10, 0);
        }
    }

    uint32_t hstate = lwu(HERO + 0x2084);
    if (hstate == 1u) {
        (void)hero_set_state(0x1eu, 1);
        hstate = lwu(HERO + 0x2084);
    }

    // First-person scope aiming raycast & reticle
    if (hstate == 0x1eu && (lhu(lwu(HERO + 0x2080) + 0x34) & 1u) != 0 && lbu(HERO + 0x20ac) == 0) {
        Vec4 ray_dir{0.0f, 0.0f, 70.0f, 0.0f};
        mat4_transform(ray_dir, ray_dir, cam_t);
        const Vec4& cam_pos = V(OV(0x00166dc0u));
        Vec4 ray_end{};
        vec3_add(ray_end, cam_pos, ray_dir);

        if (segment_collide_v(cam_pos, ray_end, 0u, m, nullptr) != 0) {
            const uint32_t outb = OV(0x00173e40u);
            const uint32_t hit = lwu(outb + 0x18);
            if (hit != 0 && (lhu(hit + 0x34) & 0x1000u) != 0) {
                sw(pv + 0x10, hit);
                sw(pv + 0x14, static_cast<uint32_t>(frame_scale_i(0x14)));
                const uint32_t locked = lwu(pv + 0x10);
                swc1(pv + 0x18, fpu_sub(lwc1(outb + 0x28), lwc1(locked + 0x18)));
            }
        }

        const int32_t period = frame_scale_i(0xf0);
        const int32_t tick_rem = ee_rem(lw(OV(0x0015f5ccu)), period);
        const float deg_per_period = frame_scale_f(240.0f);
        const float spin =
            angle_wrap(fpu_mul(fpu_div(fpu_mul(int_to_float(tick_rem), 360.0f), deg_per_period), u2f(0x3c8efa35u)));
        cam_effect_add(m, 0xff180a65u, nullptr, 0x27u, -1, 1.0f, spin, 90.0f);

        if (lwu(pv + 0x10) != 0 && lwu(pv) == 0 && sub_00233e98(-1) != 0) {
            const float d = vec3_distance(V(OV(0x00166dc0u)), V(lwu(pv + 0x10) + 0x10));
            const uint32_t locked = lwu(pv + 0x10);
            const float ratio = fpu_div(d, 100.0f);
            Vec4 lock_pt = V(locked + 0x10);
            lock_pt.z = fpu_add(lock_pt.z, lwc1(pv + 0x18));
            const float lock_scale = abs_f(fpu_sub(u2f(0x3f666666u), fpu_mul(ratio, u2f(0x3f11eb85u))));

            const int32_t period2 = frame_scale_i(0xf0);
            const int32_t tick_rem2 = ee_rem(lw(OV(0x0015f5ccu)), period2);
            const float deg_per_period2 = frame_scale_f(240.0f);
            const float spin2 = angle_wrap(
                fpu_mul(fpu_div(fpu_mul(int_to_float(tick_rem2), -360.0f), deg_per_period2), u2f(0x3c8efa35u)));
            cam_effect_add(m, 0xff0fff0fu, &lock_pt, 0x27u, -1, lock_scale, spin2, 90.0f);
        }
    }

    // Hero external lock-on target override
    if (lwu(HERO + 0x12b0) != 0) {
        sw(pv + 0x10, lwu(HERO + 0x12b8));
        sw(pv + 0x14, static_cast<uint32_t>(frame_scale_i(0x14)));
        const uint32_t locked = lwu(pv + 0x10);
        swc1(pv + 0x18, fpu_sub(lwc1(OV(0x00173e68u)), lwc1(locked + 0x18)));
    }

    // Firing check
    if (lw(OV(0x0015f5c4u)) != 2 && lwu(pv) == 0 && lbu(HERO + 0x20ac) == 0 && lbu(m + 0x52) < 2u &&
        (lwu(0x0013cae0u) & lwu(HERO + 0x10a0)) != 0 && frame_scale_i(0x16) < lw(HERO + 0x10b0)) {
        if (lbu(m + 0x53) == 0) {
            moby_anim_set(m, 1u, 0, frame_scale_i(4));
        }

        Vec4 shot_dir{0.0f, 0.0f, 0.0f, 0.0f};
        const float fwd_len = fpu_mul(lwc1(0x0015ed6cu), 10.0f);
        if (lwu(HERO + 0x2084) == 0x1eu && (lhu(lwu(HERO + 0x2080) + 0x34) & 1u) != 0) {
            shot_dir.z = fwd_len;
            mat4_transform(shot_dir, shot_dir, cam_t);
        } else {
            shot_dir.x = fwd_len;
            mat3_transform(shot_dir, shot_dir, M(HERO + 0x640));
            if (advanced_aiming()) {
                EeStackFrame sf(0x10);
                V(sf) = shot_dir;
                advanced_aim_shot_hand(m, sf);
                shot_dir = V(sf);
            }
        }

        if (sub_00233db8(-1, 1) == 0) {
            (void)moby_sound_start(1, 0, m);
        } else {
            float aim_yaw = (lwu(HERO + 0x12b0) != 0) ? lwc1(HERO + 0x12bc) : atan2_f(shot_dir.x, shot_dir.y);
            const float init_yaw = aim_yaw;
            float aim_pitch = -atan2_f(vec2_length(shot_dir), shot_dir.z);
            float init_pitch = aim_pitch;
            uint32_t best_cand = 0;
            Vec4 muzzle{};

            if (lwu(HERO + 0x2084) == 0x1eu && (lhu(lwu(HERO + 0x2080) + 0x34) & 1u) != 0) {
                vec_clear(muzzle);
                muzzle.x = fpu_add(muzzle.x, u2f(0x3e19999au));
                muzzle.z = fpu_add(muzzle.z, 0.75f);
                muzzle.y = fpu_add(muzzle.y, u2f(0x3e99999au));
                mat4_transform(muzzle, muzzle, cam_t);
                vec3_add(muzzle, muzzle, V(OV(0x00166dc0u)));
                if (lwu(pv + 0x10) != 0) {
                    z_off = lwc1(pv + 0x18);
                }
            } else {
                moby_chain_point_v(m, 0u, muzzle);
                for (uint32_t idx = 0;; ++idx) {
                    const uint32_t cand = lwu(OV(0x001aba00u) + (idx << 2));
                    if (cand == 0) break;
                    if (static_cast<int8_t>(lbu(cand + 0x20)) < 0) continue;

                    Vec4 cpos = V(cand + 0x10);
                    const uint32_t w0 = moby_pvar_word0(cand);
                    cpos.z = fpu_add(cpos.z, w0 != 0 ? lwc1(w0 + 0x10) : 0.5f);

                    if ((lhu(cand + 0x34) & 0x1000u) == 0) continue;
                    const uint32_t cls_info = lwu(cand + 0x24);
                    if (cls_info == 0 || lh(cls_info + 0x46) != 5) continue;

                    const float yaw_to = atan2_f(fpu_sub(cpos.x, muzzle.x), fpu_sub(cpos.y, muzzle.y));
                    const float pitch_to = atan2_f(vec2_distance(muzzle, cpos), fpu_sub(cpos.z, muzzle.z));
                    const float dist3d = vec3_distance(muzzle, cpos);

                    if (dist3d < 2.5f) {
                        const float hero_to =
                            atan2_f(fpu_sub(lwc1(cand + 0x10), lwc1(HERO + 0x80)),
                                    fpu_sub(lwc1(cand + 0x14), lwc1(HERO + 0x84)));
                        const uint32_t hero_moby = lwu(HERO + 0x2080);
                        if (angle_dist(lwc1(hero_moby + 0x48), hero_to) < u2f(0x3f860a92u) &&
                            abs_f(pitch_to) < u2f(0x3f490fdbu)) {
                            best_dist = dist3d;
                            aim_pitch = -pitch_to;
                            best_cand = cand;
                            aim_yaw = yaw_to;
                            break;
                        }
                    }

                    if (best_dist < dist3d) continue;

                    const float gold_cone =
                        fpu_mul(int_to_float(static_cast<int32_t>(lbu(0x0013e52bu))), u2f(0x3f32b8c2u));
                    const float pitch_adj = fpu_add(aim_pitch, fpu_mul(lwc1(HERO + 0x2e4), 0.5f));
                    Vec4 cone_pt{};
                    vec_from_spherical_v(cone_pt, dist3d, aim_yaw, pitch_adj);
                    vec3_add(cone_pt, cone_pt, muzzle);

                    const float chord = vec3_distance(cone_pt, cpos);
                    const float half_ang = asin_f(fpu_div(chord, fpu_add(dist3d, dist3d)));
                    float cone_ang = fpu_add(half_ang, half_ang);
                    float max_cone = fpu_add(gold_cone, u2f(0x3e32b8c2u));

                    if (max_cone < cone_ang && w0 != 0) {
                        const float rad = fpu_mul(int_to_float(static_cast<int32_t>(lbu(w0 + 0xa))), 0.125f);
                        if (rad < dist3d) {
                            const float sub_ang = asin_f(fpu_div(rad, dist3d));
                            cone_ang = fpu_sub(cone_ang, sub_ang);
                            if (cone_ang < 0.0f) cone_ang = 0.0f;
                        }
                        max_cone = fpu_add(gold_cone, u2f(0x3e32b8c2u));
                    }

                    if (!(cone_ang < max_cone)) continue;
                    if (segment_collide_v(V(OV(0x00166dc0u)), cpos, 6u, m, nullptr) != 0) continue;

                    best_cand = cand;
                    best_dist = dist3d;
                    aim_pitch = -pitch_to;
                    aim_yaw = yaw_to;
                    if (dev_25f878(best_cand) != 0) continue;

                    z_off = 0.5f;
                    sw(pv + 0x10, best_cand);
                    if (w0 != 0) z_off = lwc1(w0 + 0x10);
                }
            }

            (void)moby_sound_start(0, 0, m);
            sh(pv + 0x1e, static_cast<uint32_t>(frame_scale_i(5) + frame_scale_i(0xa)));
            if (lwu(HERO + 0x2084) != 0x1eu) {
                dev_2b1de0(m);
            }

            // Weapon usage statistics (slot +0x58 at 0x00141680)
            const uint32_t stats = 0x00141680u;
            const uint32_t uses = lhu(stats + 0x58);
            if (!(0xfffeu < uses)) {
                sh(stats + 0x58, uses + 1u);
            }
            const int32_t play_mins = ee_div(frame_scale_i(lw(0x0015eea4u)), 0x258);
            if (static_cast<int32_t>(lhu(stats + 0x5a)) < play_mins) {
                const int32_t mins2 = ee_div(frame_scale_i(lw(0x0015eea4u)), 0x258);
                sh(stats + 0x5a, static_cast<uint32_t>(mins2));
            }
            const uint32_t lvl_bit = 1u << (lwu(0x0015ed84u) & 31u);
            sw(stats + 0x5c, lwu(stats + 0x5c) | lvl_bit | 0x80000000u);

            const bool strafe_lock = (lh(HERO + 0x22c8) == 1 && lwu(HERO + 0x2084) != 0x1eu);
            if (strafe_lock || best_dist < 2.0f) {
                if (strafe_lock) {
                    aim_pitch = lwc1(OV(0x0017acf4u));
                    aim_yaw = lwc1(m + 0x48);
                }
                const uint32_t w0 = moby_pvar_word0(lwu(pv + 0x10));
                if (w0 != 0) z_off = lwc1(w0 + 0x10);
                dev_2afc70_v(m, muzzle, lwu(pv + 0x10), aim_yaw, aim_pitch, z_off, aim_yaw, aim_pitch);
            } else {
                if (lwu(pv + 0x10) == 0) {
                    if (best_cand != 0) {
                        best_dist = dev_2aff50(6u);
                        Vec4 cpos = V(best_cand + 0x10);
                        const uint32_t w0 = moby_pvar_word0(best_cand);
                        cpos.z = fpu_add(cpos.z, w0 != 0 ? lwc1(w0 + 0x10) : 0.5f);

                        init_pitch = fpu_sub(init_pitch, fpu_mul(lwc1(HERO + 0x2e4), 0.5f));
                        Vec4 lead_pt{};
                        vec_from_spherical_v(lead_pt, best_dist, init_yaw, init_pitch);
                        vec3_add(lead_pt, lead_pt, muzzle);

                        aim_yaw = atan2_f(fpu_sub(cpos.x, lead_pt.x), fpu_sub(cpos.y, lead_pt.y));
                        aim_pitch = -atan2_f(vec2_distance(lead_pt, cpos), fpu_sub(cpos.z, lead_pt.z));
                    } else if (lwu(HERO + 0x2084) != 0x1eu) {
                        const float half_tilt = fpu_mul(lwc1(HERO + 0x2e4), 0.5f);
                        init_pitch = fpu_sub(init_pitch, half_tilt);
                        aim_pitch = fpu_sub(aim_pitch, half_tilt);
                    }
                }
                dev_2afc70_v(m, muzzle, lwu(pv + 0x10), aim_yaw, aim_pitch, z_off, init_yaw, init_pitch);
            }

            if (lwu(HERO + 0x208c) == 0 || (lwu(HERO + 0x2084) == 2u && lwc1(HERO + 0x229c) < u2f(0x3f333333u))) {
                hero_anim_set(0x36u, 1u, static_cast<float>(frame_scale_i(0xa)));
            } else {
                hero_hold_start();
            }
        }
        sw(pv, static_cast<uint32_t>(frame_scale_i(0x23)));
    }

    // Dynamic muzzle flash light management
    if (lbu(m + 0x52) != 2 && lh(pv + 0x1e) > 0) {
        const int32_t t_rem = lh(pv + 0x1e);
        float r, g, b;
        if (frame_scale_i(0xa) < t_rem) {
            const float num1 = int_to_float((frame_scale_i(5) + frame_scale_i(0xa)) - lh(pv + 0x1e));
            r = fpu_div(num1, static_cast<float>(frame_scale_i(5)));
            const float num2 = int_to_float((frame_scale_i(5) + frame_scale_i(0xa)) - lh(pv + 0x1e));
            g = fpu_div(fpu_mul(num2, u2f(0x3f4ccccdu)), static_cast<float>(frame_scale_i(5)));
            const float num3 = int_to_float((frame_scale_i(5) + frame_scale_i(0xa)) - lh(pv + 0x1e));
            b = fpu_div(fpu_mul(num3, 0.5f), static_cast<float>(frame_scale_i(5)));
        } else {
            const float num1 = int_to_float(lh(pv + 0x1e));
            r = fpu_div(num1, static_cast<float>(frame_scale_i(0xa)));
            const float num2 = int_to_float(lh(pv + 0x1e));
            g = fpu_div(fpu_mul(num2, u2f(0x3f4ccccdu)), static_cast<float>(frame_scale_i(0xa)));
            const float num3 = int_to_float(lh(pv + 0x1e));
            b = fpu_div(fpu_mul(num3, 0.5f), static_cast<float>(frame_scale_i(0xa)));
        }

        Vec4 lpos{};
        moby_chain_point_v(m, 0u, lpos);
        const float h_ang1 = atan2_f(lwc1(HERO + 0x670), lwc1(HERO + 0x674));
        lpos.x = fpu_add(lpos.x, cos_vu(h_ang1));
        const float h_ang2 = atan2_f(lwc1(HERO + 0x670), lwc1(HERO + 0x674));
        lpos.y = fpu_add(lpos.y, sin_vu(h_ang2));
        lpos.z = fpu_add(lpos.z, u2f(0x3e99999au));

        const int32_t slot = lh(pv + 0x1c);
        if (slot == -1) {
            EeStackFrame sf(0x10);
            V(sf) = lpos;
            sh(pv + 0x1c, static_cast<uint32_t>(light_add(sf, 7.0f, 0.0f, r, g, b)));
        } else {
            const uint32_t rec = OV(0x001802c0u) + (static_cast<uint32_t>(slot) << 5);
            V(rec + 0x10) = lpos;
            swc1(rec + 8, b);
            swc1(rec + 0x1c, 7.0f);
            swc1(rec, r);
            swc1(rec + 4, g);
        }
        (void)timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x1e)));
    } else {
        const int32_t slot = lh(pv + 0x1c);
        if (slot != -1) {
            region_clear(static_cast<uint32_t>(slot));
            sh(pv + 0x1c, 0xffffu);
            sh(pv + 0x1e, 0);
        }
    }

    if (lbu(m + 0x53) == 2u) {
        region_clear(static_cast<uint32_t>(lh(pv + 0x1c)));
        sh(pv + 0x1e, 0);
        sh(pv + 0x1c, 0xffffu);
    }
}

// orig: (none)
void devastator_fns_register() {
    ovl_register_moby_fn(0x002b2100, devastator_update);
}

}  // namespace rac1
