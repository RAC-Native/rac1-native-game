// Oracle batch 543 (#150 stage 5, task C6a): the collision's native results (src/game/collision/collision_state.h: the last
// hit and the moby hit list) against their EE words - a host check at the boundary, no original code run. On the level after
// its first gameplay frame (the port's, as batches 530 and 540):
//  - state_queries: B543_N (100) queries of each kind around the hero and the level's mobys - segment_collide (flags 0, 2,
//    0x10, 1; a contact record now and then), sphere_collide, capsule_collide, sphere_mesh_collide, mobys_in_sphere (a contact
//    record now and then) - one after another on the same state (the level put back every 50 queries, then imported): after
//    each, the native state equals the EE words decoded field by field (the hit, the list's 0x80 words), every accessor
//    reads the same bits with RAC1_COLL_STATE=native and =ee, and the check mode (on) found no difference
//  - state_import: 200 rounds of random words written into the hit's and the list's EE words from outside (as a load or a
//    generator does): coll_state_import() gives a native state equal to their decode, and the accessors read them in both
//    modes
//  - state_check: with the check mode on (its report lines off), each accessor's field changed in EE memory alone: that
//    accessor's read counts one difference and returns the native value (native) or the EE word (ee); unchanged, none
//  - state_setters: each setter of collision.h writes the native field and the EE word(s) the original stores, nothing else
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 543, N = 0 .. 18.

#include "ovl_oracle.h"

#include <game/collision/collision.h>
#include <game/collision/collision_state.h>
#include <game/ovl/ovl.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {

bool selected(const char* name) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return false;
    const char* only = std::getenv("ORACLE_ONLY");
    return only == nullptr || std::strcmp(only, name) == 0;
}
bool fail(const char* what, const std::string& why) {
    std::printf("%-26s FAIL: %s\n", what, why.c_str());
    ++g_fail;
    return false;
}
std::string hex(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "0x%08x", v);
    return b;
}

// the level after its first gameplay frame (the port's), as batch 530's; the native state taken from it each time (the
// memory copy is a write that does not come through the collision module)
std::vector<uint8_t> g_snap, g_snap_spr;
void level() {
    uint8_t* ram = ee_get_ram_base();
    uint8_t* spr = hp<uint8_t>(0x70000000u);
    if (g_snap.empty()) {
        veldin_boot();
        level_setup_body(1, 0);
        w32(OV(0x0016c060), 0xf);
        w32(0x0013f2e8, 0xffffffffu);
        if (g_oracle_level == 14) w8(0x0013d3f0, 1);
        gameplay_frame();
        g_snap.assign(ram, ram + 0x02000000);
        g_snap_spr.assign(spr, spr + 0x4000);
    } else {
        std::memcpy(ram, g_snap.data(), g_snap.size());
        std::memcpy(spr, g_snap_spr.data(), g_snap_spr.size());
    }
    coll_state_import();
}

uint32_t r16(uint32_t a) { return ee_read<uint16_t>(a); }
uint32_t r32(uint32_t a) { return ee_read<uint32_t>(a); }
uint32_t first_moby() { return ee_read<uint32_t>(OV(0x0015ffd8)); }
uint32_t end_moby() { return ee_read<uint32_t>(OV(0x0015ffe0)); }
std::vector<uint32_t> live_mobys() {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < end_moby(); m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x24) != 0) v.push_back(m);
    return v;
}
Vec4 around(const Vec4& at, float r0, float r1, float z0, float z1) {
    const float r = rf(r0, r1), a = rf(-3.1f, 3.1f);
    return Vec4{at.x + r * std::cos(a), at.y + r * std::sin(a), at.z + rf(z0, z1), at.w};
}
// 0x40 bytes for sphere_mesh_collide's position: the test area, or below the EE stack where the level's moby table covers
// it (Kalebo III), as batch 530's
uint32_t scratch() {
    if (end_moby() <= TEST_AREA || first_moby() >= TEST_AREA + TEST_AREA_SIZE) return TEST_AREA;
    return 0x01ffe000u;
}

uint32_t vw(const Vec4& v, int k) { return f2u(k == 0 ? v.x : k == 1 ? v.y : k == 2 ? v.z : v.w); }

