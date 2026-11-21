// Oracle batch 531 (#130 / #168: "Ratchet cannot shimmy along Aridia's ledges"): the Novalis family's hang / shimmy path on
// the level's own ledges. Run with ORACLE_LEVEL=L build/tests/ovl_oracle --batch 531, L = 1 (Novalis) or 2 (Aridia); on the
// other levels its checks are skipped (on Veldin, from the level start as this batch lays it out, the hero did not take
// the ledges found: every case played its 20 attempts of 60 frames; not looked into). No RAM dumps.
// The ledges are found on the level's own collision: up to three surfaces a cell probed from the top down on a 1-unit grid
// over the level's mobys' extent (a ledge under an overhang too), every drop of more than 2 units from a surface to its
// neighbour cell's a candidate, the edge found by halving; a candidate is kept when the port's ledge grab, from a little out
// of the edge, finds a hang point and the port's ledge check holds there. On Aridia the construction site's ledge of the
// reports (the tower's west face at x 213.55, its striped band 16.1 high under the floor above, from the corner pillar at
// y 279.5 to y 272) is always among them. The hero is then put 0.6 above a hang point and falls onto the ledge (the game's own
// grab, states 0x18 -> 0x19); frames of the level are played (the port's) with the stick held in one of 16 directions at one
// of four strengths (or let go), the cross and L1 / R1 now and then; the frame compared is then
//  - hero_frame_update 0x2070d0 (Novalis 0x228870, Aridia 0x2179c8): the whole hero update, hanging or shimmying (the shimmy
//    against an obstacle or a ledge's end, back to hanging, the climb, the fall, the jump away);
//  - hero_input_update 0x229b70 (0x242930 / 0x230988), hero_state_update 0x217970 (0x2370b8 / 0x225c20): its two halves;
//  - hero_ledge_check 0x20c758 (0x22d838 / 0x21c228) from the hero's place and from 0.3 to either side (the shimmy's
//    probe), with the facing and the reference yaw varied;
//  - hero_ledge_grab_check 0x20bed0 (0x22d090 / 0x21ba80) in the shimmy states from the same places (the shimmy's update
//    runs it at the hero's place and 0.3 to the side).
// B531_N=n: n cases a check (default 200); B531_MAX=n: at most n ledges (default 400, spread over the level);
// B531_INFO=1: the ledges found and what the stick does at each (the shimmies held and the travel along the wall);
// B531_AT=x,y,z: only the ledges within 3 of that point; B531_LEDGES=x,y,z,wall;...: these hang points (wall: the wall's
// outward yaw) instead of the search; B531_CACHE=<file>: the ledges read from that file, or found and written to it.

#include "ovl_oracle.h"

#include <game/ovl/arrival.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/disc.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {

constexpr uint32_t P = g::PLAYER;

bool info() { return std::getenv("B531_INFO") != nullptr; }

// 989snd's IOP server with the stream system's disc reads (batch 91's)
void snd_server(bool reads) {
    auto result = [](uint32_t id, uint32_t a0) -> uint32_t {
        const uint32_t h = (id * 0x9e3779b1u) ^ (a0 * 0x85ebca6bu);
        return ((h >> 7) % 4 == 0) ? 0u : ((h >> 3) | 1u);
    };
    sif_rpc_register(0x123456, [result, reads](uint32_t fno, uint32_t send, uint32_t, uint32_t recv, uint32_t) {
        if (fno == 0x4d) {
            const uint32_t n = ee_read<uint32_t>(send);
            uint32_t p = send + 4;
            for (uint32_t i = 0; i < n; ++i) {
                const uint32_t id = ee_read<uint16_t>(p), size = ee_read<uint16_t>(p + 2);
                ee_write<uint32_t>(recv + 4 + i * 4, result(id, size != 0 ? ee_read<uint32_t>(p + 4) : 0u));
                if (reads && id == 0x38) {
                    disc_read_ee(ee_read<uint32_t>(p + 0xc), ee_read<uint32_t>(p + 4), ee_read<uint32_t>(p + 8));
                    ee_write<uint32_t>(0x00137b10, 0);
                    ee_write<uint32_t>(0x00137b00, 0);
                }
                p += 4 + ((size + 3) & ~3u);
            }
            ee_write<uint32_t>(recv, 0xffffffffu);
            ee_write<uint32_t>(recv + 4 + n * 4, 0xffffffffu);
            return;
        }
        ee_write<uint32_t>(recv + 4, result(fno, send != 0 ? ee_read<uint32_t>(send) : 0u));
        ee_write<uint32_t>(recv, 0xffffffffu);
        ee_write<uint32_t>(recv + 8, 0xffffffffu);
    });
}

