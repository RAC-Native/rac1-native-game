// See custom_arena.h: Maktar's arena controller, after RAC2's class 2905 (its update 0x3f9c10, the menu 0x3fd8b8, the waves'
// hand-out 0x3faaa4.., the prize 0x3fcdf8, the names 0x3fe1b0), written for RAC1's engine with rac2_compat.h's services.
#include <host/custom_arena.h>
#include <host/custom_arena_hud.h>
#include <host/custom_planet.h>
#include <host/rac2_compat.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/disc.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace rac1 {

namespace {

using namespace rac2;

constexpr float kCentreX = 500.0f, kCentreY = 600.0f, kFloorZ = 500.0f;  // (the arena's floor: its centre and height)
constexpr float kEntryRadius = 4.0f;       // (the prompt: the hero this near the centre, on the floor)
constexpr float kDropRadius = 14.0f;       // (an enemy lands this far from the centre, on the raised floor, in front of its gate:
                                           // RAC2's arm lets go at -9.0 / -3.3 local; ours has no floor there, a step lower)
constexpr uint32_t kIntro = 150, kBetween = 120, kWonShown = 360, kLostShown = 240, kCooldown = 120;  // (game frames)
constexpr uint32_t kPerDrop = 3;           // (a gate holds three enemies: RAC2's arm pvars +0x10..+0x18)
constexpr uint32_t kMaxAlive = 24;         // (enemies on the floor at once: the pvars pool's share, rac2_compat.h)
constexpr uint32_t kLineGap = 300;         // (frames at least between two of the announcer's remarks)
constexpr uint32_t PC_ITEM = 0x201, PC_VERSION = 1;
constexpr int kBossColumn = 8, kGatesColumn = 9;

// RAC2's four regular enemies, in the order of the wave records' columns 0..3
constexpr int32_t kRac2Enemies[4] = {2506, 2850, 3131, 3134};

// The challenges' rules our table adds (RAC2's controller keeps them in its code), by their name's text id
struct Rule {
    uint32_t text;
    uint32_t seconds;      // a time limit (0 none)
    bool wrench_only;      // the Wrench Expert's: the wrench only, and no hit
    const char* missing;   // why it is left out for now (nullptr: playable)
};
constexpr Rule kRules[] = {
    {0x2f4d, 30, false, nullptr},
    {0x2f4f, 60, false, nullptr},
    {0x3242, 0, true, nullptr},
    {0x2f6e, 0, false, "needs RAC2's Heavy Lancer"},
    {0x3240, 0, false, "needs RAC2's Mini-Nuke"},
    {0x3241, 0, false, "needs Chainblade (later)"},
};

struct Wave {
    int16_t c[10] = {};
};
struct Challenge {
    int index = 0;
    uint32_t text = 0;
    int first = -1;      // the challenge to win before (guess: the record's third word)
    int32_t prize = 0;
    std::string name;
    std::vector<Wave> waves;
    Rule rule{};
    std::string missing;  // (empty: playable)
};
struct Standin {
    int32_t uid = -1;
    uint32_t pv_size = 0;
    uint32_t tmpl = 0;  // its moby (found at the level's start)
};
// RAC2's path camera (its type 19: the camera code 0x4548b8 / 0x454c80 of its level 2): it fades the screen out (0.05 a
// frame), cuts to its path's first point, fades back in and runs along the path at its speed (units a second), looking at
// a moby (on Maktar: the hero); released, it fades out again and the game's camera comes back as the screen fades in.
// Maktar's three circle the floor's centre: record 1 wide and high (radius 43, 26 up, 50 a second), 2 at the stands'
// height (27, 3 up, 30), 3 close to the floor (6, 0.3 up, 5). How RAC2 picks them is not traced (no call from its
// controller); ours: 1 for a challenge's start, 3 for a victory, 2 for a defeat.
struct PathCamera {
    float speed = 0.0f;           // (units a second)
    std::vector<Vec3> points;
};

struct Settings {
    uint32_t gen = 0xffffffffu;
    std::vector<Challenge> challenges;
    std::map<int32_t, Standin> standins;  // by RAC1 class
    std::map<int32_t, int32_t> stand_for; // RAC2 class -> RAC1 class
    std::vector<PathCamera> cameras;      // cameras.txt's, in its order (RAC2's records 1, 2, 3 on Maktar)
    std::vector<std::pair<Vec3, int32_t>> vents;  // planet.txt's "vent": RAC2's 2953 (place, damage)
    int32_t tile_damage = 1;              // planet.txt's "tile_damage" (RAC2's 2563 pvars +0x14)
    int32_t items[2] = {0, 0};            // planet.txt's "items": the crate classes of columns c6 and c7
} g_cfg;

struct Enemy {
    uint32_t m = 0, pvars = 0;
    int32_t cls = 0;
    uint32_t dropped = 0;  // the level frame it dropped
};
struct Gate {
    uint32_t m = 0;
    bool pending = false;  // asked to drop, not yet dropped
};

enum State { Idle, Menu, Intro, Fight, Between, Won, Lost };

struct Arena {
    uint32_t base = 0, tick = 0;  // (the moby list and the level frame last seen: either changing = the level loaded again)
    StateMachine sm;
    int chosen = 0;               // the menu's challenge
    int running = -1;             // the challenge running
    int wave = 0;
    std::vector<int32_t> queue;   // the wave's enemies still to drop (RAC1 classes)
    std::vector<Enemy> alive;
    std::vector<Gate> gates;
    int kills = 0;
    uint32_t started = 0;         // the level frame the first wave came (the time limits run from it)
    int32_t prize = 0;
    std::string reason;           // (lost: why, our words)
    std::string reason_key;       // (and RAC2's text for it, hud.txt's key: empty none)
    bool electrolyzer = false;    // (won: the first victory of challenge 0, RAC2's Electrolyzer line)
    int32_t health = 0;           // the nanotech last seen
    uint32_t last_line = 0;       // the level frame of the announcer's last remark
    std::vector<std::pair<std::string, uint32_t>> lines;  // the announcer's lines waiting for the dialog slot
    bool script_done = false;
    // P5: the hazards and the items
    std::vector<uint32_t> tiles, lifts;  // (in the order they set up: RAC2's groups' order)
    size_t tiles_on = 0, vents_on = 0;    // (how many the wave wants: its columns c4, c5)
    std::vector<int32_t> crates_queued;   // (the wave's crates still to come up: columns c6, c7)
    std::vector<Enemy> crates;            // (the crates up, ours to clear)
    std::vector<uint32_t> vent_phase;     // (each vent's frame in its cycle, Vent)
} g_a;

std::vector<uint8_t> g_wins;  // victories per challenge (the .pc's)

// The path camera running (PathCamera)
struct CameraRun {
    enum Phase { Off, FadingOut, Running, Leaving } phase = Off;
    int which = -1;      // (cameras.txt's index)
    int want = -1;       // (asked for: another path camera, or -1 the game's camera)
    size_t segment = 0;  // (the path's segment it is on, and how far along it)
    float along = 0.0f;
    float fade = 0.0f;   // (the screen's fade, 0x15f3fc: 0 clear .. 1 black)
    Vec3 pos;
} g_cam;
constexpr float kFadeStep = 0.05f;
const Settings& settings();
void camera_step();
constexpr uint32_t FADE = 0x0015f3fc;

// orig: (none) — the path camera `which` (cameras.txt's index; -1 none: the game's camera back), faded to as RAC2's does
void camera_want(int which) {
    if (which >= static_cast<int>(settings().cameras.size())) which = -1;
    g_cam.want = which;
}

// orig: (none) — RAC1_CUSTOM_TRACE
bool trace() {
    static const bool t = std::getenv("RAC1_CUSTOM_TRACE") != nullptr;
    return t;
}

// orig: (none) — challenges.txt and planet.txt's lines (read again when the planet's generation changes)
const Settings& settings() {
    if (g_cfg.gen == custom_level_generation()) return g_cfg;
    g_cfg = Settings{};
    g_cfg.gen = custom_level_generation();
    std::ifstream c(custom_level_dir() + "/challenges.txt");
    std::string line;
    while (c && std::getline(c, line)) {
        std::istringstream in(line);
        std::string key;
        if (!(in >> key)) continue;
        if (key == "challenge") {
            Challenge ch;
            std::string text;
            if (!(in >> ch.index >> text >> ch.first >> ch.prize)) continue;
            ch.text = static_cast<uint32_t>(std::strtoul(text.c_str(), nullptr, 0));
            std::getline(in >> std::ws, ch.name);
            for (const Rule& r : kRules)
                if (r.text == ch.text) ch.rule = r;
            g_cfg.challenges.push_back(ch);
        } else if (key == "wave" && !g_cfg.challenges.empty()) {
            Wave w;
            for (int16_t& v : w.c) in >> v;
            g_cfg.challenges.back().waves.push_back(w);
        }
    }
    for (Challenge& ch : g_cfg.challenges) {
        if (ch.rule.missing != nullptr) ch.missing = ch.rule.missing;
        for (const Wave& w : ch.waves)
            if (w.c[kBossColumn] != 0 && ch.missing.empty()) ch.missing = "has a boss (later)";
    }
    std::ifstream p(custom_level_dir() + "/planet.txt");
    while (p && std::getline(p, line)) {
        std::istringstream in(line);
        std::string key;
        in >> key;
        int32_t a = 0, b = 0;
        uint32_t size = 0;
        if (key == "standin" && in >> a >> b >> size) g_cfg.standins[a] = Standin{b, size, 0};
        else if (key == "stand_for" && in >> a >> b) g_cfg.stand_for[a] = b;
        else if (key == "tile_damage" && in >> a) g_cfg.tile_damage = a;
        else if (key == "items" && in >> a >> b) {
            g_cfg.items[0] = a;
            g_cfg.items[1] = b;
        } else if (key == "vent") {
            Vec3 v;
            if (in >> v.x >> v.y >> v.z >> a) g_cfg.vents.emplace_back(v, a);
        }
    }
    std::ifstream cams(custom_level_dir() + "/cameras.txt");
    while (cams && std::getline(cams, line)) {
        std::istringstream in(line);
        std::string key;
        int record = 0;
        PathCamera pc;
        if (!(in >> key) || key != "camera" || !(in >> record >> pc.speed)) continue;
        Vec3 v;
        while (in >> v.x >> v.y >> v.z) pc.points.push_back(v);
        if (pc.points.size() >= 2 && pc.speed > 0.0f) g_cfg.cameras.push_back(pc);
    }
    if (g_wins.size() < g_cfg.challenges.size()) g_wins.resize(g_cfg.challenges.size(), 0);
    if (trace()) std::printf("[arena] %zu challenges, %zu stand-ins, %zu path cameras\n", g_cfg.challenges.size(), g_cfg.standins.size(), g_cfg.cameras.size());
    return g_cfg;
}

// orig: (none) — on the planet, in play, the level's mobys there
bool level_on() {
    return custom_level_active(0) && lw(0x0015ed84) == 0 && lwu(OV(0x0015ffd8)) != 0 && !settings().challenges.empty();
}

// orig: (none) — RAC2's prize (0x3fcdf8): the prize x 0.3 for each victory already, at least 500
int32_t prize_of(const Challenge& ch) {
    const uint32_t wins = static_cast<size_t>(ch.index) < g_wins.size() ? g_wins[static_cast<size_t>(ch.index)] : 0;
    return std::max<int32_t>(500, static_cast<int32_t>(static_cast<float>(ch.prize) * std::pow(0.3f, static_cast<float>(wins))));
}

// orig: (none) — open in the menu: its earlier challenge won (or that one not playable yet, so the chain goes on)
bool unlocked(const Challenge& ch) {
    const std::vector<Challenge>& all = settings().challenges;
    if (ch.first < 0 || static_cast<size_t>(ch.first) >= all.size()) return true;
    return g_wins[static_cast<size_t>(ch.first)] != 0 || !all[static_cast<size_t>(ch.first)].missing.empty();
}

// orig: (none) — an announcer's line asked for: said as soon as the dialog slot is free
void say(const char* event) {
    const uint32_t n = custom_planet_announce_count(event);
    if (n == 0) return;
    g_a.lines.emplace_back(event, static_cast<uint32_t>(rand_below(static_cast<int32_t>(n))));
    g_a.last_line = level_frame();
}

// orig: (none) — a remark (a kill, a hit): now and then, never on top of another
void remark(const char* event) {
    if (level_frame() - g_a.last_line < kLineGap || !g_a.lines.empty() || rand_below(3) != 0) return;
    say(event);
}

// orig: (none) — the lines waiting started, one at a time
void lines_step() {
    custom_planet_announce_step();
    if (g_a.lines.empty()) return;
    if (custom_planet_announce(g_a.lines.front().first.c_str(), g_a.lines.front().second) || level_frame() - g_a.last_line > 600)
        g_a.lines.erase(g_a.lines.begin());
}

// orig: (none) — the level (re)loaded: the arena from its start, the templates found
void reset(uint32_t base, uint32_t tick) {
    const std::vector<uint8_t> keep = g_wins;
    g_a = Arena{};  // (the level loaded again: its mobys, the crates' copies among them, are new)
    g_cam = CameraRun{};
    g_a.base = base;
    g_a.tick = tick;
    g_wins = keep;
    standin_reset();
    for (auto& [cls, s] : g_cfg.standins) {
        s.tmpl = moby_by_uid(s.uid);
        if (s.tmpl != 0 && lh(s.tmpl + moby::kClassId) != cls) s.tmpl = 0;
        if (trace()) std::printf("[arena] stand-in %d: template uid %d at %#x\n", cls, s.uid, s.tmpl);
    }
}

// orig: (none) — the wave's enemies queued (RAC2's controller hands them out in the order 3134, 2850, 3131, 2506)
void wave_queue(const Wave& w) {
    g_a.queue.clear();
    for (int col : {3, 1, 2, 0}) {
        const auto f = g_cfg.stand_for.find(kRac2Enemies[col]);
        if (f == g_cfg.stand_for.end()) continue;
        const auto s = g_cfg.standins.find(f->second);
        if (s == g_cfg.standins.end() || s->second.tmpl == 0) continue;
        for (int k = 0; k < w.c[col]; ++k) g_a.queue.push_back(f->second);
    }
    // (spread over the drops: the classes mixed as RAC2's gates mix them)
    for (size_t i = g_a.queue.size(); i > 1; --i) std::swap(g_a.queue[i - 1], g_a.queue[static_cast<size_t>(rand_below(static_cast<int32_t>(i)))]);
    for (Gate& g : g_a.gates) g.pending = false;
}

// ---- the hazards and items (P5) ----
// RAC2's 2953 (its update 0x401ed8, code only): turned on (0x4024b0), it waits 60 frames, warns twice (two 120-frame
// pulses, its warning drawn the first 60 of each), then burns 60 frames: a column 5 units up from it (the hero's nearest
// point on it, 0x320098) hurting what is within 1 of it (0x3201e0, its pvars' damage), flames rising (0x342b98) and its
// sound; then waits again. Turned off, it ends its cycle and stays idle. Ours: the same cycle and hurt, RAC1's particles,
// no sound yet (RAC2's sound 0x11 of its global bank).
namespace vent {
constexpr uint32_t kWait = 60, kPulse = 120, kWarn = 60, kBurn = 60, kCycle = kWait + 2 * kPulse + kBurn;
constexpr float kHeight = 5.0f, kRadius = 1.0f;

// orig: (none) — vent `k`'s frame: its phase stepped, its particles, its hurt
void step(size_t k) {
    const Vec3 at = g_cfg.vents[k].first;
    uint32_t& ph = g_a.vent_phase[k];
    if (ph == 0 && k >= g_a.vents_on) return;  // (off, idle)
    ph = (ph + 1) % kCycle;
    if (ph <= kWait) return;
    if (ph <= kWait + 2 * kPulse) {
        if ((ph - kWait - 1) % kPulse < kWarn && rand_below(2) == 0) {  // (the warning: glows flickering at its foot)
            const Vec4 p{at.x + (rand_unit() - 0.5f) * 0.8f, at.y + (rand_unit() - 0.5f) * 0.8f, at.z + 0.15f, 1.0f};
            spawn_particle_3c_v(p, Vec4{0.0f, 0.0f, 0.02f, 0.0f}, 0x803090f0u, 12, 0, 0, 0.5f);
        }
        return;
    }
    for (int n = 0; n < 3; ++n) {  // (the column of flames: RAC1's moving glows, rising)
        const Vec4 p{at.x + (rand_unit() - 0.5f) * 0.6f, at.y + (rand_unit() - 0.5f) * 0.6f, at.z + rand_unit() * kHeight * 0.4f, 1.0f};
        spawn_particle_3c_v(p, Vec4{0.0f, 0.0f, 0.1f + rand_unit() * 0.05f, 0.0f}, 0x802070f0u, 24, 0, 0, 1.2f + rand_unit() * 0.6f);
    }
    const Vec3 h = hero_pos();
    const float dx = h.x - at.x, dy = h.y - at.y, d = std::hypot(dx, dy);
    if (d < kRadius && h.z > at.z - 0.5f && h.z < at.z + kHeight) {
        const float ux = d > 1e-3f ? dx / d : 1.0f, uy = d > 1e-3f ? dy / d : 0.0f;
        hero_hit(0, Vec3{ux * 1.5f, uy * 1.5f, 1.0f}, static_cast<float>(g_cfg.vents[k].second));
        if (trace()) std::printf("[arena] frame %u: vent %zu hits the hero (nanotech %d)\n", level_frame(), k, hero_health());
    }
}
}  // namespace vent

// orig: (none) — the wave's hazards and crates (RAC2: 0x3fc518 the tiles, 0x3fc268 the vents, 0x3fd228 the crates, at a
// wave's start)
void hazards_set(const Wave& w) {
    g_a.tiles_on = static_cast<size_t>(std::max<int16_t>(0, w.c[4]));
    g_a.vents_on = std::min(g_cfg.vents.size(), static_cast<size_t>(std::max<int16_t>(0, w.c[5])));
    g_a.crates_queued.clear();
    for (int col : {6, 7}) {
        const int32_t cls = g_cfg.items[col - 6];
        const auto t = g_cfg.standins.find(cls);
        if (cls == 0 || t == g_cfg.standins.end() || t->second.tmpl == 0) continue;
        for (int k = 0; k < w.c[col]; ++k) g_a.crates_queued.push_back(cls);
    }
    for (size_t i = g_a.crates_queued.size(); i > 1; --i)
        std::swap(g_a.crates_queued[i - 1], g_a.crates_queued[static_cast<size_t>(rand_below(static_cast<int32_t>(i)))]);
    if (trace()) std::printf("[arena] frame %u: tiles %zu, vents %zu, crates %zu\n", level_frame(), g_a.tiles_on, g_a.vents_on, g_a.crates_queued.size());
}

// orig: (none) — no hazards; the crates still up cleared (a challenge over)
void hazards_off() {
    g_a.tiles_on = 0;
    g_a.vents_on = 0;
    g_a.crates_queued.clear();
    for (const Enemy& c : g_a.crates) {
        if (!moby_gone(c.m, c.cls, c.pvars)) moby_kill(c.m);
        standin_release(c.pvars);
    }
    g_a.crates.clear();
}

// orig: (none) — the frame's hazards: the vents, the crates broken (their blocks given back)
void hazards_step() {
    if (g_a.vent_phase.size() != g_cfg.vents.size()) g_a.vent_phase.assign(g_cfg.vents.size(), 0);
    for (size_t k = 0; k < g_cfg.vents.size(); ++k) vent::step(k);
    for (size_t i = 0; i < g_a.crates.size();) {
        const Enemy& c = g_a.crates[i];
        if (!moby_gone(c.m, c.cls, c.pvars)) {
            ++i;
            continue;
        }
        standin_release(c.pvars);
        g_a.crates.erase(g_a.crates.begin() + static_cast<std::ptrdiff_t>(i));
    }
}

// orig: (none)
const Challenge& running() { return g_cfg.challenges[static_cast<size_t>(g_a.running)]; }

// orig: (none) — a challenge started (RAC2: the hero warped to the floor, the camera reset, music_set(4, 0xe4))
void challenge_start(int index) {
    g_a.running = index;
    g_a.wave = 0;
    g_a.kills = 0;
    g_a.alive.clear();
    g_a.queue.clear();
    g_a.health = hero_health();
    g_a.prize = prize_of(running());
    custom_planet_music_select(1);
    say(running().text == 0x2f4c ? "start_mega" : running().text == 0x2f51 ? "start_ultimate" : "start");
    camera_want(0);
    g_a.sm.set(Intro);
    std::printf("[arena] challenge %d \"%s\": %zu waves, prize %d\n", index, running().name.c_str(), running().waves.size(), g_a.prize);
}

// orig: (none) — the enemies left on the floor killed (a lost challenge)
void enemies_clear() {
    for (const Enemy& e : g_a.alive) {
        if (!moby_gone(e.m, e.cls, e.pvars)) moby_kill(e.m);
        standin_release(e.pvars);
    }
    g_a.alive.clear();
    g_a.queue.clear();
}

// orig: (none) — the challenge over
void challenge_end(bool won) {
    hazards_off();
    if (won) {
        g_wins[static_cast<size_t>(g_a.running)] = static_cast<uint8_t>(std::min(255, g_wins[static_cast<size_t>(g_a.running)] + 1));
        g_a.electrolyzer = running().index == 0 && g_wins[static_cast<size_t>(g_a.running)] == 1;
        bolts_give(g_a.prize);
        say("winner");
        say("prize");
        camera_want(2);
        g_a.sm.set(Won);
    } else {
        enemies_clear();
        camera_want(1);
        g_a.sm.set(Lost);
    }
    custom_planet_music_select(0);
    std::printf("[arena] frame %u: challenge %d %s (%d kills)%s%s\n", level_frame(), g_a.running, won ? "won" : "lost", g_a.kills,
                won ? "" : ": ", won ? "" : g_a.reason.c_str());
}

// orig: (none) — the enemies on the floor checked: the dead counted (and their blocks given back), the fallen killed
void enemies_step() {
    static const uint32_t autokill = [] {
        const char* e = std::getenv("RAC1_WAVES_AUTOKILL");
        return e != nullptr ? static_cast<uint32_t>(std::strtoul(e, nullptr, 10)) : 0u;
    }();
    const float death = lwc1(OV(0x0015f638));
    for (size_t i = 0; i < g_a.alive.size();) {
        Enemy& e = g_a.alive[i];
        bool gone = moby_gone(e.m, e.cls, e.pvars);
        if (!gone && (lwc1(e.m + moby::kPos + 8) < death || (autokill != 0 && level_frame() - e.dropped >= autokill))) {
            moby_kill(e.m);
            gone = true;
        }
        if (!gone) {
            if (trace() && (level_frame() % 120) == 0)
                std::printf("[arena] frame %u: enemy %d at %.1f %.1f %.1f state %d flags %04x\n", level_frame(), e.cls,
                            static_cast<double>(lwc1(e.m + moby::kPos)), static_cast<double>(lwc1(e.m + moby::kPos + 4)),
                            static_cast<double>(lwc1(e.m + moby::kPos + 8)), lbu(e.m + moby::kState), lhu(e.m + moby::kFlags));
            ++i;
            continue;
        }
        standin_release(e.pvars);
        ++g_a.kills;
        remark("kill");
        g_a.alive.erase(g_a.alive.begin() + static_cast<std::ptrdiff_t>(i));
    }
}

// orig: (none) — the gates open in this wave (its last column's low byte, at least one)
size_t gates_open() {
    const int open = running().waves[static_cast<size_t>(g_a.wave)].c[kGatesColumn] & 0xff;
    return std::min(g_a.gates.size(), static_cast<size_t>(std::max(open, 1)));
}

// orig: (none)
void fight_step() {
    const Challenge& ch = running();
    enemies_step();
    const int32_t h = hero_health();
    if (h < g_a.health) {
        if (ch.rule.wrench_only) {
            g_a.reason = "you took a hit";
            g_a.reason_key = "been_hit";
        }
        remark("hurt");
    }
    g_a.health = h;
    if (ch.rule.wrench_only && hero_item() != 8 && hero_item() != 0) g_a.reason = "the wrench only";
    if (ch.rule.seconds != 0 && level_frame() - g_a.started > ch.rule.seconds * 60u) {
        g_a.reason = "time's up";
        g_a.reason_key = "time_out";
    }
    if (!g_a.reason.empty()) {
        challenge_end(false);
        return;
    }
    bool pending = false;
    for (const Gate& g : g_a.gates) pending = pending || g.pending;
    if (g_a.queue.empty() && !pending && g_a.alive.empty()) {
        if (static_cast<size_t>(g_a.wave) + 1 < ch.waves.size()) {
            std::printf("[arena] frame %u: wave %d of %zu cleared, %d kills\n", level_frame(), g_a.wave + 1, ch.waves.size(), g_a.kills);
            g_a.sm.set(Between);
        } else {
            challenge_end(true);
        }
    }
}

// orig: (none) — the frame's step of the controller
void step() {
    const Settings& cfg = settings();
    StateMachine& sm = g_a.sm;
    const Vec3 hp = hero_pos();
    const bool at_entry = std::hypot(hp.x - kCentreX, hp.y - kCentreY) < kEntryRadius && std::fabs(hp.z - kFloorZ) < 6.0f;
    // (testing) RAC1_ARENA_SCRIPT=<frame>:<challenge>: started as from the menu
    static const std::pair<uint32_t, int> script = [] {
        const char* e = std::getenv("RAC1_ARENA_SCRIPT");
        if (e == nullptr) return std::pair<uint32_t, int>{0, -1};
        char* end = nullptr;
        const uint32_t f = static_cast<uint32_t>(std::strtoul(e, &end, 10));
        return std::pair<uint32_t, int>{f, *end == ':' ? std::atoi(end + 1) : -1};
    }();
    switch (sm.state) {
    case Idle:
        sm.first();
        if (script.second >= 0 && !g_a.script_done && level_frame() >= script.first &&
            static_cast<size_t>(script.second) < cfg.challenges.size()) {
            g_a.script_done = true;
            challenge_start(script.second);
            break;
        }
        if (at_entry && sm.frames() > kCooldown && (pad_pressed() & kTriangle) != 0) {
            pad_take(kTriangle);
            sm.set(Menu);
            say("menu");
        }
        if (at_entry) pad_take(kTriangle);  // (held: not the quick select)
        break;
    case Menu: {
        sm.first();
        const uint32_t p = pad_pressed();
        pad_block();
        const int n = static_cast<int>(cfg.challenges.size());
        if ((p & kLeft) != 0) g_a.chosen = (g_a.chosen + n - 1) % n;
        if ((p & kRight) != 0) g_a.chosen = (g_a.chosen + 1) % n;
        if ((p & kTriangle) != 0) sm.set(Idle);
        const Challenge& ch = cfg.challenges[static_cast<size_t>(g_a.chosen)];
        if ((p & kCross) != 0 && ch.missing.empty() && unlocked(ch)) challenge_start(g_a.chosen);
        break;
    }
    case Intro:
        sm.first();
        if (sm.frames() >= kIntro) {
            wave_queue(running().waves[0]);
            hazards_set(running().waves[0]);
            g_a.reason.clear();
            g_a.reason_key.clear();
            g_a.started = level_frame();
            camera_want(-1);
            sm.set(Fight);
            std::printf("[arena] frame %u: wave 1 of %zu, %zu enemies\n", level_frame(), running().waves.size(), g_a.queue.size());
        }
        break;
    case Fight:
        sm.first();
        fight_step();
        break;
    case Between: {
        sm.first();
        enemies_step();
        if (sm.frames() >= kBetween) {
            ++g_a.wave;
            wave_queue(running().waves[static_cast<size_t>(g_a.wave)]);
            hazards_set(running().waves[static_cast<size_t>(g_a.wave)]);
            sm.set(Fight);
            std::printf("[arena] frame %u: wave %d of %zu, %zu enemies\n", level_frame(), g_a.wave + 1, running().waves.size(), g_a.queue.size());
        }
        break;
    }
    case Won: {
        // (RAC2's victory screen: Triangle leaves, Cross goes on fighting: here the same challenge again, for its smaller prize)
        sm.first();
        const uint32_t p = sm.frames() > 60 ? pad_pressed() : 0;
        if ((p & kCross) != 0) {
            pad_take(kCross);
            challenge_start(g_a.running);
        } else if ((p & kTriangle) != 0 || sm.frames() >= kWonShown) {
            pad_take(kTriangle);
            camera_want(-1);
            sm.set(Idle);
        }
        break;
    }
    case Lost:
        sm.first();
        if (sm.frames() >= kLostShown) {
            camera_want(-1);
            sm.set(Idle);
        }
        break;
    default: sm.set(Idle);
    }
    hazards_step();
    lines_step();
    camera_step();
}

// orig: (none) — the path camera's frame (see PathCamera): its phase, the fade, its place along the path
void camera_step() {
    CameraRun& c = g_cam;
    const std::vector<PathCamera>& cams = settings().cameras;
    auto fade_to = [&](float target) {
        c.fade = target > c.fade ? std::min(target, c.fade + kFadeStep) : std::max(target, c.fade - kFadeStep);
        swc1(OV(FADE), c.fade);
        return c.fade == target;
    };
    switch (c.phase) {
    case CameraRun::Off:
        if (c.want < 0) return;
        c.phase = CameraRun::FadingOut;
        [[fallthrough]];
    case CameraRun::FadingOut:  // (the game's camera, or the last path camera, fading out)
        if (!fade_to(1.0f)) return;
        if (c.want < 0) {
            c.phase = CameraRun::Leaving;
            c.which = -1;
            return;
        }
        c.which = c.want;
        c.segment = 0;
        c.along = 0.0f;
        c.pos = cams[static_cast<size_t>(c.which)].points[0];
        c.phase = CameraRun::Running;
        if (trace()) std::printf("[arena] frame %u: path camera %d\n", level_frame(), c.which);
        return;
    case CameraRun::Running: {
        if (c.want != c.which) {
            c.phase = CameraRun::FadingOut;
            return;
        }
        fade_to(0.0f);
        const std::vector<Vec3>& pts = cams[static_cast<size_t>(c.which)].points;
        float left = cams[static_cast<size_t>(c.which)].speed / 60.0f;
        for (int guard = 0; guard < 64 && left > 0.0f; ++guard) {
            const Vec3& a = pts[c.segment];
            const Vec3& b = pts[(c.segment + 1) % pts.size()];
            const float len = std::max(1e-4f, std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y) + (b.z - a.z) * (b.z - a.z)));
            if (c.along + left < len) {
                c.along += left;
                left = 0.0f;
            } else {
                left -= len - c.along;
                c.along = 0.0f;
                c.segment = (c.segment + 1) % (pts.size() - 1);  // (the paths are closed: the last point is the first)
            }
        }
        const Vec3& a = pts[c.segment];
        const Vec3& b = pts[(c.segment + 1) % pts.size()];
        const float len = std::max(1e-4f, std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y) + (b.z - a.z) * (b.z - a.z)));
        const float t = c.along / len;
        c.pos = Vec3{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
        return;
    }
    case CameraRun::Leaving:  // (the game's camera back, the screen fading in)
        if (c.want >= 0) {
            c.phase = CameraRun::FadingOut;
            return;
        }
        if (fade_to(0.0f)) c.phase = CameraRun::Off;
        return;
    }
}

}  // namespace

