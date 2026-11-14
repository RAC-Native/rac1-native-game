// Oracle batch 202 (levels/pokitaru-hoven, release 0.3.0): Pokitaru's (level 11) and Hoven's (level 12) own class updates on RAM dumps of
// headless runs of the port, the hero put next to each class's first moby: the states a level's first frame (batches 124 / 200) does
// not reach (a moby awake, the hero in its range, its timers running). Run with ORACLE_LEVEL=11 or 12 build/tests/ovl_oracle --batch 202.
// The dumps: assets/dumps/oracle/pokitaru_hoven/L<level>_c<class>_<frame>.bin (skipped without them), each written by (from the
// repository's root, one run at a time):
//   RAC1_HEADLESS=1 RAC1_RAMHASH=/dev/null RAC1_FRAMES=<f>+2 RAC1_RAMDUMP=<f>,assets/dumps/oracle/pokitaru_hoven/L<lv>_c<cls>_<f>.bin \
//   RAC1_HERO_AT=<cls>@300,3,0,1 RAC1_SIM_INPUT='seq:320-500:ly=-1;400+4:x;450+4:s;500-700:lx=1;520+4:x;600+4:s;650+4:o' \
//   build/rac1-native --level=<lv>                    with <f> = 420 and 700 (the hero walking up to the moby, then past it)
// Each case loads a dump, takes one of the class's mobys the dump's frame updated (the visible queue, update function +0x74) and varies a little what its update reads (the
// hero's position about where it is, the random seed, the frame counter; one case in four the moby's state byte as one of the states
// the class's mobys hold in the dumps), then runs level <lv>'s own update (the original at its address) against the port.
#include "ovl_oracle.h"

#include <game/ovl/hoven_mobys.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/empty_class_updates.h>
#include <runtime/kernel/disc.h>
#include <runtime/vu/vu0.h>

