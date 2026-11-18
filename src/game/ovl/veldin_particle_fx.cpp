// Veldin's shared particle updates (L0, reached across levels via OV() relocation):
// - types 0x06, 0x38, 0x39 and helper vec_damp_along (0x25c2b0, formerly shared_fx_tr.cpp);
// - types 0x00, 0x01, 0x2b, 0x4a, 0x4d (formerly particle_types2_tr.cpp);
// - type 0x28 (formerly particle_28_tr.cpp) and type 0x44 (formerly particle_44_tr.cpp).
// Declarations in shared_fx_tr.h, particle_types2_tr.h, particle_28_tr.h, particle_44_tr.h.
#include <game/ovl/particle_28_tr.h>
#include <game/ovl/particle_44_tr.h>
#include <game/ovl/particle_types2_tr.h>
#include <game/ovl/shared_fx_tr.h>

#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/moby_joints.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_height_map.h>
#include <game/ovl/shared_particle_kinds.h>

namespace rac1 {

namespace {
int16_t& h16(uint32_t a) { return *static_cast<int16_t*>(ee_ram_ptr(a)); }
int32_t trunc_i32(float f) { return static_cast<int32_t>(f2u(eer::cvt_w(f))); }
}  // namespace

// Removes `k` times the component of `v` along the unit direction of `dir`.
// orig: 0x0025c2b0
void vec_damp_along(Vec4& out, const Vec4& v, const Vec4& dir, float k) {
    Vec4 unit{}, proj{};
    vec3_set_length(unit, dir, 1.0f);
    const float dot = vec3_dot(unit, v);
    vec3_scale(proj, unit, fpu_mul(dot, k));
    vec3_sub(out, v, proj);
}

// Type 0x06: emitter-configured particle (`cfg` at +0x2c, flags at cfg+0x70) with optional segment collision
// bounce (bit 0), 16-bit fractional RGB ramp/oscillation (bit 1), alpha ramp/oscillation (bit 2), acceleration
// with optional moby rotation (bit 3 / bit 15), size clamp, spin step, lifetime, and AABB bounds check.
// orig: 0x00269d20
void particle_06_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    const uint32_t cfg = lwu(q + 0xc);
    Vec4 next{
        fpu_add(lwc1(p + 0x10), lwc1(q + 0)),
        fpu_add(lwc1(p + 0x14), lwc1(q + 4)),
        fpu_add(lwc1(p + 0x18), lwc1(q + 8)),
        0.0f,
    };

    if ((lwu(cfg + 0x70) & 1u) != 0 && segment_collide_v(V(p + 0x10), next, 4, p, nullptr) != 0) {
        const Vec4 normal = coll_hit_normal();
        const Vec4 vel_in{lwc1(q + 0), lwc1(q + 4), lwc1(q + 8), 0.0f};
        Vec4 bounced{}, push{};
        vec_damp_along(bounced, vel_in, normal, lwc1(cfg + 0x3c));
        swc1(q + 0, bounced.x);
        swc1(q + 4, bounced.y);
        swc1(q + 8, bounced.z);
        vec3_set_length(push, normal, u2f(0x3d6147aeu));
        vec3_add(V(p + 0x10), coll_hit_point(), push);
    } else {
        V(p + 0x10) = next;
    }

