// See context.h.
#include <game/enhance/context.h>

#include <game/enhance/pool.h>
#include <game/enhance/session.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_sky.h>
#include <platform/platform.h>
#include <runtime/kernel/ee_memory.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace rac1 {

namespace {

struct Extent {
    uint32_t veldin, level, size;
};
struct Fn {
    uint32_t veldin, size;
};
// One level's context
struct Context {
    int level = 0;
    uint32_t code_lo = 0, code_hi = 0;  // its code section
    uint32_t image = 0, image_size = 0; // its image in the session heap (CONTEXT_BASE there), up to code_lo
    std::map<uint32_t, Extent> ext;     // its engine objects, by the level's start (levelN.ext)
    std::map<uint32_t, Fn> fns;         // its functions paired with Veldin's, by the level's start (levelN.txt fn lines, variants)
    size_t sites = 0;                   // sites translated through it
    struct View {
        uint32_t veldin, size, to;
    };
    std::vector<View> views;            // its views of engine objects (ctx_view)
};
std::unique_ptr<Context> g_ctx[32];

bool trace() {
    static const bool t = [] {
        const char* e = std::getenv("RAC1_CTX_TRACE");
        return e != nullptr && std::atoi(e) != 0;
    }();
    return t;
}

std::string owner(int level) { return "context " + std::to_string(level); }

// the level's map: its engine objects (extents) and its functions paired with Veldin's
bool read_map(Context& c) {
    const std::string base = "assets/generated/overlay_map/level" + std::to_string(c.level);
    std::ifstream map(base + ".txt"), ext(base + ".ext");
    if (!map || !ext) {
        std::fprintf(stderr, "[ctx] level %d: %s.txt or .ext missing (./setup.sh): no context\n", c.level, base.c_str());
        return false;
    }
    std::string line;
    while (std::getline(map, line)) {
        std::istringstream s(line);
        std::string kind;
        uint32_t v = 0, l = 0, n = 0;
        s >> kind >> std::hex >> v >> l >> n;
        if (kind == "fn" && n != 0) c.fns[l] = Fn{v, n};
    }
    size_t n = 0;
    const VariantRow* rows = ovl_variants(c.level, &n);  // (the level's own builds standing in for Veldin's functions)
    for (size_t i = 0; i < n; ++i) c.fns[rows[i].level] = Fn{rows[i].veldin, rows[i].size};
    if (VariantRow sky; sky_dispatcher_row(c.level, &sky)) c.fns[sky.level] = Fn{sky.veldin, sky.size};
    while (std::getline(ext, line)) {
        std::istringstream s(line);
        std::string kind;
        Extent e{};
        s >> kind >> std::hex >> e.veldin >> e.level >> e.size;
        if (kind == "ext" && e.size != 0) c.ext[e.level] = e;
    }
    if (c.ext.empty()) {
        std::fprintf(stderr, "[ctx] level %d: no extents in %s.ext: no context\n", c.level, base.c_str());
        return false;
    }
    return true;
}

CtxKind classify(const Context& c, uint32_t a, uint32_t* to) {
    if (a < CONTEXT_BASE) {
        *to = a;
        return CtxKind::Base;
    }
    if (a >= c.code_lo && a < c.code_hi) {
        auto it = c.fns.upper_bound(a);
        // (a function the map pairs with Veldin's runs Veldin's port, unless the level has a port of its own for it: then
        // it is the level's, as on its own level, where its dispatch finds the level's first. Only its start: an address
        // inside a function is a label of its own, a jump table's entry its switch compares as the level's: a tag)
        if (it != c.fns.begin() && a == std::prev(it)->first && !ovl_level_moby_fn_has(c.level, region_xlate(c.level, a))) {
            *to = a - std::prev(it)->first + std::prev(it)->second.veldin;
            return CtxKind::EngineCode;
        }
        *to = ctx_tag(c.level, a);
        return CtxKind::OwnCode;
    }
    if (a < c.code_lo) {
        auto it = c.ext.upper_bound(a);
        if (it != c.ext.begin() && a - std::prev(it)->first < std::prev(it)->second.size) {
            const Extent& e = std::prev(it)->second;
            *to = a - e.level + e.veldin;  // (the session's map: Veldin's own, the identity)
            for (const Context::View& v : c.views)  // (an object the level sees a view of its own of: its copy)
                if (*to >= v.veldin && *to - v.veldin < v.size) *to = *to - v.veldin + v.to;
            return CtxKind::EngineData;
        }
        *to = c.image + (a - CONTEXT_BASE);
        return CtxKind::OwnData;
    }
    *to = a;
    return CtxKind::Outside;
}

uint32_t site_xlate(int level, uint32_t a) {
    Context* c = level >= 0 && level < 32 ? g_ctx[level].get() : nullptr;
    if (c == nullptr) return region_xlate(level, a);
    uint32_t to = a;
    const CtxKind k = classify(*c, a, &to);
    ++c->sites;
    if (k == CtxKind::Outside) {
        static std::set<std::pair<int, uint32_t>> told;
        if (told.insert({level, a}).second)
            std::fprintf(stderr, "[ctx] level %d: %08x is past its code (its layout): kept\n", level, a);
    }
    if (trace()) std::printf("[ctx] site L%d:%08x %s -> %08x\n", level, a, ctx_kind_name(k), to);
    return to;
}

// the image's pointer words relocated (those in the level's own data: the engine objects' words are never used, their
// sites go to the session's): written to the pool's context/levelNN.rel (the record of what was moved, and why)
void relocate(Context& c) {
    uint8_t* ram = ee_get_ram_base();
    size_t counts[6] = {};
    std::string rel = "# level " + std::to_string(c.level) + "'s context: the pointer words of its own data, relocated (docs/open-engine.md)\n"
                      "# <address> <word> <kind of its target> <relocated>\n";
    const bool poison = [] {
        const char* e = std::getenv("RAC1_CTX_POISON");
        return e != nullptr && std::atoi(e) != 0;
    }();
    for (uint32_t off = 0; off + 4 <= c.image_size; off += 4) {
        const uint32_t a = CONTEXT_BASE + off;
        uint32_t dummy = 0;
        if (classify(c, a, &dummy) != CtxKind::OwnData) {
            // (testing, RAC1_CTX_POISON=1: the copy's engine objects filled with a pattern: a use of them shows)
            if (poison) std::memcpy(ram + c.image + off, "\xa5\x5a\xa5\x5a", 4);
            continue;
        }
        uint32_t w = 0;
        std::memcpy(&w, ram + c.image + off, 4);
        if (w < CONTEXT_BASE || w >= c.code_hi) continue;
        if (w >= c.code_lo && (w & 3u) != 0) continue;  // (a code address is a word's)
        uint32_t to = w;
        const CtxKind k = classify(c, w, &to);
        if (k == CtxKind::Base || k == CtxKind::Outside) continue;
        ++counts[static_cast<int>(k)];
        std::memcpy(ram + c.image + off, &to, 4);
        char b[80];
        std::snprintf(b, sizeof b, "%08x %08x %s %08x\n", a, w, ctx_kind_name(k), to);
        rel += b;
    }
    char name[40];
    std::snprintf(name, sizeof name, "context/level%02d.rel", c.level);
    platform::write_file_replace(std::string(pool_dir()) + "/" + name, rel);
    std::printf("[ctx] level %d: image %08x+%#x relocated: -> own data %zu, -> engine data %zu, -> own code %zu (tags), -> engine code %zu%s\n",
                c.level, c.image, c.image_size, counts[static_cast<int>(CtxKind::OwnData)], counts[static_cast<int>(CtxKind::EngineData)],
                counts[static_cast<int>(CtxKind::OwnCode)], counts[static_cast<int>(CtxKind::EngineCode)], poison ? " (its engine objects poisoned)" : "");
}

}  // namespace

