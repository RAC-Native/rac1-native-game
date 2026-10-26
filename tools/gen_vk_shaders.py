#!/usr/bin/env python3
"""The Vulkan renderer's GLSL shaders, compiled to SPIR-V at build time (CMakeLists.txt): see main()."""

import os
import subprocess
import tempfile
import struct
import sys

SHADOW_UBO_GLSL = """
layout(std140, set = 1, binding = 0) uniform ShadowUbo {
    mat4 uShInvView;
    mat4 uShLight;
    mat4 uShCasc[3];
    mat4 uPtVP;
    vec4 uShCut;
    vec4 uShCam_Pass;
    vec4 uShSun_On;
    vec4 uShParam;
    vec4 uShTexel_SunK;
    vec4 uShKey;
    vec4 uShKeyDir_Str;
    vec4 uShPart_Near;
    vec2 uShProj;
    int uShDebug;
    int uPtN;
    int uShOpaque;
    int _shPad0, _shPad1, _shPad2;
    vec4 uPtPos[96];
    vec4 uPtCol[96];
    vec4 uPtDown[24];
    vec4 uEmis[32];
    vec4 uMat[32];
    mat4 uSsrVP;
    vec4 uSsrZ;
    vec4 uSsrSky;
};
#define uShCam (uShCam_Pass.xyz)
#define uShPass (int(uShCam_Pass.w + 0.5))
#define uShSun (uShSun_On.xyz)
#define uShOn (int(uShSun_On.w + 0.5))
#define uShTexel (uShTexel_SunK.xyz)
#define uShSunK (uShTexel_SunK.w)
#define uShKeyDir (uShKeyDir_Str.xyz)
#define uShStrength (uShKeyDir_Str.w)
#define uShPart (uShPart_Near.xyz)
#define uPtNear (uShPart_Near.w)
"""

RAC1_VK_SHADOW_VS = SHADOW_UBO_GLSL + """
layout(location = 7) smooth out vec3 vShWorld;
vec4 sh_clip(vec4 clip) {
    vShWorld = vec3(0.0);
    gl_ClipDistance[0] = 1.0;
    if (uShPass == 0 && uShOn == 0) return clip;
    vec4 wp = uShInvView * vec4(clip.x / uShProj.x, clip.y / uShProj.y, -clip.w, 1.0);
    vShWorld = wp.xyz;
    if (uShPass != 0) {
        gl_ClipDistance[0] = uShCut.w - dot(wp.xyz, uShCut.xyz);
        return uShLight * vec4(wp.xyz, 1.0);
    }
    return clip;
}
"""



def read_c_glsl(path):
    """The GLSL of a C string macro (src/game/render/mrp_materials_glsl.inc): its string literals joined, unescaped - the OpenGL
    and the Vulkan shaders share that one text."""
    import ast
    import re
    out = []
    for line in open(path, encoding="utf-8"):
        for lit in re.findall(r'"(?:[^"\\]|\\.)*"', line):
            out.append(ast.literal_eval(lit))
    return "".join(out)


# the surface materials (docs/materials.md), the end of the lighting's fragment code
MATERIALS_GLSL = read_c_glsl(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "game", "render", "mrp_materials_glsl.inc"))

RAC1_VK_SHADOW_FS = SHADOW_UBO_GLSL + """
layout(set = 1, binding = 1) uniform sampler2DArrayShadow uShMap;
layout(set = 1, binding = 3) uniform samplerCubeArrayShadow uPtMap;
layout(set = 1, binding = 4) uniform usampler2D uPtTiles;
layout(set = 1, binding = 2) uniform sampler2D uSsrCol;
layout(set = 1, binding = 5) uniform sampler2D uSsrDepth;
#if defined(SH_POINTS) || defined(SH_SPLIT)
float sh_vis = 1.0;
#endif
layout(location = 7) smooth in vec3 vShWorld;
float sh_tap(int k, vec3 p) {
    vec4 q = uShCasc[k] * vec4(p, 1.0);
    float r = uShParam.w / (float(textureSize(uShMap, 0).x) * uShPart[k]);
    float s = 0.0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            s += texture(uShMap, vec4((q.xy + vec2(x, y) * r) * uShPart[k], float(k), q.z));
    return s / 9.0;
}
bool sh_inside(int k, vec3 p, float m) {
    vec4 q = uShCasc[k] * vec4(p, 1.0);
    return all(greaterThan(q.xy, vec2(m))) && all(lessThan(q.xy, vec2(1.0 - m))) && q.z < 1.0;
}
vec4 sh_state;
vec3 sh_n = vec3(0.0, 0.0, 1.0);
void sh_prepare() {
    sh_state = vec4(1.0, 0.0, -1.0, 0.0);
    if (uShPass != 0 || uShOn == 0) return;
    vec3 dx = dFdx(vShWorld), dy = dFdy(vShWorld);
    vec3 n = cross(dx, dy);
    float nl = length(n);
    n = nl > 0.0 ? n / nl : vec3(0.0, 0.0, 1.0);
    if (dot(n, uShCam - vShWorld) < 0.0) n = -n;
    sh_n = n;
    float ndl = dot(n, uShSun);
    float facing = smoothstep(0.0, 0.25, ndl);
#ifdef SH_POINTS
    if (facing <= 0.0 && uShDebug == 0) {
        sh_state = vec4(1.0, 0.0, -1.0, 1.0);
        return;
    }
#endif
    int count = int(uShParam.y);
    float vis = 1.0;
    int casc = -1;
    for (int k = 0; k < count; ++k) {
        vec3 p = vShWorld + n * (uShTexel[k] * 1.5) + uShSun * (uShTexel[k] * 1.0);
        if (!sh_inside(k, p, 0.02)) continue;
        vis = sh_tap(k, p);
        casc = k;
        if (k + 1 < count && !sh_inside(k, p, 0.02 + uShParam.z)) {
            vec3 p2 = vShWorld + n * (uShTexel[k + 1] * 1.5) + uShSun * uShTexel[k + 1];
            vec4 q = uShCasc[k] * vec4(p, 1.0);
            vec2 e = min(q.xy - 0.02, 0.98 - q.xy);
            float t = clamp(min(e.x, e.y) / uShParam.z, 0.0, 1.0);
            vis = mix(sh_tap(k + 1, p2), vis, t);
        }
        break;
    }
#if defined(SH_POINTS) || defined(SH_SPLIT)
    vis = 1.0 - (1.0 - vis) * uShStrength;
    sh_vis = vis;
    vis = 1.0 - (1.0 - vis) * uShSunK;
#endif
    sh_state = vec4(vis, facing, float(casc), 1.0);
}
vec3 sh_points(vec3 c) {
    if (uShOn == 0 || uPtN <= 0) return uShDebug == 3 && uShOn != 0 ? vec3(0.0) : c;
    vec3 add = vec3(0.0);
    vec4 cp = uPtVP * vec4(vShWorld, 1.0);
    ivec2 ts = textureSize(uPtTiles, 0);
    vec2 t01 = cp.w > 0.0 ? cp.xy / cp.w * 0.5 + 0.5 : vec2(0.5);
    uvec4 m = texelFetch(uPtTiles, clamp(ivec2(floor(t01 * vec2(ts))), ivec2(0), ts - 1), 0);
    while ((m.x | m.y | m.z) != 0u) {
        int i;
        if (m.x != 0u) { i = findLSB(m.x); m.x &= m.x - 1u; }
        else if (m.y != 0u) { i = 32 + findLSB(m.y); m.y &= m.y - 1u; }
        else { i = 64 + findLSB(m.z); m.z &= m.z - 1u; }
        if (i >= uPtN) break;
        vec3 L = uPtPos[i].xyz - vShWorld;
        float r = uPtPos[i].w, d2 = dot(L, L);
        if (d2 >= r * r || d2 <= 0.0) continue;
        float inv_d = inversesqrt(d2), d = d2 * inv_d;
        float ndl_p = dot(sh_n, L) * inv_d;
        if (ndl_p <= 0.0) continue;
        float att = 1.0 - d / r;
        att = mix(att, att * att * max(L.z * inv_d, 0.0), uPtDown[i >> 2][i & 3]);
        float sv = 1.0;
        if (uPtCol[i].w >= 0.0) {
            float layer = floor(uPtCol[i].w), kw = clamp((fract(uPtCol[i].w) - 0.05) / 0.9, 0.0, 1.0);
            vec3 v = vShWorld + sh_n * (0.04 + 0.03 * d) - uPtPos[i].xyz;
            float f = r, nn = uPtNear;
            vec3 a = abs(v);
            float z = max(max(a.x, a.y), a.z);
            float ref = 0.5 * ((f + nn) / (f - nn) - (2.0 * f * nn) / ((f - nn) * z)) + 0.5 - 0.0002;
            float o = 0.015 * z;
            sv = 0.25 * (texture(uPtMap, vec4(v + vec3(o, o, 0.0), layer), ref)
                 + texture(uPtMap, vec4(v + vec3(-o, 0.0, o), layer), ref)
                 + texture(uPtMap, vec4(v + vec3(0.0, -o, -o), layer), ref)
                 + texture(uPtMap, vec4(v + vec3(o, -o, o), layer), ref));
            sv = mix(1.0, sv, kw);
        }
        add += uPtCol[i].rgb * (ndl_p * att * 0.5 * sv);
    }
    if (uShDebug == 3) return add;
    return c + add;
}
vec3 sh_apply_split(vec3 full, vec3 nokey) {
    if (sh_state.w == 0.0) return full;
    if (uShDebug == 2) return vec3(sh_state.x);
    return mix(nokey, full, sh_state.x);
}
#if defined(SH_POINTS) || defined(SH_SPLIT)
vec3 sh_emissive(vec3 lit, vec3 orig, vec3 tex, uint s) {
    if (s == 0u || uShOn == 0 || uShPass != 0 || uEmis[0].x == 0.0) return lit;
    vec4 e = uEmis[s];
    if (e.w > 0.0) return mix(lit, vec3(1.0, 0.0, 1.0), e.x);
    if (e.z > 0.0) return lit;
    float v = max(tex.r, max(tex.g, tex.b));
    float m = e.y > 0.0 ? smoothstep(e.y - 0.04, e.y + 0.04, v) : 1.0;
    return mix(lit, max(lit, orig * e.x), m);
}
#endif
vec3 sh_apply(vec3 c) {
    if (sh_state.w == 0.0) return c;
    if (uShDebug == 2) return vec3(sh_state.x * max(sh_state.y, 0.15));
    float k = 1.0 - uShParam.x * (1.0 - sh_state.x) * sh_state.y;
    vec3 o = c * k;
#ifdef SH_POINTS
    if (uShKey.w >= 0.0) {
        float ndg = dot(sh_n, uShKeyDir), nds = dot(sh_n, uShSun);
        vec3 kg = uShKey.rgb * max(ndg, uShKey.w * ndg), ks = uShKey.rgb * max(nds, uShKey.w * nds);
        o = max(c + uShSunK * (ks * sh_vis - kg), vec3(0.0));
    }
#endif
    if (uShDebug == 1) {
        int ci = int(sh_state.z);
        vec3 tint = ci == 0 ? vec3(1.0, 0.6, 0.6) : ci == 1 ? vec3(0.6, 1.0, 0.6) : ci == 2 ? vec3(0.6, 0.6, 1.0) : vec3(1.0);
        o *= tint;
    }
    return o;
}
""" + MATERIALS_GLSL + """"""

# ---------------------------------------------------------------------------
# 1. gs_draw.cpp shaders
# ---------------------------------------------------------------------------
GS_DRAW_PUSH = """
layout(push_constant) uniform Push {
    vec2 uFb;
    float uScaleX;
    int uRt;
    vec3 uFogCol;
    int uFge;
    vec4 uScrMap;
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
"""

GS_DRAW_VS = "#version 450\n" + GS_DRAW_PUSH + """
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec3 aTex;
layout(location = 3) in float aDepth;
layout(location = 4) in float aFog;
layout(location = 5) in vec4 aClamp;
layout(location = 0) out vec4 vColor;
layout(location = 1) out vec3 vTex;
layout(location = 2) out float vFog;
layout(location = 3) flat out vec4 vClamp;
void main() {
    float x = aPos.x / uFb.x * 2.0 - 1.0;
    float y = uRt != 0 ? (aPos.y / uFb.y * 2.0 - 1.0) : (1.0 - aPos.y / uFb.y * 2.0);
    gl_Position = vec4(x * uScaleX, y, aDepth * 2.0 - 1.0, 1.0);
    vColor = aColor;
    vTex = aTex;
    vFog = aFog;
    vClamp = aClamp;
}
"""

