// kMrpMobySplitFs (frag): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
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

struct kMrpMobySplitFs_out
{
    float4 FragColor [[color(0)]];
};

struct kMrpMobySplitFs_in
{
    float4 vColor [[user(locn0), center_no_perspective]];
    float3 vStq [[user(locn1), center_no_perspective]];
    float vFog [[user(locn2), center_no_perspective]];
    uint4 vCmd [[user(locn3)]];
    float4 vColorNoKey [[user(locn4), center_no_perspective]];
    float3 vShWorld [[user(locn7)]];
};

static inline __attribute__((always_inline))
bool sh_inside(thread const int& k, thread const float3& p, thread const float& m, constant ShadowUbo& _139)
{
    float4 q = _139.uShCasc[k] * float4(p, 1.0);
    bool _250 = all(q.xy > float2(m));
    bool _260;
    if (_250)
    {
        _260 = all(q.xy < float2(1.0 - m));
    }
    else
    {
        _260 = _250;
    }
    bool _266;
    if (_260)
    {
        _266 = q.z < 1.0;
    }
    else
    {
        _266 = _260;
    }
    return _266;
}

static inline __attribute__((always_inline))
float sh_tap(thread const int& k, thread const float3& p, constant ShadowUbo& _139, texture2d_array<float> uShMap, sampler uShMapSmplr)
{
    float4 q = _139.uShCasc[k] * float4(p, 1.0);
    float r = _139.uShParam.w / (float(int3(uShMap.get_width(), uShMap.get_height(), uShMap.get_array_size()).x) * _139.uShPart_Near[uint3(0u, 1u, 2u)[k]]);
    float s = 0.0;
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            float4 _220 = float4((q.xy + (float2(float(x), float(y)) * r)) * _139.uShPart_Near[uint3(0u, 1u, 2u)[k]], float(k), q.z);
            s += spvDepthCast(uShMap).sample_compare(uShMapSmplr, _220.xy, uint(rint(_220.z)), _220.w);
        }
    }
    return s / 9.0;
}

static inline __attribute__((always_inline))
void sh_prepare(thread float& sh_vis, thread float3& sh_n, constant ShadowUbo& _139, texture2d_array<float> uShMap, sampler uShMapSmplr, thread float4& sh_state, thread float3& vShWorld)
{
    sh_state = float4(1.0, 0.0, -1.0, 0.0);
    bool _279 = int(_139.uShCam_Pass.w + 0.5) != 0;
    bool _289;
    if (!_279)
    {
        _289 = int(_139.uShSun_On.w + 0.5) == 0;
    }
    else
    {
        _289 = _279;
    }
    if (_289)
    {
        return;
    }
    float3 dx = dfdx(vShWorld);
    float3 dy = dfdy(vShWorld);
    float3 n = cross(dx, dy);
    float nl = length(n);
    float3 _310;
    if (nl > 0.0)
    {
        _310 = n / float3(nl);
    }
    else
    {
        _310 = float3(0.0, 0.0, 1.0);
    }
    n = _310;
    if (dot(n, _139.uShCam_Pass.xyz - vShWorld) < 0.0)
    {
        n = -n;
    }
    sh_n = n;
    float ndl = dot(n, _139.uShSun_On.xyz);
    float facing = smoothstep(0.0, 0.25, ndl);
    int count = int(_139.uShParam.y);
    float vis = 1.0;
    int casc = -1;
    for (int k = 0; k < count; k++)
    {
        float3 p = (vShWorld + (n * (_139.uShTexel_SunK[uint3(0u, 1u, 2u)[k]] * 1.5))) + (_139.uShSun_On.xyz * (_139.uShTexel_SunK[uint3(0u, 1u, 2u)[k]] * 1.0));
        int param = k;
        float3 param_1 = p;
        float param_2 = 0.0199999995529651641845703125;
        if (!sh_inside(param, param_1, param_2, _139))
        {
            continue;
        }
        int param_3 = k;
        float3 param_4 = p;
        vis = sh_tap(param_3, param_4, _139, uShMap, uShMapSmplr);
        casc = k;
        bool _400 = (k + 1) < count;
        bool _413;
        if (_400)
        {
            int param_5 = k;
            float3 param_6 = p;
            float param_7 = 0.0199999995529651641845703125 + _139.uShParam.z;
            _413 = !sh_inside(param_5, param_6, param_7, _139);
        }
        else
        {
            _413 = _400;
        }
        if (_413)
        {
            float3 p2 = (vShWorld + (n * (_139.uShTexel_SunK[uint3(0u, 1u, 2u)[k + 1]] * 1.5))) + (_139.uShSun_On.xyz * _139.uShTexel_SunK[uint3(0u, 1u, 2u)[k + 1]]);
            float4 q = _139.uShCasc[k] * float4(p, 1.0);
            float2 e = fast::min(q.xy - float2(0.0199999995529651641845703125), float2(0.980000019073486328125) - q.xy);
            float t = fast::clamp(fast::min(e.x, e.y) / _139.uShParam.z, 0.0, 1.0);
            int param_8 = k + 1;
            float3 param_9 = p2;
            vis = mix(sh_tap(param_8, param_9, _139, uShMap, uShMapSmplr), vis, t);
        }
        break;
    }
    vis = 1.0 - ((1.0 - vis) * _139.uShKeyDir_Str.w);
    sh_vis = vis;
    vis = 1.0 - ((1.0 - vis) * _139.uShTexel_SunK.w);
    sh_state = float4(vis, facing, float(casc), 1.0);
}

