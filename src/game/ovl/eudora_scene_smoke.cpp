// Eudora's class 1549, a cutscene's smoke: an invisible moby that, while the game is in a cutscene (game mode 2), makes
// one of the scene's mobys smoke (scene_moby_smoke: only a class-750 moby does) on the scenes 0 (scene moby 3) and 1
// (scene moby 2). Eudora's copy of Veldin's class 1545 (moby_1545_update), without the later scenes' bursts. The scene
// state L4:0x16c9e0: +0x30 the scene, +0x178.. its mobys. Header eudora_mobys.h; registered in eudora_mobys.cpp.
// Addresses are Eudora's (`orig: L4:`); Eudora-only data is L4_ADDR. States: 0 set up, 1 runs.
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l4.h>
#include <game/ovl/veldin_mobys.h>

namespace rac1 {

// orig: L4:0x002e53a0
void eud_class1549_update_l4(uint32_t m) {
    ee_stack_alloc(0x10);  // (the original's frame; it keeps no locals there)
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
    } else if (state == 1 && lw(L4_ADDR(0x0015f5c4)) == 2) {
        const uint32_t scene = lwu(L4_ADDR(0x0016ca10));
        if (scene < 2) {
            const uint32_t idx = scene == 0 ? 3 : 2;
            scene_moby_smoke(lwu(L4_ADDR(0x0016c9e0) + 0x178 + idx * 4));
        }
    }
    ee_stack_free(0x10);
}

}  // namespace rac1
