// The class-1440 enemy of Veldin (update 0x2e0b88) and the helpers its chain needs.
#include <cmath>
#include <game/ovl/ovl_reloc.h>
#include <cstring>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>

namespace rac1 {

// A moby's +0x70 (a draw scale) set to f when the flag 0x15edb7 allows it, else 1.
// orig: 0x00263b38
void moby_set_70(uint32_t m, float f) { swc1(m + 0x70, lbu(0x0015edb7) != 0 ? f : 1.0f); }

// Releases the class-1471 effect slot holding value v (0x2e20f8's work inline): 1 if found.
// orig: 0x002e2040
int32_t fx1471_release(uint32_t v) {
    for (uint32_t i = 0; i < 3; ++i) {
        const uint32_t o = i * 4;
        if (lwu(OV(0x00161c88) + o) != v) continue;
        const int32_t r = lw(OV(0x00161c38) + o);
        sw(OV(0x00161bf8) + o, 0);
        sw(OV(0x00161c88) + o, 0);
        if (r != -1) {
            region_clear(static_cast<uint32_t>(r));
            sw(OV(0x00161c38) + o, 0xffffffffu);
        }
        if (lw(OV(0x00161c48) + o) == -1) return 1;
        region_clear(lwu(OV(0x00161c48) + o));
        sw(OV(0x00161c48) + o, 0xffffffffu);
        return 1;
    }
    return 0;
}

// Updates the class-1471 effect slot holding value v: idle (1) -> starting 2, ending (4) -> 3;
// its matrix (0x1e6d20[i], 0x40 bytes from mat) and vector (0x1e6de0[i]). 1 if found.
// orig: 0x002e2190
int32_t fx1471_set_v(uint32_t v, const Mat4& mat, const Vec4& vec) {
    for (uint32_t i = 0; i < 3; ++i) {
        if (lwu(OV(0x00161c88) + i * 4) != v) continue;
        const uint32_t s = OV(0x00161bf8) + i * 4;
        if (lw(s) == 1) sw(s, 2);
        else if (lw(s) == 4) sw(s, 3);
        M(OV(0x001e6d20) + i * 0x40) = mat;
        V(OV(0x001e6de0) + i * 0x10) = vec;
        return 1;
    }
    return 0;
}
// orig: 0x002e2190 (EE-address form)
int32_t fx1471_set(uint32_t v, uint32_t mat, uint32_t vec) { return fx1471_set_v(v, M(mat), V(vec)); }

// A random vector of length lo..hi in a random direction (two random angles).
// orig: 0x00257d00
void rand_dir_scaled_v(Vec4& d, float lo, float hi) {
    const float a = rand_angle();
    const float b = rand_angle();
    vec_from_spherical_v(d, rand_range_f(lo, hi), a, b);
}
// orig: 0x00257d00 (EE-address form)
void rand_dir_scaled(uint32_t d, float lo, float hi) { rand_dir_scaled_v(V(d), lo, hi); }

// A kind 0x45 particle at `pos` (drifting along `vel`): alpha `a` (+4 top byte, also +0x34),
// grey 0x7f7f7f, size 1000..50000, random phase, its life 10..30 frames (+0xa, 1/life +0x30),
// `w` at +0x3c. Returns the particle (0 when none was free).
// orig: 0x00274948
uint32_t spawn_particle_45_v(const Vec4& pos, const Vec4& vel, uint32_t a, uint32_t w) {
    const uint32_t p = alloc_particle(0x45);
    if (p == 0) return 0;
    V(p + 0x10) = pos;
    const int32_t s = float_to_int(4.0f);
    sb(p + 1, 0);
    sb(p + 3, 0x48);
    sb(p + 9, static_cast<uint32_t>(s + 0x70));
    swc1(p + 0xc, rand_range_f(u2f(0x45fa0000u), u2f(0x47435000u)));
    sb(p + 8, static_cast<uint32_t>(rand_mod(0x100)));
    sb(p + 2, lbu(lwu(OV(0x001b2194))));
    const int32_t life = frame_scale_i(rand_range(10, 0x1e));
    sh(p + 0xa, static_cast<uint32_t>(life));
    const float n = int_to_float(static_cast<int16_t>(life));
    const uint32_t q = p + 0x20;
    sw(q + 0x1c, w);
    sh(q + 0x14, a);
    sw(q + 0x18, 0x007f7f7fu);
    sh(q + 0x16, 0);
    swc1(q + 0x10, fpu_div(1.0f, n));
    V(q) = vel;
    sw(p + 4, (a << 24) | 0x007f7f7fu);
    return p;
}
// orig: 0x00274948 (EE-address form)
uint32_t spawn_particle_45(uint32_t pos, uint32_t vel, uint32_t a, uint32_t w) { return spawn_particle_45_v(V(pos), V(vel), a, w); }

// The class-1440 enemy's muzzle flash: at its chain point 1 (pvars +0x1c0, lowered by 0.333)
// a drifting kind 0x45 particle (size 6000..32000) and three still ones (size 80000..120000,
// the third one time in 8 180000) of 2 frames, and the draw callback 0x2e1c78.
// orig: 0x002e1aa8
void e1440_flash(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t at = pv + 0x1c0;
    moby_chain_point_v(m, 1, V(at));
    swc1(pv + 0x1c8, lwc1(pv + 0x1c8) - u2f(0x3eaa7efau));
    Vec4 drift = Vec4{};  // w: a stack leftover in the original (0 here)
    rand_dir_scaled_v(drift, u2f(0x3ba3d70au), u2f(0x3cf5c28fu));
    const uint32_t p0 = spawn_particle_45_v(V(at), drift, 0x7f, m);
    if (p0 != 0) swc1(p0 + 0xc, rand_range_f(u2f(0x45bb8000u), u2f(0x46fa0000u)));
    for (int32_t i = 0; i < 3; ++i) {
        const uint32_t p = spawn_particle_45_v(V(at), V(OV(0x0015f580)), 0x7f, m);
        if (p == 0) continue;
        if (i == 2 && rand_mod(8) == 0) {
            swc1(p + 0xc, u2f(0x482fc800u));
        } else {
            swc1(p + 0xc, rand_range_f(u2f(0x479c4000u), u2f(0x47ea6000u)));
        }
        const int32_t life = frame_scale_i(2);
        sh(p + 0xa, static_cast<uint32_t>(life));
        const float n = int_to_float(static_cast<int16_t>(life));
        sh(p + 0x36, 3);
        sw(p + 0x38, 0x007f7f7fu);
        swc1(p + 0x30, fpu_div(1.0f, n));
    }
    draw_callback_add(OVF(0x002e1c78), m);
}

// Claims a class-1471 effect slot for value v (1 if it already has one): the first free slot
// gets v, a / b (0x161c58 / 0x161c68), state 1 and its other fields reset. 0 when all are taken.
// orig: 0x002e1f28
int32_t fx1471_claim(uint32_t v, uint32_t a, uint32_t b) {
    for (uint32_t i = 0; i < 3; ++i)
        if (lwu(OV(0x00161c88) + i * 4) == v) return 1;
    for (uint32_t i = 0; i < 3; ++i) {
        const uint32_t o = i * 4;
        if (lw(OV(0x00161c88) + o) != 0) continue;
        sw(OV(0x00161c88) + o, v);
        sw(OV(0x00161c48) + o, 0xffffffffu);
        sw(OV(0x00161c58) + o, a);
        sw(OV(0x00161c68) + o, b);
        sw(OV(0x00161bf8) + o, 1);
        sw(OV(0x00161c08) + o, 0);
        sw(OV(0x00161c18) + o, 0);
        sw(OV(0x00161c28) + o, 0);
        sw(OV(0x00161c38) + o, 0xffffffffu);
        sw(OV(0x00161c78) + o, 0);
        return 1;
    }
    return 0;
}

// A joint node of moby m (joint idx) springing its angles +0x40.. (spin +0x50..) towards the
// targets +0x60.. (spring k, damping d) with its scale +0x70: when anything is to do (a target,
// a scale other than 1, an angle 0.005 or more off) it is (re)attached (0x24f590), its rotation
// +0x10 set from the angles, its scale +0x20..+0x28 from +0x70, and the targets / scale reset;
// otherwise an attached node is released (0x24f608). A node of another moby is released first.
// orig: 0x002628d8
void joint_node_spring(uint32_t m, uint32_t node, uint32_t idx, float k, float d) {
    const uint32_t cur = lwu(node + 0x78);
    if (cur != 0 && cur != m) {
        if (lbu(node + 1) != 0 && lbu(cur + 0x20) < 0x7f) sub_0024f608(cur, node);
        sb(node + 1, 0);
    }
    sw(node + 0x78, m);
    const float lim = u2f(0x3ba3d70au);
    const bool idle = lwc1(node + 0x60) == 0.0f && lwc1(node + 0x64) == 0.0f && lwc1(node + 0x68) == 0.0f && lwc1(node + 0x70) == 1.0f &&
                      abs_f(lwc1(node + 0x40)) < lim && abs_f(lwc1(node + 0x44)) < lim && abs_f(lwc1(node + 0x48)) < lim;
    if (idle) {
        if (lbu(node + 1) != 0) sub_0024f608(m, node);
        return;
    }
    for (uint32_t i = 0; i < 3; ++i) {
        float& ang = *static_cast<float*>(ee_ram_ptr(node + 0x40 + i * 4));
        spring_angle_v(ang, node + 0x50 + i * 4, 0, lwc1(node + 0x60 + i * 4), k, d, 0.0f);
    }
    if (lbu(node + 1) == 0) sub_0024f590(m, idx, node);
    quat_from_euler_v(V(node + 0x10), V(node + 0x40));
    const float sc = lwc1(node + 0x70);
    swc1(node + 0x28, sc);
    swc1(node + 0x20, sc);
    swc1(node + 0x24, sc);
    vec_clear(V(node + 0x60));
    swc1(node + 0x70, 1.0f);
}

// Is the point p (x / y) inside the polygon of n corners (16 bytes apart from v)? Even-odd
// rule on the crossings of the horizontal through p left of it.
// orig: 0x00259740
int32_t point_in_polygon(uint32_t p, uint32_t v, int32_t n) {
    int32_t in = 0;
    const float px = lwc1(p), py = lwc1(p + 4);
    int32_t j = 0;
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t a = v + static_cast<uint32_t>(i) * 0x10;
        if (++j == n) j = 0;
        const uint32_t b = v + static_cast<uint32_t>(j) * 0x10;
        const float ay = lwc1(a + 4), by = lwc1(b + 4);
        if (!((ay < py && py <= by) || (by < py && py <= ay))) continue;
        const float x = lwc1(a) + fpu_div(py - ay, by - ay) * (lwc1(b) - lwc1(a));
        if (x < px) in = in == 0 ? 1 : 0;
    }
    return in;
}

