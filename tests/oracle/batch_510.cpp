// Oracle batch 510 (stage 4, task F7): the shared ports of the class code several levels carry a copy of
// (src/game/ovl/shared_*.cpp), each against every level's own copy at that level's own address. Run it on each level that
// has a copy: ORACLE_LEVEL=<n> build/tests/ovl_oracle --batch 510 for n in 2, 4, 7, 8, 9, 10, 12 and 13 (needs the ISO; no
// dumps). A check prints "skipped" on a level without its copy. The class updates run on the level's own mobys of the
// class after the first gameplay frame (as the levels' class batches 109, 121, 122 ... do), through every state, perturbed;
// the helpers through their callers and on their own with arguments built from the level's state.
//
// The shared ports and their copies (each level's address):
//   bg_ship2_update      class 1213 / 1973   L2:0x002ece18 L9:0x00306720 L10:0x002ebb08
//   bg_ship_exhaust      (helper)            L2:0x002eca30 / 0x002ed420 / 0x002ed658, L9:0x00306338 / 0x00306d28 / 0x00306f60,
//                                            L10:0x002ec110 / 0x002ec348
//   bg_ship_glow_prep    (helper)            L2:0x002ec3f0 / 0x002ecd50, L9:0x00305cf8 / 0x00306658, L10:0x002eba40
//   mover_platform_update  class 1129 / 104 / 106     L7:0x0031aee0 L13:0x002c32d0
//   mover_door_update      class 1063 / 129 / 130 ..  L7:0x00310df0 L13:0x002c8390
//   puffer_update          class 648                  L8:0x002f5830 L10:0x002c74b8
//   homing_missile_launch  (helper)                   L8:0x002e7eb0 L12:0x002ef2e8
//   homing_missile_update  class 458                  L8:0x002e8210 L12:0x002ef648 (on missiles the launch spawns)
//   shell_launch           (helper)                   L9:0x00307ba8 L10:0x002e68b8
//   shell_update           class 1258                 L9:0x00307d68 L10:0x002e6a78 (on shells the launch spawns)
//   shot_launch            (helper)                   L4:0x002aee30 L12:0x002d4150
//   shot_update            class 184                  L4:0x002af058 L12:0x002d4378 (on shots the launch spawns)
//   tracer_update          class 1292                 L8:0x00307298 L9:0x00308f08 (on rounds spawned for the check)

#include "ovl_oracle.h"

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_bg_ships.h>
#include <game/ovl/shared_movers.h>
#include <game/ovl/shared_puffer.h>
#include <game/ovl/shared_shell.h>
#include <game/ovl/shared_tracer.h>
#include <game/ovl/shared_homing_missile.h>
#include <runtime/kernel/disc.h>

#include <cmath>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {

// one level's copy of a shared function: the level, the copy's address there
struct Copy {
    int level;
    uint32_t addr;
};
// this level's copy among `copies` (the first one listed for the level), 0 when the level has none
uint32_t copy_here(std::initializer_list<Copy> copies, int nth = 0) {
    for (const Copy& c : copies)
        if (c.level == g_oracle_level && nth-- == 0) return c.addr;
    return 0;
}

