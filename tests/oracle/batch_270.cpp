// Oracle batch 270 (levels 17 / 18 port, 0.3.0): Veldin's finale's (level 18) own code on its real level data. Run with
// ORACLE_LEVEL=18 build/tests/ovl_oracle --batch 270 (needs the ISO; no RAM dump). The subjects are the level's own mobys of each class
// (placed by its gameplay data) after the first gameplay frame, perturbed; the original is level 18's own function at its own address, the
// port the one registered for it (the hand ports that replaced veldin_finale_classes_tr_N.cpp): the 50 class updates, the 23 draw callbacks the
// classes queue, sound emitter types 3 / 5 / 6, camera type 0x12's accept, the ten hooks of classes 568 and 1906, the camera mode hook and its
// callback, and the movement-mode hero update (L18:0x0020efa8, veldin_finale_hero_tr_*.cpp). The other hero variants are proved by batch 261.
// Built after batch 260 (level 18's).

#include "ovl_oracle.h"
#include "trial_guard.h"

#include <game/enhance/advanced.h>
#include <game/level/level_data.h>

#include <game/ovl/enemy_749.h>
#include <game/ovl/hero_move_mode.h>
#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/veldin_finale_own.h>
#include <game/ovl/aridia_mobys.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/shared_class1021.h>
#include <game/ovl/shared_class1108.h>
#include <game/ovl/shared_class1135.h>
#include <game/ovl/shared_class1356.h>
#include <game/ovl/shared_class1443.h>
#include <game/ovl/shared_class1523.h>
#include <game/ovl/shared_class256.h>
#include <game/ovl/shared_class341.h>
#include <game/ovl/shared_class49.h>
#include <game/ovl/shared_class615.h>
#include <game/ovl/shared_class638.h>
#include <game/ovl/shared_class655_draw.h>
#include <game/ovl/shared_class830.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_finale.h>
#include <game/ovl/shared_clank_pad.h>
#include <game/ovl/shared_flash.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_rock.h>
#include <game/ovl/shared_shockwave.h>
#include <game/ovl/shared_swingshot_target.h>
#include <game/ovl/shared_zones.h>
#include <game/ovl/empty_class_updates.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sound_emitter.h>
#include <game/lib/libcdvd.h>
#include <runtime/kernel/disc.h>

#include <optional>
#include <cmath>
#include <cstring>
#include <string>
#include <map>
#include <unordered_set>

