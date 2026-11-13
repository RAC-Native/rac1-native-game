// The carried mobys of Hoven (level 12) and Veldin's finale (18): mobys a carrier (a platform, a ship) takes along, kept in
// attachment records in its frame. The two levels' copies are the same code, relinked; only the moby list 0x15ffd8 (OV).
// Header shared_attach.h.
#include <game/ovl/shared_attach.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// orig: L12:0x0027b268 L18:0x00265250
void attach_bind(uint32_t carrier, uint32_t recs, int32_t n) {
    // (the frame's slots as the original lays them out: the carrier's turn +0, its inverse +0x30 (its fourth row the frame's
    // leftover), a moby's turn +0x70)
    const uint32_t sp = ee_stack_alloc(0xb0);
    const uint32_t turn = sp, inverse = sp + 0x30, other = sp + 0x70;
    euler_to_mat3_vu(M(turn), V(carrier + 0x40));
    mat3_transpose(M(inverse), M(turn));
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t rec = recs + static_cast<uint32_t>(i) * 0x30;
        const int32_t index = lw(rec + 0x20);
        const uint32_t list = lwu(OV(0x0015ffd8));
        if (index == -1) continue;
        const uint32_t m = list + (static_cast<uint32_t>(index) << 8);
        vec3_sub(V(rec), V(m + 0x10), V(carrier + 0x10));
        mat4_transform(V(rec), V(rec), M(inverse));
        euler_to_mat4_vu(M(other), V(m + 0x40));
        mat4_mul(M(other), M(inverse), M(other));
        mat_to_euler(other, rec + 0x10);
    }
    ee_stack_free(0xb0);
}

// orig: L12:0x0027b370 L18:0x00265358
void attach_carry(uint32_t carrier, uint32_t recs, int32_t n) {
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t frame = sp, turn = sp + 0x40, offset = sp + 0x80;
    euler_to_mat4_vu(M(frame), V(carrier + 0x40));
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t rec = recs + static_cast<uint32_t>(i) * 0x30;
        const int32_t index = lw(rec + 0x20);
        if (index == -1) continue;
        const uint32_t m = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(index) << 8);
        euler_to_mat4_vu(M(turn), V(rec + 0x10));
        mat4_mul(M(turn), M(frame), M(turn));
        mat_to_euler(turn, m + 0x40);
        mat4_transform(V(offset), V(rec), M(frame));
        vec3_add(V(m + 0x10), V(carrier + 0x10), V(offset));
        moby_update_transform(m);
    }
    ee_stack_free(0x90);
}

}  // namespace rac1