static inline __attribute__((always_inline))
float3 sh_apply_split(thread const float3& full, thread const float3& nokey, constant ShadowUbo& _139, thread float4& sh_state)
{
    if (sh_state.w == 0.0)
    {
        return full;
    }
    if (_139.uShDebug == 2)
    {
        return float3(sh_state.x);
    }
    return mix(nokey, full, float3(sh_state.x));
}

static inline __attribute__((always_inline))
float3 sh_points_moby(thread const float3& c, thread float3& sh_n, constant ShadowUbo& _139, thread float3& vShWorld, texture2d<uint> uPtTiles, sampler uPtTilesSmplr)
{
    bool _1894 = int(_139.uShSun_On.w + 0.5) == 0;
    bool _1903;
    if (!_1894)
    {
        _1903 = int(_139.uShCam_Pass.w + 0.5) != 0;
    }
    else
    {
        _1903 = _1894;
    }
    bool _1910;
    if (!_1903)
    {
        _1910 = _139.uPtN <= 0;
    }
    else
    {
        _1910 = _1903;
    }
    if (_1910)
    {
        return c;
    }
    int first = int(_139.uEmis[0].y + 0.5);
    if (first >= _139.uPtN)
    {
        return c;
    }
    float3 add = float3(0.0);
    float4 cp = _139.uPtVP * float4(vShWorld, 1.0);
    int2 ts = int2(uPtTiles.get_width(), uPtTiles.get_height());
    float2 _1946;
    if (cp.w > 0.0)
    {
        _1946 = ((cp.xy / float2(cp.w)) * 0.5) + float2(0.5);
    }
    else
    {
        _1946 = float2(0.5);
    }
    float2 t01 = _1946;
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
        if (i >= _139.uPtN)
        {
            break;
        }
        if (i < first)
        {
            continue;
        }
        float3 L = _139.uPtPos[i].xyz - vShWorld;
        float r = _139.uPtPos[i].w;
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
        att = mix(att, (att * att) * fast::max(L.z * inv_d, 0.0), _139.uPtDown[i >> 2][i & 3]);
        add += (_139.uPtCol[i].xyz * ((ndl_p * att) * 0.5));
    }
    return c + add;
}

static inline __attribute__((always_inline))
int sm_type(thread const uint& s, constant ShadowUbo& _139)
{
    bool _713 = s == 0u;
    bool _722;
    if (!_713)
    {
        _722 = int(_139.uShSun_On.w + 0.5) == 0;
    }
    else
    {
        _722 = _713;
    }
    bool _731;
    if (!_722)
    {
        _731 = int(_139.uShCam_Pass.w + 0.5) != 0;
    }
    else
    {
        _731 = _722;
    }
    bool _739;
    if (!_731)
    {
        _739 = _139.uMat[0].x == 0.0;
    }
    else
    {
        _739 = _731;
    }
    bool _747;
    if (!_739)
    {
        _747 = _139.uEmis[s].w > 0.0;
    }
    else
    {
        _747 = _739;
    }
    if (_747)
    {
        return 0;
    }
    return int(_139.uEmis[s].z + 0.5);
}