// orig: (none) — see custom_arena.h
bool custom_arena_camera(uint32_t pos, uint32_t rows) {
    if (g_cam.phase != CameraRun::Running && !(g_cam.phase == CameraRun::FadingOut && g_cam.which >= 0)) return false;
    if (!level_on() || g_a.base == 0) return false;
    const Vec3 h = hero_pos();
    const Vec3& c = g_cam.pos;
    // (RAC1's camera angles: x 0, y the pitch down to the target, z the yaw towards it; the rows from them as its camera
    // types make theirs)
    const float dx = c.x - h.x, dy = c.y - h.y, dz = c.z - h.z;
    Vec4 ang{0.0f, std::atan2(dz, std::max(1e-4f, std::hypot(dx, dy))), std::atan2(dy, dx) + 3.14159265f, 0.0f};
    Mat4 m;
    euler_to_mat4_vu(m, ang);
    Mat4 out;
    mat3_copy(out, m);
    for (uint32_t r = 0; r < 3; ++r) {
        swc1(rows + 0x10 * r, out.r[r].x);
        swc1(rows + 0x10 * r + 4, out.r[r].y);
        swc1(rows + 0x10 * r + 8, out.r[r].z);
        swc1(rows + 0x10 * r + 0xc, 0.0f);
    }
    swc1(pos, c.x);
    swc1(pos + 4, c.y);
    swc1(pos + 8, c.z);
    swc1(pos + 0xc, 1.0f);
    return true;
}


