// The MRP world asset (ADR 0010 step 2): the level's tfrags, tie classes and shrub classes as the world's VU1
// programs draw them (tfrag A / B, tie A / B, shrub A / B), generated from the ISO by the setup tool (src/setup/mrp_export.cpp) into
// assets/generated/mrp/world/level<N>/world.mrpw and proved against the ported programs by
// Format and the programs' facts.
//
// For each program start the asset keeps the static part of the VU1 data memory (what the pass's chain UNPACKs from the
// level's own data: the class packet / the tfrag's VIF lists, as the level load left them) and its "kick program": the GS
// stream the program sends, as ops (a GIF tag, a texture prim's A+D, a vertex) whose vertices name their inputs (the
// packet's decoded vertex arrays). What changes per frame (the instance's matrix rows, its lit colours, fade, fog; the
// pass's constants) is not in the asset: the CPU reference (`*_kick_triangles`) and the MRP renderer take it from the frame.
#ifndef RAC1_FORMATS_WORLD_MRP_H
#define RAC1_FORMATS_WORLD_MRP_H

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rac1::mrpw {

constexpr uint32_t kWorldMagic = 0x5750524du;  // "MRPW"
constexpr uint32_t kWorldVersion = 1;

// One step of the GS stream a program kicks.
enum class OpKind : uint8_t {
    Tag = 0,      // a GIF tag (`tag`): PACKED with its NLOOP / PRIM / EOP as kicked
    Texture = 1,  // the A+D data of the packet's texture prim `index`
    Vertex = 2,   // a vertex (STQ, RGBAQ, XYZF2): `index` into the packet's vertex ops (`verts`)
    Other = 3,    // another qword the GIF reads (`tag` holds it): never seen in the shipped data
};
struct Op {
    OpKind kind = OpKind::Vertex;
    uint16_t index = 0;
    uint64_t tag[2] = {};
};

// A kicked vertex of a shrub packet: which of the packet's vertices (decoded arrays) fed it. The trace runs the ported
// program on tagged inputs (src/setup/world_trace.cpp): position, Q, UV and colour index sources are what
// the program's pipeline really took.
// A ShrubKickVertex colour >= kTwiceBase reads VU1 memory at 2 x (the colours' base, block + 5) + (colour - kTwiceBase): the vertex
// after a mid-list end flag (the 0x8000 bit of a vertex's n) adds the base twice (the end path's IADD runs on an index the
// pipeline had already based). 7 packets in the game (6-vertex packets flagged at their third); the colour is then whatever
// that memory holds (the output buffers' earlier kicks).
constexpr uint16_t kTwiceBase = 0x200;
struct ShrubKickVertex {
    uint16_t pos = 0;     // the vertex whose position the instance's rows transform
    uint16_t q = 0;       // the vertex whose 1 / w the projection uses
    uint16_t uv = 0;      // the vertex whose (s, t, h) STQ takes
    uint16_t colour = 0;  // the instance's lit colour index (0..23) the RGBA takes
};

// A texture prim of a packet: the A+D qwords as the class data stores them (the level load rewrites TEX0 / CLAMP:
// 3.3) and the local texture (the class's 16-byte remap names the level texture).
struct TexturePrim {
    std::array<std::array<uint64_t, 2>, 5> ad{};  // (data, register address) as stored in the file (shrub: 4 used, tie / tfrag: 5)
    uint8_t n = 4;
    int32_t local_texture = -1;
    int32_t texture = -1;  // the level texture (shrub / tie / tfrag table of the core), -1 unknown
};

struct ShrubPacket {
    std::vector<uint8_t> vif;            // the packet's VIF list (as the class stores it: the chain REFs it)
    std::vector<uint8_t> image;          // the VU1 data memory it unpacks, from the buffer base (derived: not in the file)
    // decoded (the three UNPACKs: header + GIF tags + texture prims; positions (s16 x3, the GS slot offset); (s, t, h, n | stop))
    int32_t header[4] = {};              // texture count, GIF tag count, vertex count, vertex offset
    std::vector<std::array<int16_t, 4>> pos;  // x, y, z, slot offset
    std::vector<std::array<int16_t, 4>> uv;   // s, t, h (/ 4096), n | 0x8000 (the end)
    std::vector<TexturePrim> textures;
    // the kick program (the no-clip program shrub A)
    std::vector<Op> ops;
    std::vector<ShrubKickVertex> verts;
    bool kick_ok = false;
    std::string kick_note;
};

struct ShrubClass {
    int32_t o_class = -1;
    uint32_t index = 0;  // the class's position in the core's shrub class table
    std::vector<uint8_t> header;  // 0x40 bytes
    std::array<uint8_t, 16> texture_remap{};
    std::vector<ShrubPacket> packets;
};

// A kicked vertex of a tie packet. The packet's vertices: first the "dinky" ones (two unpacked qwords: x, y, z, slot | s, t,
// q, slot 2), then the "fat" ones (three: a delta (x, y, z, slot) the instance's morph factor scales, the base position, the
// UV qword); per vertex a colour index entry (dinky: one byte, fat: three) in the packet's colour index list.
struct TieKickVertex {
    uint16_t pos = 0;     // the vertex (dinky 0.., then fat) whose position the instance's rows transform
    uint16_t q = 0;       // the vertex whose 1 / w the projection uses
    uint16_t uv = 0;      // the vertex whose UV qword STQ takes
    uint16_t colour = 0;  // the instance colour (0..63) the RGBA takes (fat: the first of its three)
    uint8_t fat = 0;      // 1: a fat vertex (position base + delta x morph, colour c0 z + (c1 + c2) / 2 w)
    uint8_t colour2[2] = {};  // fat: the other two colour indices
};

struct TiePacket {
    uint8_t lod = 0, index_in_lod = 0;
    std::array<uint8_t, 16> entry{};      // the class's packet entry (data offset relative to the entry list)
    std::vector<uint8_t> data;             // the packet's data (from the entry's offset to the end of its colour index lists)
    std::vector<uint8_t> image;            // the VU1 data memory the chain unpacks for it (absolute addresses 0..0x123; derived)
    std::vector<int32_t> prims;            // per texture slot k (VU 1 + 5 k): the class texture prim it takes
    uint32_t n_dinky = 0, n_fat = 0;
    std::vector<std::array<int16_t, 8>> dinky;   // x, y, z, slot, s, t, q, slot 2 (as unpacked: V4_16 signed)
    std::vector<std::array<int16_t, 12>> fat;    // delta x, y, z, slot; base x, y, z, -; s, t, q, slot 2
    std::vector<std::array<uint8_t, 4>> colour_index;  // the list at 0xcc (the second, at 0xf8, adds 0x40 to each)
    std::vector<Op> ops;
    std::vector<TieKickVertex> verts;
    bool kick_ok = false;
    std::string kick_note;
};

struct TieClass {
    int32_t o_class = -1;
    uint32_t index = 0;
    std::vector<uint8_t> header;   // 0x70 bytes as the file stores it
    std::array<uint8_t, 16> texture_remap{};
    float scale = 1.0f;
    std::vector<TexturePrim> textures;  // the class's texture prims (0x50 bytes each from +0x2c)
    std::vector<TiePacket> packets;     // every LOD's
};

// A tfrag (the level's 0x40-byte entry and its data). The tfrag pass sends, per visible entry, one of three images of its
// VIF lists (13): far (the lod 2 list: code 6), mid (the shared list up to lod 1's end: codes 8 / a), near
// (the shared list's common part, then the lod 0 list: codes e / 10 / 12, and the clipping program's code 2), then the
// entry's lit colours (STCYCL 2 / 1, V4_8 unsigned, from the entry's rgba_verts_loc: one qword in two).
struct TfragImage {
    std::vector<uint8_t> image;    // the VU1 data memory the lists unpack (from the buffer base, TOPS-relative; derived)
    std::vector<uint8_t> written;  // per qword of `image`: 1 if a list wrote it (derived)
    uint16_t colour_addr = 0, colour_count = 0;  // the colours' UNPACK: address (buffer-relative), count
    std::array<std::array<uint16_t, 2>, 2> refs{};  // the chain's REFs (qword offset in the entry's data, qwc); qwc 0: none
};
struct TfragEntry {
    uint32_t index = 0;
    std::array<uint8_t, 0x40> header{};  // the 0x40-byte entry as the file stores it (+0x10: its data's offset)
    std::vector<uint8_t> data;             // the entry's data (its VIF lists, the colours' base, lights)
    std::array<TfragImage, 3> lods;        // far, mid, near
    std::vector<TexturePrim> textures;     // the entry's texture prims (5 qwords each, at +tex_ofs; TEX0's word: the tfrag texture)
};

// A level mod's host mesh placed in the level (src/formats/host_meshes.h): a mesh no EE data stands for, drawn by the host
// at this placement as one of the world's tie classes. Not in world.mrpw: filled when the level's world is loaded.
struct HostMeshInstance {
    uint32_t cls = 0;            // World::ties
    float rot[3][3] = {};        // the rotation's rows (unit)
    float pos[3] = {};           // the position (world units)
    uint8_t rgba[4] = {0x80, 0x80, 0x80, 0x80};  // its colour (0x80 = 1.0), every vertex's without a palette entry
    std::vector<std::array<uint8_t, 4>> palette;  // its vertices' colours (at most 64: a vertex's index is its dinky slot lane)
    float scale = 1.0f;          // (a level object's, src/game/enhance/mod_code.cpp: its class's scale times its own) the class's scale times this
    uint8_t section = 0;         // (a session's, format 3: the section it belongs to, drawn only while that section is current; 0 main)
};

struct World {
    int32_t level = -1;
    std::vector<ShrubClass> shrubs;
    std::vector<TieClass> ties;
    std::vector<TfragEntry> tfrags;
    std::vector<HostMeshInstance> host_meshes;  // (not in the file: a level mod's, see HostMeshInstance)
};

bool write_world(const std::string& path, const World& w);
bool read_world(const std::string& path, World& w);

// ---- the CPU reference (the spec the MRP renderer reproduces, proved against the original) ----

// A kicked GIF packet as the CPU reference builds it: its qwords, each with what it is (an Op of the packet's kick program).
struct Kicked {
    std::vector<std::array<uint32_t, 4>> q;
    std::vector<uint8_t> part;  // per qword: 0 a GIF tag / other, 1 A+D, 2 STQ, 3 RGBAQ, 4 XYZF2
    std::vector<uint8_t> clip;  // per qword (XYZF2): 1 when the clipping program's test flags the vertex (outside its volume)
    // (the MRP renderer, when `want_hp`) per qword: for an XYZF2 the vertex's position as the program had it before the projection
    // (the instance's rows / the view applied: the 4-vector whose w the Q divides by), zeros for the others
    bool want_hp = false;
    std::vector<std::array<float, 4>> hp;
};

// The shrub draw (micro 0x063 of shrub A) for one instance: its block in VU1 memory (the count word's qword + 0x1c qwords
// per instance: rows R0, R1, R2, T at +1..+4, the lit colours from +5 as integers, the alpha the w of +5) and the pass's
// constants (micro 0x000's qwords 1 / 2: the fog's scale / offset (z / w), the fog's floor / ceiling (x / y)).
struct ShrubInstance {
    float rows[4][4] = {};
    int32_t colour[24][4] = {};
    int32_t alpha = 0x80;
    uint32_t block = 0;  // the instance's count qword in VU1 memory (0xee / 0x17b for the first instance)
    const uint8_t* vu_mem = nullptr;  // the VU1 data memory (16 KB) for kTwiceBase colours (null: colour 0)
};
struct ShrubConsts {
    float fog_scale = 0, fog_offset = 0, fog_min = 0, fog_max = 255;
    // the clipping program (shrub B, the same micro start 0x063): the instance's rows give clip coordinates, the screen is
    // ((p.xyz x (h / w)) x scale) + offset with the pass's qwords 3 / 4, STQ (s, t, 1) x (h / w)
    bool clip = false;
    float scale[4] = {}, offset[4] = {};
};
// `tex_ad`: the texture prims' A+D qwords as the VU1 memory holds them (the level's copy of the class: TEX0 / CLAMP as the
// level load rebuilt them, TBP0 as the frame's residency patched it), 4 per prim.
void shrub_kick(const ShrubPacket& p, const ShrubInstance& in, const ShrubConsts& k, std::span<const std::array<uint32_t, 4>> tex_ad, Kicked& out);

// The tie A draw (micro 0x000) for one instance: its block (6 qwords at the program's vi3: rows R0, R1, R2, T; +4 the morph
// factor (x) and the fat colours' factors (z, w); +5 the fog (w, as FTOI4 takes it) and the alpha-test state (x)), its 64 lit
// colours as the VU1 memory holds them (2^23 + byte floats: STMOD 1 with the row 0x4b000000) in the table its colour index list
// names (0x346 for the block at 0xc6, 0x386 for 0xf2).
struct TieInstance {
    float rows[4][4] = {};
    float morph[4] = {};
    float fog[4] = {};
    uint32_t colour[64][4] = {};
    // the clipping program (tie B): clip coordinates, the screen ((p.xyz x scale) x (q / w)) + offset (0x3ce / 0x3cd); F the
    // instance's fog as tie A's (its pass 2 adds the fog to a w lane whose -1 lands after the add)
    bool clip = false;
    float scale[4] = {}, offset[4] = {};
};
// `tex_ad`: the texture slots' A+D qwords as VU1 memory holds them (5 per slot, from 1 + 5 k).
void tie_kick(const TiePacket& p, const TieInstance& in, std::span<const std::array<uint32_t, 4>> tex_ad, Kicked& out);

// The tfrag A program (micro 0x74c instructions) for one start, re-implemented step by step on a copy of the VU1
// data memory as the program finds it (`vu_mem`, 16 KB: the entry's image at buffer base `base`, its colours, the view
// matrix at base + 5..8, the pass's 15 constants at 0x290): the transform of the vertex groups, the LOD morph toward the
// parents, the parents' substitutions, the list fix-ups, the strips' GIF packets (4). Returns
// false for a start it does not know. `out`: the packets it kicks, in order.
bool tfrag_kick(const uint8_t* vu_mem, uint32_t base, uint32_t start, std::vector<Kicked>& out, bool want_hp = false);
// (testing) what the tfrag reference did, summed over its calls: morphed vertices, vertices that took their parent's vertex
// info, list entries the fix-up replaced, vertices sent with ADC (the guard band)
struct TfragCounts {
    uint64_t morphed = 0, substituted = 0, fixed = 0, adc = 0, vertices = 0;
};
extern TfragCounts g_tfrag_counts;
// The tfrag B program's start 0x002 (the clipping program): its strips as 0x652 sends them (the records transformed there,
// the S / T offset the first prim's), each vertex flagged when the guard band test would send its triangles to clipping.
bool tfrag_b_kick(const uint8_t* vu_mem, uint32_t base, std::vector<Kicked>& out, bool want_hp = false);

}  // namespace rac1::mrpw

#endif
