// The world's static lighting: see world_lighting.cpp.
#ifndef RAC1_GAME_OVL_WORLD_LIGHTING_H
#define RAC1_GAME_OVL_WORLD_LIGHTING_H

#include <cstdint>

namespace rac1 {

// orig: 0x00295350
void world_tfrag_lighting(uint32_t list);
// orig: 0x00297728
void world_tie_lighting(uint32_t list);
// orig: 0x00288908
void world_shrub_lighting(uint32_t list);

// orig: (host) the Wii U port: a skipped pass's relighting made from the entries' flags (overlay_entry.cpp; RAC1_WIIU_RELIGHT, default
// on; the GamePad menu's Picture page). No effect when the passes run (the PC's default).
extern bool g_world_relight;

}  // namespace rac1

#endif
