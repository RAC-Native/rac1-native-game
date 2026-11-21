// Oracle batch 542 (stage 5 task K3, #150): the level cache's tables chunk (src/formats/overlay_tables.*,
// src/game/level/level_cache_load.cpp) against the game's memory, on every level. Host checks at the boundary, no
// interpreter (the chunk is not code: what it must equal is the overlay as the level has it):
//  - tables/extract: each table of overlay_tables::table_defs() found in the level's overlay read from the disc, at the
//    place OV()'s translation gives (on the NTSC-U disc all seven are there on every level);
//  - tables/file: the cache file's bytes built in memory (level_cache::Writer), read back through level_cache::Reader (the
//    stamp, the chunk's version, the CRCs, each table's place against the translation), equal to the extraction;
//  - tables/load: each table equals the game's memory right after the overlay load (setup_level);
//  - tables/setup: equal again after the port's level setup and its first gameplay frame (veldin_boot,
//    level_setup_body(1, 0), gameplay_frame: as batch 512's level start), the live words left out (the save lists' read
//    flags);
//  - tables/compare: the comparison itself sees one changed byte of each table (outside its live words) and does not see a
//    changed live word.
// No RAM dumps; nothing is written to disk.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 542, N = 0 .. 18.

#include "ovl_oracle.h"

#include <formats/level_cache.h>
#include <formats/overlay_tables.h>
#include <game/level/level_cache_load.h>
#include <game/ovl/ovl_reloc.h>

#include <string>
#include <vector>

namespace rac1::oracle {

namespace {

namespace ot = overlay_tables;

void report(const char* name, bool ok, const std::string& detail) {
    std::printf("%-26s L%d: %s%s%s\n", name, g_oracle_level, ok ? "PASS" : "FAIL", detail.empty() ? "" : ": ", detail.c_str());
    if (!ok) ++g_fail;
}

std::string summary(const std::vector<ot::Table>& tables) {
    std::string s;
    size_t n = 0;
    for (const ot::Table& t : tables) {
        char b[64];
        std::snprintf(b, sizeof b, "%s%s %08x+%zx", s.empty() ? "" : ", ", level_cache::fourcc_name(t.id).c_str(), t.level_addr, t.bytes.size());
        s += b;
        n += t.bytes.size();
    }
    return std::to_string(tables.size()) + " tables, " + std::to_string(n) + " bytes: " + s;
}

bool same_tables(const std::vector<ot::Table>& a, const std::vector<ot::Table>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].id != b[i].id || a[i].veldin != b[i].veldin || a[i].level_addr != b[i].level_addr || a[i].stride != b[i].stride ||
            a[i].live != b[i].live || a[i].bytes != b[i].bytes)
            return false;
    return true;
}

}  // namespace

