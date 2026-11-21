// A point a distance ahead of a moby along a path of points (a count at +0, the points 0x10 apart from +0x10), the
// helper Gaspar's class 263 and Gemlik Base's copy of it walk with (shared_path_ahead.cpp). Phase 1 of stage 4: the levels'
// generated copies still run; this is proved against them by oracle batch 504.
#ifndef RAC1_GAME_OVL_SHARED_PATH_AHEAD_H
#define RAC1_GAME_OVL_SHARED_PATH_AHEAD_H

#include <cstdint>

namespace rac1 {

// From point `idx` of `path` towards its next point in the direction `dir` (+1 / -1; the ends turn back): the point
// `dist` ahead of the moby `m`'s projection on that segment, written to `out`, stepping on to the next segment once (when
// it lies past the next point). Returns the index of the segment's start.
int32_t path_point_ahead(uint32_t m, uint32_t path, uint32_t out, int32_t idx, int32_t dir, float dist);  // L9:0x00294ae0 L13:0x00281e40

}  // namespace rac1

#endif
