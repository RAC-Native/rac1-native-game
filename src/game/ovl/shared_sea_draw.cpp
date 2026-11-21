// The sea's draw callback: one port of the copies Kerwan (level 3), Umbris (7, two: the second sea of class 1018),
// Batalia (8) and Oltanis (14) carry (the same code relinked: a call of ocean_draw_one, aridia_ocean.cpp, on the level's
// wave record, per level below). Veldin's finale's class 1750 has a body of the same shape that is another function (it
// writes the card's save image): not this. Header shared_sea_draw.h.
#include <game/ovl/shared_sea_draw.h>

#include <game/ovl/aridia_ocean.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l3.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

// orig: L3:0x002dc990 L7:0x002f8038 L8:0x002da0d0 L14:0x00303350
void sea_draw(uint32_t) {
    switch (ovl_reloc_level()) {
    case KERWAN: ocean_draw_one(L3_ADDR(0x001dc1a0)); return;
    case UMBRIS: ocean_draw_one(L7_ADDR(0x001d3400)); return;
    case BATALIA: ocean_draw_one(L8_ADDR(0x001d7060)); return;
    default: ocean_draw_one(L14_ADDR(0x001e9c60)); return;
    }
}

// orig: L7:0x0030d420
void sea_draw_2(uint32_t) { ocean_draw_one(L7_ADDR(0x00208560)); }

}  // namespace rac1
