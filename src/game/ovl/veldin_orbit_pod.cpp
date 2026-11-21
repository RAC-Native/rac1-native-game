// Drek's fleet's (level 17) class 1448, ported by hand from the level's own code: a pod that carries Ratchet between its two
// ends (the boxes pvars +0x78 / +0x7c). It waits at one end; Ratchet standing on it (not in the air) gets its prompt
// (0x2f20d8) and the triangle starts the ride: the screen fades, the camera follows a path (pvars +0x60 / +0x64: its
// place, +0x68 / +0x6c: where it looks, +0x70 / +0x74: the pod's own path; the first or the second of each pair by the
// direction +0x98), Ratchet is put aside in the box +0x84 (state 0x72) while the pod rides (0x2f2180); at the other end he is
// put down beside it, and the pod's moby is noted done in the level's flags. Ratchet nearer its other end than the one it
// waits at: the pod moves there. The original's order of calls and float operations kept. Level 17 only: `orig: L17:`,
// level data through lvd::*, the level's own data L17_ADDR, base ELF bare. Registered in veldin_orbit_mobys.cpp.
//
// 1.02 Fixes, fix 39 (always on the PAL 2.00 disc, whose own build this is): the pod counts the frames since it came to
// rest (+0xb0) and offers its prompt (and takes the triangle) only past frame_scale_i(30) of them.
//
// The moby: +0x20 the state, +0x2c its scale (the model's +0x24 by the level's word L17:0x1623ac, or eased between the
// ride's two scales), +0x38 the hero's moby's 8 bytes there, +0xb2 its index (the level's done flags). The pvars: +0x20 its
// attach frame (attach_frame_set: what it carries), +0x80 / +0x94 / +0x98 the end it waits at, the side and the direction
// of the ride (0 / 1), +0x84 the box Ratchet waits in, +0x88 / +0x8c the ride's end turn and its speed, +0x9c / +0xa0 the
// ride's two scales (the level's words L17:0x1623ac / 0x1623a8), +0xa4 the ride's progress (0 .. 1), +0xa8 nonzero: the ride
// starts, +0xac nonzero: its arrival's sound is done, +0xb0 the frames at rest (fix 39 only).
//
// States: 0 set up (at the end +0x7c), 1 waiting, 2 riding, 3 arrived (then 1); the others do nothing.
#include <game/ovl/veldin_orbit_own.h>

#include <game/enhance/fixes102.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/hydro.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/region_core.h>
#include <game/ovl/screen_fade.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/veldin_orbit_mobys.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }   // the frame's time scale
float dt2() { return lwc1(0x0015ed70); }  // its square

constexpr uint32_t P = 0x0013f350;  // the hero (+0x80 his place, +0x2080 his moby, +0x2fc the moby he stands on, +0x30e
                                    // nonzero: in the air)

// an EE stack frame of the original's size (its locals are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// the pod's moby `m` noted done: its byte in the level's table, or its bit in the current level's flag words 0x14c190
bool done(uint32_t m) {
    const int32_t id = static_cast<int16_t>(lhu(m + 0xb2));
    if (lbu(L17_ADDR(0x001bbb30) + static_cast<uint32_t>(id) + 0x454) != 0) return true;
    const uint32_t word = lw(0x0014c190u + (lwu(0x0015ed84u) << 8) + (static_cast<uint32_t>(id >> 5) << 2));
    return ((static_cast<int32_t>(word) >> (lhu(m + 0xb2) & 0x1f)) & 1) != 0;
}

// the pod's moby put at the end `side` (its box +0x78 / +0x7c: the centre +0x30 and the angles +0x70)
void put_at(uint32_t m, uint32_t pv, uint32_t side) {
    const uint32_t b = lvd::box(lvd::BOXES, lw(pv + 0x78 + side * 4));
    lq_sq(m + 0x10, b + 0x30);
    lq_sq(m + 0x40, b + 0x70);
}

// the path `i` of the ride's table +0x60 / +0x68 / +0x70 for the direction `dir`
uint32_t ride_path(uint32_t pv, uint32_t table, int32_t dir) {
    return lvd::path(lw(pv + table + static_cast<uint32_t>(dir) * 4));
}
}  // namespace

// The pod's prompt for the end it waits at (+0x94 0 / 1 sets +0x80; the texts of the table L17:0x1e6f40).
// orig: L17:0x002f20d8
void vo_2f20d8_l17(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t side = lw(pv + 0x94);
    if (side == 0) sw(pv + 0x80, 0);
    else if (side == 1) sw(pv + 0x80, 1);
    // (the table L17:0x1e6f40, 5 entries)
    switch (lwu(pv + 0x80)) {
    case 0: hud_message_request(0xa, TXT(0x426c)); break;           // (0x002f212c)
    case 1: case 4: hud_message_request(0xa, TXT(0x426b)); break;   // (0x002f2140)
    case 2: hud_message_request(0xa, TXT(0x1786)); break;           // (0x002f2154)
    case 3: hud_message_request(0xa, TXT(0x1784)); break;           // (0x002f2168)
    default: break;
    }
}

