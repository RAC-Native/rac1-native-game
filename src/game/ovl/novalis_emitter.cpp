// Novalis's own moby classes: the particle emitter placed in the level's data (class 27; hand-ported from Novalis's own
// overlay). Header novalis_mobys2.h, registered in novalis_mobys2.cpp. Addresses are Novalis's (`orig: L1:`); Novalis-only
// data is L1_ADDR, the base ELF's are bare (the level number 0x15ed84, the time step 0x15ed60).
//
// The pvars: the emitter's moby +0x7c; its flags +0x70 (0x1000 the velocity as angles about a direction, 0x2000 the spread
// turned with the moby, 0x4000 the velocity turned with it); the velocity: a direction +0x80 and the angles' spread
// +0x90 .. +0x98 (degrees), or a box +0x80 .. +0x98 (a time step); the fade's range +0xa0 / +0xa4, the size's range +0xa8 /
// +0xac, the colour +0xb0 .. +0xbc, the life +0xc0 (frames); the burst count +0xc4, the wait between bursts +0xc5, the
// texture +0xc6 and its set +0xc7; the countdown +0xc8; the spread +0xcc / +0xd0 / +0xd4.
#include <game/math/vecmath.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
// a stack frame of EE memory for the vectors handed to the engine by address (cleared when it goes)
struct Frame {
    uint32_t sp, n;
    explicit Frame(uint32_t size) : sp(ee_stack_alloc(size)), n(size) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};
}  // namespace

// A particle of type 6 for the emitter whose pvars are `owner` (0: none) at `pos` (an EE vector) moving at (vx, vy, vz), for
// `life` frames (frame-scaled): with `blend` 0xff the blend by the texture's low bit (0x44 / 0x48) and the texture byte from
// `tex` (its bits 5.. x 16 + 4), else `tex` and `blend` as they are; the texture of the level's list 0x1b2500 [`set`]; size
// `size`, the fade `fade` (+0x34, and x 255 its byte +8), the colour (r, g, b) (x 65535 bytes +0x38.., and packed with the
// alpha `a` into +4), the alpha `a` (+0x30).
// orig: L1:0x0027e900
void nov_27e900_l1(uint32_t owner, uint32_t pos, int32_t life, uint32_t tex, uint32_t set, uint32_t blend, float vx, float vy, float vz,
                   float size, float fade, float r, float g, float b, float a) {
    const Vec4 at = V(pos);
    if (owner == 0) return;
    const uint32_t p = alloc_particle(6);
    if (p == 0) return;
    V(p + 0x10) = at;
    tex &= 0xff;
    set &= 0xff;
    blend &= 0xff;
    if (blend != 0xff) {
        sb(p + 9, tex);
        sb(p + 3, blend);
    } else {
        sb(p + 9, ((tex >> 5) << 4) + static_cast<uint32_t>(float_to_int(4.0f)));
        sb(p + 3, (tex & 1) != 0 ? 0x48 : 0x44);
    }
    sb(p + 1, 0);
    sb(p + 2, lbu(lwu(L1_ADDR(0x001b2500) + (set << 2))));
    swc1(p + 0x20, vx);
    swc1(p + 0x24, vy);
    swc1(p + 0x28, vz);
    swc1(p + 0x34, fade);
    sb(p + 0x38, static_cast<uint32_t>(float_to_int(fpu_mul(r, 65535.0f))));
    sb(p + 0x39, static_cast<uint32_t>(float_to_int(fpu_mul(g, 65535.0f))));
    sb(p + 0x3a, static_cast<uint32_t>(float_to_int(fpu_mul(b, 65535.0f))));
    swc1(p + 0x30, a);
    sw(p + 0x2c, owner);
    swc1(p + 0xc, size);
    sb(p + 8, f2u(eer::cvt_w(fpu_mul(fade, 255.0f))));
    sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(life)));
    sb(p + 0x3b, 0);
    sw(p + 4, rgba_pack(r, g, b, a));
}

