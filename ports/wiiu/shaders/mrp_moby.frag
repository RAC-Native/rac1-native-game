// The MRP's mobys on the Wii U (ports/wiiu/game/mrp_moby_gx2.cpp): mrp_moby_renderer.cpp's kFs brought to GLSL 330 for
// CafeGLSL: the GS's texture functions, the texture level, FGE's fog and the alpha test with its fail modes (the AFAIL split's
// two passes: uC.x). What differs from the PC's: no modern lighting (the sh_* hooks are the identity), and the "full texture"
// level (mip mode 1) from the derivatives of the texture coordinates instead of textureQueryLod.
#version 330 core
noperspective in vec4 vColor;
noperspective in vec3 vStq;
noperspective in float vFog;
flat in uvec4 vCmd;  // y the texture word (bit 0 white, 1..2 TFX, 3 TCC, 4..6 MXL, 7 LCM, 8..9 L, 10..21 K, 22..24 mips - 1), z the layer
uniform sampler2DArray uTex;
layout(std140) uniform MobyPixel {
    vec4 uFogCol;  // rgb: FOGCOL / 255
    ivec4 uA;      // TME, ATE, ATST, AREF
    ivec4 uB;      // AFAIL, the mip mode (0 the GS's level from Q, 1 every mip by the derivatives), the shade debug, FGE
    ivec4 uC;      // x: the pass (0 the colour, 1 the Z of a batch whose alpha test's fail mode writes one of them only)
};
out vec4 FragColor;
void main() {
    vec4 c = vColor;
    vec4 o = c;
    uint w = vCmd.y;
    vec2 st = vStq.xy / vStq.z;
    // (the derivatives in uniform control flow, before the branches)
    vec2 tsz = vec2(textureSize(uTex, 0).xy);
    vec2 ddx = dFdx(st * tsz), ddy = dFdy(st * tsz);
    float auto_lod = 0.5 * log2(max(max(dot(ddx, ddx), dot(ddy, ddy)), 1e-20));
    int tme = uA.x, uShade = uB.z;
    if (tme != 0) {
        vec4 t;
        if ((w & 1u) != 0u) {
            t = vec4(128.0 / 255.0);
        } else {
            int mips = int((w >> 22) & 7u) + 1;
            float layer = float(vCmd.z);
            if (uB.y == 0) {
                int mxl = int((w >> 4) & 7u), lcm = int((w >> 7) & 1u), L = int((w >> 8) & 3u);
                float K = float((int(w >> 10) << 20) >> 20) / 16.0;
                float lod = lcm != 0 ? K : log2(1.0 / max(abs(vStq.z), 1e-20)) * float(1 << L) + K;
                int level = lod <= 0.0 ? 0 : min(int(floor(lod + 0.5)), mxl);
                level = min(level, mips - 1);
                t = textureLod(uTex, vec3(st, layer), float(level));
            } else {
                float lod = clamp(auto_lod, 0.0, float(mips - 1));
                t = textureLod(uTex, vec3(st, layer), lod);
            }
        }
        uint tfx = (w >> 1) & 3u, tcc = (w >> 3) & 1u;
        if (tfx == 0u) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0); o.a = tcc != 0u ? t.a * c.a * (255.0 / 128.0) : c.a; }
        else if (tfx == 1u) { o.rgb = t.rgb; o.a = tcc != 0u ? t.a : c.a; }
        else if (tfx == 2u) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc != 0u ? t.a + c.a : c.a; }
        else { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc != 0u ? t.a : c.a; }
        if (uShade == 2) o.rgb = t.rgb;
    }
    if (uShade == 1) o.rgb = c.rgb;
    else if (uShade == 3) o.rgb = vec3(0.8);
    o = clamp(o, 0.0, 1.0);
    if (uB.w != 0) {  // the GS's fogging (PRIM FGE) after the texture function: Cv F + FOGCOL (1 - F), F the byte / 255
        float f = vFog / 255.0;
        o.rgb = o.rgb * f + uFogCol.rgb * (1.0 - f);
    }
    float a = o.a * 255.0;
    if (uA.y != 0) {
        int atst = uA.z;
        float aref = float(uA.w);
        bool pass = true;
        if (atst == 0) pass = false;
        else if (atst == 2) pass = a < aref;
        else if (atst == 3) pass = a <= aref;
        else if (atst == 4) pass = abs(a - aref) < 0.5;
        else if (atst == 5) pass = a >= aref;
        else if (atst == 6) pass = a > aref;
        else if (atst == 7) pass = abs(a - aref) >= 0.5;
        int afail = uB.x;
        if (!pass && uC.x == 0 && (afail == 0 || afail == 2)) discard;
        if (!pass && uC.x == 1 && afail != 2) discard;
    }
    FragColor = vec4(o.rgb, min(o.a * (255.0 / 128.0), 1.0));
}
