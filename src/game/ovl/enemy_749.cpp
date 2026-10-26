// The class-749 enemy of Veldin (update 0x2d4610) and the helpers its chain needs.
#include <cmath>
#include <game/ovl/ovl_reloc.h>
#include <cstring>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/carry_hooks.h>
#include <game/ovl/carry_hooks_levels.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>

namespace rac1 {

// An empty function of the original (jr ra).
// orig: 0x001f6c28
void debug_stub_1f6c28() {}

// a's squared length (VU0 macro: x*x + y*y, + 1 * z*z).
// orig: 0x001ff430
float vec3_length_sq_vu(const Vec4& a) { return vuc(vuc(vuc(a.x * a.x) + vuc(a.y * a.y)) + vuc(a.z * a.z)); }

// d = a scaled down to length max_len (w: a's) when it is at least that long: 1; 0 (d untouched)
// when a is zero or shorter (VU0 macro: vsqrt, max - len tested as a 64-bit integer, vdiv).
// orig: 0x001ff598
int32_t vec3_clamp_length_vu(Vec4& d, const Vec4& a, float max_len) {
    const Vec4 v = a;
    const float l2 = vuc(vuc(vuc(v.x * v.x) + vuc(v.y * v.y)) + vuc(v.z * v.z));
    const float len = vuc(std::sqrt(std::fabs(l2)));
    const float mx = vuc(max_len);
    const float over = vuc(mx - len);
    if (f2u(len) == 0) return 0;  // (the 64-bit test also takes lane y, y*y: zero whenever len is)
    if (static_cast<int32_t>(f2u(over)) > 0) return 0;  // dsll32 + bgtz: positive and non-zero
    const float q = vuc(mx / len);
    d = Vec4{vuc(v.x * q), vuc(v.y * q), vuc(v.z * q), v.w};
    return 1;
}

// a wrapped into [-pi, pi) through its fraction of 2 pi.
// orig: 0x0025e310
float angle_wrap_mod(float a) {
    const float pi = u2f(0x40490fdbu), tau = u2f(0x40c90fdbu);
    float whole;
    const float f = split_int_frac(&whole, fpu_div(a + pi, tau));
    return f * tau - pi;
}

// The class's hook +4 (class +0x2c table) on moby m: 1 when it has none. `a1` is the register the caller
// leaves in a1 (the hooks of the carried mobys read the place it points to).
// orig: 0x002da218
int32_t class_hook_4(uint32_t m, uint32_t a1) {
    const uint32_t fn = lwu(lwu(lwu(m + 0x24) + 0x2c) + 4);
    if (fn == 0) return 1;
    // the played level's own class hooks (Novalis's, Eudora's, Rilgar's...: carry_hooks_levels.cpp)
    if (int32_t r = 0; class_hook_call_level(fn, m, a1, 0, 0.0f, r)) return r;
    uint32_t vf = ovl_code_to_veldin(fn);
    // (class 749's hooks are Veldin's own: another level's function the matcher pairs with them has the same shape but its own constants)
    if (vf == NOT_ADDR(0x002d5730) && ovl_reloc_level() != 0) vf = 0;
    switch (vf) {  // jalr fn: the ported hooks
        case 0x002db850: return hook_default_4(m);
        case 0x002d5730: return e749_hook4(m, a1, 0, 0.0f);
        case 0x002cae10: return enemy10e_hook4(m, a1, 0, 0.0f);
        default:
            return static_cast<int32_t>(TODO_PORT(fn, "class hook +4")(m));
    }
}

// The class-749 enemy's hook +0x10: its pvars + 0xd0 (0 for no moby or no pvars).
// orig: 0x002d5800
uint32_t e749_hook_10(uint32_t m) {
    if (m == 0) return 0;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return 0;
    return pv + 0xd0;
}

// The hook of the classes without one (returns 0).
// orig: 0x002db888
uint32_t class_hook_none(uint32_t) { return 0; }

// The class's hook +0x10 (class +0x2c table) on moby m: 0 for no moby, a retired one (state 0xfe /
// 0xfd), one without pvars or class, or no hook.
// orig: 0x002db890
uint32_t class_hook_10(uint32_t m) {
    if (m == 0) return 0;
    const uint32_t s = lbu(m + 0x20);
    if (s == 0xfe) return 0;
    if (s == 0xfd) return 0;
    if (lw(m + 0x78) == 0) return 0;
    const uint32_t cls = lwu(m + 0x24);
    if (cls == 0) return 0;
    const uint32_t fn = lwu(lwu(cls + 0x2c) + 0x10);
    if (fn == 0) return 0;
    // the played level's own class hooks (carry_hooks_levels.cpp; e.g. Eudora's own 0x2c41b0 has the shape of class
    // 749's hook, so the matcher paired them, but returns pvars + 0x180)
    if (int32_t r = 0; class_hook_call_level(fn, m, 0, 0, 0.0f, r)) return static_cast<uint32_t>(r);
    uint32_t vf = ovl_code_to_veldin(fn);
    if (vf == NOT_ADDR(0x002d5800) && ovl_reloc_level() != 0) vf = 0;  // (as in class_hook_4)
    switch (vf) {  // (the level's code address as Veldin's: the ports' cases)  // jalr fn: the ported hooks
        case 0x002d5800: return e749_hook_10(m);
        case 0x002db888: return class_hook_none(m);
        case 0x002caf40: return enemy10e_hook_10(m);
        default:
            return static_cast<uint32_t>(TODO_PORT(fn, "class hook +0x10")(m));
    }
}

// The mobys (flags) in the sphere (radius) at moby m's chain point `id`, hit with m's facing
// times speed (0x2598b0, its strength the float of b).
// orig: 0x002599e8
int32_t moby_chain_contact(uint32_t m, uint32_t id, int32_t b, uint32_t a, uint32_t c, uint32_t flags, float radius, float speed) {
    Vec4 at;
    moby_chain_point_v(m, id, at);
    return moby_sphere_contact_v(m, at, b, a, c, flags, radius, static_cast<float>(b), speed);
}

// Whether the segment a -> b (in the plane) crosses path `path` (0x1b04b0[path]: count, points
// from +0x10; a point's w 0 on both ends of an edge leaves it out): the points in the frame where
// a is 0 and b is x = 1, an edge crossing y = 0 inside 0 < x < 1.
// orig: 0x00261968
int32_t path_crossed_v(uint32_t path, const Vec4& a, const Vec4& b) {
    Mat4 f;
    vec3_sub(f.r[0], b, a);
    f.r[0].z = 0.0f;
    vec3_set_length(f.r[0], f.r[0], fpu_div(1.0f, vec3_length(f.r[0])));
    vec_clear(f.r[1]);
    f.r[1].x = f.r[0].y;
    f.r[1].y = -f.r[0].x;
    vec_clear(f.r[2]);
    f.r[2].z = 1.0f;
    f.r[3] = Vec4{};  // (not read: rows 0..2 only)
    const uint32_t tbl = OV(0x001b04b0) + path * 4;
    Vec4 p0;
    vec3_sub(p0, V(lwu(tbl) + 0x10), a);
    mat3_transform(p0, p0, f);
    for (int32_t i = 1; i < lw(lwu(tbl)); ++i) {
        Vec4 p1;
        vec3_sub(p1, V(lwu(tbl) + 0x10 + static_cast<uint32_t>(i) * 0x10), a);
        mat3_transform(p1, p1, f);
        if (!(p0.w == 0.0f && p1.w == 0.0f) && p1.y * p0.y < 0.0f) {
            const float x = fpu_div(p0.x - p1.x, p0.y - p1.y) * -p1.y + p1.x;
            if (0.0f < x && x < 1.0f) return 1;
        }
        p0 = p1;
    }
    return 0;
}
// orig: 0x00261968 (EE-address form)
int32_t path_crossed(uint32_t path, uint32_t a, uint32_t b) { return path_crossed_v(path, V(a), V(b)); }

// pos pushed (in the plane, its z kept) out to distance r from the edges of path `path` it is
// within r of (edges with w 0 on both ends left out), past an end from that end's point: 1 and
// out = the result when it was pushed, else 0.
// orig: 0x00261d78
int32_t path_push_out_2d_v(uint32_t path, const Vec4& pos, Vec4& out, float r) {
    int32_t hit = 0;
    Vec4 p = pos;
    const uint32_t tbl = OV(0x001b04b0) + path * 4;
    for (int32_t i = 0; i < lw(lwu(tbl)) - 1; ++i) {
        const uint32_t pts = lwu(tbl);
        const uint32_t o = static_cast<uint32_t>(i) * 0x10;
        if (lwc1(pts + o + 0x1c) == 0.0f && lwc1(pts + o + 0x2c) == 0.0f) continue;
        Vec4 rel, seg, dir, cr, t, proj;
        vec3_sub(rel, p, V(pts + o + 0x10));
        rel.z = 0.0f;
        vec3_sub(seg, V(lwu(tbl) + o + 0x20), V(lwu(tbl) + o + 0x10));
        seg.z = 0.0f;
        vec3_set_length(dir, seg, 1.0f);
        vec3_cross_ba(cr, rel, dir);
        if (r < abs_f(cr.z)) continue;
        const float len = vec3_length(seg);
        const float along = vec3_dot(rel, dir);
        if (len < along || along < 0.0f) {
            if (!(vec3_length(rel) < r)) continue;
            vec3_set_length(t, rel, r);
            hit = 1;
            vec3_add(p, t, V(lwu(tbl) + o + 0x10));
        } else {
            vec3_set_length(proj, dir, along);
            hit = 1;
            vec3_sub(t, rel, proj);
            vec3_set_length(t, t, r);
            vec3_add(t, t, proj);
            vec3_add(p, t, V(lwu(tbl) + o + 0x10));
        }
        p.z = pos.z;
    }
    if (hit != 0) out = p;
    return hit;
}
// orig: 0x00261d78 (EE-address form)
int32_t path_push_out_2d(uint32_t path, uint32_t pos, uint32_t out, float r) { return path_push_out_2d_v(path, V(pos), V(out), r); }

// Moves moby m by d (d becomes the move made): a move longer than r is swept (raised by `up`) and
// stopped short of a wall (1); its sphere (radius r, `up` above) is pushed out of what it touches
// (six tries, 1); then the floor within `drop` below (2, 4 when steeper than 0.5 rad) lifts it by
// 0.3 of the way. flags 0x10: also the second collision set.
// orig: 0x00258830
uint32_t walker_move_v(uint32_t m, Vec4& d, uint32_t flags, float up, float r, float drop) {
    uint32_t res = 0;
    const uint32_t f = (flags << 1) & 0x20u;
    Vec4& pos = V(m + 0x10);
    const Vec4 old = pos;
    vec3_add(pos, pos, d);
    Vec4 a, b, ahead;
    if (r < vec3_length(d)) {
        a = old;
        a.z = a.z + up;
        vec3_set_length(ahead, d, r);
        vec3_add(b, ahead, pos);
        b.z = b.z + up;
        if (segment_collide_v(a, b, f, m, nullptr) != 0) {
            vec3_sub(pos, V(OV(0x00173e60)), ahead);
            res = 1;
            pos.z = pos.z - up;
        }
    }
    for (int32_t i = 0; i < 6; ++i) {
        a = pos;
        a.z = a.z + up;
        if (sphere_collide_v(a, r, f | 4u, m) == 0) break;
        pos = V(OV(0x00173e70));
        res |= 1;
        pos.z = pos.z - up;
    }
    const float floor = pos.z - drop;
    a = pos;
    b = pos;
    a.z = a.z + up;
    b.z = floor - u2f(0x3d4ccccdu);
    if (segment_collide_v(a, b, f | 2u, m, nullptr) != 0 && 0 < lw(OV(0x00173e5c))) {
        res |= 2;
        if (0.5f < atan2_f(lwc1(OV(0x00173e88)), vec2_length(V(OV(0x00173e80))))) res |= 4;
        const float h = lwc1(OV(0x00173e68));
        if (floor < h) pos.z = pos.z + (h - floor) * u2f(0x3e99999au);
    }
    vec3_sub(d, pos, old);
    return res;
}
// orig: 0x00258830 (EE-address form)
uint32_t walker_move(uint32_t m, uint32_t d, uint32_t flags, float up, float r, float drop) { return walker_move_v(m, V(d), flags, up, r, drop); }

// 0x258830 with the radius in 1/1024 units.
// orig: 0x00258ad0
uint32_t walker_move_fx(uint32_t m, uint32_t d, int32_t r, uint32_t flags, float up, float drop) {
    return walker_move(m, d, flags, up, int_to_float(r) * u2f(0x3a800000u), drop);
}

// Hits (0x259a88) each moby of the n at `list` but `skip`, pushed away from pos (speed, z, w
// 5627.95): from moby m, b, bytes a / c, m's class, strength f.
// orig: 0x0025a9f8
void moby_list_hit_v(uint32_t m, const Vec4& pos, uint32_t list, int32_t n, uint32_t skip, uint32_t b, uint32_t a, uint32_t c, float f, float speed, float z) {
    const Vec4 p = pos;
    if (n <= 0) return;
    alignas(16) uint8_t rec[0x24] = {};
    for (; n != 0; --n, list += 4) {
        const uint32_t t = lwu(list);
        if (t == skip) continue;
        const uint16_t cls = lhu(m + 0xa6);
        std::memcpy(rec + 0x10, &m, 4);
        std::memcpy(rec + ee_half_addr(0x1a), &cls, 2);  // (rec laid out as EE memory: moby_hit_record_v)
        std::memcpy(rec + 0x14, &b, 4);
        std::memcpy(rec + 0x1c, &f, 4);
        std::memcpy(rec + 0x20, &b, 4);
        rec[ee_byte_addr(0x18)] = static_cast<uint8_t>(a);
        rec[ee_byte_addr(0x19)] = static_cast<uint8_t>(c);
        const float ang = atan2_f(lwc1(t + 0x10) - p.x, lwc1(t + 0x14) - p.y);
        const float x = cos_vu(ang) * speed;
        const float y = sin_vu(ang) * speed;
        const uint32_t w = 0x45afdf66u;
        std::memcpy(rec + 0, &x, 4);
        std::memcpy(rec + 4, &y, 4);
        std::memcpy(rec + 8, &z, 4);
        std::memcpy(rec + 0xc, &w, 4);
        moby_hit_record_v(lwu(list), rec);
    }
}
// orig: 0x0025a9f8 (EE-address form)
void moby_list_hit(uint32_t m, uint32_t pos, uint32_t list, int32_t n, uint32_t skip, uint32_t b, uint32_t a, uint32_t c, float f, float speed, float z) {
    moby_list_hit_v(m, V(pos), list, n, skip, b, a, c, f, speed, z);
}

// A kind 0x12 particle (grey 0x806040, size 27993..49980, 210 frames, +0x24 0.08, +0x2c 1) at
// pos plus a random offset (+-0.1, +-0.3, +-0.3) through the rotation rows of `mat`.
// orig: 0x0026c3d0
void spawn_particle_12(uint32_t pos, uint32_t mat) {
    const uint32_t p = alloc_particle(0x12);
    if (p == 0) return;
    Mat4 rm;
    rm.r[0] = V(mat);
    rm.r[1] = V(mat + 0x10);
    rm.r[2] = V(mat + 0x20);
    rm.r[3] = Vec4{};
    // w: an uninitialised stack lane in the original, 0 here (row 3 of the matrix is zero)
    Vec4 v{};
    v.x = rand_range_f(u2f(0xbdcccccdu), u2f(0x3dcccccdu));
    v.y = rand_range_f(u2f(0xbe99999au), u2f(0x3e99999au));
    v.z = rand_range_f(u2f(0xbe99999au), u2f(0x3e99999au));
    Vec4 o;
    mat4_transform(o, v, rm);
    vec3_add(V(p + 0x10), o, V(pos));
    sw(p + 4, 0x00806040u);
    const int32_t s = float_to_int(4.0f);
    sb(p + 1, 0);
    sb(p + 3, 0x48);
    sb(p + 9, static_cast<uint32_t>(s + 0x40));
    sb(p + 8, static_cast<uint32_t>(libc_rand()));
    sb(p + 2, lbu(lwu(OV(0x001b20c8))));
    swc1(p + 0x24, u2f(0x3da3d70au));
    swc1(p + 0xc, rand_range_f(u2f(0x46dab201u), u2f(0x47433c00u)));
    sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0xd2)));
    swc1(p + 0x2c, 1.0f);
}