// every accessor of the hit and the list read with the switch at `native`, as words
std::vector<uint32_t> read_all(bool native) {
    const bool was = coll_state_native();
    coll_state_set_native(native);
    std::vector<uint32_t> v;
    v.push_back(coll_hit_moby());
    v.push_back(coll_hit_flags());
    v.push_back(static_cast<uint32_t>(coll_hit_flags_signed()));
    for (const Vec4& x : {coll_hit_point(), coll_hit_pushed(), coll_hit_normal(), coll_hit_tri(0), coll_hit_tri(1), coll_hit_tri(2)})
        for (int k = 0; k < 4; ++k) v.push_back(vw(x, k));
    for (uint32_t k = 0; k < 3; ++k) v.push_back(f2u(coll_hit_point_lane(k)));
    for (uint32_t k = 0; k < 3; ++k) v.push_back(f2u(coll_hit_normal_lane(k)));
    const CollisionHit h = coll_hit();
    v.push_back(h.moby);
    v.push_back(h.flags);
    for (const Vec4& x : {h.point, h.pushed, h.normal, h.tri[0], h.tri[1], h.tri[2]})
        for (int k = 0; k < 4; ++k) v.push_back(vw(x, k));
    for (int i = 0; i < static_cast<int>(kCollHitListWords); ++i) v.push_back(coll_hit_list_at(i));
    v.push_back(static_cast<uint32_t>(coll_hit_list_count()));
    coll_state_set_native(was);
    return v;
}

// the native state against the EE words' decode and the accessors in both modes; "" when equal
std::string state_vs_ee() {
    CollState d;
    coll_state_decode_ee(d);
    const char* field = nullptr;
    if (!coll_state_same(g_coll_state, d, &field)) return std::string("the native state's ") + field + " differs from the EE words'";
    if (read_all(true) != read_all(false)) return "the accessors read other words with native than with ee";
    return "";
}

void check_queries() {
    const char* name = "state_queries";
    if (!selected(name)) return;
    const int n = std::getenv("B543_N") != nullptr ? std::atoi(std::getenv("B543_N")) : 100;
    const uint32_t hero = r32(0x001413d0);
    const uint32_t S = scratch();
    const uint64_t diffs0 = coll_state_differences();
    const bool check_was = coll_state_check();
    coll_state_set_check(true);
    uint32_t rec[9];
    int kinds[5] = {0, 0, 0, 0, 0}, hits[5] = {0, 0, 0, 0, 0}, listed = 0, bad = 0;
    std::string first;
    level();
    for (int k = 0; k < n * 5; ++k) {
        if (k % 50 == 0) level();
        const std::vector<uint32_t> ms = live_mobys();
        const uint32_t from_m = ms.empty() || coin() ? hero : ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
        const int kind = k % 5;
        Vec4 at = around(ee_ref<Vec4>(from_m + 0x10), 0.0f, 3.0f, -0.5f, 2.0f);
        const uint32_t list = r32(OV(0x0015f618));
        if (kind == 3 && list >= 0x1000u && static_cast<int32_t>(r32(list)) > 0 && ri(0, 3) != 0) {  // near a mesh chunk (1/64 units)
            const uint32_t e = list + 0x10 + static_cast<uint32_t>(ri(0, static_cast<int32_t>(r32(list)) - 1)) * 0x10;
            const float rad = static_cast<float>(r16(e + 6)) / 64.0f;
            at = Vec4{static_cast<float>(r16(e)) / 64.0f + rf(-rad, rad), static_cast<float>(r16(e + 2)) / 64.0f + rf(-rad, rad),
                      static_cast<float>(r16(e + 4)) / 64.0f + rf(-rad, rad), 1.0f};
        }
        const uint32_t fl = ri(0, 3) != 0 ? 0u : (ri(0, 2) == 0 ? 2u : (coin() ? 0x10u : 1u));
        const uint32_t ignore = coin() ? from_m : 0u;
        const Vec4 to = around(at, 1.0f, ri(0, 3) == 0 ? 120.0f : 25.0f, -8.0f, 8.0f);
        const float r = rf(0.2f, kind == 3 ? 4.0f : (kind == 4 ? 8.0f : 2.5f)), h = rf(0.1f, 2.0f);
        for (uint32_t& w : rec) w = R();
        const uint32_t* with_rec = ri(0, 3) == 0 ? rec : nullptr;
        int32_t got = 0;
        switch (kind) {
        case 0: got = segment_collide_v(at, to, fl, ignore, with_rec); break;
        case 1: got = sphere_collide_v(at, r, fl & ~0x10u, ignore); break;
        case 2: got = capsule_collide_v(at, r, h, fl & ~0x10u, ignore); break;
        case 3:
            ee_ref<Vec4>(S) = at;
            got = sphere_mesh_collide(S, r);
            break;
        default: got = mobys_in_sphere_v(at, r, coin() ? 0u : 2u, ignore, with_rec); break;
        }
        ++kinds[kind];
        hits[kind] += got != 0 ? 1 : 0;
        if (kind == 4) listed += got;
        const std::string why = state_vs_ee();
        if (!why.empty() && bad++ == 0) first = "query " + std::to_string(k) + " (kind " + std::to_string(kind) + ", flags " + hex(fl) + "): " + why;
    }
    const uint64_t diffs = coll_state_differences() - diffs0;
    coll_state_set_check(check_was);
    level();
    if (bad != 0) {
        fail(name, std::to_string(bad) + " of " + std::to_string(n * 5) + " queries left another state; first: " + first);
        return;
    }
    if (diffs != 0) {
        fail(name, "the check mode found " + std::to_string(diffs) + " difference(s)");
        return;
    }
    std::printf("%-26s ok: %d queries, the native state equal to the EE words after each (segment %d / %d hits, sphere %d / %d, capsule %d / %d, "
                "mesh %d / %d, mobys_in_sphere %d / %d, %d mobys listed)\n",
                name, n * 5, kinds[0], hits[0], kinds[1], hits[1], kinds[2], hits[2], kinds[3], hits[3], kinds[4], hits[4], listed);
}

