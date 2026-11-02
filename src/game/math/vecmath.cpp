// Faithful ports of the Veldin overlay math library — see vecmath.h.
// Notation in comments: the original VU0 macro / FPU sequence, then the port.

#include <game/math/vecmath.h>

#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/ee/ps2_fpu.h>

#include <algorithm>
#include <bit>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace rac1 {

namespace {

constexpr uint32_t ADDR_FRAME_SCALE = 0x0015ed68;   // $gp - 0x7e98
constexpr OvConst<0x00160760u> ADDR_ASIN_COEFFS{};   // $gp - 0x64a0, 4 floats
constexpr OvConst<0x001c2420u> ADDR_ATAN_COEFFS{};   // 8 floats (t, t^3 .. t^15)
constexpr OvConst<0x001c2440u> ADDR_ATAN_OCTANTS{};  // 8 x {scale, offset}

constexpr float f32(uint32_t bits) { return std::bit_cast<float>(bits); }
constexpr float PI = f32(0x40490fdb);
constexpr float HALF_PI = f32(0x3fc90fdb);
constexpr float QUARTER_PI = f32(0x3f490fdb);

// CVT.W.S / VFTOI0: truncate toward zero, saturate to the int32 range.
// orig: (helper) EE CVT.W.S / VU FTOI0 semantics
inline int32_t cvt_w(float v) {
    if (v >= 2147483648.0f) return INT32_MAX;
    if (v <= -2147483648.0f) return INT32_MIN;
    return static_cast<int32_t>(v);
}

// VDIV / VRSQRT: division by zero gives +-FLT_MAX on the PS2.
// orig: (helper) VU DIV/RSQRT divide-by-zero semantics
inline float vu_div(float num, float den) {
    if (den == 0.0f) return (std::signbit(num) != std::signbit(den)) ? -FLT_MAX : FLT_MAX;
    return num / den;
}

}  // namespace

// --- frame-rate scaling ---------------------------------------------------

// lwc1 f0, g_frame_scale ; mul.s f0, f0, f12
float frame_scale_f(float v) { return ee_read<float>(ADDR_FRAME_SCALE) * v; }

// acc = 0.25 + 0.25 ; f1 = acc + (float)frames * g_frame_scale ; cvt.w (truncate) => rounded
// orig: 0x001fef20
int32_t frame_scale_i(int32_t frames) {
    const float acc = 0.25f + 0.25f;
    return cvt_w(acc + static_cast<float>(frames) * ee_read<float>(ADDR_FRAME_SCALE));
}

// --- countdown timers -----------------------------------------------------

// orig: 0x001fef68
int32_t timer_tick_i32(int32_t& t) {
    if (t == 0) return 1;
    const int32_t v = std::max(t, 1) - 1;  // pmaxw, addi -1
    t = v;
    return v > 0 ? 0 : 2;
}

// orig: 0x001fef98
int32_t timer_tick_i16(int16_t& t) {
    const int32_t cur = t;  // lh: sign-extended
    if (cur == 0) return 1;
    const int32_t v = std::max(cur, 1) - 1;
    t = static_cast<int16_t>(v);
    return v > 0 ? 0 : 2;
}

// plzcw counts the bits after bit 31 that equal bit 31; result = 30 - count.
// orig: 0x001feff8
int32_t highest_bit_index(int32_t v) {
    const uint32_t u = static_cast<uint32_t>(v), sign = u >> 31;
    int32_t n = 0;
    for (int i = 30; i >= 0 && ((u >> i) & 1u) == sign; --i) ++n;
    return 30 - n;
}

// --- memory -----------------------------------------------------------------

// A countdown loop of nops: no observable effect outside the CPU.
// orig: 0x001ff018
void spin_delay(int32_t) {}

// orig: 0x001ff040
void mem_fill32(uint32_t dst, uint32_t value, int32_t n) {
    do {
        ee_write<uint32_t>(dst, value);
        n -= 4;
        dst += 4;
    } while (n > 0);
}

// orig: 0x001ff068
void mem_clear128(uint32_t dst, int32_t n) {
    do {
        ee_write<uint64_t>(dst & ~15u, 0);
        ee_write<uint64_t>((dst & ~15u) + 8, 0);
        n -= 16;
        dst += 16;
    } while (n > 0);
}

// orig: 0x001ff090
void mem_copy(uint32_t dst, uint32_t src, int32_t n) {
    const uint32_t end = dst + static_cast<uint32_t>(n);
    const uint32_t align = dst | src | end;
    if ((align & 15u) == 0) {
        do {
            const uint64_t lo = ee_read<uint64_t>(src), hi = ee_read<uint64_t>(src + 8);
            src += 16;
            ee_write<uint64_t>(dst, lo);
            ee_write<uint64_t>(dst + 8, hi);
            dst += 16;
        } while (dst != end);
    } else if ((align & 3u) == 0) {
        do {
            const uint32_t w = ee_read<uint32_t>(src);
            src += 4;
            ee_write<uint32_t>(dst, w);
            dst += 4;
        } while (dst != end);
    } else {
        do {
            const uint8_t b = ee_read<uint8_t>(src);
            src += 1;
            ee_write<uint8_t>(dst, b);
            dst += 1;
        } while (dst != end);
    }
}

// orig: 0x001ff150 — dst = a | b, a quadword at a time (lq / por / sq).
void mem_or128(uint32_t dst, uint32_t a, uint32_t b, int32_t n) {
    do {
        const uint64_t lo = ee_read<uint64_t>(a & ~15u) | ee_read<uint64_t>(b & ~15u);
        const uint64_t hi = ee_read<uint64_t>((a & ~15u) + 8) | ee_read<uint64_t>((b & ~15u) + 8);
        a += 16;
        b += 16;
        n -= 16;
        ee_write<uint64_t>(dst & ~15u, lo);
        ee_write<uint64_t>((dst & ~15u) + 8, hi);
        dst += 16;
    } while (0 < n);
}

