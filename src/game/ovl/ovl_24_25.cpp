// Ported overlay code 0x00240000..0x0025ffff.
#include <game/lib/libc.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl_common.h>
#include <runtime/kernel/bootstrap.h>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

namespace rac1 {

// orig: 0x0024f698 — register v in the 16-slot table at 0x196cc0: returns the
// slot already holding v or the first free one (and stores v there); -1 if full.
int32_t sub_0024f698(uint32_t v) {
    uint32_t p = OV(0x00196cc0);
    for (int32_t i = 0; i < 0x10; ++i, p += 4) {
        const uint32_t cur = lwu(p);
        if (cur == 0 || cur == v) {
            sw(p, v);
            return i;
        }
    }
    return -1;
}

// orig: 0x002502d0 — 64-bit OR of the (sign-extended) register arguments.
void moby_set_color_packed(uint32_t moby, int64_t a, int64_t b, int64_t c, int64_t d) {
    const uint64_t v = (static_cast<uint64_t>(a) << 32) | static_cast<uint64_t>(b) |
                       (static_cast<uint64_t>(c) << 8) | (static_cast<uint64_t>(d) << 16);
    sd(moby + 0x38, v);
}

// orig: 0x002502f0 — keep the low word of +0x38, replace the high one.
void moby_set_color_hi(uint32_t moby, int64_t a, int64_t b, int64_t c) {
    const uint64_t v = (ld(moby + 0x38) & 0xffffffffull) | (static_cast<uint64_t>(a) << 32) |
                       (static_cast<uint64_t>(b) << 40) | (static_cast<uint64_t>(c) << 48);
    sd(moby + 0x38, v);
}

// orig: 0x00250320
void moby_get_color_hi_v(uint32_t moby, uint32_t* out_a, uint32_t* out_b, uint32_t* out_c) {
    const uint64_t v = ld(moby + 0x38);
    *out_a = static_cast<uint32_t>(v >> 32) & 0xffu;
    *out_b = static_cast<uint32_t>(v >> 40) & 0xffu;
    *out_c = static_cast<uint32_t>(v >> 48) & 0xffu;
}
// orig: 0x00250320 (EE-address form)
void moby_get_color_hi(uint32_t moby, uint32_t out_a, uint32_t out_b, uint32_t out_c) {
    uint32_t a, b, c;
    moby_get_color_hi_v(moby, &a, &b, &c);
    sw(out_a, a);
    sw(out_b, b);
    sw(out_c, c);
}

// orig: 0x00250a40 — clear `count` consecutive bits starting at `bit` in the
// bitmap at 0x1ab7e0. Clearing an already-clear bit hits `teq zero,zero`
// (a debug trap) and stops; the EE interpreter treats the trap as a no-op.
int32_t sub_00250a40(uint32_t bit, int32_t count) {
    do {
        const uint32_t addr = OV(0x001ab7e0) + (bit >> 3);
        const uint32_t old = lbu(addr);
        const uint32_t v = old & ~(1u << (bit & 7));
        ++bit;
        --count;
        if (v == old) return 1;  // teq trap
        sb(addr, v);
    } while (count > 0);
    return 1;
}

namespace {
int32_t rand15() { return (libc_rand() >> 16) & 0x7fff; }
}  // namespace

// orig: 0x00257b50
int32_t rand_mod(int32_t n) { return ee_rem(rand15(), n); }

// orig: 0x00257b90
int32_t rand_range(int32_t lo, int32_t hi) {
    const int32_t n = static_cast<int32_t>(static_cast<uint32_t>(hi) - static_cast<uint32_t>(lo) + 1u);
    return static_cast<int32_t>(static_cast<uint32_t>(ee_rem(rand15(), n)) + static_cast<uint32_t>(lo));
}

// orig: 0x00257be8
float rand_range_f(float lo, float hi) {
    const float span = hi - lo;
    const float r = static_cast<float>(rand15());
    return lo + (r * span) * 3.0517578125e-05f;  // 0x38000000 = 2^-15
}

// orig: 0x00257cb0 — uniform angle in [-pi, pi).
float rand_angle() {
    const int32_t v = ((libc_rand() >> 16) & 0xfff) - 0x800;
    return (static_cast<float>(v) * 3.14159274f) * 4.8828125e-04f;  // 0x40490fdb, 0x3a000000 = 2^-11
}

// A random point on a sphere of random radius lo..hi (azimuth a, polar angle b; x y z only).
// orig: 0x00257d78
void rand_sphere_point_v(Vec4& out, float lo, float hi) {
    const float r = rand_range_f(lo, hi);
    const float a = rand_angle();
    const float b = rand_angle();
    out.x = (cos_vu(a) * sin_vu(b)) * r;
    out.y = (sin_vu(a) * sin_vu(b)) * r;
    out.z = cos_vu(b) * r;
}
// orig: 0x00257d78 (EE-address form)
void rand_sphere_point(uint32_t out, float lo, float hi) {
    Vec4 v = V(out);
    rand_sphere_point_v(v, lo, hi);
    V(out) = v;
}

// orig: 0x00259888 — src is passed in $a3.
void sub_00259888_v(float f, uint32_t* dst, int32_t a, int32_t b, const Vec4& src) {
    dst[4] = static_cast<uint32_t>(a);
    dst[5] = static_cast<uint32_t>(b);
    std::memcpy(&dst[7], &f, 4);
    dst[8] = 1;
    std::memcpy(dst, &src, 16);
}
// orig: 0x00259888 (EE-address form)
void sub_00259888(float f, uint32_t dst, int32_t a, int32_t b, uint32_t src) {
    sub_00259888_v(f, static_cast<uint32_t*>(ee_ram_ptr(dst)), a, b, V(src));
}

// orig: 0x0025a420 — the moby's 64-byte entry at 0x178100 (index in +0xa4,
// 0xff = none) if it still belongs to the moby and has any `mask` bit set;
// otherwise 0, and when `clear` is 0 a stale link is dropped.
uint32_t sub_0025a420(uint32_t moby, uint32_t mask, int32_t clear) {
    const uint32_t idx = lbu(moby + 0xa4);
    if (idx == 0xff) return 0;
    const uint32_t e = OV(0x00178100) + (idx << 6);
    const uint32_t owner = lwu(e + 0x34);
    if (owner != moby) return 0;
    if ((lwu(e + 0x24) & mask) != 0) return e;
    if (clear == 0) sb(owner + 0xa4, 0xff);
    return 0;
}

// orig: 0x0025b6a8 (EE FPU: an overflowing product or sum clamps to +-FLT_MAX, as for a
// degenerate segment's t = +-FLT_MAX)
float lerp_f(float a, float b, float t) { return vuc(a + vuc(vuc(b - a) * t)); }

// orig: 0x0025c338 — first word of the moby's pvars (flag 0x20 at +0x34).
uint32_t moby_pvar_word0(uint32_t moby) {
    if (moby == 0) return 0;
    if ((lhu(moby + 0x34) & 0x20) == 0) return 0;
    return lwu(lwu(moby + 0x78) + 0);
}

// orig: 0x0025d1b8
void moby_copy_hero_color(uint32_t moby) { sd(moby + 0x38, ld(lwu(g::HERO_MOBY) + 0x38)); }

// orig: 0x0025e3b8 — moby pointer inside [*(gp-0x6c28), *(gp-0x6c20)] and class 500..540.
int32_t moby_in_range_class_500(uint32_t moby) {
    if (moby == 0) return 0;
    if (moby < lwu(OV(0x00166c00u - 0x6c28))) return 0;
    if (lwu(OV(0x00166c00u - 0x6c20)) < moby) return 0;
    return (static_cast<uint32_t>(lhu(moby + 0xa6)) - 500u) < 0x29u ? 1 : 0;
}

// orig: 0x0024f590 — initialise a free node (byte +1 == 0) for anim slot `idx` of the
// moby's class and push it on the moby's list at +0x64.
void sub_0024f590(uint32_t moby, uint32_t idx, uint32_t node) {
    if (lbu(node + 1) != 0) return;
    sb(node + 0, idx);
    sb(node + 1, 1);
    swc1(node + 0x28, 1.0f);
    swc1(node + 0x1c, 1.0f);
    swc1(node + 0x20, 1.0f);
    swc1(node + 0x24, 1.0f);
    const uint32_t t = lwu(lbu(node + 0) * 4u + lwu(lwu(moby + 0x24) + 0x1c) + 4);
    sw(node + 4, (static_cast<uint32_t>(lbu(lbu(t) + t + 4)) << 6) + 0x70000000u);
    sw(node + 8, lwu(moby + 0x64));
    sw(moby + 0x64, node);
}

// orig: 0x00250aa0 — allocate a run of n bits in the bitmap at 0x1ab7e0 (first word that is
// not full, first fit at multiples of n). Returns the first bit. Note: as in the original, a run
// may be "found" past bit 31 of a word (its bits are then not set).
int32_t sub_00250aa0(int32_t n) {
    uint32_t at = OV(0x001ab7e0);
    int32_t base = -0x20;
    const uint64_t mask0 = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>((1u << (n & 31)) - 1u)));
    const uint32_t sh = static_cast<uint32_t>(n) & 63u;
    for (;;) {
        uint64_t w;
        do {
            w = lwu(at);
            at += 4;
            base += 0x20;
        } while (w == 0xffffffffull);
        uint64_t m = mask0;
        int32_t pos = 0;
        uint64_t hit;
        do {
            hit = w & m;
            m <<= sh;
            pos += n;
        } while (hit != 0);
        pos -= n;
        m >>= sh;
        if (pos == 0x20) continue;
        w |= m;
        sw(at - 4, static_cast<uint32_t>(w));
        return base + pos;
    }
}

// orig: 0x00257c48 — like rand_range_f with 12 bits, and the sign taken from bit 16 of rand().
float rand_range_signed_f(float lo, float hi) {
    const int32_t r = libc_rand() >> 16;
    const float span = hi - lo;
    float v = lo + (static_cast<float>(r & 0xfff) * span) * 2.44140625e-04f;  // 0x39800000 = 2^-12
    if ((r & 1) != 0) v = -v;
    return v;
}

// orig: 0x0025b868 — move p towards target by at most step; returns |target - p| afterwards.
float approach_f_v(float& p, float target, float step) {
    float d = target - p;
    if (step < d) {
        d = step;
    } else if (d < -step) {
        d = -step;
    }
    const float v = p + d;
    p = v;
    return abs_f(target - v);
}
// orig: 0x0025b868 (EE-address form)
float approach_f(uint32_t p, float target, float step) {
    return approach_f_v(*static_cast<float*>(ee_ram_ptr(p)), target, step);
}

// orig: 0x0025d7a0 — cur + delta wrapped modulo *count, or (wrap 0) clamped to the end the
// step goes towards.
int32_t step_index(uint32_t count, int32_t cur, int32_t delta, int32_t wrap) {
    if (wrap != 0) {
        const int32_t n = lw(count);
        const int32_t v = static_cast<int32_t>(static_cast<uint32_t>(cur) + static_cast<uint32_t>(delta) + static_cast<uint32_t>(n));
        return n == 0 ? v : (n == -1 ? 0 : v % n);
    }
    const int32_t v = static_cast<int32_t>(static_cast<uint32_t>(cur) + static_cast<uint32_t>(delta));
    if (delta < 0) return -1 < v ? v : 0;
    const int32_t last = lw(count) - 1;
    return last < v ? last : v;
}

// orig: 0x0025da70 — distance from a to b; only in xy when h > 0 and |a.z - b.z| < h.
float sub_0025da70_v(const Vec4& a, const Vec4& b, float h) {
    const Vec4 va = a, vb = b;
    if (0.0f < h && abs_f(va.z - vb.z) < h) return vec2_distance(va, vb);
    return vec3_distance(va, vb);
}
// orig: 0x0025da70 (EE-address form)
float sub_0025da70(uint32_t a, uint32_t b, float h) { return sub_0025da70_v(V(a), V(b), h); }

// orig: 0x0025f730 — clamp the vector's length to max.
void vec3_clamp_length_v(Vec4& v, float max) {
    if (max < vec3_length(v)) vec3_set_length(v, v, max);
}
// orig: 0x0025f730 (EE-address form)
void vec3_clamp_length(uint32_t v, float max) { vec3_clamp_length_v(V(v), max); }

// orig: 0x0025f780 — add the 16-bit offset of p from *(0x15ffd8) (bits 8..23) to the
// s16 set `list` (count at [0], max entries `max`) unless already present.
void sub_0025f780(uint32_t p, uint32_t list, int32_t max) {
    const int32_t key = static_cast<int32_t>((p - lwu(OV(0x0015ffd8))) << 8) >> 16;
    const int32_t m = static_cast<int16_t>(max);
    const int32_t n = lh(list);
    for (int32_t i = 1; i <= n; ++i)
        if (lh(list + static_cast<uint32_t>(i) * 2u) == key) return;
    if (!(lh(list) < m)) return;
    const uint32_t c = lhu(list) + 1u;
    sh(list, c);
    sh(list + static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(c)) * 2), static_cast<uint32_t>(key));
}

// orig: 0x0024f608 — unlink node from the moby's list (+0x64, next at +8) and clear it.
void sub_0024f608(uint32_t moby, uint32_t node) {
    if (node == 0) return;
    const uint32_t head = lwu(moby + 0x64);
    if (head == node) {
        sw(moby + 0x64, lwu(node + 8));
    } else {
        uint32_t p = head;
        for (;;) {
            const uint32_t n = lwu(p + 8);
            if (n == 0) break;
            if (n == node) {
                sw(p + 8, lwu(node + 8));
                break;
            }
            p = n;
        }
    }
    mem_fill32(node, 0, 0x40);
}

namespace {
// orig: 0x00250350 (shared body with 0x002503c0) — colour from the palette at 0x17fec0:
// entries a and b (bytes 0/1 of moby+0x38), blended by byte 2 (t/256) with vitof12/vmula/vmadd.
Vec4 palette_blend(uint32_t moby, uint32_t lane) {
    constexpr OvConst<0x0017fec0u> PAL{};
    const uint32_t lo = static_cast<uint32_t>(ld(moby + 0x38));
    const uint32_t a = lo & 0xffu, b = (lo >> 8) & 0xffu, t = (lo >> 16) & 0xffu;
    const Vec4 va = V(PAL + (a << 6) + lane);
    if (t == 0) return va;
    const Vec4 vb = V(PAL + (b << 6) + lane);
    const float x = static_cast<float>(static_cast<int32_t>(t << 4)) * (1.0f / 4096.0f);  // VITOF12
    const float w = vuc(1.0f - x);                                                          // vsubx.w vf3, vf0, vf3
    Vec4 r = va;  // w lane keeps the loaded value
    r.x = vuc(vuc(vuc(va.x) * w) + vuc(vb.x) * x);  // vmulaw.xyz ACC ; vmaddx.xyz
    r.y = vuc(vuc(vuc(va.y) * w) + vuc(vb.y) * x);
    r.z = vuc(vuc(vuc(va.z) * w) + vuc(vb.z) * x);
    return r;
}
}  // namespace

// orig: 0x00250350
void moby_palette_colour_1(uint32_t moby, uint32_t out) { V(out) = palette_blend(moby, 0x10); }
// orig: 0x00250350 (host-pointer form)
void moby_palette_colour_1_v(uint32_t moby, Vec4& out) { out = palette_blend(moby, 0x10); }

// orig: 0x002503c0
void moby_palette_colour_0(uint32_t moby, uint32_t out) { V(out) = palette_blend(moby, 0x00); }
// orig: 0x002503c0 (host-pointer form)
void moby_palette_colour_0_v(uint32_t moby, Vec4& out) { out = palette_blend(moby, 0x00); }

// orig: 0x002598b0 — mobys in the sphere (pos, radius) matching flags, the moby itself skipped;
// contact records carry (facing * speed with z = 1, w = 5627.95) built by sub_00259888 with
// (f, b), then bytes a, c and the moby's +0xa6 halfword. Facing: matrix row 0 when flag 0x100
// at +0x34, else (cos, sin, 0) of rot.z. Returns the hit count.
int32_t moby_sphere_contact_v(uint32_t moby, const Vec4& pos, int32_t b, uint32_t a, uint32_t c, uint32_t flags, float radius, float f, float speed) {
    Vec4 dir;
    if ((lhu(moby + 0x34) & 0x100) != 0) {
        dir = V(moby + 0xc0);
    } else {
        dir.x = cos_vu(lwc1(moby + 0x48));
        dir.y = sin_vu(lwc1(moby + 0x48));
        dir.z = 0.0f;
        dir.w = 0.0f;
    }
    vec3_scale(dir, dir, speed);
    dir.z = 1.0f;
    dir.w = u2f(0x45afdf66u);
    alignas(16) uint32_t rec[12] = {};
    std::memcpy(rec, &dir, 16);
    rec[4] = moby;
    rec[5] = static_cast<uint32_t>(b);
    rec[6] = (a & 0xff) | ((c & 0xff) << 8) | (static_cast<uint32_t>(lhu(moby + 0xa6)) << 16);
    std::memcpy(&rec[7], &f, 4);
    rec[8] = 1;
    return mobys_in_sphere_v(pos, radius, flags, moby, rec);
}
// orig: 0x002598b0 (EE-address form)
int32_t moby_sphere_contact(uint32_t moby, uint32_t pos, int32_t b, uint32_t a, uint32_t c, uint32_t flags, float radius, float f, float speed) {
    return moby_sphere_contact_v(moby, V(pos), b, a, c, flags, radius, f, speed);
}

// orig: 0x00259698 — floor below pos: segment from (pos.x, pos.y, pos.z + up) down to z = 0.01
// against everything matching flags | 2; the contact height (0x173e68) on a hit, else 0.
float floor_probe_v(float up, const Vec4& pos, uint32_t flags) {
    Vec4 lo = pos;
    lo.z = 0.01f;
    Vec4 hi = pos;
    hi.z = hi.z + up;
    if (segment_collide_v(hi, lo, flags | 2, 0, nullptr) == 0) return 0.0f;
    return lwc1(OV(0x00173e68));
}
// orig: 0x00259698 (EE-address form)
float floor_probe(float up, uint32_t pos, uint32_t flags) { return floor_probe_v(up, V(pos), flags); }

// orig: 0x00259468 — height reached after n = trunc(vel.z / g) frames of gravity g:
// pos.z + vel.z * n - g * n(n+1)/2; n stored in *frames_out when given.
float sub_00259468_v(const Vec4& pos, const Vec4& vel, int32_t* frames_out, float g) {
    const int32_t n = float_to_int(fpu_div(vel.z, g));
    const float fn = int_to_float(n);
    const float tri = int_to_float(static_cast<int32_t>(static_cast<uint32_t>(n) * static_cast<uint32_t>(n) + static_cast<uint32_t>(n)) >> 1);
    const float drop = tri * g;
    const float r = (pos.z + vel.z * fn) - drop;
    if (frames_out != nullptr) *frames_out = n;
    return r;
}
// orig: 0x00259468 (EE-address form)
float sub_00259468(uint32_t pos, uint32_t vel, int32_t* frames_out, float g) { return sub_00259468_v(V(pos), V(vel), frames_out, g); }

// orig: 0x00259508 — dst = base + vel * n - (0, 0, g * n(n+1)/2).
void sub_00259508_v(Vec4& dst, const Vec4& base, const Vec4& vel, int32_t n, float g) {
    Vec4 d;
    vec3_scale(d, vel, int_to_float(n));
    const float tri = int_to_float(static_cast<int32_t>(static_cast<uint32_t>(n) * static_cast<uint32_t>(n) + static_cast<uint32_t>(n)) >> 1);
    d.z = d.z - tri * g;
    vec3_add(dst, base, d);
}
// orig: 0x00259508 (EE-address form)
void sub_00259508(uint32_t dst, uint32_t base, uint32_t vel, int32_t n, float g) { sub_00259508_v(V(dst), V(base), V(vel), n, g); }

// orig: 0x00259f50 — quaternion from Euler angles: qx * qy * qz.
void quat_from_euler_v(Vec4& dst, const Vec4& angles) {
    Vec4 qx, qy, qz;
    quat_from_axis_angle(qx, 0, angles.x);
    quat_from_axis_angle(qy, 1, angles.y);
    quat_from_axis_angle(qz, 2, angles.z);
    quat_mul(dst, qx, qy);
    quat_mul(dst, dst, qz);
}
// orig: 0x00259f50 (EE-address form)
void quat_from_euler(uint32_t dst, uint32_t angles) { quat_from_euler_v(V(dst), V(angles)); }

// orig: 0x0025b6b8 — per-lane lerp of two quadwords.
void vec4_lerp_v(Vec4& dst, const Vec4& a, const Vec4& b, float t) {
    const Vec4 va = a, vb = b;
    dst.x = lerp_f(va.x, vb.x, t);
    dst.y = lerp_f(va.y, vb.y, t);
    dst.z = lerp_f(va.z, vb.z, t);
    dst.w = lerp_f(va.w, vb.w, t);
}
// orig: 0x0025b6b8 (EE-address form)
void vec4_lerp(uint32_t dst, uint32_t a, uint32_t b, float t) { vec4_lerp_v(V(dst), V(a), V(b), t); }

// orig: 0x0025bfe0 — pack four 0..1 floats as RGBA8 (x255, truncated).
uint32_t rgba_pack(float r, float g, float b, float a) {
    constexpr float K = 255.0f;  // 0x437f0000
    const uint32_t cr = static_cast<uint32_t>(float_to_int(r * K)) & 0xffu;
    const uint32_t cg = static_cast<uint32_t>(float_to_int(g * K)) & 0xffu;
    const uint32_t cb = static_cast<uint32_t>(float_to_int(b * K)) & 0xffu;
    const uint32_t ca = static_cast<uint32_t>(float_to_int(a * K));
    return cr | (cg << 8) | (cb << 16) | (ca << 24);
}

// orig: 0x0024f0e8 — unlink *slot from the moby's list (+0x60, next at +0x1c), clear the
// node and the slot.
void sub_0024f0e8(uint32_t moby, uint32_t slot) {
    const uint32_t node = lwu(slot);
    if (node == 0) return;
    const uint32_t head = lwu(moby + 0x60);
    if (head == node) {
        sw(moby + 0x60, lwu(node + 0x1c));
    } else {
        uint32_t p = head;
        for (;;) {
            const uint32_t n = lwu(p + 0x1c);
            if (n == 0) break;
            if (n == node) {
                sw(p + 0x1c, lwu(node + 0x1c));
                break;
            }
            p = n;
        }
    }
    mem_fill32(lwu(slot), 0, 0x40);
    sw(slot, 0);
}

// orig: 0x0024ebc0 — refresh the moby's current/next animation frame pointers (+0x68/+0x6c)
// from its class's animation sets (0xff in +0x52 = the default set at 0x18ecc0).
void sub_0024ebc0(uint32_t m) {
    const uint32_t a = lbu(m + 0x52);
    if (a == 0xff) {
        const uint32_t f = lbu(m + 0x50);
        sb(m + 0x7c, a);
        sb(m + 0x7e, 0);
        sw(m + 0x68, (f << 11) + OV(0x0018ecc0u));
    } else {
        const uint32_t sets = lwu(m + 0x24) + 0x48;
        const uint32_t set_a = sets + lbu(m + 0x52) * 4u;
        const uint32_t f4 = lbu(m + 0x50) * 4u;
        const uint32_t set_b = sets + lbu(m + 0x52) * 4u;
        const uint32_t frame = lwu(lwu(set_a) + f4 + 0x1c);
        const uint32_t set_c = sets + lbu(m + 0x52) * 4u;
        sw(m + 0x68, frame);
        sb(m + 0x7e, lbu(lwu(set_b) + 0x12));
        sb(m + 0x7c, lbu(lwu(set_c) + 0x11));
    }
    const uint32_t set = lwu(lwu(m + 0x24) + lbu(m + 0x53) * 4u + 0x48);
    sw(m + 0x6c, lwu(set + lbu(m + 0x51) * 4u + 0x1c));
}

// orig: 0x0024f028 — take a slot of the 6-entry pool at 0x18eb40 (first with byte +4 clear;
// index 6 if none, as in the original) for animation `idx` and push it on the moby's list +0x60.
uint32_t sub_0024f028(uint32_t moby, uint32_t idx) {
    constexpr OvConst<0x0018eb40u> POOL{};
    int32_t i = 0;
    if (lbu(POOL + 4) != 0) {
        uint32_t p = POOL + 4;
        i = 1;
        for (;;) {
            if (!(i < 6)) break;
            p += 0x40;
            if (lbu(p) == 0) break;
            ++i;
        }
    }
    const uint32_t e = POOL + (static_cast<uint32_t>(i) << 6);
    sb(e + 4, 1);
    sh(e + 0, idx);
    sw(e + 0x10, static_cast<uint32_t>(i) * 0x1500u + OV(0x00186d40u));
    sw(e + 0x14, lwu(moby + 0x24));
    const uint32_t off = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(idx)) * 4);
    const uint32_t t = lwu(off + lwu(lwu(moby + 0x24) + 0x1c) + 4);
    sh(e + 2, lbu(t + 2));
    sw(e + 0xc, t + lbu(t) + 4u);
    sw(e + 0x1c, lwu(moby + 0x60));
    sw(moby + 0x60, e);
    return e;
}

