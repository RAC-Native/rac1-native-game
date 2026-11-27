// Oracle batch 545 (#150 stage 5, task LD4): the level data's typed read API (src/game/level/level_read.h) with
// RAC1_LD_READ=ee against native - a host check at the boundary, no original code run. On the level after its setup
// (level_setup_body; the base registry built by gameplay_load) and again after one gameplay frame:
//  - every record of every kind of the base registry (each path and its points, each rail's spine, each volume, each shape of
//    the three lists, each level camera, each rail): every accessor of its family (the word reads at every word and half,
//    the rows, the named fields: path_n, path_head, path_pt, path_lane(_bits), PathView, shape_row / _centre / _mat /
//    _to_unit, volume_radius, camera_pos / _type / _rot / _data_of, rail_centre / _reach / _spine / _loop, the copy_row forms
//    into the test area) gives the same bits with ee and with native, and native reads no word from EE but a rail's +0x18 /
//    +0x1c (not held: counted as misses, one each);
//  - the check mode: a word of each kind changed in EE by hand (not through level_write.h) is reported by the native read
//    ("[ld-read] differs", counted), which returns the native word; put back;
//  - a miss: a native read at an address no record holds (the test area) returns the EE word and is counted by its accessor;
//  - the pull: EE words of each kind changed by hand, then registry_pull_from_ee: native follows (the new words, no
//    difference); a PathView made before the pull reads the new words too (its generation is gone); put back and pulled;
//  - the cost: a pull's time and a native read's against an ee read's (printed, not checked).
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 545, N = 0 .. 18.

#include "ovl_oracle.h"

#include <game/level/level_data.h>
#include <game/level/level_read.h>
#include <game/level/level_registry.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {

bool fail(const char* what, const std::string& why) {
    std::printf("%-26s FAIL: %s\n", what, why.c_str());
    ++g_fail;
    return false;
}

void level_setup() {
    veldin_boot();
    level_setup_body(1, 0);
}

// a record of the base registry: its kind's name, its EE address, its size
struct Rec {
    const char* kind;
    int fam;  // 0 path, 1 shape, 2 volume, 3 camera, 4 rail
    uint32_t ee, size;
    int32_t points;  // a path's parsed points
};

std::vector<Rec> records() {
    std::vector<Rec> v;
    const lvd::LevelRegistry* r = lvd::registry_base();
    if (r == nullptr) return v;
    for (size_t i = 0; i < r->paths.size() && i < r->path_ee.size(); ++i)
        if (r->path_ee[i] != 0) v.push_back({"path", 0, r->path_ee[i], 0x10 + 0x10 * static_cast<uint32_t>(r->paths[i].points.size()), static_cast<int32_t>(r->paths[i].points.size())});
    for (size_t i = 0; i < r->rails.size() && i < r->spine_ee.size(); ++i)
        if (r->spine_ee[i] != 0)
            v.push_back({"rail spine", 0, r->spine_ee[i], 0x10 + 0x10 * static_cast<uint32_t>(r->rails[i].spine.points.size()), static_cast<int32_t>(r->rails[i].spine.points.size())});
    for (size_t i = 0; i < r->volumes.size() && i < r->volume_ee.size(); ++i) v.push_back({"volume", 2, r->volume_ee[i], 0x90, 0});
    for (int l = 0; l < 3; ++l)
        for (size_t i = 0; i < r->shapes[l].size() && i < r->shape_ee[l].size(); ++i) v.push_back({l == 0 ? "box" : l == 1 ? "sphere" : "cylinder", 1, r->shape_ee[l][i], 0x80, 0});
    for (size_t i = 0; i < r->cameras.size() && i < r->camera_ee.size(); ++i) v.push_back({"camera", 3, r->camera_ee[i], 0x20, 0});
    for (size_t i = 0; i < r->rails.size() && i < r->rail_ee.size(); ++i) v.push_back({"rail", 4, r->rail_ee[i], 0x20, 0});
    return v;
}

