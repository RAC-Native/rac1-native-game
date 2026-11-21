// Batalia (level 8) own moby classes, hand-ported: class 441, the gunship/warship piece (held on its parent while in
// state 0; once flung off in state 1, tumbling into the sea below with smoke and a splash), and class 444, the large
// three-drum Blarg warship (33 pieces on chain points: two fins 0x1c2, two guns 0x1c1 / 0x1b9, a body 0x1bd, a turret
// 0x1ba, three counter-rotating drums 0x1c0 and twenty-four pods 0x1bb), with the shared piece spawner bat_piece_spawn_l8
// and the warship's build, follow and hit helpers. Header batalia_own.h; registered in batalia_mobys.cpp. Addresses are
// level 8's (`orig: L8:`); its own data is L8_ADDR (splash and warship tuning at 0x161c00 .. 0x161c4c), the base ELF's
// are bare.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/batalia_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;
constexpr uint32_t HERO_MOBY = 0x001413d0;
constexpr uint32_t kWarshipPieces = 33;  // the piece records after the warship's own (pvars +0x70 .. +0x27f)

const float kDegToRad = u2f(0x3c8efa35u);
const float kHalfPi = u2f(0x3fc90fdbu);
const float kPi = u2f(0x40490fdbu);

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

// the heading from (x0, y0) to (x1, y1), as the original computes it
float heading(float x0, float y0, float x1, float y1) { return atan2_f(fpu_sub(x1, x0), fpu_sub(y1, y0)); }

// a path's point `i` (the points from +0x10, 0x10 each)
uint32_t path_pt(uint32_t path, int32_t i) { return path + 0x10 + (static_cast<uint32_t>(i) << 4); }

// the piece record `k` (pvars +0x60 + 0x10 k: +0 the piece, +4 its parent, +8 the piece's chain point, +0xc the parent's)
uint32_t piece_rec(uint32_t pv, uint32_t k) { return pv + 0x60 + (k << 4); }

// the follower record `rec`'s next point (+0 the point's index, +4 the direction, a signed byte, +0x10 the path)
int32_t next_point(uint32_t rec) {
    const uint32_t path = lwu(rec + 0x10);
    const uint32_t dir = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(rec + 4))));
    return ee_rem(static_cast<int32_t>(lwu(rec) + lwu(path) + dir), lw(path));
}

// the warship stepped towards `goal` (EE vectors; `step` the step) by at most its speed 0x161c38 a second
void step_to(uint32_t m, uint32_t goal, uint32_t step) {
    vec3_sub(V(step), V(goal), V(m + 0x10));
    const float len = vec3_length(V(step));
    const float most = fpu_mul(lwc1(L8_ADDR(0x00161c38)), dt());
    if (most < len) vec3_set_length(V(step), V(step), most);
    vec3_add(V(m + 0x10), V(m + 0x10), V(step));
}

// the piece of record `rec` flung off by the warship `m`: a burst of size `burst` at it, sent off away from `from`
// (an EE vector) at `speed` a second (the piece's pvars +0), its state 1, the record emptied
void fling(uint32_t m, uint32_t rec, uint32_t from, float burst, float speed) {
    const uint32_t piece = lwu(rec);
    const uint32_t ppv = lwu(piece + 0x78);
    explosion_burst(m, piece + 0x10, -1, burst, 13.0f);
    vec3_sub(V(ppv), V(lwu(rec) + 0x10), V(from));
    vec3_set_length(V(ppv), V(ppv), fpu_mul(dt(), speed));
    sb(lwu(rec) + 0x20, 1);
    sw(rec, 0);
}

// the pod of record `rec` broken off: flung away from its parent
void break_off(uint32_t m, uint32_t rec) { fling(m, rec, lwu(rec + 4) + 0x10, 2.0f, 10.0f); }
}  // namespace

