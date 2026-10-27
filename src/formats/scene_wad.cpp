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

} // namespace rac1
