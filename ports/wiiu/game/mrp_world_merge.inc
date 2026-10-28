// The ties' / shrubs' draw commands merged into fewer GX2 draws (ports/wiiu/game/mrp_world_gx2.cpp; PERFORMANCE.md, Progress): the
// CPU half, no GX2 in it (included by mrp_world_gx2.cpp; a PC harness can include it too). Host code.
//
// What a draw command needs that is not the instance's (mrp_world.vert): its record's texture word, layer, draw_flags and FOGCOL.
// They are made per frame (gpu_packet: the GS state at the template's run - the packet's texture prims, the frame environment's
// registers -), but they belong to the template's run, not to the instance: every draw command of one run in a frame carries the same
// ones (checked per frame below: one that differs is drawn alone, as before). So:
//
// - the level (level_build, once per level): every template run gets a number (its place among the level's runs by first index), each
//   template vertex the number of the run it belongs to (an attribute after the TVert: VertR); a vertex two runs share - a strip whose
//   texture slot changes without a new PRIM - is copied (appended), so that all three vertices of a triangle carry its run whatever the
//   provoking vertex; the indices made absolute (the template's first vertex added: the base vertex baked in), at tidx's places;
// - the frame (plan_build, once per frame, for the TV's call and the GamePad's): the runs' records into a table (per run 4 words, the
//   vertex shader reads its run's with texelFetch); per instance one uniform block slot (the instance, its record words unused); then
//   in each batch the draw commands in their order, consecutive ones of the same instance made one item (Mode: kByUnit and of the same
//   texture unit; kUnits and up of any: the batch's units all bound, the pixel shader samples the triangle's), one GX2DrawIndexedEx
//   over their indices - the level's own range when they follow each other there (a packet's runs, a class LOD's packets: always so on
//   Veldin), else copied into the frame's index list; kInstanced: consecutive items of different instances over the same indices one
//   instanced draw (the tie / shrub passes draw a packet for each instance of a group in turn: inst 0's, inst 1's, the next packet),
//   their slots in a row, gl_InstanceID the slot.
//
// The draw order: an item's triangles are those of the draw commands it stands for, in their order, the items keep the draw
// commands' order, and an instanced draw is its instances' draws in turn (its instances being the consecutive items it replaces), so
// the GPU draws every triangle in the same order as one draw per command did: the same picture whatever the blending (Veldin's one
// batch blends: ABE with the AFAIL RGB_ONLY pass), the depth test or the alpha test. The GPU state stays the batch's, set per batch
// and pass as before (blending, depth, the alpha test's pass, FOGCOL / FGE), and the texture the triangle's unit's. Nothing reorders
// opaque draws (it would merge more - an instance's packets alternate with the group's other instances' -, but the GS's GEQUAL on
// coplanar triangles - decals - keeps the later one: the order is the picture).
#ifndef RAC1_PORTS_WIIU_MRP_WORLD_MERGE_INC
#define RAC1_PORTS_WIIU_MRP_WORLD_MERGE_INC

#include <game/render/mrp_world_backend.inc>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace rac1::mrpwm {

// a template vertex and its run (big-endian on the Wii U: the run read 8_IN_32)
struct VertR {
    mrpwb::TVert v;
    uint32_t run;
};
static_assert(sizeof(VertR) == 32);

constexpr uint32_t kNoRun = 0xffffffffu;

// The level's merged geometry
struct Level {
    uint64_t loads = ~0ull;
    std::vector<VertR> verts;            // the templates' vertices at their places (TVert's), then the copies the runs need
    std::vector<uint32_t> idx;           // absolute, at tidx's places
    std::vector<uint32_t> first, count;  // per run (its number): its indices, by first index
    size_t copies = 0;                   // vertices copied (shared by two runs)
    size_t bad = 0;                      // runs left out (indices outside the vertices / the index list: never merged)
};

