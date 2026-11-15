// See class_profiles.h.
#include <game/enhance/class_profiles.h>

#include <game/enhance/leveldata.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/ee_memory.h>

#include "../../../sdk/rac1_mod.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>

namespace rac1 {

uint32_t (*g_prof_base_spawn)(int32_t cls, const float pos[3], float yaw) = nullptr;

// A profile, as its file says
struct Profile {
    int level = 0;
    uint32_t cls = 0;
    std::string what;
    uint32_t pvar_size = 0;
    struct Slot {
        uint32_t off;
        std::string kind, name;
    };
    std::vector<Slot> slots;
    // what a spawn gives a slot when the mod gives nothing (in the file's order: the paths reserved in that order)
    struct Default {
        std::string slot;
        enum Kind { None, Value, World, Square, Points } kind = None;
        uint32_t value = 0;
        float r = 0, lift = 0;
        bool lifted = false;
        std::vector<std::array<float, 3>> pts;  // (ahead, left, up from the spawn, in its yaw)
        int points() const { return kind == Square ? 4 : kind == Points ? static_cast<int>(pts.size()) : 0; }
    };
    std::vector<Default> defaults;
    struct Option {
        std::string name, slot, host;  // a slot written (min..max), or a host behaviour
        int32_t min = 0, max = 0;
    };
    std::vector<Option> options;
    std::string hunt;        // "object" (a started object's hunt flag, host hunt_host), "moby" (any moby: host hunt_host)
    std::string hunt_host;
    std::string frame_host;  // after each frame's updates
    int path_defaults = 0;   // (defaults that reserve a path)
};

namespace {

std::map<std::pair<int, uint32_t>, std::unique_ptr<Profile>> g_profiles;

const Profile::Slot* slot_of(const Profile& p, const std::string& name) {
    for (const auto& s : p.slots)
        if (s.name == name) return &s;
    return nullptr;
}

std::unique_ptr<Profile> read_profile(int level, uint32_t cls) {
    char name[64];
    std::snprintf(name, sizeof name, "data/pool/profiles/L%d_%u.txt", level, cls);
    std::ifstream in(name);
    if (!in) return nullptr;
    auto p = std::make_unique<Profile>();
    p->level = level;
    p->cls = cls;
    std::string line;
    int n = 0;
    auto bad = [&](const std::string& why) { std::fprintf(stderr, "[profile] %s:%d: %s\n", name, n, why.c_str()); };
    while (std::getline(in, line)) {
        ++n;
        if (const size_t h = line.find('#'); h != std::string::npos) line.resize(h);
        std::istringstream s(line);
        std::string kw;
        if (!(s >> kw)) continue;
        if (kw == "class") {
            std::string lc;
            s >> lc;
            std::getline(s, p->what);
            p->what.erase(0, p->what.find_first_not_of(' '));
        } else if (kw == "pvar_size") {
            std::string v;
            s >> v;
            p->pvar_size = static_cast<uint32_t>(std::strtoul(v.c_str(), nullptr, 0));
        } else if (kw == "slot") {
            std::string off, kind, nm;
            if (!(s >> off >> kind >> nm)) {
                bad("slot <offset> <kind> <name>");
                continue;
            }
            p->slots.push_back({static_cast<uint32_t>(std::strtoul(off.c_str(), nullptr, 0)), kind, nm});
        } else if (kw == "default") {
            Profile::Default d;
            std::string k;
            if (!(s >> d.slot >> k) || slot_of(*p, d.slot) == nullptr) {
                bad("default <slot> ...: no such slot");
                continue;
            }
            if (k == "none") {
                d.kind = Profile::Default::None;
            } else if (k == "value") {
                std::string v;
                s >> v;
                d.kind = Profile::Default::Value;
                d.value = static_cast<uint32_t>(std::strtol(v.c_str(), nullptr, 0));
            } else if (k == "world") {
                d.kind = Profile::Default::World;
            } else if (k == "square") {
                d.kind = Profile::Default::Square;
                s >> d.r;
            } else if (k == "points") {
                d.kind = Profile::Default::Points;
                std::string rest;
                std::getline(s, rest);
                std::istringstream r(rest);
                std::string w;
                if (r >> w && w == "lift") {
                    r >> d.lift;
                    d.lifted = true;
                } else {
                    r.clear();
                    r.str(rest);
                }
                std::string pt;
                while (std::getline(r, pt, ',')) {
                    std::istringstream q(pt);
                    std::array<float, 3> v{};
                    if (q >> v[0] >> v[1] >> v[2]) d.pts.push_back(v);
                }
                if (d.pts.empty() || d.pts.size() > 16) {
                    bad("default <slot> points [lift <h>] <ahead left up>, ...: 1 to 16 points");
                    continue;
                }
            } else {
                bad("default: none, value <n>, world, square <r> or points");
                continue;
            }
            p->path_defaults += d.points() > 0;
            p->defaults.push_back(d);
        } else if (kw == "option") {
            Profile::Option o;
            std::string how;
            s >> o.name >> how;
            if (how == "slot") s >> o.slot >> o.min >> o.max;
            else if (how == "host") s >> o.host;
            p->options.push_back(o);
        } else if (kw == "hunt") {
            std::string how;
            s >> p->hunt >> how >> p->hunt_host;
        } else if (kw == "frame") {
            std::string how;
            s >> how >> p->frame_host;
        } else {
            bad("unknown line '" + kw + "'");
        }
    }
    if (p->path_defaults > 4) {
        bad("more than 4 paths reserved: the profile is not used");
        return nullptr;
    }
    return p;
}

// ---- helpers (the level's collision) ----
uint32_t moby_base() { return lwu(OV(0x0015ffd8)); }
bool is_moby(uint32_t m) {
    const uint32_t b = moby_base();
    return m >= b && m < lwu(OV(0x0015ffe0)) + 0x100 && (m - b) % 0x100 == 0;
}
void moby_moved(uint32_t m) {
    if (!is_moby(m) || lwu(m + 0x24) == 0) return;
    for (uint32_t k = 0; k < 3; ++k) {  // (a rotation out of range: folded, as the SDK's object_place)
        const float a = lwc1(m + 0x40 + 4 * k);
        if (!(a >= -3.14159265f && a <= 3.14159265f))
            swc1(m + 0x40 + 4 * k, std::isfinite(a) ? static_cast<float>(std::remainder(static_cast<double>(a), 2.0 * 3.14159265358979323846)) : 0.0f);
    }
    moby_update_transform(m);
}

// the level's collision between a and b (the mobys passed through; `self` left out): the point hit and its normal
bool level_hit(const float a[3], const float b[3], uint32_t self, float* point, float* normal) {
    Vec4 s{a[0], a[1], a[2], 1.0f};
    const Vec4 e{b[0], b[1], b[2], 1.0f};
    uint32_t skip = self;
    const uint32_t outb = OV(0x00173e40);
    for (int k = 0; k < 4; ++k) {
        if (segment_collide_v(s, e, 0, skip, nullptr) == 0) return false;
        const uint32_t m = lwu(outb + 0x18);
        if (!is_moby(m)) {
            for (uint32_t c = 0; c < 3; ++c) {
                if (point != nullptr) point[c] = lwc1(outb + 0x20 + 4 * c);
                if (normal != nullptr) normal[c] = lwc1(outb + 0x40 + 4 * c);
            }
            return true;
        }
        skip = m;  // (a moby: through it, on from where it was hit)
        s = Vec4{lwc1(outb + 0x20), lwc1(outb + 0x24), lwc1(outb + 0x28), 1.0f};
    }
    return false;
}

// the level's floor under (x, y) from height z down, NAN none within 60
float level_floor(float x, float y, float z, uint32_t self) {
    const float a[3] = {x, y, z}, b[3] = {x, y, z - 60.0f};
    float p[3];
    return level_hit(a, b, self, p, nullptr) ? p[2] : NAN;
}

// ---- the paths a profile reserves ----
// the whole level's area (a square of 4000 round the origin), one a registry, shared; reserved before the first object's
std::map<int, int16_t> g_world;  // registry level -> its path index
bool g_any = false;

int16_t world_of(int level) {
    const auto it = g_world.find(level);
    if (it != g_world.end()) return it->second;
    uint32_t mem = 0;
    const int32_t slot = ld_path_reserve(level, 4, &mem);
    g_world[level] = static_cast<int16_t>(slot);
    if (slot >= 0) {
        const float q[4][3] = {{-4000.0f, -4000.0f, 0.0f}, {4000.0f, -4000.0f, 0.0f}, {4000.0f, 4000.0f, 0.0f}, {-4000.0f, 4000.0f, 0.0f}};
        ld_path_set(mem, q, 4);
    }
    return static_cast<int16_t>(slot);
}

void square(uint32_t mem, float x, float y, float z, float r) {
    const float q[4][3] = {{x - r, y - r, z}, {x + r, y - r, z}, {x + r, y + r, z}, {x - r, y + r, z}};
    ld_path_set(mem, q, 4);
}

// an object's paths reserved (its profile's path defaults, in order): false when there is no room
bool reserve(ProfObj& o) {
    int k = 0;
    for (const Profile::Default& d : o.prof->defaults) {
        if (d.points() == 0) continue;
        const int32_t slot = ld_path_reserve(o.level, d.points(), &o.pmem[k]);
        if (slot < 0) return false;
        o.path[k++] = static_cast<int16_t>(slot);
    }
    return true;
}

// the profile's defaults written into a started object's pvars (its start and yaw: o.start)
void write_defaults(ProfObj& o) {
    const uint32_t pv = o.pvars;
    const float x = o.start[0], y = o.start[1], z = o.start[2], c = std::cos(o.start[3]), sn = std::sin(o.start[3]);
    int k = 0;
    for (const Profile::Default& d : o.prof->defaults) {
        const Profile::Slot* s = slot_of(*o.prof, d.slot);
        switch (d.kind) {
        case Profile::Default::None:
            sw(pv + s->off, 0xffffffffu);
            break;
        case Profile::Default::Value:
            sw(pv + s->off, d.value);
            break;
        case Profile::Default::World:
            sw(pv + s->off, static_cast<uint32_t>(static_cast<int32_t>(world_of(o.level))));
            break;
        case Profile::Default::Square:
            sw(pv + s->off, static_cast<uint32_t>(static_cast<int32_t>(o.path[k])));
            square(o.pmem[k], x, y, z, d.r);
            ++k;
            break;
        case Profile::Default::Points: {
            sw(pv + s->off, static_cast<uint32_t>(static_cast<int32_t>(o.path[k])));
            // (ahead along its yaw, to its left, up; `lift` first raises the spawn's point: the dropships' arrival 9 above it)
            float p[16][3];
            const float zz = d.lifted ? z + d.lift : z;
            for (size_t i = 0; i < d.pts.size(); ++i) {
                const float f = d.pts[i][0], l = d.pts[i][1], u = d.pts[i][2];
                p[i][0] = l == 0.0f ? x + f * c : x + f * c - l * sn;
                p[i][1] = l == 0.0f ? y + f * sn : y + f * sn + l * c;
                p[i][2] = u == 0.0f ? zz : zz + u;
            }
            ld_path_set(o.pmem[k], p, static_cast<int>(d.pts.size()));
            ++k;
            break;
        }
        }
    }
}

// ---- the host behaviours (the SDK's ABI 5 fixes, unchanged) ----
// Kalebo III's enemies on a level mod (their updates are Kalebo III's): on their own level they wait for its data: the level's
// paths (the table 0x1b04b0, Kalebo's 0x1b0930: a path is its point count, 12 bytes, then points of 16 bytes), its moby groups,
// its markers, a trigger class that wakes them. A level mod has none of them (the composer keeps the flight path only), so
// their profiles give them what they read at each base_spawn (L16_52.txt, L16_541.txt, L16_638.txt, L16_1356.txt), and these
// host behaviours do the rest:
// - 52, the bomb bots (0x2a1088): hunting, its alert timer (+0x190) kept running, whose target search (0x25fcb8) takes Ratchet
//   at any distance and skips the give-up test (24 units on the ground, 3 above or below).
// - 541, the Gadgetron robots (0x2cddb8): on their level they stand hidden in an alcove (state 1) until class 654 opens the
//   door of their group (+0xcc) and walks them out along a path (+0xc4); after its first update (hidden, state 1) it is shown
//   and put in its weapon's patrol state (3, 5, 7: as state 2 leaves it at the path's start); after each update a marker
//   target (+0xb4 = 2) becomes Ratchet. Hunting: the path's two points put beside Ratchet on the robot's side (the launcher
//   9 units away, the flamethrower 3.5, the club through him), again when he has moved 4 units.
// - 638, the jetpack troopers (0x2d2cf0): their flight kept out of walls and over the floor (kal_trooper). Hunting: its alert
//   timer (+0x2e0: the range 255, target_select_any) kept running and its home (+0x2b0) 10 units from Ratchet on its side
//   (short of a wall between them), again when he has moved 4 units, flown to (state 1, as from its dropship).
// - 1356, the dropships (0x2e22d8): troopers named in +0xc0..+0xcc (moby indices: up to 4, -1 none; 0x2d3dc0 boards them,
//   0x2d3de0 drops one every half second) by base_option. Hunting: no leave (it stays over the landing point and fights), the
//   area the whole level, its troopers hunting too.
// - 1445, the robots (L16:0x2e5e08 / 0x2e6208), ABI 4: their first update keeps where they stand as their guard point (pvars
//   +0x1d0); each frame the target's place (+0x180, Ratchet's: +0x1c0 the target) is tested against it, and a target more
//   than 16 units (on the ground) from it or 3 units above or below the robot makes them give up for good (+0x1c4 = 2, which
//   nothing clears): they walk back to it and stand there. Hunting: after the frame's updates the guard point is put on
//   Ratchet and the giving up cleared, so that the next frame's test passes.

uint32_t pool_moby_index(const ProfObj& o) { return (o.moby - moby_base()) >> 8; }

void hunt_on(std::vector<ProfObj>& pool, ProfObj& o) {
    o.hunt = true;
    o.anchor[0] = o.anchor[1] = o.anchor[2] = 1e9f;  // (aimed at the next frame)
    if (o.prof->hunt_host == "kal_dropship") {
        sw(o.pvars + 0xd0, static_cast<uint32_t>(static_cast<int32_t>(world_of(o.level))));
        if (lbu(o.moby + 0x20) < 4) sw(o.pvars + 0xdc, 0xffffffffu);
        for (size_t t : o.troops)
            if (prof_alive(pool[t])) hunt_on(pool, pool[t]);
    }
}

// ---- the jetpack troopers' flight (638) on a level mod ----
// Their own code (0x2d2cf0) flies a point of theirs (pvars +0x2c0; the moby is put there plus a bob of 0.25 each frame) straight
// at its target (0x2d3eb8: the home +0x2b0 in state 1, a path's points in states 3 / 7) through anything, at the target's
// height: on Kalebo III the level's paths keep them clear of walls, 0 to 2 units over the floor (measured: their points). On
// a level mod they have no such path. After their update, in their flying states (not 0 starting, 9 falling dead - the game's
// enemy_physics has the floor -, 10 carried), kal_trooper takes their point's move back from a wall it would cross (sliding
// along it when it can, rising over it - a jetpack - when it cannot) and keeps them JET_HOVER over the floor under them (up a
// ramp's slope at their pace, down a ledge more slowly); their home's height made the same, so they arrive (the game's test:
// within 1 unit, in 3D).
constexpr float JET_HOVER = 0.5f;     // over the floor (their own level's points: 0..2 over it, most within 0.4)
constexpr float JET_BODY = 0.8f;      // the wall test's height over their point
constexpr float JET_RISE = 0.1f;      // units a frame up to their height (a slope), down
constexpr float JET_SINK = 0.05f;
constexpr float JET_CLIMB = 0.08f;    // units a frame up a wall in their way, at most JET_CLIMB_MAX over the floor
constexpr float JET_CLIMB_MAX = 14.0f;

void kal_trooper(ProfObj& o, uint32_t st) {
    const uint32_t m = o.moby, pv = o.pvars;
    if (st == 0 || st == 9 || st == 10) {
        o.jprev_ok = false;
        return;
    }
    if (lw(pv + 0x128) != 0) sw(pv + 0x128, 0);  // (carried: only state 10 is; out of its dropship, no more)
    // its home: at the height over the floor there (once each time it changes)
    const float hx = lwc1(pv + 0x2b0), hy = lwc1(pv + 0x2b4), hz = lwc1(pv + 0x2b8);
    if (hx != o.jhome[0] || hy != o.jhome[1] || hz != o.jhome[2]) {
        const float g = level_floor(hx, hy, hz + 3.0f, m);
        if (!std::isnan(g)) swc1(pv + 0x2b8, g + JET_HOVER);
        o.jhome[0] = hx, o.jhome[1] = hy, o.jhome[2] = lwc1(pv + 0x2b8);
    }
    float b[3], off[3];
    for (uint32_t c = 0; c < 3; ++c) {
        b[c] = lwc1(pv + 0x2c0 + 4 * c);
        off[c] = lwc1(m + 0x10 + 4 * c) - b[c];  // (its bob)
    }
    if (!o.jprev_ok) {
        std::memcpy(o.jprev, b, sizeof b);
        o.jprev_ok = true;
        o.jclimb = o.jstuck = 0;
        return;
    }
    const float* p = o.jprev;
    const float mx = b[0] - p[0], my = b[1] - p[1];
    bool blocked = false;
    if (std::fabs(mx) + std::fabs(my) > 1e-4f) {
        const float a[3] = {p[0], p[1], p[2] + JET_BODY};
        const float e[3] = {b[0], b[1], p[2] + JET_BODY};
        float hit[3], n[3];
        if (level_hit(a, e, m, hit, n)) {
            // along the wall: the move less its part into the wall
            float sx = 0.0f, sy = 0.0f;
            const float nn = n[0] * n[0] + n[1] * n[1];
            if (nn > 0.01f) {
                const float d = (mx * n[0] + my * n[1]) / nn;
                sx = mx - d * n[0], sy = my - d * n[1];
            }
            const float e2[3] = {p[0] + sx, p[1] + sy, p[2] + JET_BODY};
            if (std::fabs(sx) + std::fabs(sy) > 1e-4f && !level_hit(a, e2, m, nullptr, nullptr)) {
                b[0] = p[0] + sx, b[1] = p[1] + sy;
            } else {
                b[0] = p[0], b[1] = p[1];
                blocked = true;
            }
            o.jclimb = 20;
        }
    }
    const float g = level_floor(b[0], b[1], p[2] + JET_BODY + 0.5f, m);
    float z = p[2];
    if (o.jclimb > 0) {
        --o.jclimb;
        if (std::isnan(g) || z < g + JET_CLIMB_MAX) z += JET_CLIMB;
        if (!std::isnan(g) && z < g + JET_HOVER) z = std::min(g + JET_HOVER, p[2] + JET_RISE);
    } else if (!std::isnan(g)) {
        z += std::clamp(g + JET_HOVER - z, -JET_SINK, JET_RISE);
    }
    b[2] = z;
    // a wall it cannot get past nor over for 3 s: it stays where it is (its home there)
    o.jstuck = blocked ? o.jstuck + 1 : 0;
    if (o.jstuck > 180 && st == 1) {
        for (uint32_t c = 0; c < 3; ++c) swc1(pv + 0x2b0 + 4 * c, b[c]);
        o.jhome[0] = b[0], o.jhome[1] = b[1], o.jhome[2] = b[2];
        o.jstuck = 0;
    }
    for (uint32_t c = 0; c < 3; ++c) {
        swc1(pv + 0x2c0 + 4 * c, b[c]);
        swc1(m + 0x10 + 4 * c, b[c] + off[c]);
    }
    moby_moved(m);
    std::memcpy(o.jprev, b, sizeof b);
}

// one started object's host behaviour after the frame's updates (`index` its place in its list: the troopers' sides)
void frame_one(std::vector<ProfObj>& list, ProfObj& o, uint32_t hero) {
    const float hx = lwc1(hero + 0x10), hy = lwc1(hero + 0x14), hz = lwc1(hero + 0x18);
    const uint32_t m = o.moby, pv = o.pvars;
    const uint32_t st = lbu(m + 0x20);
    const float mx = lwc1(m + 0x10), my = lwc1(m + 0x14), mz = lwc1(m + 0x18);
    float dx = mx - hx, dy = my - hy;
    const float d = std::sqrt(dx * dx + dy * dy);
    if (d > 0.01f) dx /= d, dy /= d;
    else dx = 1.0f, dy = 0.0f;
    const bool moved = std::fabs(o.anchor[0] - hx) + std::fabs(o.anchor[1] - hy) > 4.0f;
    const std::string& f = o.prof->frame_host;
    if (f == "kal_bomb_bot") {
        if (o.hunt) sw(pv + 0x190, 60);
    } else if (f == "kal_gadgetron") {
        if (!o.woke && st == 1) {  // (hidden in its alcove: out, as class 654's wake 0x2ceca8 and state 2's end)
            o.woke = true;
            sb(m + 0x31, 1);
            sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
            sw(pv + 0xf0, 30);
            sw(pv + 0xdc, 1);
            sw(pv + 0x108, 1);
            const int32_t w = lw(pv + 0xc0);
            sb(m + 0x20, w == 0 ? 3u : w == 1 ? 5u : 7u);
        }
        if (lw(pv + 0xb4) == 2) {  // (a marker's turn, or none found: Ratchet)
            sw(pv + 0xb4, 0);
            for (uint32_t k = 0; k < 3; ++k) swc1(pv + 0x70 + 4 * k, lwc1(hero + 0x10 + 4 * k));
        }
        if (o.hunt && o.woke && moved) {
            const int32_t w = lw(pv + 0xc0);
            const float px = -dy, py = dx;
            float p[2][3];
            if (w == 1) {
                p[0][0] = hx + 3.0f * dx, p[0][1] = hy + 3.0f * dy;
                p[1][0] = hx - 4.0f * dx, p[1][1] = hy - 4.0f * dy;
            } else {
                const float r = w == 0 ? 9.0f : 3.5f, side = w == 0 ? 3.0f : 1.5f;
                p[0][0] = hx + r * dx + side * px, p[0][1] = hy + r * dy + side * py;
                p[1][0] = hx + r * dx - side * px, p[1][1] = hy + r * dy - side * py;
            }
            p[0][2] = p[1][2] = mz;  // (on its own floor)
            ld_path_set(o.pmem[0], p, 2);
            o.anchor[0] = hx, o.anchor[1] = hy, o.anchor[2] = hz;
        }
    } else if (f == "kal_trooper") {
        if (o.hunt) {
            sw(pv + 0x2e0, 120);
            if (moved && st != 10 && st != 0) {  // (a home 10 units from him on its side, short of a wall between: flown to;
                                                   // a few units aside each, not all on one point)
                const float side = static_cast<float>(static_cast<int>(&o - list.data()) % 5 - 2) * 2.0f;
                const float ux = 10.0f * dx - side * dy, uy = 10.0f * dy + side * dx, ul = std::hypot(ux, uy);
                const float a[3] = {hx, hy, hz + JET_BODY}, e[3] = {hx + ux, hy + uy, hz + JET_BODY};
                float hit[3], r = ul;
                if (level_hit(a, e, 0, hit, nullptr)) r = std::max(0.0f, std::hypot(hit[0] - hx, hit[1] - hy) - 1.5f);
                swc1(pv + 0x2b0, hx + ux / ul * r);
                swc1(pv + 0x2b4, hy + uy / ul * r);
                swc1(pv + 0x2b8, hz);
                o.jgo = true;
                o.anchor[0] = hx, o.anchor[1] = hy, o.anchor[2] = hz;
            }
            if (o.jgo && st == 2) {  // (hovering: off to it, as when dropped - state 1, its drop's time over)
                sb(m + 0x20, 1);
                sh(pv + 0x2dc, 0);
                o.jgo = false;
            }
        }
        kal_trooper(o, lbu(m + 0x20));
    }
}

// (ABI 4) the mobys made to hunt by a "hunt moby" profile (Kalebo III's robots), any moby of the class
struct Hunter {
    uint32_t moby = 0;
    int32_t cls = 0;
    uint32_t pvars = 0;
    std::string host;
};
std::vector<Hunter> g_hunters;

void hunters_update() {
    const uint32_t h = moby_base();
    for (size_t i = 0; i < g_hunters.size();) {
        const Hunter& u = g_hunters[i];
        if (static_cast<int8_t>(lbu(u.moby + 0x20)) < 0 || lh(u.moby + 0xa6) != u.cls || lwu(u.moby + 0x78) != u.pvars) {
            g_hunters.erase(g_hunters.begin() + static_cast<std::ptrdiff_t>(i));  // (gone: killed, removed)
            continue;
        }
        if (u.host == "kal_robot_guard" && h != 0 && lbu(u.moby + 0x20) != 0) {  // (once its first update kept its guard point)
            for (uint32_t k = 0; k < 3; ++k) swc1(u.pvars + 0x1d0 + 4 * k, lwc1(h + 0x10 + 4 * k));
            if (lw(u.pvars + 0x1c4) == 2) sw(u.pvars + 0x1c4, 0);
        }
        ++i;
    }
}

// the session's spawns of imported classes with a profile
std::vector<ProfObj> g_session;

}  // namespace

// orig: (host) see class_profiles.h
const Profile* profile_find(int level, uint32_t cls) {
    const auto key = std::make_pair(level, cls);
    auto it = g_profiles.find(key);
    if (it == g_profiles.end()) it = g_profiles.emplace(key, read_profile(level, cls)).first;
    return it->second.get();
}

// orig: (host) see class_profiles.h
bool profile_slot(const Profile* p, const std::string& name, uint32_t* off, std::string* kind) {
    const Profile::Slot* s = p != nullptr ? slot_of(*p, name) : nullptr;
    if (s == nullptr) return false;
    *off = s->off;
    *kind = s->kind;
    return true;
}

// orig: (host) see class_profiles.h
bool profile_default_at(const Profile* p, uint32_t off) {
    if (p == nullptr) return false;
    for (const Profile::Default& d : p->defaults)
        if (const Profile::Slot* s = slot_of(*p, d.slot); s != nullptr && s->off == off) return true;
    return false;
}

// orig: (host) see class_profiles.h
std::string profile_slots_text(const Profile* p) {
    std::string t;
    if (p != nullptr)
        for (const Profile::Slot& s : p->slots) t += (t.empty() ? "" : " ") + s.name + ":" + s.kind;
    return t;
}

// orig: (host) see class_profiles.h
std::string profile_name(const Profile* p) { return p == nullptr ? "-" : std::to_string(p->level) + ":" + std::to_string(p->cls) + " " + p->what; }

// orig: (host) see class_profiles.h
bool prof_alive(const ProfObj& o) {
    return o.out && static_cast<int8_t>(lbu(o.moby + 0x20)) >= 0 && lh(o.moby + 0xa6) == o.cls && lwu(o.moby + 0x78) == o.pvars;
}

// orig: (host) see class_profiles.h
void profiles_pool_take(std::vector<ProfObj>& pool, int level) {
    ld_base_reset();
    g_world.clear();
    g_any = false;
    size_t n = 0, refused = 0;
    for (ProfObj& o : pool) {
        o.level = level;
        o.prof = profile_find(level, static_cast<uint32_t>(o.cls));
        if (o.prof == nullptr || o.prof->path_defaults == 0) continue;
        // (the whole level's area first, shared: reserved before the first object's paths)
        if (world_of(level) < 0) break;
        const bool ok = reserve(o);
        o.adapted = ok;
        o.refused = !ok;
        ok ? ++n : ++refused;
    }
    g_any = n > 0;
    if (n + refused > 0)
        std::printf("[sdk] the base level's profiled enemies (data/pool/profiles): %zu pool objects given paths of their own%s\n", n,
                    refused > 0 ? (" (" + std::to_string(refused) + " not: no room, base_spawn refuses them)").c_str() : "");
}

// orig: (host) see class_profiles.h
void profiles_spawned(std::vector<ProfObj>& pool, size_t i) {
    ProfObj& o = pool[i];
    o.woke = false;
    o.hunt = false;
    o.troops.clear();
    o.jprev_ok = o.jgo = false;
    o.jclimb = o.jstuck = 0;
    o.jhome[0] = o.jhome[1] = o.jhome[2] = 1e9f;
    if (!o.adapted || o.prof == nullptr) return;
    write_defaults(o);
}

// orig: (host) see class_profiles.h
int profiles_hunt(std::vector<ProfObj>& pool, uint32_t m, int on) {
    const int32_t cls = lh(m + 0xa6);
    g_hunters.erase(std::remove_if(g_hunters.begin(), g_hunters.end(), [&](const Hunter& h) { return h.moby == m; }), g_hunters.end());
    const Profile* p = profile_find(lw(0x0015ed84), static_cast<uint32_t>(cls));
    if (p == nullptr || p->hunt.empty()) return -1;
    if (p->hunt == "object") {
        ProfObj* o = nullptr;
        for (ProfObj& q : pool)
            if (q.moby == m && prof_alive(q)) o = &q;
        if (o == nullptr || !o->adapted) return 0;
        if (on == 0) {
            o->hunt = false;
            return 1;
        }
        hunt_on(pool, *o);
        return 1;
    }
    if (on == 0) return 1;
    if (lwu(m + 0x78) == 0) return 0;
    g_hunters.push_back({m, cls, lwu(m + 0x78), p->hunt_host});
    return 1;
}

// orig: (host) see class_profiles.h
int32_t profiles_option(std::vector<ProfObj>& pool, size_t i, int32_t option, int32_t value) {
    ProfObj& o = pool[i];
    if (o.prof == nullptr) return -1;
    const char* name = option == RAC1_BASE_WEAPON ? "weapon" : option == RAC1_BASE_TROOPS ? "troops" : "";
    for (const Profile::Option& op : o.prof->options) {
        if (op.name != name) continue;
        if (!op.slot.empty()) {
            const Profile::Slot* s = slot_of(*o.prof, op.slot);
            if (s == nullptr || value < op.min || value > op.max) return 0;
            sw(o.pvars + s->off, static_cast<uint32_t>(value));
            return 1;
        }
        if (op.host == "kal_dropship_troops" && g_prof_base_spawn != nullptr) {
            int32_t n = static_cast<int32_t>(o.troops.size());
            const float at[3] = {o.start[0], o.start[1], o.start[2]};
            const float yaw = o.start[3];
            for (; n < value && n < 4;) {
                const uint32_t th = g_prof_base_spawn(638, at, yaw);
                if (th == 0) break;
                ProfObj& t = pool[(th & 0xffffu) - 1];
                sw(t.pvars + 0x128, 1);  // (carried: hidden until its dropship drops it)
                sw(pool[i].pvars + 0xc0 + 4 * static_cast<uint32_t>(n), pool_moby_index(t));
                pool[i].troops.push_back((th & 0xffffu) - 1);
                ++n;
            }
            return n;
        }
        return 0;
    }
    return 0;
}

// orig: (host) see class_profiles.h
void profiles_frame(std::vector<ProfObj>& pool) {
    if (!g_hunters.empty()) hunters_update();
    if (!g_any) return;
    const uint32_t hero = moby_base();
    if (hero == 0) return;
    for (ProfObj& o : pool) {
        if (!o.adapted || !prof_alive(o) || o.prof->frame_host.empty()) continue;
        frame_one(pool, o, hero);
    }
}

// orig: (host) see class_profiles.h
void profiles_level() { g_hunters.clear(); }

// orig: (host) see class_profiles.h
bool profiles_any() { return g_any || !g_hunters.empty(); }

// ---- a session's spawns ----
// orig: (host) see class_profiles.h
bool profiles_session_spawn(int level, uint32_t cls, uint32_t moby, const float pos[3], float yaw) {
    const Profile* p = profile_find(level, cls);
    if (p == nullptr) return false;
    ProfObj o;
    o.cls = lh(moby + 0xa6);
    o.moby = moby;
    o.pvars = lwu(moby + 0x78);
    o.out = true;
    o.level = level;
    o.prof = p;
    o.start[0] = pos[0], o.start[1] = pos[1], o.start[2] = pos[2], o.start[3] = yaw;
    o.jhome[0] = o.jhome[1] = o.jhome[2] = 1e9f;
    if (o.pvars == 0) return false;
    if (!reserve(o)) {
        std::fprintf(stderr, "[profile] %s: no room for its paths in level %d's registry: its defaults not written\n", profile_name(p).c_str(), level);
        for (int k = 0; k < 4; ++k)
            if (o.path[k] >= 0) ld_path_release(level, o.path[k]);
        return false;
    }
    o.adapted = true;
    write_defaults(o);
    g_session.push_back(std::move(o));
    return true;
}

// orig: (host) see class_profiles.h
void profiles_session_frame() {
    if (g_session.empty()) return;
    const uint32_t hero = moby_base();
    for (size_t i = 0; i < g_session.size();) {
        ProfObj& o = g_session[i];
        if (!prof_alive(o)) {
            for (int k = 0; k < 4; ++k)
                if (o.path[k] >= 0) ld_path_release(o.level, o.path[k]);
            g_session.erase(g_session.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        if (hero != 0 && !o.prof->frame_host.empty()) frame_one(g_session, o, hero);
        ++i;
    }
}

// orig: (host) see class_profiles.h
void profiles_session_clear() {
    g_session.clear();
    for (auto it = g_world.begin(); it != g_world.end();)  // (the contexts' world areas: their registries go)
        it = it->first != lw(0x0015ed84) ? g_world.erase(it) : std::next(it);
}

// orig: (host) see class_profiles.h
ProfObj* profiles_session_obj(uint32_t moby) {
    for (ProfObj& o : g_session)
        if (o.moby == moby && prof_alive(o)) return &o;
    return nullptr;
}

std::vector<ProfObj>* g_prof_pool = nullptr;

// orig: (host) see class_profiles.h
ProfObj* profiles_obj(uint32_t moby) {
    if (ProfObj* o = profiles_session_obj(moby)) return o;
    if (g_prof_pool != nullptr)
        for (ProfObj& o : *g_prof_pool)
            if (o.moby == moby && o.out && o.prof != nullptr && prof_alive(o)) return &o;
    return nullptr;
}

// orig: (host) see class_profiles.h
int profiles_bind_points(uint32_t moby, const std::string& name, const float (*pts)[3], int n) {
    ProfObj* o = profiles_obj(moby);
    if (o == nullptr || n < 1 || n > 64) return 0;
    const Profile::Slot* s = slot_of(*o->prof, name);
    if (s == nullptr || (s->kind != "path" && s->kind != "area")) return 0;
    // (a path of its own reserved for the slot, again when it has fewer points than asked)
    int k = 0;
    for (const Profile::Default& d : o->prof->defaults) {
        if (d.points() == 0) continue;
        if (d.slot == name) break;
        ++k;
    }
    int32_t slot = -1;
    uint32_t mem = 0;
    const int32_t cur = lw(o->pvars + s->off);
    if (k < 4 && o->path[k] >= 0 && cur == o->path[k] && lw(o->pmem[k]) >= n) {
        slot = o->path[k];
        mem = o->pmem[k];
    } else {
        slot = ld_path_reserve(o->level, n, &mem);
        if (slot < 0) return 0;
        if (k < 4) {
            if (o->path[k] >= 0) ld_path_release(o->level, o->path[k]);
            o->path[k] = static_cast<int16_t>(slot);
            o->pmem[k] = mem;
        }
    }
    ld_path_set(mem, pts, n);
    sw(o->pvars + s->off, static_cast<uint32_t>(slot));
    return 1;
}

// orig: (host) see class_profiles.h
int profiles_bind_word(uint32_t moby, const std::string& name, uint32_t value) {
    ProfObj* o = profiles_obj(moby);
    if (o == nullptr) return 0;
    const Profile::Slot* s = slot_of(*o->prof, name);
    if (s == nullptr) return 0;
    sw(o->pvars + s->off, value);
    return 1;
}

}  // namespace rac1