// A direction vector's yaw (atan2 of x / y) into yaw; when its w is the marker 5627.925 its
// flat length scales a and its z scales b too.
// orig: 0x0025ab48
void vec_yaw_marked_v(const Vec4& v, float& yaw, float& a, float& b) {
    const Vec4 q = v;
    if (q.w != u2f(0x45afdf66u)) {
        yaw = atan2_f(q.x, q.y);
        return;
    }
    yaw = atan2_f(q.x, q.y);
    const float len = vec2_length(q);
    a = a * len;
    b = b * q.z;
}
// orig: 0x0025ab48 (EE-address form)
void vec_yaw_marked(uint32_t v, uint32_t yaw, uint32_t a, uint32_t b) {
    auto f = [](uint32_t x) -> float& { return *static_cast<float*>(ee_ram_ptr(x)); };
    vec_yaw_marked_v(V(v), f(yaw), f(a), f(b));
}

// moby_anim_set with flags: set `set` from frame `frame` (not clamped) blended over `frames`;
// the current pose captured (0x251ce8; flags 1 / 2 add 0x100 / 0x200) when it is mid-blend or
// flags 4 asks for it.
// orig: 0x002579c8
void moby_anim_set_flags(uint32_t m, uint32_t set, uint32_t frame, int32_t frames, uint32_t flags) {
    const bool capture = u2f(0x3ccccccdu) < lwc1(m + 0x54) || lw(m + 0x60) != 0 || lw(m + 0x64) != 0 || (flags & 4u) != 0;
    if (capture) {
        const int32_t slot = sub_0024f698(m);
        if (slot >= 0) {
            uint32_t f = static_cast<uint32_t>(slot);
            if ((flags & 1u) != 0) f |= 0x100u;
            if ((flags & 2u) != 0) f |= 0x200u;
            moby_pose_capture(m, f);
            lq_sq(OV(0x00196d00u) + static_cast<uint32_t>(slot) * 16, m + 0xf0);
            const uint32_t prev = lbu(m + 0x52);
            if (prev != 0xff) sb(m + 0xa5, prev);
            sb(m + 0x52, 0xff);
            sb(m + 0x50, static_cast<uint32_t>(slot));
        }
    }
    sb(m + 0x51, frame);
    sb(m + 0x53, set);
    sub_0024ebc0(m);
    swc1(m + 0x58, 1.0f);
    const float step = fpu_div(1.0f, int_to_float(frames));
    sw(m + 0x54, 0);
    sb(m + 0x70, lbu(m + 0x70) & 0xfdu);
    swc1(m + 0x5c, step);
    sb(m + 0x7c, lbu(lwu(lwu(m + 0x24) + (set & 0xffu) * 4 + 0x48) + 0x11));
}

