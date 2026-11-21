// The path links: one port of the copies Rilgar, Blarg, Umbris, Orxon and Pokitaru carry, the same code relinked (the
// normalised texts compared). Its one address is the engine's path table 0x1b04b0 (OV). Header shared_path_links.h.
#include <game/ovl/shared_path_links.h>

#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// orig: L5:0x0028be78 L6:0x00270b68 L7:0x0028a440 L10:0x00254ae8 L11:0x002873b0
void path_links_build(uint32_t walls, int32_t count, uint32_t path) {
    const uint32_t p = lwu(OV(0x001b04b0) + (path << 2));
    auto point = [p](int32_t i) { return p + 0x10 + (static_cast<uint32_t>(i) << 4); };
    for (int32_t i = 0; i < lw(p); ++i) sw(point(i) + 0xc, 0);
    for (int32_t a = 0; a < lw(p); ++a) {
        for (int32_t b = a + 1; b < lw(p); ++b) {
            bool blocked = false;
            for (int32_t w = 0; w < count; ++w) {
                if (path_crossed(lwu(walls + static_cast<uint32_t>(w) * 4), point(a), point(b)) != 0) {
                    blocked = true;
                    break;
                }
            }
            if (blocked) continue;
            sw(point(a) + 0xc, lwu(point(a) + 0xc) | (1u << (b & 31)));
            sw(point(b) + 0xc, lwu(point(b) + 0xc) | (1u << (a & 31)));
        }
    }
}

}  // namespace rac1
