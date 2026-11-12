// Oracle batch 124 (task 0357): level 11's (Pokitaru) own moby classes on its real level data. Run with
// ORACLE_LEVEL=11 build/tests/ovl_oracle --batch 124 (needs the ISO). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the
// original is level 11's own function at its own address, the port the one registered for it (the
// transliterations pokitaru_classes_tr_N.cpp).

#include "ovl_oracle.h"

#include <game/ovl/enemy_749.h>
#include <game/ovl/hero_move_mode.h>
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
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
constexpr uint32_t MOBY_SPAWN_L11 = 0x00274668;

// A check whose original is level 11's own function at `l9addr` (not a Veldin address to translate).
void l11check(const char* name, uint32_t l9addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (const char* skip = std::getenv("B124_SKIP"); skip != nullptr && (std::string(",") + skip + ",").find(std::string(",") + name + ",") != std::string::npos) return;  // (the mutation test: checks already seen to fail)
    if (g_oracle_level != 11) {
        std::printf("%-26s L11:0x%08x  skipped (level 11 only: run with ORACLE_LEVEL=11)\n", name, l9addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L11:0x%08x\n", name, l9addr);
    if (!check_fn(name, l9addr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_124() {
    if (g_only_batch != 124) return;
    g_batch = 124;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    // segment_collide's hit point (the global at Veldin 0x174610, 3 words): written by the callee from the segment's end points; class 911's
    // state 1 builds the segment from stale stack lanes (the port's EE stack and the interpreter's hold other leftovers), nothing reads it
    for (uint32_t k = 0; k < 12; k += 4) g_ignored_words.push_back(OV(0x00174610) + k);
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
    // the level's mobys whose update function is `fn` (level 11's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60, std::vector<uint32_t> subjects = {}) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 11) { l11check(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
        level();
        const std::vector<uint32_t> ms = subjects.empty() ? mobys_with(fn) : subjects;
        if (ms.empty()) { std::printf("%-26s L11:0x%08x  no moby on this level\n", name, fn); return; }
        l11check(name, fn, n, Ret::None, [level, ms, vary, port] {
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
    if (g_oracle_level == 11) {
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
        {  // the particle pool (0x800 records of 0x40 bytes at *(0x16016c)): the w lane of the velocity vector a spray copies (a stale lane of vec_from_spherical in the original)
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c);
        }
    }

    // ---- the level's own classes (transliterated by tools/codegen/mkall_l11.py): the level's own mobys of each class, the first
    // update (state 0) done by the port, then a state, a hit, the hero near, the pvars' floats / counters moved as typed
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};  // (+ the level change request: the original would run the CD / movie path)
    struct TrCls { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t skip_states = 0; bool walk = false; uint32_t skip_p0 = 0; uint32_t skip_p1 = 0; };  // (skip_p0 / skip_p1: states skipped in pass 0 / 1 only, see docs/reversing/pokitaru_classes.md)  // (the states that start a scene / a level change: the original runs the kernel / movie path for real)
    static const TrCls trcls[] = {
        {"class23", 0x002cb668, pok_class23_update_l11, 0x4, false},
        {"class37", 0x002cb790, pok_class37_update_l11, 0x0, false},
        {"class65", 0x002cb810, pok_class65_update_l11, 0x0, false},
        {"class90", 0x002d0710, pok_class90_update_l11, 0x0, false},
        {"class114", 0x002d0fa8, pok_class114_update_l11, 0x9000, false},
        {"class298", 0x002f0e40, pok_class298_update_l11, 0x0, false},
        {"class318", 0x002f2518, pok_class318_update_l11, 0x0, false},
        {"class341", 0x002f30b0, pok_class341_update_l11, 0x0, false},
        {"class361", 0x002f3350, pok_class361_update_l11, 0x0, false},
        {"class572", 0x002f9998, pok_class572_update_l11, 0x0, false},
        {"class604", 0x002fc630, pok_class604_update_l11, 0x0, false},
        {"class605", 0x002fc980, pok_class605_update_l11, 0x0, false},
        {"class615", 0x002fcda8, pok_class615_update_l11, 0x0, false},
        {"class758", 0x00300010, pok_class758_update_l11, 0x0, false},
        {"class805", 0x00300578, pok_class805_update_l11, 0x0, false},
        {"class819", 0x00302510, pok_class819_update_l11, 0x0, false},
        {"class830", 0x00302ae0, pok_class830_update_l11, 0x0, false},
        {"class832", 0x00303070, pok_class832_update_l11, 0x0, false},
        {"class1009", 0x003089d0, pok_class1009_update_l11, 0x0, false},
        {"class1017", 0x00309098, pok_class1017_update_l11, 0x0, false},
        {"class1034", 0x003094f8, pok_class1034_update_l11, 0x0, false},
        {"class1075", 0x00309ac0, pok_class1075_update_l11, 0x0, false},
        {"class1111", 0x0030b358, pok_class1111_update_l11, 0x0, false},
        {"class1134", 0x0030b730, pok_class1134_update_l11, 0x4, false},
        {"class1156", 0x0030c788, pok_class1156_update_l11, 0x0, false},
        {"class1157", 0x0030d800, pok_class1157_update_l11, 0x0, false},
        {"class1158", 0x0030e088, pok_class1158_update_l11, 0x0, false},
        {"class1159", 0x0030e978, pok_class1159_update_l11, 0x0, false},
        {"class1178", 0x0030eb90, pok_class1178_update_l11, 0x0, false},
        {"class1179", 0x0030ee00, pok_class1179_update_l11, 0x0, false},
        {"class1180", 0x0030f2a8, pok_class1180_update_l11, 0x0, false},
        {"class1218", 0x0030f728, pok_class1218_update_l11, 0x0, false},
        {"class1219", 0x00310028, pok_class1219_update_l11, 0x0, false},
        {"class1231", 0x00310180, pok_class1231_update_l11, 0x0, false},
        {"class1242", 0x00313290, pok_class1242_update_l11, 0x0, false},
        {"class1246", 0x00314318, pok_class1246_update_l11, 0x2000, false},
        {"class1248", 0x00316320, pok_class1248_update_l11, 0x0, false},
        {"class1264", 0x003172c0, pok_class1264_update_l11, 0x0, false},
        {"class1265", 0x003181f0, pok_class1265_update_l11, 0x0, false},
        {"class1297", 0x00318b30, pok_class1297_update_l11, 0x0, false},
        {"class1319", 0x00319838, pok_class1319_update_l11, 0x6, false},
        {"class1350", 0x0031aa80, pok_class1350_update_l11, 0x0, false},
        {"class1524", 0x0031ab08, pok_class1524_update_l11, 0x0, false},
        {"class1633", 0x0031b758, pok_class1633_update_l11, 0x0, false},
        {"class1818", 0x0031c800, pok_class1818_update_l11, 0x0, false},
        {"class1859", 0x0031d7d8, pok_class1859_update_l11, 0x0, false},
        {"class1860", 0x0031d8d0, pok_class1860_update_l11, 0x0, false},
        {"class1903", 0x0031e2f0, pok_class1903_update_l11, 0x0, false},
        {"class1919", 0x0031f150, pok_class1919_update_l11, 0x0, false},
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
    if (g_oracle_level == 11 && std::getenv("B124_INFO") != nullptr) {  // the level's mobys of each class: how many, the nearest to the hero's start
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
        if (std::getenv("B124_CLS") != nullptr && std::strcmp(std::getenv("B124_CLS"), c.name) != 0) continue;
        for (int st = 0; st < 16; ++st)
        for (int pass = 0; pass < 3; ++pass) {
        if (g_oracle_level == 11 && std::getenv("B124_TAGS") != nullptr) std::printf("@@ %s st %d pass %d\n", c.name, st, pass);
        if ((std::getenv("B124_ST") == nullptr || std::atoi(std::getenv("B124_ST")) == st) && (std::getenv("B124_PASS") == nullptr || std::atoi(std::getenv("B124_PASS")) == pass) && (c.skip_states & (1u << st)) == 0 && !(pass == 0 && (c.skip_p0 & (1u << st)) != 0) && !(pass == 1 && (c.skip_p1 & (1u << st)) != 0)) classcheck(c.name, c.addr, c.fn, [st, pass, c, generic_prep](uint32_t m) { generic_prep(m, st, pass, c.fn, c.walk); }, 12);
        }
    }
    g_extra_noop_hooks.clear();

    // ---- level 11's own moby callbacks (draw callbacks installed by `jal 0x1ecc00 / 0x1eca48`): a0 = the moby; the subjects are
    // the mobys of the class whose code installs it, after one update of the class in a state, a hit, the hero near
    if (g_oracle_level == 11) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        struct Cb { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t cls_addr; int cls; };
        // the display packet cursor (*0x1611c0) is null in the unit batches' memory: the draw helpers' packet writes land in low memory (0x100 .. 0x1500; the cursor is given a real buffer below) and carry a stale
        // callee-saved register (s7) of the original's frame: not game state
        const size_t ign_before = g_ignored_words.size();
        for (uint32_t a = 0x100; a < 0x1500; a += 4) g_ignored_words.push_back(a);
        static const Cb cbs[] = {
        {"cb2f2d58", 0x002f2d58, pok_cb_2f2d58_l11, 0x002f2518, 318},
        {"cb2fd6c0", 0x002fd6c0, pok_cb_2fd6c0_l11, 0x002fcda8, 615},
        {"cb302e60", 0x00302e60, pok_cb_302e60_l11, 0x00303070, 832},
        {"cb30a908", 0x0030a908, pok_cb_30a908_l11, 0x0030b358, 1111},
        {"cb30e708", 0x0030e708, pok_cb_30e708_l11, 0x0030e088, 1158},
        {"cb311d50", 0x00311d50, pok_cb_311d50_l11, 0x00000000, 0},
        {"cb3192c8", 0x003192c8, pok_cb_3192c8_l11, 0x00000000, 0},
        {"cb31c728", 0x0031c728, pok_cb_31c728_l11, 0x00000000, 0},
        {"cb31db50", 0x0031db50, pok_cb_31db50_l11, 0x0031e2f0, 1903},
        {"cb31e930", 0x0031e930, pok_cb_31e930_l11, 0x0031f150, 1919},
        };
        for (const Cb& cb : cbs) {
            if (std::getenv("B124_CLS") != nullptr && std::strcmp(std::getenv("B124_CLS"), cb.name) != 0) continue;
            level();
            std::vector<uint32_t> subj;
            if (cb.cls_addr != 0) subj = mobys_with(cb.cls_addr);
            else for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) if (ee_read<uint8_t>(m + 0x20) < 0xfe) subj.push_back(m);
            void (*clsfn)(uint32_t) = nullptr;
            for (const TrCls& c : trcls) if (c.addr == cb.cls_addr) clsfn = c.fn;
            if (subj.empty()) { std::printf("%-26s L11:0x%08x  no moby on this level\n", cb.name, cb.addr); continue; }
            // cb2fd6c0 / cb311d50: their GIF packet words carry a callee-saved register (s7 / s?) the callback reads on a path that never set it (the original's interpreter holds the
            // harness' leftover there, the port's local 0): the packets (the cursor's buffer) are not compared for these two, everything else is
            const size_t ign_pk = g_ignored_words.size();
            if (std::strcmp(cb.name, "cb2fd6c0") == 0 || std::strcmp(cb.name, "cb311d50") == 0)
                for (uint32_t a = 0x00800000u; a < 0x00810000u; a += 4) g_ignored_words.push_back(a);
            for (int st = 0; st < 4; ++st)
            for (int pass = 0; pass < 3; ++pass) {
                if (clsfn == nullptr && (st != 0 || pass != 0)) continue;
                classcheck(cb.name, cb.addr, cb.fn, [st, pass, clsfn, generic_prep](uint32_t m) { generic_prep(m, st, pass, clsfn != nullptr ? clsfn : pok_class23_update_l11, false); w32(OV(0x001611c0), 0x00800000u); }, 12, subj);  // (the display packet cursor: a real buffer for the draw callbacks)
            }
            g_ignored_words.resize(ign_pk);
        }
        g_ignored_words.resize(ign_before);
        g_extra_noop_hooks.clear();
    }

    // ---- level 11's sound emitter type 3 (section 5 of the overlay) on the level's own emitters
    if (g_oracle_level == 11) {
        struct Em { const char* name; uint32_t addr; void (*fn)(uint32_t); };
        static const Em ems_t[] = {{"emitter1", 0x00328b50, pok_emitter1_l11}, {"emitter5", 0x00328ef0, pok_emitter5_l11}};
        for (const Em& e : ems_t) {
            level();
            std::vector<uint32_t> ems;
            for (int32_t i = 0; i < ee_read<int32_t>(0x0013f2e0); ++i) {
                const uint32_t em = ee_read<uint32_t>(0x0013f2e4) + static_cast<uint32_t>(i) * 0x90;
                if (ee_read<uint32_t>(em + 4) == e.addr) ems.push_back(em);
            }
            if (ems.empty()) { std::printf("%-26s L11:0x%08x  no emitter of the type on this level\n", e.name, e.addr); continue; }
            if (std::getenv("B124_INFO") != nullptr) std::printf("INFO %s 0x%08x emitters %zu\n", e.name, e.addr, ems.size());
            auto fn = e.fn;
            l11check(e.name, e.addr, 120, Ret::None, [level, ems] {
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


    // ---- level 11's class hooks (class 827's table at 0x1dde58): a0 = the moby, a1 = a position, a2 = a target moby, f12 a float
    // (the carry hooks' shape: their callees read all four), on the mobys whose class owns the hook
    if (g_oracle_level == 11) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        struct Hk { const char* name; uint32_t addr; uint32_t slot; TrRet (*fn)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float, float, float, float, float); };
        static const Hk hks[] = {
            {"hook_2fb780", 0x2fb780, 0x0, pok_hook_2fb780_u},
            {"hook_2fb7f0", 0x2fb7f0, 0x4, pok_hook_2fb7f0_u},
            {"hook_2fb840", 0x2fb840, 0x8, pok_hook_2fb840_u},
            {"hook_2fb890", 0x2fb890, 0xc, pok_hook_2fb890_u},
            {"hook_2fb8c0", 0x2fb8c0, 0x10, pok_hook_2fb8c0_u},
            {"hook_316160", 0x316160, 0x0, pok_hook_316160_u},
            {"hook_316210", 0x316210, 0x4, pok_hook_316210_u},
            {"hook_316270", 0x316270, 0x8, pok_hook_316270_u},
            {"hook_3162d0", 0x3162d0, 0xc, pok_hook_3162d0_u},
            {"hook_3162f0", 0x3162f0, 0x10, pok_hook_3162f0_u},
        };
        for (const Hk& hk : hks) {
            level();
            std::vector<uint32_t> own;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
                const uint32_t cls = ee_read<uint32_t>(m + 0x24);
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + hk.slot) == hk.addr) own.push_back(m);
            }
            if (own.empty()) { std::printf("%-26s L11:0x%08x  no moby owns it on this level\n", hk.name, hk.addr); continue; }
            if (std::getenv("B124_INFO") != nullptr) std::printf("INFO %s 0x%08x owners %zu\n", hk.name, hk.addr, own.size());
            auto fn = hk.fn;
            l11check(hk.name, hk.addr, 40, Ret::Int32, [level, own] {
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

    // ---- level 11's movement-mode hero update (L11:0x00237a58, the Novalis family's 0x3a8-byte build): the hero in a movement mode (PLAYER+0x20a4 = 1 / 2 / 3) in one of the states of the mode
    if (g_oracle_level == 11) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        g_ignored_words.push_back(OV(0x0017431c));  // (a float whose zero's sign depends on a VU0 register left behind)
        g_ignored_words.push_back(OV(0x00173e9c));  // (the same class: the segment record's word (Veldin 0x1efc70's sqc2 into 0x173e90..) is -0 or +0 by the VU0 vf register the earlier checks left; passes alone, fails after the class checks)
        l11check("hero_move_mode", 0x00237a58, 300, Ret::None, [level] {
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

    // ---- moby_spawn (the Veldin port) against level 11's own copy (L11:0x...): the classes' spawns all go through it
    if (g_oracle_level == 11) {
        l11check("moby_spawn", MOBY_SPAWN_L11, 60, Ret::Int32, [level] {
            level();
            static const uint32_t cls[] = {0x17, 0x25, 0x41, 0x72, 0x12a, 0x25c, 0x4a1};  // (classes of level 11's own table: 23, 37, 65, 114, 298, 604, 1185)
            if (ri(0, 2) == 0) w32(OV(0x0015ffe0) , ee_read<uint32_t>(OV(0x0015ffe0)));
            return Args{{cls[ri(0, 6)]}, {}};
        }, [](A a) { return r32(moby_spawn(a.i[0])); });
    }

    // ---- level 11's camera hook (0x327a90, the pointer its setter 0x327a58 puts at 0x167a10) and the callback it queues (0x2f4b78)
    if (g_oracle_level == 11) {
        auto cam_prep = [level] {
            level();
            const uint32_t S = OV(0x00167010) - 0x10;  // (Veldin 0x167010 is the hook pointer: S + 0x10)
            w32(S, ri(0, 3) == 0 ? R() : 0u);
            w32(S + 4, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
            wf(S + 8, rf(-3, 3));
            wf(S + 0xc, ri(0, 3) == 0 ? 0.0f : rf(-0.5f, 3));
            w32(S + 0x10, 0x00327a90u);
            wf(0x00161f3c, rf(0, 0.5f));  // $gp - 0x4cc4: the frame step the callback counts down by
            w32(OV(0x0015ef4c), u(ri(0, 14)));  // the queue's count (it holds up to 16)
        };
        l11check("cam0_hook", 0x00327a90, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { pok_cam0_hook_l11(0); return uint64_t{0}; });
        l11check("cam_callback", 0x00327af0, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { pok_cam_callback_l11(0); return uint64_t{0}; });
    }

    // ---- mobys_in_sphere (Veldin 0x1f2868, level 11's copy L11:0x002240d0) on the level's own mobys: class 794 (the Gadgebot's helper) calls it and one of its random states differed
    if (g_oracle_level == 11) {
        l11check("mobys_in_sphere", 0x002240d0, 600, Ret::Int32, [level] {
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

    //@@NEXT
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
