// See level_compose.h. Not a port: our own tool. The data file's and gameplay file's layouts are the game's (src/formats/
// level_file.h, level_gameplay.h, level_collision.h); which parts of the base level are kept follows what the game's
// level setup (src/game/ovl/level_setup.cpp, level_load.cpp) reads.
#include "level_compose.h"

#include "level_music.h"
#include "level_pictures.h"
#include "mrp_export.h"

#include <formats/host_meshes.h>
#include <formats/level_collision.h>
#include <formats/level_file.h>
#include <formats/moby_mrp.h>
#include <formats/scene_wad.h>
#include <formats/wad_encode.h>
#include <formats/world_mrp.h>
#include <game/enhance/pool.h>
#include <platform/platform.h>
#include <runtime/kernel/disc.h>
#include <runtime/kernel/mods.h>

#include <stb_image.h>  // (its implementation: static_skinned_render.cpp)

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace rac1::setup {

namespace fs = std::filesystem;

namespace {

// level.txt's settings (format 1), as the docs list them
[[maybe_unused]] constexpr const char* kLevelTxtHelp =
    "name <text> / name.<fr|de|es|it> <text>   the ship's list's row and the planet page's title\n"
    "region <text> / region.<lang> <text>      the line above it\n"
    "hero <x> <y> <z> [<yaw>] | hero auto      where Ratchet starts (game units, z up; yaw in radians); auto (the default\n"
    "                                          with a ship): where he steps off the ship after the landing\n"
    "ship <x> <y> <z> <yaw> | ship none        where the ship stands (none: no way out but the pause menu)\n"
    "death_height <z>                          below it Ratchet dies\n"
    "background <r> <g> <b>                    the sky's colour behind everything (0..255)\n"
    "fog <r> <g> <b>                           the fog's colour (0..255)\n"
    "collision <file>                          the collision mesh (default collision.obj)\n"
    "collision_type <n>                        the surface type of faces with no `usemtl type<N>` (default 31)\n"
    "meshes <file>                             the drawn meshes (default meshes.obj; optional)\n"
    "up z | up y                               the OBJ files' up axis (default z; y: Blender's default OBJ export)\n"
    "scale <s>                                 the OBJ files' units to game units (default 1)\n"
    "moby <class> <x> <y> <z> [<yaw>]          an object of the base level's class <class>, as its first placement there\n"
    "music <level> | music <loop> [<intro>] | music none   the level's music (<loop>, <intro>: a VAG, or a WAV or .ogg encoded)\n"
    "sky <level>                               the sky of the game's level <level> (default: the base level's)\n"
    "base <level|name>                         the base level (as mod.json's level.base: checked against it)\n"
    "spawn <class> <count>                     room for <count> objects of the base level's class at once (the plugin's base_spawn)\n"
    "offset auto | offset <x> <y> <z>          moved by that much (auto: as far as the collision needs to be above 0)\n"
    "class <name> <model.obj> [size <s>] [solid|ghost] [scale <k>]   a class of the level's own objects (docs/sdk.md)\n"
    "object <class> <x> <y> <z> [<yaw>] [<key>=<value> ...]           an object of a level class\n"
    "sound <name> <file.wav|.ogg|.vag> [volume <v>] [range <near> <far>] [loop] [pitch <semitones>]   a sound of the level's own\n"
    "trigger <name> <x0> <y0> <z0> <x1> <y1> <z1>                    a box Ratchet enters and leaves (the plugin's trigger callback)\n"
    "picture <file.png>                        the planet on the loading screen (128 x 128; default: none, the base level's)\n"
    "banner <file.png> / banner.<lang> <file.png>   the name on the loading screen (256 x 32; default: made from name)\n"
    "map_picture <file.png>                    the ship's map's picture (306 x 108 shown; default: made from picture)\n"
    "import <name> | import <level>:<class>    a class of another level, with what it spawns (the open engine: docs/open-engine.md)\n"
    "path <name> <x> <y> <z> [<x> <y> <z> ...]  a path (1 to 64 points) the plugin gives an imported object (object_bind_path)\n"
    "session                                   (format 3) the level is a session: loaded once, a death a soft reset (mod.json's level.format 3)\n"
    "checkpoint <name> <x> <y> <z> [<yaw>]     (format 3) a place Ratchet comes back to after a death (\"start\": the first one)\n"
    "respawn reset | respawn keep              (format 3) the placed objects at a respawn: those killed come back (default) or not\n"
    "moby <class> <x> <y> <z> [<yaw>] [<slot>=<value> ...]   (format 3) slots of the class's profile set (a path's name, a number)\n"
    "moby <name>|<level>:<class> <x> <y> <z> [<yaw>] [<slot>=<value> ...]   (format 3) a class of another level placed (imported)\n"
    "section <name> [remove|freeze|keep] [respawn reset|keep] [fresh] ... end   (format 3) a section: its own collision, meshes, lights,\n"
    "                                          sky, music, fog, background, death_height, offset, entry, checkpoint, moby, portal lines\n"
    "entry <name> <x> <y> <z> [<yaw>]          (format 3) where a portal or section_enter puts Ratchet in the section\n"
    "portal <name> <x0> <y0> <z0> <x1> <y1> <z1> to <section>:<entry> [fade <frames>]   (format 3) a box that switches sections\n"
    "start <section>:<entry>                   (format 3) the session starts there once the landing is over\n"
    "leave remove|freeze|keep                  (format 3) main's objects when main is left (default remove)\n"
    "rail <name> [loop] [mesh [<material>]] [nostrip] <x> <y> <z> <x> <y> <z> ...   a grind rail (in main or a section)\n"
    "rails <file.obj> [loop] [mesh [<material>]] [nostrip]   grind rails: each object of line elements in the OBJ one, named as it\n"
    "items <item> ...                          items given for the visit (grindboots, magneboots, ...): taken back when leaving\n"
    "class <name> <model.obj> collision <coll.obj>   (with the class line's other words) a collision mesh of the class's own\n";

struct LevelTxt {
    float hero[4] = {0, 0, 1, 0};
    bool has_hero = false;
    bool hero_auto = false;             // hero auto (or no hero line, with a ship): beside the ship, where the landing leaves him
    bool ship = false;
    float ship_at[4] = {};
    bool has_death = false;
    bool update_all = false;            // update_all: every moby updated wherever the camera is (level.sdk)
    float death = -100.0f;
    bool has_background = false, has_fog = false;
    int background[3] = {}, fog[3] = {};
    std::string collision = "collision.obj", meshes = "meshes.obj";
    int collision_type = 31;
    bool y_up = false;
    float scale = 1.0f;
    struct Moby {
        int cls = 0;
        float at[4] = {};  // x y z yaw
        int line = 0;
        std::vector<std::pair<std::string, std::string>> binds;  // (format 3) slot=value: its profile's slots
        bool pool = false;  // (a spawn line's: for base_spawn, no spawn rules)
        int uid = 0;        // (its uid, given by place_mobys)
        int pvars = 0;      // (its pvars' size)
        // (format 3) a class of another level placed (`moby <name>|<level>:<class> ...`): its name as written; its level and
        // class once the composer has resolved it (an import of it is added): the session spawns it with its section
        std::string import;
        int ilevel = -1;
        uint32_t icls = 0;
    };
    std::vector<Moby> mobys;
    int music_level = -1;               // music <level>: that level's tracks
    bool music_none = false;            // music none
    std::string music_loop, music_intro;  // music <loop.vag> [<intro.vag>]: the mod's own
    int sky_level = -1;                 // sky <level>
    bool has_floor = false;
    float floor = 0;                    // (the collision's lowest z, after the offset: the default death height's check)
    int base = -1;                      // base <level|name>: the base level mod.json names (checked against it)
    struct Pool {
        int cls = 0, count = 0, line = 0;
    };
    std::vector<Pool> pools;            // spawn <class> <count>: room for the plugin's base_spawn of the base level's class
    bool offset_auto = false;           // offset auto
    float offset[3] = {};               // offset <x> <y> <z> (or auto's): added to every coordinate (not lights.txt's)
    // the level's own content (docs/sdk.md): its classes, their objects, its sounds, its triggers
    struct Class {
        std::string name, model;
        float size = 0;                 // (0: the model's largest extent)
        float scale = 1;
        bool ghost = false;
        std::string collision;  // (stage 7) `collision <coll.obj>`: a collision mesh of its own (else its box)
        int line = 0;
    };
    struct Object {
        std::string cls;
        float at[4] = {};
        std::string params;
        int line = 0;
        int section = 0;  // (format 3) the section block it is in (1.. its index + 1; 0 main's)
    };
    struct Sound {
        std::string name, file;
        float volume = 1, near = 10, far = 60, pitch = 0;
        bool loop = false;
        int line = 0;
    };
    struct Trigger {
        std::string name;
        float lo[3] = {}, hi[3] = {};
    };
    std::vector<Class> classes;
    std::vector<Object> objects;
    std::vector<Sound> sounds;
    std::vector<Trigger> triggers;
    struct Path {  // (the plugin binds them to imported objects' slots: object_bind_path, ABI 8)
        std::string name;
        std::vector<std::array<float, 3>> pts;
    };
    std::vector<Path> paths;
    // (stage 7) grind rails: `rail <name> [loop] [mesh [<material>]] [nostrip] x y z ...` and `rails <file.obj> [...]` (each OBJ object
    // of line elements a rail), in main or a section; the block +0x74 of its gameplay file (the game's rail table)
    struct Rail {
        std::string name, file, material;
        bool loop = false, mesh = false, strip = true;
        std::vector<std::array<float, 3>> pts;
        int line = 0;
    };
    std::vector<Rail> rails;
    std::vector<int> items;  // (stage 7) `items grindboots magneboots ...`: the items given for the visit (the game's item ids)
    // the classes imported from other levels (import <name|level:class>: the open engine's pool)
    struct Import {
        std::string name;
        int line = 0;
        std::string world;  // (stage 8: a class of a section's world's objects: the base level's own serves when it has it)
    };
    // (stage 9) `hero <level> [clank|giantclank|board]`: the hero code (and camera code) of another level runs Ratchet in the
    // section (main's: outside every section block), and the mode he is in there (its object spawned at his entry)
    struct Hero {
        int level = -1;  // (-1: the session base's own)
        std::string mode;
        int line = 0;
    };
    Hero hero_ctx;
    std::vector<Import> imports;
    // (format 3) the session (docs/open-engine.md, "Sessions"): its line, its checkpoints, the respawn policy
    int format = 1;                     // (mod.json's level.format, set by the composer)
    int session_line = 0;
    struct Checkpoint {
        std::string name;
        float at[4] = {};
        int line = 0;
    };
    std::vector<Checkpoint> checkpoints;
    std::string respawn;
    int respawn_line = 0;
    // (format 3) sections and portals (docs/open-engine.md, "Sections"): the lines outside any `section` block are the section
    // "main" (where the ship lands); `section <name> [remove|freeze|keep] [respawn reset|keep]` .. `end` blocks the others
    struct Entry {
        std::string name;
        float at[4] = {};
        int line = 0;
    };
    struct Portal {
        std::string name, to, entry;
        float lo[3] = {}, hi[3] = {};
        int fade = 0;
        int line = 0;
    };
    struct Section {
        std::string name, policy = "remove", respawn;
        bool fresh = false;  // (`fresh`, SDK ABI 17: every entry puts its objects back as at the first)
        int line = 0;
        std::string collision, meshes, lights;  // ("": the session's collision stays, no meshes, the session's lights)
        int sky_level = -1;                     // (-1: the session's sky)
        int music_level = -2;                   // (-2: the session's music, -1: none)
        bool has_fog = false, has_background = false, has_death = false;
        int fog[3] = {}, background[3] = {};
        float death = 0;
        float offset[3] = {};                   // (added to the level's offset: its files' and its lines' coordinates)
        std::vector<Moby> mobys;
        std::vector<Checkpoint> checkpoints;
        std::vector<Entry> entries;
        std::vector<Portal> portals;
        std::vector<Rail> rails;
        // (stage 8) a game level's world as its scenery: `world <level>` (its geometry, collision, sky, music, fog, lights, level
        // data), `objects all|<class>...` (its placed objects of those classes come with it; none by default), `box x0 y0 z0 x1 y1 z1`
        // (only its objects inside); the composer's choice: their uids there -> the session's
        int world = -1;
        int world_line = 0;
        bool zones_all = false;  // (`zones all`: every zone of the world drawn wherever the camera is, not the zone grid's choice)
        int zones_line = 0;
        bool objects_all = false;
        std::vector<std::string> objects;
        bool has_box = false;
        float box_lo[3] = {}, box_hi[3] = {};
        std::vector<std::pair<int32_t, int32_t>> wuids;
        Hero hero;  // (stage 9)
        // (stage 10) `boss <class|name|level:class>...`: its objects of those classes are its bosses (a death in the section starts
        // the fight over: a boss still in play placed again; one leaving play otherwise is beaten: the plugin told). Resolved by
        // the composer into (level, class)
        std::vector<std::string> bosses;
        std::vector<std::pair<int, uint32_t>> boss_classes;
        int boss_line = 0;
    };
    std::vector<Section> sections;
    std::vector<Entry> entries;    // (main's)
    std::vector<Portal> portals;   // (main's)
    std::string leave = "remove";  // (main's policy when it is left)
    int leave_line = 0;
    std::string start_section, start_entry;
    int start_line = 0;
    int uid_first = 0;             // (place_mobys: uids from uid_first + 1)
    // the pictures (level_pictures.h): the files given, and the names (by language: "" the default, fr de es it)
    std::string picture, map_picture;
    std::map<std::string, std::string> banners, names;
};

// a name of the level's own (a class, a sound, a trigger): letters, digits, - _ .
bool sdk_name_ok(const std::string& n) {
    if (n.empty() || n.size() > 31) return false;
    for (const char c : n)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.')) return false;
    return true;
}

struct Log {
    std::string text;
    void say(const std::string& s) {
        text += s + "\n";
        std::cout << "[compose] " << s << "\n";
    }
};

std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));
std::string fmt(const char* f, ...) {
    char b[512];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(b, sizeof b, f, ap);
    va_end(ap);
    return b;
}

bool read_file(const fs::path& p, std::vector<uint8_t>& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

bool write_file(const fs::path& p, const std::vector<uint8_t>& b) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
    return static_cast<bool>(f);
}

int32_t rd32(const std::vector<uint8_t>& b, size_t at) {
    int32_t v = 0;
    if (at + 4 <= b.size()) std::memcpy(&v, b.data() + at, 4);
    return v;
}
void wr32(std::vector<uint8_t>& b, size_t at, int32_t v) {
    if (at + 4 > b.size()) b.resize(at + 4, 0);
    std::memcpy(b.data() + at, &v, 4);
}
void wrf(std::vector<uint8_t>& b, size_t at, float v) {
    int32_t i;
    std::memcpy(&i, &v, 4);
    wr32(b, at, i);
}
size_t align_up(size_t v, size_t a) { return (v + a - 1) / a * a; }

// ---- level.txt ----
// a moby line's words after `moby`: <class>|<name>|<level>:<class> <x> <y> <z> [<yaw>] [<slot>=<value> ...]
bool moby_line(std::istringstream& in, LevelTxt::Moby& m) {
    std::string c;
    if (!(in >> c >> m.at[0] >> m.at[1] >> m.at[2]) || c.empty()) return false;
    if (c.find_first_not_of("0123456789") == std::string::npos) {
        m.cls = std::atoi(c.c_str());
    } else {
        for (const char ch : c)
            if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '.' || ch == '_' || ch == '-' || ch == ':')) return false;
        m.import = c;
    }
    m.at[3] = 0;
    std::string w;
    for (bool first = true; in >> w; first = false) {  // ([yaw] then (format 3) slot=value words)
        const size_t eq = w.find('=');
        if (eq == std::string::npos) {
            char* e = nullptr;
            m.at[3] = std::strtof(w.c_str(), &e);
            if (!first || e == w.c_str() || *e != 0) return false;
            continue;
        }
        if (eq == 0 || eq + 1 == w.size()) return false;
        m.binds.emplace_back(w.substr(0, eq), w.substr(eq + 1));
    }
    return true;
}
// `<name> <x> <y> <z> [<yaw>]` (a checkpoint's, an entry's)
bool place_line(std::istringstream& in, std::string& name, float at[4]) {
    if (!(in >> name >> at[0] >> at[1] >> at[2]) || !sdk_name_ok(name)) return false;
    if (!(in >> at[3])) at[3] = 0;
    return true;
}
// `<name> <x0> <y0> <z0> <x1> <y1> <z1> to <section>:<entry> [fade <frames>]`
bool portal_line(std::istringstream& in, LevelTxt::Portal& p) {
    std::string to, w;
    if (!(in >> p.name >> p.lo[0] >> p.lo[1] >> p.lo[2] >> p.hi[0] >> p.hi[1] >> p.hi[2] >> to) || !sdk_name_ok(p.name) || to != "to" || !(in >> w)) return false;
    const size_t c = w.find(':');
    if (c == std::string::npos || c == 0 || c + 1 == w.size()) return false;
    p.to = w.substr(0, c);
    p.entry = w.substr(c + 1);
    if (in >> w) {
        if (w != "fade" || !(in >> p.fade) || p.fade < 0 || p.fade > 120) return false;
    }
    for (int i = 0; i < 3; ++i)
        if (p.lo[i] > p.hi[i]) std::swap(p.lo[i], p.hi[i]);
    return true;
}

// (stage 7) `rail <name> [loop] [mesh [<material>]] [nostrip] <x y z> ...` / `rails <file.obj> [loop] [mesh [<material>]] [nostrip]`:
// the options, then (a rail line) its points
bool rail_line(std::istringstream& in, LevelTxt::Rail& r, bool file) {
    if (!(in >> (file ? r.file : r.name))) return false;
    if (!file && !sdk_name_ok(r.name)) return false;
    std::vector<std::string> w;
    for (std::string x; in >> x;) w.push_back(x);
    auto number = [](const std::string& x, float& v) {
        char* end = nullptr;
        v = std::strtof(x.c_str(), &end);
        return end != nullptr && end != x.c_str() && *end == 0;
    };
    size_t i = 0;
    float v = 0;
    for (; i < w.size() && !number(w[i], v); ++i) {
        if (w[i] == "loop") {
            r.loop = true;
        } else if (w[i] == "nostrip") {
            r.strip = false;
        } else if (w[i] == "mesh") {
            r.mesh = true;
            if (i + 1 < w.size() && w[i + 1] != "loop" && w[i + 1] != "nostrip" && !number(w[i + 1], v)) r.material = w[++i];
        } else {
            return false;
        }
    }
    if (file) return i == w.size();
    if ((w.size() - i) % 3 != 0 || w.size() - i < 6) return false;  // (two points at least)
    for (; i < w.size(); i += 3) {
        std::array<float, 3> p{};
        for (size_t k = 0; k < 3; ++k)
            if (!number(w[i + k], p[k])) return false;
        r.pts.push_back(p);
    }
    return true;
}

// (stage 7) `items grindboots magneboots ...`: the items given for the visit (names, or the game's item ids)
bool items_line(std::istringstream& in, std::vector<int>& items) {
    static const std::map<std::string, int> kItems = {{"helipack", 0x02}, {"thrusterpack", 0x03}, {"hydropack", 0x04}, {"o2mask", 0x06},
                                                      {"pilotshelmet", 0x07}, {"magneboots", 0x1c}, {"grindboots", 0x1d}};
    bool any = false;
    for (std::string w; in >> w;) {
        int id = -1;
        if (const auto it = kItems.find(w); it != kItems.end()) id = it->second;
        else if (!w.empty() && w.find_first_not_of("0123456789abcdefx") == std::string::npos) id = static_cast<int>(std::strtol(w.c_str(), nullptr, 0));
        if (id < 1 || id > 0x24) return false;
        if (std::find(items.begin(), items.end(), id) == items.end()) items.push_back(id);
        any = true;
    }
    return any;
}

// a game level by its name (`kalebo`) or its number: -1 none
int level_by_name(std::string a) {
    for (char& c : a) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    static const std::map<std::string, int> kLevels = {{"veldin", 0}, {"novalis", 1}, {"aridia", 2}, {"kerwan", 3}, {"eudora", 4}, {"rilgar", 5},
                                                       {"blarg", 6}, {"umbris", 7}, {"batalia", 8}, {"gaspar", 9}, {"orxon", 10}, {"pokitaru", 11},
                                                       {"hoven", 12}, {"gemlik", 13}, {"oltanis", 14}, {"quartu", 15}, {"kalebo", 16}, {"fleet", 17},
                                                       {"veldin2", 18}};
    if (const auto it = kLevels.find(a); it != kLevels.end()) return it->second;
    if (!a.empty() && a.find_first_not_of("0123456789") == std::string::npos && std::atoi(a.c_str()) <= 18) return std::atoi(a.c_str());
    return -1;
}

// (stage 9) the hero line's generic hero code (the runtime's HERO_GENERIC, variants.h)
constexpr int kHeroGeneric = -2;

// (stage 9) `hero <level> [clank|giantclank|board]`: false when not understood (the level and mode checked later)
bool hero_line(std::istringstream& in, LevelTxt::Hero& h) {
    std::string a, mode;
    if (!(in >> a)) return false;
    // (the generic hero code, Veldin's: what Veldin, Gaspar and Umbris run; on a base with a build of its own, its stand-in)
    h.level = a == "generic" || a == "veldin" || a == "gaspar" || a == "umbris" ? kHeroGeneric : level_by_name(a);
    if (h.level == -1) return false;
    if (in >> mode) {
        if (mode != "clank" && mode != "giantclank" && mode != "board") return false;
        h.mode = mode;
    }
    std::string more;
    return !(in >> more);
}

// `object <class> <x> <y> <z> [<yaw>] [<key>=<value> ...]` (after its key)
bool object_line(std::istringstream& in, LevelTxt::Object& o) {
    if (!(in >> o.cls >> o.at[0] >> o.at[1] >> o.at[2])) return false;
    std::string w;
    while (in >> w) {
        if (w.find('=') != std::string::npos) {
            if (w.front() == '=') return false;
            o.params += (o.params.empty() ? "" : " ") + w;
        } else {
            char* end = nullptr;
            const float yaw = std::strtof(w.c_str(), &end);
            if (end == nullptr || *end != 0 || !o.params.empty()) return false;
            o.at[3] = yaw;
        }
    }
    return true;
}