#include <algorithm>
#include <csetjmp>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {
// a random state can send the port's vector helpers through a pointer outside RAM (a SIGSEGV where the interpreter would wrap it):
// the trial run of a state turns it into "not a game state" (the state is dealt again), as batch 124's
sigjmp_buf g_trial_jmp;
void trial_segv(int) { siglongjmp(g_trial_jmp, 1); }
bool trial_run(void (*port)(uint32_t), uint32_t m) {
    const VUState vu_saved = vu0::global().st;  // (the port's VU0 registers: a trial must not leave its lanes to the case it vets)
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
    vu0::global().st = vu_saved;
    return ok;
}
struct Cls { int level; int cls; uint32_t addr; void (*fn)(uint32_t); };
const Cls kCls[] = {
    {11, 23, 0x002cb668, pok_class23_update_l11},
    {11, 37, 0x002cb790, pok_class37_update_l11},
    {11, 65, 0x002cb810, pok_class65_update_l11},
    {11, 90, 0x002d0710, pok_class90_update_l11},
    {11, 114, 0x002d0fa8, pok_class114_update_l11},
    {11, 298, 0x002f0e40, pok_class298_update_l11},
    {11, 318, 0x002f2518, pok_class318_update_l11},
    {11, 341, 0x002f30b0, pok_class341_update_l11},
    {11, 361, 0x002f3350, pok_class361_update_l11},
    {11, 572, 0x002f9998, pok_class572_update_l11},
    {11, 604, 0x002fc630, pok_class604_update_l11},
    {11, 605, 0x002fc980, pok_class605_update_l11},
    {11, 615, 0x002fcda8, pok_class615_update_l11},
    {11, 758, 0x00300010, pok_class758_update_l11},
    {11, 805, 0x00300578, pok_class805_update_l11},
    {11, 819, 0x00302510, pok_class819_update_l11},
    {11, 830, 0x00302ae0, pok_class830_update_l11},
    {11, 832, 0x00303070, pok_class832_update_l11},
    {11, 1009, 0x003089d0, pok_class1009_update_l11},
    {11, 1017, 0x00309098, pok_class1017_update_l11},
    {11, 1034, 0x003094f8, pok_class1034_update_l11},
    {11, 1075, 0x00309ac0, pok_class1075_update_l11},
    {11, 1111, 0x0030b358, pok_class1111_update_l11},
    {11, 1134, 0x0030b730, pok_class1134_update_l11},
    {11, 1156, 0x0030c788, pok_class1156_update_l11},
    {11, 1157, 0x0030d800, pok_class1157_update_l11},
    {11, 1158, 0x0030e088, pok_class1158_update_l11},
    {11, 1159, 0x0030e978, pok_class1159_update_l11},
    {11, 1178, 0x0030eb90, pok_class1178_update_l11},
    {11, 1179, 0x0030ee00, pok_class1179_update_l11},
    {11, 1180, 0x0030f2a8, pok_class1180_update_l11},
    {11, 1218, 0x0030f728, pok_class1218_update_l11},
    {11, 1219, 0x00310028, pok_class1219_update_l11},
    {11, 1231, 0x00310180, pok_class1231_update_l11},
    {11, 1242, 0x00313290, pok_class1242_update_l11},
    {11, 1246, 0x00314318, pok_class1246_update_l11},
    {11, 1248, 0x00316320, pok_class1248_update_l11},
    {11, 1264, 0x003172c0, pok_class1264_update_l11},
    {11, 1265, 0x003181f0, pok_class1265_update_l11},
    {11, 1297, 0x00318b30, pok_class1297_update_l11},
    {11, 1319, 0x00319838, pok_class1319_update_l11},
    {11, 1350, 0x0031aa80, pok_class1350_update_l11},
    {11, 1524, 0x0031ab08, pok_class1524_update_l11},
    {11, 1633, 0x0031b758, pok_class1633_update_l11},
    {11, 1818, 0x0031c800, pok_class1818_update_l11},
    {11, 1859, 0x0031d7d8, pok_class1859_update_l11},
    {11, 1860, 0x0031d8d0, empty_update},
    {11, 1903, 0x0031e2f0, pok_class1903_update_l11},
    {11, 1919, 0x0031f150, pok_class1919_update_l11},
    {12, 19, 0x002bf140, hov_class19_update_l12},
    {12, 37, 0x002bf9b8, hov_class37_update_l12},
    {12, 164, 0x002cb1e8, empty_update},
    {12, 178, 0x002d30f8, empty_update},
    {12, 184, 0x002d4378, hov_class184_update_l12},
    {12, 238, 0x002e1be0, hov_class238_update_l12},
    {12, 240, 0x002e3ed8, hov_class240_update_l12},
    {12, 258, 0x002e4cd8, hov_class258_update_l12},
    {12, 282, 0x002e6c48, hov_class282_update_l12},
    {12, 293, 0x002e7208, hov_class293_update_l12},
    {12, 294, 0x002e72c0, hov_class294_update_l12},
    {12, 296, 0x002e8c10, hov_class296_update_l12},
    {12, 326, 0x002e9f68, hov_class326_update_l12},
    {12, 328, 0x002eb570, hov_class328_update_l12},
    {12, 336, 0x002ebea8, hov_class336_update_l12},
    {12, 339, 0x002ec1d0, hov_class339_update_l12},
    {12, 341, 0x002ec478, hov_class341_update_l12},
    {12, 346, 0x002ec718, empty_update},
    {12, 384, 0x002ec720, hov_class384_update_l12},
    {12, 409, 0x002ece00, hov_class409_update_l12},
    {12, 422, 0x002ed280, hov_class422_update_l12},
    {12, 458, 0x002ef648, hov_class458_update_l12},
    {12, 499, 0x002f11b0, empty_update},
    {12, 604, 0x002f51f0, hov_class604_update_l12},
    {12, 605, 0x002f5540, hov_class605_update_l12},
    {12, 758, 0x002f7b28, hov_class758_update_l12},
    {12, 805, 0x002f8090, hov_class805_update_l12},
    {12, 832, 0x002fa238, hov_class832_update_l12},
    {12, 886, 0x002ff838, hov_class886_update_l12},
    {12, 1134, 0x002ffdb0, hov_class1134_update_l12},
    {12, 1135, 0x00300ce8, hov_class1135_update_l12},
    {12, 1221, 0x00301d08, hov_class1221_update_l12},
    {12, 1222, 0x00301df0, hov_class1222_update_l12},
    {12, 1259, 0x00302f30, hov_class1259_update_l12},
    {12, 1267, 0x00303540, hov_class1267_update_l12},
    {12, 1269, 0x00304e00, hov_class1269_update_l12},
    {12, 1274, 0x003069d0, hov_class1274_update_l12},
    {12, 1278, 0x003076d0, empty_update},
    {12, 1280, 0x003076d8, empty_update},
    {12, 1281, 0x00307840, hov_class1281_update_l12},
    {12, 1322, 0x003081a8, empty_update},
    {12, 1345, 0x003081b0, hov_class1345_update_l12},
    {12, 1371, 0x00308918, hov_class1371_update_l12},
    {12, 1400, 0x00308be0, hov_class1400_update_l12},
    {12, 1404, 0x003093c8, hov_class1404_update_l12},
    {12, 1557, 0x003094a0, hov_class1557_update_l12},
    {12, 1633, 0x00309648, hov_class1633_update_l12},
    {12, 1818, 0x0030a6f0, hov_class1818_update_l12},
    {12, 1862, 0x0030b6c8, hov_class1862_update_l12},
    {12, 1863, 0x0030b7c0, empty_update},
    {12, 1901, 0x0030bff0, hov_class1901_update_l12},
};
// the states batch 124 / 200 skip (the original leaves the harness there: the level change request, a kernel wait, a pointer read from a
// float of a state the class never holds): a case in one of them is dealt again
uint32_t skip_states(int lv, int cls) {
    if (lv == 11) return cls == 23 || cls == 1134 ? 0x4u : cls == 114 ? 0x9000u : cls == 1246 ? 0x2000u : cls == 1319 ? 0x6u : 0u;
    return cls == 282 || cls == 1134 ? 0x4u : cls == 1267 ? 0x120u : 0u;
}
const std::vector<uint8_t>& dump(const std::string& path) {
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
void load_dump(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}
// the dumps of level `lv`, class `cls`
std::vector<std::string> dump_files(int lv, int cls) {
    const std::string d = dump_path("pokitaru_hoven");
    const std::string pre = "L" + std::to_string(lv) + "_c" + std::to_string(cls) + "_";
    std::vector<std::string> v;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(d, ec)) {
        const std::string n = e.path().filename().string();
        if (n.rfind(pre, 0) == 0 && n.size() > 4 && n.substr(n.size() - 4) == ".bin") v.push_back(d + "/" + n);
    }
    std::sort(v.begin(), v.end());
    return v;
}
}  // namespace

