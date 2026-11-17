// Oracle batch 97 (task 0320): Novalis's own moby classes (level 1) on its real level data. Run with
// ORACLE_LEVEL=1 build/tests/ovl_oracle --batch 97 (needs the ISO). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the
// original is Novalis's own function at its own address, the port the one registered for it.

#include "ovl_oracle.h"

#include <game/ovl/checkpoint.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/walker.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/disc.h>

#include <optional>
#include <cmath>
#include <cstring>
#include <map>

namespace rac1::oracle {

namespace {
// The checks named here reach sphere_collide or capsule_collide, whose chunk buffers, unpacked vertices and cell
// walk the port keeps on the host stack (the original: 0x70000000 .. 0x70002000, 0x70002000, 0x70003800): A2's
// collision scratch ranges are left out of these checks, and only of them (stage 5, task C2b).
bool c2b_spr_check(const char* name) {
    static const char* const kNames[] = {"fan_update", "peckbot_update", "peckbot_update_state", "scripted_camera_trigger",
                                         "spawn_point_update", "water_lily_update"};
    for (const char* n : kNames)
        if (std::strcmp(n, name) == 0) return true;
    return false;
}

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
    std::optional<CollisionScratchIgnored> spr_ignored;
    if (c2b_spr_check(name)) spr_ignored.emplace();
    if (!check_fn(name, l1addr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_97() {
    if (g_only_batch != 97) return;
    g_batch = 97;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime

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

    // ---- the plants (classes 1813 tall shroom, 1042 its underside, 754 green mushroom)
    for (int pass = 0; pass < 3; ++pass)
    classcheck("tall_shroom_update", 0x0030d0f0, tall_shroom_update_l1, [pass, give_hit, hero_near](uint32_t m) {
        w8(m + 0x20, pass == 0 ? u(ri(0, 3)) : (pass == 1 ? 1u : 2u));
        give_hit(m, 0x10000); hero_near(m);
        w32(OV(0x0015f424), u(ri(0, 3) == 0 ? 0x3e : 0));  // the draw hooks' count: room or none
    });
    for (int pass = 0; pass < 2; ++pass)
    classcheck("mushroom_underside_update", 0x00307c48, mushroom_underside_update_l1, [give_hit](uint32_t m) { give_hit(m, 0x10000); });
    // the debris' velocity / spin w lanes: a VU0 register leftover of the original's sin / cos (the port's has none)
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    if (g_oracle_level == 1) level();
    for (uint32_t a = g_oracle_level == 1 ? ee_read<uint32_t>(OV(0x0015ffe0)) : pvars_lo(); a < (g_oracle_level == 1 ? ee_read<uint32_t>(OV(0x0015ffe0)) : pvars_lo()) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
    for (int pass = 0; pass < 3; ++pass)
    classcheck("green_mushroom_update", 0x002fd9a0, green_mushroom_update_l1, [pass, give_hit, hero_near](uint32_t m) {
        w8(m + 0x20, pass == 0 ? u(ri(0, 3)) : (pass == 1 ? 1u : 2u));
        give_hit(m, 0x10000); hero_near(m);
    });
    g_ignored_words = saved_ignored;

    // ---- the Peckbot (class 577) and its helpers
    if (g_oracle_level == 1) {
        l1check("moby_group_set_bc", 0x0026e090, 40, Ret::None, [level] {
            level();
            return Args{{u(ri(0, 12)), R() & 0xffu}, {}};
        }, [](A a) { moby_group_set_bc_l1(a.i[0], a.i[1]); return uint64_t{0}; });
        l1check("walker_record_init", 0x0026d930, 4, Ret::None, [level] {
            level(); wrand(OUT, 16);
            return Args{{OUT}, {}};
        }, [](A a) { walker_record_init_l1(a.i[0]); return uint64_t{0}; });
        l1check("cooldown_slot_claim", 0x002efbf8, 60, Ret::Int32, [level] {
            level();
            const uint32_t arr = OUT; const int32_t now = ee_read<int32_t>(OV(0x0015f5cc));
            for (uint32_t i = 0; i < 4; ++i) w32(arr + 4 * i, u(now + ri(-3, 3)));
            return Args{{arr, u(ri(-1, 4)), u(ri(0, 100))}, {}};
        }, [](A a) { return ri32(cooldown_slot_claim_l1(a.i[0], static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]))); });
        l1check("path_next_point", 0x0028b5e0, 80, Ret::Int32, [level] {
            level();
            const uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(1, 300));
            const uint32_t path = OUT, n = u(ri(0, 5));
            w32(path, n);
            Vec4 base = ee_ref<Vec4>(m + 0x10);
            for (uint32_t i = 0; i < 6; ++i) { Vec4 p = base; p.x += rf(-20, 20); p.y += rf(-20, 20); p.z += rf(-3, 3); ee_ref<Vec4>(path + 0x10 + 0x10 * i) = p; }
            Vec4 t = base; t.x += rf(-30, 30); t.y += rf(-30, 30); ee_ref<Vec4>(OUT + 0x200) = t;
            wrand(OUT + 0x300, 4);
            return Args{{m, path, OUT + 0x200, OUT + 0x300}, {}};
        }, [](A a) { return ri32(path_next_point_l1(a.i[0], a.i[1], a.i[2], a.i[3])); });
    }
    // the Peckbot's class hooks (its carry / suck record functions, Veldin's 0x2db8f8.. not ported): no-ops on both sides
    g_extra_noop_hooks.clear();  // (the Peckbot / Amoeboid class hooks are ported now: task 0330)
    const std::vector<uint32_t> saved_ignored_pb = g_ignored_words;
    if (g_oracle_level == 1) {
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
    }
    for (int st : {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x63})
    classcheck(st == 0 ? "peckbot_update" : "peckbot_update_state", 0x002efc60, peckbot_update_l1, [st, give_hit](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t P = 0x0013f350;
        if (st != 0) {  // set up first (state 0, the port's), then perturbed in the state under test
            w8(m + 0x20, 0);
            peckbot_update_l1(m);
        }
        w8(m + 0x20, u(st));
        if (ri(0, 2) == 0) { wf(pv + 0x1fc, coin() ? 24.0f : 12.0f); }
        w32(pv + 0x38, ri(0, 4) == 0 ? 1u : 0u);
        w16(pv + 0x208, ri(0, 3) == 0 ? u(ri(0, 3)) : 0u); w16(pv + 0x214, ri(0, 3) == 0 ? u(ri(0, 3)) : 0u); w16(pv + 0x26, ri(0, 2) == 0 ? u(ri(0, 100)) : 0u);
        w16(pv + 0x20a, R() & 1);
        w32(pv + 0x210, ri(0, 3) == 0 ? u(ri(0, 3)) : u(ri(0, 100))); w32(pv + 0x20c, ri(0, 3) == 0 ? u(ri(0, 3)) : u(ri(0, 100))); w32(pv + 0x228, ri(0, 3) == 0 ? u(ri(0, 3)) : 0u);
        wf(pv + 0x1f8, coin() ? 45.0f : -45.0f); wf(pv + 0x1f4, rf(-1, 1)); wf(pv + 0x204, rf(0.1f, 0.6f));
        w8(m + 0xbc, u(ri(0, 2))); w8(m + 0x21, ri(0, 2) == 0 ? 0xffu : u(ri(0, 3)));
        w8(m + 0x53, u(ri(0, 8))); w8(m + 0x52, coin() ? ee_read<uint32_t>(m + 0x53) : u(ri(0, 8))); w8(m + 0x70, u(R() & 3));
        wf(m + 0x48, rf(-3.1f, 3.1f)); wf(pv + 0x198, rf(-0.3f, 0.3f)); wf(pv + 0x48, rf(-0.2f, 0.2f));
        // the hero near the Peckbot (or far from it), the home near it
        { Vec4 q = ee_ref<Vec4>(m + 0x10); const float r = ri(0, 3) == 0 ? rf(0, 6) : (coin() ? rf(6, 30) : rf(30, 80)); const float a = rf(-3.1f, 3.1f);
          q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-4, 12); ee_ref<Vec4>(P + 0x80) = q; }
        { Vec4 h = ee_ref<Vec4>(m + 0x10); h.x += rf(-6, 6); h.y += rf(-6, 6); h.z += rf(-1, 1); ee_ref<Vec4>(pv + 0x1d0) = h; Vec4 g = h; g.x += rf(-5, 5); g.y += rf(-5, 5); ee_ref<Vec4>(pv + 0x1e0) = g; }
        wf(m + 0x18, ee_read<float>(m + 0x18) + (coin() ? 0.0f : rf(-3, 3)));
        w32(pv + 0x21c, ri(0, 3) == 0 ? u(ri(0, 5)) : 0xffffffffu);
        w16(pv + 0x216, u(ri(0, 12))); w32(pv + 0x218, ri(0, 2) == 0 ? ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(1, 300)) : 0u);
        wf(pv + 0x20, rf(-0.5f, 2));
        give_hit(m, 0x330000);
        if (ri(0, 4) == 0) w32(0x0015f5c4, 2u);
        w32(OV(0x0015f5cc), R() & 0xffff);
        for (uint32_t i = 0; i < 3; ++i) w32(L1_ADDR(0x00161a80) + 4 * i, ri(0, 1) ? 0u : R() & 0xffff);
        seed_rand();
    }, 40);
    g_ignored_words = saved_ignored_pb;
    g_extra_noop_hooks.clear();

