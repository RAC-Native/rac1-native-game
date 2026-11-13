// Camera type 19, a scripted fly-through: one port of the copies Orxon (L10), Gemlik (L13), Oltanis (L14) and Quartu (L15)
// carry (the same code relinked; compared with the relocated fields masked; Oltanis's accept calls its own first copy of
// the zone test, the same code as its second). What they name is the engine's (OV: the camera records 0x15ef50, the
// screen fade 0x15f3fc and its flag 0x15f404, the moby table 0x15ffd8, the camera's matrix source 0x166e04 and its
// rotation 0x166fd0, the field of view 0x16caf0, the letterbox speed 0x166ed8, the paths 0x1b04b0) or the base ELF's (bare:
// the hero 0x13f3d0 / 0x13f5e0 and his moby 0x1413d0, the frame time 0x15ed6c). Header shared_cam19.h.
//
// The camera's data (+0x1c of its record): its zone (sphere +8, box +0xc, cylinder +0x10, polygon +0x14; -1 none), the
// camera's path +0x20, the look path +0x24, the looked-at moby +0x28 with an offset +0x2c, its started / done flags +0x38 /
// +0x39, the frames it lasts +0x3a and its speed +0x3c, "ends with the fade" +0x40, the hero's states +0x44 / +0x48 it
// waits for, "the hero stays" +0x4c. The camera's state block (camera+0x70): two springs +0 / +0x20 (rate, damping, a
// vector), the path's leg +0x40 and its fraction +0x44, the two paths' lengths +0x48 / +0x4c, the distance flown +0x50, the
// phase +0x54 (0 fading in, 1 flying, 2 fading out, 3 done), the timer +0x56, the field of view's eight path points
// +0x58 (halves, -1 none) with their angles +0x68, the starting field of view +0x88.
#include <game/ovl/shared_cam19.h>

#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;  // the hero's position
constexpr uint32_t WORLD_UP = 0x0013f5e0;  // (a vector of the hero's block: its negation is the camera's up)

uint32_t cam_data(uint32_t cam) { return lwu(lwu(OV(0x0015ef50)) + (static_cast<uint32_t>(lh(cam + 0x84)) << 5) + 0x1c); }
uint32_t level_path(int32_t idx) { return lwu(OV(0x001b04b0) + (static_cast<uint32_t>(idx) << 2)); }

// the field of view from its angle in degrees (the projection's tan of the half angle)
void set_view_angle(float half) {
    swc1(OV(0x0016caf0), fpu_div(sin_vu(half), cos_vu(half)));
    projection_setup();
}

// the leg lengths of `path` into its points' w (the last's back to the first), their sum but the last into `total`
void measure_path(uint32_t path, uint32_t total) {
    for (int32_t k = 0; k < lw(path); ++k) {
        const int32_t next = ee_rem(k + 1, lw(path));
        const float len = vec3_distance(V(path + (static_cast<uint32_t>(k) << 4) + 0x10), V(path + (static_cast<uint32_t>(next) << 4) + 0x10));
        swc1(path + (static_cast<uint32_t>(k) << 4) + 0x1c, len);
        if (k < lw(path) - 1) swc1(total, fpu_add(lwc1(total), len));
    }
}

// the camera's rows from its look direction `dir`: forward, side (with the up `up`), up
void aim(uint32_t cam, uint32_t dir, uint32_t up) {
    vec3_scale(V(up), V(WORLD_UP), -1.0f);
    vec3_set_length(V(cam), V(dir), 1.0f);
    vec3_cross_ba(V(cam + 0x10), V(cam), V(up));
    vec3_set_length(V(cam + 0x10), V(cam + 0x10), 1.0f);
    vec3_cross_ba(V(cam + 0x20), V(cam + 0x10), V(cam));
}

// the looked-at moby's point (its position plus the offset turned by the camera's rotation) into `at`, through `off`
void moby_point(uint32_t data, uint32_t off, uint32_t at) {
    const uint32_t mob = lwu(OV(0x0015ffd8)) + (lwu(data + 0x28) << 8);
    swc1(off, lwc1(data + 0x2c));
    swc1(off + 4, lwc1(data + 0x30));
    sw(off + 0xc, 0);
    swc1(off + 8, lwc1(data + 0x34));
    mat3_transform(V(off), V(off), M(OV(0x00166fd0)));
    vec3_add(V(at), V(mob + 0x10), V(off));
}
}  // namespace