    const uint32_t flags = lwu(cfg + 0x70);
    if ((flags & 2u) != 0) {
        const uint32_t r16 = (lbu(p + 4) << 8) | lbu(q + 0x18);
        const uint32_t g16 = (lwu(p + 4) & 0xff00u) | lbu(q + 0x19);
        const uint32_t b16 = ((lwu(p + 4) >> 8) & 0xff00u) | lbu(q + 0x1a);
        const float inv16 = 65535.0f;
        float r = fpu_div(int_to_float(static_cast<int32_t>(r16)), inv16);
        const float g_i = static_cast<float>(float_to_int(static_cast<float>(static_cast<int32_t>(g16))));
        float g = fpu_div(g_i, inv16);
        const float b_i = static_cast<float>(float_to_int(static_cast<float>(static_cast<int32_t>(b16))));
        float b = fpu_div(b_i, inv16);

        const float sc0 = lwc1(0x0015ed60);
        const float dr = fpu_mul(lwc1(cfg + 0x4c), sc0);
        r = (lbu(q + 0x1b) & 0x10u) != 0 ? fpu_sub(r, dr) : fpu_add(r, dr);

        const uint32_t dir_bits = lbu(q + 0x1b);
        const float dg = fpu_mul(lwc1(cfg + 0x50), sc0);
        g = (dir_bits & 0x20u) != 0 ? fpu_sub(g, dg) : fpu_add(g, dg);

        const float db = fpu_mul(lwc1(cfg + 0x54), lwc1(0x0015ed60));
        b = (dir_bits & 0x40u) != 0 ? fpu_sub(b, db) : fpu_add(b, db);

        if (lwc1(cfg + 0x64) < r) {
            r = lwc1(cfg + 0x64);
            if ((lwu(cfg + 0x70) & 0x100u) != 0) sb(q + 0x1b, dir_bits ^ 0x10u);
        }
        if (r < lwc1(cfg + 0x58)) {
            r = lwc1(cfg + 0x58);
            if ((lwu(cfg + 0x70) & 0x10u) != 0) sb(q + 0x1b, lbu(q + 0x1b) ^ 0x10u);
        }
        if (lwc1(cfg + 0x68) < g) {
            g = lwc1(cfg + 0x68);
            if ((lwu(cfg + 0x70) & 0x200u) != 0) sb(q + 0x1b, lbu(q + 0x1b) ^ 0x20u);
        }
        if (g < lwc1(cfg + 0x5c)) {
            g = lwc1(cfg + 0x5c);
            if ((lwu(cfg + 0x70) & 0x20u) != 0) sb(q + 0x1b, lbu(q + 0x1b) ^ 0x20u);
        }
        if (lwc1(cfg + 0x6c) < b) {
            b = lwc1(cfg + 0x6c);
            if ((lwu(cfg + 0x70) & 0x400u) != 0) sb(q + 0x1b, lbu(q + 0x1b) ^ 0x40u);
        }
        if (b < lwc1(cfg + 0x60)) {
            b = lwc1(cfg + 0x60);
            if ((lwu(cfg + 0x70) & 0x40u) != 0) sb(q + 0x1b, lbu(q + 0x1b) ^ 0x40u);
        }

        sb(q + 0x18, static_cast<uint32_t>(float_to_int(fpu_mul(r, 65535.0f))));
        sb(q + 0x19, static_cast<uint32_t>(float_to_int(fpu_mul(g, 65535.0f))));
        sb(q + 0x1a, static_cast<uint32_t>(float_to_int(fpu_mul(b, 65535.0f))));
        const uint32_t cr = static_cast<uint32_t>(float_to_int(fpu_mul(r, 255.0f))) & 0xffu;
        const uint32_t cg = static_cast<uint32_t>(float_to_int(fpu_mul(g, 255.0f))) & 0xffu;
        const uint32_t cb = static_cast<uint32_t>(float_to_int(fpu_mul(b, 255.0f))) & 0xffu;
        const uint32_t ca = static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(q + 0x10), 255.0f))) << 24;
        sw(p + 4, cr | (cg << 8) | (cb << 16) | ca);
    } else if ((flags & 4u) != 0) {
        const uint32_t ca = static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(q + 0x10), 255.0f))) << 24;
        sw(p + 4, (lwu(p + 4) & 0xffffffu) | ca);
    }

    if ((lwu(cfg + 0x70) & 4u) != 0) {
        const float da = fpu_mul(lwc1(cfg + 0x38), lwc1(0x0015ed60));
        float a = (lbu(q + 0x1b) & 0x80u) != 0 ? fpu_sub(lwc1(q + 0x10), da) : fpu_add(lwc1(q + 0x10), da);
        swc1(q + 0x10, a);
        a = lwc1(q + 0x10);
        if (lwc1(cfg + 0x48) < a) {
            swc1(q + 0x10, lwc1(cfg + 0x48));
            if ((lwu(cfg + 0x70) & 0x800u) != 0) sb(q + 0x1b, lbu(q + 0x1b) ^ 0x80u);
            a = lwc1(q + 0x10);
        }
        if (a < lwc1(cfg + 0x44)) {
            swc1(q + 0x10, lwc1(cfg + 0x44));
            if ((lwu(cfg + 0x70) & 0x80u) != 0) sb(q + 0x1b, lbu(q + 0x1b) ^ 0x80u);
        }
    }

    sb(p + 8, static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(q + 0x14), 255.0f))));

    if ((lwu(cfg + 0x70) & 8u) != 0) {
        Vec4 vel{lwc1(q + 0), lwc1(q + 4), lwc1(q + 8), 0.0f};
        Vec4 acc_v{};
        if ((lwu(cfg + 0x70) & 0x8000u) != 0) {
            Mat4 m{};
            euler_to_mat4_vu(m, V(lwu(cfg + 0x7c) + 0x40));
            mat4_transform(acc_v, V(cfg), m);
        } else {
            acc_v = V(cfg);
        }
        vec3_add(vel, vel, acc_v);
        vec3_clamp_length_vu(vel, vel, lwc1(cfg + 0x40));
        swc1(q + 0, vel.x);
        swc1(q + 4, vel.y);
        swc1(q + 8, vel.z);
    }

    const float sz = fpu_add(lwc1(p + 0xc), fpu_mul(lwc1(cfg + 0x30), lwc1(0x0015ed60)));
    swc1(p + 0xc, sz);
    if (sz < lwc1(cfg + 0x74)) swc1(p + 0xc, lwc1(cfg + 0x74));
    if (lwc1(cfg + 0x78) < lwc1(p + 0xc)) swc1(p + 0xc, lwc1(cfg + 0x78));

    const float next_spin = fpu_add(lwc1(q + 0x14), fpu_mul(lwc1(cfg + 0x34), lwc1(0x0015ed60)));
    const float alpha_now = lwc1(q + 0x10);
    swc1(q + 0x14, next_spin);
    if (alpha_now <= 0.0f || timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }

    const float px = lwc1(p + 0x10), py = lwc1(p + 0x14), pz = lwc1(p + 0x18);
    if (px < lwc1(cfg + 0x10) || py < lwc1(cfg + 0x14) || pz < lwc1(cfg + 0x18) ||
        lwc1(cfg + 0x20) < px || lwc1(cfg + 0x24) < py || lwc1(cfg + 0x28) < pz) {
        free_particle(p);
    }
}

