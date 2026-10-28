// kMrpWorldEarlyFs (frag): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
// rac1: flat inputs (the triangles drawn last vertex first)
#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

// Implementation of the GLSL findLSB() function
template<typename T>
inline T spvFindLSB(T x)
{
    return select(ctz(x), T(-1), x == T(0));
}

template<typename T>
inline T spvSubgroupBroadcastFirst(T value)
{
    return simd_broadcast_first(value);
}

template<>
inline bool spvSubgroupBroadcastFirst(bool value)
{
    return !!simd_broadcast_first((ushort)value);
}

template<uint N>
inline vec<bool, N> spvSubgroupBroadcastFirst(vec<bool, N> value)
{
    return (vec<bool, N>)simd_broadcast_first((vec<ushort, N>)value);
}

template <typename T>
static inline depth2d<T> spvDepthCast(texture2d<T> t)
{
    return reinterpret_cast<thread const depth2d<T> &>(t);
}

template <typename T>
static inline depth2d_array<T> spvDepthCast(texture2d_array<T> t)
{
    return reinterpret_cast<thread const depth2d_array<T> &>(t);
}

template <typename T>
static inline depthcube<T> spvDepthCast(texturecube<T> t)
{
    return reinterpret_cast<thread const depthcube<T> &>(t);
}

template <typename T>
static inline depthcube_array<T> spvDepthCast(texturecube_array<T> t)
{
    return reinterpret_cast<thread const depthcube_array<T> &>(t);
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

struct spvDescriptorSetBuffer2
{
    array<texture2d_array<float>, 16> uTexs [[id(0)]];
    array<sampler, 16> uTexsSmplr [[id(16)]];
};

struct kMrpWorldEarlyFs_out
{
    float4 FragColor [[color(0)]];
};

struct kMrpWorldEarlyFs_in
{
    float3 vStq [[user(locn0)]];
    float vLodQ [[user(locn1), center_no_perspective]];
    float4 vColor [[user(locn2), center_no_perspective]];
    float4 vFlat [[user(locn3), flat]];
    float vFog [[user(locn4), center_no_perspective]];
    uint2 vTex [[user(locn5)]];
    uint2 vState [[user(locn6)]];
    float3 vShWorld [[user(locn7)]];
};

static inline __attribute__((always_inline))
bool sh_inside(thread const int& k, thread const float3& p, thread const float& m, constant ShadowUbo& _137)
{
    float4 q = _137.uShCasc[k] * float4(p, 1.0);
    bool _248 = all(q.xy > float2(m));
    bool _258;
    if (_248)
    {
        _258 = all(q.xy < float2(1.0 - m));
    }
    else
    {
        _258 = _248;
    }
    bool _264;
    if (_258)
    {
        _264 = q.z < 1.0;
    }
    else
    {
        _264 = _258;
    }
    return _264;
}

static inline __attribute__((always_inline))
float sh_tap(thread const int& k, thread const float3& p, constant ShadowUbo& _137, texture2d_array<float> uShMap, sampler uShMapSmplr)
{
    float4 q = _137.uShCasc[k] * float4(p, 1.0);
    float r = _137.uShParam.w / (float(int3(uShMap.get_width(), uShMap.get_height(), uShMap.get_array_size()).x) * _137.uShPart_Near[uint3(0u, 1u, 2u)[k]]);
    float s = 0.0;
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            float4 _218 = float4((q.xy + (float2(float(x), float(y)) * r)) * _137.uShPart_Near[uint3(0u, 1u, 2u)[k]], float(k), q.z);
            s += spvDepthCast(uShMap).sample_compare(uShMapSmplr, _218.xy, uint(rint(_218.z)), _218.w);
        }
    }
    return s / 9.0;
}

static inline __attribute__((always_inline))
void sh_prepare(thread float& sh_vis, thread float3& sh_n, constant ShadowUbo& _137, texture2d_array<float> uShMap, sampler uShMapSmplr, thread float4& sh_state, thread float3& vShWorld)
{
    sh_state = float4(1.0, 0.0, -1.0, 0.0);
    bool _277 = int(_137.uShCam_Pass.w + 0.5) != 0;
    bool _287;
    if (!_277)
    {
        _287 = int(_137.uShSun_On.w + 0.5) == 0;
    }
    else
    {
        _287 = _277;
    }
    if (_287)
    {
        return;
    }
    float3 dx = dfdx(vShWorld);
    float3 dy = dfdy(vShWorld);
    float3 n = cross(dx, dy);
    float nl = length(n);
    float3 _308;
    if (nl > 0.0)
    {
        _308 = n / float3(nl);
    }
    else
    {
        _308 = float3(0.0, 0.0, 1.0);
    }
    n = _308;
    if (dot(n, _137.uShCam_Pass.xyz - vShWorld) < 0.0)
    {
        n = -n;
    }
    sh_n = n;
    float ndl = dot(n, _137.uShSun_On.xyz);
    float facing = smoothstep(0.0, 0.25, ndl);
    bool _342 = facing <= 0.0;
    bool _350;
    if (_342)
    {
        _350 = _137.uShDebug == 0;
    }
    else
    {
        _350 = _342;
    }
    if (_350)
    {
        sh_state = float4(1.0, 0.0, -1.0, 1.0);
        return;
    }
    int count = int(_137.uShParam.y);
    float vis = 1.0;
    int casc = -1;
    for (int k = 0; k < count; k++)
    {
        float3 p = (vShWorld + (n * (_137.uShTexel_SunK[uint3(0u, 1u, 2u)[k]] * 1.5))) + (_137.uShSun_On.xyz * (_137.uShTexel_SunK[uint3(0u, 1u, 2u)[k]] * 1.0));
        int param = k;
        float3 param_1 = p;
        float param_2 = 0.0199999995529651641845703125;
        if (!sh_inside(param, param_1, param_2, _137))
        {
            continue;
        }
        int param_3 = k;
        float3 param_4 = p;
        vis = sh_tap(param_3, param_4, _137, uShMap, uShMapSmplr);
        casc = k;
        bool _412 = (k + 1) < count;
        bool _425;
        if (_412)
        {
            int param_5 = k;
            float3 param_6 = p;
            float param_7 = 0.0199999995529651641845703125 + _137.uShParam.z;
            _425 = !sh_inside(param_5, param_6, param_7, _137);
        }
        else
        {
            _425 = _412;
        }
        if (_425)
        {
            float3 p2 = (vShWorld + (n * (_137.uShTexel_SunK[uint3(0u, 1u, 2u)[k + 1]] * 1.5))) + (_137.uShSun_On.xyz * _137.uShTexel_SunK[uint3(0u, 1u, 2u)[k + 1]]);
            float4 q = _137.uShCasc[k] * float4(p, 1.0);
            float2 e = fast::min(q.xy - float2(0.0199999995529651641845703125), float2(0.980000019073486328125) - q.xy);
            float t = fast::clamp(fast::min(e.x, e.y) / _137.uShParam.z, 0.0, 1.0);
            int param_8 = k + 1;
            float3 param_9 = p2;
            vis = mix(sh_tap(param_8, param_9, _137, uShMap, uShMapSmplr), vis, t);
        }
        break;
    }
    vis = 1.0 - ((1.0 - vis) * _137.uShKeyDir_Str.w);
    sh_vis = vis;
    vis = 1.0 - ((1.0 - vis) * _137.uShTexel_SunK.w);
    sh_state = float4(vis, facing, float(casc), 1.0);
}

