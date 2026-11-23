#ifndef RAC1_GAME_COLLISION_COLLISION_H
#define RAC1_GAME_COLLISION_COLLISION_H
// What the collision queries leave for their callers, typed. segment_collide, sphere_collide, capsule_collide,
// sphere_mesh_collide and mobys_in_sphere (src/game/collision/collide_*.cpp, declared in game/ovl/ovl.h) write their nearest
// contact into the block at 0x173e40, the mobys they touch into the hit list 0x177f00 and contact records into the ring
// 0x178100; the game's code reads those words back. For now every accessor here reads (or writes) exactly those EE words,
// at the moment it is called: a reader moved onto it changes nothing the oracle, the tours or the RAM hash can see. A later
// task moves the storage behind it.
//
// The block 0x173e40 (the words before +0x18 are the queries' own state, not part of a hit):
//   +0x00 the level's collision tree, +0x08 the posed-joint cache ring, +0x10 the query counter (a moby's +0x9c stamp),
//   +0x14 the contact ring's next index;
//   +0x18 the moby hit (its EE address), 0 for the level's collision;
//   +0x1c the hit's flags: for a triangle 0x1000 + its flags (the low 5 bits its surface type, 0x1f none: sub_001f35d8;
//         bits 5..6: sub_001f3608), for a moby's primitive minus the record's EE address (sphere / capsule / segment),
//         0x3f after mobys_in_sphere;
//   +0x20 the contact point (w 0; mobys_in_sphere stores (0, 0, 0, 1)), +0x30 the query's position pushed out of the contact
//   (sphere / capsule / sphere_mesh; segment_collide leaves it), +0x40 the normal (the push for sphere_collide),
//   +0x50 / +0x60 / +0x70 the triangle hit (the previous contact's after a primitive).
// Field names: inferred from what the writers store and what the readers do with them; see the report of task C1.
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

#include <cstdint>

