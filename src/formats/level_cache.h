// The level cache's container: the format of every file under level_cache/ (docs/formats/README.md, "The level cache").
// Host data the port makes from the player's own disc (tables and descriptors read out of a level's files once, then
// loaded from here), one file per planet: <dir>/planetNN.r1lc, <dir>/boot.r1lc for the boot module. Nothing in it comes
// with the game's data; it is never in the repository.
//
// The file, little-endian throughout (every field written and read byte by byte: the same file on every host):
//   header, 64 bytes
//     +0x00  char[4]  magic "R1LC"
//     +0x04  u16      format version (kFormatVersion: 1)
//     +0x06  u16      flags (none defined in version 1: 0)
//     +0x08  char[16] the disc's serial ("SCUS-97199"), ASCII, padded with zeros
//     +0x18  u8       the disc's region (Region)
//     +0x19  u8[3]    zero
//     +0x1c  u8[20]   the sources' fingerprint (SourceFingerprint: SHA-1 of the disc files the chunks were made from)
//     +0x30  u32      the chunk count (at most kMaxChunks)
//     +0x34  u32      the file's size in bytes
//     +0x38  u32      zero
//     +0x3c  u32      CRC-32 of bytes 0x00..0x3b, then of the whole chunk table
//   chunk table, 20 bytes per chunk, from +0x40
//     +0x00  u32      id: four characters, the first in the low byte (fourcc("TABL"): the bytes read "TABL" in a dump)
//     +0x04  u16      the chunk's own version (its maker's; the container does not interpret it)
//     +0x06  u16      zero
//     +0x08  u32      offset in the file (a multiple of 16)
//     +0x0c  u32      size in bytes
//     +0x10  u32      CRC-32 of the chunk's bytes
//   then zeros to the next multiple of 16, and the chunks: in table order, each at a multiple of 16, zeros between them;
//   the file ends with the last chunk's last byte. Version 1 is uncompressed.
//
// A file is never patched or upgraded. Whatever does not match (the format version, a flag, a chunk's version, the disc,
// the sources' fingerprint, a CRC, the size) means the file is rebuilt from the disc; the Reader says why. A reader reads
// only what it asks for (the header and the table, then one chunk at a time into its caller's buffer), in file order: no
// mapping needed, and no need to hold the whole file.
//
// No exceptions: every call that can fail returns a Result (its status, and the reason in plain words).
#ifndef RAC1_FORMATS_LEVEL_CACHE_H
#define RAC1_FORMATS_LEVEL_CACHE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rac1::level_cache {

// ---- Little-endian fields (byte by byte: never a host integer copied from file bytes) ----

inline uint16_t rd_le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
inline uint32_t rd_le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}
inline uint64_t rd_le64(const uint8_t* p) { return rd_le32(p) | (static_cast<uint64_t>(rd_le32(p + 4)) << 32); }
inline void wr_le16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
}
inline void wr_le32(uint8_t* p, uint32_t v) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
inline void wr_le64(uint8_t* p, uint64_t v) {
    wr_le32(p, static_cast<uint32_t>(v));
    wr_le32(p + 4, static_cast<uint32_t>(v >> 32));
}

// A chunk id (or the magic) from its four characters: the first character in the low byte
constexpr uint32_t fourcc(const char (&s)[5]) {
    return static_cast<uint32_t>(static_cast<uint8_t>(s[0])) | (static_cast<uint32_t>(static_cast<uint8_t>(s[1])) << 8) |
           (static_cast<uint32_t>(static_cast<uint8_t>(s[2])) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(s[3])) << 24);
}
// "TABL" from fourcc("TABL"); a character that is not printable ASCII as '?'
std::string fourcc_name(uint32_t id);

constexpr uint32_t kMagic = fourcc("R1LC");
constexpr uint16_t kFormatVersion = 1;
constexpr uint16_t kKnownFlags = 0;   // (version 1 defines none: a flag this build does not know means "rebuild")
constexpr size_t kHeaderSize = 64;
constexpr size_t kEntrySize = 20;
constexpr size_t kAlign = 16;
constexpr uint32_t kMaxChunks = 1024;  // (bounds the table a reader allocates: 20 KB)
constexpr size_t kSerialSize = 16;
constexpr size_t kFingerprintSize = 20;
constexpr int kBoot = -1;             // file_path's level for the boot module

