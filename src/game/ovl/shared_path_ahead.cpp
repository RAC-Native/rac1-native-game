// A point a distance ahead along a path: one port of the copies Gaspar and Gemlik Base carry (the same code relinked; it
// touches nothing but its arguments). Header shared_path_ahead.h.
#include <game/ovl/shared_path_ahead.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl_common.h>

namespace rac1 {

namespace {
// the index after `i` in the direction `dir`: from the last point back to the one before it (dir <= 0) or round to the
// first; from the first round to the last (dir <= 0) or on to the second
int32_t next_index(uint32_t path, int32_t i, int32_t dir) {
    const int32_t count = lw(path);
    if (i == count - 1) return dir > 0 ? 0 : count - 2;
    if (i != 0) return i + dir;
    return dir < 1 ? count - 1 : 1;
}

uint32_t point(uint32_t path, int32_t i) { return path + 0x10 + (static_cast<uint32_t>(i) << 4); }
}  // namespace

// orig: L9:0x00294ae0 L13:0x00281e40
int32_t path_point_ahead(uint32_t m, uint32_t path, uint32_t out, int32_t idx, int32_t dir, float dist) {
    const uint32_t sp = ee_stack_alloc(0xd0);
    const uint32_t along = sp, rel = sp + 0x10;
    int32_t next = next_index(path, idx, dir);
    vec3_sub(V(along), V(point(path, next)), V(point(path, idx)));
    const float length = vec3_length(V(along));
    vec3_set_length(V(along), V(along), 1.0f);
    vec3_sub(V(rel), V(m + 0x10), V(point(path, idx)));
    float ahead = fpu_add(vec3_dot(V(rel), V(along)), dist);
    if (length < ahead) {  // (past the next point: on along the next segment)
        idx = next;
        next = next_index(path, idx, dir);
        ahead = fpu_sub(ahead, length);
        vec3_sub(V(along), V(point(path, next)), V(point(path, idx)));
        vec3_set_length(V(along), V(along), 1.0f);
    }
    vec3_set_length(V(along), V(along), ahead);
    vec3_add(V(out), V(along), V(point(path, idx)));
    ee_stack_free(0xd0);
    return idx;
}

}  // namespace rac1
