// The collision module (moved from ovl_1e_1f.cpp unchanged): mobys_in_sphere 0x1f2868, the mobys a sphere touches, with
// its test of a moby's primitive records.
#include <game/collision/collide_common.h>

namespace rac1 {
using namespace collide_detail;

namespace {
// the primitive records of a moby's collision data (0x20 bytes from col + 0x10, enabled unless
// the bit in moby +0x98 is set, selected by the query's set bit, the last one with +2 < 0):
// does the sphere (p, r; r2 = r * r) touch one? All in 1024ths, the moby at mp with rows R*, scale sc.
static bool moby_prims_hit(uint32_t m, uint32_t col, uint32_t flags, uint32_t s0, const L3& p, float r, float r2, const L3& mp,
                           const L3& R0, const L3& R1, const L3& R2, float sc) {
    if (lwu(col + 4) == 0) return false;
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
        L3 c;
        float w;
        if (type == 1) {  // sphere
            c = xf(muls(l3(rec + 0x10), sc));
            w = qmul(lwc1(rec + 0x1c), sc);
        } else if (type == 2) {  // sphere on a joint
            const uint32_t j = s0 + (lwu(rec + 4) << 4);
            c = xf(muls(add3(l3(j), l3(rec + 0x10)), sc));
            w = qmul(lwc1(rec + 0xc), sc);
        } else if (type == 3) {  // vertical capsule: z clamped to [base, base + height]
            c = xf(muls(l3(rec + 0x10), sc));
            w = qmul(lwc1(rec + 0x1c), sc);
            const float top = qadd(c.z, qmul(lwc1(rec + 4), sc));
            c.z = fminv(top, fmaxv(c.z, p.z));
        } else {  // capsule between two joints
            const L3 a = xf(muls(l3(s0 + (static_cast<uint32_t>(lh(rec + 4)) << 4)), sc));
            L3 ab = sub3(xf(muls(l3(s0 + (static_cast<uint32_t>(lh(rec + 6)) << 4)), sc)), a);
            const L3 ap = sub3(p, a);
            const float t = clamp01(qadd(0.0f, qdiv(hsum(mul3(ap, ab)), hsum(mul3(ab, ab)))));
            ab = muls(ab, t);
            c = add3(a, ab);
            w = qmul(lwc1(rec + 0xc), sc);
        }
        // sphere (c, w) against the query sphere
        float sr = qadd(r, w);
        const L3 dv = sub3(c, p);
        const float d2 = hsum(mul3(dv, dv));
        sr = qmul(sr, sr);
        const float q = qsqrt(d2);
        if (neg(qsub(sr, d2))) continue;
        float dist = qadd(0.0f, q);
        const float inv = qadd(0.0f, qdiv(1.0f, dist));
        dist = qmul(dist, 1.0f);
        const float reach = fminv(dist, w);
        c = add3(c, muls(muls(sub3(p, c), inv), reach));
        const L3 e = sub3(c, p);
        if (neg(qsub(r2, hsum(mul3(e, e))))) continue;
        return true;
    }
    return false;
}
}  // namespace

