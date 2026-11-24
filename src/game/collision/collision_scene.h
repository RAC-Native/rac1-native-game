#ifndef RAC1_GAME_COLLISION_COLLISION_SCENE_H
#define RAC1_GAME_COLLISION_COLLISION_SCENE_H
// The level's collision geometry as host structures (stage 5 of the native 1.0 track, task C3): the collision tree
// *(0x173e40) and the hero's mesh chunk list *(0x15f618), parsed from the EE bytes into host-endian copies of exactly the
// integers the collision routines read (the tree's cell words and each cell's fetched words: the 10 / 10 / 12-bit packed
// vertices and the face bytes; the chunks' u16 spheres, counts and vertices and their triangle bytes), so the routines compute
// the same values from them as from EE memory.
//
// RAC1_COLL_SCENE=native (the default): the routines (collide_sphere / _capsule / _segment / _mesh .cpp) take a cell's
// words and a chunk's data from the scene; `ee`, or no scene for the pointer: from EE memory, as before. The moby
// collision data (a class header's +0x94) stays in EE.
//
// Each part is keyed on its EE pointer. A part is parsed on the first query that meets a pointer, kept (a few per part:
// the open engine's sections switch the pointers back and forth) and immutable once built: replaced, never edited, so a
// thread holding one keeps a consistent copy. When the pointer changes, or the generation does (level_collision_set clears
// the parts and bumps it; coll_set_tree bumps it when the tree word changes), the kept part for that pointer is used again
// only when a hash of every EE byte it was parsed from still matches (a section streamed into a freed section's heap block
// gets its own parse). An edit of those bytes in place, the pointers and the generation unchanged, is not seen there: the
// game makes none, and the check mode (RAC1_COLL_SCENE_CHECK=1, always on in the oracle) compares every value a query takes
// from the scene with EE memory and parses again on a difference.
//
// A pointer or a row the scene cannot hold (outside RAM, below 4 KB, counts beyond any level's) gives no scene: the routines
// then read EE memory, the same values.
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace rac1 {

// The collision tree: z rows of y rows of x rows (u16 first, u16 count, then the entries: z's u16 offsets / 4, y's u32
// offsets, x's u32 cell words: the cell's offset << 8 | its size in quadwords), each cell's quadwords as the routines fetch
// them (from (word >> 8) + tree, aligned down to 16 bytes, (word & 0xff) quadwords).
class CollTreeScene {
public:
    struct Cell {
        uint32_t word;          // the cell word (0: none)
        const uint32_t* words;  // its (word & 0xff) * 4 fetched words
    };
    // orig: (host) the parse of the tree at `tree` in EE memory; null when it cannot be held (the routines read EE then)
    static std::shared_ptr<const CollTreeScene> parse(uint32_t tree);
    // orig: (host) 0x1efbd0's lookup (collide_common.h's coll_tree_cell) on the parsed rows, with its comparisons
    Cell cell(int32_t x, int32_t y, int32_t z) const {
        int32_t i = z - z_.first;
        if (i < 0 || z_.count - i <= 0) return Cell{0, nullptr};
        const int32_t yr = ent_[z_.at + static_cast<uint32_t>(i)];
        if (yr < 0) return Cell{0, nullptr};
        const Row& Y = rows_[static_cast<uint32_t>(yr)];
        i = y - Y.first;
        if (i < 0 || Y.count - i <= 0) return Cell{0, nullptr};
        const int32_t xr = ent_[Y.at + static_cast<uint32_t>(i)];
        if (xr < 0) return Cell{0, nullptr};
        const Row& X = rows_[static_cast<uint32_t>(xr)];
        i = x - X.first;
        if (i < 0 || X.count - i <= 0) return Cell{0, nullptr};
        const int32_t c = ent_[X.at + static_cast<uint32_t>(i)];
        if (c < 0) return Cell{0, nullptr};
        return Cell{cells_[static_cast<uint32_t>(c)].word, words_.data() + cells_[static_cast<uint32_t>(c)].at};
    }
    uint32_t tree() const { return tree_; }
    // orig: (host) the hash of the EE bytes the parse read, now (equal to source_hash() while they are unchanged)
    uint64_t hash_ee() const;
    uint64_t source_hash() const { return hash_; }
    // orig: (host) every row, entry and cell word equal (the same lookups and fetches); `why`: the first difference
    bool same_as(const CollTreeScene& o, std::string* why = nullptr) const;
    // orig: (host) counts for traces and the batch: rows, cells, fetched words, the EE bytes read
    size_t row_count() const { return rows_.size(); }
    size_t cell_count() const { return cells_.size(); }
    size_t word_count() const { return words_.size(); }
    size_t source_bytes() const;
    // orig: (host) the lowest and the end of the EE addresses the parse read
    std::pair<uint32_t, uint32_t> source_extent() const;
    // orig: (host) every non-empty cell's (x, y, z) and word, in the rows' order (batch 540's walk)
    struct WalkCell {
        int32_t x, y, z;
        uint32_t word;
    };
    std::vector<WalkCell> cells_walk() const;

private:
    struct Row {
        int32_t first;  // the row's first coordinate (u16, as lhu reads it)
        int32_t count;  // its entry count (u16)
        uint32_t at;    // its first entry in ent_
    };
    struct CellData {
        uint32_t word;
        uint32_t at;  // its first fetched word in words_
    };
    uint32_t tree_ = 0;
    Row z_{};
    std::vector<Row> rows_;          // the y and x rows
    std::vector<int32_t> ent_;       // the entries: z's and y's a row index, x's a cell index; -1 for a 0 entry
    std::vector<CellData> cells_;
    std::vector<uint32_t> words_;
    std::vector<std::pair<uint32_t, uint32_t>> src_;  // the EE byte ranges the parse read (address, bytes)
    uint64_t hash_ = 0;
    friend struct CollTreeParser;
};

// The hero's mesh chunk list: a count (s32), 12 bytes, then 16-byte chunks: u16 centre x, y, z (1/64), u16 radius, u8
// triangle count (+8), u16 vertex count (+0xa), the vertices' address (+0xc, relocated by level_collision_set); the vertices
// (u16 x, y, z, w) then the triangles (u8 a, b, c, flags) from there.
class CollMeshScene {
public:
    struct Chunk {
        uint16_t s[4];     // centre x, y, z, radius
        uint32_t ntri;     // lbu(+8)
        int32_t nv;        // lhu(+0xa)
        uint32_t vat;      // its first vertex in verts_ (4 u16 each; max(2, nv rounded up to even) of them: the pair loop's reads)
        uint32_t tat;      // its first triangle byte in tris_
    };
    // orig: (host) the parse of the list at `list` in EE memory; null when it cannot be held
    static std::shared_ptr<const CollMeshScene> parse(uint32_t list);
    int32_t count() const { return count_; }
    const Chunk& chunk(uint32_t i) const { return chunks_[i]; }
    // orig: (host) vertex k of chunk i (x, y, z, w)
    const uint16_t* vertex(const Chunk& c, uint32_t k) const { return verts_.data() + (c.vat + k) * 4; }
    // orig: (host) triangle t of chunk i (a, b, c, flags)
    const uint8_t* triangle(const Chunk& c, uint32_t t) const { return tris_.data() + c.tat + t * 4; }
    uint32_t list() const { return list_; }
    uint64_t hash_ee() const;
    uint64_t source_hash() const { return hash_; }
    bool same_as(const CollMeshScene& o, std::string* why = nullptr) const;
    size_t source_bytes() const;
    std::pair<uint32_t, uint32_t> source_extent() const;
    static uint32_t vertex_reads(int32_t nv) { return nv <= 2 ? 2u : static_cast<uint32_t>((nv + 1) & ~1); }

private:
    uint32_t list_ = 0;
    int32_t count_ = 0;
    std::vector<Chunk> chunks_;
    std::vector<uint16_t> verts_;
    std::vector<uint8_t> tris_;
    std::vector<std::pair<uint32_t, uint32_t>> src_;
    uint64_t hash_ = 0;
};

// ---- the switch and the current parts ----
// orig: (host) RAC1_COLL_SCENE=native (true; the default) or ee (false), read once
bool coll_scene_native();
// orig: (host, testing) the switch set (batch 540 runs the same query both ways)
void coll_scene_set_native(bool on);
// orig: (host) the check mode: RAC1_COLL_SCENE_CHECK=1, or set by the oracle's main
bool coll_scene_check();
// orig: (host) the check mode set
void coll_scene_set_check(bool on);
// orig: (host) the tree part for the tree pointer `tree` (the routines' *(0x173e40)); null: none (read EE)
std::shared_ptr<const CollTreeScene> coll_scene_tree(uint32_t tree);
// orig: (host) the mesh part for the list pointer `list` (*(0x15f618)); null: none (read EE)
std::shared_ptr<const CollMeshScene> coll_scene_mesh(uint32_t list);
// orig: (host) the check mode's answer to a difference between a part and EE memory: the part for that pointer dropped and
// parsed again (counted); a second difference after a fresh parse is the parse's own error (counted, printed once)
std::shared_ptr<const CollTreeScene> coll_scene_tree_stale(uint32_t tree, bool after_fresh);
std::shared_ptr<const CollMeshScene> coll_scene_mesh_stale(uint32_t list, bool after_fresh);
// orig: (host) level_collision_set's: the kept parts dropped, the generation bumped
void coll_scene_level_changed();
// orig: (host) coll_set_tree's: the generation bumped when the tree word changes
void coll_scene_tree_set(uint32_t old_tree, uint32_t tree);

struct CollSceneStats {
    uint64_t parses = 0;        // parts parsed (tree and mesh)
    uint64_t reused = 0;        // kept parts taken again after a pointer or generation change (their hash matched)
    uint64_t stale = 0;         // check mode: a part found different from EE memory and parsed again
    uint64_t parse_errors = 0;  // check mode: a fresh parse still different from EE memory (a bug)
    uint64_t unheld = 0;        // pointers no scene could hold (EE read)
    double parse_ms = 0.0;      // the parses' time
};
// orig: (host) the counts since the start
CollSceneStats coll_scene_stats();

}  // namespace rac1

#endif  // RAC1_GAME_COLLISION_COLLISION_SCENE_H