void put_bits(std::vector<uint32_t>& out, const Vec4& v) {
    uint32_t w[4];
    std::memcpy(w, &v, 16);
    out.insert(out.end(), w, w + 4);
}
void put_mat(std::vector<uint32_t>& out, const Mat4& m) {
    uint32_t w[16];
    std::memcpy(w, &m, 64);
    out.insert(out.end(), w, w + 16);
}

// every accessor's value for record `r`, in the current mode (bits, in a fixed order)
std::vector<uint32_t> values(const Rec& r) {
    std::vector<uint32_t> o;
    using U = uint32_t (*)(uint32_t);
    using S = int32_t (*)(uint32_t);
    using F = float (*)(uint32_t);
    using H = uint16_t (*)(uint32_t);
    using HS = int16_t (*)(uint32_t);
    using R = Vec4 (*)(uint32_t);
    using C = void (*)(uint32_t, uint32_t);
    static const U u[5] = {lvd::path_u32, lvd::shape_u32, lvd::volume_u32, lvd::camera_u32, lvd::rail_u32};
    static const S s[5] = {lvd::path_s32, lvd::shape_s32, lvd::volume_s32, lvd::camera_s32, lvd::rail_s32};
    static const F f[5] = {lvd::path_f32, lvd::shape_f32, lvd::volume_f32, lvd::camera_f32, lvd::rail_f32};
    static const H h[5] = {lvd::path_u16, lvd::shape_u16, lvd::volume_u16, lvd::camera_u16, lvd::rail_u16};
    static const HS hs[5] = {lvd::path_s16, lvd::shape_s16, lvd::volume_s16, lvd::camera_s16, lvd::rail_s16};
    static const R row[5] = {lvd::path_row, lvd::shape_row, lvd::volume_row, lvd::camera_row, lvd::rail_row};
    static const C cp[5] = {lvd::path_copy_row, lvd::shape_copy_row, lvd::volume_copy_row, lvd::camera_copy_row, lvd::rail_copy_row};
    const int k = r.fam;
    const uint32_t words = r.fam == 4 ? 0x18 : r.size;  // (a rail's +0x18 / +0x1c: the misses' check)
    for (uint32_t off = 0; off < words; off += 4) {
        const uint32_t a = r.ee + off;
        o.push_back(u[k](a));
        o.push_back(static_cast<uint32_t>(s[k](a)));
        o.push_back(std::bit_cast<uint32_t>(f[k](a)));
        o.push_back(h[k](a));
        o.push_back(h[k](a + 2));
        o.push_back(static_cast<uint32_t>(static_cast<int32_t>(hs[k](a + 2))));
    }
    for (uint32_t off = 0; off + 16 <= words; off += 16) {
        put_bits(o, row[k](r.ee + off));
        cp[k](OUT, r.ee + off);
        for (uint32_t i = 0; i < 4; ++i) o.push_back(lwu(OUT + 4 * i));
    }
    switch (r.fam) {
    case 0: {
        o.push_back(static_cast<uint32_t>(lvd::path_n(r.ee)));
        for (int32_t i = 0; i < 3; ++i) o.push_back(lvd::path_head(r.ee, i));
        const lvd::PathView pv(r.ee);
        o.push_back(static_cast<uint32_t>(pv.n()));
        for (int32_t p = 0; p < r.points; ++p) {
            put_bits(o, lvd::path_pt(r.ee, p));
            put_bits(o, pv.row(lvd::path_pt_addr(r.ee, p)));
            for (int32_t l = 0; l < 4; ++l) {
                o.push_back(lvd::path_lane_bits(r.ee, p, l));
                o.push_back(std::bit_cast<uint32_t>(lvd::path_lane(r.ee, p, l)));
                o.push_back(pv.u32(lvd::path_pt_addr(r.ee, p) + 4 * static_cast<uint32_t>(l)));
                o.push_back(std::bit_cast<uint32_t>(pv.f32(lvd::path_pt_addr(r.ee, p) + 4 * static_cast<uint32_t>(l))));
            }
        }
        for (uint32_t off = 0; off < 0x10; off += 4) o.push_back(pv.u32(r.ee + off));
        break;
    }
    case 1:
        for (int32_t i = 0; i < 8; ++i) put_bits(o, lvd::shape_row(r.ee, i));
        put_bits(o, lvd::shape_centre(r.ee));
        put_mat(o, lvd::shape_mat(r.ee));
        put_mat(o, lvd::shape_to_unit(r.ee));
        break;
    case 2:
        for (int32_t i = 0; i < 9; ++i) put_bits(o, lvd::volume_row(r.ee, i));
        put_bits(o, lvd::volume_centre(r.ee));
        put_mat(o, lvd::volume_mat(r.ee));
        put_mat(o, lvd::volume_to_unit(r.ee));
        o.push_back(std::bit_cast<uint32_t>(lvd::volume_radius(r.ee)));
        // (a volume's first 0x80 bytes through the shapes' accessors: the same layout)
        put_bits(o, lvd::shape_centre(r.ee));
        break;
    case 3:
        put_bits(o, lvd::camera_pos(r.ee));
        o.push_back(static_cast<uint32_t>(lvd::camera_type(r.ee)));
        put_bits(o, lvd::camera_rot(r.ee));
        o.push_back(lvd::camera_data_of(r.ee));
        break;
    default: {
        put_bits(o, lvd::rail_centre(r.ee));
        o.push_back(std::bit_cast<uint32_t>(lvd::rail_reach(r.ee)));
        o.push_back(lvd::rail_spine(r.ee));
        o.push_back(static_cast<uint32_t>(lvd::rail_loop(r.ee)));
        break;
    }
    }
    return o;
}

