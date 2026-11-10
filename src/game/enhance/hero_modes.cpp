// See hero_modes.h. Not a port: the session's choice of the hero build that runs Ratchet, the game's own hero modes entered
// and left for a plugin or a section, and the hero class's empty sequence slots filled from another level's core.
#include <game/enhance/hero_modes.h>

#include <formats/level_core.h>
#include <formats/scene_wad.h>
#include <game/enhance/context.h>
#include <game/enhance/pool.h>
#include <game/enhance/session.h>
#include <game/enhance/session_runtime.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <host/level_slots.h>
#include <runtime/iop/snd989_iop.h>
#include <runtime/kernel/disc.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/level/host_layout.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <future>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace rac1 {

bool g_hero_seq_on = false;

namespace {
constexpr uint32_t P = 0x0013f350;     // the player block
constexpr uint32_t HERO = P + 0x2080;  // the hero's moby (a mode's object while in it)
constexpr uint32_t OWN = P + 0xa88;    // Ratchet's own moby
constexpr uint32_t STATE = P + 0x2084, SUB = P + 0x208c, FORM = P + 0x20a4, LOST = P + 0x20b1;
constexpr uint32_t HEALTH = P + 0x22a8;
constexpr uint32_t BOARD_RIDDEN = P + 0x86c;  // the board he rides (Kalebo III's and Rilgar's builds)
constexpr uint32_t BOARD_STATE = 0x6b, BOARD_SUB = 0x16;

bool g_session = false;  // (a session's level: the hero context and every empty slot; a level mod of format 1 / 2: the magnetic walk's)

double now_ms() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

// --- the donors' sequences: a level's core's hero sequence table (as stored), read on a worker ---
using Seqs = std::map<int, std::vector<uint8_t>>;
struct Donor {
    std::shared_future<std::shared_ptr<Seqs>> fut;
    std::shared_ptr<Seqs> seqs;  // (once read)
    double asked = 0;
};
std::map<int, Donor> g_donors;

// orig: (host) level `level`'s core read from the disc: its hero sequences (the index's "sequence" parts), as stored
std::shared_ptr<Seqs> read_core_seqs(int level) {
    auto out = std::make_shared<Seqs>();
    std::vector<std::byte> wad;
    std::vector<uint8_t> index, data;
    if (!disc_read_level_wad(level, wad) || !extract_core_from_level_wad(wad, index, data)) return out;
    try {
        const DecodedLevelCoreIndex ix = decode_level_core_index(std::as_bytes(std::span(index)), std::as_bytes(std::span(data)));
        for (const LevelCoreSubAsset& a : ix.sub_assets) {
            if (a.kind != "sequence" || a.id < 0 || a.id > 255 || a.size < 0x20 || a.offset + a.size > data.size()) continue;
            std::vector<uint8_t> b;
            if (a.packed) {
                if (!decompress_wad(std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.data() + a.offset), a.size), b)) continue;
            } else {
                b.assign(data.begin() + a.offset, data.begin() + a.offset + a.size);
            }
            if (b.size() >= 0x20) (*out)[a.id] = std::move(b);
        }
    } catch (const std::exception&) {  // (a damaged core: no sequences from it)
        out->clear();
    }
    return out;
}

// orig: (host) level `level`'s core asked from the worker (once)
void donor_ask(int level) {
    if (g_donors.count(level) != 0) return;
    Donor& d = g_donors[level];
    d.asked = now_ms();
    d.fut = std::async(std::launch::async, read_core_seqs, level).share();
}