// orig: 0x0025abf0 — vertical speed to get from a to b in t = |b - a|_xy / speed frames
// under gravity g: -((a.z - b.z) + g t^2 / 2) / t (0 when t is 0); *t_out = t when given.
float sub_0025abf0(uint32_t a, uint32_t b, float* t_out, float speed, float g) {
    Vec4 d;
    vec3_sub(d, V(b), V(a));
    const float t = fpu_div(vec2_length(d), speed);
    if (t_out != nullptr) *t_out = t;
    if (t == 0.0f) return 0.0f;
    // (the EE FPU's mul / add / sub: a divide by speed 0 gives t = FLT_MAX, and t * t clamps, it does not overflow to Inf)
    const float k = fpu_mul(fpu_mul(g, fpu_mul(t, t)), 0.5f);
    const float r = -fpu_add(fpu_sub(lwc1(a + 8), lwc1(b + 8)), k);
    return fpu_div(r, t);
}

// orig: 0x0025bb38 — signed angle difference a - b, turned the other way round (+-2pi) when
// it disagrees with the sign of `dir`, unless it is below 0.1 degree (then 0).
float sub_0025bb38(int32_t dir, float a, float b) {
    const float d = angle_sub(a, b);
    if (dir == 0) return d;
    if (0.0f < d * static_cast<float>(dir)) return d;
    if (abs_f(d) <= u2f(0x3ae4c388u)) return 0.0f;
    if (0.0f < d) return d - u2f(0x40c90fdbu);
    return d + u2f(0x40c90fdbu);
}

// orig: 0x0025c170 — normalise the three columns of a 3x3 matrix.
void mat3_normalize_columns(uint32_t m) {
    for (uint32_t c = 0; c < 3; ++c) {
        Vec4 col;
        col.w = 0.0f;
        col.x = lwc1(m + c * 4 + 0x00);
        col.y = lwc1(m + c * 4 + 0x10);
        col.z = lwc1(m + c * 4 + 0x20);
        vec3_set_length(col, col, 1.0f);
        swc1(m + c * 4 + 0x00, col.x);
        swc1(m + c * 4 + 0x10, col.y);
        swc1(m + c * 4 + 0x20, col.z);
    }
}

// orig: 0x0024eed8 — store target `pos` (in the moby's local frame, x 1024/scale) and the two
// scaled parameters into entry idx (32 bytes) of the moby's table at +0x94.
void sub_0024eed8_v(uint32_t m, int32_t idx, const Vec4& pos, float a, float b) {
    const uint32_t e = lwu(m + 0x94) + static_cast<uint32_t>(idx << 5) + 0x10;
    const float k = fpu_div(1024.0f, lwc1(m + 0x2c));
    Mat4 rt;
    mat3_transpose(rt, M(m + 0xc0));
    Vec4 d;
    vec3_sub(d, pos, V(m + 0x10));
    mat4_transform(d, d, rt);
    vec3_scale(d, d, k);
    V(e + 0x10) = d;
    swc1(e + 0x1c, a * k);
    swc1(e + 0x04, b * k);
}
// orig: 0x0024eed8 (EE-address form)
void sub_0024eed8(uint32_t m, int32_t idx, uint32_t pos, float a, float b) { sub_0024eed8_v(m, idx, V(pos), a, b); }

// orig: 0x002577c8 — start animation `set` of the moby at `frame` (clamped to the set).
void moby_set_anim(uint32_t m, uint32_t set, int32_t frame) {
    const uint32_t sp = lwu(m + 0x24) + 0x48 + set * 4u;
    const int32_t n = lbu(lwu(sp) + 0x10);
    sb(m + 0x52, set);
    const int32_t f = frame < n ? frame : n - 1;
    sb(m + 0x50, static_cast<uint32_t>(f));
    sb(m + 0x51, static_cast<uint32_t>(f + 1));
    const int32_t last = static_cast<int32_t>(lbu(lwu(sp) + 0x10)) - 1;
    if (last < static_cast<int32_t>(static_cast<uint32_t>(f + 1) & 0xffu)) sb(m + 0x51, static_cast<uint32_t>(last));
    sb(m + 0x53, set);
    if (!(lbu(m + 0x51) < lbu(lwu(lwu(m + 0x24) + set * 4u + 0x48) + 0x10))) sb(m + 0x51, 0);
    sub_0024ebc0(m);
    const float t0 = lwc1(lwu(m + 0x68));
    sb(m + 0x70, lbu(m + 0x70) & 0xfdu);
    swc1(m + 0x5c, t0);
}

// orig: 0x002595a0 — real roots of a t^2 + b t + c: returns 2
// (two roots, *r_hi >= *r_lo), 1 (double root in *r_hi) or 0 (complex: the original still
// stores the values computed from |disc|).
int32_t solve_quadratic(float* r_hi, float* r_lo, float a, float b, float c) {
    const float disc = b * b - (a * 4.0f) * c;
    if (disc == 0.0f) {
        *r_hi = fpu_div(-b, a + a);
        return 1;
    }
    const float s = sqrt_f(abs_f(disc));
    const float nb = -b, a2 = a + a;
    const float r1 = fpu_div(nb + s, a2);
    const float r2 = fpu_div(nb - s, a2);
    *r_hi = r1;
    *r_lo = r2;
    const float v = *r_hi;
    if (v < r2) {
        *r_lo = v;
        *r_hi = r2;
    }
    return 0.0f < disc ? 2 : 0;
}

// orig: 0x0025b798 — damped spring: *p += k t - d *p, clamped to +-vmax (if vmax > 0) and to
// +-|t|. Returns +|t| when clamped to it from above, -|t| otherwise.
float spring_step(uint32_t p, float t, float k, float d, float vmax) {
    float v = lwc1(p) + (k * t - d * lwc1(p));
    swc1(p, v);
    if (0.0f < vmax) {
        if (vmax < v) swc1(p, vmax);
        else if (v < -vmax) swc1(p, -vmax);
    }
    if (abs_f(t) < lwc1(p)) {
        const float r = abs_f(t);
        swc1(p, r);
        return r;
    }
    const float lo = -abs_f(t);
    if (lwc1(p) < lo) swc1(p, -abs_f(t));
    return lo;
}

// orig: 0x0025bc00 — turn *ang towards target by at most `step` (direction rule of
// sub_0025bb38); returns the remaining difference.
float turn_towards(uint32_t ang, int32_t dir, float target, float step) {
    float d = sub_0025bb38(dir, target, lwc1(ang));
    if (step < d) d = step;
    else if (d < -step) d = -step;
    const float n = angle_add(lwc1(ang), d);
    swc1(ang, n);
    return sub_0025bb38(dir, target, n);
}

// orig: 0x0025f960 — is pos inside the unit box `idx` (-1: none) of the table *(0x1600ec)
// (128-byte entries: centre +0x30, inverse axes +0x40)?
int32_t point_in_box(uint32_t pos, int32_t idx) {
    if (idx == -1) return 0;
    const uint32_t box = lwu(OV(0x001600ec)) + static_cast<uint32_t>(idx << 7);
    Vec4 d;
    vec3_sub(d, V(pos), V(box + 0x30));
    d.w = 0.0f;
    Vec4 l;
    mat3_transform(l, d, M(box + 0x40));
    if (!(-1.0f <= l.x) || !(l.x <= 1.0f)) return 0;
    if (!(-1.0f <= l.y) || !(l.y <= 1.0f)) return 0;
    if (!(-1.0f <= l.z) || !(l.z <= 1.0f)) return 0;
    return 1;
}

// orig: 0x0024a7e8 — screen position (1/512 units) for HUD slot idx (>= 100: alternate
// layout; -1: current *(0x15ed84); out of 0..18: 0) from the table at 0x182a70.
void sub_0024a7e8(float* x, float* y, int32_t idx, float sx, float sy) {
    int32_t alt = 0;
    if (!(idx < 100)) {
        idx -= 100;
        alt = 1;
    }
    if (idx == -1) idx = lw(0x0015ed84);
    if (!(-1 < idx)) idx = 0;
    if (!(idx < 0x13)) idx = 0;
    constexpr OvConst<0x00182a70u> T{};
    const float k = u2f(0x3b000000u);
    if (idx == 6 && alt != 0) {
        *x = (lwc1(T + 0x6c) * sy + u2f(0x4483a000u)) * k;
        *y = (u2f(0x44390000u) - lwc1(T + 0x64) * sx) * k;
        return;
    }
    const uint32_t e = T + static_cast<uint32_t>(idx << 4);
    *x = (lwc1(e + 0) + lwc1(e + 4) * sx) * k;
    *y = (lwc1(e + 8) + lwc1(e + 0xc) * sy) * k;
}

// orig: 0x0024edc8 — current animation position of the moby in frames (frame times are s16
// 1/16 units at +4 of the frame records), interpolated by +0x54 inside one set.
float moby_anim_time(uint32_t m) {
    const uint32_t cur = lbu(m + 0x52) == 0xff ? lwu(m + 0x6c) : lwu(m + 0x68);
    const float t = lwc1(m + 0x54);
    if (t == 0.0f) return int_to_float(lh(cur + 4)) * 0.0625f;
    if (lbu(m + 0x52) != lbu(m + 0x53) || lbu(m + 0x51) < lbu(m + 0x50))
        return int_to_float(lh(cur + 4)) * 0.0625f + lwc1(m + 0x54);
    const uint32_t next = lwu(m + 0x6c);
    const float a = int_to_float(lh(cur + 4)) * (1.0f - lwc1(m + 0x54));
    const float b = int_to_float(lh(next + 4)) * lwc1(m + 0x54);
    return (a + b) * 0.0625f;
}

// orig: 0x0025b8c0 — spring p towards target with velocity *v (spring_step); snaps when
// within vmax/100. Returns the remaining distance (0 after a snap).
float spring_to_v(float& p, uint32_t v, float target, float k, float d, float vmax) {
    spring_step(v, target - p, k, d, vmax);
    const float n = p + lwc1(v);
    const float rest = target - n;
    p = n;
    if (abs_f(rest) < vmax * u2f(0x3c23d70au)) {
        p = target;
        sw(v, 0);
        return lwc1(v);
    }
    return rest;
}
// orig: 0x0025b8c0 (EE-address form)
float spring_to(uint32_t p, uint32_t v, float target, float k, float d, float vmax) {
    return spring_to_v(*static_cast<float*>(ee_ram_ptr(p)), v, target, k, d, vmax);
}

// orig: 0x0025d238 — pull dst back towards src along each axis where dst is negative, so that
// that coordinate becomes 0.1 (relative step |(src_k - 0.1) / (dst - src)_k|).
void sub_0025d238_v(const Vec4& src, Vec4& dst) {
    const Vec4 s = src;
    const float* sk = &s.x;
    Vec4 t;
    for (uint32_t k = 0; k < 3; ++k) {
        if (!((&dst.x)[k] < 0.0f)) continue;
        vec3_sub(t, dst, s);
        const float* tk = &t.x;
        const float f = abs_f(fpu_div(sk[k] - u2f(0x3dcccccdu), tk[k]));
        vec3_scale(t, t, f);
        vec3_add(dst, s, t);
    }
}
// orig: 0x0025d238 (EE-address form)
void sub_0025d238(uint32_t src, uint32_t dst) { sub_0025d238_v(V(src), V(dst)); }

// orig: 0x002483c8 — decode run-length 4-bit pixels (12-bit codes: count 8 bits, colour 4
// bits) of entry idx (offsets in `table`, data at base) into packed nibbles at dst.
void rle4_decode(uint32_t dst, int32_t idx, uint32_t base, uint32_t table) {
    uint32_t out = dst;
    int32_t half = 0;
    uint32_t src, endp;
    if (idx != 0) {
        endp = table + static_cast<uint32_t>(idx * 2);
        src = base + static_cast<uint32_t>(static_cast<int32_t>(lh(endp - 2)));
    } else {
        src = base + 0x200;
        endp = table;
    }
    const int32_t codes = static_cast<int32_t>((base + static_cast<uint32_t>(static_cast<int32_t>(lh(endp))) - src) << 1) / 3;
    for (int32_t k = 0; k < codes; ++k) {
        const uint32_t bit = static_cast<uint32_t>(k) * 3u;
        const uint32_t p = src + static_cast<uint32_t>(static_cast<int32_t>(bit) >> 1);
        uint32_t cnt = lbu(p + 1), col = lbu(p);
        if ((bit & 1) != 0) {
            col >>= 4;
        } else {
            const uint32_t c = (cnt << 4) | (col >> 4);
            col &= 0xf;
            cnt = c & 0xff;
        }
        const uint32_t hi = col << 4;
        int32_t n = cnt != 0 ? static_cast<int32_t>(cnt) : 0x100;
        const uint32_t pair = (col | hi) & 0xff;
        if (half != 0) {
            half = 0;
            n -= 1;
            sb(out, lbu(out) | hi);
            out += 1;
        }
        if (n == 0) continue;
        do {
            sb(out, pair);
            n -= 2;
            out += 1;
        } while (n > 0);
        if (n == 0) continue;
        out -= 1;
        half = 1;
        sb(out, col);
    }
}

// orig: 0x00256f14 — first object of the 16x16-unit grid cell of pos (grid *(0x15fbc0), 64
// cells per row; objects 32 bytes at *(0x15fb80)) whose xy distance^2 is below its w.
// Stores that distance^2 in *dist2; 0 if none. VU0 macro code, VU float rules.
uint32_t grid_find(uint32_t pos, float* dist2) {
    const Vec4 p = V(pos);
    const uint32_t cx = static_cast<uint32_t>(float_to_int(p.x)) >> 4;
    const uint32_t cy = static_cast<uint32_t>(float_to_int(p.y)) >> 4;
    const uint32_t grid = lwu(OV(0x0015fbc0));
    const uint32_t off = lwu(grid + (((cy << 6) + cx) << 2));
    if (off == 0) return 0;
    const uint32_t list = off + grid;
    int32_t left = lw(list) - 1;
    uint32_t it = list + 4;
    const uint32_t objs = lwu(OV(0x0015fb80));
    for (;;) {
        const uint32_t o = (lwu(it) << 5) + objs;
        it += 4;
        const Vec4 q = V(o);
        const float dx = vuc(vuc(p.x) - vuc(q.x)), dy = vuc(vuc(p.y) - vuc(q.y));
        const float d2 = vuc(vuc(dx * dx) + vuc(dy * dy));
        const float r = vuc(0.0f + vuc(q.w));
        const float s = vuc(d2 - r);
        if (std::signbit(s)) {
            *dist2 = d2;
            return o;
        }
        if (left == 0) return 0;
        --left;
    }
}

// orig: 0x00249d80 — make sure sprite `key` is decoded in the 8-entry cache at 0x1841f0
// (most recent first): on a miss the least recently used buffer is refilled (rle4_decode).
// Returns 1 when it decoded.
int32_t sprite_cache_get(uint32_t key) {
    constexpr OvConst<0x001841f0u> C{};
    if (lw(C + 0x28) == 0) return 0;
    int32_t lru = 0, i = 0, decoded = 0;
    if (lwu(C + 0x30) != key) {
        uint32_t e = C + 0x30;
        for (;;) {
            if (lw(e + 4) < lw(C + 0x34 + static_cast<uint32_t>(lru << 4))) lru = i;
            ++i;
            if (!(i < 8)) break;
            e += 0x10;
            if (lwu(e) == key) break;
        }
    }
    if (i == 8) {
        const uint32_t e = C + static_cast<uint32_t>(lru << 4);
        rle4_decode(lwu(e + 0x38), static_cast<int32_t>(key), lwu(C), lwu(C + 4));
        sw(e + 0x30, key);
        i = lru;
        decoded = 1;
    }
    if (i != 0) {
        const uint32_t first = C + 0x30, e = first + static_cast<uint32_t>(i << 4);
        const uint64_t a0 = ld(first), a1 = ld(first + 8), b0 = ld(e), b1 = ld(e + 8);
        sd(e, a0); sd(e + 8, a1);
        sd(first, b0); sd(first + 8, b1);
    }
    sw(C + 0x34, lwu(OV(0x0015f3f8)));
    return decoded;
}

// orig: 0x00256de0 — first of the spheres 0x17ecc0 (count *(0x15fbc4) + 1, w = radius^2,
// xy distance) containing pos whose box (matrix 0x17eec0 + 0x80*i) also contains it:
// *t = (local x + 1) / 2, *idx = i, returns 1. VU0 macro code, VU float rules.
int32_t sub_00256de0(uint32_t pos, float* t, int32_t* idx) {
    const uint32_t S = OV(0x0017ecc0), MATS = OV(0x0017eec0);
    const uint32_t end = (lwu(OV(0x0015fbc4)) << 4) + S;
    Vec4 p = V(pos);
    p.w = 1.0f;
    int32_t i = -1;
    for (uint32_t s = S;; s += 0x10) {
        const Vec4 q = V(s);
        ++i;
        const float dx = vuc(vuc(p.x) - vuc(q.x)), dy = vuc(vuc(p.y) - vuc(q.y));
        const float d2 = vuc(vuc(dx * dx) + vuc(dy * dy));
        if (std::signbit(vuc(d2 - vuc(0.0f + vuc(q.w))))) break;
        if (s == end) return 0;
    }
    const Mat4& m = M(MATS + static_cast<uint32_t>(i) * 0x80u);
    float r[4];
    const float* c[4] = {&m.r[0].x, &m.r[1].x, &m.r[2].x, &m.r[3].x};
    for (int k = 0; k < 4; ++k) {
        float acc = vuc(vuc(c[0][k]) * vuc(p.x));
        acc = vuc(acc + vuc(c[1][k]) * vuc(p.y));
        acc = vuc(acc + vuc(c[2][k]) * vuc(p.z));
        r[k] = vuc(acc + vuc(c[3][k]) * vuc(p.w));
    }
    // VCLIPw against w = 1.0 (integer compare of the float bits, as the hardware)
    constexpr int32_t ONE = 0x3f800000;
    for (int k = 0; k < 3; ++k) {
        int32_t b;
        std::memcpy(&b, &r[k], 4);
        if (b > ONE || static_cast<int32_t>(static_cast<uint32_t>(b) ^ 0x80000000u) > ONE) return 0;
    }
    const float x = vuc(0.5f * vuc(vuc(r[0]) + 1.0f));
    *idx = i;
    *t = x;
    return 1;
}

// orig: 0x00259bc8 L13:0x00265c30 — record an event for moby in the 64-entry ring 0x178100 (next index
// *(0x173e54)), unless its current entry is stronger than `strength`. (Gemlik's build, its own ring and counter: the same
// instructions, proved by batch 410.)
void sub_00259bc8_v(uint32_t moby, uint32_t a, uint32_t b, uint32_t pos, const Vec4& dir, float strength) {
    const uint32_t RING = OV(0x00178100), NEXT = OV(0x00173e54);
    const uint32_t s = lwu(NEXT);
    const uint32_t cur = lbu(moby + 0xa4);
    if (cur != 0xff) {
        const uint32_t e = RING + (cur << 6);
        if (lwu(e + 0x34) == moby && strength < lwc1(e + 0x2c)) return;
    }
    const uint32_t e = RING + (s << 6);
    sw(e + 0x20, a);
    sw(e + 0x24, b);
    swc1(e + 0x2c, strength);
    sw(e + 0x34, moby);
    sw(e + 0x38, 0);
    sw(e + 0x30, u2f(0x38d1b717u) < vec3_length(dir) ? 1u : 0u);
    lq_sq(e, pos);
    V(e + 0x10) = dir;
    sb(moby + 0xa4, s);
    sw(NEXT, (s + 1) & 0x3f);
}
// orig: 0x00259bc8 (EE-address form)
void sub_00259bc8(uint32_t moby, uint32_t a, uint32_t b, uint32_t pos, uint32_t dir, float strength) {
    sub_00259bc8_v(moby, a, b, pos, V(dir), strength);
}

// orig: 0x0025d330 — Euler angles (x roll, y pitch, z yaw) of a rotation matrix.
void mat_to_euler_r(const Mat4& m, Vec4& out) {
    Mat4 l = m;
    vec_clear(l.r[3]);
    l.r[3].w = 1.0f;
    const float yaw = atan2_f(l.r[0].x, l.r[0].y);
    Mat4 r;
    Vec4 ang{0.0f, 0.0f, -yaw, 0.0f};
    euler_to_mat4_vu(r, ang);
    mat4_mul(l, r, l);
    const float pitch = atan2_f(l.r[0].x, -l.r[0].z);
    ang = Vec4{0.0f, -pitch, 0.0f, 0.0f};
    euler_to_mat4_fpu(r, ang);
    mat4_mul(l, r, l);
    const float roll = atan2_f(l.r[1].y, l.r[1].z);
    out.z = yaw;
    out.y = pitch;
    out.x = roll;
}
// orig: 0x0025d330 (EE-address output)
void mat_to_euler_v(const Mat4& m, uint32_t out) { mat_to_euler_r(m, V(out)); }
// orig: 0x0025d330 (EE-address form)
void mat_to_euler(uint32_t m, uint32_t out) { mat_to_euler_v(M(m), out); }

// orig: 0x00240348 — write a 4-qword GS packet header for texture slot idx (0x182640 table,
// 24 bytes per slot; idx -1/-2/-3: fixed setups 0x1827c0 / 0x1827d8) at dst. All arguments are
// 64-bit registers (sign-extended).
void sub_00240348(uint32_t dst, int64_t a, int64_t b, int64_t c, int64_t d, int64_t idx) {
    constexpr OvConst<0x00182640u> T{};
    const uint32_t i32 = static_cast<uint32_t>(idx);
    const uint64_t t2 = ld(T + i32 * 0x18u);
    const uint64_t t0 = ld(T + (i32 * 3u + 1u) * 8u);
    const uint64_t t1 = ld(T + (i32 * 3u + 2u) * 8u);
    const uint64_t head = (static_cast<uint64_t>(a) << 32) | 0x20u | (static_cast<uint64_t>(b) << 6);
    if (idx >= 0) {
        sd(dst + 0x00, (t0 & 0x1cu) | ((static_cast<uint64_t>(b) << 6) | 0x20u) | (static_cast<uint64_t>(a) << 32));
        sd(dst + 0x10, (static_cast<uint64_t>(c) | (static_cast<uint64_t>(d) << 2)) | (static_cast<uint64_t>(idx) << 24));
        sd(dst + 0x30, t1);
        sd(dst + 0x20, t2);
        return;
    }
    sd(dst + 0x00, head);
    sd(dst + 0x10, 5);
    if (idx < -1) {
        const uint32_t s = idx == -3 ? OV(0x001827d8u) : OV(0x001827c0u);
        sd(dst + 0x20, ld(s));
        sd(dst + 0x30, ld(s + 0x10));
        return;
    }
    sd(dst + 0x20, 0x80000004cc007ffbull);
    sd(dst + 0x30, 0);
}

// orig: 0x0024f440 — build the joint-matrix block for animation `set`/`frame` of the moby's
// class at dst: header (counts/offsets), the frame's matrices, then the class's fixed joints
// scattered by their index list; ends with FlushCache(0) (no-op here).
void sub_0024f440(uint32_t m, uint32_t set, uint32_t frame, uint32_t dst) {
    const uint32_t cls = lwu(m + 0x18);
    const uint32_t fixed = lbu(cls + 8);
    const uint32_t joints = lbu(lwu(m + 0x14) + 8);
    const uint32_t fr = lwu(lwu(cls + set * 4u + 0x48) + frame * 4u + 0x1c);
    mem_clear128(dst, static_cast<int32_t>(((joints + 3) << 3) & 0xff0));
    sh(dst + 8, joints << 3);
    sh(dst + 6, static_cast<uint32_t>(static_cast<int32_t>(joints + static_cast<uint32_t>(lh(fr + 0xa)) + static_cast<uint32_t>(lh(fr + 0xe)) + 1u) >> 1));
    const uint32_t a = lhu(fr + 0xa);
    sh(dst + 0xa, a);
    sh(dst + 0xc, (a + joints) << 3);
    sh(dst + 0xe, lhu(fr + 0xe));
    const int32_t n = static_cast<int32_t>((static_cast<uint32_t>(lh(fr + 6)) << 4) - (fixed << 3));
    if (n != 0) mem_copy(dst + (joints << 3) + 0x10, fr + (fixed << 3) + 0x10, n);
    uint32_t list = lwu(lwu(m + 0x18) + 0x1c) + 4;
    uint32_t src = fr + 0x10;
    for (uint32_t k = fixed; k != 0; --k) {
        const uint32_t b = lbu(list);
        const uint64_t v = ld(src);
        list += 1;
        src += 8;
        sd((b << 3) + dst + 0x10, v);
    }
    const uint32_t q = dst + static_cast<uint32_t>(lh(dst + 6) << 4);
    if (ld(q) == 0) sd(q, 1);
}

