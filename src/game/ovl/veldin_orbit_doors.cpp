// Drek's fleet's (level 17) sliding doors, ported by hand from the level's own code: the update the level's door
// classes share (1359 .. 1373: 0x54f .. 0x55d), a panel that slides sideways along its facing (a quarter turn further
// for classes 1367, 1369, 1372 and 1373) from its place by up to 2 units when the hero, or a moby it names, comes into
// its box, and back when they and the camera have left; with its sound while it moves. The original's order of calls
// and float operations kept. Level 17 only: `orig: L17:`, the level's own data L17_ADDR, base ELF bare. Registered in
// veldin_orbit_mobys.cpp.
#include <game/ovl/veldin_orbit_mobys.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

constexpr uint32_t HERO_POS = 0x0013f3d0u;

float dt() { return lwc1(0x0015ed6cu); }

// the door's sound (its class has sounds) started again when it stopped
void door_sound(uint32_t m, uint32_t pv) {
    if (lwu(lwu(m + 0x24) + 0x28) == 0) return;
    if (sub_0028d8c0(m, lw(pv + 0x20)) != 0) return;
    sw(pv + 0x20, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
}

// the door's sound slot stopped if it still plays for the door; the slot forgotten
void door_sound_stop(uint32_t m, uint32_t pv) {
    const int32_t slot = lw(pv + 0x20);
    if (slot != -1) {
        const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot) * 0x70u;
        if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
    }
    sw(pv + 0x20, 0xffffffffu);
}

// the named moby (pvars +0x10, -1: none) inside box `box`
bool mate_in_box(uint32_t pv, int32_t box) {
    const int32_t mate = lw(pv + 0x10);
    if (mate == -1) return false;
    return point_in_box(lwu(L17_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(mate) << 8) + 0x10, box) != 0;
}

// the door placed at its start plus its slide (pvars +0x14) along its facing
void door_place(uint32_t m, uint32_t pv, uint32_t sp) {
    const uint32_t cls = lhu(m + 0xa6);
    const int16_t c = static_cast<int16_t>(cls);
    const bool turned = cls - 0x55cu < 2u || c == 0x557 || c == 0x559;
    const float a = turned ? angle_add(u2f(0x3fc90fdbu), lwc1(m + 0x48)) : lwc1(m + 0x48);
    swc1(sp + 0x0, fpu_mul(cos_vu(a), lwc1(pv + 0x14)));
    sw(sp + 0x8, 0);
    swc1(sp + 0x4, fpu_mul(sin_vu(a), lwc1(pv + 0x14)));
    vec3_add(V(sp + 0x10), V(pv), V(sp));
    lq_sq(m + 0x10, sp + 0x10);
}
}  // namespace

// The doors' update. Pvars: +0 the start, +0x10 a moby that opens it too (-1: none), +0x14 the slide, +0x18 the box
// that opens it, +0x1c the box kept clear before it closes (-1 in either: the door is retired), +0x20 its sound slot.
// +0xbc the side it slides to. States: 0 set up, 1 shut, 2 opening, 3 still, 4 open, 5 closing. Nothing moves while
// the level's word 0x15f5c4 is 2.
// orig: L17:0x002e87d8
void vo_class1359_update_l17(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(pv + 0x18) == -1 || lw(pv + 0x1c) == -1) {
        moby_retire(m);
        return;
    }
    if (lw(L17_ADDR(0x0015f5c4)) == 2) return;
    const Frame f(112);
    const uint32_t sp = f.sp;
    const uint32_t camera = L17_ADDR(0x001676c0);
    switch (lbu(m + 0x20)) {  // (the jump table L17:0x1e6de0)
    case 0: {  // (0x002e8858)
        const uint32_t cls = lhu(m + 0xa6);
        const int16_t c = static_cast<int16_t>(cls);
        if (cls - 0x550u < 2u || c == 0x553 || c == 0x557 || c == 0x55d) sb(m + 0xbc, 1);
        lq_sq(pv, m + 0x10);
        sb(m + 0x20, 1);
        return;
    }
    case 1: {  // (0x002e8a10): shut, until the hero (turned to it, or as the ship's pilot) or the mate is in the box
        door_sound_stop(m, pv);
        sw(pv + 0x14, 0);
        if (!(abs_f(lwc1(m + 0x40)) < u2f(0x3f490fdbu)) && lh(0x0013f658u) == 0) return;
        if (point_in_box(HERO_POS, lw(pv + 0x18)) == 0 && !mate_in_box(pv, lw(pv + 0x18))) return;
        door_sound(m, pv);
        sb(m + 0x20, 2);
        return;
    }
    case 2: {  // (0x002e88a8): opening
        door_sound(m, pv);
        float step = fpu_mul(dt(), 3.0f);
        if (lbu(m + 0xbc) == 0) step = -step;
        swc1(pv + 0x14, fpu_add(lwc1(pv + 0x14), step));
        door_place(m, pv, sp);
        if (2.0f < abs_f(lwc1(pv + 0x14))) {
            door_sound_stop(m, pv);
            sb(m + 0x20, 4);
        }
        return;
    }
    case 4: {  // (0x002e8b10): open, until the hero and the camera have left the second box and the mate the first
        door_sound_stop(m, pv);
        if (point_in_box(HERO_POS, lw(pv + 0x1c)) != 0) return;
        if (point_in_box(camera, lw(pv + 0x1c)) != 0) return;
        if (mate_in_box(pv, lw(pv + 0x18))) return;
        door_sound(m, pv);
        sb(m + 0x20, 5);
        return;
    }
    case 5: {  // (0x002e8bf8): closing, opening again when the hero, the camera or the mate comes into the box
        door_sound(m, pv);
        if (point_in_box(HERO_POS, lw(pv + 0x18)) != 0 || point_in_box(camera, lw(pv + 0x18)) != 0 ||
            mate_in_box(pv, lw(pv + 0x18))) {
            sb(m + 0x20, 2);
            return;
        }
        const float step = fpu_mul(dt(), 3.0f);
        if (!(step < abs_f(lwc1(pv + 0x14)))) {
            sw(pv + 0x14, 0);
        } else {
            const float slide = lwc1(pv + 0x14);
            swc1(pv + 0x14, lbu(m + 0xbc) == 0 ? fpu_sub(slide, -step) : fpu_sub(slide, step));
        }
        door_place(m, pv, sp);
        if (lwc1(pv + 0x14) == 0.0f) {
            door_sound_stop(m, pv);
            sb(m + 0x20, 1);
        }
        return;
    }
    default:  // (3: still)
        return;
    }
}

}  // namespace rac1