// orig: (host) level `level`'s sequences when read (null while the worker reads them, or never asked)
const Seqs* donor_seqs(int level) {
    const auto it = g_donors.find(level);
    if (it == g_donors.end()) return nullptr;
    Donor& d = it->second;
    if (d.seqs == nullptr) {
        if (d.fut.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return nullptr;
        d.seqs = d.fut.get();
        std::printf("[hero] level %d's core read for its hero sequences: %zu of them, %.1f ms after it was asked\n", level, d.seqs->size(), now_ms() - d.asked);
    }
    return d.seqs.get();
}

// the slots filled (the class's slot address -> the set), the room used above the game's 32 MB
std::map<uint32_t, uint32_t> g_filled;
uint32_t g_room = host_layout::HERO_SEQS;
std::set<uint32_t> g_missing_logged;
int g_base = 0;      // (the level mod's base level)
int g_wanted = -1;   // (a sequence asked while no donor had it read: filled when one has, while he plays it)

// (testing: RAC1_HERO_SEQS=0, the empty slots left empty, as before stage 9)
bool g_seq_off() {
    static const bool off = [] {
        const char* e = std::getenv("RAC1_HERO_SEQS");
        return e != nullptr && std::atoi(e) == 0;
    }();
    return off;
}

// the hero's own class (Ratchet's: not a mode's object), 0 none
uint32_t hero_class() {
    const uint32_t own = lwu(OWN) != 0 ? lwu(OWN) : lwu(HERO);
    return own != 0 ? lwu(own + 0x24) : 0;
}

// --- the hero's sounds the base's bank lacks ---
// Each level's hero class has the same 64 sound definitions, but each level's bank holds only the sounds its own hero uses:
// a definition of a sound the bank lacks has the sound -1 (sound_start's +0x1a), and the game plays nothing for it. Kalebo
// III's lacks 28 of them (his burning in lava or a deadly liquid, 0x3c / 0x7c, among them), Veldin's only 2 that no level's
// bank has (the pool's rows of class 0). On a level mod's level the definitions the base's bank lacks are pointed at Veldin's
// bank (loaded once as a host bank, kept for the run: the sound driver's own copy, 1.4 MB of host memory) and its sounds,
// once the level is in play. The original's levels never.
uint32_t g_snd_bank = 0;     // (Veldin's bank, a host bank; 0 not loaded)
bool g_snd_bank_tried = false;
bool g_snd_done = false;     // (this level's definitions looked at)

void hero_sounds_fill() {
    g_snd_done = true;
    const uint32_t c = hero_class();
    if (c == 0 || lw(0x0015ed84) == 0) return;  // (no hero; a Veldin base: its own bank has them)
    const uint32_t t = lwu(c + 0x28), n = lbu(c + 0xd);
    if (t == 0 || n == 0 || !pool_open(false)) return;
    const PoolClass* row = pool_find(0, 0);
    if (row == nullptr) return;
    std::vector<uint32_t> fill;
    for (uint32_t j = 0; j < n && j < row->sounds.size(); ++j)
        if (lhu(t + 32 * j + 0x1a) == 0xffffu && row->sounds[j] != 0xffffu) fill.push_back(j);
    if (fill.empty()) return;
    if (!g_snd_bank_tried) {
        g_snd_bank_tried = true;
        std::vector<uint8_t> b;
        g_snd_bank = pool_sound_bank(0, b) ? snd989_host_bank_load(b) : 0;
        if (g_snd_bank == 0) std::fprintf(stderr, "[hero] Veldin's sound bank could not be loaded: the hero's sounds the base's bank lacks are silent\n");
        else std::printf("[hero] Veldin's sound bank loaded (%zu KB): host bank %#x, for the hero's sounds a base's bank lacks\n", b.size() >> 10, g_snd_bank);
    }
    if (g_snd_bank == 0) return;
    for (const uint32_t j : fill) {
        sh(t + 32 * j + 0x1a, row->sounds[j]);
        sw(t + 32 * j + 0x1c, g_snd_bank);
    }
    std::printf("[hero] %zu of the hero's %u sounds are not in level %d's bank: Veldin's\n", fill.size(), n, lw(0x0015ed84));
}

// orig: (host) sequence bytes `b` (as stored in a core) put at the room's next place, their frame table and trigger block made
// addresses as the game's load makes them (dev_hoverboard.cpp's, checked there byte for byte against Kalebo III's own copy):
// its address, 0 when the room is full
uint32_t seq_place(const std::vector<uint8_t>& b) {
    const uint32_t at = g_room;
    if (at + b.size() > host_layout::HERO_SEQS + host_layout::HERO_SEQS_SIZE) return 0;
    std::memcpy(ee_ram_ptr(at), b.data(), b.size());
    const uint32_t frames = b[0x10];
    for (uint32_t k = 0; k < frames && 0x1c + 4 * k + 4 <= b.size(); ++k) ee_write<uint32_t>(at + 0x1c + 4 * k, ee_read<uint32_t>(at + 0x1c + 4 * k) + at);
    if (ee_read<uint32_t>(at + 0x14) != 0) ee_write<uint32_t>(at + 0x14, ee_read<uint32_t>(at + 0x14) + at);
    g_room = (at + static_cast<uint32_t>(b.size()) + 0xf) & ~0xfu;
    return at;
}

// --- the camera types: the session's type table (0x1ea880: rows of an id and its accept / init / update / exit functions, up
// to the id -1) copied into the session heap with the rows of the hero context's level whose functions are its own code (its
// camera types 3 and 8 on Kalebo III, 0x13 and 0x16 on Quartu...) taken from that level's table (its context image: the
// functions tags): replacing the session's row of the same id, or added before the end row (Veldin's table has no 3 or 8). The
// camera system reads the copy (camera.cpp g_camera_types_table) while the context lasts: the rows' order is the session's,
// so the cameras' rows (+0x8c) stay right ---
constexpr OvConst<0x001ea880u> CAM_TYPES{};
uint32_t g_cam_table = 0;  // (the copy's heap block)
bool g_cam_added = false;  // (rows added: the cameras made again when it goes)
int g_cam_table_level = -1;  // (the level whose rows the copy has)

// the cameras made again from the level's (as a level start makes them: their rows +0x8c found in the table now; a camera made
// with the table before would name another row: the end row's index is an added type's in the copy)
void cameras_again() {
    if (lwu(HERO) == 0) return;
    camera_init();
    camera_update();
}

void camera_types_restore() {
    if (g_cam_table == 0) return;
    g_camera_types_table = 0;
    session_heap_free(g_cam_table);
    g_cam_table = 0;
    g_cam_table_level = -1;
    if (g_cam_added) cameras_again();
    g_cam_added = false;
}

// orig: (host) level `level`'s own camera types made the session's: their count
int camera_types_take(int level) {
    const uint32_t la = ctx_level_addr(level, 0x001ea880u), img = ctx_image(level);
    if (la == 0 || img == 0) {
        std::printf("[hero] level %d's camera type table: %s\n", level, img == 0 ? "no context" : "no place in its map");
        return 0;
    }
    struct Row {
        uint32_t w[5];
    };
    std::vector<Row> rows;
    Row end{};
    for (uint32_t q = 0; q < 128; ++q) {
        Row r;
        for (uint32_t k = 0; k < 5; ++k) r.w[k] = lwu(CAM_TYPES + q * 0x14 + 4 * k);
        if (static_cast<int32_t>(r.w[0]) == -1) {
            end = r;
            break;
        }
        rows.push_back(r);
    }
    int n = 0;
    bool added = false;
    for (uint32_t q = 0; q < 128; ++q) {
        const uint32_t src = img + (la - CONTEXT_BASE) + q * 0x14;
        Row r;
        for (uint32_t k = 0; k < 5; ++k) r.w[k] = lwu(src + 4 * k);
        if (static_cast<int32_t>(r.w[0]) == -1) break;
        // (the words as the level's sites see them: a table its map pairs with Veldin's keeps its level's addresses in the image
        // unless its own-code-tables pass made them tags (Kalebo III's): its own functions tags, a paired function's start
        // Veldin's)
        for (uint32_t k = 1; k < 5; ++k)
            if (!ctx_is_tag(r.w[k]) && r.w[k] >= CTX_CODE_LO) r.w[k] = ctx_xlate(level, r.w[k]);
        bool own = false;
        for (uint32_t k = 1; k < 5; ++k) own = own || ctx_is_tag(r.w[k]);
        if (std::getenv("RAC1_CAM_TRACE") != nullptr)
            std::printf("[hero] level %d's camera type %#x: %08x %08x %08x %08x%s\n", level, r.w[0], r.w[1], r.w[2], r.w[3], r.w[4], own ? " (its own)" : "");
        if (!own) continue;
        bool put = false;
        for (Row& s : rows)
            if (s.w[0] == r.w[0]) s = r, put = true;
        if (!put) rows.push_back(r), added = true;
        ++n;
    }
    if (n == 0) return 0;
    rows.push_back(end);
    const uint32_t t = session_heap_alloc(static_cast<uint32_t>(rows.size() * 0x14), "hero context");
    if (t == 0) return 0;
    for (size_t q = 0; q < rows.size(); ++q)
        for (uint32_t k = 0; k < 5; ++k) sw(t + static_cast<uint32_t>(q) * 0x14 + 4 * k, rows[q].w[k]);
    g_cam_table = t;
    g_cam_table_level = level;
    g_cam_added = added;
    g_camera_types_table = t;
    if (added) cameras_again();
    return n;
}

// the session's camera type table made the one of the hero context's level, else of the camera level's (stage 10): its count of
// own rows
int camera_types_sync() {
    const int want = g_hero_level >= 0 ? g_hero_level : g_camera_level;
    if (want == g_cam_table_level && (want < 0 || g_cam_table != 0)) return 0;
    camera_types_restore();
    return want >= 0 ? camera_types_take(want) : 0;
}

// the board's builds: Kalebo III's and Rilgar's (their board state 0x6b rides it)
bool board_build() { return on_hero_level(16) || on_hero_level(5); }

bool trace() {
    static const bool t = std::getenv("RAC1_HERO_STATE") != nullptr;
    return t;
}
}  // namespace

