// Oltanis (level 14), classes 851 and 924: two people the hero talks to (the talk system, talk_init / talk_update on
// their pvars: the name +0x20, the choice +4, the first-time flag +8, the talk camera's distance +0xc).
// Class 851 gives the hero item 0x20 (the PDA) and runs the gondola (class 685, its moby index at pvars +0x44). Set up
// (state 0) unless the level's byte by its uid (0x1bb930 + 0x454) or its save bit (0x14c190) says it is done, or its
// gondola or checkpoint box (+0x48) is missing. Talking (1): the hero in its box (+0x40) the first time takes the
// checkpoint at the box +0x48 and readies the gondola (its direction byte +0xbc by the ride's sense, its +0x124 set);
// the talk done, the hero walks to a spot 3 in front (the level's copy of hero_walk_to, kept generated) and it waits
// for the scene's end (2). Then a choice of 3 or 4 is the deal: the item given with its message, the gondola done (its
// bits set), its own save bits set, the level change asked for and it goes; other choices talk again (1).
// Class 924 gives the coordinates of level 15. Set up as class 851, and gone as well when the story flag 0x13dd4f is
// set. Talking (1): the hero in its box the first time takes the checkpoint at the box +0x44; the talk done, the scene
// is told to put the hero at the box +0x48 (its place and its +0x78 heading; the walk-to flag of the scene state, a
// halfword at +0x46 -- a byte at +0x4a on the PAL 2.00 disc, whose own code it is) and it waits for the scene's end
// (2). Then a choice of 4 marks level 15 visited with its message, sets its save bits, asks for the level change and
// goes; other choices talk again.
// Both turn their heads toward the hero's target within 8 (as long as he moves, then 120 frames more) or a wandering
// point 6 away every 180 .. 300 frames, sprung on their two joint nodes. Ported by hand from Oltanis's own code
// (L14:0x2fba20, 0x2ef578, 0x2ef5a8, 0x2fefe0), the original's order of calls and float operations kept; the talks'
// stream waits are the talk system's.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/level_change.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/region_core.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants_l14.h>

#include <cstdint>

namespace rac1 {

namespace {
struct Frame {  // an EE stack frame of the original's size (its locals' offsets kept)
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
    uint32_t n, sp;
};

uint32_t level_moby(int32_t index) { return lwu(L14_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(index) << 8); }
uint32_t save_bits() { return 0x0014c190u + (lwu(0x0015ed84u) << 8); }

// a moby's bit by its uid in a table of words (the save's bits of this level, the level's own)
void set_uid_bit(uint32_t table, uint32_t m) {
    const uint32_t uid = lhu(m + 0xb2);
    const uint32_t w = table + (static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(uid)) >> 5) << 2);
    sw(w, lwu(w) | (1u << (uid & 0x1f)));
}

// done before: the level's byte by the uid, or the save's bit
bool talker_done(uint32_t m) {
    const uint32_t uid = lhu(m + 0xb2);
    const int32_t id = static_cast<int16_t>(uid);
    if (lbu(L14_ADDR(0x001bb930) + static_cast<uint32_t>(id) + 0x454) != 0) return true;
    return ((lw(save_bits() + (static_cast<uint32_t>(id >> 5) << 2)) >> (uid & 0x1f)) & 1) != 0;
}

// the hero in the talker's box the first time: the checkpoint at the box in pvars `box_word`, unless the talker's slot
// of things done (0x14c050) says it was taken
void talker_checkpoint(uint32_t m, uint32_t pv, uint32_t box_word) {
    const uint32_t idx = lbu(m + 0xb0);
    if (lbu(0x0014c050u + idx + (lwu(0x0015ed84u) << 4)) == 0xff) return;
    checkpoint_mark_collected(idx);
    const uint32_t box = lvd::box(lvd::BOXES, lw(pv + box_word));
    checkpoint_save(box + 0x30, box + 0x70);
}

// The head's pvars: the wandering point, the follow timer, the wander timer, the pitch, the turn's two shares, the
// talk's raise (2.75 while 0x15edb0 is set) and the two joint nodes.
struct Head {
    uint32_t point, follow, wander, pitch, turn1, turn2, raise, node0, node1;
};

