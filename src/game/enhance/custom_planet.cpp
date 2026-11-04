// See custom_planet.h.
#include <host/custom_planet.h>
#include <host/custom_arena.h>

#include <game/ovl/level_change.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/disc.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace rac1 {

namespace {

constexpr int32_t TEXT_NAME = 0x7d00, TEXT_REGION = 0x7d01;  // our text ids (after the game's, before update_notice's 0x7e00..)
constexpr uint32_t TEXTS = 0x000fe000u;                     // their strings (0xfe000..0xfe100: past advanced.cpp's 0xfb000..0xfe000,
constexpr uint32_t NAME_SIZE = 0x80, REGION_SIZE = 0x80;    // in the same unused first megabyte)
constexpr OvConst<0x001c1e40u> PLANET_TEXTS{};              // the planets' texts (0xc a planet: +0 the region, +4 the name)
constexpr OvConst<0x001b9cf0u> MENU_STATE{};                // the menu's state (0xf, 0x10: the ship's map, galactic_map.cpp)
constexpr OvConst<0x00184414u> MENU_PLANET{};               // the planet the ship's map shows
constexpr OvConst<0x001848c0u> LEVEL_LIST{};                 // the save's per-level records (16 bytes: address, size, id, flag)
constexpr int32_t BASE = 0;                                  // the slot the planet runs in (the shadow slot: level 0)
constexpr uint32_t PC_ITEM = 0x200;                          // our item in save<N>.pc (advanced.cpp's ids are below 0x100)
constexpr uint32_t PC_VERSION = 1;

const char* const kLangs[6] = {"en", "en", "fr", "de", "es", "it"};  // the text language 0x15ed88 -> planet.txt's suffix

struct Planet {
    bool read = false, ok = false;
    std::string name = "Custom Planet", region;
    std::string names[6], regions[6];  // per text language ("" = the plain one)
    bool has_death = false, has_ship = false;
    float death = 0.0f, ship[4] = {};
};
Planet g_p;

// One of the level's records in the save's per-level list: its entry, its level-0 address and size, where it is in a copy
struct Record {
    uint32_t entry = 0, addr = 0, size = 0, at = 0;
};

struct State {
    bool wanted = false;   // the ship's row chosen (or a save made on the planet loaded): level 0's next load is the planet
    bool swapped = false;  // EE memory holds the planet's level records, `base` Veldin's
    std::vector<Record> records;  // (the list as it was at the swap)
    std::vector<uint8_t> base;    // Veldin's records while the planet's are in EE memory
    std::vector<uint8_t> planet;  // the planet's records while Veldin's are in EE memory (empty: none yet, all zero)
    bool saved_on = false;        // (a save's .pc read) the save was made on the planet
} g_s;

bool trace() {
    static const bool t = std::getenv("RAC1_CUSTOM_TRACE") != nullptr;
    return t;
}

// orig: (none) — planet.txt read (once)
const Planet& planet() {
    if (g_p.read) return g_p;
    g_p.read = true;
    if (custom_level_dir().empty()) return g_p;
    g_p.ok = true;
    std::ifstream f(custom_level_dir() + "/planet.txt");
    std::string line;
    while (f && std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream in(line);
        std::string key;
        in >> key;
        std::string rest;
        std::getline(in >> std::ws, rest);
        while (!rest.empty() && (rest.back() == '\r' || rest.back() == ' ')) rest.pop_back();
        auto lang_of = [&key](const char* what) {
            const size_t n = std::strlen(what);
            if (key.compare(0, n, what) != 0 || key.size() != n + 3 || key[n] != '.') return -1;
            for (int l = 0; l < 6; ++l)
                if (key.compare(n + 1, 2, kLangs[l]) == 0) return l;
            return -1;
        };
        if (key == "name") g_p.name = rest;
        else if (key == "region") g_p.region = rest;
        else if (const int l = lang_of("name"); l >= 0) g_p.names[l] = rest;
        else if (const int l2 = lang_of("region"); l2 >= 0) g_p.regions[l2] = rest;
        else if (key == "death_height") g_p.has_death = std::sscanf(rest.c_str(), "%f", &g_p.death) == 1;
        else if (key == "ship") g_p.has_ship = std::sscanf(rest.c_str(), "%f %f %f %f", &g_p.ship[0], &g_p.ship[1], &g_p.ship[2], &g_p.ship[3]) == 4;
    }
    for (int l = 0; l < 6; ++l) {
        if (g_p.names[l].empty()) g_p.names[l] = g_p.name;
        if (g_p.regions[l].empty()) g_p.regions[l] = g_p.region;
    }
    std::printf("[planet] custom planet \"%s\" (%s)%s\n", g_p.name.c_str(), custom_level_dir().c_str(), custom_level_sticky() ? ": RAC1_CUSTOM_LEVEL, level 0 is the planet" : "");
    return g_p;
}

// orig: (none) — the save's per-level list's records of the base slot (as the overlay now has them)
std::vector<Record> base_records() {
    std::vector<Record> r;
    uint32_t at = 0;
    for (uint32_t e = LEVEL_LIST; lw(e) != 0 && r.size() < 64; e += 0x10) {
        const uint32_t size = lwu(e + 4);
        if (size == 0 || size > 0x10000) break;  // (not the list: nothing swapped)
        if (lwu(e) == 0x0013dd58u) continue;  // (the levels' visited / done flags: Veldin's stays, the story's tests read it)
        r.push_back(Record{e, lwu(e) + static_cast<uint32_t>(BASE) * size, size, at});
        at += size;
    }
    return r;
}

uint32_t records_bytes(const std::vector<Record>& r) { return r.empty() ? 0 : r.back().at + r.back().size; }

std::vector<uint8_t> records_read(const std::vector<Record>& r) {
    std::vector<uint8_t> d(records_bytes(r));
    for (const Record& x : r)
        for (uint32_t i = 0; i < x.size; ++i) d[x.at + i] = static_cast<uint8_t>(lbu(x.addr + i));
    return d;
}

void records_write(const std::vector<Record>& r, const std::vector<uint8_t>& d) {
    for (const Record& x : r)
        for (uint32_t i = 0; i < x.size; ++i) sb(x.addr + i, x.at + i < d.size() ? d[x.at + i] : 0u);
}

// orig: (none) — testing: a sum of the bytes (to compare in the traces)
uint32_t bytes_sum(const std::vector<uint8_t>& d) {
    uint32_t h = 2166136261u;
    for (uint8_t b : d) h = (h ^ b) * 16777619u;
    return h;
}

// orig: (none) — the planet's records put in, Veldin's kept
void swap_in() {
    g_s.records = base_records();
    if (g_s.records.empty()) return;
    g_s.base = records_read(g_s.records);
    records_write(g_s.records, g_s.planet);
    g_s.swapped = true;
    if (trace()) {
        std::printf("[planet] level records swapped in: %zu records, %u bytes (the planet's: %s %08x; Veldin's kept %08x)\n", g_s.records.size(),
                    records_bytes(g_s.records), g_s.planet.empty() ? "new" : "kept", bytes_sum(g_s.planet), bytes_sum(g_s.base));
        for (const Record& x : g_s.records) std::printf("[planet]   id %u: 0x%x + 0x%x\n", lwu(x.entry + 8), x.addr, x.size);
    }
}

// orig: (none) — the planet's records kept, Veldin's put back
void swap_out() {
    g_s.planet = records_read(g_s.records);
    records_write(g_s.records, g_s.base);
    g_s.base.clear();
    g_s.swapped = false;
    if (trace())
        std::printf("[planet] level records swapped back (Veldin's in EE memory %08x, the planet's %zu bytes kept %08x)\n", bytes_sum(records_read(g_s.records)),
                    g_s.planet.size(), bytes_sum(g_s.planet));
}

}  // namespace

