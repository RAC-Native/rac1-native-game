// kMrpAoBlurFs (frag): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Push
{
    float2 uTexel;
    float uMoby;
    float _pad;
    float4 uDMap;
    float4 uSMap;
};

struct kMrpAoBlurFs_out
{
    float4 FragColor [[color(0)]];
};

struct kMrpAoBlurFs_in
{
    float2 vUv [[user(locn0)]];
};

fragment kMrpAoBlurFs_out kMrpAoBlurFs(kMrpAoBlurFs_in in [[stage_in]], constant Push& _21 [[buffer(0)]], texture2d<float> uDepth [[texture(0)]], texture2d<float> uAo [[texture(1)]], texture2d<uint> uStencil [[texture(2)]], sampler uDepthSmplr [[sampler(0)]], sampler uAoSmplr [[sampler(1)]], sampler uStencilSmplr [[sampler(2)]])
{
    kMrpAoBlurFs_out out = {};
    float d0 = uDepth.sample(uDepthSmplr, ((in.vUv * _21.uDMap.zw) + _21.uDMap.xy)).x;
    if (d0 >= 1.0)
    {
        discard_fragment();
    }
    float sum = 0.0;
    float w = 0.0;
    for (int y = -2; y <= 2; y++)
    {
        for (int x = -2; x <= 2; x++)
        {
            float2 uv = in.vUv + (float2(float(x), float(y)) * _21.uTexel);
            float d = uDepth.sample(uDepthSmplr, ((uv * _21.uDMap.zw) + _21.uDMap.xy)).x;
            float k = float(abs(d - d0) < 0.0005000000237487256526947021484375);
            sum += (uAo.sample(uAoSmplr, uv).x * k);
            w += k;
        }
    }
    float _120;
    if (w > 0.0)
    {
        _120 = sum / w;
    }
    else
    {
        _120 = 1.0;
    }
    float ao = _120;
    float k_1 = 1.0;
    bool _132 = _21.uMoby < 1.0;
    bool _156;
    if (_132)
    {
        _156 = (uStencil.sample(uStencilSmplr, ((in.vUv * _21.uSMap.zw) + _21.uSMap.xy)).x & 1u) != 0u;
    }
    else
    {
        _156 = _132;
    }
    if (_156)
    {
        k_1 = _21.uMoby;
    }
    out.FragColor = float4(float3(mix(1.0, ao, k_1)), 1.0);
    return out;
}

