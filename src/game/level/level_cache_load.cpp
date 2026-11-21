// See level_cache_load.h; the format in docs/formats/README.md ("The level cache", "The tables chunk").
#include <game/level/level_cache_load.h>

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/region_core.h>
#include <io/asset_paths.h>
#include <runtime/kernel/disc.h>
#include <runtime/kernel/mods.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>

namespace rac1 {

namespace {

namespace lc = level_cache;
namespace ot = overlay_tables;

std::string hex8(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "%08x", v);
    return b;
}

// ---- The sources' fingerprint (docs/formats/README.md, "The tables chunk") ----

// One piece of a file of the disc folder's layout.txt: its offset in the file, its size, its SHA-1
struct LayoutPiece {
    uint64_t offset = 0, size = 0;
    std::array<uint8_t, 20> sha{};
};
struct Layout {
    bool ok = false;
    std::string error;
    std::map<std::string, std::vector<LayoutPiece>> files;
};

int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// orig: (host) the disc folder's layout.txt, read once (the setup writes it before the game, or the cache step, runs)
const Layout& layout() {
    static Layout l;
    static std::once_flag once;
    std::call_once(once, [] {
        const std::string path = std::string(disc_files_dir()) + "/layout.txt";
        std::ifstream f(path);
        std::string line;
        if (!f || !std::getline(f, line) || line != "rac1-native disc layout 1") {
            l.error = path + " is missing: the cache is made from the disc's files only";
            return;
        }
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#' || line.rfind("image ", 0) == 0) continue;
            std::istringstream in(line);
            std::string first, sha, name;
            LayoutPiece p;
            if (!(in >> first >> p.size >> p.offset >> sha) || !std::getline(in >> std::ws, name) || sha.size() != 40) {
                l.error = path + " is damaged";
                return;
            }
            for (size_t i = 0; i < 20; ++i) {
                const int hi = hex_digit(sha[2 * i]), lo = hex_digit(sha[2 * i + 1]);
                if (hi < 0 || lo < 0) {
                    l.error = path + " is damaged";
                    return;
                }
                p.sha[i] = static_cast<uint8_t>(hi << 4 | lo);
            }
            l.files[name].push_back(p);
        }
        l.ok = true;
    });
    return l;
}

// orig: (host) one source of the fingerprint: the file `rel` of the disc folder as the game reads it: its layout lines (per
// piece: u64 offset in the file, u64 size, its SHA-1), or, when a mod on gives that file (mods.h), "mod:" + rel with u64
// size, u64 the mod file's write time
bool add_source(lc::SourceFingerprint& fp, const std::string& rel, std::string& why) {
    const Layout& l = layout();
    if (!l.ok) {
        why = l.error;
        return false;
    }
    const auto it = l.files.find(rel);
    if (it == l.files.end()) {
        why = std::string(disc_files_dir()) + "/layout.txt has no " + rel;
        return false;
    }
    uint64_t size = 0;
    for (const LayoutPiece& p : it->second) size = std::max(size, p.offset + p.size);
    if (mods_on()) {
        const std::filesystem::path m = mods_disc_file(rel, size);
        if (!m.empty()) {
            std::error_code ec;
            const auto t = std::filesystem::last_write_time(m, ec);
            uint8_t b[16];
            lc::wr_le64(b, size);
            lc::wr_le64(b + 8, ec ? 0 : static_cast<uint64_t>(t.time_since_epoch().count()));
            fp.begin("mod:" + rel, sizeof b);
            fp.add(b, sizeof b);
            return true;
        }
    }
    std::vector<uint8_t> data;
    for (const LayoutPiece& p : it->second) {
        uint8_t b[16];
        lc::wr_le64(b, p.offset);
        lc::wr_le64(b + 8, p.size);
        data.insert(data.end(), b, b + 16);
        data.insert(data.end(), p.sha.begin(), p.sha.end());
    }
    fp.begin(rel, data.size());
    fp.add(data.data(), data.size());
    return true;
}

// orig: (host) the disc's serial ("SCUS-97199") from its boot executable's name ("SCUS_971.99")
std::string disc_serial() {
    std::string s;
    for (char c : disc_boot_elf()) {
        if (c == '_') s += '-';
        else if (c != '.') s += c;
    }
    return s;
}