// Type 0x38: slows its velocity by 2.5%, grows by 6300, and steps its grey byte down by 4 until <= 0.
// orig: 0x00272838
void particle_38_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    vec3_scale(V(q), V(q), u2f(0x3f79999au));
    vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    const float sz = fpu_add(lwc1(p + 0xc), 6300.0f);
    const int32_t c = static_cast<int32_t>(lbu(p + 4)) - 4;
    swc1(p + 0xc, sz);
    if (c <= 0) {
        free_particle(p);
        return;
    }
    const uint32_t u = static_cast<uint32_t>(c);
    sw(p + 4, 0x60000000u | (u << 16) | (u << 8) | u);
}

// Type 0x39: moves by +0x20, grows by 5880, damps its spin rate (+0x34) by 2%, counts +0xa down, and ramps
// alpha up then down symmetrically around 64.
// orig: 0x002728e8
void particle_39_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    swc1(p + 0xc, fpu_add(lwc1(p + 0xc), 5880.0f));
    const float rate = fpu_mul(lwc1(q + 0x14), u2f(0x3f7ae148u));
    const float spin = fpu_add(lwc1(q + 0x10), rate);
    swc1(q + 0x14, rate);
    swc1(q + 0x10, spin);
    sb(p + 8, static_cast<uint32_t>(float_to_int(spin)));

    const uint32_t next_life = lhu(p + 0xa) - 1u;
    sh(p + 0xa, next_life);
    if (static_cast<int16_t>(next_life) <= 0) {
        free_particle(p);
    }

    const int32_t life = lh(p + 0xa);
    const uint32_t alpha = static_cast<uint32_t>(life < 65 ? life : 0x80 - life);
    sw(p + 4, (alpha << 24) | 0x808080u);
}

