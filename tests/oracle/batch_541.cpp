// Oracle batch 541 (#150 stage 5, task L2): the native level registry (src/game/level/level_registry.h) against the EE tables
// the level's load lays out - a host check at the boundary, no original code run. On the level after its setup
// (level_setup_body, whose gameplay_load builds the registry at its end):
//  - registry_base: every path (the table's offsets, the count word, the 12 bytes after it, every point), every 0x90-byte
//    volume, every shape of the three lists, every camera (with its pvar block's address), every rail (centre, reach, loop
//    word, its spine's points), the camera-volume grid's and the zone grid's bytes equal the EE tables word by word;
//  - the level's gameplay file read again from the disc and unpacked by the host (decompress_wad, not the game's
//    wad_decompress into EE memory) and parsed into a second registry: the same objects as the first;
//  - the cameras' pvar blocks: none exactly where the file's pvar index is negative;
//  - after one gameplay frame: equal again (the run-time writers write the native records too, level_write.h);
//  - the comparison itself: a fault put into one word of each object kind's EE table (a record's word changed by far more
//    than an ulp, a count, a table entry) must be found, named with that object and index; every fault is undone;
//  - (LD1) every record's EE address equals the table's word that names it; the address index gives each record for its first
//    and last byte and not for the byte past it (and the lookups' cost); a write through each function of the write facade
//    (level_write.h) changes the EE word and the native record alike (then put back); an added path (ld_path_reserve,
//    ld_path_set) is indexed with its points, dropped by ld_base_reset; EE words of each kind changed by hand, then
//    registry_pull_from_ee: the native records equal EE again (then put back and pulled again);
//  - (LD3) a write through each of the paths' write calls (level_write.h) changes the EE words and the native record alike (a
//    lane, a row, a copied row, two rows a callee wrote, the count lowered and raised, a rail spine's lane; then put back);
//    after the gameplay frame the paths' run-time writers have written their native records too: nothing differs.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 541, N = 0 .. 18.

#include "ovl_oracle.h"

#include <formats/scene_wad.h>
#include <game/enhance/leveldata.h>
#include <game/level/level_data.h>
#include <game/level/level_registry.h>
#include <game/level/level_write.h>
#include <runtime/kernel/disc.h>

#include <array>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {

bool fail(const char* what, const std::string& why) {
    std::printf("%-26s FAIL: %s\n", what, why.c_str());
    ++g_fail;
    return false;
}

void print_diffs(const std::vector<lvd::RegistryDiff>& d, size_t n) {
    for (size_t k = 0; k < d.size() && k < n; ++k) std::printf("    %s\n", d[k].text().c_str());
}

// the level after its setup (no frame yet)
void level_setup() {
    veldin_boot();
    level_setup_body(1, 0);
}

void check_tables(const char* name) {
    std::vector<lvd::RegistryDiff> d;
    const int n = lvd::registry_compare_base(d, 16);
    if (n < 0) {
        fail(name, "no registry was built");
        return;
    }
    if (n != 0) {
        fail(name, std::to_string(n) + " differing words");
        print_diffs(d, 16);
        return;
    }
    const lvd::LevelRegistry& r = *lvd::registry_base();
    size_t points = 0;
    for (const lvd::PathSpline& p : r.paths) points += p.points.size();
    std::printf("%-26s ok: %zu paths (%zu points), %zu volumes, %zu / %zu / %zu shapes, %zu cameras, %zu rails, grids %zu + %zu bytes\n", name, r.paths.size(), points,
                r.volumes.size(), r.shapes[0].size(), r.shapes[1].size(), r.shapes[2].size(), r.cameras.size(), r.rails.size(), r.cam_volumes.bytes.size(),
                r.zones.bytes.size());
}

