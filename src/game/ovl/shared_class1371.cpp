// Class 1371, a piece knocked off something, with its spawn and throw: one port of the copies Hoven and Quartu carry
// (the same code relinked; compared with the relocated fields masked, and normalised). They name only the base ELF's words
// (bare: the frame time 0x15ed6c and its square 0x15ed70). Header shared_class1371.h.
//
// Its pvars: +0x10 the address of its physics record (+0x20: move_state_start's record, the jump's tuning +0x30..+0x48).
#include <game/ovl/shared_class1371.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapons.h>

namespace rac1 {

// While its animation set is 1 (thrown): the enemies' physics on its record, it tumbles (250 degrees a second about x
// and y) and is drawn again (flag 0x1000 off); landed or against something (the physics' bits 0x61) it bursts (a small
// burst 1 up) and leaves; below a height of 5 it leaves.
// orig: L12:0x00308918 L15:0x002ea5e0
void class1371_update(uint32_t m) {
    const uint32_t set = lbu(m + 0x53);
    if (set != 1) return;
    const uint32_t rec = lwu(m + 0x78) + 0x20;
    const uint32_t hit = enemy_physics(m, rec);
    const float rate = u2f(0x408ba058u);
    const float x = angle_add(lwc1(m + 0x40), fpu_mul(lwc1(0x0015ed6c), rate));
    const float y_rate = fpu_mul(lwc1(0x0015ed6c), rate);
    swc1(m + 0x40, x);
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), y_rate));
    sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
    if ((hit & 0x61u) != 0) {
        const uint32_t sp = ee_stack_alloc(0xc0);
        const uint32_t at = sp + 0x20;
        lq_sq(at, m + 0x10);
        swc1(at + 8, fpu_add(lwc1(at + 8), 1.0f));
        explosion_effect(m, rec, at, 5, 2, 4, 0, 1, static_cast<int32_t>(set), -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 9.0f, 1.0f, 15.0f);
        moby_retire(m);
        ee_stack_free(0xc0);
        return;
    }
    if (lwc1(m + 0x18) < 5.0f) moby_retire(m);
}

// orig: L12:0x00308670 L15:0x002ea338
uint32_t class1371_spawn(uint32_t owner) {
    const uint32_t r = moby_spawn(0x55b);
    if (r == 0) return 0;
    sb(r + 0x30, 0xff);
    sb(r + 0x31, 1);
    sh(r + 0x32, 0x7f);
    sb(r + 0x20, 0);
    const uint32_t pv = lwu(r + 0x78);
    lq_sq(r + 0x10, owner + 0x10);
    sw(r + 0xb8, owner);
    lq_sq(r + 0x40, owner + 0x40);
    mem_clear128(pv, 0x20);
    sw(pv + 0x10, pv + 0x20);
    moby_update_transform(r);
    return r;
}

// The jump: up at 40 (frame-squared) and its speed from it, back from the moby's turn; a sound (1) one time in three.
// orig: L12:0x00308708 L15:0x002ea3d0
void class1371_throw(uint32_t /*unused*/, uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (rand_mod(3) == 0) moby_sound_start(1, 0x20, m);
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t dir = sp, yaw = sp + 0x10;
    swc1(pv + 0x48, u2f(0x3f333333u));
    sw(pv + 0x40, static_cast<uint32_t>(float_to_int(u2f(0x44333333u))));
    sw(pv + 0x6c, 0);
    sb(m + 0x30, 0xff);
    sh(m + 0x34, lhu(m + 0x34) & 0xfff9u);
    const float back = angle_add(lwc1(m + 0x48), u2f(0x40490fdbu));
    swc1(yaw, back);
    swc1(dir, cos_vu(back));
    swc1(dir + 4, sin_vu(lwc1(yaw)));
    const float up = 40.0f, ahead = 100.0f, four = 4.0f;
    {
        const float dt2 = lwc1(0x0015ed70);
        const float g = fpu_mul(dt2, up);
        const float drag = fpu_mul(dt2, ahead);
        swc1(dir + 8, 1.0f);
        swc1(dir + 0xc, u2f(0x45afdf66u));
        swc1(pv + 0x30, g);
        swc1(pv + 0x34, drag);
    }
    float rise = sqrt_f(fpu_mul(fpu_mul(lwc1(0x0015ed70), up), four));
    float rise2 = sqrt_f(fpu_mul(fpu_mul(lwc1(0x0015ed70), up), four));
    {
        const float dt2 = lwc1(0x0015ed70);
        rise = fpu_add(rise, rise);
        rise2 = fpu_add(rise2, rise2);
        const float g = fpu_mul(dt2, up);
        const float drag = fpu_mul(fpu_mul(dt2, ahead), 0.5f);
        rise = fpu_div(rise, g);
        rise2 = fpu_div(rise2, g);
        swc1(pv + 0x38, fpu_add(fpu_div(3.0f, rise), fpu_mul(drag, rise2)));
        swc1(pv + 0x3c, sqrt_f(fpu_mul(g, four)));
    }
    sb(pv + 0x5d, 0);
    sw(pv + 0x44, 1);
    vec_yaw_marked(dir, yaw, pv + 0x38, pv + 0x3c);
    move_state_start(m, pv + 0x20, 1, 5, 2, lwc1(yaw));
    const float twice = fpu_add(lwc1(0x0015ed6c), lwc1(0x0015ed6c));
    swc1(pv + 0x74, -1.0f);
    swc1(pv + 0x70, -1.0f);
    swc1(pv + 0x6c, twice);
    sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
    moby_anim_set(m, 1, 0, 0);
    ee_stack_free(0x80);
}

}  // namespace rac1