// The ride for the direction `dir`: its progress +0xa4 eased toward 1 over frame_scale_i(900) frames; the camera's place
// along its path (+0x60), the pod along its own (+0x70: its yaw and pitch from the path's turns, turned by the end turn
// +0x88), the camera's angles toward its look path (+0x68), Ratchet kept in his box (+0x84), the scale eased between the
// ride's two (+0x9c / +0xa0), the end turn sought toward pi past 0.8; returns 1 at the ride's end.
// orig: L17:0x002f2180
int32_t vo_2f2180_l17(uint32_t m, int32_t dir) {
    const Frame f(256);
    const uint32_t sp = f.sp, look = sp, cam = sp + 0x10;  // (the original's: look's w lane the frame's)
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t cam_path = ride_path(pv, 0x60, dir), look_path = ride_path(pv, 0x68, dir), pod_path = ride_path(pv, 0x70, dir);
    approach_f(pv + 0xa4, 1.0f, fpu_div(1.0f, static_cast<float>(frame_scale_i(0x384))));
    const float ta = fpu_mul(lwc1(pv + 0xa4), static_cast<float>(lw(cam_path) - 1));
    const int32_t ia = float_to_int(ta);
    const float fia = static_cast<float>(ia);
    float tb = fpu_mul(lwc1(pv + 0xa4), u2f(0x3f828f5cu));  // (1.02: the look a little ahead)
    const float frac_a = fpu_sub(ta, fia);
    if (1.0f < tb) tb = 1.0f;
    const float tb_n = fpu_mul(tb, static_cast<float>(lw(look_path) - 1));
    const int32_t ib = float_to_int(tb_n);
    const float frac_b = fpu_sub(tb_n, static_cast<float>(ib));
    const float tc = fpu_mul(lwc1(pv + 0xa4), static_cast<float>(lw(pod_path) - 1));
    const float frac_c = fpu_sub(tc, fia);  // (the original takes the camera path's index here)
    const int32_t ic = float_to_int(tc);
    auto point = [](uint32_t path, int32_t k) { return path + 0x10 + static_cast<uint32_t>(k) * 0x10; };
    vec3_lerp(V(cam), V(point(cam_path, ia)), V(point(cam_path, ia + 1)), frac_a);
    vec3_lerp(V(m + 0x10), V(point(pod_path, ic)), V(point(pod_path, ic + 1)), frac_c);
    uid5_set_pvar_80(cam);
    const uint32_t hide = lvd::box(lvd::BOXES, lw(pv + 0x84));
    hero_place(hide + 0x30, hide + 0x70, 0x72, 0);
    if (ic > 0) {  // the pod's yaw and pitch between its path's turns at the points ic - 1, ic, ic + 1
        const uint32_t p0 = point(pod_path, ic - 1), p1 = point(pod_path, ic), p2 = point(pod_path, ic + 1);
        const float y0 = atan2_f(fpu_sub(lwc1(p1 + 0), lwc1(p0 + 0)), fpu_sub(lwc1(p1 + 4), lwc1(p0 + 4)));
        const float y1 = atan2_f(fpu_sub(lwc1(p2 + 0), lwc1(p1 + 0)), fpu_sub(lwc1(p2 + 4), lwc1(p1 + 4)));
        swc1(m + 0x48, angle_add(fpu_mul(angle_sub(y1, y0), frac_c), y0));
        const float a0 = -atan2_f(vec2_distance(V(p0), V(p1)), fpu_sub(lwc1(p1 + 8), lwc1(p0 + 8)));
        const float a1 = -atan2_f(vec2_distance(V(p1), V(p2)), fpu_sub(lwc1(p2 + 8), lwc1(p1 + 8)));
        swc1(m + 0x44, angle_add(fpu_mul(angle_sub(a1, a0), frac_c), a0));
        swc1(m + 0x44, fpu_mul(lwc1(m + 0x44), cos_vu(lwc1(pv + 0x88))));
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x88)));
    } else {  // at its start: along its first leg
        const uint32_t p0 = point(pod_path, 0), p1 = point(pod_path, 1);
        swc1(m + 0x44, -atan2_f(vec2_distance(V(p0), V(p1)), fpu_sub(lwc1(p1 + 8), lwc1(p0 + 8))));
        swc1(m + 0x48, atan2_f(fpu_sub(lwc1(p1 + 0), lwc1(p0 + 0)), fpu_sub(lwc1(p1 + 4), lwc1(p0 + 4))));
    }
    if (ia > 0 && ib > 0) {  // the camera's angles from its place toward the look path's points ib, ib + 1
        vec_clear(V(look));
        const uint32_t q0 = point(look_path, ib), q1 = point(look_path, ib + 1);
        const float y0 = atan2_f(fpu_sub(lwc1(q0 + 0), lwc1(cam + 0)), fpu_sub(lwc1(q0 + 4), lwc1(cam + 4)));
        const float y1 = atan2_f(fpu_sub(lwc1(q1 + 0), lwc1(cam + 0)), fpu_sub(lwc1(q1 + 4), lwc1(cam + 4)));
        swc1(look + 8, angle_add(fpu_mul(angle_sub(y1, y0), frac_b), y0));
        const float a0 = -atan2_f(vec2_distance(V(cam), V(q0)), fpu_sub(lwc1(q0 + 8), lwc1(cam + 8)));
        const float a1 = -atan2_f(vec2_distance(V(cam), V(q1)), fpu_sub(lwc1(q1 + 8), lwc1(cam + 8)));
        swc1(look + 4, angle_add(fpu_mul(angle_sub(a1, a0), frac_b), a0));
        uid5_set_pvar_90(look);
    }
    const float s0 = lwc1(pv + 0x9c);
    const float scale = fpu_sub(s0, fpu_mul(fpu_sub(s0, lwc1(pv + 0xa0)), lwc1(pv + 0xa4)));
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), scale));
    if (u2f(0x3f4ccccdu) < lwc1(pv + 0xa4)) {  // (0.8)
        const float acc = fpu_mul(dt2(), u2f(0x41490fdbu));  // (4 pi)
        yaw_seek(pv + 0x88, pv + 0x8c, u2f(0x40490fdbu), acc, acc, fpu_mul(dt(), u2f(0x40c90fdbu)));  // (pi, 2 pi)
    }
    return lwc1(pv + 0xa4) == 1.0f ? 1 : 0;
}

