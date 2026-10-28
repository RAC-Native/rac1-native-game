// kMrp2dVs (vert): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Push
{
    float2 uFb;
    float uScaleX;
    int uRt;
    packed_float3 uFogCol;
    int uFge;
    float4 uScrMap;
    int uTme;
    int uTfx;
    int uTcc;
    int uAte;
    int uAtst;
    int uAref;
    int uAfail;
    int uPass;
    int uScrTex;
    int uDate;
    int uDatm;
    int uMipN;
    int uMipLo;
    int uMipLcm;
    int uMipMxl;
    float uMipK;
    float uMipL;
    int _pad0;
    int _pad1;
    int _pad2;
};

struct kMrp2dVs_out
{
    float4 vColor [[user(locn0)]];
    float3 vTex [[user(locn1)]];
    float vFog [[user(locn2)]];
    float4 vClamp [[user(locn3)]];
    float4 gl_Position [[position]];
};

struct kMrp2dVs_in
{
    float2 aPos [[attribute(0)]];
    float4 aColor [[attribute(1)]];
    float3 aTex [[attribute(2)]];
    float aDepth [[attribute(3)]];
    float aFog [[attribute(4)]];
    float4 aClamp [[attribute(5)]];
};

vertex kMrp2dVs_out kMrp2dVs(kMrp2dVs_in in [[stage_in]], constant Push& _22 [[buffer(0)]])
{
    kMrp2dVs_out out = {};
    float x = ((in.aPos.x / _22.uFb.x) * 2.0) - 1.0;
    float _39;
    if (_22.uRt != 0)
    {
        _39 = ((in.aPos.y / _22.uFb.y) * 2.0) - 1.0;
    }
    else
    {
        _39 = 1.0 - ((in.aPos.y / _22.uFb.y) * 2.0);
    }
    float y = _39;
    out.gl_Position = float4(x * _22.uScaleX, y, (in.aDepth * 2.0) - 1.0, 1.0);
    out.vColor = in.aColor;
    out.vTex = in.aTex;
    out.vFog = in.aFog;
    out.vClamp = in.aClamp;
    out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;       // Adjust clip-space for Metal
    out.gl_Position.y = -(out.gl_Position.y);    // Invert Y-axis for Metal
    return out;
}