// orig: 0x001ff128
void mem_copy128(uint32_t dst, uint32_t src, int32_t n) {
    do {
        const uint64_t lo = ee_read<uint64_t>(src & ~15u), hi = ee_read<uint64_t>((src & ~15u) + 8);
        src += 16;
        dst += 16;
        n -= 16;
        ee_write<uint64_t>((dst - 16) & ~15u, lo);
        ee_write<uint64_t>(((dst - 16) & ~15u) + 8, hi);
    } while (n > 0);
}

// --- scalars ------------------------------------------------------------------

// vsqrt Q, vf1x (VU square root takes |x|) ; vaddq.x vf1, vf0, Q
float sqrt_f(float v) { return 0.0f + std::sqrt(std::fabs(v)); }

int32_t abs_i(int32_t v) { return v < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(v)) : v; }
float abs_f(float v) { return std::fabs(v); }
float max_f(float a, float b) { return std::max(a, b); }
float min_f(float a, float b) { return std::min(a, b); }
// max.s / min.s: v into [lo, hi]
// orig: 0x001ff240
float clampf(float v, float lo, float hi) { return std::min(std::max(v, lo), hi); }

// The sum of n bytes, two at a time (at least two).
// orig: 0x001ff7b8
int32_t bytes_sum(uint32_t p, int32_t n) {
    int32_t s = 0;
    do {
        s += ee_read<uint8_t>(p) + ee_read<uint8_t>(p + 1);
        p += 2;
        n -= 2;
    } while (n > 0);
    return s;
}

// sign * (pi/2 - sqrt(1 - |x|) * (c0 + |x| c1 + x^2 c2 + |x|^3 c3))
// orig: 0x001ff818
float asin_f(float v) {
    float sign = 1.0f;
    if (v < 0.0f) {
        v = std::fabs(v);
        sign = -1.0f;
    }
    const float one_minus = 1.0f - v;
    const float x2 = v * v;
    const float root = 0.0f + std::sqrt(std::fabs(one_minus));
    const float x3 = v * x2;
    const float c0 = ee_read<float>(ADDR_ASIN_COEFFS + 0), c1 = ee_read<float>(ADDR_ASIN_COEFFS + 4);
    const float c2 = ee_read<float>(ADDR_ASIN_COEFFS + 8), c3 = ee_read<float>(ADDR_ASIN_COEFFS + 12);
    float acc = c0 + 0.0f;
    acc = acc + v * c1;
    acc = acc + x2 * c2;
    const float poly = acc + x3 * c3;
    return sign * (HALF_PI - poly * root);
}

// Octant reduction to t = (min - max) / (min + max), odd polynomial of degree
// 15 in t, then scale/offset from an 8-entry table indexed by the signs of
// (|y| - |x|), x and y.
// orig: 0x001ff8b0
float atan2_f(float y, float x) {
    const float ay = std::fabs(y), ax = std::fabs(x);
    const float d0 = ay - ax;
    float lo, hi;
    if (std::signbit(d0)) { lo = ay; hi = ax; } else { lo = ax; hi = ay; }
    if (hi <= 0.0f) return 0.0f;
    const float t = ps2fpu::div_nearest(lo - hi, lo + hi);  // DIV.S (to nearest)

    const uint32_t idx = (std::bit_cast<uint32_t>(d0) >> 31) * 8 + (std::bit_cast<uint32_t>(x) >> 31) * 16 +
                         (std::bit_cast<uint32_t>(y) >> 31) * 32;
    const float scale = ee_read<float>(ADDR_ATAN_OCTANTS + idx);
    const float offset = ee_read<float>(ADDR_ATAN_OCTANTS + idx + 4);
    float c[8];
    for (int i = 0; i < 8; ++i) c[i] = ee_read<float>(ADDR_ATAN_COEFFS + 4 * i);

    // VU0 lanes: p = (t, t^3, t^5, t^7), q = p * t^8
    const float t2 = t * t, t4 = t2 * t2, t8 = t4 * t4;
    const float p0 = t, p1 = t * t2, p2 = t * t4, p3 = (t * t4) * t2;
    const float q0 = p0 * t8, q1 = p1 * t8, q2 = p2 * t8, q3 = p3 * t8;
    float acc = p0 * c[0] + p1 * c[1];
    acc = acc + 1.0f * (p2 * c[2]);
    acc = acc + 1.0f * (p3 * c[3]);
    acc = acc + 1.0f * (q0 * c[4]);
    acc = acc + 1.0f * (q1 * c[5]);
    acc = acc + 1.0f * (q2 * c[6]);
    const float poly = acc + 1.0f * (q3 * c[7]);
    return ((QUARTER_PI + poly) * scale) + offset;
}

// orig: 0x002000e8
float angle_add(float a, float b) {
    float s = a + b;
    if (!(s < PI)) { s = s - PI; s = s - PI; }
    else if (s < -PI) { s = s + PI; s = s + PI; }
    return s;
}

// orig: 0x00200130
float angle_sub(float a, float b) {
    float s = a - b;
    if (!(s < PI)) { s = s - PI; s = s - PI; }
    else if (s < -PI) { s = s + PI; s = s + PI; }
    return s;
}

// orig: 0x002001f0
float angle_dist(float a, float b) {
    const float d = std::fabs(a - b);
    if (d < PI) return d;
    return (PI + PI) - d;
}

