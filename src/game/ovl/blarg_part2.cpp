// Blarg's Nebula G34 part 2: its own draw callbacks registered for the indirect calls (the functions: their hand ports, formerly blarg_part2_tr.cpp).
#include <game/ovl/shared_beam.h>
#include <game/ovl/shared_blarg_meshes.h>
#include <game/ovl/shared_boss_bar.h>
#include <game/ovl/shared_class1108.h>
#include <game/ovl/shared_class1302.h>
#include <game/ovl/shared_class302.h>
#include <game/ovl/shared_class615.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_class857.h>
#include <game/ovl/shared_flash.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/blarg_part2.h>

#include <game/ovl/novalis_mobys.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

// orig: (none) -- registers the draw callbacks for the indirect calls (ovl_call_moby_fn)
void blarg_part2_register() {
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002d8738u), class302_draw);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002e2258u), class615_puzzle_draw);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002ea830u), class832_draw);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002f37d0u), class857_draw);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002f6dd0u), blg_class1035_draw_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002fd1a0u), blg_class1062_draw_a_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002fd460u), blarg_class1062_draw_b);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002fd748u), blarg_class1062_draw_c);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002ff680u), beam_draw);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x003003f0u), blarg_class1083_draw_a);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x00300588u), blarg_class1083_draw_b);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x00300720u), blarg_class1083_draw_c);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x00300958u), blarg_class1083_draw_d);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x003021d8u), class1108_draw);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x00305170u), blg_class1123_draw_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x00307578u), class1302_draw);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x0030adb0u), sandmouse_glow);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x0030bfa8u), flash_draw_quad);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x00249738u), boss_health_bar_draw);  // (a HUD element's draw function)
}

}  // namespace rac1
