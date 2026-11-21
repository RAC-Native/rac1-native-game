// Veldin's particle updates, part 1 of 2 (formerly particle_types_tr.cpp, unit veldin_particles):
// types 0x03, 0x09, 0x0a, 0x0e, 0x14 (with helper local_offset_coord, 0x25c230), 0x18, 0x1d, 0x1e, 0x1f,
// 0x24, 0x25, and 0x26. Declarations in particle_types_tr.h.
#include <game/ovl/particle_types_tr.h>

#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/moby_joints.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
int16_t& h16(uint32_t a) { return *static_cast<int16_t*>(ee_ram_ptr(a)); }
int32_t& w32_ref(uint32_t a) { return *static_cast<int32_t*>(ee_ram_ptr(a)); }
int32_t div4_s32(int32_t x) {
    const int32_t adj = x >= 0 ? x : static_cast<int32_t>(static_cast<uint32_t>(x) + 3u);
    return adj >> 2;
}
}  // namespace

// Transforms `(pt - origin)` by the transpose of `mat`'s upper 3x3 and returns coordinate `axis` (0..3).
// orig: 0x0025c230
float local_offset_coord(const Vec4& origin, const Mat4& mat, const Vec4& pt, int32_t axis) {
    Vec4 diff{};
    Mat4 inv{};
    Vec4 out{};
    vec3_sub(diff, pt, origin);
    mat3_transpose(inv, mat);
    mat4_transform(out, diff, inv);
    switch (axis) {
    case 0: return out.x;
    case 1: return out.y;
    case 2: return out.z;
    default: return out.w;
    }
}

