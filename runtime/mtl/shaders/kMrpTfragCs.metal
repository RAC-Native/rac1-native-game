// kMrpTfragCs (comp): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
// rac1: local size 64 1 1
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>
#include <metal_atomic>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

// Implementation of the unsigned GLSL findMSB() function
template<typename T>
inline T spvFindUMSB(T x)
{
    return select(clz(T(0)) - (clz(x) + T(1)), T(-1), x == T(0));
}

template<typename T>
[[clang::optnone]] T spvFMul(T l, T r)
{
    return fma(l, r, T(0));
}

template<typename T, int Cols, int Rows>
[[clang::optnone]] vec<T, Cols> spvFMulVectorMatrix(vec<T, Rows> v, matrix<T, Cols, Rows> m)
{
    vec<T, Cols> res = vec<T, Cols>(0);
    for (uint i = Rows; i > 0; --i)
    {
        vec<T, Cols> tmp(0);
        for (uint j = 0; j < Cols; ++j)
        {
            tmp[j] = m[j][i - 1];
        }
        res = fma(tmp, vec<T, Cols>(v[i - 1]), res);
    }
    return res;
}

template<typename T, int Cols, int Rows>
[[clang::optnone]] vec<T, Rows> spvFMulMatrixVector(matrix<T, Cols, Rows> m, vec<T, Cols> v)
{
    vec<T, Rows> res = vec<T, Rows>(0);
    for (uint i = Cols; i > 0; --i)
    {
        res = fma(m[i - 1], vec<T, Rows>(v[i - 1]), res);
    }
    return res;
}

template<typename T, int LCols, int LRows, int RCols, int RRows>
[[clang::optnone]] matrix<T, RCols, LRows> spvFMulMatrixMatrix(matrix<T, LCols, LRows> l, matrix<T, RCols, RRows> r)
{
    static_assert(LCols == RRows, "column-row configuration mismatch");
    matrix<T, RCols, LRows> res;
    for (uint i = 0; i < RCols; i++)
    {
        vec<T, LRows> tmp(0);
        for (uint j = 0; j < LCols; j++)
        {
            tmp = fma(vec<T, LRows>(r[i][j]), l[j], tmp);
        }
        res[i] = tmp;
    }
    return res;
}

template<typename T>
[[clang::optnone]] T spvFAdd(T l, T r)
{
    return fma(T(1), l, r);
}

template<typename T>
[[clang::optnone]] T spvFSub(T l, T r)
{
    return fma(T(-1), r, l);
}

struct Draw
{
    uint image;
    uint nq;
    uint plan;
    uint colour;
    uint col_addr;
    uint col_count;
    uint view;
    uint consts;
    uint vbase;
    uint ibase;
    uint rbase;
    uint flags;
    uint wview;
    uint p1;
    uint p2;
    uint p3;
};

struct Draw_1
{
    uint image;
    uint nq;
    uint plan;
    uint colour;
    uint col_addr;
    uint col_count;
    uint view;
    uint consts;
    uint vbase;
    uint ibase;
    uint rbase;
    uint flags;
    uint wview;
    uint p1;
    uint p2;
    uint p3;
};

struct Draws
{
    Draw_1 draws[1];
};

struct Images
{
    uint4 images[1];
};

struct Consts
{
    uint4 consts[1];
};

struct VuViews
{
    float vuviews[1];
};

struct Cols
{
    uint cols[1];
};

struct Plans
{
    uint plans[1];
};

struct Views
{
    ulong views[1];
};

struct V
{
    float wp0;
    float wp1;
    float wp2;
    float wp3;
    float vu0;
    float vu1;
    float vu2;
    float vu3;
    float s;
    float t;
    float q;
    float lodq;
    uint rgba;
    float fog;
    uint word;
    uint layer;
    uint st0;
    uint st1;
};

struct Runs
{
    uint4 runs[1];
};

struct V_1
{
    float wp0;
    float wp1;
    float wp2;
    float wp3;
    float vu0;
    float vu1;
    float vu2;
    float vu3;
    float s;
    float t;
    float q;
    float lodq;
    uint rgba;
    float fog;
    uint word;
    uint layer;
    uint st0;
    uint st1;
};

struct OutV
{
    V_1 outv[1];
};

struct OutI
{
    uint outi[1];
};

static inline __attribute__((always_inline))
uint4 at(thread const uint& a, threadgroup spvUnsafeArray<uint4, 328>& vu)
{
    uint4 _921;
    if (a < 328u)
    {
        _921 = vu[a];
    }
    else
    {
        _921 = uint4(0u);
    }
    return _921;
}

static inline __attribute__((always_inline))
float4 itof0(thread const uint& a, threadgroup spvUnsafeArray<uint4, 328>& vu)
{
    uint param = a;
    return float4(int4(at(param, vu)));
}

static inline __attribute__((always_inline))
float4 ld(thread const uint& a, threadgroup spvUnsafeArray<uint4, 328>& vu)
{
    uint param = a;
    return as_type<float4>(at(param, vu));
}

static inline __attribute__((always_inline))
float ftz(thread const float& x)
{
    uint u = as_type<uint>(x);
    float _147;
    if ((u & 2139095040u) == 0u)
    {
        _147 = as_type<float>(u & 2147483648u);
    }
    else
    {
        _147 = x;
    }
    return _147;
}