static inline __attribute__((always_inline))
float2 sh_mat_uv(thread const float2& st, thread const uint& s, constant ShadowUbo& _139, thread float3& vShWorld)
{
    uint param = s;
    int ty = sm_type(param, _139);
    if (((ty != 4) && (ty != 6)) && (ty != 7))
    {
        return st;
    }
    float _778;
    if (ty == 4)
    {
        _778 = 1.0;
    }
    else
    {
        float _784;
        if (ty == 6)
        {
            _784 = _139.uMat[s].y;
        }
        else
        {
            _784 = _139.uMat[s].x;
        }
        _778 = _784;
    }
    float flow = _778;
    float t = _139.uMat[0].y * ((ty == 4) ? 1.0 : 0.300000011920928955078125);
    float2 p = vShWorld.xy;
    float2 d = float2(sin((p.y * 0.20999999344348907470703125) + (t * 0.60000002384185791015625)) + (0.5 * sin((p.x * 0.37000000476837158203125) - (t * 0.430000007152557373046875))), cos((p.x * 0.189999997615814208984375) + (t * 0.5)) + (0.5 * cos((p.y * 0.310000002384185791015625) + (t * 0.709999978542327880859375))));
    return st + (d * (((ty == 4) ? 0.0199999995529651641845703125 : 0.01200000010430812835693359375) * flow));
}

static inline __attribute__((always_inline))
float3 sh_emissive(thread const float3& lit, thread const float3& orig, thread const float3& tex, thread const uint& s, constant ShadowUbo& _139)
{
    bool _526 = s == 0u;
    bool _535;
    if (!_526)
    {
        _535 = int(_139.uShSun_On.w + 0.5) == 0;
    }
    else
    {
        _535 = _526;
    }
    bool _544;
    if (!_535)
    {
        _544 = int(_139.uShCam_Pass.w + 0.5) != 0;
    }
    else
    {
        _544 = _535;
    }
    bool _552;
    if (!_544)
    {
        _552 = _139.uEmis[0].x == 0.0;
    }
    else
    {
        _552 = _544;
    }
    if (_552)
    {
        return lit;
    }
    float4 e = _139.uEmis[s];
    if (e.w > 0.0)
    {
        return mix(lit, float3(1.0, 0.0, 1.0), float3(e.x));
    }
    if (e.z > 0.0)
    {
        return lit;
    }
    float v = fast::max(tex.x, fast::max(tex.y, tex.z));
    float _593;
    if (e.y > 0.0)
    {
        _593 = smoothstep(e.y - 0.039999999105930328369140625, e.y + 0.039999999105930328369140625, v);
    }
    else
    {
        _593 = 1.0;
    }
    float m = _593;
    return mix(lit, fast::max(lit, orig * e.x), float3(m));
}

static inline __attribute__((always_inline))
float sm_ssr_dz(thread const float3& q, thread float2& uv, constant ShadowUbo& _139, texture2d<float> uSsrDepth, sampler uSsrDepthSmplr)
{
    float4 c = _139.uSsrVP * float4(q, 1.0);
    float2 _1602;
    if (c.w > 0.00999999977648258209228515625)
    {
        _1602 = ((c.xy / float2(c.w)) * 0.5) + float2(0.5);
    }
    else
    {
        _1602 = float2(-1.0);
    }
    uv = _1602;
    bool _1619 = uv.x < 0.0;
    bool _1626;
    if (!_1619)
    {
        _1626 = uv.y < 0.0;
    }
    else
    {
        _1626 = _1619;
    }
    bool _1633;
    if (!_1626)
    {
        _1633 = uv.x > 1.0;
    }
    else
    {
        _1633 = _1626;
    }
    bool _1640;
    if (!_1633)
    {
        _1640 = uv.y > 1.0;
    }
    else
    {
        _1640 = _1633;
    }
    if (_1640)
    {
        return -1000000000.0;
    }
    float d = uSsrDepth.sample(uSsrDepthSmplr, uv, level(0.0)).x;
    if (d >= 1.0)
    {
        return -1000000000.0;
    }
    return c.w - (_139.uSsrZ.y / (((d * 2.0) - 1.0) - _139.uSsrZ.x));
}

