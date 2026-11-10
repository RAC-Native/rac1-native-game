// See setpieces.h. Not a port: the open engine's stage 7 (rails, items for the visit, the set-piece kit), on the game's own rail
// code (find_rail_v 0x20cd08, hero_rail_land_check 0x20cf58, grind_rail_update 0x217970) and camera (camera_update 0x1ed0a8:
// the view the renderer reads, 0x166dc0 / 0x166dd0 / 0x166fd0).
#include <game/enhance/setpieces.h>

#include <game/enhance/sections.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <host/level_slots.h>
#include <runtime/kernel/ee_memory.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace rac1 {

namespace {

constexpr uint32_t P = 0x0013f350;          // the player block
constexpr uint32_t STATE = 0x001413d4;      // his state (P + 0x2084)
constexpr uint32_t OWNED = 0x0013d4c0;      // the items owned, a byte each
constexpr uint32_t RAIL_TABLE = 0x0015f70c, RAIL_COUNT = 0x0015f710;
constexpr uint32_t CAM_POS = 0x00166dc0, CAM_ANGLES = 0x00166dd0, CAM_ROWS = 0x00166fd0;
constexpr int kHazards = 64;
constexpr float kPi = 3.14159265f;

struct Rail {
    std::string name;
    int section = 0, index = 0;
    bool loop = false;
    std::vector<std::array<float, 3>> pts;
    std::vector<float> cum;    // the length to each point
    float length = 0;
    bool enabled = true;
    float radius = 0;          // (the table's reach, kept while it is taken out of the search)
    float speed = 1.0f;
};

struct Hazard {
    bool live = false, on = true;
    rac1_hazard h{};
};

struct Cam {
    bool on = false;           // the plugin's camera
    rac1_camera c{};
    std::vector<std::array<float, 3>> path;
    std::vector<float> cum;
    float along = 0;           // (PATH) units along it
    int blend = 0, k = 0;      // blending from `from` over `blend` frames (k done)
    float from_pos[3] = {}, from_yaw = 0, from_pitch = 0;
    bool releasing = false;    // blending back to the game's
    float last_pos[3] = {}, last_yaw = 0, last_pitch = 0;
    bool has_last = false;
};

struct State {
    bool mod_level = false;
    std::vector<Rail> rails;
    std::vector<int> items;            // level.txt's
    std::vector<int> given;            // given for the visit (taken back when leaving)
    bool saving = false;
    int cur_rail = 0;                  // the rail Ratchet is on (handle), 0 none
    bool ended = false;                // its end told
    int attach_rail = 0, attach_frames = 0;  // hero_rail_attach under way
    int32_t attach_dir = 1;            // (its direction: the game's grind always starts along the points, +0x578 = 1)
    std::array<float, 3> attach_at{};
    // the fade
    int f_out = 0, f_hold = 0, f_in = 0, f_k = 0, f_phase = 0;
    uint32_t f_rgb = 0;
    int f_alpha = 0;
    Cam cam;
    std::vector<Hazard> hazards;
    uint64_t frames = 0;
} g_sp;

SetpiecesHost g_host;

bool trace() {
    static const bool t = std::getenv("RAC1_SETPIECE_TRACE") != nullptr;
    return t;
}

int current_section() {
    const int c = sections_current();
    return c < 0 ? 0 : c;
}

Rail* rail_of(int32_t h) { return h >= 1 && static_cast<size_t>(h) <= g_sp.rails.size() ? &g_sp.rails[static_cast<size_t>(h) - 1] : nullptr; }

// the rail's record in the game's table (its section the current one), 0 when it is not there
uint32_t rail_entry(const Rail& r) {
    if (r.section != current_section()) return 0;
    const uint32_t t = lwu(OV(RAIL_TABLE));
    if (t == 0 || r.index >= lw(OV(RAIL_COUNT))) return 0;
    return t + static_cast<uint32_t>(r.index) * 0x20;
}

void rail_apply(Rail& r) {
    const uint32_t e = rail_entry(r);
    if (e == 0) return;
    const float v = lwc1(e + 0xc);
    if (!r.enabled && v >= 0.0f) {
        r.radius = v;
        swc1(e + 0xc, -1.0f);  // (find_rail_v: a rail whose reach is less than the distance is skipped)
    } else if (r.enabled && v < 0.0f) {
        swc1(e + 0xc, r.radius);
    }
}

void point_at(const Rail& r, float d, float out[3], float dir[3]) {
    const size_t n = r.pts.size();
    if (r.loop && r.length > 0) {
        d = std::fmod(d, r.length);
        if (d < 0) d += r.length;
    } else {
        d = std::clamp(d, 0.0f, r.length);
    }
    size_t i = static_cast<size_t>(std::upper_bound(r.cum.begin(), r.cum.end(), d) - r.cum.begin());
    i = i == 0 ? 0 : i - 1;
    if (!r.loop && i >= n - 1) i = n - 2;
    const auto& a = r.pts[i];
    const auto& b = r.pts[(i + 1) % n];
    const float seg = std::max(1e-6f, (i + 1 < r.cum.size() ? r.cum[i + 1] : r.length) - r.cum[i]);
    const float t = std::clamp((d - r.cum[i]) / seg, 0.0f, 1.0f);
    float v[3];
    float len = 0;
    for (int l = 0; l < 3; ++l) {
        v[l] = b[static_cast<size_t>(l)] - a[static_cast<size_t>(l)];
        len += v[l] * v[l];
    }
    len = std::sqrt(len);
    for (int l = 0; l < 3; ++l) {
        if (out != nullptr) out[l] = a[static_cast<size_t>(l)] + v[l] * t;
        if (dir != nullptr) dir[l] = len > 1e-6f ? v[l] / len : (l == 0 ? 1.0f : 0.0f);
    }
}

bool grinding() {
    const uint32_t st = lwu(STATE);
    return (st >= 0x28 && st <= 0x2b) || st == 0x42;
}

// the rail (handle) Ratchet is on now, 0 none
int hero_rail_now() {
    if (g_sp.rails.empty() || !grinding()) return 0;
    const uint32_t path = lwu(P + 0x560);
    for (size_t i = 0; i < g_sp.rails.size(); ++i) {
        const uint32_t e = rail_entry(g_sp.rails[i]);
        if (e != 0 && lwu(e + 0x10) == path) return static_cast<int>(i + 1);
    }
    return 0;
}

float hero_distance(const Rail& r) {
    const int32_t i = std::clamp(lw(P + 0x564), 0, static_cast<int32_t>(r.pts.size()) - 1);
    return std::min(r.length, r.cum[static_cast<size_t>(i)] + lwc1(P + 0x568));
}

void give(int id) {
    const uint32_t a = OWNED + static_cast<uint32_t>(id);
    if (lbu(a) != 0) return;
    sb(a, 1);
    g_sp.given.push_back(id);
}

void take_back_all() {
    for (int id : g_sp.given) sb(OWNED + static_cast<uint32_t>(id), 0);
    if (!g_sp.given.empty()) std::printf("[setpiece] %zu items of the visit taken back\n", g_sp.given.size());
    g_sp.given.clear();
}

// the camera's view written as the game's camera update writes it: position, rows, Euler angles (x 0, y the pitch, z the yaw;
// the game's pitch is down positive, ours up positive)
void cam_write(const float pos[3], float yaw, float pitch_up) {
    const float pitch = -pitch_up;
    for (uint32_t l = 0; l < 3; ++l) swc1(OV(CAM_POS) + 4 * l, pos[l]);
    const Vec4 ang{0.0f, pitch, yaw, 0.0f};
    Mat4 m;
    euler_to_mat4_vu(m, ang);
    for (uint32_t r = 0; r < 3; ++r)
        for (uint32_t l = 0; l < 4; ++l) swc1(OV(CAM_ROWS) + r * 0x10 + 4 * l, (&m.r[r].x)[l]);
    swc1(OV(CAM_ANGLES), 0.0f);
    swc1(OV(CAM_ANGLES) + 4, pitch);
    swc1(OV(CAM_ANGLES) + 8, yaw);
}

void look_angles(const float from[3], const float to[3], float& yaw, float& pitch) {
    const float dx = to[0] - from[0], dy = to[1] - from[1], dz = to[2] - from[2];
    yaw = std::atan2(dy, dx);
    pitch = std::atan2(dz, std::sqrt(dx * dx + dy * dy));
}

float sp_angle_lerp(float a, float b, float t) {
    float d = std::remainder(b - a, 2.0f * kPi);
    return std::remainder(a + d * t, 2.0f * kPi);
}

uint32_t moby_of(uint32_t m) { return m != 0 && m >= 0x100000 && m < 0x2000000 ? m : 0; }

void moby_pos(uint32_t m, float out[3]) {
    for (uint32_t l = 0; l < 3; ++l) out[l] = lwc1(m + 0x10 + 4 * l);
}

// the plugin's camera this frame: its place and angles
bool cam_pose(Cam& c, float pos[3], float& yaw, float& pitch) {
    const rac1_camera& k = c.c;
    float target[3] = {k.target[0], k.target[1], k.target[2]};
    const uint32_t m = moby_of(k.moby);
    switch (k.mode) {
    case RAC1_CAMERA_FIXED:
        std::memcpy(pos, k.pos, sizeof k.pos);
        yaw = k.yaw;
        pitch = k.pitch;
        return true;
    case RAC1_CAMERA_LOOK:
        std::memcpy(pos, k.pos, sizeof k.pos);
        if (m != 0) moby_pos(m, target);
        look_angles(pos, target, yaw, pitch);
        return true;
    case RAC1_CAMERA_PATH: {
        if (c.path.size() < 2) return false;
        const float total = c.cum.back();
        float d = c.along;
        if (k.loop != 0) d = std::fmod(d, total);
        d = std::clamp(d, 0.0f, total);
        size_t i = static_cast<size_t>(std::upper_bound(c.cum.begin(), c.cum.end(), d) - c.cum.begin());
        i = std::min(i == 0 ? 0 : i - 1, c.path.size() - 2);
        const float seg = std::max(1e-6f, c.cum[i + 1] - c.cum[i]);
        const float t = std::clamp((d - c.cum[i]) / seg, 0.0f, 1.0f);
        for (size_t l = 0; l < 3; ++l) pos[l] = c.path[i][l] + (c.path[i + 1][l] - c.path[i][l]) * t;
        if (m == 0 && target[0] == 0 && target[1] == 0 && target[2] == 0) {
            float ahead[3];
            for (size_t l = 0; l < 3; ++l) ahead[l] = c.path[i + 1][l] - c.path[i][l] + pos[l];
            look_angles(pos, ahead, yaw, pitch);
        } else {
            if (m != 0) moby_pos(m, target);
            look_angles(pos, target, yaw, pitch);
        }
        return true;
    }
    case RAC1_CAMERA_FOLLOW: {
        uint32_t f = m != 0 ? m : lwu(P + 0x2080);
        if (f == 0) return false;
        float at[3];
        moby_pos(f, at);
        const float fy = lwc1(f + 0x48), cy = std::cos(fy), sy = std::sin(fy);
        pos[0] = at[0] + cy * k.pos[0] - sy * k.pos[1];
        pos[1] = at[1] + sy * k.pos[0] + cy * k.pos[1];
        pos[2] = at[2] + k.pos[2];
        at[2] += k.target[2];
        look_angles(pos, at, yaw, pitch);
        return true;
    }
    default:
        return false;
    }
}

}  // namespace