static inline __attribute__((always_inline))
float chop(thread float& s, thread const float& e)
{
    bool _164 = (e == 0.0) || (s == 0.0);
    bool _170;
    if (!_164)
    {
        _170 = isinf(s);
    }
    else
    {
        _170 = _164;
    }
    bool _176;
    if (!_170)
    {
        _176 = isnan(s);
    }
    else
    {
        _176 = _170;
    }
    if (_176)
    {
        float param = s;
        return ftz(param);
    }
    if ((e < 0.0) != (s < 0.0))
    {
        s = as_type<float>(as_type<uint>(s) - 1u);
    }
    float param_1 = s;
    return ftz(param_1);
}

static inline __attribute__((always_inline))
float mulz(thread const float& a, thread const float& b)
{
    float param = a;
    float x = ftz(param);
    float param_1 = b;
    float y = ftz(param_1);
    float p = spvFMul(x, y);
    float e = fma(x, y, -p);
    float param_2 = p;
    float param_3 = e;
    float _263 = chop(param_2, param_3);
    return _263;
}

static inline __attribute__((always_inline))
float4 mul4s(thread const float4& a, thread const float& b)
{
    float param = a.x;
    float param_1 = b;
    float param_2 = a.y;
    float param_3 = b;
    float param_4 = a.z;
    float param_5 = b;
    float param_6 = a.w;
    float param_7 = b;
    return float4(mulz(param, param_1), mulz(param_2, param_3), mulz(param_4, param_5), mulz(param_6, param_7));
}

static inline __attribute__((always_inline))
float addz(thread const float& a, thread const float& b)
{
    float param = a;
    float x = ftz(param);
    float param_1 = b;
    float y = ftz(param_1);
    float s = spvFAdd(x, y);
    float bb = spvFSub(s, x);
    float e = spvFAdd(spvFSub(x, spvFSub(s, bb)), spvFSub(y, bb));
    float param_2 = s;
    float param_3 = e;
    float _230 = chop(param_2, param_3);
    return _230;
}

static inline __attribute__((always_inline))
float4 add4(thread const float4& a, thread const float4& b)
{
    float param = a.x;
    float param_1 = b.x;
    float param_2 = a.y;
    float param_3 = b.y;
    float param_4 = a.z;
    float param_5 = b.z;
    float param_6 = a.w;
    float param_7 = b.w;
    return float4(addz(param, param_1), addz(param_2, param_3), addz(param_4, param_5), addz(param_6, param_7));
}

static inline __attribute__((always_inline))
float4 xform(thread const float4& p, threadgroup spvUnsafeArray<uint4, 328>& vu)
{
    uint param = 8u;
    float4 a = ld(param, vu);
    uint param_1 = 5u;
    float4 param_2 = ld(param_1, vu);
    float param_3 = p.x;
    float4 param_4 = a;
    float4 param_5 = mul4s(param_2, param_3);
    a = add4(param_4, param_5);
    uint param_6 = 6u;
    float4 param_7 = ld(param_6, vu);
    float param_8 = p.y;
    float4 param_9 = a;
    float4 param_10 = mul4s(param_7, param_8);
    a = add4(param_9, param_10);
    uint param_11 = 7u;
    float4 param_12 = ld(param_11, vu);
    float param_13 = p.z;
    float4 param_14 = a;
    float4 param_15 = mul4s(param_12, param_13);
    return add4(param_14, param_15);
}

static inline __attribute__((always_inline))
void st(thread const uint& a, thread const float4& v, threadgroup spvUnsafeArray<uint4, 328>& vu)
{
    if (a < 328u)
    {
        vu[a] = as_type<uint4>(v);
    }
}

static inline __attribute__((always_inline))
float kf(thread const uint& c, thread const uint& l, threadgroup spvUnsafeArray<uint4, 14>& kc)
{
    return as_type<float>(((threadgroup uint*)&kc[c - 656u])[l]);
}

static inline __attribute__((always_inline))
uint u16(thread const uint& a, thread const uint& l, threadgroup spvUnsafeArray<uint4, 328>& vu)
{
    uint param = a;
    return at(param, vu)[l] & 65535u;
}

static inline __attribute__((always_inline))
float fmin_(thread const float& a, thread const float& b)
{
    return (b < a) ? b : a;
}

static inline __attribute__((always_inline))
float fmax_(thread const float& a, thread const float& b)
{
    return (a < b) ? b : a;
}

static inline __attribute__((always_inline))
float subz(thread const float& a, thread const float& b)
{
    float param = a;
    float param_1 = -b;
    return addz(param, param_1);
}

static inline __attribute__((always_inline))
float fa(thread const uint& a, thread const uint& l, threadgroup spvUnsafeArray<uint4, 328>& vu)
{
    uint param = a;
    return as_type<float>(at(param, vu)[l]);
}

