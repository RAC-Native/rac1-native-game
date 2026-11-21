// Small helpers shared by Gemlik Base's (level 13) hand-written own classes (gemlik_boss_shots.cpp, ...): the frame's
// time scales, an EE stack frame of the original's size, the timers, and the classes the boss fight's shots fly through.
#ifndef RAC1_GAME_OVL_GEMLIK_OWN_H
#define RAC1_GAME_OVL_GEMLIK_OWN_H

#include <cstdint>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1::gml {

inline float dt() { return lwc1(0x0015ed6c); }      // the frame's time scale
inline float dt_sq() { return lwc1(0x0015ed70); }   // (its square)
inline float step60() { return lwc1(0x0015ed60); }  // the frame's step (1.0 at 60 Hz)

constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 his position, +0x15f0 the ship he flies: a moby or 0)

inline int32_t tick32(uint32_t a) { return timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(a))); }
inline int32_t tick16(uint32_t a) { return timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(a))); }

// an EE stack frame of the original's size (its locals are passed to the callees by address, at the original's offsets)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// a quadword of EE memory zeroed (the original's sq of $zero)
inline void sq_zero(uint32_t a) {
    for (uint32_t k = 0; k < 16; k += 4) sw(a + k, 0);
}

// `m` is not gone and not retiring (its state byte below 0xfd)
inline bool alive(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    return st != 0xfe && st != 0xfd;
}

// The classes the boss fight's missiles and mines pass through when they touch them (the missiles 0x52 / 0x53
// themselves, the fight's ships and their parts): the same set in both classes' tests.
inline bool shot_passes_through(int32_t cls) {
    switch (cls) {
    case 0x52: case 0x53:
    case 0x184: case 0x185: case 0x186: case 0x187: case 0x188: case 0x189: case 0x18a: case 0x18b: case 0x18c:
    case 0x18f: case 0x190: case 0x191:
    case 0x504: case 0x534: case 0x535: case 0x536: case 0x537: case 0x538: case 0x539: case 0x53a:
        return true;
    default:
        return false;
    }
}

}  // namespace rac1::gml

#endif
