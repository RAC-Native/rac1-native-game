// Kerwan's own moby classes: the shuttles (classes 868, 905 and 928 run the same update), platforms that travel between the
// points of a path of the level. Header kerwan_mobys.h, registered in kerwan_mobys.cpp. Addresses are Kerwan's
// (`orig: L3:`); the engine's words shared with Veldin are the level's own (OV), the base ELF's are bare.
//
// The pvars: +0x60 the attached frame (what stands on it moves with it), +0xa0 the step, +0xb0 the path follower (+0xb0 the
// point, +0xb4 the direction, +0xc0 the path), +0xe0 the path's index, +0xe4 the speed, +0xe8 the wait, +0xec the top speed,
// +0xf0 the wait at each point (60 Hz frames; 0: none), +0xf4 the braking distance, +0xf8 1: start from the far end, +0xfc the
// model's scale, +0x100 the start's delay, +0x104 the sound's slot, +0x108 0: run from end to end, else stop at each point,
// +0x10c the frame count of an end-to-end run, +0x110 the path's length, +0x114 the run's acceleration.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

// the path's point `i` (16 bytes each after a 16-byte header)
uint32_t path_point(uint32_t path, int32_t i) { return path + 0x10 + static_cast<uint32_t>(i) * 16; }

// The moby's sound in the slot word `slot_word` stopped when that slot still plays it, and the word set to -1.
void sound_release(uint32_t m, uint32_t slot_word) {
    const int32_t slot = lw(slot_word);
    if (slot != -1) {
        const uint32_t e = 0x0013e550 + static_cast<uint32_t>(slot * 0x70);
        if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
    }
    sw(slot_word, 0xffffffffu);
}
}  // namespace

