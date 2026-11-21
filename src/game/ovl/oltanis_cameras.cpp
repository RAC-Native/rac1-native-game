// Oltanis (level 14), camera types 20 and 21 (section 4 of the overlay; the level's camera hook dispatches them,
// oltanis_hooks.cpp). Ported by hand from Oltanis's own code (L14:0x315920, 0x314f00, 0x315290, 0x2d6570, 0x316748,
// 0x3159d8, 0x315d48, 0x3167e0), the original's order of calls and float operations kept.
//
// The camera's data block (lvd::camera_data of its +0x84) and its state block (camera+0x70).
//
// Type 20, a scripted fly-along in the way of type 19 (shared_cam19.cpp): accepted while not done (+0x38), the hero in its
// zone (cam_type19_zone) in the states it names (+0x48 / +0x44), and the other camera not holding (+0x7e) unless the hero
// is in state 0xf. Its data: the camera's path +0x20 (its points' w lanes: the field of view in degrees at the points that
// carry one, then the legs' lengths), the look path +0x24, a looked-at moby +0x28 with its offset +0x2c, the frames it
// lasts +0x3a and its speed +0x3c, the box the hero is put in +0x54, the moby whose group is put on the hero's rail at
// the end +0x50. The state block: the springs +0 / +0x20, the leg +0x40 and its fraction +0x44, the two paths' lengths
// +0x48 / +0x4c, the distance flown +0x50, the phase +0x54, the timer +0x56, the field of view's eight path points +0x58
// (halves, -1 none) with their angles +0x68, the starting field of view +0x88. At the end (the path's end, the timer, or
// the data's done flag +0x39) the field of view is put back, the camera given back (+0x7e = 4), the fade flag cleared,
// the group's three mobys put on the hero's rail behind him (0x2d6570), and the camera put on the rail 10 ahead of the
// hero looking back at him.
//
// Type 21, a camera that follows the hero along one of three rails (the data's +0x34 / +0x38 / +0x3c): accepted once the
// current camera is of type 20 (its data's +0x20: 0 waiting, 1 accepted, 2 running). Its keyframes are the level's 39
// records of 0x30 bytes at L14:0x1f6488 (angles +0 / +4 / +0xc / +0x10 in degrees, the distance +8, the yaw targets per
// rail +0x18 .. +0x20, the progress per rail at which each record starts +0x24 .. +0x2c; 10000 at +0x14 ends the list), the
// rails' running lengths in three tables of 1000 floats at L14:0x1f6c80. The state block: the frame counter +0x88, the
// record +0x8c, the last yaw targets +0x90 / +0x94 and the ease +0x98, the five springs' values +0x60 .. +0x70 and their
// velocities +0x74 .. +0x84. On the way, the data's +0x30 counts three events (the three mobys +0x24 / +0x28 / +0x2c of
// the rails get their +0xbc and timers) and +0x32 two (the second sets the level's two angles L14:0x17aff4 / 0x17aff8).
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/camera_types.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/shared_cam19.h>

#include <cstdint>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;      // the hero (+0x80 his position, +0x560 .. +0x568 his rail's path, leg and fraction)
constexpr uint32_t HERO_POS = 0x0013f3d0;  // the hero's position
constexpr uint32_t WORLD_UP = 0x0013f5e0;  // (a vector of the hero's block: its negation is the camera's up)
constexpr float DEGREE = 0.0174532924f;    // (0x3c8efa35)

struct Frame {  // an EE stack frame of the original's size (its locals' offsets kept)
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
    uint32_t n, sp;
};

uint32_t level_moby(int32_t index) { return lwu(L14_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(index) << 8); }
uint32_t cam_data(uint32_t cam) { return lvd::camera_data(lh(cam + 0x84)); }
uint32_t rail_path(int32_t rail) { return lwu(lvd::rail(rail) + 0x10); }
uint32_t keyframe(int32_t i) { return L14_ADDR(0x001f6488) + static_cast<uint32_t>(i) * 0x30; }

