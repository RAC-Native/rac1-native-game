#include "io/host_endian.h"
#include <formats/scene_wad.h>
#include <array>
#include <io/byte_reader.h>
#include <map>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>

namespace rac1 {

namespace {

std::string clean_string(const char* data, std::size_t max_len) {
    std::string s;
    for (std::size_t i = 0; i < max_len; ++i) {
        if (data[i] == '\0') break;
        if (std::isprint(static_cast<unsigned char>(data[i]))) {
            s.push_back(data[i]);
        }
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

} // namespace

// The "WAD" decompressor, written after the game's own routine (wad_decompress, 0x24d8d0, ported in
// src/game/ovl/level_setup.cpp) ("WAD compression"). The game
// streams the input through the scratchpad in 0x2000-byte pieces; here the whole input is in
// memory, so a piece end only moves the read position to the next piece.
namespace {

class LzInput {
public:
    LzInput(const std::uint8_t* first, const std::uint8_t* last) : first_(first), at_(first), last_(last) {}
    bool take(std::uint32_t& v) {
        if (at_ >= last_) return false;
        v = *at_++;
        return true;
    }
    bool take(std::size_t n, std::vector<std::uint8_t>& out) {
        if (static_cast<std::size_t>(last_ - at_) < n) return false;
        out.insert(out.end(), at_, at_ + n);
        at_ += n;
        return true;
    }
    std::uint32_t back(std::size_t n) const { return at_[-static_cast<std::ptrdiff_t>(n)]; }
    void next_piece() {
        const std::size_t used = static_cast<std::size_t>(at_ - first_);
        const std::size_t piece = 0x2000;
        const std::size_t next = (used + piece - 1) / piece * piece;
        at_ = next < static_cast<std::size_t>(last_ - first_) ? first_ + next : last_;
    }

private:
    const std::uint8_t* first_;
    const std::uint8_t* at_;
    const std::uint8_t* last_;
};

}  // namespace

bool decompress_wad(std::span<const std::byte> src, std::vector<std::uint8_t>& dest) {
    if (src.size() < 16) return false;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(src.data());
    if (bytes[0] != 'W' || bytes[1] != 'A' || bytes[2] != 'D') return false;
    const std::uint32_t total = le32(bytes + 3);
    if (total > src.size()) return false;

    LzInput in(bytes + 16, bytes + total);
    bool after_literals = false;  // a literal run must be followed by a copy
    std::uint32_t code;
    // The stream may open with a literal run of (first byte - 0x11) bytes.
    if (total > 16 && bytes[16] > 0x11) {
        in.take(code);
        if (!in.take(code - 0x11, dest)) return false;
        after_literals = true;
    }
    while (in.take(code)) {
        if (code < 0x10) {  // a literal run of code + 3 bytes (0: the next byte + 18)
            if (after_literals) return false;
            std::uint32_t n = code + 3;
            if (code == 0) {
                if (!in.take(n)) return false;
                n += 18;
            }
            if (!in.take(n, dest)) return false;
            after_literals = true;
            continue;
        }
        after_literals = false;

        // A copy of `length` earlier output bytes from `distance` back.
        std::uint32_t length, distance, a, b;
        if (code >= 0x40) {  // short: length 2..8, distance 1..2048
            if (!in.take(a)) return false;
            length = (code >> 5) + 1;
            distance = 1 + ((code >> 2) & 7) + a * 8;
        } else if (code >= 0x20) {  // medium: distance 1..16384
            length = code & 0x1f;
            if (length == 0) {
                if (!in.take(length)) return false;
                length += 0x1f;
            }
            length += 2;
            if (!in.take(a) || !in.take(b)) return false;
            distance = 1 + (a >> 2) + b * 0x40;
        } else {  // far (0x10..0x1f): distance 16384 + (0..0x7fff); distance 0 is a marker
            length = code & 7;
            if (length == 0) {
                if (!in.take(length)) return false;
                length += 7;
            }
            if (!in.take(a) || !in.take(b)) return false;
            distance = (code & 8) * 0x800 + b * 0x40 + (a >> 2);
            if (distance == 0) {
                if (length != 1) {  // the end of a piece: the next one follows
                    in.next_piece();
                    continue;
                }
                length = 0;  // no copy, only the trailing literals
            } else {
                distance += 0x4000;
                length += 2;
            }
        }
        if (length != 0) {
            if (distance > dest.size()) return false;
            const std::size_t from = dest.size() - distance;
            for (std::uint32_t i = 0; i < length; ++i) {
                const std::uint8_t v = dest[from + i];
                dest.push_back(v);
            }
        }
        // 0..3 literal bytes follow a copy: the low two bits of the copy's second-to-last byte.
        if (!in.take(in.back(2) & 3, dest)) return false;
    }
    return true;
}

std::vector<SubAsset> parse_scene_wad(std::span<const std::byte> data) {
    if (data.size() < 8) {
        throw std::runtime_error("scene.wad is too small to contain a header");
    }
    
    ByteReader reader(data);
    std::int32_t header_size = reader.s32_le();
    [[maybe_unused]] std::int32_t pad_4 = reader.s32_le();
    
    if (header_size != 0x22b8) {
        throw std::runtime_error("Invalid scene.wad header size: " + std::to_string(header_size) + " (expected 0x22b8)");
    }
    
    std::array<SceneHeader, 30> scenes;
    for (int i = 0; i < 30; ++i) {
        for (int j = 0; j < 6; ++j) {
            scenes[i].sounds[j] = reader.s32_le();
        }
        for (int j = 0; j < 68; ++j) {
            scenes[i].wads[j] = reader.s32_le();
        }
    }
    
    struct TempAsset {
        std::int32_t sector_offset;
        std::vector<SubAsset::Reference> references;
    };
    std::map<std::int32_t, TempAsset> unique_temp_assets;
    
    for (int i = 0; i < 30; ++i) {
        for (int j = 0; j < 6; ++j) {
            std::int32_t off = scenes[i].sounds[j];
            if (off > 0) {
                unique_temp_assets[off].sector_offset = off;
                unique_temp_assets[off].references.push_back({i, j, SubAssetType::Sound});
            }
        }
        for (int j = 0; j < 68; ++j) {
            std::int32_t off = scenes[i].wads[j];
            if (off > 0) {
                unique_temp_assets[off].sector_offset = off;
                unique_temp_assets[off].references.push_back({i, j, SubAssetType::Wad});
            }
        }
    }
    
    std::vector<TempAsset> sorted_temp_assets;
    for (auto const& [off, asset] : unique_temp_assets) {
        sorted_temp_assets.push_back(asset);
    }
    
    std::vector<SubAsset> sub_assets;
    sub_assets.reserve(sorted_temp_assets.size());
    
    for (std::size_t k = 0; k < sorted_temp_assets.size(); ++k) {
        auto const& temp = sorted_temp_assets[k];
        std::int32_t sector_offset = temp.sector_offset;
        std::size_t byte_offset = static_cast<std::size_t>(sector_offset) * 2048;
        
        std::size_t default_size_bytes = 0;
        if (k + 1 < sorted_temp_assets.size()) {
            default_size_bytes = static_cast<std::size_t>(sorted_temp_assets[k + 1].sector_offset - sector_offset) * 2048;
        } else {
            if (data.size() > byte_offset) {
                default_size_bytes = data.size() - byte_offset;
            } else {
                default_size_bytes = 0;
            }
        }
        
        SubAsset asset;
        asset.index = static_cast<int>(k);
        asset.sector_offset = sector_offset;
        asset.byte_offset = byte_offset;
        asset.references = temp.references;
        asset.is_compressed = false;
        asset.type = SubAssetType::Unknown;
        asset.size_bytes = default_size_bytes;
        asset.decompressed_size = default_size_bytes;
        
        SubAssetType primary_ref_type = SubAssetType::Unknown;
        for (auto const& ref : temp.references) {
            if (ref.ref_type == SubAssetType::Sound) {
                primary_ref_type = SubAssetType::Sound;
                break;
            } else if (ref.ref_type == SubAssetType::Wad) {
                primary_ref_type = SubAssetType::Wad;
            }
        }
        
        if (byte_offset + 16 <= data.size()) {
            const char* magic_ptr = reinterpret_cast<const char*>(data.data() + byte_offset);
            if (magic_ptr[0] == 'W' && magic_ptr[1] == 'A' && magic_ptr[2] == 'D') {
                asset.type = SubAssetType::Wad;
                asset.is_compressed = true;
                std::uint32_t comp_size = le32(data.data() + byte_offset + 3);
                if (comp_size > 16 && byte_offset + comp_size <= data.size()) {
                    asset.size_bytes = comp_size;
                }
                asset.tag = clean_string(magic_ptr + 7, 9);
                
                std::vector<std::uint8_t> decompressed_data;
                if (decompress_wad(data.subspan(byte_offset), decompressed_data)) {
                    asset.decompressed_size = decompressed_data.size();
                } else {
                    asset.decompressed_size = 0;
                }
            } else if (magic_ptr[0] == 'V' && magic_ptr[1] == 'A' && magic_ptr[2] == 'G' && magic_ptr[3] == 'p') {
                asset.type = SubAssetType::Sound;
                std::uint32_t big_endian_data_size = (static_cast<std::uint8_t>(data[byte_offset + 0x0c]) << 24) |
                                                     (static_cast<std::uint8_t>(data[byte_offset + 0x0d]) << 16) |
                                                     (static_cast<std::uint8_t>(data[byte_offset + 0x0e]) << 8) |
                                                     static_cast<std::uint8_t>(data[byte_offset + 0x0f]);
                std::size_t vag_size = 0x30 + big_endian_data_size;
                if (byte_offset + vag_size <= data.size()) {
                    asset.size_bytes = vag_size;
                }
                asset.decompressed_size = asset.size_bytes;
                asset.tag = clean_string(magic_ptr + 0x20, 16);
            } else {
                if (primary_ref_type != SubAssetType::Unknown) {
                    asset.type = primary_ref_type;
                }
            }
        } else {
            if (primary_ref_type != SubAssetType::Unknown) {
                asset.type = primary_ref_type;
            }
        }
        
        sub_assets.push_back(asset);
    }
    
    return sub_assets;
}

} // namespace rac1
