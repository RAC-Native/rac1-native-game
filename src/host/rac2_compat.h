// (Here, not in src/game: the generated code is fingerprinted by src/game's headers; the code is src/game/enhance/rac2_compat.cpp.)
// The engine services a custom planet's RAC2 behaviours call (local experiment, branch feat/custom-levels-experiment;
// PLANET_PLAN.md P4): what RAC2's level code asks of its engine, said with RAC1's engine. Our names, RAC1's moby record and
// memory; RAC2's code only read as the reference for what each service does. Small on purpose: what Maktar's arena
// controller (custom_arena.cpp) and the arena's classes (custom_classes.cpp) need, nothing more.
//
// - mobys: RAC1's 0x100-byte record (the offsets below), found by uid, shown / hidden, and a "stand-in" spawned as a copy
//   of a hidden template moby of the same class (RAC2 recycles its pre-placed enemies; RAC1's moby_spawn gives a fresh
//   slot, whose 0x80-byte pvars are too small for an enemy's: each copy gets a block of our own pool, below);
// - the pvars pool: EE RAM 0xc0000..0xca000 (64 blocks of 0x280 bytes), in the PS2 kernel's first megabyte, where
//   nothing of the game or the runtime lives (the menus' blocks start at 0xf0000); only written on a custom planet;
// - a state machine's bookkeeping (RAC2's moby +0x94 previous state, +0x95 substate, +0x96 timer);
// - the hero (position, nanotech), the bolts, the pad (pressed buttons; blocked while a menu of ours is open), the HUD
//   (a box of lines in the game's font), our own random numbers (the game's random stream stays as it is).
// With no custom planet nothing here is called: the game is the same.
#ifndef RAC1_HOST_RAC2_COMPAT_H
#define RAC1_HOST_RAC2_COMPAT_H

#include <cstdint>
#include <string>
#include <vector>

