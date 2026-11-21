// Oracle batch 107 (task 0331): Aridia's own moby classes (level 2) on its real level data. Run with
// ORACLE_LEVEL=2 build/tests/ovl_oracle --batch 107 (needs the ISO). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the
// original is Aridia's own function at its own address, the port the one registered for it.

#include "ovl_oracle.h"

#include <game/ovl/aridia_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/disc.h>

#include <optional>
#include <cmath>
#include <cstring>
#include <map>

namespace rac1::oracle {

namespace {
// The checks named here reach segment_collide, whose crossing lists, chunk buffers and vertices the port keeps on
// the host stack (the original: 0x70003000 .. 0x70003fff, 0x70000000 .., 0x70002000): A2's collision scratch ranges
// are left out of these checks, and only of them (stage 5, task C2c).
bool c2c_spr_check(const char* name) {
    static const char* const kNames[] = {"spawn_point_update"};
    for (const char* n : kNames)
        if (std::strcmp(n, name) == 0) return true;
    return false;
}

// A check whose original is Aridia's own function at `l2addr` (not a Veldin address to translate).
void l2check(const char* name, uint32_t l2addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (g_oracle_level != 2) {
        std::printf("%-26s L2:0x%08x  skipped (Aridia only: run with ORACLE_LEVEL=2)\n", name, l2addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L2:0x%08x\n", name, l2addr);
    std::optional<CollisionScratchIgnored> spr_ignored;
    if (c2c_spr_check(name)) spr_ignored.emplace();
    if (!check_fn(name, l2addr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_107() {
    if (g_only_batch != 107) return;
    g_batch = 107;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime

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
    // the level's mobys whose update function is `fn` (Aridia's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 2) { l2check(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        level();
        const std::vector<uint32_t> ms = mobys_with(fn);
        if (ms.empty()) { std::printf("%-26s L2:0x%08x  no moby on this level\n", name, fn); return; }
        l2check(name, fn, n, Ret::None, [level, ms, vary] {
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

    // ---- the props, platforms and mechanisms
    for (int pass = 0; pass < 2; ++pass)
    classcheck("trackdrive_left_update", 0x002cb978, trackdrive_left_update_l2, [pass](uint32_t m) {
        w16(m + 0x34, R()); w32(m + 0x58, R());
        w32(OV(0x0015ffdc), pass == 0 ? ee_read<uint32_t>(OV(0x0015ffdc)) : (coin() ? m : m + 0x100));  // the array's end: below / above the moby
    }, 30);
    for (uint32_t fn : {L2_ADDR(0x002ddc00u), L2_ADDR(0x002df8c0u)})
    classcheck("platform_update", fn, platform_update_l2, [](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        wfrand(m + 0x40, 4, -3.1f, 3.1f);
        wrand(pv + 0x20, 16);
    }, 40);
    for (int pass = 0; pass < 6; ++pass)
    classcheck("bulldozer_update", 0x002dca10, bulldozer_update_l2, [pass](uint32_t m) {
        w8(m + 0x53, pass < 4 ? u(pass) : u(ri(0, 5)));
        w8(m + 0x70, u(R() & 3));
        w8(m + 0xbc, u(ri(0, 3) == 0 ? 1u : R() & 0xffu));
        seed_rand();
    }, 40);
    for (int pass = 0; pass < 10; ++pass)
    classcheck("grab_crane_update", 0x002dd370, grab_crane_update_l2, [pass](uint32_t m) {
        static const uint32_t anims[] = {0, 2, 3, 7, 1, 4, 5, 6};
        w8(m + 0x53, anims[pass % 8]);
        w8(m + 0x70, u(R() & 3));
        w8(m + 0xbc, u(ri(0, 3) == 0 ? 1u : R() & 0xffu));
        seed_rand();
    }, 40);
    // the debris' velocity / spin w lanes: a VU0 register leftover of the original's sin / cos (the port's has none)
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    if (g_oracle_level == 2) level();
    for (uint32_t a = g_oracle_level == 2 ? ee_read<uint32_t>(OV(0x0015ffe0)) : 0u; a < (g_oracle_level == 2 ? ee_read<uint32_t>(OV(0x0015ffe0)) : 0u) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
    for (int pass = 0; pass < 5; ++pass)
    classcheck("white_flower_update", 0x002f1f60, white_flower_update_l2, [pass, give_hit, hero_near](uint32_t m) {
        w8(m + 0x20, pass < 4 ? u(pass) : u(ri(0, 3)));
        give_hit(m, 0x10000);
        hero_near(m);
        seed_rand();
    }, 40);
    g_ignored_words = saved_ignored;
    // the Tresspasser doors (743 / 744) and the pad (class 615 = 0x267) they wait for
    auto pad_index = [level] {
        level();
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        for (uint32_t m = first; m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<int16_t>(m + 0xa6) == 615) return (m - first) >> 8;
        return 0u;
    };
    for (uint32_t fn : {L2_ADDR(0x002dffc8u), L2_ADDR(0x002e0138u)})
    for (int pass = 0; pass < 8; ++pass)
    classcheck("trespasser_door_update", fn, fn == L2_ADDR(0x002dffc8u) ? trespasser_door1_update_l2 : trespasser_door2_update_l2, [pass, pad_index, fn](uint32_t m) {
        const uint32_t pad = pad_index(), first = ee_read<uint32_t>(OV(0x0015ffd8));  // (pad_index reloads the level: first)
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, pass == 0 ? u(ri(0, 3)) : u(pass < 3 ? pass - 1 : 2));  // (passes 3..7: the sliding state)
        w32(pv + 0x10, ri(0, 5) == 0 ? u(ri(-1, 2)) : pad);
        w8(first + (pad << 8) + 0x20, ri(0, 2) == 0 ? u(ri(0, 5)) : 4u);
        if (ri(0, 4) == 0) w16(first + (pad << 8) + 0xa6, coin() ? 0x23fu : u(ri(0, 700)));
        ee_ref<Vec4>(pv) = ee_ref<Vec4>(m + 0x10);
        { Vec4 p = ee_ref<Vec4>(pv); p.x += rf(-4, 4); p.y += rf(-4, 4); p.z += rf(-4, 4); ee_ref<Vec4>(pv) = p; }
        wfrand(m + 0xd0, 3, -1, 1);
        if (ri(0, 2) == 0) {  // at / next to the end position: an axis-aligned direction and a whole-number start make `to` exact on
                              // both sides (the host's normalise and the original's differ in the last bit otherwise)
            const uint32_t axis = u(ri(0, 2));
            const float sgn = coin() ? 1.0f : -1.0f, len = fn == L2_ADDR(0x002dffc8u) ? 2.0f : -2.0f;
            Vec4 d = {0, 0, 0, ee_ref<Vec4>(m + 0xd0).w};
            (&d.x)[axis] = sgn;
            ee_ref<Vec4>(m + 0xd0) = d;
            Vec4 st = ee_ref<Vec4>(pv);
            st.x = static_cast<float>(ri(-20, 20)); st.y = static_cast<float>(ri(-20, 20)); st.z = static_cast<float>(ri(-20, 20));
            ee_ref<Vec4>(pv) = st;
            Vec4 now = ee_ref<Vec4>(m + 0x10);
            now.x = st.x; now.y = st.y; now.z = st.z;
            (&now.x)[axis] += sgn * len;
            static const float off[] = {0.0f, 0.0f, 0.0f, 0.001f, -0.001f, 0.03f, 0.5f};  // 0: the arrival; the others the clamp's two sides
            (&now.x)[axis] += off[ri(0, 6)];
            ee_ref<Vec4>(m + 0x10) = now;
        }
    }, 40);
    // a sound channel for the moby: slot or -1, the channel's owner and flag
    auto sound_slot = [](uint32_t m, uint32_t where) {
        const int32_t slot = ri(0, 3) == 0 ? -1 : ri(0, 15);
        w32(where, u(slot));
        if (slot >= 0) { const uint32_t c = 0x0013e550 + u(slot) * 0x70; w32(c + 0x88, ri(0, 3) == 0 ? 0u : m); w8(c + 0x74, R() & 1); }
    };
    // the crane (732)
    for (int pass = 0; pass < 12; ++pass)
    classcheck("crane_update", 0x002df1a8, crane_update_l2, [pass, sound_slot](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x53, pass < 9 ? u(pass) : u(ri(0, 8)));
        w8(m + 0x70, u(R() & 3));
        { const uint32_t a = ee_read<uint8_t>(m + 0x53); w8(m + 0xbc, (a & 1) != 0 && a != 8 ? u(ri(0, 7)) : u(ri(0, 3) == 0 ? 1u : (coin() ? u(ri(0, 8)) : (R() & 0xffu)))); }  // (the next animation in the odd states: a valid one)
        sound_slot(m, pv);
        wf(L2_ADDR(0x00161c68), rf(-2, 2));
        seed_rand();
    }, 40);
    // the escalators (652 / 653): the belt speed words 0x161c14 / 0x161c1c, the wait 0x161c18
    for (int pass = 0; pass < 16; ++pass)
    classcheck("escalator_update", 0x002dc6b0, escalator_update_l2, [pass](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, pass < 5 ? u(pass) : u(ri(0, 4)));
        wf(m + 0x48, rf(-3.1f, 3.1f));
        wf(L2_ADDR(0x00161c14), ri(0, 8) == 0 ? 0.0f : rf(-3, 3)); wf(L2_ADDR(0x00161c1c), rf(0, 2)); w32(L2_ADDR(0x00161c18), u(ri(1, 90)));
        // the offset along the belt: short / long, the timer running / expired
        const float len = ri(0, 3) == 0 ? 0.0f : rf(0, 6), yaw = ee_read<float>(m + 0x48);
        wf(pv + 0x40, len * std::cos(yaw)); wf(pv + 0x44, len * std::sin(yaw)); wf(pv + 0x48, 0.0f);
        w32(pv + 0xa0, ri(0, 2) == 0 ? 1u : u(ri(0, 4)));
        w32(pv + 0x9c, R()); w32(pv + 0xa4, ri(0, 1) ? u(R() & 3) : 0u);
        wrand(pv + 0x60, 16);
    }, 40);
    // the rocket doors (735 / 736)
    for (int pass = 0; pass < 6; ++pass)
    classcheck("rocket_door_update", 0x002df948, rocket_door_update_l2, [pass](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, pass < 3 ? u(pass) : u(ri(0, 3)));
        w8(m + 0xbc, ri(0, 2) == 0 ? 0u : u(ri(1, 3)));
        w32(0x0015f5c4, ri(0, 3) == 0 ? 2u : u(ri(0, 1)));
        w8(0x0014c050 + ee_read<uint32_t>(0x0015ed84) * 16 + ee_read<uint8_t>(m + 0xb0), coin() ? 0xffu : u(ri(0, 3)));
        w16(m + 0x34, R());
        wfrand(m + 0x40, 3, -3.1f, 3.1f);
        wfrand(pv + 0x10, 3, -180, 180);
        wfrand(pv, 4, -1, 1);
        wf(pv + 0x20, ri(0, 2) == 0 ? 0.0f : rf(-0.3f, 0.3f)); wf(pv + 0x24, ri(0, 3) == 0 ? 0.0f : rf(-0.2f, 1.2f));
    }, 40);
    // ---- the spawn point (class 805) and the zone marker (class 605): Novalis's ports on Aridia's code
    for (int pass = 0; pass < 3; ++pass)
    classcheck("spawn_point_update", 0x002e28a0, spawn_point_update_l1, [pass](uint32_t m) {
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
        if (coin()) {  // the hero in the box of the flags' index (or elsewhere)
            const int32_t b = ri(0, 3) == 0 ? ri(-1, 3) : static_cast<int32_t>(ee_read<uint32_t>(pv));
            const uint32_t box = ee_read<uint32_t>(OV(0x001600ec)) + u(b << 7);
            if (b >= 0) for (uint32_t k = 0; k < 3; ++k) wf(0x0013f3d0 + 4 * k, ee_read<float>(box + 0x30 + 4 * k) + rf(-0.5f, 0.5f));
        }
        if (ri(0, 3) == 0) w32(pv, u(ri(-1, 3)));
        if (ri(0, 2) == 0) wf(0x0013f3d0 + 8, rf(-50, 500));
    }, 80);
    for (int pass = 0; pass < 4; ++pass)
    classcheck("zone_marker_update", 0x002d74e0, zone_marker_update_l2, [pass](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t P = 0x0013f350;
        w8(m + 0x20, pass == 0 ? u(ri(0, 1)) : (pass == 1 ? 0u : 1u));
        w8(m + 0xbc, ri(0, 3) == 0 ? u(ri(1, 9)) : 0u);
        w32(0x0015ed84, u(ri(0, 0x16)));
        w32(L2_ADDR(0x00161a60), u(ri(0, 20)));
        for (uint32_t i = 0; i < 64; ++i) w8(0x0014bf10 + i, R());
        w32(pv + 0x18, u(ri(-3, 40)));
        w32(pv + 0x10, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 5))); w32(pv + 0x14, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 4)));
        // the hero in the polygon's / box's neighbourhood
        { const int32_t b = static_cast<int32_t>(ee_read<uint32_t>(pv + 0x14)); Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-10, 10); q.y += rf(-10, 10);
          if (b >= 0 && coin()) { const uint32_t box = ee_read<uint32_t>(OV(0x001600ec)) + u(b << 7); for (uint32_t k = 0; k < 3; ++k) (&q.x)[k] = ee_read<float>(box + 0x30 + 4 * k) + rf(-0.3f, 0.3f); }
          ee_ref<Vec4>(P + 0x80) = q; }
        w32(P + 0x1090, ri(0, 3) == 0 ? ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(1, 50)) : 0u); w32(P + 0x10b8, coin() ? 0x1bu : u(ri(0, 3)));
        if (ee_read<uint32_t>(P + 0x1090) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(P + 0x1090) + 0x78) == 0) w32(P + 0x1090, 0);
        wf(P + 0x2044, coin() ? 1000.0f : rf(0, 30)); w32(P + 0x2040, 0); w32(P + 0x2048, 0);
    }, 60);
    // the ocean's wave table: its header words and the table
    for (int pass = 0; pass < 3; ++pass)
    classcheck("ocean_update", 0x002ea198, ocean_update_l2, [pass](uint32_t m) {
        w8(m + 0x20, u(pass));
        wrand(L2_ADDR(0x001612a0), 4);
        for (uint32_t i = 0; i < 0x100; ++i) wf(L2_ADDR(0x001cb510) + 4 * i, rf(-10, 10));
        w32(OV(0x0015f42c), ri(0, 3) == 0 ? u(ri(0x3e, 0x41)) : u(ri(0, 0x30)));
    }, 20);

    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