void check_disc() {
    const char* name = "registry_disc_file";
    std::vector<std::byte> wad;
    if (!disc_read_level_wad(g_oracle_level, wad) || wad.size() < 0x30) {
        fail(name, "the level's file cannot be read");
        return;
    }
    // (the gameplay file's sectors in the level file's header: +0x18 / +0x1c, on another region's disc +0x20 / +0x24, as pool.cpp)
    uint32_t go = 0, gn = 0;
    std::memcpy(&go, wad.data() + (region_other() ? 0x20 : 0x18), 4);
    std::memcpy(&gn, wad.data() + (region_other() ? 0x24 : 0x1c), 4);
    const size_t ga = static_cast<size_t>(go) * 2048, gs = static_cast<size_t>(gn) * 2048;
    std::vector<uint8_t> file;
    if (gn == 0 || ga + gs > wad.size() || !decompress_wad(std::span<const std::byte>(wad.data() + ga, gs), file)) {
        fail(name, "the gameplay file cannot be unpacked");
        return;
    }
    lvd::LevelRegistry other;
    std::string error;
    if (!lvd::registry_parse(file, g_oracle_level, other, &error)) {
        fail(name, "not parsed: " + error);
        return;
    }
    std::vector<lvd::RegistryDiff> d;
    const int n = lvd::registry_compare(*lvd::registry_base(), other, d, 16);
    if (n != 0) {
        fail(name, std::to_string(n) + " differences (the load's file, the disc's)");
        print_diffs(d, 16);
        return;
    }
    std::printf("%-26s ok: the disc's gameplay file (%zu KB unpacked by the host) gives the same registry%s%s\n", name, file.size() >> 10,
                error.empty() ? "" : "; ", error.c_str());
}

void check_pvars() {
    const char* name = "registry_camera_pvars";
    const lvd::LevelRegistry& r = *lvd::registry_base();
    int bad = 0;
    for (size_t k = 0; k < r.cameras.size(); ++k) {
        const bool none = r.cameras[k].placement.pvar_index < 0;
        if (none != (r.cameras[k].pvars == 0)) {
            std::printf("    camera %zu: pvar index %d, its block %08x\n", k, r.cameras[k].placement.pvar_index, r.cameras[k].pvars);
            ++bad;
        }
    }
    if (bad != 0) {
        fail(name, std::to_string(bad) + " cameras");
        return;
    }
    std::printf("%-26s ok: %zu cameras\n", name, r.cameras.size());
}

// One fault: the EE word at `a` changed, the comparison must report `object` (and `index` unless -2), then the word back
struct Fault {
    std::string object;
    int32_t index;
    uint32_t a;
};