// Type 0x03: ambient mote drifting with wind (0x1cb7c4 / 0x1cb7d4), wrapped into the camera's forward cone
// (up to 6 times unless under geometry), faded by distance (8..96) and life, clearing byte *+0x30 on exit.
// orig: 0x002694f8
void particle_03_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    const int32_t st = lw(q + 0x1c);
    if (st >= 3) {
        if (frame_scale_i(0x78) < lh(p + 0xa) && sphere_collide(p + 0x10, u2f(0x40ffae14u), 0x12, 0) != 0) {
            sw(q + 0x14, 1);
            sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0x78)));
            sw(q + 0x1c, 0xffffffffu);
        } else {
            sw(q + 0x1c, 0);
        }
    } else if (st >= 0) {
        sw(q + 0x1c, static_cast<uint32_t>(st + 1));
    }

    if (lw(q + 0x1c) >= 0) {
        const float vx = fpu_div(fpu_mul(cos_vu(lwc1(OV(0x001cb7c4))), lwc1(OV(0x001cb7d4))), 2.5f);
        swc1(q + 0, vx);
        const float vy = fpu_div(fpu_mul(sin_vu(lwc1(OV(0x001cb7c4))), lwc1(OV(0x001cb7d4))), 2.5f);
        sw(q + 8, 0);
        swc1(q + 4, vy);
        vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    }

    const float dist = vec3_distance(V(p + 0x10), V(OV(0x00166dc0)));
    const float bearing = atan2_f(
        fpu_sub(lwc1(p + 0x10), lwc1(OV(0x00166dc0))),
        fpu_sub(lwc1(p + 0x14), lwc1(OV(0x00166dc4))));
    const float rel = angle_sub(bearing, lwc1(OV(0x00166dd8)));
    const float cone = u2f(0x3f860a92u);

    if (cone < abs_f(rel)) {
        const int32_t wraps = lw(q + 0x18);
        sw(q + 0x18, static_cast<uint32_t>(wraps + 1));
        if (!(wraps < 6)) {
            sb(lwu(q + 0x10), 0);
            free_particle(p);
            return;
        }
        const float off_x = 0.0f < rel ? u2f(0xbf860a92u) : cone;
        const float rx = fpu_mul(cos_vu(angle_add(lwc1(OV(0x00166dd8)), off_x)), dist);
        const float off_y = 0.0f < rel ? u2f(0xbf860a92u) : cone;
        const float ry = fpu_mul(sin_vu(angle_add(lwc1(OV(0x00166dd8)), off_y)), dist);
        const float rz = fpu_sub(lwc1(p + 0x18), lwc1(OV(0x00166dc8)));
        const Vec4 off{rx, ry, rz, 0.0f};
        vec3_add(V(p + 0x10), off, V(OV(0x00166dc0)));

        Vec4 top = V(p + 0x10);
        top.z = fpu_add(top.z, 32.0f);
        if (segment_collide_v(V(p + 0x10), top, 0x12, 0, nullptr) != 0) {
            sb(lwu(q + 0x10), 0);
            free_particle(p);
            return;
        }
    }

    if ((lwu(OV(0x0015f5cc)) & 3u) == 0) {
        sb(p + 8, lbu(p + 8) + 1u);
    }

    const int32_t life0 = lh(p + 0xa);
    int32_t base_a;
    if (frame_scale_i(0x78) < life0) {
        base_a = 0x14;
    } else if (lw(q + 0x14) != 0) {
        const int32_t d = frame_scale_i(0x78);
        base_a = ee_div(static_cast<int32_t>(static_cast<uint32_t>(lh(p + 0xa)) * 0x14u), d);
    } else {
        const int32_t d0 = frame_scale_i(0x78);
        const int32_t d1 = frame_scale_i(0x78);
        const int32_t elapsed = static_cast<int32_t>(static_cast<uint32_t>(d0) - static_cast<uint32_t>(lh(p + 0xa)));
        base_a = ee_div(static_cast<int32_t>(static_cast<uint32_t>(elapsed) * 0x14u), d1);
    }

    if (dist < 8.0f || 96.0f < dist) {
        sb(lwu(q + 0x10), 0);
        free_particle(p);
        return;
    }

    uint32_t ca;
    if (dist < 16.0f) {
        ca = static_cast<uint32_t>(float_to_int(fpu_mul(fpu_mul(int_to_float(base_a), fpu_sub(dist, 8.0f)), 0.125f)));
    } else if (80.0f < dist) {
        ca = static_cast<uint32_t>(float_to_int(fpu_mul(fpu_mul(int_to_float(base_a), fpu_sub(96.0f, dist)), 0.0625f)));
    } else {
        ca = static_cast<uint32_t>(base_a);
    }
    sw(p + 4, (ca << 24) | (lwu(p + 4) & 0xffffffu));

    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        if (lw(q + 0x14) != 0) {
            sb(lwu(q + 0x10), 0);
            free_particle(p);
            return;
        }
        sw(q + 0x14, 1);
        const int32_t lo_t = frame_scale_i(0x78) * 3;
        const int32_t hi_t = frame_scale_i(0x78) * 9;
        sh(p + 0xa, static_cast<uint32_t>(rand_range(lo_t, hi_t)));
    }
}

