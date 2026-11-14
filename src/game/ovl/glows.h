// The glows: a camera-facing quad (the template 0x1b0470, effect texture 0xb, additive) at a
// point, and the hero's draw callback that puts them on his equipment's points.
#ifndef RAC1_GAME_OVL_GLOWS_H
#define RAC1_GAME_OVL_GLOWS_H

#include <cstdint>

namespace rac1 {

// orig: 0x00263618 — a glow of colour `rgba` and size `size` at `point` (EE address), pulled
// `toward` units toward the camera
void glow_draw(uint32_t point, uint32_t rgba, float size, float toward);  // orig: 0x00263618
// orig: 0x00207d40 — the hero's glows (queued each frame by 0x207d00)
void hero_glows_draw(uint32_t hero);
// orig: 0x002df3d8 — class 1060's glows: on each shown moby of its group (+0x21) and class, the
// colour *(pvars + 4), size 1.2 (1.0 on levels 0 and 0x12)
void light_group_glows_draw(uint32_t m);
// orig: (none) — registers the glow callbacks for the indirect calls (ovl_call_moby_fn)
void glow_fns_register();

}  // namespace rac1

#endif
