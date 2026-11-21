// The three glows of classes 44 and 1382: one port of the three copies (the same code relinked; class 1382's keeps its points
// at another pvar offset). Each glow is Veldin's glow_draw (0x263618: the levels' copies of it are the same code). Header
// shared_three_glows.h.
#include <game/ovl/shared_three_glows.h>

#include <game/ovl/glows.h>
#include <game/ovl/ovl_common.h>

namespace rac1 {

// Three glows of size 0.2, pulled 0.08 towards the camera, in the moby's colour (+0x90) at a fifth of full alpha (0x30).
// orig: L15:0x0029aba8 L17:0x002a2aa8 L17:0x002f0448
void three_glows_draw(uint32_t m, uint32_t first) {
    const uint32_t colour = (lwu(m + 0x90) & 0xffffffu) | 0x30000000u;
    const uint32_t points = lwu(m + 0x78) + first;
    for (uint32_t k = 0; k < 3; ++k) glow_draw(points + k * 0x10, colour, u2f(0x3e4ccccdu), u2f(0x3da3d70au));  // (0.2, 0.08)
}

void class44_glows_draw(uint32_t m) { three_glows_draw(m, 0x2a0); }
void class1382_glows_draw(uint32_t m) { three_glows_draw(m, 0x200); }

}  // namespace rac1