// Class 441, a gunship/warship piece (shared across classes 441 .. 449). pvars: +0x00 its velocity once flung off, +0x10 /
// +0x14 its roll and pitch spin a frame, +0x18 a frame count (90 .. 150 scaled, written in state 1). States: 0 (and 4+):
// attached, nothing here; 1 flung off: picks its spin (-pi/2 .. pi/2 a second on roll and pitch) and +0x18, sets state 2
// and falls right in; 2 falling (gravity 0.5 x 10.8 a second squared, retired outside x/y 8 .. 500 or above z 500): one
// frame in three trails smoke (kind 0xb, one of the two words at 0x161c30), and below z 15.2 splashes (0x161c00 particles
// of kind 2, tuning 0x161c04 .. 0x161c2c) into state 3; 3 sinking under the same gravity until outside x/y 8 .. 500 or
// below z 8, then retired.
// orig: L8:0x002e16c0
void bat_class441_update_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(224);
    const uint32_t pos = m + 0x10;

    switch (lbu(m + 0x20)) {  // (a compare tree on 1, 2, 3)
    case 1: {
        const float max_spin0 = fpu_mul(dt(), kHalfPi);
        swc1(pv + 0x10, rand_range_f(-max_spin0, max_spin0));
        const float max_spin1 = fpu_mul(dt(), kHalfPi);
        swc1(pv + 0x14, rand_range_f(-max_spin1, max_spin1));
        const float lo = static_cast<float>(frame_scale_i(0x5a));
        const float hi = static_cast<float>(frame_scale_i(0x96));
        sw(pv + 0x18, static_cast<uint32_t>(float_to_int(rand_range_f(lo, hi))));
        sb(m + 0x20, 2);
        [[fallthrough]];
    }
    case 2: {
        const float grav = fpu_mul(fpu_mul(dt_sq(), 10.8f), 0.5f);
        swc1(pv + 8, fpu_sub(lwc1(pv + 8), grav));
        vec3_add(V(pos), V(pos), V(pv));
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0x10)));
        swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x14)));
        if (lwc1(pos) < 8.0f || lwc1(pos + 4) < 8.0f || 500.0f < lwc1(pos) || 500.0f < lwc1(pos + 4) ||
            500.0f < lwc1(pos + 8)) {
            moby_retire(m);
            break;
        }
        if (rand_mod(3) == 0) {
            vec_clear(V(sp + 0x10));
            sd(sp + 0x20, ld(L8_ADDR(0x00161c30)));
            const uint32_t pick = static_cast<uint32_t>(rand_mod(2)) << 2;
            const int32_t count = rand_range(frame_scale_i(0xf), frame_scale_i(0x14));
            const int32_t life = rand_range(frame_scale_i(0x19), frame_scale_i(0x1e));
            const float speed = fpu_mul(rand_range_f(8.0f, 16.0f), dt());
            spawn_particle_0b(pos, sp + 0x10, 0x2f3f3f7fu, lwu(sp + 0x20 + pick), static_cast<uint32_t>(count),
                              static_cast<uint32_t>(life), 0, 0, 400000.0f, speed);
        }
        if (!(lwc1(pos + 8) < 15.2f)) break;
        // splash on hitting the sea (sp+0x10 and sp+0x30 accumulate across iterations in the original's frame)
        const int32_t total = lw(L8_ADDR(0x00161c00));
        for (int32_t i = 0; i < total; ++i) {
            const float a = rand_angle();
            lq_sq(sp + 0x40, pos);
            swc1(sp + 0x40, fpu_add(lwc1(sp + 0x40), rand_range_f(-3.0f, 3.0f)));
            const float dy = rand_range_f(-3.0f, 3.0f);
            const float vz = fpu_mul(lwc1(L8_ADDR(0x00161c0c)), dt());
            swc1(sp + 0x44, fpu_add(lwc1(sp + 0x44), dy));
            swc1(sp + 0x18, vz);
            swc1(sp + 0x10, fpu_add(lwc1(sp + 0x10), fpu_mul(cos_vu(a), lwc1(L8_ADDR(0x00161c14)))));
            const float vy_add = fpu_mul(sin_vu(a), lwc1(L8_ADDR(0x00161c14)));
            const float dz = fpu_mul(lwc1(L8_ADDR(0x00161c10)), dt());
            swc1(sp + 0x14, fpu_add(lwc1(sp + 0x14), vy_add));
            swc1(sp + 0x38, dz);
            swc1(sp + 0x30, fpu_add(lwc1(sp + 0x30), fpu_mul(cos_vu(a), lwc1(L8_ADDR(0x00161c18)))));
            const float dy_add = fpu_mul(sin_vu(a), lwc1(L8_ADDR(0x00161c18)));
            swc1(sp + 0x1c, lwc1(L8_ADDR(0x00161c28)));
            swc1(sp + 0x34, fpu_add(lwc1(sp + 0x34), dy_add));
            swc1(sp + 0x3c, lwc1(L8_ADDR(0x00161c2c)));
            const auto span_frames = [](uint32_t addr) {
                const int32_t base = lw(addr);
                const float r = rand_range_f(static_cast<float>(base), static_cast<float>(base << 1));
                return static_cast<uint32_t>(float_to_int(frame_scale_f(r)));
            };
            const uint32_t n0 = span_frames(L8_ADDR(0x00161c1c));
            const uint32_t n1 = span_frames(L8_ADDR(0x00161c20));
            const uint32_t n2 = span_frames(L8_ADDR(0x00161c24));
            spawn_particle_02(sp + 0x40, sp + 0x10, sp + 0x30, lwu(L8_ADDR(0x00161c04)), lwu(L8_ADDR(0x00161c08)), n0, n1,
                              n2, -1);
        }
        sb(m + 0x20, 3);
        break;
    }
    case 3: {
        const float grav = fpu_mul(fpu_mul(dt_sq(), 10.8f), 0.5f);
        swc1(pv + 8, fpu_sub(lwc1(pv + 8), grav));
        vec3_add(V(pos), V(pos), V(pv));
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0x10)));
        swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x14)));
        if (lwc1(pos) < 8.0f || lwc1(pos + 4) < 8.0f || lwc1(pos + 8) < 8.0f || 500.0f < lwc1(pos) ||
            500.0f < lwc1(pos + 4))
            moby_retire(m);
        break;
    }
    default:
        break;
    }
    ee_stack_free(224);
}

