// Oracle batch 223: Oltanis (level 14) on RAM dumps of headless runs of the port: the arrival's scene, the town, the talks. Run with
// ORACLE_LEVEL=14 build/tests/ovl_oracle --batch 223 (needs the ISO and the dumps; a missing dump is skipped).
// The dumps (assets/dumps/oracle/gemlik_oltanis/, written by `tools/oracle/gemlik_oltanis_tour.sh dumps`, LEVEL=14, RAC1_GIVE_FRAME=1600
// RAC1_GIVE_ITEMS=0xc,0xf,0x2):
//   arr{30,60,300,1000}_L14.bin  RAC1_HEADLESS=1 RAC1_LEVEL=14 RAC1_SIM_INPUT=seq:0:l3 RAC1_RAMHASH=/dev/null RAC1_RAMDUMP=<frame>,<file>
//                              (the arrival's scene 0 started by class 684, game mode 2; frame 30 is in its frames 0x1c..0x21, where the
//                              cutscene extras run the effect 0x245e98 and queue the draw 0x2459c0)
//   town1700_L14.bin           the same at frame 1700 (the hero in the town after the scene)
//   c{712,924,851,1395,685}_L14.bin  RAC1_HERO_AT=<class>@1650,3,0,1,3.14 and the pad script 1700-2390: triangle every 100 frames,
//                              square every 25, the stick forward 2000-2390 (talks, the wrench), at frame 2400
//   talk{924,851}_L14.bin      the same to frame 2900 (the scenes their talks start)
// Checked on each dump: every class of the level with a live moby (its state, its pvars and a hit on it varied), the draw-list
// callbacks of the classes, and on the scene's dumps the cutscene extras (L14:0x2460f8) and their draw (L14:0x2459c0) on the scene's moby.
// Class 712's states 3 / 4 / 5 / 7, which batch 221 cannot reach on the level's first frame, are among the random states here.

#include "ovl_oracle.h"

#include <game/lib/libcdvd.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/empty_class_updates.h>

#include <csetjmp>
#include <csignal>
#include <cstring>
#include <sys/time.h>
#include <map>
#include <string>