// Type 0x09: spinning sprite with 32-bit timer +0x20, ramping alpha in/out over quarters of frame_scale_i(*0x1601a4).
// orig: 0x0026a6f0
void particle_09_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    if (timer_tick_i32(w32_ref(q)) != 0) {
        free_particle(p);
        return;
    }

    const float ang = angle_add(lwc1(q + 0xc), lwc1(q + 8));
    swc1(q + 0xc, ang);
    sb(p + 8, static_cast<uint32_t>(float_to_int(fpu_div(fpu_mul(ang, 128.0f), u2f(0x40490fdbu)))) - 0x80u);

    const int32_t s_a = frame_scale_i(lw(OV(0x001601a4)));
    const int32_t three_s = static_cast<int32_t>(static_cast<uint32_t>(s_a) * 3u);
    uint32_t alpha;
    if (div4_s32(three_s) < lw(q)) {
        const int32_t s0 = frame_scale_i(lw(OV(0x001601a4)));
        const int32_t s1 = frame_scale_i(lw(OV(0x001601a4)));
        const int32_t num = static_cast<int32_t>(lwu(q + 0x10) * (static_cast<uint32_t>(s0) - lwu(q)));
        alpha = static_cast<uint32_t>(ee_div(num, div4_s32(s1)));
    } else {
        const int32_t s0 = frame_scale_i(lw(OV(0x001601a4)));
        if (lw(q) < div4_s32(s0)) {
            const int32_t s1 = frame_scale_i(lw(OV(0x001601a4)));
            const int32_t num = static_cast<int32_t>(lwu(q + 0x10) * lwu(q));
            alpha = static_cast<uint32_t>(ee_div(num, div4_s32(s1)));
        } else {
            alpha = lwu(q + 0x10);
        }
    }

    const uint32_t rgb = lbu(p + 3) == 0x48u ? 0x4f4f4fu : 0x7f7f7fu;
    sw(p + 4, (alpha << 24) + rgb);
}

// Type 0x0a: rising smoke puff fading +0x3c, growing +0xc, warming blue +0x38 up to 0.225, and damping velocity.
// orig: 0x0026a880
void particle_0a_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    const float next_a = fpu_add(lwc1(q + 0x1c), fpu_mul(lwc1(0x0015ed60), u2f(0xbd23d70au)));
    swc1(q + 0x1c, next_a);
    if (next_a <= 0.0f || timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }

    const float sc = lwc1(0x0015ed60);
    swc1(p + 0xc, fpu_add(lwc1(p + 0xc), fpu_mul(sc, 12000.0f)));
    if (lwc1(q + 0x18) < u2f(0x3e666666u)) {
        swc1(q + 0x18, fpu_add(lwc1(q + 0x18), fpu_mul(sc, u2f(0x3d463f14u))));
    }

    const uint32_t cb = static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(q + 0x18), 255.0f))) & 0xffu;
    const uint32_t ca = static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(q + 0x1c), 255.0f))) << 24;
    sw(p + 4, lhu(p + 4) | (cb << 16) | ca);
    vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    vec3_scale(V(q), V(q), fpu_add(fpu_mul(lwc1(0x0015ed60), u2f(0xbdccccd0u)), 1.0f));
    sb(p + 8, lbu(p + 8) - 1u);
}

// Type 0x0e: class 156 trail sprite — while +0x24 is 0, sits 0.01 closer to the camera than moby +0x28; else
// counts +0xa down over frame_scale_i(6) frames.
// orig: 0x0026ba68
void particle_0e_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    sb(p + 8, lbu(p + 8) + lbu(q));
    if (lw(q + 4) == 0) {
        V(p + 0x10) = V(lwu(q + 8) + 0x10);
        Vec4 to_cam{};
        vec3_sub(to_cam, V(OV(0x00166dc0)), V(p + 0x10));
        vec3_set_length(to_cam, to_cam, u2f(0x3c23d70au));
        vec3_add(V(p + 0x10), V(p + 0x10), to_cam);
        return;
    }
    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }
    const float rate = fpu_div(127.0f, static_cast<float>(frame_scale_i(6)));
    const uint32_t ca = static_cast<uint32_t>(float_to_int(fpu_mul(static_cast<float>(lh(p + 0xa)), rate))) << 24;
    sw(p + 4, ca | 0xffffffu);
}

