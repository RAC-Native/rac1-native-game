// The MRP world assets' derived images and loading: see mrp_world_assets.h. Host code.
#include <game/render/mrp_world_assets.h>

#include <runtime/vu/vu1.h>

#include "io/host_endian.h"

#include <algorithm>
#include <cstring>

namespace rac1::mrpw {

namespace {

// The asset's blobs (ShrubPacket::vif, TiePacket::data, TfragEntry::data) hold the PS2's bytes (little-endian, as the file and
// rb() of EE memory give them); vif1_command reads its data as EE memory holds it: host-order 32-bit words. On a big-endian
// host a copy of the data has each word swapped first (on a little-endian one the two are the same bytes).
void ps2_words_to_host(uint8_t* p, size_t n) {
    if constexpr (k_host_big_endian)
        for (size_t i = 0; i + 4 <= n; i += 4) {
            const uint8_t t0 = p[i], t1 = p[i + 1];
            p[i] = p[i + 3];
            p[i + 1] = p[i + 2];
            p[i + 2] = t1;
            p[i + 3] = t0;
        }
}

// One VIF code and its data into the image (the data zero-padded to the code's size).
void vif_code(vu1::Vu1& vu, uint32_t code, const uint8_t* data, size_t avail) {
    const uint32_t nb = vu1::vif_data_bytes(vu, code);
    std::vector<uint8_t> buf(nb, 0);
    if (data != nullptr) std::memcpy(buf.data(), data, std::min<size_t>(nb, avail));
    ps2_words_to_host(buf.data(), buf.size());
    vu1::vif1_command(vu, code, buf.data(), nullptr);
}

// A VIF list (codes and their data) into the image; program starts in it are not run.
void vif_list(vu1::Vu1& vu, const uint8_t* p, size_t n) {
    std::vector<uint8_t> buf;  // (big-endian host: a code's data with its words swapped)
    for (size_t i = 0; i + 4 <= n;) {
        const uint32_t code = le32(p + i);
        i += 4;
        const uint32_t nb = vu1::vif_data_bytes(vu, code);
        const uint32_t cmd = (code >> 24) & 0x7f;
        if (cmd == 0x14 || cmd == 0x15 || cmd == 0x17) {
            i += nb;
            continue;
        }
        if (i + nb > n) break;
        if constexpr (k_host_big_endian) {
            buf.assign(p + i, p + i + nb);
            ps2_words_to_host(buf.data(), buf.size());
            vu1::vif1_command(vu, code & 0x7fffffffu, buf.data(), nullptr);
        } else {
            vu1::vif1_command(vu, code & 0x7fffffffu, p + i, nullptr);
        }
        i += nb;
    }
}

uint32_t rd32(const std::vector<uint8_t>& b, size_t at) {
    uint32_t v = 0;
    if (at + 4 <= b.size()) v = le32(b.data() + at);
    return v;
}

}  // namespace

// orig: (host)
std::vector<uint8_t> shrub_image(const ShrubPacket& p) {
    vu1::Vu1 vu;
    vif_list(vu, p.vif.data(), p.vif.size());
    uint32_t top = 0;
    for (uint32_t q = 0; q < 0x400; ++q)
        for (int b = 0; b < 16; ++b)
            if (vu.st.mem[16 * q + static_cast<uint32_t>(b)] != 0) top = q + 1;
    return std::vector<uint8_t>(vu.st.mem.begin(), vu.st.mem.begin() + static_cast<long>(top) * 16);
}

// orig: (host)
std::vector<uint8_t> tie_image(const TieClass& c, const TiePacket& p) {
    const uint32_t nprim = p.entry[4], csize = p.entry[7], vofs = p.entry[8], vsize = p.entry[9], iofs = p.entry[0xa], icount = p.entry[0xb];
    const uint32_t iq = (icount * 4 + 15) / 16;
    const uint8_t* d = p.data.data();
    const size_t n = p.data.size();
    auto avail = [n](size_t at) { return at < n ? n - at : 0; };
    vu1::Vu1 vu;
    vif_code(vu, 0x6c010000u, d, avail(0));
    for (uint32_t k = 0; k < nprim && k < p.prims.size(); ++k) {
        uint8_t prim[0x50] = {};
        const int32_t t = p.prims[k];
        if (t >= 0 && static_cast<size_t>(t) < c.textures.size())
            for (uint32_t r = 0; r < 5; ++r)  // (as the PS2's bytes, as vif_code takes its data)
                for (uint32_t h = 0; h < 2; ++h) le_store(prim + 0x10 * r + 8 * h, c.textures[static_cast<size_t>(t)].ad[r][h]);
        vif_code(vu, 0x6c050000u | (1 + 5 * k), prim, sizeof prim);
    }
    vif_code(vu, 0x6e004000u | ((4 * csize) << 16) | (1 + 5 * nprim), d + std::min<size_t>(0x20, n), avail(0x20));
    vif_code(vu, 0x6d000000u | ((2 * vsize & 0xff) << 16) | 0x32, d + std::min<size_t>(16 * vofs, n), avail(16 * vofs));
    vif_code(vu, 0x6e004000u | (icount << 16) | 0xcc, d + std::min<size_t>(16 * iofs, n), avail(16 * iofs));
    vif_code(vu, 0x6e004000u | (icount << 16) | 0xf8, d + std::min<size_t>(16 * (iofs + iq), n), avail(16 * (iofs + iq)));
    return std::vector<uint8_t>(vu.st.mem.begin(), vu.st.mem.begin() + 0x124 * 16);
}

// orig: (host)
void tfrag_image(const TfragEntry& t, int lod, TfragImage& im) {
    auto run = [&](vu1::Vu1& vu) {
        for (const auto& r : im.refs) {
            if (r[1] == 0) continue;
            vu.cl = vu.wl = 4;
            const size_t off = 16u * r[0], len = 16u * r[1];
            if (off + len > t.data.size()) continue;
            vif_list(vu, t.data.data() + off, len);
        }
    };
    (void)lod;
    vu1::Vu1 a, b;
    std::fill(b.st.mem.begin(), b.st.mem.end(), static_cast<uint8_t>(0xa5));
    run(a);
    run(b);
    uint32_t top = 0;
    im.written.assign(0x148, 0);
    for (uint32_t q = 0; q < 0x148; ++q) {
        bool marker = true;
        for (int k = 0; k < 16; ++k) marker = marker && b.st.mem[16 * q + static_cast<uint32_t>(k)] == 0xa5;
        im.written[q] = marker ? 0 : 1;
        if (!marker) top = q + 1;
    }
    im.written.resize(top);
    im.image.assign(a.st.mem.begin(), a.st.mem.begin() + static_cast<long>(top) * 16);
}

// orig: (host)
void derive_images(World& w) {
    for (auto& c : w.shrubs)
        for (auto& p : c.packets) p.image = shrub_image(p);
    for (auto& c : w.ties)
        for (auto& p : c.packets) p.image = tie_image(c, p);
    for (auto& t : w.tfrags)
        for (int l = 0; l < 3; ++l) tfrag_image(t, l, t.lods[static_cast<size_t>(l)]);
    (void)rd32;
}

// orig: (host) see mrp_world_assets.h
std::string level_dir_name(int level) {
    if (level == kTitleLevel) return "title";
    if (level == kFlightLevel) return "flight";
    return "level" + std::to_string(level);
}

// orig: (host)

bool load_world(const std::string& dir, int level, World& w) {
    if (!read_world(dir + "/" + level_dir_name(level) + "/world.mrpw", w)) return false;
    derive_images(w);
    return true;
}

}  // namespace rac1::mrpw
