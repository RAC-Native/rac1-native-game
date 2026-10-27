#include "io/host_endian.h"
#include "disc.h"

#include "ee_memory.h"

#include <platform/platform.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace rac1 {

namespace {
constexpr uint64_t kSector = 2048;
std::FILE* g_iso = nullptr;
std::string g_iso_path;
std::mutex g_iso_lock;  // (its opening)
std::atomic<int> g_source{-1};  // -1: not decided yet, 0: the files, 1: the image

// ---- The disc's files (assets/disc/, tools/disc_extract.py) ----
// layout.txt: "<first byte (hex)> <size> <offset in the file> <sha1> <path>" per piece, sorted by the first byte, not
// overlapping; the bytes no piece holds are zero. One place maps the disc's bytes to the host's files (files_find):
// a mods folder in front of assets/disc/ would change only which path a piece's file opens (files_load).
constexpr const char* kLayoutMagic = "rac1-native disc layout 1";
constexpr size_t kOpenFiles = 64;  // the files kept open at once (the oldest closed past that)

struct DiscPiece {
    uint64_t first = 0, size = 0, file_offset = 0;
    uint32_t file = 0;
};

struct DiscFiles {
    bool ok = false;
    std::string error;
    uint64_t image_bytes = 0;
    std::vector<std::filesystem::path> paths;  // per file: where it is on the host
    std::vector<uint64_t> sizes;               // per file: its size in the layout
    std::vector<DiscPiece> pieces;             // by their first byte
    std::mutex lock;                           // (the handles: the game's thread and the audio thread read at once)
    std::vector<std::shared_ptr<std::FILE>> handles;
    std::deque<uint32_t> opened;               // the files open, oldest first
};

// a file of the layout on the host: assets/disc/<path>; on Windows a long one (past the old 260 characters) through
// the \\?\ prefix, which takes it whatever the system's long path setting
std::filesystem::path files_host_path(const std::string& rel) {
    std::filesystem::path p = std::filesystem::path(disc_files_dir()) / std::filesystem::path(rel);
#if defined(_WIN32)
    std::error_code ec;
    std::filesystem::path a = std::filesystem::absolute(p, ec);
    if (!ec && a.native().size() >= 240) {
        a.make_preferred();
        return std::filesystem::path(L"\\\\?\\" + a.native());
    }
#endif
    return p;
}

void files_load(DiscFiles& d) {
    const std::string layout = std::string(disc_files_dir()) + "/layout.txt";
    std::ifstream f(layout);
    std::string line;
    if (!f || !std::getline(f, line) || line != kLayoutMagic) {
        d.error = "the disc's files are missing (" + layout + "): the game reads the files the setup extracts from your disc image";
        return;
    }
    std::string word;
    if (!std::getline(f, line) || !(std::istringstream(line) >> word >> d.image_bytes) || word != "image") {
        d.error = layout + " is damaged";
        return;
    }
    std::vector<std::string> names;
    std::unordered_map<std::string, uint32_t> index;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream in(line);
        DiscPiece p;
        std::string sha, path;
        if (!(in >> std::hex >> p.first >> std::dec >> p.size >> p.file_offset >> sha) || !std::getline(in >> std::ws, path) || path.empty()) {
            d.error = layout + " is damaged";
            return;
        }
        const auto [it, added] = index.try_emplace(path, static_cast<uint32_t>(names.size()));  // (a multi-extent file: pieces)
        p.file = it->second;
        if (added) {
            names.push_back(path);
            d.sizes.push_back(0);
        }
        d.sizes[p.file] = std::max(d.sizes[p.file], p.file_offset + p.size);
        if ((!d.pieces.empty() && p.first < d.pieces.back().first + d.pieces.back().size) || p.first + p.size > d.image_bytes) {
            d.error = layout + " is damaged";
            return;
        }
        d.pieces.push_back(p);
    }
    for (const std::string& n : names) d.paths.push_back(files_host_path(n));
    d.handles.resize(d.paths.size());
    d.ok = true;
}

