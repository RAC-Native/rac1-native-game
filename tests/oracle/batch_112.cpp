// Oracle batch 112 (task 0335): Kerwan's own moby classes (level 3) on its real level data. Run with
// ORACLE_LEVEL=3 build/tests/ovl_oracle --batch 112 (needs the ISO). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the
// original is Kerwan's own function at its own address, the port the one registered for it.
// Stage 4 (P3.1): the hand ports of the train (822), the shuttles (868), the air traffic (75 ...) and class 573 through their
// states (train822_states, shuttle868_states, traffic75_states, class573_states), and direct checks of the helpers other code
// reaches (walker_group_busy, glued into class 574's generated update; level_sound_start; traffic_horn; traffic_puff_spawn).

#include "ovl_oracle.h"

#include <game/level/level_data.h>
#include <game/lib/libcdvd.h>
#include <game/ovl/empty_class_updates.h>
#include <game/ovl/kerwan_classes_tr.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/shared_class1221.h>
#include <game/ovl/shared_class235.h>
#include <game/ovl/shared_class447.h>
#include <game/ovl/shared_class604.h>
#include <game/ovl/shared_class631.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_class848.h>
#include <game/ovl/shared_class994.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_spawner.h>
#include <game/ovl/shared_swingshot_target.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/disc.h>
#include <runtime/sif/sif_rpc.h>

