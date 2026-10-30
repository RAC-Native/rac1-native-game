// (2026-10-30) The tfrags' programs in a vertex shader (the Wii U: ports/wiiu/shaders/mrp_tfrag_vs.vert, RAC1_WIIU_TFRAG_VS). Host code:
// none of it is the game's. Included by mrp_tfrag_gpu.cpp (a .inc: no header of src/game changed - the codegen fingerprint).
//
// The CPU route (tfrag_exec_cpu_fast + tfrag_draw_cpu_one) runs a plan's steps on a copy of the image, then emits per sent vertex its
// GpuV. Without a compute shader the vertex shader cannot run the steps in order over shared memory; but the steps of every plan of the
// levels seen are independent within themselves (tfrag_level_build's "steps run in order 0"), and each slot of the image is written by
// one step at most, so a slot's final value is a closed expression of the image, the view and the band constants:
//  - a record (position + colour): X(q) - the raw record through the view (T, or the emission's own transform) -, M(q, P1, P2, band) -
//    the morph of X(q) toward two records (each X or M: two levels at most) -, or raw (never written);
//  - a vertex info: the image's own, or a selection made by the substitutions (MS: the parent's info when both parents' w are past the
//    band's threshold) and the list fix-up after them (F: the second index's info when both records are past it) - at most two MS
//    selections under one F selection (the shape SlotDesc encodes).
// tfrag_vs_build resolves every plan into those expressions once a level (the plan refused - the CPU route for its frames - when a
// step does not fit: a serial step, a slot written twice, a selection under a selection's record, more than two morph levels), as
// tables the vertex shader reads (one RGBA32UI texel each):
//  - the plan's header: form (0 morphed records, 3 raw with the guard band, 4 tfrag B), its vertices' count;
//  - per sent vertex: its slot descriptor (form 0) or its vertex info's image qword (forms 3 / 4), whether its clip flags count for ADC,
//    its run;
//  - per vertex-info slot (form 0): the selections (SlotDesc);
//  - per leaf (an image vertex info a selection can end on): its qword, its records (lane 3: the emission's; lane 2: F's);
//  - per record: X / M / raw, its image qword, its parents, its lit colour (k + 1; 0 the image's words).
// Per frame the CPU writes only the draws' records (image / plan / colours / runs / the view set), the view sets (the VU view, the clip
// matrix = the world renderers' view-projection x the draw's world view, the constants the program reads), the runs (texture word,
// layer, draw_flags, FOGCOL), and one u32 per triangle corner (draw << 16 | closing vertex << 2 | corner). The shader evaluates the
// corner's vertex as the CPU route would, and collapses a triangle whose closing vertex has ADC (forms 3 only: the corner's three
// vertices' guard band flags). The float operations are the route's, in its order, rounded as the GPU rounds (to nearest, not the EE's
// toward zero): positions within a few ulps, the decisions (substitutions, ADC) the same but where a w lies within those ulps of a
// threshold. tfrag_vs_eval_cpu is the shader's C++ twin (the PC's check, RAC1_MRPW_TFRAG_VS_CHECK).

