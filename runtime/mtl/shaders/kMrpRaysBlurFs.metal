// kMrpRaysBlurFs (frag): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Push
{
    float2 uSun;
    float2 uTaps_pad;
    float4 uParam;
};

struct kMrpRaysBlurFs_out
{
    float4 FragColor [[color(0)]];
};

struct kMrpRaysBlurFs_in
{
    float2 vUv [[user(locn0)]];
};

fragment kMrpRaysBlurFs_out kMrpRaysBlurFs(kMrpRaysBlurFs_in in [[stage_in]], constant Push& _14 [[buffer(0)]], texture2d<float> uMask [[texture(0)]], sampler uMaskSmplr [[sampler(0)]], float4 gl_FragCoord [[position]])
{
    kMrpRaysBlurFs_out out = {};
    int N = int(_14.uTaps_pad.x + 0.5);
    float uAspect = _14.uTaps_pad.y;
    float2 delta = (in.vUv - _14.uSun) * (_14.uParam.x / float(N));
    float _noise = fract(52.98291778564453125 * fract(dot(gl_FragCoord.xy, float2(0.067110560834407806396484375, 0.005837149918079376220703125))));
    float2 uv = in.vUv - (delta * _noise);
    float3 sum = float3(0.0);
    float decay = 1.0;
    for (int i = 0; i < N; i++)
    {
        uv -= delta;
        sum += ((uMask.sample(uMaskSmplr, uv).xyz * decay) * _14.uParam.z);
        decay *= _14.uParam.y;
    }
    float os = 0.0;
    float ow = 9.9999999747524270787835121154785e-07;
    bool _113 = gl_FragCoord.x < 1.0;
    bool _119;
    if (_113)
    {
        _119 = gl_FragCoord.y < 1.0;
    }
    else
    {
        _119 = _113;
    }
    if (_119)
    {
        for (int j = 0; j < 6; j++)
        {
            for (int i_1 = 0; i_1 < 6; i_1++)
            {
                float2 o = (float2(float(i_1), float(j)) - float2(2.5)) * 0.119999997317790985107421875;
                float2 q = fast::clamp(_14.uSun + float2(o.x / uAspect, o.y), float2(0.0), float2(1.0));
                float w = exp((-dot(o, o)) * 6.0);
                os += (uMask.sample(uMaskSmplr, q).w * w);
                ow += w;
            }
        }
    }
    out.FragColor = float4(sum * _14.uParam.w, os / ow);
    return out;
}