// orig: (none)
bool custom_planet_exists() { return planet().ok; }

// orig: (none)
uint32_t custom_planet_list_row(uint32_t list, uint32_t n) {
    if (!custom_planet_exists() || n >= 0x14) return n;
    const uint32_t e = list + n * 0xc;
    sh(e, static_cast<uint32_t>(TEXT_REGION));  // (as the game's rows: the region, the planet's name under it)
    sh(e + 2, 1);
    sw(e + 4, 0);
    sh(e + 8, static_cast<uint32_t>(TEXT_NAME));
    return n + 1;
}

// orig: (none)
void custom_planet_choose() {
    if (!custom_planet_exists()) return;
    g_s.wanted = true;
    if (lw(0x0015ed84) != BASE) custom_level_set_on(true);  // (else at the header's read: level 0 is still shown)
    std::printf("[planet] the ship's row chosen: \"%s\"\n", planet().name.c_str());
}

// orig: (none)
bool custom_planet_travel(int32_t planet_id) {
    if (!custom_planet_exists()) return false;
    if (planet_id != BASE) {
        g_s.wanted = false;  // (another planet: level 0 stays the planet until that one's header is read)
        return false;
    }
    custom_planet_choose();
    return lw(0x0015ed84) == BASE;
}

// orig: (none)
uint32_t custom_planet_text(int32_t id) {
    if (!custom_planet_exists() || id < 0x100) return 0;
    // on the ship's map, the planet it shows is planet 0 (only the custom planet's row points there): Veldin's name and
    // region (the planet page's labels) are the custom planet's
    const uint32_t ms = lwu(MENU_STATE);
    if ((ms == 0xf || ms == 0x10) && lw(MENU_PLANET) == BASE) {
        if (id == static_cast<int32_t>(lhu(PLANET_TEXTS))) id = TEXT_REGION;
        else if (id == static_cast<int32_t>(lhu(PLANET_TEXTS + 4))) id = TEXT_NAME;
    }
    if (id != TEXT_NAME && id != TEXT_REGION) return 0;
    uint32_t lang = lwu(0x0015ed88);
    if (lang >= 6) lang = 0;
    const bool name = id == TEXT_NAME;
    const std::string& s = name ? g_p.names[lang] : g_p.regions[lang];
    const uint32_t at = name ? TEXTS : TEXTS + NAME_SIZE, size = name ? NAME_SIZE : REGION_SIZE;
    for (uint32_t i = 0; i < size; ++i) sb(at + i, i < s.size() && i + 1 < size ? static_cast<uint8_t>(s[i]) : 0u);
    return at;
}