// Starts a move state record `rec` of moby m: heading `ang` (velocity +0 / +4 at speed +0x18,
// +8 = +0x1c, +0xc = 1), facing back +0x40 = ang + pi, timer +0x2c of 300 frames, +0x3c = 1;
// when the moby's pvar word 0 is zero +0x24 gets 0x20 and m+0x94 is cleared. Then animation
// set `set` (frame `frame`, over `frames`; with a pose capture when +0x24 has 2).
// orig: 0x0025c558
void move_state_start(uint32_t m, uint32_t rec, uint32_t set, int32_t frames, uint32_t frame, float ang) {
    swc1(rec, cos_vu(ang) * lwc1(rec + 0x18));
    const float s = sin_vu(ang);
    swc1(rec + 8, lwc1(rec + 0x1c));
    swc1(rec + 0xc, 1.0f);
    swc1(rec + 4, s * lwc1(rec + 0x18));
    swc1(rec + 0x40, angle_add(ang, u2f(0x40490fdbu)));
    sw(rec + 0x44, 0);
    const int32_t t = frame_scale_i(300);
    sh(rec + 0x3e, 0);
    sh(rec + 0x2c, static_cast<uint32_t>(t));
    sb(rec + 0x3c, 1);
    const uint32_t pw = moby_pvar_word0(m);
    if (pw != 0 && lwc1(pw) == 0.0f) {
        sw(rec + 0x24, lwu(rec + 0x24) | 0x20u);
        sw(m + 0x94, 0);
    }
    if ((lwu(rec + 0x24) & 2u) != 0) {
        moby_anim_set_flags(m, set, frame, frames, 1);
    } else {
        moby_anim_set(m, set, static_cast<int32_t>(frame), frames);
    }
}

// Starts (or restarts) a colour pulse record `rec` (timer +0, phase +2, base colour +4..+6,
// pulse colour +7..+9, frames in +0xc / out +0xe): a fresh one takes the moby's colour as its
// base; restarted mid-way the timer keeps the elapsed fraction (at least 1).
// orig: 0x0025d458
void color_pulse_start(uint32_t m, uint32_t rec) {
    const int32_t t = lh(rec);
    if (t != 0 && lh(rec + 2) == 0) return;
    sh(rec, static_cast<uint32_t>(frame_scale_i(lh(rec + 0xc))));
    sh(rec + 2, 0);
    if (t == 0) {
        uint32_t r = 0, g = 0, b = 0;
        moby_get_color_hi_v(m, &r, &g, &b);
        sb(rec + 4, r & 0xffu);
        sb(rec + 6, b & 0xffu);
        sb(rec + 5, g & 0xffu);
        return;
    }
    const int32_t in = frame_scale_i(lh(rec + 0xc));
    const float out = int_to_float(frame_scale_i(lh(rec + 0xe)));
    const int32_t v = float_to_int(static_cast<float>(in) * fpu_div(static_cast<float>(t), out));
    sh(rec, static_cast<uint32_t>(v));
    if (static_cast<int16_t>(v) < 1) sh(rec, 1);
}

// A colour pulse's frame (see 0x25d458): towards the pulse colour (the channels it has) over
// the in frames, then back to the base over the out frames, then the base colour set.
// orig: 0x0025d538
void color_pulse_update(uint32_t m, uint32_t rec) {
    if (lh(rec) == 0) return;
    int16_t t = static_cast<int16_t>(lh(rec));
    const int32_t done = timer_tick_i16(t);
    sh(rec, static_cast<uint16_t>(t));
    if (done != 0) {
        if (lh(rec + 2) != 0) {
            moby_set_color_hi(m, lbu(rec + 4), lbu(rec + 5), lbu(rec + 6));
            return;
        }
        sh(rec + 2, 1);
        sh(rec, static_cast<uint32_t>(frame_scale_i(lh(rec + 0xe))));
    }
    const bool back = lh(rec + 2) != 0;
    const float n = int_to_float(frame_scale_i(lh(rec + (back ? 0xe : 0xc))));
    const float k = fpu_div(n - static_cast<float>(lh(rec)), n);
    uint32_t c[3];
    for (uint32_t i = 0; i < 3; ++i) {
        const uint32_t base = lbu(rec + 4 + i), tint = lbu(rec + 7 + i);
        if (tint == 0) {
            c[i] = base;
            continue;
        }
        const uint32_t from = back ? tint : base, to = back ? base : tint;
        const int32_t d = static_cast<int32_t>(to - from);
        c[i] = static_cast<uint32_t>(float_to_int(static_cast<float>(static_cast<int32_t>(from)) + static_cast<float>(d) * k));
    }
    moby_set_color_hi(m, c[0], c[1], c[2]);
}

