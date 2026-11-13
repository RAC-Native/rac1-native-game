// The levels' own sound emitter types (the update functions of their emitters, +4 of the 0x90-byte records at
// *(0x13f2e4), from section 5 of the overlay: types 1 / 3 / 4 / 5 / 6; Veldin's types 0 / 2 are sound.cpp's) and the
// requests they make (a slot's volume, the reverb, the music track). Every level that has one of these functions has the
// same code, relinked: one port serves them all. What they touch is the base ELF's (the hero 0x13f3d0, the sound slots
// 0x13e5c0, the reverb request 0x13e5b0, the music streams 0x1516d0: bare) and the engine's data that every overlay moves
// the same way (the camera 0x166dc0, the level's sound list 0x15f5f4: OV; the height map through ground_height_at,
// shared_height_map.cpp); the one level-only word is type 4's (L8 / L14). The dispatch is each level's own
// (`*_sound_emitter_call`, sound.cpp). Header sound_emitter.h. An emitter's record: its update +4, its parameters +8, a size +0xc, its matrix +0x10, its position
// +0x40, the matrix's inverse +0x50. The sound slots are 0x70 bytes: +4 playing, +0x10 volume, +0x1c emitter, +0x20 position.
#include <game/ovl/sound_emitter.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_height_map.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

namespace {
constexpr uint32_t SLOTS = 0x0013e5c0;   // the sound slots (0x70 bytes)
constexpr uint32_t REVERB = 0x0013e5b0;  // the reverb request: its owner +0, depth +4, type +8, delay +9, feedback +0xa, changes +0xb
constexpr uint32_t STREAMS = 0x001516d0; // the music streams: the next track +0x22 / +0x23, the track +0x38, the two pauses +0x3e / +0x76
constexpr uint32_t HERO_POS = 0x0013f3d0;

uint32_t slot_at(int32_t slot) { return SLOTS + static_cast<uint32_t>(slot) * 0x70; }

// the camera (its position +0, its "ambience on" word +0x254)
uint32_t camera() { return OV(0x00166dc0); }

// `p` - the emitter's position, in the emitter's frame (its inverse matrix +0x50); w cleared first as the original does
Vec4 to_local(uint32_t em, const Vec4& p) {
    Vec4 d{};
    vec3_sub(d, p, V(em + 0x40));
    d.w = 0.0f;
    Vec4 local{};
    mat3_transform(local, d, M(em + 0x50));
    return local;
}

bool inside_unit_box(const Vec4& v) {
    return abs_f(v.x) <= 1.0f && abs_f(v.y) <= 1.0f && abs_f(v.z) <= 1.0f;
}

// the original's clamp of one coordinate to [-1, 1] (only a value past the bound is written)
void clamp_unit(float& v) {
    if (1.0f < v) v = 1.0f;
    else if (v < -1.0f) v = -1.0f;
}

// type 4's level-only word: how open the ground around the camera is, smoothed (0..1)
uint32_t openness_word() { return on_level(OLTANIS) ? L14_ADDR(0x001624d0) : L8_ADDR(0x001625e8); }
}  // namespace

// A sound slot's volume (+0x10 of the slot); returns 1.
// orig: L1:0x002a1968 L2:0x0028d618 L4:0x0027ee10 L5:0x002b65f8 L8:0x00296230 L9:0x002aa7f8 L11:0x002b0e10 L12:0x002a4730 L13:0x00298328 L14:0x00294220 L16:0x00286d80
uint32_t sound_slot_volume_set(int32_t slot, uint32_t volume) {
    sw(slot_at(slot) + 0x10, volume);
    return 1;
}

