// Class 1844 (Batalia; Eudora's 1834, Gaspar's 1849, Pokitaru's 1859, Hoven's 1862), a breakable prop: one port of the five
// levels' copies, which differ only in the classes of their debris and of the broken piece left behind and in the count of
// the debris' second kind (the per-level row below). Its state 0 arms it, state 1 waits for a hit (the damage mask 0x10000,
// a damage above 0), state 2 bursts: a sound, the bolts, the debris, the broken piece, the moby retired. Header
// shared_class1844.h.
#include <game/ovl/shared_class1844.h>

#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l4.h>
#include <game/ovl/variants_l9.h>

namespace rac1 {

namespace {
// The level's classes: the debris' two kinds and the second kind's count, and the broken piece
struct Pieces {
    int32_t debris0, debris1, count1, piece;
};
Pieces pieces() {
    if (on_level(EUDORA)) return {0x72c, 0x72c, 1, 0x72b};
    if (on_level(GASPAR)) return {0x73b, 0x73b, 7, 0x73a};
    if (on_level(POKITARU)) return {0x745, 0x745, 0xb, 0x744};
    if (on_level(HOVEN)) return {0x748, 0x748, 0xb, 0x747};
    return {0x736, 0x737, 1, 0x735};  // (Batalia's)
}
}  // namespace

// orig: L4:0x002e79a8 L8:0x0030bc00 L9:0x0030a678 L11:0x0031d7d8 L12:0x0030b6c8
void class1844_update(uint32_t m) {
    const uint32_t hit = sub_0025a420(m, 0x10000, 0);
    switch (lbu(m + 0x20)) {
        case 0:
            sb(m + 0x20, 1);
            break;
        case 1:
            if (hit != 0 && 0.0f < lwc1(hit + 0x2c)) sb(m + 0x20, 2);
            break;
        case 2: {
            const Pieces p = pieces();
            moby_sound_start(0, 0, m);
            plant_bolts_drop_l1(m);
            debris_burst(m, p.debris0, 1, p.debris1, 1, p.count1, 2);
            moby_clone_l1(m, static_cast<uint32_t>(p.piece));
            moby_retire(m);
            break;
        }
        default:
            break;
    }
}

}  // namespace rac1
