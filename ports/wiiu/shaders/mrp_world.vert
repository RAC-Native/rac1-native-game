// The MRP world's ties and shrubs on the Wii U (ports/wiiu/game/mrp_world_gx2.cpp): mrp_world_renderer.cpp's kVsGpu (the GPU path,
// tie A / shrub A) brought to GLSL 330 for CafeGLSL. The packet's kicked vertex (its sources: the template) evaluated as the CPU
// reference does (tie_kick / shrub_kick) with the instance's rows, morph, lit colours, F. What differs from the PC's:
//  - no storage buffers: the draw's instance and record are a uniform block (written by the PPC with each 32-bit word swapped: GX2
//    reads the blocks as little-endian words), the resident lit colours an R32UI texture read with texelFetch;
//  - the resident colours only (the static path's instances; the walker's copied colours, Cols, are not on the Wii U);
//  - no `precise` (GLSL 400): plain arithmetic (the tie fat colour's float steps: whether the compiler fuses them is to compare with the
//    PC's on the console);
//  - no modern lighting (sh_clip, vShWorld), no gl_BaseInstance;
//  - the merged draws (mrp_world_gx2.cpp, mrp_world_merge.inc): a draw stands for several draw commands - an instance's consecutive
//    ones, or (instanced) the same ones of consecutive instances -, so the record comes from the vertex's template run (aRun) in the
//    frame's run table (uRuns: 4 words a run), the instance from the block's 256-byte slot gl_InstanceID (uModeW.x). A block whose
//    dCmd2.y is 0 holds its own record (one draw command a draw: the per-draw path, RAC1_WIIU_WORLD_MERGE=0, and the commands the
//    merge leaves alone), as before.
#version 330 core
layout(location = 0) in ivec4 aPos;    // x, y, z, flags (1: a tie fat position, 2: a tie fat colour)
layout(location = 1) in ivec4 aDelta;  // a tie fat vertex's delta
layout(location = 2) in ivec4 aUv;     // s, t, q (shrub h) x 4096
layout(location = 3) in uvec4 aCol;    // colour indices (tie fat: c0, c1, c2)
layout(location = 4) in uint aRun;     // the template run the vertex belongs to (its record in uRuns)

// the frame's: mrp_world_renderer.cpp's plain uniforms
layout(std140) uniform WorldFrame {
    vec4 uFrame;   // fb width, fb height, x scale, 0
    vec4 uXyOff;   // XYOFFSET x, y (1/16 px), depth a, depth b
    ivec4 uModeV;  // x: 0 the world's projection (the instance's clip matrix), 1 the GS's frame; y: 1 (testing) the model's own x, y as the
                   // position; z: the colours' texture's row, log2 (texels)
    ivec4 uModeW;  // x: 1 the instance's slot is gl_InstanceID (the instanced draws), 0 the first; y: the run table's row, log2 (texels)
};
// the draw's instances, a 256-byte slot each (16 uvec4, the floats' bits): its instance (InstGpu: the clip matrix's columns 0..3, the
// rows R0, R1, R2, T 4..7, the morph 8, info 9: colour first, F, alpha, kind (0 tie, 1 shrub) | resident << 1) and a record (10: the
// draw command's instance, texture word, layer, draw_flags; 11: FOGCOL, 1 the record in uRuns by the vertex's run, 0, 0)
layout(std140) uniform WorldDraw {
    uvec4 iD[4096];
};
// the resident lit colours: RGBA8 as the level's data holds them (R in the low byte), 1 << uModeV.z a row
uniform usampler2D uCols;
// the frame's run table: per template run 4 texels (texture word, layer, draw_flags, FOGCOL), 1 << uModeW.y a row
uniform usampler2D uRuns;

