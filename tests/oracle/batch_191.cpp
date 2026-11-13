// Oracle batch 191 (fix/gameplay, release 0.2.0): Kerwan's helicopter and its bullets (issue #26), the Heli-Pack's start from a jump
// (issue #9) and Clank hit while attacking (issue #32).
//  - Issue #26: Kerwan's helicopter (class 631, L3:0x2cca00) and its missile (class 848, L3:0x2d47c0, spawned by L3:0x2d5208); and the
//    helicopter's bullets: particle type 0x31 (Veldin's update 0x271d20, Kerwan's L3:0x260210, spawned by L3:0x260130 from the 848's
//    update; ported as bullet_particle_update, shared_particles.cpp), on the dumps' live bullets and on new records of the type (frames, position, velocity
//    varied). The subjects of the classes are the dumps' live mobys, their state and the hero's position varied; the original is
//    Kerwan's own function at its own address. ORACLE_LEVEL=3.
//  - Issue #9: Kerwan's hero input (Veldin 0x229b70, Kerwan's variant) during a jump with the Heli-Pack, the pad's pressed history
//    (X pressed 0..30 frames ago, or not) and its held / pressed words varied. ORACLE_LEVEL=3.
//  - Issue #32: the hero's frame update (0x2070d0) and hurt (0x210ce8) with the hero as Clank, on Blarg's Clank section
//    (ORACLE_LEVEL=6, the set clank_0415/ of batch 151) and Orxon's (ORACLE_LEVEL=10), a hit on the hero (the hit table's record
//    naming his moby, a damage, the kinds), his invulnerability word +0x1c0, the attack button (square) and the health varied.
// Run with ORACLE_LEVEL=3, 6 and 10: build/tests/ovl_oracle --batch 191.
// On RAM dumps of headless runs of the port (RAC1_RAMDUMP), from the repository's root, in assets/dumps/oracle/<set>/ (skipped
// without them); every run with RAC1_HEADLESS=1 RAC1_RAMHASH=/dev/null RAC1_FRAMES=<f>+2 RAC1_RAMDUMP=<f>,<set>/<name>.bin:
//   kerwan_heli/m<f>.bin, f = 1205, 1230, 1440, 1600, 1765, and b<f>.bin, f = 1606, 1620, 1765 (the m files were recorded before
//   particle type 0x31 was ported, the b files after: live bullets)
//       RAC1_LEVEL=3 RAC1_HERO_AT='-1@800,284.0,189.4,46.5/-1@1200,252.5,196.2,48.4' RAC1_SIM_INPUT='seq:5000:l3'
//       RAC1_POKE8=820:0x1415f8=8,822:0x1415f8=8,...,2000:0x1415f8=8 (every second frame from 820 to 2000: the health kept full)
//       (the hero put in the helicopter's start box at frame 800, then in its attack box at 1200)
//   kerwan_heli_jump/j<k>.bin, k = 5, 10, 14, 15, 16, 20, 26, 32 (f = 840 + k: a standing jump)
//       RAC1_LEVEL=3 RAC1_GIVE_FRAME=780 RAC1_GIVE_ITEMS=0x1d,0xc,0x2,0xf RAC1_HERO_AT='-1@800,264,141,50.5,1.57'
//       RAC1_SIM_INPUT='seq:840+4:x'
//   kerwan_heli_jump/r<k>.bin, k = 12, 18, 24 (f = 840 + k: a running jump)
//       the same with RAC1_HERO_AT='-1@780,264,141,50.5,1.57' RAC1_SIM_INPUT='seq:830-880:ly=-1;840+4:x'
//   orxon_clank/orxon_idle900.bin (f = 900)          RAC1_LEVEL=10
//   orxon_clank/orxon_atk884.bin, orxon_atk892.bin   RAC1_LEVEL=10 RAC1_SIM_INPUT='seq:860+3:s;872+3:s;880+3:s;888+3:s' (his attack
//                                                    0x51 under way)
//   clank_0415/ring_3015.bin, ring_3815.bin          batch 151's (tools/oracle/blarg_tour.sh clankdumps)