// A check of a shared port against this level's copy at `addr` (0: the level has none, skipped).
void fcheck(const char* name, uint32_t addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (addr == 0) {
        std::printf("%-26s skipped (no copy on level %d)\n", name, g_oracle_level);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L%d:0x%08x\n", name, g_oracle_level, addr);
    if (!check_fn(name, addr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
}  // namespace

void batch_510() {
    if (g_only_batch != 510) return;
    g_batch = 510;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    for (uint32_t k = 0; k < 12; k += 4) g_ignored_words.push_back(0x00174610u + k);  // (segment_collide's hit point: see batch 121)
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // not compared: the null page and the scratchpad words the pose function spills the caller's $t8 / $t9 into (batch 121)
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);

    // 989snd's IOP server (as batch 121's): replies a function of the command; the stream system's disc reads done into EE RAM
    {
        auto result = [](uint32_t id, uint32_t a0) -> uint32_t {
            const uint32_t h = (id * 0x9e3779b1u) ^ (a0 * 0x85ebca6bu);
            return ((h >> 7) % 4 == 0) ? 0u : ((h >> 3) | 1u);
        };
        sif_rpc_register(0x123456, [result](uint32_t fno, uint32_t send, uint32_t, uint32_t recv, uint32_t) {
            if (fno == 0x4d) {
                const uint32_t n = ee_read<uint32_t>(send);
                uint32_t p = send + 4;
                for (uint32_t i = 0; i < n; ++i) {
                    const uint32_t id = ee_read<uint16_t>(p), size = ee_read<uint16_t>(p + 2);
                    ee_write<uint32_t>(recv + 4 + i * 4, result(id, size != 0 ? ee_read<uint32_t>(p + 4) : 0u));
                    if (id == 0x38) {
                        disc_read_ee(ee_read<uint32_t>(p + 0xc), ee_read<uint32_t>(p + 4), ee_read<uint32_t>(p + 8));
                        ee_write<uint32_t>(0x00137b10, 0);
                        ee_write<uint32_t>(0x00137b00, 0);
                    }
                    p += 4 + ((size + 3) & ~3u);
                }
                ee_write<uint32_t>(recv, 0xffffffffu);
                ee_write<uint32_t>(recv + 4 + n * 4, 0xffffffffu);
                return;
            }
            ee_write<uint32_t>(recv + 4, result(fno, send != 0 ? ee_read<uint32_t>(send) : 0u));
            ee_write<uint32_t>(recv, 0xffffffffu);
            ee_write<uint32_t>(recv + 8, 0xffffffffu);
        });
    }
    // the level after its first gameplay frame (the port's), the hero's equipment existing
    static std::vector<uint8_t> snap;
    snap.clear();
    auto level = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty()) {
            veldin_boot();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            gameplay_frame();
            snap.assign(ram, ram + 0x02000000);
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    // the level's live mobys whose update function is `fn` (the level's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // a class update on the level's own mobys, perturbed by `vary` (a state that sends the port through a pointer outside RAM
    // is dealt again, as in batch 121)
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 12) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (fn == 0) { fcheck(name, 0, 0, Ret::None, nullptr, nullptr); return; }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
        level();
        const std::vector<uint32_t> ms = mobys_with(fn);
        if (ms.empty()) { std::printf("%-26s L%d:0x%08x  no moby of the class on this level\n", name, g_oracle_level, fn); ++g_skipped; return; }
        fcheck(name, fn, n, Ret::None, [level, ms, vary, port] {
            static std::vector<uint8_t> saved;
            uint32_t m = 0;
            for (int attempt = 0; attempt < 40; ++attempt) {
                level();
                m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                vary(m);
                uint8_t* ram = ee_get_ram_base();
                saved.assign(ram, ram + 0x02000000);
                bool ok = true;
                const uint32_t stack_top = ee_stack_ptr();
                try { port(m); } catch (const std::exception&) { ok = false; }
                ee_stack_ptr() = stack_top;
                std::memcpy(ram, saved.data(), saved.size());
                if (ok) break;
            }
            return Args{{m}, {}};
        }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
    };
    // a hit on the moby: its slot in the hit table (0x178100, 64-byte entries) naming it, with a damage mask and a damage +0x2c
    auto give_hit = [](uint32_t m, uint32_t kinds) {
        if (ri(0, 3) == 0) { w8(m + 0xa4, 0xff); return; }
        const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
        w8(m + 0xa4, idx);
        w32(e + 0x34, ri(0, 8) == 0 ? ee_read<uint32_t>(OV(0x0015ffd8)) : m);
        w32(e + 0x24, ri(0, 2) == 0 ? (R() & ~kinds) : (kinds | (R() & 0xff)));
        { static const float dm[] = {-1.0f, 0.0f, 0.5f, 5.0f, 12.0f}; wf(e + 0x2c, dm[ri(0, 4)]); }
        wfrand(e, 4, -1, 1);
        w32(e + 0x28, u(ri(0, 20)));
    };
    // the hero near the moby or far from it
    auto hero_near = [](uint32_t m) {
        Vec4 q = ee_ref<Vec4>(m + 0x10);
        const float r = ri(0, 2) == 0 ? rf(0, 5) : rf(5, 15);
        const float a = rf(-3.1f, 3.1f);
        q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-1, 3);
        ee_ref<Vec4>(0x0013f3d0) = q;
    };
    // the generic preparation of batch 121: state `st` (after the port's state 0), pass 1 / 2 a hit and the pvars moved as typed,
    // the hero near (passes 0 and 2), the frame counter and the random seed varied
    auto generic_prep = [give_hit, hero_near](uint32_t m, int st, int pass, void (*fn)(uint32_t)) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t P = 0x0013f350;
        if (st != 0) {
            w8(m + 0x20, 0);
            const uint32_t sp0 = ee_stack_ptr();
            try { fn(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
        }
        w8(m + 0x20, u(st));
        if (pass >= 1) {
            if (coin()) { w8(m + 0x53, 0); w8(m + 0x52, 0); } w8(m + 0xbc, u(ri(0, 2)));
            wf(m + 0x48, rf(-3.1f, 3.1f));
            { Vec4 q = ee_ref<Vec4>(m + 0x10); const float r = ri(0, 3) == 0 ? rf(0, 6) : (coin() ? rf(6, 30) : rf(30, 80)); const float a = rf(-3.1f, 3.1f);
              q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-4, 12); ee_ref<Vec4>(P + 0x80) = q; }
            for (uint32_t o = 0x20; o < 0x200; o += 4) {
                if (ri(0, 9) != 0) continue;
                const uint32_t v = ee_read<uint32_t>(pv + o), ex = v & 0x7f800000u;
                if (ex >= 0x30000000u && ex < 0x50000000u) wf(pv + o, ee_read<float>(pv + o) * rf(0.5f, 1.5f));
                else if (v >= 1 && v < 16) w32(pv + o, u(ri(1, static_cast<int>(v) + 2)));
            }
            give_hit(m, ri(0, 1) ? 0x210000u : 0x330000u);
            if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
                const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                w8(e + 0x28, u(ri(0, 11))); w8(e + 0x29, u(ri(0, 4))); w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
                wf(e + 0x1c, coin() ? 5627.95f : 0.0f);
                w32(e + 0x20, ri(0, 3) == 0 ? 0u : ee_read<uint32_t>(P + 0x2080));
                wf(e + 0x2c, coin() ? rf(0.1f, 6) : 0.0f);
            }
        }
        if (pass != 1) hero_near(m);
        w32(OV(0x0015f5cc), R() & 0xffff);
        seed_rand();
    };
    // every state 0 .. `states`-1, three passes each
    auto states_check = [classcheck, generic_prep](const char* name, uint32_t fn, void (*port)(uint32_t), int states, int n = 12,
                                                   std::function<void(uint32_t)> extra = nullptr) {
        if (fn == 0) { classcheck(name, 0, port, nullptr, n); return; }
        for (int st = 0; st < states; ++st)
            for (int pass = 0; pass < 3; ++pass)
                classcheck(name, fn, port, [st, pass, port, generic_prep, extra](uint32_t m) {
                    generic_prep(m, st, pass, port);
                    if (extra) extra(m);
                }, n);
    };

    // the moby range's w lanes the original's VU0 code leaves (the debris / spawns) and the particle pool's velocity w lanes
    if (g_oracle_level != 0) {
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x80000; a += 0x40) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
        const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
        for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c);
    }
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80};

    // ---- the background ships (Aridia, Gaspar, Orxon)
    {
        const uint32_t upd = copy_here({{2, 0x002ece18}, {9, 0x00306720}, {10, 0x002ebb08}});
        states_check("bg_ship2", upd, bg_ship2_update, 4, 12, [](uint32_t m) {
            if (ri(0, 3) != 0) w8(m + 0x31, u(ri(1, 3)));  // (drawn last frame: the exhaust and the glow run)
            if (ri(0, 3) == 0) w32(0x0015ed84, u(ri(0, 3) == 0 ? 10 : ri(0, 2) == 0 ? 9 : 2));  // (the game's level number: the branches of Aridia, Gaspar, Orxon)
            if (coin()) { w32(0x0015edf4, u(ri(0, 4))); w32(0x0015edfc, u(ri(0, 6))); w8(0x0013d40a, u(ri(0, 1))); w8(0x0013d417, u(ri(0, 1))); w32(0x001413d4, coin() ? 0x32u : 0u); w8(0x001413f4, u(ri(0, 1))); }
        });
        // the tuning bases and the copies of the helpers (ship 1's and ship 2's), per level
        struct Ex { int level; uint32_t addr; uint32_t base; int count; bool slowed; int layout; };  // layout 0: the plain block, 1: Orxon's slowed one
        static const Ex exs[] = {
            {2, 0x002eca30, 0x00161f84, 3, false, 0}, {2, 0x002ed420, 0x00161fd8, 2, false, 0}, {2, 0x002ed658, 0x00161fd8, 2, true, 1},
            {9, 0x00306338, 0x00161cf0, 3, false, 0}, {9, 0x00306d28, 0x00161d44, 2, false, 0}, {9, 0x00306f60, 0x00161d44, 2, true, 1},
            {10, 0x002ec110, 0x00162074, 2, false, 0}, {10, 0x002ec348, 0x00162074, 2, true, 1},
        };
        // a moby of the ship classes (any of the level's) moved, its chain points the original reads, the way it came from random
        auto ship_moby = [level, mobys_with](uint32_t cls_fn) -> uint32_t {
            level();
            std::vector<uint32_t> ms = mobys_with(cls_fn);
            if (ms.empty()) {  // (any live moby with pvars)
                for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                    if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x78) != 0) { ms.push_back(m); break; }
            }
            return ms.empty() ? 0u : ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
        };
        for (const Ex& e : exs) {
            if (e.level != g_oracle_level) continue;
            const Ex x = e;
            fcheck(e.slowed ? "bg_ship_exhaust_slow" : "bg_ship_exhaust", x.addr, 40, Ret::None, [ship_moby, upd, x] {
                const uint32_t m = ship_moby(upd);
                const uint32_t from = T;
                Vec4 q = ee_ref<Vec4>(m + 0x10);
                q.x += rf(-3, 3); q.y += rf(-3, 3); q.z += rf(-1, 1);
                ee_ref<Vec4>(from) = q;
                w32(OV(0x0015f5cc), R() & 0xffff);
                seed_rand();
                return Args{{m, from}, {}};
            }, [x](A a) {
                if (x.layout == 0) bg_ship_exhaust(a.i[0], a.i[1], x.count, x.base, x.base + 0xc, x.base + 0x2c, false);
                else bg_ship_exhaust(a.i[0], a.i[1], x.count, x.base + 0x3c, x.base + 0xc, x.base + 0x48, true);
                return uint64_t{0};
            });
        }
        struct Gp { int level; uint32_t addr; uint32_t first, points, draw; };
        static const Gp gps[] = {{2, 0x002ec3f0, 3, 3, 0x002ec308}, {2, 0x002ecd50, 2, 6, 0x002ecc68}, {9, 0x00305cf8, 3, 3, 0x00305c10}, {9, 0x00306658, 2, 6, 0x00306570}, {10, 0x002eba40, 2, 6, 0x002eb958}};
        for (const Gp& g : gps) {
            if (g.level != g_oracle_level) continue;
            const Gp x = g;
            fcheck("bg_ship_glow_prep", x.addr, 40, Ret::None, [ship_moby, upd, x] {
                const uint32_t m = ship_moby(upd);
                if (ri(0, 4) == 0) w8(m + 0x21, 0xff);
                w8(m + 0x90, R()); w8(m + 0x91, R()); w8(m + 0x92, R());
                w32(OV(0x0015f5cc), ri(0, 3) == 0 ? 0u : (R() & 0xffff));
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                const uint32_t last = pv + 0x84 + x.points * 0x10;  // (where the frame of the last request is kept)
                if (ri(0, 2) == 0 || ee_read<uint32_t>(last) < 0x100000) w32(last, T2);
                if (coin()) w32(ee_read<uint32_t>(last), ee_read<uint32_t>(OV(0x0015f5cc)));
                return Args{{m}, {}};
            }, [x](A a) { bg_ship_glow_prep(a.i[0], x.first, x.points, x.draw); return uint64_t{0}; });
        }
    }

    // ---- the linked movers (Umbris, Gemlik Base): every state, the link's moby (its index, its state against the two the
    // record waits for) and the broken-moby byte varied
    {
        auto link_vary = [](uint32_t rec) {
            return [rec](uint32_t m) {
                const uint32_t r = ee_read<uint32_t>(m + 0x78) + rec;
                const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), n = (ee_read<uint32_t>(OV(0x0015ffe0)) - first) >> 8;
                if (ri(0, 4) == 0) w32(r + 0x10, 0xffffffffu);
                else if (ri(0, 2) == 0) w32(r + 0x10, u(ri(0, static_cast<int>(n) - 1)));
                const int32_t idx = ee_read<int32_t>(r + 0x10);
                if (idx >= 0 && ri(0, 3) != 0) {
                    const uint32_t link = first + (u(idx) << 8);
                    const int k = ri(0, 4);
                    w8(link + 0x20, k == 0 ? ee_read<uint32_t>(r + 0x14) : k == 1 ? ee_read<uint32_t>(r + 0x18) : k == 2 ? 0xfeu : u(ri(0, 8)));
                }
                if (ri(0, 3) == 0) w32(r + 0x38, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 7)));
                if (ri(0, 3) == 0) { w32(r + 0x30, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 3))); w32(r + 0x34, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 3))); }
                if (ri(0, 3) == 0) wf(r + 0xc, rf(-1, 4));
                if (ri(0, 5) == 0) { w8(m + 0xb0, u(ri(0, 15))); if (coin()) w8(0x0014c050 + (ee_read<uint32_t>(0x0015ed84) << 4) + ee_read<uint8_t>(m + 0xb0), 0xff); }
            };
        };
        states_check("mover_platform", copy_here({{7, 0x0031aee0}, {13, 0x002c32d0}}), mover_platform_update, 7, 12, link_vary(0xa0));
        states_check("mover_door", copy_here({{7, 0x00310df0}, {13, 0x002c8390}}), mover_door_update, 7, 12, link_vary(0));
    }

    // ---- the puffer (Batalia, Orxon): its timer about to expire, the defaults or its own tuning
    states_check("puffer", copy_here({{8, 0x002f5830}, {10, 0x002c74b8}}), puffer_update, 1, 40, [](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w32(pv + 0x3c, ri(0, 3) == 0 ? u(ri(2, 9)) : u(ri(0, 1)));
        if (ri(0, 2) == 0) w32(pv + 0x38, coin() ? 1u : 0u);
        wf(m + 0x40, rf(-3.1f, 3.1f)); wf(m + 0x48, rf(-3.1f, 3.1f));
    });

    // ---- the homing missiles (Batalia, Hoven): the launch from a random moby, its target any live moby (of the target class
    // or not); the flight on a missile the launch spawned, its velocity, place, life and target varied
    {
        auto any_moby = []() -> uint32_t {
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), n = (ee_read<uint32_t>(OV(0x0015ffe0)) - first) >> 8;
            for (int k = 0; k < 50; ++k) {
                const uint32_t m = first + u(ri(0, static_cast<int>(n) - 1)) * 0x100;
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x78) != 0) return m;
            }
            return first;
        };
        auto target_for = [any_moby]() -> uint32_t {
            if (ri(0, 4) == 0) return 0;
            const uint32_t t = any_moby();
            if (ri(0, 3) != 0) w16(t + 0xa6, 0x1b3);
            if (ri(0, 5) == 0) w8(t + 0x20, coin() ? 0xfeu : 0xfdu);
            wfrand(ee_read<uint32_t>(t + 0x78) + 0x20, 3, -0.5f, 0.5f);
            return t;
        };
        auto launch_args = [level, any_moby, target_for]() {
            level();
            const uint32_t src = any_moby();
            Vec4 p = ee_ref<Vec4>(src + 0x10);
            p.x += rf(-3, 3); p.y += rf(-3, 3); p.z += rf(0, 3);
            ee_ref<Vec4>(T) = p;
            ee_ref<Vec4>(T + 0x10) = Vec4{rf(-1, 1), rf(-1, 1), rf(-0.5f, 1), rf(-1, 1)};
            const uint32_t target = target_for();
            w32(OV(0x0015f5cc), R());
            if (ri(0, 3) == 0) w32(0x0015ed84, coin() ? 12u : 8u);
            seed_rand();
            return Args{{src, T, T + 0x10, target}, {}};
        };
        fcheck("homing_missile_launch", copy_here({{8, 0x002e7eb0}, {12, 0x002ef2e8}}), 60, Ret::Int32, launch_args,
               [](A a) { return uint64_t{homing_missile_launch(a.i[0], a.i[1], a.i[2], a.i[3])}; });
        // (not compared: the segment record's VU0 words 0x173e90.. (Veldin's address), as batches 242 / 330: the generated copy
        // differs from the original there the same way, the native segment_collide's)
        const std::vector<uint32_t> before = g_ignored_words;
        for (uint32_t k = 0; k < 0x30; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
        fcheck("homing_missile", copy_here({{8, 0x002e8210}, {12, 0x002ef648}}), 120, Ret::None, [launch_args, target_for] {
            uint32_t m = 0;
            for (int k = 0; k < 20 && m == 0; ++k) {
                const Args a = launch_args();
                m = homing_missile_launch(a.i[0], a.i[1], a.i[2], a.i[3]);
            }
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const int kind = ri(0, 5);
            if (kind == 0) wf(m + 0x10 + 4 * u(ri(0, 2)), coin() ? rf(0, 12) : rf(495, 520));  // (out of the level's box, or nearly)
            if (kind == 1) wfrand(m + 0x10, 3, 14, 20);  // (near the floor: a small explosion)
            if (ri(0, 2) == 0) ee_ref<Vec4>(pv) = Vec4{rf(-2, 2), rf(-2, 2), rf(-2, 2), 0.0f};
            w32(pv + 0x18, ri(0, 3) == 0 ? u(ri(0, 1)) : u(ri(2, 200)));
            if (ri(0, 2) == 0) w32(pv + 0x10, target_for());
            w32(pv + 0x1c, coin() ? 1u : 0xffffffffu);
            w32(OV(0x0015f5cc), R());
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { homing_missile_update(a.i[0]); return uint64_t{0}; });
        g_ignored_words = before;

        // ---- the shells (Gaspar, Orxon): the launch near the hero; the flight on a shell the launch spawned, its way, its fuse
        // (running, running out, out), its whistle's flags and the hero's distance varied
        auto shell_args = [level, any_moby]() {
            level();
            const uint32_t src = any_moby();
            Vec4 p = ee_ref<Vec4>(0x0013f3d0);
            p.x += rf(-6, 6); p.y += rf(-6, 6); p.z += rf(-1, 4);
            ee_ref<Vec4>(T) = p;
            ee_ref<Vec4>(T + 0x10) = Vec4{rf(-1, 1), rf(-1, 1), rf(-0.6f, 0.6f), rf(-1, 1)};
            seed_rand();
            return Args{{src, T, T + 0x10, u(ri(0, 3) == 0 ? ri(-4, 4) : ri(1, 150))}, {}};
        };
        fcheck("shell_launch", copy_here({{9, 0x00307ba8}, {10, 0x002e68b8}}), 60, Ret::Int32, shell_args,
               [](A a) { return uint64_t{shell_launch(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]))}; });
        fcheck("shell", copy_here({{9, 0x00307d68}, {10, 0x002e6a78}}), 120, Ret::None, [shell_args] {
            uint32_t m = 0;
            for (int k = 0; k < 20 && m == 0; ++k) {
                const Args a = shell_args();
                m = shell_launch(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]));
            }
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 2) == 0) { w32(pv + 0x24, u(ri(0, 1))); w32(pv + 0x28, u(ri(0, 1))); wf(pv + 0x20, rf(0, 8)); }
            if (ri(0, 2) == 0) { const int f = ri(0, 4); w16(pv + 0x1c, u(f == 0 ? 0 : f == 1 ? 1 : f == 2 ? -3 : ri(2, 60))); w16(pv + 0x1e, u(ri(-9, 90))); }
            if (ri(0, 3) == 0) w8(m + 0xbc, 1);
            if (ri(0, 4) == 0) wf(m + 0x10 + 4 * u(ri(0, 2)), coin() ? rf(0, 3) : rf(1019, 1025));
            if (ri(0, 2) == 0) ee_ref<Vec4>(pv) = Vec4{rf(-3, 3), rf(-3, 3), rf(-3, 1), 0.0f};
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { shell_update(a.i[0]); return uint64_t{0}; });

        // ---- the shots (Eudora, Hoven): as the shells, the launcher's class (0xff, 0x4b1, other) and the game's level varied
        auto shot_args = [shell_args]() {
            Args a = shell_args();
            const int k = ri(0, 3);
            if (k < 2) w16(a.i[0] + 0xa6, k == 0 ? 0xffu : 0x4b1u);
            if (ri(0, 3) == 0) w32(0x0015ed84, coin() ? 12u : 4u);
            seed_rand();
            return a;
        };
        fcheck("shot_launch", copy_here({{4, 0x002aee30}, {12, 0x002d4150}}), 60, Ret::Int32, shot_args,
               [](A a) { return uint64_t{shot_launch(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]))}; });
        fcheck("shot", copy_here({{4, 0x002af058}, {12, 0x002d4378}}), 120, Ret::None, [shot_args] {
            uint32_t m = 0;
            for (int k = 0; k < 20 && m == 0; ++k) {
                const Args a = shot_args();
                m = shot_launch(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]));
            }
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 2) == 0) { w32(pv + 0x24, u(ri(0, 1))); w32(pv + 0x28, u(ri(0, 1))); wf(pv + 0x20, rf(0, 8)); }
            if (ri(0, 2) == 0) { const int f = ri(0, 4); w16(pv + 0x1c, u(f == 0 ? 0 : f == 1 ? 1 : f == 2 ? -3 : ri(2, 60))); w16(pv + 0x1e, u(ri(-9, 90))); }
            if (ri(0, 3) == 0) w8(m + 0xbc, 1);
            if (ri(0, 4) == 0) wf(m + 0x10 + 4 * u(ri(0, 2)), rf(-2, 1));
            if (ri(0, 4) == 0) { Vec4& q = ee_ref<Vec4>(m + 0x10); q.x += coin() ? 70.0f : 110.0f; }
            if (ri(0, 2) == 0) ee_ref<Vec4>(pv) = Vec4{rf(-3, 3), rf(-3, 3), rf(-3, 1), 0.0f};
            if (ri(0, 3) == 0) w32(0x0015ed84, coin() ? 12u : 4u);
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { shot_update(a.i[0]); return uint64_t{0}; });

        // ---- the tracer rounds (Batalia, Gaspar): a round of class 1292 spawned near the hero in state 1 or 2, its velocity,
        // shooter, owner (live, dying, none), life and the screen's fade levels varied
        for (uint32_t k = 0; k < 0x30; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);  // (the segment record's VU0 words, as above)
        fcheck("tracer", copy_here({{8, 0x00307298}, {9, 0x00308f08}}), 120, Ret::None, [level, any_moby] {
            level();
            const uint32_t m = moby_spawn(1292);
            if (m == 0) return Args{{0}, {}};
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            Vec4 p = ee_ref<Vec4>(0x0013f3d0);
            p.x += rf(-4, 4); p.y += rf(-4, 4); p.z += rf(0, 3);
            ee_ref<Vec4>(m + 0x10) = p;
            ee_ref<Vec4>(pv) = Vec4{rf(-2, 2), rf(-2, 2), rf(-2, 0.5f), 0.0f};
            w8(m + 0x20, u(ri(0, 4) == 0 ? 2 : ri(0, 5) == 0 ? 3 : 1));
            w32(pv + 0x10, ri(0, 2) == 0 ? 0u : any_moby());
            const uint32_t owner = ri(0, 4) == 0 ? 0u : any_moby();
            if (owner != 0 && ri(0, 4) == 0) w8(owner + 0x20, coin() ? 0xfeu : 0xfdu);
            w32(pv + 0x14, owner);
            w32(pv + 0x18, ri(0, 3) == 0 ? u(ri(0, 1)) : u(ri(2, 90)));
            w32(pv + 0x2c, u(ri(0, 1)));
            wf(OV(0x0015f5d4), coin() ? rf(0, 0.95f) : 1.0f);
            wf(OV(0x0015f5d0), coin() ? rf(0, 0.95f) : 1.0f);
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { if (a.i[0] != 0) tracer_update(a.i[0]); return uint64_t{0}; });
        g_ignored_words = before;
    }

    g_extra_noop_hooks.clear();
    g_ignored_words.clear();
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