// orig: 0x00200228
float split_int_frac(float* int_part, float v) {
    const float i = static_cast<float>(cvt_w(v));
    *int_part = i;
    return v - i;
}

float int_to_float(int32_t v) { return static_cast<float>(v); }
int32_t float_to_int(float v) { return cvt_w(v); }

// --- vectors ------------------------------------------------------------------

void vec_clear(Vec4& d) { d = Vec4{0.0f, 0.0f, 0.0f, 0.0f}; }
void vec_set_unit_z(Vec4& d) { d = Vec4{0.0f, 0.0f, 1.0f, 0.0f}; }  // vmr32 vf0

namespace {
// orig: (helper) a VU0 result: denormals become signed zero, Inf / NaN +-max
inline float vu_result(float f) {
    uint32_t u = std::bit_cast<uint32_t>(f);
    const uint32_t e = u & 0x7f800000u;
    if (e == 0) u &= 0x80000000u;
    else if (e == 0x7f800000u) u = (u & 0x80000000u) | 0x7f7fffffu;
    return std::bit_cast<float>(u);
}
// orig: (helper) a VU0 operand: Inf / NaN is read as +-max, a denormal as a signed zero (so -denormal + 0 is +0, not -0; the Devastator's rocket)
inline float vu_in(float f) {
    uint32_t u = std::bit_cast<uint32_t>(f);
    if ((u & 0x7f800000u) == 0) u &= 0x80000000u;
    else if ((u & 0x7f800000u) == 0x7f800000u) u = (u & 0x80000000u) | 0x7f7fffffu;  // (exponent 255: read as +-max, as the VU0 / PCSX2's vuDouble do)
    return std::bit_cast<float>(u);
}
}  // namespace

void vec3_add(Vec4& d, const Vec4& a, const Vec4& b) { d = Vec4{vu_result(vu_in(a.x) + vu_in(b.x)), vu_result(vu_in(a.y) + vu_in(b.y)), vu_result(vu_in(a.z) + vu_in(b.z)), a.w}; }
void vec3_add_inplace(Vec4& a, const Vec4& b) { a = Vec4{vu_result(vu_in(a.x) + vu_in(b.x)), vu_result(vu_in(a.y) + vu_in(b.y)), vu_result(vu_in(a.z) + vu_in(b.z)), a.w}; }
void vec3_sub(Vec4& d, const Vec4& a, const Vec4& b) { d = Vec4{vu_result(vu_in(a.x) - vu_in(b.x)), vu_result(vu_in(a.y) - vu_in(b.y)), vu_result(vu_in(a.z) - vu_in(b.z)), a.w}; }
void vec3_scale(Vec4& d, const Vec4& a, float s) { d = Vec4{vu_result(vu_in(a.x) * vu_in(s)), vu_result(vu_in(a.y) * vu_in(s)), vu_result(vu_in(a.z) * vu_in(s)), a.w}; }

// The mirror image of v off the plane with normal n (normalised here): v - 2 (v.n) n, written
// as -v + 2 (n (-v.n) + v); v itself when -v.n < 0 (v leaves the plane). w: v's.
// orig: 0x001ff660
void vec3_reflect(Vec4& d, const Vec4& v, const Vec4& n) {
    const Vec4 a{vu_result(0.0f - v.x), vu_result(0.0f - v.y), vu_result(0.0f - v.z), v.w};
    const float l2 = vu_result(vu_result(n.x * n.x + n.y * n.y) + 1.0f * (n.z * n.z));
    const float q = vu_div(1.0f, std::sqrt(std::fabs(l2)));
    Vec4 u{vu_result(n.x * q), vu_result(n.y * q), vu_result(n.z * q), n.w};
    const float t = vu_result(vu_result(a.x * u.x + a.y * u.y) + 1.0f * (a.z * u.z));
    u = Vec4{vu_result(u.x * t), vu_result(u.y * t), vu_result(u.z * t), u.w};
    if (std::signbit(t)) {
        d = v;
        return;
    }
    Vec4 r = Vec4{vu_result(u.x - a.x), vu_result(u.y - a.y), vu_result(u.z - a.z), u.w};
    r = Vec4{vu_result(r.x + r.x), vu_result(r.y + r.y), vu_result(r.z + r.z), r.w};
    d = Vec4{vu_result(a.x + r.x), vu_result(a.y + r.y), vu_result(a.z + r.z), v.w};
}

// orig: 0x001ff378
void vec3_div(Vec4& d, const Vec4& a, float s) {
    const float q = vu_div(1.0f, s);
    d = Vec4{a.x * q, a.y * q, a.z * q, a.w};
}

// vmul.xyz ; vadday.x ACC = x + y ; vmaddz.x = ACC + 1 * z
float vec3_dot(const Vec4& a, const Vec4& b) {
    const float x = a.x * b.x, y = a.y * b.y, z = a.z * b.z;
    return (x + y) + 1.0f * z;
}

// vopmula ACC, b, a ; vopmsub d, a, b  =>  b x a. The original leaves d.w with
// whatever the VU register held (not defined); the port writes 0.
// orig: 0x001ff3c0
void vec3_cross_ba(Vec4& d, const Vec4& a, const Vec4& b) {
    d = Vec4{b.y * a.z - a.y * b.z, b.z * a.x - a.z * b.x, b.x * a.y - a.x * b.y, 0.0f};
}

// orig: 0x001ff3d8
float vec3_length(const Vec4& a) {
    const float l2 = (a.x * a.x + a.y * a.y) + 1.0f * (a.z * a.z);
    return 0.0f + std::sqrt(std::fabs(l2));
}

