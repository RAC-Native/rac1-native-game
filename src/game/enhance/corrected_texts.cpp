// Corrected texts (see corrected_texts.h). Not a port: the game's text tables are left as it relocates them, only the
// string pointers of the corrected records changed.
#include <game/enhance/corrected_texts.h>

#include <game/enhance/advanced.h>
#include <game/ovl/ovl_common.h>

#include <formats/scene_wad.h>
#include <platform/platform.h>
#include <runtime/kernel/disc.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace rac1 {

namespace {
constexpr uint32_t CTX = 0x000fe000u;      // the corrected strings (0xfe000..0xffc00; fixes102.cpp's block from 0xffc00)
constexpr uint32_t CTX_END = 0x000ffc00u;
constexpr int LEVELS = 19;
constexpr int32_t LANGS[] = {0, 2, 3, 4, 5};  // English, French, German, Spanish, Italian (1 empty, 6 Japanese, 7 Korean)

// The data discs that give the corrected texts, in the order they are taken (the first one that has a string wins)
struct Source {
    const char* serial;
    const char* version;
    bool platinum;  // the PAL disc 2.00: its ids renumbered after its new credits lines, its credits not taken
};
constexpr Source SOURCES[] = {{"SCES-50916", "2.00", true}, {"SCES-50916", "1.00", false}, {"SCPS-15037", "1.02", false}};
constexpr int32_t CREDITS_FIRST = 20649, CREDITS_LAST = 20853;  // the credits' ids (North American)

using Table = std::map<int32_t, std::string>;  // id -> string

// orig: (none) — enhancement: a file's bytes; empty when it cannot be read
std::vector<std::byte> read_file(const std::string& path) {
    std::vector<std::byte> d;
    std::FILE* f = platform::open_file(path, "rb");
    if (f == nullptr) return d;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n > 0) {
        d.resize(static_cast<size_t>(n));
        if (std::fread(d.data(), 1, d.size(), f) != d.size()) d.clear();
    }
    std::fclose(f);
    return d;
}

// orig: (none) — enhancement: a level's gameplay file's text tables by language (the file's header +0x10 + language * 4:
// a block's offset; the block: count, size, then 16-byte records {string's offset in the block, id, -1, 0}); none when
// the file is not there or not understood
std::map<int32_t, Table> gameplay_tables(const std::string& path) {
    std::map<int32_t, Table> out;
    const std::vector<std::byte> raw = read_file(path);
    std::vector<uint8_t> b;
    if (raw.size() >= 3 && std::memcmp(raw.data(), "WAD", 3) == 0) {
        if (!decompress_wad(raw, b)) return out;
    } else {
        b.resize(raw.size());
        if (!raw.empty()) std::memcpy(b.data(), raw.data(), raw.size());
    }
    auto u32 = [&b](size_t at) {
        uint32_t v = 0;
        if (at + 4 <= b.size()) std::memcpy(&v, b.data() + at, 4);
        return v;
    };
    if (b.size() < 0x30) return out;
    for (const int32_t lang : LANGS) {
        const uint32_t o = u32(0x10 + static_cast<uint32_t>(lang) * 4);
        if (o == 0 || o + 8 > b.size()) continue;
        const int32_t count = static_cast<int32_t>(u32(o));
        if (count <= 0 || o + 8 + static_cast<uint64_t>(count) * 16 > b.size()) continue;
        Table t;
        bool ok = true;
        for (int32_t i = 0; i < count && ok; ++i) {
            const size_t rec = o + 8 + static_cast<size_t>(i) * 16;
            const size_t s = o + u32(rec);
            const int32_t id = static_cast<int32_t>(u32(rec + 4));
            size_t e = s;
            while (e < b.size() && b[e] != 0) ++e;
            if (e >= b.size()) ok = false;
            else t[id] = std::string(reinterpret_cast<const char*>(b.data() + s), e - s);
        }
        if (ok) out[lang] = std::move(t);
    }
    return out;
}

// orig: (none) — enhancement: the PAL disc 2.00's table with the North American ids: its new credits lines (20695,
// 20743..20746) left out, the ids after each of them taken back, and the credits not taken
Table platinum_to_ntsc(const Table& t) {
    Table out;
    for (const auto& [id, s] : t) {
        int32_t n = id;
        if (id == 20695 || (id >= 20743 && id <= 20746)) continue;
        if (id > 20746) n = id - 5;
        else if (id > 20695) n = id - 1;
        if (n >= CREDITS_FIRST && n <= CREDITS_LAST) continue;
        out[n] = s;
    }
    return out;
}

