// kMrpPartFs (frag): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
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

struct kMrpPartFs_out
{
    float4 FragColor [[color(0)]];
};

struct kMrpPartFs_in
{
    float2 vQ [[user(locn0)]];
    float vA [[user(locn1)]];
    float vZ [[user(locn2)]];
};

fragment kMrpPartFs_out kMrpPartFs(kMrpPartFs_in in [[stage_in]], constant PartUbo& _20 [[buffer(0)]], texture2d<float> uDepth [[texture(0)]], sampler uDepthSmplr [[sampler(0)]], float4 gl_FragCoord [[position]])
{
    kMrpPartFs_out out = {};
    int kind = int(_20.uVel.w + 0.5);
    float r2 = dot(in.vQ, in.vQ);
    float shape;
    if (kind == 5)
    {
        shape = ((1.0 - abs(in.vQ.x)) * smoothstep(-1.0, -0.4000000059604644775390625, in.vQ.y)) * (0.449999988079071044921875 + (0.550000011920928955078125 * ((in.vQ.y * 0.5) + 0.5)));
    }
    else
    {
        if (r2 >= 1.0)
        {
            discard_fragment();
        }
        float _82;
        if (((kind == 2) || (kind == 3)) || (kind == 4))
        {
            _82 = exp((-r2) * 3.5);
        }
        else
        {
            _82 = smoothstep(1.0, 0.25, r2);
        }
        shape = _82;
    }
    float a = in.vA * shape;
    float d = uDepth.read(uint2(int2(gl_FragCoord.xy - _20.uMisc.yz)), 0).x;
    float zs = _20.uProj.y / (((d * 2.0) - 1.0) + _20.uProj.x);
    a *= fast::clamp((zs - in.vZ) / _20.uSize.w, 0.0, 1.0);
    if (a <= 0.00200000009499490261077880859375)
    {
        discard_fragment();
    }
    out.FragColor = float4(_20.uCol.xyz, fast::min(a, 1.0));
    return out;
}