// Wakes the moby m the class-749 enemy stands next to (class 0x33e; state 1 only): its spawn
// byte (+0xb0) + 2 in the table at 0x1baa24[+0xb2] and, when the spawn isn't a held one, in
// 0x1bb684[+0xb2]; state 2, +0xbc 1, +0x90 0x80208020, its sound 0; the points of its path
// (pvars +0) marked with its tag (pvars +4) get w 0.
// orig: 0x002d5830
void moby_33e_wake(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) != 1) return;
    sb(OV(0x001ba5d0) + static_cast<uint32_t>(lh(m + 0xb2)) + 0x454, lbu(m + 0xb0) + 2u);
    const uint32_t s = lbu(m + 0xb0);
    bool other = s == 0xff;
    if (!other && lbu(OV(0x0015fc88) + s) != 0xff) other = lbu(0x0014c050 + s + static_cast<uint32_t>(lw(0x0015ed84)) * 16) == 0xff;
    if (other) sb(OV(0x001bb230) + static_cast<uint32_t>(lh(m + 0xb2)) + 0x454, s + 2u);
    sb(m + 0xbc, 1);
    sb(m + 0x20, 2);
    sw(m + 0x90, 0x80208020u);
    moby_sound_start(0, 0, m);
    if (lw(pv) == -1) return;
    for (int32_t i = 0; i < lw(lwu(OV(0x001b04b0) + static_cast<uint32_t>(lw(pv)) * 4)); ++i) {
        const uint32_t e = lwu(OV(0x001b04b0) + static_cast<uint32_t>(lw(pv)) * 4) + static_cast<uint32_t>(i) * 0x10;
        if (lwc1(e + 0x1c) == lwc1(pv + 4)) sw(e + 0x1c, 0);
    }
}

