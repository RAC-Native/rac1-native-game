// kMrpPartSplashVs (vert): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct kMrpPartSplashVs_out
{
    float4 gl_Position [[position]];
};

vertex kMrpPartSplashVs_out kMrpPartSplashVs(uint gl_VertexIndex [[vertex_id]])
{
    kMrpPartSplashVs_out out = {};
    float2 p = float2(float((int(gl_VertexIndex) << 1) & 2), float(int(gl_VertexIndex) & 2));
    out.gl_Position = float4((p * 2.0) - float2(1.0), 0.0, 1.0);
    out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;       // Adjust clip-space for Metal
    out.gl_Position.y = -(out.gl_Position.y);    // Invert Y-axis for Metal
    return out;
}

