// Oracle batch 109 (task 0333): Aridia's own moby classes, part 2 (level 2): the classes transliterated from Aridia's
// overlay (tools/codegen/mkall.py, MKALL_LEVEL=2) on its real level data. Run with
// ORACLE_LEVEL=2 build/tests/ovl_oracle --batch 109 (needs the ISO). The subjects are the level's own mobys of each
// class (placed by its gameplay data) after the first gameplay frame, perturbed; the original is Aridia's own function
// at its own address, the port the one registered for it.
// Stage 4 P2.1 (hand ports of the Sand Shark, its hook +0x14, the Constructorbot, the elevator: aridia_sand_shark.cpp,
// aridia_constructorbot.cpp, aridia_elevator.cpp): "sand_shark_hi" / "constructorbot_hi" (their states past 11),
// "sand_shark_dying", "shark_hook_14" and the helpers' own checks ("shark_*", "constructorbot_hits"); "sand_shark_fix18"
// checks the shark's 1.02 fork (1.02 Fixes fix 18) when run with RAC1_FIXES102=18 (skipped otherwise).
// Stage 4 P2.2 (the rest of Aridia's own code, hand-ported: aridia_nest / _rocks / _rocket_launcher / _skid / _liftcar /
// _class1324 / _class1479 / _transport_pod.cpp): "transport_pod_hi" (states 12 .. 17), "nest_open", "launcher_aim" / "launcher_fire",
// "rocks_blast", "shark_egg_hatch", "shark_hook_10" and the helpers' own checks ("pod_*", "nest_*",
// "launcher_*", "skid_race_start").

#include "ovl_oracle.h"

#include <game/ovl/aridia_enemies.h>
#include <game/enhance/advanced.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/shared_bg_ships.h>
#include <game/ovl/shared_class604.h>
#include <game/ovl/shared_class615.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_swingshot_target.h>
#include <game/ovl/aridia_mobys.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sound_emitter.h>
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
    static const char* const kNames[] = {"constructorbot", "constructorbot_hi", "nest", "sand_shark", "sand_shark_dying",
                                         "sand_shark_hi", "sandmouse"};
    for (const char* n : kNames)
        if (std::strcmp(n, name) == 0) return true;
    return false;
}

