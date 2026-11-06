// The MRP world assets' derived images and loading: see mrp_world_assets.h. Host code.
#include <game/render/mrp_world_assets.h>

#include <runtime/vu/vu1.h>

#include <algorithm>
#include <cstring>

namespace rac1::mrpw {

namespace {

// One VIF code and its data into the image (the data zero-padded to the code's size).
void vif_code(vu1::Vu1& vu, uint32_t code, const uint8_t* data, size_t avail) {
    const uint32_t nb = vu1::vif_data_bytes(vu, code);
    std::vector<uint8_t> buf(nb, 0);
    if (data != nullptr) std::memcpy(buf.data(), data, std::min<size_t>(nb, avail));
    vu1::vif1_command(vu, code, buf.data(), nullptr);
}

// A VIF list (codes and their data) into the image; program starts in it are not run.
void vif_list(vu1::Vu1& vu, const uint8_t* p, size_t n) {
    for (size_t i = 0; i + 4 <= n;) {
        uint32_t code;
        std::memcpy(&code, p + i, 4);
        i += 4;
        const uint32_t nb = vu1::vif_data_bytes(vu, code);
        const uint32_t cmd = (code >> 24) & 0x7f;
        if (cmd == 0x14 || cmd == 0x15 || cmd == 0x17) {
            i += nb;
            continue;
        }
        if (i + nb > n) break;
        vu1::vif1_command(vu, code & 0x7fffffffu, p + i, nullptr);
        i += nb;
    }
}

uint32_t rd32(const std::vector<uint8_t>& b, size_t at) {
    uint32_t v = 0;
    if (at + 4 <= b.size()) std::memcpy(&v, b.data() + at, 4);
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
            for (uint32_t r = 0; r < 5; ++r) std::memcpy(prim + 0x10 * r, c.textures[static_cast<size_t>(t)].ad[r].data(), 16);
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

// orig: 0x00240aa8 (tie_class_register's texture loop, level_setup.cpp) - a tie class's texture prim t takes the level's tie
// texture remap[t] (its 16-byte palette, 0x1c7700 + index * 16): the prim's own TEX0 word is 0 in every class of the game, so
// the decoder's "local texture" (that word) named remap[0] for all of a class's prims (#136: the Textures tab listed a tie
// class's first texture alone, the other textures had no class and could not be listed or aimed at, and a line for a tie's
// "tex N" marked its first texture). The world files (setup's decoder, world_trace.cpp) keep that word: each prim's level
// texture is set here from its index at the load (the class's 16-byte remap: at most 16 prims; the draws read the game's own
// registers and do not change)
static void tie_textures_from_remap(World& w) {
    for (TieClass& c : w.ties)
        for (size_t t = 0; t < c.textures.size(); ++t) {
            c.textures[t].local_texture = static_cast<int32_t>(t);
            c.textures[t].texture = t < c.texture_remap.size() ? c.texture_remap[t] : -1;
        }
}

// orig: (host)

bool load_world(const std::string& dir, int level, World& w) {
    if (!read_world(dir + "/" + level_dir_name(level) + "/world.mrpw", w)) return false;
    tie_textures_from_remap(w);
    derive_images(w);
    return true;
}

}  // namespace rac1::mrpw
