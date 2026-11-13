// Classes 224 / 228, the space levels' pickups: one port of the copies Gemlik Base and Drek's fleet carry (the same code
// relinked). Nothing of it is the level's own: the collision's moby met is the engine's (OV 0x173e58), the hero's block
// (0x13f350: his ship +0x15f0, its ammo +0x15f6 / most +0x15f7, its shield +0x15fc / most +0x1600, the hero's state +0x2084)
// and the frame time 0x15ed6c the base ELF's (bare). Header shared_class224.h.
//
// The pvars: +0 the moby whose state it waits for (an index in the moby table, -1 none) and +4 that state (-1 none), +8 the
// frames before it shows (-1: at once) and +0x10 their timer, +0xc shown from the start, +0xd back after it is taken (else
// gone), +0x14 / +0x18 its bob's offset and angle. States: 0 start, 1 / 2 wait for the moby, 3 wait the frames, 4 shown, 5
// taken (it shrinks), 6 gone or back to 1.
#include <game/ovl/shared_class224.h>

#include <game/math/vecmath.h>
#include <game/ovl/globals.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
constexpr uint32_t P = g::PLAYER;
constexpr int32_t AMMO_CLASS = 0xe0;    // (224)
constexpr int32_t SHIELD_CLASS = 0xe4;  // (228)

float class_scale(uint32_t m) { return lwc1(lwu(m + 0x24) + 0x24); }

// the hero's ships: classes 0x4da, 0x45 and 0x563
bool ship_class(int32_t cls) { return cls == 0x4da || cls == 0x45 || cls == 0x563; }

// hidden: not drawn (+0x34 bit 0), no collision (+0x94), not visible (+0x31), at a tenth of its scale
void hide(uint32_t m) {
    sw(m + 0x94, 0);
    sb(m + 0x31, 0);
    sh(m + 0x34, lhu(m + 0x34) | 1u);
    swc1(m + 0x2c, fpu_mul(class_scale(m), 0.1f));
}

// the moby's position kept inside 20 .. 1003 on each axis
void keep_inside(uint32_t m) {
    for (uint32_t axis = 0x10; axis <= 0x18; axis += 4) {
        if (lwc1(m + axis) < 20.0f) swc1(m + axis, 20.0f);
        if (1003.0f < lwc1(m + axis)) swc1(m + axis, 1003.0f);
    }
}

// a sound of the ship's (when it lives)
void ship_sound(int32_t idx, uint32_t ship) {
    if (ship == 0) return;
    const uint32_t st = lbu(ship + 0x20);
    if (st == 0xfe || st == 0xfd) return;
    moby_sound_start(idx, 0, ship);
}

// State 4: it spins, grows to its scale and is taken when a sphere of 2 about its chain point 0 meets the ship, or the ship
// (the hero flying it, state 0x32) comes within 4 of it; within 40 a visible one drifts towards the ship (0.8 over the
// square root of the distance). Taken: an ammo pickup adds 5 to the ship's ammo (at most its most), a shield pickup 10 to
// its shield (the same); the ship's sound; state 5.
void shown(uint32_t m, uint32_t ship, uint32_t at, uint32_t pull) {
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), fpu_mul(lwc1(0x0015ed6c), u2f(0x3f490fdbu))));  // (pi / 4 a second)
    keep_inside(m);
    moby_chain_point_v(m, 0, V(at));
    {
        const float sc = lwc1(m + 0x2c);
        swc1(m + 0x2c, fpu_add(sc, fpu_mul(fpu_sub(class_scale(m), sc), 0.1f)));
    }
    bool hit = false;
    if (sphere_collide(at, 2.0f, 1, m) != 0) {
        const uint32_t met = lwu(OV(0x00173e58));
        if (met != 0 && ship_class(lh(met + 0xa6))) hit = true;
    }
    float dist = 100000.0f;
    if (lw(P + 0x2084) == 0x32 && ship != 0) {
        const uint32_t st = lbu(ship + 0x20);
        if (st != 0xfe && st != 0xfd && ship_class(lh(ship + 0xa6))) dist = vec3_distance(V(ship + 0x10), V(m + 0x10));
    }
    if (hit || dist < 4.0f) {
        const int32_t cls = lh(m + 0xa6);
        sw(m + 0x94, 0);
        if (cls == AMMO_CLASS) {
            sb(P + 0x15f6, lbu(P + 0x15f6) + 5);
            ship_sound(4, lwu(P + 0x15f0));
            if (lbu(P + 0x15f7) < lbu(P + 0x15f6)) sb(P + 0x15f6, lbu(P + 0x15f7));
        } else if (cls == SHIELD_CLASS) {
            swc1(P + 0x15fc, fpu_add(lwc1(P + 0x15fc), 10.0f));
            ship_sound(5, lwu(P + 0x15f0));
            if (lwc1(P + 0x1600) < lwc1(P + 0x15fc)) swc1(P + 0x15fc, lwc1(P + 0x1600));
        }
        sb(m + 0x20, 5);
        return;
    }
    if (!(dist < 40.0f) || lbu(m + 0x31) == 0) return;
    const float step = fpu_div(0.8f, sqrt_f(dist));
    vec3_sub(V(pull), V(ship + 0x10), V(m + 0x10));
    vec3_set_length(V(pull), V(pull), step);
    vec3_add(V(m + 0x10), V(m + 0x10), V(pull));
}
}  // namespace