// Type 0x14: spiralling intake particle converging onto chain 0 of moby +0x38 along basis *+0x20, freed once it
// crosses past the chain origin in the moby's local Y axis.
// orig: 0x0026c860
void particle_14_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    if (lwc1(q + 0x10) != 0.0f) {
        const uint32_t src = lwu(q);
        Mat4 basis{};
        basis.r[0] = V(src + 0x00);
        basis.r[1] = V(src + 0x10);
        basis.r[2] = V(src + 0x20);

        Vec4 ring{0.0f, fpu_mul(sin_vu(lwc1(q + 0x14)), lwc1(q + 0x10)), fpu_mul(cos_vu(lwc1(q + 0x14)), lwc1(q + 0x10)), 0.0f};
        Vec4 world_ring{};
        mat4_transform(world_ring, ring, basis);
        vec3_sub(V(p + 0x10), V(p + 0x10), world_ring);

        swc1(q + 0x1c, fpu_add(lwc1(q + 0x1c), fpu_mul(lwc1(0x0015ed60), u2f(0x3d75cd0cu))));
        Vec4 chain_pt{};
        moby_chain_point_v(lwu(q + 0x18), 0, chain_pt);
        vec3_sub(ring, chain_pt, V(p + 0x10));
        vec3_set_length(ring, ring, lwc1(q + 0x1c));
        vec3_add(V(p + 0x10), V(p + 0x10), ring);

        const float next_ang = angle_add(lwc1(q + 0x14), fpu_mul(lwc1(0x0015ed60), u2f(0x3f060a92u)));
        const float damp = fpu_add(fpu_mul(lwc1(0x0015ed60), u2f(0xbdccccd0u)), 1.0f);
        const float next_rad = fpu_mul(lwc1(q + 0x10), damp);
        swc1(q + 0x14, next_ang);
        swc1(q + 0x10, next_rad);

        ring = Vec4{0.0f, fpu_mul(sin_vu(next_ang), lwc1(q + 0x10)), fpu_mul(cos_vu(lwc1(q + 0x14)), lwc1(q + 0x10)), 0.0f};
        mat4_transform(world_ring, ring, basis);
        vec3_add(V(p + 0x10), V(p + 0x10), world_ring);

        if (0.0f < local_offset_coord(chain_pt, M(lwu(q + 0x18) + 0xc0), V(p + 0x10), 1)) {
            free_particle(p);
            return;
        }
    }

    const float da = fpu_mul(fpu_mul(lwc1(0x0015ed60), u2f(0x3c343958u)), lwc1(q + 0xc));
    const float a0 = fpu_add(lwc1(q + 4), da);
    swc1(q + 4, a0);
    if (a0 <= 0.0f || timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }

    const float a1 = lwc1(q + 4);
    if (1.0f <= a1) {
        const float sc = lwc1(0x0015ed60);
        const float neg_dir = -lwc1(q + 0xc);
        const float step = fpu_mul(fpu_mul(sc, u2f(0x3c343958u)), neg_dir);
        swc1(q + 0xc, neg_dir);
        swc1(q + 4, fpu_add(a1, step));
    }

    swc1(p + 0xc, fpu_add(lwc1(p + 0xc), fpu_mul(lwc1(0x0015ed60), -2940.0f)));
    const uint32_t ca = static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(q + 4), 255.0f))) << 24;
    sw(p + 4, (lwu(p + 4) & 0xffffffu) | ca);

    const float next_s = fpu_add(lwc1(q + 8), fpu_mul(lwc1(0x0015ed60), u2f(0x3bc49ba6u)));
    swc1(q + 8, next_s);
    if (1.0f < next_s) swc1(q + 8, fpu_sub(next_s, 1.0f));
    sb(p + 8, f2u(eer::cvt_w(fpu_mul(lwc1(q + 8), 255.0f))));
}