bool read_level_txt(const fs::path& p, LevelTxt& t, std::string& err) {
    std::ifstream f(p);
    if (!f) {
        err = p.generic_string() + " cannot be read";
        return false;
    }
    std::string line;
    int n = 0;
    int sec = -1;  // (the section block the lines are in: -1 none, main's)
    while (std::getline(f, line)) {
        ++n;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        const size_t h = line.find_first_not_of(" \t");
        if (h == std::string::npos || line[h] == '#') continue;
        std::istringstream in(line);
        std::string key;
        in >> key;
        auto bad = [&] {
            err = fmt("%s:%d: \"%s\" not understood", p.generic_string().c_str(), n, line.c_str());
            return false;
        };
        if (sec >= 0) {  // (a section's block: its own lines)
            LevelTxt::Section& S = t.sections[static_cast<size_t>(sec)];
            if (key == "end") {
                sec = -1;
            } else if (key == "collision") {
                if (!(in >> S.collision)) return bad();
            } else if (key == "meshes") {
                if (!(in >> S.meshes)) return bad();
            } else if (key == "lights") {
                if (!(in >> S.lights)) return bad();
            } else if (key == "sky") {
                if (!(in >> S.sky_level) || S.sky_level < 0 || S.sky_level > 18) return bad();
            } else if (key == "music") {
                std::string a;
                if (!(in >> a)) return bad();
                if (a == "none") S.music_level = -1;
                else if (a.find_first_not_of("0123456789") == std::string::npos && std::atoi(a.c_str()) <= 18) S.music_level = std::atoi(a.c_str());
                else return bad();
            } else if (key == "background" || key == "fog") {
                int* c = key == "fog" ? S.fog : S.background;
                if (!(in >> c[0] >> c[1] >> c[2])) return bad();
                for (int i = 0; i < 3; ++i) c[i] = std::clamp(c[i], 0, 255);
                (key == "fog" ? S.has_fog : S.has_background) = true;
            } else if (key == "death_height") {
                if (!(in >> S.death)) return bad();
                S.has_death = true;
            } else if (key == "offset") {
                if (!(in >> S.offset[0] >> S.offset[1] >> S.offset[2])) return bad();
            } else if (key == "entry") {
                LevelTxt::Entry e;
                if (!place_line(in, e.name, e.at)) return bad();
                e.line = n;
                S.entries.push_back(e);
            } else if (key == "checkpoint") {
                LevelTxt::Checkpoint c;
                if (!place_line(in, c.name, c.at)) return bad();
                c.line = n;
                S.checkpoints.push_back(c);
            } else if (key == "moby") {
                LevelTxt::Moby m;
                if (!moby_line(in, m)) return bad();
                m.line = n;
                S.mobys.push_back(m);
            } else if (key == "object") {  // (a level class's object placed in the section: drawn while it is shown)
                LevelTxt::Object o;
                o.line = n;
                o.section = sec + 1;
                if (!object_line(in, o)) return bad();
                t.objects.push_back(o);
            } else if (key == "portal") {
                LevelTxt::Portal pt;
                if (!portal_line(in, pt)) return bad();
                pt.line = n;
                S.portals.push_back(pt);
            } else if (key == "respawn") {
                if (!(in >> S.respawn) || (S.respawn != "reset" && S.respawn != "keep")) return bad();
            } else if (key == "rail" || key == "rails") {
                LevelTxt::Rail r;
                if (!rail_line(in, r, key == "rails")) return bad();
                r.line = n;
                S.rails.push_back(r);
            } else if (key == "world") {
                std::string a;
                if (!(in >> a)) return bad();
                for (char& c : a) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                static const std::map<std::string, int> kWorlds = {{"veldin", 0}, {"novalis", 1}, {"aridia", 2}, {"kerwan", 3}, {"eudora", 4}, {"rilgar", 5},
                                                                   {"blarg", 6}, {"umbris", 7}, {"batalia", 8}, {"gaspar", 9}, {"orxon", 10}, {"pokitaru", 11},
                                                                   {"hoven", 12}, {"gemlik", 13}, {"oltanis", 14}, {"quartu", 15}, {"kalebo", 16}, {"fleet", 17},
                                                                   {"veldin2", 18}};
                if (const auto it = kWorlds.find(a); it != kWorlds.end()) S.world = it->second;
                else if (!a.empty() && a.find_first_not_of("0123456789") == std::string::npos) S.world = std::atoi(a.c_str());
                else return bad();
                if (S.world > 18) return bad();
                S.world_line = n;
            } else if (key == "objects") {
                std::string w;
                bool any = false;
                while (in >> w) {
                    any = true;
                    if (w == "all") S.objects_all = true;
                    else if (w != "none") S.objects.push_back(w);
                }
                if (!any) return bad();
            } else if (key == "zones") {
                std::string w;
                if (!(in >> w) || (w != "all" && w != "grid")) return bad();
                S.zones_all = w == "all";
                S.zones_line = n;
            } else if (key == "box") {
                if (!(in >> S.box_lo[0] >> S.box_lo[1] >> S.box_lo[2] >> S.box_hi[0] >> S.box_hi[1] >> S.box_hi[2])) return bad();
                for (int i = 0; i < 3; ++i)
                    if (S.box_lo[i] > S.box_hi[i]) std::swap(S.box_lo[i], S.box_hi[i]);
                S.has_box = true;
            } else if (key == "hero") {
                if (!hero_line(in, S.hero)) return bad();
                S.hero.line = n;
            } else if (key == "boss") {
                std::string w;
                bool any = false;
                while (in >> w) any = true, S.bosses.push_back(w);
                if (!any) return bad();
                S.boss_line = n;
            } else {
                err = fmt("%s:%d: \"%s\": not a line of a section (its own: collision, meshes, lights, sky, music, fog, background, death_height, offset, "
                          "entry, checkpoint, moby, object, portal, respawn, rail, rails, world, objects, box, zones, hero, boss; end closes it)",
                          p.generic_string().c_str(), n, line.c_str());
                return false;
            }
            continue;
        }
        if (key == "section") {
            LevelTxt::Section S;
            S.line = n;
            if (!(in >> S.name) || !sdk_name_ok(S.name)) return bad();
            std::string w;
            while (in >> w) {
                if (w == "remove" || w == "freeze" || w == "keep") S.policy = w;
                else if (w == "respawn" && in >> S.respawn && (S.respawn == "reset" || S.respawn == "keep")) continue;
                else if (w == "fresh") S.fresh = true;
                else return bad();
            }
            for (const LevelTxt::Section& o : t.sections)
                if (o.name == S.name) {
                    err = fmt("%s:%d: the section \"%s\" twice", p.generic_string().c_str(), n, S.name.c_str());
                    return false;
                }
            if (S.name == "main") {
                err = fmt("%s:%d: \"main\" is the lines outside any section block (the ship's): give the section another name", p.generic_string().c_str(), n);
                return false;
            }
            t.sections.push_back(S);
            sec = static_cast<int>(t.sections.size()) - 1;
            continue;
        }
        if (key == "entry") {
            LevelTxt::Entry e;
            if (!place_line(in, e.name, e.at)) return bad();
            e.line = n;
            t.entries.push_back(e);
            continue;
        }
        if (key == "portal") {
            LevelTxt::Portal pt;
            if (!portal_line(in, pt)) return bad();
            pt.line = n;
            t.portals.push_back(pt);
            continue;
        }
        if (key == "start") {
            std::string w;
            if (!(in >> w)) return bad();
            const size_t c = w.find(':');
            if (c == std::string::npos || c == 0 || c + 1 == w.size()) return bad();
            t.start_section = w.substr(0, c);
            t.start_entry = w.substr(c + 1);
            t.start_line = n;
            continue;
        }
        if (key == "leave") {
            if (!(in >> t.leave) || (t.leave != "remove" && t.leave != "freeze" && t.leave != "keep")) return bad();
            t.leave_line = n;
            continue;
        }
        if (key == "name" || key.rfind("name.", 0) == 0) {  // (the game's too: src/game/enhance/level_slots.cpp; here the banner's)
            std::string rest;
            std::getline(in >> std::ws, rest);
            t.names[key.size() > 4 ? key.substr(5) : std::string()] = rest;
            continue;
        }
        if (key.rfind("region", 0) == 0) continue;  // (the game's: src/game/enhance/level_slots.cpp)
        if (key == "picture" || key == "map_picture" || key == "banner" || key.rfind("banner.", 0) == 0) {
            std::string f;
            if (!(in >> f)) return bad();
            const std::string lang = key.size() > 6 && key[6] == '.' ? key.substr(7) : std::string();
            if (key.rfind("banner.", 0) == 0 && lang != "fr" && lang != "de" && lang != "es" && lang != "it") return bad();
            (key == "picture" ? t.picture : key == "map_picture" ? t.map_picture : t.banners[lang]) = f;
            continue;
        }
        if (key == "hero") {
            std::string w;
            in >> w;
            // (stage 9) `hero <planet>|generic [mode]`: the hero code (a name, or one number); else Ratchet's start (x y z [yaw], auto)
            {
                std::istringstream all(line.substr(line.find(key) + key.size()));
                std::vector<std::string> ws;
                for (std::string x; all >> x;) ws.push_back(x);
                if (w != "auto" && ws.size() < 3) {
                    std::istringstream again(line.substr(line.find(key) + key.size()));
                    if (!hero_line(again, t.hero_ctx)) return bad();
                    t.hero_ctx.line = n;
                    continue;
                }
            }
            t.has_hero = true;
            t.hero_auto = w == "auto";
            if (!t.hero_auto) {
                std::istringstream in2(line.substr(line.find(key) + key.size()));
                if (!(in2 >> t.hero[0] >> t.hero[1] >> t.hero[2])) return bad();
                if (!(in2 >> t.hero[3])) t.hero[3] = 0;
            }
        } else if (key == "ship") {
            std::string w;
            in >> w;
            if (w == "none") {
                t.ship = false;
            } else {
                std::istringstream in2(line.substr(line.find(key) + key.size()));
                if (!(in2 >> t.ship_at[0] >> t.ship_at[1] >> t.ship_at[2] >> t.ship_at[3])) return bad();
                t.ship = true;
            }
        } else if (key == "update_all") {
            t.update_all = true;
        } else if (key == "death_height") {
            if (!(in >> t.death)) return bad();
            t.has_death = true;
        } else if (key == "background" || key == "fog") {
            int* c = key == "fog" ? t.fog : t.background;
            if (!(in >> c[0] >> c[1] >> c[2])) return bad();
            for (int i = 0; i < 3; ++i) c[i] = std::clamp(c[i], 0, 255);
            (key == "fog" ? t.has_fog : t.has_background) = true;
        } else if (key == "collision") {
            if (!(in >> t.collision)) return bad();
        } else if (key == "collision_type") {
            if (!(in >> t.collision_type) || t.collision_type < 0 || t.collision_type > 255) return bad();
        } else if (key == "meshes") {
            if (!(in >> t.meshes)) return bad();
        } else if (key == "up") {
            std::string a;
            in >> a;
            if (a != "y" && a != "z") return bad();
            t.y_up = a == "y";
        } else if (key == "scale") {
            if (!(in >> t.scale) || !(t.scale > 0)) return bad();
        } else if (key == "moby") {
            LevelTxt::Moby m;
            if (!moby_line(in, m)) return bad();
            m.line = n;
            t.mobys.push_back(m);
        } else if (key == "session") {
            t.session_line = n;
        } else if (key == "checkpoint") {
            LevelTxt::Checkpoint c;
            if (!(in >> c.name >> c.at[0] >> c.at[1] >> c.at[2]) || !sdk_name_ok(c.name)) return bad();
            if (!(in >> c.at[3])) c.at[3] = 0;
            for (const LevelTxt::Checkpoint& o : t.checkpoints)
                if (o.name == c.name) {
                    err = fmt("%s:%d: the checkpoint \"%s\" twice", p.generic_string().c_str(), n, c.name.c_str());
                    return false;
                }
            c.line = n;
            t.checkpoints.push_back(c);
        } else if (key == "respawn") {
            if (!(in >> t.respawn) || (t.respawn != "reset" && t.respawn != "keep")) return bad();
            t.respawn_line = n;
        } else if (key == "music") {
            std::string a, b;
            in >> a >> b;
            t.music_level = -1;
            t.music_none = false;
            t.music_loop.clear();
            t.music_intro.clear();
            if (a == "none") {
                t.music_none = true;
            } else if (!a.empty() && a.find_first_not_of("0123456789") == std::string::npos) {
                t.music_level = std::atoi(a.c_str());
                if (t.music_level > 18) return bad();
            } else if (!a.empty()) {
                t.music_loop = a;
                t.music_intro = b;
            } else {
                return bad();
            }
        } else if (key == "offset") {
            std::string a;
            in >> a;
            if (a == "auto") {
                t.offset_auto = true;
            } else {
                std::istringstream in2(line.substr(line.find(key) + key.size()));
                if (!(in2 >> t.offset[0] >> t.offset[1] >> t.offset[2])) return bad();
                t.offset_auto = false;
            }
        } else if (key == "sky") {
            if (!(in >> t.sky_level) || t.sky_level < 0 || t.sky_level > 18) return bad();
        } else if (key == "base") {
            std::string a;
            in >> a;
            for (char& c : a) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            static const std::map<std::string, int> kNames = {{"veldin", 0}, {"novalis", 1}, {"aridia", 2}, {"kerwan", 3}, {"eudora", 4}, {"rilgar", 5},
                                                              {"blarg", 6}, {"umbris", 7}, {"batalia", 8}, {"gaspar", 9}, {"orxon", 10}, {"pokitaru", 11},
                                                              {"hoven", 12}, {"gemlik", 13}, {"oltanis", 14}, {"quartu", 15}, {"kalebo", 16}, {"fleet", 17},
                                                              {"veldin2", 18}};
            if (const auto it = kNames.find(a); it != kNames.end()) t.base = it->second;
            else if (!a.empty() && a.find_first_not_of("0123456789") == std::string::npos) t.base = std::atoi(a.c_str());
            else return bad();
            if (t.base > 18) return bad();
        } else if (key == "spawn") {
            LevelTxt::Pool pl;
            pl.line = n;
            if (!(in >> pl.cls >> pl.count) || pl.cls <= 0 || pl.count <= 0 || pl.count > 64) return bad();
            t.pools.push_back(pl);
        } else if (key == "class") {
            LevelTxt::Class c;
            c.line = n;
            if (!(in >> c.name >> c.model) || !sdk_name_ok(c.name)) return bad();
            for (const LevelTxt::Class& o : t.classes)
                if (o.name == c.name) {
                    err = fmt("%s:%d: the class \"%s\" twice", p.generic_string().c_str(), n, c.name.c_str());
                    return false;
                }
            std::string w;
            while (in >> w) {
                if (w == "size") {
                    if (!(in >> c.size) || !(c.size > 0)) return bad();
                } else if (w == "scale") {
                    if (!(in >> c.scale) || !(c.scale > 0)) return bad();
                } else if (w == "ghost" || w == "solid") {
                    c.ghost = w == "ghost";
                } else if (w == "collision") {
                    if (!(in >> c.collision)) return bad();
                } else {
                    return bad();
                }
            }
            t.classes.push_back(c);
        } else if (key == "object") {
            LevelTxt::Object o;
            o.line = n;
            if (!object_line(in, o)) return bad();
            t.objects.push_back(o);
        } else if (key == "sound") {
            LevelTxt::Sound d;
            d.line = n;
            if (!(in >> d.name >> d.file) || !sdk_name_ok(d.name)) return bad();
            for (const LevelTxt::Sound& o : t.sounds)
                if (o.name == d.name) {
                    err = fmt("%s:%d: the sound \"%s\" twice", p.generic_string().c_str(), n, d.name.c_str());
                    return false;
                }
            std::string w;
            while (in >> w) {
                if (w == "volume") {
                    if (!(in >> d.volume) || d.volume < 0 || d.volume > 1) return bad();
                } else if (w == "range") {
                    if (!(in >> d.near >> d.far) || d.near < 0 || !(d.far > d.near)) return bad();
                } else if (w == "pitch") {
                    if (!(in >> d.pitch) || std::fabs(d.pitch) > 24) return bad();
                } else if (w == "loop") {
                    d.loop = true;
                } else {
                    return bad();
                }
            }
            t.sounds.push_back(d);
        } else if (key == "import") {
            LevelTxt::Import im;
            im.line = n;
            if (!(in >> im.name)) return bad();
            t.imports.push_back(im);
        } else if (key == "path") {
            LevelTxt::Path pa;
            if (!(in >> pa.name) || !sdk_name_ok(pa.name)) return bad();
            for (const LevelTxt::Path& o : t.paths)
                if (o.name == pa.name) {
                    err = fmt("%s:%d: the path \"%s\" twice", p.generic_string().c_str(), n, pa.name.c_str());
                    return false;
                }
            std::array<float, 3> v{};
            while (in >> v[0]) {
                if (!(in >> v[1] >> v[2])) return bad();
                pa.pts.push_back(v);
            }
            if (pa.pts.empty() || pa.pts.size() > 64) return bad();
            t.paths.push_back(pa);
        } else if (key == "rail" || key == "rails") {
            LevelTxt::Rail r;
            if (!rail_line(in, r, key == "rails")) return bad();
            r.line = n;
            t.rails.push_back(r);
        } else if (key == "items") {
            if (!items_line(in, t.items)) return bad();
        } else if (key == "trigger") {
            LevelTxt::Trigger g;
            if (!(in >> g.name >> g.lo[0] >> g.lo[1] >> g.lo[2] >> g.hi[0] >> g.hi[1] >> g.hi[2]) || !sdk_name_ok(g.name)) return bad();
            t.triggers.push_back(g);
        } else {
            return bad();
        }
    }
    if (sec >= 0) {
        err = fmt("%s: the section \"%s\" (line %d) has no end line", p.generic_string().c_str(), t.sections[static_cast<size_t>(sec)].name.c_str(),
                  t.sections[static_cast<size_t>(sec)].line);
        return false;
    }
    if (!t.has_hero) t.hero_auto = t.ship;  // (no hero line: where he steps off the ship)
    if (t.hero_auto && !t.ship) {
        err = p.generic_string() + ": no \"hero <x> <y> <z>\" line (where Ratchet starts; \"hero auto\" needs a ship)";
        return false;
    }
    for (const LevelTxt::Object& o : t.objects) {
        bool found = false;
        for (const LevelTxt::Class& c : t.classes) found = found || c.name == o.cls;
        if (!found) {
            err = fmt("%s:%d: no class \"%s\" (a class line names it, before or after)", p.generic_string().c_str(), o.line, o.cls.c_str());
            return false;
        }
    }
    if (t.sounds.size() > 64) {
        err = p.generic_string() + ": more than 64 sounds";
        return false;
    }
    return true;
}

// ---- OBJ ----
struct ObjVert {
    float p[3];
    float uv[2];
    float col[3];  // its colour (an OBJ's "v x y z r g b"; 1 1 1 without)
};
struct ObjTri {
    ObjVert v[3];
    int material = -1;  // index into Obj::materials (-1: none)
    int object = 0;     // index into Obj::objects
};
struct ObjMaterial {
    std::string name;
    float kd[3] = {0.8f, 0.8f, 0.8f};
    std::string map;  // map_Kd's file (relative to the .mtl)
    fs::path dir;
};
struct Obj {
    std::vector<ObjTri> tris;
    std::vector<ObjMaterial> materials;
    std::vector<std::string> objects = {"(default)"};
    // (stage 7) the line elements (`l a b c ...`): per object, its edges between the file's vertices (positions moved as the faces')
    std::vector<std::array<float, 3>> line_verts;
    std::vector<std::pair<int, std::array<int, 2>>> edges;  // (object, its two vertices)
};

int obj_index(const std::string& s, size_t n) {
    if (s.empty()) return -1;
    const int i = std::atoi(s.c_str());
    if (i > 0) return i - 1;
    if (i < 0) return static_cast<int>(n) + i;
    return -1;
}

bool read_mtl(const fs::path& p, Obj& o, std::string& err) {
    std::ifstream f(p);
    if (!f) {
        err = p.generic_string() + " cannot be read";
        return false;
    }
    std::string line;
    ObjMaterial* cur = nullptr;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        std::istringstream in(line);
        std::string k;
        in >> k;
        if (k == "newmtl") {
            ObjMaterial m;
            std::getline(in >> std::ws, m.name);
            m.dir = p.parent_path();
            o.materials.push_back(m);
            cur = &o.materials.back();
        } else if (cur != nullptr && k == "Kd") {
            in >> cur->kd[0] >> cur->kd[1] >> cur->kd[2];
        } else if (cur != nullptr && k == "map_Kd") {
            std::string rest;
            std::getline(in >> std::ws, rest);
            const size_t sp = rest.find_last_of(' ');  // (options before the file's name)
            cur->map = sp == std::string::npos ? rest : rest.substr(sp + 1);
        }
    }
    return true;
}

// reads an OBJ (positions converted to game units and axes)
bool read_obj(const fs::path& p, const LevelTxt& lt, bool with_materials, Obj& o, std::string& err) {
    std::ifstream f(p);
    if (!f) {
        err = p.generic_string() + " cannot be read";
        return false;
    }
    std::vector<std::array<float, 3>> vs, cols;
    std::vector<std::array<float, 2>> vts;
    std::string line;
    int material = -1, object = 0;
    int n = 0;
    while (std::getline(f, line)) {
        ++n;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        std::istringstream in(line);
        std::string k;
        in >> k;
        if (k == "v") {
            float x = 0, y = 0, z = 0;
            in >> x >> y >> z;
            if (lt.y_up) vs.push_back({x * lt.scale + lt.offset[0], -z * lt.scale + lt.offset[1], y * lt.scale + lt.offset[2]});
            else vs.push_back({x * lt.scale + lt.offset[0], y * lt.scale + lt.offset[1], z * lt.scale + lt.offset[2]});
            float r = 1, g = 1, b = 1;
            if (!(in >> r >> g >> b)) r = g = b = 1;
            cols.push_back({std::clamp(r, 0.0f, 1.0f), std::clamp(g, 0.0f, 1.0f), std::clamp(b, 0.0f, 1.0f)});
        } else if (k == "vt") {
            float u = 0, v = 0;
            in >> u >> v;
            vts.push_back({u, v});
        } else if (k == "mtllib" && with_materials) {
            std::string name;
            std::getline(in >> std::ws, name);
            if (!read_mtl(p.parent_path() / name, o, err)) return false;
        } else if (k == "usemtl") {
            std::string name;
            std::getline(in >> std::ws, name);
            material = -1;
            for (size_t i = 0; i < o.materials.size(); ++i)
                if (o.materials[i].name == name) material = static_cast<int>(i);
            if (material < 0) {  // (a name with no .mtl entry: kept by name, its default colour)
                ObjMaterial m;
                m.name = name;
                o.materials.push_back(m);
                material = static_cast<int>(o.materials.size()) - 1;
            }
        } else if (k == "o" || k == "g") {
            std::string name;
            std::getline(in >> std::ws, name);
            o.objects.push_back(name);
            object = static_cast<int>(o.objects.size()) - 1;
        } else if (k == "l") {
            std::vector<int> ids;
            std::string w;
            while (in >> w) {
                const int vi = obj_index(w.substr(0, w.find('/')), vs.size());
                if (vi < 0 || vi >= static_cast<int>(vs.size())) {
                    err = fmt("%s:%d: a line's vertex that is not there", p.generic_string().c_str(), n);
                    return false;
                }
                ids.push_back(vi);
            }
            if (o.line_verts.size() < vs.size()) o.line_verts = vs;
            for (size_t i = 1; i < ids.size(); ++i) o.edges.push_back({object, {ids[i - 1], ids[i]}});
        } else if (k == "f") {
            std::vector<ObjVert> poly;
            std::string w;
            while (in >> w) {
                const size_t s1 = w.find('/');
                const int vi = obj_index(w.substr(0, s1), vs.size());
                int ti = -1;
                if (s1 != std::string::npos) {
                    const size_t s2 = w.find('/', s1 + 1);
                    ti = obj_index(w.substr(s1 + 1, s2 == std::string::npos ? std::string::npos : s2 - s1 - 1), vts.size());
                }
                if (vi < 0 || vi >= static_cast<int>(vs.size()) || ti >= static_cast<int>(vts.size())) {
                    err = fmt("%s:%d: a face's vertex that is not there", p.generic_string().c_str(), n);
                    return false;
                }
                ObjVert v{};
                std::memcpy(v.p, vs[static_cast<size_t>(vi)].data(), sizeof v.p);
                std::memcpy(v.col, cols[static_cast<size_t>(vi)].data(), sizeof v.col);
                if (ti >= 0) std::memcpy(v.uv, vts[static_cast<size_t>(ti)].data(), sizeof v.uv);
                poly.push_back(v);
            }
            for (size_t i = 2; i < poly.size(); ++i) {  // (a fan)
                ObjTri t;
                t.v[0] = poly[0];
                t.v[1] = poly[i - 1];
                t.v[2] = poly[i];
                t.material = material;
                t.object = object;
                o.tris.push_back(t);
            }
        }
    }
    return true;
}

// ---- the collision (level_collision.h's layout) ----
struct CTri {
    std::array<std::array<float, 3>, 3> p;
    uint8_t type = 31;
    bool whole = false;  // (stage 10: a game level's own face: kept whole, as its level has it in its cells)
};

float dist3(const std::array<float, 3>& a, const std::array<float, 3>& b) {
    return std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
}

// triangles split until no edge is longer than `max_edge` (a cell's points lie within 32 units of its centre)
void subdivide(const CTri& t, float max_edge, std::vector<CTri>& out, int depth = 0) {
    float e = 0;
    for (int i = 0; i < 3; ++i) e = std::max(e, dist3(t.p[static_cast<size_t>(i)], t.p[static_cast<size_t>((i + 1) % 3)]));
    if (e <= max_edge || depth > 16) {
        out.push_back(t);
        return;
    }
    std::array<std::array<float, 3>, 3> m;
    for (int i = 0; i < 3; ++i)
        for (int l = 0; l < 3; ++l) m[static_cast<size_t>(i)][static_cast<size_t>(l)] = 0.5f * (t.p[static_cast<size_t>(i)][static_cast<size_t>(l)] + t.p[static_cast<size_t>((i + 1) % 3)][static_cast<size_t>(l)]);
    const CTri parts[4] = {{{t.p[0], m[0], m[2]}, t.type}, {{m[0], t.p[1], m[1]}, t.type}, {{m[2], m[1], t.p[2]}, t.type}, {{m[0], m[1], m[2]}, t.type}};
    for (const CTri& c : parts) subdivide(c, max_edge, out, depth + 1);
}

// a triangle against an axis-aligned box (separating axes: the box's, the triangle's normal, the nine edge crosses)
bool tri_box(const CTri& t, const float c[3], float h) {
    float v[3][3];
    for (int i = 0; i < 3; ++i)
        for (int l = 0; l < 3; ++l) v[i][l] = t.p[static_cast<size_t>(i)][static_cast<size_t>(l)] - c[l];
    for (int l = 0; l < 3; ++l) {
        const float lo = std::min({v[0][l], v[1][l], v[2][l]}), hi = std::max({v[0][l], v[1][l], v[2][l]});
        if (lo > h || hi < -h) return false;
    }
    float e[3][3];
    for (int i = 0; i < 3; ++i)
        for (int l = 0; l < 3; ++l) e[i][l] = v[(i + 1) % 3][l] - v[i][l];
    auto test = [&](const float a[3]) {
        const float p0 = a[0] * v[0][0] + a[1] * v[0][1] + a[2] * v[0][2];
        const float p1 = a[0] * v[1][0] + a[1] * v[1][1] + a[2] * v[1][2];
        const float p2 = a[0] * v[2][0] + a[1] * v[2][1] + a[2] * v[2][2];
        const float r = h * (std::fabs(a[0]) + std::fabs(a[1]) + std::fabs(a[2]));
        return !(std::min({p0, p1, p2}) > r || std::max({p0, p1, p2}) < -r);
    };
    const float n[3] = {e[0][1] * e[1][2] - e[0][2] * e[1][1], e[0][2] * e[1][0] - e[0][0] * e[1][2], e[0][0] * e[1][1] - e[0][1] * e[1][0]};
    if (!test(n)) return false;
    for (int i = 0; i < 3; ++i) {
        const float ax[3][3] = {{0, -e[i][2], e[i][1]}, {e[i][2], 0, -e[i][0]}, {-e[i][1], e[i][0], 0}};
        for (const auto& a : ax)
            if (!test(a)) return false;
    }
    return true;
}

struct CCell {
    std::vector<uint32_t> points;                // the packed words
    std::vector<std::array<uint8_t, 4>> faces;   // three point indices, the type
};

bool build_collision(const std::vector<CTri>& in, std::vector<uint8_t>& out, Log& log, std::string& err) {
    std::vector<CTri> tris;
    for (const CTri& t : in) {
        if (t.whole) tris.push_back(t);
        else subdivide(t, 6.0f, tris);
    }
    std::map<std::tuple<int, int, int>, CCell> cells;  // (z, y, x)
    size_t refs = 0;
    for (const CTri& t : tris) {
        int lo[3], hi[3];
        for (int l = 0; l < 3; ++l) {
            const float a = std::min({t.p[0][static_cast<size_t>(l)], t.p[1][static_cast<size_t>(l)], t.p[2][static_cast<size_t>(l)]});
            const float b = std::max({t.p[0][static_cast<size_t>(l)], t.p[1][static_cast<size_t>(l)], t.p[2][static_cast<size_t>(l)]});
            lo[l] = static_cast<int>(std::floor((a - 0.01f) / 4.0f));
            hi[l] = static_cast<int>(std::floor((b + 0.01f) / 4.0f));
            if (lo[l] < 0) {  // (the game finds a point's cell by truncating, so the grid's cells below 0 are never looked at)
                err = fmt("the collision reaches %c %.2f: the game's collision grid starts at 0, every coordinate must be above 0 (move the "
                          "level, its start and its ship there)", "xyz"[l], a);
                return false;
            }
        }
        for (int z = lo[2]; z <= hi[2]; ++z)
            for (int y = lo[1]; y <= hi[1]; ++y)
                for (int x = lo[0]; x <= hi[0]; ++x) {
                    const float c[3] = {x * 4.0f + 2.0f, y * 4.0f + 2.0f, z * 4.0f + 2.0f};
                    if (!tri_box(t, c, 2.01f)) continue;
                    if (std::abs(x) > 0x7ff0 || std::abs(y) > 0x7ff0 || std::abs(z) > 0x7ff0) {
                        err = "the collision reaches past the game's grid (32000 cells of 4 units)";
                        return false;
                    }
                    CCell& cell = cells[{z, y, x}];
                    std::array<uint8_t, 4> face{};
                    bool degenerate = false;
                    for (int i = 0; i < 3; ++i) {
                        const auto& p = t.p[static_cast<size_t>(i)];
                        const int qx = static_cast<int>(std::lround((p[0] - c[0]) * 16.0f));
                        const int qy = static_cast<int>(std::lround((p[1] - c[1]) * 16.0f));
                        const int qz = static_cast<int>(std::lround((p[2] - c[2]) * 64.0f));
                        if (std::abs(qx) > 511 || std::abs(qy) > 511 || std::abs(qz) > 2047) {
                            err = "a collision point past a cell's reach (internal)";
                            return false;
                        }
                        const uint32_t w = (static_cast<uint32_t>(qx) & 0x3ffu) | (static_cast<uint32_t>(qy) & 0x3ffu) << 10 |
                                           (static_cast<uint32_t>(qz) & 0xfffu) << 20;
                        auto it = std::find(cell.points.begin(), cell.points.end(), w);
                        if (it == cell.points.end()) {
                            cell.points.push_back(w);
                            it = cell.points.end() - 1;
                        }
                        face[static_cast<size_t>(i)] = static_cast<uint8_t>(it - cell.points.begin());
                    }
                    degenerate = face[0] == face[1] || face[1] == face[2] || face[0] == face[2];
                    if (degenerate) continue;
                    // the game's faces turn clockwise seen from the side they face (an OBJ's turn counter-clockwise)
                    std::swap(face[1], face[2]);
                    face[3] = t.type;
                    cell.faces.push_back(face);
                    ++refs;
                    if (cell.points.size() > 255 || cell.faces.size() > 0xffff) {
                        err = fmt("the collision has too many points in the cell at (%d, %d, %d): over 255", x * 4, y * 4, z * 4);
                        return false;
                    }
                }
    }
    if (cells.empty()) {
        err = "the collision mesh has no faces";
        return false;
    }
    // the layout: the head (the grid's offset, the hero groups' offset), the grid at 0x40: z0, zn, the z rows' offsets / 4;
    // the z rows (y0, yn, the y rows' offsets), the y rows (x0, xn, the cells' offsets << 8 | their size / 16), the cells
    // (16-byte aligned), then the hero groups' list (empty)
    std::map<int, std::map<int, std::map<int, const CCell*>>> grid;
    for (auto& [k, c] : cells) {
        if (c.faces.empty()) continue;
        grid[std::get<0>(k)][std::get<1>(k)][std::get<2>(k)] = &c;
    }
    const int z0 = grid.begin()->first, z1 = grid.rbegin()->first;
    std::vector<uint8_t> g;
    auto put16 = [&g](size_t at, uint32_t v) {
        if (at + 2 > g.size()) g.resize(at + 2, 0);
        g[at] = static_cast<uint8_t>(v);
        g[at + 1] = static_cast<uint8_t>(v >> 8);
    };
    auto put32 = [&g](size_t at, uint32_t v) {
        if (at + 4 > g.size()) g.resize(at + 4, 0);
        for (int i = 0; i < 4; ++i) g[at + static_cast<size_t>(i)] = static_cast<uint8_t>(v >> (8 * i));
    };
    const int zn = z1 - z0 + 1;
    put16(0, static_cast<uint32_t>(static_cast<int16_t>(z0)) & 0xffff);
    put16(2, static_cast<uint32_t>(zn));
    size_t at = align_up(4 + 2 * static_cast<size_t>(zn), 4);
    std::map<int, size_t> zrow_at;
    for (auto& [z, ys] : grid) {  // the z rows
        zrow_at[z] = at;
        const int y0 = ys.begin()->first, yn = ys.rbegin()->first - y0 + 1;
        if (at / 4 > 0xffff) {
            err = "the collision's grid is too large (its z rows past 256 KB)";
            return false;
        }
        put16(4 + 2 * static_cast<size_t>(z - z0), static_cast<uint32_t>(at / 4));
        put16(at, static_cast<uint32_t>(static_cast<int16_t>(y0)) & 0xffff);
        put16(at + 2, static_cast<uint32_t>(yn));
        at = align_up(at + 4 + 4 * static_cast<size_t>(yn), 4);
    }
    std::map<std::pair<int, int>, size_t> yrow_at;
    for (auto& [z, ys] : grid)  // the y rows
        for (auto& [y, xs] : ys) {
            yrow_at[{z, y}] = at;
            const int x0 = xs.begin()->first, xn = xs.rbegin()->first - x0 + 1;
            put32(zrow_at[z] + 4 + 4 * static_cast<size_t>(y - ys.begin()->first), static_cast<uint32_t>(at));
            put16(at, static_cast<uint32_t>(static_cast<int16_t>(x0)) & 0xffff);
            put16(at + 2, static_cast<uint32_t>(xn));
            at = align_up(at + 4 + 4 * static_cast<size_t>(xn), 4);
        }
    size_t faces = 0, points = 0, ncells = 0;
    for (auto& [z, ys] : grid)  // the cells
        for (auto& [y, xs] : ys)
            for (auto& [x, c] : xs) {
                at = align_up(at, 16);
                const size_t size = 4 + 4 * c->points.size() + 4 * c->faces.size();
                if (at > 0xffffff) {
                    err = "the collision is too large (past 16 MB)";
                    return false;
                }
                put32(yrow_at[{z, y}] + 4 + 4 * static_cast<size_t>(x - xs.begin()->first), static_cast<uint32_t>(at << 8 | ((size + 15) / 16)));
                put16(at, static_cast<uint32_t>(c->faces.size()));
                g.resize(std::max(g.size(), at + size), 0);
                g[at + 2] = static_cast<uint8_t>(c->points.size());
                g[at + 3] = 0;  // (no quads)
                size_t p = at + 4;
                for (uint32_t w : c->points) put32(p, w), p += 4;
                for (const auto& f : c->faces) {
                    std::memcpy(g.data() + p, f.data(), 4);
                    p += 4;
                }
                at += size;
                faces += c->faces.size();
                points += c->points.size();
                ++ncells;
            }
    at = align_up(at, 16);
    const size_t heroes = at;
    g.resize(heroes + 0x10, 0);  // (the hero groups' list: none)
    out.assign(0x40, 0);
    wr32(out, 0, 0x40);
    wr32(out, 4, static_cast<int32_t>(0x40 + heroes));
    out.insert(out.end(), g.begin(), g.end());
    log.say(fmt("collision: %zu triangles (%zu after splitting), %zu cells, %zu faces (%zu references), %zu points, %zu bytes", in.size(), tris.size(), ncells, faces,
                refs, points, out.size()));
    return true;
}