// A piece of class `cls` spawned for `parent`, placed so its chain point `piece_joint` sits on `parent`'s chain point
// `parent_joint`, with `parent`'s rotation +0x40 and the hero moby's lighting +0x38. Returns the piece (0: none).
// orig: L8:0x002e1c98
uint32_t bat_piece_spawn_l8(uint32_t parent, uint32_t cls, uint32_t piece_joint, uint32_t parent_joint) {
    const uint32_t sp = ee_stack_alloc(112);
    const uint32_t at = sp, want = sp + 0x10;
    const uint32_t piece = moby_spawn(cls);
    if (piece != 0) {
        sh(piece + 0x32, 0xff);
        sb(piece + 0x30, 0xff);
        sb(piece + 0x31, 1);
        sh(piece + 0x34, lhu(piece + 0x34) | 0x4000u);
        sd(piece + 0x38, ld(lwu(HERO_MOBY) + 0x38));
        lq_sq(piece + 0x40, parent + 0x40);
        moby_chain_point_v(piece, piece_joint, V(at));
        moby_chain_point_v(parent, parent_joint, V(want));
        vec3_sub(V(piece + 0x10), V(want), V(at));
        moby_update_transform(piece);
    }
    ee_stack_free(112);
    return piece;
}

// The warship `m`'s thirty-four piece records (piece_rec): 0 the warship itself; 3 the nose gun (0x1c1, 0 on 2); 1 and 2
// the fins (0x1c2, 0 on 3 and 4, the second mirrored +0x34 |= 0x8000); 7, 8 and 9 the three drums (0x1c0: 0 on 0, 1 on 1,
// 1 on drum 8's 0); 5 the body (0x1bd, 0 on drum 9's 0); 6 the turret (0x1ba, 1 on drum 7's 1); 4 the turret gun (0x1b9,
// spawned against `m` as the original does, then parented in its record to the turret 0 on 0); 10 .. 17, 18 .. 25 and
// 26 .. 33 the three rings of eight pods (0x1bb, 0 on each drum's 2 .. 9).
// orig: L8:0x002e1d70
void warship_build_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    sw(pv + 0x60, m);
    sw(pv + 0x64, 0);
    sw(pv + 0x68, 0);
    sw(pv + 0x6c, 0);
    const uint32_t gun1 = bat_piece_spawn_l8(m, 0x1c1, 0, 2);
    sw(pv + 0x94, m);
    sw(pv + 0x98, 0);
    sw(pv + 0x9c, 2);
    sw(pv + 0x90, gun1);
    const uint32_t fin1 = bat_piece_spawn_l8(m, 0x1c2, 0, 3);
    sw(pv + 0x7c, 3);
    sw(pv + 0x74, m);
    sw(pv + 0x78, 0);
    sw(pv + 0x70, fin1);
    const uint32_t fin2 = bat_piece_spawn_l8(m, 0x1c2, 0, 4);
    sw(pv + 0x80, fin2);
    sh(fin2 + 0x34, lhu(fin2 + 0x34) | 0x8000u);  // (no test for a fin not spawned)
    sw(pv + 0x84, m);
    sw(pv + 0x88, 0);
    sw(pv + 0x8c, 4);
    const uint32_t drum1 = bat_piece_spawn_l8(m, 0x1c0, 0, 0);
    sw(pv + 0xd4, m);
    sw(pv + 0xd8, 0);
    sw(pv + 0xdc, 0);
    sw(pv + 0xd0, drum1);
    const uint32_t drum2 = bat_piece_spawn_l8(m, 0x1c0, 1, 1);
    sw(pv + 0xe4, m);
    sw(pv + 0xe8, 1);
    sw(pv + 0xec, 1);
    sw(pv + 0xe0, drum2);
    const uint32_t drum3 = bat_piece_spawn_l8(drum2, 0x1c0, 1, 0);
    sw(pv + 0xf8, 1);
    sw(pv + 0xfc, 0);
    sw(pv + 0xf4, lwu(pv + 0xe0));
    sw(pv + 0xf0, drum3);
    const uint32_t body = bat_piece_spawn_l8(drum3, 0x1bd, 0, 0);
    sw(pv + 0xb8, 0);
    sw(pv + 0xbc, 0);
    sw(pv + 0xb4, lwu(pv + 0xf0));
    sw(pv + 0xb0, body);
    const uint32_t turret = bat_piece_spawn_l8(lwu(pv + 0xd0), 0x1ba, 1, 1);
    sw(pv + 0xcc, 1);
    sw(pv + 0xc8, 1);
    sw(pv + 0xc4, lwu(pv + 0xd0));
    sw(pv + 0xc0, turret);
    const uint32_t gun2 = bat_piece_spawn_l8(m, 0x1b9, 0, 0);
    sw(pv + 0xa0, gun2);
    sw(pv + 0xa4, lwu(pv + 0xc0));
    sw(pv + 0xa8, 0);
    sw(pv + 0xac, 0);
    for (uint32_t ring = 0; ring < 3; ++ring) {
        const uint32_t drum_slot = pv + 0xd0 + ring * 0x10;
        for (uint32_t i = 0; i < 8; ++i) {
            const uint32_t rec = piece_rec(pv, 10 + ring * 8 + i);
            const uint32_t pod = bat_piece_spawn_l8(lwu(drum_slot), 0x1bb, 0, i + 2);
            sw(rec, pod);
            sw(rec + 4, lwu(drum_slot));
            sw(rec + 8, 0);
            sw(rec + 0xc, i + 2);
        }
    }
}