// the level after its start (batch 91's)
std::vector<uint8_t> g_snap;
void level() {
    uint8_t* ram = ee_get_ram_base();
    if (g_snap.empty()) {
        veldin_boot();
        level_setup_body(1, 0);
        w32(OV(0x0016c060), 0xf);
        w32(0x0015ec8c, 1);
        w32(0x00137b00, 0);
        sub_00264bf8();
        g_snap.assign(ram, ram + 0x02000000);
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); ee_read<uint8_t>(m + 0x20) != 0xff; m += 0x100) {
            const uint32_t fn = ee_read<uint32_t>(m + 0x74);
            if (fn != 0 && !ovl_moby_fn_ported(fn)) g_extra_noop_hooks.push_back(fn);
        }
        for (uint32_t fn : {OVF(0x002a84c8u), OVF(0x002d89f0u)}) if (!ovl_moby_fn_ported(fn)) g_extra_noop_hooks.push_back(fn);
    } else {
        std::memcpy(ram, g_snap.data(), g_snap.size());
    }
}

// a pad packet: held buttons, the left stick (0x7f the middle; ly 0 = forward)
void pad(uint16_t held, uint8_t lx, uint8_t ly) {
    const uint16_t low = static_cast<uint16_t>(~held);
    const uint8_t pk[6] = {static_cast<uint8_t>(low >> 8), static_cast<uint8_t>(low & 0xff), 0x7f, 0x7f, lx, ly};
    pad_read_packet(0x0013c940, pk, 6);
}

void frame() {
    gameplay_frame();
    w32(OV(0x0015f3f8), ee_read<uint32_t>(OV(0x0015f3f8)) + 1);
}

struct Ledge {
    Vec4 hang;          // the grab's hang point (+0x4d0)
    float wall = 0.0f;  // the wall's outward yaw (+0x4e4)
    float top = 0.0f;   // the edge's height (+0x4e0)
};
std::vector<Ledge> g_ledges;

float top_at(float x, float y, float z) { return floor_probe_v(0.0f, Vec4{x, y, z, 1.0f}, 0); }

// The port's grab from (x, y, z) facing `yaw` in the shimmy state: the hang point on a ledge, or false.
bool grab_at(const Vec4& at, float yaw, Ledge& out) {
    V(P + 0x80) = at;
    swc1(P + 0x98, yaw);
    vec_clear(V(P + 0x110));
    sw(P + 0x2084, 0x1a);
    sw(P + 0x208c, 3);
    sh(P + 0x1b0, 0);
    sh(P + 0x308, 0);
    sb(P + 0x20a4, 0);
    hero_ledge_grab_check();
    if (lw(P + 0x4e8) == 0) return false;
    out.hang = V(P + 0x4d0);
    out.wall = lwc1(P + 0x4e4);
    out.top = lwc1(P + 0x4e0);
    return true;
}

// the extent of the level's mobys, 40 around (the highest few left out of the top: a level may keep a moby far above
// everything), the height the probes start from
struct Extent {
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0, zs = 0;
};
bool level_extent(Extent& e) {
    std::vector<float> xs, ys, zs;
    for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
        if (ee_read<uint8_t>(m + 0x20) >= 0xfe) continue;
        const Vec4 p = ee_ref<Vec4>(m + 0x10);
        if (!(std::abs(p.x) < 5000.0f && std::abs(p.y) < 5000.0f && std::abs(p.z) < 5000.0f)) continue;
        xs.push_back(p.x); ys.push_back(p.y); zs.push_back(p.z);
    }
    if (xs.empty()) return false;
    auto pct = [](std::vector<float> v, float q) { std::sort(v.begin(), v.end()); return v[static_cast<size_t>(q * static_cast<float>(v.size() - 1))]; };
    e.x0 = std::max(pct(xs, 0.0f) - 40.0f, 1.0f);
    e.y0 = std::max(pct(ys, 0.0f) - 40.0f, 1.0f);
    e.x1 = pct(xs, 1.0f) + 40.0f;
    e.y1 = pct(ys, 1.0f) + 40.0f;
    e.zs = pct(zs, 0.98f) + 40.0f;
    return true;
}

