// Quartu (level 15) factory doors, ported by hand from the level's own code:
// - class 67 (L15:0x0029aff0): horizontal sliding door triggered by a box, switch moby, or persistent bit
// - class 92 (L15:0x002a36d0): vertical sliding door (also used in the Giant Clank arena with a level flag)
// - class 93 (L15:0x002a3ba8): hinged swinging door triggered by proximity boxes or a linked switch moby
// The original's order of calls and float operations is preserved.
#include <game/ovl/quartu_mobys.h>

#include <game/lib/libgcc_fp.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {

constexpr uint32_t HERO_POS = 0x0013f3d0u;
constexpr uint32_t SOUND_SLOTS_BASE = 0x0013e550u;
constexpr uint32_t SOUND_SLOT_STRIDE = 0x70u;

void ensure_door_sound(uint32_t m, uint32_t ch_addr) {
    const int32_t ch = lw(ch_addr);
    const uint32_t slot = SOUND_SLOTS_BASE + static_cast<uint32_t>(ch * static_cast<int32_t>(SOUND_SLOT_STRIDE));
    if (lwu(slot + 0x88) != m || lbu(slot + 0x74) == 0) {
        sw(ch_addr, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
    }
}

void stop_door_sound(uint32_t m, uint32_t ch_addr) {
    const int32_t ch = lw(ch_addr);
    if (ch == -1) return;
    const uint32_t slot = SOUND_SLOTS_BASE + static_cast<uint32_t>(ch * static_cast<int32_t>(SOUND_SLOT_STRIDE));
    if (lwu(slot + 0x88) == m && lbu(slot + 0x74) != 0) {
        sub_0028d918(ch);
    }
}

void slide_horizontal(uint32_t m, uint32_t pv) {
    const float yaw = lwc1(m + 0x48);
    Vec4 delta{};
    delta.x = fpu_mul(cos_vu(yaw), -lwc1(pv + 0x18));
    delta.y = fpu_mul(sin_vu(yaw), -lwc1(pv + 0x18));
    delta.z = 0.0f;
    Vec4 pos{};
    vec3_add(pos, V(pv), delta);
    V(m + 0x10) = pos;
}

}  // namespace

