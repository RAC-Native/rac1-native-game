// Cut content brought back (see cut_content.h). Not a port: the behaviours here are reconstructed.
#include <game/enhance/cut_content.h>

#include <game/enhance/advanced.h>
#include <game/math/vecmath.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>

#include <cstdlib>
#include <cstring>
#include <string>

namespace rac1 {

namespace {
// orig: (none) — enhancement: RAC1_CUT_CONTENT's list holds `item` (-1: the variable unset, 0 no, 1 yes)
int cut_content_env(const char* item) {
    const char* e = std::getenv("RAC1_CUT_CONTENT");
    if (e == nullptr) return -1;
    const std::string list = std::string(",") + e + ",";
    return list.find(std::string(",") + item + ",") != std::string::npos ? 1 : 0;
}

// Kerwan's citizens: how near Ratchet must come for them to turn to him, and their turn (yaw_turn_damped's constants:
// the acceleration and the damping the game's turning mobys use, a slow turn at most)
constexpr float FRED_RANGE = 15.0f;
constexpr float FRED_ACCEL = 0.02f, FRED_DAMP = 0.3f, FRED_VMAX = 0.05f;
constexpr uint32_t FRED_KEPT = 1;  // the state byte (+0x20) of a citizen kept: 0 on its first update
constexpr uint32_t FRED_SPIN = 0x1bc;  // the turn's spin, in its pvars
// Their placements stand some of them in the air (never seen in the retail game: they retire at once): three (at x 314
// to 323, y 139 to 146) 0.25 above the ground, four more up to 0.1. At the first update each one is
// put on the ground under it: the collision met by a segment from FRED_SNAP_UP above its placement down to
// FRED_SNAP_DOWN below it (the game's own segment test, segment_collide: the level's triangles and the mobys that
// collide); with nothing there it stays where it was placed.
constexpr float FRED_SNAP_UP = 1.0f, FRED_SNAP_DOWN = 8.0f;

// orig: (none) — enhancement: a kept citizen put on the ground under its placement (above)
void fred_snap_to_ground(uint32_t m) {
    Vec4 from = V(m + 0x10), to = V(m + 0x10);
    from.z = from.z + FRED_SNAP_UP;
    to.z = to.z - FRED_SNAP_DOWN;
    if (segment_collide_v(from, to, 2, m, nullptr) != 0) swc1(m + 0x18, lwc1(OV(0x00173e60) + 8));  // (the hit point)
}
}  // namespace

// orig: (none) — enhancement (cut_content.h)
bool cut_content_on(const char* item) {
    if (g_advanced_disabled) return false;
    const int env = cut_content_env(item);
    if (env >= 0) return env != 0;
    return advanced_cut_content();
}

// orig: (none) — enhancement (cut_content.h)
void cut_content_fred_dispatch_l3(uint32_t m) {
    // (the choice is made at the citizen's first update, right after the level's load: the original retires it there)
    if (lbu(m + 0x20) == FRED_KEPT || cut_content_on("fred")) {
        cut_content_fred_update_l3(m);
        return;
    }
    fred_update_l3(m);
}

// orig: (none) — cut content restored (cut_content.h). The game's own stub measures what a citizen needs to face the hero
// - the distance to him (0x13f3d0) and the heading towards his moby (0x13ff80 +0x80) against its own (+0x48) - and then
// retires it; here it is kept: its one sequence (set 0) looped, and the heading turned towards Ratchet within FRED_RANGE.
// (That sequence is one frame long in the three models: the rest pose, the arms held out. No collision: Ratchet walks
// through them)
void cut_content_fred_update_l3(uint32_t m) {
    if (lbu(m + 0x20) != FRED_KEPT) {
        sb(m + 0x20, FRED_KEPT);
        fred_snap_to_ground(m);
        moby_anim_set(m, 0, 0, 0);
    } else if ((lbu(m + 0x70) & 2) != 0) {
        moby_anim_set(m, 0, 0, 0);  // (the sequence ended: again)
    }
    // the turn's spin: the last word of its 448 bytes of pvars (placement data no code of the game reads)
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    if (vec3_distance(V(m + 0x10), V(0x0013f3d0)) >= FRED_RANGE) {
        swc1(pv + FRED_SPIN, 0.0f);  // (no spin left over)
        return;
    }
    const float to = atan2_f(lwc1(0x0013ff80 + 0x80) - lwc1(m + 0x10), lwc1(0x0013ff80 + 0x84) - lwc1(m + 0x14));
    yaw_turn_damped(m, pv + FRED_SPIN, to, FRED_ACCEL, FRED_DAMP, FRED_VMAX * lwc1(0x0015ed60));
}

}  // namespace rac1