// The reverb request (sent by the sound frame): `owner` the emitter that asks, and each setting that changes marks its
// bit in +0xb (1 the type, 4 the depth, 2 the delay and feedback).
// orig: L1:0x002a1a90 L2:0x0028d740 L3:0x0027a918 L5:0x002b6720 L7:0x002b4c78 L8:0x00296358 L9:0x002aa920 L10:0x0027ead0 L12:0x002a4858 L14:0x00294348 L15:0x0027bf18 L16:0x00286ea8 L18:0x0028db78
void reverb_request(uint32_t owner, uint32_t type, uint32_t depth, uint32_t delay, uint32_t feedback) {
    sw(REVERB, owner);
    if (lbu(REVERB + 8) != type) {  // (the stored bytes compared with the whole arguments, as the original does)
        sb(REVERB + 8, type);
        sb(REVERB + 0xb, lbu(REVERB + 0xb) | 1u);
    }
    if (lwu(REVERB + 4) != depth) {
        sw(REVERB + 4, depth);
        sb(REVERB + 0xb, lbu(REVERB + 0xb) | 4u);
    }
    if (lbu(REVERB + 9) != delay || lbu(REVERB + 0xa) != feedback) {
        sb(REVERB + 0xa, feedback);
        sb(REVERB + 9, delay);
        sb(REVERB + 0xb, lbu(REVERB + 0xb) | 2u);
    }
}

// The music track `track`: nothing when it already plays (and no track is waiting: +0x22 is -1); while the music is
// paused (+0x3e or +0x76) it waits in +0x22 with `next` in +0x23, else it is the track (+0x38) at once.
// orig: L1:0x0027a248 L2:0x00266d20 L3:0x00253820 L4:0x00257ed8 L5:0x0028f5c8 L6:0x00274450 L7:0x0028ddb0 L8:0x0026f5d8 L10:0x00258898 L11:0x0028aa38 L12:0x0027dd40 L13:0x00271070 L14:0x0026cfd0 L15:0x00254c50 L16:0x00260aa8 L17:0x0025f448 L18:0x00267ae8
void music_track_request(int32_t track, uint32_t next) {
    if (lh(STREAMS + 0x38) == track && static_cast<int8_t>(lbu(STREAMS + 0x22)) == -1) return;
    if (lh(STREAMS + 0x3e) != 0 || lh(STREAMS + 0x76) != 0) {
        sb(STREAMS + 0x23, next);
        sb(STREAMS + 0x22, static_cast<uint32_t>(track));
        return;
    }
    sh(STREAMS + 0x38, static_cast<uint32_t>(track));
}

// Type 1: a box emitter whose sound fades out from its core: the camera in the box (the size +0xc turned by the inverse
// matrix) plays the sound (params: sound +0, interval +4..+8, timer +0xc, slot +0x10) at the sound's own volume inside
// the unit core, at a volume by distance (0x28c8b0) between the core and the box's edge; the slot's position follows
// the camera's place in the box (clamped to the core). Leaving the box stops it.
// orig: L1:0x00319928 L5:0x0032b478 L8:0x00316aa8 L9:0x003158e8 L11:0x00328b50 L12:0x00316680 L13:0x00318360 L14:0x00316990 L16:0x002f58a0
void emitter_fade_box_update(uint32_t em) {
    const uint32_t prm = lwu(em + 8);
    const float h = lwc1(em + 0xc);
    Vec4 local = to_local(em, V(camera()));
    const Vec4 ext{h, h, h, 0.0f};
    Vec4 edge{};
    mat3_transform(edge, ext, M(em + 0x50));
    const float edge_len = vec3_length(edge);
    const float dist = vec3_length(local);
    if (!(dist < edge_len)) {
        const int32_t slot = lw(prm + 0x10);
        if (slot != -1 && emitter_playing(em, prm)) sub_0028d918(slot);
        sw(prm + 0x10, 0xffffffffu);
        return;
    }
    const uint32_t snd = lwu(OV(0x0015f5f4)) + (lwu(prm) << 5);  // the sound's entry in the level's list
    uint32_t volume;
    if (inside_unit_box(local)) {
        volume = lwu(snd + 0xc);
    } else {
        clamp_unit(local.x);
        clamp_unit(local.y);
        clamp_unit(local.z);
        volume = static_cast<uint32_t>(sub_0028c8b0(snd, dist, vec3_length(local), edge_len));
    }
    const uint32_t flags = lbu(snd + 0x18) != 0 ? 0x14u : 0x10u;
    if (!emitter_playing(em, prm)) {
        int32_t& timer = *static_cast<int32_t*>(ee_ram_ptr(prm + 0xc));
        if (timer_tick_i32(timer) == 0) {
            sw(prm + 0x10, 0xffffffffu);
            return;
        }
        sw(prm + 0x10, static_cast<uint32_t>(level_sound_play(lw(prm), flags, em, static_cast<int32_t>(volume))));
        emitter_rearm(prm);
    }
    const int32_t slot = lw(prm + 0x10);
    if (slot == -1) return;
    sound_slot_volume_set(slot, volume);
    Vec4 at{};
    mat3_transform(at, local, M(em + 0x10));
    vec3_add(at, at, V(em + 0x40));
    V(slot_at(lw(prm + 0x10)) + 0x20) = at;
}

