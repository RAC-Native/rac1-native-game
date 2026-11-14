// Oracle batch 139 (task 0386, Eudora part 2): what a play-through of Eudora still reached unported or unproved. Run with
// ORACLE_LEVEL=4 (Eudora's own original against the relocated port; needs the ISO); the base ELF functions also on level 0.
//  - libgcc's soft-float single -> double (base ELF): __unpack_f 0x1203b0, __pack_d 0x11f738, __make_dp 0x120370,
//    __extendsfdf2 0x120478 (Eudora's enemy helper L4:0x293128 converts a distance for a debug print with it);
//  - that helper, L4:0x293128 (now the hand port walker_gait_update_l4, eudora_walker.cpp), with the flags (a2 & 0x30) that take it to the conversion, on the woodcut bots (class 563);
//  - the states of the Suck Cannon (class 1120: 1 its scene, 2 the weapon given, the checkpoint, the save request, 3) and of the
//    gold bolt (class 1134: 1 / 2, the help message and the save request) that batch 111 skips: here the level change request
//    (L4:0x23fe20, Veldin 0x24d430) runs on both sides, the console's clock the host's on both (as batch 75); 1120's scene start itself
//    is not run (see below);
//  - the classes no placement gives, spawned in play: 1516 (the Woodcut Bot's), 184 (the Blarg Trooper's), 1633 (the Sandmouse's).

#include "ovl_oracle.h"

#include <game/lib/libcdvd.h>
#include <game/lib/libgcc_fp.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_shell.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/disc.h>

#include <cmath>
#include <cstring>

