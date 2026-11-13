// Class 1331's update: one port of the copies Oltanis and Quartu carry (the same code relinked). The slots are the
// engine's (OV: their states 0x161bf8, their mobys 0x161c88, three each), and so are their helpers (Veldin's
// fx1471_slot_end / fx1471_effect_start) and the draw callback it queues (Veldin's 0x2e2af0, fx1471_draw). Header
// shared_class1331.h.
#include <game/ovl/shared_class1331.h>

#include <game/ovl/fx1471.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// As Veldin's class 1471 (moby_1471_update): state 0 clears the slots; in state 1 each requested slot starts (state 4) and
// a slot asked to end ends, the effects drawn when any started. Unlike Veldin's, an idle slot (state 1) whose moby has
// died (its state byte negative) is let go.
// orig: L14:0x003039e0 L15:0x002e8910
void class1331_update(uint32_t m) {
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        for (uint32_t i = 0; i < 3; ++i) {
            sw(OV(0x00161bf8) + i * 4, 0);
            sw(OV(0x00161c88) + i * 4, 0);
        }
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
        return;
    }
    if (state != 1) return;
    int32_t started = 0;
    for (int32_t i = 0; i < 3; ++i) {
        const uint32_t slot = OV(0x00161bf8) + static_cast<uint32_t>(i) * 4;
        const uint32_t owner = OV(0x00161c88) + static_cast<uint32_t>(i) * 4;
        const int32_t v = lw(slot);
        if (v == 1) {
            if (static_cast<int8_t>(lbu(lwu(owner) + 0x20)) < 0) {
                sw(slot, 0);
                sw(owner, 0);
            }
            continue;
        }
        if (v == 4) {
            fx1471_slot_end(i);
            continue;
        }
        if (v == 0) continue;
        ++started;
        fx1471_effect_start(lwu(owner), static_cast<uint32_t>(i));
        sw(slot, 4);
    }
    if (started != 0) draw_callback_add(OVF(0x002e2af0), m);
}

}  // namespace rac1
