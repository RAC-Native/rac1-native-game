// kMrpPartVs (vert): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
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

struct kMrpPartVs_out
{
    float2 vQ [[user(locn0)]];
    float vA [[user(locn1)]];
    float vZ [[user(locn2)]];
    float4 gl_Position [[position]];
};

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

vertex kMrpPartVs_out kMrpPartVs(constant PartUbo& _32 [[buffer(0)]], texture2d<float> uSkyMap [[texture(0)]], sampler uSkyMapSmplr [[sampler(0)]], uint gl_VertexIndex [[vertex_id]])
{
    kMrpPartVs_out out = {};
    int id = int(gl_VertexIndex);
    int pi = id / 6;
    int c = id - (pi * 6);
    float2 q = float2((((c == 1) || (c == 2)) || (c == 4)) ? 1.0 : (-1.0), (((c == 2) || (c == 4)) || (c == 5)) ? 1.0 : (-1.0));
    out.vQ = q;
    out.vA = 0.0;
    out.vZ = 1.0;
    out.gl_Position = float4(2.0, 2.0, 2.0, 1.0);
    int kind = int(_32.uVel.w + 0.5);
    uint s = (uint(pi) * 9781u) + uint(_32.uMisc.w);
    uint param = s;
    float _267 = rnd(param);
    s = param;
    uint param_1 = s;
    float _271 = rnd(param_1);
    s = param_1;
    uint param_2 = s;
    float _275 = rnd(param_2);
    s = param_2;
    float3 r0 = float3(_267, _271, _275);
    uint param_3 = s;
    float _281 = rnd(param_3);
    s = param_3;
    float r3 = _281;
    uint param_4 = s;
    float _286 = rnd(param_4);
    s = param_4;
    float r4 = _286;
    uint param_5 = s;
    float _291 = rnd(param_5);
    s = param_5;
    float r5 = _291;
    float t = _32.uCam.w;
    float3 vel = _32.uVel.xyz * (0.75 + (0.5 * r3));
    float ph = r4 * 6.283199787139892578125;
    float tw = t * _32.uMove.x;
    float3 drift = float3(sin((tw * (0.310000002384185791015625 + (0.4000000059604644775390625 * r5))) + ph), cos((tw * (0.2700000107288360595703125 + (0.3499999940395355224609375 * r3))) + (ph * 1.7000000476837158203125)), 0.5 * sin((tw * (0.23000000417232513427734375 + (0.300000011920928955078125 * r4))) + (ph * 2.2999999523162841796875))) * _32.uMisc.x;
    bool src = _32.uSrc.x > 0.5;
    float3 wp;
    float3 rel;
    float a;
    if (src)
    {
        float rate = 0.070000000298023223876953125 + (0.0599999986588954925537109375 * r5);
        float lt = (t * rate) + r4;
        float life = fract(lt);
        uint param_6 = ((uint(pi) * 2654435761u) + (uint(int(floor(lt))) * 40503u)) + uint(_32.uMisc.w);
        uint s2 = pcg(param_6);
        uint param_7 = s2;
        float _401 = rnd(param_7);
        s2 = param_7;
        float4 em = _32.uEmit[min(int(_401 * 512.0), 511)];
        bool _413 = em.w > (-1.5);
        bool _419;
        if (_413)
        {
            _419 = em.w < 0.0;
        }
        else
        {
            _419 = _413;
        }
        if (_419)
        {
            out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;       // Adjust clip-space for Metal
            out.gl_Position.y = -(out.gl_Position.y);    // Invert Y-axis for Metal
            return out;
        }
        uint param_8 = s2;
        float _426 = rnd(param_8);
        s2 = param_8;
        float ang = _426 * 6.283199787139892578125;
        uint param_9 = s2;
        float _432 = rnd(param_9);
        s2 = param_9;
        float rad = sqrt(_432) * _32.uSrc.w;
        float age = life / rate;
        float2 cur = float2(sin(((em.y * 0.070000000298023223876953125) + (t * 0.0309999994933605194091796875)) + 1.2999999523162841796875) + (0.60000002384185791015625 * sin((em.x * 0.0500000007450580596923828125) - (t * 0.02099999971687793731689453125))), cos((em.x * 0.0599999986588954925537109375) - (t * 0.026000000536441802978515625)) + (0.60000002384185791015625 * cos((em.y * 0.04500000178813934326171875) + (t * 0.0170000009238719940185546875)))) * _32.uMove.y;
        float3 wander = float3(sin((age * (0.3499999940395355224609375 + (0.300000011920928955078125 * r5))) + ph), cos((age * (0.300000011920928955078125 + (0.300000011920928955078125 * r3))) + (ph * 1.7000000476837158203125)), 0.0) * (_32.uMisc.x * fast::min(age * 0.25, 1.0));
        wp = ((em.xyz + float3(cos(ang) * rad, sin(ang) * rad, 0.0500000007450580596923828125)) + float3((cur + _32.uVel.xy) * age, (_32.uVel.z * (0.699999988079071044921875 + (0.60000002384185791015625 * r3))) * age)) + wander;
        rel = wp - _32.uCam.xyz;
        float _560;
        if (em.w >= 0.0)
        {
            _560 = em.w;
        }
        else
        {
            _560 = (-2.0) - em.w;
        }
        float since = t - _560;
        float _575;
        if (since < 0.0)
        {
            _575 = 1.0;
        }
        else
        {
            float _582;
            if (em.w >= 0.0)
            {
                _582 = fast::clamp(since / 1.5, 0.0, 1.0);
            }
            else
            {
                _582 = fast::clamp(1.0 - (since / 1.5), 0.0, 1.0);
            }
            _575 = _582;
        }
        float slot_a = _575;
        a = (((_32.uCol.w * smoothstep(0.0, 0.0599999986588954925537109375, life)) * (1.0 - smoothstep(0.5, 1.0, life))) * smoothstep(_32.uSrc.z, _32.uSrc.z * 0.75, length(rel))) * slot_a;
    }
    else
    {
        rel = (((r0 * _32.uBox.xyz) + (vel * t)) + drift) - _32.uCam.xyz;
        rel = (fract(rel / _32.uBox.xyz) - float3(0.5)) * _32.uBox.xyz;
        wp = _32.uCam.xyz + rel;
        float3 e = abs(rel) / (_32.uBox.xyz * 0.5);
        a = _32.uCol.w * smoothstep(1.0, 0.75, fast::max(fast::max(e.x, e.y), e.z));
    }
    if (_32.uLimLo.w > 0.5)
    {
        float3 d = fast::min(wp - _32.uLimLo.xyz, _32.uLimHi.xyz - wp);
        a *= fast::clamp(fast::min(fast::min(d.x, d.y), d.z), 0.0, 1.0);
    }
    if (_32.uMisc2.y > 0.5)
    {
        float3 param_10 = wp;
        a *= sky_open(param_10, _32, uSkyMap, uSkyMapSmplr);
        for (int i = 0; i < int(_32.uMisc2.x); i++)
        {
            bool _731 = all(wp > _32.uIntLo[i].xyz);
            bool _742;
            if (_731)
            {
                _742 = all(wp < _32.uIntHi[i].xyz);
            }
            else
            {
                _742 = _731;
            }
            if (_742)
            {
                a = 0.0;
            }
        }
    }
    if (kind == 2)
    {
        float life_1 = fract((t * (0.180000007152557373046875 + (0.20000000298023223876953125 * r5))) + r4);
        float _763;
        if (src)
        {
            _763 = 1.0;
        }
        else
        {
            _763 = sin(life_1 * 3.141590118408203125);
        }
        a *= (_763 * (0.60000002384185791015625 + (0.4000000059604644775390625 * sin((t * (9.0 + (6.0 * r3))) + ph))));
    }
    else
    {
        if (kind == 3)
        {
            a *= smoothstep(0.3499999940395355224609375, 1.0, 0.5 + (0.5 * sin((t * (1.10000002384185791015625 + (1.39999997615814208984375 * r5))) + (ph * 3.0))));
        }
        else
        {
            if (kind == 4)
            {
                float3 vd = fast::normalize(rel);
                float sc = powr(fast::max(dot(vd, _32.uSun.xyz), 0.0), 3.0);
                a *= (mix(1.0, 0.60000002384185791015625 + (2.400000095367431640625 * sc), _32.uSun.w) * (0.699999988079071044921875 + (0.300000011920928955078125 * sin((t * (2.0 + (3.0 * r5))) + ph))));
            }
        }
    }
    if (a <= 0.0030000000260770320892333984375)
    {
        out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;       // Adjust clip-space for Metal
        out.gl_Position.y = -(out.gl_Position.y);    // Invert Y-axis for Metal
        return out;
    }
    float4 c0 = _32.uVP * float4(wp, 1.0);
    if (c0.w < 0.300000011920928955078125)
    {
        out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;       // Adjust clip-space for Metal
        out.gl_Position.y = -(out.gl_Position.y);    // Invert Y-axis for Metal
        return out;
    }
    a *= smoothstep(0.4000000059604644775390625, (kind == 5) ? 1.0 : 2.0, c0.w);
    float size = _32.uSize.x * (1.0 + (_32.uSize.y * ((r5 * 2.0) - 1.0)));
    float half_px = (((size * _32.uProj.z) * _32.uVp.z) * 0.5) / c0.w;
    float k = fast::max((_32.uMisc2.z * 0.5) / fast::max(half_px, 9.9999997473787516355514526367188e-05), 1.0);
    if (kind == 5)
    {
        float3 v = vel - _32.uCamVel.xyz;
        float4 c1 = _32.uVP * float4(wp - (v * _32.uSize.z), 1.0);
        if (c1.w < 0.300000011920928955078125)
        {
            out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;       // Adjust clip-space for Metal
            out.gl_Position.y = -(out.gl_Position.y);    // Invert Y-axis for Metal
            return out;
        }
        float2 px = _32.uVp.zw * 0.5;
        float2 dir = ((c1.xy / float2(c1.w)) * px) - ((c0.xy / float2(c0.w)) * px);
        float len = length(dir);
        float2 _970;
        if (len > 0.001000000047497451305389404296875)
        {
            _970 = dir / float2(len);
        }
        else
        {
            _970 = float2(0.0, 1.0);
        }
        dir = _970;
        float2 nrm = float2(-dir.y, dir.x);
        float along = (q.y * 0.5) + 0.5;
        float4 cc = mix(c0, c1, float4(along));
        out.gl_Position = float4(cc.xy + (((nrm * ((q.x * half_px) * k)) / px) * cc.w), cc.zw);
        out.vZ = cc.w;
        a /= k;
        a *= (fast::clamp(len / 3.0, 0.25, 1.0) * 1.0);
    }
    else
    {
        out.gl_Position = float4(c0.xy + (((q * float2(_32.uProj.z, _32.uProj.w)) * size) * k), c0.zw);
        out.vZ = c0.w;
        a /= k;
    }
    out.vA = a;
    out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;       // Adjust clip-space for Metal
    out.gl_Position.y = -(out.gl_Position.y);    // Invert Y-axis for Metal
    return out;
}