// An enemy's target (record `out`): the hero unless hidden (P+0x208c 0x18, state 0x72) or out
// of the enemy's area (in one of the first nbox-1 boxes 0x25f960, or outside the polygon of
// npoly corners), else the nearest within `range` (flat) of the listed mobys 0x1b0830 of class
// 0xcb / 0x76c in state 3 and the 20 slots 0x1dd100 of class 0x10e with +0xbc set, in the
// area too. out: +0 its position, +0x10 its angles, +0x20 / +0x30 two aim points (the hero's
// rows +0xc0 / +0xd0, or the position 0.6 higher), +0x40 the moby, +0x44 the result: 0 the
// hero, 1 another moby, 2 none.
// orig: 0x0025ff38
int32_t target_select(uint32_t self, uint32_t out, uint32_t boxes, int32_t nbox, uint32_t poly, int32_t npoly, float range) {
    constexpr uint32_t P = 0x0013f350;
    uint32_t best = lwu(P + 0x2080);
    int32_t status = 0;
    auto in_area = [&](uint32_t pos) {
        if (nbox > 0) {
            for (int32_t i = 0; i < nbox - 1; ++i)
                if (point_in_box(pos, lw(boxes + static_cast<uint32_t>(i) * 4)) != 0) return false;
            return true;
        }
        if (poly != 0 && npoly > 0) return point_in_polygon(pos, poly, npoly) != 0;
        return true;
    };
    if (lw(P + 0x208c) == 0x18 || lw(P + 0x2084) == 0x72 || !in_area(0x0013f3d0)) {
        status = 2;
        best = 0;
    }
    auto consider = [&](uint32_t c) {
        const float d = vec2_distance(V(self + 0x10), V(c + 0x10));
        if (range <= d || !in_area(c + 0x10)) return;
        best = c;
        status = 1;
        range = d;
    };
    for (int32_t i = 1; i <= lh(OV(0x001b0830)); ++i) {
        const uint32_t c = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(lh(OV(0x001b0830) + static_cast<uint32_t>(i) * 2)) << 8);
        if (c == 0) continue;
        const int32_t cls = lh(c + 0xa6);
        if (cls != 0xcb && cls != 0x76c) continue;
        if (lbu(c + 0x20) != 3) continue;
        consider(c);
    }
    for (uint32_t i = 0; i < 0x14; ++i) {
        const uint32_t c = lwu(OV(0x001dd100) + i * 4);
        if (c == 0 || lh(c + 0xa6) != 0x10e) continue;
        const uint32_t st = lbu(c + 0x20);
        if (st == 0xfe || st == 0xfd || lbu(c + 0xbc) == 0) continue;
        consider(c);
    }
    if (best != 0) {
        sw(out + 0x40, best);
        lq_sq(out, best + 0x10);
        lq_sq(out + 0x10, best + 0x40);
        if (best == lwu(P + 0x2080)) {
            lq_sq(out + 0x20, P + 0xc0);
            lq_sq(out + 0x30, P + 0xd0);
        } else {
            lq_sq(out + 0x30, out);
            swc1(out + 0x38, lwc1(out + 0x38) + u2f(0x3f19999au));
            lq_sq(out + 0x20, out + 0x30);
        }
    } else {
        sw(out + 0x40, 0);
        for (uint32_t o = 0; o < 0x40; o += 4) sw(out + o, 0);
    }
    sw(out + 0x44, static_cast<uint32_t>(status));
    return status;
}

// target_select without an area: the hero unless hidden, else the nearest listed moby within
// `range` (the same work as 0x25ff38 with no boxes or polygon).
// orig: 0x0025fcb8
int32_t target_select_any(uint32_t self, uint32_t out, float range) { return target_select(self, out, 0, 0, 0, 0, range); }

// Moves *pos towards `target` with a speed *vel that accelerates by `accel` up to `vmax`
// (sqrt(2 accel distance)), changing by `turn` per frame, and brakes (by accel, 1.1 accel when
// it can still stop) to land on the target; moving away it first brakes to 0. Returns the step.
// orig: 0x0025b970
float seek_1d_v(float& pos, float& v, float target, float turn, float accel, float vmax) {
    const float e = target - pos;
    if (!(0.0f <= fpu_mul(v, e)) || e == 0.0f) {
        approach_f_v(v, 0.0f, accel);
        pos = pos + v;
        return v;
    }
    const float stop = fpu_div(v * v, accel) * 0.5f;
    if (abs_f(e) < stop) {
        const float sum = abs_f(e) + abs_f(v);
        if (stop < sum) {
            approach_f_v(v, 0.0f, accel);
        } else {
            approach_f_v(v, 0.0f, accel * u2f(0x3f8ccccdu));
        }
    } else {
        float sp = sqrt_f((accel + accel) * e);
        if (vmax < sp) sp = vmax;
        approach_f_v(v, e < 0.0f ? -sp : sp, turn);
    }
    if (abs_f(v) < abs_f(e)) {
        pos = pos + v;
        return v;
    }
    pos = target;
    return e;
}
// orig: 0x0025b970 (EE-address form)
float seek_1d(uint32_t pos, uint32_t vel, float target, float turn, float accel, float vmax) {
    return seek_1d_v(*static_cast<float*>(ee_ram_ptr(pos)), *static_cast<float*>(ee_ram_ptr(vel)), target, turn, accel, vmax);
}