GS_DRAW_FS = "#version 450\n" + GS_DRAW_PUSH + """
layout(location = 0) in vec4 vColor;
layout(location = 1) in vec3 vTex;
layout(location = 2) in float vFog;
layout(location = 3) flat in vec4 vClamp;
layout(location = 0, index = 0) out vec4 FragColor;
layout(location = 0, index = 1) out vec4 FragFactor;
layout(set = 0, binding = 0) uniform sampler2D uTexture;
layout(set = 0, binding = 1) uniform sampler2D uScreen;
layout(set = 0, binding = 2) uniform sampler2D uMip1;
layout(set = 0, binding = 3) uniform sampler2D uMip2;
layout(set = 0, binding = 4) uniform sampler2D uMip3;
void main() {
    if (uDate != 0) {
        bool b7 = texelFetch(uScreen, ivec2(gl_FragCoord.xy), 0).a > 254.0 / 255.0;
        if ((uDatm == 0) == b7) discard;
    }
    vec4 c = vColor;
    vec4 o = c;
    if (uTme != 0) {
        vec2 st = vTex.xy / vTex.z;
        vec4 t;
        if (uScrTex != 0) {
            t = texelFetch(uScreen, ivec2(uScrMap.x + uScrMap.y * st.x, uScrMap.z + uScrMap.w * st.y), 0);
            t.a = t.a * (128.0 / 255.0);
        } else {
            int idx = 0;
            if (uMipN > 0) {
                float lod = uMipLcm != 0 ? uMipK : log2(1.0 / max(abs(vTex.z), 1e-20)) * uMipL + uMipK;
                int level = lod <= 0.0 ? 0 : min(int(floor(lod + 0.5)), uMipMxl);
                idx = clamp(level - uMipLo, 0, uMipN);
            }
            if (vClamp.x <= vClamp.z) st = clamp(st, vClamp.xy, vClamp.zw);
            if (idx == 0) t = texture(uTexture, st);
            else if (idx == 1) t = texture(uMip1, st);
            else if (idx == 2) t = texture(uMip2, st);
            else t = texture(uMip3, st);
        }
        if (uTfx == 0) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0); o.a = uTcc != 0 ? t.a * c.a * (255.0 / 128.0) : c.a; }
        else if (uTfx == 1) { o.rgb = t.rgb; o.a = uTcc != 0 ? t.a : c.a; }
        else if (uTfx == 2) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = uTcc != 0 ? t.a + c.a : c.a; }
        else { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = uTcc != 0 ? t.a : c.a; }
    }
    o = clamp(o, 0.0, 1.0);
    if (uFge != 0) {
        float f = vFog / 255.0;
        o.rgb = o.rgb * f + uFogCol * (1.0 - f);
    }
    float a = o.a * 255.0;
    if (uAte != 0) {
        bool pass = true;
        if (uAtst == 0) pass = false;
        else if (uAtst == 2) pass = a < float(uAref);
        else if (uAtst == 3) pass = a <= float(uAref);
        else if (uAtst == 4) pass = abs(a - float(uAref)) < 0.5;
        else if (uAtst == 5) pass = a >= float(uAref);
        else if (uAtst == 6) pass = a > float(uAref);
        else if (uAtst == 7) pass = abs(a - float(uAref)) >= 0.5;
        if (!pass && uPass == 0 && (uAfail == 0 || uAfail == 2)) discard;
        if (!pass && uPass == 1 && uAfail != 2) discard;
    }
    FragColor = uRt != 0 ? o : vec4(o.rgb, min(o.a * (255.0 / 128.0), 1.0));
    FragFactor = vec4(min(o.a * (255.0 / 128.0), 1.0));
}
"""

# ---------------------------------------------------------------------------
# 2. mrp_fx.cpp shaders
# ---------------------------------------------------------------------------
MRP_FX_PUSH = """
layout(push_constant) uniform Push {
    mat4 uVP;
    vec4 uFrame;
    vec4 uXyOff;
    int uMode;
    int uShade;
    int uPass;
    int _pad;
};
"""

MRP_FX_VS = "#version 450\n" + MRP_FX_PUSH + """
layout(location = 0) in vec4 aWorld;
layout(location = 1) in vec3 aStq;
layout(location = 2) in float aFog;
layout(location = 3) in uvec4 aRgba;
layout(location = 4) in uvec4 aTex;
layout(location = 5) in vec3 aVu;
layout(location = 0) smooth out vec3 vStq;
layout(location = 1) noperspective out vec4 vColor;
layout(location = 2) flat out vec4 vFlat;
layout(location = 3) noperspective out float vFog;
layout(location = 4) flat out uvec4 vTex;
void main() {
    if (uMode == 0) {
        gl_Position = uVP * aWorld;
    } else {
        float sx = uFrame.z / (uFrame.x * 0.5), sy = 1.0 / (uFrame.y * 0.5);
        gl_Position = vec4((aVu.x - uXyOff.x) * sx, -(aVu.y - uXyOff.y) * sy, 2.0 * (uXyOff.z + uXyOff.w * aVu.z) - 1.0, 1.0);
    }
    vStq = aStq;
    vColor = vec4(aRgba) / 255.0;
    vFlat = vColor;
    vFog = aFog;
    vTex = aTex;
}
"""

MRP_FX_FS = "#version 450\n" + MRP_FX_PUSH + """
layout(set = 0, binding = 0) uniform sampler2DArray uAtlas;
layout(location = 0) smooth in vec3 vStq;
layout(location = 1) noperspective in vec4 vColor;
layout(location = 2) flat in vec4 vFlat;
layout(location = 3) noperspective in float vFog;
layout(location = 4) flat in uvec4 vTex;
layout(location = 0, index = 0) out vec4 FragColor;
layout(location = 0, index = 1) out vec4 FragFactor;
ivec2 wrapc(ivec2 c, ivec2 sz, bool rs, bool rt) {
    c.x = rs ? ((c.x % sz.x) + sz.x) % sz.x : clamp(c.x, 0, sz.x - 1);
    c.y = rt ? ((c.y % sz.y) + sz.y) % sz.y : clamp(c.y, 0, sz.y - 1);
    return c;
}
void main() {
    uint fl = vTex.z;
    bool tme = (fl & 1u) != 0u, iip = ((fl >> 4) & 1u) != 0u, fge = ((fl >> 5) & 1u) != 0u, ate = ((fl >> 6) & 1u) != 0u;
    uint tfx = (fl >> 1) & 3u, tcc = (fl >> 3) & 1u, atst = (fl >> 7) & 7u, aref = (fl >> 10) & 0xffu, afail = (fl >> 22) & 3u;
    bool lin = ((fl >> 18) & 1u) != 0u, rs = ((fl >> 19) & 1u) != 0u, rt = ((fl >> 20) & 1u) != 0u;
    vec4 c = iip ? vColor : vFlat;
    vec4 o = c;
    if (tme) {
        ivec2 sz = ivec2(int(vTex.y & 0xffffu), int(vTex.y >> 16));
        vec2 uv = vStq.xy / vStq.z * vec2(sz);
        int layer = int(vTex.x);
        vec4 t;
        if (lin) {
            vec2 p = uv - 0.5;
            ivec2 i0 = ivec2(floor(p));
            vec2 f = p - vec2(i0);
            vec4 t00 = texelFetch(uAtlas, ivec3(wrapc(i0, sz, rs, rt), layer), 0);
            vec4 t10 = texelFetch(uAtlas, ivec3(wrapc(i0 + ivec2(1, 0), sz, rs, rt), layer), 0);
            vec4 t01 = texelFetch(uAtlas, ivec3(wrapc(i0 + ivec2(0, 1), sz, rs, rt), layer), 0);
            vec4 t11 = texelFetch(uAtlas, ivec3(wrapc(i0 + ivec2(1, 1), sz, rs, rt), layer), 0);
            t = mix(mix(t00, t10, f.x), mix(t01, t11, f.x), f.y);
        } else {
            t = texelFetch(uAtlas, ivec3(wrapc(ivec2(floor(uv)), sz, rs, rt), layer), 0);
        }
        if (tfx == 0u) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0); o.a = tcc != 0u ? t.a * c.a * (255.0 / 128.0) : c.a; }
        else if (tfx == 1u) { o.rgb = t.rgb; o.a = tcc != 0u ? t.a : c.a; }
        else if (tfx == 2u) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc != 0u ? t.a + c.a : c.a; }
        else { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc != 0u ? t.a : c.a; }
        if (uShade == 2) o.rgb = t.rgb;
    }
    if (uShade == 1) o.rgb = c.rgb;
    o = clamp(o, 0.0, 1.0);
    if (fge) {
        uint fc = vTex.w;
        vec3 fogcol = vec3(float(fc & 0xffu), float((fc >> 8) & 0xffu), float((fc >> 16) & 0xffu)) / 255.0;
        float f = vFog / 255.0;
        o.rgb = o.rgb * f + fogcol * (1.0 - f);
    }
    float a = o.a * 255.0;
    if (ate) {
        bool pass = true;
        if (atst == 0u) pass = false;
        else if (atst == 2u) pass = a < float(aref);
        else if (atst == 3u) pass = a <= float(aref);
        else if (atst == 4u) pass = abs(a - float(aref)) < 0.5;
        else if (atst == 5u) pass = a >= float(aref);
        else if (atst == 6u) pass = a > float(aref);
        else if (atst == 7u) pass = abs(a - float(aref)) >= 0.5;
        if (!pass && uPass == 0 && (afail == 0u || afail == 2u)) discard;
        if (!pass && uPass == 1 && afail != 2u) discard;
    }
    float as = min(o.a * (255.0 / 128.0), 1.0);
    uint bk = (fl >> 24) & 3u;
    if (bk == 1u || bk == 2u) {
        FragColor = vec4(o.rgb * as, as);
        FragFactor = vec4(bk == 1u ? 1.0 : 1.0 - as);
    } else {
        FragColor = vec4(o.rgb, as);
        FragFactor = vec4(0.0);
    }
}
"""

# the program 7 surfaces (mrp_fx.cpp kV7Vs / kV7Fs): the effects' fragment shader with the program's fog and the modern lighting
MRP_VU7_PUSH = """
layout(push_constant) uniform Push {
    mat4 uVP;
    int uShade;
    int uPass;
    int _pad0;
    int _pad1;
};
"""

MRP_VU7_VS = "#version 450\n" + MRP_VU7_PUSH + RAC1_VK_SHADOW_VS + """
layout(location = 0) in vec3 aWorld;
layout(location = 1) in vec3 aStq;
layout(location = 2) in float aFogW;
layout(location = 3) in uvec4 aRgba;
layout(location = 4) in uvec4 aTex;
layout(location = 5) in vec2 aFogC;
layout(location = 6) in uint aEmis;
layout(location = 7) in float aDepth;
layout(location = 0) smooth out vec3 vStq;
layout(location = 1) noperspective out vec4 vColor;
layout(location = 2) flat out vec4 vFlat;
layout(location = 3) noperspective out float vFogW;
layout(location = 4) flat out vec2 vFogC;
layout(location = 5) flat out uvec4 vTex;
layout(location = 6) flat out uint vEmis;
void main() {
    gl_Position = sh_clip(uVP * vec4(aWorld, 1.0));
    if (aDepth < 1.5) gl_Position.z = aDepth * gl_Position.w;  // (the GS's depth, as the OpenGL shader)
    vStq = aStq;
    vColor = vec4(aRgba) / 255.0;
    vFlat = vColor;
    vFogW = aFogW;
    vFogC = aFogC;
    vTex = aTex;
    vEmis = aEmis;
}
"""

MRP_VU7_FS = "#version 450\n#define SH_SPLIT 1\n" + MRP_VU7_PUSH + RAC1_VK_SHADOW_FS + """
layout(set = 0, binding = 0) uniform sampler2DArray uAtlas;
layout(location = 0) smooth in vec3 vStq;
layout(location = 1) noperspective in vec4 vColor;
layout(location = 2) flat in vec4 vFlat;
layout(location = 3) noperspective in float vFogW;
layout(location = 4) flat in vec2 vFogC;
layout(location = 5) flat in uvec4 vTex;
layout(location = 6) flat in uint vEmis;
layout(location = 0, index = 0) out vec4 FragColor;
layout(location = 0, index = 1) out vec4 FragFactor;
ivec2 wrapc(ivec2 c, ivec2 sz, bool rs, bool rt) {
    c.x = rs ? ((c.x % sz.x) + sz.x) % sz.x : clamp(c.x, 0, sz.x - 1);
    c.y = rt ? ((c.y % sz.y) + sz.y) % sz.y : clamp(c.y, 0, sz.y - 1);
    return c;
}
void main() {
    sh_prepare();
    uint fl = vTex.z;
    bool tme = (fl & 1u) != 0u, iip = ((fl >> 4) & 1u) != 0u, fge = ((fl >> 5) & 1u) != 0u, ate = ((fl >> 6) & 1u) != 0u;
    uint tfx = (fl >> 1) & 3u, tcc = (fl >> 3) & 1u, atst = (fl >> 7) & 7u, aref = (fl >> 10) & 0xffu, afail = (fl >> 22) & 3u;
    bool lin = ((fl >> 18) & 1u) != 0u, rs = ((fl >> 19) & 1u) != 0u, rt = ((fl >> 20) & 1u) != 0u;
    vec4 c = iip ? vColor : vFlat;
    vec3 c_game = c.rgb;
    c.rgb = sh_points(sh_apply_split(c.rgb, c.rgb * 0.45));
    vec4 o = c;
    if (tme) {
        ivec2 sz = ivec2(int(vTex.y & 0xffffu), int(vTex.y >> 16));
        vec2 uv = sh_mat_uv(vStq.xy / vStq.z, vEmis) * vec2(sz);  // (lava, mud, quicksand: the materials' slow flow)
        int layer = int(vTex.x);
        vec4 t;
        if (lin) {
            vec2 p = uv - 0.5;
            ivec2 i0 = ivec2(floor(p));
            vec2 f = p - vec2(i0);
            vec4 t00 = texelFetch(uAtlas, ivec3(wrapc(i0, sz, rs, rt), layer), 0);
            vec4 t10 = texelFetch(uAtlas, ivec3(wrapc(i0 + ivec2(1, 0), sz, rs, rt), layer), 0);
            vec4 t01 = texelFetch(uAtlas, ivec3(wrapc(i0 + ivec2(0, 1), sz, rs, rt), layer), 0);
            vec4 t11 = texelFetch(uAtlas, ivec3(wrapc(i0 + ivec2(1, 1), sz, rs, rt), layer), 0);
            t = mix(mix(t00, t10, f.x), mix(t01, t11, f.x), f.y);
        } else {
            t = texelFetch(uAtlas, ivec3(wrapc(ivec2(floor(uv)), sz, rs, rt), layer), 0);
        }
        if (tfx == 0u) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0); o.a = tcc != 0u ? t.a * c.a * (255.0 / 128.0) : c.a; }
        else if (tfx == 1u) { o.rgb = t.rgb; o.a = tcc != 0u ? t.a : c.a; }
        else if (tfx == 2u) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc != 0u ? t.a + c.a : c.a; }
        else { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc != 0u ? t.a : c.a; }
        o.rgb = sh_emissive(o.rgb, tfx == 1u ? t.rgb : t.rgb * c_game * (255.0 / 128.0), t.rgb, vEmis);
        o.rgb = sh_material(o.rgb, tfx == 1u ? t.rgb : t.rgb * c_game * (255.0 / 128.0), t.rgb, tfx == 1u ? vec3(1.0) : c.rgb * (255.0 / 128.0), vEmis);
        if (uShade == 2) o.rgb = t.rgb;
    }
    if (uShade == 1) o.rgb = c.rgb;
    o = clamp(o, 0.0, 1.0);
    if (fge) {
        uint fc = vTex.w;
        vec3 fogcol = vec3(float(fc & 0xffu), float((fc >> 8) & 0xffu), float((fc >> 16) & 0xffu)) / 255.0;
        float fw = min(max(vFogW, vFogC.x), vFogC.y);
        float f = float(uint(max(fw, 0.0)) & 0xffu) / 255.0;
        o.rgb = o.rgb * f + fogcol * (1.0 - f);
    }
    float a = o.a * 255.0;
    if (ate) {
        bool pass = true;
        if (atst == 0u) pass = false;
        else if (atst == 2u) pass = a < float(aref);
        else if (atst == 3u) pass = a <= float(aref);
        else if (atst == 4u) pass = abs(a - float(aref)) < 0.5;
        else if (atst == 5u) pass = a >= float(aref);
        else if (atst == 6u) pass = a > float(aref);
        else if (atst == 7u) pass = abs(a - float(aref)) >= 0.5;
        if (!pass && uPass == 0 && (afail == 0u || afail == 2u)) discard;
        if (!pass && uPass == 1 && afail != 2u) discard;
    }
    float as = min(o.a * (255.0 / 128.0), 1.0);
    uint bk = (fl >> 24) & 3u;
    if (bk == 1u || bk == 2u) {
        FragColor = vec4(o.rgb * as, as);
        FragFactor = vec4(bk == 1u ? 1.0 : 1.0 - as);
    } else {
        FragColor = vec4(o.rgb, as);
        FragFactor = vec4(0.0);
    }
}
"""

