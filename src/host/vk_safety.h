// The Vulkan renderer's safety net (docs/vulkan.md#when-vulkan-cannot-start). Host code: two markers in the settings'
// folder (platform::config_dir()) that send a start to OpenGL when the last Vulkan run went wrong past vk::init's own
// checks (which fall back by themselves, every start):
//   vk_start_pending  written before vk::init, removed once 600 frames are shown or the game closes normally: found at a
//                     start, the last Vulkan start crashed (the driver in vk::init, the pipelines, the first frames)
//   vk_failed         written when the device is lost (VK_ERROR_DEVICE_LOST, GitHub issue #113): found at a start, the
//                     last Vulkan run lost its GPU
// Both stay until the player chooses the Renderer row again (Options > Display), so a start that crashes is not tried
// at every start. Only for Vulkan chosen by the row or by auto (the row never chosen, or RAC1_RENDERER=auto;
// RAC1_RENDERER=vulkan, a test's choice, neither reads nor writes them); not on macOS (Metal, no OpenGL there); never in a headless run unless RAC1_VK_MARKERS=1 (testing).
// Testing: RAC1_VK_FAIL=crash, the game ended at the 120th Vulkan frame as by a crash (RAC1_VK_FAIL's other words: vk_context.h).
#ifndef RAC1_HOST_VK_SAFETY_H
#define RAC1_HOST_VK_SAFETY_H

#include <string>

namespace rac1::vk_safety {

enum class Marker { kNone, kStartFailed, kDeviceLost };

// orig: (none) — host: the markers on (a window, or RAC1_VK_MARKERS=1 headless); false: every call below does nothing
void start(bool headless);

// orig: (none) — host: a marker left by the last Vulkan run (the start goes to OpenGL), and its English reason
Marker found(std::string* why);

// orig: (none) — host: the start marker written, just before vk::init
void starting();

// orig: (none) — host: a frame shown by Vulkan (the 600th removes the start marker)
void frame_shown();

// orig: (none) — host: the start marker removed (vk::init returned, failed: its own checks fall back at every start; the
// game closing normally)
void start_over();

// orig: (none) — host: vk::set_device_lost_hook's: the device-lost marker written
void device_lost(const char* where);

// orig: (none) — host: the Renderer row chosen again (graphics.cpp): both markers removed, the next start tries Vulkan
void cleared();

}  // namespace rac1::vk_safety

#endif
