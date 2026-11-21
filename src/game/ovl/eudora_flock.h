// Eudora's flocks (eudora_flock.cpp): class 86 spawns a flock of class 85 (its update an empty one) about its place and
// steers them each frame; the flock circles its place or follows a path. Header comments carry Eudora's addresses; the
// `orig:` lines are on the definitions.
#ifndef RAC1_GAME_OVL_EUDORA_FLOCK_H
#define RAC1_GAME_OVL_EUDORA_FLOCK_H

#include <cstdint>

namespace rac1 {

// The index of the point of the `n` (16-byte) points at `pts` nearest the moby in the plane (-1: none within 32767).
int32_t flock_nearest_point_l4(uint32_t m, uint32_t pts, int32_t n);  // L4:0x001f3038
// 1 + the first edge of the polygon `pts` (`n` points) that the point `p` is outside of (in the plane); 0 inside.
int32_t polygon_outside_edge_l4(uint32_t p, uint32_t pts, int32_t n);  // L4:0x0024c4f0
// The point of the segment a .. b nearest `p` in the plane into `out`; the position along it (not clamped).
float segment_closest_l4(uint32_t out, uint32_t a, uint32_t b, uint32_t p);  // L4:0x0024c660
// The point of the polygon's edges nearest `p` into `out`; 1 when `p` is outside the polygon.
int32_t polygon_closest_l4(uint32_t out, uint32_t p, uint32_t pts, int32_t n);  // L4:0x0024c7d8
// A flock member's steering (into its pvars +0): kept inside its path's area, toward its leader and its home, away from
// its fellows (`members`, `count`) and the hero, off the floor or the walls, with a wander; the flock's parameters at
// `params`.
void flock_steer_l4(uint32_t members, int32_t count, uint32_t m, uint32_t pv, uint32_t params);  // L4:0x001f30e0
// A flock member moved by its steering at the flock's speed, pushed out of what it meets, facing its way.
void flock_member_move_l4(uint32_t m, uint32_t pv, uint32_t params, float lift);  // L4:0x001f3a80
// The moby moved along the path `path` from the point `idx` (in the direction `dir`) to where its own place projects,
// `ahead` further; the point it is past.
int32_t path_advance_l4(uint32_t m, uint32_t path, int32_t idx, int32_t dir, float ahead);  // L4:0x0029eb20

}  // namespace rac1

#endif