DiscFiles& files() {
    static DiscFiles d;
    static std::once_flag once;
    std::call_once(once, [] { files_load(d); });
    return d;
}

// file `i` of the layout, opened (kept open, up to kOpenFiles); null when it is missing or not the layout's size
std::shared_ptr<std::FILE> files_handle(DiscFiles& d, uint32_t i) {
    std::scoped_lock lock(d.lock);
    if (d.handles[i]) return d.handles[i];
    std::FILE* raw = platform::open_file(d.paths[i], "rb");
    std::error_code ec;
    if (raw == nullptr || std::filesystem::file_size(d.paths[i], ec) != d.sizes[i]) {
        if (raw != nullptr) std::fclose(raw);
        static std::vector<bool> said;
        said.resize(d.paths.size());
        if (!said[i]) {
            said[i] = true;
            std::fprintf(stderr, "[disc] %s is %s: run the setup again (./setup.sh, setup.cmd on Windows)\n", d.paths[i].string().c_str(),
                         raw == nullptr ? "missing" : "damaged (not its size)");
        }
        return nullptr;
    }
    d.handles[i] = std::shared_ptr<std::FILE>(raw, [](std::FILE* h) { std::fclose(h); });
    d.opened.push_back(i);
    if (d.opened.size() > kOpenFiles) {
        d.handles[d.opened.front()].reset();  // (closed once no read still holds it)
        d.opened.pop_front();
    }
    return d.handles[i];
}

// the pieces holding any of the bytes [offset, offset + n): `fn(piece, first byte, last byte + 1)` for each, in order
template <class Fn>
bool files_find(const DiscFiles& d, uint64_t offset, uint64_t n, Fn&& fn) {
    auto it = std::upper_bound(d.pieces.begin(), d.pieces.end(), offset, [](uint64_t v, const DiscPiece& p) { return v < p.first; });
    if (it != d.pieces.begin()) --it;
    for (; it != d.pieces.end() && it->first < offset + n; ++it) {
        const uint64_t a = std::max(offset, it->first), b = std::min(offset + n, it->first + it->size);
        if (a < b && !fn(*it, a, b)) return false;
    }
    return true;
}

bool read_full(std::FILE* f, uint64_t at, char* dst, size_t n) {
    size_t got = 0;
    while (got < n) {
        const int64_t r = platform::read_at(f, at + got, dst + got, n - got);
        if (r <= 0) return false;
        got += static_cast<size_t>(r);
    }
    return true;
}

bool files_read(uint64_t offset, size_t n, char* dst) {
    DiscFiles& d = files();
    if (!d.ok || offset + n > d.image_bytes) return false;
    std::memset(dst, 0, n);  // (what no piece holds)
    return files_find(d, offset, n, [&](const DiscPiece& p, uint64_t a, uint64_t b) {
        const std::shared_ptr<std::FILE> f = files_handle(d, p.file);
        return f && read_full(f.get(), p.file_offset + (a - p.first), dst + (a - offset), static_cast<size_t>(b - a));
    });
}

// ---- The image ----
bool image_open(const std::string& path) {
    std::scoped_lock lock(g_iso_lock);
    if (g_iso != nullptr) return true;
    g_iso_path = path.empty() ? disc_path() : path;
    g_iso = platform::open_file(g_iso_path, "rb");  // (the Wii U reads it through several handles: platform_wiiu.cpp)
    return g_iso != nullptr;
}

struct DiscLevel {
    int state = 0;  // 0: unknown, 1: valid, -1: missing
    std::array<std::pair<uint32_t, uint32_t>, 4> ranges{};  // (lba, sectors): data, gameplay NTSC, gameplay PAL, occlusion
};
std::mutex g_toc_lock;
std::vector<uint8_t> g_toc;
std::array<DiscLevel, 19> g_levels{};