// Type 3: a box emitter that sets the reverb (params: type +0, depth +4, delay +1, feedback +2, set +3): with the hero
// in the box the depth grows from the box's -x face to its +x face; leaving it through +x keeps the reverb (its owner
// none), through the other faces turns it off.
// orig: L1:0x00319f18 L2:0x002fc6f0 L3:0x002ed800 L5:0x0032ba68 L7:0x00329740 L8:0x00316e48 L9:0x00315c88 L10:0x002f6e58 L12:0x00316a20 L14:0x00316d30 L15:0x002f9230 L16:0x002f5c40 L18:0x00306dd0
void emitter_reverb_box_update(uint32_t em) {
    const uint32_t prm = lwu(em + 8);
    const Vec4 local = to_local(em, V(HERO_POS));
    if (inside_unit_box(local)) {
        const float t = fpu_mul(fpu_add(local.x, 1.0f), 0.5f);
        const int32_t full = lw(prm + 4);
        const int32_t depth = float_to_int(fpu_mul(int_to_float(full), t));
        reverb_request(em, lbu(prm), static_cast<uint32_t>(full < depth ? full : depth), lbu(prm + 1), lbu(prm + 2));
        sb(prm + 3, 1);
        return;
    }
    if (lbu(prm + 3) == 0) return;
    if (0.0f < local.x) reverb_request(0, lbu(prm), lwu(prm + 4), lbu(prm + 1), lbu(prm + 2));
    else reverb_request(em, 0, 0, 0, 0);
    sb(prm + 3, 0);
}

