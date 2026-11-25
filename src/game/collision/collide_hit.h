#ifndef RAC1_GAME_COLLISION_COLLIDE_HIT_H
#define RAC1_GAME_COLLISION_COLLIDE_HIT_H
// What the collision routines (src/game/collision/collide_*.cpp) share now that their work is on the host's stack instead of
// the scratchpad (stage 5, tasks C2a .. C2c): the one writer of the hit block 0x173e40 (coll_hit_mirror), the transformed
// vertices with their outcodes (CollVerts), the moby grid's cells under a query (coll_grid_cell / coll_grid_cursor), and the
// level collision tree's chunk buffers and walk (CollChunkBufs, coll_tree_chunks, coll_chunk_vertices, coll_chunk_quads).
// The readers' side is collision.h (CollisionHit, coll_hit() ...); this header is the writers'.
//
// The original keeps a query's work at 0x70000000 + k (the chunk lists at +0, the vertices at +0x2000, the cell lists at
// +0x3800 ...). Nothing reads those words after the call (stage 5's task A2 measured it on the nine tours), so they live on
// the stack now and only the words the game reads later are written to EE memory: the hit block, the hit list, the contact
// ring, the mobys' +0x9c / +0xa4. The posing functions the moby tests call (moby_pose_matrices and its callees) still work on
// the scratchpad (stage 7): their words, and the joint positions the primitive tests read back from there, stay.
#include <game/collision/collide_common.h>
#include <game/collision/collision.h>
#include <game/collision/collision_scene.h>

#include <cstdint>
#include <memory>

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

// orig: (host) the hit (collision_state.h's native state) and, with the projection (RAC1_COLL_STATE mirror or ee), the hit
// block 0x173e40 (+0x18 .. +0x7f, the level's copy), written from `hit`: the parts in `parts` (the others are left as they were, in both), every EE word
// of them (the four lanes of each vector, w included: a lane that carries a raw word, as the pushed-out position's w carries
// a vertex's outcode, keeps its bits, since a Vec4 lane set with u2f and read with f2u is only ever moved), through sw only
// (the bytes as the original's on any host). The only writer of those words in the collision routines; it runs inside a
// BridgeScope (the bridge guard counts it as the mirror of a native object, not as a reader or writer of the EE block).
void coll_hit_mirror(const CollisionHit& hit, uint32_t parts = kHitAll);

// The debug switch RAC1_COLL_JOINTLESS=1 (off by default; a debug tool, not yet in docs/env-vars.md). A moby's primitive
// record that names joints (types other than 1 and 3) in a set whose joint count is 0 makes a query read its "joint
// positions" through a pointer that is still the scratchpad: within the query the port gives the same words
// (CollChunkBufs::spr_word), across queries it cannot (the words a previous query left there are no longer in the
// scratchpad). No level's data has such a record (task C2c's scan of every class of the 19 levels:
// orch/w5/c2c/jointless_scan.tsv). With the switch on, the first query after a level change scans the level's class table
// and prints every such record ("[jointless] scan ..."), and every such read is printed as it happens ("[jointless] read
// ..."): for the play tests and the tours of later waves.
// orig: (host) the switch is on
bool coll_jointless_on();
// orig: (host) the scan of the loaded level's classes, once per level (called by the queries while the switch is on)
void coll_jointless_scan_level();
// orig: (host) a read through such a record, reported (called by the primitive tests while the switch is on)
void coll_jointless_read(const char* routine, uint32_t m, uint32_t col, int32_t type, int32_t more, uint32_t flags);

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


