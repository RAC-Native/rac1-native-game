// _devtools/dev_hoverboard.cpp -- the debug menu's free hoverboard (openly invented, ADR 0003; not a port).
//
// Hoverboard anywhere on the levels without the board (all but Rilgar, 5, and Kalebo III, 16, where the game's own board is
// mounted: dev_menu.cpp). What the board needs, and where it lives:
// - the riding code: the board state 0x6b (sub-state 0x16), the wipe-out 0x6d and the fall 0x6e are level-own code of 5 and 16
//   (their transliterated hero controllers, rilgar_hero_tr_*.cpp / kalebo_hero_tr_*.cpp, read their jump tables and globals
//   at level 5's / 16's own data addresses: they cannot run on another level's memory). On every other level the hero
//   controllers send 0x6b to the same place as state 5: nothing per frame, no transitions, the set_state's common tail only.
//   So 0x6b is a state the host can own there: the game leaves the hero where he is (his animation, moby, camera, hits,
//   water and the kill height still run as for any state), and the riding below moves him between the game's frames,
//   with the game's own collision query (segment_collide 0x1efc70).
// - the hero's board animations: level data. The hero's sequences come from the level's core (its 256-entry hero sequence
//   table); Kalebo III's has the board's (58, 82, 85, 98), most levels do not (Kerwan: their class slots are null). They
//   are read from Kalebo III's core on the disc, put at host_layout::DEV_BOARD_SEQS (above the game's 32 MB) with their
//   frame pointers relocated as the game's load does (checked byte for byte against Kalebo III's own copy), and hooked into
//   the hero class's null slots while riding (put back after).
// - the board's model: Kalebo III's class 439, drawn by the MRP on the hero as the frame drew him (mrp_moby_renderer.cpp,
//   append_guest: its asset and textures from assets/generated/mrp/moby/level16/). Host draw only: no moby in the game.
// - no sound.
#include "dev_hoverboard.h"

#include <formats/level_core.h>
#include <formats/scene_wad.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/variants_l5.h>
#include <game/ovl/variants_l6.h>
#include <runtime/kernel/disc.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/kernel/vu0.h>
#include <runtime/level/host_layout.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace rac1 {
// (mrp_moby_renderer.cpp: the guest class the MRP draws on the hero; declared here, not in a header: see there)
extern std::atomic<int> g_mrp_guest_class;
extern std::atomic<bool> g_mrp_guest_on;
extern std::atomic<float> g_mrp_guest_lift;
// (ovl_1e_1f.cpp: the collision's contact registers kept aside and put back, as mrp_lod.cpp's shadow runs do)
void collide_vu_save(float out[15]);
void collide_vu_restore(const float in[15]);
}  // namespace rac1