// orig: (host) see setpieces.h
void setpieces_host(const SetpiecesHost& h) { g_host = h; }

// orig: (host) see setpieces.h
bool setpieces_rails_anywhere() { return !g_sp.rails.empty() && g_sp.mod_level && level_slots_on() && level_slot_current() >= 0; }

// orig: (host) see setpieces.h
void setpieces_level_load(int slot, const std::string& sdk) {
    take_back_all();
    const bool had_cam = g_sp.cam.on;
    g_sp = State{};
    if (had_cam && trace()) std::printf("[setpiece] the plugin's camera dropped with the level\n");
    g_sp.mod_level = slot >= 19;
    if (sdk.empty()) return;
    std::ifstream f(sdk);
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream in(line);
        std::string k;
        in >> k;
        if (k == "rail") {
            Rail r;
            int loop = 0;
            size_t n = 0;
            in >> r.name >> r.section >> r.index >> loop >> n;
            r.loop = loop != 0;
            for (size_t i = 0; i < n && in; ++i) {
                std::array<float, 3> v{};
                in >> v[0] >> v[1] >> v[2];
                r.pts.push_back(v);
            }
            if (!in || r.pts.size() < 2) continue;
            r.cum.push_back(0);
            for (size_t i = 1; i < r.pts.size(); ++i) {
                float d = 0;
                for (size_t l = 0; l < 3; ++l) d += (r.pts[i][l] - r.pts[i - 1][l]) * (r.pts[i][l] - r.pts[i - 1][l]);
                r.cum.push_back(r.cum.back() + std::sqrt(d));
            }
            r.length = r.cum.back();
            if (r.loop) {
                float d = 0;
                for (size_t l = 0; l < 3; ++l) d += (r.pts.front()[l] - r.pts.back()[l]) * (r.pts.front()[l] - r.pts.back()[l]);
                r.length += std::sqrt(d);
            }
            g_sp.rails.push_back(std::move(r));
        } else if (k == "items") {
            for (int id; in >> id;)
                if (id > 0 && id < 0x25) g_sp.items.push_back(id);
        }
    }
    if (!g_sp.rails.empty() || !g_sp.items.empty())
        std::printf("[setpiece] level slot %d: %zu rails of its own (the hero code's rail spots not needed), %zu items for the visit\n", slot, g_sp.rails.size(),
                    g_sp.items.size());
}