static inline __attribute__((always_inline))
float3 sh_apply(thread const float3& c, thread float& sh_vis, thread float3& sh_n, constant ShadowUbo& _137, thread float4& sh_state)
{
    if (sh_state.w == 0.0)
    {
        return c;
    }
    if (_137.uShDebug == 2)
    {
        return float3(sh_state.x * fast::max(sh_state.y, 0.1500000059604644775390625));
    }
    float k = 1.0 - ((_137.uShParam.x * (1.0 - sh_state.x)) * sh_state.y);
    float3 o = c * k;
    if (_137.uShKey.w >= 0.0)
    {
        float ndg = dot(sh_n, _137.uShKeyDir_Str.xyz);
        float nds = dot(sh_n, _137.uShSun_On.xyz);
        float3 kg = _137.uShKey.xyz * fast::max(ndg, _137.uShKey.w * ndg);
        float3 ks = _137.uShKey.xyz * fast::max(nds, _137.uShKey.w * nds);
        o = fast::max(c + (((ks * sh_vis) - kg) * _137.uShTexel_SunK.w), float3(0.0));
    }
    if (_137.uShDebug == 1)
    {
        int ci = int(sh_state.z);
        float3 _1101;
        if (ci == 0)
        {
            _1101 = float3(1.0, 0.60000002384185791015625, 0.60000002384185791015625);
        }
        else
        {
            float3 _1109;
            if (ci == 1)
            {
                _1109 = float3(0.60000002384185791015625, 1.0, 0.60000002384185791015625);
            }
            else
            {
                _1109 = (ci == 2) ? float3(0.60000002384185791015625, 0.60000002384185791015625, 1.0) : float3(1.0);
            }
            _1101 = _1109;
        }
        float3 tint = _1101;
        o *= tint;
    }
    return o;
}

static inline __attribute__((always_inline))
float3 sh_points(thread const float3& c, thread float3& sh_n, constant ShadowUbo& _137, thread float3& vShWorld, texture2d<uint> uPtTiles, sampler uPtTilesSmplr, texturecube_array<float> uPtMap, sampler uPtMapSmplr)
{
    bool _515 = int(_137.uShSun_On.w + 0.5) == 0;
    bool _523;
    if (!_515)
    {
        _523 = _137.uPtN <= 0;
    }
    else
    {
        _523 = _515;
    }
    if (_523)
    {
        bool _529 = _137.uShDebug == 3;
        bool _537;
        if (_529)
        {
            _537 = int(_137.uShSun_On.w + 0.5) != 0;
        }
        else
        {
            _537 = _529;
        }
        return _537 ? float3(0.0) : c;
    }
    float3 add = float3(0.0);
    float4 cp = _137.uPtVP * float4(vShWorld, 1.0);
    int2 ts = int2(uPtTiles.get_width(), uPtTiles.get_height());
    float2 _566;
    if (cp.w > 0.0)
    {
        _566 = ((cp.xy / float2(cp.w)) * 0.5) + float2(0.5);
    }
    else
    {
        _566 = float2(0.5);
    }
    float2 t01 = _566;
    uint4 m = uPtTiles.read(uint2(clamp(int2(floor(t01 * float2(ts))), int2(0), ts - int2(1))), 0);
    int i;
    while (((m.x | m.y) | m.z) != 0u)
    {
        if (m.x != 0u)
        {
            i = int(spvFindLSB(m.x));
            m.x &= (m.x - 1u);
        }
        else
        {
            if (m.y != 0u)
            {
                i = 32 + int(spvFindLSB(m.y));
                m.y &= (m.y - 1u);
            }
            else
            {
                i = 64 + int(spvFindLSB(m.z));
                m.z &= (m.z - 1u);
            }
        }
        if (i >= _137.uPtN)
        {
            break;
        }
        float3 L = _137.uPtPos[i].xyz - vShWorld;
        float r = _137.uPtPos[i].w;
        float d2 = dot(L, L);
        if ((d2 >= (r * r)) || (d2 <= 0.0))
        {
            continue;
        }
        float inv_d = rsqrt(d2);
        float d = d2 * inv_d;
        float ndl_p = dot(sh_n, L) * inv_d;
        if (ndl_p <= 0.0)
        {
            continue;
        }
        float att = 1.0 - (d / r);
        att = mix(att, (att * att) * fast::max(L.z * inv_d, 0.0), _137.uPtDown[i >> 2][i & 3]);
        float sv = 1.0;
        if (_137.uPtCol[i].w >= 0.0)
        {
            float layer = floor(_137.uPtCol[i].w);
            float kw = fast::clamp((fract(_137.uPtCol[i].w) - 0.0500000007450580596923828125) / 0.89999997615814208984375, 0.0, 1.0);
            float3 v = (vShWorld + (sh_n * (0.039999999105930328369140625 + (0.02999999932944774627685546875 * d)))) - _137.uPtPos[i].xyz;
            float f = r;
            float nn = _137.uShPart_Near.w;
            float3 a = abs(v);
            float z = fast::max(fast::max(a.x, a.y), a.z);
            float ref = ((0.5 * (((f + nn) / (f - nn)) - (((2.0 * f) * nn) / ((f - nn) * z)))) + 0.5) - 0.00019999999494757503271102905273438;
            float o = 0.014999999664723873138427734375 * z;
            float4 _830 = float4(v + float3(o, o, 0.0), layer);
            float4 _844 = float4(v + float3(-o, 0.0, o), layer);
            float4 _860 = float4(v + float3(0.0, -o, -o), layer);
            float4 _876 = float4(v + float3(o, -o, o), layer);
            sv = 0.25 * (((spvDepthCast(uPtMap).sample_compare(uPtMapSmplr, _830.xyz, uint(rint(_830.w)), ref) + spvDepthCast(uPtMap).sample_compare(uPtMapSmplr, _844.xyz, uint(rint(_844.w)), ref)) + spvDepthCast(uPtMap).sample_compare(uPtMapSmplr, _860.xyz, uint(rint(_860.w)), ref)) + spvDepthCast(uPtMap).sample_compare(uPtMapSmplr, _876.xyz, uint(rint(_876.w)), ref));
            sv = mix(1.0, sv, kw);
        }
        add += (_137.uPtCol[i].xyz * (((ndl_p * att) * 0.5) * sv));
    }
    if (_137.uShDebug == 3)
    {
        return add;
    }
    return c + add;
}

