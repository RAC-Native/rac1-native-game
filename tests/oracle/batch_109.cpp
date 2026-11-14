// Oracle batch 109 (task 0333): Aridia's own moby classes, part 2 (level 2): the classes transliterated from Aridia's
// overlay (tools/codegen/mkall.py, MKALL_LEVEL=2) on its real level data. Run with
// ORACLE_LEVEL=2 build/tests/ovl_oracle --batch 109 (needs the ISO). The subjects are the level's own mobys of each
// class (placed by its gameplay data) after the first gameplay frame, perturbed; the original is Aridia's own function
// at its own address, the port the one registered for it.

#include "ovl_oracle.h"

#include <game/ovl/aridia_classes_tr.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/aridia_mobys.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sound_emitter.h>
#include <runtime/kernel/disc.h>

#include <cmath>
#include <cstring>
#include <map>

namespace rac1::oracle {

namespace {
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
        {"class1324", 0x002ee890, class1324_update_l2}, {"class1479", 0x002ef020, class1479_update_l2}, {"swingshot_target", 0x002e0450, swingshot_target_update_l2},
        {"trespasser_pad", 0x002d8ad0, trespasser_pad_update_l2}, {"rocks", 0x002e0720, rocks_update_l2}, {"bg_ship1", 0x002ec4b8, bg_ship1_update_l2},
        {"bg_ship2", 0x002ece18, bg_ship2_update_l2},
        {"gold_bolt", 0x002ead38, gold_bolt_update_l2, 0x6, 1}, {"sandmouse", 0x002f0f88, sandmouse_update_l2}, {"class604", 0x002d7190, class604_update_l2},
        {"class832", 0x002e4a48, class832_update_l2}, {"class737", 0x002dfcd0, class737_update_l2},
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
    };
    extras[class1324_update_l2] = [hero_in_box](uint32_t m) {  // a zone (box pvar +0x5c) the hero is in, the hero's items and flags
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (ri(0, 3) != 0) hero_in_box(ee_read<int32_t>(pv + 0x5c));
        for (uint32_t i = 0; i < 37; ++i) { w32(0x0017a0c0 + 8 + 0x4c * i, ri(0, 3) == 0 ? 1u : 0u); w8(0x0013d4c0 + i, ri(0, 2) == 0 ? 0u : u(ri(1, 3))); }
        w32(0x001413dc, u(ri(0, 3))); if (ri(0, 3) != 0) { w16(0x00141be0, 0); w16(0x001418e8, 0); }
    };
    extras[class832_update_l2] = [hero_in_box](uint32_t m) {  // the hero in state 0x1d, a class-0xac moby in the list at 0x15ffe4, zone records
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
    extras[swingshot_target_update_l2] = [](uint32_t m) {  // the moby indexes it follows (pvar +0x30 / +0x3c: -1 = none) and their states
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), n = (ee_read<uint32_t>(OV(0x0015ffe0)) - first) >> 8;
        for (uint32_t o : {0x30u, 0x3cu}) {
            if (ri(0, 2) == 0) { w32(pv + o, 0xffffffffu); continue; }
            const uint32_t idx = u(ri(0, static_cast<int>(n) - 1));
            w32(pv + o, idx);
            if (ri(0, 2) == 0) w8(first + (idx << 8) + 0x20, coin() ? 0xfeu : 0xfdu);
        }
    };
    extras[liftcar_platform_update_l2] = [hero_in_box](uint32_t m) {  // its zone and the hero in / out of it
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        for (uint32_t o = 0; o < 0x100; o += 4) { const int32_t v = ee_read<int32_t>(pv + o); if (v >= 0 && v < 8 && coin()) { hero_in_box(v); break; } }
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
        }, [](A a) { return ri32(static_cast<int32_t>(cam_type_12_accept_l2(a.i[0], a.i[1]))); });
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
        }, [](A a) { cam_mode_hook_2fa710_l2(a.i[0]); return uint64_t{0}; });
    }
    for (const TrCls& c : trcls) {
        if (std::getenv("B109_CLS") != nullptr && std::strcmp(std::getenv("B109_CLS"), c.name) != 0) continue;
        for (int st = 0; st < 12; ++st)
        for (int pass = 0; pass < 4; ++pass)
        if ((c.skip_states & (1u << st)) == 0) classcheck(c.name, c.addr, c.fn, [st, pass, c, generic_prep](uint32_t m) { generic_prep(m, st, pass, c.fn); }, 20, c.frames);
    }
    g_extra_noop_hooks.clear();

    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
