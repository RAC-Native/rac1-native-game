// The native level registry (stage 5, task L2): the level-data objects of a level - its paths, volumes and the three lists of
// shapes, level cameras, grind rails, the camera-volume grid and (the loaded level's) zone grid - held in host containers,
// parsed from the level's gameplay file with explicit little-endian reads (formats/level_gameplay.h), beside the EE tables the
// engine reads (level_data.h, the facade, describes them). Nothing reads it yet: task L3a makes the facade read it and the EE
// tables its projection. Until then RAC1_LD_VERIFY compares it with the EE tables (docs/env-vars.md).
//
// One registry per place the engine's level data lives:
//   - the base: the loaded level's, built at the end of its load (gameplay_load, level_load.cpp) from the gameplay file the
//     load has just unpacked into EE memory (the same bytes the load lays out);
//   - one per registry of the open engine (enhance/leveldata.cpp's `ld_registry_make`: a context's level, a world section's,
//     the session base's own world under LD_BASE_WORLD), built from the same gameplay file (the pool's) that leveldata.cpp
//     lays out into the session heap, keyed as there.
//
// What comes from the file, what from the laid-out tables (EE):
//   - from the file: every path (its count word, the 12 bytes after it, its points), every volume (0x90 bytes read 0x80 apart,
//     as the load reads them), every shape of the three lists, the cameras' type / position / rotation / pvar index, the rails'
//     centre / reach / loop word and spine, the camera-volume grid's bytes;
//   - from EE, once at the build: a camera's pvar block address (the load moves the block out of the pvar data: the address
//     stays an EE address until stage 7, Q5) and the base's zone grid (it is the level's core data, not the gameplay file's:
//     level_setup_body points 0x15f600 at it; copied with its extent walked as zone_grid_cell walks it);
//   - not held: the moby groups (stage 7, Q5), the rail records' +0x18 / +0x1c (the load leaves them as the memory had them).
//
// Run-time writes into the tables' contents: every writer of a path, a shape, a volume or a rail writes the native record beside
// the EE word (level_write.h, the write facade); RAC1_LD_VERIFY reports any difference between a record and its EE words.
#ifndef RAC1_GAME_LEVEL_LEVEL_REGISTRY_H
#define RAC1_GAME_LEVEL_LEVEL_REGISTRY_H

#include <formats/level_gameplay.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rac1::lvd {

using PathSpline = LdPath;    // a path or a rail's spine: its count word, 12 bytes, then 16-byte points
using Shape = LdShape;        // a 0x80-byte record of the three lists (boxes, spheres, cylinders)
using Volume = LdVolume;      // a 0x90-byte volume record
using Rail = LdRail;          // a grind rail and its spine

// A level camera: its placement (the file's) and its pvar block (an EE address: the laid-out table's +0x1c)
struct Camera {
    LdCamera placement;
    uint32_t pvars = 0;
};

// The camera-volume grid (the gameplay file's +0x84 block, as gameplay_load copies it to *0x15ef80): 64 x 64 cells of 16
// units, each a word (the offset of the cell's list from the grid's start, 0 none); a list: its count word at +0, 0x30-byte
// records from +0x10 (cam_volume_find, camera_follow.cpp)
struct CamVolumeGrid {
    std::vector<uint8_t> bytes;  // little-endian, as the file has them
    bool empty() const { return bytes.empty(); }
    uint32_t word(uint32_t off) const;  // a little-endian word of the grid (0 outside it)
    float f32(uint32_t off) const;
    // the offset of cell (x, y)'s list from the grid's start, 0 none (x, y in 0 .. 64 as the original reads them)
    uint32_t cell_list(int32_t x, int32_t y) const { return word(static_cast<uint32_t>(y * 64 + x) * 4); }
    int32_t list_count(uint32_t list) const { return static_cast<int32_t>(word(list)); }
    // the offset of record `i` of the list at `list` (0x30 bytes: centre +0, radius +0xc, +0x14 a sphere's index ...)
    static uint32_t list_record(uint32_t list, int32_t i) { return list + 0x10 + static_cast<uint32_t>(i) * 0x30; }
};