// orig: (host) see hero_modes.h
bool hero_build_known(int level) { return level == 5 || level == 10 || level == 13 || level == 15 || level == 16; }

// orig: (host) see hero_modes.h
int hero_build_level() { return g_hero_level; }

// orig: (host) see hero_modes.h
bool hero_build_set(int level, const char* why) {
    if (level == ovl_reloc_level()) level = -1;  // (the overlay's own build: no context)
    if (level == HERO_GENERIC && !hero_build_known(g_session_rt ? ovl_reloc_level() : g_base)) level = -1;  // (the base's own hero code is the generic one)
    if (level == g_hero_level) return true;
    if (level == HERO_GENERIC) {
        if (!g_hero_seq_on) {  // (a level mod's level, any format)
            std::printf("[hero] the generic hero code not taken (%s): not a level mod's level\n", why);
            return false;
        }
    } else if (level >= 0) {
        if (!g_session_rt || !hero_build_known(level)) {
            std::printf("[hero] level %d's hero code not taken (%s): %s\n", level, why, !g_session_rt ? "not in a session" : "it has no hero or camera code of its own");
            return false;
        }
        if (!ctx_create(level)) {
            std::printf("[hero] level %d's hero code not taken (%s): its context cannot be made\n", level, why);
            return false;
        }
    } else if (!g_session_rt && g_hero_level != HERO_GENERIC) {
        return false;
    }
    const double t0 = now_ms();
    // (the board belongs to the build that rides it: off it first, on foot where he is)
    if (hero_mode_now() == HERO_MODE_BOARD) hero_mode_set(HERO_MODE_NONE, 0, "the hero context changes");
    const int was = g_hero_level;
    g_hero_level = level;
    const int cams = camera_types_sync();
    if (level >= 0) donor_ask(level);
    if (level == HERO_GENERIC) donor_ask(0);  // (the generic code's sequences the base's hero class lacks: Veldin's core has them all)
    auto name = [](int l) {
        return l >= 0 ? "level " + std::to_string(l) : l == HERO_GENERIC ? std::string("the generic (Veldin's)") : std::string("the base");
    };
    std::printf("[hero] the hero context: %s hero code runs Ratchet (before: %s's), %d camera types of its own (%s): %.3f ms\n", name(level).c_str(),
                name(was).c_str(), cams, why, now_ms() - t0);
    return true;
}

