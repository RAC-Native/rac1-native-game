// Class 325, a burning shell, and its glow: one port of the copies Oltanis and Quartu carry of the update and Novalis,
// Oltanis and Quartu of the glow's draw (the same code relinked). Each level keeps its own words (the shell's flight, the
// glow's two offsets and its corners' template: per level below); the rest is the engine's (OV: the zero vector 0x15f580,
// the camera 0x166dc0) or the base ELF's (bare: the hero's moby 0x1413d0, the frame scale 0x15ed60 and its square
// 0x15ed64, the view's up axis 0x13f5e0). Header shared_class325.h.
//
// The pvars: +0 the velocity, +0x10 .. +0x18 the spin, +0x20 the fuse, +0x24 the moby that fired it (not hit by its nose).
#include <game/ovl/shared_class325.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
// the level's flight words: +0 the gravity, +4 the drag, +8 the speed below which the drag stops
uint32_t words() {
    if (on_level(QUARTU)) return L15_ADDR(0x00161a80);
    return L14_ADDR(0x00161b18);
}
// the glow's offsets (+0 along the moby's x axis, +8 along its z axis)
uint32_t glow_offsets() {
    if (on_level(NOVALIS)) return L1_ADDR(0x00162064);
    if (on_level(QUARTU)) return L15_ADDR(0x00161a8c);
    return L14_ADDR(0x00161b24);
}
// the glow's four corners' template (four points)
uint32_t glow_corners() {
    if (on_level(NOVALIS)) return L1_ADDR(0x0020b9d0);
    if (on_level(QUARTU)) return L15_ADDR(0x001e2fa0);
    return L14_ADDR(0x001fc650);
}
// the level's copy of the glow's draw callback (queued by the update: the levels still register their copies)
uint32_t draw_callback() { return on_level(QUARTU) ? L15_ADDR(0x002ca810) : L14_ADDR(0x002dec40); }

// one axis of the velocity slowed by the drag (scaled by the frame) while faster than the level's mark
float dragged(float v, uint32_t w) {
    const float scale = lwc1(0x0015ed60);
    if (!(fpu_mul(lwc1(w + 8), scale) < abs_f(v))) return v;
    return fpu_mul(v, fpu_add(fpu_mul(fpu_sub(lwc1(w + 4), 1.0f), scale), 1.0f));
}
}  // namespace

// Each frame it moves (slowed by the drag, falling, spinning), gone out of the level's box; then it blows up when the fuse
// ends or its body meets something, else trails smoke. Its glow is queued every other frame or so.
// orig: L14:0x002de670 L15:0x002ca240
void class325_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (rand_mod(2) != 0) draw_callback_add(draw_callback(), m);
    const uint32_t w = words();
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
    swc1(pv, dragged(lwc1(pv), w));
    swc1(pv + 4, dragged(lwc1(pv + 4), w));
    if (0.0f < lwc1(pv + 8))  // (rising: the drag on the climb too)
        swc1(pv + 8, fpu_mul(lwc1(pv + 8), fpu_add(fpu_mul(fpu_sub(lwc1(w + 4), 1.0f), lwc1(0x0015ed60)), 1.0f)));
    swc1(pv + 8, fpu_sub(lwc1(pv + 8), fpu_mul(lwc1(w), lwc1(0x0015ed64))));
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0x10)));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x14)));
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x18)));
    for (uint32_t k = 0; k < 3; ++k) {
        const float c = lwc1(m + 0x10 + k * 4);
        if (c < 2.0f || 1021.0f < c) {
            moby_retire(m);
            return;
        }
    }
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x20))) != 0 || sphere_collide(m + 0x10, 0.5f, 0, lwu(pv + 0x24)) != 0) {
        explosion_effect(m, pv, m + 0x10, 5, 2, 4, 0, 1, 5, -1, 0, 0.0f, 0.0f, 1.0f, 0.5f, 9.0f, 1.0f, 15.0f);
        sphere_collide(m + 0x10, 1.0f, 0, lwu(0x001413d0));
        moby_retire(m);
        return;
    }
    const int32_t lo = frame_scale_i(10);
    const int32_t count = rand_range(lo, frame_scale_i(20));
    spawn_particle_15_v(V(m + 0x10), V(OV(0x0015f580)), 0x4f007fff, 0x1fffffff, static_cast<uint32_t>(count), 1, 60000.0f);
    for (int32_t i = 0; i < 2; ++i) {
        int32_t spin = rand_mod(6);
        if (rand_mod(2) != 0) spin = -spin;
        const float size = rand_range_f(40000.0f, 100000.0f);
        const uint32_t grey = static_cast<uint32_t>(rand_range(0x30, 0xff));
        const uint32_t smoke = spawn_particle_17(m + 0x10, spin, OV(0x0015f580), grey | grey << 16 | grey << 8 | 0x60000000u, 0.025f, 1.0f, 1.0f, size);
        if (smoke == 0) continue;
        sb(smoke + 3, 0x44);
        sh(smoke + 0xa, static_cast<uint32_t>(frame_scale_i(0x3c)));
        sw(smoke + 0x24, 2);
        sb(smoke + 0x2a, 0x60);
        sb(smoke + 0x2b, lbu(smoke + 0xa));
    }
}