static inline __attribute__((always_inline))
int sm_type(thread const uint& s, constant ShadowUbo& _137)
{
    bool _1219 = s == 0u;
    bool _1228;
    if (!_1219)
    {
        _1228 = int(_137.uShSun_On.w + 0.5) == 0;
    }
    else
    {
        _1228 = _1219;
    }
    bool _1237;
    if (!_1228)
    {
        _1237 = int(_137.uShCam_Pass.w + 0.5) != 0;
    }
    else
    {
        _1237 = _1228;
    }
    bool _1245;
    if (!_1237)
    {
        _1245 = _137.uMat[0].x == 0.0;
    }
    else
    {
        _1245 = _1237;
    }
    bool _1253;
    if (!_1245)
    {
        _1253 = _137.uEmis[s].w > 0.0;
    }
    else
    {
        _1253 = _1245;
    }
    if (_1253)
    {
        return 0;
    }
    return int(_137.uEmis[s].z + 0.5);
}

static inline __attribute__((always_inline))
float2 sh_mat_uv(thread const float2& st, thread const uint& s, constant ShadowUbo& _137, thread float3& vShWorld)
{
    uint param = s;
    int ty = sm_type(param, _137);
    if (((ty != 4) && (ty != 6)) && (ty != 7))
    {
        return st;
    }
    float _1284;
    if (ty == 4)
    {
        _1284 = 1.0;
    }
    else
    {
        float _1290;
        if (ty == 6)
        {
            _1290 = _137.uMat[s].y;
        }
        else
        {
            _1290 = _137.uMat[s].x;
        }
        _1284 = _1290;
    }
    float flow = _1284;
    float t = _137.uMat[0].y * ((ty == 4) ? 1.0 : 0.300000011920928955078125);
    float2 p = vShWorld.xy;
    float2 d = float2(sin((p.y * 0.20999999344348907470703125) + (t * 0.60000002384185791015625)) + (0.5 * sin((p.x * 0.37000000476837158203125) - (t * 0.430000007152557373046875))), cos((p.x * 0.189999997615814208984375) + (t * 0.5)) + (0.5 * cos((p.y * 0.310000002384185791015625) + (t * 0.709999978542327880859375))));
    return st + (d * (((ty == 4) ? 0.0199999995529651641845703125 : 0.01200000010430812835693359375) * flow));
}

static inline __attribute__((always_inline))
float3 sh_emissive(thread const float3& lit, thread const float3& orig, thread const float3& tex, thread const uint& s, constant ShadowUbo& _137)
{
    bool _910 = s == 0u;
    bool _919;
    if (!_910)
    {
        _919 = int(_137.uShSun_On.w + 0.5) == 0;
    }
    else
    {
        _919 = _910;
    }
    bool _928;
    if (!_919)
    {
        _928 = int(_137.uShCam_Pass.w + 0.5) != 0;
    }
    else
    {
        _928 = _919;
    }
    bool _936;
    if (!_928)
    {
        _936 = _137.uEmis[0].x == 0.0;
    }
    else
    {
        _936 = _928;
    }
    if (_936)
    {
        return lit;
    }
    float4 e = _137.uEmis[s];
    if (e.w > 0.0)
    {
        return mix(lit, float3(1.0, 0.0, 1.0), float3(e.x));
    }
    if (e.z > 0.0)
    {
        return lit;
    }
    float v = fast::max(tex.x, fast::max(tex.y, tex.z));
    float _977;
    if (e.y > 0.0)
    {
        _977 = smoothstep(e.y - 0.039999999105930328369140625, e.y + 0.039999999105930328369140625, v);
    }
    else
    {
        _977 = 1.0;
    }
    float m = _977;
    return mix(lit, fast::max(lit, orig * e.x), float3(m));
}

static inline __attribute__((always_inline))
float sm_ssr_dz(thread const float3& q, thread float2& uv, constant ShadowUbo& _137, texture2d<float> uSsrDepth, sampler uSsrDepthSmplr)
{
    float4 c = _137.uSsrVP * float4(q, 1.0);
    float2 _2088;
    if (c.w > 0.00999999977648258209228515625)
    {
        _2088 = ((c.xy / float2(c.w)) * 0.5) + float2(0.5);
    }
    else
    {
        _2088 = float2(-1.0);
    }
    uv = _2088;
    bool _2105 = uv.x < 0.0;
    bool _2112;
    if (!_2105)
    {
        _2112 = uv.y < 0.0;
    }
    else
    {
        _2112 = _2105;
    }
    bool _2119;
    if (!_2112)
    {
        _2119 = uv.x > 1.0;
    }
    else
    {
        _2119 = _2112;
    }
    bool _2126;
    if (!_2119)
    {
        _2126 = uv.y > 1.0;
    }
    else
    {
        _2126 = _2119;
    }
    if (_2126)
    {
        return -1000000000.0;
    }
    float d = uSsrDepth.sample(uSsrDepthSmplr, uv, level(0.0)).x;
    if (d >= 1.0)
    {
        return -1000000000.0;
    }
    return c.w - (_137.uSsrZ.y / (((d * 2.0) - 1.0) - _137.uSsrZ.x));
}