// orig: 0x0025bc98 — spring an angle `ang` towards target with angular velocity *vel
// (spring_step) along the direction rule of sub_0025bb38 (dir 2: pick the side from the
// signs); snaps when within vmax/100. Returns the remaining difference (0 after a snap).
float spring_angle_v(float& ang, uint32_t vel, int32_t dir, float target, float k, float d, float vmax) {
    if (dir == 2) {
        dir = 0;
        const float a = ang;
        if ((0.0f < target && a < 0.0f) || (target < 0.0f && 0.0f < a)) dir = target < 0.0f ? -1 : 1;
    }
    const float diff = sub_0025bb38(dir, target, ang);
    spring_step(vel, diff, k, d, vmax);
    const float n = angle_add(ang, lwc1(vel));
    ang = n;
    const float rest = sub_0025bb38(dir, target, n);
    if (abs_f(rest) < vmax * u2f(0x3c23d70au)) {
        ang = target;
        sw(vel, 0);
        return lwc1(vel);
    }
    return rest;
}
// orig: 0x0025bc98 (EE-address form)
float spring_angle(uint32_t ang, uint32_t vel, int32_t dir, float target, float k, float d, float vmax) {
    return spring_angle_v(*static_cast<float*>(ee_ram_ptr(ang)), vel, dir, target, k, d, vmax);
}

// orig: 0x0025db00 — closest point to p on the segment a-b in the xy plane (z = 0) into out;
// returns the xy distance from p to it.
float closest_point_on_segment_xy_r(Vec4& out, const Vec4& p, const Vec4& A, const Vec4& B) {
    Vec4 d;
    vec3_sub(d, A, B);
    Vec4 n{0.0f, 0.0f, 0.0f, 0.0f};
    n.x = -d.y;
    n.y = d.x;
    Vec4 t = A;
    t.z = 0.0f;
    vec2_set_length(n, n, 1.0f);
    const float off = -vec3_dot(n, t);
    const float s = off + (n.x * p.x + n.y * p.y);
    out.x = p.x - s * n.x;
    out.z = 0.0f;
    out.y = p.y - s * n.y;
    bool outside = false;
    for (uint32_t k = 0; k < 2; ++k) {
        const float o = (&out.x)[k], av = (&A.x)[k], bv = (&B.x)[k];
        if (av < o) {
            if (bv < o) { outside = true; break; }
        } else if (o < av) {
            if (o < bv) { outside = true; break; }
        }
    }
    if (outside) {
        const float da = vec2_distance(out, A);
        const float db = vec2_distance(out, B);
        out = (da < db ? A : B);
        out.z = 0.0f;
    }
    return vec2_distance(p, out);
}
// orig: 0x0025db00 (EE-address form)
float closest_point_on_segment_xy_v(Vec4& out, const Vec4& p, uint32_t a, uint32_t b) { return closest_point_on_segment_xy_r(out, p, V(a), V(b)); }
// orig: 0x0025db00 (EE-address form)
float closest_point_on_segment_xy(uint32_t out, uint32_t p, uint32_t a, uint32_t b) { return closest_point_on_segment_xy_v(V(out), V(p), a, b); }

namespace {
// orig: 0x00257024 (MMI helper) — PMULTH of a byte by a 16-bit factor, PSRLH 8 of the low
// halfword: bits 8..15 of the signed 16x16 product.
uint32_t mul_hi_byte(uint32_t factor, uint32_t byte) {
    const int32_t p = static_cast<int32_t>(static_cast<int16_t>(factor & 0xffff)) * static_cast<int32_t>(byte);
    return (static_cast<uint32_t>(p) >> 8) & 0xffu;
}
// orig: 0x00257024 (MMI helper) — PADDUB: saturating unsigned byte add.
uint32_t add_sat(uint32_t a, uint32_t b) { return a + b > 0xff ? 0xffu : a + b; }
}  // namespace

// orig: 0x00257024 — moby lighting (hand-written MMI code): ambient colour from the box zone
// (sub_00256de0, blending zone colours +0x40/+0x44 by the position) into +0x80/+0x38, then the
// colour of the nearest point light (grid_find) faded by distance added into +0x3c.
// Note: the original clobbers the caller's $s0 on the box path (not modelled).
uint32_t moby_update_light(uint32_t m) {
    float t;
    int32_t idx;
    if (sub_00256de0(m + 0x10, &t, &idx) != 0) {
        const uint32_t z = OV(0x0017eec0) + (static_cast<uint32_t>(idx) << 7);
        if ((lw(z + 0x50) & 1) != 0) {
            const int32_t k = float_to_int(255.0f * t);
            const uint32_t ca = lwu(z + 0x40), cb = lwu(z + 0x44);
            const uint32_t inv = static_cast<uint32_t>(0xff - k);
            uint32_t out = 0;
            for (uint32_t i = 0; i < 4; ++i) {
                const uint32_t a = mul_hi_byte(inv, (ca >> (8 * i)) & 0xff);
                const uint32_t b = mul_hi_byte(static_cast<uint32_t>(k), (cb >> (8 * i)) & 0xff);
                out |= add_sat(a, b) << (8 * i);
            }
            sw(m + 0x80, out);
            sw(m + 0x38, lwu(z + 0x48) | (lwu(z + 0x4c) << 8) | (static_cast<uint32_t>(k) << 16));
        }
    }
    float d2;
    const uint32_t light = grid_find(m + 0x10, &d2);
    if (light == 0) {
        sw(m + 0x3c, lwu(m + 0x80));
        return 0;
    }
    const int32_t k = float_to_int(fpu_div(d2, lwc1(light + 0xc)) * 255.0f);
    const uint32_t f = static_cast<uint32_t>(0xff - k);
    const uint32_t c = lwu(light + 0x10), base = lwu(m + 0x80);
    uint32_t out = 0;
    for (uint32_t i = 0; i < 4; ++i) out |= add_sat(mul_hi_byte(f, (c >> (8 * i)) & 0xff), (base >> (8 * i)) & 0xff) << (8 * i);
    sw(m + 0x3c, out);
    return light;
}

// orig: 0x0024e930 — initialise a moby of class `cls`: clear 0x100 bytes, set defaults, look
// up its class data (0x197300 by the class index 0x197bc0[cls]) and animation state.
void moby_init(uint32_t m, uint32_t cls) {
    mem_fill32(m, 0, 0x100);
    const uint32_t ci = lbu(OV(0x00197bc0) + cls);
    sb(m + 0x23, 0x80);
    sb(m + 0x22, ci);
    sb(m + 0xa4, 0xff);
    sb(m + 0x21, 0xff);
    sb(m + 0x71, 0xff);
    sb(m + 0x72, 0xff);
    const uint32_t lp = OV(0x00197680) + (ci & 0xff) * 4u;
    sh(m + 0xa6, cls);
    const int32_t rel = static_cast<int32_t>(m - lwu(OV(0x0015ffd8))) >> 8;
    sd(m + 0x38, 0x0040404000000000ull);
    sh(m + 0x36, 0x7f80);
    sw(m + 0xac, static_cast<uint32_t>(rel));
    sw(m + 0xa8, static_cast<uint32_t>(rel) << 16);
    sb(m + 0x7d, 0xff);
    sb(m + 0xa1, 0x7f);
    sb(m + 0xa3, 0x80);
    sb(m + 0x7e, 0);
    sb(m + 0x7c, 0xff);
    sb(m + 0xa0, 0x7f);
    sb(m + 0xa2, 0x80);
    const uint32_t l = lwu(lp);
    sw(m + 0x74, l);
    if (l == 0) sh(m + 0x34, lhu(m + 0x34) | 2u);
    const uint32_t c = lwu(OV(0x00197300) + lbu(m + 0x22) * 4u);
    if (c == 0) {
        const uint32_t f = lhu(m + 0x34);
        sw(m + 0x94, 0);
        sw(m + 0x24, 0);
        sh(m + 0x34, f | 5u);
        return;
    }
    sw(m + 0x24, c);
    uint32_t f = lhu(m + 0x34);
    sb(m + 0x72, lbu(c + 0xe));
    f |= lhu(c + 0x44);
    sh(m + 0x34, f);
    sw(m + 0x94, lwu(c + 0x10));
    swc1(m + 0x5c, 1.0f);
    swc1(m + 0x2c, lwc1(c + 0x24));
    swc1(m + 0x58, 1.0f);
    if (lw(c + 0x40) != 0) {
        sh(m + 0x34, f | 0x10u);
        sw(m + 0x90, lwu(c + 0x40));
    }
    if (lbu(lwu(m + 0x24) + 0xf) != 0) {
        const uint32_t f2 = lhu(m + 0x34);
        sb(m + 0x7f, 0x18);
        sw(m + 0x84, 0);
        sh(m + 0x34, f2 | 0x400u);
        sw(m + 0x88, 0);
        sb(m + 0xbd, 0);
    }
    if (lbu(lwu(m + 0x24) + 6) != 0) sb(m + 0x73, 0x18);
    if (lw(lwu(m + 0x24) + 0x48) == 0) return;
    sub_0024ebc0(m);
    if (!(lbu(lwu(lwu(m + 0x24) + 0x48) + 0x10) < 2)) sh(m + 0x34, lhu(m + 0x34) & 0xfffdu);
    const uint32_t cc = lwu(m + 0x24);
    if (lbu(cc + 0xc) != 1) return;
    if (!(lbu(lwu(cc + 0x48) + 0x10) < 2)) return;
    sw(m + 0x58, 0);
    if (static_cast<int8_t>(lbu(lwu(cc + 0x48) + 0x11)) >= 0) return;
    sh(m + 0x34, lhu(m + 0x34) | 0x40u);
}

// orig: 0x0024e838 — allocate a moby of class `cls` from the moby array (*(0x15ffdc) ..
// *(0x15ffe0), 0x100 bytes each): the first slot marked free (+0x20 >= 0xfe) whose release
// time +0x38 has passed (*(0x15f5cc)); 0xff marks the end of the used range and moves one
// slot up. Also hands it its 0x80-byte pvar block (*(0x15ffe8)). 0 (and a debug print) if full.
uint32_t moby_spawn(uint32_t cls) {
    const uint32_t end = lwu(OV(0x0015ffe0));
    uint32_t m = lwu(OV(0x0015ffdc));
    for (; m < end; m += 0x100) {
        const uint32_t b = lbu(m + 0x20);
        if (b < 0xfe) continue;
        if (static_cast<uint64_t>(lwu(OV(0x0015f5cc))) < ld(m + 0x38)) continue;
        if (b == 0xff) sb(m + 0x120, b);
        moby_init(m, cls);
        const uint32_t idx = static_cast<uint32_t>(static_cast<int32_t>(m - lwu(OV(0x0015ffdc))) >> 8);
        const uint32_t pv = lwu(OV(0x0015ffe8)) + (idx << 7);
        sw(m + 0x78, pv);
        mem_fill32(pv, 0, 0x80);
        const uint32_t c = lwu(OV(0x0015ffbc));
        if (c != 0) sw(OV(0x0015ffbc), c - 1u);
        return m;
    }
    debug_printf_stub();  // "out of mobys" with *(0x15f5cc)
    return 0;
}

// orig: 0x002400f0 — GS packet header for texture `tex` (width/height log2 from +4/+6,
// buffer addresses +8..+0xe relative to *(0x15ee8c) >> 8), see sub_00240348 for the slot
// forms (idx -1/-2/-3). 64-bit register arguments.
void sub_002400f0(uint32_t dst, uint32_t tex, int64_t a, int64_t b, int64_t c, int64_t d, int64_t idx) {
    const int32_t w = static_cast<int32_t>(static_cast<uint32_t>(lhu(tex + 4)) << 16);
    int32_t tw = w >> 0x17;
    int32_t th = w >> 0x16;
    if (!(0 < tw)) tw = 1;
    if (!(0 < th)) th = 1;
    const int32_t lw2 = highest_bit_index(w >> 16);
    const int32_t lh2 = highest_bit_index(lh(tex + 6));
    const int32_t off = lw(0x0015ee8c) >> 8;
    const int64_t e = static_cast<int32_t>(static_cast<uint32_t>(lh(tex + 0xe)) + static_cast<uint32_t>(off));
    const int64_t f = static_cast<int32_t>(static_cast<uint32_t>(lh(tex + 0xa)) + static_cast<uint32_t>(off));
    const int64_t h = static_cast<int32_t>(static_cast<uint32_t>(lh(tex + 0xc)) + static_cast<uint32_t>(off));
    const int64_t s8 = lh(tex + 8);
    auto U = [](int64_t v) { return static_cast<uint64_t>(v); };
    if (idx >= 0) {
        sd(dst + 0x00, (U(s8 - 1) << 2) | ((U(b) << 6) | 0x20u) | (U(a) << 32));
        sd(dst + 0x10, U(c) | (U(d) << 2) | (U(idx) << 24));
        const uint64_t q2 = (U(th) << 14) | ((U(lw2) << 26) | 0x01300000u) | (U(lh2) << 30) | ((U(f) << 37) | 0x400000000ull) |
                            0x8000000000000000ull;
        const uint64_t q3 = (U(tw) << 14) | (U(h) << 20) | ((U(e) << 40) | 0x400000000ull) | 0x0040000000000000ull;
        sd(dst + 0x30, q3);
        sd(dst + 0x20, q2);
        return;
    }
    sd(dst + 0x00, (U(b) << 6) | ((U(a) << 32) | 0x20u));
    sd(dst + 0x10, 5);
    if (idx < -1) {
        const uint32_t s = idx == -3 ? OV(0x001827d8u) : OV(0x001827c0u);
        sd(dst + 0x20, ld(s));
        sd(dst + 0x30, ld(s + 0x10));
        return;
    }
    sd(dst + 0x20, 0x80000004cc007ffbull);
    sd(dst + 0x30, 0);
}

// orig: 0x0025d808 — move along a polyline path (count at +0, points from +0x10, 16 bytes,
// w = segment length) by `step` (negative: backwards) from segment *idx, offset *frac; writes
// the position to out. `loop` wraps at the ends; returns 1 when an end was reached.
int32_t path_advance_v(uint32_t path, Vec4& out, int32_t* idx, float* frac, int32_t loop, float step) {
    int32_t ended = 0;
    int32_t dir = 1;
    if (step < 0.0f) {
        step = -step;
        dir = -1;
    }
    auto seg_len = [path](int32_t i) { return lwc1(path + static_cast<uint32_t>(i << 4) + 0x1c); };
    int32_t n = lw(path);
    int32_t i = *idx;
    while (!(step == 0.0f) && (i != n - 1 || loop != 0)) {
        float f2;
        bool cross;
        if (dir == 1) {
            f2 = *frac;
            cross = seg_len(i) < f2 + step;
        } else {
            f2 = *frac;
            cross = f2 < step;
        }
        if (!cross) {
            *frac = dir == 1 ? f2 + step : f2 - step;
            step = 0.0f;
        } else {
            int32_t ni = i + dir;
            const int32_t cnt = lw(path);
            if (!(ni < cnt) || ni < 0) ni = ee_rem(ni + cnt, cnt);
            if (dir == 1) {
                const float rest = seg_len(*idx) - f2;
                *frac = 0.0f;
                step = step - rest;
                if (step < *frac) step = *frac;
            } else {
                step = step - f2;
                *frac = seg_len(ni);
            }
            if (dir == 1 ? *idx == lw(path) - 1 : *idx == 0) ended = 1;
            *idx = ni;
        }
        n = lw(path);
        i = *idx;
    }
    out = V(path + static_cast<uint32_t>(i << 4) + 0x10);
    if (i == n - 1 && loop == 0) {
        if (dir != -1) return 1;
        *idx = 0;
        out = V(path + 0x10);
        *frac = 0.0f;
        return 1;
    }
    int32_t next = i + 1;
    if (!(next < n)) next = ee_rem(next, n);
    Vec4 d;
    vec3_sub(d, V(path + static_cast<uint32_t>(next << 4) + 0x10), V(path + static_cast<uint32_t>(i << 4) + 0x10));
    vec3_set_length(d, d, *frac);
    vec3_add(out, out, d);
    return ended;
}
// orig: 0x0025d808 (EE-address form)
int32_t path_advance(uint32_t path, uint32_t out, int32_t* idx, float* frac, int32_t loop, float step) { return path_advance_v(path, V(out), idx, frac, loop, step); }

// orig: 0x0025dcd8 — closest point to p on the segment a-b into out, returning its distance:
// in the xy plane (sub 0x25db00, z interpolated) when h > 0 and p is within h of both ends in
// z, else in 3D (clamped to the nearer end when the projection falls outside).
float closest_point_on_segment_r(Vec4& out, const Vec4& p, const Vec4& A, const Vec4& B, float h) {
    if (0.0f < h && abs_f(p.z - A.z) < h && abs_f(p.z - B.z) < h) {
        const float r = closest_point_on_segment_xy_r(out, p, A, B);
        const float da = vec2_distance(A, out);
        const float t = fpu_div(da, vec2_distance(A, B));
        const Vec4 va = A, vb = B;
        vec4_lerp_v(out, va, vb, t);
        return r;
    }
    Vec4 d;
    vec3_sub(d, B, A);
    const Vec4 l = A;
    vec3_set_length(d, d, 1.0f);
    const Vec4 d2 = d;
    const float dp = -vec3_dot(p, d);
    const float da = vec3_dot(l, d2);
    const float dd = vec3_dot(d, d2);
    vec3_scale(out, d, fpu_div(-(dp + da), dd));
    vec3_add(out, out, l);
    bool outside = false;
    for (uint32_t k = 0; k < 3; ++k) {
        const float o = (&out.x)[k], av = (&A.x)[k], bv = (&B.x)[k];
        if (av < o) {
            if (bv < o) { outside = true; break; }
        } else if (o < av) {
            if (o < bv) { outside = true; break; }
        }
    }
    if (outside) {
        const float ea = vec3_distance(out, A);
        const float eb = vec3_distance(out, B);
        out = (ea < eb ? A : B);
    }
    const Vec4 pp = p, oo = out;
    return sub_0025da70_v(pp, oo, 0.0f);
}
// orig: 0x0025dcd8 (EE-address form)
float closest_point_on_segment_v(Vec4& out, const Vec4& p, uint32_t a, uint32_t b, float h) { return closest_point_on_segment_r(out, p, V(a), V(b), h); }
// orig: 0x0025dcd8 (EE-address form)
float closest_point_on_segment(uint32_t out, uint32_t p, uint32_t a, uint32_t b, float h) { return closest_point_on_segment_v(V(out), V(p), a, b, h); }

// orig: 0x0024ec80 — keep the moby's looping sound (+0x7c, slot +0x7d) in sync: restart it
// when the requested sound changed, drop the slot when it was taken over.
void moby_update_sound(uint32_t m) {
    const uint32_t s = lbu(m + 0x7d);
    if (s != 0xff) {
        const uint32_t e = 0x0013e550 + s * 0x70u;
        if (lwu(e + 0x88) != m) {
            sb(m + 0x7d, 0xff);
            return;
        }
        if (lh(e + 0x7e) == static_cast<int32_t>(lbu(m + 0x7c))) return;
        sub_0028d918(static_cast<int32_t>(s));
        sb(m + 0x7d, 0xff);
        return;
    }
    const uint32_t want = lbu(m + 0x7c);
    if (want == s) return;
    sb(m + 0x7d, static_cast<uint32_t>(moby_sound_start(static_cast<int32_t>(want), 4, m)));
}

namespace {
// psubsb lane: signed saturating byte subtract
static int32_t subsb(uint32_t a, uint32_t b) {
    int32_t d = static_cast<int8_t>(a & 0xff) - static_cast<int8_t>(b & 0xff);
    if (d > 127) d = 127;
    if (d < -128) d = -128;
    return d;
}
// true when cell x,y lies outside the packed rect: bytes x0 y0 x1 y1, a negative rect is empty
static bool grid_outside(uint32_t x, uint32_t y, uint32_t rect) {
    if (subsb(x, rect) < 0 || subsb(y, rect >> 8) < 0) return true;
    return subsb(x, rect >> 16) > 0 || subsb(y, rect >> 24) > 0;
}
constexpr OvConst<0x0019b7e0u> GRID_CELLS{};  // 64x64 cells: u16 first block, u8 count, u8 blocks
// 32-byte blocks of 16 u16 moby ids: the cells + 0x4000 (Veldin 0x19f7e0; the original builds it
// as `addi t5, t6, 0x4000` from the cells' address, so it moves with them on every level)
struct GridPool {
    operator uint32_t() const { return GRID_CELLS + 0x4000u; }
};
constexpr GridPool GRID_POOL{};
static void grid_copy_blocks(uint32_t dst, uint32_t src, int32_t n) {
    do {
        lq_sq(dst, src);
        lq_sq(dst + 0x10, src + 0x10);
        dst += 0x20;
        src += 0x20;
        --n;
    } while (n > 0);
}
}  // namespace

// orig: 0x00250b20 — move a moby (id +0xac) in the 64x64 spatial grid from its cell rect
// +0xa0 to `rect` (packed bytes x0, y0, x1, y1; negative = none): remove its id from the cells
// it leaves (shrinking or freeing their block runs), store the new rect, and add it to the cells
// it enters (growing the runs, allocated from the bitmap 0x1ab7e0). A missing id hits the
// original's `teq` trap; the port stops removing from that cell instead.
void moby_grid_move64(uint32_t moby, uint64_t rect64) {
    const uint32_t rect = static_cast<uint32_t>(rect64);
    const uint32_t id = lw(moby + 0xac);
    if (static_cast<int32_t>(lw(moby + 0xa0)) >= 0) {
        uint32_t x = lw(moby + 0xa0) & 0xff;
        uint32_t y = lbu(moby + 0xa1);
        for (;;) {
            if (grid_outside(x, y, rect)) {
                const uint32_t cell = GRID_CELLS + x * 4 + (y << 8);
                const uint32_t first = GRID_POOL + (lhu(cell) << 5);
                const uint32_t end = first + lbu(cell + 2) * 2;
                uint32_t p = first;
                while (p != end && lhu(p) != id) p += 2;
                if (p != end) {
                    sh(p, lhu(end - 2));
                    sh(end - 2, 0);
                    const uint32_t n = lbu(cell + 2) - 1;
                    const uint32_t blocks = lbu(cell + 3);
                    sb(cell + 2, n);
                    bool shrink = n == 0;
                    if (!shrink && blocks != 1) shrink = static_cast<int32_t>(blocks * 8 - 4 - n) > 0;
                    if (shrink) {
                        sub_00250a40(lhu(cell), static_cast<int32_t>(lbu(cell + 3)));
                        const uint32_t half = lbu(cell + 3) >> 1;
                        const int32_t old = lh(cell);
                        sh(cell, 0);
                        sb(cell + 3, half);
                        if (half != 0) {
                            const int32_t nb = sub_00250aa0(static_cast<int32_t>(half));
                            sh(cell, static_cast<uint32_t>(nb));
                            grid_copy_blocks(GRID_POOL + (static_cast<uint32_t>(nb) << 5), GRID_POOL + (static_cast<uint32_t>(old) << 5),
                                             static_cast<int8_t>(lbu(cell + 3)));
                        }
                    }
                }
            }
            if (x != lbu(moby + 0xa2)) { ++x; continue; }
            x = lbu(moby + 0xa0);
            if (y != lbu(moby + 0xa3)) { ++y; continue; }
            break;
        }
    }
    const uint32_t old_rect = lw(moby + 0xa0);
    sw(moby + 0xa0, rect);
    if (static_cast<int64_t>(rect64) < 0) return;  // bltz on the full 64-bit register
    uint32_t x = rect & 0xff;
    uint32_t y = (rect >> 8) & 0xff;
    for (;;) {
        if (grid_outside(x, y, old_rect)) {
            const uint32_t cell = GRID_CELLS + x * 4 + (y << 8);
            const uint32_t blocks = lbu(cell + 3);
            const uint32_t n = lbu(cell + 2);
            if (blocks == 0 || static_cast<int32_t>(blocks * 16 - n) <= 0) {
                uint32_t nblocks = blocks * 2;
                if (static_cast<int32_t>(nblocks) < 1) nblocks = 1;
                sb(cell + 3, nblocks);
                const int32_t nb = sub_00250aa0(static_cast<int32_t>(nblocks));
                const uint32_t old = lhu(cell);
                sh(cell, static_cast<uint32_t>(nb));
                if (blocks != 0) {
                    sub_00250a40(old, static_cast<int32_t>(blocks));
                    grid_copy_blocks(GRID_POOL + (lhu(cell) << 5), GRID_POOL + (old << 5), static_cast<int32_t>(lbu(cell + 3) >> 1));
                }
            }
            const uint32_t cnt = lbu(cell + 2);
            sb(cell + 2, cnt + 1);
            sh(GRID_POOL + cnt * 2 + (lhu(cell) << 5), id);
        }
        if (x != lbu(moby + 0xa2)) { ++x; continue; }
        x = lbu(moby + 0xa0);
        if (y != lbu(moby + 0xa3)) { ++y; continue; }
        break;
    }
}
// orig: 0x00250b20 (32-bit rect, sign-extended as by `lui`/`lw`)
void moby_grid_move(uint32_t moby, uint32_t rect) { moby_grid_move64(moby, static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(rect)))); }