// One quad about a point off the moby (along its x and z axes by the level's offsets), turned to the camera: the corners'
// template scaled by 0.4 in a frame whose first axis points at the camera (the view's up axis making the other two), the
// effect texture 11 in a dim grey, blended additively.
// orig: L1:0x0030bfa8 L14:0x002dec40 L15:0x002ca810
void class325_draw(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x190);
    const uint32_t quad = sp, colours = sp + 0x40, uvs = sp + 0x50, regs = sp + 0x70, corners = sp + 0x90;
    const uint32_t frame = sp + 0xd0, centre = sp + 0x100, off_z = sp + 0x110, off_x = sp + 0x120;
    const uint32_t tmpl = glow_corners();
    for (uint32_t k = 0; k < 4; ++k) lq_sq(corners + k * 0x10, tmpl + k * 0x10);
    const uint64_t tex0 = effect_tex0(0xb);
    sd(regs + 0x10, 0x0000ff9000000260ull);
    sd(regs + 8, tex0);
    sd(regs, 5);
    sd(regs + 0x18, 0x8000000048ull);
    // the corners' (s, t): (0, 0), (0, 1), (1, 0), (1, 1)
    sw(uvs, 0);
    sw(uvs + 4, 0);
    sw(uvs + 8, 0);
    swc1(uvs + 0xc, 1.0f);
    swc1(uvs + 0x10, 1.0f);
    sw(uvs + 0x14, 0);
    swc1(uvs + 0x18, 1.0f);
    swc1(uvs + 0x1c, 1.0f);
    const uint32_t offsets = glow_offsets();
    vec3_scale(V(off_x), V(m + 0xc0), lwc1(offsets));
    vec3_scale(V(off_z), V(m + 0xe0), lwc1(offsets + 8));
    vec3_add(V(off_x), V(off_x), V(off_z));
    vec3_add(V(centre), V(off_x), V(m + 0x10));
    vec3_sub(V(frame), V(OV(0x00166dc0)), V(centre));
    vec3_set_length(V(frame), V(frame), 1.0f);
    vec3_cross_ba(V(frame + 0x10), V(frame), V(0x0013f5e0));
    vec3_set_length(V(frame + 0x10), V(frame + 0x10), -1.0f);
    vec3_cross_ba(V(frame + 0x20), V(frame + 0x10), V(frame));
    for (uint32_t k = 0; k < 4; ++k) sw(colours + k * 4, 0x202020ffu);
    for (uint32_t k = 0; k < 4; ++k) {
        vec3_scale(V(quad + k * 0x10), V(corners + k * 0x10), u2f(0x3ecccccdu));  // (0.4)
        mat4_transform(V(quad + k * 0x10), V(quad + k * 0x10), M(frame));
    }
    quad_draw(quad, 0);
    ee_stack_free(0x190);
}

}  // namespace rac1