// Type 0x18: halo following another live particle (*+0x20), scaling its size and modulating alpha.
// orig: 0x0026d470
void particle_18_update(uint32_t p) {
    if (p == 0) return;
    const uint32_t q = p + 0x20;
    const uint32_t target = lwu(q);
    if (target == 0 || (lbu(target + 1) & 0x80u) != 0) {
        free_particle(p);
        return;
    }

    V(p + 0x10) = V(target + 0x10);
    sb(p + 8, lbu(target + 8));
    swc1(p + 0xc, fpu_add(fpu_mul(lwc1(target + 0xc), lwc1(q + 4)), lwc1(q + 8)));

    const float a_next = fpu_add(lwc1(q + 0xc), lwc1(q + 0x14));
    swc1(q + 0xc, a_next);
    if (a_next < 0.0f) {
        swc1(q + 0xc, 0.0f);
        return;
    }
    if (1.0f < a_next) swc1(q + 0xc, 1.0f);

    const uint32_t src = lwu(q);
    const uint32_t rgb = lwu(p + 4) & 0xffffffu;
    const float base_a = fpu_add(fpu_mul(int_to_float(lbu(src + 7)), fpu_mul(lwc1(q + 0x10), 0.00390625f)), lwc1(q + 0xc));
    const uint32_t ca = static_cast<uint32_t>(float_to_int(fpu_mul(base_a, 256.0f))) << 24;
    sw(p + 4, rgb | ca);
}

// Type 0x1d: animated flare (*0x1b20f4, 16 frames) tracking chain +0x2e of moby +0x20 and stepping size toward 159600.
// orig: 0x0026e3a8
void particle_1d_update(uint32_t p) {
    if (p == 0) return;
    const uint32_t q = p + 0x20;
    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }
    const uint32_t m = lwu(q);
    if (m == 0 || static_cast<int8_t>(lbu(m + 0x20)) < 0) {
        free_particle(p);
        return;
    }

    sh(q + 0xc, (lhu(q + 0xc) + 1u) & 0xfu);
    const float cap = 159600.0f;
    const float sz0 = lwc1(p + 0xc);
    if (sz0 < cap) {
        const float sz1 = fpu_add(sz0, fpu_mul(lwc1(0x0015ed60), u2f(0x45795fffu)));
        swc1(p + 0xc, sz1);
        if (cap < sz1) swc1(p + 0xc, cap);
    } else if (cap < sz0) {
        const float sz1 = fpu_sub(sz0, fpu_mul(lwc1(0x0015ed60), u2f(0x46795fffu)));
        swc1(p + 0xc, sz1);
        if (sz1 < cap) swc1(p + 0xc, cap);
    }

    const float next_spin = fpu_add(lwc1(q + 8), fpu_mul(lwc1(0x0015ed60), u2f(0x3a83126fu)));
    swc1(q + 8, next_spin);
    sb(p + 8, static_cast<uint32_t>(float_to_int(fpu_mul(next_spin, 255.0f))));

    const float sz_now = lwc1(p + 0xc);
    sb(p + 2, lbu(lwu(OV(0x001b20f4)) + static_cast<uint32_t>(lh(q + 0xc))));
    if (sz_now <= cap || 0.0f < lwc1(q + 0x10)) {
        swc1(q + 4, fpu_add(lwc1(q + 4), lwc1(q + 0x10)));
    }

    const float max_a = lwc1(p + 0xc) <= cap ? u2f(0x3e99999au) : u2f(0x3e19999au);
    float a = lwc1(q + 4);
    if (max_a < a) {
        const float neg_step = -lwc1(q + 0x10);
        a = fpu_add(a, neg_step);
        swc1(q + 0x10, neg_step);
        swc1(q + 4, a);
        a = lwc1(q + 4);
    } else if (a < 0.0f) {
        free_particle(p);
        return;
    } else {
        a = lwc1(q + 4);
    }

    const uint32_t ca = static_cast<uint32_t>(float_to_int(fpu_mul(a, 256.0f))) << 24;
    sw(p + 4, (lwu(p + 4) & 0xffffffu) + ca);

    const int32_t chain = lh(q + 0xe);
    const float cur_off = lwc1(q + 0x14);
    const float next_off = fpu_add(cur_off, fpu_mul(fpu_sub(1.0f, cur_off), fpu_mul(lwc1(0x0015ed60), u2f(0x3e333333u))));
    swc1(q + 0x14, next_off);
    if (chain != -1) {
        Vec4 pt{}, to_cam{};
        moby_chain_point_v(lwu(q), static_cast<uint32_t>(chain), pt);
        vec3_sub(to_cam, V(OV(0x00166dc0)), pt);
        vec3_set_length(to_cam, to_cam, lwc1(q + 0x14));
        vec3_add(V(p + 0x10), pt, to_cam);
    }
}

