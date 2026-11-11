// The tie and shrub passes' notes of what they chained (world_chained.h).
// orig: (host)
#include <game/ovl/world_chained.h>

#include <runtime/kernel/ee_memory.h>

namespace rac1 {

namespace {
WorldChained g_chained;
int g_open = -1;  // the run being noted (an index of g_chained.runs; -1: none)
}  // namespace

// orig: (host) see world_chained.h
const WorldChained& world_chained() { return g_chained; }

// orig: (host) see world_chained.h
void world_chained_clear() {
    g_chained.tie.clear();
    g_chained.shrub.clear();
    g_chained.runs.clear();
    g_open = -1;
}

// orig: (host) see world_chained.h
void world_chained_begin(int kind, uint32_t first) {
    g_open = -1;
    if ((kind != 1 && kind != 2) || ee_journal_active()) return;
    g_open = static_cast<int>(g_chained.runs.size());
    g_chained.runs.push_back({kind, first, static_cast<uint32_t>(kind == 1 ? g_chained.tie.size() : g_chained.shrub.size()), 0, false});
}

// orig: (host) see world_chained.h
void world_chained_end() { g_open = -1; }

// orig: (host) see world_chained.h
bool world_chained_noting() { return g_open >= 0 && !ee_journal_active(); }

// orig: (host) see world_chained.h
void world_chained_tie(const WorldChained::Tie& t) {
    g_chained.tie.push_back(t);
    ++g_chained.runs[static_cast<size_t>(g_open)].n;
}

// orig: (host) see world_chained.h
void world_chained_shrub(const WorldChained::Shrub& s) {
    g_chained.shrub.push_back(s);
    ++g_chained.runs[static_cast<size_t>(g_open)].n;
}

// orig: (host) see world_chained.h
void world_chained_done() {
    if (g_open >= 0) g_chained.runs[static_cast<size_t>(g_open)].ok = true;
}

}  // namespace rac1
