#include <formats/level_mesh.h>
#include <formats/level_file.h>
#include <formats/scene_wad.h>
#include <formats/tie_packet.h>
#include <formats/vif_stream.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <unordered_map>

namespace rac1 {

namespace {

template <class T>
T get(std::span<const std::byte> b, std::size_t at) {
    T v;
    std::memcpy(&v, b.data() + at, sizeof(T));
    return v;
}

// ---- tfrags ----

// The tfrag block: (table offset, count, two more words) then the 0x40-byte entries.
constexpr std::size_t kTfragEntryBytes = 0x40;

// The fields of a tfrag entry this decoder uses (byte offsets in the entry). The VIF lists are
// relative to the entry's data; "qwords" are 16 bytes.
struct TfragEntry {
    float sphere[4];                  // +0x00
    std::int32_t data;                // +0x10 from the entry table
    std::uint16_t far_list;           // +0x14 the far image (LOD 2)
    std::uint16_t shared_list;        // +0x16 the shared groups (every nearer image starts with them)
    std::uint16_t shared_end;         // +0x18 their end
    std::uint16_t lod_list;           // +0x1a the mid groups, then the near groups
    std::uint16_t colours;            // +0x1e the lit colours
    std::uint8_t shared_qwords;       // +0x20 the shared groups' qwords
    std::uint8_t far_qwords;          // +0x21 the far image's qwords
    std::uint8_t mid_qwords;          // +0x22 the mid image's qwords (from shared_list)
    std::uint8_t colour_qwords;       // +0x29 (four colours each)
    std::uint16_t normals;            // +0x30 the vertex normals start 0x10 bytes later
    std::uint8_t vertex_count;        // +0x3c
};

TfragEntry tfrag_entry(std::span<const std::byte> e) {
    TfragEntry t;
    std::memcpy(t.sphere, e.data(), 16);
    t.data = get<std::int32_t>(e, 0x10);
    t.far_list = get<std::uint16_t>(e, 0x14);
    t.shared_list = get<std::uint16_t>(e, 0x16);
    t.shared_end = get<std::uint16_t>(e, 0x18);
    t.lod_list = get<std::uint16_t>(e, 0x1a);
    t.colours = get<std::uint16_t>(e, 0x1e);
    t.shared_qwords = get<std::uint8_t>(e, 0x20);
    t.far_qwords = get<std::uint8_t>(e, 0x21);
    t.mid_qwords = get<std::uint8_t>(e, 0x22);
    t.colour_qwords = get<std::uint8_t>(e, 0x29);
    t.normals = get<std::uint16_t>(e, 0x30);
    t.vertex_count = get<std::uint8_t>(e, 0x3c);
    return t;
}

#pragma pack(push, 1)
struct Position16 { std::int16_t x, y, z; };                    // V3_16 records
struct VertexInfo { std::int16_t s, t, parent, record; };       // V4_16: s, t (1/4096), parent, record (x 2)
struct StripCode { std::int8_t count, end_of_packet, prim, pad; };
struct Colour { std::uint8_t r, g, b, a; };
struct Normal { std::int8_t word0, intensity, azimuth, elevation; std::int16_t colour, pad; };
struct TexturePrim { std::int32_t tex0_lo, tex0_hi; std::uint8_t pad0[8]; std::uint8_t tex1[16];
                     std::int32_t clamp_lo, clamp_hi; std::uint8_t pad1[8]; std::uint8_t miptbp[32]; };
#pragma pack(pop)
static_assert(sizeof(Position16) == 6 && sizeof(VertexInfo) == 8 && sizeof(StripCode) == 4);
static_assert(sizeof(Normal) == 8 && sizeof(TexturePrim) == 80);

// An UNPACK's records into `out` (resized to the UNPACK's count; the bytes it has copied in).
template <class T>
void unpack_into(std::vector<T>& out, const VifCommand& u) {
    out.resize(u.num);
    std::memcpy(out.data(), u.payload.data(), std::min(u.payload.size(), std::size_t{u.num} * sizeof(T)));
}

constexpr std::uint32_t kV4_16 = 0b1101;
constexpr std::uint32_t kV3_16 = 0b1001;

// One face of the near image's strips: a quad, or a triangle (corner 3 = -1), and the texture prim
// in effect.
struct StripFace {
    int prim;
    int corner[4];
};

std::vector<StripFace> strip_faces(const std::vector<StripCode>& codes, const std::vector<std::uint8_t>& index) {
    std::vector<StripFace> faces;
    int prim = -1;
    int first = 0;
    for (const StripCode& c : codes) {
        int n = c.count;
        if (n <= 0) {  // 0 ends the list; a negative count switches texture first (count + 128 corners)
            if (n == 0) break;
            if (c.prim >= 0) prim = c.prim / 5;
            n += 128;
        }
        if (first + n > static_cast<int>(index.size())) break;
        if (n % 2 == 0) {  // an even count: quads, two corners at a time
            for (int i = 0; i + 2 < n; i += 2) {
                const int b = first + i;
                faces.push_back({prim, {index[b + 2], index[b + 3], index[b + 1], index[b]}});
            }
        } else {
            for (int i = 0; i + 2 < n; ++i) {
                const int b = first + i;
                faces.push_back({prim, {index[b], index[b + 1], index[b + 2], -1}});
            }
        }
        first += n;
    }
    return faces;
}

// The tfrag's near image as a mesh: the shared, mid and near groups' vertices, the near strips.
// The mid image is shared_list's first mid_qwords qwords: the shared groups, then (from lod_list)
// the mid groups; the near groups follow it.
DecodedMesh decode_tfrag(std::span<const std::byte> data, const TfragEntry& t, std::size_t number) {
    const std::vector<VifCommand> shared = split_vif_commands(data.subspan(t.shared_list, t.shared_end - t.shared_list));
    const std::vector<VifCommand> mid = vif_unpacks(
        split_vif_commands(data.subspan(t.lod_list, t.shared_list + t.mid_qwords * 16 - t.lod_list)));
    const std::vector<VifCommand> near = vif_unpacks(split_vif_commands(data.subspan(
        t.shared_list + t.mid_qwords * 16, t.colours - (t.mid_qwords + t.far_qwords - t.shared_qwords) * 16)));

    // The position base: the shared list's second STROW.
    std::int32_t base[4] = {0, 0, 0, 0};
    int strows = 0;
    for (const VifCommand& c : shared) {
        if (c.op != 0x30) continue;
        if (++strows == 2) {
            if (c.payload.size() >= 16) std::memcpy(base, c.payload.data(), 16);
            break;
        }
    }

    // Shared groups: [0] the list header, [1] the texture prims, [2] vertex infos, [3] positions.
    std::vector<TexturePrim> prims;
    std::vector<VertexInfo> infos_shared, infos_mid, infos_near;
    std::vector<Position16> pos_shared, pos_mid, pos_near;
    const std::vector<VifCommand> shared_unpacks = vif_unpacks(shared);
    if (shared_unpacks.size() >= 4) {
        unpack_into(prims, shared_unpacks[1]);
        unpack_into(infos_shared, shared_unpacks[2]);
        unpack_into(pos_shared, shared_unpacks[3]);
    }
    for (const VifCommand& u : mid) {
        if (u.format == kV4_16) unpack_into(infos_mid, u);
        else if (u.format == kV3_16) unpack_into(pos_mid, u);
    }
    std::vector<StripCode> codes;
    std::vector<std::uint8_t> index;
    if (near.size() >= 3) {
        unpack_into(pos_near, near[0]);
        unpack_into(codes, near[1]);
        index = near[2].payload;
        for (std::size_t k = 3; k < near.size(); ++k) {
            if (near[k].format == kV4_16) unpack_into(infos_near, near[k]);
        }
    }

    std::vector<Colour> colours(static_cast<std::size_t>(t.colour_qwords) * 4);
    std::memcpy(colours.data(), data.data() + t.colours, std::min(data.size() - t.colours, colours.size() * sizeof(Colour)));
    std::vector<Normal> normals(t.vertex_count);
    std::memcpy(normals.data(), data.data() + t.normals + 0x10,
                std::min(data.size() - (t.normals + 0x10), normals.size() * sizeof(Normal)));

    std::vector<const Position16*> positions;
    for (const auto* list : {&pos_shared, &pos_mid, &pos_near}) {
        for (const Position16& p : *list) positions.push_back(&p);
    }
    std::vector<VertexInfo> infos = infos_shared;
    infos.insert(infos.end(), infos_mid.begin(), infos_mid.end());
    infos.insert(infos.end(), infos_near.begin(), infos_near.end());

    DecodedMesh mesh;
    mesh.name = "tfrag_" + std::to_string(number);
    mesh.vertices.reserve(infos.size());
    for (const VertexInfo& vi : infos) {
        const int r = vi.record / 2;
        if (r < 0 || r >= static_cast<int>(positions.size())) {
            mesh.vertices.push_back({});
            continue;
        }
        const Position16& p = *positions[static_cast<std::size_t>(r)];
        DecodedVertex v{};
        v.x = (base[0] + p.x) / 1024.0f;
        v.y = (base[1] + p.y) / 1024.0f;
        v.z = (base[2] + p.z) / 1024.0f;
        v.u = vi.s * (1.0f / 4096.0f);
        v.v = vi.t * (1.0f / 4096.0f);
        if (v.u < 0.0f) v.u *= 0.5f;
        if (v.v < 0.0f) v.v *= 0.5f;
        v.r = v.g = v.b = 128;
        v.a = 255;
        if (r < static_cast<int>(colours.size())) {
            const Colour& c = colours[static_cast<std::size_t>(r)];
            v.r = c.r;
            v.g = c.g;
            v.b = c.b;
            v.a = c.a < 0x80 ? static_cast<std::uint8_t>(c.a * 2) : 255;
        }
        v.nz = 1.0f;
        if (r < static_cast<int>(normals.size())) {  // azimuth / elevation in 1/128 of half a turn
            const Normal& n = normals[static_cast<std::size_t>(r)];
            const float az = n.azimuth * (3.1415926535f / 128.0f), el = n.elevation * (3.1415926535f / 128.0f);
            const float ca = std::cos(az), sa = std::sin(az), ce = std::cos(el), se = std::sin(el);
            v.nx = ca * ce;
            v.ny = sa * ce;
            v.nz = se;
        }
        mesh.vertices.push_back(v);
    }

    // Faces grouped by (level texture, CLAMP_1 as the level setup 0x241458 builds it from the
    // prim's pair): key = texture | clamp << 32; the maps' iteration order is the submeshes'.
    std::unordered_map<std::uint64_t, std::vector<std::array<int, 3>>> tris;
    std::unordered_map<std::uint64_t, std::vector<std::array<int, 4>>> quads;
    std::unordered_map<std::uint64_t, std::uint64_t> tex0_of;
    const int vertex_total = static_cast<int>(mesh.vertices.size());
    for (const StripFace& f : strip_faces(codes, index)) {
        std::int32_t texture = 0;
        std::uint64_t tex0 = 0, clamp = 0;
        if (f.prim >= 0 && f.prim < static_cast<int>(prims.size())) {
            const TexturePrim& p = prims[static_cast<std::size_t>(f.prim)];
            texture = p.tex0_lo;
            tex0 = static_cast<std::uint64_t>(p.tex0_hi) << 32 | static_cast<std::uint32_t>(p.tex0_lo);
            clamp = static_cast<std::uint64_t>(static_cast<std::uint32_t>(p.clamp_lo)) |
                    static_cast<std::uint64_t>(static_cast<std::uint32_t>(p.clamp_hi)) << 2;
        }
        const std::uint64_t key = static_cast<std::uint32_t>(texture) | clamp << 32;
        tex0_of[key] = tex0;
        const int corners = f.corner[3] >= 0 ? 4 : 3;
        bool inside = true;
        for (int k = 0; k < corners; ++k) inside = inside && f.corner[k] >= 0 && f.corner[k] < vertex_total;
        if (!inside) continue;
        if (corners == 4) quads[key].push_back({f.corner[0], f.corner[1], f.corner[2], f.corner[3]});
        else tris[key].push_back({f.corner[0], f.corner[1], f.corner[2]});
    }
    auto submesh = [&](std::uint64_t key) {
        DecodedSubMesh sm;
        sm.material_index = static_cast<int>(static_cast<std::uint32_t>(key));
        sm.clamp = key >> 32;
        sm.tex0 = tex0_of[key];
        return sm;
    };
    for (const auto& [key, list] : tris) {
        DecodedSubMesh sm = submesh(key);
        sm.triangles = list;
        if (quads.count(key)) sm.quads = quads[key];
        mesh.submeshes.push_back(std::move(sm));
    }
    for (const auto& [key, list] : quads) {
        if (tris.count(key)) continue;
        DecodedSubMesh sm = submesh(key);
        sm.quads = list;
        mesh.submeshes.push_back(std::move(sm));
    }
    mesh.bsphere = {t.sphere[0] / 1024.0f, t.sphere[1] / 1024.0f, t.sphere[2] / 1024.0f, t.sphere[3] / 1024.0f};
    return mesh;
}

std::vector<DecodedMesh> decode_tfrags(std::span<const std::byte> block) {
    std::vector<DecodedMesh> out;
    if (block.size() < 16) return out;
    const std::size_t table = static_cast<std::size_t>(get<std::int32_t>(block, 0));
    const std::int32_t count = get<std::int32_t>(block, 4);
    if (table + count * kTfragEntryBytes > block.size()) return out;
    for (std::int32_t i = 0; i < count; ++i) {
        const TfragEntry t = tfrag_entry(block.subspan(table + static_cast<std::size_t>(i) * kTfragEntryBytes, kTfragEntryBytes));
        const std::size_t at = table + t.data;
        if (at >= block.size()) continue;
        const std::span<const std::byte> data = block.subspan(at);
        if (t.colours > data.size() || static_cast<std::size_t>(t.normals) + 0x10 > data.size()) continue;
        out.push_back(decode_tfrag(data, t, static_cast<std::size_t>(i)));
    }
    return out;
}

// ---- tie classes (their most detailed LOD, without the class prims' GS state) ----

DecodedMesh decode_tie(std::span<const std::byte> cls, const ClassRecord& rec) {
    std::vector<DecodedVertex> vertices;
    std::unordered_map<int, std::vector<std::array<int, 3>>> by_texture;
    const float scale = get<float>(cls, tie::kScale);
    const std::size_t table = static_cast<std::size_t>(get<std::int32_t>(cls, tie::kLodTables));
    const std::size_t packets = get<std::uint8_t>(cls, tie::kLodPacketCounts);
    if (table + packets * 16 <= cls.size()) {
        const tie::WalkRules rules{.cap_at_strip_count = true, .jump_gaps = true};
        for (std::size_t p = 0; p < packets; ++p) {
            for (const tie::Strip& strip : tie::packet_strips(cls, table, table + p * 16, rules)) {
                int texture = 0;
                if (strip.texture_slot >= 0 && strip.texture_slot < 16) {
                    texture = rec.texture_remap[static_cast<std::size_t>(strip.texture_slot)];
                    if (texture == 0xff) texture = 0;
                }
                const int first = static_cast<int>(vertices.size());
                for (std::size_t k = 0; k < strip.corners.size(); ++k) {
                    const tie::Corner& c = strip.corners[k];
                    DecodedVertex v{};
                    v.x = c.x * (scale / 1024.0f);
                    v.y = c.y * (scale / 1024.0f);
                    v.z = c.z * (scale / 1024.0f);
                    v.u = c.s * (1.0f / 4096.0f);
                    v.v = c.t * (1.0f / 4096.0f);
                    v.nz = 1.0f;
                    v.r = v.g = v.b = v.a = 255;
                    vertices.push_back(v);
                    if (k < 2) continue;
                    const int n = first + static_cast<int>(k);
                    if (static_cast<int>(k % 2) == (strip.odd_winding ? 1 : 0)) by_texture[texture].push_back({n - 2, n - 1, n});
                    else by_texture[texture].push_back({n, n - 1, n - 2});
                }
            }
        }
    }
    DecodedMesh mesh;
    mesh.name = "tie_class_" + std::to_string(rec.class_id);
    mesh.vertices = std::move(vertices);
    for (const auto& [texture, list] : by_texture) {
        DecodedSubMesh sm;
        sm.material_index = texture;
        sm.triangles = list;
        mesh.submeshes.push_back(std::move(sm));
    }
    return mesh;
}

} // namespace

LevelStaticGeometry decode_level_meshes(std::span<const std::byte> level_wad_data) {
    const std::span<const std::byte> file = level_file_body(level_wad_data);
    if (file.size() < sizeof(LevelFileHeader)) {
        std::cerr << "Error: level WAD data too small to contain header." << std::endl;
        return {};
    }
    LevelFileHeader fh;
    std::memcpy(&fh, file.data(), sizeof fh);
    auto fits = [&file](FileSpan s) { return s.offset >= 0 && s.size >= 0 && static_cast<std::size_t>(s.offset) + static_cast<std::size_t>(s.size) <= file.size(); };
    if (!fits(fh.asset_index) || !fits(fh.asset_data)) {
        std::cerr << "Error: the level WAD's index or data is outside the file." << std::endl;
        return {};
    }
    const std::span<const std::byte> index(file.data() + fh.asset_index.offset, static_cast<std::size_t>(fh.asset_index.size));
    std::vector<std::uint8_t> unpacked;
    if (!decompress_wad(std::span<const std::byte>(file.data() + fh.asset_data.offset, static_cast<std::size_t>(fh.asset_data.size)),
                        unpacked)) {
        std::cerr << "Error: failed to decompress the asset data." << std::endl;
        return {};
    }
    if (index.size() < sizeof(AssetIndexHeader)) {
        std::cerr << "Error: asset index too small for its header." << std::endl;
        return {};
    }
    AssetIndexHeader ih;
    std::memcpy(&ih, index.data(), sizeof ih);
    const std::span<const std::byte> data(reinterpret_cast<const std::byte*>(unpacked.data()), unpacked.size());

    // A block's size: up to the next block the index names (the classes' data included), or the
    // end of the data.
    std::vector<std::int64_t> starts = {ih.tfrag_block, ih.occlusion_block, ih.sky_block, ih.collision_block,
                                        ih.texel_block, ih.unpacked_size};
    auto add_class_starts = [&](TableRef ref, std::size_t stride) {
        if (ref.count <= 0 || ref.offset < 0) return;
        for (int i = 0; i < ref.count; ++i) {
            const std::size_t row = static_cast<std::size_t>(ref.offset) + i * stride;
            if (row + 4 > index.size()) continue;
            const std::int32_t at = get<std::int32_t>(index, row);
            if (at > 0) starts.push_back(at);
        }
    };
    add_class_starts(ih.moby_classes, sizeof(ClassRecord));
    add_class_starts(ih.tie_classes, sizeof(ClassRecord));
    add_class_starts(ih.shrub_classes, sizeof(ShrubClassRecord));
    std::sort(starts.begin(), starts.end());
    starts.erase(std::unique(starts.begin(), starts.end()), starts.end());
    auto block_size = [&](std::int32_t at) -> std::size_t {
        if (at < 0) return 0;
        const auto next = std::upper_bound(starts.begin(), starts.end(), at);
        return next != starts.end() ? static_cast<std::size_t>(*next - at) : data.size() - at;
    };

    LevelStaticGeometry out;
    if (ih.tfrag_block >= 0) {
        const std::size_t n = block_size(ih.tfrag_block);
        if (static_cast<std::size_t>(ih.tfrag_block) + n <= data.size()) out.tfrags = decode_tfrags(data.subspan(ih.tfrag_block, n));
    }

    std::vector<ClassRecord> ties;
    if (ih.tie_classes.count > 0 && ih.tie_classes.offset >= 0) {
        const std::size_t first = static_cast<std::size_t>(ih.tie_classes.offset);
        const std::size_t bytes = static_cast<std::size_t>(ih.tie_classes.count) * sizeof(ClassRecord);
        if (first + bytes > index.size()) {
            std::cerr << "Warning: tie class table out of bounds: offset=" << ih.tie_classes.offset
                      << ", count=" << ih.tie_classes.count << std::endl;
        } else {
            ties.resize(static_cast<std::size_t>(ih.tie_classes.count));
            std::memcpy(ties.data(), index.data() + first, bytes);
        }
    }
    for (const ClassRecord& rec : ties) {
        if (rec.data == 0) continue;
        const std::size_t n = block_size(rec.data);
        if (rec.data + n > data.size()) continue;
        const std::span<const std::byte> cls = data.subspan(static_cast<std::size_t>(rec.data), n);
        if (cls.size() < tie::kHeaderBytes) continue;
        out.tie_classes.push_back(decode_tie(cls, rec));
    }
    return out;
}

} // namespace rac1
