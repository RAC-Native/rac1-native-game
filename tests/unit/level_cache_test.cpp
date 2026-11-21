// The level cache's container (src/formats/level_cache.h): its byte order, its checks and its atomic write, on any host.
// The inputs are synthetic bytes written here, never the game's. The hand-built image below was made independently of
// the library (Python's struct and zlib.crc32), so decoding it and building it again prove the little-endian layout on
// big-endian and little-endian hosts alike.
//
//   level_cache_test [a folder for its temporary files: default the system's]
#include <formats/level_cache.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace lc = rac1::level_cache;

namespace {

int g_checks = 0, g_failed = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failed;                                                          \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
        }                                                                        \
    } while (0)

// a result with the status expected (and, for a failure, a reason that names the file)
#define CHECK_STATUS(expr, st)                                                                                \
    do {                                                                                                      \
        const lc::Result r_ = (expr);                                                                         \
        ++g_checks;                                                                                           \
        const bool ok_ = r_.status == (st) && (r_.status == lc::Status::Ok) == r_.why.empty();                \
        if (!ok_) {                                                                                           \
            ++g_failed;                                                                                       \
            std::printf("  FAIL %s:%d: %s: %s (%s), expected %s\n", __FILE__, __LINE__, #expr,                \
                        lc::status_name(r_.status), r_.why.c_str(), lc::status_name(st));                     \
        }                                                                                                     \
    } while (0)

// A cache file built by hand (Python: struct.pack("<...") and zlib.crc32), 131 bytes:
//   header: "R1LC", version 1, flags 0, serial "TEST-00001", region 2 (PAL), fingerprint a0..b3, 2 chunks, size 0x83,
//           CRC 0x441414b8
//   table:  "TABL" version 0x0102 at 0x70, 5 bytes, CRC 0xfade4a3e; "CLAS" version 3 at 0x80, 3 bytes, CRC 0xbe4df84c
//   chunks: 44 33 22 11 55 at 0x70, aa bb cc at 0x80
const uint8_t kImage[131] = {
    0x52, 0x31, 0x4c, 0x43, 0x01, 0x00, 0x00, 0x00, 0x54, 0x45, 0x53, 0x54, 0x2d, 0x30, 0x30, 0x30,
    0x30, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0xa0, 0xa1, 0xa2, 0xa3,
    0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3,
    0x02, 0x00, 0x00, 0x00, 0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb8, 0x14, 0x14, 0x44,
    0x54, 0x41, 0x42, 0x4c, 0x02, 0x01, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
    0x3e, 0x4a, 0xde, 0xfa, 0x43, 0x4c, 0x41, 0x53, 0x03, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x4c, 0xf8, 0x4d, 0xbe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x44, 0x33, 0x22, 0x11, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xaa, 0xbb, 0xcc,
};
const uint8_t kTabl[5] = {0x44, 0x33, 0x22, 0x11, 0x55};
const uint8_t kClas[3] = {0xaa, 0xbb, 0xcc};

lc::Stamp image_stamp() {
    lc::Stamp s;
    s.serial = "TEST-00001";
    s.region = lc::Region::Pal;
    for (size_t i = 0; i < s.fingerprint.size(); ++i) s.fingerprint[i] = static_cast<uint8_t>(0xa0 + i);
    return s;
}

std::vector<uint8_t> image() { return std::vector<uint8_t>(kImage, kImage + sizeof kImage); }

// the header's CRC made right again after a change (so that the check behind it is the one reached)
void refresh_crc(std::vector<uint8_t>& b) {
    const uint32_t count = lc::rd_le32(b.data() + 0x30);
    const uint32_t crc = lc::crc32(lc::crc32(0, b.data(), 0x3c), b.data() + 64, 20 * count);
    for (int i = 0; i < 4; ++i) b[0x3c + static_cast<size_t>(i)] = static_cast<uint8_t>(crc >> (8 * i));
}

void put32(std::vector<uint8_t>& b, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) b[at + static_cast<size_t>(i)] = static_cast<uint8_t>(v >> (8 * i));
}

