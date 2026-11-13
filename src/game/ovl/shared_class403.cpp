// Class 403, a rocket, and its trail: one port of the copies Oltanis and Quartu carry (the same code relinked). Each level
// keeps its own three words of the rocket's flight (per level below); the rest is the engine's (OV: the paths 0x15f70c, the
// zero vector 0x15f580) or the base ELF's (bare: the hero's moby 0x1413d0, the frame scale 0x15ed60 and its square
// 0x15ed64). Header shared_class403.h.
//
// The pvars: +0 the moby that fired it (its class chooses the sounds), +4 the fuse, +8 the speed along the path, +0xc the
// path's index, +0x10 / +0x14 the place on the path, +0x18 / +0x1c the last step in the plane (the smoke's drift), +0x20 ..
// +0x28 the spin, +0x30 the velocity in free flight.
#include <game/ovl/shared_class403.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
// the level's flight words: +0 the gravity, +4 the drag, +8 the speed below which the drag stops
uint32_t flight() { return on_level(QUARTU) ? L15_ADDR(0x00161aa8) : L14_ADDR(0x00161b40); }

// the blast: the owner's sound, the explosion at `pos` (moving `vel`), a push for the hero within 1 of the rocket, gone
void blow_up(uint32_t m, int32_t sound, uint32_t vel, uint32_t pos) {
    const uint32_t pv = lwu(m + 0x78);
    moby_sound_play(sound, 0, m, static_cast<uint32_t>(lh(lwu(pv) + 0xa6)));
    explosion_effect(m, vel, pos, 5, 2, 4, -1, 0, 3, -1, 0, 0.0f, 0.0f, 1.0f, 0.5f, 9.0f, 1.0f, 15.0f);
    sphere_collide(m + 0x10, 1.0f, 0, lwu(0x001413d0));
    moby_retire(m);
}

// one axis of the free flight's velocity slowed by the drag (scaled by the frame) while faster than the level's mark
float dragged(float v, uint32_t words) {
    const float scale = lwc1(0x0015ed60);
    if (!(fpu_mul(lwc1(words + 8), scale) < abs_f(v))) return v;
    return fpu_mul(v, fpu_add(fpu_mul(fpu_sub(lwc1(words + 4), 1.0f), scale), 1.0f));
}
}  // namespace

// orig: L14:0x002e03b8 L15:0x002cb0a0
void class403_trail(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x100);
    const uint32_t pos = sp, ahead = sp + 0x10, vel = sp + 0x20, side = sp + 0x30;
    // a spark just ahead of the nose, drifting a little, and three more that stay
    vec3_set_length(V(ahead), V(m + 0xe0), 0.2f);
    vec3_add(V(pos), V(m + 0x10), V(ahead));
    rand_dir_scaled(vel, u2f(0x3ba3d70au), u2f(0x3cf5c28fu));  // (0.005 .. 0.03)
    const uint32_t first = spawn_particle_45(pos, vel, 0x7f, m);
    if (first != 0) swc1(first + 0xc, rand_range_f(6000.0f, 32000.0f));
    for (int32_t i = 0; i < 3; ++i) {
        const uint32_t p = spawn_particle_45(pos, OV(0x0015f580), 0x7f, m);
        if (p == 0) continue;
        if (i == 2 && rand_mod(8) == 0) swc1(p + 0xc, 180000.0f);
        else swc1(p + 0xc, rand_range_f(80000.0f, 120000.0f));
        const int32_t life = frame_scale_i(2);
        sh(p + 0xa, static_cast<uint32_t>(life));
        const float fade = fpu_div(1.0f, int_to_float(static_cast<int16_t>(life)));
        sh(p + 0x36, 4);
        sw(p + 0x38, 0x7f7f7f);
        swc1(p + 0x30, fade);
    }
    // the smoke: from behind the rocket (further and rising in flight), off to its side, drifting with its last step
    swc1(vel, lwc1(pv + 0x18));
    swc1(vel + 4, lwc1(pv + 0x1c));
    sw(vel + 8, 0);
    swc1(vel + 0xc, 1.0f);
    if (lbu(m + 0x20) == 0) {
        vec3_set_length(V(ahead), V(m + 0xe0), u2f(0x3f733333u));  // (0.95)
    } else {
        vec3_set_length(V(ahead), V(m + 0xe0), u2f(0x3f933333u));  // (1.15)
        swc1(vel + 8, fpu_mul(lwc1(0x0015ed60), u2f(0x3cf5c28fu)));  // (0.03)
    }
    vec3_add(V(pos), V(m + 0x10), V(ahead));
    swc1(side, fpu_mul(cos_vu(lwc1(m + 0x48)), u2f(0x3e99999au)));  // (0.3)
    swc1(side + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), u2f(0x3e99999au)));
    sw(side + 8, 0);
    vec3_add(V(pos), V(pos), V(side));
    if (lbu(m + 0x20) == 1) {
        swc1(pos, fpu_add(lwc1(pos), rand_range_signed_f(0.0f, 0.1f)));
        swc1(pos + 4, fpu_add(lwc1(pos + 4), rand_range_signed_f(0.0f, 0.1f)));
    }
    const int32_t lo = frame_scale_i(10);
    const int32_t count = rand_range(lo, frame_scale_i(20));
    spawn_particle_15_v(V(pos), V(vel), 0x4f007fff, 0x1fffffff, static_cast<uint32_t>(count), 1, 60000.0f);
    int32_t spin = rand_mod(6);
    if (rand_mod(2) != 0) spin = -spin;
    const float size = rand_range_f(60000.0f, 140000.0f);
    const uint32_t grey = static_cast<uint32_t>(rand_range(0x30, 0xff));
    const uint32_t smoke = spawn_particle_17(pos, spin, vel, grey | grey << 16 | grey << 8 | 0x60000000u, 0.025f, 1.0f, 1.0f, size);
    if (smoke != 0) {
        sb(smoke + 3, 0x44);
        sh(smoke + 0xa, static_cast<uint32_t>(frame_scale_i(0x3c)));
        sb(smoke + 0x2a, 0x60);
        sw(smoke + 0x24, 2);
        sb(smoke + 0x2b, lbu(smoke + 0xa));
    }
    ee_stack_free(0x100);
}

