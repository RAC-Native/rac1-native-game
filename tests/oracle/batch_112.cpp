// Oracle batch 112 (task 0335): Kerwan's own moby classes (level 3) on its real level data. Run with
// ORACLE_LEVEL=3 build/tests/ovl_oracle --batch 112 (needs the ISO). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the
// original is Kerwan's own function at its own address, the port the one registered for it.

#include "ovl_oracle.h"

#include <game/lib/libcdvd.h>
#include <game/ovl/empty_class_updates.h>
#include <game/ovl/kerwan_classes_tr.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/disc.h>
#include <runtime/sif/sif_rpc.h>

#include <cmath>
#include <cstring>
#include <map>

namespace rac1::oracle {

namespace {
// A check whose original is Kerwan's own function at `l3addr` (not a Veldin address to translate).
void l3check(const char* name, uint32_t l3addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (g_oracle_level != 3) {
        std::printf("%-26s L3:0x%08x  skipped (Kerwan only: run with ORACLE_LEVEL=3)\n", name, l3addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L3:0x%08x\n", name, l3addr);
    if (!check_fn(name, l3addr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_112() {
    if (g_only_batch != 112) return;
    g_batch = 112;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // (task 0385) the IOP calls the originals of Helga / Big Al / Robot Qwark (890 / 909 / 914) reach (a dialogue stream's disc read,
    // sceCdRead 0x121450; the autosave's clock, 0x121780 / 0x12d6d8): their waits the interpreter's original polled forever (at 0x120af0:
    // the instruction budget, the known 7 failures); the host's on both sides, as batches 75 / 85 / 90 do
    const std::time_t saved_clock = g_host_clock_fixed;
    g_host_clock_fixed = 1759658400;
    g_extra_hooks = {{0x00121780, [](EeCpu& c) { c.set32(2, cd_read_clock(c.r32(4))); }},
                     {0x0012d6d8, [](EeCpu& c) { clock_to_local(c.r32(4)); }},
                     {0x00121450, [](EeCpu& c) { c.set32(2, cd_read(c.r32(4), c.r32(5), c.r32(6), c.r32(7))); }}};
    // and 989snd's IOP server answering as batch 75's (the stream system's disc reads, command 0x38, done as the IRX does them: the
    // sectors read into EE RAM, the status words 0x137b10 / 0x137b00 cleared): the talks' scene reads (cutscene_start's
    // stream_reads_wait) complete on both sides
    sif_rpc_register(0x123456, [](uint32_t fno, uint32_t send, uint32_t, uint32_t recv, uint32_t) {
        auto result = [](uint32_t id, uint32_t a0) -> uint32_t {
            const uint32_t h = (id * 0x9e3779b1u) ^ (a0 * 0x85ebca6bu);
            return ((h >> 7) % 4 == 0) ? 0u : ((h >> 3) | 1u);
        };
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

    // the level after its first gameplay frame (the port's), the hero's equipment existing
    static std::vector<uint8_t> snap;
    auto level = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty()) {
            veldin_boot();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            gameplay_frame();
            w32(0x0015ec8c, 1);  // (task 0385) the stream system up (the disc reads by command 0x38), the disc idle, as batch 75's
            w32(0x00137b00, 0);
            sub_00264bf8();
            snap.assign(ram, ram + 0x02000000);
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    // the level's mobys whose update function is `fn` (Kerwan's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 3) { l3check(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        level();
        const std::vector<uint32_t> ms = mobys_with(fn);
        if (ms.empty()) { std::printf("%-26s L3:0x%08x  no moby on this level\n", name, fn); return; }
        l3check(name, fn, n, Ret::None, [level, ms, vary] {
            level();
            const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            vary(m);
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

    // the debris' velocity / spin w lanes: a VU0 register leftover of the original's sin / cos (the port's has none)
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    if (g_oracle_level == 3) level();
    for (uint32_t a = g_oracle_level == 3 ? ee_read<uint32_t>(OV(0x0015ffe0)) : 0u; a < (g_oracle_level == 3 ? ee_read<uint32_t>(OV(0x0015ffe0)) : 0u) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }

    // ---- Fred 1 / 2 / 3 (915..917): the hero anywhere, the moby facing anywhere
    for (int pass = 0; pass < 3; ++pass)
    classcheck("fred_update", 0x002dc310, fred_update_l3, [hero_near](uint32_t m) {
        hero_near(m);
        wf(m + 0x48, rf(-3.1f, 3.1f));
    }, 20);
    // ---- the breakable props: the lightpost (1827), the statue (817)
    for (uint32_t fn : {L3_ADDR(0x002e2948u), L3_ADDR(0x002d3cd8u)})
    for (int pass = 0; pass < 5; ++pass)
    classcheck(fn == L3_ADDR(0x002e2948u) ? "lightpost_update" : "statue_update", fn, fn == L3_ADDR(0x002e2948u) ? lightpost_update_l3 : statue_update_l3, [pass, give_hit, hero_near](uint32_t m) {
        w8(m + 0x20, pass < 4 ? u(pass) : u(ri(0, 3)));
        give_hit(m, 0x10000);
        hero_near(m);
        seed_rand();
    }, 40);
    // (the debris' w lanes stay ignored for the transliterated classes below: they throw debris too)
    // ---- the sign (825): the yaw and the attached frame
    classcheck("robo_shack_sign_update", 0x002d3e58, robo_shack_sign_update_l3, [](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        wfrand(m + 0x40, 4, -3.1f, 3.1f);
        wrand(pv + 0x20, 16);
    }, 30);
    // ---- the garage door (997): the three states, the flag of its index set / clear
    for (int pass = 0; pass < 8; ++pass)
    classcheck("garage_door_update", 0x002dca30, garage_door_update_l3, [pass](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t level_no = ee_read<uint32_t>(0x0015ed84);
        w8(m + 0x20, pass < 4 ? u(pass) : u(ri(0, 3)));
        const uint32_t idx = u(ri(0, 0x7f));
        w16(m + 0xb2, ri(0, 5) == 0 ? (idx | 0x8000u) : idx);
        w8(OV(0x001bb230) + (static_cast<uint32_t>(static_cast<int16_t>(ee_read<uint16_t>(m + 0xb2)))) + 0x454, R() & 1);
        for (uint32_t i = 0; i < 4; ++i) w32(0x0014c190 + level_no * 256 + i * 4, R());
        wf(pv + 8, ri(0, 3) == 0 ? ee_read<float>(m + 0x18) - 4.0f : rf(-20, 100));  // (the rise ends right at the top: the edge)
        wf(m + 0x18, ee_read<float>(pv + 8) + (ri(0, 2) == 0 ? 4.0f - ee_read<float>(0x0015ed6c) : rf(-5, 10)));
    }, 40);
    // ---- the train carriage (1210): the first frame / the later ones, the flags' bit 1
    for (int pass = 0; pass < 6; ++pass)
    classcheck("train_carriage_update", 0x002953f8, train_carriage_update_l3, [pass](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, pass < 2 ? u(pass) : u(ri(0, 1)));
        w16(m + 0x34, ri(0, 4) == 0 ? (R() | 2u) : (R() & ~2u));
        wfrand(pv + 0xa0, 4, -300, 300);
        wfrand(pv + 0xb0, 4, -3.1f, 3.1f);
        wfrand(m + 0x40, 4, -3.1f, 3.1f);
    }, 40);
    // (class 1548: batch 138, task 0385)
    // ---- the spawn point (805), the zone marker (605), the scripted camera trigger (737), the lift (726): Novalis's ports on Kerwan's code
    for (int pass = 0; pass < 3; ++pass)
    classcheck("spawn_point_update", 0x002d1140, spawn_point_update_l1, [pass](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t level_no = ee_read<uint32_t>(0x0015ed84);
        w32(pv + 8, pass == 0 ? R() & 0x1e : (R() & 0x16));
        w8(m + 0xbc, ri(0, 5) == 0 ? 0u : 1u); w8(m + 0x20, R() & 1);
        w32(pv + 0xc, R() & 1); w32(pv + 0x10, R() & 1); w32(pv + 0x14, R() & 1);
        w32(pv + 0x18, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 15)));
        if (ri(0, 3) == 0) w8(m + 0xb0, ri(0, 2) == 0 ? 0xffu : u(ri(0, 15)));
        for (uint32_t i = 0; i < 16; ++i) w8(0x0014c050 + level_no * 16 + i, coin() ? 0xffu : 0u);
        const uint32_t idx = u(ri(0, 0x7f));
        w16(m + 0xb2, idx);
        w8(OV(0x001bb230) + idx + 0x454, R() & 1); w8(OV(0x001ba5d0) + idx + 0x454, R() & 1);
        for (uint32_t i = 0; i < 4; ++i) { w32(0x0014c190 + level_no * 256 + i * 4, R()); w32(OV(0x001ba4d0) + i * 4, R()); }
        static const uint32_t ks[] = {0, 1, 3, 5}, runs[] = {0, 1, 5, 9};
        w8(0x001413f4, ks[ri(0, 3)]); w32(0x001413dc, runs[ri(0, 3)]);
        if (coin()) {
            const int32_t b = ri(0, 3) == 0 ? ri(-1, 3) : static_cast<int32_t>(ee_read<uint32_t>(pv));
            const uint32_t box = ee_read<uint32_t>(OV(0x001600ec)) + u(b << 7);
            if (b >= 0) for (uint32_t k = 0; k < 3; ++k) wf(0x0013f3d0 + 4 * k, ee_read<float>(box + 0x30 + 4 * k) + rf(-0.5f, 0.5f));
        }
        if (ri(0, 3) == 0) w32(pv, u(ri(-1, 3)));
        if (ri(0, 2) == 0) wf(0x0013f3d0 + 8, rf(-50, 500));
    }, 80);
    for (int pass = 0; pass < 4; ++pass)
    classcheck("zone_marker_update", 0x002cbc38, zone_marker_update_l3, [pass](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t P = 0x0013f350;
        w8(m + 0x20, pass == 0 ? u(ri(0, 1)) : (pass == 1 ? 0u : 1u));
        w8(m + 0xbc, ri(0, 3) == 0 ? u(ri(1, 9)) : 0u);
        w32(0x0015ed84, u(ri(0, 0x16)));
        w32(L3_ADDR(0x00161990), u(ri(0, 20)));
        for (uint32_t i = 0; i < 64; ++i) w8(0x0014bf10 + i, R());
        w32(pv + 0x18, u(ri(-3, 40)));
        w32(pv + 0x10, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 5))); w32(pv + 0x14, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 4)));
        { const int32_t b = static_cast<int32_t>(ee_read<uint32_t>(pv + 0x14)); Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-10, 10); q.y += rf(-10, 10);
          if (b >= 0 && coin()) { const uint32_t box = ee_read<uint32_t>(OV(0x001600ec)) + u(b << 7); for (uint32_t k = 0; k < 3; ++k) (&q.x)[k] = ee_read<float>(box + 0x30 + 4 * k) + rf(-0.3f, 0.3f); }
          ee_ref<Vec4>(P + 0x80) = q; }
        w32(P + 0x1090, ri(0, 3) == 0 ? ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(1, 50)) : 0u); w32(P + 0x10b8, coin() ? 0x1bu : u(ri(0, 3)));
        if (ee_read<uint32_t>(P + 0x1090) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(P + 0x1090) + 0x78) == 0) w32(P + 0x1090, 0);
        wf(P + 0x2044, coin() ? 1000.0f : rf(0, 30)); w32(P + 0x2040, 0); w32(P + 0x2048, 0);
    }, 60);
    for (int pass = 0; pass < 6; ++pass)
    classcheck("lift_update", 0x00291dd0, lift_update_l1, [pass, hero_near](uint32_t m) {
        w8(m + 0xbc, u(pass % 3));
        hero_near(m);
        w32(m + 0x20, u(R() & 3));
    }, 30);