// orig: (host) the level's runs numbered, its vertices given their run (copied when shared), its indices made absolute
inline void level_build(const mrpwb::LevelView& L, std::vector<mrpwb::TplRunInfo> runs, Level& out) {
    out = Level{};
    out.loads = L.loads;
    const std::vector<mrpwb::TVert>& tv = *L.tverts;
    const std::vector<uint32_t>& ti = *L.tidx;
    out.verts.resize(tv.size());
    for (size_t i = 0; i < tv.size(); ++i) out.verts[i] = VertR{tv[i], kNoRun};
    out.idx.assign(ti.size(), 0);
    std::sort(runs.begin(), runs.end(), [](const mrpwb::TplRunInfo& a, const mrpwb::TplRunInfo& b) { return a.first_index < b.first_index; });
    std::vector<std::pair<uint32_t, uint32_t>> local;  // (a shared vertex, its copy) in the run being made
    for (const mrpwb::TplRunInfo& r : runs) {
        bool ok = static_cast<uint64_t>(r.first_index) + r.count <= ti.size() && (out.first.empty() || r.first_index >= out.first.back() + out.count.back());
        for (uint32_t k = 0; ok && k < r.count; ++k) ok = static_cast<uint64_t>(r.base_vertex) + ti[r.first_index + k] < tv.size();
        if (!ok) {
            ++out.bad;
            continue;
        }
        const uint32_t g = static_cast<uint32_t>(out.first.size());
        out.first.push_back(r.first_index);
        out.count.push_back(r.count);
        local.clear();
        for (uint32_t k = 0; k < r.count; ++k) {
            uint32_t v = r.base_vertex + ti[r.first_index + k];
            if (out.verts[v].run == kNoRun) {
                out.verts[v].run = g;
            } else if (out.verts[v].run != g) {
                const auto it = std::find_if(local.begin(), local.end(), [v](const std::pair<uint32_t, uint32_t>& p) { return p.first == v; });
                if (it != local.end()) {
                    v = it->second;
                } else {
                    const uint32_t c = static_cast<uint32_t>(out.verts.size());
                    out.verts.push_back(VertR{out.verts[v].v, g});
                    local.emplace_back(v, c);
                    ++out.copies;
                    v = c;
                }
            }
            out.idx[r.first_index + k] = v;
        }
    }
    for (VertR& v : out.verts)
        if (v.run == kNoRun) v.run = 0;  // (a vertex no triangle reads)
}

// orig: (host) the run whose indices a draw command reads (`hint`: a run to try first - the previous command's next), kNoRun: none
inline uint32_t run_of(const Level& M, uint32_t first_index, uint32_t count, uint32_t hint) {
    const size_t n = M.first.size();
    uint32_t g = hint;
    if (g >= n || M.first[g] != first_index) {
        const auto it = std::lower_bound(M.first.begin(), M.first.end(), first_index);
        if (it == M.first.end() || *it != first_index) return kNoRun;
        g = static_cast<uint32_t>(it - M.first.begin());
    }
    return M.count[g] == count ? g : kNoRun;
}

// One GX2 draw of the merged path
struct Item {
    uint32_t first = 0, count = 0;  // its indices: in the level's absolute ones, or (own) in the frame's list, or (lone) tidx's
    uint32_t inst = 0, ninst = 1;   // its instances: Plan::ilist[inst, inst + ninst) (instanced when more than one); lone: its draw command
    uint32_t unit = 0;              // its texture unit in the batch's epoch (0: none; Mode::units: its first command's)
    uint32_t cmds = 0;              // the draw commands it stands for
    int32_t base = 0;               // a lone command's template first vertex (its indices relative: tidx's)
    bool own = false;               // its indices are the frame's list's
    bool lone = false;              // a draw command drawn as before: its own block (its record in it), relative indices, `base`
};
// What merges: 1 consecutive commands of one instance and one texture unit; 2 of one instance (the batch's texture units all bound,
// the pixel shader samples the triangle's: its run's draw_flags); 3 as 2, and consecutive items of different instances over the same
// indices one instanced draw (gl_InstanceID: an instanced draw is its instances' draws in turn, the order kept)
enum Mode : int { kPerDraw = 0, kByUnit = 1, kUnits = 2, kInstanced = 3 };
constexpr uint32_t kMaxInstanced = 256;  // (an item's instances in one uniform block: 256 B each, 64 KB)

