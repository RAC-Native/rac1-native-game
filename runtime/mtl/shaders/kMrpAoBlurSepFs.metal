// kMrpAoBlurSepFs (frag): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

struct Push
{
    float2 uTexel;
    float uMoby;
    float _pad;
    float4 uDMap;
    float4 uSMap;
};

constant spvUnsafeArray<float, 5> _117 = spvUnsafeArray<float, 5>({ 0.227026998996734619140625, 0.19459460675716400146484375, 0.121621601283550262451171875, 0.054053999483585357666015625, 0.01621600054204463958740234375 });

struct kMrpAoBlurSepFs_out
{
    float4 FragColor [[color(0)]];
};

struct kMrpAoBlurSepFs_in
{
    float2 vUv [[user(locn0)]];
};

fragment kMrpAoBlurSepFs_out kMrpAoBlurSepFs(kMrpAoBlurSepFs_in in [[stage_in]], constant Push& _21 [[buffer(0)]], texture2d<float> uDepth [[texture(0)]], texture2d<float> uAo [[texture(1)]], texture2d<uint> uStencil [[texture(2)]], sampler uDepthSmplr [[sampler(0)]], sampler uAoSmplr [[sampler(1)]], sampler uStencilSmplr [[sampler(2)]])
{
    kMrpAoBlurSepFs_out out = {};
    float d0 = uDepth.sample(uDepthSmplr, ((in.vUv * _21.uDMap.zw) + _21.uDMap.xy)).x;
    if (d0 >= 1.0)
    {
        out.FragColor = float4(1.0);
        return out;
    }
    float sum = uAo.sample(uAoSmplr, in.vUv).x * 0.227026998996734619140625;
    float w = 0.227026998996734619140625;
    for (int i = 1; i <= 4; i++)
    {
        float2 off = _21.uTexel * float(i);
        float2 uv1 = in.vUv + off;
        float2 uv2 = in.vUv - off;
        float d1 = uDepth.sample(uDepthSmplr, ((uv1 * _21.uDMap.zw) + _21.uDMap.xy)).x;
        float d2 = uDepth.sample(uDepthSmplr, ((uv2 * _21.uDMap.zw) + _21.uDMap.xy)).x;
        float k1 = _117[i] * exp((-abs(d1 - d0)) * 3500.0);
        float k2 = _117[i] * exp((-abs(d2 - d0)) * 3500.0);
        sum += ((uAo.sample(uAoSmplr, uv1).x * k1) + (uAo.sample(uAoSmplr, uv2).x * k2));
        w += (k1 + k2);
    }
    float _171;
    if (w > 9.9999997473787516355514526367188e-06)
    {
        _171 = sum / w;
    }
    else
    {
        _171 = 1.0;
    }
    float ao = _171;
    float k = 1.0;
    bool _183 = _21.uMoby < 1.0;
    bool _206;
    if (_183)
    {
        _206 = (uStencil.sample(uStencilSmplr, ((in.vUv * _21.uSMap.zw) + _21.uSMap.xy)).x & 1u) != 0u;
    }
    else
    {
        _206 = _183;
    }
    if (_206)
    {
        k = _21.uMoby;
    }
    out.FragColor = float4(float3(mix(1.0, ao, k)), 1.0);
    return out;
}