// Type 0x1e: doubly-linked ribbon/rope node (+0x30 prev, +0x34 next) with spring pull along downstream free
// nodes, sphere collision against the world, screen-space tangent orientation, and tail alpha decay.
// orig: 0x0026e670
void particle_1e_update(uint32_t p) {
    const uint32_t sp = ee_stack_alloc(0x100);
    const uint32_t q = p + 0x20;

    if (((lhu(q + 0x1e) ^ 1u) & 1u) != 0) {
        uint32_t next = lwu(q + 0x14);
        uint32_t nq = next != 0 ? next + 0x20 : 0;
        vec_clear(V(sp + 0x20));
        float k = u2f(0x3e19999au);

        // Stop walking downstream at a colliding node (+0x3c != 0) or a pinned node (+0x3e bit 0).
        constexpr uint64_t STOP_MASK = 0x0001ffff00000000ull;
        if (lh(q + 0x1c) == 0 && next != 0 && (ld(nq + 0x18) & STOP_MASK) == 0) {
            float d = 2.0f;
            float d2 = fpu_mul(d, d);
            for (;;) {
                vec3_sub(V(sp), V(next + 0x10), V(p + 0x10));
                d = fpu_add(d, 1.0f);
                const float pull_len = fpu_div(u2f(0x3e4ccccdu), d2);
                k = fpu_mul(k, u2f(0x3f7fbe77u));
                vec3_set_length(V(sp), V(sp), pull_len);
                vec3_add(V(sp + 0x20), V(sp + 0x20), V(sp));
                next = lwu(nq + 0x14);
                if (next == 0) break;
                nq = next + 0x20;
                if ((ld(nq + 0x18) & STOP_MASK) != 0) break;
                d2 = fpu_mul(d, d);
            }
        }

        const float drag = fpu_add(fpu_sub(u2f(0x3e19999au), k), u2f(0x3f88f5c3u));
        if (nq != 0 && lh(nq + 0x1c) != 0) {
            vec3_scale(V(q), V(q), 0.5f);
        } else {
            vec3_add(V(q), V(q), V(sp + 0x20));
        }
        if (lwu(q + 0x14) != 0) {
            vec3_scale(V(q), V(q), drag);
        } else {
            vec3_scale(V(q), V(q), u2f(0x3f7851ecu));
        }
        swc1(q + 8, fpu_sub(lwc1(q + 8), fpu_mul(lwc1(0x0015ed64), u2f(0x3ca3d70au))));
        vec3_add(V(sp), V(p + 0x10), V(q));
    } else {
        V(sp) = V(p + 0x10);
    }

    const uint32_t prev = lwu(q + 0x10);
    if (prev != 0) {
        const float link_len = u2f(0x3e19999au);
        vec3_sub(V(sp + 0x20), V(sp), V(prev + 0x10));
        const float dist = vec3_length(V(sp + 0x20));
        if (u2f(0x3dcccccdu) < abs_f(fpu_sub(dist, link_len))) {
            vec3_set_length(V(sp + 0x20), V(sp + 0x20), link_len);
            vec3_add(V(sp), V(sp + 0x20), V(prev + 0x10));
        }

        if (sphere_collide(sp, link_len, 0, 0) == 0) {
            sh(q + 0x1c, 0);
        } else {
            if (lh(q + 0x1c) == 0) {
                V(sp) = coll_hit_pushed();
            } else {
                V(sp) = V(p + 0x10);
            }
            sh(q + 0x1c, 1);
        }

        const uint32_t cam = OV(0x00166dc0);
        const uint32_t vp = OV(0x00166d40);
        vec3_sub(V(sp + 0x20), V(lwu(q + 0x10) + 0x10), V(cam));
        mat4_transform(V(sp + 0x20), V(sp + 0x20), M(vp));
        vec3_scale(V(sp + 0x20), V(sp + 0x20), fpu_div(1.0f, lwc1(sp + 0x2c)));

        vec3_sub(V(sp + 0x30), V(sp), V(cam));
        mat4_transform(V(sp + 0x30), V(sp + 0x30), M(vp));
        vec3_scale(V(sp + 0x30), V(sp + 0x30), fpu_div(1.0f, lwc1(sp + 0x3c)));

        vec3_sub(V(sp + 0x20), V(sp + 0x20), V(sp + 0x30));
        const float ang = atan2_f(lwc1(sp + 0x20), lwc1(sp + 0x24));
        sb(p + 8, static_cast<uint32_t>(float_to_int(fpu_div(fpu_mul(ang, 128.0f), u2f(0x40490fdbu)))) - 0x20u);

        if (lh(q + 0x1c) != 0) {
            vec3_reflect(V(q), V(q), coll_hit_normal());
            vec3_scale(V(q), V(q), u2f(0x3f733333u));
        } else {
            vec3_sub(V(sp + 0x10), V(sp), V(p + 0x10));
            vec3_add(V(q), V(q), V(sp + 0x10));
            vec3_scale(V(q), V(q), 0.5f);
        }
        vec3_sub(V(sp + 0x10), V(sp), V(p + 0x10));
        vec3_add(V(q), V(q), V(sp + 0x10));
        vec3_scale(V(q), V(q), 0.5f);
    }

    V(p + 0x10) = V(sp);
    if ((lhu(q + 0x1e) & 2u) != 0) {
        const uint32_t col = lwu(p + 4);
        const int32_t next_a = static_cast<int32_t>(col >> 24) - 2;
        if (next_a > 0) {
            sw(p + 4, (col & 0xffffffu) | (static_cast<uint32_t>(next_a) << 24));
        } else if (lwu(q + 0x14) == 0) {
            const uint32_t pr = lwu(q + 0x10);
            if (pr != 0) sw(pr + 0x34, 0);
            free_particle(p);
        }
    }

    ee_stack_free(0x100);
}

