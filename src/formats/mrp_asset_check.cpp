// (testing) the MRP asset files read and hashed by value: see mrp_asset_check.h.
#include <formats/mrp_asset_check.h>

#include <formats/moby_mrp.h>
#include <formats/world_mrp.h>

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <type_traits>
#include <vector>

namespace rac1::mrp {

namespace {

// FNV-1a 64 over the values: a number fed as its little-endian bytes (by shifts: the host's order never enters), a float
// as its bits, an array / vector element by element (a vector's size first), a byte blob as its bytes
struct Hash {
    uint64_t h = 0xcbf29ce484222325ull;
    void byte(uint8_t b) {
        h ^= b;
        h *= 0x100000001b3ull;
    }
    template <typename T> void num(T v) {
        if constexpr (std::is_enum_v<T>) {
            num(static_cast<std::underlying_type_t<T>>(v));
        } else if constexpr (std::is_same_v<T, bool>) {
            byte(v ? 1 : 0);
        } else {
            static_assert(std::is_arithmetic_v<T>);
            std::make_unsigned_t<std::conditional_t<std::is_floating_point_v<T>, std::conditional_t<sizeof(T) == 4, int32_t, int64_t>, T>> u;
            static_assert(sizeof u == sizeof v);
            std::memcpy(&u, &v, sizeof v);
            for (size_t i = 0; i < sizeof u; ++i) byte(static_cast<uint8_t>(static_cast<uint64_t>(u) >> (8 * i)));
        }
    }
    template <typename T> void add(const T& v) {
        if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>) num(v);
        else if constexpr (std::is_array_v<T>) {
            for (const auto& e : v) add(e);
        } else {
            add_struct(v);
        }
    }
    template <typename T, size_t N> void add(const std::array<T, N>& v) {
        for (const auto& e : v) add(e);
    }
    template <typename T> void add(const std::vector<T>& v) {
        num(static_cast<uint32_t>(v.size()));
        for (const auto& e : v) add(e);
    }
    void add(const std::string& s) {
        num(static_cast<uint32_t>(s.size()));
        for (char c : s) byte(static_cast<uint8_t>(c));
    }

    // ---- moby (moby_mrp.h)
    void add_struct(const TexturePrim& t) {
        add(t.file);
        add(t.reg);
        add(t.next_secret);
        add(t.local_texture);
        add(t.texture);
        add(t.gs);
        add(t.gs_known);
        add(t.tex_entry);
        add(t.has_entry);
    }
    void add_struct(const StripOp& s) {
        add(s.slot);
        add(s.adc);
        add(s.texture);
    }
    void add_struct(const KickOp& k) {
        add(k.kind);
        add(k.pos);
        add(k.pos_q);
        add(k.col);
        add(k.uv);
        add(k.uv_q);
        add(k.uv_q2);
        add(k.adc);
        add(k.texture);
    }
    void add_struct(const SkinVertex& v) {
        add(v.kind);
        add(v.joint);
        add(v.cache_index);
        add(v.src);
        add(v.weight);
        add(v.store);
        add(v.blend_store);
        add(v.normal);
        add(v.pos);
        add(v.rgba);
        add(v.raw);
    }
    void add_struct(const Packet& p) {
        add(p.lod);
        add(p.index_in_lod);
        add(p.entry);
        add(p.vif);
        add(p.vdata);
        add(p.n_slots);
        add(p.uv);
        add(p.index_header);
        add(p.indices);
        add(p.textures);
        add(p.strip);
        add(p.kicks);
        add(p.kicks_clip);
        add(p.kicks_ok);
        add(p.kicks_note);
        add(p.vif_uv_addr);
        add(p.vif_index_addr);
        add(p.vif_tex_addr);
        add(p.vif_tex_off);
        add(p.matrix_transfers);
        add(p.vertices);
        add(p.duplicates);
        add(p.duplicate_source);
        add(p.stale_matrix_reads);
    }
    void add_struct(const MobyClass& c) {
        add(c.level);
        add(c.o_class);
        add(c.gadget);
        add(c.header);
        add(c.lod_count);
        add(c.shadow_count);
        add(c.shadow_first);
        add(c.joints_hi);
        add(c.joints_lo);
        add(c.tint_first);
        add(c.scale);
        add(c.bsphere);
        add(c.texture_remap);
        add(c.packets);
        add(c.joint_count);
        add(c.inverse_bind);
        add(c.joint_binds);
    }
    void add_struct(const Texture& t) {
        add(t.index);
        add(t.width);
        add(t.height);
        uint8_t pal[sizeof t.palette];  // (RGBA bytes as stored: their order in memory, the file's on any host)
        std::memcpy(pal, t.palette.data(), sizeof pal);
        add(pal);
        add(t.mips);
    }