namespace rac1::mrpw {

namespace {

struct TvsRec {  // a record expression
    uint32_t q = 0;            // its image qword (the raw record)
    uint8_t depth = 0;         // 0 X, 1 M of X / raw parents, 2 M with an M parent
    bool raw = false;          // never written: its qword's words read as floats
    bool morph = false;
    uint8_t band = 0;          // 0 A (0x29a / 0x29c), 1 B (0x29b / 0x29d)
    int32_t p1 = -1, p2 = -1;  // M: the parents' records
    uint32_t lit = 0;          // its colour (qword q + 1): the lit colour k + 1, 0 the image's words
};
struct TvsSub {  // a selection between two vertex infos: `t` when both records' w are past the band's threshold, else `f`
    bool has = false;
    uint8_t band = 0;
    int32_t p1 = -1, p2 = -1;  // the records
    uint32_t t = 0, f = 0;     // image qwords
};
struct TvsInfo {  // a vertex-info slot's value: B, or with F: (B's lane-2 record and A's lane-3 record past the threshold) ? A : B
    bool f = false;
    uint8_t band_f = 0;
    TvsSub a, b;
};

// orig: (host) one plan's expressions -> its tables appended to `out` (texels); false: refused (the CPU route's)
bool tvs_plan(const TfragEntryGpu& e, int lod, const TfragPlan& p, std::vector<std::array<uint32_t, 4>>& out, uint32_t& base, uint32_t& why) {
    const std::vector<uint8_t>& img = e.image[static_cast<size_t>(lod)];
    const uint32_t nq = static_cast<uint32_t>(img.size() / 16);
    auto lanew = [&](uint32_t q, int l) { return lane(img, q, l); };
    const uint32_t ca = e.colour_addr[static_cast<size_t>(lod)], cn = e.colour_count[static_cast<size_t>(lod)];
    auto lit_of = [&](uint32_t q) -> uint32_t {  // (the record at q: its colour qword q + 1 written with a lit colour before the steps)
        const uint32_t c = q + 1;
        if (e.colour_n == 0 || c < ca || ((c - ca) & 1u) != 0 || (c - ca) / 2 >= cn) return 0;
        return (c - ca) / 2 + 1;
    };
    std::vector<TvsRec> recs;
    std::vector<int32_t> rec_at(nq, -1);  // a slot's record expression (-1 raw)
    std::vector<uint8_t> written(nq, 0);  // (a slot written by a step: once at most)
    std::vector<TvsInfo> info(nq);
    std::vector<uint8_t> info_set(nq, 0);
    auto raw_rec = [&](uint32_t q) -> int32_t {
        TvsRec r;
        r.q = q;
        r.raw = true;
        r.lit = lit_of(q);
        recs.push_back(r);
        return static_cast<int32_t>(recs.size() - 1);
    };
    auto rec_now = [&](uint32_t q) -> int32_t { return rec_at[q] >= 0 ? rec_at[q] : raw_rec(q); };
    auto plain = [&](uint32_t q) -> uint32_t {  // (a slot's vertex info when no selection has written it: its own qword)
        return q;
    };
    auto fail = [&](uint32_t w) {
        why = w;
        return false;
    };
    bool fixed = false;  // (an F step seen: the records final from there - no T / M after it)
    for (const TfragStep& s : p.steps) {
        if (s.serial) return fail(1);
        if (fixed && s.kind != 3) return fail(4);
        if (s.kind == 3) fixed = true;
        const uint8_t band = s.band == 0x29a ? 0 : s.band == 0x29b ? 1 : 2;
        if (s.kind != 0 && band > 1) return fail(2);
        if (s.kind == 0) {  // T: the records [a, a + 2n) through the view
            for (uint32_t k = 0; k < s.n; ++k) {
                const uint32_t a = s.a + 2 * k;
                if (a + 1 >= nq) return fail(3);
                if (written[a] || written[a + 1] || info_set[a]) return fail(4);
                written[a] = written[a + 1] = 1;
                TvsRec r;
                r.q = a;
                r.lit = lit_of(a);
                recs.push_back(r);
                rec_at[a] = static_cast<int32_t>(recs.size() - 1);
            }
        } else if (s.kind == 1 || s.kind == 2) {  // M / MS
            // (non-serial: no iteration reads what another writes - the reads before the writes, as the CPU's order gives)
            struct W {
                uint32_t rec, via;
                int32_t node;
                TvsInfo inf;
            };
            std::vector<W> ws;
            for (uint32_t k = 0; k < s.n; ++k) {
                const uint32_t via = s.a + k;
                const uint32_t pl = s.b + k / 4;
                if (via >= nq || pl >= nq) return fail(3);
                if (info_set[via] || written[pl]) return fail(5);
                const uint32_t rec = lanew(via, 3) & 0xffff;
                const uint32_t pvi = lanew(pl, static_cast<int>(k % 4)) & 0xffff;
                if (pvi >= nq || info_set[pvi] || rec + 1 >= nq) return fail(6);
                const uint32_t r1 = lanew(pvi, 3) & 0xffff, r2 = lanew(via, 2) & 0xffff;
                if (r1 + 1 >= nq || r2 + 1 >= nq) return fail(3);
                if (written[rec] || written[rec + 1]) return fail(4);  // (the morph's own X: its raw record)
                const int32_t P1 = rec_now(r1), P2 = rec_now(r2);
                TvsRec r;
                r.q = rec;
                r.lit = lit_of(rec);
                r.morph = true;
                r.band = band;
                r.p1 = P1;
                r.p2 = P2;
                const uint8_t d1 = recs[static_cast<size_t>(P1)].depth, d2 = recs[static_cast<size_t>(P2)].depth;
                r.depth = static_cast<uint8_t>(1 + std::max(d1, d2));
                if (r.depth > 2) return fail(7);
                recs.push_back(r);
                W w{rec, via, static_cast<int32_t>(recs.size() - 1), {}};
                if (s.kind == 2) {
                    w.inf.b = {true, band, P1, P2, plain(pvi), plain(via)};
                }
                ws.push_back(w);
            }
            for (const W& w : ws) {
                if (written[w.rec]) return fail(4);
                written[w.rec] = written[w.rec + 1] = 1;
                rec_at[w.rec] = w.node;
                if (s.kind == 2) {
                    info[w.via] = w.inf;
                    info_set[w.via] = 1;
                    written[w.via] = 1;
                }
            }
        } else {  // F: the list's entries [a, a + n), the second index list at b
            struct W {
                uint32_t la;
                TvsInfo inf;
            };
            std::vector<W> ws;
            for (uint32_t j = 0; j < s.n; ++j) {
                const uint32_t la = s.a + j;
                const uint32_t pl = s.b + j / 4;
                if (la >= nq || pl >= nq) return fail(3);
                if (written[pl]) return fail(5);
                const uint32_t vi = lanew(pl, static_cast<int>(j % 4)) & 0xffff;
                if (vi >= nq) return fail(3);
                const TvsInfo& il = info[la];
                const TvsInfo& iv = info[vi];
                if ((info_set[la] && il.f) || (info_set[vi] && iv.f)) return fail(8);
                TvsInfo n;
                n.f = true;
                n.band_f = band;
                n.b = info_set[la] ? il.b : TvsSub{false, 0, -1, -1, la, la};
                n.a = info_set[vi] ? iv.b : TvsSub{false, 0, -1, -1, vi, vi};
                ws.push_back({la, n});
            }
            for (const W& w : ws) {
                info[w.la] = w.inf;
                info_set[w.la] = 1;
                written[w.la] = 1;
            }
        }
    }
    // the tables: [0] header; the vertices; the slots (2 texels each); the leaves; the records
    const int form = p.form;
    const uint32_t nv = static_cast<uint32_t>(p.verts.size());
    if (nv >= (1u << 13)) return fail(9);
    base = static_cast<uint32_t>(out.size());
    out.push_back({static_cast<uint32_t>(form), nv, 0u, 0u});
    // (the sent vertices four a texel: its slot's texel (form 0, plan-relative) or its vertex info's qword (forms 3 / 4) | counts << 16 |
    // its run << 17)
    const uint32_t v0 = static_cast<uint32_t>(out.size());
    out.resize(out.size() + (nv + 3) / 4, std::array<uint32_t, 4>{});
    auto vput = [&](uint32_t j, uint32_t w) { out[v0 + j / 4][j % 4] = w; };
    for (uint32_t j = 0; j < nv; ++j)
        if (p.verts[j].run >= (1u << 15)) return fail(9);
    if (form == 3 || form == 4) {
        if (!p.steps.empty()) return fail(10);
        for (uint32_t j = 0; j < nv; ++j) {
            const TfragPlanVertex& pv = p.verts[j];
            if (pv.via >= nq || (lanew(pv.via, 3) & 0xffff) + 1 >= nq) return fail(3);
            vput(j, pv.via | (pv.counts ? 1u << 16 : 0u) | static_cast<uint32_t>(pv.run) << 17);
        }
        return true;
    }
    if (form != 0) return fail(11);
    // form 0: the slots the vertices name, the leaves and records they reach
    std::vector<int32_t> slot_of(nq, -1), leaf_of(nq, -1), rec_out(recs.size(), -1);
    std::vector<std::array<uint32_t, 4>> slots, leaves, rtab;
    std::vector<uint32_t> rec_order;
    // (a record's texel: its parents first referenced by index - the indices are the table's, filled after)
    std::function<int32_t(int32_t)> rec_id = [&](int32_t r) -> int32_t {
        if (r < 0) return -1;
        if (rec_out[static_cast<size_t>(r)] >= 0) return rec_out[static_cast<size_t>(r)];
        const TvsRec& x = recs[static_cast<size_t>(r)];
        const int32_t id = static_cast<int32_t>(rtab.size());
        rec_out[static_cast<size_t>(r)] = id;
        rtab.push_back({});
        const int32_t a = x.morph ? rec_id(x.p1) : -1, b = x.morph ? rec_id(x.p2) : -1;
        rtab[static_cast<size_t>(id)] = {x.q | static_cast<uint32_t>(x.depth) << 16 | static_cast<uint32_t>(x.band) << 18 | (x.raw ? 1u << 19 : 0u) | (x.morph ? 1u << 20 : 0u),
                                         a < 0 ? 0xffffu : static_cast<uint32_t>(a), b < 0 ? 0xffffu : static_cast<uint32_t>(b), x.lit};
        return id;
    };
    auto leaf_id = [&](uint32_t q) -> int32_t {
        if (q >= nq) return -1;
        if (leaf_of[q] >= 0) return leaf_of[q];
        const uint32_t r3 = lanew(q, 3) & 0xffff, r2 = lanew(q, 2) & 0xffff;
        if (r3 + 1 >= nq) return -1;
        const int32_t a = rec_id(rec_now(r3));
        const int32_t b = r2 + 1 < nq ? rec_id(rec_now(r2)) : -1;
        leaf_of[q] = static_cast<int32_t>(leaves.size());
        leaves.push_back({q, static_cast<uint32_t>(a), b < 0 ? 0xffffu : static_cast<uint32_t>(b), 0u});
        return leaf_of[q];
    };
    auto sub_words = [&](const TvsSub& s, uint32_t& lw, uint32_t& pw) -> bool {
        const int32_t t = leaf_id(s.t), f = leaf_id(s.f);
        if (t < 0 || f < 0) return false;
        lw = static_cast<uint32_t>(t) | static_cast<uint32_t>(f) << 16;
        pw = 0;
        if (s.has) {
            const int32_t a = rec_id(s.p1), b = rec_id(s.p2);
            pw = static_cast<uint32_t>(a) | static_cast<uint32_t>(b) << 16;
        }
        return true;
    };
    for (uint32_t j = 0; j < nv; ++j) {
        const TfragPlanVertex& pv = p.verts[j];
        if (pv.via >= nq) return fail(3);
        if (slot_of[pv.via] < 0) {
            const TvsInfo inf = info_set[pv.via] ? info[pv.via] : TvsInfo{false, 0, {}, TvsSub{false, 0, -1, -1, pv.via, pv.via}};
            uint32_t la = 0, pa = 0, lb = 0, pb = 0;
            if (!sub_words(inf.b, lb, pb)) return fail(3);
            if (inf.f && !sub_words(inf.a, la, pa)) return fail(3);
            const uint32_t fl = (inf.f ? 1u : 0u) | static_cast<uint32_t>(inf.band_f) << 1 | (inf.f && inf.a.has ? 4u : 0u) | static_cast<uint32_t>(inf.a.band) << 3 |
                                (inf.b.has ? 16u : 0u) | static_cast<uint32_t>(inf.b.band) << 5;
            slot_of[pv.via] = static_cast<int32_t>(slots.size());
            slots.push_back({fl, la, lb, 0u});
            slots.push_back({pa, pb, 0u, 0u});
        }
        vput(j, static_cast<uint32_t>(slot_of[pv.via]) | (pv.counts ? 1u << 16 : 0u) | static_cast<uint32_t>(pv.run) << 17);
    }
    // (the indices plan-relative: the slots after the vertices, then the leaves, then the records)
    const uint32_t so = 1 + (nv + 3) / 4, lo = so + static_cast<uint32_t>(slots.size()), ro = lo + static_cast<uint32_t>(leaves.size());
    if (ro + rtab.size() >= 0xffffu) return fail(9);
    for (uint32_t j = 0; j < nv; ++j) {
        uint32_t& w = out[v0 + j / 4][j % 4];
        w = (((w & 0xffffu) + so) & 0xffffu) | (w & ~0xffffu);
    }
    for (size_t i = 0; i < slots.size(); i += 2) {
        std::array<uint32_t, 4>& s0 = slots[i];
        std::array<uint32_t, 4>& s1 = slots[i + 1];
        auto lfix = [&](uint32_t w) { return ((w & 0xffff) + lo) | (((w >> 16) + lo) << 16); };
        auto rfix = [&](uint32_t w) { return ((w & 0xffff) + ro) | (((w >> 16) + ro) << 16); };
        if ((s0[0] & 1u) != 0) s0[1] = lfix(s0[1]);
        s0[2] = lfix(s0[2]);
        if ((s0[0] & 4u) != 0) s1[0] = rfix(s1[0]);
        if ((s0[0] & 16u) != 0) s1[1] = rfix(s1[1]);
    }
    for (auto& l : leaves) {
        l[1] += ro;
        if (l[2] != 0xffffu) l[2] += ro;
    }
    for (auto& r : rtab) {
        if (r[1] != 0xffffu) r[1] += ro;
        if (r[2] != 0xffffu) r[2] += ro;
    }
    out[base][2] = so;
    out[base][3] = ro;
    out.insert(out.end(), slots.begin(), slots.end());
    out.insert(out.end(), leaves.begin(), leaves.end());
    out.insert(out.end(), rtab.begin(), rtab.end());
    return true;
}

}  // namespace

// orig: (host) see mrp_world_wiiu.inc
void tfrag_vs_build(const TfragLevel& L, TfragVsLevel& V) {
    V = TfragVsLevel{};
    V.generation = L.generation;
    V.plan_base.assign(L.entries.size() * 7, ~0u);
    uint32_t why_n[12] = {};
    uint32_t refused = 0, plans = 0;
    for (size_t i = 0; i < L.entries.size(); ++i) {
        const TfragEntryGpu& e = L.entries[i];
        if (!e.ok) continue;
        const TfragPlan* ps[7] = {&e.far6, &e.mid8, &e.mida, &e.neare, &e.near10, &e.near14, &e.nearb};
        const int lods[7] = {0, 1, 1, 2, 2, 2, 2};
        for (int k = 0; k < 7; ++k) {
            if (!ps[k]->ok) continue;
            ++plans;
            const size_t at = V.plans.size();
            uint32_t base = 0, why = 0;
            if (tvs_plan(e, lods[k], *ps[k], V.plans, base, why)) {
                V.plan_base[i * 7 + static_cast<size_t>(k)] = base;
            } else {
                V.plans.resize(at);
                ++refused;
                ++why_n[std::min<uint32_t>(why, 11)];
            }
        }
    }
    V.refused = refused;
    V.ok = L.ok;
    std::printf("[mrpw tfrag] the vertex shader's tables: %u plans, %u refused (why:", plans, refused);
    for (int k = 1; k < 12; ++k)
        if (why_n[k] != 0) std::printf(" %d:%u", k, why_n[k]);
    std::printf("); %zu texels (%zu KB), images %zu texels (%zu KB)\n", V.plans.size(), V.plans.size() * 16 / 1024, L.images.size(), L.images.size() * 16 / 1024);
}

// orig: (host) see mrp_world_wiiu.inc
uint32_t tfrag_vs_plan(const TfragVsLevel& V, const TfragLevel& L, uint32_t entry, const TfragPlan* p) {
    if (entry >= L.entries.size() || p == nullptr) return ~0u;
    const TfragEntryGpu& e = L.entries[entry];
    const TfragPlan* ps[7] = {&e.far6, &e.mid8, &e.mida, &e.neare, &e.near10, &e.near14, &e.nearb};
    for (size_t k = 0; k < 7; ++k)
        if (ps[k] == p) return entry * 7 + k < V.plan_base.size() ? V.plan_base[entry * 7 + k] : ~0u;
    return ~0u;
}

// ---- the vertex shader's C++ twin (the PC's check) --------------------------------------------------------------------------------

namespace {
struct TvsEval {
    const std::vector<std::array<uint32_t, 4>>* img;
    const std::vector<std::array<uint32_t, 4>>* pln;
    const uint32_t* tD;  // the frame's block (uvec4s)
    const uint32_t* cols;
    size_t ncols;
    uint32_t gImg, gPlan, gCol, gSet;
    float V[4][4];
    std::array<uint32_t, 4> I(uint32_t i) const { return i < img->size() ? (*img)[i] : std::array<uint32_t, 4>{}; }
    std::array<uint32_t, 4> P(uint32_t i) const { return i < pln->size() ? (*pln)[i] : std::array<uint32_t, 4>{}; }
    std::array<float, 4> fv(uint32_t i) const {
        std::array<float, 4> r;
        std::memcpy(r.data(), tD + 4 * i, 16);
        return r;
    }
    uint32_t tcol(uint32_t i) const { return i < ncols ? cols[i] : 0u; }
    uint32_t vtx(uint32_t j) const { return P(gPlan + 1 + j / 4)[j % 4]; }
    void xform(const float p[4], float o[4]) const {
        for (int l = 0; l < 4; ++l) {
            float a = V[3][l];
            a = a + V[0][l] * p[0];
            a = a + V[1][l] * p[1];
            o[l] = a + V[2][l] * p[2];
        }
    }
    void itof(const std::array<uint32_t, 4>& u, float o[4]) const {
        for (int l = 0; l < 4; ++l) o[l] = static_cast<float>(static_cast<int32_t>(u[static_cast<size_t>(l)]));
    }
    void rcol(uint32_t q, uint32_t lit, float c[4]) const {
        if (lit != 0) {
            const uint32_t w = gCol != 0xffffffffu ? tcol(gCol + lit - 1) : 0u;
            for (int l = 0; l < 4; ++l) c[l] = static_cast<float>((w >> (8 * l)) & 0xffu);
            return;
        }
        itof(I(gImg + q + 1), c);
    }
    void rec_x(const std::array<uint32_t, 4>& d, float p[4], float c[4]) const {
        const uint32_t q = d[0] & 0xffff;
        if ((d[0] & (1u << 19)) != 0) {
            const auto u = I(gImg + q);
            std::memcpy(p, u.data(), 16);
        } else {
            float r[4];
            itof(I(gImg + q), r);
            xform(r, p);
        }
        rcol(q, d[3], c);
    }
    void morph(const std::array<uint32_t, 4>& d, const float P1[4], const float C1[4], const float P2[4], const float C2[4], float p[4], float c[4]) const {
        float p0[4], c0[4];
        float r[4];
        itof(I(gImg + (d[0] & 0xffff)), r);
        xform(r, p0);
        rcol(d[0] & 0xffff, d[3], c0);
        const uint32_t band = (d[0] >> 18) & 1u;
        const auto K = fv(gSet + 13 + band), C = fv(gSet + 15 + band), M = fv(gSet + 9);
        float fx = C[0] * 1.0f + K[0] * p0[3], fy = C[1] * 1.0f + K[1] * p0[3];
        fx = std::max(std::min(fx, M[0]), 0.0f);
        fy = std::max(std::min(fy, M[1]), 0.0f);
        for (int l = 0; l < 4; ++l) {
            p[l] = (P1[l] + P2[l]) * fx + p0[l] * fy;
            c[l] = (C1[l] + C2[l]) * fx + c0[l] * fy;
        }
    }
    void rec1(uint32_t i, float p[4], float c[4]) const {
        const auto d = P(i);
        if ((d[0] & (1u << 20)) == 0) {
            rec_x(d, p, c);
            return;
        }
        float P1[4], C1[4], P2[4], C2[4];
        rec_x(P(gPlan + d[1]), P1, C1);
        rec_x(P(gPlan + d[2]), P2, C2);
        morph(d, P1, C1, P2, C2, p, c);
    }
    void rec2(uint32_t i, float p[4], float c[4]) const {
        const auto d = P(i);
        if (((d[0] >> 16) & 3u) < 2u) {
            rec1(i, p, c);
            return;
        }
        float P1[4], C1[4], P2[4], C2[4];
        rec1(gPlan + d[1], P1, C1);
        rec1(gPlan + d[2], P2, C2);
        morph(d, P1, C1, P2, C2, p, c);
    }
    bool cnd(uint32_t r1, uint32_t r2, uint32_t band) const {
        const float thr = fv(gSet + 13 + band)[3];
        float p[4], c[4];
        rec2(gPlan + r1, p, c);
        const bool a = (p[3] - thr) < 0.0f;
        rec2(gPlan + r2, p, c);
        return a && (p[3] - thr) < 0.0f;
    }
    uint32_t leaf(uint32_t slot) const {
        const auto s0 = P(gPlan + slot), s1 = P(gPlan + slot + 1);
        const uint32_t f = s0[0];
        uint32_t lb = s0[2] >> 16;
        if ((f & 16u) != 0 && cnd(s1[1] & 0xffff, s1[1] >> 16, (f >> 5) & 1u)) lb = s0[2] & 0xffff;
        if ((f & 1u) == 0) return lb;
        uint32_t la = s0[1] >> 16;
        if ((f & 4u) != 0 && cnd(s1[0] & 0xffff, s1[0] >> 16, (f >> 3) & 1u)) la = s0[1] & 0xffff;
        const auto LB = P(gPlan + lb), LA = P(gPlan + la);
        if (LB[2] != 0xffffu && cnd(LB[2], LA[1], (f >> 1) & 1u)) return la;
        return lb;
    }
};
}  // namespace

// orig: (host) see mrp_world_wiiu.inc
void tfrag_vs_eval_cpu(const TfragVsLevel& V, const TfragLevel& L, const TfragColours& C, const uint32_t* tD, uint32_t word, TfragVsCorner& o) {
    TvsEval E{&L.images, &V.plans, tD, C.mirror.data(), C.mirror.size(), 0, 0, 0, 0, {}};
    const uint32_t d = word >> 16, jc = (word >> 2) & 0x3fff, c = word & 3u;
    const uint32_t j = jc - 2 + c;
    const uint32_t* t0 = tD + 4 * kTvsDrawTexels * d;
    E.gImg = t0[0];
    E.gPlan = t0[1];
    E.gCol = t0[2];
    const uint32_t rbase = t0[3];
    E.gSet = t0[4];
    float stoff, q0;
    std::memcpy(&stoff, t0 + 5, 4);
    std::memcpy(&q0, t0 + 6, 4);
    const bool caster = (t0[7] & 1u) != 0;
    for (int r = 0; r < 4; ++r) {
        const auto v = E.fv(E.gSet + static_cast<uint32_t>(r));
        for (int l = 0; l < 4; ++l) E.V[r][l] = v[static_cast<size_t>(l)];
    }
    const auto hdr = E.P(E.gPlan);
    const uint32_t form = hdr[0];
    const auto c0 = E.fv(E.gSet + 8), c4 = E.fv(E.gSet + 10), c5 = E.fv(E.gSet + 11), c6 = E.fv(E.gSet + 12);
    o.adc = false;
    if (form == 3 && !caster) {  // the guard band's flags of the triangle's three vertices (those that count)
        uint32_t h = 0;
        for (uint32_t k = 0; k < 3; ++k) {
            const uint32_t vk = E.vtx(jc - 2 + k);
            if ((vk & (1u << 16)) == 0) continue;
            const uint32_t rq = E.I(E.gImg + (vk & 0xffff))[3] & 0xffff;
            float r[4], P[4];
            E.itof(E.I(E.gImg + rq), r);
            E.xform(r, P);
            float g[4];
            for (int l = 0; l < 4; ++l) g[l] = P[l] * c6[static_cast<size_t>(l)];
            const float w = std::fabs(g[3]);
            if (g[0] > w || g[0] < -w || g[1] > w || g[1] < -w || g[2] > w || g[2] < -w) h = 1;
        }
        o.adc = h != 0;
    }
    const uint32_t vt = E.vtx(j);
    float P[4], Cc[4];
    float s, t;
    uint32_t rgba[4];
    if (form == 0) {
        const uint32_t lf = E.leaf(vt & 0xffff);
        const auto LF = E.P(E.gPlan + lf);
        const auto iq = E.I(E.gImg + LF[0]);
        std::memcpy(&s, &iq[0], 4);
        std::memcpy(&t, &iq[1], 4);
        E.rec2(E.gPlan + LF[1], P, Cc);
        for (int l = 0; l < 4; ++l) {
            const float x = Cc[l] * c4[static_cast<size_t>(l)];
            rgba[l] = static_cast<uint32_t>(ftoi(x)) & 0xffu;
        }
    } else {
        const auto iq = E.I(E.gImg + (vt & 0xffff));
        std::memcpy(&s, &iq[0], 4);
        std::memcpy(&t, &iq[1], 4);
        const uint32_t rq = iq[3] & 0xffff;
        float r[4];
        E.itof(E.I(E.gImg + rq), r);
        E.xform(r, P);
        // the record's colour words: the lit colour's bytes when its qword is one of them (the draw's col_addr / col_count; none: 0),
        // else the image's
        const uint32_t ca = t0[8], cn = t0[9];
        const uint32_t cq = rq + 1;
        if (cq >= ca && ((cq - ca) & 1u) == 0 && (cq - ca) / 2 < cn) {
            const uint32_t w = E.gCol != 0xffffffffu ? E.tcol(E.gCol + (cq - ca) / 2) : 0u;
            for (int l = 0; l < 4; ++l) rgba[l] = (w >> (8 * l)) & 0xffu;
        } else {
            const auto u = E.I(E.gImg + cq);
            for (int l = 0; l < 4; ++l) rgba[l] = u[static_cast<size_t>(l)] & 0xffu;
        }
    }
    const float s0 = s - stoff, t0f = t - stoff;
    const float q = c0[0] / P[3];
    const float qq = q0 * q, ss = s0 * q, tt = t0f * q;
    float fw = P[3] + c5[3];
    if (form >= 3) {
        fw = std::min(fw, c0[2]);
        fw = std::max(fw, c0[1]);
    } else {
        fw = std::max(fw, c0[1]);
        fw = std::min(fw, c0[2]);
    }
    const float kk = qq * P[3];
    const float sgn = kk < 0.0f ? -1.0f : 1.0f;
    const float Wg = P[3] * sgn;
    const float ws = Wg != 0.0f ? Wg : 1.0f;
    o.hp = {P[0], P[1], P[2], P[3]};
    {  // (the clip matrix's columns: mat4 x vec4)
        const auto m0 = E.fv(E.gSet + 4), m1 = E.fv(E.gSet + 5), m2 = E.fv(E.gSet + 6), m3 = E.fv(E.gSet + 7);
        for (size_t l = 0; l < 4; ++l) o.clip[l] = m0[l] * P[0] + m1[l] * P[1] + m2[l] * P[2] + m3[l] * P[3];
    }
    o.stq = {ss * ws, tt * ws, qq * ws};
    o.lodq = qq;
    for (int l = 0; l < 4; ++l) o.rgba[static_cast<size_t>(l)] = static_cast<uint8_t>(rgba[l]);
    o.fog = static_cast<float>((static_cast<uint32_t>(ftoi(fw * 16.0f)) >> 4) & 0xffu);
    const uint32_t* run = tD + 4 * (rbase + (vt >> 17));
    for (int k = 0; k < 4; ++k) o.run[static_cast<size_t>(k)] = run[k];
}

}  // namespace rac1::mrpw
