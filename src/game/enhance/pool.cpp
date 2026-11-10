// See pool.h.
#include <game/enhance/pool.h>

#include <formats/scene_wad.h>
#include <game/ovl/ovl_common.h>
#include <platform/platform.h>
#include <runtime/kernel/disc.h>
#include <runtime/kernel/ee_memory.h>

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>

namespace rac1 {

namespace fs = std::filesystem;

namespace {

std::vector<PoolClass> g_classes;
std::vector<PoolLevel> g_levels;
std::map<std::pair<int, uint32_t>, size_t> g_by_key;
const std::byte* g_blocks = nullptr;  // classes.bin, mapped
size_t g_blocks_size = 0;
bool g_tried = false;

uint64_t fnv(const uint8_t* p, size_t n) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

int32_t rd32(const uint8_t* p) {
    int32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

// One level read from the disc: its data file's parts, its core (the asset index), its asset data unpacked, its overlay
// class table
struct LevelFiles {
    std::vector<uint8_t> data;   // the level's data file (data.wad)
    std::vector<uint8_t> asset;  // the asset data, unpacked
    const uint8_t* core = nullptr;
    int32_t parts[22] = {};
    std::vector<std::array<uint32_t, 3>> class_table;  // the overlay's section 3: class, update function, word (to the -1 row)
    // the overlay's data sections 0..5 as loaded (0x15ef00 up to its code: a bss as zeros) and its code section
    std::vector<uint8_t> image;
    uint32_t code_lo = 0, code_hi = 0;
};

bool read_level(int level, LevelFiles& f, std::string* why) {
    std::vector<std::byte> wad;
    if (!disc_read_level_wad(level, wad) || wad.size() < 0x30) {
        *why = "level " + std::to_string(level) + "'s files could not be read from the disc";
        return false;
    }
    uint32_t off = 0, sectors = 0;
    std::memcpy(&off, wad.data() + 0x10, 4);
    std::memcpy(&sectors, wad.data() + 0x14, 4);
    const size_t at = static_cast<size_t>(off) * 2048, n = static_cast<size_t>(sectors) * 2048;
    if (at + n > wad.size() || n < sizeof f.parts) {
        *why = "level " + std::to_string(level) + "'s data file is not where its header says";
        return false;
    }
    const auto* d = reinterpret_cast<const uint8_t*>(wad.data()) + at;
    f.data.assign(d, d + n);
    std::memcpy(f.parts, f.data.data(), sizeof f.parts);
    auto part_ok = [&](int k) {
        return f.parts[2 * k] > 0 && f.parts[2 * k + 1] > 0 && static_cast<size_t>(f.parts[2 * k]) + static_cast<size_t>(f.parts[2 * k + 1]) <= f.data.size();
    };
    if (!part_ok(0) || !part_ok(2) || !part_ok(10)) {
        *why = "level " + std::to_string(level) + "'s data file has no overlay, core or asset data";
        return false;
    }
    f.core = f.data.data() + f.parts[4];
    if (!decompress_wad(std::span<const std::byte>(reinterpret_cast<const std::byte*>(f.data.data()) + f.parts[20], static_cast<size_t>(f.parts[21])), f.asset)) {
        *why = "level " + std::to_string(level) + "'s asset data could not be unpacked";
        return false;
    }
    // the overlay: [dest, size, type, entry] blocks; section 3 is the moby class table (12-byte rows to the -1 key)
    const uint8_t* o = f.data.data() + f.parts[0];
    const size_t on = static_cast<size_t>(f.parts[1]);
    size_t p = 0;
    int32_t entry = 0;
    for (int k = 0; p + 16 <= on; ++k) {
        const int32_t n2 = rd32(o + p + 4), e = rd32(o + p + 12);
        if (k == 0) entry = e;
        if (e != entry || n2 < 0 || p + 16 + static_cast<size_t>(n2) > on) break;
        const uint32_t dest = static_cast<uint32_t>(rd32(o + p));
        if (k <= 5 && dest >= CONTEXT_BASE && dest + static_cast<uint32_t>(n2) <= 0x00400000u) {  // (a data section)
            if (f.image.size() < dest + static_cast<uint32_t>(n2) - CONTEXT_BASE) f.image.resize(dest + static_cast<uint32_t>(n2) - CONTEXT_BASE, 0);
            if (rd32(o + p + 8) != 8) std::memcpy(f.image.data() + (dest - CONTEXT_BASE), o + p + 16, static_cast<size_t>(n2));
        }
        if (static_cast<uint32_t>(entry) >= dest && static_cast<uint32_t>(entry) < dest + static_cast<uint32_t>(n2)) {  // (the code)
            f.code_lo = dest;
            f.code_hi = dest + static_cast<uint32_t>(n2);
        }
        if (k == 3) {
            for (int32_t r = 0; r + 12 <= n2; r += 12) {
                const uint8_t* row = o + p + 16 + r;
                f.class_table.push_back({static_cast<uint32_t>(rd32(row)), static_cast<uint32_t>(rd32(row + 4)), static_cast<uint32_t>(rd32(row + 8))});
                if (rd32(row) == -1) break;
            }
        }
        p += 16 + static_cast<size_t>(n2);
    }
    if (f.class_table.empty() || f.class_table.back()[0] != 0xffffffffu) {
        *why = "level " + std::to_string(level) + "'s overlay has no class table";
        return false;
    }
    if (f.code_lo <= CONTEXT_BASE || f.image.size() > f.code_lo - CONTEXT_BASE) {
        *why = "level " + std::to_string(level) + "'s overlay has no code after its data";
        return false;
    }
    f.image.resize(f.code_lo - CONTEXT_BASE, 0);
    return true;
}

// The ends of the asset data's items: the offsets its index names (a class block runs to the next one)
std::vector<size_t> named_offsets(const LevelFiles& f) {
    const uint8_t* c = f.core;
    const size_t an = f.asset.size();
    int32_t H[48];
    std::memcpy(H, c, sizeof H);
    std::vector<size_t> named;
    auto name = [&](int32_t v) {
        if (v > 0 && static_cast<size_t>(v) < an) named.push_back(static_cast<size_t>(v));
    };
    for (int fld : {2, 3, 4, 5, 24, 25, 26, 29, 31, 40, 41, 42}) name(H[fld]);
    for (int k = 0; k < H[6]; ++k) name(rd32(c + H[7] + 32 * k));         // the moby classes
    for (int k = 0; k < H[8]; ++k) name(rd32(c + H[9] + 32 * k));         // the ties
    for (int k = 0; k < H[10]; ++k) name(rd32(c + H[11] + 0x30 * k));     // the shrubs
    for (int k = 0; k < 256; ++k) name(rd32(c + H[30] + 4 * k));          // the hero's sequences
    for (int k = 0; k < H[32]; ++k) name(rd32(c + H[33] + 16 * k));       // the gadgets
    named.push_back(an);
    std::sort(named.begin(), named.end());
    named.erase(std::unique(named.begin(), named.end()), named.end());
    return named;
}

std::string hex(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "%x", v);
    return b;
}

fs::path dir() { return fs::path(pool_dir()); }

std::string stamp_disc() { return disc_boot_elf(); }

// the stamp's version and disc, "" when there is none
bool stamp_ok() {
    std::ifstream in(dir() / "pool.json");
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string s = ss.str();
    return s.find("\"version\": " + std::to_string(POOL_VERSION) + ",") != std::string::npos &&
           s.find("\"disc\": \"" + stamp_disc() + "\"") != std::string::npos;
}

bool read_index() {
    std::ifstream in(dir() / "pool.idx");
    if (!in) return false;
    g_classes.clear();
    g_levels.clear();
    g_by_key.clear();
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream s(line);
        std::string kind;
        s >> kind;
        if (kind == "level") {
            PoolLevel l;
            s >> l.level >> std::hex >> l.classes >> l.with_data >> l.moby_textures >> l.asset_bytes >> l.class_bytes >> l.collision_bytes >>
                l.sky_bytes >> l.bank_bytes >> l.vram_bytes >> l.overlay_bytes;
            g_levels.push_back(l);
        } else if (kind == "class") {
            PoolClass c;
            std::string remap;
            std::string sn;
            s >> std::dec >> c.level >> c.id >> std::hex >> c.block_off >> c.block_size >> c.hash >> c.update_fn >> c.word2c >> remap >> sn;
            for (size_t p = 0; sn != "-" && p < sn.size();) {
                const size_t q = std::min(sn.find(',', p), sn.size());
                c.sounds.push_back(static_cast<uint16_t>(std::stoul(sn.substr(p, q - p), nullptr, 16)));
                p = q + 1;
            }
            for (size_t k = 0; k < 16 && 2 * k + 1 < remap.size(); ++k)
                c.remap[k] = static_cast<uint8_t>(std::stoul(remap.substr(2 * k, 2), nullptr, 16));
            g_by_key[{c.level, c.id}] = g_classes.size();
            g_classes.push_back(c);
        }
    }
    return !g_classes.empty();
}

}  // namespace

