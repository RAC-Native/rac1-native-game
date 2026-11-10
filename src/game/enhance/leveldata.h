// The open engine's level data (docs/open-engine.md, stage 4; DESIGN 6.2): the engine's level-data objects - the paths
// (the table 0x1b04b0, its count 0x160104 and data 0x160108), the moby groups (0x1ab840, their count 0x15fff4), the cameras
// (0x15ef50 / 0x15ef54), the volumes (the 0x90-byte records 0x1600e4 / 0x1600e8 and the three lists of 0x80-byte records
// 0x1600ec..0x160100), the grind rails (0x15f70c / 0x15f710) - held once per context: a **registry**. Level A's classes in a
// session read A's registry (its sites through views of its context, context.h; the engine's functions they call during
// their update through the objects swapped in for it), filled from A's own level data (the pool's leveldata/, by index as
// on its level) and from what the mods and the class profiles add (paths, groups) from the table's end. No index competition:
// each context's indices are its own. The session base's own classes read the base's objects (the session's view), which
// the profiles add to in the same way. Nothing here runs unless a session imports a class or a profile is used.
#ifndef RAC1_GAME_ENHANCE_LEVELDATA_H
#define RAC1_GAME_ENHANCE_LEVELDATA_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rac1 {

// The engine's level-data objects (Veldin's addresses; OV() on the base)
constexpr uint32_t LD_PATH_TABLE = 0x001b04b0u;  // 176 words: a path's points (its count, 12 bytes, then 16-byte points)
constexpr uint32_t LD_PATH_MAX = 176;
constexpr uint32_t LD_VOLUMES = 0x001600e4u;     // 10 words: the 0x90 records' pointer and count, the three 0x80 lists'
                                                 // (pointer, count), the path count, the paths' data
constexpr uint32_t LD_VOLUMES_SIZE = 0x28;
constexpr uint32_t LD_CAMERAS = 0x0015ef50u;     // the cameras (0x20 each): pointer, count
constexpr uint32_t LD_RAILS = 0x0015f70cu;       // the grind rails (0x20 each): pointer, count
constexpr uint32_t LD_GROUP_COUNT = 0x0015fff4u; // the groups' count (one past the last)
constexpr uint32_t LD_GROUPS = 0x001ab840u;      // 112 words: a group's list of moby indices (the last flagged 0x8000)
constexpr uint32_t LD_GROUP_MAX = 112;

// orig: (host) level `level`'s registry (a context's: made with its first import, from its level's leveldata): true when
// it exists or was made now. The base level's (the session's view) always exists: its objects are the engine's own
bool ld_registry_make(int level);
// orig: (host) level `level`'s registry dropped (its context gone: ctx_drop / a level setup); all of them
void ld_registry_drop(int level);
void ld_registry_drop_all();
// orig: (host) level `level` has a registry of its own (a context's, not the base's)
bool ld_registry_has(int level);

// orig: (host) the session's view (the base's own objects) begins again: what was added from the table's end (the base's
// paths in the mods' block, ABI 5) forgotten. A level mod's pool take, a level setup
void ld_base_reset();
// orig: (host) a path of `n` points in level `level`'s registry (the base's: `level` the loaded one), from the table's end
// down to the level's own count: its index, -1 when there is no room; `*mem` its memory (zeroed, its count 0)
int32_t ld_path_reserve(int level, int n, uint32_t* mem);
// orig: (host) path `index` of level `level`'s registry given back (a context's: its memory freed, its entry 0)
void ld_path_release(int level, int32_t index);
// orig: (host) the path at `mem` set to `n` points (its count, then the points x y z 0)
void ld_path_set(uint32_t mem, const float (*pts)[3], int n);
// orig: (host) path `index`'s memory in level `level`'s registry (0: none)
uint32_t ld_path_mem(int level, int32_t index);
// orig: (host) a group of the mobys `mobys` (moby addresses) in level `level`'s registry: its index, -1 when no room
int32_t ld_group_create(int level, const uint32_t* mobys, int n);
// orig: (host) level `level`'s registry's path count (its level's own paths)
int32_t ld_path_count(int level);
// orig: (host) path `index` of level `index`'s own level data (the leveldata file): its points (false: none)
bool ld_level_path(int level, int32_t index, std::vector<float>& xyz);

// The objects swapped for the engine's functions a class of another level calls during its update (DESIGN 6.2's index
// calls: the engine reads the base's objects; for the update of a moby of level A's class they are A's registry's)
// orig: (host) the moby dispatch's update of moby `moby` begins: its class's registry swapped in (0: none needed)
int ld_enter(uint32_t moby);
// orig: (host) ... and ends (`level` ld_enter's): the registry's objects back from the engine's, the base's restored
void ld_leave(int level);
// a session class's level + 1 (session.cpp: the classes of other levels), per class id (0x1ec0 ids): 0 none
extern uint8_t g_ld_class_level[0x1ec0];
extern bool g_ld_any;  // (some class reads a registry: the dispatch asks)

// A placement of a level's own gameplay file (by its moby uid, or a class's first): what a session spawns it from
struct LdPlacement {
    int32_t uid = -1;
    std::vector<uint8_t> rec;    // the record (its size word first)
    std::vector<uint8_t> pvars;  // its pvars
    std::vector<std::pair<uint32_t, int32_t>> links;   // pvar offsets naming another moby: (offset, its uid)
    std::vector<uint32_t> ptrs;                        // pvar offsets holding an offset into its own pvars
    std::vector<std::pair<uint32_t, uint32_t>> shared; // pvar offsets pointing into the level's shared pvar data: (offset, data offset)
};
// orig: (host) level `level`'s placement of moby uid `uid` (cls < 0) or class `cls`'s first (uid < 0); false when none
bool ld_placement(int level, int32_t uid, int32_t cls, LdPlacement& out);
// orig: (host) level `level`'s registry's paths replaced by the `n` path pointers at `table` (the twin run: its level's paths
// as the native run had them, moved with it); false without a registry
bool ld_paths_take(int level, uint32_t table, uint32_t n);
// orig: (host) level `level`'s registry's volumes (the three lists of 0x80-byte records: boxes and the like, their centre at
// +0x30) moved by `d` (the twin run: its level's volumes moved with it, as its paths)
void ld_registry_move(int level, const float d[3]);
// orig: (host) level `level`'s shared pvar data in the session heap (made once a context: a copy of its level's block, its
// owner the level's registry); 0 none
uint32_t ld_shared_data(int level);

}  // namespace rac1

#endif