    // ---- world (world_mrp.h; the derived images are not read from the file)
    void add_struct(const mrpw::Op& o) {
        add(o.kind);
        add(o.index);
        add(o.tag);
    }
    void add_struct(const mrpw::TexturePrim& t) {
        add(t.ad);
        add(t.n);
        add(t.local_texture);
        add(t.texture);
    }
    void add_struct(const mrpw::ShrubKickVertex& v) {
        add(v.pos);
        add(v.q);
        add(v.uv);
        add(v.colour);
    }
    void add_struct(const mrpw::TieKickVertex& v) {
        add(v.pos);
        add(v.q);
        add(v.uv);
        add(v.colour);
        add(v.fat);
        add(v.colour2);
    }
    void add_struct(const mrpw::ShrubPacket& p) {
        add(p.vif);
        add(p.header);
        add(p.pos);
        add(p.uv);
        add(p.textures);
        add(p.ops);
        add(p.verts);
        add(p.kick_ok);
        add(p.kick_note);
    }
    void add_struct(const mrpw::ShrubClass& c) {
        add(c.o_class);
        add(c.index);
        add(c.header);
        add(c.texture_remap);
        add(c.packets);
    }
    void add_struct(const mrpw::TiePacket& p) {
        add(p.lod);
        add(p.index_in_lod);
        add(p.entry);
        add(p.data);
        add(p.prims);
        add(p.n_dinky);
        add(p.n_fat);
        add(p.dinky);
        add(p.fat);
        add(p.colour_index);
        add(p.ops);
        add(p.verts);
        add(p.kick_ok);
        add(p.kick_note);
    }
    void add_struct(const mrpw::TieClass& c) {
        add(c.o_class);
        add(c.index);
        add(c.header);
        add(c.texture_remap);
        add(c.scale);
        add(c.textures);
        add(c.packets);
    }
    void add_struct(const mrpw::TfragImage& t) {
        add(t.colour_addr);
        add(t.colour_count);
        add(t.refs);
    }
    void add_struct(const mrpw::TfragEntry& t) {
        add(t.index);
        add(t.header);
        add(t.data);
        add(t.lods);
        add(t.textures);
    }
    void add_struct(const mrpw::World& w) {
        add(w.level);
        add(w.shrubs);
        add(w.ties);
        add(w.tfrags);
    }
};

std::string dir_name(int level) {
    if (level == 100) return "title";  // (mrpw::kTitleLevel / kFlightLevel: mrp_world_assets.h)
    if (level == 101) return "flight";
    return "level" + std::to_string(level);
}

}  // namespace

int asset_check(int level, const std::string& moby_root, const std::string& world_root) {
    Hash total;
    int files = 0, failed = 0;
    auto report = [&](const std::string& path, bool ok, uint64_t h) {
        ++files;
        if (!ok) ++failed;
        if (ok) std::printf("[mrp-check] %s %016" PRIx64 "\n", path.c_str(), h);
        else std::printf("[mrp-check] %s FAILED\n", path.c_str());
        total.add(static_cast<uint8_t>(ok ? 1 : 0));
        total.add(h);
    };
    const std::string md = moby_root + "/" + dir_name(level);
    std::vector<std::string> classes;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(md, ec), end; !ec && it != end; it.increment(ec))
        if (it->path().extension() == ".mrpm") classes.push_back(it->path().filename().string());
    std::sort(classes.begin(), classes.end());
    for (const std::string& name : classes) {
        MobyClass c;
        const bool ok = read_class(md + "/" + name, c);
        Hash h;
        if (ok) h.add(c);
        report(md + "/" + name, ok, h.h);
    }
    auto textures = [&](const std::string& path) {
        std::vector<Texture> t;
        const bool ok = read_textures(path, t);
        Hash h;
        if (ok) h.add(t);
        report(path, ok, h.h);
    };
    textures(md + "/textures.mrpt");
    const std::string wd = world_root + "/" + dir_name(level);
    {
        mrpw::World w;
        const bool ok = mrpw::read_world(wd + "/world.mrpw", w);
        Hash h;
        if (ok) h.add(w);
        report(wd + "/world.mrpw", ok, h.h);
    }
    for (const char* k : {"tfrag", "tie", "shrub"}) textures(wd + "/textures_" + k + ".mrpt");
    std::printf("[mrp-check] total %d files (%d failed) %016" PRIx64 "\n", files, failed, total.h);
    std::fflush(stdout);
    return failed;
}

}  // namespace rac1::mrp