// ---- (stage 7) grind rails: the gameplay file's block +0x74 (level_load.cpp's "grind lines"; STUDY 5.1) ----
// The block: n, the offset of the points' data, its size, 0; n records of 0x20 (the bounding sphere x y z r, the loop flag,
// 12 bytes); n words (each rail's path: its offset in the points' data); the points' data: per path its count, 12 bytes, then
// its points (x y z, w the length to the next one; a loop's last one to its first)
constexpr float kRailStep = 1.5f;   // a rail's points at most this far apart (the arena's rails: 1.5)
constexpr size_t kRailPoints = 2048, kRails = 64;
constexpr float kRailStrip = 0.5f;  // the collision strip under a rail (the grind lets go with nothing under Ratchet's feet)

// the rails' files read (each OBJ object of line elements a rail) and the lines' points moved by `off`; densified
bool rails_resolve(const fs::path& ldir, const LevelTxt& lt, const float off[3], std::vector<LevelTxt::Rail>& rails, std::string& err) {
    std::vector<LevelTxt::Rail> out;
    for (const LevelTxt::Rail& r : rails) {
        if (r.file.empty()) {
            LevelTxt::Rail q = r;
            for (auto& v : q.pts)
                for (size_t l = 0; l < 3; ++l) v[l] += off[l];
            out.push_back(q);
            continue;
        }
        LevelTxt slt = lt;
        for (int l = 0; l < 3; ++l) slt.offset[l] = off[l];
        Obj o;
        if (!read_obj(ldir / r.file, slt, false, o, err)) return false;
        std::map<int, std::vector<std::array<int, 2>>> by_object;
        for (const auto& [ob, e] : o.edges) by_object[ob].push_back(e);
        if (by_object.empty()) {
            err = fmt("level.txt:%d: %s has no line elements (`l`: a curve converted to a mesh, its edges)", r.line, r.file.c_str());
            return false;
        }
        for (const auto& [ob, es] : by_object) {
            std::map<int, std::vector<int>> adj;
            for (const auto& e : es) {
                if (e[0] == e[1]) continue;
                adj[e[0]].push_back(e[1]);
                adj[e[1]].push_back(e[0]);
            }
            std::set<int> seen;
            int part = 0;
            for (const auto& [v0, nb0] : adj) {
                if (seen.count(v0) != 0) continue;
                // the chain's component: its ends (degree 1); none: a loop
                std::vector<int> comp{v0};
                std::set<int> cs{v0};
                for (size_t k = 0; k < comp.size(); ++k)
                    for (int w : adj[comp[k]])
                        if (cs.insert(w).second) comp.push_back(w);
                int start = -1;
                for (int v : comp) {
                    if (adj[v].size() > 2) {
                        err = fmt("%s: the object %s branches (a rail is one line)", r.file.c_str(), o.objects[static_cast<size_t>(ob)].c_str());
                        return false;
                    }
                    if (adj[v].size() == 1 && (start < 0 || v < start)) start = v;
                }
                const bool closed = start < 0;
                if (closed) start = *std::min_element(comp.begin(), comp.end());
                LevelTxt::Rail q = r;
                q.file.clear();
                q.loop = r.loop || closed;
                q.name = o.objects[static_cast<size_t>(ob)] + (part > 0 ? "_" + std::to_string(part + 1) : std::string());
                for (char& c : q.name)
                    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.')) c = '_';
                ++part;
                int prev = -1, cur = start;
                while (cur >= 0 && seen.insert(cur).second) {
                    q.pts.push_back(o.line_verts[static_cast<size_t>(cur)]);
                    int next = -1;
                    for (int w : adj[cur])
                        if (w != prev && seen.count(w) == 0) next = w;
                    prev = cur;
                    cur = next;
                }
                if (q.pts.size() >= 2) out.push_back(q);
            }
        }
    }
    for (LevelTxt::Rail& q : out) {  // densified (the game's closest-point search walks a few points either way)
        std::vector<std::array<float, 3>> d;
        const size_t n = q.pts.size();
        for (size_t i = 0; i < n; ++i) {
            if (!d.empty() && dist3(d.back(), q.pts[i]) < 1e-3f) continue;
            if (!d.empty()) {
                const std::array<float, 3> a = d.back(), b = q.pts[i];
                const int k = static_cast<int>(std::ceil(dist3(a, b) / kRailStep));
                for (int j = 1; j < k; ++j) {
                    const float t = static_cast<float>(j) / static_cast<float>(k);
                    d.push_back({a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t});
                }
            }
            d.push_back(q.pts[i]);
        }
        if (q.loop && d.size() > 2 && dist3(d.front(), d.back()) < 1e-3f) d.pop_back();  // (a loop's end on its start)
        if (q.loop && d.size() > 2) {  // the closing segment's points
            const std::array<float, 3> a = d.back(), b = d.front();
            const int k = static_cast<int>(std::ceil(dist3(a, b) / kRailStep));
            for (int j = 1; j < k; ++j) {
                const float t = static_cast<float>(j) / static_cast<float>(k);
                d.push_back({a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t});
            }
        }
        if (d.size() < 2) {
            err = fmt("level.txt:%d: the rail %s has less than two points", q.line, q.name.c_str());
            return false;
        }
        if (d.size() > kRailPoints) {
            err = fmt("level.txt:%d: the rail %s is too long (%zu points of %.1f units, at most %zu)", q.line, q.name.c_str(), d.size(), kRailStep, kRailPoints);
            return false;
        }
        q.pts = d;
    }
    rails = out;
    return true;
}

float rail_length(const LevelTxt::Rail& r) {
    float len = 0;
    for (size_t i = 1; i < r.pts.size(); ++i) len += dist3(r.pts[i - 1], r.pts[i]);
    if (r.loop) len += dist3(r.pts.back(), r.pts.front());
    return len;
}

std::vector<uint8_t> grind_block(const std::vector<LevelTxt::Rail>& rails) {
    const size_t n = rails.size();
    std::vector<uint8_t> b(0x10 + 0x20 * n + 4 * n, 0);
    b.resize(align_up(b.size(), 0x10), 0);
    const size_t pts_at = b.size();
    std::vector<uint8_t> pts;
    for (size_t k = 0; k < n; ++k) {
        const LevelTxt::Rail& r = rails[k];
        float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
        for (const auto& v : r.pts)
            for (size_t l = 0; l < 3; ++l) lo[l] = std::min(lo[l], v[l]), hi[l] = std::max(hi[l], v[l]);
        const std::array<float, 3> c{(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2};
        float rad = 0;
        for (const auto& v : r.pts) rad = std::max(rad, dist3(c, v));
        const size_t e = 0x10 + 0x20 * k;
        for (size_t l = 0; l < 3; ++l) wrf(b, e + 4 * l, c[l]);
        wrf(b, e + 0xc, rad + 4.0f);  // (the search's reach: the sphere a little larger than the rail)
        wr32(b, e + 0x10, r.loop ? 1 : 0);
        wr32(b, 0x10 + 0x20 * n + 4 * k, static_cast<int32_t>(pts.size()));
        const size_t at = pts.size();
        pts.resize(at + 0x10 + 0x10 * r.pts.size(), 0);
        wr32(pts, at, static_cast<int32_t>(r.pts.size()));
        for (size_t i = 0; i < r.pts.size(); ++i) {
            const auto& v = r.pts[i];
            const bool last = i + 1 == r.pts.size();
            const float len = last ? (r.loop ? dist3(v, r.pts.front()) : 0.0f) : dist3(v, r.pts[i + 1]);
            for (size_t l = 0; l < 3; ++l) wrf(pts, at + 0x10 + 0x10 * i + 4 * l, v[l]);
            wrf(pts, at + 0x10 + 0x10 * i + 0xc, len);
        }
    }
    wr32(b, 0, static_cast<int32_t>(n));
    wr32(b, 4, static_cast<int32_t>(pts_at));
    wr32(b, 8, static_cast<int32_t>(pts.size()));
    b.insert(b.end(), pts.begin(), pts.end());
    return b;
}

// a rail's collision strip: a ribbon kRailStrip wide just under its line (faces up)
void rail_strip(const LevelTxt::Rail& r, uint8_t type, std::vector<CTri>& out) {
    const size_t n = r.pts.size(), segs = r.loop ? n : n - 1;
    auto side = [&](size_t i) {
        const auto& a = r.pts[r.loop ? (i + n - 1) % n : (i == 0 ? 0 : i - 1)];
        const auto& b = r.pts[r.loop ? (i + 1) % n : std::min(i + 1, n - 1)];
        float tx = b[0] - a[0], ty = b[1] - a[1];
        const float l = std::sqrt(tx * tx + ty * ty);
        if (l < 1e-4f) return std::array<float, 2>{kRailStrip / 2, 0};
        tx /= l;
        ty /= l;
        return std::array<float, 2>{ty * kRailStrip / 2, -tx * kRailStrip / 2};
    };
    for (size_t i = 0; i < segs; ++i) {
        const size_t j = (i + 1) % n;
        const auto sa = side(i), sb = side(j);
        const std::array<float, 3> a0{r.pts[i][0] + sa[0], r.pts[i][1] + sa[1], r.pts[i][2] - 0.02f}, a1{r.pts[i][0] - sa[0], r.pts[i][1] - sa[1], r.pts[i][2] - 0.02f};
        const std::array<float, 3> b0{r.pts[j][0] + sb[0], r.pts[j][1] + sb[1], r.pts[j][2] - 0.02f}, b1{r.pts[j][0] - sb[0], r.pts[j][1] - sb[1], r.pts[j][2] - 0.02f};
        for (const auto& tri : {std::array<std::array<float, 3>, 3>{a0, a1, b1}, std::array<std::array<float, 3>, 3>{a0, b1, b0}}) {
            CTri c;
            c.p = tri;
            const float nz = (c.p[1][0] - c.p[0][0]) * (c.p[2][1] - c.p[0][1]) - (c.p[1][1] - c.p[0][1]) * (c.p[2][0] - c.p[0][0]);
            if (nz < 0) std::swap(c.p[1], c.p[2]);  // (facing up, as the level's floors)
            c.type = type;
            out.push_back(c);
        }
    }
}

// a rail's model: a bar 0.2 wide and 0.2 deep whose top is the rail's line (Ratchet's feet go there), as faces of an OBJ
void rail_model(const LevelTxt::Rail& r, int material, Obj& o) {
    const size_t n = r.pts.size(), segs = r.loop ? n : n - 1;
    constexpr float W = 0.1f, D = 0.2f;
    auto ring = [&](size_t i) {
        const auto& a = r.pts[r.loop ? (i + n - 1) % n : (i == 0 ? 0 : i - 1)];
        const auto& b = r.pts[r.loop ? (i + 1) % n : std::min(i + 1, n - 1)];
        float tx = b[0] - a[0], ty = b[1] - a[1];
        const float l = std::sqrt(tx * tx + ty * ty);
        const float sx = l < 1e-4f ? W : ty / l * W, sy = l < 1e-4f ? 0 : -tx / l * W;
        const auto& p = r.pts[i];
        return std::array<std::array<float, 3>, 4>{{{p[0] + sx, p[1] + sy, p[2]}, {p[0] - sx, p[1] - sy, p[2]}, {p[0] - sx, p[1] - sy, p[2] - D},
                                                    {p[0] + sx, p[1] + sy, p[2] - D}}};
    };
    auto quad = [&](const std::array<float, 3>& a, const std::array<float, 3>& b, const std::array<float, 3>& c, const std::array<float, 3>& d, float v0, float v1) {
        ObjVert va{}, vb{}, vc{}, vd{};
        for (int l = 0; l < 3; ++l) {
            va.p[l] = a[static_cast<size_t>(l)];
            vb.p[l] = b[static_cast<size_t>(l)];
            vc.p[l] = c[static_cast<size_t>(l)];
            vd.p[l] = d[static_cast<size_t>(l)];
            va.col[l] = vb.col[l] = vc.col[l] = vd.col[l] = 1.0f;
        }
        va.uv[1] = vb.uv[1] = v0;
        vc.uv[1] = vd.uv[1] = v1;
        vb.uv[0] = vc.uv[0] = 1.0f;
        ObjTri t1, t2;
        t1.v[0] = va, t1.v[1] = vb, t1.v[2] = vc;
        t2.v[0] = va, t2.v[1] = vc, t2.v[2] = vd;
        t1.material = t2.material = material;
        o.tris.push_back(t1);
        o.tris.push_back(t2);
    };
    float v = 0;
    for (size_t i = 0; i < segs; ++i) {
        const size_t j = (i + 1) % n;
        const auto A = ring(i), B = ring(j);
        const float v1 = v + dist3(r.pts[i], r.pts[j]);
        for (size_t f = 0; f < 4; ++f) quad(A[f], A[(f + 1) % 4], B[(f + 1) % 4], B[f], v, v1);
        v = v1;
    }
    if (!r.loop) {  // the ends closed
        const auto A = ring(0), B = ring(n - 1);
        quad(A[3], A[2], A[1], A[0], 0, 1);
        quad(B[0], B[1], B[2], B[3], 0, 1);
    }
}

// the rails' models (`mesh`) as more host meshes: their material the named one of the level's meshes' (else a grey metal)
bool rails_meshes(const std::vector<LevelTxt::Rail>& rails, const std::vector<ObjMaterial>& mats, mrpw::HostMeshFile& hm, Log& log, std::string& err);

// (stage 10) a world's section with rails of the mod's own: its level's collision (from the disc: data.wad's collision block, every
// face of its cells, a face seen in several cells once) with the rails' collision strips added (the grind needs ground under the
// rail), built again as the composer builds a collision; the level's hero groups (their own block, its offsets its own) kept
bool world_collision_with_rails(int level, const std::vector<LevelTxt::Rail>& rails, uint8_t type, std::vector<uint8_t>& out, Log& log,
                                std::string& err) {
    const fs::path wp = fs::path("assets/disc") / fmt("levels/level%02d", level) / "data.wad";
    std::vector<uint8_t> wad, blob;
    uint32_t off = 0, size = 0;
    bool packed = false;
    if (!read_file(wp, wad) ||
        !extract_collision_from_level_wad(std::span<const std::byte>(reinterpret_cast<const std::byte*>(wad.data()), wad.size()), blob, off, size, packed)) {
        err = wp.generic_string() + ": its collision cannot be read";
        return false;
    }
    DecodedCollision dc;
    try {
        dc = decode_level_collision(std::span<const std::byte>(reinterpret_cast<const std::byte*>(blob.data()), blob.size()));
    } catch (const std::exception& e) {
        err = fmt("level %d's collision cannot be decoded (%s)", level, e.what());
        return false;
    }
    std::vector<CTri> ct;
    std::set<std::array<int32_t, 10>> seen;
    size_t faces = 0;
    for (const DecodedCollisionOctant& c : dc.octants) {
        auto at = [&](uint8_t i) {
            const CollisionVec3& v = c.vertices[i];
            return std::array<float, 3>{c.displacement.x + v.x, c.displacement.y + v.y, c.displacement.z + v.z};
        };
        auto add = [&](uint8_t a, uint8_t b, uint8_t d, uint8_t ty) {
            if (a >= c.vertices.size() || b >= c.vertices.size() || d >= c.vertices.size()) return;
            CTri tr;
            tr.p = {at(a), at(d), at(b)};  // (as an OBJ's turn: build_collision turns them back to the game's)
            tr.type = ty;
            tr.whole = true;
            std::array<int32_t, 10> key{};  // (the face as its points at 1/64 and its type: once)
            for (int i = 0; i < 3; ++i)
                for (int l = 0; l < 3; ++l) key[static_cast<size_t>(i * 3 + l)] = static_cast<int32_t>(std::lround(tr.p[static_cast<size_t>(i)][static_cast<size_t>(l)] * 64.0f));
            key[9] = ty;
            if (seen.insert(key).second) ct.push_back(tr);
        };
        for (const DecodedCollFace& fc : c.faces) {
            ++faces;
            add(fc.v0, fc.v1, fc.v2, fc.type);
            if (fc.is_quad) add(fc.v0, fc.v2, fc.v3, fc.type);
        }
    }
    const size_t level_tris = ct.size();
    for (const LevelTxt::Rail& r : rails)
        if (r.strip) rail_strip(r, type, ct);
    if (!build_collision(ct, out, log, err)) return false;
    if (dc.hero_groups_offset > 0 && static_cast<size_t>(dc.hero_groups_offset) < blob.size()) {
        const size_t at = (out.size() + 0xf) & ~size_t{0xf};
        out.resize(at, 0);
        out.insert(out.end(), blob.begin() + dc.hero_groups_offset, blob.end());
        wr32(out, 4, static_cast<int32_t>(at));
    }
    log.say(fmt("its level %d's collision with its rails' strips: %zu of the level's faces in %zu cells (%zu triangles once), %zu triangles of the strips; "
                "%zu bytes (the level's %u)%s",
                level, faces, dc.octants.size(), level_tris, ct.size() - level_tris, out.size(), size, dc.hero_groups.empty() ? "" : ", its hero groups kept"));
    return true;
}

// (stage 7) a class's collision mesh (`class ... collision <coll.obj>`) as a moby's collision (the carrier's +0x94; the game's
// moby collision format, mobys_in_sphere's: 0x10 bytes of header (joints 0, primitives 0, the triangles' bytes, the
// vertices' bytes), the vertices (x y z as int16 at 1/1024 of a unit of the moby's scale, a pad), the triangles (three vertex
// indices, clockwise seen from outside, and a byte of flags: the bolt crate's box has 0x7f)). `k`: a model unit's int16 steps (1024 / the carrier's scale)
bool class_collision(const Obj& co, float k, std::vector<uint8_t>& out, std::string& err) {
    std::vector<std::array<int16_t, 3>> vs;
    std::vector<std::array<uint8_t, 4>> ts;
    auto vert = [&](const ObjVert& v) -> int {
        std::array<int16_t, 3> q{};
        for (int l = 0; l < 3; ++l) {
            const float f = std::round(v.p[l] * k);
            if (std::fabs(f) > 32000.0f) return -2;
            q[static_cast<size_t>(l)] = static_cast<int16_t>(f);
        }
        for (size_t i = 0; i < vs.size(); ++i)
            if (vs[i] == q) return static_cast<int>(i);
        vs.push_back(q);
        return static_cast<int>(vs.size()) - 1;
    };
    for (const ObjTri& t : co.tris) {
        int id[3];
        for (int i = 0; i < 3; ++i) {
            id[i] = vert(t.v[i]);
            if (id[i] == -2) {
                err = "a vertex too far from the model's origin for the class's size (give a larger size)";
                return false;
            }
        }
        if (vs.size() > 255) {
            err = "more than 255 vertices (the game's moby collision indexes them with a byte)";
            return false;
        }
        if (id[0] == id[1] || id[1] == id[2] || id[0] == id[2]) continue;
        // (the game's winding: clockwise seen from outside, the bolt crate's; the flag byte its 0x7f)
        ts.push_back({static_cast<uint8_t>(id[0]), static_cast<uint8_t>(id[2]), static_cast<uint8_t>(id[1]), 0x7f});
    }
    if (ts.empty()) {
        err = "no faces";
        return false;
    }
    if (vs.size() % 2 != 0) vs.push_back(vs.back());  // (the vertices read two at a time: 16 bytes)
    out.assign(0x10, 0);
    wr32(out, 8, static_cast<int32_t>(ts.size() * 4));
    wr32(out, 0xc, static_cast<int32_t>(vs.size() * 8));
    for (const auto& v : vs)
        for (int l = 0; l < 4; ++l) {
            const int16_t x = l < 3 ? v[static_cast<size_t>(l)] : 0;
            out.push_back(static_cast<uint8_t>(x));
            out.push_back(static_cast<uint8_t>(static_cast<uint16_t>(x) >> 8));
        }
    for (const auto& t : ts) out.insert(out.end(), t.begin(), t.end());
    out.resize(align_up(out.size(), 0x10), 0);
    return true;
}

// ---- the core (the new asset data) ----
struct Core {
    std::vector<uint8_t> buf;
    struct Seg {
        size_t lo, hi, at;
    };
    std::vector<Seg> segs;
    size_t put(const uint8_t* b, size_t n, size_t a = 0x100) {
        const size_t at = align_up(buf.size(), a);
        buf.resize(at, 0);
        buf.insert(buf.end(), b, b + n);
        return at;
    }
    // the old core's bytes [lo, hi), placed at the same offset modulo 0x1000
    size_t keep(const std::vector<uint8_t>& old, size_t lo, size_t hi) {
        size_t at = align_up(buf.size(), 0x1000) + (lo & 0xfff);
        if (at < buf.size()) at += 0x1000;
        buf.resize(at, 0);
        buf.insert(buf.end(), old.begin() + static_cast<std::ptrdiff_t>(lo), old.begin() + static_cast<std::ptrdiff_t>(hi));
        segs.push_back({lo, hi, at});
        return at;
    }
    bool remap(size_t off, size_t& out) const {
        for (const Seg& s : segs)
            if ((s.lo <= off && off < s.hi) || (off == s.hi && &s == &segs.back())) {
                out = s.at + off - s.lo;
                return true;
            }
        return false;
    }
};

// ---- the host meshes ----
uint32_t csm1(uint32_t i) { return (i & ~0x18u) | ((i & 0x08u) << 1) | ((i & 0x10u) >> 1); }

uint32_t pow2_at_most(int v, uint32_t lo, uint32_t hi) {
    uint32_t p = lo;
    while (p * 2 <= static_cast<uint32_t>(std::max(v, 1)) && p * 2 <= hi) p *= 2;
    return p;
}

// a material's texture: its PNG (to at most 256 x 256, powers of two, 256 colours) or its colour
bool material_texture(const ObjMaterial& m, mrp::Texture& t, Log& log, std::string& err) {
    std::vector<uint32_t> rgba;  // RGBA8 (R lowest)
    uint32_t w = 8, h = 8;
    if (!m.map.empty()) {
        const fs::path p = m.dir / m.map;
        int iw = 0, ih = 0, ch = 0;
        unsigned char* px = stbi_load(p.string().c_str(), &iw, &ih, &ch, 4);
        if (px == nullptr) {
            err = p.generic_string() + ": not a PNG (or JPEG) that can be read";
            return false;
        }
        w = pow2_at_most(iw, 8, 256);
        h = pow2_at_most(ih, 8, 256);
        rgba.resize(static_cast<size_t>(w) * h);
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x) {
                const size_t sx = static_cast<size_t>(x) * static_cast<size_t>(iw) / w, sy = static_cast<size_t>(y) * static_cast<size_t>(ih) / h;
                std::memcpy(&rgba[static_cast<size_t>(y) * w + x], px + (sy * static_cast<size_t>(iw) + sx) * 4, 4);
            }
        stbi_image_free(px);
        if (static_cast<uint32_t>(iw) != w || static_cast<uint32_t>(ih) != h)
            log.say(fmt("  %s: %dx%d scaled to %ux%u (powers of two, at most 256)", m.map.c_str(), iw, ih, w, h));
    } else {
        uint32_t c = 0xff000000u;
        for (int l = 0; l < 3; ++l) c |= static_cast<uint32_t>(std::clamp(static_cast<int>(std::lround(m.kd[l] * 255.0f)), 0, 255)) << (8 * l);
        rgba.assign(static_cast<size_t>(w) * h, c);
    }
    // the palette: the colours themselves when there are 256 at most, else a 6 x 7 x 6 cube; alpha below half: index 255,
    // clear
    std::vector<uint32_t> pal;
    std::map<uint32_t, uint8_t> exact;
    bool fits = true;
    for (uint32_t c : rgba) {
        if ((c >> 24) < 0x80) continue;
        const uint32_t k = c | 0xff000000u;
        if (exact.count(k) == 0) {
            if (exact.size() >= 255) {
                fits = false;
                break;
            }
            const uint8_t i = static_cast<uint8_t>(exact.size());
            exact[k] = i;
        }
    }
    std::vector<uint8_t> idx(rgba.size());
    if (fits) {
        pal.assign(256, 0);
        for (auto& [c, i] : exact) pal[i] = c;
        for (size_t i = 0; i < rgba.size(); ++i) idx[i] = (rgba[i] >> 24) < 0x80 ? 255 : exact[rgba[i] | 0xff000000u];
    } else {
        pal.assign(256, 0);
        for (int r = 0; r < 6; ++r)
            for (int g = 0; g < 7; ++g)
                for (int b = 0; b < 6; ++b)
                    pal[static_cast<size_t>(r * 42 + g * 6 + b)] = 0xff000000u | static_cast<uint32_t>(b * 51) << 16 | static_cast<uint32_t>(g * 255 / 6) << 8 | static_cast<uint32_t>(r * 51);
        for (size_t i = 0; i < rgba.size(); ++i) {
            const uint32_t c = rgba[i];
            if ((c >> 24) < 0x80) {
                idx[i] = 255;
                continue;
            }
            const int r = static_cast<int>(std::lround((c & 0xff) / 51.0)), g = static_cast<int>(std::lround(((c >> 8) & 0xff) * 6 / 255.0)),
                      b = static_cast<int>(std::lround(((c >> 16) & 0xff) / 51.0));
            idx[i] = static_cast<uint8_t>(r * 42 + g * 6 + b);
        }
    }
    t.width = w;
    t.height = h;
    for (uint32_t i = 0; i < 256; ++i) {
        const uint32_t c = pal[i];
        const uint32_t a = i == 255 ? 0u : 0x80u;  // (the GS's alpha: 0x80 opaque)
        t.palette[csm1(i)] = (c & 0x00ffffffu) | a << 24;
    }
    t.mips.clear();
    t.mips.push_back(idx);
    for (uint32_t mw = w / 2, mh = h / 2, s = 2; t.mips.size() < 4 && mw >= 1 && mh >= 1; mw /= 2, mh /= 2, s *= 2) {
        std::vector<uint8_t> m2(static_cast<size_t>(mw) * mh);
        for (uint32_t y = 0; y < mh; ++y)
            for (uint32_t x = 0; x < mw; ++x) m2[static_cast<size_t>(y) * mw + x] = idx[static_cast<size_t>(y * s) * w + x * s];
        t.mips.push_back(std::move(m2));
    }
    return true;
}