// orig: L14:0x002fbd50 (class 851's) L14:0x002ff2a0 (class 924's)
void talker_head(uint32_t m, uint32_t pv, uint32_t sp, const Head& h) {
    float k = u2f(0x3ca3d70au), d = u2f(0x3e99999au);
    if (lbu(m + 0x53) == 0) {
        bool near = false;
        if (vec2_distance(V(m + 0x10), V(0x0013f3d0u)) < 8.0f) {
            const float yaw = atan2_f(fpu_sub(lwc1(0x0013f420u), lwc1(m + 0x10)), fpu_sub(lwc1(0x0013f424u), lwc1(m + 0x14)));
            if (angle_dist(lwc1(m + 0x48), yaw) < u2f(0x3fc90fdbu)) {
                near = true;
                if (u2f(0x3c23d70au) < vec3_length(V(0x0013f450u))) sw(pv + h.follow, static_cast<uint32_t>(frame_scale_i(0x78)));
                else timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + h.follow)));
            }
        }
        if (!near && lw(pv + h.follow) != 0) {
            sw(pv + h.follow, 0);
            lq_sq(pv + h.point, 0x0013f420u);
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + h.wander))) != 0) {
            const float deg = u2f(0x3c8efa35u);
            sw(pv + h.wander, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 300.0f)))));
            const float yaw = angle_add(lwc1(m + 0x48), fpu_mul(rand_range_f(-90.0f, 90.0f), deg));
            const float tilt = fpu_mul(rand_range_f(0.0f, 30.0f), deg);
            vec_from_spherical_v(V(pv + h.point), 6.0f, yaw, tilt);
            vec3_add(V(pv + h.point), V(pv + h.point), V(m + 0x10));
        }
        if (lw(pv + h.follow) != 0) {
            lq_sq(sp, 0x0013f420u);
            k = u2f(0x3d23d70au);
            d = u2f(0x3e99999au);
        } else {
            lq_sq(sp, pv + h.point);
        }
        lq_sq(sp + 0x10, m + 0x10);
        swc1(sp + 0x18, fpu_add(lwc1(sp + 0x18), 1.0f));
        vec3_sub(V(sp + 0x20), V(sp), V(sp + 0x10));
        float turn = angle_sub(atan2_f(lwc1(sp + 0x20), lwc1(sp + 0x24)), lwc1(m + 0x48));
        float pitch = -atan2_f(vec2_length(V(sp + 0x20)), lwc1(sp + 0x28));
        if (u2f(0x3fc90fdbu) < turn) turn = u2f(0x3fc90fdbu);
        else if (turn < u2f(0xbfc90fdbu)) turn = u2f(0xbfc90fdbu);
        if (u2f(0x3f060a92u) < pitch) pitch = u2f(0x3f060a92u);
        else if (pitch < u2f(0xbf060a92u)) pitch = u2f(0xbf060a92u);
        swc1(pv + h.pitch, pitch);
        swc1(pv + h.turn1, fpu_mul(turn, u2f(0x3f19999au)));
        swc1(pv + h.turn2, fpu_mul(turn, u2f(0x3ecccccdu)));
    }
    if (lbu(0x0015edb0u) != 0) swc1(pv + h.raise, 2.75f);
    joint_node_spring(m, pv + h.node0, 0, fpu_mul(k, lwc1(0x0015ed64u)), fpu_mul(d, lwc1(0x0015ed64u)));
    joint_node_spring(m, pv + h.node1, 1, fpu_mul(k, lwc1(0x0015ed64u)), fpu_mul(d, lwc1(0x0015ed64u)));
}

// The gondola readied for the ride: its direction byte (+0xbc: 2 backward, 1 forward, by its pvar +0xac's sign) and
// its waiting flag (+0x124).
// orig: L14:0x002ef578
void gondola_ready(uint32_t g) {
    const uint32_t gp = lwu(g + 0x78);
    sb(g + 0xbc, lwc1(gp + 0xac) < 0.0f ? 2u : 1u);
    sh(gp + 0x124, 1);
}

// The gondola done: its byte and flag cleared, its bits set in the save's and the level's tables.
// orig: L14:0x002ef5a8
void gondola_done(uint32_t g) {
    const uint32_t gp = lwu(g + 0x78);
    sb(g + 0xbc, 0);
    sh(gp + 0x124, 0);
    set_uid_bit(save_bits(), g);
    set_uid_bit(L14_ADDR(0x001babd0), g);
}
}  // namespace