# ---------------------------------------------------------------------------
# 3. mrp_shadows.cpp shaders
# ---------------------------------------------------------------------------
MRP_SHADOWS_FACE_VS = """#version 450
layout(location = 0) in vec3 aPos;
layout(push_constant) uniform Push { mat4 uVP; };
void main() {
    gl_Position = uVP * vec4(aPos, 1.0);
}
"""

MRP_SHADOWS_FACE_FS = """#version 450
void main() {}
"""

MRP_SHADOWS_DARK_VS = """#version 450
void main() {
    vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
"""

MRP_SHADOWS_DARK_FS = """#version 450
layout(push_constant) uniform Push { vec4 uColor; };
layout(location = 0) out vec4 oCol;
void main() { oCol = uColor; }
"""

# ---------------------------------------------------------------------------
# 4. mrp_tfrag_gpu.cpp compute shader
# ---------------------------------------------------------------------------
MRP_TFRAG_CS = """#version 460
layout(local_size_x = 64) in;
struct Draw { uint image, nq, plan, colour, col_addr, col_count, view, consts, vbase, ibase, rbase, flags, wview, p1, p2, p3; };
layout(std430, set = 0, binding = 0) readonly buffer Draws { Draw draws[]; };
layout(std430, set = 0, binding = 1) readonly buffer Images { uvec4 images[]; };
layout(std430, set = 0, binding = 2) readonly buffer Plans { uint plans[]; };
layout(std430, set = 0, binding = 3) readonly buffer Cols { uint cols[]; };
layout(std430, set = 0, binding = 4) readonly buffer Views { double views[]; };
layout(std430, set = 0, binding = 5) readonly buffer VuViews { float vuviews[]; };
layout(std430, set = 0, binding = 6) readonly buffer Consts { uvec4 consts[]; };
layout(std430, set = 0, binding = 7) readonly buffer Runs { uvec4 runs[]; };
struct V { float wp0, wp1, wp2, wp3, vu0, vu1, vu2, vu3, s, t, q, lodq; uint rgba; float fog; uint word, layer, st0, st1; };
layout(std430, set = 0, binding = 8) writeonly buffer OutV { V outv[]; };
layout(std430, set = 0, binding = 9) writeonly buffer OutI { uint outi[]; };

shared uvec4 vu[0x148];
shared uvec4 kc[14];
shared uint fl[2048];

float ftz(float x) {
    uint u = floatBitsToUint(x);
    return (u & 0x7f800000u) == 0u ? uintBitsToFloat(u & 0x80000000u) : x;
}
float chop(float s, float e) {
    if (e == 0.0 || s == 0.0 || isinf(s) || isnan(s)) return ftz(s);
    if ((e < 0.0) != (s < 0.0)) s = uintBitsToFloat(floatBitsToUint(s) - 1u);
    return ftz(s);
}
float addz(float a, float b) {
    precise float x = ftz(a);
    precise float y = ftz(b);
    precise float s = x + y;
    precise float bb = s - x;
    precise float e = (x - (s - bb)) + (y - bb);
    return chop(s, e);
}
float subz(float a, float b) { return addz(a, -b); }
float mulz(float a, float b) {
    precise float x = ftz(a);
    precise float y = ftz(b);
    precise float p = x * y;
    precise float e = fma(x, y, -p);
    return chop(p, e);
}
vec4 add4(vec4 a, vec4 b) { return vec4(addz(a.x, b.x), addz(a.y, b.y), addz(a.z, b.z), addz(a.w, b.w)); }
vec4 mul4(vec4 a, vec4 b) { return vec4(mulz(a.x, b.x), mulz(a.y, b.y), mulz(a.z, b.z), mulz(a.w, b.w)); }
vec4 mul4s(vec4 a, float b) { return vec4(mulz(a.x, b), mulz(a.y, b), mulz(a.z, b), mulz(a.w, b)); }
float divz(float a, float b) {
    a = ftz(a);
    b = ftz(b);
    precise float q = a / b;
    if (isinf(q) || isnan(q) || q == 0.0) return ftz(q);
    precise float r = fma(-q, b, a);
    precise float q2 = fma(r, 1.0 / b, q);
    float best = q2;
    precise float br = abs(fma(-q2, b, a));
    float qn = uintBitsToFloat(floatBitsToUint(q2) + 1u), qp = uintBitsToFloat(floatBitsToUint(q2) - 1u);
    precise float rn = abs(fma(-qn, b, a));
    precise float rp = abs(fma(-qp, b, a));
    if (rn < br) { best = qn; br = rn; }
    if (rp < br) best = qp;
    precise float res = fma(-best, b, a);
    if (res != 0.0 && ((res < 0.0) != (a < 0.0))) best = uintBitsToFloat(floatBitsToUint(best) - 1u);
    return ftz(best);
}
double dstep(double s) {
    uvec2 u = unpackDouble2x32(s);
    if (u.x == 0u) { u.x = 0xffffffffu; u.y -= 1u; } else u.x -= 1u;
    return packDouble2x32(u);
}
double dchop(double s, double e) {
    if (e == 0.0LF || s == 0.0LF) return s;
    if ((e < 0.0LF) != (s < 0.0LF)) s = dstep(s);
    return s;
}
double daddz(double a, double b) {
    precise double s = a + b;
    precise double bb = s - a;
    precise double e = (a - (s - bb)) + (b - bb);
    return dchop(s, e);
}
double dmulz(double a, double b) {
    precise double p = a * b;
    precise double e = fma(a, b, -p);
    return dchop(p, e);
}
float d2fz(double d) {
    uvec2 u = unpackDouble2x32(d);
    uint sign = u.y & 0x80000000u;
    int e = int((u.y >> 20) & 0x7ffu) - 1023 + 127;
    if (e <= 0) return uintBitsToFloat(sign);
    if (e >= 255) return uintBitsToFloat(sign | 0x7f7fffffu);
    return uintBitsToFloat(sign | uint(e) << 23 | (u.y & 0xfffffu) << 3 | u.x >> 29);
}

uvec4 at(uint a) { return a < 0x148u ? vu[a] : uvec4(0u); }
float fa(uint a, uint l) { return uintBitsToFloat(at(a)[l]); }
float kf(uint c, uint l) { return uintBitsToFloat(kc[c - 0x290u][l]); }
uint u16(uint a, uint l) { return at(a)[l] & 0xffffu; }
vec4 ld(uint a) { return uintBitsToFloat(at(a)); }
vec4 itof0(uint a) { return vec4(ivec4(at(a))); }
void st(uint a, vec4 v) { if (a < 0x148u) vu[a] = floatBitsToUint(v); }
vec4 xform(vec4 p) {
    vec4 a = ld(8u);
    a = add4(a, mul4s(ld(5u), p.x));
    a = add4(a, mul4s(ld(6u), p.y));
    return add4(a, mul4s(ld(7u), p.z));
}
float fmin_(float a, float b) { return b < a ? b : a; }
float fmax_(float a, float b) { return a < b ? b : a; }
int ftoi(float v) {
    if (isnan(v)) return 0x7fffffff;
    if (!(v < 2147483648.0)) return 0x7fffffff;
    if (!(v > -2147483648.0)) return int(0x80000000u);
    return int(v);
}

void step_iter(uint kind, uint band, uint a, uint b, uint k) {
    if (kind == 0u) {
        uint ad = a + 2u * k;
        vec4 p = xform(itof0(ad));
        vec4 c = itof0(ad + 1u);
        st(ad, p);
        st(ad + 1u, c);
    } else if (kind <= 2u) {
        float kx = kf(band, 0u), ky = kf(band, 1u), cx = kf(band + 2u, 0u), cy = kf(band + 2u, 1u), thr = kf(band, 3u);
        float mx = kf(0x291u, 0u), my = kf(0x291u, 1u);
        uint via = a + k;
        uint rec = u16(via, 3u);
        uint pvi = at(b + k / 4u)[k % 4u] & 0xffffu;
        uint r1 = u16(pvi, 3u), r2 = u16(via, 2u);
        vec4 p = xform(itof0(rec));
        vec4 c = itof0(rec + 1u);
        vec4 P1 = ld(r1), C1 = ld(r1 + 1u), P2 = ld(r2), C2 = ld(r2 + 1u);
        float fx = addz(mulz(cx, 1.0), mulz(kx, p.w));
        float fy = addz(mulz(cy, 1.0), mulz(ky, p.w));
        fx = fmax_(fmin_(fx, mx), 0.0);
        fy = fmax_(fmin_(fy, my), 0.0);
        vec4 S = add4(P1, P2);
        vec4 SC = add4(C1, C2);
        vec4 po = add4(mul4s(S, fx), mul4s(p, fy));
        vec4 co = add4(mul4s(SC, fx), mul4s(c, fy));
        float d1 = subz(P1.w, thr);
        float d2 = subz(P2.w, thr);
        if (kind == 2u && d1 < 0.0 && d2 < 0.0) {
            vu[via] = at(pvi);
            return;
        }
        st(rec, po);
        st(rec + 1u, co);
    } else {
        float thr = kf(band, 3u);
        uint la = a + k;
        uint vi = at(b + k / 4u)[k % 4u] & 0xffffu;
        uint ra = u16(la, 2u), rb = u16(vi, 3u);
        float d1 = subz(fa(ra, 3u), thr);
        float d2 = subz(fa(rb, 3u), thr);
        if (d1 < 0.0 && d2 < 0.0) vu[la] = at(vi);
    }
}

void main() {
    Draw d = draws[gl_WorkGroupID.x];
    uint tid = gl_LocalInvocationID.x;
    for (uint q = tid; q < 0x148u; q += 64u) vu[q] = q < d.nq ? images[d.image + q] : uvec4(0u);
    if (tid < 14u) kc[tid] = consts[d.consts * 14u + tid];
    for (uint j = tid; j < 2048u; j += 64u) fl[j] = 0u;
    barrier();
    if (tid < 16u) vu[5u + tid / 4u][tid % 4u] = floatBitsToUint(vuviews[d.view * 16u + tid]);
    barrier();
    for (uint k = tid; k < min(d.col_count, 0x148u); k += 64u) {
        uint w = d.colour != 0xffffffffu ? cols[d.colour + k] : 0u;
        if (d.col_addr + 2u * k < 0x148u) vu[d.col_addr + 2u * k] = uvec4(w & 0xffu, (w >> 8) & 0xffu, (w >> 16) & 0xffu, w >> 24);
    }
    barrier();
    uint P = d.plan;
    uint hdr = plans[P];
    uint nsteps = min(hdr & 0xffu, 4u);
    int form = int((hdr >> 8) & 7u);
    uint nv = min(plans[P + 1u], 8192u);
    uint sp = P + 3u;
    for (uint si = 0u; si < nsteps; ++si) {
        uint w0 = plans[sp], w1 = plans[sp + 1u], n = min(plans[sp + 2u], 1312u);
        sp += 3u;
        uint kind = w0 & 0xffu, band = w0 >> 16, a = w1 & 0xffffu, b = w1 >> 16;
        if ((w0 & 0x100u) != 0u) {
            if (tid == 0u)
                for (uint k = 0u; k < n; ++k) step_iter(kind, band, a, b, k);
        } else {
            for (uint k = tid; k < n; k += 64u) step_iter(kind, band, a, b, k);
        }
        memoryBarrierShared();
        barrier();
    }
    uint vp = sp;
    bool bform = form == 4;
    if (bform) form = 3;
    bool guard = (form == 1 || form == 3) && (d.flags & 0x80000000u) == 0u;
    uint tex = u16(4u, 3u);
    float stoff = (form == 1 || bform) ? fa(tex + 1u, 3u) : kf(0x291u, 2u);
    vec4 c0 = uintBitsToFloat(kc[0]), c4 = uintBitsToFloat(kc[8]), c5 = uintBitsToFloat(kc[5]), c6 = uintBitsToFloat(kc[6]);
    if (guard) {
        for (uint j = tid; j < nv; j += 64u) {
            uint rec = u16(plans[vp + 2u * j] & 0xffffu, 3u);
            vec4 Pj = form <= 1 ? ld(rec) : xform(itof0(rec));
            vec4 g = mul4(Pj, c6);
            float w = abs(g.w);
            uint f = 0u;
            if (g.x > w) f |= 1u;
            if (g.x < -w) f |= 2u;
            if (g.y > w) f |= 4u;
            if (g.y < -w) f |= 8u;
            if (g.z > w) f |= 16u;
            if (g.z < -w) f |= 32u;
            if (f != 0u) atomicOr(fl[j >> 2], f << (8u * (j & 3u)));
        }
        memoryBarrierShared();
        barrier();
    }
    for (uint j = tid; j < nv; j += 64u) {
        uint v0 = plans[vp + 2u * j], v1 = plans[vp + 2u * j + 1u];
        uint via = v0 & 0xffffu, pos = (v0 >> 16) & 3u;
        uint rec = u16(via, 3u);
        vec4 Pj = form <= 1 ? ld(rec) : xform(itof0(rec));
        vec4 hp = Pj;
        float s0 = subz(fa(via, 0u), stoff);
        float t0 = subz(fa(via, 1u), stoff);
        float q0 = subz(addz(1.0, stoff), stoff);
        bool adc = false;
        if (guard && !bform) {
            uint h = 0u;
            if ((v0 & (1u << 18)) != 0u) h |= (fl[j >> 2] >> (8u * (j & 3u))) & 0xffu;
            if (pos >= 1u && (plans[vp + 2u * (j - 1u)] & (1u << 18)) != 0u) h |= (fl[(j - 1u) >> 2] >> (8u * ((j - 1u) & 3u))) & 0xffu;
            if (pos >= 2u && (plans[vp + 2u * (j - 2u)] & (1u << 18)) != 0u) h |= (fl[(j - 2u) >> 2] >> (8u * ((j - 2u) & 3u))) & 0xffu;
            adc = h != 0u;
        }
        float q = divz(c0.x, Pj.w);
        float qq = mulz(q0, q);
        float ss = mulz(s0, q);
        float tt = mulz(t0, q);
        float fw = addz(Pj.w, c5.w);
        if (form == 3) { fw = fmin_(fw, c0.z); fw = fmax_(fw, c0.y); }
        else { fw = fmax_(fw, c0.y); fw = fmin_(fw, c0.z); }
        if (adc) fw = addz(fw, c0.w);
        uvec4 rgba;
        if (form <= 1) {
            vec4 C = mul4(ld(rec + 1u), c4);
            rgba = uvec4(ftoi(C.x), ftoi(C.y), ftoi(C.z), ftoi(C.w));
        } else {
            rgba = at(rec + 1u);
        }
        uint xyzf3 = uint(ftoi(mulz(fw, 16.0)));
        uint wv = d.wview * 16u;
        double h0 = double(hp.x), h1 = double(hp.y), h2 = double(hp.z), h3 = double(hp.w);
        double x0 = daddz(daddz(daddz(dmulz(views[wv + 0u], h0), dmulz(views[wv + 4u], h1)), dmulz(views[wv + 8u], h2)), dmulz(views[wv + 12u], h3));
        double x1 = daddz(daddz(daddz(dmulz(views[wv + 1u], h0), dmulz(views[wv + 5u], h1)), dmulz(views[wv + 9u], h2)), dmulz(views[wv + 13u], h3));
        double x2 = daddz(daddz(daddz(dmulz(views[wv + 2u], h0), dmulz(views[wv + 6u], h1)), dmulz(views[wv + 10u], h2)), dmulz(views[wv + 14u], h3));
        double x3 = daddz(daddz(daddz(dmulz(views[wv + 3u], h0), dmulz(views[wv + 7u], h1)), dmulz(views[wv + 11u], h2)), dmulz(views[wv + 15u], h3));
        float kk = mulz(qq, hp.w);
        float sgn = kk < 0.0 ? -1.0 : 1.0;
        float Wg = mulz(hp.w, sgn);
        float ws = Wg != 0.0 ? Wg : 1.0;
        V o;
        o.wp0 = d2fz(x0); o.wp1 = d2fz(x1); o.wp2 = d2fz(x2); o.wp3 = d2fz(x3);
        o.vu0 = 0.0; o.vu1 = 0.0; o.vu2 = 0.0; o.vu3 = 0.0;
        o.s = mulz(ss, ws); o.t = mulz(tt, ws); o.q = mulz(qq, ws);
        o.lodq = qq;
        o.rgba = (rgba.x & 0xffu) | (rgba.y & 0xffu) << 8 | (rgba.z & 0xffu) << 16 | (rgba.w & 0xffu) << 24;
        o.fog = float((xyzf3 >> 4) & 0xffu);
        uvec4 run = runs[d.rbase + (v1 & 0xffffu)];
        o.word = run.x; o.layer = run.y; o.st0 = run.z; o.st1 = run.w;
        outv[d.vbase + j] = o;
        if (pos >= 2u) {
            uint t = d.ibase + 3u * (v1 >> 16);
            uint c = d.vbase + j;
            if (adc) { outi[t] = c; outi[t + 1u] = c; outi[t + 2u] = c; }
            else { outi[t] = c - 2u; outi[t + 1u] = c - 1u; outi[t + 2u] = c; }
        }
    }
}
"""