// orig: L17:0x002f1940
void vo_class1448_update_l17(uint32_t m) {
    const Frame f(272);
    const uint32_t sp = f.sp, old_pos = sp, old_ang = sp + 0x10, moved = sp + 0x20;
    const uint32_t pv = lwu(m + 0x78);
    const bool fix39 = fixes102_drek_pod_wait();
    lq_sq(old_pos, m + 0x10);
    lq_sq(old_ang, m + 0x40);
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(L17_ADDR(0x001623ac))));
    if (fix39) sw(pv + 0xb0, lwu(pv + 0xb0) + 1);

    switch (lbu(m + 0x20)) {
    case 0: {  // set up: at its end +0x7c, waiting (its ride's words 1)
        if (done(m)) {
            sw(pv + 0x94, 0);
            sw(pv + 0x80, 0);
            sw(pv + 0x98, 0);
        }
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0x80);
        const uint32_t hero_moby = lwu(P + 0x2080);
        sb(m + 0x20, 1);
        sd(m + 0x38, ld(hero_moby + 0x38));
        if (fix39) sw(pv + 0xb0, 0);
        if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, 0);
        sw(pv + 0x80, 1);
        sw(pv + 0x94, 1);
        sw(pv + 0x98, 1);
        put_at(m, pv, 1);
        break;
    }
    case 1: {  // waiting: Ratchet on it gets the prompt, the triangle starts the ride; nearer its other end: it moves there
        if (lbu(m + 0x53) == 2 && (lbu(m + 0x70) & 2) != 0 && lw(pv + 0xac) == 0) {
            moby_sound_start(1, 0, m);
            sw(pv + 0xac, 0);
        }
        if (lwu(P + 0x2fc) == m && lh(P + 0x30e) == 0 && (!fix39 || frame_scale_i(0x1e) < lw(pv + 0xb0))) {
            vo_2f20d8_l17(m);
            if ((lwu(0x0013cae4) & 0x10) != 0 && lw(L17_ADDR(0x0015f594)) == 0xa) {  // (the triangle, its prompt shown)
                const int32_t end = lw(pv + 0x80);
                if (end >= 0) {
                    sw(pv + 0xa8, 1);
                    if (end == 0) {
                        sw(pv + 0x94, 1);
                        swc1(pv + 0x9c, lwc1(L17_ADDR(0x001623ac)));
                        swc1(pv + 0xa0, lwc1(L17_ADDR(0x001623a8)));
                        sw(pv + 0x98, 0);
                    } else if (end == 1) {
                        sw(pv + 0x98, 1);
                        swc1(pv + 0x9c, lwc1(L17_ADDR(0x001623a8)));
                        swc1(pv + 0xa0, lwc1(L17_ADDR(0x001623ac)));
                        sw(pv + 0x94, 0);
                    }
                }
            }
        }
        if (lw(pv + 0xa8) != 0) {  // the ride starts: the fade, the camera at its path's start looking along its look path
            sw(pv + 0xa8, 0);
            hud_message_release(0xa);
            sb(m + 0x20, 2);
            moby_sound_start(2, 0, m);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 0);
            screen_fade(frame_scale_i(10));
            sw(pv + 0x88, 0);
            const int32_t dir = lw(pv + 0x98);
            const uint32_t at = sp + 0x50, to = sp + 0x30, ang = sp + 0x40;
            lq_sq(at, ride_path(pv, 0x60, dir) + 0x10);
            lq_sq(to, ride_path(pv, 0x68, dir) + 0x10);
            vec_clear(V(ang));
            swc1(ang + 8, atan2_f(fpu_sub(lwc1(to + 0), lwc1(at + 0)), fpu_sub(lwc1(to + 4), lwc1(at + 4))));
            swc1(ang + 4, -atan2_f(vec2_distance(V(at), V(to)), fpu_sub(lwc1(to + 8), lwc1(at + 8))));
            camera_scripted_set(at, ang, 2, static_cast<uint32_t>(frame_scale_i(0x12c)), 0);
            camera_angles_set(u2f(0x3a83126fu), 1.0f, 1.0f, u2f(0x3a83126fu), 1.0f, 1.0f);  // (0.001)
            uid5_set_pvar_80(at);
            uid5_set_pvar_90(ang);
            const uint32_t hide = lvd::box(lvd::BOXES, lw(pv + 0x84));
            hero_place(hide + 0x30, hide + 0x70, 0x72, 0);
            break;
        }
        if (!done(m)) break;
        const uint32_t side = lwu(pv + 0x94);
        const float here = vec2_distance(V(P + 0x80), V(lvd::box(lvd::BOXES, lw(pv + 0x78 + side * 4)) + 0x30));
        const float there = vec2_distance(V(P + 0x80), V(lvd::box(lvd::BOXES, lw(pv + 0x78 + (lwu(pv + 0x94) ^ 1) * 4)) + 0x30));
        if (!(there < here)) break;
        sw(pv + 0x80, lwu(pv + 0x80) ^ 1);
        sw(pv + 0x98, lwu(pv + 0x98) ^ 1);
        sw(pv + 0x94, lwu(pv + 0x94) ^ 1);
        put_at(m, pv, lwu(pv + 0x94));
        euler_to_mat3_vu(M(m + 0xc0), V(m + 0x40));
        swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24));
        break;
    }
    case 2: {  // riding; at the end: put at the end +0x94, Ratchet set down 2 behind it, its moby noted done
        if (vo_2f2180_l17(m, lw(pv + 0x98)) == 0) break;
        sb(m + 0x20, 3);
        sw(pv + 0xa4, 0);
        put_at(m, pv, lwu(pv + 0x94));
        euler_to_mat3_vu(M(m + 0xc0), V(m + 0x40));
        swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24));
        const uint32_t at = sp + 0x30, back = sp + 0x50, ang = sp + 0x40;
        lq_sq(at, m + 0x10);
        vec3_set_length(V(back), V(m + 0xc0), -2.0f);
        vec3_add(V(back), V(back), V(m + 0xe0));
        vec3_add(V(at), V(at), V(back));
        vec_clear(V(ang));
        swc1(ang + 8, angle_add(lwc1(m + 0x48), u2f(0x40490fdbu)));
        hero_place(at, ang, 0, 0);
        hydro_2eac18(0);
        const uint32_t idx = lhu(m + 0xb2);
        const uint32_t word = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(idx)) >> 5) << 2;
        const uint32_t bit = 1u << (idx & 0x1f);
        const uint32_t flags = 0x0014c190u + (lwu(0x0015ed84u) << 8) + word;
        sw(flags, lwu(flags) | bit);
        sw(L17_ADDR(0x001badd0) + word, lwu(L17_ADDR(0x001badd0) + word) | bit);
        break;
    }
    case 3:  // arrived: waiting again
        sb(m + 0x20, 1);
        if (fix39) sw(pv + 0xb0, 0);
        if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, frame_scale_i(0x1e));
        moby_sound_start(0, 0, m);
        sw(pv + 0xac, 1);
        break;
    default:
        break;
    }
    // what it carries moved with it
    vec3_sub(V(moved), V(m + 0x10), V(old_pos));
    attach_frame_set(pv + 0x20, moved, old_ang, m + 0x40);
}

}  // namespace rac1