static inline __attribute__((always_inline))
float3 sm_ssr(thread const float3& P, thread const float3& R, thread const float3& miss, constant ShadowUbo& _137, texture2d<float> uSsrDepth, sampler uSsrDepthSmplr, texture2d<float> uSsrCol, sampler uSsrColSmplr)
{
    bool _2159 = _137.uMat[0].x < 1.5;
    bool _2166;
    if (!_2159)
    {
        _2166 = _137.uMat[0].w == 0.0;
    }
    else
    {
        _2166 = _2159;
    }
    if (_2166)
    {
        return miss;
    }
    float t0 = 0.0;
    float t = 0.20000000298023223876953125;
    float2 param_1;
    float2 param_3;
    float2 param_5;
    for (int i = 0; i < 24; i++)
    {
        float3 param = P + (R * t);
        float _2191 = sm_ssr_dz(param, param_1, _137, uSsrDepth, uSsrDepthSmplr);
        float2 uv = param_1;
        float dz = _2191;
        if (uv.x < 0.0)
        {
            return miss;
        }
        if (dz > 0.0)
        {
            for (int k = 0; k < 5; k++)
            {
                float tm = 0.5 * (t0 + t);
                float3 param_2 = P + (R * tm);
                float _2224 = sm_ssr_dz(param_2, param_3, _137, uSsrDepth, uSsrDepthSmplr);
                uv = param_3;
                if (_2224 > 0.0)
                {
                    t = tm;
                }
                else
                {
                    t0 = tm;
                }
            }
            float3 param_4 = P + (R * t);
            float _2241 = sm_ssr_dz(param_4, param_5, _137, uSsrDepth, uSsrDepthSmplr);
            uv = param_5;
            dz = _2241;
            if (uv.x < 0.0)
            {
                return miss;
            }
            float th = 0.300000011920928955078125 + (0.0500000007450580596923828125 * t);
            float2 e = smoothstep(float2(0.0), float2(0.07999999821186065673828125), uv) * smoothstep(float2(0.0), float2(0.07999999821186065673828125), float2(1.0) - uv);
            return mix(miss, uSsrCol.sample(uSsrColSmplr, uv, level(0.0)).xyz, float3((e.x * e.y) * (1.0 - smoothstep(th, 3.0 * th, dz))));
        }
        t0 = t;
        t = (t * 1.25) + 0.1500000059604644775390625;
    }
    return miss;
}

static inline __attribute__((always_inline))
float3 sm_spec(thread const float3& N, thread const float3& V, thread const float& rough, thread const float3& F0, thread float& sh_vis, constant ShadowUbo& _137, thread float3& vShWorld, texture2d<uint> uPtTiles, sampler uPtTilesSmplr)
{
    float r = fast::clamp(rough, 0.02999999932944774627685546875, 1.0);
    float pw = fast::clamp((2.0 / (((r * r) * r) * r)) - 2.0, 1.0, 4096.0);
    float norm = (pw + 8.0) / 25.1326999664306640625;
    float3 sum = float3(0.0);
    bool _1763 = _137.uShKey.w >= 0.0;
    bool _1769;
    if (_1763)
    {
        _1769 = _137.uShTexel_SunK.w > 0.0;
    }
    else
    {
        _1769 = _1763;
    }
    if (_1769)
    {
        float3 H = fast::normalize(_137.uShSun_On.xyz + V);
        float ndl = fast::max(dot(N, _137.uShSun_On.xyz), 0.0);
        float ndh = fast::max(dot(N, H), 0.0);
        float vdh = fast::max(dot(V, H), 0.0);
        float vis = fast::clamp(1.0 - ((1.0 - sh_vis) / fast::max(_137.uShKeyDir_Str.w, 0.00999999977648258209228515625)), 0.0, 1.0);
        float3 F = F0 + ((float3(1.0) - F0) * powr(1.0 - vdh, 5.0));
        sum += (((_137.uShKey.xyz * 1.9921875) * ((((_137.uShTexel_SunK.w * vis) * norm) * powr(ndh, pw)) * ndl)) * F);
    }
    if (_137.uPtN > 0)
    {
        float4 cp = _137.uPtVP * float4(vShWorld, 1.0);
        int2 ts = int2(uPtTiles.get_width(), uPtTiles.get_height());
        float2 _1861;
        if (cp.w > 0.0)
        {
            _1861 = ((cp.xy / float2(cp.w)) * 0.5) + float2(0.5);
        }
        else
        {
            _1861 = float2(0.5);
        }
        float2 t01 = _1861;
        uint4 m = uPtTiles.read(uint2(clamp(int2(floor(t01 * float2(ts))), int2(0), ts - int2(1))), 0);
        int i;
        while (((m.x | m.y) | m.z) != 0u)
        {
            if (m.x != 0u)
            {
                i = int(spvFindLSB(m.x));
                m.x &= (m.x - 1u);
            }
            else
            {
                if (m.y != 0u)
                {
                    i = 32 + int(spvFindLSB(m.y));
                    m.y &= (m.y - 1u);
                }
                else
                {
                    i = 64 + int(spvFindLSB(m.z));
                    m.z &= (m.z - 1u);
                }
            }
            if (i >= _137.uPtN)
            {
                break;
            }
            float3 L = _137.uPtPos[i].xyz - vShWorld;
            float rr = _137.uPtPos[i].w;
            float d2 = dot(L, L);
            if ((d2 >= (rr * rr)) || (d2 <= 0.0))
            {
                continue;
            }
            float inv_d = rsqrt(d2);
            L *= inv_d;
            float ndl_1 = dot(N, L);
            if (ndl_1 <= 0.0)
            {
                continue;
            }
            float att = 1.0 - ((d2 * inv_d) / rr);
            float3 H_1 = fast::normalize(L + V);
            float ndh_1 = fast::max(dot(N, H_1), 0.0);
            float vdh_1 = fast::max(dot(V, H_1), 0.0);
            float3 F_1 = F0 + ((float3(1.0) - F0) * powr(1.0 - vdh_1, 5.0));
            sum += ((_137.uPtCol[i].xyz * (((((0.5 * att) * att) * norm) * powr(ndh_1, pw)) * ndl_1)) * F_1);
        }
    }
    return sum;
}

static inline __attribute__((always_inline))
float3 sm_env(thread const float3& R, thread const float& rough, constant ShadowUbo& _137)
{
    float up = fast::clamp((R.z * (2.0 - rough)) + 0.25, 0.0, 1.0);
    return _137.uSsrSky.xyz * mix(0.300000011920928955078125, 1.0, (up * up) * (3.0 - (2.0 * up)));
}

static inline __attribute__((always_inline))
float sm_hash(thread const float2& p)
{
    uint2 c = uint2(int2(floor(p * 16.0)));
    uint h = (c.x * 2376512323u) ^ (c.y * 3625334849u);
    h ^= (h >> uint(16));
    h *= 2146121005u;
    h ^= (h >> uint(15));
    h *= 2221713035u;
    h ^= (h >> uint(16));
    return float(h >> uint(8)) * 5.9604644775390625e-08;
}

static inline __attribute__((always_inline))
float sm_noise(thread const float2& p)
{
    float2 i = floor(p);
    float2 f = fract(p);
    float2 u = (f * f) * (float2(3.0) - (f * 2.0));
    float2 param = i;
    float2 param_1 = i + float2(1.0, 0.0);
    float2 param_2 = i + float2(0.0, 1.0);
    float2 param_3 = i + float2(1.0);
    return mix(mix(sm_hash(param), sm_hash(param_1), u.x), mix(sm_hash(param_2), sm_hash(param_3), u.x), u.y);
}