// The warship `m`'s pieces (records 1 .. 33) kept on their parents' chain points, the three drums spun (+0x40) by
// +-0x161c3c degrees a second (the middle drum opposite), the nose gun (+0x90) aimed across at Ratchet, and each piece's
// transform updated.
// orig: L8:0x002e2078
void warship_pieces_follow_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(192);
    const uint32_t at = sp, want = sp + 0x10, gap = sp + 0x20;
    for (uint32_t k = 1; k <= kWarshipPieces; ++k) {
        const uint32_t rec = piece_rec(pv, k);
        const uint32_t piece = lwu(rec);
        if (piece == 0) continue;
        swc1(piece + 0x48, lwc1(m + 0x48));
        moby_chain_point_v(piece, lwu(rec + 8), V(at));
        moby_chain_point_v(lwu(rec + 4), lwu(rec + 0xc), V(want));
        vec3_sub(V(gap), V(want), V(at));
        vec3_add(V(piece + 0x10), V(piece + 0x10), V(gap));
    }
    const auto spin_drum = [&](uint32_t slot, bool reverse) {
        if (lwu(slot) == 0) return;
        float step = fpu_mul(fpu_mul(lwc1(L8_ADDR(0x00161c3c)), kDegToRad), dt());
        if (reverse) step = -step;
        const float roll = angle_add(lwc1(lwu(slot) + 0x40), step);
        swc1(lwu(slot) + 0x40, roll);
    };
    spin_drum(pv + 0xd0, false);
    spin_drum(pv + 0xe0, true);
    spin_drum(pv + 0xf0, false);
    const uint32_t gun1 = lwu(pv + 0x90);
    if (gun1 != 0) {
        const float yaw = heading(lwc1(gun1 + 0x10), lwc1(gun1 + 0x14), lwc1(HERO_POS), lwc1(HERO_POS + 4));
        swc1(lwu(pv + 0x90) + 0x48, yaw);
    }
    for (uint32_t k = 1; k <= kWarshipPieces; ++k) {
        const uint32_t piece = lwu(piece_rec(pv, k));
        if (piece != 0) moby_update_transform(piece);
    }
    ee_stack_free(192);
}