// orig: (host) see context.h
const char* ctx_kind_name(CtxKind k) {
    switch (k) {
    case CtxKind::Base: return "base";
    case CtxKind::EngineData: return "engine-data";
    case CtxKind::OwnData: return "own-data";
    case CtxKind::EngineCode: return "engine-code";
    case CtxKind::OwnCode: return "own-code";
    case CtxKind::Outside: return "outside";
    }
    return "?";
}

// orig: (host) see context.h
bool ctx_create(int level) {
    if (level < 0 || level >= 19) return false;
    if (g_ctx[level] != nullptr) return true;
    if (level == ovl_reloc_level()) return true;  // (the loaded overlay's: native)
    if (region_other()) {
        std::fprintf(stderr, "[ctx] level %d: contexts are for the North American disc for now: no context\n", level);
        return false;
    }
    if (!session_memory_on() || !pool_open(true)) {
        std::fprintf(stderr, "[ctx] level %d: no session memory or no pool: no context\n", level);
        return false;
    }
    auto c = std::make_unique<Context>();
    c->level = level;
    std::vector<uint8_t> img;
    if (!pool_context_image(level, img, &c->code_lo, &c->code_hi)) {
        std::fprintf(stderr, "[ctx] level %d: the pool has no context image: no context\n", level);
        return false;
    }
    if (!read_map(*c)) return false;
    c->image_size = static_cast<uint32_t>(img.size());
    c->image = session_heap_alloc(c->image_size, owner(level));
    if (c->image == 0) return false;
    std::memcpy(ee_get_ram_base() + c->image, img.data(), img.size());
    relocate(*c);
    g_ctx[level] = std::move(c);
    g_ctx_xlate = site_xlate;
    g_ctx_levels |= 1u << level;
    ++g_region_gen;
    return true;
}

