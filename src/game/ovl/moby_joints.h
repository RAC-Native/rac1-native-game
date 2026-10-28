// The moby joints (ADR 0007): per skinning record the moby pass leaves in the ring, the pose of
// the moby's joints (keyframes blended, blend channels and additive layers, the hierarchy, the
// bind) as matrices on the scratchpad, then written into the moby's VIF1 chain.
#ifndef RAC1_GAME_OVL_MOBY_JOINTS_H
#define RAC1_GAME_OVL_MOBY_JOINTS_H

#include <cstdint>

namespace rac1 {

// The pose of one animation job `job` (+4 the second keyframe or 0, +8 the blend channels, +0xc
// the additive layers, +0x10 the joint count, +0x12 flags (0x400: keys lerped, not normalised),
// +0x18 the joints' rest data, +0x1c the bind matrices, +0x20.. the keyframes' transfers) into
// the joint matrices at the scratchpad's 0 (0x40 each), `work` a RAM buffer for its lists.
// orig: 0x00250fe0
void moby_joints_pose(uint32_t job, uint32_t work);

// orig: (host) the pose's hot loops in plain floats (moby_joints.cpp), not the VU0's ops: 0 the VU0's, 1 fast (fused multiply-adds,
// an estimated 1 / sqrt), 2 exact (each op rounded as the VU0's); RAC1_WIIU_FAST_POSE (1 on the Wii U, 0 elsewhere); the Wii U's
// debug menu cycles it
extern int g_pose_fast;

}  // namespace rac1

#endif
