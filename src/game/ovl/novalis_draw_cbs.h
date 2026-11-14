// Novalis's own draw callbacks: the functions its classes queue with draw_callback_add (0x1f9598) /
// 0x1f93e0 and the frame calls through ovl_call_moby_fn. Transliterated.
#ifndef RAC1_GAME_OVL_NOVALIS_DRAW_CBS_H
#define RAC1_GAME_OVL_NOVALIS_DRAW_CBS_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

TrRet nov_30ef18_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x0030ef18
void nov_class1848_draw_l1(uint32_t a0);   // L1:0x0030f0e0  class 1848's five scrolling strips (VU1 program 7), queued by its update 0x30f208
// orig: (none) — registers them for the indirect calls (ovl_call_moby_fn)
void novalis_draw_cbs_register();

}  // namespace rac1
#endif