static inline __attribute__((always_inline))
void step_iter(thread const uint& kind, thread const uint& band, thread const uint& a, thread const uint& b, thread const uint& k, threadgroup spvUnsafeArray<uint4, 328>& vu, threadgroup spvUnsafeArray<uint4, 14>& kc)
{
    if (kind == 0u)
    {
        uint ad = a + (2u * k);
        uint param = ad;
        float4 param_1 = itof0(param, vu);
        float4 p = xform(param_1, vu);
        uint param_2 = ad + 1u;
        float4 c = itof0(param_2, vu);
        uint param_3 = ad;
        float4 param_4 = p;
        st(param_3, param_4, vu);
        uint param_5 = ad + 1u;
        float4 param_6 = c;
        st(param_5, param_6, vu);
    }
    else
    {
        if (kind <= 2u)
        {
            uint param_7 = band;
            uint param_8 = 0u;
            float kx = kf(param_7, param_8, kc);
            uint param_9 = band;
            uint param_10 = 1u;
            float ky = kf(param_9, param_10, kc);
            uint param_11 = band + 2u;
            uint param_12 = 0u;
            float cx = kf(param_11, param_12, kc);
            uint param_13 = band + 2u;
            uint param_14 = 1u;
            float cy = kf(param_13, param_14, kc);
            uint param_15 = band;
            uint param_16 = 3u;
            float thr = kf(param_15, param_16, kc);
            uint param_17 = 657u;
            uint param_18 = 0u;
            float mx = kf(param_17, param_18, kc);
            uint param_19 = 657u;
            uint param_20 = 1u;
            float my = kf(param_19, param_20, kc);
            uint via = a + k;
            uint param_21 = via;
            uint param_22 = 3u;
            uint rec = u16(param_21, param_22, vu);
            uint param_23 = b + (k / 4u);
            uint pvi = at(param_23, vu)[k % 4u] & 65535u;
            uint param_24 = pvi;
            uint param_25 = 3u;
            uint r1 = u16(param_24, param_25, vu);
            uint param_26 = via;
            uint param_27 = 2u;
            uint r2 = u16(param_26, param_27, vu);
            uint param_28 = rec;
            float4 param_29 = itof0(param_28, vu);
            float4 p_1 = xform(param_29, vu);
            uint param_30 = rec + 1u;
            float4 c_1 = itof0(param_30, vu);
            uint param_31 = r1;
            float4 P1 = ld(param_31, vu);
            uint param_32 = r1 + 1u;
            float4 C1 = ld(param_32, vu);
            uint param_33 = r2;
            float4 P2 = ld(param_33, vu);
            uint param_34 = r2 + 1u;
            float4 C2 = ld(param_34, vu);
            float param_35 = cx;
            float param_36 = 1.0;
            float param_37 = kx;
            float param_38 = p_1.w;
            float param_39 = mulz(param_35, param_36);
            float param_40 = mulz(param_37, param_38);
            float fx = addz(param_39, param_40);
            float param_41 = cy;
            float param_42 = 1.0;
            float param_43 = ky;
            float param_44 = p_1.w;
            float param_45 = mulz(param_41, param_42);
            float param_46 = mulz(param_43, param_44);
            float fy = addz(param_45, param_46);
            float param_47 = fx;
            float param_48 = mx;
            float param_49 = fmin_(param_47, param_48);
            float param_50 = 0.0;
            fx = fmax_(param_49, param_50);
            float param_51 = fy;
            float param_52 = my;
            float param_53 = fmin_(param_51, param_52);
            float param_54 = 0.0;
            fy = fmax_(param_53, param_54);
            float4 param_55 = P1;
            float4 param_56 = P2;
            float4 S = add4(param_55, param_56);
            float4 param_57 = C1;
            float4 param_58 = C2;
            float4 SC = add4(param_57, param_58);
            float4 param_59 = S;
            float param_60 = fx;
            float4 param_61 = p_1;
            float param_62 = fy;
            float4 param_63 = mul4s(param_59, param_60);
            float4 param_64 = mul4s(param_61, param_62);
            float4 po = add4(param_63, param_64);
            float4 param_65 = SC;
            float param_66 = fx;
            float4 param_67 = c_1;
            float param_68 = fy;
            float4 param_69 = mul4s(param_65, param_66);
            float4 param_70 = mul4s(param_67, param_68);
            float4 co = add4(param_69, param_70);
            float param_71 = P1.w;
            float param_72 = thr;
            float d1 = subz(param_71, param_72);
            float param_73 = P2.w;
            float param_74 = thr;
            float d2 = subz(param_73, param_74);
            if (((kind == 2u) && (d1 < 0.0)) && (d2 < 0.0))
            {
                uint param_75 = pvi;
                vu[via] = at(param_75, vu);
                return;
            }
            uint param_76 = rec;
            float4 param_77 = po;
            st(param_76, param_77, vu);
            uint param_78 = rec + 1u;
            float4 param_79 = co;
            st(param_78, param_79, vu);
        }
        else
        {
            uint param_80 = band;
            uint param_81 = 3u;
            float thr_1 = kf(param_80, param_81, kc);
            uint la = a + k;
            uint param_82 = b + (k / 4u);
            uint vi = at(param_82, vu)[k % 4u] & 65535u;
            uint param_83 = la;
            uint param_84 = 2u;
            uint ra = u16(param_83, param_84, vu);
            uint param_85 = vi;
            uint param_86 = 3u;
            uint rb = u16(param_85, param_86, vu);
            uint param_87 = ra;
            uint param_88 = 3u;
            float param_89 = fa(param_87, param_88, vu);
            float param_90 = thr_1;
            float d1_1 = subz(param_89, param_90);
            uint param_91 = rb;
            uint param_92 = 3u;
            float param_93 = fa(param_91, param_92, vu);
            float param_94 = thr_1;
            float d2_1 = subz(param_93, param_94);
            if ((d1_1 < 0.0) && (d2_1 < 0.0))
            {
                uint param_95 = vi;
                vu[la] = at(param_95, vu);
            }
        }
    }
}

