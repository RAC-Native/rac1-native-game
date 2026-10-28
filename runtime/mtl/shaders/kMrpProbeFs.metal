// kMrpProbeFs (frag): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Push
{
    float4 uParams;
};

struct kMrpProbeFs_out
{
    float4 oCol [[color(0)]];
};

struct kMrpProbeFs_in
{
    float2 vUv [[user(locn0)]];
};

fragment kMrpProbeFs_out kMrpProbeFs(kMrpProbeFs_in in [[stage_in]], constant Push& _53 [[buffer(0)]], texture2d<float> uScene [[texture(0)]], sampler uSceneSmplr [[sampler(0)]])
{
    kMrpProbeFs_out out = {};
    float3 s = float3(0.0);
    for (int y = 0; y < 4; y++)
    {
        for (int x = 0; x < 4; x++)
        {
            s += uScene.sample(uSceneSmplr, (in.vUv + ((float2(float(x), float(y)) - float2(1.5)) * _53.uParams.xy))).xyz;
        }
    }
    out.oCol = float4(s * 0.0625, 1.0);
    return out;
}

