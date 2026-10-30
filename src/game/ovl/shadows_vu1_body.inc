// The shadow-mesh program's instructions (shadows_vu1.cpp), included twice there: in `exact` with the lane rules as the port always had
// them (each operand and result through vuc's integer masks), in `fast` with the same rules decided on the FPU (vu1_fast.inc). The
// including namespace defines op / opf / put (a lane's operand, a broadcast's, a result), ftoi, clipw, divq, maxw / miniw and cpw (a w
// lane's copy) - the rest is here, once. Host: see shadows_vu1.cpp for the program's description.

// orig: (helper) ADD / SUB (vectors), ADDbc / SUBbc / MULbc (a broadcast operand)
void add(VUVector& d, const VUVector& a, const VUVector& b, int m) {
    const VUVector A = a, B = b;
    for (int l = 0; l < 4; ++l) if (m & (1 << l)) put(d, l, op(A, l) + op(B, l));
}
void sub(VUVector& d, const VUVector& a, const VUVector& b, int m) {
    const VUVector A = a, B = b;
    for (int l = 0; l < 4; ++l) if (m & (1 << l)) put(d, l, op(A, l) - op(B, l));
}
// orig: (helper)
void addbc(VUVector& d, const VUVector& a, float b, int m) {
    const VUVector A = a;
    for (int l = 0; l < 4; ++l) if (m & (1 << l)) put(d, l, op(A, l) + opf(b));
}
// orig: (helper)
void subbc(VUVector& d, const VUVector& a, float b, int m) {
    const VUVector A = a;
    for (int l = 0; l < 4; ++l) if (m & (1 << l)) put(d, l, op(A, l) - opf(b));
}
// orig: (helper)
void mulbc(VUVector& d, const VUVector& a, float b, int m) {
    const VUVector A = a;
    for (int l = 0; l < 4; ++l) if (m & (1 << l)) put(d, l, op(A, l) * opf(b));
}
// orig: (helper) MULA (vectors) / MULAbc / MADDAbc into ACC, MADDbc from ACC into d (xyzw)
void mula(VUState& s, const VUVector& a, const VUVector& b) {
    const VUVector A = a, B = b;
    for (int l = 0; l < 4; ++l) put(s.acc, l, op(A, l) * op(B, l));
}
void mulabc(VUState& s, const VUVector& a, float b) {
    const VUVector A = a;
    for (int l = 0; l < 4; ++l) put(s.acc, l, op(A, l) * opf(b));
}
// orig: (helper)
void maddabc(VUState& s, const VUVector& a, float b) {
    const VUVector A = a;
    for (int l = 0; l < 4; ++l) put(s.acc, l, op(s.acc, l) + op(A, l) * opf(b));
}
// orig: (helper)
void maddbc(VUState& s, VUVector& d, const VUVector& a, float b) {
    const VUVector A = a, C = s.acc;
    for (int l = 0; l < 4; ++l) put(d, l, op(C, l) + op(A, l) * opf(b));
}
// orig: (helper)
void itof0(VUVector& d) {
    for (int l = 0; l < 4; ++l) d.f[l] = static_cast<float>(static_cast<int32_t>(d.u[l]));
}
// orig: (helper) FCAND / FCOR vi1
int16_t fcand(const VUState& s, uint32_t m) { return (s.clipflag & m) != 0 ? 1 : 0; }
int16_t fcor(const VUState& s, uint32_t m) { return ((s.clipflag | m) & 0xffffff) == 0xffffff ? 1 : 0; }
// orig: (helper) LQ / SQ / LQI / SQI / ILW / ISW on the 16 KB memory (qword addresses wrap)
uint8_t* qw(VUState& s, int32_t a) { return &s.mem[(static_cast<uint32_t>(a) & 0x3ff) << 4]; }
void lq(VUState& s, VUVector& d, int32_t a, int m = XYZW) {
    uint32_t q[4];
    std::memcpy(q, qw(s, a), 16);
    for (int l = 0; l < 4; ++l) if (m & (1 << l)) d.u[l] = q[l];
}
void sq(VUState& s, const VUVector& v, int32_t a, int m = XYZW) {
    uint32_t q[4];
    std::memcpy(q, qw(s, a), 16);
    for (int l = 0; l < 4; ++l) if (m & (1 << l)) q[l] = v.u[l];
    std::memcpy(qw(s, a), q, 16);
}
// orig: (helper)
void lqi(VUState& s, VUVector& d, int16_t& a) {
    lq(s, d, a);
    a = static_cast<int16_t>(a + 1);
}
// orig: (helper)
void sqi(VUState& s, const VUVector& v, int16_t& a) {
    sq(s, v, a);
    a = static_cast<int16_t>(a + 1);
}
// orig: (helper)
int16_t ilw(VUState& s, int32_t a, int lane) {
    uint32_t w;
    std::memcpy(&w, qw(s, a) + 4 * lane, 4);
    return static_cast<int16_t>(w & 0xffff);
}
// orig: (helper)
void isw(VUState& s, int16_t v, int32_t a, int lane) {
    const uint32_t w = static_cast<uint16_t>(v);
    std::memcpy(qw(s, a) + 4 * lane, &w, 4);
}
int16_t i16(int32_t v) { return static_cast<int16_t>(v); }
// orig: (helper) the vertex transform: ACC = r3 w, + r0 x, + r1 y, d = ACC + r2 z
void xform(VUState& s, VUVector& d, int r) {
    const VUVector v = d;
    mulabc(s, s.vf[r + 3], op(s.vf[0], 3));
    maddabc(s, s.vf[r], op(v, 0));
    maddabc(s, s.vf[r + 1], op(v, 1));
    maddbc(s, d, s.vf[r + 2], op(v, 2));
}