bool parse_level_header(uint32_t lsn, int want_level, DiscLevel& out) {
    if (lsn == 0) return false;
    uint8_t h[2048];
    if (!disc_read(lsn, 1, h)) return false;
    int32_t lid = 0, hsize = 0;
    lid = le32s(h);
    hsize = le32s(h + 4);
    if (lid != want_level || hsize != 0x2434) return false;
    for (int k = 0; k < 4; ++k) {
        uint32_t o = 0, n = 0;
        o = le32(h + 8 + 8 * k);
        n = le32(h + 12 + 8 * k);
        out.ranges[k] = {o, n};
    }
    out.state = 1;
    return true;
}

const DiscLevel* get_disc_level(int level) {
    if (level < 0 || level >= 19) return nullptr;
    std::scoped_lock lock(g_toc_lock);
    DiscLevel& dl = g_levels[static_cast<size_t>(level)];
    if (dl.state != 0) return dl.state > 0 ? &dl : nullptr;
    if (g_toc.empty()) {
        g_toc.resize(6 * 2048);
        int32_t magic = 0, toc_size = 0;
        if (!disc_read(1500, 6, g_toc.data())) {
            g_toc.clear();
            return nullptr;
        }
        magic = le32s(g_toc.data());
        toc_size = le32s(g_toc.data() + 4);
        if (magic != 1 || toc_size < 8 || static_cast<size_t>(toc_size) > g_toc.size()) {
            g_toc.clear();
            dl.state = -1;
            return nullptr;
        }
        g_toc.resize(static_cast<size_t>(toc_size));
    }
    // 0x13a448 - 0x137b80 = 0x28c8: the level table in the disc's table of contents (level_read.cpp)
    const size_t std_ofs = 0x28c8 + static_cast<size_t>(level) * 8;
    if (std_ofs + 4 <= g_toc.size()) {
        uint32_t lsn = 0;
        lsn = le32(g_toc.data() + std_ofs);
        if (parse_level_header(lsn, level, dl)) return &dl;
    }
    for (size_t ofs = 8; ofs + 8 <= g_toc.size(); ofs += 8) {
        uint32_t lsn = 0;
        lsn = le32(g_toc.data() + ofs);
        if (parse_level_header(lsn, level, dl)) return &dl;
    }
    dl.state = -1;
    return nullptr;
}
}  // namespace

std::string disc_path() {
    const char* env = std::getenv("RAC1_ISO");
    if (env != nullptr) return env;
    // the setup's link to the player's image, one name per region (the PAL disc's when it is the one
    // there); where links cannot be made (Windows without the privilege), the image's path written in a
    // text file beside it instead
    const std::string link = "assets/iso/rac1-ntsc-u.iso", pal_link = "assets/iso/rac1-pal.iso";
    std::error_code ec;
    if (std::filesystem::exists(link, ec)) return link;
    if (std::filesystem::exists(pal_link, ec)) return pal_link;
    std::ifstream f(disc_path_file());
    std::string p;
    if (f && std::getline(f, p)) {
        while (!p.empty() && (p.back() == '\r' || p.back() == '\n')) p.pop_back();
        if (!p.empty()) return p;
    }
    return link;
}

const char* disc_path_file() { return "assets/iso/disc-path.txt"; }

