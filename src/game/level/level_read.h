// The level data's typed read API (stage 5, task LD4): the readers of a path's, a shape's, a volume's, a level camera's and a
// grind rail's words, keyed by the record's EE address (the handle level_data.h hands out, kept in pvars and compared) or by
// an EE address inside the record (a point, a field), as the consumers compute it today.
//
// RAC1_LD_READ picks what a read returns:
//   ee      (the default) the EE word(s), read as the original reads them: the same width, the same address arithmetic
//           (lw, lwu, lwc1, lhu, a 16-byte V() row, lq_sq's aligned copy) - one predictable branch, then the original's load;
//   native  the native record (level_registry.h) found through the address index: the word at the same byte offset of the
//           indexed record. An address the index does not hold (a stack copy, a pvar-held point, a section's table copy, a
//           record of another kind, a word past a path's parsed points, a rail's +0x18 / +0x1c) is read from EE and counted,
//           by accessor ("[ld-read] miss" lines at the game's end, the first few with their addresses).
// RAC1_LD_READ_CHECK=1: every native read is compared with the EE word(s); "[ld-read] differs: <accessor> at <address>:
// native ..., EE ..." (the first 50 printed, the others counted). The oracle turns it on (ovl_oracle.cpp) and picks the
// ports' mode with ORACLE_LD_READ (docs/oracle.md).
//
// The handles stay EE addresses: what lvd::path(i), box(), camera(), rail() return does not change; only the reads through
// them do. Writers stay on level_write.h (the EE store and the native record together).
//
// The accessors (each `a` an EE address inside a record of that family; `p` a path's (or rail spine's) handle, `b` a shape's,
// `v` a volume's, `c` a level camera's 0x20-byte entry, `r` a rail's 0x20-byte record):
//   word reads    <family>_u32 (lwu), _s32 (lw), _f32 (lwc1), _u16 (lhu), _s16 (lh): the word or half at `a`
//   row reads     <family>_row(a): the 16 bytes at `a` as a Vec4 (a V() read, by value); <family>_copy_row(dst, a): the 16
//                 bytes at `a & ~15` stored at EE `dst & ~15` (lq_sq with the record as its source)
//   paths         path_n(p) the count word (lw), path_pt(p, k) point k (+0x10 + 16 k), path_lane(p, k, l) / path_lane_bits(p,
//                 k, l) lane l of point k as a float / as raw bits (the w lane: a length, a turn, link bits, a -1 event),
//                 path_head(p, i) the i-th of the 12 bytes after the count (a word); PathView: a path resolved once
//   shapes        shape_row(b, r) row r (+16 r: the matrix's rows 0 .. 3, the centre is row 3, the matrix to unit space rows
//                 4 .. 7), shape_centre(b) (+0x30), shape_mat(b) (+0x00) and shape_to_unit(b) (+0x40) as Mat4
//   volumes       volume_row(v, r), volume_centre(v) (+0x30), volume_mat(v), volume_to_unit(v), volume_radius(v) (+0x80)
//   cameras       camera_pos(c) (+0 .. +8, w the type's word as a float's bits), camera_type(c) (+0xc), camera_rot(c) (+0x10
//                 .. +0x18), camera_data_of(c) (+0x1c: the data block's EE address; the block is pvar state and stays EE)
//   rails         rail_centre(r) (+0 .. +8, w the reach), rail_reach(r) (+0xc), rail_spine(r) (+0x10: the spine's handle, a
//                 path record for the path accessors), rail_loop(r) (+0x14)
#ifndef RAC1_GAME_LEVEL_LEVEL_READ_H
#define RAC1_GAME_LEVEL_LEVEL_READ_H

#include <game/level/level_registry.h>
#include <game/ovl/ovl_common.h>

#include <bit>
#include <cstdint>
#include <cstring>