namespace rac1::devtools {

namespace {

constexpr uint32_t P = 0x0013f350;          // the player
constexpr uint32_t HERO_MOBY = P + 0x2080;
constexpr uint32_t POS = P + 0x80, ROT = P + 0x90, VEL = P + 0xe0, VEL_H = P + 0x100;
constexpr uint32_t POS_RING = P + 0x1b00;   // the last 32 positions (the stuck check)
constexpr uint32_t STATE = P + 0x2084, SUB = P + 0x208c;
constexpr uint32_t STICK = P + 0x1d20;      // the left stick (x, y: -1..1, y -1 forward; hero_tick copies the pad's)
constexpr uint32_t PAD = 0x0013c940;        // pad 1: +0x1a0 the buttons held
constexpr uint32_t HEALTH = P + 0x22a8;
constexpr uint32_t LEVEL = 0x0015ed84;
constexpr OvConst<0x0015f5c4u> GAME_MODE{};  // 0 playing, 3 the pause menu, 2 / 6 a dialog, a cutscene, the level's exit
constexpr OvConst<0x00166dd8u> CAM_YAW{};   // the camera's yaw (hero_stick_move's)
constexpr OvConst<0x0015f638u> KILL_Z{};    // the level's kill height (the input transition's fall, state 0x77)
constexpr OvConst<0x00173e60u> HIT_POINT{};  // segment_collide's hit (0x173e40 +0x20 the point, +0x40 the normal)
constexpr OvConst<0x00173e80u> HIT_NORMAL{};
constexpr uint32_t CROSS = 0x40;

constexpr uint32_t RIDE_STATE = 0x6b;       // the board state: nothing per frame on the levels without the board
constexpr uint32_t FALL_STATE = 0x77;       // the fall to death (below the kill height, or a level's pit)
constexpr int SOURCE_LEVEL = 16;            // Kalebo III: the board's class and the hero's board sequences
constexpr int BOARD_CLASS = 439;
constexpr int SEQ_STAND = 82, SEQ_RIDE = 85, SEQ_FALL = 58, SEQ_JUMP = 98;  // (Kalebo III's own: stood / ridden / falling / jumping)
constexpr int SEQS[] = {SEQ_STAND, SEQ_RIDE, SEQ_FALL, SEQ_JUMP};
constexpr float BOARD_LIFT = 0.215f;        // the board's origin above the hero's (Kalebo III's board, measured)

// the riding (per game frame; units the game's, metres)
constexpr uint32_t FRAME_SCALE = 0x0015ed68;  // the game's frame scale (1, or 50 / 60 on the PAL disc's 50 Hz)
constexpr float MAX_SPEED = 20.0f, ACCEL = 18.0f, BRAKE = 30.0f, DRAG = 4.0f;
constexpr float TURN_SLOW = 4.5f, TURN_FAST = 2.2f;  // rad / s at rest and at full speed
constexpr float GRAVITY = 30.0f, JUMP_SPEED = 10.0f;
constexpr float STEP_UP = 0.7f;             // a step or a ramp the board climbs in one frame
constexpr float SNAP_DOWN = 0.9f;           // the ground followed down a slope (more: a fall)
constexpr float WALL_LOW = 0.35f, WALL_HIGH = 1.1f;  // the wall checks' heights (a railing, a wall)
constexpr int COYOTE = 6;                   // frames after leaving the ground a jump still works
constexpr int LOST_FRAMES = 4 * 60;         // airborne this long: off the board, back on the last ground

float rf(uint32_t a) { return ee_read<float>(a); }
void wf(uint32_t a, float v) { ee_write<float>(a, v); }
uint32_t ru(uint32_t a) { return ee_read<uint32_t>(a); }
int32_t r32(uint32_t a) { return ee_read<int32_t>(a); }

struct V3 {
    float x = 0, y = 0, z = 0;
};
V3 rv(uint32_t a) { return {rf(a), rf(a + 4), rf(a + 8)}; }
void wv(uint32_t a, const V3& v) {
    wf(a, v.x);
    wf(a + 4, v.y);
    wf(a + 8, v.z);
}
float wrap(float a) {
    while (a > 3.14159265f) a -= 6.28318531f;
    while (a < -3.14159265f) a += 6.28318531f;
    return a;
}

// orig: (host) the level-own collision flags of the hero's floor queries (hero_fall_time's)
uint32_t floor_flags() { return on_aridia() ? 0x324u : (on_blarg() || on_oltanis()) ? 0xd24u : 0x24u; }

// orig: (host) the game's segment query from a to b (the hero moby ignored): the hit point and normal. Asked between the
// game's frames on a journal of EE memory, its contact registers kept: the game's memory (the scratchpad, the contact block
// 0x173e40, the moby pose ring) and the collision's state are left as the game left them
bool segment(const V3& a, const V3& b, V3& hit, V3& n) {
    Vec4 s{}, e{};
    s.x = a.x, s.y = a.y, s.z = a.z;
    e.x = b.x, e.y = b.y, e.z = b.z;
    float regs[15];
    collide_vu_save(regs);
    ee_journal_begin();
    const bool got = segment_collide_v(s, e, floor_flags(), ru(HERO_MOBY), nullptr) != 0;
    if (got) {
        hit = rv(HIT_POINT);
        n = rv(HIT_NORMAL);
    }
    ee_journal_restore();
    collide_vu_restore(regs);
    return got;
}

// --- the board sequences ---
struct Seqs {
    bool tried = false;
    std::vector<std::vector<uint8_t>> bytes;  // per SEQS entry, as stored in the source level's core (empty: none)
} g_seqs;

// orig: (host) the hero's sequence `id` of a level's core, as stored (empty if none)
std::vector<uint8_t> core_hero_sequence(const DecodedLevelCoreIndex& ix, const std::vector<uint8_t>& data, int id) {
    for (const LevelCoreSubAsset& a : ix.sub_assets) {
        if (a.kind != "sequence" || a.id != id || a.size == 0 || a.offset + a.size > data.size()) continue;
        std::vector<uint8_t> out;
        if (a.packed) {
            if (!decompress_wad(std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.data() + a.offset), a.size), out)) out.clear();
        } else {
            out.assign(data.begin() + a.offset, data.begin() + a.offset + a.size);
        }
        return out;
    }
    return {};
}