// Mobys touched by a sphere (pos, radius), hand-written VU0 macro code: the
// grid cells (0x19b7e0) under the sphere give moby ids; a moby counts once per query (+0x9c =
// query counter 0x173e50) when its bounding sphere (+0x00, w = radius) overlaps and it has
// collision data (+0x94): first its triangles (vertices posed by the moby's rows, outcodes,
// front faces unless flags & 0x10, types filtered by flags >> 8 when flags & 0x20, skipped with
// tri flag & 0x80 when flags & 0x80), then its primitive records (spheres, joint spheres,
// vertical capsules, joint segments; joint positions from moby_pose_matrices, cached in the
// 8-entry ring at *(0x173e48)). Hits are listed at 0x177f00 (0-terminated); with `rec`, mobys
// flagged 0x4000 also get a 64-byte contact record in the ring 0x178100. Returns the count.
// `ignore` is skipped. Coordinates must lie in [r, 1024 - r).
// orig: 0x001f2868
int32_t mobys_in_sphere_v(const Vec4& pos, float radius, uint32_t flags, uint32_t ignore, const uint32_t* rec) {
    const uint32_t SPR = 0x70000000u, OUTB = OV(0x00173e40), GRID = OV(0x0019b7e0), HITS = OV(0x00177f00);
    const int32_t q0 = lw(OUTB + 0x10);
    if (q0 < 0) return 0;
    const uint32_t query = static_cast<uint32_t>(q0 + 1);
    sw(OUTB + 0x10, query);
    const L3 p0 = L3{pos.x, pos.y, pos.z};
    const float r0 = radius;
    const L3 lo = L3{qsub(p0.x, r0), qsub(p0.y, r0), qsub(p0.z, r0)};
    const L3 hi = L3{qadd(p0.x, r0), qadd(p0.y, r0), qadd(p0.z, r0)};
    // MAC sign flags of lo - 0 and of hi - 1024
    if (neg(qsub(lo.x, 0.0f)) || neg(qsub(lo.y, 0.0f)) || neg(qsub(lo.z, 0.0f))) return 0;
    if (static_cast<int32_t>(fbits(radius)) <= 0) return 0;
    if (!(neg(qsub(hi.x, 1024.0f)) && neg(qsub(hi.y, 1024.0f)) && neg(qsub(hi.z, 1024.0f)))) return 0;
    const L3 p = muls(p0, 1024.0f);
    const float r = qmul(radius, 1024.0f);
    const float r2 = qmul(r, r);
    // the cells: [start, end) of u16 moby ids each, in a zero-terminated list on the scratchpad
    uint32_t cl = SPR + 0x3800;
    const uint32_t x0 = static_cast<uint32_t>(qftoi0(lo.x)) >> 4, y0 = static_cast<uint32_t>(qftoi0(lo.y)) >> 4;
    const uint32_t x1 = static_cast<uint32_t>(qftoi0(hi.x)) >> 4, y1 = static_cast<uint32_t>(qftoi0(hi.y)) >> 4;
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
    const uint32_t mobys = lwu(OV(0x00166c00u - 0x6c28));
    uint32_t hits = HITS;
    uint32_t s0 = SPR;             // joint positions: scratchpad or a cached buffer (kept across mobys)
    uint32_t a3 = GRID;            // $a3, stored in the cache ring (stale from earlier work)
    const L3 zero{0.0f, 0.0f, 0.0f};
    for (uint32_t cell = SPR + 0x3800;; cell += 8) {
        uint32_t id = lwu(cell);
        if (id == 0) break;
        const uint32_t cend = lwu(cell + 4);
        for (; id != cend; id += 2) {
            const uint32_t m = (static_cast<uint32_t>(lhu(id)) << 8) + mobys;
            if (m == ignore) continue;
            // bounding spheres
            const L3 c = l3(m);
            const float sr = qadd(r, lwc1(m + 0xc));
            const uint32_t col = lwu(m + 0x94);
            const L3 d = mul3(sub3(c, p), sub3(c, p));
            const float ov = qmadd(qmadd(qsub(d.x, qmul(sr, sr)), 1.0f, d.y), 1.0f, d.z);
            if (lwu(m + 0x9c) == query) continue;
            if (col == 0) continue;
            if (static_cast<int32_t>(fbits(ov)) > 0) continue;
            const uint32_t nj = lhu(col + ((flags & 4) == 0 ? 2u : 0u));
            const uint32_t key = lwu(m + 0xa8);
            sw(m + 0x9c, query);
            if (nj != 0) {
                const uint32_t ring = lwu(OUTB + 8);
                uint32_t buf = lwu(OUTB + 4);
                bool cached = false;
                for (uint32_t i = 0; i < 8; ++i) {
                    if (lwu(ring + i * 8) == key) {
                        cached = true;
                        s0 = buf + i * 0x800;
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
            bool hit = false;
            const uint32_t vbytes = lwu(col + 0xc);
            if (vbytes != 0) {
                // vertices, posed (scaled rows, relative to the moby), with outcodes
                const L3 S0 = muls(R0, sc), S1 = muls(R1, sc), S2 = muls(R2, sc);
                const L3 rel = sub3(p, mp);
                const L3 vlo = L3{qsub(rel.x, r), qsub(rel.y, r), qsub(rel.z, r)};
                const L3 vhi = L3{qadd(rel.x, r), qadd(rel.y, r), qadd(rel.z, r)};
                const uint32_t vbase = col + lwu(col + 4) + 0x10;
                uint32_t dst = SPR + 0x1fe0;
                for (int32_t left = static_cast<int32_t>(vbytes), k = 0; left > 0; left -= 0x10, ++k) {
                    dst += 0x20;
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
                        sq3(dst + h * 0x10, w, oc);
                        if (h == 0) a3 = neg(above.x) ? 2u : 0u;
                    }
                }
                // triangles: 4-byte vertex index triples + flags
                uint32_t tri = vbase + vbytes;
                const uint32_t tend = tri + lwu(col + 8);
                for (; tri != tend && !hit;) {
                    const uint32_t a0 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri)) * 16;
                    const uint32_t a1 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri + 1)) * 16;
                    const uint32_t a2 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri + 2)) * 16;
                    const uint32_t tf = lbu(tri + 3);
                    tri += 4;
                    a3 = lwu(a1 + 0xc);
                    if ((lwu(a0 + 0xc) & a3 & lwu(a2 + 0xc)) != 0) continue;
                    if ((flags & 0x80 & tf) != 0) continue;
                    const L3 v1 = l3(a0);
                    const L3 e2 = sub3(l3(a1), v1), e3 = sub3(l3(a2), v1);
                    L3 rp = sub3(sub3(p, mp), v1);
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
                        const L3 off = sub3(rp, sub3(sub3(p, mp), v1));
                        if (neg(qsub(r2, hsum(mul3(off, off))))) continue;
                    }
                    hit = true;
                }
            }
            if (!hit) hit = moby_prims_hit(m, col, flags, s0, p, r, r2, mp, R0, R1, R2, sc);
            if (hit) {
                sw(hits, m);
                hits += 4;
            }
        }
    }
    sw(hits, 0);
    sq4f(OUTB + 0x20, 0.0f, 0.0f, 0.0f, 1.0f);
    sw(OUTB + 0x1c, 0x3f);
    sw(OUTB + 0x18, lwu(HITS));
    if (rec != nullptr && hits != HITS) {
        constexpr OvConst<0x00178100u> CONTACTS{};
        uint32_t next = lwu(OUTB + 0x14);
        for (uint32_t h = HITS; h != hits; h += 4) {
            const uint32_t m = lwu(h);
            if ((lhu(m + 0x34) & 0x4000) == 0) continue;
            const int32_t old = static_cast<int8_t>(lbu(m + 0xa4));
            if (old >= 0) {
                const uint32_t o = CONTACTS + static_cast<uint32_t>(old) * 64;
                if (lwu(o + 0x34) == m && static_cast<int32_t>(lwu(o + 0x2c) - rec[7]) > 0) continue;
            }
            const uint32_t c = CONTACTS + next * 64;
            sq4f(c, 0.0f, 0.0f, 0.0f, 1.0f);
            sw(c + 0x38, 0);
            for (uint32_t w = 0; w < 9; ++w) sw(c + 0x10 + w * 4, rec[w]);
            sw(c + 0x34, m);
            sb(m + 0xa4, next);
            next = (next + 1) & 0x3f;
        }
        sw(OUTB + 0x14, next);
    }
    return static_cast<int32_t>((hits - HITS) >> 2);
}
// orig: 0x001f2868 (EE-address form)
int32_t mobys_in_sphere(uint32_t pos, float radius, uint32_t flags, uint32_t ignore, uint32_t rec) {
    return mobys_in_sphere_v(V(pos), radius, flags, ignore, rec != 0 ? static_cast<const uint32_t*>(ee_ram_ptr(rec)) : nullptr);
}

}  // namespace rac1
