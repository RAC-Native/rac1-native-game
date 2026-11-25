#ifndef RAC1_GAME_COLLISION_COLLISION_STATE_H
#define RAC1_GAME_COLLISION_COLLISION_STATE_H
// The collision's results as native state (stage 5): the last hit and the moby hit list the queries leave for their
// callers. The routines (src/game/collision/collide_*.cpp) write this state and, at the same moment, the EE words the
// original writes: the block 0x173e40 +0x18 .. +0x7f and the hit list 0x177f00 (the projection), so EE memory stays byte
// for byte what it was and the callees that still take an EE address (collision.h's *_addr()) find the words there.
// collision.h's accessors read this state (RAC1_COLL_STATE=native, the default) or the EE words (RAC1_COLL_STATE=ee: the
// state is still written, not read). RAC1_COLL_STATE_CHECK=1 compares every accessor read with the EE words and reports
// each difference with the accessor's name: a difference is an EE write that did not reach the state.
//
// What the state holds and what stays EE memory:
//   - the hit (+0x18 .. +0x7f): written by the five routines (coll_hit_mirror) and by the few class updates that change it
//     in place (collision.h's setters, which write both); read through collision.h. Native.
//   - the hit list (0x80 words 0x177f00 .. 0x1780ff, the words past its terminator included, as stale as in EE memory):
//     written by mobys_in_sphere only. Native. A list of 0x80 mobys or more runs into the contact ring 0x178100, as the
//     original's: those entries (and that terminator) are EE memory only, read from there.
//   - the head (+0x00 the tree, +0x04 / +0x08 / +0x0c the posed-joint cache, +0x10 the query counter, +0x14 the contact
//     ring's next index) stays EE memory: the tree keys the collision scene; the query counter is read and written by the
//     pause menu, a level's exit and the sections, the ring's index by moby_hit_record, the joint cache words by the boot
//     module's title set-up; and the routines update the counter and the joint cache in place (read, then write).
//   - the hit moby and the list's entries are EE addresses of mobys (moby state stays EE memory); the contact ring's
//     records are moby state too: EE memory.
// EE writes that do not come through here reach the state by coll_state_import(): after the overlay image is loaded
// (a level's start, the boot module's), after a dev tool's state snapshot is loaded, after ground_puff_spawn raises the
// hit point it was handed by address, and in the oracle before each port runs (tests/oracle/oracle_env.cpp).
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

#include <cstdint>

namespace rac1 {

// The hit the queries leave (+0x18 .. +0x7f of the block 0x173e40; field names inferred: see collision.h)
struct CollisionHit {
    uint32_t moby;   // +0x18
    uint32_t flags;  // +0x1c
    Vec4 point;      // +0x20
    Vec4 pushed;     // +0x30
    Vec4 normal;     // +0x40
    Vec4 tri[3];     // +0x50, +0x60, +0x70
};

constexpr uint32_t kCollHitListWords = 0x80;  // the hit list's room before the contact ring

struct CollState {
    CollisionHit hit;
    uint32_t list[kCollHitListWords];  // the hit list's words 0x177f00 + 4 i
};

// orig: (host) the state (the game thread's: the queries and their readers run there)
extern CollState g_coll_state;
// orig: (host) RAC1_COLL_STATE: true (native, the default) the accessors read g_coll_state, false (ee) the EE words
extern bool g_coll_state_native;
// orig: (host) RAC1_COLL_STATE_CHECK: every accessor read compared with the EE words
extern bool g_coll_state_check;

// orig: (host) the switch's value / set (the oracle's batch 543 runs both)
inline bool coll_state_native() { return g_coll_state_native; }
void coll_state_set_native(bool on);
// orig: (host) the check mode's value / set (the oracle turns it on at its start)
inline bool coll_state_check() { return g_coll_state_check; }
void coll_state_set_check(bool on);
// orig: (host) the differences the check mode found so far
uint64_t coll_state_differences();
// orig: (host) the check mode's report lines off (`quiet`) or on: batch 543 plants differences on purpose and counts them
void coll_state_set_check_quiet(bool quiet);

// orig: (host) the EE words (the block +0x18 .. +0x7f and the list's 0x80 words, the level's copy) decoded into `out`
void coll_state_decode_ee(CollState& out);
// orig: (host) the state taken from the EE words (coll_state_decode_ee): after an EE write that did not come through here
void coll_state_import();
// orig: (host) `a` and `b` the same, bit for bit; the first differing field's name in *field (when not null)
bool coll_state_same(const CollState& a, const CollState& b, const char** field);

// The check mode's comparisons (cold: only while RAC1_COLL_STATE_CHECK is on): the EE word(s) at `ee` against the state's
// value; a difference is counted and reported ("[coll-state] differs: <accessor> ...").
// orig: (host) one word
[[gnu::cold]] void coll_state_check_word(const char* accessor, uint32_t ee, uint32_t native);
// orig: (host) a vector's four words
[[gnu::cold]] void coll_state_check_vec(const char* accessor, uint32_t ee, const Vec4& native);

// The writers outside the routines (collision.h's setters): the state's field and the original's EE store, at once.
// orig: (host) +0x18 (sw)
void coll_state_put_moby(uint32_t moby);
// orig: (host) +0x20 / +0x30 / +0x40 (a quadword store: `off` 0x20, 0x30 or 0x40)
void coll_state_put_vec(uint32_t off, const Vec4& v);
// orig: (host) one lane's word of +0x20 / +0x30 / +0x40 (`off` + 4 * lane: 0x28, 0x38, 0x48 ...)
void coll_state_put_lane(uint32_t off, uint32_t lane, uint32_t word);
// orig: (host) hit list entry i (mobys_in_sphere's store at 0x177f00 + 4 i; the state's when i < 0x80)
void coll_state_list_put(uint32_t i, uint32_t moby);
// orig: (host) hit list entry i as the routine that just wrote it reads it back (the state's when i < 0x80, else EE memory)
inline uint32_t coll_state_list_entry(uint32_t i) { return i < kCollHitListWords ? g_coll_state.list[i] : lwu(OV(0x00177f00) + i * 4u); }

}  // namespace rac1

#endif  // RAC1_GAME_COLLISION_COLLISION_STATE_H