// The zone grid (the level's core data, *0x15f600): +0 the masks' offset, +4 the first z, +6 the z count, +8 the z rows'
// offsets / 4; a row: its first index (a halfword), its count, then the next level's offsets / 4 (0 none); an x cell 0xffff
// none, else the index of its 0x80-byte mask (overlay_entry.cpp zone_grid_cell)
struct ZoneGrid {
    std::vector<uint8_t> bytes;  // little-endian, from the grid's start to the end of its last row or mask
    bool empty() const { return bytes.empty(); }
    uint32_t half(uint32_t off) const;
    uint32_t word(uint32_t off) const;
    // the offset of cell (x, y, z)'s 0x80-byte mask from the grid's start, 0 none (zone_grid_cell's walk)
    uint32_t cell_mask(int32_t x, int32_t y, int32_t z) const;
};

constexpr int KEY_BASE = -1;  // the loaded level's registry (else leveldata.cpp's key: a level 0..18, LD_BASE_WORLD)

struct LevelRegistry {
    int key = KEY_BASE;
    int level = -1;  // the level whose gameplay file it is
    bool has_paths = false, has_volumes = false, has_cameras = false, has_rails = false;
    std::array<bool, 3> has_shapes{};
    std::vector<PathSpline> paths;
    uint32_t path_data_size = 0;
    std::vector<Volume> volumes;
    std::array<std::vector<Shape>, 3> shapes;  // BOXES, SPHERES, CYLINDERS (level_data.h's BoxList)
    std::vector<Camera> cameras;
    std::vector<Rail> rails;
    uint32_t rail_data_size = 0;
    CamVolumeGrid cam_volumes;
    ZoneGrid zones;  // the base's only
    // Each record's EE address (index for index with its container; 0: none), taken from the tables at the build (the base's
    // from the engine's, an open-engine registry's from its block): a path's is its table entry, a rail spine's its rail's
    // +0x10 word. A record never moves while its registry lives (the load and lay_out place it once; a repointed table names
    // other records).
    std::vector<uint32_t> path_ee, volume_ee, camera_ee, rail_ee, spine_ee;
    std::array<std::vector<uint32_t>, 3> shape_ee;
    // The paths the open engine adds past the level's own (leveldata.cpp's ld_path_reserve, filled by ld_path_set, given back
    // by ld_path_release, ld_base_reset or ld_paths_take; the twin's taken table's paths): its table slot, its EE address, its
    // contents (read from EE when it is added, then written beside the EE words by ld_path_set)
    struct AddedPath {
        int32_t slot = -1;
        uint32_t ee = 0;
        PathSpline path;
    };
    std::vector<AddedPath> added;
    // the host memory it holds (its containers' bytes)
    size_t host_bytes() const;
};

// orig: (host) the loaded level's registry (nullptr before a level's load, or when its gameplay file could not be read)
const LevelRegistry* registry_base();
// orig: (host) the twin of leveldata.cpp's registry `key` (nullptr none)
const LevelRegistry* registry_of(int key);

// orig: (host) gameplay_load's end (level_load.cpp): the base registry built from the gameplay file unpacked at EE `gameplay`
void registry_level_loaded(uint32_t gameplay);
// orig: (host) ld_registry_make (leveldata.cpp): registry `key`'s twin built from `gameplay` (level `level`'s gameplay file),
// the one lay_out has just laid out into the session heap block `block`
void registry_context_made(int key, int level, const std::vector<uint8_t>& gameplay, uint32_t block);

// orig: (host) leveldata.cpp's hooks: registry `key`'s twin dropped (ld_registry_drop); a path of `size` bytes at EE `mem` added
// at table slot `slot` of registry `key` (KEY_BASE: the base's) or given back; the base's added paths forgotten (ld_base_reset);
// registry `key`'s paths made the `n` paths of the table at `table` (ld_paths_take: they become its records, read from EE)
void registry_context_dropped(int key);
void registry_path_added(int key, int32_t slot, uint32_t mem, uint32_t size);
void registry_path_released(int key, int32_t slot);
void registry_base_added_reset();
void registry_paths_taken(int key, uint32_t table, uint32_t n);
// orig: (host) ld_path_set's native side: the record at `mem` (an added path) takes the count `n` and the points `pts` (w 0), as
// the EE words have just been written
void registry_path_set(uint32_t mem, const float (*pts)[3], int n);