// Classes 868 / 905 / 928, the shuttles. The state (+0x20):
// 0 placed at the path's first point (or its last, +0xf8), the model scaled, after the start's delay +0x100 the run's length
//   and acceleration (4 lengths over 240 frames squared) -> 1;
// 1 an end-to-end run of 240 frames: accelerating over the first half, braking over the second, from the end it left (the
//   classes 905 / 868 with their running sound 0x389), then the direction turned, the sound stopped, the wait +0xf0 -> 2;
// 2 waits -> 1;
// 3 a run from point to point: the step toward the next point (the follower's direction, wrapped), its speed brought toward
//   the top speed +0xec (toward half a frame's worth inside the braking distance +0xf4 of the run's end: the last point in its
//   direction, or the next point when it stops at each), the step clamped to it; within two steps of the point it is reached,
//   and when it stops there (a stop at each point, or the run's end) with a wait +0xf0 set: the sound stopped -> 4;
// 4 waits -> 3.
// Whatever stands on it is moved with it: the attached frame from the frame's move.
// orig: L3:0x00294c08 (the jump table L3:0x1e25b0 on the state, in order)
void kerwan_class868_update_l3(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t moved = sp, start_ang = sp + 0x10;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t fol = pv + 0xb0;
    vec3_scale(V(moved), V(m + 0x10), -1.0f);
    lq_sq(start_ang, m + 0x40);
    debug_view_2();
    auto direction = [&] { return static_cast<int32_t>(static_cast<int8_t>(lbu(fol + 4))); };
    auto path = [&] { return lwu(fol + 0x10); };
    auto last = [&] { return path() + static_cast<uint32_t>(lw(path())) * 16; };  // (the last point: the header's 16 bytes)
    auto wait_then = [&](uint32_t next_state) {  // the sound stopped, the next state, the wait +0xf0
        sound_release(m, pv + 0x104);
        sb(m + 0x20, next_state);
        sw(pv + 0xe8, static_cast<uint32_t>(frame_scale_i(lw(pv + 0xf0))));
    };
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x00294c9c)
        sw(pv + 0xc0, lvd::path(lw(pv + 0xe0)));
        sb(fol + 4, lw(pv + 0xf8) == 0 ? 1u : 0xffu);
        const int32_t count = lw(path());
        lq_sq(m + 0x10, direction() < 0 ? path() + 0x10 : path() + 0x10 + static_cast<uint32_t>(count - 1) * 16);
        sw(fol, direction() < 0 ? 0u : static_cast<uint32_t>(lw(path()) - 1));
        swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(pv + 0xfc)));
        sw(pv + 0x104, 0xffffffffu);
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x100))) == 0) break;
        swc1(pv + 0x110, vec3_distance(V(path() + 0x10), V(last())));
        const int32_t frames = frame_scale_i(0xf0) * frame_scale_i(0xf0);
        sw(pv + 0x10c, 0);
        swc1(pv + 0x114, fpu_div(fpu_mul(lwc1(pv + 0x110), 4.0f), int_to_float(frames)));
        sb(m + 0x20, 1);
        break;
    }
    case 1: {  // (0x00294da0)
        const int16_t cls = lh(m + 0xa6);
        float accel = lwc1(pv + 0x114);
        float base = 0.0f;
        if ((cls == 0x389 || cls == 0x364) && sub_0028d8c0(m, lw(pv + 0x104)) == 0)
            sw(pv + 0x104, static_cast<uint32_t>(moby_sound_play(0, 4, m, 0x389)));
        sw(pv + 0x10c, lwu(pv + 0x10c) + 1);
        int32_t t = lw(pv + 0x10c);
        const int32_t full = frame_scale_i(0xf0);
        if (full / 2 < lw(pv + 0x10c)) {  // the second half: braking, from the far end
            accel = -accel;
            base = lwc1(pv + 0x110);
            t = frame_scale_i(0xf0) - lw(pv + 0x10c);
        }
        float along = fpu_add(base, fpu_mul(fpu_mul(accel, 0.5f), int_to_float(t * t)));
        if (direction() < 0) along = fpu_sub(lwc1(pv + 0x110), along);
        const uint32_t span = sp + 0x30, step = sp + 0x20;
        vec3_sub(V(span), V(last()), V(path() + 0x10));
        lq_sq(step, span);
        vec3_set_length(V(step), V(step), along);
        vec3_add(V(span), V(path() + 0x10), V(step));
        lq_sq(m + 0x10, span);
        if (lw(pv + 0x10c) < frame_scale_i(0xf0)) break;
        sb(fol + 4, static_cast<uint32_t>(-direction()));
        wait_then(2);
        break;
    }
    case 2:  // (0x00294f30)
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xe8))) != 0) {
            sw(pv + 0x10c, 0);
            sb(m + 0x20, 1);
        }
        break;
    case 3: {  // (0x00294f48)
        const int32_t count = lw(path());
        const int32_t dir = direction();
        const int32_t end = dir < 0 ? 0 : count - 1;  // the run's last point in its direction
        const int32_t next = ee_rem(lw(fol) + count + dir, count);
        vec3_sub(V(pv + 0xa0), V(path_point(path(), next)), V(m + 0x10));
        const uint32_t goal = lw(pv + 0x108) != 0 ? path_point(path(), next) : path_point(path(), end);
        const float d = vec3_distance(V(m + 0x10), V(goal));
        const float braking = lwc1(pv + 0xf4);
        const float top = lwc1(pv + 0xec);
        if (d < braking)
            approach_f(pv + 0xe4, fpu_mul(dt(), 0.5f), fpu_mul(fpu_mul(fpu_div(fpu_mul(top, top), braking), dt_sq()), 0.5f));
        else
            approach_f(pv + 0xe4, fpu_mul(top, dt()), fpu_mul(fpu_mul(fpu_div(fpu_mul(top, top), braking), dt_sq()), 0.5f));
        vec3_clamp_length(pv + 0xa0, lwc1(pv + 0xe4));
        vec3_add(V(m + 0x10), V(m + 0x10), V(pv + 0xa0));
        const float left = vec3_distance(V(m + 0x10), V(path_point(path(), next)));
        if (!(left < fpu_add(lwc1(pv + 0xe4), lwc1(pv + 0xe4)))) break;
        sw(fol, static_cast<uint32_t>(next));
        if (next != end && lw(pv + 0x108) == 0) break;
        if (lw(pv + 0xf0) <= 0) break;
        wait_then(4);
        break;
    }
    case 4:  // (0x0029511c)
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xe8))) != 0) sb(m + 0x20, 3);
        break;
    default:
        break;
    }
    vec3_add(V(moved), V(moved), V(m + 0x10));
    attach_frame_set(pv + 0x60, moved, start_ang, m + 0x40);
    ee_stack_free(0xf0);
}

}  // namespace rac1