// orig: (host) see pool.h
const char* pool_dir() { return "assets/generated/pool"; }

// orig: (host) see pool.h
bool pool_build(std::string* why) {
    std::string dummy;
    if (why == nullptr) why = &dummy;
    std::error_code ec;
    fs::create_directories(dir(), ec);
    std::vector<uint8_t> blocks;
    std::unordered_map<uint64_t, std::vector<uint32_t>> by_hash;  // content hash -> offsets in blocks (compared byte by byte)
    std::string idx = "# the asset pool's index (rac1-native, docs/open-engine.md): made from your disc, never shared\n"
                      "# level <n> <classes> <with data> <moby textures> <asset bytes> <class bytes> <collision> <sky> <bank> <vram> <overlay> (hex)\n"
                      "# class <level> <id> <block offset> <block size> <hash> <update function> <word 2c> <texture remap> <sounds> (hex)\n";
    idx += "pool " + std::to_string(POOL_VERSION) + " " + stamp_disc() + "\n";
    size_t rows = 0, levels = 0;
    std::vector<std::pair<int, std::vector<uint8_t>>> contexts, banks;
    for (int level = 0; level < 19; ++level) {
        if (!disc_has_level(level)) continue;
        LevelFiles f;
        if (!read_level(level, f, why)) return false;
        ++levels;
        const auto named = named_offsets(f);
        int32_t H[48];
        std::memcpy(H, f.core, sizeof H);
        PoolLevel L;
        L.level = level;
        L.classes = static_cast<uint32_t>(H[6]);
        L.moby_textures = static_cast<uint32_t>(H[14]);
        L.asset_bytes = static_cast<uint32_t>(f.asset.size());
        L.collision_bytes = static_cast<uint32_t>(H[24] - H[5]);
        L.sky_bytes = static_cast<uint32_t>(H[5] - H[4]);
        L.bank_bytes = static_cast<uint32_t>(f.parts[3]);
        L.vram_bytes = static_cast<uint32_t>(f.parts[7]);
        L.overlay_bytes = static_cast<uint32_t>(f.parts[1]);
        std::string lines;
        for (int k = 0; k < H[6]; ++k) {
            const uint8_t* e = f.core + H[7] + 32 * k;
            PoolClass c;
            c.level = level;
            c.id = static_cast<uint32_t>(rd32(e + 4));
            std::memcpy(c.remap, e + 0x10, 16);
            // the update function and the word as class_update_fn_set finds them: the class's row, else the -1 row's
            const auto* row = &f.class_table.back();
            for (const auto& r : f.class_table)
                if (r[0] == c.id) {
                    row = &r;
                    break;
                }
            c.update_fn = (*row)[1];
            c.word2c = (*row)[2];
            const int32_t off = rd32(e);
            if (off > 0 && static_cast<size_t>(off) < f.asset.size()) {
                const size_t end = *std::upper_bound(named.begin(), named.end(), static_cast<size_t>(off));
                const uint32_t n = static_cast<uint32_t>(end - static_cast<size_t>(off));
                const uint8_t* b = f.asset.data() + off;
                c.block_size = n;
                c.hash = fnv(b, n);
                ++L.with_data;
                L.class_bytes += n;
                uint32_t at = 0xffffffffu;
                for (uint32_t o : by_hash[c.hash])
                    if (std::memcmp(blocks.data() + o, b, n) == 0 && (o + n == blocks.size() || o + n <= blocks.size())) {
                        at = o;
                        break;
                    }
                if (at == 0xffffffffu) {
                    at = static_cast<uint32_t>(blocks.size());
                    blocks.insert(blocks.end(), b, b + n);
                    blocks.resize((blocks.size() + 63) & ~static_cast<size_t>(63), 0);
                    by_hash[c.hash].push_back(at);
                }
                c.block_off = at;
            }
            // its sounds: the level's bank sound of each of its sound definitions (the asset index +0x70's list for the
            // class's place in the level's registration: level_setup.cpp gives the definitions +0x1a from it)
            {
                const uint8_t* s6 = f.core + rd32(f.core + 0x70);
                const uint8_t* e2 = s6 + 8 + 4 * k;
                int16_t off = 0, cnt = 0;
                std::memcpy(&off, e2, 2);
                std::memcpy(&cnt, e2 + 2, 2);
                // (the level setup reads as many as the class block has definitions, +0xd, past the list when it is
                // shorter: Orxon's 1229 has 12 for a list of 6; a class without data reads its list's own count)
                const int32_t bo = rd32(e);
                if (bo > 0 && static_cast<size_t>(bo) + 0xe <= f.asset.size()) cnt = f.asset[static_cast<size_t>(bo) + 0xd];
                for (int16_t j = 0; j < cnt && j < 256; ++j) {
                    uint16_t v = 0;
                    std::memcpy(&v, s6 + off + 4 * j, 2);
                    c.sounds.push_back(v);
                }
            }
            std::string sn;
            for (uint16_t v : c.sounds) sn += (sn.empty() ? "" : ",") + hex(v);
            std::string rm;
            for (uint8_t v : c.remap) {
                char t[3];
                std::snprintf(t, sizeof t, "%02x", v);
                rm += t;
            }
            char h[24];
            std::snprintf(h, sizeof h, "%016" PRIx64, c.hash);
            lines += "class " + std::to_string(level) + " " + std::to_string(c.id) + " " + hex(c.block_off) + " " + hex(c.block_size) + " " + h + " " +
                     hex(c.update_fn) + " " + hex(c.word2c) + " " + rm + " " + (sn.empty() ? "-" : sn) + "\n";
            ++rows;
        }
        {  // the level's sound bank, as the disc has it (a host bank for its classes in a session)
            const int32_t bo = f.parts[2], bn = f.parts[3];
            if (bo > 0 && bn > 0 && static_cast<size_t>(bo) + static_cast<size_t>(bn) <= f.data.size())
                banks.emplace_back(level, std::vector<uint8_t>(f.data.begin() + bo, f.data.begin() + bo + bn));
        }
        {  // the level's context image (its data sections, for a session's context of the level: context.h)
            std::vector<uint8_t> img(16 + f.image.size());
            const uint32_t head[4] = {0x31585443u /* CTX1 */, CONTEXT_BASE, f.code_lo, f.code_hi};
            std::memcpy(img.data(), head, 16);
            std::memcpy(img.data() + 16, f.image.data(), f.image.size());
            contexts.emplace_back(level, std::move(img));
        }
        idx += "level " + std::to_string(level) + " " + hex(L.classes) + " " + hex(L.with_data) + " " + hex(L.moby_textures) + " " +
               hex(L.asset_bytes) + " " + hex(L.class_bytes) + " " + hex(L.collision_bytes) + " " + hex(L.sky_bytes) + " " +
               hex(L.bank_bytes) + " " + hex(L.vram_bytes) + " " + hex(L.overlay_bytes) + "\n" + lines;
    }
    if (levels == 0) {
        *why = "the disc has no levels";
        return false;
    }
    char hh[24];
    std::snprintf(hh, sizeof hh, "%016" PRIx64, fnv(blocks.data(), blocks.size()) ^ fnv(reinterpret_cast<const uint8_t*>(idx.data()), idx.size()));
    const std::string json = "{\n  \"version\": " + std::to_string(POOL_VERSION) + ",\n  \"disc\": \"" + stamp_disc() + "\",\n  \"levels\": " +
                             std::to_string(levels) + ",\n  \"classes\": " + std::to_string(rows) + ",\n  \"blocks_bytes\": " +
                             std::to_string(blocks.size()) + ",\n  \"hash\": \"" + hh + "\"\n}\n";
    // (the stamp last: a pool cut short has none, and is made again)
    fs::remove(dir() / "pool.json", ec);
    fs::create_directories(dir() / "sounds", ec);
    for (const auto& [level, b] : banks) {
        char name[32];
        std::snprintf(name, sizeof name, "level%02d.bnk", level);
        if (!platform::write_file_replace(dir() / "sounds" / name, b.data(), b.size())) {
            *why = "the pool's sound banks could not be written in " + (dir() / "sounds").string();
            return false;
        }
    }
    fs::create_directories(dir() / "context", ec);
    for (const auto& [level, img] : contexts) {
        char name[32];
        std::snprintf(name, sizeof name, "level%02d.img", level);
        if (!platform::write_file_replace(dir() / "context" / name, img.data(), img.size())) {
            *why = "the pool's context images could not be written in " + (dir() / "context").string();
            return false;
        }
    }
    if (!platform::write_file_replace(dir() / "classes.bin", blocks.data(), blocks.size()) || !platform::write_file_replace(dir() / "pool.idx", idx) ||
        !platform::write_file_replace(dir() / "pool.json", json)) {
        *why = "the pool's files could not be written in " + dir().string();
        return false;
    }
    std::printf("[pool] made from %s: %zu levels, %zu class rows, %zu distinct blocks, %.1f MB\n", stamp_disc().c_str(), levels, rows,
                [&] {
                    size_t n = 0;
                    for (const auto& [h2, v] : by_hash) n += v.size();
                    return n;
                }(),
                blocks.size() / 1048576.0);
    return true;
}