#include <algorithm>
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

    // ---- the classes kerwan_mobys.cpp registers (the transliterated ones of kerwan_classes_tr.cpp and, since the re-glue R3,
    // the shared ports that took the place of Kerwan's copies): the level's own mobys of each, perturbed generically
    // (the state byte, the timer byte, a hit record, the hero near or far, the random generator)
    struct TrClass { uint32_t fn; const char* name; void (*port)(uint32_t); };
    static const TrClass tr_classes[] = {
        {0x00292e98u, "class822", kerwan_class822_update_l3},
        {0x00294c08u, "class868", kerwan_class868_update_l3},
        {0x0029dba8u, "class75", kerwan_class75_update_l3},
        {0x002bae48u, "class235", class235_update},
        {0x002c4718u, "class545", class545_update},
        {0x002c5bb8u, "class573", kerwan_class573_update_l3},
        {0x002c6fd0u, "class574", kerwan_class574_update_l3},
        {0x002c9eb8u, "class578", kerwan_class578_update_l3},
        {0x002cb8e8u, "class604", class604_update},
        {0x002cbea8u, "class627", kerwan_class627_update_l3},
        {0x002cca00u, "class631", class631_update},
        {0x002cf340u, "class737", scripted_camera_trigger_update_l1},
        {0x002d0bd8u, "class758", swingshot_target_update},
        {0x002d3198u, "class816", kerwan_class816_update_l3},
        {0x002d40e8u, "class832", class832_update},
        {0x002d43c8u, "class833", kerwan_class833_update_l3},
        {0x00293c78u, "class845", jet_update_base_l1},
        {0x002d47c0u, "class848", class848_update},
        {0x002da870u, "class890", kerwan_class890_update_l3},
        {0x002db020u, "class898", kerwan_class898_update_l3},
        {0x002db280u, "class899", kerwan_class899_update_l3},
        {0x002db558u, "class909", kerwan_class909_update_l3},
        {0x002dbd90u, "class914", kerwan_class914_update_l3},
        {0x002dc478u, "class920", class447_update},
        {0x002dc9b0u, "class994", class994_update},
        {0x002dccc0u, "class1012", kerwan_class1012_update_l3},
        {0x002de370u, "class1221", class1221_update},
        {0x002de458u, "class1222", class1221_update},
        {0x002df520u, "class1342", kerwan_class1342_update_l3},
        {0x002e08c8u, "class1633", class1633_update},
        {0x002e1970u, "class1818", sandmouse_update},
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

    // ---- (P3.1) the train (822), the shuttles (868 / 905 / 928), the air traffic (75, 115 .. 120, 132) and class 573, each through
    // its states with the state its branches read built from the level: the hero in the boxes the train waits on and standing on
    // it, the sound slots playing the moby's sound or another's, the timers and counters at their edges, the moby positions next
    // to the path points they head for, the camera near the traffic (its smoke and horn), hits of the kinds each takes, the
    // traffic group off screen, class 573's leader (a class 574) in the states it waits for and its group in the states it
    // counts; the frame scale at its 50 Hz value in a quarter of the cases. `B112_INFO=1` prints the subjects.
    {
        const bool info = std::getenv("B112_INFO") != nullptr;
        // the debris' w lanes left out above (debris_spawn's known difference) only for the two checks whose classes throw debris
        // (the air traffic's hit, class 573's burst); the others compare every word
        const std::vector<uint32_t> debris_lanes = g_ignored_words;
        g_ignored_words = saved_ignored;
        const uint32_t P = 0x0013f350;
        auto table = [] { return ee_read<uint32_t>(OV(0x0015ffd8)); };
        auto vary_frame_scale = [] {
            if (ri(0, 3) == 0) { wf(0x0015ed68, 1.2f); wf(0x0015ed6c, 0.02f); wf(0x0015ed70, 0.0004f); }  // (the frame time 1/50 s and its square)
        };
        // a sound slot word: none, a slot playing this moby's sound, one playing another's, or a random slot as it is
        auto sound_slot = [](uint32_t m, uint32_t word) {
            const uint32_t s = u(ri(0, 29)), e = 0x0013e550 + s * 0x70;
            switch (ri(0, 3)) {
            case 0: w32(word, 0xffffffffu); break;
            case 1: w32(word, s); w32(e + 0x88, m); w8(e + 0x74, coin() ? 1u : 0u); break;
            case 2: w32(word, s); w32(e + 0x88, m + 0x100); w8(e + 0x74, 1); break;
            default: break;
            }
        };
        // a timer word / halfword at its edges (idle, about to expire, running) or as it is
        auto timer32 = [](uint32_t a) { static const int32_t v[] = {0, 1, 2, 3, 40}; if (ri(0, 4) != 0) w32(a, u(v[ri(0, 4)])); };
        auto timer16 = [](uint32_t a) { static const int32_t v[] = {0, 1, 2, 3, 40}; if (ri(0, 4) != 0) w16(a, u(v[ri(0, 4)])); };
        // the hero at `at` (his moby too), standing on `on` or not
        auto hero_at = [P](const Vec4& at, uint32_t on) {
            ee_ref<Vec4>(P + 0x80) = at;
            ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10) = at;
            w32(P + 0x2fc, on);
            w32(P + 0x4f8, on);
            w16(P + 0x30e, 0);
        };
        auto box_centre = [](int32_t i) { return i < 0 ? Vec4{} : ee_ref<Vec4>(lvd::box(lvd::BOXES, i) + 0x30); };
        auto moby_at = [table](int32_t i) { return table() + (static_cast<uint32_t>(i) << 8); };
        if (g_oracle_level == 3 && info) {
            level();
            for (uint32_t m : mobys_with(0x00292e98u)) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                std::printf("  B112 train %x bc %u:", m, ee_read<uint8_t>(m + 0xbc));
                for (uint32_t k : {0xa0u, 0xa4u, 0xa8u, 0xacu, 0xb0u, 0xb4u, 0xc4u, 0xd4u, 0xd8u, 0x100u}) std::printf(" +%x=%d", k, static_cast<int32_t>(ee_read<uint32_t>(pv + k)));
                std::printf(" c0 %g c8 %g\n", ee_read<float>(pv + 0xc0), ee_read<float>(pv + 0xc8));
            }
            for (uint32_t fn : {0x00292e98u, 0x00294c08u, 0x0029dba8u, 0x002c5bb8u, 0x002c6fd0u}) {
                std::map<int, int> n;
                for (uint32_t m : mobys_with(fn)) ++n[static_cast<int16_t>(ee_read<uint16_t>(m + 0xa6))];
                std::printf("  B112 fn %x:", fn);
                for (const auto& [c, k] : n) std::printf(" class %d x%d", c, k);
                std::printf("\n");
            }
        }

        // the train (822): its state +0xbc 0 .. 7, the hero in its boxes (+0xb4 the station's, +0xb0 the stop's) or riding it, his
        // state 0x1413dc, the cameras +0x100 / +0xd8 taken or free, the stations' class 574 mobys (+0xe0, eight) in the states it
        // tests, its speeds near their ends, the engine's leg (+0x60) about the stop's (+0xd4), the hero below the track
        // (the focused passes: 11 the hero 60 frames below the track (the ride's end), 12 the hero riding with the return set up,
        // 13 the return's stop at its leg with the speed near 0, 14 the hero in the stop's box, 15 the hero in the station's box in
        // each state the start tests)
        for (int pass = 0; pass < 16; ++pass)
        classcheck("train822_states", 0x00292e98, kerwan_class822_update_l3, [=](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            static const uint32_t focus_state[] = {4, 4, 5, 6, 3};
            w8(m + 0xbc, pass < 8 ? u(pass) : (pass < 11 ? u(ri(0, 7)) : focus_state[pass - 11]));
            if (ri(0, 11) == 0) w32(pv + 0xa4, 0xffffffffu);
            const int32_t eng = static_cast<int32_t>(ee_read<uint32_t>(pv + 0xc4));
            const uint32_t epv = eng >= 0 ? ee_read<uint32_t>(moby_at(eng) + 0x78) : 0u;
            Vec4 at = ee_ref<Vec4>(m + 0x10);
            switch (ri(0, 4)) {
            case 0: at = box_centre(static_cast<int32_t>(ee_read<uint32_t>(pv + 0xb4))); break;
            case 1: at = box_centre(static_cast<int32_t>(ee_read<uint32_t>(pv + 0xb0))); break;
            case 2: at.z += 1.0f; break;
            case 3: at.x += rf(-20, 20); at.y += rf(-20, 20); at.z += coin() ? -15.0f : rf(-5, 5); break;
            default: at.x += rf(-60, 60); at.y += rf(-60, 60); break;
            }
            at.x += rf(-0.3f, 0.3f); at.y += rf(-0.3f, 0.3f);
            hero_at(at, coin() ? m : (coin() ? moby_at(ri(0, 60)) : 0u));
            { static const uint32_t hs[] = {0, 1, 2, 3, 5, 8, 9, 0xc}; w32(0x001413dc, hs[ri(0, 7)]); }
            w16(P + 0x30e, ri(0, 5) == 0 ? 1u : 0u);
            w16(0x0013f65e, coin() ? 1u : 0u);
            timer32(pv + 0x104);
            sound_slot(m, pv + 0x108);
            if (coin()) w32(pv + 0x10c, u(ri(55, 70)));
            for (uint32_t k : {0x100u, 0xd8u}) {
                const int32_t c = static_cast<int32_t>(ee_read<uint32_t>(pv + k));
                if (c >= 0) w8(moby_at(c) + 0xbc, coin() ? 1u : 0u);
                if (ri(0, 9) == 0) w32(pv + k, 0xffffffffu);
            }
            if (ri(0, 9) == 0) w32(pv + 0xac, 0xffffffffu);
            if (ri(0, 9) == 0) w32(pv + 0xd4, 0xffffffffu);
            for (uint32_t k = 0; k < 8; ++k) {
                const int32_t i = static_cast<int32_t>(ee_read<uint32_t>(pv + 0xe0 + 4 * k));
                if (i < 0) continue;
                static const uint32_t st[] = {0xe, 0x10, 0x11, 3, 0xfe, 0};
                if (coin()) w8(moby_at(i) + 0x20, st[ri(0, 5)]);
            }
            const float top = ee_read<float>(pv + 0xc0);
            switch (ri(0, 3)) {
            case 0: wf(pv + 0xc8, rf(0, 0.01f)); break;
            case 1: wf(pv + 0xc8, top - rf(0, 0.01f)); break;
            case 2: wf(pv + 0xc8, rf(0, top)); break;
            default: break;
            }
            if (epv != 0 && coin()) w32(epv + 0x60, u(static_cast<int32_t>(ee_read<uint32_t>(pv + 0xd4)) + ri(-2, 1)));
            vary_frame_scale();
            if (pass == 11) {
                Vec4 below = ee_ref<Vec4>(m + 0x10);
                below.z -= rf(5, 20);
                hero_at(below, 0);
                w32(0x001413dc, 0);
                w16(0x0013f65e, 1);
                w32(pv + 0x10c, u(ri(45, 70)));
            } else if (pass == 12) {
                hero_at(ee_ref<Vec4>(m + 0x10), m);
                w32(0x001413dc, 0);
                w16(0x0013f65e, 0);
                const int32_t c = static_cast<int32_t>(ee_read<uint32_t>(pv + 0x100));
                if (c >= 0 && ri(0, 3) != 0) w8(moby_at(c) + 0xbc, 0);
            } else if (pass == 13) {
                if (epv != 0) w32(epv + 0x60, u(static_cast<int32_t>(ee_read<uint32_t>(pv + 0xd4)) + ri(0, 1)));
                wf(pv + 0xc8, rf(0, 0.004f));
            } else if (pass == 15) {
                Vec4 at = box_centre(static_cast<int32_t>(ee_read<uint32_t>(pv + 0xb4)));
                at.x += rf(-0.3f, 0.3f);
                hero_at(at, 0);
                static const uint32_t hs[] = {0, 1, 2, 8, 9, 0xb, 0xc, 0xd};
                w32(0x001413dc, hs[ri(0, 7)]);
            } else if (pass == 14) {
                Vec4 at = box_centre(static_cast<int32_t>(ee_read<uint32_t>(pv + 0xb0)));
                at.x += rf(-0.3f, 0.3f);
                hero_at(at, 0);
            }
            seed_rand();
        }, 40);

        // the shuttles (classes 868, 905, 928 run 0x294c08): their state 0 .. 4, the timers +0x100 / +0xe8, the shuttle's count
        // +0x10c about its half and its end, the direction +0xb4, the leg +0xb0, the mode +0x108, the waits +0xf0, the sound slot +0x104,
        // the moby next to the point it heads for, the speed +0xe4
        for (int pass = 0; pass < 9; ++pass)
        classcheck("shuttle868_states", 0x00294c08, kerwan_class868_update_l3, [=](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            vary_frame_scale();
            w8(m + 0x20, pass < 5 ? u(pass) : (pass < 7 ? 3u : u(ri(0, 4))));
            timer32(pv + 0x100);
            timer32(pv + 0xe8);
            {
                const int32_t full = ee_read<float>(0x0015ed68) == 1.2f ? 200 : 240;
                const int32_t picks[] = {ri(0, 3), full / 2 + ri(-2, 2), full + ri(-2, 1), ri(0, full)};
                if (ri(0, 4) != 0) w32(pv + 0x10c, u(picks[ri(0, 3)]));
            }
            if (coin()) w8(pv + 0xb4, coin() ? 1u : 0xffu);
            if (coin()) w32(pv + 0xf8, coin() ? 1u : 0u);
            if (coin()) w32(pv + 0x108, coin() ? 1u : 0u);
            if (ri(0, 2) == 0) w32(pv + 0xf0, u(ri(-1, 1) * 30));
            sound_slot(m, pv + 0x104);
            const uint32_t path = ee_read<uint32_t>(pv + 0xc0);
            const int32_t n = path != 0 ? static_cast<int32_t>(ee_read<uint32_t>(path)) : 0;
            if (n > 0 && n < 4096) {
                if (coin()) w32(pv + 0xb0, u(ri(0, n - 1)));
                if (coin()) {
                    const int32_t i = ri(0, n - 1);
                    Vec4 q = ee_ref<Vec4>(path + 0x10 + static_cast<uint32_t>(i) * 16);
                    q.x += rf(-1, 1); q.y += rf(-1, 1); q.z += rf(-1, 1);
                    ee_ref<Vec4>(m + 0x10) = q;
                }
            }
            if (coin()) wf(pv + 0xe4, rf(0, 2) * ee_read<float>(pv + 0xec));
            if (coin()) wf(pv + 0xf4, rf(0, 3));  // (the braking distance: the run's end beyond it or within)
            if (ri(0, 2) == 0) wf(pv + 0xe4, ee_read<float>(0x0015ed6c) * rf(0.2f, 1.2f));  // (the speed near its targets)
            hero_near(m);
            seed_rand();
        }, 30);

        // the air traffic (classes 75, 115 .. 120, 132 run 0x29dba8): the state 0 .. 3, on screen or not (the camera at the moby or
        // away), the group all off screen, the frame counter on the moby's eighth, the timers +0x14a / +0x12c / +0x148, the puffs'
        // countdowns +0x12e / +0x12f, the leg's phase +0x100 about 1, the sway +0x13c / +0x134, the drifts +0x140 / +0x144 over 500,
        // the path -1, the follower's start, hits (kind 0xa30000, the bolts' bit 0x800000, the halfword 0x102), the help flag
        g_ignored_words = debris_lanes;
        for (int pass = 0; pass < 12; ++pass)
        classcheck("traffic75_states", 0x0029dba8, kerwan_class75_update_l3, [=](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, pass < 4 ? u(pass) : (pass < 10 ? 1u : u(ri(0, 5))));
            // the class number among those that run this update (each class's own sound, smoke and debris branches)
            { static const uint16_t cls[] = {75, 115, 116, 117, 118, 119, 120, 132, 795}; if (ri(0, 2) == 0) w16(m + 0xa6, cls[ri(0, 8)]); }
            // a slow frame (its move under 40 x the frame time: the smoke and sparks) in half the cases
            if (coin()) { Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-0.2f, 0.2f); q.y += rf(-0.2f, 0.2f); q.z += rf(-0.1f, 0.1f); ee_ref<Vec4>(pv + 0xd0) = q; }
            if (coin()) w8(m + 0x31, coin() ? 1u : 0u);
            {
                Vec4 cam = ee_ref<Vec4>(m + 0x10);
                static const float edges[] = {15.0f, 75.0f, 90.0f};
                const float r = ri(0, 3) == 0 ? edges[ri(0, 2)] + rf(-1.5f, 1.5f) : (ri(0, 2) == 0 ? rf(16, 70) : (coin() ? rf(0, 14) : rf(80, 300)));
                const float a = rf(-3.1f, 3.1f);
                cam.x += r * std::cos(a); cam.y += r * std::sin(a);
                ee_ref<Vec4>(OV(0x00166dc0)) = cam;
            }
            if (coin()) w32(OV(0x0015f5cc), (ee_read<uint32_t>(OV(0x0015f5cc)) & ~7u) | ((m >> 8) & 7u));
            if (ri(0, 3) == 0) w8(m + 0x21, 0xff);
            timer16(pv + 0x14a);
            timer16(pv + 0x12c);
            if (ri(0, 3) == 0) w16(pv + 0x148, 0xffffu); else timer16(pv + 0x148);
            if (coin()) w8(pv + 0x12e, u(ri(0, 4)));
            if (coin()) w8(pv + 0x12f, u(ri(0, 4)));
            sound_slot(m, pv + 0x150);
            if (coin()) wf(pv + 0x100, rf(0.9f, 1.1f));
            switch (ri(0, 4)) {
            case 0: wf(pv + 0x13c, rf(6.3f, 400.0f)); break;
            case 1: wf(pv + 0x13c, -1.0f); break;
            default: break;
            }
            if (coin()) wf(pv + 0x134, coin() ? 0.0f : rf(-2, 2));
            if (ri(0, 3) == 0) wf(pv + 0x140, rf(400, 900));
            if (ri(0, 3) == 0) wf(pv + 0x144, rf(400, 900));
            if (ri(0, 9) == 0) w32(pv + 0x74, 0xffffffffu);
            if (coin()) w8(pv + 0x65, coin() ? 0u : 2u);
            if (coin()) w32(pv + 0x60, u(ri(-1, 3)));
            if (ri(0, 3) == 0) w16(m + 0x34, ee_read<uint16_t>(m + 0x34) ^ 1u);
            { static const uint32_t kinds[] = {0x800000, 0x200000, 0x20000, 0x10000, 0xa30000}; give_hit(m, kinds[ri(0, 4)]); }
            if (ee_read<uint8_t>(m + 0xa4) != 0xff && ri(0, 3) == 0) w16(OV(0x00178100) + (u(ee_read<uint8_t>(m + 0xa4)) << 6) + 0x28, 0x102);
            w8(0x0013d40c, coin() ? 1u : 0u);
            hero_near(m);
            vary_frame_scale();
            if (info) {
                Vec4 d{};
                vec3_sub(d, ee_ref<Vec4>(pv + 0xd0), ee_ref<Vec4>(m + 0x10));
                std::printf("  B112 traffic %x cls %d st %u step %g dt %g cam %g 12e %u 12f %u 31 %u\n", m, static_cast<int16_t>(ee_read<uint16_t>(m + 0xa6)),
                            ee_read<uint8_t>(m + 0x20), std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z), ee_read<float>(0x0015ed6c),
                            vec3_distance(ee_ref<Vec4>(m + 0x10), ee_ref<Vec4>(OV(0x00166dc0))), ee_read<uint8_t>(pv + 0x12e), ee_read<uint8_t>(pv + 0x12f), ee_read<uint8_t>(m + 0x31));
            }
            seed_rand();
        }, 30);

        // class 573: its state 0 .. 9, 21 (and the tail's others), its leader (+0x248, a class 574) in the states and animation
        // it waits for, the alert +0x38 and its timer +0x240, the timers +0x244 / +0x23c, its path +0x270 (and leg +0x278), its box
        // +0x27c and radius +0x274, its animations +0x52 / +0x53 (the attack's frame 25), the animation's end bit, its group's
        // members in the states the group test counts, its height under 2, the hero within its reach or not, hits (kind 0x330000)
        if (g_oracle_level == 3) level();
        const std::vector<uint32_t> leaders = g_oracle_level == 3 ? mobys_with(0x002c6fd0u) : std::vector<uint32_t>{};
        // (pass 13: the strike's frame: the target right ahead at 1, level, the animation at frame 25)
        g_ignored_words = debris_lanes;
        // (and the w lanes enemy_physics 0x25c698 leaves in the knocked-off or falling moby's position (+0x1c) and in the collision's
        // segment and contact records (0x173e40 .. 0x173fc0, +0xc): the engine port's known difference, a VU0 register leftover of
        // the original's segment test (COMMON's list; batches 138, 514); class 573's own code never reads them)
        for (uint32_t a = OV(0x00173e40); a < OV(0x00173fc0); a += 0x10) g_ignored_words.push_back(a + 0xc);
        for (uint32_t m : g_oracle_level == 3 ? mobys_with(0x002c5bb8u) : std::vector<uint32_t>{}) g_ignored_words.push_back(m + 0x1c);
        for (int pass = 0; pass < 21; ++pass)
        classcheck("class573_states", 0x002c5bb8, kerwan_class573_update_l3, [=](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            static const uint32_t sts[] = {0, 1, 1, 1, 2, 2, 2, 3, 4, 4, 5, 5, 5, 6, 6, 7, 8, 9, 21, 12};
            w8(m + 0x20, pass < 20 ? sts[pass] : u(ri(0, 21)));
            switch (ri(0, 3)) {
            case 0: w32(pv + 0x248, 0xffffffffu); break;
            case 1: if (!leaders.empty()) w32(pv + 0x248, (leaders[static_cast<size_t>(ri(0, static_cast<int>(leaders.size()) - 1))] - table()) >> 8); break;
            default: break;
            }
            const int32_t li = static_cast<int32_t>(ee_read<uint32_t>(pv + 0x248));
            if (li >= 0) {
                const uint32_t l = moby_at(li), lpv = ee_read<uint32_t>(l + 0x78);
                static const uint32_t lst[] = {10, 13, 0, 5, 9, 0xfe};
                if (coin()) w8(l + 0x20, lst[ri(0, 5)]);
                if (coin()) { w8(l + 0x52, 10); wf(l + 0x54, 0.0f); w16(ee_read<uint32_t>(l + 0x68) + 4, u(ri(140, 180))); }
                if (lpv != 0 && coin()) w32(lpv + 0x284, coin() ? 0xffffffffu : 0u);
            }
            if (coin()) w32(pv + 0x38, coin() ? 1u : 0u);
            timer32(pv + 0x240);
            timer32(pv + 0x244);
            timer32(pv + 0x23c);
            if (ri(0, 3) == 0 && ee_read<uint8_t>(m + 0x20) != 5) w32(pv + 0x270, 0xffffffffu);
            else if (ri(0, 2) == 0 || (ee_read<uint8_t>(m + 0x20) == 5 && static_cast<int32_t>(ee_read<uint32_t>(pv + 0x270)) < 0))
                w32(pv + 0x270, u(ri(0, std::max(0, lvd::path_count() - 1))));
            {
                const int32_t pi = static_cast<int32_t>(ee_read<uint32_t>(pv + 0x270));
                const uint32_t path = pi >= 0 ? lvd::path(pi) : 0u;
                const int32_t n = path != 0 ? static_cast<int32_t>(ee_read<uint32_t>(path)) : 0;
                if (n > 0 && n < 4096 && coin()) {
                    const int32_t k = ri(0, n - 1);
                    w32(pv + 0x278, u(k));
                    if (coin()) { Vec4 q = ee_ref<Vec4>(path + 0x10 + static_cast<uint32_t>(k) * 16); q.x += rf(-0.5f, 0.5f); q.y += rf(-0.5f, 0.5f); ee_ref<Vec4>(m + 0x10) = q; }
                }
            }
            if (ri(0, 3) == 0) wf(pv + 0x274, rf(0, 40));
            if (ri(0, 3) == 0) wf(pv + 0x230, rf(0, 40));
            { static const uint32_t an[] = {0, 1, 2, 3, 5}; if (coin()) w8(m + 0x53, an[ri(0, 4)]); }
            if (ri(0, 2) == 0) { w8(m + 0x52, 5); wf(m + 0x54, 0.0f); w16(ee_read<uint32_t>(m + 0x68) + 4, u(400 + ri(-4, 4))); }
            if (coin()) w8(m + 0x70, ee_read<uint8_t>(m + 0x70) ^ 2u);
            if (ri(0, 3) == 0) w8(m + 0x21, coin() ? 0xffu : u(ri(0, 20)));
            if (ri(0, 4) == 0) wf(m + 0x18, rf(-2, 3));
            // the hero (the target) right at it and level, within its reach, or away
            {
                Vec4 at = ee_ref<Vec4>(m + 0x10);
                const int k = ri(0, 3);
                const float r = k == 0 ? rf(0.5f, 1.9f) : (k == 1 ? rf(2, 12) : (k == 2 ? rf(1.0f, 8.0f) : rf(12, 50)));
                const float a = coin() ? ee_read<float>(m + 0x48) + rf(-0.2f, 0.2f) : rf(-3.1f, 3.1f);
                at.x += r * std::cos(a); at.y += r * std::sin(a); at.z += coin() ? rf(-0.5f, 0.5f) : rf(-6, 6);
                hero_at(at, 0);
                w32(P + 0x208c, 0);
            }
            give_hit(m, 0x330000);
            if (coin()) w8(m + 0x31, coin() ? 1u : 0u);
            if (coin()) { Vec4 c = ee_ref<Vec4>(m + 0x10); c.x += rf(-40, 40); c.y += rf(-40, 40); ee_ref<Vec4>(OV(0x00166dc0)) = c; }
            vary_frame_scale();
            if ((ee_read<uint8_t>(m + 0x20) == 1 || ee_read<uint8_t>(m + 0x20) == 7) && coin()) {  // the target by its home, level
                Vec4 at = ee_ref<Vec4>(pv + 0x200);
                at.x += rf(-2.5f, 2.5f); at.y += rf(-2.5f, 2.5f); at.z = ee_read<float>(m + 0x18) + rf(-1, 1);
                hero_at(at, 0);
                w32(P + 0x208c, 0);
                wf(pv + 0x230, rf(4, 12));
                wf(pv + 0x274, rf(4, 12));
                if (coin()) w32(pv + 0x244, 0);
            }
            if (pass == 13) {
                const float yaw = ee_read<float>(m + 0x48);
                Vec4 at = ee_ref<Vec4>(m + 0x10);
                at.x += std::cos(yaw) * rf(0.8f, 1.6f); at.y += std::sin(yaw) * rf(0.8f, 1.6f); at.z += rf(-0.3f, 0.3f);
                hero_at(at, 0);
                w32(P + 0x208c, 0);
                wf(pv + 0x230, 10.0f);
                w8(m + 0x52, 5); wf(m + 0x54, 0.0f); w16(ee_read<uint32_t>(m + 0x68) + 4, u(400 + ri(-3, 3)));
                w8(m + 0xa4, 0xff);
            }
            seed_rand();
        }, 30);

        // ---- (P3.2) class 574 (the Blarg trooper: rifle shots of class 833, a blow, jumps, rides the train's carriages) and class 578
        // (the mine layer: lays class 627 mines along its path), each through its states, and the trooper with the class 573s that
        // follow it run together (an alert, the leader's call)
        {
            const std::vector<uint32_t> troopers = g_oracle_level == 3 ? mobys_with(0x002c6fd0u) : std::vector<uint32_t>{};
            const std::vector<uint32_t> carriages = g_oracle_level == 3 ? [&] { std::vector<uint32_t> v; for (uint32_t m : mobys_with(0x002953f8u)) v.push_back(m); return v; }() : std::vector<uint32_t>{};
            if (g_oracle_level == 3 && info) {
                level();
                for (uint32_t fn : {0x002c5bb8u, 0x002c6fd0u, 0x002c9eb8u}) {
                    for (uint32_t m : mobys_with(fn)) {
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        std::printf("  B112 %x cls %d st %u at (%.1f %.1f %.1f) scale %g", m, static_cast<int16_t>(ee_read<uint16_t>(m + 0xa6)), ee_read<uint8_t>(m + 0x20),
                                    ee_read<float>(m + 0x10), ee_read<float>(m + 0x14), ee_read<float>(m + 0x18), ee_read<float>(m + 0x2c));
                        if (fn == 0x002c5bb8u) std::printf(" leader %d path %d reach %g", static_cast<int32_t>(ee_read<uint32_t>(pv + 0x248)), static_cast<int32_t>(ee_read<uint32_t>(pv + 0x270)), ee_read<float>(pv + 0x22c));
                        if (fn == 0x002c6fd0u) {
                            for (uint32_t k : {0x134u, 0x262u, 0x264u, 0x274u, 0x280u, 0x284u, 0x290u, 0x29cu}) std::printf(" +%x=%d", k, k == 0x262u ? ee_read<uint8_t>(pv + k) : static_cast<int32_t>(ee_read<uint32_t>(pv + k)));
                            std::printf(" ranges %g %g %g %g health %g", ee_read<float>(pv + 0x270), ee_read<float>(pv + 0x278), ee_read<float>(pv + 0x27c), ee_read<float>(pv + 0x294), ee_read<float>(pv + 0x20));
                        }
                        if (fn == 0x002c9eb8u) std::printf(" path %d", static_cast<int32_t>(ee_read<uint32_t>(pv + 0x134)));
                        std::printf("\n");
                    }
                }
            }
            // the debris' w lanes (debris_spawn's known difference) and the w lanes enemy_physics 0x25c698 leaves in the subject's
            // position and in the segment / contact records 0x173e40 .. 0x173fc0 (the engine ports' known differences): left out of
            // these checks only, whose classes throw debris and fall through enemy_physics
            g_ignored_words = debris_lanes;
            // (and the dropped bolts' direction's w lane, their pvar +0x2c: bolt_spawn 0x2a7438 stores the direction bolts_drop built on
            // its stack, whose w lane is a leftover in the original (ORACLE_WATCH: pc 0x297aec, under moby_drop_bolts / bolts_drop))
            if (g_oracle_level == 3) {
                level();
                for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) g_ignored_words.push_back(a + 0x2c);
            }
            for (uint32_t a = OV(0x00173e40); a < OV(0x00173fc0); a += 0x10) g_ignored_words.push_back(a + 0xc);
            // (and the upper halves of two saved registers at the scratchpad's 0x70003700 .. 0x7000371c: COMMON's known leftovers)
            for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);
            for (uint32_t fn : {0x002c5bb8u, 0x002c6fd0u, 0x002c9eb8u})
                for (uint32_t m : g_oracle_level == 3 ? mobys_with(fn) : std::vector<uint32_t>{}) g_ignored_words.push_back(m + 0x1c);
            // the target (the hero) at `r` from `m`, ahead of it or anywhere, level with it or `dz` off
            auto hero_from = [=](uint32_t m, float r, bool ahead, float dz) {
                const float yaw = ee_read<float>(m + 0x48);
                const float a = ahead ? yaw + rf(-0.15f, 0.15f) : rf(-3.1f, 3.1f);
                Vec4 at = ee_ref<Vec4>(m + 0x10);
                at.x += r * std::cos(a); at.y += r * std::sin(a); at.z += dz;
                hero_at(at, 0);
                w32(P + 0x208c, 0);
            };
            // a distance about one of the class's own ranges (its pvars), or anywhere
            std::vector<uint32_t> second_paths;
            if (g_oracle_level == 3) {
                level();
                for (uint32_t t : troopers) { const int32_t i = static_cast<int32_t>(ee_read<uint32_t>(ee_read<uint32_t>(t + 0x78) + 0x290)); if (i >= 0) second_paths.push_back(u(i)); }
            }
            // a hit of the kinds the hit reaction turns into each of its reactions (+0x28 the kind, +0x29 its strength, +0x2a the
            // weapon, +0x1c the push's mark), from the hero, his moby or another moby
            auto hit_kinds = [=](uint32_t m) {
                give_hit(m, 0x330000);
                const uint32_t idx = ee_read<uint8_t>(m + 0xa4);
                if (idx == 0xff) return;
                const uint32_t e = OV(0x00178100) + (idx << 6);
                w8(e + 0x28, u(ri(0, 7)));
                w8(e + 0x29, u(ri(0, 3)));
                if (coin()) w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 3)));
                if (coin()) wf(e + 0x1c, u2f(0x45afdf66u));
                switch (ri(0, 2)) {
                case 0: w32(e + 0x20, 0); break;
                case 1: {  // (the moby the hero's word 0x1403e0 names, made a live one)
                    if (!troopers.empty()) w32(0x001403e0, troopers[static_cast<size_t>(ri(0, static_cast<int>(troopers.size()) - 1))]);
                    w32(e + 0x20, ee_read<uint32_t>(0x001403e0));
                    break;
                }
                default: w32(e + 0x20, troopers.empty() ? 0u : troopers[static_cast<size_t>(ri(0, static_cast<int>(troopers.size()) - 1))]); break;
                }
            };
            auto about = [](float range) { return ri(0, 2) == 0 ? range + rf(-0.6f, 0.6f) : (coin() ? rf(0.5f, range) : rf(range, range * 2.0f + 5.0f)); };
            // (for the troopers' checks: the w lane of the spark particles' velocity (+0x2c of the 0x40-byte particles at *0x16016c):
            // spark_burst's dev_2712b8 0x2712b8 copies its caller's stack vector whole in the original, the port stores 0 there
            // (ORACLE_WATCH: pc 0x25f7f4 under spark_burst L3:0x250ae8); the troopers' shots and the explosions throw them)
            if (g_oracle_level == 3) {
                level();
                for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(ee_read<uint32_t>(OV(0x0016016c)) + (k << 6) + 0x2c);
            }
            const std::vector<uint32_t> with_sparks = g_ignored_words;
            // (passes 16 .. 18: the shot's frame 8 in state 5; 19 .. 21: the hits' reactions in states 1 and 4; 22 / 23: the blow's
            // frames in state 6, the target ahead; 24 / 25: state 10 near its landing or below z 5; 26: state 2, its moby gone)
            for (int pass = 0; pass < 30; ++pass)
            classcheck("class574_states", 0x002c6fd0, kerwan_class574_update_l3, [=](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                // (a trooper still in its state 0 at the snapshot has no path yet: its state 0 sets +0x130 from +0x134)
                if (static_cast<int32_t>(ee_read<uint32_t>(pv + 0x134)) >= 0) w32(pv + 0x130, lvd::path(static_cast<int32_t>(ee_read<uint32_t>(pv + 0x134))));
                vary_frame_scale();
                static const uint32_t focus[] = {6, 6, 10, 10, 2};
                w8(m + 0x20, pass < 16 ? u(pass) : (pass < 19 ? 5u : (pass < 22 ? (coin() ? 1u : 4u) : (pass < 27 ? focus[pass - 22] : u(ri(0, 15))))));
                // the target: about its sight (+0x270), its fire range (+0x278), its approach (+0x27c), its path's reach (+0x294), a blow's 3.5
                {
                    static const uint32_t ranges[] = {0x270, 0x278, 0x27c, 0x294};
                    const int k = ri(0, 4);
                    const float r = k < 4 ? about(std::max(1.0f, std::fabs(ee_read<float>(pv + ranges[k])))) : rf(1.0f, 3.4f);
                    static const float dzs[] = {0.0f, 0.5f, 1.5f, 2.5f, 3.5f, 6.0f, 12.0f};
                    hero_from(m, r, coin(), dzs[ri(0, 6)] * (coin() ? 1.0f : -1.0f));
                }
                // its animation: the shot's frame 8 (anim 5), the blow's frames 25 .. 31, the end bit
                if (ri(0, 2) == 0) {
                    static const uint32_t sets[] = {0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11};
                    w8(m + 0x53, sets[ri(0, 10)]);
                }
                if (ri(0, 2) == 0) { w8(m + 0x52, coin() ? 10u : 5u); wf(m + 0x54, 0.0f); w16(ee_read<uint32_t>(m + 0x68) + 4, u(coin() ? ri(120, 140) : ri(390, 510))); }
                if (coin()) w8(m + 0x70, ee_read<uint8_t>(m + 0x70) ^ 2u);
                // its modes and timers
                if (ri(0, 3) == 0) w8(pv + 0x262, u(ri(0, 2)));
                timer16(pv + 0x250);
                if (coin()) w8(pv + 0x263, u(ri(0, 2)));
                if (coin()) w8(pv + 0x255, u(ri(0, 3)));
                if (coin()) w16(pv + 0x256, coin() ? 1u : 0u);
                if (coin()) w32(pv + 0x274, coin() ? 1u : 0u);
                if (ri(0, 3) == 0) w32(pv + 0x264, coin() ? 0xffffffffu : 0u);
                if (ri(0, 3) == 0) w32(pv + 0x284, coin() ? 0xffffffffu : u(ri(0, 300)));
                if (ri(0, 3) == 0) w32(pv + 0x290, coin() || second_paths.empty() ? 0xffffffffu : second_paths[static_cast<size_t>(ri(0, static_cast<int>(second_paths.size()) - 1))]);
                // (state 9 walks its second path: the game enters it only with one)
                if (ee_read<uint8_t>(m + 0x20) == 9 && static_cast<int32_t>(ee_read<uint32_t>(pv + 0x290)) < 0 && !second_paths.empty())
                    w32(pv + 0x290, second_paths[static_cast<size_t>(ri(0, static_cast<int>(second_paths.size()) - 1))]);
                {   // the follower's point along its own path +0x130 and the second path's +0x298
                    const uint32_t path = ee_read<uint32_t>(pv + 0x130);
                    const int32_t n = path != 0 ? static_cast<int32_t>(ee_read<uint32_t>(path)) : 0;
                    if (n > 0 && n < 4096 && coin()) w32(pv + 0x120, u(ri(0, n - 1)));
                    if (coin()) w32(pv + 0x298, u(ri(0, 3)));
                }
                // standing on a train carriage (class 1210) or on nothing
                if (ri(0, 3) == 0 && !carriages.empty()) w32(pv + 0x280, (carriages[static_cast<size_t>(ri(0, static_cast<int>(carriages.size()) - 1))] - table()) >> 8);
                else if (ri(0, 3) == 0) w32(pv + 0x280, 0xffffffffu);
                // the checkpoint table's "gone" mark of its spawn (0x14c050 + level * 16 + its +0xb0)
                if (ri(0, 3) == 0) w8(0x0014c050 + ee_read<uint32_t>(0x0015ed84) * 16 + ee_read<uint8_t>(m + 0xb0), coin() ? 0xffu : 0u);
                if (coin()) w8(m + 0xbc, u(ri(6, 9)));
                if (coin()) wf(pv + 0x28c, rf(-3.1f, 3.1f));
                if (coin()) w16(pv + 0xae, coin() ? 2u : 0u);
                if (ri(0, 4) == 0) wf(m + 0x18, ee_read<float>(m + 0x18) + rf(-8, 8));
                if (ri(0, 8) == 0) wf(m + 0x10 + 4 * u(ri(0, 2)), coin() ? rf(-5, 1.9f) : rf(1021.5f, 1030));
                if (coin()) wf(pv + 0xe8, rf(-1, 3));
                if (coin()) w8(m + 0x31, coin() ? 1u : 0u);
                if (coin()) w32(OV(0x0015f5cc), (ee_read<uint32_t>(OV(0x0015f5cc)) & ~7u) | (ee_read<uint16_t>(pv + 0x28a) & 7u));
                if (coin()) { Vec4 c = ee_ref<Vec4>(m + 0x10); c.x += rf(-60, 60); c.y += rf(-60, 60); ee_ref<Vec4>(OV(0x00166dc0)) = c; }
                if (pass >= 16 && pass < 19) {  // the animation 5 crossing its frame 8 this frame
                    w8(m + 0x52, 5); wf(m + 0x54, 0.0f); wf(m + 0x58, 1.0f); wf(m + 0x5c, 1.0f);
                    w16(ee_read<uint32_t>(m + 0x68) + 4, u(ri(126, 146)));
                }
                if (coin()) w32(pv + 0x38, 1);
                // the edges its tests turn on: the turn's 20 (the target about 20 away, well aside), the turn's 2 degrees (state 12 that
                // close to its heading), the camera about 70 (the fall-out test), a target about 16 from one of its path's points
                if (ri(0, 4) == 0) hero_from(m, rf(19.0f, 21.0f), false, rf(-1, 1));
                if (ee_read<uint8_t>(m + 0x20) == 12 && coin()) wf(m + 0x48, ee_read<float>(pv + 0x28c) + (coin() ? 1.0f : -1.0f) * rf(0.02f, 0.07f));
                if (ri(0, 3) == 0) { Vec4 c = ee_ref<Vec4>(m + 0x10); const float a = rf(-3.1f, 3.1f), r = rf(58, 72); c.x += r * std::cos(a); c.y += r * std::sin(a); ee_ref<Vec4>(OV(0x00166dc0)) = c; }
                if (ri(0, 4) == 0) {
                    const uint32_t path = ee_read<uint32_t>(pv + 0x130);
                    const int32_t n = path != 0 ? static_cast<int32_t>(ee_read<uint32_t>(path)) : 0;
                    if (n > 0 && n < 4096) {
                        Vec4 q = ee_ref<Vec4>(path + 0x10 + u(ri(0, n - 1)) * 16);
                        const float a = rf(-3.1f, 3.1f), r = rf(15.0f, 17.0f);
                        q.x += r * std::cos(a); q.y += r * std::sin(a);
                        hero_at(q, 0);
                        w32(P + 0x208c, 0);
                    }
                }
                if (pass == 22 || pass == 23) {  // the blow: the target ahead within reach, level, the animation in its frames 25 .. 31
                    hero_from(m, rf(0.8f, 3.4f), true, rf(-1.5f, 1.5f));
                    w8(m + 0x52, 2); wf(m + 0x54, 0.0f); w16(ee_read<uint32_t>(m + 0x68) + 4, u(ri(392, 504)));
                }
                if (pass == 24 || pass == 25) {  // knocked back: landed (the move record's +0x3e 2, the animation's end) or low
                    w16(pv + 0xae, coin() ? 2u : 0u);
                    if (pass == 25) {
                        wf(m + 0x18, rf(-2, 4.9f));
                        w32(OV(0x0015f5cc), (ee_read<uint32_t>(OV(0x0015f5cc)) & ~7u) | (ee_read<uint16_t>(pv + 0x28a) & 7u));
                    }
                    if (coin()) w32(pv + 0x274, 0);
                }
                if (pass == 26 && static_cast<int32_t>(ee_read<uint32_t>(pv + 0x284)) >= 0 && coin())
                    w8(moby_at(static_cast<int32_t>(ee_read<uint32_t>(pv + 0x284))) + 0x20, coin() ? 0xfeu : 0xfdu);
                // its hit state as its state 0 leaves it (+0x28: its type, 2), the hit reaction's timer +0x3c idle or running
                w8(pv + 0x28, 2);
                w16(pv + 0x3c, coin() ? 0u : u(ri(1, 20)));
                hit_kinds(m);
                if (pass >= 19 && pass < 22 && ee_read<uint8_t>(m + 0xa4) != 0xff && coin()) {  // the reaction 6 (kind 1, weak)
                    const uint32_t e = OV(0x00178100) + (u(ee_read<uint8_t>(m + 0xa4)) << 6);
                    w8(e + 0x28, 1); w8(e + 0x29, u(ri(0, 1))); w16(pv + 0x3c, 0);
                }
                seed_rand();
            }, 30);

            g_ignored_words = with_sparks;  // (the mine layer's explosion throws the same particles)
            for (int pass = 0; pass < 10; ++pass)
            classcheck("class578_states", 0x002c9eb8, kerwan_class578_update_l3, [=](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                if (static_cast<int32_t>(ee_read<uint32_t>(pv + 0x134)) >= 0) w32(pv + 0x130, lvd::path(static_cast<int32_t>(ee_read<uint32_t>(pv + 0x134))));
                vary_frame_scale();
                w8(m + 0x20, pass < 6 ? u(pass) : u(ri(0, 5)));
                hero_from(m, ri(0, 2) == 0 ? (coin() ? rf(13, 15) : rf(27, 29)) : (coin() ? rf(10, 32) : rf(0.5f, 60)), coin(), coin() ? rf(-3, 3) : rf(-8, 8));
                if (coin()) w32(pv + 0x38, coin() ? 1u : 0u);
                timer32(pv + 0x15c);
                timer16(pv + 0x154);
                timer16(pv + 0x156);
                {
                    const uint32_t path = ee_read<uint32_t>(pv + 0x130);
                    const int32_t n = path != 0 ? static_cast<int32_t>(ee_read<uint32_t>(path)) : 0;
                    if (n > 0 && n < 4096 && coin()) {
                        const int32_t i = ri(0, n - 1);
                        w32(pv + 0x120, u(i));
                        if (coin()) { Vec4 q = ee_ref<Vec4>(path + 0x10 + u(i) * 16); q.x += rf(-1, 1); q.y += rf(-1, 1); ee_ref<Vec4>(m + 0x10) = q; }
                    }
                    if (coin()) w8(pv + 0x124, coin() ? 1u : 0xffu);
                }
                { static const uint32_t an[] = {0, 1, 2}; if (coin()) w8(m + 0x53, an[ri(0, 2)]); }
                if (coin()) w8(m + 0x70, ee_read<uint8_t>(m + 0x70) ^ 2u);
                if (ri(0, 4) == 0) wf(m + 0x18, ee_read<float>(m + 0x18) + rf(-8, 3));
                if (coin()) wf(pv + 0xe8, rf(-1, 3));
                if (coin()) w8(m + 0x31, coin() ? 1u : 0u);
                if (coin()) { Vec4 c = ee_ref<Vec4>(m + 0x10); c.x += rf(-40, 40); c.y += rf(-40, 40); ee_ref<Vec4>(OV(0x00166dc0)) = c; }
                {   // right at its path's point (the turn at a point), or low (a fall that ends it)
                    const uint32_t path = ee_read<uint32_t>(pv + 0x130);
                    const int32_t n = path != 0 ? static_cast<int32_t>(ee_read<uint32_t>(path)) : 0;
                    if (n > 0 && n < 4096 && ri(0, 2) == 0) {
                        Vec4 q = ee_ref<Vec4>(path + 0x10 + u(ee_read<uint32_t>(pv + 0x120) % static_cast<uint32_t>(n)) * 16);
                        q.x += rf(-0.6f, 0.6f); q.y += rf(-0.6f, 0.6f);
                        ee_ref<Vec4>(m + 0x10) = q;
                    }
                    if (ri(0, 4) == 0) wf(m + 0x18, rf(-2, 6));
                }
                w8(pv + 0x28, 1);  // (its hit type, as its state 0 leaves it)
                w16(pv + 0x3c, coin() ? 0u : u(ri(1, 20)));
                hit_kinds(m);
                seed_rand();
            }, 30);

            g_ignored_words = with_sparks;  // (the troopers of the pair fire too)
            // the pair: a trooper with its class 573 followers (their +0x248), the hero put within its sight (an alert) or by a
            // follower, then 1 .. 40 frames of both classes' updates (the ports, in the game's order: the moby list's), then the next
            // update of the trooper and of one follower each compared with the original
            std::vector<std::pair<uint32_t, std::vector<uint32_t>>> squads;
            if (g_oracle_level == 3) {
                level();
                for (uint32_t t : troopers) {
                    std::vector<uint32_t> f;
                    for (uint32_t w : mobys_with(0x002c5bb8u))
                        if (static_cast<int32_t>(ee_read<uint32_t>(ee_read<uint32_t>(w + 0x78) + 0x248)) == static_cast<int32_t>((t - table()) >> 8)) f.push_back(w);
                    if (!f.empty()) squads.emplace_back(t, f);
                }
                if (info) std::printf("  B112 squads: %zu\n", squads.size());
            }
            for (int who = 0; who < 2; ++who)
            for (int pass = 0; pass < 4; ++pass) {
                if (g_oracle_level == 3 && squads.empty()) { std::printf("%-26s no trooper with followers\n", who == 0 ? "squad_leader" : "squad_follower"); break; }
                l3check(who == 0 ? "squad_leader" : "squad_follower", who == 0 ? 0x002c6fd0u : 0x002c5bb8u, 25, Ret::None, [=] {
                    level();
                    const auto& sq = squads[static_cast<size_t>(ri(0, static_cast<int>(squads.size()) - 1))];
                    const uint32_t lead = sq.first;
                    const uint32_t pick = sq.second[static_cast<size_t>(ri(0, static_cast<int>(sq.second.size()) - 1))];
                    const uint32_t lpv = ee_read<uint32_t>(lead + 0x78);
                    if (pass < 2) hero_from(lead, rf(0.5f, std::max(2.0f, ee_read<float>(lpv + 0x270))), coin(), rf(-1, 1));
                    else hero_from(pick, rf(1.0f, 12.0f), coin(), rf(-1, 1));
                    if (pass == 3) w32(ee_read<uint32_t>(pick + 0x78) + 0x38, 1);
                    vary_frame_scale();
                    seed_rand();
                    std::vector<uint32_t> order = sq.second;
                    order.push_back(lead);
                    std::sort(order.begin(), order.end());
                    const int frames = ri(1, 40);
                    for (int f = 0; f < frames; ++f)
                        for (uint32_t m : order) {
                            if (ee_read<uint8_t>(m + 0x20) >= 0xfd) continue;
                            if (m == lead) kerwan_class574_update_l3(m);
                            else kerwan_class573_update_l3(m);
                        }
                    w8(lead + 0xa4, 0xff);
                    w8(pick + 0xa4, 0xff);
                    return Args{{who == 0 ? lead : pick}, {}};
                }, [who](A a) { if (who == 0) kerwan_class574_update_l3(a.i[0]); else kerwan_class573_update_l3(a.i[0]); return uint64_t{0}; });
            }
        }

        g_ignored_words = saved_ignored;
        // the hand-written helpers other code reaches, each against its original: the group test class 574's generated update is
        // glued to (L3:0x2c6d98: the level's groups, their members put in the states it counts, alive or gone), the level's sound by
        // the level's own index (L3:0x27a618: indices about the list's end, the moby's slot), the traffic's puff of smoke
        // (L3:0x2bad40: the level's craft, their colours, lives, rises and sizes)
        if (g_oracle_level == 3) {
            level();
            std::vector<int32_t> groups;
            for (int32_t g = 0; g < std::min(lvd::group_count(), 112); ++g)
                if (lvd::group(g) != 0) groups.push_back(g);
            l3check("walker_group_busy", 0x002c6d98, 80, Ret::Int32, [=] {
                level();
                uint32_t g = groups.empty() || ri(0, 7) == 0 ? (coin() ? 0xffu : u(ri(0, 111))) : u(groups[static_cast<size_t>(ri(0, static_cast<int>(groups.size()) - 1))]);
                if (g != 0xff && lvd::group(static_cast<int32_t>(g)) != 0)
                    for (uint32_t e = lvd::group(static_cast<int32_t>(g)); ; e += 2) {
                        const uint32_t mob = moby_at(static_cast<int32_t>(ee_read<uint16_t>(e) & 0x7fffu));
                        static const uint32_t st[] = {5, 9, 0, 3, 0xfe, 0xfd};
                        static const uint16_t cl[] = {0x23d, 0x23e, 0x23d, 0x23e, 0x4b};
                        if (ri(0, 2) == 0) w8(mob + 0x20, st[ri(0, 5)]);
                        if (ri(0, 2) == 0) w16(mob + 0xa6, cl[ri(0, 4)]);
                        if (static_cast<int16_t>(ee_read<uint16_t>(e)) < 0) break;
                    }
                return Args{{g}, {}};
            }, [](A a) { return static_cast<uint64_t>(walker_group_busy_l3(a.i[0])); });
            const std::vector<uint32_t> craft = mobys_with(0x0029dba8u);
            l3check("level_sound_start", 0x0027a618, 60, Ret::Int32, [=] {
                level();
                const int32_t own = static_cast<int32_t>(ee_read<uint32_t>(OV(0x0015f5f0))) - static_cast<int32_t>(ee_read<uint32_t>(OV(0x0015f574)));
                const int32_t idx = ri(0, 2) == 0 ? own + ri(-2, 1) : ri(0, std::max(0, own - 1));
                const uint32_t m = craft.empty() ? table() : craft[static_cast<size_t>(ri(0, static_cast<int>(craft.size()) - 1))];
                for (uint32_t s = 0; s < 30; ++s) if (ri(0, 3) == 0) w8(0x0013e5c0 + s * 0x70 + 0x4, 0);
                return Args{{u(idx), coin() ? 0u : (R() & 0x15u), m}, {}};
            }, [](A a) { return static_cast<uint64_t>(static_cast<uint32_t>(kerwan_level_sound_start_l3(static_cast<int32_t>(a.i[0]), a.i[1], a.i[2]))); });
            // the horn (L3:0x29d1b8): its wait at its ends (the first, expiring, running), the camera about its 15 / 90 limits
            l3check("traffic_horn", 0x0029d1b8, 150, Ret::None, [=] {
                level();
                const uint32_t m = craft.empty() ? table() : craft[static_cast<size_t>(ri(0, static_cast<int>(craft.size()) - 1))];
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                static const uint16_t cls[] = {51, 64, 75, 115, 116, 117, 118, 119, 120, 132};
                if (coin()) w16(m + 0xa6, cls[ri(0, 9)]);
                static const int32_t waits[] = {-1, 0, 1, 1, 2, 30};
                w16(pv + 0x148, u(waits[ri(0, 5)]));
                if (coin()) { w16(pv + 0x14c, u(ri(0, 300))); w16(pv + 0x14e, u(ri(0, 300))); }
                Vec4 cam = ee_ref<Vec4>(m + 0x10);
                const float r = ri(0, 2) == 0 ? rf(14.5f, 16.5f) : (coin() ? rf(88.5f, 91.5f) : rf(0, 200));
                const float a = rf(-3.1f, 3.1f);
                cam.x += r * std::cos(a); cam.y += r * std::sin(a);
                ee_ref<Vec4>(OV(0x00166dc0)) = cam;
                if (ri(0, 3) == 0) { wf(0x0015ed68, 1.2f); wf(0x0015ed6c, 0.02f); wf(0x0015ed70, 0.0004f); }
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { traffic_horn_l3(a.i[0]); return uint64_t{0}; });
            l3check("traffic_puff_spawn", 0x002bad40, 60, Ret::Int32, [=] {
                level();
                const uint32_t m = craft.empty() ? table() : craft[static_cast<size_t>(ri(0, static_cast<int>(craft.size()) - 1))];
                wfrand(ee_read<uint32_t>(m + 0x78) + 0xf0, 3, -50, 50);
                seed_rand();
                return Args{{m, ee_read<uint32_t>(m + 0x78) + 0xf0, coin() ? 0xa040u : R(), coin() ? 0x90a0u : R(), u(ri(1, 80))}, {rf(0, 0.2f), rf(0.05f, 2.0f)}};
            }, [](A a) { return static_cast<uint64_t>(traffic_puff_spawn_l3(a.i[0], a.i[2], a.i[3], static_cast<int32_t>(a.i[4]), a.f[0], a.f[1])); });
        }
    }

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
