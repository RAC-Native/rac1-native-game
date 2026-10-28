// kMrpVu7Fs (frag): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
// rac1: flat inputs (the triangles drawn last vertex first)
#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

// Implementation of signed integer mod accurate to SPIR-V specification
template<typename Tx, typename Ty>
inline Tx spvSMod(Tx x, Ty y)
{
    Tx remainder = x - y * (x / y);
    return select(Tx(remainder + y), remainder, remainder == 0 || (x >= 0) == (y >= 0));
}

// Implementation of the GLSL findLSB() function
template<typename T>
inline T spvFindLSB(T x)
{
    return select(ctz(x), T(-1), x == T(0));
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
    int uShade;
    int uPass;
    int _pad0;
    int _pad1;
};

struct kMrpVu7Fs_out
{
    float4 FragColor [[color(0), index(0)]];
    float4 FragFactor [[color(0), index(1)]];
};

struct kMrpVu7Fs_in
{
    float3 vStq [[user(locn0)]];
    float4 vColor [[user(locn1), center_no_perspective]];
    float4 vFlat [[user(locn2), flat]];
    float vFogW [[user(locn3), center_no_perspective]];
    float2 vFogC [[user(locn4), flat]];
    uint4 vTex [[user(locn5)]];
    uint vEmis [[user(locn6)]];
    float3 vShWorld [[user(locn7)]];
};

static inline __attribute__((always_inline))
bool sh_inside(thread const int& k, thread const float3& p, thread const float& m, constant ShadowUbo& _149)
{
    float4 q = _149.uShCasc[k] * float4(p, 1.0);
    bool _260 = all(q.xy > float2(m));
    bool _270;
    if (_260)
    {
        _270 = all(q.xy < float2(1.0 - m));
    }
    else
    {
        _270 = _260;
    }
    bool _276;
    if (_270)
    {
        _276 = q.z < 1.0;
    }
    else
    {
        _276 = _270;
    }
    return _276;
}

static inline __attribute__((always_inline))
float sh_tap(thread const int& k, thread const float3& p, constant ShadowUbo& _149, texture2d_array<float> uShMap, sampler uShMapSmplr)
{
    float4 q = _149.uShCasc[k] * float4(p, 1.0);
    float r = _149.uShParam.w / (float(int3(uShMap.get_width(), uShMap.get_height(), uShMap.get_array_size()).x) * _149.uShPart_Near[uint3(0u, 1u, 2u)[k]]);
    float s = 0.0;
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            float4 _230 = float4((q.xy + (float2(float(x), float(y)) * r)) * _149.uShPart_Near[uint3(0u, 1u, 2u)[k]], float(k), q.z);
            s += spvDepthCast(uShMap).sample_compare(uShMapSmplr, _230.xy, uint(rint(_230.z)), _230.w);
        }
    }
    return s / 9.0;
}

static inline __attribute__((always_inline))
void sh_prepare(thread float& sh_vis, thread float3& sh_n, constant ShadowUbo& _149, texture2d_array<float> uShMap, sampler uShMapSmplr, thread float4& sh_state, thread float3& vShWorld)
{
    sh_state = float4(1.0, 0.0, -1.0, 0.0);
    bool _289 = int(_149.uShCam_Pass.w + 0.5) != 0;
    bool _299;
    if (!_289)
    {
        _299 = int(_149.uShSun_On.w + 0.5) == 0;
    }
    else
    {
        _299 = _289;
    }
    if (_299)
    {
        return;
    }
    float3 dx = dfdx(vShWorld);
    float3 dy = dfdy(vShWorld);
    float3 n = cross(dx, dy);
    float nl = length(n);
    float3 _320;
    if (nl > 0.0)
    {
        _320 = n / float3(nl);
    }
    else
    {
        _320 = float3(0.0, 0.0, 1.0);
    }
    n = _320;
    if (dot(n, _149.uShCam_Pass.xyz - vShWorld) < 0.0)
    {
        n = -n;
    }
    sh_n = n;
    float ndl = dot(n, _149.uShSun_On.xyz);
    float facing = smoothstep(0.0, 0.25, ndl);
    int count = int(_149.uShParam.y);
    float vis = 1.0;
    int casc = -1;
    for (int k = 0; k < count; k++)
    {
        float3 p = (vShWorld + (n * (_149.uShTexel_SunK[uint3(0u, 1u, 2u)[k]] * 1.5))) + (_149.uShSun_On.xyz * (_149.uShTexel_SunK[uint3(0u, 1u, 2u)[k]] * 1.0));
        int param = k;
        float3 param_1 = p;
        float param_2 = 0.0199999995529651641845703125;
        if (!sh_inside(param, param_1, param_2, _149))
        {
            continue;
        }
        int param_3 = k;
        float3 param_4 = p;
        vis = sh_tap(param_3, param_4, _149, uShMap, uShMapSmplr);
        casc = k;
        bool _410 = (k + 1) < count;
        bool _423;
        if (_410)
        {
            int param_5 = k;
            float3 param_6 = p;
            float param_7 = 0.0199999995529651641845703125 + _149.uShParam.z;
            _423 = !sh_inside(param_5, param_6, param_7, _149);
        }
        else
        {
            _423 = _410;
        }
        if (_423)
        {
            float3 p2 = (vShWorld + (n * (_149.uShTexel_SunK[uint3(0u, 1u, 2u)[k + 1]] * 1.5))) + (_149.uShSun_On.xyz * _149.uShTexel_SunK[uint3(0u, 1u, 2u)[k + 1]]);
            float4 q = _149.uShCasc[k] * float4(p, 1.0);
            float2 e = fast::min(q.xy - float2(0.0199999995529651641845703125), float2(0.980000019073486328125) - q.xy);
            float t = fast::clamp(fast::min(e.x, e.y) / _149.uShParam.z, 0.0, 1.0);
            int param_8 = k + 1;
            float3 param_9 = p2;
            vis = mix(sh_tap(param_8, param_9, _149, uShMap, uShMapSmplr), vis, t);
        }
        break;
    }
    vis = 1.0 - ((1.0 - vis) * _149.uShKeyDir_Str.w);
    sh_vis = vis;
    vis = 1.0 - ((1.0 - vis) * _149.uShTexel_SunK.w);
    sh_state = float4(vis, facing, float(casc), 1.0);
}