// orig: (host) see setpieces.h
void setpieces_level_start() {
    for (int id : g_sp.items) give(id);
    if (!g_sp.given.empty()) std::printf("[setpiece] %zu items given for the visit\n", g_sp.given.size());
    setpieces_section(current_section());
}

// orig: (host) see setpieces.h
void setpieces_section(int index) {
    for (Rail& r : g_sp.rails)
        if (r.section == index) rail_apply(r);
}

// orig: (host) see setpieces.h
void setpieces_save_begin() {
    if (g_sp.given.empty()) return;
    for (int id : g_sp.given) sb(OWNED + static_cast<uint32_t>(id), 0);
    g_sp.saving = true;
}

// orig: (host) see setpieces.h
void setpieces_save_end() {
    if (!g_sp.saving) return;
    for (int id : g_sp.given) sb(OWNED + static_cast<uint32_t>(id), 1);
    g_sp.saving = false;
    if (trace()) std::printf("[setpiece] a save written without the %zu items of the visit\n", g_sp.given.size());
}

// orig: (host) see setpieces.h
void setpieces_frame() {
    ++g_sp.frames;
    // the rails: Ratchet's rail now, its events
    if (!g_sp.rails.empty()) {
        const int now = hero_rail_now();
        if (g_sp.attach_rail != 0) {  // hero_rail_attach: until he is on it (or a few frames)
            if (now == g_sp.attach_rail && g_sp.attach_dir < 0) sw(P + 0x578, 0xffffffffu);  // (riding back: the grind's step direction)
            if (now == g_sp.attach_rail || ++g_sp.attach_frames > 10) {
                if (now != g_sp.attach_rail) std::fprintf(stderr, "[setpiece] hero_rail_attach: Ratchet did not take the rail %s\n", g_sp.rails[static_cast<size_t>(g_sp.attach_rail) - 1].name.c_str());
                g_sp.attach_rail = 0;
            }
        }
        auto event = [&](int h, int32_t ev) {
            const Rail& r = g_sp.rails[static_cast<size_t>(h) - 1];
            if (trace()) std::printf("[setpiece] frame %llu: rail %s %s\n", static_cast<unsigned long long>(g_sp.frames), r.name.c_str(), ev == RAC1_RAIL_ENTER ? "entered" : ev == RAC1_RAIL_END ? "ended" : "left");
            if (g_host.rail_event != nullptr) g_host.rail_event(h, r.name.c_str(), ev);
        };
        if (now != g_sp.cur_rail) {
            if (g_sp.cur_rail != 0) event(g_sp.cur_rail, RAC1_RAIL_LEAVE);
            g_sp.cur_rail = now;
            g_sp.ended = false;
            if (now != 0) event(now, RAC1_RAIL_ENTER);
        }
        if (now != 0) {
            Rail& r = g_sp.rails[static_cast<size_t>(now) - 1];
            if (!g_sp.ended && !r.loop) {
                const float d = hero_distance(r);
                const int32_t dir = lw(P + 0x578);
                if ((dir >= 0 && d >= r.length - 1.0f) || (dir < 0 && d <= 1.0f)) {
                    g_sp.ended = true;
                    event(now, RAC1_RAIL_END);
                }
            }
            if (r.speed != 1.0f && lw(P + 0x570) == 0) swc1(P + 0x574, lwc1(P + 0x574) * r.speed);  // (the next frame's step)
        }
    }
    // the hazards
    const uint32_t hm = lwu(P + 0x2080);
    if (hm != 0 && g_host.hero_damage != nullptr) {
        float hp[3];
        moby_pos(hm, hp);
        hp[2] += 0.5f;  // (his middle)
        for (Hazard& z : g_sp.hazards) {
            if (!z.live || !z.on) continue;
            const rac1_hazard& h = z.h;
            if (h.on_frames > 0) {
                const uint64_t round = static_cast<uint64_t>(h.on_frames) + static_cast<uint64_t>(std::max(0, h.off_frames));
                if ((g_sp.frames + static_cast<uint64_t>(std::max(0, h.phase))) % round >= static_cast<uint64_t>(h.on_frames)) continue;
            }
            float o[3] = {0, 0, 0};
            if (h.object != 0 && g_host.object_moby != nullptr) {
                const uint32_t m = g_host.object_moby(h.object);
                if (m == 0) continue;
                moby_pos(m, o);
            }
            bool in = true;
            if (h.shape == RAC1_HAZARD_SPHERE) {
                float d = 0;
                for (int l = 0; l < 3; ++l) d += (hp[l] - h.lo[l] - o[l]) * (hp[l] - h.lo[l] - o[l]);
                in = d <= h.hi[0] * h.hi[0];
            } else {
                for (int l = 0; l < 3; ++l) in = in && hp[l] >= h.lo[l] + o[l] && hp[l] <= h.hi[l] + o[l];
            }
            if (!in) continue;
            const int32_t before = lw(0x001415f8);  // (his health: the game's grace time keeps most calls from hurting)
            g_host.hero_damage(h.damage, 0);
            if (trace() && lw(0x001415f8) != before)
                std::printf("[setpiece] frame %llu: a hazard hurt Ratchet: health %d -> %d\n", static_cast<unsigned long long>(g_sp.frames), before, lw(0x001415f8));
        }
    }
    // the fade
    if (g_sp.f_phase != 0) {
        ++g_sp.f_k;
        if (g_sp.f_phase == 1) {
            g_sp.f_alpha = g_sp.f_out > 0 ? 0x80 * std::min(g_sp.f_k, g_sp.f_out) / g_sp.f_out : 0x80;
            if (g_sp.f_k >= g_sp.f_out) g_sp.f_phase = 2, g_sp.f_k = 0;
        } else if (g_sp.f_phase == 2) {
            g_sp.f_alpha = 0x80;
            if (g_sp.f_k >= g_sp.f_hold) g_sp.f_phase = 3, g_sp.f_k = 0;
        } else {
            g_sp.f_alpha = g_sp.f_in > 0 ? 0x80 - 0x80 * std::min(g_sp.f_k, g_sp.f_in) / g_sp.f_in : 0;
            if (g_sp.f_k >= g_sp.f_in) g_sp.f_phase = 0, g_sp.f_alpha = 0;
        }
    }
    // a path camera's travel
    if (g_sp.cam.on && g_sp.cam.c.mode == RAC1_CAMERA_PATH) g_sp.cam.along += g_sp.cam.c.speed / 60.0f;
}

