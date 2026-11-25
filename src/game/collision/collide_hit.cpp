// The collision routines' one writer of the hit block 0x173e40 (see collide_hit.h).
#include <game/collision/collide_hit.h>
#include <game/bridge/guard.h>

#include <cstdio>
#include <cstdlib>

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
// two overlap and nothing runs between them, so the block ends the same. The native state (collision_state.h) takes the
// same parts first, then, with the projection (RAC1_COLL_STATE mirror or ee), EE memory takes the same words.
void coll_hit_mirror(const CollisionHit& hit, uint32_t parts) {
    CollisionHit& st = g_coll_state.hit;
    if ((parts & kHitMoby) != 0) st.moby = hit.moby;
    if ((parts & kHitFlags) != 0) st.flags = hit.flags;
    if ((parts & kHitPoint) != 0) st.point = hit.point;
    if ((parts & kHitPushed) != 0) st.pushed = hit.pushed;
    if ((parts & kHitNormal) != 0) st.normal = hit.normal;
    if ((parts & kHitTri) != 0)
        for (uint32_t k = 0; k < 3; ++k) st.tri[k] = hit.tri[k];
    if (!g_coll_state_project) return;
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

// orig: (host) see collide_hit.h
bool coll_jointless_on() {
    static const bool on = std::getenv("RAC1_COLL_JOINTLESS") != nullptr && std::atoi(std::getenv("RAC1_COLL_JOINTLESS")) != 0;
    return on;
}

namespace {
// a primitive record whose type names joints (all but 1, a sphere, and 3, a vertical capsule)
bool names_joints(int32_t type) { return type != 1 && type != 3; }
}  // namespace

// orig: (host) see collide_hit.h
void coll_jointless_scan_level() {
    static int32_t scanned = -1;
    static uint32_t scanned_tab = 0;
    const int32_t level = lw(0x0015ed84);
    const uint32_t tab = lwu(OV(0x00197300));
    if (level == scanned && tab == scanned_tab) return;
    scanned = level;
    scanned_tab = tab;
    uint32_t classes = 0, with_col = 0, flagged = 0, joint_records = 0;
    for (uint32_t ci = 0; ci < 0xe0; ++ci) {
        const uint32_t c = lwu(OV(0x00197300) + ci * 4);
        if (c == 0) continue;
        ++classes;
        const uint32_t col = lwu(c + 0x10);
        if (col == 0) continue;
        ++with_col;
        const uint32_t n0 = lhu(col), n2 = lhu(col + 2);
        if (lwu(col + 4) == 0) continue;  // (no primitive records: the routines skip them)
        // the class ids that map to this class index (0x197bc0[id] == ci), for the report
        char ids[64] = {};
        int nid = 0, len = 0;
        for (uint32_t id = 0; id < 0x800 && nid < 4; ++id)
            if (lbu(OV(0x00197bc0) + id) == ci) len += std::snprintf(ids + len, sizeof ids - static_cast<size_t>(len), "%s%u", nid++ ? "," : "", id);
        int32_t more = 0;
        uint32_t r = 0;
        for (uint32_t rec = col + 0x10; more >= 0 && r < 64; rec += 0x20, ++r) {
            more = lh(rec + 2);
            const int32_t type = static_cast<int8_t>(lbu(rec));
            if (!names_joints(type)) continue;
            ++joint_records;
            for (uint32_t set = 1; set <= 2; ++set) {
                const uint32_t count = set == 1 ? n0 : n2;
                if ((static_cast<uint32_t>(more) & set) == 0 || count != 0) continue;
                ++flagged;
                std::printf("[jointless] scan level=%d ci=%u class=%s set=%u record=%u type=%d more=%04x joints=%u/%u\n", level, ci, ids, set, r,
                            type, static_cast<uint32_t>(more) & 0xffffu, n0, n2);
            }
        }
    }
    std::printf("[jointless] scan level=%d: %u classes, %u with collision, %u primitive records naming joints, %u of them in a set "
                "without joints\n", level, classes, with_col, joint_records, flagged);
}

// orig: (host) see collide_hit.h
void coll_jointless_read(const char* routine, uint32_t m, uint32_t col, int32_t type, int32_t more, uint32_t flags) {
    std::printf("[jointless] read %s level=%d moby=%08x class=%u col=%08x type=%d more=%04x joints=%u/%u flags=%x\n", routine,
                lw(0x0015ed84), m, lhu(m + 0xa6), col, type, static_cast<uint32_t>(more) & 0xffffu, lhu(col), lhu(col + 2), flags);
}

}  // namespace rac1