// orig: 0x0024eb68 — take a moby out of play: state +0x20 = 0xfd (0xfe for mobies past the
// level's static range 0x15ffdc), +0x38 = frame counter 0x15f5cc + 2, and leave the grid.
void moby_retire(uint32_t moby) {
    sb(moby + 0x20, moby < static_cast<uint32_t>(lw(OV(0x0015ffdc))) ? 0xfdu : 0xfeu);
    sd(moby + 0x38, static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(lw(OV(0x0015f5cc)) + 2))));
    moby_grid_move(moby, 0x80807f7fu);
}

namespace {
// CVT.W.S: truncate, saturating
static int32_t cvt_w_s(float f) {
    if (f >= 2147483648.0f) return INT32_MAX;
    if (f <= -2147483648.0f) return INT32_MIN;
    return static_cast<int32_t>(f);
}
static int64_t fbits(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return static_cast<int32_t>(u);
}
// tail of moby_anim_advance (0x250720): store the event flags and refresh the moby's sound
// unless its sound word +0x7c is 0xffff (or the moby is not in this frame's update quarter).
static void anim_advance_tail(uint32_t m, uint32_t flags, uint32_t snd) {
    sb(m + 0x70, flags);
    if ((snd << 16) == 0xffff0000u) return;
    if (((snd << 16) >> 24) != 0xff || ((m >> 8) & 3) == (static_cast<uint32_t>(lw(OV(0x00166c00u - 0x7634))) & 3)) moby_update_sound(m);
}
}  // namespace

// orig: 0x00250480 — advance a moby's animation (hand-written asm): position +0x54 += speed
// +0x58 * rate +0x5c (just the rate while blending to anim +0x53); whole frames step +0x50/+0x51
// through the class anim table (flags +0x70: 1 new frame, 2 looped), rescaled per frame; then
// fire sound events in the frames just played, else refresh the moby sound. The caller's f0
// (`sub.S f0,f0,f0`) is taken as 0.
void moby_anim_advance(uint32_t m) {
    const uint32_t speed = lw(m + 0x58);
    const uint32_t rate = lw(m + 0x5c);
    const uint32_t cls = lw(m + 0x24);
    uint32_t cur = lbu(m + 0x52);
    const uint32_t next = lbu(m + 0x53);
    uint32_t flags = 0;
    uint32_t snd = static_cast<uint32_t>(lw(m + 0x7c)) & 0xffffffu;
    float f1 = lwc1(m + 0x54);
    if (rate == 0) { anim_advance_tail(m, flags, snd); return; }
    float f3 = u2f(rate);
    if (cur != next) {
        f1 = f1 + f3;
    } else {
        const float acc = 0.0f + f1;
        const float p = u2f(speed) * f3;
        f1 = acc + p;
    }
    int32_t t0 = lbu(m + 0x50);
    int32_t t1 = lbu(m + 0x51);
    if (speed == 0) { anim_advance_tail(m, flags, snd); return; }
    const float old_pos = lwc1(m + 0x54);
    const int32_t old_frame = t0;
    const int64_t v0 = fbits(f1);
    const int64_t hi = 0x3f808000LL - v0;
    const int64_t lo = v0 - 0x3f7f0000LL;
    const uint32_t anim = static_cast<uint32_t>(lw(cls + next * 4 + 0x48));
    int mode;  // 0 = in frame, 1 = forward, 2 = backward
    if ((hi | lo) > 0) {
        f1 = 1.0f;
        mode = 1;
    } else if (v0 - 0x3f800000LL > 0) {
        mode = 1;
    } else if (v0 < 0) {
        mode = 2;
    } else {
        mode = 0;
    }
    if (mode == 0) {
        swc1(m + 0x54, f1);
    } else if (mode == 1) {
        do {
            f1 = f1 - 1.0f;
            t0 = t1;
            t1 = t1 + 1;
            const uint32_t a1 = lw(m + 0x6c);
            if (cur != next) {
                sb(m + 0x7e, lbu(anim + 0x12));
                cur = next;
                snd = 0xffff;
            }
            f1 = fpu_div(f1, f3);
            flags |= 1;
            const int32_t nfr = lbu(anim + 0x10);
            const uint32_t a0 = lw(anim + 0x18);
            uint32_t a2 = lw(anim + 0x1c + static_cast<uint32_t>(t1) * 4);
            f3 = u2f(a0);
            if (nfr - t1 <= 0) {
                t1 = 0;
                flags |= 2;
                a2 = lw(anim + 0x1c);
            }
            if (a0 == 0) f3 = lwc1(a1);
            f1 = f1 * f3;
            sb(m + 0x52, cur);
            sb(m + 0x50, static_cast<uint32_t>(t0));
            sb(m + 0x51, static_cast<uint32_t>(t1));
            sw(m + 0x68, a1);
            sw(m + 0x6c, a2);
            swc1(m + 0x5c, f3);
            swc1(m + 0x54, f1);
        } while (fbits(f1) - 0x3f800000LL > 0);
    } else {
        do {
            f1 = fpu_div(f1, f3);
            t1 = t0;
            t0 = t0 - 1;
            const uint32_t a2 = lw(m + 0x68);
            flags |= 1;
            const uint32_t a0 = lw(anim + 0x18);
            if (t0 < 0) {
                t0 = lbu(anim + 0x10) - 1;
                flags |= 2;
            }
            const uint32_t a1 = lw(anim + 0x1c + static_cast<uint32_t>(t0) * 4);
            f3 = u2f(a0);
            if (a0 == 0) f3 = lwc1(a1);
            f1 = f1 * f3;
            sw(m + 0x68, a1);
            sb(m + 0x50, static_cast<uint32_t>(t0));
            sb(m + 0x51, static_cast<uint32_t>(t1));
            f1 = f1 + 1.0f;
            sw(m + 0x6c, a2);
            swc1(m + 0x5c, f3);
            swc1(m + 0x54, f1);
        } while (fbits(f1) < 0);
    }
    sb(m + 0x70, flags);
    if (snd == 0xffff) return;
    // sound events of the anim: count snd>>16, (u16 index, u16 time in 1/16 frames) after the frame table
    if (cur != next) { anim_advance_tail(m, flags, snd); return; }
    int32_t n = static_cast<int32_t>(snd) >> 16;
    if (n <= 0) { anim_advance_tail(m, flags, snd); return; }
    const int32_t now = t0 * 16 + cvt_w_s(f1 * 16.0f);
    int32_t before = old_frame * 16 + cvt_w_s(old_pos * 16.0f);
    uint32_t ev = anim + lbu(anim + 0x10) * 4;
    if (now - before <= 0) { anim_advance_tail(m, flags, snd); return; }
    int32_t t = lhu(ev + 0x1e);
    before += 1;
    for (;;) {
        if (n <= 0) { anim_advance_tail(m, flags, snd); return; }
        --n;
        const bool outside = ((now - t) | (t - before)) < 0;
        ev += 4;
        if (!outside) break;
        t = lhu(ev + 0x1e);
    }
    moby_sound_start(lhu(ev + 0x18), 0, m);
}

// orig: 0x00240658 — relocate animation sequence `i` of class `cls` (+0x48 slot): its +0x14
// pointer and its +0x10 frame pointers (+0x1c..), offsets from the sequence -> addresses.
void moby_seq_relocate(uint32_t cls, uint32_t i) {
    const uint32_t s = lwu(cls + i * 4 + 0x48);
    if (lw(s + 0x14) != 0) sw(s + 0x14, s + lw(s + 0x14));
    for (uint32_t k = 0; k < lbu(s + 0x10); ++k) sw(s + 0x1c + k * 4, s + lw(s + 0x1c + k * 4));
}

// orig: 0x002406b0 — relocate a freshly loaded moby class (offsets -> pointers): packet table
// +0 (16-byte entries), +0x10/14/18/28, the +0x1c list, the +0x20 colour runs (bytes remapped
// through `remap`, terminated by 0xff), the +0x48 sub-models; store `remap` as the class
// palette 0x19a980[0x197bc0[idx]], then build every packet's GIF tags (0x2400f0 with
// textures `tex`, else 0x240348).
void moby_class_relocate(uint32_t cls, uint32_t tex, uint32_t remap, uint32_t idx) {
    const uint32_t n = lbu(cls + 4) + lbu(cls + 5) + lbu(cls + 6);
    if (lw(cls + 0) != 0) {
        const uint32_t t = cls + lw(cls + 0);
        sw(cls + 0, t);
        for (uint32_t k = 0; k < n; ++k) {
            const uint32_t e = t + 16 * k;
            sw(e + 0, lw(e + 0) + cls);
            sw(e + 8, lw(e + 8) + cls);
        }
    }
    for (uint32_t off = 0x10; off <= 0x18; off += 4) {
        if (lw(cls + off) != 0) sw(cls + off, cls + lw(cls + off));
    }
    if (lw(cls + 0x1c) != 0) {
        const uint32_t l = cls + lw(cls + 0x1c);
        sw(cls + 0x1c, l);
        const int32_t cnt = lw(l);
        for (int32_t i = 0; i < cnt; ++i) {
            const uint32_t p = lw(cls + 0x1c) + static_cast<uint32_t>(i) * 4;
            sw(p + 4, lw(p + 4) + cls);
        }
    }
    if (lw(cls + 0x20) != 0) {
        uint32_t e = cls + lw(cls + 0x20);
        sw(cls + 0x20, e);
        for (;;) {
            sw(e + 0xc, lw(e + 0xc) + cls);
            if (lbu(e) != 0xff) {
                uint32_t p = e;
                do {
                    sb(p, lbu(remap + lbu(p)));
                    ++p;
                } while (lbu(p) != 0xff);
            }
            const int32_t next = lw(e + 0xc);
            e += 16;
            if (next < 0) break;
        }
    }
    if (lw(cls + 0x28) != 0) sw(cls + 0x28, cls + lw(cls + 0x28));
    for (uint32_t i = 0; i < lbu(cls + 0xc); ++i) {
        const uint32_t slot = cls + 0x48 + 4 * i;
        if (lw(slot) == 0) continue;
        const uint32_t sm = cls + lw(slot);
        sw(slot, sm);
        if (lw(sm + 0x14) != 0) sw(sm + 0x14, cls + lw(sm + 0x14));
        for (uint32_t j = 0; j < lbu(sm + 0x10); ++j) sw(sm + 0x1c + 4 * j, cls + lw(sm + 0x1c + 4 * j));
    }
    const uint32_t pal = OV(0x0019a980) + lbu(OV(0x00197bc0) + idx) * 16;
    lq_sq(pal, remap);
    uint32_t e = lw(cls + 0);
    for (uint32_t k = 0; k < n; ++k, e += 16) {
        const uint32_t w = lw(e + 4);
        const int32_t q = static_cast<int32_t>(w) >> 16;
        const uint32_t lo = w & 0xffff;
        sw(e + 4, lo);
        uint32_t s0 = lw(e + 0) + (lo - static_cast<uint32_t>(q)) * 16;
        for (int32_t s1 = 0; s1 < q; s1 += 4, s0 += 0x40) {
            int32_t ti = lw(s0 + 0x20);
            if (ti >= 0) ti = lbu(pal + static_cast<uint32_t>(ti));
            if (tex != 0) {
                sub_002400f0(s0, tex + static_cast<uint32_t>(ti) * 16, lw(s0), lw(s0 + 4), lw(s0 + 0x10), lw(s0 + 0x14), ti);
            } else {
                sub_00240348(s0, lw(s0), lw(s0 + 4), lw(s0 + 0x10), lw(s0 + 0x14), ti);
            }
        }
    }
}

// Loads the item (gadget / weapon) moby class `cls` into its slot of the level's gadget table
// 0x1afbc0 (count 0x160008; the current slot 0x16000c, kept when it already holds `cls`): its
// compressed data (0x1afc20) unpacked into one of the two 0x18000 item buffers at *(0x173e10)
// (`buf`, -1: the one not in use 0x160010), the class table 0x197300 / 0x19a280 pointed at it,
// relocated with the slot's textures 0x1afce0, its packets' texture remaps from 0x1afe60 (frame
// *(0x160004)); a held item owned in 0x13e520 gets the GS registers 0x1827f0 / 0x182800 at the
// end of its first sub-model's packets.
// orig: 0x00245770
void item_class_load(uint32_t cls, int32_t buf) {
    const int32_t cur = lw(OV(0x0016000c));
    if (!(cur < 0) && lwu(OV(0x001afbc0) + static_cast<uint32_t>(cur) * 4) == cls) return;
    const int32_t n = lw(OV(0x00160008));
    sw(OV(0x0016000c), 0);
    if (0 < n && lwu(OV(0x001afbc0)) != cls) {
        int32_t k = 1;
        while (k < n && lwu(OV(0x001afbc0) + static_cast<uint32_t>(k) * 4) != cls) ++k;
        sw(OV(0x0016000c), static_cast<uint32_t>(k));
    }
    if (buf == -1) buf = lw(OV(0x00160010)) == 0 ? 1 : 0;
    const uint32_t dst = lwu(OV(0x00173e10)) + static_cast<uint32_t>(buf) * 0x18000u;
    sw(OV(0x00160010), static_cast<uint32_t>(buf));
    FlushCache(0);
    wad_decompress(lwu(OV(0x001afc20) + lwu(OV(0x0016000c)) * 4), dst);
    FlushCache(0);
    const uint32_t ci = lbu(OV(0x00197bc0) + cls);
    const uint32_t slot = OV(0x00197300) + ci * 4;
    sw(slot, dst);
    sw(OV(0x0019a280) + ci * 4, lwu(dst + 0x2c));
    moby_class_relocate(dst, OV(0x001aebc0), OV(0x001afce0) + lwu(OV(0x0016000c)) * 16, cls);
    const uint32_t remaps = OV(0x001afe60) + lwu(OV(0x0016000c)) * 32;
    const uint32_t frame = lwu(OV(0x00160004));
    for (uint32_t j = 0; j < 0x10; ++j) {
        const int32_t t = lh(remaps + j * 2);
        if (t < 0) continue;
        sh(lwu(lwu(slot) + 0x28) + j * 32 + 0x1a, static_cast<uint32_t>(t));
        sw(lwu(lwu(slot) + 0x28) + j * 32 + 0x1c, frame);
    }
    for (uint32_t item = 0; item < 0x25; ++item) {
        if (lwu(OV(0x00179ac0) + item * 0x4c + 0x10) != cls) continue;
        if (lbu(0x0013e520 + item) == 0) return;
        const uint32_t c = lwu(slot);
        if (lbu(c + 6) == 0) return;
        const uint32_t sub = lwu(c) + lbu(c + 7) * 16u;
        const uint32_t end = lwu(sub) + (lwu(sub + 4) - 4u) * 16u;
        sd(end + 0x20, ld(OV(0x001827f0)));
        sd(end + 0x30, ld(OV(0x00182800)));
        return;
    }
}

// orig: 0x00259e58 — water/floor height under pos: the region height grid if any region covers
// it, else the level's flat water disc (height 0x1612e8, centre 0x1612e0, radius 0x1612ec, within
// 0.5 of pos.z), else pos.z; *n gets the surface normal (up when not from the grid). n may be null.
float surface_height_v(uint32_t pos, Vec4* n) {
    if (lw(OV(0x001612d8)) != 0) {
        float h = lwc1(pos + 8);
        if (region_height_sample_v(&h, n, lwc1(pos + 0), lwc1(pos + 4), lwc1(pos + 8)) != 0) return h;
    }
    bool disc = lw(OV(0x001612dc)) != 0 && abs_f(lwc1(pos + 8) - lwc1(OV(0x001612e8))) < 0.5f;
    if (disc) disc = vec2_distance(V(pos), V(OV(0x001612e0))) < lwc1(OV(0x001612ec));
    if (disc) {
        if (n != nullptr) vec_set_unit_z(*n);
        return lwc1(OV(0x001612e8));
    }
    if (n != nullptr) vec_set_unit_z(*n);
    return lwc1(pos + 8);
}
// orig: 0x00259e58 — EE-address wrapper (n = 0: no normal)
float surface_height(uint32_t pos, uint32_t n) { return surface_height_v(pos, n != 0 ? &V(n) : nullptr); }

// orig: 0x0025df68 — point of `path` closest to p (metric sub_0025da70 with h): walk the path
// in `step`s; if the best walk point is within range + step, rescan the vertices one step around
// it and refine on the segments before/after the best vertex. Writes out, *idx (segment) and
// *dist (3D); returns 0 when nothing is in range (out cleared for an empty path).
int32_t path_closest_point_v(uint32_t path, const Vec4& p, Vec4& out, int32_t* idx, float* dist, int32_t loop, float range, float step, float h) {
    int32_t cur = 0;
    float frac = 0.0f;
    Vec4 walk = V(path + 0x10);
    Vec4 pp = p;
    float best_frac = 0.0f;
    int32_t best = 0;
    float best_d = sub_0025da70_v(walk, pp, h);
    if (lw(path) == 0) {
        vec_clear(out);
        return 0;
    }
    const float reach = step + range;
    int32_t end;
    do {
        end = path_advance_v(path, walk, &cur, &frac, loop, step);
        pp = p;
        const float d = sub_0025da70_v(walk, pp, h);
        if (d < best_d) {
            best = cur;
            best_d = d;
            best_frac = frac;
        }
    } while (end == 0);
    if (reach < best_d) return 0;
    int32_t from = best;
    frac = best_frac;
    path_advance_v(path, walk, &from, &frac, loop, -step);
    int32_t to = best;
    frac = best_frac;
    path_advance_v(path, walk, &to, &frac, loop, step);
    to = step_index(path, to, 1, loop);
    cur = from;
    best_d = u2f(0x4e6e6b28u);
    for (;;) {
        const Vec4 q = p;
        const Vec4 v = V(path + 0x10 + static_cast<uint32_t>(cur) * 16);
        const float d = sub_0025da70_v(v, q, h);
        if (d < best_d) {
            best = cur;
            best_d = d;
        }
        if (cur == to) break;
        cur = ee_rem(cur + 1, lw(path));
    }
    best_d = u2f(0x4e6e6b28u);
    int32_t n = lw(path);
    if (best > 0 || loop != 0) {
        const int32_t prev = ee_rem(best + n - 1, n);
        best_d = closest_point_on_segment_v(out, p, path + 0x10 + static_cast<uint32_t>(prev) * 16, path + 0x10 + static_cast<uint32_t>(best) * 16, h);
        *idx = prev;
        const Vec4 a = V(path + 0x10 + static_cast<uint32_t>(prev) * 16);
        const Vec4 o = out;
        *dist = sub_0025da70_v(a, o, 0.0f);
        n = lw(path);
    }
    if (!(best < n - 1) && loop == 0) return 1;
    const int32_t next = ee_rem(best + 1, n);
    Vec4 c;
    const float d = closest_point_on_segment_v(c, p, path + 0x10 + static_cast<uint32_t>(best) * 16, path + 0x10 + static_cast<uint32_t>(next) * 16, h);
    if (!(d < best_d)) return 1;
    out = c;
    *idx = best;
    const Vec4 a = V(path + 0x10 + static_cast<uint32_t>(best) * 16);
    const Vec4 o = out;
    *dist = sub_0025da70_v(a, o, 0.0f);
    return 1;
}
// orig: 0x0025df68 (EE-address form)
int32_t path_closest_point(uint32_t path, uint32_t p, uint32_t out, int32_t* idx, float* dist, int32_t loop, float range, float step, float h) {
    return path_closest_point_v(path, V(p), V(out), idx, dist, loop, range, step, h);
}

namespace {
// VU0 macro-mode lane arithmetic as the interpreter models it (operands and results clamped)
static float vmul(float a, float b) { return vuc(vuc(a) * vuc(b)); }
static float vmadd(float acc, float a, float b) { return vuc(vuc(acc) + vuc(a) * vuc(b)); }
static float vmsub(float acc, float a, float b) { return vuc(vuc(acc) - vuc(a) * vuc(b)); }
static int32_t vftoi0(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    if ((u & 0x7f800000u) >= 0x4f000000u) return (u & 0x80000000u) != 0 ? INT32_MIN : INT32_MAX;
    return static_cast<int32_t>(f);
}
}  // namespace

// orig: 0x00250df8 — per-frame moby transform (hand-written asm): rotation rows +0xc0..0xe0 from
// the Euler angles +0x40 (VU0 micro 0x1a3, or kept with flag 0x100; row 1 mirrored with 0x8000),
// world position +0x00 = rows * (anim root offset * scale +0x2c) + pos +0x10 * 1024 (offset from
// the anim header, blended by +0x54 while switching anims, cached in +0xf0/+0x71), frame
// counter +0xa8; then re-file the moby in the 16 m grid when its cell rect changed.
static void moby_transform_core(uint32_t m, bool keep_rows) {
    if (static_cast<int8_t>(lbu(m + 0x20)) < 0) return;
    const uint32_t flags = lhu(m + 0x34);
    const uint32_t cls = lw(m + 0x24);
    const uint32_t cur = lbu(m + 0x52);
    const uint32_t nxt = lbu(m + 0x53);
    const float scale = lwc1(m + 0x2c);
    Vec4 r0, r1, r2;
    if (keep_rows || (flags & 0x100) != 0) {
        r0 = V(m + 0xc0);
        r1 = V(m + 0xd0);
        r2 = V(m + 0xe0);
    } else {
        Mat4 r;
        euler_to_mat3_vu(r, V(m + 0x40));
        r0 = r.r[0];
        r1 = r.r[1];
        r2 = r.r[2];
    }
    Vec4 off;
    if (cur != nxt) {
        const Vec4 a = cur == 0xff ? V(OV(0x00196d00) + lbu(m + 0x50) * 16u) : V(lw(cls + cur * 4 + 0x48));
        const Vec4 b = V(lw(cls + nxt * 4 + 0x48));
        const float t = lwc1(m + 0x54);
        for (int k = 0; k < 4; ++k) {
            const float acc = vmadd(vmul((&b.x)[k], t), (&a.x)[k], 1.0f);
            (&off.x)[k] = vmsub(acc, (&a.x)[k], t);
        }
    } else if (cur != lbu(m + 0x71)) {
        sb(m + 0x71, cur);
        off = V(lw(cls + cur * 4 + 0x48));
        V(m + 0xf0) = off;
    } else {
        off = V(m + 0xf0);
    }
    if ((flags & 0x8000) != 0) {
        r1.x = vuc(0.0f - vuc(r1.x));
        r1.y = vuc(0.0f - vuc(r1.y));
        r1.z = vuc(0.0f - vuc(r1.z));
    }
    V(m + 0xc0) = r0;
    V(m + 0xd0) = r1;
    V(m + 0xe0) = r2;
    Vec4 o;
    for (int k = 0; k < 4; ++k) (&o.x)[k] = vmul((&off.x)[k], scale);
    Vec4 p = V(m + 0x10);
    p.x = vmul(p.x, 1024.0f);
    p.y = vmul(p.y, 1024.0f);
    p.z = vmul(p.z, 1024.0f);
    const int32_t radius = vftoi0(o.w);
    Vec4 w = o;
    for (int k = 0; k < 3; ++k) {
        float acc = vmul((&r0.x)[k], o.x);
        acc = vmadd(acc, (&r1.x)[k], o.y);
        acc = vmadd(acc, (&r2.x)[k], o.z);
        (&w.x)[k] = vmadd(acc, (&p.x)[k], 1.0f);
    }
    const int32_t ix = vftoi0(w.x), iy = vftoi0(w.y);
    V(m + 0) = w;
    sh(m + 0xa8, lhu(m + 0xa8) + 1);
    if (lw(m + 0x94) == 0) return;
    auto cell = [](int32_t v) { return static_cast<uint32_t>(v >> 14) & 0xffu; };
    const int32_t x0 = static_cast<int32_t>(static_cast<uint32_t>(ix) - static_cast<uint32_t>(radius));
    const int32_t y0 = static_cast<int32_t>(static_cast<uint32_t>(iy) - static_cast<uint32_t>(radius));
    const int32_t x1 = static_cast<int32_t>(static_cast<uint32_t>(ix) + static_cast<uint32_t>(radius));
    const int32_t y1 = static_cast<int32_t>(static_cast<uint32_t>(iy) + static_cast<uint32_t>(radius));
    const uint64_t rect = cell(x0) | cell(y0) << 8 | cell(x1) << 16 | static_cast<uint64_t>(cell(y1)) << 24;
    if (rect == static_cast<uint64_t>(static_cast<int64_t>(lw(m + 0xa0)))) return;
    if ((rect & 0xc0c0u) != 0) return;
    moby_grid_move64(m, rect);
}
// orig: 0x00250df8 (entry; the work is in moby_transform_core above)
void moby_update_transform(uint32_t m) { moby_transform_core(m, false); }
// orig: 0x00250f98 — moby_update_transform keeping the rotation rows +0xc0..0xe0 as they are
void moby_update_transform_kept(uint32_t m) { moby_transform_core(m, true); }

