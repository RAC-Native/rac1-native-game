// See level_slots.h. Not a port: the host's level slots, written on top of the game's own level change, saves and planet
// list (the experiment that tried a level mod on Veldin's slot first is its source for the records' swap and the row).
#include <host/level_slots.h>
#include <host/mod_code.h>

#include <game/ovl/level_change.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/pause_menu_pages.h>
#include <game/ovl/planet_map.h>
#include <runtime/kernel/disc.h>
#include <runtime/kernel/mods.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace rac1 {

namespace {

float yaw_wrap(float a) {
    while (a > 3.14159265f) a -= 6.2831853f;
    while (a < -3.14159265f) a += 6.2831853f;
    return a;
}

// Our texts: each level row's name and region (ids after the Mods page's 0x7d00.., before the update notice's 0x7e00..:
// 0x7dc0 + 2 k the name of the k-th level mod on, + 1 its region), their strings in the mods' block of EE memory
// (0xe0000..0xf0000, in the PS2 kernel's first megabyte, which the port's kernel leaves unused; the level mods' texts its
// first 0x1000 bytes, 0x100 a level)
constexpr int32_t TEXT_NAME = 0x7dc0, TEXT_REGION = 0x7dc1, TEXT_END = TEXT_NAME + 2 * kLevelSlots;
constexpr uint32_t TEXTS = 0x000e0000u;
constexpr uint32_t NAME_SIZE = 0x80, REGION_SIZE = 0x80;
static_assert(TEXT_END <= 0x7e00 && (NAME_SIZE + REGION_SIZE) * kLevelSlots <= 0x1000, "the level rows' texts");
constexpr uint32_t UNLOCKED = 0x0013d510u;       // the planets the ship can fly to (words, 0 ends)
constexpr uint32_t ROWS_MAX = 0x14;              // the planet list's rows at most (galmap_list_focus_in)
constexpr OvConst<0x001c1e40u> PLANET_TEXTS{};  // the planets' texts (0xc a planet: +0 the region, +4 the name)
constexpr OvConst<0x001b9cf0u> MENU_STATE{};    // the menu's state (0xe..0x10: the ship's planet map, galactic_map.cpp)
constexpr OvConst<0x00184414u> MENU_PLANET{};   // the planet the ship's map shows
constexpr OvConst<0x001848c0u> LEVEL_LIST{};    // the save's per-level records (16 bytes: address, size, id, flag)
constexpr uint32_t LEVEL = 0x0015ed84u;          // the game's current level
constexpr uint32_t HEADER = 0x0013a4e0u;         // the level's header (+8 data, +0x10 gameplay NTSC, +0x18 PAL, +0x20 occlusion)
constexpr uint32_t PC_ITEM = 0x200;              // our item in save<N>.pc (advanced.cpp's ids are below 0x100)
constexpr uint32_t PC_VERSION = 1;

const char* const kLangs[6] = {"en", "en", "fr", "de", "es", "it"};  // the text language 0x15ed88 -> level.txt's suffix

// A slot's level as the player sees it (its mod's level.txt)
struct Names {
    bool read = false;
    std::string names[6], regions[6];
};

// One of the base level's records in the save's per-level list: its entry, its address and size, where it is in a copy
struct Record {
    uint32_t entry = 0, addr = 0, size = 0, at = 0;
};

// A level's progress kept by the host (by level.id): its records while it is not in EE memory
struct Progress {
    std::vector<uint8_t> records;  // (empty: none yet, all zero)
};

// An item of save<N>.pc read: a level's (ours when its mod is on, else carried over as it was)
struct PcItem {
    std::string id;
    bool on = false;      // the save was made on that level
    int32_t from = -1;    // the level flown from
    std::vector<uint8_t> records;
    std::vector<uint8_t> raw;  // the item's bytes
};

struct State {
    int pending = -1;          // the slot chosen (the ship's row, a save made there): its base level's next load is it
    int current = -1;          // the slot whose level is current
    int last_real = 1;         // the last of the game's own levels loaded (a slot's level flown from)
    int from = -1;             // the level the current slot's level was flown from
    int pending_from = -1;     // (a save made there loaded) the level it was flown from, for the next load
    bool swapped = false;      // EE memory holds the current slot's records, `base` the base level's
    std::vector<Record> records;  // (the list as it was at the swap)
    std::vector<uint8_t> base;    // the base level's records while a slot's are in EE memory
    std::map<std::string, Progress> progress;  // by level.id
    std::vector<PcItem> carried;  // the items of the save loaded whose level is not on (written back as they were)
    std::string saved_on;      // (a save's .pc read) the level.id it was made on ("" none)
    int32_t saved_from = -1;   // ... and the level flown from
    bool prompt = false;       // the planet prompt waits (the save's level is not on)
    bool prompt_open = false;  // the ship's planet map is open as the planet prompt (until a travel)
    std::string prompt_name;   // its name, for the log
    int prompt_frames = 0;
    // the ship's planet list as galmap_list_focus_in last made it (our rows added): each row's planet and level slot (-1:
    // the planet's own level); the row selected's slot (-1: none of ours)
    struct Row {
        int32_t planet = 0;
        int slot = -1;
    };
    std::vector<Row> rows;
    int row_sel = -1;
} g_s;

Names g_names[16];  // per slot - kFirstLevelSlot
std::atomic<int> g_current{-1};  // (g_s.current for the renderer's threads: level_slot_of)

bool trace() {
    static const bool t = std::getenv("RAC1_LEVEL_TRACE") != nullptr;
    return t;
}

const ModInfo* slot_mod(int slot) { return slot >= kFirstLevelSlot ? mods_level_at_slot(slot) : nullptr; }

// the k-th level mod on (its row's texts), -1 when none
int slot_index(int slot) {
    const std::vector<const ModInfo*> ls = mods_levels();
    for (size_t k = 0; k < ls.size(); ++k)
        if (ls[k]->level_slot == slot) return static_cast<int>(k);
    return -1;
}

// our rows are on the ship's list: a level mod on, or the planet prompt's map
bool rows_on() { return g_s.prompt_open || level_slots_on(); }

int base_of(int slot) {
    const ModInfo* m = slot_mod(slot);
    return m != nullptr ? m->level_base : -1;
}

// orig: (none) — enhancement: slot `slot`'s names (its mod's level.txt: name, region, name.<lang>, region.<lang>)
const Names& names(int slot) {
    Names& n = g_names[(slot - kFirstLevelSlot) & 15];
    if (n.read) return n;
    n.read = true;
    const ModInfo* m = slot_mod(slot);
    std::string name = m != nullptr ? m->name : "Custom Level", region;
    std::ifstream f(m != nullptr ? (m->dir / "level" / "level.txt") : std::filesystem::path());
    std::string line;
    while (f && std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        std::istringstream in(line);
        std::string key, rest;
        in >> key;
        std::getline(in >> std::ws, rest);
        auto lang_of = [&key](const char* what) {
            const size_t k = std::strlen(what);
            if (key.compare(0, k, what) != 0 || key.size() != k + 3 || key[k] != '.') return -1;
            for (int l = 2; l < 6; ++l)
                if (key.compare(k + 1, 2, kLangs[l]) == 0) return l;
            return -1;
        };
        if (key == "name") name = rest;
        else if (key == "region") region = rest;
        else if (const int l = lang_of("name"); l >= 0) n.names[l] = rest;
        else if (const int l2 = lang_of("region"); l2 >= 0) n.regions[l2] = rest;
    }
    for (int l = 0; l < 6; ++l) {
        if (n.names[l].empty()) n.names[l] = name;
        if (n.regions[l].empty()) n.regions[l] = region;
    }
    return n;
}

// orig: (none) — enhancement: the save's per-level list's records of level `base` (as the overlay now has them)
std::vector<Record> base_records(int base) {
    std::vector<Record> r;
    uint32_t at = 0;
    for (uint32_t e = LEVEL_LIST; lw(e) != 0 && r.size() < 64; e += 0x10) {
        const uint32_t size = lwu(e + 4);
        if (size == 0 || size > 0x10000) break;  // (not the list: nothing swapped)
        if (lwu(e) == 0x0013dd58u) continue;  // (the levels' visited / done flags: the base level's stays, the story's tests read it)
        r.push_back(Record{e, lwu(e) + static_cast<uint32_t>(base) * size, size, at});
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

// orig: (none) — enhancement: the current slot's records put in, its base level's kept
void swap_in(int slot) {
    const ModInfo* m = slot_mod(slot);
    if (m == nullptr) return;
    g_s.records = base_records(m->level_base);
    if (g_s.records.empty()) return;
    g_s.base = records_read(g_s.records);
    const std::vector<uint8_t>& mine = g_s.progress[m->level_id].records;
    records_write(g_s.records, mine);
    g_s.swapped = true;
    if (trace())
        std::printf("[levels] level records swapped in: %zu records, %u bytes (the level's: %s %08x; level %d's kept %08x)\n", g_s.records.size(),
                    records_bytes(g_s.records), mine.empty() ? "new" : "kept", bytes_sum(mine), m->level_base, bytes_sum(g_s.base));
}

// orig: (none) — enhancement: the slot's records kept, its base level's put back
void swap_out(const std::string& id) {
    g_s.progress[id].records = records_read(g_s.records);
    records_write(g_s.records, g_s.base);
    g_s.base.clear();
    g_s.swapped = false;
    if (trace()) std::printf("[levels] level records swapped back (the base level's in EE memory %08x)\n", bytes_sum(records_read(g_s.records)));
}

std::string swapped_id;  // the level.id whose records are in EE memory (g_s.swapped)

// orig: (none) — enhancement: the slot's music in the level's header (its music table, 15 words at +0x148: the sectors of
// the tracks the level's code plays, an intro then its loop): the base level's (as read), none, another level's (its header
// read from the disc), or the build's own files (in the slot's window after its data and gameplay files)
void music_set(int slot) {
    const ModInfo* m = slot_mod(slot);
    if (m == nullptr || m->level_music == kLevelMusicBase) return;
    constexpr uint32_t MUSIC = HEADER + 0x148, WORDS = 15;
    uint32_t words[WORDS] = {};
    if (m->level_music == kLevelMusicOwn) {
        uint32_t lba = 0, n = 0;
        const size_t k = m->level_music_files.size();  // (the intro, then the loop: the code plays a track, then queues the next)
        for (size_t i = 0; i < k && i < 2; ++i)
            if (disc_level_window_file(slot, 2 + i, lba, n)) words[i] = lba;
        if (k == 1) words[1] = words[0];  // (no intro: the loop twice)
    } else if (m->level_music >= 0 && m->level_music < 19) {
        std::vector<uint8_t> h(5 * 2048);
        if (!disc_read(lwu(0x0013a448 + static_cast<uint32_t>(m->level_music) * 8), 5, h.data())) {
            std::fprintf(stderr, "[levels] level %d's header cannot be read: the base level's music\n", m->level_music);
            return;
        }
        std::memcpy(words, h.data() + 0x148, sizeof words);
    }
    for (uint32_t i = 0; i < WORDS; ++i) sw(MUSIC + i * 4, words[i]);
    std::printf("[levels] level slot %d's music: %s (sectors %#x, %#x)\n", slot,
                m->level_music == kLevelMusicOwn ? "the mod's" : m->level_music == kLevelMusicNone ? "none" : ("level " + std::to_string(m->level_music) + "'s").c_str(),
                words[0], words[1]);
}

// orig: (none) — enhancement: slot `slot` chosen: its base level's next load is it
void choose(int slot) {
    const ModInfo* m = slot_mod(slot);
    if (m == nullptr) return;
    g_s.pending = slot;
    disc_level_redirect(m->level_base, slot);
    std::printf("[levels] \"%s\" chosen (level slot %d, on level %d)\n", names(slot).names[0].c_str(), slot, m->level_base);
}

}  // namespace

// orig: (none) — enhancement (level_slots.h)
bool level_slots_on() {
    for (const ModInfo& m : mods_list())  // (empty in a vanilla run: no mod looked at)
        if (m.active && m.level_slot >= 0) return true;
    return false;
}

// orig: (none) — enhancement (level_slots.h)
int level_slot() {
    const int lv = lw(LEVEL);
    if (g_s.current >= 0 && lv == base_of(g_s.current)) return g_s.current;
    return lv;
}

// orig: (none) — enhancement (level_slots.h)
int level_slot_of(int level) {
    const int c = g_current.load(std::memory_order_relaxed);
    return c >= 0 && level == base_of(c) ? c : level;
}

// orig: (none) — enhancement (level_slots.h)
int level_slot_current() { return g_s.current >= 0 && lw(LEVEL) == base_of(g_s.current) ? g_s.current : -1; }

// orig: (none) — enhancement (level_slots.h)
int level_slot_base(int slot) { return base_of(slot); }

// orig: (none) — enhancement (level_slots.h)
std::string level_slot_build_dir(int slot) {
    const ModInfo* m = slot_mod(slot);
    return m != nullptr ? mods_level_build_dir(m->folder).string() : std::string();
}

// orig: (none) — enhancement (level_slots.h)
std::string level_slot_mod_dir(int slot) {
    const ModInfo* m = slot_mod(slot);
    return m != nullptr ? m->dir.string() : std::string();
}

// orig: (none) — enhancement (level_slots.h)
uint32_t level_slots_list_rows(uint32_t list, uint32_t n) {
    if (!rows_on()) return n;
    g_s.rows.clear();
    g_s.row_sel = -1;
    for (uint32_t i = 0; i < n; ++i) g_s.rows.push_back({lw(UNLOCKED + i * 4), -1});
    // the planet prompt: the level flown from first (the save's unlocked planets, as the game listed them, after it)
    if (g_s.prompt_open) {
        const int32_t here = lw(LEVEL);
        for (uint32_t i = 1; i < n; ++i) {
            if (g_s.rows[i].planet != here) continue;
            uint8_t e[0xc];
            for (uint32_t b = 0; b < 0xc; ++b) e[b] = static_cast<uint8_t>(lbu(list + i * 0xc + b));
            for (uint32_t k = i; k > 0; --k)
                for (uint32_t b = 0; b < 0xc; ++b) sb(list + k * 0xc + b, lbu(list + (k - 1) * 0xc + b));
            for (uint32_t b = 0; b < 0xc; ++b) sb(list + b, e[b]);
            std::rotate(g_s.rows.begin(), g_s.rows.begin() + i, g_s.rows.begin() + i + 1);
            break;
        }
    }
    // a row per level mod on, in load order, after the unlocked planets (as the game's rows: the region, the name under it)
    const std::vector<const ModInfo*> ls = level_slots_on() ? mods_levels() : std::vector<const ModInfo*>();
    for (size_t k = 0; k < ls.size(); ++k) {
        if (n >= ROWS_MAX) {
            static bool said = false;
            if (!said) std::fprintf(stderr, "[levels] the ship's planet list is full (%u rows): \"%s\" and the levels after it have no row\n", ROWS_MAX,
                                    ls[k]->level_id.c_str());
            said = true;
            break;
        }
        const uint32_t e = list + n * 0xc;
        sh(e, static_cast<uint32_t>(TEXT_REGION + 2 * static_cast<int32_t>(k)));
        sh(e + 2, 1);
        sw(e + 4, 0);
        sh(e + 8, static_cast<uint32_t>(TEXT_NAME + 2 * static_cast<int32_t>(k)));
        g_s.rows.push_back({ls[k]->level_base, ls[k]->level_slot});
        ++n;
    }
    if (trace()) {
        std::string t;
        for (const State::Row& r : g_s.rows) t += " " + std::to_string(r.planet) + (r.slot >= 0 ? "/" + std::to_string(r.slot) : "");
        std::printf("[levels] the ship's planet list:%s%s\n", t.c_str(), g_s.prompt_open ? " (the planet prompt)" : "");
    }
    return n;
}

// orig: (none) — enhancement (level_slots.h)
int32_t level_slots_list_focus(uint32_t n) {
    if (!rows_on() || n == 0) return -1;
    if (g_s.prompt_open) {  // (the prompt: the level flown from, the list's first)
        sw(MENU_PLANET, static_cast<uint32_t>(g_s.rows.empty() ? lw(LEVEL) : g_s.rows[0].planet));
        return 0;
    }
    const int cur = level_slot_current();
    for (size_t i = 0; cur >= 0 && i < g_s.rows.size() && i < n; ++i)
        if (g_s.rows[i].slot == cur) {
            g_s.row_sel = cur;
            sw(MENU_PLANET, static_cast<uint32_t>(g_s.rows[i].planet));
            return static_cast<int32_t>(i);
        }
    return -1;
}

// orig: (none) — enhancement (level_slots.h)
void level_slots_list_select(uint32_t items, int32_t sel) {
    if (!rows_on() || items != OV(0x001ba3b8u) || sel < 0 || static_cast<size_t>(sel) >= g_s.rows.size()) return;
    const State::Row& r = g_s.rows[static_cast<size_t>(sel)];
    sw(MENU_PLANET, static_cast<uint32_t>(r.planet));  // (ours: the game's table has no entry for them; the prompt's order)
    if (trace() && g_s.row_sel != r.slot) std::printf("[levels] the ship's list: row %d (planet %d, slot %d)\n", sel, r.planet, r.slot);
    g_s.row_sel = r.slot;
}

// orig: (none) — enhancement (level_slots.h)
void level_slots_list_back() {
    if (!rows_on()) return;
    g_s.row_sel = level_slot_current();  // (the map's planet is the current level's: on a slot's level, that slot)
}

// orig: (none) — enhancement (level_slots.h)
bool level_slots_travel(int32_t planet) {
    if (!rows_on()) return false;
    const bool prompt = g_s.prompt_open;
    g_s.prompt_open = false;
    const int slot = g_s.row_sel;
    g_s.row_sel = -1;
    if (slot >= 0 && planet == base_of(slot)) {
        choose(slot);
        return lw(LEVEL) == planet;
    }
    g_s.pending = -1;  // (another planet: the redirect stays until that level's header is read)
    if (prompt && planet == lw(LEVEL)) {
        // the planet prompt left for the level it was opened on (chosen, or the map closed: triangle, start): Ratchet is not
        // in his ship there, so not the ship's landing on the level shown but the level loaded again, as from elsewhere
        std::printf("[levels] the planet prompt: level %d chosen (loaded again)\n", planet);
        return true;
    }
    return false;
}

// orig: (none) — enhancement (level_slots.h)
int32_t level_slots_choose_id(const std::string& id) {
    if (!level_slots_on()) return -1;
    const ModInfo* m = mods_level_by_id(id);
    if (m == nullptr) {
        std::fprintf(stderr, "[levels] RAC1_GOTO: no level mod on has the id \"%s\"\n", id.c_str());
        return -1;
    }
    choose(m->level_slot);
    return m->level_base;
}

// orig: (none) — enhancement (level_slots.h)
void level_slots_header_read(int32_t level) {
    if (!level_slots_on()) return;
    const int slot = g_s.pending;
    if (slot >= 0 && level == base_of(slot)) {
        uint32_t dl = 0, dn = 0, gl = 0, gn = 0;
        if (!disc_level_window_ranges(slot, dl, dn, gl, gn)) {
            const ModInfo* m = slot_mod(slot);
            const std::string b = level_slot_build_dir(slot);
            std::vector<std::string> files = {b + "/data.wad", b + "/gameplay.bin"};
            if (m != nullptr) files.insert(files.end(), m->level_music_files.begin(), m->level_music_files.end());
            if (m == nullptr || !disc_level_window_set(slot, files) || !disc_level_window_ranges(slot, dl, dn, gl, gn)) {
                std::fprintf(stderr, "[levels] level slot %d's build cannot be read: level %d is the game's\n", slot, level);
                g_s.pending = -1;
                disc_level_redirect(level, -1);
                g_s.current = -1;
                g_current.store(-1);
                return;
            }
        }
        disc_level_redirect(level, slot);
        sw(HEADER + 0x08, dl);
        sw(HEADER + 0x0c, dn);
        sw(HEADER + 0x10, gl);
        sw(HEADER + 0x14, gn);
        sw(HEADER + 0x18, gl);
        sw(HEADER + 0x1c, gn);
        sw(HEADER + 0x20, 0);  // (no occlusion)
        sw(HEADER + 0x24, 0);
        music_set(slot);
        if (g_s.current != slot) g_s.from = g_s.pending_from >= 0 ? g_s.pending_from : g_s.last_real;
        g_s.pending_from = -1;
        g_s.current = slot;
        g_current.store(slot);
        std::printf("[levels] level %d's header: level slot %d's (\"%s\", flown from level %d)\n", level, slot, names(slot).names[0].c_str(), g_s.from);
        return;
    }
    // one of the game's own levels
    g_s.pending = -1;
    g_s.current = -1;
    g_current.store(-1);
    g_s.last_real = level;
    for (int l = 0; l < 19; ++l)
        if (disc_level_redirected(l) >= 0) disc_level_redirect(l, -1);
}

// orig: (none) — enhancement (level_slots.h)
void level_slots_level_start(int32_t level) {
    if (!level_slots_on()) return;
    const int slot = g_s.current >= 0 && level == base_of(g_s.current) ? g_s.current : -1;
    if (g_s.swapped && (slot < 0 || slot_mod(slot)->level_id != swapped_id)) swap_out(swapped_id);
    if (slot >= 0 && !g_s.swapped) {
        swap_in(slot);
        if (g_s.swapped) swapped_id = slot_mod(slot)->level_id;
    }
}

// orig: (none) — enhancement (level_slots.h)
bool level_slots_skip_story() { return level_slots_on() && (g_s.pending >= 0 || level_slot_current() >= 0); }

// orig: (none) — enhancement (level_slots.h)
bool level_slots_story_to(int32_t next) { return level_slots_on() && g_s.pending >= 0 && next == base_of(g_s.pending); }

// orig: (none) — enhancement (level_slots.h)
bool level_slots_planet_arrival() { return level_slots_on() && g_s.pending >= 0 && lw(LEVEL) == base_of(g_s.pending); }

// orig: (none) — enhancement (level_slots.h)
bool level_slots_keep_base_progress() { return level_slots_on() && level_slot_current() >= 0; }

// orig: (none) — testing (level_slots.h): RAC1_LEVEL_TRACE: where the landing leaves the scene's Ratchet and where the hero
// is put, both in the ship's frame (x forward along its yaw, y to its left, z up): `hero auto`'s offset (level_compose.cpp)
void level_slots_landing_end(uint32_t hero) {
    if (!trace() || hero == 0) return;
    const uint32_t ship = lwu(0x0013e030u);
    if (ship == 0) return;
    const float yaw = u2f(lwu(ship + 0x48)), c = std::cos(yaw), s = std::sin(yaw);
    auto rel = [&](uint32_t at, float out[3]) {
        const float dx = u2f(lwu(at)) - u2f(lwu(ship + 0x10)), dy = u2f(lwu(at + 4)) - u2f(lwu(ship + 0x14));
        out[0] = c * dx + s * dy;
        out[1] = -s * dx + c * dy;
        out[2] = u2f(lwu(at + 8)) - u2f(lwu(ship + 0x18));
    };
    float a[3], b[3];
    rel(hero + 0x10, a);
    rel(0x0013e090u, b);
    std::printf("[levels] the landing's end (level %d, slot %d, ship variant %d): the scene's Ratchet at (%.3f %.3f %.3f) of the ship, turned %.3f; "
                "the hero put at (%.3f %.3f %.3f), turned %.3f (the ship's yaw %.3f)\n",
                lw(LEVEL), level_slot_current(), static_cast<int>(lh(0x0013e056u)), a[0], a[1], a[2], yaw_wrap(u2f(lwu(hero + 0x48)) - yaw), b[0], b[1], b[2],
                yaw_wrap(u2f(lwu(0x0013e0a8u)) - yaw), yaw);
}

namespace {

// orig: (none) — enhancement: a file of slot `slot`'s build (picture.img, banner.img, map.img: images in the layout
// tex_image_upload reads, made by the composer), empty when it has none
std::vector<uint8_t> build_image(int slot, const std::string& name) {
    std::vector<uint8_t> d;
    const std::string dir = level_slot_build_dir(slot);
    if (dir.empty()) return d;
    std::ifstream f(dir + "/" + name, std::ios::binary);
    if (f) d.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return d;
}

// orig: (none) — enhancement: an image (`img`, the layout of tex_image_upload: +8 / +0xc its size, +0x10 PSMT8, the palette
// at +0x20, the pixels at +0x420) written over the image at `at` in EE memory when they have the same size and format
bool image_over(uint32_t at, const std::vector<uint8_t>& img, const char* what) {
    auto u32 = [&img](size_t o) { return o + 4 <= img.size() ? static_cast<uint32_t>(img[o] | img[o + 1] << 8 | img[o + 2] << 16 | static_cast<uint32_t>(img[o + 3]) << 24) : 0u; };
    const uint32_t w = u32(8), h = u32(0xc);
    if (img.empty()) return false;
    if (u32(0x10) != 0x13 || w != lwu(at + 8) || h != lwu(at + 0xc) || lwu(at + 0x10) != 0x13 || img.size() != 0x420u + w * h) {
        std::fprintf(stderr, "[levels] the level's %s is not the game's size (%u x %u, 8-bit: %u x %u): not used\n", what, w, h, lwu(at + 8), lwu(at + 0xc));
        return false;
    }
    for (size_t i = 0; i < img.size(); ++i) sb(at + static_cast<uint32_t>(i), img[i]);
    return true;
}

}  // namespace

// orig: (none) — enhancement (level_slots.h)
void level_slots_loading_pictures(uint32_t pics, int32_t lang) {
    if (!level_slots_planet_arrival()) return;
    const int slot = g_s.pending, base = lw(LEVEL);
    const bool planet = image_over(pics + lwu(pics + 4 + static_cast<uint32_t>(base) * 4), build_image(slot, "picture.img"), "picture");
    // the name: the language's banner (banner.<lang>.img), else banner.img (the mod's, or the one the composer made from its name)
    const uint32_t at = pics + lwu(pics + 0x50 + static_cast<uint32_t>(lang * 0x13 + base) * 4);
    uint32_t tl = lwu(0x0015ed88);
    if (tl >= 6) tl = 0;
    std::vector<uint8_t> b = build_image(slot, std::string("banner.") + kLangs[tl] + ".img");
    if (b.empty()) b = build_image(slot, "banner.img");
    const bool banner = image_over(at, b, "banner");
    std::printf("[levels] the loading screen: level slot %d's planet picture %s, its name %s\n", slot, planet ? "the mod's" : "the base level's",
                banner ? "the mod's" : "the base level's");
}

// orig: (none) — enhancement (level_slots.h)
bool level_slots_map_row() {
    if (!rows_on() || g_s.row_sel < 0) return false;
    const uint32_t ms = lwu(MENU_STATE);
    return (ms == 0xf || ms == 0x10) && lw(MENU_PLANET) == base_of(g_s.row_sel);
}

namespace {
bool g_map_ours = false;           // the map's picture widget shows one of our rows (its picture, or none)
int g_map_slot = -1;               // ... that slot's (its map.img at the map's +0x258, or none): -1 when gone
int g_planet_slot = -1;            // the planet page's planet is that slot's (its picture.img at the map's +0x258, or none)
bool g_planet_shown = false;
}  // namespace

// orig: (none) — enhancement (level_slots.h)
bool level_slots_map_picture(uint32_t w) {
    if (!level_slots_map_row()) {
        if (g_map_ours && lw(w + 0x50) >= 0x100) sw(w + 0x50, 0xffffffffu);  // (ours shown: the game's read again)
        g_map_ours = false;
        g_map_slot = g_planet_slot = -1;  // (the game's pictures and planets go to the map's +0x258 now)
        return false;
    }
    const int slot = g_s.row_sel;
    const int32_t key = 0x100 + slot;  // (not a planet: the game's picture is read again when it is one)
    const int32_t st = lw(w + 0x44);
    if ((lw(w + 0x50) == key && g_map_slot == slot) || (st != 0 && st != 2)) return true;
    const uint32_t buf = lwu(w + 0x48);
    if (buf == 0 || lh(0x001516d8) != 0) return true;
    const std::vector<uint8_t> img = build_image(slot, "map.img");
    auto u32 = [&img](size_t o) { return o + 4 <= img.size() ? static_cast<uint32_t>(img[o] | img[o + 1] << 8 | img[o + 2] << 16 | static_cast<uint32_t>(img[o + 3]) << 24) : 0u; };
    const uint32_t iw = u32(8), ih = u32(0xc);
    const bool fits = !img.empty() && u32(0x10) == 0x13 && iw == 512 && ih == 128 && img.size() == 0x420u + iw * ih &&
                      static_cast<int32_t>(img.size()) <= menu_buf_size(buf);
    sw(w + 0x50, static_cast<uint32_t>(key));
    if (!fits) {  // (none: no picture, not the base level's)
        if (!img.empty()) std::fprintf(stderr, "[levels] level slot %d's map.img is not the map's size (512 x 128, 8-bit): not shown\n", slot);
        sw(w + 0x44, 0);
        g_map_ours = true;
        g_map_slot = slot;
        return true;
    }
    for (size_t i = 0; i < img.size(); ++i) sb(buf + static_cast<uint32_t>(i), img[i]);
    constexpr OvConst<0x001841f0u> MAP{};
    const uint64_t t = image_upload_at(highest_bit_index(static_cast<int32_t>(iw)), highest_bit_index(static_cast<int32_t>(ih)), buf + 0x20, buf + 0x420, lw(MAP + 0x244),
                                       lw(MAP + 0x248));
    sd(MAP + 0x258, t);
    texture_uploads();
    sw(w + 0x44, 2);
    g_map_ours = true;
    g_map_slot = slot;
    g_planet_slot = -1;
    if (trace()) std::printf("[levels] the ship's map: level slot %d's picture (%u x %u)\n", slot, iw, ih);
    return true;
}

// orig: (none) — enhancement (level_slots.h)
void level_slots_map_planet_upload() {
    constexpr OvConst<0x001841f0u> MAP{};
    if (lwu(MENU_STATE) != 0x10 || !level_slots_map_row()) return;
    const int slot = g_s.row_sel;
    if (g_planet_slot == slot && lw(MAP + 0x228) == -1) return;  // (+0x228 kept -1: no planet's tables, the game's read again after)
    // the planet page's planet: the level's picture (as the planets' surface, 128 x 128 at the map's +0x258)
    const std::vector<uint8_t> img = build_image(slot, "picture.img");
    const uint32_t tmp = img.size() == 0x4420 ? menu_buf_alloc(0) : 0;
    if (tmp == 0 || menu_buf_size(tmp) < static_cast<int32_t>(img.size())) {
        if (tmp != 0) menu_buf_free(tmp);
        sw(MAP + 0x228, 0xffffffffu);
        g_planet_slot = slot;  // (none: nothing drawn; tried once until another row)
        g_planet_shown = false;
        return;
    }
    for (size_t i = 0; i < img.size(); ++i) sb(tmp + static_cast<uint32_t>(i), img[i]);
    sd(MAP + 0x258, image_upload_at(7, 7, tmp + 0x20, tmp + 0x420, lw(MAP + 0x244), lw(MAP + 0x248)));
    texture_uploads();
    menu_buf_free(tmp);
    sw(MAP + 0x228, 0xffffffffu);
    g_planet_slot = slot;
    g_planet_shown = true;
    g_map_slot = -1;  // (the galaxy page's picture is gone from there)
    if (trace()) std::printf("[levels] the ship's map's planet page: level slot %d's picture\n", slot);
}

// orig: (none) — enhancement (level_slots.h)
bool level_slots_map_planet_shown() {
    constexpr OvConst<0x001841f0u> MAP{};
    return level_slots_map_row() && g_s.row_sel >= 0 && g_planet_slot == g_s.row_sel && g_planet_shown && lw(MAP + 0x228) == -1;
}

// orig: (none) — enhancement (level_slots.h)
uint32_t level_slots_text(int32_t id) {
    if (!level_slots_on() || id < 0x100) return 0;
    // on the ship's map, while one of our rows is selected the planet it shows is that level's base level: its name and
    // region (the planet page's labels) are the slot's level's
    const uint32_t ms = lwu(MENU_STATE);
    if ((ms == 0xf || ms == 0x10) && g_s.row_sel >= 0 && lw(MENU_PLANET) == base_of(g_s.row_sel)) {
        const uint32_t pt = PLANET_TEXTS + static_cast<uint32_t>(base_of(g_s.row_sel)) * 0xc;
        const int32_t k = slot_index(g_s.row_sel);
        if (k >= 0 && id == static_cast<int32_t>(lhu(pt))) id = TEXT_REGION + 2 * k;
        else if (k >= 0 && id == static_cast<int32_t>(lhu(pt + 4))) id = TEXT_NAME + 2 * k;
    }
    if (id < TEXT_NAME || id >= TEXT_END) return 0;
    const std::vector<const ModInfo*> ls = mods_levels();
    const size_t k = static_cast<size_t>(id - TEXT_NAME) / 2;
    if (k >= ls.size()) return 0;
    uint32_t lang = lwu(0x0015ed88);
    if (lang >= 6) lang = 0;
    const bool name = (id - TEXT_NAME) % 2 == 0;
    const Names& n = names(ls[k]->level_slot);
    const std::string& s = name ? n.names[lang] : n.regions[lang];
    const uint32_t base = TEXTS + static_cast<uint32_t>(k) * (NAME_SIZE + REGION_SIZE);
    const uint32_t at = name ? base : base + NAME_SIZE, size = name ? NAME_SIZE : REGION_SIZE;
    for (uint32_t i = 0; i < size; ++i) {
        const char c = i < s.size() ? s[i] : 0;
        sb(at + i, i + 1 < size && c >= 0x20 && c < 0x7f ? static_cast<uint8_t>(c) : (i < s.size() && i + 1 < size ? '?' : 0u));
    }
    return at;
}

// orig: (none) — enhancement (level_slots.h)
bool level_slots_save_record(uint32_t list, int32_t index, uint32_t e, uint32_t out) {
    if (!g_s.swapped || list != LEVEL_LIST || g_s.current < 0 || index != base_of(g_s.current)) return false;
    for (const Record& x : g_s.records) {
        if (x.entry != e || x.size != lwu(e + 4)) continue;
        for (uint32_t i = 0; i < x.size; ++i) sb(out + i, g_s.base[x.at + i]);
        return true;
    }
    return false;
}

// orig: (none) — enhancement (level_slots.h)
void level_slots_block_read(uint32_t list, int32_t index) {
    if (!level_slots_on() || list != LEVEL_LIST) return;
    bool base = false;  // (the block of a level mod's base level)
    for (const ModInfo* m : mods_levels()) base = base || index == m->level_base;
    if (!base) return;
    // EE memory has the save's (the game's) records: nothing to swap back; the levels' state is the save's (its .pc, next)
    g_s.swapped = false;
    g_s.base.clear();
    g_s.progress.clear();
}

// orig: (none) — enhancement (level_slots.h)
void level_slots_new_game() {
    g_s.pending = -1;
    g_s.carried.clear();
    g_s.saved_on.clear();
    g_s.prompt = false;
    g_s.prompt_open = false;
    g_s.row_sel = -1;
    if (!level_slots_on()) return;
    g_s.progress.clear();
    for (int l = 0; l < 19; ++l)
        if (disc_level_redirected(l) >= 0) disc_level_redirect(l, -1);
}

// orig: (none) — enhancement (level_slots.h)
uint32_t level_slots_pc_items(std::vector<uint8_t>& d) {
    uint32_t count = 0;
    auto put_item = [&](const std::vector<uint8_t>& item) {
        if (item.size() > 0xffff) return;
        d.push_back(static_cast<uint8_t>(PC_ITEM));
        d.push_back(static_cast<uint8_t>(PC_ITEM >> 8));
        d.push_back(static_cast<uint8_t>(item.size()));
        d.push_back(static_cast<uint8_t>(item.size() >> 8));
        d.insert(d.end(), item.begin(), item.end());
        ++count;
    };
    for (const PcItem& c : g_s.carried) {  // (the levels whose mod is not on: as they were read, except that this save is not
        std::vector<uint8_t> raw = c.raw;  // made on them: their "made on it" flag cleared, a version 1 item's word +4)
        if (!c.id.empty() && raw.size() >= 8) std::fill(raw.begin() + 4, raw.begin() + 8, uint8_t{0});
        put_item(raw);
    }
    if (!level_slots_on()) return count;
    const int cur = level_slot_current();
    for (const ModInfo* m : mods_levels()) {
        // its records: in EE memory while it is played, else the host's copy (none yet and not on it: no item)
        const bool on = cur == m->level_slot;
        std::vector<uint8_t> rec = g_s.swapped && swapped_id == m->level_id ? records_read(g_s.records) : g_s.progress[m->level_id].records;
        if (rec.empty() && !on) continue;
        std::vector<uint8_t> item;
        auto u32 = [&item](uint32_t v) {
            for (int i = 0; i < 4; ++i) item.push_back(static_cast<uint8_t>(v >> (8 * i)));
        };
        u32(PC_VERSION);
        u32(on ? 1u : 0u);
        u32(static_cast<uint32_t>(on ? g_s.from : -1));
        u32(static_cast<uint32_t>(m->level_id.size()));
        item.insert(item.end(), m->level_id.begin(), m->level_id.end());
        u32(static_cast<uint32_t>(rec.size()));
        item.insert(item.end(), rec.begin(), rec.end());
        put_item(item);
        if (trace()) std::printf("[levels] .pc item \"%s\": on %d, from %d, %zu record bytes\n", m->level_id.c_str(), on ? 1 : 0, on ? g_s.from : -1, rec.size());
    }
    return count;
}

// orig: (none) — enhancement (level_slots.h)
void level_slots_pc_reset() {
    g_s.carried.clear();
    g_s.saved_on.clear();
    g_s.saved_from = -1;
    g_s.prompt = false;
}

// orig: (none) — enhancement (level_slots.h)
bool level_slots_pc_item(uint32_t id, const uint8_t* p, uint32_t size) {
    if (id != PC_ITEM) return false;
    auto u32 = [p, size](uint32_t at) { return at + 4 <= size ? static_cast<uint32_t>(p[at] | p[at + 1] << 8 | p[at + 2] << 16 | static_cast<uint32_t>(p[at + 3]) << 24) : 0u; };
    PcItem it;
    it.raw.assign(p, p + size);
    if (u32(0) != PC_VERSION) {  // (a newer version's: kept as it is)
        g_s.carried.push_back(std::move(it));
        return true;
    }
    it.on = u32(4) != 0;
    it.from = static_cast<int32_t>(u32(8));
    const uint32_t nlen = u32(12);
    if (16 + nlen + 4 > size) return true;
    it.id.assign(reinterpret_cast<const char*>(p + 16), nlen);
    const uint32_t rlen = u32(16 + nlen);
    if (20 + nlen + rlen > size) return true;
    it.records.assign(p + 20 + nlen, p + 20 + nlen + rlen);
    if (it.on) {
        g_s.saved_on = it.id;
        g_s.saved_from = it.from;
    }
    if (trace()) std::printf("[levels] .pc item read: \"%s\", on %d, from %d, %u record bytes\n", it.id.c_str(), it.on ? 1 : 0, it.from, rlen);
    if (level_slots_on() && mods_level_by_id(it.id) != nullptr) {
        g_s.progress[it.id].records = std::move(it.records);
        return true;
    }
    g_s.carried.push_back(std::move(it));  // (its mod is off or missing: written back as it was)
    return true;
}

// orig: (none) — enhancement (level_slots.h)
void level_slots_save_loaded() {
    if (g_s.saved_on.empty()) return;
    const ModInfo* m = level_slots_on() ? mods_level_by_id(g_s.saved_on) : nullptr;
    if (m != nullptr) {  // (the save's level is its base level, as the game wrote it: back on the slot's level)
        choose(m->level_slot);
        g_s.pending_from = g_s.saved_from;
        g_s.current = -1;  // (the level is loaded again: its header read sets it)
        g_current.store(-1);
        std::printf("[levels] the save was made on \"%s\": back there\n", names(m->level_slot).names[0].c_str());
        return;
    }
    // its mod is off or missing: the level flown from, and the planet prompt there
    const int32_t to = g_s.saved_from >= 0 && g_s.saved_from < 19 ? g_s.saved_from : 1;
    std::printf("[levels] the save was made on the level \"%s\", whose mod is off or missing: level %d (flown from) instead, the planet map opened there\n",
                g_s.saved_on.c_str(), to);
    sw(LEVEL, static_cast<uint32_t>(to));
    g_s.prompt = true;
    g_s.prompt_frames = 0;
    g_s.prompt_name = g_s.saved_on;
}

// orig: (none) — enhancement (level_slots.h)
void level_slots_frame() {
    if (!g_s.prompt) return;
    // the planet prompt: the ship's planet map (as the ship's take-off opens it, arrival.cpp), once the level is under way
    if (lw(OV(0x0015f5c4)) != 0) return;  // (a menu or a screen already)
    if (++g_s.prompt_frames < 30) return;
    g_s.prompt = false;
    g_s.prompt_open = true;  // (its list: the level flown from first; leaving it for that level loads it again)
    menu_open(0);
    sw(MENU_STATE, 0xe);
    sw(OV(0x0015f5d8), 1);
    std::printf("[levels] the planet prompt: the ship's planet map opened on level %d\n", lw(LEVEL));
}

// orig: (none) — testing: RAC1_LEVEL_SAVE_IMAGE=<frame>[,<frame>..]:<file prefix>: at those game frames the save's blocks as
// the card would get them (save_block_write: the game's block, then the 20 levels', each block written in the save
// buffer 0x1506d0 and copied out) to <prefix><frame>.bin, and our .pc items to <prefix><frame>.pc: a save made on a
// slot's level must give the same level blocks as one made elsewhere
void level_slots_test_frame(uint64_t frame) {
    // RAC1_LEVEL_MAP_AT=<frame>: the ship's planet map opened at that game frame, as a take-off opens it (its list, our rows)
    static const long long map_at = std::getenv("RAC1_LEVEL_MAP_AT") != nullptr ? std::atoll(std::getenv("RAC1_LEVEL_MAP_AT")) : -1;
    if (map_at >= 0 && frame == static_cast<uint64_t>(map_at) && lw(OV(0x0015f5c4)) == 0) {
        menu_open(0);
        sw(MENU_STATE, 0xe);
        sw(OV(0x0015f5d8), 1);
        std::printf("[levels] RAC1_LEVEL_MAP_AT: the ship's planet map opened at frame %llu\n", static_cast<unsigned long long>(frame));
    }
    static const char* spec = std::getenv("RAC1_LEVEL_SAVE_IMAGE");
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
    level_slots_pc_items(pc);
    mod_code_pc_items(pc);  // (the mods' code's items too, src/host/mod_code.h)
    if (std::FILE* f = std::fopen((base + ".pc").c_str(), "wb")) {
        std::fwrite(pc.data(), 1, pc.size(), f);
        std::fclose(f);
    }
    std::printf("[levels] RAC1_LEVEL_SAVE_IMAGE: frame %llu: game block %zu bytes (%08x), level blocks %zu bytes (%08x), .pc items %zu bytes -> %s.*\n",
                static_cast<unsigned long long>(frame), game, bytes_sum(std::vector<uint8_t>(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(game))), out.size() - game,
                bytes_sum(std::vector<uint8_t>(out.begin() + static_cast<std::ptrdiff_t>(game), out.end())), pc.size(), base.c_str());
}

}  // namespace rac1
