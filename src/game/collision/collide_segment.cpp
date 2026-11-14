// The collision module (moved from ovl_1e_1f.cpp unchanged): segment_collide 0x1efc70, the nearest hit of a segment with
// the level's collision and the mobys, and its cache of the mobys' collision meshes (RAC1_PERF_COLL_MESH).
#include <game/collision/collide_common.h>
#include <cstdio>
#include <vector>

namespace rac1 {
using namespace collide_detail;

namespace {
// (host) RAC1_PERF_COLL_MESH (default off: about 2 us saved a use, a few uses a frame on the PC; 1 on, 2 on and the reuse printed
// every 5000 uses): a moby's collision mesh turned into the world by segment_collide_v (each vertex: the rows scaled, three VU0
// multiply-adds) is a function of the rows', the scale's bits and the vertex words alone: the result kept per moby with those bits
// and a copy of the words, and used again while all of them are the same (most mobys do not move; floor probes and sound lines
// test the same ones every frame) - the same values. The outcodes, which depend on the segment, are still made per call.
struct MeshCacheEntry {
    uint32_t m = 0, vbase = 0, nb = 0;
    uint32_t key[10] = {};
    std::vector<uint32_t> raw;
    std::vector<L3> w;
};
struct MeshCache {
    static constexpr uint32_t kN = 128;
    MeshCacheEntry e[kN];
};
int mesh_cache_mode() {
    static const int on = std::getenv("RAC1_PERF_COLL_MESH") != nullptr ? std::atoi(std::getenv("RAC1_PERF_COLL_MESH")) : 0;
    return on;
}
MeshCache& mesh_cache() {
    static thread_local MeshCache c{};
    return c;
}
}  // namespace

// Nearest hit of the segment start -> end with the level collision and the mobys (hand-written VU0
// macro code). The 4-unit cells the segment passes are listed in order (per-axis crossings at
// SPR + 0x3000/0x3200/0x3400 merged into (t, packed cell) at SPR + 0x3800, ending with t = 1);
// unless flags & 1 their chunks are fetched (double-buffered) and their triangles hit where the
// segment crosses them (front faces unless flags & 0x10), nearest t kept in vf24. Then the mobys
// of the grid cells along the segment (only those flagged 0x4000 when flags & 9): bounding sphere
// near the segment, triangles as above, primitives (spheres, joint spheres, vertical cylinders,
// joint capsules: the entry point). Writes the hit to 0x173e40 as sphere_collide (no pushed-out
// position) and returns 1; a moby hit flagged 0x4000 also gets a contact record (with `rec`).
// orig: 0x001efc70
int32_t segment_collide_v(const Vec4& start, const Vec4& end, uint32_t flags, uint32_t ignore, const uint32_t* rec) {
    const uint32_t SPR = 0x70000000u, OUTB = OV(0x00173e40), GRID = OV(0x0019b7e0);
    const int32_t q0 = lw(OUTB + 0x10);
    const L3 s0f = L3{start.x, start.y, start.z}, e0f = L3{end.x, end.y, end.z};
    const L3 mn = L3{fminv(s0f.x, e0f.x), fminv(s0f.y, e0f.y), fminv(s0f.z, e0f.z)};
    const L3 mx = L3{fmaxv(s0f.x, e0f.x), fmaxv(s0f.y, e0f.y), fmaxv(s0f.z, e0f.z)};
    const L3 ps = muls(s0f, 1024.0f), pe = muls(e0f, 1024.0f);   // vf30 / vf31
    // vf28 / vf29 are the contact registers (g_vu) the other routines store from: this one loads the end points into them
    // (lqc2 at 0x1efc74 / 0x1efc7c) and converts them in place (vftoi0 at 0x1efd08 / 0x1efd20), before its first exits, so
    // every return leaves them so (a later contact that sets only vf25 / vf26 stores these, not an older contact's)
    const int32_t is[3] = {qftoi0(ps.x), qftoi0(ps.y), qftoi0(ps.z)};
    const int32_t ie[3] = {qftoi0(pe.x), qftoi0(pe.y), qftoi0(pe.z)};
    const L3 is_f = L3{u2f(static_cast<uint32_t>(is[0])), u2f(static_cast<uint32_t>(is[1])), u2f(static_cast<uint32_t>(is[2]))};
    const L3 ie_f = L3{u2f(static_cast<uint32_t>(ie[0])), u2f(static_cast<uint32_t>(ie[1])), u2f(static_cast<uint32_t>(ie[2]))};
    g_vu.v28 = s0f;
    g_vu.v29 = e0f;
    if (q0 < 0) return 0;
    const uint32_t query = static_cast<uint32_t>(q0 + 1);
    sw(OUTB + 0x10, query);
    g_vu.v28 = is_f;
    if (neg(qsub(mn.x, 0.0f)) || neg(qsub(mn.y, 0.0f)) || neg(qsub(mn.z, 0.0f))) return 0;
    const uint32_t tree = lwu(OUTB);
    g_vu.v29 = ie_f;
    if (!(neg(qsub(mx.x, 1024.0f)) && neg(qsub(mx.y, 1024.0f)) && neg(qsub(mx.z, 1024.0f)))) return 0;
    const L3 seg = sub3(pe, ps);   // vf13
    const L3 zero{0.0f, 0.0f, 0.0f};
    uint32_t s5 = 0, s6 = 0;
    // vf25..vf27 from the previous contact; vf28 / vf29 still hold the end points as integers
    L3 v25 = g_vu.v25, v26 = g_vu.v26, v27 = g_vu.v27;
    L3 v28 = is_f;
    L3 v29 = ie_f;
    float best = 1.0f;   // vf24.x
    const uint32_t c0[3] = {static_cast<uint32_t>(is[0]) >> 12, static_cast<uint32_t>(is[1]) >> 12, static_cast<uint32_t>(is[2]) >> 12};
    const uint32_t c1[3] = {static_cast<uint32_t>(ie[0]) >> 12, static_cast<uint32_t>(ie[1]) >> 12, static_cast<uint32_t>(ie[2]) >> 12};
    uint32_t cellw = (c0[0] & 0xff) | (c0[1] & 0xff) << 8 | (c0[2] & 0xff) << 16;
    sw(SPR + 0x3800, 0);
    sw(SPR + 0x3804, cellw);
    if ((flags & 1) == 0) {
        const uint32_t chunk = coll_tree_cell(tree, static_cast<int32_t>(c0[0] & 0xffff), static_cast<int32_t>(c0[1]), static_cast<int32_t>(c0[2]));
        sd(SPR, 0);
        if (chunk != 0) spr_dma_to(0, (chunk >> 8) + tree, chunk & 0xff);
    }
    uint32_t list = SPR + 0x3800;
    if (c0[0] != c1[0] || c0[1] != c1[1] || c0[2] != c1[2]) {
        // crossings per axis: the t of each cell boundary passed, clamped to [0, 1]
        int32_t total = 0;
        const float sv[3] = {seg.x, seg.y, seg.z}, st[3] = {ps.x, ps.y, ps.z};
        for (uint32_t ax = 0; ax < 3; ++ax) {
            const float q = qdiv(1.0f, sv[ax]);
            int32_t a = static_cast<int32_t>(c0[ax]), b = static_cast<int32_t>(c1[ax]), d = 1;
            if (b - a < 0) {
                ++a;
                ++b;
                d = -1;
            }
            uint32_t out = SPR + 0x3000 + ax * 0x200;
            if (a != b) {
                for (a += d;; a += d) {
                    float t = qsub(static_cast<float>(a << 12), st[ax]);
                    t = qmul(t, q);
                    ++total;
                    int32_t tb = static_cast<int32_t>(fbits(t));
                    tb = std::max(std::min(tb, 0x3f800000), 0);
                    sw(out, static_cast<uint32_t>(tb));
                    out += 4;
                    if (a == b) break;
                }
            }
            sw(out, 0x50000000u);
            sb(SPR + 0x3ffc + ax, static_cast<uint32_t>(d));
        }
        // merged, nearest first: each step moves the packed cell along its axis
        const uint32_t dx = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(SPR + 0x3ffc))));
        const uint32_t dy = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(SPR + 0x3ffd)))) << 8;
        const uint32_t dz = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(SPR + 0x3ffe)))) << 16;
        uint32_t px = SPR + 0x3000, py = SPR + 0x3200, pz = SPR + 0x3400;
        for (; total > 0; --total) {
            const int32_t tx = lw(px), ty = lw(py), tz = lw(pz);
            list += 8;
            if (ty - tx < 0) {
                if (tz - ty < 0) {
                    sw(list, static_cast<uint32_t>(tz));
                    cellw += dz;
                    pz += 4;
                } else {
                    sw(list, static_cast<uint32_t>(ty));
                    cellw += dy;
                    py += 4;
                }
            } else if (tz - tx < 0) {
                sw(list, static_cast<uint32_t>(tz));
                cellw += dz;
                pz += 4;
            } else {
                sw(list, static_cast<uint32_t>(tx));
                cellw += dx;
                px += 4;
            }
            sw(list + 4, cellw);
        }
    } else {
        // one cell: nothing to do when the end points coincide
        const int32_t d0 = is[0] - ie[0], d1 = is[1] - ie[1], d2 = is[2] - ie[2];
        if ((static_cast<uint64_t>(static_cast<uint32_t>(d0 | d1)) | (static_cast<uint64_t>(static_cast<uint32_t>(d1 | d2)) << 32)) == 0) return 0;
    }
    sd(list + 8, 0x3f800000u);
    // triangles [tri, tend) over SPR + 0x2000 (relative to org; the segment relative: a, b)
    auto tri_scan = [&](uint32_t tri, uint32_t tend, const L3& a, const L3& b, const L3& org, uint32_t obj) {
        for (; tri != tend;) {
            const uint32_t a0 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri)) * 16;
            const uint32_t a1 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri + 1)) * 16;
            const uint32_t a2 = SPR + 0x2000 + static_cast<uint32_t>(lbu(tri + 2)) * 16;
            const uint32_t tf = lbu(tri + 3);
            tri += 4;
            if ((lwu(a0 + 0xc) & lwu(a1 + 0xc) & lwu(a2 + 0xc)) != 0) continue;
            if ((flags & 0x80 & tf) != 0) continue;
            // (RAC1_PERF_VU_CLAMP: the vertices clamped once as they are read; seg, a, b, best are VU results, org a cell's integer
            // coordinates: every operand below clean)
            const L3 v1 = cl3(a0);
            const L3 e2 = csub3(cl3(a1), v1), e3 = csub3(cl3(a2), v1);
            if ((flags & 0x20) != 0 && (((flags >> 8) ^ tf) & 0x1f) == 0) continue;
            const L3 n = copm(e3, e2);
            const float sn = chsum(cmul3(seg, n));
            const float ds = chsum(cmul3(csub3(v1, a), n)), de = chsum(cmul3(csub3(v1, b), n));
            const float q = qdiv(ds, sn);
            if ((flags & 0x10) == 0 && (!neg(ds) || static_cast<int32_t>(fbits(de)) <= 0)) continue;
            if (!neg(u2f(fbits(ds) ^ fbits(de)))) continue;
            const L3 hp = cmadds(cmuls(csub3(a, v1), 1.0f), seg, q);
            const L3 wv1 = cadd3(v1, org);
            const float s7 = chsum(cmul3(copm(hp, e2), n)), s8 = chsum(cmul3(copm(e3, hp), n));
            const float s9 = chsum(cmul3(copm(csub3(hp, e2), csub3(e3, e2)), n));
            const float gain = csub(best, q);
            if (neg(s7) || neg(s8) || neg(s9) || static_cast<int32_t>(fbits(gain)) <= 0) continue;
            s6 = tf + 0x1000;
            s5 = obj;
            best = qadd(0.0f, q);
            v25 = add3(hp, wv1);
            v26 = add3(n, zero);
            v27 = add3(zero, wv1);
            v28 = add3(e2, wv1);
            v29 = add3(e3, wv1);
        }
    };
    if ((flags & 1) == 0) {
        uint32_t buf = SPR + 0x1000;
        for (uint32_t e = SPR + 0x3800;; e += 8) {
            const uint32_t t = lwu(e), tnext = lwu(e + 8), cw = lwu(e + 4);
            if (t == 0x3f800000u) break;
            if (s6 != 0 && !(static_cast<int64_t>(static_cast<uint64_t>(fbits(best)) << 32) - static_cast<int64_t>(static_cast<uint64_t>(t) << 32) > 0)) break;
            const uint32_t ncw = lwu(e + 0xc);
            buf ^= 0x1000;
            if (tnext != 0x3f800000u) {
                const uint32_t chunk = coll_tree_cell(tree, static_cast<int32_t>(ncw & 0xff), static_cast<int32_t>((ncw >> 8) & 0xff), static_cast<int32_t>(ncw >> 16));
                const uint32_t other = buf ^ 0x1000;
                sd(other, 0);
                if (chunk != 0) spr_dma_to(other - SPR, (chunk >> 8) + tree, chunk & 0xff);
            }
            const uint32_t nv = lbu(buf + 2);
            const L3 pin = madds(muls(seg, u2f(t)), ps, 1.0f), pout = madds(muls(seg, u2f(tnext)), ps, 1.0f);
            if (nv == 0) continue;
            const int32_t ci[3] = {static_cast<int32_t>(((cw & 0xff) << 12) + 0x800), static_cast<int32_t>((((cw >> 8) & 0xff) << 12) + 0x800), static_cast<int32_t>((((cw >> 16) & 0xff) << 12) + 0x800)};
            const L3 org = L3{static_cast<float>(ci[0]), static_cast<float>(ci[1]), static_cast<float>(ci[2])};
            const int32_t ri[3] = {qftoi0(pin.x) - ci[0], qftoi0(pin.y) - ci[1], qftoi0(pin.z) - ci[2]};
            const int32_t ro[3] = {qftoi0(pout.x) - ci[0], qftoi0(pout.y) - ci[1], qftoi0(pout.z) - ci[2]};
            int16_t blo[3], bhi[3];
            for (uint32_t k = 0; k < 3; ++k) {
                blo[k] = h16(std::min(ri[k], ro[k]) - 1);
                bhi[k] = h16(std::max(ri[k], ro[k]) + 1);
            }
            const L3 a = sub3(ps, org), b = sub3(pe, org);
            uint32_t src = buf + 4, dst = SPR + 0x1fe0;
            for (int32_t left = static_cast<int32_t>(nv); left > 0; left -= 2) {
                dst += 0x20;
                for (uint32_t hh = 0; hh < 2; ++hh, src += 4) {
                    const uint32_t pv = lwu(src);
                    const int16_t c[3] = {h16(static_cast<int32_t>((pv & 0x3ff) << 6)), h16(static_cast<int32_t>(((pv >> 10) & 0x3ff) << 6)), h16(static_cast<int32_t>((pv >> 20) << 4))};
                    uint32_t oc = 0;
                    for (uint32_t k = 0; k < 3; ++k) {
                        if ((static_cast<uint16_t>(c[k] - blo[k]) & 0x8000u) != 0) oc |= 1u << (k * 8);
                        if ((static_cast<uint16_t>(bhi[k] - c[k]) & 0x8000u) != 0) oc |= 2u << (k * 8);
                    }
                    const L3 vf = L3{static_cast<float>(c[0]), static_cast<float>(c[1]), static_cast<float>(c[2])};
                    sq3(dst + hh * 0x10, vf, oc);
                }
            }
            const uint32_t t0 = buf + 4 + nv * 4;
            const uint32_t tend = t0 + static_cast<uint32_t>(lbu(buf)) * 4;
            tri_scan(t0, tend, a, b, org, tree);
            const uint32_t nq = lbu(buf + 3);
            if (nq == 0) continue;
            for (uint32_t i = 0; i < nq; ++i) {
                const uint32_t tt = t0 + i * 4;
                const uint32_t c = lbu(tt + 2), x = lbu(tend + i);
                sb(tt + 1, c);
                sb(tt + 2, x);
            }
            tri_scan(t0, t0 + nq * 4, a, b, org, tree);
        }
    }
    // the mobys: grid cells (16 units) along the segment, from the cells list
    s5 = 0;
    const float len2 = hsum(mul3(seg, seg));
    const float inv2 = qdiv(1.0f, len2);
    {
        auto gcell = [](uint32_t cw) {
            const uint32_t x = (cw & 0xff) >> 2, y = ((cw >> 8) & 0xff) >> 2;
            return x | y << 16;
        };
        uint32_t cur = gcell(lhu(SPR + 0x3804));
        sw(SPR + 0x3800, cur);
        uint32_t out = SPR + 0x3804;
        for (uint32_t e = SPR + 0x3808; lwu(e) != 0x3f800000u; e += 8) {
            const uint32_t g = gcell(lhu(e + 4));
            if (g == cur) continue;
            sw(out, g);
            cur = g;
            out += 4;
        }
        sw(out, 0xffffffffu);
    }
    const L3 sdir = muls(seg, inv2);   // vf21
    const uint32_t mobys = lwu(OV(0x00166c00u - 0x6c28));
    uint32_t s0 = SPR;
    for (uint32_t ce = SPR + 0x3800;; ce += 4) {
        const uint32_t gv = lwu(ce);
        if (static_cast<int32_t>(gv) < 0) break;
        const uint32_t c = GRID + (gv >> 8) + ((gv & 0xffff) << 2);
        uint32_t id = (static_cast<uint32_t>(lhu(c)) << 5) + GRID + 0x4000;
        const uint32_t cend = id + static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(c + 2))) * 2);
        sw(ce, cend);
        for (; id != cend; id += 2) {
            const uint32_t m = (static_cast<uint32_t>(lhu(id)) << 8) + mobys;
            if (m == ignore) continue;
            const L3 cm = l3(m);
            const float rr = qmul(lwc1(m + 0xc), lwc1(m + 0xc));
            const uint32_t col = lwu(m + 0x94);
            if ((flags & 9) != 0 && (lhu(m + 0x34) & 0x4000) == 0) continue;
            // nearest point of the segment (up to the best hit) to the bounding sphere
            float t = hsum(mul3(sub3(cm, ps), sdir));
            t = fminv(fmaxv(t, 0.0f), best);
            const L3 np = sub3(madds(add3(zero, ps), seg, t), cm);
            const L3 nq = mul3(np, np);
            const float ov = qmadd(qmadd(qsub(nq.x, rr), 1.0f, nq.y), 1.0f, nq.z);
            if (col == 0) continue;
            if (static_cast<int32_t>(fbits(ov)) > 0) continue;
            const uint32_t prev = lwu(m + 0x9c);
            sw(m + 0x9c, query);
            if (prev == query) continue;
            const uint32_t nj = lhu(col + ((flags & 4) == 0 ? 2u : 0u));
            const uint32_t vbytes = lwu(col + 0xc);
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
                    sw(ring + slot * 8 + 4, query);
                    moby_pose_matrices(m, static_cast<int32_t>(nj), static_cast<int32_t>(lwu(OUTB + 4) + (slot << 11)), m, ce);
                    s0 = SPR;
                }
            }
            const L3 mp = muls(l3(m + 0x10), 1024.0f);
            const L3 R0 = l3(m + 0xc0), R1 = l3(m + 0xd0), R2 = l3(m + 0xe0);
            const float sc = lwc1(m + 0x2c);
            if (vbytes != 0) {
                const L3 a = sub3(ps, mp), b = sub3(pe, mp);
                const L3 S0 = muls(R0, sc), S1 = muls(R1, sc), S2 = muls(R2, sc);
                const L3 vlo = L3{fminv(a.x, b.x), fminv(a.y, b.y), fminv(a.z, b.z)};
                const L3 vhi = L3{fmaxv(a.x, b.x), fmaxv(a.y, b.y), fmaxv(a.z, b.z)};
                const uint32_t vbase = col + lwu(col + 4) + 0x10;
                uint32_t dst = SPR + 0x1fe0;
                int32_t k = 0;
                const uint32_t nb = (vbytes + 0xf) & ~0xfu;   // the bytes the loop below reads (vbytes as a count > 0)
                const MeshCacheEntry* ce = nullptr;
                if (mesh_cache_mode() != 0 && static_cast<int32_t>(vbytes) > 0 && nb <= 0x1000 && (vbase & 3) == 0) {
                    const uint32_t key[10] = {fbits(R0.x), fbits(R0.y), fbits(R0.z), fbits(R1.x), fbits(R1.y), fbits(R1.z),
                                              fbits(R2.x), fbits(R2.y), fbits(R2.z), fbits(sc)};
                    const void* rp = ee_ram_ptr_read(vbase);
                    MeshCacheEntry& e = mesh_cache().e[(m >> 8) & (MeshCache::kN - 1)];
                    static const bool stats = mesh_cache_mode() == 2;
                    static thread_local uint32_t n_call = 0, n_miss = 0;
                    if (stats && ++n_call % 5000 == 0) std::fprintf(stderr, "[meshcache] %u uses, %u made\n", n_call, n_miss);
                    if (!(e.m == m && e.vbase == vbase && e.nb == nb && std::memcmp(e.key, key, sizeof key) == 0 &&
                          std::memcmp(e.raw.data(), rp, nb) == 0)) {
                        ++n_miss;
                        e.m = m;
                        e.vbase = vbase;
                        e.nb = nb;
                        std::memcpy(e.key, key, sizeof key);
                        e.raw.resize(nb / 4);
                        std::memcpy(e.raw.data(), rp, nb);
                        e.w.resize(nb / 8);
                        for (uint32_t i = 0; i < nb / 8; ++i) {
                            const uint32_t va = vbase + i * 8;
                            const L3 v = L3{static_cast<float>(lh(va)), static_cast<float>(lh(va + 2)), static_cast<float>(lh(va + 4))};
                            e.w[i] = madds(madds(muls(S0, v.x), S1, v.y), S2, v.z);
                        }
                    }
                    ce = &e;
                }
                if (ce != nullptr) {
                    for (uint32_t i = 0; i < nb / 8; ++i) {
                        if ((i & 1) == 0) dst += 0x20;
                        const L3& w = ce->w[i];
                        const L3 bl = sub3(w, vlo), ab = sub3(vhi, w);
                        uint32_t oc = 0;
                        if (neg(bl.x)) oc |= 1u;
                        if (neg(ab.x)) oc |= 2u;
                        if (neg(bl.y)) oc |= 1u << 8;
                        if (neg(ab.y)) oc |= 2u << 8;
                        if (neg(bl.z)) oc |= 1u << 16;
                        if (neg(ab.z)) oc |= 2u << 16;
                        sq3(dst + (i & 1) * 0x10, w, oc);
                    }
                } else
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
                    }
                }
                const uint32_t t0 = vbase + vbytes;
                tri_scan(t0, t0 + lwu(col + 8), a, b, mp, m);
            }
            if ((flags & 2) != 0 || lwu(col + 4) == 0) continue;
            auto rot = [&](const L3& v) { return madds(madds(muls(R0, v.x), R1, v.y), R2, v.z); };
            const int32_t want = (flags & 4) == 0 ? 2 : 1;
            uint32_t mask = lwu(m + 0x98);
            int32_t more = 0;
            for (uint32_t pr = col + 0x10; more >= 0; pr += 0x20) {
                const uint32_t off = mask & 1;
                mask >>= 1;
                more = lh(pr + 2);
                const int32_t type = static_cast<int8_t>(lbu(pr));
                if (off != 0 || (want & more) == 0) continue;
                L3 pc;
                float pw;
                if (type == 1) {
                    pc = add3(rot(muls(l3(pr + 0x10), sc)), mp);
                    pw = qmul(lwc1(pr + 0x1c), sc);
                } else if (type == 2) {
                    const uint32_t j = s0 + (lwu(pr + 4) << 4);
                    pc = add3(rot(muls(add3(l3(j), l3(pr + 0x10)), sc)), mp);
                    pw = qmul(lwc1(pr + 0xc), sc);
                } else if (type == 3) {
                    // vertical cylinder: the segment's entry (in xy) clamps the centre's height
                    pc = add3(rot(muls(l3(pr + 0x10), sc)), mp);
                    pw = qmul(lwc1(pr + 0x1c), sc);
                    const L3 d = seg, cs = sub3(pc, ps);
                    float a2 = qadd(qmul(d.x, d.x), qmul(d.y, d.y));
                    float b2 = qadd(qmul(cs.x, d.x), qmul(cs.y, d.y));
                    float c2 = qadd(qmul(cs.x, cs.x), qmul(cs.y, cs.y));
                    a2 = fmaxv(a2, u2f(0x3c800000u));
                    const float rr2 = qmul(pw, pw);
                    float tt = clamp01(qadd(0.0f, qdiv(b2, a2)));
                    const float nx = qsub(qmadd(qadd(0.0f, ps.x), d.x, tt), pc.x), ny = qsub(qmadd(qadd(0.0f, ps.y), d.y, tt), pc.y);
                    const float ovc = qmadd(qsub(qmul(nx, nx), rr2), 1.0f, qmul(ny, ny));
                    if (static_cast<int32_t>(fbits(ovc)) > 0) continue;
                    b2 = qadd(b2, b2);
                    c2 = qsub(c2, rr2);
                    float disc = qsub(qmul(b2, b2), qmul(qmul(4.0f, a2), c2));
                    disc = fmaxv(disc, 0.0f);
                    const float sq = qsqrt(disc);
                    b2 = qsub(0.0f, b2);
                    a2 = qadd(a2, a2);
                    const float h = qmul(lwc1(pr + 4), sc);
                    b2 = qadd(b2, sq);
                    tt = clamp01(qsub(0.0f, qdiv(b2, a2)));
                    const L3 ent = madds(add3(zero, ps), d, tt);
                    const float top = qadd(pc.z, h);
                    pc.z = fminv(fmaxv(pc.z, ent.z), top);
                } else {
                    // capsule between two joints: its axis point nearest to the segment's entry
                    const L3 a = rot(muls(l3(s0 + (static_cast<uint32_t>(lh(pr + 4)) << 4)), sc));
                    L3 ax = sub3(rot(muls(l3(s0 + (static_cast<uint32_t>(lh(pr + 6)) << 4)), sc)), a);
                    pw = qmul(lwc1(pr + 0xc), sc);
                    const float rr2 = qmul(qadd(0.0f, pw), qadd(0.0f, pw));
                    const L3 wa = add3(a, mp);
                    const L3 cr = opm(ax, seg), crn = opm(seg, ax);
                    const L3 c2 = mul3(cr, cr);
                    const L3 m7 = opm(seg, cr), m8 = opm(ax, crn);
                    L3 ent = ps;
                    const L3 par = sub3(c2, L3{1.0f, 1.0f, 1.0f});
                    if (!(neg(par.x) && neg(par.y) && neg(par.z))) {
                        const float cc = hsum(c2);
                        const float k7 = hsum(mul3(m7, sub3(ps, wa))), k8 = hsum(mul3(m8, sub3(wa, ps)));
                        const L3 on_ax = madds(add3(zero, wa), ax, qdiv(k7, cc));
                        const L3 on_seg = madds(add3(zero, ps), seg, qdiv(k8, cc));
                        const float al = hsum(mul3(ax, ax)), sl = hsum(mul3(seg, seg));
                        const L3 dd = sub3(on_seg, on_ax);
                        const L3 dq = mul3(dd, dd);
                        const float gap = qmadd(qmadd(qadd(qsub(0.0f, rr2), dq.x), 1.0f, dq.y), 1.0f, dq.z);
                        if (!neg(gap)) continue;
                        const L3 ah = muls(ax, qrsqrt(1.0f, al));
                        L3 sh = muls(seg, qrsqrt(1.0f, sl));
                        float cs = hsum(mul3(ah, sh));
                        cs = qmul(cs, cs);
                        cs = qsub(1.0f, cs);
                        float back = qadd(0.0f, qdiv(qsub(0.0f, gap), cs));
                        back = fmaxv(back, 0.0f);
                        sh = muls(sh, qsqrt(back));
                        ent = sub3(on_seg, sh);
                        if (neg(hsum(mul3(sub3(ent, ps), seg)))) ent = ps;
                    }
                    const L3 rel = sub3(ent, wa);
                    const float tt = clamp01(qadd(0.0f, qdiv(hsum(mul3(rel, ax)), hsum(mul3(ax, ax)))));
                    ax = muls(ax, tt);
                    pc = add3(wa, ax);
                }
                // sphere (pc, pw) against the segment: the entry t
                const L3 d = seg, cs = sub3(pc, ps);
                float a2 = hsum(mul3(d, d)), b2 = hsum(mul3(cs, d)), c2 = hsum(mul3(cs, cs));
                const float rr2 = qmul(pw, pw);
                float tt = clamp01(qadd(0.0f, qdiv(b2, a2)));
                const L3 nn = sub3(madds(add3(zero, ps), d, tt), pc);
                const L3 nq2 = mul3(nn, nn);
                const float ovs = qmadd(qmadd(qsub(nq2.x, rr2), 1.0f, nq2.y), 1.0f, nq2.z);
                if (static_cast<int32_t>(fbits(ovs)) > 0) continue;
                b2 = qadd(b2, b2);
                c2 = qsub(c2, rr2);
                float disc = qsub(qmul(b2, b2), qmul(qmul(4.0f, a2), c2));
                disc = fmaxv(disc, 0.0f);
                const float sq = qsqrt(disc);
                b2 = qsub(0.0f, b2);
                a2 = qadd(a2, a2);
                b2 = qadd(b2, sq);
                tt = clamp01(qsub(0.0f, qdiv(b2, a2)));
                const L3 hit = madds(add3(zero, ps), d, tt);
                const L3 nrm = sub3(hit, pc);
                if (static_cast<int32_t>(fbits(qsub(best, tt))) <= 0) continue;
                s5 = m;
                s6 = 0u - pr;
                best = qadd(0.0f, tt);
                v25 = add3(zero, hit);
                v26 = add3(zero, nrm);
            }
        }
    }
    if (s6 == 0) {
        g_vu = VuContact{v25, v26, v27, v28, v29};  // (no contact: vf25..vf29 as the scan left them)
        return 0;
    }
    // Known leftovers (#145, batch 530's notes): the original stores vf25 .. vf29 whole (sqc2), so the w lanes of +0x20,
    // +0x40 and +0x50 (and +0xc of the contact record below) are whatever VU0 code last left there, and on a moby's
    // primitive hit (no triangle hit in this call) +0x50 .. +0x58 is vf27 as earlier VU0 code left it; the port has 0 / its
    // own last contact. Only the rockets (Veldin's 0x2b03a0 and its level copies, class 256) read +0x50 / +0x60, and only
    // after a world hit, a triangle's vertices on both sides; these words are left out of the family checks for that.
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
    g_vu = VuContact{v25, v26, v27, v28, v29};
    if (s5 == 0 || rec == nullptr || (lhu(s5 + 0x34) & 0x4000) == 0) return 1;
    const uint32_t next = lwu(OUTB + 0x14);
    const int32_t old = static_cast<int8_t>(lbu(s5 + 0xa4));
    if (old >= 0) {
        const uint32_t o = OV(0x00178100) + static_cast<uint32_t>(old) * 64;
        if (lwu(o + 0x34) == s5 && static_cast<int32_t>(lwu(o + 0x2c) - rec[7]) > 0) return 1;
    }
    sw(OUTB + 0x14, (next + 1) & 0x3f);
    sb(s5 + 0xa4, next);
    const uint32_t c = OV(0x00178100) + next * 64;
    uint32_t rw[9];
    for (uint32_t w = 0; w < 9; ++w) rw[w] = rec[w];
    sq3(c, v25, 0);
    for (uint32_t w = 0; w < 9; ++w) sw(c + 0x10 + w * 4, rw[w]);
    sw(c + 0x34, s5);
    const uint32_t col = lwu(s5 + 0x94);
    const int32_t at = static_cast<int32_t>(s6) >= 0 ? -0x20 : static_cast<int32_t>((0u - s6) - col);
    sw(c + 0x38, static_cast<uint32_t>(at >> 5));
    return 1;
}
// orig: 0x001efc70 (EE-address form)
int32_t segment_collide(uint32_t start, uint32_t end, uint32_t flags, uint32_t ignore, uint32_t rec) {
    return segment_collide_v(V(start), V(end), flags, ignore, rec != 0 ? static_cast<const uint32_t*>(ee_ram_ptr(rec)) : nullptr);
}

}  // namespace rac1