# ---------------------------------------------------------------------------
# 5. mrp_lighting.cpp post-process shaders
# ---------------------------------------------------------------------------
MRP_AO_VS = """#version 450
layout(location = 0) out vec2 vUv;
void main() {
    vUv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(vUv * 2.0 - 1.0, 0.0, 1.0);
}
"""

MRP_AO_FS = """#version 450
layout(location = 0) in vec2 vUv;
layout(location = 0) out float FragAo;
layout(set = 0, binding = 0) uniform sampler2D uDepth;
layout(push_constant) uniform Push {
    mat4 uInvVP;
    mat4 uVP;
    vec4 uCam_Pad;
    vec4 uParam;
};
#define uCam (uCam_Pad.xyz)
vec3 world(vec2 uv, float d) {
    vec4 p = uInvVP * vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    return p.xyz / p.w;
}
void main() {
    float d = texture(uDepth, vUv).r;
    vec3 P = world(vUv, d);
    vec3 dPdx = dFdx(P), dPdy = dFdy(P);
    if (d >= 1.0) { FragAo = 1.0; return; }
    int mode = int(uParam.w + 0.5);
    vec3 N;
    if (mode <= 1) {
        N = cross(dPdx, dPdy);
    } else {
        vec2 texel = 1.0 / vec2(textureSize(uDepth, 0));
        float dr = texture(uDepth, vUv + vec2(texel.x, 0.0)).r;
        float dl = texture(uDepth, vUv - vec2(texel.x, 0.0)).r;
        float du = texture(uDepth, vUv + vec2(0.0, texel.y)).r;
        float dd = texture(uDepth, vUv - vec2(0.0, texel.y)).r;
        vec3 dPx = (abs(dr - d) < abs(d - dl) && dr < 1.0) ? (world(vUv + vec2(texel.x, 0.0), dr) - P)
                                                            : (dl < 1.0 ? (P - world(vUv - vec2(texel.x, 0.0), dl)) : dPdx);
        vec3 dPy = (abs(du - d) < abs(d - dd) && du < 1.0) ? (world(vUv + vec2(0.0, texel.y), du) - P)
                                                            : (dd < 1.0 ? (P - world(vUv - vec2(0.0, texel.y), dd)) : dPdy);
        N = cross(dPx, dPy);
    }
    if (dot(N, N) < 1e-12) { FragAo = 1.0; return; }
    N = normalize(N);
    vec3 V = uCam - P;
    float vlen = length(V);
    V /= max(vlen, 1e-5);
    if (dot(N, V) < 0.0) N = -N;
    float noise = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    float noise2 = fract(52.9829189 * fract(dot(gl_FragCoord.xy + vec2(17.0, 31.0), vec2(0.06711056, 0.00583715))));
    vec3 t = normalize(abs(N.z) < 0.9 ? cross(N, vec3(0.0, 0.0, 1.0)) : cross(N, vec3(1.0, 0.0, 0.0)));
    vec3 b = cross(N, t);
    float R = uParam.x;
    {
        vec4 c0 = uVP * vec4(P, 1.0), c1 = uVP * vec4(P + b * R, 1.0);
        if (c0.w > 0.0 && c1.w > 0.0) {
            float px = length(c1.xy / c1.w - c0.xy / c0.w) * 0.5;
            if (px > 0.04) R *= 0.04 / px;
        }
    }
    float invR2 = 1.0 / max(R * R, 1e-4);

    if (mode <= 2) {
        float a = 6.2831853 * noise;
        vec3 T = t * cos(a) + b * sin(a), B = cross(N, T);
        float occ = 0.0;
        const int K = 12;
        for (int i = 0; i < K; ++i) {
            float fi = (float(i) + 0.5) / float(K);
            float z = sqrt(1.0 - fi);
            float r = sqrt(fi), ph = float(i) * 2.3999632;
            vec3 h = T * (r * cos(ph)) + B * (r * sin(ph)) + N * z;
            float s = mix(0.15, 1.0, fi * fi) * R;
            vec3 S = P + N * uParam.z + h * s;
            vec4 c = uVP * vec4(S, 1.0);
            if (c.w <= 0.0) continue;
            vec2 suv = c.xy / c.w * 0.5 + 0.5;
            if (any(lessThan(suv, vec2(0.0))) || any(greaterThan(suv, vec2(1.0)))) continue;
            float sd = texture(uDepth, suv).r;
            float zs = c.z / c.w * 0.5 + 0.5;
            if (sd < zs - 0.00002) {
                float dist = length(world(suv, sd) - S);
                occ += smoothstep(0.0, 1.0, R * 1.5 / max(dist, 1e-3) - 0.5);
            }
        }
        FragAo = 1.0 - uParam.y * occ / float(K);
        return;
    }

    if (mode == 3) {
        const int D = 4, S = 3;
        float occ = 0.0;
        for (int d_i = 0; d_i < D; ++d_i) {
            float phi = (float(d_i) + noise) * (6.2831853 / float(D));
            vec3 dir = t * cos(phi) + b * sin(phi);
            float max_sin_h = 0.12;
            float best_att = 0.0;
            for (int s_i = 1; s_i <= S; ++s_i) {
                float frac_r = (float(s_i) - 0.5 + 0.5 * noise2) / float(S);
                vec3 Q = P + N * uParam.z + dir * (frac_r * R);
                vec4 c = uVP * vec4(Q, 1.0);
                if (c.w <= 0.0) continue;
                vec2 suv = c.xy / c.w * 0.5 + 0.5;
                if (any(lessThan(suv, vec2(0.0))) || any(greaterThan(suv, vec2(1.0)))) continue;
                float sd = texture(uDepth, suv).r;
                if (sd >= 1.0) continue;
                vec3 H = world(suv, sd) - P;
                float d2 = dot(H, H);
                if (d2 < 1e-5 || d2 > R * R * 1.44) continue;
                float sin_h = dot(H, N) * inversesqrt(d2);
                if (sin_h > max_sin_h) {
                    float att = clamp(1.0 - d2 * invR2, 0.0, 1.0);
                    best_att += (sin_h - max_sin_h) * att;
                    max_sin_h = sin_h;
                }
            }
            occ += clamp(best_att, 0.0, 1.0);
        }
        FragAo = clamp(1.0 - uParam.y * 1.25 * (occ / float(D)), 0.0, 1.0);
        return;
    }

    const int D = 3, S = 3;
    float vis = 0.0;
    for (int d_i = 0; d_i < D; ++d_i) {
        float phi = (float(d_i) + noise) * (3.14159265 / float(D));
        vec3 dir = t * cos(phi) + b * sin(phi);
        float h1 = -1.0, h2 = -1.0;
        for (int s_i = 1; s_i <= S; ++s_i) {
            float step_r = ((float(s_i) - 0.5 + 0.5 * noise2) / float(S)) * R;
            vec4 cp = uVP * vec4(P + N * uParam.z + dir * step_r, 1.0);
            if (cp.w > 0.0) {
                vec2 uvp = cp.xy / cp.w * 0.5 + 0.5;
                if (all(greaterThanEqual(uvp, vec2(0.0))) && all(lessThanEqual(uvp, vec2(1.0)))) {
                    float sdp = texture(uDepth, uvp).r;
                    if (sdp < 1.0) {
                        vec3 Hp = world(uvp, sdp) - P;
                        float d2 = dot(Hp, Hp);
                        if (d2 > 1e-5) {
                            float c_h = dot(Hp, N) * inversesqrt(d2);
                            float falloff = clamp(1.0 - d2 * invR2, 0.0, 1.0);
                            h1 = max(h1, mix(-1.0, c_h, falloff));
                        }
                    }
                }
            }
            vec4 cn = uVP * vec4(P + N * uParam.z - dir * step_r, 1.0);
            if (cn.w > 0.0) {
                vec2 uvn = cn.xy / cn.w * 0.5 + 0.5;
                if (all(greaterThanEqual(uvn, vec2(0.0))) && all(lessThanEqual(uvn, vec2(1.0)))) {
                    float sdn = texture(uDepth, uvn).r;
                    if (sdn < 1.0) {
                        vec3 Hn = world(uvn, sdn) - P;
                        float d2 = dot(Hn, Hn);
                        if (d2 > 1e-5) {
                            float c_h = dot(Hn, N) * inversesqrt(d2);
                            float falloff = clamp(1.0 - d2 * invR2, 0.0, 1.0);
                            h2 = max(h2, mix(-1.0, c_h, falloff));
                        }
                    }
                }
            }
        }
        float slice_vis = 0.5 * ((1.0 - max(h1, 0.0)) + (1.0 - max(h2, 0.0)));
        vis += clamp(slice_vis, 0.0, 1.0);
    }
    vis /= float(D);
    float mb = vis / (vis * 0.6 + 0.4);
    FragAo = clamp(mix(1.0, mix(vis, mb, 0.35), uParam.y * 1.15), 0.0, 1.0);
}
"""