static inline __attribute__((always_inline))
float3 sm_ssr(thread const float3& P, thread const float3& R, thread const float3& miss, constant ShadowUbo& _139, texture2d<float> uSsrDepth, sampler uSsrDepthSmplr, texture2d<float> uSsrCol, sampler uSsrColSmplr)
{
    bool _1673 = _139.uMat[0].x < 1.5;
    bool _1680;
    if (!_1673)
    {
        _1680 = _139.uMat[0].w == 0.0;
    }
    else
    {
        _1680 = _1673;
    }
    if (_1680)
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
        float _1705 = sm_ssr_dz(param, param_1, _139, uSsrDepth, uSsrDepthSmplr);
        float2 uv = param_1;
        float dz = _1705;
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
                float _1738 = sm_ssr_dz(param_2, param_3, _139, uSsrDepth, uSsrDepthSmplr);
                uv = param_3;
                if (_1738 > 0.0)
                {
                    t = tm;
                }
                else
                {
                    t0 = tm;
                }
            }
            float3 param_4 = P + (R * t);
            float _1755 = sm_ssr_dz(param_4, param_5, _139, uSsrDepth, uSsrDepthSmplr);
            uv = param_5;
            dz = _1755;
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
float3 sm_spec(thread const float3& N, thread const float3& V, thread const float& rough, thread const float3& F0, thread float& sh_vis, constant ShadowUbo& _139, thread float3& vShWorld, texture2d<uint> uPtTiles, sampler uPtTilesSmplr)
{
    float r = fast::clamp(rough, 0.02999999932944774627685546875, 1.0);
    float pw = fast::clamp((2.0 / (((r * r) * r) * r)) - 2.0, 1.0, 4096.0);
    float norm = (pw + 8.0) / 25.1326999664306640625;
    float3 sum = float3(0.0);
    bool _1265 = _139.uShKey.w >= 0.0;
    bool _1271;
    if (_1265)
    {
        _1271 = _139.uShTexel_SunK.w > 0.0;
    }
    else
    {
        _1271 = _1265;
    }
    if (_1271)
    {
        float3 H = fast::normalize(_139.uShSun_On.xyz + V);
        float ndl = fast::max(dot(N, _139.uShSun_On.xyz), 0.0);
        float ndh = fast::max(dot(N, H), 0.0);
        float vdh = fast::max(dot(V, H), 0.0);
        float vis = fast::clamp(1.0 - ((1.0 - sh_vis) / fast::max(_139.uShKeyDir_Str.w, 0.00999999977648258209228515625)), 0.0, 1.0);
        float3 F = F0 + ((float3(1.0) - F0) * powr(1.0 - vdh, 5.0));
        sum += (((_139.uShKey.xyz * 1.9921875) * ((((_139.uShTexel_SunK.w * vis) * norm) * powr(ndh, pw)) * ndl)) * F);
    }
    if (_139.uPtN > 0)
    {
        float4 cp = _139.uPtVP * float4(vShWorld, 1.0);
        int2 ts = int2(uPtTiles.get_width(), uPtTiles.get_height());
        float2 _1368;
        if (cp.w > 0.0)
        {
            _1368 = ((cp.xy / float2(cp.w)) * 0.5) + float2(0.5);
        }
        else
        {
            _1368 = float2(0.5);
        }
        float2 t01 = _1368;
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
            if (i >= _139.uPtN)
            {
                break;
            }
            float3 L = _139.uPtPos[i].xyz - vShWorld;
            float rr = _139.uPtPos[i].w;
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
            sum += ((_139.uPtCol[i].xyz * (((((0.5 * att) * att) * norm) * powr(ndh_1, pw)) * ndl_1)) * F_1);
        }
    }
    return sum;
}

static inline __attribute__((always_inline))
float3 sm_env(thread const float3& R, thread const float& rough, constant ShadowUbo& _139)
{
    float up = fast::clamp((R.z * (2.0 - rough)) + 0.25, 0.0, 1.0);
    return _139.uSsrSky.xyz * mix(0.300000011920928955078125, 1.0, (up * up) * (3.0 - (2.0 * up)));
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
    bool _1017 = w.w < 0.5;
    bool _1024;
    if (!_1017)
    {
        _1024 = w.z <= 0.0;
    }
    else
    {
        _1024 = _1017;
    }
    if (_1024)
    {
        return 1.0;
    }
    float2 g = ((p.xy - w.xy) / float2(w.z)) - float2(0.5);
    int2 i0 = int2(floor(g));
    float2 f = g - float2(i0);
    float _1078;
    float4 o;
    for (int k = 0; k < 4; k++)
    {
        int2 c = i0 + int2(k & 1, k >> 1);
        bool _1069 = any(c < int2(0));
        bool _1077;
        if (!_1069)
        {
            _1077 = any(c >= n);
        }
        else
        {
            _1077 = _1069;
        }
        if (_1077)
        {
            _1078 = -1000000.0;
        }
        else
        {
            _1078 = uMatSky.read(uint2(c), 0).x;
        }
        float h = _1078;
        o[k] = smoothstep(h - 0.25, h + 0.25, p.z + 0.75);
    }
    return mix(mix(o.x, o.y, f.x), mix(o.z, o.w, f.x), f.y);
}

static inline __attribute__((always_inline))
float3 sm_rain(thread const float3& n, thread const float3& P, thread const float& t, thread const float& k)
{
    float2 g = float2(0.0);
    float2 _1196;
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
            _1196 = v / float2(d);
        }
        else
        {
            _1196 = float2(0.0);
        }
        g += (_1196 * (((cos(x * 3.0) * exp((-x) * x)) * (1.0 - ph)) * 0.60000002384185791015625));
    }
    return fast::normalize(n - (float3(g, 0.0) * (k * smoothstep(0.60000002384185791015625, 0.89999997615814208984375, n.z))));
}

