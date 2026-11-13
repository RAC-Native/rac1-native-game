// Novalis's own moby classes, part 2b: the gold weapon cases. Header novalis_mobys2.h. The port serves Novalis's and Gemlik
// Base's copies (the `orig:` line lists them): the globals the engine shares with Veldin are the level's own (OV), the case
// table each level's own (L<n>_ADDR), the base ELF's are bare.
#include <game/math/vecmath.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/level_change.h>
#include <game/ovl/novalis_gold_scene.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_gold_scene.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/variants_l13.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
// the cases' weapons, by the case's number (each level's own table: Novalis 0x1deb10)
uint32_t case_weapon_table() { return on_level(GEMLIK) ? L13_ADDR(0x001d3420) : L1_ADDR(0x001deb10); }
}  // namespace

// Classes 304 (the gold weapon case) and 1456..1465 (the golden weapons, one class each): the pvar's +0x40 is the
// case's number (-1: none). An even number is a case that goes once the level's flag 0x15ee20 is clear. States: 0
// (a class other than 304 goes to 2) looks up in the level's table L1:0x1deb10 the case's weapon and its "bought"
// byte (0x13e520 + weapon); not bought: starts the dialogue (talk_init; -1 -> 3, else 1), bought -> 3; 1 runs the
// dialogue (pvar +0xc = 1.9) and when it answers (talk +4 == 2) marks it bought, goes to 4, takes the weapon's
// price (table 0x1c40b0, 0x18 a row, +0x14) from the bolts 0x15ed98 and starts the weapon scene (L1:0x2aea70,
// gold_weapon_scene); 4 waits for no scripted scene (0x15f5c4) and asks for the level change (0, -1), then 3; 2 retires the
// moby once it is bought; 3 nothing.
// orig: L1:0x002e1ac0 L13:0x002e7238
void gold_case_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (pv != 0) {
        const int32_t n = lw(pv + 0x40);
        if (n != -1 && ((static_cast<uint32_t>(n) ^ 1u) & 1u) != 0) {
            if (lw(0x0015ee20) == 0) {
                moby_retire(m);
                return;
            }
        }
    }
    const uint32_t st = lbu(m + 0x20);
    const uint32_t table = case_weapon_table();
    const uint32_t bought = 0x0013e520;  // (+ the weapon: a byte per weapon)
    switch (st) {
    case 0:
        if (lh(m + 0xa6) != 0x130) {
            sb(m + 0x20, 2);
            return;
        }
        if (lbu(lwu(table + (lwu(pv + 0x40) << 2)) + bought) != 0) {
            sb(m + 0x20, 3);
            return;
        }
        sb(m + 0x20, talk_init(m, pv) != -1 ? 1u : 3u);
        return;
    case 1: {
        swc1(pv + 0xc, u2f(0x3ff33333u));
        talk_update(m, pv);
        if (lh(pv + 4) != 2) return;
        const uint32_t weapon = lwu(table + (lwu(pv + 0x40) << 2));
        sb(weapon + bought, 1);
        sb(m + 0x20, 4);
        const uint32_t w2 = lwu(table + (lwu(pv + 0x40) << 2));
        const int32_t price = static_cast<int32_t>(lhu(w2 * 0x18 + OV(0x001c40b0) + 0x14));
        sw(0x0015ed98, static_cast<uint32_t>(lw(0x0015ed98) - price));
        // (jal L1:0x2aea70 / L13:0x2a5390. Novalis keeps its generated copy until its re-glue: the oracle cannot compare
        // the shared port of this function, the original waits on the sound driver in every case)
        if (on_level(GEMLIK)) gold_weapon_scene(m, w2);
        else gold_weapon_scene_l1(m, w2);
        return;
    }
    case 2:
        if (lbu(lwu(table + (lwu(pv + 0x40) << 2)) + bought) != 0) moby_retire(m);
        return;
    case 4:
        if (lw(OV(0x0015f5c4)) != 0) return;
        level_change_request(0, -1);
        sb(m + 0x20, 3);
        return;
    default:
        return;
    }
}

}  // namespace rac1