namespace rac1 {

struct CollisionHit {
    uint32_t moby;   // +0x18
    uint32_t flags;  // +0x1c
    Vec4 point;      // +0x20
    Vec4 pushed;     // +0x30
    Vec4 normal;     // +0x40
    Vec4 tri[3];     // +0x50, +0x60, +0x70
};

// orig: (host) the moby the last query hit, 0 for the level's collision (+0x18: the most read word)
inline uint32_t coll_hit_moby() { return lwu(OV(0x00173e58)); }
// orig: (host) the last hit's flags (+0x1c; signed: a primitive's is negative)
inline uint32_t coll_hit_flags() { return lwu(OV(0x00173e5c)); }
// orig: (host) the same flags read signed (+0x1c: above 0 a triangle of the level, below 0 a moby's primitive)
inline int32_t coll_hit_flags_signed() { return lw(OV(0x00173e5c)); }
// orig: (host) the last contact point (+0x20; its y and z are +0x24 / +0x28)
inline Vec4 coll_hit_point() { return V(OV(0x00173e60)); }
// orig: (host) the query's position pushed out of the last contact (+0x30)
inline Vec4 coll_hit_pushed() { return V(OV(0x00173e70)); }
// orig: (host) the last contact's normal (+0x40; its z is +0x48)
inline Vec4 coll_hit_normal() { return V(OV(0x00173e80)); }
// orig: (host) the last triangle hit's corner k = 0..2 (+0x50 + k * 0x10)
inline Vec4 coll_hit_tri(int k) { return V(OV(0x00173e90) + static_cast<uint32_t>(k) * 0x10u); }
// orig: (host) the last contact point's lane k = 0..2 (x, y, z: +0x20 + k * 4), as a loop over the lanes reads it
inline float coll_hit_point_lane(uint32_t k) { return lwc1(OV(0x00173e60) + 4u * k); }
// orig: (host) the last contact's normal's lane k = 0..2 (+0x40 + k * 4)
inline float coll_hit_normal_lane(uint32_t k) { return lwc1(OV(0x00173e80) + 4u * k); }
// orig: (host) the whole hit (+0x18 .. +0x7f), read now
inline CollisionHit coll_hit() {
    CollisionHit h;
    h.moby = coll_hit_moby();
    h.flags = coll_hit_flags();
    h.point = coll_hit_point();
    h.pushed = coll_hit_pushed();
    h.normal = coll_hit_normal();
    for (int k = 0; k < 3; ++k) h.tri[k] = coll_hit_tri(k);
    return h;
}
// orig: (host) the hit moby set (+0x18), as a few class updates do after a query
inline void coll_hit_set_moby(uint32_t moby) { sw(OV(0x00173e58), moby); }
// orig: (host) the last contact's normal set (+0x40), as a few class updates do (they shorten it in place)
inline void coll_hit_set_normal(const Vec4& n) { V(OV(0x00173e80)) = n; }
// orig: (host) the contact point set (+0x20), as the hero on a hoverboard and the camera's sphere volumes do
inline void coll_hit_set_point(const Vec4& p) { V(OV(0x00173e60)) = p; }
// orig: (host) the pushed-out position set (+0x30), as the camera's sphere volumes do
inline void coll_hit_set_pushed(const Vec4& p) { V(OV(0x00173e70)) = p; }
// orig: (host) the contact point's z set (+0x28), as a particle does to lift its landing point
inline void coll_hit_set_point_z(float z) { swc1(OV(0x00173e68), z); }
// orig: (host) the pushed-out position's z set (+0x38), as a doomlet does after its floor probe
inline void coll_hit_set_pushed_z(float z) { swc1(OV(0x00173e78), z); }
// orig: (host) the normal's z cleared (+0x48, a word write of 0), as the hero's wall slide does
inline void coll_hit_clear_normal_z() { sw(OV(0x00173e88), 0); }
// The EE addresses of the hit's vectors, for the callees that still take an address (a later task gives them values).
// orig: (host) the contact point's EE address (+0x20)
inline uint32_t coll_hit_point_addr() { return OV(0x00173e60); }
// orig: (host) the pushed-out position's EE address (+0x30)
inline uint32_t coll_hit_pushed_addr() { return OV(0x00173e70); }
// orig: (host) the normal's EE address (+0x40)
inline uint32_t coll_hit_normal_addr() { return OV(0x00173e80); }

// The queries' own state (the words before +0x18: not part of a hit).
// orig: (host) the level's collision tree (+0x00)
inline uint32_t coll_tree() { return lwu(OV(0x00173e40)); }
// orig: (host) the tree word's EE address (+0x00), for a reader that reads it through its own path (the drawing's copy of EE
// memory: draw_lwu)
inline uint32_t coll_tree_addr() { return OV(0x00173e40); }
// orig: (host) the level's collision tree set (+0x00), as level_collision_set does
inline void coll_set_tree(uint32_t tree) { sw(OV(0x00173e40), tree); }
// orig: (host) the posed-joint cache's buffers set (+0x04: eight of 0x800 bytes), as gameplay_load does
inline void coll_set_joint_cache_buffers(uint32_t a) { sw(OV(0x00173e44), a); }
// orig: (host) the posed-joint cache's key ring set (+0x08: eight pairs of words), as gameplay_load does
inline void coll_set_joint_cache_keys(uint32_t a) { sw(OV(0x00173e48), a); }
// orig: (host) the posed-joint cache's next slot set (+0x0c, 0..7), as gameplay_load does
inline void coll_set_joint_cache_next(uint32_t slot) { sw(OV(0x00173e4c), slot); }
// orig: (host) the query counter (+0x10: a moby's +0x9c stamp; the pause menu and a level's exit set its top bit)
inline uint32_t coll_query_counter() { return lwu(OV(0x00173e50)); }
// orig: (host) the query counter set (+0x10)
inline void coll_set_query_counter(uint32_t n) { sw(OV(0x00173e50), n); }
// orig: (host) the contact ring's next index (+0x14, 0..63)
inline uint32_t coll_contact_next() { return lwu(OV(0x00173e54)); }
// orig: (host) the contact ring's next index set (+0x14)
inline void coll_set_contact_next(uint32_t i) { sw(OV(0x00173e54), i); }

// The hit list 0x177f00: the mobys the last mobys_in_sphere touched, 0-terminated (room for 0x80 words, up to the ring).
// orig: (host) its EE address (what moby_list_hit and its kin take)
inline uint32_t coll_hit_list() { return OV(0x00177f00); }
// orig: (host) its entry i
inline uint32_t coll_hit_list_at(int i) { return lwu(OV(0x00177f00) + static_cast<uint32_t>(i) * 4u); }
// orig: (host) its length (the entries before the 0; mobys_in_sphere returns the same count)
inline int coll_hit_list_count() {
    int n = 0;
    while (n < 0x80 && coll_hit_list_at(n) != 0) ++n;
    return n;
}

// The contact ring 0x178100: 64 records of 64 bytes, indexed by a moby's +0xa4 (the queries' `rec` argument fills them).
// orig: (host) record `index`'s EE address, for now
inline uint32_t coll_contact(int index) { return OV(0x00178100) + (static_cast<uint32_t>(index) << 6); }

}  // namespace rac1

#endif  // RAC1_GAME_COLLISION_COLLISION_H