// (a level class's model, `as_class`: the whole OBJ one mesh round its own origin, no placement; its colours and extent out)
struct ClassModel {
    int32_t mesh = -1;
    float extent = 0;  // the model's largest extent (its bounds' largest edge)
    std::vector<std::array<uint8_t, 4>> palette;
};
bool build_host_meshes(const Obj& o, mrpw::HostMeshFile& out, Log& log, std::string& err, const char* as_class = nullptr, ClassModel* model = nullptr) {
    std::vector<int> tex_of(o.materials.size() + 1, -1);  // material (+1, 0 none) -> the file's texture
    auto texture = [&](int material) -> int {
        int& slot = tex_of[static_cast<size_t>(material + 1)];
        if (slot >= 0) return slot;
        mrp::Texture t;
        ObjMaterial none;
        none.name = "(none)";
        if (!material_texture(material >= 0 ? o.materials[static_cast<size_t>(material)] : none, t, log, err)) return -1;
        t.index = static_cast<uint32_t>(out.textures.size());
        out.textures.push_back(std::move(t));
        slot = static_cast<int>(out.textures.size()) - 1;
        return slot;
    };
    size_t total_tris = 0;
    const size_t groups = as_class != nullptr ? 1 : o.objects.size();
    for (size_t obj = 0; obj < groups; ++obj) {
        std::vector<const ObjTri*> tris;
        for (const ObjTri& t : o.tris)
            if (as_class != nullptr || t.object == static_cast<int>(obj)) tris.push_back(&t);
        const std::string mesh_name = as_class != nullptr ? std::string(as_class) : o.objects[obj];
        if (tris.empty()) {
            if (as_class != nullptr) {
                err = std::string("the class \"") + as_class + "\": its model has no faces";
                return false;
            }
            continue;
        }
        std::stable_sort(tris.begin(), tris.end(), [](const ObjTri* a, const ObjTri* b) { return a->material < b->material; });
        float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
        for (const ObjTri* t : tris)
            for (const ObjVert& v : t->v)
                for (int l = 0; l < 3; ++l) lo[l] = std::min(lo[l], v.p[l]), hi[l] = std::max(hi[l], v.p[l]);
        float centre[3], reach = 0, mid[3];
        for (int l = 0; l < 3; ++l) mid[l] = centre[l] = 0.5f * (lo[l] + hi[l]);
        if (as_class != nullptr)  // (round its own origin: the object's pivot)
            for (float& c : centre) c = 0.0f;
        for (const ObjTri* t : tris)
            for (const ObjVert& v : t->v)
                for (int l = 0; l < 3; ++l) reach = std::max(reach, std::fabs(v.p[l] - centre[l]));
        mrpw::HostMeshClass c;
        c.id = static_cast<int32_t>(out.classes.size());
        c.scale = std::max(reach * 1024.0f / 32000.0f, 1.0f / 64.0f);
        const float k = 1024.0f / c.scale;
        float r2 = 0;
        for (const ObjTri* t : tris)
            for (const ObjVert& v : t->v) {
                float d = 0;
                for (int l = 0; l < 3; ++l) d += (v.p[l] - centre[l]) * (v.p[l] - centre[l]);
                r2 = std::max(r2, d);
            }
        c.sphere[3] = std::sqrt(r2) / c.scale;
        if (as_class != nullptr) {  // (the sphere round the model's middle: its offset from the origin, in vertex units / 1024)
            r2 = 0;
            for (const ObjTri* t : tris)
                for (const ObjVert& v : t->v) {
                    float d = 0;
                    for (int l = 0; l < 3; ++l) d += (v.p[l] - mid[l]) * (v.p[l] - mid[l]);
                    r2 = std::max(r2, d);
                }
            for (int l = 0; l < 3; ++l) c.sphere[l] = mid[l] / c.scale;
            c.sphere[3] = std::sqrt(r2) / c.scale;
        }
        // the vertices' colours (the OBJ's, times the material's Kd when it has a texture: a colour material's Kd is its
        // texture) in the instance's palette: at most 64 (the tie's colour table), so made coarser until they fit
        auto vcol = [&](const ObjTri* t, int i) {
            std::array<int, 3> c{};
            const ObjMaterial* m = t->material >= 0 ? &o.materials[static_cast<size_t>(t->material)] : nullptr;
            for (int l = 0; l < 3; ++l) {
                const float k = m != nullptr && !m->map.empty() ? std::clamp(m->kd[l], 0.0f, 1.0f) : 1.0f;
                c[static_cast<size_t>(l)] = static_cast<int>(std::lround(t->v[i].col[l] * k * 128.0f));
            }
            return c;
        };
        std::map<std::array<int, 3>, uint8_t> pal;
        int step = 1;
        for (const int st : {1, 2, 4, 8, 16, 32, 43}) {
            step = st;
            pal.clear();
            for (const ObjTri* t : tris)
                for (int i = 0; i < 3; ++i) {
                    std::array<int, 3> c = vcol(t, i);
                    for (int& x : c) x = std::min(128, (x + st / 2) / st * st);
                    pal.emplace(c, 0);
                }
            if (pal.size() <= 64) break;
        }
        const bool coloured = !(pal.size() == 1 && pal.begin()->first == std::array<int, 3>{128, 128, 128});
        mrpw::HostMeshInstance in;
        if (coloured) {
            for (auto& [c, k] : pal) {
                k = static_cast<uint8_t>(in.palette.size());
                in.palette.push_back({static_cast<uint8_t>(c[0]), static_cast<uint8_t>(c[1]), static_cast<uint8_t>(c[2]), 0x80});
            }
            if (step > 1) log.say(fmt("  %s: its vertex colours made coarser (steps of %d / 128) to fit 64", mesh_name.c_str(), step));
        }
        auto vindex = [&](const ObjTri* t, int i) -> int16_t {
            if (!coloured) return 0;
            std::array<int, 3> c = vcol(t, i);
            for (int& x : c) x = std::min(128, (x + step / 2) / step * step);
            return static_cast<int16_t>(pal[c]);
        };
        std::map<int, uint32_t> local;  // material -> local texture
        mrpw::HostMeshPacket pk;
        auto flush = [&] {
            if (pk.dinky.empty()) return;
            pk.index_in_lod = static_cast<uint8_t>(c.packets.size());
            c.packets.push_back(std::move(pk));
            pk = mrpw::HostMeshPacket{};
        };
        bool clamped = false;
        for (const ObjTri* t : tris) {
            if (local.count(t->material) == 0) {
                if (local.size() >= 16) {
                    err = "the mesh \"" + mesh_name + "\" has more than 16 materials (split it into objects)";
                    return false;
                }
                const int tx = texture(t->material);
                if (tx < 0) return false;
                const uint32_t lt = static_cast<uint32_t>(local.size());
                local[t->material] = lt;
                c.textures.push_back({static_cast<uint32_t>(tx), 0, 0x44});
            }
            if (pk.dinky.size() + 3 > 240) flush();
            // the UVs moved by whole turns to start near 0 (they are s16 / 4096: within +-8)
            const float su = std::floor(std::min({t->v[0].uv[0], t->v[1].uv[0], t->v[2].uv[0]}));
            const float sv = std::floor(std::min({1.0f - t->v[0].uv[1], 1.0f - t->v[1].uv[1], 1.0f - t->v[2].uv[1]}));
            mrpw::HostMeshStrip strip;
            strip.local_texture = local[t->material];
            for (int vi = 0; vi < 3; ++vi) {
                const ObjVert& v = t->v[vi];
                std::array<int16_t, 8> d{};
                for (int l = 0; l < 3; ++l) d[static_cast<size_t>(l)] = static_cast<int16_t>(std::clamp(std::lround((v.p[l] - centre[l]) * k), -32767l, 32767l));
                d[3] = vindex(t, vi);
                const float s = (v.uv[0] - su) * 4096.0f, tt = (1.0f - v.uv[1] - sv) * 4096.0f;
                if (s > 32767.0f || tt > 32767.0f) clamped = true;
                d[4] = static_cast<int16_t>(std::clamp(std::lround(s), -32767l, 32767l));
                d[5] = static_cast<int16_t>(std::clamp(std::lround(tt), -32767l, 32767l));
                d[6] = 4096;
                strip.verts.push_back(static_cast<uint16_t>(pk.dinky.size()));
                pk.dinky.push_back(d);
            }
            pk.strips.push_back(std::move(strip));
        }
        flush();
        if (clamped) log.say("  " + mesh_name + ": UVs spanning more than 8 repeats in one triangle were clamped");
        for (int r = 0; r < 3; ++r) in.rot[r][r] = 1.0f;
        for (int l = 0; l < 3; ++l) in.pos[l] = centre[l];
        in.cls = static_cast<uint32_t>(out.classes.size());
        if (as_class == nullptr) {
            out.instances.push_back(in);
        } else if (model != nullptr) {
            model->mesh = static_cast<int32_t>(out.classes.size());
            model->palette = in.palette;
            model->extent = std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]});
        }
        log.say(fmt("  %s \"%s\": %zu triangles, %zu packets, %zu textures, %zu vertex colours, %s (%.1f %.1f %.1f), reach %.1f", as_class != nullptr ? "class model" : "mesh",
                    mesh_name.c_str(), tris.size(), c.packets.size(), c.textures.size(), in.palette.size(), as_class != nullptr ? "middle" : "centre", mid[0], mid[1], mid[2], reach));
        total_tris += tris.size();
        out.classes.push_back(std::move(c));
    }
    if (as_class == nullptr) log.say(fmt("host meshes: %zu meshes, %zu triangles, %zu textures", out.classes.size(), total_tris, out.textures.size()));
    return true;
}

// (stage 7) the rails' models (`mesh`): more host meshes, their material the named one of the level's meshes' (else grey metal)
bool rails_meshes(const std::vector<LevelTxt::Rail>& rails, const std::vector<ObjMaterial>& mats, mrpw::HostMeshFile& hm, Log& log, std::string& err) {
    Obj o;
    size_t made = 0;
    for (const LevelTxt::Rail& r : rails) {
        if (!r.mesh) continue;
        int mat = -1;
        for (size_t i = 0; i < o.materials.size() && mat < 0; ++i)
            if (o.materials[i].name == (r.material.empty() ? std::string("(rail)") : r.material)) mat = static_cast<int>(i);
        if (mat < 0) {
            ObjMaterial m;
            if (r.material.empty()) {
                m.name = "(rail)";
                m.kd[0] = 0.55f, m.kd[1] = 0.57f, m.kd[2] = 0.62f;
            } else {
                bool found = false;
                for (const ObjMaterial& x : mats)
                    if (x.name == r.material) m = x, found = true;
                if (!found) {
                    err = fmt("level.txt:%d: the rail %s's mesh material \"%s\" is not one of the meshes' (their .mtl)", r.line, r.name.c_str(), r.material.c_str());
                    return false;
                }
            }
            o.materials.push_back(m);
            mat = static_cast<int>(o.materials.size()) - 1;
        }
        rail_model(r, mat, o);
        ++made;
    }
    if (made == 0) return true;
    if (!build_host_meshes(o, hm, log, err)) return false;
    log.say(fmt("rails: %zu models made (%zu triangles)", made, o.tris.size()));
    return true;
}

// ---- the level's own sounds: one 989snd sound bank (the format of the game's own banks, third_party/989snd's loader) ----
// a sound's samples, mono, and their rate
struct Pcm {
    std::vector<int16_t> s;
    uint32_t rate = 0;
};

uint32_t le32(const std::vector<uint8_t>& b, size_t at) { return at + 4 <= b.size() ? static_cast<uint32_t>(b[at] | b[at + 1] << 8 | b[at + 2] << 16 | static_cast<uint32_t>(b[at + 3]) << 24) : 0; }
uint32_t le16(const std::vector<uint8_t>& b, size_t at) { return at + 2 <= b.size() ? static_cast<uint32_t>(b[at] | b[at + 1] << 8) : 0; }
uint32_t be32(const std::vector<uint8_t>& b, size_t at) { return at + 4 <= b.size() ? static_cast<uint32_t>(b[at] << 24 | b[at + 1] << 16 | b[at + 2] << 8 | b[at + 3]) : 0; }

// a WAV (RIFF, PCM 8 or 16 bits, mono or stereo: mixed to mono)
bool read_wav(const std::vector<uint8_t>& b, Pcm& out, std::string& why) {
    if (b.size() < 12 || std::memcmp(b.data(), "RIFF", 4) != 0 || std::memcmp(b.data() + 8, "WAVE", 4) != 0) {
        why = "not a WAV (RIFF / WAVE)";
        return false;
    }
    uint32_t fmt_tag = 0, channels = 0, bits = 0;
    size_t data = 0, data_size = 0;
    for (size_t at = 12; at + 8 <= b.size();) {
        const uint32_t n = le32(b, at + 4);
        if (std::memcmp(b.data() + at, "fmt ", 4) == 0) {
            fmt_tag = le16(b, at + 8);
            channels = le16(b, at + 10);
            out.rate = le32(b, at + 12);
            bits = le16(b, at + 22);
        } else if (std::memcmp(b.data() + at, "data", 4) == 0) {
            data = at + 8;
            data_size = std::min<size_t>(n, b.size() - data);
        }
        at += 8 + n + (n & 1);
    }
    if (fmt_tag != 1 || (bits != 8 && bits != 16) || channels < 1 || channels > 2 || data == 0) {
        why = "a WAV of another kind (PCM 8 or 16 bits, mono or stereo only)";
        return false;
    }
    const size_t frame = channels * bits / 8;
    for (size_t at = data; at + frame <= data + data_size; at += frame) {
        int32_t sum = 0;
        for (uint32_t c = 0; c < channels; ++c)
            sum += bits == 16 ? static_cast<int16_t>(le16(b, at + c * 2)) : (static_cast<int32_t>(b[at + c]) - 128) * 256;
        out.s.push_back(static_cast<int16_t>(sum / static_cast<int32_t>(channels)));
    }
    return true;
}

// samples as PS-ADPCM (the SPU2's): 16-byte blocks of 28 samples, each with the predictor and shift that fit it best;
// `loop`: the first block marks the loop's start, the last its end (and repeat), else the last ends the voice
std::vector<uint8_t> adpcm_encode(const std::vector<int16_t>& in, bool loop) {
    static const int coefs[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};
    std::vector<int32_t> pcm(in.begin(), in.end());
    if (pcm.empty()) pcm.assign(28, 0);
    pcm.resize((pcm.size() + 27) / 28 * 28, 0);
    std::vector<uint8_t> out;
    int32_t s1 = 0, s2 = 0;
    for (size_t b = 0; b < pcm.size(); b += 28) {
        int64_t best_err = -1;
        int best_f = 0, best_shift = 0;
        int32_t best_s1 = 0, best_s2 = 0;
        uint8_t best_nib[28] = {};
        for (int f = 0; f < 5; ++f) {
            const int c1 = coefs[f][0], c2 = coefs[f][1];
            int32_t p1 = s1, p2 = s2, peak = 0;
            for (size_t i = 0; i < 28; ++i) {
                const int32_t x = pcm[b + i];
                peak = std::max(peak, std::abs(x - ((p1 * c1 + p2 * c2) >> 6)));
                p2 = p1;
                p1 = x;
            }
            int shift = 12;
            while (shift > 0 && (7 << (12 - shift)) < peak) --shift;
            p1 = s1;
            p2 = s2;
            int64_t err = 0;
            uint8_t nib[28];
            for (size_t i = 0; i < 28; ++i) {
                const int32_t x = pcm[b + i];
                const int32_t pred = (p1 * c1 + p2 * c2) >> 6;
                const double qd = std::round(static_cast<double>(x - pred) / static_cast<double>(1 << (12 - shift)));
                const int32_t q = std::clamp(static_cast<int32_t>(qd), -8, 7);
                const int32_t y = std::clamp(((q << 12) >> shift) + pred, -32768, 32767);
                err += static_cast<int64_t>(x - y) * (x - y);
                nib[i] = static_cast<uint8_t>(q & 15);
                p2 = p1;
                p1 = y;
            }
            if (best_err < 0 || err < best_err) {
                best_err = err;
                best_f = f;
                best_shift = shift;
                best_s1 = p1;
                best_s2 = p2;
                std::memcpy(best_nib, nib, sizeof nib);
            }
        }
        s1 = best_s1;
        s2 = best_s2;
        out.push_back(static_cast<uint8_t>(best_f << 4 | best_shift));
        out.push_back(0);
        for (size_t i = 0; i < 28; i += 2) out.push_back(static_cast<uint8_t>(best_nib[i] | best_nib[i + 1] << 4));
    }
    if (loop) {
        out[1] |= 4;
        out[out.size() - 15] |= 3;
    } else {
        out[out.size() - 15] |= 1;
    }
    return out;
}

// a VAG's blocks (the 0x30-byte "VAGp" header: the data's size +0xc, the rate +0x10, big endian), their flags made the
// sound's (looping or not; the trailing silent blocks of a one-shot dropped)
bool read_vag(const std::vector<uint8_t>& v, bool loop, std::vector<uint8_t>& out, uint32_t& rate, std::string& why) {
    if (v.size() < 0x40 || std::memcmp(v.data(), "VAGp", 4) != 0) {
        why = "not a VAG (no \"VAGp\" header)";
        return false;
    }
    const uint32_t size = be32(v, 12);
    rate = be32(v, 16);
    if (size < 16 || size + 0x30 > v.size() || rate < 8000 || rate > 48000) {
        why = fmt("its header's size %u or rate %u do not fit the file (%zu bytes)", size, rate, v.size());
        return false;
    }
    out.assign(v.begin() + 0x30, v.begin() + 0x30 + size / 16 * 16);
    if (out.size() >= 16 && std::all_of(out.begin(), out.begin() + 16, [](uint8_t c) { return c == 0; }) && out.size() > 16)
        out.erase(out.begin(), out.begin() + 16);  // (the leading silent block most VAGs have)
    while (out.size() > 16 && (out[out.size() - 15] & 1) != 0 && std::all_of(out.end() - 14, out.end(), [](uint8_t c) { return c == 0 || c == 0x77; }))
        out.resize(out.size() - 16);  // (the end blocks: ours below)
    for (size_t i = 0; i < out.size(); i += 16) out[i + 1] = 0;
    if (out.empty()) {
        why = "no samples";
        return false;
    }
    if (loop) {
        out[1] |= 4;
        out[out.size() - 15] |= 3;
    } else {
        out[out.size() - 15] |= 1;
    }
    return true;
}

// the bank: the file's chunk table (type 1, two chunks: the SBlk block, the samples), the block (version 1, as the
// game's: a header of 0x3c bytes, a 12-byte entry per sound, a 0x28-byte tone grain per sound), the samples (each sound's
// ADPCM, 16-byte aligned). A tone at the handler's note 60 plays at its sample's rate: its centre note and fine set so
// (60 - centre) + fine / 128 semitones is 12 log2(rate / 48000) (plus the line's pitch).
struct BankSound {
    std::vector<uint8_t> adpcm;
    uint32_t rate = 0;
    float volume = 1, pitch = 0;
};
std::vector<uint8_t> sound_bank(const std::vector<BankSound>& sounds) {
    const uint32_t n = static_cast<uint32_t>(sounds.size());
    const uint32_t first_sound = 0x40, first_grain = static_cast<uint32_t>(align_up(first_sound + 12 * n, 16));
    std::vector<uint8_t> blk(align_up(first_grain + 0x28 * n, 16), 0);
    std::vector<uint8_t> smp;
    auto w8 = [&](size_t at, int v) { blk[at] = static_cast<uint8_t>(v); };
    auto w16 = [&](size_t at, uint32_t v) {
        blk[at] = static_cast<uint8_t>(v);
        blk[at + 1] = static_cast<uint8_t>(v >> 8);
    };
    auto w32 = [&](size_t at, uint32_t v) { wr32(blk, at, static_cast<int32_t>(v)); };
    std::memcpy(blk.data(), "SBlk", 4);
    w32(4, 1);           // the version
    w32(8, 0);           // flags: no names, no user data
    w32(0xc, 0x4d4f4453);  // the bank's id ("SDOM")
    w16(0x16, n);        // sounds
    w16(0x18, n);        // grains
    w16(0x1a, n);        // VAGs
    w32(0x1c, first_sound);
    w32(0x20, first_grain);
    for (uint32_t i = 0; i < n; ++i) {
        const BankSound& b = sounds[i];
        const uint32_t sample_at = static_cast<uint32_t>(smp.size());
        smp.insert(smp.end(), b.adpcm.begin(), b.adpcm.end());
        smp.resize(align_up(smp.size(), 16), 0);
        const size_t e = first_sound + 12 * i;
        w8(e + 0, 127);      // its volume
        w8(e + 1, 0);        // its volume group (the world's sounds)
        w16(e + 2, 0);       // its pan
        w8(e + 4, 1);        // one grain
        w8(e + 5, 0);        // no instance limit
        w16(e + 6, 0);
        w32(e + 8, 0x28 * i);  // its grain (from the grains' start)
        const size_t g = first_grain + 0x28 * i;
        w32(g + 0, 1);       // a tone
        w32(g + 4, 0);       // no delay
        const double semis = 12.0 * std::log2(static_cast<double>(b.rate) / 48000.0) + b.pitch;
        const double fl = std::floor(semis);
        int centre = 60 - static_cast<int>(fl), fine = static_cast<int>(std::lround((semis - fl) * 128.0));
        if (fine >= 128) {
            fine -= 128;
            --centre;
        }
        w8(g + 8, 0x40);                                                   // priority
        w8(g + 9, static_cast<int>(std::lround(std::clamp(b.volume, 0.0f, 1.0f) * 127.0f)));  // volume
        w8(g + 10, -centre);                                               // (negative: no 44.1 kHz scaling)
        w8(g + 11, fine);
        w16(g + 12, 0);      // pan
        w8(g + 14, 0);       // the notes it maps
        w8(g + 15, 127);
        w8(g + 16, 2);       // pitch bend range
        w8(g + 17, 2);
        w16(g + 18, 0x00ff); // ADSR: the fastest attack, full sustain
        w16(g + 20, 0x1fc8); //       held, a short release
        w16(g + 22, 1);      // reverb (as the game's world sounds)
        w32(g + 24, sample_at);
    }
    w32(0x24, n);        // VAGs in sound RAM
    w32(0x28, static_cast<uint32_t>(smp.size()));
    w32(0x2c, static_cast<uint32_t>(smp.size()));
    std::vector<uint8_t> f(0x20, 0);
    wr32(f, 0, 1);       // the file's type (a sound block)
    wr32(f, 4, 2);       // two chunks
    wr32(f, 8, 0x20);
    wr32(f, 12, static_cast<int32_t>(blk.size()));
    wr32(f, 16, static_cast<int32_t>(0x20 + blk.size()));
    wr32(f, 20, static_cast<int32_t>(smp.size()));
    f.insert(f.end(), blk.begin(), blk.end());
    f.insert(f.end(), smp.begin(), smp.end());
    return f;
}

// ---- the gameplay file ----
// the ship's take-off path (arrival.cpp's step 3 follows it; its last six points fade the screen out): up and ahead along
// its heading. The paths block: count, the data's offset and size, then the paths' offsets in the data; a path: its point
// count, 12 bytes, then 16-byte points (x y z, the turn the game computes)
std::vector<uint8_t> flight_path(const float ship[4]) {
    const int n = 12;
    const float dx = std::cos(ship[3]), dy = std::sin(ship[3]);
    std::vector<uint8_t> path(16, 0);
    wr32(path, 0, n);
    for (int k = 0; k < n; ++k) {
        const float ahead = 6.0f * static_cast<float>(k * k) / 4.0f + 4.0f * static_cast<float>(k);
        const size_t at = path.size();
        path.resize(at + 16, 0);
        wrf(path, at, ship[0] + dx * ahead);
        wrf(path, at + 4, ship[1] + dy * ahead);
        wrf(path, at + 8, ship[2] + 6.0f + 9.0f * static_cast<float>(k));
    }
    std::vector<uint8_t> b(0x20, 0);
    wr32(b, 0, 1);
    wr32(b, 4, 0x20);
    wr32(b, 8, static_cast<int32_t>(path.size()));
    b.insert(b.end(), path.begin(), path.end());
    return b;
}

// the base level's moby records (0x78 bytes and more each) by class: the first of each
std::map<int, std::vector<uint8_t>> moby_templates(const std::vector<uint8_t>& g, uint32_t mo, int n1) {
    std::map<int, std::vector<uint8_t>> t;
    size_t p = mo + 0x10;
    for (int i = 0; i < n1 && p + 0x78 <= g.size(); ++i) {
        const int sz = rd32(g, p);
        if (sz < 0x78 || p + static_cast<size_t>(sz) > g.size()) break;
        const int cls = rd32(g, p + 0x18);
        if (t.count(cls) == 0) t[cls].assign(g.begin() + static_cast<std::ptrdiff_t>(p), g.begin() + static_cast<std::ptrdiff_t>(p + static_cast<size_t>(sz)));
        p += static_cast<size_t>(sz);
    }
    return t;
}

// a fixup list (the pvar moby links +0x50, the pvar pointers +0x5c: 8-byte rows (pvar index, offset) up to a negative
// index): its rows, and the end row as it was
void fixup_rows(const std::vector<uint8_t>& b, std::vector<std::pair<int32_t, int32_t>>& rows, std::vector<uint8_t>& end) {
    rows.clear();
    end.assign(8, 0);
    wr32(end, 0, -1);
    for (size_t r = 0; r + 8 <= b.size(); r += 8) {
        if (rd32(b, r) < 0) {
            end.assign(b.begin() + static_cast<std::ptrdiff_t>(r), b.begin() + static_cast<std::ptrdiff_t>(r + 8));
            return;
        }
        rows.emplace_back(rd32(b, r), rd32(b, r + 4));
    }
}

std::vector<uint8_t> fixup_block(const std::vector<std::pair<int32_t, int32_t>>& rows, const std::vector<uint8_t>& end) {
    std::vector<uint8_t> b;
    for (const auto& [i, o] : rows) {
        wr32(b, b.size(), i);
        wr32(b, b.size(), o);
    }
    b.insert(b.end(), end.begin(), end.end());
    b.resize(align_up(b.size(), 0x10), 0);
    return b;
}