struct ModeScope {
    const lvd::ReadMode mode = lvd::read_mode();
    const bool check = lvd::read_check();
    ~ModeScope() {
        lvd::set_read_mode(mode);
        lvd::set_read_check(check);
    }
};

// every record's accessors: ee against native (the check mode on and off), and the misses only where expected
bool check_equal(const char* name) {
    const ModeScope keep;
    const std::vector<Rec> recs = records();
    if (recs.empty()) return fail(name, "no record");
    size_t values_n = 0, kinds[5] = {};
    for (int pass = 0; pass < 2; ++pass) {
        lvd::set_read_check(pass == 1);
        const lvd::ReadStats before = lvd::read_stats_total();
        for (const Rec& r : recs) {
            lvd::set_read_mode(lvd::ReadMode::EE);
            const std::vector<uint32_t> e = values(r);
            lvd::set_read_mode(lvd::ReadMode::NATIVE);
            const std::vector<uint32_t> n = values(r);
            if (e.size() != n.size()) return fail(name, "value counts differ");
            for (size_t i = 0; i < e.size(); ++i)
                if (e[i] != n[i]) {
                    char b[160];
                    std::snprintf(b, sizeof b, "%s at %08x: value %zu: ee %08x, native %08x (check %s)", r.kind, r.ee, i, e[i], n[i], pass ? "on" : "off");
                    return fail(name, b);
                }
            if (pass == 0) {
                values_n += e.size();
                ++kinds[r.fam];
            }
        }
        lvd::set_read_mode(lvd::ReadMode::NATIVE);
        // the rails' +0x18 / +0x1c: not held, a miss each (rail_u32)
        size_t rails = 0;
        for (const Rec& r : recs)
            if (r.fam == 4) {
                if (lvd::rail_u32(r.ee + 0x18) != lwu(r.ee + 0x18) || lvd::rail_u32(r.ee + 0x1c) != lwu(r.ee + 0x1c)) return fail(name, "a rail's +0x18 / +0x1c");
                ++rails;
            }
        const lvd::ReadStats after = lvd::read_stats_total();
        if (after.misses - before.misses != 2 * rails) {
            char b[120];
            std::snprintf(b, sizeof b, "%llu misses, %zu expected (the rails' unheld words)", static_cast<unsigned long long>(after.misses - before.misses), 2 * rails);
            return fail(name, b);
        }
        if (after.differs != before.differs) return fail(name, "the check mode found a difference");
    }
    std::printf("%-26s ok: %zu records (%zu paths and spines, %zu shapes, %zu volumes, %zu cameras, %zu rails), %zu values alike with ee and native\n", name,
                recs.size(), kinds[0], kinds[1], kinds[2], kinds[3], kinds[4], values_n);
    return true;
}