static inline __attribute__((always_inline))
float3 sm_waves(thread const float3& n, thread const float3& P, thread const float& amt, thread const float& t)
{
    float2 d1 = float2(0.800000011920928955078125, 0.60000002384185791015625);
    float2 d2 = float2(-0.4469999969005584716796875, 0.893999993801116943359375);
    float2 d3 = float2(0.19599999487400054931640625, -0.981000006198883056640625);
    float2 g = d1 * (0.064999997615814208984375 * cos((dot(d1, P.xy) * 1.2999999523162841796875) + (t * 1.7000000476837158203125)));
    g += (d2 * (0.063000001013278961181640625 * cos((dot(d2, P.xy) * 2.099999904632568359375) - (t * 2.2999999523162841796875))));
    g += (d3 * (0.054999999701976776123046875 * cos((dot(d3, P.xy) * 3.7000000476837158203125) + (t * 3.099999904632568359375))));
    float2 param = (P.xy * 2.2999999523162841796875) + float2(t * 0.60000002384185791015625);
    float2 param_1 = ((P.xy * 2.2999999523162841796875) - float2(t * 0.5)) + float2(9.1000003814697265625);
    g += (float2(sm_noise(param) - 0.5, sm_noise(param_1) - 0.5) * 0.119999997317790985107421875);
    return fast::normalize(n - (float3(g, 0.0) * (amt * smoothstep(0.300000011920928955078125, 0.699999988079071044921875, n.z))));
}

static inline __attribute__((always_inline))
float sm_sky(thread const float3& p, texture2d<float> uMatSky, sampler uMatSkySmplr)
{
    int2 n = int2(uMatSky.get_width(), uMatSky.get_height());
    if (n.y < 2)
    {
        return 1.0;
    }
    n.y--;
    float4 w = float4(uMatSky.read(uint2(int2(0, n.y)), 0).x, uMatSky.read(uint2(int2(1, n.y)), 0).x, uMatSky.read(uint2(int2(2, n.y)), 0).x, uMatSky.read(uint2(int2(3, n.y)), 0).x);
    bool _1520 = w.w < 0.5;
    bool _1527;
    if (!_1520)
    {
        _1527 = w.z <= 0.0;
    }
    else
    {
        _1527 = _1520;
    }
    if (_1527)
    {
        return 1.0;
    }
    float2 g = ((p.xy - w.xy) / float2(w.z)) - float2(0.5);
    int2 i0 = int2(floor(g));
    float2 f = g - float2(i0);
    float _1580;
    float4 o;
    for (int k = 0; k < 4; k++)
    {
        int2 c = i0 + int2(k & 1, k >> 1);
        bool _1571 = any(c < int2(0));
        bool _1579;
        if (!_1571)
        {
            _1579 = any(c >= n);
        }
        else
        {
            _1579 = _1571;
        }
        if (_1579)
        {
            _1580 = -1000000.0;
        }
        else
        {
            _1580 = uMatSky.read(uint2(c), 0).x;
        }
        float h = _1580;
        o[k] = smoothstep(h - 0.25, h + 0.25, p.z + 0.75);
    }
    return mix(mix(o.x, o.y, f.x), mix(o.z, o.w, f.x), f.y);
}

static inline __attribute__((always_inline))
float3 sm_rain(thread const float3& n, thread const float3& P, thread const float& t, thread const float& k)
{
    float2 g = float2(0.0);
    float2 _1697;
    for (int l = 0; l < 2; l++)
    {
        float2 q = (P.xy * 1.60000002384185791015625) + float2(float(l) * 0.37000000476837158203125, float(l) * 0.709999978542327880859375);
        float2 c = floor(q);
        float2 param = c + float2(float(l) * 17.0);
        float ph = fract((t * 0.89999997615814208984375) + sm_hash(param));
        float2 param_1 = c + float2(3.099999904632568359375);
        float2 param_2 = c + float2(7.69999980926513671875);
        float2 v = q - ((c + float2(0.25)) + (float2(sm_hash(param_1), sm_hash(param_2)) * 0.5));
        float d = length(v);
        float x = (d - (ph * 0.60000002384185791015625)) * 18.0;
        if (d > 9.9999997473787516355514526367188e-05)
        {
            _1697 = v / float2(d);
        }
        else
        {
            _1697 = float2(0.0);
        }
        g += (_1697 * (((cos(x * 3.0) * exp((-x) * x)) * (1.0 - ph)) * 0.60000002384185791015625));
    }
    return fast::normalize(n - (float3(g, 0.0) * (k * smoothstep(0.60000002384185791015625, 0.89999997615814208984375, n.z))));
}

static inline __attribute__((always_inline))
float3 sm_shimmer(thread const float3& c, thread const float3& P, thread const float& t, constant ShadowUbo& _137, texture2d<float> uSsrCol, sampler uSsrColSmplr)
{
    bool _2297 = _137.uMat[0].x < 1.5;
    bool _2304;
    if (!_2297)
    {
        _2304 = _137.uMat[0].w == 0.0;
    }
    else
    {
        _2304 = _2297;
    }
    if (_2304)
    {
        return c;
    }
    float4 q = _137.uSsrVP * float4(P, 1.0);
    if (q.w <= 0.00999999977648258209228515625)
    {
        return c;
    }
    float2 uv = ((q.xy / float2(q.w)) * 0.5) + float2(0.5);
    float2 param = (P.xy * 1.7000000476837158203125) + float2(0.0, (-t) * 1.89999997615814208984375);
    float2 param_1 = (P.xy * 1.2999999523162841796875) + float2(t * 1.2999999523162841796875, 5.0);
    float2 o = float2(sm_noise(param) - 0.5, sm_noise(param_1) - 0.5) * 0.01400000043213367462158203125;
    return mix(c, uSsrCol.sample(uSsrColSmplr, fast::clamp(uv + o, float2(0.0), float2(1.0)), level(0.0)).xyz, float3(0.3499999940395355224609375));
}

