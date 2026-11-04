// The proof of the MRP world assets: at a world VU1 program start, the packet in VU1 memory is found in the
// assets, the CPU reference (src/formats/world_mrp.h) evaluates its kick program with the instance data and constants the
// VU1 memory / registers hold, and the result is compared with the GIF packets the ported program kicks. Used by (captured
// states, the levels' passes) and by the running game (RAC1_WORLD_MRP_CHECK=1).
#ifndef RAC1_GAME_RENDER_MRP_WORLD_CHECK_H
#define RAC1_GAME_RENDER_MRP_WORLD_CHECK_H

#include <formats/world_mrp.h>
#include <runtime/vu/vu_interpreter.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace rac1::mrpw {

// The world programs, by the hash of their instructions.
enum class Program : int { None = -1, TfragA = 0, TfragB = 1, TieA = 2, TieB = 3, ShrubA = 4, ShrubB = 5 };
Program program_of(const uint64_t* micro);  // the program the micro memory holds (from address 0)

// The assets of one or more levels, indexed by the content their packets leave in VU1 memory.
class WorldIndex {
public:
    void add(const World* w);
    struct Hit {
        const World* world = nullptr;
        int cls = -1, packet = -1;
    };
    // shrub: the packet at buffer base `base` of the VU1 memory after its set-up program
    bool find_shrub(const uint8_t* mem, uint32_t base, Hit& hit) const;
    // tie: the packet whose static part (header, vertices, colour index list) the VU1 memory holds (after its set-up program)
    bool find_tie(const uint8_t* mem, Hit& hit) const;
    // tfrag: the entry image (`lod` 0 far, 1 mid, 2 near) the VU1 memory holds at buffer base `base` (the lists' qwords, the
    // colours' qwords excluded)
    bool find_tfrag(const uint8_t* mem, uint32_t base, int lod, Hit& hit) const;
    // (testing, a custom level) the world whose World::custom_ties the scene draws itself (null: none)
    const World* custom = nullptr;

private:
    std::multimap<uint64_t, Hit> shrub_, tie_, tfrag_;  // (tfrag: Hit::packet = the lod)
};
uint64_t tie_key(const TiePacket& p);
uint64_t shrub_key(const ShrubPacket& p);  // (the content key of a packet, as find_shrub computes it from VU1 memory)

struct CheckStats {
    struct Kind {
        uint64_t starts = 0, unknown = 0, kicks = 0, kick_count_differ = 0, qwords = 0, exact = 0, within = 0, differ = 0, clipped_skipped = 0;
        // the clipping programs: the reference's triangles none of whose vertices the clip test flags, found among the kicked ones
        uint64_t tri_ref = 0, tri_found = 0, tri_flagged = 0, tri_kicked = 0, tri_kicked_unmatched = 0;
        uint64_t part_differ[5] = {};
    };
    Kind kind[6];
    std::vector<std::string> notes;  // the first differences
    std::map<std::string, uint64_t> cover;  // what ran: per program and start
    std::string summary() const;
};

// One program start: `st` the VU1 state as the program starts (not modified), `start` its micro address.
void check_start(const WorldIndex& idx, const VUState& st, const uint64_t* micro, uint32_t start, CheckStats& stats);

// One draw start's kicks as the CPU reference made them (`want`) against the GIF packets the program kicked (`got`): the A
// programs qword by qword, the clipping programs (B) by the triangles their clip test leaves alone (the in-game check of the
// MRP world's scene, mrp_world_scene.h)
void compare_draw(Program prog, const std::vector<Kicked>& want, const std::vector<std::vector<std::array<uint32_t, 4>>>& got, CheckStats& stats, const char* what);

}  // namespace rac1::mrpw

#endif