// orig: (none) — see custom_arena.h
void custom_arena_frame() {
    if (!level_on()) {
        if (g_a.base != 0) g_a = Arena{};
        g_cam = CameraRun{};
        return;
    }
    if (!in_play()) return;
    const uint32_t base = lwu(OV(0x0015ffd8)), tick = level_frame();
    if (g_a.base != base || tick < g_a.tick) reset(base, tick);
    g_a.tick = tick;
    step();
}

// orig: (none) — see custom_arena.h: RAC2's arena HUD (custom_arena_hud.h) for the controller's state
void custom_arena_hud() {
    if (!level_on() || !in_play() || g_a.base == 0) return;
    const Settings& cfg = settings();
    switch (g_a.sm.state) {
    case Idle: {
        if (g_a.sm.frames() <= kCooldown) return;
        const Vec3 hp = hero_pos();
        if (std::hypot(hp.x - kCentreX, hp.y - kCentreY) >= kEntryRadius || std::fabs(hp.z - kFloorZ) >= 6.0f) return;
        arena_hud::prompt();
        return;
    }
    case Menu: {
        const Challenge& ch = cfg.challenges[static_cast<size_t>(g_a.chosen)];
        arena_hud::Menu m;
        m.name = ch.name;
        m.index = g_a.chosen;
        m.count = static_cast<int>(cfg.challenges.size());
        m.victories = g_wins[static_cast<size_t>(ch.index)];
        m.prize = prize_of(ch);
        if (!ch.missing.empty()) m.blocked = "Not yet: " + ch.missing;
        else if (!unlocked(ch)) m.blocked = "Win \"" + cfg.challenges[static_cast<size_t>(ch.first)].name + "\" first";
        arena_hud::menu(m);
        return;
    }
    case Intro:
        arena_hud::intro(running().name);
        return;
    case Fight:
    case Between: {
        const Challenge& ch = running();
        arena_hud::Fight f;
        f.wave = g_a.wave;
        f.waves = static_cast<int>(ch.waves.size());
        f.kills = g_a.kills;
        if (ch.rule.seconds != 0) {
            const int32_t s = static_cast<int32_t>(ch.rule.seconds) - static_cast<int32_t>((level_frame() - g_a.started) / 60);
            f.seconds_left = std::max(0, s);
        }
        arena_hud::fight(f);
        return;
    }
    case Won:
        arena_hud::won(g_a.prize, g_a.electrolyzer);
        return;
    case Lost:
        arena_hud::lost(g_a.reason_key, g_a.reason);
        return;
    default: return;
    }
}

