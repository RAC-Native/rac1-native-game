// The collision routines' one writer of the hit block 0x173e40 (see collide_hit.h).
#include <game/collision/collide_hit.h>
#include <game/bridge/guard.h>

namespace rac1 {

namespace {
void put_vec(uint32_t a, const Vec4& v) {
    sw(a, f2u(v.x));
    sw(a + 4, f2u(v.y));
    sw(a + 8, f2u(v.z));
    sw(a + 0xc, f2u(v.w));
}
}  // namespace

// orig: (host) see collide_hit.h. The original's routines store these words in their own orders (sphere_mesh_collide the
// point, the normal, the triangle, +0x18, +0x1c and the pushed-out position last; mobys_in_sphere +0x20, +0x1c, +0x18): no
// two overlap and nothing runs between them, so the block ends the same.
void coll_hit_mirror(const CollisionHit& hit, uint32_t parts) {
    const BridgeScope bridge;
    constexpr OvConst<0x00173e40u> OUTB{};
    if ((parts & kHitMoby) != 0) sw(OUTB + 0x18, hit.moby);
    if ((parts & kHitFlags) != 0) sw(OUTB + 0x1c, hit.flags);
    if ((parts & kHitPoint) != 0) put_vec(OUTB + 0x20, hit.point);
    if ((parts & kHitPushed) != 0) put_vec(OUTB + 0x30, hit.pushed);
    if ((parts & kHitNormal) != 0) put_vec(OUTB + 0x40, hit.normal);
    if ((parts & kHitTri) != 0)
        for (uint32_t k = 0; k < 3; ++k) put_vec(OUTB + 0x50 + k * 0x10, hit.tri[k]);
}

}  // namespace rac1