// Type 0x00: rain streak (+0x10..+0x28) falling under wind (0x160250) and gravity against the ground height map.
// orig: 0x00268d48
void particle_00_update(uint32_t p) {
    const uint32_t s1 = p + 0x30;
    float floor_z = lwc1(s1 + 0xc);
    if (lwc1(p + 0x18) < floor_z) {
        free_particle(p);
        return;
    }
    const float x = lwc1(p + 0x10), y = lwc1(p + 0x14);
    if (x < 0.0f || y < 0.0f || 512.0f < x || 512.0f < y) {
        free_particle(p);
        return;
    }

    const uint32_t cx0 = f2u(eer::cvt_w(lwc1(p + 0x20)));
    const uint32_t cy0 = f2u(eer::cvt_w(lwc1(p + 0x24)));
    Vec4 step{};
    vec3_add(step, V(s1), V(OV(0x00160250)));
    vec3_add(V(p + 0x20), V(p + 0x20), step);
    vec3_add(V(p + 0x10), V(p + 0x10), step);

    if (lh(p + 0xa) != 2) {
        const uint32_t cx1 = f2u(eer::cvt_w(lwc1(p + 0x20)));
        if (cx0 != cx1 || cy0 != f2u(eer::cvt_w(lwc1(p + 0x24)))) {
            Vec4 pos = V(p + 0x20);
            const float h0 = ground_height_at(pos);
            const float min_h = lwc1(OV(0x00160260));
            if (h0 < min_h) {
                floor_z = min_h;
            } else {
                Vec4 pos2 = V(p + 0x20);
                floor_z = ground_height_at(pos2);
            }
            if (lwc1(p + 0x28) < floor_z) {
                free_particle(p);
                return;
            }
        }
    }

    const float dt2 = lwc1(0x0015ed70);
    swc1(s1 + 8, fpu_sub(lwc1(s1 + 8), fpu_mul(dt2, u2f(0x411ccccdu))));
    swc1(s1 + 0xc, fpu_add(floor_z, lwc1(OV(0x00160258))));
}

// Type 0x01: rain splash ring stepping through 9 sprite frames at *0x1b2084 over frame_scale_i(27) frames.
// orig: 0x00268fe8
void particle_01_update(uint32_t p) {
    const int32_t dur = frame_scale_i(0x1b);
    const int32_t a_num = static_cast<int32_t>(static_cast<uint32_t>(lh(p + 0xa)) * lwu(p + 0x20));
    const uint32_t alpha = static_cast<uint32_t>(ee_div(a_num, dur)) << 24;
    sw(p + 4, (lwu(p + 4) & 0xffffffu) | alpha);
    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }
    const int32_t d0 = frame_scale_i(0x1b);
    const int32_t d1 = frame_scale_i(0x1b);
    const int32_t elapsed = static_cast<int32_t>(static_cast<uint32_t>(d0) - static_cast<uint32_t>(lh(p + 0xa)));
    const int32_t frame = ee_div(static_cast<int32_t>(static_cast<uint32_t>(elapsed) * 9u), d1);
    sb(p + 2, lbu(lwu(OV(0x001b2084)) + static_cast<uint32_t>(frame)));
}

// Type 0x4d: drifts by +0x20 (whose w holds initial lifetime), fading alpha from 38 and spin from 33.
// orig: 0x00275860
void particle_4d_update(uint32_t p) {
    Vec4 vel = V(p + 0x20);
    const float total = vel.w;
    vel.w = 0.0f;
    const float ratio = fpu_div(int_to_float(lh(p + 0xa)), total);
    const uint32_t alpha = static_cast<uint32_t>(float_to_int(fpu_mul(ratio, 38.0f))) << 24;
    const float spin = fpu_mul(ratio, 33.0f);
    sw(p + 4, alpha | 0x808080u);
    sb(p + 8, static_cast<uint32_t>(float_to_int(spin)));
    vec3_add(V(p + 0x10), V(p + 0x10), vel);
    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
    }
}

