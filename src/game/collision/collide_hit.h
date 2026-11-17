#ifndef RAC1_GAME_COLLISION_COLLIDE_HIT_H
#define RAC1_GAME_COLLISION_COLLIDE_HIT_H
// What the collision routines (src/game/collision/collide_*.cpp) share now that their work is on the host's stack instead of
// the scratchpad (stage 5, tasks C2a .. C2c): the one writer of the hit block 0x173e40 (coll_hit_mirror), the transformed
// vertices with their outcodes (CollVerts), and the moby grid's cells under a query (coll_grid_cell / coll_grid_cursor).
// The readers' side is collision.h (CollisionHit, coll_hit() ...); this header is the writers'.
//
// The original keeps a query's work at 0x70000000 + k (the chunk lists at +0, the vertices at +0x2000, the cell lists at
// +0x3800 ...). Nothing reads those words after the call (stage 5's task A2 measured it on the nine tours), so they live on
// the stack now and only the words the game reads later are written to EE memory: the hit block, the hit list, the contact
// ring, the mobys' +0x9c / +0xa4. The posing functions the moby tests call (moby_pose_matrices and its callees) still work on
// the scratchpad (stage 7): their words, and the joint positions the primitive tests read back from there, stay.
#include <game/collision/collide_common.h>
#include <game/collision/collision.h>

#include <cstdint>

namespace rac1 {

// The parts of the hit block a query writes (the others it leaves as they were): coll_hit_mirror's `parts`.
enum CollHitPart : uint32_t {
    kHitMoby = 1u << 0,    // +0x18
    kHitFlags = 1u << 1,   // +0x1c
    kHitPoint = 1u << 2,   // +0x20 .. +0x2f
    kHitPushed = 1u << 3,  // +0x30 .. +0x3f
    kHitNormal = 1u << 4,  // +0x40 .. +0x4f
    kHitTri = 1u << 5,     // +0x50 .. +0x7f (the three corners)
    kHitAll = 0x3fu,
};

// orig: (host) the hit block 0x173e40 (+0x18 .. +0x7f, the level's copy) written from `hit`: the parts in `parts`, every word
// of them (the four lanes of each vector, w included: a lane that carries a raw word, as the pushed-out position's w carries
// a vertex's outcode, keeps its bits, since a Vec4 lane set with u2f and read with f2u is only ever moved), through sw only
// (the bytes as the original's on any host). The only writer of those words in the collision routines; it runs inside a
// BridgeScope (the bridge guard counts it as the mirror of a native object, not as a reader or writer of the EE block).
void coll_hit_mirror(const CollisionHit& hit, uint32_t parts = kHitAll);

namespace collide_detail {

// orig: (host) a hit vector from a contact register's x, y, z and the raw word its w lane stores
inline Vec4 hit_vec(const L3& v, uint32_t w) { return Vec4{v.x, v.y, v.z, u2f(w)}; }

// The transformed vertices a query tests triangles against, with their outcodes: the original's quadwords at 0x70002000 +
// 16 i (x, y, z as floats, the outcode in w: bit 0 below the query's box, bit 1 above, per axis at bits 0 / 8 / 16), i the
// triangle's u8 index. 256 entries, all a u8 index reaches (4 KB). A chunk with more vertices made the original write past
// +0x3000 (no triangle reads them back; past the scratchpad's end the port's memory refused the store and stopped): put()
// drops those. Value-initialise it (`CollVerts verts{};`): a triangle that names a vertex its chunk did not write (data no
// level has) read the scratchpad's stale words in the original, here zeros, the same in every run.
struct CollVerts {
    static constexpr uint32_t kCount = 256;
    struct Vert {
        L3 p;
        uint32_t oc;  // the original's w lane (the triangles AND the three; vf2.w keeps the second corner's)
    };
    Vert v[kCount];
    void put(uint32_t i, const L3& p, uint32_t oc) {
        if (i < kCount) v[i] = Vert{p, oc};
    }
    const Vert& operator[](uint32_t i) const { return v[i & (kCount - 1)]; }  // (i is a u8 index)
};

// The moby grid 0x19b7e0 (64 x 64 cells of 16 units; each cell u16 first << 5 into the id lists at +0x4000, s8 count of u16
// moby ids): the original lists the cells under a query as (start, end) pairs at 0x70003800, row by row, zero-terminated,
// then walks the list; the routines walk the cells directly, in the same order. (The list had room for 255 cells before
// the scratchpad's end: a bigger box ran past it, which the port's memory refused. The walk has no such limit.)
struct CollGridCell {
    uint32_t start, end;  // EE addresses of the cell's first moby id and one past its last
};
// orig: (host) cell (x, y) of the grid at `grid` (the level's copy)
inline CollGridCell coll_grid_cell(uint32_t grid, uint32_t x, uint32_t y) {
    const uint32_t c = grid + (x << 2) + (y << 8);
    const uint32_t start = (static_cast<uint32_t>(lhu(c)) << 5) + grid + 0x4000;
    return CollGridCell{start, start + static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(c + 2))) * 2)};
}
// orig: (host) the original's list cursor at the i-th cell (0x70003800 + 8 i): its register $t9, which moby_pose_matrices
// saves at 0x70003710 (posing's own words, compared by the oracle and copied out by the drawing): passed on as a value only
inline uint32_t coll_grid_cursor(uint32_t i) { return 0x70003800u + i * 8u; }

}  // namespace collide_detail
}  // namespace rac1

#endif  // RAC1_GAME_COLLISION_COLLIDE_HIT_H
