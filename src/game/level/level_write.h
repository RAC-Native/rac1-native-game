// The level data's write facade (stage 5, tasks LD1 and LD3): the run-time writers of the shapes (the three lists' 0x80-byte
// records, the 0x90-byte volumes), of the grind rails and of the paths (and the rails' spines) store through these calls. Each one makes the EE store the original makes
// (the same width, order and float bits, inline in its caller) and then writes the same words into the native record
// (level_registry.h) found through the address index; an address no record holds gets the EE store alone (counted).
// Nothing reads the native records but RAC1_LD_VERIFY and oracle batch 541.
#ifndef RAC1_GAME_LEVEL_LEVEL_WRITE_H
#define RAC1_GAME_LEVEL_LEVEL_WRITE_H

#include <game/level/level_registry.h>
#include <game/ovl/ovl_common.h>

#include <bit>
#include <cstdint>

namespace rac1::lvd {

// orig: (host) a shape's (or volume's) float at EE `a` set to `v` (swc1)
inline void set_shape_f32(uint32_t a, float v) {
    swc1(a, v);
    const uint32_t w = std::bit_cast<uint32_t>(v);
    record_written(a, &w, 1);
}
// orig: (host) a shape's word at EE `a` set to `v` (sw)
inline void set_shape_word(uint32_t a, uint32_t v) {
    sw(a, v);
    record_written(a, &v, 1);
}
// orig: (host) a shape's 16-byte row at EE `a` (a centre +0x30, the angles +0x70 ...) set to `v` (a quad store)
inline void set_shape_row(uint32_t a, const Vec4& v) {
    V(a) = v;
    const uint32_t w[4] = {std::bit_cast<uint32_t>(v.x), std::bit_cast<uint32_t>(v.y), std::bit_cast<uint32_t>(v.z), std::bit_cast<uint32_t>(v.w)};
    record_written(a, w, 4);
}
// orig: (host) a shape's 16-byte row at EE `a` copied from EE `src` (lq_sq)
inline void copy_shape_row(uint32_t a, uint32_t src) {
    const uint32_t w[4] = {lwu(src), lwu(src + 4), lwu(src + 8), lwu(src + 12)};
    lq_sq(a, src);
    record_written(a, w, 4);
}
// orig: (host) a shape's 16-byte row at EE `a` just written by a callee through an out-pointer (sub_00260400's out_pos: the
// EE stores are the callee's own): the words it left there taken into the native record
inline void shape_row_written(uint32_t a) {
    const uint32_t w[4] = {lwu(a), lwu(a + 4), lwu(a + 8), lwu(a + 12)};
    record_written(a, w, 4);
}
// orig: (host) a shape's centre (+0x30 .. +0x38) at EE `shape` moved by `d` (each lane read, `d` added, stored: swc1)
inline void move_shape_centre(uint32_t shape, const float d[3]) {
    for (uint32_t i = 0; i < 3; ++i) set_shape_f32(shape + 0x30 + 4 * i, lwc1(shape + 0x30 + 4 * i) + d[i]);
}
// orig: (host) rail `rail`'s reach (+0xc) set to `v` (swc1)
inline void set_rail_reach(uint32_t rail, float v) {
    swc1(rail + 0xc, v);
    const uint32_t w = std::bit_cast<uint32_t>(v);
    record_written(rail + 0xc, &w, 1);
}
// orig: (host) rail `rail`'s reach (+0xc) set to the word `v` (sw: 0 the original's "off")
inline void set_rail_reach_word(uint32_t rail, uint32_t v) {
    sw(rail + 0xc, v);
    record_written(rail + 0xc, &v, 1);
}

// ---- the paths (task LD3): a path's (or a rail spine's) count word (+0), the 12 bytes after it, its 16-byte points from +0x10
// (x, y, z and a w lane: the segment's cached length, a turn, a class's flags or marks) ----
// orig: (host) a path's float at EE `a` (a point's lane) set to `v` (swc1)
inline void set_path_f32(uint32_t a, float v) {
    swc1(a, v);
    const uint32_t w = std::bit_cast<uint32_t>(v);
    record_written(a, &w, 1);
}
// orig: (host) a path's word at EE `a` (a point's lane stored as a word) set to `v` (sw)
inline void set_path_word(uint32_t a, uint32_t v) {
    sw(a, v);
    record_written(a, &v, 1);
}
// orig: (host) path `path`'s count word set to `n` (sw: a mover dropping a looped path's closing point, a class resetting it)
inline void set_path_count(uint32_t path, uint32_t n) {
    sw(path, n);
    record_written(path, &n, 1);
}
// orig: (host) a path's 16-byte row at EE `a` (a point) set to `v` (a quad store)
inline void set_path_row(uint32_t a, const Vec4& v) {
    V(a) = v;
    const uint32_t w[4] = {std::bit_cast<uint32_t>(v.x), std::bit_cast<uint32_t>(v.y), std::bit_cast<uint32_t>(v.z), std::bit_cast<uint32_t>(v.w)};
    record_written(a, w, 4);
}
// orig: (host) a path's 16-byte row at EE `a` copied from EE `src` (lq_sq)
inline void copy_path_row(uint32_t a, uint32_t src) {
    const uint32_t w[4] = {lwu(src), lwu(src + 4), lwu(src + 8), lwu(src + 12)};
    lq_sq(a, src);
    record_written(a, w, 4);
}
// orig: (host) `n` 16-byte rows of a path from EE `a` just written by EE stores the call does not make (a vector operation
// into V(a), a callee through an out-pointer): the words they left there taken into the native record
inline void path_rows_written(uint32_t a, uint32_t n = 1) {
    for (uint32_t r = 0; r < n; ++r) {
        const uint32_t q = a + 0x10 * r;
        const uint32_t w[4] = {lwu(q), lwu(q + 4), lwu(q + 8), lwu(q + 12)};
        record_written(q, w, 4);
    }
}

}  // namespace rac1::lvd

#endif
