// Controller vibration (see vibration.h). Not a port: everything here is new, reading the game's state only.
#include <game/enhance/vibration.h>

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

#include <algorithm>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the player block (g::PLAYER)

struct Pulse {
    float low = 0.0f, high = 0.0f;  // the motors at its start
    int32_t left = 0, frames = 0;   // frames left of its fade, of its length
};
constexpr int kPulses = 12;

struct Seen {  // the game's values the last gameplay frame (valid: read once since the level / the setting started)
    bool valid = false;
    int32_t nanotech = 0, hits = 0, state = 0, bolts = 0, shake_frames = 0, ammo = 0;
    uint32_t item = 0, level = 0;
    bool lost = false, wrench_hit = false;
    float z = 0.0f, vz = 0.0f;
    int32_t bolt_wait = 0;
};

struct Vibe {
    Pulse pulses[kPulses];
    Seen seen;
    float low = 0.0f, high = 0.0f;  // this gameplay frame's motors (taken by the host)
    bool fresh = false;             // a gameplay frame ran since the host's last take
} g_vibe;

// orig: (none) — enhancement: a pulse started (replacing the weakest when all are in use); its motors fade to 0 over
// `frames` game frames
void pulse(float low, float high, int32_t frames) {
    Pulse* slot = &g_vibe.pulses[0];
    float weakest = 2.0f;
    for (Pulse& p : g_vibe.pulses) {
        if (p.left <= 0) {
            slot = &p;
            break;
        }
        const float s = std::max(p.low, p.high) * static_cast<float>(p.left) / static_cast<float>(p.frames);
        if (s < weakest) {
            weakest = s;
            slot = &p;
        }
    }
    *slot = Pulse{std::min(low, 1.0f), std::min(high, 1.0f), frames, frames};
}

// orig: (none) — enhancement: the hero's airborne states (the jump family of jump_update 0x217970, as the strafe's)
bool airborne(int32_t st) {
    return st == 7 || (st >= 9 && st <= 0x12) || st == 0x1c || st == 0x3c || st == 0x49 || st == 0x4c || st == 0x5e || st == 0x69;
}

// orig: (none) — enhancement: a weapon's shot by its item (vibration.h): the motors and frames
void shot_pulse(uint32_t item) {
    switch (item) {
        case 0x0f:  // Blaster
        case 0x10:  // Pyrocitor
        case 0x13:  // Tesla Claw
            pulse(0.0f, 0.35f, 4);
            return;
        case 0x0b:  // Devastator
        case 0x17:  // R.Y.N.O.
            pulse(0.8f, 0.6f, 16);
            return;
        default:  // the gloves (Bomb, Mine, Doom, Decoy), the Suck Cannon, the Visibomb, the Morph-o-Ray, ...
            pulse(0.3f, 0.4f, 8);
            return;
    }
}

// orig: (none) — enhancement: the events of this gameplay frame against the last one's
void events(Seen& s) {
    const int32_t nanotech = lw(HERO + 0x22a8), hits = lw(0x0015eea8), st = lw(HERO + 0x2084), bolts = lw(0x0015ed98);
    const int32_t shake_frames = lw(OV(0x00166de8));
    const uint32_t item = lwu(HERO + 0x10b8), level = lwu(0x0015ed84);
    const int32_t ammo = item < 0x40 ? lw(0x0013d428 + item * 4) : 0;
    const bool lost = lbu(HERO + 0x20b1) != 0, wrench_hit = lw(HERO + 0xa64) != 0;
    const float z = lwc1(HERO + 0x88);
    const bool ship = lbu(HERO + 0x20a4) == 2;
    if (s.valid && s.level == level) {
        // damage
        if (nanotech < s.nanotech && nanotech > 0) {
            const float k = 0.5f + 0.25f * static_cast<float>(s.nanotech - nanotech);
            pulse(0.7f * k, 0.5f * k, 16);
        } else if (hits > s.hits) {
            pulse(0.45f, 0.4f, 12);  // (the ship's shield, a hit that took no nanotech)
        }
        // death
        const bool dying = (st == 0x3d || st == 0x5d) && s.state != st;
        if (dying || (lost && !s.lost)) pulse(1.0f, 1.0f, 60);
        // a hard landing (falling faster than 18 units/s: 0.3 a frame at the game's 60)
        if (airborne(s.state) && !airborne(st) && s.vz < -0.3f && !ship) {
            const float k = std::clamp((-s.vz - 0.3f) / 0.5f, 0.2f, 1.0f);
            pulse(0.6f * k, 0.3f * k, 10);
        }
        // explosions nearby (a new shake: its frames set again)
        if (shake_frames > s.shake_frames) {
            const float k = std::clamp(lwc1(OV(0x00166de0)) / 0.4f, 0.1f, 1.0f);
            pulse(k, 0.6f * k, static_cast<int32_t>(10.0f + 15.0f * k));
        }
        // a shot (the item in hand the same, its ammo lower)
        if (item == s.item && ammo < s.ammo) shot_pulse(item);
        // the wrench's hit
        if (wrench_hit && !s.wrench_hit) pulse(0.3f, 0.5f, 6);
        // bolts (a subtle crunch, not a buzz while many come in)
        if (s.bolt_wait > 0) --s.bolt_wait;
        if (bolts > s.bolts && s.bolt_wait == 0) {
            pulse(0.0f, 0.15f, 3);
            s.bolt_wait = 4;
        }
    }
    s.valid = true;
    s.level = level;
    s.nanotech = nanotech;
    s.hits = hits;
    s.state = st;
    s.bolts = bolts;
    s.shake_frames = shake_frames;
    s.item = item;
    s.ammo = ammo;
    s.lost = lost;
    s.wrench_hit = wrench_hit;
    s.vz = z - s.z;
    s.z = z;
}
}  // namespace

// orig: (none) — enhancement (vibration.h)
void vibration_frame(uint32_t level) {
    Vibe& v = g_vibe;
    if (level == 0) {
        v = Vibe{};
        return;
    }
    events(v.seen);
    float low = 0.0f, high = 0.0f;
    for (Pulse& p : v.pulses) {
        if (p.left <= 0) continue;
        const float k = static_cast<float>(p.left) / static_cast<float>(p.frames);
        low = std::max(low, p.low * k);
        high = std::max(high, p.high * k);
        --p.left;
    }
    if (lbu(HERO + 0x20a4) == 2) {  // the ship flying: a low hum
        low = std::max(low, 0.12f);
        high = std::max(high, 0.04f);
    }
    const float scale = level == 1 ? 0.45f : 1.0f;  // Low, High
    v.low = low * scale;
    v.high = high * scale;
    v.fresh = true;
}

// orig: (none) — enhancement (vibration.h)
void vibration_take(float& low, float& high) {
    Vibe& v = g_vibe;
    low = v.fresh ? v.low : 0.0f;
    high = v.fresh ? v.high : 0.0f;
    v.fresh = false;
}

}  // namespace rac1