// orig: (none)
void custom_planet_level_header(int32_t level) {
    if (!custom_planet_exists()) return;
    if (level != BASE) g_s.wanted = false;
    custom_level_set_on(level == BASE && (g_s.wanted || custom_level_sticky()));
}

// orig: (none)
void custom_planet_level_start(int32_t level) {
    if (!custom_planet_exists()) return;
    const bool on = custom_level_active(level);
    if (on && !g_s.swapped) swap_in();
    else if (!on && g_s.swapped) swap_out();
}

// orig: (none)
void custom_planet_settings(uint32_t st) {
    if (!custom_level_active(lw(0x0015ed84))) return;
    const Planet& p = planet();
    if (p.has_death) swc1(st + 0x28, p.death);
    if (p.has_ship)
        for (uint32_t i = 0; i < 4; ++i) swc1(st + 0x2c + i * 4, p.ship[i]);
}

// orig: (none)
bool custom_planet_skip_story() { return custom_planet_exists() && (g_s.wanted || custom_level_on()); }

// orig: (none)
bool custom_planet_save_record(uint32_t list, int32_t index, uint32_t e, uint32_t out) {
    if (!g_s.swapped || index != BASE || list != LEVEL_LIST) return false;
    for (const Record& x : g_s.records) {
        if (x.entry != e || x.size != lwu(e + 4)) continue;
        for (uint32_t i = 0; i < x.size; ++i) sb(out + i, g_s.base[x.at + i]);
        return true;
    }
    return false;
}

// orig: (none)
void custom_planet_block_read(uint32_t list, int32_t index) {
    if (!custom_planet_exists() || index != BASE || list != LEVEL_LIST) return;
    // EE memory has the save's (Veldin's) records: nothing to swap back; the planet's state is the save's (its .pc, next)
    g_s.swapped = false;
    g_s.base.clear();
    g_s.planet.clear();
    g_s.saved_on = false;
}

// orig: (none)
void custom_planet_new_game() {
    if (!custom_planet_exists()) return;
    custom_arena_new_game();
    g_s.wanted = false;
    g_s.saved_on = false;
    g_s.planet.clear();
}

// orig: (none)
uint32_t custom_planet_pc_items(std::vector<uint8_t>& d) {
    if (!custom_planet_exists()) return 0;
    // the planet's records: in EE memory while it is played, else the copy (none yet: no item)
    std::vector<uint8_t> rec = g_s.swapped ? records_read(g_s.records) : g_s.planet;
    const bool on = g_s.wanted || custom_level_on();
    const uint32_t arena = custom_arena_pc_items(d);  // (the arena's victories: our item 0x201)
    if (rec.empty() && !on) return arena;
    std::vector<uint8_t> item;
    auto u32 = [&item](uint32_t v) {
        for (int i = 0; i < 4; ++i) item.push_back(static_cast<uint8_t>(v >> (8 * i)));
    };
    const std::string& name = g_p.name;
    u32(PC_VERSION);
    u32(on ? 1u : 0u);
    u32(static_cast<uint32_t>(name.size()));
    item.insert(item.end(), name.begin(), name.end());
    u32(static_cast<uint32_t>(rec.size()));
    item.insert(item.end(), rec.begin(), rec.end());
    if (item.size() > 0xffff) return arena;
    d.push_back(static_cast<uint8_t>(PC_ITEM));
    d.push_back(static_cast<uint8_t>(PC_ITEM >> 8));
    d.push_back(static_cast<uint8_t>(item.size()));
    d.push_back(static_cast<uint8_t>(item.size() >> 8));
    d.insert(d.end(), item.begin(), item.end());
    if (trace()) std::printf("[planet] .pc item: on %d, %zu record bytes\n", on ? 1 : 0, rec.size());
    return 1 + arena;
}

