// Eudora's class 1190: a talking NPC (the talk system's talk_init / talk_update on its pvar block) who gives the
// coordinates of level 6: once the talk reaches its step 2, level 6 is marked visited, the moby's collectible is marked
// taken, the hero is sent to walk to a spot 2 in front of it (the walk-to scene block) and the level change is asked
// for. Header eudora_mobys.h; registered in eudora_mobys.cpp. Addresses are Eudora's (`orig: L4:`); Eudora-only data is
// L4_ADDR, the base ELF's are bare.
#include <game/math/vecmath.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/level_change.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/region_core.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants_l4.h>

namespace rac1 {

// pvars: the talk record (+4 its step), +0xc the talk's volume (255 with the hero in the box +0x140, else 1).
// States: 0 starts the talk (or, its collectible already taken, hides), 1 talks, 2 once the talk's step is 2 the walk
// and the level change (else, outside a scene, the talk goes on), 3 hidden.
// orig: L4:0x002e3078
void eud_class1190_update_l4(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x60);
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L4_ADDR(0x00166f40))) < 30.0f) {  // (its floor band near the camera)
        moby_floor_band(m);
        sb(m + 0x7f, 0x18);
    }
    class_mates_mark_fixed(m, 0);
    bool hide = false;
    switch (lbu(m + 0x20)) {
    case 0: {
        const uint32_t row = 0x0014c050u + lbu(m + 0xb0) + (static_cast<uint32_t>(lw(0x0015ed84)) << 4);
        if (lbu(row) == 0xff) {
            hide = true;
            break;
        }
        sb(m + 0x20, 1);
        talk_init(m, pv);
        break;
    }
    case 1:
        swc1(pv + 0xc, point_in_box(0x0013f3d0, lw(pv + 0x140)) != 0 ? 255.0f : 1.0f);
        if (talk_update(m, pv) != 0) sb(m + 0x20, 2);
        break;
    case 2: {
        if (lh(pv + 4) != 2) {
            if (lw(L4_ADDR(0x0015f5c4)) != 2) talk_update(m, pv);
            break;
        }
        level_mark_visited(6);
        checkpoint_mark_collected(lbu(m + 0xb0));
        // the walk-to scene block: the spot 2 in front (its rows +0xc0), facing back at the NPC; the walk asked for
        const uint32_t at = sp, face = sp + 0x10;
        vec3_set_length(V(at), V(m + 0xc0), 2.0f);
        vec3_add(V(at), V(at), V(m + 0x10));
        lq_sq(face, m + 0x40);
        swc1(face + 8, angle_add(lwc1(face + 8), u2f(0x40490fd0u)));
        lq_sq(L4_ADDR(0x0016c9f0), at);
        lq_sq(L4_ADDR(0x0016ca00), face);
        // (region: the 1.02 build's flag is a byte at the block's +0x4a, NTSC-U's and PAL 1.00's a halfword at +0x46,
        // as hero_walk_to_l1 writes it)
        if (region_core_moved()) sb(L4_ADDR(0x0016c9f0) + 0x3a, 1);
        else sh(L4_ADDR(0x0016c9f0) + 0x36, 1);
        level_change_request(0, -1);
        hide = true;
        break;
    }
    case 3:
        sw(m + 0x94, 0);
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        break;
    default:
        break;
    }
    if (hide) {
        sb(m + 0x20, 3);
        sw(m + 0x94, 0);
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
    }
    ee_stack_free(0x60);
}

}  // namespace rac1
