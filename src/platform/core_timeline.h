// (testing, the Wii U, 2026-10-30) the cores' timeline of a host frame: wall-clock marks (OSGetTime: the time base, no allocation, no
// lock) the main thread, the game thread, the chain's reader and the world's preparation leave, and the chain reader's time by kind.
// ports/wiiu/game/wiiu_gfx.cpp turns them into offsets from the cycle's start (the prepare: the frame's chain started), averaged over
// 30 cycles, on the log ("[core0]"), with the timing on (RAC1_WIIU_TIMING=1 or the debug menu). Elsewhere (the PC) every call is empty.
#pragma once

#include <cstdint>

#if defined(__WIIU__)
#include <coreinit/core.h>
#include <coreinit/time.h>

#include <atomic>
#endif

namespace rac1::ctl {

// the marks, one time each a cycle (the last one written counts; the cycle's start is the prepare's, wiiu_gfx.cpp rac1_wiiu_cycle)
enum Ev : int {
    kCycle,      // (main) the prepare's start: the frame before's chain joined
    kChainKick,  // (main) mrp_chain_start returned (the frame's chain copied, its reader woken)
    kPrepEnd,    // (main) the prepare's end (the mobys', the world's: the world's job dispatched)
    kGt0,        // (game thread) its steps' start
    kGt1,        // (game thread) their end
    kGfx,        // (main) wiiu_gfx_frame: the drawing's start
    kWJoin0,     // (main) the "world's worker join" at the drawing's start
    kWJoin1,
    kCJoin0,     // (main) the chain's join
    kCJoin1,
    kFx1,        // (main) the effects' prepare and the 2D's build done
    kGpu1,       // (main) the wait for the GPU's frame before done
    kBegin1,     // (main) WHBGfxBeginRender (the swap's wait) done
    kDJoin0,     // (main) the world's join before its draws (draw_frame, the TV's)
    kDJoin1,
    kDraw1,      // (main) the TV's frame drawn
    kFrame1,     // (main) wiiu_gfx_frame's end
    kGtJoin0,    // (main) the game thread joined
    kGtJoin1,
    kChain0,     // (the chain's reader) its reading's start
    kChain1,     //   and end
    kPrep0,      // (the world's job, any thread) its start
    kPrepW0,     //   its wait for the chain's reader (RAC1_WIIU_PREP_SPLIT's tail on the worker)
    kPrepW1,
    kHead1,      //   the first half (wiiu_prep_head) done
    kTail0,      //   the second half's start (wiiu_prep_tail)
    kPrep1,      //   its end
    kEvN
};

// the chain reader's time by kind (exclusive: a kind's time stops while one inside it runs)
enum Cat : int {
    kCatVif,       // the walk: the tags, the VIF codes, UNPACK / MPG, the GIF's own state, the primitives' assembly - the rest
    kCatVuRange,   // the VU1 programs run inside the effects' / program 7's ranges (their kicks kept)
    kCatVuOther,   // the VU1 programs run elsewhere (their kicks read for the GS registers only)
    kCatKick,      // the programs' kicks' GIF data (registers, vertices, primitives)
    kCatTex,       // the textures: texture_at_kick (snaps, decodes, the ring), the screen's picture
    kCatXfer,      // the GS local memory: the IMAGE transfers' stores
    kCatN
};

#if defined(__WIIU__)
inline std::atomic<bool> g_on{false};  // (set by wiiu_gfx_frame: the timing on)
inline bool on() { return g_on.load(std::memory_order_relaxed); }
inline std::atomic<uint64_t> g_ev[kEvN] = {};
inline std::atomic<int> g_prep_core{-1};  // (the core the world's first half ran on)
inline std::atomic<int> g_tail_core{-1};  // (and its second half)
inline uint64_t now() { return static_cast<uint64_t>(OSGetTime()); }
inline void mark(Ev e) {
    if (on()) g_ev[e].store(now(), std::memory_order_relaxed);
}

// the chain reader's (one reading at a time: its thread, or the main thread's synchronous reading - joined before taken)
struct ChainProf {
    int cur = kCatVif;
    uint64_t t = 0;
    uint64_t acc[kCatN] = {};
    uint32_t runs[2] = {};  // (programs run in the ranges, elsewhere)
    struct Prog {
        uint32_t pc = 0;
        uint8_t range = 0;
        uint32_t runs = 0;
        uint64_t ticks = 0;  // (inclusive: its kicks, their textures)
    };
    static constexpr int kProgs = 24;
    Prog prog[kProgs] = {};
    int nprog = 0;
    uint32_t prog_other = 0;  // (runs past the table)
    uint32_t vu7_kicks = 0, vu7_verts = 0;  // ((2026-10-31) program 7's kept batches' kicks read without their vertices: mrp_chain.cpp vu7_kick)
};
inline ChainProf g_cp;
inline void chain_begin() {
    if (!on()) return;
    g_cp.t = now();
    g_cp.cur = kCatVif;
    g_ev[kChain0].store(g_cp.t, std::memory_order_relaxed);
}
inline void chain_end() {
    if (!on()) return;
    const uint64_t n = now();
    g_cp.acc[g_cp.cur] += n - g_cp.t;
    g_cp.t = n;
    g_ev[kChain1].store(n, std::memory_order_relaxed);
}
// a kind's time starts (the one before paused): what to give cat_pop
inline int cat_push(int c) {
    if (!on()) return -1;
    const uint64_t n = now();
    g_cp.acc[g_cp.cur] += n - g_cp.t;
    g_cp.t = n;
    const int prev = g_cp.cur;
    g_cp.cur = c;
    return prev;
}
inline void cat_pop(int prev) {
    if (prev < 0) return;
    const uint64_t n = now();
    g_cp.acc[g_cp.cur] += n - g_cp.t;
    g_cp.t = n;
    g_cp.cur = prev;
}
// a program's run: its start address, in a range or not, its ticks
inline void prog_note(uint32_t pc, bool range, uint64_t ticks) {
    if (!on()) return;
    ++g_cp.runs[range ? 0 : 1];
    for (int i = 0; i < g_cp.nprog; ++i) {
        ChainProf::Prog& p = g_cp.prog[i];
        if (p.pc == pc && p.range == (range ? 1 : 0)) {
            ++p.runs;
            p.ticks += ticks;
            return;
        }
    }
    if (g_cp.nprog < ChainProf::kProgs) g_cp.prog[g_cp.nprog++] = ChainProf::Prog{pc, static_cast<uint8_t>(range ? 1 : 0), 1, ticks};
    else ++g_cp.prog_other;
}
// (2026-10-31) a kept batch's kick read by mrp_chain.cpp's vu7_kick: its vertices
inline void vu7_kick_note(uint32_t verts) {
    if (!on()) return;
    ++g_cp.vu7_kicks;
    g_cp.vu7_verts += verts;
}
inline int core_id() { return static_cast<int>(OSGetCoreId()); }
#else
inline bool on() { return false; }
inline uint64_t now() { return 0; }
inline void mark(Ev) {}
inline void chain_begin() {}
inline void chain_end() {}
inline int cat_push(int) { return -1; }
inline void cat_pop(int) {}
inline void prog_note(uint32_t, bool, uint64_t) {}
inline void vu7_kick_note(uint32_t) {}
inline int core_id() { return -1; }
#endif

// a kind's time over a scope
struct CatScope {
    int prev;
    explicit CatScope(int c) : prev(cat_push(c)) {}
    ~CatScope() { cat_pop(prev); }
    CatScope(const CatScope&) = delete;
    CatScope& operator=(const CatScope&) = delete;
};

}  // namespace rac1::ctl
