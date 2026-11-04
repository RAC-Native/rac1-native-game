// See custom_planet.h ("Sound and music"). The custom planet's sounds and music from Ratchet & Clank 2's disc, a data disc
// (the setup links its image: runtime/kernel/disc.h's windows), read where they are: nothing extracted.
#include <host/custom_planet.h>

#include <game/enhance/voice_language.h>
#include <game/lib/snd989.h>
#include <game/ovl/music_streams.h>
#include <game/ovl/ovl_common.h>
#include <runtime/kernel/disc.h>
#include <runtime/kernel/ee_memory.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace rac1 {

namespace {

constexpr uint32_t MUSIC = 0x0013a628;  // the level header's music sectors (a word a track: music_streams.cpp)

// One of Ratchet & Clank 2's discs the setup listed (assets/data-discs.txt, region "RAC2-..."), with the two files the
// arena needs found in its ISO 9660 directory
struct Rac2Disc {
    std::string region;  // "RAC2-NTSC-U", "RAC2-PAL", "RAC2-NTSC-J"
    uint32_t base = 0;   // its sector 0 in the disc layer's sectors
    uint32_t level_lba = 0, level_bytes = 0;  // G/LEVELn.WAD
    uint32_t audio_lba = 0, audio_bytes = 0;  // G/AUDIOn.WAD
    std::vector<uint32_t> audio_table;        // AUDIOn.WAD's table: {sector, bytes} per entry, from the file's start
    bool ok = false;
};

// What planet.txt says of its sounds (our own keys; the planet's folder: custom_level_dir()):
//   rac2_level <n>           the RAC2 level whose files it uses (2: Maktar)
//   rac2_banks <c> [<c>..]   the level WAD's header words (byte offsets) of the sound banks to load: 0x18 the level's,
//                            0x48 / 0x50 its chunks'
//   music <intro> <loop>     AUDIOn.WAD's entries of the planet's music, left channels (the right ones follow: n + 1)
//   music_challenge <i> <l>  the same, while a challenge of its arena runs (custom_arena.h)
//   music_boss <i> <l>       the same, while a boss fights
//   announce <event> <e>..   the announcer's lines for an event: each the first entry of a group of five (slot 0 English, or
//                            Japanese on the Japanese disc; 1 French, 2 German, 3 Spanish, 4 Italian)
struct Sounds {
    bool read = false;
    int level = -1;
    std::vector<uint32_t> banks;
    int music[3][2] = {{-1, -1}, {-1, -1}, {-1, -1}};  // (the planet's, a challenge's, a boss's: intro, loop)
    std::vector<std::pair<std::string, std::vector<uint32_t>>> announce;
};
Sounds g_snd;

struct Loaded {
    std::vector<uint32_t> banks;  // the banks' handles (the IOP's), while the planet is on
    uint32_t left[2] = {}, right[2] = {};  // the music's sectors (the disc layer's), the left channel's and the right's
} g_loaded;
int g_music = 0;  // the music playing (Sounds::music's index)

bool trace() {
    static const bool t = std::getenv("RAC1_CUSTOM_TRACE") != nullptr;
    return t;
}

const Sounds& sounds() {
    if (g_snd.read) return g_snd;
    g_snd.read = true;
    std::ifstream f(custom_level_dir() + "/planet.txt");
    std::string line;
    while (f && std::getline(f, line)) {
        std::istringstream in(line);
        std::string key;
        in >> key;
        if (key == "rac2_level") {
            in >> g_snd.level;
        } else if (key == "rac2_banks") {
            std::string w;
            while (in >> w) g_snd.banks.push_back(static_cast<uint32_t>(std::strtoul(w.c_str(), nullptr, 0)));
        } else if (key == "music" || key == "music_challenge" || key == "music_boss") {
            const int k = key == "music" ? 0 : key == "music_challenge" ? 1 : 2;
            in >> g_snd.music[k][0] >> g_snd.music[k][1];
        } else if (key == "announce") {
            std::string event;
            std::vector<uint32_t> entries;
            in >> event;
            for (uint32_t e; in >> e;) entries.push_back(e);
            if (!entries.empty()) g_snd.announce.emplace_back(event, entries);
        }
    }
    return g_snd;
}

uint32_t le32(const uint8_t* p) { return static_cast<uint32_t>(p[0] | p[1] << 8 | p[2] << 16) | static_cast<uint32_t>(p[3]) << 24; }

// orig: (host) a file of the disc at `base` by its path ("G/LEVEL2.WAD"): its sector and size from the ISO 9660
// directories (the primary volume descriptor at sector 16, the root's record at +156)
bool iso_find(uint32_t base, const std::string& path, uint32_t& lba, uint32_t& bytes) {
    std::vector<uint8_t> s(2048);
    if (!disc_read(base + 16, 1, s.data()) || std::memcmp(s.data() + 1, "CD001", 5) != 0) return false;
    uint32_t dir = le32(&s[156 + 2]), dir_bytes = le32(&s[156 + 10]);
    std::string rest = path;
    for (;;) {
        const size_t slash = rest.find('/');
        const std::string name = rest.substr(0, slash);
        std::vector<uint8_t> d(((dir_bytes + 2047) / 2048) * 2048);
        if (d.empty() || !disc_read(base + dir, static_cast<uint32_t>(d.size() / 2048), d.data())) return false;
        bool found = false;
        for (size_t o = 0; o < dir_bytes;) {
            const uint8_t n = d[o];
            if (n == 0) {
                o = (o / 2048 + 1) * 2048;
                continue;
            }
            const uint8_t nl = d[o + 32];
            std::string nm(reinterpret_cast<const char*>(&d[o + 33]), nl);
            if (const size_t semi = nm.find(';'); semi != std::string::npos) nm.resize(semi);
            if (nm == name) {
                dir = le32(&d[o + 2]);
                dir_bytes = le32(&d[o + 10]);
                found = true;
                break;
            }
            o += n;
        }
        if (!found) return false;
        if (slash == std::string::npos) {
            lba = dir;
            bytes = dir_bytes;
            return true;
        }
        rest = rest.substr(slash + 1);
    }
}

// orig: (host) the RAC2 discs listed, each looked into once
std::vector<Rac2Disc>& rac2_discs() {
    static std::vector<Rac2Disc> discs;
    static bool done = false;
    if (done) return discs;
    done = true;
    const int lv = sounds().level;
    if (lv < 0) return discs;
    for (const DataDisc& d : disc_data_discs()) {
        if (!d.ok || d.region.rfind("RAC2-", 0) != 0) continue;
        Rac2Disc r;
        r.region = d.region;
        r.base = d.base;
        const std::string n = std::to_string(lv);
        r.ok = iso_find(d.base, "G/LEVEL" + n + ".WAD", r.level_lba, r.level_bytes) && iso_find(d.base, "G/AUDIO" + n + ".WAD", r.audio_lba, r.audio_bytes);
        if (r.ok) {  // AUDIOn.WAD's table: +0 its size, then {sector, bytes} per entry from +8
            std::vector<uint8_t> h(8 * 2048);
            r.ok = disc_read(d.base + r.audio_lba, 8, h.data()) && le32(h.data()) <= h.size();
            for (uint32_t o = 8; r.ok && o + 8 <= le32(h.data()); o += 8) {
                r.audio_table.push_back(le32(&h[o]));
                r.audio_table.push_back(le32(&h[o + 4]));
            }
        }
        std::printf("[planet] Ratchet & Clank 2's disc %s (%s): %s\n", d.serial.c_str(), d.region.c_str(),
                    r.ok ? "its level and audio files found" : "its files not found");
        if (r.ok) discs.push_back(r);
    }
    return discs;
}

// orig: (host) the RAC2 disc to read for the voice language (custom_planet.h): Japanese from the Japanese disc, the
// European languages but English from the PAL disc, English from the North American disc; else the first one there
const Rac2Disc* rac2_disc() {
    std::vector<Rac2Disc>& all = rac2_discs();
    if (all.empty()) return nullptr;
    const uint32_t lang = voice_language();
    const char* want = lang == 1 ? "RAC2-NTSC-J" : lang >= 2 ? "RAC2-PAL" : "RAC2-NTSC-U";
    const char* order[] = {want, "RAC2-NTSC-U", "RAC2-PAL", "RAC2-NTSC-J"};
    for (const char* w : order)
        for (const Rac2Disc& r : all)
            if (r.region == w) return &r;
    return &all.front();
}

// orig: (none) — level 0's music tracks 0 / 1 (the start and the loop) made the planet's music `which` (Sounds::music);
// false when it has none
bool music_tracks_set(int which) {
    const Sounds& s = sounds();
    const Rac2Disc* d = rac2_disc();
    if (d == nullptr || s.music[which][0] < 0 || s.music[which][1] < 0) return false;
    for (int k = 0; k < 2; ++k) {
        const size_t e = static_cast<size_t>(s.music[which][k]) * 2;
        if (e + 3 >= d->audio_table.size()) return false;
    }
    for (int k = 0; k < 2; ++k) {
        const size_t e = static_cast<size_t>(s.music[which][k]) * 2;
        g_loaded.left[k] = d->base + d->audio_lba + d->audio_table[e];
        g_loaded.right[k] = d->base + d->audio_lba + d->audio_table[e + 2];
        sw(MUSIC + static_cast<uint32_t>(k) * 4, g_loaded.left[k]);
    }
    g_music = which;
    if (trace())
        std::printf("[planet] music %d from %s's AUDIO%d.WAD entries %d / %d (stereo): sectors %#x / %#x\n", which, d->region.c_str(), s.level,
                    s.music[which][0], s.music[which][1], g_loaded.left[0], g_loaded.left[1]);
    return true;
}

}  // namespace