// orig: (none) — enhancement: the ids of a table
std::set<int32_t> ids_of(const Table& t) {
    std::set<int32_t> s;
    for (const auto& kv : t) s.insert(kv.first);
    return s;
}

struct Corrections {
    bool built = false;
    std::map<uint32_t, uint32_t> where;  // (language << 16 | id) -> the string's EE address in CTX
    std::vector<uint8_t> block;          // CTX's bytes
};
Corrections g_corr;

// orig: (none) — enhancement: the data discs that give the corrected texts (their folders), in SOURCES' order
std::vector<std::pair<const Source*, std::string>> source_discs() {
    std::vector<std::pair<const Source*, std::string>> out;
    const std::vector<DataDisc> discs = disc_data_discs();
    for (const Source& s : SOURCES)
        for (const DataDisc& d : discs)
            if (d.ok && d.serial == s.serial && d.version == s.version) {
                out.emplace_back(&s, d.dir);
                break;
            }
    return out;
}

// orig: (none) — enhancement: the corrections found, once (the files read and compared, the strings laid out for CTX)
void corrections_build() {
    if (g_corr.built) return;
    g_corr.built = true;
    const auto sources = source_discs();
    std::map<uint32_t, std::string> found;  // (language << 16 | id) -> the corrected string
    int used[3] = {};
    char name[64];
    for (int lv = 0; lv < LEVELS; ++lv) {
        std::snprintf(name, sizeof name, "/levels/level%02d/", lv);
        const auto base = gameplay_tables(std::string(disc_files_dir()) + name + "gameplay_ntsc.bin");
        std::vector<std::map<int32_t, Table>> src;
        for (const auto& [s, dir] : sources) {
            auto t = gameplay_tables(dir + name + "gameplay_pal.bin");
            if (s->platinum)
                for (auto& [lang, table] : t) table = platinum_to_ntsc(table);
            src.push_back(std::move(t));
        }
        for (const auto& [lang, table] : base) {
            // the sources' tables of this language that are used: a table whose ids are not the North American ones is
            // not (the PAL disc 2.00's lacks its credits only). Decided once a table (compared again for each of its
            // ~1700 strings, the ids' sets made each time, it took 20..30 s on the game's thread at the title's load)
            const std::set<int32_t> ids = ids_of(table);
            std::vector<const Table*> usable(src.size(), nullptr);
            for (size_t k = 0; k < src.size(); ++k) {
                const auto it = src[k].find(lang);
                if (it == src[k].end()) continue;
                std::set<int32_t> want = ids;
                if (sources[k].first->platinum)
                    for (int32_t c = CREDITS_FIRST; c <= CREDITS_LAST; ++c) want.erase(c);
                if (ids_of(it->second) == want) usable[k] = &it->second;
            }
            for (const auto& [id, text] : table) {
                for (size_t k = 0; k < src.size(); ++k) {
                    if (usable[k] == nullptr) continue;
                    const Table& t = *usable[k];
                    const auto s = t.find(id);
                    if (s == t.end()) continue;  // (the PAL disc 2.00's credits: the next disc's)
                    // (Spanish 20172: the PAL disc 1.00 made it a copy of 20171, the PAL disc 2.00 put it back;
                    // never taken from the others)
                    if (lang == 4 && id == 20172 && !sources[k].first->platinum) break;
                    const uint32_t key = static_cast<uint32_t>(lang) << 16 | static_cast<uint32_t>(id);
                    if (s->second != text && found.count(key) == 0) {
                        found[key] = s->second;
                        ++used[k];
                    }
                    break;
                }
            }
        }
    }
    for (const auto& [key, s] : found) {
        if (CTX + g_corr.block.size() + s.size() + 1 > CTX_END) {
            std::fprintf(stderr, "[corrected texts] the strings do not fit in 0x%x..0x%x: the others left out\n", CTX, CTX_END);
            break;
        }
        g_corr.where[key] = CTX + static_cast<uint32_t>(g_corr.block.size());
        g_corr.block.insert(g_corr.block.end(), s.begin(), s.end());
        g_corr.block.push_back(0);
    }
    std::printf("[corrected texts] %zu strings corrected (%zu bytes)", g_corr.where.size(), g_corr.block.size());
    for (size_t k = 0; k < sources.size(); ++k)
        std::printf("%s %s %s: %d", k == 0 ? ";" : ",", sources[k].first->serial, sources[k].first->version, used[k]);
    std::printf("\n");
}