// Type 0x1f: streak pulling both endpoints (+0x10 and +0x20) toward moby +0x30 by factors 0x1601f4 / 0x1601f8.
// orig: 0x0026eb88
void particle_1f_update(uint32_t p) {
    const uint32_t m = lwu(p + 0x30);
    if (m == 0 || lbu(m + 0x20) == 0xfeu || lbu(m + 0x20) == 0xfdu || timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }

    Vec4 diff{}, scaled{};
    vec3_sub(diff, V(m + 0x10), V(p + 0x10));
    scaled = diff;
    const float k0 = fpu_add(fpu_mul(fpu_sub(lwc1(OV(0x001601f4)), 1.0f), lwc1(0x0015ed60)), 1.0f);
    vec3_scale(scaled, scaled, k0);
    vec3_add(diff, scaled, V(m + 0x10));
    V(p + 0x10) = diff;

    const uint32_t m1 = lwu(p + 0x30);
    vec3_sub(diff, V(m1 + 0x10), V(p + 0x20));
    scaled = diff;
    const float k1 = fpu_add(fpu_mul(fpu_sub(lwc1(OV(0x001601f8)), 1.0f), lwc1(0x0015ed60)), 1.0f);
    vec3_scale(scaled, scaled, k1);
    vec3_add(diff, scaled, V(lwu(p + 0x30) + 0x10));
    V(p + 0x20) = diff;

    if (vec3_distance(V(p + 0x10), V(p + 0x20)) < u2f(0x3dcccccdu) ||
        vec3_distance(V(p + 0x10), V(lwu(p + 0x30) + 0x10)) < 1.0f) {
        free_particle(p);
    }
}

