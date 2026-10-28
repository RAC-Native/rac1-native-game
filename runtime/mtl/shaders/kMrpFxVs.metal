// kMrpFxVs (vert): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Push
{
    float4x4 uVP;
    float4 uFrame;
    float4 uXyOff;
    int uMode;
    int uShade;
    int uPass;
    int _pad;
};

struct kMrpFxVs_out
{
    float3 vStq [[user(locn0)]];
    float4 vColor [[user(locn1)]];
    float4 vFlat [[user(locn2)]];
    float vFog [[user(locn3)]];
    uint4 vTex [[user(locn4)]];
    float4 gl_Position [[position]];
};

struct kMrpFxVs_in
{
    float4 aWorld [[attribute(0)]];
    float3 aStq [[attribute(1)]];
    float aFog [[attribute(2)]];
    uint4 aRgba [[attribute(3)]];
    uint4 aTex [[attribute(4)]];
    float3 aVu [[attribute(5)]];
};

vertex kMrpFxVs_out kMrpFxVs(kMrpFxVs_in in [[stage_in]], constant Push& _12 [[buffer(0)]])
{
    kMrpFxVs_out out = {};
    if (_12.uMode == 0)
    {
        out.gl_Position = _12.uVP * in.aWorld;
    }
    else
    {
        float sx = _12.uFrame.z / (_12.uFrame.x * 0.5);
        float sy = 1.0 / (_12.uFrame.y * 0.5);
        out.gl_Position = float4((in.aVu.x - _12.uXyOff.x) * sx, (-(in.aVu.y - _12.uXyOff.y)) * sy, (2.0 * (_12.uXyOff.z + (_12.uXyOff.w * in.aVu.z))) - 1.0, 1.0);
    }
    out.vStq = in.aStq;
    out.vColor = float4(in.aRgba) / float4(255.0);
    out.vFlat = out.vColor;
    out.vFog = in.aFog;
    out.vTex = in.aTex;
    out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;       // Adjust clip-space for Metal
    out.gl_Position.y = -(out.gl_Position.y);    // Invert Y-axis for Metal
    return out;
}