namespace {
// "in a vehicle/turret" test shared by the map conditions (sub-states 0x11/0x12 or +0x12e4 == 1)
static bool hero_riding() {
    if (static_cast<uint32_t>(lw(g::PLAYER + 0x208c) - 0x11) < 2u) return true;
    return lbu(g::PLAYER + 0x12e4) == 1;
}
static int32_t flag_set(uint32_t a) { return lbu(a) != 0 ? 1 : 0; }
}  // namespace

// orig: 0x0024abf8 — 1 if map cell (x, y) lies right of the directed edge (ax, ay) -> (bx, by)
// (32-bit cross product < 0).
int32_t map_side(int32_t x, int32_t y, int32_t ax, int32_t ay, int32_t bx, int32_t by) {
    const uint32_t l = static_cast<uint32_t>(bx - ax) * static_cast<uint32_t>(y - ay);
    const uint32_t r = static_cast<uint32_t>(by - ay) * static_cast<uint32_t>(x - ax);
    return static_cast<int32_t>(l - r) < 0 ? 1 : 0;
}

namespace {
// The per-level map-reveal conditions (table 0x183f90, 8 per level), called by
// map_reveal 0x2484e0 with the map cell (x, y) and the hero position.
// orig: 0x00248bf0
static int32_t mc_248bf0(int32_t x, int32_t y, float, float, float) { return map_side(x, y, 0xd3, 0xdb, 0x129, 0xf9); }
// orig: 0x00248c48
static int32_t mc_248c48(int32_t, int32_t y, float, float, float z) {
    const bool sub10 = lw(g::PLAYER + 0x208c) == 0x10;
    const bool ride = hero_riding();
    if (y < 0xc8) return (39.5f <= z && z <= 42.5f && !ride) ? 1 : 0;
    return (87.0f <= z && !sub10) ? 1 : 0;
}
// orig: 0x00248d08
static int32_t mc_248d08(int32_t x, int32_t, float, float, float z) {
    const bool sub10 = lw(0x001413dc) == 0x10;
    if (x < 0x100) return flag_set(0x0013d396);
    return (58.0f <= z && z <= 86.0f && !sub10) ? 1 : 0;
}
// orig: 0x00248d88
static int32_t mc_248d88(int32_t x, int32_t, float, float, float z) {
    if (x < 0x100) return hero_riding() ? 1 : 0;
    return 95.0f <= z ? 1 : 0;
}
// orig: 0x00248e50
static int32_t mc_248e50(int32_t x, int32_t y, float fx, float fy, float z) {
    if (map_side(x, y, 0x189, 0x16f, 0x87, 0xed) != 0) {
        if (map_side(x, y, 0x75, 0x146, 0x18b, 0x1cc) == 0) return 0;
        if (map_side(x, y, 0x141, 0x130, 0xb0, 0x130) == 0) return 0;
        return u2f(0x422f999au) <= z ? 1 : 0;
    }
    if (map_side(x, y, 0x191, 0xcd, 0xd2, 0x13b) != 0) {
        Vec4 a{0.0f, 0.0f, 0.0f, 0.0f}, b{0.0f, 0.0f, 0.0f, 0.0f};
        b.x = 337.5f;
        a.x = fx;
        a.y = fy;
        b.y = 250.0f;
        if (vec2_distance(a, b) <= 7.0f) return 1;
        if (map_side(x, y, 0x142, 0x12a, 0x173, 0xfc) == 0) return 0;
        return lw(OV(0x001844b0)) != 0 ? 1 : 0;
    }
    if (!hero_riding()) return 0;
    if (map_side(x, y, 0x12b, 0xb8, 0x13a, 0xf2) == 0) return 0;
    if (map_side(x, y, 0x130, 0xed, 0x165, 0xcb) == 0) return 0;
    if (map_side(x, y, 0x161, 0xdc, 0x13b, 0xb0) == 0) return 0;
    return map_side(x, y, 0x156, 0xa8, 0x119, 0xd1) != 0 ? 1 : 0;
}
// orig: 0x00249090
static int32_t mc_249090(int32_t x, int32_t y, float, float, float z) {
    const bool ride = hero_riding();
    if (map_side(x, y, 0x93, 0x168, 0x182, 0x168) != 0 && !(y < 0x135) && lw(OV(0x001844a8)) != 0 && u2f(0x423ecccdu) <= z) return 1;
    if (!ride) return 0;
    if (map_side(x, y, 0xc5, 0x9a, 0x13c, 0xe1) == 0) return 0;
    if (map_side(x, y, 0xd6, 0xc3, 0x157, 0xc5) == 0) return 0;
    return map_side(x, y, 0x107, 0xda, 0x171, 0xa0) != 0 ? 1 : 0;
}
// orig: 0x002491c8
static int32_t mc_2491c8(int32_t x, int32_t y, float, float, float) {
    if (y < 0xe9) {
        if (map_side(x, y, 0x132, 0xa0, 0x15f, 0xd8) == 0) return 0;
        if (map_side(x, y, 0x14d, 0xd8, 0x181, 0x9a) == 0) return 0;
        if (map_side(x, y, 0x182, 0xb4, 0x137, 0x93) == 0) return 0;
        return map_side(x, y, 0x157, 0x8c, 0x130, 0xaa) != 0 ? 1 : 0;
    }
    int32_t a = map_side(x, y, 0x8f, 0x115, 0x148, 0x14b);
    int32_t b = map_side(x, y, 0xe7, 0x108, 0x127, 0x164);
    if (a == 0 && b == 0) return 0;
    a = map_side(x, y, 0xed, 0x15f, 0x154, 0x10e);
    b = map_side(x, y, 0xa2, 0x12b, 0x16f, 0x147);
    if (a == 0 && b == 0) return 0;
    a = map_side(x, y, 0x132, 0x163, 0x141, 0xcc);
    b = map_side(x, y, 0xc2, 0x108, 0x1a0, 0x12e);
    return (a != 0 || b != 0) ? 1 : 0;
}
// orig: 0x00249378
static int32_t mc_249378(int32_t x, int32_t y, float, float, float z) {
    if (y < 0xbb) {
        if (hero_riding()) return 0;
        return lw(OV(0x001844bc)) != 0 ? 1 : 0;
    }
    if (!(51.5f <= z)) return 0;
    if (!(z <= 54.0f)) return 0;
    return map_side(x, y, 0x10a, 0xe5, 0x124, 0xf9) != 0 ? 1 : 0;
}
// orig: 0x00249438
static int32_t mc_249438(int32_t x, int32_t y, float, float, float) {
    const int32_t a = map_side(x, y, 0x99, 0xed, 0x160, 0x117);
    const int32_t b = map_side(x, y, 0x10e, 0xf7, 0x13d, 0x119);
    if (lw(OV(0x001844b4)) == 0) return 0;
    if (a != 0) return 1;
    return b != 0 ? 1 : 0;
}
// orig: 0x002494c8
static int32_t mc_2494c8(int32_t, int32_t y, float, float, float z) {
    if (y < 0x9d) {
        if (z < 73.5f || 80.0f < z) return 0;
        return lw(OV(0x001844c0)) != 0 ? 1 : 0;
    }
    if (z < 51.5f || 54.0f < z) return 0;
    return lw(OV(0x001844b8)) != 0 ? 1 : 0;
}
// orig: 0x00249578
static int32_t mc_249578(int32_t x, int32_t y, float, float, float z) {
    int32_t r = hero_riding() ? 1 : 0;
    if (!(y < 0x105)) {
        if (lw(OV(0x001844ac)) == 0) return 0;
        return u2f(0x423ecccdu) <= z ? 1 : 0;
    }
    if (y < 0xc1) return r;
    if (map_side(x, y, 0xd9, 0xb8, 0x156, 0xd2) != 0) r = 0;
    return r;
}
// orig: 0x00249630
static int32_t mc_249630(int32_t x, int32_t y, float, float, float z) {
    if (hero_riding()) return 0;
    if (z < 71.5f) return 0;
    if (map_side(x, y, 0x131, 0xe2, 0xc6, 0x93) != 0) return 0;
    return map_side(x, y, 0x190, 0x89, 0xd1, 0xfb) == 0 ? 1 : 0;
}
// orig: 0x002496f8
static int32_t mc_2496f8(int32_t, int32_t y, float, float, float) {
    if (y < 0x101) return flag_set(0x0013d3bd);
    return lw(0x001413dc) == 0xf ? 1 : 0;
}
// orig: 0x00249728
static int32_t mc_249728(int32_t x, int32_t, float, float, float z) { return x < 0xe0 ? (47.75f <= z ? 1 : 0) : (z < 29.0f ? 1 : 0); }
// orig: 0x00249780
static int32_t mc_249780(int32_t x, int32_t, float, float, float z) { return x < 0xe0 ? ((44.0f <= z && z <= 45.0f) ? 1 : 0) : (37.0f <= z ? 1 : 0); }
// orig: 0x002497f8
static int32_t mc_2497f8(int32_t x, int32_t, float, float, float z) { return x < 0xe0 ? ((42.0f <= z && z <= 43.0f) ? 1 : 0) : (39.0f <= z ? 1 : 0); }
// orig: 0x00249870
static int32_t mc_249870(int32_t x, int32_t, float, float, float z) { return (x < 0xe0 && z <= 38.0f) ? 1 : 0; }
// orig: 0x00249908
static int32_t mc_249908(int32_t x, int32_t, float, float, float z) { return !(x < 0xbe) ? flag_set(0x0013d3d8) : (58.5f <= z ? 1 : 0); }
// orig: 0x00249998
static int32_t mc_249998(int32_t x, int32_t, float, float, float z) {
    if (x < 0xdd) return (232.0f <= z && z <= 235.0f) ? flag_set(0x0013d3de) : 0;
    return z <= 180.0f ? 1 : 0;
}
// orig: 0x00249a18
static int32_t mc_249a18(int32_t x, int32_t, float, float, float z) {
    if (x < 0xdd) return 242.0f <= z ? 1 : 0;
    return z <= 180.0f ? flag_set(0x0013d3df) : 0;
}
// orig: 0x00249a78
static int32_t mc_249a78(int32_t x, int32_t, float, float, float z) {
    if (x < 0xdd) return (238.0f <= z && z <= 241.0f) ? 1 : 0;
    return z <= 180.0f ? flag_set(0x0013d3e0) : 0;
}
// orig: 0x00249af8
static int32_t mc_249af8(int32_t x, int32_t, float, float, float z) {
    const int32_t ride = hero_riding() ? 1 : 0;
    if (x < 0x15f) return 200.0f <= z ? ride : 0;
    return (233.0f <= z && z <= 235.0f) ? flag_set(0x0013d3e2) : 0;
}
// orig: 0x00249ba8
static int32_t mc_249ba8(int32_t x, int32_t, float, float, float z) {
    if (x < 0x15f) return (228.0f <= z && z <= 230.0f) ? 1 : 0;
    return (233.0f <= z && z <= 235.0f) ? flag_set(0x0013d3e3) : 0;
}
// orig: 0x00249c50
static int32_t mc_249c50(int32_t, int32_t y, float, float, float z) { return !(y < 0x141) ? 1 : (63.5f <= z ? 1 : 0); }
// orig: 0x00249c90
static int32_t mc_249c90(int32_t x, int32_t y, float, float, float) {
    if (lw(OV(0x001844a8)) != 0) return 1;
    Vec4 a{0.0f, 0.0f, 0.0f, 0.0f}, b{0.0f, 0.0f, 0.0f, 0.0f};
    a.y = static_cast<float>(y);
    a.x = static_cast<float>(x);
    b.x = 161.5f;
    b.y = 249.0f;
    return vec2_distance(a, b) <= 35.0f ? 1 : 0;
}

}  // namespace

// orig: 0x00248bf0..0x00249d70 (one-flag ones inline) — the per-level map reveal conditions (table 0x183f90),
// called as fn(cell x, cell y, pos x, pos y, pos z); dispatched by address.
int32_t map_condition(uint32_t fn, int32_t x, int32_t y, float fx, float fy, float fz) {
    switch (ovl_code_to_veldin(fn)) {  // (the level's code address as Veldin's: the ports' cases)
    case 0x00248bf0: return mc_248bf0(x, y, fx, fy, fz);
    case 0x00248c18: return flag_set(0x0013d394);
    case 0x00248c28: return flag_set(0x0013d395);
    case 0x00248c38: return flag_set(0x0013d39d);
    case 0x00248c48: return mc_248c48(x, y, fx, fy, fz);
    case 0x00248d08: return mc_248d08(x, y, fx, fy, fz);
    case 0x00248d88: return mc_248d88(x, y, fx, fy, fz);
    case 0x00248df0: return lw(OV(0x00166c00u - 0x6e04)) == 0 ? 1 : 0;
    case 0x00248e00: return flag_set(0x0013d3a4);
    case 0x00248e10: return flag_set(0x0013d3a5);
    case 0x00248e20: return flag_set(0x0013d3a6);
    case 0x00248e30: return flag_set(0x0013d3a7);
    case 0x00248e40: return flag_set(0x0013d3ad);
    case 0x00248e50: return mc_248e50(x, y, fx, fy, fz);
    case 0x00249090: return mc_249090(x, y, fx, fy, fz);
    case 0x002491c8: return mc_2491c8(x, y, fx, fy, fz);
    case 0x00249378: return mc_249378(x, y, fx, fy, fz);
    case 0x00249438: return mc_249438(x, y, fx, fy, fz);
    case 0x002494c8: return mc_2494c8(x, y, fx, fy, fz);
    case 0x00249578: return mc_249578(x, y, fx, fy, fz);
    case 0x00249630: return mc_249630(x, y, fx, fy, fz);
    case 0x002496f8: return mc_2496f8(x, y, fx, fy, fz);
    case 0x00249728: return mc_249728(x, y, fx, fy, fz);
    case 0x00249780: return mc_249780(x, y, fx, fy, fz);
    case 0x002497f8: return mc_2497f8(x, y, fx, fy, fz);
    case 0x00249870: return mc_249870(x, y, fx, fy, fz);
    case 0x002498a8: return flag_set(0x0013d3b8);
    case 0x002498b8: return flag_set(0x0013d3b9);
    case 0x002498c8: return flag_set(0x0013d3ba);
    case 0x002498d8: return flag_set(0x0013d3cb);
    case 0x002498e8: return flag_set(0x0013d3cc);
    case 0x002498f8: return flag_set(0x0013d3cd);
    case 0x00249908: return mc_249908(x, y, fx, fy, fz);
    case 0x00249948: return flag_set(0x0013d3d4);
    case 0x00249958: return flag_set(0x0013d3d5);
    case 0x00249968: return flag_set(0x0013d3d6);
    case 0x00249978: return flag_set(0x0013d3d7);
    case 0x00249988: return flag_set(0x0013d3d9);
    case 0x00249998: return mc_249998(x, y, fx, fy, fz);
    case 0x00249a18: return mc_249a18(x, y, fx, fy, fz);
    case 0x00249a78: return mc_249a78(x, y, fx, fy, fz);
    case 0x00249af8: return mc_249af8(x, y, fx, fy, fz);
    case 0x00249ba8: return mc_249ba8(x, y, fx, fy, fz);
    case 0x00249c40: return flag_set(0x0013d3e1);
    case 0x00249c50: return mc_249c50(x, y, fx, fy, fz);
    case 0x00249c80: return 1;
    case 0x00249c88: return 1;
    case 0x00249c90: return mc_249c90(x, y, fx, fy, fz);
    case 0x00249d30: return flag_set(0x0013d3fa);
    case 0x00249d40: return flag_set(0x0013d3fb);
    case 0x00249d50: return flag_set(0x0013d3fc);
    case 0x00249d60: return flag_set(0x0013d3fd);
    case 0x00249d70: return flag_set(0x0013d407);
    default: break;
    }
    std::fprintf(stderr, "map_condition: unknown condition 0x%08x\n", fn);
    std::abort();
}

// orig: 0x002484e0 — reveal the map around the hero: hero cell via sub_0024a7e8, per-region
// permission from 0x182ba0[level][region] (height band, flag bits, callbacks 0x183f90[level]),
// then clear fog bits (0x1841f0 +0xc) inside the circle mask where the cell's region is allowed.
void map_reveal(uint32_t pos) {
    if (lw(OV(0x00184218)) == 0) return;
    float mx = 0.0f, my = 0.0f;
    if (lw(OV(0x00166c00u - 0x6e08)) != 0) {
        sub_0024a7e8(&mx, &my, lw(OV(0x00166c00u - 0x7e7c)) + 0x64, lwc1(pos + 0), lwc1(pos + 4));
    } else {
        sub_0024a7e8(&mx, &my, lw(OV(0x00166c00u - 0x7e7c)), lwc1(pos + 0), lwc1(pos + 4));
    }
    auto cvtw = [](float f) {
        if (f >= 2147483648.0f) return INT32_MAX;
        if (f <= -2147483648.0f) return INT32_MIN;
        return static_cast<int32_t>(f);
    };
    const int32_t cx = cvtw(mx * 512.0f) + 1;
    const int32_t cy = cvtw(my * 512.0f) + 1;
    if (cx < 0 || cy < 0 || !(cx < 0x200) || !(cy < 0x200)) return;
    int32_t level = lw(0x0015ed84);
    if (!(-1 < level)) level = 0;
    if (!(level < 0x13)) level = 0;
    const bool ride = hero_riding();
    const int32_t sub = lw(g::PLAYER + 0x208c);
    const bool sub_f = sub == 0xf, sub_10 = sub == 0x10;
    int32_t allowed[16];
    for (uint32_t r = 0; r < 16; ++r) {
        allowed[r] = 1;
        if (r == 0) continue;
        const uint32_t R = OV(0x00182ba0) + static_cast<uint32_t>(level) * 256 + r * 16;
        const float lo = lwc1(R + 0), hi = lwc1(R + 4);
        if (!(lo == hi)) {
            const float hz = lwc1(lw(g::PLAYER + 0x2080) + 0x18);
            if (hz < lo || hi < hz) {
                allowed[r] = 0;
                continue;
            }
        }
        const uint32_t f = lw(R + 8);
        const int32_t idx = lw(R + 0xc);
        if (f == 0) continue;
        bool ok = true;
        if ((f & 1) != 0 && !ride) ok = false;
        else if ((f & 2) != 0 && ride) ok = false;
        else if ((f & 4) != 0 && !sub_10) ok = false;
        else if ((f & 8) != 0 && sub_10) ok = false;
        else if ((f & 0x10) != 0 && !sub_f) ok = false;
        else if ((f & 0x20) != 0 && sub_f) ok = false;
        else if ((f & 0x40) != 0 && lh(g::PLAYER + 0x308) == 0) ok = false;
        else if ((f & 0x80) != 0 && lw(OV(0x001844a8) + static_cast<uint32_t>(idx) * 4) == 0) ok = false;
        for (uint32_t b = 0; ok && b < 8; ++b) {
            if ((f & (0x100u << b)) == 0) continue;
            const uint32_t fn = lw(OV(0x00183f90) + static_cast<uint32_t>(lw(OV(0x00166c00u - 0x7e7c))) * 32 + b * 4);
            if (fn == 0) continue;
            if (map_condition(fn, cx, cy, lwc1(pos + 0), lwc1(pos + 4), lwc1(pos + 8)) == 0) ok = false;
        }
        if (!ok) allowed[r] = 0;
    }
    int32_t y0 = cy - 16, y1 = cy + 16, x0 = cx - 16, x1 = cx + 16;
    if (!(-1 < y0)) y0 = 0;
    if (!(y1 < 0x200)) y1 = 0x1ff;
    if (!(-1 < x0)) x0 = 0;
    if (!(x1 < 0x200)) x1 = 0x1ff;
    constexpr OvConst<0x001841f0u> MAP{};
    for (int32_t y = y0; y < y1; ++y) {
        const uint32_t tile_row = static_cast<uint32_t>(y >> 5) << 4;
        const uint32_t pix_row = static_cast<uint32_t>(y & 0x1f) << 4;
        const int32_t dy = y - cy + 16;
        for (int32_t x = x0; x < x1; ++x) {
            const uint32_t fog = static_cast<uint32_t>(lw(MAP + 0xc)) + static_cast<uint32_t>(y << 6) + static_cast<uint32_t>(x >> 3);
            if (lbu(fog) == 0) continue;
            const uint32_t bit = 1u << (x & 7);
            if ((lbu(fog) & bit) == 0) continue;
            const int32_t dx = x - cx + 16;
            const int32_t byte = dx >> 3;
            if (((lbu(MAP + 0x1a4 + static_cast<uint32_t>(byte + dy * 4)) >> (dx - (byte << 3))) & 1) != 0) continue;
            sprite_cache_get(static_cast<uint32_t>(x >> 5) + tile_row);
            const uint32_t pix = lbu(static_cast<uint32_t>(lw(MAP + 0x38)) + static_cast<uint32_t>((x & 0x1f) >> 1) + pix_row);
            const uint32_t region = (x & 1) != 0 ? (pix >> 4) : (pix & 0xf);
            if (allowed[region] != 0) sb(fog, lbu(fog) & ~bit);
        }
    }
}


namespace {
struct Q4 {
    float v[4];
};
static Q4 lq4(uint32_t a) { return Q4{{lwc1(a), lwc1(a + 4), lwc1(a + 8), lwc1(a + 12)}}; }
static void sq4(uint32_t a, const Q4& q) { for (uint32_t i = 0; i < 4; ++i) swc1(a + i * 4, q.v[i]); }
static float vadd(float a, float b) { return vuc(vuc(a) + vuc(b)); }
static float vsub(float a, float b) { return vuc(vuc(a) - vuc(b)); }
// VRSQRT Q = 1 / sqrt(|t|) with the VU zero-divisor rule (s = +1)
static float vrsqrt1(float t) {
    const float b = vuc(t);
    if (b == 0.0f) {
        uint32_t u;
        std::memcpy(&u, &t, 4);
        return u2f((u & 0x80000000u) != 0 ? 0xff7fffffu : 0x7f7fffffu);
    }
    return vuc(1.0f / std::sqrt(std::fabs(b)));
}
// a joint rotation record (8 bytes): three u16 * 8 as 1.15 fixed point, the joint byte at +6
static Q4 rot_rec(uint32_t a) {
    Q4 q{{0.0f, 0.0f, 0.0f, 0.0f}};
    for (uint32_t i = 0; i < 3; ++i) q.v[i] = static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(lhu(a + i * 2)) << 3)) * (1.0f / 32768.0f);
    return q;
}
// a joint translation record: three s16 as integers
static Q4 pos_rec(uint32_t a) {
    Q4 q{{0.0f, 0.0f, 0.0f, 1.0f}};
    for (uint32_t i = 0; i < 3; ++i) q.v[i] = static_cast<float>(lh(a + i * 2));
    return q;
}
// a key quaternion: four s16 as 1.15 fixed point
static Q4 key_q(uint32_t a) {
    Q4 q{};
    for (uint32_t i = 0; i < 4; ++i) q.v[i] = static_cast<float>(lh(a + i * 2)) * (1.0f / 32768.0f);
    return q;
}
static void sq3w(uint32_t a, const Q4& q, float w) { swc1(a, q.v[0]); swc1(a + 4, q.v[1]); swc1(a + 8, q.v[2]); swc1(a + 12, w); }
// blend the joints listed from list to end: (1 - t) * the quadword at +off + t * the one at
// +off + 0x10, xyz into +dst (its w kept); the list end gets a dummy entry
static void pose_blend_list(float t, float tw, uint32_t list, uint32_t end, uint32_t off, uint32_t dst, uint32_t dummy = 0x70001e00u) {
    uint32_t at = list, v0 = lwu(at);
    sw(end, dummy);
    if (at == end) return;
    Q4 x = lq4(v0 + off), y = lq4(v0 + off + 0x10);
    do {
        Q4 o{};
        for (int i = 0; i < 3; ++i) o.v[i] = vmadd(vmul(x.v[i], tw), y.v[i], t);
        const uint32_t w = lwu(v0 + dst + 0xc), cur = v0;
        v0 = lwu(at + 4);
        at += 4;
        x = lq4(v0 + off);
        y = lq4(v0 + off + 0x10);
        sq3w(cur + dst, o, 0.0f);
        sw(cur + dst + 0xc, w);
    } while (at != end);
}
}  // namespace


