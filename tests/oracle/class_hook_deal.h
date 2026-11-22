// The levels' class hooks checked through the level's dispatch (class_hook_call_level, the way the game's class_hook_*
// reach them), on states dealt for them (task Y1, after H2's p13h_hook_* of batch 220): included by the levels' class
// batches (120, 122, 124, 221).
//
// A hook table (class +0x2c) holds five functions: +0 .. +0xc take the moby, a position (a1), a target moby (a2) and a
// float (f12): the carry hooks of the Suck Cannon, which find the moby's carry record through the table's own +0x10 and
// switch over its state (+0x68: 0 .. 9, 8 refused) and progress (+0x6c); +0x10 returns that record (pvars + an offset,
// 0 without the moby or its pvars). The deal: the moby's state about the hook's carried state (or retired, 0xfe / 0xfd),
// the record's state each of 0 .. 9 and others, its progress about 1, no pvars in a sixth, the position the hero's, the
// moby's or one about it with the lob's w (0x3f9e0610) or not, the target none or a live moby, the moby itself 0 in half
// of the +0x10 cases, the frame at 50 Hz in a quarter.
#ifndef RAC1_TESTS_ORACLE_CLASS_HOOK_DEAL_H
#define RAC1_TESTS_ORACLE_CLASS_HOOK_DEAL_H

#include "ovl_oracle.h"

#include <game/ovl/carry_hooks_levels.h>
#include <game/ovl/novalis_mobys2.h>  // (TrRet)
#include <game/ovl/ovl_common.h>

#include <cstdio>
#include <initializer_list>
#include <stdexcept>
#include <vector>

namespace rac1::oracle {

// The hook at the played level's address FN, run through the level's dispatch, in the uniform ABI of the batches' hook tables
// (it throws when FN is not one of the level's own hooks: the check then fails).
template <uint32_t FN>
TrRet hook_dispatched(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float f12, float, float, float,
                      float, float, float, float) {
    int32_t r = 0x7eadbeef;
    if (!class_hook_call_level(FN, a0, a1, a2, f12, r)) throw std::runtime_error("not one of the level's class hooks");
    return TrRet{static_cast<uint32_t>(r), 0.0f};
}

// One hook to deal: its check's name, its address, its slot in the table, the offset of the class's carry record in the
// pvars (0: none dealt) and the moby state the hook sets while carried (0: none).
struct HookDeal {
    const char* name;
    uint32_t fn;
    uint32_t slot;
    uint32_t rec_off;
    uint32_t carried;
};

// The checks of `hooks`, `n` cases each, on the live mobys whose class's hook table is one of `tables`; `check` is the
// batch's own check (lNcheck(name, addr, n, ret, gen, port)), `level` restores its snapshot. Prints each subject (INFO).
template <class Check, class Level>
void class_hook_deal_checks(Check check, Level level, std::initializer_list<uint32_t> tables, std::initializer_list<HookDeal> hooks, int n) {
    level();
    std::vector<uint32_t> subj, live;
    for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
        if (ee_read<uint8_t>(m + 0x20) >= 0xfe) continue;
        live.push_back(m);
        const uint32_t cls = ee_read<uint32_t>(m + 0x24);
        if (cls == 0) continue;
        for (const uint32_t t : tables)
            if (ee_read<uint32_t>(cls + 0x2c) == t) subj.push_back(m);
    }
    for (const uint32_t m : subj) {
        const Vec4 q = ee_ref<Vec4>(m + 0x10);
        std::printf("INFO class hooks: moby %u uid %u class %u at (%.1f, %.1f, %.1f) state %u\n", (m - ee_read<uint32_t>(OV(0x0015ffd8))) >> 8,
                    ee_read<uint16_t>(m + 0xb2), ee_read<uint16_t>(m + 0xa6), q.x, q.y, q.z, ee_read<uint8_t>(m + 0x20));
    }
    for (const HookDeal& h : hooks) {
        if (subj.empty()) {
            std::printf("%-26s 0x%08x  no moby owns it on this level\n", h.name, h.fn);
            continue;
        }
        const HookDeal hk = h;
        check(hk.name, hk.fn, n, Ret::Int32, [=] {
            level();
            if (ri(0, 3) == 0) { wf(0x0015ed68, 1.2f); wf(0x0015ed6c, 0.02f); wf(0x0015ed70, 0.0004f); }  // (50 Hz)
            w32(OV(0x0015f5cc), R() & 0xffff);
            seed_rand();
            const uint32_t m = subj[static_cast<size_t>(ri(0, static_cast<int>(subj.size()) - 1))];
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const int s = ri(0, 9);
            w8(m + 0x20, s == 0 ? (coin() ? 0xfeu : 0xfdu) : (s < 5 && hk.carried != 0) ? hk.carried : u(ri(0, 15)));
            if (pv != 0 && hk.rec_off != 0) {
                const uint32_t rec = pv + hk.rec_off;
                const int rs = ri(0, 10);
                w16(rec + 0x68, rs == 10 ? u(ri(10, 0x7fff)) : u(rs));
                wf(rec + 0x6c, coin() ? rf(0.9f, 1.1f) : rf(0.0f, 2.0f));
            }
            if (ri(0, 5) == 0) w32(m + 0x78, 0);  // (no pvars: no record)
            ee_ref<Vec4>(T4) = Vec4{ee_read<float>(m + 0x10) + rf(-3, 3), ee_read<float>(m + 0x14) + rf(-3, 3), ee_read<float>(m + 0x18) + rf(-1, 3),
                                    coin() ? u2f(0x3f9e0610u) : rf(-1, 2)};
            const uint32_t tgt = ri(0, 3) == 0 ? 0u : live[static_cast<size_t>(ri(0, static_cast<int>(live.size()) - 1))];
            const int a1k = ri(0, 2);
            return Args{{(hk.slot == 0x10 && coin()) ? 0u : m, a1k == 0 ? 0x0013f3d0u : a1k == 1 ? m + 0x10 : T4, tgt}, {rf(-1, 3)}};
        }, [fn = hk.fn](A a) {
            int32_t r = 0x7eadbeef;
            if (!class_hook_call_level(fn, a.i[0], a.i[1], a.i[2], a.f[0], r)) throw std::runtime_error("not one of the level's class hooks");
            return r32(static_cast<uint32_t>(r));
        });
    }
}

}  // namespace rac1::oracle

#endif