namespace rac1::oracle {

namespace {
// ORACLE_ONLY: one check's name, or a prefix ending in '*' (the checks named so)
bool only_mismatch(const char* only, const char* name) {
    const size_t n = std::strlen(only);
    if (n > 0 && only[n - 1] == '*') return std::strncmp(only, name, n - 1) != 0;
    return std::strcmp(only, name) != 0;
}

// The checks named here reach sphere_collide or capsule_collide, whose chunk buffers, unpacked vertices and cell
// walk the port keeps on the host stack (the original: 0x70000000 .. 0x70002000, 0x70002000, 0x70003800): A2's
// collision scratch ranges are left out of these checks, and only of them (stage 5, task C2b).
bool c2b_spr_check(const char* name) {
    static const char* const kNames[] = {"class1454", "class1906", "class638", "class885", "hero_move_mode", "class1134",
                                         "class1135", "class1355", "class583", "class644", "class805",
                                         "own41_rocket", "own564_shell", "own624_ring", "own628_burst", "own568_bomb", "own583_gate", "own587_float",
                                         "own644_pan", "own885_piece", "own983_ball", "own1355_diver",
                                         "own1422_preamble",
                                         "own1454_trooper", "own1454_walk", "own1906_robot", "own1906_walk", "own1563_scene"};  // (P18.4: walker_move, floor_probe_v, the blasts' shards)  // (P18.3 part 3: the phase change's floor_probe_v)  // (P18.2's: hero_place, sphere / floor collisions)  // (P18.1's: segment / sphere / floor collisions)
    for (const char* n : kNames)
        if (std::strcmp(n, name) == 0) return true;
    return std::strncmp(name, "own1422_update_s", 16) == 0;  // (P18.3 part 4: the boss's states' floor_probe_v)
}

// Veldin's class 749 hook +0x10 (the moby only) in the hook table's uniform form (T00: a level's copy of it)
TrRet e749_hook_10_uniform(uint32_t a0, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float,
                           float, float, float, float) {
    return TrRet{e749_hook_10(a0), 0.0f};
}

// The shared carry hook (shared_helpers.cpp) with a level-18 copy's constants (which hook, the carried state, the released
// one: as batch 506 checks these addresses), in the hook table's uniform form
template <uint32_t Which, uint32_t Carried, int32_t Released>
TrRet carry_state_uniform(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float f12, float, float,
                          float, float, float, float, float) {
    return TrRet{static_cast<uint32_t>(carry_state_hook(a0, a1, a2, f12, Which, Carried, Released)), 0.0f};
}

// Class 568's carry hooks +0 / +0xc / +0x10 (veldin_finale_walking_bomb.cpp) in the hook table's uniform form
template <uint32_t Which>
TrRet bomb_hook_uniform(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float f12, float, float, float,
                        float, float, float, float) {
    if constexpr (Which == 0) return TrRet{static_cast<uint32_t>(vf_bomb_hook0_l18(a0, a1, a2, f12)), 0.0f};
    else if constexpr (Which == 0xc) return TrRet{static_cast<uint32_t>(vf_bomb_hookc_l18(a0, a1, a2, f12)), 0.0f};
    else return TrRet{vf_bomb_hook10_l18(a0), 0.0f};
}

// Class 1906's carry hook +0xc (veldin_finale_robots.cpp) in the hook table's uniform form
TrRet robot_hookc_uniform(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float f12, float, float, float,
                          float, float, float, float) {
    return TrRet{static_cast<uint32_t>(vf_robot_hookc_l18(a0, a1, a2, f12)), 0.0f};
}

// A random state can hold a pointer outside RAM that the port's vector helpers dereference without a check (a SIGSEGV where the interpreter would wrap it): the trial run of a state
// turns it into "not a game state" (the state is dealt again), as the port's "Pointer resolution out of bounds" exception does.

// A check whose original is level 18's own function at `l9addr` (not a Veldin address to translate).
void l18check(const char* name, uint32_t l9addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && only_mismatch(only, name)) return;
    if (const char* skip = std::getenv("B270_SKIP"); skip != nullptr && (std::string(",") + skip + ",").find(std::string(",") + name + ",") != std::string::npos) return;  // (the mutation test: checks already seen to fail)
    if (g_oracle_level != 18) {
        std::printf("%-26s L18:0x%08x  skipped (level 18 only: run with ORACLE_LEVEL=18)\n", name, l9addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L18:0x%08x\n", name, l9addr);
    std::optional<CollisionScratchIgnored> spr_ignored;
    if (c2b_spr_check(name)) spr_ignored.emplace();
    if (!check_fn(name, l9addr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_270() {
    if (g_only_batch != 270) return;
    g_batch = 270;
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
            w32(0x0015ec8c, 1);  // the stream system up (the disc reads by command 0x38), the disc idle, as batches 75 / 112 do: the scenes' stream reads
            w32(0x00137b00, 0);  // (cutscene_start's stream_reads_wait) complete on both sides
            sub_00264bf8();
            snap.assign(ram, ram + 0x02000000);
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    // the level's mobys whose update function is `fn` (level 18's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60, std::vector<uint32_t> subjects = {}) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 18) { l18check(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && only_mismatch(only, name)) return;
        level();
        const std::vector<uint32_t> ms = subjects.empty() ? mobys_with(fn) : subjects;
        if (ms.empty()) { std::printf("%-26s L18:0x%08x  no moby on this level\n", name, fn); return; }
        l18check(name, fn, n, Ret::None, [level, ms, vary, port] {
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
    if (g_oracle_level == 18) {
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
        {  // the particle pool (0x800 records of 0x40 bytes at *(0x16016c)): the w lane of the velocity vector a spray copies (a stale lane of vec_from_spherical in the original)
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c);
        }
    }

    // ---- the level's own classes (once transliterated by tools/codegen/mkall_l18.py; hand ports now): the level's own mobys of each class, the first
    // update (state 0) done by the port, then a state, a hit, the hero near, the pvars' floats / counters moved as typed
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};  // (+ the level change request: the original would run the CD / movie path)
    struct TrCls { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t skip_states = 0; bool walk = false; uint32_t skip_p0 = 0; uint32_t skip_p1 = 0; };  // (skip_p0 / skip_p1: states skipped in pass 0 / 1 only, see the batch's notes)  // (the states that start a scene / a level change: the original runs the kernel / movie path for real)
    static const TrCls trcls[] = {
        {"class41", 0x002a7220, vf_class41_update_l18, 0x0, false},
        {"class49", 0x002a76e0, class49_update, 0x0, false},
        {"class50", 0x002a7b90, class50_update, 0x0, false},
        {"class256", 0x002aa3b0, class256_update, 0x0, false},
        {"class1427", 0x002ab7c0, shockwave_update, 0x0, false},
        {"class1523", 0x002abaa0, class1523_update, 0x0, false},
        {"class331", 0x002ce7d0, trackdrive_left_update_l2, 0x0, false},
        {"class341", 0x002ce800, class341_update, 0x0, false},
        {"class419", 0x002ceaa0, empty_update, 0x0, false},
        {"class564", 0x002d5050, vf_class564_update_l18, 0x0, false},
        {"class568", 0x002d5918, vf_class568_update_l18, 0x0, false},
        {"class582", 0x002d62e8, vf_class582_update_l18, 0x0, false},
        {"class583", 0x002d6600, vf_class583_update_l18, 0x0, false},
        {"class586", 0x002d7b20, vf_class586_update_l18, 0x0, false},
        {"class587", 0x002d82c0, vf_class587_update_l18, 0x0, false},
        {"class605", 0x002d8ba8, zone_marker_update, 0x0, false},
        {"class615", 0x002d8fd0, class615_update, 0x0, false},
        {"class624", 0x002db460, vf_class624_update_l18, 0x0, false},
        {"class628", 0x002dc260, vf_class628_update_l18, 0x0, false},
        {"class638", 0x002dc918, class638_update, 0x0, false},
        {"class644", 0x002df608, vf_class644_update_l18, 0x0, false},
        {"class696", 0x002dfda8, rock_update, 0x0, false},
        {"class830", 0x002e1508, class830_update, 0x0, false},
        {"class758", 0x002e19d8, swingshot_target_update, 0x0, false},
        {"class805", 0x002e1f40, spawn_point_update_l1, 0x0, false},
        {"class832", 0x002e4168, class832_update, 0x0, false},
        {"class885", 0x002e9768, vf_class885_update_l18, 0x0, false},
        {"class983", 0x002e9e70, vf_class983_update_l18, 0x0, false},
        {"class1021", 0x002ec3e8, class1021_update, 0x0, false},
        {"class1039", 0x002ec858, deadly_zones_update, 0x0, false},
        {"class1134", 0x002ecc40, gold_bolt_update, 0x4, false},
        {"class1135", 0x002edb78, teleporter_update_l1, 0x0, false},
        {"class1355", 0x002efb88, vf_class1355_update_l18, 0x0, false},
        {"class1356", 0x002f0920, class1356_update, 0x0, false},
        {"class1381", 0x002f16f0, vf_class1381_update_l18, 0x0, false},
        {"class1392", 0x002f1e68, vf_class1392_update_l18, 0x0, false},
        {"class1402", 0x002f2310, vf_class1402_update_l18, 0x0, false},
        {"class1422", 0x002f2bf0, vf_class1422_update_l18, 0xe00, false},
        {"class1432", 0x002f7ab0, vf_class1432_update_l18, 0x0, false},
        {"class1434", 0x002f7ad8, vf_class1434_update_l18, 0x0, false},
        {"class1454", 0x002f7c40, vf_class1454_update_l18, 0x0, false},
        {"class1563", 0x002f88e8, vf_class1563_update_l18, 0x0, false},
        {"class1568", 0x002fa640, vf_class1568_update_l18, 0x0, false},
        {"class1584", 0x002fa728, vf_class1584_update_l18, 0x0, false},
        {"class1750", 0x002fad08, vf_class1750_update_l18, 0x0, false},
        {"class1799", 0x002fad28, vf_class1799_update_l18, 0x0, false},
        {"class1890", 0x002fae48, class1443_update, 0x0, false},
        {"class1898", 0x002fb548, flash_update, 0x0, false},
        {"class1899", 0x002fb868, clank_pad_update, 0x0, false},
        {"class1906", 0x002fbb98, vf_class1906_update_l18, 0x0, false},
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
    if (g_oracle_level == 18 && std::getenv("B270_INFO") != nullptr) {  // the level's mobys of each class: how many, the nearest to the hero's start
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
        if (std::getenv("B270_CLS") != nullptr && std::strcmp(std::getenv("B270_CLS"), c.name) != 0) continue;
        for (int st = 0; st < 16; ++st)
        for (int pass = 0; pass < 3; ++pass) {
        if (g_oracle_level == 18 && std::getenv("B270_TAGS") != nullptr) std::printf("@@ %s st %d pass %d\n", c.name, st, pass);
        if ((std::getenv("B270_ST") == nullptr || std::atoi(std::getenv("B270_ST")) == st) && (std::getenv("B270_PASS") == nullptr || std::atoi(std::getenv("B270_PASS")) == pass) && (c.skip_states & (1u << st)) == 0 && !(pass == 0 && (c.skip_p0 & (1u << st)) != 0) && !(pass == 1 && (c.skip_p1 & (1u << st)) != 0)) classcheck(c.name, c.addr, c.fn, [st, pass, c, generic_prep](uint32_t m) { generic_prep(m, st, pass, c.fn, c.walk); }, 12);
        }
    }
    g_extra_noop_hooks.clear();
    if (std::getenv("B270_CLASSES_ONLY") != nullptr) { g_ignored_words = saved_ignored; g_ee_null_is_ram = null_ram; g_extra_hooks.clear(); g_host_clock_fixed = saved_clock; return; }  // (a quick run of the class checks alone)

    // ---- level 18's own moby callbacks (draw callbacks installed by `jal 0x1ecc00 / 0x1eca48`): a0 = the moby; the subjects are
    // the mobys of the class whose code installs it, after one update of the class in a state, a hit, the hero near
    if (g_oracle_level == 18) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        struct Cb { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t cls_addr; int cls; };
        // the display packet cursor (*0x1611c0) is null in the unit batches' memory: the draw helpers' packet writes land in low memory (0x100 .. 0x1500; the cursor is given a real buffer below) and carry a stale
        // callee-saved register (s7) of the original's frame: not game state
        const size_t ign_before = g_ignored_words.size();
        for (uint32_t a = 0x100; a < 0x1500; a += 4) g_ignored_words.push_back(a);
        static const Cb cbs[] = {
        {"cb2d5768", 0x002d5768, vf_cb_2d5768_l18, 0x002d5050, 564},
        {"cb2d8098", 0x002d8098, class1108_draw, 0x002d7b20, 586},
        {"cb2d98e8", 0x002d98e8, class615_puzzle_draw, 0x002d8fd0, 615},
        {"cb2dba20", 0x002dba20, vf_cb_2dba20_l18, 0x002db460, 624},
        {"cb2dc4b8", 0x002dc4b8, vf_cb_2dc4b8_l18, 0x002dc260, 628},
        {"cb2e3f58", 0x002e3f58, class832_draw, 0x002e4168, 832},
        {"cb2ea1f8", 0x002ea1f8, vf_cb_2ea1f8_l18, 0x002e9e70, 983},
        {"cb2eb9f8", 0x002eb9f8, class655_draw, 0x002e9e70, 983},
        {"cb2ee490", 0x002ee490, class1135_draw, 0x002edb78, 1135},
        {"cb2f0390", 0x002f0390, vf_cb_2f0390_l18, 0x002efb88, 1355},
        {"cb2f06c0", 0x002f06c0, vf_cb_2f06c0_l18, 0x002efb88, 1355},
        {"cb2f1fd8", 0x002f1fd8, vf_cb_2f1fd8_l18, 0x002f1e68, 1392},
        {"cb2f2620", 0x002f2620, [](uint32_t m) { finale_quads_draw(0, m); }, 0x002f2310, 1402},
        {"cb2f27c8", 0x002f27c8, [](uint32_t m) { finale_quads_draw(1, m); }, 0x002f2310, 1402},
        {"cb2f2970", 0x002f2970, [](uint32_t m) { finale_quads_draw(2, m); }, 0x002f2310, 1402},
        {"cb2f7880", 0x002f7880, vf_cb_2f7880_l18, 0x002f2bf0, 1422},
        {"cb2f9780", 0x002f9780, vf_cb_2f9780_l18, 0x002f88e8, 1563},
        {"cb2f9a98", 0x002f9a98, vf_cb_2f9a98_l18, 0x002f88e8, 1563},
        {"cb2f9c20", 0x002f9c20, vf_cb_2f9c20_l18, 0x002f88e8, 1563},
        {"cb2f9eb8", 0x002f9eb8, vf_cb_2f9eb8_l18, 0x002f88e8, 1563},
        {"cb2fb018", 0x002fb018, class1443_draw, 0x002fae48, 1890},
        {"cb2fb690", 0x002fb690, flash_draw_quad, 0x002fb548, 1898},
        {"cb2fc860", 0x002fc860, vf_cb_2fc860_l18, 0x002fbb98, 1906},
        {"cb2fca98", 0x002fca98, vf_cb_2fca98_l18, 0x002fbb98, 1906},
        };
        for (const Cb& cb : cbs) {
            if (std::getenv("B270_CLS") != nullptr && std::strcmp(std::getenv("B270_CLS"), cb.name) != 0) continue;
            level();
            std::vector<uint32_t> subj;
            if (cb.cls_addr != 0) subj = mobys_with(cb.cls_addr);
            else for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) if (ee_read<uint8_t>(m + 0x20) < 0xfe) subj.push_back(m);
            void (*clsfn)(uint32_t) = nullptr;
            for (const TrCls& c : trcls) if (c.addr == cb.cls_addr) clsfn = c.fn;
            if (subj.empty()) { std::printf("%-26s L18:0x%08x  no moby on this level\n", cb.name, cb.addr); continue; }
            // cb2fd6c0 / cb311d50: their GIF packet words carry a callee-saved register (s7 / s?) the callback reads on a path that never set it (the original's interpreter holds the
            // harness' leftover there, the port's local 0): the packets (the cursor's buffer) are not compared for these two, everything else is
            const size_t ign_pk = g_ignored_words.size();
            for (int st = 0; st < 4; ++st)
            for (int pass = 0; pass < 3; ++pass) {
                if (clsfn == nullptr && (st != 0 || pass != 0)) continue;
                classcheck(cb.name, cb.addr, cb.fn, [st, pass, clsfn, generic_prep](uint32_t m) { generic_prep(m, st, pass, clsfn != nullptr ? clsfn : vf_class41_update_l18, false); w32(OV(0x001611c0), 0x00800000u); }, 12, subj);  // (the display packet cursor: a real buffer for the draw callbacks)
            }
            g_ignored_words.resize(ign_pk);
        }
        g_ignored_words.resize(ign_before);
        g_extra_noop_hooks.clear();
    }

    // ---- level 18's sound emitter type 3 (section 5 of the overlay) on the level's own emitters
    if (g_oracle_level == 18) {
        struct Em { const char* name; uint32_t addr; void (*fn)(uint32_t); };
        static const Em ems_t[] = {{"emitter3", 0x00306dd0, emitter_reverb_box_update}, {"emitter5", 0x00306f30, emitter_camera_bed_update}, {"emitter6", 0x00306fe0, emitter_music_box_update}};
        for (const Em& e : ems_t) {
            level();
            std::vector<uint32_t> ems;
            for (int32_t i = 0; i < ee_read<int32_t>(0x0013f2e0); ++i) {
                const uint32_t em = ee_read<uint32_t>(0x0013f2e4) + static_cast<uint32_t>(i) * 0x90;
                if (ee_read<uint32_t>(em + 4) == e.addr) ems.push_back(em);
            }
            if (ems.empty()) { std::printf("%-26s L18:0x%08x  no emitter of the type on this level\n", e.name, e.addr); continue; }
            if (std::getenv("B270_INFO") != nullptr) std::printf("INFO %s 0x%08x emitters %zu\n", e.name, e.addr, ems.size());
            auto fn = e.fn;
            l18check(e.name, e.addr, 120, Ret::None, [level, ems] {
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


    // ---- level 18's class hooks (class 827's table at 0x1dde58): a0 = the moby, a1 = a position, a2 = a target moby, f12 a float
    // (the carry hooks' shape: their callees read all four), on the mobys whose class owns the hook
    if (g_oracle_level == 18) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        struct Hk { const char* name; uint32_t addr; uint32_t slot; TrRet (*fn)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float, float, float, float, float); };
        static const Hk hks[] = {
            {"hook_2d6108", 0x2d6108, 0x0, bomb_hook_uniform<0>},
            {"hook_2d6190", 0x2d6190, 0x4, carry_state_uniform<4, 4, -1>},
            {"hook_2d61e0", 0x2d61e0, 0x8, carry_state_uniform<8, 4, -1>},
            {"hook_2d6230", 0x2d6230, 0xc, bomb_hook_uniform<0xc>},
            {"hook_2d6250", 0x2d6250, 0x10, bomb_hook_uniform<0x10>},
            {"hook_2fcbb0", 0x2fcbb0, 0x0, carry_state_uniform<0, 7, 2>},
            {"hook_2fcc00", 0x2fcc00, 0x4, carry_state_uniform<4, 7, 2>},
            {"hook_2fcc50", 0x2fcc50, 0x8, carry_state_uniform<8, 7, 2>},
            {"hook_2fcca0", 0x2fcca0, 0xc, robot_hookc_uniform},
            {"hook_2fccd0", 0x2fccd0, 0x10, e749_hook_10_uniform},
        };
        for (const Hk& hk : hks) {
            level();
            std::vector<uint32_t> own;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
                const uint32_t cls = ee_read<uint32_t>(m + 0x24);
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + hk.slot) == hk.addr) own.push_back(m);
            }
            if (own.empty()) { std::printf("%-26s L18:0x%08x  no moby owns it on this level\n", hk.name, hk.addr); continue; }
            if (std::getenv("B270_INFO") != nullptr) std::printf("INFO %s 0x%08x owners %zu\n", hk.name, hk.addr, own.size());
            auto fn = hk.fn;
            l18check(hk.name, hk.addr, 40, Ret::Int32, [level, own] {
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

    // ---- level 18's movement-mode hero update (L18:0x0020efa8, 0x428 bytes): the hero in a movement mode (PLAYER+0x20a4 = 1 / 2 / 3) in one of the states of the mode
    if (g_oracle_level == 18) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        g_ignored_words.push_back(OV(0x0017431c));  // (a float whose zero's sign depends on a VU0 register left behind)
        g_ignored_words.push_back(OV(0x00173e9c));  // (the same class: the segment record's word (Veldin 0x1efc70's sqc2 into 0x173e90..) is -0 or +0 by the VU0 vf register the earlier checks left; passes alone, fails after the class checks)
        l18check("hero_move_mode", 0x0020efa8, 300, Ret::None, [level] {
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

    // ---- moby_spawn (the Veldin port) against level 18's own copy (the map's equivalent): the classes' spawns all go through it
    if (g_oracle_level == 18) {
        check("moby_spawn", 0x0024e838, 60, Ret::Int32, [level] {
            level();
            static const uint32_t cls[] = {0x29, 0x31, 0x32, 0x100, 0x238, 0x58e, 0x772};  // (classes of level 18's own table: 41, 49, 50, 256, 568, 1422, 1906)
            return Args{{cls[ri(0, 6)]}, {}};
        }, [](A a) { return r32(moby_spawn(a.i[0])); });
    }

    // ---- level 18's camera hook (0x305318, the pointer its setter 0x3052e0 puts at 0x167a10) and the callback it queues (0x305378)
    if (g_oracle_level == 18) {
        auto cam_prep = [level] {
            level();
            const uint32_t S = 0x00167a00;  // (level 18's hook block: its setter 0x3052e0 writes S .. S + 0x10)
            w32(S, ri(0, 3) == 0 ? R() : 0u);
            w32(S + 4, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
            wf(S + 8, rf(-3, 3));
            wf(S + 0xc, ri(0, 3) == 0 ? 0.0f : rf(-0.5f, 3));
            w32(S + 0x10, 0x00305318u);
            wf(0x00162794, rf(0, 0.5f));  // $gp - 0x446c: the frame step the callback counts down by
            w32(OV(0x0015ef4c), u(ri(0, 14)));  // the queue's count (it holds up to 16)
        };
        l18check("cam0_hook", 0x00305318, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_hook(0); return uint64_t{0}; });
        l18check("cam_callback", 0x00305378, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_callback(0); return uint64_t{0}; });
    }

    // ---- level 18's camera type 0x12's accept function (section 4; its init / update / exit are Veldin's), as batch 120 checks level 8's types: a live
    // camera of the type (or another live camera's words), the hero put in or near its box, its data varied
    if (g_oracle_level == 18) {
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
            if (ri(0, 3) != 0 && d != 0) {
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
            if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0) { any = true; if (ee_read<int16_t>(OV(0x00167150) + i * 0xa0 + 0x8c) == 0x12) own = true; }
        if (!any) std::printf("cam_type18                no live camera on this level\n");
        else {
            if (!own) std::printf("cam_type18                no live camera of the type on this level: checked on another live camera's words\n");
            l18check("cam_type18_accept", 0x00306370, 300, Ret::Int32, [cam_setup] { uint32_t other = 0; const uint32_t cam = cam_setup(0x12, true, &other); return Args{{cam, other}, {}}; },
                     [](A a) { return r32(a.i[0] != 0 ? static_cast<uint32_t>(cam_type18_accept(a.i[0], a.i[1])) : 0u); });
        }
    }

    // ---- class 1249 (Drek's NPC model in the level's scenes): Veldin's empty update (0x2df998, a `jr ra`), the level's copy L18:0x2eeb98
    if (g_oracle_level == 18) {
        check("drek_npc_update", 0x002df998, 20, Ret::None, [level, mobys_with] {
            level();
            const std::vector<uint32_t> ms = mobys_with(ORACLE_LADDR(18, 0x002eeb98));
            return Args{{ms.empty() ? 0x00180000u : ms[0]}, {}};
        }, [](A a) { drek_npc_update(a.i[0]); return uint64_t{0}; });
    }

    // T00: the level's bare `jr ra` functions its classes and cameras reach through a pointer (class updates, camera
    // callbacks; the scan pairs them with Veldin's 0x1f6bf8): empty_update runs in their places, each against the original
    // called directly (the level may have no moby of the class)
    if (g_oracle_level == 18) {
        static const uint32_t empties[] = {0x002ceaa0};
        for (const uint32_t a : empties) {
            char nm[24];
            std::snprintf(nm, sizeof nm, "empty_%x", a);
            l18check(nm, a, 20, Ret::None, [level] {
                level();
                return Args{{R(), R(), R(), R()}, {rf(-9, 9)}};
            }, [](A a) { empty_update(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- P18.1: the level's own classes hand-written (veldin_finale_shots / _walking_bomb / _rail_switch / _gate / _platforms.cpp),
    // in the states the class loop above cannot build: classes 41, 564, 624 and 628 have no moby at the level's start (the
    // troopers and the boss spawn them: here they are spawned by their own spawners, L18:0x2a75f0 / 0x2d5348 /
    // 0x2db938 / 0x2dc3e0, from points near the hero), and the placed classes' timers, flags, the hero on them and the level's
    // mode words are set as the game sets them. A third of the cases at 50 Hz. Each check runs the port of one function at
    // its level-18 address; B270_OWN_INFO=1 prints the placed subjects (the play-test spots).
    if (g_oracle_level == 18) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        constexpr uint32_t SCR = 0x00880000;  // (three scratch vectors the spawners read)
        constexpr uint32_t HERO = 0x0013f350;
        auto fifty = [] {
            if (ri(0, 2) != 0) return;
            wf(0x0015ed68, 1.2f);
            wf(0x0015ed6c, 0.02f);
            wf(0x0015ed70, 0.0004f);
        };
        auto put = [](uint32_t a, const Vec4& v) { VV(a) = v; };
        auto hero_at = [](const Vec4& q) {
            VV(0x0013f3d0) = q;
            const uint32_t hm = ee_read<uint32_t>(0x001413d0);
            if (hm != 0) VV(hm + 0x10) = q;
        };
        auto near = [](const Vec4& c, float lo, float hi, float dz) {
            Vec4 q = c;
            const float r = rf(lo, hi), a = rf(-3.1f, 3.1f);
            q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-dz, dz);
            return q;
        };
        // a check whose generator builds the subject (0: deal again), then the trial as the class loop's
        auto owncheck = [level](const char* name, uint32_t addr, void (*port)(uint32_t), std::function<uint32_t()> make, int n = 60) {
            if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && only_mismatch(only, name)) return;
            l18check(name, addr, n, Ret::None, [level, make, port] {
                static std::vector<uint8_t> saved;
                uint32_t m = 0;
                for (int attempt = 0; attempt < 60; ++attempt) {
                    level();
                    m = make();
                    if (m == 0) continue;
                    w32(OV(0x0015f5cc), R() & 0xffff);
                    seed_rand();
                    uint8_t* ram = ee_get_ram_base();
                    saved.assign(ram, ram + 0x02000000);
                    const uint32_t stack_top = ee_stack_ptr();
                    const bool ok = run_trial(port, m);
                    ee_stack_ptr() = stack_top;
                    std::memcpy(ram, saved.data(), saved.size());
                    if (ok) break;
                    if (std::getenv("B270_REDEAL") != nullptr) std::printf("  (re-dealt)\n");
                }
                return Args{{m}, {}};
            }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
        };
        auto pick = [](const std::vector<uint32_t>& v) { return v.empty() ? 0u : v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; };
        auto first_moby = [] { return ee_read<uint32_t>(OV(0x0015ffd8)); };
        // the moby's set-up (its state 0, the port's: at the snapshot the placed classes have not run it yet), as the class
        // loop's `walk`
        auto set_up = [](uint32_t m, void (*port)(uint32_t)) {
            w8(m + 0x20, 0);
            const uint32_t sp0 = ee_stack_ptr();
            try { port(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
        };
        if (std::getenv("B270_OWN_INFO") != nullptr) {
            level();
            for (const uint32_t c : {0x002d5918u, 0x002d62e8u, 0x002d6600u, 0x002d7b20u, 0x002d82c0u})
                for (const uint32_t m : mobys_with(c)) {
                    const Vec4 p = VV(m + 0x10);
                    std::printf("INFO spot class fn 0x%08x moby %u uid %d at %.1f %.1f %.1f\n", c, (m - first_moby()) >> 8,
                                static_cast<int>(ee_read<int16_t>(m + 0xb2)), p.x, p.y, p.z);
                }
        }

        // (the segment record's word OV(0x173e9c): -0 or +0 by the VU0 register segment_collide's original finds left by the earlier
        // checks (a known difference of the shared port, as hero_move_mode's above; seen in own41_rocket's case 26, and in
        // own564_shell's case 0 once P18.3 changed that generator's cases): left out of these two checks only)
        const size_t ign_seg = g_ignored_words.size();
        g_ignored_words.push_back(OV(0x00173e9c));
        // class 41, the troopers' rocket: from a trooper (class 1454) or another moby toward the hero, homing or not, its owner
        // given class 46 (its push from the owner), its life at its end or not, aimed at the ground or at the hero
        owncheck("own41_rocket", 0x002a7220, vf_class41_update_l18, [=] {
            fifty();
            const std::vector<uint32_t> tr = mobys_with(0x002f7c40);
            const uint32_t shooter = !tr.empty() && coin() ? pick(tr) : first_moby() + 0x100 * u(ri(0, 60));
            const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
            Vec4 at = near(h, 1, 12, 0); at.z = h.z + rf(0.3f, 3);
            Vec4 vel{(h.x - at.x) * rf(0.02f, 0.2f), (h.y - at.y) * rf(0.02f, 0.2f), coin() ? rf(-0.6f, 0.1f) : 0.0f, 0.0f};
            put(SCR, at);
            put(SCR + 0x10, vel);
            const uint32_t m = vf_rocket_spawn_l18(shooter, SCR, SCR + 0x10, u(ri(0, 1)));
            if (m == 0) return 0u;
            moby_update_transform(m);  // (its matrix, as the engine's moby pass makes it after the spawn's frame)
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w16(pv + 0x10, ri(0, 3) == 0 ? u(ri(0, 2)) : u(ri(3, 240)));
            if (coin()) { w16(shooter + 0xa6, 0x2e); if (ri(0, 3) == 0) w8(shooter + 0x20, coin() ? 0xfe : 0xfd); }
            if (ri(0, 6) == 0) w32(pv + 0x14, 0);  // (no owner)
            if (ri(0, 4) == 0) { Vec4 q = VV(m + 0x10); q.x += rf(-0.05f, 0.05f); q.y += rf(-0.05f, 0.05f); VV(0x0013f410) = q; }
            else if (coin()) VV(0x0013f410) = near(VV(m + 0x10), 0.5f, 6, 1);
            return m;
        });
        // class 564, the boss's shell: from above the boss / near the hero to a point by the hero, its frame anywhere in its
        // arc (its last too), every flag; across a class 419 moby; the hero dead, the level's mode 2, Giant Clank
        owncheck("own564_shell", 0x002d5050, vf_class564_update_l18, [=] {
            fifty();
            const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
            Vec4 from = near(h, 4, 20, 0), to = near(h, 0, 4, 0);
            from.z = h.z + rf(1, 10);
            to.z = h.z + rf(-1, 0.5f);
            if (ri(0, 2) == 0) {  // (its arc through a moby: class 419's, a gate's, a lift's, a floating platform's)
                static const uint32_t cls[] = {0x002ceaa0, 0x002d6600, 0x002d7b20, 0x002d82c0};
                const uint32_t o = pick(mobys_with(cls[ri(0, 3)]));
                if (o != 0) { to = VV(o + 0x10); to.z += rf(0, 1); from = near(to, 0.5f, 2, 0.5f); }
            }
            const Vec4 vel{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(0, 0.6f), 0};
            put(SCR, from); put(SCR + 0x10, vel); put(SCR + 0x20, to);
            const int frames = ri(10, 90);
            const uint32_t fl = u(ri(0, 7));
            const float f12 = rf(-1.2f, 1.2f), f13 = rf(-1.2f, 1.2f);
            const uint32_t m = vf_shell_spawn_l18(SCR, SCR + 0x10, SCR + 0x20, frames, fl, f12, f13);  // (the spawner: own1422_shell_spawn)
            if (m == 0) return 0u;
            moby_update_transform(m);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w32(pv + 0x2c, ri(0, 3) == 0 ? u(frames - 1) : u(ri(0, frames - 1)));
            if (ri(0, 4) == 0) {  // (from where its arc has it at its frame)
                Vec4 q = from; const float t = rf(0, 1); q.x += (to.x - from.x) * t; q.y += (to.y - from.y) * t; q.z += (to.z - from.z) * t; VV(m + 0x10) = q;
            }
            if (ri(0, 9) == 0) w32(0x001413d4, 0x72);
            if (ri(0, 9) == 0) w32(0x0015f5c4, 2);
            if (ri(0, 9) == 0) w8(m + 0x20, 1);
            w8(0x001413f4, coin() ? 2u : u(ri(0, 3)));
            return m;
        });
        g_ignored_words.resize(ign_seg);
        // (the segment record's words OV(0x173e90) .. 0x173e98: written by segment_collide's original (L18:0x1f8958, the store at
        // 0x1f995c, called by the ring's hurt 0x2dbe80: ORACLE_WATCH) from a VU0 register the earlier work left, the same store
        // as OV(0x173e9c)'s below; the known difference of the shared collision port in the segment records, COMMON.md; seen in
        // this check's case 30 once P18.3 made its generator spawn through vf_ring_spawn_l18, which changed the cases dealt):
        // left out of this check only)
        const size_t ign_ring = g_ignored_words.size();
        for (uint32_t k = 0; k < 12; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
        // class 624, the boss's ring: falling to the ground (from above it, or below z 10), waiting there, widening (its radius
        // under 3: the sparks, in view of the camera), the hero on its edge; past its end; the hero dead, the mode 2
        owncheck("own624_ring", 0x002db460, vf_class624_update_l18, [=] {
            fifty();
            const std::vector<uint32_t> boss = mobys_with(0x002f2bf0);
            const int st = ri(0, 3);
            const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
            Vec4 at = near(h, 0, 8, 0);
            at.z = st == 0 ? (ri(0, 6) == 0 ? rf(0, 9) : h.z + rf(-0.3f, 3)) : h.z + rf(-0.3f, 0.5f);
            if (st == 2 && coin()) {  // (the ring in view: the screen, the projection and the camera's view set up as batch 506's, the point moved about the camera)
                if (ee_read<uint16_t>(0x001518d0) == 0) { w16(0x001518d0, 640); w16(0x001518d2, 448); }  // (the oracle's display has no size)
                screen_setup();
                projection_setup();
                render_view_setup();
                const Vec4 c = VV(OV(0x00166dc0));
                for (int k = 0; k < 200; ++k) {
                    put(SCR + 0x30, Vec4{c.x + rf(-15, 15), c.y + rf(-15, 15), c.z + rf(-5, 5), 0});
                    if (point_on_screen(SCR + 0x30, 1.0f) != -1) { at = VV(SCR + 0x30); break; }
                }
            }
            put(SCR, at);
            put(SCR + 0x10, Vec4{rf(-0.2f, 0.2f), rf(-0.2f, 0.2f), rf(-0.3f, 0.2f), 0});
            const float maxr = rf(3, 16);
            const uint32_t owner = boss.empty() || coin() ? 0u : boss[0];
            vf_ring_spawn_l18(SCR, SCR + 0x10, owner, maxr, rf(0.05f, 0.5f));  // (the spawner: own1422_ring_spawn)
            const std::vector<uint32_t> rings = mobys_with(0x002db460);  // (the spawner returns no moby; the level has no other ring)
            if (rings.size() != 1) return 0u;
            const uint32_t m = rings[0];
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(st == 3 && ri(0, 2) != 0 ? ri(4, 6) : st));
            w32(pv + 0x2c, u(ri(0, 3)));
            wf(pv + 0x30, rf(-3.1f, 3.1f));
            const float r = coin() ? rf(0.4f, 3) : rf(3, maxr + 1);
            wf(pv + 0x24, r);
            if (coin()) { Vec4 q = near(at, r - 0.3f, r + 0.3f, 0); q.z = at.z; hero_at(q); }
            if (ri(0, 9) == 0) w32(0x001413d4, 0x72);
            if (ri(0, 9) == 0) w32(0x0015f5c4, 2);
            return m;
        });
        g_ignored_words.resize(ign_ring);
        // class 628, the boss's burst: about the hero (inside its radius or not), its life at its end or not, the level's
        // counter's bit 4 both ways (the counter is dealt by the check)
        owncheck("own628_burst", 0x002dc260, vf_class628_update_l18, [=] {
            fifty();
            const std::vector<uint32_t> boss = mobys_with(0x002f2bf0);
            const uint32_t owner = !boss.empty() && coin() ? boss[0] : first_moby() + 0x100 * u(ri(0, 60));
            const float r = rf(1, 6);
            put(SCR, near(ee_ref<Vec4>(0x0013f3d0), 0, r + 2, 0.5f));
            const uint32_t m = vf_burst_spawn_l18(owner, SCR, r);  // (the spawner: own1422_burst_spawn)
            if (m == 0) return 0u;
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w32(pv + 8, u(ri(0, 4)));
            wf(pv + 4, rf(0, 2));
            return m;
        });
        // class 568, the walking bombs: falling, sliding, walking (to the hero or another moby within 6), carried; the fuse
        // running, ending, out; hit (the mask 0x330000), held by the hero, at a target; the hero dead
        owncheck("own568_bomb", 0x002d5918, vf_class568_update_l18, [=] {
            fifty();
            const std::vector<uint32_t> ms = mobys_with(0x002d5918);
            const uint32_t m = pick(ms);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 3) == 0) set_up(m, vf_class568_update_l18);  // (at rest after it: hidden from the targets)
            const int st = ri(0, 9) == 0 ? (coin() ? 0 : 5) : ri(0, 2) == 0 ? 3 : ri(1, 4);
            w8(m + 0x20, u(st));
            w32(pv + 0x194, ri(0, 4) == 0 ? u(ri(0, 1)) : u(ri(2, 300)));
            w32(pv + 0x19c, ri(0, 6) == 0 ? 1u : 0u);
            if (ri(0, 8) == 0) w32(HERO + (coin() ? 0x23c : 0x240), m);
            if (ri(0, 2) == 0) {
                give_hit(m, 0x330000u);
                if (ee_read<uint8_t>(m + 0xa4) != 0xff && ri(0, 3) != 0) {  // (one kind of the mask alone, or one outside it)
                    static const uint32_t one[] = {0x100000, 0x20000, 0x100000, 0x20000, 0x10000, 0x200000, 0x400000};
                    const uint32_t e = OV(0x00178100) + (u(ee_read<uint8_t>(m + 0xa4)) << 6);
                    w32(e + 0x24, one[ri(0, 6)]);
                    w32(e + 0x34, m);
                }
            } else w8(m + 0xa4, 0xff);
            if (ri(0, 5) == 0) w32(0x001413d4, 0x72);
            const Vec4 p = VV(m + 0x10);
            switch (st) {
            case 1:
                wf(pv + 0x180, rf(-0.2f, 0.2f)); wf(pv + 0x184, rf(-0.2f, 0.2f)); wf(pv + 0x188, rf(-0.5f, 0.2f));
                wf(pv + 0x178, p.z + rf(-0.6f, 0.4f));
                if (coin()) hero_at(near(p, 0, 3, 0.5f));
                break;
            case 2:
                wf(pv + 0x188, ri(0, 3) == 0 ? 0.0f : rf(-0.2f, 0.2f));
                w8(m + 0x53, u(ri(0, 1)));
                break;
            case 3: {
                w8(m + 0x31, ri(0, 4) == 0 ? 0u : 1u);
                w8(m + 0x53, u(ri(0, 2)));
                w8(m + 0x70, u(R() & 3));
                wfrand(pv + 0x40, 3, -0.1f, 0.1f);
                wf(pv + 0x48, rf(-0.3f, 0.3f));
                if (coin()) hero_at(near(p, 0.5f, 7, 0.5f));
                else {
                    hero_at(near(p, 20, 40, 0.5f));  // (the hero out of its reach)  // (the bomb moved next to another live moby: a trooper's, the boss's, a class 1906 robot's, any)
                    static const uint32_t cls[] = {0x002f7c40, 0x002f2bf0, 0x002fbb98};
                    uint32_t o = pick(mobys_with(cls[ri(0, 2)]));
                    if (o == 0 || ri(0, 3) == 0) o = first_moby() + 0x100 * u(ri(0, 300));
                    if (ee_read<uint8_t>(o + 0x20) < 0xfe && o != m) VV(m + 0x10) = near(VV(o + 0x10), 0.5f, 5, 0.2f);
                }
                break;
            }
            default:
                break;
            }
            return m;
        }, 120);
        // class 568's hook +0 in each state (the class loop's hook check sees the bombs at rest): its arguments as the hook check's
        l18check("own568_hook0", 0x002d6108, 60, Ret::Int32, [=] {
            level();
            const uint32_t m = pick(mobys_with(0x002d5918));
            w8(m + 0x20, u(ri(0, 5)));
            w8(m + 0xbc, u(ri(0, 5)));
            w16(ee_read<uint32_t>(m + 0x78) + 0xc8, ri(0, 2) == 0 ? 8u : u(ri(0, 7)));  // (the carry record's state: 8 refuses)
            hero_at(near(VV(m + 0x10), 0, 4, 1));
            const uint32_t tgt = ri(0, 3) == 0 ? 0u : first_moby() + 0x100 * u(ri(0, 100));
            return Args{{m, ri(0, 2) == 0 ? 0x0013f3d0u : m + 0x10, tgt}, {rf(-1, 3)}};
        }, [](A a) { return r32(static_cast<uint32_t>(vf_bomb_hook0_l18(a.i[0], a.i[1], a.i[2], a.f[0]))); });
        // class 568's hook +0x10 without a moby or without its pvars, and on a bomb
        l18check("own568_hook10", 0x002d6250, 30, Ret::Int32, [=] {
            level();
            const int k = ri(0, 2);
            uint32_t m = 0;
            if (k != 0) {
                m = pick(mobys_with(0x002d5918));
                if (k == 1) w32(m + 0x78, 0);
            }
            return Args{{m}, {}};
        }, [](A a) { return r32(vf_bomb_hook10_l18(a.i[0])); });
        // class 582, the rail switch: the hero near each pair's ends, grinding on one of them or not
        owncheck("own582_rails", 0x002d62e8, vf_class582_update_l18, [=] {
            fifty();
            const uint32_t m = pick(mobys_with(0x002d62e8));
            if (m == 0) return 0u;
            set_up(m, vf_class582_update_l18);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, 1);
            for (uint32_t j = 0; j < 8; ++j) {  // (the pairs' rails open or shut at random: the switch's choice shows)
                const uint32_t r = ee_read<uint32_t>(pv + j * 4);
                if (r != 0) wf(r + 0xc, coin() ? 0.0f : rf(0.5f, 3));
            }
            const uint32_t k = u(ri(0, 3)), rail = ee_read<uint32_t>(pv + k * 8 + (coin() ? 4 : 0));
            if (rail != 0) hero_at(near(VV(ee_read<uint32_t>(rail + 0x10) + 0x10), 0, 20, 1));
            if (coin()) {
                w32(HERO + 0x208c, 0xf);
                if (rail != 0 && coin()) w32(HERO + 0x560, ee_read<uint32_t>(rail + 0x10));
            }
            return m;
        });
        // class 583, the gates: open or shut, the leaves at their opening or not, the gate up, down or between; its wait ending,
        // the level's mode 2 and the boss's phase, the hero in his state 0x22; standing on a moby
        owncheck("own583_gate", 0x002d6600, vf_class583_update_l18, [=] {
            fifty();
            const uint32_t m = pick(mobys_with(0x002d6600));
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            set_up(m, vf_class583_update_l18);
            w8(m + 0x20, ri(0, 9) == 0 ? 0u : 1u);
            const float goal = coin() ? 0.0f : rf(0.5f, 4);
            wf(pv + 0x24, goal);
            wf(pv + 0x20, coin() ? goal : rf(0, 4));
            wf(pv + 0x34, ri(0, 2) == 0 ? ee_read<float>(0x0016199c) : ri(0, 1) == 0 ? ee_read<float>(0x001619a0) : rf(-2, 4));
            wf(pv + 0x28, rf(-0.2f, 0.2f));
            w32(pv + 0x30, ri(0, 2) == 0 ? 1u : 0u);
            w32(pv + 0x2c, u(ri(0, 3)));
            w32(pv + 0x38, coin() ? 1u : 0u);
            w32(0x0015f5c4, coin() ? 2u : u(ri(0, 3)));
            w32(0x0016d290, u(ri(1, 6)));
            if (coin()) { w16(HERO + 0x30e, ri(0, 3) == 0 ? 1u : 0u); w32(0x001413d4, coin() ? 0x22u : u(ri(0, 0x30))); }
            return m;
        });
        // class 586, the lift and the pad: each state; the hero stepping on it (his state 0x22, standing on it) or near it; its
        // sound slot playing; the hint's dialogue record (heard or not, when) and the level's bit, the dialogue system idle in
        // half the cases (the request then goes through)
        owncheck("own586_lift", 0x002d7b20, vf_class586_update_l18, [=] {
            fifty();
            const uint32_t m = pick(mobys_with(0x002d7b20));
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            set_up(m, vf_class586_update_l18);
            const int st = ri(0, 2) == 0 ? 4 : ri(0, 6);
            w8(m + 0x20, u(st));
            if (st == 0) w32(pv + 0x60, coin() ? 1u : 0u);
            if (coin()) { w32(HERO + 0x2fc, m); w16(HERO + 0x30e, ri(0, 4) == 0 ? 1u : 0u); w32(0x001413d4, ri(0, 4) == 0 ? 0x72u : 0x22u); }
            hero_at(near(VV(m + 0x10), 0, ri(0, 2) != 0 ? 2.4f : 6, 0.5f));
            w32(pv + 0x6c, u(ri(0, 3)));
            if (coin()) {  // (its sound in a slot)
                const uint32_t slot = u(ri(0, 15)), e = 0x0013e550 + slot * 0x70;
                w32(pv + 0x70, slot);
                if (coin()) w32(e + 0x88, m);
                w8(e + 0x74, u(ri(0, 1)));
            } else w32(pv + 0x70, 0xffffffffu);
            const uint32_t lv = ee_read<uint32_t>(0x0015ed84) & 31;
            if (ri(0, 3) == 0) w32(0x0015717c, ee_read<uint32_t>(0x0015717c) | (1u << lv));
            else w32(0x0015717c, ee_read<uint32_t>(0x0015717c) & ~(1u << lv));
            w16(0x00141cf0, coin() ? 0u : u(ri(1, 3)));
            w16(0x00141cf2, ri(0, 3) == 0 ? 0u : u(ri(1, 200)));
            w32(0x0015eea4, coin() ? u(ri(0, 200000)) : u(ee_read<uint16_t>(0x00141cf2) * 600 + ri(0, 1500)));  // (the hint's wait over or not)
            if (coin()) {  // (the dialogue system idle: sub_00203908 takes the request)
                w32(OV(0x00179410), 0);
                w32(OV(0x00179410) + 0x24, 0xffffffffu);
                w32(0x001516d0 + 0x50, 0);
                w32(0x001516d0 + 0x1c, 0xffffffffu);
            }
            return m;
        }, 120);
        // class 587, the floating platforms: each state; the flood anywhere about its range; each kind (+0x80 / +0x84 / +0x88);
        // the bubbles due; the hero on it (under the water's level, in his state 0x77 or not) or near it; sunk below its start
        // by 10 and below z 15; its rocking past its limits
        owncheck("own587_float", 0x002d82c0, vf_class587_update_l18, [=] {
            fifty();
            const uint32_t m = pick(mobys_with(0x002d82c0));
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            set_up(m, vf_class587_update_l18);
            const int st = ri(0, 5);
            w8(m + 0x20, u(st));
            w8(m + 0x31, ri(0, 4) == 0 ? 0u : 1u);
            w32(pv + 0x8c, u(ri(0, 2)));
            w32(pv + 0x7c, u(ri(0, 2)));
            for (const uint32_t o : {0x80u, 0x84u, 0x88u}) w32(pv + o, coin() ? 1u : 0u);
            const float lo = ee_read<float>(0x00161a60), hi = ee_read<float>(0x00161a64);
            w32(0x001623a8, u(static_cast<int32_t>(rf(lo - (hi - lo) * 0.3f, hi + (hi - lo) * 0.3f))));
            wf(pv + 0x6c, rf(-0.6f, 0.6f));
            wf(m + 0x44, rf(-0.6f, 0.6f));
            wf(pv + 0x70, rf(-0.3f, 0.1f));
            if (st == 5 && ri(0, 3) != 0) {  // (sunk below its start by 10: some below z 15)
                const float z = coin() ? rf(5, 15) : ee_read<float>(pv + 0x60) - rf(10, 14);
                wf(m + 0x18, z);
            }
            if (coin()) {
                w32(HERO + 0x2fc, m);
                w16(HERO + 0x30e, ri(0, 4) == 0 ? 1u : 0u);
                const Vec4 p = VV(m + 0x10);
                Vec4 q = near(p, 0, 1, 0);
                q.z = coin() ? ee_read<float>(0x0015f638) - rf(0.1f, 2) : p.z + 1;
                hero_at(q);
                if (ri(0, 4) == 0) w32(0x001413d4, 0x77);
            } else hero_at(near(VV(m + 0x10), 0, 8, 1));
            return m;
        });

        // ---- P18.2: classes 644, 885, 983, 1355, 1381, 1392, 1402 hand-written (veldin_finale_camera_pan / _breakable /
        // _energy_ball / _diver / _raft / _barrier / _water.cpp): each class through its states as the level sets them, each
        // helper at its own address on the same states. Class 983 has no moby at the level's start: the balls are spawned
        // by the level's own spawner (the generated L18:0x2ea0f0) and launched by L18:0x2ea168, from points near the hero.
        // The 1.02 fixes 45 / 46 have their own checks (run with RAC1_FIXES102=45,46).
        auto class_moby = [](int32_t cls) {  // the level's mobys of class `cls` (+0xa6)
            std::vector<uint32_t> v;
            for (uint32_t o = ee_read<uint32_t>(0x0015ffd8); o < ee_read<uint32_t>(0x0015ffe0); o += 0x100)
                if (ee_read<uint8_t>(o + 0x20) < 0xfe && ee_read<int16_t>(o + 0xa6) == cls) v.push_back(o);
            return v;
        };
        auto any_moby = [=] { return first_moby() + 0x100 * u(ri(0, 300)); };
        auto timer = [](uint32_t a) { w32(a, ri(0, 3) == 0 ? u(ri(0, 2)) : u(ri(3, 200))); };  // (0 idle, 1 ending, more running)
        auto dialogue_idle = [] {  // (the dialogue system idle: sub_00203908 takes the request)
            w32(0x00179410, 0);
            w32(0x00179410 + 0x24, 0xffffffffu);
            w32(0x001516d0 + 0x50, 0);
            w32(0x001516d0 + 0x1c, 0xffffffffu);
        };
        auto sound_slot = [](uint32_t m, uint32_t word) {  // a sound channel in the word: playing on `m` or not, or none
            if (coin()) {
                const uint32_t slot = u(ri(0, 15)), e = 0x0013e550 + slot * 0x70;
                w32(word, slot);
                if (coin()) w32(e + 0x88, m);
                w8(e + 0x74, u(ri(0, 1)));
            } else w32(word, 0xffffffffu);
        };
        // a helper of one of these classes at its own address, on the states of its class's generator
        auto helpercheck = [level](const char* name, uint32_t addr, Ret ret, std::function<Args()> make, std::function<uint64_t(A)> port,
                                   int n = 60) {
            if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && only_mismatch(only, name)) return;
            l18check(name, addr, n, ret, [level, make] {
                for (int attempt = 0; attempt < 60; ++attempt) {
                    level();
                    Args a = make();
                    if (a.i.empty() || a.i[0] == 0) continue;
                    w32(0x0015f5cc, R() & 0xffff);
                    seed_rand();
                    return a;
                }
                return Args{{0}, {}};
            }, port);
        };
        if (std::getenv("B270_OWN_INFO") != nullptr) {
            level();
            for (const uint32_t c : {0x002df608u, 0x002e9768u, 0x002efb88u, 0x002f16f0u, 0x002f1e68u, 0x002f2310u})
                for (const uint32_t m : mobys_with(c)) {
                    const Vec4 p = VV(m + 0x10);
                    std::printf("INFO spot class fn 0x%08x moby %u uid %d class %d at %.1f %.1f %.1f\n", c, (m - first_moby()) >> 8,
                                static_cast<int>(ee_read<int16_t>(m + 0xb2)), static_cast<int>(ee_read<int16_t>(m + 0xa6)), p.x, p.y, p.z);
                }
        }

        // class 644, the camera's move: each state; the fade near its ends; the move's progress at its start, middle, a fifth,
        // its end; its wait; Clank's line or not (the dialogue system idle in half); a talk running or not; the hero held or not
        auto make644 = [=]() -> uint32_t {
            const std::vector<uint32_t> ms = mobys_with(0x002df608);
            if (ms.empty()) return 0u;
            fifty();
            const uint32_t m = pick(ms), pv = ee_read<uint32_t>(m + 0x78);
            const int st = ri(0, 5) == 0 ? ri(0, 6) : ri(2, 5);
            w8(m + 0x20, u(st));
            const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
            put(pv + 0x00, near(h, 2, 12, 3));
            put(pv + 0x10, near(h, 2, 12, 3));
            for (uint32_t o = 0x20; o < 0x40; o += 4) wf(pv + o, rf(-3.1f, 3.1f));
            put(pv + 0x40, near(h, 0, 4, 0.5f));
            static const float ts[] = {0.0f, 0.1f, 0.19f, 0.21f, 0.5f, 0.98f, 1.0f, 1.2f};
            wf(pv + 0x60, coin() ? ts[ri(0, 7)] : rf(0, 1.1f));
            wf(pv + 0x64, rf(-0.05f, 0.1f));
            wf(pv + 0x68, rf(0.0005f, 0.01f));
            static const float fs[] = {0.0f, 0.01f, 0.5f, 0.95f, 0.99f, 1.0f};
            wf(pv + 0x6c, coin() ? fs[ri(0, 5)] : rf(0, 1));
            wf(pv + 0x70, rf(0.01f, 0.1f));
            timer(pv + 0x74);
            w32(pv + 0x78, coin() ? 1u : 0u);
            wf(0x0015f3fc, rf(0, 1));
            w32(0x00179e10, ri(0, 2) == 0 ? 1u : 0u);
            w32(0x00179e34, coin() ? 0xffffffffu : u(ri(0, 8)));
            w32(0x001413d4, coin() ? 0x72u : u(ri(0, 0x30)));
            if (coin()) dialogue_idle();
            return m;
        };
        owncheck("own644_pan", 0x002df608, vf_class644_update_l18, make644, 120);

        // class 885, the breakable pieces: set up (gone for good or not, a record to mark or none), hit by the mask 0x80000 or
        // not, broken (class 892 or not), flying (near the ground, below its start by 16, in the open)
        auto make885 = [=]() -> uint32_t {
            const std::vector<uint32_t> ms = mobys_with(0x002e9768);
            if (ms.empty()) return 0u;
            fifty();
            const uint32_t m = pick(ms), pv = ee_read<uint32_t>(m + 0x78);
            const int st = ri(0, 7) == 0 ? 4 : ri(0, 3);
            w8(m + 0x20, u(st));
            if (st == 0) {
                const uint32_t k = coin() ? 0xffu : u(ri(0, 15));
                w8(m + 0xb0, k);
                if (k != 0xff) w8(0x0014c050 + (ee_read<uint32_t>(0x0015ed84) << 4) + k, coin() ? 0xffu : u(ri(0, 3)));
                w32(pv + 0, coin() ? SCR + 0x40 : 0u);
            }
            if (st == 1) {
                give_hit(m, 0x80000);
                if (ee_read<uint8_t>(m + 0xa4) != 0xff) w32(OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6) + 0x34, m);
            }
            if (st == 2 && coin()) w16(m + 0xa6, 0x37c);
            if (st == 3) {
                put(pv + 0x10, Vec4{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), 0});
                wf(pv + 8, rf(-0.1f, 0.1f));
                const float z = ee_read<float>(m + 0x18);
                wf(pv + 0x1c, ri(0, 2) == 0 ? z + rf(15.5f, 17) : z + rf(-2, 2));
                if (coin()) wf(m + 0x18, z - rf(0, 1.5f));
            }
            hero_at(near(VV(m + 0x10), 0, 8, 1));
            return m;
        };
        owncheck("own885_piece", 0x002e9768, vf_class885_update_l18, make885, 120);
        helpercheck("own885_group_break", 0x002e9e50, Ret::None, [=] {
            const std::vector<uint32_t> ms = mobys_with(0x002e9768);
            return Args{{ms.empty() || coin() ? u(ri(1, 40)) : u(ee_read<uint8_t>(pick(ms) + 0x21))}, {}};
        }, [](A a) { vf_group_break_l18(a.i[0]); return uint64_t{0}; }, 30);

        // class 983, the energy ball: spawned near the hero by its owner (the boss, another moby, none), launched at a point (near
        // enough to blow up this frame, or far), its life running or out, its size, its arc started again or not, the level bright or
        // not; state 0's wait; the hero in its way
        auto make983 = [=]() -> uint32_t {
            fifty();
            const std::vector<uint32_t> boss = mobys_with(0x002f2bf0);
            const uint32_t owner = !boss.empty() && coin() ? boss[0] : ri(0, 3) == 0 ? 0u : any_moby();
            const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
            Vec4 from = near(h, 1, 10, 0);
            from.z = h.z + rf(0.2f, 4);
            put(SCR, from);
            const uint32_t m = vf_ball_spawn_l18(owner, SCR, u(ri(0, 3) == 0 ? ri(0, 2) : ri(3, 300)));  // (the spawner: own1422_ball_spawn)
            if (m == 0) return 0u;
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            Vec4 to = ri(0, 3) == 0 ? near(from, 0, 0.3f, 0.1f) : coin() ? h : near(h, 0, 15, 2);
            put(SCR + 0x10, to);
            const uint32_t go = ri(0, 5) == 0 ? 0u : 1u;
            vf_ball_launch_l18(m, SCR, SCR + 0x10, go, coin() ? rf(0.2f, 2) : 1.0f);  // (own1422_ball_launch)
            moby_update_transform(m);
            if (ri(0, 4) == 0) w8(m + 0x20, u(ri(0, 3)));
            if (coin()) wf(pv + 0x24, ri(0, 3) == 0 ? rf(0, 0.25f) : rf(0.3f, 2));
            if (coin()) timer(pv + 0x34);
            if (coin()) timer(pv + 0x28);
            w32(0x00161fec, ri(0, 2) == 0 ? 0u : 1u);
            w8(0x0013e533, coin() ? 1u : 0u);
            if (coin()) hero_at(near(VV(m + 0x10), 0, 2.5f, 0.5f));
            return m;
        };
        owncheck("own983_ball", 0x002e9e70, vf_class983_update_l18, make983, 120);
        // its helpers on the same balls (the arc's: started again first in half, as the update runs them)
        auto ball_args = [=] { const uint32_t m = make983(); return Args{{m}, {}}; };
        helpercheck("own983_fizz", 0x002ea598, Ret::None, ball_args, [](A a) { vf_ball_fizz_l18(a.i[0]); return uint64_t{0}; });
        helpercheck("own983_blast", 0x002ea800, Ret::None, [=] {
            const Args a = ball_args();
            if (coin()) {  // (the shards' and the shake's sizes either side of 0, the sprays' counts small)
                for (const uint32_t o : {0x00161fc0u, 0x00161fccu, 0x00161fd8u, 0x00161fe4u}) wf(o, coin() ? 0.0f : rf(0.1f, 2));
                w32(0x00161f6c, u(ri(0, 4)));
                w32(0x00161f98, u(ri(0, 4)));
            }
            return a;
        }, [](A a) { vf_ball_blast_l18(a.i[0]); return uint64_t{0}; });
        helpercheck("own983_arc_reset", 0x002eacd8, Ret::None, ball_args, [](A a) { vf_ball_arc_reset_l18(a.i[0]); return uint64_t{0}; });
        helpercheck("own983_arc", 0x002eaea0, Ret::None, [=] {
            const Args a = ball_args();
            if (a.i[0] != 0 && coin()) {
                w32(0x00161fec, 1);
                vf_ball_arc_reset_l18(a.i[0]);
                for (uint32_t j = 0; j < 4; ++j) w16(0x001620b8 + j * 2, u(ri(0, 3)));  // (some branches re-form, some flicker)
            }
            return a;
        }, [](A a) { vf_ball_arc_l18(a.i[0]); return uint64_t{0}; });

        // class 1355, the diver: set up, waiting, roaming (its point reached or not, its time up, the moby it keeps off near or
        // not), diving (above or below its home's height), pitched up with its wait ending; the trail's frame and count; the level's
        // mode 2 at the boss's phase 3; a dive's odds 1 in 1
        auto make1355 = [=]() -> uint32_t {
            const std::vector<uint32_t> ms = mobys_with(0x002efb88);
            if (ms.empty()) return 0u;
            fifty();
            const uint32_t m = pick(ms), pv = ee_read<uint32_t>(m + 0x78);
            set_up(m, vf_class1355_update_l18);
            const int st = ri(0, 6) == 0 ? ri(0, 1) : ri(2, 4);
            w8(m + 0x20, u(st));
            const Vec4 home = VV(pv + 0x120);
            Vec4 at = near(home, 0, 25, 0);
            at.z = home.z + rf(-1, 12);
            put(m + 0x10, at);
            wf(m + 0x48, rf(-3.1f, 3.1f));
            wf(m + 0x44, rf(-1.5f, 1.5f));
            moby_update_transform(m);
            if (coin()) put(pv + 0x110, near(at, 0, 2.5f, 1)); else put(pv + 0x110, near(home, 5, 20, 5));
            timer(pv + 0x160);
            if (coin()) { const uint32_t o = any_moby(); w32(pv + 0x16c, o); if (coin()) put(o + 0x10, near(at, 0, 5, 1)); }
            else w32(pv + 0x16c, coin() ? 0u : ee_read<uint32_t>(0x001413d0));
            wf(pv + 0x14c, rf(0, 1));
            put(pv + 0x130, Vec4{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(-0.3f, 0.1f), 0});
            if (st == 3 && coin()) wf(m + 0x18, home.z + rf(-0.5f, 0.8f));
            w16(pv + 0x164, u(ri(0, 5)));
            w16(pv + 0x166, u(ri(0, 15)));
            w32(0x00162230, coin() ? 0u : 0xffffffffu);
            if (ri(0, 3) == 0) w32(0x00162234, 1);
            if (ri(0, 6) == 0) { w32(0x0015f5c4, 2); w32(0x0016d290, coin() ? 3u : 2u); }
            return m;
        };
        owncheck("own1355_diver", 0x002efb88, vf_class1355_update_l18, make1355, 120);
        helpercheck("own1355_step", 0x002f0120, Ret::Int32, [=] {
            const uint32_t m = make1355();
            return Args{{m, m == 0 ? 0u : ee_read<uint32_t>(m + 0x78) + 0x110}, {}};
        }, [](A a) { return uint64_t{vf_diver_step_l18(a.i[0], a.i[1])}; });
        helpercheck("own1355_new_point", 0x002f02e8, Ret::None, [=] {
            const uint32_t m = make1355();
            return Args{{m, m == 0 ? 0u : ee_read<uint32_t>(m + 0x78) + 0x110}, {}};
        }, [](A a) { vf_diver_new_point_l18(a.i[0], a.i[1]); return uint64_t{0}; });
        helpercheck("own1355_trail", 0x002f0848, Ret::None, [=] { return Args{{make1355()}, {}}; },
                    [](A a) { vf_diver_trail_l18(a.i[0]); return uint64_t{0}; });

        // class 1381, the raft: set up, waiting, gliding (the level's word on or off, its progress about 1.5, the rocks' and the
        // end's marks, the odds of a rock), the hero on one of its platforms, on a class 1892 moby, on another raft (sinking or
        // not), on another moby or on none, in the air or not, its wait ending; sinking (above or below height 20)
        auto make1381 = [=]() -> uint32_t {
            const std::vector<uint32_t> ms = mobys_with(0x002f16f0);
            if (ms.empty()) return 0u;
            fifty();
            const uint32_t m = pick(ms), pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 5) != 0) set_up(m, vf_class1381_update_l18);
            const int st = ri(0, 5) == 0 ? ri(0, 5) : (coin() ? 2 : ri(1, 4));
            w8(m + 0x20, u(st));
            w32(0x00162288, ri(0, 3) == 0 ? 0u : 1u);
            static const float ps[] = {0.0f, 0.5f, 1.4999f, 1.5f, 1.9f, 2.0f};
            wf(pv + 0x330, coin() ? ps[ri(0, 5)] : rf(0, 2));
            if (coin()) wf(pv + 0x330, ee_read<float>(coin() ? 0x001622b8u : 0x001622b4u) + rf(-0.05f, 0.05f));
            wf(pv + 0x334, rf(-0.02f, 0.05f));
            wf(pv + 0x344, ri(0, 2) == 0 ? 0.0f : rf(1, 12));
            w32(pv + 8, coin() ? 1u : 0u);
            w32(pv + 0x348, coin() ? 1u : 0u);
            w32(pv + 0x350, u(ri(0, 40)));
            timer(pv + 0x34c);
            wf(pv + 0x340, rf(-0.5f, 0.2f));
            if (st == 4 && coin()) wf(m + 0x18, 20.0f + rf(-0.6f, 0.6f));
            const int n = ee_read<int32_t>(pv + 0x338);
            const int who = ri(0, 5);
            uint32_t under = 0;
            if (who == 0 && n > 0) under = first_moby() + (ee_read<uint32_t>(pv + 0x50 + u(ri(0, n - 1)) * 0x30) << 8);
            else if (who == 1) { const std::vector<uint32_t> g = class_moby(0x764); under = g.empty() ? 0u : pick(g); }
            else if (who == 2) { under = pick(ms); if (coin()) w8(under + 0x20, 4); }
            else if (who == 3) under = any_moby();
            w32(HERO + 0x2fc, under);
            w16(HERO + 0x30e, ri(0, 3) == 0 ? 1u : 0u);
            return m;
        };
        owncheck("own1381_raft", 0x002f16f0, vf_class1381_update_l18, make1381, 160);
        helpercheck("own1381_platform", 0x002fa8e0, Ret::None, [=] {
            const uint32_t m = make1381();
            if (m == 0) return Args{{0}, {}};
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const int n = ee_read<int32_t>(pv + 0x338);
            uint32_t o = n > 0 && ri(0, 3) != 0 ? first_moby() + (ee_read<uint32_t>(pv + 0x50 + u(ri(0, n - 1)) * 0x30) << 8) : any_moby();
            if (ee_read<int16_t>(o + 0xa6) == 0x630) w32(ee_read<uint32_t>(o + 0x78) + 0x84, coin() ? any_moby() : 0u);  // (its +0x84 moby, or none)
            return Args{{o}, {}};
        }, [](A a) { vf_raft_platform_retire_l18(a.i[0]); return uint64_t{0}; });

        // class 1392, the force field: set up, standing (its hum playing or not, its switch none / not a switch / below its state 3
        // / at it or past), down (fading)
        auto make1392 = [=]() -> uint32_t {
            const std::vector<uint32_t> ms = mobys_with(0x002f1e68);
            if (ms.empty()) return 0u;
            fifty();
            const uint32_t m = pick(ms), pv = ee_read<uint32_t>(m + 0x78);
            const int st = ri(0, 5) == 0 ? ri(0, 3) : 1;
            w8(m + 0x20, u(st));
            const std::vector<uint32_t> sws = class_moby(0x267);
            const int k = ri(0, 4);
            if (k == 0) w32(pv + 0, 0xffffffffu);
            else if (k == 1) w32(pv + 0, (any_moby() - first_moby()) >> 8);
            else if (!sws.empty()) {
                const uint32_t o = ee_read<uint32_t>(pv + 0) != 0xffffffffu && coin() ? first_moby() + (ee_read<uint32_t>(pv + 0) << 8) : pick(sws);
                w32(pv + 0, (o - first_moby()) >> 8);
                w8(o + 0x20, u(ri(0, 5)));
            }
            sound_slot(m, pv + 4);
            wf(pv + 0xc, rf(0, 1));
            return m;
        };
        owncheck("own1392_field", 0x002f1e68, vf_class1392_update_l18, make1392, 120);

        // class 1402, the water: set up (the level's word set or not), its first frame (the flag set or not, its turn, the hero's
        // byte), toward +0 / +4 (the hit bits 4 / 8, its turn), each kind of draw (the ripple's phase about 1); the hero swimming
        auto make1402 = [=]() -> uint32_t {
            const std::vector<uint32_t> ms = mobys_with(0x002f2310);
            if (ms.empty()) return 0u;
            fifty();
            const uint32_t m = pick(ms), pv = ee_read<uint32_t>(m + 0x78);
            const int st = ri(0, 7) == 0 ? 4 : ri(0, 3);
            w8(m + 0x20, u(st));
            w32(0x001622fc, ri(0, 2) == 0 ? 0u : (coin() ? 1u : 0x101u));
            w32(0x001622f8, u(ri(0, 3)));
            w8(0x0013d406, coin() ? 1u : 0u);
            w8(0x00141400, ri(0, 2) == 0 ? 1u : 0u);
            w32(pv + 8, coin() ? 1u : 0u);
            w8(m + 0xbc, ri(0, 2) == 0 ? u(ri(0, 15)) : (coin() ? 4u : 8u));
            w32(pv + 0x14, ri(0, 6) == 0 ? u(ri(3, 4)) : u(ri(0, 2)));
            wf(0x00162308, rf(0.8f, 1.0f));
            const float lo = ee_read<float>(pv + 0), hi = ee_read<float>(pv + 4);
            wf(m + 0x18, coin() ? lo + (hi - lo) * rf(0, 1) : (coin() ? lo : hi) + rf(-0.15f, 0.15f));
            wf(pv + 0x18, rf(-0.05f, 0.05f));
            if (coin()) { w32(HERO + 0x208c, coin() ? 0x11u : 0x12u); wf(HERO + 0x2f0, ee_read<float>(m + 0x18) + rf(-1, 1)); }
            return m;
        };
        owncheck("own1402_water", 0x002f2310, vf_class1402_update_l18, make1402, 120);
        helpercheck("own1402_group", 0x002f2598, Ret::None, [=] { return Args{{make1402()}, {}}; },
                    [](A a) { vf_water_group_l18(a.i[0]); return uint64_t{0}; });

        // 1.02 fixes 45 and 46 (the PAL 2.00 disc's own code, ported): the port with the fix on against the port with it off on
        // an equivalent state, the fix's own words set as its 2.00 code sets them. Run with RAC1_FIXES102=45,46.
        // - fix 45, the raft: set up, its wait is frame_scale_i(5) and its +0x350 0; gliding with the hero on a platform, the
        //   frames ridden counted and ridden only past frame_scale_i(30) (the port without the fix, which takes him for a rider at
        //   once, run with the raft's +0x348 as the fix leaves it); on a sinking raft of its class held (the port without the fix
        //   run with the hero in the air, +0x30e, put back); otherwise the same
        // - fix 46, the water toward +0: Ratchet swimming below its surface gets its height when it is more than 0.1 from +0
        auto fixcheck = [=](const char* name, int fix, int cases, const std::function<uint32_t()>& make,
                            const std::function<int(uint32_t, std::vector<uint8_t>&)>& expect) {
            if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && only_mismatch(only, name)) return;
            const char* fx = std::getenv("RAC1_FIXES102");
            if (fx == nullptr || std::string(fx).find(std::to_string(fix)) == std::string::npos) {
                std::printf("%-26s skipped (run with RAC1_FIXES102=%d)\n", name, fix);
                ++g_skipped;
                return;
            }
            int bad = 0, n = 0, differ = 0;
            static std::vector<uint8_t> start, with_fix, expected;
            const std::unordered_set<uint32_t> ignored(g_ignored_words.begin(), g_ignored_words.end());
            for (int c = 0; c < cases; ++c) {
                level();
                const uint32_t m = make();
                if (m == 0) continue;
                w32(0x0015f5cc, R() & 0xffff);
                seed_rand();
                uint8_t* ram = ee_get_ram_base();
                start.assign(ram, ram + 0x02000000);
                g_advanced_disabled = false;
                if (fix == 45) vf_class1381_update_l18(m); else vf_class1402_update_l18(m);
                g_advanced_disabled = true;
                with_fix.assign(ram, ram + 0x02000000);
                std::memcpy(ram, start.data(), start.size());
                differ += expect(m, expected);
                ++n;
                for (uint32_t a = 0x00100000; a < 0x02000000; a += 4) {
                    if (ignored.count(a) != 0) continue;
                    if (a >= ee_stack_ptr() - 0x10000 && a < ee_stack_ptr() + 0x10000) continue;
                    uint32_t xw, yw;
                    std::memcpy(&xw, &with_fix[a], 4); std::memcpy(&yw, &expected[a], 4);
                    if (xw != yw) { if (bad < 8) std::printf("  %s case %d: %08x fix=%08x expected=%08x\n", name, c, a, xw, yw); ++bad; break; }
                }
            }
            std::printf("%-26s %s (%d cases, %d where the fix changes the result)\n", name, bad == 0 && differ > 0 ? "ok" : "FAIL", n, differ);
            if (bad != 0 || differ == 0) ++g_fail;
        };
        fixcheck("own1381_fix45", 45, 200, [=]() -> uint32_t {
            const uint32_t m = make1381();
            if (m == 0) return 0u;
            if (ri(0, 4) == 0) w8(m + 0x20, 0);
            else if (ri(0, 4) != 0) w8(m + 0x20, 2);
            return m;
        }, [=](uint32_t m, std::vector<uint8_t>& out) {
            // the expected result: the port without the fix on the state the fix's test makes equivalent, then the fix's words
            uint8_t* ram = ee_get_ram_base();
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const int st = ee_read<uint8_t>(m + 0x20);
            int changed = 0;
            if (st == 0) {
                vf_class1381_update_l18(m);
                if (ee_read<uint8_t>(m + 0x20) == 1) { w32(pv + 0x34c, u(frame_scale_i(5))); w32(pv + 0x350, 0); ++changed; }
                out.assign(ram, ram + 0x02000000);
                return changed;
            }
            if (st != 2) { vf_class1381_update_l18(m); out.assign(ram, ram + 0x02000000); return 0; }
            const int n = ee_read<int32_t>(pv + 0x338);
            const uint32_t under = ee_read<uint32_t>(HERO + 0x2fc);
            bool on_platform = false;
            for (int k = 0; k < n; ++k)
                if (under == first_moby() + (ee_read<uint32_t>(pv + 0x50 + u(k) * 0x30) << 8)) on_platform = true;
            const bool on_sinking = under != 0 && ee_read<int16_t>(under + 0xa6) == ee_read<int16_t>(m + 0xa6) && ee_read<uint8_t>(under + 0x20) == 4;
            if (on_platform) {
                const uint32_t ridden = ee_read<uint32_t>(pv + 0x350) + 1;
                const uint32_t was = ee_read<uint32_t>(pv + 0x348);
                vf_class1381_update_l18(m);  // (without the fix: held, +0x348 1)
                w32(pv + 0x350, ridden);
                const uint32_t now = frame_scale_i(0x1e) < static_cast<int32_t>(ridden) ? 1u : was;
                if (now != 1) ++changed;
                w32(pv + 0x348, now);
            } else if (on_sinking) {
                const uint16_t air = ee_read<uint16_t>(HERO + 0x30e);
                w16(HERO + 0x30e, 1);
                vf_class1381_update_l18(m);
                w16(HERO + 0x30e, air);
                ++changed;
            } else {
                vf_class1381_update_l18(m);
            }
            out.assign(ram, ram + 0x02000000);
            return changed;
        });
        fixcheck("own1402_fix46", 46, 160, [=]() -> uint32_t {
            const uint32_t m = make1402();
            if (m == 0) return 0u;
            w8(m + 0x20, ri(0, 4) == 0 ? u(ri(0, 3)) : 2u);
            w8(m + 0xbc, 0);
            if (ri(0, 4) != 0) { w32(HERO + 0x208c, coin() ? 0x11u : 0x12u); wf(HERO + 0x2f0, ee_read<float>(m + 0x18) + rf(-1, 0.5f)); }
            return m;
        }, [=](uint32_t m, std::vector<uint8_t>& out) {
            uint8_t* ram = ee_get_ram_base();
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const bool toward_low = ee_read<uint8_t>(m + 0x20) == 2;
            vf_class1402_update_l18(m);
            int changed = 0;
            if (toward_low) {
                const float z = ee_read<float>(m + 0x18), goal = ee_read<float>(pv + 0);
                const uint32_t swim = ee_read<uint32_t>(HERO + 0x208c);
                if (0.1f < std::fabs(z - goal) && (swim == 0x11 || swim == 0x12) && ee_read<float>(HERO + 0x2f0) < z) { wf(HERO + 0x2f0, z); ++changed; }
            }
            out.assign(ram, ram + 0x02000000);
            return changed;
        });

        // ---- P18.3 (part 1): the boss's (class 1422, still generated) helpers that act on the level's other mobys, hand-written
        // (veldin_finale_boss_props.cpp), each at its own address with its arguments in the order the boss passes them: the level's
        // own groups (one holding the class the helper looks for, any other, one whose list is taken away), the group's mobys dealt
        // into each state the helper tests (taken out, 0xfe / 0xfd, too), the lift's and the platforms' states and timers, the
        // sound slots playing or not. The class loop's class1422 row and batch 271's fight dumps run them inside the boss.
        auto own3 = [level](const char* name, uint32_t addr, Ret ret, std::function<Args()> make, std::function<uint64_t(A)> port, int n = 60) {
            if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && only_mismatch(only, name)) return;
            l18check(name, addr, n, ret, [level, make] {
                level();
                Args a = make();
                w32(0x0015f5cc, R() & 0xffff);
                seed_rand();
                return a;
            }, port);
        };
        auto group_mobys = [=](int32_t g) {  // the mobys of the level's group `g`, in its list's order
            std::vector<uint32_t> v;
            uint32_t e = lvd::group(g);
            if (e == 0) return v;
            for (;; e += 2) {
                v.push_back(first_moby() + ((u(ee_read<uint16_t>(e)) & 0x7fffu) << 8));
                if ((ee_read<uint16_t>(e) & 0x8000u) != 0) break;
            }
            return v;
        };
        auto deal_group = [=](int16_t cls) -> int32_t {  // a group holding a moby of class `cls` in most cases, any group else
            std::vector<int32_t> gs;
            for (int32_t g = 0; g < lvd::group_count(); ++g)
                for (const uint32_t o : group_mobys(g))
                    if (ee_read<int16_t>(o + 0xa6) == cls) { gs.push_back(g); break; }
            const int32_t g = !gs.empty() && ri(0, 4) != 0 ? gs[static_cast<size_t>(ri(0, static_cast<int>(gs.size()) - 1))]
                                                          : ri(0, lvd::group_count() - 1);
            if (ri(0, 12) == 0) w32(OV(0x001ab840) + u(g) * 4, 0);  // (the group without a list)
            return g;
        };
        auto deal_states = [=](int32_t g, std::initializer_list<int> states) {  // the group's mobys in the states given (or left)
            const std::vector<int> st(states);
            for (const uint32_t o : group_mobys(g))
                if (coin()) w8(o + 0x20, u(st[static_cast<size_t>(ri(0, static_cast<int>(st.size()) - 1))]));
        };
        // 0x28d878, the level's sound: an index of the level's own sounds, past their count, negative; flags; on the boss or any moby
        own3("own1422_sound", 0x0028d878, Ret::Int32, [=] {
            const int32_t n = ee_read<int32_t>(0x0015f5f0) - ee_read<int32_t>(0x0015f574);
            const int32_t idx = ri(0, 6) == 0 ? ri(n - 1, n + 2) : ri(0, 5) == 0 ? ri(-3, 0) : ri(0, std::max(0, n - 1));
            static const uint32_t fl[] = {0, 0, 0, 1, 4, 8, 8};
            const std::vector<uint32_t> boss = mobys_with(0x002f2bf0);
            const uint32_t m = !boss.empty() && coin() ? boss[0] : any_moby();
            if (coin()) for (int k = 0; k < ri(0, 20); ++k) w8(0x0013e550 + u(ri(0, 15)) * 0x70 + 0x74, u(ri(0, 1)));
            return Args{{u(idx), fl[ri(0, 6)], m}, {}};
        }, [](A a) { return r32(static_cast<uint32_t>(vf_level_sound_l18(static_cast<int32_t>(a.i[0]), a.i[1], a.i[2]))); });
        // 0x2f6fa8 / 0x2f7028, a group hidden / shown (a0 the boss, unused; a1 the group): the group's mobys in any state, taken out too
        for (const bool show : {false, true}) {
            own3(show ? "own1422_group_show" : "own1422_group_hide", show ? 0x002f7028 : 0x002f6fa8, Ret::None, [=] {
                const std::vector<uint32_t> boss = mobys_with(0x002f2bf0);
                const int32_t g = deal_group(static_cast<int16_t>(ri(0, 1) == 0 ? 0x247 : 0x24b));
                deal_states(g, {0, 1, 2, 3, 0xfe, 0xfd});
                for (const uint32_t o : group_mobys(g)) {
                    if (coin()) w8(o + 0x31, u(ri(0, 2)));
                    if (coin()) w16(o + 0x34, R() & 0xffff);
                    if (coin()) w32(o + 0x94, R());
                }
                return Args{{boss.empty() ? 0u : boss[0], u(g)}, {}};
            }, [show](A a) {
                if (show) vf_group_show_l18(static_cast<int32_t>(a.i[1])); else vf_group_hide_l18(static_cast<int32_t>(a.i[1]));
                return uint64_t{0};
            });
        }
        // 0x2fa888, the raft platforms' +0xbc: groups with class 1584 platforms or none, any value
        own3("own1422_platforms_flag", 0x002fa888, Ret::None, [=] {
            const int32_t g = deal_group(0x630);
            for (const uint32_t o : group_mobys(g)) w8(o + 0xbc, u(ri(0, 3)));
            return Args{{u(g), coin() ? 1u : R() & 0xff}, {}};
        }, [](A a) { vf_group_platforms_flag_l18(static_cast<int32_t>(a.i[0]), a.i[1]); return uint64_t{0}; });
        // 0x2fc7e0, the robots out: the group's class 1906 robots in their state 8 or another
        own3("own1422_robots_out", 0x002fc7e0, Ret::Int32, [=] {
            const int32_t g = deal_group(0x772);
            deal_states(g, {8, 8, 0, 1, 2, 9});
            return Args{{u(g)}, {}};
        }, [](A a) { return r32(static_cast<uint32_t>(vf_robots_out_l18(static_cast<int32_t>(a.i[0])))); });
        // 0x2d6b58 / 0x2d6c80, the gates picked: points about the group's gates (some far away), the gates' class changed in part
        auto gate_point = [=](int32_t g, float far) {
            const std::vector<uint32_t> ms = group_mobys(g);
            const Vec4 c = ms.empty() || ri(0, 5) == 0 ? ee_ref<Vec4>(0x0013f3d0) : VV(pick(ms) + 0x10);
            return near(c, 0, far, 3);
        };
        own3("own1422_gate_round", 0x002d6b58, Ret::Int32, [=] {
            const int32_t g = deal_group(0x247);
            for (const uint32_t o : group_mobys(g)) if (ri(0, 6) == 0) w16(o + 0xa6, 0x248);
            put(SCR, gate_point(g, 40));
            put(SCR + 0x10, ri(0, 6) == 0 ? VV(SCR) : gate_point(g, 40));
            return Args{{SCR, SCR + 0x10, u(g)}, {}};
        }, [](A a) { return r32(vf_gate_farthest_round_l18(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]))); });
        own3("own1422_gate_nearest", 0x002d6c80, Ret::Int32, [=] {
            const int32_t g = deal_group(0x247);
            for (const uint32_t o : group_mobys(g)) if (ri(0, 6) == 0) w16(o + 0xa6, 0x248);
            put(SCR, gate_point(g, ri(0, 4) == 0 ? 400.0f : 30.0f));
            return Args{{SCR, u(g)}, {}};
        }, [](A a) { return r32(vf_gate_nearest_l18(a.i[0], static_cast<int32_t>(a.i[1]))); });
        // 0x2d6d50 / 0x2d6dc0, the gates opened: a group's, one gate (or another moby with pvars)
        own3("own1422_gates_open", 0x002d6d50, Ret::None, [=] {
            const int32_t g = deal_group(0x247);
            for (const uint32_t o : group_mobys(g)) if (ri(0, 6) == 0) w16(o + 0xa6, 0x248);
            return Args{{u(g)}, {}};
        }, [](A a) { vf_gates_open_l18(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
        own3("own1422_gate_open", 0x002d6dc0, Ret::None, [=] {
            const std::vector<uint32_t> gates = class_moby(0x247);
            const uint32_t o = gates.empty() || ri(0, 6) == 0 ? pick(mobys_with(0x002f2bf0)) : pick(gates);
            if (o != 0) { const uint32_t pv = ee_read<uint32_t>(o + 0x78); w32(pv + 0x2c, R() & 0xff); wf(pv + 0x24, rf(0, 2)); }
            return Args{{o}, {}};
        }, [](A a) { vf_gate_open_l18(a.i[0]); return uint64_t{0}; });
        // 0x2d8858 / 0x2d8888, a floating platform (class 587) sunk / rocked: in each of its states; the waits as the boss passes them
        own3("own1422_float_sink", 0x002d8858, Ret::Int32, [=] {
            const uint32_t o = pick(mobys_with(0x002d82c0));
            w8(o + 0x20, u(ri(0, 6)));
            return Args{{o}, {}};
        }, [](A a) { return r32(vf_float_sink_l18(a.i[0])); });
        own3("own1422_float_rock", 0x002d8888, Ret::Int32, [=] {
            fifty();
            const uint32_t o = pick(mobys_with(0x002d82c0));
            w8(o + 0x20, ri(0, 2) == 0 ? 1u : u(ri(0, 6)));
            return Args{{o, u(frame_scale_i(0x3c)), u(frame_scale_i(ee_read<int32_t>(0x0016233c)))}, {}};
        }, [](A a) { return r32(vf_float_rock_l18(a.i[0], static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]))); });
        // 0x2d8050 / 0x2d7f68 / 0x2d7fd0, the lift (class 586): polled, sent up, stopped, in each of its states; its wait over or
        // not; its sound slot playing for it, for another moby, idle, none
        auto make586 = [=] {
            const uint32_t o = pick(mobys_with(0x002d7b20));
            if (o == 0) return 0u;
            const uint32_t pv = ee_read<uint32_t>(o + 0x78);
            w8(o + 0x20, ri(0, 2) == 0 ? 2u : u(ri(0, 4)));
            timer(pv + 0x6c);
            sound_slot(o, pv + 0x70);
            return o;
        };
        own3("own1422_lift_poll", 0x002d8050, Ret::Int32, [=] { return Args{{make586()}, {}}; },
             [](A a) { return r32(static_cast<uint32_t>(vf_lift_poll_l18(a.i[0]))); });
        own3("own1422_lift_start", 0x002d7f68, Ret::None, [=] {
            fifty();
            return Args{{make586(), u(frame_scale_i(coin() ? 0x708 : ri(1, 200)))}, {}};
        }, [](A a) { vf_lift_start_l18(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
        own3("own1422_lift_stop", 0x002d7fd0, Ret::None, [=] { return Args{{make586()}, {}}; },
             [](A a) { vf_lift_stop_l18(a.i[0]); return uint64_t{0}; });

        // ---- P18.3 (part 1b): the boss's shots spawned and moved and its launches out of a group (veldin_finale_boss_spawns.cpp),
        // each at its own address with every argument register and float the original reads (the own564 / own624 / own628 /
        // own983 generators above spawn through these ports: these checks are the spawners' proof)
        auto shot_from = [=] {  // a point above the boss or near the hero
            const std::vector<uint32_t> boss = mobys_with(0x002f2bf0);
            const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
            Vec4 p = !boss.empty() && coin() ? VV(boss[0] + 0x10) : near(h, 2, 20, 0);
            p.z += rf(0.5f, 8);
            return p;
        };
        auto shot_owner = [=] {
            const std::vector<uint32_t> boss = mobys_with(0x002f2bf0);
            return !boss.empty() && coin() ? boss[0] : ri(0, 4) == 0 ? 0u : any_moby();
        };
        own3("own1422_shell_spawn", 0x002d5348, Ret::Int32, [=] {
            put(SCR, shot_from());
            put(SCR + 0x10, Vec4{rf(-0.4f, 0.4f), rf(-0.4f, 0.4f), rf(-0.3f, 0.6f), rf(-1, 1)});
            put(SCR + 0x20, near(ee_ref<Vec4>(0x0013f3d0), 0, 5, 1));
            const int frames = ri(0, 8) == 0 ? ri(-3, 1) : ri(2, 120);
            return Args{{SCR, SCR + 0x10, SCR + 0x20, u(frames), u(ri(0, 7))}, {rf(-1.5f, 1.5f), rf(-1.5f, 1.5f)}};
        }, [](A a) { return r32(vf_shell_spawn_l18(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]), a.i[4], a.f[0], a.f[1])); });
        own3("own1422_ring_spawn", 0x002db938, Ret::None, [=] {
            put(SCR, shot_from());
            put(SCR + 0x10, Vec4{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(-0.4f, 0.2f), rf(-1, 1)});
            return Args{{SCR, SCR + 0x10, shot_owner()}, {rf(2, 20), rf(0.02f, 0.6f)}};
        }, [](A a) { vf_ring_spawn_l18(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1]); return uint64_t{0}; });
        own3("own1422_burst_spawn", 0x002dc3e0, Ret::Int32, [=] {
            put(SCR, shot_from());
            return Args{{shot_owner(), SCR}, {rf(0.5f, 8)}};
        }, [](A a) { return r32(vf_burst_spawn_l18(a.i[0], a.i[1], a.f[0])); });
        own3("own1422_burst_move", 0x002dc458, Ret::None, [=] {
            fifty();
            put(SCR, shot_from());
            const uint32_t m = vf_burst_spawn_l18(shot_owner(), SCR, rf(0.5f, 8));
            if (m == 0) return Args{{0u, SCR}, {1.0f}};
            const int32_t cap = frame_scale_i(ee_read<int32_t>(0x00161c9c));
            w32(ee_read<uint32_t>(m + 0x78) + 8, u(ri(0, 2) == 0 ? ri(cap - 3, cap + 2) : ri(-2, cap)));
            put(SCR + 0x10, near(VV(m + 0x10), 0, 3, 1));
            return Args{{m, SCR + 0x10}, {rf(0.5f, 8)}};
        }, [](A a) { vf_burst_move_l18(a.i[0], a.i[1], a.f[0]); return uint64_t{0}; });
        own3("own1422_ball_spawn", 0x002ea0f0, Ret::Int32, [=] {
            put(SCR, shot_from());
            return Args{{shot_owner(), SCR, u(ri(0, 3) == 0 ? ri(0, 2) : ri(3, 300))}, {}};
        }, [](A a) { return r32(vf_ball_spawn_l18(a.i[0], a.i[1], a.i[2])); });
        own3("own1422_ball_launch", 0x002ea168, Ret::None, [=] {
            fifty();
            put(SCR, shot_from());
            const uint32_t m = vf_ball_spawn_l18(shot_owner(), SCR, u(ri(3, 300)));
            if (m == 0) return Args{{0u, SCR, SCR, 0u}, {1.0f}};
            w8(m + 0x20, ri(0, 2) == 0 ? 0u : u(ri(0, 3)));
            put(SCR + 0x10, near(VV(m + 0x10), 0, 2, 1));
            put(SCR + 0x20, coin() ? ee_ref<Vec4>(0x0015f580) : near(ee_ref<Vec4>(0x0013f3d0), 0, 6, 1));
            return Args{{m, SCR + 0x10, SCR + 0x20, ri(0, 2) == 0 ? 0u : (coin() ? 1u : R())}, {coin() ? 1.0f : rf(0.2f, 2)}};
        }, [](A a) { vf_ball_launch_l18(a.i[0], a.i[1], a.i[2], a.i[3], a.f[0]); return uint64_t{0}; });
        own3("own1422_bomb_throw", 0x002d5cf8, Ret::Int32, [=] {
            fifty();
            const int32_t g = deal_group(0x238);
            deal_states(g, {5, 5, 5, 1, 3, 4});
            for (const uint32_t o : group_mobys(g)) if (coin()) w8(o + 0x53, u(ri(0, 2)));
            put(SCR, shot_from());
            put(SCR + 0x10, near(ee_ref<Vec4>(0x0013f3d0), 0, 12, 1));
            return Args{{shot_owner(), u(g), SCR, SCR + 0x10, u(ri(0, 4) == 0 ? ri(0, 2) : ri(30, 400))}, {rf(0.1f, 1.6f)}};
        }, [](A a) { return r32(vf_bomb_throw_l18(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2], a.i[3], a.i[4], a.f[0])); });
        own3("own1422_robot_launch", 0x002fc668, Ret::Int32, [=] {
            fifty();
            const int32_t g = deal_group(0x772);
            deal_states(g, {8, 8, 8, 0, 1, 2});
            put(SCR, shot_from());
            put(SCR + 0x10, near(ee_ref<Vec4>(0x0013f3d0), 0, 12, 1));
            return Args{{u(g), SCR, ri(0, 2) == 0 ? SCR : SCR + 0x10}, {rf(0.1f, 1.6f)}};
        }, [](A a) { return r32(vf_robot_launch_l18(static_cast<int32_t>(a.i[0]), a.i[1], a.i[2], a.f[0])); });
        own3("own1422_divers_launch", 0x002f0030, Ret::Int32, [=] {
            const int32_t g = deal_group(0x54b);
            deal_states(g, {1, 1, 0, 2, 3});
            put(SCR, shot_from());
            return Args{{shot_owner(), u(g), SCR}, {}};
        }, [](A a) { return r32(vf_divers_launch_l18(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2])); });

        // ---- P18.3 (part 2): the boss's moves and what it sets in the level (veldin_finale_boss_moves.cpp /
        // veldin_finale_boss_level.cpp), each at its own address with every argument the original reads (the camera move's ninth
        // word on the stack): the boss with its fight's pvars dealt (the arena one of the boxes its pvars name, its home inside
        // and outside the orbit's radius, its speed, height, state, phase, last attack), the hero about the arena in his modes
        // (0xf, 0x77 or another), the rafts and the ties of their boxes, the class 644 camera move, the scene's moby list. A third
        // of the cases at 50 Hz where the frame time is read.
        auto boss_dealt = [=]() -> uint32_t {
            const std::vector<uint32_t> bs = mobys_with(0x002f2bf0);
            if (bs.empty()) return 0u;
            const uint32_t m = bs[0], pv = ee_read<uint32_t>(m + 0x78);
            static const uint32_t boxes[] = {0x250, 0x254, 0x258, 0x25c, 0x294, 0x2a0};
            w32(pv + 0x344, ee_read<uint32_t>(pv + boxes[ri(0, 5)]));
            const Vec4 c = VV(lvd::box(lvd::BOXES, ee_read<int32_t>(pv + 0x344)) + 0x30);
            put(pv + 0x3c0, near(c, 0, ri(0, 3) == 0 ? 60.0f : 26.0f, 3));
            put(m + 0x10, near(VV(pv + 0x3c0), 0, 4, 2));
            wf(pv + 0x374, ri(0, 3) == 0 ? rf(0, 0.12f) : rf(-0.5f, 2));
            wf(pv + 0x3c8, c.z + rf(-6, 6));
            static const uint8_t st[] = {2, 2, 12, 13, 13, 15, 15, 19, 21, 10};
            w8(m + 0x20, st[ri(0, 9)]);
            hero_at(near(c, 0, ri(0, 3) == 0 ? 70.0f : 32.0f, 3));
            if (ri(0, 2) == 0) w32(0x001413dc, 0xf);
            wf(0x0013f4b0, rf(0, 1.5f));
            fifty();
            // (in a third of the cases the frame time's square raised: the home's acceleration then reaches the top speed the move
            // computes in one frame, so that top speed shows in +0x374)
            if (ri(0, 2) == 0) wf(0x0015ed70, rf(0.05f, 0.5f));
            return m;
        };
        own3("own1422_home_step", 0x002f6878, Ret::Int32, [=] {
            const uint32_t m = boss_dealt();
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const int k = ri(0, 3);
            put(SCR, near(VV(pv + 0x3c0), k == 0 ? 0.0f : k == 1 ? 0.05f : 0.2f, k == 0 ? 0.09f : k == 1 ? 0.15f : 24.0f, 2));
            return Args{{m, SCR}, {ri(0, 3) == 0 ? rf(0.01f, 0.2f) : rf(0.2f, 25)}};
        }, [](A a) { return r32(vf_boss_home_step_l18(a.i[0], a.i[1], a.f[0])); });
        own3("own1422_charge", 0x002f6960, Ret::None, [=] { return Args{{boss_dealt()}, {}}; },
             [](A a) { vf_boss_charge_l18(a.i[0]); return uint64_t{0}; });
        own3("own1422_orbit", 0x002f6aa0, Ret::None, [=] { return Args{{boss_dealt()}, {}}; },
             [](A a) { vf_boss_orbit_l18(a.i[0]); return uint64_t{0}; });
        own3("own1422_float_pick", 0x002f6e10, Ret::Int32, [=] {
            const uint32_t m = boss_dealt();
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const int32_t g = ri(0, 2) == 0 ? ee_read<int32_t>(pv + 0x204) : deal_group(0x24b);
            w32(pv + 0x398, u(g));
            if (ri(0, 8) == 0) w32(OV(0x001ab840) + u(g) * 4, 0);  // (the group without a list)
            deal_states(g, {0, 1, 1, 2, 2, 3, 4, 5});
            const std::vector<uint32_t> ms = group_mobys(g);
            if (!ms.empty()) put(pv + 0x3c0, near(VV(pick(ms) + 0x10), 0, 30, 2));
            if (coin()) put(m + 0xc0, Vec4{rf(-1, 1), rf(-1, 1), rf(-0.3f, 0.3f), 0});
            return Args{{m}, {}};
        }, [](A a) { return r32(vf_boss_float_pick_l18(a.i[0])); });
        own3("own1422_next_attack", 0x002f70b8, Ret::Int32, [=] {
            const uint32_t m = boss_dealt();
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w32(pv + 0x34c, u(ri(0, 3)));
            static const uint32_t last[] = {13, 14, 15, 16, 17, 19, 14, 19, 0, 12};
            w32(pv + 0x350, last[ri(0, 9)]);
            w32(pv + 0x364, ri(0, 3) == 0 ? 1u : 0u);
            const Vec4 c = VV(lvd::box(lvd::BOXES, ee_read<int32_t>(pv + 0x25c)) + 0x30);
            const bool far = coin();  // (past the charge's reach L18:0x162378 + 2 = 32 or within it)
            hero_at(near(c, far ? 33.0f : 0.0f, far ? 70.0f : 31.0f, 2));
            return Args{{m}, {}};
        }, [](A a) { return r32(static_cast<uint32_t>(vf_boss_next_attack_l18(a.i[0]))); });
        own3("own1422_bearing", 0x002f7220, Ret::None, [=] {
            const std::vector<uint32_t> bs = mobys_with(0x002f2bf0);
            const uint32_t m = !bs.empty() && coin() ? bs[0] : any_moby();
            wf(m + 0x48, rf(-3.1f, 3.1f));
            put(SCR, near(ee_ref<Vec4>(0x0013f3d0), 0, 30, 3));
            put(SCR + 0x10, ri(0, 6) == 0 ? VV(SCR) : near(VV(SCR), 0, 30, 3));
            const uint32_t out = !bs.empty() && coin() ? ee_read<uint32_t>(bs[0] + 0x78) + 0x140 : SCR + 0x100;
            return Args{{m, out, SCR, SCR + 0x10}, {}};
        }, [](A a) { vf_boss_bearing_l18(a.i[0], a.i[1], a.i[2], a.i[3]); return uint64_t{0}; });
        // the camera move (class 644): started from five vectors (frames none, negative, few, many), its progress read back
        auto pan_moby = [=] { return pick(mobys_with(0x002df608)); };
        own3("own1422_pan_start", 0x002dfaa0, Ret::None, [=] {
            fifty();
            for (uint32_t k = 0; k < 0x50; k += 0x10) put(SCR + k, Vec4{rf(-700, 700), rf(-700, 700), rf(-10, 200), rf(-4, 4)});
            const int32_t frames = ri(0, 8) == 0 ? ri(-2, 1) : ri(10, 600);
            return Args{{pan_moby(), SCR, SCR + 0x10, SCR + 0x20, SCR + 0x30, SCR + 0x40, u(frames), u(ri(0, 6) == 0 ? ri(-1, 1) : ri(2, 300)),
                         coin() ? u(ri(0, 1)) : R()}, {}};
        }, [](A a) { vf_pan_start_l18(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5], static_cast<int32_t>(a.i[6]), static_cast<int32_t>(a.i[7]), a.i[8]); return uint64_t{0}; });
        own3("own1422_pan_progress", 0x002dfb90, Ret::Float, [=] {
            const uint32_t o = pan_moby();
            wf(ee_read<uint32_t>(o + 0x78) + 0x60, rf(-1, 2));
            return Args{{o}, {}};
        }, [](A a) { return fbits(vf_pan_progress_l18(a.i[0])); });
        // the rafts (class 1381): sent off with their platforms, sunk by the ball (in each state, another class's update in part),
        // the ties of the three kinds (and other kinds the level loaded) freed in the rafts' boxes and others
        auto raft_dealt = [=] {
            const uint32_t o = pick(mobys_with(0x002f16f0));
            if (o == 0) return 0u;
            const uint32_t pv = ee_read<uint32_t>(o + 0x78);
            w8(o + 0x20, ri(0, 2) == 0 ? 1u : u(ri(0, 4)));
            if (ri(0, 5) == 0) w16(o + 0xa6, 0x566);
            if (coin()) { w32(pv + 0x330, R()); w32(pv + 0x344, R()); }
            if (ri(0, 5) == 0) w32(pv + 0xc, 0xffffffffu);
            else if (ri(0, 4) == 0) w32(pv + 0xc, u(ri(0, lvd::box_count(lvd::BOXES) - 1)));
            return o;
        };
        auto ties_dealt = [=](uint32_t kind) {  // the kind's instances' +0xc / +0x14 set to other values in part
            const uint32_t idx = ee_read<uint8_t>(0x001c6500 + kind);
            const uint32_t hdr = ee_read<uint32_t>(0x001c6200 + idx * 4);
            const uint32_t inst = ee_read<uint32_t>(hdr + 0x28);
            for (int32_t i = 0; i < ee_read<int16_t>(hdr + 0x26); ++i)
                if (coin()) { w32(inst + u(i) * 0x20 + 0xc, coin() ? 1u : R()); wf(inst + u(i) * 0x20 + 0x14, rf(0, 1)); }
        };
        // a few of the kind's instances moved about the box `box`'s edges (inside, outside, past one side only: the box's own
        // frame through mat3_transform, as the original tests it)
        auto ties_near_box = [=](uint32_t kind, int32_t box) {
            const uint32_t hdr = ee_read<uint32_t>(0x001c6200 + u(ee_read<uint8_t>(0x001c6500 + kind)) * 4);
            const int32_t n = ee_read<int16_t>(hdr + 0x26);
            if (n <= 0) return;
            const uint32_t b = lvd::box(lvd::BOXES, box);
            for (int k = 0, put_n = 0; k < 400 && put_n < 6; ++k) {
                Vec4 p = near(VV(b + 0x30), 0, rf(1, 40), 0), loc{};
                vec3_sub(loc, p, VV(b + 0x30));
                mat3_transform(loc, loc, ee_ref<Mat4>(b + 0x40));
                const float e = std::max(std::fabs(loc.x), std::fabs(loc.y));
                if (e < 0.6f || e > 1.6f) continue;
                const uint32_t inst = ee_read<uint32_t>(hdr + 0x28) + u(ri(0, n - 1)) * 0x20;
                wf(inst + 0, p.x); wf(inst + 4, p.y); wf(inst + 8, p.z);
                ++put_n;
            }
        };
        own3("own1422_raft_hold", 0x002f1cc8, Ret::None, [=] {
            const uint32_t o = raft_dealt();
            const uint32_t pv = ee_read<uint32_t>(o + 0x78);
            const int32_t n = ee_read<int32_t>(pv + 0x338);
            if (ri(0, 3) == 0) w32(pv + 0x338, u(ri(0, std::max(0, n))));
            put(o + 0x10, near(VV(o + 0x10), 0, 6, 1));
            return Args{{o}, {}};
        }, [](A a) { vf_raft_hold_l18(a.i[0]); return uint64_t{0}; });
        own3("own1422_ties_free", 0x002f1d08, Ret::None, [=] {
            static const uint32_t kinds[] = {0x802, 0x908, 0x954};
            uint32_t kind = kinds[ri(0, 2)];
            if (ri(0, 5) == 0)
                for (int t = 0; t < 50; ++t) {
                    const uint32_t k = u(ri(0, 0xbff));
                    if (ee_read<uint8_t>(0x001c6500 + k) != 0xff) { kind = k; break; }
                }
            ties_dealt(kind);
            const std::vector<uint32_t> rafts = mobys_with(0x002f16f0);
            int32_t box = ri(0, lvd::box_count(lvd::BOXES) - 1);
            if (!rafts.empty() && ri(0, 3) != 0) box = ee_read<int32_t>(ee_read<uint32_t>(pick(rafts) + 0x78) + 0xc);
            if (box < 0) box = 0;
            ties_near_box(kind, box);
            return Args{{kind, u(box)}, {}};
        }, [](A a) { vf_ties_free_l18(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
        own3("own1422_raft_sink", 0x002f1c38, Ret::None, [=] {
            const uint32_t o = raft_dealt();
            for (const uint32_t kind : {0x802u, 0x908u, 0x954u}) {
                ties_dealt(kind);
                if (coin() && ee_read<int32_t>(ee_read<uint32_t>(o + 0x78) + 0xc) >= 0) ties_near_box(kind, ee_read<int32_t>(ee_read<uint32_t>(o + 0x78) + 0xc));
            }
            for (const uint32_t p : group_mobys(ee_read<uint8_t>(o + 0x21))) w8(p + 0xbc, u(ri(0, 3)));
            return Args{{o}, {}};
        }, [](A a) { vf_raft_sink_l18(a.i[0]); return uint64_t{0}; });
        // 0x265ff8: the record of the boss's (or another moby's) pvar word 0 by the hero's distance (no moby, no flag 0x20, no record)
        own3("own1422_record_dist", 0x00265ff8, Ret::None, [=] {
            const std::vector<uint32_t> bs = mobys_with(0x002f2bf0);
            uint32_t m = !bs.empty() && coin() ? bs[0] : any_moby();
            if (ri(0, 10) == 0) m = 0;
            if (m != 0) {  // (a moby without pvars in RAM never has the flag; another than the boss gets a record in the test area)
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                const bool has_pv = pv >= 0x00100000 && pv < 0x01ff0000;
                const bool own = (ee_read<uint16_t>(m + 0x34) & 0x20) != 0 && coin();  // (the game's own record kept)
                if (has_pv && ri(0, 4) != 0) w16(m + 0x34, ee_read<uint16_t>(m + 0x34) | 0x20); else w16(m + 0x34, ee_read<uint16_t>(m + 0x34) & 0xffdf);
                if (has_pv && !own) w32(pv, ri(0, 8) == 0 ? 0u : SCR + 0x200);
                w8(SCR + 0x20a, R() & 0xff);
                hero_at(near(VV(m + 0x10), 0, ri(0, 3) == 0 ? 80.0f : 20.0f, 3));
            }
            return Args{{m}, {coin() ? 0.5f : rf(0, 4)}};
        }, [](A a) { vf_record_by_hero_distance_l18(a.i[0], a.f[0]); return uint64_t{0}; });
        // 0x2f5e18: the hero's ride on a floating platform (he stands on one or not, in his state 0x77 or not; counted already)
        own3("own1422_float_count", 0x002f5e18, Ret::None, [=] {
            const std::vector<uint32_t> bs = mobys_with(0x002f2bf0);
            if (bs.empty()) return Args{{0u}, {}};
            const uint32_t pv = ee_read<uint32_t>(bs[0] + 0x78);
            const std::vector<uint32_t> fl = mobys_with(0x002d82c0);
            auto some = [&] { const int k = ri(0, 4); return k == 0 ? 0u : k == 1 ? any_moby() : pick(fl); };
            w32(pv + 0x3e4, ri(0, 4) == 0 ? 1u : 0u);
            w32(pv + 0x3e8, some());
            w32(0x0013f650, coin() ? 1u : 0u);
            w32(0x0013f64c, some());
            if (ri(0, 3) != 0) w32(0x001413d4, 0x77);
            w32(0x001623a8, u(ri(0, 5)));
            return Args{{bs[0]}, {}};
        }, [](A a) { vf_hero_float_count_l18(a.i[0]); return uint64_t{0}; });
        // 0x2f2b18: Drek's model (class 1249) of the scene's moby list scaled (the level's mode 2 or not, the flag, the list's
        // count, Drek's models and others, the joint node set up already)
        own3("own1422_drek_scale", 0x002f2b18, Ret::None, [=] {
            w32(0x0015f5c4, ri(0, 3) != 0 ? 2u : u(ri(0, 5)));
            w8(0x0015edb0, ri(0, 3) != 0 ? 1u : 0u);
            const std::vector<uint32_t> dk = class_moby(0x4e1);
            const std::vector<uint32_t> bs = mobys_with(0x002f2bf0);
            const int32_t n = ri(0, 6) == 0 ? ri(-1, 0) : ri(1, 8);
            w16(0x0016d2a4, u(n) & 0xffff);
            for (int32_t i = 0; i < std::max(n, 0); ++i) {
                uint32_t o = ri(0, 2) == 0 || dk.empty() ? (bs.empty() || coin() ? any_moby() : bs[0]) : pick(dk);
                if (ri(0, 5) == 0 && !bs.empty()) { o = bs[0]; w16(o + 0xa6, 0x4e1); }
                w32(0x0016d260 + 0x178 + u(i) * 4, o);
            }
            w8(0x0017ce40 + 1, ri(0, 4) == 0 ? 1u : 0u);
            wf(0x001623a4, rf(0.2f, 3));
            return Args{{}, {}};
        }, [](A) { vf_drek_scale_l18(); return uint64_t{0}; });

        // ---- P18.3 (part 3): the boss every frame around its state (veldin_finale_boss_frame.cpp), at its own address: the
        // boss of boss_dealt in the states each branch tests, its phase 0 .. 5 (the damage floors 250 .. 0) and its health about
        // the floor, a hit by each kind of moby the preamble sorts (its own ring / shell, a walking bomb thrown back or not,
        // classes 0xb1 / 0x131, others, none; kind 0x80000 or not), its attached mobys taken away, its sound slot and camera
        // request in each state, the lift (no state 2 with its wait over: that branch starts scene 7, which the oracle cannot
        // run), the fight's restart word and 0x1415f8 both ways; the camera's states with the hero in his modes, a level
        // camera slot of type 0x12 live or not, the hero's ride; the smoke's counts as the level has them or 0 .. 3.
        auto boss_frame = [=](const uint8_t* states, int n_states) -> uint32_t {
            const uint32_t m = boss_dealt();
            if (m == 0) return 0u;
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w32(pv + 0x334, first_moby() + (ee_read<uint32_t>(pv + 0x220) << 8));  // (the camera move's moby, as state 0 sets it)
            w8(m + 0x20, states[ri(0, n_states - 1)]);
            if (coin()) w8(m + 0x53, u(ri(0, 0xe)));
            sound_slot(m, pv + 0x3b0);
            const int key = ri(0, 2);
            if (key == 0) w32(pv + 0x368, 0xffffffffu);
            else if (key == 1) w32(pv + 0x368, u(camera_request(0x16, 0xffff, ORACLE_LADDR(18, 0x00239360), ORACLE_LADDR(18, 0x00239480),
                                                               ORACLE_LADDR(18, 0x0023cd60), pv + 0x36c, 0x15e)));
            else w32(pv + 0x368, u(ri(0, 40)));
            wf(pv + 0x38c, ri(0, 5) == 0 ? (coin() ? 0.0f : -rf(0, 5)) : rf(1, 400));
            return m;
        };
        own3("own1422_preamble", 0x002f5e80, Ret::None, [=] {
            static const uint8_t sts[] = {2, 2, 2, 12, 13, 14, 15, 16, 17, 18, 19, 19, 3, 7, 21, 26, 10, 0x1a};
            const uint32_t m = boss_frame(sts, 18);
            if (m == 0) return Args{{0u}, {}};
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 2) != 0) {  // a frame before by the port (the attached mobys spawned, the target found): no hit, the lift idle,
                // no phase change (the state dealt again below)
                const uint8_t st0 = ee_read<uint8_t>(m + 0x20);
                w8(m + 0xa4, 0xff);
                w8(first_moby() + (ee_read<uint32_t>(pv + 0x21c) << 8) + 0x20, 0);
                w32(0x0016238c, 0);
                w32(0x001415f8, 0);
                vf_boss_preamble_l18(m);
                w8(m + 0x20, st0);
            }
            const int kind_of_case = ri(0, 5);  // 0, 1: the floor crossed; 2: the death; else any
            int32_t phase = ri(0, 5);
            if (kind_of_case <= 1) phase = ri(0, 2);
            if (kind_of_case == 2) phase = 5;
            w32(pv + 0x34c, u(phase));
            const float floor = static_cast<float>(5 - phase) / 7.0f * 350.0f;
            if (ri(0, 4) != 0) wf(pv + 0x38c, floor + rf(-6, 14));
            if (kind_of_case <= 1) wf(pv + 0x38c, floor + rf(0.1f, 3));
            if (kind_of_case == 2) {
                wf(pv + 0x38c, rf(1, 20));
                if (coin()) {  // its sound slot playing for it
                    const uint32_t slot = u(ri(0, 15));
                    w32(pv + 0x3b0, slot);
                    w32(0x0013e550 + slot * 0x70 + 0x88, m);
                    w8(0x0013e550 + slot * 0x70 + 0x74, u(ri(1, 2)));
                }
            }
            if (kind_of_case <= 2) {
                static const uint8_t fight[] = {2, 2, 12, 13, 14, 15, 16, 17, 18, 19};
                w8(m + 0x20, fight[ri(0, 9)]);
            }
            if (ri(0, 6) == 0) w32(pv + 0x3a0, 0);
            if (ri(0, 6) == 0) w32(pv + 0x3a4, 0);
            if (ri(0, 3) == 0) w32(pv + 0xb0, 0);
            // the hit: its slot naming the boss, a kind in the mask 0x330000 (0x80000 in part) or not, a damage, its moby
            if (ri(0, 4) != 0) {
                const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
                w8(m + 0xa4, idx);
                w32(e + 0x34, m);
                w32(e + 0x24, ri(0, 5) == 0 ? (R() & ~0x330000u) : ((coin() ? 0x10000u : 0x20000u) | (coin() ? 0x80000u : 0u) | (R() & 0xffu)));
                w8(e + 0x28, u(ri(0, 11))); w8(e + 0x29, u(ri(0, 4))); w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
                wf(e + 0x1c, coin() ? 5627.95f : 0.0f);
                wfrand(e, 4, -1, 1);
                const float health = ee_read<float>(pv + 0x38c);
                const int dk = ri(0, 5);
                wf(e + 0x2c, dk == 0 ? 0.0f : dk == 1 ? health : dk == 2 ? health - floor + rf(-1, 1) : rf(0.1f, 14));
                if (kind_of_case <= 1) wf(e + 0x2c, rf(4, 10));
                if (kind_of_case == 2) wf(e + 0x2c, health + rf(0, 5));
                static const int16_t kinds[] = {0x270, 0x234, 0x238, 0x238, 0xb1, 0xb1, 0x131, 0x131, -1, -1, 0};
                const int16_t kind = kinds[ri(0, 10)];
                uint32_t by = 0;
                if (kind != 0) {
                    by = ri(0, 3) == 0 ? ee_read<uint32_t>(0x001413d0) : any_moby();
                    if (by == m) by = ee_read<uint32_t>(0x001413d0);
                    if (kind > 0 && by != ee_read<uint32_t>(0x001413d0)) { w16(by + 0xa6, u(kind)); w8(by + 0xbc, u(ri(0, 1))); }
                }
                w32(e + 0x20, by);
            } else w8(m + 0xa4, 0xff);
            // the lift: at its top, on its way with its wait running, or idle (never on its way with the wait over: scene 7)
            {
                const uint32_t lift = first_moby() + (ee_read<uint32_t>(pv + 0x21c) << 8);
                const int ls = ri(0, 3);
                w8(lift + 0x20, ls == 0 ? 3u : ls == 1 ? 2u : u(ri(0, 1)));
                if (ls == 1) w32(ee_read<uint32_t>(lift + 0x78) + 0x6c, u(ri(1, 200)));
            }
            w32(0x0016238c, ri(0, 4) == 0 ? 1u : 0u);
            w32(0x001415f8, ri(0, 3) != 0 ? 1u : 0u);
            if (kind_of_case <= 1) { w32(0x0016238c, 0); w32(0x001415f8, 1); }
            return Args{{m}, {}};
        }, [](A a) { vf_boss_preamble_l18(a.i[0]); return uint64_t{0}; }, 120);
        own3("own1422_camera", 0x002f7288, Ret::None, [=] {
            static const uint8_t sts[] = {0, 1, 2, 3, 6, 9, 10, 11, 12, 13, 19, 19, 19, 19, 19, 19, 27, 27, 21, 15};
            const uint32_t m = boss_frame(sts, 20);
            if (m == 0) return Args{{0u}, {}};
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 2) == 0) w32(0x001413d4, 0x42);
            if (ri(0, 3) == 0) for (uint32_t i = 0; i < 48; ++i) w32(0x0016a290 + i * 4, 0);  // (no live camera of type 0x12)
            w32(pv + 0x364, ri(0, 3) == 0 ? 1u : 0u);
            wf(pv + 0x390, rf(0, 40));
            w32(pv + 0x360, ri(0, 2) == 0 ? 0u : u(ri(1, 30)));
            w32(pv + 0x3ec, ri(0, 3) == 0 ? 1u : 0u);
            {  // the hero's ride: a floating platform (class 587), another moby, none
                const std::vector<uint32_t> fl = mobys_with(0x002d82c0);
                const int k = ri(0, 3);
                w32(0x0013f64c, k == 0 ? 0u : k == 1 ? any_moby() : pick(fl));
            }
            w32(0x001623a0, ri(0, 2) == 0 ? 1u : 0u);
            if (coin()) {  // a level camera slot of type 0x12 live (its level camera one of the level's)
                const uint32_t i = u(ri(0, 47)), c = 0x00167b50 + i * 0xa0;
                w32(0x0016a290 + i * 4, 1);
                w16(c + 0x86, 0x12);
                w16(c + 0x84, u(ri(0, std::max(0, lvd::camera_count() - 1))));
            }
            const uint32_t active = ee_read<uint32_t>(0x00167800);
            if (active != 0 && ri(0, 3) == 0) w16(active + 0x86, coin() ? 5u : 0u);
            if (coin()) hero_at(near(VV(pv + 0x3c0), 0, ri(0, 2) == 0 ? 80.0f : 30.0f, 3));
            if (coin()) {  // the camera's distance / height among the targets the states seek (so that the target chosen shows)
                const uint32_t c = lvd::camera_data(ee_read<int32_t>(pv + 0x224));
                float lo = 1e9f, hi = -1e9f, hlo = 1e9f, hhi = -1e9f;
                for (const uint32_t t : {0x001623c0u, 0x001623c8u, 0x001623d0u, 0x001623d8u, 0x001623e0u}) {
                    lo = std::min(lo, ee_read<float>(t)); hi = std::max(hi, ee_read<float>(t));
                    hlo = std::min(hlo, ee_read<float>(t + 4)); hhi = std::max(hhi, ee_read<float>(t + 4));
                }
                wf(c + 0x34, rf(lo, hi)); wf(c + 0x38, rf(hlo, hhi));
                if (coin()) { wf(pv + 0x378, 0.0f); wf(pv + 0x37c, 0.0f); }
            }
            if (ee_read<uint8_t>(m + 0x20) == 19 && coin())  // (about the 28 from the arena's centre)
                hero_at(near(VV(lvd::box(lvd::BOXES, ee_read<int32_t>(pv + 0x344)) + 0x30), 18, 34, 2));
            return Args{{m}, {}};
        }, [](A a) { vf_boss_camera_l18(a.i[0]); return uint64_t{0}; }, 120);
        own3("own1422_smoke", 0x002f76a0, Ret::None, [=] {
            fifty();
            const std::vector<uint32_t> bs = mobys_with(0x002f2bf0);
            const uint32_t m = !bs.empty() && ri(0, 3) != 0 ? bs[0] : any_moby();
            w8(m + 0xbc, u(ri(0, 3)));
            if (coin()) {  // another facing
                const float a = rf(-3.1f, 3.1f);
                wf(m + 0xc0, std::cos(a)); wf(m + 0xc4, std::sin(a)); wf(m + 0xc8, rf(-0.3f, 0.3f));
            }
            if (ri(0, 2) == 0) w32(0x00162408, u(ri(0, 3)));
            if (ri(0, 2) == 0) w32(0x00162438, u(ri(0, 3)));
            put(SCR, near(VV(m + 0x10), 0, 3, 2));
            return Args{{m, SCR}, {}};
        }, [](A a) { vf_boss_smoke_l18(a.i[0], a.i[1]); return uint64_t{0}; });