// The warship `m`'s hits (mask 0x10000 on itself and its 33 pieces; each hit piece gets +0xa4 0xff): the tally +0x28c up
// by 1 on records 0 .. 6, 2 on 7 .. 9 (the drums; the larger only), 4 more for a pod (10 .. 33, the first pod hit), the
// sound 1 (2 for a pod). At 8 or more the tally keeps its remainder by 8 and a pod breaks off (the pod hit, else one from
// a random place in 10 .. 33; no sound then); with no pod left the warship is destroyed (state 99, sound 0).
// orig: L8:0x002e2250
void warship_hits_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    int32_t tally = 0;
    int32_t sound = -1;
    uint32_t pod_hit = 0;
    for (uint32_t k = 0; k < 34; ++k) {
        const uint32_t rec = piece_rec(pv, k);
        if (lwu(rec) == 0 || sub_0025a420(lwu(rec), 0x10000, 0) == 0) continue;
        if (k < 7) {
            if (tally <= 0) {
                tally = 1;
                sound = 1;
            }
        } else if (k < 10) {
            if (tally < 2) {
                tally = 2;
                sound = 1;
            }
        } else if (tally < 4) {
            tally += 4;
            pod_hit = k;
            sound = 2;
        }
        sb(lwu(rec) + 0xa4, 0xff);
    }
    const int16_t hits = static_cast<int16_t>(lhu(pv + 0x28c) + static_cast<uint32_t>(tally));
    sh(pv + 0x28c, static_cast<uint32_t>(hits));
    if (hits >= 8) {
        sound = 0;
        sh(pv + 0x28c, static_cast<uint32_t>(hits % 8));
        if (pod_hit != 0) {
            break_off(m, piece_rec(pv, pod_hit));
            return;
        }
        const int32_t from = float_to_int(rand_range_f(0.0f, 23.0f));
        for (int32_t i = 0; i < 24; ++i) {
            const uint32_t rec = piece_rec(pv, static_cast<uint32_t>(ee_rem(i + from, 24) + 10));
            if (lwu(rec) != 0) {
                break_off(m, rec);
                return;
            }
        }
        sb(m + 0x20, 0x63);
    }
    if (sound >= 0) moby_sound_start(sound, 0, m);
}