    // ---- the transliterated classes (kerwan_classes_tr.cpp): the level's own mobys of each, perturbed generically
    // (the state byte, the timer byte, a hit record, the hero near or far, the random generator)
    struct TrClass { uint32_t fn; const char* name; void (*port)(uint32_t); };
    static const TrClass tr_classes[] = {
        {0x00292e98u, "class822", kerwan_class822_update_l3},
        {0x00294c08u, "class868", kerwan_class868_update_l3},
        {0x0029dba8u, "class75", kerwan_class75_update_l3},
        {0x002bae48u, "class235", kerwan_class235_update_l3},
        {0x002c4718u, "class545", kerwan_class545_update_l3},
        {0x002c5bb8u, "class573", kerwan_class573_update_l3},
        {0x002c6fd0u, "class574", kerwan_class574_update_l3},
        {0x002c9eb8u, "class578", kerwan_class578_update_l3},
        {0x002cb8e8u, "class604", kerwan_class604_update_l3},
        {0x002cbea8u, "class627", kerwan_class627_update_l3},
        {0x002cca00u, "class631", kerwan_class631_update_l3},
        {0x002cf340u, "class737", kerwan_class737_update_l3},
        {0x002d0bd8u, "class758", kerwan_class758_update_l3},
        {0x002d3198u, "class816", kerwan_class816_update_l3},
        {0x002d40e8u, "class832", kerwan_class832_update_l3},
        {0x002d43c8u, "class833", kerwan_class833_update_l3},
        {0x00293c78u, "class845", kerwan_class845_update_l3},
        {0x002d47c0u, "class848", kerwan_class848_update_l3},
        {0x002da870u, "class890", kerwan_class890_update_l3},
        {0x002db020u, "class898", kerwan_class898_update_l3},
        {0x002db280u, "class899", kerwan_class899_update_l3},
        {0x002db558u, "class909", kerwan_class909_update_l3},
        {0x002dbd90u, "class914", kerwan_class914_update_l3},
        {0x002dc478u, "class920", kerwan_class920_update_l3},
        {0x002dc9b0u, "class994", kerwan_class994_update_l3},
        {0x002dccc0u, "class1012", kerwan_class1012_update_l3},
        {0x002de370u, "class1221", kerwan_class1221_update_l3},
        {0x002de458u, "class1222", kerwan_class1222_update_l3},
        {0x002df520u, "class1342", kerwan_class1342_update_l3},
        {0x002e08c8u, "class1633", kerwan_class1633_update_l3},
        {0x002e1970u, "class1818", kerwan_class1818_update_l3},
        {0x002e2a40u, "class1828", empty_update},
    };
    for (const TrClass& c : tr_classes)
    for (int pass = 0; pass < 6; ++pass)
    classcheck(c.name, c.fn, c.port, [pass, give_hit, hero_near](uint32_t m) {
        if (pass == 1) w8(m + 0x20, 0);
        if (pass == 2) w8(m + 0x20, 1);
        give_hit(m, 0x10000 | (R() & 0x3ffffu));
        hero_near(m);
        seed_rand();
    }, 20);

