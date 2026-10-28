// kMrpMobySplitVs (vert): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

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

struct ShadowUbo
{
    float4x4 uShInvView;
    float4x4 uShLight;
    float4x4 uShCasc[3];
    float4x4 uPtVP;
    float4 uShCut;
    float4 uShCam_Pass;
    float4 uShSun_On;
    float4 uShParam;
    float4 uShTexel_SunK;
    float4 uShKey;
    float4 uShKeyDir_Str;
    float4 uShPart_Near;
    float2 uShProj;
    int uShDebug;
    int uPtN;
    int uShOpaque;
    int _shPad0;
    int _shPad1;
    int _shPad2;
    float4 uPtPos[96];
    float4 uPtCol[96];
    float4 uPtDown[24];
    float4 uEmis[32];
    float4 uMat[32];
    float4x4 uSsrVP;
    float4 uSsrZ;
    float4 uSsrSky;
};

struct Joints
{
    float4 J[1];
};

struct Cmds
{
    uint4 cmds[1];
};

struct Part
{
    spvUnsafeArray<float4, 4> clip;
    spvUnsafeArray<float4, 4> rows;
    spvUnsafeArray<float4, 8> light;
    float4 sub;
    int4 info;
};

struct Part_1
{
    float4 clip[4];
    float4 rows[4];
    float4 light[8];
    float4 sub;
    int4 info;
};

struct Parts
{
    Part_1 parts[1];
};

struct Normals
{
    float2 normals[1];
};

struct Push
{
    float4 uOffset;
    float4 uFrame;
    float4 uXyOff;
    float4 uFogCol_Min;
    int uMode;
    float uProj;
    int uTme;
    int uAte;
    int uAtst;
    int uAref;
    int uAfail;
    int uMipMode;
    int uShade;
    int uFge;
    int uPass;
    int _pad;
};

struct kMrpMobySplitVs_out
{
    float4 vColor [[user(locn0)]];
    float3 vStq [[user(locn1)]];
    float vFog [[user(locn2)]];
    uint4 vCmd [[user(locn3)]];
    float4 vColorNoKey [[user(locn4)]];
    float3 vShWorld [[user(locn7)]];
    float4 gl_Position [[position]];
    float gl_ClipDistance [[clip_distance]] [1];
    float gl_ClipDistance_0 [[user(clip0)]];
};

struct kMrpMobySplitVs_in
{
    int4 aPosN [[attribute(0)]];
    uint4 aJoint [[attribute(1)]];
    uint4 aWeight [[attribute(2)]];
    uint4 aRgba [[attribute(3)]];
    int2 aUv [[attribute(4)]];
    int4 aQ2Pos [[attribute(5)]];
    uint4 aQ2Joint [[attribute(6)]];
    uint4 aQ2Weight [[attribute(7)]];
};

static inline __attribute__((always_inline))
float4 joint_row(thread const uint& j, thread const int& r, thread const int& first, thread const int& count, const device Joints& _167)
{
    float4 _161;
    if (int(j) < count)
    {
        _161 = _167.J[((first + int(j)) * 4) + r];
    }
    else
    {
        _161 = float4(0.0);
    }
    return _161;
}

static inline __attribute__((always_inline))
void matrix_of(thread const uint4& jt, thread const uint4& wt, thread const int& first, thread const int& count, thread spvUnsafeArray<float4, 4>& m, const device Joints& _167)
{
    if (jt.w == 1u)
    {
        for (int r = 0; r < 4; r++)
        {
            uint param = jt.x;
            int param_1 = r;
            int param_2 = first;
            int param_3 = count;
            m[r] = joint_row(param, param_1, param_2, param_3, _167);
        }
        return;
    }
    if (jt.w == 0u)
    {
        for (int r_1 = 0; r_1 < 4; r_1++)
        {
            m[r_1] = float4(0.0);
        }
        return;
    }
    float w0 = spvFMul(spvFMul(float(wt.x), 0.000244140625), 16.0);
    float w1 = spvFMul(spvFMul(float(wt.y), 0.000244140625), 16.0);
    float w2 = spvFMul(spvFMul(float(wt.z), 0.000244140625), 16.0);
    for (int r_2 = 0; r_2 < 4; r_2++)
    {
        uint param_4 = jt.x;
        int param_5 = r_2;
        int param_6 = first;
        int param_7 = count;
        float4 a = joint_row(param_4, param_5, param_6, param_7, _167) * w0;
        uint param_8 = jt.y;
        int param_9 = r_2;
        int param_10 = first;
        int param_11 = count;
        a = spvFAdd(a, joint_row(param_8, param_9, param_10, param_11, _167) * w1);
        if (jt.w == 3u)
        {
            uint param_12 = jt.z;
            int param_13 = r_2;
            int param_14 = first;
            int param_15 = count;
            a = spvFAdd(a, joint_row(param_12, param_13, param_14, param_15, _167) * w2);
        }
        m[r_2] = a;
    }
}