// the field of view from its half angle (the projection's tan of it)
void set_view_angle(float half) {
    swc1(L14_ADDR(0x0016d1f0), fpu_div(sin_vu(half), cos_vu(half)));
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
    const uint32_t mob = level_moby(lw(data + 0x28));
    swc1(off, lwc1(data + 0x2c));
    swc1(off + 4, lwc1(data + 0x30));
    sw(off + 0xc, 0);
    swc1(off + 8, lwc1(data + 0x34));
    mat3_transform(V(off), V(off), M(L14_ADDR(0x001676d0)));  // (the camera's rotation)
    vec3_add(V(at), V(mob + 0x10), V(off));
}

// The group of `m` (its +0x21; the rail drones) put on the hero's rail: those on it are put at the point nearest each one's
// own path's start, facing along the rail, the hero placed there (state 0x28); then the first three, facing the hero, are
// moved 0.0 .. behind him on their rails (the level's distance back), at rest (state 6), their slots 0 / 2 / 4.
// orig: L14:0x002d6570
void group_to_rail(uint32_t m) {
    uint32_t e = lvd::group(lbu(m + 0x21));
    if (e == 0) return;
    const Frame f(0x100);
    const uint32_t members = f.sp, near = f.sp + 0x10, along = f.sp + 0x20, rot = f.sp + 0x30, seg = f.sp + 0x40, dist = f.sp + 0x44;
    uint32_t slot = members;  // (the members' list overlaps the locals past its fourth: the original's own layout)
    for (;; e += 2) {
        const uint32_t o = level_moby(static_cast<int32_t>(lhu(e) & 0x7fffu));
        const uint32_t rail = lwu(HERO + 0x560);
        const uint32_t opv = lwu(o + 0x78);
        if (rail == rail_path(lw(opv + 0x60))) {
            swc1(dist, 0.0f);
            int32_t idx = lw(seg);
            float d = lwc1(dist);
            path_closest_point_v(rail, V(lvd::path(lw(opv + 0x88)) + 0x10), V(near), &idx, &d, 0, 20.0f, 5.0f, 0.0f);
            sw(seg, static_cast<uint32_t>(idx));
            swc1(dist, d);
            const uint32_t p = rail + (lwu(seg) << 4);
            vec3_sub(V(along), V(p + 0x40), V(p + 0x10));
            swc1(rot, 0.0f);
            swc1(rot + 4, 0.0f);
            swc1(rot + 8, angle_add(atan2_f(lwc1(along), lwc1(along + 4)), u2f(0xbfc90fdbu)));  // (- pi / 2)
            swc1(rot + 0xc, 0.0f);
            sw(HERO + 0x564, lwu(seg));
            swc1(HERO + 0x568, 0.0f);
            hero_place(rail + (lwu(seg) << 4) + 0x10, rot, 0x28, 0);
        }
        sw(slot, o);
        slot += 4;
        if (lh(e) < 0) break;
    }
    for (int32_t k = 0; k < 3; ++k) {
        const uint32_t o = lwu(members + static_cast<uint32_t>(k) * 4);
        const uint32_t opv = lwu(o + 0x78);
        const uint32_t path = rail_path(lw(opv + 0x60));
        sb(o + 0x20, 6);
        swc1(o + 0x48, atan2_f(fpu_sub(lwc1(HERO + 0x80), lwc1(m + 0x10)), fpu_sub(lwc1(HERO + 0x84), lwc1(m + 0x14))));
        sw(opv + 0x64, lwu(HERO + 0x564));
        swc1(opv + 0x68, lwc1(HERO + 0x568));
        int32_t idx = lw(opv + 0x64);
        float frac = lwc1(opv + 0x68);
        path_advance_v(path, V(o + 0x10), &idx, &frac, 0, -lwc1(L14_ADDR(0x00161a38)));
        sw(opv + 0x64, static_cast<uint32_t>(idx));
        swc1(opv + 0x68, frac);
        vec_clear(V(opv + 0x90));
        sw(opv + 0xf8, static_cast<uint32_t>(k << 1));
        sw(opv + 0xfc, static_cast<uint32_t>((k << 1) + 1));
        if (lw(opv + 0xf0) != -1) {
            region_clear(lwu(opv + 0xf0));
            sw(opv + 0xf0, 0xffffffffu);
        }
    }
}

