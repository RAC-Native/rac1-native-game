// Oracle batch 541 (#150 stage 5, task L2): the native level registry (src/game/level/level_registry.h) against the EE tables
// the level's load lays out - a host check at the boundary, no original code run. On the level after its setup
// (level_setup_body, whose gameplay_load builds the registry at its end):
//  - registry_base: every path (the table's offsets, the count word, the 12 bytes after it, every point), every 0x90-byte
//    volume, every shape of the three lists, every camera (with its pvar block's address), every rail (centre, reach, loop
//    word, its spine's points), the camera-volume grid's and the zone grid's bytes equal the EE tables word by word;
//  - the level's gameplay file read again from the disc and unpacked by the host (decompress_wad, not the game's
//    wad_decompress into EE memory) and parsed into a second registry: the same objects as the first;
//  - the cameras' pvar blocks: none exactly where the file's pvar index is negative;
//  - after one gameplay frame: equal again but for the run-time writes the verify follows (level_registry.cpp's list);
//  - the comparison itself: a fault put into one word of each object kind's EE table (a record's word changed by far more
//    than an ulp, a count, a table entry) must be found, named with that object and index; every fault is undone.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 541, N = 0 .. 18.

#include "ovl_oracle.h"

#include <formats/scene_wad.h>
#include <game/level/level_data.h>
#include <game/level/level_registry.h>
#include <runtime/kernel/disc.h>

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

void check_tables(const char* name, bool follow) {
    std::vector<lvd::RegistryDiff> d;
    const int n = lvd::registry_compare_base(d, 16, follow);
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
    check_tables("registry_tables", false);
    check_disc();
    check_pvars();
    check_faults();
    // after a gameplay frame (the mobys' first updates: the run-time writers the verify follows)
    w32(OV(0x0016c060), 0xf);
    w32(0x0013f2e8, 0xffffffffu);
    if (g_oracle_level == 14) w8(0x0013d3f0, 1);
    gameplay_frame();
    check_tables("registry_tables_frame", true);
}

}  // namespace rac1::oracle
