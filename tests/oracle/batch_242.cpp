// Oracle batch 242 (levels/quartu-kalebo, release 0.3.0 levels): the levels' own hero controllers in their special sections, on RAM
// dumps of headless runs of the port: Quartu's Giant Clank (ORACLE_LEVEL=15) and Kalebo III's hoverboard race (ORACLE_LEVEL=16). The
// Veldin ports run (hero_frame_update 0x2070d0, hero_state_update 0x217970, hero_input_update 0x229b70, hero_hurt 0x210ce8, hero_set_state
// 0x2223f8) and dispatch to the levels' own transliterations (quartu_hero_tr_*.cpp / kalebo_hero_tr_*.cpp); the original is the level's
// own function (the variants rows, variants_l15.cpp / variants_l16.cpp). Each case loads a dump and varies the pad (the stick, cross,
// square, triangle, circle held / pressed), a hit naming the hero's moby, his invulnerability word, the health and the random seed.
// On the race's dumps also Kalebo III's hero callbacks (the hoverboard's, installed by its hero_set_state; a0 = the hero's moby) and
// the race camera (type 8: accept / init / update) on the dump's live camera of the type.
// Run with ORACLE_LEVEL=15 and 16: build/tests/ovl_oracle --batch 242.
// The dumps (assets/dumps/oracle/levels_15_16/, skipped without them), from the repository's root; every run with RAC1_HEADLESS=1
// RAC1_RAMHASH=/dev/null RAC1_FRAMES=<f>+2 RAC1_RAMDUMP=<f>,assets/dumps/oracle/levels_15_16/<name>.bin:
//   gclank_<f>_L15.bin, f = 3500, 4500, 6000: RAC1_LEVEL=15 RAC1_HERO_AT='1446@740/1451@2300,0,0,0.5' (next to the scientist, then on
//       the Giant Clank pad) and RAC1_SIM_INPUT=seq: circle (o) at frames 760, 800, .. 2200 and cross (x) 20 frames after each (his
//       scene), triangle at 2950 and 3000 (Clank grows), cross at 3300 / 3360, then from 3600 the stick forward (ly=-1) with square every
//       25 frames, and from 5000 every 300 frames 100 frames of lx=1 and a cross (Giant Clank walking and punching among the drones)
//   race_<f>_L16.bin, f = 1500, 2500, 3500: RAC1_LEVEL=16 RAC1_GIVE_ITEMS=30 RAC1_HERO_AT=439@740 (the hoverboard) and
//       RAC1_SIM_INPUT=seq: circle at frames 760, 790, .. 1600 and cross 15 frames after each (the race starts), then from 1700 the
//       stick forward with cross every 60 frames (on the track, lap 1)

#include "ovl_oracle.h"

#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_hoverboard.h>
#include <game/ovl/shared_race_camera.h>
#include <game/ovl/shared_race_hud.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace rac1::oracle {