// orig: 0x001ff408
float vec2_length(const Vec4& a) {
    const float l2 = a.x * a.x + a.y * a.y;
    return 0.0f + std::sqrt(std::fabs(l2));
}

// orig: 0x001ff450
float vec3_distance(const Vec4& a, const Vec4& b) {
    const float x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
    const float l2 = (x * x + y * y) + 1.0f * (z * z);
    return 0.0f + std::sqrt(std::fabs(l2));
}

// orig: 0x001ff488
float vec2_distance(const Vec4& a, const Vec4& b) {
    const float x = a.x - b.x, y = a.y - b.y;
    return 0.0f + std::sqrt(std::fabs(x * x + y * y));
}

// orig: 0x001ff500
void vec3_set_length(Vec4& d, const Vec4& a, float len) {
    const float l2 = (a.x * a.x + a.y * a.y) + 1.0f * (a.z * a.z);
    if (std::bit_cast<uint32_t>(l2) == 0) {
        d = Vec4{0.0f, 0.0f, 0.0f, a.w};
        return;
    }
    const float q = vu_div(len, std::sqrt(std::fabs(l2)));  // vrsqrt
    d = Vec4{a.x * q, a.y * q, a.z * q, a.w};
}

// orig: 0x001ff550
void vec2_set_length(Vec4& d, const Vec4& a, float len) {
    const float l2 = a.x * a.x + a.y * a.y;
    if (std::bit_cast<uint32_t>(l2) == 0) {
        d = Vec4{0.0f, 0.0f, a.z, a.w};
        return;
    }
    const float q = vu_div(len, std::sqrt(std::fabs(l2)));
    d = Vec4{a.x * q, a.y * q, a.z, a.w};
}

// orig: 0x001ff600
int32_t vec2_clamp_length(Vec4& d, const Vec4& a, float max_len) {
    const float len = 0.0f + std::sqrt(std::fabs(a.x * a.x + a.y * a.y));
    const float diff = max_len - len;
    if (std::bit_cast<uint32_t>(len) == 0) return 0;
    if (static_cast<int32_t>(std::bit_cast<uint32_t>(diff)) > 0) return 0;  // diff > +0
    const float q = vu_div(max_len, len);
    d = Vec4{a.x * q, a.y * q, a.z, a.w};
    return 1;
}

// orig: 0x001ff768
uint64_t vec4_to_s16x4(const Vec4& a, float scale) {
    const float v[4] = {a.x * scale, a.y * scale, a.z * scale, a.w * scale};
    uint64_t out = 0;
    for (int i = 0; i < 4; ++i) {
        const int32_t c = std::clamp(cvt_w(v[i]), -32768, 32767);
        out |= static_cast<uint64_t>(static_cast<uint16_t>(c)) << (16 * i);
    }
    return out;
}

// The four bytes of `c` as floats (PEXTLB / PEXTLH, VITOF0).
// orig: 0x001ff728
void rgba_to_vec(Vec4& d, uint32_t c) {
    d = Vec4{static_cast<float>(c & 0xffu), static_cast<float>((c >> 8) & 0xffu), static_cast<float>((c >> 16) & 0xffu),
             static_cast<float>(c >> 24)};
}

// The vector's four components truncated (VFTOI0), each one's low byte packed (PPACH / PPACB).
// orig: 0x001ff740
uint32_t vec_to_rgba(const Vec4& a) {
    const float v[4] = {a.x, a.y, a.z, a.w};
    uint32_t out = 0;
    for (int i = 0; i < 4; ++i) out |= (static_cast<uint32_t>(cvt_w(v[i])) & 0xffu) << (8 * i);
    return out;
}

// ((1 - t) * a + t * b) per RGBA byte, truncated, low 8 bits kept.
// orig: 0x00200298
uint32_t rgba_lerp(uint32_t a, uint32_t b, float t) {
    const float inv = 1.0f - t;
    uint32_t out = 0;
    for (int i = 0; i < 4; ++i) {
        const float ca = static_cast<float>((a >> (8 * i)) & 0xffu);
        const float cb = static_cast<float>((b >> (8 * i)) & 0xffu);
        const int32_t c = cvt_w(ca * inv + cb * t);
        out |= (static_cast<uint32_t>(c) & 0xffu) << (8 * i);
    }
    return out;
}

// --- matrices -------------------------------------------------------------------

namespace {
inline float mac3(float m0, float vx, float m1, float vy, float m2, float vz) { return (m0 * vx + m1 * vy) + m2 * vz; }
// orig: (helper) the VU operand rule on a vector's / a matrix's lanes
inline Vec4 vu_operands(const Vec4& v) { return Vec4{vu_result(v.x), vu_result(v.y), vu_result(v.z), vu_result(v.w)}; }
inline Mat4 vu_operands(const Mat4& m) {
    Mat4 r;
    for (int i = 0; i < 4; ++i) r.r[i] = vu_operands(m.r[i]);
    return r;
}
}  // namespace

// (the VU operand rule on the matrix and the vector: denormals read as signed zero, Inf / NaN
// as the largest float)
void mat3_transform(Vec4& d, const Vec4& v, const Mat4& m) {
    const Mat4 a = vu_operands(m);
    const Vec4 s = vu_operands(v);
    d.x = mac3(a.r[0].x, s.x, a.r[1].x, s.y, a.r[2].x, s.z) + 0.0f * s.w;
    d.y = mac3(a.r[0].y, s.x, a.r[1].y, s.y, a.r[2].y, s.z) + 0.0f * s.w;
    d.z = mac3(a.r[0].z, s.x, a.r[1].z, s.y, a.r[2].z, s.z) + 0.0f * s.w;
    d.w = mac3(a.r[0].w, s.x, a.r[1].w, s.y, a.r[2].w, s.z) + 1.0f * s.w;
}

