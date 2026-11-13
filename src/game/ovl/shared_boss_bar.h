// The bosses' health bar on the HUD (Blarg's class 1051, Umbris, Gemlik, the finale's Veldin), shared: one port of the
// four levels' copies (shared_boss_bar.cpp). Its `orig:` line lists every level's address.
#ifndef RAC1_GAME_OVL_SHARED_BOSS_BAR_H
#define RAC1_GAME_OVL_SHARED_BOSS_BAR_H

#include <cstdint>

namespace rac1 {

// The bar of the boss `m` (its health +0x74 of +8, its fade +0x70).
void boss_health_bar_draw(uint32_t m);  // L6:0x00249738 L7:0x00262078 L13:0x00245cf8 L18:0x0023cd60

}  // namespace rac1

#endif
