// RAC1_WORLD_CULL_FAST: the world passes' three culls (tfrag 0x294368, tie 0x295fa0, shrub 0x286f10) as written by hand
// (world_cull_tfrag.cpp, world_cull_tie.cpp, world_cull_shrub.cpp) or as transliterated (world_cull.cpp's *_tr, the reference).
//   0  the transliterations, as before;
//   1  the hand-written culls (the default); the transliteration still runs where they cannot (a VU0 micro program running,
//      RAC1_PERF_VU0_NF=0);
//   2  both, compared at every call: the hand-written cull runs inside a journal of EE memory, its results (the EE RAM, the
//      scratchpad, the VU0 registers and flags, the DMA registers, the visible lists) are kept, the memory put back, then the
//      transliteration runs and its results are compared; the game goes on with the transliteration's. Differences are printed
//      (the first few) and counted, and the counts printed at the end of the run (they must be 0). Slow: two 32 MB copies a call
//      (RAC1_HAND_CHECK_EVERY: about one call in n).
// orig: (host)
#include <game/ovl/world_cull.h>
#include <game/ovl/world_cull_hand.h>

#include <runtime/kernel/ee_memory.h>
#include <runtime/vu/vu0.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

namespace rac1 {

using world_hand::HandCheck;

namespace {

int cull_mode() {
    static const int m = [] {
        const char* e = std::getenv("RAC1_WORLD_CULL_FAST");
        return e != nullptr ? std::atoi(e) : 1;
    }();
    return m;
}

constexpr uint32_t RAM_BYTES = 0x02000000u;  // the EE's 32 MB (what the oracle compares)

// the state a cull leaves behind, beside EE memory
struct Side {
    VUState vu;  // (vf, acc, q and the flags compared)
    std::array<uint32_t, 0x500 / 4> dma{};
    WorldVisible visible;
    void take() {
        const VUState& s = vu0::global().st;
        vu.vf = s.vf;
        vu.acc = s.acc;
        vu.q = s.q;
        vu.macflag = s.macflag;
        vu.statusflag = s.statusflag;
        vu.clipflag = s.clipflag;
        dma = eer::hwregs().spr;
        visible = world_visible();
    }
    void put() const {
        VUState& s = vu0::global().st;
        s.vf = vu.vf;
        s.acc = vu.acc;
        s.q = vu.q;
        s.macflag = vu.macflag;
        s.statusflag = vu.statusflag;
        s.clipflag = vu.clipflag;
        eer::hwregs().spr = dma;
        world_visible_restore(visible);
    }
};

std::vector<HandCheck*>& checks() {
    static std::vector<HandCheck*> v;
    return v;
}

// compare two byte ranges word by word; the first differences printed (limited over the run)
size_t compare(const HandCheck& c, const char* what, uint32_t base, const uint8_t* ref, const uint8_t* fast, size_t n) {
    static int printed = 0;
    size_t diffs = 0;
    for (size_t i = 0; i < n; i += 4) {
        uint32_t a, b;
        std::memcpy(&a, ref + i, 4);
        std::memcpy(&b, fast + i, 4);
        if (a == b) continue;
        if (diffs++ < 8 && printed < 200) {
            ++printed;
            std::fprintf(stderr, "%s %s: %s 0x%08zx: transliteration %08x, hand-written %08x\n", c.tag, c.name, what, base + i, a, b);
        }
    }
    return diffs;
}

bool same_visible(const WorldVisible& a, const WorldVisible& b) {
    if (a.tie != b.tie || a.shrub != b.shrub || a.tfrag.size() != b.tfrag.size()) return false;
    for (size_t i = 0; i < a.tfrag.size(); ++i)
        if (a.tfrag[i].index != b.tfrag[i].index || a.tfrag[i].lod != b.tfrag[i].lod) return false;
    return true;
}

}  // namespace

// orig: (host) the checks' counts (the host's run end: it leaves with _Exit, no atexit handler runs)
void world_cull_check_report() {
    for (const HandCheck* c : checks())
        if (c->calls != 0) std::fprintf(stderr, "%s check (%s=2): %s, %llu calls, %llu differing\n", c->tag, c->var, c->name, c->calls, c->differing);
}

// orig: (host) see world_cull_hand.h
void world_hand::hand_check(HandCheck& c, const std::function<bool()>& hand, const std::function<void()>& tr) {
    // RAC1_HAND_CHECK_EVERY=n: about one call in n checked (a random pick, not every n-th: a routine called a fixed number of
    // times a frame would be checked at the same call each frame)
    static const uint32_t every = [] {
        const char* e = std::getenv("RAC1_HAND_CHECK_EVERY");
        const int n = e != nullptr ? std::atoi(e) : 1;
        return n > 1 ? static_cast<uint32_t>(n) : 1u;
    }();
    static uint32_t pick = 0x2545f491u;
    if (every > 1) {
        pick ^= pick << 13, pick ^= pick >> 17, pick ^= pick << 5;
        if (pick % every != 0) return tr();
    }
    if (c.calls++ == 0) checks().push_back(&c);
    static std::unique_ptr<uint8_t[]> ram_fast(new uint8_t[RAM_BYTES]);
    static uint8_t spr_fast[SPR_SIZE];
    Side before, fast, ref;
    before.take();
    ee_journal_begin();
    if (!hand()) std::fprintf(stderr, "%s %s: the hand-written routine did not run\n", c.tag, c.name);
    fast.take();
    std::memcpy(ram_fast.get(), ee_get_ram_base(), RAM_BYTES);
    std::memcpy(spr_fast, ee_get_scratchpad_base(), SPR_SIZE);
    ee_journal_restore();
    before.put();
    tr();
    ref.take();
    size_t d = compare(c, "RAM", 0, ee_get_ram_base(), ram_fast.get(), RAM_BYTES);
    d += compare(c, "scratchpad", SPR_BASE, ee_get_scratchpad_base(), spr_fast, SPR_SIZE);
    d += compare(c, "VU0 vf", 0, reinterpret_cast<const uint8_t*>(ref.vu.vf.data()), reinterpret_cast<const uint8_t*>(fast.vu.vf.data()), sizeof(ref.vu.vf));
    d += compare(c, "VU0 acc", 0, reinterpret_cast<const uint8_t*>(&ref.vu.acc), reinterpret_cast<const uint8_t*>(&fast.vu.acc), 16);
    const uint32_t rf[4] = {f2u(ref.vu.q), ref.vu.macflag, ref.vu.statusflag, ref.vu.clipflag};
    const uint32_t ff[4] = {f2u(fast.vu.q), fast.vu.macflag, fast.vu.statusflag, fast.vu.clipflag};
    d += compare(c, "VU0 q / mac / status / clip", 0, reinterpret_cast<const uint8_t*>(rf), reinterpret_cast<const uint8_t*>(ff), sizeof rf);
    d += compare(c, "DMA registers", 0x1000d000u, reinterpret_cast<const uint8_t*>(ref.dma.data()), reinterpret_cast<const uint8_t*>(fast.dma.data()), sizeof ref.dma);
    if (!same_visible(ref.visible, fast.visible)) {
        ++d;
        std::fprintf(stderr, "%s %s: the visible lists differ\n", c.tag, c.name);
    }
    if (d != 0) ++c.differing;
}

namespace {

HandCheck g_checks[3] = {{"[world-cull]", "RAC1_WORLD_CULL_FAST", "tfrag cull"},
                         {"[world-cull]", "RAC1_WORLD_CULL_FAST", "tie cull"},
                         {"[world-cull]", "RAC1_WORLD_CULL_FAST", "shrub cull"}};

void run(int which, bool (*hand)(), void (*tr)()) {
    const int m = cull_mode();
    if (m == 0) return tr();
    // (the LOD slider's / wide view's re-runs are inside the renderer's own journal: no check there)
    if (m == 1 || ee_journal_active()) {
        if (!hand()) tr();
        return;
    }
    if (!vu0l::usable()) return tr();
    hand_check(g_checks[which], hand, tr);
}

}  // namespace

// orig: 0x00294368 -- see world_cull.cpp's world_tfrag_cull_tr
void world_tfrag_cull() { run(0, world_hand::tfrag_cull, world_tfrag_cull_tr); }
// orig: 0x00295fa0
void world_tie_cull() { run(1, world_hand::tie_cull, world_tie_cull_tr); }
// orig: 0x00286f10
void world_shrub_cull() { run(2, world_hand::shrub_cull, world_shrub_cull_tr); }

}  // namespace rac1