MRP_AO_BLUR_FS = """#version 450
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 0) uniform sampler2D uAo;
layout(set = 0, binding = 1) uniform sampler2D uDepth;
layout(set = 0, binding = 2) uniform usampler2D uStencil;  // (the depth's stencil: bit 0 the mobys' pixels, mrp_lighting_ao_moby_mark)
layout(push_constant) uniform Push {
    vec2 uTexel;
    float uMoby;  // (the share of the occlusion the mobys' pixels keep; 1: all, the stencil not read)
    float _pad;
};
void main() {
    float d0 = texture(uDepth, vUv).r;
    if (d0 >= 1.0) discard;
    float sum = 0.0, w = 0.0;
    for (int y = -2; y <= 2; ++y)
        for (int x = -2; x <= 2; ++x) {
            vec2 uv = vUv + vec2(x, y) * uTexel;
            float d = texture(uDepth, uv).r;
            float k = abs(d - d0) < 0.0005 ? 1.0 : 0.0;
            sum += texture(uAo, uv).r * k;
            w += k;
        }
    float ao = w > 0.0 ? sum / w : 1.0;
    float k = 1.0;
    if (uMoby < 1.0 && (texture(uStencil, vUv).r & 1u) != 0u) k = uMoby;
    FragColor = vec4(vec3(mix(1.0, ao, k)), 1.0);
}
"""

MRP_AO_BLUR_SEP_FS = """#version 450
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 0) uniform sampler2D uAo;
layout(set = 0, binding = 1) uniform sampler2D uDepth;
layout(set = 0, binding = 2) uniform usampler2D uStencil;  // (the depth's stencil: bit 0 the mobys' pixels, mrp_lighting_ao_moby_mark)
layout(push_constant) uniform Push {
    vec2 uTexel;
    float uMoby;  // (the share of the occlusion the mobys' pixels keep; 1: all, the stencil not read)
    float _pad;
};
void main() {
    float d0 = texture(uDepth, vUv).r;
    if (d0 >= 1.0) { FragColor = vec4(1.0); return; }
    const float gw[5] = float[5](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    float sum = texture(uAo, vUv).r * gw[0], w = gw[0];
    for (int i = 1; i <= 4; ++i) {
        vec2 off = uTexel * float(i);
        vec2 uv1 = vUv + off, uv2 = vUv - off;
        float d1 = texture(uDepth, uv1).r, d2 = texture(uDepth, uv2).r;
        float k1 = gw[i] * exp(-abs(d1 - d0) * 3500.0);
        float k2 = gw[i] * exp(-abs(d2 - d0) * 3500.0);
        sum += texture(uAo, uv1).r * k1 + texture(uAo, uv2).r * k2;
        w += k1 + k2;
    }
    float ao = w > 1e-5 ? sum / w : 1.0;
    float k = 1.0;
    if (uMoby < 1.0 && (texture(uStencil, vUv).r & 1u) != 0u) k = uMoby;
    FragColor = vec4(vec3(mix(1.0, ao, k)), 1.0);
}
"""

MRP_RAYS_MASK_FS = """#version 450
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 0) uniform sampler2D uDepth;
layout(set = 0, binding = 1) uniform sampler2D uColor;
layout(push_constant) uniform Push {
    vec2 uSun;
    vec2 uAspect;
    vec4 uTint_Pad;
    vec4 uSrc;  // (the view in the 3D's full buffers: offset xy, size zw, 0..1)
};
#define uTint (uTint_Pad.xyz)
void main() {
    vec2 suv = uSrc.xy + vUv * uSrc.zw;
    float d = texture(uDepth, suv).r;
    if (d < 1.0) { FragColor = vec4(0.0); return; }
    vec3 c = texture(uColor, suv).rgb;
    vec2 v = (vUv - uSun) * uAspect;
    float k = exp(-dot(v, v) * 6.0);
    float lum = dot(c, vec3(0.299, 0.587, 0.114));
    FragColor = vec4(mix(c, uTint * lum, 0.5) * k, 1.0);
}
"""

MRP_RAYS_BLUR_FS = """#version 450
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 0) uniform sampler2D uMask;
layout(push_constant) uniform Push {
    vec2 uSun;
    vec2 uTaps_pad;  // (x: the taps, the step's; y: the view's width / height)
    vec4 uParam;
};
void main() {
    int N = int(uTaps_pad.x + 0.5);
    float uAspect = uTaps_pad.y;
    vec2 delta = (vUv - uSun) * (uParam.x / float(N));
    float noise = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    vec2 uv = vUv - delta * noise;
    vec3 sum = vec3(0.0);
    float decay = 1.0;
    for (int i = 0; i < N; ++i) {
        uv -= delta;
        sum += texture(uMask, uv).rgb * decay * uParam.z;
        decay *= uParam.y;
    }
    // how open the sky around the sun is (as the OpenGL kRaysBlurFs)
    float os = 0.0, ow = 1e-6;
    if (gl_FragCoord.x < 1.0 && gl_FragCoord.y < 1.0) {
        for (int j = 0; j < 6; ++j) {
            for (int i = 0; i < 6; ++i) {
                vec2 o = (vec2(float(i), float(j)) - 2.5) * 0.12;
                vec2 q = clamp(uSun + vec2(o.x / uAspect, o.y), 0.0, 1.0);  // (a sun off the view: the sky at the view's edge)
                float w = exp(-dot(o, o) * 6.0);
                os += texture(uMask, q).a * w;
                ow += w;
            }
        }
    }
    FragColor = vec4(sum * uParam.w, os / ow);
}
"""

MRP_RAYS_SPLIT_FS = """#version 450
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 0) uniform sampler2D uRays;
layout(push_constant) uniform Push {
    vec4 uA;  // xy the sun's place
    vec4 uB;  // x the view's width / height; the open sky's glare: y near the sun, z far from it, w its fall-off
};
void main() {
    // the rays here and across them (to the sides, square to the way toward the sun): what they share is the broad light of the
    // sky, what this texel has above it a shaft
    vec2 asp = vec2(uB.x, 1.0);
    vec2 v = (vUv - uA.xy) * asp;
    vec2 side = normalize(vec2(-v.y, v.x) + vec2(1e-6, 0.0)) / asp;
    vec3 r = texture(uRays, vUv).rgb;
    vec3 across = 0.25 * (texture(uRays, vUv + side * 0.05).rgb + texture(uRays, vUv - side * 0.05).rgb +
                          texture(uRays, vUv + side * 0.11).rgb + texture(uRays, vUv - side * 0.11).rgb);
    vec3 broad = min(r, across), shaft = r - broad;
    // an open sky around the sun (the blur's texel (0, 0)'s alpha toward 1): its broad light is a glare, kept mostly near the sun; in
    // a covered view (toward 0: a tunnel's mouth, the gaps in a roof of leaves) all of it stays
    float glare = uB.y * mix(uB.z, 1.0, exp(-dot(v, v) * uB.w));
    float open = smoothstep(0.2, 0.6, texelFetch(uRays, ivec2(0), 0).a);
    FragColor = vec4(shaft + broad * mix(1.0, glare, open), 1.0);
}
"""

MRP_RAYS_ADD_FS = """#version 450
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 0) uniform sampler2D uRays;
layout(push_constant) uniform Push {
    float uK;
    float uCap;
};
void main() {
    FragColor = vec4(uCap * (1.0 - exp(-texture(uRays, vUv).rgb * uK / uCap)), 0.0);  // (blended as a screen)
}
"""

MRP_PROBE_FS = """#version 450
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 oCol;
layout(set = 0, binding = 0) uniform sampler2D uScene;
layout(push_constant) uniform Push {
    vec4 uParams;
};
#define uStep (uParams.xy)
void main() {
    vec3 s = vec3(0.0);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            s += texture(uScene, vUv + (vec2(x, y) - 1.5) * uStep).rgb;
    oCol = vec4(s * (1.0 / 16.0), 1.0);
}
"""

# ---------------------------------------------------------------------------
# 6. mrp_moby_renderer.cpp shaders
# ---------------------------------------------------------------------------
MRP_MOBY_PUSH = """
layout(push_constant) uniform Push {
    vec4 uOffset;
    vec4 uFrame;
    vec4 uXyOff;
    vec4 uFogCol_Min;
    int uMode;
    float uProj;
    int uTme;
    int uAte;
    int uAtst;
    int uAref;
    int uAfail;
    int uMipMode;
    int uShade;
    int uFge;
    int uPass;
    int _pad;
};
#define uFogCol (uFogCol_Min.xyz)
#define uFogMin (uFogCol_Min.w)
"""

MRP_MOBY_VS_BODY = """
layout(location = 0) in ivec4 aPosN;
layout(location = 1) in uvec4 aJoint;
layout(location = 2) in uvec4 aWeight;
layout(location = 3) in uvec4 aRgba;
layout(location = 4) in ivec2 aUv;
layout(location = 5) in ivec4 aQ2Pos;
layout(location = 6) in uvec4 aQ2Joint;
layout(location = 7) in uvec4 aQ2Weight;

struct Part {
    vec4 clip[4];
    vec4 rows[4];
    vec4 light[8];
    vec4 sub;
    ivec4 info;
};
layout(std430, set = 0, binding = 0) readonly buffer Joints { vec4 J[]; };
layout(std430, set = 0, binding = 1) readonly buffer Parts { Part parts[]; };
layout(std430, set = 0, binding = 2) readonly buffer Cmds { uvec4 cmds[]; };
layout(std430, set = 0, binding = 3) readonly buffer Normals { vec2 normals[]; };
""" + MRP_MOBY_PUSH + RAC1_VK_SHADOW_VS + """
layout(location = 0) noperspective out vec4 vColor;
layout(location = 1) noperspective out vec3 vStq;
layout(location = 2) noperspective out float vFog;
layout(location = 3) flat out uvec4 vCmd;
#ifdef SH_SPLIT
layout(location = 4) noperspective out vec4 vColorNoKey;
#endif

vec4 joint_row(uint j, int r, int first, int count) {
    return int(j) < count ? J[(first + int(j)) * 4 + r] : vec4(0.0);
}
void matrix_of(uvec4 jt, uvec4 wt, int first, int count, out vec4 m[4]) {
    if (jt.w == 1u) {
        for (int r = 0; r < 4; ++r) m[r] = joint_row(jt.x, r, first, count);
        return;
    }
    if (jt.w == 0u) {
        for (int r = 0; r < 4; ++r) m[r] = vec4(0.0);
        return;
    }
    float w0 = float(wt.x) * (1.0 / 4096.0) * 16.0, w1 = float(wt.y) * (1.0 / 4096.0) * 16.0, w2 = float(wt.z) * (1.0 / 4096.0) * 16.0;
    for (int r = 0; r < 4; ++r) {
        precise vec4 a = joint_row(jt.x, r, first, count) * w0;
        a = a + joint_row(jt.y, r, first, count) * w1;
        if (jt.w == 3u) a = a + joint_row(jt.z, r, first, count) * w2;
        m[r] = a;
    }
}
ivec3 skin_pos(vec4 m[4], ivec3 p) {
    ivec3 q;
    for (int l = 0; l < 3; ++l) {
        precise float a = m[0][l] * float(p.x);
        a = a + m[1][l] * float(p.y);
        a = a + m[2][l] * float(p.z);
        a = a + m[3][l];
        int i = int(clamp(a, -2147483648.0, 2147483520.0));
        q[l] = (i << 16) >> 16;
    }
    return q;
}
int ftoi4(float f) {
    return int(clamp(f * 16.0, -2147483648.0, 2147483520.0));
}

void main() {
    uvec4 cmd = cmds[gl_InstanceIndex];
    Part P = parts[cmd.x];
    int first = P.info.x, count = P.info.y;
    vec4 m[4];
    matrix_of(aJoint, aWeight, first, count, m);
    ivec3 q = skin_pos(m, aPosN.xyz);

    uint nb = uint(aPosN.w) & 0xffffu;
    vec2 ta = normals[nb & 0xffu], te = normals[(nb >> 8) & 0xffu];
    precise float nx = ta.x * te.x;
    precise float ny = ta.y * te.x;
    float nw = te.y;
    precise vec4 n = m[0] * nx;
    n = n + m[1] * ny;
    n = n + m[2] * nw;
    precise float len2 = n.x * n.x + n.y * n.y;
    len2 = len2 + n.z * n.z;
    float qn = len2 > 0.0 ? 1.0 / sqrt(len2) : 3.4028235e38;
    precise vec4 d = P.light[0] * n.x;
    d = d + P.light[1] * n.y;
    d = d + P.light[2] * n.z;
    precise vec4 dk = d * (-abs(P.light[6]));
    d = max(d, dk);
    precise vec4 a = P.light[3] * d.x;
    a = a + P.light[4] * d.y;
    a = a + P.light[5] * d.z;
    ivec4 col;
    for (int l = 0; l < 4; ++l) {
        precise float c = (a[l] == 0.0 ? 0.0 : a[l] * qn) + P.light[7][l] * 1.0;
        int h = int(floatBitsToUint(c) << 16) >> 16;
        int pr = clamp(h * int(aRgba[l]), -32768, 32767);
        col[l] = (pr >> 7) & 0xff;
    }
    if ((P.info.z & 1) != 0) col = ivec4(P.sub);
#ifdef SH_SPLIT
    vColorNoKey = vec4(col) / 255.0;
    if (uShOn != 0 && (P.info.z & 1) == 0) {
        vec4 dn = P.light[0] * n.x + P.light[1] * n.y + P.light[2] * n.z;
        dn = max(dn, dn * (-abs(P.light[6])));
        vec4 an = P.light[4] * dn.y + P.light[5] * dn.z;
        ivec4 cn;
        for (int l = 0; l < 4; ++l) {
            precise float c = (an[l] == 0.0 ? 0.0 : an[l] * qn) + P.light[7][l] * 1.0;
            int h = int(floatBitsToUint(c) << 16) >> 16;
            int pr = clamp(h * int(aRgba[l]), -32768, 32767);
            cn[l] = (pr >> 7) & 0xff;
        }
        vColorNoKey = vec4(cn) / 255.0;
    }
#endif

    precise vec4 p = P.rows[3];
    p = p + P.rows[0] * float(q.x);
    p = p + P.rows[1] * float(q.y);
    p = p + P.rows[2] * float(q.z);
    float Q = uProj / p.w;
    float qq = Q;
    if ((aWeight.w & 1u) != 0u) {
        vec4 m2[4];
        matrix_of(aQ2Joint, aQ2Weight, first, count, m2);
        ivec3 q2 = skin_pos(m2, aQ2Pos.xyz);
        precise float w2 = P.rows[3].w;
        w2 = w2 + P.rows[0].w * float(q2.x);
        w2 = w2 + P.rows[1].w * float(q2.y);
        w2 = w2 + P.rows[2].w * float(q2.z);
        qq = Q * (uProj / w2);
    }
    vec3 stq = vec3(float(aUv.x) * (1.0 / 4096.0) * qq, float(aUv.y) * (1.0 / 4096.0) * qq, 1.0 * qq);
    precise float X = p.x * Q + uOffset.x;
    precise float Y = p.y * Q + uOffset.y;
    precise float Z = p.z * Q + uOffset.z;
    int xi = ftoi4(X), yi = ftoi4(Y), zi = ftoi4(Z);

    if (uMode == 0) {
        gl_Position = sh_clip(mat4(P.clip[0], P.clip[1], P.clip[2], P.clip[3]) * vec4(vec3(q), 1.0));
    } else {
        vShWorld = vec3(0.0);
        gl_ClipDistance[0] = 1.0;
        float wq = p.w / uProj;
        float X1 = uFrame.x * 0.5;
        float sx = uFrame.z / X1, sy = 1.0 / (uFrame.y * 0.5);
        vec2 c = vec2(uXyOff.x / 16.0 + X1, uXyOff.y / 16.0 + uFrame.y * 0.5);
        vec2 xy = wq > 0.0 ? vec2(float(xi), float(yi)) / 16.0 * wq : vec2(p.x + uOffset.x * wq, p.y + uOffset.y * wq);
        float zc = (2.0 * (uXyOff.z + uXyOff.w * uOffset.z) - 1.0) * wq + 2.0 * uXyOff.w * p.z;
        gl_Position = vec4((xy.x - c.x * wq) * sx, (uFrame.w != 0.0 ? 1.0 : -1.0) * (xy.y - c.y * wq) * sy, zc, wq);
    }
    vFog = floor(min(max(((P.info.w & 1) != 0 ? p.w : P.rows[3].w) + uOffset.w, uFogMin), 255.0));
    vColor = vec4(col) / 255.0;
    vStq = stq;
    vCmd = cmd;
}
"""

