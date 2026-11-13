// Class 1021: one port of the copies Blarg (L6), Drek's fleet (L17) and the finale's Veldin (L18) carry, the same code
// relinked (the normalised texts compared, the state table's entries the same offsets). Its addresses are the engine's (OV:
// the moby table 0x15ffd8) or the base ELF's (bare: the hero's position 0x13f3d0, the frame time 0x15ed6c and its square
// 0x15ed70); the level's own data is one block of the class's five tunings (per level below). Header shared_class1021.h.
//
// The pvars: the home +0 and its yaw +0x10, the opening +0x14 and its rate +0x20, the trigger moby +0x1c (an index, -1 the
// hero), the seven other petals +0x24.. (mobys).
#include <game/ovl/shared_class1021.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;
constexpr uint32_t PETAL_CLASS = 0x3fd;  // class 1021
constexpr int32_t PETALS = 7;

// The level's tunings (its own data, five words): the opening's acceleration +0, deceleration +4 and speed +8 (degrees a
// second), the hero's trigger off +0xc (0: on), the lift along the up axis +0x10.
uint32_t tunings(uint32_t o) {
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x00161dd0) + o;
    if (on_level(VELDIN_FINALE)) return L18_ADDR(0x001620ec) + o;
    return L6_ADDR(0x00161d38) + o;
}

// the opening (or closing) toward `goal` (radians), the yaw following it back from home's
void turn_toward(uint32_t m, uint32_t pv, float goal) {
    const float deg = u2f(0x3c8efa35u);
    const float accel = fpu_mul(fpu_mul(lwc1(tunings(0)), deg), lwc1(0x0015ed70));
    const float decel = fpu_mul(fpu_mul(lwc1(tunings(4)), deg), lwc1(0x0015ed70));
    const float most = fpu_mul(fpu_mul(lwc1(tunings(8)), deg), lwc1(0x0015ed6c));
    yaw_seek(pv + 0x14, pv + 0x20, goal, accel, decel, most);
    swc1(m + 0x40, angle_sub(lwc1(pv + 0x10), lwc1(pv + 0x14)));
}
}  // namespace

// The states (the jump table L6:0x201650 (L17:0x1e6c70, L18:0x1f29d0), 10 entries, by its index): 0 the flower's start
// (the seven other petals spawned, each turned another 45 degrees, in state 1; the flower 2), 1 a petal's start (3), 2 the
// flower waits for its trigger and all its petals in 3, then opens (4, its petals 5), 3 waits; 4 / 5 open to 60 degrees,
// then solid (+0x34 bit 0) and hidden (+0x31 0) in 6 / 7; 8 / 9 close, back to 2 / 3. While waiting (1 .. 3) each is lifted
// along its up axis from home.
// orig: L6:0x002f4f00 L17:0x002e2c58 L18:0x002ec3e8
void class1021_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xc0);
    // lifted from home
    auto lift = [m, pv, sp] {
        vec3_set_length(V(sp), V(m + 0xe0), lwc1(tunings(0x10)));
        vec3_add(V(m + 0x10), V(sp), V(pv));
    };
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002f4f60)
        swc1(pv + 0x10, lwc1(m + 0x40));
        lq_sq(pv, m + 0x10);
        sb(m + 0x20, 2);
        for (int32_t i = 0; i < PETALS; ++i) {
            const uint32_t slot = pv + 0x24 + static_cast<uint32_t>(i) * 4;
            sw(slot, moby_spawn(PETAL_CLASS));
            sb(lwu(slot) + 0x30, 0x40);
            sh(lwu(slot) + 0x32, 0x40);
            sb(lwu(slot) + 0x31, 1);
            sd(lwu(slot) + 0x38, ld(m + 0x38));
            sh(lwu(slot) + 0x34, lhu(m + 0x34));
            const uint32_t o = lwu(slot);
            lq_sq(o + 0x10, m + 0x10);
            lq_sq(o + 0x40, m + 0x40);
            swc1(lwu(slot) + 0x40, angle_add(lwc1(o + 0x40), fpu_mul(int_to_float((i + 1) * 0x2d), u2f(0x3c8efa35u))));
            sb(lwu(slot) + 0x20, 1);
            euler_to_mat3_vu(M(lwu(slot) + 0xc0), V(lwu(slot) + 0x40));
        }
        break;
    }
    case 1:  // (0x002f5060)
        swc1(pv + 0x10, lwc1(m + 0x40));
        lq_sq(pv, m + 0x10);
        sb(m + 0x20, 3);
        lift();
        break;
    case 2: {  // (0x002f5094)
        if (lbu(m + 0xbc) == 0) {
            bool fire = false;
            const int32_t idx = lw(pv + 0x1c);
            if (idx >= 0) {
                const uint32_t o = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8);
                const int32_t cls = lh(o + 0xa6);
                if (cls == 0x5a8) fire = vec2_distance(V(HERO_POS), V(o + 0x10)) < 20.0f;
                else if (cls == 0x24a) fire = lbu(o + 0xbc) != 0;
                else fire = lbu(o + 0x20) >= 3;
            } else if (vec2_distance(V(m + 0x10), V(HERO_POS)) < 6.0f) {
                fire = lwc1(tunings(0xc)) == 0.0f;
            }
            if (fire) {
                for (int32_t i = 0; i < PETALS; ++i)
                    if (lbu(lwu(pv + 0x24 + static_cast<uint32_t>(i) * 4) + 0x20) != 3) fire = false;
            }
            if (fire) {
                moby_sound_start(0, 0, m);
                sb(m + 0x20, 4);
                sw(pv + 0x18, 0);
                sw(pv + 0x14, 0);
                for (int32_t i = 0; i < PETALS; ++i) sb(lwu(pv + 0x24 + static_cast<uint32_t>(i) * 4) + 0x20, 5);
            }
        }
        lift();
        break;
    }
    case 3:  // (0x002f51d4)
        lift();
        break;
    case 4:
    case 5: {  // (0x002f51fc)
        const float open = u2f(0x3f860a92u);  // 60 degrees
        turn_toward(m, pv, open);
        if (open <= lwc1(pv + 0x14)) {
            sh(m + 0x34, lhu(m + 0x34) | 1u);
            sb(m + 0x31, 0);
            sb(m + 0x20, lbu(m + 0x20) == 4 ? 6 : 7);
        }
        break;
    }
    case 8:
    case 9:  // (0x002f52a0)
        turn_toward(m, pv, 0.0f);
        if (lwc1(pv + 0x14) <= 0.0f) sb(m + 0x20, lbu(m + 0x20) != 8 ? 3 : 2);
        break;
    default:  // 6, 7 (0x002f532c) and beyond the table
        break;
    }
    ee_stack_free(0xc0);
}

}  // namespace rac1