// orig: L14:0x002fba20
void olt_class851_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0xb0);
    class_mates_mark_fixed(m, 0);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L14_ADDR(0x001674c0))) < 28.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x16);
    }
    switch (lbu(m + 0x20)) {
    case 0:  // (0x002fbadc) set up
        if (talker_done(m) || lw(pv + 0x44) == -1 || lw(pv + 0x48) == -1) {
            moby_retire(m);
            return;
        }
        sw(pv + 0x20, L14_ADDR(0x00161f20));
        talk_init(m, pv);
        sb(pv + 8, 1);
        sb(m + 0x20, 1);
        break;
    case 1:  // (0x002fbb68) talking
        if (lbu(pv + 8) == 1 && point_in_box(0x0013f3d0u, lw(pv + 0x40))) {
            talker_checkpoint(m, pv, 0x48);
            gondola_ready(level_moby(lw(pv + 0x44)));
            swc1(pv + 0xc, 255.0f);
        } else {
            swc1(pv + 0xc, u2f(0x3fd9999au));
        }
        if (talk_update(m, pv) == 0) break;
        olt_26b390_l14(m, 0, 0, 0, 0, 0, 0, 0, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);  // (hero_walk_to: 3 in front)
        sb(m + 0x20, 2);
        break;
    case 2: {  // (0x002fbc44) the scene over: the deal
        if (lw(L14_ADDR(0x0015f5c4)) == 2) break;
        sb(m + 0x20, 1);
        const uint32_t choice = lhu(pv + 4);
        if (choice - 3u >= 2u) break;
        sub_00263d40(static_cast<int16_t>(choice) == 4 ? TXT(0x53dd) : TXT(0x36b3), -1);
        item_give(0x20, 1);
        gondola_done(level_moby(lw(pv + 0x44)));
        set_uid_bit(save_bits(), m);
        set_uid_bit(L14_ADDR(0x001babd0), m);
        level_change_request(0, -1);
        moby_retire(m);
        return;
    }
    case 3:  // (0x002fbd3c)
        moby_retire(m);
        return;
    default:
        break;
    }
    talker_head(m, pv, f.sp, Head{0x180, 0x194, 0x198, 0xe4, 0xe8, 0x168, 0xf0, 0x80, 0x100});
}

// orig: L14:0x002fefe0
void olt_class924_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0xb0);
    class_mates_mark_fixed(m, 0);
    switch (lbu(m + 0x20)) {
    case 0:  // (0x002ff058) set up (gone once the story flag 0x13dd4f is set)
        if (lbu(0x0013dd4fu) != 0 || talker_done(m) || lw(pv + 0x44) == -1 || lw(pv + 0x48) == -1) {
            moby_retire(m);
            return;
        }
        sw(pv + 0x20, L14_ADDR(0x00161fe8));
        talk_init(m, pv);
        sb(m + 0x20, 1);
        sb(pv + 8, 1);
        break;
    case 1: {  // (0x002ff0f0) talking
        if (lbu(pv + 8) == 1 && point_in_box(0x0013f3d0u, lw(pv + 0x40))) {
            talker_checkpoint(m, pv, 0x44);
            swc1(pv + 0xc, 255.0f);
        } else {
            swc1(pv + 0xc, 3.0f);
        }
        if (talk_update(m, pv) == 0) break;
        // the scene puts the hero at the box +0x48 when it ends (the walk-to: hero_walk_to's fields, set from the box)
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x48));
        const uint32_t S = OV(0x0016c860);
        if (region_core_moved()) sb(S + 0x4a, 1);  // (region: the 1.02 build's flag, a byte at +0x4a: hero_walk_to_l1)
        else sh(S + 0x46, 1);
        lq_sq(S + 0x10, box + 0x30);
        vec_clear(V(S + 0x20));
        swc1(S + 0x28, lwc1(box + 0x78));
        sb(m + 0x20, 2);
        break;
    }
    case 2:  // (0x002ff1e0) the scene over: the coordinates
        if (lw(L14_ADDR(0x0015f5c4)) == 2) break;
        sb(m + 0x20, 1);
        if (lh(pv + 4) != 4) break;
        level_mark_visited(0xf);
        level_arrival_message(0xf);
        set_uid_bit(save_bits(), m);
        set_uid_bit(L14_ADDR(0x001babd0), m);
        level_change_request(0, -1);
        moby_retire(m);
        return;
    case 3:  // (0x002ff28c)
        moby_retire(m);
        return;
    default:
        break;
    }
    talker_head(m, pv, f.sp, Head{0x150, 0x160, 0x164, 0xb4, 0xb8, 0x138, 0xc0, 0x50, 0xd0});
}

}  // namespace rac1
