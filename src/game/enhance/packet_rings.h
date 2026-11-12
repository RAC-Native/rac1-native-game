// The frame's packet buffers (the two rings of *(0x1611cc) bytes at *(0x173e04) / *(0x173e08), memory_layout 0x23d520): the
// original sizes them per level (1.4 to 1.7 MB, table 0x1c20b8) and never bounds what is written into one. A frame's passes
// write into a ring: the world (sky, terrain, instances, foliage), the draw lists, the mobys (their draw loop stops at its limit
// 0x1611c8: the skinning ring's start 0x15f5fc less 0x10000), then everything after it - the mobys' drop shadows (up to 48 mobys,
// 0x3d000 bytes with 48 of the arena's enemies), the particles, the sparkles, the HUD - with nothing checking: a crowd of
// mobys filled the loop's share, and the shadows and the particles after it ran through the skinning ring (the top 0x1e000 bytes
// of the buffer, *(0x15f578)) and out of the buffer into what lies above it, the HUD's tables (a crash with 110 enemies).
//
// Here the shadows and the particles stop before they run out of the buffer (a shadow or a particle less, never a write past
// its end), and so does the moby draw loop's skinning ring (a moby less). A frame the original filled without overflowing is
// unchanged: the checks only act where it wrote past the buffer.
#ifndef RAC1_GAME_ENHANCE_PACKET_RINGS_H
#define RAC1_GAME_ENHANCE_PACKET_RINGS_H

#include <cstdint>

namespace rac1 {

// orig: (host) the room left in the frame's buffer before the skinning ring (*(0x15f5f8)): the bytes the passes can still
// write at the packet cursor (*(0x1611c0)); negative when it is past
int32_t packet_room();
// orig: (host) the end of the frame's buffer (the skinning ring's end): the moby draw loop's records stop there
uint32_t packet_joint_end();

}  // namespace rac1

#endif