// Class 27: on Novalis only while its place (radius 20) is on screen; every +0xc5 frames a burst of +0xc4 particles of
// type 6 spread about it (turned with it when flagged), their velocity a random turn of its direction (flag 0x1000) or a
// random vector of its box scaled by the time step (turned with it when flagged).
// orig: L1:0x002bd100
void nov_class27_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0xa0);
    const uint32_t sp = f.sp;
    const uint32_t vel = sp + 0x10, at = sp + 0x20, spread = sp + 0x30, turn = sp + 0x40, box_turn = sp + 0x50, angles = sp + 0x80;
    if (lwu(0x0015ed84) == 1) {
        V(at) = V(m + 0x10);
        swc1(at + 0xc, 20.0f);
        if (point_on_screen(at, 240.0f) == -1) return;
    }
    const int32_t wait = lw(pv + 0xc8);
    sw(pv + 0x7c, m);
    if (wait > 0) {
        sw(pv + 0xc8, static_cast<uint32_t>(wait - 1));
        return;
    }
    sw(pv + 0xc8, lbu(pv + 0xc5));
    if (lbu(pv + 0xc4) == 0) return;
    const float pi = u2f(0x40490fdbu);
    auto degrees = [pi](float lim) { return fpu_div(fpu_mul(rand_range_f(-lim, lim), pi), 180.0f); };
    int32_t n = 0;
    do {
        V(at) = V(m + 0x10);
        swc1(spread, rand_range_f(-lwc1(pv + 0xcc), lwc1(pv + 0xcc)));
        swc1(spread + 4, rand_range_f(-lwc1(pv + 0xd0), lwc1(pv + 0xd4)));
        swc1(spread + 8, rand_range_f(-lwc1(pv + 0xd4), lwc1(pv + 0xd4)));
        if ((lwu(pv + 0x70) & 0x2000) != 0) {
            euler_to_mat4_vu(M(turn), V(m + 0x40));
            mat4_transform(V(spread), V(spread), M(turn));
        }
        vec3_add(V(at), V(at), V(spread));
        float vx, vy, vz;
        if ((lwu(pv + 0x70) & 0x1000) != 0) {
            swc1(angles, degrees(lwc1(pv + 0x90)));
            swc1(angles + 4, degrees(lwc1(pv + 0x94)));
            swc1(angles + 8, degrees(lwc1(pv + 0x98)));
            if ((lwu(pv + 0x70) & 0x4000) != 0) {  // (the second and third from the first: the original's own order)
                const float x = angle_add(lwc1(angles), lwc1(m + 0x40));
                swc1(angles, x);
                swc1(angles + 4, angle_add(x, lwc1(m + 0x44)));
                swc1(angles + 8, angle_add(lwc1(angles), lwc1(m + 0x48)));
            }
            euler_to_mat4_vu(M(turn), V(angles));
            mat4_transform(V(vel), V(pv + 0x80), M(turn));
            vx = lwc1(vel);
            vy = lwc1(vel + 4);
            vz = lwc1(vel + 8);
        } else {
            const float step = lwc1(0x0015ed60);
            swc1(turn, fpu_mul(rand_range_f(lwc1(pv + 0x80), lwc1(pv + 0x90)), step));
            swc1(turn + 4, fpu_mul(rand_range_f(lwc1(pv + 0x84), lwc1(pv + 0x94)), lwc1(0x0015ed60)));
            swc1(turn + 8, fpu_mul(rand_range_f(lwc1(pv + 0x88), lwc1(pv + 0x98)), lwc1(0x0015ed60)));
            if ((lwu(pv + 0x70) & 0x4000) != 0) {
                euler_to_mat4_vu(M(box_turn), V(m + 0x40));
                mat4_transform(V(turn), V(turn), M(box_turn));
            }
            vx = lwc1(turn);
            vy = lwc1(turn + 4);
            vz = lwc1(turn + 8);
        }
        const float size = rand_range_f(lwc1(pv + 0xa8), lwc1(pv + 0xac));
        const float fade = rand_range_f(lwc1(pv + 0xa0), lwc1(pv + 0xa4));
        nov_27e900_l1(pv, at, lw(pv + 0xc0), lbu(pv + 0xc6), lbu(pv + 0xc7), 0xff, vx, vy, vz, size, fade, lwc1(pv + 0xb0),
                      lwc1(pv + 0xb4), lwc1(pv + 0xb8), lwc1(pv + 0xbc));
    } while (++n < static_cast<int32_t>(lbu(pv + 0xc4)));
}

}  // namespace rac1