namespace {
// SYSTEM.CNF's BOOT2 file name, read through the ISO9660 root directory ("" when it cannot be read)
std::string read_boot2() {
    auto rd = [](uint32_t lba, uint32_t n) {
        std::vector<uint8_t> b(static_cast<size_t>((n + kSector - 1) / kSector) * kSector);
        if (!disc_read(lba, static_cast<uint32_t>(b.size() / kSector), b.data())) b.clear();
        b.resize(std::min<size_t>(b.size(), n));
        return b;
    };
    std::string out;
    const std::vector<uint8_t> pvd = rd(16, 2048);
    if (pvd.size() == 2048 && std::memcmp(&pvd[1], "CD001", 5) == 0) {
        uint32_t lba = 0, size = 0;
        lba = le32(&pvd[156 + 2]);  // (ISO 9660: both byte orders; the little-endian half)
        size = le32(&pvd[156 + 10]);
        const std::vector<uint8_t> dir = size < 0x10000 ? rd(lba, size) : std::vector<uint8_t>{};
        for (size_t o = 0; o < dir.size();) {
            const uint8_t len = dir[o];
            if (len == 0) {
                o = (o / 2048 + 1) * 2048;
                continue;
            }
            if (o + len > dir.size() || len < 34) break;
            const std::string name(reinterpret_cast<const char*>(&dir[o + 33]), dir[o + 32]);
            if (name.rfind("SYSTEM.CNF", 0) == 0) {
                uint32_t flba = 0, fsize = 0;
                flba = le32(&dir[o + 2]);
                fsize = le32(&dir[o + 10]);
                const std::vector<uint8_t> cnf = fsize < 2048 ? rd(flba, fsize) : std::vector<uint8_t>{};
                const std::string txt(cnf.begin(), cnf.end());
                const size_t b = txt.find("cdrom0:\\");
                if (b != std::string::npos) {
                    const size_t e = txt.find_first_of(";\r\n", b + 8);
                    out = txt.substr(b + 8, e == std::string::npos ? std::string::npos : e - (b + 8));
                }
                break;
            }
            o += len;
        }
    }
    return out;
}
}  // namespace

const std::string& disc_boot_elf() {
    static const std::string name = [] {
        const std::string b = read_boot2();
        if (b.empty()) return std::string("SCUS_971.99");
        if (b != "SCUS_971.99") std::fprintf(stderr, "[disc] the disc boots %s: another region's build\n", b.c_str());
        return b;
    }();
    return name;
}

std::string disc_boot_elf_path() { return "assets/dumps/" + disc_boot_elf(); }

bool disc_other_region() { return disc_boot_elf() != "SCUS_971.99"; }

const char* disc_files_dir() { return "assets/disc"; }

DiscSource disc_source() {
    int s = g_source.load();
    if (s < 0) {
        // debugging: RAC1_READ_ISO=1 reads the image itself (disc_path()), to compare with the extracted files
        const char* e = std::getenv("RAC1_READ_ISO");
        s = e != nullptr && std::atoi(e) != 0 ? 1 : 0;
        int unset = -1;
        if (!g_source.compare_exchange_strong(unset, s)) s = unset;
    }
    return s != 0 ? DiscSource::Image : DiscSource::Files;
}

bool disc_ready(std::string& why) {
    if (disc_source() == DiscSource::Image) {
        if (image_open("")) return true;
        why = "your disc image was not found (" + disc_path() + "; RAC1_READ_ISO=1 reads the image, not the disc's files).";
        return false;
    }
    const DiscFiles& d = files();
    if (!d.ok) why = d.error + ".";
    return d.ok;
}

bool disc_open(const std::string& path) {
    const bool ok = image_open(path);
    if (ok) g_source.store(1);
    return ok;
}

bool disc_is_open() {
    if (disc_source() == DiscSource::Image) return g_iso != nullptr;
    return files().ok;
}

uint64_t disc_size(DiscSource src) {
    if (src == DiscSource::Files) return files().ok ? files().image_bytes : 0;
    if (!image_open("")) return 0;
    std::error_code ec;
    const uint64_t n = std::filesystem::file_size(g_iso_path, ec);
    return ec ? 0 : n;
}

// a positional read (pread; ReadFile at an offset on Windows), no shared position: the game's thread and the host's
// audio thread (the IOP mirror's stream reader, ADR 0013) read at the same time
bool disc_read_from(DiscSource src, uint32_t lba, uint32_t sectors, void* dst) {
    const size_t n = static_cast<size_t>(sectors) * kSector;
    if (src == DiscSource::Files) return files_read(uint64_t{lba} * kSector, n, static_cast<char*>(dst));
    if (g_iso == nullptr && !image_open("")) return false;
    return read_full(g_iso, uint64_t{lba} * kSector, static_cast<char*>(dst), n);
}