void mat4_transform(Vec4& d, const Vec4& v, const Mat4& m) {
    const Mat4 a = vu_operands(m);
    const Vec4 s = vu_operands(v);
    d.x = mac3(a.r[0].x, s.x, a.r[1].x, s.y, a.r[2].x, s.z) + a.r[3].x * s.w;
    d.y = mac3(a.r[0].y, s.x, a.r[1].y, s.y, a.r[2].y, s.z) + a.r[3].y * s.w;
    d.z = mac3(a.r[0].z, s.x, a.r[1].z, s.y, a.r[2].z, s.z) + a.r[3].z * s.w;
    d.w = mac3(a.r[0].w, s.x, a.r[1].w, s.y, a.r[2].w, s.z) + a.r[3].w * s.w;
}

// orig: 0x001ff9e8
void mat4_identity(Mat4& m) {
    m.r[0] = Vec4{1.0f, 0.0f, 0.0f, 0.0f};
    m.r[1] = Vec4{0.0f, 1.0f, 0.0f, 0.0f};
    m.r[2] = Vec4{0.0f, 0.0f, 1.0f, 0.0f};
    m.r[3] = Vec4{0.0f, 0.0f, 0.0f, 1.0f};
}

// orig: 0x001ffcb8
void mat3_copy_affine(Mat4& d, const Mat4& s) {
    const Vec4 r0 = s.r[0], r1 = s.r[1], r2 = s.r[2];
    d.r[0] = r0; d.r[1] = r1; d.r[2] = r2;
    d.r[3] = Vec4{0.0f, 0.0f, 0.0f, 1.0f};
}

// orig: 0x001ffcd8
void mat3_copy(Mat4& d, const Mat4& s) {
    const Vec4 r0 = s.r[0], r1 = s.r[1], r2 = s.r[2];
    d.r[0] = r0; d.r[1] = r1; d.r[2] = r2;
}

// Elements are produced by vaddN.M vfD, vf0, vfS, i.e. 0.0f + value (which
// turns -0 into +0); the w lanes are vf0.w - vf0.w = 0.
// orig: 0x001ffcf8
void mat3_transpose(Mat4& d, const Mat4& s) {
    const Vec4 a = s.r[0], b = s.r[1], c = s.r[2];
    d.r[3] = Vec4{0.0f, 0.0f, 0.0f, 1.0f};
    d.r[0] = Vec4{0.0f + a.x, 0.0f + b.x, 0.0f + c.x, 1.0f - 1.0f};
    d.r[1] = Vec4{0.0f + a.y, 0.0f + b.y, 0.0f + c.y, 1.0f - 1.0f};
    d.r[2] = Vec4{0.0f + a.z, 0.0f + b.z, 0.0f + c.z, 1.0f - 1.0f};
}

// orig: 0x001ffd88
void mat3_mul(Mat4& d, const Mat4& a, const Mat4& b) {
    const Mat4 A = a, B = b;
    for (int i = 0; i < 3; ++i) {
        const Vec4& v = B.r[i];
        d.r[i] = Vec4{mac3(A.r[0].x, v.x, A.r[1].x, v.y, A.r[2].x, v.z), mac3(A.r[0].y, v.x, A.r[1].y, v.y, A.r[2].y, v.z),
                      mac3(A.r[0].z, v.x, A.r[1].z, v.y, A.r[2].z, v.z), mac3(A.r[0].w, v.x, A.r[1].w, v.y, A.r[2].w, v.z)};
    }
}

// orig: 0x001ffdd8
void mat4_mul(Mat4& d, const Mat4& a, const Mat4& b) {
    const Mat4 A = a;
    for (int i = 0; i < 4; ++i) {
        const Vec4 v = b.r[i];  // read row i of b, then write row i of d (same order as the original loop)
        Vec4 out;
        mat4_transform(out, v, A);
        d.r[i] = out;
    }
}

// orig: 0x001ffe68
void quat_mul(Vec4& d, const Vec4& a, const Vec4& b) {
    const float ww = b.w * a.w;
    const float dx = b.x * a.x, dy = b.y * a.y, dz = b.z * a.z;
    const float dot = (dx + dy) + 1.0f * dz;
    const Vec4 bw{b.x * a.w, b.y * a.w, b.z * a.w, 0.0f};
    const Vec4 aw{a.x * b.w, a.y * b.w, a.z * b.w, 0.0f};
    const Vec4 cr{a.y * b.z - b.y * a.z, a.z * b.x - b.z * a.x, a.x * b.y - b.x * a.y, 0.0f};  // a x b
    d = Vec4{(bw.x + aw.x) + cr.x, (bw.y + aw.y) + cr.y, (bw.z + aw.z) + cr.z, ww - dot};
}

// --- trigonometry and rotations ----------------------------------------------