#include "ovl_oracle.h"

#include <game/ovl/shared_particles.h>

#include <game/ovl/kerwan_classes_tr.h>
#include <game/ovl/particle_types_tr.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <system_error>
#include <vector>

namespace rac1::oracle {

namespace {
void k3check(const char* name, uint32_t l3addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (g_oracle_level != 3) {
        std::printf("%-26s L3:0x%08x  skipped (Kerwan only: run with ORACLE_LEVEL=3)\n", name, l3addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L3:0x%08x\n", name, l3addr);
    if (!check_fn(name, l3addr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
const std::vector<uint8_t>& b191_dump(const std::string& path) {
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
void b191_load(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}
// The .bin files of a set (dump_dir()/<set>) whose name starts with `prefix`, sorted.
std::vector<std::string> b191_files(const char* set, const char* prefix = "") {
    const std::string d = dump_path(set);
    std::vector<std::string> v;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(d, ec)) {
        const std::string n = e.path().filename().string();
        if (n.size() > 4 && n.substr(n.size() - 4) == ".bin" && n.rfind(prefix, 0) == 0) v.push_back(d + "/" + n);
    }
    std::sort(v.begin(), v.end());
    return v;
}
std::string b191_base(const std::string& path) {
    const std::string base = path.substr(path.find_last_of('/') + 1);
    return base.substr(0, base.size() - 4);
}
std::vector<uint32_t> b191_live(int cls) {
    std::vector<uint32_t> v;
    for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) < 0xfe && static_cast<int16_t>(ee_read<uint16_t>(m + 0xa6)) == cls) v.push_back(m);
    return v;
}
}  // namespace

void batch_191() {
    if (g_only_batch != 191) return;
    g_batch = 191;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // (the particle update's saved return address)
    g_ignored_words.push_back(OV(0x0016018c));
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    g_extra_noop_hooks = {0x00122298, 0x001232c8, 0x00122fe0, 0x00118a80};

    // ---- issue #26: the helicopter (631) and its missile (848)
    const std::vector<std::string> heli = b191_files("kerwan_heli");
    if (g_oracle_level == 3 && heli.empty())
        skip_check("heli631 / missile848", ORACLE_LADDR(3, 0x002cca00u), "no RAM dump (assets/dumps/oracle/kerwan_heli: batch 191's header)");
    for (const std::string& path : heli) {
        if (g_oracle_level != 3) break;
        const std::vector<uint8_t>& d = b191_dump(path);
        if (d.empty()) continue;
        const std::string base = b191_base(path);
        std::printf("  (on %s)\n", path.c_str());
        b191_load(d);
        const std::vector<uint32_t> missiles = b191_live(848), helis = b191_live(631);
        // the hero's position: as dumped, near the moby, or far
        auto hero_vary = [](uint32_t m) {
            const int k = ri(0, 2);
            if (k == 0) return;
            Vec4 q = VV(m + 0x10);
            const float r = k == 1 ? rf(0.0f, 3.0f) : rf(3.0f, 40.0f);
            const float a = rf(-3.1f, 3.1f);
            q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-3.0f, 2.0f); q.w = 1.0f;
            VV(0x0013f3d0) = q;
            VV(ee_read<uint32_t>(0x001413d0) + 0x10) = q;
        };
        if (!missiles.empty()) {
            const std::string nm = "missile848_" + base;
            k3check(nm.c_str(), ORACLE_LADDR(3, 0x002d47c0u), 60, Ret::None, [&d, missiles, hero_vary] {
                b191_load(d);
                const uint32_t m = missiles[static_cast<size_t>(ri(0, static_cast<int>(missiles.size()) - 1))];
                const int k = ri(0, 5);
                if (k == 1) w8(m + 0x20, 2);  // (exploding)
                if (k == 2) w8(m + 0x20, ri(0, 4));
                if (k == 3) wf(ee_read<uint32_t>(m + 0x78) + 0x8, rf(-0.3f, 0.3f));  // (the vertical speed)
                hero_vary(m);
                w32(OV(0x0015f5cc), R());
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { kerwan_class848_update_l3(a.i[0]); return uint64_t{0}; });
        }
        if (!helis.empty()) {
            const std::string nm = "heli631_" + base;
            k3check(nm.c_str(), ORACLE_LADDR(3, 0x002cca00u), 40, Ret::None, [&d, helis, hero_vary] {
                b191_load(d);
                const uint32_t m = helis[0];
                hero_vary(m);
                if (ri(0, 3) == 0) w16(ee_read<uint32_t>(m + 0x78) + 0x204, ri(0, 3));  // (the fire timer near its end)
                w32(OV(0x0015f5cc), R());
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { kerwan_class631_update_l3(a.i[0]); return uint64_t{0}; });
        }
    }
    // ---- issue #26: the bullets, particle type 0x31
    for (const std::string& path : heli) {
        if (g_oracle_level != 3) break;
        const std::vector<uint8_t>& d = b191_dump(path);
        if (d.empty()) continue;
        const std::string base = b191_base(path);
        b191_load(d);
        std::vector<uint32_t> qs;
        const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
        const int hi = static_cast<int>(ee_read<uint32_t>(OV(0x00160174)));
        for (int i = 0; i <= hi && i < 4096; ++i) {
            const uint32_t q = pool + 0x40u * static_cast<uint32_t>(i);
            if (static_cast<int8_t>(ee_read<uint8_t>(q + 1)) >= 0 && ee_read<uint8_t>(q) == 0x31) qs.push_back(q);
        }
        std::printf("  (on %s: %zu live bullets)\n", path.c_str(), qs.size());
        if (!qs.empty()) {
            const std::string nm = "particle31_live_" + base;
            check(nm.c_str(), 0x00271d20, 60, Ret::None, [&d, qs] {
                b191_load(d);
                const uint32_t q = qs[static_cast<size_t>(ri(0, static_cast<int>(qs.size()) - 1))];
                if (ri(0, 3) == 0) w16(q + 0xa, u(ri(-1, 3)));
                seed_rand();
                return Args{{q}, {}};
            }, [](A a) { bullet_particle_update(a.i[0]); return uint64_t{0}; });
        }
        // new records: about the hero (into the ground or the air), their frames and velocity varied
        const std::string nm = "particle31_new_" + base;
        check(nm.c_str(), 0x00271d20, 80, Ret::None, [&d] {
            b191_load(d);
            const uint32_t q = alloc_particle(0x31);
            const Vec4 h = VV(0x0013f3d0);
            VV(q + 0x10) = Vec4{h.x + rf(-6, 6), h.y + rf(-6, 6), h.z + rf(-2, 4), 1.0f};
            VV(q + 0x20) = VV(q + 0x10);
            VV(q + 0x20).x += rf(-2, 2);
            VV(q + 0x20).z += rf(-2, 2);
            wfrand(q + 0x30, 3, -2.0f, 2.0f);
            w32(q + 0x3c, ri(0, 2) == 0 ? 0u : ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100u * u(ri(0, 40)));
            w16(q + 0xa, u(ri(0, 4) == 0 ? ri(-1, 2) : ri(2, 45)));
            seed_rand();
            return Args{{q}, {}};
        }, [](A a) { bullet_particle_update(a.i[0]); return uint64_t{0}; });
    }
    // ---- issue #9: the glide's start from the jump (hero_input_update on Kerwan, its variant)
    {
        const std::vector<std::string> v = b191_files("kerwan_heli_jump");
        if (g_oracle_level == 3 && v.empty())
            skip_check("heli_input", 0x00229b70, "no RAM dump (assets/dumps/oracle/kerwan_heli_jump: batch 191's header)");
        for (const std::string& path : v) {
            if (g_oracle_level != 3) break;
            const std::vector<uint8_t>& d = b191_dump(path);
            if (d.empty()) continue;
            const std::string nm = "heli_input_" + b191_base(path);
            check(nm.c_str(), 0x00229b70, 60, Ret::None, [&d] {
                b191_load(d);
                const uint32_t P = 0x0013c940;  // (the pad)
                const int k = ri(0, 3);
                if (k != 0) {  // the pressed history: X k frames ago (or other buttons), the count
                    for (uint32_t i = 0; i < 0x1e; ++i) if (coin()) w32(P + 0x1e0 + 4 * i, ri(0, 3) == 0 ? (R() & 0xff) : 0u);
                    const int32_t cur = static_cast<int16_t>(ee_read<uint16_t>(P + 0x18e));
                    const int ago = ri(1, 30);
                    const uint32_t idx = static_cast<uint32_t>((cur - ago + 0x1e) % 0x1e);
                    if (k != 3) w32(P + 0x1e0 + 4 * idx, ee_read<uint32_t>(P + 0x1e0 + 4 * idx) | 0x40u);
                    w32(P + 0x190, u(ri(0, 30)));
                }
                if (ri(0, 3) == 0) w32(P + 0x1a0, ee_read<uint32_t>(P + 0x1a0) ^ 0x40u);
                if (ri(0, 3) == 0) w32(P + 0x1c4, ee_read<uint32_t>(P + 0x1c4) ^ 0x40u);
                seed_rand();
                return Args{};
            }, [](A) { hero_input_update(); return uint64_t{0}; }, OV(0x00173e7c));
        }
    }
    // ---- issue #32: Clank hit while attacking (levels 6 and 10)
    if (g_oracle_level == 6 || g_oracle_level == 10) {
        std::vector<std::string> v;
        const char* set = g_oracle_level == 6 ? "clank_0415" : "orxon_clank";
        if (g_oracle_level == 6) {
            for (const char* f : {"clank_0415/ring_3015.bin", "clank_0415/ring_3815.bin"})
                if (!b191_dump(dump_path(f)).empty()) v.push_back(dump_path(f));
        } else {
            v = b191_files("orxon_clank", "orxon_");
        }
        if (v.empty()) {
            const std::string why = std::string("no RAM dump (assets/dumps/oracle/") + set + ": batch 191's header)";
            skip_check("clank_frame / clank_hurt", 0x002070d0, why.c_str());
        }
        for (const std::string& path : v) {
            const std::vector<uint8_t>& d = b191_dump(path);
            if (d.empty()) continue;
            const std::string base = b191_base(path);
            auto vary = [] {
                const uint32_t PL = 0x0013f350;
                const uint32_t hm = ee_read<uint32_t>(PL + 0x2080);
                if (ri(0, 3) != 0 && hm != 0) {  // a hit naming the hero
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
                const uint32_t P = 0x0013c940;  // (the pad: square held / pressed)
                if (coin()) { w32(P + 0x1a0, ee_read<uint32_t>(P + 0x1a0) | 0x80u); w32(P + 0x1c4, ee_read<uint32_t>(P + 0x1c4) | (coin() ? 0x80u : 0u)); }
                if (ri(0, 4) == 0) w32(0x001415f8, u(ri(1, 8)));  // (the health)
                seed_rand();
            };
            std::string nm = "clank_frame_" + base;
            check(nm.c_str(), 0x002070d0, 40, Ret::None, [&d, vary] {
                b191_load(d);
                vary();
                return Args{};
            }, [](A) { hero_frame_update(); return uint64_t{0}; }, OV(0x00173e7c));
            nm = "clank_hurt_" + base;
            check(nm.c_str(), 0x00210ce8, 60, Ret::Int32, [&d, vary] {
                b191_load(d);
                vary();
                return Args{{u(ri(0, 3))}, {}};
            }, [](A a) { return ri32(hero_hurt(static_cast<int32_t>(a.i[0]))); }, OV(0x00173e7c));
        }
    }
    g_extra_noop_hooks.clear();
    g_ee_null_is_ram = null_ram;
    g_ignored_words = saved_ignored;
}

}  // namespace rac1::oracle