void batch_202() {
    if (g_only_batch != 202) return;
    g_batch = 202;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // (the particle update's saved return address)
    g_ignored_words.push_back(OV(0x0016018c));
    for (uint32_t k = 0; k < 12; k += 4) g_ignored_words.push_back(OV(0x00174610) + k);  // (segment_collide's hit point: as batch 124's)
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page)
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);  // (the pose function's spills: as batch 124's)
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};  // (as batch 124's: the kernel, the level change request)
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
    const int lv = g_oracle_level;
    if (lv != 11 && lv != 12) {
        skip_check("pokitaru_hoven_dumps", 0, "level 11 / 12 only: run with ORACLE_LEVEL=11 or 12");
    }
    int ndumps = 0;
    for (const Cls& c : kCls) {
        if (c.level != lv) continue;
        // class 1404 (Hoven) starts the level's scene from the dumps' states: the scene's stream read waits on the IOP forever in the
        // harness (the port's stream_reads_wait); its update is checked by batch 200 on the level's first frame
        if (lv == 12 && c.cls == 1404) { skip_check("d_class1404", c.addr, "starts the level's scene: the stream read never completes in the harness (batch 200 checks it)"); continue; }
        char name[48];
        std::snprintf(name, sizeof name, "d_class%d", c.cls);
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) continue;
        std::vector<std::string> files = dump_files(lv, c.cls);
        // the dumps whose RAM holds live mobys of the class
        std::vector<std::pair<std::string, std::vector<uint32_t>>> subj;
        std::vector<uint8_t> states;
        for (const std::string& f : files) {
            const std::vector<uint8_t>& d = dump(f);
            if (d.empty()) continue;
            ++ndumps;
            load_dump(d);
            // the subjects: the class's mobys the frame of the dump updated (the visible queue moby_update_visible walked: its head at
            // 0x15ffe4, linked through +0x28); a moby the engine does not update (out of view, a dormant copy) is not a game state for its update
            std::vector<uint32_t> ms;
            int guard = 0;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffe4)); m != 0 && guard < 0x1000; m = ee_read<uint32_t>(m + 0x28), ++guard)
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == c.addr) { ms.push_back(m); states.push_back(ee_read<uint8_t>(m + 0x20)); }
            if (!ms.empty()) subj.emplace_back(f, ms);
        }
        if (subj.empty()) {
            std::printf("%-26s L%d:0x%08x  no dump with a moby of the class in the frame's update queue (%zu dump(s))\n", name, lv, c.addr, files.size());
            continue;
        }
        std::printf("%-26s L%d:0x%08x  (%zu dump(s))\n", name, lv, c.addr, subj.size());
        auto fn = c.fn;
        const uint32_t skip = skip_states(lv, c.cls);
        const bool ok = check_fn(name, c.addr, 60, Ret::None, [subj, states, fn, skip] {
            static std::vector<uint8_t> saved, saved_spr;
            uint32_t m = 0;
            for (int attempt = 0; attempt < 40; ++attempt) {  // (a state that sends the port through a pointer outside RAM is dealt again, as batch 124's)
                const auto& s = subj[static_cast<size_t>(ri(0, static_cast<int>(subj.size()) - 1))];
                load_dump(dump(s.first));
                m = s.second[static_cast<size_t>(ri(0, static_cast<int>(s.second.size()) - 1))];
                if (ri(0, 3) == 0) {
                    const uint8_t st = states[static_cast<size_t>(ri(0, static_cast<int>(states.size()) - 1))];
                    if (st >= 16 || (skip & (1u << st)) == 0) w8(m + 0x20, st);
                }
                if (attempt < 39 && ee_read<uint8_t>(m + 0x20) < 16 && (skip & (1u << ee_read<uint8_t>(m + 0x20))) != 0) continue;
                if (coin()) { Vec4 q = ee_ref<Vec4>(0x0013f3d0); q.x += rf(-2, 2); q.y += rf(-2, 2); q.z += rf(-0.5f, 1); ee_ref<Vec4>(0x0013f3d0) = q; }
                w32(OV(0x0015f5cc), R() & 0xffff);
                seed_rand();
                saved.assign(ee_get_ram_base(), ee_get_ram_base() + 0x02000000);
                { const uint8_t* spr = static_cast<const uint8_t*>(ee_ram_ptr(0x70000000u)); saved_spr.assign(spr, spr + 0x4000); }
                const uint32_t sp0 = ee_stack_ptr();
                const bool ok1 = trial_run(fn, m);
                ee_stack_ptr() = sp0;
                std::memcpy(ee_get_ram_base(), saved.data(), saved.size());
                std::memcpy(ee_ram_ptr(0x70000000u), saved_spr.data(), saved_spr.size());  // (the scratchpad too: the trial's port run writes it)
                if (ok1) break;
                if (std::getenv("B202_DEBUG") != nullptr) std::printf("  (attempt %d: the trial faulted, state %d)\n", attempt, ee_read<uint8_t>(m + 0x20));
            }
            return Args{{m}, {}};
        }, [fn](A a) {
            // (a SIGSEGV of the port, where the original ran: a failure, marked in RAM so that the case differs, not a crash of the batch)
            if (!trial_run(fn, a.i[0])) { std::printf("  [%s] the port faulted (SIGSEGV / exception)\n", "d_class"); w32(0x00180000u, 0xdeadbeefu); }
            return uint64_t{0};
        }, Scope::FullRam, 0, 0);
        if (!ok) ++g_fail;
    }
    if ((lv == 11 || lv == 12) && ndumps == 0)
        skip_check("pokitaru_hoven_dumps", 0, ("no RAM dumps in " + dump_path("pokitaru_hoven") + ": the command lines in batch 202's header").c_str());
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