// orig: 0x00252990 — build the moby's joint pose on the scratchpad (DMA in, hand-written VU0
// macro code) and DMA it to +0x10 as n x 3 quadwords: key quaternion (frames +0x38 / +0x3c, blended
// by +0x24, normalised when +0x20..0x23 say the frames differ), joint rotation and translation
// records over the bind pose of class +0x14 (+0x18, +8 quadwords). The DMA waits are not modelled.
void moby_pose_blend(uint32_t m) {
    constexpr uint32_t S = 0x70000000u;
    const uint32_t cls = lwu(m + 0x14);
    const uint32_t fa = lwu(m + 0x38) + 0x10, fb = lwu(m + 0x3c) + 0x10;
    const int32_t n = lhu(m + 2);
    const uint32_t idx = lwu(m + 0xc);
    const uint32_t bind_qwc = lbu(cls + 8);
    const Q4 kZero = {};
    auto rec_of = [](uint32_t r) { return 0x70000000u + static_cast<uint32_t>(lbu(0x70002800u + lbu(r + 6))) * 64; };
    sd(S + bind_qwc * 16 + 0x1ff8, 0);
    spr_dma_to(0x2000, lwu(cls + 0x18), bind_qwc);
    for (uint32_t o = 0; o < 0x80; o += 8) sd(S + 0x2800 + o, 0x7f7f7f7f7f7f7f7full);
    uint32_t ma = idx, mk = 0;
    do { sb(S + 0x2800 + lbu(ma), mk); ++ma; ++mk; } while (ma != idx + static_cast<uint32_t>(n));
    const int32_t na = lh(fa - 0xa), na_rot = lh(fa - 6), na_pos = lh(fa - 2);
    sq4(S + static_cast<uint32_t>(na) * 16 + 0x2ff0, kZero);
    const Q4 one{{1.0f, 1.0f, 1.0f, 0.0f}};
    uint32_t ia = S - 0x80;
    int32_t ic = n;
    do {
        sq4(ia + 0x80, one); sq4(ia + 0x90, one);
        ia += 0x80;
        sq4(ia + 0x40, one); sq4(ia + 0x50, one);
        ic -= 2;
    } while (ic > 0);
    sq4(S + 0x1fc0, one);
    sq4(S + 0x1fd0, one);
    spr_dma_to(0x3000, fa, static_cast<uint32_t>(na));
    uint32_t br = S - 0x40, ba = idx;
    do {
        br += 0x40;
        lq_sq(br + 0x20, S + 0x2000 + static_cast<uint32_t>(lbu(ba)) * 16);
        lq_sq(br + 0x30, S + 0x2000 + static_cast<uint32_t>(lbu(ba)) * 16);
        sw(br + 0x3c, 0);
        ++ba;
    } while (ba != idx + static_cast<uint32_t>(n));
    sq4(S + 0x1fe0, kZero);
    sq4(S + 0x1ff0, kZero);
    if (lwu(m + 0x24) == 0) {
        // a single key frame
        uint32_t a = S + 0x3000 + bind_qwc * 8;
        if (na_rot != 0) {
            int32_t c = na_rot;
            do {
                const uint32_t r = rec_of(a);
                sq3w(r + 0x10, rot_rec(a), 0.0f);
                sw(r + 0x1c, static_cast<uint32_t>(ld(a) >> 62) - 1);
                a += 8;
            } while (--c > 0);
        }
        if (na_pos != 0) {
            int32_t c = na_pos;
            do {
                sq4(rec_of(a) + 0x20, pos_rec(a));
                a += 8;
            } while (--c > 0);
        }
        uint32_t r = S - 0x40, a2 = idx;
        do {
            const uint32_t b = lbu(a2);
            r += 0x40;
            sw(r + 0x2c, S + b * 64);
            sq4(r, key_q(S + 0x3000 + b * 8));
            ++a2;
        } while (a2 != idx + static_cast<uint32_t>(n));
    } else {
        const float t = lwc1(m + 0x24), tw = vsub(1.0f, t);
        const int32_t nb = lh(fb - 0xa), nb_rot = lh(fb - 6), nb_pos = lh(fb - 2);
        sq4(S + static_cast<uint32_t>(nb) * 16 + 0x35f0, kZero);
        spr_dma_to(0x3600, fb, static_cast<uint32_t>(nb));
        uint32_t la = S + 0x3c00, lb = S + 0x3d00;
        uint32_t a = S + 0x3000 + bind_qwc * 8;
        if (na_rot != 0) {
            int32_t c = na_rot;
            do {
                const uint32_t r = rec_of(a);
                sw(la, r);
                la += 4;
                sw(r + 0x1c, static_cast<uint32_t>(ld(a) >> 62) - 1);
                sq3w(r, rot_rec(a), 0.0f);
                sw(r + 0xc, 0xfffffffcu);  // joint listed
                a += 8;
            } while (--c > 0);
        }
        if (na_pos != 0) {
            int32_t c = na_pos;
            do {
                const uint32_t r = rec_of(a);
                sw(lb, r);
                lb += 4;
                const uint32_t keep = lwu(r + 0x2c);
                sq4(r + 0x20, pos_rec(a));
                sw(r + 0x2c, keep);
                sw(r + 0x3c, 0xfffffffcu);
                a += 8;
            } while (--c > 0);
        }
        a = S + 0x3600 + bind_qwc * 8;
        if (nb_rot != 0) {
            int32_t c = nb_rot;
            do {
                const uint32_t r = rec_of(a);
                sw(la, r);
                const uint32_t step = lwu(r + 0xc);
                sq3w(r + 0x10, rot_rec(a), 0.0f);
                sw(r + 0x1c, static_cast<uint32_t>(ld(a) >> 62) - 1);
                la += 4 + step;  // a joint frame A already listed is not listed again
                a += 8;
            } while (--c != 0);
        }
        if (nb_pos != 0) {
            int32_t c = nb_pos;
            do {
                const uint32_t r = rec_of(a);
                sw(lb, r);
                const uint32_t step = lwu(r + 0x3c);
                sq4(r + 0x30, pos_rec(a));
                sw(r + 0x3c, step);
                lb += 4 + step;
                a += 8;
            } while (--c > 0);
        }
        // blend the listed joints: (1 - t) * frame A + t * frame B (xyz; w kept)
        pose_blend_list(t, tw, S + 0x3c00, la, 0, 0x10);     // rotations: A +0x00, B +0x10 -> +0x10
        pose_blend_list(t, tw, S + 0x3d00, lb, 0x20, 0x20);  // translations: A +0x20, B +0x30 -> +0x20
        const bool nlerp = ((static_cast<int32_t>(lbu(m + 0x22)) - lbu(m + 0x23)) | (static_cast<int32_t>(lbu(m + 0x20)) + 1 - lbu(m + 0x21))) != 0;
        uint32_t r = S - 0x40;
        for (uint32_t a2 = idx;; ) {
            const uint32_t b = lbu(a2);
            const Q4 ka = key_q(S + 0x3000 + b * 8), kb = key_q(S + 0x3600 + b * 8);
            Q4 o{};
            if (!nlerp) {
                for (int i = 0; i < 4; ++i) o.v[i] = vmadd(vmul(ka.v[i], tw), kb.v[i], t);
            } else {
                Q4 pa{}, pb{}, pr{};
                for (int i = 0; i < 4; ++i) { pa.v[i] = vmul(ka.v[i], tw); pb.v[i] = vmul(kb.v[i], t); o.v[i] = vadd(pa.v[i], pb.v[i]); pr.v[i] = vmul(pa.v[i], pb.v[i]); }
                const float d = vmadd(vmadd(vadd(pr.v[1], pr.v[0]), 1.0f, pr.v[2]), 1.0f, pr.v[3]);
                uint32_t du;
                std::memcpy(&du, &d, 4);
                if ((du & 0x80000000u) != 0) for (int i = 0; i < 4; ++i) o.v[i] = vsub(pa.v[i], pb.v[i]);
                Q4 sq{};
                for (int i = 0; i < 4; ++i) sq.v[i] = vmul(o.v[i], o.v[i]);
                const float q = vrsqrt1(vmadd(vmadd(vadd(sq.v[0], sq.v[1]), 1.0f, sq.v[2]), 1.0f, sq.v[3]));
                for (int i = 0; i < 4; ++i) o.v[i] = vmul(o.v[i], q);
            }
            r += 0x40;
            sw(r + 0x2c, S + b * 64);
            sq4(r, o);
            ++a2;
            if (a2 == idx + static_cast<uint32_t>(n)) break;
        }
    }
    // compact the 0x40-byte joint records to 3 quadwords and DMA them out
    uint32_t ca = S, cv = S - 0x30;
    int32_t cc = n;
    do {
        const uint64_t q[6] = {ld(ca), ld(ca + 8), ld(ca + 0x10), ld(ca + 0x18), ld(ca + 0x20), ld(ca + 0x28)};
        cv += 0x30;
        ca += 0x40;
        for (uint32_t i = 0; i < 6; ++i) sd(cv + i * 8, q[i]);
    } while (--cc > 0);
    spr_dma_from(lwu(m + 0x10), 0, static_cast<uint32_t>(n) * 3);
}


namespace {
static Q4 nlerp_q(const Q4& pa, const Q4& pb) {
    Q4 o{}, pr{}, sq{};
    for (int i = 0; i < 4; ++i) { o.v[i] = vadd(pa.v[i], pb.v[i]); pr.v[i] = vmul(pa.v[i], pb.v[i]); }
    const float d = vmadd(vmadd(vadd(pr.v[1], pr.v[0]), 1.0f, pr.v[2]), 1.0f, pr.v[3]);
    uint32_t du;
    std::memcpy(&du, &d, 4);
    if ((du & 0x80000000u) != 0) for (int i = 0; i < 4; ++i) o.v[i] = vsub(pa.v[i], pb.v[i]);
    for (int i = 0; i < 4; ++i) sq.v[i] = vmul(o.v[i], o.v[i]);
    const float q = vrsqrt1(vmadd(vmadd(vadd(sq.v[0], sq.v[1]), 1.0f, sq.v[2]), 1.0f, sq.v[3]));
    for (int i = 0; i < 4; ++i) o.v[i] = vmul(o.v[i], q);
    return o;
}
static Q4 scale4(const Q4& a, float s, int lanes) {
    Q4 o = a;
    for (int i = 0; i < lanes; ++i) o.v[i] = vmul(a.v[i], s);
    return o;
}
// rows * parent: r' = p0 * r.x + p1 * r.y + p2 * r.z (+ p3 for the translation row)
static Q4 xform_row(const Q4 p[4], const Q4& r, bool trans) {
    Q4 o{};
    for (int i = 0; i < 4; ++i) {
        float acc = vmadd(vmul(p[0].v[i], r.v[0]), p[1].v[i], r.v[1]);
        if (!trans) {
            o.v[i] = vmadd(acc, p[2].v[i], r.v[2]);
        } else {
            acc = vmadd(acc, p[2].v[i], r.v[2]);
            o.v[i] = vmadd(acc, p[3].v[i], 1.0f);
        }
    }
    return o;
}
// key quaternions of two frames (s16 1.15 at ka / kb, 8 bytes per joint) blended into the joint
// records +0, two joints per step: linear, or normalised (shorter way) when nlerp is set
static void pose_keys2(uint32_t ka, uint32_t kb, int32_t n, bool nlerp, float t, float tw) {
    uint32_t r = 0x70000000u - 0x80;
    int32_t c = n;
    do {
        r += 0x80;
        for (uint32_t h = 0; h < 2; ++h) {
            const Q4 qa = key_q(ka + h * 8), qb = key_q(kb + h * 8);
            Q4 o{};
            if (!nlerp) {
                for (int i = 0; i < 4; ++i) o.v[i] = vmadd(vmul(qa.v[i], tw), qb.v[i], t);
            } else {
                o = nlerp_q(scale4(qa, tw, 4), scale4(qb, t, 4));
            }
            sq4(r + h * 0x40, o);
        }
        ka += 0x10;
        kb += 0x10;
        c -= 2;
    } while (c > 0);
}
// key quaternions of one frame into the joint records +0, four joints per step
static void pose_keys1(uint32_t k, int32_t n) {
    uint32_t r = 0x70000000u - 0x100;
    int32_t c = n;
    do {
        r += 0x100;
        for (uint32_t h = 0; h < 4; ++h) sq4(r + h * 0x40, key_q(k + h * 8));
        k += 0x20;
        c -= 4;
    } while (c > 0);
}
static uint32_t top2(uint32_t rec) { return static_cast<uint32_t>(ld(rec) >> 62); }
// blend layers over the joint records (layer records as moby_pose_blend makes them: quaternion,
// scale, translation -> joint at +0x2c), DMA'd to the scratchpad at spr_off; limit (if nonzero)
// ends a layer at the first joint record at or past it
static void pose_layers(uint32_t layer, uint32_t spr_off, uint32_t limit, uint32_t dummy = 0x70001e00u) {
    constexpr uint32_t S = 0x70000000u;
    while (layer != 0) {
        const uint32_t cnt = lhu(layer + 2) * 3;
        spr_dma_to(spr_off, lwu(layer + 0x10), cnt);
        const uint32_t end = S + spr_off + cnt * 16;
        const float t = lwc1(layer + 8), tw = vsub(1.0f, t);
        layer = lwu(layer + 0x1c);
        sw(end + 0x2c, dummy);
        uint32_t src = S + spr_off, dst = lwu(src + 0x2c);
        Q4 l0 = lq4(src), l1 = lq4(src + 0x10), l2 = lq4(src + 0x20);
        Q4 d0 = lq4(dst), d1 = lq4(dst + 0x10), d2 = lq4(dst + 0x20);
        for (;;) {
            d0 = scale4(d0, tw, 4);
            l0 = scale4(l0, t, 4);
            d1 = scale4(d1, tw, 3);
            if (limit != 0 && static_cast<int32_t>(limit - dst) <= 0) break;
            l1 = scale4(l1, t, 3);
            d2 = scale4(d2, tw, 3);
            l2 = scale4(l2, t, 3);
            for (int i = 0; i < 3; ++i) { d1.v[i] = vadd(d1.v[i], l1.v[i]); d2.v[i] = vadd(d2.v[i], l2.v[i]); }
            const Q4 q = nlerp_q(d0, l0);
            const uint32_t w = lwu(src + 0x1c), next = lwu(src + 0x5c);
            l0 = lq4(src + 0x30); l1 = lq4(src + 0x40); l2 = lq4(src + 0x50);
            sq4(dst + 0x10, d1);
            sq4(dst + 0x20, d2);
            d0 = lq4(next); d1 = lq4(next + 0x10); d2 = lq4(next + 0x20);
            if (lwu(dst + 0x1c) == 0) sw(dst + 0x1c, w);
            sq4(dst, q);
            src += 0x30;
            dst = next;
            if (src == end) break;
        }
    }
}
// one per-joint override node o applied to the joint record at: mode 0 multiplies (quaternion
// product, scale product, translation sum), otherwise blends by the weight +0xc
static void pose_overrides_one(uint32_t cur, uint32_t at) {
    const uint32_t mode = lbu(cur + 3);
    Q4 q = lq4(at), sc = lq4(at + 0x10), tr = lq4(at + 0x20);
    const Q4 oq = lq4(cur + 0x10), os = lq4(cur + 0x20), ot = lq4(cur + 0x30);
    if (mode == 0) {
        const float w = vmul(oq.v[3], q.v[3]);
        const float dot = vmadd(vadd(vmul(oq.v[0], q.v[0]), vmul(oq.v[1], q.v[1])), 1.0f, vmul(oq.v[2], q.v[2]));
        const float cr[3] = {vmsub(vmul(q.v[1], oq.v[2]), oq.v[1], q.v[2]), vmsub(vmul(q.v[2], oq.v[0]), oq.v[2], q.v[0]),
                             vmsub(vmul(q.v[0], oq.v[1]), oq.v[0], q.v[1])};
        Q4 r = q;
        for (int i = 0; i < 3; ++i) r.v[i] = vadd(vadd(vmul(oq.v[i], q.v[3]), vmul(q.v[i], oq.v[3])), cr[i]);
        r.v[3] = vsub(w, dot);
        q = r;
        for (int i = 0; i < 3; ++i) { sc.v[i] = vmul(sc.v[i], os.v[i]); tr.v[i] = vadd(tr.v[i], ot.v[i]); }
    } else {
        const float t = lwc1(cur + 0xc), tw = vsub(1.0f, t);
        const Q4 pa = scale4(q, tw, 4), pb = scale4(oq, t, 4);
        const Q4 s1 = scale4(sc, tw, 3), s2 = scale4(os, t, 3), t1 = scale4(tr, tw, 3), t2 = scale4(ot, t, 3);
        for (int i = 0; i < 3; ++i) { sc.v[i] = vadd(s1.v[i], s2.v[i]); tr.v[i] = vadd(t1.v[i], t2.v[i]); }
        q = nlerp_q(pa, pb);
    }
    sq4(at, q);
    sq4(at + 0x10, sc);
    sq4(at + 0x20, tr);
}
// the per-joint override list from o; limit (if nonzero) skips joint records at or past it
static void pose_overrides(uint32_t o, uint32_t limit) {
    while (o != 0) {
        const uint32_t at = lwu(o + 4);
        const uint32_t cur = o;
        o = lwu(o + 8);
        if (limit != 0 && static_cast<int32_t>(limit - at) <= 0) continue;
        pose_overrides_one(cur, at);
    }
}
}  // namespace

// The moby's n joint matrices on the scratchpad (0x40 each, parent matrix at
// record +0x2c): key frames +0x68 / +0x6c blended by +0x54 over the class bind pose (+0x24),
// blend layers / overrides from the lists at +0x60, then quaternion -> matrix * parent. out > 0
// also gets the translations; the compacted translations overwrite the start of the scratchpad.
// Returns the value the hand-written code leaves in $a3 (0x1f2868 stores it in its cache ring);
// t8 / t9 are the caller's registers, which it saves on the scratchpad (0 when not meaningful).
// orig: 0x002531e0
uint32_t moby_pose_matrices(uint32_t m, int32_t n, int32_t out, uint32_t t8, uint32_t t9) {
    uint32_t a3 = 0;  // what the key loops leave in $a3 (a later key halfword; 0x1f2868 stores it)
    auto sx16 = [](uint32_t a) { return static_cast<uint32_t>(static_cast<int32_t>(lh(a))); };
    constexpr uint32_t S = 0x70000000u;
    // the caller's $t8 / $t9 are spilled here (lq/sq; their upper 96 bits taken as 0)
    sd(S + 0x3700, static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(t8))));
    sd(S + 0x3708, 0);
    sd(S + 0x3710, static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(t9))));
    sd(S + 0x3718, 0);
    const uint32_t cls = lwu(m + 0x24);
    const uint32_t fa = lwu(m + 0x68) + 0x10, fb = lwu(m + 0x6c) + 0x10;
    const uint32_t un = static_cast<uint32_t>(n);
    const Q4 kZero = {};
    sw(S + 0x3720, static_cast<uint32_t>(out));
    sd(S + un * 16 + 0x2ff8, 0);
    spr_dma_to(0x3000, lwu(cls + 0x18), un);
    const int32_t na = lh(fa - 0xa), na_rot = lh(fa - 6), na_pos = lh(fa - 2);
    const uint32_t key_bytes = static_cast<uint32_t>(lbu(cls + 8)) * 8;
    sq4(S + static_cast<uint32_t>(na) * 16 + 0x1ff0, kZero);
    sw(S + 0x3728, key_bytes);
    const Q4 one{{1.0f, 1.0f, 1.0f, 0.0f}};
    uint32_t ia = S - 0x80;
    int32_t ic = n;
    do {
        sq4(ia + 0x80, one); sq4(ia + 0x90, one);
        ia += 0x80;
        sq4(ia + 0x40, one); sq4(ia + 0x50, one);
        ic -= 2;
    } while (ic > 0);
    spr_dma_to(0x2000, fa, static_cast<uint32_t>(na));
    uint32_t bs = S + 0x3000, br = S - 0x100;
    int32_t bc = n;
    do {
        br += 0x100;
        bc -= 4;
        for (uint32_t i = 0; i < 4; ++i) {
            const Q4 b = lq4(bs + i * 0x10);
            sq4(br + i * 0x40 + 0x20, b);
            Q4 bw = scale4(b, 1.0f, 3);
            bw.v[3] = 0.0f;
            sq4(br + i * 0x40 + 0x30, bw);
        }
        bs += 0x40;
    } while (bc > 0);
    auto joint = [un](uint32_t rec) -> uint32_t {  // record's joint, or 0 when out of range
        const int32_t j = lbu(rec + 6);
        return static_cast<int32_t>(un) - j > 0 ? 0x70000000u + static_cast<uint32_t>(j) * 64 : 0u;
    };
    if (lwu(m + 0x54) != 0) {
        const float t = lwc1(m + 0x54), tw = vsub(1.0f, t);
        const int32_t mix = (static_cast<int32_t>(lbu(m + 0x52)) - lbu(m + 0x53)) | (static_cast<int32_t>(lbu(m + 0x50)) + 1 - lbu(m + 0x51));
        sw(S + 0x3724, static_cast<uint32_t>(mix));
        const int32_t nb = lh(fb - 0xa), nb_rot = lh(fb - 6), nb_pos = lh(fb - 2);
        sq4(S + static_cast<uint32_t>(nb) * 16 + 0x25f0, kZero);
        spr_dma_to(0x2600, fb, static_cast<uint32_t>(nb));
        uint32_t lr = S + 0x2c00, lp = S + 0x2d00;
        uint32_t a = S + 0x2000 + lwu(S + 0x3728);
        uint32_t ap = a + static_cast<uint32_t>(na_rot) * 8;
        for (int32_t c = na_rot; c > 0; --c, a += 8) {
            const uint32_t r = joint(a);
            if (r == 0) break;
            if (top2(a) == 0) continue;
            sw(lr, r);
            lr += 4;
            sq3w(r, rot_rec(a), 0.0f);
            sw(r + 0xc, 0xfffffffcu);  // joint listed
        }
        for (int32_t c = na_pos; c > 0; --c, ap += 8) {
            const uint32_t r = joint(ap);
            if (r == 0) break;
            sw(lp, r);
            lp += 4;
            const uint32_t keep = lwu(r + 0x2c);
            sq4(r + 0x20, pos_rec(ap));
            sw(r + 0x2c, keep);
            sw(r + 0x3c, 0xfffffffcu);
        }
        a = S + 0x2600 + lwu(S + 0x3728);
        ap = a + static_cast<uint32_t>(nb_rot) * 8;
        if (nb_rot != 0) {
            for (int32_t c = nb_rot; c > 0; a += 8) {
                const uint32_t r = joint(a);
                if (r == 0) break;
                sw(lr, r);
                const uint32_t step = lwu(r + 0xc);
                --c;
                if (top2(a) == 0) continue;
                sq3w(r + 0x10, rot_rec(a), 0.0f);
                lr += 4 + step;  // a joint frame A already listed is not listed again
                if (c == 0) break;
            }
        }
        for (int32_t c = nb_pos; c > 0; --c, ap += 8) {
            const uint32_t r = joint(ap);
            if (r == 0) break;
            sw(lp, r);
            const uint32_t step = lwu(r + 0x3c);
            sq4(r + 0x30, pos_rec(ap));
            lp += 4 + step;
        }
        pose_blend_list(t, tw, S + 0x2c00, lr, 0, 0x10);
        pose_blend_list(t, tw, S + 0x2d00, lp, 0x20, 0x20);
        const uint32_t k2 = static_cast<uint32_t>(n > 2 ? (n + 1) / 2 : 1);  // do-while, 2 joints a pass
        a3 = lwu(S + 0x3724) != 0 ? sx16(S + 0x2600 + 0x10 * k2) : sx16(S + 0x2600 + 0x10 * (k2 - 1));
        pose_keys2(S + 0x2000, S + 0x2600, n, lwu(S + 0x3724) != 0, t, tw);
    } else {
        uint32_t a = S + 0x2000 + lwu(S + 0x3728);
        uint32_t ap = a + static_cast<uint32_t>(na_rot) * 8;
        for (int32_t c = na_rot; c > 0; --c, a += 8) {
            const uint32_t r = joint(a);
            if (r == 0) break;
            if (top2(a) != 0) sq3w(r + 0x10, rot_rec(a), 0.0f);
        }
        for (int32_t c = na_pos; c > 0; --c, ap += 8) {
            const uint32_t r = joint(ap);
            if (r == 0) break;
            const uint32_t keep = lwu(r + 0x2c);
            sq4(r + 0x20, pos_rec(ap));
            sw(r + 0x2c, keep);
        }
        const uint32_t k4 = static_cast<uint32_t>(n > 4 ? (n + 3) / 4 : 1);  // do-while, 4 joints a pass
        a3 = sx16(S + 0x2000 + 0x20 * (k4 - 1) + 0x10);
        pose_keys1(S + 0x2000, n);
    }
    const uint64_t lists = ld(m + 0x60);
    if (lists != 0) {
        pose_layers(static_cast<uint32_t>(lists), 0x2000, S + un * 64);
        pose_overrides(static_cast<uint32_t>(lists >> 32), S + un * 64);
    }
    // quaternion + scale + translation -> matrix, times the parent matrix (record +0x2c)
    const uint32_t end = S + un * 64;
    sw(end + 0x2c, S);
    Q4 par[4] = {Q4{{1.0f, 0.0f, 0.0f, 0.0f}}, Q4{{0.0f, 1.0f, 0.0f, 0.0f}}, Q4{{0.0f, 0.0f, 1.0f, 0.0f}}, Q4{{0.0f, 0.0f, 0.0f, 1.0f}}};
    uint32_t j = S;
    do {
        const Q4 q = lq4(j);
        Q4 q2{};
        for (int i = 0; i < 4; ++i) q2.v[i] = vadd(q.v[i], q.v[i]);
        float a[3], b[3];
        for (int i = 0; i < 3; ++i) { a[i] = vmul(q2.v[i], q.v[3]); b[i] = vmul(q2.v[i], q.v[0]); }
        const float c1 = vmul(q2.v[1], q.v[1]), c2 = vmul(q2.v[2], q.v[1]), d2 = vmul(q2.v[2], q.v[2]);
        Q4 r0{{1.0f, 0.0f, 0.0f, 0.0f}}, r1{{0.0f, 1.0f, 0.0f, 0.0f}}, r2{{0.0f, 0.0f, 1.0f, 0.0f}};
        r1.v[0] = vadd(0.0f, a[2]);
        r2.v[0] = vsub(0.0f, a[1]);
        r2.v[1] = vadd(0.0f, a[0]);
        r0.v[0] = vsub(r0.v[0], c1);
        r1.v[1] = vsub(r1.v[1], b[0]);
        r2.v[2] = vsub(r2.v[2], b[0]);
        r0.v[1] = vsub(b[1], a[2]);
        r0.v[2] = vadd(b[2], a[1]);
        r1.v[2] = vsub(c2, a[0]);
        r1.v[0] = vadd(r1.v[0], b[1]);
        r2.v[0] = vadd(r2.v[0], b[2]);
        r2.v[1] = vadd(r2.v[1], c2);
        r0.v[0] = vsub(r0.v[0], d2);
        r1.v[1] = vsub(r1.v[1], d2);
        r2.v[2] = vsub(r2.v[2], c1);
        const Q4 sc = lq4(j + 0x10), tr = lq4(j + 0x20);
        r0 = scale4(r0, sc.v[0], 3);
        r1 = scale4(r1, sc.v[1], 3);
        r2 = scale4(r2, sc.v[2], 3);
        const uint32_t next_par = lwu(j + 0x6c);
        const Q4 r3 = Q4{{vadd(0.0f, tr.v[0]), vadd(0.0f, tr.v[1]), vadd(0.0f, tr.v[2]), 1.0f}};
        sq4(j, xform_row(par, r0, false));
        sq4(j + 0x10, xform_row(par, r1, false));
        sq4(j + 0x20, xform_row(par, r2, false));
        sq4(j + 0x30, xform_row(par, r3, true));
        for (uint32_t i = 0; i < 4; ++i) par[i] = lq4(next_par + i * 0x10);
        j += 0x40;
    } while (j != end);
    // translations: compacted to the scratchpad start, and copied to out when out > 0
    if (out < 0) return a3;
    uint32_t src = S, dst = S - 0x20, ram = static_cast<uint32_t>(out);
    int32_t c = n;
    for (;;) {
        const Q4 t0 = lq4(src + 0x30), t1 = lq4(src + 0x70);
        dst += 0x20;
        src += 0x80;
        sq4(dst, t0);
        sq4(dst + 0x10, t1);
        c -= 2;
        if (out > 0) {
            ram += 0x20;
            sq4(ram - 0x20, t0);
            if (c < 0) break;
            sq4(ram - 0x10, t1);
        }
        if (!(c > 0)) break;
    }
    return a3;
}