// Horizontal sliding door (class 67).
// orig: L15:0x0029aff0
void qua_class67_update_l15(uint32_t m) {
    const uint8_t state = lbu(m + 0x20);
    const uint32_t pv = lwu(m + 0x78);
    if (state >= 5) return;

    // Jump table L15:0x001e25f0 (5 entries)
    switch (state) {
    case 0: {  // (0x0029b034)
        lq_sq(pv, m + 0x10);
        sb(m + 0x20, 1);
        break;
    }
    case 1: {  // (0x0029b048)
        const int32_t trig_idx = lw(pv + 0x1c);
        bool open = false;
        sw(pv + 0x18, 0);
        if (trig_idx != -1) {
            const uint32_t tm = lwu(L15_ADDR(0x0015ffd8u)) + (static_cast<uint32_t>(trig_idx) << 8);
            const int16_t cls = lh(tm + 0xa6);
            if (cls == 0x4b9 && lbu(tm + 0x20) == 4) {
                open = true;
            } else if (cls == 0x596) {
                open = (lbu(tm + 0x20) == 4);
            }
        }
        if (lw(pv + 0x28) != 0) {
            const int32_t uid = static_cast<int32_t>(lh(m + 0xb2));
            if (lbu(L15_ADDR(0x001bb830u) + static_cast<uint32_t>(uid) + 0x454) != 0) {
                open = true;
            } else {
                const uint32_t lvl = lwu(0x0015ed84u);
                const uint32_t word_addr = 0x0014c190u + (lvl << 8) + static_cast<uint32_t>((uid >> 5) << 2);
                if (((lw(word_addr) >> (uid & 0x1f)) & 1) != 0) {
                    open = true;
                }
            }
        }
        if (point_in_box(HERO_POS, lw(pv + 0x10)) != 0 || open) {
            sb(m + 0x20, 2);
        }
        break;
    }
    case 2: {  // (0x0029b140)
        ensure_door_sound(m, pv + 0x20);
        const float dist = lwc1(pv + 0x24);
        const float step = fpu_mul(fpu_div(dist, u2f(0x3f19999au)), lwc1(0x0015ed6cu));
        const float cur = fpu_add(lwc1(pv + 0x18), step);
        swc1(pv + 0x18, cur);
        if (dist < cur) {
            stop_door_sound(m, pv + 0x20);
            swc1(pv + 0x18, lwc1(pv + 0x24));
            sw(pv + 0x20, 0xffffffffu);
            if (lw(pv + 0x1c) != -1 || lw(pv + 0x28) != 0) {
                const int32_t uid = static_cast<int32_t>(lh(m + 0xb2));
                const uint32_t lvl = lwu(0x0015ed84u);
                const uint32_t bit = 1u << (uid & 0x1f);
                const uint32_t save_addr = 0x0014c190u + (lvl << 8) + static_cast<uint32_t>((uid >> 5) << 2);
                sw(save_addr, lwu(save_addr) | bit);
                const uint32_t flag_addr = L15_ADDR(0x001baad0u) + static_cast<uint32_t>((uid >> 5) << 2);
                sw(flag_addr, lwu(flag_addr) | bit);
                sb(m + 0x20, 5);
            } else {
                sb(m + 0x20, 3);
            }
        }
        slide_horizontal(m, pv);
        break;
    }
    case 3: {  // (0x0029b2f0)
        if (point_in_box(HERO_POS, lw(pv + 0x14)) == 0 &&
            point_in_box(L15_ADDR(0x001673c0u), lw(pv + 0x14)) == 0) {
            sb(m + 0x20, 4);
        }
        break;
    }
    case 4: {  // (0x0029b328)
        ensure_door_sound(m, pv + 0x20);
        const float step = fpu_mul(fpu_div(lwc1(pv + 0x24), u2f(0x3f19999au)), lwc1(0x0015ed6cu));
        const float cur = fpu_sub(lwc1(pv + 0x18), step);
        swc1(pv + 0x18, cur);
        if (cur < 0.0f) {
            stop_door_sound(m, pv + 0x20);
            sw(pv + 0x18, 0);
            sw(pv + 0x20, 0xffffffffu);
            sb(m + 0x20, 1);
        }
        slide_horizontal(m, pv);
        if (point_in_box(HERO_POS, lw(pv + 0x10)) != 0 ||
            point_in_box(L15_ADDR(0x001673c0u), lw(pv + 0x10)) != 0) {
            sb(m + 0x20, 2);
        }
        break;
    }
    }
}

