// The pose memo of the widened view's / LOD slider's second moby pass (mrp_lod.h mrp_lod_moby_shadow).
// Host code: an enhancement's cost cut, none of it is the game's.
//
// In the second pass (the shadow run) every moby it draws is posed (moby_joints_pose 0x250fe0): at LOD High that is 250-350
// mobys a frame, most of them far, not animating, posed the same as the frame before. The pose is a function of the bytes it
// reads: the job (the skinning record's first 0x40 bytes), what its toSPR chain brings (the joints' rest data, the keyframes),
// the blend channels (+8: their headers and keys), the additive layers (+0xc) and the bind matrices (+0x1c). The memo keeps,
// per moby and model, those bytes and the joint matrices the pose left (0x40 a joint at the scratchpad's 0, before
// advanced_pose_hook); when the bytes are the same again the matrices are put on the scratchpad and the pose is not run.
// Only in the shadow run (its memory, the scratchpad and the VU0 are put back after it) and only for a record whose skinning the
// MRP skips (g_moby_mrp_skip: the skinning would read the rest of the pose's scratchpad and VU0 state; the MRP reads only the
// matrices). RAC1_PERF_POSE_MEMO: 0 off, 1 on (default), 2 verify (the pose run on every hit too, its matrices compared).
#ifndef RAC1_GAME_RENDER_MRP_POSE_MEMO_H
#define RAC1_GAME_RENDER_MRP_POSE_MEMO_H

#include <cstdint>

namespace rac1 {

// orig: (host) installs the memo into the pose (moby_joints.h g_moby_pose_memo)
void mrp_pose_memo_install();
// orig: (host) the totals (mrp_lod_report)
void mrp_pose_memo_report();

}  // namespace rac1

#endif