namespace rac1::oracle {

namespace {
// A check whose original is Eudora's own function at `l4addr` (as batch 111's).
void l4check139(const char* name, uint32_t l4addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (g_oracle_level != 4) {
        std::printf("%-26s L4:0x%08x  skipped (Eudora only: run with ORACLE_LEVEL=4)\n", name, l4addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L4:0x%08x\n", name, l4addr);
    if (!check_fn(name, l4addr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
// a float of every kind: zero, a denormal, a normal number (small / large / near 1), infinity, NaN, any bits
float any_float() {
    uint32_t b = 0;
    switch (ri(0, 7)) {
    case 0: b = R() & 0x80000000u; break;
    case 1: b = (R() & 0x807fffffu); break;
    case 2: b = (R() & 0x807fffffu) | (u(ri(1, 254)) << 23); break;
    case 3: b = (R() & 0x807fffffu) | (u(ri(120, 134)) << 23); break;
    case 4: b = (R() & 0x80000000u) | 0x7f800000u; break;
    case 5: b = (R() & 0x807fffffu) | 0x7f800000u | 1u; break;
    default: b = R(); break;
    }
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}
}  // namespace

void batch_139() {
    if (g_only_batch != 139) return;
    g_batch = 139;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // (the particle update's saved return address)
    g_ignored_words.push_back(OV(0x0016018c));
    g_extra_noop_hooks.clear();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime

    // ---- libgcc (the base ELF): the soft-float conversion single -> double and its helpers, on the test area
    check("libgcc_unpack_f", 0x001203b0, 400, Ret::None, [] {
        wrand(T, 8);
        wf(T2, any_float());
        return Args{{T2, T}, {}};
    }, [](A a) { libgcc_unpack_f(a.i[0], a.i[1]); return uint64_t{0}; });
    check("libgcc_pack_d", 0x0011f738, 600, Ret::Int64, [] {
        wrand(T, 8);
        const int k = ri(0, 9);
        w32(T, k == 0 ? R() : u(ri(0, 5)));  // the class (0 / 1 NaN, 2 zero, 3 number, 4 infinity; others as a number)
        w32(T + 4, coin() ? u(ri(0, 1)) : R());  // the sign
        static const int32_t ex[] = {-1100, -1080, -1060, -1030, -1023, -1022, -1021, -1000, -127, -1, 0, 1, 127, 1000, 1022, 1023, 1024, 1100};
        w32(T + 8, ri(0, 3) == 0 ? R() : u(ex[ri(0, 17)] + ri(-3, 3)));
        uint64_t frac = (static_cast<uint64_t>(R()) << 32) | R();
        switch (ri(0, 5)) {
        case 0: frac = 0; break;
        case 1: frac = static_cast<uint64_t>((R() & 0x7fffffu) << 7 | 0x40000000u) << 30; break;  // as __extendsfdf2 gives it
        case 2: frac = (frac >> 2) | (uint64_t{1} << 61); frac = (frac & ~uint64_t{0x1ff}) | (coin() ? 0x80 : (coin() ? 0x180 : (R() & 0x1ff))); break;  // the rounding's ties
        case 3: frac = ~uint64_t{0} >> ri(0, 4); break;  // the carry out of the rounding
        default: break;
        }
        ee_write<uint64_t>(T + 0x10, frac);
        return Args{{T, 0, R()}, {}};
    }, [](A a) { return libgcc_pack_d(a.i[0], static_cast<uint64_t>(sx(a.i[2]))); });
    check("libgcc_make_dp", 0x00120370, 300, Ret::Int64, [] {
        static const int32_t ex[] = {-1100, -1030, -1022, -127, -1, 0, 1, 127, 1023, 1024};
        return Args{{u(ri(0, 4)), u(ri(0, 1)), u(ex[ri(0, 9)] + ri(-2, 2)), R()}, {}};
    }, [](A a) { return libgcc_make_dp(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), static_cast<uint64_t>(sx(a.i[3]))); });
    check("libgcc_extendsfdf2", 0x00120478, 600, Ret::Int64, [] { return Args{{}, {any_float()}}; },
          [](A a) { return libgcc_extendsfdf2(a.f[0]); });

    if (g_oracle_level != 4) {
        l4check139("enemy_helper_293128", 0x00293128, 0, Ret::None, nullptr, nullptr);
        l4check139("class1120_scene", 0x002e1a10, 0, Ret::None, nullptr, nullptr);
        l4check139("class1134_scene", 0x002e1f38, 0, Ret::None, nullptr, nullptr);
        g_ignored_words.clear();
        g_ee_null_is_ram = null_ram;
        return;
    }

    // ---- Eudora's own code (ORACLE_LEVEL=4): the level as batch 111 sets it up
    // not compared (as batch 111): the null page, the scratchpad words the pose function spills the caller's $t8 / $t9 into
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);
    // 989snd's IOP server (as batch 111's): replies a function of the command; the stream system's disc reads done into EE RAM
    {
        auto result = [](uint32_t id, uint32_t a0) -> uint32_t {
            const uint32_t h = (id * 0x9e3779b1u) ^ (a0 * 0x85ebca6bu);
            return ((h >> 7) % 4 == 0 && id != 0x38) ? 0u : ((h >> 3) | 1u);  // (the stream's disc read always accepted: a scene start waits for it)
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
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    level();
    for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }

    // the kernel's vsync wait and the IOP / DMA calls batch 111 stubs (no longer 0x120478: ported)
    g_extra_noop_hooks = {0x00122298, 0x001232c8, 0x00122fe0, 0x00118a80};

    // ---- L4:0x293128 (the enemy helper of classes 217 / 563, reached through 0x295778): its states (+0xb7 of the record a1, 0..20)
    // with the flags a2 holding 0x30 or not, on the woodcut bots (a1 = pvars + 0x160, as class 563's update passes it)
    {
        const std::vector<uint32_t> ms = mobys_with(0x002d16b8);
        if (ms.empty()) std::printf("enemy_helper_293128        no class 563 moby on this level\n");
        else
            l4check139("enemy_helper_293128", 0x00293128, 300, Ret::None, [level, ms] {
                level();
                const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                const uint32_t rec = ee_read<uint32_t>(m + 0x78) + 0x160;
                w8(rec + 0xb7, u(ri(0, 21)));
                if (coin()) wf(rec + 0xc0, rf(0, 20));
                Vec4 q = ee_ref<Vec4>(m + 0x10);  // the hero near or far
                const float r = coin() ? rf(0, 6) : rf(6, 40), an = rf(-3.1f, 3.1f);
                q.x += r * std::cos(an); q.y += r * std::sin(an);
                ee_ref<Vec4>(0x0013f3d0) = q;
                seed_rand();
                const uint32_t flags = ri(0, 3) == 0 ? (R() & 0xff) : ((R() & 0xcf) | 0x10u << ri(0, 1));
                return Args{{m, rec, flags}, {rf(0, 3), rf(-3.1f, 3.1f), rf(0, 20)}};
            }, [](A a) { walker_gait_update_l4(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
    }

    // ---- the scene / level-change states of the Suck Cannon (1120) and the gold bolt (1134): the level change request runs on both
    // sides (its console clock the host's, fixed), the hero at the moby or near it, the game mode 0 or 2
    g_host_clock_fixed = 1759658400;
    g_extra_hooks = {{0x00121780, [](EeCpu& c) { c.set32(2, cd_read_clock(c.r32(4))); }},
                     {0x0012d6d8, [](EeCpu& c) { clock_to_local(c.r32(4)); }},
                     // the scene's disc reads (1120's state 1 starts scene 2: libcdvd) the host's on both sides, as batch 84's
                     {0x00121450, [](EeCpu& c) { c.set32(2, cd_read(c.r32(4), c.r32(5), c.r32(6), c.r32(7))); }},
                     {0x00120c30, [](EeCpu& c) { c.set32(2, cd_sync(static_cast<int32_t>(c.r32(4)))); }},
                     {0x00121630, [](EeCpu& c) { c.set32(2, cd_get_error()); }},
                     {0x001216c8, [](EeCpu& c) { c.set32(2, cd_break()); }}};
    struct Cls { const char* name; uint32_t addr; void (*fn)(uint32_t); int st_lo, st_hi; };
    static const Cls cls[] = {
        {"class1120_scene", 0x002e1a10, eud_class1120_update_l4, 1, 3},
        {"class1134_scene", 0x002e1f38, gold_bolt_update, 1, 2},
    };
    for (const Cls& c : cls) {
        const std::vector<uint32_t> ms = mobys_with(c.addr);
        if (ms.empty()) { std::printf("%-26s L4:0x%08x  no moby on this level\n", c.name, c.addr); continue; }
        l4check139(c.name, c.addr, 120, Ret::None, [level, ms, c] {
            level();
            const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            w8(m + 0x20, 0);
            c.fn(m);  // (its first update, the port's: as batch 111's)
            const int st = ri(c.st_lo, c.st_hi);
            w8(m + 0x20, u(st));
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            // (1120's state 1 within 3 units starts scene 2 (cutscene_start, proved by batch 80 on Veldin): its stream read ends in
            // 989snd's callback, run from the SIF's end function, which the oracle's host SIF does not run -- the original waits forever.
            // Here the hero stays 3+ units away in that state; the scene start was checked in the game, run tour)
            const bool no_scene = c.addr == 0x002e1a10 && st == 1;
            const float r = no_scene ? rf(3.2f, 10) : ri(0, 2) != 0 ? rf(0, 2.5f) : rf(2.5f, 10), an = rf(-3.1f, 3.1f);
            q.x += r * std::cos(an); q.y += r * std::sin(an); q.z += rf(-1, 1);
            ee_ref<Vec4>(0x0013f3d0) = q;
            w32(0x0013f350 + 0x22a8, ri(0, 4) == 0 ? 0u : 1u);  // (the hero able to collect: 1134's pick-up tests it)
            { const int k = ri(0, 3); w8(0x0013f350 + 0x20a4, k == 0 ? 3u : k == 1 ? 0u : u(ri(1, 8))); }
            if (c.addr == 0x002e1f38 && st == 2) {  // (state 2: the pick-up animation's end, m + 0x52 == 1; the flags it tests)
                if (coin()) w8(m + 0x52, 1);
                w8(m + 0x70, R());
                w32(0x0013f350 + 0xa98, R());
            }
            if (c.addr == 0x002e1f38 && coin()) w16(ee_read<uint32_t>(m + 0x78) + 0xc, u(ri(0, 1)));  // (the bolt's save flag: the help message + save request, or its pick-up animation)
            w32(OV(0x0015f5c4), ri(0, 3) == 0 ? 2u : 0u);  // the game mode (state 2 waits while it is 2: the scene)
            if (coin()) w8(0x0013d4c9, u(ri(0, 1)));  // (the flag 1120's state 2 tests for its help message)
            w32(OV(0x0015f5cc), R() & 0xffff);
            {  // the dialog / stream slot idle, as batch 80's sound_ready (a scene start waits for its stream reads: 0x2660c0)
                const uint32_t S = 0x001516d0;
                w16(S + 0x5a, 3);
                w32(S + 0x50, (R() & 0x7ffffffeu) | 1u);
                w16(S + 0x5c, 0);
                w32(S + 0x1c, 0xffffffffu);
                w16(S + 8, 0);
            }
            seed_rand();
            return Args{{m}, {}};
        }, [c](A a) { c.fn(a.i[0]); return uint64_t{0}; });
    }
    g_extra_hooks.clear();
    g_host_clock_fixed = 0;

    // ---- the classes no placement gives (batch 111 had no subject), spawned in play: 1516 (the Woodcut Bot's chips), 184 (the Blarg
    // Trooper's, through L4:0x2aee30), 1633 (L4:0x2e5818, the Sandmouse's helper). The level run on (the port's gameplay frames) with the
    // hero put near the spawners until such a moby lives; its update then compared in states 0..3 with the hero near / far
    struct Spawned { const char* name; uint32_t addr; void (*fn)(uint32_t); float hx, hy, hz; };
    static const Spawned sp[] = {
        {"class1516", 0x002e5178, eud_class1516_update_l4, 222.0f, 166.0f, 56.0f},
        {"class184", 0x002af058, shot_update, 157.0f, 226.0f, 60.0f},
        {"class1633", 0x002e5928, class1633_update, 146.0f, 203.0f, 60.0f},
    };
    for (const Spawned& c : sp) {
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, c.name) != 0) continue;
        static std::vector<uint8_t> snap2;
        level();
        std::vector<uint32_t> ms;
        int frames = 0;
        for (; frames < 1200 && ms.empty(); ++frames) {
            if (frames % 60 == 0) ee_ref<Vec4>(0x0013f3d0) = Vec4{c.hx, c.hy, c.hz, 1.0f};
            gameplay_frame();
            if (c.addr == 0x002e5928 && frames == 60)  // (1633: its spawner L4:0x2e5818, which the Sandmouse's helper 0x2e6080 calls with the
                // sandmouse as the parent and two distances, an angle, a scale in f12..f15: called here so on the level's Sandmouse)
                for (uint32_t s : mobys_with(0x002e69d0))
                    for (int k = 0; k < 3; ++k) {
                        const float d0 = rf(1, 5), d1 = rf(1, 5), ang = rf(-3, 3), sc = rf(0.5f, 1.5f);
                        class1633_spawn(s, 0, d0, d1, ang, sc);  // (the shared port of the spawner, shared_sandmouse.cpp)
                    }
            ms = mobys_with(c.addr);
        }
        if (ms.empty()) { std::printf("%-26s L4:0x%08x  none spawned in %d frames (not compared)\n", c.name, c.addr, frames); continue; }
        std::printf("%-26s L4:0x%08x  %zu spawned after %d frames\n", c.name, c.addr, ms.size(), frames);
        uint8_t* ram = ee_get_ram_base();
        snap2.assign(ram, ram + 0x02000000);
        l4check139(c.name, c.addr, 120, Ret::None, [ms, c] {
            std::memcpy(ee_get_ram_base(), snap2.data(), snap2.size());
            const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            if (ri(0, 2) == 0) w8(m + 0x20, u(ri(0, 3)));
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            const float r = coin() ? rf(0, 4) : rf(4, 40), an = rf(-3.1f, 3.1f);
            q.x += r * std::cos(an); q.y += r * std::sin(an);
            ee_ref<Vec4>(0x0013f3d0) = q;
            w32(OV(0x0015f5cc), R() & 0xffff);
            seed_rand();
            return Args{{m}, {}};
        }, [c](A a) { c.fn(a.i[0]); return uint64_t{0}; });
    }

    g_ignored_words.clear();
    g_extra_noop_hooks.clear();
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