// orig: (host) the source level's board sequences read from the disc (once)
void seqs_read() {
    if (g_seqs.tried) return;
    g_seqs.tried = true;
    g_seqs.bytes.assign(std::size(SEQS), {});
    std::vector<std::byte> wad;
    std::vector<uint8_t> index, data;
    if (!disc_read_level_wad(SOURCE_LEVEL, wad) || !extract_core_from_level_wad(wad, index, data)) {
        std::printf("[hoverboard] level %d's core not read: no board animations\n", SOURCE_LEVEL);
        return;
    }
    try {
        const DecodedLevelCoreIndex ix = decode_level_core_index(std::as_bytes(std::span(index)), std::as_bytes(std::span(data)));
        for (size_t i = 0; i < std::size(SEQS); ++i) g_seqs.bytes[i] = core_hero_sequence(ix, data, SEQS[i]);
    } catch (const std::exception& e) {  // (a damaged core: the board without its animations, not the game stopped)
        std::printf("[hoverboard] level %d's core not decoded (%s): no board animations\n", SOURCE_LEVEL, e.what());
    }
}

// --- the ride ---
struct Ride {
    bool on = false;
    int32_t level = -1;
    V3 pos;
    float yaw = 0.0f, speed = 0.0f, vz = 0.0f;
    bool ground = true;
    int air = 0;          // frames off the ground
    int grounded = 0;     // frames on the ground in a row
    V3 safe;              // the last solid ground (a little behind)
    float safe_yaw = 0.0f;
    V3 trail[16];         // the ground positions of the last frames (safe: the oldest)
    int trail_n = 0;
    int anim = -1;
    bool cross_was = false;
    uint32_t tick = 0;    // the hero's state timer at the last step (+0x198: runs with his update; the pause stops it)
    std::vector<uint32_t> slots;  // the class slots hooked
    bool restore_pending = false;
} g_ride;

// orig: (host) the board sequences hooked into the hero class's null slots; false when the class has none of them (Clank's).
// A sequence not read (no disc) stays out: Ratchet keeps the animation he had (a ride without the board's poses)
bool seqs_hook() {
    seqs_read();
    const uint32_t hm = ru(HERO_MOBY);
    const uint32_t cls = hm != 0 ? ru(hm + 0x24) : 0;
    if (cls == 0) return false;
    const uint32_t count = ee_read<uint8_t>(cls + 0xc);
    uint32_t at = host_layout::DEV_BOARD_SEQS;
    g_ride.slots.clear();
    for (size_t i = 0; i < std::size(SEQS); ++i) {
        const uint32_t id = static_cast<uint32_t>(SEQS[i]);
        if (id >= count) return false;
        const uint32_t slot = cls + 0x48 + 4 * id;
        if (ru(slot) != 0) continue;  // (the level has it: Veldin has every sequence)
        const std::vector<uint8_t>& b = g_seqs.bytes[i];
        if (b.size() < 0x20 || at + b.size() > host_layout::DEV_BOARD_SEQS + 0x40000) continue;
        std::memcpy(ee_ram_ptr(at), b.data(), b.size());
        // the frame table after the 0x1c-byte head (one pointer a frame, +0x10 the count; top bits: packed) and the trigger
        // block (+0x14): offsets in the sequence, made addresses as the game's load does
        const uint32_t frames = b[0x10];
        for (uint32_t k = 0; k < frames && 0x1c + 4 * k + 4 <= b.size(); ++k) ee_write<uint32_t>(at + 0x1c + 4 * k, ru(at + 0x1c + 4 * k) + at);
        if (ru(at + 0x14) != 0) ee_write<uint32_t>(at + 0x14, ru(at + 0x14) + at);
        ee_write<uint32_t>(slot, at);
        g_ride.slots.push_back(slot);
        at = (at + static_cast<uint32_t>(b.size()) + 0xf) & ~0xfu;
    }
    return true;
}

