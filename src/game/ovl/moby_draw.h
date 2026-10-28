// The moby pass (ADR 0007): the hand-written draw loop that culls the level's mobys against the
// view, lights them and writes, per moby, the VIF1 chain for the VU1 moby programs into the
// frame's packet buffer (0x1611c0) and a skinning record through the scratchpad into the ring
// (0x15ffd4) the later joint pass fills the chain's matrices from.
#ifndef RAC1_GAME_OVL_MOBY_DRAW_H
#define RAC1_GAME_OVL_MOBY_DRAW_H

#include <cstdint>

namespace rac1 {

// The draw loop: mobys from `first` (0x100 bytes each, the list ended by +0x20 == -1), records
// into the ring at `ring`; `defer` set: the mobys whose drop shadow is seen (+0x34 & 0xc00) kept
// for a second pass after the first chain's end (0x160000). Returns the ring's next record.
// orig: 0x002559c0
uint32_t moby_draw_loop(uint32_t first, uint32_t ring, int32_t count, uint32_t defer);

// orig: (host) one VU1 program start (MSCAL) of a moby's chain as the draw loop builds it, given to the observer by the
// MRP-only pass (g_moby_pass_mrp, moby_pass.h) instead of the chain: the MSCAL's address, the model-to-view matrix the part
// uploads itself (VU1 0x129 / 0x28d, 0 none: the parts after the first two reuse the buffers'), the GS state its DIRECT
// sends (the colour fade's ALPHA / FBA / TEST: the GIF packet, `gs_qwc` quadwords)
struct MobyDrawPart {
    uint16_t mscal = 0;
    uint16_t matrix_dest = 0;
    uint32_t matrix[16] = {};  // (the four rows' words, as the chain holds them)
    uint32_t gs_qwc = 0;       // 0: no DIRECT in the part
    uint32_t gs[8] = {};       // the GIF tag and its A+D quadword
};

// orig: (host) an observer of the draw loop (no effect on it: called once per moby drawn, after
// its chain parts and its skinning record are written): the moby, its chain parts [chain_begin,
// chain_end) in the packet buffer, the record's place in the ring (ring) and in the scratchpad
// (spr_rec, `qwords` quadwords, sent to the ring by the fromSPR just issued), the moby's detail
// (0 near / 1 far mesh) and whether this is the deferred (drop shadow) pass; with the MRP-only
// pass the parts as the loop built them (`parts`, `nparts`; else null: the chain has them). Used
// by the MRP scene state (render/mrp_scene.h).
using MobyDrawObserver = void (*)(uint32_t moby, uint32_t chain_begin, uint32_t chain_end, uint32_t ring, uint32_t spr_rec, uint32_t qwords, uint32_t far_lod,
                                  uint32_t deferred, const MobyDrawPart* parts, uint32_t nparts);
extern MobyDrawObserver g_moby_draw_observer;

}  // namespace rac1

#endif