namespace {
// VFTOIn as the interpreter models it: f * scale, saturating
static int32_t vftoi_s(float f, float scale) {
    f *= scale;
    uint32_t u;
    std::memcpy(&u, &f, 4);
    if ((u & 0x7f800000u) >= 0x4f000000u) return (u & 0x80000000u) != 0 ? INT32_MIN : INT32_MAX;
    return static_cast<int32_t>(f);
}
static uint64_t pack_h4(const uint32_t h[4]) {
    uint64_t v = 0;
    for (int i = 3; i >= 0; --i) v = (v << 16) | (h[i] & 0xffffu);
    return v;
}
}  // namespace

// orig: 0x00251ce8 — the moby's current pose (key frames +0x68 / +0x6c blended by +0x54 over the
// class bind pose, plus the +0x60 layers / overrides unless flags 0x200 / 0x100) re-encoded as a
// key frame (header, s16 quaternions, scale records for joints not at 1.0, translation records
// for joints off the bind pose) and DMA'd to slot (flags & 0xff) of the 0x800-byte frames at 0x18ecc0.
void moby_pose_capture(uint32_t m, uint32_t flags) {
    constexpr uint32_t S = 0x70000000u;
    const uint32_t cls = lwu(m + 0x24);
    const uint32_t fa = lwu(m + 0x68) + 0x10, fb = lwu(m + 0x6c) + 0x10;
    const uint32_t n = lbu(cls + 8);
    const int32_t sn = static_cast<int32_t>(n);
    const Q4 kZero = {};
    auto rec_at = [](uint32_t a) { return 0x70000000u + static_cast<uint32_t>(lbu(a + 6)) * 64; };
    sd(S + n * 16 + 0x1ff8, 0);
    spr_dma_to(0x2000, lwu(cls + 0x18), n);
    const int32_t na = lh(fa - 0xa), na_rot = lh(fa - 6), na_pos = lh(fa - 2);
    sq4(S + static_cast<uint32_t>(na) * 16 + 0x2ff0, kZero);
    const Q4 one{{1.0f, 1.0f, 1.0f, 0.0f}};
    uint32_t ia = S - 0x80;
    int32_t ic = sn;
    do {
        sq4(ia + 0x80, one); sq4(ia + 0x90, one);
        ia += 0x80;
        sq4(ia + 0x40, one); sq4(ia + 0x50, one);
        ic -= 2;
    } while (ic > 0);
    spr_dma_to(0x3000, fa, static_cast<uint32_t>(na));
    uint32_t bs = S + 0x2000, br = S - 0x100;
    int32_t bc = sn;
    do {
        br += 0x100;
        bc -= 4;
        for (uint32_t i = 0; i < 4; ++i) {
            const Q4 b = lq4(bs + i * 0x10);
            sq4(br + i * 0x40 + 0x20, b);
            Q4 bw = scale4(b, 1.0f, 3);
            bw.v[3] = 0.0f;
            sq4(br + i * 0x40 + 0x30, bw);
        }
        bs += 0x40;
    } while (bc > 0);
    if (lwu(m + 0x54) != 0) {
        const float t = lwc1(m + 0x54), tw = vsub(1.0f, t);
        const int32_t mix = (static_cast<int32_t>(lbu(m + 0x52)) - lbu(m + 0x53)) | (static_cast<int32_t>(lbu(m + 0x50)) + 1 - lbu(m + 0x51));
        const int32_t nb = lh(fb - 0xa), nb_rot = lh(fb - 6), nb_pos = lh(fb - 2);
        sq4(S + static_cast<uint32_t>(nb) * 16 + 0x35f0, kZero);
        spr_dma_to(0x3600, fb, static_cast<uint32_t>(nb));
        uint32_t lr = S + 0x3c00, lp = S + 0x3d00;
        uint32_t a = S + 0x3000 + n * 8;
        if (na_rot != 0) {
            int32_t c = na_rot;
            do {
                const uint32_t r = rec_at(a);
                sw(lr, r);
                lr += 4;
                sw(r + 0x1c, top2(a) - 1);
                sq3w(r, rot_rec(a), 0.0f);
                sw(r + 0xc, 0xfffffffcu);  // joint listed
                a += 8;
            } while (--c > 0);
        }
        if (na_pos != 0) {
            int32_t c = na_pos;
            do {
                const uint32_t r = rec_at(a);
                sw(lp, r);
                lp += 4;
                const uint32_t keep = lwu(r + 0x2c);
                sq4(r + 0x20, pos_rec(a));
                sw(r + 0x2c, keep);
                sw(r + 0x3c, 0xfffffffcu);
                a += 8;
            } while (--c > 0);
        }
        a = S + 0x3600 + n * 8;
        if (nb_rot != 0) {
            int32_t c = nb_rot;
            do {
                const uint32_t r = rec_at(a);
                sw(lr, r);
                const uint32_t step = lwu(r + 0xc);
                sq3w(r + 0x10, rot_rec(a), 0.0f);
                sw(r + 0x1c, top2(a) - 1);
                lr += 4 + step;  // a joint frame A already listed is not listed again
                a += 8;
            } while (--c != 0);
        }
        if (nb_pos != 0) {
            int32_t c = nb_pos;
            do {
                const uint32_t r = rec_at(a);
                sw(lp, r);
                const uint32_t step = lwu(r + 0x3c);
                sq4(r + 0x30, pos_rec(a));
                lp += 4 + step;
                a += 8;
            } while (--c > 0);
        }
        pose_blend_list(t, tw, S + 0x3c00, lr, 0, 0x10);
        pose_blend_list(t, tw, S + 0x3d00, lp, 0x20, 0x20);
        pose_keys2(S + 0x3000, S + 0x3600, sn, mix != 0, t, tw);
    } else {
        uint32_t a = S + 0x3000 + n * 8;
        if (na_rot != 0) {
            int32_t c = na_rot;
            do {
                const uint32_t r = rec_at(a);
                sq3w(r + 0x10, rot_rec(a), 0.0f);
                sw(r + 0x1c, top2(a) - 1);
                a += 8;
            } while (--c > 0);
        }
        if (na_pos != 0) {
            int32_t c = na_pos;
            do {
                const uint32_t r = rec_at(a);
                const uint32_t keep = lwu(r + 0x2c);
                sq4(r + 0x20, pos_rec(a));
                sw(r + 0x2c, keep);
                a += 8;
            } while (--c > 0);
        }
        pose_keys1(S + 0x3000, sn);
    }
    const uint64_t lists = ld(m + 0x60);
    if (lists != 0) {
        if ((flags & 0x200) == 0) pose_layers(static_cast<uint32_t>(lists), 0x2800, 0);
        if ((flags & 0x100) == 0) pose_overrides(static_cast<uint32_t>(lists >> 32), 0);
    }
    // encode: quaternions at +0x10, scale records at 0x2800, translation records at 0x2c00
    uint32_t qp = S + 0x10, rp = S + 0x2800, pp = S + 0x2c00;
    uint32_t k = 0;
    do {
        const uint32_t r = S + k * 64, b = S + 0x2000 + k * 16;
        uint32_t q[4], sc[4], tr[4], bd[4];
        for (uint32_t i = 0; i < 4; ++i) {
            q[i] = static_cast<uint32_t>(std::min(std::max(vftoi_s(lwc1(r + i * 4), 32768.0f), -32768), 32767));
            const uint32_t s = static_cast<uint32_t>(vftoi_s(lwc1(r + 0x10 + i * 4), 32768.0f)) >> 3;
            sc[i] = std::min<uint32_t>(s, 0xffff);
            tr[i] = static_cast<uint32_t>(vftoi_s(lwc1(r + 0x20 + i * 4), 1.0f));
            bd[i] = static_cast<uint32_t>(vftoi_s(lwc1(b + i * 4), 1.0f));
        }
        sd(qp, pack_h4(q));
        qp += 8;
        if (!(sc[0] == 0x1000 && sc[1] == 0x1000 && sc[2] == 0x1000)) {
            const uint32_t w = lwu(r + 0x1c);
            sd(rp, pack_h4(sc));
            sb(rp + 6, k);
            sb(rp + 7, (w & 0x80) ^ 0x80);
            rp += 8;
        }
        if (((tr[0] ^ bd[0]) | (tr[1] ^ bd[1]) | (tr[2] ^ bd[2])) & 0xffffu) {
            sd(pp, pack_h4(tr));
            sh(pp + 6, k);
            pp += 8;
        }
        ++k;
    } while (k != n);
    const uint32_t rb = rp - (S + 0x2800), pb = pp - (S + 0x2c00), kb = n * 8;
    sh(S + 8, kb);
    sh(S + 0xc, kb + rb);
    sd(S, 0);
    const uint32_t qwc = (kb + rb + pb + 8) >> 4;
    sh(S + 6, qwc);
    sh(S + 0xa, rb >> 3);
    sh(S + 0xe, pb >> 3);
    for (uint32_t a = S + 0x2800; a != rp; a += 8, qp += 8) sd(qp, ld(a));
    for (uint32_t a = S + 0x2c00; a != pp; a += 8, qp += 8) sd(qp, ld(a));
    sd(qp, 1);
    spr_dma_from(OV(0x0018ecc0u) + (flags & 0xffu) * 0x800, 0, qwc + 1);
}


// orig: 0x00254b58 — the moby's local joint pose left on the scratchpad (records of 0x40 per
// joint: quaternion, scale, translation): key frames +0x68 / +0x6c blended by +0x54 over the
// class bind pose, then the +0x60 layers and +0x64 overrides. Rotation records whose top bits are
// clear are ignored.
void moby_pose_eval(uint32_t m) {
    constexpr uint32_t S = 0x70000000u, DUMMY = 0x70002000u;
    const uint32_t cls = lwu(m + 0x24);
    const uint32_t fa = lwu(m + 0x68) + 0x10, fb = lwu(m + 0x6c) + 0x10;
    const uint32_t n = lbu(cls + 8);
    const int32_t sn = static_cast<int32_t>(n);
    const Q4 kZero = {};
    auto rec_at = [](uint32_t a) { return 0x70000000u + static_cast<uint32_t>(lbu(a + 6)) * 64; };
    sd(S + n * 16 + 0x1ff8, 0);
    spr_dma_to(0x2000, lwu(cls + 0x18), n);
    const int32_t na = lh(fa - 0xa), na_rot = lh(fa - 6), na_pos = lh(fa - 2);
    sq4(S + static_cast<uint32_t>(na) * 16 + 0x2ff0, kZero);
    const Q4 one{{1.0f, 1.0f, 1.0f, 0.0f}};
    uint32_t ia = S - 0x80;
    int32_t ic = sn;
    do {
        sq4(ia + 0x80, one); sq4(ia + 0x90, one);
        ia += 0x80;
        sq4(ia + 0x40, one); sq4(ia + 0x50, one);
        ic -= 2;
    } while (ic > 0);
    spr_dma_to(0x3000, fa, static_cast<uint32_t>(na));
    uint32_t bs = S + 0x2000, br = S - 0x100;
    int32_t bc = sn;
    do {
        br += 0x100;
        bc -= 4;
        for (uint32_t i = 0; i < 4; ++i) {
            const Q4 b = lq4(bs + i * 0x10);
            sq4(br + i * 0x40 + 0x20, b);
            Q4 bw = scale4(b, 1.0f, 3);
            bw.v[3] = 0.0f;
            sq4(br + i * 0x40 + 0x30, bw);
        }
        bs += 0x40;
    } while (bc > 0);
    if (lwu(m + 0x54) != 0) {
        const float t = lwc1(m + 0x54), tw = vsub(1.0f, t);
        const int32_t mix = (static_cast<int32_t>(lbu(m + 0x52)) - lbu(m + 0x53)) | (static_cast<int32_t>(lbu(m + 0x50)) + 1 - lbu(m + 0x51));
        const int32_t nb = lh(fb - 0xa), nb_rot = lh(fb - 6), nb_pos = lh(fb - 2);
        sq4(S + static_cast<uint32_t>(nb) * 16 + 0x35f0, kZero);
        spr_dma_to(0x3600, fb, static_cast<uint32_t>(nb));
        uint32_t lr = S + 0x3c00, lp = S + 0x3d00;
        uint32_t a = S + 0x3000 + n * 8;
        for (int32_t c = na_rot; c > 0; --c, a += 8) {
            if (top2(a) == 0) continue;
            const uint32_t r = rec_at(a);
            sw(lr, r);
            lr += 4;
            sq3w(r, rot_rec(a), 0.0f);
            sw(r + 0xc, 0xfffffffcu);  // joint listed
        }
        if (na_pos != 0) {
            int32_t c = na_pos;
            do {
                const uint32_t r = rec_at(a);
                sw(lp, r);
                lp += 4;
                const uint32_t keep = lwu(r + 0x2c);
                sq4(r + 0x20, pos_rec(a));
                sw(r + 0x2c, keep);
                sw(r + 0x3c, 0xfffffffcu);
                a += 8;
            } while (--c > 0);
        }
        a = S + 0x3600 + n * 8;
        for (int32_t c = nb_rot; c > 0;) {
            const uint32_t rec = a;
            a += 8;
            const uint32_t r = rec_at(rec);
            sw(lr, r);
            const uint32_t step = lwu(r + 0xc);
            --c;
            if (top2(rec) == 0) continue;
            sq3w(r + 0x10, rot_rec(rec), 0.0f);
            lr += 4 + step;  // a joint frame A already listed is not listed again
            if (c == 0) break;
        }
        if (nb_pos != 0) {
            int32_t c = nb_pos;
            do {
                const uint32_t r = rec_at(a);
                sw(lp, r);
                const uint32_t step = lwu(r + 0x3c);
                sq4(r + 0x30, pos_rec(a));
                lp += 4 + step;
                a += 8;
            } while (--c > 0);
        }
        pose_blend_list(t, tw, S + 0x3c00, lr, 0, 0x10, DUMMY);
        pose_blend_list(t, tw, S + 0x3d00, lp, 0x20, 0x20, DUMMY);
        pose_keys2(S + 0x3000, S + 0x3600, sn, mix != 0, t, tw);
    } else {
        uint32_t a = S + 0x3000 + n * 8;
        for (int32_t c = na_rot; c > 0; --c, a += 8) {
            if (top2(a) != 0) sq3w(rec_at(a) + 0x10, rot_rec(a), 0.0f);
        }
        if (na_pos != 0) {
            int32_t c = na_pos;
            do {
                const uint32_t r = rec_at(a);
                const uint32_t keep = lwu(r + 0x2c);
                sq4(r + 0x20, pos_rec(a));
                sw(r + 0x2c, keep);
                a += 8;
            } while (--c > 0);
        }
        pose_keys1(S + 0x3000, sn);
    }
    pose_layers(lwu(m + 0x60), 0x2000, 0, DUMMY);
    pose_overrides(lwu(m + 0x64), 0);
}


namespace {
// quaternion -> rotation rows (VU0 sequence shared by the pose routines)
static void quat_rows(const Q4& q, Q4& r0, Q4& r1, Q4& r2) {
    Q4 q2{};
    for (int i = 0; i < 4; ++i) q2.v[i] = vadd(q.v[i], q.v[i]);
    float a[3], b[3];
    for (int i = 0; i < 3; ++i) { a[i] = vmul(q2.v[i], q.v[3]); b[i] = vmul(q2.v[i], q.v[0]); }
    const float c1 = vmul(q2.v[1], q.v[1]), c2 = vmul(q2.v[2], q.v[1]), d2 = vmul(q2.v[2], q.v[2]);
    r0 = Q4{{1.0f, 0.0f, 0.0f, 0.0f}};
    r1 = Q4{{0.0f, 1.0f, 0.0f, 0.0f}};
    r2 = Q4{{0.0f, 0.0f, 1.0f, 0.0f}};
    r1.v[0] = vadd(0.0f, a[2]);
    r2.v[0] = vsub(0.0f, a[1]);
    r2.v[1] = vadd(0.0f, a[0]);
    r0.v[0] = vsub(r0.v[0], c1);
    r1.v[1] = vsub(r1.v[1], b[0]);
    r2.v[2] = vsub(r2.v[2], b[0]);
    r0.v[1] = vsub(b[1], a[2]);
    r0.v[2] = vadd(b[2], a[1]);
    r1.v[2] = vsub(c2, a[0]);
    r1.v[0] = vadd(r1.v[0], b[1]);
    r2.v[0] = vadd(r2.v[0], b[2]);
    r2.v[1] = vadd(r2.v[1], c2);
    r0.v[0] = vsub(r0.v[0], d2);
    r1.v[1] = vsub(r1.v[1], d2);
    r2.v[2] = vsub(r2.v[2], c1);
}
static bool sign64(uint32_t a) { return (ld(a) >> 63) != 0; }
static float len4(const Q4& v) {  // vadday.x, vmaddaz.x, vmaddw.x of v * v
    Q4 s{};
    for (int i = 0; i < 4; ++i) s.v[i] = vmul(v.v[i], v.v[i]);
    return vmadd(vmadd(vadd(s.v[0], s.v[1]), 1.0f, s.v[2]), 1.0f, s.v[3]);
}
static float dot4y(const Q4& a, const Q4& b) {  // vaddax.y, vmaddaz.y, vmaddw.y of a * b
    Q4 p{};
    for (int i = 0; i < 4; ++i) p.v[i] = vmul(a.v[i], b.v[i]);
    return vmadd(vmadd(vadd(p.v[1], p.v[0]), 1.0f, p.v[2]), 1.0f, p.v[3]);
}
static bool fneg(float f) { uint32_t u; std::memcpy(&u, &f, 4); return (u & 0x80000000u) != 0; }
}  // namespace