bool place_mobys(const std::vector<uint8_t>& g, const uint32_t G[36], uint32_t mo, int n1, int32_t hero_uid, LevelTxt& lt, std::vector<uint8_t>& mobys,
                 std::map<uint32_t, std::vector<uint8_t>>& blocks, Log& log, std::string& err) {
    if (lt.mobys.empty()) return true;
    std::vector<uint32_t> offs;
    for (int i = 0; i < 36; ++i)
        if (G[i] != 0) offs.push_back(G[i]);
    offs.push_back(static_cast<uint32_t>(g.size()));
    std::sort(offs.begin(), offs.end());
    auto block = [&](uint32_t field) -> std::vector<uint8_t> {
        const uint32_t o = G[field / 4];
        if (o == 0) return {};
        const uint32_t e = *std::upper_bound(offs.begin(), offs.end(), o);
        return std::vector<uint8_t>(g.begin() + o, g.begin() + e);
    };
    const std::map<int, std::vector<uint8_t>> tmpl = moby_templates(g, mo, n1);
    std::vector<uint8_t> table = block(0x54), data = block(0x58), shared = block(0x4c);
    std::vector<std::pair<int32_t, int32_t>> links, ptrs;
    std::vector<uint8_t> links_end, ptrs_end;
    fixup_rows(block(0x50), links, links_end);
    fixup_rows(block(0x5c), ptrs, ptrs_end);
    // the shared pvar pointers: (data size, count, -, -), the data, then count rows (pvar index | offset << 16, the data's offset)
    std::vector<std::pair<uint32_t, uint32_t>> shared_rows;
    std::vector<uint8_t> shared_data;
    if (shared.size() >= 0x10) {
        const int32_t size = rd32(shared, 0), n = rd32(shared, 4);
        if (size >= 0 && n >= 0 && 0x10 + static_cast<size_t>(size) + 8 * static_cast<size_t>(n) <= shared.size()) {
            shared_data.assign(shared.begin() + 0x10, shared.begin() + 0x10 + size);
            for (int32_t k = 0; k < n; ++k)
                shared_rows.emplace_back(static_cast<uint32_t>(rd32(shared, 0x10 + static_cast<size_t>(size) + 8 * static_cast<size_t>(k))),
                                         static_cast<uint32_t>(rd32(shared, 0x10 + static_cast<size_t>(size) + 8 * static_cast<size_t>(k) + 4)));
        }
    }
    table.resize(align_up(table.size(), 8), 0);
    int32_t uid = lt.uid_first;
    std::map<int, int> per_class;
    size_t pvar_bytes = 0;
    for (LevelTxt::Moby& pm : lt.mobys) {
        const auto t = tmpl.find(pm.cls);
        if (t == tmpl.end()) {
            std::string have;
            for (const auto& [c, r] : tmpl)
                if (c != 0) have += (have.empty() ? "" : " ") + std::to_string(c);
            err = fmt("level.txt:%d: the base level has no object of class %d to copy (its classes placed: %s)", pm.line, pm.cls, have.c_str());
            return false;
        }
        std::vector<uint8_t> r = t->second;
        if (++uid == hero_uid) ++uid;
        wr32(r, 0xc, uid);
        for (int i = 0; i < 3; ++i) wrf(r, static_cast<size_t>(0x30 + 4 * i), pm.at[i]);
        wrf(r, 0x3c, 0.0f);
        wrf(r, 0x40, 0.0f);
        wrf(r, 0x44, pm.at[3]);
        wr32(r, 0x48, -1);  // (its moby group, the base level's (the composed level has none): none)
        // its spawn rules (level_load.cpp): not the base level's missions' (flags 1, 2: spawned by a mission's state, 0x10);
        // 4 (gone for good once destroyed) kept; one spawned by a mission gets 8 instead (gone for this visit once
        // destroyed): a moby with no rule at all drops no bolts when killed (moby_drop_bolts: +0xb1 0xfe). A pool's: none
        // (base_spawn gives it its bolts)
        {
            const int32_t f = rd32(r, 0x8);
            wr32(r, 0x8, pm.pool ? 0 : (f & 4) != 0 ? 4 : (f & 0x1b) != 0 ? 8 : 0);
        }
        pm.uid = uid;
        const int32_t pi = rd32(r, 0x58);
        if (pi >= 0) {
            if (static_cast<size_t>(pi) * 8 + 8 > table.size()) {
                err = fmt("level.txt:%d: class %d's pvars are not in the base level's table", pm.line, pm.cls);
                return false;
            }
            const int32_t off = rd32(table, static_cast<size_t>(pi) * 8), size = rd32(table, static_cast<size_t>(pi) * 8 + 4);
            if (off < 0 || size < 0 || static_cast<size_t>(off) + static_cast<size_t>(size) > data.size()) {
                err = fmt("level.txt:%d: class %d's pvars are past the base level's pvar data", pm.line, pm.cls);
                return false;
            }
            std::vector<uint8_t> copy(data.begin() + off, data.begin() + off + size);
            for (const auto& [i, o] : links)  // (another moby named: none here)
                if (i == pi && o >= 0 && static_cast<size_t>(o) + 4 <= copy.size()) wr32(copy, static_cast<size_t>(o), -1);
            const int32_t ni = static_cast<int32_t>(table.size() / 8);
            data.resize(align_up(data.size(), 0x10), 0);
            const size_t at = data.size();
            data.insert(data.end(), copy.begin(), copy.end());
            wr32(table, table.size(), static_cast<int32_t>(at));
            wr32(table, table.size(), size);
            for (const auto& [i, o] : std::vector<std::pair<int32_t, int32_t>>(ptrs))
                if (i == pi) ptrs.emplace_back(ni, o);
            for (const auto& [w, o] : std::vector<std::pair<uint32_t, uint32_t>>(shared_rows))
                if (static_cast<int32_t>(w & 0xffff) == pi) shared_rows.emplace_back((w & 0xffff0000u) | static_cast<uint32_t>(ni), o);
            wr32(r, 0x58, ni);
            pvar_bytes += static_cast<size_t>(size);
            pm.pvars = size;
        }
        mobys.insert(mobys.end(), r.begin(), r.end());
        ++per_class[pm.cls];
    }
    blocks[0x54] = table;
    data.resize(align_up(data.size(), 0x10), 0);
    blocks[0x58] = data;
    blocks[0x50] = fixup_block(links, links_end);
    blocks[0x5c] = fixup_block(ptrs, ptrs_end);
    if (!shared.empty()) {
        std::vector<uint8_t> sb(0x10, 0);
        wr32(sb, 0, static_cast<int32_t>(shared_data.size()));
        wr32(sb, 4, static_cast<int32_t>(shared_rows.size()));
        sb.insert(sb.end(), shared_data.begin(), shared_data.end());
        for (const auto& [w, o] : shared_rows) {
            wr32(sb, sb.size(), static_cast<int32_t>(w));
            wr32(sb, sb.size(), static_cast<int32_t>(o));
        }
        sb.resize(align_up(sb.size(), 0x10), 0);
        blocks[0x4c] = sb;
    }
    std::string what;
    for (const auto& [c, k] : per_class) what += fmt("%s%d x%d", what.empty() ? "" : ", ", c, k);
    log.say(fmt("objects: %zu placed (class x count: %s), uids %d..%d, %zu pvar bytes copied", lt.mobys.size(), what.c_str(), lt.uid_first + 1, uid, pvar_bytes));
    return true;
}

bool build_gameplay(const std::vector<uint8_t>& g, LevelTxt& lt, std::vector<uint8_t>& out, Log& log, std::string& err) {
    if (g.size() < 0x90) {
        err = "the base level's gameplay file is too small";
        return false;
    }
    uint32_t G[36];
    std::memcpy(G, g.data(), sizeof G);
    std::vector<uint32_t> offs;
    for (uint32_t x : G)
        if (x != 0) offs.push_back(x);
    offs.push_back(static_cast<uint32_t>(g.size()));
    std::sort(offs.begin(), offs.end());
    auto block = [&](uint32_t field) -> std::vector<uint8_t> {
        const uint32_t o = G[field / 4];
        if (o == 0) return {};
        const uint32_t e = *std::upper_bound(offs.begin(), offs.end(), o);
        return std::vector<uint8_t>(g.begin() + o, g.begin() + e);
    };
    std::map<uint32_t, std::vector<uint8_t>> blocks;
    std::map<uint32_t, bool> none;  // (written as 0: absent)
    // the settings: background +0 (three words), fog colour +0xc (three bytes in words), death height +0x28, the ship +0x2c
    // (x y z yaw; x 0: none), its take-off path +0x3c and the two level cameras its view blends between +0x40 / +0x44
    std::vector<uint8_t> st = block(0x00);
    if (st.size() < 0x48) {
        err = "the base level's settings block is too small";
        return false;
    }
    if (lt.has_background)
        for (int i = 0; i < 3; ++i) wr32(st, static_cast<size_t>(4 * i), lt.background[i]);
    if (lt.has_fog)
        for (int i = 0; i < 3; ++i) wr32(st, static_cast<size_t>(0xc + 4 * i), lt.fog[i]);
    float base_death = 0;
    std::memcpy(&base_death, st.data() + 0x28, 4);
    if (lt.has_death) {
        wrf(st, 0x28, lt.death);
    } else if (lt.has_floor && base_death > lt.floor - 4.0f) {
        // (the base level's kill height is its own world's: above the level's lowest floor it would kill Ratchet standing there)
        lt.death = std::floor(lt.floor) - 8.0f;
        lt.has_death = true;
        wrf(st, 0x28, lt.death);
        log.say(fmt("death height: the base level's %.1f is above the collision's lowest point (%.1f): %.1f instead (give death_height to choose)", base_death,
                    lt.floor, lt.death));
    }
    for (int i = 0; i < 4; ++i) wrf(st, static_cast<size_t>(0x2c + 4 * i), lt.ship ? lt.ship_at[i] : 0.0f);
    wr32(st, 0x3c, 0);
    wr32(st, 0x40, -1);
    wr32(st, 0x44, -1);
    blocks[0x00] = st;
    const std::vector<uint8_t> empty(0x10, 0);
    for (uint32_t f : {0x0cu, 0x30u, 0x34u, 0x38u, 0x3cu, 0x60u, 0x64u, 0x68u, 0x6cu, 0x80u, 0x88u}) blocks[f] = empty;
    // the cameras: the base level's generic ones (the follow camera, type 0, and types 4..7 with no place), not its scripted
    // ones (their boxes are the base level's cuboids, dropped)
    {
        const std::vector<uint8_t> cb = block(0x08);
        std::vector<uint8_t> cams(0x10, 0);
        int n = 0;
        const int total = cb.size() >= 4 ? rd32(cb, 0) : 0;
        for (int k = 0; k < total && 0x30 + 0x20 * static_cast<size_t>(k) <= cb.size(); ++k) {
            const size_t c = 0x10 + 0x20 * static_cast<size_t>(k);
            const int type = rd32(cb, c);
            bool unplaced = true;
            for (size_t i = 4; i < 16; ++i) unplaced = unplaced && cb[c + i] == 0;
            if ((type == 0 || (type >= 4 && type <= 7)) && unplaced) {
                cams.insert(cams.end(), cb.begin() + static_cast<std::ptrdiff_t>(c), cb.begin() + static_cast<std::ptrdiff_t>(c + 0x20));
                ++n;
            }
        }
        wr32(cams, 0, n);
        blocks[0x08] = cams;
    }
    blocks[0x70] = lt.ship ? flight_path(lt.ship_at) : std::vector<uint8_t>(0x10, 0);  // the paths
    if (lt.rails.empty()) {
        std::vector<uint8_t> grind(0x10, 0);  // the grind lines: none
        wr32(grind, 4, 0x10);
        blocks[0x74] = grind;
    } else {
        blocks[0x74] = grind_block(lt.rails);  // (stage 7) level.txt's rails
    }
    // the mobys: the hero only (the base level's class 0 record), at the start
    {
        const uint32_t mo = G[0x44 / 4];
        if (mo == 0 || mo + 0x10 > g.size()) {
            err = "the base level has no mobys block";
            return false;
        }
        const int n1 = rd32(g, mo), n2 = rd32(g, mo + 4);
        size_t p = mo + 0x10;
        std::vector<uint8_t> hero;
        for (int i = 0; i < n1 && p + 0x20 <= g.size(); ++i) {
            const int sz = rd32(g, p);
            if (sz <= 0) break;
            if (rd32(g, p + 0x18) == 0) {
                hero.assign(g.begin() + static_cast<std::ptrdiff_t>(p), g.begin() + static_cast<std::ptrdiff_t>(p + static_cast<size_t>(sz)));
                break;
            }
            p += static_cast<size_t>(sz);
        }
        if (hero.size() < 0x48) {
            err = "the base level has no hero record (class 0)";
            return false;
        }
        for (int i = 0; i < 3; ++i) wrf(hero, static_cast<size_t>(0x30 + 4 * i), lt.hero[i]);
        wrf(hero, 0x3c, 0.0f);
        wrf(hero, 0x40, 0.0f);
        wrf(hero, 0x44, lt.hero[3]);
        std::vector<uint8_t> b(0x10, 0);
        // (format 3, a session: the room for spawned objects as large as the base level's placed ones leave free, the moby
        // array no larger than the base level's own, at most 2047 mobys (the game's per-moby tables: 0x7ff entries); their
        // 0x80-byte pvars in the room the base level's placed pvars took)
        int32_t spawn_room = n2;
        if (lt.format >= 3) {
            const uint32_t pd = G[0x58 / 4], pt = G[0x54 / 4];
            int64_t base_pvars = 0;
            if (pt != 0 && pd != 0)
                for (size_t q = pt; q + 8 <= pd && q + 8 <= g.size(); q += 8) base_pvars += std::max(0, rd32(g, q + 4));
            const int64_t free_mobys = static_cast<int64_t>(n1) - 1 - static_cast<int64_t>(lt.mobys.size());
            const int64_t by_pvars = base_pvars / 0x80;
            const int64_t more = std::max<int64_t>(0, std::min({free_mobys, by_pvars, static_cast<int64_t>(0x7ff) - n1 - n2}));
            spawn_room = n2 + static_cast<int32_t>(more);
            log.say(fmt("session: room for %d spawned objects (the base level's %d, and %lld its placed ones leave)", spawn_room, n2, static_cast<long long>(more)));
        }
        wr32(b, 4, spawn_room);
        b.insert(b.end(), hero.begin(), hero.end());
        // the placed objects (level.txt's moby lines): each a copy of its class's first placement in the base level (its
        // settings, its pvars copied into entries of their own), at the line's place; uids of our own, 1, 2, ... in the
        // lines' order (the hero's skipped); their pvar words that name another moby (the moby links) none (-1), no group
        if (!place_mobys(g, G, mo, n1, rd32(hero, 0xc), lt, b, blocks, log, err)) return false;
        wr32(b, 0, 1 + static_cast<int32_t>(lt.mobys.size()));
        blocks[0x44] = b;
    }
    blocks[0x48] = empty;  // (no moby groups: the base level's name its own records)
    none[0x8c] = true;     // (the occlusion's: none)
    for (uint32_t f = 0; f < 0x90; f += 4)
        if (blocks.count(f) == 0 && none.count(f) == 0) {
            std::vector<uint8_t> b = block(f);
            if (!b.empty()) blocks[f] = b;
        }
    // the blocks in the base file's order (the ones it lacks after them)
    std::vector<uint32_t> order;
    for (uint32_t f = 0; f < 0x90; f += 4) order.push_back(f);
    std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
        const uint32_t oa = G[a / 4] == 0 ? 0xffffffffu : G[a / 4], ob = G[b / 4] == 0 ? 0xffffffffu : G[b / 4];
        return oa < ob;
    });
    std::vector<uint8_t> ng(0xa0, 0);
    for (uint32_t f : order) {
        const auto it = blocks.find(f);
        if (it == blocks.end()) continue;
        const size_t at = align_up(ng.size(), 0x10);
        ng.resize(at, 0);
        ng.insert(ng.end(), it->second.begin(), it->second.end());
        wr32(ng, f, static_cast<int32_t>(at));
    }
    // the budget: the base level's own gameplay data fits its memory; ours is its blocks with fewer objects (a few more
    // bytes per placed object's pvars at most)
    if (lt.mobys.size() + 1 > static_cast<size_t>(rd32(g, G[0x44 / 4]))) {
        err = fmt("too many objects: %zu, the base level places %d", lt.mobys.size(), rd32(g, G[0x44 / 4]) - 1);
        return false;
    }
    if (ng.size() > g.size() + 0x10000) {
        err = fmt("the gameplay data is too large (%zu bytes, the base level's %zu)", ng.size(), g.size());
        return false;
    }
    out = wad_encode(ng);
    std::vector<uint8_t> back;
    std::string why;
    if (!wad_decode_strict(out, back, why) || back != ng) {
        err = "the gameplay file does not decode back through the game's decoder: " + why;
        return false;
    }
    out.resize(align_up(out.size(), 2048), 0);
    log.say(fmt("gameplay: %zu bytes, packed %zu; hero at (%.1f %.1f %.1f), ship %s, death height %.1f", ng.size(), out.size(), lt.hero[0], lt.hero[1], lt.hero[2],
                lt.ship ? fmt("at (%.1f %.1f %.1f)", lt.ship_at[0], lt.ship_at[1], lt.ship_at[2]).c_str() : "none", lt.has_death ? lt.death : base_death));
    return true;
}

// ---- the data file ----
// a level's data file's asset data (decompressed) and its index's first words
bool data_core(const std::vector<uint8_t>& wad, std::vector<uint8_t>& core, int32_t H[48], std::string& err) {
    FileSpan parts[11];
    if (wad.size() < sizeof parts) {
        err = "a data file too small";
        return false;
    }
    std::memcpy(parts, wad.data(), sizeof parts);
    for (const FileSpan& sp : parts)
        if (sp.offset < 0 || sp.size < 0 || static_cast<size_t>(sp.offset) + static_cast<size_t>(sp.size) > wad.size()) {
            err = "a data file damaged";
            return false;
        }
    if (static_cast<size_t>(parts[2].size) < 48 * 4) {
        err = "a data file's asset index too small";
        return false;
    }
    std::memcpy(H, wad.data() + parts[2].offset, 48 * 4);
    if (!decompress_wad(std::span<const std::byte>(reinterpret_cast<const std::byte*>(wad.data() + parts[10].offset), static_cast<size_t>(parts[10].size)), core)) {
        err = "a data file's asset data cannot be decompressed";
        return false;
    }
    return true;
}

bool build_data(const std::vector<uint8_t>& wad, const std::vector<uint8_t>& coll, const std::vector<uint8_t>* sky, std::vector<uint8_t>& out, Log& log,
                std::string& err) {
    if (wad.size() < sizeof(LevelFileHeader)) {
        err = "the base level's data file is too small";
        return false;
    }
    FileSpan parts[11];
    std::memcpy(parts, wad.data(), sizeof parts);
    for (const FileSpan& s : parts)
        if (s.offset < 0 || s.size < 0 || static_cast<size_t>(s.offset) + static_cast<size_t>(s.size) > wad.size()) {
            err = "the base level's data file is damaged";
            return false;
        }
    auto part = [&](int i) { return std::vector<uint8_t>(wad.begin() + parts[i].offset, wad.begin() + parts[i].offset + parts[i].size); };
    std::vector<uint8_t> idx = part(2);
    std::vector<uint8_t> old;
    if (!decompress_wad(std::span<const std::byte>(reinterpret_cast<const std::byte*>(wad.data() + parts[10].offset), static_cast<size_t>(parts[10].size)), old)) {
        err = "the base level's asset data cannot be decompressed";
        return false;
    }
    int32_t H[48];
    if (idx.size() < sizeof H) {
        err = "the base level's asset index is too small";
        return false;
    }
    std::memcpy(H, idx.data(), sizeof H);
    auto in_core = [&](int32_t v) { return v > 0 && static_cast<size_t>(v) <= old.size(); };
    if (!in_core(H[4]) || !in_core(H[5]) || H[5] <= H[4] || !in_core(H[24]) || !in_core(H[25]) || !in_core(H[31])) {
        err = "the base level's asset index is not as expected (sky, collision, texels, effects)";
        return false;
    }
    Core core;
    // the tfrags: none ({the entries' offset 0x40, count 0, 12.0, 0})
    std::vector<uint8_t> tf(0x40, 0);
    wr32(tf, 0, 0x40);
    wrf(tf, 8, 12.0f);
    const size_t o_tfrag = core.put(tf.data(), tf.size());
    // the sky: the base level's (up to its collision), or another level's (level.txt's sky line)
    const size_t o_sky = sky != nullptr ? core.put(sky->data(), sky->size()) : core.put(old.data() + H[4], static_cast<size_t>(H[5] - H[4]));
    // (testing: RAC1_COMPOSE_BASE_COLLISION=1 keeps the base level's collision, to tell a collision fault from another)
    static const bool base_coll = std::getenv("RAC1_COMPOSE_BASE_COLLISION") != nullptr;
    const size_t o_coll = base_coll ? core.put(old.data() + H[5], static_cast<size_t>(H[24] - H[5])) : core.put(coll.data(), coll.size());
    // the moby textures' texels, in a texel block of their own
    const int32_t nmt = H[14];
    size_t m_lo = SIZE_MAX, m_hi = 0;
    for (int32_t k = 0; k < nmt; ++k) {
        TextureRecord r;
        std::memcpy(&r, idx.data() + H[15] + 16 * k, 16);
        const size_t n = static_cast<size_t>(r.width) * static_cast<size_t>(r.height) + (r.mip_count >= 2 ? static_cast<size_t>(r.width / 2) * static_cast<size_t>(r.height / 2) : 0);
        m_lo = std::min(m_lo, static_cast<size_t>(r.texels));
        m_hi = std::max(m_hi, static_cast<size_t>(r.texels) + n);
    }
    if (nmt <= 0 || static_cast<size_t>(H[24]) + m_hi > old.size()) {
        err = "the base level's moby textures are not as expected";
        return false;
    }
    const size_t o_texel = core.put(old.data() + H[24] + m_lo, m_hi - m_lo);
    {
        const std::vector<uint8_t> pad(0x400, 0);  // (the exporter reads two mip levels whatever the count)
        core.put(pad.data(), pad.size());
    }
    // the base level's parts kept, each block the index names by its own extent (up to the next offset the index names):
    // its particle and effect banks, the +0xa0 / +0xa4 / +0xa8 blocks, the moby classes, the hero's sequences (from +0x7c)
    // and the gadgets. Dropped: the tfrags, occlusion, sky and collision (replaced), the texel block (its moby textures'
    // texels kept above), the tie and shrub classes. The levels order these differently (Veldin's tie classes after its
    // moby classes, others' the same; +0x74 after the effects on Veldin, inside the texels elsewhere), so the order is
    // not assumed.
    {
        struct Named {
            size_t at;
            bool keep;
        };
        std::vector<Named> named;
        auto name = [&](int32_t v, bool keep) {
            if (v > 0 && static_cast<size_t>(v) < old.size()) named.push_back({static_cast<size_t>(v), keep});
        };
        for (int f : {2, 3, 4, 5, 24}) name(H[f], false);
        for (int f : {25, 26, 31, 40, 41, 42}) name(H[f], true);
        for (int32_t k = 0; k < H[6]; ++k) name(rd32(idx, static_cast<size_t>(H[7] + 32 * k)), true);
        for (int32_t k = 0; k < H[8]; ++k) name(rd32(idx, static_cast<size_t>(H[9] + 32 * k)), false);
        for (int32_t k = 0; k < H[10]; ++k) name(rd32(idx, static_cast<size_t>(H[11] + 0x30 * k)), false);
        for (int k = 0; k < 256; ++k) name(rd32(idx, static_cast<size_t>(H[30] + 4 * k)), true);
        for (int32_t k = 0; k < H[32]; ++k) name(rd32(idx, static_cast<size_t>(H[33] + 16 * k)), true);
        // (+0x74: a block of its own after the effects (Veldin), else a word inside the texels the game never reads
        // (level_setup.cpp reads no +0x74 of the index): not a block start then, written 0 below)
        const size_t tex_end = static_cast<size_t>(H[24]) + m_hi;
        if (H[29] > 0 && !(static_cast<size_t>(H[29]) >= static_cast<size_t>(H[24]) && static_cast<size_t>(H[29]) < tex_end)) name(H[29], true);
        named.push_back({old.size(), false});
        std::sort(named.begin(), named.end(), [](const Named& a, const Named& b) { return a.at != b.at ? a.at < b.at : a.keep > b.keep; });
        // each kept block up to the next named offset, runs of kept blocks one part (their layout as it was)
        size_t lo = 0;
        bool open = false;
        for (size_t i = 0; i + 1 < named.size(); ++i) {
            if (named[i].at == named[i + 1].at) {
                named[i + 1].keep = named[i + 1].keep || named[i].keep;
                continue;
            }
            if (named[i].keep && !open) {
                lo = named[i].at;
                open = true;
            } else if (!named[i].keep && open) {
                core.keep(old, lo, named[i].at);
                open = false;
            }
        }
        if (open) core.keep(old, lo, old.size());
    }
    // the index: the base level's, re-pointed
    auto remap = [&](size_t field_at, const char* what) {
        const int32_t v = rd32(idx, field_at);
        if (v <= 0) return true;
        size_t to = 0;
        if (!core.remap(static_cast<size_t>(v), to)) {
            err = fmt("the base level's %s (0x%x) is in no part kept", what, v);
            return false;
        }
        wr32(idx, field_at, static_cast<int32_t>(to));
        return true;
    };
    wr32(idx, 0x08, static_cast<int32_t>(o_tfrag));
    wr32(idx, 0x0c, 0);  // no occlusion
    wr32(idx, 0x10, static_cast<int32_t>(o_sky));
    wr32(idx, 0x14, static_cast<int32_t>(o_coll));
    for (int32_t k = 0; k < H[6]; ++k)
        if (!remap(static_cast<size_t>(H[7] + 32 * k), "moby class")) return false;
    wr32(idx, 0x20, 0);  // no tie classes
    wr32(idx, 0x28, 0);  // no shrub classes
    wr32(idx, 0x30, 0);  // no tfrag textures
    wr32(idx, 0x40, 0);  // no tie textures
    wr32(idx, 0x48, 0);  // no shrub textures
    for (int32_t k = 0; k < nmt; ++k) wr32(idx, static_cast<size_t>(H[15] + 16 * k), rd32(idx, static_cast<size_t>(H[15] + 16 * k)) - static_cast<int32_t>(m_lo));
    wr32(idx, 0x60, static_cast<int32_t>(o_texel));
    if (H[29] > 0 && size_t(H[29]) >= size_t(H[24]) && size_t(H[29]) < size_t(H[24]) + m_hi) {
        wr32(idx, 0x74, 0);  // (inside the base level's texels: a word the game never reads)
        log.say(fmt("the base level's index word +0x74 (0x%x, inside its texels, not read by the game): 0", H[29]));
    }
    for (size_t f : {0x64u, 0x68u, 0x74u, 0x7cu, 0xa0u, 0xa4u, 0xa8u})
        if (!remap(f, fmt("block +0x%zx", f).c_str())) return false;
    for (int k = 0; k < 256; ++k)
        if (!remap(static_cast<size_t>(H[30] + 4 * k), "hero sequence")) return false;
    for (int32_t k = 0; k < H[32]; ++k)
        if (!remap(static_cast<size_t>(H[33] + 16 * k), "gadget")) return false;
    std::vector<uint8_t> packed = wad_encode(core.buf);
    {
        std::vector<uint8_t> back;
        std::string why;
        if (!wad_decode_strict(packed, back, why) || back != core.buf) {
            err = "the asset data does not decode back through the game's decoder: " + why;
            return false;
        }
    }
    wr32(idx, 0x88, static_cast<int32_t>(packed.size()));
    wr32(idx, 0x8c, static_cast<int32_t>(core.buf.size()));
    // the budget: the base level's code places what follows its asset data for its own, so the level's must not be larger
    log.say(fmt("budget: asset data %.2f MB of the base level's %.2f MB (%.0f %%)", static_cast<double>(core.buf.size()) / 1048576.0,
                static_cast<double>(old.size()) / 1048576.0, 100.0 * static_cast<double>(core.buf.size()) / static_cast<double>(old.size())));
    if (core.buf.size() > old.size()) {
        err = fmt("the level is too large: its asset data (%.2f MB: the base level's classes, sky and effects, and the collision, %.2f MB) is larger "
                  "than the base level's (%.2f MB), which is what the game's memory holds there (make the collision smaller or coarser)",
                  static_cast<double>(core.buf.size()) / 1048576.0, static_cast<double>(coll.size()) / 1048576.0, static_cast<double>(old.size()) / 1048576.0);
        return false;
    }
    log.say(fmt("asset data: %zu bytes (tfrags none, sky 0x%zx, collision 0x%zx, texels 0x%zx; the base level's %zu kept), packed %zu (%.0f %%)", core.buf.size(), o_sky,
                o_coll, o_texel, old.size(), packed.size(), 100.0 * static_cast<double>(packed.size()) / static_cast<double>(core.buf.size())));
    // the data file: the base level's code, sound bank, VRAM image and HUD; the new index and asset data
    out.assign(0x80, 0);
    FileSpan np[11];
    for (int i = 0; i < 11; ++i) {
        const std::vector<uint8_t> b = i == 2 ? idx : i == 10 ? packed : part(i);
        const size_t at = align_up(out.size(), 0x40);
        out.resize(at, 0);
        out.insert(out.end(), b.begin(), b.end());
        np[i] = {static_cast<int32_t>(at), static_cast<int32_t>(b.size())};
    }
    std::memcpy(out.data(), np, sizeof np);
    out.resize(align_up(out.size(), 2048), 0);
    log.say(fmt("data file: %zu bytes (the base level's %zu)", out.size(), wad.size()));
    return true;
}