// Type 4: an ambience bed at the camera whose volume follows how open the ground around it is (params: sound +0, the
// volume's floor +4 as a float, slot +8). Twenty points on a circle of radius 4 around the camera are tested against the
// height map: each whose ground lies below the camera counts 0.05. The level's smoothed openness (its own word) moves
// toward that count (up at 0.2, down at 0.75 per unit of 0x15ed7c) within [0, 1]; a playing slot gets the volume
// (floor + openness) / (floor + 1) of 1024 and the camera's position; a stopped one is started again (flags 0x15).
// orig: L8:0x00316fa8 L14:0x00316e90
void emitter_open_bed_update(uint32_t em) {
    const uint32_t prm = lwu(em + 8);
    const uint32_t cam = camera();
    const uint32_t open_at = openness_word();
    const Vec4 arm{4.0f, 0.0f, 0.0f, 1.0f};  // (y / z: stack words the original never writes; zero in the port's frame)
    const Vec4 up{0.0f, 0.0f, 1.0f, 0.0f};
    float count = 0.0f;
    for (float angle = 0.0f; angle < 6.2831854820251465f; angle = fpu_add(angle, 0.3141592741012573f)) {
        Vec4 p{};
        vec_rotate_axis(p, arm, up, angle);
        vec3_add(p, V(cam), p);
        if (ground_height_at(p) < lwc1(cam + 8)) count = fpu_add(count, 0.05000000074505806f);
    }
    const float was = lwc1(open_at);
    const float dt = lwc1(0x0015ed7c);
    float open = was < count ? fpu_add(was, fpu_mul(dt, 0.2f)) : fpu_sub(was, fpu_mul(dt, 0.75f));
    swc1(open_at, open);
    open = lwc1(open_at);
    if (1.0f < open) swc1(open_at, 1.0f);
    else if (open < 0.0f) swc1(open_at, 0.0f);

    const int32_t slot = lw(prm + 8);  // (-1 too: the original reads and writes the slot below the table then)
    if (lwu(slot_at(slot) + 0x1c) == em && lbu(slot_at(slot) + 4) != 0) {
        const float floor = lwc1(prm + 4);
        const float share = fpu_div(fpu_add(floor, lwc1(open_at)), fpu_add(floor, 1.0f));
        sound_slot_volume_set(slot, static_cast<uint32_t>(float_to_int(fpu_mul(share, 1024.0f))));
    }
    const int32_t now = lw(prm + 8);
    V(slot_at(now) + 0x20) = V(cam);
    if (lwu(slot_at(now) + 0x1c) == em && lbu(slot_at(now) + 4) != 0) return;
    sw(prm + 8, static_cast<uint32_t>(level_sound_play(lw(prm), 0x15, em, 0x400)));
}

// Type 5: a sound that follows the camera (params: sound +0, slot +4): its slot is put at the camera each frame; it
// plays (flags 0x15) while the camera's +0x254 is set and stops when it is clear.
// orig: L1:0x0031a078 L5:0x0032bbc8 L11:0x00328ef0 L12:0x00316b80 L15:0x002f9390 L17:0x00300e70 L18:0x00306f30
void emitter_camera_bed_update(uint32_t em) {
    const uint32_t prm = lwu(em + 8);
    const int32_t slot = lw(prm + 4);
    V(slot_at(slot) + 0x20) = V(camera());  // (also for no slot, -1: the original writes the word below the table)
    const bool playing = lwu(slot_at(slot) + 0x1c) == em && lbu(slot_at(slot) + 4) != 0;
    if (lw(camera() + 0x254) != 0) {
        if (!playing) sw(prm + 4, static_cast<uint32_t>(level_sound_play(lw(prm), 0x15, em, 0x400)));
    } else if (playing) {
        sub_0028d918(slot);
        sw(prm + 4, 0xffffffffu);
    }
}

// Type 6: a box that changes the music track as the hero leaves it (params: the tracks +0 / +4, their next tracks as
// halves +0xc / +0xe (-1: the other one), in the box +8): leaving through +x plays the second track, through the other
// faces the first. Both halves -1: nothing.
// orig: L1:0x0031a128 L2:0x002fc850 L3:0x002ed960 L4:0x002f1988 L5:0x0032bc78 L7:0x003298a0 L8:0x00317230 L12:0x00316c30 L14:0x00317118 L16:0x002f5da0 L18:0x00306fe0
void emitter_music_box_update(uint32_t em) {
    const uint32_t prm = lwu(em + 8);
    if (lw(prm + 0xc) == -1) return;
    const Vec4 local = to_local(em, V(HERO_POS));
    if (inside_unit_box(local)) {
        sw(prm + 8, 1);
        return;
    }
    if (lw(prm + 8) == 0) return;
    if (lh(prm + 0xe) == -1) sh(prm + 0xe, lhu(prm + 0xc));
    if (lh(prm + 0xc) == -1) sh(prm + 0xc, lhu(prm + 0xe));
    if (0.0f < local.x) music_track_request(lw(prm + 4), static_cast<uint32_t>(static_cast<int32_t>(lh(prm + 0xe))));
    else music_track_request(lw(prm), static_cast<uint32_t>(static_cast<int32_t>(lh(prm + 0xc))));
    sw(prm + 8, 0);
}

}  // namespace rac1