static inline __attribute__((always_inline))
float3 sm_shimmer(thread const float3& c, thread const float3& P, thread const float& t, constant ShadowUbo& _139, texture2d<float> uSsrCol, sampler uSsrColSmplr)
{
    bool _1813 = _139.uMat[0].x < 1.5;
    bool _1820;
    if (!_1813)
    {
        _1820 = _139.uMat[0].w == 0.0;
    }
    else
    {
        _1820 = _1813;
    }
    if (_1820)
    {
        return c;
    }
    float4 q = _139.uSsrVP * float4(P, 1.0);
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
float3 sh_material(thread const float3& lit, thread const float3& orig, thread const float3& tex, thread const float3& light, thread const uint& s, thread float& sh_vis, thread float3& sh_n, constant ShadowUbo& _139, thread float3& vShWorld, texture2d<float> uMatSky, sampler uMatSkySmplr, texture2d<uint> uPtTiles, sampler uPtTilesSmplr, texture2d<float> uSsrDepth, sampler uSsrDepthSmplr, texture2d<float> uSsrCol, sampler uSsrColSmplr)
{
    uint param = s;
    int ty = sm_type(param, _139);
    if (ty == 0)
    {
        return lit;
    }
    float4 m = _139.uMat[s];
    float t = _139.uMat[0].y;
    float rain = _139.uMat[0].z;
    float3 P = vShWorld;
    float3 N = sh_n;
    float3 V = fast::normalize(_139.uShCam_Pass.xyz - P);
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
        float3 env = (_139.uSsrSky.xyz * mix(0.25, 1.14999997615814208984375, (up * up) * (3.0 - (2.0 * up)))) * L;
        if (gloss > 0.4000000059604644775390625)
        {
            float3 param_1 = P + (N * 0.02999999932944774627685546875);
            float3 param_2 = R;
            float3 param_3 = env;
            env = mix(env, sm_ssr(param_1, param_2, param_3, _139, uSsrDepth, uSsrDepthSmplr, uSsrCol, uSsrColSmplr), float3(smoothstep(0.4000000059604644775390625, 0.75, gloss)));
        }
        float3 mirror = (env * Fv) * mix(0.3499999940395355224609375, 1.5, gloss * gloss);
        float3 param_4 = N;
        float3 param_5 = V;
        float param_6 = r;
        float3 param_7 = F0;
        return (lit * (1.0 - (m.y * mix(0.25, 0.550000011920928955078125, gloss)))) + ((sm_spec(param_4, param_5, param_6, param_7, sh_vis, _139, vShWorld, uPtTiles, uPtTilesSmplr) + mirror) * m.y);
    }
    if ((ty == 2) || (ty == 3))
    {
        float _2299;
        if (ty == 2)
        {
            _2299 = m.x;
        }
        else
        {
            _2299 = m.z;
        }
        float wet = _2299;
        float _2311;
        if (ty == 2)
        {
            _2311 = m.y;
        }
        else
        {
            _2311 = 0.4000000059604644775390625;
        }
        float rough = _2311;
        float Fw = 0.039999999105930328369140625 + (0.959999978542327880859375 * powr(1.0 - ndv, 5.0));
        float3 param_8 = N;
        float3 param_9 = V;
        float param_10 = rough;
        float3 param_11 = float3(0.039999999105930328369140625);
        float3 param_12 = R;
        float param_13 = rough;
        float3 base = (lit * (1.0 - (0.4000000059604644775390625 * wet))) + ((sm_spec(param_8, param_9, param_10, param_11, sh_vis, _139, vShWorld, uPtTiles, uPtTilesSmplr) + ((sm_env(param_12, param_13, _139) * L) * (Fw * 0.800000011920928955078125))) * wet);
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
        float3 param_29 = sm_env(param_25, param_26, _139) * L;
        float3 refl = sm_ssr(param_27, param_28, param_29, _139, uSsrDepth, uSsrDepthSmplr, uSsrCol, uSsrColSmplr);
        float3 param_30 = Np;
        float3 param_31 = V;
        float param_32 = 0.0599999986588954925537109375;
        float3 param_33 = float3(0.0199999995529651641845703125);
        float3 pud = ((lit * (0.4000000059604644775390625 * (1.0 - F))) + (refl * F)) + sm_spec(param_30, param_31, param_32, param_33, sh_vis, _139, vShWorld, uPtTiles, uPtTilesSmplr);
        return mix(base, pud, float3(mask));
    }
    if (ty == 4)
    {
        float2 param_34 = P.xy * 0.07999999821186065673828125;
        float pulse = 1.0 + (m.y * sin((t * 0.800000011920928955078125) + (sm_noise(param_34) * 6.283199787139892578125)));
        float v = fast::max(tex.x, fast::max(tex.y, tex.z));
        float th = m.z;
        float _2523;
        if (th > 0.0)
        {
            _2523 = smoothstep(th - 0.039999999105930328369140625, th + 0.039999999105930328369140625, v);
        }
        else
        {
            _2523 = 1.0;
        }
        float k = _2523;
        float3 c = mix(lit, fast::max(lit, orig * (m.x * pulse)), float3(k));
        float3 param_35 = c;
        float3 param_36 = P;
        float param_37 = t;
        return sm_shimmer(param_35, param_36, param_37, _139, uSsrCol, uSsrColSmplr);
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
        float3 param_51 = sm_env(param_47, param_48, _139) * L;
        float3 refl_1 = sm_ssr(param_49, param_50, param_51, _139, uSsrDepth, uSsrDepthSmplr, uSsrCol, uSsrColSmplr);
        float3 param_52 = Nw;
        float3 param_53 = V;
        float param_54 = 0.070000000298023223876953125;
        float3 param_55 = float3(0.0199999995529651641845703125);
        return (lit * (1.0 - ((0.3499999940395355224609375 * F_1) * m.x))) + (((refl_1 * (F_1 * 0.699999988079071044921875)) + (sm_spec(param_52, param_53, param_54, param_55, sh_vis, _139, vShWorld, uPtTiles, uPtTilesSmplr) * 1.5)) * m.x);
    }
    float _2650;
    if (ty == 6)
    {
        _2650 = m.x;
    }
    else
    {
        _2650 = m.y;
    }
    float g = _2650;
    float Fm = 0.039999999105930328369140625 + (0.959999978542327880859375 * powr(1.0 - ndv, 5.0));
    float3 param_56 = N;
    float3 param_57 = V;
    float param_58 = (ty == 6) ? 0.37999999523162841796875 : 0.5;
    float3 param_59 = float3(0.039999999105930328369140625);
    float3 param_60 = R;
    float param_61 = 0.5;
    return (lit * (1.0 - (0.25 * g))) + (((sm_spec(param_56, param_57, param_58, param_59, sh_vis, _139, vShWorld, uPtTiles, uPtTilesSmplr) * 0.800000011920928955078125) + ((sm_env(param_60, param_61, _139) * L) * (Fm * 0.5))) * g);
}