// The clipper's polygon emits (subroutines returning by JR vi10): the edge from vf3 (previous
// vertex: vf14 ST, vf15 colour) to vf10 (current: vf16, vf17) cut at Q along it.
// orig: (helper) 0x22b: entering — the cut point, then the current vertex
void clip_in(VUState& s) {
    auto& vf = s.vf;
    auto& vi = s.vi;
    sub(vf[11], vf[10], vf[3], XYZW);
    mulabc(s, vf[11], s.q);
    maddbc(s, vf[11], vf[3], op(vf[0], 3));
    sqi(s, vf[11], vi[8]);
    sub(vf[11], vf[16], vf[14], XYZW);
    mulabc(s, vf[11], s.q);
    maddbc(s, vf[11], vf[14], op(vf[0], 3));
    sqi(s, vf[11], vi[8]);
    sub(vf[11], vf[17], vf[15], XYZW);
    mulabc(s, vf[11], s.q);
    maddbc(s, vf[11], vf[15], op(vf[0], 3));
    sqi(s, vf[11], vi[8]);
    sqi(s, vf[10], vi[8]);
    sqi(s, vf[16], vi[8]);
    sqi(s, vf[17], vi[8]);
    vi[9] = i16(vi[9] + 2);  // 0x23b (JR's delay slot)
}
// orig: (helper) 0x23c: leaving — the cut point only
void clip_out(VUState& s) {
    auto& vf = s.vf;
    auto& vi = s.vi;
    sub(vf[10], vf[10], vf[3], XYZW);
    vi[9] = i16(vi[9] + 1);
    sub(vf[16], vf[16], vf[14], XYZW);
    sub(vf[17], vf[17], vf[15], XYZW);
    mulabc(s, vf[10], s.q);
    maddbc(s, vf[10], vf[3], op(vf[0], 3));
    mulabc(s, vf[16], s.q);
    maddbc(s, vf[16], vf[14], op(vf[0], 3));
    mulabc(s, vf[17], s.q);
    sqi(s, vf[10], vi[8]);
    maddbc(s, vf[17], vf[15], op(vf[0], 3));
    sqi(s, vf[16], vi[8]);
    sqi(s, vf[17], vi[8]);  // 0x246 (JR's delay slot)
}
// orig: (helper) 0x247: both inside — the current vertex
void clip_keep(VUState& s) {
    auto& vf = s.vf;
    auto& vi = s.vi;
    sqi(s, vf[10], vi[8]);
    sqi(s, vf[16], vi[8]);
    sqi(s, vf[17], vi[8]);
    vi[9] = i16(vi[9] + 1);  // 0x24b
}