static inline __attribute__((always_inline))
float3 sh_apply_split(thread const float3& full, thread const float3& nokey, constant ShadowUbo& _149, thread float4& sh_state)
{
    if (sh_state.w == 0.0)
    {
        return full;
    }
    if (_149.uShDebug == 2)
    {
        return float3(sh_state.x);
    }
    return mix(nokey, full, float3(sh_state.x));
}

static inline __attribute__((always_inline))
float3 sh_points(thread const float3& c, thread float3& sh_n, constant ShadowUbo& _149, thread float3& vShWorld, texture2d<uint> uPtTiles, sampler uPtTilesSmplr, texturecube_array<float> uPtMap, sampler uPtMapSmplr)
{
    bool _513 = int(_149.uShSun_On.w + 0.5) == 0;
    bool _522;
    if (!_513)
    {
        _522 = _149.uPtN <= 0;
    }
    else
    {
        _522 = _513;
    }
    if (_522)
    {
        bool _529 = _149.uShDebug == 3;
        bool _537;
        if (_529)
        {
            _537 = int(_149.uShSun_On.w + 0.5) != 0;
        }
        else
        {
            _537 = _529;
        }
        return _537 ? float3(0.0) : c;
    }
    float3 add = float3(0.0);
    float4 cp = _149.uPtVP * float4(vShWorld, 1.0);
    int2 ts = int2(uPtTiles.get_width(), uPtTiles.get_height());
    float2 _564;
    if (cp.w > 0.0)
    {
        _564 = ((cp.xy / float2(cp.w)) * 0.5) + float2(0.5);
    }
    else
    {
        _564 = float2(0.5);
    }
    float2 t01 = _564;
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
        if (i >= _149.uPtN)
        {
            break;
        }
        float3 L = _149.uPtPos[i].xyz - vShWorld;
        float r = _149.uPtPos[i].w;
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
        att = mix(att, (att * att) * fast::max(L.z * inv_d, 0.0), _149.uPtDown[i >> 2][i & 3]);
        float sv = 1.0;
        if (_149.uPtCol[i].w >= 0.0)
        {
            float layer = floor(_149.uPtCol[i].w);
            float kw = fast::clamp((fract(_149.uPtCol[i].w) - 0.0500000007450580596923828125) / 0.89999997615814208984375, 0.0, 1.0);
            float3 v = (vShWorld + (sh_n * (0.039999999105930328369140625 + (0.02999999932944774627685546875 * d)))) - _149.uPtPos[i].xyz;
            float f = r;
            float nn = _149.uShPart_Near.w;
            float3 a = abs(v);
            float z = fast::max(fast::max(a.x, a.y), a.z);
            float ref = ((0.5 * (((f + nn) / (f - nn)) - (((2.0 * f) * nn) / ((f - nn) * z)))) + 0.5) - 0.00019999999494757503271102905273438;
            float o = 0.014999999664723873138427734375 * z;
            float4 _828 = float4(v + float3(o, o, 0.0), layer);
            float4 _842 = float4(v + float3(-o, 0.0, o), layer);
            float4 _858 = float4(v + float3(0.0, -o, -o), layer);
            float4 _874 = float4(v + float3(o, -o, o), layer);
            sv = 0.25 * (((spvDepthCast(uPtMap).sample_compare(uPtMapSmplr, _828.xyz, uint(rint(_828.w)), ref) + spvDepthCast(uPtMap).sample_compare(uPtMapSmplr, _842.xyz, uint(rint(_842.w)), ref)) + spvDepthCast(uPtMap).sample_compare(uPtMapSmplr, _858.xyz, uint(rint(_858.w)), ref)) + spvDepthCast(uPtMap).sample_compare(uPtMapSmplr, _874.xyz, uint(rint(_874.w)), ref));
            sv = mix(1.0, sv, kw);
        }
        add += (_149.uPtCol[i].xyz * (((ndl_p * att) * 0.5) * sv));
    }
    if (_149.uShDebug == 3)
    {
        return add;
    }
    return c + add;
}

