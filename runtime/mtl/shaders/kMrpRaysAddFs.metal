// kMrpRaysAddFs (frag): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Push
{
    float uK;
    float uCap;
};

struct kMrpRaysAddFs_out
{
    float4 FragColor [[color(0)]];
};

struct kMrpRaysAddFs_in
{
    float2 vUv [[user(locn0)]];
};

fragment kMrpRaysAddFs_out kMrpRaysAddFs(kMrpRaysAddFs_in in [[stage_in]], constant Push& _12 [[buffer(0)]], texture2d<float> uRays [[texture(0)]], sampler uRaysSmplr [[sampler(0)]])
{
    kMrpRaysAddFs_out out = {};
    out.FragColor = float4((float3(1.0) - exp(((-uRays.sample(uRaysSmplr, in.vUv).xyz) * _12.uK) / float3(_12.uCap))) * _12.uCap, 0.0);
    return out;
}