constexpr uint32_t kStub = 0x01fe0000;  // the update-N-times stub (unused RAM below the stack)
// A check whose original is Aridia's own function at `l2addr` (not a Veldin address to translate).
void l2check(const char* name, uint32_t l2addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (g_oracle_level != 2) {
        std::printf("%-26s L2:0x%08x  skipped (Aridia only: run with ORACLE_LEVEL=2)\n", name, l2addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L2:0x%08x\n", name, l2addr);
    std::optional<CollisionScratchIgnored> spr_ignored;
    if (c2b_spr_check(name)) spr_ignored.emplace();
    if (!check_fn(name, l2addr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_109() {
    if (g_only_batch != 109) return;
    g_batch = 109;
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
    // the IOP server the scene start's library code binds (0x80000595): it answers every call with zeros
    sif_rpc_register(0x80000595, [](uint32_t, uint32_t, uint32_t, uint32_t recv, uint32_t rsize) {
        for (uint32_t i = 0; i + 4 <= rsize && i < 0x400; i += 4) ee_write<uint32_t>(recv + i, 0);
    });
    // the scene's sound ready (the dialog slot in state 3 with a handle: the start's wait loop not
    // entered)
    auto sound_ready = [] {
        w16(0x001516d0 + 0x5a, 3);
        w32(0x001516d0 + 0x50, (R() & 0x7ffffffeu) | 1u);
        w16(0x001516d0 + 0x5c, 0);
        w32(0x001516d0 + 0x1c, 0xffffffffu);
    };
    // the level after its first gameplay frame (the port's), the hero's equipment existing
    // (snapshots after 1, 40, 150 and 400 gameplay frames of the port's own engine: the classes have moved on from their start,
    // the original and the port are compared from each; `level(0)` is the first, `level()` one of them)
    static std::vector<std::vector<uint8_t>> snaps;
    auto level = [](int which = -1) {
        uint8_t* ram = ee_get_ram_base();
        if (snaps.empty()) {
            veldin_boot();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            // a stub the original runs a class update through N times (a0 = the moby, a1 = N, a2 = the function; our own loop,
            // assembled by hand, not the game's code): the state
            // machine moves on between the calls (written into every snapshot, so every `level()` has it)
            static const uint32_t stub[] = {0x27bdffd0, 0xafbf0010, 0xafb00014, 0xafb10018, 0xafb2001c, 0x00808021, 0x00a08821, 0x00c09021,
                                            0x02002021, 0x0240f809, 0x00000000, 0x2631ffff, 0x1e20fffb, 0x00000000, 0x8fb2001c, 0x8fb10018,
                                            0x8fb00014, 0x8fbf0010, 0x27bd0030, 0x03e00008, 0x00000000};
            int frame = 0;
            for (int target : {1, 40, 150, 400}) {
                while (frame < target) { gameplay_frame(); ++frame; }
                for (size_t i = 0; i < sizeof stub / sizeof stub[0]; ++i) ee_write<uint32_t>(kStub + 4 * static_cast<uint32_t>(i), stub[i]);
                snaps.emplace_back(ram, ram + 0x02000000);
            }
        }
        const size_t k = which >= 0 ? static_cast<size_t>(which) : static_cast<size_t>(ri(0, static_cast<int>(snaps.size()) - 1));
        std::memcpy(ram, snaps[k].data(), snaps[k].size());
    };
    // the level's mobys whose update function is `fn` (Aridia's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60, int frames = 4) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 2) { l2check(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        level(0);
        const std::vector<uint32_t> ms = mobys_with(fn);
        if (ms.empty()) { std::printf("%-26s L2:0x%08x  no moby on this level\n", name, fn); return; }
        l2check(name, kStub, n, Ret::None, [level, ms, vary, fn, frames] {
            level();
            uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            if (!(ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn)) { level(0); m = ms[0]; }  // (not alive in this snapshot)
            vary(m);
            return Args{{m, u(ri(1, frames)), fn}, {}};  // (the update, 1..frames times)
        }, [port](A a) { for (uint32_t i = 0; i < a.i[1]; ++i) port(a.i[0]); return uint64_t{0}; });
    };
    (void)classcheck;
    if (std::getenv("B109_INFO") != nullptr && g_oracle_level == 2) {  // the animation sets of each class' first moby
        level(0);
        std::printf("mobys %08x..%08x\n", ee_read<uint32_t>(OV(0x0015ffd8)), ee_read<uint32_t>(OV(0x0015ffe0)));
        std::map<int, int> seen;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
            const int c = ee_read<int16_t>(m + 0xa6);
            if (seen[c]++ != 0) continue;
            const uint32_t cls = ee_read<uint32_t>(m + 0x24);
            std::printf("  class %d moby %08x fn %08x anim %u/%u sets:", c, m, ee_read<uint32_t>(m + 0x74), ee_read<uint8_t>(m + 0x53), ee_read<uint8_t>(m + 0x52));
            for (uint32_t k = 0; k < 9; ++k) std::printf(" %08x", ee_read<uint32_t>(cls + 0x48 + 4 * k));
            std::printf("\n");
            if (ee_read<uint32_t>(cls + 0x2c) != 0) {  // the class hooks (class +0x2c table)
                std::printf("      hooks:");
                for (uint32_t k = 0; k < 8; ++k) std::printf(" %08x", ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + 4 * k));
                std::printf("\n");
            }
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


    // the debris spawns leave w lanes of a VU0 sin / cos in the original (the port has none)
    if (g_oracle_level == 2) {
        level(0);
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x80000; a += 0x40) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }  // (the w lanes of the particles' vectors, VU0 leftovers of the original: every 0x40)
    }
    // callees the ports leave TODO_PORT: no-ops on both sides
    g_extra_noop_hooks = {0x00122298, 0x00118a80, 0x00122fe0, 0x001232c8, OVF(0x00299460), OVF(0x0024d430), 
                          OVF(0x002a08c8), OVF(0x0029a6b0), OVF(0x00266040), OVF(0x00264740)};  // (task 0384: 0x260418 / 0x26e548 run on both sides: explosion_effect, the sparks transliterated)
    struct TrCls { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t skip_states = 0; int frames = 4; };  // (skip_states: the states that start a scene / a movie: the original runs the kernel path for real)
    static const TrCls trcls[] = {
        {"sand_shark", 0x002d3e50, sand_shark_update_l2}, {"constructorbot", 0x002d7748, constructorbot_update_l2}, {"nest", 0x002dcb38, nest_update_l2},
        {"transport_pod", 0x002ddc88, transport_pod_update_l2}, {"rocket_launcher", 0x002df3d8, rocket_launcher_update_l2}, {"skid", 0x002e1950, skid_update_l2, 0x2, 1},
        {"skid_agent", 0x002e0dc0, skid_agent_update_l2, 0x22, 1}, {"liftcar_platform", 0x002e2228, liftcar_platform_update_l2}, {"elevator", 0x002dbd38, elevator_update_l2},
        {"class1324", 0x002ee890, class1324_update_l2}, {"class1479", 0x002ef020, class1479_update_l2}, {"swingshot_target", 0x002e0450, swingshot_target_update},
        {"trespasser_pad", 0x002d8ad0, class615_update}, {"rocks", 0x002e0720, rocks_update_l2}, {"bg_ship1", 0x002ec4b8, bg_ship1_update},
        {"bg_ship2", 0x002ece18, bg_ship2_update},
        {"gold_bolt", 0x002ead38, gold_bolt_update, 0x6, 1}, {"sandmouse", 0x002f0f88, sandmouse_update}, {"class604", 0x002d7190, class604_update},
        {"class832", 0x002e4a48, class832_update}, {"class737", 0x002dfcd0, scripted_camera_trigger_update_l1},
    };
    // the hero inside the level box `b` (the boxes at *(0x1600ec), 0x80 each: a position in it at +0x30)
    auto hero_in_box = [](int32_t b) {
        if (b < 0 || b > 400) return;
        const uint32_t box = ee_read<uint32_t>(OV(0x001600ec)) + u(b << 7);
        Vec4 q = ee_ref<Vec4>(0x0013f3d0);
        for (uint32_t k = 0; k < 3; ++k) (&q.x)[k] = ee_read<float>(box + 0x30 + 4 * k) + rf(-0.3f, 0.3f);
        ee_ref<Vec4>(0x0013f3d0) = q;
    };
    // class-specific preparation (what the class's own code looks at), by update function
    static std::map<void (*)(uint32_t), std::function<void(uint32_t)>> extras;
    extras.clear();
    extras[sand_shark_update_l2] = [hero_in_box](uint32_t m) {  // its patrol / lair boxes (pvar +0x270 / +0x274) and the hero in one
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (coin()) hero_in_box(ee_read<int32_t>(pv + (coin() ? 0x274u : 0x270u)));
        // (P2.1) on screen (+0x31: its floor band near the camera, its sand), the camera near; no target (+0x214 2) or one, its
        // box +0x274 off, its heading's wobble +0x284, the frame counter's low bits
        if (coin()) { w8(m + 0x31, 1); if (coin()) { Vec4 c = ee_ref<Vec4>(m + 0x10); c.x += rf(-20, 20); c.y += rf(-20, 20); c.z += rf(0, 10); ee_ref<Vec4>(OV(0x00166dc0)) = c; } }
        if (ri(0, 2) == 0) w32(pv + 0x214, coin() ? 2u : 0u);
        if (ri(0, 3) == 0) w32(pv + 0x274, 0xffffffffu);
        if (ri(0, 3) == 0) w32(pv + 0x284, u(ri(0, 3)));
    };
    extras[class1324_update_l2] = [hero_in_box](uint32_t m) {  // a zone (box pvar +0x5c) the hero is in, the hero's items and flags
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (ri(0, 3) != 0) hero_in_box(ee_read<int32_t>(pv + 0x5c));
        for (uint32_t i = 0; i < 37; ++i) { w32(0x0017a0c0 + 8 + 0x4c * i, ri(0, 3) == 0 ? 1u : 0u); w8(0x0013d4c0 + i, ri(0, 2) == 0 ? 0u : u(ri(1, 3))); }
        w32(0x001413dc, u(ri(0, 3))); if (ri(0, 3) != 0) { w16(0x00141be0, 0); w16(0x001418e8, 0); }
        // (P2.2) the hero in one of its other boxes, its mode, the hints' records, the hero's state, story and switch bytes
        static const uint32_t boxes[] = {0x10, 0x14, 0x18, 0x1c, 0x40, 0x44, 0x48, 0x4c};
        if (ri(0, 3) != 0) hero_in_box(ee_read<int32_t>(pv + boxes[ri(0, 7)]));
        w32(pv + 0x54, u(ri(0, 2)));
        if (coin()) w32(pv + 0x58, u(ri(0, 1)));
        if (coin()) w32(pv + 0x30, u(ri(0, 3)));
        if (coin()) w32(pv + 0x50, coin() ? 0u : u(ri(500, 700)));
        const uint32_t rec = 0x00141968;
        for (const uint32_t o : {0x60u, 0x68u, 0x80u, 0x8au, 0x90u, 0x92u, 0x230u, 0x238u, 0x3b0u}) if (coin()) w16(rec + o, ri(0, 2) == 0 ? 0u : u(ri(1, 40)));
        if (coin()) { w32(rec + 0x264, R()); w32(rec + 0x3b4, R()); }
        static const uint32_t states[] = {0x2c, 0x72, 0x1d, 0};
        w32(0x001413d4, states[ri(0, 3)]); w32(0x001413e0, coin() ? 0x2cu : 0u); w32(0x0013f4e8, u(ri(0, 1)));
        w32(0x0013f650, ri(0, 3) == 0 ? 1u : 0u);
        for (const uint32_t b : {0x0013d409u, 0x0013d4ccu, 0x0013d4d0u, 0x0013d39cu, 0x0015eddcu, 0x0015ede0u}) w8(b, coin() ? 1u : 0u);
        w16(0x00141888, u(ri(0, 4)));
        w8(0x0014c050u + ee_read<uint32_t>(pv) + (ee_read<uint32_t>(0x0015ed84) << 4), coin() ? 0xffu : 0u);
        w32(0x0014ee90u + (ee_read<uint32_t>(pv) << 2), u(ri(0, 8)));
        w32(0x0015eea4, u(ri(0, 200000)));
        if (coin()) {  // the moby the hero is at: one of class 0x267
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), n = (ee_read<uint32_t>(OV(0x0015ffe0)) - first) >> 8;
            const uint32_t o = first + u(ri(0, static_cast<int>(n) - 1)) * 0x100;
            if (o != m) { w16(o + 0xa6, 0x267); w32(0x0013f64c, o); }
        }
    };
    extras[class1479_update_l2] = [](uint32_t m) {  // (P2.2) the vent where the hero is (on screen: the camera follows him), its timers
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (coin()) { Vec4 q = ee_ref<Vec4>(0x0013f3d0); q.z += 1.0f; ee_ref<Vec4>(m + 0x10) = q; }
        if (coin()) w32(pv + 0xc0, u(ri(0, 1)));
        if (coin()) w32(pv + 0xc4, u(ri(0, 1)));
        if (coin()) w32(pv + 0xc8, u(ri(0, 40)));
    };
    extras[class832_update] = [hero_in_box](uint32_t m) {  // the hero in state 0x1d, a class-0xac moby in the list at 0x15ffe4, zone records
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (ri(0, 3) != 0) w32(0x001413d4, 0x1d);
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), n = (ee_read<uint32_t>(OV(0x0015ffe0)) - first) >> 8;
        uint32_t x = first + u(ri(0, static_cast<int>(n) - 1)) * 0x100;
        if (x == m) x = first;
        if (ri(0, 3) != 0) { w16(x + 0xa6, 0xac); w32(x + 0x28, 0); w32(OV(0x0015ffe4), x); }
        for (uint32_t i = 0; i < 20; ++i) {
            const uint32_t rec = pv + 0x10 * i;
            w32(rec, ri(0, 4) != 0 ? 0xffffffffu : u(ri(0, 1)));  // (-1: no polygon test)
            wf(rec + 4, rf(-50, 50)); wf(rec + 8, rf(-20, 100)); w32(rec + 0xc, u(ri(0, 6)));
        }
        w32(pv + 0x140, u(ri(0, 80)));  // (below the frame count that retires the class-0xac moby: a made-up moby that the retire would free by its real class)
        const float zz = rf(-30, 90); wf(x + 0x18, zz);
        hero_in_box(ri(0, 6));
    };
    extras[swingshot_target_update] = [](uint32_t m) {  // the moby indexes it follows (pvar +0x30 / +0x3c: -1 = none) and their states
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), n = (ee_read<uint32_t>(OV(0x0015ffe0)) - first) >> 8;
        for (uint32_t o : {0x30u, 0x3cu}) {
            if (ri(0, 2) == 0) { w32(pv + o, 0xffffffffu); continue; }
            const uint32_t idx = u(ri(0, static_cast<int>(n) - 1));
            w32(pv + o, idx);
            if (ri(0, 2) == 0) w8(first + (idx << 8) + 0x20, coin() ? 0xfeu : 0xfdu);
        }
    };
    extras[elevator_update_l2] = [](uint32_t m) {  // (P2.1) its call flag, the hero level with it and in its cylinder, its goal and speed
        const uint32_t pv = ee_read<uint32_t>(m + 0x78), P = 0x0013f350;
        if (pv == 0) return;
        w8(0x0013d4da, ri(0, 3) == 0 ? 1u : 0u);
        if (coin()) wf(P + 0x88, ee_read<float>(m + 0x18) + (coin() ? rf(1.5f, 2.5f) : rf(-4.0f, 4.0f)));
        if (coin()) { Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-0.5f, 0.5f); q.y += rf(-0.5f, 0.5f); q.z += rf(0.0f, 3.0f); ee_ref<Vec4>(P + 0xd0) = q; }
        if (ri(0, 2) == 0) wf(pv + 0xa8, ee_read<float>(m + 0x18) + (coin() ? 0.0f : rf(-3.0f, 3.0f)));
        if (ri(0, 2) == 0) wf(pv + 0xb0, coin() ? 0.0f : rf(-0.3f, 0.3f));
        if (ri(0, 2) == 0) w32(pv + 0xb4, coin() ? 0xffffffffu : u(ri(0, 15)));
        if (ri(0, 3) == 0) w8(m + 0xbc, u(ri(0, 1)));
        if (ri(0, 3) == 0) {  // the hero about 20 away (the distance it waits for him at)
            const float a = rf(-3.1f, 3.1f), r = rf(19.0f, 21.0f);
            wf(P + 0x80, ee_read<float>(m + 0x10) + r * std::cos(a)); wf(P + 0x84, ee_read<float>(m + 0x14) + r * std::sin(a));
        }
        if (ri(0, 3) == 0) {  // the hero riding it (in its cylinder, 2 below its top), the elevator at or above its goal
            Vec4 q = ee_ref<Vec4>(m + 0x10); q.z -= 2.0f + rf(-0.9f, 0.9f); ee_ref<Vec4>(P + 0xd0) = q; wf(P + 0x88, q.z);
            wf(pv + 0xa8, ee_read<float>(m + 0x18) - (coin() ? 0.0f : rf(0.0f, 2.0f)));
            if (coin()) wfrand(pv + 0x70, 3, -0.5f, 0.5f);  // (its attachment record's velocity, cleared when it arrives)
        }
    };
    extras[skid_agent_update_l2] = [](uint32_t m) {  // (P2.2) the talk's state +4 and the frame's mode (state 2: the checkpoint)
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w16(pv + 4, u(ri(0, 1)));  // (not 2, the deal: its movie start runs the kernel's wait in the interpreter)
        w32(L2_ADDR(0x0015f5c4), ri(0, 2) == 0 ? 2u : u(ri(0, 1)));
        if (coin()) w32(pv + 0x150, coin() ? 0xffffffffu : u(ri(0, 5)));
    };
    extras[transport_pod_update_l2] = [](uint32_t m) {  // (P2.2) the hero at a station or in it, the doors near their marks, the flags
        const uint32_t pv = ee_read<uint32_t>(m + 0x78), P = 0x0013f350;
        if (pv == 0) return;
        const int r = ri(0, 3);
        if (r < 2) {  // the hero near a station (+0x60 / +0x64)
            const int32_t b = ee_read<int32_t>(pv + (r == 0 ? 0x60u : 0x64u));
            if (b >= 0 && b < 400) { Vec4 q = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(b << 7) + 0x30); q.x += rf(-20, 20); q.y += rf(-20, 20); ee_ref<Vec4>(P + 0x80) = q; }
        }
        if (ri(0, 2) == 0) { w32(P + 0x2fc, m); w16(P + 0x30e, ri(0, 3) == 0 ? 1u : 0u); }
        w8(P + 0x20a4, ri(0, 4) == 0 ? 1u : 0u);
        w8(m + 0xbc, u(ri(0, 2)));
        wf(L2_ADDR(0x00161c40), coin() ? 1.0f : 0.0f);
        wf(L2_ADDR(0x00161c44), ri(0, 4) == 0 ? 1.0f : 0.0f);
        for (uint32_t o : {0x84u, 0x88u}) {  // (the doors near their marks)
            const uint32_t d = ee_read<uint32_t>(pv + o);
            if (d >= 0x00100000u && d < 0x01fff000u && coin()) {
                static const float marks[] = {2.0f, -4.0f, -1.0f};
                wf(d + 0x18, ee_read<float>(m + 0x18) + marks[ri(0, 2)] + (coin() ? 0.0f : rf(-0.02f, 0.02f)));
            }
        }
        if (coin()) { wf(pv + 0xa4, rf(0.0f, 8.0f)); w32(pv + 0xa0, u(ri(0, 8))); }
    };
    extras[constructorbot_update_l2] = [](uint32_t m) {  // (P2.1) at its work place, its target near it and level, facing it or nearly
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (ri(0, 2) == 0) wf(pv + 0x48, ee_read<float>(0x0015ed6c) * 40.0f + rf(-1.0f, 0.2f));  // (its rise speed about its top)
        if (coin()) { w8(m + 0x31, 1); if (coin()) { Vec4 c = ee_ref<Vec4>(m + 0x10); c.x += rf(-20, 20); c.y += rf(-20, 20); c.z += rf(0, 10); ee_ref<Vec4>(OV(0x00166dc0)) = c; } }
        const int32_t b = ee_read<int32_t>(pv + 0x188);
        if (b < 0 || b > 400) return;
        const Vec4 place = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(b << 7) + 0x30);
        if (coin()) { Vec4 q = place; q.x += rf(-1.2f, 1.2f); q.y += rf(-1.2f, 1.2f); ee_ref<Vec4>(m + 0x10) = q; }
        const uint32_t t = ee_read<uint32_t>(pv + 0x220);
        if (t >= 0x00100000u && t < 0x01fff000u && coin()) {
            Vec4 q = place; const float a = rf(-3.1f, 3.1f), r = rf(0.0f, ee_read<float>(pv + 0x180) * 1.2f);
            q.x += r * std::cos(a); q.y += r * std::sin(a); q.z = ee_read<float>(m + 0x18) + rf(-5.0f, 5.0f);
            ee_ref<Vec4>(t + 0x10) = q;
            const float yaw = std::atan2(q.y - ee_read<float>(m + 0x14), q.x - ee_read<float>(m + 0x10));
            if (coin()) wf(m + 0x48, yaw + rf(-0.6f, 0.6f));
        }
    };
    extras[liftcar_platform_update_l2] = [hero_in_box](uint32_t m) {  // its zone and the hero in / out of it
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        for (uint32_t o = 0; o < 0x100; o += 4) { const int32_t v = ee_read<int32_t>(pv + o); if (v >= 0 && v < 8 && coin()) { hero_in_box(v); break; } }
        if (coin()) {  // (P2.2) the hero about 16 away (the stations' call reach), 5 or more below or above it
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            const float a = rf(-3.1f, 3.1f), r = rf(14.5f, 16.5f);
            q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += coin() ? rf(4.0f, 7.0f) : -rf(4.0f, 7.0f);
            ee_ref<Vec4>(0x0013f3d0) = q;
        }
        if (ri(0, 3) == 0) w8(m + 0xbc, u(ri(0, 1)));
        const uint32_t car = ee_read<uint32_t>(OV(0x0015ffd8)) + (ee_read<uint32_t>(pv + 0xa0) << 8);  // (its car: played out, stopped)
        if (car >= 0x00100000u && car < 0x01fff000u) {
            if (coin()) w8(car + 0x70, ee_read<uint8_t>(car + 0x70) | 2u);
            if (coin()) wf(car + 0x58, coin() ? 0.0f : 1.0f);
        }
        if (ri(0, 3) == 0) {  // the hero under it (within 1.5, more than 1.5 below) or just off it
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            q.x += rf(-1.0f, 1.0f); q.y += rf(-1.0f, 1.0f); q.z -= coin() ? rf(1.6f, 4.0f) : rf(0.0f, 1.4f);
            ee_ref<Vec4>(0x0013f3d0) = q;
        }
        if (coin()) w32(pv + 0xa4, coin() ? 0xffffffffu : u(ri(0, 15)));
    };
    auto generic_prep = [give_hit, hero_near, sound_ready](uint32_t m, int st, int pass, void (*fn)(uint32_t)) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t P = 0x0013f350;
        if (st != 0) {
            w8(m + 0x20, 0);
            fn(m);
        }
        w8(m + 0x20, u(st));
        if (pass >= 1) {  // 1: hits and a typed mutation of the pvars; 2: a heavier mutation, no hit; 3: the hero close, hits
            {  // an animation set the class has (the set table at class +0x48: its pointers, then other data of the class)
                const uint32_t sets = ee_read<uint32_t>(m + 0x24) + 0x48;
                std::vector<uint32_t> have;
                for (uint32_t k = 0; k < 9; ++k) { const uint32_t v = ee_read<uint32_t>(sets + 4 * k); if (v < 0x00100000u || v >= 0x02000000u) break; have.push_back(k); }  // (the table's pointers, then other data)
                if (!have.empty()) {
                    w8(m + 0x53, have[static_cast<size_t>(ri(0, static_cast<int>(have.size()) - 1))]);
                    w8(m + 0x52, coin() ? ee_read<uint32_t>(m + 0x53) : have[static_cast<size_t>(ri(0, static_cast<int>(have.size()) - 1))]);
                }
                w8(m + 0x50, 0); w8(m + 0x51, 0);  // (the frames: the first)
                w8(m + 0xbc, u(ri(0, 9)));  // (the class's own mode byte: its states use values to 7)
                w8(m + 0x70, u(R() & 7));  // (the animation flags: bit 1 = played out)
            }
            wf(m + 0x48, rf(-3.1f, 3.1f));
            { Vec4 q = ee_ref<Vec4>(m + 0x10); const float r = pass == 3 ? rf(0, 3) : (ri(0, 3) == 0 ? rf(0, 6) : (coin() ? rf(6, 30) : rf(30, 80))); const float a = rf(-3.1f, 3.1f);
              q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-4, 12); ee_ref<Vec4>(P + 0x80) = q; }
            for (uint32_t o = 0x20; o < 0x200; o += 4) {  // a few of the pvars kept as they are typed: floats scaled, small counters moved
                if (ri(0, pass == 2 ? 2 : 9) != 0) continue;
                const uint32_t v = ee_read<uint32_t>(pv + o), ex = v & 0x7f800000u;
                if (ex >= 0x30000000u && ex < 0x50000000u) wf(pv + o, ee_read<float>(pv + o) * rf(0.5f, 1.5f));
                else if (v >= 1 && v < 16) w32(pv + o, u(ri(0, static_cast<int>(v) + 2)));  // (not a zero: a float field made a denormal, which the host floats of the ports do not flush)
            }
            if (pass != 2) give_hit(m, ri(0, 1) ? 0x210000u : 0x330000u); else w8(m + 0xa4, 0xff);
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
        if (pass >= 1) if (const auto it = extras.find(fn); it != extras.end()) it->second(m);
        if (ri(0, 2) == 0) { w32(P + 0x2fc, m); w16(P + 0x30e, u(ri(0, 2))); }  // (the moby the hero interacts with, 0x13f64c, is this one)
        if (ri(0, 2) == 0) {  // the hero's gadget moby (the Trespasser, class 1005) in use: P + 0x1090, its id P + 0x10b8 (0x1a)
            for (uint32_t g = ee_read<uint32_t>(OV(0x0015ffd8)); g < ee_read<uint32_t>(OV(0x0015ffe0)); g += 0x100)
                if (ee_read<int16_t>(g + 0xa6) == 1005) {
                    w32(P + 0x1090, g); w32(P + 0x10b8, coin() ? 0x1au : u(ri(0, 30))); w8(g + 0xbc, u(ri(0, 2)));
                    break;
                }
        }
        w32(OV(0x0015f5cc), R() & 0xffff);
        if (coin()) sound_ready();  // (a scene's / dialog's sound ready: the start's wait loop not entered)
        seed_rand();
    };
    // the carry update (Aridia's 0x2e7660: Veldin's carry_update 0x2dc9f0 runs there, the same code word for word with the
    // relocations through the maps) as the Sand Shark calls it: (moby, pvar + 0x120)
    if (g_oracle_level == 2 && std::getenv("B109_CARRY") != nullptr) {
        level(0);
        const std::vector<uint32_t> ms = mobys_with(0x002d3e50);
        if (!ms.empty()) l2check("carry_update", 0x002e7660, 60, Ret::Int32, [level, ms, generic_prep] {
            level();
            const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            generic_prep(m, 0, ri(0, 3), sand_shark_update_l2);
            return Args{{m, ee_read<uint32_t>(m + 0x78) + 0x120}, {}};
        }, [](A a) { return ri32(carry_update(a.i[0], a.i[1])); });
    }
    // the particle spawn 0x0c (Veldin 0x26b230, the same code at L2:0x26c8a8) as the Constructorbot calls it
    if (g_oracle_level == 2 && std::getenv("B109_PART") != nullptr) {
        level(0);
        const std::vector<uint32_t> ms = mobys_with(0x002d7748);
        if (!ms.empty()) l2check("spawn_particle_0c", 0x0026c8a8, 60, Ret::Int32, [level, ms] {
            level();
            const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            return Args{{m + 0x10, m + 0x40, u(ri(0, 7))}, {rf(0.1f, 2)}};
        }, [](A a) { return ri32(static_cast<int32_t>(spawn_particle_0c(a.i[0], a.i[1], a.i[2], a.f[0]))); });
    }
    // the joint node spring (Veldin 0x2628d8, the same code at L2:0x2641f0) as the skid / agent call it: node = pvar + 0x40 / 0xc0
    if (g_oracle_level == 2 && std::getenv("B109_JNS") != nullptr) {
        level(0);
        const std::vector<uint32_t> ms = mobys_with(0x002e1950);
        if (!ms.empty()) l2check("joint_node_spring", 0x002641f0, 40, Ret::None, [level, ms, generic_prep] {
            level();
            const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            generic_prep(m, 0, 1, skid_update_l2);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            return Args{{m, pv + (coin() ? 0x40u : 0xc0u), u(ri(0, 1))}, {rf(0, 2), rf(0, 2)}};
        }, [](A a) { joint_node_spring(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1]); return uint64_t{0}; });
    }
    // the camera type 0x12's accept function (Aridia's own: the type table's row at L2:0x1fb778): a camera and the current one
    if (g_oracle_level == 2) {
        l2check("cam_type_12_accept", 0x002fc298, 60, Ret::Int32, [level] {
            level();
            const uint32_t cams = OV(0x00167150);
            auto valid = [](uint32_t c) {  // a camera whose level camera entry (index +0x84, 0x20 each at *(0x15ef50)) has its pointer +0x1c
                const uint32_t e = ee_read<uint32_t>(OV(0x0015ef50)) + u(ee_read<int16_t>(c + 0x84)) * 0x20;
                const uint32_t p = e < 0x02000000u - 0x20 ? ee_read<uint32_t>(e + 0x1c) : 0u;
                if (!(p >= 0x00100000u && p < 0x01fffff0u)) return false;
                const int32_t t = ee_read<int32_t>(p + 0x44);  // the record's table index (the table at 0x1abe40: pointers or zero)
                if (t < 0) return true;
                if (t >= 0x400) return false;
                const uint32_t q = ee_read<uint32_t>(0x001abe40 + 4 * u(t));
                return q == 0 || (q >= 0x00100000u && q < 0x01fffff0u);
            };
            std::vector<uint32_t> good;
            for (uint32_t k = 0; k < 48; ++k) if (valid(cams + k * 0xa0)) good.push_back(cams + k * 0xa0);
            if (good.empty()) good.push_back(cams);
            const uint32_t cam = good[static_cast<size_t>(ri(0, static_cast<int>(good.size()) - 1))], cur = cams + u(ri(0, 47)) * 0xa0;
            w32(0x00167400, cur);  // (the current camera pointer the function reads: lw 0x7400(0x160000))
            if (ri(0, 2) == 0) w16(cur + 0x86, u(ri(0, 1) ? 0 : ri(0, 30)));
            return Args{{cam, cur}, {}};
        }, [](A a) { return ri32(static_cast<int32_t>(cam_type18_accept(a.i[0], a.i[1]))); });
        // the sound emitters of Aridia's own types (the update function at emitter +4)
        for (uint32_t fn : {0x002fc6f0u, 0x002fc850u}) {
            level();
            std::vector<uint32_t> ems;
            for (int32_t i = 0; i < ee_read<int32_t>(0x0013f2e0); ++i) {
                const uint32_t em = ee_read<uint32_t>(0x0013f2e4) + u(i) * 0x90;
                if (ee_read<uint32_t>(em + 4) == fn) ems.push_back(em);
            }
            if (ems.empty()) { std::printf("%-26s L2:0x%08x  no emitter of this type on this level\n", "sound_emitter", fn); continue; }
            l2check("sound_emitter", fn, 60, Ret::None, [level, ems, hero_near] {
                level(0);
                const uint32_t em = ems[static_cast<size_t>(ri(0, static_cast<int>(ems.size()) - 1))];
                hero_near(em);
                { Vec4 q = ee_ref<Vec4>(0x0013f3d0); if (coin()) { q.x += rf(-3, 3); q.y += rf(-3, 3); } ee_ref<Vec4>(0x0013f3d0) = q; }
                if (ri(0, 2) == 0) wfrand(em + 0x10, 12, -2, 2);
                seed_rand();
                return Args{{em}, {}};
            }, [fn](A a) { if (fn == 0x002fc6f0u) emitter_reverb_box_update(a.i[0]); else emitter_music_box_update(a.i[0]); return uint64_t{0}; });
        }
    }
    // the camera hook Aridia's follow camera calls each frame (*(0x167010) = 0x2fa710)
    if (g_oracle_level == 2) {
        l2check("cam_mode_hook", 0x002fa710, 40, Ret::None, [level] {
            level();
            if (ri(0, 3) != 0) wf(0x0016760c, rf(-3, 3));
            if (ri(0, 4) == 0) wf(0x0016760c, 0.0f);
            const uint32_t cams = OV(0x00167150);
            const uint32_t cam = cams + u(ri(0, 3)) * 0xa0;
            w32(OV(0x00166e00), cam);  // (the active camera)
            return Args{{0}, {}};
        }, [](A a) { cam0_level_hook(a.i[0]); return uint64_t{0}; });
    }
    for (const TrCls& c : trcls) {
        if (std::getenv("B109_CLS") != nullptr && std::strcmp(std::getenv("B109_CLS"), c.name) != 0) continue;
        for (int st = 0; st < 12; ++st)
        for (int pass = 0; pass < 4; ++pass)
        if ((c.skip_states & (1u << st)) == 0) classcheck(c.name, c.addr, c.fn, [st, pass, c, generic_prep](uint32_t m) { generic_prep(m, st, pass, c.fn); }, 20, c.frames);
    }
    // ---- stage 4 P2.1: the Sand Shark (its states past 11, its dying, its hook +0x14, its helpers, its 1.02 fork: fix 18),
    // the Constructorbot (its states past 11, its hits) and their helpers, each against Aridia's own function
    if (g_oracle_level == 2 && std::getenv("B109_CLS") == nullptr) {
        level(0);
        const std::vector<uint32_t> sharks = mobys_with(0x002d3e50), bots = mobys_with(0x002d7748);
        const uint32_t P = 0x0013f350;
        // a moby of the list alive in this snapshot (or the first one of the first snapshot)
        auto pick_of = [level](const std::vector<uint32_t>& v, uint32_t fn) {
            uint32_t m = v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))];
            if (!(ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn)) { level(0); m = v[0]; }
            return m;
        };
        // the frame scale, the frame time and its square at their 50 Hz values in a quarter of the cases
        auto frame50 = [] { if (ri(0, 3) == 0) { wf(0x0015ed68, 1.2f); wf(0x0015ed6c, 1.2f); wf(0x0015ed70, 1.44f); } };
        // the shark's group (+0x21): none (0xff), the level's, or a list of the test area in a row of the group table the
        // level leaves empty: some of the sharks (class 580, whose alert the group's alert raises), lairs (class 0x29c:
        // other mobys of the level given that class, up (below 0x7f) or not), other mobys, the subject now and then
        auto shark_group = [sharks](uint32_t m) {
            const int r = ri(0, 3);
            if (r == 0) { w8(m + 0x21, 0xff); return; }
            if (r == 1) return;
            int32_t row = -1;
            for (int32_t i = 0x6f; i >= 0; --i) if (ee_read<uint32_t>(OV(0x001ab840) + u(i) * 4) == 0) { row = i; break; }
            if (row < 0) return;
            const uint32_t base = ee_read<uint32_t>(OV(0x0015ffd8)), n = (ee_read<uint32_t>(OV(0x0015ffe0)) - base) >> 8;
            std::vector<uint32_t> mem;
            for (uint32_t s : sharks) if (s != m && coin()) mem.push_back(s);
            for (int k = ri(0, 4); k > 0; --k) {
                const uint32_t o = base + u(ri(0, static_cast<int>(n) - 1)) * 0x100;
                if (o == m || ee_read<int16_t>(o + 0xa6) == 0x244) continue;
                if (ri(0, 3) != 0) { w16(o + 0xa6, 0x29c); w8(o + 0x20, ri(0, 3) != 0 ? u(ri(0, 0x7e)) : u(ri(0x7f, 0xfd))); }
                mem.push_back(o);
            }
            if (ri(0, 3) == 0) mem.push_back(m);
            if (mem.empty()) mem.push_back(m);
            const uint32_t list = T + 0x800;
            for (size_t i = 0; i < mem.size(); ++i) w16(list + 2 * u(static_cast<int32_t>(i)), ((mem[i] - base) >> 8) | (i + 1 == mem.size() ? 0x8000u : 0u));
            w32(OV(0x001ab840) + u(row) * 4, list);
            w8(m + 0x21, u(row));
        };
        // the shark knocked out by a hit (the port's frames until it is, then 0 .. 90 more of its dying throw), its animation
        // played out in two thirds of the cases (the stub runs the update alone: the engine's animation step does not run)
        auto shark_dying = [generic_prep, give_hit, shark_group, frame50](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            generic_prep(m, 5, 0, sand_shark_update_l2);
            for (int k = 0; k < 8 && ee_read<uint8_t>(m + 0x20) != 0x18 && ee_read<uint8_t>(m + 0x20) < 0xfe; ++k) {
                w8(m + 0x20, coin() ? 5u : 2u);
                wf(pv + 0x20, 0.01f);
                give_hit(m, 0x330000u);
                if (ee_read<uint8_t>(m + 0xa4) == 0xff) continue;
                const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                w32(e + 0x24, 0x330000u | (R() & 0xffu));
                wf(e + 0x2c, rf(1.0f, 6.0f));
                sand_shark_update_l2(m);
            }
            for (int k = ri(0, 90); k > 0 && ee_read<uint8_t>(m + 0x20) == 0x18; --k) sand_shark_update_l2(m);
            if (ri(0, 2) == 0) w32(pv + 0x29c, 0);
            if (ri(0, 3) != 0) w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u);
            if (coin()) give_hit(m, 0x330000u);  // (hit while it dies: not counted)  // (its death animation played out: the events 0x40)
            shark_group(m);
            frame50();
            seed_rand();
        };
        if (!sharks.empty()) {
            // the states 12 .. 29 (the class loop above runs 0 .. 11)
            for (int st = 12; st < 30; ++st)
                for (int pass = 0; pass < 4; ++pass)
                    classcheck("sand_shark_hi", 0x002d3e50, sand_shark_update_l2, [st, pass, generic_prep, shark_group, frame50](uint32_t m) {
                        generic_prep(m, st, pass, sand_shark_update_l2);
                        if ((st == 13 || st == 27) && coin()) {  // a leap about to end: about its home's height, falling or not
                            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                            wf(m + 0x18, ee_read<float>(pv + 0x228) + rf(-1.5f, 3.5f));
                            wfrand(pv + 0x40, 3, -0.3f, 0.3f);
                        }
                        shark_group(m);
                        frame50();
                        seed_rand();
                    }, 10);
            // dying: its landing, its bolts, back at a lair or gone
            classcheck("sand_shark_dying", 0x002d3e50, sand_shark_update_l2, shark_dying, 60);
            // the hook +0x14: back at a group mate's lair or gone
            l2check("shark_hook_14", 0x002d6340, 60, Ret::None, [level, sharks, pick_of, shark_group, frame50] {
                level();
                const uint32_t m = pick_of(sharks, 0x002d3e50);
                shark_group(m);
                frame50();
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { shark_hook_14_l2(a.i[0]); return uint64_t{0}; });
            // the helpers on their own: the group's lair pick and alert, the frame's common part, the polygon test, the sand
            l2check("shark_group_mate", 0x002d5658, 60, Ret::Int32, [level, sharks, pick_of, shark_group] {
                level();
                const uint32_t m = pick_of(sharks, 0x002d3e50);
                shark_group(m);
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { return r32(shark_group_mate_l2(a.i[0])); });
            l2check("shark_group_alert", 0x002d5728, 60, Ret::None, [level, sharks, pick_of, shark_group, frame50] {
                level();
                const uint32_t m = pick_of(sharks, 0x002d3e50);
                shark_group(m);
                for (uint32_t s : sharks) w16(ee_read<uint32_t>(s + 0x78) + 0x25e, ri(0, 2) == 0 ? 0u : u(ri(0, 400)));
                frame50();
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { shark_group_alert_l2(a.i[0]); return uint64_t{0}; });
            l2check("shark_frame", 0x002d5c88, 80, Ret::None, [level, sharks, pick_of, generic_prep, shark_group, frame50, P] {
                level();
                const uint32_t m = pick_of(sharks, 0x002d3e50);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                generic_prep(m, 0, ri(0, 3), sand_shark_update_l2);
                static const uint8_t states[] = {0, 2, 5, 6, 7, 0xe, 0x12, 0x16, 0x18, 0x1a, 0x1d};
                w8(m + 0x20, states[ri(0, 10)]);
                if (ri(0, 3) == 0) w8(pv + 0x2e, 2);
                if (ri(0, 2) == 0) {  // its lair: none or another shark's slot (pvars with a count +0x14c)
                    const uint32_t base = ee_read<uint32_t>(OV(0x0015ffd8));
                    w32(pv + 0x288, coin() ? 0xffffffffu : (sharks[static_cast<size_t>(ri(0, static_cast<int>(sharks.size()) - 1))] - base) >> 8);
                }
                if (ri(0, 2) == 0) w32(pv + 0x38, coin() ? 1u : 0u);
                if (ri(0, 2) == 0) w16(pv + 0x25c, u(ri(0, 2)));
                if (ri(0, 2) == 0) w16(pv + 0x25e, u(ri(0, 2)));
                if (ri(0, 2) == 0) wf(pv + 0x20, coin() ? 0.01f : rf(0.5f, 8.0f));
                if (ri(0, 3) == 0) w32(pv + 0x210, 0);
                if (ri(0, 3) == 0) { Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-8, 8); q.y += rf(-8, 8); ee_ref<Vec4>(P + 0x80) = q; }
                shark_group(m);
                frame50();
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { shark_frame_l2(a.i[0]); return uint64_t{0}; });
            l2check("shark_path_check", 0x002d63c0, 60, Ret::Int32, [level, sharks, pick_of] {
                level();
                const uint32_t m = pick_of(sharks, 0x002d3e50);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                const uint32_t path = ee_read<uint32_t>(OV(0x001b04b0) + ee_read<uint32_t>(pv + 0x290) * 4);
                const int32_t np = path >= 0x00100000u && path < 0x01fffff0u ? ee_read<int32_t>(path) : 0;
                if (np > 0 && np < 0x1000 && coin()) {  // near one of its path's points, inside or outside
                    Vec4 q = ee_ref<Vec4>(path + 0x10 + u(ri(0, np - 1)) * 16);
                    q.x += rf(-3, 3); q.y += rf(-3, 3);
                    ee_ref<Vec4>(m + 0x10) = q;
                }
                w8(m + 0x53, u(ri(0, 1)));
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { return r32(shark_path_check_l2(a.i[0])); });
            // the sand: a line of grains, an edge, the outline, the spray and the puff (fewer while the particles are many)
            auto at_shark = [level, sharks, pick_of](uint32_t out) {
                level();
                const uint32_t m = pick_of(sharks, 0x002d3e50);
                Vec4 q = ee_ref<Vec4>(m + 0x10);
                q.x += rf(-2, 2); q.y += rf(-2, 2); q.z += rf(-1, 1); q.w = rf(-1, 1);
                ee_ref<Vec4>(out) = q;
                return m;
            };
            auto live_particles = [](int32_t near) {
                const int r = ri(0, 3);
                w32(0x00160178, r == 0 ? u(ri(0, 1000)) : r == 1 ? u(near + ri(-2, 1)) : u(ri(near - 600, near + 300)));
            };
            l2check("shark_sand_line", 0x002d3a50, 60, Ret::None, [at_shark, frame50] {
                at_shark(T);
                at_shark(T + 0x10);
                frame50();
                seed_rand();
                return Args{{0, T, T + 0x10, u(ri(-1, 8))}, {rf(0.0f, 0.02f), rf(0.0f, 0.02f)}};
            }, [](A a) { shark_sand_line_l2(a.i[1], a.i[2], static_cast<int32_t>(a.i[3]), a.f[0], a.f[1]); return uint64_t{0}; });
            l2check("shark_sand_edge", 0x002d3c98, 40, Ret::None, [at_shark] {
                const uint32_t m = at_shark(T);
                seed_rand();
                return Args{{m, u(ri(0, 9)), u(ri(0, 9)), u(ri(0, 5))}, {}};
            }, [](A a) { shark_sand_edge_l2(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3])); return uint64_t{0}; });
            l2check("shark_sand_burst", 0x002d3dd0, 30, Ret::None, [at_shark, frame50] {
                const uint32_t m = at_shark(T);
                frame50();
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { shark_sand_burst_l2(a.i[0]); return uint64_t{0}; });
            l2check("shark_sand_spray", 0x002d57f8, 80, Ret::None, [at_shark, frame50, live_particles] {
                at_shark(T);
                live_particles(coin() ? 1025 : 1501);
                frame50();
                seed_rand();
                return Args{{T, u(ri(-1, 0x1b))}, {}};
            }, [](A a) { shark_sand_spray_l2(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
            l2check("shark_sand_puff", 0x002d5a68, 60, Ret::None, [at_shark, frame50, live_particles] {
                at_shark(T);
                live_particles(1701);
                frame50();
                seed_rand();
                return Args{{T}, {}};
            }, [](A a) { shark_sand_puff_l2(a.i[0]); return uint64_t{0}; });
            // 1.02 Fixes, fix 18 (RAC1_FIXES102=18): the original with the 1.02 build's burst put in after its moby_drop_bolts
            // (the interpreter's call of moby_drop_bolts from L2:0x2d54d8 runs the port's, then the burst written out here: the
            // PAL disc 2.00's L2:0x2d68d0 .. 0x2d6920, 0.333 over the moby's place), against the port with the fix on
            if (const char* only = std::getenv("ORACLE_ONLY"); only == nullptr || std::strcmp(only, "sand_shark_fix18") == 0) {
                if (std::getenv("RAC1_FIXES102") == nullptr) {
                    std::printf("%-26s skipped (run with RAC1_FIXES102=18)\n", "sand_shark_fix18");
                    ++g_skipped;
                } else {
                    static int burst_cases = 0;
                    burst_cases = 0;
                    g_extra_hooks = {{OVF(0x00257470), [](EeCpu& c) {
                        const uint32_t m = c.r32(4);
                        moby_drop_bolts(m, c.r32(5), c.s32(6));
                        if (c.r32(31) != 0x002d54e0u) return;
                        ++burst_cases;
                        const float k = u2f(0x3eaa7efau);
                        const uint32_t at = ee_stack_alloc(0x10);
                        lq_sq(at, m + 0x10);
                        swc1(at + 8, fpu_add(lwc1(at + 8), k));
                        enemy_death_burst(m, at, k, 0.0f);
                        ee_stack_free(0x10);
                    }}};
                    level(0);
                    l2check("sand_shark_fix18", kStub, 60, Ret::None, [level, sharks, pick_of, shark_dying] {
                        level();
                        const uint32_t m = pick_of(sharks, 0x002d3e50);
                        shark_dying(m);
                        return Args{{m, u(ri(1, 4)), 0x002d3e50}, {}};
                    }, [](A a) {
                        g_advanced_disabled = false;
                        for (uint32_t i = 0; i < a.i[1]; ++i) sand_shark_update_l2(a.i[0]);
                        g_advanced_disabled = true;
                        return uint64_t{0};
                    });
                    g_extra_hooks.clear();
                    std::printf("%-26s %d runs of the original reached the burst\n", "sand_shark_fix18", burst_cases);
                    if (burst_cases == 0) ++g_fail;
                }
            }
        }
        if (!bots.empty()) {
            for (int st = 12; st < 18; ++st)
                for (int pass = 0; pass < 4; ++pass)
                    classcheck("constructorbot_hi", 0x002d7748, constructorbot_update_l2, [st, pass, generic_prep, frame50](uint32_t m) {
                        generic_prep(m, st, pass, constructorbot_update_l2);
                        frame50();
                        seed_rand();
                    }, 20);
            // the hits and the target: hits by class-0x264 mobys, a target of class 0x10e / 0xcb / 0, alive or not, in state 0xc
            l2check("constructorbot_hits", 0x002d85b8, 80, Ret::None, [level, bots, pick_of, generic_prep, frame50] {
                level();
                const uint32_t m = pick_of(bots, 0x002d7748);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                generic_prep(m, 0, ri(1, 3), constructorbot_update_l2);
                static const uint8_t states[] = {0, 3, 7, 0xc, 0xc, 0xd, 0x10, 0x11};
                w8(m + 0x20, states[ri(0, 7)]);
                const uint32_t base = ee_read<uint32_t>(OV(0x0015ffd8)), n = (ee_read<uint32_t>(OV(0x0015ffe0)) - base) >> 8;
                if (ee_read<uint8_t>(m + 0xa4) != 0xff && coin()) {  // the hit by a class-0x264 moby
                    const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                    const uint32_t o = base + u(ri(0, static_cast<int>(n) - 1)) * 0x100;
                    if (o != m) { w16(o + 0xa6, 0x264); w32(e + 0x20, o); }
                }
                if (ri(0, 2) == 0) wf(pv + 0x20, coin() ? 0.5f : rf(1.0f, 6.0f));
                const int t = ri(0, 4);
                if (t == 0) w32(pv + 0x220, 0);
                else if (t <= 2) {  // a moby of the level as its target, of a kept class, alive or not
                    const uint32_t o = base + u(ri(0, static_cast<int>(n) - 1)) * 0x100;
                    if (o != m) {
                        static const uint16_t cls[] = {0x10e, 0xcb, 0};
                        if (ri(0, 3) != 0) w16(o + 0xa6, cls[ri(0, 2)]);
                        if (ri(0, 3) == 0) w8(o + 0x20, u(ri(0x80, 0xfd)));
                        w32(pv + 0x220, o);
                    }
                }
                frame50();
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { constructorbot_hits_l2(a.i[0]); return uint64_t{0}; });
        }
        // ---- stage 4 P2.2: the rest of Aridia's own code: the transport pod's states past 11, Skid's and his agent's talk
        // state (1; frames 1, no talk started), the Sand Shark's hook +0x10, and the helpers' own checks
        const std::vector<uint32_t> pods = mobys_with(0x002ddc88), nests = mobys_with(0x002dcb38), launchers = mobys_with(0x002df3d8),
                                    skids = mobys_with(0x002e1950);
        if (!pods.empty()) {
            for (int st = 12; st < 18; ++st)
                for (int pass = 0; pass < 4; ++pass)
                    classcheck("transport_pod_hi", 0x002ddc88, transport_pod_update_l2, [st, pass, generic_prep, frame50](uint32_t m) {
                        generic_prep(m, st, pass, transport_pod_update_l2);
                        frame50();
                        seed_rand();
                    }, 20);
            // the pod's doors (after its set up: generic_prep's state 0 run spawns them), at their heights or moving
            auto pod_ready = [level, pods, pick_of, generic_prep, frame50]() {
                level();
                const uint32_t m = pick_of(pods, 0x002ddc88);
                generic_prep(m, 4, ri(0, 3), transport_pod_update_l2);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                for (uint32_t o : {0x84u, 0x88u}) {
                    const uint32_t d = ee_read<uint32_t>(pv + o);
                    if (d >= 0x00100000u && d < 0x01fff000u && coin()) wf(d + 0x18, ee_read<float>(m + 0x18) + rf(-5.0f, 3.0f));
                }
                wf(pv + 0x94, rf(-4.0f, 3.0f)); wf(pv + 0x98, rf(-4.0f, 3.0f));
                if (coin()) { wf(pv + 0x8c, rf(-0.5f, 0.5f)); wf(pv + 0x90, rf(-0.5f, 0.5f)); }
                frame50();
                seed_rand();
                return m;
            };
            l2check("pod_doors_seek", 0x002def80, 60, Ret::None, [pod_ready] { return Args{{pod_ready()}, {}}; },
                    [](A a) { pod_doors_seek_l2(a.i[0]); return uint64_t{0}; });
            l2check("pod_doors_snap", 0x002df0b0, 40, Ret::None, [pod_ready] { return Args{{pod_ready()}, {}}; },
                    [](A a) { pod_doors_snap_l2(a.i[0]); return uint64_t{0}; });
            l2check("pod_hero_out", 0x00237478, 40, Ret::None, [level, frame50] {
                level();
                wf(0x0013f4b0, coin() ? rf(0.0f, 2.6f) : rf(2.6f, 10.0f));
                frame50();
                return Args{{}, {}};
            }, [](A) { pod_hero_out_l2(); return uint64_t{0}; });
            l2check("pod_camera_mode", 0x002f8a18, 40, Ret::None, [level] {
                level();
                return Args{{u(ri(0, 4)), u(ri(0, 400))}, {}};
            }, [](A a) { pod_camera_mode_l2(a.i[0], a.i[1]); return uint64_t{0}; });
        }
        if (!sharks.empty()) {
            // the egg's hatch (class 755: nothing on Aridia spawns it; a shark stands in as the egg, another one is its shark +0x60)
            l2check("shark_egg_hatch", 0x002e0280, 40, Ret::None, [level, sharks, pick_of] {
                level();
                const uint32_t egg = pick_of(sharks, 0x002d3e50);
                uint32_t s = pick_of(sharks, 0x002d3e50);
                if (s == egg) s = sharks[0] == egg && sharks.size() > 1 ? sharks[1] : sharks[0];
                w32(ee_read<uint32_t>(egg + 0x78) + 0x60, s);
                w8(egg + 0x20, u(ri(1, 3)));
                seed_rand();
                return Args{{egg}, {}};
            }, [](A a) { shark_egg_update_l2(a.i[0]); return uint64_t{0}; });
            l2check("shark_hook_10", 0x002d6310, 30, Ret::Int32, [level, sharks, pick_of] {
                level();
                const int r = ri(0, 3);
                if (r == 0) return Args{{0}, {}};
                const uint32_t m = pick_of(sharks, 0x002d3e50);
                if (r == 1) w32(m + 0x78, 0);
                return Args{{m}, {}};
            }, [](A a) { return r32(shark_hook_10_l2(a.i[0])); });
        }
        if (!nests.empty() && !sharks.empty()) {
            // the nest's group: some of the sharks (in their lair, 0x1a, or not), other mobys
            auto nest_group = [sharks](uint32_t m) {
                if (ri(0, 4) == 0) { w8(m + 0x21, 0xff); return; }
                int32_t row = -1;
                for (int32_t i = 0x6f; i >= 0; --i) if (ee_read<uint32_t>(OV(0x001ab840) + u(i) * 4) == 0) { row = i; break; }
                if (row < 0) return;
                const uint32_t base = ee_read<uint32_t>(OV(0x0015ffd8));
                std::vector<uint32_t> mem;
                for (uint32_t s : sharks) if (coin()) { mem.push_back(s); w8(s + 0x20, ri(0, 2) != 0 ? 0x1au : u(ri(0, 0x1d))); w16(s + 0xb4, u(ri(0, 40))); }
                if (mem.empty()) mem.push_back(sharks[0]);
                const uint32_t list = T + 0x900;
                for (size_t i = 0; i < mem.size(); ++i) w16(list + 2 * u(static_cast<int32_t>(i)), ((mem[i] - base) >> 8) | (i + 1 == mem.size() ? 0x8000u : 0u));
                w32(OV(0x001ab840) + u(row) * 4, list);
                w8(m + 0x21, u(row));
                w16(m + 0xb4, u(ri(0, 40)));
            };
            l2check("nest_find_shark", 0x002dd2a8, 60, Ret::Int32, [level, nests, pick_of, nest_group] {
                level();
                const uint32_t m = pick_of(nests, 0x002dcb38);
                nest_group(m);
                return Args{{m}, {}};
            }, [](A a) { return r32(nest_find_shark_l2(a.i[0])); });
            l2check("nest_hits", 0x002dcfe0, 80, Ret::None, [level, nests, pick_of, generic_prep, frame50] {
                level();
                const uint32_t m = pick_of(nests, 0x002dcb38);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                generic_prep(m, 0, ri(1, 3), nest_update_l2);
                static const uint8_t states[] = {0, 1, 2, 3, 4};
                w8(m + 0x20, states[ri(0, 4)]);
                if (ri(0, 4) == 0) w8(pv + 0x2e, 2);
                if (ri(0, 2) == 0) w32(pv + 0xe0, coin() ? 0xffffffffu : (pick_of(nests, 0x002dcb38) - ee_read<uint32_t>(OV(0x0015ffd8))) >> 8);  // (its lair: none or a nest)
                if (ri(0, 2) == 0) wf(pv + 0x20, coin() ? 0.5f : rf(1.0f, 4.0f));
                frame50();
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { nest_hits_l2(a.i[0]); return uint64_t{0}; });
            // the nest opening: state 2 at its animation's frame 12 (its frame record moved to the test area), a shark of its group
            // in its lair: one sent out
            classcheck("nest_open", 0x002dcb38, nest_update_l2, [nest_group, frame50](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w8(m + 0x20, 2);
                w8(m + 0x70, ee_read<uint8_t>(m + 0x70) & ~2u);
                w8(m + 0x52, ee_read<uint8_t>(m + 0x53));
                wf(m + 0x54, 0.0f);
                w32(m + 0x68, T + 0xa00);
                w16(T + 0xa04, ri(0, 4) == 0 ? u(ri(0, 300)) : 12u * 16u);
                nest_group(m);
                w32(pv + 0xe0, coin() ? 0xffffffffu : (m - ee_read<uint32_t>(OV(0x0015ffd8))) >> 8);
                frame50();
                seed_rand();
            }, 40, 1);
            l2check("nest_shark_launch", 0x002d6118, 40, Ret::None, [level, sharks, pick_of] {
                level();
                const uint32_t s = pick_of(sharks, 0x002d3e50);
                Vec4 p = ee_ref<Vec4>(s + 0x10); p.x += rf(-3, 3); p.z += rf(0, 3); ee_ref<Vec4>(T) = p;
                Vec4 v{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(0.0f, 0.2f), 0.0f}; ee_ref<Vec4>(T + 0x10) = v;
                w8(s + 0x53, u(ri(0, 12)));
                return Args{{s, T, T + 0x10}, {}};
            }, [](A a) { nest_shark_launch_l2(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
        }
        if (!launchers.empty()) {
            l2check("launcher_sound", 0x002df730, 60, Ret::None, [level, launchers, pick_of, frame50] {
                level();
                const uint32_t m = pick_of(launchers, 0x002df3d8);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                wf(pv + 0x10, ri(0, 3) == 0 ? -1.0f : rf(0.0f, 120.0f));
                if (coin()) { Vec4 c = ee_ref<Vec4>(m + 0x10); c.x += rf(-60, 60); c.y += rf(-60, 60); c.z += rf(0, 20); ee_ref<Vec4>(OV(0x00166dc0)) = c; }
                if (ri(0, 4) == 0) w32(0x001413d4, 0x72);
                frame50();
                return Args{{m}, {}};
            }, [](A a) { launcher_sound_l2(a.i[0]); return uint64_t{0}; });
            // the firing state (2): its colour along the animation (its frame record in the test area: 0 .. 12 frames in)
            classcheck("launcher_fire", 0x002df3d8, rocket_launcher_update_l2, [frame50](uint32_t m) {
                w8(m + 0x20, 2);
                w8(m + 0x70, coin() ? (ee_read<uint8_t>(m + 0x70) & ~2u) : (ee_read<uint8_t>(m + 0x70) | 2u));
                w8(m + 0x52, ee_read<uint8_t>(m + 0x53));
                wf(m + 0x54, 0.0f);
                w32(m + 0x68, T + 0xa00);
                w16(T + 0xa04, u(ri(0, 200)));
                frame50();
                seed_rand();
            }, 40, 1);
            // aiming (1): facing its heading (exactly, within a degree or not), its turning speed (+0) zero or not, its sound's slot
            classcheck("launcher_aim", 0x002df3d8, rocket_launcher_update_l2, [frame50](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w8(m + 0x20, 1);
                wf(pv + 4, rf(0.0f, 3.1f));
                const int r = ri(0, 2);  // (facing it exactly and still: it fires; else near it or far, turning or not)
                wf(m + 0x48, ee_read<float>(pv + 4) + (r == 0 ? 0.0f : r == 1 ? rf(-0.01f, 0.01f) : rf(-1.0f, 1.0f)));
                wf(pv, r == 0 || coin() ? 0.0f : rf(-0.05f, 0.05f));  // (its turning speed)
                w16(pv + 0xc, ri(0, 2) == 0 ? 0xffffu : u(ri(0, 15)));
                frame50();
                seed_rand();
            }, 40, 1);
            l2check("launcher_rocket_spawn", 0x002ec228, 40, Ret::Int32, [level, launchers, pick_of] {
                level();
                const uint32_t m = pick_of(launchers, 0x002df3d8);
                Vec4 p = ee_ref<Vec4>(m + 0x10); p.z += rf(0, 3); ee_ref<Vec4>(T) = p;
                Vec4 d{rf(-1, 1), rf(-1, 1), rf(-0.3f, 0.6f), 0.0f}; ee_ref<Vec4>(T + 0x10) = d;
                seed_rand();
                return Args{{m, T, T + 0x10}, {}};
            }, [](A a) { return r32(launcher_rocket_spawn_l2(a.i[0], a.i[1], a.i[2])); });
        }
        if (!skids.empty()) {
            // the race start: the hero in the race box (+0x148) or out, the cameras' done words, the wait
            l2check("skid_race_start", 0x002e1fb8, 60, Ret::None, [level, skids, pick_of, hero_in_box, frame50] {
                level();
                const uint32_t m = pick_of(skids, 0x002e1950);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w32(pv + 0x180, u(ri(0, 2)));
                if (coin()) hero_in_box(ee_read<int32_t>(pv + 0x148));
                if (coin()) w32(pv + 0x14c, 0);
                if (coin()) w32(pv + 0x150, 0);
                w16(pv + 0x168, u(ri(0, 2)));
                frame50();
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { skid_race_start_l2(a.i[0]); return uint64_t{0}; });
        }
        // the rocks blasted (a hit of kind 0x800000 in state 1; the story byte 0x13d39d set or not for state 0)
        if (!mobys_with(0x002e0720).empty())
            classcheck("rocks_blast", 0x002e0720, rocks_update_l2, [give_hit](uint32_t m) {
                w8(m + 0x20, ri(0, 4) == 0 ? 0u : 1u);
                w8(0x0013d39d, coin() ? 1u : 0u);
                give_hit(m, 0x800000u);
                if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
                    const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                    w32(e + 0x24, 0x800000u | (R() & 0xffu));
                    w32(e + 0x34, m);
                }
                seed_rand();
            }, 30, 1);
        // (Skid's and his agent's talk state 1 is left out, as the class loop does: in one case in forty the original enters the
        // stream system's or the kernel's wait from it, which the interpreter cannot run)
    }
    g_extra_noop_hooks.clear();

    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