// a stamp as build.json writes it
std::string stamp_hex(uint64_t v) { return fmt("%016llx", static_cast<unsigned long long>(v)); }


// level.txt's import lines resolved (the pool's names, data/pool/names.txt, or <level>:<class>) with their closure (the
// classes each spawns: the pool's dependencies, tools/overlay_match/deps.py), each from its level unless the base level
// has it; written to the build's imports.txt for the game (src/game/enhance/session.cpp), one line a class to import
// A class profile as the composer checks level.txt against it (the game's reader: src/game/enhance/class_profiles.cpp):
// data/pool/profiles/L<level>_<class>.txt's slots (name -> kind) and the slots a spawn gives a default
struct ProfileSlots {
    bool found = false;
    std::map<std::string, std::string> kinds;
    std::set<std::string> defaults;
};
ProfileSlots profile_slots(int level, uint32_t cls) {
    ProfileSlots r;
    std::ifstream f(fmt("data/pool/profiles/L%d_%u.txt", level, cls));
    if (!f) return r;
    r.found = true;
    std::string line;
    while (std::getline(f, line)) {
        const size_t h = line.find('#');
        if (h != std::string::npos) line.resize(h);
        std::istringstream in(line);
        std::string k, a, b, c;
        if (!(in >> k)) continue;
        if (k == "slot" && in >> a >> b >> c) r.kinds[c] = b;
        if (k == "default" && in >> a) r.defaults.insert(a);
    }
    return r;
}

// level.txt against the class profiles (DESIGN 6.3): a moby line's slot=value words (format 3) name its class's profile's
// slots (unknown: warned), a path's or an area's value is a path line's name, a number else; a path or area slot the
// profile gives no default and the line no value is warned (the object keeps its placement's word: an index into the base
// level's own paths). Warnings only: the level is composed
void check_profiles(const LevelTxt& lt, const std::vector<LevelTxt::Moby>& mobys, int base, Log& log) {
    for (const LevelTxt::Moby& mb0 : mobys) {
        if (mb0.pool) continue;
        // (a class of another level placed: its own level's profile)
        LevelTxt::Moby mb = mb0;
        const int plevel = mb.ilevel >= 0 ? mb.ilevel : base;
        if (mb.ilevel >= 0) mb.cls = static_cast<int>(mb.icls);
        const ProfileSlots ps = profile_slots(plevel, static_cast<uint32_t>(mb.cls));
        if (!ps.found) {
            if (!mb.binds.empty())
                log.say(fmt("profiles: warning: level.txt:%d: class %d has no profile (data/pool/profiles/L%d_%d.txt): its slots %s are unknown, not set", mb.line,
                            mb.cls, plevel, mb.cls, mb.binds.front().first.c_str()));
            continue;
        }
        std::set<std::string> given;
        for (const auto& [slot, value] : mb.binds) {
            const auto k = ps.kinds.find(slot);
            if (k == ps.kinds.end()) {
                std::string names;
                for (const auto& [n, kind] : ps.kinds) names += (names.empty() ? "" : ", ") + n;
                log.say(fmt("profiles: warning: level.txt:%d: class %d has no slot \"%s\" (its profile's: %s): not set", mb.line, mb.cls, slot.c_str(), names.c_str()));
                continue;
            }
            given.insert(slot);
            if (k->second == "path" || k->second == "area") {
                bool have = false;
                for (const LevelTxt::Path& pa : lt.paths) have = have || pa.name == value;
                if (!have) log.say(fmt("profiles: warning: level.txt:%d: slot %s=%s: no path line \"%s\" (a %s slot takes a path's name)", mb.line, slot.c_str(), value.c_str(),
                                       value.c_str(), k->second.c_str()));
            } else {
                char* e = nullptr;
                (void)std::strtol(value.c_str(), &e, 0);
                if (value != "none" && (e == value.c_str() || *e != 0))
                    log.say(fmt("profiles: warning: level.txt:%d: slot %s=%s: a %s slot takes a number (or none)", mb.line, slot.c_str(), value.c_str(), k->second.c_str()));
            }
        }
        for (const auto& [n, kind] : ps.kinds)
            if ((kind == "path" || kind == "area") && given.count(n) == 0 && ps.defaults.count(n) == 0)
                log.say(fmt("profiles: warning: level.txt:%d: class %d's slot %s (%s) has no value and no default: it keeps its placement's (the base level's own "
                            "level data)", mb.line, mb.cls, n.c_str(), kind.c_str()));
    }
}

// a collision OBJ's faces as the collision's triangles (their surface type: `usemtl type<N>`, else `type`)
std::vector<CTri> collision_tris(const Obj& co, int type) {
    std::vector<CTri> out;
    for (const ObjTri& t : co.tris) {
        CTri c;
        for (int i = 0; i < 3; ++i) std::memcpy(c.p[static_cast<size_t>(i)].data(), t.v[i].p, sizeof t.v[i].p);
        c.type = static_cast<uint8_t>(type);
        if (t.material >= 0) {
            const std::string& n = co.materials[static_cast<size_t>(t.material)].name;
            if (n.rfind("type", 0) == 0 && n.size() > 4) c.type = static_cast<uint8_t>(std::clamp(std::atoi(n.c_str() + 4), 0, 255));
        }
        out.push_back(c);
    }
    return out;
}

// (format 3) a section's gameplay file: only its placements (the base level's classes, as place_mobys copies them) and what
// the game's rules need to place them (the pvar table and data of their own, the fix-up lists, the shared pvar data), in the
// gameplay file's layout (the other blocks absent): section_spawn_placements reads it as gameload_load reads a level's
bool build_section_gameplay(const std::vector<uint8_t>& g, LevelTxt& lt, std::vector<uint8_t>& out, Log& log, std::string& err) {
    uint32_t G[36];
    std::memcpy(G, g.data(), sizeof G);
    const uint32_t mo = G[0x44 / 4];
    if (mo == 0 || mo + 0x10 > g.size()) {
        err = "the base level has no mobys block";
        return false;
    }
    const int n1 = rd32(g, mo);
    int32_t hero_uid = -1;
    {
        size_t p = mo + 0x10;
        for (int i = 0; i < n1 && p + 0x20 <= g.size(); ++i) {
            const int sz = rd32(g, p);
            if (sz <= 0) break;
            if (rd32(g, p + 0x18) == 0) {
                hero_uid = rd32(g, p + 0xc);
                break;
            }
            p += static_cast<size_t>(sz);
        }
    }
    std::vector<uint8_t> recs;
    std::map<uint32_t, std::vector<uint8_t>> blocks;
    if (!lt.mobys.empty() && !place_mobys(g, G, mo, n1, hero_uid, lt, recs, blocks, log, err)) return false;
    // the pvar entries the placements name, renumbered from 0 with their data
    const std::vector<uint8_t>& table = blocks[0x54];
    const std::vector<uint8_t>& data = blocks[0x58];
    std::map<int32_t, int32_t> renum;
    std::vector<uint8_t> ntab, ndata;
    for (size_t p = 0; p + 0x78 <= recs.size();) {
        const int sz = rd32(recs, p);
        const int32_t pi = rd32(recs, p + 0x58);
        if (pi >= 0) {
            if (renum.count(pi) == 0) {
                const int32_t off = rd32(table, static_cast<size_t>(pi) * 8), size = rd32(table, static_cast<size_t>(pi) * 8 + 4);
                ndata.resize(align_up(ndata.size(), 0x10), 0);
                const int32_t ni = static_cast<int32_t>(ntab.size() / 8);
                wr32(ntab, ntab.size(), static_cast<int32_t>(ndata.size()));
                wr32(ntab, ntab.size(), size);
                ndata.insert(ndata.end(), data.begin() + off, data.begin() + off + size);
                renum[pi] = ni;
            }
            wr32(recs, p + 0x58, renum[pi]);
        }
        p += static_cast<size_t>(sz);
    }
    ndata.resize(align_up(ndata.size(), 0x10), 0);
    ntab.resize(align_up(ntab.size(), 0x10), 0);
    auto rows = [&](uint32_t field) {  // (a fix-up list's rows of the renumbered entries)
        std::vector<std::pair<int32_t, int32_t>> rs, out_rows;
        std::vector<uint8_t> end;
        fixup_rows(blocks[field], rs, end);
        for (const auto& [i, o] : rs)
            if (renum.count(i) != 0) out_rows.emplace_back(renum[i], o);
        return fixup_block(out_rows, end);
    };
    std::map<uint32_t, std::vector<uint8_t>> nb;
    std::vector<uint8_t> mb(0x10, 0);
    wr32(mb, 0, static_cast<int32_t>(lt.mobys.size()));
    mb.insert(mb.end(), recs.begin(), recs.end());
    nb[0x44] = mb;
    nb[0x54] = ntab;
    nb[0x58] = ndata;
    nb[0x50] = rows(0x50);
    nb[0x5c] = rows(0x5c);
    if (!lt.rails.empty()) nb[0x74] = grind_block(lt.rails);  // (stage 7) the section's rails
    if (const auto sh = blocks.find(0x4c); sh != blocks.end() && sh->second.size() >= 0x10) {
        const std::vector<uint8_t>& s = sh->second;
        const int32_t size = rd32(s, 0), n = rd32(s, 4);
        std::vector<uint8_t> b(s.begin(), s.begin() + 0x10 + size);
        int32_t kept = 0;
        for (int32_t k = 0; k < n; ++k) {
            const uint32_t w = static_cast<uint32_t>(rd32(s, 0x10 + static_cast<size_t>(size) + 8 * static_cast<size_t>(k)));
            const int32_t o = rd32(s, 0x10 + static_cast<size_t>(size) + 8 * static_cast<size_t>(k) + 4);
            const auto it = renum.find(static_cast<int32_t>(w & 0xffff));
            if (it == renum.end()) continue;
            wr32(b, b.size(), static_cast<int32_t>((w & 0xffff0000u) | static_cast<uint32_t>(it->second)));
            wr32(b, b.size(), o);
            ++kept;
        }
        wr32(b, 4, kept);
        b.resize(align_up(b.size(), 0x10), 0);
        if (kept > 0) nb[0x4c] = b;
    }
    std::vector<uint8_t> ng(0xa0, 0);
    for (const auto& [f, b] : nb) {
        const size_t at = align_up(ng.size(), 0x10);
        ng.resize(at, 0);
        ng.insert(ng.end(), b.begin(), b.end());
        wr32(ng, f, static_cast<int32_t>(at));
    }
    out = wad_encode(ng);
    std::vector<uint8_t> back;
    std::string why;
    if (!wad_decode_strict(out, back, why) || back != ng) {
        err = "the section's gameplay file does not decode back through the game's decoder: " + why;
        return false;
    }
    log.say(fmt("section gameplay: %zu bytes (%zu placements, %zu pvar entries, %zu pvar bytes, %zu rails), packed %zu", ng.size(), lt.mobys.size(), renum.size(),
                ndata.size(), lt.rails.size(), out.size()));
    return true;
}

// (format 3) a section composed (DESIGN 7.2): its collision block (the game's, as the level's own), its sky (a game level's
// block, as the level's), its meshes (more host meshes: its instances' range), its objects of the base level's classes (a
// gameplay file of the game's format with only them: the session places them with the game's rules, section_spawn_placements)
struct SectionOut {
    std::vector<uint8_t> coll, sky, gameplay;
    size_t rails = 0;
    size_t first = 0, count = 0;  // (its meshes' instances in host_meshes.mrpc)
    float death = 0;
    int objects = 0, imported = 0;
};
// (stage 8) a section's game level world: its level's placed objects the section brings (`objects`, `box`), each given a session
// uid after the others', their classes imported (the base level's own when it has them); what a world's section cannot have
bool compose_world(LevelTxt::Section& S, LevelTxt& lt, int base, int& uid_next, Log& log, std::string& err) {
    if (!S.meshes.empty()) {
        err = "a world's section draws its level's world: no meshes line (stage 8; its rail and rails lines add to its level's rails)";
        return false;
    }
    for (const LevelTxt::Moby& mb : S.mobys)
        if (mb.import.empty()) {
            err = fmt("moby line %d: a world's section places its level's objects (an objects line) and classes of other levels, not the base level's own", mb.line);
            return false;
        }
    if (S.offset[0] != 0 || S.offset[1] != 0 || S.offset[2] != 0) {
        err = "a world's section is where its level has it: no offset (its lines are in its level's coordinates)";
        return false;
    }
    if (S.world == base) {
        err = fmt("level %d's world is the base level's: a world's section of another level", S.world);
        return false;
    }
    if (!disc_is_open() && !disc_open()) {
        err = "the game's disc cannot be read: a world's section needs the asset pool, made from it (./setup.sh first)";
        return false;
    }
    if (!pool_open(true)) {
        err = "the asset pool cannot be made (a world's section needs it: its level's context and level data)";
        return false;
    }
    // its level's gameplay file: the placements (+0x44: a count, then records of their own size: +0xc the uid, +0x18 the class,
    // +0x30 the position)
    const fs::path gpath = fs::path("assets/disc") / fmt("levels/level%02d", S.world) / "gameplay_ntsc.bin";
    std::vector<uint8_t> packed, g;
    if (!read_file(gpath, packed) || !decompress_wad(std::span<const std::byte>(reinterpret_cast<const std::byte*>(packed.data()), packed.size()), g)) {
        err = gpath.generic_string() + " cannot be read";
        return false;
    }
    std::set<uint32_t> want;
    for (const std::string& w : S.objects) {
        int l = S.world;
        uint32_t c = 0;
        if (!w.empty() && w.find_first_not_of("0123456789") == std::string::npos) c = static_cast<uint32_t>(std::atoi(w.c_str()));
        else if (!pool_name(w, &l, &c)) {
            err = fmt("objects %s: no such class (its level's class id, a name of data/pool/names.txt, or <level>:<class>)", w.c_str());
            return false;
        }
        if (l != S.world) {
            err = fmt("objects %s: level %d's class, not the world's (level %d)", w.c_str(), l, S.world);
            return false;
        }
        want.insert(c);
    }
    const size_t mo = static_cast<size_t>(rd32(g, 0x44));
    if (mo == 0 || mo + 0x10 > g.size()) {
        err = gpath.generic_string() + ": no placements";
        return false;
    }
    const int32_t n1 = rd32(g, mo);
    size_t p = mo + 0x10, total = 0;
    std::map<uint32_t, int> classes;
    const int first = uid_next + 1;
    for (int32_t k = 0; k < n1 && p + 0x40 <= g.size(); ++k) {
        const int32_t sz = rd32(g, p);
        if (sz < 0x40 || p + static_cast<size_t>(sz) > g.size()) break;
        const int32_t uid = rd32(g, p + 0xc);
        const uint32_t cls = static_cast<uint32_t>(rd32(g, p + 0x18));
        float at[3];
        std::memcpy(at, g.data() + p + 0x30, 12);
        p += static_cast<size_t>(sz);
        if (cls == 0) continue;  // (the hero's record)
        ++total;
        if (!S.objects_all && want.count(cls) == 0) continue;
        if (S.has_box && (at[0] < S.box_lo[0] || at[0] > S.box_hi[0] || at[1] < S.box_lo[1] || at[1] > S.box_hi[1] || at[2] < S.box_lo[2] || at[2] > S.box_hi[2]))
            continue;
        S.wuids.push_back({uid, ++uid_next});
        ++classes[cls];
    }
    std::string list;
    for (const auto& [c, nc] : classes) {
        list += fmt("%s%u x%d%s", list.empty() ? "" : ", ", c, nc, pool_find(base, c) != nullptr ? " (the base's)" : "");
        LevelTxt::Import im;
        im.name = fmt("%d:%u", S.world, c);
        im.line = S.world_line;
        im.world = S.name;
        bool have = false;
        for (const LevelTxt::Import& o : lt.imports) have = have || o.name == im.name;
        if (!have) lt.imports.push_back(im);
    }
    log.say(fmt("section %s: level %d's world (its geometry, collision%s, sky%s, music, lights, fog, level data from the disc when it streams in); %zu of its %zu "
                "objects%s%s%s, uids %d..%d",
                S.name.c_str(), S.world, S.collision.empty() ? "" : " (its own instead)", (S.sky_level >= 0 ? " (level " + std::to_string(S.sky_level) + "'s instead)" : std::string()).c_str(),
                S.wuids.size(), total, S.has_box ? " in its box" : "", classes.empty() ? "" : ": ", list.c_str(), first, uid_next));
    return true;
}

bool compose_sections(const fs::path& ldir, LevelTxt& lt, int base, const std::vector<uint8_t>& base_gp, mrpw::HostMeshFile& hm,
                      const std::vector<ObjMaterial>& mats, std::vector<SectionOut>& outs, Log& log, std::string& err) {
    int uid_next = lt.uid_first;
    for (LevelTxt::Section& S : lt.sections) {
        SectionOut o;
        LevelTxt slt = lt;
        for (int l = 0; l < 3; ++l) slt.offset[l] = lt.offset[l] + S.offset[l];
        auto fail = [&](const std::string& why) {
            err = fmt("level.txt:%d: section %s: ", S.line, S.name.c_str()) + why;
            return false;
        };
        o.death = S.has_death ? S.death : lt.death;
        // a planet's world is one section: the level state its objects read and write is the planet's, one copy in the engine (its
        // moby groups, the camera links of its cameras, its rails and host meshes): two sections of it would each overwrite and empty
        // the other's (a boss beaten in one and the other's objects reading a group list that is gone: an out-of-range read)
        if (S.world >= 0)
            for (const LevelTxt::Section& P : lt.sections) {
                if (&P == &S) break;
                if (P.world == S.world)
                    return fail(fmt("level %d's world is already section %s's (level.txt:%d). A planet's world is one section: its level state "
                                    "(the moby groups its objects read, its cameras' links, its rails, the host meshes drawn in it) is the planet's, one copy "
                                    "in the game, not one per section. Use one section for it, with several entries (and a box that holds them all)",
                                    S.world, P.name.c_str(), P.world_line));
            }
        if (S.world >= 0 && !compose_world(S, lt, base, uid_next, log, err)) return fail(err);
        if (S.zones_line != 0 && S.world < 0) {
            err = fmt("level.txt:%d: section %s: \"zones\" is a world's section's line (its level's zone grid): no world line", S.zones_line, S.name.c_str());
            return false;
        }
        if (S.zones_all) log.say(fmt("section %s: every zone of level %d's world drawn (zones all), wherever the camera is", S.name.c_str(), S.world));
        // (stage 10) its boss lines: each a class of its world's level (an id there), a pool name or <level>:<class>; a class it does
        // not place (by `objects` or a `moby` line) is refused
        for (const std::string& w : S.bosses) {
            int l = S.world >= 0 ? S.world : base;
            uint32_t c = 0;
            if (!w.empty() && w.find_first_not_of("0123456789") == std::string::npos) c = static_cast<uint32_t>(std::atoi(w.c_str()));
            else if (!pool_name(w, &l, &c)) return fail(fmt("boss %s (line %d): no such class (its world's class id, a name of data/pool/names.txt, or <level>:<class>)", w.c_str(), S.boss_line));
            bool placed = false;
            for (const LevelTxt::Moby& mb : S.mobys)
                placed = placed || (mb.import.empty() ? (l == base && static_cast<uint32_t>(mb.cls) == c) : (mb.import == w || (mb.ilevel == l && mb.icls == c)));
            if (S.world == l) {
                if (S.objects_all) placed = true;
                for (const std::string& o : S.objects) {
                    int ol = S.world;
                    uint32_t oc = 0;
                    if (!o.empty() && o.find_first_not_of("0123456789") == std::string::npos) oc = static_cast<uint32_t>(std::atoi(o.c_str()));
                    else pool_name(o, &ol, &oc);
                    placed = placed || (ol == l && oc == c);
                }
            }
            if (!placed) return fail(fmt("boss %s (line %d): the section places no object of class %d:%u (an objects or moby line)", w.c_str(), S.boss_line, l, c));
            S.boss_classes.emplace_back(l, c);
        }
        bool strips = false;
        for (const LevelTxt::Rail& r : S.rails) strips = strips || r.strip;
        if (S.world >= 0 && S.collision.empty() && strips) {  // (stage 10: the mod's rails in a world: their strips in its level's collision)
            if (!world_collision_with_rails(S.world, S.rails, static_cast<uint8_t>(lt.collision_type), o.coll, log, err)) return fail(err);
        }
        if (!S.collision.empty()) {
            Obj co;
            if (!read_obj(ldir / S.collision, slt, false, co, err)) return fail(err);
            std::vector<CTri> ct = collision_tris(co, lt.collision_type);
            if (ct.empty()) return fail(S.collision + " has no faces");
            for (const LevelTxt::Rail& r : S.rails)
                if (r.strip) rail_strip(r, static_cast<uint8_t>(lt.collision_type), ct);  // (stage 7) its rails' strips
            if (!build_collision(ct, o.coll, log, err)) return fail(err);
            float lo = 1e30f;
            for (const CTri& c : ct)
                for (const auto& v : c.p) lo = std::min(lo, v[2]);
            if (!S.has_death && lt.death > lo - 4.0f) o.death = std::floor(lo) - 8.0f;  // (as the level's: 8 under its lowest point)
        }
        if (S.sky_level >= 0) {
            std::vector<uint8_t> w, c;
            int32_t SH[48];
            const fs::path sp = fs::path("assets/disc") / fmt("levels/level%02d", S.sky_level) / "data.wad";
            if (!read_file(sp, w) || !data_core(w, c, SH, err)) return fail(sp.generic_string() + ": its sky cannot be read (" + err + ")");
            if (SH[4] <= 0 || SH[5] <= SH[4] || static_cast<size_t>(SH[5]) > c.size()) return fail(fmt("level %d has no sky", S.sky_level));
            o.sky.assign(c.begin() + SH[4], c.begin() + SH[5]);
        }
        o.first = hm.instances.size();
        if (!S.meshes.empty()) {
            Obj mo;
            if (!read_obj(ldir / S.meshes, slt, true, mo, err)) return fail(err);
            if (!build_host_meshes(mo, hm, log, err)) return fail(err);
        }
        if (S.world < 0 && !rails_meshes(S.rails, mats, hm, log, err)) return fail(err);  // (stage 7) its rails' models (a world's: after the classes')
        o.count = hm.instances.size() - o.first;
        slt.mobys.clear();
        for (const LevelTxt::Moby& mb : S.mobys)
            if (mb.import.empty()) slt.mobys.push_back(mb);
        o.objects = static_cast<int>(slt.mobys.size());
        o.imported = static_cast<int>(S.mobys.size()) - o.objects;
        o.rails = S.rails.size();
        slt.rails = S.rails;
        if (!slt.mobys.empty() || !slt.rails.empty()) {
            slt.pools.clear();
            slt.ship = false;
            slt.format = 2;
            slt.uid_first = uid_next;
            if (!build_section_gameplay(base_gp, slt, o.gameplay, log, err)) return fail(err);
            size_t k = 0;
            for (LevelTxt::Moby& mb : S.mobys) {
                if (!mb.import.empty()) continue;
                mb.uid = slt.mobys[k].uid;
                mb.pvars = slt.mobys[k].pvars;
                uid_next = std::max(uid_next, mb.uid);
                ++k;
            }
        }
        log.say(fmt("section %s (%s when left%s, respawn %s): collision %s, sky %s, %zu meshes, %d objects of the base level's classes%s, %d of other levels', "
                    "%zu entries, %zu checkpoints, %zu portals, death height %.1f",
                    S.name.c_str(), S.policy.c_str(), S.fresh ? ", fresh at every entry" : "", S.respawn.empty() ? "the session's" : S.respawn.c_str(),
                    o.coll.empty() ? (S.world >= 0 ? "its world's" : "the session's") : fmt("%.1f KB", static_cast<double>(o.coll.size()) / 1024.0).c_str(),
                    S.sky_level >= 0 ? fmt("level %d's (%zu bytes)", S.sky_level, o.sky.size()).c_str() : S.world >= 0 ? "its world's" : "the session's", o.count,
                    o.objects,
                    o.objects > 0 ? fmt(" (uids %d..%d)", slt.uid_first + 1, uid_next).c_str() : "", o.imported, S.entries.size(), S.checkpoints.size(),
                    S.portals.size(), o.death));
        outs.push_back(std::move(o));
    }
    if (uid_next >= 0x7ff) {
        err = fmt("too many objects: their uids reach %d (the game keeps its records of at most 2047)", uid_next);
        return false;
    }
    return true;
}