// the sum of the legs' lengths of `path` from point `from` up to (not with) point `to`, added to `sum`
float legs(uint32_t path, int32_t from, int32_t to, float sum) {
    for (int32_t k = from; k < to; ++k) sum = fpu_add(sum, lwc1(path + (static_cast<uint32_t>(k) << 4) + 0x1c));
    return sum;
}
}  // namespace

// orig: L14:0x00315920
TrRet olt_cam_type20_accept_u(uint32_t cam, uint32_t other, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float,
                              float, float, float, float, float) {
    const uint32_t d = cam_data(cam);
    const auto ret = [](int32_t v) { return TrRet{static_cast<uint32_t>(v), 0.0f}; };
    if (lbu(d + 0x38) != 0) return ret(-1);
    if (other != 0 && lh(other + 0x7e) == 0 && lw(0x001413dcu) != 0xf) return ret(0);
    if (cam_type19_zone(cam) == 0) return ret(0);
    if (lw(d + 0x48) >= 0 && lw(0x001413dcu) != lw(d + 0x48)) return ret(0);
    if (lw(d + 0x44) >= 0 && lw(0x001413d4u) != lw(d + 0x44)) return ret(0);
    return ret(1);
}

// orig: L14:0x00314f00
void olt_cam_type20_init_l14(uint32_t cam) {
    const uint32_t d = cam_data(cam);
    const Frame f(0xd0);
    sb(d + 0x38, 3);
    const uint32_t st = lwu(cam + 0x70);
    for (uint32_t o = 0x40; o < 0x54; o += 4) sw(st + o, 0);
    sh(st + 0x54, 0);
    sh(st + 0x56, static_cast<uint32_t>(frame_scale_i(lh(d + 0x3a))));
    for (uint32_t k = 0; k < 8; ++k) {
        sw(st + 0x68 + k * 4, 0);
        sh(st + 0x58 + k * 2, 0xffff);
    }
    swc1(st + 0x88, [] { const float a = atan2_f(1.0f, lwc1(L14_ADDR(0x0016d1f0))); return fpu_add(a, a); }());
    if (lw(d + 0x20) >= 0) {  // the camera's path: its points carrying a field of view kept (their index, the angle), then its legs measured
        const uint32_t path = lvd::path(lw(d + 0x20));
        uint32_t angle = st + 0x68, point = st + 0x58;
        for (int32_t k = 0; k < lw(path); ++k) {
            const uint32_t w = path + (static_cast<uint32_t>(k) << 4) + 0x1c;
            if (0.0f < lwc1(w)) {
                swc1(angle, lwc1(w));
                sh(point, static_cast<uint32_t>(k));
                angle += 4;
                point += 2;
            }
            const int32_t next = ee_rem(k + 1, lw(path));
            const float len = vec3_distance(V(path + (static_cast<uint32_t>(k) << 4) + 0x10), V(path + (static_cast<uint32_t>(next) << 4) + 0x10));
            swc1(w, len);
            if (k < lw(path) - 1) swc1(st + 0x48, fpu_add(lwc1(st + 0x48), len));
        }
    }
    if (lh(d + 0x3a) > 0) swc1(d + 0x3c, fpu_mul(fpu_div(lwc1(st + 0x48), int_to_float(frame_scale_i(lh(d + 0x3a)))), 60.0f));
    if (lw(d + 0x24) >= 0) measure_path(lvd::path(lw(d + 0x24)), st + 0x4c);
    swc1(st, u2f(0x3cf5c28fu));  // (0.03)
    swc1(st + 4, u2f(0x3e4ccccdu));  // (0.2)
    vec_clear(V(st + 0x10));
    swc1(st + 0x20, u2f(0x3cf5c28fu));
    swc1(st + 0x24, u2f(0x3e4ccccdu));
    vec_clear(V(st + 0x30));
    {  // the camera where the current one is (its matrix source)
        const uint32_t src = lwu(L14_ADDR(0x00167504));
        lq_sq(cam + 0x30, src + 0x30);
        lq_sq(cam, src);
        lq_sq(cam + 0x10, src + 0x10);
        lq_sq(cam + 0x20, src + 0x20);
    }
    // the hero put in its box, unturned (state 0x72)
    const uint32_t rot = f.sp;
    for (uint32_t o = 0; o < 0x10; o += 4) sw(rot + o, 0);
    swc1(rot + 0xc, 1.0f);
    hero_place(lvd::box(lvd::BOXES, lw(d + 0x54)) + 0x30, rot, 0x72, 0);
}