// orig: (host) see hero_modes.h
bool hero_camera_level_set(int level, const char* why) {
    if (level == ovl_reloc_level()) level = -1;  // (the overlay's own camera code)
    if (level == g_camera_level) return true;
    if (level >= 0 && (!g_session_rt || !ctx_create(level))) {
        std::printf("[hero] level %d's camera code not taken (%s): %s\n", level, why, !g_session_rt ? "not in a session" : "its context cannot be made");
        level = -1;
        if (level == g_camera_level) return false;
    }
    const double t0 = now_ms();
    const int was = g_camera_level;
    g_camera_level = level;
    const int cams = camera_types_sync();
    std::printf("[hero] the camera level: %s's camera code runs the cameras (before: %s), %d camera types of its own in the table%s (%s): %.3f ms\n",
                level >= 0 ? ("level " + std::to_string(level)).c_str() : "the base", was >= 0 ? ("level " + std::to_string(was)).c_str() : "the base's", cams,
                g_hero_level >= 0 ? " (the hero context's level gives the table)" : "", why, now_ms() - t0);
    return level >= 0;
}

// orig: (host) see hero_modes.h
int32_t hero_mode_now() {
    if (lwu(HERO) == 0) return HERO_MODE_NONE;
    const uint32_t form = lbu(FORM);
    if (form != 0) return static_cast<int32_t>(form);
    const uint32_t st = lwu(STATE);
    if (lw(SUB) == static_cast<int32_t>(BOARD_SUB) && st >= BOARD_STATE && st <= 0x6e && board_build()) return HERO_MODE_BOARD;
    return HERO_MODE_NONE;
}

