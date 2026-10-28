// kMrpRaysMaskFs (frag): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Push
{
    float2 uSun;
    float2 uAspect;
    float4 uTint_Pad;
    float4 uSrc;
};

struct kMrpRaysMaskFs_out
{
    float4 FragColor [[color(0)]];
};

struct kMrpRaysMaskFs_in
{
    float2 vUv [[user(locn0)]];
};

fragment kMrpRaysMaskFs_out kMrpRaysMaskFs(kMrpRaysMaskFs_in in [[stage_in]], constant Push& _13 [[buffer(0)]], texture2d<float> uDepth [[texture(0)]], texture2d<float> uColor [[texture(1)]], sampler uDepthSmplr [[sampler(0)]], sampler uColorSmplr [[sampler(1)]])
{
    kMrpRaysMaskFs_out out = {};
    float2 suv = _13.uSrc.xy + (in.vUv * _13.uSrc.zw);
    float d = uDepth.sample(uDepthSmplr, suv).x;
    if (d < 1.0)
    {
        out.FragColor = float4(0.0);
        return out;
    }
    float3 c = uColor.sample(uColorSmplr, suv).xyz;
    float2 v = (in.vUv - _13.uSun) * _13.uAspect;
    float k = exp((-dot(v, v)) * 6.0);
    float lum = dot(c, float3(0.2989999949932098388671875, 0.58700001239776611328125, 0.114000000059604644775390625));
    out.FragColor = float4(mix(c, _13.uTint_Pad.xyz * lum, float3(0.5)) * k, 1.0);
    return out;
}