std::string hex(const lc::Fingerprint& f) {
    std::string s;
    char t[3];
    for (uint8_t b : f) {
        std::snprintf(t, sizeof t, "%02x", b);
        s += t;
    }
    return s;
}

lc::Fingerprint sha1_of(const std::string& s) {
    lc::Sha1 h;
    h.update(s.data(), s.size());
    return h.finish();
}

lc::Result open_bytes(lc::Reader& r, const std::vector<uint8_t>& b) { return r.open_memory(b, "test image"); }

// ---- the tests ----

void test_byte_order() {
    const uint8_t b[8] = {0x04, 0x03, 0x02, 0x01, 0x08, 0x07, 0x06, 0x05};
    CHECK(lc::rd_le16(b) == 0x0304);
    CHECK(lc::rd_le32(b) == 0x01020304u);
    CHECK(lc::rd_le64(b) == 0x0506070801020304ull);
    uint8_t w[8] = {};
    lc::wr_le16(w, 0xbeef);
    CHECK(w[0] == 0xef && w[1] == 0xbe);
    lc::wr_le32(w, 0xdeadbeefu);
    CHECK(w[0] == 0xef && w[1] == 0xbe && w[2] == 0xad && w[3] == 0xde);
    lc::wr_le64(w, 0x0102030405060708ull);
    CHECK(w[0] == 0x08 && w[3] == 0x05 && w[4] == 0x04 && w[7] == 0x01);
    lc::wr_le32(w, lc::fourcc("TABL"));  // (an id's bytes read as its name)
    CHECK(std::memcmp(w, "TABL", 4) == 0);
    CHECK(lc::kMagic == lc::rd_le32(reinterpret_cast<const uint8_t*>("R1LC")));
    CHECK(lc::fourcc_name(lc::fourcc("CLAS")) == "CLAS");
    CHECK(lc::fourcc_name(0x00414243u) == "CBA?");
}

void test_crc32() {
    CHECK(lc::crc32(0, "123456789", 9) == 0xcbf43926u);  // (the CRC-32 check value)
    CHECK(lc::crc32(lc::crc32(0, "1234", 4), "56789", 5) == 0xcbf43926u);
    CHECK(lc::crc32(0, nullptr, 0) == 0);
    CHECK(lc::crc32(0, kTabl, sizeof kTabl) == 0xfade4a3eu);  // (zlib.crc32's)
    CHECK(lc::crc32(0, kClas, sizeof kClas) == 0xbe4df84cu);
}