// orig: (host) the slots put back (null) once the hero plays none of the board's sequences
void seqs_unhook() {
    const uint32_t hm = ru(HERO_MOBY);
    if (hm != 0) {
        const uint32_t a = ee_read<uint8_t>(hm + 0x53);
        for (int s : SEQS)
            if (a == static_cast<uint32_t>(s)) {
                g_ride.restore_pending = true;
                return;
            }
        const uint32_t cls = ru(hm + 0x24);
        for (uint32_t slot : g_ride.slots)
            if (cls != 0 && slot >= cls && slot < cls + 0x48 + 4 * 256) ee_write<uint32_t>(slot, 0);
    }
    g_ride.slots.clear();
    g_ride.restore_pending = false;
}

// orig: (host) the hero's animation (the game's hero_anim_set), when the class has it
void anim(int seq, float blend) {
    if (g_ride.anim == seq) return;
    const uint32_t hm = ru(HERO_MOBY);
    const uint32_t cls = hm != 0 ? ru(hm + 0x24) : 0;
    if (cls == 0 || ru(cls + 0x48 + 4 * static_cast<uint32_t>(seq)) == 0) return;
    g_ride.anim = seq;
    hero_anim_set(static_cast<uint32_t>(seq), 0, blend);
}

// orig: (host) the hero where the ride puts him: position, facing, velocity (his moby follows at his next update)
void place(const V3& p, float yaw, const V3& vel) {
    wv(POS, p);
    wv(P + 0xc0, p);
    for (uint32_t i = 0; i < 32; ++i) wv(POS_RING + i * 16, p);
    wv(VEL, vel);
    wv(VEL_H, {vel.x, vel.y, 0.0f});
    wf(ROT + 8, yaw);
    const uint32_t hm = ru(HERO_MOBY);
    if (hm != 0) {
        wv(hm + 0x10, p);
        wf(hm + 0x48, yaw);
    }
}

// orig: (host) off the board: the hero on foot (the game's standing state) at `p`
void get_off(const V3& p, float yaw) {
    place(p, yaw, {});
    if (r32(STATE) == static_cast<int32_t>(RIDE_STATE) || r32(STATE) == static_cast<int32_t>(FALL_STATE)) hero_set_state(0, 1);
    g_ride.on = false;
    g_mrp_guest_on = false;
    seqs_unhook();
}

