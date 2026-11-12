// Oracle batch 103 (task 0327): Novalis's own moby classes, part 2 (level 1) on its real level data. Run with
// ORACLE_LEVEL=1 build/tests/ovl_oracle --batch 103 (needs the ISO). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the
// original is Novalis's own function at its own address, the port the one registered for it.

#include "ovl_oracle.h"

#include <game/ovl/checkpoint.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/shared_classes_tr.h>
#include <game/ovl/walker.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/disc.h>

#include <cmath>
#include <cstring>
#include <map>

namespace rac1::oracle {

namespace {
// A check whose original is Novalis's own function at `l1addr` (not a Veldin address to translate).
void l1check(const char* name, uint32_t l1addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (g_oracle_level != 1) {
        std::printf("%-26s L1:0x%08x  skipped (Novalis only: run with ORACLE_LEVEL=1)\n", name, l1addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L1:0x%08x\n", name, l1addr);
    if (!check_fn(name, l1addr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_103() {
    if (g_only_batch != 103) return;
    g_batch = 103;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime

    // 989snd's IOP server (as batch 75's): replies a function of the command; the stream system's
    // disc reads (command 0x38) done into EE RAM as the IRX does them
    auto snd_server = [](bool reads) {
        auto result = [](uint32_t id, uint32_t a0) -> uint32_t {
            const uint32_t h = (id * 0x9e3779b1u) ^ (a0 * 0x85ebca6bu);
            return ((h >> 7) % 4 == 0) ? 0u : ((h >> 3) | 1u);
        };
        sif_rpc_register(0x123456, [result, reads](uint32_t fno, uint32_t send, uint32_t, uint32_t recv, uint32_t) {
            if (fno == 0x4d) {
                const uint32_t n = ee_read<uint32_t>(send);
                uint32_t p = send + 4;
                for (uint32_t i = 0; i < n; ++i) {
                    const uint32_t id = ee_read<uint16_t>(p), size = ee_read<uint16_t>(p + 2);
                    ee_write<uint32_t>(recv + 4 + i * 4, result(id, size != 0 ? ee_read<uint32_t>(p + 4) : 0u));
                    if (reads && id == 0x38) {
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
    };
    snd_server(true);
    // the scene's sound ready (the dialog slot in state 3 with a handle: the start's wait loop not
    // entered)
    auto sound_ready = [] {
        w16(0x001516d0 + 0x5a, 3);
        w32(0x001516d0 + 0x50, (R() & 0x7ffffffeu) | 1u);
        w16(0x001516d0 + 0x5c, 0);
        w32(0x001516d0 + 0x1c, 0xffffffffu);
    };
    // the level after its first gameplay frame (the port's), the hero's equipment existing
    static std::vector<uint8_t> snap;
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
    // the level's mobys whose update function is `fn` (Novalis's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 1) { l1check(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        level();
        const std::vector<uint32_t> ms = mobys_with(fn);
        if (ms.empty()) { std::printf("%-26s L1:0x%08x  no moby on this level\n", name, fn); return; }
        l1check(name, fn, n, Ret::None, [level, ms, vary] {
            level();
            const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            vary(m);
            return Args{{m}, {}};
        }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
    };
    (void)classcheck;
    // a function taking one of the level's mobys of class `cls` (a helper of the class's update), perturbed by `vary`
    auto clscheck = [level](const char* name, uint32_t fn, void (*port)(uint32_t), int cls, std::function<void(uint32_t)> vary, int n = 60) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 1) { l1check(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        level();
        std::vector<uint32_t> ms;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<int16_t>(m + 0xa6) == cls) ms.push_back(m);
        if (ms.empty()) { std::printf("%-26s L1:0x%08x  no moby of class %d on this level\n", name, fn, cls); return; }
        l1check(name, fn, n, Ret::None, [level, ms, vary] {
            level();
            const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            vary(m);
            return Args{{m}, {}};
        }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
    };

    if (std::getenv("ORACLE_B97_INFO") != nullptr && g_oracle_level == 1) {
        level();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), end = ee_read<uint32_t>(OV(0x0015ffe0));
        std::printf("mobys %08x..%08x (%u), spawn slots from %08x: pvars %08x %08x\n", first, end, (end - first) >> 8, end, ee_read<uint32_t>(end + 0x78),
                    ee_read<uint32_t>(end + 0x100 + 0x78));
        for (uint32_t m = first; m < first + 0x400; m += 0x100) std::printf("  moby %08x class %d pvars %08x\n", m, ee_read<int16_t>(m + 0xa6), ee_read<uint32_t>(m + 0x78));
        std::map<int, int> seen;  // the class hook tables (class +0x2c) of the level's classes with a Novalis update
        for (uint32_t m = first; m < end; m += 0x100) {
            const int c = ee_read<int16_t>(m + 0xa6);
            const uint32_t cls = ee_read<uint32_t>(m + 0x24);
            if (cls == 0 || seen[c]++ != 0 || ee_read<uint32_t>(cls + 0x2c) == 0) continue;
            const uint32_t t = ee_read<uint32_t>(cls + 0x2c);
            std::printf("  class %d (fn %08x) hooks:", c, ee_read<uint32_t>(m + 0x74));
            for (uint32_t k = 0; k < 8; ++k) std::printf(" %08x", ee_read<uint32_t>(t + 4 * k));
            std::printf("\n");
        }
    }
    // a hit on the moby: its slot in the hit table (0x178100, 64-byte entries) naming it, with a damage mask
    // (the `kinds` bits are mixed with random ones) and a damage at +0x2c
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

    // the debris / bee spawns leave w lanes of a VU0 sin / cos in the original (the port has none)
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    if (g_oracle_level == 1) {
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
    }

    // ---- the beehive (class 815) and its helpers
    if (g_oracle_level == 1) {
        l1check("moby_group_count", 0x0026e008, 60, Ret::Int32, [level] {
            level();
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
            for (int i = 0; i < 6; ++i) w8(first + 0x100 * u(ri(0, 300)) + 0x20, R());  // states, some negative
            return Args{{u(ri(0, 14)), ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 14))}, {}};
        }, [](A a) { return ri32(moby_group_count_l1(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]))); });
    }
    if (g_oracle_level == 1) {
        l1check("moby_group_set_state", 0x0026e0e0, 40, Ret::None, [level] {
            level();
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
            for (int i = 0; i < 6; ++i) w8(first + 0x100 * u(ri(0, 300)) + 0x20, R());
            return Args{{u(ri(0, 14)), R() & 0xffu}, {}};
        }, [](A a) { moby_group_set_state_l1(static_cast<int32_t>(a.i[0]), a.i[1]); return uint64_t{0}; });
    }
    // the hive: a group of bees (its pvar +0) to launch one of
    auto hive_vary = [](uint32_t m, int pass) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, pass == 0 ? u(ri(0, 2)) : 1u);
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        if (ri(0, 2) == 0) w32(pv, u(ri(-1, 14)));
        w32(pv + 8, u(ri(0, 8)));
        w32(L1_ADDR(0x00161e90), u(ri(0, 8)));
        // the group's bees waiting (state 0xc: not counted), some alive or dead
        if (ri(0, 3) != 0) {
            const int32_t gg = ee_read<int32_t>(pv);
            uint32_t p = gg >= 0 && gg < 20 ? ee_read<uint32_t>(OV(0x001ab840) + 4 * u(gg)) : 0;
            for (int i = 0; p != 0 && i < 40; ++i, p += 2) {
                const uint32_t id = ee_read<uint16_t>(p);
                w8(first + ((id & 0x7fff) << 8) + 0x20, ri(0, 5) == 0 ? (coin() ? 0xfeu : 0x3u) : 0xcu);
                if (static_cast<int16_t>(id) < 0) break;
            }
        }
        Vec4 c = ee_ref<Vec4>(0x0013f350 + 0x80); c.x += rf(-20, 20); c.y += rf(-20, 20);
        ee_ref<Vec4>(0x0013f350 + 0x80) = c;
        seed_rand();
    };
    for (int pass = 0; pass < 4; ++pass)
    classcheck("beehive_update", 0x003021b8, beehive_update_l1, [pass, hive_vary](uint32_t m) { hive_vary(m, pass); }, 60);

    // ---- the gold weapon cases (classes 304, 1456..1465)
    g_extra_noop_hooks = {0x00122298};  // (the kernel vsync wait; the weapon scene L1:0x2aea70, ported by task 0383, is not reached: see the answer below)
    for (int pass = 0; pass < 6; ++pass)
    classcheck("gold_case_update", 0x002e1ac0, gold_case_update_l1, [pass, sound_ready](uint32_t m) {
        sound_ready();
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, pass == 0 ? u(ri(0, 5)) : u(pass - 1));
        if (ri(0, 2) == 0) w32(pv + 0x40, u(ri(-1, 9)));
        w32(0x0015ee20, u(ri(0, 1)));
        if (ri(0, 2) == 0) w16(m + 0xa6, coin() ? 0x130u : 0x5b0u);
        const uint32_t w = ee_read<uint32_t>(L1_ADDR(0x001deb10) + 4 * ee_read<uint32_t>(pv + 0x40));
        if (ee_read<uint32_t>(pv + 0x40) < 12) w8(0x0013e520 + w, u(ri(0, 1)));
        { const uint32_t ans = u(ri(0, 3)); w16(pv + 4, ee_read<uint8_t>(m + 0x20) == 1 && ans == 2 ? 3u : ans); }  // (state 1 answered 2 starts the weapon scene 0x2aea70: not run in the oracle, batch 136)
        w32(0x0015ed98, R() & 0xffff); w32(OV(0x0015f5c4), 1u);  // (state 4 asks for the level change: the original would run it for real, not compared here)
    }, 80);
    g_extra_noop_hooks.clear();

    // ---- the teleporter pads (class 1135)
    // a pad past its first update (its three pieces spawned), with its springs and flags perturbed
    auto tele_setup = [](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, 0);
        teleporter_update_l1(m);
        w8(m + 0x20, u(ri(0, 8)));
        for (uint32_t o : {0x20u, 0x24u, 0x28u, 0x2cu, 0x34u}) wf(pv + o, ri(0, 3) == 0 ? 0.0f : (ri(0, 3) == 0 ? 1.0f : rf(-0.3f, 1.3f)));
        w32(pv + 0x10, u(ri(0, 5)));
        w32(pv + 0x3c, u(ri(0, 1))); w32(pv + 0x38, u(ri(0, 1)));
        w32(pv + 0x30, u(ri(-1, 3)));
        w32(0x0015ee20, u(ri(0, 1)));
        w32(0x0015ed84, ri(0, 3) == 0 ? u(ri(0, 19)) : 1u);
    };
    for (int pass = 0; pass < 2; ++pass)
    clscheck("teleporter_trigger", 0x00308b28, teleporter_trigger_l1, 1135, [tele_setup](uint32_t m) {
        tele_setup(m); w8(m + 0x21, ri(0, 2) == 0 ? 0xffu : u(ri(0, 12))); w16(m + 0xb2, u(ri(0, 600)));
    });
    clscheck("teleporter_pieces_update", 0x003092d0, teleporter_pieces_update_l1, 1135, [tele_setup](uint32_t m) { tele_setup(m); });
    clscheck("teleporter_hint", 0x00309430, teleporter_hint_l1, 1135, [tele_setup](uint32_t m) { tele_setup(m); }, 40);
    clscheck("teleporter_hide", 0x003097e8, teleporter_hide_l1, 1135, [tele_setup](uint32_t m) { tele_setup(m); if (ri(0, 3) == 0) w32(ee_read<uint32_t>(m + 0x78) + 0x18, 0); }, 20);
    clscheck("teleporter_show", 0x00309838, teleporter_show_l1, 1135, [tele_setup](uint32_t m) { tele_setup(m); if (ri(0, 3) == 0) w32(ee_read<uint32_t>(m + 0x78) + 0x18, 0); }, 20);
    // the pad's update in each state (the destination pad pvar +0, the hero on it or not)
    g_extra_noop_hooks = {0x00122298};  // (the kernel vsync wait; the music pair 0x27a248 ported by task 0341)
    for (int st = 0; st < 9; ++st)
    for (int pass = 0; pass < 3; ++pass)
    classcheck("teleporter_update", 0x00308bd8, teleporter_update_l1, [st, tele_setup](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        tele_setup(m);
        w8(m + 0x20, u(st));
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        w32(pv, ri(0, 4) == 0 ? u(ri(-1, 3)) : 0u);
        for (uint32_t o = first; o < ee_read<uint32_t>(OV(0x0015ffe0)); o += 0x100)  // the destination: the level's other pad
            if (o != m && ee_read<int16_t>(o + 0xa6) == 1135 && ri(0, 4) != 0) { w32(pv, (o - first) >> 8); break; }
        if (ri(0, 2) == 0) w32(pv + 4, u(ri(-1, 12)));
        if (ri(0, 2) == 0) w32(pv + 8, u(ri(-1, 4)));
        w32(pv + 0x40, ri(0, 2) == 0 ? u(ri(0, 3)) : 0xffffffffu); w32(pv + 0x44, ri(0, 2) == 0 ? u(ri(0, 3)) : 0xffffffffu);
        w32(0x0013cae4, coin() ? 0x10u : 0u); w32(OV(0x0015f594), ri(0, 3) == 0 ? 4u : u(ri(0, 5)));
        if (ri(0, 2) == 0) { Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-1, 1); q.y += rf(-1, 1); ee_ref<Vec4>(0x0013f3d0) = q; }
        w32(0x0013f350 + 0x2fc, coin() ? m : 0u); w16(0x0013f350 + 0x30e, ri(0, 3) == 0 ? 1u : 0u); w8(0x0013f350 + 0x20a4, ri(0, 3) == 0 ? 2u : 0u);
        seed_rand();
    }, 50);
    g_extra_noop_hooks.clear();

    // ---- the path follower library (the jets' follower: pvar +0x60, its path number +0x74)
    std::vector<uint32_t> jet_ms;
    if (g_oracle_level == 1) {
        level();
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<int16_t>(m + 0xa6) == 660) jet_ms.push_back(m);
        std::printf("batch 103: %zu fighter jets\n", jet_ms.size());
    }
    // a jet's follower, its path real or perturbed (idx, the open-path byte, the segment lengths)
    auto jet_follower = [level, &jet_ms](uint32_t* path_out = nullptr) {
        level();
        const uint32_t m = jet_ms[static_cast<size_t>(ri(0, static_cast<int>(jet_ms.size()) - 1))];
        const uint32_t pv = ee_read<uint32_t>(m + 0x78), f = pv + 0x60;
        const uint32_t path = ee_read<uint32_t>(OV(0x001b04b0) + 4 * ee_read<uint32_t>(f + 0x14));
        w32(f + 0x10, path);
        w8(f + 4, ri(0, 4) == 0 ? 0x80u : 0u);
        w32(f, u(ri(0, 6)));
        if (ri(0, 3) == 0) w32(f + 0x14, u(ri(0, 12)));
        if (path_out) *path_out = path;
        return std::pair<uint32_t, uint32_t>(m, f);
    };
    if (g_oracle_level == 1 && !jet_ms.empty()) {
        l1check("path_follower_init", 0x0028b410, 30, Ret::None, [jet_follower] {
            auto [m, f] = jet_follower();
            if (ri(0, 3) == 0) w32(f + 0x14, u(ri(0, 12)));
            return Args{{f}, {}};
        }, [](A a) { path_follower_init_l1(a.i[0]); return uint64_t{0}; });
        l1check("path_nearest", 0x0028b510, 60, Ret::Int32, [jet_follower] {
            uint32_t path = 0;
            jet_follower(&path);
            Vec4 p = ee_ref<Vec4>(path + 0x10 + 0x10 * u(ri(0, 3)));
            p.x += rf(-6, 6); p.y += rf(-6, 6); p.z += rf(-3, 3);
            ee_ref<Vec4>(OUT) = p;
            return Args{{OUT, path}, {ri(0, 2) == 0 ? 0.0f : rf(-5, 40)}};
        }, [](A a) { return ri32(path_nearest_l1(a.i[0], a.i[1], a.f[0])); });
        l1check("path_point", 0x0028b7b0, 60, Ret::Int32, [jet_follower] {
            auto [m, f] = jet_follower();
            return Args{{f, u(ri(-4, 14))}, {}};
        }, [](A a) { return ri32(static_cast<int32_t>(path_point_l1(a.i[0], static_cast<int32_t>(a.i[1])))); });
        l1check("path_index_wrap", 0x0028b828, 60, Ret::Int32, [jet_follower] {
            auto [m, f] = jet_follower();
            return Args{{f, u(ri(-4, 14))}, {}};
        }, [](A a) { return ri32(path_index_wrap_l1(a.i[0], static_cast<int32_t>(a.i[1]))); });
        l1check("path_index_advance", 0x0028b878, 40, Ret::Int32, [jet_follower] {
            auto [m, f] = jet_follower();
            return Args{{f}, {}};
        }, [](A a) { return ri32(path_index_advance_l1(a.i[0])); });
        l1check("path_spline_tangent", 0x0028b8c8, 80, Ret::None, [jet_follower] {
            auto [m, f] = jet_follower();
            wrand(OUT, 8);
            return Args{{OUT, f, u(ri(0, 3))}, {rf(-1, 1), rf(-1, 1)}};
        }, [](A a) { path_spline_tangent_l1(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), a.f[0], a.f[1]); return uint64_t{0}; });
        l1check("hermite", 0x0028ba80, 60, Ret::None, [level] {
            level();
            wfrand(OUT, 4, -10, 10); wfrand(OUT + 0x10, 4, -10, 10); wfrand(OUT + 0x20, 4, -10, 10); wfrand(OUT + 0x30, 4, -10, 10); wfrand(OUT + 0x40, 4, -10, 10);
            return Args{{OUT + 0x40, OUT, OUT + 0x10, OUT + 0x20, OUT + 0x30}, {rf(0, 1)}};
        }, [](A a) { hermite_l1(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.f[0]); return uint64_t{0}; });
        l1check("path_measure", 0x0028bb90, 20, Ret::Float, [jet_follower] {
            auto [m, f] = jet_follower();
            return Args{{f, u(ri(0, 1)), m}, {rf(-1, 1), rf(-1, 1)}};
        }, [](A a) { return rf32(path_measure_l1(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1])); }, 0, 0);
        l1check("debug_particle", 0x00285768, 30, Ret::Int32, [level] {
            level();
            wfrand(OUT, 4, -10, 10);
            return Args{{OUT, u(ri(0, 5)), u(ri(0, 20)), R(), u(R() & 0xff), 0xffffffffu, ri(0, 1) ? 0xffu : u(ri(0, 0xfe))}, {rf(0, 2)}};
        }, [](A a) { return ri32(static_cast<int32_t>(debug_particle_l1(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], static_cast<int32_t>(a.i[5]), a.i[6], a.f[0]))); });
        // the aircraft base's small helpers
        l1check("jet_banking", 0x002f5040, 40, Ret::None, [jet_follower] {
            auto [m, f] = jet_follower();
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            wf(pv + 0x130, ri(0, 3) == 0 ? 0.0f : rf(-3, 3)); wf(pv + 0x134, ri(0, 3) == 0 ? 0.0f : rf(-3, 3));
            wfrand(m + 0x40, 3, -3, 3);
            wfrand(OUT, 4, -10, 10);
            return Args{{m, OUT}, {}};
        }, [](A a) { jet_banking_l1(a.i[0], a.i[1]); return uint64_t{0}; });
        l1check("smoke_particle", 0x0027f660, 20, Ret::None, [level] {
            level(); wfrand(OUT, 4, -10, 10); wfrand(OUT + 0x10, 4, -3, 3);
            return Args{{OUT, OUT + 0x10}, {}};
        }, [](A a) { smoke_particle_l1(a.i[0], a.i[1]); return uint64_t{0}; });
        l1check("jet_debris_spawn", 0x0030be70, 20, Ret::Int32, [jet_follower] {
            auto [m, f] = jet_follower();
            wfrand(OUT, 4, -2, 2);
            seed_rand();
            return Args{{m, OUT}, {rf(0.2f, 2)}};
        }, [](A a) { return ri32(static_cast<int32_t>(jet_debris_spawn_l1(a.i[0], a.i[1], a.f[0]))); });
    }

    if (std::getenv("ORACLE_B103_INFO") != nullptr && g_oracle_level == 1) {
        level();
        for (uint32_t a = 0; a < 0x40; a += 0x10) { const Vec4 v = ee_ref<Vec4>(OV(0x0016ccc0) + a); std::printf("view %02x: %g %g %g %g\n", a, v.x, v.y, v.z, v.w); }
        for (uint32_t a = 0; a < 0x80; a += 0x10) { const Vec4 v = ee_ref<Vec4>(OV(0x0016cd00) - 0x10 + a); std::printf("frus %02x: %g %g %g %g\n", a, v.x, v.y, v.z, v.w); }
        const Vec4 h = ee_ref<Vec4>(0x0013f3d0); std::printf("hero %g %g %g\n", h.x, h.y, h.z);
    }
    // ---- the aircraft base on the fighter jets
    g_extra_noop_hooks = {0x00122298};  // (0x273310: explosion_effect since task 0383)
    auto jet_prep = [give_hit, hero_near](uint32_t m, int st, int pass) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        w8(m + 0x20, u(st));
        if (st == 1) { w8(m + 0x20, 0); jet_update_base_l1(m); w8(m + 0x20, 1); }  // (set up by the port's own state 0)
        w8(pv + 0x120, pass == 0 ? 0u : u(R() & 0x67));
        w8(pv + 0x121, u(ri(0, 8)));
        for (int i = 0; i < 4; ++i) w32(pv + 0x140 + 4 * u(i), ri(0, 3) == 0 ? 0xffffffffu : (m - first) / 0x100 + u(ri(0, 3)));
        w32(pv + 0x14, 0);
        wf(pv + 0xf0, ri(0, 2) == 0 ? rf(0.5f, 1.6f) : rf(0, 1));
        wf(pv + 0xf4, rf(-1, 1)); wf(pv + 0xf8, rf(-1, 1)); wf(pv + 0xfc, rf(0.5f, 3));
        wf(pv + 0x124, ri(0, 3) == 0 ? 0.0f : rf(0, 3)); wf(pv + 0x128, rf(-3, 3)); wf(pv + 0x12c, ri(0, 3) == 0 ? -1.0f : (ri(0, 6) == 0 ? 7.0f : rf(-3, 3)));
        wf(pv + 0x110, rf(0.5f, 4)); wf(pv + 0x108, rf(5, 90)); wf(pv + 0x100, rf(5, 90)); wf(pv + 0x114, rf(0, 2)); wf(pv + 0x118, rf(0, 2));
        w16(pv + 0x122, u(ri(-20, 20)));
        wf(pv + 0x130, ri(0, 3) == 0 ? 0.0f : (ri(0, 2) == 0 ? rf(0, 100) : rf(400, 900))); wf(pv + 0x134, ri(0, 3) == 0 ? 0.0f : (ri(0, 2) == 0 ? rf(0, 100) : rf(400, 900)));
        w16(pv + 0x138, ri(0, 3) == 0 ? 0xffffu : u(ri(0, 5))); w16(pv + 0x13a, ri(0, 3) == 0 ? 0u : u(ri(0, 5))); w16(pv + 0x13c, u(ri(1, 40))); w16(pv + 0x13e, u(ri(1, 40)));
        w16(pv + 0x11c, ri(0, 3) == 0 ? u(ri(0, 3)) : 0u); w16(pv + 0x11e, ri(0, 3) == 0 ? 0xffffu : u(ri(0, 6)));
        w8(m + 0x21, ri(0, 2) == 0 ? u(ri(0, 13)) : 0xffu);
        w32(OV(0x0015f5cc), R() & 0xffff); w32(0x0015ed84, ri(0, 2) == 0 ? (coin() ? 3u : 9u) : 1u);
        w32(L1_ADDR(0x00161b00), ri(0, 12) == 0 ? 1u : 0u);
        wf(pv + 0x104, ri(0, 2) == 0 ? 0.0f : rf(-0.05f, 0.05f)); wf(pv + 0x10c, ri(0, 2) == 0 ? 0.0f : rf(-0.05f, 0.05f));
        if (ri(0, 1) == 0 && ee_read<uint8_t>(m + 0x21) != 0xff) {  // the group's list: this jet first, then another (bit 15 ends it)
            const uint32_t list = OUT + 0x300, g = ee_read<uint8_t>(m + 0x21);
            w16(list, (m - first) >> 8); w16(list + 2, ((first + 0x100 * u(ri(0, 300)) - first) >> 8) | (coin() ? 0x8000u : 0u));
            if (ee_read<uint16_t>(list + 2) < 0x8000) w16(list + 4, 0x8000u | u(ri(0, 300)));
            w32(OV(0x001ab840) + 4 * g, list);
            w32(OV(0x0015fff4), 20);
        }
        hero_near(m);
        { Vec4 q = ee_ref<Vec4>(0x0013f3d0); q.z += rf(-2, 8); ee_ref<Vec4>(OV(0x00166dc0)) = q; }
        if (ri(0, 2) == 0) { Vec4 q = ee_ref<Vec4>(0x0013f3d0); q.x += rf(-30, 30); q.y += rf(-30, 30); q.z += rf(-5, 20); ee_ref<Vec4>(m + 0x10) = q; }
        {  // the view state (zero in the snapshot): a camera near the hero, random rotation rows and frustum terms
            Vec4 cam = ee_ref<Vec4>(0x0013f3d0);
            cam.x = (cam.x + rf(-10, 10)) * 1024.0f; cam.y = (cam.y + rf(-10, 10)) * 1024.0f; cam.z = (cam.z + rf(-10, 10)) * 1024.0f; cam.w = 1024.0f;
            ee_ref<Vec4>(OV(0x0016ccf0)) = cam;
            for (uint32_t a = 0; a < 0x30; a += 0x10) { Vec4 r = {rf(-1, 1), rf(-1, 1), rf(-1, 1), 0.0f}; ee_ref<Vec4>(OV(0x0016ccc0) + a) = r; }
            for (uint32_t a = 0; a < 0x40; a += 0x10) { Vec4 r = {rf(0.1f, 3), rf(0.1f, 3), rf(0.1f, 3), rf(0.1f, 3)}; ee_ref<Vec4>(OV(0x0016cd00) + a) = r; }
        }
        if (ri(0, 9) < 7) {  // on the screen: the moby's bounding position (+0x00, 1024 times the world's) in front of the camera
            Vec4 q = ee_ref<Vec4>(0x0013f3d0);
            q.x = (q.x + rf(-15, 15)) * 1024.0f; q.y = (q.y + rf(-15, 15)) * 1024.0f; q.z = (q.z + rf(-5, 15)) * 1024.0f; q.w = 1.0f;
            ee_ref<Vec4>(m) = q;
            const float rr = rf(0.5f, 20.0f);
            w16(m + 0x32, u(static_cast<int32_t>(rr)));
        }
        give_hit(m, pass == 4 && st == 0 ? 0x800000u : 0x210000u);
        w16(m + 0xa6, ri(0, 5) == 0 ? (coin() ? 0x33u : 0x46au) : 660u);
        seed_rand();
    };
    for (int st = 0; st < 4; ++st)
    for (int pass = 0; pass < 5; ++pass)
    clscheck("jet_update_base", 0x002f5168, jet_update_base_l1, 660, [st, pass, jet_prep](uint32_t m) { jet_prep(m, st, pass); }, 40);
    // the fighter jet's own update: the hit (mask 0x800000) or not, the first-frame sizes, the height fade
    for (int st = 0; st < 4; ++st)
    for (int pass = 0; pass < 6; ++pass)
    classcheck("jet_fighter_update", 0x002f4428, jet_fighter_update_l1, [st, pass, jet_prep, give_hit](uint32_t m) {
        jet_prep(m, st, pass % 5);
        if (pass == 5) w8(m + 0x20, 0x65);
        if (pass >= 3) give_hit(m, 0x800000);
        w16(m + 0xa6, 660u);
        wf(m + 0x18, ri(0, 3) == 0 ? rf(100, 200) : rf(-5, 300));
        wf(L1_ADDR(0x00161af4), rf(0.2f, 0.9f)); wf(L1_ADDR(0x00161af8), rf(40, 200)); wf(L1_ADDR(0x00161afc), rf(0, 40));
        w8(0x0013d408, u(ri(0, 1)));
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        for (uint32_t o : {0xd0u, 0xe0u}) wfrand(pv + o, 3, -5, 5);
    }, 30);
    // ---- the bomber (class 688) and its shot
    g_extra_noop_hooks = {0x00122298};
    if (g_oracle_level == 1) {
        l1check("bomber_shot_spawn", 0x002f68a0, 30, Ret::Int32, [level] {
            level();
            seed_rand();
            for (uint32_t o : {0u, 0x10u, 0x20u}) wfrand(OUT + o, 4, -10, 10);
            return Args{{ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(1, 200)), OUT, OUT + 0x10, OUT + 0x20, u(ri(0, 1))}, {}};
        }, [](A a) { return ri32(static_cast<int32_t>(bomber_shot_spawn_l1(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4]))); });
    }
    auto bomber_prep = [jet_prep, give_hit](uint32_t m, int st, int pass) {
        jet_prep(m, st, pass % 5);
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        w16(m + 0xa6, 688u);
        if (pass >= 5) give_hit(m, 0x800000);
        w8(m + 0x20, u(st));
        auto box = [] { return u(ri(0, 3)); };
        const uint32_t hb = u(ri(0, 3));  // the box the hero (and the camera) stand in
        w32(pv + 0x170, ri(0, 5) == 0 ? 0xffffffffu : box());
        w32(pv + 0x174, 0xffffffffu);  // (the scene start: hero state 0x72 hangs the original on this snapshot: not exercised)
        w16(pv + 0x186, ri(0, 2) == 0 ? 0xffffu : (ri(0, 2) == 0 ? u(frame_scale_i(0x3c)) : u(ri(0, 300))));
        w32(pv + 0x188, box()); w32(pv + 0x18c, box());
        for (uint32_t i = 0; i < 4; ++i) w8(pv + 0x180 + i, u(ri(0, 6)));
        w32(pv + 0x60, ri(0, 2) == 0 ? u(ee_read<uint8_t>(pv + 0x180 + u(ri(0, 3)))) : u(ri(0, 6)));
        w32(pv + 0x16c, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 6)));
        for (uint32_t i = 0; i < 16; ++i) w32(pv + 0x190 + 4 * i, ri(0, 3) == 0 ? 0xffffffffu : box());
        for (uint32_t i = 0; i < 8; ++i) {
            const uint32_t id = ri(0, 4) == 0 ? 0xffffffffu : u(ri(2, 250));
            w32(pv + 0x1d0 + 4 * i, id);
            if (id != 0xffffffffu && ri(0, 4) != 0) {
                const uint32_t mm = first + (id << 8);
                w16(mm + 0xa6, 0x2bc);
                const uint32_t tp = ee_read<uint32_t>(mm + 0x78);
                if (tp != 0) { w16(tp + 4, ri(0, 6) == 0 ? 1u : 0u); w32(tp, ri(0, 6) == 0 ? 1u : 0u); w32(tp + 8, ri(0, 4) == 0 ? box() : hb); }
            }
        }
        for (uint32_t i = 0; i < 4; ++i) {
            const uint32_t id = ri(0, 4) == 0 ? 0xffffffffu : u(ri(2, 250));
            w32(pv + 0x1f0 + 4 * i, id);
            if (id != 0xffffffffu && ri(0, 4) != 0) {
                const uint32_t mm = first + (id << 8);
                w16(mm + 0xa6, 0x1cb);
                if (ri(0, 4) == 0) w8(mm + 0x20, coin() ? 0xfeu : 0xfdu);
            }
        }
        w32(OV(0x00173e58), ri(0, 2) == 0 ? 0u : (coin() ? m : first + 0x100 * u(ri(2, 250))));
        wf(L1_ADDR(0x00161bd0), rf(0.2f, 0.9f)); wf(L1_ADDR(0x00161bd4), rf(40, 200)); wf(L1_ADDR(0x00161bd8), rf(0, 40));
        w8(0x0013d408, u(ri(0, 1)));
        w16(m + 0xb2, u(ri(0, 600)));
        w8(OV(0x001bb230) + (R() % 600) + 0x454, R() & 1);
        for (uint32_t o : {0xd0u, 0xe0u}) wfrand(pv + o, 3, -5, 5);
        // the hero at one of the level's boxes (or not)
        if (ri(0, 2) != 0) {
            const uint32_t b = ee_read<uint32_t>(OV(0x001600ec)) + (hb << 7) + 0x30;
            Vec4 q = ee_ref<Vec4>(b);
            ee_ref<Vec4>(0x0013f3d0) = q;
            if (ri(0, 2) != 0) { q.z += 0.5f; ee_ref<Vec4>(OV(0x00166dc0)) = q; }
        }
        // the first map word of the level's collected table cleared or set
        w32(0x0014c190 + (u(ee_read<int32_t>(0x0015ed84)) << 8) + 4 * u(ri(0, 20)), R() | (R() << 7));
        { const int32_t pn = ee_read<int32_t>(pv + 0x74); const uint32_t path = pn >= 0 && pn < 100 ? ee_read<uint32_t>(OV(0x001b04b0) + 4 * u(pn)) : 0; if (std::getenv("B103_DBG")) std::printf("bomber m=%08x st=%u pn=%d path=%08x count=%d\n", m, ee_read<uint8_t>(m + 0x20), pn, path, path ? ee_read<int32_t>(path) : -99); }
        seed_rand();
    };
    for (int st = 0; st < 3; ++st)
    for (int pass = 0; pass < 8; ++pass)
    classcheck("jet_bomber_update", 0x002f7728, jet_bomber_update_l1, [st, pass, bomber_prep](uint32_t m) { bomber_prep(m, st, pass); }, 30);
    for (int st = 0; st < 4; ++st)
    for (int pass = 0; pass < 5; ++pass)
    if (!std::getenv("B103_ST") || std::atoi(std::getenv("B103_ST")) == st) clscheck("jet_update_base_ship", 0x002f5168, jet_update_base_l1, 666, [st, pass, jet_prep](uint32_t m) { jet_prep(m, st, pass); w16(m + 0xa6, 666u); }, 40);
    // ---- the stationary ship (class 666)
    g_extra_noop_hooks = {0x00122298};
    for (int stage = 0; stage < 4; ++stage)
    for (int pass = 0; pass < 6; ++pass)
    classcheck("jet_ship_update", 0x002f4960, jet_ship_update_l1, [stage, pass, jet_prep](uint32_t m) {
        jet_prep(m, 1, pass % 5);
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        w16(m + 0xa6, 666u);
        w32(pv + 0x170, ri(0, 6) == 0 ? 0xffffffffu : u(ri(0, 3)));
        w16(pv + 0x174, u(stage));
        w32(pv + 0x180, ri(0, 3) == 0 ? 0xffffffffu : u(ri(2, 250)));
        wf(pv + 0x188, rf(0, 3)); wf(pv + 0x190, rf(0, 90));
        w32(pv + 0x17c, ri(0, 3) == 0 ? u(ri(0, 5)) : ee_read<uint32_t>(pv + 0x60));
        w32(pv + 0x178, u(ri(0, 1))); w16(pv + 0x176, ri(0, 3) == 0 ? 0u : u(ri(0, 3))); w32(pv + 0x184, u(ri(0, 1)));
        w8(m + 0x52, ri(0, 2) == 0 ? 2u : u(ri(0, 5))); w8(m + 0x53, u(ri(0, 5)));
        wf(pv + 0x190, rf(0, 90));
        for (uint32_t i = 0; i < 8; ++i) {
            const uint32_t id = ri(0, 4) == 0 ? 0xffffffffu : u(ri(2, 250));
            w32(pv + 0x150 + 4 * i, id);
            if (id != 0xffffffffu) {
                static const int classes[] = {0x1ab, 0x154, 0x1cb, 0x77, 0x1ac};
                w16(first + (id << 8) + 0xa6, u(classes[ri(0, 4)]));
            }
        }
        if (ri(0, 2) == 0) { const uint32_t c = ee_read<uint32_t>(pv + 0x180); if (c != 0xffffffffu) w16(first + (c << 8) + 0xa6, 0x77u); }
        if (ri(0, 3) == 0) {  // the follower at the second last point of the path
            const int32_t pn = ee_read<int32_t>(pv + 0x74);
            if (pn >= 0 && pn < 100) { const uint32_t path = ee_read<uint32_t>(OV(0x001b04b0) + 4 * u(pn)); w32(pv + 0x60, u(ee_read<int32_t>(path) - 2)); }
        }
        wf(OV(0x00166dd8), rf(-3.1f, 3.1f));
        if (ri(0, 1)) { const uint32_t b = ee_read<uint32_t>(OV(0x001600ec)) + (u(ri(0, 3)) << 7) + 0x30; ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(b); }
        if (std::getenv("B103_DBG")) std::printf("ship m=%08x stage=%u yaw=%g st=%u pn=%d\n", m, u(stage), ee_read<float>(m + 0x48), ee_read<uint8_t>(m + 0x20), ee_read<int32_t>(pv + 0x170));
        seed_rand();
    }, 30);
    g_extra_noop_hooks.clear();

    // ---- the Birdbot (class 459; novalis_birdbot.cpp)
    g_extra_noop_hooks = {0x00122298};  // (0x273310: explosion_effect since task 0341)
    auto bird_prep = [give_hit, hero_near](uint32_t m, int st, int pass) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t P = 0x0013f350;
        if (st != 0) {  // set up first (state 0, the port's), then perturbed in the state under test
            w8(m + 0x20, 0);
            birdbot_update_l1(m);
        }
        w8(m + 0x20, u(st));
        if (ri(0, 2) == 0) w32(pv + 0x38, u(ri(0, 1)));
        w16(pv + 0x1e2, ri(0, 3) == 0 ? u(ri(0, 4)) : 0u);
        w32(pv + 0x1c8, ri(0, 3) == 0 ? u(ri(-1, 4)) : ee_read<uint32_t>(pv + 0x1c8));
        wf(pv + 0x1a0, rf(3, 30));
        w32(pv + 0x190, ri(0, 3) == 0 ? u(ri(0, 4)) : 0u);
        wf(pv + 0x198, rf(-2, 5)); w16(pv + 0x1a4, u(ri(0, 6))); wf(pv + 0x1b8, rf(0, 5)); wf(pv + 0x1cc, rf(0, 30));
        w16(pv + 0x1ac, u(ri(0, 1))); w16(pv + 0x1ae, u(ri(0, 1))); w16(pv + 0x1ee, u(ri(0, 1))); w16(pv + 0x26, ri(0, 3) == 0 ? u(ri(0, 5)) : 0u);
        w32(pv + 0x1e8, u(ri(0, 1)));
        w32(pv + 0x1c4, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 3)));
        w32(pv + 0x1e4, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 3)));
        wf(pv + 0x20, rf(-0.5f, 2)); wf(pv + 0x54 - 0x34 + 0x34, rf(-1, 1));
        w8(m + 0x53, u(ri(0, 8))); w8(m + 0x52, coin() ? ee_read<uint32_t>(m + 0x53) : u(ri(0, 8))); w8(m + 0xbc, u(ri(0, 2)));
        wf(m + 0x48, rf(-3.1f, 3.1f));
        { Vec4 q = ee_ref<Vec4>(m + 0x10); const float r = ri(0, 3) == 0 ? rf(0, 6) : (coin() ? rf(6, 30) : rf(30, 80)); const float a = rf(-3.1f, 3.1f);
          q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-4, 12); ee_ref<Vec4>(P + 0x80) = q; }
        wf(m + 0x18, ee_read<float>(m + 0x18) + (coin() ? 0.0f : rf(-3, 3)));
        give_hit(m, 0x210000);
        if (ee_read<uint8_t>(m + 0xa4) != 0xff) {  // a believable hit record (kind, strength, weapon, applied mark, attacker)
            const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
            w8(e + 0x28, u(ri(0, 11))); w8(e + 0x29, u(ri(0, 4))); w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
            wf(e + 0x1c, coin() ? 5627.95f : 0.0f);
            w32(e + 0x20, ri(0, 3) == 0 ? 0u : ee_read<uint32_t>(P + 0x2080));
            wf(e + 0x2c, coin() ? rf(0.1f, 6) : 0.0f);
        }
        if (std::getenv("B103_DBG")) std::printf("bird st=%d idx=%u hit=%08x pv26=%u\n", st, ee_read<uint8_t>(m + 0xa4), sub_0025a420(m, 0x210000, 1), ee_read<uint16_t>(pv + 0x26));
        if (ri(0, 4) == 0) w32(0x0015f5c4, 2u);
        w32(OV(0x0015f5cc), R() & 0xffff);
        wf(L1_ADDR(0x00161954), rf(0.5f, 8));
        seed_rand();
    };
    for (int st : {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14})
    for (int pass = 0; pass < 2; ++pass)
    classcheck("birdbot_update", 0x002e6bf0, birdbot_update_l1, [st, pass, bird_prep](uint32_t m) { bird_prep(m, st, pass); }, 40);
    g_extra_noop_hooks.clear();

    // ---- the Amoeboids (classes 572 / 865 / 866; novalis_amoeboid.cpp)
    // (the class's carry / suck hooks, 0x2efa88.. 0x3040d8 0x2f1c78 0x2f1cc8, belong to the weapons (task 0321): no-ops on both sides)
    g_extra_noop_hooks = {0x00273310, 0x00122298, 0x003040d8};  // (the Peckbot and Amoeboid hooks are ported: task 0330)
    auto amoeba_prep = [give_hit, hero_near](uint32_t m, int st, int pass) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t P = 0x0013f350;
        if (st != 0) {
            w8(m + 0x20, 0);
            amoeboid_update_l1(m);
        }
        w8(m + 0x20, u(st));
        w8(m + 0x53, u(ri(0, 8))); w8(m + 0x52, coin() ? ee_read<uint32_t>(m + 0x53) : u(ri(0, 8))); w8(m + 0xbc, u(ri(0, 2)));
        wf(m + 0x48, rf(-3.1f, 3.1f));
        { Vec4 q = ee_ref<Vec4>(m + 0x10); const float r = ri(0, 3) == 0 ? rf(0, 6) : (coin() ? rf(6, 30) : rf(30, 80)); const float a = rf(-3.1f, 3.1f);
          q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-4, 12); ee_ref<Vec4>(P + 0x80) = q; }
        wf(m + 0x18, ee_read<float>(m + 0x18) + (coin() ? 0.0f : rf(-3, 3)));
        for (uint32_t o = 0x20; o < 0x200; o += 4) {  // a few of the pvars kept as they are typed: floats scaled, small counters moved
            if (ri(0, 9) != 0) continue;
            const uint32_t v = ee_read<uint32_t>(pv + o), ex = v & 0x7f800000u;
            if (ex >= 0x30000000u && ex < 0x50000000u) wf(pv + o, ee_read<float>(pv + o) * rf(0.5f, 1.5f));
            else if (v < 16) w32(pv + o, u(ri(0, static_cast<int>(v) + 2)));
        }
        give_hit(m, ri(0, 1) ? 0x210000u : 0x330000u);
        if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
            const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
            w8(e + 0x28, u(ri(0, 11))); w8(e + 0x29, u(ri(0, 4))); w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
            wf(e + 0x1c, coin() ? 5627.95f : 0.0f);
            w32(e + 0x20, ri(0, 3) == 0 ? 0u : ee_read<uint32_t>(P + 0x2080));
            wf(e + 0x2c, coin() ? rf(0.1f, 6) : 0.0f);
        }
        w32(OV(0x0015f5cc), R() & 0xffff);
        seed_rand();
    };
    if (std::getenv("B103_DBG") != nullptr && g_oracle_level == 1) {
        level();
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint32_t>(m + 0x74) == 0x002edca0) {
                const uint32_t t = ee_read<uint32_t>(ee_read<uint32_t>(m + 0x24) + 0x2c);
                std::printf("amoeboid %08x class %d hooks:", m, ee_read<int16_t>(m + 0xa6));
                for (uint32_t k = 0; k < 8; ++k) std::printf(" %08x", ee_read<uint32_t>(t + 4 * k));
                std::printf("\n");
                break;
            }
    }
    // the carry state's callee: Veldin's port carry_update (0x2dc9f0) against Novalis's own copy 0x305260, on the Amoeboids
    if (g_oracle_level == 1) {
        l1check("carry_update_l1", 0x00305260, 40, Ret::Int32, [level, amoeba_prep] {
            level();
            std::vector<uint32_t> ms;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == 0x002edca0) ms.push_back(m);
            const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            amoeba_prep(m, 14, 0);
            return Args{{m, ee_read<uint32_t>(m + 0x78) + 0x60}, {}};
        }, [](A a) { return ri32(carry_update(a.i[0], a.i[1])); });
    }
    if (g_oracle_level == 1) {  // the shared ports the Amoeboid calls, against Novalis's own copies
        auto amoeba_m = [level, amoeba_prep] {
            level();
            std::vector<uint32_t> ms;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == 0x002edca0) ms.push_back(m);
            const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            amoeba_prep(m, 15, 0);
            return m;
        };
        l1check("walker_record_step", 0x0026d9a8, 40, Ret::Int32, [amoeba_m] {
            const uint32_t m = amoeba_m(), pv = ee_read<uint32_t>(m + 0x78);
            wfrand(OUT, 3, -3, 3);
            return Args{{m, pv + 0x170, OUT, OUT + 0x20}, {rf(0.3f, 1.5f)}};
        }, [](A a) { return r32(walker_record_step(a.f[0], a.i[0], a.i[1], a.i[2], a.i[3])); });
        l1check("yaw_seek", 0x00270cc0, 40, Ret::Float, [amoeba_m] {
            const uint32_t m = amoeba_m(), pv = ee_read<uint32_t>(m + 0x78);
            return Args{{m + 0x48, pv + 0x220}, {rf(-3, 3), rf(0, 1), rf(0, 1), rf(0, 1)}};
        }, [](A a) { return rf32(yaw_seek(a.i[0], a.i[1], a.f[0], a.f[1], a.f[2], a.f[3])); });
        l1check("moby_floor_band", 0x0026f020, 40, Ret::None, [amoeba_m] {
            const uint32_t m = amoeba_m();
            return Args{{m}, {}};
        }, [](A a) { moby_floor_band(a.i[0]); return uint64_t{0}; });
        l1check("anim_crossed", 0x002765b0, 40, Ret::Int32, [amoeba_m] {
            const uint32_t m = amoeba_m();
            return Args{{m}, {rf(0, 1)}};
        }, [](A a) { return r32(static_cast<uint32_t>(anim_crossed(a.i[0], a.f[0]))); });
        l1check("sub_00259bc8", 0x0026eaa8, 40, Ret::None, [amoeba_m] {
            const uint32_t m = amoeba_m();
            wfrand(OUT, 8, -3, 3);
            return Args{{m, u(ri(0, 5)), u(ri(0, 5)), OUT, OUT + 0x10}, {rf(0, 3)}};
        }, [](A a) { sub_00259bc8(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.f[0]); return uint64_t{0}; });
    }
    for (int st = 0; st < 17; ++st)
    for (int pass = 0; pass < 2; ++pass)
    if (!std::getenv("B103_ST") || std::atoi(std::getenv("B103_ST")) == st) classcheck("amoeboid_update", 0x002edca0, amoeboid_update_l1, [st, pass, amoeba_prep](uint32_t m) { amoeba_prep(m, st, pass); }, 40);
    g_extra_noop_hooks.clear();

    // ---- the remaining classes (transliterated by tools/codegen/mkall.py): the level's own mobys of each class, the first
    // update (state 0) done by the port, then a state, a hit, the hero near, the pvars' floats / counters moved as typed
    g_extra_noop_hooks = {0x00122298, 0x00118a80, 0x00122fe0, 0x001232c8, OVF(0x00299460), OVF(0x0023e008), OVF(0x0024d430),
                          0x002efa88, 0x002efaf8, 0x002efb48, 0x002efb98, 0x002efbc8, 0x003040d8, 0x002f1c78, 0x002f1cc8};
    struct TrCls { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t skip_states = 0; };  // (the states that start a scene / a level change: the original runs the kernel / movie path for real)
    static const TrCls trcls[] = {
        {"class700", 0x002f8c58, nov_class700_update_l1}, {"class679", 0x002f6328, nov_class679_update_l1}, {"class27", 0x002bd100, nov_class27_update_l1},
        {"class701", 0x002f9080, nov_class701_update_l1}, {"class704", 0x002f9810, nov_class704_update_l1}, {"class761", 0x002feb58, nov_class761_update_l1},
        {"class729", 0x002fa800, nov_class729_update_l1}, {"class730", 0x002fad68, nov_class730_update_l1, 0xe2}, {"class1504", 0x0030b618, nov_class1504_update_l1},
        {"class613", 0x002f3120, nov_class613_update_l1}, {"class709", 0x002f9d80, nov_class709_update_l1}, {"class1848", 0x0030f208, nov_class1848_update_l1},
        {"class604", 0x002f2b68, nov_class604_update_l1}, {"class778", 0x002ff860, nov_class778_update_l1}, {"class832", 0x00302648, nov_class832_update_l1},
        {"class567", 0x002eda00, nov_class567_update_l1}, {"class722", 0x002fa4c0, nov_class722_update_l1}, {"class779", 0x002ffb28, nov_class779_update_l1},
        {"class787", 0x002ffdc0, nov_class787_update_l1}, {"class156", 0x002c7530, nov_class156_update_l1}, {"class1546", 0x0030c190, nov_class1546_update_l1},
        {"class686", 0x002f6a30, nov_class686_update_l1}, {"class696", 0x002f8718, nov_class696_update_l1}, {"class1510", 0x0030ba18, nov_class1510_update_l1},
        {"class1633", 0x0030c9a8, nov_class1633_update_l1}, {"class751", 0x002fd0e8, nov_class751_update_l1}, {"class1818", 0x0030df40, nov_class1818_update_l1},
        {"class437", 0x002e3f20, nov_class437_update_l1}, {"class280", 0x002e0c68, nov_class280_update_l1}, {"class1134", 0x00307ca0, nov_class1134_update_l1, 0x6},
    };
    auto generic_prep = [give_hit, hero_near](uint32_t m, int st, int pass, void (*fn)(uint32_t)) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t P = 0x0013f350;
        if (st != 0) {
            w8(m + 0x20, 0);
            fn(m);
        }
        w8(m + 0x20, u(st));
        if (pass == 1) {
            w8(m + 0x53, u(ri(0, 8))); w8(m + 0x52, coin() ? ee_read<uint32_t>(m + 0x53) : u(ri(0, 8))); w8(m + 0xbc, u(ri(0, 2)));
            wf(m + 0x48, rf(-3.1f, 3.1f));
            { Vec4 q = ee_ref<Vec4>(m + 0x10); const float r = ri(0, 3) == 0 ? rf(0, 6) : (coin() ? rf(6, 30) : rf(30, 80)); const float a = rf(-3.1f, 3.1f);
              q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-4, 12); ee_ref<Vec4>(P + 0x80) = q; }
            for (uint32_t o = 0x20; o < 0x200; o += 4) {  // a few of the pvars kept as they are typed: floats scaled, small counters moved
                if (ri(0, 9) != 0) continue;
                const uint32_t v = ee_read<uint32_t>(pv + o), ex = v & 0x7f800000u;
                if (ex >= 0x30000000u && ex < 0x50000000u) wf(pv + o, ee_read<float>(pv + o) * rf(0.5f, 1.5f));
                else if (v < 16) w32(pv + o, u(ri(0, static_cast<int>(v) + 2)));
            }
            give_hit(m, ri(0, 1) ? 0x210000u : 0x330000u);
            if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
                const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                w8(e + 0x28, u(ri(0, 11))); w8(e + 0x29, u(ri(0, 4))); w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
                wf(e + 0x1c, coin() ? 5627.95f : 0.0f);
                w32(e + 0x20, ri(0, 3) == 0 ? 0u : ee_read<uint32_t>(P + 0x2080));
                wf(e + 0x2c, coin() ? rf(0.1f, 6) : 0.0f);
            }
        } else {
            hero_near(m);
        }
        w32(OV(0x0015f5cc), R() & 0xffff);
        seed_rand();
    };
    if (g_oracle_level == 1 && std::getenv("B103_DBG") != nullptr) {
        l1check("tr_2b7a48", 0x002b7a48, 10, Ret::Int32, [level] { level(); return Args{{0x001e34c0u, 0x15u}, {}}; },
                [](A a) { return ri32(static_cast<int32_t>(nov_2b7a48_l1(a.i[0], a.i[1], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).v0)); });
    }
    for (const TrCls& c : trcls) {
        if (std::getenv("B103_CLS") != nullptr && std::strcmp(std::getenv("B103_CLS"), c.name) != 0) continue;
        for (int st = 0; st < 8; ++st)
        for (int pass = 0; pass < 2; ++pass)
        if ((c.skip_states & (1u << st)) == 0) classcheck(c.name, c.addr, c.fn, [st, pass, c, generic_prep](uint32_t m) { generic_prep(m, st, pass, c.fn); }, 20);
    }
    g_extra_noop_hooks.clear();

    // ---- the classes shared with Veldin (transliterated at Veldin's addresses, run on both levels: ORACLE_LEVEL=0 / 1)
    {
        g_extra_noop_hooks = {0x00122298, 0x00118a80, 0x00122fe0, 0x001232c8, OVF(0x002a08c8), OVF(0x0029a6b0), OVF(0x00299460), OVF(0x0023e008),
                              OVF(0x0026ced0), OVF(0x0024d430), OVF(0x00266040), OVF(0x00264740)};
        auto sharedcheck = [level, generic_prep](const char* name, uint32_t vaddr, void (*port)(uint32_t), int n) {
            if (g_only_batch != 0 && g_only_batch != g_batch) return;
            if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
            level();
            const uint32_t fn = ovl_xlate_code(vaddr);
            std::vector<uint32_t> ms;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) ms.push_back(m);
            if (ms.empty()) { std::printf("%-26s 0x%08x  no moby of the class on level %d\n", name, vaddr, g_oracle_level); return; }
            for (int st = 0; st < 8; ++st)
            for (int pass = 0; pass < 2; ++pass) {
                std::printf("%-26s 0x%08x\n", name, vaddr);
                if (!check_fn(name, fn, n, Ret::None, [level, ms, st, pass, generic_prep, port] {
                    level();
                    const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                    // the pvars' small values are indices into the level's tables (the Infobot's path table): kept as they are
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    uint32_t keep[0x200 / 4];
                    for (uint32_t o = 0x20; o < 0x200; o += 4) keep[o / 4] = ee_read<uint32_t>(pv + o);
                    generic_prep(m, st, pass, port);
                    for (uint32_t o = 0x20; o < 0x200; o += 4) if (keep[o / 4] < 16) w32(pv + o, keep[o / 4]);
                    if (ee_read<uint32_t>(pv + 0x70) == 0xffffffffu) w32(pv + 0x70, 0);  // no path: the table's -1th word is the float 1.0 (the original reads it as a pointer)
                    if (ee_read<uint32_t>(pv + 0x58) == 0xffffffffu) w32(pv + 0x58, 0);  // (state 6's path index)
                    if (std::getenv("B103_SH") != nullptr) {
                        const uint32_t i = ee_read<uint32_t>(pv + 0xa8), k = ee_read<uint32_t>(pv + i * 4 + 0x70);
                        std::printf("  sh: st %d pass %d idx %u slot %u tab %08x\n", st, pass, i, k, ee_read<uint32_t>(OV(0x001b04b0) + k * 4));
                    }
                    return Args{{m}, {}};
                }, [port](A a) { port(a.i[0]); return uint64_t{0}; }, Scope::FullRam)) ++g_fail;
            }
        };
        sharedcheck("vendor_update", 0x002a57a8, vendor_update_tr, 20);
        sharedcheck("infobot_update", 0x002d5de8, infobot_update_tr, 20);
        g_extra_noop_hooks.clear();
    }

    //@@NEXT
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
