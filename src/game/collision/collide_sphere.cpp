// The collision module (moved from ovl_1e_1f.cpp unchanged): sphere_collide 0x1f0d60, the nearest contact of a sphere
// with the level's collision and the mobys.
#include <game/collision/collide_common.h>
#include <game/collision/collide_hit.h>
#include <game/bridge/guard.h>

namespace rac1 {
using namespace collide_detail;

// Nearest contact of a sphere (pos, radius) with the level collision and the mobys, hand-written
// VU0 macro code. Level (unless flags & 1): the 4-unit cells of the tree *(0x173e40) near the sphere
// are fetched to the scratchpad by DMA (double-buffered); packed vertices (10/10/12 bits) get
// outcodes, then the triangles (and each chunk's quads as second triangles) are tested as in
// mobys_in_sphere, each hit shrinking the radius (x 0.999). Mobys: as mobys_in_sphere, without
// their primitives when flags & 2. Writes the nearest contact to 0x173e40 (+0x18 moby or 0, +0x1c
// 0x1000 | triangle flags or minus the primitive record, +0x20 point, +0x30 pushed-out position,
// +0x40 normal / push, +0x50.. triangle: the previous contact's after a primitive) and returns 1, or
// 0 without a contact. The w lanes of vf25..vf29 (never written) are stored as 0; the w of +0x30
// is the last vf2.w this call loaded (0 if none: the register's older content is not modelled).
// The original's chunk buffers (0x70000000 / 0x70001000), cell list (0x70003800) and vertices (0x70002000) are not kept:
// the chunks are copied into two stack buffers (8 KB), the vertices are on the stack (4 KB), the cells walked directly
// (stage 5, task C2b); the joint positions moby_pose_matrices leaves at 0x70000000 are still read from there.
// orig: 0x001f0d60
int32_t sphere_collide_v(const Vec4& pos, float radius, uint32_t flags, uint32_t ignore) {
    const SprScope spr_scope(SprScopeId::SphereCollide);  // (RAC1_BRIDGE_GUARD=spr: the scratchpad's data flow, src/game/bridge/guard.h)
    if (__builtin_expect(coll_jointless_on(), false)) coll_jointless_scan_level();
    const uint32_t SPR = 0x70000000u, OUTB = OV(0x00173e40), GRID = OV(0x0019b7e0);
    const int32_t q0 = lw(OUTB + 0x10);
    if (q0 < 0) return 0;
    const uint32_t query = static_cast<uint32_t>(q0 + 1);
    const L3 p0 = L3{pos.x, pos.y, pos.z};
    const L3 lo0 = L3{qsub(p0.x, radius), qsub(p0.y, radius), qsub(p0.z, radius)};
    const L3 hi0 = L3{qadd(p0.x, radius), qadd(p0.y, radius), qadd(p0.z, radius)};
    sw(OUTB + 0x10, query);
    const L3 p = muls(p0, 1024.0f);
    const float r = qmul(radius, 1024.0f);
    float r2 = qmul(r, r);
    if (neg(qsub(lo0.x, 0.0f)) || neg(qsub(lo0.y, 0.0f)) || neg(qsub(lo0.z, 0.0f))) return 0;
    const uint32_t tree = lwu(OUTB);
    if (static_cast<int32_t>(fbits(radius)) <= 0) return 0;
    if (!(neg(qsub(hi0.x, 1024.0f)) && neg(qsub(hi0.y, 1024.0f)) && neg(qsub(hi0.z, 1024.0f)))) return 0;
    const int32_t ilo[3] = {qftoi0(lo0.x), qftoi0(lo0.y), qftoi0(lo0.z)};
    const int32_t ihi[3] = {qftoi0(hi0.x), qftoi0(hi0.y), qftoi0(hi0.z)};
    const L3 lo = muls(lo0, 1024.0f), hi = muls(hi0, 1024.0f);
    const float nr2 = qsub(0.0f, r2);
    const float k999 = qmul(1.0f, u2f(0x3f7fdf3bu));
    r2 = qmul(r2, k999);
    const L3 zero{0.0f, 0.0f, 0.0f};
    // the nearest contact so far (w2: vf2.w, the pushed-out position's w +0x3c; where this call loads no vf2 the original's is
    // a leftover of earlier VU0 code, here 0. Callers copy that quad into a moby's position (enemy_physics: its w +0x1c), and
    // from there it reaches what is spawned at that place (class 28's shards); class 28's update with the turrets' w lanes
    // dealt at random reads them nowhere else (#145): a harmless difference)
    uint32_t s5 = 0, s6 = 0, w2 = 0;
    L3 v25 = g_vu.v25, v26 = g_vu.v26, v27 = g_vu.v27, v28 = g_vu.v28, v29 = g_vu.v29;
    uint32_t s0 = SPR, a3 = 0;
    CollVerts verts{};  // (kept across chunks and mobys, as the original's scratchpad words)
    CollChunkBufs bufs{};  // the level's chunks (0x70000000 / 0x70001000 in the original)
    // the joint positions a primitive record reads through s0: while s0 is still the scratchpad and no moby_pose_matrices of
    // this query ran (a moby without joints in the selected set), the original reads the chunk buffers' words there
    bool posed = false;
    auto jword = [&](uint32_t a) { return !posed ? bufs.spr_word(a) : lwu(a); };
    auto jl3 = [&](uint32_t a) { return L3{u2f(jword(a)), u2f(jword(a + 4)), u2f(jword(a + 8))}; };
    // triangles [tri, tend) (tri_word(t): the word of bytes a, b, c, flags) over `verts` (relative to `org`, query `rel`)
    auto tri_scan = [&](uint32_t tri, uint32_t tend, auto&& tri_word, const L3& rel, const L3& org, uint32_t obj) {
        for (; tri != tend;) {
            const uint32_t tw = tri_word(tri);
            const CollVerts::Vert& c0 = verts[tw & 0xff];
            const CollVerts::Vert& c1 = verts[(tw >> 8) & 0xff];
            const CollVerts::Vert& c2 = verts[(tw >> 16) & 0xff];
            const uint32_t tf = tw >> 24;
            tri += 4;
            a3 = c1.oc;
            if ((c0.oc & a3 & c2.oc) != 0) continue;
            w2 = a3;
            if ((flags & 0x80 & tf) != 0) continue;
            const L3 v1 = c0.p;
            const L3 e2 = sub3(c1.p, v1), e3 = sub3(c2.p, v1);
            L3 rp = sub3(rel, v1);
            a3 = ((flags >> 8) ^ tf) & 0x1f;
            if ((flags & 0x20) != 0 && a3 == 0) continue;
            const L3 n = opm(e3, e2);
            const float dn = hsum(mul3(rp, n));
            const float q = qdiv(dn, hsum(mul3(n, n)));
            a3 = 0;
            if ((flags & 0x10) == 0 && neg(dn)) continue;
            const L3 acc = add3(zero, rp);
            L3 t7 = muls(n, q);
            rp = madds(acc, sub3(zero, n), q);
            t7 = mul3(t7, t7);
            L3 t9v = sub3(e3, e2);
            const L3 t10 = sub3(rp, e2);
            float d2 = hsum(t7);
            t7 = opm(rp, e2);
            L3 t8v = opm(e3, rp);
            if (neg(qsub(r2, d2))) continue;
            t9v = opm(t10, t9v);
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
                const L3 off = sub3(rp, sub3(rel, v1));
                d2 = hsum(mul3(off, off));
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
        // the level: the cells of the collision tree whose box reaches the sphere, their chunks in two buffers
        const int32_t c0[3] = {static_cast<int32_t>(static_cast<uint32_t>(ilo[0]) >> 2), static_cast<int32_t>(static_cast<uint32_t>(ilo[1]) >> 2), static_cast<int32_t>(static_cast<uint32_t>(ilo[2]) >> 2)};
        const int32_t c1[3] = {static_cast<int32_t>(static_cast<uint32_t>(ihi[0]) >> 2), static_cast<int32_t>(static_cast<uint32_t>(ihi[1]) >> 2), static_cast<int32_t>(static_cast<uint32_t>(ihi[2]) >> 2)};
        CollTreeSrc cells(tree);  // (the collision scene's or EE memory's: collision_scene.h)
        coll_tree_chunks(cells, p, nr2, c0, c1, bufs, [&](const L3& org, uint32_t b) {
            const L3 rel = sub3(p, org);
            if (bufs.byte(b, 2) == 0) return;
            const L3 rlo = sub3(lo, org), rhi = sub3(hi, org);
            const int16_t blo[3] = {h16(qftoi0(rlo.x)), h16(qftoi0(rlo.y)), h16(qftoi0(rlo.z))};
            const int16_t bhi[3] = {h16(qftoi0(rhi.x)), h16(qftoi0(rhi.y)), h16(qftoi0(rhi.z))};
            const uint32_t nv = coll_chunk_vertices(bufs, b, blo, bhi, verts);
            w2 = 0;  // (each vertex pair: vf2.w cleared)
            auto word = [&bufs, b](uint32_t t) { return bufs.word(b, t); };
            const uint32_t t0 = 4 + nv * 4;
            const uint32_t tend = t0 + bufs.byte(b, 0) * 4;
            tri_scan(t0, tend, word, rel, org, tree);
            // quads: the first byte(+3) triangles again as (a, c, extra vertex)
            const uint32_t nq = bufs.byte(b, 3);
            if (nq == 0) return;
            coll_chunk_quads(bufs, b, t0, tend, nq);
            tri_scan(t0, t0 + nq * 4, word, rel, org, tree);
        });
    }
    // the mobys: the grid's cells under the sphere, row by row (the original lists them first: see coll_grid_cell)
    s5 = 0;
    const uint32_t x0 = static_cast<uint32_t>(qftoi0(lo.x)) >> 14, y0 = static_cast<uint32_t>(qftoi0(lo.y)) >> 14;
    const uint32_t x1 = static_cast<uint32_t>(qftoi0(hi.x)) >> 14, y1 = static_cast<uint32_t>(qftoi0(hi.y)) >> 14;
    const uint32_t mobys = lwu(OV(0x00166c00u - 0x6c28));
    a3 = GRID;
    uint32_t ci = 0;  // the cell's index in the original's list
    for (uint32_t y = y0;; ++y) {
        for (uint32_t x = x0;; ++x) {
            const CollGridCell cell = coll_grid_cell(GRID, x, y);
            for (uint32_t id = cell.start; id != cell.end; id += 2) {
                const uint32_t m = (static_cast<uint32_t>(lhu(id)) << 8) + mobys;
                if (m == ignore) continue;
                const L3 c = l3(m);
                const float sr = qadd(r, lwc1(m + 0xc));
                const uint32_t col = lwu(m + 0x94);
                const L3 d = mul3(sub3(c, p), sub3(c, p));
                const float ov = qmadd(qmadd(qsub(d.x, qmul(sr, sr)), 1.0f, d.y), 1.0f, d.z);
                if (lwu(m + 0x9c) == query) continue;
                if (col == 0) continue;
                if (static_cast<int32_t>(fbits(ov)) > 0) continue;
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
                        a3 = moby_pose_matrices(m, static_cast<int32_t>(nj), static_cast<int32_t>(lwu(OUTB + 4) + (slot << 11)), m, coll_grid_cursor(ci));
                        s0 = SPR;
                        posed = true;
                    }
                }
                const L3 mp = muls(l3(m + 0x10), 1024.0f);
                const L3 R0 = l3(m + 0xc0), R1 = l3(m + 0xd0), R2 = l3(m + 0xe0);
                const float sc = lwc1(m + 0x2c);
                if (vbytes != 0) {
                    const L3 S0 = muls(R0, sc), S1 = muls(R1, sc), S2 = muls(R2, sc);
                    const L3 rel = sub3(p, mp);
                    const L3 vlo = L3{qsub(rel.x, r), qsub(rel.y, r), qsub(rel.z, r)};
                    const L3 vhi = L3{qadd(rel.x, r), qadd(rel.y, r), qadd(rel.z, r)};
                    const uint32_t vbase = col + lwu(col + 4) + 0x10;
                    int32_t k = 0;
                    for (int32_t left = static_cast<int32_t>(vbytes); left > 0; left -= 0x10, ++k) {
                        for (uint32_t h = 0; h < 2; ++h) {
                            const uint32_t va = vbase + static_cast<uint32_t>(k) * 0x10 + h * 8;
                            const L3 v = L3{static_cast<float>(lh(va)), static_cast<float>(lh(va + 2)), static_cast<float>(lh(va + 4))};
                            const L3 w = madds(madds(muls(S0, v.x), S1, v.y), S2, v.z);
                            const L3 below = sub3(w, vlo), above = sub3(vhi, w);
                            uint32_t oc = 0;
                            if (neg(below.x)) oc |= 1u;
                            if (neg(above.x)) oc |= 2u;
                            if (neg(below.y)) oc |= 1u << 8;
                            if (neg(above.y)) oc |= 2u << 8;
                            if (neg(below.z)) oc |= 1u << 16;
                            if (neg(above.z)) oc |= 2u << 16;
                            verts.put(static_cast<uint32_t>(k) * 2 + h, w, oc);
                            if (h == 0) a3 = neg(above.x) ? 2u : 0u;
                        }
                    }
                    w2 = static_cast<uint32_t>(static_cast<int32_t>(lh(vbase + static_cast<uint32_t>(k) * 0x10 + 0xe)));  // prefetched vertex
                    const uint32_t t0 = vbase + vbytes;
                    tri_scan(t0, t0 + lwu(col + 8), coll_tri_word_ee, rel, mp, m);
                }
                // primitives, nearest kept
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
                    if (__builtin_expect(coll_jointless_on(), false) && s0 == SPR && !posed && type != 1 && type != 3) coll_jointless_read("sphere_collide", m, col, type, more, flags);
                    L3 pc;
                    float pw;
                    if (type == 1) {
                        pc = xf(muls(l3(rec + 0x10), sc));
                        pw = qmul(lwc1(rec + 0x1c), sc);
                    } else if (type == 2) {
                        w2 = lwu(rec + 0xc);
                        const uint32_t j = s0 + (lwu(rec + 4) << 4);
                        pc = xf(muls(add3(jl3(j), l3(rec + 0x10)), sc));
                        pw = qmul(lwc1(rec + 0xc), sc);
                    } else if (type == 3) {
                        w2 = lwu(rec + 0xc);
                        pc = xf(muls(l3(rec + 0x10), sc));
                        pw = qmul(lwc1(rec + 0x1c), sc);
                        const float top = qadd(pc.z, qmul(lwc1(rec + 4), sc));
                        pc.z = fminv(top, fmaxv(pc.z, p.z));
                    } else {
                        const uint32_t jb = s0 + (static_cast<uint32_t>(lh(rec + 6)) << 4);
                        w2 = jword(jb + 0xc);
                        const L3 a = xf(muls(jl3(s0 + (static_cast<uint32_t>(lh(rec + 4)) << 4)), sc));
                        L3 ab = sub3(xf(muls(jl3(jb), sc)), a);
                        const L3 ap = sub3(p, a);
                        const float t = clamp01(qadd(0.0f, qdiv(hsum(mul3(ap, ab)), hsum(mul3(ab, ab)))));
                        ab = muls(ab, t);
                        pc = add3(a, ab);
                        pw = qmul(lwc1(rec + 0xc), sc);
                    }
                    float psr = qadd(r, pw);
                    const L3 dv = sub3(pc, p);
                    const float d2 = hsum(mul3(dv, dv));
                    psr = qmul(psr, psr);
                    const float q = qsqrt(d2);
                    if (neg(qsub(psr, d2))) continue;
                    float dist = qadd(0.0f, q);
                    const float inv = qadd(0.0f, qdiv(1.0f, dist));
                    dist = qmul(dist, 1.0f);
                    const float reach = fminv(dist, pw);
                    const L3 push = muls(muls(sub3(p, pc), inv), reach);
                    pc = add3(pc, push);
                    const L3 e = sub3(pc, p);
                    const float d2b = hsum(mul3(e, e));
                    if (neg(qsub(r2, d2b))) continue;
                    s5 = m;
                    s6 = 0u - rec;
                    r2 = qmul(d2b, 1.0f);
                    v25 = add3(zero, pc);
                    v26 = add3(zero, push);
                }
            }
            ++ci;
            if (x == x1) break;
        }
        if (y == y1) break;
    }
    if (s6 == 0) return 0;
    const float q = qrsqrt(r, r2);
    L3 push = sub3(p, v25);
    const float k = u2f(0x3a800000u);
    v25 = muls(v25, k);
    v27 = muls(v27, k);
    v28 = muls(v28, k);
    v29 = muls(v29, k);
    push = add3(muls(muls(push, q), k), v25);
    CollisionHit out;
    out.moby = s5;
    out.flags = s6;
    out.point = hit_vec(v25, 0);
    out.pushed = hit_vec(push, w2);
    out.normal = hit_vec(v26, 0);
    out.tri[0] = hit_vec(v27, 0);
    out.tri[1] = hit_vec(v28, 0);
    out.tri[2] = hit_vec(v29, 0);
    coll_hit_mirror(out);
    g_vu = VuContact{v25, v26, v27, v28, v29};
    return 1;
}
// orig: 0x001f0d60 (EE-address form)
int32_t sphere_collide(uint32_t pos, float radius, uint32_t flags, uint32_t ignore) { return sphere_collide_v(V(pos), radius, flags, ignore); }

}  // namespace rac1
