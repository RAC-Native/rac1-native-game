// orig: (host) testing: the GS replica's projected 3D by source (see replica_inventory.h).
#include <game/render/replica_inventory.h>

#include <runtime/vu/vu1.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace rac1 {

namespace {

const bool g_on = [] {
    const char* e = std::getenv("RAC1_REPLICA_INVENTORY");
    return e != nullptr && std::atoi(e) != 0;
}();
std::vector<gs::Gs::LabelRange> g_notes;

uint32_t fld(uint64_t v, int lo, int n) { return static_cast<uint32_t>((v >> lo) & ((1ull << n) - 1)); }

// level, mode, label, program hash, start, primitive type, drawn by (0 the replica, 1 the MRP)
using Key = std::tuple<int, std::string, uint32_t, uint64_t, uint32_t, int, int>;
struct Count {
    uint64_t prims = 0;
    double area = 0;
};
std::map<Key, Count> g_counts;
std::map<std::pair<int, std::string>, uint64_t> g_frames;

// orig: (host) the area on the frame of a primitive's vertices clamped to it (a cost's measure)
double area_of(const gs::Primitive& p, float fw, float fh) {
    auto cx = [fw](float x) { return std::clamp(x, 0.0f, fw); };
    auto cy = [fh](float y) { return std::clamp(y, 0.0f, fh); };
    if (p.type == 6 && p.v.size() >= 2) return std::fabs((cx(p.v[1].x) - cx(p.v[0].x)) * (cy(p.v[1].y) - cy(p.v[0].y)));
    if (p.type == 3 && p.v.size() >= 3) {
        const float x0 = cx(p.v[0].x), y0 = cy(p.v[0].y), x1 = cx(p.v[1].x), y1 = cy(p.v[1].y), x2 = cx(p.v[2].x), y2 = cy(p.v[2].y);
        return 0.5 * std::fabs((x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0));
    }
    return 0.0;
}

}  // namespace

// orig: (host) see replica_inventory.h
bool replica_inventory_on() { return g_on; }

// orig: (host) see replica_inventory.h
void replica_inventory_note(uint32_t fn, uint32_t first, uint32_t end) {
    if (!g_on || end <= first) return;
    g_notes.push_back({first, end - 1, fn});
}

// orig: (host) see replica_inventory.h
void replica_inventory_chain(gs::Gs& gs) {
    if (!g_on) return;
    gs.set_inventory(true);
    // (the ranges may nest: a callback inside another's never happens, but keep the last one starting at an address)
    std::sort(g_notes.begin(), g_notes.end(), [](const gs::Gs::LabelRange& a, const gs::Gs::LabelRange& b) { return a.first < b.first; });
    std::vector<gs::Gs::LabelRange> out;
    for (const auto& r : g_notes)
        if (out.empty() || r.first > out.back().last) out.push_back(r);
    gs.set_chain_labels(std::move(out));
    g_notes.clear();
}

// orig: (host) see replica_inventory.h
void replica_inventory_count(const gs::Gs& gs, int prim_filter, const gs::DepthMap* depth, float fb_w, float fb_h, int level, const char* mode) {
    if (!g_on) return;
    const std::string m = mode;
    ++g_frames[{level, m}];
    const auto& vb = gs.vu7_batches();
    for (const gs::Primitive& p : gs.primitives()) {
        if (p.type >= 7 && p.type != 9) continue;
        if (prim_filter == 2 && p.moby) continue;
        const gs::DrawState& st = p.st;
        const uint32_t ztst = fld(st.test, 17, 2);
        bool z3d = false;
        if (depth != nullptr && fld(st.test, 16, 1) != 0 && ztst >= 2)
            for (const gs::Vertex& q : p.v) {
                const double d = depth->a + depth->b * static_cast<double>(q.z);
                if (q.z != 0 && d > 0.0 && d < 1.0) z3d = true;
            }
        bool sub3d = p.vu;
        if (!sub3d && fld(st.test, 16, 1) != 0 && ztst >= 2)
            for (const gs::Vertex& q : p.v) sub3d = sub3d || (q.z != 0 && q.z < 0xff0000u);
        if (p.sub != 0 && (p.sub == 1) != sub3d) continue;
        if (!p.vu && !z3d) continue;  // (the 2D)
        const bool mrp = p.fx != 0 && gs.fx_external();
        if (mrp && p.fx_id == gs::Gs::kVu7Kicked) continue;  // (a batch the MRP draws from its input: counted by its type 9)
        Count& c = g_counts[Key{level, m, p.src, p.vu_hash, p.vu ? p.vu_start : 0xfff, p.type, mrp ? 1 : 0}];
        if (p.type == 9) {  // (the batch's triangles)
            if (p.fx_id < vb.size()) c.prims += vb[p.fx_id].n >= 2 ? vb[p.fx_id].n - 2 : 0;
        } else {
            ++c.prims;
            c.area += area_of(p, fb_w, fb_h);
        }
    }
}

// orig: (host) see replica_inventory.h
void replica_inventory_report() {
    if (!g_on) return;
    for (const auto& [k, n] : g_frames) std::printf("[replica-inv] level %d mode %s: %llu frames\n", k.first, k.second.c_str(), static_cast<unsigned long long>(n));
    for (const auto& [k, c] : g_counts) {
        const auto& [level, mode, src, hash, start, type, mrp] = k;
        const double f = static_cast<double>(std::max<uint64_t>(1, g_frames[{level, mode}]));
        const char* name = hash != 0 ? vu1::program_name(hash) : nullptr;
        std::printf("[replica-inv] level %d mode %s src %08x prog %s start %03x type %d by %s: %.1f prims/frame, %.0f px/frame\n", level, mode.c_str(), src,
                    name != nullptr ? name : (hash != 0 ? "?" : "path2"), start, type, mrp ? "mrp" : "replica", static_cast<double>(c.prims) / f, c.area / f);
    }
    const gs::Gs& gs = gs::gs_global();
    for (const auto& [k, t] : gs.prog_times()) {
        const auto& [src, hash, start] = k;
        const char* name = vu1::program_name(hash);
        std::printf("[replica-inv] vu1 src %08x prog %s start %03x: %llu runs, %.3f ms total\n", src, name != nullptr ? name : "?", start,
                    static_cast<unsigned long long>(t.runs), static_cast<double>(t.ns) / 1e6);
    }
}

}  // namespace rac1
