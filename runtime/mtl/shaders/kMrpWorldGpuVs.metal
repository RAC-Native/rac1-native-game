// kMrpWorldGpuVs (vert): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
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

struct Cmds
{
    uint4 cmds[1];
};

struct Inst
{
    float4x4 clip;
    spvUnsafeArray<float4, 4> rows;
    float4 morph;
    uint4 info;
};

struct Inst_1
{
    float4x4 clip;
    float4 rows[4];
    float4 morph;
    uint4 info;
};

struct Insts
{
    Inst_1 insts[1];
};

struct RCols
{
    uint rcols[1];
};

struct Cols
{
    uint4 cols[1];
};

struct Push
{
    float4x4 uVP;
    float4 uFrame;
    float4 uXyOff;
    float4 uFogCol_Pad;
    int uMode;
    int uMipMode;
    int uShade;
    int uFge;
    int uAtPass;
    int uU8;
    int _pad0;
    int _pad1;
};

struct kMrpWorldGpuVs_out
{
    float3 vStq [[user(locn0)]];
    float vLodQ [[user(locn1)]];
    float4 vColor [[user(locn2)]];
    float4 vFlat [[user(locn3)]];
    float vFog [[user(locn4)]];
    uint2 vTex [[user(locn5)]];
    uint2 vState [[user(locn6)]];
    float3 vShWorld [[user(locn7)]];
    float4 gl_Position [[position]];
    float gl_ClipDistance [[clip_distance]] [1];
    float gl_ClipDistance_0 [[user(clip0)]];
};

struct kMrpWorldGpuVs_in
{
    int4 aPos [[attribute(0)]];
    int4 aDelta [[attribute(1)]];
    int4 aUv [[attribute(2)]];
    uint4 aCol [[attribute(3)]];
};

static inline __attribute__((always_inline))
float4 sh_clip(thread const float4& clip, thread float3& vShWorld, thread float4& gl_Position, thread float (&gl_ClipDistance)[1], constant ShadowUbo& _50)
{
    vShWorld = float3(0.0);
    gl_ClipDistance[0] = 1.0;
    bool _58 = int(_50.uShCam_Pass.w + 0.5) == 0;
    bool _67;
    if (_58)
    {
        _67 = int(_50.uShSun_On.w + 0.5) == 0;
    }
    else
    {
        _67 = _58;
    }
    if (_67)
    {
        return clip;
    }
    float4 wp = _50.uShInvView * float4(clip.x / _50.uShProj.x, clip.y / _50.uShProj.y, -clip.w, 1.0);
    vShWorld = wp.xyz;
    if (int(_50.uShCam_Pass.w + 0.5) != 0)
    {
        gl_ClipDistance[0] = _50.uShCut.w - dot(wp.xyz, _50.uShCut.xyz);
        return _50.uShLight * float4(wp.xyz, 1.0);
    }
    return clip;
}

static inline __attribute__((always_inline))
int ftoi4(thread const float& f)
{
    return int(fast::clamp(f * 16.0, -2147483648.0, 2147483520.0));
}