// orig: L14:0x00315290
void olt_cam_type20_update_l14(uint32_t cam) {
    const uint32_t st = lwu(cam + 0x70);
    const uint32_t d = cam_data(cam);
    const Frame f(0x110);
    const uint32_t dir = f.sp, a = f.sp + 0x10, b = f.sp + 0x20, c = f.sp + 0x30;
    if (lh(st + 0x54) == 0) {  // (the start: at the path's first point, looking at the moby or the look path's first point)
        const uint32_t path = lvd::path(lw(d + 0x20));
        const uint32_t look = lvd::path(lw(d + 0x24));
        sw(L14_ADDR(0x0015f404), 1);  // (the fade's flag)
        sh(st + 0x54, 1);
        lq_sq(cam + 0x30, path + 0x10);
        if (lw(d + 0x28) >= 0) {
            moby_point(d, a, b);
            vec3_sub(V(dir), V(b), V(cam + 0x30));
        } else {
            vec3_sub(V(dir), V(look + 0x10), V(cam + 0x30));
        }
        aim(cam, dir, a);
        return;
    }
    // (0x00315428) along the path at its speed
    const uint32_t path = lvd::path(lw(d + 0x20));
    lq_sq(dir, cam + 0x30);  // (the place before the step)
    int32_t ended;
    {
        int32_t leg = lw(st + 0x40);
        float frac = lwc1(st + 0x44);
        ended = path_advance_v(path, V(cam + 0x30), &leg, &frac, 0, fpu_mul(lwc1(d + 0x3c), lwc1(0x0015ed6cu)));
        sw(st + 0x40, static_cast<uint32_t>(leg));
        swc1(st + 0x44, frac);
    }
    if (lh(st + 0x56) != 0 && timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(st + 0x56))) != 0) ended = 1;
    if (lbu(d + 0x39) != 0) ended = 1;
    if (ended != 0) {  // the end: the field of view back, the camera given back, the group on the rail, 10 ahead of the hero
        set_view_angle(fpu_mul(lwc1(st + 0x88), 0.5f));
        swc1(L14_ADDR(0x001675d8), 0.25f);  // (the letterbox's speed)
        sh(cam + 0x7e, 4);
        sw(L14_ADDR(0x0015f404), 0);
        group_to_rail(level_moby(lw(d + 0x50)));
        const uint32_t rail = lwu(HERO + 0x560);
        int32_t leg = lw(HERO + 0x564);
        float frac = lwc1(HERO + 0x568);
        path_advance_v(rail, V(cam + 0x30), &leg, &frac, 0, 10.0f);
        swc1(cam + 0x38, fpu_add(lwc1(cam + 0x38), 2.0f));
        leg = lw(HERO + 0x564);
        frac = lwc1(HERO + 0x568);
        path_advance_v(rail, V(b), &leg, &frac, 0, 0.0f);
        swc1(b + 8, fpu_add(lwc1(b + 8), 1.5f));
        vec3_sub(V(a), V(b), V(cam + 0x30));
        vec3_set_length(V(cam), V(a), 1.0f);
        vec3_cross_ba(V(cam + 0x10), V(cam), V(WORLD_UP));
        vec3_set_length(V(cam + 0x10), V(cam + 0x10), -1.0f);
        vec3_cross_ba(V(cam + 0x20), V(cam + 0x10), V(cam));
        return;
    }
    // (0x00315618) the field of view between the two of its path points around the camera's leg (eased by the distance)
    if (lh(st + 0x58) >= 0 && lh(st + 0x5a) >= 0) {
        for (int32_t i = 0;;) {
            const int32_t leg = lw(st + 0x40);
            const int32_t p0 = lh(st + 0x58 + static_cast<uint32_t>(i) * 2), p1 = lh(st + 0x58 + static_cast<uint32_t>(i + 1) * 2);
            if (!(leg < p0) && leg < p1) {
                const float done = fpu_add(legs(path, p0, leg, 0.0f), lwc1(st + 0x44));
                const float whole = legs(path, leg, p1, legs(path, p0, leg, 0.0f));
                const float t = fpu_div(done, whole);
                const float deg = cos_ease(lwc1(st + 0x68 + static_cast<uint32_t>(i) * 4), lwc1(st + 0x68 + static_cast<uint32_t>(i + 1) * 4), t);
                set_view_angle(fpu_mul(fpu_mul(deg, DEGREE), 0.5f));
            }
            ++i;
            if (!(i < 7) || lh(st + 0x58 + static_cast<uint32_t>(i) * 2) < 0 || lh(st + 0x58 + static_cast<uint32_t>(i + 1) * 2) < 0) break;
        }
    }
    // (0x003157a0) the distance flown; the look at the moby, or along the look path by the share flown
    const float flown = fpu_add(lwc1(st + 0x50), vec3_distance(V(dir), V(cam + 0x30)));
    swc1(st + 0x50, flown);
    if (lw(d + 0x28) >= 0) {
        moby_point(d, b, c);
        vec3_sub(V(a), V(c), V(cam + 0x30));
    } else {
        int32_t leg = 0;
        float frac = 0.0f;
        path_advance_v(lvd::path(lw(d + 0x24)), V(b), &leg, &frac, 0, fpu_mul(fpu_div(flown, lwc1(st + 0x48)), lwc1(st + 0x4c)));
        vec3_sub(V(a), V(b), V(cam + 0x30));
    }
    aim(cam, a, b);
}