void check_faults() {
    const char* name = "registry_faults";
    const lvd::LevelRegistry& r = *lvd::registry_base();
    {
        std::vector<lvd::RegistryDiff> d;
        if (lvd::registry_compare_base(d, 1) != 0) {
            fail(name, "not run: the tables differ before any fault");
            return;
        }
    }
    std::vector<Fault> f;
    const auto pick = [](size_t n) { return static_cast<int32_t>(ri(0, static_cast<int32_t>(n) - 1)); };
    f.push_back({"path count", -1, OV(0x00160104)});
    for (int t = 0; t < 4 && !r.paths.empty(); ++t) {
        const int32_t i = pick(r.paths.size());
        f.push_back({"path table", i, OV(0x001b04b0) + 4 * u(i)});
        const lvd::PathSpline& p = r.paths[static_cast<size_t>(i)];
        const uint32_t words = 4 + 4 * static_cast<uint32_t>(p.points.size());
        f.push_back({"path", i, lvd::path(i) + 4 * u(ri(0, static_cast<int32_t>(words) - 1))});
    }
    for (int t = 0; t < 3 && !r.volumes.empty(); ++t) {
        const int32_t i = pick(r.volumes.size());
        f.push_back({"volume", i, lvd::volume(i) + 4 * u(ri(0, 35))});
    }
    static const char* const kShape[3] = {"box", "sphere", "cylinder"};
    for (int l = 0; l < 3; ++l)
        for (int t = 0; t < 3 && !r.shapes[static_cast<size_t>(l)].empty(); ++t) {
            const int32_t i = pick(r.shapes[static_cast<size_t>(l)].size());
            f.push_back({kShape[l], i, lvd::box(static_cast<lvd::BoxList>(l), i) + 4 * u(ri(0, 31))});
        }
    for (int t = 0; t < 3 && !r.cameras.empty(); ++t) {
        const int32_t i = pick(r.cameras.size());
        f.push_back({"camera", i, lvd::camera(i) + 4 * u(ri(0, 7))});
    }
    for (int t = 0; t < 3 && !r.rails.empty(); ++t) {
        const int32_t i = pick(r.rails.size());
        static const uint32_t kRailWords[] = {0, 4, 8, 0xc, 0x14};
        f.push_back({"rail", i, lvd::rail(i) + kRailWords[ri(0, 4)]});
        const lvd::PathSpline& s = r.rails[static_cast<size_t>(i)].spine;
        f.push_back({"rail spine", i, lwu(lvd::rail(i) + 0x10) + 4 * u(ri(0, static_cast<int32_t>(3 + 4 * s.points.size())))});
    }
    if (!r.cam_volumes.empty())
        for (int t = 0; t < 2; ++t) f.push_back({"camera-volume grid", -1, lvd::cam_volume_grid() + 4 * u(ri(0, static_cast<int32_t>(r.cam_volumes.bytes.size() / 4) - 1))});
    if (!r.zones.empty())
        for (int t = 0; t < 2; ++t) f.push_back({"zone grid", -1, lvd::zone_grid() + 4 * u(ri(0, static_cast<int32_t>(r.zones.bytes.size() / 4) - 1))});
    int caught = 0;
    for (const Fault& x : f) {
        const uint32_t old = ee_read<uint32_t>(x.a);
        w32(x.a, old ^ 0x00410000u);
        std::vector<lvd::RegistryDiff> d;
        const int n = lvd::registry_compare_base(d, 4);
        w32(x.a, old);
        const bool named = n > 0 && d[0].object == x.object && (x.index == -1 || d[0].index == x.index);
        if (named) {
            ++caught;
        } else {
            std::printf("    fault at %08x (%s %d) %s%s\n", x.a, x.object.c_str(), x.index, n == 0 ? "not found" : "reported as ", n == 0 ? "" : d[0].text().c_str());
        }
    }
    std::vector<lvd::RegistryDiff> d;
    if (lvd::registry_compare_base(d, 4) != 0) {
        fail(name, "a fault was not undone");
        return;
    }
    if (caught != static_cast<int>(f.size())) {
        fail(name, std::to_string(f.size() - static_cast<size_t>(caught)) + " of " + std::to_string(f.size()) + " faults not found or misnamed");
        return;
    }
    std::printf("%-26s ok: %d of %zu faults found and named\n", name, caught, f.size());
}

// ---- LD1 ----
uint32_t path_size(const lvd::PathSpline& p) { return 0x10 + 0x10 * static_cast<uint32_t>(p.points.size()); }

// each record's EE address against the tables (through the facade's accessors)
void check_addresses() {
    const char* name = "registry_addresses";
    const lvd::LevelRegistry& r = *lvd::registry_base();
    int bad = 0;
    size_t n = 0;
    const auto eq = [&](const char* what, size_t i, uint32_t rec, uint32_t table) {
        ++n;
        if (rec == table) return;
        if (++bad <= 8) std::printf("    %s %zu: the record's %08x, the table's %08x\n", what, i, rec, table);
    };
    for (size_t i = 0; i < r.paths.size(); ++i) eq("path", i, r.path_ee[i], lvd::path(static_cast<int32_t>(i)));
    for (size_t i = 0; i < r.volumes.size(); ++i) eq("volume", i, r.volume_ee[i], lvd::volume(static_cast<int32_t>(i)));
    for (int l = 0; l < 3; ++l)
        for (size_t i = 0; i < r.shapes[static_cast<size_t>(l)].size(); ++i)
            eq("shape", i, r.shape_ee[static_cast<size_t>(l)][i], lvd::box(static_cast<lvd::BoxList>(l), static_cast<int32_t>(i)));
    for (size_t i = 0; i < r.cameras.size(); ++i) eq("camera", i, r.camera_ee[i], lvd::camera(static_cast<int32_t>(i)));
    for (size_t i = 0; i < r.rails.size(); ++i) {
        eq("rail", i, r.rail_ee[i], lvd::rail(static_cast<int32_t>(i)));
        eq("rail spine", i, r.spine_ee[i], lwu(lvd::rail(static_cast<int32_t>(i)) + 0x10));
    }
    if (bad != 0) {
        fail(name, std::to_string(bad) + " of " + std::to_string(n) + " addresses differ");
        return;
    }
    std::printf("%-26s ok: %zu records' addresses equal the tables'\n", name, n);
}