static inline __attribute__((always_inline))
int sm_type(thread const uint& s, constant ShadowUbo& _149)
{
    bool _1116 = s == 0u;
    bool _1125;
    if (!_1116)
    {
        _1125 = int(_149.uShSun_On.w + 0.5) == 0;
    }
    else
    {
        _1125 = _1116;
    }
    bool _1134;
    if (!_1125)
    {
        _1134 = int(_149.uShCam_Pass.w + 0.5) != 0;
    }
    else
    {
        _1134 = _1125;
    }
    bool _1142;
    if (!_1134)
    {
        _1142 = _149.uMat[0].x == 0.0;
    }
    else
    {
        _1142 = _1134;
    }
    bool _1150;
    if (!_1142)
    {
        _1150 = _149.uEmis[s].w > 0.0;
    }
    else
    {
        _1150 = _1142;
    }
    if (_1150)
    {
        return 0;
    }
    return int(_149.uEmis[s].z + 0.5);
}

static inline __attribute__((always_inline))
float2 sh_mat_uv(thread const float2& st, thread const uint& s, constant ShadowUbo& _149, thread float3& vShWorld)
{
    uint param = s;
    int ty = sm_type(param, _149);
    if (((ty != 4) && (ty != 6)) && (ty != 7))
    {
        return st;
    }
    float _1181;
    if (ty == 4)
    {
        _1181 = 1.0;
    }
    else
    {
        float _1187;
        if (ty == 6)
        {
            _1187 = _149.uMat[s].y;
        }
        else
        {
            _1187 = _149.uMat[s].x;
        }
        _1181 = _1187;
    }
    float flow = _1181;
    float t = _149.uMat[0].y * ((ty == 4) ? 1.0 : 0.300000011920928955078125);
    float2 p = vShWorld.xy;
    float2 d = float2(sin((p.y * 0.20999999344348907470703125) + (t * 0.60000002384185791015625)) + (0.5 * sin((p.x * 0.37000000476837158203125) - (t * 0.430000007152557373046875))), cos((p.x * 0.189999997615814208984375) + (t * 0.5)) + (0.5 * cos((p.y * 0.310000002384185791015625) + (t * 0.709999978542327880859375))));
    return st + (d * (((ty == 4) ? 0.0199999995529651641845703125 : 0.01200000010430812835693359375) * flow));
}

static inline __attribute__((always_inline))
int2 wrapc(thread int2& c, thread const int2& sz, thread const bool& rs, thread const bool& rt)
{
    int _2843;
    if (rs)
    {
        _2843 = spvSMod(spvSMod(c.x, sz.x) + sz.x, sz.x);
    }
    else
    {
        _2843 = clamp(c.x, 0, sz.x - 1);
    }
    c.x = _2843;
    int _2867;
    if (rt)
    {
        _2867 = spvSMod(spvSMod(c.y, sz.y) + sz.y, sz.y);
    }
    else
    {
        _2867 = clamp(c.y, 0, sz.y - 1);
    }
    c.y = _2867;
    return c;
}

static inline __attribute__((always_inline))
float3 sh_emissive(thread const float3& lit, thread const float3& orig, thread const float3& tex, thread const uint& s, constant ShadowUbo& _149)
{
    bool _932 = s == 0u;
    bool _941;
    if (!_932)
    {
        _941 = int(_149.uShSun_On.w + 0.5) == 0;
    }
    else
    {
        _941 = _932;
    }
    bool _950;
    if (!_941)
    {
        _950 = int(_149.uShCam_Pass.w + 0.5) != 0;
    }
    else
    {
        _950 = _941;
    }
    bool _958;
    if (!_950)
    {
        _958 = _149.uEmis[0].x == 0.0;
    }
    else
    {
        _958 = _950;
    }
    if (_958)
    {
        return lit;
    }
    float4 e = _149.uEmis[s];
    if (e.w > 0.0)
    {
        return mix(lit, float3(1.0, 0.0, 1.0), float3(e.x));
    }
    if (e.z > 0.0)
    {
        return lit;
    }
    float v = fast::max(tex.x, fast::max(tex.y, tex.z));
    float _999;
    if (e.y > 0.0)
    {
        _999 = smoothstep(e.y - 0.039999999105930328369140625, e.y + 0.039999999105930328369140625, v);
    }
    else
    {
        _999 = 1.0;
    }
    float m = _999;
    return mix(lit, fast::max(lit, orig * e.x), float3(m));
}