// The two buffers the original fetches the level collision tree's chunks into by DMA, at 0x70000000 and 0x70001000 (task
// C2b): chunk k of a query goes to buffer k & 1 (the next one is fetched while one is tested; nothing in between writes the
// tree, so the order of the copies is the original's). Each holds the EE words as host values (word i = bytes 4i .. 4i + 3,
// little-endian as on the EE: the byte accessors below shift, so the contents are the same on any host). A chunk is
// (u8 triangles, u8 -, u8 vertices, u8 quads; the vertices' packed words; the triangles' words; the quads' extra bytes):
// its counts are bytes, so nothing reads past 4 + 4 * 255 * 2 + 255 bytes (< 0x1000) of a buffer, and its quadword count
// is a byte (the tree's cell word & 0xff): a fetch fills at most 255 of the 256 quadwords. Value-initialise it
// (`CollChunkBufs bufs{};`): a chunk whose counts reach past what was fetched read the scratchpad's stale words in the
// original (no level's data does), here zeros or the words of the chunk fetched two before, the same in every run.
// spr_word() gives the moby tests the words the original finds in its first 8 KB of scratchpad through a joint pointer
// that is still the scratchpad (see there); past 8 KB (the vertices, a joint index of 512 or more) and once
// moby_pose_matrices ran in the query (its joint positions then; an index past the moby's joint count would read chunk words
// it did not overwrite in the original) the scratchpad itself is read.
struct CollChunkBufs {
    static constexpr uint32_t kWords = 0x1000 / 4;
    uint32_t w[2][kWords];
    uint32_t used[2];  // the words of each buffer this query wrote (0 .. used): the original's scratchpad words it replaced
    // orig: (host) sd(buffer, 0): the first two words cleared (the original does it before every fetch)
    void clear_head(uint32_t b) {
        w[b][0] = 0;
        w[b][1] = 0;
        used[b] = std::max(used[b], 2u);
    }
    // orig: (host) spr_dma_to(buffer, madr, qwc): qwc (a byte) quadwords from madr aligned down to 16 bytes, as the DMA reads
    void fetch(uint32_t b, uint32_t madr, uint32_t qwc) {
        const uint32_t src = madr & ~15u, n = (qwc & 0xffu) * 4;
        for (uint32_t i = 0; i < n; ++i) w[b][i] = lwu(src + i * 4);
        used[b] = std::max(used[b], n);
    }
    // orig: (host) the word the original reads at the EE address a when it reads its scratchpad's first 8 KB through a moby's
    // joint pointer that is still the scratchpad itself (a moby's primitive records naming joints before any
    // moby_pose_matrices of the query ran: a joint count of 0 for the selected set): there the original finds these buffers'
    // words, which this query wrote (0 .. used), or the scratchpad's older ones, still there (read from it)
    uint32_t spr_word(uint32_t a) const {
        const uint32_t off = a - 0x70000000u;
        if (off < 0x2000u && (off & 0xfffu) >> 2 < used[off >> 12]) return w[off >> 12][(off & 0xfffu) >> 2];
        return lwu(a);
    }
    // orig: (host) the same fetch from a collision scene's copy of the quadwords (RAC1_COLL_SCENE=native): n words
    void fetch_words(uint32_t b, const uint32_t* src, uint32_t n) {
        std::memcpy(w[b], src, n * 4);
        used[b] = std::max(used[b], n);
    }
    uint32_t word(uint32_t b, uint32_t off) const { return w[b][(off >> 2) & (kWords - 1)]; }
    uint32_t byte(uint32_t b, uint32_t off) const { return (word(b, off) >> ((off & 3) * 8)) & 0xffu; }
    void set_byte(uint32_t b, uint32_t off, uint32_t v) {
        uint32_t& x = w[b][(off >> 2) & (kWords - 1)];
        const uint32_t sh = (off & 3) * 8;
        x = (x & ~(0xffu << sh)) | ((v & 0xffu) << sh);
    }
};

// orig: (host) where one query takes the level collision tree's cells from: the collision scene of the tree pointer
// (collision_scene.h; RAC1_COLL_SCENE=native and a scene that holds the tree) or EE memory (`ee`, or no scene). In the check
// mode every cell word and fetched word taken from the scene is compared with EE memory; a difference drops the scene and
// parses it again (an edit of the tree's bytes in place: the oracle's test trees), a difference right after that is the
// parse's own error (counted, and the scene's value kept so that the query shows it).
struct CollTreeSrc {
    uint32_t tree;
    std::shared_ptr<const CollTreeScene> scene;
    bool check = false;
    // (`used` false: a query that does not test the level, flags & 1: no scene looked up)
    explicit CollTreeSrc(uint32_t t, bool used = true) : tree(t) {
        if (used && coll_scene_native()) {
            scene = coll_scene_tree(t);
            check = scene != nullptr && coll_scene_check();
        }
    }
    // orig: (host) the cell word at (x, y, z) (coll_tree_cell) and, from a scene, its fetched words
    CollTreeScene::Cell cell(int32_t x, int32_t y, int32_t z) {
        if (scene == nullptr) return CollTreeScene::Cell{coll_tree_cell(tree, x, y, z), nullptr};
        CollTreeScene::Cell c = scene->cell(x, y, z);
        if (check && !same_as_ee(x, y, z, c)) {
            scene = coll_scene_tree_stale(tree, false);
            if (scene == nullptr) return CollTreeScene::Cell{coll_tree_cell(tree, x, y, z), nullptr};
            c = scene->cell(x, y, z);
            if (!same_as_ee(x, y, z, c)) coll_scene_tree_stale(tree, true);
        }
        return c;
    }
    // orig: (host) the cell's quadwords into buffer b, as the original's DMA from (word >> 8) + tree
    void fetch(CollChunkBufs& bufs, uint32_t b, const CollTreeScene::Cell& c) const {
        if (c.words == nullptr) bufs.fetch(b, (c.word >> 8) + tree, c.word & 0xff);
        else bufs.fetch_words(b, c.words, (c.word & 0xffu) * 4);
    }

private:
    bool same_as_ee(int32_t x, int32_t y, int32_t z, const CollTreeScene::Cell& c) const {
        if (coll_tree_cell(tree, x, y, z) != c.word) return false;
        if (c.word == 0) return true;
        const uint32_t src = ((c.word >> 8) + tree) & ~15u, n = (c.word & 0xffu) * 4;
        for (uint32_t i = 0; i < n; ++i)
            if (lwu(src + i * 4) != c.words[i]) return false;
        return true;
    }
};