// Vertical sliding door (class 92).
// orig: L15:0x002a36d0
void qua_class92_update_l15(uint32_t m) {
    const uint8_t state = lbu(m + 0x20);
    const uint32_t pv = lwu(m + 0x78);
    if (state >= 6) return;

    auto move_step = [pv]() -> float {
        float step = fpu_mul(fpu_div(lwc1(L15_ADDR(0x001614f8u)), u2f(0x3f19999au)), lwc1(0x0015ed6cu));
        if (lw(pv + 0x20) != -1) {
            step = fpu_mul(step, 5.0f);
        }
        return step;
    };

    auto apply_z = [m, pv]() {
        float off = lwc1(pv + 0x18);
        const float base_z = lwc1(pv + 0x8);
        if (lw(pv + 0x20) != -1) {
            off = -off;
        }
        swc1(m + 0x18, fpu_add(base_z, off));
    };

    // Jump table L15:0x001e2730 (6 entries)
    switch (state) {
    case 0: {  // (0x002a371c)
        lq_sq(pv, m + 0x10);
        sb(m + 0x20, 1);
        sw(pv + 0x28, 0);
        if (lw(pv + 0x20) != -1) {
            sb(m + 0x30, 0xff);
        }
        if (lbu(0x0013dd50u) != 0 && lw(pv + 0x20) != -1) {
            sb(m + 0x20, 5);
            swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), lwc1(L15_ADDR(0x001614f8u))));
        }
        break;
    }
    case 1: {  // (0x002a376c)
        sw(pv + 0x18, 0);
        if (lw(pv + 0x20) != -1) {
            sw(L15_ADDR(0x00184aa8u), 0);
        }
        const int32_t linked_idx = lw(pv + 0x24);
        if (linked_idx != -1) {
            const uint32_t lm = lwu(L15_ADDR(0x0015ffd8u)) + (static_cast<uint32_t>(linked_idx) << 8);
            bool retired = (lm == 0);
            if (!retired) {
                const uint8_t lst = lbu(lm + 0x20);
                retired = (lst == 0xfe || lst == 0xfd);
            }
            if (retired && lw(L15_ADDR(0x00161ac0u)) == 0) {
                sb(m + 0x20, 2);
                sw(pv + 0x28, 1);
            }
        } else {
            if (point_in_box(HERO_POS, lw(pv + 0x10)) != 0) {
                if (lw(pv + 0x20) == -1 || lbu(0x001413f4u) == 2) {
                    sb(m + 0x20, 2);
                }
            }
        }
        if (lbu(0x0013dd50u) != 0 && lw(pv + 0x20) != -1) {
            sb(m + 0x20, 5);
            swc1(m + 0x18, fpu_sub(lwc1(pv + 0x8), lwc1(L15_ADDR(0x001614f8u))));
        }
        break;
    }
    case 2: {  // (0x002a384c)
        const float step = move_step();
        ensure_door_sound(m, pv + 0x1c);
        const float cur = fpu_add(lwc1(pv + 0x18), step);
        const float height = lwc1(L15_ADDR(0x001614f8u));
        swc1(pv + 0x18, cur);
        if (height < cur) {
            stop_door_sound(m, pv + 0x1c);
            sw(pv + 0x1c, 0xffffffffu);
            swc1(pv + 0x18, lwc1(L15_ADDR(0x001614f8u)));
            sb(m + 0x20, 3);
        }
        apply_z();
        break;
    }
    case 3: {  // (0x002a3960)
        if (lw(pv + 0x20) != -1) {
            sw(L15_ADDR(0x00184aa8u), 1);
        }
        bool close = false;
        if (point_in_box(HERO_POS, lw(pv + 0x14)) != 0 ||
            point_in_box(L15_ADDR(0x001673c0u), lw(pv + 0x14)) != 0) {
            if (point_in_box(HERO_POS, lw(pv + 0x20)) != 0 && lw(pv + 0x28) == 0) {
                close = true;
            }
        } else if (lw(pv + 0x28) == 0) {
            close = true;
        }
        if (close) {
            sb(m + 0x20, 4);
        }
        break;
    }
    case 4: {  // (0x002a39d8)
        const float step = move_step();
        ensure_door_sound(m, pv + 0x1c);
        const float cur = fpu_sub(lwc1(pv + 0x18), step);
        swc1(pv + 0x18, cur);
        if (cur < 0.0f) {
            stop_door_sound(m, pv + 0x1c);
            sw(pv + 0x18, 0);
            sw(pv + 0x1c, 0xffffffffu);
            sb(m + 0x20, 1);
            if (lw(pv + 0x20) != -1) {
                sb(m + 0x20, 5);
                sw(L15_ADDR(0x00184aa8u), 0);
            }
        }
        apply_z();
        if (point_in_box(HERO_POS, lw(pv + 0x10)) != 0 ||
            point_in_box(L15_ADDR(0x001673c0u), lw(pv + 0x10)) != 0) {
            sb(m + 0x20, 2);
        }
        break;
    }
    case 5: {  // (0x002a3b34)
        if (lbu(0x0013dd50u) != 0) {
            swc1(m + 0x18, fpu_sub(lwc1(pv + 0x8), lwc1(L15_ADDR(0x001614f8u))));
            sw(L15_ADDR(0x00184aa8u), 1);
        } else if (lbu(0x001413f4u) != 2) {
            sb(m + 0x20, 1);
            swc1(m + 0x18, lwc1(pv + 0x8));
            sw(L15_ADDR(0x00184aa8u), 0);
        }
        break;
    }
    }
}