// orig: (host) see setpieces.h
void setpieces_camera() {
    Cam& c = g_sp.cam;
    if (!c.on && !c.releasing) return;
    float pos[3], yaw = 0, pitch = 0;
    if (c.releasing) {  // back to the game's: from our last view to its, blended
        ++c.k;
        if (c.k >= c.blend || !c.has_last) {
            c.releasing = false;
            return;
        }
        float gp[3];
        for (uint32_t l = 0; l < 3; ++l) gp[l] = lwc1(OV(CAM_POS) + 4 * l);
        const float gpitch = -lwc1(OV(CAM_ANGLES) + 4), gyaw = lwc1(OV(CAM_ANGLES) + 8);
        const float t = static_cast<float>(c.k) / static_cast<float>(c.blend);
        for (int l = 0; l < 3; ++l) pos[l] = c.last_pos[l] + (gp[l] - c.last_pos[l]) * t;
        cam_write(pos, sp_angle_lerp(c.last_yaw, gyaw, t), sp_angle_lerp(c.last_pitch, gpitch, t));
        return;
    }
    if (!cam_pose(c, pos, yaw, pitch)) return;
    if (c.k < c.blend) {  // from the game's view it was at camera_set
        ++c.k;
        const float t = static_cast<float>(c.k) / static_cast<float>(c.blend);
        for (int l = 0; l < 3; ++l) pos[l] = c.from_pos[l] + (pos[l] - c.from_pos[l]) * t;
        yaw = sp_angle_lerp(c.from_yaw, yaw, t);
        pitch = sp_angle_lerp(c.from_pitch, pitch, t);
    }
    cam_write(pos, yaw, pitch);
    std::memcpy(c.last_pos, pos, sizeof pos);
    c.last_yaw = yaw;
    c.last_pitch = pitch;
    c.has_last = true;
}