// ---- the address index ----
// One sorted table of every live record's EE bytes [ee, ee + size) over the base, the open-engine registries and their added
// paths; rebuilt at the next lookup after a registry is built or dropped or a path added or given back (O(n log n), n the
// records); a lookup is a binary search (O(log n)).
enum class RecordKind : uint8_t { NONE, PATH, SPINE, VOLUME, SHAPE, CAMERA, RAIL, ADDED_PATH };
struct RecordRef {
    RecordKind kind = RecordKind::NONE;
    int key = KEY_BASE;   // its registry (KEY_BASE the base's, else leveldata.cpp's key)
    int list = 0;         // a shape's list (BOXES, SPHERES, CYLINDERS)
    int32_t index = -1;   // its index in its container (an added path's: its table slot)
    uint32_t offset = 0;  // the byte offset of the address in the record
    uint32_t ee = 0;      // the record's EE address
    uint32_t size = 0;    // its size in bytes
    explicit operator bool() const { return kind != RecordKind::NONE; }
};
// orig: (host) the record whose bytes hold EE address `ee` (kind NONE: none)
RecordRef record_at(uint32_t ee);
// orig: (host) a record kind's name ("path", "rail spine", "volume", "box" ...)
const char* record_kind_name(RecordKind k, int list = 0);
// The index's counters (since the start)
struct IndexStats {
    size_t records = 0;          // records indexed now
    size_t overlaps = 0;         // pairs of live records whose bytes overlap (an error)
    uint64_t rebuilds = 0, lookups = 0, misses = 0;
    uint64_t writes = 0, write_misses = 0, write_unheld = 0;  // the write facade's words: written, no record, a field not held
};
IndexStats index_stats();
// orig: (host) the native slot of a record's word at `offset` (a 4-byte field of its container's element, read and written
// with memcpy), nullptr when the registry does not hold it (a rail's +0x18 / +0x1c)
void* record_slot(const RecordRef& r);
// orig: (host) the write facade's native side (level_write.h): the native record's `n` words from EE address `a` take `w` (the
// words the game has just stored there), each found through the index; no record: the EE store alone, counted
void record_written(uint32_t a, const uint32_t* w, uint32_t n);
// orig: (host) every live registry's records and added paths and the index rebuilt from the EE tables and records as they are
// now (the oracle's import: its cases write the level data to EE)
void registry_pull_from_ee();

// ---- RAC1_LD_VERIFY ----
// RAC1_LD_VERIFY=<n> (0 / unset off): every registry compared with its EE tables at its build and then every n game frames.
extern int g_ld_verify;
void registry_verify_frame_slow(uint64_t frame);
// orig: (host) the game's end (orig_main.cpp): the summary lines of the registries still live (RAC1_LD_VERIFY on)
void registry_verify_end();
// orig: (host) once a game frame at its boundary (orig_main.cpp, beside session_frame): the comparison when it is due
inline void registry_verify_frame(uint64_t frame) {
    if (g_ld_verify > 0) registry_verify_frame_slow(frame);
}

// A difference between a registry and its EE tables
struct RegistryDiff {
    std::string object;  // "path", "path table", "volume", "box" ...
    int32_t index = 0;   // the object's index (-1 its count / pointer)
    uint32_t offset = 0; // the byte offset in its record
    uint32_t native = 0, ee = 0;
    uint32_t addr = 0;   // the EE address compared (0: a count or a value not read at one address)
    std::string text() const;
};
// orig: (host) the base registry compared with the engine's EE tables (OV()) word by word: the differences (at most `max` kept,
// all counted); -1 when there is no base registry (the comparison changes nothing). Batch 541's check.
int registry_compare_base(std::vector<RegistryDiff>& out, size_t max = 64);
// orig: (host) a registry built from the unpacked gameplay file `file` of level `level` (no EE read: no camera pvars, no zone
// grid), to compare with another (batch 541: the disc's file against the one the load unpacked)
bool registry_parse(std::span<const uint8_t> file, int level, LevelRegistry& out, std::string* error = nullptr);
// orig: (host) two registries' file-parsed contents compared (not the camera pvars, not the zone grid); the differences
int registry_compare(const LevelRegistry& a, const LevelRegistry& b, std::vector<RegistryDiff>& out, size_t max = 64);

}  // namespace rac1::lvd

#endif