static inline __attribute__((always_inline))
int3 skin_pos(thread const spvUnsafeArray<float4, 4>& m, thread const int3& p)
{
    int3 q;
    for (int l = 0; l < 3; l++)
    {
        float a = spvFMul(m[0][l], float(p.x));
        a = spvFAdd(a, spvFMul(m[1][l], float(p.y)));
        a = spvFAdd(a, spvFMul(m[2][l], float(p.z)));
        a = spvFAdd(a, m[3][l]);
        int i = int(fast::clamp(a, -2147483648.0, 2147483520.0));
        q[l] = (i << 16) >> 16;
    }
    return q;
}

static inline __attribute__((always_inline))
int ftoi4(thread const float& f)
{
    return int(fast::clamp(f * 16.0, -2147483648.0, 2147483520.0));
}

static inline __attribute__((always_inline))
float4 sh_clip(thread const float4& clip, thread float3& vShWorld, thread float4& gl_Position, thread float (&gl_ClipDistance)[1], constant ShadowUbo& _79)
{
    vShWorld = float3(0.0);
    gl_ClipDistance[0] = 1.0;
    bool _87 = int(_79.uShCam_Pass.w + 0.5) == 0;
    bool _96;
    if (_87)
    {
        _96 = int(_79.uShSun_On.w + 0.5) == 0;
    }
    else
    {
        _96 = _87;
    }
    if (_96)
    {
        return clip;
    }
    float4 wp = _79.uShInvView * float4(clip.x / _79.uShProj.x, clip.y / _79.uShProj.y, -clip.w, 1.0);
    vShWorld = wp.xyz;
    if (int(_79.uShCam_Pass.w + 0.5) != 0)
    {
        gl_ClipDistance[0] = _79.uShCut.w - dot(wp.xyz, _79.uShCut.xyz);
        return _79.uShLight * float4(wp.xyz, 1.0);
    }
    return clip;
}

