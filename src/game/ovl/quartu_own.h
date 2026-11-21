// Quartu (level 15): small helpers its hand-written own code shares (quartu_props.cpp, quartu_triggers.cpp,
// quartu_clank_pad.cpp, ...): an EE stack frame, the frame time, the moby of an index and the walk of a moby group's list.
// Level 15 only.
#ifndef RAC1_GAME_OVL_QUARTU_OWN_H
#define RAC1_GAME_OVL_QUARTU_OWN_H

#include <cstdint>

#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1::quartu {

constexpr uint32_t HERO = 0x0013f350;      // the player block
constexpr uint32_t HERO_POS = 0x0013f3d0;  // the hero's position (the player block's +0x80)
constexpr uint32_t HERO_MOBY = 0x001413d0;  // the player block's +0x2080: the hero's moby
constexpr uint32_t HERO_FORM = 0x001413f4;  // the player block's +0x20a4, his movement mode: 2 while the hero is Giant Clank

// orig: (host) the frame time (0x15ed6c) and its square (0x15ed70)
inline float frame_time() { return lwc1(0x0015ed6c); }
inline float frame_time2() { return lwc1(0x0015ed70); }

// orig: (host) the frame timer of the EE word at `a` (timer_tick_i32: 1 idle, 0 running, 2 expired)
inline int32_t tick32(uint32_t a) { return timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(a))); }

// orig: (host) an EE stack frame of the original's size, for locals whose EE address the original passes to a callee
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// orig: (host) the moby of index `i` in the moby table (0x15ffd8)
inline uint32_t moby_at(uint32_t i) { return lwu(OV(0x0015ffd8)) + ((i & 0x7fffu) << 8); }

// orig: (host) every moby of group `group`'s list (lvd::group: halfword indices, the last one negative), in order; a group
// without a list is skipped
template <class Fn>
void for_each_group_moby(int32_t group, Fn&& fn) {
    uint32_t e = lvd::group(group);
    if (e == 0) return;
    for (;; e += 2) {
        fn(moby_at(lhu(e)));
        if (lh(e) < 0) break;
    }
}

}  // namespace rac1::quartu

#endif