static inline __attribute__((always_inline))
float sm_ssr_dz(thread const float3& q, thread float2& uv, constant ShadowUbo& _149, texture2d<float> uSsrDepth, sampler uSsrDepthSmplr)
{
    float4 c = _149.uSsrVP * float4(q, 1.0);
    float2 _1987;
    if (c.w > 0.00999999977648258209228515625)
    {
        _1987 = ((c.xy / float2(c.w)) * 0.5) + float2(0.5);
    }
    else
    {
        _1987 = float2(-1.0);
    }
    uv = _1987;
    bool _2004 = uv.x < 0.0;
    bool _2011;
    if (!_2004)
    {
        _2011 = uv.y < 0.0;
    }
    else
    {
        _2011 = _2004;
    }
    bool _2018;
    if (!_2011)
    {
        _2018 = uv.x > 1.0;
    }
    else
    {
        _2018 = _2011;
    }
    bool _2025;
    if (!_2018)
    {
        _2025 = uv.y > 1.0;
    }
    else
    {
        _2025 = _2018;
    }
    if (_2025)
    {
        return -1000000000.0;
    }
    float d = uSsrDepth.sample(uSsrDepthSmplr, uv, level(0.0)).x;
    if (d >= 1.0)
    {
        return -1000000000.0;
    }
    return c.w - (_149.uSsrZ.y / (((d * 2.0) - 1.0) - _149.uSsrZ.x));
}

static inline __attribute__((always_inline))
float3 sm_ssr(thread const float3& P, thread const float3& R, thread const float3& miss, constant ShadowUbo& _149, texture2d<float> uSsrDepth, sampler uSsrDepthSmplr, texture2d<float> uSsrCol, sampler uSsrColSmplr)
{
    bool _2058 = _149.uMat[0].x < 1.5;
    bool _2065;
    if (!_2058)
    {
        _2065 = _149.uMat[0].w == 0.0;
    }
    else
    {
        _2065 = _2058;
    }
    if (_2065)
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
        float _2090 = sm_ssr_dz(param, param_1, _149, uSsrDepth, uSsrDepthSmplr);
        float2 uv = param_1;
        float dz = _2090;
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
                float _2123 = sm_ssr_dz(param_2, param_3, _149, uSsrDepth, uSsrDepthSmplr);
                uv = param_3;
                if (_2123 > 0.0)
                {
                    t = tm;
                }
                else
                {
                    t0 = tm;
                }
            }
            float3 param_4 = P + (R * t);
            float _2140 = sm_ssr_dz(param_4, param_5, _149, uSsrDepth, uSsrDepthSmplr);
            uv = param_5;
            dz = _2140;
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
float3 sm_spec(thread const float3& N, thread const float3& V, thread const float& rough, thread const float3& F0, thread float& sh_vis, constant ShadowUbo& _149, thread float3& vShWorld, texture2d<uint> uPtTiles, sampler uPtTilesSmplr)
{
    float r = fast::clamp(rough, 0.02999999932944774627685546875, 1.0);
    float pw = fast::clamp((2.0 / (((r * r) * r) * r)) - 2.0, 1.0, 4096.0);
    float norm = (pw + 8.0) / 25.1326999664306640625;
    float3 sum = float3(0.0);
    bool _1662 = _149.uShKey.w >= 0.0;
    bool _1668;
    if (_1662)
    {
        _1668 = _149.uShTexel_SunK.w > 0.0;
    }
    else
    {
        _1668 = _1662;
    }
    if (_1668)
    {
        float3 H = fast::normalize(_149.uShSun_On.xyz + V);
        float ndl = fast::max(dot(N, _149.uShSun_On.xyz), 0.0);
        float ndh = fast::max(dot(N, H), 0.0);
        float vdh = fast::max(dot(V, H), 0.0);
        float vis = fast::clamp(1.0 - ((1.0 - sh_vis) / fast::max(_149.uShKeyDir_Str.w, 0.00999999977648258209228515625)), 0.0, 1.0);
        float3 F = F0 + ((float3(1.0) - F0) * powr(1.0 - vdh, 5.0));
        sum += (((_149.uShKey.xyz * 1.9921875) * ((((_149.uShTexel_SunK.w * vis) * norm) * powr(ndh, pw)) * ndl)) * F);
    }
    if (_149.uPtN > 0)
    {
        float4 cp = _149.uPtVP * float4(vShWorld, 1.0);
        int2 ts = int2(uPtTiles.get_width(), uPtTiles.get_height());
        float2 _1760;
        if (cp.w > 0.0)
        {
            _1760 = ((cp.xy / float2(cp.w)) * 0.5) + float2(0.5);
        }
        else
        {
            _1760 = float2(0.5);
        }
        float2 t01 = _1760;
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
            if (i >= _149.uPtN)
            {
                break;
            }
            float3 L = _149.uPtPos[i].xyz - vShWorld;
            float rr = _149.uPtPos[i].w;
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
            sum += ((_149.uPtCol[i].xyz * (((((0.5 * att) * att) * norm) * powr(ndh_1, pw)) * ndl_1)) * F_1);
        }
    }
    return sum;
}

