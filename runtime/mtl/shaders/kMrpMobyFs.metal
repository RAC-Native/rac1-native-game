// kMrpMobyFs (frag): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
// rac1: flat inputs (the triangles drawn last vertex first)
#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

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

struct kMrpMobyFs_out
{
    float4 FragColor [[color(0)]];
};

struct kMrpMobyFs_in
{
    float4 vColor [[user(locn0), center_no_perspective]];
    float3 vStq [[user(locn1), center_no_perspective]];
    float vFog [[user(locn2), center_no_perspective]];
    uint4 vCmd [[user(locn3)]];
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

fragment kMrpMobyFs_out kMrpMobyFs(kMrpMobyFs_in in [[stage_in]], constant ShadowUbo& _53 [[buffer(0)]], constant Push& _504 [[buffer(1)]], texture2d_array<float> uShMap [[texture(0)]], texture2d_array<float> uTex [[texture(1)]], sampler uShMapSmplr [[sampler(0)]], sampler uTexSmplr [[sampler(1)]])
{
    kMrpMobyFs_out out = {};
    float3 sh_n = float3(0.0, 0.0, 1.0);
    float4 sh_state;
    sh_prepare(sh_n, _53, uShMap, uShMapSmplr, sh_state, in.vShWorld);
    float4 c = in.vColor;
    float3 param = c.xyz;
    float3 _485 = sh_apply(param, _53, sh_state);
    c.x = _485.x;
    c.y = _485.y;
    c.z = _485.z;
    float4 o = c;
    uint w = in.vCmd.y;
    if (_504.uTme != 0)
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
            float layer = float(in.vCmd.z);
            if (_504.uMipMode == 0)
            {
                int mxl = int((w >> uint(4)) & 7u);
                int lcm = int((w >> uint(7)) & 1u);
                int L = int((w >> uint(8)) & 3u);
                float K = float((int(w >> uint(10)) << 20) >> 20) / 16.0;
                float _582;
                if (lcm != 0)
                {
                    _582 = K;
                }
                else
                {
                    _582 = (log2(1.0 / fast::max(abs(in.vStq.z), 9.9999996826552253889678874634872e-21)) * float(1 << L)) + K;
                }
                float lod = _582;
                int _604;
                if (lod <= 0.0)
                {
                    _604 = 0;
                }
                else
                {
                    _604 = min(int(floor(lod + 0.5)), mxl);
                }
                int level0 = _604;
                level0 = min(level0, (mips - 1));
                float3 _628 = float3(st, layer);
                t = uTex.sample(uTexSmplr, _628.xy, uint(rint(_628.z)), level(float(level0)));
                bool _633 = level0 == 0;
                bool _640;
                if (_633)
                {
                    _640 = (w & 33554432u) != 0u;
                }
                else
                {
                    _640 = _633;
                }
                if (_640)
                {
                    out.FragColor = float4(1.0, 0.0, 1.0, 1.0);
                    return out;
                }
                bool _648 = level0 == 1;
                bool _655;
                if (_648)
                {
                    _655 = (w & 67108864u) != 0u;
                }
                else
                {
                    _655 = _648;
                }
                if (_655)
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
                float3 _695 = float3(st, layer);
                t = uTex.sample(uTexSmplr, _695.xy, uint(rint(_695.z)), level(lod_1));
            }
        }
        uint tfx = (w >> uint(1)) & 3u;
        uint tcc = (w >> uint(3)) & 1u;
        if (tfx == 0u)
        {
            float3 _717 = (t.xyz * c.xyz) * 1.9921875;
            o.x = _717.x;
            o.y = _717.y;
            o.z = _717.z;
            float _726;
            if (tcc != 0u)
            {
                _726 = (t.w * c.w) * 1.9921875;
            }
            else
            {
                _726 = c.w;
            }
            o.w = _726;
        }
        else
        {
            if (tfx == 1u)
            {
                o.x = t.xyz.x;
                o.y = t.xyz.y;
                o.z = t.xyz.z;
                float _755;
                if (tcc != 0u)
                {
                    _755 = t.w;
                }
                else
                {
                    _755 = c.w;
                }
                o.w = _755;
            }
            else
            {
                if (tfx == 2u)
                {
                    float3 _779 = ((t.xyz * c.xyz) * 1.9921875) + float3(c.w);
                    o.x = _779.x;
                    o.y = _779.y;
                    o.z = _779.z;
                    float _788;
                    if (tcc != 0u)
                    {
                        _788 = t.w + c.w;
                    }
                    else
                    {
                        _788 = c.w;
                    }
                    o.w = _788;
                }
                else
                {
                    float3 _811 = ((t.xyz * c.xyz) * 1.9921875) + float3(c.w);
                    o.x = _811.x;
                    o.y = _811.y;
                    o.z = _811.z;
                    float _820;
                    if (tcc != 0u)
                    {
                        _820 = t.w;
                    }
                    else
                    {
                        _820 = c.w;
                    }
                    o.w = _820;
                }
            }
        }
        if (_504.uShade == 2)
        {
            o.x = t.xyz.x;
            o.y = t.xyz.y;
            o.z = t.xyz.z;
        }
    }
    if (_504.uShade == 1)
    {
        o.x = c.xyz.x;
        o.y = c.xyz.y;
        o.z = c.xyz.z;
    }
    else
    {
        if (_504.uShade == 3)
        {
            o.x = 0.800000011920928955078125;
            o.y = 0.800000011920928955078125;
            o.z = 0.800000011920928955078125;
        }
    }
    o = fast::clamp(o, float4(0.0), float4(1.0));
    if (_504.uFge != 0)
    {
        float f = in.vFog / 255.0;
        float4 _885 = o;
        float3 _896 = (_885.xyz * f) + (_504.uFogCol_Min.xyz * (1.0 - f));
        o.x = _896.x;
        o.y = _896.y;
        o.z = _896.z;
    }
    float a = o.w * 255.0;
    if (_504.uAte != 0)
    {
        bool pass = true;
        if (_504.uAtst == 0)
        {
            pass = false;
        }
        else
        {
            if (_504.uAtst == 2)
            {
                pass = a < float(_504.uAref);
            }
            else
            {
                if (_504.uAtst == 3)
                {
                    pass = a <= float(_504.uAref);
                }
                else
                {
                    if (_504.uAtst == 4)
                    {
                        pass = abs(a - float(_504.uAref)) < 0.5;
                    }
                    else
                    {
                        if (_504.uAtst == 5)
                        {
                            pass = a >= float(_504.uAref);
                        }
                        else
                        {
                            if (_504.uAtst == 6)
                            {
                                pass = a > float(_504.uAref);
                            }
                            else
                            {
                                if (_504.uAtst == 7)
                                {
                                    pass = abs(a - float(_504.uAref)) >= 0.5;
                                }
                            }
                        }
                    }
                }
            }
        }
        bool _993 = !pass;
        bool _1001;
        if (_993)
        {
            _1001 = int(_53.uShCam_Pass.w + 0.5) != 0;
        }
        else
        {
            _1001 = _993;
        }
        if (_1001)
        {
            discard_fragment();
        }
        bool _1005 = !pass;
        bool _1012;
        if (_1005)
        {
            _1012 = _504.uPass == 0;
        }
        else
        {
            _1012 = _1005;
        }
        bool _1025;
        if (_1012)
        {
            bool _1017 = _504.uAfail == 0;
            bool _1024;
            if (!_1017)
            {
                _1024 = _504.uAfail == 2;
            }
            else
            {
                _1024 = _1017;
            }
            _1025 = _1024;
        }
        else
        {
            _1025 = _1012;
        }
        if (_1025)
        {
            discard_fragment();
        }
        bool _1029 = !pass;
        bool _1035;
        if (_1029)
        {
            _1035 = _504.uPass == 1;
        }
        else
        {
            _1035 = _1029;
        }
        bool _1041;
        if (_1035)
        {
            _1041 = _504.uAfail != 2;
        }
        else
        {
            _1041 = _1035;
        }
        if (_1041)
        {
            discard_fragment();
        }
    }
    out.FragColor = float4(o.xyz, fast::min(o.w * 1.9921875, 1.0));
    return out;
}

