// Novalis's own draw callbacks registered for the indirect calls (the functions: novalis_draw_cbs_tr.cpp, and the shared
// ports shared_*.cpp for the classes several levels carry).
#include <game/ovl/novalis_draw_cbs.h>

#include <game/ovl/novalis_mobys.h>
#include <game/ovl/shared_class1135.h>
#include <game/ovl/shared_class325.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/variants_l1.h>

namespace rac1 {

// orig: (none) — registers the draw callbacks for the indirect calls (ovl_call_moby_fn)
void novalis_draw_cbs_register() {
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x00302438u), class832_draw);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x003094f0u), class1135_draw);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x0030bfa8u), class325_draw);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x0030de68u), sandmouse_glow);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x0030f0e0u), nov_class1848_draw_l1);
}

}  // namespace rac1