static inline __attribute__((always_inline))
float4 mul4(thread const float4& a, thread const float4& b)
{
    float param = a.x;
    float param_1 = b.x;
    float param_2 = a.y;
    float param_3 = b.y;
    float param_4 = a.z;
    float param_5 = b.z;
    float param_6 = a.w;
    float param_7 = b.w;
    return float4(mulz(param, param_1), mulz(param_2, param_3), mulz(param_4, param_5), mulz(param_6, param_7));
}

static inline __attribute__((always_inline))
float divz(thread float& a, thread float& b)
{
    float param = a;
    a = ftz(param);
    float param_1 = b;
    b = ftz(param_1);
    float q = a / b;
    bool _368 = isinf(q);
    bool _374;
    if (!_368)
    {
        _374 = isnan(q);
    }
    else
    {
        _374 = _368;
    }
    if (_374 || (q == 0.0))
    {
        float param_2 = q;
        return ftz(param_2);
    }
    float r = fma(-q, b, a);
    float q2 = fma(r, 1.0 / b, q);
    float best = q2;
    float br = abs(fma(-q2, b, a));
    float qn = as_type<float>(as_type<uint>(q2) + 1u);
    float qp = as_type<float>(as_type<uint>(q2) - 1u);
    float rn = abs(fma(-qn, b, a));
    float rp = abs(fma(-qp, b, a));
    if (rn < br)
    {
        best = qn;
        br = rn;
    }
    if (rp < br)
    {
        best = qp;
    }
    float res = fma(-best, b, a);
    bool _450 = res != 0.0;
    bool _458;
    if (_450)
    {
        _458 = (res < 0.0) != (a < 0.0);
    }
    else
    {
        _458 = _450;
    }
    if (_458)
    {
        best = as_type<float>(as_type<uint>(best) - 1u);
    }
    float param_3 = best;
    return ftz(param_3);
}

static inline __attribute__((always_inline))
int ftoi(thread const float& v)
{
    if (isnan(v))
    {
        return 2147483647;
    }
    if (!(v < 2147483648.0))
    {
        return 2147483647;
    }
    if (!(v > (-2147483648.0)))
    {
        return int(0x80000000);
    }
    return int(v);
}

static inline __attribute__((always_inline))
ulong f2d(thread const float& f)
{
    uint u = as_type<uint>(f);
    ulong _sign = ulong(u & 2147483648u) << ulong(32);
    int e = int((u >> uint(23)) & 255u);
    uint m = u & 8388607u;
    if (e == 0)
    {
        if (m == 0u)
        {
            return _sign;
        }
        int lz = 23 - int(spvFindUMSB(m));
        m = (m << uint(lz)) & 8388607u;
        e = 1 - lz;
    }
    else
    {
        if (e == 255)
        {
            return (_sign | 9218868437227405312ul) | (ulong(m) << ulong(29));
        }
    }
    return (_sign | (ulong(long((e - 127) + 1023)) << ulong(52))) | (ulong(m) << ulong(29));
}

static inline __attribute__((always_inline))
ulong d_pack(thread const ulong& _sign, thread const int& e, thread const ulong& mant)
{
    if (e <= 0)
    {
        return _sign;
    }
    if (e >= 2047)
    {
        return _sign | 576179277326712831ul;
    }
    return (_sign | (ulong(long(e)) << ulong(52))) | (mant & 4503599627370495ul);
}

static inline __attribute__((always_inline))
ulong dmulz(thread const ulong& a, thread const ulong& b)
{
    ulong _sign = (a ^ b) & 9223372036854775808ul;
    int ea = int(uint((a >> ulong(52)) & 2047ul));
    int eb = int(uint((b >> ulong(52)) & 2047ul));
    if ((ea == 0) || (eb == 0))
    {
        return _sign;
    }
    if ((ea == 2047) || (eb == 2047))
    {
        return _sign | 9218868437227405312ul;
    }
    ulong ma = (a & 4503599627370495ul) | 4503599627370496ul;
    ulong mb = (b & 4503599627370495ul) | 4503599627370496ul;
    ulong al = ma & 4294967295ul;
    ulong ah = ma >> ulong(32);
    ulong bl = mb & 4294967295ul;
    ulong bh = mb >> ulong(32);
    ulong ll = al * bl;
    ulong lh = al * bh;
    ulong hl = ah * bl;
    ulong hh = ah * bh;
    ulong mid = ((ll >> ulong(32)) + (lh & 4294967295ul)) + (hl & 4294967295ul);
    ulong lo = (ll & 4294967295ul) | (mid << ulong(32));
    ulong hi = ((hh + (lh >> ulong(32))) + (hl >> ulong(32))) + (mid >> ulong(32));
    int e = (ea + eb) - 1023;
    ulong mant;
    if ((hi & 2199023255552ul) != 0ul)
    {
        mant = (hi << ulong(11)) | (lo >> ulong(53));
        e++;
    }
    else
    {
        mant = (hi << ulong(12)) | (lo >> ulong(52));
    }
    ulong param = _sign;
    int param_1 = e;
    ulong param_2 = mant;
    return d_pack(param, param_1, param_2);
}