// orig: L14:0x00316748
TrRet olt_cam_type21_accept_u(uint32_t cam, uint32_t other, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float,
                              float, float, float, float, float) {
    const uint32_t d = cam_data(cam);
    const auto ret = [](uint32_t v) { return TrRet{v, 0.0f}; };
    const bool current_is_20 = lh(lwu(L14_ADDR(0x00167500)) + 0x86) == 0x14;  // (the current camera)
    if (lw(d + 0x20) == 0) {  // (waiting: accepted, for the next frame, once the current camera is of type 20)
        if (current_is_20) sw(d + 0x20, 1);
        return ret(0);
    }
    if (lw(d + 0x20) != 1) return ret(0);
    if (!current_is_20) return ret(1);
    if (other == 0 || lh(other + 0x7e) != 0) return ret(1);
    return ret(0);
}

// orig: L14:0x003159d8
void olt_cam_type21_init_l14(uint32_t cam) {
    const uint32_t d = cam_data(cam);
    const Frame f(0x100);
    sw(d + 0x20, 2);
    sh(d + 0x30, 0);
    sh(d + 0x32, 0);
    for (uint32_t r = 0; r < 3; ++r) {  // the three rails' running lengths
        const uint32_t path = rail_path(lw(d + 0x34 + r * 4));
        const uint32_t table = L14_ADDR(0x001f6c80) + r * 0xfa0;
        float run = 0.0f;
        int32_t i = 0;
        if (lw(path) - 1 > 0) {
            for (;;) {
                swc1(table + static_cast<uint32_t>(i) * 4, run);
                ++i;
                const bool more = i < lw(path) - 1;
                run = fpu_add(run, lwc1(path + static_cast<uint32_t>(i - 1) * 0x10 + 0x1c));
                if (!more) break;
            }
        }
        swc1(table + static_cast<uint32_t>(i) * 4, run);
    }
    const uint32_t st = lwu(cam + 0x70);
    const uint32_t first = L14_ADDR(0x001f6488);
    sw(st + 0x88, 1);
    sw(st + 0x98, 0);
    sw(st + 0x8c, 0);
    sw(st + 0x90, 0);
    sw(st + 0x94, 0);
    swc1(L14_ADDR(0x001624a8), 1.0f);
    const uint32_t s = st + 0x60;
    swc1(s, fpu_mul(lwc1(first), DEGREE));
    swc1(s + 4, fpu_mul(lwc1(first + 4), DEGREE));
    swc1(s + 8, lwc1(first + 8));
    swc1(s + 0xc, fpu_mul(lwc1(first + 0xc), DEGREE));
    swc1(s + 0x10, fpu_mul(lwc1(first + 0x10), DEGREE));
    for (uint32_t o = 0x74; o < 0x88; o += 4) sw(st + o, 0);
    // behind the hero (his moby's facing), looking at him 1.5 above
    const uint32_t ang = f.sp, dir = f.sp + 0x20, up = f.sp + 0x30;
    const uint32_t hm = lwu(0x001413d0u);
    swc1(ang, atan2_f(lwc1(hm + 0xd0), lwc1(hm + 0xd4)));
    swc1(ang + 4, lwc1(s + 4));
    swc1(ang + 8, lwc1(s + 8));
    camera_polar_point(cam + 0x30, ang, HERO_POS);
    vec3_sub(V(dir), V(HERO_POS), V(cam + 0x30));
    vec3_scale(V(up), V(WORLD_UP), -1.5f);
    vec3_add(V(dir), V(dir), V(up));
    vec3_set_length(V(cam), V(dir), 1.0f);
    vec3_cross_ba(V(cam + 0x10), V(cam), V(up));
    vec3_set_length(V(cam + 0x10), V(cam + 0x10), 1.0f);
    vec3_cross_ba(V(cam + 0x20), V(cam + 0x10), V(cam));
    // the keyframes' list ended (10000) where a record has no start
    for (uint32_t k = 0; k < 39; ++k) {
        const uint32_t w = first + 0x44 + k * 0x30;
        if (lw(w) == 0) sw(w, 0x2710);
    }
}