smooth out vec3 vStq;
noperspective out float vLodQ;
noperspective out vec4 vColor;
flat out vec4 vFlat;
noperspective out float vFog;
flat out uvec2 vTex;
flat out uvec2 vState;
int ftoi4(float f) { return int(clamp(f * 16.0, -2147483648.0, 2147483520.0)); }
uint rcol(uint i) {
    int n = uModeV.z;
    return texelFetch(uCols, ivec2(int(i) & ((1 << n) - 1), int(i) >> n), 0).x;
}
// (a resident colour as the bytes VU1 memory holds: 2^23 + byte, after the UNPACK's STMOD row 0x4b000000)
uvec4 vu_bytes(uint c) { return (uvec4(c) >> uvec4(0u, 8u, 16u, 24u) & 0xffu) | 0x4b000000u; }
uint run_word(uint i) {
    int n = uModeW.y;
    return texelFetch(uRuns, ivec2(int(i) & ((1 << n) - 1), int(i) >> n), 0).x;
}
vec4 fv(int i) { return uintBitsToFloat(iD[i]); }
void main() {
    int b = uModeW.x != 0 ? gl_InstanceID * 16 : 0;
    vec4 iClip0 = fv(b), iClip1 = fv(b + 1), iClip2 = fv(b + 2), iClip3 = fv(b + 3);
    vec4 iRows0 = fv(b + 4), iRows1 = fv(b + 5), iRows2 = fv(b + 6), iRows3 = fv(b + 7);
    vec4 iMorph = fv(b + 8);
    uvec4 iInfo = iD[b + 9];
    uvec4 dCmd = iD[b + 10];
    uvec4 dCmd2 = iD[b + 11];
    if (dCmd2.y != 0u) {
        uint r = aRun * 4u;
        dCmd = uvec4(dCmd.x, run_word(r), run_word(r + 1u), run_word(r + 2u));
        dCmd2.x = run_word(r + 3u);
    }
    vState = uvec2(dCmd.w, dCmd2.x);
    vTex = dCmd.yz;
    // the position: tie fat (delta x morph) + base
    vec3 P = vec3(aPos.xyz);
    if ((aPos.w & 1) != 0) {
        vec3 d = vec3(aDelta.xyz) * iMorph.x;
        P = d + vec3(aPos.xyz);
    }
    vec4 p = iRows3;
    p = p + iRows0 * P.x;
    p = p + iRows1 * P.y;
    p = p + iRows2 * P.z;
    float Q = 1.0 / p.w;
    vec3 uv = vec3(aUv.xyz) * (1.0 / 4096.0);
    // the colour: tie dinky the lit colour's bytes, tie fat ((C1 0.5 + C2 0.5) morph.w + C0 morph.z), shrub the colour and the instance's alpha
    uvec4 c;
    uint first = iInfo.x;
    uint kind = iInfo.w & 1u;
    if (kind == 0u) {
        uvec4 c0 = vu_bytes(rcol(first + aCol.x));
        if ((aPos.w & 2) != 0) {
            uvec4 c1 = vu_bytes(rcol(first + aCol.y));
            uvec4 c2 = vu_bytes(rcol(first + aCol.z));
            for (int l = 0; l < 4; ++l) {
                float a = uintBitsToFloat(c1[l]) * 0.5;
                a = a + uintBitsToFloat(c2[l]) * 0.5;
                float b = uintBitsToFloat(c0[l]) * iMorph.z;
                float r = a * iMorph.w;
                r = r + b;
                c[l] = floatBitsToUint(r) & 0xffu;
            }
        } else {
            c = c0 & 0xffu;
        }
    } else {
        c = uvec4(uvec3(rcol(first + aCol.x)) >> uvec3(0u, 8u, 16u) & 0xffu, iInfo.z);
    }
    vColor = vec4(c) / 255.0;
    vFlat = vColor;
    vFog = float(iInfo.y);
    vStq = uv;  // ((s, t, q) Q: undivided by Q = 1 / w)
    vLodQ = uv.z * Q;
    if (uModeV.x == 0) {
        gl_Position = mat4(iClip0, iClip1, iClip2, iClip3) * vec4(P, 1.0);
    } else {
        // the GS's frame from the kicked coordinates (FTOI4, 16 bits; Z 24 bits), the host's depth a + b Z, as homogeneous coordinates
        // with w
        float w = p.w;
        float X = float(ftoi4(p.x * Q) & 0xffff) / 16.0, Y = float(ftoi4(p.y * Q) & 0xffff) / 16.0;
        float Z = float((ftoi4(p.z * Q) >> 4) & 0xffffff);
        float X1 = uFrame.x * 0.5;
        float sx = uFrame.z / X1, sy = 1.0 / (uFrame.y * 0.5);
        vec2 cc = vec2(uXyOff.x / 16.0 + X1, uXyOff.y / 16.0 + uFrame.y * 0.5);
        float zc = (2.0 * (uXyOff.z + uXyOff.w * Z) - 1.0) * w;
        gl_Position = vec4((X - cc.x) * w * sx, -(Y - cc.y) * w * sy, zc, w);
    }
    if (uModeV.y == 1) gl_Position = vec4(vec2(aPos.xy) / 2048.0, 0.5, 1.0);
}
