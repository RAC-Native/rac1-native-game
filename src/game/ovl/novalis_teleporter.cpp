// Novalis's own moby classes, part 2c: the teleporter pads (class 1135). Header novalis_mobys2.h. Addresses are Novalis's
// (`orig: L1:`); Novalis-only data is L1_ADDR, the globals the
// engine shares with Veldin are the level's own (OV), the base ELF's are bare. The group state and the pieces' update
// serve every level's copy of them (their levels' own words from per-level tables).
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/variants_l8.h>
#include <game/port_stubs.h>

namespace rac1 {

// Sets the state byte (+0x20) of every moby of group `group` (the list 0x1ab840 / Novalis 0x1abcc0) that is alive
// (the byte not negative) to `state`. Its copies on other levels are the same code (the engine's data only).
// orig: L1:0x0026e0e0 L5:0x00283498 L6:0x00267f70 L7:0x00281c30 L8:0x00263730 L11:0x0027f0e0 L12:0x002720d0 L13:0x00265050 L15:0x00248ac8 L16:0x002549d8 L17:0x00253970 L18:0x0025b4a0
void moby_group_set_state(int32_t group, uint32_t state) {
    uint32_t p = lwu(OV(0x001ab840) + (static_cast<uint32_t>(group) << 2));
    if (p == 0) return;
    for (;; p += 2) {
        const uint32_t mm = lwu(OV(0x0015ffd8)) + ((lhu(p) & 0x7fffu) << 8);
        if (static_cast<int8_t>(lbu(mm + 0x20)) >= 0) sb(mm + 0x20, state);
        if (lh(p) < 0) break;
    }
}

// The pad stepped on: its bit (the moby's index +0xb2) is set in the level's collected table (0x14c190 + level * 0x100)
// and in the saved one (0x1ba950 / Veldin 0x1ba4d0); the group in +0x21 (none: 0xff) goes to state 1, else the pad
// itself.
// orig: L1:0x00308b28
void teleporter_trigger_l1(uint32_t m) {
    const uint32_t level = static_cast<uint32_t>(lw(0x0015ed84));
    uint32_t idx = lhu(m + 0xb2);
    uint32_t a = (static_cast<uint32_t>(static_cast<int32_t>(idx << 16) >> 21) << 2) + (level << 8) + 0x0014c190;
    sw(a, lwu(a) | (1u << (idx & 0x1f)));
    idx = lhu(m + 0xb2);
    a = (static_cast<uint32_t>(static_cast<int32_t>(idx << 16) >> 21) << 2) + OV(0x001ba4d0);
    sw(a, lwu(a) | (1u << (idx & 0x1f)));
    const uint32_t g = lbu(m + 0x21);
    if (g == 0xff) {
        sb(m + 0x20, 1);
        return;
    }
    moby_group_set_state(static_cast<int32_t>(g), 1);
}

namespace {
// the pieces' height above the pad and their turn (degrees): two words of each level's own small data
uint32_t pieces_height_word() {
    if (on_level(NOVALIS)) return L1_ADDR(0x00161f78);
    if (on_level(UMBRIS)) return L7_ADDR(0x00161c28);
    if (on_level(BATALIA)) return L8_ADDR(0x00162258);
    if (on_level(POKITARU)) return L11_ADDR(0x00161900);
    if (on_level(HOVEN)) return L12_ADDR(0x00161d48);
    if (on_level(GEMLIK)) return L13_ADDR(0x00161da0);
    if (on_level(KALEBO)) return L16_ADDR(0x00161ce0);
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x00161ea8);
    return L18_ADDR(0x00162178);
}
uint32_t pieces_turn_word() {
    if (on_level(NOVALIS)) return L1_ADDR(0x00161f80);
    if (on_level(UMBRIS)) return L7_ADDR(0x00161c30);
    if (on_level(BATALIA)) return L8_ADDR(0x00162260);
    if (on_level(POKITARU)) return L11_ADDR(0x00161908);
    if (on_level(HOVEN)) return L12_ADDR(0x00161d50);
    if (on_level(GEMLIK)) return L13_ADDR(0x00161da8);
    if (on_level(KALEBO)) return L16_ADDR(0x00161ce8);
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x00161eb0);
    return L18_ADDR(0x00162180);
}
}  // namespace

