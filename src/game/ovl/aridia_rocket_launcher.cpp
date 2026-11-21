// Aridia's own moby classes: the Rocket Launcher, its rocket's spawn and its sound (hand-ported from Aridia's overlay,
// `orig: L2:`). Aridia-only data is L2_ADDR; the base ELF's (the sound slots 0x13e550, the frame time 0x15ed6c) are
// bare. Declared in aridia_enemies.h, registered in aridia_mobys.cpp.
#include <game/math/vecmath.h>
#include <game/ovl/aridia_enemies.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }
float dt2() { return lwc1(0x0015ed70); }

// a volume 0 .. 1024 from a fraction (1024 above 1024, 0 below 0)
int32_t volume_of(float f) {
    int32_t v = float_to_int(fpu_mul(f, 1024.0f));
    if (v >= 1025) v = 0x400;
    else if (!(-1 < v)) v = 0;
    return v;
}
}  // namespace

// The launcher's launch sound (pvar +0x10: a wave front growing from the launch by the level's 0x161c80 a frame, -1 when
// none): when it reaches the camera, and the hero is not in state 0x72, the two layers (sounds 0 and 1) start, their
// volumes by the distance (the near one fading out over 0x161c88 .. 0x161c8c, the far one in over 0x161c90 .. 0x161c94).
// orig: L2:0x002df730
void launcher_sound_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (!(0.0f <= lwc1(pv + 0x10))) return;
    const float d = vec3_distance(V(L2_ADDR(0x001673c0)), V(m + 0x10));
    const float front = fpu_add(lwc1(pv + 0x10), fpu_mul(lwc1(L2_ADDR(0x00161c80)), dt()));
    swc1(pv + 0x10, front);
    if (!(d < front)) return;
    if (lw(0x001413d4) == 0x72) return;
    const int32_t near_slot = moby_sound_start(0, 0x10, m);
    const int32_t far_slot = moby_sound_start(1, 0x10, m);
    const float n0 = lwc1(L2_ADDR(0x00161c88)), n1 = lwc1(L2_ADDR(0x00161c8c));
    const int32_t near_vol = volume_of(fpu_sub(1.0f, fpu_div(fpu_sub(d, n0), fpu_sub(n1, n0))));
    const float f0 = lwc1(L2_ADDR(0x00161c90)), f1 = lwc1(L2_ADDR(0x00161c94));
    const int32_t far_vol = volume_of(fpu_div(fpu_sub(d, f0), fpu_sub(f1, f0)));
    sound_slot_volume_set(near_slot, static_cast<uint32_t>(near_vol));
    sound_slot_volume_set(far_slot, static_cast<uint32_t>(far_vol));
    swc1(pv + 0x10, -1.0f);
}

// The launcher's rocket (class 0x4b8, 1208) at `pos` along `dir` (EE vectors), fired by `m`: opaque, its colour the level's
// 0x161f4c .. 0x161f54, m's flags +0x34, turned along dir; its pvars: dir, a life of 60 frames (+0x10). The rocket (0: none).
// orig: L2:0x002ec228
uint32_t launcher_rocket_spawn_l2(uint32_t m, uint32_t pos, uint32_t dir) {
    const uint32_t r = moby_spawn(0x4b8);
    if (r == 0) return 0;
    sb(r + 0x30, 0xff);
    sb(r + 0x31, 1);
    sh(r + 0x32, 0xff);
    moby_set_color_hi(r, lwu(L2_ADDR(0x00161f4c)), lwu(L2_ADDR(0x00161f50)), lwu(L2_ADDR(0x00161f54)));
    sh(r + 0x34, lhu(m + 0x34));
    lq_sq(r + 0x10, pos);
    swc1(r + 0x48, atan2_f(lwc1(dir), lwc1(dir + 4)));
    swc1(r + 0x44, atan2_f(vec2_length(V(dir)), lwc1(dir + 8)));
    sb(r + 0x20, 1);
    const uint32_t rpv = lwu(r + 0x78);
    lq_sq(rpv, dir);
    sw(rpv + 0x10, static_cast<uint32_t>(frame_scale_i(0x3c)));
    return r;
}

// The Rocket Launcher: state 0 waits (pvar +8), then picks a heading (+4, 0 .. pi) and turns to it (1, its turning sound
// in the slot +0xc, its turning speed +0); facing it and still, it fires (2): the turning sound stopped, the
// launch sound, the wave front +0x10, a blast at its muzzle (joint 1) and a rocket out of it; while the firing animation
// plays its colour goes from 0x161c78 to 0x161c7c; then it waits 60 .. 180 frames again.
// orig: L2:0x002df3d8
void rocket_launcher_update_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    launcher_sound_l2(m);
    switch (lbu(m + 0x20)) {
    case 0:
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 8))) == 0) return;
        swc1(pv + 4, rand_range_f(0.0f, u2f(0x40490fd0u)));
        sb(m + 0x20, 1);
        moby_sound_start(4, 0, m);
        return;
    case 1: {
        const float deg = u2f(0x3c8efa35u);
        const float turn = fpu_mul(fpu_mul(lwc1(L2_ADDR(0x00161c70)), deg), dt2());
        yaw_seek(m + 0x48, pv, lwc1(pv + 4), turn, turn, fpu_mul(fpu_mul(lwc1(L2_ADDR(0x00161c74)), deg), dt()));
        if (sub_0028d8c0(m, lh(pv + 0xc)) == 0) sh(pv + 0xc, static_cast<uint32_t>(moby_sound_start(2, 4, m)));
        if (!(angle_dist(lwc1(m + 0x48), lwc1(pv + 4)) < deg)) return;
        if (!(lwc1(pv) == 0.0f)) return;
        const int32_t slot = lh(pv + 0xc);
        if (slot != -1) {  // its turning sound stopped when the slot still plays it
            const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot * 0x70);
            if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
        }
        sh(pv + 0xc, 0xffffu);
        moby_sound_start(3, 0, m);
        swc1(pv + 0x10, lwc1(L2_ADDR(0x00161c84)));
        sb(m + 0x20, 2);
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, 1);
        const uint32_t sp = ee_stack_alloc(0x90);
        const uint32_t dir = sp + 0x20, muzzle = sp + 0x30;
        moby_chain_point_v(m, 1, V(muzzle));
        moby_chain_point_v(m, 0, V(dir));
        vec3_sub(V(dir), V(muzzle), V(dir));
        vec3_set_length(V(dir), V(dir), fpu_mul(lwc1(L2_ADDR(0x00161c6c)), dt()));
        explosion_effect(m, dir, muzzle, 0, 6, 0x20, -1, 0, 1, -1, 0, 0.0f, 0.0f, 7.0f, 4.0f, 1.0f, 3.0f, 30.0f);
        launcher_rocket_spawn_l2(m, muzzle, dir);
        ee_stack_free(0x90);
        return;
    }
    case 2: {
        sh(m + 0x34, lhu(m + 0x34) | 0x10u);
        float t = fpu_div(moby_anim_time(m), 10.0f);
        if (1.0f < t) t = 1.0f;
        sw(m + 0x90, rgba_lerp(lwu(L2_ADDR(0x00161c78)), lwu(L2_ADDR(0x00161c7c)), t));
        if ((lbu(m + 0x70) & 2) == 0) return;
        if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 1);
        sb(m + 0x20, 0);
        sh(pv + 8, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(60.0f, 180.0f)))));
        return;
    }
    default:
        return;
    }
}

}  // namespace rac1