// Hinged swinging door (class 93).
// orig: L15:0x002a3ba8
void qua_class93_update_l15(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    bool open = false;
    bool close = false;
    const int32_t linked_idx = lw(pv + 0x14);

    if (linked_idx == -1) {
        if (lbu(m + 0x20) != 0) {
            if (point_in_box(HERO_POS, lw(pv + 0x4)) != 0 ||
                point_in_box(L15_ADDR(0x001673c0u), lw(pv + 0x4)) != 0) {
                open = true;
            } else if (point_in_box(HERO_POS, lw(pv + 0x8)) == 0 &&
                       point_in_box(L15_ADDR(0x001673c0u), lw(pv + 0x8)) == 0) {
                close = true;
            }
            for (uint32_t node = lwu(L15_ADDR(0x0015ffe4u)); node != 0; node = lwu(node + 0x28)) {
                if (moby_pvar_word0(node) == 0) continue;
                if (point_in_box(node + 0x10, lw(pv + 0x4)) != 0) {
                    open = true;
                } else if (point_in_box(node + 0x10, lw(pv + 0x8)) == 0) {
                    close = true;
                }
            }
        }
    } else {
        const uint32_t lm = lwu(L15_ADDR(0x0015ffd8u)) + (static_cast<uint32_t>(linked_idx) << 8);
        if (lh(lm + 0xa6) == 0x49b) {
            open = (lbu(lm + 0x20) == 2);
        }
    }

    if (open) close = false;
    const uint8_t state = lbu(m + 0x20);
    if (state >= 5) return;

    auto swing_delta = []() -> float {
        const float rate = fpu_div(lwc1(L15_ADDR(0x001614fcu)), lwc1(L15_ADDR(0x00161500u)));
        return fpu_mul(fpu_mul(rate, u2f(0x3c8efa35u)), lwc1(0x0015ed6cu));
    };

    auto apply_yaw = [m, pv]() {
        float off = lwc1(pv + 0xc);
        if (lw(pv + 0x10) != 0) off = -off;
        swc1(m + 0x48, angle_add(lwc1(pv + 0x0), off));
    };

    // Jump table L15:0x001e2750 (5 entries)
    switch (state) {
    case 0: {  // (0x002a3d10)
        if (lw(pv + 0x10) != 0) {
            sh(m + 0x34, static_cast<uint16_t>(lhu(m + 0x34) | 0x8000u));
        }
        float rad = fpu_mul(lwc1(L15_ADDR(0x001614fcu)), u2f(0x3c8efa35u));
        if (lw(pv + 0x10) == 0) rad = -rad;
        const float base_yaw = angle_add(lwc1(m + 0x48), rad);
        sw(pv + 0xc, 0);
        swc1(pv + 0x0, base_yaw);
        sb(m + 0x20, 1);
        swc1(m + 0x48, base_yaw);
        break;
    }
    case 1: {  // (0x002a3d68)
        sw(pv + 0xc, 0);
        swc1(m + 0x48, lwc1(pv + 0x0));
        if (open) {
            sb(m + 0x20, 2);
            sw(pv + 0x18, static_cast<uint32_t>(moby_sound_start(0, 0, m)));
        }
        break;
    }
    case 2: {  // (0x002a3d80)
        const float cur = angle_add(lwc1(pv + 0xc), swing_delta());
        const float limit = fpu_mul(lwc1(L15_ADDR(0x001614fcu)), u2f(0x3c8efa35u));
        swc1(pv + 0xc, cur);
        if (cur < limit) {
            swc1(pv + 0xc, limit);
            sb(m + 0x20, 3);
        }
        apply_yaw();
        break;
    }
    case 3: {  // (0x002a3df4)
        if (close) {
            sb(m + 0x20, 4);
            sw(pv + 0x18, static_cast<uint32_t>(moby_sound_start(0, 0, m)));
        }
        break;
    }
    case 4: {  // (0x002a3e18)
        const float cur = angle_add(lwc1(pv + 0xc), -swing_delta());
        swc1(pv + 0xc, cur);
        if (0.0f < cur) {
            swc1(pv + 0xc, 0.0f);
            sb(m + 0x20, 1);
        }
        apply_yaw();
        if (open) {
            sb(m + 0x20, 2);
        }
        break;
    }
    }
}

}  // namespace rac1
