// The debris pieces, one port of the copies Rilgar (class 980) and Batalia (class 344 and the classes that share its
// update) carry: the same code relinked (compared with the relocated fields masked, and normalised: no other difference).
// They name the engine's words (OV: the collision's result 0x173e40.., its moby +0x18, its normal +0x40) or the base ELF's
// (bare: the frame time 0x15ed6c and its square 0x15ed70). Header shared_class344.h.
//
// A piece's pvars: its life +0, the joint it hangs from +4 (when above 0), the moby it ignores +8, its radius +0xc (an
// integer, 1024ths), its offset +0x10, its spin +0x20..+0x28, its velocity +0x30.
#include <game/ovl/shared_class344.h>

#include <game/math/vecmath.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// State 0: a moby drawn mirrored (+0x32 bit 0) keeps its flag 0x8000 (+0x34); state 2 (still). State 1 (flying): it spins
// (the angles +0x40.. by the spin rates, its matrix rebuilt: its y row negated when mirrored), its centre (the joint's, or
// the offset turned) tested against the world (radius +0xc): on a hit of the world or of another moby than the one it
// ignores, its life stops and, when moving into the surface, it bounces off at half its speed. When its life runs out it
// fades (128 in a third of a second) and leaves once faded. It falls (gravity 10.8), moves by its velocity and by the
// turn of its centre, and leaves below a height of 5.
// orig: L5:0x003186f8 L8:0x002dba40
void class344_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        if ((lhu(m + 0x32) & 1u) != 0) sh(m + 0x34, lhu(m + 0x34) | 0x8000u);
        sb(m + 0x20, 2);
        return;
    }
    if (st != 1) return;
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t rot = sp, was = sp + 0x40, now = sp + 0x50, turn = sp + 0x60;
    const uint32_t pos = m + 0x10, mat = m + 0xc0, vel = pv + 0x30;
    auto spin = [&] {
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0x20)));
        swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x24)));
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x28)));
        euler_to_mat4_vu(M(rot), V(m + 0x40));
    };
    uint32_t centre;
    if (lw(pv + 4) > 0) {
        moby_chain_point(m, lwu(pv + 4), was);
        spin();
        mat3_copy(M(mat), M(rot));
        moby_chain_point(m, lwu(pv + 4), now);
        vec3_sub(V(turn), V(was), V(now));
        centre = now;
    } else {
        mat3_transform(V(was), V(pv + 0x10), M(mat));
        spin();
        if ((lhu(m + 0x34) & 0x8000u) != 0) vec3_scale(V(rot + 0x10), V(rot + 0x10), -1.0f);
        mat3_copy(M(mat), M(rot));
        mat3_transform(V(now), V(pv + 0x10), M(mat));
        vec3_sub(V(turn), V(was), V(now));
        vec3_add(V(now), V(now), V(pos));
        centre = now;
    }
    const float radius = fpu_mul(int_to_float(lw(pv + 0xc)), u2f(0x3a800000u));
    if (sphere_collide(centre, radius, 0, m) != 0) {
        const uint32_t hit = OV(0x00173e40);
        const uint32_t other = lwu(hit + 0x18);
        if (other == 0 || other != lwu(pv + 8)) {
            const uint32_t normal = hit + 0x40;
            sw(pv, 0);
            if (vec3_dot(V(vel), V(normal)) < 0.0f) {
                vec3_reflect(V(vel), V(vel), V(normal));
                vec3_scale(V(vel), V(vel), 0.5f);
            }
        }
    }
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv))) != 0) {
        const float step = fpu_div(128.0f, static_cast<float>(frame_scale_i(0x14)));
        sw(pv, 0);
        const uint32_t alpha = lbu(m + 0x23);
        if (step <= static_cast<float>(static_cast<int32_t>(alpha)))
            sb(m + 0x23, alpha - static_cast<uint32_t>(static_cast<int32_t>(f2u(eer::cvt_w(step)))));
        else
            moby_retire(m);
    }
    swc1(pv + 0x38, fpu_sub(lwc1(pv + 0x38), fpu_mul(lwc1(0x0015ed70), u2f(0x412ccccdu))));
    vec3_add(V(pos), V(pos), V(vel));
    vec3_add(V(pos), V(pos), V(turn));
    if (lwc1(m + 0x18) < 5.0f) moby_retire(m);
    ee_stack_free(0xf0);
}

// orig: L5:0x00318648 L8:0x002db990
void class344_throw(uint32_t m, uint32_t /*unused*/, uint32_t vel, uint32_t offset, int32_t frames) {
    const uint32_t pv = lwu(m + 0x78);
    sb(m + 0x20, 1);
    lq_sq(pv + 0x30, vel);
    lq_sq(pv + 0x10, offset);
    sw(pv, static_cast<uint32_t>(frame_scale_i(frames)));
    const float rate = u2f(0x40860a92u);  // (240 degrees a second)
    for (uint32_t k = 0; k < 0xc; k += 4) {
        const float most = fpu_mul(lwc1(0x0015ed6c), rate);
        swc1(pv + 0x20 + k, rand_range_f(-most, most));
    }
}

}  // namespace rac1