// Tops of the surface types the ledge checks pass over (0 for the grab, 9 and 12 for both; sub_001f35d8 of the probe's
// hit): on a 2-unit grid over the level, up to 300 of each
struct Typed {
    float x, y, z;
};
std::vector<Typed> g_typed;
bool g_typed_done = false;
void find_typed() {
    if (g_typed_done) return;
    g_typed_done = true;
    level();
    Extent e;
    if (!level_extent(e)) return;
    int n0 = 0, n9 = 0, n12 = 0;
    for (float y = e.y0; y <= e.y1; y += 2.0f)
        for (float x = e.x0; x <= e.x1; x += 2.0f) {
            const float t = top_at(x, y, e.zs);
            if (t <= 0.5f) continue;
            const int32_t ty = sub_001f35d8();
            int& n = ty == 0 ? n0 : ty == 9 ? n9 : n12;
            if ((ty == 0 || ty == 9 || ty == 0xc) && n < 300) {
                ++n;
                g_typed.push_back(Typed{x, y, t});
            }
        }
    std::printf("  (surfaces of type 0 / 9 / 12 found: %d / %d / %d)\n", n0, n9, n12);
}
// the hero put before one of those tops, facing it, its top 1.15 .. 1.65 above (false when the level has none)
bool before_typed() {
    if (g_typed.empty()) return false;
    const Typed& t = g_typed[static_cast<size_t>(ri(0, static_cast<int>(g_typed.size()) - 1))];
    const float yaw = rf(-3.1f, 3.1f), r = lwc1(P + 0x234) + rf(0.0f, 0.3f);
    swc1(P + 0x80, t.x - std::cos(yaw) * r);
    swc1(P + 0x84, t.y - std::sin(yaw) * r);
    swc1(P + 0x88, t.z - rf(1.15f, 1.65f));
    swc1(P + 0x98, yaw);
    swc1(P + 0x2d8, t.z - rf(2.0f, 4.0f));
    return true;
}

