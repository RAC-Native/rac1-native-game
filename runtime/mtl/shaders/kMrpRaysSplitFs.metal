// kMrpRaysSplitFs (frag): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Push
{
    float4 uA;
    float4 uB;
};

struct kMrpRaysSplitFs_out
{
    float4 FragColor [[color(0)]];
};

struct kMrpRaysSplitFs_in
{
    float2 vUv [[user(locn0)]];
};

fragment kMrpRaysSplitFs_out kMrpRaysSplitFs(kMrpRaysSplitFs_in in [[stage_in]], constant Push& _13 [[buffer(0)]], texture2d<float> uRays [[texture(0)]], sampler uRaysSmplr [[sampler(0)]])
{
    kMrpRaysSplitFs_out out = {};
    float2 asp = float2(_13.uB.x, 1.0);
    float2 v = (in.vUv - _13.uA.xy) * asp;
    float2 side = fast::normalize(float2(-v.y, v.x) + float2(9.9999999747524270787835121154785e-07, 0.0)) / asp;
    float3 r = uRays.sample(uRaysSmplr, in.vUv).xyz;
    float3 across = (((uRays.sample(uRaysSmplr, (in.vUv + (side * 0.0500000007450580596923828125))).xyz + uRays.sample(uRaysSmplr, (in.vUv - (side * 0.0500000007450580596923828125))).xyz) + uRays.sample(uRaysSmplr, (in.vUv + (side * 0.10999999940395355224609375))).xyz) + uRays.sample(uRaysSmplr, (in.vUv - (side * 0.10999999940395355224609375))).xyz) * 0.25;
    float3 broad = fast::min(r, across);
    float3 shaft = r - broad;
    float glare = _13.uB.y * mix(_13.uB.z, 1.0, exp((-dot(v, v)) * _13.uB.w));
    float open = smoothstep(0.20000000298023223876953125, 0.60000002384185791015625, uRays.read(uint2(int2(0)), 0).w);
    out.FragColor = float4(shaft + (broad * mix(1.0, glare, open)), 1.0);
    return out;
}