// orig: (host) see pool.h
bool pool_open(bool build) {
    if (g_blocks != nullptr) return true;
    if (g_tried && !build) return false;
    g_tried = true;
    if (!stamp_ok()) {
        if (!build) return false;
        std::string why;
        std::printf("[pool] making the asset pool (%s)\n", fs::exists(dir() / "pool.json") ? "made from another disc or by an older version" : "none yet");
        if (!pool_build(&why)) {
            std::fprintf(stderr, "[pool] the asset pool could not be made: %s\n", why.c_str());
            return false;
        }
    }
    if (!read_index() || !platform::map_file(dir() / "classes.bin", &g_blocks, &g_blocks_size)) {
        std::fprintf(stderr, "[pool] %s could not be read\n", dir().string().c_str());
        g_blocks = nullptr;
        return false;
    }
    return true;
}

// orig: (host) see pool.h
const std::vector<PoolClass>& pool_classes() { return g_classes; }
const std::vector<PoolLevel>& pool_levels() { return g_levels; }

// orig: (host) see pool.h
const PoolClass* pool_find(int level, uint32_t id) {
    const auto it = g_by_key.find({level, id});
    return it != g_by_key.end() ? &g_classes[it->second] : nullptr;
}

// orig: (host) see pool.h
const uint8_t* pool_block(const PoolClass& c) {
    if (c.block_size == 0 || g_blocks == nullptr || static_cast<size_t>(c.block_off) + c.block_size > g_blocks_size) return nullptr;
    return reinterpret_cast<const uint8_t*>(g_blocks) + c.block_off;
}

