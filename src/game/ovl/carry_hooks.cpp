// Ported overlay code: the class hooks of the carried / sucked mobys: the default class's four hooks
// (0x2db848..0x2db860), the generic carry bodies 0x2db8f8 (+0), 0x2dbb20 (+4), 0x2dbc30 (+8), 0x2dbe20 (+0xc),
// class 749's hooks (0x2d56e0..0x2d57d0), and class 0x10e's hooks (0x2cadc0..0x2caf40).
#include <game/ovl/carry_hooks.h>

#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/suck_cannon.h>
#include <game/port_stubs.h>

namespace rac1 {

// The default class's hook +0.
// orig: 0x002db848
int32_t hook_default_0(uint32_t) { return 0; }

// The default class's hook +4.
// orig: 0x002db850
int32_t hook_default_4(uint32_t) { return 0; }

// The default class's hook +8.
// orig: 0x002db858
int32_t hook_default_8(uint32_t) { return 0; }

// The default class's hook +0xc.
// orig: 0x002db860
int32_t hook_default_c(uint32_t) { return 0; }

// Class 0x10e's hook +0x10: the word at its pvars +0x14 (0 for no moby or no pvars).
// orig: 0x002caf40
uint32_t enemy10e_hook_10(uint32_t m) {
    if (m == 0) return 0;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return 0;
    return lwu(pv + 0x14);
}

// Generic carry hook +0: the Suck Cannon pulls / captures the moby.
// orig: 0x002db8f8
int32_t carry_hook0(uint32_t m, uint32_t pa1, uint32_t pa2, float /*pf12*/) {
    const uint32_t r = class_hook_10(m);
    if (r == 0) {
        debug_printf_stub();
        return 0;
    }
    if (m == 0) return 0;
    const uint32_t mst = lbu(m + 0x20);
    if (mst == 0xfeu || mst == 0xfdu) return 0;
    const int32_t st = lh(r + 0x68);
    if (st == 8) return 0;

    const uint32_t raw_st = lhu(r + 0x68);
    lq_sq(r, pa1);
    sw(r + 0x60, pa2);
    swc1(r + 0xc, 1.0f);

    const int16_t idx = static_cast<int16_t>(raw_st - 1u);
    if (static_cast<uint16_t>(idx) < 7u) {
        switch (ovl_code_to_veldin(lwu(OV(0x001ea060u) + static_cast<uint32_t>(idx) * 4u))) {
        case 0x002db99cu: {
            const float prog = lwc1(r + 0x6c);
            const uint32_t anims = lwu(r + 0x70);
            if (1.0f <= prog) {
                const uint32_t anim = lbu(anims + 2);
                if (lbu(m + 0x53) != anim) {
                    moby_anim_set(m, anim, 0, frame_scale_i(0xa));
                }
                sw(r + 0x6c, 0);
                sh(r + 0x68, 2);
                return 1;
            }
            const uint32_t anim = lbu(anims + 1);
            if (lbu(m + 0x53) != anim) {
                moby_anim_set(m, anim, 0, frame_scale_i(0xa));
            }
            return 1;
        }
        case 0x002dba24u:
            return (1.0f <= lwc1(r + 0x6c)) ? 2 : 1;
        case 0x002dba44u:
            return 2;
        case 0x002dba4cu:
            return 3;
        case 0x002dba54u: {
            const uint32_t anims = lwu(r + 0x70);
            const uint32_t anim = lbu(anims + 3);
            if (lbu(m + 0x53) != anim) {
                moby_anim_set(m, anim, 0, frame_scale_i(0xa));
            }
            sh(r + 0x68, 3);
            sw(r + 0x6c, 0);
            swc1(m + 0x58, 1.0f);
            swc1(r + 0x64, vec2_distance(V(m + 0x10), V(pa2 + 0x10)));
            return 2;
        }
        default:
            TODO_PORT(0, "jr to an unknown address")();
            return 0;
        }
    }

    const uint32_t anims = lwu(r + 0x70);
    const uint32_t anim = lbu(anims + 1);
    if (lbu(m + 0x53) != anim) {
        moby_anim_set(m, anim, 0, frame_scale_i(0xa));
    }
    sw(r + 0x6c, 0);
    sh(r + 0x68, 1);
    return 1;
}

// Generic carry hook +4: the moby enters the Suck Cannon's magazine.
// orig: 0x002dbb20
int32_t carry_hook4(uint32_t m, uint32_t pa1, uint32_t /*pa2*/, float /*pf12*/) {
    const uint32_t r = class_hook_10(m);
    if (r == 0 || m == 0) return 0;
    const uint32_t mst = lbu(m + 0x20);
    if (mst == 0xfeu || mst == 0xfdu) return 0;
    if (lh(r + 0x68) == 8) return 0;

    lq_sq(m + 0x10, pa1);
    suck_2d9fd0(lwu(r + 0x60), m, OV(0x00161b20u));
    moby_update_transform_kept(m);

    const uint32_t snd_moby = lwu(0x001403e0u);
    if (snd_moby != 0 && lh(snd_moby + 0xa6) == 0x351) {
        moby_sound_start(5, 0, snd_moby);
    }
    moby_drop_bolts(m, 0, -1);

    const uint32_t anims = lwu(r + 0x70);
    sh(r + 0x68, 4);
    const uint32_t anim = lbu(anims + 4);
    if (lbu(m + 0x53) != anim) {
        moby_anim_set(m, anim, 0, frame_scale_i(0xa));
    }
    sw(m + 0x2c, 0);
    return 1;
}

// Generic carry hook +8: the Suck Cannon fires the carried moby as a projectile.
// orig: 0x002dbc30
int32_t carry_hook8(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const uint32_t r = class_hook_10(m);
    if (r == 0 || m == 0) return 0;
    const uint32_t mst = lbu(m + 0x20);
    if (mst == 0xfeu || mst == 0xfdu) return 0;
    if (lh(r + 0x68) == 8) return 0;

    const uint32_t flags = lhu(m + 0x34) & 0xfffcu;
    sb(m + 0x31, 1);
    sh(m + 0x34, flags);

    const bool lobbed = lwc1(pa1 + 0xc) == u2f(0x3f9e0610u);
    const uint32_t anims = lwu(r + 0x70);
    if (lobbed) {
        const uint32_t anim = lbu(anims + 6);
        if (lbu(m + 0x53) != anim) {
            moby_anim_set(m, anim, 0, frame_scale_i(0x14));
        }
        sw(m + 0x98, lwu(r + 0x88));
        sh(r + 0x68, 6);
    } else {
        const uint32_t anim = lbu(anims + 5);
        if (lbu(m + 0x53) != anim) {
            moby_anim_set(m, anim, 0, frame_scale_i(0));
        }
        sh(r + 0x68, 5);
    }

    lq_sq(r + 0x10, pa1);
    sw(r + 0x94, static_cast<uint32_t>(frame_scale_i(0x12c)));
    sw(r + 0x74, pa2);
    if (pa2 != 0) {
        const uint32_t pv0 = moby_pvar_word0(pa2);
        if (pv0 != 0) {
            sh(pv0 + 0x1e, lhu(pv0 + 0x1e) | 0x80u);
        }
        lq_sq(r + 0x40, pa2 + 0x10);
        const float z = lwc1(r + 0x48);
        swc1(r + 0x90, pf12);
        swc1(r + 0x48, fpu_add(z, pf12));
    }

    if (vec2_distance(V(m + 0x10), V(0x0013f3d0u)) < 1.0f) {
        vec3_add(V(m + 0x10), V(m + 0x10), V(r + 0x10));
    }

    const int32_t snd = lh(r + 0x9c);
    if (snd >= 0) {
        moby_sound_start(snd, 0, m);
    }
    return 2;
}

// Generic carry hook +0xc: explodes / releases a carried moby.
// orig: 0x002dbe20
int32_t carry_hookc(uint32_t m, uint32_t /*pa1*/, uint32_t /*pa2*/, float /*pf12*/) {
    const uint32_t r = class_hook_10(m);
    if (r == 0 || m == 0) return 0;
    const uint32_t mst = lbu(m + 0x20);
    if (mst == 0xfeu || mst == 0xfdu) return 0;
    const int16_t st = lh(r + 0x68);
    if (st == 8 || static_cast<uint16_t>(st) >= 10u) return 0;

    switch (ovl_code_to_veldin(lwu(OV(0x001ea080u) + static_cast<uint32_t>(st) * 4u))) {
    case 0x002dbe90u: {
        const int32_t slot = lh(r + 0x6a);
        if (slot != -1) {
            constexpr uint32_t P = 0x0013f350u;
            sw(P + 0x2050u + (static_cast<uint32_t>(slot) << 2), 0);
            const int32_t cnt = lw(P + 0x207cu);
            if (cnt > 0) {
                sw(P + 0x207cu, static_cast<uint32_t>(cnt - 1));
            }
        }
        [[fallthrough]];
    }
    case 0x002dbec0u: {
        const uint32_t anims = lwu(r + 0x70);
        const uint32_t anim = lbu(anims + 7);
        if (lbu(m + 0x53) != anim) {
            moby_anim_set(m, anim, 0, frame_scale_i(0xa));
        }
        sh(r + 0x68, 7);
        return 0;
    }
    case 0x002dbf00u:
        return 1;
    case 0x002dbf08u:
        return 0;
    default:
        TODO_PORT(0, "jr to an unknown address")();
        return 0;
    }
}

// orig: 0x002d56e0
int32_t e749_hook0(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hook0(m, pa1, pa2, pf12);
    if (res != 0) {
        sb(m + 0x20, 9);
    } else if (lbu(m + 0x20) == 9) {
        sb(m + 0x20, 5);
    }
    return res;
}

// orig: 0x002d5730
int32_t e749_hook4(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hook4(m, pa1, pa2, pf12);
    if (res != 0) {
        sb(m + 0x20, 9);
    } else if (lbu(m + 0x20) == 9) {
        sb(m + 0x20, 5);
    }
    return res;
}

// orig: 0x002d5780
int32_t e749_hook8(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hook8(m, pa1, pa2, pf12);
    if (res != 0) {
        sb(m + 0x20, 9);
    } else if (lbu(m + 0x20) == 9) {
        sb(m + 0x20, 5);
    }
    return res;
}

// orig: 0x002d57d0
int32_t e749_hookc(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hookc(m, pa1, pa2, pf12);
    sb(m + 0x20, 9);
    return res;
}

// orig: 0x002cadc0
int32_t enemy10e_hook0(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hook0(m, pa1, pa2, pf12);
    if (res != 0) {
        sb(m + 0x20, 5);
    } else if (lbu(m + 0x20) == 5) {
        sb(m + 0x20, 1);
    }
    return res;
}

// orig: 0x002cae10
int32_t enemy10e_hook4(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hook4(m, pa1, pa2, pf12);
    if (res != 0) {
        sb(m + 0x20, 5);
    } else if (lbu(m + 0x20) == 5) {
        sb(m + 0x20, 1);
    }
    return res;
}

// orig: 0x002cae60
int32_t enemy10e_hook8(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const uint32_t r = class_hook_10(m);
    const int32_t res = carry_hook8(m, pa1, pa2, pf12);
    if (res != 0) {
        sb(m + 0x20, 5);
    } else if (lbu(m + 0x20) == 5) {
        sb(m + 0x20, 1);
    }
    if (lh(r + 0x68) == 6) {
        moby_sound_start(1, 0, m);
    }
    return res;
}

// orig: 0x002caf10
int32_t enemy10e_hookc(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hookc(m, pa1, pa2, pf12);
    sb(m + 0x20, 5);
    return res;
}

}  // namespace rac1