// orig: (host) one game frame of riding
void step() {
    Ride& R = g_ride;
    const float fs = rf(FRAME_SCALE);
    const float DT = (fs > 0.5f && fs < 1.5f ? 1.0f / fs : 1.0f) / 60.0f;  // (a 50 Hz frame is longer)
    // the stick, through the camera as the hero's own walk turns it (hero_stick_move)
    const float sx = rf(STICK), sy = rf(STICK + 4);
    float mag = std::sqrt(sx * sx + sy * sy);
    if (mag > 1.0f) mag = 1.0f;
    if (mag > 0.2f) {
        const float want = wrap(rf(CAM_YAW) + std::atan2(-sx, -sy));  // (stick up: the camera's heading; right: clockwise)
        const float d = wrap(want - R.yaw);
        const float t = R.speed / MAX_SPEED;
        const float rate = (TURN_SLOW + (TURN_FAST - TURN_SLOW) * std::min(1.0f, t)) * DT;
        R.yaw = wrap(R.yaw + std::clamp(d, -rate, rate));
        const float target = std::cos(d) > 0.0f ? MAX_SPEED * mag * std::cos(d) : 0.0f;
        if (R.speed < target) R.speed = std::min(target, R.speed + ACCEL * DT);
        else R.speed = std::max(target, R.speed - (std::cos(d) < -0.3f ? BRAKE : DRAG) * DT);
    } else {
        R.speed = std::max(0.0f, R.speed - DRAG * DT);
    }
    // the jump (cross, on the ground or just off it)
    const bool cross = (ru(PAD + 0x1a0) & CROSS) != 0;
    if (cross && !R.cross_was && (R.ground || R.air < COYOTE) && R.vz < JUMP_SPEED * 0.5f) {
        R.vz = JUMP_SPEED;
        R.ground = false;
        R.air = COYOTE;
    }
    R.cross_was = cross;
    // the horizontal move, the walls slid along
    const V3 fwd{std::cos(R.yaw), std::sin(R.yaw), 0.0f};
    V3 mv{fwd.x * R.speed * DT, fwd.y * R.speed * DT, 0.0f};
    for (int pass = 0; pass < 3 && (mv.x != 0.0f || mv.y != 0.0f); ++pass) {
        const float len = std::sqrt(mv.x * mv.x + mv.y * mv.y);
        V3 hit, n;
        bool wall = false;
        for (float h : {WALL_LOW, WALL_HIGH}) {
            const V3 a{R.pos.x, R.pos.y, R.pos.z + h};
            const V3 b{a.x + mv.x / len * (len + 0.45f), a.y + mv.y / len * (len + 0.45f), a.z};
            if (segment(a, b, hit, n) && std::fabs(n.z) <= 0.6f) {
                wall = true;
                break;
            }
        }
        if (!wall) break;
        const float nl = std::sqrt(n.x * n.x + n.y * n.y);
        if (nl <= 0.0f) break;
        const float nx = n.x / nl, ny = n.y / nl;
        const float into = mv.x * nx + mv.y * ny;
        if (into >= 0.0f) break;
        mv.x -= nx * into;
        mv.y -= ny * into;
        const float keep = std::sqrt(mv.x * mv.x + mv.y * mv.y) / len;  // (the speed the slide keeps)
        R.speed *= std::max(0.2f, keep);
        if (pass == 2) mv = {};  // (a corner: stopped)
    }
    V3 np{R.pos.x + mv.x, R.pos.y + mv.y, R.pos.z};
    // the ground under the new position: stepped up onto, followed down a slope, or a fall
    const float old_z = R.pos.z;
    const float down = R.ground ? SNAP_DOWN : std::max(0.05f, -R.vz * DT) + 0.05f;
    const bool rising = !R.ground && R.vz > 0.0f;
    V3 hit, n;
    bool on = false;
    if (!rising && segment({np.x, np.y, np.z + STEP_UP}, {np.x, np.y, np.z - down}, hit, n) && n.z > 0.5f) {
        np.z = hit.z;
        on = true;
    }
    if (on) {
        if (R.ground) R.vz = std::clamp((np.z - old_z) / DT, -12.0f, 12.0f);  // (the slope's: carried off a ramp's top)
        else R.vz = 0.0f;
        R.ground = true;
        R.air = 0;
        ++R.grounded;
    } else {
        if (R.ground) R.vz = std::max(R.vz, 0.0f);
        R.ground = false;
        R.grounded = 0;
        ++R.air;
        R.vz -= GRAVITY * DT;
        np.z += R.vz * DT;
        if (R.vz > 0.0f && segment({np.x, np.y, old_z + 1.2f}, {np.x, np.y, np.z + 1.5f}, hit, n) && n.z < -0.3f) {  // (a ceiling)
            R.vz = 0.0f;
            np.z = old_z;
        }
    }
    R.pos = np;
    // the last solid ground: where he was a quarter second ago on flat enough ground
    if (R.ground && n.z > 0.8f) {
        R.trail[R.trail_n % 16] = R.pos;
        ++R.trail_n;
        if (R.grounded > 16) {
            R.safe = R.trail[R.trail_n % 16];
            R.safe_yaw = R.yaw;
        }
    }
    // off the level: the void, the water, the kill height, the map's edge
    const bool water = ee_read<uint8_t>(P + 0x12e4) != 0 && R.pos.z < rf(P + 0x2f0) - 0.2f;
    const bool deep = R.pos.z < rf(KILL_Z) + 1.0f;
    const bool edge = R.pos.x < 3.0f || R.pos.y < 3.0f || R.pos.x > 1021.0f || R.pos.y > 1021.0f;
    if (water || deep || edge || R.air > LOST_FRAMES) {
        std::printf("[hoverboard] off the board (%s): back on foot at %.2f %.2f %.2f\n", water ? "water" : deep ? "below the level" : edge ? "the map's edge" : "a long fall",
                    R.safe.x, R.safe.y, R.safe.z);
        get_off(R.safe, R.safe_yaw);
        return;
    }
    place(R.pos, R.yaw, {});  // (no velocity: the game's own move of the hero, which runs in any state, would add it)
    // the animation
    if (!R.ground && R.air > COYOTE) anim(R.vz > 0.0f ? SEQ_JUMP : SEQ_FALL, 8.0f);
    else anim(R.speed > 2.0f ? SEQ_RIDE : SEQ_STAND, 8.0f);
}

}  // namespace