        // ---- P18.3 (part 4): the boss's update (veldin_finale_boss.cpp) at its own address, in each of its states, short of
        // the scenes the oracle cannot run (state 1's Giant Clank box: his form kept off 2; state 2 above 301 health; state 4
        // with the hero on a floating platform; state 7's hero lost; state 27's steps 0 .. 6, 8 and the menu; the preamble's
        // lift lost): the boss after its state 0 (the port's, as the class loop's set-up), its fight dealt (boss_dealt), the
        // phase's path flown and its point, the energy ball held or not, its animation and the animation's end flag, its
        // timers at their edges, the scene record for state 7, the end's count for state 27
        for (int st = 0; st < 28; ++st) {
            if (st == 25 || st == 24 || st == 23) continue;  // (22 stands for the tail-only states 6, 22 .. 25)
            const std::string nm = "own1422_update_s" + std::to_string(st);
            owncheck(strdup(nm.c_str()), 0x002f2bf0, vf_class1422_update_l18, [=]() -> uint32_t {
                const std::vector<uint32_t> bs = mobys_with(0x002f2bf0);
                if (bs.empty()) return 0u;
                const uint32_t m = bs[0], pv = ee_read<uint32_t>(m + 0x78);
                set_up(m, vf_class1422_update_l18);
                boss_dealt();
                const int32_t phase = st == 21 ? ri(0, 5) : st == 12 ? ri(0, 3) : ri(0, 2);
                w32(pv + 0x34c, u(phase));
                const uint32_t path = lvd::path(ee_read<int32_t>(pv + 0x260 + u(ri(0, 2)) * 4));
                w32(pv + 0x340, path);
                const int32_t np = ee_read<int32_t>(path);
                w32(pv + 0x348, u(ri(0, 4) == 0 ? std::max(0, np - ri(0, 2)) : ri(0, std::max(0, np - 1))));
                if (coin()) w32(pv + 0x33c, 0);
                else if (ee_read<uint32_t>(pv + 0x33c) == 0) w32(pv + 0x33c, vf_ball_spawn_l18(m, m + 0x10, u(frame_scale_i(0x12c))));
                static const uint8_t anims[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe};
                if (ri(0, 2) != 0) {  // an animation of the boss's at a frame its states test (1, 16, 30) or another
                    const uint8_t a = anims[ri(0, 14)];
                    static const int fr[] = {0, 1, 1, 16, 16, 30, 5};
                    moby_anim_set(m, a, fr[ri(0, 6)], 0);
                    if (coin()) w8(m + 0x52, a);
                }
                if (coin()) w8(m + 0x70, u(ri(0, 3)));
                if (coin()) w8(m + 0x31, u(ri(0, 1)));
                for (const uint32_t t : {0x354u, 0x358u, 0x35cu, 0x360u}) timer(pv + t);
                if (ri(0, 3) == 0) w32(pv + 0x354, 1);  // (its last frame)
                if (ri(0, 3) == 0) wf(pv + 0x390, rf(0, 50));
                {  // the camera move (class 644): its state (3: done, as state 3 waits for) and its progress (states 8 / 9)
                    const uint32_t pan = ee_read<uint32_t>(pv + 0x334);
                    if (pan != 0 && coin()) w8(pan + 0x20, u(ri(2, 3)));
                    if (pan != 0 && coin()) wf(ee_read<uint32_t>(pan + 0x78) + 0x60, rf(0.4f, 1.1f));
                }
                if (coin()) {  // the home at a goal its moves test: the lunge's box, the gate, the path's point
                    const int k = ri(0, 2);
                    const uint32_t g = ee_read<uint32_t>(pv + 0x330);
                    Vec4 q = k == 0 ? VV(lvd::box(lvd::BOXES, ee_read<int32_t>(pv + 0x3a8)) + 0x30)
                           : k == 1 && g != 0 ? VV(g + 0x10) : VV(path + 0x10 + (ee_read<uint32_t>(pv + 0x348) << 4));
                    put(pv + 0x3c0, near(q, 0, 0.3f, 0));
                    wf(pv + 0x374, rf(0, 0.5f));
                }
                if (ri(0, 3) == 0) w32(pv + 0x358, u(frame_scale_i(0x78) - ri(0, 1)));
                if (ri(0, 4) == 0) w32(pv + 0x354, u(frame_scale_i(0x1e)));
                wf(pv + 0x38c, rf(302, 400));
                w8(0x001413f4, u(ri(0, 1)));  // (never Giant Clank: scene 0)
                w32(0x0016238c, 0);
                if (ri(0, 2) == 0) w32(0x00162360, 1);
                if (ri(0, 2) == 0) w32(0x001623a0, 1);
                if (ri(0, 3) == 0) w32(0x00162390, 1);
                {  // the lift: never on its way up with its wait over (the preamble's scene 7)
                    const uint32_t lift = first_moby() + (ee_read<uint32_t>(pv + 0x21c) << 8);
                    const int ls = ri(0, 3);
                    w8(lift + 0x20, ls == 0 ? 3u : ls == 1 ? 2u : u(ri(0, 1)));
                    w32(ee_read<uint32_t>(lift + 0x78) + 0x6c, u(ri(1, 200)));
                }
                sound_slot(m, pv + 0x3b0);
                if (st == 4 || ri(0, 3) == 0) {  // the hero on a floating platform (state 4: else scene 2), hanging, or not
                    const std::vector<uint32_t> fl = mobys_with(0x002d82c0);
                    w32(0x0013f64c, st == 4 || coin() ? pick(fl) : 0u);
                    w16(0x0013f65e, st != 4 && coin() ? 1u : 0u);
                }
                if (st == 7) {  // the scene running (the level's mode 2: scene 2 / 3 at its frames) or over
                    const bool running = coin();
                    w32(0x0015f5c4, running ? 2u : u(ri(0, 1)));
                    w8(pv + 0x39c, u(ri(0, 2) == 0 ? 5 : ri(9, 0x15)));
                    const int32_t num = ri(2, 3);
                    w32(0x0016d260 + 0x30, u(num));
                    const int32_t lo = frame_scale_i(num == 2 ? 0xa28 : 0x3b6), hi = frame_scale_i(num == 2 ? 0xb36 : 0x3c0);
                    w32(0x0016d260 + 0x34, u(ri(0, 2) == 0 ? ri(lo - 3, lo + 3) : ri(0, 2) == 0 ? ri(hi - 3, hi + 3) : ri(lo - 20, hi + 20)));
                }
                if (st == 27) {  // the end's steps the oracle can run: 7 (its count below the scene's 40 frames), 9 .. 12 with the
                    // level not back in its mode 0
                    const int step = ri(0, 2) == 0 ? 7 : ri(9, 12);
                    w32(pv + 0x34c, u(step));
                    w32(0x0015f5c4, u(ri(1, 2)));
                    w32(pv + 0x3ac, u(ri(0, 2) == 0 ? 0 : ri(1, frame_scale_i(0x28) - 1)));
                    if (coin()) {  // the hero on a gate in his mode 0x22
                        const std::vector<uint32_t> gates = class_moby(0x247);
                        w32(0x0013f64c, pick(gates));
                        w16(0x0013f65e, 0);
                        w32(0x001413d4, 0x22);
                    }
                }
                if (st == 1) {  // the hero about the three boxes, inside the polygon (its points' mean)
                    static const uint32_t bx[] = {0x250, 0x254, 0x258, 0x274};
                    hero_at(near(VV(lvd::box(lvd::BOXES, ee_read<int32_t>(pv + bx[ri(0, 3)])) + 0x30), 0, ri(0, 2) == 0 ? 49.0f : 80.0f, 2));
                    if (ri(0, 2) == 0) {
                        const uint32_t poly = lvd::path(ee_read<int32_t>(pv + 0x270));
                        Vec4 c{0, 0, 0, 0};
                        const int32_t n = ee_read<int32_t>(poly);
                        for (int32_t i = 0; i < n; ++i) { const Vec4 q = VV(poly + 0x10 + u(i) * 16); c.x += q.x / n; c.y += q.y / n; c.z += q.z / n; }
                        hero_at(near(c, 0, 3, 1));
                    }
                }
                if (st == 19 && coin()) w32(0x001413d4, 0x16);
                // the state's own edges: its goal reached (21 the gate, 18 the lunge box), its animation at the frame it
                // tests with its end flag (2 / 9 / 13 / 26), the path's end (11), the floor level with the robots' point (17),
                // the boss near the camera (the floor band)
                {  // the gate it heads for (+0x330: states 4 / 11 set it), a gate of the level
                    const std::vector<uint32_t> gates = class_moby(0x247);
                    if (!gates.empty()) w32(pv + 0x330, pick(gates));
                }
                const uint32_t g = ee_read<uint32_t>(pv + 0x330);
                if (st == 9 && coin()) { put(pv + 0x3c0, near(VV(path + 0x10), 0, 0.05f, 0)); wf(pv + 0x374, rf(0, 0.05f)); }
                if (st == 12 && coin()) {  // (the charge allowed: phase 3, the hero near its box)
                    w32(pv + 0x34c, 3);
                    w32(pv + 0x364, 0);
                    hero_at(near(VV(lvd::box(lvd::BOXES, ee_read<int32_t>(pv + 0x25c)) + 0x30), 0, 20, 2));
                }
                if (st == 21 && g != 0 && coin()) { put(pv + 0x3c0, near(VV(g + 0x10), 0, 0.05f, 0)); wf(pv + 0x374, rf(0, 0.05f)); }
                if (st == 18 && coin()) { put(pv + 0x3c0, near(VV(lvd::box(lvd::BOXES, ee_read<int32_t>(pv + 0x3a8)) + 0x30), 0, 0.05f, 0)); wf(pv + 0x374, rf(0, 0.05f)); }
                if ((st == 2 || st == 9 || st == 13 || st == 26) && coin()) {
                    const uint8_t a = st == 2 ? 0xa : st == 9 ? 7 : st == 13 ? (coin() ? 2 : 4) : 0xe;
                    static const int fr[] = {1, 16, 30, 0};
                    moby_anim_set(m, a, fr[ri(0, 3)], 0);
                    w8(m + 0x52, a);
                    w8(m + 0x70, u(ri(0, 3)));
                }
                if (st == 2 && coin()) wf(0x0013f418, ee_read<float>(pv + 0x3c8) + rf(-4, 4));
                if (st == 16 && coin()) deal_states(ee_read<int32_t>(pv + 0x29c), {5, 5, 5, 1});  // (bombs to throw)
                if (st == 11 && coin()) {
                    w32(pv + 0x348, ee_read<uint32_t>(path));
                    w32(0x0013f64c, 0);
                    w16(0x0013f65e, 0);
                    hero_at(near(VV(m + 0x10), 0, 45, 2));
                }
                if (st == 17 && coin()) {  // (the boss's height level with the floor 4.3 to its side)
                    Vec4 q{}, d = VV(m + 0xd0);
                    vec3_set_length(q, d, coin() ? 4.3f : -4.3f);
                    vec3_add(q, q, VV(pv + 0x3c0));
                    wf(pv + 0x3c8, floor_probe_v(0.5f, q, 0) + rf(-0.8f, 0.8f));
                }
                if (coin()) { w8(m + 0x31, 1); if (coin()) put(m + 0x10, near(VV(0x001677c0), 0, 30, 3)); }
                w8(m + 0x20, u(st));
                return m;
            }, 60);
        }

