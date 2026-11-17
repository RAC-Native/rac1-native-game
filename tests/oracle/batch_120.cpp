// Oracle batch 120 (task 0353): level 8's (Batalia) own moby classes on its real level data. Run with
// ORACLE_LEVEL=8 build/tests/ovl_oracle --batch 120 (needs the ISO). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the
// original is level 8's own function at its own address, the port the one registered for it (the
// transliterations batalia_classes_tr_N.cpp, or the shared hand-written ports registered since the level's re-glue).

#include "ovl_oracle.h"

#include <game/ovl/aridia_mobys.h>
#include <game/ovl/batalia_mobys.h>
#include <game/ovl/batalia_own.h>
#include <game/ovl/variants_l8.h>
#include <game/ovl/carry_hooks.h>
#include <game/ovl/carry_hooks_levels.h>
#include <game/ovl/empty_class_updates.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/novalis_camera.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class1023.h>
#include <game/ovl/shared_class1135.h>
#include <game/ovl/shared_class1400.h>
#include <game/ovl/shared_class156.h>
#include <game/ovl/shared_class1629.h>
#include <game/ovl/shared_class1844.h>
#include <game/ovl/shared_class252.h>
#include <game/ovl/shared_class280.h>
#include <game/ovl/shared_class344.h>
#include <game/ovl/shared_class567.h>
#include <game/ovl/shared_class604.h>
#include <game/ovl/shared_class615.h>
#include <game/ovl/shared_class679.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_homing_missile.h>
#include <game/ovl/shared_puffer.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_scroll.h>
#include <game/ovl/shared_spawner.h>
#include <game/ovl/shared_tracer.h>
#include <game/ovl/shared_zones.h>
#include <game/ovl/sound_emitter.h>
#include <runtime/kernel/disc.h>

#include <optional>
#include <cmath>
#include <cstring>
#include <string>
#include <map>