vertex kMrpMobySplitVs_out kMrpMobySplitVs(kMrpMobySplitVs_in in [[stage_in]], constant ShadowUbo& _79 [[buffer(0)]], const device Joints& _167 [[buffer(1)]], const device Cmds& _377 [[buffer(2)]], const device Parts& _397 [[buffer(3)]], const device Normals& _445 [[buffer(4)]], constant Push& _781 [[buffer(5)]], uint gl_InstanceIndex [[instance_id]])
{
    kMrpMobySplitVs_out out = {};
    uint4 cmd = _377.cmds[int(gl_InstanceIndex)];
    Part _403;
    _403.clip[0] = _397.parts[cmd.x].clip[0];
    _403.clip[1] = _397.parts[cmd.x].clip[1];
    _403.clip[2] = _397.parts[cmd.x].clip[2];
    _403.clip[3] = _397.parts[cmd.x].clip[3];
    _403.rows[0] = _397.parts[cmd.x].rows[0];
    _403.rows[1] = _397.parts[cmd.x].rows[1];
    _403.rows[2] = _397.parts[cmd.x].rows[2];
    _403.rows[3] = _397.parts[cmd.x].rows[3];
    _403.light[0] = _397.parts[cmd.x].light[0];
    _403.light[1] = _397.parts[cmd.x].light[1];
    _403.light[2] = _397.parts[cmd.x].light[2];
    _403.light[3] = _397.parts[cmd.x].light[3];
    _403.light[4] = _397.parts[cmd.x].light[4];
    _403.light[5] = _397.parts[cmd.x].light[5];
    _403.light[6] = _397.parts[cmd.x].light[6];
    _403.light[7] = _397.parts[cmd.x].light[7];
    _403.sub = _397.parts[cmd.x].sub;
    _403.info = _397.parts[cmd.x].info;
    Part P = _403;
    int first = P.info.x;
    int count = P.info.y;
    uint4 param = in.aJoint;
    uint4 param_1 = in.aWeight;
    int param_2 = first;
    int param_3 = count;
    spvUnsafeArray<float4, 4> param_4;
    matrix_of(param, param_1, param_2, param_3, param_4, _167);
    spvUnsafeArray<float4, 4> m = param_4;
    spvUnsafeArray<float4, 4> param_5 = m;
    int3 param_6 = in.aPosN.xyz;
    int3 q = skin_pos(param_5, param_6);
    uint nb = uint(in.aPosN.w) & 65535u;
    float2 ta = _445.normals[nb & 255u];
    float2 te = _445.normals[(nb >> uint(8)) & 255u];
    float nx = spvFMul(ta.x, te.x);
    float ny = spvFMul(ta.y, te.x);
    float nw = te.y;
    float4 n = m[0] * nx;
    n = spvFAdd(n, m[1] * ny);
    n = spvFAdd(n, m[2] * nw);
    float len2 = spvFAdd(spvFMul(n.x, n.x), spvFMul(n.y, n.y));
    len2 = spvFAdd(len2, spvFMul(n.z, n.z));
    float _513;
    if (len2 > 0.0)
    {
        _513 = 1.0 / sqrt(len2);
    }
    else
    {
        _513 = 3.4028234663852885981170418348452e+38;
    }
    float qn = _513;
    float4 d = P.light[0] * n.x;
    d = spvFAdd(d, P.light[1] * n.y);
    d = spvFAdd(d, P.light[2] * n.z);
    float4 dk = spvFMul(d, -abs(P.light[6]));
    d = fast::max(d, dk);
    float4 a = P.light[3] * d.x;
    a = spvFAdd(a, P.light[4] * d.y);
    a = spvFAdd(a, P.light[5] * d.z);
    float _585;
    int4 col;
    for (int l = 0; l < 4; l++)
    {
        if (a[l] == 0.0)
        {
            _585 = 0.0;
        }
        else
        {
            _585 = spvFMul(a[l], qn);
        }
        float c = spvFAdd(_585, spvFMul(P.light[7][l], 1.0));
        int h = int(as_type<uint>(c) << uint(16)) >> 16;
        int pr = clamp(h * int(in.aRgba[l]), -32768, 32767);
        col[l] = (pr >> 7) & 255;
    }
    if ((P.info.z & 1) != 0)
    {
        col = int4(P.sub);
    }
    out.vColorNoKey = float4(col) / float4(255.0);
    bool _649 = int(_79.uShSun_On.w + 0.5) != 0;
    bool _656;
    if (_649)
    {
        _656 = (P.info.z & 1) == 0;
    }
    else
    {
        _656 = _649;
    }
    if (_656)
    {
        float4 dn = spvFAdd(spvFAdd(P.light[0] * n.x, P.light[1] * n.y), P.light[2] * n.z);
        dn = fast::max(dn, spvFMul(dn, -abs(P.light[6])));
        float4 an = spvFAdd(P.light[4] * dn.y, P.light[5] * dn.z);
        float _710;
        int4 cn;
        for (int l_1 = 0; l_1 < 4; l_1++)
        {
            if (an[l_1] == 0.0)
            {
                _710 = 0.0;
            }
            else
            {
                _710 = spvFMul(an[l_1], qn);
            }
            float c_1 = spvFAdd(_710, spvFMul(P.light[7][l_1], 1.0));
            int h_1 = int(as_type<uint>(c_1) << uint(16)) >> 16;
            int pr_1 = clamp(h_1 * int(in.aRgba[l_1]), -32768, 32767);
            cn[l_1] = (pr_1 >> 7) & 255;
        }
        out.vColorNoKey = float4(cn) / float4(255.0);
    }
    float4 p = P.rows[3];
    p = spvFAdd(p, P.rows[0] * float(q.x));
    p = spvFAdd(p, P.rows[1] * float(q.y));
    p = spvFAdd(p, P.rows[2] * float(q.z));
    float Q = _781.uProj / p.w;
    float qq = Q;
    if ((in.aWeight.w & 1u) != 0u)
    {
        uint4 param_7 = in.aQ2Joint;
        uint4 param_8 = in.aQ2Weight;
        int param_9 = first;
        int param_10 = count;
        spvUnsafeArray<float4, 4> param_11;
        matrix_of(param_7, param_8, param_9, param_10, param_11, _167);
        spvUnsafeArray<float4, 4> m2 = param_11;
        spvUnsafeArray<float4, 4> param_12 = m2;
        int3 param_13 = in.aQ2Pos.xyz;
        int3 q2 = skin_pos(param_12, param_13);
        float w2 = P.rows[3].w;
        w2 = spvFAdd(w2, spvFMul(P.rows[0].w, float(q2.x)));
        w2 = spvFAdd(w2, spvFMul(P.rows[1].w, float(q2.y)));
        w2 = spvFAdd(w2, spvFMul(P.rows[2].w, float(q2.z)));
        qq = Q * (_781.uProj / w2);
    }
    float3 stq = float3((float(in.aUv.x) * 0.000244140625) * qq, (float(in.aUv.y) * 0.000244140625) * qq, 1.0 * qq);
    float X = spvFAdd(spvFMul(p.x, Q), _781.uOffset.x);
    float Y = spvFAdd(spvFMul(p.y, Q), _781.uOffset.y);
    float Z = spvFAdd(spvFMul(p.z, Q), _781.uOffset.z);
    float param_14 = X;
    int xi = ftoi4(param_14);
    float param_15 = Y;
    int yi = ftoi4(param_15);
    float param_16 = Z;
    int zi = ftoi4(param_16);
    if (_781.uMode == 0)
    {
        float4 param_17 = float4x4(float4(P.clip[0]), float4(P.clip[1]), float4(P.clip[2]), float4(P.clip[3])) * float4(float3(q), 1.0);
        float4 _950 = sh_clip(param_17, out.vShWorld, out.gl_Position, out.gl_ClipDistance, _79);
        out.gl_Position = _950;
    }
    else
    {
        out.vShWorld = float3(0.0);
        out.gl_ClipDistance[0] = 1.0;
        float wq = p.w / _781.uProj;
        float X1 = _781.uFrame.x * 0.5;
        float sx = _781.uFrame.z / X1;
        float sy = 1.0 / (_781.uFrame.y * 0.5);
        float2 c_2 = float2((_781.uXyOff.x / 16.0) + X1, (_781.uXyOff.y / 16.0) + (_781.uFrame.y * 0.5));
        float2 _991;
        if (wq > 0.0)
        {
            _991 = (float2(float(xi), float(yi)) / float2(16.0)) * wq;
        }
        else
        {
            _991 = float2(p.x + (_781.uOffset.x * wq), p.y + (_781.uOffset.y * wq));
        }
        float2 xy = _991;
        float zc = (((2.0 * (_781.uXyOff.z + (_781.uXyOff.w * _781.uOffset.z))) - 1.0) * wq) + ((2.0 * _781.uXyOff.w) * p.z);
        out.gl_Position = float4((xy.x - (c_2.x * wq)) * sx, (((_781.uFrame.w != 0.0) ? 1.0 : (-1.0)) * (xy.y - (c_2.y * wq))) * sy, zc, wq);
    }
    float _1074;
    if ((P.info.w & 1) != 0)
    {
        _1074 = p.w;
    }
    else
    {
        _1074 = P.rows[3].w;
    }
    out.vFog = floor(fast::min(fast::max(_1074 + _781.uOffset.w, _781.uFogCol_Min.w), 255.0));
    out.vColor = float4(col) / float4(255.0);
    out.vStq = stq;
    out.vCmd = cmd;
    out.gl_ClipDistance_0 = out.gl_ClipDistance[0];
    out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;       // Adjust clip-space for Metal
    out.gl_Position.y = -(out.gl_Position.y);    // Invert Y-axis for Metal
    return out;
}

