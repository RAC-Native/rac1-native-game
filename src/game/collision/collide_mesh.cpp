// The collision module (moved from ovl_1e_1f.cpp unchanged): the VU0 contact registers every collision routine keeps its
// nearest contact in (and the host's reset / save / restore of them), and sphere_mesh_collide 0x1f3170, a sphere against
// the hero's collision mesh.
#include <game/collision/collide_common.h>
#include <game/collision/collide_hit.h>
#include <game/bridge/guard.h>

namespace rac1 {
using namespace collide_detail;

namespace collide_detail {
VuContact g_vu;
}  // namespace collide_detail

// The VU0 contact registers above zeroed (the oracle starts each original call with clear VU
// registers).
// orig: (host) test support
void collide_vu_reset() { g_vu = VuContact{}; }
// The contact registers above (vf25..vf29: 15 floats) saved and put back whole: a host stretch that runs the game's code on a
// journal of EE memory (mrp_lod.cpp's shadow runs) leaves them as it found them, as it leaves the memory.
// orig: (host)
void collide_vu_save(float out[15]) {
    static_assert(sizeof(VuContact) == 15 * sizeof(float), "the five registers' x / y / z");
    std::memcpy(out, &g_vu, sizeof(VuContact));
}
// orig: (host)
void collide_vu_restore(const float in[15]) { std::memcpy(&g_vu, in, sizeof(VuContact)); }

// orig: 0x001f3170 — sphere (pos, radius) against the collision mesh *(gp-0x75e8) in 1/64 units
// (hand-written VU0 macro code): bounding spheres select chunks, their vertices get outcodes, and each front-facing
// triangle's closest point inside the radius shrinks it; the nearest hit goes to 0x173e40 (+0x20 point, +0x30 pushed-out pos,
// +0x40 normal, +0x50.. triangle, +0x1c 0x1000 | flags, +0x18 0). Returns 1 on a hit. vf25..vf29 w lanes (caller leftovers)
// are stored as 0. The original's chunk list (0x70000000 + 4 i) and vertices (0x70002000 + 16 i) are on the stack here
// (8 KB: chunks[] 4 KB, verts 4 KB): nothing reads them after the call (stage 5, task C2a).
int32_t sphere_mesh_collide(uint32_t pos, float radius) {
    const SprScope spr_scope(SprScopeId::SphereMeshCollide);  // (RAC1_BRIDGE_GUARD=spr: the scratchpad's data flow, src/game/bridge/guard.h)
    uint32_t t8 = lwu(OV(0x00166c00u - 0x75e8));
    if (t8 == 0) return 0;
    int32_t t9 = lw(t8);
    t8 += 0x10;
    const float w21 = u2f(0x3f7fdf3bu);
    const float r = qmul(radius, 64.0f);
    const L3 p = muls(l3(pos), 64.0f);
    const L3 lo3 = L3{qsub(p.x, r), qsub(p.y, r), qsub(p.z, r)};
    const L3 hi3 = L3{qadd(p.x, r), qadd(p.y, r), qadd(p.z, r)};
    float r2 = qmul(r, r);
    // bounding spheres: collect the chunks whose sphere overlaps. The original lists them at 0x70000000 + 4 n and then walks
    // the list up to 0x70000000 + (its end & 0xfff): 1024 chunks or more wrap that end, and only the first n mod 1024 of them
    // are tested (none for exactly 1024). Kept (a level with that many chunks under one sphere would behave so in the
    // original); the entries past 1024, never walked, are not kept.
    constexpr uint32_t kListCap = 0x1000 / 4;
    uint32_t chunks[kListCap];
    uint32_t nchunks = 0;
    for (; t9 > 0; --t9, t8 += 0x10) {
        const L3 c = u16x3(t8);
        float s = qadd(r, static_cast<float>(lhu(t8 + 6)));
        s = qmul(s, s);
        const L3 d = mul3(sub3(c, p), sub3(c, p));
        const float y = qmadd(qmadd(qsub(d.y, s), 1.0f, d.x), 1.0f, d.z);
        const int64_t v = static_cast<int64_t>((static_cast<uint64_t>(fbits(y)) << 32) | fbits(d.x));
        if (v > 0) continue;
        if (nchunks < kListCap) chunks[nchunks] = t8;
        ++nchunks;
    }
    const uint32_t walked = nchunks & (kListCap - 1);  // (the wrap above)
    const uint16_t lo[3] = {static_cast<uint16_t>(qftoi0(lo3.x)), static_cast<uint16_t>(qftoi0(lo3.y)), static_cast<uint16_t>(qftoi0(lo3.z))};
    const uint16_t hi[3] = {static_cast<uint16_t>(qftoi0(hi3.x)), static_cast<uint16_t>(qftoi0(hi3.y)), static_cast<uint16_t>(qftoi0(hi3.z))};
    CollVerts verts{};
    uint32_t hit = 0, w2 = 0;  // w2: vf2.w, stored with the pushed-out position
    L3 h_pt{}, h_n{}, h_a{}, h_b{}, h_c{};
    for (uint32_t i = 0; i < walked; ++i) {
        const uint32_t ent = chunks[i];
        // vertices (pairs) as floats + outcode (bit 0 below, bit 1 above, per axis)
        uint32_t vs = lwu(ent + 0xc);
        uint32_t vi = 0;
        int32_t nv = lhu(ent + 0xa);
        do {
            for (uint32_t k = 0; k < 2; ++k, vs += 8, ++vi) {
                uint32_t oc = 0;
                for (uint32_t a = 0; a < 3; ++a) {
                    const uint16_t c = lhu(vs + a * 2);
                    if ((static_cast<uint16_t>(c - lo[a]) & 0x8000u) != 0) oc |= 1u << (a * 8);
                    if ((static_cast<uint16_t>(hi[a] - c) & 0x8000u) != 0) oc |= 2u << (a * 8);
                }
                verts.put(vi, u16x3(vs), oc);
                if (k == 1) w2 = lhu(vs + 6);  // vf2 keeps the raw w of the pair's second vertex
            }
            nv -= 2;
        } while (nv > 0);
        // triangles
        uint32_t tri = lwu(ent + 0xc) + static_cast<uint32_t>(lhu(ent + 0xa)) * 8;
        const uint32_t tend = tri + static_cast<uint32_t>(lbu(ent + 8)) * 4;
        for (; tri != tend; tri += 4) {
            const CollVerts::Vert& c0 = verts[lbu(tri)];
            const CollVerts::Vert& c1 = verts[lbu(tri + 1)];
            const CollVerts::Vert& c2 = verts[lbu(tri + 2)];
            const uint32_t flags = lbu(tri + 3);
            if ((c0.oc & c1.oc & c2.oc) != 0) continue;
            const L3 v1 = c0.p;
            w2 = c1.oc;
            const L3 e2 = sub3(c1.p, v1), e3 = sub3(c2.p, v1);
            L3 rel = sub3(p, v1);
            const L3 n = opm(e3, e2);
            const float dn = hsum(mul3(rel, n));
            const float q = qdiv(dn, hsum(mul3(n, n)));
            if (neg(dn)) continue;
            const L3 nn = sub3(L3{0.0f, 0.0f, 0.0f}, n);
            const L3 acc = add3(L3{0.0f, 0.0f, 0.0f}, rel);
            L3 t7 = muls(n, q);
            rel = madds(acc, nn, q);
            t7 = mul3(t7, t7);
            L3 t9v = sub3(e3, e2);
            const L3 t10 = sub3(rel, e2);
            float d2 = hsum(t7);
            t7 = opm(rel, e2);
            L3 t8v = opm(e3, rel);
            if (neg(qsub(r2, d2))) continue;
            t9v = opm(t10, t9v);
            const float s7 = hsum_y(mul3(t7, n)), s8 = hsum_y(mul3(t8v, n)), s9 = hsum_y(mul3(t9v, n));
            if (neg(s7) || neg(s8) || neg(s9)) {
                L3 a{}, b{};
                if (neg(s7)) { b = add3(e2, L3{}); }
                else if (neg(s8)) { a = add3(e3, L3{}); }
                else { a = add3(e2, L3{}); b = add3(e3, L3{}); }
                const L3 d = sub3(rel, a);
                b = sub3(b, a);
                float t = qadd(0.0f, qdiv(hsum(mul3(d, b)), hsum(mul3(b, b))));
                const uint32_t tb = fbits(t);
                if (static_cast<int32_t>(tb) < 0) t = 0.0f;  // vmax with +0
                else if (static_cast<int32_t>(tb) > static_cast<int32_t>(0x3f800000u)) t = 1.0f;
                rel = madds(add3(L3{}, a), b, t);
                const L3 off = sub3(rel, sub3(p, v1));
                d2 = hsum(mul3(off, off));
                if (neg(qsub(r2, d2))) continue;
            }
            hit = flags + 0x1000;
            r2 = qmul(d2, w21);
            h_n = add3(n, L3{});
            h_pt = add3(rel, v1);
            h_a = add3(L3{}, v1);
            h_b = add3(e2, v1);
            h_c = add3(e3, v1);
        }
    }
    if (hit == 0) return 0;
    const float q = qrsqrt(r, r2);
    L3 push = sub3(p, h_pt);
    const float k = u2f(0x3c800000u);
    h_pt = muls(h_pt, k);
    h_a = muls(h_a, k);
    h_b = muls(h_b, k);
    h_c = muls(h_c, k);
    push = add3(muls(muls(push, q), k), h_pt);
    CollisionHit out;
    out.moby = 0;
    out.flags = hit;
    out.point = hit_vec(h_pt, 0);
    out.pushed = hit_vec(push, w2);
    out.normal = hit_vec(h_n, 0);
    out.tri[0] = hit_vec(h_a, 0);
    out.tri[1] = hit_vec(h_b, 0);
    out.tri[2] = hit_vec(h_c, 0);
    coll_hit_mirror(out);
    g_vu = VuContact{h_pt, h_n, h_a, h_b, h_c};
    return 1;
}

}  // namespace rac1