static inline __attribute__((always_inline))
ulong shr_jam(thread const ulong& a, thread const int& n)
{
    if (n <= 0)
    {
        return a;
    }
    if (n >= 63)
    {
        return ulong(a != 0ul);
    }
    return (a >> ulong(n)) | ulong((a << ulong(64 - n)) != 0ul);
}

static inline __attribute__((always_inline))
ulong daddz(thread ulong& a, thread ulong& b)
{
    if ((a & 9223372036854775807ul) < (b & 9223372036854775807ul))
    {
        ulong t = a;
        a = b;
        b = t;
    }
    int ea = int(uint((a >> ulong(52)) & 2047ul));
    int eb = int(uint((b >> ulong(52)) & 2047ul));
    if (eb == 0)
    {
        ulong _771;
        if (ea == 0)
        {
            _771 = (a & b) & 9223372036854775808ul;
        }
        else
        {
            _771 = a;
        }
        return _771;
    }
    if (ea == 2047)
    {
        return a;
    }
    ulong _sign = a & 9223372036854775808ul;
    ulong ma = ((a & 4503599627370495ul) | 4503599627370496ul) << ulong(10);
    ulong param = ((b & 4503599627370495ul) | 4503599627370496ul) << ulong(10);
    int param_1 = ea - eb;
    ulong mb = shr_jam(param, param_1);
    int e = ea;
    ulong m;
    if (((a ^ b) & 9223372036854775808ul) == 0ul)
    {
        m = ma + mb;
        if ((m & 9223372036854775808ul) != 0ul)
        {
            ulong param_2 = m;
            int param_3 = 1;
            m = shr_jam(param_2, param_3);
            e++;
        }
    }
    else
    {
        m = ma - mb;
        if (m == 0ul)
        {
            return 0ul;
        }
        while ((m & 4611686018427387904ul) == 0ul)
        {
            m = m << ulong(1);
            e--;
        }
    }
    ulong param_4 = _sign;
    int param_5 = e;
    ulong param_6 = m >> ulong(10);
    return d_pack(param_4, param_5, param_6);
}

static inline __attribute__((always_inline))
float d2fz(thread const ulong& d)
{
    uint hi = uint(d >> ulong(32));
    uint lo = uint(d & 4294967295ul);
    uint _sign = hi & 2147483648u;
    int e = (int((hi >> uint(20)) & 2047u) - 1023) + 127;
    if (e <= 0)
    {
        return as_type<float>(_sign);
    }
    if (e >= 255)
    {
        return as_type<float>(_sign | 2139095039u);
    }
    return as_type<float>(((_sign | (uint(e) << uint(23))) | ((hi & 1048575u) << uint(3))) | (lo >> uint(29)));
}

