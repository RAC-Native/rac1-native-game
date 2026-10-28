// kMrpPartSplashFs (frag): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct PartUbo
{
    float4x4 uVP;
    float4x4 uInvVP;
    float4 uCam;
    float4 uBox;
    float4 uVel;
    float4 uCol;
    float4 uSize;
    float4 uCamVel;
    float4 uSun;
    float4 uVp;
    float4 uProj;
    float4 uLimLo;
    float4 uLimHi;
    float4 uMisc;
    float4 uMisc2;
    float4 uIntLo[4];
    float4 uIntHi[4];
    float4 uSrc;
    float4 uMove;
    float4 uSky;
    float4 uEmit[512];
};

struct kMrpPartSplashFs_out
{
    float4 FragColor [[color(0)]];
};

static inline __attribute__((always_inline))
float sky_open(thread const float3& p, constant PartUbo& _32, texture2d<float> uSkyMap, sampler uSkyMapSmplr)
{
    if (_32.uSky.w < 0.5)
    {
        return 1.0;
    }
    float2 g = ((p.xy - _32.uSky.xy) / float2(_32.uSky.z)) - float2(0.5);
    int2 i0 = int2(floor(g));
    float2 f = g - float2(i0);
    int2 n = int2(uSkyMap.get_width(), uSkyMap.get_height()) - int2(0, 1);
    float _119;
    float4 o;
    for (int k = 0; k < 4; k++)
    {
        int2 c = i0 + int2(k & 1, k >> 1);
        bool _110 = any(c < int2(0));
        bool _118;
        if (!_110)
        {
            _118 = any(c >= n);
        }
        else
        {
            _118 = _110;
        }
        if (_118)
        {
            _119 = -1000000.0;
        }
        else
        {
            _119 = uSkyMap.read(uint2(c), 0).x;
        }
        float h = _119;
        o[k] = smoothstep(h - 0.25, h + 0.25, p.z);
    }
    return mix(mix(o.x, o.y, f.x), mix(o.z, o.w, f.x), f.y);
}

static inline __attribute__((always_inline))
uint pcg(thread const uint& v)
{
    uint s = (v * 747796405u) + 2891336453u;
    uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    return (w >> 22u) ^ w;
}

static inline __attribute__((always_inline))
float rnd(thread uint& s)
{
    uint param = s;
    s = pcg(param);
    return float(s) * 2.3283064365386962890625e-10;
}

fragment kMrpPartSplashFs_out kMrpPartSplashFs(constant PartUbo& _32 [[buffer(0)]], texture2d<float> uSkyMap [[texture(0)]], texture2d<float> uDepth [[texture(1)]], sampler uSkyMapSmplr [[sampler(0)]], sampler uDepthSmplr [[sampler(1)]], float4 gl_FragCoord [[position]])
{
    kMrpPartSplashFs_out out = {};
    float d = uDepth.read(uint2(int2(gl_FragCoord.xy - _32.uMisc.yz)), 0).x;
    float2 ndc = (((gl_FragCoord.xy - _32.uVp.xy) / _32.uVp.zw) * 2.0) - float2(1.0);
    float4 w = _32.uInvVP * float4(ndc, (d * 2.0) - 1.0, 1.0);
    float3 p = w.xyz / float3(w.w);
    float3 n = cross(dfdx(p), dfdy(p));
    float nl = length(n);
    if ((d >= 0.999989986419677734375) || (nl <= 0.0))
    {
        discard_fragment();
    }
    if (dot(n, _32.uCam.xyz - p) < 0.0)
    {
        n = -n;
    }
    float up = n.z / nl;
    float dist = length(p - _32.uCam.xyz);
    bool _294 = up < 0.85000002384185791015625;
    bool _303;
    if (!_294)
    {
        _303 = dist > _32.uSize.w;
    }
    else
    {
        _303 = _294;
    }
    if (_303)
    {
        discard_fragment();
    }
    bool _309 = _32.uLimLo.w > 0.5;
    bool _330;
    if (_309)
    {
        bool _318 = any(p < _32.uLimLo.xyz);
        bool _329;
        if (!_318)
        {
            _329 = any(p > _32.uLimHi.xyz);
        }
        else
        {
            _329 = _318;
        }
        _330 = _329;
    }
    else
    {
        _330 = _309;
    }
    if (_330)
    {
        discard_fragment();
    }
    for (int i = 0; i < int(_32.uMisc2.x); i++)
    {
        bool _354 = all(p > (_32.uIntLo[i].xyz - float3(0.0, 0.0, 0.5)));
        bool _365;
        if (_354)
        {
            _365 = all(p < _32.uIntHi[i].xyz);
        }
        else
        {
            _365 = _354;
        }
        if (_365)
        {
            discard_fragment();
        }
    }
    float3 param = p + float3(0.0, 0.0, 0.75);
    float open = sky_open(param, _32, uSkyMap, uSkyMapSmplr);
    if (open <= 0.0)
    {
        discard_fragment();
    }
    float2 gi = floor(p.xy / float2(0.550000011920928955078125));
    uint s = uint(int(gi.x) * 73856093) ^ uint(int(gi.y) * 19349663);
    uint param_1 = s;
    float _406 = rnd(param_1);
    s = param_1;
    float rate = 1.39999997615814208984375 + (_406 * 1.2000000476837158203125);
    uint param_2 = s;
    float _418 = rnd(param_2);
    s = param_2;
    float tt = (_32.uCam.w * rate) + _418;
    float cyc = floor(tt);
    float ph = fract(tt);
    uint s2 = s ^ (uint(int(cyc)) * 83492791u);
    uint param_3 = s2;
    float _437 = rnd(param_3);
    s2 = param_3;
    if (_437 > _32.uMisc2.w)
    {
        discard_fragment();
    }
    uint param_4 = s2;
    float _450 = rnd(param_4);
    s2 = param_4;
    uint param_5 = s2;
    float _454 = rnd(param_5);
    s2 = param_5;
    float2 ctr = ((gi + float2(0.25)) + (float2(_450, _454) * 0.5)) * 0.550000011920928955078125;
    float r = length(p.xy - ctr);
    float rad = 0.0199999995529651641845703125 + (ph * 0.119999997317790985107421875);
    float ring = (smoothstep(0.01400000043213367462158203125, 0.0, abs(r - rad)) * (1.0 - ph)) + (smoothstep(0.02500000037252902984619140625, 0.0, r) * fast::max(1.0 - (ph * 5.0), 0.0));
    float a = (((ring * _32.uCol.w) * smoothstep(_32.uSize.w, _32.uSize.w * 0.5, dist)) * up) * open;
    if (a <= 0.0030000000260770320892333984375)
    {
        discard_fragment();
    }
    out.FragColor = float4(_32.uCol.xyz, fast::min(a, 1.0));
    return out;
}

