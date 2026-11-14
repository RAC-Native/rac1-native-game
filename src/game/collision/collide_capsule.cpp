// The collision module (moved from ovl_1e_1f.cpp unchanged): capsule_collide 0x1f19a0, the nearest contact of an upright
// capsule with the level's collision and the mobys.
#include <game/collision/collide_common.h>

namespace rac1 {
using namespace collide_detail;

// Nearest contact of an upright capsule (pos at the bottom, radius, height) with the level
// collision and the mobys: sphere_collide with the triangle test done against the capsule's axis
// segment (the nearest axis point to the triangle's plane, or the plane crossing), the moby
// bounding test against the capsule's z range, the primitives against the nearest axis point (a
// joint segment projected in xy). Output as sphere_collide; the pushed-out position keeps the
// contact's height along the axis.
// orig: 0x001f19a0
int32_t capsule_collide_v(const Vec4& pos, float radius, float height, uint32_t flags, uint32_t ignore) {
    const uint32_t SPR = 0x70000000u, OUTB = OV(0x00173e40), GRID = OV(0x0019b7e0);
    const int32_t q0 = lw(OUTB + 0x10);
    if (q0 < 0) return 0;
    const uint32_t query = static_cast<uint32_t>(q0 + 1);
    const L3 p0 = L3{pos.x, pos.y, pos.z};
    L3 hi0 = L3{qadd(p0.x, radius), qadd(p0.y, radius), qadd(p0.z, radius)};
    const L3 lo0 = L3{qsub(p0.x, radius), qsub(p0.y, radius), qsub(p0.z, radius)};
    hi0.z = qadd(hi0.z, height);
    sw(OUTB + 0x10, query);
    const L3 p = muls(p0, 1024.0f);
    const float r = qmul(radius, 1024.0f), h = qmul(height, 1024.0f);
    float r2 = qmul(r, r);
    if (neg(qsub(lo0.x, 0.0f)) || neg(qsub(lo0.y, 0.0f)) || neg(qsub(lo0.z, 0.0f))) return 0;
    const uint32_t tree = lwu(OUTB);
    if (static_cast<int32_t>(fbits(radius)) <= 0) return 0;
    if (!(neg(qsub(hi0.x, 1024.0f)) && neg(qsub(hi0.y, 1024.0f)) && neg(qsub(hi0.z, 1024.0f)))) return 0;
    if (static_cast<int32_t>(fbits(height)) <= 0) return 0;
    const int32_t ilo[3] = {qftoi0(lo0.x), qftoi0(lo0.y), qftoi0(lo0.z)};
    const int32_t ihi[3] = {qftoi0(hi0.x), qftoi0(hi0.y), qftoi0(hi0.z)};
    const L3 lo = muls(lo0, 1024.0f), hi = muls(hi0, 1024.0f);
    const float nr2 = qsub(0.0f, r2);
    const float k999 = qmul(1.0f, u2f(0x3f7fdf3bu));
    r2 = qmul(r2, k999);
    const L3 zero{0.0f, 0.0f, 0.0f};
    uint32_t s5 = 0, s6 = 0, w2 = 0;
    float v5y = 0.0f;  // vf5.y: read by the moby bounding test when a height difference is exactly +0
    L3 v25 = g_vu.v25, v26 = g_vu.v26, v27 = g_vu.v27, v28 = g_vu.v28, v29 = g_vu.v29;
    uint32_t s0 = SPR, a3 = 0;
    auto tri_scan = [&](uint32_t tri, uint32_t tend, const L3& rel, const L3& org, uint32_t obj) {
        for (; tri != tend;) {
            const uint32_t a0 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri)) * 16;
            const uint32_t a1 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri + 1)) * 16;
            const uint32_t a2 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri + 2)) * 16;
            const uint32_t tf = lbu(tri + 3);
            tri += 4;
            a3 = lwu(a1 + 0xc);
            if ((lwu(a0 + 0xc) & a3 & lwu(a2 + 0xc)) != 0) continue;
            w2 = a3;
            if ((flags & 0x80 & tf) != 0) continue;
            const L3 v1 = l3(a0);
            const L3 e2 = sub3(l3(a1), v1), e3 = sub3(l3(a2), v1);
            const L3 bot = sub3(rel, v1);   // vf4
            L3 top = bot;                   // vf5
            v5y = top.y;
            a3 = ((flags >> 8) ^ tf) & 0x1f;
            top.z = qadd(top.z, h);
            if ((flags & 0x20) != 0 && a3 == 0) continue;
            const L3 n = opm(e3, e2);
            const float db = hsum(mul3(bot, n)), dt = hsum(mul3(top, n));
            const float hn = qmul(h, n.z);
            const uint32_t anz = fbits(n.z) & 0x7fffffffu;   // vabs.z
            const float z2 = qsub(bot.z, e2.z), z3 = qsub(bot.z, e3.z);
            if ((flags & 0x10) == 0) {
                a3 = fbits(dt);
                if (neg(u2f(fbits(db) & fbits(dt)))) continue;
            }
            const float tz2 = qsub(top.z, e2.z), tz3 = qsub(top.z, e3.z);
            L3 a13;         // vf13: the axis point
            float q;        // n scale to the plane
            if (static_cast<int32_t>(anz - 0x3f800000u) < 0) {
                // |n.z| < 1: the axis point at v1's height, clamped to the axis
                float z = qsub(0.0f, bot.z);
                z = fmaxv(fminv(z, h), 0.0f);
                a13 = L3{bot.x, bot.y, qadd(z, bot.z)};
                q = qdiv(hsum(mul3(a13, n)), hsum(mul3(n, n)));
            } else {
                const float qc = qdiv(db, hn);
                if (!neg(u2f(fbits(bot.z) | fbits(z2) | fbits(z3)))) {          // bottom above the triangle
                    q = qdiv(db, hsum(mul3(n, n)));
                    a13 = bot;
                } else if (neg(u2f(fbits(top.z) & fbits(tz2) & fbits(tz3)))) {  // top below it
                    q = qdiv(dt, hsum(mul3(n, n)));
                    a13 = top;
                } else {
                    // the axis crosses the plane at x: the nearest triangle point to it, its height on the axis
                    const float cz = qmul(h, qc);
                    L3 x = L3{bot.x, bot.y, qsub(bot.z, cz)};
                    top = x;
                    v5y = top.y;
                    const L3 t9 = sub3(e3, e2), t10 = sub3(x, e2);
                    const float s7 = hsum_y(mul3(opm(x, e2), n)), s8 = hsum_y(mul3(opm(e3, x), n)), s9 = hsum_y(mul3(opm(t10, t9), n));
                    float pz;
                    if (!neg(s7) && !neg(s8) && !neg(s9)) {
                        pz = x.z;
                    } else {
                        L3 ea{}, eb{};
                        if (neg(s7)) { eb = add3(e2, zero); }
                        else if (neg(s8)) { ea = add3(e3, zero); }
                        else { ea = add3(e2, zero); eb = add3(e3, zero); }
                        const L3 dd = sub3(x, ea);
                        eb = sub3(eb, ea);
                        const float t = clamp01(qadd(0.0f, qdiv(hsum(mul3(dd, eb)), hsum(mul3(eb, eb)))));
                        pz = madds(add3(zero, ea), eb, t).z;
                    }
                    float z = qsub(pz, bot.z);
                    z = fmaxv(fminv(z, h), 0.0f);
                    a13 = L3{bot.x, bot.y, qadd(z, bot.z)};
                    q = qdiv(hsum(mul3(a13, n)), hsum(mul3(n, n)));
                }
            }
            const L3 off = muls(n, q);
            L3 rp = sub3(a13, off);
            float d2 = hsum(mul3(off, off));
            a3 = 0;  // qmfc2 + dsll32 of r2 - d2
            const L3 t9v0 = sub3(e3, e2), t11 = sub3(rp, e2);
            const L3 t7 = opm(rp, e2), t8v = opm(e3, rp), t9v = opm(t11, t9v0);
            if (neg(qsub(r2, d2))) continue;
            const L3 m7 = mul3(t7, n);
            a3 = fbits(m7.x);
            const float s7 = hsum_y(m7), s8 = hsum_y(mul3(t8v, n)), s9 = hsum_y(mul3(t9v, n));
            if (neg(s7) || neg(s8) || neg(s9)) {
                L3 ea{}, eb{};
                if (neg(s7)) { eb = add3(e2, zero); }
                else if (neg(s8)) { ea = add3(e3, zero); }
                else { ea = add3(e2, zero); eb = add3(e3, zero); }
                const L3 dd = sub3(rp, ea);
                eb = sub3(eb, ea);
                const float t = clamp01(qadd(0.0f, qdiv(hsum(mul3(dd, eb)), hsum(mul3(eb, eb)))));
                rp = madds(add3(zero, ea), eb, t);
                const L3 e = sub3(rp, a13);
                d2 = hsum(mul3(e, e));
                if (neg(qsub(r2, d2))) continue;
            }
            s5 = obj;
            const L3 wv1 = add3(v1, org);
            s6 = tf + 0x1000;
            r2 = qmul(d2, k999);
            v26 = add3(n, zero);
            v25 = add3(rp, wv1);
            v27 = add3(zero, wv1);
            v28 = add3(e2, wv1);
            v29 = add3(e3, wv1);
        }
    };
    if ((flags & 1) == 0) {
        // the level: cells of the collision tree whose box reaches the sphere, as a list of
        // (centre, chunk) on the scratchpad; the first chunk is fetched right away
        sd(SPR, 0);
        uint32_t list = SPR + 0x3800;
        bool fetching = false;
        const int32_t c0[3] = {static_cast<int32_t>(static_cast<uint32_t>(ilo[0]) >> 2), static_cast<int32_t>(static_cast<uint32_t>(ilo[1]) >> 2), static_cast<int32_t>(static_cast<uint32_t>(ilo[2]) >> 2)};
        const int32_t c1[3] = {static_cast<int32_t>(static_cast<uint32_t>(ihi[0]) >> 2), static_cast<int32_t>(static_cast<uint32_t>(ihi[1]) >> 2), static_cast<int32_t>(static_cast<uint32_t>(ihi[2]) >> 2)};
        const L3 base = L3{static_cast<float>(c0[0] << 12), static_cast<float>(c0[1] << 12), static_cast<float>(c0[2] << 12)};
        L3 corner = base;
        for (int32_t cz = c0[2];; ++cz) {
            corner.y = base.y;
            for (int32_t cy = c0[1];; ++cy) {
                corner.x = base.x;
                for (int32_t cx = c0[0];; ++cx) {
                    const L3 far = L3{qadd(corner.x, 4096.0f), qadd(corner.y, 4096.0f), qadd(corner.z, 4096.0f)};
                    const L3 cl = L3{fminv(fmaxv(corner.x, p.x), far.x), fminv(fmaxv(corner.y, p.y), far.y), fminv(fmaxv(corner.z, p.z), far.z)};
                    const L3 ctr = L3{qadd(corner.x, 2048.0f), qadd(corner.y, 2048.0f), qadd(corner.z, 2048.0f)};
                    const L3 d = sub3(cl, p);
                    const L3 dq = mul3(d, d);
                    const float gap = qmadd(qmadd(qadd(nr2, dq.x), 1.0f, dq.y), 1.0f, dq.z);
                    if (neg(gap)) {
                        const uint32_t chunk = coll_tree_cell(tree, cx, cy, cz);
                        sq3(list, ctr, 0);
                        sw(list + 0xc, chunk);
                        if (chunk != 0) {
                            list += 0x10;
                            if (!fetching) {
                                fetching = true;
                                spr_dma_to(0, (chunk >> 8) + tree, chunk & 0xff);
                            }
                        }
                    }
                    corner.x = qadd(corner.x, 4096.0f);
                    if (cx == c1[0]) break;
                }
                corner.y = qadd(corner.y, 4096.0f);
                if (cy == c1[1]) break;
            }
            corner.z = qadd(corner.z, 4096.0f);
            if (cz == c1[2]) break;
        }
        sw(list + 0xc, 0);
        // the chunks: two buffers (SPR + 0 / + 0x1000), the next one fetched while this one is tested
        uint32_t buf = SPR + 0x1000;
        for (uint32_t e = SPR + 0x3800;; e += 0x10) {
            const uint32_t chunk = lwu(e + 0xc), next = lwu(e + 0x1c);
            const L3 org = l3(e);
            if (chunk == 0) break;
            buf ^= 0x1000;
            if (next != 0) {
                const uint32_t other = buf ^ 0x1000;
                sd(other, 0);
                spr_dma_to(other - SPR, (next >> 8) + tree, next & 0xff);
            }
            const uint32_t nv = lbu(buf + 2);
            const L3 rel = sub3(p, org);
            if (nv == 0) continue;
            const L3 rlo = sub3(lo, org), rhi = sub3(hi, org);
            const int16_t blo[3] = {h16(qftoi0(rlo.x)), h16(qftoi0(rlo.y)), h16(qftoi0(rlo.z))};
            const int16_t bhi[3] = {h16(qftoi0(rhi.x)), h16(qftoi0(rhi.y)), h16(qftoi0(rhi.z))};
            uint32_t src = buf + 4, dst = SPR + 0x1fe0;
            for (int32_t left = static_cast<int32_t>(nv); left > 0; left -= 2) {
                dst += 0x20;
                for (uint32_t hh = 0; hh < 2; ++hh, src += 4) {
                    const uint32_t pv = lwu(src);
                    const int16_t c[3] = {h16(static_cast<int32_t>((pv & 0x3ff) << 6)), h16(static_cast<int32_t>(((pv >> 10) & 0x3ff) << 6)), h16(static_cast<int32_t>((pv >> 20) << 4))};
                    uint32_t oc = 0;
                    for (uint32_t a = 0; a < 3; ++a) {
                        if ((static_cast<uint16_t>(c[a] - blo[a]) & 0x8000u) != 0) oc |= 1u << (a * 8);
                        if ((static_cast<uint16_t>(bhi[a] - c[a]) & 0x8000u) != 0) oc |= 2u << (a * 8);
                    }
                    const L3 vf = L3{static_cast<float>(c[0]), static_cast<float>(c[1]), static_cast<float>(c[2])};
                    sq3(dst + hh * 0x10, vf, oc);
                }
                w2 = 0;
            }
            const uint32_t t0 = buf + 4 + nv * 4;
            const uint32_t tend = t0 + static_cast<uint32_t>(lbu(buf)) * 4;
            tri_scan(t0, tend, rel, org, tree);
            // quads: the first lbu(+3) triangles again as (a, c, extra vertex)
            const uint32_t nq = lbu(buf + 3);
            if (nq == 0) continue;
            for (uint32_t i = 0; i < nq; ++i) {
                const uint32_t t = t0 + i * 4;
                const uint32_t c = lbu(t + 2), x = lbu(tend + i);
                sb(t + 1, c);
                sb(t + 2, x);
            }
            tri_scan(t0, t0 + nq * 4, rel, org, tree);
        }
    }
    // the mobys
    s5 = 0;
    {
        const uint32_t x0 = static_cast<uint32_t>(qftoi0(lo.x)) >> 14, y0 = static_cast<uint32_t>(qftoi0(lo.y)) >> 14;
        const uint32_t x1 = static_cast<uint32_t>(qftoi0(hi.x)) >> 14, y1 = static_cast<uint32_t>(qftoi0(hi.y)) >> 14;
        uint32_t cl = SPR + 0x3800;
        for (uint32_t y = y0;; ++y) {
            for (uint32_t x = x0;; ++x) {
                const uint32_t c = GRID + (x << 2) + (y << 8);
                const uint32_t start = (static_cast<uint32_t>(lhu(c)) << 5) + GRID + 0x4000;
                sw(cl, start);
                sw(cl + 4, start + static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(c + 2))) * 2));
                cl += 8;
                if (x == x1) break;
            }
            if (y == y1) break;
        }
        sd(cl, 0);
    }
    const uint32_t mobys = lwu(OV(0x00166c00u - 0x6c28));
    a3 = GRID;
    for (uint32_t cell = SPR + 0x3800;; cell += 8) {
        uint32_t id = lwu(cell);
        if (id == 0) break;
        const uint32_t cend = lwu(cell + 4);
        for (; id != cend; id += 2) {
            const uint32_t m = (static_cast<uint32_t>(lhu(id)) << 8) + mobys;
            if (m == ignore) continue;
            // bounding sphere against the capsule: xy distance, then the z range
            const L3 c = l3(m);
            const float sr = qadd(r, lwc1(m + 0xc));
            const uint32_t col = lwu(m + 0x94);
            const float below = qsub(qsub(qsub(c.z, sr), p.z), h);   // vf4.z: sphere bottom - capsule top
            const float above = qsub(qadd(c.z, sr), p.z);             // vf5.z: sphere top - capsule bottom
            const float dx = qsub(c.x, p.x), dy = qsub(c.y, p.y);
            const float ov = qmadd(qsub(qmul(dx, dx), qmul(sr, sr)), 1.0f, qmul(dy, dy));
            if (lwu(m + 0x9c) == query) continue;
            if (col == 0) continue;
            if (static_cast<int32_t>(fbits(ov)) > 0) continue;
            const int64_t up = static_cast<int64_t>((static_cast<uint64_t>(fbits(above)) << 32) | fbits(v5y));
            if (up <= 0) continue;
            if (!neg(below)) continue;
            const uint32_t nj = lhu(col + ((flags & 4) == 0 ? 2u : 0u));
            const uint32_t vbytes = lwu(col + 0xc);
            sw(m + 0x9c, query);
            if (nj != 0) {
                if (((flags & 2) == 0 ? 1u : 0u) == vbytes) continue;
                const uint32_t key = lwu(m + 0xa8);
                const uint32_t ring = lwu(OUTB + 8);
                const uint32_t bufs = lwu(OUTB + 4);
                bool cached = false;
                for (uint32_t i = 0; i < 8; ++i) {
                    if (lwu(ring + i * 8) == key) {
                        cached = true;
                        s0 = bufs + i * 0x800;
                        break;
                    }
                }
                if (!cached) {
                    const uint32_t slot = lwu(OUTB + 0xc);
                    sw(OUTB + 0xc, (slot + 1) & 7);
                    sw(ring + slot * 8, key);
                    sw(ring + slot * 8 + 4, a3);
                    a3 = moby_pose_matrices(m, static_cast<int32_t>(nj), static_cast<int32_t>(lwu(OUTB + 4) + (slot << 11)), m, cell);
                    s0 = SPR;
                }
            }
            const L3 mp = muls(l3(m + 0x10), 1024.0f);
            const L3 R0 = l3(m + 0xc0), R1 = l3(m + 0xd0), R2 = l3(m + 0xe0);
            const float sc = lwc1(m + 0x2c);
            if (vbytes != 0) {
                const L3 S0 = muls(R0, sc), S1 = muls(R1, sc), S2 = muls(R2, sc);
                v5y = S2.y;
                const L3 rel = sub3(p, mp);
                L3 vhi = L3{qadd(rel.x, r), qadd(rel.y, r), qadd(rel.z, r)};
                const L3 vlo = L3{qsub(rel.x, r), qsub(rel.y, r), qsub(rel.z, r)};
                vhi.z = qadd(vhi.z, h);
                const uint32_t vbase = col + lwu(col + 4) + 0x10;
                uint32_t dst = SPR + 0x1fe0;
                int32_t k = 0;
                for (int32_t left = static_cast<int32_t>(vbytes); left > 0; left -= 0x10, ++k) {
                    dst += 0x20;
                    for (uint32_t hh = 0; hh < 2; ++hh) {
                        const uint32_t va = vbase + static_cast<uint32_t>(k) * 0x10 + hh * 8;
                        const L3 v = L3{static_cast<float>(lh(va)), static_cast<float>(lh(va + 2)), static_cast<float>(lh(va + 4))};
                        const L3 w = madds(madds(muls(S0, v.x), S1, v.y), S2, v.z);
                        const L3 bl = sub3(w, vlo), ab = sub3(vhi, w);
                        uint32_t oc = 0;
                        if (neg(bl.x)) oc |= 1u;
                        if (neg(ab.x)) oc |= 2u;
                        if (neg(bl.y)) oc |= 1u << 8;
                        if (neg(ab.y)) oc |= 2u << 8;
                        if (neg(bl.z)) oc |= 1u << 16;
                        if (neg(ab.z)) oc |= 2u << 16;
                        sq3(dst + hh * 0x10, w, oc);
                        if (hh == 0) a3 = neg(ab.x) ? 2u : 0u;
                    }
                }
                w2 = static_cast<uint32_t>(static_cast<int32_t>(lh(vbase + static_cast<uint32_t>(k) * 0x10 + 0xe)));
                const uint32_t t0 = vbase + vbytes;
                tri_scan(t0, t0 + lwu(col + 8), rel, mp, m);
            }
            // primitives, nearest kept: against the nearest point of the capsule axis
            if ((flags & 2) != 0 || lwu(col + 4) == 0) continue;
            auto xf = [&](const L3& v) { return madds(madds(madds(muls(R0, v.x), R1, v.y), R2, v.z), mp, 1.0f); };
            const int32_t want = (flags & 4) == 0 ? 2 : 1;
            uint32_t mask = lwu(m + 0x98);
            int32_t more = 0;
            for (uint32_t rec = col + 0x10; more >= 0; rec += 0x20) {
                const uint32_t off = mask & 1;
                mask >>= 1;
                more = lh(rec + 2);
                const int32_t type = static_cast<int8_t>(lbu(rec));
                if (off != 0 || (want & more) == 0) continue;
                L3 pc;
                float pw;
                if (type == 1) {
                    pc = xf(muls(l3(rec + 0x10), sc));
                    pw = qmul(lwc1(rec + 0x1c), sc);
                } else if (type == 2) {
                    w2 = lwu(rec + 0xc);
                    const uint32_t j = s0 + (lwu(rec + 4) << 4);
                    pc = xf(muls(add3(l3(j), l3(rec + 0x10)), sc));
                    pw = qmul(lwc1(rec + 0xc), sc);
                } else if (type == 3) {
                    w2 = lwu(rec + 0xc);
                    pc = xf(muls(l3(rec + 0x10), sc));
                    pw = qmul(lwc1(rec + 0x1c), sc);
                    const float top = qadd(pc.z, qmul(lwc1(rec + 4), sc));
                    pc.z = fminv(top, fmaxv(pc.z, p.z));
                } else {
                    // joint segment: nearest point to the axis in xy (a short one: its z range clamped)
                    const uint32_t jb = s0 + (static_cast<uint32_t>(lh(rec + 6)) << 4);
                    w2 = lwu(jb + 0xc);
                    const L3 a = xf(muls(l3(s0 + (static_cast<uint32_t>(lh(rec + 4)) << 4)), sc));
                    const L3 ab = sub3(xf(muls(l3(jb), sc)), a);
                    const L3 ap = sub3(p, a);
                    const float num = qadd(qmul(ap.x, ab.x), qmul(ap.y, ab.y));
                    const float den = qadd(qmul(ab.x, ab.x), qmul(ab.y, ab.y));
                    const float q = qdiv(num, den);
                    a3 = 0;  // qmfc2 + dsll32 of 1 - |den|
                    if (!neg(qsub(1.0f, u2f(fbits(den) & 0x7fffffffu)))) {
                        const L3 b = add3(ab, a);
                        pc = a;
                        pc.z = fminv(b.z, fmaxv(a.z, p.z));
                    } else {
                        const float t = clamp01(qadd(0.0f, q));
                        pc = madds(add3(zero, a), ab, t);
                    }
                    pw = qmul(lwc1(rec + 0xc), sc);
                }
                // sphere (pc, pw) against the axis point at its height
                float az = qsub(pc.z, p.z);
                az = fmaxv(fminv(az, h), 0.0f);
                const L3 axis = L3{p.x, p.y, qadd(p.z, az)};
                float psr = qadd(r, pw);
                const L3 dv = sub3(pc, axis);
                const float d2 = hsum(mul3(dv, dv));
                psr = qmul(psr, psr);
                const float q = qsqrt(d2);
                if (neg(qsub(psr, d2))) continue;
                float dist = qadd(0.0f, q);
                const float inv = qadd(0.0f, qdiv(1.0f, dist));
                dist = qmul(dist, 1.0f);
                const float reach = fminv(dist, pw);
                L3 push = sub3(axis, pc);
                v5y = push.y;
                push = muls(push, inv);
                v5y = push.y;
                push = muls(push, reach);
                v5y = push.y;
                pc = add3(pc, push);
                const L3 e = sub3(pc, axis);
                const float d2b = hsum(mul3(e, e));
                if (neg(qsub(r2, d2b))) continue;
                s5 = m;
                s6 = 0u - rec;
                r2 = qmul(d2b, 1.0f);
                v25 = add3(zero, pc);
                v26 = add3(zero, push);
            }
        }
    }
    if (s6 == 0) return 0;
    const float q = qrsqrt(r, r2);
    L3 push = sub3(p, v25);
    L3 back = sub3(v25, p);
    const float k = u2f(0x3a800000u);
    v25 = muls(v25, k);
    v27 = muls(v27, k);
    v28 = muls(v28, k);
    v29 = muls(v29, k);
    sq3(OUTB + 0x20, v25, 0);
    sq3(OUTB + 0x40, v26, 0);
    sq3(OUTB + 0x50, v27, 0);
    sq3(OUTB + 0x60, v28, 0);
    sq3(OUTB + 0x70, v29, 0);
    sw(OUTB + 0x18, s5);
    sw(OUTB + 0x1c, s6);
    back.z = fmaxv(fminv(back.z, h), 0.0f);
    push.z = qadd(push.z, back.z);
    push = muls(push, q);
    push.z = qsub(push.z, back.z);
    push = add3(muls(push, k), v25);
    sq3(OUTB + 0x30, push, w2);
    g_vu = VuContact{v25, v26, v27, v28, v29};
    return 1;
}
// orig: 0x001f19a0 (EE-address form)
int32_t capsule_collide(uint32_t pos, float radius, float height, uint32_t flags, uint32_t ignore) {
    return capsule_collide_v(V(pos), radius, height, flags, ignore);
}

}  // namespace rac1