// The pad's three pieces (pvar +0x14.. the mobys of class 0x13b): each at the pad's position, up by the level's height word
// (Novalis 0x161f78), turned about z by its turn word (0x161f80) degrees, pushed out along its own forward axis by
// 2.4 * (pvar +0x20) - 1.2, then tilted by -1.05 * (pvar +0x24) - 0.349 (rotation matrices from the angles), their
// transforms updated. The same code on every level with teleporter pads.
// orig: L1:0x003092d0 L7:0x0031cc20 L8:0x003049a0 L11:0x002f2b70 L12:0x003013e0 L13:0x00304f80 L16:0x002e0818 L17:0x002e5628 L18:0x002ee270
void teleporter_pieces_update(uint32_t m) {
    const uint32_t buf = ee_stack_alloc(0x10);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t from = m + 0x10;
    const float f25 = u2f(0x3c8efa35u), f24 = u2f(0x4019999au), f23 = u2f(0x3f99999au), f22 = u2f(0xbf860a92u), f21 = u2f(0x3eb2b8c2u);
    uint32_t s0 = pv + 0x14;
    for (int32_t s2 = 2; s2 >= 0; --s2, s0 += 4) {
        uint32_t part = lwu(s0);
        lq_sq(part + 0x10, from);
        const float f2 = lwc1(pieces_height_word());
        swc1(part + 0x18, lwc1(part + 0x18) + f2);
        const float f0 = lwc1(pieces_turn_word()) * f25;
        part = lwu(s0);
        swc1(part + 0x44, f0);
        float f20 = lwc1(pv + 0x20) * f24;
        part = lwu(s0);
        f20 = f20 - f23;
        euler_to_mat3_vu(M(part + 0xc0), V(part + 0x40));
        part = lwu(s0);
        vec3_set_length(V(buf), V(part + 0xc0), f20);
        part = lwu(s0);
        vec3_add(V(part + 0x10), V(part + 0x10), V(buf));
        const float g = lwc1(pv + 0x24) * f22 - f21;
        part = lwu(s0);
        swc1(part + 0x44, g);
        part = lwu(s0);
        euler_to_mat3_vu(M(part + 0xc0), V(part + 0x40));
        part = lwu(s0);
        moby_update_transform(part);
    }
    ee_stack_free(0x10);
}

// The hint a pad shows when the hero stands on it (hud_message_request(4, id)): the level's table (selected by the
// level number 1..18; the levels without one show nothing) indexed by the pad's pvar +0x10.
// orig: L1:0x00309430
void teleporter_hint_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t k = static_cast<uint32_t>(lw(0x0015ed84)) - 1u;
    if (!(k < 0x12)) return;
    uint32_t tab = 0;
    switch (k) {
    case 0: case 6: case 12: tab = L1_ADDR(0x00201600u); break;
    case 7: tab = L1_ADDR(0x00166c00u - 0x4c68u); break;
    case 11: tab = L1_ADDR(0x00166c00u - 0x4c60u); break;
    case 15: tab = L1_ADDR(0x00166c00u - 0x4c58u); break;
    case 16: tab = L1_ADDR(0x00201618u); break;
    case 17: tab = L1_ADDR(0x00166c00u - 0x4c50u); break;
    default: return;
    }
    hud_message_request(4, lw(tab + (lwu(pv + 0x10) << 2)));
}

// A pad hidden: it and its three pieces flagged +0x34 |= 1, +0x31 = 0 (the pad's +0x94 cleared).
// orig: L1:0x003097e8
void teleporter_hide_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    sb(m + 0x31, 0);
    sh(m + 0x34, lhu(m + 0x34) | 1u);
    sw(m + 0x94, 0);
    uint32_t a1 = pv + 0x14;
    for (int32_t a2 = 2; a2 >= 0; --a2, a1 += 4) {
        const uint32_t p = lwu(a1);
        if (p != 0) {
            sh(p + 0x34, lhu(p + 0x34) | 1u);
            sb(lwu(a1) + 0x31, 0);
        }
    }
}

// A pad shown: its +0x94 from its class's +0x10, +0x31 = 1, the flag +0x34 bit 0 cleared on it and its pieces.
// orig: L1:0x00309838
void teleporter_show_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
    sb(m + 0x31, 1);
    sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
    uint32_t a1 = pv + 0x14;
    for (int32_t a3 = 2; a3 >= 0; --a3, a1 += 4) {
        const uint32_t p = lwu(a1);
        if (p != 0) {
            sh(p + 0x34, lhu(p + 0x34) & 0xfffeu);
            sb(lwu(a1) + 0x31, 1);
        }
    }
}

