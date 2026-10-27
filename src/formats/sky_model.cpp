#include <formats/sky_model.h>
#include "io/host_endian.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <unordered_map>

namespace rac1 {

namespace {

template <class T>
T get(std::span<const std::byte> b, std::size_t at) {  // (the file's numbers are little-endian)
    return le_load<T>(b.data() + at);
}

// The sky file's head: byte offsets of the fields this decoder reads.
constexpr std::size_t kHeadBytes = 0x40;
constexpr std::size_t kClearColour = 0x00;   // u8[4] RGBA
constexpr std::size_t kClearScreen = 0x04;   // s16
constexpr std::size_t kShellCount = 0x06;    // s16, at most 8
constexpr std::size_t kTextureCount = 0x0c;  // s16
constexpr std::size_t kTextures = 0x10;      // s32: 16-byte texture records (palette, texels, width, height)
constexpr std::size_t kTextureData = 0x14;   // s32: the block the records' offsets start from
constexpr std::size_t kShells = 0x20;        // s32[8]

// A shell: (cluster count, flags), 8 more bytes, then 32-byte cluster records.
constexpr std::size_t kClusterBytes = 32;

struct TextureRef {
    std::int32_t palette, texels, width, height;
    bool operator==(const TextureRef&) const = default;
};

// The 8-bit texture at `t` as RGBA (palette indices in the GS CLUT order: bits 3 and 4 swapped;
// GS alpha 0..0x80 widened to 0..255).
std::vector<std::uint8_t> sky_rgba(std::span<const std::byte> sky, std::size_t texels, std::size_t count, std::size_t palette) {
    std::array<std::uint32_t, 256> clut;
    std::memcpy(clut.data(), sky.data() + palette, 1024);
    std::vector<std::uint8_t> rgba(count * 4);
    for (std::size_t i = 0; i < count; ++i) {
        std::uint8_t k = static_cast<std::uint8_t>(sky[texels + i]);
        if (((k >> 3) & 1) != ((k >> 4) & 1)) k ^= 0x18;
        const std::uint32_t c = le32(&clut[k]);
        const std::uint32_t a = c >> 24;
        rgba[i * 4 + 0] = static_cast<std::uint8_t>(c);
        rgba[i * 4 + 1] = static_cast<std::uint8_t>(c >> 8);
        rgba[i * 4 + 2] = static_cast<std::uint8_t>(c >> 16);
        rgba[i * 4 + 3] = static_cast<std::uint8_t>(a < 0x80 ? a * 2 : 255);
    }
    return rgba;
}

} // namespace

DecodedSky decode_level_sky(std::span<const std::byte> sky_data, int level_id) {
    const std::span<const std::byte> sky = sky_data;
    DecodedSky out;
    if (sky.size() < kHeadBytes) {
        std::cerr << "Error: Sky WAD data too small for its header." << std::endl;
        return out;
    }
    const std::int16_t shell_count = get<std::int16_t>(sky, kShellCount);
    if (shell_count < 0 || shell_count > 8) {
        std::cerr << "Warning: Invalid sky shell count: " << shell_count << std::endl;
        return out;
    }
    out.color_r = get<std::uint8_t>(sky, kClearColour);
    out.color_g = get<std::uint8_t>(sky, kClearColour + 1);
    out.color_b = get<std::uint8_t>(sky, kClearColour + 2);
    out.color_a = get<std::uint8_t>(sky, kClearColour + 3);
    out.clear_screen = get<std::int16_t>(sky, kClearScreen) != 0;

    // Local texture -> decoded texture (records naming the same texels and palette share one;
    // -1 when the texture does not fit in the file).
    std::vector<int> local_to_decoded;
    const std::int16_t texture_count = get<std::int16_t>(sky, kTextureCount);
    const std::int32_t records = get<std::int32_t>(sky, kTextures);
    if (texture_count > 0 && records > 0) {
        if (static_cast<std::size_t>(records) + static_cast<std::size_t>(texture_count) * 16 <= sky.size()) {
            const std::size_t block = static_cast<std::size_t>(get<std::int32_t>(sky, kTextureData));
            std::vector<TextureRef> refs;
            for (int i = 0; i < texture_count; ++i) {
                const std::size_t r = static_cast<std::size_t>(records) + static_cast<std::size_t>(i) * 16;
                const TextureRef t{get<std::int32_t>(sky, r), get<std::int32_t>(sky, r + 4), get<std::int32_t>(sky, r + 8),
                                   get<std::int32_t>(sky, r + 12)};
                const std::size_t seen = static_cast<std::size_t>(std::find(refs.begin(), refs.end(), t) - refs.begin());
                int decoded = seen < refs.size() ? local_to_decoded[seen] : -1;
                if (seen == refs.size()) {  // not seen before
                    const std::size_t texels = block + t.texels, count = static_cast<std::size_t>(t.width) * t.height;
                    const std::size_t palette = block + t.palette;
                    if (texels + count <= sky.size() && palette + 1024 <= sky.size()) {
                        decoded = static_cast<int>(out.textures.size());
                        DecodedTexture tex;
                        tex.name = "sky_" + std::to_string(level_id) + "_" + std::to_string(decoded);
                        tex.category = "sky";
                        tex.index = decoded;
                        tex.width = t.width;
                        tex.height = t.height;
                        tex.rgba_data = sky_rgba(sky, texels, count, palette);
                        out.textures.push_back(std::move(tex));
                    }
                }
                refs.push_back(t);
                local_to_decoded.push_back(decoded);
            }
        } else {
            std::cerr << "Warning: Sky texture records out of bounds." << std::endl;
        }
    }

    for (int s = 0; s < shell_count; ++s) {
        const std::int32_t at = get<std::int32_t>(sky, kShells + 4 * static_cast<std::size_t>(s));
        if (at <= 0 || static_cast<std::size_t>(at) + 8 > sky.size()) continue;
        const std::int32_t clusters = get<std::int32_t>(sky, static_cast<std::size_t>(at));
        DecodedSkyShell shell;
        shell.textured = (get<std::int32_t>(sky, static_cast<std::size_t>(at) + 4) & 1) == 0;
        shell.mesh.name = "sky_shell_" + std::to_string(s);

        // Triangles by decoded texture (-1: untextured); the map's order is the submeshes'.
        std::unordered_map<int, std::vector<std::array<int, 3>>> by_texture;
        for (int c = 0; c < clusters; ++c) {
            const std::size_t rec = at + 0x10 + c * kClusterBytes;
            if (rec + kClusterBytes > sky.size()) break;
            // The cluster: sphere (+0), its data (+0x10), vertex / triangle counts (+0x14 / +0x16),
            // the positions, texture coordinates and triangles from its data (+0x18 / +0x1a / +0x1c).
            const std::size_t data = static_cast<std::size_t>(get<std::int32_t>(sky, rec + 0x10));
            const std::int16_t vertices = get<std::int16_t>(sky, rec + 0x14), triangles = get<std::int16_t>(sky, rec + 0x16);
            const std::size_t pos = data + get<std::int16_t>(sky, rec + 0x18);
            const std::size_t uv = data + get<std::int16_t>(sky, rec + 0x1a);
            const std::size_t tri = data + get<std::int16_t>(sky, rec + 0x1c);
            if (pos + vertices * std::size_t{8} > sky.size() || uv + vertices * std::size_t{4} > sky.size() ||
                tri + triangles * std::size_t{4} > sky.size()) {
                continue;
            }
            const int first = static_cast<int>(shell.mesh.vertices.size());
            for (int v = 0; v < vertices; ++v) {
                const std::size_t p = pos + static_cast<std::size_t>(v) * 8, q = uv + static_cast<std::size_t>(v) * 4;
                DecodedVertex d{};
                d.x = get<std::int16_t>(sky, p) * (1.f / 1024.f);
                d.y = get<std::int16_t>(sky, p + 2) * (1.f / 1024.f);
                d.z = get<std::int16_t>(sky, p + 4) * (1.f / 1024.f);
                d.u = static_cast<float>(get<std::int16_t>(sky, q)) / 4096.f;
                d.v = static_cast<float>(get<std::int16_t>(sky, q + 2)) / 4096.f;
                d.r = shell.textured ? 255 : out.color_r;
                d.g = shell.textured ? 255 : out.color_g;
                d.b = shell.textured ? 255 : out.color_b;
                const std::int16_t alpha = get<std::int16_t>(sky, p + 6);
                d.a = alpha == 0x80 ? 255 : static_cast<std::uint8_t>(alpha * 2);
                shell.mesh.vertices.push_back(d);
            }
            for (int f = 0; f < triangles; ++f) {
                const std::size_t r = tri + static_cast<std::size_t>(f) * 4;  // three corners, the local texture
                const std::uint8_t local = get<std::uint8_t>(sky, r + 3);
                const int texture = local != 0xff && local < local_to_decoded.size() ? local_to_decoded[local] : -1;
                by_texture[texture].push_back({first + get<std::uint8_t>(sky, r + 2), first + get<std::uint8_t>(sky, r + 1),
                                               first + get<std::uint8_t>(sky, r)});
            }
        }
        for (auto& [texture, list] : by_texture) {
            DecodedSubMesh sm;
            sm.material_index = texture;
            sm.triangles = std::move(list);
            shell.mesh.submeshes.push_back(std::move(sm));
        }
        out.shells.push_back(std::move(shell));
    }

    // The whole sky moved so that its bounding box is centred on the origin.
    float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
    bool any = false;
    for (const DecodedSkyShell& shell : out.shells) {
        for (const DecodedVertex& v : shell.mesh.vertices) {
            const float p[3] = {v.x, v.y, v.z};
            for (int k = 0; k < 3; ++k) {
                lo[k] = std::min(lo[k], p[k]);
                hi[k] = std::max(hi[k], p[k]);
            }
            any = true;
        }
    }
    if (any) {
        const float c[3] = {(lo[0] + hi[0]) * 0.5f, (lo[1] + hi[1]) * 0.5f, (lo[2] + hi[2]) * 0.5f};
        for (DecodedSkyShell& shell : out.shells) {
            for (DecodedVertex& v : shell.mesh.vertices) {
                v.x -= c[0];
                v.y -= c[1];
                v.z -= c[2];
            }
        }
    }
    return out;
}

} // namespace rac1