// Type 0x2b: counts age (+0xa) up to +0x38, fading alpha in over frame_scale_i(30) frames and out afterwards,
// drifting by +0x20 with upward buoyancy.
// orig: 0x002710a0
void particle_2b_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    const uint32_t next_age_u = lhu(p + 0xa) + 1u;
    sh(p + 0xa, next_age_u);
    const int32_t age = static_cast<int16_t>(next_age_u);
    if (lh(q + 0x18) < age) {
        free_particle(p);
        return;
    }

    const float rem = fpu_sub(1.0f, fpu_div(int_to_float(age), int_to_float(lh(q + 0x18))));
    const uint32_t cr = static_cast<uint32_t>(float_to_int(fpu_add(int_to_float(lbu(q + 0x14)), fpu_mul(rem, u2f(0xba83126fu))))) & 0xffu;
    const uint32_t cg = static_cast<uint32_t>(float_to_int(fpu_add(int_to_float(lbu(q + 0x15)), fpu_mul(rem, u2f(0xba902de0u))))) & 0xffu;
    const uint32_t cb = static_cast<uint32_t>(float_to_int(fpu_add(int_to_float(lbu(q + 0x16)), fpu_mul(rem, u2f(0xba917d6bu))))) & 0xffu;

    float fade;
    if (lh(p + 0xa) < frame_scale_i(0x1e)) {
        fade = fpu_div(int_to_float(lh(p + 0xa)), int_to_float(frame_scale_i(0x1e)));
    } else {
        const int32_t num = lh(p + 0xa) - frame_scale_i(0x1e);
        const int32_t den = lh(q + 0x18) - frame_scale_i(0x1e);
        fade = fpu_sub(1.0f, fpu_div(int_to_float(num), int_to_float(den)));
    }
    const uint32_t ca = (static_cast<uint32_t>(float_to_int(fpu_mul(int_to_float(lbu(q + 0x17)), fade))) & 0xffu) << 24;
    const uint32_t next_spin = lbu(p + 8) + 1u;
    sw(p + 4, ca | (cb << 16) | (cg << 8) | cr);
    sb(p + 8, next_spin);
    vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    swc1(q + 8, fpu_add(lwc1(q + 8), fpu_mul(lwc1(0x0015ed64), u2f(0x3a03126fu))));
}

// Type 0x4a: Sandmouse dust puff attached in local coordinates (+0x30) to moby +0x2c with descriptor at +0x3c.
// orig: 0x00275510
void particle_4a_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    const uint32_t rec = lwu(q + 0x1c);
    const int32_t total = lw(rec + 0xc);
    const int32_t elapsed = static_cast<int32_t>(static_cast<uint32_t>(total) - static_cast<uint32_t>(lh(p + 0xa)));
    const int32_t sz_i = ee_div(static_cast<int32_t>(static_cast<uint32_t>(lh(rec + 8)) * static_cast<uint32_t>(elapsed)), total) + lh(rec + 0xa);
    swc1(p + 0xc, fpu_mul(int_to_float(sz_i), 1000.0f));

    Vec4 vel{lwc1(q + 0), lwc1(q + 4), lwc1(q + 8), 0.0f};
    const float damp = fpu_add(fpu_mul(lwc1(0x0015ed60), u2f(0xbca3d700u)), 1.0f);
    vec3_scale(vel, vel, damp);
    swc1(q + 0, vel.x);
    swc1(q + 4, vel.y);
    swc1(q + 8, vel.z);

    const uint32_t m = lwu(q + 0xc);
    if (m == 0 || lbu(m + 0x20) == 0xfeu || lbu(m + 0x20) == 0xfdu) {
        free_particle(p);
        return;
    }

    Vec4 loc{lwc1(q + 0x10), lwc1(q + 0x14), lwc1(q + 0x18), 0.0f};
    Vec4 rot_loc{};
    vec3_add(loc, loc, vel);
    swc1(q + 0x10, loc.x);
    swc1(q + 0x14, loc.y);
    swc1(q + 0x18, loc.z);
    mat3_transform(rot_loc, loc, M(m + 0xc0));
    vec3_add(V(p + 0x10), V(m + 0x10), rot_loc);

    sb(p + 8, lbu(p + 8) + 1u);
    const float ratio = fpu_div(static_cast<float>(lh(p + 0xa)), int_to_float(lw(rec + 0xc)));
    sw(p + 4, rgba_lerp_ovl(lwu(rec + 4), lwu(rec + 0), ratio));
    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
    }
}