// A class-0x7a debris moby at pos moving at vel (its pvars +0), spinning at a random 2..4 pi a
// second on two axes (+0x10 / +0x14), `life` frames (+0x18 / +0x1a); not `keep`: its scale
// shrunk by 0.5..0.75 (+0x1c the scale); +0xbc keep | flag << 1. Returns it (0 when none).
// orig: 0x002af450
uint32_t debris_7a_spawn_v(const Vec4& pos, const Vec4& vel, uint32_t life, uint32_t keep, uint32_t flag) {
    const uint32_t m = moby_spawn(0x7a);
    if (m == 0) return 0;
    sb(m + 0x31, 1);
    sb(m + 0x23, 0x80);
    sh(m + 0x32, 0xff);
    sb(m + 0x30, 0xff);
    const uint32_t pv = lwu(m + 0x78);
    moby_set_color_hi(m, 0x7f, 0x7f, 0x7f);
    V(m + 0x10) = pos;
    V(pv) = vel;
    const float tau = u2f(0x40c90fdbu), tau2 = u2f(0x41490fdbu);
    const float t0 = lwc1(0x0015ed6c);
    swc1(pv + 0x10, rand_range_f(t0 * tau, t0 * tau2));
    const float t1 = lwc1(0x0015ed6c);
    swc1(pv + 0x14, rand_range_f(t1 * tau, t1 * tau2));
    sh(pv + 0x1a, life);
    sh(pv + 0x18, life);
    if (keep == 0) swc1(m + 0x2c, lwc1(m + 0x2c) * rand_range_f(0.5f, 0.75f));
    swc1(pv + 0x1c, lwc1(m + 0x2c));
    sb(m + 0xbc, keep | (flag << 1));
    moby_update_transform(m);
    return m;
}
// orig: 0x002af450 (EE-address form)
uint32_t debris_7a_spawn(uint32_t pos, uint32_t vel, uint32_t life, uint32_t keep, uint32_t flag) { return debris_7a_spawn_v(V(pos), V(vel), life, keep, flag); }

// A class-0x4a8 effect moby at pos moving at vel (pvars +0), a random rotation, colour r / g / b,
// alpha a (also pvars +0x1e), its scale times `scale` into pvars +0x18 (own scale 0), `v` at
// pvars +0x10, `life` at pvars +0x14 (its low half also +0x1c). Returns it (0 when none).
// orig: 0x002df808
uint32_t fx_4a8_spawn_v(uint32_t v, const Vec4& pos, const Vec4& vel, uint32_t life, uint32_t r, uint32_t g, uint32_t b, uint32_t a, float scale) {
    const uint32_t m = moby_spawn(0x4a8);
    if (m == 0) return 0;
    const uint32_t pv = lwu(m + 0x78);
    const float pi = u2f(0x40490fdbu);
    // a stack quadword cleared with por (all 128 bits), so w stays 0
    Vec4 rot{};
    rot.x = rand_range_f(-pi, pi);
    rot.y = rand_range_f(-pi, pi);
    rot.z = rand_range_f(-pi, pi);
    sb(m + 0x20, 0);
    sb(m + 0x23, a & 0xffu);
    sb(m + 0x31, 1);
    sb(m + 0x30, 0xff);
    sh(m + 0x32, 0xff);
    moby_set_color_hi(m, r & 0xffu, g & 0xffu, b & 0xffu);
    swc1(pv + 0x18, lwc1(m + 0x2c) * scale);
    sw(m + 0x2c, 0);
    sw(pv + 0x10, v);
    V(m + 0x10) = pos;
    V(pv) = vel;
    V(m + 0x40) = rot;
    sh(pv + 0x1e, a & 0xffu);
    sw(pv + 0x14, life);
    sh(pv + 0x1c, lhu(pv + 0x14));
    moby_update_transform(m);
    return m;
}
// orig: 0x002df808 (EE-address form)
uint32_t fx_4a8_spawn(uint32_t v, uint32_t pos, uint32_t vel, uint32_t life, uint32_t r, uint32_t g, uint32_t b, uint32_t a, float scale) {
    return fx_4a8_spawn_v(v, V(pos), V(vel), life, r, g, b, a, scale);
}

// The class-749 enemy heading for `target`: the yaw towards it (offset by pvars +0x1fc beyond
// 2 units) and, while its animation isn't changing, the turn (0x25be00 on +0x48 / +0x1f4), the
// speed +0x1f8 (0x25b970 from `cur` to the distance, at least 0) and the step (0x258830, flags
// 0x10, falling by 10 a second plus pvars +0x270, the drop kept in +0x270 up to 4 a frame).
// Returns the distance, or 37 when the step left it airborne (+0x70 bit 2 clear).
// orig: 0x002d54c8
float e749_chase(uint32_t m, uint32_t target, float cur) {
    const uint32_t pv = lwu(m + 0x78);
    float pos = cur;
    float yaw = atan2_f(lwc1(target) - lwc1(m + 0x10), lwc1(target + 4) - lwc1(m + 0x14));
    const float z0 = lwc1(m + 0x18);
    const float dist = vec3_distance(V(m + 0x10), V(target));
    if (2.0f < dist) yaw = angle_add(yaw, lwc1(pv + 0x1fc));
    if (lbu(m + 0x52) != lbu(m + 0x53)) return dist;
    const float tau2 = u2f(0x41490fdbu);
    const float turn = lwc1(0x0015ed70) * tau2;
    yaw_seek(m + 0x48, pv + 0x1f4, yaw, turn, turn, lwc1(0x0015ed6c) * tau2);
    const float dt = lwc1(0x0015ed70);
    seek_1d_v(pos, *static_cast<float*>(ee_ram_ptr(pv + 0x1f8)), dist, dt * 8.0f, dt * u2f(0x41400000u), lwc1(0x0015ed6c) * 4.0f);
    if (lwc1(pv + 0x1f8) < 0.0f) swc1(pv + 0x1f8, 0.0f);
    swc1(pv + 0x278, lwc1(m + 0x48));
    Vec4 d;
    d.x = cos_vu(lwc1(m + 0x48)) * lwc1(pv + 0x1f8);
    d.y = sin_vu(lwc1(pv + 0x278)) * lwc1(pv + 0x1f8);
    d.z = lwc1(pv + 0x270) - lwc1(0x0015ed70) * 10.0f;
    d.w = 0.0f;  // an uninitialised stack lane in the original (not read by 0x258830's result)
    walker_move_v(m, d, 0x10, 0.5f, 0.5f, 0.0f);
    const float drop = lwc1(m + 0x18) - z0;
    const float most = lwc1(0x0015ed6c) * 4.0f;
    swc1(pv + 0x270, drop);
    if (most < drop) swc1(pv + 0x270, most);
    if ((lbu(m + 0x70) & 2u) != 0) return dist;
    return u2f(0x42140000u);
}