// orig: (none) — see custom_arena.h
ArenaMood custom_arena_mood() {
    if (!level_on() || g_a.base == 0) return ArenaMood::Waiting;
    switch (g_a.sm.state) {
    case Fight: return ArenaMood::Fighting;
    case Between:
    case Won: return ArenaMood::Cheering;
    default: return ArenaMood::Waiting;
    }
}

// orig: (none) — see custom_arena.h
int custom_arena_gate_register(uint32_t m) {
    for (size_t k = 0; k < g_a.gates.size(); ++k)
        if (g_a.gates[k].m == m) return static_cast<int>(k);
    g_a.gates.push_back(Gate{m, false});
    return static_cast<int>(g_a.gates.size()) - 1;
}

// orig: (none) — see custom_arena.h: a gate open in the wave, enemies still to come and room for them on the floor
bool custom_arena_gate_wanted(int gate) {
    if (!level_on() || g_a.sm.state != Fight || gate < 0 || static_cast<size_t>(gate) >= g_a.gates.size()) return false;
    if (static_cast<size_t>(gate) >= gates_open()) return false;
    Gate& g = g_a.gates[static_cast<size_t>(gate)];
    if (g.pending) return false;
    size_t asked = 0;
    for (const Gate& o : g_a.gates) asked += o.pending ? kPerDrop : 0;
    if (g_a.queue.size() <= asked || g_a.alive.size() + asked >= kMaxAlive) return false;
    g.pending = true;
    return true;
}