// orig: (host) see setpieces.h
void setpieces_hud() {
    if (g_sp.f_alpha <= 0) return;
    // (to 513 x 449: gs_flat_rect's corners are a pixel in, and a fill reaching the frame's right edge is what a widened view
    // stretches to the window's: 512 stopped at the 4:3 frame's right side on an ultra-wide screen)
    gs_flat_rect(0, 0, 513, 449, static_cast<uint64_t>(static_cast<uint32_t>(std::min(g_sp.f_alpha, 0x80)) << 24 | (g_sp.f_rgb & 0xffffffu)), 0);
}

// ---- the SDK's services ----

// orig: (host) see setpieces.h
int32_t sp_rail_find(const char* name) {
    if (name == nullptr) return 0;
    for (size_t i = 0; i < g_sp.rails.size(); ++i)
        if (g_sp.rails[i].name == name) return static_cast<int32_t>(i + 1);
    return 0;
}

// orig: (host) see setpieces.h
int sp_hero_rail(rac1_rail_info* out) {
    if (!grinding()) return 0;
    rac1_rail_info v{};
    v.size = sizeof v;
    v.state = lw(STATE);
    v.rail = hero_rail_now();
    v.segment = lw(P + 0x564);
    v.speed = lwc1(P + 0x574);
    v.direction = lw(P + 0x578) < 0 ? -1 : 1;
    for (uint32_t l = 0; l < 3; ++l) v.pos[l] = lwc1(P + 0x500 + 4 * l);
    if (const Rail* r = rail_of(v.rail); r != nullptr) {
        v.distance = hero_distance(*r);
        v.length = r->length;
    }
    if (out != nullptr) {
        const uint32_t n = out->size == 0 ? static_cast<uint32_t>(sizeof v) : std::min<uint32_t>(out->size, sizeof v);
        std::memcpy(out, &v, n);
        out->size = n;
    }
    return 1;
}