namespace rac1::lvd {

enum class ReadMode : uint8_t { EE = 0, NATIVE = 1 };
extern ReadMode g_ld_read;     // RAC1_LD_READ (read at the start)
extern bool g_ld_read_check;   // RAC1_LD_READ_CHECK
inline ReadMode read_mode() { return g_ld_read; }
inline bool read_native() { return g_ld_read == ReadMode::NATIVE; }
void set_read_mode(ReadMode m);
inline bool read_check() { return g_ld_read_check; }
void set_read_check(bool on);

// The accessors, by name (the miss and difference counters are kept per accessor)
enum class ReadAcc : uint8_t {
    PATH_U32, PATH_S32, PATH_F32, PATH_U16, PATH_S16, PATH_ROW, PATH_COPY_ROW, PATH_N, PATH_PT, PATH_LANE, PATH_HEAD, PATH_VIEW,
    SHAPE_U32, SHAPE_S32, SHAPE_F32, SHAPE_U16, SHAPE_S16, SHAPE_ROW, SHAPE_COPY_ROW, SHAPE_CENTRE, SHAPE_MAT,
    VOLUME_U32, VOLUME_S32, VOLUME_F32, VOLUME_U16, VOLUME_S16, VOLUME_ROW, VOLUME_COPY_ROW, VOLUME_CENTRE, VOLUME_MAT, VOLUME_RADIUS,
    CAMERA_U32, CAMERA_S32, CAMERA_F32, CAMERA_U16, CAMERA_S16, CAMERA_ROW, CAMERA_COPY_ROW, CAMERA_POS, CAMERA_TYPE, CAMERA_ROT, CAMERA_DATA,
    RAIL_U32, RAIL_S32, RAIL_F32, RAIL_U16, RAIL_S16, RAIL_ROW, RAIL_COPY_ROW, RAIL_CENTRE, RAIL_REACH, RAIL_SPINE, RAIL_LOOP,
    COUNT
};
const char* read_acc_name(ReadAcc acc);

// The native side (level_read.cpp): the word at EE `a & ~3` from the indexed record (a miss: the EE word, counted); `n` words
// from `a` (one lookup while they stay inside the record); a row's 16 bytes stored at EE `dst`
uint32_t read_native_word(uint32_t a, ReadAcc acc);
void read_native_words(uint32_t a, uint32_t* out, uint32_t n, ReadAcc acc);
void read_native_copy_row(uint32_t dst, uint32_t a, ReadAcc acc);

// Counters since the start (or the last reset): native reads (words), misses, differences found by the check
struct ReadStats {
    uint64_t reads = 0, misses = 0, differs = 0;
};
ReadStats read_stats(ReadAcc acc);
ReadStats read_stats_total();
void read_stats_reset();
// orig: (host) the game's end (orig_main.cpp; the oracle's end): the "[ld-read]" summary, the misses by accessor and the
// differences' count, when the mode was native at any time
void read_report();

namespace read_detail {
inline Vec4 row_of(const uint32_t w[4]) {
    Vec4 v;
    std::memcpy(&v, w, 16);
    return v;
}
inline uint32_t half_of(uint32_t w, uint32_t a) { return (w >> ((a & 2u) * 8u)) & 0xffffu; }
inline Mat4 mat_native(uint32_t a, ReadAcc acc) {
    uint32_t w[16];
    read_native_words(a, w, 16, acc);
    Mat4 m;
    std::memcpy(&m, w, 64);
    return m;
}
}  // namespace read_detail

// ---- the word and row reads of each family (inline: ee is the original's load) ----
#define RAC1_LD_READ_FAMILY(fam, FAM)                                                                                          \
    inline uint32_t fam##_u32(uint32_t a) { return read_native() ? read_native_word(a, ReadAcc::FAM##_U32) : lwu(a); }        \
    inline int32_t fam##_s32(uint32_t a) {                                                                                     \
        return read_native() ? static_cast<int32_t>(read_native_word(a, ReadAcc::FAM##_S32)) : lw(a);                         \
    }                                                                                                                          \
    inline float fam##_f32(uint32_t a) {                                                                                       \
        return read_native() ? std::bit_cast<float>(read_native_word(a, ReadAcc::FAM##_F32)) : lwc1(a);                       \
    }                                                                                                                          \
    inline uint16_t fam##_u16(uint32_t a) {                                                                                    \
        return read_native() ? static_cast<uint16_t>(read_detail::half_of(read_native_word(a, ReadAcc::FAM##_U16), a)) : lhu(a); \
    }                                                                                                                          \
    inline int16_t fam##_s16(uint32_t a) {                                                                                     \
        return read_native() ? static_cast<int16_t>(read_detail::half_of(read_native_word(a, ReadAcc::FAM##_S16), a)) : lh(a);  \
    }                                                                                                                          \
    inline Vec4 fam##_row(uint32_t a) {                                                                                        \
        if (!read_native()) return V(a);                                                                                       \
        uint32_t w[4];                                                                                                         \
        read_native_words(a, w, 4, ReadAcc::FAM##_ROW);                                                                        \
        return read_detail::row_of(w);                                                                                         \
    }                                                                                                                          \
    inline void fam##_copy_row(uint32_t dst, uint32_t a) {                                                                     \
        if (!read_native()) lq_sq(dst, a);                                                                                     \
        else read_native_copy_row(dst, a, ReadAcc::FAM##_COPY_ROW);                                                            \
    }
RAC1_LD_READ_FAMILY(path, PATH)
RAC1_LD_READ_FAMILY(shape, SHAPE)
RAC1_LD_READ_FAMILY(volume, VOLUME)
RAC1_LD_READ_FAMILY(camera, CAMERA)
RAC1_LD_READ_FAMILY(rail, RAIL)
#undef RAC1_LD_READ_FAMILY

// ---- paths (and rails' spines, and the open engine's added paths): the count word at +0, 12 bytes, 16-byte points from +0x10
// orig: (host) path `p`'s count word (lw)
inline int32_t path_n(uint32_t p) { return read_native() ? static_cast<int32_t>(read_native_word(p, ReadAcc::PATH_N)) : lw(p); }
// orig: (host) the i-th word (0 .. 2) of the 12 bytes after path `p`'s count (lwu)
inline uint32_t path_head(uint32_t p, int32_t i) {
    const uint32_t a = p + 4 + static_cast<uint32_t>(i) * 4;
    return read_native() ? read_native_word(a, ReadAcc::PATH_HEAD) : lwu(a);
}
// orig: (host) the EE address of path `p`'s point k
inline uint32_t path_pt_addr(uint32_t p, int32_t k) { return p + 0x10 + static_cast<uint32_t>(k) * 0x10; }
// orig: (host) path `p`'s point k (x y z w, a V() read by value)
inline Vec4 path_pt(uint32_t p, int32_t k) {
    const uint32_t a = path_pt_addr(p, k);
    if (!read_native()) return V(a);
    uint32_t w[4];
    read_native_words(a, w, 4, ReadAcc::PATH_PT);
    return read_detail::row_of(w);
}
// orig: (host) lane l (0 x .. 3 w) of path `p`'s point k as raw bits (lwu) and as a float (lwc1)
inline uint32_t path_lane_bits(uint32_t p, int32_t k, int32_t l) {
    const uint32_t a = path_pt_addr(p, k) + static_cast<uint32_t>(l) * 4;
    return read_native() ? read_native_word(a, ReadAcc::PATH_LANE) : lwu(a);
}
inline float path_lane(uint32_t p, int32_t k, int32_t l) {
    const uint32_t a = path_pt_addr(p, k) + static_cast<uint32_t>(l) * 4;
    return read_native() ? std::bit_cast<float>(read_native_word(a, ReadAcc::PATH_LANE)) : lwc1(a);
}

// A path resolved once (for a loop or a hot reader that reads many words of one path): found from any EE address inside it;
// its reads take the words from the record without a lookup each. It stays valid while no registry is built or dropped and
// no path is added, given back, taken or pulled (the index's generation); after that, or when the address names no path
// record, or in ee, each read is the plain accessor's. A stack value: never kept past the function that made it.
class PathView {
public:
    explicit PathView(uint32_t a) : a0_(a) {
        if (read_native()) resolve(a);
    }
    uint32_t u32(uint32_t a) const {
        uint32_t w;
        if (fast(a, 4) && slot(a, w)) return w;
        return read_native() ? read_native_word(a, ReadAcc::PATH_VIEW) : lwu(a);
    }
    int32_t s32(uint32_t a) const { return read_native() ? static_cast<int32_t>(u32(a)) : lw(a); }
    float f32(uint32_t a) const { return read_native() ? std::bit_cast<float>(u32(a)) : lwc1(a); }
    Vec4 row(uint32_t a) const {
        if (!read_native()) return V(a);
        uint32_t w[4];
        if (fast(a, 16) && ((a - ee_) & 0xfu) == 0 && a - ee_ >= 0x10) {
            std::memcpy(w, &rec_->points[(a - ee_ - 0x10) >> 4], 16);
            return read_detail::row_of(w);
        }
        read_native_words(a, w, 4, ReadAcc::PATH_VIEW);
        return read_detail::row_of(w);
    }
    // the count word of the path the view was made from (its handle)
    int32_t n() const { return s32(a0_); }

private:
    void resolve(uint32_t a);
    bool fast(uint32_t a, uint32_t bytes) const {
        return rec_ != nullptr && gen_ == g_ld_gen && !g_ld_read_check && a >= ee_ && a - ee_ + bytes <= size_;
    }
    bool slot(uint32_t a, uint32_t& w) const {
        const uint32_t o = (a - ee_) & ~3u;
        if (o == 0) std::memcpy(&w, &rec_->count, 4);
        else if (o < 0x10) std::memcpy(&w, &rec_->head[(o - 4) / 4], 4);
        else std::memcpy(&w, reinterpret_cast<const uint8_t*>(&rec_->points[(o - 0x10) >> 4]) + (o & 0xcu), 4);
        return true;
    }
    const PathSpline* rec_ = nullptr;
    uint32_t a0_ = 0, ee_ = 0, size_ = 0, gen_ = 0;
};
inline PathView path_view(uint32_t a) { return PathView(a); }

// ---- the 0x80-byte shapes of the three lists (a volume's first 0x80 bytes have the same layout)
// orig: (host) row r (+16 r) of shape `b` (a V() read by value)
inline Vec4 shape_row(uint32_t b, int32_t r) { return shape_row(b + static_cast<uint32_t>(r) * 0x10); }
// orig: (host) shape `b`'s centre (+0x30, its matrix's fourth row)
inline Vec4 shape_centre(uint32_t b) {
    if (!read_native()) return V(b + 0x30);
    uint32_t w[4];
    read_native_words(b + 0x30, w, 4, ReadAcc::SHAPE_CENTRE);
    return read_detail::row_of(w);
}
// orig: (host) shape `b`'s matrix (+0x00) and its matrix to unit space (+0x40), by value (an M() read)
inline Mat4 shape_mat(uint32_t b) { return read_native() ? read_detail::mat_native(b, ReadAcc::SHAPE_MAT) : M(b); }
inline Mat4 shape_to_unit(uint32_t b) { return read_native() ? read_detail::mat_native(b + 0x40, ReadAcc::SHAPE_MAT) : M(b + 0x40); }

// ---- the 0x90-byte volumes
inline Vec4 volume_row(uint32_t v, int32_t r) { return volume_row(v + static_cast<uint32_t>(r) * 0x10); }
inline Vec4 volume_centre(uint32_t v) {
    if (!read_native()) return V(v + 0x30);
    uint32_t w[4];
    read_native_words(v + 0x30, w, 4, ReadAcc::VOLUME_CENTRE);
    return read_detail::row_of(w);
}
inline Mat4 volume_mat(uint32_t v) { return read_native() ? read_detail::mat_native(v, ReadAcc::VOLUME_MAT) : M(v); }
inline Mat4 volume_to_unit(uint32_t v) { return read_native() ? read_detail::mat_native(v + 0x40, ReadAcc::VOLUME_MAT) : M(v + 0x40); }
// orig: (host) volume `v`'s end caps' radius (+0x80, lwc1)
inline float volume_radius(uint32_t v) {
    return read_native() ? std::bit_cast<float>(read_native_word(v + 0x80, ReadAcc::VOLUME_RADIUS)) : lwc1(v + 0x80);
}

// ---- the level cameras' 0x20-byte entries
// orig: (host) camera `c`'s position (+0 .. +8; w: the type word's bits, as a V() read of +0 has it)
inline Vec4 camera_pos(uint32_t c) {
    if (!read_native()) return V(c);
    uint32_t w[4];
    read_native_words(c, w, 4, ReadAcc::CAMERA_POS);
    return read_detail::row_of(w);
}
// orig: (host) camera `c`'s type (+0xc, lw)
inline int32_t camera_type(uint32_t c) {
    return read_native() ? static_cast<int32_t>(read_native_word(c + 0xc, ReadAcc::CAMERA_TYPE)) : lw(c + 0xc);
}
// orig: (host) camera `c`'s rotation (+0x10 .. +0x18; w: the data block's address's bits, as a V() read of +0x10 has it)
inline Vec4 camera_rot(uint32_t c) {
    if (!read_native()) return V(c + 0x10);
    uint32_t w[4];
    read_native_words(c + 0x10, w, 4, ReadAcc::CAMERA_ROT);
    return read_detail::row_of(w);
}
// orig: (host) the EE address of camera `c`'s data block (+0x1c, lwu; the block itself is pvar state: read from EE)
inline uint32_t camera_data_of(uint32_t c) { return read_native() ? read_native_word(c + 0x1c, ReadAcc::CAMERA_DATA) : lwu(c + 0x1c); }

// ---- the grind rails' 0x20-byte records
// orig: (host) rail `r`'s centre (+0 .. +8; w its reach, as a V() read of +0 has it)
inline Vec4 rail_centre(uint32_t r) {
    if (!read_native()) return V(r);
    uint32_t w[4];
    read_native_words(r, w, 4, ReadAcc::RAIL_CENTRE);
    return read_detail::row_of(w);
}
// orig: (host) rail `r`'s reach (+0xc, lwc1: find_rail_v skips the rail beyond it; 0 off)
inline float rail_reach(uint32_t r) { return read_native() ? std::bit_cast<float>(read_native_word(r + 0xc, ReadAcc::RAIL_REACH)) : lwc1(r + 0xc); }
// orig: (host) the EE address of rail `r`'s spine (+0x10, lwu: a path record for the path accessors)
inline uint32_t rail_spine(uint32_t r) { return read_native() ? read_native_word(r + 0x10, ReadAcc::RAIL_SPINE) : lwu(r + 0x10); }
// orig: (host) rail `r`'s loop word (+0x14, lw)
inline int32_t rail_loop(uint32_t r) { return read_native() ? static_cast<int32_t>(read_native_word(r + 0x14, ReadAcc::RAIL_LOOP)) : lw(r + 0x14); }

}  // namespace rac1::lvd

#endif