namespace {

// VU MAX / MINI compare the float bit patterns as signed-magnitude integers.
// orig: (helper) a VU result: Inf / NaN become +-max, denormals signed zero
inline float vuclamp(float f) {
    uint32_t u = std::bit_cast<uint32_t>(f);
    const uint32_t e = u & 0x7f800000u;
    if (e == 0) u &= 0x80000000u;
    else if (e == 0x7f800000u) u = (u & 0x80000000u) | 0x7f7fffffu;
    return std::bit_cast<float>(u);
}
// orig: (helper) VU MAX semantics
inline float vu_max(float a, float b) {
    const int32_t x = std::bit_cast<int32_t>(a), y = std::bit_cast<int32_t>(b);
    return std::bit_cast<float>((x < 0 && y < 0) ? std::min(x, y) : std::max(x, y));
}
// orig: (helper) VU MINI semantics
inline float vu_min(float a, float b) {
    const int32_t x = std::bit_cast<int32_t>(a), y = std::bit_cast<int32_t>(b);
    return std::bit_cast<float>((x < 0 && y < 0) ? std::max(x, y) : std::min(x, y));
}

// VU0 micro program body shared by 0x190/0x192 and the Euler program's
// subroutine at 0x1c4: fold x into [-pi/2, pi/2] with MINI/MAX against
// (pi - x) and (-pi - x), then the degree-9 odd polynomial.
// orig: 0x001ff800 (VU0 micro 0x192 / subroutine 0x1c4)
float vu_sin_kernel(float x) {
    constexpr float VPI = f32(0x40490fda), VNPI = f32(0xc0490fda);
    const float pi = 0.0f + VPI, npi = 0.0f + VNPI;
    x = vu_max(vu_min(x, vuclamp(pi - x)), vuclamp(npi - x));
    // (each VU result clamps to +-max: a huge unfolded angle gives +-max, never inf or NaN)
    const float x2 = vuclamp(x * x), x3 = vuclamp(x * x2), x5 = vuclamp(x3 * x2), x7 = vuclamp(x5 * x2), x9 = vuclamp(x7 * x2);
    float acc = vuclamp(x * 1.0f);
    acc = vuclamp(acc + vuclamp(x3 * f32(0xbe2aaaa4)));
    acc = vuclamp(acc + vuclamp(x5 * f32(0x3c08873e)));
    acc = vuclamp(acc + vuclamp(x7 * f32(0xb94fb21d)));
    return vuclamp(acc + vuclamp(x9 * f32(0x362e9c14)));
}

constexpr float VU_HALF_PI = f32(0x3fc90fda);

// Subroutine 0x1c4 of the Euler program: sin(x) and sin(x + pi/2).
// orig: 0x001ffa50 (VU0 micro subroutine 0x1c4)
void vu_sincos(float x, float& s, float& c) {
    c = vu_sin_kernel(x + VU_HALF_PI);
    s = vu_sin_kernel(x);
}

// Subroutine 0x1e5: rows r = R0 * r.x + R1 * r.y + R2 * r.z (all four lanes).
// orig: 0x001ffa50 (VU0 micro subroutine 0x1e5)
inline Vec4 rot_row(const Vec4& r, const Vec4& a, const Vec4& b, const Vec4& c) {
    auto lane = [](float ar, float br, float cr, float x, float y, float z) {
        return vuclamp(vuclamp(vuclamp(ar * x) + vuclamp(br * y)) + vuclamp(cr * z));
    };
    return Vec4{lane(a.x, b.x, c.x, r.x, r.y, r.z), lane(a.y, b.y, c.y, r.x, r.y, r.z),
                lane(a.z, b.z, c.z, r.x, r.y, r.z), lane(a.w, b.w, c.w, r.x, r.y, r.z)};
}

// VU0 micro program 0x1a3. Angles equal to +-0 are skipped (the program reads
// the MAC zero flags of `vf31 = angles` with FMOR).
// orig: 0x001ffa50 (VU0 micro program 0x1a3)
void euler_vu(const Vec4& ang, Vec4& r0, Vec4& r1, Vec4& r2) {
    r0 = Vec4{1.0f, 0.0f, 0.0f, 0.0f};
    r1 = Vec4{0.0f, 1.0f, 0.0f, 0.0f};
    r2 = Vec4{0.0f, 0.0f, 1.0f, 0.0f};
    const Vec4 a{ang.x + 0.0f, ang.y + 0.0f, ang.z + 0.0f, 0.0f};
    float s, c;
    if (a.x != 0.0f) {  // X: overwrites rows 1 and 2
        vu_sincos(a.x, s, c);
        r1.y = 0.0f + c; r1.z = 0.0f + s;
        r2.z = 0.0f + c; r2.y = 0.0f - s;
    }
    if (a.y != 0.0f) {
        vu_sincos(0.0f + a.y, s, c);
        const Vec4 m0{0.0f + c, 0.0f, 0.0f - s, 0.0f}, m1{0.0f, 0.0f + 1.0f, 0.0f, 0.0f}, m2{0.0f + s, 0.0f, 0.0f + c, 0.0f};
        r0 = rot_row(r0, m0, m1, m2); r1 = rot_row(r1, m0, m1, m2); r2 = rot_row(r2, m0, m1, m2);
    }
    if (a.z != 0.0f) {
        vu_sincos(0.0f + a.z, s, c);
        const Vec4 m0{0.0f + c, 0.0f + s, 0.0f, 0.0f}, m1{0.0f - s, 0.0f + c, 0.0f, 0.0f}, m2{0.0f, 0.0f, 1.0f, 0.0f};
        r0 = rot_row(r0, m0, m1, m2); r1 = rot_row(r1, m0, m1, m2); r2 = rot_row(r2, m0, m1, m2);
    }
}

// EE-FPU sine used by 0x001ffa90 (note: the x^7 coefficient differs from the VU one).
// orig: 0x001ffa90 (inline FPU sine)
float fpu_sin_kernel(float x) {
    if (!(x < HALF_PI)) x = PI - x;
    else if (x < -HALF_PI) x = -PI - x;
    const float x2 = x * x, x3 = x * x2, x5 = x3 * x2, x7 = x5 * x2, x9 = x7 * x2;
    float acc = x + 0.0f;
    acc = acc + x3 * f32(0xbe2aaaa4);
    acc = acc + x5 * f32(0x3c08873e);
    acc = acc + x7 * f32(0xb94fb21f);
    return acc + x9 * f32(0x362e9c14);
}

}  // namespace