// a word of each kind changed in EE by hand: the check mode reports it, the native read gives the native word; a miss
bool check_mode(const char* name) {
    const ModeScope keep;
    const std::vector<Rec> recs = records();
    lvd::set_read_mode(lvd::ReadMode::NATIVE);
    lvd::set_read_check(true);
    int planted = 0, found = 0;
    for (int fam = 0; fam < 5; ++fam) {
        const Rec* r = nullptr;
        for (const Rec& x : recs)
            if (x.fam == fam && (fam != 0 || x.points > 0)) {
                r = &x;
                break;
            }
        if (r == nullptr) continue;
        const uint32_t a = fam == 0 ? r->ee + 0x18 : fam == 4 ? r->ee + 0xc : fam == 3 ? r->ee + 0x10 : r->ee + 0x30;
        const uint32_t keep_w = lwu(a);
        w32(a, keep_w ^ 0x00400000u);
        ++planted;
        const uint64_t d0 = lvd::read_stats_total().differs;
        uint32_t got = 0;
        switch (fam) {
        case 0: got = lvd::path_lane_bits(r->ee, 0, 2); break;
        case 1: got = lvd::shape_u32(a); break;
        case 2: got = std::bit_cast<uint32_t>(lvd::volume_centre(r->ee).x); break;
        case 3: got = lvd::camera_u32(a); break;
        default: got = std::bit_cast<uint32_t>(lvd::rail_reach(r->ee)); break;
        }
        if (lvd::read_stats_total().differs == d0 + 1 && got == keep_w) ++found;
        w32(a, keep_w);
    }
    // a miss: the test area holds no record
    w32(T4, 0x12345678u);
    const uint64_t m0 = lvd::read_stats(lvd::ReadAcc::PATH_U32).misses;
    const bool miss_ok = lvd::path_u32(T4) == 0x12345678u && lvd::read_stats(lvd::ReadAcc::PATH_U32).misses == m0 + 1;
    if (planted == 0) return fail(name, "no record");
    if (found != planted) return fail(name, std::to_string(found) + " of " + std::to_string(planted) + " planted differences reported");
    if (!miss_ok) return fail(name, "a miss not read from EE or not counted");
    std::printf("%-26s ok: %d planted differences reported (the native word returned), a miss read from EE and counted\n", name, planted);
    return true;
}

// EE words of each kind changed by hand, then the pull: native follows; a PathView made before reads the new words
bool check_pull(const char* name) {
    const ModeScope keep;
    const std::vector<Rec> recs = records();
    lvd::set_read_mode(lvd::ReadMode::NATIVE);
    lvd::set_read_check(true);
    std::vector<std::pair<uint32_t, uint32_t>> at;  // (address, the word it had)
    uint32_t path = 0;
    for (int fam = 0; fam < 5; ++fam)
        for (const Rec& x : recs)
            if (x.fam == fam && (fam != 0 || x.points > 1)) {
                const uint32_t a = fam == 0 ? x.ee + 0x24 : fam >= 3 ? x.ee + 0x4 : x.ee + 0x34;
                if (fam == 0 && path == 0) path = x.ee;
                at.emplace_back(a, lwu(a));
                break;
            }
    if (at.empty()) return fail(name, "no record");
    const lvd::PathView view(path != 0 ? path : at[0].first);
    for (auto& [a, w] : at) w32(a, w ^ 0x00010000u);
    const uint64_t d0 = lvd::read_stats_total().differs;
    lvd::registry_pull_from_ee();
    bool ok = true;
    for (auto& [a, w] : at) {
        const lvd::RecordRef r = lvd::record_at(a);
        uint32_t n = 0;
        switch (r.kind) {
        case lvd::RecordKind::PATH: case lvd::RecordKind::SPINE: case lvd::RecordKind::ADDED_PATH: n = lvd::path_u32(a); break;
        case lvd::RecordKind::SHAPE: n = lvd::shape_u32(a); break;
        case lvd::RecordKind::VOLUME: n = lvd::volume_u32(a); break;
        case lvd::RecordKind::CAMERA: n = lvd::camera_u32(a); break;
        case lvd::RecordKind::RAIL: n = lvd::rail_u32(a); break;
        default: ok = false;
        }
        if (n != (w ^ 0x00010000u)) ok = false;
    }
    if (path != 0 && view.u32(path + 0x24) != lwu(path + 0x24)) ok = false;
    for (auto& [a, w] : at) w32(a, w);
    lvd::registry_pull_from_ee();
    for (auto& [a, w] : at)
        if (lvd::record_at(a).kind == lvd::RecordKind::PATH && lvd::path_u32(a) != w) ok = false;
    if (lvd::read_stats_total().differs != d0) ok = false;
    if (!ok) return fail(name, "native does not follow the pulled EE words");
    std::printf("%-26s ok: %zu words changed in EE, pulled: native follows (a view made before too); put back, pulled\n", name, at.size());
    return true;
}