static inline __attribute__((always_inline))
float3 sm_env(thread const float3& R, thread const float& rough, constant ShadowUbo& _149)
{
    float up = fast::clamp((R.z * (2.0 - rough)) + 0.25, 0.0, 1.0);
    return _149.uSsrSky.xyz * mix(0.300000011920928955078125, 1.0, (up * up) * (3.0 - (2.0 * up)));
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
    bool _1418 = w.w < 0.5;
    bool _1425;
    if (!_1418)
    {
        _1425 = w.z <= 0.0;
    }
    else
    {
        _1425 = _1418;
    }
    if (_1425)
    {
        return 1.0;
    }
    float2 g = ((p.xy - w.xy) / float2(w.z)) - float2(0.5);
    int2 i0 = int2(floor(g));
    float2 f = g - float2(i0);
    float _1478;
    float4 o;
    for (int k = 0; k < 4; k++)
    {
        int2 c = i0 + int2(k & 1, k >> 1);
        bool _1469 = any(c < int2(0));
        bool _1477;
        if (!_1469)
        {
            _1477 = any(c >= n);
        }
        else
        {
            _1477 = _1469;
        }
        if (_1477)
        {
            _1478 = -1000000.0;
        }
        else
        {
            _1478 = uMatSky.read(uint2(c), 0).x;
        }
        float h = _1478;
        o[k] = smoothstep(h - 0.25, h + 0.25, p.z + 0.75);
    }
    return mix(mix(o.x, o.y, f.x), mix(o.z, o.w, f.x), f.y);
}

static inline __attribute__((always_inline))
float3 sm_rain(thread const float3& n, thread const float3& P, thread const float& t, thread const float& k)
{
    float2 g = float2(0.0);
    float2 _1595;
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
            _1595 = v / float2(d);
        }
        else
        {
            _1595 = float2(0.0);
        }
        g += (_1595 * (((cos(x * 3.0) * exp((-x) * x)) * (1.0 - ph)) * 0.60000002384185791015625));
    }
    return fast::normalize(n - (float3(g, 0.0) * (k * smoothstep(0.60000002384185791015625, 0.89999997615814208984375, n.z))));
}