// One clipping plane over the polygon at vi15 (vi7 vertices) into vi8: per edge (vi11 the
// previous vertex, vi12 the current) the cut factor and the emit chosen by the two clip results.
// `c` the lane (0 x, 1 y, 2 z), `neg` the side (true: c < -w, the A form; false: c > w), `ret` the
// address loaded into vi10, `both` / `prev` / `cur` the FCOR masks (both outside, previous only,
// current only).
// orig: (helper) the planes at 0x13e (z, -w), 0x166 (x, -w), 0x18e (x, +w), 0x1b7 (y, -w),
// 0x1df (y, +w)
void clip_plane(VUState& s, int c, bool neg, int16_t ret, uint32_t both, uint32_t prev, uint32_t cur) {
    auto& vf = s.vf;
    auto& vi = s.vi;
    const int C = 1 << c;
    vi[12] = i16(vi[7] - 1);
    vi[11] = i16(vi[12] + vi[12]);
    vi[11] = i16(vi[11] + vi[12]);
    vi[11] = i16(vi[11] + vi[15]);
    vi[12] = vi[15];
    for (;;) {
        lq(s, vf[3], vi[11]);
        lq(s, vf[10], vi[12]);
        lq(s, vf[14], vi[11] + 1);
        lq(s, vf[15], vi[11] + 2);
        clipw(s, vf[3]);
        lq(s, vf[16], vi[12] + 1);
        if (neg) {
            sub(vf[11], vf[10], vf[3], C | W);
            lq(s, vf[17], vi[12] + 2);
            addbc(vf[11], vf[11], op(vf[11], c), W);
            subbc(vf[11], vf[0], op(vf[3], 3), C);
            subbc(vf[11], vf[11], op(vf[3], c), C);
        } else {
            sub(vf[11], vf[10], vf[3], C);
            lq(s, vf[17], vi[12] + 2);
            sub(vf[11], vf[3], vf[10], W);
            addbc(vf[11], vf[11], op(vf[11], c), W);
            addbc(vf[11], vf[0], op(vf[3], 3), C);
            subbc(vf[11], vf[11], op(vf[3], c), C);
        }
        clipw(s, vf[10]);
        divq(s, vf[11], c, vf[11], 3);
        vi[10] = ret;
        vi[1] = fcor(s, both);
        const bool skip = vi[1] != 0;
        vi[1] = fcor(s, prev);  // (IBNE's delay slot)
        if (!skip) {
            const bool in = vi[1] != 0;
            vi[1] = fcor(s, cur);  // (IBNE's delay slot)
            if (in) clip_in(s);
            else if (vi[1] != 0) clip_out(s);
            else clip_keep(s);
        }
        // the return address: vi10
        vi[7] = i16(vi[7] - 1);
        vi[11] = vi[12];
        vi[12] = i16(vi[12] + 3);
        if (vi[7] == 0) break;
    }
}