// orig: (host) a fetched chunk's packed vertices (x, y 10 bits << 6, z 12 bits << 4: 1/64 and 1/16 of a unit from the cell's
// centre) unpacked into `verts` with their outcodes against the box [blo, bhi] (bit 0 below, bit 1 above, per axis at bits
// 0 / 8 / 16), in pairs as the original's loop (an odd count unpacks the triangles' first word as one more vertex). Returns
// the vertex count (byte 2 of the chunk).
[[maybe_unused]] static uint32_t coll_chunk_vertices(const CollChunkBufs& bufs, uint32_t b, const int16_t blo[3], const int16_t bhi[3],
                                                     CollVerts& verts) {
    const uint32_t nv = bufs.byte(b, 2);
    uint32_t src = 4, vi = 0;
    for (int32_t left = static_cast<int32_t>(nv); left > 0; left -= 2) {
        for (uint32_t h = 0; h < 2; ++h, src += 4, ++vi) {
            const uint32_t pv = bufs.word(b, src);
            const int16_t c[3] = {h16(static_cast<int32_t>((pv & 0x3ff) << 6)), h16(static_cast<int32_t>(((pv >> 10) & 0x3ff) << 6)), h16(static_cast<int32_t>((pv >> 20) << 4))};
            uint32_t oc = 0;
            for (uint32_t a = 0; a < 3; ++a) {
                if ((static_cast<uint16_t>(c[a] - blo[a]) & 0x8000u) != 0) oc |= 1u << (a * 8);
                if ((static_cast<uint16_t>(bhi[a] - c[a]) & 0x8000u) != 0) oc |= 2u << (a * 8);
            }
            verts.put(vi, L3{static_cast<float>(c[0]), static_cast<float>(c[1]), static_cast<float>(c[2])}, oc);
        }
    }
    return nv;
}
// orig: (host) a chunk's quads: its first nq triangles become (a, c, extra vertex), the extra vertices the bytes after the
// triangles (tend = the triangles' end), rewritten in the buffer byte by byte in the original's order
[[maybe_unused]] static void coll_chunk_quads(CollChunkBufs& bufs, uint32_t b, uint32_t t0, uint32_t tend, uint32_t nq) {
    for (uint32_t i = 0; i < nq; ++i) {
        const uint32_t t = t0 + i * 4;
        const uint32_t c = bufs.byte(b, t + 2), x = bufs.byte(b, tend + i);
        bufs.set_byte(b, t + 1, c);
        bufs.set_byte(b, t + 2, x);
    }
}
// orig: (host) a triangle word (bytes a, b, c, flags) read from EE memory byte by byte, as the moby tests do
[[maybe_unused]] static uint32_t coll_tri_word_ee(uint32_t t) { return lbu(t) | lbu(t + 1) << 8 | lbu(t + 2) << 16 | lbu(t + 3) << 24; }