    // T00: the level's bare `jr ra` functions its classes and cameras reach through a pointer (class updates, camera
    // callbacks; the scan pairs them with Veldin's 0x1f6bf8): empty_update runs in their places, each against the original
    // called directly (the level may have no moby of the class)
    if (g_oracle_level == 3) {
        static const uint32_t empties[] = {0x002e2a40};
        for (const uint32_t a : empties) {
            char nm[24];
            std::snprintf(nm, sizeof nm, "empty_%x", a);
            l3check(nm, a, 20, Ret::None, [level] {
                level();
                return Args{{R(), R(), R(), R()}, {rf(-9, 9)}};
            }, [](A a) { empty_update(a.i[0]); return uint64_t{0}; });
        }
    }

    // T00: L3:0x295190, a bare `jr ra` the level's classes call (the scan pairs it with Veldin's 0x1f6bf8, debug_view_2): the
    // empty port runs in its place; the original against it on random argument registers
    if (g_oracle_level == 3)
        l3check("empty_295190", 0x00295190, 40, Ret::None, [level] {
            level();
            return Args{{R(), R(), R(), R()}, {rf(-9, 9)}};
        }, [](A) { debug_view_2(); return uint64_t{0}; });

    g_extra_hooks.clear();
    g_host_clock_fixed = saved_clock;
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