// orig: L10:0x002f5a78 L13:0x00316f80 L14:0x003141c8 L14:0x00314e20 L15:0x002f7958
int32_t cam_type19_zone(uint32_t cam) {
    const uint32_t d = cam_data(cam);
    int32_t in;
    if (lw(d + 0xc) >= 0) in = point_in_box(HERO_POS, lw(d + 0xc));
    else if (lw(d + 0x10) >= 0) in = point_in_cylinder(HERO_POS, lw(d + 0x10));
    else if (lw(d + 8) >= 0) in = point_in_sphere(HERO_POS, lw(d + 8));
    else if (lw(d + 0x14) >= 0) {
        const uint32_t poly = level_path(lw(d + 0x14));
        in = point_in_polygon(HERO_POS, poly + 0x10, lw(poly));
    } else {
        return lbu(d + 0x38) != 0 && lbu(d + 0x39) == 0 ? 1 : 0;
    }
    return in != 0 ? 1 : 0;
}

// Never once done (+0x39); while started (+0x38) only from a camera of type 0x13; not from a camera of a higher priority
// (+0x7c) that is not ending (+0x7e); then the hero in the zone and in the states it waits for (his moby's +0xc / +4).
// orig: L10:0x002f65f8 L13:0x00317b00 L14:0x00314d48 L15:0x002f84d8
int32_t cam_type19_accept(uint32_t cam, uint32_t other) {
    const uint32_t d = cam_data(cam);
    if (lbu(d + 0x39) != 0) return -1;
    if (lbu(d + 0x38) != 0 && lh(other + 0x86) != 0x13) return 1;
    if (other != 0 && lh(other + 0x7e) == 0 && !(lbu(other + 0x7c) < lbu(cam + 0x7c))) return 0;
    if (cam_type19_zone(cam) == 0) return 0;
    if (lw(d + 0x48) >= 0 && lw(0x001413dc) != lw(d + 0x48)) return 0;
    if (lw(d + 0x44) >= 0 && lw(0x001413d4) != lw(d + 0x44)) return 0;
    return 1;
}

// Started: the state block cleared, the timer from +0x3a, the field of view's points from the camera path's points whose
// w (an angle in degrees, 0..180; above: the starting view) is set, the paths' leg lengths and totals (the speed +0x3c
// from the frames), the springs, the camera's matrix from the current one; the hero stopped (state 0x72) unless he stays.
// orig: L10:0x002f5b58 L13:0x00317060 L14:0x003142a8 L15:0x002f7a38
void cam_type19_init(uint32_t cam) {
    const uint32_t d = cam_data(cam);
    sb(d + 0x38, 1);
    const uint32_t st = lwu(cam + 0x70);
    const uint32_t blk = st + 0x40, points = st + 0x58, angles = st + 0x68;
    for (uint32_t o = 0; o < 0x14; o += 4) sw(blk + o, 0);
    sh(blk + 0x14, 0);
    sh(blk + 0x16, static_cast<uint32_t>(frame_scale_i(lh(d + 0x3a))));
    for (uint32_t k = 0; k < 8; ++k) {
        sw(angles + k * 4, 0);
        sh(points + k * 2, 0xffff);
    }
    const float half_view = atan2_f(1.0f, lwc1(OV(0x0016caf0)));
    swc1(points + 0x30, fpu_add(half_view, half_view));
    if (lw(d + 0x20) >= 0) {
        const uint32_t path = level_path(lw(d + 0x20));
        uint32_t slot = 0;
        for (int32_t k = 0; k < lw(path); ++k) {
            const uint32_t pt = path + (static_cast<uint32_t>(k) << 4);
            const float deg = lwc1(pt + 0x1c);
            if (0.0f < deg) {
                swc1(angles + slot * 4, 180.0f < deg ? fpu_mul(lwc1(points + 0x30), u2f(0x42652ee0u)) : deg);
                sh(points + slot * 2, static_cast<uint32_t>(k));
                ++slot;
            }
            const int32_t next = ee_rem(k + 1, lw(path));
            const float len = vec3_distance(V(pt + 0x10), V(path + (static_cast<uint32_t>(next) << 4) + 0x10));
            swc1(pt + 0x1c, len);
            if (k < lw(path) - 1) swc1(blk + 8, fpu_add(lwc1(blk + 8), len));
        }
    }
    if (lh(d + 0x3a) > 0)
        swc1(d + 0x3c, fpu_mul(fpu_div(lwc1(blk + 8), int_to_float(frame_scale_i(lh(d + 0x3a)))), 60.0f));
    if (lw(d + 0x24) >= 0) measure_path(level_path(lw(d + 0x24)), blk + 0xc);
    for (const uint32_t spring : {st, st + 0x20}) {
        swc1(spring, u2f(0x3cf5c28fu));
        swc1(spring + 4, 0.2f);
        vec_clear(V(spring + 0x10));
    }
    const uint32_t src = lwu(OV(0x00166e04));
    lq_sq(cam + 0x30, src + 0x30);
    lq_sq(cam, src);
    lq_sq(cam + 0x10, src + 0x10);
    lq_sq(cam + 0x20, src + 0x20);
    if (lw(d + 0x4c) == 0) hero_set_state(0x72, 1);
}