// orig: (host) see hero_modes.h
uint32_t hero_mode_class(int32_t mode) {
    switch (mode) {
    case HERO_MODE_CLANK: return 87;
    case HERO_MODE_GIANT_CLANK: return 419;
    case HERO_MODE_BOARD: return 439;
    case HERO_MODE_SHIP: return 69;
    default: return 0;
    }
}

// orig: (host) see hero_modes.h
bool hero_mode_set(int32_t mode, uint32_t moby, const char* why) {
    const uint32_t hm = lwu(HERO);
    const int32_t now = hero_mode_now();
    auto refuse = [&](const char* r) {
        std::printf("[hero] mode %d not entered (%s): %s\n", static_cast<int>(mode), why, r);
        return false;
    };
    if (hm == 0) return refuse("no hero");
    if (mode == HERO_MODE_NONE) {
        if (now == HERO_MODE_NONE) return true;
        if (now == HERO_MODE_BOARD) {
            hero_set_state(0, 1);  // (as the race's own end: on foot where he is; the board goes home by itself)
        } else {
            hero_mode_exit();  // (the game's: his own moby back at the mode's object's place, state 0; form 1's health given back)
        }
        std::printf("[hero] mode %d left (%s): Ratchet at %.2f %.2f %.2f, state %#x, health %d\n", static_cast<int>(now), why, lwc1(lwu(HERO) + 0x10),
                    lwc1(lwu(HERO) + 0x14), lwc1(lwu(HERO) + 0x18), lwu(STATE), lw(HEALTH));
        return true;
    }
    if (lbu(LOST) != 0 || lw(HEALTH) <= 0) return refuse("Ratchet is lost or dying");
    if (now != HERO_MODE_NONE) return refuse("he is in another mode: leave it first");
    if (moby == 0 || static_cast<int8_t>(lbu(moby + 0x20)) < 0) return refuse("no such object");
    const double t0 = now_ms();
    if (mode == HERO_MODE_CLANK || mode == HERO_MODE_GIANT_CLANK) {
        // (the game's own: the object becomes the hero's moby, the form byte set, the form's idle state; level_setup's
        // start in a vehicle: 1 Clank, 2 Giant Clank)
        hero_mode_enter(static_cast<uint32_t>(mode), mode == HERO_MODE_CLANK ? 0x43u : 0x5au, moby);
    } else if (mode == HERO_MODE_BOARD) {
        if (!board_build()) return refuse("the hero code has no board state: a hero context of level 16 or 5 first");
        // the board's mount (what its own update does in its state 0 once Ratchet is near: dev_menu.cpp's)
        const uint32_t pv = lwu(moby + 0x78);
        sw(BOARD_RIDDEN, moby);
        hero_set_state(BOARD_STATE, 1);
        if (pv != 0 && lwu(pv + 0x50) == 0)
            for (uint32_t k = 0; k < 16; k += 4) sw(pv + 0x10 + k, lwu(moby + 0x10 + k));
        sb(moby + 0x20, 1);
        sb(moby + 0x30, 0xff);
    } else {
        return refuse("not a mode the session knows (1 Clank, 2 Giant Clank, 4 the board)");
    }
    std::printf("[hero] mode %d entered (%s): riding moby %08x (class %u), state %#x, form %u: %.3f ms\n", static_cast<int>(mode), why, moby, lhu(moby + 0xa6),
                lwu(STATE), lbu(FORM), now_ms() - t0);
    return hero_mode_now() == mode;
}