// orig: (host) the cells of the level collision tree *(0x173e40) (from `tree`: the scene or EE memory) whose 4-unit box (4096
// in 1024ths) reaches the sphere (p, r: nr2 = -r * r), cells c0 .. c1 (x fastest, then y, then z), in the original's order:
// for each one with a chunk, the chunk fetched into its buffer (the k-th into k & 1, its first two words cleared first) and
// fn(org, buffer) called with the cell's centre. The original lists them first as (centre, chunk) quadwords at 0x70003800 (the empty cells'
// entries written and overwritten, a terminator, the next entry read ahead), then walks the list: the same cells in the same
// order, since testing a chunk writes nothing the list depends on. (Its room before the scratchpad's end was 127 cells: a
// bigger box ran past it, which the port's memory refused. The walk has no such limit.)
template <typename Fn>
static void coll_tree_chunks(CollTreeSrc& tree, const L3& p, float nr2, const int32_t c0[3], const int32_t c1[3], CollChunkBufs& bufs, Fn&& fn) {
    const L3 base = L3{static_cast<float>(c0[0] << 12), static_cast<float>(c0[1] << 12), static_cast<float>(c0[2] << 12)};
    L3 corner = base;
    uint32_t k = 0;
    for (int32_t cz = c0[2];; ++cz) {
        corner.y = base.y;
        for (int32_t cy = c0[1];; ++cy) {
            corner.x = base.x;
            for (int32_t cx = c0[0];; ++cx) {
                const L3 far = L3{qadd(corner.x, 4096.0f), qadd(corner.y, 4096.0f), qadd(corner.z, 4096.0f)};
                const L3 cl = L3{fminv(fmaxv(corner.x, p.x), far.x), fminv(fmaxv(corner.y, p.y), far.y), fminv(fmaxv(corner.z, p.z), far.z)};
                const L3 ctr = L3{qadd(corner.x, 2048.0f), qadd(corner.y, 2048.0f), qadd(corner.z, 2048.0f)};
                const L3 d = sub3(cl, p);
                const L3 dq = mul3(d, d);
                const float gap = qmadd(qmadd(qadd(nr2, dq.x), 1.0f, dq.y), 1.0f, dq.z);
                if (neg(gap)) {
                    const CollTreeScene::Cell chunk = tree.cell(cx, cy, cz);
                    if (chunk.word != 0) {
                        const uint32_t b = k & 1;
                        bufs.clear_head(b);
                        tree.fetch(bufs, b, chunk);
                        fn(ctr, b);
                        ++k;
                    }
                }
                corner.x = qadd(corner.x, 4096.0f);
                if (cx == c1[0]) break;
            }
            corner.y = qadd(corner.y, 4096.0f);
            if (cy == c1[1]) break;
        }
        corner.z = qadd(corner.z, 4096.0f);
        if (cz == c1[2]) break;
    }
}

// (C2c) segment_collide's lists, kept as the original lays them out at 0x70003000 .. 0x70003fff: the per-axis crossings at
// 0x70003000 / 0x70003200 / 0x70003400 (0x200 bytes each, 0x50000000-terminated), the merged (t, packed cell) list at
// 0x70003800 (t = 1 ends it) that the moby part then compacts in place into grid cells and overwrites with their ends, and
// the three step bytes at 0x70003ffc. The routine keeps the original's addresses and reads and writes them here, so the
// lists' overflows behave as the original's: an axis's crossings past its room run into the next axis's list (the image then
// starts as the scratchpad's words, load_from_spr(), since the merge reads the words it runs into); a merged list past
// 0x70004000 (more than 253 cells crossed) ran past the scratchpad's end, which the port's memory refused: those writes are
// dropped and those reads give 0 here. Not initialised otherwise: every word read is written first.
// A long segment's lists (126 crossings or more: the merged list past 0x70003c00, or the z list past 0x70003600) reach
// words the original leaves behind for later readers of the scratchpad: for such a segment (`through`, set with
// load_from_spr) every write also goes to the scratchpad, at the original's moment, so that it is left as the original
// leaves it. Short segments (all of the games' but a few long probes) never touch the scratchpad here.
struct SegLists {
    static constexpr uint32_t kBase = 0x70003000u, kBytes = 0x1000u;
    uint32_t w[kBytes / 4];
    bool through = false;
    uint32_t lw(uint32_t a) const {
        const uint32_t o = a - kBase;
        return o < kBytes ? w[o >> 2] : 0u;
    }
    void sw(uint32_t a, uint32_t v) {
        const uint32_t o = a - kBase;
        if (o < kBytes) {
            w[o >> 2] = v;
            if (through) ::rac1::sw(a, v);
        }
    }
    uint32_t lhu(uint32_t a) const { return (lw(a & ~3u) >> ((a & 2u) * 8)) & 0xffffu; }
    uint32_t lbu(uint32_t a) const { return (lw(a & ~3u) >> ((a & 3u) * 8)) & 0xffu; }
    void sb(uint32_t a, uint32_t v) {
        const uint32_t sh = (a & 3u) * 8;
        sw(a & ~3u, (lw(a & ~3u) & ~(0xffu << sh)) | ((v & 0xffu) << sh));
    }
    // orig: (host) the scratchpad's words 0x70003000 .. 0x70003fff, as the original finds them before it writes its lists;
    // the writes from here on also made there (see above)
    void load_from_spr() {
        for (uint32_t i = 0; i < kBytes / 4; ++i) w[i] = lwu(kBase + i * 4);
        through = true;
    }
};
// orig: (host) a vertex as cl3() reads one from the scratchpad (RAC1_PERF_VU_CLAMP: clamped as it is read)
[[maybe_unused]] static L3 cl3v(const L3& p) { return g_clamp_once ? L3{vuc(p.x), vuc(p.y), vuc(p.z)} : p; }

}  // namespace collide_detail
}  // namespace rac1

#endif  // RAC1_GAME_COLLISION_COLLIDE_HIT_H