// A debris moby of class `cls` broken off `src` (scaled like it, its colour +0x38): at `pos`
// turned by `ang`, always drawn, pvars: velocity +0 (`vel`, or a random throw 2..4 out and
// 5..8 up a second), spin +0x10 (`spin`, or a random one of *(gp-0x7e94) * pi..2pi), size
// +0x20 (`size` when it has length or w, else the class's bounding sphere +0x30 at the moby's
// scale), life +0x30 (`life` frames, or 60..120 frames), +0x34 = flags, gravity +0x38 (`grav`
// on a pvar block that already had one, else 10..15 a frame squared).
// orig: 0x00263fd8
uint32_t debris_spawn_v(uint32_t src, uint32_t cls, const Vec4& pos, const Vec4& ang, int32_t life, uint32_t flags, const Vec4& vel, const Vec4& spin,
                        const Vec4& size, float grav) {
    const uint32_t c = moby_spawn(cls);
    if (c == 0) return 0;
    const float k = fpu_div(lwc1(src + 0x2c), lwc1(lwu(src + 0x24) + 0x24));
    sb(c + 0x31, 1);
    sh(c + 0x34, lhu(c + 0x34) | 0x100u);
    const uint32_t pv = lwu(c + 0x78);
    swc1(c + 0x2c, lwc1(lwu(c + 0x24) + 0x24) * k);
    sb(c + 0x20, 1);
    sd(c + 0x38, ld(src + 0x38));
    V(c + 0x10) = pos;
    euler_to_mat3_vu(M(c + 0xc0), ang);
    sb(c + 0x30, 0xff);
    sh(c + 0x32, 0x80);
    sw(c + 0x94, 0);
    if (life == 0) {
        sw(pv + 0x30, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(60.0f, 120.0f)))));
    } else {
        sw(pv + 0x30, static_cast<uint32_t>(life));
    }
    sw(pv + 0x34, flags);
    if (lwc1(pv + 0x38) == 0.0f) {
        swc1(pv + 0x38, rand_range_f(10.0f, 15.0f) * lwc1(0x0015ed70));
    } else {
        swc1(pv + 0x38, grav);
    }
    Vec4 tmp = Vec4{};  // a stack quad in the original (its w a leftover, 0 here: pvars +0xc / +0x1c; the pieces' update does
                        // not read them, batch 129 deals them at random, #145)
    if (vec3_length(vel) == 0.0f) {
        const float a = rand_angle();
        const float sp = rand_range_f(2.0f, 4.0f) * lwc1(0x0015ed6c);
        tmp.x = cos_vu(a) * sp;
        tmp.y = sin_vu(a) * sp;
        tmp.z = rand_range_f(5.0f, 8.0f) * lwc1(0x0015ed6c);
        V(pv) = tmp;
    } else {
        V(pv) = vel;
    }
    if (vec3_length(spin) == 0.0f) {
        const float g = lwc1(OV(0x00166c00u - 0x7e94));
        rand_sphere_point_v(tmp, g * u2f(0x40490fdbu), g * u2f(0x40c90fdbu));
        V(pv + 0x10) = tmp;
    } else {
        V(pv + 0x10) = spin;
    }
    if (vec3_length(size) == 0.0f && size.w == 0.0f) {
        tmp = V(lwu(c + 0x24) + 0x30);
        const float q = lwc1(c + 0x2c) * u2f(0x3a800000u);
        vec3_scale(tmp, tmp, q);
        tmp.w = tmp.w * q;
        V(pv + 0x20) = tmp;
    } else {
        V(pv + 0x20) = size;
    }
    moby_update_transform(c);
    return c;
}
// orig: 0x00263fd8 (EE-address form)
uint32_t debris_spawn(uint32_t src, uint32_t cls, uint32_t pos, uint32_t ang, int32_t life, uint32_t flags, uint32_t vel, uint32_t spin, uint32_t size, float grav) {
    return debris_spawn_v(src, cls, V(pos), V(ang), life, flags, V(vel), V(spin), V(size), grav);
}

// A moby breaking into debris: n0 pieces of classes cls0, cls0 + 1, .. at its position, then
// up to n1 pieces of random classes cls1 + 0..nrand-1 at points spread through its volume
// (0x25aca0, 1000 a unit), randomly turned. `stay` keeps them (flags 2).
// orig: 0x00263e30
void debris_burst(uint32_t m, int32_t cls0, int32_t n0, int32_t cls1, int32_t nrand, int32_t n1, uint32_t stay) {
    const uint32_t flags = stay != 0 ? 2u : 0u;
    const Vec4& zero = V(OV(0x0015f580));
    if (cls0 <= 0) return;
    for (int32_t i = 0; i < n0; ++i)
        debris_spawn_v(m, static_cast<uint32_t>(cls0 + i), V(m + 0x10), V(m + 0x40), 0, flags, zero, zero, zero, 0.0f);
    if (nrand <= 0) return;
    Vec4 pts[64];  // 16 on the original's stack
    const int32_t n = moby_volume_points(1000.0f, m, n1, pts, 0xffff);
    if (n == 0 || n1 <= 0) return;
    for (int32_t i = 0; i < n1 && i < n; ++i) {
        Vec4 ang;
        vec_clear(ang);
        ang.x = rand_angle();
        ang.y = rand_angle();
        ang.z = rand_angle();
        const int32_t k = rand_mod(nrand);
        debris_spawn_v(m, static_cast<uint32_t>(cls1 + k), pts[i], ang, 0, flags, zero, zero, zero, 0.0f);
    }
}