static inline __attribute__((always_inline))
float3 sm_shimmer(thread const float3& c, thread const float3& P, thread const float& t, constant ShadowUbo& _149, texture2d<float> uSsrCol, sampler uSsrColSmplr)
{
    bool _2197 = _149.uMat[0].x < 1.5;
    bool _2204;
    if (!_2197)
    {
        _2204 = _149.uMat[0].w == 0.0;
    }
    else
    {
        _2204 = _2197;
    }
    if (_2204)
    {
        return c;
    }
    float4 q = _149.uSsrVP * float4(P, 1.0);
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
float3 sh_material(thread const float3& lit, thread const float3& orig, thread const float3& tex, thread const float3& light, thread const uint& s, thread float& sh_vis, thread float3& sh_n, constant ShadowUbo& _149, thread float3& vShWorld, texture2d<uint> uPtTiles, sampler uPtTilesSmplr, texture2d<float> uMatSky, sampler uMatSkySmplr, texture2d<float> uSsrDepth, sampler uSsrDepthSmplr, texture2d<float> uSsrCol, sampler uSsrColSmplr)
{
    uint param = s;
    int ty = sm_type(param, _149);
    if (ty == 0)
    {
        return lit;
    }
    float4 m = _149.uMat[s];
    float t = _149.uMat[0].y;
    float rain = _149.uMat[0].z;
    float3 P = vShWorld;
    float3 N = sh_n;
    float3 V = fast::normalize(_149.uShCam_Pass.xyz - P);
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
        float3 env = (_149.uSsrSky.xyz * mix(0.25, 1.14999997615814208984375, (up * up) * (3.0 - (2.0 * up)))) * L;
        if (gloss > 0.4000000059604644775390625)
        {
            float3 param_1 = P + (N * 0.02999999932944774627685546875);
            float3 param_2 = R;
            float3 param_3 = env;
            env = mix(env, sm_ssr(param_1, param_2, param_3, _149, uSsrDepth, uSsrDepthSmplr, uSsrCol, uSsrColSmplr), float3(smoothstep(0.4000000059604644775390625, 0.75, gloss)));
        }
        float3 mirror = (env * Fv) * mix(0.3499999940395355224609375, 1.5, gloss * gloss);
        float3 param_4 = N;
        float3 param_5 = V;
        float param_6 = r;
        float3 param_7 = F0;
        return (lit * (1.0 - (m.y * mix(0.25, 0.550000011920928955078125, gloss)))) + ((sm_spec(param_4, param_5, param_6, param_7, sh_vis, _149, vShWorld, uPtTiles, uPtTilesSmplr) + mirror) * m.y);
    }
    if ((ty == 2) || (ty == 3))
    {
        float _2444;
        if (ty == 2)
        {
            _2444 = m.x;
        }
        else
        {
            _2444 = m.z;
        }
        float wet = _2444;
        float _2456;
        if (ty == 2)
        {
            _2456 = m.y;
        }
        else
        {
            _2456 = 0.4000000059604644775390625;
        }
        float rough = _2456;
        float Fw = 0.039999999105930328369140625 + (0.959999978542327880859375 * powr(1.0 - ndv, 5.0));
        float3 param_8 = N;
        float3 param_9 = V;
        float param_10 = rough;
        float3 param_11 = float3(0.039999999105930328369140625);
        float3 param_12 = R;
        float param_13 = rough;
        float3 base = (lit * (1.0 - (0.4000000059604644775390625 * wet))) + ((sm_spec(param_8, param_9, param_10, param_11, sh_vis, _149, vShWorld, uPtTiles, uPtTilesSmplr) + ((sm_env(param_12, param_13, _149) * L) * (Fw * 0.800000011920928955078125))) * wet);
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
        float3 param_29 = sm_env(param_25, param_26, _149) * L;
        float3 refl = sm_ssr(param_27, param_28, param_29, _149, uSsrDepth, uSsrDepthSmplr, uSsrCol, uSsrColSmplr);
        float3 param_30 = Np;
        float3 param_31 = V;
        float param_32 = 0.0599999986588954925537109375;
        float3 param_33 = float3(0.0199999995529651641845703125);
        float3 pud = ((lit * (0.4000000059604644775390625 * (1.0 - F))) + (refl * F)) + sm_spec(param_30, param_31, param_32, param_33, sh_vis, _149, vShWorld, uPtTiles, uPtTilesSmplr);
        return mix(base, pud, float3(mask));
    }
    if (ty == 4)
    {
        float2 param_34 = P.xy * 0.07999999821186065673828125;
        float pulse = 1.0 + (m.y * sin((t * 0.800000011920928955078125) + (sm_noise(param_34) * 6.283199787139892578125)));
        float v = fast::max(tex.x, fast::max(tex.y, tex.z));
        float th = m.z;
        float _2668;
        if (th > 0.0)
        {
            _2668 = smoothstep(th - 0.039999999105930328369140625, th + 0.039999999105930328369140625, v);
        }
        else
        {
            _2668 = 1.0;
        }
        float k = _2668;
        float3 c = mix(lit, fast::max(lit, orig * (m.x * pulse)), float3(k));
        float3 param_35 = c;
        float3 param_36 = P;
        float param_37 = t;
        return sm_shimmer(param_35, param_36, param_37, _149, uSsrCol, uSsrColSmplr);
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
        float3 param_51 = sm_env(param_47, param_48, _149) * L;
        float3 refl_1 = sm_ssr(param_49, param_50, param_51, _149, uSsrDepth, uSsrDepthSmplr, uSsrCol, uSsrColSmplr);
        float3 param_52 = Nw;
        float3 param_53 = V;
        float param_54 = 0.070000000298023223876953125;
        float3 param_55 = float3(0.0199999995529651641845703125);
        return (lit * (1.0 - ((0.3499999940395355224609375 * F_1) * m.x))) + (((refl_1 * (F_1 * 0.699999988079071044921875)) + (sm_spec(param_52, param_53, param_54, param_55, sh_vis, _149, vShWorld, uPtTiles, uPtTilesSmplr) * 1.5)) * m.x);
    }
    float _2795;
    if (ty == 6)
    {
        _2795 = m.x;
    }
    else
    {
        _2795 = m.y;
    }
    float g = _2795;
    float Fm = 0.039999999105930328369140625 + (0.959999978542327880859375 * powr(1.0 - ndv, 5.0));
    float3 param_56 = N;
    float3 param_57 = V;
    float param_58 = (ty == 6) ? 0.37999999523162841796875 : 0.5;
    float3 param_59 = float3(0.039999999105930328369140625);
    float3 param_60 = R;
    float param_61 = 0.5;
    return (lit * (1.0 - (0.25 * g))) + (((sm_spec(param_56, param_57, param_58, param_59, sh_vis, _149, vShWorld, uPtTiles, uPtTilesSmplr) * 0.800000011920928955078125) + ((sm_env(param_60, param_61, _149) * L) * (Fm * 0.5))) * g);
}

