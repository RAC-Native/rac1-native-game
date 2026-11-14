// Oracle batch 122 (task 0357): level 10's (Orxon) own moby classes on its real level data. Run with
// ORACLE_LEVEL=10 build/tests/ovl_oracle --batch 122 (needs the ISO). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the
// original is level 10's own function at its own address, the port the one registered for it (the
// transliterations orxon_classes_tr_N.cpp).

#include "ovl_oracle.h"

#include <game/ovl/helmet_tr.h>
#include <game/lib/libc.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/orxon_mobys.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/empty_class_updates.h>
#include <runtime/kernel/disc.h>

#include <cmath>
#include <cstring>
#include <string>
#include <map>

namespace rac1::oracle {

namespace {
constexpr uint32_t MOBY_SPAWN_L10 = 0x002412b0;

// A check whose original is level 10's own function at `l9addr` (not a Veldin address to translate).
void l10check(const char* name, uint32_t l9addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (const char* skip = std::getenv("B122_SKIP"); skip != nullptr && (std::string(",") + skip + ",").find(std::string(",") + name + ",") != std::string::npos) return;  // (the mutation test: checks already seen to fail)
    if (g_oracle_level != 10) {
        std::printf("%-26s L10:0x%08x  skipped (level 10 only: run with ORACLE_LEVEL=10)\n", name, l9addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L10:0x%08x\n", name, l9addr);
    if (!check_fn(name, l9addr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_122() {
    if (g_only_batch != 122) return;
    g_batch = 122;
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
    // the level's mobys whose update function is `fn` (level 10's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60, std::vector<uint32_t> subjects = {}) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 10) { l10check(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
        level();
        const std::vector<uint32_t> ms = subjects.empty() ? mobys_with(fn) : subjects;
        if (ms.empty()) { std::printf("%-26s L10:0x%08x  no moby on this level\n", name, fn); return; }
        l10check(name, fn, n, Ret::None, [level, ms, vary, port] {
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
    if (g_oracle_level == 10) {
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
        {  // the particle pool (0x800 records of 0x40 bytes at *(0x16016c)): the w lane of the velocity vector a spray copies (a stale lane of vec_from_spherical in the original)
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c);
        }
    }

    // ---- the level's own classes (transliterated by tools/codegen/mkall_l10.py): the level's own mobys of each class, the first
    // update (state 0) done by the port, then a state, a hit, the hero near, the pvars' floats / counters moved as typed
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x0023fea8};  // (+ the level change request: the original would run the CD / movie path)
    struct TrCls { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t skip_states = 0; bool walk = false; uint32_t skip_p0 = 0; uint32_t skip_p1 = 0; };  // (skip_p0 / skip_p1: states skipped in pass 0 / 1 only, see docs/reversing/orxon_classes.md)  // (the states that start a scene / a level change: the original runs the kernel / movie path for real)
    static const TrCls trcls[] = {
        {"class1033", 0x00295a38, orx_class1033_update_l10, 0x0, false},
        {"class1117", 0x00295c20, orx_class1117_update_l10, 0x0, false},
        {"class18", 0x00298668, orx_class18_update_l10, 0x4, false},
        {"class22", 0x00298b68, orx_class22_update_l10, 0x0, false},
        {"class87", 0x0029e1c8, empty_update, 0x0, false},
        {"class258", 0x002bae38, orx_class258_update_l10, 0x0, false},
        {"class302", 0x002bd0e8, orx_class302_update_l10, 0x0, false},
        {"class303", 0x002bd768, orx_class303_update_l10, 0x0, false},
        {"class351", 0x002be858, orx_class351_update_l10, 0x0, false},
        {"class353", 0x002befe8, orx_class353_update_l10, 0x0, false},
        {"class532", 0x002c4bc8, empty_update, 0x0, false},
        {"class605", 0x002c6470, orx_class605_update_l10, 0x0, false},
        {"class648", 0x002c74b8, orx_class648_update_l10, 0x0, false},
        {"class702", 0x002c7a20, orx_class702_update_l10, 0x0, false},
        {"class737", 0x002c7cf8, orx_class737_update_l10, 0x0, false},
        {"class758", 0x002c9590, orx_class758_update_l10, 0x0, false},
        {"class794", 0x002c9c70, orx_class794_update_l10, 0x0, false},
        {"class805", 0x002ca220, orx_class805_update_l10, 0x0, false},
        {"class819", 0x002cc2e8, orx_class819_update_l10, 0x0, false},
        {"class827", 0x002cc8b8, orx_class827_update_l10, 0x0, true, 0x10, 0x0},
        {"class832", 0x002cea78, orx_class832_update_l10, 0x0, false},
        {"class857", 0x002d4078, orx_class857_update_l10, 0x0, false},
        {"class859", 0x002d7b80, orx_class859_update_l10, 0x0, false},
        {"class915", 0x002d7fe8, orx_class915_update_l10, 0x0, false},
        {"class938", 0x002d85c8, orx_class938_update_l10, 0x0, false},
        {"class939", 0x002d8c00, orx_class939_update_l10, 0x0, false},
        {"class947", 0x002d8d30, orx_class947_update_l10, 0x0, false},
        {"class1015", 0x002d90a8, orx_class1015_update_l10, 0x0, false},
        {"class1031", 0x002d92b8, orx_class1031_update_l10, 0x0, false},
        {"class1038", 0x002d9550, orx_class1038_update_l10, 0x0, false},
        {"class1040", 0x002d9bf8, orx_class1040_update_l10, 0x0, false},
        {"class1047", 0x002d9eb8, orx_class1047_update_l10, 0x0, false},
        {"class1067", 0x002da2c8, orx_class1067_update_l10, 0x0, false},
        {"class1068", 0x002dabe8, orx_class1068_update_l10, 0x0, false},
        {"class1073", 0x002dcc58, orx_class1073_update_l10, 0x0, false},
        {"class1085", 0x002dd010, orx_class1085_update_l10, 0x0, false},
        {"class1090", 0x002dd3d8, orx_class1090_update_l10, 0x0, false},
        {"class1100", 0x002dd650, orx_class1100_update_l10, 0x0, false},
        {"class1122", 0x002dd8d8, empty_update, 0x0, false},
        {"class1134", 0x002dd8e0, orx_class1134_update_l10, 0x4, false},
        {"class1141", 0x002de860, orx_class1141_update_l10, 0x0, false},
        {"class1196", 0x002df270, orx_class1196_update_l10, 0x0, false},
        {"class1199", 0x002e01a8, orx_class1199_update_l10, 0x0, false},
        {"class1202", 0x002e1d38, orx_class1202_update_l10, 0x0, false},
        {"class1229", 0x002e4a88, orx_class1229_update_l10, 0x0, false},
        {"class1240", 0x002e5be0, orx_class1240_update_l10, 0x0, false},
        {"class1258", 0x002e6a78, orx_class1258_update_l10, 0x0, false},
        {"class1302", 0x002e7cd8, orx_class1302_update_l10, 0x0, false},
        {"class1326", 0x002e8358, orx_class1326_update_l10, 0x2, false},
        {"class1344", 0x002e85b8, orx_class1344_update_l10, 0x0, false},
        {"class1346", 0x002e8ab0, orx_class1346_update_l10, 0x0, false},
        {"class1378", 0x002e9028, orx_class1378_update_l10, 0x0, false},
        {"class1421", 0x002e9648, orx_class1421_update_l10, 0x0, false},
        {"class1424", 0x002e9990, orx_class1424_update_l10, 0x0, false},
        {"class1544", 0x002ea1f0, orx_class1544_update_l10, 0x0, false},
        {"class1555", 0x002eacd8, orx_class1555_update_l10, 0x0, false},
        {"class1806", 0x002eb1b0, orx_class1806_update_l10, 0x0, false},
        {"class1855", 0x002eb6f0, orx_class1855_update_l10, 0x0, false},
        {"class1856", 0x002eb7e0, orx_class1856_update_l10, 0x0, false},
        {"class1857", 0x002eb8b8, empty_update, 0x0, false},
        {"class1921", 0x002eb950, empty_update, 0x0, false},
        {"class1973", 0x002ebb08, orx_class1973_update_l10, 0x0, false},
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
    if (g_oracle_level == 10 && std::getenv("B122_INFO") != nullptr) {  // the level's mobys of each class: how many, the nearest to the hero's start
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
        if (std::getenv("B122_CLS") != nullptr && std::strcmp(std::getenv("B122_CLS"), c.name) != 0) continue;
        for (int st = 0; st < 16; ++st)
        for (int pass = 0; pass < 3; ++pass) {
        if (g_oracle_level == 10 && std::getenv("B122_TAGS") != nullptr) std::printf("@@ %s st %d pass %d\n", c.name, st, pass);
        if ((std::getenv("B122_ST") == nullptr || std::atoi(std::getenv("B122_ST")) == st) && (std::getenv("B122_PASS") == nullptr || std::atoi(std::getenv("B122_PASS")) == pass) && (c.skip_states & (1u << st)) == 0 && !(pass == 0 && (c.skip_p0 & (1u << st)) != 0) && !(pass == 1 && (c.skip_p1 & (1u << st)) != 0)) classcheck(c.name, c.addr, c.fn, [st, pass, c, generic_prep](uint32_t m) { generic_prep(m, st, pass, c.fn, c.walk); }, 12);
        }
    }
    g_extra_noop_hooks.clear();

    // ---- level 10's own moby callbacks (draw callbacks installed by `jal 0x1ecc00 / 0x1eca48`): a0 = the moby; the subjects are
    // the mobys of the class whose code installs it, after one update of the class in a state, a hit, the hero near
    if (g_oracle_level == 10) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x0023fea8};
        struct Cb { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t cls_addr; int cls; };
        static const Cb cbs[] = {
        {"cb2bd280", 0x002bd280, orx_cb_2bd280_l10, 0x002bd0e8, 302},
        {"cb2ce868", 0x002ce868, orx_cb_2ce868_l10, 0x002cea78, 832},
        {"cb2d7808", 0x002d7808, orx_cb_2d7808_l10, 0x002d4078, 857},
        {"cb2da690", 0x002da690, orx_cb_2da690_l10, 0x002da2c8, 1067},
        {"cb2dc698", 0x002dc698, orx_cb_2dc698_l10, 0x002dabe8, 1068},
        {"cb2e7520", 0x002e7520, orx_cb_2e7520_l10, 0x002e7cd8, 1302},
        {"cb2e91b8", 0x002e91b8, orx_cb_2e91b8_l10, 0x002e9028, 1378},
        {"cb2eb958", 0x002eb958, orx_cb_2eb958_l10, 0x002ebb08, 1973},
        };
        for (const Cb& cb : cbs) {
            if (std::getenv("B122_CLS") != nullptr && std::strcmp(std::getenv("B122_CLS"), cb.name) != 0) continue;
            level();
            std::vector<uint32_t> subj;
            if (cb.cls_addr != 0) subj = mobys_with(cb.cls_addr);
            else for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) if (ee_read<uint8_t>(m + 0x20) < 0xfe) subj.push_back(m);
            void (*clsfn)(uint32_t) = nullptr;
            for (const TrCls& c : trcls) if (c.addr == cb.cls_addr) clsfn = c.fn;
            if (subj.empty()) { std::printf("%-26s L10:0x%08x  no moby on this level\n", cb.name, cb.addr); continue; }
            for (int st = 0; st < 4; ++st)
            for (int pass = 0; pass < 3; ++pass) {
                if (clsfn == nullptr && (st != 0 || pass != 0)) continue;
                classcheck(cb.name, cb.addr, cb.fn, [st, pass, clsfn, generic_prep](uint32_t m) { generic_prep(m, st, pass, clsfn != nullptr ? clsfn : orx_class1033_update_l10, false); }, 12, subj);
            }
        }
        g_extra_noop_hooks.clear();
    }

    // ---- level 10's sound emitter type 3 (section 5 of the overlay) on the level's own emitters
    if (g_oracle_level == 10) {
        struct Em { const char* name; uint32_t addr; void (*fn)(uint32_t); };
        static const Em ems_t[] = {{"emitter3", 0x002f6e58, emitter_reverb_box_update}};
        for (const Em& e : ems_t) {
            level();
            std::vector<uint32_t> ems;
            for (int32_t i = 0; i < ee_read<int32_t>(0x0013f2e0); ++i) {
                const uint32_t em = ee_read<uint32_t>(0x0013f2e4) + static_cast<uint32_t>(i) * 0x90;
                if (ee_read<uint32_t>(em + 4) == e.addr) ems.push_back(em);
            }
            if (ems.empty()) { std::printf("%-26s L10:0x%08x  no emitter of the type on this level\n", e.name, e.addr); continue; }
            if (std::getenv("B122_INFO") != nullptr) std::printf("INFO %s 0x%08x emitters %zu\n", e.name, e.addr, ems.size());
            auto fn = e.fn;
            l10check(e.name, e.addr, 120, Ret::None, [level, ems] {
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


    // ---- level 10's class hooks (class 827's table at 0x1dde58): a0 = the moby, a1 = a position, a2 = a target moby, f12 a float
    // (the carry hooks' shape: their callees read all four), on the mobys whose class owns the hook
    if (g_oracle_level == 10) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x0023fea8};
        struct Hk { const char* name; uint32_t addr; uint32_t slot; TrRet (*fn)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float, float, float, float, float); };
        static const Hk hks[] = {
            {"hook_2ce618", 0x2ce618, 0x0, orx_hook_2ce618_u},
            {"hook_2ce6e0", 0x2ce6e0, 0x4, orx_hook_2ce6e0_u},
            {"hook_2ce770", 0x2ce770, 0x8, orx_hook_2ce770_u},
            {"hook_2ce800", 0x2ce800, 0xc, orx_hook_2ce800_u},
            {"hook_2ce838", 0x2ce838, 0x10, orx_hook_2ce838_u},
        };
        for (const Hk& hk : hks) {
            level();
            std::vector<uint32_t> own;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
                const uint32_t cls = ee_read<uint32_t>(m + 0x24);
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + hk.slot) == hk.addr) own.push_back(m);
            }
            if (own.empty()) { std::printf("%-26s L10:0x%08x  no moby owns it on this level\n", hk.name, hk.addr); continue; }
            if (std::getenv("B122_INFO") != nullptr) std::printf("INFO %s 0x%08x owners %zu\n", hk.name, hk.addr, own.size());
            auto fn = hk.fn;
            l10check(hk.name, hk.addr, 40, Ret::Int32, [level, own] {
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

    // ---- moby_spawn (the Veldin port) against level 10's own copy (L10:0x...): the classes' spawns all go through it
    if (g_oracle_level == 10) {
        l10check("moby_spawn", MOBY_SPAWN_L10, 60, Ret::Int32, [level] {
            level();
            static const uint32_t cls[] = {0x12, 0x16, 0x102, 0x25d, 0x359, 0x42c, 0x4cd};  // (classes of level 10's own table: 18, 22, 258, 605, 857, 1068, 1229)
            if (ri(0, 2) == 0) w32(OV(0x0015ffe0) , ee_read<uint32_t>(OV(0x0015ffe0)));
            return Args{{cls[ri(0, 6)]}, {}};
        }, [](A a) { return r32(moby_spawn(a.i[0])); });
    }

    // ---- level 10's camera hook (0x2f4b18, the pointer its setter 0x2f4ae0 puts at 0x167490) and the callback it queues (0x2f4b78)
    if (g_oracle_level == 10) {
        auto cam_prep = [level] {
            level();
            const uint32_t S = OV(0x00167010) - 0x10;  // (Veldin 0x167010 is the hook pointer: S + 0x10)
            w32(S, ri(0, 3) == 0 ? R() : 0u);
            w32(S + 4, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
            wf(S + 8, rf(-3, 3));
            wf(S + 0xc, ri(0, 3) == 0 ? 0.0f : rf(-0.5f, 3));
            w32(S + 0x10, 0x002f4b18u);
            wf(0x00161f3c, rf(0, 0.5f));  // $gp - 0x4cc4: the frame step the callback counts down by
            w32(OV(0x0015ef4c), u(ri(0, 14)));  // the queue's count (it holds up to 16)
        };
        l10check("cam0_hook", 0x002f4b18, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { orx_cam0_hook_l10(0); return uint64_t{0}; });
        l10check("cam_callback", 0x002f4b78, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { orx_cam_callback_l10(0); return uint64_t{0}; });
    }

    // ---- level 10's camera type 0x13 (accept / init / update / exit: section 4 of the overlay)
    if (g_oracle_level == 10) {
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
        static const CamT cams[] = {{19, 0x002f65f8, 0x002f5b58, 0x002f5f18, 0x002f66c0}};
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
            l10check(name("accept"), ct.accept, 300, Ret::Int32, [cam_setup, type] { uint32_t other = 0; const uint32_t cam = cam_setup(type, true, &other); return Args{{cam, other}, {}}; },
                    [type](A a) { return r32(a.i[0] != 0 ? orx_cam_type19_accept_u(a.i[0], a.i[1], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).v0 : 0u); });
            l10check(name("init"), ct.init, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) orx_cam_type19_init_l10(a.i[0]); return uint64_t{0}; });
            l10check(name("update"), ct.update, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) orx_cam_type19_update_l10(a.i[0]); return uint64_t{0}; });
            l10check(name("exit"), ct.exit, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) empty_update(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- mobys_in_sphere (Veldin 0x1f2868, level 10's copy L10:0x001e5ed0) on the level's own mobys: class 794 (the Gadgebot's helper) calls it and one of its random states differed
    if (g_oracle_level == 10) {
        l10check("mobys_in_sphere", 0x001e5ed0, 600, Ret::Int32, [level] {
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

    // ---- libc_memcpy (the base ELF's memcpy 0x115248, ported by this task: class 794 calls it). Overlapping and unaligned ranges included.
    if (g_oracle_level == 10) {
        l10check("libc_memcpy", 0x00115248, 400, Ret::Int32, [] {
            const uint32_t base = 0x00180000u;
            for (uint32_t k = 0; k < 0x200; k += 4) w32(base + k, R());
            const uint32_t n = ri(0, 3) == 0 ? u(ri(0, 0x1f)) : u(ri(0, 0xa0));
            auto pick = [&] { return base + 0x40 + (ri(0, 1) != 0 ? 0x10 * u(ri(0, 8)) : u(ri(0, 0x80))); };
            return Args{{pick(), pick(), n}, {}};
        }, [](A a) { return r32(libc_memcpy(a.i[0], a.i[1], a.i[2])); });
    }

    // T00: the level's copy of Veldin's helmet_init_fx (0x2e06a8; the scan's body hash pairs them, the overlay map does not) at
    // L10:0x00298860: the hand port runs in its place; a live moby's pvars +0x10 .. +0x4c set up (the random bytes, the timers by
    // the frame scale)
    if (g_oracle_level == 10) {
        l10check("helmet_init_fx_298860", 0x00298860, 120, Ret::None, [level] {
            level();
            std::vector<uint32_t> ms;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x78) != 0) ms.push_back(m);
            const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            if (ri(0, 3) == 0) wf(0x0015ed68, 1.2f);  // (the frame scale at its 50 Hz value: frame_scale_i's counts)
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { helmet_init_fx(a.i[0]); return uint64_t{0}; });
    }

    // T00: the level's bare `jr ra` functions its classes and cameras reach through a pointer (class updates, camera
    // callbacks; the scan pairs them with Veldin's 0x1f6bf8): empty_update runs in their places, each against the original
    // called directly (the level may have no moby of the class)
    if (g_oracle_level == 10) {
        static const uint32_t empties[] = {0x0029e1c8, 0x002c4bc8, 0x002dd8d8, 0x002eb8b8, 0x002eb950, 0x002f66c0};
        for (const uint32_t a : empties) {
            char nm[24];
            std::snprintf(nm, sizeof nm, "empty_%x", a);
            l10check(nm, a, 20, Ret::None, [level] {
                level();
                return Args{{R(), R(), R(), R()}, {rf(-9, 9)}};
            }, [](A a) { empty_update(a.i[0]); return uint64_t{0}; });
        }
    }

    //@@NEXT
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