// record_at of each record's first and last byte and of the byte past it
void check_index() {
    const char* name = "registry_index";
    const lvd::LevelRegistry& r = *lvd::registry_base();
    int bad = 0;
    size_t n = 0;
    std::vector<uint32_t> firsts;
    const auto one = [&](lvd::RecordKind k, int list, size_t i, uint32_t ee, uint32_t size) {
        if (ee == 0 || size == 0) return;
        ++n;
        firsts.push_back(ee);
        const lvd::RecordRef a = lvd::record_at(ee), b = lvd::record_at(ee + size - 1), c = lvd::record_at(ee + size);
        const auto is = [&](const lvd::RecordRef& x, uint32_t off) {
            return x.kind == k && x.key == lvd::KEY_BASE && x.list == list && x.index == static_cast<int32_t>(i) && x.offset == off && x.ee == ee && x.size == size;
        };
        const bool past = !c || c.ee != ee;
        if (is(a, 0) && is(b, size - 1) && past) return;
        if (++bad <= 8)
            std::printf("    %s %zu at %08x (+%#x): first %s %d +%#x, last %s %d +%#x, past %s %d\n", lvd::record_kind_name(k, list), i, ee, size, lvd::record_kind_name(a.kind, a.list),
                        a.index, a.offset, lvd::record_kind_name(b.kind, b.list), b.index, b.offset, lvd::record_kind_name(c.kind, c.list), c.index);
    };
    using K = lvd::RecordKind;
    for (size_t i = 0; i < r.paths.size(); ++i) one(K::PATH, 0, i, r.path_ee[i], path_size(r.paths[i]));
    for (size_t i = 0; i < r.volumes.size(); ++i) one(K::VOLUME, 0, i, r.volume_ee[i], 0x90);
    for (int l = 0; l < 3; ++l)
        for (size_t i = 0; i < r.shapes[static_cast<size_t>(l)].size(); ++i) one(K::SHAPE, l, i, r.shape_ee[static_cast<size_t>(l)][i], 0x80);
    for (size_t i = 0; i < r.cameras.size(); ++i) one(K::CAMERA, 0, i, r.camera_ee[i], 0x20);
    for (size_t i = 0; i < r.rails.size(); ++i) {
        one(K::RAIL, 0, i, r.rail_ee[i], 0x20);
        one(K::SPINE, 0, i, r.spine_ee[i], path_size(r.rails[i].spine));
    }
    const lvd::IndexStats st = lvd::index_stats();
    if (st.overlaps != 0) {
        ++bad;
        std::printf("    %zu pairs of records overlap\n", st.overlaps);
    }
    if (bad != 0) {
        fail(name, std::to_string(bad) + " of " + std::to_string(n) + " records");
        return;
    }
    // the cost: lookups of every record's bytes in turn
    uint64_t found = 0, k = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int rep = 0; rep < 200 && !firsts.empty(); ++rep)
        for (const uint32_t a : firsts) {
            found += lvd::record_at(a + static_cast<uint32_t>(k & 0xc)) ? 1 : 0;
            ++k;
        }
    const double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / static_cast<double>(k == 0 ? 1 : k);
    std::printf("%-26s ok: %zu records found by their first and last byte, none by the byte past it; %zu indexed in all, no overlap; %.0f ns a lookup (%llu found)\n", name,
                n, st.records, ns, static_cast<unsigned long long>(found));
}