// orig: 0x00254108 — the moby's joint matrices on the scratchpad for the joints flagged in
// mask[] (count mask[0x7f]): key frames +0x68 / +0x6c (one chained DMA via the tags at 0x196f00)
// blended by +0x54 over the class bind pose, the +0x60 layers and +0x64 overrides, then
// quaternion -> matrix (scaled when +0x1c is set) times the parent matrix at +0x2c (0: identity).
void moby_pose_matrices_masked(uint32_t m, uint32_t mask) {
    const uint32_t S = 0x70000000u, TAGS = OV(0x00196f00u);
    const uint32_t cls = lwu(m + 0x24);
    const uint32_t n = lbu(cls + 8);
    if (n == 0) return;
    const uint32_t fa = lwu(m + 0x68) + 0x10, fb = lwu(m + 0x6c) + 0x10;
    const uint32_t blend = lwu(m + 0x54);
    const int32_t na = lh(fa - 0xa);
    const uint32_t nb = lhu(fb - 0xa);
    sh(TAGS, n); sh(TAGS + 2, 0x3000); sw(TAGS + 4, lwu(cls + 0x18));
    sh(TAGS + 0x12, 0x3000); sh(TAGS + 0x10, static_cast<uint32_t>(na)); sw(TAGS + 0x14, fa);
    sw(TAGS + 0x24, fb); sw(TAGS + 0x20, nb);
    if (blend == 0) sh(TAGS + 0x12, 0);
    const uint32_t ka = S + 0x2000 + n * 16, kb = ka + static_cast<uint32_t>(na) * 16, kend = kb + nb * 16;
    sw(ka - 4, 0xffffffffu);
    sw(kb - 4, 0xffffffffu);
    sw(kend - 4, 0xffffffffu);
    // the chain: bind pose (n), frame A (na) and, when blending, frame B (nb), back to back
    spr_dma_to(0x2000, lwu(cls + 0x18), n);
    spr_dma_to(0x2000 + n * 16, fa, static_cast<uint32_t>(na));
    if (blend != 0) spr_dma_to(0x2000 + n * 16 + static_cast<uint32_t>(na) * 16, fb, nb);
    const int32_t na_rot = lh(fa - 6), na_pos = lh(fa - 2), nb_rot = lh(fb - 6), nb_pos = lh(fb - 2);
    const Q4 one{{1.0f, 1.0f, 1.0f, 0.0f}};
    for (uint32_t r = S; r != S + n * 64; r += 0x40) { sq4(r, one); sq4(r + 0x10, one); }
    const int32_t d = ((static_cast<int32_t>(lbu(m + 0x51)) - lbu(m + 0x50)) & 0xfffe) + (static_cast<int32_t>(lbu(m + 0x52)) - lbu(m + 0x53));
    const bool linear = d == 0;
    uint32_t bs = S + 0x2000, br = S - 0x100;
    int32_t bc = static_cast<int32_t>(n);
    do {
        br += 0x100;
        bc -= 4;
        const Q4 b[4] = {lq4(bs), lq4(bs + 0x10), lq4(bs + 0x20), lq4(bs + 0x30)};
        for (uint32_t i = 0; i < 4; ++i) {
            sq4(br + i * 0x40 + 0x20, b[i]);
            Q4 w = b[i];
            w.v[3] = vmul(w.v[3], 0.0f);
            sq4(br + i * 0x40 + 0x30, w);
        }
        bs += 0x40;
    } while (bc > 0);
    const float t = lwc1(m + 0x54), tw = vsub(1.0f, t);
    const int32_t count = lbu(mask + 0x7f);
    const Q4 zero4 = {};
    sq4(S + 0x2000, zero4);
    if (blend != 0) {
        // frame A: rotations (records with the top bit set) and translations into linked lists
        uint32_t a = ka + n * 8, tail = S + 0x1ff4;
        for (int32_t c = na_rot; c != 0;) {
            const uint32_t rec = a;
            --c;
            a += 8;
            if (!sign64(rec)) continue;
            const uint32_t r = S + static_cast<uint32_t>(lbu(rec + 6)) * 64;
            sw(tail + 0xc, r);
            tail = r;
            sw(r + 0x1c, top2(rec) - 1);
            sq3w(r, rot_rec(rec), 0.0f);
        }
        uint32_t ptail = S + 0x1fc8;
        for (int32_t c = na_pos; c != 0; --c, a += 8) {
            const uint32_t r = S + static_cast<uint32_t>(lbu(a + 6)) * 64;
            const uint32_t keep = lwu(r + 0x2c);
            sw(ptail + 0x3c, r);
            ptail = r;
            sq3w(r + 0x20, pos_rec(a), 0.0f);
            sw(r + 0x2c, keep);
        }
        sw(ptail + 0x3c, 0);
        // frame B: joints not yet listed are appended
        a = kb + n * 8;
        for (int32_t c = nb_rot; c != 0;) {
            const uint32_t rec = a;
            --c;
            a += 8;
            const uint32_t r = S + static_cast<uint32_t>(lbu(rec + 6)) * 64;
            const uint32_t next = lwu(r + 0xc);
            if (!sign64(rec)) continue;
            Q4 q = rot_rec(rec);
            q.v[3] = 0.0f;
            sq4(r + 0x10, q);
            sw(r + 0x1c, top2(rec) - 1);
            if (next != 0) continue;
            sw(tail + 0xc, r);
            tail = r;
            sw(tail + 0xc, 0);
        }
        for (int32_t c = nb_pos; c != 0; --c, a += 8) {
            const uint32_t r = S + static_cast<uint32_t>(lbu(a + 6)) * 64;
            const uint32_t next = lwu(r + 0x3c);
            sq3w(r + 0x30, pos_rec(a), 0.0f);
            sw(r + 0x3c, next);
            if (next != 0) continue;
            sw(ptail + 0x3c, r);
            ptail = r;
            sw(ptail + 0x3c, 0);
        }
        for (uint32_t r = lwu(S + 0x2000); r != 0;) {
            const Q4 x = lq4(r), y = lq4(r + 0x10);
            Q4 o = y;
            for (int i = 0; i < 3; ++i) o.v[i] = vmadd(vmul(x.v[i], tw), y.v[i], t);
            const uint32_t next = lwu(r + 0xc);
            sq4(r + 0x10, o);
            r = next;
        }
        for (uint32_t r = lwu(S + 0x2004); r != 0;) {
            const Q4 x = lq4(r + 0x20), y = lq4(r + 0x30);
            Q4 o = x;
            for (int i = 0; i < 3; ++i) o.v[i] = vmadd(vmul(x.v[i], tw), y.v[i], t);
            const uint32_t next = lwu(r + 0x3c);
            sq4(r + 0x20, o);
            r = next;
        }
        // key quaternions, two joints per step
        uint32_t qa = ka, qb = kb, r = S - 0x80;
        for (int32_t c = count; c > 0; c -= 2, qa += 0x10, qb += 0x10) {
            r += 0x80;
            for (uint32_t h = 0; h < 2; ++h) {
                const Q4 A = key_q(qa + h * 8), B = key_q(qb + h * 8);
                Q4 o{};
                if (linear) {
                    for (int i = 0; i < 4; ++i) o.v[i] = vmadd(vmul(A.v[i], tw), B.v[i], t);
                } else {
                    const float dt = dot4y(A, B);
                    for (int i = 0; i < 4; ++i) {
                        const float acc = vmul(A.v[i], tw);
                        o.v[i] = fneg(dt) ? vmadd(acc, vmul(B.v[i], -1.0f), t) : vmadd(acc, B.v[i], t);
                    }
                    const float q = vrsqrt1(len4(o));
                    for (int i = 0; i < 4; ++i) o.v[i] = vmul(o.v[i], q);
                }
                sq4(r + h * 0x40, o);
            }
        }
    } else {
        uint32_t a = ka + n * 8;
        for (int32_t c = na_rot; c != 0;) {
            const uint32_t rec = a;
            --c;
            a += 8;
            if (!sign64(rec)) continue;
            const uint32_t r = S + static_cast<uint32_t>(lbu(rec + 6)) * 64;
            sq3w(r + 0x10, rot_rec(rec), 0.0f);
            sw(r + 0x1c, top2(rec) - 1);
        }
        for (int32_t c = na_pos; c != 0; --c, a += 8) {
            const uint32_t r = S + static_cast<uint32_t>(lbu(a + 6)) * 64;
            const uint32_t keep = lwu(r + 0x2c);
            const Q4 w1 = Q4{{0.0f, 0.0f, 0.0f, 1.0f}};
            sq4(r + 0x30, w1);
            sq4(r + 0x20, pos_rec(a));
            sw(r + 0x2c, keep);
        }
        uint32_t qa = ka, r = S - 0x80;
        for (int32_t c = count; c > 0; c -= 2, qa += 0x10) {
            r += 0x80;
            sq4(r, key_q(qa));
            sq4(r + 0x40, key_q(qa + 8));
        }
    }
    // layers: each record blended into its joint in turn
    for (uint32_t layer = lwu(m + 0x60); layer != 0;) {
        const uint32_t cnt = lhu(layer + 2) * 3;
        spr_dma_to(0x2000, lwu(layer + 0x10), cnt);
        const float w = lwc1(layer + 8), ww = vsub(1.0f, w);
        const uint32_t end = S + 0x2000 + cnt * 16;
        layer = lwu(layer + 0x1c);
        uint32_t src = S + 0x2000;
        do {
            const uint32_t at = lwu(src + 0x2c);
            src += 0x30;
            const Q4 l0 = scale4(lq4(src - 0x30), w, 4), l1 = scale4(lq4(src - 0x20), w, 3), l2 = scale4(lq4(src - 0x10), w, 3);
            const Q4 d0 = scale4(lq4(at), ww, 4);
            Q4 d1 = scale4(lq4(at + 0x10), ww, 3), d2 = scale4(lq4(at + 0x20), ww, 3);
            for (int i = 0; i < 3; ++i) { d1.v[i] = vadd(d1.v[i], l1.v[i]); d2.v[i] = vadd(d2.v[i], l2.v[i]); }
            const Q4 q = nlerp_q(d0, l0);
            sq4(at + 0x10, d1);
            sq4(at + 0x20, d2);
            if (lwu(at + 0x1c) == 0) sw(at + 0x1c, lwu(src - 0x14));
            sq4(at, q);
        } while (src != end);
    }
    // overrides; each marks its joint scaled (+0x1c = 1)
    for (uint32_t o = lwu(m + 0x64); o != 0;) {
        const uint32_t at = lwu(o + 4);
        const uint32_t next = lwu(o + 8);
        pose_overrides_one(o, at);
        sw(at + 0x1c, 1);
        o = next;
    }
    // matrices of the flagged joints
    const uint32_t end = S + static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(mask + 0x7f))) * 64);
    uint32_t j = S, mp = mask;
    do {
        const int8_t f = static_cast<int8_t>(lbu(mp));
        ++mp;
        const uint32_t rec = j;
        j += 0x40;
        if (f == 0) continue;
        Q4 r0, r1, r2;
        quat_rows(lq4(rec), r0, r1, r2);
        const uint32_t scaled = lwu(rec + 0x1c), par = lwu(rec + 0x2c);
        const Q4 sc = lq4(rec + 0x10), tr = lq4(rec + 0x20);
        if (scaled != 0) {
            r0 = scale4(r0, sc.v[0], 3);
            r1 = scale4(r1, sc.v[1], 3);
            r2 = scale4(r2, sc.v[2], 3);
        }
        Q4 p[4] = {Q4{{1.0f, 0.0f, 0.0f, 0.0f}}, Q4{{0.0f, 1.0f, 0.0f, 0.0f}}, Q4{{0.0f, 0.0f, 1.0f, 0.0f}}, Q4{{0.0f, 0.0f, 0.0f, 1.0f}}};
        if (par != 0) for (uint32_t i = 0; i < 4; ++i) p[i] = lq4(par + i * 0x10);
        const Q4 r3 = Q4{{vadd(0.0f, tr.v[0]), vadd(0.0f, tr.v[1]), vadd(0.0f, tr.v[2]), 1.0f}};
        sq4(rec, xform_row(p, r0, false));
        sq4(rec + 0x10, xform_row(p, r1, false));
        sq4(rec + 0x20, xform_row(p, r2, false));
        sq4(rec + 0x30, xform_row(p, r3, true));
        if (j == end) break;
    } while (true);
}


// orig: 0x0024f190 — advance the moby animation state s (the first argument is unused): time
// +0x24 += rate +0x28 * speed +0x2c (snapped to 1 within 0.99..1.01); each whole frame moves to
// the next frame (+0x21, wrapping at the set's frame count with flag 2 in +5, flag 1 per step),
// rescaling the remainder by the new frame's speed; then resolves the frame pointers +0x38 /
// +0x3c (sub_0024f440 for blended sets +6 / +7) and builds the pose (moby_pose_blend).
void moby_anim_step(uint32_t s) {
    sb(s + 5, 0);
    auto frame_ptr = [s](uint32_t set, uint32_t frame) {
        const uint32_t sp = lwu(lwu(s + 0x14) + set * 4 + 0x48);
        return lwu(sp + frame * 4 + 0x1c);
    };
    const float rate = lwc1(s + 0x28);
    if (!(rate == 0.0f) && !(lwc1(s + 0x2c) == 0.0f)) {
        float tm = lwc1(s + 0x24) + rate * lwc1(s + 0x2c);
        swc1(s + 0x24, tm);
        if (0.0f < rate) {
            if (u2f(0x3f7d70a4u) < tm && tm < u2f(0x3f8147aeu)) swc1(s + 0x24, 1.0f);
            if (1.0f <= lwc1(s + 0x24)) {
                const uint32_t blend = lbu(s + 7);
                do {
                    sb(s + 6, blend);
                    sb(s + 5, lbu(s + 5) | 1);
                    sb(s + 0x20, lbu(s + 0x21));
                    if (blend != 0) sw(s + 0x38, lwu(s + 0x3c));
                    const uint32_t next = (lbu(s + 0x21) + 1) & 0xff;
                    const uint32_t set = lbu(s + 0x23);
                    sb(s + 0x22, set);
                    sb(s + 0x21, next);
                    if (!(next < lbu(lwu(lwu(s + 0x14) + set * 4 + 0x48) + 0x10))) {
                        sb(s + 0x21, 0);
                        sb(s + 5, lbu(s + 5) | 2);
                    }
                    tm = fpu_div(lwc1(s + 0x24) - 1.0f, lwc1(s + 0x2c));
                    swc1(s + 0x24, tm);
                    const float speed = lwc1(frame_ptr(lbu(s + 0x22), lbu(s + 0x20)));
                    tm = tm * speed;
                    swc1(s + 0x2c, speed);
                    swc1(s + 0x24, tm);
                } while (1.0f <= tm);
            }
        }
    }
    if (lbu(s + 6) != 0) sub_0024f440(s, lbu(s + 0x22), lbu(s + 0x20), lwu(s + 0x38));
    else sw(s + 0x38, frame_ptr(lbu(s + 0x22), lbu(s + 0x20)));
    if (lbu(s + 7) != 0) sub_0024f440(s, lbu(s + 0x23), lbu(s + 0x21), lwu(s + 0x3c));
    else sw(s + 0x3c, frame_ptr(lbu(s + 0x23), lbu(s + 0x21)));
    moby_pose_blend(s);
}


namespace {
// flag the joints of k chains (ids from id(i)) in the mask at 0x196e00, last joints listed at
// +0x80, and evaluate them (moby_pose_matrices_masked)
template <typename Ids>
static void chain_eval(uint32_t m, int32_t k, Ids id) {
    constexpr OvConst<0x00196e00u> MASK{};
    const uint32_t tab = lwu(lwu(m + 0x24) + 0x1c);
    for (uint32_t o = 0; o < 0x80; o += 8) sd(MASK + o, 0);
    int32_t top = 0;
    uint32_t last = MASK + 0x80;
    int32_t c = k, i = 0;
    do {
        uint32_t ch = lwu(tab + id(i++) * 4 + 4);
        int32_t cnt = lhu(ch);
        uint32_t j;
        do {
            j = lbu(ch + 4);
            ++ch;
            --cnt;
            sb(MASK + j, 1);
        } while (cnt > 0);
        sb(last, j);
        ++last;
        --c;
        top = std::max(top, static_cast<int32_t>(j));
    } while (c > 0);
    sb(MASK + 0x7f, static_cast<uint32_t>(top + 1));
    sb(MASK + static_cast<uint32_t>(top), 0xff);
    moby_pose_matrices_masked(m, MASK);
}
static uint32_t chain_last_record(uint32_t i) { return 0x70000000u + static_cast<uint32_t>(lbu(OV(0x00196e80u) + i)) * 64; }
}  // namespace

// orig: 0x00253fa0 — matrices for k joint chains of the moby (ids at chains[], chain table
// class +0x1c: u16 count, joint bytes from +4): flag every chain joint in the mask at 0x196e00,
// evaluate them (moby_pose_matrices_masked) and copy each chain's last joint matrix to dst.
void moby_chain_matrices(uint32_t m, int32_t k, uint32_t chains, uint32_t dst) {
    chain_eval(m, k, [chains](int32_t i) { return lwu(chains + static_cast<uint32_t>(i) * 4); });
    uint32_t out = dst - 0x40;
    int32_t c = k;
    for (uint32_t i = 0;; ++i) {
        const uint32_t src = chain_last_record(i);
        out += 0x40;
        for (uint32_t q = 0; q < 0x40; q += 0x10) lq_sq(out + q, src + q);
        if (!(--c > 0)) break;
    }
}


// orig: 0x00257880 — switch the moby to animation set `set` at frame `frame` (clamped to the
// set's last frame), blending in over `frames` frames: when mid-blend (+0x54 > 0.025) or with
// layers / overrides, the current pose is captured into a free slot (sub_0024f698,
// moby_pose_capture) and becomes the blend source (+0x50 slot, +0x52 = 0xff).
void moby_anim_set(uint32_t m, uint32_t set, int32_t frame, int32_t frames) {
    auto set_ptr = [m, set] { return lwu(lwu(m + 0x24) + set * 4 + 0x48); };
    const int32_t cnt = lbu(set_ptr() + 0x10);
    const int32_t last = frame < cnt ? frame : cnt - 1;
    bool capture = u2f(0x3ccccccdu) < lwc1(m + 0x54) || lwu(m + 0x60) != 0 || lwu(m + 0x64) != 0;
    if (capture) {
        const int32_t slot = sub_0024f698(m);
        if (slot < 0) {
            capture = false;
        } else {
            moby_pose_capture(m, static_cast<uint32_t>(slot) | 0x300);
            lq_sq(OV(0x00196d00u) + static_cast<uint32_t>(slot) * 16, m + 0xf0);
            const uint32_t prev = lbu(m + 0x52);
            if (prev != 0xff) sb(m + 0xa5, prev);
            sb(m + 0x52, 0xff);
            sb(m + 0x50, static_cast<uint32_t>(slot));
        }
    }
    sb(m + 0x51, static_cast<uint32_t>(last));
    sb(m + 0x53, set);
    sub_0024ebc0(m);
    swc1(m + 0x58, 1.0f);
    const float step = fpu_div(1.0f, int_to_float(frames));
    sw(m + 0x54, 0);
    sb(m + 0x70, lbu(m + 0x70) & 0xfd);
    swc1(m + 0x5c, step);
    sb(m + 0x7c, lbu(set_ptr() + 0x11));
}


// orig: 0x0024f8f0 — world matrices for k joint chains of the moby (moby_chain_matrices into dst):
// each is multiplied by the moby rotation (+0xc0, affine copy) and its translation row scaled by
// +0x2c / 1024 and offset by the moby position +0x10.
void moby_chain_world(uint32_t m, int32_t k, uint32_t chains, uint32_t dst) {
    const float s = lwc1(m + 0x2c) * u2f(0x3a800000u);
    Mat4 rot;
    mat3_copy_affine(rot, M(m + 0xc0));
    moby_chain_matrices(m, k, chains, dst);
    if (k <= 0) return;
    uint32_t d = dst;
    int32_t c = k;
    do {
        mat4_mul(M(d), rot, M(d));
        --c;
        vec3_scale(V(d + 0x30), V(d + 0x30), s);
        vec3_add(V(d + 0x30), V(d + 0x30), V(m + 0x10));
        d += 0x40;
    } while (c != 0);
}


// orig: 0x0024f3e0 — start animation record r of moby m on set/frame from the moby's current
// set and frame, blending over `frames` frames; step it once when `step`.
void moby_anim_rec_start(uint32_t m, uint32_t r, uint32_t set, uint32_t frame, int32_t frames, int32_t step) {
    const float rate = fpu_div(1.0f, int_to_float(frames));
    sb(r + 0x22, lbu(m + 0x53));
    sb(r + 0x23, set);
    sb(r + 0x20, lbu(m + 0x51));
    sb(r + 0x21, frame);
    swc1(r + 0x2c, rate);
    sw(r + 0x24, 0);
    swc1(r + 0x28, 1.0f);
    if (step != 0) moby_anim_step(r);
}

// orig: 0x0024f728 — world matrix of the last joint of chain `id` of the moby into dst: the
// chain matrix with its translation scaled by +0x2c / 1024, times the moby rotation, plus +0x10.
void moby_chain_world_one_v(uint32_t m, uint32_t id, Mat4& d) {
    const float s = lwc1(m + 0x2c) * u2f(0x3a800000u);
    chain_eval(m, 1, [id](int32_t) { return id; });
    d = M(chain_last_record(0));
    vec3_scale(d.r[3], d.r[3], s);
    Mat4 rot;
    mat3_copy_affine(rot, M(m + 0xc0));
    mat4_mul(d, rot, d);
    vec3_add(d.r[3], d.r[3], V(m + 0x10));
}
// orig: 0x0024f728 (EE-address form)
void moby_chain_world_one(uint32_t m, uint32_t id, uint32_t dst) { moby_chain_world_one_v(m, id, M(dst)); }

// orig: 0x0024f7c8 — world position of the last joint of chain `id` of the moby into out.
void moby_chain_point_v(uint32_t m, uint32_t id, Vec4& out) {
    const float s = lwc1(m + 0x2c) * u2f(0x3a800000u);
    chain_eval(m, 1, [id](int32_t) { return id; });
    const Vec4 t = V(chain_last_record(0) + 0x30);
    Vec4 v;
    vec3_scale(v, t, s);
    mat3_transform(v, v, M(m + 0xc0));
    vec3_add(v, v, V(m + 0x10));
    out = v;
}
// orig: 0x0024f7c8 (EE-address form)
void moby_chain_point(uint32_t m, uint32_t id, uint32_t out) { moby_chain_point_v(m, id, V(out)); }

// orig: 0x00257e58 — cosine ease from a (t = 0) to b (t = 1).
float cos_ease(float a, float b, float t) {
    if (t == 0.0f) return a;
    if (t == 1.0f) return b;
    const float c = cos_vu(t * u2f(0x40490fdbu));
    return a + (b - a) * ((1.0f - c) * 0.5f);
}

// orig: 0x0025cef0 — rotation rows 0..2 of m to a quaternion.
void mat_to_quat(Vec4& q, const Mat4& m) {
    const float m00 = m.r[0].x, m11 = m.r[1].y, m22 = m.r[2].z;
    const float tr = (m00 + m11) + m22;
    if (0.0f < tr) {
        const float s = sqrt_f(tr + 1.0f);
        q.w = s * 0.5f;
        const float k = fpu_div(0.5f, s);
        q.x = (m.r[2].y - m.r[1].z) * k;
        q.y = (m.r[0].z - m.r[2].x) * k;
        q.z = (m.r[1].x - m.r[0].y) * k;
        return;
    }
    const float a[9] = {m00, m.r[0].y, m.r[0].z, m.r[1].x, m11, m.r[1].z, m.r[2].x, m.r[2].y, m22};  // 3x3, stride 3
    static constexpr int32_t next[3] = {1, 2, 0};  // the table at 0x160090
    int32_t i = (m00 < m11) ? 1 : 0;
    if (a[i * 4] < m22) i = 2;
    const int32_t j = next[i], k = next[j];
    float* out = &q.x;
    const float s = sqrt_f((a[i * 4] - (a[j * 4] + a[k * 4])) + 1.0f);
    out[i] = s * 0.5f;
    const float h = (s == 0.0f) ? s : fpu_div(0.5f, s);
    const float w = (a[k * 3 + j] - a[j * 3 + k]) * h;
    const float vj = (a[j * 3 + i] + a[i * 3 + j]) * h;
    const float vk = (a[k * 3 + i] + a[i * 3 + k]) * h;
    q.w = w;
    out[j] = vj;
    out[k] = vk;
}

// orig: 0x0025d168 — the quaternion of the moby / camera matrix at m (rows 0..2).
void mat_to_quat_at_v(Vec4& q, uint32_t m) {
    Mat4 t;
    mat3_copy_affine(t, M(m));
    mat_to_quat(q, t);
    mat3_copy(M(m), t);
}
// orig: 0x0025d168 (EE-address form)
void mat_to_quat_at(uint32_t q, uint32_t m) { mat_to_quat_at_v(V(q), m); }

// orig: 0x0025d1d0 — quaternion of a rotation by angle about a unit axis.
void quat_axis_angle(Vec4& q, const Vec4& axis, float angle) {
    const float h = angle * 0.5f;
    vec3_scale(q, axis, sin_vu(h));
    q.w = cos_vu(h);
}

// orig: 0x0025fb78 — v rotated by the quaternion q (q v q*).
void quat_rotate(Vec4& d, const Vec4& v, const Vec4& q) {
    Vec4 c;
    vec3_scale(c, q, -1.0f);
    c.w = q.w;
    Vec4 p = v;
    p.w = 0.0f;
    Vec4 t;
    quat_mul(t, q, p);
    quat_mul(d, t, c);
}

// orig: 0x0025fc08 — v rotated by angle about axis (a copy when |angle| < 1e-5).
void vec_rotate_axis(Vec4& d, const Vec4& v, const Vec4& axis, float angle) {
    if (abs_f(angle) < u2f(0x3727c5acu)) {
        d = v;
        return;
    }
    Vec4 q;
    vec3_set_length(q, axis, 1.0f);
    quat_axis_angle(q, q, angle);
    quat_rotate(d, v, q);
}

// orig: 0x00259830 — does the moby's collision (+0x94) have its +8 word set?
int32_t moby_coll_flag8(uint32_t m) {
    const uint32_t c = lwu(m + 0x94);
    if (c == 0) return 0;
    return lw(c + 8) != 0 ? 1 : 0;
}

// orig: 0x00259a88 — record a hit on moby m in the 64-entry ring at 0x178100 (next slot
// *(0x173e54)), unless the moby's current record is newer (+0x1c below its +0x2c). `rec` is laid out as EE memory (words in
// place; its bytes and halfword at their swizzled offsets: ee_byte_addr / ee_half_addr)
void moby_hit_record_v(uint32_t m, const uint8_t* rec) {
    const int32_t slot = lw(OV(0x00173e54));
    const uint32_t cur = lbu(m + 0xa4);
    float strength;
    std::memcpy(&strength, rec + 0x1c, 4);
    if (cur != 0xff) {
        const uint32_t e = OV(0x00178100) + cur * 0x40;
        if (lwu(e + 0x34) == m && strength < lwc1(e + 0x2c)) return;
    }
    const uint32_t e = OV(0x00178100) + static_cast<uint32_t>(slot) * 0x40;
    vec_clear(V(e));
    std::memcpy(ee_ram_ptr(e + 0x10), rec, 16);
    uint32_t w;
    std::memcpy(&w, rec + 0x10, 4);
    sw(e + 0x20, w);
    std::memcpy(&w, rec + 0x14, 4);
    sw(e + 0x24, w);
    sb(e + 0x28, rec[ee_byte_addr(0x18)]);
    sb(e + 0x29, rec[ee_byte_addr(0x19)]);
    uint16_t h;
    std::memcpy(&h, rec + ee_half_addr(0x1a), 2);
    sh(e + 0x2a, h);
    swc1(e + 0x2c, strength);
    std::memcpy(&w, rec + 0x20, 4);
    sw(e + 0x38, 0);
    sw(e + 0x30, w);
    sw(e + 0x34, m);
    sb(m + 0xa4, static_cast<uint32_t>(slot));
    sw(OV(0x00173e54), static_cast<uint32_t>((slot + 1) & 0x3f));
}
// orig: 0x00259a88 (EE-address form)
void moby_hit_record(uint32_t m, uint32_t rec) { moby_hit_record_v(m, static_cast<const uint8_t*>(ee_ram_ptr(rec))); }

}  // namespace rac1