// orig: (none) — see custom_arena.h: RAC2's arm lets go of the enemies its controller gave it (0x3d7900, at -9.0 / -3.3
// in its own axes); ours spawns them there, in from the gate, facing the centre
void custom_arena_gate_drop(int gate, uint32_t m) {
    if (gate < 0 || static_cast<size_t>(gate) >= g_a.gates.size()) return;
    Gate& g = g_a.gates[static_cast<size_t>(gate)];
    if (!g.pending) return;
    g.pending = false;
    if (g_a.sm.state != Fight) return;
    const Vec3 at = moby_pos(m);
    float dx = kCentreX - at.x, dy = kCentreY - at.y;
    const float d = std::max(0.001f, std::hypot(dx, dy));
    dx /= d;
    dy /= d;
    const float yaw = std::atan2(dy, dx);
    // (the spots below the gate: three abreast, further in when an enemy still stands on one)
    auto free_spot = [&](Vec3& p) {
        for (int row = 0; row < 4; ++row)
            for (int k : {0, -1, 1}) {
                const float side = static_cast<float>(k) * 2.5f, in = d - kDropRadius + 3.0f * static_cast<float>(row);
                p = Vec3{at.x + dx * in - dy * side, at.y + dy * in + dx * side, kFloorZ + 0.5f};
                bool taken = false;
                for (const Enemy& e : g_a.alive) {
                    const Vec3 q = moby_pos(e.m);
                    taken = taken || std::hypot(q.x - p.x, q.y - p.y) < 2.0f;
                }
                if (!taken) return true;
            }
        return false;
    };
    for (uint32_t k = 0; k < kPerDrop && !g_a.queue.empty() && g_a.alive.size() < kMaxAlive; ++k) {
        const int32_t cls = g_a.queue.back();
        const Standin& s = g_cfg.standins[cls];
        Vec3 p;
        if (!free_spot(p)) break;
        const uint32_t e = standin_spawn(s.tmpl, s.pv_size, p, yaw);
        if (e == 0) break;
        g_a.queue.pop_back();
        g_a.alive.push_back(Enemy{e, lwu(e + moby::kPvars), cls, level_frame()});
        if (trace()) std::printf("[arena] frame %u: gate %d drops a %d at %.1f %.1f\n", level_frame(), gate, cls, static_cast<double>(p.x), static_cast<double>(p.y));
    }
}