// orig: (host) see context.h
void ctx_drop_all() {
    bool any = false;
    for (int l = 0; l < 32; ++l) {
        if (g_ctx[l] == nullptr) continue;
        std::printf("[ctx] level %d's context dropped (%zu sites translated through it)\n", l, g_ctx[l]->sites);
        session_heap_free_owner(owner(l));
        g_ctx[l].reset();
        any = true;
    }
    g_ctx_levels = 0;
    if (any) ++g_region_gen;
}

// orig: (host) see context.h
bool ctx_view(int level, uint32_t veldin, uint32_t size, uint32_t to) {
    if (!ctx_exists(level)) return false;
    auto& vs = g_ctx[level]->views;
    for (auto it = vs.begin(); it != vs.end(); ++it)
        if (it->veldin == veldin) {
            vs.erase(it);
            break;
        }
    if (to != 0) vs.push_back(Context::View{veldin, size, to});
    std::printf("[ctx] level %d: Veldin's object %08x+%#x seen by its sites as %08x\n", level, veldin, size, to);
    ++g_region_gen;
    return true;
}

// orig: (host) see context.h
bool ctx_exists(int level) { return level >= 0 && level < 32 && g_ctx[level] != nullptr; }

// orig: (host) see context.h
uint32_t ctx_image(int level) { return ctx_exists(level) ? g_ctx[level]->image : 0; }

// orig: (host) see context.h
CtxKind ctx_classify(int level, uint32_t a, uint32_t* to) {
    if (!ctx_exists(level)) {
        *to = a;
        return CtxKind::Outside;
    }
    return classify(*g_ctx[level], a, to);
}

// orig: (host) see context.h
uint32_t ctx_xlate(int level, uint32_t a) {
    uint32_t to = a;
    if (ctx_exists(level)) classify(*g_ctx[level], a, &to);
    return to;
}

// orig: (host) see context.h
uint32_t ctx_to_level(int level, uint32_t s) {
    if (!ctx_exists(level)) return 0;
    const Context& c = *g_ctx[level];
    if (ctx_is_tag(s)) return ctx_tag_level(s) == level ? ctx_tag_addr(s) : 0;
    if (s >= c.image && s < c.image + c.image_size) return CONTEXT_BASE + (s - c.image);
    for (const auto& [l, e] : c.ext)  // (Veldin's object back to the level's)
        if (s >= e.veldin && s - e.veldin < e.size) return s - e.veldin + e.level;
    for (const auto& [l, f] : c.fns)
        if (s >= f.veldin && s - f.veldin < f.size) return s - f.veldin + l;
    return 0;
}

}  // namespace rac1