// Type 0x28: splitting smoke/splash particle (Orxon goo) that spawns a child on timer expiry, cycles textures
// from *0x1b20b0, tests segments against the world/mobys, and reflects on hit.
// orig: 0x0026ff10
void particle_28_update(uint32_t p) {
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t q = p + 0x20;

    if (lh(p + 0xa) != 0 && timer_tick_i16(h16(p + 0xa)) != 0) {
        sb(q + 0x18, lbu(q + 0x18) | 0x88u);
        const float old_sz = lwc1(p + 0xc);
        const float spread = fpu_div(old_sz, 900000.0f);
        const float jitter = u2f(0x3e4ccccdu);
        swc1(p + 0xc, fpu_mul(old_sz, u2f(0x3f4ccccdu)));
        vec3_scale(V(sp), V(q), rand_range_f(0.5f, u2f(0x3f333333u)));

        const float r_px = rand_range_f(-spread, spread);
        swc1(sp + 0x10, fpu_add(lwc1(p + 0x10), r_px));
        const float r_py = rand_range_f(-spread, spread);
        swc1(sp + 0x14, fpu_add(lwc1(p + 0x14), r_py));
        const float r_pz = rand_range_f(-spread, spread);
        swc1(sp + 0x18, fpu_add(lwc1(p + 0x18), r_pz));

        const float vx = lwc1(q + 0);
        const float r_vx = rand_range_f(fpu_mul(-vx, jitter), fpu_mul(vx, jitter));
        swc1(sp + 0, fpu_add(lwc1(sp + 0), r_vx));
        const float vy = lwc1(q + 4);
        const float r_vy = rand_range_f(fpu_mul(-vy, jitter), fpu_mul(vy, jitter));
        swc1(sp + 4, fpu_add(lwc1(sp + 4), r_vy));
        const float vz = lwc1(q + 8);
        const float r_vz = rand_range_f(fpu_mul(-vz, jitter), 0.0f);
        swc1(sp + 8, fpu_add(lwc1(sp + 8), r_vz));

        const uint32_t child = particle_28_spawn(sp + 0x10, sp, 0x88, lwu(q + 0x1c));
        if (child != 0) {
            sh(child + 0xa, static_cast<uint32_t>(frame_scale_i(2)));
            swc1(child + 0xc, lwc1(p + 0xc));
        }

        vec3_scale(V(q), V(q), rand_range_f(0.5f, u2f(0x3f333333u)));
        const float vx2 = lwc1(q + 0);
        const float r_vx2 = rand_range_f(fpu_mul(-vx2, jitter), fpu_mul(vx2, jitter));
        swc1(q + 0, fpu_add(lwc1(q + 0), r_vx2));
        const float vy2 = lwc1(q + 4);
        const float r_vy2 = rand_range_f(fpu_mul(-vy2, jitter), fpu_mul(vy2, jitter));
        swc1(q + 4, fpu_add(lwc1(q + 4), r_vy2));
        const float vz2 = lwc1(q + 8);
        const float r_vz2 = rand_range_f(fpu_mul(-vz2, jitter), 0.0f);
        swc1(q + 8, fpu_add(lwc1(q + 8), r_vz2));

        const float r_qx = rand_range_f(-spread, spread);
        swc1(p + 0x10, fpu_add(lwc1(p + 0x10), r_qx));
        const float r_qy = rand_range_f(-spread, spread);
        swc1(p + 0x14, fpu_add(lwc1(p + 0x14), r_qy));
        const float r_qz = rand_range_f(-spread, spread);
        swc1(p + 0x18, fpu_add(lwc1(p + 0x18), r_qz));
    }

    const uint32_t tex_tbl = lwu(OV(0x001b20b0));
    const uint32_t tex_idx = (static_cast<uint32_t>(lh(p + 0xa)) + static_cast<uint32_t>(lh(q + 0x16)) + (lbu(p + 2) - lbu(tex_tbl))) & 7u;
    sb(p + 2, lbu(tex_tbl + tex_idx));
    vec3_add(V(sp), V(p + 0x10), V(q));

    if (frame_scale_i(3) < lh(q + 0x14)) {
        vec3_sub(V(sp + 0x10), V(p + 0x10), V(q));
    } else {
        V(sp + 0x10) = V(p + 0x10);
    }

    const uint32_t flt = sp + 0x20;
    vec3_scale(V(flt), V(flt), u2f(0x3c23d70au));
    const uint32_t flags0 = lbu(q + 0x18);
    uint32_t seg_flags, seg_flt;
    if ((flags0 & 8u) != 0) {
        if ((lwu(p + 4) & 0xff000000u) == 0) {
            free_particle(p);
            ee_stack_free(0xf0);
            return;
        }
        sb(q + 0x18, flags0 | 8u);
        seg_flags = 4;
        seg_flt = 0;
    } else {
        sw(sp + 0x30, lwu(q + 0x1c));
        sw(sp + 0x34, 0x10001u);
        swc1(sp + 0x3c, 1.0f);
        V(flt) = V(q);
        if ((flags0 & 0x40u) != 0) vec_clear(V(flt));
        sw(sp + 0x40, 1);
        seg_flags = 0;
        seg_flt = flt;
    }

    const int32_t hit = segment_collide_v(
        V(sp + 0x10), V(sp), seg_flags, lwu(q + 0x1c),
        seg_flt != 0 ? static_cast<const uint32_t*>(ee_ram_ptr(seg_flt)) : nullptr);
    if (hit != 0) {
        const Vec4 normal = coll_hit_normal();
        Vec4 push{};
        vec3_reflect(V(q), V(q), normal);
        vec3_scale(V(q), V(q), u2f(0x3ca3d70au));
        vec3_set_length(push, normal, u2f(0x3d6147aeu));
        vec3_add(V(p + 0x10), coll_hit_point(), push);
        const float sz = lwc1(p + 0xc);
        uint32_t f18 = lbu(q + 0x18);
        if (sz < 200000.0f && (f18 & 8u) == 0) {
            swc1(p + 0xc, fpu_add(sz, sz));
            f18 = lbu(q + 0x18);
        }
        sb(q + 0x18, f18 | 8u);
    } else {
        V(p + 0x10) = V(sp);
    }

    if ((lbu(q + 0x18) & 8u) != 0) {
        const float sc = lwc1(0x0015ed60);
        const float steps = static_cast<float>(trunc_i32(fpu_div(8.0f, fpu_mul(sc, u2f(0x3ee2d0e5u)))));
        sh(q + 0x16, lhu(q + 0x16) + 1u);
        const float dec = fpu_mul(fpu_mul(fpu_div(260000.0f, steps), sc), 1.25f);
        const float sz2 = fpu_sub(lwc1(p + 0xc), dec);
        swc1(p + 0xc, sz2);
        if (sz2 <= 0.0f) {
            free_particle(p);
            ee_stack_free(0xf0);
            return;
        }
    } else {
        const float sc = lwc1(0x0015ed60);
        const float steps = static_cast<float>(trunc_i32(fpu_div(8.0f, fpu_mul(sc, u2f(0x3ee2d0e5u)))));
        const float inc = fpu_mul(fpu_div(260000.0f, steps), sc);
        swc1(p + 0xc, fpu_add(lwc1(p + 0xc), inc));
    }

    const float t14 = int_to_float(lh(q + 0x14));
    const float sc = lwc1(0x0015ed60);
    const float steps = static_cast<float>(trunc_i32(fpu_div(8.0f, fpu_mul(sc, u2f(0x3ee2d0e5u)))));
    const float max_a = u2f(0x3da3d70au);
    float a = fpu_add(fpu_mul(fpu_mul(fpu_div(max_a, steps), sc), fpu_div(t14, steps)), max_a);
    if (a < 0.0f) a = 0.0f;
    if (max_a < a) a = max_a;
    if ((lbu(q + 0x18) & 8u) != 0) {
        a = fpu_div(a, fpu_add(fpu_div(static_cast<float>(lh(q + 0x16)), 7.0f), 1.0f));
    }
    const uint32_t ca = static_cast<uint32_t>(float_to_int(fpu_mul(a, 256.0f))) << 24;
    sw(p + 4, (lwu(p + 4) & 0xffffffu) | ca);

    swc1(q + 8, fpu_add(lwc1(q + 8), fpu_mul(lwc1(0x0015ed64), u2f(0x3c03126fu))));
    if (frame_scale_i(0x19) < lh(q + 0x14) || (lbu(q + 0x18) & 8u) != 0) {
        swc1(q + 0, fpu_mul(lwc1(q + 0), u2f(0x3f733333u)));
        swc1(q + 4, fpu_mul(lwc1(q + 4), u2f(0x3f733333u)));
    }

    const uint32_t f18 = lbu(q + 0x18);
    const float dir = (f18 & 1u) != 0 ? 1.0f : -1.0f;
    if ((f18 & 8u) != 0) {
        swc1(q + 8, fpu_add(lwc1(q + 8), fpu_mul(lwc1(0x0015ed64), u2f(0x3c03126fu))));
        const float denom = int_to_float(lh(q + 0x16));
        const float dspin = fpu_div(fpu_mul(dir, fpu_mul(lwc1(0x0015ed60), u2f(0xbc23d70au))), denom);
        swc1(q + 0x10, fpu_add(lwc1(q + 0x10), dspin));
    } else {
        const float dspin = fpu_mul(dir, fpu_mul(lwc1(0x0015ed60), u2f(0xbc23d70au)));
        swc1(q + 0x10, fpu_add(lwc1(q + 0x10), dspin));
    }
    if (1.0f < lwc1(q + 0x10)) swc1(q + 0x10, fpu_sub(lwc1(q + 0x10), 1.0f));
    if (lwc1(q + 0x10) < 0.0f) swc1(q + 0x10, fpu_add(lwc1(q + 0x10), 1.0f));
    sb(p + 8, f2u(eer::cvt_w(fpu_mul(lwc1(q + 0x10), 256.0f))));

    ee_stack_free(0xf0);
}