void batch_542() {
    if (g_only_batch != 542) return;
    g_batch = 542;
    const int level = g_oracle_level;

    // extract
    std::vector<ot::Table> tables;
    std::vector<std::string> notes;
    std::string why;
    const bool got = level_cache_extract(level, tables, notes, why);
    for (const std::string& n : notes) std::printf("  note: %s\n", n.c_str());
    report("tables/extract", got && notes.empty() && tables.size() == ot::table_defs().size(), got ? summary(tables) : why);
    if (!got) return;
    for (const ot::Table& t : tables)
        if (ovl_xlate_data(t.veldin) != t.level_addr) report("tables/extract", false, level_cache::fourcc_name(t.id) + " not where OV() says");

    // shape: each table's extent as its definition says (a table cut short would still equal memory)
    {
        std::string bad_shape;
        for (const ot::Table& t : tables) {
            const ot::TableDef* d = ot::table_def(t.id);
            const uint32_t n = static_cast<uint32_t>(t.bytes.size());
            auto word = [&t](uint32_t o) { return level_cache::rd_le32(&t.bytes[o]); };
            bool ok = d != nullptr && t.stride == d->stride && t.live == d->live && n % 4 == 0;
            if (ok && d->extent == ot::Extent::Fixed) ok = n == d->size;
            if (ok && d->extent == ot::Extent::RowsToZero) {  // (the first row whose address is 0 is the last)
                ok = n >= d->stride && n % d->stride == 0 && word(n - d->stride) == 0;
                for (uint32_t o = 0; ok && o + d->stride < n; o += d->stride) ok = word(o) != 0;
            }
            if (ok && d->extent == ot::Extent::Section) {  // (a -1 key row; the section's end: the next word is not the overlay's)
                bool end = false;
                for (uint32_t o = 0; o + 4 <= n && !end; o += d->stride) end = word(o) == 0xffffffffu;
                ok = end && n % d->stride == 0;
            }
            if (!ok && bad_shape.empty()) bad_shape = level_cache::fourcc_name(t.id) + " has " + std::to_string(n) + " bytes";
        }
        report("tables/shape", bad_shape.empty(), bad_shape);
    }

    // file: built, read back
    {
        level_cache::Stamp stamp;
        std::vector<uint8_t> file;
        std::vector<ot::Table> back;
        bool ok = level_cache_stamp(level, stamp, why) && level_cache_file_bytes(level, stamp, tables, file, why) &&
                  level_cache_read("(batch 542)", file, level, stamp, back, why);
        if (ok && !same_tables(tables, back)) {
            ok = false;
            why = "the tables read back differ from those written";
        }
        // and a file of another disc's sources is refused
        if (ok) {
            level_cache::Stamp other = stamp;
            other.fingerprint[0] ^= 1;
            std::string why2;
            if (level_cache_read("(batch 542)", file, level, other, back, why2)) {
                ok = false;
                why = "a file made from other sources was not refused";
            }
        }
        report("tables/file", ok, ok ? std::to_string(file.size()) + " bytes" : why);
    }

    // load: the overlay as setup_level loaded it
    std::string first;
    int bad = level_cache_compare(tables, first);
    report("tables/load", bad == 0, first);

    // setup: the port's level start and its first frame
    veldin_boot();
    level_setup_body(1, 0);
    w32(OV(0x0016c060), 0xf);
    w32(0x0013f2e8, 0xffffffffu);
    if (level == 14) w8(0x0013d3f0, 1);  // (Oltanis: the arrival scene seen, as batch 221's)
    gameplay_frame();
    bad = level_cache_compare(tables, first);
    report("tables/setup", bad == 0, first);

    // compare: the comparison sees a changed byte, not a changed live word
    {
        int seen = 0, live_seen = 0, lives = 0;
        for (size_t i = 0; i < tables.size(); ++i) {
            std::vector<ot::Table> one{tables[i]};
            ot::Table& t = one[0];
            if (level_cache_compare(one, first) != 0) continue;  // (reported above)
            uint32_t at = static_cast<uint32_t>(t.bytes.size() / 2) & ~3u;
            while (at < t.bytes.size() && ot::is_live(t, at)) at += 4;
            if (at >= t.bytes.size()) continue;
            t.bytes[at + 1] ^= 0x10;
            seen += level_cache_compare(one, first) == 1 ? 1 : 0;
            t.bytes[at + 1] ^= 0x10;
            for (uint32_t o = 0; o < t.bytes.size(); o += 4)
                if (ot::is_live(t, o)) {
                    ++lives;
                    t.bytes[o] ^= 0x5a;
                    live_seen += level_cache_compare(one, first) != 0 ? 1 : 0;
                    t.bytes[o] ^= 0x5a;
                    break;
                }
        }
        report("tables/compare", seen == static_cast<int>(tables.size()) && live_seen == 0,
               std::to_string(seen) + " of " + std::to_string(tables.size()) + " changed bytes seen, " + std::to_string(live_seen) + " of " +
                   std::to_string(lives) + " changed live words seen");
    }
}

}  // namespace rac1::oracle