// The class-749 enemy's hits and targets each frame: drawn unless 0x15f5c4 is 2; a hit
// (0x25a420 mask 0x330000, reaction 0x25a478) with damage takes it from its health (pvars +0x20),
// sets its knock-back (+0x80.. from the table 0x161970, scaled by the frame time; heading by
// 0x25ab48), starts move state +0x70 with set 6 or 7, flash colour +0xc0 / +0xc4, state 0xc,
// a colour pulse (+0x60) and drops its bolts; the pulse runs on; the target (0x25ff38 inside its
// path, +0x1e4 at range 64 in state 3, else +0x1e0 at 24; none: the hero); in states 5 / 8 the
// rest timer +0x276 (reloaded with 180..240 frames when +0x38 is set) sends it to state 0xa.
// orig: 0x002d5160
void e749_hit_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) return;
    uint32_t f = lhu(m + 0x34);
    if (lw(OV(0x0015f5c4)) == 2) {
        sb(m + 0x31, 0);
        f |= 1u;
    } else {
        sb(m + 0x31, 1);
        f &= 0xfffeu;
    }
    sh(m + 0x34, f);
    float dmg = 0.0f;
    int32_t state = 0;
    const uint32_t hit = sub_0025a420(m, 0x00330000u, 0);
    moby_hit_react_r(m, hit, pv + 0x20, 0, state, &dmg, nullptr, 4);
    if (state != 1 && lbu(m + 0x20) != 0xc && dmg != 0.0f) {
        const float dt = lwc1(0x0015ed70), ft = lwc1(0x0015ed6c);
        const float k0 = lwc1(OV(0x00161970)) * dt, k1 = lwc1(OV(0x00161974)) * dt;
        const float k2 = lwc1(OV(0x00161978)) * ft, k3 = lwc1(OV(0x0016197c)) * ft;
        sb(pv + 0xad, 0);
        const float health = lwc1(pv + 0x20) - dmg;
        const float t = lwc1(0x0015ed6c);
        swc1(pv + 0x80, k0);
        swc1(pv + 0x98, 0.75f);
        swc1(pv + 0x20, health);
        swc1(pv + 0x84, k1);
        swc1(pv + 0x88, k2);
        swc1(pv + 0x8c, k3);
        swc1(pv + 0xbc, t + t);
        sw(pv + 0x94, 0x29);
        sw(pv + 0x90, 0x200);
        sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
        const Vec4 push = V(hit + 0x10);
        float yaw;
        vec_yaw_marked_v(push, yaw, *static_cast<float*>(ee_ram_ptr(pv + 0x88)), *static_cast<float*>(ee_ram_ptr(pv + 0x8c)));
        const int32_t r = rand_mod(2);
        move_state_start(m, pv + 0x70, static_cast<uint32_t>(r + 6), 1, 0, yaw);
        swc1(pv + 0xc0, lwc1(OV(0x0016198c)));
        swc1(pv + 0xc4, lwc1(OV(0x00161988)));
        sb(m + 0x20, 0xc);
        sb(pv + 0x67, 0x78);
        color_pulse_start(m, pv + 0x60);
        moby_drop_bolts(m, 0, -1);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
    const bool far = lbu(m + 0x20) == 3;
    const uint32_t path = lwu(OV(0x001b04b0) + static_cast<uint32_t>(lw(pv + (far ? 0x1e4 : 0x1e0))) * 4);
    target_select(m, pv + 0x180, 0, 0, path + 0x10, lw(path), far ? 64.0f : 24.0f);
    if (lw(pv + 0x1c0) == 0) {
        sw(pv + 0x1c0, lwu(0x0013f350 + 0x2080));
        lq_sq(pv + 0x180, 0x0013f350 + 0x80);
    }
    timer_tick_i16(ee_ref<int16_t>(pv + 0x276));
    if (lw(pv + 0x38) != 0) sh(pv + 0x276, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 240.0f)))));
    sw(pv + 0x38, 0);
    const uint32_t st = lbu(m + 0x20);
    if (st != 5 && st != 8) return;
    if (lh(pv + 0x276) == 0) return;
    sb(m + 0x20, 0xa);
    if (lbu(m + 0x53) == 4) return;
    moby_anim_set(m, 4, 0, frame_scale_i(0x14));
}

// The moby a class hook retires (moby_retire).
// orig: 0x002db868
void class_hook_retire(uint32_t m) { moby_retire(m); }