float cos_vu(float a) { return vu_sin_kernel(a + VU_HALF_PI); }
float sin_vu(float a) { return vu_sin_kernel(a); }

namespace {
// (host) RAC1_PERF_EULER_CACHE (default off): euler_vu is a function of the three angles' bits alone (its constants, nothing else
// read; a thread's rounding mode does not change once set): its rows kept per angles in a small table per thread, a moby whose
// rotation did not change (most of them, every frame) found there - the same rows.
struct EulerCache {
    static constexpr uint32_t kN = 512;
    uint32_t key[kN][3];
    Vec4 rows[kN][3];
    bool used[kN];
};
bool euler_cache_on() {
    static const bool on = std::getenv("RAC1_PERF_EULER_CACHE") != nullptr && std::atoi(std::getenv("RAC1_PERF_EULER_CACHE")) != 0;
    return on;
}
EulerCache& euler_cache() {
    static thread_local EulerCache c{};
    return c;
}
}  // namespace

// orig: 0x001ffa50
void euler_to_mat3_vu(Mat4& d, const Vec4& angles) {
    Vec4 r0, r1, r2;
    if (euler_cache_on()) {
        const uint32_t ax = std::bit_cast<uint32_t>(angles.x), ay = std::bit_cast<uint32_t>(angles.y), az = std::bit_cast<uint32_t>(angles.z);
        const uint32_t i = ((ax * 0x9e3779b1u) ^ (ay * 0x85ebca77u) ^ (az * 0xc2b2ae3du)) >> 23;  // (0 .. 511)
        EulerCache& c = euler_cache();
        if (c.used[i] && c.key[i][0] == ax && c.key[i][1] == ay && c.key[i][2] == az) {
            d.r[0] = c.rows[i][0]; d.r[1] = c.rows[i][1]; d.r[2] = c.rows[i][2];
            return;
        }
        euler_vu(angles, r0, r1, r2);
        c.used[i] = true;
        c.key[i][0] = ax, c.key[i][1] = ay, c.key[i][2] = az;
        c.rows[i][0] = r0, c.rows[i][1] = r1, c.rows[i][2] = r2;
    } else {
        euler_vu(angles, r0, r1, r2);
    }
    d.r[0] = r0; d.r[1] = r1; d.r[2] = r2;
}

// orig: 0x001ffa70
void euler_to_mat4_vu(Mat4& d, const Vec4& angles) {
    euler_to_mat3_vu(d, angles);
    d.r[3] = Vec4{0.0f, 0.0f, 0.0f, 1.0f};
}

// orig: 0x001ffa90
void euler_to_mat4_fpu(Mat4& d, const Vec4& ang) {
    Vec4 r0{1.0f, 0.0f, 0.0f, 0.0f}, r1{0.0f, 1.0f, 0.0f, 0.0f}, r2{0.0f, 0.0f, 1.0f, 0.0f};
    const Vec4 r3{0.0f, 0.0f, 0.0f, 1.0f};
    auto sincos = [](float a, float& s, float& c) {
        const float a2 = a + HALF_PI;  // computed before folding
        s = fpu_sin_kernel(a);
        c = fpu_sin_kernel(a2);
    };
    float s, c;
    if (std::bit_cast<uint32_t>(ang.z) != 0) {  // Z: written directly into rows 0 and 1
        sincos(ang.z, s, c);
        r0.x = 0.0f + c; r0.y = 0.0f + s;
        r1.y = 0.0f + c; r1.x = 0.0f - s;
    }
    if (std::bit_cast<uint32_t>(ang.y) != 0) {
        sincos(ang.y, s, c);
        const Vec4 m0{0.0f + c, 0.0f, 0.0f - s, 0.0f}, m1{0.0f, 0.0f + 1.0f, 0.0f, 0.0f}, m2{0.0f + s, 0.0f, 0.0f + c, 0.0f};
        r0 = rot_row(r0, m0, m1, m2); r1 = rot_row(r1, m0, m1, m2); r2 = rot_row(r2, m0, m1, m2);
    }
    if (std::bit_cast<uint32_t>(ang.x) != 0) {
        sincos(ang.x, s, c);
        const Vec4 m0{0.0f + 1.0f, 0.0f, 0.0f, 0.0f}, m1{0.0f, 0.0f + c, 0.0f + s, 0.0f}, m2{0.0f, 0.0f - s, 0.0f + c, 0.0f};
        r0 = rot_row(r0, m0, m1, m2); r1 = rot_row(r1, m0, m1, m2); r2 = rot_row(r2, m0, m1, m2);
    }
    d.r[0] = r0; d.r[1] = r1; d.r[2] = r2; d.r[3] = r3;
}

// Half-angle quaternion for a rotation about one axis:
// w = 0.5 * sqrt(2 + 2|cos a|) style, built in two branches on the sign of cos a.
// orig: 0x001fff28
void quat_from_axis_angle(Vec4& d, int32_t axis, float angle) {
    const float s = sin_vu(angle);
    const float c = cos_vu(angle);
    const float neg_s = -s;
    const float two_c = c + c;
    const float root = std::sqrt(std::fabs(std::fabs(two_c) + 2.0f));
    float x, w;
    if (std::signbit(two_c)) {
        x = 0.5f * root;
        w = 0.0f + vu_div(neg_s, 0.0f + root);
    } else {
        w = 0.5f * root;
        x = 0.0f + vu_div(neg_s, 0.0f + root);
    }
    if (axis == 0) d = Vec4{x, 0.0f, 0.0f, w};
    else if (axis == 1) d = Vec4{0.0f + 0.0f, 0.0f + x, 0.0f, w};
    else d = Vec4{0.0f + 0.0f, 0.0f, 0.0f + x, w};
}