// orig: (host) see pool.h
bool pool_sound_bank(int level, std::vector<uint8_t>& bank) {
    char name[32];
    std::snprintf(name, sizeof name, "level%02d.bnk", level);
    std::ifstream in(dir() / "sounds" / name, std::ios::binary);
    if (!in) return false;
    bank.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return !bank.empty();
}

// orig: (host) see pool.h
bool pool_context_image(int level, std::vector<uint8_t>& image, uint32_t* code_lo, uint32_t* code_hi) {
    char name[32];
    std::snprintf(name, sizeof name, "level%02d.img", level);
    std::ifstream in(dir() / "context" / name, std::ios::binary);
    if (!in) return false;
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    uint32_t head[4];
    if (b.size() < 16) return false;
    std::memcpy(head, b.data(), 16);
    if (head[0] != 0x31585443u || head[1] != CONTEXT_BASE || head[2] < CONTEXT_BASE || b.size() - 16 != head[2] - CONTEXT_BASE) return false;
    image.assign(b.begin() + 16, b.end());
    *code_lo = head[2];
    *code_hi = head[3];
    return true;
}

// orig: (host) see pool.h
void pool_report() {
    if (!pool_open(true)) return;
    std::printf("level | classes (with data) | class blocks MB | moby textures | collision MB | sky MB | bank MB | VRAM MB | overlay MB\n");
    size_t rows = 0, bytes = 0;
    for (const auto& l : g_levels) {
        std::printf("%5d | %3u (%3u) | %5.2f | %3u | %4.2f | %4.2f | %4.2f | %4.2f | %4.2f\n", l.level, l.classes, l.with_data, l.class_bytes / 1048576.0,
                    l.moby_textures, l.collision_bytes / 1048576.0, l.sky_bytes / 1048576.0, l.bank_bytes / 1048576.0, l.vram_bytes / 1048576.0,
                    l.overlay_bytes / 1048576.0);
        rows += l.classes;
        bytes += l.class_bytes;
    }
    std::map<uint32_t, std::set<uint64_t>> by_id;
    std::map<uint32_t, size_t> levels_of;
    std::set<std::pair<uint32_t, uint32_t>> distinct_blocks;
    for (const auto& c : g_classes) {
        ++levels_of[c.id];
        if (c.block_size != 0) {
            by_id[c.id].insert(c.hash);
            distinct_blocks.insert({c.block_off, c.block_size});
        }
    }
    size_t multi = 0, differ = 0, pooled = 0;
    for (const auto& [id, n] : levels_of) multi += n > 1;
    for (const auto& [id, hs] : by_id) differ += hs.size() > 1 && levels_of[id] > 1;
    for (const auto& [o, n] : distinct_blocks) pooled += (n + 63) & ~63u;
    std::printf("all levels: %zu class rows, %.1f MB of class blocks; %zu distinct class ids, %zu in more than one level, of those %zu with "
                "different bytes; classes.bin %zu blocks, %.1f MB\n",
                rows, bytes / 1048576.0, levels_of.size(), multi, differ, distinct_blocks.size(), pooled / 1048576.0);
}