// orig: (host) see hero_modes.h
void hero_seq_need(uint32_t set) {
    if (!g_hero_seq_on || set > 255 || g_seq_off()) return;
    const uint32_t hm = lwu(HERO);
    if (hm == 0 || lbu(FORM) != 0) return;  // (a mode's object plays its own class's)
    const uint32_t cls = lwu(hm + 0x24);
    if (cls == 0 || cls != hero_class() || set >= lbu(cls + 0xc)) return;
    const uint32_t slot = cls + 0x48 + 4 * set;
    if (lwu(slot) != 0) return;
    if (!g_session && g_hero_level != HERO_GENERIC && lbu(P + 0x12e7) == 0) return;  // (a level mod of format 1 / 2: the magnetic walk's only)
    // the donors: the hero context's level's core, then Veldin's (every sequence)
    const int donors[2] = {g_hero_level >= 0 ? g_hero_level : -1, 0};
    for (const int d : donors) {
        if (d < 0) continue;
        donor_ask(d);
        const Seqs* s = donor_seqs(d);
        if (s == nullptr) continue;
        const auto it = s->find(static_cast<int>(set));
        if (it == s->end()) continue;
        const uint32_t at = seq_place(it->second);
        if (at == 0) {
            if (g_missing_logged.insert(set | 0x1000u).second) std::printf("[hero] sequence %u: no room left for it\n", set);
            return;
        }
        sw(slot, at);
        g_filled[slot] = set;
        if (g_wanted == static_cast<int>(set)) g_wanted = -1;
        std::printf("[hero] the hero's sequence %u (empty in his class) from level %d's core: %zu bytes at %08x\n", set, d, it->second.size(), at);
        return;
    }
    g_wanted = static_cast<int>(set);  // (filled by hero_modes_frame once a donor is read, while he plays it)
    if (trace() && g_missing_logged.insert(set).second) std::printf("[hero] sequence %u asked, empty in his class; no donor has it read yet\n", set);
}

