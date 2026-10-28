// kMrpVu7Vs (vert): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
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
    int uShade;
    int uPass;
    int _pad0;
    int _pad1;
};

struct kMrpVu7Vs_out
{
    float3 vStq [[user(locn0)]];
    float4 vColor [[user(locn1)]];
    float4 vFlat [[user(locn2)]];
    float vFogW [[user(locn3)]];
    float2 vFogC [[user(locn4)]];
    uint4 vTex [[user(locn5)]];
    uint vEmis [[user(locn6)]];
    float3 vShWorld [[user(locn7)]];
    float4 gl_Position [[position]];
    float gl_ClipDistance [[clip_distance]] [1];
    float gl_ClipDistance_0 [[user(clip0)]];
};

struct kMrpVu7Vs_in
{
    float3 aWorld [[attribute(0)]];
    float3 aStq [[attribute(1)]];
    float aFogW [[attribute(2)]];
    uint4 aRgba [[attribute(3)]];
    uint4 aTex [[attribute(4)]];
    float2 aFogC [[attribute(5)]];
    uint aEmis [[attribute(6)]];
    float aDepth [[attribute(7)]];
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

vertex kMrpVu7Vs_out kMrpVu7Vs(kMrpVu7Vs_in in [[stage_in]], constant ShadowUbo& _45 [[buffer(0)]], constant Push& _126 [[buffer(1)]])
{
    kMrpVu7Vs_out out = {};
    float4 param = _126.uVP * float4(in.aWorld, 1.0);
    float4 _139 = sh_clip(param, out.vShWorld, out.gl_Position, out.gl_ClipDistance, _45);
    out.gl_Position = _139;
    if (in.aDepth < 1.5)
    {
        out.gl_Position.z = in.aDepth * out.gl_Position.w;
    }
    out.vStq = in.aStq;
    out.vColor = float4(in.aRgba) / float4(255.0);
    out.vFlat = out.vColor;
    out.vFogW = in.aFogW;
    out.vFogC = in.aFogC;
    out.vTex = in.aTex;
    out.vEmis = in.aEmis;
    out.gl_ClipDistance_0 = out.gl_ClipDistance[0];
    out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;       // Adjust clip-space for Metal
    out.gl_Position.y = -(out.gl_Position.y);    // Invert Y-axis for Metal
    return out;
}