lc::Region disc_region() {
    const std::string& b = disc_boot_elf();
    if (b.rfind("SCUS", 0) == 0 || b.rfind("SLUS", 0) == 0) return lc::Region::NtscU;
    if (b.rfind("SCES", 0) == 0 || b.rfind("SLES", 0) == 0) return lc::Region::Pal;
    if (b.rfind("SCPS", 0) == 0 || b.rfind("SLPS", 0) == 0 || b.rfind("SCAJ", 0) == 0) return lc::Region::NtscJ;
    return lc::Region::Unknown;
}

std::string data_file(int level) {
    char b[40];
    std::snprintf(b, sizeof b, "levels/level%02d/data.wad", level);
    return b;
}

// ---- The region's fix (a build whose boot executable moved: PAL 2.00) ----
// The overlay load takes each word of the overlay's data that points into the boot executable back to the NTSC-U address
// (region_core.cpp's region_core_fix, LevelLoader's section callback): the cache holds the bytes as the overlay has them
// after that load, so the same fix is made here, from the same files (region_core.txt's runs, region_levelN.txt's `ptr`
// lines). On the NTSC-U disc and PAL 1.00 nothing moved: no fix. (A copy of region_core.cpp's two helpers, which are not
// public; RAC1_CACHE_VERIFY=1 compares the result with the loaded overlay.)
struct CoreFix {
    using Run = std::array<uint32_t, 3>;  // lo, hi, displacement
    std::vector<Run> code, data;
    // orig: (host) region_core.cpp's CoreRuns::back: the disc's core address `p` as NTSC-U's (0: none)
    uint32_t back(uint32_t p) const {
        for (const Run& r : code)
            if (p - r[2] >= r[0] && p - r[2] < r[1]) return p - r[2];
        uint32_t found = 0;
        for (const Run& r : data)
            if (p - r[2] >= r[0] && p - r[2] <= r[1]) found = p - r[2];
        return found;
    }
};