fragment kMrpVu7Fs_out kMrpVu7Fs(kMrpVu7Fs_in in [[stage_in]], constant ShadowUbo& _149 [[buffer(0)]], constant Push& _3348 [[buffer(1)]], texture2d_array<float> uShMap [[texture(0)]], texture2d<uint> uPtTiles [[texture(1)]], texturecube_array<float> uPtMap [[texture(2)]], texture2d<float> uMatSky [[texture(3)]], texture2d<float> uSsrDepth [[texture(4)]], texture2d<float> uSsrCol [[texture(5)]], texture2d_array<float> uAtlas [[texture(6)]], sampler uShMapSmplr [[sampler(0)]], sampler uPtTilesSmplr [[sampler(1)]], sampler uPtMapSmplr [[sampler(2)]], sampler uMatSkySmplr [[sampler(3)]], sampler uSsrDepthSmplr [[sampler(4)]], sampler uSsrColSmplr [[sampler(5)]], sampler uAtlasSmplr [[sampler(6)]])
{
    kMrpVu7Fs_out out = {};
    float sh_vis = 1.0;
    float3 sh_n = float3(0.0, 0.0, 1.0);
    float4 sh_state;
    sh_prepare(sh_vis, sh_n, _149, uShMap, uShMapSmplr, sh_state, in.vShWorld);
    uint fl = in.vTex.z;
    bool tme = (fl & 1u) != 0u;
    bool iip = ((fl >> uint(4)) & 1u) != 0u;
    bool fge = ((fl >> uint(5)) & 1u) != 0u;
    bool ate = ((fl >> uint(6)) & 1u) != 0u;
    uint tfx = (fl >> uint(1)) & 3u;
    uint tcc = (fl >> uint(3)) & 1u;
    uint atst = (fl >> uint(7)) & 7u;
    uint aref = (fl >> uint(10)) & 255u;
    uint afail = (fl >> uint(22)) & 3u;
    bool lin = ((fl >> uint(18)) & 1u) != 0u;
    bool rs = ((fl >> uint(19)) & 1u) != 0u;
    bool rt = ((fl >> uint(20)) & 1u) != 0u;
    float4 c = iip ? in.vColor : in.vFlat;
    float3 c_game = c.xyz;
    float3 param = c.xyz;
    float3 param_1 = c.xyz * 0.449999988079071044921875;
    float3 param_2 = sh_apply_split(param, param_1, _149, sh_state);
    float3 _2978 = sh_points(param_2, sh_n, _149, in.vShWorld, uPtTiles, uPtTilesSmplr, uPtMap, uPtMapSmplr);
    c.x = _2978.x;
    c.y = _2978.y;
    c.z = _2978.z;
    float4 o = c;
    if (tme)
    {
        int2 sz = int2(int(in.vTex.y & 65535u), int(in.vTex.y >> uint(16)));
        float2 param_3 = in.vStq.xy / float2(in.vStq.z);
        uint param_4 = in.vEmis;
        float2 uv = sh_mat_uv(param_3, param_4, _149, in.vShWorld) * float2(sz);
        int layer = int(in.vTex.x);
        float4 t;
        if (lin)
        {
            float2 p = uv - float2(0.5);
            int2 i0 = int2(floor(p));
            float2 f = p - float2(i0);
            int2 param_5 = i0;
            int2 param_6 = sz;
            bool param_7 = rs;
            bool param_8 = rt;
            int2 _3052 = wrapc(param_5, param_6, param_7, param_8);
            int3 _3056 = int3(_3052, layer);
            float4 t00 = uAtlas.read(uint2(_3056.xy), uint(_3056.z), 0);
            int2 param_9 = i0 + int2(1, 0);
            int2 param_10 = sz;
            bool param_11 = rs;
            bool param_12 = rt;
            int2 _3071 = wrapc(param_9, param_10, param_11, param_12);
            int3 _3075 = int3(_3071, layer);
            float4 t10 = uAtlas.read(uint2(_3075.xy), uint(_3075.z), 0);
            int2 param_13 = i0 + int2(0, 1);
            int2 param_14 = sz;
            bool param_15 = rs;
            bool param_16 = rt;
            int2 _3090 = wrapc(param_13, param_14, param_15, param_16);
            int3 _3094 = int3(_3090, layer);
            float4 t01 = uAtlas.read(uint2(_3094.xy), uint(_3094.z), 0);
            int2 param_17 = i0 + int2(1);
            int2 param_18 = sz;
            bool param_19 = rs;
            bool param_20 = rt;
            int2 _3109 = wrapc(param_17, param_18, param_19, param_20);
            int3 _3113 = int3(_3109, layer);
            float4 t11 = uAtlas.read(uint2(_3113.xy), uint(_3113.z), 0);
            t = mix(mix(t00, t10, float4(f.x)), mix(t01, t11, float4(f.x)), float4(f.y));
        }
        else
        {
            int2 param_21 = int2(floor(uv));
            int2 param_22 = sz;
            bool param_23 = rs;
            bool param_24 = rt;
            int2 _3145 = wrapc(param_21, param_22, param_23, param_24);
            int3 _3149 = int3(_3145, layer);
            t = uAtlas.read(uint2(_3149.xy), uint(_3149.z), 0);
        }
        if (tfx == 0u)
        {
            float3 _3161 = (t.xyz * c.xyz) * 1.9921875;
            o.x = _3161.x;
            o.y = _3161.y;
            o.z = _3161.z;
            float _3170;
            if (tcc != 0u)
            {
                _3170 = (t.w * c.w) * 1.9921875;
            }
            else
            {
                _3170 = c.w;
            }
            o.w = _3170;
        }
        else
        {
            if (tfx == 1u)
            {
                o.x = t.xyz.x;
                o.y = t.xyz.y;
                o.z = t.xyz.z;
                float _3199;
                if (tcc != 0u)
                {
                    _3199 = t.w;
                }
                else
                {
                    _3199 = c.w;
                }
                o.w = _3199;
            }
            else
            {
                if (tfx == 2u)
                {
                    float3 _3223 = ((t.xyz * c.xyz) * 1.9921875) + float3(c.w);
                    o.x = _3223.x;
                    o.y = _3223.y;
                    o.z = _3223.z;
                    float _3232;
                    if (tcc != 0u)
                    {
                        _3232 = t.w + c.w;
                    }
                    else
                    {
                        _3232 = c.w;
                    }
                    o.w = _3232;
                }
                else
                {
                    float3 _3255 = ((t.xyz * c.xyz) * 1.9921875) + float3(c.w);
                    o.x = _3255.x;
                    o.y = _3255.y;
                    o.z = _3255.z;
                    float _3264;
                    if (tcc != 0u)
                    {
                        _3264 = t.w;
                    }
                    else
                    {
                        _3264 = c.w;
                    }
                    o.w = _3264;
                }
            }
        }
        float3 _3276;
        if (tfx == 1u)
        {
            _3276 = t.xyz;
        }
        else
        {
            _3276 = (t.xyz * c_game) * 1.9921875;
        }
        float3 param_25 = o.xyz;
        float3 param_26 = _3276;
        float3 param_27 = t.xyz;
        uint param_28 = in.vEmis;
        float3 _3297 = sh_emissive(param_25, param_26, param_27, param_28, _149);
        o.x = _3297.x;
        o.y = _3297.y;
        o.z = _3297.z;
        float3 _3306;
        if (tfx == 1u)
        {
            _3306 = t.xyz;
        }
        else
        {
            _3306 = (t.xyz * c_game) * 1.9921875;
        }
        float3 _3319;
        if (tfx == 1u)
        {
            _3319 = float3(1.0);
        }
        else
        {
            _3319 = c.xyz * 1.9921875;
        }
        float3 param_29 = o.xyz;
        float3 param_30 = _3306;
        float3 param_31 = t.xyz;
        float3 param_32 = _3319;
        uint param_33 = in.vEmis;
        float3 _3339 = sh_material(param_29, param_30, param_31, param_32, param_33, sh_vis, sh_n, _149, in.vShWorld, uPtTiles, uPtTilesSmplr, uMatSky, uMatSkySmplr, uSsrDepth, uSsrDepthSmplr, uSsrCol, uSsrColSmplr);
        o.x = _3339.x;
        o.y = _3339.y;
        o.z = _3339.z;
        if (_3348.uShade == 2)
        {
            o.x = t.xyz.x;
            o.y = t.xyz.y;
            o.z = t.xyz.z;
        }
    }
    if (_3348.uShade == 1)
    {
        o.x = c.xyz.x;
        o.y = c.xyz.y;
        o.z = c.xyz.z;
    }
    o = fast::clamp(o, float4(0.0), float4(1.0));
    if (fge)
    {
        uint fc = in.vTex.w;
        float3 fogcol = float3(float(fc & 255u), float((fc >> uint(8)) & 255u), float((fc >> uint(16)) & 255u)) / float3(255.0);
        float fw = fast::min(fast::max(in.vFogW, in.vFogC.x), in.vFogC.y);
        float f_1 = float(uint(fast::max(fw, 0.0)) & 255u) / 255.0;
        float4 _3420 = o;
        float3 _3428 = (_3420.xyz * f_1) + (fogcol * (1.0 - f_1));
        o.x = _3428.x;
        o.y = _3428.y;
        o.z = _3428.z;
    }
    float a = o.w * 255.0;
    if (ate)
    {
        bool pass = true;
        if (atst == 0u)
        {
            pass = false;
        }
        else
        {
            if (atst == 2u)
            {
                pass = a < float(aref);
            }
            else
            {
                if (atst == 3u)
                {
                    pass = a <= float(aref);
                }
                else
                {
                    if (atst == 4u)
                    {
                        pass = abs(a - float(aref)) < 0.5;
                    }
                    else
                    {
                        if (atst == 5u)
                        {
                            pass = a >= float(aref);
                        }
                        else
                        {
                            if (atst == 6u)
                            {
                                pass = a > float(aref);
                            }
                            else
                            {
                                if (atst == 7u)
                                {
                                    pass = abs(a - float(aref)) >= 0.5;
                                }
                            }
                        }
                    }
                }
            }
        }
        bool _3511 = !pass;
        bool _3517;
        if (_3511)
        {
            _3517 = _3348.uPass == 0;
        }
        else
        {
            _3517 = _3511;
        }
        bool _3525;
        if (_3517)
        {
            _3525 = (afail == 0u) || (afail == 2u);
        }
        else
        {
            _3525 = _3517;
        }
        if (_3525)
        {
            discard_fragment();
        }
        bool _3529 = !pass;
        bool _3535;
        if (_3529)
        {
            _3535 = _3348.uPass == 1;
        }
        else
        {
            _3535 = _3529;
        }
        if (_3535 && (afail != 2u))
        {
            discard_fragment();
        }
    }
    float as = fast::min(o.w * 1.9921875, 1.0);
    uint bk = (fl >> uint(24)) & 3u;
    if ((bk == 1u) || (bk == 2u))
    {
        out.FragColor = float4(o.xyz * as, as);
        float _3571;
        if (bk == 1u)
        {
            _3571 = 1.0;
        }
        else
        {
            _3571 = 1.0 - as;
        }
        out.FragFactor = float4(_3571);
    }
    else
    {
        out.FragColor = float4(o.xyz, as);
        out.FragFactor = float4(0.0);
    }
    return out;
}

