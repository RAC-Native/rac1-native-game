// The MRP moby asset (ADR 0010 step 1): a moby class as the game draws it, decoded
// from the class's own data (the same bytes the moby pass's DMA chains send), generated from the
// ISO by the setup tool (src/setup/mrp_export.cpp) into assets/generated/mrp/moby/level<N>/<class>.mrpm and proved
// against the faithful VU0 / VU1 path by Format.
//
// Per packet (one VU1 program start) the asset keeps the raw bytes (the class's packet entry, its
// VIF list, its vertex data) and their decoding:
//   - the VU1 vertex slots 1..N (the order the skinning writes them and the index list names
//     them): a unique vertex record or a duplicate of an earlier record (by the skinning's vertex
//     cache), each with its UV (the VIF list's V2_16 unpack);
//   - the index list as the VU1 program walks it: per entry the slot, the ADC flag (a negative
//     index: the vertex is sent with the GS's "no drawing" bit), the texture switches (an index 0:
//     the A+D packet k of the packet's texture prims, then the slot its secret index names);
//   - the texture prims (TEX1 / CLAMP / TEX0 / MIPTBP1 A+D data as the class stores them, TEX0's
//     low bits the class's local texture index);
//   - the skinning: the matrix transfers (joint -> VU0 matrix address), per unique vertex its
//     kind (2-way / 3-way blend / single matrix), its VU0 matrix addresses and weights, its joint,
//     its bind position (s16) and normal angles, its vertex colour (RGBA bytes, the skinning
//     multiplies the lit colour by it), its vertex-cache index.
#ifndef RAC1_FORMATS_MOBY_MRP_H
#define RAC1_FORMATS_MOBY_MRP_H

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rac1::mrp {

constexpr uint32_t kMobyMagic = 0x4d50524du;  // "MRPM"
constexpr uint32_t kMobyVersion = 1;

enum class VertexKind : uint8_t { TwoWay = 0, ThreeWay = 1, Single = 2 };

// One unique vertex record of a packet's vertex data (16 bytes in the class, decoded).
struct SkinVertex {
    VertexKind kind = VertexKind::Single;
    uint8_t joint = 0;          // bits 9..15 of the record's first halfword: the joint matrix the EE hands the VU0 (2-way / single: stored at `store`)
    uint16_t cache_index = 0;   // the vertex-cache entry its skinned result is written to (the 9 bits the record 7 places later holds)
    uint8_t src[3] = {0, 0, 0}; // VU0 matrix addresses (quadwords) read: 2-way src[0..1], 3-way src[0..2] (src[2] = (halfword >> 8) & 0xfe), single src[0]
    uint8_t weight[3] = {0, 0, 0};  // the blend weights (/ 256): 2-way [0..1], 3-way [0..2]
    uint8_t store = 0;          // 2-way / single: the VU0 address the joint matrix is stored at (before the matrices are read)
    uint8_t blend_store = 0;    // 2-way / 3-way: the VU0 address the blended matrix is stored at
    uint8_t normal[2] = {0, 0}; // the normal's angle bytes (table indices: the normal is (t[a].x * t[e].x, t[a].y * t[e].x, t[e].y))
    int16_t pos[3] = {0, 0, 0}; // bind-space position (class units: x scale / 1024 = metres)
    uint8_t rgba[4] = {0x80, 0x80, 0x80, 0x80};  // vertex colour (0x80 = 1.0)
    uint8_t raw[16] = {};       // the record as stored
};

// A texture switch's A+D data. The class file stores four short words per prim; the level load
// (0x2406b0 -> 0x2400f0) turns them into the GS registers, with the level's texture table entry
// of the texture: `gs` is that result with the VRAM addresses relative to the level's GS RAM block
// (add *(0x15ee8c) >> 8 to CBP / TBP2 / TBP3) and TBP0 / TBP1 left 0 (the frame's VRAM slots of
// mips 0 / 1, 0x24fad0).
struct TexturePrim {
    uint64_t file[4] = {};      // the four qwords' data as the class file stores them
    uint8_t reg[4] = {};        // their GS register addresses (A+D): TEX1, CLAMP, TEX0, MIPTBP1
    int8_t next_secret = 0;     // the slot the next texture switch takes (0: the packet's end)
    int32_t local_texture = -1; // the file's TEX0 word: the class's local texture (0..15), or -1 none / -2 chrome / -3 glass
    int32_t texture = -1;       // the level moby texture (the class's remap of local_texture), or the special value
    uint64_t gs[4] = {};        // TEX1, CLAMP, TEX0, MIPTBP1 as the load builds them (see above)
    bool gs_known = false;      // false: built from a table the class file does not name (an item class's), or chrome / glass
    uint8_t tex_entry[16] = {}; // the level's texture table entry 0x2400f0 read (width +4, height +6, MXL + 1 +8, CBP +0xa, TBP2 +0xc, TBP3 +0xe)
    bool has_entry = false;
};

// One entry of the index list, as the VU1 program walks it.
struct StripOp {
    uint8_t slot = 0;           // 1..N
    bool adc = false;           // sent with ADC (no triangle closes at it)
    int16_t texture = -1;       // >= 0: a texture switch before this vertex (the A+D prim index), the GS strip restarted
};

// One step of the GS stream the VU1 program kicks for a packet (its "kick program"), derived by
// running the ported program (micro 0x22d, moby_vu1.cpp) once on inputs that name their slot
// (src/setup/kick_trace.cpp): the program's own pipeline decides which slot's
// position / colour / UV and which vertex's Q reach each GS vertex -- the index list is its input.
enum class KickKind : uint8_t { Vertex = 0, Restart = 1, Texture = 2 };
struct KickOp {
    KickKind kind = KickKind::Vertex;
    uint8_t pos = 0;    // the slot whose position (by the moby's matrix rows) is projected
    uint8_t pos_q = 0;  // the slot whose Q (proj / w) the projection uses (0xff: the Q register as the program started)
    uint8_t col = 0;    // the slot whose colour is sent
    uint8_t uv = 0;     // the slot whose UV is sent
    uint8_t uv_q = 0;   // STQ = (s, t, 1) / 4096 x Q(uv_q) [x Q(uv_q2)]
    uint8_t uv_q2 = 0;
    bool adc = false;   // the vertex closes no triangle
    int16_t texture = -1;  // Texture: the packet's prim index
};

struct Packet {
    uint8_t lod = 0;            // 0 high, 1 low, 2 the shadow mesh (class +6 / +7)
    uint8_t index_in_lod = 0;
    uint8_t entry[16] = {};     // the packet table entry (pointers as offsets into the class)
    std::vector<uint8_t> vif;   // the VIF list (UNPACKs of UVs, index list, texture prims)
    std::vector<uint8_t> vdata; // the vertex data (the skinning's input)
    // decoded
    uint32_t n_slots = 0;       // VU1 vertex slots (the entry's +0xf)
    std::vector<std::array<int16_t, 2>> uv;  // per slot (s, t: / 4096)
    int8_t index_header[4] = {}; // the index list's header (x: culling mode, y: texture prims' qword offset, z: the first switch's slot)
    std::vector<int8_t> indices;  // the index list as unpacked (signed bytes)
    std::vector<TexturePrim> textures;
    std::vector<StripOp> strip;
    std::vector<KickOp> kicks;  // the GS stream as the program assembles it (empty: not derived)
    std::vector<KickOp> kicks_clip;  // the same from the clipping program (micro 0x1a) for triangles it keeps whole
    bool kicks_ok = false;      // false: the trace met a vertex it could not attribute (see kicks_note)
    std::string kicks_note;
    uint16_t vif_uv_addr = 0, vif_index_addr = 0, vif_tex_addr = 0;  // the UNPACK addresses (relative to the buffer base)
    uint32_t vif_tex_off = 0;   // the texture prims' offset in the VIF list (their TEX0 / MIPTBP1 TBP are patched per frame)
    // skinning
    std::vector<std::array<uint8_t, 2>> matrix_transfers;  // (joint, VU0 address)
    std::vector<SkinVertex> vertices;  // unique records, in output order (slots 1..n)
    std::vector<uint16_t> duplicates;  // per duplicate slot: the vertex-cache index it copies
    // a duplicate's source resolved through the vertex cache of the class's packets drawn in
    // order (same LOD): (packet index in the class's table, vertex index); -1: never written by
    // this class (the cache holds another moby's data)
    std::vector<std::array<int32_t, 2>> duplicate_source;
    uint32_t stale_matrix_reads = 0;  // matrix reads of VU0 addresses no packet of this LOD wrote before (another moby's data)
};

struct MobyClass {
    int32_t level = -1;
    int32_t o_class = -1;
    bool gadget = false;
    std::vector<uint8_t> header;  // the class header (0x48 bytes, pointers as offsets)
    uint8_t lod_count[2] = {};    // +4 / +5
    uint8_t shadow_count = 0, shadow_first = 0;  // +6 / +7
    uint8_t joints_hi = 0, joints_lo = 0;        // +8 / +9 (0: not animated at that LOD)
    uint8_t tint_first[2] = {};   // +0xa / +0xb: the first packet the sub-mesh colour (moby +0x90) replaces the colours of (0xff: none)
    float scale = 1.0f;           // +0x24
    std::array<float, 4> bsphere = {};
    std::array<uint8_t, 16> texture_remap = {};  // the level's moby texture of each local index (the class table entry)
    std::vector<Packet> packets;  // the packet table, every entry any LOD uses
    // the skeleton as stored (for the renderer's bind pose): joint count, the inverse bind matrices
    // (+0x14, 0x40 each), the bone translations / parents (+0x18, 0x10 each)
    uint32_t joint_count = 0;
    std::vector<std::array<float, 16>> inverse_bind;
    std::vector<std::array<uint8_t, 16>> joint_binds;
};

// Decodes a class from its bytes. `base`: the value the stored pointers are relative to (0 for
// the class file, the class's EE address for the level's copy in memory).
// `tex_table`: the level's moby texture table (16-byte entries, the core's moby_textures), for the
// texture prims' GS registers (empty: not built).
bool decode_class(std::span<const uint8_t> cls, uint32_t base, MobyClass& out, std::string& err, std::span<const uint8_t> tex_table = {});
// The load's GS registers of a texture prim (0x2400f0 / 0x240348's special cases; see TexturePrim).
void build_prim_gs(TexturePrim& t, std::span<const uint8_t> tex_table);
// The same registers for the level's GS RAM base `vram` (*(0x15ee8c) >> 8): what the class's packets hold in memory.
bool prim_gs(const TexturePrim& t, int32_t vram, uint64_t gs[4]);

bool write_class(const std::string& path, const MobyClass& c);
bool read_class(const std::string& path, MobyClass& c);

// A level's moby textures with every mip level as the level stores them (8-bit indices + the
// 256-entry palette as stored, CSM1 order and the GS's 0..0x80 alpha).
struct Texture {
    uint32_t index = 0;
    uint32_t width = 0, height = 0;
    std::array<uint32_t, 256> palette = {};
    std::vector<std::vector<uint8_t>> mips;  // mip 0 = width x height, then halves (0, 1: the level's texture data; 2, 3: its GS RAM block)
};
constexpr uint32_t kTexMagic = 0x5450524du;  // "MRPT"
bool write_textures(const std::string& path, const std::vector<Texture>& t);
bool read_textures(const std::string& path, std::vector<Texture>& t);

// ---- the CPU reference of the faithful path (the MRP's spec, proved against the original) ----

// The VU0 skinning of one packet: the VU0 matrix memory (256 quadwords, persistent across the
// packets of one moby), the joint matrices (scratchpad 0x0 + 0x40 j), the light rows (the
// skinning record's +0x40..+0xbf as the scratchpad 0x1c00 copy), the normal table (scratchpad
// 0x3800: 256 x 2 floats), the vertex cache (512 x (s16 x3 position, RGBA)). Writes the
// packet's VU1 input: per slot the position (s16 x3) and colour (RGBA).
struct SkinState {
    std::array<std::array<float, 4>, 256> vu0{};
    std::array<std::array<int16_t, 3>, 512> cache_pos{};
    std::array<std::array<uint8_t, 4>, 512> cache_rgba{};
};
struct SkinInput {
    const float (*joints)[16] = nullptr;  // joint j: 4 rows of 4 floats
    uint32_t joint_count = 0;
    std::array<std::array<float, 4>, 8> light{};  // the 8 light rows
    const float (*normal_table)[2] = nullptr;     // 256 entries
};
void skin_packet(const Packet& p, const SkinInput& in, SkinState& st, std::vector<std::array<int16_t, 3>>& pos,
                 std::vector<std::array<uint8_t, 4>>& rgba);

// The VU1 moby program's per-moby constants (its data memory at the buffer base B; the chain
// sends them: B+291..B+296 with the frame's view, B+297..B+300 per moby).
struct Vu1Consts {
    float rows[4][4] = {};  // B+297..B+300: the model-to-screen rows (x, y, z; translation, its w the fog)
    float offset[4] = {};   // B+296: added after the projection (screen centre, depth offset; w the fog offset)
    float proj = 0.0f;      // B+292 x: the projection numerator (Q = proj / w)
    float fog_min = 0.0f;   // B+291 x: the fog's floor
    float q_start = 0.0f;   // the VU1 Q register as the program starts (the previous run's last DIV): a few
                            // vertices after some texture switches take it (KickOp 0xff)
};
// One GS vertex as the program kicks it (STQ, RGBAQ, XYZF2).
struct GsVertex {
    float s = 0, t = 0, q = 0;
    int32_t rgba[4] = {};
    uint32_t x = 0, y = 0, z = 0, f = 0;
    bool adc = false;
};
// A slot's vertex: position (s16 x3 as the skinning wrote it), colour, UV (s, t; q = 4096).
GsVertex vu1_vertex(const Vu1Consts& k, const int32_t pos[3], const int32_t rgba[4], const int32_t uv[2], bool adc);
// The packet's triangles as the program assembles them (a texture switch restarts the strip),
// each with the texture prim in effect (-1: the one before the packet).
struct GsTriangle {
    GsVertex v[3];
    int texture = -1;
    uint8_t slot[3] = {};
};
// The same from the packet's kick program (the program's real GS stream: its pipeline's choices).
// `clip`: the form the clipping program (micro 0x1a, starts 0x008 / 0x00a) gives the triangles it
// does not clip or cull: the fog per vertex, F = min(max(p.w + offset.w, fog_min), 255), where the
// no-clip program takes the moby's (T.w + offset.w).
void kick_triangles(const Packet& p, const Vu1Consts& k, const std::vector<std::array<int32_t, 3>>& pos, const std::vector<std::array<int32_t, 4>>& rgba,
                    std::vector<GsTriangle>& out, bool clip = false);

}  // namespace rac1::mrp

#endif
