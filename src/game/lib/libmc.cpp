// libmc's host stand-ins: a virtual memory card (see libmc.h).
#include <game/lib/libmc.h>

#include <runtime/kernel/ee_memory.h>

#include <platform/platform.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>

namespace rac1 {

namespace {
namespace fs = std::filesystem;

constexpr int32_t NO_CARD = -10, CHANGED = -1, UNFORMATTED = -2, NO_ROOM = -3, NOT_FOUND = -4, DENIED = -5, NOT_EMPTY = -6, TOO_MANY = -7;
constexpr int32_t BUSY = -200;
constexpr int32_t CLUSTERS = 8000;  // an 8 MB card's free clusters (1 KB) when empty
constexpr int32_t MAX_FDS = 16;

struct Entry {
    bool dir = false;
    std::vector<uint8_t> data;
    uint64_t order = 0;     // creation order (a directory's listing order)
    std::time_t created = 0, modified = 0;
};

struct OpenFile {
    bool used = false;
    std::string path;
    uint32_t pos = 0;
    int32_t mode = 0;
};

struct Card {
    bool present = false;
    bool seen = false;       // GetInfo answered once since it was inserted
    bool formatted = true;
    std::string host;        // the backing directory ("" in memory only)
    std::map<std::string, Entry> entries;  // "DIR/FILE" (no leading '/')
    std::string cwd;         // the current directory ("" the root)
    uint64_t next_order = 1;
    OpenFile fds[MAX_FDS];
    std::vector<std::string> listing;  // GetDir's matches, and the next one to give
    size_t listed = 0;
};

struct Pending {
    bool on = false;
    int32_t cmd = 0, result = 0;
};

Card g_card;
Pending g_mc;
Card g_snap_card;           // testing: a copy of the card, put back before the port's run
Pending g_snap_mc;
uint32_t g_trace = 0;       // testing: an EE word holding a hash of the calls made (0: none)

// orig: (host) testing
void trace(uint32_t a, uint32_t b = 0, uint32_t c = 0, uint32_t d = 0, uint32_t e = 0) {
    if (g_trace == 0) return;
    uint32_t h = ee_read<uint32_t>(g_trace);
    for (uint32_t v : {a, b, c, d, e}) h = (h ^ v) * 0x01000193u;
    ee_write<uint32_t>(g_trace, h);
}

// orig: (host)
std::string ee_string(uint32_t a) {
    std::string s;
    if (a == 0) return s;
    for (uint32_t i = 0; i < 0x400; ++i) {
        const char c = static_cast<char>(ee_read<uint8_t>(a + i));
        if (c == 0) break;
        s.push_back(c);
    }
    return s;
}

// orig: (host) a card path made absolute (from the current directory) and normal: "A/B"; false
// when it climbs above the root
bool resolve(const std::string& name, std::string& out) {
    std::vector<std::string> parts;
    auto push = [&parts](const std::string& s) -> bool {
        size_t i = 0;
        while (i <= s.size()) {
            const size_t j = s.find('/', i);
            const std::string p = s.substr(i, j == std::string::npos ? std::string::npos : j - i);
            if (p == "..") {
                if (parts.empty()) return false;
                parts.pop_back();
            } else if (!p.empty() && p != ".") {
                parts.push_back(p);
            }
            if (j == std::string::npos) break;
            i = j + 1;
        }
        return true;
    };
    if (name.empty() || name[0] != '/') {
        if (!push(g_card.cwd)) return false;
    }
    if (!push(name)) return false;
    out.clear();
    for (size_t i = 0; i < parts.size(); ++i) out += (i != 0 ? "/" : "") + parts[i];
    return true;
}

// orig: (host)
std::string parent_of(const std::string& p) {
    const size_t k = p.rfind('/');
    return k == std::string::npos ? std::string() : p.substr(0, k);
}

// orig: (host)
bool dir_exists(const std::string& p) {
    if (p.empty()) return true;
    const auto it = g_card.entries.find(p);
    return it != g_card.entries.end() && it->second.dir;
}

// orig: (host) files by the KB, a directory's entries two to a KB
int32_t used_clusters() {
    int32_t n = 0;
    std::map<std::string, int32_t> in_dir;
    for (const auto& [k, e] : g_card.entries) {
        if (!e.dir) n += static_cast<int32_t>((e.data.size() + 1023) / 1024);
        ++in_dir[parent_of(k)];
    }
    for (const auto& [k, e] : g_card.entries)
        if (e.dir) n += (in_dir[k] + 2 + 1) / 2;
    return n;
}

// orig: (host)
fs::path host_path(const std::string& p) { return fs::path(g_card.host) / p; }

// orig: (host) the file as the card has it
void host_write(const std::string& p) {
    if (g_card.host.empty()) return;
    const Entry& e = g_card.entries[p];
    // (through a temporary file renamed over the old one: a crash or a full disk during a save never leaves half a file)
    if (!platform::write_file_replace(host_path(p), e.data.data(), e.data.size()))
        std::fprintf(stderr, "[memcard] cannot write %s\n", host_path(p).string().c_str());
}

// orig: (host)
void host_mkdir(const std::string& p) {
    if (g_card.host.empty()) return;
    std::error_code ec;
    fs::create_directories(host_path(p), ec);
}

// orig: (host)
void host_remove(const std::string& p) {
    if (g_card.host.empty()) return;
    std::error_code ec;
    fs::remove(host_path(p), ec);
}

// orig: (host) the card's tree read from the directory
void host_load(const fs::path& root) {
    std::error_code ec;
    std::vector<std::pair<std::string, Entry>> found;
    for (auto it = fs::recursive_directory_iterator(root, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        const std::string rel = fs::relative(it->path(), root, ec).generic_string();
        if (ec || rel.empty()) continue;
        Entry e;
        e.dir = it->is_directory(ec);
        if (!e.dir && it->path().extension() == ".tmp") continue;  // (a write that never finished: host_write's temporary file)
        if (!e.dir) {
            std::ifstream f(it->path(), std::ios::binary);
            e.data.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
#if defined(__WIIU__)
            if (e.data.empty()) continue;
#endif
        }
        const auto t = fs::last_write_time(it->path(), ec);
        const auto sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(t - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        e.created = e.modified = std::chrono::system_clock::to_time_t(sys);
        found.emplace_back(rel, std::move(e));
    }
    std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (auto& [k, e] : found) {
        e.order = g_card.next_order++;
        g_card.entries[k] = std::move(e);
    }
}

// orig: (host) a sceMcStDateTime (the card keeps Japan's time: the host's own)
void date(uint32_t a, std::time_t t) {
    std::tm tm{};
    platform::local_time(t, &tm);
    ee_write<uint8_t>(a, 0);
    ee_write<uint8_t>(a + 1, static_cast<uint8_t>(tm.tm_sec));
    ee_write<uint8_t>(a + 2, static_cast<uint8_t>(tm.tm_min));
    ee_write<uint8_t>(a + 3, static_cast<uint8_t>(tm.tm_hour));
    ee_write<uint8_t>(a + 4, static_cast<uint8_t>(tm.tm_mday));
    ee_write<uint8_t>(a + 5, static_cast<uint8_t>(tm.tm_mon + 1));
    ee_write<uint16_t>(a + 6, static_cast<uint16_t>(tm.tm_year + 1900));
}

// orig: (host) '*' / '?' patterns
bool wild(const char* p, const char* s) {
    if (*p == 0) return *s == 0;
    if (*p == '*') return wild(p + 1, s) || (*s != 0 && wild(p, s + 1));
    if (*s == 0) return false;
    return (*p == '?' || *p == *s) && wild(p + 1, s + 1);
}

// orig: (host) a command accepted: its number and result kept for sceMcSync; -200 when one is pending
int32_t post(int32_t cmd, int32_t result) {
    if (g_mc.on) return BUSY;
    trace(0x100 + static_cast<uint32_t>(cmd), static_cast<uint32_t>(result));
    g_mc.on = true;
    g_mc.cmd = cmd;
    g_mc.result = result;
    return 0;
}

// orig: (host) the card's state for a call on (port, slot): 0 usable, else the error to answer
int32_t card_state(int32_t port, int32_t slot) {
    if (port != 0 || slot != 0 || !g_card.present) return NO_CARD;
    if (!g_card.formatted) return UNFORMATTED;
    return 0;
}

// orig: (host)
OpenFile* file_of(int32_t fd) {
    if (fd < 0 || fd >= MAX_FDS || !g_card.fds[fd].used) return nullptr;
    return &g_card.fds[fd];
}

// orig: (host)
int32_t do_open(int32_t port, int32_t slot, const std::string& name, int32_t mode) {
    if (const int32_t s = card_state(port, slot); s != 0) return s;
    std::string p;
    if (!resolve(name, p) || p.empty()) return NOT_FOUND;
    if (!dir_exists(parent_of(p))) return NOT_FOUND;
    auto it = g_card.entries.find(p);
    if ((mode & 0x40) != 0) {  // a directory made
        if (it != g_card.entries.end()) return NOT_FOUND;
        if (used_clusters() + 1 > CLUSTERS) return NO_ROOM;
        Entry e;
        e.dir = true;
        e.order = g_card.next_order++;
        e.created = e.modified = std::time(nullptr);
        g_card.entries[p] = e;
        host_mkdir(p);
        return 0;
    }
    if (it == g_card.entries.end()) {
        if ((mode & 0x200) == 0) return NOT_FOUND;
        Entry e;
        e.order = g_card.next_order++;
        e.created = e.modified = std::time(nullptr);
        it = g_card.entries.emplace(p, e).first;
#if !defined(__WIIU__)
        host_write(p);
#endif
    } else if (it->second.dir) {
        return DENIED;
    }
    for (int32_t fd = 0; fd < MAX_FDS; ++fd) {
        if (g_card.fds[fd].used) continue;
        g_card.fds[fd] = OpenFile{true, p, 0, mode};
        return fd;
    }
    return TOO_MANY;
}
}  // namespace

// orig: (host) stands in for 0x001233f0 (sceMcInit: the SIF client bound to mcserv)
int32_t mc_init() { return 0; }

// orig: (host) stands in for 0x00123d68 (sceMcGetInfo, command 1): type / free / format are
// written as the reply's end function 0x123d10 does (when their pointers are not 0)
int32_t mc_get_info(int32_t port, int32_t slot, uint32_t type, uint32_t free, uint32_t format) {
    if (g_mc.on) return BUSY;
    trace(1, static_cast<uint32_t>(port), static_cast<uint32_t>(slot), type, free ^ (format << 7));
    int32_t r = card_state(port, slot);
    const bool card = r != NO_CARD;
    if (card && r == 0 && !g_card.seen) r = CHANGED;
    if (card) g_card.seen = true;
    if (type != 0) ee_write<int32_t>(type, card ? 2 : 0);
    if (card && free != 0) ee_write<int32_t>(free, g_card.formatted ? CLUSTERS - used_clusters() : 0);
    if (card && format != 0) ee_write<int32_t>(format, g_card.formatted ? 1 : 0);
    return post(1, r);
}

// orig: (host) stands in for 0x001235b0 (sceMcOpen, command 2)
int32_t mc_open(int32_t port, int32_t slot, uint32_t name, int32_t mode) {
    if (g_mc.on) return BUSY;
    if (name == 0 || ee_read<uint8_t>(name) == 0) return -210;  // (the call refused: no name)
    trace(2, static_cast<uint32_t>(port), static_cast<uint32_t>(slot), static_cast<uint32_t>(std::hash<std::string>()(ee_string(name))), static_cast<uint32_t>(mode));
    return post(2, do_open(port, slot, ee_string(name), mode));
}

// orig: (host) stands in for 0x001236d8 (sceMcMkdir: sceMcOpen with mode 0x40, command 0xb)
int32_t mc_mkdir(int32_t port, int32_t slot, uint32_t name) {
    if (g_mc.on) return BUSY;
    if (name == 0 || ee_read<uint8_t>(name) == 0) return -210;
    trace(0xb, static_cast<uint32_t>(port), static_cast<uint32_t>(slot), static_cast<uint32_t>(std::hash<std::string>()(ee_string(name))));
    return post(0xb, do_open(port, slot, ee_string(name), 0x40));
}

// orig: (host) stands in for 0x00123710 (sceMcClose, command 3)
int32_t mc_close(int32_t fd) {
    if (g_mc.on) return BUSY;
    trace(3, static_cast<uint32_t>(fd));
    OpenFile* f = file_of(fd);
    if (f == nullptr) return post(3, g_card.present ? NOT_FOUND : NO_CARD);
    f->used = false;
    return post(3, 0);
}

// orig: (host) stands in for 0x001237c8 (sceMcSeek, command 4)
int32_t mc_seek(int32_t fd, int32_t offset, int32_t origin) {
    if (g_mc.on) return BUSY;
    trace(4, static_cast<uint32_t>(fd), static_cast<uint32_t>(offset), static_cast<uint32_t>(origin));
    OpenFile* f = file_of(fd);
    if (f == nullptr) return post(4, g_card.present ? NOT_FOUND : NO_CARD);
    const int64_t size = static_cast<int64_t>(g_card.entries[f->path].data.size());
    int64_t base = origin == 1 ? f->pos : origin == 2 ? size : 0;
    const int64_t pos = std::max<int64_t>(0, base + offset);
    f->pos = static_cast<uint32_t>(pos);
    return post(4, static_cast<int32_t>(pos));
}

// orig: (host) stands in for 0x00123930 (sceMcRead, command 5): the data reaches EE memory before
// the command is seen finished, as the IOP's DMA does
int32_t mc_read(int32_t fd, uint32_t buf, int32_t size) {
    if (g_mc.on) return BUSY;
    trace(5, static_cast<uint32_t>(fd), buf, static_cast<uint32_t>(size));
    OpenFile* f = file_of(fd);
    if (f == nullptr) return post(5, g_card.present ? NOT_FOUND : NO_CARD);
    if ((f->mode & 1) == 0) return post(5, DENIED);
    const std::vector<uint8_t>& d = g_card.entries[f->path].data;
    const uint32_t n = size <= 0 || f->pos >= d.size() ? 0u : std::min<uint32_t>(static_cast<uint32_t>(size), static_cast<uint32_t>(d.size()) - f->pos);
    for (uint32_t i = 0; i < n; ++i) ee_write<uint8_t>(buf + i, d[f->pos + i]);
    f->pos += n;
    return post(5, static_cast<int32_t>(n));
}

// orig: (host) stands in for 0x00123a48 (sceMcWrite, command 6)
int32_t mc_write(int32_t fd, uint32_t buf, int32_t size) {
    if (g_mc.on) return BUSY;
    trace(6, static_cast<uint32_t>(fd), buf, static_cast<uint32_t>(size));
    OpenFile* f = file_of(fd);
    if (f == nullptr) return post(6, g_card.present ? NOT_FOUND : NO_CARD);
    if ((f->mode & 2) == 0) return post(6, DENIED);
    Entry& e = g_card.entries[f->path];
    const uint32_t n = size <= 0 ? 0u : static_cast<uint32_t>(size);
    const size_t end = static_cast<size_t>(f->pos) + n;
    if (end > e.data.size()) {
        const int32_t grow = static_cast<int32_t>((end + 1023) / 1024 - (e.data.size() + 1023) / 1024);
        if (used_clusters() + grow > CLUSTERS) return post(6, NO_ROOM);
        e.data.resize(end);
    }
    for (uint32_t i = 0; i < n; ++i) e.data[f->pos + i] = ee_read<uint8_t>(buf + i);
    if (g_trace != 0) {
        uint32_t h = 0x811c9dc5u;
        for (uint32_t i = 0; i < n; ++i) h = (h ^ e.data[f->pos + i]) * 0x01000193u;
        trace(0x60, h);
    }
    f->pos += n;
    e.modified = std::time(nullptr);
    host_write(f->path);
    return post(6, static_cast<int32_t>(n));
}

// orig: (host) stands in for 0x00123ef0 (sceMcGetDir, command 0xd)
int32_t mc_get_dir(int32_t port, int32_t slot, uint32_t name, int32_t mode, int32_t max, uint32_t table) {
    if (g_mc.on) return BUSY;
    if (name == 0 || ee_read<uint8_t>(name) == 0) return -210;
    trace(0xd, static_cast<uint32_t>(port), static_cast<uint32_t>(std::hash<std::string>()(ee_string(name))), static_cast<uint32_t>(mode), static_cast<uint32_t>(max) ^ (table << 8));
    if (const int32_t s = card_state(port, slot); s != 0) return post(0xd, s);
    if (mode == 0) {
        g_card.listing.clear();
        g_card.listed = 0;
        const std::string pat = ee_string(name);
        const size_t k = pat.rfind('/');
        std::string dir;
        if (!resolve(k == std::string::npos ? std::string() : pat.substr(0, k + 1), dir) || !dir_exists(dir)) return post(0xd, NOT_FOUND);
        const std::string leaf = k == std::string::npos ? pat : pat.substr(k + 1);
        std::vector<std::pair<uint64_t, std::string>> m;
        for (const auto& [p, e] : g_card.entries)
            if (parent_of(p) == dir && wild(leaf.c_str(), p.substr(dir.empty() ? 0 : dir.size() + 1).c_str())) m.emplace_back(e.order, p);
        std::sort(m.begin(), m.end());
        for (const auto& [o, p] : m) g_card.listing.push_back(p);
    }
    int32_t n = 0;
    while (n < max && g_card.listed < g_card.listing.size()) {
        const std::string p = g_card.listing[g_card.listed++];
        const auto it = g_card.entries.find(p);
        if (it == g_card.entries.end()) continue;
        const uint32_t t = table + static_cast<uint32_t>(n) * 0x40;
        for (uint32_t i = 0; i < 0x40; i += 4) ee_write<uint32_t>(t + i, 0);
        date(t, it->second.created);
        date(t + 8, it->second.modified);
        ee_write<uint32_t>(t + 0x10, static_cast<uint32_t>(it->second.data.size()));
        ee_write<uint16_t>(t + 0x14, it->second.dir ? 0x8427 : 0x8497);
        const std::string leaf = p.substr(p.rfind('/') == std::string::npos ? 0 : p.rfind('/') + 1);
        for (size_t i = 0; i < leaf.size() && i < 31; ++i) ee_write<uint8_t>(t + 0x20 + static_cast<uint32_t>(i), static_cast<uint8_t>(leaf[i]));
        ++n;
    }
    return post(0xd, n);
}

// orig: (host) stands in for 0x001240c0 (sceMcChdir, command 0xc): the current directory kept
// (its old value to `cur` when not 0)
int32_t mc_chdir(int32_t port, int32_t slot, uint32_t dir, uint32_t cur) {
    if (g_mc.on) return BUSY;
    trace(0xc, static_cast<uint32_t>(port), static_cast<uint32_t>(slot), static_cast<uint32_t>(std::hash<std::string>()(ee_string(dir))), cur);
    if (const int32_t s = card_state(port, slot); s != 0) return post(0xc, s);
    if (cur != 0) {
        const std::string c = "/" + g_card.cwd;
        for (size_t i = 0; i <= c.size() && i < 0x3ff; ++i) ee_write<uint8_t>(cur + static_cast<uint32_t>(i), i < c.size() ? static_cast<uint8_t>(c[i]) : 0);
    }
    std::string p;
    if (dir == 0 || !resolve(ee_string(dir), p) || !dir_exists(p)) return post(0xc, NOT_FOUND);
    g_card.cwd = p;
    return post(0xc, 0);
}

// orig: (host) stands in for 0x00124200 (sceMcFormat, command 0x10): the card emptied
int32_t mc_format(int32_t port, int32_t slot) {
    if (g_mc.on) return BUSY;
    trace(0x10, static_cast<uint32_t>(port), static_cast<uint32_t>(slot));
    if (port != 0 || slot != 0 || !g_card.present) return post(0x10, NO_CARD);
    for (auto it = g_card.entries.rbegin(); it != g_card.entries.rend(); ++it) host_remove(it->first);
    g_card.entries.clear();
    g_card.cwd.clear();
    g_card.formatted = true;
    for (OpenFile& f : g_card.fds) f.used = false;
    return post(0x10, 0);
}

// orig: (host) stands in for 0x001242d0 (sceMcDelete, command 0xf)
int32_t mc_delete(int32_t port, int32_t slot, uint32_t name) {
    if (g_mc.on) return BUSY;
    if (name == 0 || ee_read<uint8_t>(name) == 0) return -210;
    trace(0xf, static_cast<uint32_t>(port), static_cast<uint32_t>(slot), static_cast<uint32_t>(std::hash<std::string>()(ee_string(name))));
    if (const int32_t s = card_state(port, slot); s != 0) return post(0xf, s);
    std::string p;
    if (!resolve(ee_string(name), p) || p.empty()) return post(0xf, NOT_FOUND);
    const auto it = g_card.entries.find(p);
    if (it == g_card.entries.end()) return post(0xf, NOT_FOUND);
    if (it->second.dir)
        for (const auto& [k, e] : g_card.entries)
            if (parent_of(k) == p) return post(0xf, NOT_EMPTY);
    for (const OpenFile& f : g_card.fds)
        if (f.used && f.path == p) return post(0xf, DENIED);
    g_card.entries.erase(it);
    host_remove(p);
    return post(0xf, 0);
}

// orig: (host) stands in for 0x001243e8 (sceMcUnformat, command 0x11): the card unformatted (in
// memory: the host's directory keeps its files)
int32_t mc_unformat(int32_t port, int32_t slot) {
    if (g_mc.on) return BUSY;
    trace(0x11, static_cast<uint32_t>(port), static_cast<uint32_t>(slot));
    if (port != 0 || slot != 0 || !g_card.present) return post(0x11, NO_CARD);
    g_card.formatted = false;
    return post(0x11, 0);
}

// orig: (host) stands in for 0x00123c30 (sceMcSync): the command done at once
int32_t mc_sync(int32_t mode, uint32_t cmd, uint32_t result) {
    (void)mode;
    if (!g_mc.on) return -1;
    if (cmd != 0) ee_write<int32_t>(cmd, g_mc.cmd);
    if (result != 0) ee_write<int32_t>(result, g_mc.result);
    g_mc.on = false;
    return 1;
}

// orig: (host)
void mc_card_insert(const std::string& dir) {
    g_card = Card{};
    g_card.present = true;
    g_card.host = dir;
    if (!dir.empty()) {
        std::error_code ec;
        fs::create_directories(dir, ec);
        host_load(dir);
    }
}

// orig: (host)
void mc_card_remove() { g_card = Card{}; }

// orig: (host)
bool mc_card_present() { return g_card.present; }

// orig: (host)
std::string mc_default_card_dir() {
    if (const char* d = std::getenv("RAC1_MEMCARD_DIR"); d != nullptr && *d != 0) return d;
    // ($XDG_DATA_HOME/rac1-native, else ~/.local/share/rac1-native; %APPDATA%\rac1-native on Windows)
    if (const std::filesystem::path d = platform::data_dir(); !d.empty()) return (d / "memcard").string();
    return "memcard";
}

// orig: (host) testing
void mc_reset() {
    g_mc = Pending{};
    g_card = Card{};
}

// orig: (host) testing
void mc_snapshot_take() {
    g_snap_card = g_card;
    g_snap_mc = g_mc;
}

// orig: (host) testing
void mc_snapshot_restore() {
    g_card = g_snap_card;
    g_mc = g_snap_mc;
}

// orig: (host) testing
void mc_set_trace(uint32_t ee_word) { g_trace = ee_word; }

// orig: (host) testing: a card in memory with a file (its directories made)
void mc_card_put(const std::string& path, const std::vector<uint8_t>& data) {
    std::string p;
    if (!resolve("/" + path, p) || p.empty()) return;
    for (size_t k = p.find('/'); k != std::string::npos; k = p.find('/', k + 1)) {
        Entry& d = g_card.entries[p.substr(0, k)];
        if (d.order == 0) d.order = g_card.next_order++;
        d.dir = true;
    }
    Entry& e = g_card.entries[p];
    if (e.order == 0) e.order = g_card.next_order++;
    e.dir = false;
    e.data = data;
}

// orig: (host) testing: a card file's data (empty when none)
std::vector<uint8_t> mc_card_get(const std::string& path) {
    const auto it = g_card.entries.find(path);
    return it == g_card.entries.end() ? std::vector<uint8_t>() : it->second.data;
}

// orig: (host) a file of the host's own on the card (not through the game's commands)
bool mc_card_file_write(const std::string& path, const std::vector<uint8_t>& data) {
    if (!g_card.present || !g_card.formatted) return false;
    std::string p;
    if (!resolve("/" + path, p) || p.empty() || !dir_exists(parent_of(p))) return false;
    Entry& e = g_card.entries[p];
    if (e.dir) return false;
    if (e.order == 0) {
        e.order = g_card.next_order++;
        e.created = std::time(nullptr);
    }
    e.modified = std::time(nullptr);
    e.data = data;
    host_write(p);
    return true;
}

// orig: (host)
bool mc_card_file_read(const std::string& path, std::vector<uint8_t>& out) {
    if (!g_card.present) return false;
    std::string p;
    if (!resolve("/" + path, p)) return false;
    const auto it = g_card.entries.find(p);
    if (it == g_card.entries.end() || it->second.dir) return false;
    out = it->second.data;
    return true;
}

// orig: (host) testing
void mc_set_pending(bool on, int32_t cmd, int32_t result) {
    g_mc.on = on;
    g_mc.cmd = cmd;
    g_mc.result = result;
}

}  // namespace rac1