// orig: (host) see hero_modes.h
void hero_modes_level(bool level_mod, bool session, int base) {
    if (g_hero_level >= 0) std::printf("[hero] the hero context dropped with the level (it was level %d's)\n", g_hero_level);
    g_hero_level = -1;
    g_camera_level = -1;
    g_camera_types_table = 0;  // (the session heap goes with the level)
    g_cam_table = 0;
    g_cam_table_level = -1;
    g_cam_added = false;
    g_hero_seq_on = level_mod;
    g_session = session;
    g_filled.clear();
    g_room = host_layout::HERO_SEQS;
    g_missing_logged.clear();
    g_base = base;
    g_wanted = -1;
    g_snd_done = false;
}

// orig: (host) see hero_modes.h
void hero_modes_build_file(const std::string& build_dir) {
    std::ifstream in(build_dir + "/hero.txt");
    std::string k, what;
    if (!(in >> k >> what) || k != "hero") return;
    if (what == "generic") hero_build_set(HERO_GENERIC, "the level's hero line");
}

// orig: (host) see hero_modes.h
void hero_modes_frame(uint64_t frame) {
    if (g_hero_seq_on && !g_snd_done && lw(OV(0x0015f5c4)) == 0) hero_sounds_fill();  // (a level mod's level in play: the hero's sounds)
    if (g_hero_seq_on && !g_seq_off()) {
        // a level mod of format 1 / 2 on a base whose core lacks hero sequences (every level's but Veldin's): Veldin's core read on
        // the worker the first time Ratchet is on a Magneboots surface (his magnetic walk's sequence may be empty; read at the
        // level's start, the read slowed the level's load: the arena's regression runs changed)
        if (!g_session && g_base != 0 && lbu(P + 0x12e7) != 0) donor_ask(0);
        // a sequence asked before its donor was read: filled once it is, while he still plays it (the game sets it once, then
        // loops it)
        if (g_wanted >= 0) {
            const uint32_t hm = lwu(HERO);
            if (hm != 0 && lbu(hm + 0x53) == static_cast<uint32_t>(g_wanted)) hero_seq_need(static_cast<uint32_t>(g_wanted));
            else g_wanted = -1;
        }
    }
    // (testing: RAC1_MOBY_WATCH=<class>[:<n>[:<offset>]]: the first live moby of that class every n frames (default 30), on any
    // level: its state, animation, place, +0x2c, four words of its pvars at the offset (hex, default 0); a boss's fight on its level against the same in a session)
    // (testing: RAC1_MOBY_POKE=<class>@<frame>:<m|p|M|P><offset>=<value>[,...]: at that game frame, values written into the first
    // live moby of that class (m: a word of its record, p: of its pvars, M / P: a byte; offset and value in hex): a boss's fight taken to a given state on its level
    // and in a session alike, then compared with RAC1_MOBY_WATCH)
    static const char* poke = std::getenv("RAC1_MOBY_POKE");
    if (poke != nullptr) {
        static const int pcls = std::atoi(poke);
        static const uint64_t pat = std::strchr(poke, '@') != nullptr ? std::strtoull(std::strchr(poke, '@') + 1, nullptr, 10) : 0;
        if (frame == pat && std::strchr(poke, ':') != nullptr) {
            const uint32_t first = lwu(OV(0x0015ffd8));
            for (uint32_t m = first; m != 0 && lbu(m + 0x20) != 0xffu && m < first + 0x100u * 2048; m += 0x100) {
                if (static_cast<int8_t>(lbu(m + 0x20)) < 0 || lhu(m + 0xa6) != static_cast<uint32_t>(pcls)) continue;
                for (const char* p = std::strchr(poke, ':') + 1; p != nullptr && *p != 0;) {
                    const char where = *p++;
                    char* e = nullptr;
                    const uint32_t off = static_cast<uint32_t>(std::strtoul(p, &e, 16));
                    if (e == nullptr || *e != '=') break;
                    const uint32_t v = static_cast<uint32_t>(std::strtoul(e + 1, &e, 16));
                    const uint32_t base = where == 'p' || where == 'P' ? lwu(m + 0x78) : m;
                    if (base != 0 && (where == 'M' || where == 'P')) sb(base + off, v);  // (M / P: a byte)
                    else if (base != 0) sw(base + off, v);
                    std::printf("[watch] frame %llu: class %d moby %08x: %c+%#x = %08x\n", static_cast<unsigned long long>(frame), pcls, m, where, off, v);
                    p = e != nullptr && *e == ',' ? e + 1 : nullptr;
                }
                break;
            }
        }
    }
    static const char* watch = std::getenv("RAC1_MOBY_WATCH");
    if (watch != nullptr) {
        static const int cls = std::atoi(watch);
        static const char* c1 = std::strchr(watch, ':');
        static const int n = c1 != nullptr ? std::max(1, std::atoi(c1 + 1)) : 30;
        static const uint32_t pvo = c1 != nullptr && std::strchr(c1 + 1, ':') != nullptr ? static_cast<uint32_t>(std::strtoul(std::strchr(c1 + 1, ':') + 1, nullptr, 16)) : 0;
        if (frame % static_cast<uint64_t>(n) == 0) {
            const uint32_t first = lwu(OV(0x0015ffd8));
            for (uint32_t m = first; m != 0 && lbu(m + 0x20) != 0xffu && m < first + 0x100u * 2048; m += 0x100) {
                if (static_cast<int8_t>(lbu(m + 0x20)) < 0 || lhu(m + 0xa6) != static_cast<uint32_t>(cls)) continue;
                const uint32_t pv = lwu(m + 0x78);
                std::printf("[watch] frame %llu: class %d moby %08x state %u anim %u frame %u at %.2f %.2f %.2f yaw %.2f +0x2c %d, pvars %08x: %08x %08x %08x %08x; "
                            "hero %.2f %.2f %.2f health %d; update %08x\n",
                            static_cast<unsigned long long>(frame), cls, m, lbu(m + 0x20), lbu(m + 0x53), lbu(m + 0x51), lwc1(m + 0x10), lwc1(m + 0x14),
                            lwc1(m + 0x18), lwc1(m + 0x48), static_cast<int8_t>(lbu(m + 0x2c)), pv, pv != 0 ? lwu(pv + pvo) : 0u, pv != 0 ? lwu(pv + pvo + 4) : 0u,
                            pv != 0 ? lwu(pv + pvo + 8) : 0u, pv != 0 ? lwu(pv + pvo + 0xc) : 0u, lwc1(P + 0x80), lwc1(P + 0x84), lwc1(P + 0x88), lw(HEALTH), lwu(m + 0x74));
                break;
            }
        }
    }
    // (testing: RAC1_HERO_STATE=<n>: the hero's state, sub-state, form, place and animation every n frames, on any level)
    static const int every = std::getenv("RAC1_HERO_STATE") != nullptr ? std::max(1, std::atoi(std::getenv("RAC1_HERO_STATE"))) : 0;
    if (every == 0 || frame % static_cast<uint64_t>(every) != 0) return;
    const uint32_t hm = lwu(HERO);
    if (hm == 0) return;
    std::printf("[hero] frame %llu: state %#x sub %#x form %u at %.2f %.2f %.2f, speed %.2f, anim %u frame %u, board %08x, build %d\n",
                static_cast<unsigned long long>(frame), lwu(STATE), lwu(SUB), lbu(FORM), lwc1(hm + 0x10), lwc1(hm + 0x14), lwc1(hm + 0x18), lwc1(P + 0x160),
                lbu(hm + 0x53), lbu(hm + 0x51), lwu(BOARD_RIDDEN), g_hero_level);
}

// orig: (host) see hero_modes.h
void hero_modes_respawn_begin() {
    const int32_t m = hero_mode_now();
    if (m == HERO_MODE_CLANK || m == HERO_MODE_GIANT_CLANK) hero_mode_set(HERO_MODE_NONE, 0, "a death: the soft reset");
}

}  // namespace rac1