// Class 444, the three-drum Blarg warship. pvars: +0x00 its attack run (a follower: +0x00 point, +0x04 direction, +0x10
// path, +0x14 path index), +0x30 its circuit (+0x30, +0x34, +0x40, +0x44), +0x60 .. +0x27f its thirty-four piece records
// (warship_build_l8), +0x280 its target's moby index, +0x284 its yaw rate, +0x288 its circuit shot timer (a 32-bit word,
// 300 .. 600 frames), +0x28c its hits' tally, +0x28e its engine sound's slot. With 0x161c4c set nothing runs. States: 0
// set up (retired when its level flag +0xb0 at 0x14c050 is 0xff): pieces built on its circuit's start, facing point 1,
// state 2; 1 attack run: turns (0.004 / 0.3 / 0.02), hits, pieces, steps at 0x161c38 a second, and when ordered (+0xbc)
// fires a homing missile (class 435) from one of its two guns (+0x90 / +0xa0 at random) into the target's slots +0x60;
// 2 circuit: seeks the next point's heading (yaw_seek, 0x161c44 / 0x161c48 deg), steps on, pieces, and on +0x288 fires a
// straight shot (state 5) from +0xa0; 3 jumps to its attack run's start (roll cleared) -> 1; 99 flings all 33 pieces off
// with bursts, bursts and retires.
// orig: L8:0x002e24e8
void bat_class444_update_l8(uint32_t m) {
    if (lw(L8_ADDR(0x00161c4c)) != 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (sub_0028d8c0(m, lh(pv + 0x28e)) == 0) sh(pv + 0x28e, static_cast<uint32_t>(moby_sound_start(3, 4, m)));

    const uint32_t sp = ee_stack_alloc(240);
    const uint32_t goal = sp, step = sp + 0x10, muzzle = sp + 0x20, vel = sp + 0x30;
    const uint32_t pos = m + 0x10;
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002e2598) set up
        if (lbu(0x0014c050u + (lwu(0x0015ed84) << 4) + lbu(m + 0xb0)) == 0xff) {
            moby_retire(m);
            break;
        }
        sw(pv + 0x10, lvd::path(lw(pv + 0x14)));
        sw(pv + 0x40, lvd::path(lw(pv + 0x44)));
        sw(pv + 0x288, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(300.0f, 600.0f)))));
        lq_sq(pos, lwu(pv + 0x40) + 0x10);
        warship_build_l8(m);
        sb(m + 0x20, 2);
        const uint32_t p1 = path_pt(lwu(pv + 0x40), 1);
        swc1(m + 0x48, heading(lwc1(pos), lwc1(pos + 4), lwc1(p1), lwc1(p1 + 4)));
        sh(m + 0x34, lhu(m + 0x34) | 0x4000u);
        break;
    }
    case 1: {  // (0x002e2680) the attack run
        const int32_t next = next_point(pv);
        const uint32_t target = lwu(L8_ADDR(0x0015ffd8)) + (lwu(pv + 0x280) << 8);
        const uint32_t p = path_pt(lwu(pv + 0x10), next);
        yaw_turn_damped(m, pv + 0x284, heading(lwc1(pos), lwc1(pos + 4), lwc1(p), lwc1(p + 4)), 0.004f, 0.3f, 0.02f);
        warship_hits_l8(m);
        warship_pieces_follow_l8(m);
        lq_sq(goal, path_pt(lwu(pv + 0x10), next));
        if (vec3_distance(V(pos), V(goal)) < 0.5f) sw(pv, static_cast<uint32_t>(next));
        step_to(m, goal, step);
        if (lbu(m + 0xbc) == 0) break;
        sb(m + 0xbc, 0);
        const uint32_t gun = (rand_mod(0xff) & 1) != 0 ? lwu(pv + 0x90) : lwu(pv + 0xa0);
        moby_chain_point_v(gun, 1, V(muzzle));
        const float climb = rand_range_f(u2f(0x3e32b8c2u), u2f(0x3f060a92u));  // (10 .. 30 degrees)
        const float away =
            angle_add(heading(lwc1(pos), lwc1(pos + 4), lwc1(target + 0x10), lwc1(target + 0x14)), kPi);
        float yaw = angle_add(away, rand_range_f(u2f(0xbe860a92u), u2f(0x3f490fdbu)));  // (-15 .. 45 degrees)
        if (0.0f < yaw) {
            if (yaw < 0.34f) yaw = 0.34f;
        } else if (yaw <= 0.0f) {
            if (-0.34f < yaw) yaw = -0.34f;
        }
        const float speed = lwc1(L8_ADDR(0x00161c38));
        vec3_set_length(V(vel), V(step), fpu_mul(fpu_add(speed, speed), dt()));
        const uint32_t shot = bat_class435_spawn_l8(muzzle, target, vel, climb, yaw);
        swc1(shot + 0x2c, fpu_div(lwc1(lwu(shot + 0x24) + 0x24), 5.0f));  // (no test for a shot not spawned)
        const uint32_t tpv = lwu(target + 0x78);
        for (uint32_t i = 0; i < 8; ++i) {
            if (lwu(tpv + 0x60 + i * 4) == 0) {
                sw(tpv + 0x60 + i * 4, shot);
                break;
            }
        }
        break;
    }
    case 2: {  // (0x002e299c) the circuit
        const uint32_t rec = pv + 0x30;
        const uint32_t path = lwu(rec + 0x10);
        const int32_t at = lw(rec);
        const int32_t n = lw(path);
        const int32_t next = ee_rem(at + 1, n);
        // (the original also computes class 462's two-leg eased heading here and discards it in favour of yaw_seek)
        const uint32_t p1 = path_pt(path, next);
        const float want = heading(lwc1(pos), lwc1(pos + 4), lwc1(p1), lwc1(p1 + 4));
        const float accel = fpu_mul(fpu_mul(lwc1(L8_ADDR(0x00161c44)), kDegToRad), dt_sq());
        const float vmax = fpu_mul(fpu_mul(lwc1(L8_ADDR(0x00161c48)), kDegToRad), dt());
        yaw_seek(m + 0x48, pv + 0x284, want, accel, accel, vmax);
        lq_sq(goal, path_pt(lwu(rec + 0x10), next));
        if (vec3_distance(V(pos), V(goal)) < 0.5f) sw(rec, static_cast<uint32_t>(next));
        step_to(m, goal, step);
        warship_pieces_follow_l8(m);
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x288))) == 0) break;
        sw(pv + 0x288, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(300.0f, 600.0f)))));
        moby_chain_point_v(lwu(pv + 0xa0), 1, V(muzzle));
        const float dive = rand_range_f(u2f(0xbf060a92u), u2f(0xbe860a92u));  // (-30 .. -15 degrees)
        const float yaw = rand_angle();
        const float speed = lwc1(L8_ADDR(0x00161c38));
        vec3_set_length(V(vel), V(step), fpu_mul(fpu_add(speed, speed), dt()));
        const uint32_t shot = bat_class435_spawn_l8(muzzle, 0, vel, dive, yaw);
        if (shot == 0) break;
        const float gun_yaw = lwc1(lwu(pv + 0xa0) + 0x48);
        sb(shot + 0x20, 5);
        swc1(shot + 0x48, gun_yaw);
        sw(shot + 0x44, 0);
        break;
    }
    case 3: {  // (0x002e2c68) back to the attack run
        sb(m + 0x20, 1);
        const uint32_t path = lwu(pv + 0x10);
        lq_sq(pos, path + 0x10);
        const uint32_t p1 = path_pt(path, 1);
        const float yaw = heading(lwc1(pos), lwc1(pos + 4), lwc1(p1), lwc1(p1 + 4));
        sw(m + 0x40, 0);
        swc1(m + 0x48, yaw);
        warship_pieces_follow_l8(m);
        break;
    }
    case 0x63:  // (0x002e2cb8) destroyed
        for (uint32_t k = 1; k <= kWarshipPieces; ++k) {
            const uint32_t rec = piece_rec(pv, k);
            if (lwu(rec) != 0) fling(m, rec, pos, 3.0f, 5.0f);
        }
        explosion_burst(m, pos, -1, 5.0f, 13.0f);
        moby_retire(m);
        break;
    default:
        break;
    }
    ee_stack_free(240);
}

}  // namespace rac1