static inline __attribute__((always_inline))
float3 sh_material(thread const float3& lit, thread const float3& orig, thread const float3& tex, thread const float3& light, thread const uint& s, thread float& sh_vis, thread float3& sh_n, constant ShadowUbo& _137, thread float3& vShWorld, texture2d<uint> uPtTiles, sampler uPtTilesSmplr, texture2d<float> uMatSky, sampler uMatSkySmplr, texture2d<float> uSsrDepth, sampler uSsrDepthSmplr, texture2d<float> uSsrCol, sampler uSsrColSmplr)
{
    uint param = s;
    int ty = sm_type(param, _137);
    if (ty == 0)
    {
        return lit;
    }
    float4 m = _137.uMat[s];
    float t = _137.uMat[0].y;
    float rain = _137.uMat[0].z;
    float3 P = vShWorld;
    float3 N = sh_n;
    float3 V = fast::normalize(_137.uShCam_Pass.xyz - P);
    float3 A = fast::clamp(tex * 1.9921875, float3(0.0), float3(1.0));
    float3 L = fast::clamp(light, float3(0.0), float3(1.5));
    float ndv = fast::max(dot(N, V), 0.0);
    float3 R = reflect(-V, N);
    if (ty == 1)
    {
        float r = fast::clamp(m.x, 0.02999999932944774627685546875, 1.0);
        float gloss = 1.0 - r;
        float3 F0 = mix(float3(0.039999999105930328369140625), A, float3(0.85000002384185791015625));
        float3 Fv = F0 + ((float3(1.0) - F0) * powr(1.0 - ndv, 5.0));
        float up = fast::clamp((R.z * mix(1.0, 6.0, gloss * gloss)) + 0.5, 0.0, 1.0);
        float3 env = (_137.uSsrSky.xyz * mix(0.25, 1.14999997615814208984375, (up * up) * (3.0 - (2.0 * up)))) * L;
        if (gloss > 0.4000000059604644775390625)
        {
            float3 param_1 = P + (N * 0.02999999932944774627685546875);
            float3 param_2 = R;
            float3 param_3 = env;
            env = mix(env, sm_ssr(param_1, param_2, param_3, _137, uSsrDepth, uSsrDepthSmplr, uSsrCol, uSsrColSmplr), float3(smoothstep(0.4000000059604644775390625, 0.75, gloss)));
        }
        float3 mirror = (env * Fv) * mix(0.3499999940395355224609375, 1.5, gloss * gloss);
        float3 param_4 = N;
        float3 param_5 = V;
        float param_6 = r;
        float3 param_7 = F0;
        return (lit * (1.0 - (m.y * mix(0.25, 0.550000011920928955078125, gloss)))) + ((sm_spec(param_4, param_5, param_6, param_7, sh_vis, _137, vShWorld, uPtTiles, uPtTilesSmplr) + mirror) * m.y);
    }
    if ((ty == 2) || (ty == 3))
    {
        float _2544;
        if (ty == 2)
        {
            _2544 = m.x;
        }
        else
        {
            _2544 = m.z;
        }
        float wet = _2544;
        float _2556;
        if (ty == 2)
        {
            _2556 = m.y;
        }
        else
        {
            _2556 = 0.4000000059604644775390625;
        }
        float rough = _2556;
        float Fw = 0.039999999105930328369140625 + (0.959999978542327880859375 * powr(1.0 - ndv, 5.0));
        float3 param_8 = N;
        float3 param_9 = V;
        float param_10 = rough;
        float3 param_11 = float3(0.039999999105930328369140625);
        float3 param_12 = R;
        float param_13 = rough;
        float3 base = (lit * (1.0 - (0.4000000059604644775390625 * wet))) + ((sm_spec(param_8, param_9, param_10, param_11, sh_vis, _137, vShWorld, uPtTiles, uPtTilesSmplr) + ((sm_env(param_12, param_13, _137) * L) * (Fw * 0.800000011920928955078125))) * wet);
        if (ty == 2)
        {
            return base;
        }
        float2 param_14 = P.xy * m.y;
        float2 param_15 = (P.xy * (m.y * 2.7000000476837158203125)) + float2(7.30000019073486328125);
        float n = (sm_noise(param_14) * 0.64999997615814208984375) + (sm_noise(param_15) * 0.3499999940395355224609375);
        float mask = smoothstep((1.0 - m.x) - 0.039999999105930328369140625, (1.0 - m.x) + 0.039999999105930328369140625, n) * smoothstep(0.85000002384185791015625, 0.9700000286102294921875, N.z);
        if (mask <= 0.0)
        {
            return base;
        }
        float3 param_16 = N;
        float3 param_17 = P;
        float param_18 = 0.0500000007450580596923828125;
        float param_19 = t;
        float3 Np = sm_waves(param_16, param_17, param_18, param_19);
        if (rain > 0.0)
        {
            float3 param_20 = P;
            float3 param_21 = Np;
            float3 param_22 = P;
            float param_23 = t;
            float param_24 = (rain * 0.60000002384185791015625) * sm_sky(param_20, uMatSky, uMatSkySmplr);
            Np = sm_rain(param_21, param_22, param_23, param_24);
        }
        float3 Rp = reflect(-V, Np);
        float F = mix(0.1500000059604644775390625, 0.89999997615814208984375, powr(1.0 - fast::max(dot(Np, V), 0.0), 4.0));
        float3 param_25 = Rp;
        float param_26 = 0.0500000007450580596923828125;
        float3 param_27 = P + (N * 0.02999999932944774627685546875);
        float3 param_28 = Rp;
        float3 param_29 = sm_env(param_25, param_26, _137) * L;
        float3 refl = sm_ssr(param_27, param_28, param_29, _137, uSsrDepth, uSsrDepthSmplr, uSsrCol, uSsrColSmplr);
        float3 param_30 = Np;
        float3 param_31 = V;
        float param_32 = 0.0599999986588954925537109375;
        float3 param_33 = float3(0.0199999995529651641845703125);
        float3 pud = ((lit * (0.4000000059604644775390625 * (1.0 - F))) + (refl * F)) + sm_spec(param_30, param_31, param_32, param_33, sh_vis, _137, vShWorld, uPtTiles, uPtTilesSmplr);
        return mix(base, pud, float3(mask));
    }
    if (ty == 4)
    {
        float2 param_34 = P.xy * 0.07999999821186065673828125;
        float pulse = 1.0 + (m.y * sin((t * 0.800000011920928955078125) + (sm_noise(param_34) * 6.283199787139892578125)));
        float v = fast::max(tex.x, fast::max(tex.y, tex.z));
        float th = m.z;
        float _2768;
        if (th > 0.0)
        {
            _2768 = smoothstep(th - 0.039999999105930328369140625, th + 0.039999999105930328369140625, v);
        }
        else
        {
            _2768 = 1.0;
        }
        float k = _2768;
        float3 c = mix(lit, fast::max(lit, orig * (m.x * pulse)), float3(k));
        float3 param_35 = c;
        float3 param_36 = P;
        float param_37 = t;
        return sm_shimmer(param_35, param_36, param_37, _137, uSsrCol, uSsrColSmplr);
    }
    if (ty == 5)
    {
        float3 param_38 = N;
        float3 param_39 = P;
        float param_40 = m.y;
        float param_41 = t;
        float3 Nw = sm_waves(param_38, param_39, param_40, param_41);
        if (rain > 0.0)
        {
            float3 param_42 = P;
            float3 param_43 = Nw;
            float3 param_44 = P;
            float param_45 = t;
            float param_46 = (rain * 0.5) * sm_sky(param_42, uMatSky, uMatSkySmplr);
            Nw = sm_rain(param_43, param_44, param_45, param_46);
        }
        float3 Rw = reflect(-V, Nw);
        float F_1 = 0.0199999995529651641845703125 + (0.980000019073486328125 * powr(1.0 - fast::max(dot(Nw, V), 0.0), 5.0));
        F_1 = mix(0.1500000059604644775390625, 1.0, F_1);
        float3 param_47 = Rw;
        float param_48 = 0.0500000007450580596923828125;
        float3 param_49 = P + (N * 0.02999999932944774627685546875);
        float3 param_50 = Rw;
        float3 param_51 = sm_env(param_47, param_48, _137) * L;
        float3 refl_1 = sm_ssr(param_49, param_50, param_51, _137, uSsrDepth, uSsrDepthSmplr, uSsrCol, uSsrColSmplr);
        float3 param_52 = Nw;
        float3 param_53 = V;
        float param_54 = 0.070000000298023223876953125;
        float3 param_55 = float3(0.0199999995529651641845703125);
        return (lit * (1.0 - ((0.3499999940395355224609375 * F_1) * m.x))) + (((refl_1 * (F_1 * 0.699999988079071044921875)) + (sm_spec(param_52, param_53, param_54, param_55, sh_vis, _137, vShWorld, uPtTiles, uPtTilesSmplr) * 1.5)) * m.x);
    }
    float _2895;
    if (ty == 6)
    {
        _2895 = m.x;
    }
    else
    {
        _2895 = m.y;
    }
    float g = _2895;
    float Fm = 0.039999999105930328369140625 + (0.959999978542327880859375 * powr(1.0 - ndv, 5.0));
    float3 param_56 = N;
    float3 param_57 = V;
    float param_58 = (ty == 6) ? 0.37999999523162841796875 : 0.5;
    float3 param_59 = float3(0.039999999105930328369140625);
    float3 param_60 = R;
    float param_61 = 0.5;
    return (lit * (1.0 - (0.25 * g))) + (((sm_spec(param_56, param_57, param_58, param_59, sh_vis, _137, vShWorld, uPtTiles, uPtTilesSmplr) * 0.800000011920928955078125) + ((sm_env(param_60, param_61, _137) * L) * (Fm * 0.5))) * g);
}