// An explosion at `pos`: three sparks (colour pairs 0x1e9030 / 0x1e9048) of size `scale`, two
// orange shards off `src` (if any), a camera shake (0x166de0 = scale / 10 for 20 frames) when
// on screen, the sound `snd` on `src` (not -1, src alive) and the blast effect 0x1b02f0 sized
// `size` (none at 0; 13 when negative).
// orig: 0x0025f090
void explosion_burst(uint32_t src, uint32_t pos, int32_t snd, float scale, float size) {
    Vec4 still;
    vec_clear(still);
    for (int32_t i = 0; i < 3; ++i) {
        const float fx = scale * 400000.0f;
        const float sp = rand_range_f(8.0f, 10.0f) * lwc1(0x0015ed6c);
        const uint32_t c0 = lwu(OV(0x001e9030) + static_cast<uint32_t>(rand_mod(6)) * 4);
        const uint32_t c1 = lwu(OV(0x001e9048) + static_cast<uint32_t>(rand_mod(6)) * 4);
        const int32_t a = frame_scale_i(0xf);
        const int32_t life = rand_range(a, frame_scale_i(0x14));
        const int32_t b = frame_scale_i(0x19);
        const int32_t fade = rand_range(b, frame_scale_i(0x1e));
        spawn_particle_0b_v(V(pos), still, c0, c1, static_cast<uint32_t>(life), static_cast<uint32_t>(fade), 0, 0, fx, sp * scale);
    }
    if (src != 0) {
        shard_spawn_v(src, V(pos), still, static_cast<uint32_t>(frame_scale_i(0x14)), 0x7f, 0x40, 0, 0x30, scale * 4.0f);
        shard_spawn_v(src, V(pos), still, static_cast<uint32_t>(frame_scale_i(0x1d)), 0x60, 0x20, 0, 0x20, scale * 3.0f);
    }
    Vec4 at = V(pos);
    at.w = 2.0f;
    if (point_on_screen_v(at, 10.0f) != -1) {
        swc1(OV(0x00166de0), scale * u2f(0x3dcccccdu));
        sw(OV(0x00166de8), static_cast<uint32_t>(frame_scale_i(0x14)));
    }
    if (src != 0) {
        const uint32_t st = lbu(src + 0x20);
        if (st != 0xfe && st != 0xfd && snd != -1) moby_sound_start(snd, 0, src);
    }
    if (size == 0.0f) return;
    const float sz = 0.0f < size ? size : 13.0f;
    swc1(OV(0x001b02f0) + 0x20, sz);
    swc1(OV(0x001b02f0) + 0x24, sz);
    swc1(OV(0x001b02f0) + 0x28, sz);
    effect_spawn(OV(0x001b02f0), pos, 0, 0);
}

// The class-1440 enemy's hits and target (0x2e1678, a moby out of state 0): a pending alert
// (+0x38) arms the alert timer +0x1da (180..300 frames). A hit (0x25a420 / 0x25a478 on its
// health +0x20) that does damage: knocked back along the hit (gp-0x502c.. pushes), health less
// the damage; at 0 it dies (state 8, set 0xb, a white pulse, its bolts dropped), else it
// flinches (state 7, set 8, for 60 frames, a pulse); its effect slot released. Then its colour
// pulse runs and it picks a target (+0xd0 record): in state 1 the hero inside its area
// 0x1b04b0[+0x1b4] within 64, otherwise anyone within gp-0x5030 (37 when alerted), "lost"
// (+0x114 = 2) when beyond; the hero by default, or the moby +0x1bc when that is nearer.
// orig: 0x002e1678
void e1440_hits_and_target(uint32_t m) {
    constexpr uint32_t P = 0x0013f350;
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) return;
    if (lw(pv + 0x38) != 0) {
        sw(pv + 0x38, 0);
        sh(pv + 0x1da, static_cast<uint32_t>(frame_scale_i(rand_range(0xb4, 300))));
    }
    {
        int16_t t = static_cast<int16_t>(lh(pv + 0x1da));
        timer_tick_i16(t);
        sh(pv + 0x1da, static_cast<uint16_t>(t));
    }
    const uint32_t pulse = pv + 0x60;
    float dmg = 0.0f;
    int32_t state = 0;
    const uint32_t hit = sub_0025a420(m, 0x330000, 0);
    moby_hit_react_r(m, hit, pv + 0x20, 0, state, &dmg, nullptr, 4);
    if (state != 1 && lbu(m + 0x20) != 8 && dmg != 0.0f) {
        auto f = [](uint32_t x) -> float& { return *static_cast<float*>(ee_ram_ptr(x)); };
        const float dt2 = lwc1(0x0015ed70), dt = lwc1(0x0015ed6c);
        const float px = lwc1(OV(0x00166c00u - 0x502c)) * dt2;
        const float py = lwc1(OV(0x00166c00u - 0x5028)) * dt2;
        const float pz = lwc1(OV(0x00166c00u - 0x5024)) * dt;
        const float pw = lwc1(OV(0x00166c00u - 0x5020)) * dt;
        const float hp = lwc1(pv + 0x20) - dmg;
        sb(pv + 0xad, 0);
        sw(pv + 0x90, 0x200);
        swc1(pv + 0x80, px);
        swc1(pv + 0x84, py);
        swc1(pv + 0x88, pz);
        swc1(pv + 0x8c, pw);
        sw(pv + 0x94, 9);
        swc1(pv + 0xbc, lwc1(0x0015ed6c) + lwc1(0x0015ed6c));
        swc1(pv + 0x20, hp);
        swc1(pv + 0x98, 0.5f);
        if (hp <= 0.0f) {
            sw(pv + 0x94, 0x29);
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            float yaw = 0.0f;
            vec_yaw_marked_v(V(hit + 0x10), yaw, f(pv + 0x88), f(pv + 0x8c));
            move_state_start(m, pv + 0x70, 0xb, 1, 0, yaw);
            swc1(pv + 0xc0, 7.0f);
            swc1(pv + 0xc4, 16.0f);
            sb(m + 0x20, 8);
            sw(m + 0x94, 0);
            sb(pv + 0x67, 0x78);
            color_pulse_start(m, pulse);
            moby_drop_bolts(m, 0, -1);
        } else {
            const float d = lwc1(0x0015ed6c);
            swc1(pv + 0x88, lwc1(OV(0x00166c00u - 0x5024)) * d * u2f(0x3eb33333u));
            swc1(pv + 0x8c, lwc1(OV(0x00166c00u - 0x5020)) * d * 0.5f);
            float yaw = 0.0f;
            vec_yaw_marked_v(V(hit + 0x10), yaw, f(pv + 0x88), f(pv + 0x8c));
            move_state_start(m, pv + 0x70, 8, 1, 0, atan2_f(lwc1(hit + 0x10), lwc1(hit + 0x14)));
            swc1(pv + 0xc4, -1.0f);
            swc1(pv + 0xc0, -1.0f);
            sb(m + 0x20, 7);
            sb(pv + 0x67, 0xfa);
            sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0x3c)));
            color_pulse_start(m, pulse);
        }
        fx1471_release(m);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pulse);
    const uint32_t st = lbu(m + 0x20);
    if (st == 1) {
        const uint32_t area = lwu(OV(0x001b04b0) + lwu(pv + 0x1b4) * 4);
        target_select(m, pv + 0xd0, 0, 0, area + 0x10, lw(area), 64.0f);
    } else if (st != 5) {
        const float r = lh(pv + 0x1da) != 0 ? 37.0f : lwc1(OV(0x00166c00u - 0x5030));
        if (target_select_any(m, pv + 0xd0, r) != 2 && r < vec2_distance(V(m + 0x10), V(pv + 0xd0))) sw(pv + 0x114, 2);
    }
    if (lw(pv + 0x110) == 0) {
        sw(pv + 0x110, lwu(P + 0x2080));
        lq_sq(pv + 0xd0, P + 0x80);
    }
    const int32_t k = lw(pv + 0x1bc);
    if (k < 0) return;
    const uint32_t c = lwu(OV(0x00166c00u - 0x6c28)) + (static_cast<uint32_t>(k) << 8);
    if (c == 0 || lbu(c + 0x20) == 0xfe || lbu(c + 0x20) == 0xfd) {
        sw(pv + 0x1bc, 0xffffffffu);
        return;
    }
    const float d1 = vec2_distance(V(m + 0x10), V(c + 0x10));
    const float d2 = vec2_distance(V(m + 0x10), V(lwu(pv + 0x110) + 0x10));
    if (!(d1 < d2)) return;
    sw(pv + 0x110, c);
    lq_sq(pv + 0xd0, c + 0x10);
    sw(pv + 0x114, 1);
}

