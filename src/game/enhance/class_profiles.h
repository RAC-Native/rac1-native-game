// The open engine's class profiles (docs/open-engine.md, stage 4; DESIGN 6.3): our knowledge of a game class's level data, as
// data files in the repository (data/pool/profiles/L<level>_<class>.txt: ids and offsets only, nothing of the game's). A
// profile says which pvar words of the class hold level data (paths, areas, groups, other mobys, plain words), what a spawn
// gives them when the mod gives nothing (none, a value, a square round the spawn, points relative to it, the whole level),
// the options a plugin may set (base_option), and the host behaviours data cannot say (C++ registered here by name: a hunt
// mode, the jetpack troopers' flight). Profiles serve both the base level's own classes started by base_spawn (their paths
// in the base's objects, the session's view) and the classes of other levels a session spawns (their paths in their
// context's registry, leveldata.h). The SDK's ABI 5 fixes for Kalebo III's enemies (52, 541, 638, 1356; 1445's hunt, ABI 4)
// are these profiles' host behaviours, moved here unchanged.
#ifndef RAC1_GAME_ENHANCE_CLASS_PROFILES_H
#define RAC1_GAME_ENHANCE_CLASS_PROFILES_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rac1 {

struct Profile;

// An object a profile serves: a level mod's spawn pool object (mod_code.cpp's, its record and pvars as the level loaded
// them kept for base_spawn) or a session's spawn of an imported class (session.cpp's import_spawn)
struct ProfObj {
    int32_t cls = 0;
    uint32_t moby = 0, pvars = 0;
    std::vector<uint8_t> m, pv;  // (the spawn pool's: as loaded)
    bool out = false;            // started (until it is gone)
    uint16_t gen = 0;
    int level = -1;               // its class's level (its registry: leveldata.h; the base's own for the base level's classes)
    const Profile* prof = nullptr;
    // its paths (the profile's path defaults, in their order): the registry's index and memory, -1 none
    int16_t path[4] = {-1, -1, -1, -1};
    uint32_t pmem[4] = {0, 0, 0, 0};
    bool adapted = false, refused = false, woke = false, hunt = false;
    float anchor[3] = {0, 0, 0};   // the place its hunt last aimed from
    float start[4] = {0, 0, 0, 0}; // where it was started, its yaw
    std::vector<size_t> troops;    // (a dropship's: its pool's indices)
    // (a jetpack trooper's flight, kal_trooper) its flight position at the last frame, its climb and stuck frames, the home it
    // was last sent to (hunting: a flight there asked)
    float jprev[3] = {0, 0, 0};
    bool jprev_ok = false, jgo = false;
    int32_t jclimb = 0, jstuck = 0;
    float jhome[3] = {0, 0, 0};
};

// orig: (host) class `cls` of level `level`'s profile (read once from data/pool/profiles/), nullptr when it has none
const Profile* profile_find(int level, uint32_t cls);
// orig: (host) the profile's slot named `name`: its pvar offset and kind ("path", "area", "group", "moby", "word"); false
// when it has none
bool profile_slot(const Profile* p, const std::string& name, uint32_t* off, std::string* kind);
// orig: (host) the profile gives the slot at pvar offset `off` a default of its own (a spawn writes it: the placement's word
// is not used); false without a profile
bool profile_default_at(const Profile* p, uint32_t off);
// orig: (host) the profile's slots, "name:kind name:kind ..." (the SDK's object_slots)
std::string profile_slots_text(const Profile* p);
// orig: (host) the profile's text for the logs ("16:541 the Gadgetron robot")
std::string profile_name(const Profile* p);

// orig: (host) a started object still lives (its moby its class's, its pvars its own, in play)
bool prof_alive(const ProfObj& o);

// The base level's spawn pools (a level mod's spawn lines: mod_code.cpp's base_spawn)
// orig: (host) the pool taken at the level's load (`level` the base): each object of a profiled class given its paths (in
// the base's objects, from the path table's end; the whole level's area first, shared), logged
void profiles_pool_take(std::vector<ProfObj>& pool, int level);
// orig: (host) object `i` of the pool just started by base_spawn (its record and pvars as loaded, written back): its
// profile's defaults written into its pvars
void profiles_spawned(std::vector<ProfObj>& pool, size_t i);
// orig: (host) base_hunt on a moby: 1 done, 0 not (no profile with a hunt, not a started object of the pool), -1 not a
// profiled class (the caller's own rules)
int profiles_hunt(std::vector<ProfObj>& pool, uint32_t moby, int on);
// orig: (host) base_option on a pool object just started: what it did (1 / a count), 0 refused, -1 no profile option
int32_t profiles_option(std::vector<ProfObj>& pool, size_t i, int32_t option, int32_t value);
// orig: (host) after the frame's updates: each started object's host behaviours (the hunt, the troopers' flight)
void profiles_frame(std::vector<ProfObj>& pool);
// orig: (host) the level changes: the hunting mobys forgotten
void profiles_level();
// orig: (host) some pool object is served (profiles_frame has work)
bool profiles_any();
// base_spawn for a profile's host behaviours (a dropship's troopers): mod_code.cpp's, set at its start
extern uint32_t (*g_prof_base_spawn)(int32_t cls, const float pos[3], float yaw);

// A session's spawns of imported classes (session.cpp)
// orig: (host) moby `moby` of class `id` (level `level`'s class `cls`) just spawned by import_spawn at pos / yaw: its
// profile's defaults written (its paths in its context's registry); false when the class has no profile
bool profiles_session_spawn(int level, uint32_t cls, uint32_t moby, const float pos[3], float yaw);
// orig: (host) the session's spawns after the frame's updates (their host behaviours; those gone: their paths given back)
void profiles_session_frame();
// orig: (host) the session's spawns forgotten (a level setup: their registries go)
void profiles_session_clear();
// orig: (host) the session spawn of moby `moby` (its profile object), nullptr when it is none
ProfObj* profiles_session_obj(uint32_t moby);
// The base level's spawn pool (mod_code.cpp's base_spawn objects), for the binds below: set by mod_code.cpp
extern std::vector<ProfObj>* g_prof_pool;
// orig: (host) the profiled object of moby `moby`: a session spawn of an imported class, or a started object of the base
// level's spawn pool (its class's profile); nullptr when it is neither
ProfObj* profiles_obj(uint32_t moby);

// A slot bound by a mod (the plugin's object_bind, ABI 8): `value` a path's points (kind path / area), a moby (kind moby:
// its address, 0 none), a word
// orig: (host) moby `moby`'s slot `name` bound to the points `pts` (n of them; a path or an area): 1 done, 0 refused
int profiles_bind_points(uint32_t moby, const std::string& name, const float (*pts)[3], int n);
// orig: (host) moby `moby`'s slot `name` set to the word `value` (a group's index, a moby's index, a plain word): 1 done
int profiles_bind_word(uint32_t moby, const std::string& name, uint32_t value);

}  // namespace rac1

#endif