[[ early_fragment_tests ]] fragment kMrpWorldEarlyFs_out kMrpWorldEarlyFs(kMrpWorldEarlyFs_in in [[stage_in]], constant spvDescriptorSetBuffer2& spvDescriptorSet2 [[buffer(2)]], constant ShadowUbo& _137 [[buffer(3)]], constant Push& _3090 [[buffer(4)]], texture2d_array<float> uShMap [[texture(0)]], texture2d<uint> uPtTiles [[texture(1)]], texturecube_array<float> uPtMap [[texture(2)]], texture2d<float> uMatSky [[texture(3)]], texture2d<float> uSsrDepth [[texture(4)]], texture2d<float> uSsrCol [[texture(5)]], sampler uShMapSmplr [[sampler(0)]], sampler uPtTilesSmplr [[sampler(1)]], sampler uPtMapSmplr [[sampler(2)]], sampler uMatSkySmplr [[sampler(3)]], sampler uSsrDepthSmplr [[sampler(4)]], sampler uSsrColSmplr [[sampler(5)]])
{
    kMrpWorldEarlyFs_out out = {};
    float sh_vis = 1.0;
    float3 sh_n = float3(0.0, 0.0, 1.0);
    float4 sh_state;
    sh_prepare(sh_vis, sh_n, _137, uShMap, uShMapSmplr, sh_state, in.vShWorld);
    uint fl = in.vState.x;
    int uTme = int(fl & 1u);
    int uIip = int((fl >> uint(1)) & 1u);
    int uAte = int((fl >> uint(3)) & 1u);
    int uAtst = int((fl >> uint(4)) & 7u);
    int uAref = int((fl >> uint(8)) & 255u);
    uint unit = (fl >> uint(16)) & 15u;
    if ((int(_137.uShCam_Pass.w + 0.5) != 0) && (uAte == 0))
    {
        return out;
    }
    float4 c = (uIip != 0) ? in.vColor : in.vFlat;
    float3 c_game = c.xyz;
    float3 param = c.xyz;
    float3 _3006 = sh_apply(param, sh_vis, sh_n, _137, sh_state);
    c.x = _3006.x;
    c.y = _3006.y;
    c.z = _3006.z;
    float3 param_1 = c.xyz;
    float3 _3016 = sh_points(param_1, sh_n, _137, in.vShWorld, uPtTiles, uPtTilesSmplr, uPtMap, uPtMapSmplr);
    c.x = _3016.x;
    c.y = _3016.y;
    c.z = _3016.z;
    float4 o = c;
    uint w = in.vTex.x;
    float uLayer = float(in.vTex.y);
    if (uTme != 0)
    {
        float2 st = in.vStq.xy / float2(in.vStq.z);
        float2 st_dx = dfdx(st);
        float2 st_dy = dfdy(st);
        float2 param_2 = st;
        uint param_3 = (w >> uint(25)) & 31u;
        st = sh_mat_uv(param_2, param_3, _137, in.vShWorld);
        float4 t;
        if ((w & 1u) != 0u)
        {
            t = float4(0.501960813999176025390625);
        }
        else
        {
            int mips = int((w >> uint(22)) & 7u) + 1;
            float _3123;
            int _3145;
            for (;;)
            {
                uint u = spvSubgroupBroadcastFirst(unit);
                if (u != unit)
                {
                    continue;
                }
                if (_3090.uMipMode == 0)
                {
                    int mxl = int((w >> uint(4)) & 7u);
                    int lcm = int((w >> uint(7)) & 1u);
                    int L = int((w >> uint(8)) & 3u);
                    float K = float((int(w >> uint(10)) << 20) >> 20) / 16.0;
                    if (lcm != 0)
                    {
                        _3123 = K;
                    }
                    else
                    {
                        _3123 = (log2(1.0 / fast::max(abs(in.vLodQ), 9.9999996826552253889678874634872e-21)) * float(1 << L)) + K;
                    }
                    float lod = _3123;
                    if (lod <= 0.0)
                    {
                        _3145 = 0;
                    }
                    else
                    {
                        _3145 = min(int(floor(lod + 0.5)), mxl);
                    }
                    int level0 = _3145;
                    level0 = min(level0, (mips - 1));
                    float3 _3174 = float3(st, uLayer);
                    t = spvDescriptorSet2.uTexs[u].sample(spvDescriptorSet2.uTexsSmplr[u], _3174.xy, uint(rint(_3174.z)), level(float(level0)));
                }
                else
                {
                    float2 tsz = float2(int3(spvDescriptorSet2.uTexs[u].get_width(), spvDescriptorSet2.uTexs[u].get_height(), spvDescriptorSet2.uTexs[u].get_array_size()).xy);
                    float2 ddx = st_dx * tsz;
                    float2 ddy = st_dy * tsz;
                    float lod_1 = fast::clamp(0.5 * log2(fast::max(dot(ddx, ddx), dot(ddy, ddy))), 0.0, float(mips - 1));
                    float3 _3216 = float3(st, uLayer);
                    t = spvDescriptorSet2.uTexs[u].sample(spvDescriptorSet2.uTexsSmplr[u], _3216.xy, uint(rint(_3216.z)), level(lod_1));
                }
                break;
            }
        }
        uint tfx = (w >> uint(1)) & 3u;
        uint tcc = (w >> uint(3)) & 1u;
        if (tfx == 0u)
        {
            float3 _3237 = (t.xyz * c.xyz) * 1.9921875;
            o.x = _3237.x;
            o.y = _3237.y;
            o.z = _3237.z;
            float _3246;
            if (tcc != 0u)
            {
                _3246 = (t.w * c.w) * 1.9921875;
            }
            else
            {
                _3246 = c.w;
            }
            o.w = _3246;
        }
        else
        {
            if (tfx == 1u)
            {
                o.x = t.xyz.x;
                o.y = t.xyz.y;
                o.z = t.xyz.z;
                float _3275;
                if (tcc != 0u)
                {
                    _3275 = t.w;
                }
                else
                {
                    _3275 = c.w;
                }
                o.w = _3275;
            }
            else
            {
                if (tfx == 2u)
                {
                    float3 _3299 = ((t.xyz * c.xyz) * 1.9921875) + float3(c.w);
                    o.x = _3299.x;
                    o.y = _3299.y;
                    o.z = _3299.z;
                    float _3308;
                    if (tcc != 0u)
                    {
                        _3308 = t.w + c.w;
                    }
                    else
                    {
                        _3308 = c.w;
                    }
                    o.w = _3308;
                }
                else
                {
                    float3 _3331 = ((t.xyz * c.xyz) * 1.9921875) + float3(c.w);
                    o.x = _3331.x;
                    o.y = _3331.y;
                    o.z = _3331.z;
                    float _3340;
                    if (tcc != 0u)
                    {
                        _3340 = t.w;
                    }
                    else
                    {
                        _3340 = c.w;
                    }
                    o.w = _3340;
                }
            }
        }
        float3 _3352;
        if (tfx == 1u)
        {
            _3352 = t.xyz;
        }
        else
        {
            _3352 = (t.xyz * c_game) * 1.9921875;
        }
        float3 param_4 = o.xyz;
        float3 param_5 = _3352;
        float3 param_6 = t.xyz;
        uint param_7 = (w >> uint(25)) & 31u;
        float3 _3375 = sh_emissive(param_4, param_5, param_6, param_7, _137);
        o.x = _3375.x;
        o.y = _3375.y;
        o.z = _3375.z;
        float3 _3384;
        if (tfx == 1u)
        {
            _3384 = t.xyz;
        }
        else
        {
            _3384 = (t.xyz * c_game) * 1.9921875;
        }
        float3 _3397;
        if (tfx == 1u)
        {
            _3397 = float3(1.0);
        }
        else
        {
            _3397 = c.xyz * 1.9921875;
        }
        float3 param_8 = o.xyz;
        float3 param_9 = _3384;
        float3 param_10 = t.xyz;
        float3 param_11 = _3397;
        uint param_12 = (w >> uint(25)) & 31u;
        float3 _3418 = sh_material(param_8, param_9, param_10, param_11, param_12, sh_vis, sh_n, _137, in.vShWorld, uPtTiles, uPtTilesSmplr, uMatSky, uMatSkySmplr, uSsrDepth, uSsrDepthSmplr, uSsrCol, uSsrColSmplr);
        o.x = _3418.x;
        o.y = _3418.y;
        o.z = _3418.z;
        if (_3090.uShade == 2)
        {
            o.x = t.xyz.x;
            o.y = t.xyz.y;
            o.z = t.xyz.z;
        }
    }
    if (_3090.uShade == 1)
    {
        o.x = c.xyz.x;
        o.y = c.xyz.y;
        o.z = c.xyz.z;
    }
    else
    {
        if (_3090.uShade == 3)
        {
            o.x = 0.800000011920928955078125;
            o.y = 0.800000011920928955078125;
            o.z = 0.800000011920928955078125;
        }
    }
    o = fast::clamp(o, float4(0.0), float4(1.0));
    if (_3090.uFge != 0)
    {
        float f = in.vFog / 255.0;
        float4 _3478 = o;
        float3 _3489 = (_3478.xyz * f) + (_3090.uFogCol_Pad.xyz * (1.0 - f));
        o.x = _3489.x;
        o.y = _3489.y;
        o.z = _3489.z;
    }
    float a = o.w * 255.0;
    if (uAte != 0)
    {
        bool pass = true;
        if (uAtst == 0)
        {
            pass = false;
        }
        else
        {
            if (uAtst == 2)
            {
                pass = a < float(uAref);
            }
            else
            {
                if (uAtst == 3)
                {
                    pass = a <= float(uAref);
                }
                else
                {
                    if (uAtst == 4)
                    {
                        pass = abs(a - float(uAref)) < 0.5;
                    }
                    else
                    {
                        if (uAtst == 5)
                        {
                            pass = a >= float(uAref);
                        }
                        else
                        {
                            if (uAtst == 6)
                            {
                                pass = a > float(uAref);
                            }
                            else
                            {
                                if (uAtst == 7)
                                {
                                    pass = abs(a - float(uAref)) >= 0.5;
                                }
                            }
                        }
                    }
                }
            }
        }
        if ((int(_137.uShCam_Pass.w + 0.5) != 0) && (!pass))
        {
            discard_fragment();
        }
        if ((_3090.uAtPass == 1) && (!pass))
        {
            discard_fragment();
        }
        if ((_3090.uAtPass == 2) && pass)
        {
            discard_fragment();
        }
    }
    out.FragColor = float4(o.xyz, fast::min(o.w * 1.9921875, 1.0));
    return out;
}