MRP_MOBY_FS_BODY = """
layout(set = 0, binding = 4) uniform sampler2DArray uTex;
""" + MRP_MOBY_PUSH + RAC1_VK_SHADOW_FS + """
layout(location = 0) noperspective in vec4 vColor;
layout(location = 1) noperspective in vec3 vStq;
layout(location = 2) noperspective in float vFog;
layout(location = 3) flat in uvec4 vCmd;
#ifdef SH_SPLIT
layout(location = 4) noperspective in vec4 vColorNoKey;
#endif
layout(location = 0) out vec4 FragColor;
void main() {
    sh_prepare();
    vec4 c = vColor;
#ifdef SH_SPLIT
    vec3 c_game = c.rgb;
    c.rgb = sh_apply_split(c.rgb, vColorNoKey.rgb);
    c.rgb = sh_points_moby(c.rgb);  // (ours per pixel, the game's left to the record: #110)
#else
    c.rgb = sh_apply(c.rgb);
#endif
    vec4 o = c;
    uint w = vCmd.y;
    if (uTme != 0) {
        vec2 st = vStq.xy / vStq.z;
        vec2 st_dx = dFdx(st), st_dy = dFdy(st);  // (in uniform control flow: the texture level below)
#ifdef SH_SPLIT
        st = sh_mat_uv(st, (w >> 27) & 31u);
#endif
        vec4 t;
        if ((w & 1u) != 0u) {
            t = vec4(128.0 / 255.0);
        } else {
            int mips = int((w >> 22) & 7u) + 1;
            float layer = float(vCmd.z);
            if (uMipMode == 0) {
                int mxl = int((w >> 4) & 7u), lcm = int((w >> 7) & 1u), L = int((w >> 8) & 3u);
                float K = float((int(w >> 10) << 20) >> 20) / 16.0;
                float lod = lcm != 0 ? K : log2(1.0 / max(abs(vStq.z), 1e-20)) * float(1 << L) + K;
                int level = lod <= 0.0 ? 0 : min(int(floor(lod + 0.5)), mxl);
                level = min(level, mips - 1);
                t = textureLod(uTex, vec3(st, layer), float(level));
                if (level == 0 && (w & (1u << 25)) != 0u) { FragColor = vec4(1.0, 0.0, 1.0, 1.0); return; }
                if (level == 1 && (w & (1u << 26)) != 0u) { FragColor = vec4(0.0, 1.0, 1.0, 1.0); return; }
            } else {
                // (the isotropic level, as the OpenGL path's textureQueryLod gives it on radeonsi; Vulkan drivers may fold the sampler's
                // anisotropy into it - RADV does -, which picked sharper levels than OpenGL: Aridia's and Hoven's Gadgetron signs)
                vec2 tsz = vec2(textureSize(uTex, 0).xy);
                vec2 ddx = st_dx * tsz, ddy = st_dy * tsz;
                float lod = clamp(0.5 * log2(max(dot(ddx, ddx), dot(ddy, ddy))), 0.0, float(mips - 1));
                t = textureLod(uTex, vec3(st, layer), lod);
            }
        }
        uint tfx = (w >> 1) & 3u, tcc = (w >> 3) & 1u;
        if (tfx == 0u) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0); o.a = tcc != 0u ? t.a * c.a * (255.0 / 128.0) : c.a; }
        else if (tfx == 1u) { o.rgb = t.rgb; o.a = tcc != 0u ? t.a : c.a; }
        else if (tfx == 2u) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc != 0u ? t.a + c.a : c.a; }
        else { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc != 0u ? t.a : c.a; }
#ifdef SH_SPLIT
        o.rgb = sh_emissive(o.rgb, tfx == 1u ? t.rgb : t.rgb * c_game * (255.0 / 128.0), t.rgb, (w >> 27) & 31u);
        o.rgb = sh_material(o.rgb, tfx == 1u ? t.rgb : t.rgb * c_game * (255.0 / 128.0), t.rgb, tfx == 1u ? vec3(1.0) : c.rgb * (255.0 / 128.0), (w >> 27) & 31u);
#endif
        if (uShade == 2) o.rgb = t.rgb;
    }
    if (uShade == 1) o.rgb = c.rgb;
    else if (uShade == 3) o.rgb = vec3(0.8);
    o = clamp(o, 0.0, 1.0);
    if (uFge != 0) {
        float f = vFog / 255.0;
        o.rgb = o.rgb * f + uFogCol * (1.0 - f);
    }
    float a = o.a * 255.0;
    if (uAte != 0) {
        bool pass = true;
        if (uAtst == 0) pass = false;
        else if (uAtst == 2) pass = a < float(uAref);
        else if (uAtst == 3) pass = a <= float(uAref);
        else if (uAtst == 4) pass = abs(a - float(uAref)) < 0.5;
        else if (uAtst == 5) pass = a >= float(uAref);
        else if (uAtst == 6) pass = a > float(uAref);
        else if (uAtst == 7) pass = abs(a - float(uAref)) >= 0.5;
        if (!pass && uShPass != 0) discard;
        if (!pass && uPass == 0 && (uAfail == 0 || uAfail == 2)) discard;
        if (!pass && uPass == 1 && uAfail != 2) discard;
    }
    FragColor = vec4(o.rgb, min(o.a * (255.0 / 128.0), 1.0));
}
"""

MRP_MOBY_VS = "#version 450\n" + MRP_MOBY_VS_BODY
MRP_MOBY_FS = "#version 450\n" + MRP_MOBY_FS_BODY
MRP_MOBY_SPLIT_VS = "#version 450\n#define SH_SPLIT 1\n" + MRP_MOBY_VS_BODY
MRP_MOBY_SPLIT_FS = "#version 450\n#define SH_SPLIT 1\n" + MRP_MOBY_FS_BODY

# ---------------------------------------------------------------------------
# 7. mrp_world_renderer.cpp shaders
# ---------------------------------------------------------------------------
MRP_WORLD_PUSH = """
layout(push_constant) uniform Push {
    mat4 uVP;
    vec4 uFrame;
    vec4 uXyOff;
    vec4 uFogCol_Pad;
    int uMode;
    int uMipMode;
    int uShade;
    int uFge;
    int uAtPass;
    int uU8;
    int _pad0;
    int _pad1;
};
#define uFogCol (uFogCol_Pad.xyz)
"""

MRP_WORLD_VS_BODY = """
layout(location = 0) in vec4 aWorld;
layout(location = 1) in vec4 aVu;
layout(location = 2) in vec3 aStq;
layout(location = 3) in float aLodQ;
layout(location = 4) in uvec4 aRgba;
layout(location = 5) in float aFog;
layout(location = 6) in uvec2 aTex;
layout(location = 7) in uvec2 aState;
""" + MRP_WORLD_PUSH + RAC1_VK_SHADOW_VS + """
layout(location = 0) smooth out vec3 vStq;
layout(location = 1) noperspective out float vLodQ;
layout(location = 2) noperspective out vec4 vColor;
layout(location = 3) flat out vec4 vFlat;
layout(location = 4) noperspective out float vFog;
layout(location = 5) flat out uvec2 vTex;
layout(location = 6) flat out uvec2 vState;
void main() {
    vTex = aTex;
    vState = aState;
    if (uMode == 0) {
        gl_Position = sh_clip(uVP * aWorld);
    } else {
        vShWorld = vec3(0.0);
        gl_ClipDistance[0] = 1.0;
        float w = aVu.w;
        float X1 = uFrame.x * 0.5;
        float sx = uFrame.z / X1, sy = 1.0 / (uFrame.y * 0.5);
        vec2 c = vec2(uXyOff.x / 16.0 + X1, uXyOff.y / 16.0 + uFrame.y * 0.5);
        float zc = 2.0 * (uXyOff.z * w + uXyOff.w * aVu.z) - w;
        gl_Position = vec4((aVu.x - c.x * w) * sx, -(aVu.y - c.y * w) * sy, zc, w);
    }
    vStq = aStq;
    vLodQ = aLodQ;
    vColor = vec4(aRgba) / 255.0;
    vFlat = vColor;
    vFog = aFog;
}
"""

MRP_WORLD_GPU_VS_BODY = """
layout(location = 0) in ivec4 aPos;
layout(location = 1) in ivec4 aDelta;
layout(location = 2) in ivec4 aUv;
layout(location = 3) in uvec4 aCol;

struct Inst {
    mat4 clip;
    vec4 rows[4];
    vec4 morph;
    uvec4 info;
};
layout(std430, set = 0, binding = 0) readonly buffer Insts { Inst insts[]; };
layout(std430, set = 0, binding = 1) readonly buffer Cols { uvec4 cols[]; };
layout(std430, set = 0, binding = 2) readonly buffer Cmds { uvec4 cmds[]; };
layout(std430, set = 0, binding = 3) readonly buffer RCols { uint rcols[]; };
""" + MRP_WORLD_PUSH + RAC1_VK_SHADOW_VS + """
layout(location = 0) smooth out vec3 vStq;
layout(location = 1) noperspective out float vLodQ;
layout(location = 2) noperspective out vec4 vColor;
layout(location = 3) flat out vec4 vFlat;
layout(location = 4) noperspective out float vFog;
layout(location = 5) flat out uvec2 vTex;
layout(location = 6) flat out uvec2 vState;

int ftoi4(float f) { return int(clamp(f * 16.0, -2147483648.0, 2147483520.0)); }

void main() {
    uvec4 cmd = cmds[2 * gl_InstanceIndex];
    vState = uvec2(cmd.w, cmds[2 * gl_InstanceIndex + 1].x);
    Inst I = insts[cmd.x];
    vTex = cmd.yz;
    vec3 P = vec3(aPos.xyz);
    if ((aPos.w & 1) != 0) {
        precise vec3 d = vec3(aDelta.xyz) * I.morph.x;
        P = d + vec3(aPos.xyz);
    }
    precise vec4 p = I.rows[3];
    p = p + I.rows[0] * P.x;
    p = p + I.rows[1] * P.y;
    p = p + I.rows[2] * P.z;
    float Q = 1.0 / p.w;
    vec3 uv = vec3(aUv.xyz) * (1.0 / 4096.0);
    uvec4 c;
    uint first = I.info.x;
    uint kind = I.info.w & 1u;
    bool resident = (I.info.w & 2u) != 0u;
    if (kind == 0u) {
        uvec4 c0 = resident ? (uvec4(rcols[first + aCol.x]) >> uvec4(0u, 8u, 16u, 24u) & 0xffu) | 0x4b000000u : cols[first + aCol.x];
        if ((aPos.w & 2) != 0) {
            uvec4 c1 = resident ? (uvec4(rcols[first + aCol.y]) >> uvec4(0u, 8u, 16u, 24u) & 0xffu) | 0x4b000000u : cols[first + aCol.y];
            uvec4 c2 = resident ? (uvec4(rcols[first + aCol.z]) >> uvec4(0u, 8u, 16u, 24u) & 0xffu) | 0x4b000000u : cols[first + aCol.z];
            for (int l = 0; l < 4; ++l) {
                precise float a = uintBitsToFloat(c1[l]) * 0.5;
                a = a + uintBitsToFloat(c2[l]) * 0.5;
                precise float b = uintBitsToFloat(c0[l]) * I.morph.z;
                precise float r = a * I.morph.w;
                r = r + b;
                c[l] = floatBitsToUint(r) & 0xffu;
            }
        } else {
            c = c0 & 0xffu;
        }
    } else if (resident) {
        c = uvec4(uvec3(rcols[first + aCol.x]) >> uvec3(0u, 8u, 16u) & 0xffu, I.info.z);
    } else {
        c = uvec4(cols[first + aCol.x].xyz & 0xffu, I.info.z);
    }
    vColor = vec4(c) / 255.0;
    vFlat = vColor;
    vFog = float(I.info.y);
    vStq = uv;
    vLodQ = uv.z * Q;
    if (uMode == 0) {
        gl_Position = sh_clip(I.clip * vec4(P, 1.0));
    } else {
        vShWorld = vec3(0.0);
        gl_ClipDistance[0] = 1.0;
        float w = p.w;
        float X = float(ftoi4(p.x * Q) & 0xffff) / 16.0, Y = float(ftoi4(p.y * Q) & 0xffff) / 16.0;
        float Z = float((ftoi4(p.z * Q) >> 4) & 0xffffff);
        float X1 = uFrame.x * 0.5;
        float sx = uFrame.z / X1, sy = 1.0 / (uFrame.y * 0.5);
        vec2 cc = vec2(uXyOff.x / 16.0 + X1, uXyOff.y / 16.0 + uFrame.y * 0.5);
        float zc = (2.0 * (uXyOff.z + uXyOff.w * Z) - 1.0) * w;
        gl_Position = vec4((X - cc.x) * w * sx, -(Y - cc.y) * w * sy, zc, w);
    }
}
"""

