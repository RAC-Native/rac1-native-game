// Class 235, a trail a moby leaves: one port of the copies Kerwan and Kalebo III carry (the same code relinked; compared
// with the relocated fields masked, and normalised: no other difference). It names only the base ELF's words (bare: the
// frame time 0x15ed6c and the frame scale 0x15ed60). Header shared_class235.h.
//
// Its pvars: the moby it follows +0 (that moby's pvars +0xf0: the point it starts from), its colours at its end +4 and its
// start +8, its life +0xc and what is left of it +0xe, its distance back +0x10 and the speed it moves back +0x14.
#include <game/ovl/shared_class235.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// Each frame: its life ticks down (it leaves at the end); it is turned as its moby and put at its moby's point, moved
// back along its turn by its distance (which grows by its speed), spun about x (300 degrees a second); its colour goes
// from its start's to its end's and its alpha from 128 to 0 with its life, and it grows by 2 % a frame (frame-scaled).
// orig: L3:0x002bae48 L16:0x002c3be8
void class235_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0xe))) != 0) {
        moby_retire(m);
        return;
    }
    const uint32_t pos = m + 0x10;
    swc1(m + 0x44, lwc1(lwu(pv) + 0x44));
    swc1(m + 0x48, lwc1(lwu(pv) + 0x48));
    lq_sq(pos, lwu(lwu(pv) + 0x78) + 0xf0);
    const float back = fpu_add(lwc1(pv + 0x10), lwc1(pv + 0x14));
    swc1(pv + 0x10, back);
    sub_00262cf0(pos, pos, -back, lwc1(m + 0x48), lwc1(m + 0x44));
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), fpu_mul(lwc1(0x0015ed6c), u2f(0x40a78d36u))));
    const float left = fpu_div(int_to_float(lh(pv + 0xe)), int_to_float(lh(pv + 0xc)));
    const uint32_t colour = rgba_lerp(lwu(pv + 8), lwu(pv + 4), left);
    moby_set_color_hi(m, colour & 0xff, (colour >> 8) & 0xff, (colour >> 16) & 0xff);
    sb(m + 0x23, static_cast<uint32_t>(float_to_int(fpu_mul(left, 128.0f))));
    const float grow = fpu_add(fpu_mul(lwc1(0x0015ed60), u2f(0x3ca3d700u)), 1.0f);
    swc1(m + 0x2c, fpu_mul(lwc1(m + 0x2c), grow));
}

}  // namespace rac1