// ---- the check against the native loads ----

namespace {
struct CheckCounts {
    size_t classes = 0, blocks_equal = 0, blocks_differ = 0, remap_differ = 0, fn_differ = 0, word_differ = 0, missing = 0;
};
std::map<int, CheckCounts> g_check;
}  // namespace

// orig: (host) see pool.h
bool pool_checking() {
    static const bool on = [] {
        const char* e = std::getenv("RAC1_POOL_CHECK");
        return e != nullptr && std::atoi(e) != 0 && pool_open(true);
    }();
    return on;
}

// orig: (host) see pool.h
void pool_check_class(int level, uint32_t id, uint32_t hdr, uint32_t remap) {
    CheckCounts& k = g_check[level];
    ++k.classes;
    const PoolClass* c = pool_find(level, id);
    if (c == nullptr) {
        ++k.missing;
        std::printf("[pool] check level %d: class %u is not in the pool\n", level, id);
        return;
    }
    uint8_t rm[16];
    for (uint32_t i = 0; i < 16; ++i) rm[i] = lbu(remap + i);
    if (std::memcmp(rm, c->remap, 16) != 0) {
        ++k.remap_differ;
        std::printf("[pool] check level %d: class %u's texture remap differs\n", level, id);
    }
    if ((hdr == 0) != (c->block_size == 0)) {
        ++k.blocks_differ;
        std::printf("[pool] check level %d: class %u %s data in the pool\n", level, id, hdr == 0 ? "has no data loaded, but" : "has data loaded, but no");
        return;
    }
    if (hdr == 0) return;
    const uint8_t* b = pool_block(*c);
    const uint8_t* ram = static_cast<const uint8_t*>(ee_ram_ptr_read(hdr));
    if (b != nullptr && ram != nullptr && std::memcmp(b, ram, c->block_size) == 0) {
        ++k.blocks_equal;
    } else {
        ++k.blocks_differ;
        std::printf("[pool] check level %d: class %u's block (%#x bytes at %08x) differs from the pool's\n", level, id, c->block_size, hdr);
    }
}