// orig: (host) see setpieces.h
int sp_rail_point(int32_t rail, float distance, float out[3], float dir[3]) {
    const Rail* r = rail_of(rail);
    if (r == nullptr || !std::isfinite(distance)) return 0;
    point_at(*r, distance, out, dir);
    return 1;
}

// orig: (host) see setpieces.h
int sp_rail_enable(int32_t rail, int on) {
    Rail* r = rail_of(rail);
    if (r == nullptr) return 0;
    r->enabled = on != 0;
    rail_apply(*r);
    return 1;
}

// orig: (host) see setpieces.h
int sp_hero_rail_attach(int32_t rail, float distance, int32_t direction) {
    Rail* r = rail_of(rail);
    if (r == nullptr || rail_entry(*r) == 0 || lbu(OWNED + 0x1d) == 0 || lwu(P + 0x2080) == 0 || g_host.hero_place == nullptr || !std::isfinite(distance)) return 0;
    if (!r->enabled) {
        r->enabled = true;
        rail_apply(*r);
    }
    float at[3], dir[3];
    point_at(*r, distance, at, dir);
    if (direction < 0)
        for (float& d : dir) d = -d;
    // as if he had landed on it: put standing on its line, facing along it; the game's landing check (sub-state 0) latches the
    // rail under his feet and his state 0 takes it (0x28)
    g_host.hero_place(at, std::atan2(dir[1], dir[0]));
    g_sp.attach_rail = rail;
    g_sp.attach_frames = 0;
    g_sp.attach_dir = direction < 0 ? -1 : 1;
    g_sp.attach_at = {at[0], at[1], at[2]};
    if (trace()) std::printf("[setpiece] hero_rail_attach: Ratchet put on %s at %.1f (%.2f %.2f %.2f)\n", r->name.c_str(), distance, at[0], at[1], at[2]);
    return 1;
}