void find_ledges() {
    if (!g_ledges.empty()) return;
    // B531_CACHE=<file>: the ledges read from the file when it exists, else found and written to it (for repeated runs)
    const char* cache = std::getenv("B531_CACHE");
    if (cache != nullptr) {
        if (std::FILE* f = std::fopen(cache, "r"); f != nullptr) {
            Ledge L;
            while (std::fscanf(f, "%f %f %f %f %f %f", &L.hang.x, &L.hang.y, &L.hang.z, &L.hang.w, &L.wall, &L.top) == 6) g_ledges.push_back(L);
            std::fclose(f);
            std::printf("  (%zu ledges from %s)\n", g_ledges.size(), cache);
            return;
        }
    }
    // B531_LEDGES=x,y,z,wall;...: these hang points only
    if (const char* ls = std::getenv("B531_LEDGES"); ls != nullptr) {
        for (const char* q = ls; q != nullptr && *q != 0;) {
            Ledge L;
            if (std::sscanf(q, "%f,%f,%f,%f", &L.hang.x, &L.hang.y, &L.hang.z, &L.wall) != 4) break;
            L.hang.w = 0.0f;
            L.top = L.hang.z + 1.43f;
            g_ledges.push_back(L);
            q = std::strchr(q, ';');
            if (q != nullptr) ++q;
        }
        std::printf("  (%zu ledges given)\n", g_ledges.size());
        return;
    }
    level();
    Extent e;
    if (!level_extent(e)) return;
    const float x0 = e.x0, y0 = e.y0, x1 = e.x1, y1 = e.y1, zs = e.zs;
    const int nx = static_cast<int>(x1 - x0) + 1, ny = static_cast<int>(y1 - y0) + 1;
    // up to three surfaces a cell, from the top down (a ledge under an overhang too: the next probe starts 1.2 below the
    // surface found)
    constexpr int NL = 3;
    std::vector<float> h(static_cast<size_t>(nx) * static_cast<size_t>(ny) * NL, 0.0f);
    auto layer = [&](int i, int j, int l) -> float& { return h[(static_cast<size_t>(j) * nx + static_cast<size_t>(i)) * NL + static_cast<size_t>(l)]; };
    for (int j = 0; j < ny; ++j)
        for (int i = 0; i < nx; ++i) {
            float from = zs;
            for (int l = 0; l < NL; ++l) {
                const float t = top_at(x0 + static_cast<float>(i), y0 + static_cast<float>(j), from);
                if (t <= 0.5f) break;
                layer(i, j, l) = t;
                from = t - 1.2f;
                if (from <= 1.0f) break;
            }
        }
    const float radius = lwc1(P + 0x234);
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    const size_t cap = std::getenv("B531_MAX") != nullptr ? static_cast<size_t>(std::atoi(std::getenv("B531_MAX"))) : 400u;
    int candidates = 0;
    std::vector<Ledge> all;
    for (int j = 1; j + 1 < ny; ++j)
        for (int i = 1; i + 1 < nx; ++i)
            for (int l = 0; l < NL; ++l) {
                const float t = layer(i, j, l);
                if (t <= 0.5f) break;
                for (int d = 0; d < 4; ++d) {
                    // the neighbour's highest surface below this one's height + 0.5
                    float b = 0.0f;
                    for (int m = 0; m < NL; ++m) {
                        const float v = layer(i + dx[d], j + dy[d], m);
                        if (v > 0.5f && v < t + 0.5f) { b = v; break; }
                    }
                    if (!(t - b > 2.0f)) continue;
                    ++candidates;
                    // the edge between the two cells, by halving (probing from just above this surface)
                    float lo = 0.0f, hi = 1.0f;
                    const float x = x0 + static_cast<float>(i), y = y0 + static_cast<float>(j);
                    for (int k = 0; k < 10; ++k) {
                        const float mid = (lo + hi) * 0.5f;
                        if (top_at(x + dx[d] * mid, y + dy[d] * mid, t + 0.5f) > t - 1.0f) lo = mid; else hi = mid;
                    }
                    const float ex = x + dx[d] * lo, ey = y + dy[d] * lo;
                    const float yaw = std::atan2(static_cast<float>(-dy[d]), static_cast<float>(-dx[d]));
                    // falling past the edge, a little out from it, the top 1.4 above
                    const Vec4 at{ex + dx[d] * (radius + 0.25f), ey + dy[d] * (radius + 0.25f), t - 1.4f, 1.0f};
                    level();
                    swc1(P + 0x2d8, b);
                    Ledge L;
                    if (!grab_at(at, yaw, L)) continue;
                    // the hang point itself must hold (the hang state's own check)
                    const float ref = lwc1(P + 0x98);
                    if (hero_ledge_check_v(L.hang, &ref) == 0) continue;
                    bool near = false;
                    for (const Ledge& o : all)
                        if (std::abs(o.hang.x - L.hang.x) + std::abs(o.hang.y - L.hang.y) + std::abs(o.hang.z - L.hang.z) < 1.5f) { near = true; break; }
                    if (!near) all.push_back(L);
                }
            }
    // B531_AT=x,y,z: only the ledges within 3 of that point
    if (const char* at = std::getenv("B531_AT"); at != nullptr) {
        float px = 0, py = 0, pz = 0;
        if (std::sscanf(at, "%f,%f,%f", &px, &py, &pz) == 3) {
            std::vector<Ledge> v;
            for (const Ledge& L : all)
                if (std::abs(L.hang.x - px) + std::abs(L.hang.y - py) + std::abs(L.hang.z - pz) < 3.0f) v.push_back(L);
            all.swap(v);
        }
    }
    // spread over the level: every k-th one when there are more than the cap; on Aridia the construction site's ledge of
    // the reports (#130, #168: the tower's west face, its band 16.1 high under the floor above, from its corner pillar at
    // y 279.5 to y 272) always kept
    if (all.size() > cap) {
        std::vector<Ledge> v, keep;
        for (const Ledge& L : all)
            if (g_oracle_level == 2 && std::abs(L.hang.x - 213.55f) < 0.5f && L.hang.y > 271.0f && L.hang.y < 280.5f && std::abs(L.hang.z - 14.66f) < 0.5f)
                keep.push_back(L);
        for (size_t k = 0; k + keep.size() < cap; ++k) v.push_back(all[k * all.size() / (cap - keep.size())]);
        v.insert(v.end(), keep.begin(), keep.end());
        all.swap(v);
    }
    g_ledges = all;
    if (cache != nullptr) {
        if (std::FILE* f = std::fopen(cache, "w"); f != nullptr) {
            for (const Ledge& L : all) std::fprintf(f, "%.9g %.9g %.9g %.9g %.9g %.9g\n", L.hang.x, L.hang.y, L.hang.z, L.hang.w, L.wall, L.top);
            std::fclose(f);
        }
    }
    std::printf("  (the level's extent %.0f..%.0f x %.0f..%.0f, top %.0f; %d drops, %zu ledges kept; the hero's radius %.3f)\n", x0, x1, y0,
                y1, zs, candidates, g_ledges.size(), radius);
    if (info())
        for (const Ledge& L : g_ledges)
            std::printf("    ledge %.2f %.2f %.2f wall %.3f top %.2f\n", L.hang.x, L.hang.y, L.hang.z, L.wall, L.top);
}