// the path table's slots past the count (printed only: a reader handed such a slot's address reads EE, a miss)
void report_table(const char* name) {
    const int32_t n = lvd::path_count();
    int past = 0, held = 0;
    std::string first;
    for (int32_t i = std::max(n, 0); i < 176; ++i) {
        const uint32_t a = lvd::path(i);
        if (a == 0) continue;
        ++past;
        if (lvd::record_at(a)) ++held;
        if (past <= 4) {
            char b[24];
            std::snprintf(b, sizeof b, " %d:%08x", i, a);
            first += b;
        }
    }
    std::printf("%-26s ok: %d paths; %d slots past the count non-zero (%d of them inside a record)%s\n", name, n, past, held, first.c_str());
}

// the cost of a pull and of a read (printed only)
void report_cost(const char* name) {
    const ModeScope keep;
    lvd::set_read_check(false);
    const std::vector<Rec> recs = records();
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 50; ++i) lvd::registry_pull_from_ee();
    const double pull_us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / 50.0;
    double ns[2] = {};
    uint64_t words = 0;
    uint32_t sink = 0;
    for (int m = 0; m < 2; ++m) {
        lvd::set_read_mode(m == 0 ? lvd::ReadMode::EE : lvd::ReadMode::NATIVE);
        words = 0;
        const auto t1 = std::chrono::steady_clock::now();
        for (int pass = 0; pass < 20; ++pass)
            for (const Rec& r : recs)
                if (r.fam == 0)
                    for (uint32_t off = 0; off < r.size; off += 4, ++words) sink += lvd::path_u32(r.ee + off);
        ns[m] = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t1).count() / static_cast<double>(words ? words : 1);
    }
    std::printf("%-26s ok: a pull %.1f us (%zu records); a path word read %.1f ns ee, %.1f ns native (%llu words a pass; %08x)\n", name, pull_us, recs.size(),
                ns[0], ns[1], static_cast<unsigned long long>(words / 20), sink & 0u);
}

}  // namespace

void batch_545() {
    if (g_only_batch != 545) return;
    g_batch = 545;
    std::printf("batch 545: the level data's read API (ee against native) on level %d\n", g_oracle_level);
    level_setup();
    if (lvd::registry_base() == nullptr) {
        fail("registry_base", "no registry after the level's setup");
        return;
    }
    check_equal("ld_read_equal");
    check_mode("ld_read_check");
    check_pull("ld_read_pull");
    report_cost("ld_read_cost");
    report_table("ld_read_table");
    // after a gameplay frame (the mobys' first updates: the writers write both, the readers read either)
    w32(OV(0x0016c060), 0xf);
    w32(0x0013f2e8, 0xffffffffu);
    if (g_oracle_level == 14) w8(0x0013d3f0, 1);
    gameplay_frame();
    check_equal("ld_read_equal_frame");
}

}  // namespace rac1::oracle
