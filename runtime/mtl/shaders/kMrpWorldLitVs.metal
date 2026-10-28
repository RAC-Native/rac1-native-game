// kMrpWorldLitVs (vert): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct ShadowUbo
{
    float4x4 uShInvView;
    float4x4 uShLight;
    float4x4 uShCasc[3];
    float4x4 uPtVP;
    float4 uShCut;
    float4 uShCam_Pass;
    float4 uShSun_On;
    float4 uShParam;
    float4 uShTexel_SunK;
    float4 uShKey;
    float4 uShKeyDir_Str;
    float4 uShPart_Near;
    float2 uShProj;
    int uShDebug;
    int uPtN;
    int uShOpaque;
    int _shPad0;
    int _shPad1;
    int _shPad2;
    float4 uPtPos[96];
    float4 uPtCol[96];
    float4 uPtDown[24];
    float4 uEmis[32];
    float4 uMat[32];
    float4x4 uSsrVP;
    float4 uSsrZ;
    float4 uSsrSky;
};

struct Push
{
    float4x4 uVP;
    float4 uFrame;
    float4 uXyOff;
    float4 uFogCol_Pad;
    int uMode;
    int uMipMode;
    int uShade;
    int uFge;
    int uAtPass;
    int uU8;
    int _pad0;
    int _pad1;
};

struct kMrpWorldLitVs_out
{
    float3 vStq [[user(locn0)]];
    float vLodQ [[user(locn1)]];
    float4 vColor [[user(locn2)]];
    float4 vFlat [[user(locn3)]];
    float vFog [[user(locn4)]];
    uint2 vTex [[user(locn5)]];
    uint2 vState [[user(locn6)]];
    float3 vShWorld [[user(locn7)]];
    float4 gl_Position [[position]];
    float gl_ClipDistance [[clip_distance]] [1];
    float gl_ClipDistance_0 [[user(clip0)]];
};

struct kMrpWorldLitVs_in
{
    float4 aWorld [[attribute(0)]];
    float4 aVu [[attribute(1)]];
    float3 aStq [[attribute(2)]];
    float aLodQ [[attribute(3)]];
    uint4 aRgba [[attribute(4)]];
    float aFog [[attribute(5)]];
    uint2 aTex [[attribute(6)]];
    uint2 aState [[attribute(7)]];
};

static inline __attribute__((always_inline))
float4 sh_clip(thread const float4& clip, thread float3& vShWorld, thread float4& gl_Position, thread float (&gl_ClipDistance)[1], constant ShadowUbo& _45)
{
    vShWorld = float3(0.0);
    gl_ClipDistance[0] = 1.0;
    bool _53 = int(_45.uShCam_Pass.w + 0.5) == 0;
    bool _62;
    if (_53)
    {
        _62 = int(_45.uShSun_On.w + 0.5) == 0;
    }
    else
    {
        _62 = _53;
    }
    if (_62)
    {
        return clip;
    }
    float4 wp = _45.uShInvView * float4(clip.x / _45.uShProj.x, clip.y / _45.uShProj.y, -clip.w, 1.0);
    vShWorld = wp.xyz;
    if (int(_45.uShCam_Pass.w + 0.5) != 0)
    {
        gl_ClipDistance[0] = _45.uShCut.w - dot(wp.xyz, _45.uShCut.xyz);
        return _45.uShLight * float4(wp.xyz, 1.0);
    }
    return clip;
}

vertex kMrpWorldLitVs_out kMrpWorldLitVs(kMrpWorldLitVs_in in [[stage_in]], constant ShadowUbo& _45 [[buffer(0)]], constant Push& _135 [[buffer(1)]])
{
    kMrpWorldLitVs_out out = {};
    out.vTex = in.aTex;
    out.vState = in.aState;
    if (_135.uMode == 0)
    {
        float4 param = _135.uVP * in.aWorld;
        float4 _150 = sh_clip(param, out.vShWorld, out.gl_Position, out.gl_ClipDistance, _45);
        out.gl_Position = _150;
    }
    else
    {
        out.vShWorld = float3(0.0);
        out.gl_ClipDistance[0] = 1.0;
        float w = in.aVu.w;
        float X1 = _135.uFrame.x * 0.5;
        float sx = _135.uFrame.z / X1;
        float sy = 1.0 / (_135.uFrame.y * 0.5);
        float2 c = float2((_135.uXyOff.x / 16.0) + X1, (_135.uXyOff.y / 16.0) + (_135.uFrame.y * 0.5));
        float zc = (2.0 * ((_135.uXyOff.z * w) + (_135.uXyOff.w * in.aVu.z))) - w;
        out.gl_Position = float4((in.aVu.x - (c.x * w)) * sx, (-(in.aVu.y - (c.y * w))) * sy, zc, w);
    }
    out.vStq = in.aStq;
    out.vLodQ = in.aLodQ;
    out.vColor = float4(in.aRgba) / float4(255.0);
    out.vFlat = out.vColor;
    out.vFog = in.aFog;
    out.gl_ClipDistance_0 = out.gl_ClipDistance[0];
    out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;       // Adjust clip-space for Metal
    out.gl_Position.y = -(out.gl_Position.y);    // Invert Y-axis for Metal
    return out;
}

