// See overlay_tables.h (the tables chunk), docs/formats/README.md ("The tables chunk").
#include <formats/overlay_tables.h>

#include <algorithm>
#include <cstdio>

namespace rac1::overlay_tables {

namespace {

using level_cache::fourcc;
using level_cache::rd_le32;
using level_cache::wr_le32;

// The tables (stage 5, K3). Each is read by the game and written by nothing after the overlay load but the words its live
// mask names; the evidence (the code's writers, the guard's census on the tours) is in the K3 report.
const TableDef kDefs[] = {
    // The save's two lists of records (16 bytes: the data's address, its size, its id, a read flag; ended by address 0):
    // the game's state, then each level's. Read by the card code (save_list_size, save_block_read / _write) and the level
    // slots; save_block_read writes each record's read flag (+0xc) on every save read and new game: that word is state.
    {fourcc("SAVG"), "the save's game list", 0x001845c0u, Extent::RowsToZero, 0, 0x10, 1u << 3},
    {fourcc("SAVL"), "the save's level list", 0x001848c0u, Extent::RowsToZero, 0, 0x10, 1u << 3},
    // The talkers' ranges: one word per level 0..0x13, the first of the level's talking mobys (talk.cpp, level_change.cpp,
    // level_setup.cpp read [level] and [level + 1])
    {fourcc("TALK"), "the talkers' ranges", 0x001c44b8u, Extent::Fixed, 0x50, 0, 0},
    // The GIF tag quadword of the sparkles' and the screen quads' packets (sparkles_draw, gs_quad_packet, and the levels'
    // generated copies: an lq, never a store)
    {fourcc("GIFT"), "the quads' GIF tag", 0x001609a0u, Extent::Fixed, 0x10, 0, 0},
    // The overlay's three tables of rows ended by a -1 key, each a section of its own (3, 4, 5): the classes' update
    // functions (class, function, +0x2c word: class_update_fn_set), the camera types (id, accept / init / update / exit:
    // camera.cpp, hero_modes.cpp copies it into a session's heap, never in place), the records' handlers (id, function:
    // sub_0028df58)
    {fourcc("CLSR"), "the classes' rows", 0x001ea300u, Extent::Section, 0, 0xc, 0},
    {fourcc("CAMT"), "the camera types", 0x001ea880u, Extent::Section, 0, 0x14, 0},
    {fourcc("HNDL"), "the handlers' rows", 0x001ea980u, Extent::Section, 0, 0x8, 0},
};

std::string hex(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "%08x", v);
    return b;
}

constexpr uint32_t kHeader = 0x10, kEntry = 0x20;
constexpr uint32_t align16(uint32_t v) { return (v + 15u) & ~15u; }

}  // namespace

// orig: (host) see overlay_tables.h
std::span<const TableDef> table_defs() { return kDefs; }

// orig: (host) see overlay_tables.h
const TableDef* table_def(uint32_t id) {
    for (const TableDef& d : kDefs)
        if (d.id == id) return &d;
    return nullptr;
}

// orig: (host) see overlay_tables.h
bool is_live(const Table& t, uint32_t offset) {
    if (t.live == 0 || t.stride == 0) return false;
    const uint32_t word = (offset % t.stride) / 4;
    return word < 32 && ((t.live >> word) & 1u) != 0;
}

// orig: (host) see overlay_tables.h
bool parse_overlay(std::span<const uint8_t> head, std::vector<Section>& out, std::string& why) {
    out.clear();
    if (head.size() < 8) {
        why = "the level's data file is shorter than its header";
        return false;
    }
    const uint32_t off = rd_le32(head.data()), size = rd_le32(head.data() + 4);
    if (size == 0 || off > head.size() || size > head.size() - off) {
        why = "the overlay's place in the data file (" + hex(off) + ", " + hex(size) + " bytes) is not inside what was read";
        return false;
    }
    const std::span<const uint8_t> exe = head.subspan(off, size);
    // the blocks: [address, size, type, entry] then the bytes; every block repeats the first's entry point, the first that
    // does not ends the list (ratchet_executable.h)
    uint32_t entry = 0;
    for (size_t p = 0; p + 16 <= exe.size();) {
        const uint32_t addr = rd_le32(&exe[p]), n = rd_le32(&exe[p + 4]), type = rd_le32(&exe[p + 8]), e = rd_le32(&exe[p + 12]);
        if (out.empty()) entry = e;
        else if (e != entry) break;
        Section s;
        s.index = static_cast<int>(out.size());
        s.addr = addr;
        s.size = n;
        s.type = type;
        if (type != 8) {  // (NOBITS: zeros, no bytes in the file)
            if (n > exe.size() - p - 16) {
                why = "overlay block " + std::to_string(s.index) + " runs past the overlay's end";
                return false;
            }
            s.bytes = exe.subspan(p + 16, n);
            p += 16 + n;
        } else {
            p += 16 + n;  // (the game's loader skips the declared size either way)
        }
        out.push_back(s);
    }
    if (out.empty()) {
        why = "the overlay has no blocks";
        return false;
    }
    return true;
}

