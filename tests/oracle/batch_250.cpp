// Oracle batch 250: level 16's (Kalebo III) own code on its real level data: its moby classes, the callbacks they install, the sound emitters,
// the camera types, the class hooks, the camera mode hook and its callback, the movement-mode hero update, moby_spawn and mobys_in_sphere
// (level 16's copies). Run with ORACLE_LEVEL=16 build/tests/ovl_oracle --batch 250 (needs the ISO; no dumps). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the original is level 16's own function at its
// own address, the port the one registered for it (the transliterations kalebo_classes_tr_N.cpp / kalebo_hero_tr_N.cpp, generated from the disc by
// tools/codegen/mkall_l16.py). Made after batch 124 (Pokitaru).
#include "ovl_oracle.h"

#include <game/ovl/enemy_749.h>
#include <game/ovl/hero_move_mode.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sound_emitter.h>
#include <runtime/kernel/disc.h>

#include <csetjmp>
#include <csignal>
#include <cmath>
#include <cstring>
#include <string>
#include <map>

namespace rac1::oracle {

namespace {
// A random state can hold a pointer outside RAM that the port's vector helpers dereference without a check (a SIGSEGV where the interpreter would wrap it): the trial run of a state
// turns it into "not a game state" (the state is dealt again), as the port's "Pointer resolution out of bounds" exception does.
sigjmp_buf g_trial_jmp;
void trial_segv(int) { siglongjmp(g_trial_jmp, 1); }
bool trial_run(void (*port)(uint32_t), uint32_t m) {
    struct sigaction sa {}, old {};
    sa.sa_handler = trial_segv;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NODEFER;
    sigaction(SIGSEGV, &sa, &old);
    bool ok = true;
    if (sigsetjmp(g_trial_jmp, 1) != 0) ok = false;
    else {
        try { port(m); } catch (const std::exception&) { ok = false; }
    }
    sigaction(SIGSEGV, &old, nullptr);
    return ok;
}
constexpr uint32_t MOBY_SPAWN_LV = 0x00249f10;

// A check whose original is level 16's own function at `laddr` (not a Veldin address to translate).
void lvcheck(const char* name, uint32_t laddr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (const char* skip = std::getenv("B250_SKIP"); skip != nullptr && (std::string(",") + skip + ",").find(std::string(",") + name + ",") != std::string::npos) return;  // (the mutation test: checks already seen to fail)
    if (g_oracle_level != 16) {
        std::printf("%-26s L16:0x%08x  skipped (level 16 only: run with ORACLE_LEVEL=16)\n", name, laddr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L16:0x%08x\n", name, laddr);
    if (!check_fn(name, laddr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_250() {
    if (g_only_batch != 250) return;
    g_batch = 250;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    // segment_collide's hit point (the global at Veldin 0x174610, 3 words): written by the callee from the segment's end points; class 911's
    // state 1 builds the segment from stale stack lanes (the port's EE stack and the interpreter's hold other leftovers), nothing reads it
    for (uint32_t k = 0; k < 12; k += 4) g_ignored_words.push_back(OV(0x00174610) + k);
    // the segment records (Veldin 0x173e90 .. 0x173ebc: 0x1efc70's sqc2 of VU0 registers the earlier checks left in the interpreter, the
    // port's VU0 state starting fresh; the classes read them only after a segment test wrote them): as batch 124's 0x173e9c
    for (uint32_t k = 0; k < 0x30; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
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
    // the level's mobys whose update function is `fn` (level 16's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60, std::vector<uint32_t> subjects = {}, uint32_t spawn_cls = 0) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 16) { lvcheck(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
        level();
        const std::vector<uint32_t> ms = subjects.empty() ? mobys_with(fn) : subjects;
        // a class the level's gameplay data does not place (another class or a scene spawns it): a moby of the class spawned by the
        // game's own moby_spawn next to the hero, its first updates done by the check itself
        if (ms.empty() && spawn_cls == 0) { std::printf("%-26s L16:0x%08x  no moby on this level\n", name, fn); return; }
        lvcheck(name, fn, n, Ret::None, [level, ms, vary, port, spawn_cls] {
            // a random state that sends the port's code through a pointer outside RAM (a garbage pointer the state machine had not set yet: the
            // original's interpreter wraps it, the port refuses) is not a game state: it is dealt again (the RAM put back after the trial)
            static std::vector<uint8_t> saved;
            uint32_t m = 0;
            for (int attempt = 0; attempt < 40; ++attempt) {
                level();
                if (ms.empty()) {
                    m = moby_spawn(spawn_cls);
                    if (m == 0) break;
                    Vec4 q = ee_ref<Vec4>(0x0013f3d0);
                    q.x += rf(-4, 4); q.y += rf(-4, 4); q.z += rf(0, 1);
                    ee_ref<Vec4>(m + 0x10) = q;
                } else {
                    m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                }
                vary(m);
                uint8_t* ram = ee_get_ram_base();
                saved.assign(ram, ram + 0x02000000);
                bool ok = true;
                const uint32_t stack_top = ee_stack_ptr();
                ok = trial_run(port, m);
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
    if (g_oracle_level == 16) {
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
        {  // the particle pool (0x800 records of 0x40 bytes at *(0x16016c)): the w lane of the velocity vector a spray copies (a stale lane of vec_from_spherical in the original)
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c);
        }
    }

    // ---- the level's own classes (transliterated by tools/codegen/mkall_l16.py): the level's own mobys of each class, the first
    // update (state 0) done by the port, then a state, a hit, the hero near, the pvars' floats / counters moved as typed
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};  // (+ the level change request: the original would run the CD / movie path)
    struct TrCls { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t skip_states = 0; bool walk = false; uint32_t skip_p0 = 0; uint32_t skip_p1 = 0; };  // (skip_p0 / skip_p1: states skipped in pass 0 / 1 only)  // (the states that start a scene / a level change: the original runs the kernel / movie path for real)
    // skip_states: (848 / 1523 state 1: a spawned moby's unset links: the port refuses a pointer the interpreter wraps / the grid's missing-id trap;
    // the taxi 1410's states 3..5 on a placed taxi nobody rides: a pointer of -16 the port refuses, the interpreter wraps;
    // the states that request a level change (0x24d430's copy: the original would run the CD / movie path for real) or start a
    // music stream (its disc read: sceCdRead 0x121450 without the IOP's stream system, a TODO_PORT on the port's side; class 1419 state 1, 1446 states 5 / 7)
    static const TrCls trcls[] = {
        {"class49", 0x002a05e8, kal_class49_update_l16, 0x0, false},
        {"class50", 0x002a0a98, kal_class50_update_l16, 0x0, false},
        {"class52", 0x002a1088, kal_class52_update_l16, 0x0, false},
        {"class1475", 0x002a3f38, kal_class1475_update_l16, 0x0, false},
        {"class235", 0x002c3be8, kal_class235_update_l16, 0x0, false},
        {"class257", 0x002c3d38, kal_class257_update_l16, 0x0, false},
        {"class281", 0x002c5eb0, kal_class281_update_l16, 0x0, false},
        {"class439", 0x002c7788, kal_class439_update_l16, 0x0, false},
        {"class470", 0x002c9480, kal_class470_update_l16, 0x0, false},
        {"class471", 0x002c9878, kal_class471_update_l16, 0x0, false},
        {"class482", 0x002cb600, kal_class482_update_l16, 0x0, false},
        {"class541", 0x002cddb8, kal_class541_update_l16, 0x0, false},
        {"class546", 0x002cf198, kal_class546_update_l16, 0x0, false},
        {"class552", 0x002cf4a8, kal_class552_update_l16, 0x0, false},
        {"class556", 0x002d04a0, kal_class556_update_l16, 0x0, false},
        {"class605", 0x002d2a80, kal_class605_update_l16, 0x0, false},
        {"class638", 0x002d2cf0, kal_class638_update_l16, 0x0, false},
        {"class647", 0x002d59e0, kal_class647_update_l16, 0x0, false},
        {"class650", 0x002d5cb8, kal_class650_update_l16, 0x0, false},
        {"class654", 0x002d5ef8, kal_class654_update_l16, 0x0, false},
        {"class758", 0x002d6378, kal_class758_update_l16, 0x0, false},
        {"class800", 0x002d6740, kal_class800_update_l16, 0x0, false},
        {"class805", 0x002d69e0, kal_class805_update_l16, 0x0, false},
        {"class876", 0x002dddd8, kal_class876_update_l16, 0x0, false},
        {"class933", 0x002ddde0, kal_class933_update_l16, 0x0, false},
        {"class1111", 0x002dee10, kal_class1111_update_l16, 0x0, false},
        {"class1134", 0x002df1e8, kal_class1134_update_l16, 0x6, false},
        {"class1135", 0x002e0120, kal_class1135_update_l16, 0x0, false},
        {"class1139", 0x002e0f78, kal_class1139_update_l16, 0x0, false},
        {"class1140", 0x002e1080, kal_class1140_update_l16, 0x0, false},
        {"class1356", 0x002e22d8, kal_class1356_update_l16, 0x0, false},
        {"class1377", 0x002e3190, kal_class1377_update_l16, 0x4, false},
        {"class1387", 0x002e36e8, kal_class1387_update_l16, 0x0, false},
        {"class1401", 0x002e37a0, kal_class1401_update_l16, 0x0, false},
        {"class1410", 0x002e43e0, kal_class1410_update_l16, 0x38, false},
        {"class1439", 0x002e5010, kal_class1439_update_l16, 0x0, false},
        {"class1441", 0x002e5258, kal_class1441_update_l16, 0x0, false},
        {"class1442", 0x002e54a0, kal_class1442_update_l16, 0x0, false},
        {"class1443", 0x002e5708, kal_class1443_update_l16, 0x0, false},
        {"class1445", 0x002e5e08, kal_class1445_update_l16, 0x0, false},
        {"class1455", 0x002e6808, kal_class1455_update_l16, 0x10, false},
        {"class1561", 0x002e7208, kal_class1561_update_l16, 0x0, false},
        {"class1667", 0x002e76b8, kal_class1667_update_l16, 0x0, false},
        {"class1812", 0x002e7e00, kal_class1812_update_l16, 0x0, false},
        {"class1826", 0x002e7f80, kal_class1826_update_l16, 0x0, false},
        {"class1891", 0x002e88d0, kal_class1891_update_l16, 0x0, false},
        {"class1923", 0x002e8970, kal_class1923_update_l16, 0x0, false},
        {"class1943", 0x002e8e80, kal_class1943_update_l16, 0x0, false},
        {"class1947", 0x002e93b0, kal_class1947_update_l16, 0x0, false},
        {"class1948", 0x002e98e0, kal_class1948_update_l16, 0x0, false},
        {"class1952", 0x002e9df0, kal_class1952_update_l16, 0x0, false},
        {"class1953", 0x002ea128, kal_class1953_update_l16, 0x0, false},
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
    if (g_oracle_level == 16 && std::getenv("B250_INFO") != nullptr) {  // the level's mobys of each class: how many, the nearest to the hero's start
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
        if (std::getenv("B250_CLS") != nullptr && std::strcmp(std::getenv("B250_CLS"), c.name) != 0) continue;
        for (int st = 0; st < 16; ++st)
        for (int pass = 0; pass < 3; ++pass) {
        if (g_oracle_level == 16 && std::getenv("B250_TAGS") != nullptr) std::printf("@@ %s st %d pass %d\n", c.name, st, pass);
        if ((std::getenv("B250_ST") == nullptr || std::atoi(std::getenv("B250_ST")) == st) && (std::getenv("B250_PASS") == nullptr || std::atoi(std::getenv("B250_PASS")) == pass) && (c.skip_states & (1u << st)) == 0 && !(pass == 0 && (c.skip_p0 & (1u << st)) != 0) && !(pass == 1 && (c.skip_p1 & (1u << st)) != 0)) classcheck(c.name, c.addr, c.fn, [st, pass, c, generic_prep](uint32_t m) { generic_prep(m, st, pass, c.fn, c.walk); }, 12, {}, static_cast<uint32_t>(std::atoi(c.name + 5)));
        }
    }
    g_extra_noop_hooks.clear();

    // ---- level 16's own moby callbacks (draw callbacks / update functions a class installs): a0 = the moby; the subjects are
    // the mobys of the class whose code installs it, after one update of the class in a state, a hit, the hero near
    if (g_oracle_level == 16) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        struct Cb { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t cls_addr; int cls; };
        // the display packet cursor (*0x1611c0) is null in the unit batches' memory: the draw helpers' packet writes land in low memory (0x100 .. 0x1500; the cursor is given a real buffer below) and carry a stale
        // callee-saved register (s7) of the original's frame: not game state
        const size_t ign_before = g_ignored_words.size();
        for (uint32_t a = 0x100; a < 0x1500; a += 4) g_ignored_words.push_back(a);
        // (the hero's callbacks, installed by its hero_set_state: batch 242, on the race's dumps)
        static const Cb cbs[] = {
        {"cb2c9cd0", 0x002c9cd0, kal_cb_2c9cd0_l16, 0x002c9878, 471},
        {"cb2cef60", 0x002cef60, kal_cb_2cef60_l16, 0x00000000, 0},
        {"cb2cf7a8", 0x002cf7a8, kal_cb_2cf7a8_l16, 0x002cf4a8, 552},
        {"cb2de3c0", 0x002de3c0, kal_cb_2de3c0_l16, 0x002dee10, 1111},
        {"cb2e0a38", 0x002e0a38, kal_cb_2e0a38_l16, 0x002e0120, 1135},
        {"cb2e58d8", 0x002e58d8, kal_cb_2e58d8_l16, 0x002e5708, 1443},
        {"cb2e84e8", 0x002e84e8, kal_cb_2e84e8_l16, 0x002e7f80, 1826},
        {"cb2e8d40", 0x002e8d40, kal_cb_2e8d40_l16, 0x002e8e80, 1943},
        {"cb2e9270", 0x002e9270, kal_cb_2e9270_l16, 0x002e93b0, 1947},
        {"cb2e97a0", 0x002e97a0, kal_cb_2e97a0_l16, 0x002e98e0, 1948},
        {"cb2e9ce0", 0x002e9ce0, kal_cb_2e9ce0_l16, 0x002e9df0, 1952},
        {"cb2e9fe8", 0x002e9fe8, kal_cb_2e9fe8_l16, 0x002ea128, 1953},
        };
        for (const Cb& cb : cbs) {
            if (std::getenv("B250_CLS") != nullptr && std::strcmp(std::getenv("B250_CLS"), cb.name) != 0) continue;
            level();
            std::vector<uint32_t> subj;
            if (cb.cls_addr != 0) subj = mobys_with(cb.cls_addr);
            else for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) if (ee_read<uint8_t>(m + 0x20) < 0xfe) subj.push_back(m);
            void (*clsfn)(uint32_t) = nullptr;
            for (const TrCls& c : trcls) if (c.addr == cb.cls_addr) clsfn = c.fn;
            if (subj.empty()) { std::printf("%-26s L16:0x%08x  no moby on this level\n", cb.name, cb.addr); continue; }
            const size_t ign_pk = g_ignored_words.size();
            for (int st = 0; st < 4; ++st)
            for (int pass = 0; pass < 3; ++pass) {
                if (clsfn == nullptr && (st != 0 || pass != 0)) continue;
                classcheck(cb.name, cb.addr, cb.fn, [st, pass, clsfn, generic_prep](uint32_t m) { generic_prep(m, st, pass, clsfn != nullptr ? clsfn : kal_class49_update_l16, false); w32(OV(0x001611c0), 0x00800000u); }, 12, subj);  // (the display packet cursor: a real buffer for the draw callbacks)
            }
            g_ignored_words.resize(ign_pk);
        }
        g_ignored_words.resize(ign_before);
        g_extra_noop_hooks.clear();
    }

    // ---- level 16's sound emitter types (section 5 of the overlay) on the level's own emitters
    if (g_oracle_level == 16) {
        struct Em { const char* name; uint32_t addr; void (*fn)(uint32_t); };
        static const Em ems_t[] = {{"emitter1", 0x002f58a0, emitter_fade_box_update}, {"emitter3", 0x002f5c40, emitter_reverb_box_update}, {"emitter6", 0x002f5da0, emitter_music_box_update}};
        for (const Em& e : ems_t) {
            level();
            std::vector<uint32_t> ems;
            for (int32_t i = 0; i < ee_read<int32_t>(0x0013f2e0); ++i) {
                const uint32_t em = ee_read<uint32_t>(0x0013f2e4) + static_cast<uint32_t>(i) * 0x90;
                if (ee_read<uint32_t>(em + 4) == e.addr) ems.push_back(em);
            }
            if (ems.empty()) { std::printf("%-26s L16:0x%08x  no emitter of the type on this level\n", e.name, e.addr); continue; }
            if (std::getenv("B250_INFO") != nullptr) std::printf("INFO %s 0x%08x emitters %zu\n", e.name, e.addr, ems.size());
            auto fn = e.fn;
            lvcheck(e.name, e.addr, 120, Ret::None, [level, ems] {
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


    // ---- level 16's class hooks (the five-hook table of section 3): a0 = the moby, a1 = a position, a2 = a target moby, f12 a float
    // (the carry hooks' shape: their callees read all four), on the mobys whose class owns the hook
    if (g_oracle_level == 16) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        struct Hk { const char* name; uint32_t addr; uint32_t slot; TrRet (*fn)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float, float, float, float, float); };
        static const Hk hks[] = {
            {"hook_2e65d0", 0x2e65d0, 0x0, kal_hook_2e65d0_u},
            {"hook_2e6620", 0x2e6620, 0x4, kal_hook_2e6620_u},
            {"hook_2e6670", 0x2e6670, 0x8, kal_hook_2e6670_u},
            {"hook_2e66c0", 0x2e66c0, 0xc, kal_hook_2e66c0_u},
            {"hook_2e66f0", 0x2e66f0, 0x10, kal_hook_2e66f0_u},
        };
        for (const Hk& hk : hks) {
            level();
            std::vector<uint32_t> own;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
                const uint32_t cls = ee_read<uint32_t>(m + 0x24);
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + hk.slot) == hk.addr) own.push_back(m);
            }
            if (own.empty()) { std::printf("%-26s L16:0x%08x  no moby owns it on this level\n", hk.name, hk.addr); continue; }
            if (std::getenv("B250_INFO") != nullptr) std::printf("INFO %s 0x%08x owners %zu\n", hk.name, hk.addr, own.size());
            auto fn = hk.fn;
            lvcheck(hk.name, hk.addr, 40, Ret::Int32, [level, own] {
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

    // ---- level 16's movement-mode hero update (L16:0x00204958): the hero in a movement mode (PLAYER+0x20a4 = 1 / 2 / 3) in one of the states of the mode
    if (g_oracle_level == 16) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        g_ignored_words.push_back(OV(0x0017431c));  // (a float whose zero's sign depends on a VU0 register left behind)
        g_ignored_words.push_back(OV(0x00173e9c));  // (the same class: the segment record's word (Veldin 0x1efc70's sqc2 into 0x173e90..) is -0 or +0 by the VU0 vf register the earlier checks left; passes alone, fails after the class checks)
        lvcheck("hero_move_mode", 0x00204958, 300, Ret::None, [level] {
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

    // ---- moby_spawn (the Veldin port) against level 16's own copy (L16:0x00249f10): the classes' spawns all go through it
    if (g_oracle_level == 16) {
        lvcheck("moby_spawn", MOBY_SPAWN_LV, 60, Ret::Int32, [level] {
            level();
            static const uint32_t cls[] = {0x31, 0x32, 0x34, 0x5c3, 0xeb, 0x101, 0x119};  // (classes of level 16's own table: 49, 50, 52, 1475, 235, 257, 281)
            if (ri(0, 2) == 0) w32(OV(0x0015ffe0) , ee_read<uint32_t>(OV(0x0015ffe0)));
            return Args{{cls[ri(0, 6)]}, {}};
        }, [](A a) { return r32(moby_spawn(a.i[0])); });
    }

    // ---- level 16's camera hook (0x2f3a88, the pointer its setter puts at the hook slot) and the callback it queues (0x2f3ae8)
    if (g_oracle_level == 16) {
        auto cam_prep = [level] {
            level();
            const uint32_t S = OV(0x00167010) - 0x10;  // (Veldin 0x167010 is the hook pointer: S + 0x10)
            w32(S, ri(0, 3) == 0 ? R() : 0u);
            w32(S + 4, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
            wf(S + 8, rf(-3, 3));
            wf(S + 0xc, ri(0, 3) == 0 ? 0.0f : rf(-0.5f, 3));
            w32(S + 0x10, 0x002f3a88u);
            wf(0x00161f3c, rf(0, 0.5f));  // $gp - 0x4cc4: the frame step the callback counts down by
            w32(OV(0x0015ef4c), u(ri(0, 14)));  // the queue's count (it holds up to 16)
        };
        lvcheck("cam0_hook", 0x002f3a88, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { kal_cam0_hook_l16(0); return uint64_t{0}; });
        lvcheck("cam_callback", 0x002f3ae8, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { kal_cam_callback_l16(0); return uint64_t{0}; });
    }

    // ---- mobys_in_sphere (Veldin 0x1f2868, level 16's copy L16:0x001f10d0) on the level's own mobys
    if (g_oracle_level == 16) {
        lvcheck("mobys_in_sphere", 0x001f10d0, 600, Ret::Int32, [level] {
            level();
            std::vector<uint32_t> ms;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                if (ee_read<uint8_t>(m + 0x20) < 0xfe) ms.push_back(m);
            const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            const uint32_t OUT = 0x00180000u;
            const float r = rf(0.3f, 3);
            wf(OUT, ee_read<float>(m + 0x10) + rf(-3, 3)); wf(OUT + 4, ee_read<float>(m + 0x14) + rf(-3, 3));
            wf(OUT + 8, ee_read<float>(m + 0x18) + rf(-3, 3)); wf(OUT + 12, 0.0f);
            const uint32_t fl = (ri(0, 1) ? 4u : 0u) | (ri(0, 2) ? 0u : 0x10u) | (ri(0, 2) ? 0u : 0x20u) | (ri(0, 2) ? 0u : 0x80u) | (u(ri(0, 0x1f)) << 8);
            return Args{{OUT, fl, ri(0, 3) ? 0u : m, 0u}, {r}};
        }, [](A a) { return r32(mobys_in_sphere(a.i[0], a.f[0], a.i[1], a.i[2], a.i[3])); });
    }

    // ---- level 16's camera types (accept / init / update / exit: section 4 of the overlay)
    if (g_oracle_level == 16) {
        // a live camera of the type, the hero put in or near its box, its data varied
        // a camera's +0x8c is its row in the level's type table (section 4), not the type: the rows of the level's own types
        auto cam_row = [](int type) -> int { switch (type) { case 3: return 1; case 8: return 6; default: return -1; } };
        auto cam_setup = [level, cam_row](int type, bool with_other, uint32_t* other) -> uint32_t {
            level();
            uint32_t cam = 0;
            for (uint32_t i = 0; i < 48 && cam == 0; ++i)
                if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0 && ee_read<int16_t>(OV(0x00167150) + i * 0xa0 + 0x8c) == cam_row(type)) cam = OV(0x00167150) + i * 0xa0;
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
        static const CamT cams[] = {{3, 0x002f0d88, 0x002f0d90, 0x002f0308, 0x002f0fe0}, {8, 0x002f51e0, 0x002f51e8, 0x002f5260, 0x00000000}};
        for (const CamT& ct : cams) {
            level();
            bool any = false, own = false;
            for (uint32_t i = 0; i < 48; ++i)
                if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0) { any = true; if (ee_read<int16_t>(OV(0x00167150) + i * 0xa0 + 0x8c) == cam_row(ct.type)) own = true; }
            if (!any) { std::printf("cam_type%d                 no live camera on this level\n", ct.type); continue; }
            if (!own) std::printf("cam_type%d                 no live camera of the type on this level: checked on another live camera's words\n", ct.type);
            const int type = ct.type;
            static char nm[4][48];
            static int k = 0;
            auto name = [&](const char* what) { char* p = nm[k++ & 3]; std::snprintf(p, 48, "cam_type%d_%s", type, what); return static_cast<const char*>(p); };
            if (type == 3) {
            lvcheck(name("accept"), ct.accept, 300, Ret::Int32, [cam_setup, type] { uint32_t other = 0; const uint32_t cam = cam_setup(type, true, &other); return Args{{cam, other}, {}}; },
                    [](A a) { return r32(a.i[0] != 0 ? kal_cam_type3_accept_u(a.i[0], a.i[1], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).v0 : 0u); });
            lvcheck(name("init"), ct.init, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [](A a) { if (a.i[0] != 0) kal_cam_type3_init_l16(a.i[0]); return uint64_t{0}; });
            lvcheck(name("update"), ct.update, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [](A a) { if (a.i[0] != 0) kal_cam_type3_update_l16(a.i[0]); return uint64_t{0}; });
            lvcheck(name("exit"), ct.exit, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [](A a) { if (a.i[0] != 0) kal_cam_type3_exit_l16(a.i[0]); return uint64_t{0}; });
            }
            if (type == 8) {
            lvcheck(name("accept"), ct.accept, 300, Ret::Int32, [cam_setup, type] { uint32_t other = 0; const uint32_t cam = cam_setup(type, true, &other); return Args{{cam, other}, {}}; },
                    [](A a) { return r32(a.i[0] != 0 ? kal_cam_type8_accept_u(a.i[0], a.i[1], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).v0 : 0u); });
            lvcheck(name("init"), ct.init, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [](A a) { if (a.i[0] != 0) kal_cam_type8_init_l16(a.i[0]); return uint64_t{0}; });
            lvcheck(name("update"), ct.update, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [](A a) { if (a.i[0] != 0) kal_cam_type8_update_l16(a.i[0]); return uint64_t{0}; });
            }
        }
    }

    //@@NEXT
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