fragment kMrpMobySplitFs_out kMrpMobySplitFs(kMrpMobySplitFs_in in [[stage_in]], constant ShadowUbo& _139 [[buffer(0)]], constant Push& _2739 [[buffer(1)]], texture2d_array<float> uShMap [[texture(0)]], texture2d<float> uMatSky [[texture(1)]], texture2d<uint> uPtTiles [[texture(2)]], texture2d<float> uSsrDepth [[texture(3)]], texture2d<float> uSsrCol [[texture(4)]], texture2d_array<float> uTex [[texture(5)]], sampler uShMapSmplr [[sampler(0)]], sampler uMatSkySmplr [[sampler(1)]], sampler uPtTilesSmplr [[sampler(2)]], sampler uSsrDepthSmplr [[sampler(3)]], sampler uSsrColSmplr [[sampler(4)]], sampler uTexSmplr [[sampler(5)]])
{
    kMrpMobySplitFs_out out = {};
    float sh_vis = 1.0;
    float3 sh_n = float3(0.0, 0.0, 1.0);
    float4 sh_state;
    sh_prepare(sh_vis, sh_n, _139, uShMap, uShMapSmplr, sh_state, in.vShWorld);
    float4 c = in.vColor;
    float3 c_game = c.xyz;
    float3 param = c.xyz;
    float3 param_1 = in.vColorNoKey.xyz;
    float3 _2712 = sh_apply_split(param, param_1, _139, sh_state);
    c.x = _2712.x;
    c.y = _2712.y;
    c.z = _2712.z;
    float3 param_2 = c.xyz;
    float3 _2722 = sh_points_moby(param_2, sh_n, _139, in.vShWorld, uPtTiles, uPtTilesSmplr);
    c.x = _2722.x;
    c.y = _2722.y;
    c.z = _2722.z;
    float4 o = c;
    uint w = in.vCmd.y;
    if (_2739.uTme != 0)
    {
        float2 st = in.vStq.xy / float2(in.vStq.z);
        float2 st_dx = dfdx(st);
        float2 st_dy = dfdy(st);
        float2 param_3 = st;
        uint param_4 = (w >> uint(27)) & 31u;
        st = sh_mat_uv(param_3, param_4, _139, in.vShWorld);
        float4 t;
        if ((w & 1u) != 0u)
        {
            t = float4(0.501960813999176025390625);
        }
        else
        {
            int mips = int((w >> uint(22)) & 7u) + 1;
            float layer = float(in.vCmd.z);
            if (_2739.uMipMode == 0)
            {
                int mxl = int((w >> uint(4)) & 7u);
                int lcm = int((w >> uint(7)) & 1u);
                int L = int((w >> uint(8)) & 3u);
                float K = float((int(w >> uint(10)) << 20) >> 20) / 16.0;
                float _2821;
                if (lcm != 0)
                {
                    _2821 = K;
                }
                else
                {
                    _2821 = (log2(1.0 / fast::max(abs(in.vStq.z), 9.9999996826552253889678874634872e-21)) * float(1 << L)) + K;
                }
                float lod = _2821;
                int _2843;
                if (lod <= 0.0)
                {
                    _2843 = 0;
                }
                else
                {
                    _2843 = min(int(floor(lod + 0.5)), mxl);
                }
                int level0 = _2843;
                level0 = min(level0, (mips - 1));
                float3 _2867 = float3(st, layer);
                t = uTex.sample(uTexSmplr, _2867.xy, uint(rint(_2867.z)), level(float(level0)));
                bool _2872 = level0 == 0;
                bool _2879;
                if (_2872)
                {
                    _2879 = (w & 33554432u) != 0u;
                }
                else
                {
                    _2879 = _2872;
                }
                if (_2879)
                {
                    out.FragColor = float4(1.0, 0.0, 1.0, 1.0);
                    return out;
                }
                bool _2887 = level0 == 1;
                bool _2894;
                if (_2887)
                {
                    _2894 = (w & 67108864u) != 0u;
                }
                else
                {
                    _2894 = _2887;
                }
                if (_2894)
                {
                    out.FragColor = float4(0.0, 1.0, 1.0, 1.0);
                    return out;
                }
            }
            else
            {
                float2 tsz = float2(int3(uTex.get_width(), uTex.get_height(), uTex.get_array_size()).xy);
                float2 ddx = st_dx * tsz;
                float2 ddy = st_dy * tsz;
                float lod_1 = fast::clamp(0.5 * log2(fast::max(dot(ddx, ddx), dot(ddy, ddy))), 0.0, float(mips - 1));
                float3 _2933 = float3(st, layer);
                t = uTex.sample(uTexSmplr, _2933.xy, uint(rint(_2933.z)), level(lod_1));
            }
        }
        uint tfx = (w >> uint(1)) & 3u;
        uint tcc = (w >> uint(3)) & 1u;
        if (tfx == 0u)
        {
            float3 _2953 = (t.xyz * c.xyz) * 1.9921875;
            o.x = _2953.x;
            o.y = _2953.y;
            o.z = _2953.z;
            float _2962;
            if (tcc != 0u)
            {
                _2962 = (t.w * c.w) * 1.9921875;
            }
            else
            {
                _2962 = c.w;
            }
            o.w = _2962;
        }
        else
        {
            if (tfx == 1u)
            {
                o.x = t.xyz.x;
                o.y = t.xyz.y;
                o.z = t.xyz.z;
                float _2991;
                if (tcc != 0u)
                {
                    _2991 = t.w;
                }
                else
                {
                    _2991 = c.w;
                }
                o.w = _2991;
            }
            else
            {
                if (tfx == 2u)
                {
                    float3 _3015 = ((t.xyz * c.xyz) * 1.9921875) + float3(c.w);
                    o.x = _3015.x;
                    o.y = _3015.y;
                    o.z = _3015.z;
                    float _3024;
                    if (tcc != 0u)
                    {
                        _3024 = t.w + c.w;
                    }
                    else
                    {
                        _3024 = c.w;
                    }
                    o.w = _3024;
                }
                else
                {
                    float3 _3047 = ((t.xyz * c.xyz) * 1.9921875) + float3(c.w);
                    o.x = _3047.x;
                    o.y = _3047.y;
                    o.z = _3047.z;
                    float _3056;
                    if (tcc != 0u)
                    {
                        _3056 = t.w;
                    }
                    else
                    {
                        _3056 = c.w;
                    }
                    o.w = _3056;
                }
            }
        }
        float3 _3068;
        if (tfx == 1u)
        {
            _3068 = t.xyz;
        }
        else
        {
            _3068 = (t.xyz * c_game) * 1.9921875;
        }
        float3 param_5 = o.xyz;
        float3 param_6 = _3068;
        float3 param_7 = t.xyz;
        uint param_8 = (w >> uint(27)) & 31u;
        float3 _3091 = sh_emissive(param_5, param_6, param_7, param_8, _139);
        o.x = _3091.x;
        o.y = _3091.y;
        o.z = _3091.z;
        float3 _3100;
        if (tfx == 1u)
        {
            _3100 = t.xyz;
        }
        else
        {
            _3100 = (t.xyz * c_game) * 1.9921875;
        }
        float3 _3113;
        if (tfx == 1u)
        {
            _3113 = float3(1.0);
        }
        else
        {
            _3113 = c.xyz * 1.9921875;
        }
        float3 param_9 = o.xyz;
        float3 param_10 = _3100;
        float3 param_11 = t.xyz;
        float3 param_12 = _3113;
        uint param_13 = (w >> uint(27)) & 31u;
        float3 _3135 = sh_material(param_9, param_10, param_11, param_12, param_13, sh_vis, sh_n, _139, in.vShWorld, uMatSky, uMatSkySmplr, uPtTiles, uPtTilesSmplr, uSsrDepth, uSsrDepthSmplr, uSsrCol, uSsrColSmplr);
        o.x = _3135.x;
        o.y = _3135.y;
        o.z = _3135.z;
        if (_2739.uShade == 2)
        {
            o.x = t.xyz.x;
            o.y = t.xyz.y;
            o.z = t.xyz.z;
        }
    }
    if (_2739.uShade == 1)
    {
        o.x = c.xyz.x;
        o.y = c.xyz.y;
        o.z = c.xyz.z;
    }
    else
    {
        if (_2739.uShade == 3)
        {
            o.x = 0.800000011920928955078125;
            o.y = 0.800000011920928955078125;
            o.z = 0.800000011920928955078125;
        }
    }
    o = fast::clamp(o, float4(0.0), float4(1.0));
    if (_2739.uFge != 0)
    {
        float f = in.vFog / 255.0;
        float4 _3196 = o;
        float3 _3207 = (_3196.xyz * f) + (_2739.uFogCol_Min.xyz * (1.0 - f));
        o.x = _3207.x;
        o.y = _3207.y;
        o.z = _3207.z;
    }
    float a = o.w * 255.0;
    if (_2739.uAte != 0)
    {
        bool pass = true;
        if (_2739.uAtst == 0)
        {
            pass = false;
        }
        else
        {
            if (_2739.uAtst == 2)
            {
                pass = a < float(_2739.uAref);
            }
            else
            {
                if (_2739.uAtst == 3)
                {
                    pass = a <= float(_2739.uAref);
                }
                else
                {
                    if (_2739.uAtst == 4)
                    {
                        pass = abs(a - float(_2739.uAref)) < 0.5;
                    }
                    else
                    {
                        if (_2739.uAtst == 5)
                        {
                            pass = a >= float(_2739.uAref);
                        }
                        else
                        {
                            if (_2739.uAtst == 6)
                            {
                                pass = a > float(_2739.uAref);
                            }
                            else
                            {
                                if (_2739.uAtst == 7)
                                {
                                    pass = abs(a - float(_2739.uAref)) >= 0.5;
                                }
                            }
                        }
                    }
                }
            }
        }
        bool _3303 = !pass;
        bool _3311;
        if (_3303)
        {
            _3311 = int(_139.uShCam_Pass.w + 0.5) != 0;
        }
        else
        {
            _3311 = _3303;
        }
        if (_3311)
        {
            discard_fragment();
        }
        bool _3315 = !pass;
        bool _3321;
        if (_3315)
        {
            _3321 = _2739.uPass == 0;
        }
        else
        {
            _3321 = _3315;
        }
        bool _3334;
        if (_3321)
        {
            bool _3326 = _2739.uAfail == 0;
            bool _3333;
            if (!_3326)
            {
                _3333 = _2739.uAfail == 2;
            }
            else
            {
                _3333 = _3326;
            }
            _3334 = _3333;
        }
        else
        {
            _3334 = _3321;
        }
        if (_3334)
        {
            discard_fragment();
        }
        bool _3338 = !pass;
        bool _3344;
        if (_3338)
        {
            _3344 = _2739.uPass == 1;
        }
        else
        {
            _3344 = _3338;
        }
        bool _3350;
        if (_3344)
        {
            _3350 = _2739.uAfail != 2;
        }
        else
        {
            _3350 = _3344;
        }
        if (_3350)
        {
            discard_fragment();
        }
    }
    out.FragColor = float4(o.xyz, fast::min(o.w * 1.9921875, 1.0));
    return out;
}