kernel void kMrpTfragCs(device Draws& _1397 [[buffer(0)]], device Images& _1432 [[buffer(1)]], device Consts& _1454 [[buffer(2)]], device VuViews& _1495 [[buffer(3)]], device Cols& _1531 [[buffer(4)]], device Plans& _1580 [[buffer(5)]], device Views& _2195 [[buffer(6)]], device Runs& _2467 [[buffer(7)]], device OutV& _2494 [[buffer(8)]], device OutI& _2525 [[buffer(9)]], uint3 gl_WorkGroupID [[threadgroup_position_in_grid]], uint3 gl_LocalInvocationID [[thread_position_in_threadgroup]])
{
    threadgroup spvUnsafeArray<uint4, 328> vu;
    threadgroup spvUnsafeArray<uint4, 14> kc;
    threadgroup spvUnsafeArray<uint, 2048> fl;
    Draw _1407;
    _1407.image = _1397.draws[gl_WorkGroupID.x].image;
    _1407.nq = _1397.draws[gl_WorkGroupID.x].nq;
    _1407.plan = _1397.draws[gl_WorkGroupID.x].plan;
    _1407.colour = _1397.draws[gl_WorkGroupID.x].colour;
    _1407.col_addr = _1397.draws[gl_WorkGroupID.x].col_addr;
    _1407.col_count = _1397.draws[gl_WorkGroupID.x].col_count;
    _1407.view = _1397.draws[gl_WorkGroupID.x].view;
    _1407.consts = _1397.draws[gl_WorkGroupID.x].consts;
    _1407.vbase = _1397.draws[gl_WorkGroupID.x].vbase;
    _1407.ibase = _1397.draws[gl_WorkGroupID.x].ibase;
    _1407.rbase = _1397.draws[gl_WorkGroupID.x].rbase;
    _1407.flags = _1397.draws[gl_WorkGroupID.x].flags;
    _1407.wview = _1397.draws[gl_WorkGroupID.x].wview;
    _1407.p1 = _1397.draws[gl_WorkGroupID.x].p1;
    _1407.p2 = _1397.draws[gl_WorkGroupID.x].p2;
    _1407.p3 = _1397.draws[gl_WorkGroupID.x].p3;
    Draw d = _1407;
    uint tid = gl_LocalInvocationID.x;
    uint4 _1426;
    for (uint q = tid; q < 328u; q += 64u)
    {
        if (q < d.nq)
        {
            _1426 = _1432.images[d.image + q];
        }
        else
        {
            _1426 = uint4(0u);
        }
        vu[q] = _1426;
    }
    if (tid < 14u)
    {
        kc[tid] = _1454.consts[(d.consts * 14u) + tid];
    }
    for (uint j = tid; j < 2048u; j += 64u)
    {
        fl[j] = 0u;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (tid < 16u)
    {
        ((threadgroup uint*)&vu[5u + (tid / 4u)])[tid % 4u] = as_type<uint>(_1495.vuviews[(d.view * 16u) + tid]);
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    uint _1525;
    for (uint k = tid; k < min(d.col_count, 328u); k += 64u)
    {
        if (d.colour != 4294967295u)
        {
            _1525 = _1531.cols[d.colour + k];
        }
        else
        {
            _1525 = 0u;
        }
        uint w = _1525;
        if ((d.col_addr + (2u * k)) < 328u)
        {
            vu[d.col_addr + (2u * k)] = uint4(w & 255u, (w >> uint(8)) & 255u, (w >> uint(16)) & 255u, w >> uint(24));
        }
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    uint P = d.plan;
    uint hdr = _1580.plans[P];
    uint nsteps = min((hdr & 255u), 4u);
    int form = int((hdr >> uint(8)) & 7u);
    uint nv = min(_1580.plans[P + 1u], 8192u);
    uint sp = P + 3u;
    for (uint si = 0u; si < nsteps; si++)
    {
        uint w0 = _1580.plans[sp];
        uint w1 = _1580.plans[sp + 1u];
        uint n = min(_1580.plans[sp + 2u], 1312u);
        sp += 3u;
        uint kind = w0 & 255u;
        uint band = w0 >> uint(16);
        uint a = w1 & 65535u;
        uint b = w1 >> uint(16);
        if ((w0 & 256u) != 0u)
        {
            if (tid == 0u)
            {
                for (uint k_1 = 0u; k_1 < n; k_1++)
                {
                    uint param = kind;
                    uint param_1 = band;
                    uint param_2 = a;
                    uint param_3 = b;
                    uint param_4 = k_1;
                    step_iter(param, param_1, param_2, param_3, param_4, vu, kc);
                }
            }
        }
        else
        {
            for (uint k_2 = tid; k_2 < n; k_2 += 64u)
            {
                uint param_5 = kind;
                uint param_6 = band;
                uint param_7 = a;
                uint param_8 = b;
                uint param_9 = k_2;
                step_iter(param_5, param_6, param_7, param_8, param_9, vu, kc);
            }
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    uint vp = sp;
    bool bform = form == 4;
    if (bform)
    {
        form = 3;
    }
    bool _1714 = (form == 1) || (form == 3);
    bool _1721;
    if (_1714)
    {
        _1721 = (d.flags & 2147483648u) == 0u;
    }
    else
    {
        _1721 = _1714;
    }
    bool guard = _1721;
    uint param_10 = 4u;
    uint param_11 = 3u;
    uint tex = u16(param_10, param_11, vu);
    float _1731;
    if ((form == 1) || bform)
    {
        uint param_12 = tex + 1u;
        uint param_13 = 3u;
        _1731 = fa(param_12, param_13, vu);
    }
    else
    {
        uint param_14 = 657u;
        uint param_15 = 2u;
        _1731 = kf(param_14, param_15, kc);
    }
    float stoff = _1731;
    float4 c0 = as_type<float4>(kc[0]);
    float4 c4 = as_type<float4>(kc[8]);
    float4 c5 = as_type<float4>(kc[5]);
    float4 c6 = as_type<float4>(kc[6]);
    if (guard)
    {
        float4 _1787;
        for (uint j_1 = tid; j_1 < nv; j_1 += 64u)
        {
            uint param_16 = _1580.plans[vp + (2u * j_1)] & 65535u;
            uint param_17 = 3u;
            uint rec = u16(param_16, param_17, vu);
            if (form <= 1)
            {
                uint param_18 = rec;
                _1787 = ld(param_18, vu);
            }
            else
            {
                uint param_19 = rec;
                float4 param_20 = itof0(param_19, vu);
                _1787 = xform(param_20, vu);
            }
            float4 Pj = _1787;
            float4 param_21 = Pj;
            float4 param_22 = c6;
            float4 g = mul4(param_21, param_22);
            float w_1 = abs(g.w);
            uint f = 0u;
            if (g.x > w_1)
            {
                f |= 1u;
            }
            if (g.x < (-w_1))
            {
                f |= 2u;
            }
            if (g.y > w_1)
            {
                f |= 4u;
            }
            if (g.y < (-w_1))
            {
                f |= 8u;
            }
            if (g.z > w_1)
            {
                f |= 16u;
            }
            if (g.z < (-w_1))
            {
                f |= 32u;
            }
            if (f != 0u)
            {
                uint _1875 = atomic_fetch_or_explicit((threadgroup atomic_uint*)&fl[j_1 >> uint(2)], f << (8u * (j_1 & 3u)), memory_order_relaxed);
            }
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    float4 _1918;
    uint4 rgba;
    V o;
    for (uint j_2 = tid; j_2 < nv; j_2 += 64u)
    {
        uint v0 = _1580.plans[vp + (2u * j_2)];
        uint v1 = _1580.plans[(vp + (2u * j_2)) + 1u];
        uint via = v0 & 65535u;
        uint pos = (v0 >> uint(16)) & 3u;
        uint param_23 = via;
        uint param_24 = 3u;
        uint rec_1 = u16(param_23, param_24, vu);
        if (form <= 1)
        {
            uint param_25 = rec_1;
            _1918 = ld(param_25, vu);
        }
        else
        {
            uint param_26 = rec_1;
            float4 param_27 = itof0(param_26, vu);
            _1918 = xform(param_27, vu);
        }
        float4 Pj_1 = _1918;
        float4 hp = Pj_1;
        uint param_28 = via;
        uint param_29 = 0u;
        float param_30 = fa(param_28, param_29, vu);
        float param_31 = stoff;
        float s0 = subz(param_30, param_31);
        uint param_32 = via;
        uint param_33 = 1u;
        float param_34 = fa(param_32, param_33, vu);
        float param_35 = stoff;
        float t0 = subz(param_34, param_35);
        float param_36 = 1.0;
        float param_37 = stoff;
        float param_38 = addz(param_36, param_37);
        float param_39 = stoff;
        float q0 = subz(param_38, param_39);
        bool adc = false;
        if (guard && (!bform))
        {
            uint h = 0u;
            if ((v0 & 262144u) != 0u)
            {
                h |= ((fl[j_2 >> uint(2)] >> (8u * (j_2 & 3u))) & 255u);
            }
            bool _1987 = pos >= 1u;
            bool _1999;
            if (_1987)
            {
                _1999 = (_1580.plans[vp + (2u * (j_2 - 1u))] & 262144u) != 0u;
            }
            else
            {
                _1999 = _1987;
            }
            if (_1999)
            {
                h |= ((fl[(j_2 - 1u) >> uint(2)] >> (8u * ((j_2 - 1u) & 3u))) & 255u);
            }
            bool _2016 = pos >= 2u;
            bool _2028;
            if (_2016)
            {
                _2028 = (_1580.plans[vp + (2u * (j_2 - 2u))] & 262144u) != 0u;
            }
            else
            {
                _2028 = _2016;
            }
            if (_2028)
            {
                h |= ((fl[(j_2 - 2u) >> uint(2)] >> (8u * ((j_2 - 2u) & 3u))) & 255u);
            }
            adc = h != 0u;
        }
        float param_40 = c0.x;
        float param_41 = Pj_1.w;
        float _2053 = divz(param_40, param_41);
        float q_1 = _2053;
        float param_42 = q0;
        float param_43 = q_1;
        float qq = mulz(param_42, param_43);
        float param_44 = s0;
        float param_45 = q_1;
        float ss = mulz(param_44, param_45);
        float param_46 = t0;
        float param_47 = q_1;
        float tt = mulz(param_46, param_47);
        float param_48 = Pj_1.w;
        float param_49 = c5.w;
        float fw = addz(param_48, param_49);
        if (form == 3)
        {
            float param_50 = fw;
            float param_51 = c0.z;
            fw = fmin_(param_50, param_51);
            float param_52 = fw;
            float param_53 = c0.y;
            fw = fmax_(param_52, param_53);
        }
        else
        {
            float param_54 = fw;
            float param_55 = c0.y;
            fw = fmax_(param_54, param_55);
            float param_56 = fw;
            float param_57 = c0.z;
            fw = fmin_(param_56, param_57);
        }
        if (adc)
        {
            float param_58 = fw;
            float param_59 = c0.w;
            fw = addz(param_58, param_59);
        }
        if (form <= 1)
        {
            uint param_60 = rec_1 + 1u;
            float4 param_61 = ld(param_60, vu);
            float4 param_62 = c4;
            float4 C = mul4(param_61, param_62);
            float param_63 = C.x;
            float param_64 = C.y;
            float param_65 = C.z;
            float param_66 = C.w;
            rgba = uint4(uint(ftoi(param_63)), uint(ftoi(param_64)), uint(ftoi(param_65)), uint(ftoi(param_66)));
        }
        else
        {
            uint param_67 = rec_1 + 1u;
            rgba = at(param_67, vu);
        }
        float param_68 = fw;
        float param_69 = 16.0;
        float param_70 = mulz(param_68, param_69);
        uint xyzf3 = uint(ftoi(param_70));
        uint wv = d.wview * 16u;
        float param_71 = hp.x;
        ulong h0 = f2d(param_71);
        float param_72 = hp.y;
        ulong h1 = f2d(param_72);
        float param_73 = hp.z;
        ulong h2 = f2d(param_73);
        float param_74 = hp.w;
        ulong h3 = f2d(param_74);
        ulong param_75 = _2195.views[wv + 0u];
        ulong param_76 = h0;
        ulong param_77 = _2195.views[wv + 4u];
        ulong param_78 = h1;
        ulong param_79 = dmulz(param_75, param_76);
        ulong param_80 = dmulz(param_77, param_78);
        ulong _2215 = daddz(param_79, param_80);
        ulong param_81 = _2195.views[wv + 8u];
        ulong param_82 = h2;
        ulong param_83 = _2215;
        ulong param_84 = dmulz(param_81, param_82);
        ulong _2226 = daddz(param_83, param_84);
        ulong param_85 = _2195.views[wv + 12u];
        ulong param_86 = h3;
        ulong param_87 = _2226;
        ulong param_88 = dmulz(param_85, param_86);
        ulong _2238 = daddz(param_87, param_88);
        ulong x0 = _2238;
        ulong param_89 = _2195.views[wv + 1u];
        ulong param_90 = h0;
        ulong param_91 = _2195.views[wv + 5u];
        ulong param_92 = h1;
        ulong param_93 = dmulz(param_89, param_90);
        ulong param_94 = dmulz(param_91, param_92);
        ulong _2258 = daddz(param_93, param_94);
        ulong param_95 = _2195.views[wv + 9u];
        ulong param_96 = h2;
        ulong param_97 = _2258;
        ulong param_98 = dmulz(param_95, param_96);
        ulong _2270 = daddz(param_97, param_98);
        ulong param_99 = _2195.views[wv + 13u];
        ulong param_100 = h3;
        ulong param_101 = _2270;
        ulong param_102 = dmulz(param_99, param_100);
        ulong _2282 = daddz(param_101, param_102);
        ulong x1 = _2282;
        ulong param_103 = _2195.views[wv + 2u];
        ulong param_104 = h0;
        ulong param_105 = _2195.views[wv + 6u];
        ulong param_106 = h1;
        ulong param_107 = dmulz(param_103, param_104);
        ulong param_108 = dmulz(param_105, param_106);
        ulong _2302 = daddz(param_107, param_108);
        ulong param_109 = _2195.views[wv + 10u];
        ulong param_110 = h2;
        ulong param_111 = _2302;
        ulong param_112 = dmulz(param_109, param_110);
        ulong _2314 = daddz(param_111, param_112);
        ulong param_113 = _2195.views[wv + 14u];
        ulong param_114 = h3;
        ulong param_115 = _2314;
        ulong param_116 = dmulz(param_113, param_114);
        ulong _2325 = daddz(param_115, param_116);
        ulong x2 = _2325;
        ulong param_117 = _2195.views[wv + 3u];
        ulong param_118 = h0;
        ulong param_119 = _2195.views[wv + 7u];
        ulong param_120 = h1;
        ulong param_121 = dmulz(param_117, param_118);
        ulong param_122 = dmulz(param_119, param_120);
        ulong _2345 = daddz(param_121, param_122);
        ulong param_123 = _2195.views[wv + 11u];
        ulong param_124 = h2;
        ulong param_125 = _2345;
        ulong param_126 = dmulz(param_123, param_124);
        ulong _2357 = daddz(param_125, param_126);
        ulong param_127 = _2195.views[wv + 15u];
        ulong param_128 = h3;
        ulong param_129 = _2357;
        ulong param_130 = dmulz(param_127, param_128);
        ulong _2369 = daddz(param_129, param_130);
        ulong x3 = _2369;
        float param_131 = qq;
        float param_132 = hp.w;
        float kk = mulz(param_131, param_132);
        float sgn = (kk < 0.0) ? (-1.0) : 1.0;
        float param_133 = hp.w;
        float param_134 = sgn;
        float Wg = mulz(param_133, param_134);
        float ws = (Wg != 0.0) ? Wg : 1.0;
        ulong param_135 = x0;
        o.wp0 = d2fz(param_135);
        ulong param_136 = x1;
        o.wp1 = d2fz(param_136);
        ulong param_137 = x2;
        o.wp2 = d2fz(param_137);
        ulong param_138 = x3;
        o.wp3 = d2fz(param_138);
        o.vu0 = 0.0;
        o.vu1 = 0.0;
        o.vu2 = 0.0;
        o.vu3 = 0.0;
        float param_139 = ss;
        float param_140 = ws;
        o.s = mulz(param_139, param_140);
        float param_141 = tt;
        float param_142 = ws;
        o.t = mulz(param_141, param_142);
        float param_143 = qq;
        float param_144 = ws;
        o.q = mulz(param_143, param_144);
        o.lodq = qq;
        o.rgba = (((rgba.x & 255u) | ((rgba.y & 255u) << uint(8))) | ((rgba.z & 255u) << uint(16))) | ((rgba.w & 255u) << uint(24));
        o.fog = float((xyzf3 >> uint(4)) & 255u);
        uint4 run = _2467.runs[d.rbase + (v1 & 65535u)];
        o.word = run.x;
        o.layer = run.y;
        o.st0 = run.z;
        o.st1 = run.w;
        V_1 _2502;
        _2502.wp0 = o.wp0;
        _2502.wp1 = o.wp1;
        _2502.wp2 = o.wp2;
        _2502.wp3 = o.wp3;
        _2502.vu0 = o.vu0;
        _2502.vu1 = o.vu1;
        _2502.vu2 = o.vu2;
        _2502.vu3 = o.vu3;
        _2502.s = o.s;
        _2502.t = o.t;
        _2502.q = o.q;
        _2502.lodq = o.lodq;
        _2502.rgba = o.rgba;
        _2502.fog = o.fog;
        _2502.word = o.word;
        _2502.layer = o.layer;
        _2502.st0 = o.st0;
        _2502.st1 = o.st1;
        _2494.outv[d.vbase + j_2] = _2502;
        if (pos >= 2u)
        {
            uint t = d.ibase + (3u * (v1 >> uint(16)));
            uint c = d.vbase + j_2;
            if (adc)
            {
                _2525.outi[t] = c;
                _2525.outi[t + 1u] = c;
                _2525.outi[t + 2u] = c;
            }
            else
            {
                _2525.outi[t] = c;
                _2525.outi[t + 1u] = c - 2u;
                _2525.outi[t + 2u] = c - 1u;
            }
        }
    }
}