// orig: (host) see dev_hoverboard.h
bool dev_free_board_on() { return g_ride.on; }

// orig: (host) see dev_hoverboard.h
bool dev_free_board_start() {
    Ride& R = g_ride;
    if (R.on) return true;
    if (R.restore_pending) seqs_unhook();
    const uint32_t hm = ru(HERO_MOBY);
    if (hm == 0 || ee_read<uint8_t>(P + 0x20b1) != 0 || r32(HEALTH) <= 0) return false;  // (no hero / lost / dying)
    if (ee_read<uint8_t>(P + 0x20a4) != 0 || r32(GAME_MODE) != 0) return false;        // (another movement mode: the ship...; not playing)
    const int32_t sub = r32(SUB);
    if (sub == 0x14) return false;
    if (!seqs_hook()) {
        seqs_unhook();
        return false;
    }
    if (hero_set_state(RIDE_STATE, 1) == 0 || r32(STATE) != static_cast<int32_t>(RIDE_STATE)) {
        seqs_unhook();
        return false;
    }
    std::vector<uint32_t> slots = std::move(R.slots);
    R = Ride{};
    R.slots = std::move(slots);
    R.on = true;
    R.level = r32(LEVEL);
    R.pos = rv(POS);
    R.yaw = rf(ROT + 8);
    R.safe = R.pos;
    R.safe_yaw = R.yaw;
    R.ground = false;  // (the first step finds the ground)
    R.air = COYOTE;
    R.tick = ru(P + 0x198);
    R.cross_was = (ru(PAD + 0x1a0) & CROSS) != 0;
    g_mrp_guest_class = SOURCE_LEVEL * 10000 + BOARD_CLASS;
    g_mrp_guest_lift = BOARD_LIFT;
    g_mrp_guest_on = true;
    anim(SEQ_STAND, 6.0f);
    return true;
}

// orig: (host) see dev_hoverboard.h
void dev_free_board_stop() {
    if (!g_ride.on) return;
    get_off(g_ride.pos, g_ride.yaw);
}

// orig: (host) see dev_hoverboard.h
void dev_free_board_frame() {
    Ride& R = g_ride;
    if (!R.on) {
        if (R.restore_pending) seqs_unhook();
        return;
    }
    // the game took him off (a hit, a cutscene, the water...) or another level: the ride ends, the game's state kept
    if (r32(LEVEL) != R.level || ru(HERO_MOBY) == 0) {
        R.on = false;
        g_mrp_guest_on = false;
        g_mrp_guest_class = -1;  // (not loaded with the next level's)
        R.slots.clear();  // (a new level: the hero's class is the new level's)
        return;
    }
    if (r32(STATE) == static_cast<int32_t>(FALL_STATE)) {  // (a level's pit: back on the last ground at once, as off the level)
        std::printf("[hoverboard] fell into the level's pit: back on foot at %.2f %.2f %.2f\n", R.safe.x, R.safe.y, R.safe.z);
        get_off(R.safe, R.safe_yaw);
        return;
    }
    if (r32(STATE) != static_cast<int32_t>(RIDE_STATE)) {
        std::printf("[hoverboard] the game took Ratchet off the board: state 0x%x (sub-state 0x%x, nanotech %d) at %.2f %.2f %.2f (kill height %.2f)\n", r32(STATE), r32(SUB),
                    r32(HEALTH), rf(POS), rf(POS + 4), rf(POS + 8), rf(KILL_Z));
        R.on = false;
        g_mrp_guest_on = false;
        seqs_unhook();
        return;
    }
    const int32_t mode = r32(GAME_MODE);
    g_mrp_guest_on = mode == 0 || mode == 3;  // (not in the level exit's flight)
    const uint32_t tick = ru(P + 0x198);
    if (tick == R.tick || mode != 0) return;  // (the hero not updated, or not playing: paused, a dialog, the level's exit)
    R.tick = tick;
    step();
}

}  // namespace rac1::devtools