void test_sha1() {
    // FIPS 180 test vectors
    CHECK(hex(sha1_of("")) == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    CHECK(hex(sha1_of("abc")) == "a9993e364706816aba3e25717850c26c9cd0d89d");
    CHECK(hex(sha1_of("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
          "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    lc::Sha1 h;  // a million 'a', in uneven pieces (the buffering across blocks)
    const std::string a(1000, 'a');
    for (int i = 0; i < 1000; ++i) {
        h.update(a.data(), 1);
        h.update(a.data() + 1, 62);
        h.update(a.data() + 63, 937);
    }
    CHECK(hex(h.finish()) == "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
    CHECK(hex(h.finish()) == "da39a3ee5e6b4b0d3255bfef95601890afd80709");  // (finish starts again)
}

void test_source_fingerprint() {
    // the same as Python's: h.update(struct.pack("<I", len(n)) + n + struct.pack("<Q", len(d)) + d) per source
    std::vector<uint8_t> level(200);
    for (size_t i = 0; i < level.size(); ++i) level[i] = static_cast<uint8_t>(i);
    lc::SourceFingerprint f;
    f.begin("BOOT.ELF", 5);
    f.add("hello", 5);
    f.begin("level03/data", level.size());
    f.add(level.data(), 77);  // (in pieces)
    f.add(level.data() + 77, level.size() - 77);
    lc::Fingerprint out{};
    CHECK(f.finish(out));
    CHECK(hex(out) == "7cfac9d6453d86285b0c9e9d40251a02fdbce7fd");
    // a source shorter than begun (a short read): no fingerprint
    lc::Fingerprint keep{};
    keep[0] = 0x5a;
    f.begin("BOOT.ELF", 5);
    f.add("hell", 4);
    CHECK(!f.finish(keep) && keep[0] == 0x5a);
    // bytes of no source
    f.add("x", 1);
    CHECK(!f.finish(keep));
    // and the builder starts again after a finish
    f.begin("BOOT.ELF", 5);
    f.add("hello", 5);
    f.begin("level03/data", level.size());
    f.add(level.data(), level.size());
    CHECK(f.finish(out) && hex(out) == "7cfac9d6453d86285b0c9e9d40251a02fdbce7fd");
}

void test_decode_hand_built() {
    lc::Reader r;
    CHECK_STATUS(r.open_memory(std::span<const uint8_t>(kImage, sizeof kImage)), lc::Status::Ok);
    CHECK(r.format_version() == 1 && r.flags() == 0 && r.file_size() == 131);
    CHECK(r.stamp().serial == "TEST-00001");
    CHECK(r.stamp().region == lc::Region::Pal);
    CHECK(r.stamp().fingerprint == image_stamp().fingerprint);
    CHECK_STATUS(r.check_stamp(image_stamp()), lc::Status::Ok);
    CHECK(r.chunks().size() == 2);
    if (r.chunks().size() != 2) return;
    const lc::Chunk& t = r.chunks()[0];
    const lc::Chunk& c = r.chunks()[1];
    CHECK(t.id == lc::fourcc("TABL") && t.version == 0x0102 && t.offset == 0x70 && t.size == 5 && t.crc == 0xfade4a3eu);
    CHECK(c.id == lc::fourcc("CLAS") && c.version == 3 && c.offset == 0x80 && c.size == 3 && c.crc == 0xbe4df84cu);
    uint8_t buf[8] = {};
    CHECK_STATUS(r.read(t, buf, sizeof buf), lc::Status::Ok);
    CHECK(lc::rd_le32(buf) == 0x11223344u && buf[4] == 0x55);
    std::span<const uint8_t> v;
    CHECK_STATUS(r.view(c, v), lc::Status::Ok);
    CHECK(v.data() == kImage + 0x80 && v.size() == 3 && v[0] == 0xaa && v[2] == 0xcc);
    const lc::Chunk* p = nullptr;
    CHECK_STATUS(r.require(lc::fourcc("CLAS"), 3, p), lc::Status::Ok);
    CHECK(p == &c);
}

void test_encode_matches() {
    lc::Writer w(image_stamp());
    CHECK_STATUS(w.add(lc::fourcc("TABL"), 0x0102, kTabl), lc::Status::Ok);
    CHECK_STATUS(w.add(lc::fourcc("CLAS"), 3, kClas), lc::Status::Ok);
    std::vector<uint8_t> out;
    CHECK_STATUS(w.build(out), lc::Status::Ok);
    CHECK(out == image());  // (byte for byte the hand-built file)
}

void test_writer_limits() {
    lc::Writer w(image_stamp());
    CHECK_STATUS(w.add(lc::fourcc("TABL"), 1, kTabl), lc::Status::Ok);
    CHECK_STATUS(w.add(lc::fourcc("TABL"), 2, kClas), lc::Status::BadArgument);
    lc::Stamp longer = image_stamp();
    longer.serial = "SCUS-97199-1.00-X";  // (17 characters)
    std::vector<uint8_t> out;
    CHECK_STATUS(lc::Writer(longer).build(out), lc::Status::BadArgument);
    // no chunks at all: a 64-byte file
    CHECK_STATUS(lc::Writer(image_stamp()).build(out), lc::Status::Ok);
    lc::Reader r;
    CHECK(out.size() == 64);
    CHECK_STATUS(open_bytes(r, out), lc::Status::Ok);
    CHECK(r.chunks().empty());
    // the most chunks a file holds, and one more
    lc::Writer many(image_stamp());
    uint8_t one = 0;
    for (uint32_t i = 0; i < lc::kMaxChunks; ++i) {
        one = static_cast<uint8_t>(i);
        if (!many.add(i, 1, std::span<const uint8_t>(&one, 1))) {
            CHECK(false);
            break;
        }
    }
    CHECK_STATUS(many.add(lc::kMaxChunks, 1, {}), lc::Status::BadArgument);
    CHECK_STATUS(many.build(out), lc::Status::Ok);
    CHECK_STATUS(open_bytes(r, out), lc::Status::Ok);
    CHECK(r.chunks().size() == lc::kMaxChunks);
    uint8_t got = 0;
    if (r.chunks().size() == lc::kMaxChunks) {
        CHECK_STATUS(r.read(r.chunks()[700], &got, 1), lc::Status::Ok);
        CHECK(got == static_cast<uint8_t>(700) && r.chunks()[700].offset % 16 == 0);
    }
}

void test_rejects() {
    lc::Reader r;
    std::vector<uint8_t> b;
    // short files
    CHECK_STATUS(open_bytes(r, {}), lc::Status::Truncated);
    CHECK_STATUS(open_bytes(r, {0x52, 0x31, 0x4c}), lc::Status::Truncated);
    b.assign(kImage, kImage + 6);
    CHECK_STATUS(open_bytes(r, b), lc::Status::Truncated);
    b.assign(kImage, kImage + 40);
    CHECK_STATUS(open_bytes(r, b), lc::Status::Truncated);
    b.assign(kImage, kImage + 80);  // (in the table)
    CHECK_STATUS(open_bytes(r, b), lc::Status::Truncated);
    b.assign(kImage, kImage + 120);  // (in the second chunk)
    CHECK_STATUS(open_bytes(r, b), lc::Status::Truncated);
    b = image();
    b.push_back(0);  // (a byte past the end the header gives)
    CHECK_STATUS(open_bytes(r, b), lc::Status::BadTable);
    // not a cache file
    b = image();
    b[3] = 'X';
    CHECK_STATUS(open_bytes(r, b), lc::Status::BadMagic);
    CHECK_STATUS(open_bytes(r, {'n', 'o'}), lc::Status::Truncated);
    b.assign(kImage, kImage + 4);
    b[0] = 'r';
    CHECK_STATUS(open_bytes(r, b), lc::Status::BadMagic);
    // versions: a newer and an older format
    b = image();
    b[4] = 2;
    CHECK_STATUS(open_bytes(r, b), lc::Status::NewerVersion);
    b[4] = 0;
    b[5] = 1;  // (256: the version's high byte)
    CHECK_STATUS(open_bytes(r, b), lc::Status::NewerVersion);
    b[5] = 0;
    CHECK_STATUS(open_bytes(r, b), lc::Status::OldVersion);
    b.resize(10);  // (a newer version says so even when too short for this version's header)
    b[4] = 9;
    CHECK_STATUS(open_bytes(r, b), lc::Status::NewerVersion);
    // a flag this build does not know
    b = image();
    b[7] = 0x80;
    refresh_crc(b);
    CHECK_STATUS(open_bytes(r, b), lc::Status::UnknownFlags);
    // the header's CRC
    b = image();
    b[0x0c] ^= 0x01;  // (the serial)
    CHECK_STATUS(open_bytes(r, b), lc::Status::HeaderCrc);
    b = image();
    b[0x40 + 0x04] ^= 0x01;  // (a chunk's version in the table)
    CHECK_STATUS(open_bytes(r, b), lc::Status::HeaderCrc);
    // a chunk count that would read past the end, or allocate without bound
    b = image();
    put32(b, 0x30, 0xffffffffu);
    CHECK_STATUS(open_bytes(r, b), lc::Status::BadTable);
    put32(b, 0x30, 6);  // (64 + 120 bytes > 131)
    CHECK_STATUS(open_bytes(r, b), lc::Status::BadTable);
    b = image();
    put32(b, 0x30, 3);  // (a third entry read from the padding and the chunks: its CRC made right)
    refresh_crc(b);
    CHECK_STATUS(open_bytes(r, b), lc::Status::BadTable);
    // the chunk table pointing past the end (each with the CRC made right, so that this check is the one reached)
    b = image();
    put32(b, 0x54 + 0x0c, 0x10);  // (CLAS: 16 bytes at 0x80 in a 131-byte file)
    refresh_crc(b);
    CHECK_STATUS(open_bytes(r, b), lc::Status::ChunkPastEnd);
    b = image();
    put32(b, 0x54 + 0x08, 0xfffffff0u);  // (an offset whose end would wrap a u32)
    put32(b, 0x54 + 0x0c, 0x20);
    refresh_crc(b);
    CHECK_STATUS(open_bytes(r, b), lc::Status::ChunkPastEnd);
    b = image();
    put32(b, 0x54 + 0x0c, 0xffffffffu);
    refresh_crc(b);
    CHECK_STATUS(open_bytes(r, b), lc::Status::ChunkPastEnd);
    // alignment, order, overlap, duplicates
    b = image();
    put32(b, 0x54 + 0x08, 0x81);
    refresh_crc(b);
    CHECK_STATUS(open_bytes(r, b), lc::Status::BadTable);
    b = image();
    put32(b, 0x54 + 0x08, 0x70);  // (CLAS over TABL)
    refresh_crc(b);
    CHECK_STATUS(open_bytes(r, b), lc::Status::BadTable);
    b = image();
    put32(b, 0x40 + 0x08, 0x60);  // (TABL over the table)
    refresh_crc(b);
    CHECK_STATUS(open_bytes(r, b), lc::Status::BadTable);
    b = image();
    put32(b, 0x54, lc::fourcc("TABL"));
    refresh_crc(b);
    CHECK_STATUS(open_bytes(r, b), lc::Status::BadTable);
    // reserved bytes and the serial's padding
    const size_t reserved[] = {0x19, 0x1b, 0x38, 0x3b, 0x40 + 0x06, 0x54 + 0x07, 0x08 + 12};
    for (size_t at : reserved) {
        b = image();
        b[at] = 0x01;
        refresh_crc(b);
        CHECK_STATUS(open_bytes(r, b), lc::Status::BadTable);
    }
    // a chunk's bytes against its CRC: the file opens (the header is right), the chunk does not read
    b = image();
    b[0x72] ^= 0x10;
    CHECK_STATUS(open_bytes(r, b), lc::Status::Ok);
    uint8_t buf[8] = {};
    std::vector<uint8_t> v{1, 2, 3};
    std::span<const uint8_t> s;
    if (r.chunks().size() == 2) {
        CHECK_STATUS(r.read(r.chunks()[0], buf, sizeof buf), lc::Status::ChunkCrc);
        CHECK_STATUS(r.read(r.chunks()[0], v), lc::Status::ChunkCrc);
        CHECK(v.empty());
        CHECK_STATUS(r.view(r.chunks()[0], s), lc::Status::ChunkCrc);
        CHECK(s.empty());
        CHECK_STATUS(r.read(r.chunks()[1], buf, sizeof buf), lc::Status::Ok);  // (the other chunk is fine)
        // the caller's buffer too small (not a reason to rebuild)
        lc::Result small = r.read(r.chunks()[1], buf, 2);
        CHECK(small.status == lc::Status::BufferTooSmall && !small.rebuild());
    } else {
        CHECK(false);
    }
    // the stamp: another disc, another region, other sources
    CHECK_STATUS(r.open_memory(std::span<const uint8_t>(kImage, sizeof kImage)), lc::Status::Ok);
    lc::Stamp other = image_stamp();
    other.serial = "TEST-00002";
    CHECK_STATUS(r.check_stamp(other), lc::Status::OtherDisc);
    other = image_stamp();
    other.region = lc::Region::NtscU;
    CHECK_STATUS(r.check_stamp(other), lc::Status::OtherDisc);
    other = image_stamp();
    other.fingerprint[19] ^= 1;
    CHECK_STATUS(r.check_stamp(other), lc::Status::OtherSources);
    // chunks the caller needs
    const lc::Chunk* p = r.chunks().data();
    CHECK_STATUS(r.require(lc::fourcc("TABL"), 0x0101, p), lc::Status::ChunkVersion);
    CHECK(p == nullptr);
    CHECK_STATUS(r.require(lc::fourcc("ZONE"), 1, p), lc::Status::NoChunk);
    CHECK(p == nullptr && r.find(lc::fourcc("ZONE")) == nullptr);
    // a reason to rebuild that names the file and the chunk
    const lc::Result miss = r.require(lc::fourcc("ZONE"), 1, p);
    CHECK(miss.rebuild() && miss.why.rfind("(memory): ", 0) == 0 && miss.why.find("ZONE") != std::string::npos);
    // a failed open leaves the reader closed
    b = image();
    b[0] = 0;
    CHECK(!open_bytes(r, b) && !r.is_open() && r.chunks().empty());
    CHECK_STATUS(r.read(lc::Chunk{}, buf, sizeof buf), lc::Status::BadArgument);
}

// every shorter file, and every byte changed: a clean failure each time, never a crash or a read past the bytes (the
// image is copied into a buffer of its own size, so a sanitizer sees an over-read); a change outside the padding is
// always caught
void test_every_cut_and_flip() {
    lc::Reader r;
    int caught = 0, tried = 0;
    for (size_t n = 0; n < sizeof kImage; ++n) {
        std::vector<uint8_t> b(kImage, kImage + n);
        const lc::Result res = open_bytes(r, b);
        CHECK(!res && res.rebuild() && !res.why.empty());
    }
    auto padding = [](size_t at) { return (at >= 0x68 && at < 0x70) || (at >= 0x75 && at < 0x80); };
    const uint8_t masks[] = {0x01, 0x80, 0xff};
    for (size_t at = 0; at < sizeof kImage; ++at) {
        for (uint8_t m : masks) {
            std::vector<uint8_t> b = image();
            b[at] ^= m;
            ++tried;
            bool failed = !open_bytes(r, b) || !r.check_stamp(image_stamp());
            uint8_t buf[16];
            for (size_t k = 0; !failed && k < r.chunks().size(); ++k) failed = !r.read(r.chunks()[k], buf, sizeof buf);
            if (failed) ++caught;
            if (!padding(at)) CHECK(failed);
        }
    }
    std::printf("  %d of %d single-byte changes caught (the rest in the padding, which no CRC covers)\n", caught, tried);
}

void test_files(const std::filesystem::path& base) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path dir = base / ("level_cache_test_" +
                                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const fs::path cache = dir / "level_cache";  // (not there yet: write() makes it)
    CHECK(lc::file_path(cache, 3) == cache / "planet03.r1lc");
    CHECK(lc::file_path(cache, 18) == cache / "planet18.r1lc");
    CHECK(lc::file_path(cache, lc::kBoot) == cache / "boot.r1lc");
    const fs::path path = lc::file_path(cache, 3);

    lc::Reader r;
    CHECK_STATUS(r.open(path), lc::Status::Missing);

    lc::Writer w(image_stamp());
    CHECK_STATUS(w.add(lc::fourcc("TABL"), 0x0102, kTabl), lc::Status::Ok);
    CHECK_STATUS(w.add(lc::fourcc("CLAS"), 3, kClas), lc::Status::Ok);
    CHECK_STATUS(w.write(path), lc::Status::Ok);
    CHECK(fs::file_size(path, ec) == sizeof kImage);
    fs::path tmp = path;
    tmp += ".tmp";
    CHECK(!fs::exists(tmp));

    CHECK_STATUS(r.open(path), lc::Status::Ok);
    CHECK_STATUS(r.check_stamp(image_stamp()), lc::Status::Ok);
    const lc::Chunk* t = nullptr;
    CHECK_STATUS(r.require(lc::fourcc("TABL"), 0x0102, t), lc::Status::Ok);
    uint8_t buf[5] = {};
    if (t != nullptr) {
        CHECK_STATUS(r.read(*t, buf, sizeof buf), lc::Status::Ok);  // (into the caller's buffer, exactly its size)
        CHECK(std::memcmp(buf, kTabl, sizeof kTabl) == 0);
        CHECK_STATUS(r.read(*t, buf, 4), lc::Status::BufferTooSmall);
        std::span<const uint8_t> s;
        CHECK_STATUS(r.view(*t, s), lc::Status::BadArgument);  // (a file read through its handle: no view)
    }
    std::vector<uint8_t> v;
    if (r.chunks().size() == 2) CHECK_STATUS(r.read(r.chunks()[1], v), lc::Status::Ok);
    CHECK(v.size() == 3 && v[1] == 0xbb);

    // a reader moved keeps the file open; the one moved from is closed
    lc::Reader moved = std::move(r);
    CHECK(moved.is_open() && !r.is_open() && moved.chunks().size() == 2);
    moved.close();

    // replaced by a new file (the rename over an existing one); an empty chunk and a bigger one
    std::vector<uint8_t> big(1000);
    for (size_t i = 0; i < big.size(); ++i) big[i] = static_cast<uint8_t>(i * 7);
    lc::Writer w2(image_stamp());
    CHECK_STATUS(w2.add(lc::fourcc("EMPT"), 1, {}), lc::Status::Ok);
    CHECK_STATUS(w2.add(lc::fourcc("BIGC"), 1, big), lc::Status::Ok);
    CHECK_STATUS(w2.write(path), lc::Status::Ok);
    CHECK(!fs::exists(tmp));
    CHECK_STATUS(r.open(path), lc::Status::Ok);
    CHECK(r.chunks().size() == 2 && r.find(lc::fourcc("TABL")) == nullptr);
    if (r.chunks().size() == 2) {
        CHECK_STATUS(r.read(r.chunks()[0], v), lc::Status::Ok);
        CHECK(v.empty());
        CHECK_STATUS(r.read(r.chunks()[1], v), lc::Status::Ok);
        CHECK(v == big);
    }
    r.close();

    // a file cut short on disk, a file that is a folder, and a folder that cannot be made (a file in its place)
    std::vector<uint8_t> cut(kImage, kImage + 70);
    const fs::path short_path = lc::file_path(cache, 4);
    if (std::FILE* f = std::fopen(short_path.string().c_str(), "wb")) {
        std::fwrite(cut.data(), 1, cut.size(), f);
        std::fclose(f);
    }
    CHECK_STATUS(r.open(short_path), lc::Status::Truncated);
    CHECK(!r.is_open());
    fs::create_directories(lc::file_path(cache, 5), ec);
    CHECK_STATUS(r.open(lc::file_path(cache, 5)), lc::Status::Unreadable);
    CHECK_STATUS(w.write(short_path / "planet00.r1lc"), lc::Status::WriteFailed);

    fs::remove_all(dir, ec);
    CHECK(!ec);
}

}  // namespace

int main(int argc, char** argv) {
    const auto t0 = std::chrono::steady_clock::now();
    std::error_code ec;
    const std::filesystem::path base = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::temp_directory_path(ec);
    struct Test {
        const char* name;
        void (*run)();
    };
    const Test tests[] = {
        {"byte order", test_byte_order},
        {"crc32", test_crc32},
        {"sha1", test_sha1},
        {"source fingerprint", test_source_fingerprint},
        {"decode a hand-built file", test_decode_hand_built},
        {"encode the same file", test_encode_matches},
        {"writer limits", test_writer_limits},
        {"rejected files", test_rejects},
        {"every cut and every byte changed", test_every_cut_and_flip},
    };
    for (const Test& t : tests) {
        const int before = g_failed;
        t.run();
        std::printf("%s %s\n", g_failed == before ? "ok  " : "FAIL", t.name);
    }
    {
        const int before = g_failed;
        test_files(base);
        std::printf("%s files (in %s)\n", g_failed == before ? "ok  " : "FAIL", base.string().c_str());
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::printf("level_cache_test: %d checks, %d failed (%.1f ms)\n", g_checks, g_failed, ms);
    return g_failed == 0 ? 0 : 1;
}
