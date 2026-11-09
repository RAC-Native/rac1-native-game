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

// orig: (host) the second moby pass's pose memo (render/mrp_pose_memo.h), null when off: `lookup` before the pose of `job`
// (true: its joint matrices are on the scratchpad already, the pose is not run), `store` after the pose ran
struct MobyPoseMemo {
    bool (*lookup)(uint32_t job) = nullptr;
    void (*store)(uint32_t job) = nullptr;
};
extern MobyPoseMemo g_moby_pose_memo;

}  // namespace rac1

#endif
