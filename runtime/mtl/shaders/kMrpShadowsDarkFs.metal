// kMrpShadowsDarkFs (frag): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Push
{
    float4 uColor;
};

struct kMrpShadowsDarkFs_out
{
    float4 oCol [[color(0)]];
};

fragment kMrpShadowsDarkFs_out kMrpShadowsDarkFs(constant Push& _12 [[buffer(0)]])
{
    kMrpShadowsDarkFs_out out = {};
    out.oCol = _12.uColor;
    return out;
}