// orig: (none)
void custom_planet_level_header_read(int32_t level) {
    if (!custom_level_active(level)) {
        g_loaded.left[0] = g_loaded.left[1] = 0;
        return;
    }
    music_tracks_set(0);
}

// orig: (none)
void custom_planet_music_select(int which) {
    if (which < 0 || which > 2 || which == g_music || !custom_level_active(lw(0x0015ed84))) return;
    if (!music_tracks_set(which)) return;
    music_stop_all();
    music_start(0, 1, 0x400);  // (as the level's start does: track 0, its loop queued after it, flag 1)
}

// orig: (none)
uint32_t custom_planet_announce_count(const char* event) {
    for (const auto& [name, entries] : sounds().announce)
        if (name == event) return static_cast<uint32_t>(entries.size());
    return 0;
}

// orig: (none)
bool custom_planet_announce(const char* event, uint32_t pick) {
    const Rac2Disc* d = rac2_disc();
    if (d == nullptr || !custom_level_active(lw(0x0015ed84))) return false;
    for (const auto& [name, entries] : sounds().announce) {
        if (name != event || entries.empty()) continue;
        const uint32_t lang = voice_language();
        const uint32_t slot = lang >= 2 && lang <= 5 ? lang - 1 : 0;  // (the group's: English / Japanese, French, German, Spanish, Italian)
        const size_t e = (static_cast<size_t>(entries[pick % entries.size()]) + slot) * 2;
        if (e + 1 >= d->audio_table.size() || d->audio_table[e + 1] == 0) return false;
        const uint32_t lsn = d->base + d->audio_lba + d->audio_table[e];
        if (!custom_dialog_play(lsn)) return false;
        if (trace()) std::printf("[planet] announcer: %s, AUDIO%d.WAD entry %zu (%s)\n", event, sounds().level, e / 2, d->region.c_str());
        return true;
    }
    return false;
}