// orig: 0x0010e818 (the program; shadows_vu1 runs it with the exact or the fast lane rules)
void program(VUState& s, uint32_t start, const std::function<void(uint32_t)>& kick) {
    auto& vf = s.vf;
    auto& vi = s.vi;
    const float one = op(vf[0], 3);
    if (start == 0) {  // 0x000..0x00b: the constants (0x00b the E bit's delay slot)
        static const int regs[12] = {18, 19, 20, 21, 22, 23, 24, 25, 31, 29, 30, 7};
        for (int k = 0; k < 12; ++k) lq(s, vf[regs[k]], 0x3a4 + k);
        return;
    }
    if (start == 0xc) {
        vi[15] = static_cast<int16_t>(s.xtop);  // 0x00d XTOP (B's delay slot)
        // 0x010
        vi[1] = 0x7fff;
        vi[12] = ilw(s, vi[15], 0);
        vi[12] = i16(vi[12] & vi[1]);
        vi[6] = i16(vi[15] + 1);
        vi[7] = i16(vi[15] + 1);
        lq(s, vf[8], vi[6] + 2);
        lq(s, vf[12], vi[6]);
        vi[6] = i16(vi[6] + 3);
        xform(s, vf[8], 18);  // 0x018..0x01b
        lq(s, vf[1], vi[6] + 2);
        lq(s, vf[5], vi[6]);
        divq(s, vf[31], 3, vf[8], 3);
        vi[6] = i16(vi[6] + 3);
        xform(s, vf[1], 18);  // 0x023..0x026
        cpw(vf[9], vf[8]);
        mulbc(vf[9], vf[8], s.q, XYZ);
        mulbc(vf[13], vf[12], s.q, XYZ);
        lq(s, vf[8], vi[6] + 2);
        lq(s, vf[12], vi[6]);
        divq(s, vf[31], 3, vf[1], 3);
        vi[6] = i16(vi[6] + 3);
        add(vf[9], vf[9], vf[30], XYZW);  // 0x02e
        mulabc(s, vf[21], one);
        maddabc(s, vf[18], op(vf[8], 0));
        maddabc(s, vf[19], op(vf[8], 1));
        maxw(vf[9], vf[7], 0);
        cpw(vf[2], vf[1]);
        { const VUVector v = vf[8]; maddbc(s, vf[8], vf[20], op(v, 2)); }
        miniw(vf[9], vf[7], 1);
        mulbc(vf[2], vf[1], s.q, XYZ);
        mulbc(vf[6], vf[5], s.q, XYZ);
        vi[12] = i16(vi[12] - 3);
        for (;;) {
            // 0x037
            lq(s, vf[1], vi[6] + 2);
            divq(s, vf[31], 3, vf[8], 3);
            ftoi(vf[9], 16.0f);
            lq(s, vf[5], vi[6]);
            add(vf[2], vf[2], vf[30], XYZW);
            vi[6] = i16(vi[6] + 3);
            mulabc(s, vf[21], one);
            maddabc(s, vf[18], op(vf[1], 0));
            vi[12] = i16(vi[12] - 1);
            maddabc(s, vf[19], op(vf[1], 1));
            sq(s, vf[13], vi[7]);
            maxw(vf[2], vf[7], 0);
            sq(s, vf[9], vi[7] + 2);
            { const VUVector v = vf[1]; maddbc(s, vf[1], vf[20], op(v, 2)); }
            cpw(vf[9], vf[8]);
            miniw(vf[2], vf[7], 1);
            mulbc(vf[9], vf[8], s.q, XYZ);  // 0x041 | IBEQ vi12, vi0 -> 0x071
            const bool done = vi[12] == 0;
            mulbc(vf[13], vf[12], s.q, XYZ);
            vi[7] = i16(vi[7] + 3);
            if (done) {
                // 0x071
                lq(s, vf[8], vi[6] + 2);
                divq(s, vf[29], 3, vf[1], 3);
                lq(s, vf[12], vi[6]);
                ftoi(vf[2], 16.0f);
                add(vf[9], vf[9], vf[30], XYZW);
                mulabc(s, vf[21], one);
                maddabc(s, vf[18], op(vf[8], 0));
                sq(s, vf[6], vi[7]);
                maddabc(s, vf[19], op(vf[8], 1));
                sq(s, vf[2], vi[7] + 2);
                maxw(vf[9], vf[7], 0);
                cpw(vf[2], vf[1]);
                { const VUVector v = vf[8]; maddbc(s, vf[8], vf[20], op(v, 2)); }
                miniw(vf[9], vf[7], 1);
                mulbc(vf[2], vf[1], s.q, XYZ);
                mulbc(vf[6], vf[5], s.q, XYZ);
                vi[7] = i16(vi[7] + 3);
                divq(s, vf[29], 3, vf[8], 3);  // 0x07f
                ftoi(vf[9], 16.0f);
                add(vf[2], vf[2], vf[30], XYZW);
                sq(s, vf[13], vi[7]);
                sq(s, vf[9], vi[7] + 2);
                maxw(vf[2], vf[7], 0);
                cpw(vf[9], vf[8]);
                miniw(vf[2], vf[7], 1);
                mulbc(vf[9], vf[8], s.q, XYZ);
                mulbc(vf[13], vf[12], s.q, XYZ);
                vi[7] = i16(vi[7] + 3);
                ftoi(vf[2], 16.0f);
                sq(s, vf[6], vi[7]);
                sq(s, vf[2], vi[7] + 2);
                kick(static_cast<uint16_t>(vi[15]));  // 0x091 XGKICK; 0x092 [E]
                lq(s, vf[31], 0x3ac);                 // 0x093 (the E bit's delay slot)
                return;
            }
            // 0x043
            lq(s, vf[8], vi[6] + 2);
            divq(s, vf[31], 3, vf[1], 3);
            ftoi(vf[2], 16.0f);
            lq(s, vf[12], vi[6]);
            add(vf[9], vf[9], vf[30], XYZW);
            vi[6] = i16(vi[6] + 3);
            mulabc(s, vf[21], one);
            maddabc(s, vf[18], op(vf[8], 0));
            vi[12] = i16(vi[12] - 1);
            maddabc(s, vf[19], op(vf[8], 1));
            sq(s, vf[6], vi[7]);
            maxw(vf[9], vf[7], 0);
            sq(s, vf[2], vi[7] + 2);
            { const VUVector v = vf[8]; maddbc(s, vf[8], vf[20], op(v, 2)); }
            cpw(vf[2], vf[1]);
            miniw(vf[9], vf[7], 1);
            mulbc(vf[2], vf[1], s.q, XYZ);  // 0x04d | IBNE vi12, vi0 -> 0x037
            const bool more = vi[12] != 0;
            mulbc(vf[6], vf[5], s.q, XYZ);
            vi[7] = i16(vi[7] + 3);
            if (more) continue;
            // 0x04f
            lq(s, vf[1], vi[6] + 2);
            divq(s, vf[31], 3, vf[8], 3);
            lq(s, vf[5], vi[6]);
            ftoi(vf[9], 16.0f);
            add(vf[2], vf[2], vf[30], XYZW);
            mulabc(s, vf[21], one);
            maddabc(s, vf[18], op(vf[1], 0));
            sq(s, vf[13], vi[7]);
            maddabc(s, vf[19], op(vf[1], 1));
            sq(s, vf[9], vi[7] + 2);
            maxw(vf[2], vf[7], 0);
            cpw(vf[9], vf[8]);
            { const VUVector v = vf[1]; maddbc(s, vf[1], vf[20], op(v, 2)); }
            miniw(vf[2], vf[7], 1);
            mulbc(vf[9], vf[8], s.q, XYZ);
            mulbc(vf[13], vf[12], s.q, XYZ);
            vi[7] = i16(vi[7] + 3);
            divq(s, vf[29], 3, vf[1], 3);  // 0x05d
            ftoi(vf[2], 16.0f);
            add(vf[9], vf[9], vf[30], XYZW);
            sq(s, vf[6], vi[7]);
            sq(s, vf[2], vi[7] + 2);
            maxw(vf[9], vf[7], 0);
            cpw(vf[2], vf[1]);
            miniw(vf[9], vf[7], 1);
            mulbc(vf[2], vf[1], s.q, XYZ);
            mulbc(vf[6], vf[5], s.q, XYZ);
            vi[7] = i16(vi[7] + 3);
            ftoi(vf[9], 16.0f);
            sq(s, vf[13], vi[7]);
            sq(s, vf[9], vi[7] + 2);
            kick(static_cast<uint16_t>(vi[15]));  // 0x06f XGKICK; 0x070 [E]
            lq(s, vf[8], vi[6] + 2);              // 0x071 (the E bit's delay slot)
            return;
        }
    }
    // start 0xe: 0x00f XTOP (B's delay slot), the clipped batch from 0x093
    vi[15] = static_cast<int16_t>(s.xtop);
    lq(s, vf[31], 0x3ac);
    vi[1] = 0x7fff;
    vi[13] = ilw(s, vi[15], 0);
    vi[13] = i16(vi[13] & vi[1]);
    vi[6] = i16(vi[15] + 1);
    vi[7] = i16(vi[15] + 1);
    lq(s, vf[8], vi[6] + 2);
    lq(s, vf[12], vi[6]);
    vi[6] = i16(vi[6] + 3);
    mulabc(s, vf[25], one);  // 0x09c | LOI 2048
    s.i = 2048.0f;
    {
        const VUVector v = vf[8];
        maddabc(s, vf[22], op(v, 0));
        maddabc(s, vf[23], op(v, 1));
        maddbc(s, vf[8], vf[24], op(v, 2));
    }
    put(vf[4], 3, one * opf(s.i));  // 0x0a0 MULi.w vf4, vf0, I
    lq(s, vf[1], vi[6] + 2);
    lq(s, vf[5], vi[6]);
    divq(s, vf[0], 3, vf[8], 3);
    vi[6] = i16(vi[6] + 3);
    xform(s, vf[1], 22);  // 0x0a7..0x0aa
    cpw(vf[9], vf[8]);
    mulbc(vf[9], vf[8], s.q, XYZ);
    mulbc(vf[13], vf[12], s.q, XYZ);
    clipw(s, vf[8]);  // 0x0ad
    vf[28] = vf[27];
    vf[27] = vf[26];
    vf[26] = vf[8];
    lq(s, vf[8], vi[6] + 2);
    lq(s, vf[12], vi[6]);
    divq(s, vf[0], 3, vf[1], 3);
    vi[6] = i16(vi[6] + 3);
    mula(s, vf[9], vf[29]);  // 0x0b4
    maddbc(s, vf[9], vf[30], one);
    xform(s, vf[8], 22);  // 0x0b6..0x0b9
    cpw(vf[2], vf[1]);
    miniw(vf[9], vf[7], 1);
    mulbc(vf[2], vf[1], s.q, XYZ);
    vi[2] = 0;
    mulbc(vf[6], vf[5], s.q, XYZ);
    vi[3] = 0;
    maxw(vf[9], vf[7], 0);
    // the strip, a vertex pair per pass; 'at' the place resumed after the clipper (JR vi14)
    enum { AT_0BE, AT_0CC, AT_0DC } at = AT_0BE;
    for (;;) {
        bool clip = false;  // to the classification at 0x0e2 / 0x0ef
        if (at == AT_0BE) {
            clipw(s, vf[1]);  // 0x0be
            vf[28] = vf[27];
            vf[27] = vf[26];
            vf[26] = vf[1];
            ftoi(vf[9], 16.0f);
            lq(s, vf[1], vi[6] + 2);
            mula(s, vf[2], vf[29]);  // 0x0c2
            divq(s, vf[0], 3, vf[8], 3);
            maddbc(s, vf[2], vf[30], one);
            lq(s, vf[5], vi[6]);
            mulabc(s, vf[25], one);
            vi[6] = i16(vi[6] + 3);
            vi[1] = fcand(s, 0x02fbef);  // 0x0c5
            vi[13] = i16(vi[13] - 1);
            maxw(vf[2], vf[7], 0);
            vi[3] = vi[1];
            maddabc(s, vf[22], op(vf[1], 0));
            sq(s, vf[13], vi[7]);
            maddabc(s, vf[23], op(vf[1], 1));
            sq(s, vf[9], vi[7] + 2);
            { const VUVector v = vf[1]; maddbc(s, vf[1], vf[24], op(v, 2)); }  // 0x0ca | IBNE vi1 -> 0x0e2
            clip = vi[1] != 0;
            miniw(vf[2], vf[7], 1);
            cpw(vf[9], vf[8]);
            if (clip) {
                // 0x0e2: all three outside one plane -> just disabled (ADC), else clipped
                vi[1] = fcor(s, 0xfdf7df);
                addbc(vf[2], vf[2], op(vf[4], 3), W);
                bool out = vi[1] != 0;
                vi[1] = fcor(s, 0xff7df7);
                if (!out) { out = vi[1] != 0; vi[1] = fcor(s, 0xffbefb); }
                if (!out) { out = vi[1] != 0; vi[1] = fcor(s, 0xffdf7d); }
                if (!out) { out = vi[1] != 0; vi[1] = fcor(s, 0xffefbe); }
                if (!out) out = vi[1] != 0;
                if (!out) vi[14] = 0xcc;
                at = AT_0CC;
                clip = !out;
            } else {
                at = AT_0CC;
            }
        }
        if (!clip && at == AT_0CC) {
            mulbc(vf[9], vf[8], s.q, XYZ);  // 0x0cc | IBEQ vi13, vi0 -> 0x0de
            const bool done = vi[13] == 0;
            mulbc(vf[13], vf[12], s.q, XYZ);
            vi[7] = i16(vi[7] + 3);
            if (done) break;
            clipw(s, vf[8]);  // 0x0ce
            vf[28] = vf[27];
            vf[27] = vf[26];
            vf[26] = vf[8];
            ftoi(vf[2], 16.0f);
            lq(s, vf[8], vi[6] + 2);
            mula(s, vf[9], vf[29]);  // 0x0d2
            divq(s, vf[0], 3, vf[1], 3);
            maddbc(s, vf[9], vf[30], one);
            lq(s, vf[12], vi[6]);
            mulabc(s, vf[25], one);
            vi[6] = i16(vi[6] + 3);
            vi[1] = fcand(s, 0x02fbef);  // 0x0d5
            vi[13] = i16(vi[13] - 1);
            maxw(vf[9], vf[7], 0);
            vi[2] = vi[1];
            maddabc(s, vf[22], op(vf[8], 0));
            sq(s, vf[6], vi[7]);
            maddabc(s, vf[23], op(vf[8], 1));
            sq(s, vf[2], vi[7] + 2);
            { const VUVector v = vf[8]; maddbc(s, vf[8], vf[24], op(v, 2)); }  // 0x0da | IBNE vi1 -> 0x0ef
            clip = vi[1] != 0;
            miniw(vf[9], vf[7], 1);
            cpw(vf[2], vf[1]);
            if (clip) {
                // 0x0ef
                vi[1] = fcor(s, 0xfdf7df);
                addbc(vf[9], vf[9], op(vf[4], 3), W);
                bool out = vi[1] != 0;
                vi[1] = fcor(s, 0xff7df7);
                if (!out) { out = vi[1] != 0; vi[1] = fcor(s, 0xffbefb); }
                if (!out) { out = vi[1] != 0; vi[1] = fcor(s, 0xffdf7d); }
                if (!out) { out = vi[1] != 0; vi[1] = fcor(s, 0xffefbe); }
                if (!out) out = vi[1] != 0;
                if (!out) vi[14] = 0xdc;
                clip = !out;
            }
            at = AT_0DC;
        }
        if (!clip && at == AT_0DC) {
            mulbc(vf[2], vf[1], s.q, XYZ);  // 0x0dc | IBNE vi13, vi0 -> 0x0be
            const bool more = vi[13] != 0;
            mulbc(vf[6], vf[5], s.q, XYZ);
            vi[7] = i16(vi[7] + 3);
            if (!more) break;
            at = AT_0BE;
            continue;
        }
        if (!clip) continue;
        // 0x0fc: the clipper, unless no vertex is left or fewer than two are stored
        const auto resume = [&] { at = vi[14] == 0xcc ? AT_0CC : AT_0DC; };  // JR vi14
        if (!(vi[13] > 0)) { resume(); continue; }
        vi[1] = i16(vi[7] - vi[15]);  // 0x100
        vi[1] = i16(vi[1] - 3);
        if (!(vi[1] > 0)) { resume(); continue; }
        // 0x107: the triangle (vf28, vf27, vf26) saved and tested against the sides x 3
        put(vf[3], 3, one * opf(s.q));
        s.i = 3.0f;
        // (host) the test drops a triangle wholly past one side of a third of the clip matrix's guard band: past 4 / 3 of the 4:3
        // frame's half width (the guard band 0x166d40 is 4 times it). A wider window (16:9, 32:9) shows past that: in a widened view
        // the factor divided by the view's widening (vu1::g_side_widen, the replica's walk), the sides of the view the window shows
        // (Aridia's sand sea near the camera at the far left of a 32:9 view: its clipped cells' triangles there dropped). The
        // clipping that follows stays at the game's guard band. The game's own view: 3 x 1, as the program.
        const float kx = s.i / vu1::g_side_widen[0], ky = s.i / vu1::g_side_widen[1];
        vf[14].u[2] = vf[28].u[2];
        vf[14].u[3] = vf[28].u[3];
        mulbc(vf[14], vf[28], kx, X);
        mulbc(vf[14], vf[28], ky, Y);
        vf[15].u[2] = vf[27].u[2];
        vf[15].u[3] = vf[27].u[3];
        mulbc(vf[15], vf[27], kx, X);
        mulbc(vf[15], vf[27], ky, Y);
        vf[16].u[2] = vf[26].u[2];
        vf[16].u[3] = vf[26].u[3];
        mulbc(vf[16], vf[26], kx, X);
        mulbc(vf[16], vf[26], ky, Y);
        isw(s, vi[6], 0x3b3, 0);
        isw(s, vi[7], 0x3b3, 1);
        clipw(s, vf[14]);
        isw(s, vi[15], 0x3b3, 2);
        clipw(s, vf[15]);
        sq(s, vf[3], 0x3b3, W);
        clipw(s, vf[16]);
        vi[15] = 0x3b4;
        vi[1] = fcor(s, 0xff7df7);  // 0x113
        bool gone = vi[1] != 0;
        vi[1] = fcor(s, 0xffbefb);
        if (!gone) { gone = vi[1] != 0; vi[1] = fcor(s, 0xffdf7d); }
        if (!gone) { gone = vi[1] != 0; vi[1] = fcor(s, 0xffefbe); }
        if (!gone) gone = vi[1] != 0;
        if (!gone) {
            // 0x11c: the polygon (clip position, ST / q, colour) at 0x3b4
            vi[7] = i16(vi[7] - 3);
            sq(s, vf[28], vi[15]);
            lq(s, vf[10], vi[7]);
            divq(s, vf[0], 3, vf[10], 2);
            mulbc(vf[10], vf[10], s.q, XYZ);
            sq(s, vf[10], vi[15] + 1);
            lq(s, vf[3], vi[7] + 1);
            itof0(vf[3]);
            sq(s, vf[3], vi[15] + 2);
            sq(s, vf[27], vi[15] + 3);
            lq(s, vf[10], vi[7] + 3);
            divq(s, vf[0], 3, vf[10], 2);
            mulbc(vf[10], vf[10], s.q, XYZ);
            sq(s, vf[10], vi[15] + 4);
            lq(s, vf[3], vi[7] + 4);
            itof0(vf[3]);
            sq(s, vf[3], vi[15] + 5);
            sq(s, vf[26], vi[15] + 6);
            lq(s, vf[10], vi[7] + 6);
            sq(s, vf[10], vi[15] + 7);
            lq(s, vf[3], vi[7] + 7);
            itof0(vf[3]);
            sq(s, vf[3], vi[15] + 8);
            vi[7] = 3;
            vi[9] = 0;
            vi[8] = i16(vi[15] + 0x19);
            vi[5] = vi[8];
            struct Plane {
                int c;
                bool neg;
                int16_t ret;
                uint32_t both, prev, cur;
            };
            static const Plane planes[5] = {{2, true, 0x155, 0xfff7df, 0xfff7ff, 0xffffdf}, {0, true, 0x17d, 0xffff7d, 0xffff7f, 0xfffffd},
                                            {0, false, 0x1a6, 0xffffbe, 0xffffbf, 0xfffffe}, {1, true, 0x1ce, 0xfffdf7, 0xfffdff, 0xfffff7},
                                            {1, false, 0x1f7, 0xfffefb, 0xfffeff, 0xfffffb}};
            bool empty = false;
            for (const Plane& p : planes) {
                clip_plane(s, p.c, p.neg, p.ret, p.both, p.prev, p.cur);
                // the output becomes the input (0x15a..0x160 and the like)
                vi[10] = i16(vi[9] - 3);
                vi[8] = vi[15];
                vi[15] = vi[5];  // (IBLTZ's delay slot)
                if (vi[10] < 0) { empty = true; break; }
                vi[5] = vi[8];
                vi[7] = vi[9];
                vi[9] = 0;
            }
            if (!empty) {
                // 0x203: the fan's GIF packet at 0x3e6
                vi[8] = 0x3e6;
                vi[11] = vi[8];
                sqi(s, vf[31], vi[8]);
                kick(static_cast<uint16_t>(vi[11]));  // 0x206 XGKICK (the tag: the PRIM)
                vi[11] = vi[8];
                vf[3].u[1] = vf[31].u[1];
                vf[3].u[2] = vf[31].u[2];
                vf[3].u[3] = vf[31].u[3];
                vi[10] = static_cast<int16_t>(vf[31].u[0] & 0xffff);
                vi[10] = i16(vi[10] + vi[7]);
                vf[3].u[0] = static_cast<uint32_t>(static_cast<int32_t>(vi[10]));
                sqi(s, vf[3], vi[8]);
                do {
                    lqi(s, vf[3], vi[15]);  // 0x20d
                    lqi(s, vf[14], vi[15]);
                    lqi(s, vf[15], vi[15]);
                    divq(s, vf[0], 3, vf[3], 3);
                    mulbc(vf[3], vf[3], s.q, XYZ);
                    mulbc(vf[14], vf[14], s.q, XYZ);
                    ftoi(vf[15], 1.0f);
                    mula(s, vf[3], vf[29]);
                    maddbc(s, vf[3], vf[30], one);
                    vi[7] = i16(vi[7] - 1);
                    miniw(vf[3], vf[7], 1);
                    maxw(vf[3], vf[7], 0);
                    ftoi(vf[3], 16.0f);
                    sqi(s, vf[14], vi[8]);
                    sqi(s, vf[15], vi[8]);
                    sqi(s, vf[3], vi[8]);  // (IBNE's delay slot)
                } while (vi[7] != 0);
                kick(static_cast<uint16_t>(vi[11]));  // 0x21d XGKICK
            }
        }
        // 0x21e: the clip flags and registers restored
        clipw(s, vf[28]);
        lq(s, vf[3], 0x3b3, W);
        clipw(s, vf[27]);
        divq(s, vf[3], 3, vf[0], 3);
        clipw(s, vf[26]);
        vi[6] = ilw(s, 0x3b3, 0);
        vi[7] = ilw(s, 0x3b3, 1);
        vi[15] = ilw(s, 0x3b3, 2);
        resume();
    }
    kick(static_cast<uint16_t>(vi[15]));  // 0x0de XGKICK; 0x0df [E], 0x0e0 nop
}