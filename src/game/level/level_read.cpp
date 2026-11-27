// The level data's typed read API's native side (stage 5, task LD4): see level_read.h. A native read finds the record behind an
// EE address through the address index (level_registry.cpp: a binary search), checks that the record is of the accessor's
// family, and copies the word at the same byte offset from the record's host container; anything else is a miss, read from EE
// and counted. The check mode compares each native word with the EE word.
#include <game/level/level_read.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_MSC_VER)
#include <intrin.h>
#define RAC1_LD_CALLER _ReturnAddress()
#else
#define RAC1_LD_CALLER __builtin_return_address(0)
#endif

namespace rac1::lvd {

namespace {

ReadMode mode_from_env() {
    const char* e = std::getenv("RAC1_LD_READ");
    return e != nullptr && std::strcmp(e, "native") == 0 ? ReadMode::NATIVE : ReadMode::EE;
}
bool check_from_env() {
    const char* e = std::getenv("RAC1_LD_READ_CHECK");
    return e != nullptr && std::atoi(e) != 0;
}

constexpr size_t N_ACC = static_cast<size_t>(ReadAcc::COUNT);
constexpr size_t MISS_KEPT = 4;    // the first addresses kept per accessor
constexpr uint64_t DIFFS_SAID = 50;  // the differences printed

const char* const kNames[N_ACC] = {
    "path_u32", "path_s32", "path_f32", "path_u16", "path_s16", "path_row", "path_copy_row", "path_n", "path_pt", "path_lane", "path_head", "path_view",
    "shape_u32", "shape_s32", "shape_f32", "shape_u16", "shape_s16", "shape_row", "shape_copy_row", "shape_centre", "shape_mat",
    "volume_u32", "volume_s32", "volume_f32", "volume_u16", "volume_s16", "volume_row", "volume_copy_row", "volume_centre", "volume_mat", "volume_radius",
    "camera_u32", "camera_s32", "camera_f32", "camera_u16", "camera_s16", "camera_row", "camera_copy_row", "camera_pos", "camera_type", "camera_rot", "camera_data_of",
    "rail_u32", "rail_s32", "rail_f32", "rail_u16", "rail_s16", "rail_row", "rail_copy_row", "rail_centre", "rail_reach", "rail_spine", "rail_loop",
};
static_assert(sizeof(kNames) / sizeof(kNames[0]) == N_ACC);

enum class Family : uint8_t { PATH, SHAPE, VOLUME, CAMERA, RAIL };
Family family_of(ReadAcc acc) {
    const auto a = static_cast<uint8_t>(acc);
    if (a < static_cast<uint8_t>(ReadAcc::SHAPE_U32)) return Family::PATH;
    if (a < static_cast<uint8_t>(ReadAcc::VOLUME_U32)) return Family::SHAPE;
    if (a < static_cast<uint8_t>(ReadAcc::CAMERA_U32)) return Family::VOLUME;
    if (a < static_cast<uint8_t>(ReadAcc::RAIL_U32)) return Family::CAMERA;
    return Family::RAIL;
}
// a record of this kind serves this accessor (a shape accessor also reads a volume's first 0x80 bytes: the same layout)
bool kind_serves(RecordKind k, Family f) {
    switch (f) {
    case Family::PATH: return k == RecordKind::PATH || k == RecordKind::SPINE || k == RecordKind::ADDED_PATH;
    case Family::SHAPE: return k == RecordKind::SHAPE || k == RecordKind::VOLUME;
    case Family::VOLUME: return k == RecordKind::VOLUME;
    case Family::CAMERA: return k == RecordKind::CAMERA;
    default: return k == RecordKind::RAIL;
    }
}

struct Counter {
    uint64_t reads = 0, misses = 0, differs = 0;
    std::array<uint32_t, MISS_KEPT> first{};
    std::array<uint8_t, MISS_KEPT> why{};  // 0 no record, 1 another kind, 2 a word the record does not hold
    std::array<const void*, MISS_KEPT> from{};  // the reader's code address (the accessor is inlined into it)
};
std::array<Counter, N_ACC> g_count;
uint64_t g_differs_said = 0;
bool g_native_seen = false;
const char* const kWhy[3] = {"no record", "another kind", "not held"};

void miss(Counter& c, uint32_t a, uint8_t why, const void* from) {
    if (c.misses < MISS_KEPT) {
        c.first[c.misses] = a;
        c.why[c.misses] = why;
        c.from[c.misses] = from;
    }
    ++c.misses;
}

void differs(ReadAcc acc, Counter& c, uint32_t a, uint32_t nat, uint32_t ee) {
    ++c.differs;
    if (g_differs_said++ < DIFFS_SAID)
        std::printf("[ld-read] differs: %s at %08x: native %08x, EE %08x\n", kNames[static_cast<size_t>(acc)], a, nat, ee);
}

// the native word at the word-aligned EE address `w`, with `r` the record holding it (looked up by the caller); false: a miss
bool word_of(const RecordRef& r, uint32_t w, Family f, Counter& c, uint32_t& out, const void* from) {
    if (!r) {
        miss(c, w, 0, from);
        return false;
    }
    if (!kind_serves(r.kind, f)) {
        miss(c, w, 1, from);
        return false;
    }
    RecordRef at = r;
    at.offset = w - r.ee;
    const void* slot = record_slot(at);
    if (slot == nullptr) {
        miss(c, w, 2, from);
        return false;
    }
    std::memcpy(&out, slot, 4);
    return true;
}

}  // namespace

ReadMode g_ld_read = mode_from_env();
bool g_ld_read_check = check_from_env();

void set_read_mode(ReadMode m) {
    g_ld_read = m;
    if (m == ReadMode::NATIVE) g_native_seen = true;
}
void set_read_check(bool on) { g_ld_read_check = on; }

const char* read_acc_name(ReadAcc acc) { return static_cast<size_t>(acc) < N_ACC ? kNames[static_cast<size_t>(acc)] : "?"; }

// orig: (host) see level_read.h
uint32_t read_native_word(uint32_t a, ReadAcc acc) {
    g_native_seen = true;
    Counter& c = g_count[static_cast<size_t>(acc)];
    ++c.reads;
    const uint32_t w = a & ~3u;
    uint32_t v;
    if (!word_of(record_at(w), w, family_of(acc), c, v, RAC1_LD_CALLER)) return lwu(w);
    if (g_ld_read_check) {
        const uint32_t e = lwu(w);
        if (e != v) differs(acc, c, w, v, e);
    }
    return v;
}

namespace {
void words_from(uint32_t a, uint32_t* out, uint32_t n, ReadAcc acc, const void* from) {
    g_native_seen = true;
    Counter& c = g_count[static_cast<size_t>(acc)];
    const Family f = family_of(acc);
    RecordRef r;
    for (uint32_t k = 0; k < n; ++k) {
        ++c.reads;
        const uint32_t w = (a & ~3u) + 4 * k;
        if (!r || w < r.ee || w - r.ee >= r.size) r = record_at(w);
        if (!word_of(r, w, f, c, out[k], from)) {
            out[k] = lwu(w);
            continue;
        }
        if (g_ld_read_check) {
            const uint32_t e = lwu(w);
            if (e != out[k]) differs(acc, c, w, out[k], e);
        }
    }
}
}  // namespace

// orig: (host) see level_read.h
void read_native_words(uint32_t a, uint32_t* out, uint32_t n, ReadAcc acc) { words_from(a, out, n, acc, RAC1_LD_CALLER); }

// orig: (host) see level_read.h
void read_native_copy_row(uint32_t dst, uint32_t a, ReadAcc acc) {
    uint32_t w[4];
    words_from(a & ~15u, w, 4, acc, RAC1_LD_CALLER);
    const uint32_t d = dst & ~15u;
    for (uint32_t k = 0; k < 4; ++k) sw(d + 4 * k, w[k]);
}

// orig: (host) see level_read.h (PathView's lookup, once)
void PathView::resolve(uint32_t a) {
    const RecordRef r = record_at(a);
    if (!r || !kind_serves(r.kind, Family::PATH)) return;
    rec_ = record_path(r);
    if (rec_ == nullptr) return;
    ee_ = r.ee;
    size_ = r.size;
    gen_ = g_ld_gen;
}

ReadStats read_stats(ReadAcc acc) {
    const Counter& c = g_count[static_cast<size_t>(acc)];
    return ReadStats{c.reads, c.misses, c.differs};
}
ReadStats read_stats_total() {
    ReadStats s;
    for (const Counter& c : g_count) {
        s.reads += c.reads;
        s.misses += c.misses;
        s.differs += c.differs;
    }
    return s;
}
void read_stats_reset() {
    g_count = {};
    g_differs_said = 0;
}

// orig: (host) see level_read.h
void read_report() {
    if (!g_native_seen) return;
    const ReadStats t = read_stats_total();
    std::printf("[ld-read] mode %s, check %s: %llu native reads (words), %llu misses read from EE, %llu differing\n",
                g_ld_read == ReadMode::NATIVE ? "native" : "ee", g_ld_read_check ? "on" : "off", static_cast<unsigned long long>(t.reads),
                static_cast<unsigned long long>(t.misses), static_cast<unsigned long long>(t.differs));
    for (size_t k = 0; k < N_ACC; ++k) {
        const Counter& c = g_count[k];
        if (c.misses == 0) continue;
        std::printf("[ld-read] miss: %s: %llu of %llu reads (first", kNames[k], static_cast<unsigned long long>(c.misses), static_cast<unsigned long long>(c.reads));
        for (size_t i = 0; i < MISS_KEPT && i < c.misses; ++i)
        {
            const long long d = static_cast<long long>(reinterpret_cast<uintptr_t>(c.from[i]) - reinterpret_cast<uintptr_t>(&read_report));
            std::printf(" %08x %s @%c%llx", c.first[i], kWhy[c.why[i]], d < 0 ? '-' : '+', static_cast<unsigned long long>(d < 0 ? -d : d));
        }
        std::printf(")\n");
    }
    if (t.misses != 0)
        std::printf("[ld-read] (a miss's @offset: its reader's code address from read_report's; addr2line -f at read_report's symbol + offset)\n");
    if (t.differs > DIFFS_SAID) std::printf("[ld-read] differs: %llu in all (the first %llu printed)\n", static_cast<unsigned long long>(t.differs), static_cast<unsigned long long>(DIFFS_SAID));
    std::fflush(stdout);
}

}  // namespace rac1::lvd
