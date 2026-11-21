// Classes 1221 and 1222, falling pieces: one port of the eight copies (each level has one per class) Kerwan, Umbris, Hoven
// and Quartu carry, the same code relinked (compared with the relocated fields masked, and normalised). They name only the
// base ELF's words (bare: the frame time's square 0x15ed70). Header shared_class1221.h.
//
// The piece's pvars: its velocity +0, its spin +0x10..+0x18, its life +0x20 and what is left of it +0x24.
#include <game/ovl/shared_class1221.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// It falls (gravity 9.8) and moves by its velocity, spins, its alpha the share of its life left (of 255), and leaves when
// its life runs out.
// orig: L3:0x002de370 L3:0x002de458 L7:0x0031d9f8 L7:0x0031dae0 L12:0x00301d08 L12:0x00301df0 L15:0x002e5e70 L15:0x002e5f58
void class1221_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float fall = fpu_mul(lwc1(0x0015ed70), u2f(0x411ccccdu));
    swc1(pv + 8, fpu_sub(lwc1(pv + 8), fall));
    vec3_add_inplace(V(m + 0x10), V(pv));
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0x10)));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x14)));
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x18)));
    const float left = int_to_float(lw(pv + 0x24));
    const float life = int_to_float(lw(pv + 0x20));
    sb(m + 0x23, static_cast<uint32_t>(float_to_int(fpu_div(fpu_mul(left, 255.0f), life))));
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x24))) != 0) moby_retire(m);
}

}  // namespace rac1