// Phase 0 fades the screen in (0.05 a frame); faded, the camera goes to its path's first point looking at the moby or
// the look path's first point (phase 1). Phase 1 / others fade back out and fly: along the camera path at its speed, the
// field of view eased between the path's view points by the distance along the legs, the look point the moby's or as far
// along the look path as the camera along its own; at the path's end, when the timer runs out (or nears 20 frames with
// "ends with the fade"), or once done, phase 2. Phase 2 fades in again, then ends (phase 3: the starting view back, done
// +0x39, the letterbox opening, the camera ending +0x7e 4, the hero free unless he stays) and, with "ends with the fade",
// keeps flying meanwhile.
// orig: L10:0x002f5f18 L13:0x00317420 L14:0x00314668 L15:0x002f7df8
void cam_type19_update(uint32_t cam) {
    const uint32_t st = lwu(cam + 0x70);
    const uint32_t points = st + 0x58, blk = st + 0x40;
    const uint32_t d = cam_data(cam);
    const uint32_t fade = OV(0x0015f3fc);
    const uint32_t sp = ee_stack_alloc(0x120);
    const uint32_t was = sp, dir = sp + 0x10, up = sp + 0x20, at = sp + 0x30;
    const int32_t phase = lh(blk + 0x14);
    if (phase == 0) {
        const float f = fpu_add(lwc1(fade), 0.05f);
        swc1(fade, f);
        if (1.0f <= f) {
            const uint32_t path = level_path(lw(d + 0x20));
            const uint32_t look = level_path(lw(d + 0x24));
            swc1(fade, 1.0f);
            sw(OV(0x0015f404), 1);
            sh(blk + 0x14, 1);
            lq_sq(cam + 0x30, path + 0x10);
            // (here the direction is sp+0 and the up sp+0x10)
            if (lw(d + 0x28) >= 0) {
                moby_point(d, dir, up);
                vec3_sub(V(sp), V(up), V(cam + 0x30));
            } else {
                vec3_sub(V(sp), V(look + 0x10), V(cam + 0x30));
            }
            aim(cam, sp, dir);
        }
        ee_stack_free(0x120);
        return;
    }
    if (phase == 3) {
        ee_stack_free(0x120);
        return;
    }
    bool fly;
    if (phase == 2) {
        const float f = fpu_add(lwc1(fade), 0.05f);
        swc1(fade, f);
        if (1.0f <= f) {
            sh(blk + 0x14, 3);
            swc1(fade, 1.0f);
            set_view_angle(fpu_mul(lwc1(points + 0x30), 0.5f));
            sb(d + 0x39, 1);
            swc1(OV(0x00166ed8), 0.05f);
            sh(cam + 0x7e, 4);
            sw(OV(0x0015f404), 0);
            if (lw(d + 0x4c) == 0) hero_set_state(0, 1);
        }
        fly = lw(d + 0x40) != 0;
    } else {
        const float f = fpu_sub(lwc1(fade), 0.05f);
        swc1(fade, f);
        if (f < 0.0f) swc1(fade, 0.0f);
        fly = true;
    }
    if (!fly) {
        ee_stack_free(0x120);
        return;
    }

    const uint32_t path = level_path(lw(d + 0x20));
    const uint32_t pos = cam + 0x30;
    lq_sq(was, pos);
    int32_t ended;
    {
        int32_t leg = lw(blk);
        float frac = lwc1(blk + 4);
        ended = path_advance_v(path, V(pos), &leg, &frac, 0, fpu_mul(lwc1(d + 0x3c), lwc1(0x0015ed6c)));
        sw(blk, static_cast<uint32_t>(leg));
        swc1(blk + 4, frac);
    }
    bool done = ended != 0;
    if (lh(blk + 0x16) != 0) {
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(blk + 0x16))) != 0) done = true;
        else if (lw(d + 0x40) != 0 && lh(blk + 0x16) < frame_scale_i(float_to_int(20.0f))) done = true;
    }
    if (lbu(d + 0x39) != 0) done = true;
    if (done) {
        sh(blk + 0x14, 2);
        if (lw(d + 0x40) == 0) {
            ee_stack_free(0x120);
            return;
        }
    }

    // the field of view: between the view points around the camera's leg
    if (lh(points) >= 0 && lh(points + 2) >= 0) {
        int32_t i = 0;
        for (;;) {
            const int32_t leg = lw(blk);
            const int32_t a = lh(points + static_cast<uint32_t>(i) * 2);
            const int32_t next = i + 1;
            if (!(leg < a)) {
                const int32_t b = lh(points + static_cast<uint32_t>(next) * 2);
                if (leg < b) {
                    float from_a = 0.0f;
                    for (int32_t k = a; k < leg; ++k) from_a = fpu_add(from_a, lwc1(path + (static_cast<uint32_t>(k) << 4) + 0x1c));
                    float span = from_a;
                    from_a = fpu_add(from_a, lwc1(blk + 4));
                    for (int32_t k = leg; k < b; ++k) span = fpu_add(span, lwc1(path + (static_cast<uint32_t>(k) << 4) + 0x1c));
                    const uint32_t angles = points + 0x10;
                    const float deg = cos_ease(lwc1(angles + static_cast<uint32_t>(i) * 4), lwc1(angles + static_cast<uint32_t>(next) * 4),
                                               fpu_div(from_a, span));
                    set_view_angle(fpu_mul(fpu_mul(deg, u2f(0x3c8efa35u)), 0.5f));
                }
            }
            if (!(next < 7) || lh(points + static_cast<uint32_t>(next) * 2) < 0 || lh(points + static_cast<uint32_t>(next + 1) * 2) < 0) break;
            i = next;
        }
    }

    const float flown = fpu_add(lwc1(blk + 0x10), vec3_distance(V(was), V(pos)));
    swc1(blk + 0x10, flown);
    if (lw(d + 0x28) >= 0) {
        moby_point(d, up, at);
        vec3_sub(V(dir), V(at), V(pos));
    } else {
        const float along = fpu_mul(fpu_div(flown, lwc1(blk + 8)), lwc1(blk + 0xc));
        sw(sp + 0x40, 0);
        sw(sp + 0x44, 0);
        const uint32_t look = level_path(lw(d + 0x24));
        int32_t leg = lw(sp + 0x40);
        float frac = lwc1(sp + 0x44);
        path_advance_v(look, V(up), &leg, &frac, 0, along);
        sw(sp + 0x40, static_cast<uint32_t>(leg));
        swc1(sp + 0x44, frac);
        sub_001f6c30(up, 0.2f);
        debug_stub_1f6c28();
        vec3_sub(V(dir), V(up), V(pos));
    }
    aim(cam, dir, up);
    ee_stack_free(0x120);
}

}  // namespace rac1
