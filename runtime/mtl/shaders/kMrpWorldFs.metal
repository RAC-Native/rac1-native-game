// kMrpWorldFs (frag): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
// rac1: flat inputs (the triangles drawn last vertex first)
#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

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

struct kMrpWorldFs_out
{
    float4 FragColor [[color(0)]];
};

struct kMrpWorldFs_in
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
bool sh_inside(thread const int& k, thread const float3& p, thread const float& m, constant ShadowUbo& _53)
{
    float4 q = _53.uShCasc[k] * float4(p, 1.0);
    bool _164 = all(q.xy > float2(m));
    bool _174;
    if (_164)
    {
        _174 = all(q.xy < float2(1.0 - m));
    }
    else
    {
        _174 = _164;
    }
    bool _180;
    if (_174)
    {
        _180 = q.z < 1.0;
    }
    else
    {
        _180 = _174;
    }
    return _180;
}

static inline __attribute__((always_inline))
float sh_tap(thread const int& k, thread const float3& p, constant ShadowUbo& _53, texture2d_array<float> uShMap, sampler uShMapSmplr)
{
    float4 q = _53.uShCasc[k] * float4(p, 1.0);
    float r = _53.uShParam.w / (float(int3(uShMap.get_width(), uShMap.get_height(), uShMap.get_array_size()).x) * _53.uShPart_Near[uint3(0u, 1u, 2u)[k]]);
    float s = 0.0;
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            float4 _134 = float4((q.xy + (float2(float(x), float(y)) * r)) * _53.uShPart_Near[uint3(0u, 1u, 2u)[k]], float(k), q.z);
            s += spvDepthCast(uShMap).sample_compare(uShMapSmplr, _134.xy, uint(rint(_134.z)), _134.w);
        }
    }
    return s / 9.0;
}

static inline __attribute__((always_inline))
void sh_prepare(thread float3& sh_n, constant ShadowUbo& _53, texture2d_array<float> uShMap, sampler uShMapSmplr, thread float4& sh_state, thread float3& vShWorld)
{
    sh_state = float4(1.0, 0.0, -1.0, 0.0);
    bool _193 = int(_53.uShCam_Pass.w + 0.5) != 0;
    bool _203;
    if (!_193)
    {
        _203 = int(_53.uShSun_On.w + 0.5) == 0;
    }
    else
    {
        _203 = _193;
    }
    if (_203)
    {
        return;
    }
    float3 dx = dfdx(vShWorld);
    float3 dy = dfdy(vShWorld);
    float3 n = cross(dx, dy);
    float nl = length(n);
    float3 _224;
    if (nl > 0.0)
    {
        _224 = n / float3(nl);
    }
    else
    {
        _224 = float3(0.0, 0.0, 1.0);
    }
    n = _224;
    if (dot(n, _53.uShCam_Pass.xyz - vShWorld) < 0.0)
    {
        n = -n;
    }
    sh_n = n;
    float ndl = dot(n, _53.uShSun_On.xyz);
    float facing = smoothstep(0.0, 0.25, ndl);
    int count = int(_53.uShParam.y);
    float vis = 1.0;
    int casc = -1;
    for (int k = 0; k < count; k++)
    {
        float3 p = (vShWorld + (n * (_53.uShTexel_SunK[uint3(0u, 1u, 2u)[k]] * 1.5))) + (_53.uShSun_On.xyz * (_53.uShTexel_SunK[uint3(0u, 1u, 2u)[k]] * 1.0));
        int param = k;
        float3 param_1 = p;
        float param_2 = 0.0199999995529651641845703125;
        if (!sh_inside(param, param_1, param_2, _53))
        {
            continue;
        }
        int param_3 = k;
        float3 param_4 = p;
        vis = sh_tap(param_3, param_4, _53, uShMap, uShMapSmplr);
        casc = k;
        bool _314 = (k + 1) < count;
        bool _327;
        if (_314)
        {
            int param_5 = k;
            float3 param_6 = p;
            float param_7 = 0.0199999995529651641845703125 + _53.uShParam.z;
            _327 = !sh_inside(param_5, param_6, param_7, _53);
        }
        else
        {
            _327 = _314;
        }
        if (_327)
        {
            float3 p2 = (vShWorld + (n * (_53.uShTexel_SunK[uint3(0u, 1u, 2u)[k + 1]] * 1.5))) + (_53.uShSun_On.xyz * _53.uShTexel_SunK[uint3(0u, 1u, 2u)[k + 1]]);
            float4 q = _53.uShCasc[k] * float4(p, 1.0);
            float2 e = fast::min(q.xy - float2(0.0199999995529651641845703125), float2(0.980000019073486328125) - q.xy);
            float t = fast::clamp(fast::min(e.x, e.y) / _53.uShParam.z, 0.0, 1.0);
            int param_8 = k + 1;
            float3 param_9 = p2;
            vis = mix(sh_tap(param_8, param_9, _53, uShMap, uShMapSmplr), vis, t);
        }
        break;
    }
    sh_state = float4(vis, facing, float(casc), 1.0);
}