// orig: (host) see overlay_tables.h
void extract(std::span<const Section> sections, const Xlate& xlate, const Fix& fix, std::vector<Table>& out,
             std::vector<std::string>& notes) {
    out.clear();
    for (const TableDef& d : kDefs) {
        const uint32_t at = xlate(d.veldin);
        const Section* sec = nullptr;
        for (const Section& s : sections)
            if (s.type == 1 && !s.bytes.empty() && at >= s.addr && at - s.addr < s.size) {
                sec = &s;
                break;
            }
        const std::string what = std::string(d.name) + " (" + hex(d.veldin) + " -> " + hex(at) + ")";
        if (sec == nullptr) {
            notes.push_back(what + ": in no section of the overlay with bytes: left out");
            continue;
        }
        const uint32_t pos = at - sec->addr, room = sec->size - pos;
        uint32_t size = 0;
        switch (d.extent) {
        case Extent::Fixed:
            size = d.size;
            break;
        case Extent::RowsToZero:
            for (uint32_t o = 0; o + d.stride <= room; o += d.stride)
                if (rd_le32(&sec->bytes[pos + o]) == 0) {
                    size = o + d.stride;
                    break;
                }
            if (size == 0) {
                notes.push_back(what + ": no end row in its section: left out");
                continue;
            }
            break;
        case Extent::Section: {
            if (pos != 0) {
                notes.push_back(what + ": not the start of a section: left out");
                continue;
            }
            size = sec->size;
            bool end = false;
            for (uint32_t o = 0; o + 4 <= size && !end; o += d.stride) end = rd_le32(&sec->bytes[o]) == 0xffffffffu;
            if (!end) {
                notes.push_back(what + ": no -1 row in its section: left out");
                continue;
            }
            break;
        }
        }
        if (size > room) {
            notes.push_back(what + ": runs past its section's end: left out");
            continue;
        }
        // one piece: the table's last word where its first word's place says (the same block of the maps); the sections
        // are the level's own, whole
        if (d.extent != Extent::Section && size >= 4 && xlate(d.veldin + size - 4) != at + size - 4) {
            notes.push_back(what + ": its last word translates to " + hex(xlate(d.veldin + size - 4)) + ", not " + hex(at + size - 4) +
                            ": the maps do not place it as one piece: left out");
            continue;
        }
        Table t;
        t.id = d.id;
        t.veldin = d.veldin;
        t.level_addr = at;
        t.stride = d.stride;
        t.live = d.live;
        t.bytes.assign(sec->bytes.begin() + pos, sec->bytes.begin() + pos + size);
        if (fix) fix(at, t.bytes);
        out.push_back(std::move(t));
    }
}

// orig: (host) see overlay_tables.h
std::vector<uint8_t> encode(int level, std::span<const Table> tables) {
    const uint32_t n = static_cast<uint32_t>(tables.size());
    uint32_t at = align16(kHeader + n * kEntry), total = 0;
    std::vector<uint8_t> out(at);
    wr_le32(&out[0], static_cast<uint32_t>(level));
    wr_le32(&out[4], n);
    for (uint32_t i = 0; i < n; ++i) {
        const Table& t = tables[i];
        const uint32_t size = static_cast<uint32_t>(t.bytes.size());
        uint8_t* e = &out[kHeader + i * kEntry];
        wr_le32(e + 0x00, t.id);
        wr_le32(e + 0x04, t.veldin);
        wr_le32(e + 0x08, t.level_addr);
        wr_le32(e + 0x0c, size);
        wr_le32(e + 0x10, at);
        wr_le32(e + 0x14, t.stride);
        wr_le32(e + 0x18, t.live);
        out.resize(align16(at + size));
        std::copy(t.bytes.begin(), t.bytes.end(), out.begin() + at);
        at = static_cast<uint32_t>(out.size());
        total += size;
    }
    wr_le32(&out[8], total);
    return out;
}

// orig: (host) see overlay_tables.h
bool decode(std::span<const uint8_t> c, int& level, std::vector<Table>& out, std::string& why) {
    out.clear();
    if (c.size() < kHeader) {
        why = "the tables chunk is shorter than its header";
        return false;
    }
    const uint32_t lv = rd_le32(&c[0]), n = rd_le32(&c[4]), total = rd_le32(&c[8]);
    if (lv > 18 || rd_le32(&c[12]) != 0 || n > 256 || c.size() < kHeader + static_cast<size_t>(n) * kEntry) {
        why = "the tables chunk's header is not a version 1 header";
        return false;
    }
    uint32_t sum = 0;
    for (uint32_t i = 0; i < n; ++i) {
        const uint8_t* e = &c[kHeader + i * kEntry];
        Table t;
        t.id = rd_le32(e);
        t.veldin = rd_le32(e + 4);
        t.level_addr = rd_le32(e + 8);
        const uint32_t size = rd_le32(e + 0xc), off = rd_le32(e + 0x10);
        t.stride = rd_le32(e + 0x14);
        t.live = rd_le32(e + 0x18);
        if (rd_le32(e + 0x1c) != 0 || off % 16 != 0 || off < kHeader + n * kEntry || off > c.size() || size > c.size() - off ||
            t.stride % 4 != 0) {
            why = "table " + std::to_string(i) + " of the tables chunk is out of its bounds";
            return false;
        }
        t.bytes.assign(c.begin() + off, c.begin() + off + size);
        sum += size;
        out.push_back(std::move(t));
    }
    if (sum != total) {
        why = "the tables chunk's byte count does not add up";
        return false;
    }
    level = static_cast<int>(lv);
    return true;
}

}  // namespace rac1::overlay_tables
