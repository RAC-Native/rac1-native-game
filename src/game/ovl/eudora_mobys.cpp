// Eudora's own moby classes: the registration table. The ports are the transliterations in
// eudora_classes_tr_N.cpp (declarations: eudora_mobys.h) and, for the classes other levels share, the shared hand-written
// ports (shared_*.cpp, novalis_*.cpp: each proved against Eudora's own copy by its family batch); classes whose update is a
// bare `jr ra` are empty here.
// Keyed by Eudora's own address and by level, so the indirect call (`jalr` through moby +0x74) reaches them only
// while the played level's overlay is Eudora's (RAC1_OWN_OVERLAY=1 RAC1_LEVEL=4).
#include <game/ovl/eudora_mobys.h>

#include <game/enhance/cut_content.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/shared_class1844.h>
#include <game/ovl/shared_class280.h>
#include <game/ovl/shared_class437.h>
#include <game/ovl/shared_class604.h>
#include <game/ovl/shared_class615.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_shell.h>
#include <game/ovl/shared_swingshot_target.h>

#include <initializer_list>

#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
// orig: L4:0x0029eb18 (also 0x002c6bb0, 0x002ce118, 0x002d3570, 0x002e7aa0: classes 85, 433, 499, 570, 1835) -- a bare `jr ra`
void eud_empty_update_l4(uint32_t) {}
}  // namespace

// orig: (none) -- Eudora's registrations
void eudora_moby_fns_register() {
    for (const uint32_t a : {L4_ADDR(0x0029eb18u), L4_ADDR(0x002c6bb0u), L4_ADDR(0x002ce118u), L4_ADDR(0x002d3570u), L4_ADDR(0x002e7aa0u)}) ovl_register_level_moby_fn(EUDORA, a, eud_empty_update_l4);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x0029ecf8), eud_class86_update_l4);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002a5a08), eud_class161_update_l4);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002af058), shot_update);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002ba520), eud_class217_update_l4);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002c2270), eud_class340_update_l4);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002c4850), eud_class427_update_l4);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002c6858), eud_class432_update_l4);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002c6bb8), eud_class434_update_l4);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002ca420), eud_class466_update_l4);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002cdda0), eud_class481_update_l4);
    // (the Transport Ship: through the Cut Content option's choice, cut_content.h; with it off the retail update runs,
    // L4:0x002ce060: a bare tail call of moby_retire)
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002ce060), cut_content_ships_dispatch_l4);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002d16b8), eud_class563_update_l4);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002d3580), eud_class584_update_l4);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002d4ae0), class615_update);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002d53f8), class615_puzzle_draw);  // (the pad's draw-list callback)
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002d6f68), eud_class617_update_l4);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002d7e90), eud_class642_update_l4);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002d9a58), swingshot_target_update);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002e17d8), eud_class1101_update_l4);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002e1a10), eud_class1120_update_l4);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002e3078), eud_class1190_update_l4);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002e4418), eud_class1343_update_l4);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002e5178), eud_class1516_update_l4);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002e53a0), eud_class1549_update_l4);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002e79a8), class1844_update);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002bfd30), class280_update);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002c7e28), class437_update);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002d4368), class604_update);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002d46b8), zone_marker_update);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002d9fc0), spawn_point_update_l1);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002dc168), class832_update);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002dbf58), class832_draw);  // (its draw-list callback)
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002e68f8), sandmouse_glow);  // (the Sandmouse's draw-list callback)
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002e1f38), gold_bolt_update);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002e5928), class1633_update);
    ovl_register_level_moby_fn(EUDORA, L4_ADDR(0x002e69d0), sandmouse_update);
}

}  // namespace rac1
