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
// orig: (host) the last contact point (+0x20; its y and z are +0x24 / +0x28)
inline Vec4 coll_hit_point() { return V(OV(0x00173e60)); }
// orig: (host) the query's position pushed out of the last contact (+0x30)
inline Vec4 coll_hit_pushed() { return V(OV(0x00173e70)); }
// orig: (host) the last contact's normal (+0x40; its z is +0x48)
inline Vec4 coll_hit_normal() { return V(OV(0x00173e80)); }
// orig: (host) the last triangle hit's corner k = 0..2 (+0x50 + k * 0x10)
inline Vec4 coll_hit_tri(int k) { return V(OV(0x00173e90) + static_cast<uint32_t>(k) * 0x10u); }
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
