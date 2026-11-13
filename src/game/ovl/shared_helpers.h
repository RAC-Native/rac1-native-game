// Small helpers several levels carry copies of, each ported once (shared_helpers.cpp). Each function's `orig:` line lists
// every level's address. A function of Veldin's own (an L0 copy) is declared with its levels' addresses only, without
// the `orig:` prefix the code generator reads: the copies stay transliterated in their units until each level is
// re-glued (then the declaration takes Veldin's `// orig: 0x...` form).
#ifndef RAC1_GAME_OVL_SHARED_HELPERS_H
#define RAC1_GAME_OVL_SHARED_HELPERS_H

#include <cstdint>

namespace rac1 {

// The talker entry of moby `m` (talker_index: the 16-byte entries at 0x13d5b0) gets `value` in its word +0xc.
void talker_word_set(uint32_t m, uint32_t value);  // L0:0x00266858 L3:0x00254a10 L5:0x002907b8 L7:0x0028ec18 L10:0x00259700 L11:0x0028bc28 L13:0x00272260 L14:0x0026e1c0 L16:0x00261c98
// `count` records of 0x1190 bytes at `base` cleared (three blocks of 0x5c0 each) and their limits set.
void grid_records_reset(uint32_t base, int32_t count);  // L1:0x002b7a48 L5:0x002cc428 L7:0x002ca980 L11:0x002c6bd0 L12:0x002ba560 L13:0x002ae368
// The point `p` in the grid's fixed point (1024ths, shifted by the grid's origin) into `out` (three words and two halves).
void grid_point_fixed(uint32_t p, uint32_t out);  // L1:0x002b7c68 L5:0x002cc648 L7:0x002caba0 L11:0x002c6df0 L12:0x002ba780 L13:0x002ae588
// The grid's 92 weights scaled by `scale` / their 17th (taken before the scaling).
void grid_weights_scale(float scale);  // L1:0x002b7ef0 L5:0x002cc8d0 L7:0x002cae28 L11:0x002c7078 L12:0x002baa08 L13:0x002ae810
// A jump's arc of height `height` under gravity `g` into the record `rec` (its horizontal reach +0x10 given): the
// launch speed +0x1c, the time's factor +0x18, the curve +0x14.
void jump_arc_set(uint32_t rec, float height, float g);  // L3:0x00250a78 L4:0x00255150 L6:0x00271ac0 L10:0x00255a40 L15:0x00251c90
// The record's band of 0.2 around its +0x18: +0x84 below, +0x88 above.
void band_set(uint32_t rec);  // L2:0x0025c758 L3:0x00248b78 L5:0x002843a8 L12:0x00272fe0 L15:0x002499d8
// A GS packet of 9 qwords at the level's packet cursor (*0x1611c0, advanced by 0xa0): its tags, the template 0x1609a0,
// PRIM 5 (with `textured`) or 0, the register word `regs`, then four vertices from `uv` (doublewords), `colour` and
// `xyz` (words).
void gs_quad_packet(uint32_t xyz, uint32_t colour, uint32_t uv, uint64_t regs, uint32_t textured);  // L2:0x0020bc88 L4:0x001ed6b8 L6:0x00212938 L8:0x0020d760 L11:0x0022c5b8 L12:0x0021d810 L13:0x00206738 L18:0x00203a38
// A float timer `t` counted down by the frame's time (0x15ed7c): 0 while it runs; when it runs out it is cleared and
// the result is -1 when it was not above 0 (its sign bit, or +0), else -2.
int32_t float_timer_tick(uint32_t t);  // L8:0x00211b08 L12:0x00221ec8 L14:0x00212598 L15:0x001f8730 L17:0x001fc060
// Class hook +0xc of Batalia's and Oltanis's class 252: the carry hook (carry_hookc), then state 3 with its pvars'
// +0x200 4 / +0x208 0.5, the animation set 2, +0xc8 cleared. Returns the carry hook's result.
int32_t class252_hook_c(uint32_t m, uint32_t a1, uint32_t a2, float f12);  // L8:0x002d4050 L14:0x002dae50

// A stub several levels' code points at (camera types' accept functions, class hooks): 0.
// (Blarg's L6:0x00217300, which the plan's text comparison put in this family, stores vf0 at its argument: another
// function, left out.)
int32_t zero_stub();  // L1:0x002c72c0 L5:0x00326280 L5:0x0032a6d8 L6:0x002be860 L8:0x002025d0 L8:0x002025d8 L8:0x002ba660 L8:0x003127d0 L9:0x002d0b18 L12:0x00212638 L14:0x00202db8 L15:0x002aa948 L16:0x002f0d88 L16:0x002f51e0 L17:0x002af398 L17:0x002fd208
// With the game playing (mode 2) and the flag 0x15edb0 set: each moby of `m`'s class in the list 0x16c860 (+0x178, its
// count +0x44) puts its joint `joint`'s point into the record 0x17c440 (sub_0024f590) when the record is free (+1 0),
// sized `size` (+0x20 / +0x24 / +0x28).
void class_mates_mark(uint32_t m, uint32_t joint, float size);
// The copies with the size 2.75 (joint 1 at Hoven's 0x2e6b60, Quartu's 0x2ec678, Kalebo III's 0x2e30a8; else 0).
void class_mates_mark_fixed(uint32_t m, uint32_t joint);  // L2:0x002e0cd8 L2:0x002e1868 L4:0x002e2f90 L5:0x00317fb8 L8:0x00302c00 L11:0x002d0ec0 L11:0x002f0d58 L12:0x002e6b60 L14:0x002fb938 L14:0x002feef8 L15:0x002ec678 L16:0x002e30a8 L16:0x002e6720
// The copies with the size the level's own float holds (joint 1 on Kerwan, Blarg and Hoven; 0 on Rilgar and Pokitaru).
void class_mates_mark_level(uint32_t m, uint32_t joint);  // L3:0x002db480 L5:0x00317398 L6:0x00300f98 L11:0x002d0638 L12:0x002eb498
// A carry hook (`which`: carry_hook0 / 4 / 8) that puts the moby in state `carried` while it holds, and back to
// `released` (-1: its +0xbc) when it lets go in that state. Returns the carry hook's result.
int32_t carry_state_hook(uint32_t m, uint32_t a1, uint32_t a2, float f12, uint32_t which, uint32_t carried, int32_t released);  // L8:0x002d4000 L12:0x002e1758 L12:0x002e17a8 L14:0x002dae00 L18:0x002d6190 L18:0x002d61e0 L18:0x002fcbb0 L18:0x002fcc00 L18:0x002fcc50

// A burst of sparks at `pos` (an EE address) around moby `m`'s axes (its matrix rows +0xc0 / +0xe0): 24 sparks
// (particle 0x2712b8's) spread round a turn, then 6 puffs (particle 0x15), each drifting by `offset` (an EE vector) when
// it is not 0.
void spark_burst(uint32_t m, uint32_t pos, uint32_t offset);  // L3:0x00250ae8 L10:0x00255ab0 L15:0x00251d00 L18:0x00264c10

// Class hooks that put the moby in one state and return it (each copy its own state: Kerwan 0xe, Blarg 3 / 2 / 5, Oltanis
// 2 / 1, Kalebo III 1). (The finale's L18:0x002dfb90, which the plan's text comparison put with them, returns its pvars'
// +0x60 instead: another function, left out.)
uint32_t moby_state_hook(uint32_t m, uint32_t state);  // L3:0x002c6c90 L6:0x002fc630 L6:0x00303150 L6:0x00304088 L14:0x002fe228 L14:0x002ffc08 L16:0x002de3a8

}  // namespace rac1

#endif
