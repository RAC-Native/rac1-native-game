// Class 994 of Kerwan (level 3) and class 1260 of Oltanis (14): the sea's controller. One port of both levels' copies, the
// same code relinked; the wave's alpha differs (Kerwan 0x20, Oltanis 0x40). The wave record and the draw callback queued
// are the level's own (per level below); the wave table's scaling is the shared ocean_wave_table_set (the levels' copies
// L3:0x291198 / L14:0x2aabf8). Header shared_class994.h.
//
// States: 0 start, 1 running.
#include <game/ovl/shared_class994.h>

#include <game/ovl/aridia_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l3.h>

namespace rac1 {

namespace {
// the level's wave record, the wave's alpha, and its copy of the sea's draw callback
uint32_t wave_record() { return on_level(OLTANIS) ? L14_ADDR(0x001e9c60) : L3_ADDR(0x001dc1a0); }
uint32_t wave_alpha() { return on_level(OLTANIS) ? 0x40u : 0x20u; }
uint32_t draw_callback() { return on_level(OLTANIS) ? L14_ADDR(0x00303350) : L3_ADDR(0x002dc990); }
}  // namespace

// orig: L3:0x002dc9b0 L14:0x00303370
void class994_update(uint32_t m) {
    switch (lbu(m + 0x20)) {
    case 0:
        ocean_wave_table_set(wave_record(), wave_alpha(), u2f(0x3f2aaaabu));  // (2/3)
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
        return;
    case 1:
        sub_001f93e0(draw_callback(), m);
        return;
    default:
        return;
    }
}

}  // namespace rac1