// orig: (host) see setpieces.h
int sp_rail_speed(int32_t rail, float scale) {
    Rail* r = rail_of(rail);
    if (r == nullptr || !std::isfinite(scale)) return 0;
    r->speed = std::clamp(scale, 0.25f, 4.0f);
    return 1;
}

// orig: (host) see setpieces.h
int sp_item_visit(int32_t item, int on) {
    if (item < 1 || item >= 0x25 || !g_sp.mod_level) return 0;
    const uint32_t a = OWNED + static_cast<uint32_t>(item);
    const auto it = std::find(g_sp.given.begin(), g_sp.given.end(), item);
    if (on != 0) {
        if (lbu(a) != 0) return it != g_sp.given.end() ? 1 : 0;
        give(item);
        return 1;
    }
    if (it == g_sp.given.end()) return 0;
    sb(a, 0);
    g_sp.given.erase(it);
    return 1;
}

// orig: (host) see setpieces.h
int sp_fade(int32_t out_frames, int32_t hold_frames, int32_t in_frames, uint32_t rgb) {
    g_sp.f_out = std::clamp(out_frames, 0, 600);
    g_sp.f_hold = std::clamp(hold_frames, 0, 3600);
    g_sp.f_in = std::clamp(in_frames, 0, 600);
    g_sp.f_rgb = rgb & 0xffffffu;
    g_sp.f_k = 0;
    g_sp.f_phase = 1;
    return 1;
}

// orig: (host) see setpieces.h
int sp_fade_active() { return g_sp.f_phase; }