// The disc's region, as the header stores it
enum class Region : uint8_t { Unknown = 0, NtscU = 1, Pal = 2, NtscJ = 3 };

using Fingerprint = std::array<uint8_t, kFingerprintSize>;

// What a file was made from: the disc and the bytes of its sources. A reader compares the whole stamp; any difference
// rebuilds the file.
struct Stamp {
    std::string serial;  // the disc's serial, "SCUS-97199" (at most kSerialSize characters)
    Region region = Region::Unknown;
    Fingerprint fingerprint{};
};

enum class Status : uint8_t {
    Ok,
    // the file is to be rebuilt:
    Missing,         // there is no file
    Unreadable,      // it cannot be opened or read
    Truncated,       // shorter than its header (or its table, or a header) says
    BadMagic,        // not a level cache file
    OldVersion,      // an older format version
    NewerVersion,    // a newer format version (made by a newer build)
    UnknownFlags,    // a flag this build does not know
    BadTable,        // the header or the chunk table is inconsistent (order, alignment, duplicates, reserved bytes, size)
    ChunkPastEnd,    // a chunk runs past the file's end
    HeaderCrc,       // the header and table's CRC does not match
    ChunkCrc,        // a chunk's bytes do not match its CRC
    OtherDisc,       // made from another disc (its serial or region)
    OtherSources,    // made from other source files (the fingerprint)
    NoChunk,         // a chunk the caller needs is not there
    ChunkVersion,    // a chunk the caller needs has another version
    // the caller's own mistakes (not the file's):
    BufferTooSmall,  // read(): the caller's buffer is smaller than the chunk
    BadArgument,     // a call that does not apply (a duplicate id to add, view() on a file not in memory ...)
    WriteFailed,     // the file could not be written
};
// The status's name ("Truncated" ...)
const char* status_name(Status s);

struct Result {
    Status status = Status::Ok;
    std::string why;  // empty when Ok; else the file's name and the reason in plain words
    explicit operator bool() const { return status == Status::Ok; }
    // The file has to be made again (every failure but the caller's own mistakes)
    bool rebuild() const { return status != Status::Ok && status < Status::BufferTooSmall; }
};

// One entry of the chunk table, decoded
struct Chunk {
    uint32_t id = 0;
    uint16_t version = 0;
    uint32_t offset = 0;
    uint32_t size = 0;
    uint32_t crc = 0;
};

// The file of a level's cache in `dir` (the caller's: the cache's folder): <dir>/planetNN.r1lc for level NN (0..18),
// <dir>/boot.r1lc for kBoot
std::filesystem::path file_path(const std::filesystem::path& dir, int level);

// ---- Writing ----

// Collects the chunks, then writes the whole file at once: into <path>.tmp beside it, then renamed over <path>
// (platform::write_file_replace, as the saves: a crash or a full disk never leaves half a file). The chunks are kept in
// memory until then (a level's are a few KB).
class Writer {
public:
    explicit Writer(Stamp stamp, uint16_t flags = 0) : stamp_(std::move(stamp)), flags_(flags) {}

    // A chunk, after those added before (the file keeps this order). BadArgument for an id already added, past kMaxChunks
    // chunks, or past 4 GB in all.
    Result add(uint32_t id, uint16_t version, std::span<const uint8_t> data);
    // The file's bytes
    Result build(std::vector<uint8_t>& out) const;
    // The file written (its folder made when missing)
    Result write(const std::filesystem::path& path) const;

private:
    Stamp stamp_;
    uint16_t flags_ = 0;
    std::vector<Chunk> chunks_;  // (offsets from the start of body_)
    std::vector<uint8_t> body_;  // the chunks, each at a multiple of 16
};

// ---- Reading ----