// orig: L14:0x00315d48
void olt_cam_type21_update_l14(uint32_t cam) {
    const uint32_t d = cam_data(cam);
    const uint32_t st = lwu(cam + 0x70);
    const uint32_t s = st + 0x60, vel = st + 0x74, run = st + 0x88;
    const Frame f(0x120);
    const uint32_t hero_rail = lwu(HERO + 0x560);
    sw(L14_ADDR(0x001624b4), lwu(st + 0x88));
    sw(L14_ADDR(0x001624b8), lwu(run + 4));
    // the rail the hero is on (0, 1, or the third)
    int32_t r;
    if (hero_rail == rail_path(lw(d + 0x34))) r = 0;
    else r = hero_rail == rail_path(lw(d + 0x38)) ? 1 : 2;
    const uint32_t table = L14_ADDR(0x001f6c80) + static_cast<uint32_t>(r) * 0xfa0;
    const float progress = fpu_div(fpu_add(lwc1(table + (lwu(HERO + 0x564) << 2)), lwc1(HERO + 0x568)), lwc1(table + 0xf9c));
    swc1(L14_ADDR(0x001624c0), progress);
    {  // (the second counter: its first step at the level's mark)
        const int32_t n = lh(d + 0x32);
        if (n < 2 && lwc1(L14_ADDR(0x001f6c38) + static_cast<uint32_t>(n) * 0xc + static_cast<uint32_t>(r) * 4) <= progress) sh(d + 0x32, lhu(d + 0x32) + 1);
    }
    if (lh(d + 0x32) == 1) {
        swc1(L14_ADDR(0x0017aff4), fpu_mul(lwc1(L14_ADDR(0x001624b0)), DEGREE));
        swc1(L14_ADDR(0x0017aff8), fpu_mul(lwc1(L14_ADDR(0x001624ac)), DEGREE));
    }
    {  // (0x00315ee8) the first counter's three events at the level's marks: the rails' mobys
        const int32_t n = lh(d + 0x30);
        if (n < 3 && lwc1(L14_ADDR(0x001f6c08) + static_cast<uint32_t>(n) * 0xc + static_cast<uint32_t>(r) * 4) <= lwc1(L14_ADDR(0x001624c0))) {
            const uint32_t ma = level_moby(lw(d + 0x24)), mb = level_moby(lw(d + 0x28)), mc = level_moby(lw(d + 0x2c));
            uint32_t near, first, second;
            bool chose = false;
            if (lwu(HERO + 0x560) == rail_path(lw(lwu(ma + 0x78) + 0x60))) {
                near = ma, first = mb, second = mc;
            } else if (lwu(HERO + 0x560) == rail_path(lw(lwu(mb + 0x78) + 0x60))) {
                near = mb, first = ma, second = mc;
                chose = true;  // (which of the other two goes first is drawn)
            } else {
                near = mc, first = mb, second = ma;
            }
            sb(near + 0xbc, 1);
            if (lh(d + 0x30) > 0) {
                if (chose && rand_mod(2) == 0) {
                    first = mc;
                    second = ma;
                }
                const uint32_t fpv = lwu(first + 0x78);
                sb(first + 0xbc, 5);
                if (lh(d + 0x30) == 2) {
                    sw(fpv + 0x1c8, static_cast<uint32_t>(frame_scale_i(0x2d)));
                    sb(second + 0xbc, 5);
                    sw(lwu(second + 0x78) + 0x1c8, static_cast<uint32_t>(frame_scale_i(0x6e)));
                } else {
                    sw(fpv + 0x1c8, static_cast<uint32_t>(frame_scale_i(lh(d + 0x30) == 3 ? 0x2d : 0x50)));
                }
            }
            sh(d + 0x30, lhu(d + 0x30) + 1);
        }
    }
    {  // (0x003160e4) the next keyframe once the progress reaches its start on the hero's rail
        const uint32_t col = 0x24 + static_cast<uint32_t>(r) * 4;
        const int32_t k = lw(run + 4);
        if (lwc1(keyframe(k + 1) + col) <= lwc1(L14_ADDR(0x001624c0))) sw(run + 4, static_cast<uint32_t>(k + 1));
    }
    float t;
    {  // (0x003161a4) the share of the way between this keyframe and the next (the list's end: the last one, whole)
        const int32_t k = lw(run + 4);
        if (lw(keyframe(k + 1) + 0x14) == 0x2710) {
            t = 1.0f;
            sw(run + 4, static_cast<uint32_t>(k - 1));
        } else {
            const uint32_t col = 0x24 + static_cast<uint32_t>(r) * 4;
            const float a0 = lwc1(keyframe(k) + col), a1 = lwc1(keyframe(k + 1) + col);
            t = fpu_div(fpu_sub(lwc1(L14_ADDR(0x001624c0)), a0), fpu_sub(a1, a0));
        }
    }
    // (0x00316224) the five values sprung toward the keyframes' blend
    const auto angle_at = [&](uint32_t o) {
        const int32_t k = lw(run + 4);
        return angle_lerp(fpu_mul(lwc1(keyframe(k) + o), DEGREE), fpu_mul(lwc1(keyframe(k + 1) + o), DEGREE), t);
    };
    swc1(s, camera_angle_spring(lwc1(s), angle_at(0), lwc1(L14_ADDR(0x00162490)), u2f(0x3e4ccccdu), 0.0f, vel));
    swc1(s + 4, camera_angle_spring(lwc1(s + 4), angle_at(4), lwc1(L14_ADDR(0x00162494)), u2f(0x3e4ccccdu), 0.0f, vel + 4));
    {
        const int32_t k = lw(run + 4);
        const float d0 = lwc1(keyframe(k) + 8), d1 = lwc1(keyframe(k + 1) + 8);
        swc1(s + 8, camera_spring(lwc1(s + 8), fpu_add(d0, fpu_mul(fpu_sub(d1, d0), t)), lwc1(L14_ADDR(0x00162498)), u2f(0x3e4ccccdu), 0.0f, vel + 8));
    }
    swc1(s + 0xc, camera_angle_spring(lwc1(s + 0xc), angle_at(0xc), lwc1(L14_ADDR(0x0016249c)), u2f(0x3e4ccccdu), 0.0f, vel + 0xc));
    swc1(s + 0x10, camera_angle_spring(lwc1(s + 0x10), angle_at(0x10), lwc1(L14_ADDR(0x001624a0)), u2f(0x3e4ccccdu), 0.0f, vel + 0x10));
    // (the yaw: the hero's moby's facing plus the first value, eased toward the keyframes' yaw target for the rail)
    const uint32_t hm = lwu(0x001413d0u);
    float yaw = angle_add(atan2_f(lwc1(hm + 0xd0), lwc1(hm + 0xd4)), lwc1(s));
    swc1(L14_ADDR(0x001624bc), yaw);
    {
        const uint32_t col = 0x18 + static_cast<uint32_t>(r) * 4;
        const int32_t k = lw(run + 4);
        float next = lwc1(keyframe(k + 1) + col);
        const float here = lwc1(keyframe(k) + col);
        const uint32_t ease = run + 0x10;
        if (next == 0.0f && here == 0.0f) {  // (no target: back to the last one, the ease running out)
            next = lwc1(run + 0xc);
            if (lwc1(run + 0xc) == 0.0f) next = lwc1(run + 8);
            const float e = fpu_add(lwc1(ease), fpu_mul(fpu_sub(0.0f, lwc1(ease)), lwc1(L14_ADDR(0x001624a4))));
            swc1(ease, e);
            yaw = angle_lerp(yaw, next, cubic_interp(1.0f, 0.0f, 1.0f, 0.0f, e));
        } else if (next != 0.0f) {  // (the next keyframe's target, the ease running in)
            const float e = fpu_add(lwc1(ease), fpu_mul(fpu_sub(1.0f, lwc1(ease)), lwc1(L14_ADDR(0x001624a4))));
            swc1(ease, e);
            yaw = angle_lerp(yaw, next, cubic_interp(1.0f, 0.0f, 1.0f, 0.0f, e));
            swc1(run + 8, here);
            swc1(run + 0xc, next);
        } else {  // (this keyframe's target, the ease running out)
            const float e = fpu_add(lwc1(ease), fpu_mul(fpu_sub(0.0f, lwc1(ease)), lwc1(L14_ADDR(0x001624a4))));
            swc1(ease, e);
            yaw = angle_lerp(yaw, here, cubic_interp(1.0f, 0.0f, 1.0f, 0.0f, e));
            swc1(run + 8, here);
            swc1(run + 0xc, next);
        }
    }
    // (0x003165b0) the camera at the yaw / pitch / distance from the hero, looking at him 1.5 above, rolled and turned
    const uint32_t ang = f.sp, dir = f.sp + 0x20, up = f.sp + 0x30;
    swc1(ang, yaw);
    swc1(ang + 4, lwc1(s + 4));
    swc1(ang + 8, lwc1(s + 8));
    camera_polar_point(cam + 0x30, ang, HERO_POS);
    vec3_sub(V(dir), V(HERO_POS), V(cam + 0x30));
    vec3_scale(V(up), V(WORLD_UP), -1.5f);
    vec3_add(V(dir), V(dir), V(up));
    vec3_set_length(V(cam), V(dir), 1.0f);
    vec3_cross_ba(V(cam + 0x10), V(cam), V(up));
    if (lwc1(s + 0xc) != 0.0f) {
        vec_rotate_axis(V(cam), V(cam), V(cam + 0x10), lwc1(s + 0xc));
        vec3_set_length(V(cam), V(cam), 1.0f);
        vec3_cross_ba(V(cam + 0x10), V(cam), V(up));
    }
    if (lwc1(s + 0x10) != 0.0f) {
        vec_rotate_axis(V(cam), V(cam), V(up), lwc1(s + 0x10));
        vec3_set_length(V(cam), V(cam), 1.0f);
        vec3_cross_ba(V(cam + 0x10), V(cam), V(up));
    }
    vec3_set_length(V(cam + 0x10), V(cam + 0x10), 1.0f);
    vec3_cross_ba(V(cam + 0x20), V(cam + 0x10), V(cam));
    sw(st + 0x88, lwu(st + 0x88) + 1);
}

// orig: L14:0x003167e0
void olt_cam_type21_exit_l14(uint32_t cam) {
    if (lw(0x001413dcu) != 0xf) sh(cam + 0x7e, 3);
}

}  // namespace rac1
