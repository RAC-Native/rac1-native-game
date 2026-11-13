// Ported overlay code: the Swingshot's grapple hook moby (class 0xd1, update 0x2c7070).
#include <game/ovl/grapple.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>

namespace rac1 {

// orig: 0x002c7070
void swingshot_grapple_update(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;

    const uint32_t hand = lwu(pv);
    if (hand == 0 || lh(hand + 0xa6) != 0xd0) {
        moby_retire(m);
        return;
    }
    const uint32_t hst = lbu(hand + 0x20);
    if (hst == 0xfe || hst == 0xfd || lwu(lwu(hand + 0x78) + 0x14) != m) {
        moby_retire(m);
        return;
    }

    if ((lhu(hand + 0x34) & 0x1u) != 0 || lbu(0x001413ffu) != 0) {
        sh(m + 0x34, lhu(m + 0x34) | 0x0041u);
    } else {
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
    }

    switch (lbu(m + 0x20)) {
    case 0: {
        if ((lbu(m + 0x70) & 0x2u) != 0) {
            if (lbu(m + 0x53) != 1) {
                moby_anim_set(m, 1, 0, frame_scale_i(0xa));
            }
            sb(m + 0x20, 1);
            swc1(m + 0x58, 1.0f);
        }
        break;
    }
    case 1: {
        const uint32_t cur_hst = lbu(lwu(pv) + 0x20);
        if (cur_hst == 2) {
            if (lbu(m + 0x53) != 3) {
                moby_anim_set(m, 3, 0, frame_scale_i(0xa));
            }
            sb(m + 0x20, 3);
            swc1(m + 0x58, 1.0f);
        } else if (cur_hst == 8) {
            if (lbu(m + 0x53) != 2) {
                moby_anim_set(m, 2, 0, frame_scale_i(0xa));
            }
            sb(m + 0x20, 2);
            swc1(m + 0x58, 1.0f);
        }
        break;
    }
    case 2: {
        if ((lbu(m + 0x70) & 0x2u) != 0) {
            moby_retire(m);
            return;
        }
        break;
    }
    case 3: {
        if ((lbu(m + 0x70) & 0x2u) != 0) {
            if (lbu(m + 0x53) != 4) {
                moby_anim_set(m, 4, 0, frame_scale_i(0xa));
            }
            sb(m + 0x20, 4);
        }
        break;
    }
    case 4: {
        const uint32_t cur_hst = lbu(lwu(pv) + 0x20);
        if (cur_hst - 3u < 3u) {
            if (lbu(m + 0x53) != 5) {
                moby_anim_set(m, 5, 0, frame_scale_i(0xa));
            }
            sb(m + 0x20, 5);
        }
        break;
    }
    case 5: {
        if ((lbu(m + 0x70) & 0x2u) != 0) {
            swc1(m + 0x58, 0.0f);
        }
        const uint32_t cur_hst = lbu(lwu(pv) + 0x20);
        if (cur_hst - 6u < 2u) {
            const uint32_t anim = lbu(m + 0x53);
            swc1(m + 0x58, 1.0f);
            if (anim != 7) {
                moby_anim_set(m, 7, 0, frame_scale_i(0xa));
            }
            sb(m + 0x20, 7);
        }
        break;
    }
    case 6:
        break;
    case 7:
        if ((lbu(m + 0x70) & 0x2u) == 0) break;
        [[fallthrough]];
    default: {
        if (lbu(m + 0x53) != 1) {
            moby_anim_set(m, 1, 0, frame_scale_i(0xa));
        }
        sb(m + 0x20, 1);
        break;
    }
    }

    moby_update_transform_kept(m);
}

// orig: (none)
void grapple_fns_register() {
    ovl_register_moby_fn(0x002c7070, swingshot_grapple_update);
}

}  // namespace rac1