bool disc_read(uint32_t lba, uint32_t sectors, void* dst) { return disc_read_from(disc_source(), lba, sectors, dst); }

void disc_will_need(uint32_t lba, uint32_t sectors) {
    const uint64_t offset = uint64_t{lba} * kSector, n = uint64_t{sectors} * kSector;
    if (disc_source() == DiscSource::Image) {
        if (g_iso == nullptr && !image_open("")) return;
        platform::will_need(g_iso, offset, n);
        return;
    }
    DiscFiles& d = files();
    if (!d.ok) return;
    files_find(d, offset, n, [&](const DiscPiece& p, uint64_t a, uint64_t b) {
        if (const std::shared_ptr<std::FILE> f = files_handle(d, p.file)) platform::will_need(f.get(), p.file_offset + (a - p.first), b - a);
        return true;
    });
}

bool disc_read_ee(uint32_t ee_dst, uint32_t lba, uint32_t sectors) {
    std::vector<uint8_t> buf(static_cast<size_t>(sectors) * 2048);
    if (!disc_read(lba, sectors, buf.data())) return false;
    for (size_t i = 0; i < buf.size(); ++i) ee_write<uint8_t>(ee_dst + static_cast<uint32_t>(i), buf[i]);
    return true;
}

bool disc_has_level(int level) { return get_disc_level(level) != nullptr; }

bool disc_read_level_wad(int level, std::vector<std::byte>& out) {
    const DiscLevel* lv = get_disc_level(level);
    if (lv == nullptr) return false;
    uint32_t low = UINT32_MAX, high = 0;
    for (const auto& r : lv->ranges) {
        if (r.second == 0) continue;
        low = std::min(low, r.first);
        high = std::max(high, r.first + r.second);
    }
    if (low == UINT32_MAX || high <= low || low == 0) return false;
    const uint32_t adjust = low - 1;  // one header sector in front
    const size_t total_sectors = high - adjust;
    out.assign(total_sectors * 2048, std::byte{0});
    if (!disc_read(low, high - low, out.data() + 2048)) {
        out.clear();
        return false;
    }
    const int32_t hdr_prefix[4] = {0x30, 0, level, 0};
    for (int k = 0; k < 4; ++k) le_store(out.data() + 4 * k, hdr_prefix[k]);
    for (int k = 0; k < 4; ++k) {
        const uint32_t rel_o = lv->ranges[k].second != 0 ? lv->ranges[k].first - adjust : 0;
        const uint32_t rel_n = lv->ranges[k].second;
        le_store(out.data() + 16 + 8 * k, rel_o);
        le_store(out.data() + 20 + 8 * k, rel_n);
    }
    return true;
}

bool disc_read_level_overlay(int level, std::vector<std::byte>& out) {
    const DiscLevel* lv = get_disc_level(level);
    if (lv == nullptr || lv->ranges[0].second == 0) return false;
    const uint32_t data_lba = lv->ranges[0].first, data_sectors = lv->ranges[0].second;
    std::byte head[2048];
    if (!disc_read(data_lba, 1, head)) return false;
    int32_t ovl_off = 0, ovl_size = 0;
    ovl_off = le32s(head);
    ovl_size = le32s(head + 4);
    if (ovl_off < 0 || ovl_size <= 0) return false;
    const uint64_t end = static_cast<uint64_t>(ovl_off) + static_cast<uint64_t>(ovl_size);
    const uint64_t need_sectors = (end + 2047) / 2048;
    if (need_sectors > data_sectors) return false;
    out.resize(static_cast<size_t>(need_sectors) * 2048);
    std::memcpy(out.data(), head, 2048);
    if (need_sectors > 1 && !disc_read(data_lba + 1, static_cast<uint32_t>(need_sectors - 1), out.data() + 2048)) {
        out.clear();
        return false;
    }
    return true;
}

}  // namespace rac1