// orig: (none) — see custom_arena.h
int custom_arena_tile_register(uint32_t m) {
    for (size_t k = 0; k < g_a.tiles.size(); ++k)
        if (g_a.tiles[k] == m) return static_cast<int>(k);
    g_a.tiles.push_back(m);
    return static_cast<int>(g_a.tiles.size()) - 1;
}

// orig: (none) — see custom_arena.h
bool custom_arena_tile_on(int tile) {
    return level_on() && tile >= 0 && static_cast<size_t>(tile) < g_a.tiles_on;
}

// orig: (none) — see custom_arena.h: the tiles' hurt, said once for all (RAC2's 0x3d7140: within 1.5 of a tile, knocked
// along its turn, its damage)
int32_t custom_arena_tile_damage() { return g_cfg.tile_damage; }

// orig: (none) — see custom_arena.h
int custom_arena_lift_register(uint32_t m) {
    for (size_t k = 0; k < g_a.lifts.size(); ++k)
        if (g_a.lifts[k] == m) return static_cast<int>(k);
    g_a.lifts.push_back(m);
    return static_cast<int>(g_a.lifts.size()) - 1;
}

// orig: (none) — see custom_arena.h: RAC2's 0x3fd228 hands each crate to a lift ready for it at a 1-in-6 chance a frame
uint32_t custom_arena_lift_crate(int lift, float x, float y, float z) {
    constexpr size_t kMaxCrates = 12;  // (crates up at once: the pvars pool's share, rac2_compat.h)
    if (!level_on() || lift < 0 || g_a.sm.state != Fight || g_a.crates_queued.empty() || g_a.crates.size() >= kMaxCrates) return 0;
    if (rand_below(6) != 0) return 0;
    const int32_t cls = g_a.crates_queued.back();
    const Standin& s = g_cfg.standins[cls];
    const uint32_t c = standin_spawn(s.tmpl, s.pv_size, Vec3{x, y, z}, rand_unit() * 6.2831853f);
    if (c == 0) return 0;
    g_a.crates_queued.pop_back();
    g_a.crates.push_back(Enemy{c, lwu(c + moby::kPvars), cls, level_frame()});
    if (trace()) std::printf("[arena] frame %u: lift %d brings a crate %d up\n", level_frame(), lift, cls);
    return c;
}

