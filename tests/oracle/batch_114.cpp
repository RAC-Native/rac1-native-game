// Oracle batch 114 (task 0340): Rilgar's own moby classes (level 5) on its real level data. Run with
// ORACLE_LEVEL=5 build/tests/ovl_oracle --batch 114 (needs the ISO). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the
// original is Rilgar's own function at its own address, the port the one registered for it (the
// transliterations eudora_classes_tr_N.cpp, the hand ports eudora_*.cpp).

#include "ovl_oracle.h"

#include <game/ovl/enemy_749.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sound_emitter.h>
#include <runtime/kernel/disc.h>

#include <cmath>
#include <cstring>
#include <string>
#include <map>

namespace rac1::oracle {

namespace {
// A check whose original is Rilgar's own function at `l5addr` (not a Veldin address to translate).
void l5check(const char* name, uint32_t l5addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (const char* skip = std::getenv("B114_SKIP"); skip != nullptr && (std::string(",") + skip + ",").find(std::string(",") + name + ",") != std::string::npos) return;  // (the mutation test: checks already seen to fail)
    if (g_oracle_level != 5) {
        std::printf("%-26s L5:0x%08x  skipped (Rilgar only: run with ORACLE_LEVEL=5)\n", name, l5addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L5:0x%08x\n", name, l5addr);
    if (!check_fn(name, l5addr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_114() {
    if (g_only_batch != 114) return;
    g_batch = 114;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
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
    // the level's mobys whose update function is `fn` (Rilgar's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60, std::vector<uint32_t> subjects = {}) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 5) { l5check(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
        level();
        const std::vector<uint32_t> ms = subjects.empty() ? mobys_with(fn) : subjects;
        if (ms.empty()) { std::printf("%-26s L5:0x%08x  no moby on this level\n", name, fn); return; }
        l5check(name, fn, n, Ret::None, [level, ms, vary, port] {
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
                try { port(m); } catch (const std::exception&) { ok = false; }
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
    if (g_oracle_level == 5) {
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
        {  // the particle pool (0x800 records of 0x40 bytes at *(0x16016c)): the w lane of the velocity vector a spray copies (a stale lane of vec_from_spherical in the original)
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c);
        }
    }

    // ---- the level's own classes (transliterated by tools/codegen/generate_all.py): the level's own mobys of each class, the first
    // update (state 0) done by the port, then a state, a hit, the hero near, the pvars' floats / counters moved as typed
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x002763f8};  // (+ the level change request: the original would run the CD / movie path)
    struct TrCls { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t skip_states = 0; bool walk = false; };  // (the states that start a scene / a level change: the original runs the kernel / movie path for real)
    static const TrCls trcls[] = {
        {"class35", 0x002d1688, ril_class35_update_l5, 0x0, false},
        {"class79", 0x002d7140, ril_class79_update_l5, 0x0, false},
        {"class133", 0x002dac80, ril_class133_update_l5, 0x0, false},
        {"class258", 0x002f5200, ril_class258_update_l5, 0x0, false},
        {"class341", 0x002f8080, ril_class341_update_l5, 0x0, false},
        {"class439", 0x002f87a8, ril_class439_update_l5, 0x0, false},
        {"class572", 0x002fecf0, ril_class572_update_l5, 0x0, false},
        {"class604", 0x00301988, ril_class604_update_l5, 0x0, false},
        {"class605", 0x00301cd8, ril_class605_update_l5, 0x0, false},
        {"class623", 0x00301f48, ril_class623_update_l5, 0x0, false},
        {"class625", 0x00304320, ril_class625_update_l5, 0x0, false},
        {"class679", 0x003067b8, ril_class679_update_l5, 0x0, false},
        {"class717", 0x00307910, ril_class717_update_l5, 0x0, false},
        {"class758", 0x003098d8, ril_class758_update_l5, 0x0, false},
        {"class805", 0x00309e40, ril_class805_update_l5, 0x0, false},
        {"class810", 0x0030bdd8, ril_class810_update_l5, 0x0, false},
        {"class812", 0x0030bf98, ril_class812_update_l5, 0x0, false},
        {"class823", 0x0030c0a8, ril_class823_update_l5, 0x0, false},
        {"class830", 0x0030c5b0, ril_class830_update_l5, 0x0, false},
        {"class831", 0x0030ca80, ril_class831_update_l5, 0x0, false},
        {"class832", 0x0030dac8, ril_class832_update_l5, 0x0, false},
        {"class838", 0x0030dc68, ril_class838_update_l5, 0x0, false},
        {"class841", 0x0030e1f8, ril_class841_update_l5, 0x0, false},
        {"class843", 0x0030e508, ril_class843_update_l5, 0x0, false},
        {"class844", 0x0030e7f8, ril_class844_update_l5, 0x0, false},
        {"class846", 0x0030f5c8, ril_class846_update_l5, 0x0, false},
        {"class852", 0x00314eb0, ril_class852_update_l5, 0x0, false},
        {"class855", 0x003150f0, ril_class855_update_l5, 0x0, false},
        {"class877", 0x003156d0, ril_class877_update_l5, 0x0, false},
        {"class879", 0x00316110, ril_class879_update_l5, 0x0, false},
        {"class893", 0x00316258, ril_class893_update_l5, 0x0, false},
        {"class895", 0x003166a0, ril_class895_update_l5, 0x0, false},
        {"class918", 0x00316ab8, ril_class918_update_l5, 0x0, false},
        {"class919", 0x00317470, ril_class919_update_l5, 0x0, false},
        {"class920", 0x00317aa0, ril_class920_update_l5, 0x0, false},
        {"class925", 0x003180a0, ril_class925_update_l5, 0x0, false},
        {"class980", 0x003186f8, ril_class980_update_l5, 0x0, false},
        {"class982", 0x00318a68, ril_class982_update_l5, 0x0, false},
        {"class984", 0x00318b98, ril_class984_update_l5, 0x0, false},
        {"class998", 0x00318c78, ril_class998_update_l5, 0x78, false},
        {"class1099", 0x00319c28, ril_class1099_update_l5, 0x0, false},
        {"class1134", 0x00319c60, ril_class1134_update_l5, 0x6, false},
        {"class1139", 0x0031abe0, ril_class1139_update_l5, 0x0, false},
        {"class1347", 0x0031bf40, ril_class1347_update_l5, 0x0, false},
        {"class1511", 0x0031c8e0, ril_class1511_update_l5, 0x0, false},
        {"class1550", 0x0031d5c8, ril_class1550_update_l5, 0x0, false},
        {"class1633", 0x0031d760, ril_class1633_update_l5, 0x0, false},
        {"class1818", 0x0031e808, ril_class1818_update_l5, 0x0, false},
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
    if (g_oracle_level == 5 && std::getenv("B114_INFO") != nullptr) {  // the level's mobys of each class: how many, the nearest to the hero's start
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
        if (std::getenv("B114_CLS") != nullptr && std::strcmp(std::getenv("B114_CLS"), c.name) != 0) continue;
        for (int st = 0; st < 16; ++st)
        for (int pass = 0; pass < 3; ++pass) {
        if (g_oracle_level == 5 && std::getenv("B114_TAGS") != nullptr) std::printf("@@ %s st %d pass %d\n", c.name, st, pass);
        if ((std::getenv("B114_ST") == nullptr || std::atoi(std::getenv("B114_ST")) == st) && (std::getenv("B114_PASS") == nullptr || std::atoi(std::getenv("B114_PASS")) == pass) && (c.skip_states & (1u << st)) == 0) classcheck(c.name, c.addr, c.fn, [st, pass, c, generic_prep](uint32_t m) { generic_prep(m, st, pass, c.fn, c.walk); }, 12);
        }
    }
    g_extra_noop_hooks.clear();

    if (g_oracle_level == 5 && std::getenv("B114_INFO") != nullptr) {  // the classes with an empty update: how many placements
        level();
        for (const uint32_t fn : {0x0031ace8u, 0x0031d158u}) std::printf("INFO empty 0x%08x instances %zu\n", fn, mobys_with(fn).size());
    }

    // ---- the callbacks and class hooks Rilgar's classes install (a0 = the moby): checked on the mobys that own them (the class' update
    // function installs the callback; a hook is in the class' hook table at +0x2c)
    if (g_oracle_level == 5) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x002763f8};
        struct Cb { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t owner_update; uint32_t hook_slot; };
        static const Cb cbs[] = {
            {"cb_30d678", 0x0030d678, ril_cb_30d678_l5, 0x0030ca80, 0},  // class 831's callback
            {"cb_30d8b8", 0x0030d8b8, ril_cb_30d8b8_l5, 0x0030dac8, 0},  // class 832's callback
            // (the class hook +8, 0x300b98, takes (moby, a position pointer, a target moby, a float): not checked here, unproved)
            {"hook_c", 0x00300be8, ril_cb_300be8_l5, 0, 0xc},             // the class hook +0xc
        };
        for (const Cb& cb : cbs) {
            level();
            std::vector<uint32_t> subj;
            if (cb.owner_update != 0) subj = mobys_with(cb.owner_update);
            else
                for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
                    const uint32_t cls = ee_read<uint32_t>(m + 0x24);
                    if (ee_read<uint8_t>(m + 0x20) >= 0xfe || cls == 0 || ee_read<uint32_t>(cls + 0x2c) == 0) continue;
                    if (ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + cb.hook_slot) == cb.addr) subj.push_back(m);
                }
            if (subj.empty()) { std::printf("%-26s L5:0x%08x  no moby owns it on this level\n", cb.name, cb.addr); continue; }
            if (std::getenv("B114_INFO") != nullptr) std::printf("INFO %s 0x%08x owners %zu\n", cb.name, cb.addr, subj.size());
            const Cb c = cb;
            for (int pass = 0; pass < 3; ++pass)
                classcheck(cb.name, cb.addr, cb.fn, [c, pass, generic_prep](uint32_t m) { generic_prep(m, 0, pass, ril_class831_update_l5); }, 20, subj);
        }
        g_extra_noop_hooks.clear();
    }

    // ---- Rilgar's sound emitter types (1, 3, 5, 6: section 5 of the overlay) on the level's own emitters
    if (g_oracle_level == 5) {
        struct Em { const char* name; uint32_t addr; void (*fn)(uint32_t); };
        static const Em ems_t[] = {
            {"emitter1", 0x0032b478, emitter_fade_box_update}, {"emitter3", 0x0032ba68, emitter_reverb_box_update},
            {"emitter5", 0x0032bbc8, emitter_camera_bed_update}, {"emitter6", 0x0032bc78, emitter_music_box_update},
        };
        for (const Em& e : ems_t) {
            level();
            std::vector<uint32_t> ems;
            for (int32_t i = 0; i < ee_read<int32_t>(0x0013f2e0); ++i) {
                const uint32_t em = ee_read<uint32_t>(0x0013f2e4) + static_cast<uint32_t>(i) * 0x90;
                if (ee_read<uint32_t>(em + 4) == e.addr) ems.push_back(em);
            }
            if (ems.empty()) { std::printf("%-26s L5:0x%08x  no emitter of the type on this level\n", e.name, e.addr); continue; }
            if (std::getenv("B114_INFO") != nullptr) std::printf("INFO %s 0x%08x emitters %zu\n", e.name, e.addr, ems.size());
            auto fn = e.fn;
            l5check(e.name, e.addr, 120, Ret::None, [level, ems] {
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

    // ---- Rilgar's camera types 3 and 8 (accept / init / update / exit: section 4 of the overlay)
    if (g_oracle_level == 5) {
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
        static const CamT cams[] = {{3, 0x00326280, 0x00326288, 0x00325800, 0x003264d8}, {8, 0x0032a6d8, 0x0032a6e0, 0x0032a758, 0x0032a788}};
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
            l5check(name("accept"), ct.accept, 300, Ret::Int32, [cam_setup, type] { uint32_t other = 0; const uint32_t cam = cam_setup(type, true, &other); return Args{{cam, other}, {}}; },
                    [type](A a) { return r32(a.i[0] != 0 ? (type == 3 ? ril_cam_type3_accept_u : ril_cam_type8_accept_u)(a.i[0], a.i[1], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).v0 : 0u); });
            l5check(name("init"), ct.init, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) (type == 3 ? ril_cam_type3_init_l5 : ril_cam_type8_init_l5)(a.i[0]); return uint64_t{0}; });
            l5check(name("update"), ct.update, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) (type == 3 ? ril_cam_type3_update_l5 : ril_cam_type8_update_l5)(a.i[0]); return uint64_t{0}; });
            l5check(name("exit"), ct.exit, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) (type == 3 ? ril_cam_type3_exit_l5 : ril_cam_type8_exit_l5)(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- Rilgar's camera hook (0x328f80, the pointer its setter 0x328f48 puts at 0x167410) and the callback it queues (0x328fe0)
    if (g_oracle_level == 5) {
        auto cam_prep = [level] {
            level();
            const uint32_t S = 0x00167400;
            w32(S, ri(0, 3) == 0 ? R() : 0u);
            w32(S + 4, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
            wf(S + 8, rf(-3, 3));
            wf(S + 0xc, ri(0, 3) == 0 ? 0.0f : rf(-0.5f, 3));
            w32(S + 0x10, 0x00328f80u);
            if (ri(0, 3) == 0) w32(OV(0x0015ef4c), u(ri(0, 15)));  // the queue's count (it holds up to 16)
            wf(0x001621a4, rf(0, 0.5f));  // $gp - 0x4a5c: the frame step the callback counts down by
            w32(OV(0x0015ef4c), u(ri(0, 14)));
        };
        l5check("cam0_hook", 0x00328f80, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { ril_cam0_hook_l5(0); return uint64_t{0}; });
        l5check("cam_callback", 0x00328fe0, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { ril_cam_callback_l5(0); return uint64_t{0}; });
    }

    // ---- Rilgar's class hooks: +4 (0x300b48), +0x10 (0x300c18: pvars + 0xc0; the matcher pairs it with class 749's 0x2d5800), and the dispatcher on it
    if (g_oracle_level == 5) {
        l5check("hook_10", 0x00300c18, 60, Ret::Int32, [level] {
            level();
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
            const uint32_t m = ri(0, 4) == 0 ? 0u : first + 0x100 * u(ri(0, 100));
            if (m != 0 && ri(0, 3) == 0) w32(m + 0x78, 0);
            return Args{{m}, {}};
        }, [](A a) { return r32(ril_hook_10_u(a.i[0], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).v0); });
    }
    if (g_oracle_level == 5) {
        level();
        std::vector<uint32_t> hm;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
            const uint32_t cls = ee_read<uint32_t>(m + 0x24);
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + 0x10) != 0) hm.push_back(m);
        }
        if (hm.empty()) std::printf("class_hook_10              no moby with a +0x10 hook on this level\n");
        else
            l5check("class_hook_10", ovl_code_lookup(0x002db890), 120, Ret::Int32, [level, hm] {
                level();
                const uint32_t m = hm[static_cast<size_t>(ri(0, static_cast<int>(hm.size()) - 1))];
                if (ri(0, 6) == 0) w8(m + 0x20, coin() ? 0xfe : 0xfd);
                if (ri(0, 6) == 0) w32(m + 0x78, 0);
                return Args{{m}, {}};
            }, [](A a) { return r32(class_hook_10(a.i[0])); });
    }

    //@@NEXT
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
