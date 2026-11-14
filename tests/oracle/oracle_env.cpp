#include <stdexcept>
#include <game/ovl/ovl_reloc.h>
#include "oracle_env.h"

#include <runtime/kernel/disc.h>
#include <runtime/level/level_loader.h>
#include <runtime/sif/sif_rpc.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <unordered_set>

namespace rac1::oracle {

int g_oracle_level = 0;

bool setup_level(int level) {
    if (level < 0 || level >= 19) {
        std::printf("no level %d\n", level);
        return false;
    }
    if (!std::filesystem::exists("assets/dumps/SCUS_971.99") || !disc_has_level(level)) {
        std::printf("SKIP: game data missing (./setup.sh with the NTSC-U disc: the oracle runs on it only)\n");
        return false;
    }
    init_memory();
    LevelLoader::load_level_overlay(level);  // the level's own overlay image, as the host loads it
    ovl_reloc_set_level(level);              // the ports' (and the generators') addresses on it
    if (ovl_reloc_level() != level) {
        std::printf("SKIP: no overlay map for level %d (run ./setup.sh)\n", level);
        return false;
    }
    g_oracle_level = level;
    return true;
}

bool setup_veldin() { return setup_level(0); }

std::string dump_dir() {
    const char* d = std::getenv("RAC1_ORACLE_DUMPS");
    return d != nullptr && *d != 0 ? std::string(d) : std::string("assets/dumps/oracle");
}

std::string dump_path(const std::string& rel) { return dump_dir() + "/" + rel; }

// ORACLE_BAD_MAX: how many failing cases are described (default 2)
static const int bad_max = std::getenv("ORACLE_BAD_MAX") != nullptr ? std::atoi(std::getenv("ORACLE_BAD_MAX")) : 2;
std::vector<uint32_t> g_ignored_words;
std::vector<uint32_t> g_extra_noop_hooks;
bool g_ignore_lone_w_lanes = false;
std::vector<std::pair<uint32_t, uint32_t>> g_ignored_spr_ranges;

// orig: (host) see oracle_env.h. The scratch areas the five collision routines' own code writes (segment_collide 0x1efc70,
// sphere_collide 0x1f0d60, capsule_collide 0x1f19a0, mobys_in_sphere 0x1f2868, sphere_mesh_collide 0x1f3170: their stores,
// their sq3 vertices, the chunk transfers they start; measured with RAC1_BRIDGE_GUARD=spr on the nine tours, A2's writes
// table), each found dead after the call (A2's flows table and the restore runs: a later call reads none of them before
// writing it, except the read-and-dropped words listed below; putting them back as they were before each call changed no
// tour). The areas the posing functions they call use on their own (+0x3600 .. +0x3800, the saved registers 0x70003700 ..
// 0x7000371c among them) and the drawing's (+0x3c00 .. +0x3ffc) are not in it: still compared.
//   +0x0000 .. +0x1000  the chunk list / the first chunk buffer (sphere_mesh_collide's list, the transfers at 0)
//   +0x1000 .. +0x2000  the second chunk buffer (the next chunk fetched while one is tested)
//   +0x2000 .. +0x3000  the transformed vertices with their outcodes (sq3; from +0x1fe0 + 0x20)
//   +0x3000 .. +0x3600  segment_collide's per-axis crossings (x 0x3000, y 0x3200, z 0x3400)
//   +0x3800 .. +0x3c00  the merged (t, cell) list / the (centre, chunk) cell list; its entry past the terminator is read
//                       and dropped by the next call (the loops read the next entry's words before they test the end)
//   +0x3ffc .. +0x4000  segment_collide's step bytes
void oracle_ignore_collision_scratch() {
    static const std::pair<uint32_t, uint32_t> kRanges[] = {
        {0x70000000u, 0x70003600u},
        {0x70003800u, 0x70003c00u},
        {0x70003ffcu, 0x70004000u},
    };
    for (const auto& r : kRanges)
        if (r.first < r.second) g_ignored_spr_ranges.push_back(r);
}

namespace {
// the scratchpad words g_ignored_spr_ranges leaves out (none with ORACLE_NO_SPR_IGNORE=1)
bool spr_ignored(uint32_t addr) {
    static const bool off = std::getenv("ORACLE_NO_SPR_IGNORE") != nullptr;
    if (off) return false;
    for (const auto& [lo, hi] : g_ignored_spr_ranges)
        if (addr >= lo && addr < hi) return true;
    return false;
}
}  // namespace
std::vector<std::pair<uint32_t, std::function<void(EeCpu&)>>> g_extra_hooks;
std::function<void(EeInterpreter&)> g_pre_call;
bool g_oracle_front = false;
static bool g_vu_contact_reset = false;
void oracle_vu_contact_reset() { g_vu_contact_reset = true; }

std::unordered_set<uint32_t> g_ee_cover;  // ORACLE_COVER_EE=<end address>: the pcs the original ran (printed by check_fn)
OrigResult call_original(uint32_t addr, const std::vector<uint32_t>& args, const std::vector<float>& fargs) {
    static EeInterpreter ee;
    static bool hooked = false;
    if (!hooked) {  // render loops the port leaves to the engine: no-ops on both sides
        // the vsync wait (the kernel's semaphore: the host's frame boundary, ADR 0012) and the DMA kick of a frame's packets
        // (0x2a08c8, a TODO_PORT: the host sends them): the screen fade 0x1f9838 (task 0351) runs on both sides with them
        ee.set_hook(0x00122298, [](EeCpu&) {});
        ee.set_hook(OVF(0x002a08c8), [](EeCpu&) {});
        // the level setup's hardware / not yet ported parts (task 0293; the port has TODO_PORT there)
        for (uint32_t a : {0x00118a80u,
                           OVF(0x00266040u), OVF(0x002a1128u), OVF(0x00264740u), 0x00120558u, 0x00122658u})
            ee.set_hook(a, [](EeCpu&) {});
        // the kernel's SIF RPC calls go to the same host servers as the port's (libkernel
        // sceSifInitRpc / BindRpc / CallRpc / CheckStatRpc), libcdvd sceCdSync / sceCdCallback idle,
        // printf silent
        ee.set_hook(0x0011ab20, [](EeCpu&) {});
        ee.set_hook(0x0011aff8, [](EeCpu& c) { c.set32(2, sif_bind_rpc(c.r32(4), c.r32(5), c.r32(6))); });
        ee.set_hook(0x0011b1c8, [](EeCpu& c) {
            c.set32(2, sif_call_rpc(c.r32(4), c.r32(5), c.r32(6), c.r32(7), c.r32(8), c.r32(9), c.r32(10), c.r32(11), ee_read<uint32_t>(c.r32(29))));
        });
        ee.set_hook(0x0011b3b8, [](EeCpu& c) { c.set32(2, 0); });
        ee.set_hook(0x00118b20, [](EeCpu& c) { c.set32(2, static_cast<int32_t>(sif_set_dma(c.r32(4), c.r32(5)))); });
        ee.set_hook(0x00120c30, [](EeCpu& c) { c.set32(2, 0); });
        ee.set_hook(0x00120678, [](EeCpu& c) { c.set32(2, 0); });
        ee.set_hook(0x00116078, [](EeCpu& c) { c.set32(2, 0); });
        if (const char* t = std::getenv("ORACLE_MEMTRACE")) {  // pc:reg:nwords,... (reg a number: 4 = a0): the words at *reg printed when the pc is reached
            for (const char* p = t; *p != 0;) {
                char* e;
                const uint32_t pc = static_cast<uint32_t>(std::strtoul(p, &e, 16));
                if (*e != ':') break;
                const int reg = static_cast<int>(std::strtol(e + 1, &e, 10));
                if (*e != ':') break;
                const int n = static_cast<int>(std::strtol(e + 1, &e, 10));
                ee.add_trace_mem(pc, reg, n);
                p = (*e == ',') ? e + 1 : e;
            }
        }
        if (const char* t = std::getenv("ORACLE_TRACE")) {  // pcs where the FPU registers are printed
            for (const char* p = t; *p != 0;) { char* e; ee.add_trace_pc(static_cast<uint32_t>(std::strtoul(p, &e, 16))); p = (*e == ',') ? e + 1 : e; if (e == p && *p != 0) break; }
        }
        hooked = true;
    }
    static bool front_installed = false;  // the boot module's image (task 0308): Veldin's overlay hooks off
    if (g_oracle_front != front_installed) {
        static const uint32_t veldin_only[] = {0x002a08c8u, 0x00266040u, 0x002a1128u, 0x00264740u};
        for (uint32_t a : veldin_only) {
            if (g_oracle_front) ee.remove_hook(a);
            else ee.set_hook(a, [](EeCpu&) {});
        }
        if (g_oracle_front) ee.set_hook(0x002336a0u, [](EeCpu&) {});  // (the boot module's DMA kick, the overlays' 0x2a08c8)
        else ee.remove_hook(0x002336a0u);
        front_installed = g_oracle_front;
    }
    static std::vector<uint32_t> installed;
    if (installed != g_extra_noop_hooks) {  // per-batch no-op hooks (callees the port leaves TODO_PORT)
        for (uint32_t a : installed) ee.remove_hook(a);
        for (uint32_t a : g_extra_noop_hooks) ee.set_hook(a, [](EeCpu& c) { c.set32(2, 0); });
        installed = g_extra_noop_hooks;
    }
    static std::vector<uint32_t> installed_host;  // per-batch host hooks (set again each call)
    for (uint32_t a : installed_host) ee.remove_hook(a);
    installed_host.clear();
    {  // the vsync wait and the kick stay no-ops whatever a batch's lists named and removed (task 0351: the fade runs on both sides)
        auto named = [](uint32_t a) {
            for (uint32_t x : g_extra_noop_hooks) if (x == a) return true;
            return false;
        };
        if (!named(0x00122298)) ee.set_hook(0x00122298, [](EeCpu&) {});
        if (!g_oracle_front && !named(OVF(0x002a08c8))) ee.set_hook(OVF(0x002a08c8), [](EeCpu&) {});
    }
    for (const auto& [a, fn] : g_extra_hooks) {
        ee.set_hook(a, fn);
        installed_host.push_back(a);
    }
    {  // the world lighting routines (task 0324): the level load's calls are ported and run for real; the render pass's per-frame
       // calls (the lists 0x1c5400 / 0x1c7300 / 0x1bcfb0 -- the boot module's 0x1e1300 / 0x1e3200 -- come from the culling passes
       // the port has not ported) are TODO_PORT there: no-ops on both sides. (Task 0365: the boot module's title draw runs its passes
       // and their lighting, 0x234f98 / 0x237370 / 0x22a5e0, for real on both sides: no hooks on its image.)
        static std::vector<std::pair<uint32_t, uint32_t>> wl;  // (function, its frame list) installed
        std::vector<std::pair<uint32_t, uint32_t>> want;
        if (g_oracle_front) want = {};
        else want = {{OVF(0x00295350u), OV(0x001c5400u)}, {OVF(0x00297728u), OV(0x001c7300u)}, {OVF(0x00288908u), OV(0x001bcfb0u)}};
        if (want != wl) {
            for (const auto& w : wl) ee.remove_cond_hook(w.first);
            for (const auto& [fn, list] : want) ee.set_cond_hook(fn, [list = list](EeCpu& c) { return c.r32(4) == list; });
            wl = want;
        }
    }
    OrigResult r;
    int f = 12;
    for (float v : fargs) ee.cpu().fpr[f++] = v;
    int reg = 4;
    for (size_t k = 0; k < args.size(); ++k) {
        if (k < 8) ee.cpu().set32(reg++, static_cast<int32_t>(args[k]));
        else ee_write<int64_t>(0x02000000u - 64 + static_cast<uint32_t>(k - 8) * 8, static_cast<int32_t>(args[k]));  // the 9th.. on the stack, 8-byte slots (the interpreter's sp: its stack top 32 MB less 64)
    }
    ee.cpu().gpr[29].lo = 0;
    ee.cpu().gpr[24] = {};  // t8/t9: spilled to the scratchpad by 0x2531e0, zero in the port
    ee.cpu().gpr[25] = {};
    if (g_vu_contact_reset) {  // vf25..vf29 as the port's collide_vu_reset() leaves its copy
        for (int i = 25; i <= 29; ++i) ee.vu0().vf[static_cast<size_t>(i)] = {};
        g_vu_contact_reset = false;
    }
    ee.set_vu0_coexec(false);  // (a check that needs VU0 beside the EE turns it on in g_pre_call)
    if (const char* pt = std::getenv("ORACLE_PCTRACE")) {  // lo:hi[:call]: every pc of the range of the call-th original call (0: the first)
        static int ncall = 0;
        char* e;
        const uint32_t lo = static_cast<uint32_t>(std::strtoul(pt, &e, 16));
        const uint32_t hi = (*e == ':') ? static_cast<uint32_t>(std::strtoul(e + 1, &e, 16)) : lo + 0x1000;
        const int which = (*e == ':') ? static_cast<int>(std::strtol(e + 1, nullptr, 10)) : 0;
        ee.set_pc_trace(lo, hi, ncall++ == which);
    } else {
        ee.set_pc_trace(0, 0, false);
    }
    ee.set_cover(std::getenv("ORACLE_COVER_EE") != nullptr ? &g_ee_cover : nullptr);
    if (g_pre_call) g_pre_call(ee);
    if (const char* w = std::getenv("ORACLE_WATCH")) ee.set_watch(static_cast<uint32_t>(std::strtoul(w, nullptr, 0)));  // who writes this word
    static const uint64_t max_steps = std::getenv("ORACLE_MAXSTEPS") != nullptr ? std::strtoull(std::getenv("ORACLE_MAXSTEPS"), nullptr, 0) : 2'000'000'000;  // the level setup (batch 59) runs ~0.5 G instructions
    r.ok = ee.call(addr, {}, GP, max_steps);
    r.error = ee.error();
    if (!r.ok && std::getenv("ORACLE_STACK") != nullptr) {  // where the original was when it failed
        std::printf("  call stack:");
        for (uint32_t f : ee.call_stack()) std::printf(" %08x", f);
        std::printf("\n  recent:");
        for (uint32_t f : ee.recent_pcs()) std::printf(" %08x", f);
        std::printf("\n");
    }
    r.v0 = ee.cpu().gpr[2].lo;
    r.a3 = static_cast<uint32_t>(ee.cpu().gpr[7].lo);
    r.f0 = ee.cpu().fpr[0];
    return r;
}

OrigResult call_original(uint32_t addr, std::initializer_list<uint32_t> args, std::initializer_list<float> fargs) {
    return call_original(addr, std::vector<uint32_t>(args), std::vector<float>(fargs));
}

std::vector<uint8_t> save_range(uint32_t addr, uint32_t size) {
    std::vector<uint8_t> v(size);
    std::memcpy(v.data(), ee_ram_ptr(addr), size);
    return v;
}

void restore_range(uint32_t addr, const std::vector<uint8_t>& bytes) {
    std::memcpy(ee_ram_ptr(addr), bytes.data(), bytes.size());
}

size_t diff_words(const char* what, uint32_t base, const std::vector<uint8_t>& a, const std::vector<uint8_t>& b,
                  int max_print) {
    size_t n = 0;
    for (size_t i = 0; i + 4 <= a.size(); i += 4) {
        uint32_t x, y;
        std::memcpy(&x, &a[i], 4);
        std::memcpy(&y, &b[i], 4);
        // (a lone w lane; ORACLE_NO_LONE_W=1 compares them anyway: what a check's g_ignore_lone_w_lanes hides)
        static const bool lone_w_off = std::getenv("ORACLE_NO_LONE_W") != nullptr;
        if (x != y && g_ignore_lone_w_lanes && !lone_w_off && ((base + i) & 0xf) == 0xc && std::memcmp(&a[i - 12], &b[i - 12], 12) == 0) continue;
        if (x != y) {
            if (static_cast<int>(n) < max_print) {
                float fx, fy;
                std::memcpy(&fx, &x, 4);
                std::memcpy(&fy, &y, 4);
                std::printf("  %s: %08zx orig=%08x (%g) port=%08x (%g)\n", what, base + i, x, fx, y, fy);
            }
            ++n;
        }
    }
    return n;
}

namespace {
std::vector<uint8_t> g_baseline_ram, g_baseline_spr;
constexpr uint32_t RAM_SIZE = 32u << 20;
constexpr uint32_t STACK_LO = 0x01FE0000u;  // interpreter stack lives below 0x01FF0000

struct Full { std::vector<uint8_t> ram, spr; };
void save_full(Full& f) {  // reuses f's buffers: no per-case 32 MB allocations
    f.ram.assign(ee_get_ram_base(), ee_get_ram_base() + RAM_SIZE);
    f.spr.assign(ee_get_scratchpad_base(), ee_get_scratchpad_base() + SPR_SIZE);
}
void restore_full(const Full& f) {
    std::memcpy(ee_get_ram_base(), f.ram.data(), RAM_SIZE);
    std::memcpy(ee_get_scratchpad_base(), f.spr.data(), SPR_SIZE);
}
}  // namespace

void set_baseline_current() {
    g_baseline_ram.assign(ee_get_ram_base(), ee_get_ram_base() + RAM_SIZE);
    g_baseline_spr.assign(ee_get_scratchpad_base(), ee_get_scratchpad_base() + SPR_SIZE);
}

void restore_baseline() {
    std::memcpy(ee_get_ram_base(), g_baseline_ram.data(), RAM_SIZE);
    std::memcpy(ee_get_scratchpad_base(), g_baseline_spr.data(), SPR_SIZE);
}

// ORACLE_WRITES=1: per check, what the original actually changed in each case (the words of RAM below the interpreter's
// stack and of the scratchpad that differ from the case's starting state, the ignored words left out), how many cases
// changed nothing, how many wrote into the null page (0 .. 0x1000: a packet cursor or a pointer at 0) and how many moved
// the packet cursor 0x1611c0 (the bytes of packets it advanced over): a check whose original changes nothing compares nothing
namespace {
struct WriteCensus {
    int cases = 0, none = 0, null_page = 0, cursor = 0, only_seed = 0;
    uint64_t words = 0, cursor_bytes = 0;
    uint32_t min_words = 0xffffffffu, max_words = 0;
};
bool writes_on() {
    static const bool on = std::getenv("ORACLE_WRITES") != nullptr;
    return on;
}
}  // namespace

bool check_fn(const char* name, uint32_t orig, int iters, Ret ret, const std::function<Args()>& gen,
              const std::function<uint64_t(const Args&)>& port, Scope scope, uint32_t ignore_word, uint32_t ignore_word2) {
    int bad = 0;
    WriteCensus census;
    // ORACLE_FOCUS=<name>[:<case>] (task 0392): every other check of the batch only runs its generators (the host's random
    // sequence as in the whole batch), so the focused check sees the very cases of a full run, quickly; with :<case>, that
    // case alone is run (the generator still runs for the others)
    static const char* focus = std::getenv("ORACLE_FOCUS");
    std::string focus_name;
    int focus_case = -1;
    if (focus != nullptr) {
        focus_name = focus;
        if (const size_t c = focus_name.find(':'); c != std::string::npos) { focus_case = std::atoi(focus_name.c_str() + c + 1); focus_name.resize(c); }
    }
    for (int it = 0; it < iters; ++it) {
        if (scope == Scope::FullRam) restore_baseline();
        const Args args = gen();
        if (focus != nullptr && (focus_name != name || (focus_case >= 0 && it != focus_case))) continue;
        std::vector<uint8_t> before_t, a_t, b_t;
        static Full before_f, a_f, b_f;
        if (scope == Scope::TestArea) before_t = save_range(TEST_AREA, TEST_AREA_SIZE);
        else save_full(before_f);
        const OrigResult o = call_original(orig, args.i, args.f);
        if (!o.ok) {
            std::printf("  [%s] interpreter: %s (case %d, a0..a3 %08x %08x %08x %08x)\n", name, o.error.c_str(), it,
                        args.i.size() > 0 ? args.i[0] : 0u, args.i.size() > 1 ? args.i[1] : 0u, args.i.size() > 2 ? args.i[2] : 0u, args.i.size() > 3 ? args.i[3] : 0u);
            ++bad;
            break;
        }
        uint64_t pr;
        size_t nd = 0;
        if (scope == Scope::TestArea) {
            a_t = save_range(TEST_AREA, TEST_AREA_SIZE);
            restore_range(TEST_AREA, before_t);
            try {
                pr = port(args);
            } catch (const std::exception& e) {  // a port fault is a failing case, not the end of the run
                std::printf("  [%s] port: %s (case %d, a0..a3 %08x %08x %08x %08x)\n", name, e.what(), it, args.i.size() > 0 ? args.i[0] : 0u,
                            args.i.size() > 1 ? args.i[1] : 0u, args.i.size() > 2 ? args.i[2] : 0u, args.i.size() > 3 ? args.i[3] : 0u);
                ++bad;
                break;
            }
            b_t = save_range(TEST_AREA, TEST_AREA_SIZE);
            if (ignore_word) { std::memset(&a_t[ignore_word - TEST_AREA], 0, 4); std::memset(&b_t[ignore_word - TEST_AREA], 0, 4); }
            nd = diff_words(name, TEST_AREA, a_t, b_t, bad < bad_max ? 4 : 0);
        } else {
            save_full(a_f);
            if (writes_on()) {
                std::unordered_set<uint32_t> ign(g_ignored_words.begin(), g_ignored_words.end());
                ign.insert(ignore_word);
                ign.insert(ignore_word2);
                uint32_t n = 0, nnull = 0, nseed = 0;
                const uint32_t seed_word = ee_read<uint32_t>(0x0012f76c) + 0x58;
                for (uint32_t w = 0; w < STACK_LO; w += 4) {
                    if (std::memcmp(&a_f.ram[w], &before_f.ram[w], 4) == 0) continue;
                    if ((w >= 0x000f0000u && w < 0x000f8000u) || ign.count(w) != 0) continue;
                    ++n;
                    if (w < 0x1000) ++nnull;
                    if (w == seed_word) ++nseed;
                }
                for (uint32_t w = 0; w < SPR_SIZE; w += 4)
                    if (std::memcmp(&a_f.spr[w], &before_f.spr[w], 4) != 0 && ign.count(SPR_BASE + w) == 0 && !(w >= 0x800 && w < 0x840) && w < 0x3fa8 &&
                        !spr_ignored(SPR_BASE + w))
                        ++n;
                uint32_t c0, c1;
                std::memcpy(&c0, &before_f.ram[OV(0x001611c0u) & 0x01ffffffu], 4);
                std::memcpy(&c1, &a_f.ram[OV(0x001611c0u) & 0x01ffffffu], 4);
                ++census.cases;
                census.words += n;
                census.min_words = std::min(census.min_words, n);
                census.max_words = std::max(census.max_words, n);
                if (n == 0) ++census.none;
                if (n != 0 && n == nseed) ++census.only_seed;
                if (nnull != 0) ++census.null_page;
                if (c1 != c0) { ++census.cursor; census.cursor_bytes += c1 - c0; }
            }
            restore_full(before_f);
            try {
                pr = port(args);
            } catch (const std::exception& e) {  // a port fault is a failing case, not the end of the run
                std::printf("  [%s] port: %s (case %d, a0..a3 %08x %08x %08x %08x)\n", name, e.what(), it, args.i.size() > 0 ? args.i[0] : 0u,
                            args.i.size() > 1 ? args.i[1] : 0u, args.i.size() > 2 ? args.i[2] : 0u, args.i.size() > 3 ? args.i[3] : 0u);
                ++bad;
                break;
            }
            uint8_t* ram = ee_get_ram_base();
            for (uint32_t ig : {ignore_word, ignore_word2}) {
                if (ig >= SPR_BASE && ig < SPR_BASE + SPR_SIZE) {  // a scratchpad word
                    std::memset(&a_f.spr[ig - SPR_BASE], 0, 4); std::memset(ee_get_scratchpad_base() + (ig - SPR_BASE), 0, 4);
                } else if (ig) { std::memset(&a_f.ram[ig & 0x01ffffffu], 0, 4); std::memset(ram + (ig & 0x01ffffffu), 0, 4); }
            }
            // the world lighting routines (0x295350 / 0x297728 / 0x288908) spill the caller's s registers to the
            // scratchpad (0x70000800.., 0x70003fa8..: the culling passes spill s0..ra there): a whole-level run's caller holds values the port's has not
            for (uint32_t ig = SPR_BASE + 0x800; ig < SPR_BASE + 0x840; ig += 4) { std::memset(&a_f.spr[ig - SPR_BASE], 0, 4); std::memset(ee_get_scratchpad_base() + (ig - SPR_BASE), 0, 4); }
            for (uint32_t ig = SPR_BASE + 0x3fa8; ig < SPR_BASE + 0x4000; ig += 4) { std::memset(&a_f.spr[ig - SPR_BASE], 0, 4); std::memset(ee_get_scratchpad_base() + (ig - SPR_BASE), 0, 4); }
            // the PC version's enhancement blocks (src/game/enhance/advanced.cpp 0xf0000..0xf2000, graphics.cpp ..0xf6000, controls.cpp ..0xf8000: the kernel's first
            // megabyte, never the game's): written by the port's frame (the settings, the combo table copy), not part of the original
            std::memset(&a_f.ram[0x000f0000u], 0, 0x8000);
            std::memset(ram + 0x000f0000u, 0, 0x8000);
            for (uint32_t ig : g_ignored_words) {
                if (ig >= SPR_BASE && ig < SPR_BASE + SPR_SIZE) {  // (a scratchpad word)
                    std::memset(&a_f.spr[ig - SPR_BASE], 0, 4);
                    std::memset(ee_get_scratchpad_base() + (ig - SPR_BASE), 0, 4);
                } else {
                    std::memset(&a_f.ram[ig & 0x01ffffffu], 0, 4);
                    std::memset(ram + (ig & 0x01ffffffu), 0, 4);
                }
            }
            if (!g_ignored_spr_ranges.empty())  // (a check's own opt-in: the collision's dead scratch words, A2)
                for (uint32_t w = 0; w < SPR_SIZE; w += 4)
                    if (spr_ignored(SPR_BASE + w)) {
                        std::memset(&a_f.spr[w], 0, 4);
                        std::memset(ee_get_scratchpad_base() + w, 0, 4);
                    }
            if (std::memcmp(a_f.ram.data(), ram, STACK_LO) != 0) {
                b_f.ram.assign(ram, ram + STACK_LO);
                a_f.ram.resize(STACK_LO);
                const char* nds = std::getenv("ORACLE_DIFFS");  // words shown per failing case
                nd += diff_words(name, 0, a_f.ram, b_f.ram, bad < bad_max ? (nds != nullptr ? std::atoi(nds) : 6) : 0);
            }
            if (std::memcmp(a_f.spr.data(), ee_get_scratchpad_base(), SPR_SIZE) != 0) {
                b_f.spr.assign(ee_get_scratchpad_base(), ee_get_scratchpad_base() + SPR_SIZE);
                nd += diff_words(name, SPR_BASE, a_f.spr, b_f.spr, bad < bad_max ? 4 : 0);
            }
        }
        bool ok = nd == 0;
        if (nd != 0 && bad < bad_max) std::printf("  [%s] (case %d)\n", name, it);
        switch (ret) {
            case Ret::None: break;
            case Ret::Int32:
                if (static_cast<uint32_t>(o.v0) != static_cast<uint32_t>(pr)) {
                    if (bad < bad_max) std::printf("  [%s] v0 orig=%08x port=%08x\n", name, static_cast<uint32_t>(o.v0), static_cast<uint32_t>(pr));
                    ok = false;
                }
                break;
            case Ret::Int64:
                if (o.v0 != pr) {
                    if (bad < bad_max) std::printf("  [%s] v0 orig=%016llx port=%016llx\n", name, (unsigned long long)o.v0, (unsigned long long)pr);
                    ok = false;
                }
                break;
            case Ret::A3:
                if (o.a3 != static_cast<uint32_t>(pr)) {
                    if (bad < bad_max) std::printf("  [%s] a3 orig=%08x port=%08x\n", name, o.a3, static_cast<uint32_t>(pr));
                    ok = false;
                }
                break;
            case Ret::Float: {
                if (fbits(o.f0) != static_cast<uint32_t>(pr)) {
                    float pf; uint32_t pb = static_cast<uint32_t>(pr); std::memcpy(&pf, &pb, 4);
                    if (bad < bad_max) std::printf("  [%s] f0 orig=%.9g port=%.9g\n", name, o.f0, pf);
                    ok = false;
                }
                break;
            }
        }
        if (!ok) ++bad;
    }
    std::printf("%-26s 0x%08x  %s (%d/%d)\n", name, orig, bad ? "FAIL" : "ok", iters - bad, iters);
    if (writes_on() && census.cases != 0)
        std::printf("WRITES %s: %d cases; the original changed nothing in %d, only the random seed in %d; words changed min %u avg %llu max %u; "
                    "the null page written in %d; the packet cursor moved in %d (avg 0x%llx bytes)\n",
                    name, census.cases, census.none, census.only_seed, census.min_words,
                    static_cast<unsigned long long>(census.words / static_cast<uint64_t>(census.cases)), census.max_words, census.null_page,
                    census.cursor, static_cast<unsigned long long>(census.cursor != 0 ? census.cursor_bytes / static_cast<uint64_t>(census.cursor) : 0));
    if (const char* ce = std::getenv("ORACLE_COVER_EE")) {  // the instructions of [orig, <end>) no case ran
        const uint32_t end = static_cast<uint32_t>(std::strtoul(ce, nullptr, 16));
        uint32_t total = 0, hit = 0, run_lo = 0;
        bool in_run = false;
        std::printf("  uncovered:");
        for (uint32_t pc = std::getenv("ORACLE_COVER_FROM") != nullptr ? static_cast<uint32_t>(std::strtoul(std::getenv("ORACLE_COVER_FROM"), nullptr, 16)) : orig; pc < end; pc += 4) {  // (task 0385: ORACLE_COVER_FROM=<hex>, a callee's range)
            ++total;
            if (g_ee_cover.count(pc) != 0) { ++hit; if (in_run) { std::printf(" %x-%x", run_lo, pc - 4); in_run = false; } }
            else if (!in_run) { in_run = true; run_lo = pc; }
        }
        if (in_run) std::printf(" %x-%x", run_lo, end - 4);
        std::printf("\n  covered %u / %u instructions\n", hit, total);
        g_ee_cover.clear();
    }
    return bad == 0;
}

}  // namespace rac1::oracle