// The hero put 0.6 above a ledge's hang point, facing the wall, and left to fall onto it (the game's grab): true once
// hanging (state 0x19).
bool hang_on(const Ledge& L) {
    level();
    const uint32_t buf = OUT + 0x100;
    const float yaw = angle_add(L.wall, u2f(0x40490fdbu));
    wf(buf, L.hang.x); wf(buf + 4, L.hang.y); wf(buf + 8, L.hang.z + 0.6f); wf(buf + 0xc, 0.0f);
    w32(buf + 0x10, 0); w32(buf + 0x14, 0); wf(buf + 0x18, yaw); w32(buf + 0x1c, 0);
    hero_place(buf, buf + 0x10, -1, 1);
    for (int f = 0; f < 60; ++f) {
        pad(0, 0x7f, 0x7f);
        frame();
        if (lw(P + 0x2084) == 0x19) return true;
    }
    return false;
}

// a stick direction (16) and strength (four) as a pad byte pair
void stick_bytes(int dir, int mag, uint8_t& lx, uint8_t& ly) {
    static const float s[4] = {0.45f, 0.62f, 0.85f, 1.0f};
    const float a = static_cast<float>(dir) * 0.39269908f;
    lx = static_cast<uint8_t>(std::clamp(127.5f + std::cos(a) * s[mag] * 127.5f, 0.0f, 255.0f));
    ly = static_cast<uint8_t>(std::clamp(127.5f + std::sin(a) * s[mag] * 127.5f, 0.0f, 255.0f));
}