// Type 0x24: pulsing beacon offset 0.25 toward the camera from +0x30, clearing word *+0x20 when done.
// orig: 0x0026f898
void particle_24_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    Vec4 to_cam{};
    vec3_sub(to_cam, V(OV(0x00166dc0)), V(p + 0x30));
    if (1.0f < vec3_length(to_cam)) {
        vec3_set_length(to_cam, to_cam, 0.25f);
    } else {
        vec3_scale(to_cam, to_cam, 0.25f);
    }
    vec3_add(V(p + 0x10), V(p + 0x30), to_cam);

    const float ang = angle_add(lwc1(q + 4), lwc1(q + 0x1c));
    swc1(q + 4, ang);
    const float wave = sin_vu(ang);

    const int32_t expired = timer_tick_i16(h16(p + 0xa));
    uint32_t flag_ptr = lwu(q);
    if (expired != 0) {
        if (wave < lwc1(q + 8) || wave < u2f(0xbf666666u)) {
            sw(flag_ptr, 0);
            free_particle(p);
            return;
        }
        flag_ptr = lwu(q);
    }
    if (lw(flag_ptr) == 0) {
        sw(flag_ptr, 0);
        free_particle(p);
        return;
    }

    const float floor_w = lwc1(q + 8);
    if (floor_w < wave) {
        const float t = fpu_div(fpu_sub(wave, floor_w), fpu_sub(1.0f, floor_w));
        sw(p + 4, rgba_lerp_ovl(0, lwu(q + 0xc), t));
    } else {
        sw(p + 4, 0);
    }
}

// Type 0x25: glow offset up to 1.25 toward the camera from +0x20, lerping colour up then down around half-life +0x34.
// orig: 0x0026fa00
void particle_25_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }

    Vec4 to_cam{};
    vec3_sub(to_cam, V(OV(0x00166dc0)), V(q));
    if (5.0f < vec3_length(to_cam)) {
        vec3_set_length(to_cam, to_cam, 1.25f);
    } else {
        vec3_scale(to_cam, to_cam, 0.25f);
    }
    vec3_add(V(p + 0x10), V(q), to_cam);

    const int32_t half = lw(q + 0x14);
    const float denom = static_cast<float>(half);
    int32_t diff = static_cast<int32_t>(static_cast<uint32_t>(lh(p + 0xa)) - static_cast<uint32_t>(half));
    if (diff < 0) diff = static_cast<int32_t>(0u - static_cast<uint32_t>(diff));
    const int32_t level = static_cast<int32_t>(static_cast<uint32_t>(half) - static_cast<uint32_t>(diff));
    sw(p + 4, rgba_lerp_ovl(0, lwu(q + 0x10), fpu_div(static_cast<float>(level), denom)));
}

// Type 0x26: puff spinning (+8), growing by +0x30, fading +0x34 by 0.01 * frame_scale, and damping velocity.
// orig: 0x0026fb00
void particle_26_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }

    sb(p + 8, lbu(p + 8) + 1u);
    swc1(p + 0xc, fpu_add(lwc1(p + 0xc), lwc1(q + 0x10)));
    const float a = fpu_sub(lwc1(q + 0x14), fpu_mul(lwc1(0x0015ed60), u2f(0x3c23d70au)));
    swc1(q + 0x14, a < 0.0f ? 0.0f : a);

    sw(p + 4, lwu(p + 4) & 0xffffffu);
    const uint32_t ca = static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(q + 0x14), 255.0f))) << 24;
    sw(p + 4, lwu(p + 4) | ca);
    if (lwc1(q + 0x14) <= 0.0f) {
        free_particle(p);
        return;
    }

    vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    vec3_scale(V(q), V(q), fpu_add(fpu_mul(lwc1(0x0015ed60), u2f(0xbca3d700u)), 1.0f));
}

}  // namespace rac1