// the native word of EE address `a` (through the index), and whether one is held
bool native_word(uint32_t a, uint32_t& out) {
    const lvd::RecordRef r = lvd::record_at(a);
    const void* slot = r ? lvd::record_slot(r) : nullptr;
    if (slot == nullptr) return false;
    std::memcpy(&out, slot, 4);
    return true;
}

// a write through each facade function: the EE words and the native record alike; then put back
void check_facade() {
    const char* name = "registry_write_facade";
    const lvd::LevelRegistry& r = *lvd::registry_base();
    int bad = 0, done = 0;
    const auto same = [&](const char* what, uint32_t a, uint32_t words, const uint32_t* want) {
        ++done;
        for (uint32_t k = 0; k < words; ++k) {
            uint32_t nat = 0;
            const uint32_t ee = lwu(a + 4 * k);
            if (!native_word(a + 4 * k, nat) || nat != ee || ee != want[k]) {
                if (++bad <= 8) std::printf("    %s at %08x +%u: EE %08x, native %08x, written %08x\n", what, a, 4 * k, ee, nat, want[k]);
                return;
            }
        }
    };
    // (a shape of the first non-empty list, a volume)
    uint32_t shape = 0;
    for (int l = 0; l < 3 && shape == 0; ++l)
        if (!r.shapes[static_cast<size_t>(l)].empty()) shape = lvd::box(static_cast<lvd::BoxList>(l), 0);
    if (shape != 0) {
        uint32_t keep[32];
        for (uint32_t k = 0; k < 32; ++k) keep[k] = lwu(shape + 4 * k);
        const float f = std::bit_cast<float>(keep[14]) + 3.25f;
        lvd::set_shape_f32(shape + 0x38, f);
        const uint32_t wf = std::bit_cast<uint32_t>(f);
        same("set_shape_f32", shape + 0x38, 1, &wf);
        const uint32_t ww = keep[15] ^ 0x00410000u;
        lvd::set_shape_word(shape + 0x3c, ww);
        same("set_shape_word", shape + 0x3c, 1, &ww);
        const Vec4 v{1.5f, -2.5f, 3.75f, 0.0f};
        lvd::set_shape_row(shape + 0x70, v);
        const uint32_t wv[4] = {std::bit_cast<uint32_t>(v.x), std::bit_cast<uint32_t>(v.y), std::bit_cast<uint32_t>(v.z), std::bit_cast<uint32_t>(v.w)};
        same("set_shape_row", shape + 0x70, 4, wv);
        lvd::copy_shape_row(shape + 0x30, shape + 0x70);
        same("copy_shape_row", shape + 0x30, 4, wv);
        const Vec4 v2{-4.5f, 6.25f, 0.125f, 1.0f};
        V(shape + 0x70) = v2;  // (a callee's store through an out-pointer)
        lvd::shape_row_written(shape + 0x70);
        const uint32_t wv2[4] = {std::bit_cast<uint32_t>(v2.x), std::bit_cast<uint32_t>(v2.y), std::bit_cast<uint32_t>(v2.z), std::bit_cast<uint32_t>(v2.w)};
        same("shape_row_written", shape + 0x70, 4, wv2);
        const float d[3] = {10.0f, -20.0f, 0.5f};
        lvd::move_shape_centre(shape, d);
        const uint32_t wm[3] = {std::bit_cast<uint32_t>(v.x + d[0]), std::bit_cast<uint32_t>(v.y + d[1]), std::bit_cast<uint32_t>(v.z + d[2])};
        same("move_shape_centre", shape + 0x30, 3, wm);
        for (uint32_t k = 0; k < 32; ++k) lvd::set_shape_word(shape + 4 * k, keep[k]);  // (put back)
    }
    if (!r.volumes.empty()) {
        const uint32_t a = lvd::volume(0) + 0x84, keep = lwu(a);
        const float f = std::bit_cast<float>(keep) + 1.0f;
        lvd::set_shape_f32(a, f);
        const uint32_t w = std::bit_cast<uint32_t>(f);
        same("set_shape_f32 (a volume's tail)", a, 1, &w);
        lvd::set_shape_word(a, keep);
    }
    if (!r.rails.empty()) {
        const uint32_t rail = lvd::rail(0), keep = lwu(rail + 0xc);
        lvd::set_rail_reach(rail, -1.0f);
        const uint32_t w1 = std::bit_cast<uint32_t>(-1.0f);
        same("set_rail_reach", rail + 0xc, 1, &w1);
        lvd::set_rail_reach_word(rail, 0);
        const uint32_t w0 = 0;
        same("set_rail_reach_word", rail + 0xc, 1, &w0);
        lvd::set_rail_reach_word(rail, keep);
    }
    std::vector<lvd::RegistryDiff> diffs;
    const int n = lvd::registry_compare_base(diffs, 4);
    if (bad != 0 || n != 0) {
        print_diffs(diffs, 4);
        fail(name, std::to_string(bad) + " writes differ, " + std::to_string(n) + " differing words after putting back");
        return;
    }
    std::printf("%-26s ok: %d writes (EE and native alike), put back\n", name, done);
}