// what the stick does at each ledge (B531_INFO): for 8 stick directions at full strength, 90 frames each from the hanging
// state: "<direction>:<L|R|-><frames>/<travel>": the shimmy to the hero's left (0x1a) or right (0x1b) last held, the frames
// spent shimmying, the travel along the wall (positive to the hero's right)
void survey() {
    int hung = 0, stuck = 0;
    for (size_t k = 0; k < g_ledges.size(); ++k) {
        const Ledge& L = g_ledges[k];
        if (!hang_on(L)) {
            std::printf("    ledge %zu (%.2f %.2f %.2f): no hang\n", k, L.hang.x, L.hang.y, L.hang.z);
            continue;
        }
        ++hung;
        std::vector<uint8_t> hanging(ee_get_ram_base(), ee_get_ram_base() + 0x02000000);
        const float tx = -std::sin(L.wall), ty = std::cos(L.wall);  // along the wall
        std::string line;
        bool any_shimmy_no_move = false;
        for (int d = 0; d < 16; d += 2) {
            std::memcpy(ee_get_ram_base(), hanging.data(), hanging.size());
            const Vec4 p0 = V(P + 0x80);
            uint8_t lx, ly;
            stick_bytes(d, 3, lx, ly);
            int sh_frames = 0;
            uint32_t last = 0;
            for (int f = 0; f < 90; ++f) {
                pad(0, lx, ly);
                frame();
                const uint32_t st = lwu(P + 0x2084);
                if (st == 0x1a || st == 0x1b) { ++sh_frames; last = st; }
            }
            const Vec4 p1 = V(P + 0x80);
            const float along = (p1.x - p0.x) * tx + (p1.y - p0.y) * ty;
            char buf[64];
            std::snprintf(buf, sizeof buf, " %d:%s%d/%+.2f", d, last == 0x1a ? "L" : last == 0x1b ? "R" : "-", sh_frames, along);
            line += buf;
            // (held for 60 frames or more and less than a third of a free run's travel: against an end or an obstacle)
            if (sh_frames >= 60 && std::abs(along) < 0.6f) any_shimmy_no_move = true;
        }
        if (any_shimmy_no_move) ++stuck;
        std::printf("    ledge %zu (%.2f %.2f %.2f wall %.2f)%s%s\n", k, L.hang.x, L.hang.y, L.hang.z, L.wall, line.c_str(), any_shimmy_no_move ? "  BLOCKED" : "");
    }
    std::printf("  (survey: %d of %zu ledges hung on; %d where a held shimmy is blocked)\n", hung, g_ledges.size(), stuck);
}

// a hanging hero after some frames of play with the stick held: the state the compared frame starts from; the pad of the
// compared frame set. False when the ledge could not be hung on.
bool hang_and_play() {
    const Ledge& L = g_ledges[static_cast<size_t>(ri(0, static_cast<int>(g_ledges.size()) - 1))];
    if (!hang_on(L)) return false;
    if (ri(0, 9) == 0) {  // (just hanging, the grab's animation 0x20 still the moby's; its end flagged (+0xa98 bit 1) as the
        // frame it ends has it: the hang's own animation 0x21 starts)
        pad(0, 0x7f, 0x7f);
        if (coin()) sw(P + 0xa98, lwu(P + 0xa98) | 2u);
        return true;
    }
    if (ri(0, 4) == 0) {  // (hanging still, then a push: the move input +0x1d20 and its tilt +0x229c taken in as the hero's
        // tick 0x211670 has them when the state's input runs: the compared input / state update starts the shimmy, or not at a
        // ledge's end)
        const int n = ri(0, 25);
        for (int f = 0; f < n; ++f) {
            pad(0, 0x7f, 0x7f);
            frame();
        }
        uint8_t lx, ly;
        stick_bytes(coin() ? (coin() ? 0 : 8) : ri(0, 15), ri(0, 3), lx, ly);
        pad(ri(0, 9) == 0 ? 0x0040 : 0, lx, ly);
        swc1(P + 0x1d20, lwc1(0x0013c940 + 0x108));
        swc1(P + 0x1d24, lwc1(0x0013c940 + 0x10c));
        swc1(P + 0x229c, sub_00211830());  // (the tilt of the move input: as after a frame of the push, before its input)
        return true;
    }
    uint8_t lx, ly;
    stick_bytes(ri(0, 15), ri(0, 3), lx, ly);
    if (ri(0, 7) == 0) lx = ly = 0x7f;  // (the stick let go)
    // (0 .. 15 frames a quarter of the time, 10 .. 20 a sixth (the grab's animation ends there: the hang's own starts), else
    // up to 70)
    const int nk = ri(0, 11);
    const int n = nk < 3 ? ri(0, 15) : nk < 5 ? ri(10, 20) : ri(0, 70);
    const int xp = ri(0, 5) == 0 ? ri(0, n + 2) : -100;  // (a cross press during the frames played)
    // half the time a stick of its own for the last 0 .. 2 frames played and the compared one (a push from hanging still, a
    // turn during a shimmy: the hero reads the stick's tilt +0x229c of the frame before)
    uint8_t lx2 = lx, ly2 = ly;
    const bool fresh = coin();
    if (fresh) {
        // (a quarter of them back, away from the wall with the camera behind the hero: the jump away; a third to the side)
        const int dk = ri(0, 11);
        stick_bytes(dk < 3 ? ri(3, 5) : dk < 7 ? (coin() ? 0 : 8) : ri(0, 15), ri(0, 3), lx2, ly2);
        if (ri(0, 7) == 0) lx2 = ly2 = 0x7f;
    }
    const int sw_at = coin() ? n - 1 : n - ri(0, 2);  // (n - 1: the frame whose stick the compared one reads)
    for (int f = 0; f < n; ++f) {
        const uint16_t held = (f >= xp && f < xp + 3) ? 0x0040 : (ri(0, 60) == 0 ? 0x000a : 0);
        if (f >= sw_at) pad(held, lx2, ly2); else pad(held, lx, ly);
        frame();
    }
    // the compared frame's pad: the cross pressed now (the climb, or the jump away with the stick back) or L1 / R1 (the fall)
    // now and then
    const int k = ri(0, 9);
    pad(k < 2 ? 0x0040 : k == 2 ? (coin() ? 0x0002 : 0x0008) : 0, lx2, ly2);
    // now and then the hero moved off the hang point (the ledge gone: the fall)
    if (ri(0, 11) == 0) {
        swc1(P + 0x80, lwc1(P + 0x80) + rf(-0.8f, 0.8f));
        swc1(P + 0x84, lwc1(P + 0x84) + rf(-0.8f, 0.8f));
        swc1(P + 0x88, lwc1(P + 0x88) + rf(-0.6f, 0.6f));
    }
    return true;
}

}  // namespace

