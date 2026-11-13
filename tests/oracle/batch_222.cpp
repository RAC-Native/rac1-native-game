// Oracle batch 222: Gemlik Base (level 13, the code's "Oltanis Orbit") on RAM dumps of headless runs of the port: Qwark's space fight
// and the landing. Run with ORACLE_LEVEL=13 build/tests/ovl_oracle --batch 222 (needs the ISO and the dumps; a missing dump is skipped).
// The dumps (assets/dumps/oracle/gemlik_oltanis/, written by `tools/oracle/gemlik_oltanis_tour.sh dumps`, LEVEL=13):
//   land{60,200,400}_L13.bin   RAC1_HEADLESS=1 RAC1_LEVEL=13 RAC1_SIM_INPUT=seq:0:l3 RAC1_RAMHASH=/dev/null RAC1_RAMDUMP=<frame>,<file>
//                              (the ship's landing, game mode 6: the type-0x44 particles of its engines live)
//   boss{2000,2800,3500,4000,5000,6500}_L13.bin  the same with RAC1_HERO_AT=388@800,12,0,1,3.14 RAC1_GIVE_FRAME=780
//                              RAC1_GIVE_ITEMS=0xc,0xf,0x2 RAC1_SIM_INPUT=seq:<a press of cross every 6 frames from 2700> (the hero put by
//                              Qwark's class 388: its scene (mode 2, frames ~1700..2650), then the ship's run of the fight, the player's ship
//                              class 69 in its states 1 / 4 / 7, Qwark 388 in 1 / 2 / 4 / 6, his health bar (HUD element 0x11) on screen).
// Checked on each dump: every class of the level with a live moby (its state, its pvars and a hit on it varied), the draw-list
// callbacks of the classes, the HUD element 0x11's draw function (L13:0x245cf8), and particle type 0x44's update (Veldin 0x274818,
// particle_44_tr.cpp, L13:0x280530) on the dump's own particles. Class 388's state 5, which batch 220 cannot reach on the level's first
// frame, is among the fight's random states here.

#include "ovl_oracle.h"
#include "trial_guard.h"

#include <game/lib/libcdvd.h>
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_44_tr.h>
#include <game/ovl/variants_l13.h>

#include <cstring>
#include <map>
#include <string>

namespace rac1::oracle {

namespace {
// A check whose original is level 13's own function at `addr`.
void g2check(const char* name, uint32_t addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    std::printf("%-26s L13:0x%08x\n", name, addr);
    if (!check_fn(name, addr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
const std::vector<uint8_t>& dump222(const std::string& path) {
    static std::map<std::string, std::vector<uint8_t>> cache;
    std::vector<uint8_t>& d = cache[path];
    if (d.empty()) {
        if (FILE* f = std::fopen(path.c_str(), "rb")) {
            d.resize(0x02000000 + 0x4000);
            if (std::fread(d.data(), 1, d.size(), f) != d.size()) d.clear();
            std::fclose(f);
        }
    }
    return d;
}
void load222(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}
std::vector<uint32_t> live222(int cls) {
    std::vector<uint32_t> v;
    for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) < 0xfe && (cls < 0 || static_cast<int16_t>(ee_read<uint16_t>(m + 0xa6)) == cls)) v.push_back(m);
    return v;
}
std::vector<uint32_t> particles222(uint8_t type) {
    std::vector<uint32_t> out;
    const uint32_t base = ee_read<uint32_t>(OV(0x0016016c));
    const int32_t hi = ee_read<int32_t>(OV(0x00160174));
    for (int32_t i = 0; i <= hi && i < 4096; ++i) {
        const uint32_t p = base + u(i) * 64;
        if (ee_read<uint8_t>(p) == type && ee_read<int8_t>(p + 1) >= 0) out.push_back(p);
    }
    return out;
}
// A trial run of the port on a random state, as batch 124's, with a time limit too: a state whose pvars send a path walk (path_advance)
// round a path that has no length loops forever on both sides (the interpreter stops at its instruction budget): not a game state.
// POSIX rejects and retries a timed-out state; Windows reports a fatal timeout (trial_guard.h).

uint32_t pick222(const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; }
}  // namespace

void batch_222() {
    if (g_only_batch != 222) return;
    g_batch = 222;
    if (g_oracle_level != 13) {
        std::printf("batch 222: Gemlik Base's dumps only: run with ORACLE_LEVEL=13\n");
        ++g_skipped;
        return;
    }
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // (the particle update's saved return address)
    g_ignored_words.push_back(OV(0x0016018c));
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);  // (the pose function's spills of the caller's $t8 / $t9)
    // the collision's contact vectors' w lanes and the vectors segment_collide (0x1efc70) stores at Veldin 0x173ee0.. from the VU0 registers
    // (a RAM dump does not hold them), the floor lights' normals' w lanes (as batch 142)
    for (uint32_t a = OV(0x00173e80); a < OV(0x00173fc0); a += 0x10) g_ignored_words.push_back(a + 0xc);
    for (uint32_t a = OV(0x00173ee0); a < OV(0x00173f40); a += 4) g_ignored_words.push_back(a);
    g_ignored_words.push_back(OV(0x00173e9c));
    for (uint32_t i = 0; i < 0x20; ++i) g_ignored_words.push_back(OV(0x0016e080) + i * 0x20 + 0x1c);
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    const std::time_t saved_clock = g_host_clock_fixed;
    g_host_clock_fixed = 1759658400;
    g_extra_hooks = {{0x00121780, [](EeCpu& c) { c.set32(2, cd_read_clock(c.r32(4))); }},
                     {0x0012d6d8, [](EeCpu& c) { clock_to_local(c.r32(4)); }},
                     {0x00121450, [](EeCpu& c) { c.set32(2, cd_read(c.r32(4), c.r32(5), c.r32(6), c.r32(7))); }}};
    g_extra_noop_hooks = {0x00122298, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};  // (vsync / kernel waits, the level change request)