// (LD3) a write through each of the paths' write calls (on the first path with two points or more, and the first rail's spine):
// the EE words and the native record alike; the count lowered and raised; two rows a "callee" wrote through an out-pointer
// taken in; a store at an address no record holds is the EE store alone, counted; then every word put back
void check_path_facade() {
    const char* name = "registry_path_facade";
    const lvd::LevelRegistry& r = *lvd::registry_base();
    int bad = 0, done = 0;
    const auto same = [&](const char* what, uint32_t a, uint32_t words, const uint32_t* want) {
        ++done;
        for (uint32_t k = 0; k < words; ++k) {
            uint32_t nat = 0;
            const uint32_t ee = lwu(a + 4 * k);
            if (!native_word(a + 4 * k, nat) || nat != ee || ee != want[k]) {
                if (++bad <= 8) std::printf("    %s at %08x +%u: EE %08x, native %08x, written %08x\n", what, a, 4 * k, ee, nat, want[k]);
                return;
            }
        }
    };
    const auto bits = [](const Vec4& v) { return std::array<uint32_t, 4>{std::bit_cast<uint32_t>(v.x), std::bit_cast<uint32_t>(v.y), std::bit_cast<uint32_t>(v.z), std::bit_cast<uint32_t>(v.w)}; };
    int32_t pick = -1;
    for (size_t i = 0; i < r.paths.size() && pick < 0; ++i)
        if (r.paths[i].points.size() >= 2 && r.paths[i].count >= 2) pick = static_cast<int32_t>(i);
    uint32_t paths = 0;
    if (pick >= 0) {
        const uint32_t path = lvd::path(pick);
        const uint32_t size = 0x30;  // (the count word, the 12 bytes after it, points 0 and 1)
        uint32_t keep[size / 4];
        for (uint32_t k = 0; k < size / 4; ++k) keep[k] = lwu(path + 4 * k);
        const float f = std::bit_cast<float>(keep[7]) + 2.5f;
        lvd::set_path_f32(path + 0x1c, f);
        const uint32_t wf = std::bit_cast<uint32_t>(f);
        same("set_path_f32 (point 0's w)", path + 0x1c, 1, &wf);
        const uint32_t ww = keep[11] ^ 0x00010001u;
        lvd::set_path_word(path + 0x2c, ww);
        same("set_path_word (point 1's w)", path + 0x2c, 1, &ww);
        const uint32_t n = keep[0];
        lvd::set_path_count(path, n - 1);
        const uint32_t lowered = n - 1, raised = n + 1;
        same("set_path_count (lowered)", path, 1, &lowered);
        lvd::set_path_count(path, n + 1);
        same("set_path_count (raised)", path, 1, &raised);
        lvd::set_path_count(path, n);
        const Vec4 v{12.5f, -7.25f, 3.0f, -1.0f};
        lvd::set_path_row(path + 0x20, v);
        same("set_path_row (point 1)", path + 0x20, 4, bits(v).data());
        lvd::copy_path_row(path + 0x10, path + 0x20);
        same("copy_path_row (point 0)", path + 0x10, 4, bits(v).data());
        const Vec4 v0{-3.5f, 8.0f, 0.25f, 4.0f}, v1{6.0f, -0.5f, -9.75f, 0.0f};
        V(path + 0x10) = v0;  // (a callee's stores through an out-pointer, two rows)
        V(path + 0x20) = v1;
        lvd::path_rows_written(path + 0x10, 2);
        same("path_rows_written (point 0)", path + 0x10, 4, bits(v0).data());
        same("path_rows_written (point 1)", path + 0x20, 4, bits(v1).data());
        for (uint32_t k = 0; k < size / 4; ++k) lvd::set_path_word(path + 4 * k, keep[k]);  // (put back)
        ++paths;
    }
    if (!r.rails.empty() && !r.rails[0].spine.points.empty()) {
        const uint32_t spine = lwu(lvd::rail(0) + 0x10), keep = lwu(spine + 0x1c);
        const float f = std::bit_cast<float>(keep) + 1.0f;
        lvd::set_path_f32(spine + 0x1c, f);
        const uint32_t w = std::bit_cast<uint32_t>(f);
        same("set_path_f32 (a rail spine's point 0's w)", spine + 0x1c, 1, &w);
        lvd::set_path_word(spine + 0x1c, keep);
        ++paths;
    }
    {  // (an address no record holds: the EE store alone, counted)
        const uint32_t sp = ee_stack_alloc(0x10);
        const uint64_t misses = lvd::index_stats().write_misses;
        lvd::set_path_word(sp + 0xc, 0x12345678u);
        if (lwu(sp + 0xc) != 0x12345678u || lvd::index_stats().write_misses != misses + 1) {
            ++bad;
            std::printf("    set_path_word at %08x (no record): EE %08x, misses %llu -> %llu\n", sp + 0xc, lwu(sp + 0xc), static_cast<unsigned long long>(misses),
                        static_cast<unsigned long long>(lvd::index_stats().write_misses));
        }
        ++done;
        ee_stack_free(0x10);
    }
    std::vector<lvd::RegistryDiff> diffs;
    const int n = lvd::registry_compare_base(diffs, 4);
    if (bad != 0 || n != 0) {
        print_diffs(diffs, 4);
        fail(name, std::to_string(bad) + " writes differ, " + std::to_string(n) + " differing words after putting back");
        return;
    }
    std::printf("%-26s ok: %d writes on %u paths (EE and native alike), put back\n", name, done, paths);
}

