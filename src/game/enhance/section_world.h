// The open engine's game worlds as sections (docs/open-engine.md, "Game worlds as sections (stage 8)"; DESIGN 7.1, stage 8): a
// section of a session (sections.h) whose scenery is a game level's world - its tfrags, ties and shrubs, drawn by the game's own
// world passes (their culling, LODs, zone grid and lighting) and by the modern renderer from that level's MRP world assets -, its
// collision, sky, lights, fog and background, music, its level data and (a chosen part of) its placed objects. The level's
// files are read from the disc on a worker; its world's blocks (the tfrags, the zone grid, the texels, the tie and shrub
// classes) copied into the session heap, registered by the game's own functions (moby_lod_set, tie_class_register,
// shrub_class_register) and lit by its own (world_*_lighting) as its level load does, within the commit's budget a frame,
// into tables the section keeps (WorldState) and swaps into the engine's when it is entered. Nothing here runs unless a
// session's section names a `world`.
#ifndef RAC1_GAME_ENHANCE_SECTION_WORLD_H
#define RAC1_GAME_ENHANCE_SECTION_WORLD_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rac1 {

// A level's files as the worker read them: the asset index, the world's blocks of its asset data (by their offsets there),
// its collision and sky blocks, its gameplay file (decompressed)
struct WorldBytes {
    struct Range {
        uint32_t lo = 0;            // (its offset in the level's asset data)
        std::vector<uint8_t> b;
        const char* what = "";
    };
    std::vector<uint8_t> index;
    std::vector<Range> ranges;
    std::vector<uint8_t> coll, sky, gp;
    size_t core = 0;  // (the asset data's size: for the logs)
};
// orig: (host) the worker's part: level `level`'s data file read (its index, its asset data decompressed: the world's blocks,
// the collision and sky blocks when asked) and its gameplay file (`pal`: the PAL disc's) decompressed; false (`err`) when not
bool world_read(int level, bool pal, bool coll, bool sky, WorldBytes& out, std::string& err);

// The engine's world state: what the level load sets for its world and the per-frame passes read (the tfrag entries and LOD
// distances, the zone grid and its mode, the tie and shrub classes' tables and instances, the world textures' tables), and
// separately its lights (the directional lights and the light grid) and the fog's words.
struct WorldState {
    std::vector<uint32_t> words;
    std::vector<uint8_t> tables;
};
struct WorldLights {
    uint32_t grid_count = 0;
    std::vector<uint8_t> bytes;
};
// orig: (host) the engine's world state now / made the engine's
void world_state_get(WorldState& s);
void world_state_put(const WorldState& s);
void world_lights_get(WorldLights& l);
void world_lights_put(const WorldLights& l);

// A world's build into the session heap, a step at a time (the commit's budget)
struct WorldBuild {
    int level = -1;
    uint32_t block = 0, index = 0;  // (the world's heap block, the index's copy in it)
    struct At {
        uint32_t lo, size, at;
    };
    std::vector<At> map;  // (each copied range: its asset-data offset, size, heap address)
    uint32_t gp = 0;      // (the level's gameplay file in the heap: the section's)
    WorldState st;        // the section's world state once built
    WorldLights lights;   // its level's lights
    // the steps
    int step = 0;
    size_t r = 0, copied = 0;
    int32_t k = 0, last = -1;
    uint32_t tie_n = 0, shrub_n = 0, tfrag_n = 0;
    bool masks = false;
    size_t bytes = 0;  // (the heap bytes: the world block, the instances)
    double ms = 0;     // (the build's time on the game's thread, every step)
    double step_max[9] = {};  // (the longest call of each step)
    // orig: (host) the heap address of the level's asset-data offset `off` (0: not copied)
    uint32_t core(uint32_t off) const;
};
// orig: (host) the heap blocks the world needs (its blocks, the index): false when the heap has no room (`owner` the section's)
bool world_build_begin(WorldBuild& w, int level, const WorldBytes& b, uint32_t gp, const std::string& owner);
// orig: (host) one step of the build (a copy of at most 64 KB, the registrations, a batch of instances or of their lighting):
// false when it is done. The section's tables are the engine's during the step, the engine's own (the current section's) put
// back after it
bool world_build_step(WorldBuild& w, const WorldBytes& b, const std::string& owner);

// orig: (host) the world's level's placed objects' zone masks (+0x36, the gameplay file's +0x8c list by the placement's uid on
// its level): set on the moby `m` placed from its level's uid `uid` (a world with a zone grid)
void world_moby_mask(const WorldBuild& w, uint32_t m, int32_t uid);

// (testing) RAC1_COLL_PROBE=<frame>:<x0>,<y0>,<x1>,<y1>,<step>,<z>: at that game frame the current collision probed straight down from z
// (the game's floor_probe) on the grid, the heights' hash and sum logged (`[probe] ...`): a world section's against its native level
// orig: (host)
void world_probe_frame(uint64_t frame);

// orig: (host) the modern world renderer's level (mrp_world_renderer.cpp): the world section's level while it is the current
// section's, -1 the session's own (the level mod's slot)
void world_renderer_level(int level);
int world_renderer_level_get();

}  // namespace rac1

#endif