        // ---- P18.4 (part 1): the level's last own classes hand-written: the troopers 1454 (veldin_finale_troopers.cpp), the
        // scenes' classes 1432, 1434, 1563, 1568, 1750, 1799 (veldin_finale_scene.cpp), the rafts' platforms 1584
        // (veldin_finale_raft.cpp), the robots 1906 (veldin_finale_robots.cpp), each through its states as the level and its
        // scenes set them, each helper at its own address. The scene record L18:0x16d260 (+0x30 the scene, +0x34 its frame)
        // dealt with the frames the classes test, frame-scaled at the case's own frame time; its mobys +0x178 .. +0x194 the
        // level's own (the boss, the troopers, the robots); class 1568 has no placed moby: spawned here (its grid rect "not in
        // the grid"). The checks set the level's frame counter themselves (owncheck_k).
        auto owncheck_k = [level](const char* name, uint32_t addr, void (*port)(uint32_t), std::function<uint32_t()> make, int n = 60) {
            if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && only_mismatch(only, name)) return;
            l18check(name, addr, n, Ret::None, [level, make, port] {
                static std::vector<uint8_t> saved;
                uint32_t m = 0;
                for (int attempt = 0; attempt < 60; ++attempt) {
                    level();
                    w32(0x0015f5cc, R() & 0xffff);
                    m = make();
                    if (m == 0) continue;
                    seed_rand();
                    uint8_t* ram = ee_get_ram_base();
                    saved.assign(ram, ram + 0x02000000);
                    const uint32_t stack_top = ee_stack_ptr();
                    const bool ok = run_trial(port, m);
                    ee_stack_ptr() = stack_top;
                    std::memcpy(ram, saved.data(), saved.size());
                    if (ok) break;
                    if (std::getenv("B270_REDEAL") != nullptr) std::printf("  (re-dealt)\n");
                }
                return Args{{m}, {}};
            }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
        };
        auto fast_turn = [] { if (ri(0, 3) == 0) wf(0x0015ed70, rf(0.05f, 2.0f)); };  // (the frame time's square raised: the turns' top speeds show)
        auto real_hit = [](uint32_t m, float dmg) {  // a hit of kind 0x330000 naming `m` with damage `dmg`
            const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
            w8(m + 0xa4, idx);
            w32(e + 0x34, m);
            w32(e + 0x24, 0x00330000u | (R() & 0xff));
            wf(e + 0x2c, dmg);
            wfrand(e, 4, -1, 1);
            w32(e + 0x28, u(ri(0, 20)));
        };
        const uint32_t REC = 0x0016d260;  // (L18:0x16d260: the scene record)
        auto scene_mobys = [=] {  // the scene's mobys +0x178 .. +0x194: the level's own (a few left 0)
            std::vector<uint32_t> pool = mobys_with(0x002f2bf0);
            for (const uint32_t o : mobys_with(0x002f7c40)) pool.push_back(o);
            for (const uint32_t o : mobys_with(0x002fbb98)) pool.push_back(o);
            for (const uint32_t o : mobys_with(0x002f7ad8)) pool.push_back(o);
            for (uint32_t k = 0x178; k <= 0x194; k += 4) w32(REC + k, ri(0, 5) == 0 || pool.empty() ? 0u : pick(pool));
        };
        auto in_scene = [=](int scene, int32_t frame) {  // the level in a scene (mode 2: 5 cases in 6), its scene and frame
            w32(0x0015f5c4, ri(0, 5) == 0 ? u(ri(0, 3)) : 2u);
            w32(REC + 0x30, u(scene));
            w32(REC + 0x34, u(frame));
        };
        if (std::getenv("B270_OWN_INFO") != nullptr) {
            level();
            for (const uint32_t c : {0x002f7ab0u, 0x002f7ad8u, 0x002f7c40u, 0x002f88e8u, 0x002fa728u, 0x002fad08u, 0x002fad28u, 0x002fbb98u})
                for (const uint32_t m : mobys_with(c)) {
                    const Vec4 p = VV(m + 0x10);
                    std::printf("INFO spot class fn 0x%08x moby %u uid %d class %d at %.1f %.1f %.1f\n", c, (m - first_moby()) >> 8,
                                static_cast<int>(ee_read<int16_t>(m + 0xb2)), static_cast<int>(ee_read<int16_t>(m + 0xa6)), p.x, p.y, p.z);
                }
        }

