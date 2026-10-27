// The MRP's mobys on the Wii U (ports/wiiu/game/mrp_moby_gx2.cpp): mrp_moby_renderer.cpp's kVs brought to GLSL 330 for
// CafeGLSL. The same arithmetic: the VU0 skinning (one joint, or the blend of two / three, weights / 256, the position
// truncated to the EE's s16), the original's lighting (three light rows and the tint row, the EE's fixed-point colour step on
// the float's bits), the VU1 program's transform (micro 0x22d) and its fog. What differs from the PC's:
//  - no storage buffers: the draw's part and record, the moby's joints, the normal table and the frame's uniforms are
//    uniform blocks (std140, written by the PPC as big-endian words: what GX2 reads);
//  - the joints of the draw's moby alone (MobyDraw's pInfo.x is 0, pInfo.y their count, at most kMaxJoints);
//  - no `precise` (GLSL 400): plain arithmetic (whether the compiler fuses a * b + c is to compare with the PC's on the console);
//  - no modern lighting (the sh_* hooks are the identity), no shader check outputs, no gl_BaseInstance.
#version 330 core
layout(location = 0) in ivec4 aPosN;     // s16 x, y, z, the normal's angle bytes n0 | n1 << 8
layout(location = 1) in uvec4 aJoint;    // joints 0..2, n (0 unresolved, 1 a joint, 2 / 3 a blend)
layout(location = 2) in uvec4 aWeight;   // weights 0..2, flags (1: STQ x the Q of the q2 slot)
layout(location = 3) in uvec4 aRgba;
layout(location = 4) in ivec2 aUv;
layout(location = 5) in ivec4 aQ2Pos;
layout(location = 6) in uvec4 aQ2Joint;
layout(location = 7) in uvec4 aQ2Weight;

// the frame's (or a view's): mrp_moby_renderer.cpp's plain uniforms
layout(std140) uniform MobyFrame {
    vec4 uOffset;   // B+296
    vec4 uFrame;    // fb width, fb height, x scale, 1 a render target (its rows not flipped)
    vec4 uXyOff;    // XYOFFSET x, y (1/16 px), depth a, depth b
    vec4 uProjFog;  // x: B+292 x (Q = proj / w), y: B+291 x (the fog's floor)
    ivec4 uModeV;   // x: 0 the world's projection (the part's clip matrix), 1 the VU1's onto the game's frame; y: 1 (testing) the
                    // model's own x, y as the position, 2 a triangle over the screen
};
// the overlay's normal table 0x166080: 256 (a, b) pairs, two to a vector
layout(std140) uniform MobyNormals {
    vec4 uNormals[128];
};
// the draw's moby's joints: four rows each (row 3 the translation)
layout(std140) uniform MobyJoints {
    vec4 J[512];
};
// the draw: its part (PartGpu) and its record (the draw command's 4 words: part, texture word, layer, 0)
layout(std140) uniform MobyDraw {
    vec4 pClip[4];   // world mode: model -> clip (columns)
    vec4 pRows[4];   // the VU1 rows R0, R1, R2, T
    vec4 pLight[8];  // the skinning record's rows (+0x40..+0xbf)
    vec4 pSub;       // the sub-mesh colour (bytes)
    ivec4 pInfo;     // first joint (0 here), joint count, flags (1: sub-mesh colour), 1: the clipping program (the fog per vertex)
    uvec4 pCmd;
};

noperspective out vec4 vColor;
noperspective out vec3 vStq;
noperspective out float vFog;  // the XYZF2's F byte (the GS interpolates it along the triangle)
flat out uvec4 vCmd;

vec2 normal_of(uint i) {
    vec4 v = uNormals[i >> 1];
    return (i & 1u) != 0u ? v.zw : v.xy;
}
vec4 joint_row(uint j, int r, int first, int count) {
    return int(j) < count ? J[(first + int(j)) * 4 + r] : vec4(0.0);
}
// the VU0 matrix of a slot (mrp::skin_packet): a joint, or the blend M[j0] w0 + M[j1] w1 (+ M[j2] w2)
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
        vec4 a = joint_row(jt.x, r, first, count) * w0;
        a = a + joint_row(jt.y, r, first, count) * w1;
        if (jt.w == 3u) a = a + joint_row(jt.z, r, first, count) * w2;
        m[r] = a;
    }
}
// the position: ((R0 x + R1 y) + R2 z) + R3, truncated (FTOI0, saturating), the EE's s16
ivec3 skin_pos(vec4 m[4], ivec3 p) {
    ivec3 q;
    for (int l = 0; l < 3; ++l) {
        float a = m[0][l] * float(p.x);
        a = a + m[1][l] * float(p.y);
        a = a + m[2][l] * float(p.z);
        a = a + m[3][l];
        int i = int(clamp(a, -2147483648.0, 2147483520.0));
        q[l] = (i << 16) >> 16;
    }
    return q;
}
// FTOI4, saturating
int ftoi4(float f) {
    return int(clamp(f * 16.0, -2147483648.0, 2147483520.0));
}