MRP_WORLD_FS_HEADER = """
#extension GL_KHR_shader_subgroup_ballot : require
layout(set = 0, binding = 4) uniform sampler2DArray uTexs[16];
""" + MRP_WORLD_PUSH + RAC1_VK_SHADOW_FS

MRP_WORLD_FS_BODY = """
layout(location = 0) smooth in vec3 vStq;
layout(location = 1) noperspective in float vLodQ;
layout(location = 2) noperspective in vec4 vColor;
layout(location = 3) flat in vec4 vFlat;
layout(location = 4) noperspective in float vFog;
layout(location = 5) flat in uvec2 vTex;
layout(location = 6) flat in uvec2 vState;
layout(location = 0) out vec4 FragColor;
void main() {
    sh_prepare();
    uint fl = vState.x;
    int uTme = int(fl & 1u), uIip = int((fl >> 1) & 1u), uAte = int((fl >> 3) & 1u), uAtst = int((fl >> 4) & 7u), uAref = int((fl >> 8) & 0xffu);
    uint unit = (fl >> 16) & 15u;
#ifdef SH_POINTS
    if (uShPass != 0 && uAte == 0) return;
#endif
    vec4 c = uIip != 0 ? vColor : vFlat;
#ifdef SH_POINTS
    vec3 c_game = c.rgb;
#endif
    c.rgb = sh_apply(c.rgb);
#ifdef SH_POINTS
    c.rgb = sh_points(c.rgb);
#endif
    vec4 o = c;
    uint w = vTex.x;
    float uLayer = float(vTex.y);
    if (uTme != 0) {
        vec2 st = vStq.xy / vStq.z;
        vec2 st_dx = dFdx(st), st_dy = dFdy(st);  // (in uniform control flow: the texture level below)
#ifdef SH_POINTS
        st = sh_mat_uv(st, (w >> 25) & 31u);
#endif
        vec4 t;
        if ((w & 1u) != 0u) {
            t = vec4(128.0 / 255.0);
        } else {
            int mips = int((w >> 22) & 7u) + 1;
            for (;;) {
                uint u = subgroupBroadcastFirst(unit);
                if (u != unit) continue;
                if (uMipMode == 0) {
                    int mxl = int((w >> 4) & 7u), lcm = int((w >> 7) & 1u), L = int((w >> 8) & 3u);
                    float K = float((int(w >> 10) << 20) >> 20) / 16.0;
                    float lod = lcm != 0 ? K : log2(1.0 / max(abs(vLodQ), 1e-20)) * float(1 << L) + K;
                    int level = lod <= 0.0 ? 0 : min(int(floor(lod + 0.5)), mxl);
                    level = min(level, mips - 1);
                    t = textureLod(uTexs[u], vec3(st, uLayer), float(level));
                } else {
                    vec2 tsz = vec2(textureSize(uTexs[u], 0).xy);  // (the isotropic level: see the mobys' shader)
                    vec2 ddx = st_dx * tsz, ddy = st_dy * tsz;
                    float lod = clamp(0.5 * log2(max(dot(ddx, ddx), dot(ddy, ddy))), 0.0, float(mips - 1));
                    t = textureLod(uTexs[u], vec3(st, uLayer), lod);
                }
                break;
            }
        }
        uint tfx = (w >> 1) & 3u, tcc = (w >> 3) & 1u;
        if (tfx == 0u) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0); o.a = tcc != 0u ? t.a * c.a * (255.0 / 128.0) : c.a; }
        else if (tfx == 1u) { o.rgb = t.rgb; o.a = tcc != 0u ? t.a : c.a; }
        else if (tfx == 2u) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc != 0u ? t.a + c.a : c.a; }
        else { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc != 0u ? t.a : c.a; }
#ifdef SH_POINTS
        o.rgb = sh_emissive(o.rgb, tfx == 1u ? t.rgb : t.rgb * c_game * (255.0 / 128.0), t.rgb, (w >> 25) & 31u);
        o.rgb = sh_material(o.rgb, tfx == 1u ? t.rgb : t.rgb * c_game * (255.0 / 128.0), t.rgb, tfx == 1u ? vec3(1.0) : c.rgb * (255.0 / 128.0), (w >> 25) & 31u);
#endif
        if (uShade == 2) o.rgb = t.rgb;
    }
    if (uShade == 1) o.rgb = c.rgb;
    else if (uShade == 3) o.rgb = vec3(0.8);
    o = clamp(o, 0.0, 1.0);
    if (uFge != 0) {
        float f = vFog / 255.0;
        o.rgb = o.rgb * f + uFogCol * (1.0 - f);
    }
    float a = o.a * 255.0;
    if (uAte != 0) {
        bool pass = true;
        if (uAtst == 0) pass = false;
        else if (uAtst == 2) pass = a < float(uAref);
        else if (uAtst == 3) pass = a <= float(uAref);
        else if (uAtst == 4) pass = abs(a - float(uAref)) < 0.5;
        else if (uAtst == 5) pass = a >= float(uAref);
        else if (uAtst == 6) pass = a > float(uAref);
        else if (uAtst == 7) pass = abs(a - float(uAref)) >= 0.5;
        if (uShPass != 0 && !pass) discard;
        if (uAtPass == 1 && !pass) discard;
        if (uAtPass == 2 && pass) discard;
    }
    FragColor = vec4(o.rgb, min(o.a * (255.0 / 128.0), 1.0));
}
"""

# ---------------------------------------------------------------------------
# The ambient particles (src/game/render/mrp_particles.cpp: the same bodies as its OpenGL shaders - keep them in step)
MRP_PART_UBO = """layout(std140, set = 0, binding = 0) uniform PartUbo {
    mat4 uVP; mat4 uInvVP; vec4 uCam; vec4 uBox; vec4 uVel; vec4 uCol; vec4 uSize; vec4 uCamVel; vec4 uSun;
    vec4 uVp; vec4 uProj; vec4 uLimLo; vec4 uLimHi; vec4 uMisc; vec4 uMisc2; vec4 uIntLo[4]; vec4 uIntHi[4];
    vec4 uSrc; vec4 uMove; vec4 uSky; vec4 uEmit[512];
};
layout(set = 0, binding = 1) uniform sampler2D uDepth;
layout(set = 0, binding = 2) uniform sampler2D uSkyMap;
// (the sky: 1 under the open sky, 0 under the topmost face of the level's collision at the place - a roof -, blended over the
// grid's squares)
float sky_open(vec3 p) {
    if (uSky.w < 0.5) return 1.0;
    vec2 g = (p.xy - uSky.xy) / uSky.z - 0.5;
    ivec2 i0 = ivec2(floor(g));
    vec2 f = g - vec2(i0);
    ivec2 n = textureSize(uSkyMap, 0) - ivec2(0, 1);  // (the last row: the window's place, for the materials)
    vec4 o;
    for (int k = 0; k < 4; ++k) {
        ivec2 c = i0 + ivec2(k & 1, k >> 1);
        float h = (any(lessThan(c, ivec2(0))) || any(greaterThanEqual(c, n))) ? -1.0e6 : texelFetch(uSkyMap, c, 0).r;
        o[k] = smoothstep(h - 0.25, h + 0.25, p.z);
    }
    return mix(mix(o.x, o.y, f.x), mix(o.z, o.w, f.x), f.y);
}
"""

MRP_PART_VS = "#version 450\n" + MRP_PART_UBO + """
#define VID gl_VertexIndex
layout(location = 0) out vec2 vQ;
layout(location = 1) out float vA;
layout(location = 2) out float vZ;
uint pcg(uint v) {
    uint s = v * 747796405u + 2891336453u;
    uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    return (w >> 22u) ^ w;
}
float rnd(inout uint s) {
    s = pcg(s);
    return float(s) * (1.0 / 4294967296.0);
}
void main() {
    int id = VID;
    int pi = id / 6, c = id - pi * 6;
    vec2 q = vec2((c == 1 || c == 2 || c == 4) ? 1.0 : -1.0, (c == 2 || c == 4 || c == 5) ? 1.0 : -1.0);
    vQ = q;
    vA = 0.0;
    vZ = 1.0;
    gl_Position = vec4(2.0, 2.0, 2.0, 1.0);  // (culled)
    int kind = int(uVel.w + 0.5);
    uint s = uint(pi) * 9781u + uint(uMisc.w);
    vec3 r0 = vec3(rnd(s), rnd(s), rnd(s));
    float r3 = rnd(s), r4 = rnd(s), r5 = rnd(s);
    float t = uCam.w;
    vec3 vel = uVel.xyz * (0.75 + 0.5 * r3);
    float ph = r4 * 6.2832;
    float tw = t * uMove.x;  // (the drift's pace: slower for a calm kind)
    vec3 drift = uMisc.x * vec3(sin(tw * (0.31 + 0.4 * r5) + ph), cos(tw * (0.27 + 0.35 * r3) + ph * 1.7), 0.5 * sin(tw * (0.23 + 0.3 * r4) + ph * 2.3));
    vec3 rel, wp;
    float a;
    bool src = uSrc.x > 0.5;
    if (src) {
        // from the lava: each life starts at one of the lava's points near the camera (uEmit: xyz the point, w the time its slot got
        // it, < 0 none) and rises slowly, carried by the air's slow currents, fading as it goes
        float rate = 0.07 + 0.06 * r5;  // (a life of 8 to 14 s)
        float lt = t * rate + r4;
        float life = fract(lt);
        uint s2 = pcg(uint(pi) * 2654435761u + uint(int(floor(lt))) * 40503u + uint(uMisc.w));
        vec4 em = uEmit[min(int(rnd(s2) * 512.0), 511)];
        if (em.w > -1.5 && em.w < 0.0) return;  // (an empty slot)
        float ang = rnd(s2) * 6.2832, rad = sqrt(rnd(s2)) * uSrc.w;
        float age = life / rate;  // (seconds since it left the lava)
        vec2 cur = uMove.y * vec2(sin(em.y * 0.07 + t * 0.031 + 1.3) + 0.6 * sin(em.x * 0.05 - t * 0.021),
                                  cos(em.x * 0.06 - t * 0.026) + 0.6 * cos(em.y * 0.045 + t * 0.017));
        vec3 wander = uMisc.x * min(age * 0.25, 1.0) * vec3(sin(age * (0.35 + 0.3 * r5) + ph), cos(age * (0.3 + 0.3 * r3) + ph * 1.7), 0.0);
        wp = em.xyz + vec3(cos(ang) * rad, sin(ang) * rad, 0.05) + vec3((cur + uVel.xy) * age, uVel.z * (0.7 + 0.6 * r3) * age) + wander;
        rel = wp - uCam.xyz;
        float since = t - (em.w >= 0.0 ? em.w : -2.0 - em.w);  // (the slot's point taken - or let go - this long ago)
        float slot_a = since < 0.0 ? 1.0 : (em.w >= 0.0 ? clamp(since / 1.5, 0.0, 1.0) : clamp(1.0 - since / 1.5, 0.0, 1.0));
        a = uCol.a * smoothstep(0.0, 0.06, life) * (1.0 - smoothstep(0.5, 1.0, life)) * smoothstep(uSrc.z, uSrc.z * 0.75, length(rel)) *
            slot_a;  // (born bright, fading as it rises; faded far off, and as its slot takes or lets go of its point)
    } else {
        // the particle's place in the world, wrapped into the volume around the camera
        rel = r0 * uBox.xyz + vel * t + drift - uCam.xyz;
        rel = (fract(rel / uBox.xyz) - 0.5) * uBox.xyz;
        wp = uCam.xyz + rel;
        vec3 e = abs(rel) / (0.5 * uBox.xyz);
        a = uCol.a * smoothstep(1.0, 0.75, max(max(e.x, e.y), e.z));  // (faded toward the volume's faces: no popping where it wraps)
    }
    if (uLimLo.w > 0.5) {  // the line's box
        vec3 d = min(wp - uLimLo.xyz, uLimHi.xyz - wp);
        a *= clamp(min(min(d.x, d.y), d.z), 0.0, 1.0);
    }
    if (uMisc2.y > 0.5) {  // (from the sky: none under a roof, none inside the level's interiors)
        a *= sky_open(wp);
        for (int i = 0; i < int(uMisc2.x); ++i)
            if (all(greaterThan(wp, uIntLo[i].xyz)) && all(lessThan(wp, uIntHi[i].xyz))) a = 0.0;
    }
    if (kind == 2) {  // embers: a life (rising, fading), flickering
        float life = fract(t * (0.18 + 0.2 * r5) + r4);
        a *= (src ? 1.0 : sin(life * 3.14159)) * (0.6 + 0.4 * sin(t * (9.0 + 6.0 * r3) + ph));  // (from the lava: its own life above)
    } else if (kind == 3) {  // fireflies: blinking
        a *= smoothstep(0.35, 1.0, 0.5 + 0.5 * sin(t * (1.1 + 1.4 * r5) + ph * 3.0));
    } else if (kind == 4) {  // motes: bright looking toward the sun (forward scatter), twinkling
        vec3 vd = normalize(rel);
        float sc = pow(max(dot(vd, uSun.xyz), 0.0), 3.0);
        a *= mix(1.0, 0.6 + 2.4 * sc, uSun.w) * (0.7 + 0.3 * sin(t * (2.0 + 3.0 * r5) + ph));
    }
    if (a <= 0.003) return;
    vec4 c0 = uVP * vec4(wp, 1.0);
    if (c0.w < 0.3) return;
    a *= smoothstep(0.4, kind == 5 ? 1.0 : 2.0, c0.w);  // (none right at the lens: a speck there would be a blot)
    float size = uSize.x * (1.0 + uSize.y * (r5 * 2.0 - 1.0));
    float half_px = size * uProj.z * uVp.z * 0.5 / c0.w;  // (half the size in pixels)
    float k = max(uMisc2.z * 0.5 / max(half_px, 1e-4), 1.0);  // (at least the smallest size: its alpha lowered instead)
    if (kind == 5) {  // rain: a streak along its motion as the camera sees it (the camera's own taken off)
        vec3 v = vel - uCamVel.xyz;
        vec4 c1 = uVP * vec4(wp - v * uSize.z, 1.0);
        if (c1.w < 0.3) return;
        vec2 px = uVp.zw * 0.5;
        vec2 dir = c1.xy / c1.w * px - c0.xy / c0.w * px;
        float len = length(dir);
        dir = len > 1e-3 ? dir / len : vec2(0.0, 1.0);
        vec2 nrm = vec2(-dir.y, dir.x);
        float along = q.y * 0.5 + 0.5;
        vec4 cc = mix(c0, c1, along);
        gl_Position = vec4(cc.xy + nrm * (q.x * half_px * k) / px * cc.w, cc.zw);
        vZ = cc.w;
        a /= k;
        a *= clamp(len / 3.0, 0.25, 1.0) * 1.0;  // (a short streak - seen along its fall - lighter)
    } else {
        gl_Position = vec4(c0.xy + q * vec2(uProj.z, uProj.w) * size * k, c0.zw);
        vZ = c0.w;
        a /= k;  // (far: drawn at the smallest size, fainter - between the area's k * k and nothing, so that they still read)
    }
    vA = a;
}
"""