        // class 1454, the troopers: set up (its two pieces) or not; walking toward its path's point (within 1 of it in part),
        // aiming with the animation at its firing frames 4 / 10 or not, the hero within 10, 10 .. 30, beyond; its target moby
        // kept or not, lost or not; its health about the hit's damage (killed in part)
        auto deal1454 = [=]() -> uint32_t {
            const std::vector<uint32_t> ms = mobys_with(0x002f7c40);
            if (ms.empty()) return 0u;
            fifty();
            fast_turn();
            const uint32_t m = pick(ms), pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 2) != 0) set_up(m, vf_class1454_update_l18);
            const int st = ri(0, 5) == 0 ? 0 : ri(1, 2);
            const uint32_t path = lvd::path(ee_read<int32_t>(pv + 0x100));
            const int32_t n = ee_read<int32_t>(path);
            if (n > 0) {
                w32(pv + 0x120, u(ri(0, n - 1)));
                if (coin()) put(m + 0x10, near(VV(path + 0x10 + (ee_read<uint32_t>(pv + 0x120) << 4)), 0, 1.3f, 0.3f));
            }
            wf(m + 0x48, rf(-3.1f, 3.1f));
            wf(pv + 0x12c, coin() ? 0.0f : rf(0, 0.4f));
            wf(pv + 0x128, rf(-0.2f, 0.2f));
            wf(pv + 0x110, rf(-3.1f, 3.1f)); wf(pv + 0x118, rf(-1, 1));
            wf(pv + 0x114, rf(-0.3f, 0.3f)); wf(pv + 0x11c, rf(-0.3f, 0.3f));
            wf(pv + 0x130, rf(3, 35)); wf(pv + 0x134, rf(-3.1f, 3.1f));
            const int far = ri(0, 2);
            hero_at(near(VV(m + 0x10), far == 0 ? 0.5f : far == 1 ? 10.5f : 31, far == 0 ? 9.5f : far == 1 ? 29 : 60, 2));
            if (ri(0, 5) == 0) { Vec4 q = near(VV(m + 0x10), 0.5f, 6, 0); q.z += rf(8, 20); hero_at(q); }  // (high above it: the aim's pitch at its lower clamp)
            const uint32_t hm = ee_read<uint32_t>(0x001413d0);
            w32(pv + 0xb0, ri(0, 2) == 0 ? 0u : coin() ? hm : any_moby());
            w32(pv + 0xb4, coin() ? 2u : 0u);
            if (st == 2 && ri(0, 2) != 0) {  // (aiming: the animation blending into its time 4 or 10 (a shot) or another)
                moby_anim_set(m, 2, 0, 0);
                w8(m + 0x52, 1);
                wf(m + 0x54, ri(0, 3) != 0 ? (coin() ? 4.0f : 10.0f) + (coin() ? 0.0f : rf(0, 0.4f)) : rf(0, 12));
                wf(m + 0x5c, rf(0.5f, 1.0f));  // (a frame's step of the animation, as its update leaves it)
            }
            else if (coin()) moby_anim_set(m, u(ri(1, 2)), ri(0, 12), 0);
            wf(pv + 0x20, coin() ? rf(0.2f, 1) : rf(1, 6));
            if (ri(0, 3) != 0) real_hit(m, coin() ? rf(0.5f, 6) : 0.0f);
            else give_hit(m, 0x330000u);
            w8(m + 0x20, u(st));
            return m;
        };
        owncheck("own1454_trooper", 0x002f7c40, vf_class1454_update_l18, deal1454, 160);
        helpercheck("own1454_frame", 0x002f8050, Ret::None, [=] { return Args{{deal1454()}, {}}; }, [](A a) { vf_trooper_frame_l18(a.i[0]); return uint64_t{0}; }, 120);
        helpercheck("own1454_walk", 0x002f8658, Ret::None, [=] { return Args{{deal1454()}, {}}; }, [](A a) { vf_trooper_walk_l18(a.i[0]); return uint64_t{0}; }, 120);
        // the rocket's spawner (L18:0x2a75f0): from a trooper or another moby, at a point and velocity, homing or not
        helpercheck("own41_spawn", 0x002a75f0, Ret::Int32, [=] {
            fifty();
            const std::vector<uint32_t> tr = mobys_with(0x002f7c40);
            const uint32_t owner = !tr.empty() && coin() ? pick(tr) : any_moby();
            put(SCR, near(ee_ref<Vec4>(0x0013f3d0), 1, 12, 3));
            put(SCR + 0x10, Vec4{rf(-0.6f, 0.6f), rf(-0.6f, 0.6f), rf(-0.6f, 0.3f), rf(-1, 1)});
            return Args{{owner, SCR, SCR + 0x10, u(ri(0, 1))}, {}};
        }, [](A a) { return r32(vf_rocket_spawn_l18(a.i[0], a.i[1], a.i[2], a.i[3])); });

        // class 1906, the robots: set up (hidden, state 8) then each state; the level's frame counter at the robots' word or
        // not (the draw L18:0x2fca98 queued); the hero in his state 0x72 or a scene (held hidden), in his mode 0xf (his
        // record's point: a real path record) or not, within its path's area or not, within 1.5 / 5 / 10 of it or far; its
        // height about 100 and its floor; its strike's frame 14 and end; its knock taken from a real hit by the port (state 5);
        // its pop running or not
        auto deal1906 = [=]() -> uint32_t {
            const std::vector<uint32_t> ms = mobys_with(0x002fbb98);
            if (ms.empty()) return 0u;
            fifty();
            fast_turn();
            const uint32_t m = pick(ms), pv = ee_read<uint32_t>(m + 0x78), H = 0x0013f350;
            if (ri(0, 5) != 0) set_up(m, vf_class1906_update_l18);
            static const int sts[] = {0, 1, 1, 2, 3, 3, 4, 4, 5, 6, 7, 8};
            const int st = sts[ri(0, 11)];
            const uint32_t path = lvd::path(ee_read<int32_t>(pv + 0x1e0));
            const int32_t n = ee_read<int32_t>(path);
            if (n > 0 && coin()) {  // the hero within the path's area (its points' middle) or near the robot
                Vec4 c{};
                for (int32_t k = 0; k < n; ++k) { const Vec4 q = VV(path + 0x10 + 0x10 * u(k)); c.x += q.x; c.y += q.y; c.z += q.z; }
                c.x /= static_cast<float>(n); c.y /= static_cast<float>(n); c.z /= static_cast<float>(n);
                put(m + 0x10, near(c, 0, 12, 0.5f));
            }
            const int d = ri(0, 3);
            hero_at(near(VV(m + 0x10), d == 0 ? 0.2f : d == 1 ? 1.6f : d == 2 ? 5.2f : 11, d == 0 ? 1.4f : d == 1 ? 4.8f : d == 2 ? 9.8f : 60, 1));
            if (st == 4 && coin()) hero_at(near(VV(m + 0x10), 2.2f, 3.3f, 0.3f));  // (the strike over at about its 2.5)
            if (ri(0, 7) == 0) w32(0x001413d4, 0x72);
            if (ri(0, 7) == 0) w32(0x0015f5c4, 2);
            else if (coin()) w32(0x0015f5c4, u(ri(0, 1)));
            if ((ri(0, 7) == 0 || ((st == 3 || st == 4) && ri(0, 3) == 0)) && n > 0) {  // (his record: a real path's, its last point)
                w32(H + 0x208c, 0xf);
                w32(H + 0x560, path);
                if (coin()) {  // (he away, outside the path's area; the robot by that point)
                    hero_at(near(VV(path + 0x10 * u(n)), 60, 120, 1));
                    put(m + 0x10, near(VV(path + 0x10 * u(n)), 0, 6, 0.5f));
                }
            }
            if (coin()) w32(0x00162604, ee_read<uint32_t>(0x0015f5cc));
            if (coin()) wf(m + 0x18, rf(94, 106));
            wf(pv + 0x1f8, rf(-3.1f, 3.1f));
            wf(pv + 0x1f4, coin() ? 0.0f : rf(0.01f, 1));
            wf(pv + 0x1f0, rf(0, 0.3f)); wf(pv + 0x1e8, rf(-0.2f, 0.2f)); wf(m + 0x48, rf(-3.1f, 3.1f));
            wf(pv + 0x20, coin() ? rf(0.2f, 1) : 1.0f);
            if (st == 5) {  // the knock as the port takes it: a fatal hit in state 2, run until the coin falls on the knock
                bool ok = false;
                for (int k = 0; k < 6 && !ok; ++k) {
                    w8(m + 0x20, 2); wf(pv + 0x20, 0.5f); real_hit(m, 5.0f);
                    w32(0x0015f5c4, 0); w32(0x001413d4, 0);
                    const uint32_t sp0 = ee_stack_ptr();
                    try { vf_robot_frame_l18(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
                    ok = ee_read<uint8_t>(m + 0x20) == 5;
                }
                if (!ok) return 0u;
                if (coin()) put(m + 0x10, near(VV(m + 0x10), 0, 0.5f, 0.5f));
            } else if (st == 1) {
                put(pv + 0x1d0, Vec4{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(-0.4f, 0.3f), 0});
                Vec4 q = VV(m + 0x10);
                if (ri(0, 2) == 0) { q.x += 600; q.z = rf(2, 9.9f); }  // (thrown off the arena, over no floor: below 10 it blows up)
                else q.z = floor_probe_v(0.5f, Vec4{q.x, q.y, q.z + 1, q.w}, 0) + rf(-0.6f, 0.6f);
                put(m + 0x10, q);
                if (coin()) w8(m + 0x53, u(ri(0, 2)));
            } else if (st == 4 && ri(0, 3) != 0) {
                moby_anim_set(m, 2, 0, 0);
                if (ri(0, 2) != 0) { w8(m + 0x52, 1); wf(m + 0x54, ri(0, 2) != 0 ? 14.0f : rf(0, 20)); wf(m + 0x5c, rf(0.5f, 1.0f)); }  // (blending into the strike: its time at 14 or another, a frame's step)
            } else if (st == 3 && coin()) {
                moby_anim_set(m, u(ri(0, 2)), 0, 0);
            }
            if (st == 4 || st == 3) w8(m + 0x70, coin() ? 2u : 0u);
            if (st != 5) {
                if (ri(0, 2) != 0) real_hit(m, coin() ? rf(0.3f, 3) : 0.0f);
                else give_hit(m, 0x330000u);
            }
            if (st != 8) w16(m + 0x34, ee_read<uint16_t>(m + 0x34) & ~0x41u);
            w8(m + 0x20, u(st));
            return m;
        };
        owncheck_k("own1906_robot", 0x002fbb98, vf_class1906_update_l18, deal1906, 300);
        helpercheck("own1906_frame", 0x002fc100, Ret::None, [=] { return Args{{deal1906()}, {}}; }, [](A a) { vf_robot_frame_l18(a.i[0]); return uint64_t{0}; }, 120);
        helpercheck("own1906_walk", 0x002fc4e0, Ret::Float, [=] {
            const uint32_t m = deal1906();
            if (m == 0) return Args{{0}, {}};
            put(SCR, coin() ? VV(ee_read<uint32_t>(m + 0x78) + 0x180) : near(VV(m + 0x10), 0, 8, 1));
            return Args{{m, SCR}, {}};
        }, [](A a) { return uint64_t{f2u(vf_robot_walk_l18(a.i[0], a.i[1]))}; }, 120);

        // class 1563, the end scenes' effects: set up (its windows) then in scene 0 .. 7 at the frames it tests, frame-scaled at
        // the case's frame time; scene 7's windows copied (its frame 1) or not; the class 0x59b moby on the drawn chain or not;
        // the previous joint points of scene 5 near the moby's or the level's; the camera set up for scene 3's ring in half
        auto deal1563 = [=]() -> uint32_t {
            const std::vector<uint32_t> ms = mobys_with(0x002f88e8);
            if (ms.empty()) return 0u;
            fifty();
            const uint32_t m = ms[0];
            if (ri(0, 7) != 0) set_up(m, vf_class1563_update_l18);
            scene_mobys();
            auto fs = [](int32_t f) { return frame_scale_i(f); };
            auto win = [](uint32_t a) { return ee_read<int32_t>(a); };
            static const int scenes[] = {4, 4, 4, 4, 7, 7, 5, 5, 1, 1, 3, 3, 0, 2, 6};
            const int scene = scenes[ri(0, 14)];
            int32_t f = ri(0, 900);
            if (scene == 4) {
                const int32_t c[] = {1, 2, fs(0x1f4) - 1, fs(0x1f4), fs(0x200), fs(0x285) - 1, fs(0x285), fs(0x2a0), fs(0x2c2), fs(0x2c6), fs(0x2d0), fs(0x2da),
                                     fs(0x2da) + 1, fs(0x2e0), fs(0x2e0) + 1, fs(0x2ec), fs(0x35c), fs(0x35c) + 1,
                                     fs(win(0x001624bc)) + ri(-1, 30), fs(win(0x001624c0)) - ri(0, 2), fs(win(0x001624c4)) + ri(-1, 40),
                                     fs(win(0x001624c8)) - ri(0, 2), fs(win(0x00162498)) + ri(-1, 4), fs(win(0x0016249c)) + ri(-2, 2), fs(win(0x001624a0)) - ri(0, 2)};
                const int32_t blast[] = {fs(0x2c2), fs(0x2c6), fs(0x2d0), fs(0x2da), fs(0x2ec)};
                if (ri(0, 2) == 0) f = blast[ri(0, 4)];
                else if (ri(0, 4) != 0) f = c[ri(0, 24)];
            } else if (scene == 7) {
                if (coin()) for (uint32_t k = 0; k < 6; ++k) {  // (the windows as its frame 1 left them)
                    static const uint32_t to[] = {0x001624bc, 0x001624c0, 0x001624c4, 0x001624c8, 0x00162498, 0x0016249c};
                    w32(to[k], ee_read<uint32_t>(0x001624a4 + 4 * k));
                }
                const int32_t c[] = {1, 1, win(0x001624bc) + ri(-1, 30), fs(win(0x001624c0)) - ri(0, 2), win(0x001624c4) + ri(-1, 40),
                                     fs(win(0x001624c8)) - ri(0, 2), fs(win(0x00162498)) + ri(-1, 4), win(0x0016249c) + ri(-2, 2), fs(win(0x001624a0)) - ri(0, 2)};
                if (ri(0, 4) != 0) f = c[ri(0, 8)];
            } else if (scene == 5) {
                const int32_t c[] = {fs(0x8a) - 1, fs(0x8a), fs(0x8c), fs(0x8c) + 1, fs(0x8c) + ri(2, 100), fs(0x108), fs(0x108) + 1};
                f = c[ri(0, 6)];
                const uint32_t o = ee_read<uint32_t>(REC + 0x180);
                if (o != 0 && coin()) {
                    Vec4 a{}, b{};
                    moby_chain_point_v(o, 0, a);
                    moby_chain_point_v(o, 2, b);
                    put(0x00162530, near(a, 0, 2, 1));
                    put(0x00162540, near(b, 0, 2, 1));
                }
            } else if (scene == 1) {
                const int32_t c[] = {fs(0x1be) - 1, fs(0x1be), fs(0x1be) + ri(1, 40), fs(0x1f6) - 1, fs(0x1f6), ri(0, 60)};
                f = c[ri(0, 5)];
            } else if (scene == 3) {
                const int32_t c[] = {fs(0x24a) - 1, fs(0x24a), fs(0x24a), fs(0x24a) + 1, ri(0, 500)};
                f = c[ri(0, 4)];
                if (coin()) {
                    if (ee_read<uint16_t>(0x001518d0) == 0) { w16(0x001518d0, 640); w16(0x001518d2, 448); }
                    screen_setup();
                    projection_setup();
                    render_view_setup();
                }
            }
            in_scene(scene, f);
            if (coin()) {  // a moby of class 0x59b on the drawn chain
                std::vector<uint32_t> chain;
                for (uint32_t o = ee_read<uint32_t>(0x0015ffe4); o != 0 && chain.size() < 400; o = ee_read<uint32_t>(o + 0x28)) chain.push_back(o);
                if (!chain.empty()) w16(pick(chain) + 0xa6, 0x59b);
            }
            if (ri(0, 7) == 0) w8(m + 0x20, u(ri(0, 2)));
            return m;
        };
        {  // (scene 1's beam, morph_2bd0a0 (morph.cpp, Veldin's port): the w lanes of its helix points (L18:0x1d0ea0 .. 0x1d1020,
           // Veldin 0x1dc020 / 0x1dc0e0: mat4_transform of a frame whose rows' w lanes the original's VU0 leaves stale, the port's 0)
           // and of its rings' points (L18:0x1d1020 + 0x140 x segment 1 .. 11, 20 points each, Veldin 0x1dc1a0: vec3_add's sqc2
           // of a stale lane, the port's 0): differences of that shared port (the generated class 1563 called it the same way),
           // not of this task's code; left out of this check only)
            const size_t ign_morph = g_ignored_words.size();
            for (uint32_t k = 0; k < 24; ++k) g_ignored_words.push_back(0x001d0ea0 + 0x10 * k + 0xc);
            for (uint32_t k = 0; k < 12 * 20; ++k) g_ignored_words.push_back(0x001d1020 + 0x140 * (k / 20) + 0x10 * (k % 20) + 0xc);
            // (and the w lane at +0x1c of the particle records the beam's spawn_particle_35 (Veldin 0x272060's port, called by
            // morph_2bd0a0) fills: the original's L18:0x274144 sq of a register whose upper lane is stale; the pool of 0x800 records
            // of 0x40 bytes at *(0x16016c), whose +0x2c the batch already leaves out for the same kind of lane)
            {
                level();
                const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
                for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x1c);
            }
            // (and the beam's line against the world, segment_collide's record OV(0x173e90) .. 0x173ebc: the known difference
            // of that shared port, as the batches 514 / 520 leave it out)
            for (uint32_t k = 0; k < 0x30; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
            owncheck_k("own1563_scene", 0x002f88e8, vf_class1563_update_l18, deal1563, 240);
            g_ignored_words.resize(ign_morph);
        }
        helpercheck("own1563_sparks", 0x002fa250, Ret::None, [=] {
            fifty();
            const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
            put(SCR, near(h, 0, 6, 2));
            put(SCR + 0x10, coin() ? VV(SCR) : near(h, 0, 6, 2));
            put(SCR + 0x20, Vec4{rf(-0.2f, 0.2f), rf(-0.2f, 0.2f), rf(-0.2f, 0.2f), 0});
            return Args{{SCR, SCR + 0x10, coin() ? 0x0015f580 : SCR + 0x20}, {rf(0.05f, 2), rf(0.05f, 2)}};
        }, [](A a) { vf_scene_sparks_l18(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1]); return uint64_t{0}; }, 60);

        // classes 1432, 1434, 1568: shown and hidden by the scenes (scene 3), the level's first frames with L18:0x1623a0
        // set, turned or not
        auto deal_prop = [=](uint32_t fn, int states) -> uint32_t {
            std::vector<uint32_t> ms = mobys_with(fn);
            uint32_t m = 0;
            if (fn == 0x002fa640) {  // class 1568: none placed, one spawned near the hero
                m = moby_spawn(0x620);
                if (m == 0) return 0u;
                w32(m + 0xa0, 0x80807f7fu);  // ("not in the grid")
                put(m + 0x10, near(ee_ref<Vec4>(0x0013f3d0), 2, 20, 1));
                wf(m + 0x40, coin() ? 0.0f : rf(-3.1f, 3.1f));
            } else {
                if (ms.empty()) return 0u;
                m = pick(ms);
            }
            fifty();
            in_scene(ri(0, 2) == 0 ? 3 : ri(0, 7), ri(0, 600));
            if (ri(0, 3) == 0) w32(0x0015f5c4, u(ri(0, 1)));
            w32(0x0015f5cc, ri(0, 2) == 0 ? u(ri(0, 6)) : R() & 0xffff);
            w32(0x001623a0, coin() ? 0u : 1u);
            wf(m + 0x40, coin() ? 0.0f : rf(-3.1f, 3.1f));
            w8(m + 0x20, u(ri(0, states)));
            if (coin()) w16(m + 0x34, ee_read<uint16_t>(m + 0x34) ^ 1u);
            return m;
        };
        owncheck_k("own1432_prop", 0x002f7ab0, vf_class1432_update_l18, [=] { return deal_prop(0x002f7ab0, 2); }, 40);
        owncheck_k("own1434_prop", 0x002f7ad8, vf_class1434_update_l18, [=] { return deal_prop(0x002f7ad8, 3); }, 160);
        owncheck_k("own1568_prop", 0x002fa640, vf_class1568_update_l18, [=] { return deal_prop(0x002fa640, 3); }, 100);

        // class 1799, the smoke of the scene's moby: scenes 0 .. 5 (and a scene number past them), the scene's mobys real or 0
        owncheck_k("own1799_smoke", 0x002fad28, vf_class1799_update_l18, [=]() -> uint32_t {
            const std::vector<uint32_t> ms = mobys_with(0x002fad28);
            if (ms.empty()) return 0u;
            fifty();
            const uint32_t m = ms[0];
            if (ri(0, 5) != 0) set_up(m, vf_class1799_update_l18);
            scene_mobys();
            for (const uint32_t k : {0x184u, 0x188u})  // (the two it smokes from: a scene's actors: the boss (its joints 8 / 9), another in part)
                if (ri(0, 7) != 0) w32(0x0016d260 + k, mobys_with(0x002f2bf0).empty() ? first_moby() : mobys_with(0x002f2bf0)[0]);
            in_scene(ri(0, 7) == 0 ? -1 : ri(0, 5), ri(0, 600));
            if (ri(0, 7) == 0) w8(m + 0x20, u(ri(0, 2)));
            return m;
        }, 80);
        // class 1750, the save image: built once (L18:0x1ba7d0 clear) or already kept
        owncheck_k("own1750_save", 0x002fad08, vf_class1750_update_l18, [=]() -> uint32_t {
            const std::vector<uint32_t> ms = mobys_with(0x002fad08);
            if (ms.empty()) return 0u;
            w32(0x001ba6f0 + 0xe0, coin() ? 0u : 0x001dfc10);
            return ms[0];
        }, 30);
        helpercheck("own1750_image", 0x00281fa8, Ret::None, [=] {
            w32(0x001ba6f0 + 0xe0, coin() ? 0u : SCR);
            return Args{{coin() ? 0x001dfc10 : SCR + 0x1000}, {}};
        }, [](A a) { vf_save_image_once_l18(a.i[0]); return uint64_t{0}; }, 30);

        // class 1584, the rafts' platforms: the rafts set up (their platforms bound), a platform moved and turned since its
        // last frame or not; its +0xbc and +0x80 set or not, its ground moby (class 1892) there or not
        owncheck_k("own1584_platform", 0x002fa728, vf_class1584_update_l18, [=]() -> uint32_t {
            const std::vector<uint32_t> ms = mobys_with(0x002fa728);
            if (ms.empty()) return 0u;
            fifty();
            if (ri(0, 3) != 0) for (const uint32_t r : mobys_with(0x002f16f0)) set_up(r, vf_class1381_update_l18);
            const uint32_t m = pick(ms), pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 4) != 0) set_up(m, vf_class1584_update_l18);
            if (ri(0, 7) == 0) w8(m + 0x20, 2);
            if (coin()) { put(m + 0x10, near(VV(m + 0x10), 0, 0.5f, 0.2f)); wf(m + 0x48, ee_read<float>(m + 0x48) + rf(-0.2f, 0.2f)); }
            w8(m + 0xbc, coin() ? 1u : 0u);
            w32(pv + 0x80, coin() ? 1u : 0u);
            if (ri(0, 3) == 0) {
                const uint32_t g = moby_spawn(0x764);
                if (g != 0) { w32(g + 0xa0, 0x80807f7fu); put(g + 0x10, near(VV(m + 0x10), 0, 1, 0.2f)); }
                w32(pv + 0x84, g);
            } else {
                w32(pv + 0x84, 0);
            }
            return m;
        }, 120);

        // ---- P18.4 (part 2): three of the level's draw callbacks hand-written (veldin_finale_draws.cpp): the robots' pop
        // (0x2fc860) and glows (0x2fca98), the scene's glows on its moby (0x2f9a98). Each at its own address on the moby its
        // class queues it for, the screen and projection set up and the camera seeing the subject (from behind it, then about
        // it) or away, the packet cursor on a buffer of the test area (0x800000 + up to 8 quadwords, its end 0x8f0000) so
        // quad_draw's and glow_draw's packets are written there and compared. VU0: no instruction of their own; the last VU0
        // writers in the original's run are their callees (quad_draw, glow_draw, moby_chain_point_v / _world_one, the vec3
        // helpers, sin_vu, rgba_lerp), whose host ports keep no VU0 state, as in the generated code before: no lane compared.
        auto screen_ready = [] {
            if (ee_read<uint16_t>(0x001518d0) == 0) { w16(0x001518d0, 640); w16(0x001518d2, 448); }
            screen_setup();
            projection_setup();
        };
        auto camera_sees = [=](const Vec4& at, float r, float spread) {
            screen_ready();
            for (int k = 0; k < 60; ++k) {
                const Vec4 fwd = VV(OV(0x00166fd0));
                const float back = rf(4.0f, 30.0f);
                VV(OV(0x00166dc0)) = k < 30 ? Vec4{at.x - fwd.x * back + rf(-2, 2), at.y - fwd.y * back + rf(-2, 2), at.z - fwd.z * back + rf(-1, 1), 1.0f}
                                            : Vec4{at.x + rf(-spread, spread), at.y + rf(-spread, spread), at.z + rf(-spread / 4, spread / 4), 1.0f};
                render_view_setup();
                put(SCR + 0x40, at);
                if (point_on_screen(SCR + 0x40, r) != -1) return true;
            }
            return false;
        };
        auto camera_away = [=](const Vec4& at) {
            screen_ready();
            VV(OV(0x00166dc0)) = Vec4{at.x + 400.0f, at.y + 400.0f, at.z + 300.0f, 1.0f};
            render_view_setup();
        };
        auto view_on = [=](const Vec4& at) {  // the camera on `at` (near: quads partly off screen, the clipper) or away; the cursor
            if (ri(0, 4) != 0) camera_sees(at, coin() ? 0.5f : 3.0f, coin() ? 3.0f : 12.0f); else camera_away(at);
            w32(OV(0x001611c0), 0x00800000u + u(ri(0, 8)) * 0x10u);
            w32(OV(0x0015f5f8), 0x008f0000u);
        };
        // the robot's pop: a robot dealt as own1906_robot's, its pop running (or at its ends), the level's sizes dealt in part
        helpercheck("own1906_cb_2fc860", 0x002fc860, Ret::None, [=] {
            const uint32_t m = deal1906();
            if (m == 0) return Args{{0}, {}};
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            wf(pv + 0x1f4, ri(0, 5) == 0 ? (coin() ? 0.0f : 1.0f) : rf(0.01f, 1));
            if (ri(0, 3) == 0) { wf(0x0016264c, rf(-0.5f, 3)); wf(0x00162650, rf(-0.5f, 3)); wf(0x00162648, rf(-1, 2)); }
            view_on(VV(m + 0x10));
            return Args{{m}, {}};
        }, [](A a) { vf_cb_2fc860_l18(a.i[0]); return uint64_t{0}; }, 200);
        // the robots' glows: a robot's group with its robots in states 0 .. 8, their colours and the alpha dealt in part
        helpercheck("own1906_cb_2fca98", 0x002fca98, Ret::None, [=] {
            const std::vector<uint32_t> ms = mobys_with(0x002fbb98);
            if (ms.empty()) return Args{{0}, {}};
            fifty();
            const uint32_t m = pick(ms);
            const int32_t g = ee_read<uint8_t>(m + 0x21);
            for (const uint32_t o : group_mobys(g)) {
                if (ee_read<int16_t>(o + 0xa6) != 0x772) continue;
                w8(o + 0x20, u(ri(0, 3) == 0 ? ri(0, 8) : ri(1, 5)));
                if (coin()) w32(o + 0x90, R());
            }
            if (coin()) w32(0x00162654, u(ri(0, 0x80)));
            view_on(VV(m + 0x10));
            return Args{{m}, {}};
        }, [](A a) { vf_cb_2fca98_l18(a.i[0]); return uint64_t{0}; }, 200);
        // the scene's glows: on the boss (the scene's moby: its joints 1 / 2) or another of the level's, the level's counter
        // anywhere, the pulse's period and sizes the level's or dealt
        helpercheck("own1563_cb_2f9a98", 0x002f9a98, Ret::None, [=] {
            const std::vector<uint32_t> boss = mobys_with(0x002f2bf0);
            fifty();
            const uint32_t m = !boss.empty() && ri(0, 3) != 0 ? boss[0] : pick(mobys_with(0x002f7c40));
            if (m == 0) return Args{{0}, {}};
            w32(0x0015f5cc, R() & 0xffff);
            if (ri(0, 3) == 0) w32(0x00162460, u(ri(1, 120)));
            if (ri(0, 3) == 0) { wf(0x00162450, rf(0.1f, 3)); wf(0x00162454, rf(0, 1)); wf(0x00162500, rf(-1, 2)); }
            view_on(VV(m + 0x10));
            return Args{{m}, {}};
        }, [](A a) { vf_cb_2f9a98_l18(a.i[0]); return uint64_t{0}; }, 200);
        // the scene's flat glow: on the class 0x59b moby (a moby of the level, as the class loop's) at the scene's window
        // frames (its first frame + 1: the offset reset), the offset, sizes, colours and the window's alpha dealt in part
        helpercheck("own1563_cb_2f9c20", 0x002f9c20, Ret::None, [=] {
            fifty();
            const std::vector<uint32_t> pool = mobys_with(0x002f7ad8);
            const uint32_t m = !pool.empty() && coin() ? pick(pool) : any_moby();
            const int32_t first = frame_scale_i(ee_read<int32_t>(0x001624bc) + 1);
            w32(0x0016d294, u(coin() ? first : first + ri(-3, 40)));
            if (coin()) put(0x00162560, Vec4{rf(-3, 3), rf(-3, 3), rf(-50, 5), rf(0, 1)});
            if (ri(0, 3) == 0) { wf(0x00162508, rf(0.2f, 6)); wf(0x0016250c, rf(0.2f, 6)); }
            if (coin()) { w32(0x00162504, R() & 0xffffff); w32(0x00162510, R() & 0xffffff); }
            w32(0x00162524, u(ri(0, 0xff)));
            Vec4 c = VV(m + 0x10);
            if (coin()) c.z -= 45;
            view_on(c);
            return Args{{m}, {}};
        }, [](A a) { vf_cb_2f9c20_l18(a.i[0]); return uint64_t{0}; }, 200);
        // the scene's halo before the camera (its moby unused): at the window's first frame + 1 (the offset reset, scene 7 or
        // another) or not, the offset, the size and colours dealt in part, the camera anywhere the view is set up
        helpercheck("own1563_cb_2f9780", 0x002f9780, Ret::None, [=] {
            fifty();
            const int32_t first = frame_scale_i(ee_read<int32_t>(0x00162498) + 1);
            w32(0x0016d294, u(coin() ? first : first + ri(-3, 40)));
            w32(0x0016d290, coin() ? 7u : 4u);
            if (coin()) put(0x00162550, Vec4{rf(0.5f, 3), rf(-1, 1), rf(-1, 1), rf(0, 1)});
            wf(0x00162520, coin() ? 10.0f : rf(0.2f, 12));
            if (coin()) { w32(0x001624f8, R()); w32(0x001624fc, R()); }
            const uint32_t m = any_moby();
            view_on(VV(m + 0x10));
            return Args{{m}, {}};
        }, [](A a) { vf_cb_2f9780_l18(a.i[0]); return uint64_t{0}; }, 200);
        // the scene's beam glow: on a moby of the level turned anywhere, the offset, the lengths, the width and colours dealt
        helpercheck("own1563_cb_2f9eb8", 0x002f9eb8, Ret::None, [=] {
            fifty();
            const std::vector<uint32_t> pool = mobys_with(0x002f7ad8);
            const uint32_t m = !pool.empty() && coin() ? pick(pool) : any_moby();
            if (coin()) { wf(m + 0x40, rf(-3.1f, 3.1f)); wf(m + 0x44, rf(-1, 1)); wf(m + 0x48, rf(-3.1f, 3.1f)); euler_to_mat3_vu(MM(m + 0xc0), VV(m + 0x40)); }
            if (coin()) put(0x00162560, Vec4{rf(-3, 3), rf(-3, 3), rf(-50, 5), rf(0, 1)});
            if (ri(0, 3) != 0) { wf(0x00162514, rf(0.2f, 30)); wf(0x00162518, rf(0.2f, 30)); wf(0x0016251c, rf(0.2f, 30)); }
            wf(0x0016252c, ri(0, 3) == 0 ? 0.25f : rf(0.25f, 200.25f));
            if (coin()) { w32(0x00162504, R()); w32(0x00162510, R()); }
            Vec4 c = VV(m + 0x10);
            if (coin()) c.z -= rf(0, 45);
            view_on(c);
            return Args{{m}, {}};
        }, [](A a) { vf_cb_2f9eb8_l18(a.i[0]); return uint64_t{0}; }, 200);

        // ---- P18.5: the level's last seven draw callbacks hand-written (veldin_finale_shot_draws.cpp: the shell's glow 0x2d5768,
        // the ring's 0x2dba20, the burst's 0x2dc4b8, the energy ball's glow 0x2ea1f8 that nothing served before; veldin_finale_
        // draws.cpp: the divers' trails 0x2f0390 and glows 0x2f06c0, the force field 0x2f1fd8, the boss's glows 0x2f7880). As
        // part 2's: each at its own address on the moby its class queues it for (the shots spawned through the boss's spawners,
        // P18.3's ports), the view set up on the subject or away, the packet cursor on the test area's buffer. VU0: no
        // instruction of their own (the notes of P18.5): no lane compared.
        auto chance = [](int n) { return ri(0, n - 1) == 0; };
        // the shell's glow: a shell from near the hero to a point by him (its pvars +0x10, the glow's place), its frame count
        // +0x2c anywhere (its low four bits: the pulse), the level's two colours dealt in part
        helpercheck("own564_cb_2d5768", 0x002d5768, Ret::None, [=] {
            fifty();
            const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
            Vec4 from = near(h, 4, 20, 0), to = near(h, 0, 4, 0.5f);
            from.z = h.z + rf(1, 10);
            put(SCR, from); put(SCR + 0x10, Vec4{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(0, 0.6f), 0}); put(SCR + 0x20, to);
            const uint32_t m = vf_shell_spawn_l18(SCR, SCR + 0x10, SCR + 0x20, ri(10, 90), u(ri(0, 7)), rf(-1.2f, 1.2f), rf(-1.2f, 1.2f));
            if (m == 0) return Args{{0}, {}};
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w32(pv + 0x2c, coin() ? u(ri(0, 15)) : R());
            if (chance(3)) { w32(0x00161980, R()); w32(0x00161984, R()); }
            view_on(to);
            return Args{{m}, {}};
        }, [](A a) { vf_cb_2d5768_l18(a.i[0]); return uint64_t{0}; }, 200);
        // the ring's: a ring spawned by the hero (its size, the size it grows to and its speed: the fade below 0, between, past 1),
        // the level's step (a quad every 30 degrees and others), lift, sizes and colours dealt in part
        helpercheck("own624_cb_2dba20", 0x002dba20, Ret::None, [=] {
            fifty();
            const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
            Vec4 at = near(h, 0, 8, 0);
            at.z = h.z + rf(-0.3f, 0.5f);
            put(SCR, at);
            put(SCR + 0x10, Vec4{rf(-0.2f, 0.2f), rf(-0.2f, 0.2f), rf(-0.3f, 0.2f), 0});
            vf_ring_spawn_l18(SCR, SCR + 0x10, 0, rf(3, 16), rf(0.05f, 0.5f));
            const std::vector<uint32_t> rings = mobys_with(0x002db460);
            if (rings.size() != 1) return Args{{0}, {}};
            const uint32_t m = rings[0], pv = ee_read<uint32_t>(m + 0x78);
            const float r = rf(0.2f, 12);
            wf(pv + 0x24, r);
            const int fade = ri(0, 3);  // (the fade below 0, between, past 1, the speed 0)
            wf(pv + 0x28, fade == 0 ? r - rf(0, 3) : fade == 1 ? r + rf(0, 4) : r + rf(4, 40));
            wf(pv + 0x20, fade == 3 && coin() ? 0.0f : rf(0.02f, 0.6f));
            if (chance(3)) wf(0x00161c64, coin() ? 30.0f : rf(25, 120));
            if (chance(3)) { wf(0x00161c68, rf(-1, 2)); wf(0x00161c70, rf(-1, 2)); wf(0x00161c74, rf(0, 3)); }
            if (chance(3)) { wf(0x00161c6c, rf(0, 2)); wf(0x00161c78, rf(0.5f, 4)); wf(0x00161c7c, rf(0.5f, 60)); }
            if (chance(3)) { w32(0x00161c5c, R()); w32(0x00161c60, R()); }
            view_on(at);
            return Args{{m}, {}};
        }, [](A a) { vf_cb_2dba20_l18(a.i[0]); return uint64_t{0}; }, 200);
        // the burst's: a burst about the hero (its radius, phase and frames left: the fade 0 .. past 1), the level's words dealt
        // in part
        helpercheck("own628_cb_2dc4b8", 0x002dc4b8, Ret::None, [=] {
            fifty();
            const float r = rf(0.5f, 8);
            const Vec4 at = near(ee_ref<Vec4>(0x0013f3d0), 0, 4, 0.5f);
            put(SCR, at);
            const uint32_t m = vf_burst_spawn_l18(0, SCR, r);
            if (m == 0) return Args{{0}, {}};
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            wf(pv + 0, r);
            wf(pv + 4, rf(-2, 3));
            w32(pv + 8, chance(4) ? u(ri(-3, 1)) : u(ri(0, 80)));
            if (chance(3)) w32(0x00161c9c, u(ri(1, 60)));
            if (chance(3)) wf(0x00161cc0, coin() ? 45.0f : rf(25, 120));
            if (chance(3)) { wf(0x00161cc4, rf(-1, 2)); wf(0x00161ccc, rf(-1, 2)); wf(0x00161cc8, rf(0, 2)); wf(0x00161cd4, rf(0.5f, 4)); }
            if (chance(3)) { w32(0x00161cb8, R()); w32(0x00161cbc, R()); }
            view_on(at);
            return Args{{m}, {}};
        }, [](A a) { vf_cb_2dc4b8_l18(a.i[0]); return uint64_t{0}; }, 200);
        // the energy ball's glow: a ball as own983_ball's (its size, its glow angles), the camera about it at every elevation,
        // the level's colours dealt in part
        if (std::getenv("B270_OWN_INFO") != nullptr) {  // (does the level run a spawned ball? its update word +0x74 and class record +0x24)
            level();
            put(SCR, VV(0x0013f3d0));
            const uint32_t b = vf_ball_spawn_l18(0, SCR, 100);
            if (b != 0)
                std::printf("INFO class 983 spawned: moby %u class index %u update 0x%08x class record 0x%08x flags 0x%04x\n", (b - first_moby()) >> 8,
                            u(ee_read<uint8_t>(b + 0x22)), ee_read<uint32_t>(b + 0x74), ee_read<uint32_t>(b + 0x24), u(ee_read<uint16_t>(b + 0x34)));
        }
        helpercheck("own983_cb_2ea1f8", 0x002ea1f8, Ret::None, [=] {
            const uint32_t m = make983();
            if (m == 0) return Args{{0}, {}};
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            wf(pv + 0x24, chance(4) ? 0.0f : rf(0.05f, 2.5f));
            wf(pv + 0x2c, rf(-3.1f, 3.1f));
            wf(pv + 0x30, rf(-3.1f, 3.1f));
            if (chance(3)) for (uint32_t k = 0; k < 4; ++k) w32(0x00161ef8 + k * 4, R());
            Vec4 c = VV(m + 0x10);
            c.z += rf(-6, 6);
            view_on(c);
            return Args{{m}, {}};
        }, [](A a) { vf_cb_2ea1f8_l18(a.i[0]); return uint64_t{0}; }, 200);
        // the divers' draws: a diver and its group (divers only: the trail's walk never leaves an entry that is not one), each
        // diver's trail points about it, their count 0 .. 16 and newest anywhere, its state 0 .. 5 and glow angle; the level's
        // two switches L18:0x162238 / 0x16223c on in part, its counter anywhere, its colours and sizes dealt in part
        auto deal_divers = [=]() -> uint32_t {
            const std::vector<uint32_t> ms = mobys_with(0x002efb88);
            if (ms.empty()) return 0u;
            fifty();
            const uint32_t m = pick(ms);
            const std::vector<uint32_t> g = group_mobys(ee_read<uint8_t>(m + 0x21));
            if (g.empty()) return 0u;
            for (const uint32_t o : g) if (ee_read<int16_t>(o + 0xa6) != 0x54b) return 0u;
            for (const uint32_t o : g) {
                const uint32_t pv = ee_read<uint32_t>(o + 0x78);
                if (chance(3)) set_up(o, vf_class1355_update_l18);
                w8(o + 0x20, u(ri(0, 5)));
                Vec4 p = VV(o + 0x10);
                for (uint32_t k = 0; k < 16; ++k) { p = near(p, 0, 1.2f, 0.4f); p.w = 1.0f; put(pv + 0x10 + k * 0x10, p); }
                w16(pv + 0x164, u(chance(5) ? 0 : ri(1, 16)));
                w16(pv + 0x166, u(ri(0, 15)));
                wf(pv + 0x168, rf(-3.1f, 3.1f));
                if (chance(4)) { wf(o + 0x40, rf(-1, 1)); wf(o + 0x48, rf(-3.1f, 3.1f)); moby_update_transform(o); }
            }
            w32(0x00162238, chance(8) ? 1u : 0u);
            w32(0x0016223c, chance(8) ? 1u : 0u);
            if (chance(3)) { w32(0x00162250, R()); w32(0x00162254, R()); w32(0x00162260, R()); w32(0x00162264, R()); }
            if (chance(3)) { wf(0x00162258, rf(-400, 400)); wf(0x0016225c, rf(-400, 400)); wf(0x00162268, rf(0.1f, 3)); wf(0x0016226c, rf(-2, 2)); }
            view_on(VV(m + 0x10));
            return m;
        };
        helpercheck("own1355_cb_2f0390", 0x002f0390, Ret::None, [=] { return Args{{deal_divers()}, {}}; },
                    [](A a) { vf_cb_2f0390_l18(a.i[0]); return uint64_t{0}; }, 200);
        helpercheck("own1355_cb_2f06c0", 0x002f06c0, Ret::None, [=] {  // (here a group member of another class in a quarter: skipped)
            const uint32_t m = deal_divers();
            if (m != 0 && chance(4)) {
                const std::vector<uint32_t> g = group_mobys(ee_read<uint8_t>(m + 0x21));
                w16(pick(g) + 0xa6, 0x772);
            }
            return Args{{m}, {}};
        },
                    [](A a) { vf_cb_2f06c0_l18(a.i[0]); return uint64_t{0}; }, 200);
        // the force field: a field set up or not, turned, its scroll about the wrap, its glow's cap, the camera within 48 or
        // farther (the glow's clamps), the level's speed, lift, rise and colours dealt in part
        helpercheck("own1392_cb_2f1fd8", 0x002f1fd8, Ret::None, [=] {
            const std::vector<uint32_t> ms = mobys_with(0x002f1e68);
            if (ms.empty()) return Args{{0}, {}};
            fifty();
            const uint32_t m = pick(ms), pv = ee_read<uint32_t>(m + 0x78);
            if (coin()) set_up(m, vf_class1392_update_l18);
            if (chance(3)) { wf(m + 0x48, rf(-3.1f, 3.1f)); moby_update_transform(m); }
            wf(pv + 8, coin() ? rf(0.97f, 1.0f) : rf(0, 1));
            wf(pv + 0xc, chance(5) ? 0.0f : rf(0.1f, 1.5f));
            if (chance(3)) { wf(0x001622e0, rf(0, 3)); wf(0x001622d0, rf(-1, 2)); wf(0x001622d4, rf(0, 2)); }
            if (chance(3)) { w32(0x001622c8, R()); w32(0x001622cc, R()); }
            Vec4 c = VV(m + 0x10);
            if (chance(3)) c.z += rf(30, 60);
            view_on(c);
            if (chance(4)) camera_away(c);
            else if (chance(2)) {  // (the camera 38 .. 50 back along its view: the glow between its clamps)
                const Vec4 fwd = VV(OV(0x00166fd0));
                const float d = rf(38, 50);
                VV(OV(0x00166dc0)) = Vec4{c.x - fwd.x * d, c.y - fwd.y * d, c.z - fwd.z * d, 1.0f};
                render_view_setup();
            }
            return Args{{m}, {}};
        }, [](A a) { vf_cb_2f1fd8_l18(a.i[0]); return uint64_t{0}; }, 200);
        // the boss's glows: the boss after its set-up (its attached mobys found) or with another of the level's mobys attached,
        // its animation 0xa .. 0xe (0xb .. 0xd: the extra pair), its glow colour, the pulses' periods and the level's counter
        // anywhere, the sizes and colours dealt in part
        helpercheck("own1422_cb_2f7880", 0x002f7880, Ret::None, [=] {
            const std::vector<uint32_t> boss = mobys_with(0x002f2bf0);
            if (boss.empty()) return Args{{0}, {}};
            fifty();
            const uint32_t m = boss[0], pv = ee_read<uint32_t>(m + 0x78);
            set_up(m, vf_class1422_update_l18);
            if (ee_read<uint32_t>(pv + 0x3a0) == 0 || chance(4)) w32(pv + 0x3a0, coin() ? m : pick(mobys_with(0x002f7c40)));
            if (ee_read<uint32_t>(pv + 0x3a0) == 0) return Args{{0}, {}};
            w8(m + 0x53, u(ri(0xa, 0xe)));
            w32(pv + 0x3b8, R());
            w32(0x0015f5cc, R() & 0xffff);
            if (chance(3)) { w32(0x00162460, u(ri(1, 120))); w32(0x00162474, u(ri(1, 120))); }
            if (chance(3)) { wf(0x00162450, rf(0.1f, 3)); wf(0x00162454, rf(0, 1)); wf(0x00162464, rf(0.1f, 3)); wf(0x00162468, rf(0, 1)); }
            if (chance(3)) { w32(0x00162458, R()); w32(0x0016245c, R()); w32(0x0016246c, R()); w32(0x00162470, R()); }
            view_on(VV(m + 0x10));
            return Args{{m}, {}};
        }, [](A a) { vf_cb_2f7880_l18(a.i[0]); return uint64_t{0}; }, 200);
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