    auto vary_pvars = [](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (pv == 0 || !coin()) return;
        for (uint32_t o = 0; o < 0x80; o += 4) {
            const uint32_t w = ee_read<uint32_t>(pv + o);
            const uint32_t e = (w >> 23) & 0xff;
            if (e > 100 && e < 150 && coin()) wf(pv + o, ee_read<float>(pv + o) * rf(0.5f, 1.5f));
            else if (w >= 1 && w < 0x100 && coin()) w32(pv + o, u(ri(1, 0x40)));  // (a 0 stays 0: a zero float made a denormal is no game state)
        }
    };
    // a hit on the moby: its slot in the hit table (Veldin 0x178100, 64-byte entries) naming it, a damage (as batch 124's)
    auto give_hit = [](uint32_t m) {
        if (ri(0, 2) == 0) { w8(m + 0xa4, 0xff); return; }
        const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
        w8(m + 0xa4, idx);
        w32(e + 0x34, ri(0, 8) == 0 ? ee_read<uint32_t>(OV(0x0015ffd8)) : m);
        w32(e + 0x24, coin() ? R() : (0x210000u | (R() & 0xff)));
        { static const float dm[] = {-1.0f, 0.0f, 0.5f, 5.0f, 12.0f}; wf(e + 0x2c, dm[ri(0, 4)]); }
        wfrand(e, 4, -1, 1);
        w32(e + 0x28, u(ri(0, 20)));
    };
    static const struct { int cls; uint32_t fn; void (*port)(uint32_t); } classes[] = {
            {726, 0x002b0478, gml_class726_update_l13}, {6, 0x002b0e80, gml_class6_update_l13}, {21, 0x002b39e0, gml_class21_update_l13},
            {29, 0x002b41b8, gml_class29_update_l13}, {36, 0x002b4c80, gml_class36_update_l13}, {63, 0x002b50d8, gml_class63_update_l13},
            {69, 0x002bb068, gml_class69_update_l13}, {82, 0x002c1528, gml_class82_update_l13}, {83, 0x002c2048, gml_class83_update_l13},
            {101, 0x002c25b8, gml_class101_update_l13}, {104, 0x002c32d0, gml_class104_update_l13}, {111, 0x002c4428, gml_class111_update_l13},
            {127, 0x002c7f38, gml_class127_update_l13}, {129, 0x002c8390, gml_class129_update_l13}, {170, 0x002cefc0, gml_class170_update_l13},
            {212, 0x002e1638, gml_class212_update_l13}, {224, 0x002e1cc8, gml_class224_update_l13}, {231, 0x002e4448, gml_class231_update_l13},
            {258, 0x002e4bd0, gml_class258_update_l13}, {295, 0x002e6c08, gml_class295_update_l13}, {304, 0x002e7238, gml_class304_update_l13},
            {352, 0x002e84d8, gml_class352_update_l13}, {388, 0x002eb098, gml_class388_update_l13}, {389, 0x002ece00, gml_class389_update_l13},
            {404, 0x002ed4a8, gml_class404_update_l13}, {447, 0x002edbe8, gml_class447_update_l13}, {533, 0x002f3750, gml_class533_update_l13},
            {558, 0x002f3778, gml_class558_update_l13}, {605, 0x002f51f0, gml_class605_update_l13}, {615, 0x002f5618, gml_class615_update_l13},
            {667, 0x002f8880, gml_class667_update_l13}, {674, 0x002f8a78, gml_class674_update_l13}, {737, 0x002f8e70, gml_class737_update_l13},
            {758, 0x002fa708, gml_class758_update_l13}, {805, 0x002fac70, gml_class805_update_l13}, {832, 0x002fce18, gml_class832_update_l13},
            {913, 0x00302418, gml_class913_update_l13}, {1009, 0x00302780, gml_class1009_update_l13}, {1017, 0x00302e48, gml_class1017_update_l13},
            {1034, 0x00303128, gml_class1034_update_l13}, {1039, 0x003036f0, gml_class1039_update_l13}, {1085, 0x00303878, gml_class1085_update_l13},
            {1134, 0x00303950, gml_class1134_update_l13}, {1135, 0x00304888, gml_class1135_update_l13}, {1233, 0x00305a58, gml_class1233_update_l13},
            {1238, 0x00306300, gml_class1238_update_l13}, {1261, 0x00307b10, gml_class1261_update_l13}, {1262, 0x00307e98, gml_class1262_update_l13},
            {1263, 0x00309e88, gml_class1263_update_l13}, {1270, 0x0030a6d8, gml_class1270_update_l13}, {1271, 0x0030af50, gml_class1271_update_l13},
            {1353, 0x0030b628, gml_class1353_update_l13}, {1403, 0x0030bb70, gml_class1403_update_l13}, {1558, 0x0030bdf0, gml_class1558_update_l13},
            {1577, 0x0030be60, gml_class1577_update_l13}, {1632, 0x0030c060, gml_class1632_update_l13}, {1634, 0x0030c3b8, gml_class1634_update_l13},
            {1805, 0x0030cdb8, gml_class1805_update_l13}, {1885, 0x0030d550, gml_class1885_update_l13}, {1886, 0x0030dd80, gml_class1886_update_l13}};
    struct Cb { const char* name; uint32_t addr; void (*fn)(uint32_t); int cls; };
    static const Cb cbs[] = {{"cb2b97f8", 0x002b97f8, gml_cb_2b97f8_l13, 69},   {"cb2ea8c8", 0x002ea8c8, gml_cb_2ea8c8_l13, 388},
                             {"cb2f5f30", 0x002f5f30, gml_cb_2f5f30_l13, 615},  {"cb2fcc08", 0x002fcc08, gml_cb_2fcc08_l13, 832},
                             {"cb3051a0", 0x003051a0, gml_cb_3051a0_l13, 1135}, {"cb3098d0", 0x003098d0, gml_cb_3098d0_l13, 1262},
                             {"cb30a460", 0x0030a460, gml_cb_30a460_l13, 1263}};
    static const char* const dumps[] = {"land60", "land200", "land400", "boss2000", "boss2800", "boss3500", "boss4000", "boss5000", "boss6500"};
    for (const char* dn : dumps) {
        const std::string path = dump_path("gemlik_oltanis/") + dn + "_L13.bin";
        const std::vector<uint8_t>& d = dump222(path);
        if (d.empty()) {
            std::printf("dump %s missing: skipped (tools/oracle/gemlik_oltanis_tour.sh dumps)\n", path.c_str());
            ++g_skipped;
            continue;
        }
        const std::string tag = std::string("_") + dn;
        std::printf("  (on %s)\n", path.c_str());
        // the particle pool's records (0x800 of 0x40 bytes at *(Veldin 0x16016c)): the w lane of the velocity a spray copies (a stale lane of
        // vec_from_spherical in the original, as batch 124)
        const size_t ign_pool = g_ignored_words.size();
        load222(d);
        { const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c)); for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c); }
        // the debris / spawns' records after the moby pool: the w lanes of a VU0 sin / cos the original leaves (the port has none; as batch 124)
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
        // ---- every class of the level with a live moby in the dump
        for (const auto& c : classes) {
            if (const char* cl = std::getenv("B222_CLS"); cl != nullptr && std::atoi(cl) != c.cls) continue;
            load222(d);
            std::vector<uint32_t> ms;
            for (uint32_t m : live222(-1))
                if (ee_read<uint32_t>(m + 0x74) == c.fn) ms.push_back(m);
            if (ms.empty()) continue;
            const std::string nm = "class" + std::to_string(c.cls) + tag;
            auto port = c.port;
            const int cls = c.cls;
            g2check(nm.c_str(), c.fn, cls == 388 || cls == 69 ? 200 : 40, Ret::None, [&d, ms, vary_pvars, give_hit, port, cls] {
                // a random state that sends the port through a pointer outside RAM is not a game state: dealt again (as batch 142's)
                static std::vector<uint8_t> saved;
                uint32_t m = 0;
                for (int attempt = 0; attempt < 40; ++attempt) {
                    load222(d);
                    m = pick222(ms);
                    if (cls != 1353 && ri(0, 2) == 0) vary_pvars(m);  // (1353's timers at their end send it to the level change)
                    if (ri(0, 3) == 0) {  // (not the states that request the level change / play a movie: the original runs the kernel / movie path)
                        uint32_t st = u(ri(0, cls == 388 ? 9 : 12));
                        if ((cls == 1134 && st == 2) || (cls == 304 && st == 4) || (cls == 1353 && (st == 1 || st >= 8))) st = 0;
                        w8(m + 0x20, st);
                    }
                    if (cls != 1353 && coin()) give_hit(m);  // (a hit sends 1353 to its level change: the original's kernel / movie path)
                    w32(OV(0x0015f5cc), R());
                    seed_rand();
                    saved.assign(ee_get_ram_base(), ee_get_ram_base() + 0x02000000);
                    bool ok = true;
                    const uint32_t stack_top = ee_stack_ptr();
                    ok = run_trial(port, m, 2);
                    ee_stack_ptr() = stack_top;
                    std::memcpy(ee_get_ram_base(), saved.data(), saved.size());
                    if (ok) break;
                }
                return Args{{m}, {}};
            }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
        }
        if (std::getenv("B222_CLS") != nullptr) continue;
        // ---- the classes' draw-list callbacks on the dump's own camera and mobys
        for (const Cb& cb : cbs) {
            load222(d);
            const std::vector<uint32_t> ms = live222(cb.cls);
            if (ms.empty()) continue;
            const std::string nm = std::string(cb.name) + tag;
            g2check(nm.c_str(), cb.addr, 40, Ret::None, [&d, ms, vary_pvars] {
                load222(d);
                const uint32_t m = pick222(ms);
                vary_pvars(m);
                w32(OV(0x001611c0), 0x00800000u);  // (the display packet cursor: a buffer of its own)
                w32(OV(0x0015f5cc), R());
                seed_rand();
                return Args{{m}, {}};
            }, [cb](A a) { cb.fn(a.i[0]); return uint64_t{0}; });
        }
        // ---- the HUD element 0x11 (Qwark's health bar): the HUD slots (Veldin 0x17dc50 + 0x90 i) whose draw function is L13:0x245cf8
        {
            load222(d);
            std::vector<uint32_t> es;
            for (uint32_t e = OV(0x0017dc50), i = 0; i < 13; ++i, e += 0x90)
                if (ee_read<uint32_t>(e + 0x18) == 0x00245cf8u) es.push_back(e);
            if (!es.empty())
                g2check(("hud_245cf8" + tag).c_str(), 0x00245cf8, 60, Ret::None, [&d, es] {
                    load222(d);
                    const uint32_t e = pick222(es);
                    if (coin()) w32(e + 0x24, u(ri(0, 0x40)));
                    const uint32_t v = ee_read<uint32_t>(e + 0xc);  // (the slot's argument: class 388's pvars + 0x108)
                    if (v != 0 && coin()) wf(v, ee_read<float>(v) * rf(0, 1));
                    w32(OV(0x0015f5cc), R());
                    return Args{{e}, {}};
                }, [](A a) { gml_hud_245cf8_l13(a.i[0]); return uint64_t{0}; });
        }
        // ---- particle type 0x44 (Veldin 0x274818, the level's L13:0x280530) on the dump's own particles: its frames, size and velocity varied
        {
            load222(d);
            const std::vector<uint32_t> ps = particles222(0x44);
            if (!ps.empty())
                check(("particle_44_update" + tag).c_str(), 0x00274818, 60, Ret::None, [&d, ps] {
                    load222(d);
                    const uint32_t p = pick222(ps);
                    { const int k = ri(0, 3); if (k == 0) w16(p + 0xa, 0); else if (k == 1) w16(p + 0xa, u(ri(1, 2))); else if (k == 2) w16(p + 0xa, u(ri(3, 60))); }
                    if (coin()) wf(p + 0xc, rf(0.05f, 3.0f));
                    if (coin()) wfrand(p + 0x20, 3, -0.5f, 0.5f);
                    if (coin()) w8(p + 0x38, R() & 0xff);
                    seed_rand();
                    return Args{{p}, {}};
                }, [](A a) { particle_44_update(a.i[0]); return uint64_t{0}; });
        }
        g_ignored_words.resize(ign_pool);
    }
    g_extra_hooks.clear();
    g_extra_noop_hooks.clear();
    g_host_clock_fixed = saved_clock;
    g_ee_null_is_ram = null_ram;
    g_ignored_words = saved_ignored;
}

}  // namespace rac1::oracle
