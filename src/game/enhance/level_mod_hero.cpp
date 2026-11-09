// See level_mod_hero.h. Not a port: the generic build's lava and deadly liquid (hero_surface_update, 0x0020b960) on Kalebo III's
// hero code, with the states that code has.
#include <game/enhance/level_mod_hero.h>

#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <host/level_slots.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350;  // the player block

// the hero's feet within 1 of the height h (0.25 above them) and below it, coming down (hero_update.cpp's sinking_into)
bool sinking_into(float h) {
    if (!(std::fabs(h - (lwc1(P + 0x88) + 0.25f)) < 1.0f)) return false;
    return 0.0f < h - lwc1(P + 0x88);
}

int g_hops = 0;              // the hops in a row (the generic 0x68's +0x1660): within 3 seconds of the one before
uint32_t g_frames = 0, g_hop_at = 0;  // (this function's calls: one a frame of play)
bool g_lost = false;         // lost: until the respawn (+0x20b1 cleared)

bool trace() {
    static const bool t = std::getenv("RAC1_LEVEL_TRACE") != nullptr;
    return t;
}

// the hop out (the generic build's 0x69 / 0x3c, here the standing jump 7), or lost after three hops within 3 seconds (the generic
// lava's two bounces out of 0x68, then lost) or with no health
bool hop_or_lost(uint32_t hop) {
    if (g_hops > 0 && g_frames - g_hop_at < 20) return false;  // (the hop under way: its rise starts after its first frames)
    if (g_frames - g_hop_at > 180) g_hops = 0;  // (out of it for a while: a new run)
    if (lw(P + 0x22a8) > 0 && g_hops < 3) {
        ++g_hops;
        g_hop_at = g_frames;
        hero_mode3_exit();
        hero_set_state(7, 1);  // (the standing jump: Kalebo's 0x69 / 0x3c leave him held in place without the generic 0x68 before them)
        if (trace()) std::printf("[levels] Kalebo III's hero: surface %s, a hop out (%d in a row)\n", hop == 0x69 ? "lava" : "deadly liquid", g_hops);
        return true;
    }
    g_hops = 0;
    g_lost = true;
    if (trace()) std::printf("[levels] Kalebo III's hero: lost in the %s\n", hop == 0x69 ? "lava" : "deadly liquid");
    hero_lost();
    return false;
}
}  // namespace

// orig: (none) — enhancement (level_mod_hero.h)
bool level_mod_kalebo_surface(int32_t kind) {
    if (!level_slots_on() || level_slot_current() < 0) return false;  // (the original's Kalebo III: as transliterated)
    ++g_frames;
    if (g_lost) {
        if (lbu(P + 0x20b1) != 0) return false;  // (the respawn under way)
        g_lost = false;
    }
    if (kind == 3) sb(P + 0x12e6, 1);
    if (kind == 1) sb(P + 0x12e5, 1);
    if (lbu(P + 0x12e5) != 0 && lw(P + 0x300) != 0 && !(lw(P + 0x2084) == 0x3c && lh(P + 0x41e) == 0)) return hop_or_lost(0x3c);  // deadly liquid
    if (lbu(P + 0x12e6) != 0 && sinking_into(lwc1(P + 0x2f4)) && lwc1(P + 0x108) < 0.0f) return hop_or_lost(0x69);  // lava (coming down into it)
    return false;
}

// orig: (none) — enhancement (level_mod_hero.h)
bool level_mod_kalebo_lava_ground() {
    if (!level_slots_on() || level_slot_current() < 0) return false;
    return lh(P + 0x12e0) == 3;
}

}  // namespace rac1
