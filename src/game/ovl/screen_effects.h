// Screen-space effects: the lens flare (0x167040: a light moby's flare, 16 elements along the
// line from the screen centre through it) and the list of moby-anchored 2D effects (0x169040,
// 0x30 bytes each, count 0x169100: rings / quadrant patterns / single sprites of an effect
// texture at a projected point), drawn as GIF sprites into the frame's packet buffer.
#ifndef RAC1_GAME_OVL_SCREEN_EFFECTS_H
#define RAC1_GAME_OVL_SCREEN_EFFECTS_H

#include <cstdint>

namespace rac1 {

// orig: 0x001f6c38 — `in` (EE address) to screen: out = (x, y) in GS units x 16 (2048 the
// centre), z / 1024
void screen_project(uint32_t out, uint32_t in);  // orig: 0x001f6c38
// orig: 0x001fa8f0 — a turned sprite (64x64 texel corner a0 / a1, flipped by t1 / t2) of size
// (sx, sy), turned by `angle` about the pivot (px, py) of its unit square, at screen (x, y)
void screen_sprite(int32_t u, int32_t v, uint64_t tex0, uint32_t z, uint32_t rgba, uint32_t flip_u, uint32_t flip_v, float x, float y, float sx, float sy, float angle, float px, float py);  // orig: 0x001fa8f0
// orig: 0x001edc38 — one entry of the list at screen (x, y)
void screen_effect_draw(uint32_t e, float x, float y);
// orig: 0x001ee040 — the list's entries, then emptied
void screen_effects_draw();
// orig: 0x001ed880 — the lens flare
void lens_flare_draw();

}  // namespace rac1

#endif
