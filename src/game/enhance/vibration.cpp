// Controller vibration (see vibration.h). Not a port: everything here is new, reading the game's state only.
#include <game/enhance/vibration.h>

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;   // the player block (g::PLAYER)
constexpr uint32_t SLOTS = 0x0013e5c0;  // the sound slots (30, 0x70 each: +4 state, +8 sound, +0x18 moby, +0x20 position)
constexpr int kSlots = 30;
constexpr float kFrameMs = 1000.0f / 60.0f;  // a gameplay frame (the game's 60)

struct Vec {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};
// orig: (none) — enhancement: a vector in EE RAM
Vec vec_at(uint32_t a) { return Vec{lwc1(a), lwc1(a + 4), lwc1(a + 8)}; }
// orig: (none) — enhancement: the distance of two points
float dist(const Vec& a, const Vec& b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)); }

// A pulse: its motors at the peak, its pan (positional: -1 left .. 1 right), its length and how long it is held at the
// peak (frames), its age
struct Pulse {
    float low = 0.0f, high = 0.0f;
    float pan = 0.0f;
    bool positional = false;
    int32_t frames = 0, hold = 0, age = 0;
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

struct Shot {  // the shots for the triggers' feedback (read while either setting is on)
    bool valid = false;
    uint32_t item = 0;
    int32_t ammo = 0;
    int32_t fired = 1000;  // gameplay frames since the last shot
};

struct SlotSeen {  // a sound slot the last gameplay frame
    bool active = false;
    uint32_t sound = 0, moby = 0;
    int32_t age = 0;  // gameplay frames it has played
    Vec pos;
    float speed = 0.0f;  // units a frame
};

struct Vibe {
    Pulse pulses[kPulses];
    Seen seen;
    Shot shot;
    SlotSeen slots[kSlots];
    VibeOut out;                // this gameplay frame's (taken by the host)
    VibeOut logged;             // the last one logged
    bool fresh = false;         // a gameplay frame ran since the host's last take
    uint32_t frame = 0;         // gameplay frames counted (the log's)
    float hum_pan = 0.0f;       // (the log: the moving source's pan last logged)
    uint32_t hum_moby = 0;
} g_vibe;

std::atomic<bool> g_dualsense{false};

// orig: (none) — enhancement: RAC1_VIBRATION_LOG
bool log_on() {
    static const bool on = std::getenv("RAC1_VIBRATION_LOG") != nullptr;
    return on;
}

// orig: (none) — enhancement: the camera's view of a point: its pan, -1 (left) .. 1 (right); centred within a unit of the
// camera (as the game's sound_pan 0x28c9e8 scales its pan down there)
float pan_of(const Vec& p) {
    const Vec cam = vec_at(OV(0x00166dc0)), fwd = vec_at(OV(0x00166fd0)), left = vec_at(OV(0x00166fe0));
    const Vec d{p.x - cam.x, p.y - cam.y, p.z - cam.z};
    const float f = d.x * fwd.x + d.y * fwd.y + d.z * fwd.z, l = d.x * left.x + d.y * left.y + d.z * left.z;
    const float h = std::sqrt(f * f + l * l);
    if (!(h > 1e-3f)) return 0.0f;
    return std::clamp(-l / h, -1.0f, 1.0f) * std::clamp(h - 1.0f, 0.0f, 1.0f);
}

// orig: (none) — enhancement: the hero's position
Vec hero_pos() { return vec_at(HERO + 0x80); }

// orig: (none) — enhancement: a moby pointer that can be read (in EE RAM, past the kernel's first megabyte)
bool moby_ok(uint32_t m) { return m >= 0x00100000u && m < 0x01fff000u && (m & 0xf) == 0; }

// orig: (none) — enhancement: a pulse started (replacing the weakest when all are in use): its motors at the peak, its
// length in milliseconds (held for `hold` of it, then cut down to 0), its pan (positional) or centred
void pulse(const char* name, float low, float high, float ms, float hold, bool positional = false, float pan = 0.0f, float d = -1.0f) {
    Vibe& v = g_vibe;
    Pulse* slot = &v.pulses[0];
    float weakest = 2.0f;
    for (Pulse& p : v.pulses) {
        if (p.frames == 0) {
            slot = &p;
            break;
        }
        const float s = std::max(p.low, p.high) * static_cast<float>(p.frames - p.age) / static_cast<float>(p.frames);
        if (s < weakest) {
            weakest = s;
            slot = &p;
        }
    }
    const int32_t frames = std::max(1, static_cast<int32_t>(std::lround(ms / kFrameMs)));
    *slot = Pulse{std::min(low, 1.0f), std::min(high, 1.0f), pan, positional, frames, std::clamp(static_cast<int32_t>(std::lround(frames * hold)), 1, frames), 0};
    if (log_on()) {
        std::printf("[vibe] f%u %s: low %.2f high %.2f, %d frames (%.0f ms, held %d)", v.frame, name, slot->low, slot->high, frames,
                    frames * kFrameMs, slot->hold);
        if (positional) std::printf(", pan %+.2f", pan);
        else std::printf(", centred");
        if (d >= 0.0f) std::printf(", %.1f units", d);
        std::printf("\n");
    }
}

// orig: (none) — enhancement: a pulse's strength now: 1 while held, then down to 0 over the frames left (the last
// frame's still above 0, the next one's 0)
float envelope(const Pulse& p) {
    if (p.age < p.hold) return 1.0f;
    return static_cast<float>(p.frames - p.age) / static_cast<float>(p.frames - p.hold + 1);
}

// orig: (none) — enhancement: a DualSense's two actuators for a strength `a` at pan `pan`: the constant-power pan (sin /
// cos of a quarter turn), raised by sqrt 2 so that the centre is `a` on both sides and a side at most `a`
void pan_split(float a, float pan, float& l, float& r) {
    const float t = (std::clamp(pan, -1.0f, 1.0f) + 1.0f) * 0.78539816f;
    l = a * std::min(1.0f, 1.41421356f * std::cos(t));
    r = a * std::min(1.0f, 1.41421356f * std::sin(t));
}

// orig: (none) — enhancement: the hero's airborne states (the jump family of jump_update 0x217970, as the strafe's)
bool airborne(int32_t st) {
    return st == 7 || (st >= 9 && st <= 0x12) || st == 0x1c || st == 0x3c || st == 0x49 || st == 0x4c || st == 0x5e || st == 0x69;
}

enum Weight { W_NONE, W_LIGHT, W_MEDIUM, W_HEAVY };
// orig: (none) — enhancement: the item's class: a weapon (the item table 0x1c40b0 gives it ammo, +0xe the most) light,
// medium or heavy; W_NONE for the wrench and the gadgets
Weight weight(uint32_t item) {
    if (item >= 0x40 || lhu(OV(0x001c40b0) + item * 0x18 + 0xe) == 0) return W_NONE;
    switch (item) {
        case 0x0f:  // Blaster
        case 0x10:  // Pyrocitor
        case 0x13:  // Tesla Claw
            return W_LIGHT;
        case 0x0b:  // Devastator
        case 0x17:  // R.Y.N.O.
            return W_HEAVY;
        default:  // the gloves (Bomb, Mine, Doom, Decoy), the Suck Cannon, the Visibomb, the Morph-o-Ray, ...
            return W_MEDIUM;
    }
}

// orig: (none) — enhancement: a weapon's shot by its class (vibration.h): the high motor for the light ones, both for
// the heavy ones
void shot_pulse(Weight w) {
    switch (w) {
        case W_LIGHT:
            pulse("shot (light)", 0.0f, 0.35f, 50.0f, 0.5f);
            return;
        case W_HEAVY:
            pulse("shot (heavy)", 0.7f, 0.5f, 90.0f, 0.5f);
            return;
        default:
            pulse("shot", 0.25f, 0.45f, 70.0f, 0.5f);
            return;
    }
}

// orig: (none) — enhancement: the sound slots read: which are playing, since when, how fast their moby moves
void slots_read(bool valid) {
    for (int i = 0; i < kSlots; ++i) {
        const uint32_t e = SLOTS + static_cast<uint32_t>(i) * 0x70;
        SlotSeen& s = g_vibe.slots[i];
        const uint32_t st = lbu(e + 4), sound = lwu(e + 8), moby = lwu(e + 0x18);
        const bool active = st != 0 && st != 4 && st != 6 && sound != 0;  // (7 to start, 1 started, 2 playing; 4 / 6 stopping)
        const Vec pos = vec_at(e + 0x20);
        const bool same = valid && s.active && active && s.sound == sound && s.moby == moby;
        s.speed = same ? dist(pos, s.pos) : 0.0f;
        s.age = same ? s.age + 1 : 0;
        s.active = active;
        s.sound = sound;
        s.moby = moby;
        s.pos = pos;
    }
}

// orig: (none) — enhancement: the sound an explosion started (a slot new this frame) nearest the distance from the camera
// its shake says (0.4 - 0.0175 x the distance; 0.05 from 20 units on): false when none fits
bool explosion_source(float shake, Vec& pos, float& d) {
    const Vec cam = vec_at(OV(0x00166dc0));  // (the blasts measure their shake from the camera)
    const float want = shake > 0.05f ? (0.4f - shake) / 0.0175f : 20.0f;
    float best = 1e9f;
    for (const SlotSeen& s : g_vibe.slots) {
        if (!s.active || s.age != 0) continue;
        const float sd = dist(s.pos, cam);
        const float err = want >= 20.0f ? std::max(0.0f, 14.0f - sd) : std::fabs(sd - want);
        if (err < best) {
            best = err;
            pos = s.pos;
            d = sd;
        }
    }
    return best <= 5.0f;
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
    const bool valid = s.valid && s.level == level;
    slots_read(valid);
    if (valid) {
        // damage: from the attacker's side when hero_hurt kept it
        if ((nanotech < s.nanotech && nanotech > 0) || (hits > s.hits && !(nanotech < s.nanotech))) {
            const bool shield = !(nanotech < s.nanotech);  // (the ship's shield, a hit that took no nanotech)
            const int32_t lost_n = shield ? 0 : s.nanotech - nanotech;
            const float k = std::min(1.0f, 0.75f + 0.25f * static_cast<float>(lost_n - 1));  // (1 nanotech 0.75, 2 or more 1)
            const float low = shield ? 0.55f : 0.8f * k, high = shield ? 0.4f : 0.55f * k;
            const float ms = shield ? 150.0f : std::min(250.0f, 170.0f + 40.0f * static_cast<float>(lost_n - 1));
            const uint32_t att = lwu(HERO + 0x2280);
            if (moby_ok(att) && att != lwu(HERO + 0x2080)) {
                const Vec p = vec_at(att + 0x10);
                pulse(shield ? "hit (shield)" : "hit", low, high, ms, 0.5f, true, pan_of(p), dist(p, hero_pos()));
            } else {
                pulse(shield ? "hit (shield)" : "hit", low, high, ms, 0.5f);
            }
        }
        // death
        const bool dying = (st == 0x3d || st == 0x5d) && s.state != st;
        if (dying || (lost && !s.lost)) pulse("death", 1.0f, 0.8f, 600.0f, 0.5f);
        // a hard landing (falling faster than 18 units/s: 0.3 a frame at the game's 60)
        if (airborne(s.state) && !airborne(st) && s.vz < -0.3f && !ship) {
            const float k = std::clamp((-s.vz - 0.3f) / 0.5f, 0.2f, 1.0f);
            pulse("landing", 0.6f * k, 0.25f * k, 100.0f, 0.5f);
        }
        // explosions nearby (a new shake: its frames set again), from the blast's sound
        if (shake_frames > s.shake_frames) {
            const float shake = lwc1(OV(0x00166de0));
            const float k = std::clamp(shake / 0.4f, 0.15f, 1.0f);
            Vec p;
            float d = 0.0f;
            if (explosion_source(shake, p, d)) pulse("explosion", k, 0.5f * k, 200.0f + 100.0f * k, 0.4f, true, pan_of(p), d);
            else pulse("explosion", k, 0.5f * k, 200.0f + 100.0f * k, 0.4f);
        }
        // a shot (the item in hand the same, its ammo lower)
        if (item == s.item && ammo < s.ammo) shot_pulse(weight(item));
        // the wrench's hit
        if (wrench_hit && !s.wrench_hit) pulse("wrench hit", 0.3f, 0.5f, 60.0f, 0.5f);
        // bolts (a subtle crunch, not a buzz while many come in)
        if (s.bolt_wait > 0) --s.bolt_wait;
        if (bolts > s.bolts && s.bolt_wait == 0) {
            pulse("bolts", 0.0f, 0.12f, 30.0f, 1.0f);
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

// orig: (none) — enhancement: a continuous source added to the motors: centred on the classic ones, panned on the
// DualSense's (strength `a`)
void add_source(VibeOut& o, float low, float high, float a, bool positional, float pan) {
    o.low = std::max(o.low, low);
    o.high = std::max(o.high, high);
    float l = low, r = high;
    if (positional) pan_split(a, pan, l, r);
    o.left = std::max(o.left, l);
    o.right = std::max(o.right, r);
}

// orig: (none) — enhancement: the sources that last (the ship's hum, the thrown wrench, the nearest moving sound)
void sources(VibeOut& o) {
    Vibe& v = g_vibe;
    if (lbu(HERO + 0x20a4) == 2) add_source(o, 0.12f, 0.04f, 0.12f, false, 0.0f);  // the ship flying: a low hum
    const Vec hero = hero_pos();
    const uint32_t wrench = lwu(HERO + 0x1090), hero_moby = lwu(HERO + 0x2080);
    if (lbu(HERO + 0x10aa) != 0 && moby_ok(wrench)) {  // the thrown wrench: a light whir from its side
        const Vec p = vec_at(wrench + 0x10);
        if (dist(p, hero) > 1.0f) add_source(o, 0.0f, 0.1f, 0.14f, true, pan_of(p));
    }
    // the nearest moving sound source (a moby's sound playing a second or more, the moby faster than 9 units/s)
    float best = 0.0f, best_pan = 0.0f;
    uint32_t best_moby = 0;
    for (const SlotSeen& s : v.slots) {
        if (!s.active || s.age < 60 || s.speed < 0.15f || s.speed > 5.0f || !moby_ok(s.moby) || s.moby == hero_moby || s.moby == wrench) continue;
        const float a = 0.2f * std::clamp((20.0f - dist(s.pos, hero)) / 16.0f, 0.0f, 1.0f);  // (full within 4 units, none past 20)
        if (a > best) {
            best = a;
            best_pan = pan_of(s.pos);
            best_moby = s.moby;
        }
    }
    if (best > 0.0f) add_source(o, 0.6f * best, 0.15f * best, best, true, best_pan);
    if (log_on() && (best_moby != v.hum_moby || std::fabs(best_pan - v.hum_pan) > 0.25f)) {
        if (best_moby != 0)
            std::printf("[vibe] f%u moving source: moby %06x class %d, strength %.2f, pan %+.2f\n", v.frame, best_moby, lh(best_moby + 0xa6), best,
                        best_pan);
        else
            std::printf("[vibe] f%u moving source: none\n", v.frame);
        v.hum_moby = best_moby;
        v.hum_pan = best_pan;
    }
}

// orig: (none) — enhancement: the adaptive triggers and the light bar (Adaptive Triggers on, Aiming = Modern, on foot)
void triggers(VibeOut& o, bool modern) {
    const Shot& s = g_vibe.shot;
    o.dualsense = true;
    const int32_t hp = lw(HERO + 0x22a8), most = lbu(HERO + 0x20a4) == 1 ? 4 : lw(0x0015eda0);  // (as hero_health_max: Clank's 4)
    o.health = most > 0 ? std::clamp(static_cast<float>(hp) / static_cast<float>(most), 0.0f, 1.0f) : -1.0f;
    if (!modern || lbu(HERO + 0x20a4) != 0) return;  // (the triggers fire and aim only with the modern controls, on foot)
    o.l2 = VibeTrigger{1, 0x40, 0x50, 0};  // the aim: a soft resistance from a quarter of the travel
    switch (weight(lwu(HERO + 0x10b8))) {
        case W_LIGHT:  // a light resistance, a weapon's rattle while firing
            o.r2 = s.fired < 8 ? VibeTrigger{3, 30, 0xa0, 0x30} : VibeTrigger{1, 0x30, 0x40, 0};
            return;
        case W_MEDIUM:  // a section that gives way (the click), a kick at the shot
            o.r2 = s.fired < 4 ? VibeTrigger{3, 60, 0xff, 0x00} : VibeTrigger{2, 0x40, 0x90, 0xa0};
            return;
        case W_HEAVY:
            o.r2 = s.fired < 6 ? VibeTrigger{3, 40, 0xff, 0x00} : VibeTrigger{2, 0x50, 0xb0, 0xff};
            return;
        default:
            return;
    }
}

// orig: (none) — enhancement: the log's lines when the result changes
void log_out(const VibeOut& o) {
    Vibe& v = g_vibe;
    VibeOut& l = v.logged;
    auto q = [](float x) { return std::lround(x * 100.0f); };
    if (q(o.low) != q(l.low) || q(o.high) != q(l.high) || q(o.left) != q(l.left) || q(o.right) != q(l.right))
        std::printf("[vibe] f%u motors low %.2f high %.2f | left %.2f right %.2f\n", v.frame, o.low, o.high, o.left, o.right);
    auto same = [](const VibeTrigger& a, const VibeTrigger& b) { return a.kind == b.kind && a.a == b.a && a.b == b.b && a.c == b.c; };
    if (o.dualsense != l.dualsense || !same(o.r2, l.r2) || !same(o.l2, l.l2) || q(o.health) != q(l.health))
        std::printf("[vibe] f%u triggers %s: r2 %d %02x %02x %02x, l2 %d %02x %02x %02x, health %.2f\n", v.frame, o.dualsense ? "on" : "off",
                    o.r2.kind, o.r2.a, o.r2.b, o.r2.c, o.l2.kind, o.l2.a, o.l2.b, o.l2.c, o.health);
    l = o;
}
}  // namespace

// orig: (none) — enhancement (vibration.h)
void vibration_frame(uint32_t level, uint32_t trig, bool modern_aim) {
    Vibe& v = g_vibe;
    if (level == 0 && trig == 0) {
        const uint32_t frame = v.frame;
        v = Vibe{};
        v.frame = frame;
        return;
    }
    ++v.frame;
    VibeOut o;
    if (level != 0) {
        events(v.seen);
        for (Pulse& p : v.pulses) {
            if (p.frames == 0) continue;
            const float k = envelope(p);
            o.low = std::max(o.low, p.low * k);
            o.high = std::max(o.high, p.high * k);
            float l = p.low * k, r = p.high * k;
            if (p.positional) pan_split(std::max(p.low, p.high) * k, p.pan, l, r);
            o.left = std::max(o.left, l);
            o.right = std::max(o.right, r);
            if (++p.age >= p.frames) p = Pulse{};
        }
        sources(o);
        const float scale = level == 1 ? 0.45f : 1.0f;  // Low, High
        o.low *= scale;
        o.high *= scale;
        o.left *= scale;
        o.right *= scale;
    } else {
        v.seen.valid = false;  // (the events not read while the vibration is off: read afresh when it comes back)
    }
    {  // the shots, for the triggers' feedback
        Shot& s = v.shot;
        const uint32_t item = lwu(HERO + 0x10b8);
        const int32_t ammo = item < 0x40 ? lw(0x0013d428 + item * 4) : 0;
        if (s.fired < 1000) ++s.fired;
        if (s.valid && item == s.item && ammo < s.ammo) s.fired = 0;
        s.valid = true;
        s.item = item;
        s.ammo = ammo;
    }
    if (trig != 0 && vibration_dualsense()) triggers(o, modern_aim);
    if (log_on()) log_out(o);
    v.out = o;
    v.fresh = true;
}

// orig: (none) — enhancement (vibration.h)
void vibration_take(VibeOut& out) {
    Vibe& v = g_vibe;
    out = v.fresh ? v.out : VibeOut{};
    v.fresh = false;
}

// orig: (none) — enhancement (vibration.h)
void vibration_set_dualsense(bool present) { g_dualsense.store(present); }

// orig: (none) — enhancement (vibration.h)
bool vibration_dualsense() { return g_dualsense.load(); }

}  // namespace rac1