void batch_531() {
    if (g_only_batch != 531) return;
    g_batch = 531;
    if (g_oracle_level != 1 && g_oracle_level != 2) {
        skip_check("hero_frame_update/hang", 0x002070d0, "Novalis (1) and Aridia (2) only");
        skip_check("hero_input_update/hang", 0x00229b70, "Novalis (1) and Aridia (2) only");
        skip_check("hero_state_update/hang", 0x00217970, "Novalis (1) and Aridia (2) only");
        skip_check("hero_ledge_check/level", 0x0020c758, "Novalis (1) and Aridia (2) only");
        skip_check("hero_ledge_grab_check/shimmy", 0x0020bed0, "Novalis (1) and Aridia (2) only");
        return;
    }
    g_ignored_words = vcross_lanes();
    g_extra_noop_hooks = {OVF(0x001f3aa0), OVF(0x001f4490)};  // (the screens, ported: task 0301)
    g_ignored_words.push_back(OV(0x00160188));                 // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;
    snd_server(true);
    find_ledges();
    const int ncase = std::getenv("B531_N") != nullptr ? std::atoi(std::getenv("B531_N")) : 200;
    if (g_ledges.empty()) {
        skip_check("hero_ledge_check/level", 0x0020c758, "no ledge found on the level");
    } else {
        if (info()) survey();
        auto played = [] {
            for (int t = 0; t < 20; ++t)
                if (hang_and_play()) return Args{};
            return Args{};
        };
        check("hero_frame_update/hang", 0x002070d0, ncase, Ret::None, played, [](A) { hero_frame_update(); return uint64_t{0}; }, OV(0x00173e7c));
        check("hero_input_update/hang", 0x00229b70, ncase, Ret::None, played, [](A) { hero_input_update(); return uint64_t{0}; }, OV(0x00173e7c));
        check("hero_state_update/hang", 0x00217970, ncase, Ret::None, played, [](A) { hero_state_update(); return uint64_t{0}; }, OV(0x00173e7c));
        // the ledge check from the hero's place or 0.3 to either side, the facing and the reference yaw varied
        find_typed();
        check("hero_ledge_check/level", 0x0020c758, ncase, Ret::Int32, [played] {
            played();
            if (ri(0, 3) == 0) before_typed();  // (a quarter of the time before a top of type 0 / 9 / 12)
            Vec4 at = V(P + 0x80);
            const int side = ri(0, 2);
            const float yaw = lwc1(P + 0x98) + (ri(0, 2) == 0 ? rf(-0.6f, 0.6f) : 0.0f);
            swc1(P + 0x98, yaw);
            if (side != 0) {
                const float a = angle_add(yaw, side == 1 ? u2f(0x3fc90fdbu) : u2f(0xbfc90fdbu));
                at.x = at.x + cos_vu(a) * u2f(0x3e99999au);
                at.y = at.y + sin_vu(a) * u2f(0x3e99999au);
            }
            if (ri(0, 3) == 0) at.z = at.z + rf(-0.4f, 0.4f);
            ee_ref<Vec4>(OUT) = at;
            wf(OUT + 0x20, ri(0, 3) == 0 ? yaw + rf(-1.2f, 1.2f) : yaw);
            if (ri(0, 3) == 0) swc1(P + 0x2d8, at.z + rf(-1.0f, 1.5f));  // (the ground close below the edge: no ledge)
            return Args{{OUT, OUT + 0x20}, {}};
        }, [](A a) { return ri32(hero_ledge_check(a.i[0], hp<float>(a.i[1]))); }, OV(0x00173e7c));
        // the grab in the shimmy states from the hero's place or 0.3 to the side (as the shimmy's update runs it), or beside
        // a moby. Left out of this check only: the segment record 0x173e90 .. 0x173e9c, which segment_collide leaves from VU0
        // registers in the original when a line hits a moby (the known difference of the port, COMMON.md / batch 520's
        // segment_words; nothing reads it): met here on the grab's probe of a moby's top
        const std::vector<uint32_t> saved_ignored = g_ignored_words;
        for (uint32_t k = 0; k < 0x10; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
        check("hero_ledge_grab_check/shimmy", 0x0020bed0, ncase, Ret::None, [played] {
            played();
            const int side = ri(0, 2);
            sw(P + 0x2084, side == 2 ? 0x1bu : 0x1au);
            if (ri(0, 4) == 0) before_typed();  // (before a top of type 0 / 9 / 12)
            if (ri(0, 4) == 0) {  // (beside one of the level's mobys with collision (a crate, a platform), its top 0.5 .. 2.5 up: a
                // moby's top, grabbable or not by its pvars)
                std::vector<uint32_t> ms;
                for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                    if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x94) != 0 && m != lwu(P + 0x2080)) ms.push_back(m);
                if (!ms.empty()) {
                    const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                    const float a = rf(-3.1f, 3.1f), r = rf(0.6f, 2.0f);
                    swc1(P + 0x80, lwc1(m + 0x10) + std::cos(a) * r);
                    swc1(P + 0x84, lwc1(m + 0x14) + std::sin(a) * r);
                    swc1(P + 0x88, lwc1(m + 0x18) + rf(-2.5f, 0.5f));
                    swc1(P + 0x98, angle_add(a, u2f(0x40490fdbu)));
                }
            }
            if (ri(0, 5) == 0) {  // (the states the grab leaves alone: 0xe early (+0xaa8 below 37), another one)
                sw(P + 0x2084, coin() ? 0xeu : 0x19u);
                swc1(P + 0xaa8, rf(30.0f, 45.0f));
            }
            if (side != 0) {
                const float a = angle_add(lwc1(P + 0x98), side == 1 ? u2f(0x3fc90fdbu) : u2f(0xbfc90fdbu));
                swc1(P + 0x80, lwc1(P + 0x80) + cos_vu(a) * u2f(0x3e99999au));
                swc1(P + 0x84, lwc1(P + 0x84) + sin_vu(a) * u2f(0x3e99999au));
            }
            return Args{};
        }, [](A) { hero_ledge_grab_check(); return uint64_t{0}; }, OV(0x00173e7c));
        g_ignored_words = saved_ignored;
    }
    snd_server(false);
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