vertex kMrpWorldGpuVs_out kMrpWorldGpuVs(kMrpWorldGpuVs_in in [[stage_in]], constant ShadowUbo& _50 [[buffer(0)]], const device Cmds& _143 [[buffer(1)]], const device Insts& _174 [[buffer(2)]], const device RCols& _274 [[buffer(3)]], const device Cols& _299 [[buffer(4)]], constant Push& _473 [[buffer(5)]], uint gl_InstanceIndex [[instance_id]])
{
    kMrpWorldGpuVs_out out = {};
    uint4 cmd = _143.cmds[2 * int(gl_InstanceIndex)];
    out.vState = uint2(cmd.w, ((device uint*)&_143.cmds[(2 * int(gl_InstanceIndex)) + 1])[0u]);
    Inst _180;
    _180.clip = _174.insts[cmd.x].clip;
    _180.rows[0] = _174.insts[cmd.x].rows[0];
    _180.rows[1] = _174.insts[cmd.x].rows[1];
    _180.rows[2] = _174.insts[cmd.x].rows[2];
    _180.rows[3] = _174.insts[cmd.x].rows[3];
    _180.morph = _174.insts[cmd.x].morph;
    _180.info = _174.insts[cmd.x].info;
    Inst I = _180;
    out.vTex = cmd.yz;
    float3 P = float3(in.aPos.xyz);
    if ((in.aPos.w & 1) != 0)
    {
        float3 d = float3(in.aDelta.xyz) * I.morph.x;
        P = spvFAdd(d, float3(in.aPos.xyz));
    }
    float4 p = I.rows[3];
    p = spvFAdd(p, I.rows[0] * P.x);
    p = spvFAdd(p, I.rows[1] * P.y);
    p = spvFAdd(p, I.rows[2] * P.z);
    float Q = 1.0 / p.w;
    float3 uv = float3(in.aUv.xyz) * 0.000244140625;
    uint first = I.info.x;
    uint kind = I.info.w & 1u;
    bool resident = (I.info.w & 2u) != 0u;
    uint4 c;
    if (kind == 0u)
    {
        uint4 _268;
        if (resident)
        {
            _268 = ((uint4(_274.rcols[first + in.aCol.x]) >> uint4(0u, 8u, 16u, 24u)) & uint4(255u)) | uint4(1258291200u);
        }
        else
        {
            _268 = _299.cols[first + in.aCol.x];
        }
        uint4 c0 = _268;
        if ((in.aPos.w & 2) != 0)
        {
            uint4 _315;
            if (resident)
            {
                _315 = ((uint4(_274.rcols[first + in.aCol.y]) >> uint4(0u, 8u, 16u, 24u)) & uint4(255u)) | uint4(1258291200u);
            }
            else
            {
                _315 = _299.cols[first + in.aCol.y];
            }
            uint4 c1 = _315;
            uint4 _340;
            if (resident)
            {
                _340 = ((uint4(_274.rcols[first + in.aCol.z]) >> uint4(0u, 8u, 16u, 24u)) & uint4(255u)) | uint4(1258291200u);
            }
            else
            {
                _340 = _299.cols[first + in.aCol.z];
            }
            uint4 c2 = _340;
            for (int l = 0; l < 4; l++)
            {
                float a = spvFMul(as_type<float>(c1[l]), 0.5);
                a = spvFAdd(a, spvFMul(as_type<float>(c2[l]), 0.5));
                float b = spvFMul(as_type<float>(c0[l]), I.morph.z);
                float r = spvFMul(a, I.morph.w);
                r = spvFAdd(r, b);
                c[l] = as_type<uint>(r) & 255u;
            }
        }
        else
        {
            c = c0 & uint4(255u);
        }
    }
    else
    {
        if (resident)
        {
            c = uint4((uint3(_274.rcols[first + in.aCol.x]) >> uint3(0u, 8u, 16u)) & uint3(255u), I.info.z);
        }
        else
        {
            c = uint4(_299.cols[first + in.aCol.x].xyz & uint3(255u), I.info.z);
        }
    }
    out.vColor = float4(c) / float4(255.0);
    out.vFlat = out.vColor;
    out.vFog = float(I.info.y);
    out.vStq = uv;
    out.vLodQ = uv.z * Q;
    if (_473.uMode == 0)
    {
        float4 param = I.clip * float4(P, 1.0);
        float4 _490 = sh_clip(param, out.vShWorld, out.gl_Position, out.gl_ClipDistance, _50);
        out.gl_Position = _490;
    }
    else
    {
        out.vShWorld = float3(0.0);
        out.gl_ClipDistance[0] = 1.0;
        float w = p.w;
        float param_1 = p.x * Q;
        float X = float(ftoi4(param_1) & 65535) / 16.0;
        float param_2 = p.y * Q;
        float Y = float(ftoi4(param_2) & 65535) / 16.0;
        float param_3 = p.z * Q;
        float Z = float((ftoi4(param_3) >> 4) & 16777215);
        float X1 = _473.uFrame.x * 0.5;
        float sx = _473.uFrame.z / X1;
        float sy = 1.0 / (_473.uFrame.y * 0.5);
        float2 cc = float2((_473.uXyOff.x / 16.0) + X1, (_473.uXyOff.y / 16.0) + (_473.uFrame.y * 0.5));
        float zc = ((2.0 * (_473.uXyOff.z + (_473.uXyOff.w * Z))) - 1.0) * w;
        out.gl_Position = float4(((X - cc.x) * w) * sx, ((-(Y - cc.y)) * w) * sy, zc, w);
    }
    out.gl_ClipDistance_0 = out.gl_ClipDistance[0];
    out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;       // Adjust clip-space for Metal
    out.gl_Position.y = -(out.gl_Position.y);    // Invert Y-axis for Metal
    return out;
}