    // the moby array index of a real crank (class 280)
    auto crank_index = [level] {
        level();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        for (uint32_t m = first; m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<int16_t>(m + 0xa6) == 280) return (m - first) >> 8;
        return 0u;
    };
    // a sound channel for the moby: slot or -1, the channel's owner and flag
    auto sound_slot = [](uint32_t m, uint32_t where) {
        const int32_t slot = ri(0, 3) == 0 ? -1 : ri(0, 15);
        w32(where, u(slot));
        if (slot >= 0) { const uint32_t c = 0x0013e550 + u(slot) * 0x70; w32(c + 0x88, ri(0, 3) == 0 ? 0u : m); w8(c + 0x74, R() & 1); }
    };

    // ---- doors, platforms, fan, bridge half
    if (g_oracle_level == 1) level();
    const uint32_t crank = g_oracle_level == 1 ? crank_index() : 0;
    for (int pass = 0; pass < 3; ++pass)
    classcheck("door_half_update", 0x002f4710, door_half_update_l1, [pass, crank, sound_slot](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, pass == 0 ? u(ri(0, 2)) : 1u);
        w32(pv, ri(0, 5) == 0 ? u(ri(-1, 300)) : crank);
        wf(pv + 4, rf(-20, 20)); wf(pv + 8, rf(-20, 20));
        wf(pv + 0xc, ri(0, 5) == 0 ? 0.0f : rf(-20, 20)); wf(pv + 0x10, ri(0, 5) == 0 ? 0.0f : rf(-20, 20));
        sound_slot(m, pv + 0x14);
        const uint32_t q = ee_read<uint32_t>(ee_read<uint32_t>(OV(0x0015ffd8)) + (ee_read<uint32_t>(pv) << 8) + 0x78);
        const float cur = ri(0, 2) == 0 ? 0.0f : (coin() ? 1.0f : rf(0, 1));
        if (ee_read<uint32_t>(pv) < 300) wf(q, cur);
        wf(pv + 0x18, ri(0, 2) == 0 ? cur : rf(0, 1));
    });
    for (int pass = 0; pass < 6; ++pass)
    classcheck(pass < 3 ? "door_update" : "door_update_769", 0x002fed68, door_update_l1, [pass](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, pass % 3 == 0 ? u(ri(0, 5)) : u(1 + (R() % 4)));
        Vec4 c = ee_ref<Vec4>(m + 0x10); c.x += rf(-1, 1); c.y += rf(-1, 1);
        ee_ref<Vec4>(pv) = c;
        wf(pv + 0x10, ri(0, 3) == 0 ? rf(0, 1) : (coin() ? 0.0f : 1.0f)); wf(pv + 0x14, rf(2, 12));
        Vec4 h = c; const float r = rf(0, 20); const float a = rf(-3, 3); h.x += r * std::cos(a); h.y += r * std::sin(a); h.z += rf(-3, 3);
        ee_ref<Vec4>(0x0013f3d0) = h;
        Vec4 k = c; k.x += rf(-25, 25); k.y += rf(-25, 25);
        ee_ref<Vec4>(OV(0x00166dc0)) = k;
        wf(m + 0x48, rf(-3.1f, 3.1f)); w16(m + 0xa6, coin() ? 0x300u : 0x301u);
    }, 50);
    for (int pass = 0; pass < 4; ++pass)
    classcheck("piston_platform_update", 0x002f95c0, piston_platform_update_l1, [pass](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, pass == 0 ? u(ri(0, 1)) : 1u);
        w8(m + 0xbc, u(ri(0, 3)));
        w16(pv + 0xa6, ri(0, 2) == 0 ? u(ri(0, 3)) : 0u);
        wf(pv + 0xa8, rf(-5, 5)); wf(pv + 0xac, rf(-0.2f, 0.2f)); wf(pv + 0xb0, rf(0.1f, 3)); wf(pv + 0xa0, rf(-10, 10));
        w8(pv + 0xa4, R() & 3);
    }, 60);
    for (int pass = 0; pass < 3; ++pass)
    classcheck("fan_update", 0x002f9bf0, fan_update_l1, [pass](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, pass == 0 ? 0u : 1u);
        wf(pv, ri(0, 3) == 0 ? rf(-1, 1) : (coin() ? 1.0f : -1.0f)); wf(pv + 4, coin() ? 1.0f : -1.0f);
        if (ri(0, 2) == 0) wf(pv + 4, ee_read<float>(pv));
        w32(pv + 8, ri(0, 2) == 0 ? u(ri(0, 2)) : u(ri(0, 200)));
        wf(m + 0x48, rf(-3.1f, 3.1f));
        seed_rand();
    }, 60);
    for (int pass = 0; pass < 4; ++pass)
    classcheck("bridge_half_update", 0x002fb8a8, bridge_half_update_l1, [pass, sound_slot](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, pass == 0 ? u(ri(0, 3)) : u(pass));
        w8(m + 0xbc, ri(0, 2) == 0 ? 2u : u(ri(0, 1)));
        wf(pv + 0x70, ri(0, 3) == 0 ? rf(0, 0.2f) : rf(0, 1)); sound_slot(m, pv + 0x74);
        wfrand(pv + 0x60, 4, -30, 30); wf(m + 0x48, rf(-3.1f, 3.1f));
        const uint32_t lv = ee_read<uint32_t>(0x0015ed84); w8(m + 0xb0, u(ri(0, 15)));
        for (uint32_t i = 0; i < 16; ++i) w8(0x0014c050 + lv * 16 + i, coin() ? 0xffu : 0u);
    }, 60);

    // ---- the lift (class 726)
    for (int pass = 0; pass < 6; ++pass)
    classcheck("lift_update", 0x002b9eb0, lift_update_l1, [pass, hero_near](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t P = 0x0013f350;
        const uint32_t level_no = ee_read<uint32_t>(0x0015ed84);
        w8(m + 0x20, pass < 2 ? u(pass) : 1u);
        w8(m + 0xbc, pass < 2 ? u(ri(0, 3)) : u(ri(0, 2)));
        if (ri(0, 5) == 0) w32(pv + 0xb4, 0xffffffffu);
        if (ri(0, 3) == 0) w32(pv + 0xcc, u(ri(-1, 3))); if (ri(0, 3) == 0) w32(pv + 0xc0, u(ri(-1, 3)));
        w16(pv + 0xba, ri(0, 2) == 0 ? u(ri(0, 4)) : 0u); w16(pv + 0xb8, R() & 1);
        wf(pv + 0xb0, rf(0.05f, 1.0f)); wf(pv + 0xa4, rf(-0.2f, 1.2f)); wf(pv + 0xa8, rf(-0.05f, 0.05f)); wf(pv + 0xac, rf(-0.05f, 0.05f));
        if (ri(0, 4) == 0) wf(pv + 0xac, 0);
        wf(pv + 0xbc, rf(-5, 5)); w8(pv + 0xa0, R() & 1); w32(pv + 0xc8, R() & 1);
        if (pass >= 2 && ri(0, 2) == 0) wf(pv + 0xa4, coin() ? rf(0.97f, 1.01f) : rf(-0.01f, 0.03f));
        const int32_t slot = ri(0, 3) == 0 ? -1 : ri(0, 15);
        w32(pv + 0xc4, u(slot));
        if (slot >= 0) { const uint32_t c = 0x0013e550 + u(slot) * 0x70; w32(c + 0x88, ri(0, 3) == 0 ? 0u : m); w8(c + 0x74, R() & 1); }
        hero_near(m);
        if (ri(0, 2) == 0) { Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-2, 2); q.y += rf(-2, 2); q.z += rf(-0.5f, 3); ee_ref<Vec4>(0x0013f3d0) = q; }
        static const uint32_t modes[] = {3, 0, 1, 2}, states[] = {0x1c, 0, 5, 0x13};
        w32(P + 0x208c, modes[ri(0, 3)]); w32(P + 0x2084, states[ri(0, 3)]);
        w32(P + 0x4f8, coin() ? m : 0u); w32(P + 0x2fc, coin() ? m : 0u); w16(P + 0x30e, ri(0, 3) == 0 ? 1u : 0u);
        // the hero in a box (the lift's or its other): the boxes' index in the table
        if (coin()) {
            const int32_t b = static_cast<int32_t>(ee_read<uint32_t>(pv + (coin() ? 0xccu : 0xc0u)));
            if (b >= 0 && b < 8) { const uint32_t box = ee_read<uint32_t>(OV(0x001600ec)) + u(b << 7); for (uint32_t k = 0; k < 3; ++k) wf(0x0013f3d0 + 4 * k, ee_read<float>(box + 0x30 + 4 * k) + rf(-0.4f, 0.4f)); }
        }
        const uint32_t idx = u(ri(0, 0x7f));
        w16(m + 0xb2, idx);
        w8(OV(0x001bb230) + idx + 0x454, ri(0, 3) == 0 ? 1u : 0u);
        for (uint32_t i = 0; i < 4; ++i) w32(0x0014c190 + level_no * 256 + i * 4, ri(0, 3) == 0 ? R() : 0u);
    }, 60);

    // ---- the scripted-camera trigger (class 737)
    g_extra_noop_hooks = {};  // (L1:0x317070 = Veldin's 0x2eac18, the scripted camera's end: ported since task 0364, run on both sides)
    for (int pass = 0; pass < 4; ++pass)
    classcheck("scripted_camera_trigger", 0x002fb5b0, scripted_camera_trigger_update_l1, [pass, hero_near](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t level_no = ee_read<uint32_t>(0x0015ed84);
        w8(m + 0x20, pass == 0 ? u(ri(0, 4)) : u(pass));
        w8(m + 0xbc, R() & 1);
        w32(pv + 0x20, u(ri(-1, 3))); w32(pv + 0x24, ri(0, 6) == 0 ? 0xffffffffu : u(ri(0, 3))); w32(pv + 0x28, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 3)));
        w32(pv + 0x2c, u(ri(0, 4))); w32(pv + 0x30, R() & 1); w32(pv + 0x34, R());
        wfrand(pv, 8, -5, 5);
        const uint32_t idx = u(ri(0, 0x7f));
        w16(m + 0xb2, idx);
        w8(OV(0x001bb230) + idx + 0x454, ri(0, 3) == 0 ? 1u : 0u);
        for (uint32_t i = 0; i < 4; ++i) w32(0x0014c190 + level_no * 256 + i * 4, ri(0, 3) == 0 ? R() : 0u);
        if (coin()) {  // the hero in the box
            const int32_t b = static_cast<int32_t>(ee_read<uint32_t>(pv + 0x20));
            if (b >= 0) { const uint32_t box = ee_read<uint32_t>(OV(0x001600ec)) + u(b << 7); for (uint32_t k = 0; k < 3; ++k) wf(0x0013f3d0 + 4 * k, ee_read<float>(box + 0x30 + 4 * k) + rf(-0.3f, 0.3f)); }
        }
        // the scripted camera's node list
        w32(OV(0x0015f5c4), coin() ? 2u : u(ri(0, 3))); w8(0x0015edb0, ri(0, 3) != 0 ? 1u : 0u);
        const uint32_t list = 0x0016cce0, first = ee_read<uint32_t>(OV(0x0015ffd8));
        w16(list + 0x44, u(ri(0, 3)));
        for (uint32_t i = 0; i < 4; ++i) {
            const uint32_t t = first + 0x100 * u(ri(0, 100));
            w32(list + 0x178 + 4 * i, t);
            w16(t + 0xa6, ri(0, 2) == 0 ? 0x398u : (coin() ? 0x1bfu : u(ri(0, 100))));
        }
        w8(0x0017c8c1, R() & 1);
    }, 80);
    g_extra_noop_hooks.clear();

    // ---- the cave door (class 641)
    for (int pass = 0; pass < 2; ++pass)
    classcheck("cave_door_update", 0x002f4348, cave_door_update_l1, [pass](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, pass == 0 ? u(ri(0, 2)) : 1u);
        if (pass > 0) {
            wf(pv + 4, rf(-3.1f, 3.1f)); wf(pv + 8, rf(-3.1f, 3.1f));
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
            const uint32_t t = ri(0, 3) == 0 ? first + 0x100 * u(ri(0, 200)) : first;  // some other moby, or the crank
            w32(pv, ri(0, 6) == 0 ? 0xffffffffu : (t - first) >> 8);
            if (coin()) w16(t + 0xa6, 0x118);
            if (ri(0, 2) == 0) wf(ee_read<uint32_t>(t + 0x78), rf(-4, 4));
        }
    });

    // ---- the walkers' stepping (Veldin's addresses: the level's equivalents run)
    {
        // a real moby standing on the level (class 577, the Peckbot, on Novalis; any moby with a pvar block else)
        auto walker_moby = [level](int pick) -> uint32_t {
            level();
            std::vector<uint32_t> v;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x78) != 0 && ee_read<int16_t>(m + 0xa6) != 0) v.push_back(m);
            return v[static_cast<size_t>(pick) % v.size()];
        };
        auto set_rec = [](uint32_t rec) {
            w32(rec, coin() ? 0x280u : u(ri(0x100, 0x400))); wf(rec + 4, rf(0.5f, 1.5f)); wf(rec + 8, rf(0.2f, 1.0f)); wf(rec + 0xc, rf(0.2f, 1.0f));
            wf(rec + 0x10, rf(-2, 2)); wf(rec + 0x14, 0); wf(rec + 0x18, rf(-1, 1)); wf(rec + 0x1c, rf(0.1f, 0.5f)); wf(rec + 0x20, rf(0.1f, 0.5f));
            wf(rec + 0x24, rf(0.5f, 4)); wf(rec + 0x28, 1.57f); wf(rec + 0x2c, rf(0, 1)); wf(rec + 0x30, rf(0, 1));
            w32(rec + 0x38, R() & 0x2f); wf(rec + 0x4c, rf(-3.1f, 3.1f));
        };
        {
        // (stage 5, task C2c) walker_probe reaches segment_collide: A2's collision scratch
        // ranges left out of this check (see batch 43's checks of it)
        const CollisionScratchIgnored spr_ignored;
        check("walker_probe", 0x002583f0, 80, Ret::Int32, [walker_moby] {
            const uint32_t m = walker_moby(ri(0, 1000));
            Vec4 p = ee_ref<Vec4>(m + 0x10); p.x += rf(-3, 3); p.y += rf(-3, 3); p.z += rf(-2, 2); if (ri(0, 3) == 0) p.z = rf(-1, 0.6f); ee_ref<Vec4>(OUT) = p;
            w32(OV(0x00173e58), ri(0, 2) == 0 ? m : 0u); w32(OV(0x00173e5c), u(ri(-2, 3)));
            return Args{{OUT, m, ri(0, 1) ? 0x20u : 0u}, {rf(0, 2), rf(0, 2)}};
        }, [](A a) { return r32(static_cast<uint32_t>(walker_probe(a.f[0], a.f[1], a.i[0], a.i[1], a.i[2]))); });
        }
        // (the float args are in f12 / f13; the generator's a0.. are pos, self, flag)
        {
        // (stage 5, task C2b) walker_step reaches sphere_collide or capsule_collide: A2's collision scratch
        // ranges left out of this check (see batch 43's checks of it)
        const CollisionScratchIgnored spr_ignored;
        check("walker_step", 0x00258b50, 160, Ret::Int32, [walker_moby, set_rec] {
            const uint32_t m = walker_moby(ri(0, 1000));
            const uint32_t rec = T, target = T + 0x100, out = T + 0x140;
            set_rec(rec);
            Vec4 p = ee_ref<Vec4>(m + 0x10);
            p.x += rf(-1, 1); p.y += rf(-1, 1); p.z += rf(0, 1);
            ee_ref<Vec4>(target) = p;
            Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-12, 12); q.y += rf(-12, 12);
            ee_ref<Vec4>(target) = ri(0, 4) == 0 ? p : q;
            wf(m + 0x48, rf(-3.1f, 3.1f));
            wrand(out, 4);
            return Args{{m, rec, target, out}, {ri(0, 5) == 0 ? rf(0, 4) : 1.0f}};
        }, [](A a) { return r32(walker_record_step(a.f[0], a.i[0], a.i[1], a.i[2], a.i[3])); });
        }
    }

    // ---- the checkpoint helpers (Veldin's addresses: the level's equivalents run)
    check("checkpoint_mark_collected", 0x002502a0, 40, Ret::None, [level] {
        level();
        w32(0x0015ed84, u(ri(0, 18)));
        return Args{{coin() ? u(ri(0, 15)) : (coin() ? 0xffu : R() & 0xffu)}, {}};
    }, [](A a) { checkpoint_mark_collected(a.i[0]); return uint64_t{0}; });
    check("checkpoint_save", 0x00284e50, 40, Ret::None, [level] {
        level();
        w32(0x0015ed84, u(ri(0, 18)));
        for (uint32_t i = 0; i < 0x7ff; ++i) w8(OV(0x001ba620) + 0x404 + i, ri(0, 3) == 0 ? u(ri(0, 20)) : (coin() ? 0u : 1u));
        for (uint32_t i = 0; i < 16 * 19; ++i) w8(0x0014c050 + i, coin() ? 0xffu : u(ri(0, 3)));
        wrand(OV(0x001ba4d0), 0x40); wrand(0x0014c190, 0x50); wrand(OV(0x001bb280), 0x20);
        const uint32_t P = 0x0013f350, HUD = 0x0013e550;
        w32(P + 0x2080, ri(0, 5) == 0 ? ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(0, 200)) : ee_read<uint32_t>(P + 0x2080));
        w16(P + 0x22cc, R()); w8(P + 0x20a4, R()); w32(HUD + 0x64, R()); w8(HUD + 0x68, R()); w8(HUD + 0x69, R()); w8(HUD + 0x6a, R());
        w8(0x001516d0 + 0x22, coin() ? 0xffu : R()); w16(0x001516d0 + 0x38, R());
        wrand(OUT, 4); wrand(OUT + 0x20, 4);
        return Args{{OUT, OUT + 0x20}, {}};
    }, [](A a) { checkpoint_save(a.i[0], a.i[1]); return uint64_t{0}; });

    // ---- the spawn point (class 805)
    for (int pass = 0; pass < 3; ++pass)
    classcheck("spawn_point_update", 0x00300220, spawn_point_update_l1, [pass](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t level_no = ee_read<uint32_t>(0x0015ed84);
        w32(pv + 8, pass == 0 ? R() & 0x1e : (R() & 0x16));
        w8(m + 0xbc, ri(0, 5) == 0 ? 0u : 1u); w8(m + 0x20, R() & 1);
        w32(pv + 0xc, R() & 1); w32(pv + 0x10, R() & 1); w32(pv + 0x14, R() & 1);
        w32(pv + 0x18, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 15)));
        if (ri(0, 3) == 0) w8(m + 0xb0, ri(0, 2) == 0 ? 0xffu : u(ri(0, 15)));
        for (uint32_t i = 0; i < 16; ++i) w8(0x0014c050 + level_no * 16 + i, coin() ? 0xffu : 0u);
        const uint32_t idx = u(ri(0, 0x7f));
        w16(m + 0xb2, idx);
        w8(OV(0x001bb230) + idx + 0x454, R() & 1); w8(OV(0x001ba5d0) + idx + 0x454, R() & 1);
        for (uint32_t i = 0; i < 4; ++i) { w32(0x0014c190 + level_no * 256 + i * 4, R()); w32(OV(0x001ba4d0) + i * 4, R()); }
        static const uint32_t ks[] = {0, 1, 3, 5}, runs[] = {0, 1, 5, 9};
        w8(0x001413f4, ks[ri(0, 3)]); w32(0x001413dc, runs[ri(0, 3)]);
        if (coin()) {  // the hero in the box of the flags' index (or elsewhere)
            const int32_t b = ri(0, 3) == 0 ? ri(-1, 3) : static_cast<int32_t>(ee_read<uint32_t>(pv));
            const uint32_t box = ee_read<uint32_t>(OV(0x001600ec)) + u(b << 7);
            if (b >= 0) for (uint32_t k = 0; k < 3; ++k) wf(0x0013f3d0 + 4 * k, ee_read<float>(box + 0x30 + 4 * k) + rf(-0.5f, 0.5f));
        }
        if (ri(0, 3) == 0) w32(pv, u(ri(-1, 3)));
        if (ri(0, 2) == 0) wf(0x0013f3d0 + 8, rf(-50, 500));
    }, 80);

    // ---- the help-message controller (class 1341)
    check("moby_state_is_1", 0x002d8128, 20, Ret::Int32, [level] {
        level();
        const uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(0, 200));
        w8(m + 0x20, u(ri(0, 3)));
        return Args{{m}, {}};
    }, [](A a) { return r32(static_cast<uint32_t>(moby_state_is_1(a.i[0]))); });
    for (int pass = 0; pass < 6; ++pass)
    classcheck("help_controller_update", 0x0030acb8, help_controller_update_l1, [pass](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t S = 0x00141848, H = 0x00141968, P = 0x0013f350, G = OV(0x00179410);
        w8(m + 0x20, pass == 0 ? u(ri(0, 2)) : 1u);
        // the boxes (the pvar's indexes), the hero in one of them most of the time
        const uint32_t offs[] = {0x3c, 0x38, 0x30, 0x20, 0x24};
        for (uint32_t o : offs) w32(pv + o, ri(0, 5) == 0 ? 0xffffffffu : u(ri(0, 4)));
        if (ri(0, 4) != 0) {
            const int32_t b = static_cast<int32_t>(ee_read<uint32_t>(pv + offs[ri(0, 4)]));
            if (b >= 0) { const uint32_t box = ee_read<uint32_t>(OV(0x001600ec)) + u(b << 7); for (uint32_t k = 0; k < 3; ++k) wf(0x0013f3d0 + 4 * k, ee_read<float>(box + 0x30 + 4 * k) + rf(-0.3f, 0.3f)); }
        }
        static const uint32_t runs[] = {0, 1, 9, 0x11, 0x12, 2, 3, 0x12, 1, 0};
        w32(P + 0x208c, runs[ri(0, 9)]); w32(P + 0x2084, coin() ? 1u : u(ri(0, 0x1d))); w32(P + 0x2090, coin() ? 0x3bu : 0u); w32(P + 0x198, ri(0, 3) == 0 ? 1u : 0u);
        w32(G, ri(0, 4) == 0 ? 1u : 0u); w32(G + 0x24, ri(0, 4) == 0 ? 5u : 0xffffffffu);
        w8(0x0013dd42, R() & 1); w8(0x0013dd43, R() & 1);
        for (uint32_t o : {0x20u, 0x28u, 0x30u, 0x8u, 0x10u, 0x200u, 0x208u, 0x210u}) w16(H + o, ri(0, 2) == 0 ? u(ri(0, 2)) : 0u);
        w32(H + 0x21c, R()); w32(H + 0x28c, u(ri(-1, 1))); w32(H + 0x394, u(ri(-1, 1))); w16(H + 0x220, R());
        for (uint32_t o : {0x58u, 0x60u, 0x62u, 0xe8u, 0xeau, 0xb0u, 0xb2u, 0xb8u, 0xbau, 0x68u, 0x6au}) w16(S + o, ri(0, 3) == 0 ? u(ri(0, 20)) : (ri(0, 6) == 0 ? 0xffffu : 0u));
        w32(S + 0xb4, R()); w32(S + 0xbc, R()); w32(S + 0x6c, R());
        w16(0x00141892, u(ri(0, 5)));
        w32(0x0015eea4, u(ri(0, 20000))); w32(OV(0x0015f5cc), u(ri(0, 800))); w32(OV(0x0015f5c4), coin() ? 0u : 2u);
        w32(OV(0x0015f68c), u(ri(0, 5))); w32(OV(0x0015f688), ri(0, 2) == 0 ? 1u : 0xffffffffu);
        w32(pv + 0x28, ri(0, 5) == 0 ? 1u : 0u); w32(pv + 0x2c, u(ri(0, 500))); w32(pv + 0x34, u(ri(0, 7)));
        w32(0x00141000 + 0x5f8, R() & 0xff); w32(0x0015eda0, R() & 0xff);  // (hero +0x2228 = 0x1415f8)
        // some mobys of the live list as cranks (0x118) / the class-0x326 ones, near the hero
        uint32_t o = ee_read<uint32_t>(OV(0x0015ffe4));
        for (int i = 0; i < 6 && o != 0; ++i, o = ee_read<uint32_t>(o + 0x28)) {
            if (ri(0, 2) == 0) continue;
            w16(o + 0xa6, coin() ? 0x118u : 0x326u);
            Vec4 q = ee_ref<Vec4>(0x0013f3d0); q.x += rf(-8, 8); q.y += rf(-8, 8); q.z += rf(-2, 2); ee_ref<Vec4>(o + 0x10) = q;
            if (ee_read<uint32_t>(o + 0x78) != 0) wf(ee_read<uint32_t>(o + 0x78), ri(0, 2) == 0 ? 1.0f : rf(0, 1));
            w8(o + 0x20, u(ri(0, 2)));
            for (uint32_t k = 0; k < 4; ++k) wf(o + 4 * k, rf(-3000, 3000));
        }
    }, 60);

    // ---- the Plumber (class 774) and its helpers
    // 989snd's IOP server (as batch 80's): replies a function of the command; the stream system's disc reads
    // (command 0x38) done into EE RAM as the IRX does them
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
    if (g_oracle_level == 1) {
        l1check("moby_data_note", 0x0029b0a0, 60, Ret::None, [level] {
            level();
            const uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(1, 200));
            const uint32_t list = OUT + 0x100;
            const int32_t n = ri(0, 3);
            w32(list, u(n));
            const uint32_t at = ri(0, 1) ? m + 0x20 : ee_read<uint32_t>(m + 0x78) + 0x44;
            const int32_t kind = ri(0, 1);
            const int16_t off = static_cast<int16_t>(kind == 1 ? at - m : at - (ee_read<uint32_t>(m + 0x78) & 0xffff));
            for (int32_t i = 0; i < 4; ++i) {
                const uint32_t e = list + 4 + 0x10 * u(i);
                w32(e, ri(0, 1) ? u(static_cast<int32_t>(off)) : R()); w16(e + 8, ri(0, 1) ? 1u : u(ri(0, 3)));
                w16(e + 0xa, u(ri(0, 1))); w16(e + 0xc, ri(0, 1) ? ee_read<uint16_t>(m + 0xb2) : u(ri(0, 99))); w16(e + 0xe, ri(0, 1) ? ee_read<uint16_t>(m + 0xa6) : u(ri(0, 99)));
            }
            return Args{{at, 1, m, u(kind), list}, {}};
        }, [](A a) { moby_data_note_l1(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2], static_cast<int32_t>(a.i[3]), a.i[4]); return uint64_t{0}; });
        l1check("hero_walk_to", 0x002783a8, 20, Ret::None, [level] {
            level();
            const uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(1, 200));
            wf(m + 0x48, rf(-3.1f, 3.1f)); wrand(L1_ADDR(0x0016cce0) + 0x10, 12);
            return Args{{m}, {rf(0, 5)}};
        }, [](A a) { hero_walk_to_l1(a.f[0], a.i[0]); return uint64_t{0}; });
    }
    g_extra_noop_hooks = {0x00122298, OVF(0x00299460), OVF(0x0023e008), OVF(0x0026ced0), OVF(0x0024d430)};  // (the kernel's vsync wait, the movie player, scene effects, the level change: as batch 80)
    for (int st : {0, 2, 3, 5, 6, 7})  // (states 1 and 4 call talk_update, whose sound start waits forever on the mock IOP server of the original: batch 80 proves it)
    classcheck("plumber_update", 0x002ff118, plumber_update_l1, [st](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t P = 0x0013f350;
        w8(0x0013dd42, 0);
        if (st != 0) { w8(m + 0x20, 0); plumber_update_l1(m); }
        { const uint32_t Sd = 0x001516d0; w16(Sd + 0x5a, 3); w32(Sd + 0x50, (R() & 0x7ffffffeu) | 1u); w16(Sd + 0x5c, 0); w32(Sd + 0x1c, 0xffffffffu); w32(0x0015ec8c, 1); w32(0x00137b00, 0); }  // (the scene's sound ready, the stream system idle: as batch 80)
        w8(m + 0x20, u(st));
        w16(pv + 4, u(ri(0, 3)));  /* (4 would call level_change_request, whose original waits on the disc: tested in its own batches) */ w16(pv + 6, coin() ? 2u : u(ri(0, 3))); w8(pv + 8, R() & 1);
        w32(pv + 0x4c, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 4)));
        w32(OV(0x0015f5c4), ri(0, 3) == 0 ? 2u : 0u); w8(0x0015edb0, R() & 1);
        w32(pv + 0x164, ri(0, 2) == 0 ? u(ri(0, 5)) : 0u); w32(pv + 0x168, ri(0, 2) == 0 ? u(ri(0, 3)) : u(ri(0, 100)));
        w8(m + 0x53, ri(0, 3) == 0 ? u(ri(0, 3)) : 0u); w8(m + 0x31, R() & 1); wf(m + 0x48, rf(-3.1f, 3.1f));
        { Vec4 q = ee_ref<Vec4>(m + 0x10); const float r = ri(0, 2) == 0 ? rf(0, 8) : rf(8, 40); const float a = rf(-3.1f, 3.1f); q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-1, 2); ee_ref<Vec4>(P + 0x80) = q; }
        wfrand(P + 0xd0, 4, -1000, 1000); wfrand(P + 0x100, 4, -0.5f, 0.5f);
        w8(m + 0xb0, u(ri(0, 15)));
        for (uint32_t i = 0; i < 16; ++i) w8(0x0014c050 + ee_read<uint32_t>(0x0015ed84) * 16 + i, coin() ? 0xffu : 0u);
        wfrand(pv + 0x150, 4, -50, 50);
        seed_rand();
    }, 40);

    // ---- the water lily (class 695)
    for (int pass = 0; pass < 3; ++pass)
    classcheck("water_lily_update", 0x002f8268, water_lily_update_l1, [pass](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, pass == 0 ? u(ri(0, 2)) : 1u);
        Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-1, 1); q.y += rf(-1, 1); ee_ref<Vec4>(pv) = q;
        wfrand(pv + 0x10, 2, -0.3f, 0.3f);
        const float r = ri(0, 2) == 0 ? rf(0, 1.5f) : rf(1.5f, 12), a = rf(-3.1f, 3.1f);
        Vec4 h = ee_ref<Vec4>(m + 0x10); h.x += r * std::cos(a); h.y += r * std::sin(a); h.z += rf(-1.5f, 1.5f); ee_ref<Vec4>(0x0013f3d0) = h;
        if (ri(0, 3) == 0) { wf(m + 0x10, ee_read<float>(m + 0x10) + rf(-8, 8)); }
    }, 50);

    // ---- the zone marker (class 605)
    for (int pass = 0; pass < 4; ++pass)
    classcheck("zone_marker_update", 0x002f2eb8, zone_marker_update_l1, [pass](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t P = 0x0013f350;
        w8(m + 0x20, pass == 0 ? u(ri(0, 1)) : (pass == 1 ? 0u : 1u));
        w8(m + 0xbc, ri(0, 3) == 0 ? u(ri(1, 9)) : 0u);
        w32(0x0015ed84, u(ri(0, 0x16)));
        w32(L1_ADDR(0x00161af0), u(ri(0, 20)));
        for (uint32_t i = 0; i < 64; ++i) w8(0x0014bf10 + i, R());
        w32(pv + 0x18, u(ri(-3, 40)));
        w32(pv + 0x10, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 5))); w32(pv + 0x14, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 4)));
        // the hero in the polygon's / box's neighbourhood
        { const int32_t b = static_cast<int32_t>(ee_read<uint32_t>(pv + 0x14)); Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-10, 10); q.y += rf(-10, 10);
          if (b >= 0 && coin()) { const uint32_t box = ee_read<uint32_t>(OV(0x001600ec)) + u(b << 7); for (uint32_t k = 0; k < 3; ++k) (&q.x)[k] = ee_read<float>(box + 0x30 + 4 * k) + rf(-0.3f, 0.3f); }
          ee_ref<Vec4>(P + 0x80) = q; }
        w32(P + 0x1090, ri(0, 3) == 0 ? ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(1, 50)) : 0u); w32(P + 0x10b8, coin() ? 0x1bu : u(ri(0, 3)));
        if (ee_read<uint32_t>(P + 0x1090) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(P + 0x1090) + 0x78) == 0) w32(P + 0x1090, 0);
        wf(P + 0x2044, coin() ? 1000.0f : rf(0, 30)); w32(P + 0x2040, 0); w32(P + 0x2048, 0);
    }, 60);

    // ---- the draw-hook classes (676, 678, 1225)
    for (int pass = 0; pass < 2; ++pass) {
        classcheck("draw_hook_676", 0x002f6128, draw_hook_676_update_l1, [](uint32_t) { w32(OV(0x0015f424), u(ri(0, 0x41))); }, 20);
        classcheck("draw_hook_678", 0x002f6180, draw_hook_678_update_l1, [](uint32_t) { w32(OV(0x0015f424), u(ri(0, 0x41))); w32(L1_ADDR(0x00161ba0), u(ri(0, 1))); }, 20);
        classcheck("draw_hook_1225", 0x00309c98, draw_hook_1225_update_l1, [](uint32_t) { w32(OV(0x0015f424), u(ri(0, 0x41))); }, 20);
    }

    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