// An explosion at moby m (the blast flag 0x13e529 b: sizes times b + 1, its colours swapped by
// b; for b != 0 the mobys within 3 are hit): 10 (5 without b) light and 4 (2) heavy debris
// pieces (0x2af450) flung sideways and up, one more towards the camera, up to four kind 0x0b
// spark bursts (from the colour tables 0x1ea100 / 0x1ea118, more and bigger the nearer the
// camera), the class-0x4a8 smoke puffs (two more beyond 9 units), the camera shake +0x160 / +0x168
// and the blast effect 0x1ea0b0.
// orig: 0x002dbf28
void explosion_burst(uint32_t m) {
    constexpr uint32_t B = 0x0013e520;  // (+9: the blast flag)
    const uint32_t pos = m + 0x10;
    Vec4 up{}, rise{}, cam;
    rise.z = lwc1(0x0015ed6c) + lwc1(0x0015ed6c);
    up.z = 1.0f;
    const float k = int_to_float(lbu(B + 9)) + 1.0f;
    const int32_t div = lbu(B + 9) == 0 ? 2 : 1;
    vec3_scale(rise, rise, k);
    vec3_sub(cam, V(OV(0x00166dc0)), V(pos));
    const float dist = vec3_length(cam);
    Vec4 v, t;
    auto fling = [&](float lo, float hi) {  // sideways in the plane, a random rise, times k lo..hi
        Vec4 r{};
        r.x = rand_range_f(-1.0f, 1.0f);
        r.y = rand_range_f(-1.0f, 1.0f);
        v = r;
        vec3_set_length(t, up, vec3_dot(v, up));
        vec3_sub(v, v, t);
        vec3_set_length(t, up, rand_range_f(0.0f, 1.0f));
        vec3_add(v, v, t);
        vec3_set_length(v, v, k * rand_range_f(lo, hi) * lwc1(0x0015ed6c));
        vec3_add(v, v, rise);
    };
    for (int32_t i = 0; i < ee_div(10, div); ++i) {
        fling(u2f(0x40600000u), u2f(0x40d00000u));
        const int32_t a = frame_scale_i(0x3c);
        const int32_t life = rand_range(a, frame_scale_i(0x78));
        debris_7a_spawn_v(V(pos), v, static_cast<uint32_t>(life), 0, lbu(B + 9));
    }
    for (int32_t i = 0; i < ee_div(4, div); ++i) {
        fling(u2f(0x40d00000u), 10.0f);
        const int32_t a = frame_scale_i(0x3c);
        const int32_t life = rand_range(a, frame_scale_i(0x5a));
        debris_7a_spawn_v(V(pos), v, static_cast<uint32_t>(life), 1, lbu(B + 9));
    }
    {
        Vec4 r{};
        r.x = rand_range_f(-1.0f, 1.0f);
        r.y = rand_range_f(-1.0f, 1.0f);
        r.z = rand_range_f(-1.0f, 1.0f);
        const float s = fpu_div(dist, 5.0f);
        cam.z = cam.z + dist * 0.5f;
        v = r;
        vec3_set_length(v, v, s * lwc1(0x0015ed6c));
        vec3_set_length(cam, cam, (dist + dist) * lwc1(0x0015ed6c));
        vec3_add(v, v, cam);
        vec3_clamp_length_vu(v, v, lwc1(0x0015ed6c) * 10.0f);
        const int32_t a = frame_scale_i(0x3c);
        const int32_t life = rand_range(a, frame_scale_i(0x5a));
        debris_7a_spawn_v(V(pos), v, static_cast<uint32_t>(life), 1, lbu(B + 9));
    }
    int32_t bursts = 4;
    if (dist < 6.0f) bursts = float_to_int(dist) + 1;
    float near = 0.0f;
    if (dist < 7.0f) near = 7.0f - dist;
    for (int32_t i = bursts; i > 0; --i) {
        float speed = k * rand_range_f(8.0f, 10.0f);
        const float ft = lwc1(0x0015ed6c);
        const float slow = near * ft;
        speed = speed * ft - slow;
        // (the original copies the two 6-colour tables to the stack first)
        const uint32_t c0 = color_swap_channels(lwu(OV(0x001ea100) + static_cast<uint32_t>(rand_mod(6)) * 4), lbu(B + 9));
        const uint32_t c1 = color_swap_channels(lwu(OV(0x001ea118) + static_cast<uint32_t>(rand_mod(6)) * 4), lbu(B + 9));
        const float size = k * u2f(0x48c35000u);
        const int32_t a = frame_scale_i(0xf);
        const int32_t count = rand_range(a, frame_scale_i(0x14));
        const int32_t b = frame_scale_i(0x19);
        const int32_t fade = rand_range(b, frame_scale_i(0x1e));
        spawn_particle_0b_v(V(pos), rise, c0, c1, static_cast<uint32_t>(count), static_cast<uint32_t>(fade), 0, 0, size, speed);
    }
    auto smoke = [&](uint32_t r, uint32_t g, uint32_t b, uint32_t alpha, float scale, int32_t frames) {
        uint32_t x = r, y = g, z = b;
        swap3_by_mask(x, y, z, lbu(B + 9));
        const float sc = k * scale;
        const int32_t life = frame_scale_i(frames);
        fx_4a8_spawn_v(m, V(pos), rise, static_cast<uint32_t>(life), x & 0xffu, y & 0xffu, z & 0xffu, alpha, sc);
    };
    if (9.0f < dist) {
        const float sc = k * 4.0f;
        fx_4a8_spawn_v(m, V(pos), rise, static_cast<uint32_t>(frame_scale_i(0xf)), 0x7f, 0x7f, 0x7f, 0x20, sc);
        smoke(0x7f, 0x20, 0, 0x20, 4.0f, 0x18);
    }
    smoke(0x7f, 0x3f, 0, 0x30, 4.0f, 0x14);
    smoke(0x60, 0x10, 0, 0x40, u2f(0x40600000u), 0x1b);
    smoke(0x20, 0, 0, 0x20, 3.0f, 0x1d);
    constexpr OvConst<0x00166c80u> C{};
    if (dist < 20.0f) swc1(C + 0x160, u2f(0x3ecccccdu) - dist * u2f(0x3c8f5c29u));
    else swc1(C + 0x160, u2f(0x3d4cccd0u));
    sw(C + 0x168, static_cast<uint32_t>(frame_scale_i(0x19)));
    effect_spawn(OV(0x001ea0b0), pos, 0, 0);  // (the original passes a stack copy of the 0x50-byte table)
    if (lbu(B + 9) == 0) return;
    const int32_t n = mobys_in_sphere(pos, 3.0f, 0x10, m, 0);
    const Vec4 at = V(m + 0x10);
    moby_list_hit_v(m, at, OV(0x00177f00), n, 0, 0x00830000u, 2, 1, 2.0f, 1.0f, 1.0f);
}

// Moby m blown up (0x2dbf28 when `burst`) and reset for its class's hook +0x14 (else retired):
// level rotation, the matrix rebuilt, flag 4 at +0x34 cleared, +0x98 from r +0x84, the class
// scale (+0x24) back at +0x2c, r +0x68 cleared.
// orig: 0x002dc938
void explode_and_reset(uint32_t m, uint32_t r, uint32_t burst) {
    if (burst != 0) explosion_burst(m);
    sw(m + 0x98, lwu(r + 0x84));
    V(m + 0x40) = Vec4{};
    Mat4 rot;
    euler_to_mat4_vu(rot, V(m + 0x40));
    mat3_copy(M(m + 0xc0), rot);
    moby_update_transform_kept(m);
    sh(m + 0x34, lhu(m + 0x34) & 0xfffbu);
    const uint32_t cls = lwu(m + 0x24);
    sw(m + 0x98, lwu(r + 0x84));
    swc1(m + 0x2c, lwc1(cls + 0x24));
    sh(r + 0x68, 0);
    const uint32_t fn = lwu(lwu(lwu(m + 0x24) + 0x2c) + 0x14);
    if (fn != 0) ovl_call_moby_fn(fn, m);
    else moby_retire(m);
}