// random words into the hit's and the list's EE words (an outside writer: plain stores)
void scribble() {
    const uint32_t b = OV(0x00173e40);
    for (uint32_t o = 0x18; o < 0x80; o += 4) w32(b + o, coin() ? R() : (coin() ? 0u : f2u(rf(-500.0f, 500.0f))));
    const uint32_t l = OV(0x00177f00);
    const uint32_t n = static_cast<uint32_t>(ri(0, static_cast<int32_t>(kCollHitListWords)));
    for (uint32_t i = 0; i < kCollHitListWords; ++i) w32(l + i * 4, i < n ? R() : (i == n ? 0u : (coin() ? R() : 0u)));
}

void check_import() {
    const char* name = "state_import";
    if (!selected(name)) return;
    level();
    const bool check_was = coll_state_check();
    coll_state_set_check(false);
    int stale = 0, bad = 0;
    std::string first;
    for (int k = 0; k < 200; ++k) {
        scribble();
        CollState d;
        coll_state_decode_ee(d);
        if (!coll_state_same(g_coll_state, d, nullptr)) ++stale;
        coll_state_import();
        const std::string why = state_vs_ee();
        if (!why.empty() && bad++ == 0) first = "round " + std::to_string(k) + ": " + why;
    }
    coll_state_set_check(check_was);
    level();
    if (bad != 0) {
        fail(name, std::to_string(bad) + " of 200 rounds: " + first);
        return;
    }
    if (stale == 0) {
        fail(name, "no round left the native state behind the EE words before the import (the test tests nothing)");
        return;
    }
    std::printf("%-26s ok: 200 rounds of outside writes, the import equal to the EE words' decode (%d behind before it)\n", name, stale);
}

// one accessor's field: its EE word (the first, for a vector) and a read of it as a word
struct Probe {
    const char* name;
    uint32_t off;  // from the block 0x173e40, or 0x1000 + 4 i for list entry i
    uint32_t (*read)();
};
uint32_t ee_of(uint32_t off) { return off >= 0x1000u ? OV(0x00177f00) + (off - 0x1000u) : OV(0x00173e40) + off; }

void check_check() {
    const char* name = "state_check";
    if (!selected(name)) return;
    level();
    const Probe probes[] = {
        {"coll_hit_moby", 0x18, [] { return coll_hit_moby(); }},
        {"coll_hit_flags", 0x1c, [] { return coll_hit_flags(); }},
        {"coll_hit_flags_signed", 0x1c, [] { return static_cast<uint32_t>(coll_hit_flags_signed()); }},
        {"coll_hit_point", 0x24, [] { return vw(coll_hit_point(), 1); }},
        {"coll_hit_pushed", 0x3c, [] { return vw(coll_hit_pushed(), 3); }},
        {"coll_hit_normal", 0x48, [] { return vw(coll_hit_normal(), 2); }},
        {"coll_hit_tri", 0x50, [] { return vw(coll_hit_tri(0), 0); }},
        {"coll_hit_tri", 0x64, [] { return vw(coll_hit_tri(1), 1); }},
        {"coll_hit_tri", 0x7c, [] { return vw(coll_hit_tri(2), 3); }},
        {"coll_hit_point_lane", 0x28, [] { return f2u(coll_hit_point_lane(2)); }},
        {"coll_hit_normal_lane", 0x40, [] { return f2u(coll_hit_normal_lane(0)); }},
        {"coll_hit (moby)", 0x18, [] { return coll_hit().moby; }},
        {"coll_hit (tri)", 0x68, [] { return vw(coll_hit().tri[1], 2); }},
        {"coll_hit_list_at", 0x1000, [] { return coll_hit_list_at(0); }},
        {"coll_hit_list_at", 0x1000 + 0x1fc, [] { return coll_hit_list_at(0x7f); }},
    };
    const bool check_was = coll_state_check(), native_was = coll_state_native();
    coll_state_set_check(true);
    coll_state_set_check_quiet(true);
    int bad = 0, planted = 0;
    std::string first;
    auto note = [&](const std::string& w) {
        if (bad++ == 0) first = w;
    };
    for (const Probe& p : probes) {
        for (const bool native : {true, false}) {
            coll_state_set_native(native);
            const uint32_t a = ee_of(p.off);
            const uint32_t was = r32(a);
            uint64_t d0 = coll_state_differences();
            const uint32_t v0 = p.read();
            if (coll_state_differences() != d0) note(std::string(p.name) + ": a difference counted with none planted");
            if (v0 != was) note(std::string(p.name) + ": reads " + hex(v0) + ", the word is " + hex(was));
            const uint32_t planted_word = was ^ (1u << ri(0, 31));
            w32(a, planted_word);  // (EE memory alone: the native state keeps `was`)
            d0 = coll_state_differences();
            const uint32_t v1 = p.read();
            ++planted;
            if (coll_state_differences() != d0 + 1)
                note(std::string(p.name) + " (" + (native ? "native" : "ee") + "): " + std::to_string(coll_state_differences() - d0) +
                     " difference(s) counted for one planted at " + hex(a));
            if (v1 != (native ? was : planted_word)) note(std::string(p.name) + " (" + (native ? "native" : "ee") + "): read " + hex(v1));
            w32(a, was);
        }
    }
    coll_state_set_check_quiet(false);
    coll_state_set_check(check_was);
    coll_state_set_native(native_was);
    level();
    if (bad != 0) {
        fail(name, std::to_string(bad) + " problem(s); first: " + first);
        return;
    }
    std::printf("%-26s ok: %d planted differences found, one each, under the accessor read; none without\n", name, planted);
}