// orig: (none)
uint32_t custom_planet_music_right(uint32_t lsn) {
    for (int k = 0; k < 2; ++k)
        if (lsn != 0 && lsn == g_loaded.left[k]) return g_loaded.right[k];
    return 0;
}

// orig: (none)
void custom_planet_banks_load() {
    if (!custom_level_active(lw(0x0015ed84)) || !g_loaded.banks.empty()) return;
    const Rac2Disc* d = rac2_disc();
    if (d == nullptr) return;
    std::vector<uint8_t> h(2048);
    if (!disc_read(d->base + d->level_lba, 1, h.data())) return;
    for (uint32_t at : sounds().banks) {
        if (at + 8 > le32(h.data())) continue;
        const uint32_t sector = le32(&h[at]);
        if (sector == 0) continue;
        const int32_t handle = snd_bank_load_by_loc(d->base + d->level_lba + sector, 0);
        if (handle != 0) g_loaded.banks.push_back(static_cast<uint32_t>(handle));
        std::printf("[planet] RAC2 sound bank (LEVEL%d.WAD +%#x, %u sectors, %s): handle %#x\n", sounds().level, at, le32(&h[at + 4]), d->region.c_str(),
                    static_cast<uint32_t>(handle));
    }
}

// orig: (none)
void custom_planet_banks_unload() {
    for (uint32_t b : g_loaded.banks) snd_cmd_06(b);
    if (!g_loaded.banks.empty() && trace()) std::printf("[planet] %zu RAC2 sound banks unloaded\n", g_loaded.banks.size());
    g_loaded.banks.clear();
}

// orig: (none)
const std::vector<uint32_t>& custom_planet_banks() { return g_loaded.banks; }

}  // namespace rac1
