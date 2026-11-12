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

// orig: (host) an observer of the draw loop (no effect on it: called once per moby drawn, after
// its chain parts and its skinning record are written): the moby, its chain parts [chain_begin,
// chain_end) in the packet buffer, the record's place in the ring (ring) and in the scratchpad
// (spr_rec, `qwords` quadwords, sent to the ring by the fromSPR just issued), the moby's detail
// (0 near / 1 far mesh) and whether this is the deferred (drop shadow) pass. Used by the MRP
// scene state (render/mrp_scene.h).
using MobyDrawObserver = void (*)(uint32_t moby, uint32_t chain_begin, uint32_t chain_end, uint32_t ring, uint32_t spr_rec, uint32_t qwords, uint32_t far_lod,
                                  uint32_t deferred);
extern MobyDrawObserver g_moby_draw_observer;
// orig: (host) the VU1 program's start the loop chose for the moby it is drawing (its MSCAL: 0xa the clipping program - the
// moby crosses the guard band -, 0xe the other), set where the moby's draw begins (0x255e48)
extern uint32_t g_moby_draw_mscal;
// orig: (host) what the loop wrote of the moby it is drawing in its chain: the parts of its mesh and, with a shadow mesh
// (+0x73), that mesh's after them - a part per packet of the class, each ended by the VU1 program's start. Of a group: how
// many parts; the tag of its first matrix upload (its first two parts upload the group's matrix, to VU1 0x129 / 0x28d by
// turns: `dest` the first's); the program's start (`mscal`); the tag of the GS state packet sent before its first start
// (0: none - the mesh's own of a faded / alpha-tested moby; the shadow mesh's is the one that put the mesh's back).
// Cleared where the moby's draw begins, read by the observer (the MRP scene's parts: render/mrp_scene.h)
struct MobyDrawParts {
    struct Group {
        uint32_t count, matrix_tag, gs_tag, dest, mscal;
    };
    Group group[2];
    uint32_t groups;
    uint32_t put_back;  // (the tag of the GS state packet after the mesh's last part)
};
extern MobyDrawParts g_moby_draw_parts;
// orig: (host) asked where a moby's draw begins (0x255e48: the moby seen, its program chosen - `mscal` -; `deferred`: in the
// deferred pass, its place in the deferred list taken): true leaves the moby out - nothing of it is written, the loop goes
// on as after a moby drawn. The MRP's shadow run for the extras alone (render/mrp_lod.h) leaves out what the real pass drew
// the same. Null: no moby left out
using MobyDrawSkip = bool (*)(uint32_t moby, uint32_t mscal, uint32_t deferred);
extern MobyDrawSkip g_moby_draw_skip;

}  // namespace rac1

#endif