struct Plan {
    uint64_t serial = ~0ull;
    int kinds = 3, mode = 0;              // (what it was made for: kinds 1 the ties, 2 the shrubs; the Mode)
    std::vector<Item> items;
    std::vector<uint32_t> batch_items;    // per batch + 1: its items [batch_items[b], batch_items[b + 1])
    std::vector<uint32_t> idx;            // the frame's own indices (absolute)
    std::vector<uint32_t> ilist;          // the items' instances
    std::vector<uint32_t> runs_used;      // the runs drawn this frame (their records in `table`)
    std::vector<uint32_t> table;          // per run 4 words: texture word, layer, draw_flags (kByUnit: without the unit), FOGCOL; valid for runs_used
    std::vector<uint32_t> stamp;          // per run: the plan's number when its record was set
    uint32_t number = 0;
    // (plan_build's: the groups - one instance's consecutive commands - before the instancing, their index segments)
    struct Group {
        uint32_t inst = 0, unit = 0, cmds = 0, seg0 = 0, nseg = 0;
    };
    std::vector<Group> groups;
    std::vector<std::pair<uint32_t, uint32_t>> segs;
    // its counts: draw commands, lone commands (a record that differs from its run's, a command outside the level's runs), indices
    // copied, instanced items and their instances
    size_t cmds = 0, conflicts = 0, unmatched = 0, copied = 0, instanced = 0, instanced_insts = 0;
};

constexpr uint32_t kUnitMask = 0xfu << 16;  // (draw_flags' texture unit)

