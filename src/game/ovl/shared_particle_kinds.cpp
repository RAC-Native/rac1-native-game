// Particle types several levels carry copies of: one port of each, the copies the same code relinked (compared with the
// relocated fields masked, and normalised). They name the engine's words (OV: the particle types' texture tables
// 0x1b20b0 / 0x1b2168 / 0x1b218c) or the base ELF's (bare: the frame scale 0x15ed60, the frame time 0x15ed6c; libc's rand).
// Header shared_particle_kinds.h.
//
// A particle: +2 its texture, +3 its blend, +4 its colour, +8 its spin, +9 its depth bias, +0xa its life, +0xc its size, +0x10
// its position, +0x20.. its type's fields.
#include <game/ovl/shared_particle_kinds.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
int32_t trunc_w(float f) { return static_cast<int32_t>(f2u(eer::cvt_w(f))); }
}  // namespace

// Out of the level's box (2..1021 on each axis) or its life out (+0xa): gone. Else its size shrinks by 1 % a frame (5 %
// once its +0x20 word is 0) and its colour goes from its own toward 0x30ffffff (0x603f1008) with the share of its life
// left.
// orig: L0:0x00275eb0 L5:0x002a00f0 L7:0x0029e4a0
void beam_particle_update(uint32_t p) {
    for (uint32_t k = 0; k < 0xc; k += 4) {
        const float c = lwc1(p + 0x10 + k);
        if (c < 2.0f || 1021.0f < c) {
            free_particle(p);
            return;
        }
    }
    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(p + 0xa))) != 0) {
        free_particle(p);
        return;
    }
    const bool lit = lw(p + 0x20) != 0;
    swc1(p + 0xc, fpu_mul(lwc1(p + 0xc), lit ? u2f(0x3f7d70a4u) : u2f(0x3f733333u)));
    const float left = fpu_div(int_to_float(lh(p + 0xa) - 1), int_to_float(lh(p + 0xa)));
    sw(p + 4, rgba_lerp(lit ? 0x30ffffffu : 0x603f1008u, lwu(p + 4), left));
}

// orig: L0:0x002743a8 L5:0x0029e698 L16:0x0026f7e8
uint32_t particle_43_spawn(uint32_t owner, uint32_t pos, int32_t life, uint32_t mode, uint32_t alpha, uint32_t vel, float a, float b) {
    const uint32_t p = alloc_particle(0x43);
    if (p == 0) return 0;
    const uint32_t q = p + 0x20;
    lq_sq(p + 0x10, pos);
    sw(q + 0xc, owner);
    swc1(q + 0x10, a);
    swc1(q + 0x14, b);
    sw(p + 4, alpha | 0x409b9b00u);
    sb(p + 1, 0);
    sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x40));
    sb(p + 3, 0x48);
    swc1(p + 0xc, 52500.0f);
    sb(p + 8, static_cast<uint32_t>(rand_mod(0xff)));
    sh(q + 0x1c, mode);
    sh(q + 0x18, 0);
    sb(p + 2, lbu(lwu(OV(0x001b218c)) + (mode != 0 ? 1u : 0u)));
    if (mode == 0) sh(q + 0x1a, static_cast<uint32_t>(float_to_int(fpu_div(127.0f, int_to_float(life)))));
    if (vel != 0) {
        swc1(q, lwc1(vel));
        swc1(q + 4, lwc1(vel + 4));
        swc1(q + 8, lwc1(vel + 8));
    }
    sh(p + 0xa, static_cast<uint32_t>(life));
    return p;
}

// orig: L0:0x002729c8 L7:0x0029ae58
uint32_t particle_3a_spawn(uint32_t pos, uint32_t kind, uint32_t b1, uint32_t b2, uint32_t b3, uint32_t b5, uint32_t word, float size,
                           float speed) {
    kind &= 0xff;
    b1 &= 0xff;
    b2 &= 0xff;
    b3 &= 0xff;
    b5 &= 0xff;
    const uint32_t p = alloc_particle(0x3a);
    if (p == 0) return 0;
    lq_sq(p + 0x10, pos);
    const uint32_t q = p + 0x20;
    sb(p + 1, 0);
    sb(p + 3, 0x44);
    sb(p + 9, static_cast<uint32_t>(float_to_int(0.0f) + 0x70));
    sb(p + 2, lbu(lwu(OV(0x001b2168)) + (static_cast<uint32_t>(libc_rand()) & 7u)));
    swc1(p + 0xc, 21000.0f);
    sb(p + 8, static_cast<uint32_t>(libc_rand()));
    sb(q + 0x10, kind);
    sb(q + 0x12, b2);
    sb(q + 0x13, b3);
    sw(q + 0x18, word);
    swc1(q + 0x1c, size);
    sb(q + 0x11, b1);
    sb(q + 0x15, b5);
    if (kind < 2) {
        sh(p + 0xa, b1);
        sw(p + 4, 0xff808080u);
        sb(q + 0x14, 0);
    } else if (kind == 2) {
        const float throw_speed = fpu_mul(fpu_mul(rand_range_f(3.0f, 9.0f), speed), lwc1(0x0015ed6c));
        const float yaw = rand_angle();
        const float pitch = rand_range_f(u2f(0x3fac0508u), u2f(0x3fe61aadu));
        vec_from_spherical_v(V(q), throw_speed, yaw, pitch);
        swc1(p + 0xc, 42000.0f);
        const uint32_t a = (static_cast<uint32_t>(libc_rand()) & 0x1fu) + 0x10u;
        sb(q + 0x16, a);
        sh(p + 0xa, b5);
        sw(p + 4, ((a & 0xffu) << 24) | 0x808080u);
        sb(q + 0x14, kind);
    }
    return p;
}

// orig: L0:0x0026fd28 L10:0x00262b08
uint32_t particle_28_spawn(uint32_t pos, uint32_t vel, uint32_t kind, uint32_t word) {
    kind &= 0xff;
    const uint32_t p = alloc_particle(0x28);
    if (p == 0) return 0;
    const uint32_t q = p + 0x20;
    lq_sq(p + 0x10, pos);
    lq_sq(q, vel);
    const float grey = u2f(0x3f333333u);
    sw(p + 4, rgba_pack(grey, grey, grey, u2f(0x3da3d70au)));
    sb(p + 1, 0);
    sb(p + 3, 0x44);
    sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x40));
    const float spin = rand_angle();
    swc1(q + 0x10, spin);
    sb(p + 8, static_cast<uint32_t>(trunc_w(fpu_mul(spin, 256.0f))));
    sb(p + 2, lbu(lwu(OV(0x001b20b0)) + (static_cast<uint32_t>(libc_rand()) & 7u)));
    sb(q + 0x18, kind);
    sh(q + 0x14, 0);
    sh(q + 0x16, 0);
    const float rate = u2f(0x3ee2d0e5u);
    if (kind < 5) {
        swc1(p + 0xc, rand_range_f(7000.0f, 10000.0f));
        const int32_t r = libc_rand();
        const int32_t life = trunc_w(fpu_div(8.0f, fpu_mul(lwc1(0x0015ed60), rate)));
        sh(p + 0xa, static_cast<uint32_t>(life - ee_rem(r, ee_div(life * 6, 7))));
    } else {
        const float scale = fpu_mul(lwc1(0x0015ed60), rate);
        swc1(p + 0xc, 10000.0f);
        sh(p + 0xa, static_cast<uint32_t>(trunc_w(fpu_div(8.0f, scale))));
    }
    sw(q + 0x1c, word);
    return p;
}

}  // namespace rac1