// Class 1135, a teleporter pad: it is shown (3 pieces of class 0x13b spawned at its first update) until it has been
// used (its bit in the level's tables, or the flag pvar +0x3c), its states 0 = start, 1 = waiting (the hero on it,
// Circle: moves the hero's camera, the pieces spring open: 2), 2 / 7 = opening, 3 = the count to the jump (the hero
// placed at the destination pad pvar +0, its state 4), 4 / 8 = closing, 5 = nothing, 6 = group start.
// orig: L1:0x00308bd8
void teleporter_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    uint32_t s4 = 0;
    const int32_t idx = static_cast<int16_t>(lhu(m + 0xb2));
    const uint32_t idx16 = lhu(m + 0xb2);
    if (lbu(OV(0x001bb230) + static_cast<uint32_t>(idx) + 0x454) != 0) {
        s4 = 1;
    } else {
        const uint32_t level = static_cast<uint32_t>(lw(0x0015ed84));
        if (((lwu(0x0014c190 + (level << 8) + (static_cast<uint32_t>(idx >> 5) << 2)) >> (idx16 & 0x1f)) & 1u) != 0) s4 = 1;
    }
    if (lbu(m + 0x20) != 0) {
        s4 = s4 != 0 ? 1u : 0u;
        const int32_t a1 = lw(pv + 4);
        if (a1 != -1 && s4 == 0) {
            if (point_in_box(0x0013f3d0, a1) != 0) {
                s4 = 1;
                teleporter_trigger_l1(m);
                teleporter_show_l1(m);
                if (lbu(m + 0x20) == 5) sb(m + 0x20, s4);
            }
        }
    } else {
        s4 = s4 != 0 ? 1u : 0u;
    }
    if (lw(pv + 0x3c) != 0) {
        if (lw(0x0015ee20) == 0) {
            sh(m + 0x34, lhu(m + 0x34) | 0x41u);
            sw(m + 0x94, 0);
            return;
        }
        const uint32_t cls = lwu(m + 0x24);
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
        sw(m + 0x94, lwu(cls + 0x10));
    }
    const uint32_t P = 0x0013f350;
    // one step of a pair of the pad's springs: the position at `pos`, its velocity at `vel`, towards `target`
    auto seek = [&](uint32_t pos, uint32_t vel, float target, float ft) {
        const float f13 = lwc1(L1_ADDR(0x00161f88)) * ft;
        const float f14 = lwc1(L1_ADDR(0x00161f8c)) * ft;
        const float f15 = lwc1(L1_ADDR(0x00161f90)) * lwc1(0x0015ed6c);
        seek_1d_v(*static_cast<float*>(ee_ram_ptr(pos)), *static_cast<float*>(ee_ram_ptr(vel)), target, f13, f14, f15);
    };
    // the state 1 body (also the end of state 6)
    auto state_1 = [&] {
        const int32_t v1 = lw(pv + 8);
        if (!(v1 < 0)) {
            const uint32_t level = static_cast<uint32_t>(lw(0x0015ed84));
            if (lbu(0x0014c050 + static_cast<uint32_t>(v1) + (level << 4)) != 0xff) return;
        }
        if (lw(pv) == -1) return;
        if (lbu(P + 0x20a4) == 2) return;
        if (lwu(P + 0x2fc) != m) return;
        if (lh(P + 0x30e) != 0) return;
        teleporter_hint_l1(m);
        if ((lwu(0x0013cae4) & 0x10u) == 0) return;
        moby_sound_start(0, 0, m);
        sb(m + 0x20, 2);
        sw(pv + 0x30, 0xffffffffu);
        sw(pv + 0x28, 0);
        sw(pv + 0x2c, 0);
        hero_cam_target_set(m + 0x10, lwc1(m + 0x48), 0);
        moby_sphere_contact(m, m + 0x10, 0x10000, 0, 1, 0, u2f(0x3fc00000u), u2f(0x41a00000u), 1.0f);
    };
    const uint32_t st = lbu(m + 0x20);
    if (st >= 9) return;
    switch (st) {
    case 0: {
        sb(m + 0x30, 0xff);
        const float f20 = u2f(0x40060a92u);
        uint32_t s0 = pv + 0x14;
        for (int32_t s3 = 0; s3 < 3;) {
            sw(s0, moby_spawn(0x13b));
            sh(lwu(s0) + 0x32, 0x40);
            sb(lwu(s0) + 0x31, 1);
            sd(lwu(s0) + 0x38, ld(lwu(P + 0x2080) + 0x38));
            sh(lwu(s0) + 0x34, lhu(m + 0x34));
            const uint32_t a1 = lwu(s0);
            lq_sq(a1 + 0x10, m + 0x10);
            lq_sq(a1 + 0x40, m + 0x40);
            const float f13 = static_cast<float>(s3) * f20;
            ++s3;
            const float f0 = angle_add(lwc1(a1 + 0x48), f13);
            swc1(lwu(s0) + 0x48, f0);
            s0 += 4;
        }
        if (lw(pv + 0x38) != 0 || s4 != 0) {
            sb(m + 0x20, 6);
        } else {
            teleporter_hide_l1(m);
            sb(m + 0x20, 5);
        }
        teleporter_pieces_update(m);
        return;
    }
    case 6:
        if (lbu(m + 0x21) != 0xff) moby_group_set_state(static_cast<int32_t>(lbu(m + 0x21)), 1);
        sb(m + 0x20, 1);
        [[fallthrough]];
    case 1:
        state_1();
        return;
    case 2: case 7: {
        const float one = u2f(0x3f800000u);
        int32_t v1 = 0;
        if (st == 2) v1 = sub_00233f38();
        if (lwc1(pv + 0x20) < one) {
            seek(pv + 0x20, pv + 0x28, one, lwc1(0x0015ed70));
        } else if (lwc1(pv + 0x24) < one) {
            seek(pv + 0x24, pv + 0x2c, one, lwc1(0x0015ed70));
        } else if (st == 7) {
            moby_sound_start(1, 0, m);
            sb(m + 0x20, 8);
            sw(pv + 0x34, 0);
        } else {
            const uint32_t v0 = lwu(OV(0x0015f594));
            if (v0 == 4 && u2f(0x3e800000u) < lwc1(pv + 0x20)) {
                // (nothing: the quarter-open pad keeps waiting)
            } else if (v1 == 2) {
                swc1(pv + 0x34, u2f(0x3ff9999au));
                sb(m + 0x20, 3);
            } else if (v1 == 0) {
                sb(m + 0x20, 4);
            }
        }
        teleporter_pieces_update(m);
        return;
    }
    case 3: {
        if (lwc1(pv + 0x34) != 0.0f) {
            sub_001f93e0(L1_ADDR(0x003094f0), m);
            approach_f_v(*static_cast<float*>(ee_ram_ptr(pv + 0x34)), 0.0f, lwc1(0x0015ed6c) * u2f(0x40800000u));
            return;
        }
        const uint32_t s0 = lwu(OV(0x0015ffd8)) + (lwu(pv) << 8);
        const uint32_t s3 = lwu(s0 + 0x78);
        const uint32_t buf = ee_stack_alloc(0x10);
        lq_sq(buf, s0 + 0x10);
        const float z = lwc1(buf + 8) + u2f(0x3e4ccccdu);
        swc1(buf + 8, z);
        hero_place(buf, s0 + 0x40, 0, 1);
        ee_stack_free(0x10);
        sb(0x001413f5, 1);
        uint32_t v0 = 4;
        if (lw(pv + 0x40) != -1 && lw(pv + 0x44) != -1) {
            music_track_request(lw(pv + 0x40), lwu(pv + 0x44));  // (the music track pair: L1:0x0027a248)
            v0 = 4;
        }
        sb(s0 + 0x20, v0);
        sw(s3 + 0x34, 0);
        sw(s3 + 0x28, 0);
        sw(s3 + 0x2c, 0);
        swc1(s3 + 0x24, 1.0f);
        swc1(s3 + 0x20, 1.0f);
        moby_sound_start(1, 0, s0);
        sb(m + 0x20, 1);
        sw(pv + 0x20, 0);
        sw(pv + 0x24, 0);
        sw(pv + 0x28, 0);
        sw(pv + 0x2c, 0);
        swc1(pv + 0x34, u2f(0x3ff9999au));
        teleporter_pieces_update(m);
        return;
    }
    case 4: case 8: {
        const float f20 = u2f(0x3ff9999au);
        if (lwc1(pv + 0x34) < f20) {
            sub_001f93e0(L1_ADDR(0x003094f0), m);
            approach_f_v(*static_cast<float*>(ee_ram_ptr(pv + 0x34)), f20, lwc1(0x0015ed6c) * u2f(0x40800000u));
            teleporter_pieces_update(m);
        } else if (0.0f < lwc1(pv + 0x24)) {
            seek(pv + 0x24, pv + 0x28, 0.0f, lwc1(0x0015ed70));
            teleporter_pieces_update(m);
        } else if (0.0f < lwc1(pv + 0x20)) {
            seek(pv + 0x20, pv + 0x2c, 0.0f, lwc1(0x0015ed70));
            teleporter_pieces_update(m);
        } else {
            sb(m + 0x20, 1);
            teleporter_pieces_update(m);
        }
        return;
    }
    default:
        return;
    }
}

}  // namespace rac1