// orig: (none)
bool custom_planet_pc_item(uint32_t id, const uint8_t* p, uint32_t size) {
    if (custom_arena_pc_item(id, p, size)) return true;
    if (id != PC_ITEM) return false;
    if (!custom_planet_exists()) return true;
    auto u32 = [p, size](uint32_t at) { return at + 4 <= size ? static_cast<uint32_t>(p[at] | p[at + 1] << 8 | p[at + 2] << 16 | static_cast<uint32_t>(p[at + 3]) << 24) : 0u; };
    if (u32(0) != PC_VERSION) return true;
    const uint32_t nlen = u32(8);
    if (12 + nlen + 4 > size || std::string(reinterpret_cast<const char*>(p + 12), nlen) != g_p.name) return true;  // (another planet's)
    const uint32_t rlen = u32(12 + nlen);
    if (12 + nlen + 4 + rlen > size) return true;
    g_s.saved_on = u32(4) != 0;
    g_s.planet.assign(p + 16 + nlen, p + 16 + nlen + rlen);
    if (trace()) std::printf("[planet] .pc item read: on %d, %u record bytes\n", g_s.saved_on ? 1 : 0, rlen);
    return true;
}

// orig: (none)
void custom_planet_save_loaded(bool) {
    if (!custom_planet_exists()) return;
    g_s.wanted = g_s.saved_on;  // (the save's level 0, made on the planet: the planet; its header's read decides)
    if (g_s.wanted) std::printf("[planet] the save was made on \"%s\": its level 0 is the planet\n", g_p.name.c_str());
}

// orig: (none) — testing: RAC1_PLANET_SAVE_IMAGE=<frame>[,<frame>..]:<file prefix>: at those game frames the save's blocks as
// the card would get them (save_block_write: the game's block, then the 20 levels', each block written in the save
// buffer 0x1506d0 and copied out) to <prefix><frame>.bin, and our .pc item to <prefix><frame>.pc: a save made on the
// planet must give the same level blocks as one made elsewhere
void custom_planet_test_frame(uint64_t frame) {
    static const char* spec = std::getenv("RAC1_PLANET_SAVE_IMAGE");
    if (spec == nullptr) return;
    const char* colon = std::strchr(spec, ':');
    if (colon == nullptr) return;
    bool now = false;
    for (const char* q = spec; q < colon;) {
        char* e = nullptr;
        if (std::strtoull(q, &e, 10) == frame && e != q) now = true;
        if (e == q || *e != ',') break;
        q = e + 1;
    }
    if (!now) return;
    constexpr uint32_t BUF = 0x001506d0u;
    std::vector<uint8_t> out;
    auto block = [&out](int32_t index, uint32_t list) {
        const uint32_t n = static_cast<uint32_t>(save_block_write(BUF, index, list));
        for (uint32_t i = 0; i < n; ++i) out.push_back(static_cast<uint8_t>(lbu(BUF + i)));
    };
    block(0, OV(0x001845c0));
    const size_t game = out.size();
    for (int32_t i = 0; i < 0x14; ++i) block(i, LEVEL_LIST);
    const std::string base = std::string(colon + 1) + std::to_string(frame);
    if (std::FILE* f = std::fopen((base + ".bin").c_str(), "wb")) {
        std::fwrite(out.data(), 1, out.size(), f);
        std::fclose(f);
    }
    std::vector<uint8_t> pc;
    custom_planet_pc_items(pc);
    if (std::FILE* f = std::fopen((base + ".pc").c_str(), "wb")) {
        std::fwrite(pc.data(), 1, pc.size(), f);
        std::fclose(f);
    }
    std::printf("[planet] RAC1_PLANET_SAVE_IMAGE: frame %llu: game block %zu bytes, level blocks %zu bytes (%08x), .pc item %zu bytes -> %s.*\n",
                static_cast<unsigned long long>(frame), game, out.size() - game,
                bytes_sum(std::vector<uint8_t>(out.begin() + static_cast<std::ptrdiff_t>(game), out.end())), pc.size(), base.c_str());
}

}  // namespace rac1