bool compose_imports(const LevelTxt& lt, int base, const fs::path& out_dir, Log& log, std::string& err) {
    std::error_code ec;
    if (lt.imports.empty()) {
        fs::remove(out_dir / "imports.txt", ec);
        return true;
    }
    if (!disc_is_open() && !disc_open()) {
        err = "the game's disc cannot be read: the import lines need the asset pool, made from it (./setup.sh first)";
        return false;
    }
    if (!pool_open(true)) {
        err = "the asset pool cannot be made (the import lines need it)";
        return false;
    }
    if (!fs::exists(fs::path(pool_dir()) / "deps", ec)) {  // (the classes' dependencies: made once, from the generated code)
        const int r = platform::run_quiet({platform::python_program(), "tools/overlay_match/deps.py"});
        log.say(r == 0 ? "imports: the classes' dependencies made (tools/overlay_match/deps.py)"
                       : "imports: warning: the classes' dependencies could not be made (tools/overlay_match/deps.py): the imports bring no dependencies");
    }
    struct Want {
        int level;
        uint32_t cls;
        std::string why;
    };
    std::vector<Want> todo;
    for (const LevelTxt::Import& im : lt.imports) {
        int l = -1;
        uint32_t c = 0;
        if (!pool_name(im.name, &l, &c)) {
            err = fmt("level.txt:%d: import %s: no such class (a name of data/pool/names.txt, or <level>:<class>)", im.line, im.name.c_str());
            return false;
        }
        const PoolClass* row = pool_find(l, c);
        if (row == nullptr && !im.world.empty() && pool_find(base, c) == nullptr) {  // (a world's object of a class its level does not have)
            log.say(fmt("imports: section %s's objects of class %d:%u (not in the pool) are not placed", im.world.c_str(), l, c));
            continue;
        }
        if ((row == nullptr || row->block_size == 0) && im.world.empty()) {
            err = fmt("level.txt:%d: import %s: level %d has no class %u with data", im.line, im.name.c_str(), l, c);
            return false;
        }
        todo.push_back({l, c, !im.world.empty() ? "world-of-" + im.world : "imported (level.txt:" + std::to_string(im.line) + ")"});
    }
    std::string out = "# the classes the level imports from other levels, with their closure (the composer's: docs/open-engine.md)\n"
                      "# import <level> <class> <name> <why>\n";
    std::set<std::pair<int, uint32_t>> seen;
    size_t n = 0, native = 0;
    for (size_t k = 0; k < todo.size(); ++k) {
        const Want w = todo[k];
        if (!seen.insert({w.level, w.cls}).second) continue;
        const bool root = w.why.rfind("imported", 0) == 0;
        // (the base level's own classes: its own serve, a class id being the same class on every level)
        if (w.level == base || (!root && pool_find(base, w.cls) != nullptr)) {
            ++native;
            if (root) log.say(fmt("import %s: the base level's own class", pool_name_of(w.level, w.cls).c_str()));
            continue;
        }
        const PoolClass* row = pool_find(w.level, w.cls);
        // (a world's object of a class of code alone - no block, an update function: its level's lava sea, a trigger - is
        // imported as its level registers it, with no header; composer 9)
        const bool code_alone = row != nullptr && row->block_size == 0 && row->update_fn != 0 && w.why.rfind("world-of-", 0) == 0;
        if (row == nullptr || (row->block_size == 0 && !code_alone)) {
            log.say(fmt("import: warning: %d:%u (%s) has no data in the pool: not imported", w.level, w.cls, w.why.c_str()));
            continue;
        }
        out += fmt("import %d %u %s %s\n", w.level, w.cls, pool_name_of(w.level, w.cls).c_str(), w.why.c_str());
        ++n;
        const PoolDeps* d = pool_deps(w.level, w.cls);
        std::string line = fmt("import %s (%d:%u, %s): %u KB, %zu sounds", pool_name_of(w.level, w.cls).c_str(), w.level, w.cls, w.why.c_str(),
                               row->block_size >> 10, row->sounds.size());
        if (d != nullptr) {
            auto list = [&](const std::vector<uint32_t>& v) {
                std::string r;
                for (uint32_t c : v) r += (r.empty() ? "" : ",") + std::to_string(c) + (pool_find(base, c) != nullptr ? " (the base's)" : "");
                return r.empty() ? std::string("none") : r;
            };
            line += "; spawns " + list(d->own) + "; the engine for it " + list(d->engine) + "; seen spawning " + list(d->seen) + "; reads " + d->reads;
            // (the profiles: a class that reads its planet's level data and has none is spawned as its first placement there)
            if (root && d->reads != "-" && !d->reads.empty() && !profile_slots(w.level, w.cls).found)
                line += fmt("\n[compose] profiles: warning: %s reads its planet's level data (%s) but has no profile (data/pool/profiles/L%d_%u.txt): spawned "
                            "as its first placement there, its values indices into its planet's data (its registry's); the plugin cannot bind its slots",
                            pool_name_of(w.level, w.cls).c_str(), d->reads.c_str(), w.level, w.cls);
            const std::string by = std::to_string(w.level) + ":" + std::to_string(w.cls);
            for (uint32_t c : d->own) todo.push_back({w.level, c, "spawned-by-" + by});
            for (uint32_t c : d->engine) todo.push_back({w.level, c, "spawned-for-" + by});
            for (uint32_t c : d->seen) todo.push_back({w.level, c, "seen-spawned-by-" + by});
        } else {
            line += "; no dependencies known";
        }
        log.say(line);
    }
    if (!write_file(out_dir / "imports.txt", std::vector<uint8_t>(out.begin(), out.end()))) {
        err = "cannot write into " + out_dir.generic_string();
        return false;
    }
    log.say(fmt("imports: %zu classes from other levels (%zu lines, their closure), %zu the base level's own", n, lt.imports.size(), native));
    return true;
}

}  // namespace

