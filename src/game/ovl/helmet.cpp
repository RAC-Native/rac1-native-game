// Class 1290 (Pilot's Helmet item pickup, 0x002e03a0) and its sparkle helpers.
#include <game/ovl/helmet_tr.h>

#include <game/math/vecmath.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/level_change.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/veldin_mobys.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {

// orig: 0x002e06a8
void helmet_init_fx(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    for (uint32_t i = 0; i < 4; ++i) {
        swc1(pv + 0x10 + 4u * i, int_to_float(rand_mod(0xff)));
    }
    swc1(pv + 0x20, -1.0f);
    swc1(pv + 0x24, -2.25f);
    swc1(pv + 0x28, 1.25f);
    swc1(pv + 0x2c, 2.5f);
    for (uint32_t i = 0; i < 4; ++i) {
        sw(pv + 0x30 + 4u * i, static_cast<uint32_t>(frame_scale_i(0x3f + 0x40 * static_cast<int32_t>(i))));
    }
    swc1(pv + 0x4c, 2.5f);
    swc1(pv + 0x48, 3.0f);
    swc1(pv + 0x40, 2.5f);
    swc1(pv + 0x44, 3.0f);
}

// orig: 0x002e0788
void helmet_spawn_fx(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t mpos = m + 0x10;
    const uint32_t pos = ee_stack_alloc(0x10);
    Vec4 step;

    vec3_sub(V(pos), V(OV(0x00166dc0u)), V(mpos));
    vec3_set_length(V(pos), V(pos), u2f(0xbe99999au));
    vec3_set_length(step, V(pos), u2f(0x3dcccccdu));
    vec3_add(V(pos), V(pos), V(mpos));

    for (uint32_t i = 0; i < 4; ++i) {
        const uint32_t off = 4u * i;
        float ang = fpu_add(lwc1(pv + 0x10 + off), lwc1(pv + 0x20 + off));
        swc1(pv + 0x10 + off, ang);
        if (255.0f <= ang) {
            swc1(pv + 0x10 + off, fpu_sub(ang, 255.0f));
        } else if (ang <= 0.0f) {
            swc1(pv + 0x10 + off, fpu_add(ang, 255.0f));
        }

        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x30 + off))) != 0) {
            sw(pv + 0x30 + off, static_cast<uint32_t>(frame_scale_i(0xff)));
        }

        const int32_t elapsed = frame_scale_i(0xff) - lw(pv + 0x30 + off);
        const float ratio = fpu_div(int_to_float(elapsed), int_to_float(frame_scale_i(0xff)));
        const float fade = abs_f(fpu_sub(0.5f, ratio));
        const uint32_t col = static_cast<uint32_t>(rgba_lerp(0x4040ffffu, 0x1040ffffu, fade));
        const uint32_t rot = static_cast<uint32_t>(float_to_int(lwc1(pv + 0x10 + off))) & 0xffu;

        spawn_particle_3b(pos, col, rot, 0x35u, 1u, 2u, 0u, lwc1(pv + 0x40 + off));
        vec3_add(V(pos), V(pos), step);
    }

    ee_stack_free(0x10);
}

}  // namespace

// orig: 0x002e06a8
TrRet ovl_2e06a8_tr(uint32_t a0, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    helmet_init_fx(a0);
    return TrRet{0, 0.0f};
}

// orig: 0x002e0788
TrRet ovl_2e0788_tr(uint32_t a0, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    helmet_spawn_fx(a0);
    return TrRet{0, 0.0f};
}

// orig: 0x002e03a0
void helmet_update_tr(uint32_t m) {
    const float d_ang = fpu_mul(lwc1(0x0015ed6cu), u2f(0x3fc90fdbu));
    const uint32_t pv = lwu(m + 0x78);
    const uint8_t state = lbu(m + 0x20);
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), d_ang));

    switch (state) {
    case 0:
        helmet_init_fx(m);
        if (lw(pv + 0x0) == -1) {
            debug_printf_stub();
        }
        if (lbu(0x0013d4c7u) != 0) {
            debug_printf_stub();
            moby_retire(m);
            return;
        }
        sb(m + 0x20, 1u);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 1.0f));
        break;

    case 1: {
        helmet_spawn_fx(m);
        if (!(vec2_distance(V(m + 0x10), V(0x0013f3d0u)) < 3.0f)) {
            return;
        }
        if (!(abs_f(fpu_sub(lwc1(m + 0x18), lwc1(0x0013f3d8u))) < 2.0f)) {
            return;
        }
        if (lw(0x001415f8u) == 0) {
            return;
        }
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        const int32_t cs = lw(pv + 0x0);
        if (cs != -1) {
            cutscene_start(cs);
        } else {
            debug_printf_stub();
        }
        sb(m + 0x20, 2u);
        break;
    }

    case 2: {
        if (lw(OV(0x0015f5c4u)) == 2) {
            return;
        }
        debug_printf_stub();
        sub_00263d40(0x232a, -1);
        sw(OV(0x0015f640u), 0xb4u);
        item_give(7, 1);
        if (lhu(0x00141b08u) == 0) {
            sub_00203908(0x2328, 0x34);
        }

        const int32_t target_idx = lw(pv + 0x4);
        const uint32_t moby_tab = lwu(OV(0x0015ffd8u));
        if (target_idx != -1) {
            const uint32_t tm = moby_tab + (static_cast<uint32_t>(target_idx) << 8);
            sb(OV(0x001ba5d0u) + static_cast<uint32_t>(lh(tm + 0xb2)) + 0x454u, lbu(tm + 0xb0) + 2u);

            const uint32_t tm2 = moby_tab + (static_cast<uint32_t>(lw(pv + 0x4)) << 8);
            const uint8_t uid = lbu(tm2 + 0xb0);
            bool mark_b = (uid == 0xffu);
            if (!mark_b && lbu(OV(0x0015fc88u) + uid) != 0xffu) {
                const uint32_t lvl = lwu(0x0015ed84u);
                if (lbu(0x0014c050u + (lvl << 4) + uid) == 0xffu) {
                    mark_b = true;
                }
            }
            if (mark_b) {
                sb(OV(0x001bb230u) + static_cast<uint32_t>(lh(tm2 + 0xb2)) + 0x454u, uid + 2u);
            }
        } else {
            debug_printf_stub();
        }

        const uint8_t self_uid = lbu(m + 0xb0);
        if (self_uid != 0xffu) {
            checkpoint_mark_collected(self_uid);
        }
        sw(0x00141410u, 7u);
        sb(m + 0x20, 3u);
        break;
    }

    case 3:
        level_change_request(0, -1);
        moby_retire(m);
        break;

    default:
        break;
    }
}

}  // namespace rac1
