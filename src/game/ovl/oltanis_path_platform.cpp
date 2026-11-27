// Oltanis (level 14), class 1417: a platform that flies a path of the level (pvars +0x78; +0x70 / +0x74 its place on it,
// +0x8e "loops", +0xc8 its speed), springing toward the path's point (+0x80 .. +0x88), turning toward the way (+0x98), its
// pitch (+0x94) and its bank by the turn (+0x90), carrying a second moby of the level (+0xa0, laid at its offsets +0xa4 ..
// +0xac along the platform's axes; -1 none). Set up (state 0) it measures its path's legs and starts at its first point,
// hidden (1) until the hero is in its box (+0xc4; -1: none, it shows at once). Flying (2) it speeds up (its +0xb4 by the
// rate +0xb8, to the level's limit) while the hero rides it (on its carried moby, in either of the two riding states) and
// slows down otherwise, its height lowered by that speed (never below 10); a rider on a type 7 camera gets that camera's
// settings (0x3135b8), and the carried moby's pvars +0x20 a value by how long ago he rode it (2.0 while riding, then 0, or
// class 803's 40 with its rates). At the path's end (when it does not loop) or once below the level's floor it blows up;
// a hit of kind 0x800000 does the same. Gone (3), it comes back at its path's start after the level's delay (+0x8c), with
// its colour. Its sound (4, the slot +0x7c) plays while it flies. Ported by hand from Oltanis's own code (L14:0x306ee0,
// 0x307510, 0x307620, 0x3135b8), the original's order of calls and float operations kept.
#include <game/level/level_data.h>
#include <game/level/level_write.h>
#include <game/math/vecmath.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l14.h>

#include <cstdint>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 his position, +0x964 / +0x994 the mobys of his two riding states)

struct Frame {  // an EE stack frame of the original's size (its locals' offsets kept)
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
    uint32_t n, sp;
};

uint32_t level_moby(int32_t index) { return lwu(L14_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(index) << 8); }

// the moby's sound in slot `at` stopped (while still its own and playing), the slot cleared
void sound_stop(uint32_t m, uint32_t at) {
    const int32_t slot = lw(at);
    if (slot != -1) {
        const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot) * 0x70u;
        if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
    }
    sw(at, 0xffffffffu);
}

// the carried moby laid at its offsets along the platform's three axes (through the scratch vector `off`)
void lay_carried(uint32_t m, uint32_t pv, uint32_t o, uint32_t off) {
    const uint32_t at = o + 0x10;
    vec3_scale(V(off), V(m + 0xc0), lwc1(pv + 0xa4));
    vec3_add(V(at), V(m + 0x10), V(off));
    vec3_scale(V(off), V(m + 0xd0), lwc1(pv + 0xa8));
    vec3_add(V(at), V(at), V(off));
    vec3_scale(V(off), V(m + 0xe0), lwc1(pv + 0xac));
    vec3_add(V(at), V(at), V(off));
}

// at the path's first point, facing its second, the springs at rest
void path_start(uint32_t m, uint32_t pv, uint32_t path) {
    sw(pv + 0x70, 0);
    sw(pv + 0x74, 0);
    lq_sq(m + 0x10, path + 0x10);
    swc1(m + 0x48, atan2_f(fpu_sub(lwc1(path + 0x20), lwc1(path + 0x10)), fpu_sub(lwc1(path + 0x24), lwc1(path + 0x14))));
    for (uint32_t o : {0x90u, 0x88u, 0x84u, 0x80u, 0x98u, 0x94u}) swc1(pv + o, 0.0f);
}

// blown up: hidden (the carried moby too, put away at 5, 5, 5), its sound stopped
void blow_up(uint32_t m, uint32_t pv) {
    sh(m + 0x34, lhu(m + 0x34) | 0x41u);
    if (lw(pv + 0xa0) >= 0) {
        const uint32_t o = level_moby(lw(pv + 0xa0));
        sh(o + 0x34, lhu(o + 0x34) | 0x41u);
        swc1(o + 0x10, 5.0f);
        swc1(o + 0x18, 5.0f);
        swc1(o + 0x14, 5.0f);
    }
    sound_stop(m, pv + 0x7c);
}