// A carried / thrown moby m (its record r from the class hook +0x10, `p` its other state), by
// r +0x68: 1 turning to face r +0 (then 2), 2 rising while r +0x6c counts to 1, 3 held by the
// moby r +0x60 (pulled to its chain point 0, scaled by distance; close enough it is handed over
// through the class hook +4), 4 resting, 5 / 6 flying at r +0x10 (homing on r +0x74, hitting
// what it touches, bouncing off floors, exploding on walls, on its timer +0x94, far from the
// hero or when it stops), 7 falling and sliding, 8 a debug print. Then its matrix and, below
// state 4, the class hook +0xc each time the countdown r +0xc runs out. Returns 1 when a slide
// settled it, else 0.
// orig: 0x002dc9f0
int32_t carry_update(uint32_t m, uint32_t p) {
    if (m == 0) return 0;
    const uint32_t s = lbu(m + 0x20);
    if (s == 0xfe) return 0;
    if (s == 0xfd) return 0;
    const uint32_t r = class_hook_10(m);
    const uint32_t P = 0x0013f350, HIT = OV(0x00173e40);
    if (r != 0) {
        V(r + 0x30) = V(m + 0x10);
        const int32_t st = static_cast<int16_t>(lhu(r + 0x68) - 1u);
        switch (static_cast<uint32_t>(st) < 8u ? st : -1) {
            case 0: {  // turning to face r +0
                const float tol = u2f(0x3c23d70au);
                const float yaw = angle_add(atan2_f(lwc1(r) - lwc1(m + 0x10), lwc1(r + 4) - lwc1(m + 0x14)), u2f(0x40490fdbu));
                const float acc = lwc1(0x0015ed64) * u2f(0x3e490fdbu);
                yaw_turn_damped(m, p + 0x44, yaw, acc, acc, lwc1(0x0015ed60) * u2f(0x3ec90ff9u));
                if (angle_sub(yaw, tol) < lwc1(m + 0x48) && lwc1(m + 0x48) < angle_add(yaw, tol)) {
                    sw(r + 0x6c, 0);
                    sh(r + 0x68, 2);
                }
                vec_clear(V(r + 0x10));
                break;
            }
            case 1: {  // rising
                swc1(m + 0x58, lwc1(r + 0x6c) + 1.0f);
                const float step = fpu_div(1.0f, int_to_float(frame_scale_i(1)));
                const float t = lwc1(r + 0x6c) + step;
                swc1(r + 0x6c, t);
                if (1.0f < t) swc1(r + 0x6c, 1.0f);
                swc1(m + 0x18, lwc1(m + 0x18) + fpu_div(lwc1(r + 0x6c), 100.0f) * lwc1(0x0015ed60));
                mat_to_quat_at(r + 0x20, m + 0xc0);
                vec_clear(V(r + 0x10));
                break;
            }
            case 2: {  // held by r +0x60
                sb(m + 0x31, 1);
                sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
                if (lwc1(r + 0x6c) < 1.0f) {
                    swc1(r + 0x6c, lwc1(r + 0x6c) + fpu_div(1.0f, int_to_float(frame_scale_i(0x1e))));
                    Vec4 ang{};
                    ang.z = u2f(0xbfc90fdbu);
                    ang.x = u2f(0xbfc90fdbu);
                    Mat4 e, h, hm;
                    euler_to_mat4_vu(e, ang);
                    mat3_copy_affine(h, M(lwu(r + 0x60) + 0xc0));
                    mat4_mul(hm, h, e);
                    Vec4 qa, q;
                    mat_to_quat(qa, hm);
                    quat_nlerp(q, V(r + 0x20), qa, lwc1(r + 0x6c));
                    quat_to_mat4(q, h);
                    mat3_copy(M(m + 0xc0), h);
                    sh(m + 0x34, (lhu(m + 0x34) | 4u) & 0x7fffu);
                } else {
                    swc1(r + 0x6c, 1.0f);
                }
                Vec4 at;
                moby_chain_point_v(lwu(r + 0x60), 0, at);
                const float d = vec2_distance(V(m + 0x10), at);
                const float k = lwc1(0x0015ed64) * u2f(0x3de147aeu), most = lwc1(0x0015ed60) * 0.75f;
                swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24) * (fpu_div(d, lwc1(r + 0x64) + lwc1(r + 0x64)) + 0.5f));
                const float pull = lwc1(r + 0x98) + k;
                swc1(r + 0x98, pull);
                if (most < pull) swc1(r + 0x98, most);
                if (d < lwc1(r + 0x98) * 70.0f) moby_anim_set(lwu(r + 0x60), 4, 0, frame_scale_i(2));
                if (d < 3.0f) {
                    sb(0x001403fc, 2);
                    sw(m + 0x94, 0);
                }
                if (d < lwc1(r + 0x98)) {  // handed over
                    sw(P + 0x207c, lwu(P + 0x207c) - 1u);
                    sw(P + 0x2078, lwu(P + 0x2078) + 1u);
                    const uint32_t at_ee = ee_stack_alloc(0x10);  // (the vector `at` is in the original's frame, a1 = sp)
                    V(at_ee) = at;
                    class_hook_4(m, at_ee);
                    ee_stack_free(0x10);
                    const uint32_t o = lwu(P + 0x2050 + static_cast<uint32_t>(lh(r + 0x6a)) * 4);
                    sh(o + 0x34, lhu(o + 0x34) & 0xefffu);
                    break;
                }
                Mat4 cam;
                mat3_transpose(cam, M(0x0013f990));
                Vec4 rel;
                vec3_sub(rel, V(m + 0x10), at);
                mat4_transform(rel, rel, cam);
                // x / y written, z 0; w: a stale stack lane in the original (0 here, not used)
                Vec4 off{};
                if (rel.x < 0.0f) {
                    if (0.0f < rel.y) {
                        const float nx = -rel.x;
                        off.y = nx < 2.0f ? nx : 2.0f;
                    } else {
                        off.y = -2.0f < rel.x ? rel.x : -2.0f;
                    }
                    off.x = -rel.x * 0.5f;
                } else {
                    off.y = rel.y * 0.5f;
                    off.x = abs_f(rel.y) < 2.0f ? abs_f(rel.y) : 2.0f;
                }
                off.z = 0.0f;
                mat3_transform(off, off, M(0x0013f990));
                off.z = 0.0f;
                vec3_add(at, at, off);
                debug_stub_1f6c28();  // (given the chain point before and after the offset)
                Vec4 step;
                vec3_sub(step, at, V(m + 0x10));
                vec3_set_length(step, step, lwc1(r + 0x98));
                vec3_add(V(m + 0x10), V(m + 0x10), step);
                moby_update_transform_kept(m);
                break;
            }
            case 3: {  // resting
                sb(m + 0x31, 0);
                sh(m + 0x34, (lhu(m + 0x34) | 3u) & 0xfffbu);
                sw(m + 0x94, 0);
                if ((lbu(m + 0x70) & 2u) == 0) break;
                const uint32_t set = lbu(lwu(r + 0x70) + 4);
                if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(3));
                swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24) * u2f(0x3e99999au));
                V(m + 0x40) = Vec4{};
                break;
            }
            case 4:
            case 5: {  // flying
                sb(m + 0x31, 1);
                sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
                if (lwc1(m + 0x18) < 2.0f) {
                    explode_and_reset(m, r, 0);
                    return 0;
                }
                {
                    const uint32_t cls = lwu(m + 0x24);
                    const float sc = lwc1(m + 0x2c) + lwc1(cls + 0x24) * (lwc1(r + 0x7c) * lwc1(0x0015ed60));
                    swc1(m + 0x2c, sc);
                    if (lwc1(cls + 0x24) < sc) swc1(m + 0x2c, lwc1(cls + 0x24));
                }
                const Vec4 vel0 = V(r + 0x10);  // (a stack copy: the tumble's speed is taken from it)
                Vec4& vel = V(r + 0x10);
                const uint32_t t = lwu(r + 0x74);
                bool homing = t != 0 && lbu(t + 0x20) != 0xfe && lbu(t + 0x20) != 0xfd;
                if (homing) {
                    const int32_t a = frame_scale_i(0x12c);
                    homing = lw(r + 0x94) < a - frame_scale_i(5);
                }
                if (homing) {  // lead the target: where it will be when the shot reaches it
                    Vec4 tp = V(t + 0x10);
                    tp.z = tp.z + lwc1(r + 0x90);
                    Vec4 tv, to;
                    vec3_sub(tv, tp, V(r + 0x40));
                    vec3_sub(to, tp, V(m + 0x10));
                    V(r + 0x40) = tp;
                    const float speed = vec3_length(vel);
                    float a = speed * speed - vec3_length_sq_vu(tv);
                    float b = ((tv.x * to.x + tv.y * to.y) + tv.z * to.z) * -2.0f;
                    const float c = -vec3_length_sq_vu(to);
                    const float root = sqrt_f(b * b - a * 4.0f * c);
                    b = -b;
                    a = a + a;
                    const float t1 = fpu_div(b + root, a), t2 = fpu_div(b - root, a);
                    float tm;
                    if (0.0f < t1 && 0.0f < t2) {
                        tm = t2 < t1 ? t1 : t2;
                    } else if (0.0f < t1) {
                        tm = t1;
                    } else if (0.0f < t2) {
                        tm = t2;
                    } else {
                        tm = -1.0f;
                    }
                    if (0.0f < tm) {
                        Vec4 aim;
                        vec3_scale(aim, tv, tm);
                        vec3_add(aim, aim, to);
                        vec3_set_length(aim, aim, lwc1(0x0015ed70) * 90.0f);
                        vec3_add(vel, vel, aim);
                        vec3_set_length(vel, vel, speed);
                    } else {
                        vec3_set_length(to, to, lwc1(0x0015ed70) * 90.0f);
                        vec3_add(vel, vel, to);
                        vec3_set_length(vel, vel, speed);
                    }
                }
                vec3_add(V(m + 0x10), V(m + 0x10), vel);
                alignas(16) uint32_t rec[12] = {};  // (+0x24.. not written: stack in the original)
                sub_00259888_v(2.0f, rec, static_cast<int32_t>(m), 0x00430000, vel);
                Vec4 dir;
                std::memcpy(&dir, rec, 16);
                vec2_set_length(dir, dir, 1.0f);
                dir.w = u2f(0x45afdf66u);
                dir.z = 1.0f;
                std::memcpy(rec, &dir, 16);
                rec[6] = 1u | (3u << 8) | (0x351u << 16);  // bytes +0x18 1, +0x19 3, half +0x1a 0x351
                uint32_t hit = static_cast<uint32_t>(mobys_in_sphere_v(V(m + 0x10), lwc1(r + 0x80), 1, m, rec));
                hit |= static_cast<uint32_t>(sphere_collide_v(V(m + 0x10), lwc1(r + 0x80), 0, m));
                if (hit != 0) {
                    V(m + 0x10) = V(OV(0x00173e70));
                    vec3_sub(V(r + 0x50), V(OV(0x00173e60)), V(m + 0x10));
                }
                if (lwu(HIT + 0x18) != 0 && lh(lwu(HIT + 0x18) + 0xa6) == 0) hit = 0;
                if (lh(r + 0x68) == 6) {  // tumbling about the contact
                    const uint32_t cls = lwu(m + 0x24);
                    sh(m + 0x34, lhu(m + 0x34) & 0x7fffu);
                    sh(m + 0x34, lhu(m + 0x34) | 4u);
                    sw(m + 0x94, lwu(cls + 0x10));
                    swc1(r + 0x58, 1.0f);
                    sw(r + 0x50, 0);
                    sw(r + 0x54, 0);
                    swc1(r + 0x5c, u2f(0x3c23d70au));
                    const float ang = fpu_div(vec3_length(vel0), lwc1(r + 0x80));
                    Vec4 q0;
                    mat_to_quat_at_v(q0, m + 0xc0);
                    vec3_set_length(V(r + 0x50), V(r + 0x50), 1.0f);
                    angle_wrap_mod(ang);  // (result unused)
                    Vec4 back, axis, q1, q;
                    vec3_set_length(back, vel, -1.0f);
                    vec3_cross_ba(axis, back, V(r + 0x50));
                    vec3_set_length(axis, axis, 1.0f);
                    quat_axis_angle(q1, axis, ang);
                    quat_mul(q, q0, q1);
                    Mat4 rm;
                    quat_to_mat4(q, rm);
                    vec3_set_length(V(m + 0xc0), rm.r[0], 1.0f);
                    vec3_set_length(V(m + 0xd0), rm.r[1], 1.0f);
                    vec3_set_length(V(m + 0xe0), rm.r[2], 1.0f);
                }
                if (hit != 0) {
                    int32_t kind = 0;
                    const uint32_t o = lwu(HIT + 0x18);
                    if (o != 0 && lwu(o + 0x24) != 0) kind = lh(lwu(o + 0x24) + 0x46);
                    vec3_set_length(V(HIT + 0x40), V(HIT + 0x40), 1.0f);
                    const uint32_t set = lbu(lwu(r + 0x70) + 6);
                    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(0x14));
                    sw(m + 0x98, lwu(r + 0x88));
                    sh(r + 0x68, 6);
                    if (u2f(0x3f5f66f3u) < atan2_f(lwc1(HIT + 0x48), vec2_length(V(HIT + 0x40)))) {  // a wall
                        if (lh(r + 0x9e) >= 0) moby_sound_start(lh(r + 0x9e), 0, m);
                        explode_and_reset(m, r, 1);
                        return 0;
                    }
                    if (lbu(0x0013e529) != 0 && !(kind < 5)) {
                        if (lh(r + 0x9e) >= 0) moby_sound_start(lh(r + 0x9e), 0, m);
                        explode_and_reset(m, r, 1);
                    }
                    Vec4 n{};
                    n.z = lwc1(HIT + 0x48);
                    vec3_reflect(vel, vel, n);
                }
                const bool expired = timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(r + 0x94))) != 0;
                if (expired || 60.0f < vec2_distance(V(m + 0x10), V(P + 0x80)) || vec3_length(vel) < lwc1(0x0015ed60) * u2f(0x3c23d70au)) {
                    explode_and_reset(m, r, 1);
                    return 0;
                }
                swc1(r + 0x18, lwc1(r + 0x18) - lwc1(0x0015ed64) * u2f(0x3c178d50u));
                spawn_particle_12(m + 0x10, m + 0xc0);
                break;
            }
            case 6: {  // falling and sliding
                sb(m + 0x31, 1);
                sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
                swc1(r + 0x18, lwc1(r + 0x18) - lwc1(0x0015ed64) * u2f(0x3c54fdf4u));
                if (lwc1(m + 0x18) < 2.0f) {
                    explode_and_reset(m, r, 1);
                    return 0;
                }
                const int32_t rad = float_to_int(lwc1(r + 0x8c) * 1024.0f);
                const uint32_t moved = walker_move_fx(m, r + 0x10, rad, 0, u2f(0x3f4ccccdu), 0.0f);
                const float ax = fpu_div(lwc1(m + 0x40), 1.5f), ay = fpu_div(lwc1(m + 0x44), 1.5f);
                swc1(m + 0x40, ax);
                swc1(m + 0x44, ay);
                if (moved != 0) {
                    swc1(m + 0x44, 0.0f);
                    swc1(m + 0x58, 1.0f);
                    swc1(m + 0x40, 0.0f);
                    return 1;
                }
                if ((lbu(m + 0x70) & 2u) == 0) break;
                swc1(m + 0x58, lwc1(r + 0x78));
                break;
            }
            case 7:
                debug_printf_stub();  // (the format at 0x1ea130)
                break;
            default:
                break;
        }
    }
    if ((lhu(m + 0x34) & 4u) != 0) moby_update_transform_kept(m);
    if (lh(r + 0x68) < 4) {
        if (lwc1(r + 0xc) <= 0.0f) {
            const uint32_t fn = lwu(lwu(lwu(m + 0x24) + 0x2c) + 0xc);
            if (fn != 0) ovl_call_moby_fn(fn, m);
        }
        swc1(r + 0xc, lwc1(r + 0xc) - 1.0f);
    }
    return 0;
}

