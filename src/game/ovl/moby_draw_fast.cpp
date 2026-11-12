// RAC1_PERF_MOBY_DRAW: the moby pass's draw loop 0x2559c0 in its faster form (moby_draw_hand.cpp: the VU0 registers as a local
// copy, the memory accessors' fast paths inline) or as transliterated (moby_draw.cpp's moby_draw_loop_tr, the reference).
//   0  the transliteration, as before;
//   1  the faster form (the default); the transliteration still runs where it cannot (a VU0 micro program running,
//      RAC1_PERF_VU0_NF=0);
//   2  both, compared at every call (world_cull_hand.h's hand_check: the EE RAM, the scratchpad, the VU0 registers and flags,
//      the DMA registers), and besides: the ring position returned and every call of the draw loop's observer (the MRP scene's:
//      its arguments and the skinning record it is handed); the game and the MRP go on with the transliteration's. Inside the
//      widened view's / LOD slider's second pass (a memory journal already) the faster form runs unchecked, as the culls do.
// orig: (host)
#include <game/ovl/moby_draw.h>
#include <game/ovl/moby_draw_hand.h>
#include <game/ovl/world_cull_hand.h>

#include <runtime/kernel/ee_memory.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace rac1 {

namespace {

int draw_mode() {
    static const int m = [] {
        const char* e = std::getenv("RAC1_PERF_MOBY_DRAW");
        return e != nullptr ? std::atoi(e) : 1;
    }();
    return m;
}

world_hand::HandCheck g_check{"[moby-draw]", "RAC1_PERF_MOBY_DRAW", "moby draw loop"};

// the observer's calls of one run (check mode): its arguments, the record it is handed and the loop's notes for it (the
// program chosen, the parts written)
struct ObsCall {
    uint32_t a[8];
    std::vector<uint8_t> rec;
    uint32_t mscal;
    MobyDrawParts parts;
    bool operator==(const ObsCall& o) const {
        return std::memcmp(a, o.a, sizeof a) == 0 && rec == o.rec && mscal == o.mscal && std::memcmp(&parts, &o.parts, sizeof parts) == 0;
    }
};
std::vector<ObsCall> g_calls[2];
MobyDrawObserver g_real = nullptr;
template <int K>
void record(uint32_t moby, uint32_t cb, uint32_t ce, uint32_t ring, uint32_t spr_rec, uint32_t qwords, uint32_t far_lod, uint32_t deferred) {
    ObsCall c{{moby, cb, ce, ring, spr_rec, qwords, far_lod, deferred}, {}, g_moby_draw_mscal, g_moby_draw_parts};
    const uint32_t n = qwords * 16;
    if (n != 0 && spr_rec >= SPR_BASE && spr_rec - SPR_BASE <= SPR_SIZE - n) {
        const uint8_t* p = ee_get_scratchpad_base() + (spr_rec - SPR_BASE);
        c.rec.assign(p, p + n);
    }
    g_calls[K].push_back(std::move(c));
    if (K == 1 && g_real != nullptr) g_real(moby, cb, ce, ring, spr_rec, qwords, far_lod, deferred);  // (the transliteration's: the MRP's)
}

}  // namespace

// orig: 0x002559c0 -- see moby_draw_hand.h
uint32_t moby_draw_loop(uint32_t first, uint32_t ring, int32_t count, uint32_t defer) {
    const int m = draw_mode();
    if (m == 0 || !vu0l::usable()) return moby_draw_loop_tr(first, ring, count, defer);
    if (m == 1 || ee_journal_active()) return moby_hand::draw_loop(first, ring, count, defer);
    g_real = g_moby_draw_observer;
    g_calls[0].clear();
    g_calls[1].clear();
    uint32_t r_hand = 0, r_tr = 0;
    const unsigned long long before = g_check.differing, calls = g_check.calls;
    world_hand::hand_check(
        g_check,
        [&] {
            g_moby_draw_observer = &record<0>;
            r_hand = moby_hand::draw_loop(first, ring, count, defer);
            g_moby_draw_observer = g_real;
            return true;
        },
        [&] {
            g_moby_draw_observer = &record<1>;
            r_tr = moby_draw_loop_tr(first, ring, count, defer);
            g_moby_draw_observer = g_real;
        });
    if (g_check.calls != calls) {  // (checked: not skipped by RAC1_HAND_CHECK_EVERY)
        const bool same = r_hand == r_tr && g_calls[0] == g_calls[1];
        if (!same) {
            static int printed = 0;
            if (printed++ < 20)
                std::fprintf(stderr, "[moby-draw] the returned ring (%08x / %08x) or the observer's calls (%zu / %zu) differ\n", r_tr, r_hand, g_calls[1].size(),
                             g_calls[0].size());
            if (g_check.differing == before) ++g_check.differing;
        }
    }
    return r_tr;
}

}  // namespace rac1
