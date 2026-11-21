// Oracle batch 260 (levels 17 / 18 port, 0.3.0): Drek's fleet's (level 17, code name Veldin Orbit) own code on its real level data. Run with
// ORACLE_LEVEL=17 build/tests/ovl_oracle --batch 260 (needs the ISO; no RAM dump). The subjects are the level's own mobys of each class
// (placed by its gameplay data) after the first gameplay frame, perturbed; the original is level 17's own function at its own address, the
// port the one registered for it (the transliterations veldin_orbit_classes_tr_N.cpp): the 59 class updates, the 26 draw callbacks the
// classes queue, sound emitter type 5, camera type 3, class 1382's five hooks, the camera mode hook and its callback, and the movement-mode
// hero update (L17:0x00203290, veldin_orbit_hero_tr_*.cpp). The other hero variants are proved by batch 261. Built after batch 124.

#include "ovl_oracle.h"
#include "trial_guard.h"

#include <game/enhance/advanced.h>
#include <game/enhance/fixes102.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/carry_hooks_levels.h>
#include <game/ovl/hero_move_mode.h>
#include <game/ovl/veldin_orbit_mobys.h>
#include <game/ovl/veldin_orbit_own.h>
#include <game/ovl/variants_l17.h>
#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/novalis_camera.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/shared_class1021.h>
#include <game/ovl/shared_class1038.h>
#include <game/ovl/shared_class1108.h>
#include <game/ovl/shared_class1135.h>
#include <game/ovl/shared_class1224.h>
#include <game/ovl/shared_class123_draw.h>
#include <game/ovl/shared_class123.h>
#include <game/ovl/shared_class1319.h>
#include <game/ovl/shared_class1378.h>
#include <game/ovl/shared_class156.h>
#include <game/ovl/shared_class1952.h>
#include <game/ovl/shared_class196.h>
#include <game/ovl/shared_class224.h>
#include <game/ovl/shared_class258.h>
#include <game/ovl/shared_class295.h>
#include <game/ovl/shared_class408.h>
#include <game/ovl/shared_class408_tint.h>
#include <game/ovl/shared_class44.h>
#include <game/ovl/shared_class49.h>
#include <game/ovl/shared_class567.h>
#include <game/ovl/shared_class655_draw.h>
#include <game/ovl/shared_class655.h>
#include <game/ovl/shared_class77.h>
#include <game/ovl/shared_class78_draw.h>
#include <game/ovl/shared_class78.h>
#include <game/ovl/shared_class819.h>
#include <game/ovl/shared_class830.h>
#include <game/ovl/shared_class934.h>
#include <game/ovl/shared_envmap_meshes.h>
#include <game/ovl/shared_floor_switch.h>
#include <game/ovl/shared_gadget_pickups.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/shared_ripples.h>
#include <game/ovl/shared_ship_shots.h>
#include <game/ovl/shared_swingshot_target.h>
#include <game/ovl/shared_three_glows.h>
#include <game/ovl/shared_waves.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sound_emitter.h>
#include <game/lib/libcdvd.h>
#include <game/ovl/empty_class_updates.h>
#include <runtime/kernel/disc.h>

#include <optional>
#include <cmath>
#include <cstring>
#include <string>
#include <map>
#include <unordered_set>
#include <algorithm>