MRP_PART_FS = "#version 450\n" + MRP_PART_UBO + """
layout(location = 0) in vec2 vQ;
layout(location = 1) in float vA;
layout(location = 2) in float vZ;
layout(location = 0) out vec4 FragColor;
void main() {
    int kind = int(uVel.w + 0.5);
    float r2 = dot(vQ, vQ);
    float shape;
    if (kind == 5) {
        shape = (1.0 - abs(vQ.x)) * smoothstep(-1.0, -0.4, vQ.y) * (0.45 + 0.55 * (vQ.y * 0.5 + 0.5));  // (a streak: its head brighter)
    } else {
        if (r2 >= 1.0) discard;
        shape = (kind == 2 || kind == 3 || kind == 4) ? exp(-r2 * 3.5) : smoothstep(1.0, 0.25, r2);  // (glows; soft discs)
    }
    float a = vA * shape;
    // soft against the scene: faded in front of the surface behind it, hidden behind it
    float d = texelFetch(uDepth, ivec2(gl_FragCoord.xy - uMisc.yz), 0).r;
    float zs = uProj.y / ((d * 2.0 - 1.0) + uProj.x);
    a *= clamp((zs - vZ) / uSize.w, 0.0, 1.0);
    if (a <= 0.002) discard;
    FragColor = vec4(uCol.rgb, min(a, 1.0));
}
"""

MRP_PART_SPLASH_VS = """#version 450
void main() {
    vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
"""

MRP_PART_SPLASH_FS = "#version 450\n" + MRP_PART_UBO + """
layout(location = 0) out vec4 FragColor;
uint pcg(uint v) {
    uint s = v * 747796405u + 2891336453u;
    uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    return (w >> 22u) ^ w;
}
float rnd(inout uint s) {
    s = pcg(s);
    return float(s) * (1.0 / 4294967296.0);
}
void main() {
    float d = texelFetch(uDepth, ivec2(gl_FragCoord.xy - uMisc.yz), 0).r;
    vec2 ndc = (gl_FragCoord.xy - uVp.xy) / uVp.zw * 2.0 - 1.0;
    vec4 w = uInvVP * vec4(ndc, d * 2.0 - 1.0, 1.0);
    vec3 p = w.xyz / w.w;
    vec3 n = cross(dFdx(p), dFdy(p));
    float nl = length(n);
    if (d >= 0.99999 || nl <= 0.0) discard;
    if (dot(n, uCam.xyz - p) < 0.0) n = -n;  // (the side the camera sees: a roof's underside is no ground)
    float up = n.z / nl;
    float dist = length(p - uCam.xyz);
    if (up < 0.85 || dist > uSize.w) discard;
    if (uLimLo.w > 0.5 && (any(lessThan(p, uLimLo.xyz)) || any(greaterThan(p, uLimHi.xyz)))) discard;
    for (int i = 0; i < int(uMisc2.x); ++i)
        if (all(greaterThan(p, uIntLo[i].xyz - vec3(0.0, 0.0, 0.5))) && all(lessThan(p, uIntHi[i].xyz))) discard;
    float open = sky_open(p + vec3(0.0, 0.0, 0.75));  // (the ground the topmost face there: under the sky)
    if (open <= 0.0) discard;
    const float cell = 0.55;
    vec2 gi = floor(p.xy / cell);
    uint s = uint(int(gi.x) * 73856093) ^ uint(int(gi.y) * 19349663);
    float rate = 1.4 + rnd(s) * 1.2;
    float tt = uCam.w * rate + rnd(s);
    float cyc = floor(tt), ph = fract(tt);
    uint s2 = s ^ (uint(int(cyc)) * 83492791u);
    if (rnd(s2) > uMisc2.w) discard;
    vec2 ctr = (gi + 0.25 + 0.5 * vec2(rnd(s2), rnd(s2))) * cell;
    float r = length(p.xy - ctr), rad = 0.02 + ph * 0.12;
    float ring = smoothstep(0.014, 0.0, abs(r - rad)) * (1.0 - ph) + smoothstep(0.025, 0.0, r) * max(1.0 - ph * 5.0, 0.0);
    float a = ring * uCol.a * smoothstep(uSize.w, uSize.w * 0.5, dist) * up * open;
    if (a <= 0.003) discard;
    FragColor = vec4(uCol.rgb, min(a, 1.0));
}
"""

MRP_WORLD_VS = "#version 460\n" + MRP_WORLD_VS_BODY
MRP_WORLD_GPU_VS = "#version 460\n" + MRP_WORLD_GPU_VS_BODY
MRP_WORLD_FS = "#version 460\n" + MRP_WORLD_FS_HEADER + MRP_WORLD_FS_BODY
MRP_WORLD_LIT_VS = "#version 460\n#define SH_POINTS 1\n" + MRP_WORLD_VS_BODY
MRP_WORLD_GPU_LIT_VS = "#version 460\n#define SH_POINTS 1\n" + MRP_WORLD_GPU_VS_BODY
MRP_WORLD_LIT_FS = "#version 460\n#define SH_POINTS 1\n" + MRP_WORLD_FS_HEADER + MRP_WORLD_FS_BODY
MRP_WORLD_EARLY_FS = "#version 460\n#define SH_POINTS 1\n" + MRP_WORLD_FS_HEADER + "\nlayout(early_fragment_tests) in;\n" + MRP_WORLD_FS_BODY

SHADERS = [
    ("kGsDrawVs", "vert", GS_DRAW_VS),
    ("kGsDrawFs", "frag", GS_DRAW_FS),
    ("kMrpFxVs", "vert", MRP_FX_VS),
    ("kMrpFxFs", "frag", MRP_FX_FS),
    ("kMrpVu7Vs", "vert", MRP_VU7_VS),
    ("kMrpVu7Fs", "frag", MRP_VU7_FS),
    ("kMrpShadowsFaceVs", "vert", MRP_SHADOWS_FACE_VS),
    ("kMrpShadowsFaceFs", "frag", MRP_SHADOWS_FACE_FS),
    ("kMrpShadowsDarkVs", "vert", MRP_SHADOWS_DARK_VS),
    ("kMrpShadowsDarkFs", "frag", MRP_SHADOWS_DARK_FS),
    ("kMrpTfragCs", "comp", MRP_TFRAG_CS),
    ("kMrpAoVs", "vert", MRP_AO_VS),
    ("kMrpAoFs", "frag", MRP_AO_FS),
    ("kMrpAoBlurFs", "frag", MRP_AO_BLUR_FS),
    ("kMrpAoBlurSepFs", "frag", MRP_AO_BLUR_SEP_FS),
    ("kMrpRaysMaskFs", "frag", MRP_RAYS_MASK_FS),
    ("kMrpRaysBlurFs", "frag", MRP_RAYS_BLUR_FS),
    ("kMrpRaysSplitFs", "frag", MRP_RAYS_SPLIT_FS),
    ("kMrpRaysAddFs", "frag", MRP_RAYS_ADD_FS),
    ("kMrpProbeFs", "frag", MRP_PROBE_FS),
    ("kMrpPartVs", "vert", MRP_PART_VS),
    ("kMrpPartFs", "frag", MRP_PART_FS),
    ("kMrpPartSplashVs", "vert", MRP_PART_SPLASH_VS),
    ("kMrpPartSplashFs", "frag", MRP_PART_SPLASH_FS),
    ("kMrpMobyVs", "vert", MRP_MOBY_VS),
    ("kMrpMobyFs", "frag", MRP_MOBY_FS),
    ("kMrpMobySplitVs", "vert", MRP_MOBY_SPLIT_VS),
    ("kMrpMobySplitFs", "frag", MRP_MOBY_SPLIT_FS),
    ("kMrpWorldVs", "vert", MRP_WORLD_VS),
    ("kMrpWorldGpuVs", "vert", MRP_WORLD_GPU_VS),
    ("kMrpWorldFs", "frag", MRP_WORLD_FS),
    ("kMrpWorldLitVs", "vert", MRP_WORLD_LIT_VS),
    ("kMrpWorldGpuLitVs", "vert", MRP_WORLD_GPU_LIT_VS),
    ("kMrpWorldLitFs", "frag", MRP_WORLD_LIT_FS),
    ("kMrpWorldEarlyFs", "frag", MRP_WORLD_EARLY_FS),
]


def compile_shader(compiler: str, name: str, stage: str, src: str) -> bytes:
    with tempfile.TemporaryDirectory() as td:
        in_path = os.path.join(td, f"{name}.{stage}")
        out_path = os.path.join(td, f"{name}.spv")
        with open(in_path, "w", encoding="utf-8") as f:
            f.write(src)
        if os.path.basename(compiler).lower().startswith("glslangvalidator"):
            cmd = [compiler, "-V", "--target-env", "vulkan1.3", "-S", stage, in_path, "-o", out_path]
        else:
            cmd = [compiler, "--target-env=vulkan1.3", "-O", f"-fshader-stage={stage}", in_path, "-o", out_path]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode != 0:
            print(f"ERROR compiling {name} ({stage}):\n{res.stdout}{res.stderr}", file=sys.stderr)
            for idx, line in enumerate(src.splitlines(), 1):
                print(f"{idx:4d}: {line}", file=sys.stderr)
            sys.exit(1)
        with open(out_path, "rb") as f:
            return f.read()


def write_if_changed(path: str, text: str) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    try:
        with open(path, "r", encoding="utf-8") as f:
            if f.read() == text:
                return
    except OSError:
        pass
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def main():
    # Usage: gen_vk_shaders.py <out_dir> [<glslc or glslangValidator>]
    # Writes <out_dir>/runtime/vk/vk_shaders.h (the arrays' declarations, with their sizes) and <out_dir>/vk_shaders.cpp
    # (their SPIR-V). Without a compiler the arrays are one word each and kAvailable is false: the renderer then does not
    # start (vk::init fails, the game stays on OpenGL).
    if len(sys.argv) < 2:
        print("usage: gen_vk_shaders.py <out_dir> [glslc|glslangValidator]", file=sys.stderr)
        sys.exit(2)
    out_dir = sys.argv[1]
    compiler = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] and not sys.argv[2].endswith("NOTFOUND") else ""
    header = [
        "// Generated by tools/gen_vk_shaders.py at build time - not in the repository",
        "#pragma once",
        "#include <cstdint>",
        "",
        "namespace rac1::vk::shaders {",
        "",
        f"inline constexpr bool kAvailable = {'true' if compiler else 'false'};",
        "",
    ]
    body = [
        "// Generated by tools/gen_vk_shaders.py at build time - not in the repository",
        "#include \"runtime/vk/vk_shaders.h\"",
        "",
        "namespace rac1::vk::shaders {",
        "",
    ]
    for name, stage, src in SHADERS:
        if compiler:
            spv = compile_shader(compiler, name, stage, src)
            assert len(spv) % 4 == 0
            words = struct.unpack(f"<{len(spv) // 4}I", spv)
        else:
            words = (0,)
        header.append(f"extern const uint32_t {name}[{len(words)}];  // {stage}")
        body.append(f"const uint32_t {name}[{len(words)}] = {{")
        for i in range(0, len(words), 8):
            body.append("    " + ", ".join(f"0x{w:08x}u" for w in words[i:i + 8]) + ",")
        body.append("};")
    header += ["", "}  // namespace rac1::vk::shaders", ""]
    body += ["", "}  // namespace rac1::vk::shaders", ""]
    write_if_changed(os.path.join(out_dir, "runtime", "vk", "vk_shaders.h"), "\n".join(header))
    write_if_changed(os.path.join(out_dir, "vk_shaders.cpp"), "\n".join(body))
    print(f"[vk] {len(SHADERS)} shaders {'compiled with ' + compiler if compiler else 'left out (no glslc / glslangValidator found): the Vulkan renderer will not start'}")


if __name__ == "__main__":
    main()