// orig: (host) see level_compose.h
bool compose_level_mod(const std::string& folder, bool force) {
    ModInfo m;
    m.folder = folder;
    m.dir = mods_dir() / folder;
    if (!mods_manifest_read(m)) {
        std::cerr << "[compose] " << folder << ": " << m.error << "\n";
        return false;
    }
    if (!m.level) {
        std::cerr << "[compose] " << folder << ": not a level mod (its mod.json has no \"kind\": \"level\")\n";
        return false;
    }
    const fs::path out_dir = mods_level_build_dir(folder);
    if (!force && mods_level_build_check(m).empty()) {
        std::cout << "[compose] " << folder << ": up to date\n";
        return true;
    }
    Log log;
    log.say(fmt("%s: the level \"%s\" (format %d) on level %d", folder.c_str(), m.level_id.c_str(), m.level_format, m.level_base));
    const fs::path ldir = m.dir / "level";
    LevelTxt lt;
    std::string err;
    auto fail = [&](const std::string& why) {
        std::cerr << "[compose] " << folder << ": " << why << "\n";
        return false;
    };
    if (!read_level_txt(ldir / "level.txt", lt, err)) return fail(err);
    lt.format = m.level_format;
    if (m.level_format < 3) {  // (the session's lines: format 3's)
        int line = lt.session_line != 0 ? lt.session_line : lt.respawn_line != 0 ? lt.respawn_line : !lt.checkpoints.empty() ? lt.checkpoints.front().line : 0;
        for (const LevelTxt::Moby& mb : lt.mobys)
            if (line == 0 && (!mb.binds.empty() || !mb.import.empty())) line = mb.line;
        if (line == 0 && !lt.sections.empty()) line = lt.sections.front().line;
        if (line == 0 && !lt.portals.empty()) line = lt.portals.front().line;
        if (line == 0 && !lt.entries.empty()) line = lt.entries.front().line;
        if (line == 0) line = lt.start_line != 0 ? lt.start_line : lt.leave_line;
        if (line == 0 && lt.hero_ctx.level != kHeroGeneric) line = lt.hero_ctx.line;  // (`hero generic`: format 2 too)
        if (line != 0)
            return fail(fmt("level.txt:%d: a session's line (session, checkpoint, respawn, section, portal, entry, start, leave, hero, a moby line's slots or "
                            "another level's class): format 3 only (mod.json's level.format is %d)",
                            line, m.level_format));
    }
    // (format 3) the sections' names, entries and checkpoints: a portal's and start's target there, a checkpoint's name once
    {
        auto entries_of = [&](const std::string& s) -> const std::vector<LevelTxt::Entry>* {
            if (s == "main") return &lt.entries;
            for (const LevelTxt::Section& S : lt.sections)
                if (S.name == s) return &S.entries;
            return nullptr;
        };
        auto target = [&](const std::string& s, const std::string& e, int line) -> std::string {
            const std::vector<LevelTxt::Entry>* es = entries_of(s);
            if (es == nullptr) return fmt("level.txt:%d: no section \"%s\"", line, s.c_str());
            for (const LevelTxt::Entry& x : *es)
                if (x.name == e) return "";
            return fmt("level.txt:%d: the section \"%s\" has no entry \"%s\"", line, s.c_str(), e.c_str());
        };
        std::set<std::string> cps;
        for (const LevelTxt::Checkpoint& c : lt.checkpoints) cps.insert(c.name);
        for (const LevelTxt::Portal& pt : lt.portals)
            if (const std::string w = target(pt.to, pt.entry, pt.line); !w.empty()) return fail(w);
        for (const LevelTxt::Section& S : lt.sections) {
            for (const LevelTxt::Portal& pt : S.portals)
                if (const std::string w = target(pt.to, pt.entry, pt.line); !w.empty()) return fail(w);
            for (const LevelTxt::Checkpoint& c : S.checkpoints)
                if (!cps.insert(c.name).second) return fail(fmt("level.txt:%d: the checkpoint \"%s\" twice", c.line, c.name.c_str()));
        }
        if (lt.start_line != 0)
            if (const std::string w = target(lt.start_section, lt.start_entry, lt.start_line); !w.empty()) return fail(w);
        if (lt.sections.size() > 31) return fail("more than 31 sections");
    }
    if (lt.base >= 0 && lt.base != m.level_base)
        return fail(fmt("level.txt's base is level %d, mod.json's level.base %d: make them the same (mod.json's decides which code the level runs)", lt.base, m.level_base));
    // the offset: `offset auto` as far as the collision's lowest corner needs to be 16 units above 0 (in whole cells of 4)
    if (lt.offset_auto) {
        Obj probe;
        if (!read_obj(ldir / lt.collision, lt, false, probe, err)) return fail(err);
        float lo[3] = {1e30f, 1e30f, 1e30f};
        for (const ObjTri& t : probe.tris)
            for (const ObjVert& v : t.v)
                for (int l = 0; l < 3; ++l) lo[l] = std::min(lo[l], v.p[l]);
        for (int l = 0; l < 3; ++l) lt.offset[l] = !probe.tris.empty() && lo[l] < 16.0f ? std::ceil((16.0f - lo[l]) / 4.0f) * 4.0f : 0.0f;
    }
    if (lt.offset[0] != 0 || lt.offset[1] != 0 || lt.offset[2] != 0) {
        for (int l = 0; l < 3; ++l) {
            lt.hero[l] += lt.offset[l];
            lt.ship_at[l] += lt.offset[l];
            for (LevelTxt::Moby& mb : lt.mobys) mb.at[l] += lt.offset[l];
            for (LevelTxt::Object& ob : lt.objects)
                if (ob.section == 0) ob.at[l] += lt.offset[l];  // (a section's: below)
            for (LevelTxt::Trigger& tg : lt.triggers) {
                tg.lo[l] += lt.offset[l];
                tg.hi[l] += lt.offset[l];
            }
            for (LevelTxt::Path& pa : lt.paths)
                for (auto& v : pa.pts) v[static_cast<size_t>(l)] += lt.offset[l];
            for (LevelTxt::Checkpoint& c : lt.checkpoints) c.at[l] += lt.offset[l];
            for (LevelTxt::Entry& e : lt.entries) e.at[l] += lt.offset[l];
            for (LevelTxt::Portal& pt : lt.portals) {
                pt.lo[l] += lt.offset[l];
                pt.hi[l] += lt.offset[l];
            }
        }
        lt.death += lt.offset[2];
        log.say(fmt("offset: (%g %g %g) added to every coordinate%s (lights.txt's are the game's: not moved)", lt.offset[0], lt.offset[1], lt.offset[2],
                    lt.offset_auto ? " (auto)" : ""));
    }
    // hero auto: where the landing scene leaves Ratchet (the same in every ship's scene: measured, the game's
    // RAC1_LEVEL_TRACE "the landing's end" line), in the ship's frame (x along its yaw, y to its left), facing away from it
    if (lt.hero_auto) {
        constexpr float kStepX = 2.513f, kStepY = 3.526f;
        const float yaw = lt.ship_at[3], c = std::cos(yaw), s = std::sin(yaw);
        lt.hero[0] = lt.ship_at[0] + c * kStepX - s * kStepY;
        lt.hero[1] = lt.ship_at[1] + s * kStepX + c * kStepY;
        lt.hero[2] = lt.ship_at[2];
        lt.hero[3] = std::remainder(yaw + std::atan2(kStepY, kStepX), 6.2831853f);
        log.say(fmt("hero auto: Ratchet steps off the ship at (%g %g %g), turned %g", lt.hero[0], lt.hero[1], lt.hero[2], lt.hero[3]));
    }
    // (format 3) the sections' lines moved by the level's offset and their own
    for (LevelTxt::Section& S : lt.sections) {
        for (int l = 0; l < 3; ++l) {
            const float o = S.world >= 0 ? 0.0f : lt.offset[l] + S.offset[l];  // (stage 8: a world's section's lines are its level's coordinates)
            for (LevelTxt::Moby& mb : S.mobys) mb.at[l] += o;
            for (LevelTxt::Object& ob : lt.objects)
                if (ob.section == static_cast<int>(&S - lt.sections.data()) + 1) ob.at[l] += o;  // (its object lines)
            for (LevelTxt::Checkpoint& c : S.checkpoints) c.at[l] += o;
            for (LevelTxt::Entry& e : S.entries) e.at[l] += o;
            for (LevelTxt::Portal& pt : S.portals) {
                pt.lo[l] += o;
                pt.hi[l] += o;
            }
        }
        if (S.has_death && S.world < 0) S.death += lt.offset[2] + S.offset[2];
    }
    // (stage 7) the grind rails: their files read, their points moved and densified; their names once in the level
    size_t rails_all = 0;
    {
        if (!rails_resolve(ldir, lt, lt.offset, lt.rails, err)) return fail(err);
        std::set<std::string> names;
        auto check = [&](const std::vector<LevelTxt::Rail>& rs, const char* where) {
            for (const LevelTxt::Rail& r : rs)
                if (!names.insert(r.name).second) return fmt("level.txt:%d: the rail %s twice", r.line, r.name.c_str());
            if (rs.size() > kRails) return fmt("%s: more than %zu rails", where, kRails);
            return std::string();
        };
        if (const std::string w = check(lt.rails, "main"); !w.empty()) return fail(w);
        for (LevelTxt::Section& S : lt.sections) {
            float o[3] = {lt.offset[0] + S.offset[0], lt.offset[1] + S.offset[1], lt.offset[2] + S.offset[2]};
            if (S.world >= 0) o[0] = o[1] = o[2] = 0;  // (a world's section: its lines are in its level's coordinates)
            if (!rails_resolve(ldir, lt, o, S.rails, err)) return fail(err);
            if (const std::string w = check(S.rails, S.name.c_str()); !w.empty()) return fail(w);
        }
        rails_all = names.size();
        auto say = [&](const std::vector<LevelTxt::Rail>& rs, const std::string& where) {
            for (const LevelTxt::Rail& r : rs)
                log.say(fmt("rail %s (%s): %zu points, %.1f units%s%s%s", r.name.c_str(), where.c_str(), r.pts.size(), rail_length(r), r.loop ? ", a loop" : "",
                            r.mesh ? ", its model" : "", r.strip ? "" : ", no collision strip"));
        };
        say(lt.rails, "main");
        for (const LevelTxt::Section& S : lt.sections) say(S.rails, "section " + S.name);
    }
    // (stage 9) the hero lines: a level with hero or camera code of its own (Rilgar, Orxon, Gemlik, Quartu, Kalebo III), a mode's
    // class imported (Clank 10:87, Giant Clank 15:419, a board 16:439 or Rilgar's 5:439)
    {
        auto check = [&](const LevelTxt::Hero& h, const std::string& where) -> std::string {
            if (h.line == 0) return "";
            if (h.level == kHeroGeneric) {
                if (m.level_base != 5 && m.level_base != 15 && m.level_base != 16)
                    return fmt("level.txt:%d: the base level %d's hero code is the generic one already (hero generic is for Rilgar 5, Quartu 15, Kalebo III 16)",
                               h.line, m.level_base);
                if (!h.mode.empty() && m.level_format < 3) return fmt("level.txt:%d: a hero mode is format 3's (a session's)", h.line);
                log.say(fmt("%s: Ratchet runs on the generic hero code (Veldin's: its lava, its states), not level %d's own", where.c_str(), m.level_base));
                if (h.mode.empty()) return "";
            }
            if (h.level != kHeroGeneric && h.level != 5 && h.level != 10 && h.level != 13 && h.level != 15 && h.level != 16 && h.level != m.level_base)
                return fmt("level.txt:%d: level %d has no hero code of its own to run Ratchet (Rilgar 5, Orxon 10, Gemlik 13, Quartu 15, Kalebo III 16)", h.line,
                           h.level);
            if (h.mode == "board" && h.level != 16 && h.level != 5)
                return fmt("level.txt:%d: the board needs the hero code of a level that rides it: hero kalebo board (or rilgar)", h.line);
            std::string imp;
            if (h.mode == "clank") imp = "10:87";
            if (h.mode == "giantclank") imp = "15:419";
            if (h.mode == "board") imp = fmt("%d:439", h.level);
            if (!imp.empty()) {
                bool have = false;
                for (const LevelTxt::Import& o : lt.imports) have = have || o.name == imp;
                if (!have) {
                    LevelTxt::Import im;
                    im.name = imp;
                    im.line = h.line;
                    lt.imports.push_back(im);
                }
            }
            if (h.level != kHeroGeneric)
            log.say(fmt("%s: Ratchet runs on level %d's hero code and camera code%s", where.c_str(), h.level,
                        h.mode.empty() ? "" : (", as " + std::string(h.mode == "clank" ? "Clank" : h.mode == "giantclank" ? "Giant Clank" : "on the hoverboard") +
                                               " (" + imp + ", spawned at his entry)").c_str()));
            return "";
        };
        if (const std::string w = check(lt.hero_ctx, "main"); !w.empty()) return fail(w);
        for (const LevelTxt::Section& S : lt.sections)
            if (const std::string w = check(S.hero, "section " + S.name); !w.empty()) return fail(w);
    }
    // (format 3) classes of other levels placed (moby lines naming one): spawned by the session with their section, each an
    // import of the level (added when no import line names it)
    std::vector<LevelTxt::Moby> main_imports;
    for (auto it = lt.mobys.begin(); it != lt.mobys.end();) {
        if (it->import.empty()) {
            ++it;
            continue;
        }
        main_imports.push_back(*it);
        it = lt.mobys.erase(it);
    }
    {
        auto want = [&](const LevelTxt::Moby& mb) {
            for (const LevelTxt::Import& im : lt.imports)
                if (im.name == mb.import) return;
            LevelTxt::Import im;
            im.name = mb.import;
            im.line = mb.line;
            lt.imports.push_back(im);
        };
        for (const LevelTxt::Moby& mb : main_imports) want(mb);
        for (const LevelTxt::Section& S : lt.sections)
            for (const LevelTxt::Moby& mb : S.mobys)
                if (!mb.import.empty()) want(mb);
    }
    // the spawn pools: their objects after the placed ones (where Ratchet starts: the game takes them out of play when the
    // level loads, before their first update; base_spawn starts them again where the plugin says)
    const size_t placed_objects = lt.mobys.size();
    for (const LevelTxt::Pool& pl : lt.pools)
        for (int k = 0; k < pl.count; ++k) {
            LevelTxt::Moby mb;
            mb.cls = pl.cls;
            mb.line = pl.line;
            mb.pool = true;
            std::memcpy(mb.at, lt.hero, sizeof mb.at);
            lt.mobys.push_back(mb);
        }
    // the collision
    Obj co;
    if (!read_obj(ldir / lt.collision, lt, false, co, err)) return fail(err);
    std::vector<CTri> ctris;
    for (const ObjTri& t : co.tris) {
        CTri c;
        for (int i = 0; i < 3; ++i) std::memcpy(c.p[static_cast<size_t>(i)].data(), t.v[i].p, sizeof t.v[i].p);
        c.type = static_cast<uint8_t>(lt.collision_type);
        if (t.material >= 0) {
            const std::string& n = co.materials[static_cast<size_t>(t.material)].name;
            if (n.rfind("type", 0) == 0 && n.size() > 4) c.type = static_cast<uint8_t>(std::clamp(std::atoi(n.c_str() + 4), 0, 255));
        }
        ctris.push_back(c);
    }
    {  // (stage 7) the rails' collision strips: main's, and those of the sections that keep main's collision
        const size_t before = ctris.size();
        for (const LevelTxt::Rail& r : lt.rails)
            if (r.strip) rail_strip(r, static_cast<uint8_t>(lt.collision_type), ctris);
        for (const LevelTxt::Section& S : lt.sections)
            if (S.collision.empty())
                for (const LevelTxt::Rail& r : S.rails)
                    if (r.strip) rail_strip(r, static_cast<uint8_t>(lt.collision_type), ctris);
        if (ctris.size() != before) log.say(fmt("rails: their collision strips, %zu triangles", ctris.size() - before));
    }
    std::vector<uint8_t> coll;
    if (!build_collision(ctris, coll, log, err)) return fail(err);
    for (const CTri& c : ctris)
        for (const auto& v : c.p) {
            lt.floor = lt.has_floor ? std::min(lt.floor, v[2]) : v[2];
            lt.has_floor = true;
        }
    // the base level's own kill heights in its hero code (hero_transitions.cpp's kill_fall_input: the fall below the death
    // height ends at once under them, else after two seconds; Kalebo III's also after its gliding): the level stays above
    if (const float kh = m.level_base == 3 ? 5.0f : m.level_base == 16 ? 77.0f : 0.0f; kh > 0 && lt.has_floor && lt.floor < kh)
        log.say(fmt("warning: the collision reaches z %.1f, below the base level's own kill height %.0f: a fall below the death height there ends at "
                    "once (and on Kalebo III's code a glide's fall too); keep the level above %.0f (offset) for the game's usual fall",
                    lt.floor, kh, kh));
    // the base level's files (the player's disc, as the setup extracted it)
    char base_dir[32];
    std::snprintf(base_dir, sizeof base_dir, "levels/level%02d", m.level_base);
    const fs::path bdir = fs::path("assets/disc") / base_dir;
    std::vector<uint8_t> base_wad, base_gp_file, base_gp;
    if (!read_file(bdir / "data.wad", base_wad) || !read_file(bdir / "gameplay_ntsc.bin", base_gp_file))
        return fail("the base level's files are missing (" + bdir.generic_string() + "): run the setup with your disc first");
    if (!decompress_wad(std::span<const std::byte>(reinterpret_cast<const std::byte*>(base_gp_file.data()), base_gp_file.size()), base_gp))
        return fail("the base level's gameplay file cannot be decompressed");
    // another level's sky
    std::vector<uint8_t> sky;
    if (lt.sky_level >= 0 && lt.sky_level != m.level_base) {
        std::vector<uint8_t> w, c;
        int32_t SH[48];
        const fs::path sp = fs::path("assets/disc") / fmt("levels/level%02d", lt.sky_level) / "data.wad";
        if (!read_file(sp, w) || !data_core(w, c, SH, err)) return fail(sp.generic_string() + ": its sky cannot be read (" + err + ")");
        if (SH[4] <= 0 || SH[5] <= SH[4] || static_cast<size_t>(SH[5]) > c.size()) return fail(fmt("level %d has no sky", lt.sky_level));
        sky.assign(c.begin() + SH[4], c.begin() + SH[5]);
        log.say(fmt("sky: level %d's (%zu bytes)", lt.sky_level, sky.size()));
    }
    std::vector<uint8_t> data, gameplay;
    if (!build_data(base_wad, coll, sky.empty() ? nullptr : &sky, data, log, err)) return fail(err);
    if (!build_gameplay(base_gp, lt, gameplay, log, err)) return fail(err);
    // the host meshes
    mrpw::HostMeshFile hm;
    const bool has_meshes = fs::exists(ldir / lt.meshes);
    std::vector<ObjMaterial> mesh_mats;  // (the meshes' materials: a rail's model may name one)
    if (has_meshes) {
        Obj mo;
        if (!read_obj(ldir / lt.meshes, lt, true, mo, err)) return fail(err);
        if (!build_host_meshes(mo, hm, log, err)) return fail(err);
        mesh_mats = mo.materials;
    } else {
        log.say("host meshes: none (no " + lt.meshes + ")");
    }
    if (!rails_meshes(lt.rails, mesh_mats, hm, log, err)) return fail(err);  // (stage 7) main's rails' models
    // (format 3) the sections: their collisions, skies, meshes (after the level's own, before its classes' models) and objects
    const size_t main_instances = hm.instances.size();
    std::vector<SectionOut> souts;
    if (!lt.sections.empty()) {
        for (const LevelTxt::Moby& mb : lt.mobys) lt.uid_first = std::max(lt.uid_first, mb.uid);
        if (!compose_sections(ldir, lt, m.level_base, base_gp, hm, mesh_mats, souts, log, err)) return fail(err);
    }
    // the level's own classes: their models, more host meshes (no placement: the objects are placed by the game)
    std::vector<ClassModel> models;
    struct ClassColl {
        std::vector<uint8_t> blob;
        float centre[3] = {}, radius = 0;  // (its bounding sphere, model units times the class's scale)
    };
    std::vector<ClassColl> colls(lt.classes.size());
    for (LevelTxt::Class& c : lt.classes) {
        LevelTxt unmoved = lt;  // (a model is made round its own origin: level.txt's up and scale, not its offset)
        for (float& o : unmoved.offset) o = 0;
        Obj mo;
        if (!read_obj(ldir / c.model, unmoved, true, mo, err)) return fail(err);
        ClassModel cm;
        if (!build_host_meshes(mo, hm, log, err, c.name.c_str(), &cm)) return fail(fmt("level.txt:%d: ", c.line) + err);
        models.push_back(cm);
        if (!c.collision.empty()) {  // (stage 7) a collision mesh of its own: the carrier's moby collision
            Obj cc;
            if (!read_obj(ldir / c.collision, unmoved, false, cc, err)) return fail(fmt("level.txt:%d: ", c.line) + err);
            float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f}, maxabs = 0;
            for (const ObjTri& t : cc.tris)
                for (const ObjVert& v : t.v)
                    for (int l = 0; l < 3; ++l) {
                        lo[l] = std::min(lo[l], v.p[l]);
                        hi[l] = std::max(hi[l], v.p[l]);
                        maxabs = std::max(maxabs, std::fabs(v.p[l]));
                    }
            const float need = maxabs * c.scale * 0.77f;  // (the carrier's scale size / 24: int16 vertices at 1/1024 of its units)
            float size = c.size > 0 ? c.size : std::max(cm.extent * c.scale, 0.1f);
            if (size < need) size = need;
            c.size = size;
            ClassColl& k = colls[static_cast<size_t>(&c - lt.classes.data())];
            if (!class_collision(cc, c.scale * 24576.0f / size, k.blob, err)) return fail(fmt("level.txt:%d: %s: ", c.line, c.collision.c_str()) + err);
            for (int l = 0; l < 3; ++l) k.centre[l] = (lo[l] + hi[l]) / 2 * c.scale;
            for (const ObjTri& t : cc.tris)
                for (const ObjVert& v : t.v) {
                    const std::array<float, 3> a{v.p[0] * c.scale, v.p[1] * c.scale, v.p[2] * c.scale}, b{k.centre[0], k.centre[1], k.centre[2]};
                    k.radius = std::max(k.radius, dist3(a, b));
                }
            log.say(fmt("class \"%s\": its collision mesh %s, %zu triangles, %zu bytes; its size %.2f", c.name.c_str(), c.collision.c_str(), cc.tris.size(),
                        k.blob.size(), size));
        }
        log.say(fmt("class \"%s\": %s, its hit box %.2f units%s%s", c.name.c_str(), c.model.c_str(), c.size > 0 ? c.size : std::max(cm.extent, 0.1f),
                    c.size > 0 ? "" : " (the model's extent)", c.ghost ? ", ghost (no collision)" : ""));
    }
    // (a world section's rails' models: more host meshes after all the others - the classes' models included -, so that they shift
    // no index lights.txt's `material tie <n>` lines give; the renderer adds them to the world's level when the section is shown)
    for (size_t k = 0; k < lt.sections.size(); ++k) {
        const LevelTxt::Section& S = lt.sections[k];
        if (S.world < 0) continue;
        souts[k].first = hm.instances.size();
        const size_t classes0 = hm.classes.size();
        if (!rails_meshes(S.rails, mesh_mats, hm, log, err)) return fail(fmt("level.txt:%d: section %s: ", S.line, S.name.c_str()) + err);
        souts[k].count = hm.instances.size() - souts[k].first;
        if (souts[k].count != 0)
            log.say(fmt("section %s: its rails' %zu models drawn in level %d's world, host meshes %zu..%zu (after the classes': no other index moves)", S.name.c_str(),
                        souts[k].count, S.world, classes0, hm.classes.size() - 1));
    }
    for (const LevelTxt::Object& o : lt.objects)
        log.say(fmt("object \"%s\" at (%g %g %g)%s%s%s", o.cls.c_str(), o.at[0], o.at[1], o.at[2], o.section > 0 ? (" in section " + lt.sections[static_cast<size_t>(o.section - 1)].name).c_str() : "",
                    o.params.empty() ? "" : " ", o.params.c_str()));
    for (const LevelTxt::Trigger& g : lt.triggers)
        log.say(fmt("trigger \"%s\": (%g %g %g) .. (%g %g %g)", g.name.c_str(), g.lo[0], g.lo[1], g.lo[2], g.hi[0], g.hi[1], g.hi[2]));
    // the level's own sounds: one bank
    std::vector<BankSound> bank_sounds;
    for (const LevelTxt::Sound& d : lt.sounds) {
        std::vector<uint8_t> f;
        if (!read_file(ldir / d.file, f)) return fail(fmt("level.txt:%d: %s cannot be read", d.line, d.file.c_str()));
        BankSound b;
        b.volume = d.volume;
        b.pitch = d.pitch;
        std::string why;
        if (f.size() >= 4 && std::memcmp(f.data(), "VAGp", 4) == 0) {
            if (!read_vag(f, d.loop, b.adpcm, b.rate, why)) return fail(fmt("level.txt:%d: %s: ", d.line, d.file.c_str()) + why);
        } else {
            Pcm pcm;
            if (f.size() >= 4 && std::memcmp(f.data(), "OggS", 4) == 0) {  // an Ogg Vorbis file (level_music.h): mixed to mono
                std::vector<std::vector<int16_t>> ch;
                if (!ogg_decode(f, ch, pcm.rate, why)) return fail(fmt("level.txt:%d: %s: ", d.line, d.file.c_str()) + why);
                pcm.s.resize(ch[0].size());
                for (size_t i = 0; i < pcm.s.size(); ++i) pcm.s[i] = static_cast<int16_t>(ch.size() == 2 ? (ch[0][i] + ch[1][i]) / 2 : ch[0][i]);
            } else if (!read_wav(f, pcm, why)) {
                return fail(fmt("level.txt:%d: %s: ", d.line, d.file.c_str()) + why + " (a WAV, an Ogg Vorbis file or a VAG)");
            }
            if (pcm.rate < 8000 || pcm.rate > 48000) return fail(fmt("level.txt:%d: %s: its rate %u Hz (8000 .. 48000)", d.line, d.file.c_str(), pcm.rate));
            b.rate = pcm.rate;
            b.adpcm = adpcm_encode(pcm.s, d.loop);
        }
        log.say(fmt("sound \"%s\": %s, %u Hz, %.2f s%s", d.name.c_str(), d.file.c_str(), b.rate, static_cast<double>(b.adpcm.size()) / 16.0 * 28.0 / b.rate,
                    d.loop ? ", looping" : ""));
        bank_sounds.push_back(std::move(b));
    }
    // written: into a fresh folder, build.json last (a build without it is not used)
    std::error_code ec;
    if (fs::is_directory(out_dir, ec))  // (its plugin's build, mod_plugins.cpp, kept)
        for (const auto& e : fs::directory_iterator(out_dir, ec))
            if (e.path().filename() != "plugin" && e.path().filename() != "plugin-build") fs::remove_all(e.path(), ec);
    fs::create_directories(out_dir, ec);
    if (!write_file(out_dir / "data.wad", data) || !write_file(out_dir / "gameplay.bin", gameplay)) return fail("cannot write into " + out_dir.generic_string());
    for (size_t k = 0; k < souts.size(); ++k) {  // (format 3) each section's files in a folder of its own
        const fs::path sd = out_dir / ("section_" + lt.sections[k].name);
        fs::create_directories(sd, ec);
        const SectionOut& o = souts[k];
        if ((!o.coll.empty() && !write_file(sd / "collision.bin", o.coll)) || (!o.sky.empty() && !write_file(sd / "sky.bin", o.sky)) ||
            (!o.gameplay.empty() && !write_file(sd / "gameplay.bin", o.gameplay)))
            return fail("cannot write into " + sd.generic_string());
    }
    // the music: the build's choice (build.json's "music"), the mod's own VAG files copied into the build
    std::string music_json;
    if (lt.music_none) {
        music_json = "\"none\"";
        log.say("music: none");
    } else if (lt.music_level >= 0) {
        music_json = std::to_string(lt.music_level);
        log.say(fmt("music: level %d's", lt.music_level));
    } else if (!lt.music_loop.empty()) {
        // each a VAG (mono or stereo), a WAV or an Ogg Vorbis file (encoded), src/setup/level_music.h; an intro's and its
        // loop's rate the same (the stream keeps its first part's pitch: a WAV or an Ogg encoded at the other's rate, the
        // loop's if neither is a VAG), one stereo: both made stereo
        std::vector<std::string> names;
        std::vector<std::pair<std::string, std::string>> files;
        if (!lt.music_intro.empty()) files.push_back({lt.music_intro, "music_intro.vag"});
        files.push_back({lt.music_loop, "music_loop.vag"});
        std::vector<std::vector<uint8_t>> inputs(files.size());
        std::vector<MusicVag> made(files.size());
        auto make = [&](size_t i, int channels, uint32_t rate) {
            std::string why;
            if (music_vag_make(inputs[i], files[i].second == "music_loop.vag", channels, rate, made[i], why)) return true;
            fail(files[i].first + ": " + why + ": see docs/mods.md, \"Level mods\", Music");
            return false;
        };
        for (size_t i = 0; i < files.size(); ++i) {
            if (!read_file(ldir / files[i].first, inputs[i])) return fail(files[i].first + " cannot be read");
            if (!make(i, 0, 0)) return false;
        }
        if (files.size() == 2 && made[0].rate != made[1].rate) {
            const size_t redo = made[0].encoded ? 0 : made[1].encoded ? 1 : 2;  // (the intro first: the loop's rate kept)
            if (redo == 2)
                return fail(fmt("music: %s is %u Hz, %s %u Hz: an intro and its loop need the same rate (or give a WAV or an .ogg)", files[0].first.c_str(), made[0].rate,
                                files[1].first.c_str(), made[1].rate));
            if (!make(redo, made[redo].channels, made[1 - redo].rate)) return false;
        }
        if (std::any_of(made.begin(), made.end(), [](const MusicVag& v) { return v.channels == 2; }))
            for (size_t i = 0; i < files.size(); ++i)
                if (made[i].channels != 2 && !make(i, 2, made[i].rate)) return false;
        for (size_t i = 0; i < files.size(); ++i) {
            const MusicVag& m = made[i];
            std::vector<uint8_t> v = m.file;
            v.resize(align_up(v.size(), 2048), 0);
            if (!write_file(out_dir / files[i].second, v)) return fail("cannot write into " + out_dir.generic_string());
            names.push_back("\"" + files[i].second + "\"");
            log.say(fmt("music: %s, %u Hz, %s, %.1f s, as %s (%s)", files[i].first.c_str(), m.rate, m.channels == 2 ? "stereo" : "mono",
                        static_cast<double>(m.samples) / m.rate, files[i].second.c_str(), m.how.c_str()));
        }
        music_json = "[" + names[0] + (names.size() > 1 ? ", " + names[1] : std::string()) + "]";
    }
    if (!hm.classes.empty() && !mrpw::write_host_meshes((out_dir / "host_meshes.mrpc").string(), hm, err)) return fail(err);
    if (!bank_sounds.empty() && !write_file(out_dir / "sounds.bnk", sound_bank(bank_sounds))) return fail("cannot write into " + out_dir.generic_string());
    if (!compose_imports(lt, m.level_base, out_dir, log, err)) return fail(err);
    // (format 3) the classes of other levels placed: their level and class
    {
        auto resolve = [&](LevelTxt::Moby& mb) {
            int l = -1;
            uint32_t c = 0;
            if (!pool_name(mb.import, &l, &c)) {
                err = fmt("level.txt:%d: moby %s: no such class (a name of data/pool/names.txt, or <level>:<class>)", mb.line, mb.import.c_str());
                return false;
            }
            mb.ilevel = l;
            mb.icls = c;
            return true;
        };
        for (LevelTxt::Moby& mb : main_imports)
            if (!resolve(mb)) return fail(err);
        for (LevelTxt::Section& S : lt.sections)
            for (LevelTxt::Moby& mb : S.mobys)
                if (!mb.import.empty() && !resolve(mb)) return fail(err);
    }
    {
        std::vector<LevelTxt::Moby> all = lt.mobys;
        all.insert(all.end(), main_imports.begin(), main_imports.end());
        for (const LevelTxt::Section& S : lt.sections) all.insert(all.end(), S.mobys.begin(), S.mobys.end());
        check_profiles(lt, all, m.level_base, log);
    }
    // (format 3) sections.txt: the sections, their entries, checkpoints, portals, settings, meshes and placed classes of other
    // levels (src/game/enhance/sections.cpp reads it)
    if (m.level_format >= 3 && (!lt.sections.empty() || !lt.portals.empty() || !lt.entries.empty() || !main_imports.empty() || lt.start_line != 0 ||
                                 lt.hero_ctx.line != 0)) {
        std::ostringstream o;
        o << "sections\t1\n";
        auto index_of = [&](const std::string& s) {
            for (size_t k = 0; k < lt.sections.size(); ++k)
                if (lt.sections[k].name == s) return static_cast<int>(k + 1);
            return 0;
        };
        auto put_common = [&](int k, const std::vector<LevelTxt::Entry>& es, const std::vector<LevelTxt::Portal>& ps, const std::vector<LevelTxt::Moby>& mbs) {
            for (const LevelTxt::Entry& e : es) o << fmt("entry\t%d\t%s\t%.6g\t%.6g\t%.6g\t%.6g\n", k, e.name.c_str(), e.at[0], e.at[1], e.at[2], e.at[3]);
            for (const LevelTxt::Portal& pt : ps)
                o << fmt("portal\t%d\t%s\t%.6g\t%.6g\t%.6g\t%.6g\t%.6g\t%.6g\t%d\t%s\t%d\n", k, pt.name.c_str(), pt.lo[0], pt.lo[1], pt.lo[2], pt.hi[0], pt.hi[1],
                         pt.hi[2], index_of(pt.to), pt.entry.c_str(), pt.fade);
            int j = 0;
            for (const LevelTxt::Moby& mb : mbs) {
                if (mb.import.empty()) {
                    for (const auto& [slot, value] : mb.binds) o << fmt("bind\t%d\t%d\t%d\t%s\t%s\n", k, mb.uid, mb.cls, slot.c_str(), value.c_str());
                    continue;
                }
                o << fmt("spawn\t%d\t%d\t%d\t%u\t%.6g\t%.6g\t%.6g\t%.6g\n", k, j, mb.ilevel, mb.icls, mb.at[0], mb.at[1], mb.at[2], mb.at[3]);
                for (const auto& [slot, value] : mb.binds) o << fmt("ibind\t%d\t%d\t%s\t%s\n", k, j, slot.c_str(), value.c_str());
                ++j;
            }
        };
        o << fmt("section\t0\tmain\t%s\t-\n", lt.leave.c_str());
        o << fmt("meshes\t0\t0\t%zu\n", main_instances);
        if (lt.hero_ctx.line != 0) o << fmt("hero\t0\t%d\t%s\n", lt.hero_ctx.level, lt.hero_ctx.mode.empty() ? "-" : lt.hero_ctx.mode.c_str());
        put_common(0, lt.entries, lt.portals, main_imports);
        for (size_t k = 0; k < lt.sections.size(); ++k) {
            const LevelTxt::Section& S = lt.sections[k];
            const SectionOut& so = souts[k];
            const int i = static_cast<int>(k + 1);
            o << fmt("section\t%d\t%s\t%s\t%s\n", i, S.name.c_str(), S.policy.c_str(), S.respawn.empty() ? "-" : S.respawn.c_str());
            if (S.fresh) o << fmt("fresh\t%d\n", i);
            o << fmt("files\t%d\t%d\t%d\t%d\n", i, so.coll.empty() ? 0 : 1, so.sky.empty() ? 0 : 1, so.gameplay.empty() ? 0 : 1);
            o << fmt("meshes\t%d\t%zu\t%zu\n", i, so.first, so.count);
            if (S.sky_level >= 0) o << fmt("sky\t%d\t%d\n", i, S.sky_level);
            o << fmt("music\t%d\t%d\n", i, S.music_level);
            if (S.has_fog) o << fmt("fog\t%d\t%d\t%d\t%d\n", i, S.fog[0], S.fog[1], S.fog[2]);
            if (S.has_background) o << fmt("background\t%d\t%d\t%d\t%d\n", i, S.background[0], S.background[1], S.background[2]);
            if (S.world < 0 || S.has_death) o << fmt("death\t%d\t%.6g\n", i, so.death);  // (a world's: its level's unless given)
            if (S.world >= 0) {  // (stage 8) its level's world, its objects (their uids there -> the session's)
                o << fmt("world\t%d\t%d\n", i, S.world);
                if (S.zones_all) o << fmt("zones\t%d\tall\n", i);
                for (const auto& [from, to] : S.wuids) o << fmt("wuid\t%d\t%d\t%d\n", i, from, to);
            }
            if (!S.lights.empty()) o << fmt("lights\t%d\t%s\n", i, S.lights.c_str());
            if (S.hero.line != 0) o << fmt("hero\t%d\t%d\t%s\n", i, S.hero.level, S.hero.mode.empty() ? "-" : S.hero.mode.c_str());
            for (const auto& [bl, bc] : S.boss_classes) o << fmt("boss\t%d\t%d\t%u\n", i, bl, bc);  // (stage 10)
            for (const LevelTxt::Checkpoint& c : S.checkpoints)
                o << fmt("checkpoint\t%d\t%s\t%.6g\t%.6g\t%.6g\t%.6g\n", i, c.name.c_str(), c.at[0], c.at[1], c.at[2], c.at[3]);
            put_common(i, S.entries, S.portals, S.mobys);
        }
        if (lt.start_line != 0) o << fmt("start\t%d\t%s\n", index_of(lt.start_section), lt.start_entry.c_str());
        const std::string t = o.str();
        if (!write_file(out_dir / "sections.txt", std::vector<uint8_t>(t.begin(), t.end()))) return fail("cannot write into " + out_dir.generic_string());
        size_t np = lt.portals.size(), nobj = 0;
        for (const LevelTxt::Section& S : lt.sections) np += S.portals.size();
        for (const SectionOut& so : souts) nobj += static_cast<size_t>(so.objects + so.imported);
        log.say(fmt("sections: main and %zu more, %zu portals, %zu objects in the sections (spawned when they are entered: in the room for spawned objects), "
                    "%zu classes of other levels placed in main%s",
                    lt.sections.size(), np, nobj, main_imports.size(),
                    lt.start_line != 0 ? fmt("; the session starts in %s at %s", lt.start_section.c_str(), lt.start_entry.c_str()).c_str() : ""));
    }
    // (stage 9) hero.txt: a format 1 / 2 level's `hero generic` (src/game/enhance/hero_modes.cpp reads it)
    {
        std::error_code ec;
        if (m.level_format < 3 && lt.hero_ctx.line != 0) {
            const std::string t = "hero\tgeneric\n";
            if (!write_file(out_dir / "hero.txt", std::vector<uint8_t>(t.begin(), t.end()))) return fail("cannot write into " + out_dir.generic_string());
        } else {
            fs::remove(out_dir / "hero.txt", ec);
        }
    }
    // (format 3) session.txt: the session's checkpoints and policy (src/game/enhance/session_runtime.cpp reads it)
    if (m.level_format >= 3) {
        std::ostringstream o;
        o << "session\t1\n";
        bool start = false;
        for (const LevelTxt::Checkpoint& c : lt.checkpoints) start = start || c.name == "start";
        if (!start) o << fmt("checkpoint\tstart\t%.6g\t%.6g\t%.6g\t%.6g\n", lt.hero[0], lt.hero[1], lt.hero[2], lt.hero[3]);  // (where Ratchet starts)
        for (const LevelTxt::Checkpoint& c : lt.checkpoints) o << fmt("checkpoint\t%s\t%.6g\t%.6g\t%.6g\t%.6g\n", c.name.c_str(), c.at[0], c.at[1], c.at[2], c.at[3]);
        o << "respawn\t" << (lt.respawn.empty() ? "reset" : lt.respawn) << "\n";
        for (size_t i = 0; i < lt.mobys.size(); ++i) {  // (the moby lines' slots: set at the arrival, by the objects' uids)
            const LevelTxt::Moby& mb = lt.mobys[i];
            for (const auto& [slot, value] : mb.binds) o << fmt("bind\t%d\t%d\t%s\t%s\n", mb.uid, mb.cls, slot.c_str(), value.c_str());
        }
        const std::string t = o.str();
        if (!write_file(out_dir / "session.txt", std::vector<uint8_t>(t.begin(), t.end()))) return fail("cannot write into " + out_dir.generic_string());
        log.say(fmt("session: %zu checkpoints%s, respawn %s", lt.checkpoints.size() + (start ? 0 : 1), start ? "" : " (\"start\": where Ratchet starts)",
                    lt.respawn.empty() ? "reset" : lt.respawn.c_str()));
    }
    // level.sdk: the level's own content for the game (src/game/enhance/mod_code.cpp reads it)
    if (!lt.classes.empty() || !lt.sounds.empty() || !lt.triggers.empty() || !lt.paths.empty() || lt.update_all || rails_all > 0 || !lt.items.empty()) {
        std::ostringstream o;
        o << "sdk\t1\n";
        if (lt.update_all) o << "update_all\t1\n";
        for (size_t i = 0; i < lt.classes.size(); ++i) {
            const LevelTxt::Class& c = lt.classes[i];
            const ClassModel& cm = models[i];
            o << fmt("class\t%s\t%d\t%.6g\t%.6g\t%d\t%zu", c.name.c_str(), cm.mesh, c.size > 0 ? c.size : std::max(cm.extent * c.scale, 0.1f), c.scale, c.ghost ? 1 : 0,
                     cm.palette.size());
            for (const auto& p : cm.palette) o << fmt("\t%d %d %d %d", p[0], p[1], p[2], p[3]);
            o << "\n";
        }
        for (const LevelTxt::Object& ob : lt.objects) {
            size_t ci = 0;
            while (lt.classes[ci].name != ob.cls) ++ci;
            if (ob.section == 0) o << fmt("object\t%zu\t%.6g\t%.6g\t%.6g\t%.6g\t", ci, ob.at[0], ob.at[1], ob.at[2], ob.at[3]) << ob.params << "\n";
            else o << fmt("sobject\t%d\t%zu\t%.6g\t%.6g\t%.6g\t%.6g\t", ob.section, ci, ob.at[0], ob.at[1], ob.at[2], ob.at[3]) << ob.params << "\n";  // (format 3)
        }
        for (size_t i = 0; i < lt.sounds.size(); ++i) {
            const LevelTxt::Sound& d = lt.sounds[i];
            o << fmt("sound\t%s\t%zu\t%d\t%.6g\t%.6g\t%d\n", d.name.c_str(), i, 0x400, d.near, d.far, d.loop ? 1 : 0);
        }
        for (const LevelTxt::Trigger& g : lt.triggers)
            o << fmt("trigger\t%s\t%.6g\t%.6g\t%.6g\t%.6g\t%.6g\t%.6g\n", g.name.c_str(), g.lo[0], g.lo[1], g.lo[2], g.hi[0], g.hi[1], g.hi[2]);
        for (const LevelTxt::Path& pa : lt.paths) {
            o << fmt("path\t%s\t%zu", pa.name.c_str(), pa.pts.size());
            for (const auto& v : pa.pts) o << fmt("\t%.6g\t%.6g\t%.6g", v[0], v[1], v[2]);
            o << "\n";
        }
        // (stage 7) the rails (their section, their index in its table, their points as the table has them), the items for the
        // visit, the classes' collision meshes (their files)
        auto rails_of = [&](int sec, const std::vector<LevelTxt::Rail>& rs) {
            for (size_t k = 0; k < rs.size(); ++k) {
                o << fmt("rail\t%s\t%d\t%zu\t%d\t%zu", rs[k].name.c_str(), sec, k, rs[k].loop ? 1 : 0, rs[k].pts.size());
                for (const auto& v : rs[k].pts) o << fmt("\t%.6g\t%.6g\t%.6g", v[0], v[1], v[2]);
                o << "\n";
            }
        };
        rails_of(0, lt.rails);
        for (size_t k = 0; k < lt.sections.size(); ++k) rails_of(static_cast<int>(k + 1), lt.sections[k].rails);
        if (!lt.items.empty()) {
            o << "items";
            for (int id : lt.items) o << fmt("\t%d", id);
            o << "\n";
        }
        for (size_t i = 0; i < lt.classes.size(); ++i) {
            if (colls[i].blob.empty()) continue;
            const std::string f = "class_" + lt.classes[i].name + ".col";
            if (!write_file(out_dir / f, colls[i].blob)) return fail("cannot write into " + out_dir.generic_string());
            o << fmt("collision\t%zu\t%s\t%.6g\t%.6g\t%.6g\t%.6g\n", i, f.c_str(), colls[i].centre[0], colls[i].centre[1], colls[i].centre[2], colls[i].radius);
        }
        const std::string t = o.str();
        if (!write_file(out_dir / "level.sdk", std::vector<uint8_t>(t.begin(), t.end()))) return fail("cannot write into " + out_dir.generic_string());
        log.say(fmt("level.sdk: %zu classes, %zu objects, %zu sounds, %zu triggers, %zu paths, %zu rails, %zu items for the visit", lt.classes.size(), lt.objects.size(),
                    lt.sounds.size(), lt.triggers.size(), lt.paths.size(), rails_all, lt.items.size()));
    }
    // the pictures (level_pictures.h): the loading screen's planet and name, the ship's map's picture
    {
        auto image = [&](const std::string& src, int w, int h, const char* what, const std::string& dst) {
            Rgba in;
            if (!picture_read(ldir / src, in, err)) return false;
            if (in.w != w || in.h != h) log.say(fmt("%s: %s is %d x %d: scaled to %d x %d", what, src.c_str(), in.w, in.h, w, h));
            const std::vector<uint8_t> img = picture_image(picture_scaled(in, w, h));
            if (!write_file(out_dir / dst, img)) {
                err = "cannot write into " + out_dir.generic_string();
                return false;
            }
            log.say(fmt("%s: %s as %s", what, src.c_str(), dst.c_str()));
            return true;
        };
        if (!lt.picture.empty() && !image(lt.picture, kPictureW, kPictureH, "picture", "picture.img")) return fail(err);
        for (const auto& [lang, f] : lt.banners)
            if (!image(f, kBannerW, kBannerH, "banner", lang.empty() ? "banner.img" : "banner." + lang + ".img")) return fail(err);
        // a name with no banner of the mod's: its banner made from it (the default's, and each language's whose name differs)
        if (lt.names.count("") == 0) lt.names[""] = m.name;  // (no name line: the mod's name, as the ship's list has it)
        for (const auto& [lang, name] : lt.names) {
            if (lt.banners.count(lang) != 0 || (!lang.empty() && (lt.banners.count("") != 0 || name == lt.names.at("")))) continue;
            std::string why;
            const std::vector<uint8_t> img = picture_image(banner_from_text(name, why));
            if (!why.empty()) log.say("banner: " + why);
            const std::string dst = lang.empty() ? "banner.img" : "banner." + lang + ".img";
            if (!write_file(out_dir / dst, img)) return fail("cannot write into " + out_dir.generic_string());
            log.say(fmt("banner: made from the name \"%s\" as %s", name.c_str(), dst.c_str()));
        }
        Rgba map;
        if (!lt.map_picture.empty()) {
            if (!picture_read(ldir / lt.map_picture, map, err)) return fail(err);
            map = map_from_picture(map);
            log.say(fmt("map picture: %s (%d x %d shown of %d x %d) as map.img", lt.map_picture.c_str(), kMapShownW, kMapShownH, kMapW, kMapH));
        } else if (!lt.picture.empty()) {
            Rgba planet;
            if (!picture_read(ldir / lt.picture, planet, err)) return fail(err);
            map = map_from_planet(planet);
            log.say("map picture: made from the picture as map.img");
        }
        if (map.w != 0 && !write_file(out_dir / "map.img", picture_image(map))) return fail("cannot write into " + out_dir.generic_string());
    }
    // the MRP world of the composed data file, the host meshes' classes and textures added
    const fs::path wdir = out_dir / "mrp" / "world" / ("level" + std::to_string(m.level_base));
    if (!export_mrp_world_wad(std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.data()), data.size()), m.level_base, wdir.string()))
        return fail("the MRP world export of the composed level failed");
    if (!hm.classes.empty()) {
        mrpw::HostMeshes back;
        if (!mrpw::read_host_meshes((out_dir / "host_meshes.mrpc").string(), back, err)) return fail(err);
        mrpw::World w;
        if (!mrpw::read_world((wdir / "world.mrpw").string(), w)) return fail("the composed world cannot be read back");
        if (!w.ties.empty()) return fail("the composed world has tie classes of its own");
        w.ties = std::move(back.classes);
        if (!mrpw::write_world((wdir / "world.mrpw").string(), w) || !mrp::write_textures((wdir / "textures_tie.mrpt").string(), back.textures))
            return fail("cannot write the host meshes into the MRP world");
    }
    log.say(fmt("MRP world: %s", wdir.generic_string().c_str()));
    {
        std::ofstream lf(out_dir / "compose.log");
        lf << log.text;
    }
    // the pools: per class, its objects' uids (mod_code.cpp's base_spawn)
    std::string pools_json;
    for (const LevelTxt::Pool& pl : lt.pools) {
        std::string uids;
        int pv = 0;
        for (const LevelTxt::Moby& mb : lt.mobys)
            if (mb.pool && mb.cls == pl.cls) {
                uids += (uids.empty() ? "" : ", ") + std::to_string(mb.uid);
                pv = mb.pvars;
            }
        pools_json += (pools_json.empty() ? ",\n  \"pools\": [" : ", ") + fmt("{\"class\": %d, \"pvars\": %d, \"uids\": [%s]}", pl.cls, pv, uids.c_str());
        log.say(fmt("spawn pool: class %d, %d objects (uids %s), %d pvar bytes each", pl.cls, pl.count, uids.c_str(), pv));
    }
    if (!pools_json.empty()) pools_json += "]";
    std::ofstream bj(out_dir / "build.json");
    bj << "{\n  \"composer\": " << kLevelComposerVersion << ",\n  \"format\": " << m.level_format << ",\n  \"id\": \"" << m.level_id << "\",\n  \"base\": " << m.level_base
       << (music_json.empty() ? std::string() : ",\n  \"music\": " + music_json)
       << (lt.sky_level >= 0 && lt.sky_level != m.level_base ? fmt(",\n  \"sky\": %d", lt.sky_level) : std::string()) << ",\n  \"objects\": " << placed_objects << pools_json << ",\n  \"inputs\": \"" << stamp_hex(mods_level_inputs_stamp(m.dir)) << "\"\n}\n";
    if (!bj) return fail("cannot write build.json");
    std::cout << "[compose] " << folder << ": built into " << out_dir.generic_string() << "\n";
    return true;
}

// orig: (host) see level_compose.h
bool compose_level_mods(bool force) {
    std::error_code ec;
    const fs::path root = mods_dir();
    if (!fs::is_directory(root, ec)) return true;
    std::vector<std::string> folders;
    for (const auto& e : fs::directory_iterator(root, ec)) {
        if (!e.is_directory(ec)) continue;
        const std::string f = e.path().filename().string();
        if (f.empty() || f[0] == '.' || f[0] == '_') continue;
        ModInfo m;
        m.folder = f;
        m.dir = e.path();
        if (mods_manifest_read(m) && m.level) folders.push_back(f);
    }
    std::sort(folders.begin(), folders.end());
    bool ok = true;
    for (const std::string& f : folders) ok = compose_level_mod(f, force) && ok;
    return ok;
}

}  // namespace rac1::setup