// orig: 0x001ff330 — vmulx.xyzw: every lane times s.
void vec4_scale(Vec4& d, const Vec4& a, float s) { d = Vec4{vuclamp(a.x * s), vuclamp(a.y * s), vuclamp(a.z * s), vuclamp(a.w * s)}; }

// orig: 0x001ff348 — vmul.xyz: lane by lane, w stays a's (the destination register was loaded from a).
void vec3_mul(Vec4& d, const Vec4& a, const Vec4& b) {
    d = Vec4{vuclamp(vuclamp(a.x) * vuclamp(b.x)), vuclamp(vuclamp(a.y) * vuclamp(b.y)), vuclamp(vuclamp(a.z) * vuclamp(b.z)), a.w};
}

// orig: 0x001ff360 — vmul.xyzw: lane by lane.
void vec4_mul(Vec4& d, const Vec4& a, const Vec4& b) {
    d = Vec4{vuclamp(vuclamp(a.x) * vuclamp(b.x)), vuclamp(vuclamp(a.y) * vuclamp(b.y)), vuclamp(vuclamp(a.z) * vuclamp(b.z)),
             vuclamp(vuclamp(a.w) * vuclamp(b.w))};
}

// orig: 0x001ff1b0 — vclipw.xyz of p against its own |w|: the six outside bits (+x -x +y -y +z -z).
int32_t clip_outcode(const Vec4& p) {
    uint32_t u[4];
    std::memcpy(u, &p, 16);
    const int32_t lim = (u[3] & 0x7f800000u) != 0 ? static_cast<int32_t>(u[3] & 0x7fffffffu) : 0x007fffff;
    int32_t c = 0;
    for (int i = 0; i < 3; ++i) {
        if (static_cast<int32_t>(u[i]) > lim) c |= 1 << (2 * i);
        if (static_cast<int32_t>(u[i] ^ 0x80000000u) > lim) c |= 2 << (2 * i);
    }
    return c;
}

// orig: 0x00200178 — wrap an angle into [-pi, pi) by whole turns.
float angle_wrap(float a) {
    float f = a;
    if (!(f < PI)) {
        do {
            f = f - PI;
            f = f - PI;
        } while (!(f < PI));
    }
    if (f < -PI) {
        do {
            f = f + PI;
            f = f + PI;
        } while (f < -PI);
    }
    return f;
}

// orig: 0x001ffea8 — normalised lerp of two quaternions (b negated when the dot is negative).
void quat_nlerp(Vec4& d, const Vec4& a, const Vec4& b, float t) {
    const float s = 1.0f - t;
    const Vec4 p{a.x * s, a.y * s, a.z * s, a.w * s};
    const Vec4 q{b.x * t, b.y * t, b.z * t, b.w * t};
    Vec4 r{p.x + q.x, p.y + q.y, p.z + q.z, p.w + q.w};
    const float dot = ((p.y * q.y + p.x * q.x) + 1.0f * (p.z * q.z)) + 1.0f * (p.w * q.w);
    if (std::signbit(dot)) r = Vec4{p.x - q.x, p.y - q.y, p.z - q.z, p.w - q.w};
    const float l2 = ((r.x * r.x + r.y * r.y) + 1.0f * (r.z * r.z)) + 1.0f * (r.w * r.w);
    const float k = vu_div(1.0f, std::sqrt(std::fabs(l2)));  // vrsqrt
    d = Vec4{r.x * k, r.y * k, r.z * k, r.w * k};
}

namespace {
// orig: (helper) the rotation rows of 0x1fffe8 / 0x200060 from a unit quaternion
void quat_rows(const Vec4& q, Vec4& r0, Vec4& r1, Vec4& r2) {
    const Vec4 t{q.x + q.x, q.y + q.y, q.z + q.z, q.w + q.w};
    const float xw = t.x * q.w, yw = t.y * q.w, zw = t.z * q.w;
    const float yx = t.y * q.x, zx = t.z * q.x, xx = t.x * q.x;
    const float yy = t.y * q.y, zy = t.z * q.y, zz = t.z * q.z;
    r0 = Vec4{(1.0f - yy) - zz, yx - zw, zx + yw, 0.0f};
    r1 = Vec4{(0.0f + zw) + yx, (1.0f - xx) - zz, zy - xw, 0.0f};
    r2 = Vec4{(0.0f - yw) + zx, (0.0f + xw) + zy, (1.0f - xx) - yy, 0.0f};
}
}  // namespace

// orig: 0x001fffe8 — quaternion to rotation rows 0..2.
void quat_to_mat3(const Vec4& q, Mat4& m) {
    Vec4 r0, r1, r2;
    quat_rows(q, r0, r1, r2);
    m.r[0] = r0; m.r[1] = r1; m.r[2] = r2;
}

// orig: 0x00200060 — quaternion to a 4x4 rotation (row 3 = 0, 0, 0, 1).
void quat_to_mat4(const Vec4& q, Mat4& m) {
    quat_to_mat3(q, m);
    m.r[3] = Vec4{0.0f, 0.0f, 0.0f, 1.0f};
}

// orig: 0x001ff2d8 — a + (b - a) * t on xyz (w of a).
void vec3_lerp(Vec4& d, const Vec4& a, const Vec4& b, float t) {
    const Vec4 r{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w};
    d = r;
}

}  // namespace rac1