namespace rac1::oracle {

namespace {
// The checks named here reach sphere_collide or capsule_collide, whose chunk buffers, unpacked vertices and cell
// walk the port keeps on the host stack (the original: 0x70000000 .. 0x70002000, 0x70002000, 0x70003800): A2's
// collision scratch ranges are left out of these checks, and only of them (stage 5, task C2b).
bool c2b_spr_check(const char* name) {
    static const char* const kNames[] = {"class248_grenade", "class333", "class333_grenadier", "class333_hit",
                                         "class333_landing", "class344", "class425_shell", "class438", "class452",
                                         "bomber_tank_drive_l8", "class1134", "class1135", "class1400", "class1818",
                                         "class252", "class253", "class253_bomber_tank", "class253_fire", "class253_spot",
                                         "class455", "class805"};
    for (const char* n : kNames)
        if (std::strcmp(n, name) == 0) return true;
    return false;
}

// a hand-written class hook (moby, position, target, float) in the hook table's uniform form: the levels' copies of
// Veldin's hooks and the levels' hooks written by hand run these (T00)
template <int32_t (*H)(uint32_t, uint32_t, uint32_t, float)>
TrRet hook_uniform(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float f12, float, float, float,
                   float, float, float, float) {
    return TrRet{static_cast<uint32_t>(H(a0, a1, a2, f12)), 0.0f};
}
// level 8's hook +8 of class 252's table (L8:0x2d4000): the shared carry_state_hook with Batalia's states (hook +8, carried 5, released 1)
int32_t bat_hook_2d4000(uint32_t m, uint32_t a1, uint32_t a2, float f12) { return carry_state_hook(m, a1, a2, f12, 8, 5, 1); }

// A check whose original is level 8's own function at `l8addr` (not a Veldin address to translate).
void l8check(const char* name, uint32_t l8addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (const char* skip = std::getenv("B120_SKIP"); skip != nullptr && (std::string(",") + skip + ",").find(std::string(",") + name + ",") != std::string::npos) return;  // (the mutation test: checks already seen to fail)
    if (g_oracle_level != 8) {
        std::printf("%-26s L8:0x%08x  skipped (level 8 only: run with ORACLE_LEVEL=8)\n", name, l8addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L8:0x%08x\n", name, l8addr);
    std::optional<CollisionScratchIgnored> spr_ignored;
    if (c2b_spr_check(name)) spr_ignored.emplace();
    if (!check_fn(name, l8addr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_120() {
    if (g_only_batch != 120) return;
    g_batch = 120;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    // segment_collide's hit point (the global at Veldin 0x174610, 3 words): written by the callee from the segment's end points; class 911's
    // state 1 builds the segment from stale stack lanes (the port's EE stack and the interpreter's hold other leftovers), nothing reads it
    for (uint32_t k = 0; k < 12; k += 4) g_ignored_words.push_back(0x00174610u + k);
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // not compared: the null page (a null pointer the random states give the original's code: it runs on stale registers) and the
    // scratchpad words the pose function (0x245f18) spills the caller's $t8 / $t9 into (the transliterated callers have no such registers)
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);

    // 989snd's IOP server (as batch 103's): replies a function of the command
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
    // the level's mobys whose update function is `fn` (level 8's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    static int redealt_count = 0;
    auto redealt = []() -> int& { return redealt_count; };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with, redealt](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60, std::vector<uint32_t> subjects = {}) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 8) { l8check(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
        level();
        const std::vector<uint32_t> ms = subjects.empty() ? mobys_with(fn) : subjects;
        if (ms.empty()) { std::printf("%-26s L8:0x%08x  no moby on this level\n", name, fn); return; }
        l8check(name, fn, n, Ret::None, [level, ms, vary, port, redealt] {
            // a random state that sends the port's code through a pointer outside RAM (a garbage pointer the state machine had not set yet: the
            // original's interpreter wraps it, the port refuses) is not a game state: it is dealt again (the RAM put back after the trial)
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
                try { port(m); } catch (const std::exception&) { ok = false; ++redealt(); }
                ee_stack_ptr() = stack_top;  // (an exception leaves the port's frames allocated)
                std::memcpy(ram, saved.data(), saved.size());
                if (ok) break;
            }
            return Args{{m}, {}};
        }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
        // (P8.1) the cases dealt again because the port threw on them: a fault that makes the port throw hides there
        if (redealt() != 0) std::printf("%-26s %d cases dealt again (the port threw)\n", name, redealt());
        redealt() = 0;
    };
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

    // the debris / spawns leave w lanes of a VU0 sin / cos in the original (the port has none)
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    if (g_oracle_level == 8) {
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
        {  // the particle pool (0x800 records of 0x40 bytes at *(0x16016c)): the w lane of the velocity vector a spray copies (a stale lane of vec_from_spherical in the original)
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c);
        }
    }

    // ---- the level's own classes (transliterated by tools/codegen/mkall_l8.py): the level's own mobys of each class, the first
    // update (state 0) done by the port, then a state, a hit, the hero near, the pvars' floats / counters moved as typed
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x002763f8};  // (+ the level change request: the original would run the CD / movie path)
    struct TrCls { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t skip_states = 0; bool walk = false; };  // (the states that start a scene / a level change: the original runs the kernel / movie path for real)
    static const TrCls trcls[] = {
        {"class152", 0x002b82a0, bat_class152_update_l8, 0x0, false},
        {"class156", 0x002ba8d0, class156_update, 0x0, false},
        {"class248", 0x002d23f0, bat_class248_update_l8, 0x0, false},
        {"class252", 0x002d2af0, class252_update, 0x0, false},
        {"class253", 0x002d42d8, bat_class253_update_l8, 0x0, false},
        {"class280", 0x002d8270, class280_update, 0x0, false},
        {"class324", 0x002da0c8, empty_update, 0x0, false},
        {"class327", 0x002da0f0, bat_class327_update_l8, 0x0, false},
        {"class331", 0x002da3c0, trackdrive_left_update_l2, 0x0, false},
        {"class333", 0x002dabf0, bat_class333_update_l8, 0x0, false},
        {"class344", 0x002dba40, class344_update, 0x0, false},
        {"class424", 0x002dbdb0, bat_class424_update_l8, 0x8, true},
        {"class425", 0x002dc9a8, bat_class425_update_l8, 0x0, false},
        {"class435", 0x002dd3c0, bat_class435_update_l8, 0x0, false},
        {"class438", 0x002de848, bat_class438_update_l8, 0x0, false},
        {"class440", 0x002e0328, bat_class440_update_l8, 0x0, false},
        {"class441", 0x002e16c0, bat_class441_update_l8, 0x0, false},
        {"class444", 0x002e24e8, bat_class444_update_l8, 0x0, false},
        {"class452", 0x002e2df0, bat_class452_update_l8, 0x0, false},
        {"class455", 0x002e6530, class455_update, 0x0, false},
        {"class458", 0x002e8210, homing_missile_update, 0x0, false},
        {"class462", 0x002e8cd0, bat_class462_update_l8, 0x0, false},
        {"class463", 0x002e9b70, bat_class463_update_l8, 0x0, false},
        {"class467", 0x002ea398, bat_class467_update_l8, 0x0, false},
        {"class468", 0x002ea4c0, bat_class468_update_l8, 0x0, false},
        {"class545", 0x002ee6f0, class545_update, 0x0, false},
        {"class566", 0x002efb50, empty_update, 0x0, false},
        {"class567", 0x002efb60, class567_update, 0x0, false},
        {"class600", 0x002f1548, bat_class600_update_l8, 0x0, false},
        {"class604", 0x002f18b8, class604_update, 0x0, false},
        {"class605", 0x002f1c08, zone_marker_update, 0x0, false},
        {"class615", 0x002f2030, class615_update, 0x0, false},
        {"class618", 0x002f44b8, empty_update, 0x0, false},
        {"class621", 0x002f44c0, bat_class621_update_l8, 0x0, false},
        {"class648", 0x002f5830, puffer_update, 0x0, false},
        {"class671", 0x002f70a0, bat_class671_update_l8, 0x0, false},
        {"class679", 0x002f74c0, class679_update, 0x0, false},
        {"class774", 0x002f90c8, bat_class774_update_l8, 0x0, false},
        {"class805", 0x002f99b0, spawn_point_update_l1, 0x0, false},
        {"class832", 0x002fbb58, class832_update, 0x0, false},
        {"class1023", 0x00301158, class1023_update, 0x0, false},
        {"class1039", 0x00302a78, deadly_zones_update, 0x0, false},
        {"class1130", 0x00302ce8, bat_class1130_update_l8, 0x0, false},
        {"class1134", 0x00303370, gold_bolt_update, 0x4, false},
        {"class1135", 0x003042a8, teleporter_update_l1, 0x0, false},
        {"class1144", 0x00305270, bat_class1144_update_l8, 0x0, false},
        {"class1283", 0x003065d8, bat_class1283_update_l8, 0x20, false},
        {"class1292", 0x00307298, tracer_update, 0x0, false},
        {"class1349", 0x00307540, bat_class1349_update_l8, 0x0, false},
        {"class1400", 0x00307cf0, class1400_update, 0x0, false},
        {"class1553", 0x003084d8, bat_class1553_update_l8, 0x0, false},
        {"class1629", 0x003085f0, class1629_update, 0x0, false},
        {"class1633", 0x00308920, class1633_update, 0x0, false},
        {"class1641", 0x00308c90, bat_class1641_update_l8, 0x0, false},
        {"class1649", 0x003097b0, bat_class1649_update_l8, 0x0, false},
        {"class1818", 0x0030ac28, sandmouse_update, 0x0, false},
        {"class1844", 0x0030bc00, class1844_update, 0x0, false},
        {"class1845", 0x0030bcf8, empty_update, 0x0, false},
    };
    auto generic_prep = [give_hit, hero_near](uint32_t m, int st, int pass, void (*fn)(uint32_t), bool walk = false) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t P = 0x0013f350;
        if (st != 0) {
            for (int k = 0; k < (walk ? st : 1); ++k) {  // (walk: the state machine goes through the states before: a path / a record they make)
                w8(m + 0x20, u(k));
                const uint32_t sp0 = ee_stack_ptr();
                try { fn(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; break; }
            }
        }
        w8(m + 0x20, u(st));
        if (pass >= 1) {
            if (coin()) { w8(m + 0x53, 0); w8(m + 0x52, 0); } w8(m + 0xbc, u(ri(0, 2)));  // (the animation indices stay inside the class's animation table)
            wf(m + 0x48, rf(-3.1f, 3.1f));
            { Vec4 q = ee_ref<Vec4>(m + 0x10); const float r = ri(0, 3) == 0 ? rf(0, 6) : (coin() ? rf(6, 30) : rf(30, 80)); const float a = rf(-3.1f, 3.1f);
              q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-4, 12); ee_ref<Vec4>(P + 0x80) = q; }
            for (uint32_t o = 0x20; o < 0x200; o += 4) {  // a few of the pvars kept as they are typed: floats scaled, small counters moved
                if (ri(0, 9) != 0) continue;
                const uint32_t v = ee_read<uint32_t>(pv + o), ex = v & 0x7f800000u;
                if (ex >= 0x30000000u && ex < 0x50000000u) wf(pv + o, ee_read<float>(pv + o) * rf(0.5f, 1.5f));
                else if (v >= 1 && v < 16) w32(pv + o, u(ri(1, static_cast<int>(v) + 2)));  // (a 0 stays 0: a null pointer / an unset slot)
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
    if (g_oracle_level == 8 && std::getenv("B120_INFO") != nullptr) {  // the level's mobys of each class: how many, the nearest to the hero's start
        level();
        const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
        std::printf("hero start %.1f %.1f %.1f\n", h.x, h.y, h.z);
        for (const TrCls& c : trcls) {
            const std::vector<uint32_t> ms = mobys_with(c.addr);
            float best = 1e9f;
            for (uint32_t m : ms) { const Vec4 q = ee_ref<Vec4>(m + 0x10); best = std::fmin(best, std::sqrt((q.x - h.x) * (q.x - h.x) + (q.y - h.y) * (q.y - h.y) + (q.z - h.z) * (q.z - h.z))); }
            std::printf("INFO %s 0x%08x instances %zu nearest %.0f\n", c.name, c.addr, ms.size(), ms.empty() ? -1.0f : best);
        }
    }
    for (const TrCls& c : trcls) {
        if (std::getenv("B120_CLS") != nullptr && std::strcmp(std::getenv("B120_CLS"), c.name) != 0) continue;
        for (int st = 0; st < 16; ++st)
        for (int pass = 0; pass < 3; ++pass) {
        if (g_oracle_level == 8 && std::getenv("B120_TAGS") != nullptr) std::printf("@@ %s st %d pass %d\n", c.name, st, pass);
        // (class 253 state 14, the tank's destruction: its helper L8:0x2d5d08 spawns 40 debris (classes 0x310 / 0x311) and copies the
        // vector at its sp+0x40 into each one's pvars +0x30 (L8:0x2db990): the vector's w lane (sp+0x4c) is never written, a stale stack
        // word on both sides (the interpreter's stack and the port's are not the same memory): the pvars' +0x3c words are not compared)
        const size_t n_ign = g_ignored_words.size();
        if (c.addr == 0x002d42d8 && st == 14)
            for (uint32_t k = 0; k < 0x400; ++k) g_ignored_words.push_back(ee_read<uint32_t>(OV(0x0015ffe8)) + (k << 7) + 0x3c);
        if ((std::getenv("B120_ST") == nullptr || std::atoi(std::getenv("B120_ST")) == st) && (std::getenv("B120_PASS") == nullptr || std::atoi(std::getenv("B120_PASS")) == pass) && (c.skip_states & (1u << st)) == 0) classcheck(c.name, c.addr, c.fn, [st, pass, c, generic_prep](uint32_t m) { generic_prep(m, st, pass, c.fn, c.walk); }, 12);
        g_ignored_words.resize(n_ign);
        }
    }
    g_extra_noop_hooks.clear();

    // ---- level 8's sound emitter types (3, 6: section 5 of the overlay) on the level's own emitters
    if (g_oracle_level == 8) {
        struct Em { const char* name; uint32_t addr; void (*fn)(uint32_t); };
        static const Em ems_t[] = {{"emitter1", 0x00316aa8, emitter_fade_box_update}, {"emitter3", 0x00316e48, emitter_reverb_box_update}, {"emitter4", 0x00316fa8, emitter_open_bed_update}, {"emitter6", 0x00317230, emitter_music_box_update}};
        for (const Em& e : ems_t) {
            level();
            std::vector<uint32_t> ems;
            for (int32_t i = 0; i < ee_read<int32_t>(0x0013f2e0); ++i) {
                const uint32_t em = ee_read<uint32_t>(0x0013f2e4) + static_cast<uint32_t>(i) * 0x90;
                if (ee_read<uint32_t>(em + 4) == e.addr) ems.push_back(em);
            }
            if (ems.empty()) { std::printf("%-26s L8:0x%08x  no emitter of the type on this level\n", e.name, e.addr); continue; }
            if (std::getenv("B120_INFO") != nullptr) std::printf("INFO %s 0x%08x emitters %zu\n", e.name, e.addr, ems.size());
            auto fn = e.fn;
            l8check(e.name, e.addr, 120, Ret::None, [level, ems] {
                level();
                const uint32_t em = ems[static_cast<size_t>(ri(0, static_cast<int>(ems.size()) - 1))];
                const uint32_t prm = ee_read<uint32_t>(em + 8);
                Vec4 q = ee_ref<Vec4>(em + 0x40);  // the hero (the camera's point) inside the emitter's volume or around it
                const float r = ri(0, 2) == 0 ? rf(0, 3) : rf(3, 40);
                const float a = rf(-3.1f, 3.1f);
                q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-3, 3);
                ee_ref<Vec4>(0x0013f3d0) = q;
                if (ri(0, 4) == 0) w32(prm + 0xc, 0xffffffffu);
                else if (ri(0, 2) == 0) w32(prm + 0xc, R());
                w32(prm + 8, u(ri(0, 1)));
                if (ri(0, 3) == 0) w32(prm + 0x10, u(ri(0, 20)));
                return Args{{em}, {}};
            }, [fn](A a) { fn(a.i[0]); return uint64_t{0}; });
        }
    }


    // ---- level 8's camera type 0x12 (accept / init / update / exit: section 4 of the overlay)
    if (g_oracle_level == 8) {
        // a live camera of the type, the hero put in or near its box, its data varied
        auto cam_setup = [level](int type, bool with_other, uint32_t* other) -> uint32_t {
            level();
            uint32_t cam = 0;
            for (uint32_t i = 0; i < 48 && cam == 0; ++i)
                if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0 && ee_read<int16_t>(OV(0x00167150) + i * 0xa0 + 0x8c) == type) cam = OV(0x00167150) + i * 0xa0;
            for (uint32_t i = 0; i < 48 && cam == 0; ++i)  // (no live camera of the type on the level: any live camera, the functions are checked on its words)
                if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0) cam = OV(0x00167150) + i * 0xa0;
            if (cam == 0) return 0;
            const uint32_t e = ee_read<uint32_t>(OV(0x0015ef50)) + u(ee_read<int16_t>(cam + 0x84) * 0x20);
            const uint32_t d = ee_read<uint32_t>(e + 0x1c);
            Vec4 at = ee_ref<Vec4>(0x0013f3d0);
            if (ri(0, 3) != 0 && d != 0) {  // inside the first box, or near it
                const int32_t idx = ee_read<int32_t>(d + 0xc);
                if (idx >= 0) at = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(idx) * 0x80 + 0x30);
            }
            at.x += rf(-2, 2); at.y += rf(-2, 2); at.z += rf(-1, 1);
            ee_ref<Vec4>(0x0013f3d0) = at;
            if (ri(0, 2) == 0) ee_ref<Vec4>(OV(0x00166dc0)) = at;
            w8(cam + 0x7c, u(ri(0, 3)));
            w32(cam + 0x74, ri(0, 3) == 0 ? u(ri(0, 6)) : 4u);
            if (with_other && other != nullptr) {
                *other = ri(0, 2) == 0 ? 0u : (coin() ? cam : OV(0x00167150u));
                if (*other != 0) { w16(*other + 0x7e, u(ri(0, 2) == 0 ? 0 : 1)); w8(*other + 0x7c, u(ri(0, 3))); }
            }
            return cam;
        };
        struct CamT { int type; uint32_t accept, init, update, exit; };
        static const CamT cams[] = {{3, 0x003127d0, 0x003127d8, 0x00311d50, 0x00312a28}, {18, 0x00316298, 0x00316300, 0x00316308, 0x00316310}};
        for (const CamT& ct : cams) {
            level();
            bool any = false, own = false;
            for (uint32_t i = 0; i < 48; ++i)
                if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0) { any = true; if (ee_read<int16_t>(OV(0x00167150) + i * 0xa0 + 0x8c) == ct.type) own = true; }
            if (!any) { std::printf("cam_type%d                 no live camera on this level\n", ct.type); continue; }
            if (!own) std::printf("cam_type%d                 no live camera of the type on this level: checked on another live camera's words\n", ct.type);
            const int type = ct.type;
            static char nm[4][48];
            static int k = 0;
            auto name = [&](const char* what) { char* p = nm[k++ & 3]; std::snprintf(p, 48, "cam_type%d_%s", type, what); return static_cast<const char*>(p); };
            l8check(name("accept"), ct.accept, 300, Ret::Int32, [cam_setup, type] { uint32_t other = 0; const uint32_t cam = cam_setup(type, true, &other); return Args{{cam, other}, {}}; },
                    [type](A a) { return r32(a.i[0] != 0 ? static_cast<uint32_t>(type == 3 ? zero_stub() : cam_type18_accept(a.i[0], a.i[1])) : 0u); });
            l8check(name("init"), ct.init, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) (type == 3 ? cam_type3_init : empty_update)(a.i[0]); return uint64_t{0}; });
            l8check(name("update"), ct.update, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) (type == 3 ? cam_type3_update : empty_update)(a.i[0]); return uint64_t{0}; });
            l8check(name("exit"), ct.exit, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) (type == 3 ? cam_type3_exit : empty_update)(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- level 8's class hooks (the tables at 0x1fd200 / 0x1fd218): a0 = the moby, a1 = a position, a2 = a target moby, f12 a float
    // (the carry hooks' shape: their callees read all four), on the mobys whose class owns the hook
    if (g_oracle_level == 8) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x002763f8};
        struct Hk { const char* name; uint32_t addr; uint32_t slot; TrRet (*fn)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float, float, float, float, float); };
        static const Hk hks[] = {
            {"hook_2d3f60", 0x2d3f60, 0x0, hook_uniform<enemy10e_hook0>},
            {"hook_2d3fb0", 0x2d3fb0, 0x4, hook_uniform<enemy10e_hook4>},
            {"hook_2d4000", 0x2d4000, 0x8, hook_uniform<bat_hook_2d4000>},
            {"hook_2d4050", 0x2d4050, 0xc, hook_uniform<class252_hook_c>},
            {"hook_2d40d8", 0x2d40d8, 0x10, bat_hook_2d40d8_u},
            {"hook_2d8180", 0x2d8180, 0xc, hook_uniform<bat_carry_hookc_l8>},
        };
        for (const Hk& hk : hks) {
            level();
            std::vector<uint32_t> own;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
                const uint32_t cls = ee_read<uint32_t>(m + 0x24);
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + hk.slot) == hk.addr) own.push_back(m);
            }
            if (own.empty()) { std::printf("%-26s L8:0x%08x  no moby owns it on this level\n", hk.name, hk.addr); continue; }
            if (std::getenv("B120_INFO") != nullptr) std::printf("INFO %s 0x%08x owners %zu\n", hk.name, hk.addr, own.size());
            auto fn = hk.fn;
            l8check(hk.name, hk.addr, 40, Ret::Int32, [level, own] {
                level();
                const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
                const uint32_t m = own[static_cast<size_t>(ri(0, static_cast<int>(own.size()) - 1))];
                if (ri(0, 4) == 0) w8(m + 0x20, coin() ? 0xfe : 0xfd);
                const uint32_t tgt = ri(0, 3) == 0 ? 0u : first + 0x100 * u(ri(0, 100));
                return Args{{m, ri(0, 2) == 0 ? 0x0013f3d0u : m + 0x10, tgt}, {rf(-1, 3)}};
            }, [fn](A a) { return r32(fn(a.i[0], a.i[1], a.i[2], 0, 0, 0, 0, 0, a.f[0], 0, 0, 0, 0, 0, 0, 0).v0); });
        }
        g_extra_noop_hooks.clear();
    }

    // ---- moby_spawn (the Veldin port) against level 8's own copy (L8:0x00258d38): the classes' spawns all go through it
    if (g_oracle_level == 8) {
        l8check("moby_spawn", 0x00258d38, 60, Ret::Int32, [level] {
            level();
            static const uint32_t cls[] = {0x221, 0x14b, 0xb3, 0x2d, 0x1b1, 0x3f, 0x1f4};
            if (ri(0, 2) == 0) w32(OV(0x0015ffe0) , ee_read<uint32_t>(OV(0x0015ffe0)));
            return Args{{cls[ri(0, 6)]}, {}};
        }, [](A a) { return r32(moby_spawn(a.i[0])); });
    }

    // T00: the level's bare `jr ra` functions its classes and cameras reach through a pointer (class updates, camera
    // callbacks; the scan pairs them with Veldin's 0x1f6bf8): empty_update runs in their places, each against the original
    // called directly (the level may have no moby of the class)
    if (g_oracle_level == 8) {
        static const uint32_t empties[] = {0x002da0c8, 0x002efb50, 0x002f44b8, 0x0030bcf8, 0x00316300, 0x00316308, 0x00316310};
        for (const uint32_t a : empties) {
            char nm[24];
            std::snprintf(nm, sizeof nm, "empty_%x", a);
            l8check(nm, a, 20, Ret::None, [level] {
                level();
                return Args{{R(), R(), R(), R()}, {rf(-9, 9)}};
            }, [](A a) { empty_update(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- stage 4 P8.1: the hand ports of classes 152, 248, 253, 327, 333, 424 and 425 (batalia_bolt_prize.cpp,
    // batalia_projectiles.cpp, batalia_bomber_tank.cpp, batalia_sea.cpp, batalia_grenadier.cpp, batalia_shuttle.cpp) and their
    // helpers, each against level 8's own code: their states driven through the branches the class loop above seldom
    // reaches, the helpers called directly
    if (g_oracle_level == 8 && std::getenv("B120_CLS") == nullptr) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x002763f8};  // (as the class loop's: the level change)
        const uint32_t P = 0x0013f350;
        // the frame scale, the frame time and its square at their 50 Hz values in a quarter of the cases (the snapshot's are
        // 1.0, 1/60 and its square)
        auto frame50 = [] { if (ri(0, 3) == 0) { wf(0x0015ed68, 1.2f); wf(0x0015ed6c, 0.02f); wf(0x0015ed70, 0.0004f); } };
        // the batch-wide ignores narrowed for these checks: the base list (the VU0 lanes of the camera rows, the null page,
        // the particle update's saved return address ...) without the moby table's / particle pool's w lanes; each check adds
        // back only what it needs: `seg` the segment record's hit point (OV(0x174610) .. +0xc, the known stale-VU0-lane
        // difference of the collision records), `mobys` the w lanes debris and spawns leave in the moby table (a VU0 sin / cos
        // the port has not), `pool` the particle pool's velocity w lane
        std::vector<uint32_t> base_ignored = saved_ignored;
        auto ignores = [base_ignored](bool seg, bool mobys, bool pool) {
            g_ignored_words = base_ignored;
            if (seg) for (uint32_t k = 0; k < 16; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
            if (mobys)
                for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) {
                    g_ignored_words.push_back(a + 0xc);
                    g_ignored_words.push_back(a + 0x1c);
                }
            if (pool) {
                const uint32_t pool_at = ee_read<uint32_t>(OV(0x0016016c));
                for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool_at + k * 0x40 + 0x2c);
            }
        };
        // a sound slot word: none, a free slot, or a slot playing the moby's sound (its owner +0x88, playing +0x74)
        auto sound_slot = [](uint32_t m) -> uint32_t {
            const int r = ri(0, 3);
            if (r == 0) return 0xffffffffu;
            const uint32_t s = u(ri(0, 15)), e = 0x0013e550u + s * 0x70;
            if (r >= 2) { w32(e + 0x88, r == 2 ? m : m + 0x100); w8(e + 0x74, coin() ? 1u : 0u); }
            return s;
        };
        // the camera somewhere about the moby (within `r`)
        auto camera_near = [](uint32_t m, float r) {
            Vec4 c = ee_ref<Vec4>(m + 0x10);
            c.x += rf(-r, r); c.y += rf(-r, r); c.z += rf(-2, r * 0.5f);
            ee_ref<Vec4>(OV(0x00166dc0)) = c;
        };
        // the hero at the moby's place, `d` away in a random direction
        auto hero_at = [](uint32_t m, float d, float dz) {
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            const float a = rf(-3.1f, 3.1f);
            q.x += d * std::cos(a); q.y += d * std::sin(a); q.z += dz;
            ee_ref<Vec4>(0x0013f3d0) = q;
        };
        // the uid-5 camera (the scripted camera's pvars +0x80 / +0x90 scenes move): an entry of the camera table, in the test area
        auto uid5_camera = [] {
            const uint32_t cam = OV(0x00167150) + u(ri(0, 3)) * 0xa0;
            w16(cam + 0x86, 5);
            w32(cam + 0x70, T);
            wfrand(T + 0x80, 8, -50, 50);
        };
        // the mobys of the group `group` set to `state` now and then (a list of the level's)
        auto group_states = [](int32_t group, std::initializer_list<uint32_t> states) {
            if (group < 0 || group >= 0x70) return;
            const uint32_t list = ee_read<uint32_t>(OV(0x001ab840) + u(group) * 4);
            for (uint32_t e = list; list != 0 && e < list + 0x80; e += 2) {
                const uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                if (coin()) w8(o + 0x20, *(states.begin() + ri(0, static_cast<int>(states.size()) - 1)));
                if (ee_read<int16_t>(e) < 0) break;
            }
        };
        // the voice line free (sub_00203908 takes a request), now and then
        auto voice_free = [] { if (ri(0, 3) != 0) { w32(OV(0x00179410), 0); w32(OV(0x00179434), 0xffffffffu); w32(0x00151720, 0); w32(0x001516ec, 0xffffffffu); } };
        (void)frame50; (void)sound_slot; (void)camera_near; (void)hero_at; (void)uid5_camera; (void)group_states; (void)voice_free; (void)P;
        level();
        ignores(false, false, false);
        // class 424, the shuttle: at rest by its paths' ends (Ratchet near one of them, the end it is at +0x100 the same or not,
        // the checkpoints +0x94 / +0x98 taken or not, Ratchet beyond 32 / the level's first frames), Ratchet on it (the voice
        // line busy or free, on its own path +0x8c so the push-out leaves him, the button with the help line his), its ride
        // (the fraction +0xe4 at, near or far from its goal +0x104 of 0 / 1, on each of its three paths), parked (+0x80 -1)
        {
            const std::vector<uint32_t> shuttles = mobys_with(0x002dbdb0);
            std::printf("INFO shuttles %zu\n", shuttles.size());
            auto path_of = [](int32_t i) -> uint32_t { return i >= 0 && i < 0xb0 ? ee_read<uint32_t>(OV(0x001b04b0) + u(i) * 4) : 0u; };
            auto shuttle_prep = [generic_prep, frame50, voice_free, uid5_camera, path_of](uint32_t m, int st, int pass) {
                generic_prep(m, st == 0 ? 0 : 2, pass, bat_class424_update_l8, true);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w8(m + 0x20, u(st));
                if (st == 0 && ri(0, 3) == 0) w32(pv + 0x80, 0xffffffffu);
                const uint32_t ends[3] = {path_of(ee_read<int32_t>(pv + 0x80)), path_of(ee_read<int32_t>(pv + 0x84)), path_of(ee_read<int32_t>(pv + 0x88))};
                const uint32_t row = 0x0014c050 + (ee_read<uint32_t>(0x0015ed84) << 4);
                for (const uint32_t o : {0x94u, 0x98u}) {
                    const uint32_t i = ee_read<uint32_t>(pv + o);
                    if (i < 16) w8(row + i, coin() ? 0xffu : 0u);
                }
                w32(pv + 0x100, u(ri(-1, 3)));
                const int r = ri(0, 4);
                if (r < 3 && ends[r] != 0) {  // Ratchet near one of its ends (the start of +0x80 / +0x84, the end of +0x88)
                    const uint32_t e = ends[r];
                    Vec4 q = ee_ref<Vec4>(e + 0x10 + (r == 2 ? u(ee_read<int32_t>(e) - 1) << 4 : 0u));
                    q.x += rf(-3, 3); q.y += rf(-3, 3);
                    ee_ref<Vec4>(0x0013f3d0) = q;
                } else if (r == 3) {
                    Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += coin() ? rf(33, 60) : rf(0, 30); ee_ref<Vec4>(0x0013f3d0) = q;
                }
                w32(L8_ADDR(0x0015f5cc), coin() ? u(ri(0, 4)) : u(ri(5, 5000)));
                if (st == 2 && pass != 0) {  // Ratchet on it, where its own path keeps him
                    w32(P + 0x2fc, m);
                    ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(m + 0x10);
                    voice_free();
                    w32(0x0013cae4, coin() ? 0x10u : 0u);
                    w16(0x0013f65e, ri(0, 3) == 0 ? 1u : 0u);
                    w32(L8_ADDR(0x0015f594), ri(0, 3) != 0 ? 10u : 0u);
                }
                if (st == 3 || coin()) {  // its ride: on one of its paths, the fraction about its goal
                    const int k = ri(0, 2);
                    if (ends[k] != 0) w32(pv + 0x120, ends[k]);
                    const float goal = coin() ? 1.0f : 0.0f;
                    wf(pv + 0x104, goal);
                    const int f = ri(0, 3);
                    wf(pv + 0xe4, f == 0 ? goal : f == 1 ? goal + (goal == 0.0f ? rf(0, 0.2f) : -rf(0, 0.2f)) : rf(0, 1));
                    wf(pv + 0xe8, f == 0 ? 0.0f : rf(-0.02f, 0.02f));  // (at its goal and still: the ride ends this frame)
                    if (coin()) wf(pv + 0x108, rf(0, 3.1f));
                }
                if (ri(0, 4) == 0) w32(L8_ADDR(0x0015f5c4), 2);
                if (ri(0, 4) == 0) w8(pv + 0xa1, 0);
                uid5_camera();
                frame50();
            };
            if (shuttles.empty()) std::printf("%-26s no shuttle (class 424) on this level\n", "class424_shuttle");
            else {
                // (the states 2 / 3 on the shuttles that have their three paths: a parked shuttle, +0x80 -1, goes to 4 at its set-up and
                // never waits or rides)
                std::vector<uint32_t> riders;
                for (const uint32_t m : shuttles) {
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    bool ok = true;
                    for (const uint32_t o : {0x80u, 0x84u, 0x88u}) ok = ok && path_of(ee_read<int32_t>(pv + o)) != 0;
                    if (ok) riders.push_back(m);
                }
                for (const int st : {0, 1, 2, 3, 4})
                    for (int pass = 0; pass < 3; ++pass)
                        classcheck("class424_shuttle", 0x002dbdb0, bat_class424_update_l8, [st, pass, shuttle_prep](uint32_t m) { shuttle_prep(m, st, pass); }, 12,
                                   (st == 2 || st == 3) && !riders.empty() ? riders : std::vector<uint32_t>{});
                // the helpers directly, on the same states
                struct RaftFn { const char* name; uint32_t addr; Ret ret; uint64_t (*fn)(uint32_t); };
                static const RaftFn rfns[] = {
                    {"shuttle_corners_l8", 0x002dc0c8, Ret::None, [](uint32_t m) { shuttle_corners_l8(m); return uint64_t{0}; }},
                    {"shuttle_to_nearest_l8", 0x002dc180, Ret::None, [](uint32_t m) { shuttle_to_nearest_l8(m); return uint64_t{0}; }},
                    {"shuttle_ride_l8", 0x002dc648, Ret::Int32, [](uint32_t m) { return r32(shuttle_ride_l8(m) ? 1u : 0u); }},
                    {"shuttle_help_l8", 0x002dc7f0, Ret::Int32, [](uint32_t m) { return r32(u(shuttle_help_l8(m))); }}};
                // (the shuttles the helpers run on in the game: with their three paths, and for the corners their own path +0x8c; the
                // ride on the path the nearest end gave it)
                std::vector<uint32_t> routed, cornered;
                for (const uint32_t m : shuttles) {
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    bool ok = true;
                    for (const uint32_t o : {0x80u, 0x84u, 0x88u}) ok = ok && path_of(ee_read<int32_t>(pv + o)) != 0;
                    if (ok) routed.push_back(m);
                    if (path_of(ee_read<int32_t>(pv + 0x8c)) != 0) cornered.push_back(m);
                }
                std::printf("INFO shuttles with their paths %zu, with their own %zu\n", routed.size(), cornered.size());
                for (const RaftFn& f : rfns) {
                    auto fn = f.fn;
                    const std::vector<uint32_t>& from = f.addr == 0x002dc0c8 ? cornered : routed;
                    if (from.empty()) { std::printf("%-26s no shuttle for it on this level\n", f.name); continue; }
                    l8check(f.name, f.addr, 60, f.ret, [level, from, shuttle_prep, path_of] {
                        level();
                        const uint32_t m = from[static_cast<size_t>(ri(0, static_cast<int>(from.size()) - 1))];
                        shuttle_prep(m, coin() ? 2 : 3, ri(0, 2));
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        if (ee_read<uint32_t>(pv + 0x120) == 0) w32(pv + 0x120, path_of(ee_read<int32_t>(pv + 0x80)));
                        if (coin()) w32(OV(0x0015f594), 0);
                        return Args{{m}, {}};
                    }, [fn](A a) { return fn(a.i[0]); });
                }
            }
        }
        //@@P81_ME
        // ---- (P8.1 h1) class 253, the Bomber Tank (batalia_bomber_tank.cpp): its update on the level's tanks in each state,
        // and its six helpers called directly
        {
            level();
            const std::vector<uint32_t> bt_tanks = mobys_with(0x002d42d8);
            const uint32_t bt_tab = ee_read<uint32_t>(OV(0x0015ffd8)), bt_end = ee_read<uint32_t>(OV(0x0015ffe0));
            // a path with points (the level's path table; its count 0x160104)
            auto bt_path_ok = [](int32_t i) {
                if (i < 0 || i >= ee_read<int32_t>(OV(0x00160104)) || i >= 176) return false;
                const uint32_t p = ee_read<uint32_t>(OV(0x001b04b0) + u(i) * 4);
                return p != 0 && ee_read<int32_t>(p) > 0 && ee_read<int32_t>(p) < 0x400;
            };
            auto bt_path = [](int32_t i) { return ee_read<uint32_t>(OV(0x001b04b0) + u(i) * 4); };
            std::vector<int32_t> bt_routes;
            for (int32_t i = 0; i < 176; ++i) if (bt_path_ok(i)) bt_routes.push_back(i);
            // a wall's piece: classes 588 .. 598 (thrown) or 472 (blown up)
            auto bt_is_piece = [](uint32_t o) { const uint32_t c = ee_read<uint16_t>(o + 0xa6); return c - 0x24cu < 11u || c == 0x1d8u; };
            auto bt_list = [](uint32_t o) -> uint32_t {  // the group list of the moby `o` (its +0x21), 0 for none
                const uint32_t g = ee_read<uint8_t>(o + 0x21);
                return g < 0x70 ? ee_read<uint32_t>(OV(0x001ab840) + g * 4) : 0u;
            };
            // the walls: live mobys (index > 0) whose group lists a wall piece; the tanks' own walls first
            std::vector<uint32_t> bt_walls;
            for (const uint32_t m : bt_tanks) {
                const int32_t w = ee_read<int32_t>(ee_read<uint32_t>(m + 0x78) + 0x144);
                if (w > 0 && bt_tab + (u(w) << 8) < bt_end && bt_list(bt_tab + (u(w) << 8)) != 0) bt_walls.push_back(u(w));
            }
            const size_t bt_own_walls = bt_walls.size();
            for (uint32_t o = bt_tab + 0x100; o < bt_end && bt_walls.size() < 64; o += 0x100) {
                if (ee_read<uint8_t>(o + 0x20) >= 0xfe) continue;
                const uint32_t list = bt_list(o);
                bool has = false;
                for (uint32_t e = list; list != 0 && e < list + 0x200; e += 2) {
                    const uint32_t q = bt_tab + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                    if (q < bt_end && bt_is_piece(q)) has = true;
                    if (ee_read<int16_t>(e) < -1) break;
                }
                if (has) bt_walls.push_back((o - bt_tab) >> 8);
            }
            // the mobys with pvars (one of them lets a waiting tank in: its pvar +0 at 1.0)
            std::vector<uint32_t> bt_keys;
            for (uint32_t o = bt_tab + 0x100; o < bt_end; o += 0x100)
                if (ee_read<uint8_t>(o + 0x20) < 0xfe && ee_read<uint32_t>(o + 0x78) != 0) bt_keys.push_back((o - bt_tab) >> 8);
            std::printf("INFO class253 tanks %zu walls %zu (the tanks' own %zu) routes %zu keys %zu\n", bt_tanks.size(), bt_walls.size(), bt_own_walls,
                        bt_routes.size(), bt_keys.size());
            // the wall pieces' pvars +0x3c and the spawned mobys' (the pvar blocks of 0x15ffe8): the throw copies the velocity's w lane
            // (the break's sp+0x2c when its rubble loop did not run, sp+0x4c for the rubble), a stale stack word on both sides (the
            // interpreter's stack and the port's are not the same memory), as the class loop's state 14 note says
            auto bt_ignore_throws = [bt_walls, bt_tab, bt_end, bt_list, bt_is_piece] {
                for (uint32_t k = 0; k < 0x400; ++k) g_ignored_words.push_back(ee_read<uint32_t>(OV(0x0015ffe8)) + (k << 7) + 0x3c);
                for (const uint32_t w : bt_walls) {
                    const uint32_t list = bt_list(bt_tab + (w << 8));
                    for (uint32_t e = list; list != 0 && e < list + 0x200; e += 2) {
                        const uint32_t q = bt_tab + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                        if (q < bt_end && ee_read<uint32_t>(q + 0x78) != 0) g_ignored_words.push_back(ee_read<uint32_t>(q + 0x78) + 0x3c);
                        if (ee_read<int16_t>(e) < -1) break;
                    }
                }
            };
            // the tank's indices kept valid: its routes / area (paths with points), its route's point, the rubble's box
            auto bt_fix = [bt_path_ok, bt_path, bt_routes](uint32_t pv) {
                for (const uint32_t o : {0x14cu, 0x150u, 0x154u})
                    if (!bt_path_ok(ee_read<int32_t>(pv + o)) && !bt_routes.empty()) w32(pv + o, u(bt_routes[static_cast<size_t>(ri(0, static_cast<int>(bt_routes.size()) - 1))]));
                const int32_t n = ee_read<int32_t>(bt_path(ee_read<int32_t>(pv + 0x150)));
                const int32_t at = ee_read<int32_t>(pv + 0x168);
                if (at < 0 || at >= n) w32(pv + 0x168, u(ri(0, n - 1)));
                const int32_t nb = ee_read<int32_t>(OV(0x001600f0)), b = ee_read<int32_t>(pv + 0x148);
                if ((b < 0 || b >= nb) && nb > 0) w32(pv + 0x148, u(ri(0, nb - 1)));
            };
            // (v2) its two tracks (+0x160 / +0x164) valid: a word that is not a live class-331 moby of the table cleared, then the
            // missing ones spawned by its update's state 0 (the port's, as generic_prep's first run), its state put back
            auto bt_tracks = [bt_tab, bt_end](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                for (const uint32_t o : {0x160u, 0x164u}) {
                    const uint32_t t = ee_read<uint32_t>(pv + o);
                    if (t != 0 && (t < bt_tab || t >= bt_end || ((t - bt_tab) & 0xff) != 0 || ee_read<int16_t>(t + 0xa6) != 0x14b || ee_read<uint8_t>(t + 0x20) >= 0xfe)) w32(pv + o, 0);
                }
                if (ee_read<uint32_t>(pv + 0x160) != 0 && ee_read<uint32_t>(pv + 0x164) != 0) return;
                const uint32_t st = ee_read<uint8_t>(m + 0x20);
                w8(m + 0x20, 0);
                const uint32_t sp0 = ee_stack_ptr();
                try { bat_class253_update_l8(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
                w8(m + 0x20, st);
            };
            // a hit record `idx` of the hit table (0x178100, 64-byte entries) on `who`: the mask `mask`, the damage `dmg`, the source `src`
            auto bt_hit = [](uint32_t who, uint32_t idx, uint32_t mask, float dmg, uint32_t src) {
                const uint32_t e = OV(0x00178100) + (idx << 6);
                w8(who + 0xa4, idx);
                w32(e + 0x34, who);
                w32(e + 0x24, mask | (R() & 0xff));
                wfrand(e, 4, -1, 1);
                wf(e + 0x2c, dmg);
                w8(e + 0x28, u(ri(0, 11)));
                w8(e + 0x29, u(ri(0, 4)));
                w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
                wf(e + 0x1c, coin() ? 5627.95f : 0.0f);
                w32(e + 0x20, src);
            };
            // a hit's source: none, the hero's moby, or a moby-like record (M2) of a class the tank tells apart: 1633 (the skill
            // point's shot), 176 / 177 (the longer flinch), 425 / 71 (ignored), another
            auto bt_source = [P](bool skill) -> uint32_t {
                const int r = skill ? 2 : ri(0, 5);
                if (r == 0) return 0u;
                if (r == 1) return ee_read<uint32_t>(P + 0x2080);
                static const uint32_t cls[] = {0x661, 0x661, 0xb0, 0xb1, 0x1a9, 0x47, 0x10};
                w16(M2 + 0xa6, skill ? 0x661u : cls[ri(0, 6)]);
                return M2;
            };
            // the hits of a frame: none, or on the tank (mask 0x330000) and / or its tracks (0x210000), each of a damage below 4,
            // 4 .. 6, from 6, or 0; a mask that misses now and then
            auto bt_hits = [bt_hit, bt_source](uint32_t m, uint32_t pv, bool any, bool skill) {
                const uint32_t left = ee_read<uint32_t>(pv + 0x160), right = ee_read<uint32_t>(pv + 0x164);
                w8(m + 0xa4, 0xff);
                if (left != 0) w8(left + 0xa4, 0xff);
                if (right != 0) w8(right + 0xa4, 0xff);
                if (!any) return;
                static const float dm[] = {0.0f, 0.5f, 2.0f, 3.9f, 4.0f, 5.0f, 6.0f, 8.0f, 12.0f};
                auto dmg = [] { return dm[ri(0, 8)]; };
                auto mask = [](uint32_t kinds) { return ri(0, 5) == 0 ? 0x01000000u : (kinds & (coin() ? 0xffff0000u : (coin() ? 0x00010000u : 0x00200000u))); };
                const int who = skill ? 0 : ri(0, 4);  // 0 the tank, 1 the left track, 2 the right one, 3 all three, 4 the tank and one track
                if (who == 0 || who >= 3) bt_hit(m, u(ri(0, 4)), skill ? 0x330000u : mask(0x330000u), skill ? 12.0f : dmg(), bt_source(skill));
                if ((who == 1 || who == 3 || (who == 4 && coin())) && left != 0) bt_hit(left, u(ri(5, 9)), mask(0x210000u), dmg(), bt_source(false));
                if ((who == 2 || who == 3 || (who == 4 && coin())) && right != 0) bt_hit(right, u(ri(10, 15)), mask(0x210000u), dmg(), bt_source(false));
            };
            // the hero (and his moby) about the tank: in its targeting area, within 20, about 22, far; level with it, above (the
            // turret's pitch held at 20 degrees up) or below (45 down); the target moby cleared now and then
            auto bt_hero = [P, bt_path](uint32_t m, uint32_t pv) {
                Vec4 q = ee_ref<Vec4>(m + 0x10);
                const int r = ri(0, 4);
                if (r == 0) {
                    const uint32_t area = bt_path(ee_read<int32_t>(pv + 0x154));
                    const int32_t n = ee_read<int32_t>(area);
                    q = ee_ref<Vec4>(area + 0x10 + u(ri(0, n - 1)) * 0x10);
                    q.x += rf(-3, 3); q.y += rf(-3, 3);
                } else {
                    const float d = r == 1 ? rf(0.5f, 19.5f) : r == 2 ? rf(19.5f, 23.0f) : rf(23.0f, 90.0f);
                    const float a = rf(-3.1f, 3.1f);
                    q.x += d * std::cos(a); q.y += d * std::sin(a);
                    const int h = ri(0, 3);
                    q.z += h == 0 ? rf(8, 20) : h == 1 ? rf(-12, -3) : rf(-1, 4);
                }
                ee_ref<Vec4>(P + 0x80) = q;
                const uint32_t hm = ee_read<uint32_t>(P + 0x2080);
                if (hm != 0 && ri(0, 3) != 0) ee_ref<Vec4>(hm + 0x10) = q;
                if (ri(0, 3) == 0) w32(pv + 0xf0, 0);
            };
            // the tank's animation at the time `t` (its frame's time + the fraction +0x54, a step of 1 a frame): the bombs' frames 4 and
            // 10, the smoke's 29 / 59 / 89 / 119
            auto bt_anim_at = [](uint32_t m, float t) {
                const uint32_t cur = ee_read<uint8_t>(m + 0x52) == 0xff ? ee_read<uint32_t>(m + 0x6c) : ee_read<uint32_t>(m + 0x68);
                if (cur == 0 || cur >= 0x01ff0000u) return;
                w8(m + 0x51, 0);
                w8(m + 0x50, 1);
                wf(m + 0x54, t - static_cast<float>(ee_read<int16_t>(cur + 4)) * 0.0625f);
                wf(m + 0x58, 1.0f);
                wf(m + 0x5c, 1.0f);
            };
            // its engine: the sound slot +0x15c (none, free, playing its sound or another's) at a volume below 256, 256 .. 1023 or
            // from 1024; its velocity +0x40 (a hit's push) or none
            auto bt_engine = [sound_slot](uint32_t m, uint32_t pv) {
                const uint32_t s = sound_slot(m);
                w32(pv + 0x15c, s);
                if (s != 0xffffffffu) {
                    static const uint32_t vols[] = {0x40, 0xff, 0x100, 0x300, 0x3ff, 0x400, 0x600};
                    w32(0x0013e550u + s * 0x70 + 0x80, vols[ri(0, 6)]);
                }
                if (coin()) wfrand(pv + 0x40, 3, -0.2f, 0.2f);
                else for (uint32_t k = 0; k < 12; k += 4) wf(pv + 0x40 + k, 0.0f);
            };
            // a wall for the tank (its own when it has one)
            auto bt_wall = [bt_walls](uint32_t pv) {
                if (bt_walls.empty()) return;
                const int32_t own = ee_read<int32_t>(pv + 0x144);
                if (own > 0 && std::find(bt_walls.begin(), bt_walls.end(), u(own)) != bt_walls.end() && coin()) return;
                w32(pv + 0x144, bt_walls[static_cast<size_t>(ri(0, static_cast<int>(bt_walls.size()) - 1))]);
            };
            // the wall's pieces live or not (state 0x7f and the free slot 0xfe skipped), the free-moby count 0x15ffbc below 50 or not,
            // the camera ahead of the thrown pieces (within 15 degrees of the x axis) or anywhere; now and then the wall's group
            // list rebuilt in the test area (T) with the tank itself in it and one more of the level's mobys (not a piece)
            auto bt_wall_prep = [bt_tab, bt_list, bt_is_piece](uint32_t m, uint32_t pv) {
                const uint32_t wall = bt_tab + (ee_read<uint32_t>(pv + 0x144) << 8);
                const uint32_t list = bt_list(wall);
                if (list != 0 && ri(0, 2) == 0) {
                    std::vector<uint32_t> es;
                    for (uint32_t e = list; e < list + 0x200 && es.size() < 60; e += 2) {
                        es.push_back(ee_read<uint16_t>(e) & 0x7fffu);
                        if (ee_read<int16_t>(e) < -1) break;
                    }
                    es.insert(es.begin() + ri(0, static_cast<int>(es.size())), (m - bt_tab) >> 8);
                    if (const uint32_t x = u(ri(1, 40)); coin() && bt_tab + (x << 8) < ee_read<uint32_t>(OV(0x0015ffe0)) && !bt_is_piece(bt_tab + (x << 8)))
                        es.insert(es.begin() + ri(0, static_cast<int>(es.size())), x);  // (not a piece: its pvars +0x3c stay compared)
                    for (size_t k = 0; k < es.size(); ++k) w16(T + u(k) * 2, es[k] | (k + 1 == es.size() ? 0x8000u : 0u));
                    w32(OV(0x001ab840) + ee_read<uint8_t>(wall + 0x21) * 4, T);
                }
                const uint32_t l = bt_list(wall);
                for (uint32_t e = l; l != 0 && e < l + 0x200; e += 2) {
                    const uint32_t q = bt_tab + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                    if (q != m && ri(0, 4) == 0) w8(q + 0x20, coin() ? 0x7fu : 0xfeu);
                    if (ee_read<int16_t>(e) < -1) break;
                }
                w32(L8_ADDR(0x0015ffbc), u(ri(0, 3) == 0 ? ri(0, 49) : ri(50, 300)));
                Vec4 c = ee_ref<Vec4>(m + 0x10);
                const float r = rf(10, 60), b = coin() ? rf(-0.3f, 0.3f) : rf(-3.1f, 3.1f);
                c.x += r * std::cos(b); c.y += r * std::sin(b); c.z += rf(0, 10);
                ee_ref<Vec4>(OV(0x00166dc0)) = c;
            };
            // the skill point's state: the tank's number +0x184 (0 .. 4), the four marks 0x15f5b0 .. 0x15f5bc, the skill point taken or not
            auto bt_skill = [](uint32_t pv) {
                w32(pv + 0x184, u(ri(0, 4)));
                for (uint32_t k = 0; k < 16; k += 4) w32(L8_ADDR(0x0015f5b0) + k, coin() ? 1u : 0u);
                w8(0x0013d415, ri(0, 3) == 0 ? 1u : 0u);
            };

            if (bt_tanks.empty()) {
                std::printf("%-26s no tank (class 253) on this level\n", "class253_bomber_tank");
            } else {
                // class 253, the tank: each state (0 set up: let in by a moby, a wall to break or broken before, its tracks spawned
                // again; 3 firing at its animation's frames 4 / 10; 6 driving, the target within 20 and the help voice; 0xb the moby
                // that lets it in; 0xc the flinch's animation; 0xe the wall's break; 0x63 wrecked, smoking; 1 / 0xf nothing more).
                // Pass 0 no hit, pass 1 hits of every damage and source on the tank and its tracks, pass 2 low health (the wreck and
                // the end, the skill point, gone at once with +0x2e 2). Ignores: seg (the drive's floor probe: the segment record's
                // stale VU0 lane), mobys (the tracks, the bombs, the rubble, the debris and the bolts spawned), pool (the
                // explosions' sprays); in state 0xe the thrown pieces' pvars +0x3c (bt_ignore_throws)
                auto tank_prep = [=](uint32_t m, int st, int pass) {
                    generic_prep(m, st, 0, bat_class253_update_l8);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    bt_fix(pv);
                    if (st != 0 || ri(0, 3) != 0) bt_tracks(m);  // (v2: state 0 with its tracks there, 0x2d43f8 / 0x2d4480)
                    w8(m + 0x20, u(st));
                    w8(m + 0x53, u(ri(0, 2)));
                    if (st == 0) {
                        const int r = ri(0, 3);
                        w32(pv + 0x140, r == 0 && !bt_keys.empty() ? bt_keys[static_cast<size_t>(ri(0, static_cast<int>(bt_keys.size()) - 1))] : (coin() ? 0u : 0xffffffffu));
                        if (r == 1 || r == 3) bt_wall(pv); else w32(pv + 0x144, coin() ? 0u : 0xffffffffu);
                        w8(0x0013d3b6, r == 3 ? 1u : (r == 1 ? 0u : (coin() ? 1u : 0u)));
                        if (r == 3) bt_wall_prep(m, pv);
                        if (ri(0, 3) == 0) w32(pv + 0x160, 0);  // (its tracks spawned again)
                        if (ri(0, 3) == 0) w32(pv + 0x164, 0);
                    }
                    if (st == 0xb) {
                        if (!bt_keys.empty() && (ee_read<int32_t>(pv + 0x140) <= 0 || coin()))
                            w32(pv + 0x140, bt_keys[static_cast<size_t>(ri(0, static_cast<int>(bt_keys.size()) - 1))]);
                        const uint32_t key = bt_tab + (ee_read<uint32_t>(pv + 0x140) << 8);
                        if (ee_read<int32_t>(pv + 0x140) > 0 && ee_read<uint32_t>(key + 0x78) != 0)
                            wf(ee_read<uint32_t>(key + 0x78), ri(0, 2) != 0 ? 1.0f : (coin() ? 0.0f : rf(0.0f, 0.99f)));
                    }
                    if (st == 0xe) {
                        bt_wall(pv);
                        w8(0x0013d3b6, coin() ? 1u : 0u);
                        if (ee_read<int32_t>(pv + 0x144) > 0) bt_wall_prep(m, pv);
                    }
                    if (st == 3) {  // (v2: the animation 1 asked in half the cases, so that a fall back to driving (6) keeps the time; 4.0 exactly in a third)
                        w8(m + 0x53, coin() ? 1u : 2u);
                        static const float ts[] = {4.0f, 4.0f, 4.0f, 4.5f, 10.0f, 10.5f, 7.0f};
                        bt_anim_at(m, ri(0, 7) == 7 ? rf(0, 20) : ts[ri(0, 6)] + (ri(0, 2) != 0 ? 0.0f : rf(0.0f, 0.4f)));
                    }
                    if (st == 0x63) {
                        static const float ts[] = {59.0f, 59.5f, 60.0f, 119.0f, 120.0f, 29.0f, 29.5f, 30.0f, 89.0f, 90.0f, 45.0f, 100.0f};
                        bt_anim_at(m, ri(0, 6) == 0 ? rf(0, 125) : ts[ri(0, 11)]);
                    }
                    if (st == 6) {
                        voice_free();
                        w16(0x00141c38, ri(0, 3) == 0 ? 1u : 0u);
                        w8(0x0013d4cb, ri(0, 3) == 0 ? 1u : 0u);
                    }
                    bt_engine(m, pv);
                    bt_hero(m, pv);
                    bt_skill(pv);
                    if (pass == 0) bt_hits(m, pv, false, false);
                    if (pass == 1) {
                        wf(pv + 0x20, rf(1.0f, 9.0f));
                        bt_hits(m, pv, true, false);
                    }
                    if (pass == 2) {
                        const bool skill = ri(0, 2) == 0;
                        wf(pv + 0x20, coin() ? rf(0.1f, 2.0f) : rf(2.0f, 12.0f));
                        bt_hits(m, pv, true, skill);
                        if (ri(0, 5) == 0) {  // (gone at once; v2: with a track missing now and then, 0x2d4344)
                            w8(pv + 0x2e, 2);
                            if (coin()) w32(pv + 0x160, 0);
                            if (coin()) w32(pv + 0x164, 0);
                        }
                    }
                    frame50();
                    seed_rand();
                };
                static const int bt_states[] = {0, 1, 3, 6, 7, 0xb, 0xc, 0xe, 0xf, 0x63};  // (v2: 7, a state of 4 .. 10 but 6: 0x2d43c0)
                for (const int st : bt_states)
                    for (int pass = 0; pass < 3; ++pass) {
                        level();
                        ignores(true, true, true);
                        if (st == 0xe) bt_ignore_throws();
                        classcheck("class253_bomber_tank", 0x002d42d8, bat_class253_update_l8, [st, pass, tank_prep](uint32_t m) { tank_prep(m, st, pass); }, 12);
                    }
                // (v2) the firing alone (state 3, no hit): the target within 20 (it stays firing), its animation at the frame 4 exactly
                // (the joint 1, 0x2d489c) or 10 (the joint 2)
                level();
                ignores(true, true, true);
                classcheck("class253_fire", 0x002d42d8, bat_class253_update_l8, [tank_prep, bt_anim_at, hero_at, P](uint32_t m) {
                    tank_prep(m, 3, 0);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    w8(m + 0x53, 1);  // (the animation 1 asked: a fall back to driving keeps the time too)
                    hero_at(m, rf(1.0f, 15.0f), rf(-1.0f, 2.0f));
                    const uint32_t hm = ee_read<uint32_t>(P + 0x2080);
                    if (hm != 0) ee_ref<Vec4>(hm + 0x10) = ee_ref<Vec4>(P + 0x80);
                    w32(pv + 0xf0, 0);
                    bt_anim_at(m, coin() ? 4.0f : 10.0f);
                }, 20);
                // (P8.1) driving (state 6) with Ratchet within 20 in front of the area, the help voice's record and flag clear, the
                // voice line free: the spotting voice (0x1f44) and the turn to fire (state 3) with its animation asked
                classcheck("class253_spot", 0x002d42d8, bat_class253_update_l8, [tank_prep, hero_at, voice_free, P](uint32_t m) {
                    tank_prep(m, 6, 0);
                    w16(0x00141c38, 0);
                    w8(0x0013d4cb, 0);
                    voice_free();
                    if (coin()) w8(m + 0x53, 0);
                    hero_at(m, rf(1.0f, 18.0f), rf(-1.0f, 2.0f));
                    const uint32_t hm = ee_read<uint32_t>(P + 0x2080);
                    if (hm != 0) ee_ref<Vec4>(hm + 0x10) = ee_ref<Vec4>(P + 0x80);
                }, 20);

                // the tank's frame, directly: every state's hits (on the tank, a track, all three; no track now and then), the flinch's
                // damages and sources, the wreck (state 6 / 3 / 0xc at low health) and the end (0x63), the skill point's marks and its
                // shot, the target's place (the pitch's two limits, the mode 2). Ignores: mobys (the debris, the bolts), pool (the explosions)
                level();
                ignores(false, true, true);
                l8check("bomber_tank_frame_l8", 0x002d4ca0, 120, Ret::None, [=] {
                    level();
                    const uint32_t m = bt_tanks[static_cast<size_t>(ri(0, static_cast<int>(bt_tanks.size()) - 1))];
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    bt_fix(pv);
                    if (ri(0, 3) != 0) bt_tracks(m);  // (v2)
                    static const uint32_t sts[] = {0, 3, 6, 0xb, 0xc, 0xe, 0x63, 0x63};
                    w8(m + 0x20, sts[ri(0, 7)]);
                    w8(m + 0x53, u(ri(0, 2)));
                    if (ri(0, 7) == 0) w32(pv + 0x160, 0);  // (a track that did not spawn)
                    if (ri(0, 7) == 0) w32(pv + 0x164, 0);
                    const bool skill = ri(0, 3) == 0;
                    if (skill) w8(m + 0x20, 0x63);
                    bt_skill(pv);
                    wf(pv + 0x20, ri(0, 2) == 0 ? rf(0.1f, 3.0f) : rf(3.0f, 12.0f));
                    bt_hits(m, pv, ri(0, 5) != 0, skill);
                    bt_hero(m, pv);
                    frame50();
                    seed_rand();
                    return Args{{m}, {}};
                }, [](A a) { bomber_tank_frame_l8(a.i[0]); return uint64_t{0}; });

                // the drive, directly: its route (its own, its entry's or another path), the point it heads for (the last one with
                // the tank on it: back to the first), its speed (the tracks' animation speed held at 2), a track missing now and
                // then, its engine (started, turned up or down, stopped). Ignores: seg (the floor probe's segment record)
                level();
                ignores(true, false, false);
                l8check("bomber_tank_drive_l8", 0x002d55c8, 80, Ret::None, [=] {
                    level();
                    const uint32_t m = bt_tanks[static_cast<size_t>(ri(0, static_cast<int>(bt_tanks.size()) - 1))];
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    bt_fix(pv);
                    if (ri(0, 3) != 0) bt_tracks(m);  // (v2)
                    const int r = ri(0, 3);
                    const int32_t route = r == 0 ? ee_read<int32_t>(pv + 0x150) : r == 1 ? ee_read<int32_t>(pv + 0x14c)
                                        : bt_routes[static_cast<size_t>(ri(0, static_cast<int>(bt_routes.size()) - 1))];
                    const uint32_t path = bt_path(route);
                    const int32_t n = ee_read<int32_t>(path);
                    const int32_t at = coin() ? n - 1 : ri(0, n - 1);
                    w32(pv + 0x168, u(at));
                    if (coin()) {  // on the point (within 0.5)
                        Vec4 q = ee_ref<Vec4>(path + 0x10 + u(at) * 0x10);
                        q.x += rf(-0.3f, 0.3f); q.y += rf(-0.3f, 0.3f);
                        ee_ref<Vec4>(m + 0x10) = q;
                    }
                    wf(pv + 0x180, ri(0, 3) == 0 ? rf(0.15f, 0.4f) : rf(0.0f, 0.15f));
                    wf(pv + 0x16c, 2.0f * ee_read<float>(0x0015ed6c));  // (its top speed, as set up)
                    wf(pv + 0x158, rf(-0.2f, 0.2f));
                    wf(m + 0x48, rf(-3.1f, 3.1f));
                    if (ri(0, 7) == 0) w32(pv + 0x160, 0);
                    if (ri(0, 7) == 0) w32(pv + 0x164, 0);
                    bt_engine(m, pv);
                    frame50();
                    return Args{{m, u(route)}, {}};
                }, [](A a) { bomber_tank_drive_l8(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });

                // the smoke, directly: its joints 4 and 5 (the bombs' 1 / 2 now and then), 10 puffs (or 0 .. 12: the first third and
                // the rest), its velocity +0x40 (a hit's push) or none, the 50 Hz frame times. Ignores: none
                level();
                ignores(false, false, false);
                l8check("bomber_tank_smoke_l8", 0x002d5950, 40, Ret::None, [=] {
                    level();
                    const uint32_t m = bt_tanks[static_cast<size_t>(ri(0, static_cast<int>(bt_tanks.size()) - 1))];
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    static const uint32_t joints[] = {4, 5, 4, 5, 1, 2};
                    const int32_t count = ri(0, 2) != 0 ? 10 : ri(0, 12);
                    if (coin()) wfrand(pv + 0x40, 4, -0.2f, 0.2f);
                    frame50();
                    seed_rand();
                    return Args{{m, joints[ri(0, 5)], u(count)}, {}};
                }, [](A a) { bomber_tank_smoke_l8(a.i[0], a.i[1], static_cast<int32_t>(a.i[2])); return uint64_t{0}; });

                if (bt_walls.empty()) {
                    std::printf("%-26s no wall (a moby whose group lists classes 588 .. 598 / 472) on this level\n", "bomber_tank_break_wall_l8");
                } else {
                    // the wall's break, directly: each wall, the rubble loop (the free-moby count 0x15ffbc from 50, or below: none),
                    // the pieces thrown toward the camera or away, the breakable part blown up, pieces not live skipped, the tank in
                    // its own wall's list (the list rebuilt in T). Ignores: mobys (the rubble spawned), pool (the part's explosion),
                    // the thrown pieces' pvars +0x3c (bt_ignore_throws: the velocity's stale w lane)
                    level();
                    ignores(false, true, true);
                    bt_ignore_throws();
                    l8check("bomber_tank_break_wall_l8", 0x002d5d08, 40, Ret::None, [=] {
                        level();
                        const uint32_t m = bt_tanks[static_cast<size_t>(ri(0, static_cast<int>(bt_tanks.size()) - 1))];
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        bt_fix(pv);
                        bt_wall(pv);
                        bt_wall_prep(m, pv);
                        frame50();
                        seed_rand();
                        return Args{{m}, {}};
                    }, [](A a) { bomber_tank_break_wall_l8(a.i[0]); return uint64_t{0}; });

                    // the wall already broken, directly: its pieces and its part retired, those not live and the tank left alone.
                    // Ignores: none
                    level();
                    ignores(false, false, false);
                    l8check("bomber_tank_wall_gone_l8", 0x002d6450, 30, Ret::None, [=] {
                        level();
                        const uint32_t m = bt_tanks[static_cast<size_t>(ri(0, static_cast<int>(bt_tanks.size()) - 1))];
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        bt_wall(pv);
                        bt_wall_prep(m, pv);
                        return Args{{m}, {}};
                    }, [](A a) { bomber_tank_wall_gone_l8(a.i[0]); return uint64_t{0}; });
                }

                // a bomb, directly: at a point by the tank (OUT) with a velocity (OUT + 0x10, w lanes too). Ignores: mobys (the
                // bomb spawned)
                level();
                ignores(false, true, false);
                l8check("bomber_tank_bomb_l8", 0x002dc8a0, 30, Ret::Int32, [=] {
                    level();
                    const uint32_t m = bt_tanks[static_cast<size_t>(ri(0, static_cast<int>(bt_tanks.size()) - 1))];
                    Vec4 q = ee_ref<Vec4>(m + 0x10);
                    q.x += rf(-2, 2); q.y += rf(-2, 2); q.z += rf(0, 3); q.w = rf(-1, 1);
                    ee_ref<Vec4>(OUT) = q;
                    wfrand(OUT + 0x10, 4, -0.5f, 0.5f);
                    seed_rand();
                    return Args{{m, OUT, OUT + 0x10}, {}};
                }, [](A a) { return r32(bomber_tank_bomb_l8(a.i[0], a.i[1], a.i[2])); });
            }
        }
        // ---- (P8.1 h2) class 333, the grenadier (batalia_grenadier.cpp), and class 152, the bolt prize (batalia_bolt_prize.cpp)
        // the hero (his position and his moby's, the grenadier's target) `d` away from the moby, in a random direction
        auto target_at = [P](uint32_t m, float d) {
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            const float a = rf(-3.1f, 3.1f);
            q.x += d * std::cos(a); q.y += d * std::sin(a); q.z += rf(-1, 2);
            ee_ref<Vec4>(0x0013f3d0) = q;
            const uint32_t hm = ee_read<uint32_t>(P + 0x2080);
            if (hm != 0) { Vec4 h = ee_ref<Vec4>(hm + 0x10); h.x = q.x; h.y = q.y; h.z = q.z; ee_ref<Vec4>(hm + 0x10) = h; }
        };
        // the camera (its position 0x1675c0, its yaw 0x1675d8) 4 .. 40 from the moby, facing it (within 0.5) or turned away
        auto camera_view = [](uint32_t m, bool facing) {
            const float b = rf(-3.1f, 3.1f), r = rf(4, 40);
            Vec4 c = ee_ref<Vec4>(m + 0x10);
            c.x -= r * std::cos(b); c.y -= r * std::sin(b); c.z += rf(1, 6);
            ee_ref<Vec4>(L8_ADDR(0x001675c0)) = c;
            wf(L8_ADDR(0x001675d8), facing ? b + rf(-0.5f, 0.5f) : b + (coin() ? 1.0f : -1.0f) * rf(0.6f, 3.0f));
        };
        // a hit on the moby (as generic_prep's, the mask 0x330000) with the damage `dmg`
        auto hit_with = [P](uint32_t m, float dmg) {
            const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
            w8(m + 0xa4, idx);
            w32(e + 0x34, m);
            w32(e + 0x24, 0x330000u | (R() & 0xff));
            wfrand(e, 4, -1, 1);
            w32(e + 0x28, u(ri(0, 20)));
            w8(e + 0x28, u(ri(0, 11))); w8(e + 0x29, u(ri(0, 4))); w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
            wf(e + 0x1c, coin() ? 5627.95f : 0.0f);
            w32(e + 0x20, ri(0, 3) == 0 ? 0u : ee_read<uint32_t>(P + 0x2080));
            wf(e + 0x2c, dmg);
        };
        // the mobys of the moby `m`'s group (+0x21; none without a list)
        auto group_of = [](uint32_t m) {
            std::vector<uint32_t> v;
            const uint32_t g = ee_read<uint8_t>(m + 0x21);
            if (g >= 0x70) return v;
            const uint32_t list = ee_read<uint32_t>(OV(0x001ab840) + g * 4);
            for (uint32_t e = list; list != 0 && e < list + 0x80; e += 2) {
                v.push_back(ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8));
                if (ee_read<int16_t>(e) < 0) break;
            }
            return v;
        };
        // the posts of the grenadier's group (its members not of class 333) taken (+0xbc) or free at random
        auto posts_mixed = [group_of](uint32_t m) {
            for (const uint32_t o : group_of(m))
                if (ee_read<int16_t>(o + 0xa6) != 0x14d) w8(o + 0xbc, ri(0, 2) == 0 ? 1u : 0u);
        };
        // (v2) every post of the grenadier's group taken (+0xbc): the next post finds none
        auto posts_all_taken = [group_of](uint32_t m) {
            for (const uint32_t o : group_of(m))
                if (ee_read<int16_t>(o + 0xa6) != 0x14d) w8(o + 0xbc, 1);
        };
        // the groups of the level without a list (a grenadier there finds no post)
        level();
        std::vector<int32_t> unlisted_groups;
        for (int32_t i = 0; i < 0x70; ++i) if (ee_read<uint32_t>(OV(0x001ab840) + u(i) * 4) == 0) unlisted_groups.push_back(i);
        // one frame of the port's update on the moby (the preparation of a later state)
        auto port_frame333 = [](uint32_t m) {
            const uint32_t sp0 = ee_stack_ptr();
            try { bat_class333_update_l8(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
        };

        // class 333, the grenadier: each state (0 .. 6, 90 .. 93, 99, 100 and 2, which does nothing) after its set-up, the
        // generic perturbation (a hit, the hero, the pvars) and per state: the set-up with its group's posts taken or free (or a
        // group without a list); 1 at its post (within 3, facing it or not) with Ratchet within 6 or farther, or away with its
        // post timer at its end; 6 the animation's end (+0x70 bit 2) with the animation 1 playing or not; 3 Ratchet 0.2 .. 30
        // away (8 .. 20 the rise), the throw timer at its end or not, the camera facing it or not, the rise asked (+0x38); 4 at
        // its spot 2 in front of the post or off it, the animation's end, Ratchet within 6, the camera's view; 5 the animation
        // at its time 19 (its own key frame and a blend) with Ratchet 1 .. 30 away (the reach's clamps), or its
        // end; 91 the fall's end; 90 / 92 / 93 the move's end or below a height of 5; 99 the move's end; 100 its fade (+0x130 /
        // +0x132) part way; (v2) +0x31 set or not (the floor band), every post taken in 1 / 4 / 90 .. 93 (no next post)
        level();
        const std::vector<uint32_t> grenadiers = mobys_with(0x002dabf0);
        if (grenadiers.empty()) std::printf("%-26s no grenadier (class 333) on this level\n", "class333_grenadier");
        auto grenadier_prep = [generic_prep, frame50, target_at, camera_view, posts_mixed, posts_all_taken, group_of, unlisted_groups, P](uint32_t m, int st, int pass) {
            generic_prep(m, st, pass, bat_class333_update_l8);
            frame50();
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            camera_view(m, coin());
            w8(m + 0x31, coin() ? 1u : 0u);  // (v2: +0x31 set, the floor band within 29 of the camera)
            w32(pv + 0x38, ri(0, 3) == 0 ? 1u : 0u);
            if (ri(0, 4) == 0) w32(P + 0x2084, 0x72);  // (the hero out of reach: the target the hero's moby all the same)
            uint32_t post = ee_read<uint32_t>(pv + 0x120);
            if (st != 0 && post == 0)  // (no post taken at the set-up: one of the group's, or none)
                for (const uint32_t o : group_of(m)) if (ee_read<int16_t>(o + 0xa6) != 0x14d) { post = o; w32(pv + 0x120, o); w8(o + 0xbc, 1); break; }
            auto end_flag = [m](bool on) { w8(m + 0x70, on ? (ee_read<uint8_t>(m + 0x70) | 2u) : (ee_read<uint8_t>(m + 0x70) & ~2u)); };
            auto anim_now = [m](uint32_t set) { w8(m + 0x53, set); w8(m + 0x52, set); };
            switch (st) {
            case 0:
                posts_mixed(m);
                if (ri(0, 5) == 0 && !unlisted_groups.empty()) w8(m + 0x21, u(unlisted_groups[static_cast<size_t>(ri(0, static_cast<int>(unlisted_groups.size()) - 1))]));
                else if (ri(0, 5) == 0) posts_all_taken(m);
                target_at(m, rf(0, 40));
                break;
            case 1:
                if (post != 0 && pass != 1) {
                    Vec4 q = ee_ref<Vec4>(post + 0x10);
                    q.x += rf(-2, 2); q.y += rf(-2, 2); q.z += rf(-0.5f, 0.5f);
                    if (ri(0, 4) == 0) { q.x += 3; q.y += 3; }
                    ee_ref<Vec4>(m + 0x10) = q;
                    const float yaw = std::atan2(ee_read<float>(post + 0x14) - q.y, ee_read<float>(post + 0x10) - q.x);
                    wf(m + 0x48, yaw + (coin() ? rf(-0.2f, 0.2f) : rf(0.3f, 3.0f)));
                }
                target_at(m, coin() ? rf(0, 6) : rf(6, 25));
                w32(pv + 0x134, ri(0, 2) == 0 ? u(ri(0, 1)) : u(ri(2, 240)));
                if (ri(0, 2) == 0) posts_all_taken(m); else posts_mixed(m);
                break;
            case 6:
                end_flag(ri(0, 3) != 0);
                anim_now(coin() ? 1u : 5u);
                target_at(m, rf(0, 25));
                break;
            case 3: {
                const int r = ri(0, 4);
                target_at(m, r == 0 ? rf(0.2f, 0.6f) : r == 1 ? rf(0.6f, 8) : r == 2 ? rf(20, 30) : rf(8.2f, 19.8f));
                w32(pv + 0x128, ri(0, 2) != 0 ? u(ri(0, 1)) : u(ri(2, 180)));
                anim_now(coin() ? 2u : 1u);
                break;
            }
            case 4:
                if (post != 0) {
                    const float yaw = rf(-3.1f, 3.1f);
                    wf(m + 0x48, yaw);
                    Vec4 q = ee_ref<Vec4>(post + 0x10);
                    const float off = coin() ? rf(0, 0.15f) : rf(0.3f, 2.0f), b = rf(-3.1f, 3.1f);
                    q.x += 2.0f * std::cos(yaw) + off * std::cos(b); q.y += 2.0f * std::sin(yaw) + off * std::sin(b);
                    ee_ref<Vec4>(m + 0x10) = q;
                }
                end_flag(ri(0, 3) != 0);
                anim_now(coin() ? 4u : 3u);
                target_at(m, coin() ? rf(0, 6) : rf(6, 25));
                camera_view(m, coin());
                if (ri(0, 2) == 0) posts_all_taken(m); else posts_mixed(m);
                break;
            case 5:
                if (pass != 2) {
                    // (v2) the throw's frame: the animation time 19 (or 18.5) from its own key frame (+0x68, its frame at +4 in
                    // 16ths) and a blend (+0x51 below +0x50) of 19 less that frame (+0x54; the sum exact): the key frame stays
                    // the moby's own (the chain point 9 the throw takes reads its joints from it)
                    anim_now(3);
                    const uint32_t key = ee_read<uint32_t>(m + 0x68);
                    const float frame = static_cast<float>(ee_read<int16_t>(key + 4)) * 0.0625f;
                    w8(m + 0x50, 4);
                    w8(m + 0x51, u(ri(0, 3)));
                    const float t = (ri(0, 3) == 0 ? 18.5f : 19.0f) - frame;
                    wf(m + 0x54, t != 0.0f ? t : 1.0f);
                } else if (coin()) {
                    w8(m + 0x52, ee_read<uint8_t>(m + 0x53) ^ 1u);
                }
                end_flag(coin());
                if (coin()) anim_now(4);
                target_at(m, rf(1, 30));
                break;
            case 91:
                end_flag(ri(0, 3) != 0);
                anim_now(coin() ? 7u : 4u);
                break;
            case 90: case 92: case 93:  // (v2) enemy_physics's 0x40 is the animation's end (+0x70 bit 2): the move's end
                if (ri(0, 2) == 0) wf(m + 0x18, rf(-20, 4.9f));  // (out of the level)
                end_flag(ri(0, 3) != 0);
                anim_now(coin() ? 4u : 1u);
                if (ri(0, 2) == 0) posts_all_taken(m); else posts_mixed(m);
                break;
            case 99:  // (v2) the throw's end (the animation's end): state 100, the death arcs, gone
                end_flag(ri(0, 3) != 0);
                break;
            case 100:
                w16(pv + 0x132, 0x28);
                w16(pv + 0x130, ee_read<uint16_t>(L8_ADDR(0x001619d0)) - u(ri(0, 60)));
                break;
            default:
                break;
            }
        };
        if (!grenadiers.empty()) {
            static const int states333[] = {0, 1, 2, 3, 4, 5, 6, 90, 91, 92, 93, 99, 100};
            for (const int st : states333)
                for (int pass = 0; pass < 3; ++pass) {
                    level();
                    // (pass 1 / 2 carry a hit: a death drops its bolts, spawned mobys; state 5 throws a grenade, a spawned moby;
                    // 99 / 100 the death arcs' sparks)
                    ignores(false, pass >= 1 || st == 5, st >= 99);
                    classcheck("class333_grenadier", 0x002dabf0, bat_class333_update_l8, [st, pass, grenadier_prep](uint32_t m) { grenadier_prep(m, st, pass); }, 12);
                }
            // (P8.1) the hit's three outcomes: a state of the walk / the post / the throw, its health 0.4 .. 2 and a hit of 0.5
            // (the colour pulse), 1.5 (knocked back) or 5 (killed: its bolts, its post freed)
            level();
            ignores(false, true, false);
            classcheck("class333_hit", 0x002dabf0, bat_class333_update_l8, [grenadier_prep, hit_with](uint32_t m) {
                static const int sts[] = {1, 3, 4, 5, 6};
                grenadier_prep(m, sts[ri(0, 4)], 0);
                static const float hp[] = {2.0f, 1.0f, 0.4f};
                static const float dm[] = {0.5f, 1.5f, 5.0f};
                wf(ee_read<uint32_t>(m + 0x78) + 0x20, hp[ri(0, 2)]);
                hit_with(m, dm[ri(0, 2)]);
            }, 20);
            // (P8.1) the move's end: knocked back (1.5) or killed (5) by a hit the port's frame takes, then the frame after it
            // with the animation's end (+0x70 bit 2, enemy_physics's 0x40) in most cases (the next post, every one taken or
            // not; or state 100 with the death arcs and gone); a third of the cases thrown below a height of 5 instead (gone)
            level();
            ignores(false, true, true);
            classcheck("class333_landing", 0x002dabf0, bat_class333_update_l8, [grenadier_prep, hit_with, port_frame333, posts_all_taken](uint32_t m) {
                grenadier_prep(m, 1, 0);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                wf(pv + 0x20, 2.0f);
                hit_with(m, coin() ? 1.5f : 5.0f);
                port_frame333(m);
                const uint32_t st = ee_read<uint8_t>(m + 0x20);
                if (st != 0x5a && st != 0x63) return;  // (the hit not taken: a case of its state all the same)
                if (ri(0, 2) == 0) { wf(m + 0x18, rf(-20, 4.9f)); return; }
                if (ri(0, 4) != 0) w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u);
                if (coin()) posts_all_taken(m);
                seed_rand();
            }, 20);
        }

        // the grenadier's helpers, directly: the grenade's spawn from a point with a velocity (the test area) by a grenadier
        // or another moby; the next post with its group's posts taken or free, Ratchet and the grenadier moved about, a group
        // without a list now and then
        level();
        ignores(false, false, false);
        if (grenadiers.empty()) std::printf("%-26s no grenadier (class 333) on this level\n", "grenade_spawn_l8");
        else {
            l8check("grenade_spawn_l8", 0x002d22e0, 40, Ret::Int32, [level, grenadiers] {
                level();
                const uint32_t m = ri(0, 3) != 0 ? grenadiers[static_cast<size_t>(ri(0, static_cast<int>(grenadiers.size()) - 1))]
                                                 : ee_read<uint32_t>(OV(0x0015ffd8)) + u(ri(0, 40)) * 0x100;
                Vec4 at = ee_ref<Vec4>(m + 0x10);
                at.x += rf(-1, 1); at.y += rf(-1, 1); at.z += rf(0, 2);
                ee_ref<Vec4>(OUT) = at;
                wfrand(OUT + 0x10, 4, -0.5f, 0.5f);
                seed_rand();
                return Args{{m, OUT, OUT + 0x10}, {}};
            }, [](A a) { return r32(grenade_spawn_l8(a.i[0], a.i[1], a.i[2])); });
            l8check("grenadier_next_post_l8", 0x002daa10, 60, Ret::Int32, [level, grenadiers, posts_mixed, target_at, unlisted_groups] {
                level();
                const uint32_t m = grenadiers[static_cast<size_t>(ri(0, static_cast<int>(grenadiers.size()) - 1))];
                posts_mixed(m);
                if (ri(0, 4) == 0) { Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-15, 15); q.y += rf(-15, 15); ee_ref<Vec4>(m + 0x10) = q; }
                target_at(m, rf(0, 40));
                if (ri(0, 6) == 0 && !unlisted_groups.empty()) w8(m + 0x21, u(unlisted_groups[static_cast<size_t>(ri(0, static_cast<int>(unlisted_groups.size()) - 1))]));
                return Args{{m}, {}};
            }, [](A a) { return r32(grenadier_next_post_l8(a.i[0])); });
        }

        // class 152, the bolt prize: none on the level, so spawned (moby_spawn(152), the slot the spawn takes after level(): the
        // same each time) a few units from the hero's start; its value, falling or not (+0x2c); each state 0 .. 5 after its
        // set-up (its glow), the generic perturbation, and: its velocity 0 .. 1.5 a frame (the fall's clamp), the frame count's
        // parity (the probe's frames), Ratchet touching it or not both ways (in his mode 0xf (P+0x208c) one of his points
        // +0x510 / +0x520 / +0x530 within 0.5 or not; else in or out of his reach +0x2288 / +0x228c), its glow gone or not, its
        // bob's frame count
        level();
        const uint32_t prize = moby_spawn(0x98);
        if (prize == 0) std::printf("%-26s no slot for a prize (class 152) on this level\n", "class152_bolt_prize");
        else std::printf("INFO class152 the prize spawned at 0x%08x (class 0x%08x, update 0x%08x)\n", prize, ee_read<uint32_t>(prize + 0x24), ee_read<uint32_t>(prize + 0x74));
        auto prize_spawn = [P](uint32_t m) {
            const uint32_t s = moby_spawn(0x98);
            (void)s;  // (== m: the same slot after level())
            Vec4 q = ee_ref<Vec4>(P + 0x80);
            q.x += rf(-6, 6); q.y += rf(-6, 6); q.z += rf(0, 3);
            ee_ref<Vec4>(m + 0x10) = q;
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w32(pv + 0x20, u(coin() ? ri(1, 50) : ri(100, 5000)));
            w32(pv + 0x2c, coin() ? 1u : 0u);
        };
        auto prize_prep = [generic_prep, frame50, prize_spawn, hero_at, camera_near, P](uint32_t m, int st, int pass) {
            prize_spawn(m);
            camera_near(m, 20);
            generic_prep(m, st, pass, bat_class152_update_l8);
            frame50();
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            { const float v = coin() ? 0.3f : 1.5f; wfrand(pv + 0x10, 3, -v, v); }
            w32(pv + 0x28, u(ri(0, 2) == 0 ? ri(0, 12) : ri(0, 400)));
            if (ri(0, 4) == 0) w32(pv + 0x24, 0);
            const bool mode_f = ri(0, 2) == 0;
            w32(P + 0x208c, mode_f ? 0xfu : (coin() ? ee_read<uint32_t>(P + 0x208c) : 0u));
            if (mode_f) {
                for (const uint32_t o : {0x510u, 0x520u, 0x530u}) {
                    Vec4 q = ee_ref<Vec4>(m + 0x10);
                    const float r = ri(0, 3) == 0 ? rf(0, 0.45f) : rf(0.55f, 4), a = rf(-3.1f, 3.1f);
                    q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-1, 1);
                    ee_ref<Vec4>(P + o) = q;
                }
            } else if (pass != 1) {
                const float reach = ee_read<float>(P + 0x2288), up = ee_read<float>(P + 0x228c);
                hero_at(m, coin() ? rf(0, reach) : rf(reach, reach * 2 + 1), coin() ? rf(-up, up) : (coin() ? 1.0f : -1.0f) * rf(up, up * 2 + 1));
            }
        };
        if (prize != 0) {
            level();
            ignores(false, false, false);
            for (int st = 0; st < 6; ++st)
                for (int pass = 0; pass < 3; ++pass)
                    classcheck("class152_bolt_prize", 0x002b82a0, bat_class152_update_l8, [st, pass, prize_prep](uint32_t m) { prize_prep(m, st, pass); }, 12, {prize});
            // the prize's helpers, directly: its set-up and its glow on the spawned prize (the camera about it), the glow also on
            // another of the level's mobys
            level();
            ignores(false, false, false);
            l8check("bolt_prize_setup_l8", 0x002b8228, 20, Ret::None, [level, prize, prize_spawn, camera_near] {
                level();
                prize_spawn(prize);
                camera_near(prize, 20);
                wf(prize + 0x48, rf(-3.1f, 3.1f));
                seed_rand();
                return Args{{prize}, {}};
            }, [](A a) { bolt_prize_setup_l8(a.i[0]); return uint64_t{0}; });
            l8check("bolt_prize_glow_l8", 0x00279f00, 20, Ret::Int32, [level, prize, prize_spawn, camera_near] {
                level();
                prize_spawn(prize);
                const uint32_t m = coin() ? prize : ee_read<uint32_t>(OV(0x0015ffd8)) + u(ri(0, 40)) * 0x100;
                camera_near(m, 30);
                return Args{{m}, {}};
            }, [](A a) { return r32(bolt_prize_glow_l8(a.i[0])); });
        }
        // ---- (P8.1, h3) the grenade (class 248), the Bomber Tank's shell (class 425) and the sea (class 327). The grenade and the
        // shell are spawned mobys (none in the level's data): each case spawns one through its spawner's port (grenade_spawn_l8 /
        // bomber_tank_bomb_l8: the same slot after every level(), so the subject is that slot) and sets it up as its thrower does,
        // placed (0) just over the level's floor at a point a probe segment found (the bounce, the slow bounce that brings it to
        // rest, the shell's hit), (1) inside another moby's reach (the touch test: its thrower / owner or not), (2) aimed through
        // a moby from out of its reach (the path test's moby hit: its thrower / owner or not), or (3) in the air (the misses, the
        // fuse); one pass each
        level();
        const uint32_t h3_hero = ee_read<uint32_t>(P + 0x2080);
        const uint32_t h3_grenade = moby_spawn(0xf8);
        level();
        const uint32_t h3_shell = moby_spawn(0x1a9);
        level();
        std::vector<uint32_t> h3_live;  // the level's live mobys (touch targets, throwers, owners)
        for (uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)); o < ee_read<uint32_t>(OV(0x0015ffe0)); o += 0x100)
            if (ee_read<uint8_t>(o + 0x20) < 0xfe && o != h3_grenade && o != h3_shell) h3_live.push_back(o);
        h3_live.push_back(h3_hero);
        const std::vector<uint32_t> h3_grenadiers = mobys_with(0x002dabf0), h3_tanks = mobys_with(0x002d42d8);
        auto h3_any = [h3_live](const std::vector<uint32_t>& prefer) -> uint32_t {
            if (!prefer.empty() && coin()) return prefer[static_cast<size_t>(ri(0, static_cast<int>(prefer.size()) - 1))];
            return h3_live[static_cast<size_t>(ri(0, static_cast<int>(h3_live.size()) - 1))];
        };
        // the level's floor under (x, y): a probe segment from 3 above `z` to 6 below it (Ratchet's moby ignored), its contact's z
        // (false: no floor there). (Run by the generator: the collision block it leaves is the start state of both sides.)
        auto h3_floor = [h3_hero](float x, float y, float z, float& floor_z) -> bool {
            Vec4 a{x, y, z + 3.0f, 0.0f}, b{x, y, z - 6.0f, 0.0f};
            ee_ref<Vec4>(T3 + 0x20) = a;
            ee_ref<Vec4>(T3 + 0x30) = b;
            if (segment_collide(T3 + 0x20, T3 + 0x30, 0, h3_hero, 0) == 0) return false;
            floor_z = ee_read<float>(OV(0x00173e68));
            return true;
        };
        // a place and a velocity for a projectile: `where` 0 just over the floor 1 to 4 from Ratchet (`slow`: a hundredth of a unit
        // a frame, so that the bounce brings it to rest), 1 within a moby's reach (the moby to `near`), 2 aimed through a moby (to
        // `near`) from 0.7 to 3 away, the path checked by probes to hit it while no moby is within 0.5 of the start (`lower`: how
        // much lower than the projectile its path test runs, 0.16 for the grenade), 3 in the air; to T3 / T3 + 0x10
        auto h3_place = [h3_floor, h3_any](int where, bool slow, uint32_t& near, float lower) {
            const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
            Vec4 at = h, vel{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(-0.4f, 0.2f), 0.0f};
            near = 0;
            float fz = 0.0f;
            const float a = rf(-3.1f, 3.1f), d = rf(1.0f, 4.0f);
            if (where == 0 && h3_floor(h.x + d * std::cos(a), h.y + d * std::sin(a), h.z, fz)) {
                at.x = h.x + d * std::cos(a);
                at.y = h.y + d * std::sin(a);
                if (slow) {
                    at.z = fz + 0.16f + rf(0.0f, 0.004f);
                    vel = Vec4{(coin() ? 1.0f : -1.0f) * rf(0.002f, 0.006f), (coin() ? 1.0f : -1.0f) * rf(0.002f, 0.006f), -rf(0.005f, 0.015f), 0.0f};
                } else {
                    const float up = rf(0.0f, 0.4f);
                    at.z = fz + 0.16f + up;
                    vel = Vec4{rf(-0.15f, 0.15f), rf(-0.15f, 0.15f), ri(0, 4) == 0 ? -rf(0.0f, up) : -rf(up, up + 0.8f), 0.0f};  // (1 in 5 short of the floor)
                }
            } else if (where == 1) {
                near = h3_any({});
                at = ee_ref<Vec4>(near + 0x10);
                at.x += rf(-0.4f, 0.4f); at.y += rf(-0.4f, 0.4f); at.z += rf(0.0f, 1.2f);
            } else if (where == 2) {
                for (int tries = 0; tries < 12; ++tries) {
                    near = h3_any({});
                    Vec4 c = ee_ref<Vec4>(near + 0x10);
                    c.z += rf(0.2f, 1.2f);
                    const float yaw = rf(-3.1f, 3.1f), pitch = rf(-0.6f, 0.6f), dist = rf(0.7f, 3.0f), k = rf(1.1f, 1.8f);
                    const float dx = std::cos(yaw) * std::cos(pitch), dy = std::sin(yaw) * std::cos(pitch), dz = std::sin(pitch);
                    at = Vec4{c.x + dx * dist, c.y + dy * dist, c.z + dz * dist + lower, 0.0f};
                    vel = Vec4{-dx * dist * k, -dy * dist * k, -dz * dist * k, 0.0f};
                    const Vec4 s{at.x, at.y, at.z - lower, 0.0f}, e{at.x + vel.x, at.y + vel.y, at.z - lower + vel.z, 0.0f};
                    ee_ref<Vec4>(T3 + 0x20) = s;
                    ee_ref<Vec4>(T3 + 0x30) = e;
                    ee_ref<Vec4>(T3 + 0x40) = at;
                    if (segment_collide(T3 + 0x20, T3 + 0x30, 0, 0, 0) != 0 && ee_read<uint32_t>(OV(0x00173e58)) == near &&
                        sphere_collide(T3 + 0x40, 0.5f, 0x11, 0) == 0 && mobys_in_sphere(T3 + 0x40, 0.5f, 0, 0, 0) == 0)
                        break;
                }
            } else {
                at.x += rf(-10, 10); at.y += rf(-10, 10); at.z += rf(3, 10);
            }
            at.w = 0.0f;
            ee_ref<Vec4>(T3) = at;
            ee_ref<Vec4>(T3 + 0x10) = vel;
        };

        // class 248, the grenade: in flight (the touch test, its thrower before / after the first bounce, the smoke trail, the
        // bounce off the floor with its flags' bit 0 set or clear, the slow bounce to rest, the path test's misses and the fuse
        // at 1), at rest (the fuse at 60 / 40 / 20 frames from its end, at 1), the explosion 0x62 and the burst 0x63 among the
        // mobys and Ratchet, the state 0x37 that skips the touch test, 0 (nothing)
        if (h3_grenade == 0) std::printf("%-26s no grenade (class 248) could be spawned\n", "class248_grenade");
        auto grenade_prep = [frame50, h3_place, h3_any, h3_grenadiers](uint32_t m, int st, int pass) {
            frame50();
            const int where = pass;
            uint32_t near = 0;
            h3_place(st == 2 ? 0 : where, st == 2 || ri(0, 2) == 0, near, 0.16f);
            if (st == 2) { wf(T3 + 0x10, 0.0f); wf(T3 + 0x14, 0.0f); wf(T3 + 0x18, 0.0f); }  // (at rest: no velocity)
            const uint32_t thrower = near != 0 && coin() ? near : h3_any(h3_grenadiers);
            if (grenade_spawn_l8(thrower, T3, T3 + 0x10) != m) return;
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            // as class 333's throw sets it up after the spawn: the fuse 180, the pulse's halves +0xc / +0xe, state 1, flags 3
            w32(pv + 0x20, u(frame_scale_i(0xb4)));
            w16(pv + 0xc, u(frame_scale_i(0xa)));
            w16(pv + 0xe, u(frame_scale_i(0xa)));
            w8(m + 0xbc, ee_read<uint8_t>(m + 0xbc) | 3u);
            if (coin()) w8(m + 0xbc, (ee_read<uint8_t>(m + 0xbc) & ~3u) | u(ri(0, 3)));  // (bit 0: bounces; bit 1: the burst)
            w32(pv + 0x24, coin() ? 0u : u(ri(1, 4)));
            if (st == 2) {
                static const int32_t at[] = {0x3c, 0x28, 0x14};
                const int r = ri(0, 5);
                w32(pv + 0x20, u(r < 3 ? frame_scale_i(at[r]) + 1 : r == 3 ? 1 : r == 4 ? 2 : ri(3, frame_scale_i(0x78))));
                wf(pv + 0x28, 254.0f / static_cast<float>(frame_scale_i(0x78)));
                w32(pv + 0x24, u(ri(1, 6)));
            } else {
                const int r = where == 3 && coin() ? 0 : ri(0, 4);  // (in the air: the fuse at 1 in half the cases, its end on a miss)
                w32(pv + 0x20, u(r == 0 ? 1 : r == 1 ? 2 : ri(3, frame_scale_i(0xb4))));
            }
            w8(m + 0x20, u(st));
            seed_rand();
        };
        if (h3_grenade != 0) {
            static const int g_states[] = {1, 2, 0x37, 0x62, 0x63, 0};
            for (const int st : g_states) {
                // (the ignores: the collision queries' triangle record lanes for every state that tests (all but 0x37), and the
                // blasts' spawns -- explosion_effect's / explosion_burst's debris and particles -- that every state but 0x37 can
                // reach in the same frame: the touch test sets the blast off at once)
                ignores(st != 0x37, st != 0x37, st != 0x37);
                // (and the collision routines' record OV(0x173e40) .. 0x173ebc past the hit point and normal the grenade reads:
                // the known engine-port difference of the VU0 leftovers in the collision records (COMMON.md), which nothing
                // reads; met at L8 0x17467c and 0x1746a8 .. 0x1746b8 once the path test runs)
                if (st != 0x37)
                    for (uint32_t a = OV(0x00173e40); a < OV(0x00173ec0); a += 4)
                        if (a != OV(0x00173e60) && a != OV(0x00173e64) && a != OV(0x00173e68) && a != OV(0x00173e80) && a != OV(0x00173e84) && a != OV(0x00173e88))
                            g_ignored_words.push_back(a);
                for (int pass = 0; pass < 4; ++pass)
                    classcheck("class248_grenade", 0x002d23f0, bat_class248_update_l8, [st, pass, grenade_prep](uint32_t m) { grenade_prep(m, st, pass); }, 12, {h3_grenade});
            }
        }

        // class 425, the Bomber Tank's shell: in flight (the touch test: its owner, with +0x14 set or not; the path's hit with no
        // owner, on its owner, on the floor or another moby; the miss with the fuse at 1), the blast 0x63 (its 50 particles), 0
        // and 2 (nothing)
        if (h3_shell == 0) std::printf("%-26s no shell (class 425) could be spawned\n", "class425_shell");
        auto shell_prep = [frame50, h3_place, h3_any, h3_tanks](uint32_t m, int st, int pass) {
            frame50();
            const int where = pass;
            uint32_t near = 0;
            h3_place(where, false, near, 0.0f);
            const int r = ri(0, 4);
            const uint32_t owner = r == 0 ? 0u : (r == 1 && near != 0) ? near : h3_any(h3_tanks);
            if (bomber_tank_bomb_l8(owner, T3, T3 + 0x10) != m) return;
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 3) == 0) w32(pv + 0x14, 1);
            const int f = where == 3 && coin() ? 0 : ri(0, 4);  // (in the air: the fuse at 1 in half the cases)
            w32(pv + 0x10, u(f == 0 ? 1 : f == 1 ? 2 : ri(3, frame_scale_i(0x12c))));
            w8(m + 0x20, u(st));
            seed_rand();
        };
        if (h3_shell != 0) {
            static const int s_states[] = {1, 0x63, 0, 2};
            for (const int st : s_states) {
                // (the ignores: the path test's triangle record lanes in flight; the particle pool's velocity w lane, the stale
                // VU0 lane vec_from_spherical leaves in the blast's fourth kind, which every state reaches through the touch test)
                ignores(st == 1, false, true);
                for (int pass = 0; pass < 4; ++pass)
                    classcheck("class425_shell", 0x002dc9a8, bat_class425_update_l8, [st, pass, shell_prep](uint32_t m) { shell_prep(m, st, pass); }, 12, {h3_shell});
            }
        }

        // class 327, the sea: the wave table built (its flag at 0) or not, Ratchet crossing the surface downward this frame
        // (his sound slot none, free, his and playing or not, another moby's), deeper and sinking slowly, above it
        // (the ignores: the particle pool's velocity w lane: the drops' velocity w is a stack word the original never writes)
        level();
        ignores(false, false, true);
        auto sea_prep = [frame50, sound_slot, h3_hero, P](uint32_t m, int pass) {
            (void)m;
            frame50();
            w32(L8_ADDR(0x00161970), coin() ? 0u : 1u);
            const float sea = ee_read<float>(L8_ADDR(0x001d7068));
            const int r = pass == 0 ? 0 : ri(0, 3);
            float z = 0.0f, vz = 0.0f;
            if (r == 0 || r == 3) { const float depth = rf(0.0f, 0.6f); z = sea - depth; vz = -(depth + rf(0.0001f, 0.5f)); }
            else if (r == 1) { z = sea - rf(0.5f, 3.0f); vz = -rf(0.0f, 0.4f); }
            else { z = sea + rf(0.0f, 5.0f); vz = rf(-1.0f, 1.0f); }
            wf(P + 0x88, z);
            wf(P + 0x108, vz);
            w16(0x00141602, sound_slot(h3_hero) & 0xffffu);
            seed_rand();
        };
        for (int pass = 0; pass < 3; ++pass)
            classcheck("class327_sea", 0x002da0f0, bat_class327_update_l8, [pass, sea_prep](uint32_t m) { sea_prep(m, pass); }, 20);
        level();
        ignores(false, false, false);
        //@@P81_HELPERS
        g_extra_noop_hooks.clear();
        g_ignored_words = saved_ignored;
    }

    //@@NEXT
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
