#ifndef RAC1_GAME_LEVEL_LEVEL_GRIDS_H
#define RAC1_GAME_LEVEL_LEVEL_GRIDS_H
// The level's two lookup grids as host copies (stage 5 of the native 1.0 track, task LD2), in the collision scene's style
// (src/game/collision/collision_scene.h):
// - the zone grid *(0x15f600) (overlay_entry.cpp zone_grid_cell 0x1f7278, zone_grid_neighbour, zones_from_camera 0x1f7408):
//   +0 the masks' offset, +4 the first z (u16), +6 the z count, +8 the z rows' offsets / 4 (u16, 0 none); a y or x row: its
//   first index (u16), its count (u16), then u16 entries (a y row's: the x rows' offsets / 4, 0 none; an x row's: the index of
//   its cell's 0x80-byte mask, 0xffff none); the masks from grid + *grid;
// - the camera-volume grid *(0x15ef80) (camera_follow.cpp cam_volume_find 0x1ee1b0, cam0_volume_pass 0x2e9aa0): 64 x 64 cell
//   words (the offset of the cell's list from the grid, 0 none; cam_volume_find reads x and y up to 64, so 4161 words), a
//   list: its count (s32), then 0x30-byte records from +0x10 (+0 the centre, +0xc the radius, +0x10 the type, +0x14 the index
//   of the type's shape).
// Each copy holds exactly the integers the lookups read, host-endian (the floats as their bits), so a lookup computes the same
// values from it as from EE memory. Its outputs stay EE: zone_grid_cell returns the mask's EE address and
// zones_from_camera writes the visible masks 0x173d00 .. as before (from the copy's mask bytes); cam_volume_find returns
// the record's EE address (the grid's address + the record's offset: the records stay EE for their callers).
//
// RAC1_LD_GRIDS=native (the default): the lookups read the copies; `ee`, or no copy for the pointer: EE memory, as before.
//
// Each grid is keyed on its EE pointer word's value: a copy is parsed when a lookup first meets a pointer, kept (a few per
// grid: the open engine's sections switch the pointers back and forth) and immutable once built. When this thread's pointer
// changes, or the generation does (overlay_entry_start bumps it and drops the kept copies at each level's start: a level may
// load its grids where the last level's were), a kept copy for that pointer is used again only when a hash of every EE byte
// it was parsed from still matches (a world section streamed into a freed block gets its own parse). The title, the boot
// skip, the level's setup, a section's world and a snapshot's load write the pointers with plain stores: followed without
// hooks. An edit of the bytes in place with neither the pointer nor the generation changing is not seen there (the game
// makes none: the grids are written by the loads only); the check mode (RAC1_LD_GRIDS_CHECK=1, always on in the oracle)
// compares every value a lookup takes from a copy with EE memory, prints the first differences, parses again and repeats
// the lookup; a difference right after a fresh parse is the parse's own error (counted, the copy's value kept).
//
// A pointer the copies cannot hold (below 4 KB, rows outside RAM, counts beyond any level's) gives no copy: the lookups read
// EE memory, the same values. A camera-volume cell past the table (the 65 words cam_volume_find reads for y 64 are the lists'
// bytes) or whose list cannot be held is read from EE memory for that cell alone.
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace rac1 {

class ZoneGridCopy {
public:
    // orig: (host) the parse of the zone grid at `g` in EE memory; null when it cannot be held
    static std::shared_ptr<const ZoneGridCopy> parse(uint32_t g);
    // orig: (host) 0x1f7278's walk on the copy: the cell's mask's EE address, 0 none. `Check`: each value taken compared with
    // EE memory, `*bad` set on a difference
    template <bool Check>
    uint32_t cell(int32_t x, int32_t y, int32_t z, bool* bad) const;
    // orig: (host) the 0x80 bytes mem_copy128 / mem_or128 read for the mask at EE address `a` ((a & ~15) on), or null when `a`
    // is not one of this grid's masks
    const uint8_t* mask(uint32_t a) const {
        if (top_ < 0 || a - masks_ >= static_cast<uint32_t>(top_ + 1) * 0x80u || ((a - masks_) & 0x7fu) != 0) return nullptr;
        return mbytes_.data() + (a - masks_);
    }
    uint32_t grid() const { return g_; }
    uint32_t masks() const { return masks_; }
    int32_t mask_count() const { return top_ + 1; }
    uint64_t hash_ee() const;
    uint64_t source_hash() const { return hash_; }
    size_t source_bytes() const;
    std::pair<uint32_t, uint32_t> source_extent() const;
    size_t row_count() const { return rows_.size(); }
    size_t cell_count() const { return cells_; }
    bool same_as(const ZoneGridCopy& o, std::string* why = nullptr) const;

private:
    struct Row {
        int32_t first;  // the row's first index (u16)
        int32_t count;  // its entry count (u16)
        uint32_t at;    // its first entry in ent_ / raw_
        uint32_t ee;    // its EE address (first, count, then the entries)
    };
    uint32_t g_ = 0;
    uint32_t moff_ = 0;    // *grid
    uint32_t masks_ = 0;   // grid + *grid
    Row z_{};              // (ee: grid + 4)
    std::vector<Row> rows_;
    std::vector<int32_t> ent_;   // z / y entries: a row's index, -1 none; x entries: the mask index, -1 none (0xffff)
    std::vector<uint16_t> raw_;  // the entries as EE holds them
    int32_t top_ = -1;           // the highest mask index
    std::vector<uint8_t> mbytes_;  // the masks as mem_copy128 reads them: from (masks & ~15), (top + 1) * 0x80 bytes
    size_t cells_ = 0;
    std::vector<std::pair<uint32_t, uint32_t>> src_;
    uint64_t hash_ = 0;
    friend struct ZoneGridParser;
};

class CamGridCopy {
public:
    static constexpr int32_t kCells = 64 * 64 + 65;  // the words cam_volume_find can read (x, y in 0 .. 64)
    struct Record {
        uint32_t w[4];  // +0 .. +0xc: the centre (x, y, z) and the radius, as bits
        int32_t type;   // +0x10
        int32_t index;  // +0x14
    };
    struct List {
        int32_t count;  // lw(+0): 0 or less none
        uint32_t at;    // its first record in recs_
        uint32_t ee;    // its EE address (grid + the cell's word)
    };
    // orig: (host) the parse of the camera-volume grid at `tab`; null when it cannot be held
    static std::shared_ptr<const CamGridCopy> parse(uint32_t tab);
    uint32_t grid() const { return tab_; }
    // the cell's word (0: none)
    uint32_t word(int32_t c) const { return words_[static_cast<uint32_t>(c)]; }
    // the cell's list: its index in lists_, -1 none (word 0), -2 not held (read EE for this cell)
    int32_t list_of(int32_t c) const { return cell_[static_cast<uint32_t>(c)]; }
    const List& list(int32_t i) const { return lists_[static_cast<uint32_t>(i)]; }
    const Record& record(const List& l, int32_t i) const { return recs_[l.at + static_cast<uint32_t>(i)]; }
    // every one of the 64 x 64 cells held (cam0_volume_pass takes pass_spheres() then)
    bool all_held() const { return all_held_; }
    // the shape indices of the type-5 records, cells 0 .. 4095 in order, each list's records in order (cam0_volume_pass's)
    const std::vector<int32_t>& pass_spheres() const { return pass_; }
    uint64_t hash_ee() const;
    uint64_t source_hash() const { return hash_; }
    size_t source_bytes() const;
    std::pair<uint32_t, uint32_t> source_extent() const;
    size_t list_count() const { return lists_.size(); }
    size_t record_count() const { return recs_.size(); }
    int32_t unheld_cells() const { return unheld_; }
    bool same_as(const CamGridCopy& o, std::string* why = nullptr) const;

private:
    uint32_t tab_ = 0;
    std::vector<uint32_t> words_;
    std::vector<int32_t> cell_;
    std::vector<List> lists_;
    std::vector<Record> recs_;
    std::vector<int32_t> pass_;
    bool all_held_ = false;
    int32_t unheld_ = 0;
    std::vector<std::pair<uint32_t, uint32_t>> src_;
    uint64_t hash_ = 0;
};

// ---- the switch, the check mode and the current copies ----
// orig: (host) RAC1_LD_GRIDS=native (true; the default) or ee (false), read once
bool ld_grids_native();
// orig: (host, testing) the switch set (batch 544 runs the same lookup both ways)
void ld_grids_set_native(bool on);
// orig: (host) the check mode: RAC1_LD_GRIDS_CHECK=1, or set by the oracle's main
bool ld_grids_check();
void ld_grids_set_check(bool on);
// orig: (host) the copy for the zone grid pointer `g` (*(0x15f600)); null: none (read EE)
std::shared_ptr<const ZoneGridCopy> ld_grids_zone(uint32_t g);
// orig: (host) the copy for the camera-volume grid pointer `tab` (*(0x15ef80)); null: none (read EE)
std::shared_ptr<const CamGridCopy> ld_grids_cam(uint32_t tab);
// orig: (host) the check mode's answer to a difference: the copy for that pointer dropped and parsed again (counted stale); a
// difference right after a fresh parse (`after_fresh`) is the parse's own error (counted, printed once; the copy kept)
std::shared_ptr<const ZoneGridCopy> ld_grids_zone_stale(uint32_t g, bool after_fresh);
std::shared_ptr<const CamGridCopy> ld_grids_cam_stale(uint32_t tab, bool after_fresh);
// orig: (host) the check mode's report of one value a lookup took from a copy that differs from EE memory (the first 50
// printed as `[ld-grids] differs: ...`, then counted)
void ld_grids_differs(const char* what, uint32_t grid, uint32_t ee, uint32_t copy_value, uint32_t ee_value);
// orig: (host) overlay_entry_start's, at each level's start: the kept copies dropped, the generation bumped
void ld_grids_level_started();

// orig: (host) zones_from_camera's copies of a mask: mem_copy128(dst, src, 0x80) / mem_or128(dst, a, b, 0x80), the mask
// `src` / `b` taken from the copy of the zone grid `g` (*(0x15f600)) when it is one of its masks (else EE memory, as before;
// the destination and `a` are EE memory either way)
void ld_grids_mask_copy(uint32_t g, uint32_t dst, uint32_t src);
void ld_grids_mask_or(uint32_t g, uint32_t dst, uint32_t a, uint32_t b);

struct LdGridsStats {
    uint64_t parses = 0;        // copies parsed (both grids)
    uint64_t reused = 0;        // kept copies taken again after a pointer or generation change (their hash matched)
    uint64_t stale = 0;         // check mode: a copy found different from EE memory and parsed again
    uint64_t parse_errors = 0;  // check mode: a fresh parse still different from EE memory (a bug)
    uint64_t unheld = 0;        // pointers no copy could hold (EE read)
    uint64_t differs = 0;       // check mode: values that differed from EE memory
    double parse_ms = 0.0;      // the parses' time
};
LdGridsStats ld_grids_stats();

}  // namespace rac1

#endif  // RAC1_GAME_LEVEL_LEVEL_GRIDS_H