// State 0 rides the path (pvar +0xc) and trails, blowing up when the fuse ends; any other state flies free (the velocity
// slowed by the drag, falling, spinning), blowing up when the fuse ends or the nose meets something, gone out of the
// level's box, and trails every other frame or so.
// orig: L14:0x002dfd10 L15:0x002ca9f8
void class403_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xb0);
    const uint32_t was = sp + 0x20, step = sp + 0x30;
    if (lbu(m + 0x20) == 0) {
        const uint32_t path = lwu(lwu(OV(0x0015f70c)) + (lwu(pv + 0xc) << 5) + 0x10);
        lq_sq(was, m + 0x10);
        int32_t index = lw(pv + 0x10);
        float frac = lwc1(pv + 0x14);
        path_advance_v(path, V(m + 0x10), &index, &frac, 0, lwc1(pv + 8));
        sw(pv + 0x10, static_cast<uint32_t>(index));
        swc1(pv + 0x14, frac);
        vec3_sub(V(step), V(m + 0x10), V(was));
        swc1(pv + 0x18, lwc1(step));
        swc1(pv + 0x1c, lwc1(step + 4));
        class403_trail(m);
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 4))) != 0) {
            vec3_set_length(V(step), V(step), 0.5f);
            lq_sq(was, m + 0x10);
            swc1(was + 8, fpu_add(lwc1(was + 8), 0.25f));
            blow_up(m, 1, step, was);
        }
        ee_stack_free(0xb0);
        return;
    }
    const uint32_t words = flight();
    const uint32_t vel = pv + 0x30;
    vec3_add(V(m + 0x10), V(m + 0x10), V(vel));
    swc1(vel, dragged(lwc1(vel), words));
    swc1(vel + 4, dragged(lwc1(vel + 4), words));
    if (0.0f < lwc1(vel + 8))  // (rising: the drag on the climb too)
        swc1(vel + 8, fpu_mul(lwc1(vel + 8), fpu_add(fpu_mul(fpu_sub(lwc1(words + 4), 1.0f), lwc1(0x0015ed60)), 1.0f)));
    swc1(vel + 8, fpu_sub(lwc1(vel + 8), fpu_mul(lwc1(words), lwc1(0x0015ed64))));
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0x20)));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x24)));
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x28)));
    bool inside = true;
    for (uint32_t k = 0; k < 3; ++k) {
        const float c = lwc1(m + 0x10 + k * 4);
        if (c < 2.0f || 1021.0f < c) inside = false;
        if (!inside) break;
    }
    if (!inside) {
        moby_retire(m);
        ee_stack_free(0xb0);
        return;
    }
    vec3_set_length(V(step), V(m + 0xe0), u2f(0x3f051eb8u));  // (0.52: the nose)
    vec3_add(V(was), V(m + 0x10), V(step));
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 4))) != 0 || sphere_collide(was, 0.5f, 0, lwu(pv)) != 0) {
        blow_up(m, 0, vel, m + 0x10);
    } else if (rand_mod(2) != 0) {
        class403_trail(m);
    }
    ee_stack_free(0xb0);
}

}  // namespace rac1