// The class-1440 enemy (a gun turret-like walker of Veldin): hits / target first (0x2e1678),
// its head joint (node pvars +0x120) sprung, its floor band kept near the camera. States:
// 0 set up (health 2, home +0x1a0; on a path 0x1b04b0[+0x1b8] it starts walking it (1), else it
// watches (3); a path without an area +0x1b4 removes it), 1 idles (sets 1 / 2) until a target
// (+0x114 not 2) makes it 2 walk the path's waypoints (+0x1dc, speed +0x1d4 seeking the
// distance left) to 3, 3 turns to the target (sets 1 / 2, 4 near the hero) and claims an effect
// slot to 4 aim, 5 fires (a beam from chain point 0 along its facing, hitting the hero ends it;
// the effect slot follows it; after *(gp-0x5018) cycles 6), 6 turns until its animation ends,
// 7 flinches (0x25c698) back to 3, 8 dies: an explosion, debris 0x7ab and the pieces
// 0x78d..0x78f, then it is removed. Every state but the removal ends with the flash 0x2e1aa8.
// orig: 0x002e0b88
void e1440_update(uint32_t m) {
    constexpr uint32_t P = 0x0013f350;
    const uint32_t pv = lwu(m + 0x78);
    e1440_hits_and_target(m);
    const uint32_t node = pv + 0x120;
    const uint32_t tgt = lwu(pv + 0x110);
    moby_set_70(node, 2.5f);
    if (lbu(0x0015edb7) != 0) {
        const float g = lwc1(OV(0x00166c00u - 0x7e9c));
        joint_node_spring(m, node, 2, g * u2f(0x3cf5c28fu), g * u2f(0x3e99999au));
    }
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 27.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x15);
    }
    const float four_pi = u2f(0x41490fdbu);
    auto turn_to = [&](float tx, float ty, float accel, float vmax) {
        const float yaw = atan2_f(tx - lwc1(m + 0x10), ty - lwc1(m + 0x14));
        yaw_seek(m + 0x48, pv + 0x1d0, yaw, accel, accel, vmax);
    };
    auto set = [&](uint32_t s, int32_t frames) {
        if (lbu(m + 0x53) != s) moby_anim_set(m, s, 0, frames);
    };
    switch (lbu(m + 0x20)) {
    case 0: {
        V(pv + 0x1a0) = V(m + 0x10);
        sb(pv + 0x29, 0);
        swc1(pv + 0x20, 2.0f);
        sb(pv + 0x28, 1);
        sh(pv + 0x24, 2);
        sb(pv + 0x58, static_cast<uint32_t>(float_to_int(14.0f)));
        const int32_t b = float_to_int(8.0f);
        sw(pv + 0x1dc, 1);
        sb(pv + 0x5a, static_cast<uint32_t>(b));
        if (rand_mod(2) != 0) sh(m + 0x34, lhu(m + 0x34) | 0x8000u);
        const int32_t path = lw(pv + 0x1b8);
        if (path >= 0) {
            if (lw(pv + 0x1b4) == -1) {
                moby_retire(m);
                return;
            }
            lq_sq(m + 0x10, lwu(OV(0x001b04b0) + static_cast<uint32_t>(path) * 4) + 0x10);
            sb(m + 0x20, 1);
        } else {
            sb(m + 0x20, 3);
        }
        set(1, 5);
        break;
    }
    case 1:
        if ((lbu(m + 0x70) & 2u) != 0) {
            if (lbu(m + 0x53) != 1) {
                moby_anim_set(m, 1, 0, frame_scale_i(0x14));
            } else if (rand_mod(4) == 0) {
                if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, frame_scale_i(0x14));
            } else if (lbu(m + 0x53) != 1) {
                moby_anim_set(m, 1, 0, frame_scale_i(0x14));
            }
        }
        if (lw(pv + 0x114) == 2) break;
        sb(m + 0x20, 2);
        if (lbu(m + 0x53) != 9) moby_anim_set(m, 9, 0, frame_scale_i(10));
        break;
    case 2: {
        const uint32_t path = lwu(OV(0x001b04b0) + lwu(pv + 0x1b8) * 4);
        float left = 0.0f;
        const Vec4 wp = V(path + lwu(pv + 0x1dc) * 0x10 + 0x10);
        turn_to(wp.x, wp.y, lwc1(0x0015ed70) * four_pi, lwc1(0x0015ed6c) * four_pi);
        Vec4 d;
        vec3_sub(d, wp, V(m + 0x10));
        const float dist = vec3_distance(V(m + 0x10), V(path + lwu(path) * 0x10));
        const float dt2 = lwc1(0x0015ed70), dt = lwc1(0x0015ed6c);
        seek_1d_v(left, *static_cast<float*>(ee_ram_ptr(pv + 0x1d4)), dist, dt2 * 8.0f, dt2 * 16.0f, dt * 12.0f);
        if (vec3_length(d) < lwc1(pv + 0x1d4) + lwc1(pv + 0x1d4)) {
            const int32_t n = lw(pv + 0x1dc) + 1;
            sw(pv + 0x1dc, static_cast<uint32_t>(n));
            if (n == lw(path)) {
                sb(m + 0x20, 3);
                set(1, frame_scale_i(10));
            }
        }
        vec3_set_length(d, d, lwc1(pv + 0x1d4));
        vec3_add(V(m + 0x10), V(m + 0x10), d);
        break;
    }
    case 3:
        turn_to(lwc1(tgt + 0x10), lwc1(tgt + 0x14), lwc1(0x0015ed70) * four_pi, lwc1(0x0015ed6c) * four_pi);
        if (vec2_distance(V(m + 0x10), V(0x0013f3d0)) < 20.0f) {
            set(4, frame_scale_i(0x14));
        } else if ((lbu(m + 0x70) & 2u) != 0) {
            const uint32_t a = lbu(m + 0x53);
            if (a != static_cast<uint32_t>(rand_mod(2) + 1)) {
                const int32_t r = rand_mod(2) + 1;
                moby_anim_set(m, static_cast<uint32_t>(r), 0, frame_scale_i(0x14));
            }
        }
        if (lw(pv + 0x114) == 2 || fx1471_claim(m, 0, 0) == 0) break;
        sb(m + 0x20, 4);
        set(5, frame_scale_i(10));
        break;
    case 4:
        turn_to(lwc1(tgt + 0x10), lwc1(tgt + 0x14), lwc1(0x0015ed70) * four_pi, lwc1(0x0015ed6c) * four_pi);
        if ((lbu(m + 0x70) & 2u) == 0) break;
        sh(pv + 0x1d8, 0);
        sb(m + 0x20, 5);
        set(6, frame_scale_i(10));
        break;
    case 5: {
        float yaw = atan2_f(lwc1(pv + 0xd0) - lwc1(m + 0x10), lwc1(pv + 0xd4) - lwc1(m + 0x14));
        const float off = lwc1(OV(0x00166c00u - 0x5014));
        yaw = (lhu(m + 0x34) & 0x8000u) != 0 ? angle_add(yaw, off) : angle_sub(yaw, off);
        yaw_seek(m + 0x48, pv + 0x1d0, yaw, lwc1(0x0015ed70) * u2f(0x3fc90fdbu), lwc1(0x0015ed70) * u2f(0x3fc90fdbu), lwc1(0x0015ed6c) * four_pi);
        alignas(16) Mat4 mat;
        moby_chain_world_one_v(m, 0, mat);
        Vec4 dir = V(m + 0xc0);
        dir.z = 0.0f;
        vec3_set_length(dir, dir, lwc1(OV(0x00166c00u - 0x501c)));
        const Vec4 start = mat.r[3];
        Vec4 end;
        vec3_add(end, start, dir);
        uint32_t rec[9];
        rec[0] = f2u(cos_vu(lwc1(m + 0x48)));
        rec[1] = f2u(sin_vu(lwc1(m + 0x48)));
        rec[2] = f2u(1.0f);
        rec[3] = 0x45afdf66u;
        rec[4] = m;
        rec[5] = 0x10000u;
        rec[6] = 0x100u | (static_cast<uint32_t>(lhu(m + 0xa6)) << 16);
        rec[7] = f2u(1.0f);
        rec[8] = 1;
        bool done = false;
        if (lbu(m + 0x52) == lbu(m + 0x53) && 1.0f < moby_anim_time(m)) {
            done = true;
            rec[5] |= 1u;
        }
        Vec4 body = V(m + 0x10);
        body.z = body.z + 0.5f;
        auto hit_hero = [&] {
            fx1471_release(m);
            sb(m + 0x20, 6);
            set(7, frame_scale_i(10));
        };
        if (segment_collide_v(start, body, 0, m, rec) != 0 && done && lwu(OV(0x00173e58)) == lwu(0x001413d0)) hit_hero();
        if (segment_collide_v(start, end, 0, m, rec) != 0) {
            end = V(OV(0x00173e60));
            if (done && lwu(OV(0x00173e58)) == lwu(0x001413d0)) hit_hero();
        }
        fx1471_set_v(m, mat, end);
        if ((lbu(m + 0x70) & 2u) == 0) break;
        const int32_t c = lh(pv + 0x1d8);
        sh(pv + 0x1d8, static_cast<uint32_t>(c + 1));
        if (c != lw(OV(0x00166c00u - 0x5018))) break;
        fx1471_release(m);
        sb(m + 0x20, 6);
        set(0xc, frame_scale_i(10));
        break;
    }
    case 6:
    case 7: {
        uint32_t v;
        if (lbu(m + 0x20) == 6) {
            turn_to(lwc1(tgt + 0x10), lwc1(tgt + 0x14), lwc1(0x0015ed70) * four_pi, lwc1(0x0015ed6c) * four_pi);
            v = lbu(m + 0x70) & 2u;
        } else {
            v = enemy_physics(m, pv + 0x70) & 0x140u;
        }
        if (v == 0) break;
        sb(m + 0x20, 3);
        set(1, frame_scale_i(10));
        break;
    }
    case 8: {
        if ((enemy_physics(m, pv + 0x70) & 0x140u) == 0) break;
        explosion_burst(m, m + 0x10, -1, 0.75f, 13.0f);
        debris_burst(m, 0x7ab, 1, 0x7ab, 1, 4, 0);
        const Vec4& zero = V(OV(0x0015f580));
        for (uint32_t c = 0x78d; c <= 0x78f; ++c) debris_spawn_v(m, c, V(m + 0x10), V(m + 0x40), 0, 0, zero, zero, zero, 0.0f);
        moby_retire(m);
        return;
    }
    default:
        break;
    }
    e1440_flash(m);
}

}  // namespace rac1
