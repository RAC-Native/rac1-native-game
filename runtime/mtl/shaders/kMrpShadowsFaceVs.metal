// kMrpShadowsFaceVs (vert): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Push
{
    float4x4 uVP;
};

struct kMrpShadowsFaceVs_out
{
    float4 gl_Position [[position]];
};

struct kMrpShadowsFaceVs_in
{
    float3 aPos [[attribute(0)]];
};

vertex kMrpShadowsFaceVs_out kMrpShadowsFaceVs(kMrpShadowsFaceVs_in in [[stage_in]], constant Push& _19 [[buffer(0)]])
{
    kMrpShadowsFaceVs_out out = {};
    out.gl_Position = _19.uVP * float4(in.aPos, 1.0);
    out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;       // Adjust clip-space for Metal
    out.gl_Position.y = -(out.gl_Position.y);    // Invert Y-axis for Metal
    return out;
}