namespace rac1::oracle {

namespace {
// A check whose original is level 14's own function at `addr`.
void o2check(const char* name, uint32_t addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    std::printf("%-26s L14:0x%08x\n", name, addr);
    if (!check_fn(name, addr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
const std::vector<uint8_t>& dump223(const std::string& path) {
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
void load223(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}
std::vector<uint32_t> live223(int cls) {
    std::vector<uint32_t> v;
    for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) < 0xfe && (cls < 0 || static_cast<int16_t>(ee_read<uint16_t>(m + 0xa6)) == cls)) v.push_back(m);
    return v;
}
std::vector<uint32_t> particles223(uint8_t type) {
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
// round a path that has no length loops forever on both sides (the interpreter stops at its instruction budget): not a game state, dealt again.
sigjmp_buf g_trial_jmp_223;
void trial_sig_223(int) { siglongjmp(g_trial_jmp_223, 1); }
bool trial_run_223(void (*port)(uint32_t), uint32_t m) {
    struct sigaction sa {}, old_segv {}, old_alrm {};
    sa.sa_handler = trial_sig_223;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NODEFER;
    sigaction(SIGSEGV, &sa, &old_segv);
    sigaction(SIGALRM, &sa, &old_alrm);
    itimerval it {};
    it.it_value.tv_sec = 2;
    bool ok = true;
    if (sigsetjmp(g_trial_jmp_223, 1) != 0) ok = false;
    else {
        setitimer(ITIMER_REAL, &it, nullptr);
        try { port(m); } catch (const std::exception&) { ok = false; }
    }
    itimerval off {};
    setitimer(ITIMER_REAL, &off, nullptr);
    sigaction(SIGSEGV, &old_segv, nullptr);
    sigaction(SIGALRM, &old_alrm, nullptr);
    return ok;
}
uint32_t pick223(const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; }
}  // namespace

void batch_223() {
    if (g_only_batch != 223) return;
    g_batch = 223;
    if (g_oracle_level != 14) {
        std::printf("batch 223: Oltanis's dumps only: run with ORACLE_LEVEL=14\n");
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
            {903, 0x002aba80, olt_class903_update_l14}, {8, 0x002ac618, olt_class8_update_l14}, {28, 0x002b17d8, olt_class28_update_l14},
            {30, 0x002b3bf0, olt_class30_update_l14}, {31, 0x002b46e8, olt_class31_update_l14}, {81, 0x002bac78, olt_class81_update_l14},
            {211, 0x002d5f40, olt_class211_update_l14}, {250, 0x002d96e0, olt_class250_update_l14}, {252, 0x002d98f0, olt_class252_update_l14},
            {258, 0x002db378, olt_class258_update_l14}, {309, 0x002de1f8, olt_class309_update_l14}, {325, 0x002de670, olt_class325_update_l14},
            {386, 0x002dee28, olt_class386_update_l14}, {403, 0x002dfd10, olt_class403_update_l14}, {455, 0x002e1b20, olt_class455_update_l14},
            {531, 0x002e7190, empty_update}, {545, 0x002e72d8, olt_class545_update_l14}, {557, 0x002e7bd8, olt_class557_update_l14},
            {604, 0x002ea9d0, olt_class604_update_l14}, {605, 0x002ead20, olt_class605_update_l14}, {610, 0x002eaf88, olt_class610_update_l14},
            {643, 0x002ec810, olt_class643_update_l14}, {681, 0x002ece00, olt_class681_update_l14}, {684, 0x002ed280, olt_class684_update_l14},
            {685, 0x002ee8d8, olt_class685_update_l14}, {686, 0x002ef840, olt_class686_update_l14}, {712, 0x002f0538, olt_class712_update_l14},
            {758, 0x002f2cb8, olt_class758_update_l14}, {765, 0x002f3ef0, empty_update}, {805, 0x002f4190, olt_class805_update_l14},
            {832, 0x002f6338, olt_class832_update_l14}, {851, 0x002fba20, olt_class851_update_l14}, {908, 0x002fc0f0, olt_class908_update_l14},
            {913, 0x002fcba8, empty_update}, {921, 0x002fcbb0, olt_class921_update_l14}, {922, 0x002fdbb8, olt_class922_update_l14},
            {923, 0x002fe2a0, olt_class923_update_l14}, {924, 0x002fefe0, olt_class924_update_l14}, {933, 0x002ff640, olt_class933_update_l14},
            {1134, 0x002ffc18, olt_class1134_update_l14}, {1193, 0x00300e00, olt_class1193_update_l14}, {1224, 0x003015d0, olt_class1224_update_l14},
            {1260, 0x00303370, olt_class1260_update_l14}, {1331, 0x003039e0, olt_class1331_update_l14}, {1352, 0x00305408, olt_class1352_update_l14},
            {1354, 0x00305758, olt_class1354_update_l14}, {1395, 0x00305d28, olt_class1395_update_l14}, {1396, 0x00306150, empty_update},
            {1397, 0x003061d8, olt_class1397_update_l14}, {1400, 0x00306390, olt_class1400_update_l14}, {1416, 0x00306b78, olt_class1416_update_l14},
            {1417, 0x00306ee0, olt_class1417_update_l14}, {1418, 0x00307a80, olt_class1418_update_l14}, {1511, 0x00307ad8, olt_class1511_update_l14},
            {1514, 0x00308350, empty_update}, {1559, 0x003087c0, olt_class1559_update_l14}, {1629, 0x003088b0, olt_class1629_update_l14},
            {1633, 0x00308be0, olt_class1633_update_l14}, {1818, 0x00309c88, olt_class1818_update_l14}};
    struct Cb { const char* name; uint32_t addr; void (*fn)(uint32_t); int cls; };
    static const Cb cbs[] = {{"cb2aee28", 0x002aee28, olt_cb_2aee28_l14, 8},
                             {"cb2b2928", 0x002b2928, olt_cb_2b2928_l14, 28},
                             {"cb2d71f0", 0x002d71f0, olt_cb_2d71f0_l14, 211},
                             {"cb2da678", 0x002da678, olt_cb_2da678_l14, 252},
                             {"cb2daf08", 0x002daf08, olt_cb_2daf08_l14, 252},
                             {"cb2dec40", 0x002dec40, olt_cb_2dec40_l14, 325},
                             {"cb2df458", 0x002df458, olt_cb_2df458_l14, 386},
                             {"cb2eb810", 0x002eb810, olt_cb_2eb810_l14, 610},
                             {"cb2edc18", 0x002edc18, olt_cb_2edc18_l14, 684},
                             {"cb2f6128", 0x002f6128, olt_cb_2f6128_l14, 832},
                             {"cb301fa8", 0x00301fa8, olt_cb_301fa8_l14, 1224},
                             {"cb303350", 0x00303350, olt_cb_303350_l14, 1260},
                             {"cb307a28", 0x00307a28, olt_cb_307a28_l14, 1418},
                             {"cb308150", 0x00308150, olt_cb_308150_l14, 1511},
                             {"cb309bb0", 0x00309bb0, olt_cb_309bb0_l14, 1818},
                             {"cb2ae260", 0x002ae260, olt_cb_2ae260_l14, 8},
                             {"cb2b3a20", 0x002b3a20, olt_cb_2b3a20_l14, 28}};
    static const char* const dumps[] = {"arr30", "arr60", "arr300", "arr1000", "town1700", "c712", "c924", "c851", "c1395", "c685", "talk924", "talk851"};
    for (const char* dn : dumps) {
        const std::string path = dump_path("gemlik_oltanis/") + dn + "_L14.bin";
        const std::vector<uint8_t>& d = dump223(path);
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
        load223(d);
        { const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c)); for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c); }
        // the debris / spawns' records after the moby pool: the w lanes of a VU0 sin / cos the original leaves (the port has none; as batch 124)
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
        // ---- every class of the level with a live moby in the dump
        for (const auto& c : classes) {
            if (const char* cl = std::getenv("B223_CLS"); cl != nullptr && std::atoi(cl) != c.cls) continue;
            load223(d);
            std::vector<uint32_t> ms;
            for (uint32_t m : live223(-1))
                if (ee_read<uint32_t>(m + 0x74) == c.fn) ms.push_back(m);
            if (ms.empty()) continue;
            const std::string nm = "class" + std::to_string(c.cls) + tag;
            auto port = c.port;
            const int cls = c.cls;
            o2check(nm.c_str(), c.fn, cls == 712 || cls == 684 ? 200 : 40, Ret::None, [&d, ms, vary_pvars, give_hit, port, cls] {
                // a random state that sends the port through a pointer outside RAM is not a game state: dealt again (as batch 142's)
                static std::vector<uint8_t> saved;
                uint32_t m = 0;
                for (int attempt = 0; attempt < 40; ++attempt) {
                    load223(d);
                    m = pick223(ms);
                    if (cls != 211 && ri(0, 2) == 0) vary_pvars(m);  // (211's pvars hold its path's pointer beside floats: a varied one is no address)
                    if (ri(0, 3) == 0) {  // (not the states that request the level change / play a movie: the original runs the kernel / movie path)
                        uint32_t st = u(ri(0, 12));
                        if ((cls == 1134 || cls == 1354) && st == 2) st = 0;
                        w8(m + 0x20, st);
                    }
                    if (coin()) give_hit(m);
                    w32(OV(0x0015f5cc), R());
                    seed_rand();
                    saved.assign(ee_get_ram_base(), ee_get_ram_base() + 0x02000000);
                    bool ok = true;
                    const uint32_t stack_top = ee_stack_ptr();
                    ok = trial_run_223(port, m);
                    ee_stack_ptr() = stack_top;
                    std::memcpy(ee_get_ram_base(), saved.data(), saved.size());
                    if (ok) break;
                }
                return Args{{m}, {}};
            }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
        }
        if (std::getenv("B223_CLS") != nullptr) continue;
        // ---- the classes' draw-list callbacks on the dump's own camera and mobys
        for (const Cb& cb : cbs) {
            load223(d);
            const std::vector<uint32_t> ms = live223(cb.cls);
            if (ms.empty()) continue;
            const std::string nm = std::string(cb.name) + tag;
            o2check(nm.c_str(), cb.addr, 40, Ret::None, [&d, ms, vary_pvars] {
                load223(d);
                const uint32_t m = pick223(ms);
                vary_pvars(m);
                w32(OV(0x001611c0), 0x00800000u);  // (the display packet cursor: a buffer of its own)
                w32(OV(0x0015f5cc), R());
                seed_rand();
                return Args{{m}, {}};
            }, [cb](A a) { cb.fn(a.i[0]); return uint64_t{0}; });
        }
        // ---- the cutscene extras (L14:0x2460f8) and their draw (L14:0x2459c0) on the scene's own state (arrival dumps: scene 0), its frame moved
        // about 0x1c..0x21
        if (std::strncmp(dn, "arr", 3) == 0) {
            const uint32_t SC = OV(0x0016c860);  // (the scene state: L14:0x16cf60)
            o2check(("cutscene_level_extras" + tag).c_str(), 0x002460f8, 60, Ret::None, [&d, SC] {
                load223(d);
                w32(SC + 0x34, u(ri(0x18, 0x24)));
                w32(OV(0x001611c0), 0x00800000u);
                w32(OV(0x0015f5cc), R());
                seed_rand();
                return Args{};
            }, [](A) { olt_cutscene_level_extras_l14(0); return uint64_t{0}; });
            load223(d);
            const uint32_t sm = ee_read<uint32_t>(SC + 0x17c);
            if (sm != 0)
                o2check(("cb2459c0" + tag).c_str(), 0x002459c0, 60, Ret::None, [&d, sm, vary_pvars] {
                    load223(d);
                    vary_pvars(sm);
                    w32(OV(0x001611c0), 0x00800000u);
                    w32(OV(0x0015f5cc), R());
                    seed_rand();
                    return Args{{sm}, {}};
                }, [](A a) { olt_cb_2459c0_l14(a.i[0]); return uint64_t{0}; });
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
