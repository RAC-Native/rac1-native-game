// See custom_arena.h: Maktar's arena controller, after RAC2's class 2905 (its update 0x3f9c10, the menu 0x3fd8b8, the waves'
// hand-out 0x3faaa4.., the prize 0x3fcdf8, the names 0x3fe1b0), written for RAC1's engine with rac2_compat.h's services.
#include <host/custom_arena.h>
#include <host/custom_planet.h>
#include <host/rac2_compat.h>

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
struct Settings {
    uint32_t gen = 0xffffffffu;
    std::vector<Challenge> challenges;
    std::map<int32_t, Standin> standins;  // by RAC1 class
    std::map<int32_t, int32_t> stand_for; // RAC2 class -> RAC1 class
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
    std::string reason;           // (lost: why)
    int32_t health = 0;           // the nanotech last seen
    uint32_t last_line = 0;       // the level frame of the announcer's last remark
    std::vector<std::pair<std::string, uint32_t>> lines;  // the announcer's lines waiting for the dialog slot
    bool script_done = false;
} g_a;

std::vector<uint8_t> g_wins;  // victories per challenge (the .pc's)

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
    }
    if (g_wins.size() < g_cfg.challenges.size()) g_wins.resize(g_cfg.challenges.size(), 0);
    if (trace()) std::printf("[arena] %zu challenges, %zu stand-ins\n", g_cfg.challenges.size(), g_cfg.standins.size());
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
    if (g_a.lines.empty()) return;
    if (custom_planet_announce(g_a.lines.front().first.c_str(), g_a.lines.front().second) || level_frame() - g_a.last_line > 600)
        g_a.lines.erase(g_a.lines.begin());
}

// orig: (none) — the level (re)loaded: the arena from its start, the templates found
void reset(uint32_t base, uint32_t tick) {
    const std::vector<uint8_t> keep = g_wins;
    g_a = Arena{};
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
    if (won) {
        g_wins[static_cast<size_t>(g_a.running)] = static_cast<uint8_t>(std::min(255, g_wins[static_cast<size_t>(g_a.running)] + 1));
        bolts_give(g_a.prize);
        say("winner");
        say("prize");
        g_a.sm.set(Won);
    } else {
        enemies_clear();
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
        if (ch.rule.wrench_only) g_a.reason = "you took a hit";
        remark("hurt");
    }
    g_a.health = h;
    if (ch.rule.wrench_only && hero_item() != 8 && hero_item() != 0) g_a.reason = "the wrench only";
    if (ch.rule.seconds != 0 && level_frame() - g_a.started > ch.rule.seconds * 60u) g_a.reason = "time's up";
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
            g_a.reason.clear();
            g_a.started = level_frame();
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
            sm.set(Fight);
            std::printf("[arena] frame %u: wave %d of %zu, %zu enemies\n", level_frame(), g_a.wave + 1, running().waves.size(), g_a.queue.size());
        }
        break;
    }
    case Won:
        sm.first();
        if (sm.frames() >= kWonShown) sm.set(Idle);
        break;
    case Lost:
        sm.first();
        if (sm.frames() >= kLostShown) sm.set(Idle);
        break;
    default: sm.set(Idle);
    }
    lines_step();
}

}  // namespace

// orig: (none) — see custom_arena.h
void custom_arena_frame() {
    if (!level_on()) {
        if (g_a.base != 0) g_a = Arena{};
        return;
    }
    if (!in_play()) return;
    const uint32_t base = lwu(OV(0x0015ffd8)), tick = level_frame();
    if (g_a.base != base || tick < g_a.tick) reset(base, tick);
    g_a.tick = tick;
    step();
}

// orig: (none) — see custom_arena.h
void custom_arena_hud() {
    if (!level_on() || !in_play() || g_a.base == 0) return;
    const Settings& cfg = settings();
    char buf[160];
    std::vector<Line> lines;
    switch (g_a.sm.state) {
    case Idle:
        if (g_a.sm.frames() <= kCooldown) return;
        {
            const Vec3 hp = hero_pos();
            if (std::hypot(hp.x - kCentreX, hp.y - kCentreY) >= kEntryRadius || std::fabs(hp.z - kFloorZ) >= 6.0f) return;
        }
        lines.push_back({"Press TRIANGLE: Enter the Battle Arena", 0xf0f0f0});
        hud_lines(0x150, lines);
        return;
    case Menu: {
        const Challenge& ch = cfg.challenges[static_cast<size_t>(g_a.chosen)];
        lines.push_back({"Select Your Challenge:", 0xf0d060});
        std::snprintf(buf, sizeof buf, "<  %d / %zu  >", g_a.chosen + 1, cfg.challenges.size());
        lines.push_back({buf, 0xc0c0c0});
        lines.push_back({ch.name, 0xf0f0f0});
        std::snprintf(buf, sizeof buf, "Victories: %d     Waves: %zu", g_wins[static_cast<size_t>(ch.index)], ch.waves.size());
        lines.push_back({buf, 0xc0c0c0});
        if (!ch.missing.empty()) {
            lines.push_back({"Not yet: " + ch.missing, 0xf07060});
        } else if (!unlocked(ch)) {
            lines.push_back({"Win \"" + cfg.challenges[static_cast<size_t>(ch.first)].name + "\" first", 0xf07060});
        } else {
            std::snprintf(buf, sizeof buf, "Prize: %d bolts", prize_of(ch));
            lines.push_back({buf, 0x60f060});
        }
        lines.push_back({"LEFT/RIGHT: choose   X: start   TRIANGLE: exit", 0xa0a0a0});
        hud_lines(0x60, lines);
        return;
    }
    case Intro:
        lines.push_back({running().name, 0xf0d060});
        lines.push_back({"Get ready!", 0xf0f0f0});
        hud_lines(0x60, lines);
        return;
    case Fight:
    case Between: {
        const Challenge& ch = running();
        size_t left = g_a.queue.size() + g_a.alive.size();
        if (g_a.sm.state == Between) std::snprintf(buf, sizeof buf, "Wave %d of %zu cleared!", g_a.wave + 1, ch.waves.size());
        else std::snprintf(buf, sizeof buf, "Wave %d of %zu   Enemies: %zu   Kills: %d", g_a.wave + 1, ch.waves.size(), left, g_a.kills);
        lines.push_back({buf, 0xf0f0f0});
        if (ch.rule.seconds != 0) {
            const int32_t s = static_cast<int32_t>(ch.rule.seconds) - static_cast<int32_t>((level_frame() - g_a.started) / 60);
            std::snprintf(buf, sizeof buf, "Time: %d", std::max(0, s));
            lines.push_back({buf, s <= 5 ? 0xf06060u : 0xf0f0f0u});
        }
        hud_lines(0x30, lines);
        return;
    }
    case Won:
        lines.push_back({"Congratulations!", 0xf0d060});
        std::snprintf(buf, sizeof buf, "Prize: %d bolts", g_a.prize);
        lines.push_back({buf, 0x60f060});
        hud_lines(0x60, lines);
        return;
    case Lost:
        lines.push_back({"Challenge failed: " + g_a.reason, 0xf07060});
        hud_lines(0x60, lines);
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