namespace rac1::oracle {

namespace {
// The checks named here reach sphere_collide or capsule_collide, whose chunk buffers, unpacked vertices and cell
// walk the port keeps on the host stack (the original: 0x70000000 .. 0x70002000, 0x70002000, 0x70003800): A2's
// collision scratch ranges are left out of these checks, and only of them (stage 5, task C2b).
bool c2b_spr_check(const char* name) {
    static const char* const kNames[] = {"class1379", "class1382", "class1843", "class44", "class835", "hero_move_mode",
                                         "class1134", "class1135", "class123", "class1448", "class408", "class805",
                                         "class99", "p17_class99", "p17_class347", "p17_class347_break", "p17_class1368", "p17_class669",
                                         "p17_class835", "p17_class1379", "p17_class1379_hits", "p17_2ec360", "p17_class1382", "p17_class1382_hits",
                                         "p17_2efa48", "p17_2efd48", "p17_class1448", "p17_2f2180",
                                         "p17_class1382_close", "p17_class1382_carry"};
    for (const char* n : kNames)
        if (std::strcmp(n, name) == 0) return true;
    return false;
}

// a hand-written class hook (moby, position, target, float) in the hook table's uniform form (as batch 119's): Drek's
// fleet's three hooks that drop what is carried (carry hooks +0 / +4 / +8: carry_drop_hook, shared_little.cpp)
template <int32_t (*H)(uint32_t, uint32_t, uint32_t, float)>
TrRet hook_uniform(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float f12, float, float, float,
                   float, float, float, float) {
    return TrRet{static_cast<uint32_t>(H(a0, a1, a2, f12)), 0.0f};
}
int32_t drop_hook0(uint32_t m, uint32_t a1, uint32_t a2, float f12) { return carry_drop_hook(m, a1, a2, f12, 0); }  // L17:0x2efea8
int32_t drop_hook4(uint32_t m, uint32_t a1, uint32_t a2, float f12) { return carry_drop_hook(m, a1, a2, f12, 4); }  // L17:0x2eff30
int32_t drop_hook8(uint32_t m, uint32_t a1, uint32_t a2, float f12) { return carry_drop_hook(m, a1, a2, f12, 8); }  // L17:0x2effb8
int32_t pvars60_hook(uint32_t m, uint32_t, uint32_t, float) { return static_cast<int32_t>(class_hook_pvars60_l6(m)); }  // L17:0x2f0070 (Blarg's L6:0x2ea800, the same code)

// A random state can hold a pointer outside RAM that the port's vector helpers dereference without a check (a SIGSEGV where the interpreter would wrap it): the trial run of a state
// turns it into "not a game state" (the state is dealt again), as the port's "Pointer resolution out of bounds" exception does.

// A check whose original is level 17's own function at `l9addr` (not a Veldin address to translate).
void l17check(const char* name, uint32_t l9addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (const char* skip = std::getenv("B260_SKIP"); skip != nullptr && (std::string(",") + skip + ",").find(std::string(",") + name + ",") != std::string::npos) return;  // (the mutation test: checks already seen to fail)
    if (g_oracle_level != 17) {
        std::printf("%-26s L17:0x%08x  skipped (level 17 only: run with ORACLE_LEVEL=17)\n", name, l9addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L17:0x%08x\n", name, l9addr);
    std::optional<CollisionScratchIgnored> spr_ignored;
    if (c2b_spr_check(name)) spr_ignored.emplace();
    if (!check_fn(name, l9addr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_260() {
    if (g_only_batch != 260) return;
    g_batch = 260;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    // segment_collide's hit point (the global at Veldin 0x174610, 3 words): written by the callee from the segment's end points; class 911's
    // state 1 builds the segment from stale stack lanes (the port's EE stack and the interpreter's hold other leftovers), nothing reads it
    for (uint32_t k = 0; k < 12; k += 4) g_ignored_words.push_back(OV(0x00174610) + k);
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // the console clock / the disc reads the originals poll (IOP calls: the Gold Bolt's autosave, the streamed sounds): the host's on both
    // sides, as batches 138 / 141 do
    const std::time_t saved_clock = g_host_clock_fixed;
    g_host_clock_fixed = 1759658400;
    const auto clock_hooks = decltype(g_extra_hooks){{0x00121780, [](EeCpu& c) { c.set32(2, cd_read_clock(c.r32(4))); }},
                                                     {0x0012d6d8, [](EeCpu& c) { clock_to_local(c.r32(4)); }},
                                                     {0x00121450, [](EeCpu& c) { c.set32(2, cd_read(c.r32(4), c.r32(5), c.r32(6), c.r32(7))); }}};
    g_extra_hooks = clock_hooks;
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
            w32(0x0013f2e8, 0xffffffffu);  // (the sound listener's cached points refreshed on the first frame, as in the game: task 0323's level 18 frame)
            gameplay_frame();
            snap.assign(ram, ram + 0x02000000);
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    // the level's mobys whose update function is `fn` (level 17's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60, std::vector<uint32_t> subjects = {}) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 17) { l17check(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
        level();
        const std::vector<uint32_t> ms = subjects.empty() ? mobys_with(fn) : subjects;
        if (ms.empty()) { std::printf("%-26s L17:0x%08x  no moby on this level\n", name, fn); return; }
        l17check(name, fn, n, Ret::None, [level, ms, vary, port] {
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
                ok = run_trial(port, m);
                ee_stack_ptr() = stack_top;  // (an exception leaves the port's frames allocated)
                std::memcpy(ram, saved.data(), saved.size());
                if (ok) break;
            }
            return Args{{m}, {}};
        }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
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
    if (g_oracle_level == 17) {
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
        {  // the particle pool (0x800 records of 0x40 bytes at *(0x16016c)): the w lane of the velocity vector a spray copies (a stale lane of vec_from_spherical in the original)
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c);
        }
    }

    // ---- the level's own classes (transliterated by tools/codegen/mkall_l17.py): the level's own mobys of each class, the first
    // update (state 0) done by the port, then a state, a hit, the hero near, the pvars' floats / counters moved as typed
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};  // (+ the level change request: the original would run the CD / movie path)
    struct TrCls { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t skip_states = 0; bool walk = false; uint32_t skip_p0 = 0; uint32_t skip_p1 = 0; };  // (skip_p0 / skip_p1: states skipped in pass 0 / 1 only, see the batch's notes)  // (the states that start a scene / a level change: the original runs the kernel / movie path for real)
    static const TrCls trcls[] = {
        {"class44", 0x0029f8d8, class44_update, 0x0, false},
        {"class49", 0x002a2b30, class49_update, 0x0, false},
        {"class77", 0x002a7b58, class77_update, 0x0, false},
        {"class78", 0x002a82c0, class78_update, 0x0, false},
        {"class99", 0x002a8da0, vo_class99_update_l17, 0x0, false},
        {"class123", 0x002ac740, class123_update, 0x0, false},
        {"class156", 0x002af608, class156_update, 0x0, false},
        {"class197", 0x002c0f00, class196_update, 0x0, false},
        {"class224", 0x002c53d8, class224_update, 0x0, false},
        {"class258", 0x002c7cb0, class258_update, 0x0, false},
        {"class295", 0x002c9ce8, class295_update, 0x0, false},
        {"class347", 0x002cb310, vo_class347_update_l17, 0x0, false},
        {"class408", 0x002cbe80, class408_update, 0x0, false},
        {"class567", 0x002d2dc0, class567_update, 0x0, false},
        {"class605", 0x002d3da0, zone_marker_update, 0x0, false},
        {"class655", 0x002d4e88, class655_update, 0x0, false},
        {"class669", 0x002d77f0, vo_class669_update_l17, 0x0, false},
        {"class758", 0x002d95a8, swingshot_target_update, 0x0, false},
        {"class805", 0x002d9b10, spawn_point_update_l1, 0x0, false},
        {"class819", 0x002dbaa8, class819_update, 0x0, false},
        {"class830", 0x002dc078, class830_update, 0x0, false},
        {"class835", 0x002dc3f8, vo_class835_update_l17, 0x0, false},
        {"class934", 0x002e1c20, class934_update, 0x0, false},
        {"class1009", 0x002e22b0, class1009_update, 0x0, false},
        {"class1017", 0x002e2978, class1017_update, 0x0, false},
        {"class1021", 0x002e2c58, class1021_update, 0x0, false},
        {"class1034", 0x002e30c8, class1034_update, 0x0, false},
        {"class1038", 0x002e3690, class1038_update, 0x0, false},
        {"class1040", 0x002e3d38, class1040_update, 0x0, false},
        {"class1134", 0x002e3ff8, gold_bolt_update, 0x4, false},
        {"class1135", 0x002e4f30, teleporter_update_l1, 0x0, false},
        {"class1209", 0x002e5f50, floor_switch_update, 0x0, false},
        {"class1224", 0x002e6468, class1224_update, 0x0, false},
        {"class1359", 0x002e87d8, vo_class1359_update_l17, 0x0, false},
        {"class1368", 0x002e8e08, vo_class1368_update_l17, 0x0, false},
        {"class1378", 0x002e9608, class1378_update, 0x0, false},
        {"class1379", 0x002ed018, vo_class1379_update_l17, 0x0, false},
        {"class1380", 0x002ee2b0, vo_class1380_update_l17, 0x0, false},
        {"class1382", 0x002eeb68, vo_class1382_update_l17, 0x0, false},
        {"class1405", 0x002f08e0, class1405_update, 0x0, false},
        {"class1406", 0x002f0ca0, class1406_update, 0x0, false},
        {"class1407", 0x002f1060, class1407_update, 0x0, false},
        {"class1426", 0x002f1418, class1426_update, 0x0, false},
        {"class1428", 0x002f1790, class1428_update, 0x4, false},
        {"class1448", 0x002f1940, vo_class1448_update_l17, 0x0, false},
        {"class1470", 0x002f26d0, vo_class1470_update_l17, 0x0, false},
        {"class1562", 0x002f2e78, vo_class1562_update_l17, 0x0, false},
        {"class1772", 0x002f3848, vo_class1772_update_l17, 0x0, false},
        {"class1843", 0x002f40d8, vo_class1843_update_l17, 0x0, false},
        {"class1873", 0x002f5260, vo_class1873_update_l17, 0x0, false},
        {"class1874", 0x002f5388, empty_update, 0x0, false},
        {"class1876", 0x002f5390, vo_class1876_update_l17, 0x0, false},
        {"class1877", 0x002f54d0, empty_update, 0x0, false},
        {"class1907", 0x002f57a0, class1907_update, 0x0, false},
        {"class1908", 0x002f5ab8, class1908_update, 0x0, false},
        {"class1910", 0x002f5dd0, class1910_update, 0x0, false},
        {"class1911", 0x002f60e8, class1911_update, 0x0, false},
        {"class1913", 0x002f6400, class1913_update, 0x0, false},
        {"class1918", 0x002f6718, class1918_update, 0x0, false},
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
        // (class 123's state 2 turns by 360 / (pvars +0x24 x 2 pi / +0x30) a frame: the level's Spiderbots hold 0 at +0x24 until something gives them a
        // speed (the class never writes it), and the division by zero sends the original's angle_wrap loop past the interpreter's budget: a speed given)
        if (fn == class123_update && st == 2 && ee_read<uint32_t>(pv + 0x24) == 0) wf(pv + 0x24, rf(0.5f, 4.0f));
        w32(OV(0x0015f5cc), R() & 0xffff);
        seed_rand();
    };
    if (g_oracle_level == 17 && std::getenv("B260_INFO") != nullptr) {  // the level's mobys of each class: how many, the nearest to the hero's start
        level();
        const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
        std::printf("hero start %.1f %.1f %.1f\n", h.x, h.y, h.z);
        for (const TrCls& c : trcls) {
            const std::vector<uint32_t> ms = mobys_with(c.addr);
            float best = 1e9f;
            for (uint32_t m : ms) { const Vec4 q = ee_ref<Vec4>(m + 0x10); best = std::fmin(best, std::sqrt((q.x - h.x) * (q.x - h.x) + (q.y - h.y) * (q.y - h.y) + (q.z - h.z) * (q.z - h.z))); }
            std::printf("INFO %s 0x%08x instances %zu nearest %.0f\n", c.name, c.addr, ms.size(), ms.empty() ? -1.0f : best);
            if (std::getenv("B260_PV") != nullptr && std::strcmp(std::getenv("B260_PV"), c.name) == 0)
                for (uint32_t m : ms) {
                    std::printf("  moby %08x state %u pvars:", m, ee_read<uint8_t>(m + 0x20));
                    for (uint32_t o = 0; o < 0x40; o += 4) std::printf(" %08x", ee_read<uint32_t>(ee_read<uint32_t>(m + 0x78) + o));
                    std::printf("\n");
                }
        }
    }
    for (const TrCls& c : trcls) {
        if (std::getenv("B260_CLS") != nullptr && std::strcmp(std::getenv("B260_CLS"), c.name) != 0) continue;
        for (int st = 0; st < 16; ++st)
        for (int pass = 0; pass < 3; ++pass) {
        if (g_oracle_level == 17 && std::getenv("B260_TAGS") != nullptr) std::printf("@@ %s st %d pass %d\n", c.name, st, pass);
        if ((std::getenv("B260_ST") == nullptr || std::atoi(std::getenv("B260_ST")) == st) && (std::getenv("B260_PASS") == nullptr || std::atoi(std::getenv("B260_PASS")) == pass) && (c.skip_states & (1u << st)) == 0 && !(pass == 0 && (c.skip_p0 & (1u << st)) != 0) && !(pass == 1 && (c.skip_p1 & (1u << st)) != 0)) classcheck(c.name, c.addr, c.fn, [st, pass, c, generic_prep](uint32_t m) { generic_prep(m, st, pass, c.fn, c.walk); }, 12);
        }
    }
    g_extra_noop_hooks.clear();
    if (std::getenv("B260_CLASSES_ONLY") != nullptr) { g_ignored_words = saved_ignored; g_ee_null_is_ram = null_ram; g_extra_hooks.clear(); g_host_clock_fixed = saved_clock; return; }  // (a quick run of the class checks alone)

    // ---- level 17's own moby callbacks (draw callbacks installed by `jal 0x1ecc00 / 0x1eca48`): a0 = the moby; the subjects are
    // the mobys of the class whose code installs it, after one update of the class in a state, a hit, the hero near
    if (g_oracle_level == 17) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        struct Cb { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t cls_addr; int cls; };
        // the display packet cursor (*0x1611c0) is null in the unit batches' memory: the draw helpers' packet writes land in low memory (0x100 .. 0x1500; the cursor is given a real buffer below) and carry a stale
        // callee-saved register (s7) of the original's frame: not game state
        const size_t ign_before = g_ignored_words.size();
        for (uint32_t a = 0x100; a < 0x1500; a += 4) g_ignored_words.push_back(a);
        static const Cb cbs[] = {
        {"cb2a2aa8", 0x002a2aa8, class44_glows_draw, 0x0029f8d8, 44},
        {"cb2a8088", 0x002a8088, class77_draw, 0x002a7b58, 77},
        {"cb2a8808", 0x002a8808, class78_draw, 0x002a82c0, 78},
        {"cb2a95b8", 0x002a95b8, vo_cb_2a95b8_l17, 0x002a8da0, 99},
        {"cb2acf00", 0x002acf00, class123_draw, 0x002ac740, 123},
        {"cb2cc740", 0x002cc740, class408_tint, 0x002cbe80, 408},
        {"cb2d5e38", 0x002d5e38, class1108_draw, 0x002d4e88, 655},
        {"cb2d6d88", 0x002d6d88, class655_draw, 0x002d4e88, 655},
        {"cb2d7cf0", 0x002d7cf0, vo_cb_2d7cf0_l17, 0x002d77f0, 669},
        {"cb2e5848", 0x002e5848, class1135_draw, 0x002e4f30, 1135},
        {"cb2e6e40", 0x002e6e40, class1224_draw, 0x002e6468, 1224},
        {"cb2e9798", 0x002e9798, class1378_draw, 0x002e9608, 1378},
        {"cb2eb9a8", 0x002eb9a8, vo_cb_2eb9a8_l17, 0x002ed018, 1379},
        {"cb2f0448", 0x002f0448, class1382_glows_draw, 0x002eeb68, 1382},
        {"cb2f04d0", 0x002f04d0, ripple_rings_draw, 0x002f08e0, 1405},
        {"cb2f0948", 0x002f0948, fleet_waves_draw_a, 0x002f0ca0, 1406},
        {"cb2f0d08", 0x002f0d08, fleet_waves_draw_b, 0x002f1060, 1407},
        {"cb2f10c8", 0x002f10c8, ripple_pool_draw, 0x002f1418, 1426},
        {"cb2f32f0", 0x002f32f0, vo_cb_2f32f0_l17, 0x002f3848, 1772},
        {"cb2f3b68", 0x002f3b68, class1319_trails_draw, 0x002f40d8, 1843},
        {"cb2f5690", 0x002f5690, class1907_draw, 0x002f57a0, 1907},
        {"cb2f59a8", 0x002f59a8, class1908_draw, 0x002f5ab8, 1908},
        {"cb2f5cc0", 0x002f5cc0, class1910_draw, 0x002f5dd0, 1910},
        {"cb2f5fd8", 0x002f5fd8, class1911_draw, 0x002f60e8, 1911},
        {"cb2f62f0", 0x002f62f0, class1913_draw, 0x002f6400, 1913},
        {"cb2f6608", 0x002f6608, class1918_draw, 0x002f6718, 1918},
        };
        for (const Cb& cb : cbs) {
            if (std::getenv("B260_CLS") != nullptr && std::strcmp(std::getenv("B260_CLS"), cb.name) != 0) continue;
            level();
            std::vector<uint32_t> subj;
            if (cb.cls_addr != 0) subj = mobys_with(cb.cls_addr);
            else for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) if (ee_read<uint8_t>(m + 0x20) < 0xfe) subj.push_back(m);
            void (*clsfn)(uint32_t) = nullptr;
            for (const TrCls& c : trcls) if (c.addr == cb.cls_addr) clsfn = c.fn;
            if (subj.empty()) { std::printf("%-26s L17:0x%08x  no moby on this level\n", cb.name, cb.addr); continue; }
            // cb2fd6c0 / cb311d50: their GIF packet words carry a callee-saved register (s7 / s?) the callback reads on a path that never set it (the original's interpreter holds the
            // harness' leftover there, the port's local 0): the packets (the cursor's buffer) are not compared for these two, everything else is
            const size_t ign_pk = g_ignored_words.size();
            for (int st = 0; st < 4; ++st)
            for (int pass = 0; pass < 3; ++pass) {
                if (clsfn == nullptr && (st != 0 || pass != 0)) continue;
                classcheck(cb.name, cb.addr, cb.fn, [st, pass, clsfn, generic_prep](uint32_t m) { generic_prep(m, st, pass, clsfn != nullptr ? clsfn : class44_update, false); w32(OV(0x001611c0), 0x00800000u); }, 12, subj);  // (the display packet cursor: a real buffer for the draw callbacks)
            }
            g_ignored_words.resize(ign_pk);
        }
        g_ignored_words.resize(ign_before);
        g_extra_noop_hooks.clear();
    }

    // ---- level 17's sound emitter type 3 (section 5 of the overlay) on the level's own emitters
    if (g_oracle_level == 17) {
        struct Em { const char* name; uint32_t addr; void (*fn)(uint32_t); };
        static const Em ems_t[] = {{"emitter5", 0x00300e70, emitter_camera_bed_update}};
        for (const Em& e : ems_t) {
            level();
            std::vector<uint32_t> ems;
            for (int32_t i = 0; i < ee_read<int32_t>(0x0013f2e0); ++i) {
                const uint32_t em = ee_read<uint32_t>(0x0013f2e4) + static_cast<uint32_t>(i) * 0x90;
                if (ee_read<uint32_t>(em + 4) == e.addr) ems.push_back(em);
            }
            if (ems.empty()) { std::printf("%-26s L17:0x%08x  no emitter of the type on this level\n", e.name, e.addr); continue; }
            if (std::getenv("B260_INFO") != nullptr) std::printf("INFO %s 0x%08x emitters %zu\n", e.name, e.addr, ems.size());
            auto fn = e.fn;
            l17check(e.name, e.addr, 120, Ret::None, [level, ems] {
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


    // ---- level 17's class hooks (class 827's table at 0x1dde58): a0 = the moby, a1 = a position, a2 = a target moby, f12 a float
    // (the carry hooks' shape: their callees read all four), on the mobys whose class owns the hook
    if (g_oracle_level == 17) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        struct Hk { const char* name; uint32_t addr; uint32_t slot; TrRet (*fn)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float, float, float, float, float); };
        static const Hk hks[] = {
            {"hook_2efea8", 0x2efea8, 0x0, hook_uniform<drop_hook0>},
            {"hook_2eff30", 0x2eff30, 0x4, hook_uniform<drop_hook4>},
            {"hook_2effb8", 0x2effb8, 0x8, hook_uniform<drop_hook8>},
            {"hook_2f0040", 0x2f0040, 0xc, hook_uniform<vo_carry_hookc_l17>},
            {"hook_2f0070", 0x2f0070, 0x10, hook_uniform<pvars60_hook>},
        };
        for (const Hk& hk : hks) {
            level();
            std::vector<uint32_t> own;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
                const uint32_t cls = ee_read<uint32_t>(m + 0x24);
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + hk.slot) == hk.addr) own.push_back(m);
            }
            if (own.empty()) { std::printf("%-26s L17:0x%08x  no moby owns it on this level\n", hk.name, hk.addr); continue; }
            if (std::getenv("B260_INFO") != nullptr) std::printf("INFO %s 0x%08x owners %zu\n", hk.name, hk.addr, own.size());
            auto fn = hk.fn;
            l17check(hk.name, hk.addr, 40, Ret::Int32, [level, own] {
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

    // ---- level 17's movement-mode hero update (L17:0x00203290, 0x8d0 bytes as Blarg's): the hero in a movement mode (PLAYER+0x20a4 = 1 / 2 / 3) in one of the states of the mode
    if (g_oracle_level == 17) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        g_ignored_words.push_back(OV(0x0017431c));  // (a float whose zero's sign depends on a VU0 register left behind)
        g_ignored_words.push_back(OV(0x00173e9c));  // (the same class: the segment record's word (Veldin 0x1efc70's sqc2 into 0x173e90..) is -0 or +0 by the VU0 vf register the earlier checks left; passes alone, fails after the class checks)
        l17check("hero_move_mode", 0x00203290, 300, Ret::None, [level] {
            level();
            const uint32_t P = 0x0013f350;
            const int mode = ri(1, 3);
            w8(P + 0x20a4, u(mode));
            static const uint32_t st1[] = {0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f};
            static const uint32_t st2[] = {0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62};
            static const uint32_t st3[] = {0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x52};
            w32(P + 0x2090, mode == 1 ? st1[ri(0, 12)] : mode == 2 ? st2[ri(0, 8)] : st3[ri(0, 7)]);
            if (ri(0, 2) == 0) { Vec4 q = ee_ref<Vec4>(P + 0x80); q.x += rf(-3, 3); q.y += rf(-3, 3); q.z += rf(-2, 2); ee_ref<Vec4>(P + 0x80) = q; }
            if (ri(0, 2) == 0) wfrand(P + 0x90, 3, -2, 2);
            w32(OV(0x0015f5cc), R() & 0xffff);
            seed_rand();
            return Args{};
        }, [](A) { hero_move_mode(); return uint64_t{0}; });
        g_ignored_words.pop_back();
        g_ignored_words.pop_back();
        g_extra_noop_hooks.clear();
    }

    // ---- moby_spawn (the Veldin port) against level 17's own copy (the map's equivalent): the classes' spawns all go through it
    if (g_oracle_level == 17) {
        check("moby_spawn", 0x0024e838, 60, Ret::Int32, [level] {
            level();
            static const uint32_t cls[] = {0x2c, 0x31, 0x4d, 0x63, 0x7b, 0x566, 0x6ec};  // (classes of level 17's own table: 44, 49, 77, 99, 123, 1382, 1772)
            return Args{{cls[ri(0, 6)]}, {}};
        }, [](A a) { return r32(moby_spawn(a.i[0])); });
    }

    // ---- level 17's camera hook (0x2fff38, the pointer its setter 0x2fff00 puts at 0x167910) and the callback it queues (0x2fff98)
    if (g_oracle_level == 17) {
        auto cam_prep = [level] {
            level();
            const uint32_t S = 0x00167900;  // (level 17's hook block: its setter 0x2fff00 writes S .. S + 0x10)
            w32(S, ri(0, 3) == 0 ? R() : 0u);
            w32(S + 4, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
            wf(S + 8, rf(-3, 3));
            wf(S + 0xc, ri(0, 3) == 0 ? 0.0f : rf(-0.5f, 3));
            w32(S + 0x10, 0x002fff38u);
            wf(0x0016267c, rf(0, 0.5f));  // $gp - 0x4584: the frame step the callback counts down by
            w32(OV(0x0015ef4c), u(ri(0, 14)));  // the queue's count (it holds up to 16)
        };
        l17check("cam0_hook", 0x002fff38, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_hook(0); return uint64_t{0}; });
        l17check("cam_callback", 0x002fff98, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_callback(0); return uint64_t{0}; });
    }

    // ---- level 17's camera type 3 (accept / init / update / exit: section 4 of the overlay), as batch 120 checks level 8's: a live camera of the type
    // (or, with none on the level at its start, another live camera's words), the hero put in or near its box, its data varied
    if (g_oracle_level == 17) {
        auto cam_setup = [level](int type, bool with_other, uint32_t* other) -> uint32_t {
            level();
            uint32_t cam = 0;
            for (uint32_t i = 0; i < 48 && cam == 0; ++i)
                if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0 && ee_read<int16_t>(OV(0x00167150) + i * 0xa0 + 0x8c) == type) cam = OV(0x00167150) + i * 0xa0;
            for (uint32_t i = 0; i < 48 && cam == 0; ++i)
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
        level();
        bool any = false, own = false;
        for (uint32_t i = 0; i < 48; ++i)
            if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0) { any = true; if (ee_read<int16_t>(OV(0x00167150) + i * 0xa0 + 0x8c) == 3) own = true; }
        if (!any) std::printf("cam_type3                 no live camera on this level\n");
        else {
            if (!own) std::printf("cam_type3                 no live camera of the type on this level: checked on another live camera's words\n");
            l17check("cam_type3_accept", 0x002fd208, 300, Ret::Int32, [cam_setup] { uint32_t other = 0; const uint32_t cam = cam_setup(3, true, &other); return Args{{cam, other}, {}}; },
                     [](A a) { return r32(a.i[0] != 0 ? static_cast<uint32_t>(zero_stub()) : 0u); });
            l17check("cam_type3_init", 0x002fd210, 100, Ret::None, [cam_setup] { return Args{{cam_setup(3, false, nullptr)}, {}}; },
                     [](A a) { if (a.i[0] != 0) cam_type3_init(a.i[0]); return uint64_t{0}; });
            l17check("cam_type3_update", 0x002fc788, 100, Ret::None, [cam_setup] { return Args{{cam_setup(3, false, nullptr)}, {}}; },
                     [](A a) { if (a.i[0] != 0) cam_type3_update(a.i[0]); return uint64_t{0}; });
            l17check("cam_type3_exit", 0x002fd460, 100, Ret::None, [cam_setup] { return Args{{cam_setup(3, false, nullptr)}, {}}; },
                     [](A a) { if (a.i[0] != 0) cam_type3_exit(a.i[0]); return uint64_t{0}; });
        }
    }

    // T00: the level's bare `jr ra` functions its classes and cameras reach through a pointer (class updates, camera
    // callbacks; the scan pairs them with Veldin's 0x1f6bf8): empty_update runs in their places, each against the original
    // called directly (the level may have no moby of the class)
    if (g_oracle_level == 17) {
        static const uint32_t empties[] = {0x002f5388, 0x002f54d0};
        for (const uint32_t a : empties) {
            char nm[24];
            std::snprintf(nm, sizeof nm, "empty_%x", a);
            l17check(nm, a, 20, Ret::None, [level] {
                level();
                return Args{{R(), R(), R(), R()}, {rf(-9, 9)}};
            }, [](A a) { empty_update(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- P17.1 (stage 4): the hand-written classes 99, 347, 669, 835, 1359, 1368 and their helpers (veldin_orbit_beams.cpp,
    // _cannon.cpp, _hazards.cpp, _doors.cpp), each driven through its states with the hero, the camera, its boxes and its mates
    // placed so that every branch is reached (the camera seeing the subject through render_view_setup, the hero moved with his
    // moby, the frame scale at its 50 Hz value in a quarter of the cases); B260_INFO=1 prints the subjects and the re-dealt cases
    if (g_oracle_level == 17) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        // (the class checks' batch-wide w lanes of the pvar area and the particle pool are not left out here: each check
        // below leaves out only what it needs, saying why)
        const std::vector<uint32_t> class_ignored = g_ignored_words;
        g_ignored_words = saved_ignored;
        const bool lone_w = g_ignore_lone_w_lanes;
        g_ignore_lone_w_lanes = false;  // (no lone w lane is left out either: class 99's draw width pv + 0x4c is one)
        const uint32_t P = 0x0013f350;
        const bool info = std::getenv("B260_INFO") != nullptr;
        static uint32_t p17_subject;
        auto table = [] { return ee_read<uint32_t>(OV(0x0015ffd8)); };
        auto live = [table] {
            std::vector<uint32_t> v;
            for (uint32_t m = table(); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                if (ee_read<uint8_t>(m + 0x20) < 0xfe) v.push_back(m);
            return v;
        };
        auto of_class = [live](int16_t cls) {
            std::vector<uint32_t> v;
            for (uint32_t m : live()) if (ee_read<int16_t>(m + 0xa6) == cls) v.push_back(m);
            return v;
        };
        auto pick = [](const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; };
        auto fifty = [] {
            if (ri(0, 3) != 0) return;
            wf(0x0015ed68, 1.2f); wf(0x0015ed6c, 0.02f); wf(0x0015ed70, 0.0004f);
        };
        auto hero_at = [P](const Vec4& q) {
            ee_ref<Vec4>(P + 0x80) = q;
            const uint32_t hm = ee_read<uint32_t>(P + 0x2080);
            if (hm != 0) ee_ref<Vec4>(hm + 0x10) = q;
        };
        auto jitter = [](Vec4 q, float r, float rz) { q.x += rf(-r, r); q.y += rf(-r, r); q.z += rf(-rz, rz); return q; };
        auto box_centre = [](int32_t b) { return ee_ref<Vec4>(lvd::box(lvd::BOXES, b) + 0x30); };
        // the camera (0x166dc0) moved about `at` until the screen test sees it with radius r, the view matrices made from it
        // (first behind `at` along the camera's forward row 0x166fd0, then anywhere about it)
        // (the screen and projection constants first: the oracle's display leaves them broken, nothing projects; a 640 x 448
        // screen, as batch 506 sets it up)
        auto screen_ready = [] {
            if (ee_read<uint16_t>(0x001518d0) == 0) { w16(0x001518d0, 640); w16(0x001518d2, 448); }
            screen_setup();
            projection_setup();
        };
        auto camera_sees = [screen_ready](const Vec4& at, float r, float spread) {
            screen_ready();
            for (int k = 0; k < 60; ++k) {
                const Vec4 fwd = ee_ref<Vec4>(OV(0x00166fd0));
                const float back = rf(8.0f, 30.0f);
                ee_ref<Vec4>(OV(0x00166dc0)) = k < 30 ? Vec4{at.x - fwd.x * back + rf(-2, 2), at.y - fwd.y * back + rf(-2, 2), at.z - fwd.z * back + rf(-1, 1), 1.0f}
                                                      : Vec4{at.x + rf(-spread, spread), at.y + rf(-spread, spread), at.z + rf(-spread / 4, spread / 4), 1.0f};
                render_view_setup();
                ee_ref<Vec4>(T4) = at;
                if (point_on_screen(T4, r) != -1) return true;
            }
            return false;
        };
        auto camera_away = [screen_ready](const Vec4& at) {
            screen_ready();
            ee_ref<Vec4>(OV(0x00166dc0)) = Vec4{at.x + 400.0f, at.y + 400.0f, at.z + 300.0f, 1.0f};
            render_view_setup();
        };
        auto finish = [] {
            w32(OV(0x0015f5cc), R() & 0xffff);
            seed_rand();
        };
        // a class update on `subjects` with the state `vary` makes; a state that sends the port through a pointer outside RAM is dealt
        // again (counted: B260_INFO prints the re-deals)
        auto pclass = [level, info](const char* name, uint32_t fn, void (*port)(uint32_t), int n, std::function<std::vector<uint32_t>()> subjects,
                                    std::function<bool(uint32_t)> vary) {
            if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
            level();
            const std::vector<uint32_t> ms0 = subjects();
            if (ms0.empty()) { std::printf("%-26s L17:0x%08x  no subject on this level\n", name, fn); return; }
            static int redeals;
            redeals = 0;
            l17check(name, fn, n, Ret::None, [level, subjects, vary, port] {
                static std::vector<uint8_t> saved;
                uint32_t m = 0;
                for (int attempt = 0; attempt < 40; ++attempt) {
                    level();
                    const std::vector<uint32_t> ms = subjects();
                    m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                    p17_subject = 0;
                    if (!vary(m)) continue;
                    if (p17_subject != 0) m = p17_subject;  // (a subject the state made: class 1368's shot)
                    uint8_t* ram = ee_get_ram_base();
                    saved.assign(ram, ram + 0x02000000);
                    const uint32_t stack_top = ee_stack_ptr();
                    const bool ok = run_trial(port, m);
                    ee_stack_ptr() = stack_top;
                    std::memcpy(ram, saved.data(), saved.size());
                    if (ok) break;
                    ++redeals;
                }
                return Args{{m}, {}};
            }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
            if (info) std::printf("INFO %s subjects %zu re-dealt %d\n", name, ms0.size(), redeals);
        };
        // a hit on `m` from a moby of class `cls` (another live moby given that class: only its class halfword is read)
        auto hit_from = [give_hit, live, pick](uint32_t m, uint32_t kinds, int16_t cls) {
            give_hit(m, kinds);
            if (ee_read<uint8_t>(m + 0xa4) == 0xff) return;
            const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
            w32(e + 0x24, kinds | (R() & 0xff));
            const std::vector<uint32_t> all = live();
            uint32_t q = pick(all);
            if (q == m) q = all.front();
            if (ri(0, 4) == 0) { w32(e + 0x20, 0); return; }
            w32(e + 0x20, q);
            w16(q + 0xa6, u(cls));
        };

        if (info) {  // the play-test spots: each placed moby of the hand-written classes (index, uid, position)
            level();
            for (uint32_t m : live()) {
                const int16_t c = ee_read<int16_t>(m + 0xa6);
                if (c == 0x63 || c == 0x15b || c == 0x29d || c == 0x343 || ee_read<uint32_t>(m + 0x74) == 0x002e87d8u) {
                    const Vec4 q = ee_ref<Vec4>(m + 0x10);
                    std::printf("INFO p17 class %d moby %u uid %u at (%.1f, %.1f, %.1f)\n", c, (m - table()) >> 8, ee_read<uint16_t>(m + 0xb2), q.x, q.y, q.z);
                }
            }
        }
        // class 99, the laser beam: its states, its box with the hero in or out, its target (a box / a mate), a follower, the camera
        {
            auto beams = [of_class] { return of_class(0x63); };
            pclass("p17_class99", 0x002a8da0, vo_class99_update_l17, 300, beams, [=](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                static const uint8_t sts[] = {0, 1, 1, 2, 2, 2, 3, 4};
                w8(m + 0x20, sts[ri(0, 7)]);
                const std::vector<uint32_t> others = beams();
                const int32_t nb = lvd::box_count(lvd::BOXES);
                if (ri(0, 4) == 0) w32(pv + 0x40, 0xffffffffu);
                else if (nb > 0 && coin()) w32(pv + 0x40, u(ri(0, nb - 1)));  // (the level's beams have none: a box given, the hero in it or not)
                const int32_t box = ee_read<int32_t>(pv + 0x40);
                switch (ri(0, 3)) {  // the target: a box, a mate, none
                case 0: if (nb > 0) { w32(pv + 0x44, u(ri(0, nb - 1))); w32(pv + 0x68, 0xffffffffu); } break;
                case 1: w32(pv + 0x44, 0xffffffffu); w32(pv + 0x68, (pick(others) - table()) >> 8); break;
                case 2: w32(pv + 0x44, 0xffffffffu); w32(pv + 0x68, 0xffffffffu); break;
                default: break;
                }
                if (ee_read<uint8_t>(m + 0x20) == 2 && ee_read<int32_t>(pv + 0x44) == -1 && ee_read<int32_t>(pv + 0x68) == -1) w32(pv + 0x68, (pick(others) - table()) >> 8);
                wf(pv + 0x50, ri(0, 3) == 0 ? (coin() ? rf(0.95f, 1.0f) : rf(0.0f, 0.05f)) : rf(0.0f, 1.0f));
                wf(pv + 0x54, coin() ? rf(-0.08f, 0.08f) : ee_read<float>(pv + 0x54));
                // (the level's beams' animation has one frame: pv + 0x5c is 0 and the draw width's phase overflows; half the cases
                // get a real animation span so the width's arithmetic is compared)
                if (coin()) { wf(pv + 0x58, rf(0.0f, 2.0f)); wf(pv + 0x5c, rf(0.5f, 3.0f)); }
                w32(pv + 0x60, ri(0, 2) == 0 ? pick(others) : 0u);
                if (ee_read<uint32_t>(pv + 0x60) == m) w32(pv + 0x60, 0);
                const Vec4 at = ee_ref<Vec4>(m + 0x10);
                if (box >= 0 && box < nb && coin()) hero_at(jitter(box_centre(box), 1.0f, 0.5f));
                else if (coin()) {  // across the beam
                    const Vec4 d = ee_ref<Vec4>(m + 0xc0);
                    const float t = rf(0, 13);
                    hero_at(jitter(Vec4{at.x + d.x * t, at.y + d.y * t, at.z + d.z * t, 1.0f}, 0.6f, 0.6f));
                } else hero_at(jitter(at, 30.0f, 5.0f));
                if (ri(0, 2) != 0) camera_sees(at, 32.0f, 12.0f); else camera_away(at);
                fifty();
                finish();
                return true;
            });
        }
        // the spark (particle 0x4f) directly: tied to a beam or free
        l17check("p17_26fb60", 0x0026fb60, 120, Ret::Int32, [level, of_class, pick] {
            level();
            const std::vector<uint32_t> bs = of_class(0x63);
            const uint32_t m = ri(0, 2) == 0 || bs.empty() ? 0u : pick(bs);
            const Vec4 at = m != 0 ? ee_ref<Vec4>(m + 0x10) : Vec4{rf(100, 800), rf(100, 800), rf(100, 400), 1.0f};
            ee_ref<Vec4>(T4) = Vec4{at.x + rf(-5, 5), at.y + rf(-5, 5), at.z + rf(-5, 5), rf(-1, 1)};
            wfrand(T4 + 0x10, 4, -1, 1);
            if (ri(0, 3) == 0) { wf(0x0015ed68, 1.2f); wf(0x0015ed6c, 0.02f); wf(0x0015ed70, 0.0004f); }
            seed_rand();
            return Args{{m, T4, T4 + 0x10}, {}};
        }, [](A a) { return r32(vo_26fb60_l17(a.i[0], a.i[1], a.i[2])); });

        // class 347, the laser cannon (and through it 0x2cbc10's hits, 0x2cbd38, 0x2e94d8, 0x2f4fb8)
        {
            auto cannons = [of_class] { return of_class(0x15b); };
            auto ships = [of_class] { return of_class(0x563); };
            auto fighter_group = [of_class] {  // a group whose list holds a class-1843 fighter (-1: none)
                for (int32_t g = 0; g < lvd::group_count(); ++g) {
                    const uint32_t l = lvd::group(g);
                    if (l == 0) continue;
                    for (uint32_t e = l;; e += 2) {
                        const uint32_t q = ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                        if (ee_read<int16_t>(q + 0xa6) == 0x733) return g;
                        if (ee_read<int16_t>(e) < 0) break;
                    }
                }
                return -1;
            };
            // the hero walking (+0x308) or flying his ship (state 0x32 with a class-1379 moby as the vehicle), or neither
            auto hero_mode = [P, ships, pick](int mode) {
                w16(P + 0x308, mode == 0 ? u(ri(1, 3)) : 0u);
                if (mode == 1) {
                    w32(P + 0x2084, 0x32);
                    const std::vector<uint32_t> s = ships();
                    if (!s.empty() && ri(0, 7) != 0) w32(P + 0x15f0, pick(s));
                    else if (coin()) w32(P + 0x15f0, 0);
                } else if (coin()) w32(P + 0x2084, u(ri(0, 0x40)));
            };
            // the cannon turned toward `target` (yaw and the barrel's pitch from its pivot joint), with `err` radians off
            auto aim_at = [](uint32_t m, const Vec4& target, float err) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                moby_joint_positions(m, 2, L17_ADDR(0x00161980), T4 + 0x40);
                const Vec4 j = ee_ref<Vec4>(T4 + 0x40);
                ee_ref<Vec4>(T4 + 0x60) = Vec4{target.x - j.x, target.y - j.y, target.z - j.z, 0.0f};
                const Vec4 m0 = ee_ref<Vec4>(m + 0x10);
                wf(m + 0x48, angle_wrap(atan2_f(target.x - m0.x, target.y - m0.y) + rf(-err, err)));
                const float pitch = atan2_f(vec2_length(ee_ref<Vec4>(T4 + 0x60)), target.z - j.z);
                wf(pv + 0xd4, -pitch + rf(-err, err));
                wf(pv + 0x78, 0.0f);
            };
            // states 0 .. 3 with a health no dealt damage takes to 0 (`brk` false: every word compared), or the break (state
            // 4, or a low health and a hit: `brk` true, in p17_class347_break)
            auto vary347 = [=](uint32_t m, bool brk) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                const int st = brk ? (coin() ? 4 : ri(1, 3)) : (ri(0, 7) < 1 ? 0 : ri(1, 3));
                w8(m + 0x20, u(st));
                fifty();
                wf(pv + 0x20, brk ? rf(0.1f, 3.0f) : rf(15.0f, 30.0f));
                if (ri(0, 2) == 0) hit_from(m, NOT_ADDR(0x330000u), coin() ? 0x3f1 : (coin() ? 0x127 : 0x343));
                else w8(m + 0xa4, 0xff);
                const Vec4 at = ee_ref<Vec4>(m + 0x10);
                const int32_t nb = lvd::box_count(lvd::BOXES);
                if (st == 0) {
                    if (coin()) { const int32_t g = fighter_group(); if (g >= 0) w16(pv + 0x80, u(g)); }
                    if (ri(0, 3) == 0) w16(pv + 0x80, 0xffff);
                    if (lvd::path_count() > 0) w32(pv + 0x84, u(ri(0, lvd::path_count() - 1)));
                    if (ri(0, 3) == 0) w8(0x0014c050 + ee_read<uint8_t>(m + 0xb0) + (ee_read<uint32_t>(0x0015ed84) << 4), 0xff);
                } else if (st == 1) {
                    const int mode = ri(0, 2);
                    hero_mode(mode);
                    hero_at(jitter(at, coin() ? 240.0f : 300.0f, 30.0f));
                } else if (st == 2) {
                    hero_mode(coin() ? 0 : 2);
                    int32_t b = ee_read<int32_t>(pv + 0x70);
                    if (b < 0 || b >= nb) { if (nb == 0) return false; b = ri(0, nb - 1); w32(pv + 0x70, u(b)); }
                    if (coin()) for (uint32_t k = 0; k < 0x30; k += 4) w32(lvd::box(lvd::BOXES, b) + k, 0);  // (a box of one point: every shot aims at its centre)
                    aim_at(m, box_centre(b), coin() ? 0.01f : 0.3f);
                    const int32_t period = frame_scale_i(0x5a);
                    const int32_t frame = ri(0, 0xffff);
                    w32(OV(0x0015f5cc), u(frame));
                    w32(pv + 0x74, u(ri(0, 2) != 0 ? frame % period : ri(0, period - 1)));
                    hero_at(jitter(at, 20.0f, 5.0f));
                    seed_rand();
                    return true;
                } else if (st == 3) {
                    hero_mode(ri(0, 4) == 0 ? 2 : 1);
                    const Vec4 h = jitter(at, 60.0f, 20.0f);
                    hero_at(h);
                    const uint32_t ship = ee_read<uint32_t>(P + 0x15f0);
                    if (ship != 0) ee_ref<Vec4>(ship + 0x10) = h;
                    aim_at(m, h, coin() ? 0.01f : 0.3f);
                    w32(pv + 0x74, coin() ? 1u : u(ri(0, 4)));
                    if (coin()) { const int32_t g = fighter_group(); w32(pv + 0x7c, g >= 0 && coin() ? u(g) : 0u); }
                }
                finish();
                return true;
            };
            pclass("p17_class347", 0x002cb310, vo_class347_update_l17, 400, cannons, [=](uint32_t m) { return vary347(m, false); });
            // (the break's debris: the w lanes debris_spawn's port leaves at 0 in the spawned pieces' pvars, +0xc / +0x1c of
            // each 0x80-byte block of the pvar area: a known difference, left out of this check only)
            level();
            const size_t ign347 = g_ignored_words.size();
            for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
            pclass("p17_class347_break", 0x002cb310, vo_class347_update_l17, 120, cannons, [=](uint32_t m) { return vary347(m, true); });
            g_ignored_words.resize(ign347);
            // the hits alone (0x2cbc10): from the hero's shots (classes 1009 / 295), from others, from no moby
            l17check("p17_2cbc10", 0x002cbc10, 150, Ret::None, [=] {
                level();
                const std::vector<uint32_t> cs = cannons();
                const uint32_t m = pick(cs), pv = ee_read<uint32_t>(m + 0x78);
                w8(m + 0x20, u(ri(0, 4)));
                wf(pv + 0x20, ri(0, 2) == 0 ? rf(0.0f, 2.0f) : rf(2.0f, 15.0f));
                hit_from(m, NOT_ADDR(0x330000u), ri(0, 2) == 0 ? 0x3f1 : (coin() ? 0x127 : u(ri(1, 0x700))));
                finish();
                return Args{{m}, {}};
            }, [](A a) { vo_2cbc10_l17(a.i[0]); return uint64_t{0}; });
            // the fighters laid out (0x2cbd38): a group with fighters, another group, none; the level's paths
            l17check("p17_2cbd38", 0x002cbd38, 120, Ret::None, [=] {
                level();
                const uint32_t m = pick(cannons()), pv = ee_read<uint32_t>(m + 0x78);
                const int32_t g = fighter_group();
                switch (ri(0, 4)) {
                case 0: w16(pv + 0x80, 0xffff); break;
                case 1: if (lvd::group_count() > 0) w16(pv + 0x80, u(ri(0, lvd::group_count() - 1))); break;
                default: if (g >= 0) w16(pv + 0x80, u(g)); break;
                }
                if (lvd::path_count() > 0) w32(pv + 0x84, u(ri(0, lvd::path_count() - 1)));
                return Args{{m}, {}};
            }, [](A a) { vo_2cbd38_l17(a.i[0]); return uint64_t{0}; });
            // a fighter of the group sent off (0x2f4fb8): some of them waiting (state 4), the camera's pitch on either side of its limit
            l17check("p17_2f4fb8", 0x002f4fb8, 150, Ret::Int32, [=] {
                level();
                const int32_t g = fighter_group();
                if (g < 0) return Args{{0xffffffffu}, {}};
                for (uint32_t e = lvd::group(g);; e += 2) {
                    const uint32_t q = table() + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                    if (ee_read<int16_t>(q + 0xa6) == 0x733) {
                        w8(q + 0x20, ri(0, 2) != 0 ? 4u : u(ri(0, 6)));
                        w32(ee_read<uint32_t>(q + 0x78) + 0x88, u(ri(0, 3)));
                        w8(q + 0x53, coin() ? 0u : u(ri(1, 2)));
                    }
                    if (ee_read<int16_t>(e) < 0) break;
                }
                wfrand(L17_ADDR(0x001676d0), 3, -0.6f, 0.6f);
                fifty();
                finish();
                return Args{{u(g)}, {}};
            }, [](A a) { return r32(a.i[0] == 0xffffffffu ? 0u : vo_2f4fb8_l17(static_cast<int32_t>(a.i[0]))); });
            // the shot's spawn (0x2e94d8) directly
            l17check("p17_2e94d8", 0x002e94d8, 100, Ret::Int32, [=] {
                level();
                const uint32_t m = pick(cannons());
                const Vec4 at = ee_ref<Vec4>(m + 0x10);
                ee_ref<Vec4>(T4) = jitter(at, 3.0f, 3.0f);
                ee_ref<Vec4>(T4 + 0x10) = jitter(at, 100.0f, 40.0f);
                fifty();
                return Args{{m, T4, T4 + 0x10}, {rf(0.5f, 4.0f)}};
            }, [](A a) { return r32(vo_2e94d8_l17(a.i[0], a.i[1], a.i[2], a.f[0])); });

            // class 1368, the shot: spawned by a cannon (the port's 0x2e94d8) at the hero, a mate cannon, the world or into the open
            // (the collision record's triangle OV(0x173e90) .. 0x173ebc differs when the hero's moby is the one met: a known
            // difference of segment_collide's port, left out of this check only)
            const size_t ign1368 = g_ignored_words.size();
            for (uint32_t k = 0; k < 0x30; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
            pclass("p17_class1368", 0x002e8e08, vo_class1368_update_l17, 400, cannons, [=](uint32_t m) {
                const std::vector<uint32_t> cs = cannons();
                const Vec4 at = ee_ref<Vec4>(m + 0x10);
                hero_mode(ri(0, 3) == 0 ? ri(1, 2) : 0);
                const uint32_t hm = ee_read<uint32_t>(P + 0x2080);
                const bool at_hero = hm != 0 && ri(0, 2) == 0;  // (at the hero's moby where the collision grid has it: the hero not moved)
                Vec4 h = at_hero ? ee_ref<Vec4>(hm + 0x10) : jitter(at, 40.0f, 10.0f);
                if (at_hero) h.z += 0.5f; else hero_at(h);
                if (coin()) ee_ref<Vec4>(P + 0xd0) = jitter(h, 6.0f, 2.0f);  // (the point the shot homes on within 8)
                Vec4 from = jitter(at, 2.0f, 2.0f), to = h;
                if (at_hero) {
                    from = jitter(h, 4.0f, 1.0f);
                    if (coin()) w16(0x00141dc0, 0);
                    if (coin()) { w16(0x00141dc2, u(ri(0, 3))); w32(0x0015eea4, R() & 0xfffff); }  // (the record's time behind the play time)
                }
                else switch (ri(0, 4)) {
                case 0: { const uint32_t o = pick(cs); to = ee_ref<Vec4>(o + 0x10); if (o != m) from = jitter(to, 14.0f, 4.0f); break; }
                case 1: from = jitter(h, 1.0f, 1.0f); break;
                case 2: to = jitter(at, 200.0f, 100.0f); break;
                default: from = jitter(h, 12.0f, 3.0f); break;
                }
                if (ri(0, 7) == 0) { from = Vec4{coin() ? rf(0, 12) : rf(960, 975), rf(10, 900), rf(10, 900), 1.0f}; }
                else if (ri(0, 6) == 0) { from.z = coin() ? rf(60, 85) : rf(885, 900); }
                ee_ref<Vec4>(T4) = from;
                ee_ref<Vec4>(T4 + 0x10) = to;
                fifty();
                const uint32_t s = vo_2e94d8_l17(m, T4, T4 + 0x10, fpu_mul(lwc1(0x0015ed6c), 60.0f));
                if (s == 0) return false;
                const uint32_t spv = ee_read<uint32_t>(s + 0x78);
                if (coin()) w32(spv + 0x38, u(ri(0, 2) == 0 ? ri(0, 2) : (coin() ? ri(50, 75) : ri(60, 600))));
                if (ri(0, 3) == 0) w8(s + 0x20, ri(0, 9) == 0 ? 2u : 1u);
                if (coin()) w16(0x00141dc0, 0);
                if (ri(0, 4) == 0) wf(ee_read<uint32_t>(s + 0x78) + 0x40, rf(0.5f, 3.0f));
                p17_subject = s;  // (the subject is the shot)
                finish();
                return true;
            });
            g_ignored_words.resize(ign1368);
        }

        // class 669, the electric fan: its states, the hero near its arms or not, the camera seeing it or not
        {
            auto fans = [of_class] { return of_class(0x29d); };
            pclass("p17_class669", 0x002d77f0, vo_class669_update_l17, 240, fans, [=](uint32_t m) {
                static const uint8_t sts[] = {0, 1, 1, 1, 2};
                w8(m + 0x20, sts[ri(0, 4)]);
                // (it runs only while the hero is in one of four states, or in sub-state 0x11 / 0x12: sub_0020d568)
                static const uint32_t hs[] = {0x6a, 0x82, 0x76, 0x75};
                switch (ri(0, 5)) {
                case 0: w32(P + 0x208c, u(ri(0x11, 0x12))); break;
                case 1: w32(P + 0x208c, 0); w32(P + 0x2084, u(ri(0, 0x90))); break;
                default: w32(P + 0x208c, 0); w32(P + 0x2084, hs[ri(0, 3)]); break;
                }
                const Vec4 at = ee_ref<Vec4>(m + 0x10);
                hero_at(jitter(at, coin() ? 6.0f : 40.0f, 3.0f));
                wf(m + 0x40, rf(-3.1f, 3.1f));
                if (ri(0, 2) != 0) camera_sees(at, 20.0f, 15.0f); else camera_away(at);
                fifty();
                finish();
                return true;
            });
        }
        // class 835, the floating mine: hits by mines and others, stood on, a moby met, nothing met
        {
            auto mines = [of_class] { return of_class(0x343); };
            pclass("p17_class835", 0x002dc3f8, vo_class835_update_l17, 300, mines, [=](uint32_t m) {
                static const uint8_t sts[] = {0, 1, 1, 1, 2};
                w8(m + 0x20, sts[ri(0, 4)]);
                if (ri(0, 2) == 0) hit_from(m, NOT_ADDR(0x230000u), coin() ? 0x343 : u(ri(1, 0x700)));
                else w8(m + 0xa4, 0xff);
                switch (ri(0, 3)) {
                case 0: w32(P + 0x23c, m); break;
                case 1: w32(P + 0x240, m); break;
                default: break;
                }
                if (coin()) {  // the mine next to another moby (mobys_in_sphere finds it in its grid cell)
                    const std::vector<uint32_t> all = live();
                    const uint32_t q = pick(all);
                    if (q != m) { Vec4 p = ee_ref<Vec4>(q + 0x10); p.z -= 0.75f; ee_ref<Vec4>(m + 0x10) = jitter(p, 0.5f, 0.5f); }
                }
                coll_hit_set_moby(ri(0, 2) == 0 ? 0u : pick(live()));
                hero_at(jitter(ee_ref<Vec4>(m + 0x10), 5.0f, 2.0f));
                fifty();
                finish();
                return true;
            });
        }
        // the sliding doors (classes 1359 .. 1373): every state, the hero / the camera / its mate in its boxes or not, its sound
        {
            auto doors = [live] {
                std::vector<uint32_t> v;
                for (uint32_t m : live()) if (ee_read<uint32_t>(m + 0x74) == 0x002e87d8u) v.push_back(m);
                return v;
            };
            pclass("p17_class1359", 0x002e87d8, vo_class1359_update_l17, 400, doors, [=](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w8(m + 0x20, u(ri(0, 6)));
                const std::vector<uint32_t> all = live();
                if (ri(0, 9) == 0) w32(coin() ? pv + 0x18 : pv + 0x1c, 0xffffffffu);
                if (ri(0, 7) == 0) w32(L17_ADDR(0x0015f5c4), 2);
                const int32_t nb = lvd::box_count(lvd::BOXES);
                const int32_t b18 = ee_read<int32_t>(pv + 0x18), b1c = ee_read<int32_t>(pv + 0x1c);
                auto in_box = [&](int32_t b) { return b >= 0 && b < nb ? jitter(box_centre(b), 0.5f, 0.3f) : jitter(ee_ref<Vec4>(m + 0x10), 3.0f, 1.0f); };
                hero_at(coin() ? in_box(coin() ? b18 : b1c) : jitter(ee_ref<Vec4>(m + 0x10), 30.0f, 5.0f));
                ee_ref<Vec4>(OV(0x00166dc0)) = ri(0, 2) == 0 ? in_box(coin() ? b18 : b1c) : jitter(ee_ref<Vec4>(m + 0x10), 40.0f, 10.0f);
                if (coin()) {
                    const uint32_t q = pick(all);
                    w32(pv + 0x10, (q - table()) >> 8);
                    if (coin() && q != m) ee_ref<Vec4>(q + 0x10) = in_box(b18);
                } else w32(pv + 0x10, 0xffffffffu);
                wf(m + 0x40, coin() ? rf(-0.7f, 0.7f) : rf(0.8f, 3.1f) * (coin() ? 1.0f : -1.0f));
                w16(P + 0x308, coin() ? 0u : 1u);
                w8(m + 0xbc, u(ri(0, 1)));
                wf(pv + 0x14, ri(0, 3) == 0 ? rf(-0.06f, 0.06f) : (coin() ? rf(-2.2f, 2.2f) : (coin() ? 1.99f : -1.99f)));
                if (ri(0, 3) == 0) {  // its sound slot playing for it (or for another moby)
                    const int32_t slot = ri(0, 7);
                    w32(pv + 0x20, u(slot));
                    const uint32_t e = 0x0013e550u + u(slot) * 0x70u;
                    w32(e + 0x88, coin() ? m : pick(all));
                    w8(e + 0x74, u(ri(0, 1)));
                } else if (coin()) w32(pv + 0x20, 0xffffffffu);
                fifty();
                finish();
                return true;
            });
        }
        // the laser glow (particle 0x50, 0x26fdd0) directly
        l17check("p17_26fdd0", 0x0026fdd0, 120, Ret::Int32, [level] {
            level();
            ee_ref<Vec4>(T4) = Vec4{rf(100, 800), rf(100, 800), rf(100, 400), rf(-1, 1)};
            const int32_t fade = ri(0, 3) == 0 ? 0 : ri(-2, 17);
            return Args{{T4, coin() ? 1u : 0u, u(fade)}, {}};
        }, [](A a) { return r32(vo_26fdd0_l17(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]))); });

        // ---- P17.2 (stage 4): class 1379, the hero's ship (veldin_orbit_ship.cpp), with its helpers 0x253a90 (a class's
        // mobys shown / solid), 0x2ec000 (the guns), 0x2ec150 (the missiles), 0x2ec360 (the flight), 0x2edf10 (the arena's
        // edge): each state with the hero by its pad or not, the prompt free or refused, its boxes the level's / others /
        // none, the stick and the buttons, the speed and the frames about their thresholds, the ship about its arena's edges,
        // its sounds playing or not, the HUD's record, the camera seeing it, hits and crashes (p17_class1379_hits)
        {
            auto ships = [of_class] { return of_class(0x563); };
            // the camera record of uid 5 (the table OV(0x167150), 48 records of 0xa0, the uid at +0x86, the cached one at OV(0x166e00)):
            // uid5_set_pvar_80 / _90 write the flight's (and the lift's) camera place and angles into its +0x70's +0x80 / +0x90; when the
            // level has no such record, or its +0x70 is 0, the writes land in the null page (not compared): one is made with its +0x70
            // on a test buffer (T3) in most of the cases
            auto cam5_ready = [] {
                const uint32_t tab = OV(0x00167150);
                uint32_t r = 0;
                for (uint32_t k = 0; k < 48 && r == 0; ++k) if (ee_read<int16_t>(tab + k * 0xa0 + 0x86) == 5) r = tab + k * 0xa0;
                if (r == 0) { r = tab + 47 * 0xa0; w16(r + 0x86, 5); }
                if (ee_read<uint32_t>(r + 0x70) == 0) w32(r + 0x70, T3);
                w32(OV(0x00166e00), r);
            };
            if (info) {
                level();
                const uint32_t tab = OV(0x00167150);
                for (uint32_t k = 0; k < 48; ++k) if (ee_read<int16_t>(tab + k * 0xa0 + 0x86) == 5) std::printf("INFO p17 the level's uid-5 camera record %u, +0x70 %08x\n", k, ee_read<uint32_t>(tab + k * 0xa0 + 0x70));
            }
            auto any_box = [] { const int32_t nb = lvd::box_count(lvd::BOXES); return nb > 0 ? ri(0, nb - 1) : -1; };
            auto box_ok = [](int32_t b) { return b >= 0 && b < lvd::box_count(lvd::BOXES); };
            auto help_idle = [] { w32(0x00151720, 0); w32(0x001516d0 + 0x50, 0); w32(0x001516d0 + 0x1c, 0xffffffffu); };
            // a sound slot in the pvar word `at`: none, one playing for the ship, or one stopped / another moby's
            auto sound_slot = [live, pick](uint32_t m, uint32_t at) {
                if (ri(0, 3) == 0) { w32(at, 0xffffffffu); return; }
                const int32_t s = ri(0, 7);
                w32(at, u(s));
                const uint32_t e = 0x0013e550u + u(s) * 0x70u;
                w32(e + 0x88, ri(0, 3) != 0 ? m : pick(live()));
                w8(e + 0x74, u(ri(0, 1)));
            };
            if (info) {
                level();
                for (uint32_t m : ships()) {
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    const Vec4 q = ee_ref<Vec4>(m + 0x10);
                    std::printf("INFO p17 class 1379 moby %u uid %u at (%.1f, %.1f, %.1f) boxes fc %d 100 %d 110 %d 120 %d 12c %d group %d sounds %d %d\n",
                                (m - table()) >> 8, ee_read<uint16_t>(m + 0xb2), q.x, q.y, q.z, ee_read<int32_t>(pv + 0xfc), ee_read<int32_t>(pv + 0x100),
                                ee_read<int32_t>(pv + 0x110), ee_read<int32_t>(pv + 0x120), ee_read<int32_t>(pv + 0x12c), ee_read<int32_t>(pv + 0x11c),
                                ee_read<int32_t>(pv + 0x108), ee_read<int32_t>(pv + 0x10c));
                }
            }
            // the flight's inputs: the frames about the speed's (L17:0x162000) and the stick's (0x162014) thresholds, the
            // stick, the buttons (0x13caf0 held: 0x84 the guns, 0x28 the missiles, 0x40 the boost; 0x13caf4 / 0x13caf8 pressed /
            // released; 0x13cae0 0x200 R1), the cooldowns, the missiles left, the speed, the angles, the camera's offset
            auto flight_inputs = [=](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                const int32_t t0 = ee_read<int32_t>(L17_ADDR(0x00162000)), t1 = ee_read<int32_t>(L17_ADDR(0x00162014));
                switch (ri(0, 3)) {
                case 0: w16(pv + 0x68, u(ri(0, std::max(t0, 0) + 1))); break;
                case 1: w16(pv + 0x68, u(std::max(t1 + ri(-1, 1), 0))); break;
                default: w16(pv + 0x68, u(std::max(t0, t1) + ri(1, 300))); break;
                }
                wf(0x00141070, ri(0, 4) == 0 ? 0.0f : rf(-1.0f, 1.0f));
                wf(0x00141074, ri(0, 4) == 0 ? 0.0f : rf(-1.0f, 1.0f));
                w32(0x0013caf0, R() & 0xecu);
                w32(0x0013caf4, coin() ? 0x40u : 0u);
                w32(0x0013caf8, coin() ? 0x40u : 0u);
                w32(0x0013cae0, coin() ? 0x200u : 0u);
                w32(pv + 0x80, u(ri(0, 3)));
                w32(pv + 0x84, u(ri(0, 3)));
                w8(P + 0x15f6, u(ri(0, 3)));
                w32(pv + 0xec, coin() ? 0u : pick(live()));
                const float sc = ee_read<float>(L17_ADDR(0x001620a0)), top = ee_read<float>(L17_ADDR(0x00162008)) * sc;
                wf(pv + 0x64, ri(0, 5) == 0 ? ee_read<float>(L17_ADDR(0x00162004)) * sc : rf(0.0f, top * 1.3f));
                w8(pv + 0x60, u(ri(0, 1)));
                w32(pv + 0xdc, ri(0, 2) == 0 ? 0u : u(ri(1, 5)));
                wf(pv + 0x34, rf(-1.6f, 1.6f)); wf(pv + 0x38, rf(-3.1f, 3.1f)); wf(m + 0x44, rf(-1.6f, 1.6f)); wf(pv + 0x30, rf(-0.8f, 0.8f));
                wf(pv + 0x6c, rf(-1.0f, 1.0f)); wf(pv + 0x70, rf(-1.0f, 1.0f));
                { const float h = rf(-3.1f, 3.1f), p = rf(-1.0f, 1.0f); ee_ref<Vec4>(pv + 0x10) = Vec4{std::cos(h) * std::cos(p), std::sin(h) * std::cos(p), std::sin(p), 0.0f}; }
                if (ri(0, 3) == 0) w32(pv + 0x110, 0xffffffffu); else if (coin()) w32(pv + 0x110, u(any_box()));
                if (!box_ok(ee_read<int32_t>(pv + 0x12c)) || ri(0, 4) == 0) w32(pv + 0x12c, u(any_box()));
                sound_slot(m, pv + 0x10c);
            };
            // the ship about its arena (the box +0x12c): inside, or near its horizontal edge / its top / its bottom (0x2edf10)
            auto in_arena = [=](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                const uint32_t b = lvd::box(lvd::BOXES, ee_read<int32_t>(pv + 0x12c));
                const Vec4 c = ee_ref<Vec4>(b + 0x30), r0 = ee_ref<Vec4>(b);
                const float reach = std::sqrt(r0.x * r0.x + r0.y * r0.y), half = ee_read<float>(b + 0x28);
                const float a = rf(-3.1f, 3.1f);
                float d = rf(0.0f, reach * 0.6f), z = c.z + rf(-half * 0.6f, half * 0.6f);
                switch (ri(0, 3)) {
                case 0: d = reach + rf(-15.0f, 5.0f); break;
                case 1: z = c.z + half + rf(-15.0f, 5.0f); break;
                case 2: z = c.z - half + rf(-5.0f, 15.0f); break;
                default: break;
                }
                ee_ref<Vec4>(m + 0x10) = Vec4{c.x + d * std::cos(a), c.y + d * std::sin(a), z, 1.0f};
                if (ri(0, 4) == 0) wf(m + 0x10 + 4u * u(ri(0, 2)), coin() ? rf(0.0f, 20.0f) : rf(1000.0f, 1020.0f));  // (the space's bounds)
            };
            // a state of the ship; `crash`: next to a live moby (of a class it ignores, a hull cannon, another) or the world, a hit on
            // it that may take the last of its health (the debris)
            auto vary1379 = [=](uint32_t m, bool crash) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                static const uint8_t sts[] = {0, 1, 1, 1, 2, 2, 2, 3, 4, 4, 4, 4, 5, 6, 7, 7, 8, 8, 8, 9, 9, 10, 12};
                static const uint8_t crash_sts[] = {4, 4, 4, 4, 2, 8, 8, 0, 3, 5, 9};
                const int st = crash ? crash_sts[ri(0, 10)] : sts[ri(0, 22)];
                w8(m + 0x20, u(st));
                w8(m + 0xbc, ri(0, 5) == 0 ? 1u : 0u);
                const float full = ee_read<float>(L17_ADDR(0x00162084));
                wf(P + 0x15fc, rf(full * 0.2f, full));
                if (coin()) w32(P + 0x15f0, m); else if (coin()) w32(P + 0x15f0, 0);
                if (ri(0, 3) == 0) w8(P + 0x160f, u(R() & 3u));
                if (!box_ok(ee_read<int32_t>(pv + 0xfc)) || ri(0, 4) == 0) w32(pv + 0xfc, u(any_box()));
                sound_slot(m, pv + 0x108);
                sound_slot(m, pv + 0x10c);
                switch (ri(0, 3)) {  // its moby group: none, 0, a group of the level
                case 0: w32(pv + 0x11c, coin() ? 0xffffffffu : 0u); break;
                case 1: if (lvd::group_count() > 1) w32(pv + 0x11c, u(ri(1, lvd::group_count() - 1))); if (lvd::group(ee_read<int32_t>(pv + 0x11c)) == 0) w32(pv + 0x11c, 0); break;
                default: break;
                }
                const Vec4 at = ee_ref<Vec4>(m + 0x10);
                switch (st) {
                case 0:
                    w32(L17_ADDR(0x00162184), coin() ? 1u : 0u);
                    if (ee_read<uint8_t>(m + 0xb0) != 0xff && coin()) w8(0x0014c050 + ee_read<uint8_t>(m + 0xb0) + (ee_read<uint32_t>(0x0015ed84) << 4), coin() ? 0xffu : 0u);
                    if (ri(0, 8) == 0) w32(L17_ADDR(0x0015f420), ee_read<uint32_t>(L17_ADDR(0x0015f420)) + 4u * u(ri(1, 3)));  // (the palette's copy unaligned)
                    break;
                case 1: {
                    wf(pv + 0x130, coin() ? 0.0f : rf(0.0f, 1.0f));
                    const float gap = ee_read<float>(L17_ADDR(0x00161fe0));
                    const bool near = coin();
                    hero_at(near ? jitter(at, 0.5f, 0.3f) : jitter(at, gap * 4.0f + 4.0f, 2.0f));
                    const uint32_t hm = ee_read<uint32_t>(P + 0x2080);  // (moby_sphere_gap: the mobys' spheres +0)
                    if (near && hm != 0) { ee_ref<Vec4>(hm) = ee_ref<Vec4>(m); wf(hm, ee_read<float>(m) + rf(-0.5f, 0.5f)); }
                    if (ri(0, 4) != 0) help_idle();
                    w32(0x0013cae4, coin() ? 0x10u : (R() & ~0x10u));
                    w32(P + 0x2084, ri(0, 3) == 0 ? (coin() ? 0x32u : 0x1du) : 0u);
                    break;
                }
                case 2:
                    wf(pv + 0x130, ri(0, 3) == 0 ? 1.0f : (coin() ? rf(0.95f, 1.0f) : rf(0.0f, 0.95f)));
                    w32(pv + 0x120, coin() ? 0xffffffffu : u(any_box()));
                    flight_inputs(m);
                    in_arena(m);
                    break;
                case 4: case 8:
                    flight_inputs(m);
                    in_arena(m);
                    if (st == 4) {
                        w32(pv + 0xe8, ri(0, 4) == 0 ? u(ri(-1, 0)) : 1u);
                        wf(pv + 0x130, coin() ? 0.0f : rf(0.0f, 1.0f));
                    } else {
                        w32(pv + 0xf8, u(ri(0, 3)));
                        wf(pv + 0x130, ri(0, 3) == 0 ? 1.0f : (coin() ? rf(0.9f, 1.0f) : rf(0.0f, 0.9f)));
                        w32(pv + 0x100, coin() ? 0xffffffffu : u(any_box()));
                        if (ri(0, 4) == 0) w32(pv + 0xfc, 0xffffffffu);
                        if (ri(0, 3) == 0) w8(m + 0xb0, 0xff);
                    }
                    break;
                case 7:
                    w16(pv + 0x62, u(ri(0, 3)));
                    break;
                case 9:
                    wf(pv + 0x130, coin() ? 0.0f : rf(0.0f, 0.2f));
                    break;
                default:
                    break;
                }
                // a hit (not in the states 1, 2, 7): the damage below the health, or with `crash` above it in part of the cases
                if (st != 1 && st != 2 && st != 7 && ri(0, 2) != 0) {
                    give_hit(m, 0x30001u);
                    if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
                        const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                        w32(e + 0x24, 0x30001u | (R() & 0xffu));
                        const float health = ee_read<float>(P + 0x15fc);
                        wf(e + 0x2c, crash && coin() ? rf(health, health * 2.0f + 1.0f) : rf(0.0f, health * 0.9f));
                        w32(e + 0x30, R());
                        wfrand(e + 0x10, 4, -3.0f, 3.0f);
                    }
                } else {
                    w8(m + 0xa4, 0xff);
                }
                if (crash && (st == 4 || st == 8 || st == 2)) {  // next to a moby or on the world
                    const std::vector<uint32_t> all = live();
                    uint32_t q = pick(all);
                    if (q == m) q = all.front();
                    if (q != m) {
                        static const int16_t cls[] = {0x15b, 0x192, 0x4c2, 0x52, 0, -1, -1, -1};
                        const int16_t c = cls[ri(0, 7)];
                        if (c >= 0) w16(q + 0xa6, u(c));
                        ee_ref<Vec4>(m + 0x10) = jitter(ee_ref<Vec4>(q + 0x10), 1.5f, 1.5f);
                    }
                    if (coin()) wf(P + 0x15fc, rf(0.0f, ee_read<float>(L17_ADDR(0x00162060)) * 1.5f));
                    w32(pv + 0xdc, coin() ? 0u : 2u);
                }
                if (ri(0, 2) != 0) camera_sees(ee_ref<Vec4>(m + 0x10), 6.0f, 25.0f); else camera_away(ee_ref<Vec4>(m + 0x10));
                if (ri(0, 4) != 0) cam5_ready();
                fifty();
                finish();
                return true;
            };
            // The guns' shot (class 1009, class1009_launch) takes the aim's vector whole as its velocity (its pvars +0): the aim's
            // w lane is never written by 0x2ec000 (vec_from_spherical_v and vec3_add write x y z), the original's is a stale lane
            // of its frame (an earlier callee's leftover; ORACLE_WATCH: written at pc 0x2e21f8 under 0x2ec000), the port's frame
            // and the generated code's before it are cleared: the +0xc word of each 0x80-byte block of the pvar area beyond the
            // ship's own is left out of the checks that run the guns from the flight (with `debris`, also +0x1c: the blasts' pieces,
            // the w lanes debris_spawn's port leaves at 0). A known difference, these checks only.
            auto pvar_w_ignored = [level, ships](bool debris) {
                level();
                std::vector<uint32_t> own;
                for (uint32_t s : ships()) own.push_back(ee_read<uint32_t>(s + 0x78));
                for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) {
                    bool mine = false;
                    for (uint32_t pv : own) mine = mine || (a >= pv && a < pv + 0x180);
                    if (mine) continue;
                    g_ignored_words.push_back(a + 0xc);
                    if (debris) g_ignored_words.push_back(a + 0x1c);
                }
            };
            // (and the collision record's triangle OV(0x173e90) .. 0x173ebc, which differs when a moby is the one met: a known
            // difference of the collision ports, left out of the checks that run the flight's sphere_collide only)
            auto triangle_ignored = [] { for (uint32_t k = 0; k < 0x30; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k); };
            const size_t ign1379 = g_ignored_words.size();
            pvar_w_ignored(false);
            triangle_ignored();
            // the states without a blast (a case the port blows up is dealt again: p17_class1379_hits has them)
            pclass("p17_class1379", 0x002ed018, vo_class1379_update_l17, 500, ships, [=](uint32_t m) {
                if (!vary1379(m, false)) return false;
                static std::vector<uint8_t> keep;
                uint8_t* ram = ee_get_ram_base();
                keep.assign(ram, ram + 0x02000000);
                const uint32_t stack_top = ee_stack_ptr();
                const uint8_t st0 = ee_read<uint8_t>(m + 0x20);
                bool blast = false;
                try {
                    vo_class1379_update_l17(m);
                    blast = st0 != 7 && ee_read<uint8_t>(m + 0x20) == 7;  // (every blast puts it in the state 7)
                } catch (const std::exception&) {}
                ee_stack_ptr() = stack_top;
                std::memcpy(ram, keep.data(), keep.size());
                return !blast;
            });
            g_ignored_words.resize(ign1379);
            // the hits and the crashes (the blasts' debris: pvar_w_ignored(true))
            pvar_w_ignored(true);
            triangle_ignored();
            pclass("p17_class1379_hits", 0x002ed018, vo_class1379_update_l17, 300, ships, [=](uint32_t m) { return vary1379(m, true); });
            g_ignored_words.resize(ign1379);
            pvar_w_ignored(false);
            triangle_ignored();
            // the flight alone (0x2ec360), without a blast
            l17check("p17_2ec360", 0x002ec360, 300, Ret::None, [=] {
                for (int k = 0; k < 40; ++k) {
                    level();
                    const uint32_t m = pick(ships()), pv = ee_read<uint32_t>(m + 0x78);
                    w8(m + 0x20, coin() ? 4u : 8u);
                    w8(m + 0xbc, ri(0, 5) == 0 ? 1u : 0u);
                    const float full = ee_read<float>(L17_ADDR(0x00162084));
                    wf(P + 0x15fc, rf(full * 0.2f, full));
                    flight_inputs(m);
                    in_arena(m);
                    if (ri(0, 2) != 0) camera_sees(ee_ref<Vec4>(m + 0x10), 6.0f, 25.0f); else camera_away(ee_ref<Vec4>(m + 0x10));
                    cam5_ready();
                    fifty();
                    finish();
                    static std::vector<uint8_t> keep;
                    uint8_t* ram = ee_get_ram_base();
                    keep.assign(ram, ram + 0x02000000);
                    const uint32_t stack_top = ee_stack_ptr();
                    bool blast = true;
                    try {
                        vo_2ec360_l17(m, pv);
                        blast = ee_read<uint8_t>(m + 0x20) == 7;  // (every blast puts it in the state 7)
                    } catch (const std::exception&) {}
                    ee_stack_ptr() = stack_top;
                    std::memcpy(ram, keep.data(), keep.size());
                    if (!blast) return Args{{m, pv}, {}};
                }
                return Args{{0u, 0u}, {}};
            }, [](A a) { if (a.i[0] != 0) vo_2ec360_l17(a.i[0], a.i[1]); return uint64_t{0}; });
            g_ignored_words.resize(ign1379);
            // the arena's edge (0x2edf10) on the stick's two words in the test area
            l17check("p17_2edf10", 0x002edf10, 300, Ret::None, [=] {
                level();
                const uint32_t m = pick(ships());
                flight_inputs(m);
                in_arena(m);
                wf(T4, rf(-1.0f, 1.0f)); wf(T4 + 4, rf(-1.0f, 1.0f));
                fifty();
                return Args{{m, T4, T4 + 4}, {}};
            }, [](A a) { vo_2edf10_l17(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
            // the guns (0x2ec000) along an aim
            l17check("p17_2ec000", 0x002ec000, 240, Ret::None, [=] {
                level();
                const uint32_t m = pick(ships()), pv = ee_read<uint32_t>(m + 0x78);
                flight_inputs(m);
                in_arena(m);
                if (ri(0, 2) != 0) camera_sees(ee_ref<Vec4>(m + 0x10), 6.0f, 25.0f); else camera_away(ee_ref<Vec4>(m + 0x10));
                fifty();
                finish();
                return Args{{m, pv}, {rf(-3.1f, 3.1f), rf(-1.5f, 1.5f)}};
            }, [](A a) { vo_2ec000_l17(a.i[0], a.i[1], a.f[0], a.f[1]); return uint64_t{0}; });
            // the missiles (0x2ec150)
            l17check("p17_2ec150", 0x002ec150, 240, Ret::None, [=] {
                level();
                const uint32_t m = pick(ships()), pv = ee_read<uint32_t>(m + 0x78);
                flight_inputs(m);
                in_arena(m);
                fifty();
                finish();
                return Args{{m, pv}, {}};
            }, [](A a) { vo_2ec150_l17(a.i[0], a.i[1]); return uint64_t{0}; });
            // a class's mobys shown / solid / glowing (0x253a90): class 0x325, a live moby's class, a class with none
            l17check("p17_253a90", 0x00253a90, 200, Ret::None, [=] {
                level();
                const std::vector<uint32_t> all = live();
                const int k = ri(0, 3);
                const uint32_t cls = k == 0 ? 0x325u : (k == 1 ? 0x7ffu : static_cast<uint32_t>(ee_read<int16_t>(pick(all) + 0xa6)));
                if (coin()) for (int n = 0; n < 4; ++n) { const uint32_t q = pick(all); if (ee_read<int16_t>(q + 0xa6) == static_cast<int16_t>(cls)) w8(q + 0x20, coin() ? 0xfeu : 3u); }
                for (uint32_t q : all) if (ee_read<int16_t>(q + 0xa6) == static_cast<int16_t>(cls)) { w16(q + 0x34, R() & 0xffffu); w8(q + 0x31, u(ri(0, 1))); }
                auto v = [] { return u(ri(-1, 2)); };
                return Args{{cls, v(), v(), v()}, {}};
            }, [](A a) { vo_253a90_l17(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]), static_cast<int32_t>(a.i[3])); return uint64_t{0}; });

            // 1.02 Gameplay, fix 10 in class 1379: with the fix on, the pad's prompt is not offered while the hero is in the state 0x32
            // / 0x1d or a vehicle mode (+0x20a4), the triangle's press sets the hero's state 0x32 at once, and the fade's end (state 2)
            // does not set it again. Run with RAC1_GAMEPLAY102=10: the port with the fix on leaves what the port with the fix off
            // leaves with the fix's effect added by hand (kind 0: the hero out of reach; 1: hero_set_state(0x32, 1) after it; 2: the
            // fade's hero_set_state made a no-op by the scene mode 0x15f5c4 = 2), as batch 220's p132_class69_fix10 does
            if (const char* only = std::getenv("ORACLE_ONLY"); only == nullptr || std::strcmp(only, "p17_class1379_fix10") == 0) {
                const std::vector<uint32_t> c1379 = mobys_with(0x002ed018);
                if (std::getenv("RAC1_GAMEPLAY102") == nullptr || c1379.empty()) {
                    std::printf("%-26s skipped (run with RAC1_GAMEPLAY102=10)\n", "p17_class1379_fix10");
                    ++g_skipped;
                } else {
                    int bad = 0, n = 0, blocked = 0, boarded = 0, fade_skipped = 0;
                    static std::vector<uint8_t> start, with_fix;
                    const std::unordered_set<uint32_t> ignored(g_ignored_words.begin(), g_ignored_words.end());
                    for (int c = 0; c < 90; ++c) {
                        level();
                        const uint32_t m = c1379[0], pv = ee_read<uint32_t>(m + 0x78), hm = ee_read<uint32_t>(0x001413d0u);
                        const int kind = c % 3;
                        if (kind < 2) {
                            w8(m + 0x20, 1u);
                            ee_ref<Vec4>(hm) = ee_ref<Vec4>(m);  // (the hero's moby's sphere at the ship's)
                            wf(hm, ee_read<float>(m) + rf(-0.5f, 0.5f));
                            help_idle();
                            w32(OV(0x0015f5c4), 0u);
                            w32(0x0013cae4u, 0x10u);
                            if (kind == 0) {
                                const int blk = ri(0, 2);
                                w32(0x001413d4u, blk == 0 ? 0x32u : (blk == 1 ? 0x1du : 0u));
                                w8(0x001413f4u, blk == 2 ? 1u : 0u);
                            } else {
                                w32(0x001413d4u, 0u);
                                w8(0x001413f4u, 0u);
                            }
                        } else {
                            w8(m + 0x20, 2u);
                            wf(pv + 0x130, 0.99f);
                            w32(OV(0x0015f5c4), 2u);  // (both runs in the scene mode 2: hero_set_state(0x32, 1) a no-op in the run without the fix)
                            if (c % 2 == 0) w32(pv + 0x120, 0xffffffffu);  // (no boarding box: hero_place, which enters 0x32 itself, not called)
                            w32(0x001413d4u, 0u);
                        }
                        if (ri(0, 3) == 0) { wf(0x0015ed68, 1.2f); wf(0x0015ed6c, 0.02f); wf(0x0015ed70, 0.0004f); }  // (50 Hz)
                        seed_rand();
                        uint8_t* ram = ee_get_ram_base();
                        start.assign(ram, ram + 0x02000000);
                        g_advanced_disabled = false;
                        vo_class1379_update_l17(m);
                        g_advanced_disabled = true;
                        with_fix.assign(ram, ram + 0x02000000);
                        std::memcpy(ram, start.data(), start.size());
                        if (kind == 0) {
                            const float h0 = ee_read<float>(hm);
                            wf(hm, h0 + 200000.0f);
                            vo_class1379_update_l17(m);
                            wf(hm, h0);
                            ++blocked;
                        } else if (kind == 1) {
                            vo_class1379_update_l17(m);
                            if (ee_read<uint8_t>(m + 0x20) != 2u) { if (bad < 8) std::printf("  p17_class1379_fix10 case %d: not boarded\n", c); ++bad; }
                            hero_set_state(0x32, 1);
                            ++boarded;
                        } else {
                            // out of the scene mode: without the fix the fade's end enters 0x32, with it not
                            w32(OV(0x0015f5c4), 0u);
                            vo_class1379_update_l17(m);
                            const bool unfixed_entered = ee_read<uint32_t>(0x001413d4u) == 0x32u;
                            std::memcpy(ram, start.data(), start.size());
                            w32(OV(0x0015f5c4), 0u);
                            g_advanced_disabled = false;
                            vo_class1379_update_l17(m);
                            g_advanced_disabled = true;
                            const bool fixed_entered = ee_read<uint32_t>(0x001413d4u) == 0x32u;
                            const bool fixed_flew = ee_read<uint8_t>(m + 0x20) == 4u;
                            std::memcpy(ram, start.data(), start.size());
                            vo_class1379_update_l17(m);  // (the run without the fix in the scene mode, compared below)
                            const bool placed = ee_read<int32_t>(ee_read<uint32_t>(m + 0x78) + 0x120) != -1;
                            if (unfixed_entered && (placed || !fixed_entered) && fixed_flew && with_fix[m + 0x20] == 4u) { if (!placed) ++fade_skipped; }
                            else { if (bad < 8) std::printf("  p17_class1379_fix10 case %d: the fade: entered %d / %d, flew %d\n", c, unfixed_entered, fixed_entered, fixed_flew); ++bad; }
                        }
                        ++n;
                        for (uint32_t a = 0x00100000; a < 0x02000000; a += 4) {
                            if (ignored.count(a) != 0) continue;
                            if (a >= ee_stack_ptr() - 0x10000 && a < ee_stack_ptr() + 0x10000) continue;
                            uint32_t xw, yw;
                            std::memcpy(&xw, &with_fix[a], 4); std::memcpy(&yw, &ram[a], 4);
                            if (xw != yw) { if (bad < 8) std::printf("  p17_class1379_fix10 case %d (kind %d): %08x fix=%08x expected=%08x\n", c, kind, a, xw, yw); ++bad; break; }
                        }
                    }
                    std::printf("%-26s %s (%d cases, %d blocked, %d boarded, %d fade_skipped)\n", "p17_class1379_fix10",
                                bad == 0 && blocked > 0 && boarded > 0 && fade_skipped > 0 ? "ok" : "FAIL", n, blocked, boarded, fade_skipped);
                    if (bad != 0 || blocked == 0 || boarded == 0 || fade_skipped == 0) ++g_fail;
                }
            }
        }
        // class 1380, a lift (veldin_orbit_lift.cpp), with its helpers 0x2ee9c0 (the camera beside it) and 0x2eea70 (the hero's
        // path laid out on it): each state, the hero on it or not, walking or not, at its centre or near it below / above, its
        // height about its two stops, its sound playing or not, its path the level's / none / another, the camera's ease
        {
            auto lifts = [of_class] { return of_class(0x564); };
            auto cam5_ready = [] {  // (as class 1379's: the camera record of uid 5 with its +0x70 on a test buffer)
                const uint32_t tab = OV(0x00167150);
                uint32_t r = 0;
                for (uint32_t k = 0; k < 48 && r == 0; ++k) if (ee_read<int16_t>(tab + k * 0xa0 + 0x86) == 5) r = tab + k * 0xa0;
                if (r == 0) { r = tab + 47 * 0xa0; w16(r + 0x86, 5); }
                if (ee_read<uint32_t>(r + 0x70) == 0) w32(r + 0x70, T3);
                w32(OV(0x00166e00), r);
            };
            if (info) {
                level();
                for (uint32_t m : lifts()) {
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    const Vec4 q = ee_ref<Vec4>(m + 0x10);
                    std::printf("INFO p17 class 1380 moby %u uid %u at (%.1f, %.1f, %.1f) state %u heights %.2f %.2f path %d (%d points) 7c %d\n",
                                (m - table()) >> 8, ee_read<uint16_t>(m + 0xb2), q.x, q.y, q.z, ee_read<uint8_t>(m + 0x20), ee_read<float>(pv + 0x60),
                                ee_read<float>(pv + 0x64), ee_read<int32_t>(pv + 0x80),
                                ee_read<int32_t>(pv + 0x80) >= 0 ? ee_read<int32_t>(lvd::path(ee_read<int32_t>(pv + 0x80))) : -1, ee_read<int32_t>(pv + 0x7c));
                }
            }
            auto hero_on = [P, live, pick](uint32_t m) {  // the moby the hero stands on, his walk, his +0x30e
                switch (ri(0, 4)) {
                case 0: w32(P + 0x2fc, 0); break;
                case 1: w32(P + 0x2fc, pick(live())); break;
                default: w32(P + 0x2fc, m); break;
                }
                w16(P + 0x308, ri(0, 2) != 0 ? u(ri(1, 3)) : 0u);
                w16(P + 0x30e, ri(0, 3) == 0 ? 1u : 0u);
            };
            auto vary1380 = [=](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                const int st = ri(0, 11);
                w8(m + 0x20, u(st));
                w32(pv + 0x7c, ri(0, 2) == 0 ? 1u : 0u);
                w8(m + 0xbc, u(ri(0, 2)));
                const float hi = ee_read<float>(pv + 0x60), lo = ee_read<float>(pv + 0x64);
                const Vec4 at = ee_ref<Vec4>(m + 0x10);
                switch (ri(0, 3)) {  // its height: between its stops, at one (to the last bit or a hair off), past them
                case 0: wf(m + 0x18, lo + (hi - lo) * rf(0.0f, 1.0f)); break;
                case 1: wf(m + 0x18, coin() ? hi : lo); break;
                case 2: wf(m + 0x18, (coin() ? hi : lo) + rf(-0.01f, 0.01f)); break;
                default: wf(m + 0x18, coin() ? hi + rf(0.0f, 3.0f) : lo - rf(0.0f, 3.0f)); break;
                }
                wf(pv + 0x68, coin() ? 0.0f : rf(-0.3f, 0.3f));
                if (ri(0, 3) == 0) w32(pv + 0x6c, 0xffffffffu);
                else {
                    const int32_t slot = ri(0, 7);
                    w32(pv + 0x6c, u(slot));
                    const uint32_t e = 0x0013e550u + u(slot) * 0x70u;
                    w32(e + 0x88, ri(0, 3) != 0 ? m : pick(live()));
                    w8(e + 0x74, u(ri(0, 1)));
                }
                hero_on(m);
                const Vec4 top = Vec4{at.x, at.y, ee_read<float>(m + 0x18), 1.0f};
                switch (ri(0, 3)) {  // the hero at its centre, near it (2 .. 32), on it off centre, or far
                case 0: hero_at(jitter(top, 0.3f, 0.2f)); break;
                case 1: { const float r = rf(1.5f, 34.0f), a = rf(-3.1f, 3.1f); hero_at(Vec4{at.x + r * std::cos(a), at.y + r * std::sin(a), coin() ? hi : lo, 1.0f}); break; }
                case 2: hero_at(jitter(top, 1.5f, 0.2f)); break;
                default: hero_at(jitter(at, 80.0f, 20.0f)); break;
                }
                {  // his ground's height: near the lift's lower stop, its upper, or elsewhere
                    const float half = std::fabs(hi - lo) * 0.5f;
                    const int k = ri(0, 2);
                    wf(P + 0x2d8, k == 0 ? lo + rf(-half * 1.2f, half * 1.2f) : (k == 1 ? hi + rf(-half * 1.2f, half * 1.2f) : rf(0.0f, 900.0f)));
                }
                ee_ref<Vec4>(P + 0xd0) = jitter(ee_ref<Vec4>(P + 0x80), 2.0f, 1.0f);
                if (st == 0) {
                    const int k = ri(0, 3);
                    if (k == 0) w32(pv + 0x80, 0xffffffffu);
                    else if (k == 1 && lvd::path_count() > 0) w32(pv + 0x80, u(ri(0, lvd::path_count() - 1)));
                } else if (st == 1) {
                    wf(pv + 0x70, coin() ? rf(0.0f, 1.0f) : rf(0.97f, 1.0f));
                    wf(pv + 0x74, rf(0.0f, 0.2f));
                    ee_ref<Vec4>(OV(0x00166dc0)) = jitter(at, 20.0f, 8.0f);
                    wfrand(OV(0x00166dd0), 3, -3.0f, 3.0f);
                    w8(m + 0xbc, u(ri(0, 9)));
                    if (ri(0, 4) != 0) cam5_ready();
                } else if (st == 6 || st == 7) {
                    wf(pv + 0x78, rf(-3.1f, 3.1f));
                }
                wf(m + 0x40, rf(-0.3f, 0.3f)); wf(m + 0x48, rf(-3.1f, 3.1f));
                fifty();
                finish();
                return true;
            };
            pclass("p17_class1380", 0x002ee2b0, vo_class1380_update_l17, 400, lifts, vary1380);
            // the camera's place and angles beside the lift (0x2ee9c0) on the test area
            l17check("p17_2ee9c0", 0x002ee9c0, 150, Ret::None, [=] {
                level();
                const uint32_t m = pick(lifts());
                ee_ref<Vec4>(OV(0x00166dc0)) = jitter(ee_ref<Vec4>(m + 0x10), 20.0f, 8.0f);
                ee_ref<Vec4>(P + 0xd0) = jitter(ee_ref<Vec4>(m + 0x10), 5.0f, 2.0f);
                wfrand(T4, 8, -9.0f, 9.0f);
                return Args{{m, T4, T4 + 0x10}, {}};
            }, [](A a) { vo_2ee9c0_l17(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
            // the hero's path on the lift (0x2eea70): him on it or not, walking or not, the moby turned
            l17check("p17_2eea70", 0x002eea70, 150, Ret::None, [=] {
                level();
                const uint32_t m = pick(lifts());
                hero_on(m);
                wf(m + 0x48, rf(-3.1f, 3.1f)); wf(m + 0x40, rf(-0.4f, 0.4f));
                moby_update_transform(m);
                return Args{{m}, {}};
            }, [](A a) { vo_2eea70_l17(a.i[0]); return uint64_t{0}; });
        }

        // ---- P17.3 (stage 4): class 1382, a trooper (veldin_orbit_trooper.cpp), with its helpers 0x2efa48 (its hits and its
        // sight), 0x2efd48 (a walking step), 0x2f00a0 (its head's look), 0x2f0210 (its lights): each state with the camera in or
        // out of its boxes (the active pair +0x2b0 / +0x2b4 or the other +0x2b8 / +0x2bc), the hero (its target) ahead within
        // striking reach, near, far, hidden, inside its area or not, its sight and timers about their edges, its animation and
        // its end bit, the strike's frame (17.5) reached or not, hits from beams and other classes (p17_class1382_hits: the
        // knock-down's fall, burst and debris)
        {
            auto troopers = [of_class] { return of_class(0x566); };
            auto any_box = [] { const int32_t nb = lvd::box_count(lvd::BOXES); return nb > 0 ? ri(0, nb - 1) : -1; };
            // the trooper's area (the level path +0x1d0 taken as a polygon): its corners' mean, or none
            auto area_centre = [](uint32_t pv, Vec4& c) {
                const int32_t path = ee_read<int32_t>(pv + 0x1d0);
                if (path < 0 || path >= lvd::path_count()) return false;
                const uint32_t a = lvd::path(path);
                const int32_t n = ee_read<int32_t>(a);
                if (n <= 0 || n > 64) return false;
                c = Vec4{0, 0, 0, 1};
                for (int32_t k = 0; k < n; ++k) { const Vec4 q = ee_ref<Vec4>(a + 0x10 + static_cast<uint32_t>(k) * 0x10); c.x += q.x; c.y += q.y; c.z += q.z; }
                c.x /= static_cast<float>(n); c.y /= static_cast<float>(n); c.z /= static_cast<float>(n);
                return true;
            };
            if (info) {
                level();
                for (uint32_t m : troopers()) {
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    const Vec4 q = ee_ref<Vec4>(m + 0x10);
                    Vec4 c{};
                    const bool has = area_centre(pv, c);
                    std::printf("INFO p17 class 1382 moby %u uid %u at (%.1f, %.1f, %.1f) state %u boxes %d %d %d %d area %d (%d corners, centre %.1f %.1f %.1f) speed %.3f\n",
                                (m - table()) >> 8, ee_read<uint16_t>(m + 0xb2), q.x, q.y, q.z, ee_read<uint8_t>(m + 0x20), ee_read<int32_t>(pv + 0x2b0),
                                ee_read<int32_t>(pv + 0x2b4), ee_read<int32_t>(pv + 0x2b8), ee_read<int32_t>(pv + 0x2bc), ee_read<int32_t>(pv + 0x1d0),
                                has ? ee_read<int32_t>(lvd::path(ee_read<int32_t>(pv + 0x1d0))) : -1, c.x, c.y, c.z, ee_read<float>(pv + 0x1ec));
                }
            }
            // the camera (OV(0x166dc0), the level's 0x1676c0) and the trooper's boxes: the level's, an active box with the camera
            // in it or not, the other pair
            auto cam_boxes = [=](uint32_t m, bool active) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                switch (active ? 0 : ri(0, 4)) {
                case 0: break;
                case 1: w32(pv + 0x2b0, u(any_box())); w32(pv + 0x2b4, coin() ? 0xffffffffu : u(any_box())); break;
                case 2: w32(pv + 0x2b0, 0xffffffffu); w32(pv + 0x2b8, u(any_box())); w32(pv + 0x2bc, coin() ? 0xffffffffu : u(any_box())); break;
                case 3: w32(pv + 0x2b0, 0xffffffffu); w32(pv + 0x2b8, 0xffffffffu); break;
                default: w32(pv + 0x2b0, 0xffffffffu); w32(pv + 0x2b8, u(any_box())); w32(pv + 0x2bc, u(any_box())); break;
                }
                const int32_t boxes[] = {ee_read<int32_t>(pv + 0x2b0), ee_read<int32_t>(pv + 0x2b4), ee_read<int32_t>(pv + 0x2b8), ee_read<int32_t>(pv + 0x2bc)};
                const int k = active ? (ee_read<int32_t>(pv + 0x2b0) >= 0 ? 0 : 4) : ri(0, 5);
                if (k < 4 && boxes[k] >= 0 && boxes[k] < lvd::box_count(lvd::BOXES)) ee_ref<Vec4>(OV(0x00166dc0)) = jitter(box_centre(boxes[k]), 0.5f, 0.3f);
                else if (k == 4) ee_ref<Vec4>(OV(0x00166dc0)) = jitter(ee_ref<Vec4>(m + 0x10), 30.0f, 6.0f);  // (within 29 of it: its floor band)
                else ee_ref<Vec4>(OV(0x00166dc0)) = jitter(ee_ref<Vec4>(m + 0x10), 300.0f, 60.0f);
            };
            // the hero (the trooper's target) ahead of it within striking reach, near, far, in its area, or hidden
            auto hero_place = [=](uint32_t m, bool close) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                const Vec4 at = ee_ref<Vec4>(m + 0x10);
                const float yaw = ee_read<float>(m + 0x48);
                Vec4 q = at;
                Vec4 c{};
                switch (close ? 0 : ri(0, 5)) {
                case 0: case 1: {  // ahead (within 15 degrees mostly), 0.5 .. 3 away, a little above or below
                    const float a = yaw + (coin() ? rf(-0.25f, 0.25f) : rf(-1.2f, 1.2f)), r = rf(0.5f, 3.2f);
                    q = Vec4{at.x + r * std::cos(a), at.y + r * std::sin(a), at.z + (coin() ? rf(-0.9f, 0.9f) : rf(-4.0f, 4.0f)), 1.0f};
                    break;
                }
                case 2: { const float a = rf(-3.1f, 3.1f), r = rf(3.0f, 70.0f); q = Vec4{at.x + r * std::cos(a), at.y + r * std::sin(a), at.z + rf(-2, 4), 1.0f}; break; }
                case 3: if (area_centre(pv, c)) q = jitter(c, 2.0f, 0.5f); else q = jitter(at, 10.0f, 1.0f); break;
                case 4: q = jitter(at, 200.0f, 30.0f); break;
                default: q = jitter(at, 8.0f, 1.0f); break;
                }
                hero_at(q);
                ee_ref<Vec4>(P + 0xd0) = Vec4{q.x, q.y, q.z + rf(0.5f, 1.5f), 1.0f};
                w32(P + 0x208c, !close && ri(0, 7) == 0 ? 0x18u : 0u);
                w8(P + 0x20a4, ri(0, 2) == 0 ? 3u : u(ri(0, 2)));
            };
            static const uint8_t want_anim[16][3] = {{0, 1, 2}, {1, 0, 2}, {2, 7, 8}, {0, 3, 1}, {3, 0, 5}, {3, 4, 5}, {7, 2, 8}, {8, 2, 7},
                                                     {9, 1, 3}, {9, 10, 3}, {10, 9, 3}, {10, 9, 3}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {3, 0, 1}};
            auto vary1382 = [=](uint32_t m, int st, bool close = false) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w8(m + 0x20, u(st));
                w8(m + 0x53, ri(0, 3) != 0 ? want_anim[st][ri(0, 2)] : u(ri(0, 10)));
                w8(m + 0x70, (ee_read<uint8_t>(m + 0x70) & 0xfdu) | (coin() ? 2u : 0u));  // (its animation's end bit)
                wf(m + 0x48, rf(-3.1f, 3.1f));
                w8(pv + 0x1e4, u(ri(0, 2)));
                w32(pv + 0x38, ri(0, 3) == 0 ? 1u : 0u);
                w8(pv + 0x15d, R() & 0xffu);
                switch (ri(0, 3)) {  // the chase timer: run out, about to, running
                case 0: case 1: wf(pv + 0x1d4, 0.0f); break;
                case 2: wf(pv + 0x1d4, rf(0.0f, 1.5f)); break;
                default: wf(pv + 0x1d4, rf(1.0f, 200.0f)); break;
                }
                { static const uint16_t hs[] = {0, 0, 1, 2, 50}; w16(pv + 0x1e0, hs[ri(0, 4)]); w16(pv + 0x1e2, hs[ri(0, 4)]); }
                wf(pv + 0x1d8, coin() ? 0.0f : rf(0.0f, 0.2f));
                wf(pv + 0x1dc, coin() ? 0.0f : rf(-0.3f, 0.3f));
                if (ri(0, 3) == 0) w32(pv + 0x1e8, 0xffffffffu);
                // (the level's troopers have not run their set-up yet: their home, speed and health as the state 0 makes them)
                ee_ref<Vec4>(pv + 0x1f0) = ee_ref<Vec4>(m + 0x10);
                wf(pv + 0x1ec, rf(0.11f, 0.15f));
                wf(pv + 0x20, 1.0f);
                w8(pv + 0x58, 8); w8(pv + 0x5a, 7); w8(pv + 0x29, 0); w16(pv + 0x24, 1);
                w32(pv + 0xd0, L17_ADDR(0x00162200));  // (its carry record's animation sets, +0x70: the level's, as the state 0 sets them)
                w16(m + 0x34, ee_read<uint16_t>(m + 0x34) | 0x1000u);
                if (ri(0, 5) == 0) ee_ref<Vec4>(pv + 0x1f0) = jitter(ee_ref<Vec4>(m + 0x10), 40.0f, 4.0f);  // (its home away from it)
                else if (coin()) ee_ref<Vec4>(pv + 0x1f0) = jitter(ee_ref<Vec4>(m + 0x10), 1.5f, 0.1f);
                if (ri(0, 4) == 0) wf(m + 0x18, ee_read<float>(m + 0x18) + rf(-1.0f, 6.0f));
                cam_boxes(m, close || ri(0, 2) != 0);  // (active in most cases: its target searched)
                hero_place(m, close);
                if (close && coin()) w8(m + 0x70, ee_read<uint8_t>(m + 0x70) & 0xfdu);
                if (st == 12) {  // carried: its carry record (the hook +0x10's, pvars +0x60) falling and sliding (7: let go when it
                                 // lands), or in another state, its countdown about 0
                    const uint32_t r = pv + 0x60;
                    // (not its state 3, held by the hero: carry_update's port differs from this level's original there on these
                    // records, in the moby grid's cells OV(0x19b7e0): a known difference of the shared port in dealt records, as
                    // batch 517 says; B260_CARRY_ST=3 shows it)
                    static const uint8_t rst[] = {0, 1, 2, 4, 5, 6, 8};
                    w16(r + 0x68, coin() ? 7u : u(rst[ri(0, 6)]));
                    if (const char* cs = std::getenv("B260_CARRY_ST")) w16(r + 0x68, u(std::atoi(cs)));  // (the record's state alone)
                    w32(r + 0x60, ee_read<uint32_t>(P + 0x2080));
                    wfrand(r + 0x10, 3, -0.2f, 0.2f);
                    wf(r + 0x18, rf(-0.3f, 0.0f));
                    wf(r + 0x8c, rf(0.3f, 1.0f));
                    wf(r + 0xc, rf(-1.0f, 2.0f));
                    wf(r + 0x6c, rf(0.0f, 1.2f));
                    wf(r + 0x78, rf(0.5f, 1.5f));
                }
                if (st == 10) {  // its animation's time about the strike's frame 17.5: the strike's animation set as the state 9 sets it,
                                 // its frame record copied onto a test buffer with the frame's time +4 about 280 / 16
                    moby_anim_set(m, 0xa, 5, 0);
                    const uint32_t rec = ee_read<uint32_t>(m + 0x68);
                    if (rec >= 0x00100000u && rec < 0x01ff0000u && ri(0, 4) != 0) {
                        for (uint32_t k = 0; k < 0x100; k += 4) w32(T4 + 0x200 + k, ee_read<uint32_t>(rec + k));
                        w8(m + 0x52, ee_read<uint8_t>(m + 0x53));
                        wf(m + 0x54, 0.0f);
                        w32(m + 0x68, T4 + 0x200);
                        w16(T4 + 0x204, u(280 + ri(-6, 6)));
                    }
                    if (ri(0, 3) == 0) w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u);  // (its animation's end: walks home)
                }
                fifty();
                finish();
                return true;
            };
            // (the debris' and other mobys' pvar w lanes are not the trooper's: left out of the knock-down's check only, as class
            // 1379's blasts do; the collision record's triangle when the hero's moby is met: the collision ports' known difference)
            auto pvar_w_ignored = [level, troopers] {
                level();
                std::vector<uint32_t> own;
                for (uint32_t s : troopers()) own.push_back(ee_read<uint32_t>(s + 0x78));
                for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) {
                    bool mine = false;
                    for (uint32_t pv : own) mine = mine || (a >= pv && a < pv + 0x300);
                    if (mine) continue;
                    g_ignored_words.push_back(a + 0xc);
                    g_ignored_words.push_back(a + 0x1c);
                }
            };
            // (B260_INFO: the states of the cases the port refused, dealt again)
            static int refused1382[16];
            auto note_refused = [](uint32_t m) {
                if (std::getenv("B260_INFO") == nullptr) return;
                static std::vector<uint8_t> keep;
                uint8_t* ram = ee_get_ram_base();
                keep.assign(ram, ram + 0x02000000);
                const uint32_t stack_top = ee_stack_ptr();
                const uint8_t st = ee_read<uint8_t>(m + 0x20);
                if (!run_trial(vo_class1382_update_l17, m)) ++refused1382[st & 15];
                ee_stack_ptr() = stack_top;
                std::memcpy(ram, keep.data(), keep.size());
            };
            auto print_refused = [info](const char* name) {
                if (!info) return;
                std::printf("INFO %s refused by state:", name);
                for (int k = 0; k < 16; ++k) { std::printf(" %d", refused1382[k]); refused1382[k] = 0; }
                std::printf("\n");
            };
            pclass("p17_class1382", 0x002eeb68, vo_class1382_update_l17, 600, troopers, [=](uint32_t m) {
                vary1382(m, ri(0, 14));
                w8(m + 0xa4, 0xff);  // (no hit: the knock-down is p17_class1382_hits')
                note_refused(m);
                return true;
            });
            print_refused("p17_class1382");
            // the states 9, 10, 11 with the hero (its target) just ahead: the walk's arrival, the strike's frame, the reach
            // from its walk home
            pclass("p17_class1382_close", 0x002eeb68, vo_class1382_update_l17, 300, troopers, [=](uint32_t m) {
                static const uint8_t sts[] = {6, 7, 8, 9, 10, 10, 11, 11};
                const int st = sts[ri(0, 7)];
                vary1382(m, st, true);
                if (st == 6 || st == 7 || st == 8) {  // on guard / turning: the hero seen ahead, out of its area (4 .. 12 away: in
                                                      // sight), the chase timer run out, the animation at its end
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    const Vec4 at = ee_ref<Vec4>(m + 0x10);
                    const float a = ee_read<float>(m + 0x48) + rf(-0.6f, 0.6f), r = coin() ? rf(4.0f, 12.0f) : rf(8.0f, 40.0f);
                    const Vec4 q{at.x + r * std::cos(a), at.y + r * std::sin(a), at.z + rf(-0.5f, 0.5f), 1.0f};
                    hero_at(q);
                    ee_ref<Vec4>(P + 0xd0) = Vec4{q.x, q.y, q.z + 1.0f, 1.0f};
                    if (ri(0, 3) != 0) wf(pv + 0x1d4, 0.0f);
                    w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u);
                }
                if (st == 11 && ri(0, 2) != 0) {  // its home away from it, its speed up
                    ee_ref<Vec4>(ee_read<uint32_t>(m + 0x78) + 0x1f0) = jitter(ee_ref<Vec4>(m + 0x10), 12.0f, 0.2f);
                    wf(ee_read<uint32_t>(m + 0x78) + 0x1d8, rf(0.0f, 0.3f));
                }
                w8(m + 0xa4, 0xff);
                note_refused(m);
                return true;
            });
            print_refused("p17_class1382_close");
            // carried (state 12) alone: its carry record (pvars +0x60) in each of its states, falling and sliding (7) in half
            pclass("p17_class1382_carry", 0x002eeb68, vo_class1382_update_l17, 300, troopers, [=](uint32_t m) {
                vary1382(m, 12);
                w8(m + 0xa4, 0xff);
                note_refused(m);
                return true;
            });
            print_refused("p17_class1382_carry");
            {
                const size_t ign = g_ignored_words.size();
                pvar_w_ignored();
                pclass("p17_class1382_hits", 0x002eeb68, vo_class1382_update_l17, 300, troopers, [=](uint32_t m) {
                    static const uint8_t sts[] = {1, 2, 4, 4, 5, 6, 9, 10, 11, 15, 15, 15};
                    const int st = sts[ri(0, 11)];
                    vary1382(m, st);
                    if (st == 15 || st == 4) {  // near its floor, or below the height 2, or above
                        const int k = ri(0, 2);
                        if (k == 1) wf(m + 0x18, rf(0.0f, 2.2f));
                        else if (k == 2) wf(m + 0x18, ee_read<float>(m + 0x18) + rf(0.5f, 5.0f));
                    }
                    if (ri(0, 3) != 0) hit_from(m, 0x330000u, ri(0, 2) == 0 ? int16_t{0x63} : static_cast<int16_t>(ee_read<int16_t>(pick(live()) + 0xa6)));
                    else w8(m + 0xa4, 0xff);
                    note_refused(m);
                    return true;
                });
                print_refused("p17_class1382_hits");
                g_ignored_words.resize(ign);
            }
            // its hits and its sight alone (0x2efa48), idle or not
            l17check("p17_2efa48", 0x002efa48, 300, Ret::None, [=] {
                level();
                const uint32_t m = pick(troopers());
                vary1382(m, ri(0, 3) == 0 ? 0 : ri(1, 15));
                if (coin()) hit_from(m, 0x330000u, ri(0, 2) == 0 ? int16_t{0x63} : static_cast<int16_t>(ee_read<int16_t>(pick(live()) + 0xa6)));
                else w8(m + 0xa4, 0xff);
                return Args{{m, ri(0, 2) == 0 ? 1u : 0u}, {}};
            }, [](A a) { vo_2efa48_l17(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
            // a walking step (0x2efd48) toward its target's point, its home or another point
            l17check("p17_2efd48", 0x002efd48, 200, Ret::Int32, [=] {
                level();
                const uint32_t m = pick(troopers()), pv = ee_read<uint32_t>(m + 0x78);
                vary1382(m, 9);
                ee_ref<Vec4>(pv + 0x180) = jitter(ee_ref<Vec4>(m + 0x10), coin() ? 0.3f : 6.0f, 0.5f);
                ee_ref<Vec4>(T4) = jitter(ee_ref<Vec4>(m + 0x10), ri(0, 2) == 0 ? 0.25f : 10.0f, 1.0f);
                const int k = ri(0, 2);
                const uint32_t goal = k == 0 ? pv + 0x180 : (k == 1 ? pv + 0x1f0 : T4);
                return Args{{m, goal}, {coin() ? ee_read<float>(pv + 0x1ec) : rf(0.0f, 0.3f)}};
            }, [](A a) { return r32(static_cast<uint32_t>(vo_2efd48_l17(a.i[0], a.i[1], a.f[0]))); });
            // its head's look (0x2f00a0) at the hero's point about it
            l17check("p17_2f00a0", 0x002f00a0, 200, Ret::None, [=] {
                level();
                const uint32_t m = pick(troopers());
                vary1382(m, ri(0, 14));
                if (coin()) ee_ref<Vec4>(P + 0xd0) = jitter(ee_ref<Vec4>(m + 0x10), 6.0f, 4.0f);
                if (ri(0, 3) == 0) wf(0x0015ed64, rf(0.5f, 2.0f));
                return Args{{m}, {}};
            }, [](A a) { vo_2f00a0_l17(a.i[0]); return uint64_t{0}; });
            // its lights (0x2f0210) in each state, the level's frame counter anywhere
            l17check("p17_2f0210", 0x002f0210, 240, Ret::None, [=] {
                level();
                const uint32_t m = pick(troopers());
                w8(m + 0x20, u(ri(0, 17)));
                w32(m + 0x90, R());
                fifty();
                finish();
                return Args{{m}, {}};
            }, [](A a) { vo_2f0210_l17(a.i[0]); return uint64_t{0}; });
        }
        // class 1448, a pod that carries the hero between its two ends (veldin_orbit_pod.cpp), with its helpers 0x2f20d8 (its
        // prompt) and 0x2f2180 (the ride): each state, the hero on it or not, in the air or not, the triangle, its prompt shown
        // or not, the end it waits at and the side, its moby done or not, the hero nearer one end or the other, the ride's
        // progress about its start, 0.8 and its end, both directions
        {
            auto pods = [of_class] { return of_class(0x5a8); };
            auto cam5_ready = [] {  // (as class 1379's: the camera record of uid 5 with its +0x70 on a test buffer)
                const uint32_t tab = OV(0x00167150);
                uint32_t r = 0;
                for (uint32_t k = 0; k < 48 && r == 0; ++k) if (ee_read<int16_t>(tab + k * 0xa0 + 0x86) == 5) r = tab + k * 0xa0;
                if (r == 0) { r = tab + 47 * 0xa0; w16(r + 0x86, 5); }
                if (ee_read<uint32_t>(r + 0x70) == 0) w32(r + 0x70, T3);
                w32(OV(0x00166e00), r);
            };
            // a path of the level with at least `n` points (-1: none)
            auto path_with = [](int32_t n) {
                for (int t = 0; t < 40; ++t) {
                    const int32_t k = ri(0, lvd::path_count() - 1);
                    if (ee_read<int32_t>(lvd::path(k)) >= n) return k;
                }
                return -1;
            };
            auto help_idle = [] { w32(0x00151720, 0); w32(0x001516d0 + 0x50, 0); w32(0x001516d0 + 0x1c, 0xffffffffu); };
            if (info) {
                level();
                std::vector<uint32_t> pvs;
                for (uint32_t q : live()) pvs.push_back(ee_read<uint32_t>(q + 0x78));
                std::sort(pvs.begin(), pvs.end());
                for (uint32_t m : pods()) {
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    const Vec4 q = ee_ref<Vec4>(m + 0x10);
                    uint32_t next = 0;
                    for (uint32_t a : pvs) if (a > pv) { next = a; break; }
                    std::printf("INFO p17 class 1448 moby %u uid %u at (%.1f, %.1f, %.1f) state %u ends %d %d hide %d paths %d %d %d %d %d %d words 80 %d 94 %d 98 %d b0 %08x pvars %08x next %08x (%d bytes)\n",
                                (m - table()) >> 8, ee_read<uint16_t>(m + 0xb2), q.x, q.y, q.z, ee_read<uint8_t>(m + 0x20), ee_read<int32_t>(pv + 0x78),
                                ee_read<int32_t>(pv + 0x7c), ee_read<int32_t>(pv + 0x84), ee_read<int32_t>(pv + 0x60), ee_read<int32_t>(pv + 0x64),
                                ee_read<int32_t>(pv + 0x68), ee_read<int32_t>(pv + 0x6c), ee_read<int32_t>(pv + 0x70), ee_read<int32_t>(pv + 0x74),
                                ee_read<int32_t>(pv + 0x80), ee_read<int32_t>(pv + 0x94), ee_read<int32_t>(pv + 0x98), ee_read<uint32_t>(pv + 0xb0), pv, next,
                                next != 0 ? static_cast<int>(next - pv) : -1);
                }
            }
            // its moby done (the level's byte, or its bit in the level's flag words) or not
            auto done_flag = [](uint32_t m) {
                const int32_t id = ee_read<int16_t>(m + 0xb2);
                const uint32_t byte = L17_ADDR(0x001bbb30) + static_cast<uint32_t>(id) + 0x454;
                const uint32_t word = 0x0014c190u + (ee_read<uint32_t>(0x0015ed84) << 8) + (static_cast<uint32_t>(id >> 5) << 2);
                const uint32_t bit = 1u << (ee_read<uint16_t>(m + 0xb2) & 0x1f);
                switch (ri(0, 3)) {
                case 0: w8(byte, 1); break;
                case 1: w8(byte, 0); w32(word, ee_read<uint32_t>(word) | bit); break;
                default: w8(byte, 0); w32(word, ee_read<uint32_t>(word) & ~bit); break;
                }
            };
            auto vary1448 = [=](uint32_t m, int st) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w8(m + 0x20, u(st));
                w8(m + 0x53, ri(0, 2) == 0 ? u(ri(0, 3)) : (coin() ? 2u : 0u));
                w8(m + 0x70, (ee_read<uint8_t>(m + 0x70) & 0xfdu) | (coin() ? 2u : 0u));
                w32(pv + 0xac, coin() ? 0u : 1u);
                w32(pv + 0xa8, ri(0, 4) == 0 ? 1u : 0u);
                w32(pv + 0x80, ri(0, 2) != 0 ? u(ri(0, 1)) : u(ri(-1, 4)));
                w32(pv + 0x94, ri(0, 5) != 0 ? u(ri(0, 1)) : u(ri(-1, 4)));  // (only 0 / 1 in the game; the prompt's other texts)
                w32(pv + 0x98, u(ri(0, 1)));
                done_flag(m);
                switch (ri(0, 3)) {  // the hero on it (in the air or not), near one end, or elsewhere
                case 0: case 1: w32(P + 0x2fc, m); break;
                case 2: w32(P + 0x2fc, pick(live())); break;
                default: w32(P + 0x2fc, 0); break;
                }
                w16(P + 0x30e, ri(0, 3) == 0 ? 1u : 0u);
                const int32_t ends[2] = {ee_read<int32_t>(pv + 0x78), ee_read<int32_t>(pv + 0x7c)};
                const int e = ri(0, 2);
                if (e < 2 && ends[e] >= 0 && ends[e] < lvd::box_count(lvd::BOXES)) hero_at(jitter(box_centre(ends[e]), 6.0f, 1.0f));
                else hero_at(jitter(ee_ref<Vec4>(m + 0x10), 40.0f, 5.0f));
                w32(0x0013cae4, ri(0, 2) != 0 ? 0x10u : (R() & 0xffefu));  // (the triangle pressed or not)
                w32(L17_ADDR(0x0015f594), ri(0, 2) != 0 ? 0xau : u(ri(-1, 12)));  // (the HUD message shown: its prompt, 10)
                switch (ri(0, 4)) {  // the ride's progress: about its start, past 0.8, about its end, anywhere
                case 0: wf(pv + 0xa4, rf(0.0f, 0.02f)); break;
                case 1: wf(pv + 0xa4, rf(0.79f, 0.99f)); break;
                case 2: wf(pv + 0xa4, coin() ? 1.0f : rf(0.999f, 1.0f)); break;
                default: wf(pv + 0xa4, rf(0.0f, 1.0f)); break;
                }
                wf(pv + 0x88, rf(-3.1f, 3.1f)); wf(pv + 0x8c, rf(-0.2f, 0.2f));
                wf(pv + 0x9c, rf(0.5f, 2.0f)); wf(pv + 0xa0, rf(0.5f, 2.0f));
                if (ri(0, 3) == 0) {  // the ride's paths other paths of the level (of other point counts than the level's three)
                    for (uint32_t o = 0x60; o < 0x78; o += 4) {
                        const int32_t k = path_with(3);
                        if (k >= 0 && coin()) w32(pv + o, u(k));
                    }
                }
                if (ri(0, 4) != 0) cam5_ready();
                if (coin()) help_idle();
                fifty();
                finish();
            };
            pclass("p17_class1448", 0x002f1940, vo_class1448_update_l17, 500, pods, [=](uint32_t m) {
                static const uint8_t sts[] = {0, 1, 1, 1, 1, 2, 2, 2, 3, 4};
                vary1448(m, sts[ri(0, 9)]);
                return true;
            });
            l17check("p17_2f20d8", 0x002f20d8, 150, Ret::None, [=] {
                level();
                const uint32_t m = pick(pods());
                vary1448(m, 1);
                return Args{{m}, {}};
            }, [](A a) { vo_2f20d8_l17(a.i[0]); return uint64_t{0}; });
            l17check("p17_2f2180", 0x002f2180, 300, Ret::Int32, [=] {
                level();
                const uint32_t m = pick(pods());
                vary1448(m, 2);
                return Args{{m, u(ri(0, 1))}, {}};
            }, [](A a) { return r32(static_cast<uint32_t>(vo_2f2180_l17(a.i[0], static_cast<int32_t>(a.i[1])))); });

            // 1.02 Fixes, fix 39 (the PAL 2.00 disc's own code, ported): the port with the fix on against the port with it off
            // on an equivalent state, then the fix's own word: the frames at rest +0xb0 counted each frame (cleared in the states
            // 0 and 3); in the state 1 the prompt and the triangle only past frame_scale_i(30) of them (the port without the fix
            // run with the hero in the air, +0x30e, put back). Run with RAC1_FIXES102=39.
            {
                const char* name = "p17_class1448_fix39";
                const char* only = std::getenv("ORACLE_ONLY");
                const char* fx = std::getenv("RAC1_FIXES102");
                if (only != nullptr && std::strcmp(only, name) != 0) {
                } else if (fx == nullptr || std::string(fx).find("39") == std::string::npos) {
                    std::printf("%-26s skipped (run with RAC1_FIXES102=39)\n", name);
                    ++g_skipped;
                } else {
                    int bad = 0, n = 0, blocked = 0, counted = 0;
                    static std::vector<uint8_t> start, with_fix;
                    const std::unordered_set<uint32_t> ignored(g_ignored_words.begin(), g_ignored_words.end());
                    for (int c = 0; c < 300; ++c) {
                        level();
                        const uint32_t m = pick(pods()), pv = ee_read<uint32_t>(m + 0x78);
                        static const uint8_t sts[] = {0, 1, 1, 1, 1, 1, 2, 3, 4};
                        const int st = sts[ri(0, 8)];
                        vary1448(m, st);
                        if (st == 1) w32(pv + 0xa8, 0);  // (no ride already asked for: the press alone starts it)
                        const uint32_t frames = u(ri(0, 3) == 0 ? ri(0, 1000) : ri(15, 40));
                        w32(pv + 0xb0, frames);
                        uint8_t* ram = ee_get_ram_base();
                        start.assign(ram, ram + 0x02000000);
                        const uint32_t sp0 = ee_stack_ptr();
                        g_advanced_disabled = false;
                        try { vo_class1448_update_l17(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
                        g_advanced_disabled = true;
                        with_fix.assign(ram, ram + 0x02000000);
                        std::memcpy(ram, start.data(), start.size());
                        // the expected result
                        const uint32_t count = frames + 1;
                        const bool on = ee_read<uint32_t>(P + 0x2fc) == m && ee_read<int16_t>(P + 0x30e) == 0;
                        const bool wait = st == 1 && on && !(frame_scale_i(0x1e) < static_cast<int32_t>(count));
                        if (wait) {
                            w16(P + 0x30e, 1);
                            try { vo_class1448_update_l17(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
                            w16(P + 0x30e, 0);
                            ++blocked;
                        } else {
                            try { vo_class1448_update_l17(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
                        }
                        w32(pv + 0xb0, st == 0 || st == 3 ? 0u : count);
                        ++counted;
                        ++n;
                        for (uint32_t a = 0x00100000; a < 0x02000000; a += 4) {
                            if (ignored.count(a) != 0) continue;
                            if (a >= ee_stack_ptr() - 0x10000 && a < ee_stack_ptr() + 0x10000) continue;
                            uint32_t xw, yw;
                            std::memcpy(&xw, &with_fix[a], 4); std::memcpy(&yw, &ram[a], 4);
                            if (xw != yw) { if (bad < 8) std::printf("  %s case %d (state %d): %08x fix=%08x expected=%08x\n", name, c, st, a, xw, yw); ++bad; break; }
                        }
                    }
                    std::printf("%-26s %s (%d cases, %d held by the wait, %d counted)\n", name, bad == 0 && blocked > 0 ? "ok" : "FAIL", n, blocked, counted);
                    if (bad != 0 || blocked == 0) ++g_fail;
                }
            }
        }
        // class 1562 (a cutscene's moby kept smoking) and the props 1873 / 1876 (veldin_orbit_props.cpp): each state, the game
        // mode and the scene's state about their values, the scene's moby of the smoking class 0x2ee or not; the props hit with
        // each damage (-1, 0, 0.5, 5, 12) or not, and breaking (their debris' and broken copies' pvar w lanes left out: the
        // debris_spawn port's known w lanes)
        {
            auto subjects = [live](uint32_t fn) {
                return [live, fn] {
                    std::vector<uint32_t> v;
                    for (uint32_t q : live()) if (ee_read<uint32_t>(q + 0x74) == fn) v.push_back(q);
                    return v;
                };
            };
            if (info) {
                level();
                for (uint32_t fn : {0x002f2e78u, 0x002f5260u, 0x002f5390u})
                    for (uint32_t m : subjects(fn)()) {
                        const Vec4 q = ee_ref<Vec4>(m + 0x10);
                        std::printf("INFO p17 class %d moby %u uid %u at (%.1f, %.1f, %.1f) state %u\n", ee_read<int16_t>(m + 0xa6), (m - table()) >> 8,
                                    ee_read<uint16_t>(m + 0xb2), q.x, q.y, q.z, ee_read<uint8_t>(m + 0x20));
                    }
            }
            pclass("p17_class1562", 0x002f2e78, vo_class1562_update_l17, 200, subjects(0x002f2e78), [=](uint32_t m) {
                w8(m + 0x20, u(ri(0, 2)));
                w32(L17_ADDR(0x0015f5c4), ri(0, 2) != 0 ? 2u : u(ri(0, 7)));
                w32(L17_ADDR(0x0016d190), u(ri(0, 3)));
                const uint32_t q = ri(0, 4) == 0 ? 0u : pick(live());
                if (q != 0 && coin()) w16(q + 0xa6, 0x2ee);
                w32(L17_ADDR(0x0016d160) + 0x180, q);
                w32(L17_ADDR(0x0016d160) + 0x17c, ri(0, 3) == 0 ? 0u : pick(live()));
                fifty();
                finish();
                return true;
            });
            const size_t ign = g_ignored_words.size();
            {
                std::vector<uint32_t> own;
                level();
                for (uint32_t s : live()) {
                    const uint32_t fn = ee_read<uint32_t>(s + 0x74);
                    if (fn == 0x002f5260u || fn == 0x002f5390u) own.push_back(ee_read<uint32_t>(s + 0x78));
                }
                for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) {
                    bool mine = false;
                    for (uint32_t pv : own) mine = mine || (a >= pv && a < pv + 0x80);
                    if (mine) continue;
                    g_ignored_words.push_back(a + 0xc);
                    g_ignored_words.push_back(a + 0x1c);
                }
            }
            auto vary_prop = [=](uint32_t m) {
                w8(m + 0x20, u(ri(0, 3) == 0 ? ri(0, 3) : ri(1, 2)));
                give_hit(m, 0x10000u);
                fifty();
                finish();
                return true;
            };
            pclass("p17_class1873", 0x002f5260, vo_class1873_update_l17, 200, subjects(0x002f5260), vary_prop);
            pclass("p17_class1876", 0x002f5390, vo_class1876_update_l17, 200, subjects(0x002f5390), vary_prop);
            g_ignored_words.resize(ign);
        }

        g_ignored_words = class_ignored;
        g_ignore_lone_w_lanes = lone_w;
        g_extra_noop_hooks.clear();
    }

    //@@NEXT
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
    g_extra_hooks.clear();
    g_host_clock_fixed = saved_clock;
}

}  // namespace rac1::oracle
