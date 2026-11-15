// The moby pass's draw loop 0x2559c0 in two forms: the transliteration (moby_draw.cpp, the reference) and the same code with the
// VU0 registers as a local copy and the memory accessors' fast paths inline (moby_draw_hand.cpp). moby_draw_loop (moby_draw.h)
// picks one, or runs both and compares them (moby_draw_fast.cpp, RAC1_PERF_MOBY_DRAW).
// orig: (host)
#ifndef RAC1_GAME_OVL_MOBY_DRAW_HAND_H
#define RAC1_GAME_OVL_MOBY_DRAW_HAND_H

#include <cstdint>

namespace rac1 {

// orig: 0x002559c0 -- the transliteration (moby_draw.cpp)
uint32_t moby_draw_loop_tr(uint32_t first, uint32_t ring, int32_t count, uint32_t defer);

namespace moby_hand {
// orig: 0x002559c0 -- the faster form (moby_draw_hand.cpp); only while vu0l::usable()
uint32_t draw_loop(uint32_t first, uint32_t ring, int32_t count, uint32_t defer);
}  // namespace moby_hand

}  // namespace rac1

#endif
