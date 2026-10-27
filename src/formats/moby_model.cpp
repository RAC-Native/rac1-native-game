#include <formats/moby_model.h>
#include <array>
#include <formats/animation.h>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <limits>
#include <numeric>
#include <functional>

namespace rac1 {

namespace {

// Moby class geometry is a list of "packets" (the unit the game sends to VU1 in one go). The
// structures below are the on-disc records, little-endian and packed
// for how the game uses them.
#pragma pack(push, 1)

// One entry of the class's packet table.
struct PacketRecord {
    std::uint32_t vif_offset;          // the packet's VIF command list
    std::uint16_t vif_qwords;          // its length in quadwords (16 bytes)
    std::uint16_t texture_prim_qwords; // the texture primitives' length in quadwords (4 per primitive)
    std::uint32_t vertex_offset;       // the packet's vertex data
    std::uint8_t vertex_qwords;
    std::uint8_t position_unpack_qwords;
    std::uint8_t colour_unpack_qwords;
    std::uint8_t slot_count;           // vertices sent to VU1 (the UNPACK's NUM)
};
static_assert(sizeof(PacketRecord) == 16);

// The 32-byte head of a packet's vertex data.
struct VertexTableHead {
    std::uint32_t matrix_uploads;      // joint matrices copied to VU0 before the vertex loop
    std::uint32_t two_way_vertices;    // vertices blended from two matrices
    std::uint32_t three_way_vertices;  // ... from three
    std::uint32_t plain_vertices;      // vertices with one matrix
    std::uint32_t duplicates;          // vertices that repeat an earlier one (own texture coordinates)
    std::uint32_t slot_count;          // same as PacketRecord::slot_count
    std::uint32_t array_offset;        // the vertex array, from the start of the vertex data
    std::uint32_t tail_offset;         // end of the array and its trailing records, from the same point
};
static_assert(sizeof(VertexTableHead) == 32);

// One vertex record. The low 9 bits of the first halfword are the vertex's slot number and the
// upper 7 a joint number, but the slot number belongs to the vertex seven records earlier (the
// game's vertex loop is software-pipelined).
struct PackedVertex {
    std::uint16_t index_and_joint;
    std::uint8_t blend[6];             // VU0 matrix addresses and weights; meaning depends on the kind
    std::uint8_t normal_azimuth;
    std::uint8_t normal_elevation;
    std::int16_t x, y, z;              // fixed point, 1/1024 of the class scale
};
static_assert(sizeof(PackedVertex) == 16);

// The header of the second UNPACK (the index list).
struct IndexListHeader {
    std::uint8_t cull_mode;
    std::uint8_t texture_prim_offset;  // in quadwords
    std::int8_t first_switch_slot;
    std::uint8_t pad;
};
static_assert(sizeof(IndexListHeader) == 4);

// A GS register write as an "A+D" quadword: two data words, the register number, padding.
struct GifRegWrite {
    std::int32_t lo, hi;
    std::uint8_t reg;
    std::uint8_t pad[3];
};
static_assert(sizeof(GifRegWrite) == 12);

// A texture primitive: four register writes (TEX1_1, CLAMP_1, TEX0_1, MIPTBP1_1), each followed by
// a word the VU1 program reads as the slot of the next vertex after a texture switch.
struct TexturePrim {
    struct Quad {
        GifRegWrite write;
        std::uint32_t next_slot;
    } quad[4];
};
static_assert(sizeof(TexturePrim) == 64);

struct TexCoord {
    std::int16_t s, t;
};
static_assert(sizeof(TexCoord) == 4);

// One joint matrix copied from the joint table into VU0 memory before the vertex loop.
struct MatrixUpload {
    std::uint8_t joint;
    std::uint8_t vu0_address;
};
static_assert(sizeof(MatrixUpload) == 2);

#pragma pack(pop)

struct VifPacket {
    std::uint32_t cmd;
    std::uint32_t num;
    std::uint32_t vnvl;
    std::uint32_t addr;
    std::vector<std::uint8_t> data;
};

std::vector<VifPacket> parse_vif_packets(std::span<const std::byte> stream) {
    std::vector<VifPacket> packets;
    std::size_t ofs = 0;
    while (ofs + 4 <= stream.size()) {
        std::uint32_t val;
        std::memcpy(&val, stream.data() + ofs, 4);
        
        std::uint32_t cmd = (val >> 24) & 0x7f;
        std::uint32_t num = (val >> 16) & 0xff;
        if (num == 0) num = 256;
        
        bool is_unpack = (cmd & 0x60) == 0x60;
        
        std::size_t packet_size_quads = 1;
        if (cmd == 0x20) { // STMASK
            packet_size_quads = 2;
        } else if (cmd == 0x30 || cmd == 0x31) { // STROW, STCOL
            packet_size_quads = 5;
        } else if (cmd == 0x4a) { // MPG
            packet_size_quads = 1 + num * 2;
        } else if (cmd == 0x50 || cmd == 0x51) { // DIRECT, DIRECTHL
            std::uint32_t size = val & 0xffff;
            if (size == 0) size = 65536;
            packet_size_quads = 1 + size * 4;
        } else if (is_unpack) {
            std::uint32_t vnvl = (val >> 24) & 0xf;
            std::uint32_t vl = vnvl & 3;
            std::uint32_t vn = (vnvl >> 2) & 3;
            std::uint32_t element_size = ((32 >> vl) * (vn + 1)) / 8;
            std::uint32_t total_bytes = num * element_size;
            if (total_bytes % 4 != 0) {
                total_bytes += 4 - (total_bytes % 4);
            }
            packet_size_quads = 1 + (total_bytes / 4);
        }
        
        std::size_t packet_size_bytes = packet_size_quads * 4;
        if (ofs + packet_size_bytes > stream.size()) {
            break;
        }
        
        VifPacket pkt;
        pkt.cmd = cmd;
        pkt.num = num;
        pkt.vnvl = (val >> 24) & 0xf;
        pkt.addr = val & 0x3ff;
        pkt.data.resize(packet_size_bytes - 4);
        if (!pkt.data.empty()) {
            std::memcpy(pkt.data.data(), stream.data() + ofs + 4, pkt.data.size());
        }
        
        packets.push_back(std::move(pkt));
        ofs += packet_size_bytes;
    }
    return packets;
}

std::vector<VifPacket> filter_vif_unpacks(const std::vector<VifPacket>& pkts) {
    std::vector<VifPacket> res;
    for (const auto& p : pkts) {
        if ((p.cmd & 0x60) == 0x60) {
            res.push_back(p);
        }
    }
    return res;
}

} // namespace

namespace {

// The skin of a vertex before it is reduced to four joints: a weighted mix of joints.
struct JointMix {
    int count = 0;
    int joints[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    float weights[8] = {0, 0, 0, 0, 0, 0, 0, 0};
};

JointMix one_joint(int joint, float weight) {
    JointMix m;
    m.count = 1;
    m.joints[0] = joint;
    m.weights[0] = weight;
    return m;
}

// a * wa + b * wb (+ c * wc), the weights being the vertex's bytes taken as fractions of their sum.
JointMix blend(std::initializer_list<std::pair<const JointMix*, float>> parts) {
    JointMix out;
    float total = 0.0f;
    for (const auto& p : parts) total += p.second;
    if (total <= 0.0f) total = 1.0f;
    for (const auto& p : parts) {
        const JointMix& src = *p.first;
        for (int k = 0; k < src.count; ++k) {
            int slot = -1;
            for (int q = 0; q < out.count; ++q) {
                if (out.joints[q] == src.joints[k]) slot = q;
            }
            if (slot < 0 && out.count < 8) {
                slot = out.count++;
                out.joints[slot] = src.joints[k];
                out.weights[slot] = 0.0f;
            }
            if (slot >= 0) out.weights[slot] += src.weights[k] * (p.second / total);
        }
    }
    return out;
}

// VU0 memory as the vertex loop uses it: 64 matrix slots, addressed in units of four quadwords.
class MatrixSlots {
public:
    const JointMix& load(int address) const {
        static const JointMix empty{};
        const int slot = address / 4;
        return slot >= 0 && slot < 64 ? slots_[slot] : empty;
    }
    void store(int address, const JointMix& mix) {
        const int slot = address / 4;
        if (slot >= 0 && slot < 64) slots_[slot] = mix;
    }

private:
    JointMix slots_[64];
};

// What a vertex's six blend bytes mean, by kind. The sources are read before anything is stored.
//   two-way:   [0] [1] source slots, [2] [3] their weights, [4] where the vertex's own joint
//              (named by the upper 7 index bits) is stored, [5] where the blend is stored
//   three-way: [0] [1] source slots, [2] [3] [4] weights, the third source slot is twice the
//              joint number in the index bits, [5] where the blend is stored
//   plain:     [0] the source slot, [1] where the vertex's own joint is stored
JointMix skin_of(const PackedVertex& v, bool two_way, bool three_way, MatrixSlots& vu0) {
    const int own_joint = (v.index_and_joint & 0xfe00) >> 9;
    const std::uint8_t* b = v.blend;
    if (two_way) {
        const JointMix a = vu0.load(b[0]);
        const JointMix c = vu0.load(b[1]);
        vu0.store(b[4], one_joint(own_joint, 1.0f));
        const JointMix mixed = blend({{&a, static_cast<float>(b[2])}, {&c, static_cast<float>(b[3])}});
        vu0.store(b[5], mixed);
        return mixed;
    }
    if (three_way) {
        const JointMix a = vu0.load(b[0]);
        const JointMix c = vu0.load(b[1]);
        const JointMix d = vu0.load(own_joint * 2);
        const JointMix mixed = blend({{&a, static_cast<float>(b[2])}, {&c, static_cast<float>(b[3])}, {&d, static_cast<float>(b[4])}});
        vu0.store(b[5], mixed);
        return mixed;
    }
    const JointMix only = vu0.load(b[0]);
    vu0.store(b[1], one_joint(own_joint, 1.0f));
    return only;
}

// The slot number of each vertex of the array. Vertex i's number is in the record i + 7: the
// records after the array (its trailing records) carry the numbers of the last vertices, and the
// last of them also holds six more numbers in its bytes 4..15. `base` is the start of the packet's
// vertex data, `count` the number of vertices in the array.
void recover_slot_numbers(std::span<const std::byte> blob, std::size_t base, const VertexTableHead& head,
                          std::vector<PackedVertex>& vertices) {
    const std::size_t count = vertices.size();
    if (count < 7) return;

    const std::size_t array_at = base + head.array_offset;
    const int trailing = static_cast<int>((head.tail_offset - head.array_offset) / 16 - count);
    auto set_number = [&](std::int64_t vertex, std::uint16_t word) {
        if (vertex < 0 || vertex >= static_cast<std::int64_t>(count)) return;
        PackedVertex& v = vertices[static_cast<std::size_t>(vertex)];
        v.index_and_joint = static_cast<std::uint16_t>((v.index_and_joint & ~0x1ff) | (word & 0x1ff));
    };

    // Records inside the array, then the trailing records.
    for (std::size_t r = 7; r < count; ++r) set_number(static_cast<std::int64_t>(r) - 7, vertices[r].index_and_joint);
    std::size_t last_read = count - 1;
    for (int r = 0; r < trailing; ++r) {
        const std::size_t at = array_at + (count + static_cast<std::size_t>(r)) * 16;
        if (at + 16 > blob.size()) break;
        std::uint16_t word;
        std::memcpy(&word, blob.data() + at, 2);
        set_number(static_cast<std::int64_t>(count) + r - 7, word);
        last_read = count + static_cast<std::size_t>(r);
    }

    // The six numbers in the last record read.
    const std::size_t at = array_at + last_read * 16;
    if (at + 16 > blob.size()) return;
    std::uint16_t extra[6];
    std::memcpy(extra, blob.data() + at + 4, sizeof(extra));
    for (int k = 0; k < 6; ++k) set_number(static_cast<std::int64_t>(count) + trailing + k - 7, extra[k]);
}

} // namespace

DecodedMobyModel decode_moby_class_model(
    std::int32_t o_class,
    std::span<const std::byte> moby_class_data,
    std::span<const std::uint8_t, 16> texture_remap,
    const std::vector<MobyPacketTriangles>* game
) {
    DecodedMobyModel model;
    model.o_class = o_class;

    if (moby_class_data.size() < 0x48) {
        return model;
    }

    // Class header: +0 packet table, +4 high-LOD packet count, +0x24 scale, +0x30 bounding sphere.
    std::int32_t packet_table_offset = 0;
    std::memcpy(&packet_table_offset, moby_class_data.data() + 0x00, 4);

    float scale = 1.0f;
    std::memcpy(&scale, moby_class_data.data() + 0x24, 4);
    model.scale = scale;

    std::array<float, 4> bounding_sphere = {0.0f, 0.0f, 0.0f, 0.0f};
    std::memcpy(bounding_sphere.data(), moby_class_data.data() + 0x30, 16);
    model.bounding_sphere = bounding_sphere;

    const std::uint8_t high_lod_count = static_cast<std::uint8_t>(moby_class_data[0x04]);

    if (packet_table_offset <= 0 || static_cast<std::size_t>(packet_table_offset) >= moby_class_data.size()) {
        return model;
    }

    std::unordered_map<int, std::vector<std::array<int, 3>>> global_mat_triangles;

    // VU0 memory is shared by all of the class's packets: a packet may use a matrix an earlier
    // one stored.
    MatrixSlots vu0;

    // The vertex slots of the VU1 vertex buffer keep their contents between packets, so a
    // duplicate in one packet can name a vertex of an earlier packet. Resetting this per packet
    // would lose about a quarter of the duplicates.
    std::array<DecodedVertex, 512> vertex_cache = {};
    std::array<bool, 512> vertex_cache_set = {};

    // The current material persists across packets: a packet that does not start with a texture
    // switch goes on with the previous packet's material.
    int current_material_index = texture_remap[0];

    // A vertex's skin: its (up to) four largest weights, normalised; the main joint.
    auto apply_skin = [](DecodedVertex& dv, const JointMix& a) {
        int order[8];
        for (int k = 0; k < 8; ++k) order[k] = k;
        std::sort(order, order + a.count, [&a](int x, int y) { return a.weights[x] > a.weights[y]; });
        float sum = 0.0f;
        const int n = std::min(a.count, 4);
        for (int k = 0; k < n; ++k) sum += a.weights[order[k]];
        for (int k = 0; k < 4; ++k) {
            dv.joints[k] = k < n ? a.joints[order[k]] : 0;
            dv.weights[k] = k < n && sum > 0.0f ? a.weights[order[k]] / sum : 0.0f;
        }
        if (n == 0) dv.weights[0] = 1.0f;
        dv.joint_idx = n > 0 ? a.joints[order[0]] : 0;
    };

    for (int i = 0; i < static_cast<int>(high_lod_count); ++i) {
        const std::size_t record_at = static_cast<std::size_t>(packet_table_offset) + i * sizeof(PacketRecord);
        if (record_at + sizeof(PacketRecord) > moby_class_data.size()) {
            break;
        }

        PacketRecord entry;
        std::memcpy(&entry, moby_class_data.data() + record_at, sizeof(entry));

        if (entry.vif_qwords == 0) {
            continue;
        }

        if (static_cast<std::size_t>(entry.vif_offset) + entry.vif_qwords * 16 > moby_class_data.size()) {
            continue;
        }

        std::span<const std::byte> vif_stream(
            moby_class_data.data() + entry.vif_offset,
            entry.vif_qwords * 16
        );

        auto vif_packets = parse_vif_packets(vif_stream);
        auto unpacks = filter_vif_unpacks(vif_packets);

        if (unpacks.size() < 2) {
            continue;
        }

        printf("Moby %d: Unpack 0 cmd=0x%x size=%zu, Unpack 1 cmd=0x%x size=%zu\n",
               o_class, unpacks[0].cmd, unpacks[0].data.size(), unpacks[1].cmd, unpacks[1].data.size());

        // The first UNPACK: texture coordinates, one per vertex slot.
        const auto& uv_unpack = unpacks[0];
        std::span<const TexCoord> sts(
            reinterpret_cast<const TexCoord*>(uv_unpack.data.data()),
            uv_unpack.data.size() / sizeof(TexCoord)
        );

        // The second UNPACK: the index list, a 4-byte header and then signed byte indices.
        const auto& index_unpack = unpacks[1];
        if (index_unpack.data.size() < sizeof(IndexListHeader)) {
            continue;
        }

        IndexListHeader idx_hdr;
        std::memcpy(&idx_hdr, index_unpack.data.data(), sizeof(IndexListHeader));

        // The VIF packet is padded to a multiple of 16 bytes, but NUM gives the exact count of
        // 4-byte elements.
        std::size_t actual_bytes = index_unpack.num * 4;
        if (actual_bytes > index_unpack.data.size()) {
            actual_bytes = index_unpack.data.size();
        }
        std::size_t num_indices = 0;
        if (actual_bytes > sizeof(IndexListHeader)) {
            num_indices = actual_bytes - sizeof(IndexListHeader);
        }

        std::vector<std::int8_t> vif_indices(num_indices);
        if (!vif_indices.empty()) {
            std::memcpy(vif_indices.data(), index_unpack.data.data() + sizeof(IndexListHeader), vif_indices.size());
        }

        // The index list is not trimmed: the walk below stops at the terminator (an index 0 whose
        // switch slot is 0), as the VU1 program does; the padding indices before it only make
        // degenerate triangles, which are dropped.

        // After an index 0 (a texture switch) the next vertex's slot comes from here: first the
        // list header's, then one per texture primitive.
        std::vector<std::int8_t> switch_slots;
        switch_slots.push_back(idx_hdr.first_switch_slot);

        std::vector<TexturePrim> textures;

        // The third UNPACK (optional): the texture primitives.
        if (unpacks.size() >= 3) {
            const auto& tex_unpack = unpacks[2];
            const std::size_t num_prims = tex_unpack.data.size() / sizeof(TexturePrim);

            for (std::size_t t = 0; t < num_prims; ++t) {
                // A primitive's switch slot is the last word of its first quadword (the primitive
                // t is at t * 0x40).
                const std::size_t slot_at = t * sizeof(TexturePrim) + 0xc;
                switch_slots.push_back(slot_at < tex_unpack.data.size() ? static_cast<std::int8_t>(tex_unpack.data[slot_at]) : 0);

                TexturePrim prim;
                std::memcpy(&prim, tex_unpack.data.data() + t * sizeof(TexturePrim), sizeof(TexturePrim));
                textures.push_back(prim);
            }
        }

        // The vertex data.
        if (entry.vertex_offset + sizeof(VertexTableHead) > moby_class_data.size()) {
            continue;
        }

        VertexTableHead head;
        std::memcpy(&head, moby_class_data.data() + entry.vertex_offset, sizeof(VertexTableHead));

        if (entry.slot_count != head.slot_count) {
            continue;
        }

        const std::size_t in_file_vertex_count = head.two_way_vertices + head.three_way_vertices + head.plain_vertices;
        const std::size_t vertex_array_at = entry.vertex_offset + head.array_offset;

        if (vertex_array_at + in_file_vertex_count * sizeof(PackedVertex) > moby_class_data.size()) {
            continue;
        }

        std::vector<PackedVertex> vertices(in_file_vertex_count);
        if (in_file_vertex_count > 0) {
            std::memcpy(vertices.data(), moby_class_data.data() + vertex_array_at, in_file_vertex_count * sizeof(PackedVertex));
        }
        recover_slot_numbers(moby_class_data, entry.vertex_offset, head, vertices);

        // After the joint uploads (padded to 8 bytes) comes the list of duplicates: a halfword each,
        // whose bits 7.. name the vertex slot being repeated.
        std::size_t duplicates_at = entry.vertex_offset + sizeof(VertexTableHead) + head.matrix_uploads * sizeof(MatrixUpload);
        if (duplicates_at % 4 != 0) {
            duplicates_at += 2;
        }
        if (duplicates_at % 8 != 0) {
            duplicates_at += 4;
        }

        std::vector<std::uint16_t> duplicate_vertices;
        if (head.duplicates != 0 && duplicates_at + head.duplicates * 2 <= moby_class_data.size()) {  // (none: no memcpy from a null data())
            duplicate_vertices.resize(head.duplicates);
            std::memcpy(duplicate_vertices.data(), moby_class_data.data() + duplicates_at, head.duplicates * 2);
        }

        // The class's animation data (the joint skinning lists).
        model.anims = decode_moby_animations(moby_class_data);

        // The joint matrices the packet uploads to VU0 before its vertex loop.
        if (head.matrix_uploads > 0) {
            const std::size_t uploads_at = entry.vertex_offset + sizeof(VertexTableHead);
            if (uploads_at + head.matrix_uploads * sizeof(MatrixUpload) <= moby_class_data.size()) {
                std::vector<MatrixUpload> uploads(head.matrix_uploads);
                std::memcpy(uploads.data(), moby_class_data.data() + uploads_at, head.matrix_uploads * sizeof(MatrixUpload));
                for (const MatrixUpload& u : uploads) {
                    // (these carry weight 255, the ones a vertex stores weight 1: as the decoder has always read them)
                    vu0.store(u.vu0_address, one_joint(u.joint, 255.0f));
                }
            }
        }

        // The vertices.
        std::vector<DecodedVertex> decoded_vertices;
        decoded_vertices.reserve(in_file_vertex_count + duplicate_vertices.size());

        for (std::size_t j = 0; j < in_file_vertex_count; ++j) {
            DecodedVertex dv;
            dv.x = vertices[j].x * (scale / 1024.f);
            dv.y = vertices[j].y * (scale / 1024.f);
            dv.z = vertices[j].z * (scale / 1024.f);

            // The normal is two angles: azimuth and elevation, 256 steps per half turn.
            const float azimuth = vertices[j].normal_azimuth * (3.1415926535f / 128.f);
            const float elevation = vertices[j].normal_elevation * (3.1415926535f / 128.f);
            dv.nx = std::sin(azimuth) * std::cos(elevation);
            dv.ny = std::cos(azimuth) * std::cos(elevation);
            dv.nz = std::sin(elevation);

            if (j < sts.size()) {
                dv.u = sts[j].s / 4096.0f;
                dv.v = sts[j].t / 4096.0f;
            } else {
                dv.u = 0.0f;
                dv.v = 0.0f;
            }

            dv.r = dv.g = dv.b = dv.a = 255;

            const bool two_way = j < head.two_way_vertices;
            const bool three_way = !two_way && j < head.two_way_vertices + head.three_way_vertices;
            apply_skin(dv, skin_of(vertices[j], two_way, three_way, vu0));

            decoded_vertices.push_back(dv);
            const int slot = vertices[j].index_and_joint & 0x1ff;
            vertex_cache[slot] = dv;
            vertex_cache_set[slot] = true;
        }

        for (std::size_t j = 0; j < duplicate_vertices.size(); ++j) {
            const int slot = (duplicate_vertices[j] >> 7) & 0x1ff;
            DecodedVertex dv;
            if (vertex_cache_set[slot]) {
                dv = vertex_cache[slot];
            } else {
                // A duplicate of a vertex nothing wrote: a NaN position, so that the triangles
                // using it are dropped below.
                dv.x = std::numeric_limits<float>::quiet_NaN();
                dv.y = std::numeric_limits<float>::quiet_NaN();
                dv.z = std::numeric_limits<float>::quiet_NaN();
                dv.nx = 0.0f; dv.ny = 0.0f; dv.nz = 1.0f;
                dv.r = dv.g = dv.b = dv.a = 0;
                dv.joint_idx = 0;
            }

            const std::size_t sts_idx = in_file_vertex_count + j;
            if (sts_idx < sts.size()) {
                dv.u = sts[sts_idx].s / 4096.0f;
                dv.v = sts[sts_idx].t / 4096.0f;
            } else {
                dv.u = 0.0f;
                dv.v = 0.0f;
            }
            decoded_vertices.push_back(dv);
        }

        // Both the vertices and the duplicates go into the model as separate entries: a duplicate
        // has the position and joints of the vertex it repeats but its own texture coordinates, and
        // a triangle with two corners on the same entry would be dropped.
        const int vertex_offset = static_cast<int>(model.vertices.size());
        for (const auto& v : decoded_vertices) {
            model.vertices.push_back(v);
        }

        // The packet's VU1 slots (0-based here) to model vertices: the vertices first, then the duplicates.
        std::vector<int> vu1_to_global;
        for (std::size_t j = 0; j < in_file_vertex_count; ++j) {
            vu1_to_global.push_back(vertex_offset + static_cast<int>(j));
        }
        for (std::size_t j = 0; j < duplicate_vertices.size(); ++j) {
            vu1_to_global.push_back(vertex_offset + static_cast<int>(in_file_vertex_count) + static_cast<int>(j));
        }

        if (game != nullptr && i < static_cast<int>(game->size())) {
            // the game's triangles (the packet's kick program): slots -> this packet's vertices
            const MobyPacketTriangles& gt = (*game)[static_cast<size_t>(i)];
            for (size_t t = 0; t < gt.slots.size(); ++t) {
                const int16_t pr = gt.prim[t];
                if (pr >= 0 && pr < static_cast<int>(textures.size())) {
                    const int local_tex = textures[static_cast<size_t>(pr)].quad[2].write.lo;
                    current_material_index = (local_tex >= 0 && local_tex < 16) ? texture_remap[local_tex] : local_tex;
                }
                int g[3];
                bool ok = true;
                for (int c = 0; c < 3; ++c) {
                    const int s = gt.slots[t][static_cast<size_t>(c)] - 1;
                    ok = ok && s >= 0 && s < static_cast<int>(vu1_to_global.size());
                    g[c] = ok ? vu1_to_global[static_cast<size_t>(s)] : -1;
                }
                if (!ok || g[0] == g[1] || g[1] == g[2] || g[0] == g[2]) continue;
                if (std::isnan(model.vertices[g[0]].x) || std::isnan(model.vertices[g[1]].x) || std::isnan(model.vertices[g[2]].x)) continue;
                global_mat_triangles[current_material_index].push_back({g[0], g[1], g[2]});
            }
            continue;
        }
        // The strip as the VU1 program walks it: an index > 0
        // names its slot, < 0 the slot + 128 sent with ADC (it closes no triangle), 0 a texture
        // switch: the next secret index names the slot, a new GIF packet restarts the GS strip.
        // (The legacy walk used the raw bytes two and one back as the triangle's corners: the
        // first triangle after each switch had the 0 byte as a corner and was dropped.)
        int switch_count = 0;
        int queue[2] = {-1, -1};
        int queued = 0;
        int tri_in_strip = 0;  // (winding alternated per triangle of a strip, as before)
        for (std::size_t j = 0; j < vif_indices.size(); ++j) {
            const std::int8_t index = vif_indices[j];
            int slot = 0;
            bool adc = false;
            if (index == 0) {
                if (switch_count >= static_cast<int>(switch_slots.size())) break;
                const std::int8_t switch_slot = switch_slots[switch_count];
                if (switch_slot == 0) break;  // the packet's end
                slot = switch_slot;
                int local_tex = 0;
                if (switch_count < static_cast<int>(textures.size())) local_tex = textures[switch_count].quad[2].write.lo;
                current_material_index = (local_tex >= 0 && local_tex < 16) ? texture_remap[local_tex] : local_tex;
                switch_count++;
                queued = 0;  // (the GIF packet's PRIM: a new strip)
                tri_in_strip = 0;
            } else if (index < 0) {
                slot = index + 128;
                adc = true;
                tri_in_strip = 0;
            } else {
                slot = index;
            }
            const int cur = slot - 1;
            if (!adc && queued >= 2) {
                int idx0 = queue[0], idx1 = queue[1];
                const int idx2 = cur;
                if ((tri_in_strip & 1) != 0) std::swap(idx0, idx1);
                tri_in_strip++;
                if (idx0 >= 0 && idx0 < static_cast<int>(vu1_to_global.size()) && idx1 >= 0 && idx1 < static_cast<int>(vu1_to_global.size()) && idx2 >= 0 &&
                    idx2 < static_cast<int>(vu1_to_global.size())) {
                    const int g0 = vu1_to_global[idx0], g1 = vu1_to_global[idx1], g2 = vu1_to_global[idx2];
                    if (g0 != g1 && g1 != g2 && g2 != g0) {
                        const auto& v0 = model.vertices[g0];
                        const auto& v1 = model.vertices[g1];
                        const auto& v2 = model.vertices[g2];
                        // (a duplicate the class never wrote: NaN, dropped)
                        if (!std::isnan(v0.x) && !std::isnan(v1.x) && !std::isnan(v2.x)) global_mat_triangles[current_material_index].push_back({g0, g1, g2});
                    }
                }
            }
            queue[0] = queue[1];
            queue[1] = cur;
            queued = std::min(queued + 1, 2);
        }
    }

    for (auto& [mat_idx, tris] : global_mat_triangles) {
        DecodedSubMesh sm;
        sm.material_index = mat_idx;
        sm.triangles = std::move(tris);
        model.submeshes.push_back(std::move(sm));
    }

    return model;
}

} // namespace rac1