// The path's legs measured (each point's w the distance to the next, the last's to the first), then at its start.
// orig: L14:0x00307510
void path_setup(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t path = lvd::path(lw(pv + 0x78));
    for (int32_t i = 0; i < lw(path); ++i) {
        const int32_t next = ee_rem(i + 1, lw(path));
        lvd::set_path_f32(path + (static_cast<uint32_t>(i) << 4) + 0x1c,
             vec3_distance(V(path + (static_cast<uint32_t>(i) << 4) + 0x10), V(path + (static_cast<uint32_t>(next) << 4) + 0x10)));
    }
    path_start(m, pv, path);
}

// One frame along the path; 0 when it blew up (at the path's end, or below the level's floor).
// orig: L14:0x00307620
int32_t fly(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x90);
    const uint32_t point = f.sp, way = f.sp + 0x10;
    int32_t seg = lw(pv + 0x70);
    float frac = lwc1(pv + 0x74);
    const int32_t at_end = path_advance_v(lvd::path(lw(pv + 0x78)), V(point), &seg, &frac, lh(pv + 0x8e), fpu_mul(lwc1(pv + 0xc8), lwc1(0x0015ed6cu)));
    sw(pv + 0x70, static_cast<uint32_t>(seg));
    swc1(pv + 0x74, frac);
    if ((lh(pv + 0x8e) == 0 && at_end != 0) || lwc1(m + 0x18) <= lwc1(L14_ADDR(0x0016228c))) {  // (0x003076b8)
        explosion_burst(m, m + 0x10, -1, 5.0f, 13.0f);
        sb(m + 0x20, 3);
        sw(m + 0x94, 0);
        blow_up(m, pv);
        return 0;
    }
    // ridden: the hero in his first riding state (0x25 / 0x26) on its carried moby, or in his second (0xe) on it
    const int32_t carried = lw(pv + 0xa0);
    const bool ridden = (lwu(HERO + 0x2084) - 0x25u < 2u && carried >= 0 && lwu(HERO + 0x964) == level_moby(carried)) ||
                        (lw(HERO + 0x208c) == 0xe && carried >= 0 && lwu(HERO + 0x994) == level_moby(carried));
    if (ridden) {  // (0x003077e4) faster, to the level's limit
        const float speed = fpu_add(lwc1(pv + 0xb4), fpu_mul(lwc1(pv + 0xb8), lwc1(0x0015ed64u)));
        const float limit = fpu_mul(lwc1(L14_ADDR(0x00162280)), lwc1(0x0015ed6cu));
        swc1(pv + 0xb4, limit < speed ? limit : speed);
        swc1(pv + 0xb0, fpu_add(lwc1(pv + 0xb0), lwc1(pv + 0xb4)));
    } else {  // (0x0030782c) slower, never below 0
        const float drop = fpu_sub(lwc1(pv + 0xb0), fpu_mul(lwc1(L14_ADDR(0x00162288)), lwc1(0x0015ed6cu)));
        sw(pv + 0xb4, 0);
        const float floor = lwc1(pv + 0xb4);
        swc1(pv + 0xb0, drop < floor ? floor : drop);
    }
    {  // (0x00307858) the point lowered by it, never below 10
        const float z = fpu_sub(lwc1(point + 8), lwc1(pv + 0xb0));
        swc1(point + 8, z < 10.0f ? 10.0f : z);
    }
    spring_to(m + 0x10, pv + 0x80, lwc1(point), lwc1(L14_ADDR(0x0016226c)), lwc1(L14_ADDR(0x00162270)), 0.0f);
    spring_to(m + 0x14, pv + 0x84, lwc1(point + 4), lwc1(L14_ADDR(0x0016226c)), lwc1(L14_ADDR(0x00162270)), 0.0f);
    spring_to(m + 0x18, pv + 0x88, lwc1(point + 8), lwc1(L14_ADDR(0x0016226c)), lwc1(L14_ADDR(0x00162270)), 0.0f);
    vec3_sub(V(way), V(point), V(m + 0x10));
    if (!(u2f(0x3c23d70au) < abs_f(lwc1(way)))) return 1;  // (0.01)
    if (!(u2f(0x3c23d70au) < abs_f(lwc1(way + 4)))) return 1;
    const float yaw = atan2_f(lwc1(way), lwc1(way + 4));
    float turn = angle_sub(lwc1(m + 0x48), yaw);
    yaw_turn_damped(m, pv + 0x98, yaw, lwc1(L14_ADDR(0x0016226c)), lwc1(L14_ADDR(0x00162270)), 0.0f);
    spring_to(m + 0x44, pv + 0x94, -atan2_f(vec2_length(V(way)), lwc1(way + 8)), lwc1(L14_ADDR(0x0016226c)), lwc1(L14_ADDR(0x00162270)), 0.0f);
    turn = fpu_div(turn, lwc1(L14_ADDR(0x00162278)));
    if (1.0f < turn) turn = 1.0f;
    if (turn < -1.0f) turn = -1.0f;
    spring_to(m + 0x40, pv + 0x90, fpu_mul(turn, lwc1(L14_ADDR(0x00162274))), u2f(0x3b449ba6u), lwc1(L14_ADDR(0x00162270)), 0.0f);  // (0.003)
    return 1;
}

