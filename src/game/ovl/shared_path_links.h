// The links between a path's points that see each other past the walls of a list of paths, shared by the levels that build
// them: one port of the levels' copies (shared_path_links.cpp).
#ifndef RAC1_GAME_OVL_SHARED_PATH_LINKS_H
#define RAC1_GAME_OVL_SHARED_PATH_LINKS_H

#include <cstdint>

namespace rac1 {

// For the path `path` (an index of the level's path table): each point's word +0xc becomes the mask of the other points
// (bit n: point n) the segment to which crosses none of the `count` wall paths listed at `walls` (path indices).
void path_links_build(uint32_t walls, int32_t count, uint32_t path);  // L5:0x0028be78 L6:0x00270b68 L7:0x0028a440 L10:0x00254ae8 L11:0x002873b0

}  // namespace rac1

#endif