// orig: (host) a region file's `ptr` lines (sorted), and its runs into `runs` when given
std::vector<uint32_t> read_ptrs(const std::string& path, CoreFix* runs) {
    std::vector<uint32_t> out;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream s(line);
        std::string kind, d;
        uint32_t lo = 0, hi = 0;
        s >> kind >> std::hex >> lo >> hi >> d;
        if (kind == "ptr") out.push_back(lo);
        else if (runs != nullptr && (kind == "code" || kind == "data")) {
            const uint32_t disp = d.empty() ? 0 : static_cast<uint32_t>(std::stoll(d, nullptr, 16));
            (kind == "code" ? runs->code : runs->data).push_back({lo, hi, disp});
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

// orig: (host) the words of `bytes` (level `level`'s overlay data at `addr`) that point into the disc's boot executable,
// taken back to NTSC-U's addresses
void fix_core_pointers(int level, uint32_t addr, std::vector<uint8_t>& bytes) {
    if (!region_core_moved()) return;
    static CoreFix runs;
    static std::map<int, std::vector<uint32_t>> ptrs;
    static std::mutex mu;
    std::lock_guard<std::mutex> lock(mu);
    static const bool read = (read_ptrs(io::asset_path("generated/overlay_map/region_core.txt"), &runs), true);
    (void)read;
    auto it = ptrs.find(level);
    if (it == ptrs.end())
        it = ptrs.emplace(level, read_ptrs(io::asset_path("generated/overlay_map/region_level" + std::to_string(level) + ".txt"), nullptr)).first;
    const uint32_t end = addr + static_cast<uint32_t>(bytes.size());
    for (auto p = std::lower_bound(it->second.begin(), it->second.end(), addr); p != it->second.end() && *p + 4 <= end; ++p) {
        const uint32_t o = *p - addr;
        const uint32_t b = runs.back(lc::rd_le32(&bytes[o]));
        if (b != 0) lc::wr_le32(&bytes[o], b);
    }
}

// ---- The switches (docs/env-vars.md) ----

bool cache_on() {
    static const bool on = [] {
        const char* e = std::getenv("RAC1_LEVEL_CACHE");
        return e == nullptr || std::strcmp(e, "0") != 0;
    }();
    return on;
}
bool verify_on() {
    static const bool on = [] {
        const char* e = std::getenv("RAC1_CACHE_VERIFY");
        return e != nullptr && *e != '\0' && std::strcmp(e, "0") != 0;
    }();
    return on;
}

double ms_since(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

// orig: (host) the cache file of `level` made from the disc (the current translation is the level's) and written
bool make_file(int level, const lc::Stamp& stamp, std::vector<ot::Table>& tables, size_t& bytes, std::string& why) {
    std::vector<std::string> notes;
    if (!level_cache_extract(level, tables, notes, why)) return false;
    for (const std::string& n : notes) std::printf("[level-cache] planet%02d: %s\n", level, n.c_str());
    lc::Writer w(stamp);
    const std::vector<uint8_t> chunk = ot::encode(level, tables);
    if (lc::Result r = w.add(ot::kChunkId, ot::kChunkVersion, chunk); !r) {
        why = r.why;
        return false;
    }
    const std::filesystem::path path = lc::file_path(level_cache_dir(), level);
    if (lc::Result r = w.write(path); !r) {
        why = r.why;
        return false;
    }
    std::error_code ec;
    bytes = static_cast<size_t>(std::filesystem::file_size(path, ec));
    return true;
}

}  // namespace

// orig: (host) see level_cache_load.h
const std::string& level_cache_dir() {
    static const std::string dir = io::asset_path("generated/level_cache");
    return dir;
}

// orig: (host) see level_cache_load.h
bool level_cache_stamp(int level, lc::Stamp& out, std::string& why) {
    lc::SourceFingerprint fp;
    // the sources, in this order: the boot executable (it tells the builds apart: PAL 1.00 from 2.00), the level's data
    // file (its overlay)
    if (!add_source(fp, "files/" + disc_boot_elf(), why) || !add_source(fp, data_file(level), why)) return false;
    out.serial = disc_serial();
    out.region = disc_region();
    if (!fp.finish(out.fingerprint)) {
        why = "the fingerprint's sources did not add up";
        return false;
    }
    return true;
}

// orig: (host) see level_cache_load.h
bool level_cache_extract(int level, std::vector<ot::Table>& out, std::vector<std::string>& notes, std::string& why) {
    out.clear();
    std::vector<std::byte> raw;
    if (!disc_read_level_overlay(level, raw)) {
        why = "level " + std::to_string(level) + "'s overlay cannot be read from the disc";
        return false;
    }
    const std::span<const uint8_t> head(reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
    std::vector<ot::Section> sections;
    if (!ot::parse_overlay(head, sections, why)) return false;
    ot::extract(
        sections, [](uint32_t veldin) { return ovl_xlate_data(veldin); },
        [level](uint32_t addr, std::vector<uint8_t>& bytes) { fix_core_pointers(level, addr, bytes); }, out, notes);
    return true;
}

// orig: (host) see level_cache_load.h
bool level_cache_file_bytes(int level, const lc::Stamp& stamp, const std::vector<ot::Table>& tables, std::vector<uint8_t>& out,
                            std::string& why) {
    lc::Writer w(stamp);
    if (lc::Result r = w.add(ot::kChunkId, ot::kChunkVersion, ot::encode(level, tables)); !r) {
        why = r.why;
        return false;
    }
    if (lc::Result r = w.build(out); !r) {
        why = r.why;
        return false;
    }
    return true;
}

// orig: (host) see level_cache_load.h
bool level_cache_read(const std::string& path, std::span<const uint8_t> image, int level, const lc::Stamp& stamp,
                      std::vector<ot::Table>& out, std::string& why) {
    lc::Reader r;
    lc::Result res = image.empty() ? r.open(path) : r.open_memory(image, path);
    const lc::Chunk* c = nullptr;
    if (res) res = r.check_stamp(stamp);
    if (res) res = r.require(ot::kChunkId, ot::kChunkVersion, c);
    std::vector<uint8_t> chunk;
    if (res) res = r.read(*c, chunk);
    if (!res) {
        why = res.why;
        return false;
    }
    int lv = -1;
    if (!ot::decode(chunk, lv, out, why)) return false;
    if (lv != level) {
        why = path + ": made for level " + std::to_string(lv);
        return false;
    }
    // the translation now (the overlay maps): each table where the file says (a map made again moves them)
    for (const ot::Table& t : out)
        if (ovl_xlate_data(t.veldin) != t.level_addr) {
            why = path + ": " + lc::fourcc_name(t.id) + " is at " + hex8(t.level_addr) + ", the overlay maps say " + hex8(ovl_xlate_data(t.veldin));
            return false;
        }
    return true;
}

// orig: (host) see level_cache_load.h
int level_cache_compare(const std::vector<ot::Table>& tables, std::string& first) {
    int bad = 0;
    first.clear();
    for (const ot::Table& t : tables) {
        const uint32_t n = static_cast<uint32_t>(t.bytes.size());
        for (uint32_t o = 0; o < n; o += 4) {
            if (ot::is_live(t, o)) continue;
            const uint32_t k = std::min<uint32_t>(4, n - o);
            bool same = true;
            for (uint32_t b = 0; b < k; ++b) same = same && lbu(t.level_addr + o + b) == t.bytes[o + b];
            if (same) continue;
            if (first.empty()) {
                uint32_t mem = 0, cache = 0;
                for (uint32_t b = 0; b < k; ++b) {
                    mem |= static_cast<uint32_t>(lbu(t.level_addr + o + b)) << (8 * b);
                    cache |= static_cast<uint32_t>(t.bytes[o + b]) << (8 * b);
                }
                first = lc::fourcc_name(t.id) + " (Veldin " + hex8(t.veldin) + ") at " + hex8(t.level_addr + o) + " (+0x" + hex8(o).substr(4) +
                        "): the cache " + hex8(cache) + ", memory " + hex8(mem);
            }
            ++bad;
            break;
        }
    }
    return bad;
}

// orig: (none) — enhancement: see level_cache_load.h
void level_cache_level_started() {
    if (!cache_on()) return;
    const int level = ovl_reloc_level();  // (the overlay loaded: RAC1_OWN_OVERLAY=0 plays every level on Veldin's)
    if (level < 0 || level > 18) return;
    if (disc_level_redirected(level) >= 0) {  // (a level mod's build in the level's place: its own files, not the disc's)
        static std::set<int> said;
        if (said.insert(level).second) std::printf("[level-cache] planet%02d: a level mod's build is played: no cache\n", level);
        return;
    }
    const auto t0 = std::chrono::steady_clock::now();
    std::string why;
    lc::Stamp stamp;
    if (!level_cache_stamp(level, stamp, why)) {
        static bool said = false;
        if (!said) std::printf("[level-cache] no cache: %s\n", why.c_str());
        said = true;
        return;
    }
    std::vector<ot::Table> tables;
    const std::string path = lc::file_path(level_cache_dir(), level).string();
    if (!level_cache_read(path, {}, level, stamp, tables, why)) {
        size_t bytes = 0;
        std::string why2;
        if (!make_file(level, stamp, tables, bytes, why2)) {
            std::printf("[level-cache] planet%02d: cannot be made: %s\n", level, why2.c_str());
            return;
        }
        std::printf("[level-cache] planet%02d made (%s): %zu tables, %zu bytes, %.2f ms\n", level, why.c_str(), tables.size(), bytes, ms_since(t0));
    }
    if (verify_on()) {
        std::string first;
        size_t n = 0;
        for (const ot::Table& t : tables) n += t.bytes.size();
        const int bad = level_cache_compare(tables, first);
        if (bad == 0)
            std::printf("[level-cache] verify planet%02d: %zu tables (%zu bytes) equal the game's memory\n", level, tables.size(), n);
        else
            std::printf("[level-cache] verify planet%02d: %d of %zu tables differ; first: %s\n", level, bad, tables.size(), first.c_str());
    }
}

// orig: (host) see level_cache_load.h
bool level_cache_setup_step(bool force) {
    bool ok = true;
    int made = 0, kept = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int level = 0; level <= 18; ++level) {
        if (!disc_has_level(level)) continue;
        ovl_reloc_set_level(level);  // (this level's maps: the setup's process alone)
        std::string why;
        lc::Stamp stamp;
        if (!level_cache_stamp(level, stamp, why)) {
            std::printf("\n[setup] the level cache: %s\n", why.c_str());
            ok = false;
            break;
        }
        std::vector<ot::Table> tables;
        if (!force && level_cache_read(lc::file_path(level_cache_dir(), level).string(), {}, level, stamp, tables, why)) {
            ++kept;
            continue;
        }
        size_t bytes = 0;
        if (!make_file(level, stamp, tables, bytes, why)) {
            std::printf("\n[setup] the level cache: planet%02d: %s\n", level, why.c_str());
            ok = false;
            continue;
        }
        std::printf(" %d", level);
        std::fflush(stdout);
        ++made;
    }
    ovl_reloc_set_level(OVL_NONE);
    std::printf(" (%d made, %d already there, %.0f ms)", made, kept, ms_since(t0));
    return ok;
}

}  // namespace rac1
