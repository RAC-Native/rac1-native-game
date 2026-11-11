// What the tie and shrub passes chained for their first programs (tie A, shrub A), as they wrote it. The hand-written passes
// (world_cull_tie.cpp, world_cull_shrub.cpp) cull and build the chain themselves: here they note each draw of that chain - its
// class and LOD packet, where its instance's matrix is in the frame packet, where its colours are - in the chain's order. The MRP
// makes those draws' scene packets from the notes and the level's own tables instead of unpacking and running that part of the
// chain on its private VU1 (mrp_world_scene.h, RAC1_PERF_WORLD_CHAINED): the same draws, as the pass decided them.
// A pass's notes stand only when it ran by hand, to its end, as the frame's real pass (world_pass's run; not inside a journal:
// the hand-written culls' check, a shadow run). Otherwise the MRP walks that pass's chain as before.
// orig: (host)
#ifndef RAC1_GAME_OVL_WORLD_CHAINED_H
#define RAC1_GAME_OVL_WORLD_CHAINED_H

#include <cstdint>
#include <vector>

namespace rac1 {

struct WorldChained {
    // a tie A draw (0x296dd4: an instance of a LOD packet): the class record, the LOD and the packet's place in the LOD's list, the
    // instance's 0x60 bytes in the frame packet (its rows, morph, state and fog), its colours (the instance data's + 0x40)
    struct Tie {
        uint32_t cls = 0, matrix = 0, colours = 0;
        uint16_t packet = 0;
        uint8_t lod = 0;
    };
    // a shrub A draw (0x287848: up to 5 instances of a LOD packet): the class record, the packet's place in its list, each instance's
    // 0x50 bytes in the frame packet (its rows, then a faded one's first colour and fade) and its colours' address
    struct Shrub {
        uint32_t cls = 0;
        uint32_t matrix[5] = {}, colours[5] = {};
        uint16_t packet = 0;
        uint8_t n = 0;
        bool faded = false;
    };
    // a pass's real run: `kind` 1 tie, 2 shrub; `first` the chain's address when it started (the pass's range, scene_note_pass); its
    // draws [at, at + n) of `tie` / `shrub`; `ok`: it ran by hand to its end
    struct Run {
        int kind = 0;
        uint32_t first = 0, at = 0, n = 0;
        bool ok = false;
    };
    std::vector<Tie> tie;
    std::vector<Shrub> shrub;
    std::vector<Run> runs;
};
const WorldChained& world_chained();
// orig: (host) a new game frame: no notes
void world_chained_clear();
// orig: (host) world_pass: a tie / shrub pass's real run starts, the chain at `first`; and it returned
void world_chained_begin(int kind, uint32_t first);
void world_chained_end();
// orig: (host) the passes: whether to note (a run is open and no journal is on), a draw, the pass at its end
bool world_chained_noting();
void world_chained_tie(const WorldChained::Tie& t);
void world_chained_shrub(const WorldChained::Shrub& s);
void world_chained_done();

}  // namespace rac1

#endif