void main() {
    int first = pInfo.x, count = pInfo.y;
    vec4 m[4];
    matrix_of(aJoint, aWeight, first, count, m);
    ivec3 q = skin_pos(m, aPosN.xyz);

    // the lighting (skin_packet: the VU0 program's second half and the EE's colour step)
    uint nb = uint(aPosN.w) & 0xffffu;
    vec2 ta = normal_of(nb & 0xffu), te = normal_of((nb >> 8) & 0xffu);
    float nx = ta.x * te.x;
    float ny = ta.y * te.x;
    float nw = te.y;
    vec4 n = m[0] * nx;
    n = n + m[1] * ny;
    n = n + m[2] * nw;
    float len2 = n.x * n.x + n.y * n.y;
    len2 = len2 + n.z * n.z;
    float qn = len2 > 0.0 ? 1.0 / sqrt(len2) : 3.4028235e38;
    vec4 d = pLight[0] * n.x;
    d = d + pLight[1] * n.y;
    d = d + pLight[2] * n.z;
    vec4 dk = d * (-abs(pLight[6]));
    d = max(d, dk);
    vec4 a = pLight[3] * d.x;
    a = a + pLight[4] * d.y;
    a = a + pLight[5] * d.z;
    ivec4 col;
    for (int l = 0; l < 4; ++l) {
        float c = (a[l] == 0.0 ? 0.0 : a[l] * qn) + pLight[7][l] * 1.0;
        int h = int(floatBitsToUint(c) << 16) >> 16;
        int pr = clamp(h * int(aRgba[l]), -32768, 32767);
        col[l] = (pr >> 7) & 0xff;
    }
    if ((pInfo.z & 1) != 0) col = ivec4(pSub);

    // the VU1 program (micro 0x22d, mrp::vu1_vertex / kick_triangles)
    float uProj = uProjFog.x;
    vec4 p = pRows[3];
    p = p + pRows[0] * float(q.x);
    p = p + pRows[1] * float(q.y);
    p = p + pRows[2] * float(q.z);
    float Q = uProj / p.w;
    float qq = Q;
    if ((aWeight.w & 1u) != 0u) {  // STQ x Q twice: the Q of another slot
        vec4 m2[4];
        matrix_of(aQ2Joint, aQ2Weight, first, count, m2);
        ivec3 q2 = skin_pos(m2, aQ2Pos.xyz);
        float w2 = pRows[3].w;
        w2 = w2 + pRows[0].w * float(q2.x);
        w2 = w2 + pRows[1].w * float(q2.y);
        w2 = w2 + pRows[2].w * float(q2.z);
        qq = Q * (uProj / w2);
    }
    vec3 stq = vec3(float(aUv.x) * (1.0 / 4096.0) * qq, float(aUv.y) * (1.0 / 4096.0) * qq, 1.0 * qq);
    float X = p.x * Q + uOffset.x;
    float Y = p.y * Q + uOffset.y;
    int xi = ftoi4(X), yi = ftoi4(Y);

    if (uModeV.x == 0) {
        gl_Position = mat4(pClip[0], pClip[1], pClip[2], pClip[3]) * vec4(vec3(q), 1.0);
    } else {
        // the GS's frame: x = (X16 - XYOFFSET) / 16 px (the VU's 1/16 px snap), the host's depth a + b Z as homogeneous
        // coordinates with w = 1 / Q = p.w / proj (x w, y w, z w all linear in p) so that the GPU clips at the near plane what
        // the VU's clipping program (0x00a) clips; behind the camera (w <= 0) no snap
        float wq = p.w / uProj;
        float X1 = uFrame.x * 0.5;
        float sx = uFrame.z / X1, sy = 1.0 / (uFrame.y * 0.5);
        vec2 c = vec2(uXyOff.x / 16.0 + X1, uXyOff.y / 16.0 + uFrame.y * 0.5);  // the frame's centre in GS units
        vec2 xy = wq > 0.0 ? vec2(float(xi), float(yi)) / 16.0 * wq : vec2(p.x + uOffset.x * wq, p.y + uOffset.y * wq);
        float zc = (2.0 * (uXyOff.z + uXyOff.w * uOffset.z) - 1.0) * wq + 2.0 * uXyOff.w * p.z;  // (2 (a + b Z) - 1) w, Z = p.z Q + offset.z
        gl_Position = vec4((xy.x - c.x * wq) * sx, (uFrame.w != 0.0 ? 1.0 : -1.0) * (xy.y - c.y * wq) * sy, zc, wq);
    }
    if (uModeV.y == 1) gl_Position = vec4(vec2(aPosN.xy) / 2048.0, 0.5, 1.0);
    if (uModeV.y == 2) {  // (testing: a triangle over the screen from the vertex's index alone)
        int k = gl_VertexID % 3;
        gl_Position = vec4(k == 1 ? 3.0 : -1.0, k == 2 ? 3.0 : -1.0, 0.5, 1.0);
    }
    // the fog: the VU1 program's F = min(max(T.w + offset.w, floor), 255) (the moby's, micro 0x22d) or, in the clipping program
    // (0x00a: pInfo.w bit 0), min(max(p.w + offset.w, floor), 255) per vertex; the XYZF2's F is the integer part
    vFog = floor(min(max(((pInfo.w & 1) != 0 ? p.w : pRows[3].w) + uOffset.w, uProjFog.y), 255.0));
    vColor = vec4(col) / 255.0;
    vStq = stq;
    vCmd = pCmd;
}