// Reads a file through a handle (platform::read_at: no shared position, so several threads may read() at once), or bytes
// the caller holds in memory (a buffer, or a mapping: io/mapped_file.h). open() reads and checks only the header and the
// chunk table; read() reads one chunk and checks its CRC.
//
// Use: open(), check_stamp() with what the file must have been made from, then require() and read() each chunk needed;
// any result with rebuild() set means: make the file again from the disc.
class Reader {
public:
    Reader() = default;
    ~Reader() { close(); }
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;
    Reader(Reader&& o) noexcept { *this = std::move(o); }
    Reader& operator=(Reader&& o) noexcept;

    Result open(const std::filesystem::path& path);
    // The file's bytes in the caller's memory (kept by reference: they must outlive the reader); `name` for the messages
    Result open_memory(std::span<const uint8_t> image, std::string name = "(memory)");
    void close();
    bool is_open() const { return file_ != nullptr || image_.data() != nullptr; }

    uint16_t format_version() const { return version_; }
    uint16_t flags() const { return flags_; }
    const Stamp& stamp() const { return stamp_; }
    uint64_t file_size() const { return size_; }
    // OtherDisc / OtherSources when the file was not made from `expected`
    Result check_stamp(const Stamp& expected) const;

    std::span<const Chunk> chunks() const { return chunks_; }
    // The chunk with `id`, else null
    const Chunk* find(uint32_t id) const;
    // The chunk with `id` and the version the caller reads: NoChunk / ChunkVersion otherwise
    Result require(uint32_t id, uint16_t version, const Chunk*& out) const;

    // The chunk's bytes into the caller's buffer (`capacity` bytes, BufferTooSmall when less than the chunk), CRC checked
    Result read(const Chunk& c, void* dst, size_t capacity) const;
    // The same into `out`, resized to the chunk (at most the file's size: the table was checked against it)
    Result read(const Chunk& c, std::vector<uint8_t>& out) const;
    // A file opened from memory only: the chunk's bytes where they are (16-byte aligned when the image is), CRC checked
    Result view(const Chunk& c, std::span<const uint8_t>& out) const;

private:
    Result parse();
    bool fetch(uint64_t offset, void* dst, size_t n) const;
    Result fail(Status s, const std::string& text) const;

    std::FILE* file_ = nullptr;
    std::span<const uint8_t> image_;
    uint64_t size_ = 0;
    std::string name_;
    uint16_t version_ = 0, flags_ = 0;
    Stamp stamp_;
    std::vector<Chunk> chunks_;
};

// ---- Checksums ----

// CRC-32 (IEEE 802.3, reflected, polynomial 0xedb88320: the same as zlib's crc32 and Python's zlib.crc32), continued
// from `crc` (0 to start)
uint32_t crc32(uint32_t crc, const void* data, size_t n);

// SHA-1 (FIPS 180-4), fed in pieces
class Sha1 {
public:
    Sha1() { reset(); }
    void reset();
    void update(const void* data, size_t n);
    // The digest; the object then starts again
    Fingerprint finish();

private:
    void block(const uint8_t* p);
    uint32_t h_[5] = {};
    uint8_t buf_[64] = {};
    size_t used_ = 0;
    uint64_t bytes_ = 0;
};

// The header's fingerprint of the files a cache was made from: SHA-1 over each source in turn, framed so that no two
// lists of sources give the same bytes: u32 the name's length, the name (as its maker names it: "SCUS_971.99",
// "level03/data"), u64 the byte count, the bytes (all little-endian). The same in Python:
//   h = hashlib.sha1(); for name, data in sources: h.update(struct.pack("<I", len(name)) + name +
//                                                           struct.pack("<Q", len(data)) + data)
// The bytes are those read through the disc reader (so a mod's replaced file changes the fingerprint), in pieces of
// any size: nothing has to be held whole.
class SourceFingerprint {
public:
    // A source of `size` bytes, given next through add()
    void begin(std::string_view name, uint64_t size);
    void add(const void* data, size_t n);
    // False (and `out` untouched) when a source's bytes did not add up to the size begun with
    bool finish(Fingerprint& out);

private:
    bool close_source();
    Sha1 sha_;
    uint64_t expected_ = 0, got_ = 0;
    bool open_ = false, ok_ = true;
};

}  // namespace rac1::level_cache

#endif