namespace rac1::rac2 {

// RAC1's moby record, the fields RAC2's code reads at its own offsets (RAC2: update +0x64, pvars +0x68, class +0xaa)
namespace moby {
constexpr uint32_t kPos = 0x10, kState = 0x20, kClass = 0x24, kFlags = 0x34, kRot = 0x40, kNextSeq = 0x53, kSpeed = 0x58,
                   kAnimFlags = 0x70, kPvars = 0x78, kSlot = 0xa8, kSlotIndex = 0xac, kClassId = 0xa6, kUid = 0xb2, kMatrix = 0xc0;
constexpr uint32_t kHidden = 3;  // (+0x34 bits 0 and 1: not drawn, not updated)
}  // namespace moby

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

// orig: (none) — the level's frame count (0x15f5cc)
uint32_t level_frame();
// orig: (none) — the game in play: game mode 0 (no menu, scene or loading screen)
bool in_play();

// orig: (none) — a moby's position
Vec3 moby_pos(uint32_t m);
// orig: (none) — the live moby of uid `uid` in the level's list, 0 none
uint32_t moby_by_uid(int32_t uid);
// orig: (none) — the moby shown and updated, or hidden and still
void moby_show(uint32_t m, bool shown);
// orig: (none) — the moby dead or its slot gone to another (state +0x20 >= 0xfe, another class or another pvars block)
bool moby_gone(uint32_t m, int32_t cls, uint32_t pvars);
// orig: (none) — the moby killed as the game's death leaves it (state 0xfe: the slot freed)
void moby_kill(uint32_t m);

// ---- the stand-ins: copies of a hidden template moby ----
// orig: (none) — a copy of template `tmpl` (its class, record and `pv_size` bytes of pvars, unchanged since the level
// loaded: hidden and not updated) spawned at `at` facing `yaw`, shown; its pvars a block of the pool. 0: no moby slot or
// no block free
uint32_t standin_spawn(uint32_t tmpl, uint32_t pv_size, Vec3 at, float yaw);
// orig: (none) — the copy's pvars block given back (the moby gone)
void standin_release(uint32_t pvars);
// orig: (none) — every block free (the level loaded again)
void standin_reset();
// orig: (none) — blocks in use
uint32_t standin_count();

// ---- animation, joints, the ground (RAC2's anim_seq_set 0x31e3c0, its joint points 0x315618, said with RAC1's) ----
// orig: (none) — sequence `seq` from frame `from` (its first by default), blended in over `blend` frames, unless it plays already
void seq_play(uint32_t m, uint32_t seq, int32_t blend, int32_t from = 0);
// orig: (none) — the sequence playing (or blending in)
uint32_t seq_now(uint32_t m);
// orig: (none) — the frame the sequence is at (its index and fraction)
float seq_frame(uint32_t m);
// orig: (none) — the time the sequence is at (RAC2's 0x3146d0: its frame records' own times, in frames)
float seq_time(uint32_t m);
// orig: (none) — the sequence played to its end (or looped) in this frame's step
bool seq_ended(uint32_t m);
// orig: (none) — the class's attachment points first .. first + n - 1 (its +0x1c table: each a chain of joints, the point its
// last one; RAC2's 0x315618 takes the same) in the world (RAC1's moby_joint_positions), n at most 8
void joints_world(uint32_t m, uint32_t first, uint32_t n, Vec3* out);
// orig: (none) — the ground's height under `m` (a ray from 1 above it down to `lowest`), `lowest` when there is none
float ground_under(uint32_t m, float lowest);

// orig: (none) — (testing) RAC1_ARENA_ENEMY_HIT=<level frame>[,<level frame>...]: at those frames a blast on `m` (a bomb's
// sphere test with the blaster's hit record, as RAC1_ARENA_MONITOR_HIT's): the hit through RAC1's collision and hit records
void test_hit(uint32_t m);

// orig: (none) — a hit taken as RAC1's own enemies take theirs (749's 0x2d5160): EVERY frame the moby's hit record (0x25a420,
// the enemies' mask 0x330000, or none) goes through its reaction (0x25a478) with the hit state block `hs` (0x20 bytes: +0x14
// the last weapon, +0x1c its repeat timer: the same weapon again within 15..60 frames does not count; the timer only runs
// down while this is called each frame), then the record is let go (+0xa4 0xff: a record left linked is met again every
// frame and keeps the timer going round, so a later hit of the same weapon was lost). The damage that counts (0: none), the
// hit's push in `push` (when not null and a hit came)
float hit_receive(uint32_t m, uint32_t hs, Vec3* push);

// ---- parts: mobys an enemy spawns and carries (RAC2's 0x314170 + 0x328500: 2850's blades, 2506's shield), and a carried
// part thrown off (RAC2's 0x32b468: it flies, falls, bounces, lies a while and goes). A part's own pvars (moby_spawn's 0x80
// bytes) hold its carrier; its update (part_update, the role "part") keeps it no longer than its carrier lives ----
// orig: (none) — a part of class `cls` spawned for carrier `m`, drawn as far as it is, updated, 0.75-sized as RAC2's when
// `scale` (else its class's size times `scale`); 0 no moby slot
uint32_t part_spawn(uint32_t m, int32_t cls, float scale);
// orig: (none) — the part placed on the carrier's attachment point `point` (RAC1's "keep the rows" flag: the point's rows,
// turned by `spin` radians about its own z)
void part_place(uint32_t part, uint32_t m, uint32_t point, float spin);
// orig: (none) — the part still the one spawned for `m` (alive, its class, carried by it)
bool part_of(uint32_t part, uint32_t m);
// orig: (none) — the part thrown off its carrier at `vel` (units a frame), spinning: it falls on the arena's floor at `floor`,
// bounces, lies `lie` frames, goes
void part_throw(uint32_t part, Vec3 vel, float floor, uint32_t lie);
// orig: (none) — every part's update: carried, gone with its carrier; thrown, its flight
void part_update(uint32_t part);
// orig: (none) — the part put at `at`, its x axis along `dir` (its z as near up as that leaves), drawn or not (still updated:
// it goes with its carrier either way); P7: Chainblade's chain links (RAC2's 0x3d5748 places each a frame)
void part_put(uint32_t part, Vec3 at, Vec3 dir, bool shown);
// orig: (none) — attachment point `point` of `m` in the world: its place and its matrix's rows (RAC2's 0x315578 reads its x)
void point_frame(uint32_t m, uint32_t point, Vec3& at, Vec3& x, Vec3& y, Vec3& z);

// orig: (none) — the camera shaken (RAC2's camera +0x160 strength, +0x168 frames: RAC1's 0x166de0 / 0x166de8, as its blasts
// shake it)
void camera_shake(float strength, uint32_t frames);

// ---- sounds (RAC2's 0x366b30 starts a class sound on a moby, looping with its flag 1, 0x366830 stops it, 0x366bd8 plays
// another class's sound on it): RAC1's class sound table on the moby and its sound slots ----
// orig: (none) — `m`'s looping class sound `idx` on (kept on while asked each frame or not; -1: stopped now)
void sound_loop(uint32_t m, int32_t idx);
// orig: (none) — sound `idx` of RAC1 class `cls`'s table (the class loaded on the level) played once on `m`
void sound_of_class(uint32_t m, int32_t cls, int32_t idx);

// ---- the attack turns (RAC2's controller keeps a count per enemy class, its pvars +0x50 2506, +0x54 2850, +0x58 3131,
// +0x5c 3134: 0x3fc0d0 takes one, 0x3fc1b0 gives it back; an enemy without one waits). Their start counts, set when a
// challenge starts (0x3fa52c: the four bytes at gp + 0x21d8), are turns_start's; 3131 takes none (its activate 0x40b868
// sets its +0x2e2: it attacks as soon as it is in). Every 4th frame (0x3fae80 -> 0x3fbcf8) the controller takes back the
// turns of the 2506s and 2850s and hands them to the two of them nearest Ratchet (in their fighting states) ----
// orig: (none) — RAC2's start count of class `rac2_cls` (0: it takes no turns)
int attack_turns_start(int32_t rac2_cls);
// orig: (none) — a turn for `m` (an enemy of RAC2 class `rac2_cls`, ready to fight: it asks every frame it is): true when
// it holds one (it keeps it until it gives it back, dies, or, a 2506 / 2850, the 4-frame pick passes it over: the turns of
// the dead are taken back here)
bool attack_turn_take(int32_t rac2_cls, uint32_t m);
// orig: (none) — its turn given back (none held: nothing)
void attack_turn_give(uint32_t m);
// orig: (none) — true while `m` holds a turn
bool attack_turn_held(uint32_t m);

// ---- sparring (RAC2's 0x3d1998 mode 2: a 2506 without a turn pairs with the nearest other 2506 without one, and the two
// fight each other, each the other's target, until one gets a turn, is hurt or dies; its pvars +0x2e0 the partner, +0x2f6
// 2) ----
// orig: (none) — `m`'s sparring partner (asked each frame it waits; 0: none to pair with): the pair kept while both ask
uint32_t spar_partner(int32_t rac2_cls, uint32_t m);
// orig: (none) — `m` out of its pair (hurt: its partner looks for another)
void spar_leave(uint32_t m);

// ---- steering round the others (RAC2's mover 0x331608 steers round what is in its way; ours: a push away from the
// other hand-ported enemies within `radius`) ----
// orig: (none) — `m`'s place noted for this frame, and the way away from the others near it (x, y: 0..1 each, summed)
Vec3 steer_round(uint32_t m, float radius);

// ---- a state machine (RAC2's +0x94..+0x96 kept by us) ----
struct StateMachine {
    int state = 0, previous = -1;
    uint32_t since = 0;  // the level frame it was entered
    bool entered = true; // (the first step in it)
    // orig: (none)
    void set(int s) {
        previous = state;
        state = s;
        since = level_frame();
        entered = true;
    }
    // orig: (none) — true on the first step in the state (cleared by it)
    bool first() {
        const bool f = entered;
        entered = false;
        return f;
    }
    // orig: (none) — frames in the state
    uint32_t frames() const { return level_frame() - since; }
};

// ---- the hero ----
// orig: (none)
Vec3 hero_pos();
// orig: (none) — nanotech now / most
int32_t hero_health();
int32_t hero_health_max();
// orig: (none) — the item in hand (8 the wrench)
uint32_t hero_item();

// orig: (none) — the hero hit by `by` (a moby, or 0: none), RAC1's hit protocol as its enemies and the Novalis fans hit
// him (a contact record in the hero moby's ring: the kinds 0x10001, a push `push` (x, y, z; RAC2's knock-back mark
// 5627.95 in w), `damage` the hit's strength); his own code then hurts him (hero_hurt), not while he cannot be hurt
void hero_hit(uint32_t by, Vec3 push, float damage);

// orig: (none) — bolts given (the HUD's counter shows them)
void bolts_give(int32_t n);

// ---- the pad (pad 1: 0x13c940) ----
enum : uint32_t {
    kTriangle = 0x10, kCircle = 0x20, kCross = 0x40, kSquare = 0x80, kStart = 0x800, kUp = 0x1000, kRight = 0x2000, kDown = 0x4000, kLeft = 0x8000
};
// orig: (none) — the buttons pressed this frame
uint32_t pad_pressed();
// orig: (none) — the buttons `mask` taken from the game this frame (held, pressed, released)
void pad_take(uint32_t mask);
// orig: (none) — the whole pad taken from the game this frame (the sticks centred): a menu of ours is open
void pad_block();

// ---- the HUD (from hud_draw) ----
struct Line {
    std::string text;
    uint32_t rgb = 0xf0f0f0;
};
// orig: (none) — lines centred at x 0x100 from y, on a box
void hud_lines(int32_t y, const std::vector<Line>& lines);

// ---- random numbers of our own (xorshift) ----
// orig: (none) — 0 .. n - 1
int32_t rand_below(int32_t n);
// orig: (none) — 0 .. 1
float rand_unit();

}  // namespace rac1::rac2

#endif