static inline __attribute__((always_inline))
float3 sh_apply(thread const float3& c, constant ShadowUbo& _53, thread float4& sh_state)
{
    if (sh_state.w == 0.0)
    {
        return c;
    }
    if (_53.uShDebug == 2)
    {
        return float3(sh_state.x * fast::max(sh_state.y, 0.1500000059604644775390625));
    }
    float k = 1.0 - ((_53.uShParam.x * (1.0 - sh_state.x)) * sh_state.y);
    float3 o = c * k;
    if (_53.uShDebug == 1)
    {
        int ci = int(sh_state.z);
        float3 _451;
        if (ci == 0)
        {
            _451 = float3(1.0, 0.60000002384185791015625, 0.60000002384185791015625);
        }
        else
        {
            float3 _459;
            if (ci == 1)
            {
                _459 = float3(0.60000002384185791015625, 1.0, 0.60000002384185791015625);
            }
            else
            {
                _459 = (ci == 2) ? float3(0.60000002384185791015625, 0.60000002384185791015625, 1.0) : float3(1.0);
            }
            _451 = _459;
        }
        float3 tint = _451;
        o *= tint;
    }
    return o;
}

fragment kMrpWorldFs_out kMrpWorldFs(kMrpWorldFs_in in [[stage_in]], constant spvDescriptorSetBuffer2& spvDescriptorSet2 [[buffer(2)]], constant ShadowUbo& _53 [[buffer(3)]], constant Push& _599 [[buffer(4)]], texture2d_array<float> uShMap [[texture(0)]], sampler uShMapSmplr [[sampler(0)]])
{
    kMrpWorldFs_out out = {};
    float3 sh_n = float3(0.0, 0.0, 1.0);
    float4 sh_state;
    sh_prepare(sh_n, _53, uShMap, uShMapSmplr, sh_state, in.vShWorld);
    uint fl = in.vState.x;
    int uTme = int(fl & 1u);
    int uIip = int((fl >> uint(1)) & 1u);
    int uAte = int((fl >> uint(3)) & 1u);
    int uAtst = int((fl >> uint(4)) & 7u);
    int uAref = int((fl >> uint(8)) & 255u);
    uint unit = (fl >> uint(16)) & 15u;
    float4 c = (uIip != 0) ? in.vColor : in.vFlat;
    float3 param = c.xyz;
    float3 _532 = sh_apply(param, _53, sh_state);
    c.x = _532.x;
    c.y = _532.y;
    c.z = _532.z;
    float4 o = c;
    uint w = in.vTex.x;
    float uLayer = float(in.vTex.y);
    if (uTme != 0)
    {
        float2 st = in.vStq.xy / float2(in.vStq.z);
        float2 st_dx = dfdx(st);
        float2 st_dy = dfdy(st);
        float4 t;
        if ((w & 1u) != 0u)
        {
            t = float4(0.501960813999176025390625);
        }
        else
        {
            int mips = int((w >> uint(22)) & 7u) + 1;
            float _635;
            int _657;
            for (;;)
            {
                uint u = spvSubgroupBroadcastFirst(unit);
                if (u != unit)
                {
                    continue;
                }
                if (_599.uMipMode == 0)
                {
                    int mxl = int((w >> uint(4)) & 7u);
                    int lcm = int((w >> uint(7)) & 1u);
                    int L = int((w >> uint(8)) & 3u);
                    float K = float((int(w >> uint(10)) << 20) >> 20) / 16.0;
                    if (lcm != 0)
                    {
                        _635 = K;
                    }
                    else
                    {
                        _635 = (log2(1.0 / fast::max(abs(in.vLodQ), 9.9999996826552253889678874634872e-21)) * float(1 << L)) + K;
                    }
                    float lod = _635;
                    if (lod <= 0.0)
                    {
                        _657 = 0;
                    }
                    else
                    {
                        _657 = min(int(floor(lod + 0.5)), mxl);
                    }
                    int level0 = _657;
                    level0 = min(level0, (mips - 1));
                    float3 _686 = float3(st, uLayer);
                    t = spvDescriptorSet2.uTexs[u].sample(spvDescriptorSet2.uTexsSmplr[u], _686.xy, uint(rint(_686.z)), level(float(level0)));
                }
                else
                {
                    float2 tsz = float2(int3(spvDescriptorSet2.uTexs[u].get_width(), spvDescriptorSet2.uTexs[u].get_height(), spvDescriptorSet2.uTexs[u].get_array_size()).xy);
                    float2 ddx = st_dx * tsz;
                    float2 ddy = st_dy * tsz;
                    float lod_1 = fast::clamp(0.5 * log2(fast::max(dot(ddx, ddx), dot(ddy, ddy))), 0.0, float(mips - 1));
                    float3 _729 = float3(st, uLayer);
                    t = spvDescriptorSet2.uTexs[u].sample(spvDescriptorSet2.uTexsSmplr[u], _729.xy, uint(rint(_729.z)), level(lod_1));
                }
                break;
            }
        }
        uint tfx = (w >> uint(1)) & 3u;
        uint tcc = (w >> uint(3)) & 1u;
        if (tfx == 0u)
        {
            float3 _751 = (t.xyz * c.xyz) * 1.9921875;
            o.x = _751.x;
            o.y = _751.y;
            o.z = _751.z;
            float _760;
            if (tcc != 0u)
            {
                _760 = (t.w * c.w) * 1.9921875;
            }
            else
            {
                _760 = c.w;
            }
            o.w = _760;
        }
        else
        {
            if (tfx == 1u)
            {
                o.x = t.xyz.x;
                o.y = t.xyz.y;
                o.z = t.xyz.z;
                float _789;
                if (tcc != 0u)
                {
                    _789 = t.w;
                }
                else
                {
                    _789 = c.w;
                }
                o.w = _789;
            }
            else
            {
                if (tfx == 2u)
                {
                    float3 _813 = ((t.xyz * c.xyz) * 1.9921875) + float3(c.w);
                    o.x = _813.x;
                    o.y = _813.y;
                    o.z = _813.z;
                    float _822;
                    if (tcc != 0u)
                    {
                        _822 = t.w + c.w;
                    }
                    else
                    {
                        _822 = c.w;
                    }
                    o.w = _822;
                }
                else
                {
                    float3 _845 = ((t.xyz * c.xyz) * 1.9921875) + float3(c.w);
                    o.x = _845.x;
                    o.y = _845.y;
                    o.z = _845.z;
                    float _854;
                    if (tcc != 0u)
                    {
                        _854 = t.w;
                    }
                    else
                    {
                        _854 = c.w;
                    }
                    o.w = _854;
                }
            }
        }
        if (_599.uShade == 2)
        {
            o.x = t.xyz.x;
            o.y = t.xyz.y;
            o.z = t.xyz.z;
        }
    }
    if (_599.uShade == 1)
    {
        o.x = c.xyz.x;
        o.y = c.xyz.y;
        o.z = c.xyz.z;
    }
    else
    {
        if (_599.uShade == 3)
        {
            o.x = 0.800000011920928955078125;
            o.y = 0.800000011920928955078125;
            o.z = 0.800000011920928955078125;
        }
    }
    o = fast::clamp(o, float4(0.0), float4(1.0));
    if (_599.uFge != 0)
    {
        float f = in.vFog / 255.0;
        float4 _918 = o;
        float3 _929 = (_918.xyz * f) + (_599.uFogCol_Pad.xyz * (1.0 - f));
        o.x = _929.x;
        o.y = _929.y;
        o.z = _929.z;
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
        if ((int(_53.uShCam_Pass.w + 0.5) != 0) && (!pass))
        {
            discard_fragment();
        }
        if ((_599.uAtPass == 1) && (!pass))
        {
            discard_fragment();
        }
        if ((_599.uAtPass == 2) && pass)
        {
            discard_fragment();
        }
    }
    out.FragColor = float4(o.xyz, fast::min(o.w * 1.9921875, 1.0));
    return out;
}

