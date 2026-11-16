// Eudora's class 642: something light the wind blows about (a leaf or a scrap). It waits hidden (scale 0) at its home
// for a random 120 .. 2400 frames, counted only while it is within 32 of the camera point; then it is blown for a random
// 720 .. 1200 frames: shown, it drifts with the level's wind (L4:0x1cb940: +4 its heading, +0x14 its speed), sinking,
// tumbling (a random spin), its own swirl (+0x10) nudged by a random gust each frame and bounced off what it meets (one
// time in eight), itself bounced off what it hits; its scale grows in over the first 10 frames (from its base scale
// times -elapsed / 10, as the original computes it) and shrinks by 1 % a frame over the last 100; then it is back home,
// hidden. Header eudora_mobys.h; registered in eudora_mobys.cpp. Addresses are Eudora's (`orig: L4:`); Eudora-only data
// is L4_ADDR, the base ELF's are bare.
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l4.h>

namespace rac1 {

namespace {
const float kPi = u2f(0x40490fdbu);
float dt() { return lwc1(0x0015ed6c); }  // the frame's time scale
float wind_heading() { return lwc1(L4_ADDR(0x001cb944)); }
float wind_speed() { return lwc1(L4_ADDR(0x001cb954)); }

// the wait (hidden) or the flight: its length into +0x40, its countdown +0x44
void timer_start(uint32_t pv, int32_t lo, int32_t hi) {
    const int32_t frames = rand_range(lo, hi);
    sw(pv + 0x40, static_cast<uint32_t>(frames));
    sw(pv + 0x44, static_cast<uint32_t>(frames));
}

// the wind's push of this frame into `out` (an EE vector): the wind's speed along its heading, a sink of half the
// frame's time
void wind_push(uint32_t out) {
    swc1(out, fpu_mul(wind_speed(), cos_vu(wind_heading())));
    swc1(out + 4, fpu_mul(wind_speed(), sin_vu(wind_heading())));
    swc1(out + 8, fpu_mul(dt(), -0.5f));
}
}  // namespace

// pvars: the velocity +0 (+8 its sink), the swirl +0x10, the spin +0x20 (added to the angles +0x40 .. +0x48 each frame),
// home +0x30, the timer's length +0x40 and countdown +0x44, the base scale +0x4c (its scale +0x2c times 0.8 .. 1.2).
// States: 0 set up, 1 waits hidden, 2 blown.
// orig: L4:0x002d7e90
void eud_class642_update_l4(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x110);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10;
    // (the EE frame's locals: the gust +0, the place before +0x10, the probe ahead / the wind's push +0x20, the spin's
    // axis +0x30, the random spin +0x40)
    const uint32_t gust = sp, from = sp + 0x10, ahead = sp + 0x20, axis = sp + 0x30, spin = sp + 0x40;
    vec_clear(V(from));
    swc1(from, rand_range_f(-1.0f, 1.0f));
    swc1(from + 4, rand_range_f(-1.0f, 1.0f));
    swc1(from + 8, rand_range_f(-1.0f, 1.0f));
    lq_sq(gust, from);
    const float speed0 = vec2_length(V(pv));  // (the speed before this frame)
    lq_sq(from, pos);
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002d7f74) set up: home here, waiting, its base scale, hidden
        lq_sq(pv + 0x30, pos);
        timer_start(pv, 0x78, 0x960);
        const float r = rand_range_f(u2f(0x3f4ccccdu), u2f(0x3f99999au));
        swc1(pv + 0x4c, fpu_mul(lwc1(m + 0x2c), r));
        sb(m + 0x20, 1);
        sw(m + 0x2c, 0);
        break;
    }
    case 1: {  // (0x002d7fc4) waiting near the camera point: then blown, along the wind, with a random spin
        if (!(vec2_distance(V(pos), V(L4_ADDR(0x00166f40))) < 32.0f)) break;
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x44))) == 0) break;
        vec_clear(V(spin));
        swc1(spin, rand_range_f(-1.0f, 1.0f));
        swc1(spin + 4, rand_range_f(-1.0f, 1.0f));
        const float z = rand_range_f(-1.0f, 1.0f);
        sh(m + 0x32, 0x40);
        sb(m + 0x30, 0xff);
        sw(m + 0x2c, 0);
        swc1(spin + 8, z);
        lq_sq(axis, spin);
        vec3_set_length(V(axis), V(axis), fpu_div(wind_speed(), 5.0f));
        wind_push(pv);
        vec3_set_length(V(m + 0x40), V(axis), kPi);
        vec3_set_length(V(pv + 0x20), V(axis), u2f(0x3dc90fdbu));
        vec3_set_length(V(pv + 0x10), V(axis), fpu_mul(wind_speed(), 0.5f));
        timer_start(pv, 0x2d0, 0x4b0);
        sb(m + 0x20, 2);
        break;
    }
    case 2: {  // (0x002d8128) blown
        timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x44)));
        // the swirl: the gust added, the swirl's probe (5 ahead) bouncing it off what it meets one time in eight
        vec3_set_length(V(gust), V(gust), fpu_div(wind_speed(), 10.0f));
        vec3_add(V(pv + 0x10), V(pv + 0x10), V(gust));
        vec3_set_length(V(ahead), V(pv + 0x10), 5.0f);
        vec3_set_length(V(pv + 0x10), V(pv + 0x10), fpu_mul(wind_speed(), 0.5f));
        vec3_add(V(ahead), V(ahead), V(pos));
        if (segment_collide(from, ahead, 6, 0, 0) != 0 && rand_mod(8) == 0) {
            vec3_reflect(V(pv + 0x10), V(pv + 0x10), coll_hit_normal());
            swc1(pv + 0x18, fpu_add(lwc1(pv + 0x18), fpu_div(wind_speed(), 10.0f)));
        }
        // the velocity: the swirl added, then 0.6 of it and 0.4 of the wind's push; the move, bounced off a hit
        vec3_add(V(pv), V(pv), V(pv + 0x10));
        wind_push(ahead);
        vec3_scale(V(ahead), V(ahead), u2f(0x3ecccccdu));
        vec3_scale(V(pv), V(pv), u2f(0x3f19999au));
        vec3_add(V(pv), V(ahead), V(pv));
        vec3_add(V(pos), V(pos), V(pv));
        if (segment_collide(from, pos, 6, 0, 0) != 0) {
            V(pos) = coll_hit_point();
            vec3_reflect(V(pv), V(pv), coll_hit_normal());
            vec3_scale(V(pv), V(pv), 0.25f);
            vec3_add(V(pos), V(pos), V(pv));
            swc1(pv + 8, fpu_add(lwc1(pv + 8), fpu_div(wind_speed(), 5.0f)));
        }
        if (wind_speed() < speed0) vec3_set_length(V(pv), V(pv), wind_speed());  // (no faster than the wind)
        // the sink eased toward half the frame's time
        const float sink = fpu_mul(dt(), -0.5f);
        const float vz = lwc1(pv + 8);
        swc1(pv + 8, sink < vz ? fpu_add(vz, fpu_div(sink, 10.0f)) : sink);
        // the tumble
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0x20)));
        swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x24)));
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x28)));
        // the scale: growing in over the first 10 frames, shrinking over the last 100
        const int32_t length = lw(pv + 0x40), left = lw(pv + 0x44);
        if (length - frame_scale_i(0xa) < left) {
            swc1(m + 0x2c, fpu_div(fpu_mul(lwc1(pv + 0x4c), int_to_float(left - length)), frame_scale_f(10.0f)));
        } else {
            swc1(m + 0x2c, lw(pv + 0x44) < frame_scale_i(0x64) ? fpu_mul(lwc1(m + 0x2c), u2f(0x3f7d70a4u)) : lwc1(pv + 0x4c));
        }
        if (lw(pv + 0x44) > 0) break;
        // over: back home, waiting hidden
        lq_sq(pos, pv + 0x30);
        timer_start(pv, 0x78, 0x960);
        sb(m + 0x20, 1);
        sw(m + 0x2c, 0);
        break;
    }
    default:
        break;
    }
    ee_stack_free(0x110);
}

}  // namespace rac1