namespace {
// orig: (helper) EE CVT.W.S: truncate toward zero, saturating
int32_t cvt_w_s(float f) {
    if (f >= 2147483648.0f) return INT32_MAX;
    if (f <= -2147483648.0f) return INT32_MIN;
    return static_cast<int32_t>(f);
}
}  // namespace

// The class-749 enemy of Veldin (pvars: path +0x1e0 / patrol path +0x1e8 / patrol point +0x274,
// trigger moby +0x1ec, the moby it wakes +0x1f0, target +0x180 / +0x1c0, home +0x1d0, wander
// record +0x240, carry record +0xd0 with its state at +0x70). After the hits and targets
// (0x2d5160), its floor band near the camera and the joint scale of node +0x200, by state:
// 0 set up (patrol path: walk it from state 3; a trigger moby: wait in state 1; neither: retire),
// 1 face the trigger moby (gone: state 8), 2 walk to the patrol point then wait (1), 3 / 4 walk
// the patrol path (then idle, 5), 5 idle wandering until a target shows (6), 6 chase the target
// (near and facing it: 7), 7 swing (chain point 0 hits), 8 go home (then 5), 9 carried
// (0x2dc9f0, landed: 5), 10 chase without crossing its path, 11 knocked back (landed: 5), 12 dying
// (landed: sound 7, death burst, retired). Then, unless carried or knocked back, it sits on the
// floor and is kept off its path; near the moby to wake (class 0x33e) it wakes it.
// orig: 0x002d4610
void e749_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    e749_hit_update(m);
    const uint32_t tgt = lwu(pv + 0x1c0);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 26.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x15);
    }
    joint_node_scale(m, 1, pv + 0x200, u2f(0x40066666u));
    auto anim = [m](uint32_t set, uint32_t frame, int32_t frames) { moby_anim_set(m, set, static_cast<int32_t>(frame), frames); };
    // the state change with its animation (when not already playing it) used by most states
    auto go = [m, &anim](uint32_t state, uint32_t set, int32_t frames) {
        sb(m + 0x20, state);
        if (lbu(m + 0x53) == set) return;
        anim(set, 0, frame_scale_i(frames));
    };
    const auto path_tbl = [](uint32_t idx) { return lwu(OV(0x001b04b0) + idx * 4); };
    const uint32_t st = lbu(m + 0x20);
    switch (st < 13 ? st : 13) {
        case 0: {  // set up
            swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24) * lwc1(OV(0x0016196c)));
            V(pv + 0x1d0) = V(m + 0x10);
            if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, 0);
            sb(pv + 0x29, 1);
            sw(pv + 0x140, OV(0x00161960));
            V(pv + 0x240) = V(m + 0x10);
            const float ft = lwc1(0x0015ed6c);
            swc1(pv + 0x25c, 1.5f);
            swc1(pv + 0x254, ft + ft);
            swc1(pv + 0x258, ft * u2f(0x403de44eu));
            Vec4 spot{};
            spot.y = 127.0f;
            spot.x = 150.0f;
            if (vec2_distance(V(m + 0x10), spot) < 5.0f) swc1(pv + 0x25c, 0.75f);
            sb(pv + 0x58, static_cast<uint32_t>(float_to_int(12.0f)));
            sb(pv + 0x5a, static_cast<uint32_t>(float_to_int(2.0f)));
            const int32_t patrol = lw(pv + 0x1e8);
            if (patrol < 0) {
                sb(m + 0x20, 5);
                break;
            }
            if (lw(pv + 0x1e4) >= 0) {
                const uint32_t path = path_tbl(static_cast<uint32_t>(patrol));
                V(pv + 0x1d0) = V(path + static_cast<uint32_t>(lw(path)) * 16);
                V(m + 0x10) = V(path + 0x10);
                sb(m + 0x20, 3);
                break;
            }
            if (lw(pv + 0x1ec) < 0) {
                moby_retire(m);
                return;
            }
            V(pv + 0x1d0) = V(m + 0x10);
            go(1, 2, 0xa);
            break;
        }
        case 1: {  // facing the trigger moby
            const uint32_t t = lwu(OV(0x0015ffd8)) + static_cast<uint32_t>(lw(pv + 0x1ec)) * 0x100;
            if (t == 0 || lbu(t + 0x20) == 0xfe || lbu(t + 0x20) == 0xfd) {
                go(8, 4, 0xa);
                break;
            }
            const float yaw = atan2_f(lwc1(t + 0x10) - lwc1(m + 0x10), lwc1(t + 0x14) - lwc1(m + 0x14));
            const float tau2 = u2f(0x41490fdbu);
            const float turn = lwc1(0x0015ed70) * tau2;
            yaw_seek(m + 0x48, pv + 0x1f4, yaw, turn, turn, lwc1(0x0015ed6c) * tau2);
            break;
        }
        case 2: {  // to the patrol point
            const uint32_t at = path_tbl(static_cast<uint32_t>(lw(pv + 0x1e8))) + static_cast<uint32_t>(lh(pv + 0x274)) * 16 + 0x10;
            if (!(e749_chase(m, at, 0.5f) < 1.0f)) break;
            go(1, 2, 0xa);
            break;
        }
        case 3:  // patrol start
            if (lw(pv + 0x1c4) == 2) break;
            go(4, 4, 0xa);
            break;
        case 4: {  // along the patrol path
            const uint32_t path = path_tbl(static_cast<uint32_t>(lw(pv + 0x1e8)));
            const uint32_t at = path + static_cast<uint32_t>(lh(pv + 0x274)) * 16 + 0x10;
            if (e749_chase(m, at, 0.5f) < 1.0f) {
                if (lh(pv + 0x274) < lw(path) - 1) {
                    sh(pv + 0x274, lhu(pv + 0x274) + 1u);
                    break;
                }
                go(5, 1, 0xa);
                break;
            }
            if (lw(pv + 0x1c4) == 2) break;
            go(6, 4, 0xa);
            break;
        }
        case 5: {  // idle, wandering
            if (lbu(m + 0x53) != 4) {
                const int32_t frame = rand_range(0, 5);
                if (lbu(m + 0x53) != 4) anim(4, static_cast<uint32_t>(frame), frame_scale_i(0xa));
                swc1(m + 0x58, rand_range_f(u2f(0x3f6b851fu), u2f(0x3f8a3d71u)));
            }
            wander_update(m, pv + 0x240, 0.5f, 0.5f);
            if (lw(pv + 0x1c4) == 2) break;
            swc1(pv + 0x1fc, rand_range_f(-20.0f, 20.0f) * u2f(0x3c8efa35u));
            go(6, 4, 0xa);
            break;
        }
        case 6: {  // chasing the target
            if (e749_chase(m, pv + 0x180, 0.5f) < lwc1(OV(0x00161980))) {
                const float a = atan2_f(lwc1(tgt + 0x10) - lwc1(m + 0x10), lwc1(tgt + 0x14) - lwc1(m + 0x14));
                if (angle_dist(lwc1(m + 0x48), a) < u2f(0x3e32b8c2u)) {
                    sb(m + 0x20, 7);
                    if (lbu(m + 0x53) == 5) break;
                    const int32_t frame = cvt_w_s(lwc1(OV(0x00161984)));
                    anim(5, static_cast<uint32_t>(frame), frame_scale_i(0xa));
                    break;
                }
            }
            if (lw(pv + 0x1c4) != 2) break;
            go(8, 4, 0xa);
            break;
        }
        case 7: {  // the swing
            if (anim_crossed(m, 34.0f) != 0) {
                moby_chain_contact(m, 0, 1, 0, 1, 0, u2f(0x3eaa7efau), 1.0f);
                break;
            }
            if ((lbu(m + 0x70) & 2u) == 0) break;
            if (lw(pv + 0x1c4) == 2) {
                go(8, 4, 0xa);
                break;
            }
            if (!(1.5f < vec2_distance(V(m + 0x10), V(tgt + 0x10)))) {
                const float a = atan2_f(lwc1(tgt + 0x10) - lwc1(m + 0x10), lwc1(tgt + 0x14) - lwc1(m + 0x14));
                if (!(u2f(0x3e32b8c2u) < angle_dist(lwc1(m + 0x48), a))) break;
            }
            go(6, 4, 0xa);
            break;
        }
        case 8:  // home
            if (e749_chase(m, pv + 0x1d0, 0.5f) < 1.5f) {
                go(5, 1, 0xa);
                break;
            }
            if (lw(pv + 0x1c4) == 2) break;
            go(6, 4, 0xa);
            break;
        case 9:  // carried
            if (carry_update(m, pv + 0x70) == 0) break;
            sb(m + 0x20, 5);
            sh(pv + 0x138, 0);
            break;
        case 10: {  // chasing without crossing its path
            Vec4 ahead;
            vec3_sub(ahead, V(pv + 0x180), V(m + 0x10));
            ahead.z = 0.0f;
            vec3_set_length(ahead, ahead, 2.5f);
            vec3_add(ahead, ahead, V(m + 0x10));
            if (path_crossed_v(static_cast<uint32_t>(lw(pv + 0x1e0)), V(m + 0x10), ahead) != 0) {
                const uint32_t t = lwu(pv + 0x1c0);
                const float yaw = atan2_f(lwc1(t + 0x10) - lwc1(m + 0x10), lwc1(t + 0x14) - lwc1(m + 0x14));
                const float tau2 = u2f(0x41490fdbu);
                const float turn = lwc1(0x0015ed70) * tau2;
                yaw_seek(m + 0x48, pv + 0x1f4, yaw, turn, turn, lwc1(0x0015ed6c) * tau2);
                if ((lbu(m + 0x70) & 2u) == 0) break;
                const uint32_t cur = lbu(m + 0x53);
                if (cur != static_cast<uint32_t>(rand_mod(2) + 2)) {
                    const int32_t set = rand_mod(2) + 2;
                    anim(static_cast<uint32_t>(set), 0, frame_scale_i(0x14));
                }
            } else {
                if (lbu(m + 0x53) != 4) anim(4, 0, frame_scale_i(0x14));
                e749_chase(m, pv + 0x180, 0.5f);
            }
            if ((lbu(m + 0x70) & 2u) == 0) break;
            if (lw(pv + 0x1c4) != 2) {
                go(6, 4, 0xa);
                break;
            }
            if (lh(pv + 0x276) != 0) break;
            go(8, 4, 0x14);
            break;
        }
        case 11:  // knocked back
            if ((enemy_physics(m, pv + 0x70) & 0x140u) == 0) break;
            go(5, 1, 0xa);
            break;
        case 12:  // dying
            if ((enemy_physics(m, pv + 0x70) & 0x140u) == 0) break;
            moby_sound_start(7, 0, m);
            enemy_death_burst(m, m + 0x10, 0.75f, 10.0f);  // ($a2 = -1 is passed too, unused)
            moby_retire(m);
            return;
        default:
            break;
    }
    const uint32_t s = lbu(m + 0x20);
    if (!(s - 0xbu < 2u) && s != 9) {
        approach_f(m + 0x18, floor_probe(0.5f, m + 0x10, 0), lwc1(0x0015ed6c) * 7.0f);
        if (lbu(m + 0x20) != 4) path_push_out_2d(static_cast<uint32_t>(lw(pv + 0x1e0)), m + 0x10, m + 0x10, 0.25f);
    }
    if (lw(pv + 0x1f0) == -1) return;
    const uint32_t o = lwu(OV(0x0015ffd8)) + static_cast<uint32_t>(lw(pv + 0x1f0)) * 0x100;
    if (!(vec2_distance(V(m + 0x10), V(o + 0x10)) < 1.0f)) return;
    if (lh(o + 0xa6) != 0x33e) return;
    moby_33e_wake(o);
    sw(pv + 0x1f0, 0xffffffffu);
}

}  // namespace rac1
