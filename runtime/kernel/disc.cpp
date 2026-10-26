#include "disc.h"

#include "ee_memory.h"

#include <platform/platform.h>
#include "io/host_endian.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>

namespace rac1 {

namespace {
std::FILE* g_iso = nullptr;

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
    std::FILE* f = std::fopen(disc_path().c_str(), "rb");
    if (f == nullptr) return "";
    auto rd = [f](uint32_t off, uint32_t n) {
        std::vector<uint8_t> b(n);
        if (std::fseek(f, static_cast<long>(off), SEEK_SET) != 0 || std::fread(b.data(), 1, n, f) != n) b.clear();
        return b;
    };
    std::string out;
    const std::vector<uint8_t> pvd = rd(16 * 2048, 2048);
    if (pvd.size() == 2048 && std::memcmp(&pvd[1], "CD001", 5) == 0) {
        uint32_t lba = 0, size = 0;
        lba = le32(&pvd[156 + 2]);  // (ISO 9660: both byte orders; the little-endian half)
        size = le32(&pvd[156 + 10]);
        const std::vector<uint8_t> dir = size < 0x10000 ? rd(lba * 2048, size) : std::vector<uint8_t>{};
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
                const std::vector<uint8_t> cnf = fsize < 2048 ? rd(flba * 2048, fsize) : std::vector<uint8_t>{};
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
    std::fclose(f);
    return out;
}
}  // namespace

const std::string& disc_boot_elf() {
    static const std::string name = [] {
        const std::string b = read_boot2();
        if (b.empty()) return std::string("SCUS_971.99");
        if (b != "SCUS_971.99") std::fprintf(stderr, "[disc] %s boots %s: another region's build\n", disc_path().c_str(), b.c_str());
        return b;
    }();
    return name;
}

std::string disc_boot_elf_path() { return "assets/dumps/" + disc_boot_elf(); }

bool disc_other_region() { return disc_boot_elf() != "SCUS_971.99"; }

bool disc_open(const std::string& path) {
    if (g_iso != nullptr) return true;
    std::string p = path;
    if (p.empty()) p = disc_path();
    g_iso = std::fopen(p.c_str(), "rb");
    return g_iso != nullptr;
}

bool disc_is_open() { return g_iso != nullptr; }

// a positional read (pread; ReadFile at an offset on Windows), no shared position: the game's thread and the host's
// audio thread (the IOP mirror's stream reader, ADR 0013) read at the same time
bool disc_read(uint32_t lba, uint32_t sectors, void* dst) {
    if (g_iso == nullptr && !disc_open()) return false;
    const size_t n = static_cast<size_t>(sectors) * 2048;
    size_t got = 0;
    while (got < n) {
        const int64_t r = platform::read_at(g_iso, static_cast<uint64_t>(lba) * 2048 + got, static_cast<char*>(dst) + got, n - got);
        if (r <= 0) return false;
        got += static_cast<size_t>(r);
    }
    return true;
}

void disc_will_need(uint32_t lba, uint32_t sectors) {
    if (g_iso == nullptr && !disc_open()) return;
    platform::will_need(g_iso, static_cast<uint64_t>(lba) * 2048, static_cast<uint64_t>(sectors) * 2048);
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