void check_setters() {
    const char* name = "state_setters";
    if (!selected(name)) return;
    level();
    const bool check_was = coll_state_check();
    coll_state_set_check(false);
    int bad = 0, n = 0;
    std::string first;
    const uint32_t b = OV(0x00173e40);
    for (int k = 0; k < 50; ++k) {
        const uint8_t* ram = ee_get_ram_base();
        std::vector<uint8_t> before(ram, ram + 0x02000000);
        const Vec4 v{rf(-100.0f, 100.0f), rf(-100.0f, 100.0f), rf(-100.0f, 100.0f), u2f(R())};
        const float z = rf(-100.0f, 100.0f);
        const uint32_t m = R();
        const int which = k % 8;
        std::vector<std::pair<uint32_t, uint32_t>> expect;  // (EE address, word)
        auto vec_at = [&](uint32_t off) {
            for (uint32_t i = 0; i < 4; ++i) expect.push_back({b + off + i * 4, vw(v, static_cast<int>(i))});
        };
        switch (which) {
        case 0: coll_hit_set_moby(m); expect.push_back({b + 0x18, m}); break;
        case 1: coll_hit_set_normal(v); vec_at(0x40); break;
        case 2: coll_hit_set_point(v); vec_at(0x20); break;
        case 3: coll_hit_set_pushed(v); vec_at(0x30); break;
        case 4: coll_hit_set_point_z(z); expect.push_back({b + 0x28, f2u(z)}); break;
        case 5: coll_hit_set_pushed_z(z); expect.push_back({b + 0x38, f2u(z)}); break;
        case 6: coll_hit_clear_normal_z(); expect.push_back({b + 0x48, 0u}); break;
        default: coll_state_list_put(static_cast<uint32_t>(ri(0, 0x7f)), m); break;
        }
        ++n;
        std::string why = state_vs_ee();
        for (const auto& [a, w] : expect)
            if (why.empty() && r32(a) != w) why = "the word at " + hex(a) + " is " + hex(r32(a)) + ", not " + hex(w);
        if (why.empty() && which != 7) {  // nothing else changed
            for (uint32_t a = 0; a < 0x02000000u && why.empty(); a += 4) {
                bool listed = false;
                for (const auto& e : expect) listed = listed || e.first == a;
                if (!listed && std::memcmp(&before[a], ram + a, 4) != 0) why = "the word at " + hex(a) + " changed too";
            }
        }
        if (!why.empty() && bad++ == 0) first = "setter " + std::to_string(which) + ": " + why;
    }
    coll_state_set_check(check_was);
    level();
    if (bad != 0) {
        fail(name, std::to_string(bad) + " of " + std::to_string(n) + ": " + first);
        return;
    }
    std::printf("%-26s ok: %d setter calls, the native field and the EE word(s) written, nothing else\n", name, n);
}

}  // namespace

void batch_543() {
    if (g_only_batch != 543) return;
    g_batch = 543;
    std::printf("batch 543: the collision's native results on level %d\n", g_oracle_level);
    check_queries();
    check_import();
    check_check();
    check_setters();
}

}  // namespace rac1::oracle
