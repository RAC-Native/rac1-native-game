// The swingshot's targets (classes 758 and 803) and the follow camera's look toward them, shared by every level that has
// them: one port of the levels' copies (shared_swingshot_target.cpp). Each definition's `orig:` line lists every level's
// address.
#ifndef RAC1_GAME_OVL_SHARED_SWINGSHOT_TARGET_H
#define RAC1_GAME_OVL_SHARED_SWINGSHOT_TARGET_H

#include <cstdint>

namespace rac1 {

// The target's update (class 758; class 803 also turns toward the hero and sparkles).
void swingshot_target_update(uint32_t m);  // L2:0x002e0450 L3:0x002d0bd8 L4:0x002d9a58 L5:0x003098d8 L6:0x002e6270 L7:0x00302238 L9:0x002f8c70 L10:0x002c9590 L11:0x00300010 L12:0x002f7b28 L13:0x002fa708 L14:0x002f2cb8 L15:0x002db100 L16:0x002d6378 L17:0x002d95a8 L18:0x002e19d8
// A target in the camera's view near the hero raises the follow camera's look (the block 0x167000).
void swingshot_target_look(uint32_t m);  // L2:0x002fa7c8 L3:0x002eb4c0 L4:0x002f0500 L5:0x00329038 L6:0x00314868 L7:0x00328148 L9:0x003142d8 L10:0x002f4bd0 L11:0x00327b48 L12:0x003149a8 L13:0x00316c00 L14:0x00313298 L15:0x002f6a38 L16:0x002f3b40 L17:0x002ffff0 L18:0x003053d0
// The follow camera's look-height block 0x167000 reset and the level's hook (cam0_level_hook's copy) installed.
void cam0_level_hook_install();  // L2:0x002fa6d8 L3:0x002eb3d0 L4:0x002f0410 L5:0x00328f48 L6:0x00314778 L7:0x00328058 L9:0x003141e8 L10:0x002f4ae0 L11:0x00327a58 L12:0x003148b8 L13:0x00316b10 L14:0x003131a8 L15:0x002f6948 L16:0x002f3a50 L17:0x002fff00 L18:0x003052e0

}  // namespace rac1

#endif