// orig: (host) the frame's merged draws (see the top) in `mode` (kByUnit..kInstanced). `kinds`: the instances drawn (1 the ties, 2 the
// shrubs: the debug modes); a batch the caller does not draw (the sky's, the CPU path's, the tfrags', the casters') gets no items
inline void plan_build(const mrpwb::FrameView& F, const Level& M, int kinds, int mode, Plan& P) {
    const std::vector<mrpwb::Batch>& batches = *F.batches;
    const std::vector<mrpwb::DrawCmd>& draws = *F.draws;
    const std::vector<uint32_t>& cmds = *F.cmds;
    const std::vector<mrpwb::InstGpu>& insts = *F.insts;
    P.serial = F.serial;
    P.kinds = kinds;
    P.mode = mode;
    P.items.clear();
    P.batch_items.assign(1, 0);
    P.idx.clear();
    P.ilist.clear();
    P.runs_used.clear();
    P.cmds = P.conflicts = P.unmatched = P.copied = P.instanced = P.instanced_insts = 0;
    const size_t nruns = M.first.size();
    if (P.table.size() != nruns * 4) {
        P.table.assign(nruns * 4, 0);
        P.stamp.assign(nruns, 0);
        P.number = 0;
    }
    if (++P.number == 0) {  // (the stamps wrapped: cleared)
        std::fill(P.stamp.begin(), P.stamp.end(), 0);
        P.number = 1;
    }
    // (kByUnit: the unit is the item's, bound by the CPU - not compared; else the run's record holds it: a run drawn with two units
    // in a frame - two texture epochs - is drawn alone the second time)
    const uint32_t cmp_mask = mode == kByUnit ? ~kUnitMask : ~0u;
    std::vector<Plan::Group>& groups = P.groups;
    std::vector<std::pair<uint32_t, uint32_t>>& segs = P.segs;
    // the batch's groups made items: consecutive groups over the same indices (kInstanced) one instanced item; a group's indices the
    // level's own when they are one segment, else copied into the frame's list
    auto flush = [&] {
        for (size_t k = 0; k < groups.size();) {
            const Plan::Group& g = groups[k];
            size_t e = k + 1;
            if (mode >= kInstanced)
                while (e < groups.size() && e - k < kMaxInstanced && groups[e].unit == g.unit && groups[e].nseg == g.nseg &&
                       std::equal(segs.begin() + groups[e].seg0, segs.begin() + groups[e].seg0 + groups[e].nseg, segs.begin() + g.seg0))
                    ++e;
            Item it;
            it.unit = g.unit;
            it.inst = static_cast<uint32_t>(P.ilist.size());
            it.ninst = static_cast<uint32_t>(e - k);
            for (size_t q = k; q < e; ++q) {
                P.ilist.push_back(groups[q].inst);
                it.cmds += groups[q].cmds;
            }
            if (it.ninst > 1) {
                ++P.instanced;
                P.instanced_insts += it.ninst;
            }
            if (g.nseg == 1) {
                it.first = segs[g.seg0].first;
                it.count = segs[g.seg0].second;
            } else {
                it.own = true;
                it.first = static_cast<uint32_t>(P.idx.size());
                for (uint32_t s = g.seg0; s < g.seg0 + g.nseg; ++s) P.idx.insert(P.idx.end(), M.idx.begin() + segs[s].first, M.idx.begin() + segs[s].first + segs[s].second);
                it.count = static_cast<uint32_t>(P.idx.size()) - it.first;
                P.copied += it.count;
            }
            P.items.push_back(it);
            k = e;
        }
        groups.clear();
        segs.clear();
    };
    uint32_t hint = 0;
    for (size_t bi = 0; bi < batches.size(); ++bi) {
        const mrpwb::Batch& b = batches[bi];
        const bool drawn = b.gpu && !b.tfrag && !b.sky && !b.rk.shadow && static_cast<uint64_t>(b.first) + b.count <= draws.size();
        for (uint32_t i = b.first; drawn && i < b.first + b.count; ++i) {
            const mrpwb::DrawCmd& c = draws[i];
            if (c.instances != 1 || (static_cast<uint64_t>(c.base_instance) + 1) * 8 > cmds.size()) continue;  // (the caller refuses the batch)
            const uint32_t* rec = &cmds[8 * static_cast<size_t>(c.base_instance)];
            if (rec[0] >= insts.size()) continue;
            const uint32_t kind = insts[rec[0]].info[3] & 1u;
            if ((kinds & (kind != 0 ? 2 : 1)) == 0) continue;
            ++P.cmds;
            const uint32_t unit = (rec[3] >> 16) & 15u;
            const uint32_t g = run_of(M, c.first_index, c.count, hint);
            bool lone = g == kNoRun;
            if (lone) {
                ++P.unmatched;
            } else {
                hint = g + 1;
                uint32_t* t = &P.table[4 * static_cast<size_t>(g)];
                const uint32_t w[4] = {rec[1], rec[2], rec[3] & cmp_mask, rec[4]};
                if (P.stamp[g] != P.number) {
                    P.stamp[g] = P.number;
                    std::copy(w, w + 4, t);
                    P.runs_used.push_back(g);
                } else if (!std::equal(w, w + 4, t)) {
                    lone = true;
                    ++P.conflicts;
                }
            }
            if (lone) {  // drawn as before: its record in a block of its own, its relative indices
                flush();
                Item it;
                it.first = c.first_index;
                it.count = c.count;
                it.inst = i;
                it.unit = unit;
                it.cmds = 1;
                it.base = c.base_vertex;
                it.lone = true;
                P.items.push_back(it);
                continue;
            }
            if (groups.empty() || groups.back().inst != rec[0] || (mode == kByUnit && groups.back().unit != unit))
                groups.push_back(Plan::Group{rec[0], unit, 0, static_cast<uint32_t>(segs.size()), 0});
            Plan::Group& gr = groups.back();
            ++gr.cmds;
            if (gr.nseg > 0 && segs.back().first + segs.back().second == c.first_index) {
                segs.back().second += c.count;
            } else {
                segs.emplace_back(c.first_index, c.count);
                ++gr.nseg;
            }
        }
        flush();
        P.batch_items.push_back(static_cast<uint32_t>(P.items.size()));
    }
}

}  // namespace rac1::mrpwm

#endif
