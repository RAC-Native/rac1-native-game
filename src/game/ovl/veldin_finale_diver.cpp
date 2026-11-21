// Veldin's finale (level 18) class 1355: a flyer with a trail that roams above its home spot, dives to the ground now and
// then and blows up there, to come back down from the sky. Set up (state 0) it takes its home on the ground under where
// it stands and a first point to fly to, and waits (state 1) until the level starts it (state 2): then it flies at the
// level's speed toward its point (turning, climbing to the point's height), choosing a new one about its home when it is
// there or its time is up, and one far off when it comes within 4.5 of the moby its +0x16c names; one frame in the
// level's L18:0x162234 it dives (state 3) along its heading, bending toward its home, until under its home's height: a
// sound, then (state 4) it pitches up for a second and a half and blows up (an explosion on the moby +0x16c's behalf), and
// is put 20 above the camera with its trail gathered there (state 2 again). It spins about its z while not in state 4;
// in the level's mode 2 from the boss fight's phase 3 on it is gone. Its trail is sixteen points it lays every fourth
// frame, drawn by two callbacks (L18:0x2f0390, 0x2f06c0) the first such moby queues once a frame. Level 18 only: the
// addresses are the finale's (`orig: L18:`), its own data L18_ADDR, the base ELF's bare. Registered in
// veldin_finale_mobys.cpp; the helpers' declarations in veldin_finale_own.h.
//
// The pvars: +0x10 .. +0x10f the trail's sixteen points, +0x110 the point it flies to, +0x120 its home (on the ground,
// +0x128 its height), +0x130 the dive's first step, +0x140 its turn speed, +0x144 its climb speed, +0x148 a turn added to
// its aim, +0x14c the dive's bend, +0x150 / +0x154 its turn's acceleration / top speed, +0x158 / +0x15c its climb's, +0x160
// its timer, +0x164 the trail's points laid, +0x166 the trail's last point, +0x168 an angle, +0x16c the moby it keeps off.
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/veldin_finale_own.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

// an EE stack frame of the original's size (its locals are passed to the callees by address, and the stale lanes they
// read are the fresh frame's, as in the transliteration)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// the level's speed of the flyer, this frame
float speed() { return fpu_mul(lwc1(L18_ADDR(0x00162214)), dt()); }
}  // namespace

// A step toward `target` (an EE point): turned toward it (plus the pvar +0x148), moved along the heading at the level's
// speed, its height sought; pitched by its climb. 1 when it is within 2 of the point or its time is up; else 0, with the
// point moved 15 .. 20 away from the moby +0x16c (flat, the point's height kept) when it is within 4.5 of it.
// orig: L18:0x002f0120
uint32_t vf_diver_step_l18(uint32_t m, uint32_t target) {
    const Frame f(112);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78), pos = m + 0x10, way = sp, off = sp + 0x10;
    vec3_sub(V(way), V(target), V(pos));
    const float aim = angle_add(atan2_f(lwc1(way), lwc1(way + 4)), lwc1(pv + 0x148));
    yaw_seek(m + 0x48, pv + 0x140, aim, lwc1(pv + 0x150), lwc1(pv + 0x150), lwc1(pv + 0x154));
    swc1(m + 0x10, fpu_add(lwc1(m + 0x10), fpu_mul(cos_vu(lwc1(m + 0x48)), speed())));
    swc1(m + 0x14, fpu_add(lwc1(m + 0x14), fpu_mul(sin_vu(lwc1(m + 0x48)), speed())));
    seek_1d(m + 0x18, pv + 0x144, lwc1(target + 8), lwc1(pv + 0x158), lwc1(pv + 0x158), lwc1(pv + 0x15c));
    swc1(m + 0x44, fpu_mul(lwc1(L18_ADDR(0x0016222c)), lwc1(pv + 0x144)));
    if (vec3_length(V(way)) < 2.0f) return 1;
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x160))) != 0) return 1;
    const uint32_t keep_off = lwu(pv + 0x16c);
    if (keep_off == 0) return 0;
    vec3_sub(V(off), V(pos), V(keep_off + 0x10));
    if (!(vec2_length(V(off)) < 4.5f)) return 0;
    vec2_set_length(V(off), V(off), rand_range_f(15.0f, 20.0f));
    vec3_add(V(off), V(off), V(keep_off + 0x10));
    swc1(off + 8, lwc1(target + 8));
    lq_sq(target, off);
    return 0;
}

// A new point to fly to (into `target`, an EE vector): about its home at the level's distance and height; its time
// reset (3 seconds).
// orig: L18:0x002f02e8
void vf_diver_new_point_l18(uint32_t m, uint32_t target) {
    const Frame f(80);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    const float r = rand_range_f(lwc1(L18_ADDR(0x00162224)), lwc1(L18_ADDR(0x00162228)));
    const float a = rand_angle();
    swc1(sp + 0, fpu_mul(cos_vu(a), r));
    const float y = fpu_mul(sin_vu(a), r);
    sw(sp + 8, 0);
    swc1(sp + 4, y);
    swc1(sp + 8, rand_range_f(lwc1(L18_ADDR(0x0016221c)), lwc1(L18_ADDR(0x00162220))));
    vec3_add(V(target), V(sp), V(pv + 0x120));
    sw(pv + 0x160, static_cast<uint32_t>(frame_scale_i(0xb4)));
}