// an added path of the base: indexed, written beside, dropped by ld_base_reset
void check_added_path() {
    const char* name = "registry_added_path";
    const int level = lw(0x0015ed84);
    const uint32_t table = OV(0x001b04b0);
    uint32_t keep[LD_PATH_MAX];
    for (uint32_t k = 0; k < LD_PATH_MAX; ++k) keep[k] = lwu(table + 4 * k);
    std::vector<uint32_t> mods(0x100 / 4);
    for (uint32_t k = 0; k < mods.size(); ++k) mods[k] = lwu(0x000df000u + 4 * k);
    uint32_t mem = 0;
    const int32_t slot = ld_path_reserve(level, 3, &mem);
    std::string why;
    if (slot < 0) {
        why = "no room";
    } else {
        const lvd::RecordRef a = lvd::record_at(mem);
        if (a.kind != lvd::RecordKind::ADDED_PATH || a.key != lvd::KEY_BASE || a.index != slot || a.offset != 0 || a.size != 0x40) why = "not indexed after ld_path_reserve";
        const float pts[3][3] = {{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, {-7.0f, 8.5f, 9.25f}};
        ld_path_set(mem, pts, 3);
        uint32_t nat = 0;
        if (why.empty() && (!native_word(mem, nat) || nat != 3u || !native_word(mem + 0x38, nat) || nat != std::bit_cast<uint32_t>(9.25f))) why = "its native record not written by ld_path_set";
        std::vector<lvd::RegistryDiff> d;
        if (why.empty() && lvd::registry_compare_base(d, 4) != 0) {
            print_diffs(d, 4);
            why = "its record differs from EE";
        }
        ld_path_release(level, slot);  // (the base's outside a session: kept, as leveldata.cpp keeps it)
        if (why.empty() && !lvd::record_at(mem)) why = "dropped by ld_path_release outside a session";
        ld_base_reset();
        if (why.empty() && lvd::record_at(mem)) why = "still indexed after ld_base_reset";
    }
    for (uint32_t k = 0; k < LD_PATH_MAX; ++k) w32(table + 4 * k, keep[k]);
    for (uint32_t k = 0; k < mods.size(); ++k) w32(0x000df000u + 4 * k, mods[k]);
    std::vector<lvd::RegistryDiff> d;
    if (why.empty() && lvd::registry_compare_base(d, 4) != 0) why = "the tables differ after it";
    if (!why.empty()) {
        fail(name, why);
        return;
    }
    std::printf("%-26s ok: slot %d at %08x indexed, written beside its EE words, dropped by ld_base_reset\n", name, slot, mem);
}

// EE words of each kind changed by hand, then the pull
void check_pull() {
    const char* name = "registry_pull";
    const lvd::LevelRegistry& r = *lvd::registry_base();
    std::vector<uint32_t> at;
    if (!r.paths.empty()) at.push_back(lvd::path(0) + 0x4);
    if (!r.volumes.empty()) at.push_back(lvd::volume(0) + 0x34);
    for (int l = 0; l < 3; ++l)
        if (!r.shapes[static_cast<size_t>(l)].empty()) at.push_back(lvd::box(static_cast<lvd::BoxList>(l), 0) + 0x44);
    if (!r.cameras.empty()) at.push_back(lvd::camera(0) + 0x14);
    if (!r.rails.empty()) {
        at.push_back(lvd::rail(0) + 0x4);
        at.push_back(lwu(lvd::rail(0) + 0x10) + 0x8);
    }
    std::vector<uint32_t> keep;
    for (const uint32_t a : at) keep.push_back(lwu(a));
    for (const uint32_t a : at) w32(a, lwu(a) ^ 0x00410000u);
    std::vector<lvd::RegistryDiff> d;
    const int before = lvd::registry_compare_base(d, 4);
    lvd::registry_pull_from_ee();
    int bad = 0;
    for (const uint32_t a : at) {
        uint32_t nat = 0;
        if (!native_word(a, nat) || nat != lwu(a)) ++bad;
    }
    const int after = lvd::registry_compare_base(d, 4);
    for (size_t k = 0; k < at.size(); ++k) w32(at[k], keep[k]);
    lvd::registry_pull_from_ee();
    const int back = lvd::registry_compare_base(d, 4);
    if (before != static_cast<int>(at.size()) || bad != 0 || after != 0 || back != 0) {
        print_diffs(d, 4);
        fail(name, std::to_string(at.size()) + " words changed: " + std::to_string(before) + " differences before the pull, " + std::to_string(bad) +
                       " native words not pulled, " + std::to_string(after) + " after it, " + std::to_string(back) + " after putting back");
        return;
    }
    std::printf("%-26s ok: %zu words (one of each kind held) changed in EE, pulled, put back and pulled again\n", name, at.size());
}

}  // namespace

void batch_541() {
    if (g_only_batch != 541) return;
    g_batch = 541;
    std::printf("batch 541: the native level registry on level %d\n", g_oracle_level);
    level_setup();
    if (lvd::registry_base() == nullptr) {
        fail("registry_base", "no registry after the level's setup");
        return;
    }
    check_tables("registry_tables");
    check_disc();
    check_pvars();
    check_faults();
    check_addresses();
    check_index();
    check_facade();
    check_path_facade();
    check_added_path();
    check_pull();
    // after a gameplay frame (the mobys' first updates: their writers write the native records too, nothing differs)
    w32(OV(0x0016c060), 0xf);
    w32(0x0013f2e8, 0xffffffffu);
    if (g_oracle_level == 14) w8(0x0013d3f0, 1);
    gameplay_frame();
    check_tables("registry_tables_frame");
}

}  // namespace rac1::oracle