// orig: (none) — enhancement: RAC1_CORRECTED_TEXTS=1 / 0: -1 unset
int env_choice() {
    static const int v = [] {
        const char* e = std::getenv("RAC1_CORRECTED_TEXTS");
        if (e == nullptr || *e == 0) return -1;
        if (std::strcmp(e, "1") == 0) return 1;
        if (std::strcmp(e, "0") == 0) return 0;
        std::fprintf(stderr, "RAC1_CORRECTED_TEXTS=%s: 1 or 0\n", e);
        return -1;
    }();
    return v;
}
}  // namespace

// orig: (none) — enhancement (corrected_texts.h)
bool corrected_texts_available() {
    static const bool v = !disc_other_region() && !source_discs().empty();
    return v;
}

// orig: (none) — enhancement (corrected_texts.h)
bool corrected_texts_on() {
    if (g_advanced_disabled || !corrected_texts_available()) return false;
    const int env = env_choice();
    if (env >= 0) return env != 0;
    return advanced_corrected_texts();
}

namespace {
// The title's tables (title_load), while they are there (until a level's load): the main menu's row takes effect on them
// at once (corrected_texts_row_changed); the records' original strings, to put back when it is turned off
struct TitleTable {
    uint32_t table;
    int32_t count, lang;
};
std::vector<TitleTable> g_title;
bool g_title_patched = false;
std::vector<std::pair<uint32_t, uint32_t>> g_title_orig;  // (record, its original string)

// orig: (none) — enhancement: a table's corrected records pointed at CTX (the originals noted in `orig` when given)
void patch_table(uint32_t table, int32_t count, int32_t lang, std::vector<std::pair<uint32_t, uint32_t>>* orig) {
    corrections_build();
    if (g_corr.where.empty()) return;
    // (the strings written each time: the block is ours, and a table may be patched before anything else ran)
    std::memcpy(ee_ram_ptr(CTX), g_corr.block.data(), g_corr.block.size());
    int32_t n = 0;
    for (int32_t i = 0; i < count; ++i) {
        const uint32_t rec = table + static_cast<uint32_t>(i) * 16;
        const int32_t id = lw(rec + 4);
        if (id < 0 || id >= 0x10000) continue;
        const auto it = g_corr.where.find(static_cast<uint32_t>(lang) << 16 | static_cast<uint32_t>(id));
        if (it == g_corr.where.end() || lwu(rec) == it->second) continue;
        if (orig != nullptr) orig->emplace_back(rec, lwu(rec));
        sw(rec, it->second);
        ++n;
    }
    if (n != 0) std::printf("[corrected texts] language %d: %d of the table's %d strings corrected\n", lang, n, count);
}
}  // namespace

// orig: (none) — enhancement (corrected_texts.h)
void corrected_texts_patch(uint32_t table, int32_t count, int32_t lang) {
    g_title.clear();  // (a level's table: the title's are gone)
    g_title_orig.clear();
    g_title_patched = false;
    if (corrected_texts_on()) patch_table(table, count, lang, nullptr);
}

// orig: (none) — enhancement (corrected_texts.h)
void corrected_texts_title_patch(uint32_t table, int32_t count, int32_t lang) {
    if (lang == 0) {  // (the first of the eight: the title loaded again)
        g_title.clear();
        g_title_orig.clear();
        g_title_patched = false;
    }
    g_title.push_back({table, count, lang});
    if (!corrected_texts_on()) return;
    patch_table(table, count, lang, &g_title_orig);
    g_title_patched = true;
}

// orig: (none) — enhancement (corrected_texts.h)
void corrected_texts_row_changed() {
    if (g_title.empty() || corrected_texts_on() == g_title_patched) return;
    if (!g_title_patched) {
        for (const TitleTable& t : g_title) patch_table(t.table, t.count, t.lang, &g_title_orig);
        g_title_patched = true;
        return;
    }
    for (const auto& [rec, s] : g_title_orig) sw(rec, s);
    g_title_orig.clear();
    g_title_patched = false;
}

}  // namespace rac1