// The trail: every fourth frame (of the level's counter) a point more (up to four counted), its last point the moby's
// position (w 1); its two draws queued, by the first of these mobys this frame.
// orig: L18:0x002f0848
void vf_diver_trail_l18(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if ((lwu(L18_ADDR(0x0015f5cc)) & 3) == 0) {
        const int32_t laid = lh(pv + 0x164);
        sh(pv + 0x166, (lhu(pv + 0x166) + 1) & 0xf);
        if (static_cast<float>(laid) < 4.0f) sh(pv + 0x164, lhu(pv + 0x164) + 1);
    }
    const uint32_t point = pv + 0x10 + (static_cast<uint32_t>(lh(pv + 0x166)) << 4);
    lq_sq(point, m + 0x10);
    swc1(point + 0xc, 1.0f);
    const uint32_t frame = lwu(L18_ADDR(0x0015f5cc));
    if (frame == lwu(L18_ADDR(0x00162230))) return;
    sw(L18_ADDR(0x00162230), frame);
    sub_001f93e0(L18_ADDR(0x002f0390), m);
    sub_001f93e0(L18_ADDR(0x002f06c0), m);
}

// orig: L18:0x002efb88
void vf_class1355_update_l18(uint32_t m) {
    const Frame f(160);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78), pos = m + 0x10, target = pv + 0x110;
    if (lbu(m + 0x20) != 4)  // its spin
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), fpu_mul(fpu_mul(lwc1(L18_ADDR(0x00162210)), u2f(0x3c8efa35u)), dt())));
    if (lw(L18_ADDR(0x0015f5c4)) == 2 && lw(L18_ADDR(0x0016d290)) == 3) {
        moby_retire(m);
        return;
    }
    switch (lbu(m + 0x20)) {
    case 0: {  // set up (0x002efc58)
        swc1(pv + 0x15c, fpu_mul(dt(), 10.0f));
        swc1(pv + 0x150, fpu_mul(dt_sq(), u2f(0x40490fdbu)));  // (pi)
        swc1(pv + 0x154, fpu_mul(dt(), u2f(0x41490fdbu)));     // (4 pi)
        swc1(pv + 0x158, fpu_mul(dt_sq(), 10.0f));
        swc1(pv + 0x168, rand_angle());
        sh(m + 0x32, 0xff);
        lq_sq(pv + 0x120, pos);
        swc1(pv + 0x128, fpu_add(lwc1(pv + 0x128), 5.0f));
        swc1(pv + 0x128, floor_probe_v(0.5f, V(pv + 0x120), 0));
        vf_diver_new_point_l18(m, target);
        sb(m + 0x20, 1);
        sw(m + 0x94, 0);
        sh(m + 0x34, lhu(m + 0x34) | 1);
        sb(m + 0x31, 0);
        return;
    }
    case 2:  // roaming (0x002efd1c)
        if (vf_diver_step_l18(m, target) != 0) vf_diver_new_point_l18(m, target);
        vf_diver_trail_l18(m);
        if (rand_mod(lw(L18_ADDR(0x00162234))) != 0) return;
        sb(m + 0x20, 3);  // the dive: its first step along its heading
        swc1(pv + 0x130, fpu_mul(cos_vu(lwc1(m + 0x48)), speed()));
        {
            const float y = fpu_mul(sin_vu(lwc1(m + 0x48)), speed());
            sw(pv + 0x14c, 0);
            swc1(pv + 0x138, lwc1(pv + 0x144));
            swc1(pv + 0x134, y);
        }
        return;
    case 3: {  // diving, bending toward its home (0x002efdac)
        const uint32_t step = sp + 0x20, nose = sp + 0x30;
        vec3_sub(V(step), V(pv + 0x120), V(pos));
        vec3_set_length(V(step), V(step), fpu_mul(lwc1(L18_ADDR(0x00162218)), dt()));
        approach_f(pv + 0x14c, 1.0f, dt());
        vec3_lerp(V(step), V(pv + 0x130), V(step), lwc1(pv + 0x14c));
        vec3_add(V(pos), V(pos), V(step));
        swc1(m + 0x48, atan2_f(lwc1(step), lwc1(step + 4)));
        swc1(m + 0x44, -atan2_f(vec2_length(V(step)), lwc1(step + 8)));
        vec3_set_length(V(nose), V(m + 0xc0), 1.0f);
        vec3_add(V(nose), V(nose), V(pos));
        vf_diver_trail_l18(m);
        if (!(lwc1(nose + 8) < lwc1(pv + 0x128))) return;
        moby_sound_start(1, 0, m);  // (on the ground)
        sb(m + 0x20, 4);
        sw(pv + 0x160, static_cast<uint32_t>(frame_scale_i(0x5a)));
        {
            const float low = fpu_add(lwc1(pv + 0x128), 0.7f);
            if (lwc1(m + 0x18) < low) swc1(m + 0x18, low);
        }
        return;
    }
    case 4: {  // pitched up, then the blast (0x002efedc)
        float pitch = lwc1(m + 0x44);
        if (pitch < u2f(0x3f5f66f3u)) pitch = u2f(0x3f5f66f3u);  // (50 degrees at least)
        turn_towards(m + 0x44, 0, pitch, fpu_mul(dt(), u2f(0x41490fdbu)));
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x160))) == 0) {
            vf_diver_trail_l18(m);
            return;
        }
        sb(m + 0x20, 2);
        sh(pv + 0x164, 0);
        explosion_effect(lwu(pv + 0x16c), L18_ADDR(0x0015f580), pos, 0xa, 3, 0x10, 2, 0, 0, -1, 0, 1.0f, 1.0f, 1.5f, 0.5f, 9.0f, 1.0f, 5.0f);
        lq_sq(pos, L18_ADDR(0x001677c0));  // (put 20 above the camera)
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 20.0f));
        for (uint32_t k = 0; k < 4; ++k) lq_sq(pv + 0x10 + k * 0x10, pos);
        return;
    }
    default:  // 1: waiting for the level
        return;
    }
}

}  // namespace rac1
