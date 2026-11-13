// Blarg's and Veldin's finale's flash (class 1898) and two small helpers of both levels (shared_flash.cpp: one port for
// both levels' copies; the state query is another build on each, both written). Phase 1 of stage 4: the generated copies
// still run; proved against them by oracle batch 510.
#ifndef RAC1_GAME_OVL_SHARED_FLASH_H
#define RAC1_GAME_OVL_SHARED_FLASH_H

#include <cstdint>

namespace rac1 {

void flash_update(uint32_t m);  // class 1898 (L6:0x0030be60, L18:0x002fb548)
// its draw (the callback flash_update queues): a screen quad at its point, growing, its colour fading (L6:0x0030bfa8 L18:0x002fb690)
void flash_draw_quad(uint32_t m);
// a flash at `pos` (EE vector) over `frames` frames, its two words `a` / `b`; 1, or 0 when none could be spawned (L6:0x0030bef8, L18:0x002fb5e0)
uint32_t flash_spawn(uint32_t pos, int32_t frames, uint32_t a, uint32_t b);
// a camera of type 0x12: its record's flag +0x50 set (L6:0x00316330, L18:0x00306338)
void camera_type12_flag(uint32_t cam);
// a moby's state as its class asks it: Blarg's build (L6:0x003047f8): 2 in state 7, else 1 past state 3, else 0; the
// finale's (L18:0x002dfba0): 1 in state 2, 2 in state 3, else 0
uint32_t state_query_blarg(uint32_t m);
uint32_t state_query_finale(uint32_t m);

}  // namespace rac1

#endif
