// The levels' palette blend (Kerwan's 0x23c428, Rilgar's 0x277b20, Umbris's 0x2762e0, Batalia's 0x257d40, Gaspar's 0x26d240,
// Oltanis's 0x255ce0: one routine, the same code in each overlay) - its texel loop on the host.
// orig: (host)
#ifndef RAC1_GAME_OVL_PALETTE_BLEND_H
#define RAC1_GAME_OVL_PALETTE_BLEND_H

#include <cstdint>

namespace rac1 {

// The routine's last loop (its label after the two palettes' conversions; tools/codegen/hooks.glue puts the call there), the
// registers at its start: `a0` the texels written (one word each), `a1` / `a3` the two index streams (a byte each), `t1` the
// texels' count (the loop: t1 >> 2 groups of four). Each texel: the palettes at the scratchpad's 0x0 (a1's index) and 0x2000
// (a3's), converted to floats by the routine, weighted by vf31.w / vf31.x (VMULbc), summed (VADD), to integers (VFTOI0), each
// lane's low byte packed (PPACH / PPACB) into the texel's word. The same values as the transliterated loop (each palette entry's
// product computed once, not per texel: the same product), the same VU0 registers left (vf2..vf9 of the last group; not the ops'
// MAC / status flags, which the routine's code does not read after - RAMHASH the same); true and `*v0` (the routine's return: the
// last group's first word) when done, false (the loop as transliterated) when off - RAC1_PERF_PALETTE_BLEND=0 - or when there is
// nothing to do.
bool palette_blend_loop(uint32_t a0, uint32_t a1, uint32_t a3, uint32_t t1, uint32_t* v0);

}  // namespace rac1

#endif