namespace {
// The checks named here reach segment_collide, whose crossing lists, chunk buffers and vertices the port keeps on
// the host stack (the original: 0x70003000 .. 0x70003fff, 0x70000000 .., 0x70002000): A2's collision scratch ranges
// are left out of these checks, and only of them (stage 5, task C2c).
bool c2c_spr_check(const char* name) {
    static const char* const kNames[] = {"frame_gclank_3500_L15", "frame_gclank_4500_L15", "frame_gclank_6000_L15",
                                         "input_gclank_3500_L15", "state_gclank_4500_L15"};
    for (const char* n : kNames)
        if (std::strcmp(n, name) == 0) return true;
    return false;
}

const std::vector<uint8_t>& b242_dump(const std::string& path) {
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
void b242_load(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}
std::vector<std::string> b242_files(const char* prefix) {
    const std::string d = dump_path("levels_15_16");
    std::vector<std::string> v;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(d, ec)) {
        const std::string n = e.path().filename().string();
        if (n.size() > 4 && n.substr(n.size() - 4) == ".bin" && n.rfind(prefix, 0) == 0) v.push_back(d + "/" + n);
    }
    std::sort(v.begin(), v.end());
    return v;
}
// A check whose original is level 16's own function at `laddr` (not a Veldin address to translate).
void l16check(const char* name, uint32_t laddr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    std::printf("%-26s L16:0x%08x\n", name, laddr);
    std::optional<CollisionScratchIgnored> spr_ignored;
    if (c2c_spr_check(name)) spr_ignored.emplace();
    if (!check_fn(name, laddr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
}  // namespace

void batch_242() {
    if (g_only_batch != 242) return;
    g_batch = 242;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // (the particle update's saved return address)
    g_ignored_words.push_back(OV(0x0016018c));
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    // the segment records (Veldin 0x173e90 .. 0x173ebc: VU0 registers the earlier cases left in the interpreter, as in batch 240)
    for (uint32_t k = 0; k < 0x30; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};

    const char* prefix = g_oracle_level == 15 ? "gclank_" : g_oracle_level == 16 ? "race_" : nullptr;
    std::vector<std::string> files;
    if (prefix != nullptr) files = b242_files(prefix);
    if (prefix == nullptr || files.empty()) {
        const char* why = prefix == nullptr ? "the dumps are Quartu's / Kalebo III's: ORACLE_LEVEL=15 or 16"
                                            : "no RAM dump (assets/dumps/oracle/levels_15_16: batch 242's header)";
        skip_check("hero_frame / state / input / hurt / set_state", 0x002070d0, why);
    }
    for (const std::string& path : files) {
        const std::vector<uint8_t>& d = b242_dump(path);
        if (d.empty()) continue;
        std::string base = path.substr(path.find_last_of('/') + 1);
        base = base.substr(0, base.size() - 4);
        auto vary = [] {
            const uint32_t PL = 0x0013f350;
            const uint32_t hm = ee_read<uint32_t>(PL + 0x2080);
            if (ri(0, 3) == 0 && hm != 0) {  // a hit naming the hero
                const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
                w8(hm + 0xa4, idx);
                w32(e + 0x34, ri(0, 8) == 0 ? ee_read<uint32_t>(OV(0x0015ffd8)) : hm);
                w32(e + 0x24, ri(0, 3) == 0 ? R() : u(ri(1, 3)) | (R() & 0xfc));
                { static const float dm[] = {-1.0f, 0.0f, 1.0f, 2.0f, 5.0f}; wf(e + 0x2c, dm[ri(0, 4)]); }
                wfrand(e, 4, -1, 1);
                w32(e + 0x28, u(ri(0, 20)));
            }
            const int k = ri(0, 3);
            if (k == 1) w32(PL + 0x1c0, 0);
            if (k == 2) w32(PL + 0x1c0, u(ri(1, 90)));
            const uint32_t P = 0x0013c940;  // (the pad: held +0x1a0, pressed +0x1c4; cross 0x40, square 0x80, triangle 0x10, circle 0x20)
            static const uint32_t bt[] = {0x40, 0x80, 0x10, 0x20, 0x08, 0x04};
            for (int j = 0; j < 2; ++j)
                if (coin()) {
                    const uint32_t b = bt[ri(0, 5)];
                    w32(P + 0x1a0, ee_read<uint32_t>(P + 0x1a0) | b);
                    if (coin()) w32(P + 0x1c4, ee_read<uint32_t>(P + 0x1c4) | b);
                }
            if (ri(0, 4) == 0) w32(0x001415f8, u(ri(1, 8)));  // (the health)
            w32(OV(0x0015f5cc), ee_read<uint32_t>(OV(0x0015f5cc)) + u(ri(0, 3)));
            seed_rand();
        };
        std::string nm = "frame_" + base;
        {
        std::optional<CollisionScratchIgnored> spr_ignored;
        if (c2c_spr_check(nm.c_str())) spr_ignored.emplace();
        check(nm.c_str(), 0x002070d0, 40, Ret::None, [&d, vary] { b242_load(d); vary(); return Args{}; },
              [](A) { hero_frame_update(); return uint64_t{0}; });
        }
        nm = "state_" + base;
        {
        std::optional<CollisionScratchIgnored> spr_ignored;
        if (c2c_spr_check(nm.c_str())) spr_ignored.emplace();
        check(nm.c_str(), 0x00217970, 40, Ret::None, [&d, vary] { b242_load(d); vary(); return Args{}; },
              [](A) { hero_state_update(); return uint64_t{0}; });
        }
        nm = "input_" + base;
        {
        std::optional<CollisionScratchIgnored> spr_ignored;
        if (c2c_spr_check(nm.c_str())) spr_ignored.emplace();
        check(nm.c_str(), 0x00229b70, 40, Ret::None, [&d, vary] { b242_load(d); vary(); return Args{}; },
              [](A) { hero_input_update(); return uint64_t{0}; });
        }
        nm = "hurt_" + base;
        {
        std::optional<CollisionScratchIgnored> spr_ignored;
        if (c2c_spr_check(nm.c_str())) spr_ignored.emplace();
        check(nm.c_str(), 0x00210ce8, 40, Ret::Int32, [&d, vary] { b242_load(d); vary(); return Args{{u(ri(0, 3))}, {}}; },
              [](A a) { return ri32(hero_hurt(static_cast<int32_t>(a.i[0]))); });
        }
        if (g_oracle_level != 16) continue;
        // Kalebo III's hero callbacks (the hoverboard's draws and updates, installed by its hero_set_state: L16:0x2351f8 .. 0x238e20, as
        // Rilgar's): a0 = the hero's moby, the display packet cursor given a real buffer
        struct Cb { const char* name; uint32_t addr; void (*fn)(uint32_t); };
        static const Cb cbs[] = {
            {"cb2351f8", 0x002351f8, race_hud_start}, {"cb235268", 0x00235268, race_meter_update}, {"cb235660", 0x00235660, kal_cb_235660_l16},
            {"cb2388b0", 0x002388b0, race_hud_reset}, {"cb2388f8", 0x002388f8, race_hud_update}, {"cb238a30", 0x00238a30, kal_cb_238a30_l16},
            {"cb238e20", 0x00238e20, race_hud_draw},
        };
        for (const Cb& cb : cbs) {
            nm = std::string(cb.name) + "_" + base;
            auto fn = cb.fn;
            l16check(nm.c_str(), ORACLE_LADDR(16, cb.addr), 40, Ret::None, [&d, vary] {
                b242_load(d);
                vary();
                w32(OV(0x001611c0), 0x00800000u);  // (the display packet cursor: a real buffer)
                return Args{{ee_read<uint32_t>(0x0013f350 + 0x2080)}, {}};
            }, [fn](A a) { fn(a.i[0]); return uint64_t{0}; });
        }
        // the race's camera (type 8, L16:0x2f51e0 / 0x2f51e8 / 0x2f5260; its exit is Veldin's 0x2ed340): the dump's live camera of the type
        // (its +0x8c is the row in the type table: type 8 is level 16's row 6)
        b242_load(d);
        uint32_t cam8 = 0;
        for (uint32_t i = 0; i < 48 && cam8 == 0; ++i)
            if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0 && ee_read<int16_t>(OV(0x00167150) + i * 0xa0 + 0x8c) == 6) cam8 = OV(0x00167150) + i * 0xa0;
        if (cam8 == 0) { std::printf("cam_type8_%s          no live camera of type 8 in the dump\n", base.c_str()); continue; }
        auto camvary = [cam8] {
            if (ri(0, 2) == 0) { Vec4 q = ee_ref<Vec4>(0x0013f3d0); q.x += rf(-2, 2); q.y += rf(-2, 2); q.z += rf(-1, 1); ee_ref<Vec4>(0x0013f3d0) = q; }
            w8(cam8 + 0x7c, u(ri(0, 3)));
        };
        nm = "cam8_accept_" + base;
        l16check(nm.c_str(), ORACLE_LADDR(16, 0x002f51e0u), 60, Ret::Int32, [&d, vary, camvary, cam8] { b242_load(d); vary(); camvary(); return Args{{cam8, ri(0, 2) == 0 ? 0u : cam8}, {}}; },
              [](A) { return r32(static_cast<uint32_t>(zero_stub())); });
        nm = "cam8_init_" + base;
        l16check(nm.c_str(), ORACLE_LADDR(16, 0x002f51e8u), 60, Ret::None, [&d, vary, camvary, cam8] { b242_load(d); vary(); camvary(); return Args{{cam8}, {}}; },
              [](A a) { race_cam_init(a.i[0]); return uint64_t{0}; });
        nm = "cam8_update_" + base;
        l16check(nm.c_str(), ORACLE_LADDR(16, 0x002f5260u), 60, Ret::None, [&d, vary, camvary, cam8] { b242_load(d); vary(); camvary(); return Args{{cam8}, {}}; },
              [](A a) { race_cam_update(a.i[0]); return uint64_t{0}; });
    }
    g_extra_noop_hooks.clear();
    g_ee_null_is_ram = null_ram;
    g_ignored_words = saved_ignored;
}

}  // namespace rac1::oracle