// orig: (host) see setpieces.h
int sp_camera_set(const rac1_camera* cam) {
    if (cam == nullptr || cam->mode < RAC1_CAMERA_FIXED || cam->mode > RAC1_CAMERA_FOLLOW) return 0;
    Cam c;
    const uint32_t n = cam->size == 0 ? static_cast<uint32_t>(sizeof c.c) : std::min<uint32_t>(cam->size, sizeof c.c);
    std::memcpy(&c.c, cam, n);
    if (c.c.mode == RAC1_CAMERA_PATH) {
        if (g_host.path == nullptr || !g_host.path(c.c.path, c.path) || c.path.size() < 2) return 0;
        c.cum.push_back(0);
        for (size_t i = 1; i < c.path.size(); ++i) {
            float d = 0;
            for (size_t l = 0; l < 3; ++l) d += (c.path[i][l] - c.path[i - 1][l]) * (c.path[i][l] - c.path[i - 1][l]);
            c.cum.push_back(c.cum.back() + std::sqrt(d));
        }
    }
    // blended from the view now (the game's, or the plugin's camera before)
    for (uint32_t l = 0; l < 3; ++l) c.from_pos[l] = lwc1(OV(CAM_POS) + 4 * l);
    c.from_pitch = -lwc1(OV(CAM_ANGLES) + 4);
    c.from_yaw = lwc1(OV(CAM_ANGLES) + 8);
    c.blend = std::clamp(c.c.blend, 0, 600);
    c.k = 0;
    c.on = true;
    g_sp.cam = c;
    if (trace()) std::printf("[setpiece] camera_set: mode %d, blend %d\n", c.c.mode, c.blend);
    return 1;
}

// orig: (host) see setpieces.h
int sp_camera_release(int32_t blend) {
    if (!g_sp.cam.on) return 1;
    g_sp.cam.on = false;
    g_sp.cam.releasing = blend > 0;
    g_sp.cam.blend = std::clamp(blend, 0, 600);
    g_sp.cam.k = 0;
    if (trace()) std::printf("[setpiece] camera_release: blend %d\n", blend);
    return 1;
}

// orig: (host) see setpieces.h
int32_t sp_hazard_add(const rac1_hazard* h) {
    if (h == nullptr || (h->shape != RAC1_HAZARD_BOX && h->shape != RAC1_HAZARD_SPHERE) || h->damage <= 0) return 0;
    size_t i = 0;
    while (i < g_sp.hazards.size() && g_sp.hazards[i].live) ++i;
    if (i == g_sp.hazards.size()) {
        if (g_sp.hazards.size() >= kHazards) return 0;
        g_sp.hazards.emplace_back();
    }
    Hazard& z = g_sp.hazards[i];
    z = Hazard{};
    const uint32_t n = h->size == 0 ? static_cast<uint32_t>(sizeof z.h) : std::min<uint32_t>(h->size, sizeof z.h);
    std::memcpy(&z.h, h, n);
    if (z.h.shape == RAC1_HAZARD_BOX)
        for (int l = 0; l < 3; ++l)
            if (z.h.hi[l] < z.h.lo[l]) std::swap(z.h.lo[l], z.h.hi[l]);
    z.live = true;
    return static_cast<int32_t>(i + 1);
}

// orig: (host) see setpieces.h
int sp_hazard_set(int32_t hazard, int on) {
    if (hazard < 1 || static_cast<size_t>(hazard) > g_sp.hazards.size() || !g_sp.hazards[static_cast<size_t>(hazard) - 1].live) return 0;
    Hazard& z = g_sp.hazards[static_cast<size_t>(hazard) - 1];
    if (on < 0) z.live = false;
    else z.on = on != 0;
    return 1;
}

// orig: (host) see setpieces.h
int sp_rail_info(int32_t rail, char* name, int32_t cap, float* length, int32_t* section) {
    const Rail* r = rail_of(rail);
    if (r == nullptr) return 0;
    if (name != nullptr && cap > 0) std::snprintf(name, static_cast<size_t>(cap), "%s", r->name.c_str());
    if (length != nullptr) *length = r->length;
    if (section != nullptr) *section = r->section;
    return 1;
}

}  // namespace rac1