// orig: (host) see pool.h
void pool_check_registered(int level, uint32_t id, uint32_t idx) {
    const PoolClass* c = pool_find(level, id);
    if (c == nullptr) return;
    CheckCounts& k = g_check[level];
    if (lwu(OV(0x00197680) + idx * 4) != c->update_fn) {
        ++k.fn_differ;
        std::printf("[pool] check level %d: class %u's update function %08x, the pool's %08x\n", level, id, lwu(OV(0x00197680) + idx * 4), c->update_fn);
    }
    const uint32_t hdr = lwu(OV(0x00197300) + idx * 4);
    if (hdr != 0 && lwu(hdr + 0x2c) != c->word2c) {
        ++k.word_differ;
        std::printf("[pool] check level %d: class %u's +0x2c word %08x, the pool's %08x\n", level, id, lwu(hdr + 0x2c), c->word2c);
    }
}

// orig: (host) see pool.h
void pool_check_sounds(int level) {
    size_t classes = 0, sounds = 0, differ = 0;
    const uint32_t n = std::min<uint32_t>(lwu(OV(0x0015ffc0)), 0x100);
    std::set<uint32_t> seen;  // (a header several class ids share - the weapons' -: the last one's sounds are in it)
    for (uint32_t i = n; i-- > 0;) {
        const uint32_t hdr = lwu(OV(0x00197300) + i * 4);
        const PoolClass* c = pool_find(level, lhu(OV(0x00197a00) + i * 2));
        if (hdr == 0 || c == nullptr || lwu(hdr + 0x28) == 0 || !seen.insert(hdr).second) continue;
        ++classes;
        const uint32_t cnt = lbu(hdr + 0xd);
        for (uint32_t j = 0; j < cnt; ++j) {
            ++sounds;
            if (j >= c->sounds.size() || lhu(lwu(hdr + 0x28) + 32 * j + 0x1a) != c->sounds[j]) {
                ++differ;
                std::printf("[pool] check level %d: class %u's sound %u is %#x, the pool's %#x\n", level, c->id, j, lhu(lwu(hdr + 0x28) + 32 * j + 0x1a),
                            j < c->sounds.size() ? c->sounds[j] : 0xffffu);
            }
        }
        if (cnt != c->sounds.size()) {
            ++differ;
            std::printf("[pool] check level %d: class %u has %u sounds, the pool's row %zu\n", level, c->id, cnt, c->sounds.size());
        }
    }
    std::printf("[pool] check level %d: the sounds of %zu classes, %zu definitions, %zu differ: %s\n", level, classes, sounds, differ,
                differ == 0 ? "ok" : "FAILED");
}

// orig: (host) see pool.h
void pool_check_level_done(int level) {
    const CheckCounts k = g_check[level];
    g_check.erase(level);
    const bool ok = k.missing == 0 && k.blocks_differ == 0 && k.remap_differ == 0 && k.fn_differ == 0 && k.word_differ == 0;
    std::printf("[pool] check level %d: %zu classes, %zu blocks equal, %zu differ, %zu not in the pool, remaps differ %zu, update functions differ %zu, "
                "words differ %zu: %s\n",
                level, k.classes, k.blocks_equal, k.blocks_differ, k.missing, k.remap_differ, k.fn_differ, k.word_differ, ok ? "ok" : "FAILED");
}

}  // namespace rac1