// orig: (none) — see custom_arena.h: {version, count, a byte a challenge}
uint32_t custom_arena_pc_items(std::vector<uint8_t>& d) {
    bool any = false;
    for (uint8_t w : g_wins) any = any || w != 0;
    if (!any) return 0;
    const uint32_t size = 8 + static_cast<uint32_t>(g_wins.size());
    d.push_back(static_cast<uint8_t>(PC_ITEM));
    d.push_back(static_cast<uint8_t>(PC_ITEM >> 8));
    d.push_back(static_cast<uint8_t>(size));
    d.push_back(static_cast<uint8_t>(size >> 8));
    for (uint32_t v : {PC_VERSION, static_cast<uint32_t>(g_wins.size())})
        for (int i = 0; i < 4; ++i) d.push_back(static_cast<uint8_t>(v >> (8 * i)));
    d.insert(d.end(), g_wins.begin(), g_wins.end());
    return 1;
}

// orig: (none) — see custom_arena.h
bool custom_arena_pc_item(uint32_t id, const uint8_t* p, uint32_t size) {
    if (id != PC_ITEM) return false;
    if (size < 8 || (p[0] | p[1] << 8) != static_cast<int>(PC_VERSION)) return true;
    const uint32_t n = static_cast<uint32_t>(p[4] | p[5] << 8);
    if (8 + n > size) return true;
    g_wins.assign(p + 8, p + 8 + n);
    if (g_wins.size() < g_cfg.challenges.size()) g_wins.resize(g_cfg.challenges.size(), 0);
    return true;
}

// orig: (none) — see custom_arena.h
void custom_arena_new_game() { g_wins.assign(g_wins.size(), 0); }

}  // namespace rac1
