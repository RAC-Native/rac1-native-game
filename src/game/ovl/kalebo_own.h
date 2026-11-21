// Kalebo III (level 16): small helpers its hand-written own code shares (kalebo_*.cpp): an EE stack frame, the moby of an
// index and the walk of a moby group's list. Level 16 only.
#ifndef RAC1_GAME_OVL_KALEBO_OWN_H
#define RAC1_GAME_OVL_KALEBO_OWN_H

#include <cstdint>

#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1::kalebo {


// orig: (host) the frame timers of the EE word / halfword at `a` (timer_tick_i32 / _i16: 1 idle, 0 running, 2 expired)
inline int32_t tick32(uint32_t a) { return timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(a))); }
inline int32_t tick16(uint32_t a) { return timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(a))); }

// orig: (host) an EE stack frame of the original's size, for locals whose EE address the original passes to a callee
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// orig: (host) the ALPHA word of a quad_draw descriptor (+0x88) as the level's draws build it from four selector words at
// `at` (A, B, C, D: each word sign-extended, shifted by 0 / 2 / 4 / 6 in 64 bits) with FIX 0x80
inline uint64_t quad_blend(uint32_t at) {
    auto word = [](uint32_t a) { return static_cast<uint64_t>(static_cast<int64_t>(lw(a))); };
    return word(at) | (word(at + 4) << 2) | (word(at + 8) << 4) | (word(at + 0xc) << 6) | (0x80ull << 32);
}

// orig: (host) the moby of index `i` in the moby table (0x15ffd8)
inline uint32_t moby_at(uint32_t i) { return lwu(OV(0x0015ffd8)) + ((i & 0x7fffu) << 8); }

// orig: (host) every moby of group `group`'s list (lvd::group: halfword indices, the last one negative), in order
template <class Fn>
void for_each_group_moby(int32_t group, Fn&& fn) {
    uint32_t e = lvd::group(group);
    if (e == 0) return;
    for (;; e += 2) {
        fn(moby_at(lhu(e)));
        if (lh(e) < 0) break;
    }
}

// orig: (host) the moby of group entry `e` as the level's code at 0x2ceca8, 0x2d5ad0 and 0x2d5bc8 reads it: an entry whose moby
// is not of class `cls` is read again, without end (the original never leaves that loop; the level's groups that reach it hold
// only that class). The counter is volatile so that the loop is not assumed to end.
inline uint32_t group_entry_of_class(uint32_t e, int16_t cls) {
    volatile uint32_t reads = 0;
    for (;;) {
        const uint32_t m = moby_at(lhu(e));
        if (lh(m + 0xa6) == cls) return m;
        reads = reads + 1u;
    }
}

}  // namespace rac1::kalebo

#endif
