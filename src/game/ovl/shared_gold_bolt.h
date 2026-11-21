// Class 1134, the gold bolt: it spins and bobs over its place with a ring of glitter and sparkles; the hero taking it starts
// its scene (the camera about the hero, the glitter at its hand) and then the level change to the gold bolt's tally. One port
// of the copies every level from Novalis to Veldin's finale carries (shared_gold_bolt.cpp), and the glitter's pvars set up
// (the gold bolt's, and another build of it at the pvars' start other classes use). Phase 1 of stage 4: the levels'
// generated copies still run; these are proved against them by oracle batch 514.
#ifndef RAC1_GAME_OVL_SHARED_GOLD_BOLT_H
#define RAC1_GAME_OVL_SHARED_GOLD_BOLT_H

#include <cstdint>

namespace rac1 {

void gold_bolt_update(uint32_t m);  // L1:0x00307ca0 L2:0x002ead38 L3:0x002dd188 L4:0x002e1f38 L5:0x00319c60 L6:0x003053a8 L7:0x0031b5f0 L8:0x00303370 L9:0x003022f0 L10:0x002dd8e0 L11:0x0030b730 L12:0x002ffdb0 L13:0x00303950 L14:0x002ffc18 L15:0x002e4770 L16:0x002df1e8 L17:0x002e3ff8 L18:0x002ecc40
// The ring of four glitter particles (0x3b) about it, faced to the camera, their colours cycling.
void gold_bolt_glitter(uint32_t m);  // L1:0x00308550 L2:0x002eb5e8 L3:0x002dda38 L4:0x002e27e8 L5:0x0031a510 L6:0x00305c58 L7:0x0031bea0 L8:0x00303c20 L9:0x00302ba0 L10:0x002de190 L11:0x0030bfe0 L12:0x00300660 L13:0x00304200 L14:0x003004c8 L15:0x002e5020 L16:0x002dfa98 L17:0x002e48a8 L18:0x002ed4f0
// The scene's camera: about the hero (no path) or along the level's camera path's two points.
void gold_bolt_camera(uint32_t m);  // L1:0x003087e0 L2:0x002eb878 L3:0x002ddcc8 L4:0x002e2a78 L5:0x0031a7a0 L6:0x00305ee8 L7:0x0031c130 L8:0x00303eb0 L9:0x00302e30 L10:0x002de420 L11:0x0030c270 L12:0x003008f0 L13:0x00304490 L14:0x00300758 L15:0x002e52b0 L16:0x002dfd28 L17:0x002e4b38 L18:0x002ed780
// Four sparkles (particle 0x3c) flying off it.
void gold_bolt_sparkles(uint32_t m);  // L1:0x003089f0 L2:0x002eba88 L3:0x002dded8 L4:0x002e2c88 L5:0x0031a9b0 L6:0x003060f8 L7:0x0031c340 L8:0x003040c0 L9:0x00303040 L10:0x002de630 L11:0x0030c480 L12:0x00300b00 L13:0x003046a0 L14:0x00300968 L15:0x002e54c0 L16:0x002dff38 L17:0x002e4d48 L18:0x002ed990
// Its spin and bob, the glitter and now and then the sparkles (in the scene).
void gold_bolt_spin(uint32_t m);  // L1:0x00308380 L2:0x002eb418 L3:0x002dd868 L4:0x002e2618 L5:0x0031a340 L6:0x00305a88 L7:0x0031bcd0 L8:0x00303a50 L9:0x003029d0 L10:0x002ddfc0 L11:0x0030be10 L12:0x00300490 L13:0x00304030 L14:0x003002f8 L15:0x002e4e50 L16:0x002df8c8 L17:0x002e46d8 L18:0x002ed320
// The glitter's pvars set up at `at` in the moby's pvars (the gold bolt's 0x20; 0 the other build's): four random colour
// phases, their rates, the timers, the sizes.
void glitter_pvars_init(uint32_t m, uint32_t at);  // at 0x20: L1:0x00308470 L2:0x002eb508 L3:0x002dd958 L4:0x002e2708 L5:0x0031a430 L6:0x00305b78 L7:0x0031bdc0 L8:0x00303b40 L9:0x00302ac0 L10:0x002de0b0 L11:0x0030bf00 L12:0x00300580 L13:0x00304120 L14:0x003003e8 L15:0x002e4f40 L16:0x002df9b8 L17:0x002e47c8 L18:0x002ed410; at 0: L2:0x002ea7d0 L6:0x002f4bf8 L15:0x002eba70 L17:0x002f1488

}  // namespace rac1

#endif