// orig: L13:0x002e1cc8 L17:0x002c53d8
void class224_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t ship = lwu(P + 0x15f0);
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t at = sp, pull = sp + 0x10;
    switch (lbu(m + 0x20)) {  // (the level's state table: L13 0x1f4cc0, each state its own case)
    case 0:  // (0x002e1d38)
        if (lbu(pv + 0xc) != 0) {
            sb(m + 0x20, 4);
            swc1(m + 0x2c, class_scale(m));
        } else {
            hide(m);
            sb(m + 0x20, 1);
        }
        break;
    case 1:  // (0x002e1d8c)
        sb(m + 0x20, lw(pv) == -1 ? 3 : 2);
        break;
    case 2: {  // (0x002e1da4)
        const uint32_t other = lwu(OV(0x0015ffd8)) + (lwu(pv) << 8);
        bool done = true;
        if (other != 0) {
            const uint32_t st = lbu(other + 0x20);
            if (st != 0xfe && st != 0xfd) {
                const int32_t want = lw(pv + 4);
                done = want == -1 || static_cast<int32_t>(lbu(other + 0x20)) == want;
            }
        }
        if (done) sb(m + 0x20, 3);
        break;
    }
    case 3:  // (0x002e1df8)
        if (lw(pv + 8) == -1 || timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x10))) != 0) sb(m + 0x20, 4);
        if (lbu(m + 0x20) == 4) {
            sb(m + 0x31, 1);
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
            sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
            swc1(m + 0x2c, fpu_mul(class_scale(m), 0.1f));
        }
        break;
    case 4:  // (0x002e1e68)
        shown(m, ship, at, pull);
        break;
    case 5: {  // (0x002e2184)
        const float sc = lwc1(m + 0x2c);
        const float spin = fpu_sub(class_scale(m), sc);
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), fpu_add(spin, spin)));
        const float now = fpu_add(sc, fpu_mul(fpu_sub(fpu_mul(class_scale(m), 0.01f), sc), 0.3f));
        swc1(m + 0x2c, now);
        if (now < fpu_mul(class_scale(m), 0.05f)) sb(m + 0x20, 6);
        break;
    }
    case 6:  // (0x002e2208)
        if (lbu(pv + 0xd) == 0) {
            moby_retire(m);
            ee_stack_free(0x90);
            return;
        }
        hide(m);
        sw(pv + 0x10, lwu(pv + 8));
        sb(m + 0x20, 1);
        break;
    default:
        break;
    }
    sub_00262b00(m, pv + 0x18, pv + 0x14, 0.7f, fpu_mul(lwc1(0x0015ed6c), u2f(0x3f9c61aau)));  // (70 degrees a second)
    keep_inside(m);
    ee_stack_free(0x90);
}

}  // namespace rac1
