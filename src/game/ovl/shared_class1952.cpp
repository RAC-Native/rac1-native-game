// The draw-queuing classes of Kalebo III and Drek's fleet: one body (the same code in the seven copies), each class's
// draw callback the level's own address. Header shared_class1952.h.
#include <game/ovl/shared_class1952.h>

#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
// state 0 -> 1; in state 1 the draw `callback` queued for the moby
void queue_draw(uint32_t m, uint32_t callback) {
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) sb(m + 0x20, 1);
    else if (st == 1) sub_001f93e0(callback, m);
}
}  // namespace

// orig: L16:0x002e9df0
void class1952_update(uint32_t m) { queue_draw(m, L16_ADDR(0x002e9ce0)); }
// orig: L17:0x002f57a0
void class1907_update(uint32_t m) { queue_draw(m, L17_ADDR(0x002f5690)); }
// orig: L17:0x002f5ab8
void class1908_update(uint32_t m) { queue_draw(m, L17_ADDR(0x002f59a8)); }
// orig: L17:0x002f5dd0
void class1910_update(uint32_t m) { queue_draw(m, L17_ADDR(0x002f5cc0)); }
// orig: L17:0x002f60e8
void class1911_update(uint32_t m) { queue_draw(m, L17_ADDR(0x002f5fd8)); }
// orig: L17:0x002f6400
void class1913_update(uint32_t m) { queue_draw(m, L17_ADDR(0x002f62f0)); }
// orig: L17:0x002f6718
void class1918_update(uint32_t m) { queue_draw(m, L17_ADDR(0x002f6608)); }

}  // namespace rac1