// A rider on a type 7 camera: the camera told to follow the platform (its state block's +0x80 .. +0x8c, +0xa0, +0xb0).
// orig: L14:0x003135b8
void rider_camera(uint32_t m) {
    const uint32_t cam = lwu(L14_ADDR(0x00167500));  // (the current camera)
    if (lh(cam + 0x86) != 7) return;
    const uint32_t s = lwu(cam + 0x70) + 0x40;
    sw(s + 0x40, 1);
    sw(s + 0x44, m);
    swc1(s + 0x4c, 2.0f);
    swc1(lwu(cam + 0x70) + 0xb0, 2.0f);
    swc1(lwu(cam + 0x70) + 0xa0, 7.0f);
}
}  // namespace

// orig: L14:0x00306ee0
void olt_class1417_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x90);
    const uint32_t off = f.sp;
    {  // (the hit: of kind 0x800000, blown up unless already gone)
        const uint32_t hit = sub_0025a420(m, 0x00800000u, 0);
        sb(m + 0xa4, 0xff);
        if (hit != 0 && lbu(m + 0x20) != 3) {
            sb(m + 0x20, 3);
            explosion_burst(m, m + 0x10, -1, 5.0f, 13.0f);
            sw(m + 0x94, 0);
            blow_up(m, pv);
        }
    }
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x0030701c) set up on its path (none, or one with no point: gone); hidden until the hero is in its box
        if (lw(pv + 0x78) == -1 || lw(lvd::path(lw(pv + 0x78))) == 0) {
            debug_printf_stub();
            moby_retire(m);
            return;
        }
        path_setup(m);
        sb(m + 0x20, 2);
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0xff);
        sw(pv + 0x7c, 0xffffffffu);
        sh(pv + 0x8c, static_cast<uint32_t>(frame_scale_i(lw(L14_ADDR(0x0016227c)))));
        sw(pv + 0xb0, 0);
        sw(pv + 0xb4, 0);
        sw(pv + 0xbc, 0);
        sw(pv + 0xc0, 0);
        sw(pv + 0xcc, 0);
        if (lw(pv + 0xa0) >= 0) {
            const uint32_t o = level_moby(lw(pv + 0xa0));
            sh(o + 0x32, lhu(m + 0x32));
            sb(o + 0x30, lbu(m + 0x30));
        }
        mem_fill32(pv + 0xd0, 0, 0x40);
        sub_0024f590(m, 0, pv + 0xd0);
        color_pulse_start(m, pv + 0x60);
        if (lw(pv + 0xc4) < 0) break;
        sw(m + 0x94, 0);
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        if (lw(pv + 0xa0) >= 0) {
            const uint32_t o = level_moby(lw(pv + 0xa0));
            sh(o + 0x34, lhu(o + 0x34) | 0x41u);
        }
        sb(m + 0x20, 1);
        break;
    }
    case 1:  // (0x00307154) hidden; the hero in its box: shown, flying
        if (point_in_box(0x0013f3d0u, lw(pv + 0xc4)) == 0) break;
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        if (lw(pv + 0xa0) >= 0) {
            const uint32_t o = level_moby(lw(pv + 0xa0));
            sh(o + 0x34, lhu(o + 0x34) & 0xffbeu);
        }
        sb(m + 0x20, 2);
        break;
    case 2: {  // (0x003071b0) flying, the carried moby along; its rider's camera, the carried moby's value
        sb(pv + 0xd2, 1);
        swc1(pv + 0xf0, u2f(0x38d1b717u));  // (0.0001)
        swc1(pv + 0xf8, u2f(0x38d1b717u));
        swc1(pv + 0xf4, u2f(0x38d1b717u));
        sub_00262b80(m, pv + 0xbc, pv + 0xc0, lwc1(L14_ADDR(0x00162290)), lwc1(L14_ADDR(0x00162294)), lwc1(L14_ADDR(0x00162298)));
        if (fly(m) == 0) break;
        if (lw(pv + 0xa0) < 0) break;
        const uint32_t o = level_moby(lw(pv + 0xa0));
        lay_carried(m, pv, o, off);
        const int32_t hs = lw(0x001413dcu);  // (the hero's state)
        if ((hs == 0xd && lwu(HERO + 0x964) == o) || (hs == 0xe && lwu(HERO + 0x994) == o)) {
            rider_camera(m);
            sw(pv + 0xcc, static_cast<uint32_t>(frame_scale_i(0x1e)));
            break;
        }
        timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xcc)));
        const uint32_t opv = lwu(o + 0x78);
        if (lw(pv + 0xcc) != 0) {
            swc1(opv + 0x20, 2.0f);
            break;
        }
        sw(opv + 0x20, 0);
        if (lh(o + 0xa6) != 0x323) break;
        swc1(opv + 0x20, 40.0f);
        swc1(opv + 0x2c, fpu_mul(lwc1(0x0015ed6cu), 24.0f));
        swc1(opv + 0x24, u2f(0x3c23d70au));  // (0.01)
        swc1(opv + 0x28, u2f(0x3e4ccccdu));  // (0.2)
        break;
    }
    case 3: {  // (0x00307344) gone; after the level's delay back at the path's start, with its colour
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x8c))) == 0) break;
        const uint32_t path = lvd::path(lw(pv + 0x78));
        moby_set_color_hi(m, lbu(pv + 0x64), lbu(pv + 0x65), lbu(pv + 0x66));
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        sh(pv + 0x8c, static_cast<uint32_t>(frame_scale_i(lw(L14_ADDR(0x0016227c)))));
        sb(m + 0x20, 2);
        path_start(m, pv, path);
        if (lw(pv + 0xa0) >= 0) {
            const uint32_t o = level_moby(lw(pv + 0xa0));
            sh(o + 0x34, lhu(o + 0x34) & 0xffbeu);
            lay_carried(m, pv, o, off);
        }
        if (lw(pv + 0xc4) < 0) break;
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        sb(m + 0x20, 1);
        sw(m + 0x94, 0);
        break;
    }
    default:
        break;
    }
    // (0x003074a4) its sound while flying
    const uint32_t st = lbu(m + 0x20);
    if (st == 0 || st == 3 || st == 1) return;
    if (sub_0028d8c0(m, lw(pv + 0x7c)) != 0) return;
    sw(pv + 0x7c, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
}

}  // namespace rac1