// Type 0x44: jet streak anchored to chain +0x34 of moby +0x30 (expected class +0x36), trailing along the
// chain's world axis by a jittered multiple of +0x3c.
// orig: 0x00274818
void particle_44_update(uint32_t p) {
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t s3 = p + 0x30;
    const uint32_t m = lwu(s3);
    if (m == 0 || lh(m + 0xa6) != lh(s3 + 6) || lbu(m + 0x20) == 0xfeu || lbu(m + 0x20) == 0xfdu) {
        free_particle(p);
        ee_stack_free(0xc0);
        return;
    }

    const float r0 = rand_range_f(u2f(0x3d4ccccdu), 1.0f);
    const float w_out = fpu_mul(lwc1(s3 + 8), r0);
    const float saved_w = lwc1(p + 0x2c);
    moby_chain_point_v(lwu(s3), lbu(s3 + 4), V(p + 0x10));
    moby_chain_world_one(lwu(s3), lbu(s3 + 4), sp);

    const float jitter_lim = int_to_float(lbu(s3 + 5));
    const float r1 = rand_range_f(-jitter_lim, jitter_lim);
    const float len = fpu_mul(fpu_sub(fpu_div(r1, 100.0f), 1.0f), lwc1(s3 + 0xc));
    Vec4 tail{};
    vec3_set_length(tail, V(sp), len);
    vec3_add(V(p + 0x20), V(p + 0x10), tail);
    swc1(p + 0x2c, saved_w);
    swc1(p + 0x1c, w_out);

    ee_stack_free(0xc0);
}

}  // namespace rac1
