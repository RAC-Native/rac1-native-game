// See level_cache.h (the format), docs/formats/README.md ("The level cache").
#include <formats/level_cache.h>

#include <platform/platform.h>

#include <algorithm>
#include <cstring>
#include <system_error>

namespace rac1::level_cache {

namespace {

constexpr uint64_t align_up(uint64_t v) { return (v + (kAlign - 1)) & ~static_cast<uint64_t>(kAlign - 1); }

std::string path_text(const std::filesystem::path& p) {
    const std::u8string s = p.u8string();  // (UTF-8 on every system; path::string() can throw on Windows)
    return std::string(s.begin(), s.end());
}

std::string num(uint64_t v) { return std::to_string(v); }

Result failure(Status s, const std::string& name, const std::string& text) { return Result{s, name + ": " + text}; }

const std::array<uint32_t, 256> kCrcTable = [] {
    std::array<uint32_t, 256> t{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
        t[i] = c;
    }
    return t;
}();

uint32_t rd_be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) |
           static_cast<uint32_t>(p[3]);
}
uint32_t rotl(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

}  // namespace

// orig: (host) see level_cache.h
std::string fourcc_name(uint32_t id) {
    std::string s(4, '?');
    for (int i = 0; i < 4; ++i) {
        const auto c = static_cast<uint8_t>(id >> (8 * i));
        if (c >= 0x20 && c < 0x7f) s[static_cast<size_t>(i)] = static_cast<char>(c);
    }
    return s;
}

// orig: (host) see level_cache.h
const char* status_name(Status s) {
    switch (s) {
        case Status::Ok: return "Ok";
        case Status::Missing: return "Missing";
        case Status::Unreadable: return "Unreadable";
        case Status::Truncated: return "Truncated";
        case Status::BadMagic: return "BadMagic";
        case Status::OldVersion: return "OldVersion";
        case Status::NewerVersion: return "NewerVersion";
        case Status::UnknownFlags: return "UnknownFlags";
        case Status::BadTable: return "BadTable";
        case Status::ChunkPastEnd: return "ChunkPastEnd";
        case Status::HeaderCrc: return "HeaderCrc";
        case Status::ChunkCrc: return "ChunkCrc";
        case Status::OtherDisc: return "OtherDisc";
        case Status::OtherSources: return "OtherSources";
        case Status::NoChunk: return "NoChunk";
        case Status::ChunkVersion: return "ChunkVersion";
        case Status::BufferTooSmall: return "BufferTooSmall";
        case Status::BadArgument: return "BadArgument";
        case Status::WriteFailed: return "WriteFailed";
    }
    return "?";
}

// orig: (host) see level_cache.h
std::filesystem::path file_path(const std::filesystem::path& dir, int level) {
    if (level == kBoot) return dir / "boot.r1lc";
    char name[32];
    std::snprintf(name, sizeof name, "planet%02d.r1lc", level);
    return dir / name;
}

// ---- Checksums ----

// orig: (host) see level_cache.h
uint32_t crc32(uint32_t crc, const void* data, size_t n) {
    const auto* p = static_cast<const uint8_t*>(data);
    crc = ~crc;
    for (size_t i = 0; i < n; ++i) crc = kCrcTable[(crc ^ p[i]) & 0xffu] ^ (crc >> 8);
    return ~crc;
}

// orig: (host) see level_cache.h
void Sha1::reset() {
    h_[0] = 0x67452301u;
    h_[1] = 0xefcdab89u;
    h_[2] = 0x98badcfeu;
    h_[3] = 0x10325476u;
    h_[4] = 0xc3d2e1f0u;
    used_ = 0;
    bytes_ = 0;
}

// orig: (host) one 64-byte block (FIPS 180-4, 6.1.2; the words read big-endian byte by byte)
void Sha1::block(const uint8_t* p) {
    uint32_t w[80];
    for (int t = 0; t < 16; ++t) w[t] = rd_be32(p + 4 * t);
    for (int t = 16; t < 80; ++t) w[t] = rotl(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
    uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4];
    for (int t = 0; t < 80; ++t) {
        uint32_t f, k;
        if (t < 20) {
            f = (b & c) | (~b & d);
            k = 0x5a827999u;
        } else if (t < 40) {
            f = b ^ c ^ d;
            k = 0x6ed9eba1u;
        } else if (t < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8f1bbcdcu;
        } else {
            f = b ^ c ^ d;
            k = 0xca62c1d6u;
        }
        const uint32_t tmp = rotl(a, 5) + f + e + k + w[t];
        e = d;
        d = c;
        c = rotl(b, 30);
        b = a;
        a = tmp;
    }
    h_[0] += a;
    h_[1] += b;
    h_[2] += c;
    h_[3] += d;
    h_[4] += e;
}

// orig: (host) see level_cache.h
void Sha1::update(const void* data, size_t n) {
    const auto* p = static_cast<const uint8_t*>(data);
    bytes_ += n;
    if (used_ > 0) {
        const size_t take = std::min(n, sizeof buf_ - used_);
        std::memcpy(buf_ + used_, p, take);
        used_ += take;
        p += take;
        n -= take;
        if (used_ < sizeof buf_) return;
        block(buf_);
        used_ = 0;
    }
    for (; n >= 64; p += 64, n -= 64) block(p);
    if (n > 0) {
        std::memcpy(buf_, p, n);
        used_ = n;
    }
}

// orig: (host) see level_cache.h
Fingerprint Sha1::finish() {
    const uint64_t bits = bytes_ * 8;
    uint8_t pad[72] = {0x80};
    const size_t pad_len = (used_ < 56 ? 56 - used_ : 120 - used_);
    for (int i = 0; i < 8; ++i) pad[pad_len + static_cast<size_t>(i)] = static_cast<uint8_t>(bits >> (56 - 8 * i));
    update(pad, pad_len + 8);
    Fingerprint out{};
    for (size_t i = 0; i < 5; ++i)
        for (size_t k = 0; k < 4; ++k) out[i * 4 + k] = static_cast<uint8_t>(h_[i] >> (24 - 8 * k));
    reset();
    return out;
}

// orig: (host) see level_cache.h
bool SourceFingerprint::close_source() {
    if (open_ && got_ != expected_) ok_ = false;
    open_ = false;
    return ok_;
}

// orig: (host) see level_cache.h
void SourceFingerprint::begin(std::string_view name, uint64_t size) {
    close_source();
    uint8_t frame[8];
    wr_le32(frame, static_cast<uint32_t>(name.size()));
    sha_.update(frame, 4);
    sha_.update(name.data(), name.size());
    wr_le64(frame, size);
    sha_.update(frame, 8);
    expected_ = size;
    got_ = 0;
    open_ = true;
}

// orig: (host) see level_cache.h
void SourceFingerprint::add(const void* data, size_t n) {
    if (!open_) {
        ok_ = false;  // (bytes of no source)
        return;
    }
    got_ += n;
    sha_.update(data, n);
}

// orig: (host) see level_cache.h
bool SourceFingerprint::finish(Fingerprint& out) {
    const bool ok = close_source();
    const Fingerprint digest = sha_.finish();
    ok_ = true;
    if (!ok) return false;
    out = digest;
    return true;
}

// ---- Writing ----

// orig: (host) see level_cache.h
Result Writer::add(uint32_t id, uint16_t version, std::span<const uint8_t> data) {
    const std::string name = "level cache chunk " + fourcc_name(id);
    for (const Chunk& c : chunks_)
        if (c.id == id) return failure(Status::BadArgument, name, "added twice");
    if (chunks_.size() >= kMaxChunks) return failure(Status::BadArgument, name, "more than " + num(kMaxChunks) + " chunks");
    const uint64_t at = align_up(body_.size());
    // (the file's size is a u32: the header, the table, the padding and the chunks all below 4 GB)
    if (at + data.size() + kHeaderSize + kEntrySize * kMaxChunks + kAlign > 0xffffffffull)
        return failure(Status::BadArgument, name, "the file would be past 4 GB");
    Chunk c;
    c.id = id;
    c.version = version;
    c.offset = static_cast<uint32_t>(at);
    c.size = static_cast<uint32_t>(data.size());
    c.crc = crc32(0, data.data(), data.size());
    body_.resize(static_cast<size_t>(at), 0);
    body_.insert(body_.end(), data.begin(), data.end());
    chunks_.push_back(c);
    return {};
}

// orig: (host) see level_cache.h
Result Writer::build(std::vector<uint8_t>& out) const {
    if (stamp_.serial.size() > kSerialSize)
        return failure(Status::BadArgument, "level cache", "the serial \"" + stamp_.serial + "\" is longer than 16 characters");
    const size_t table = kEntrySize * chunks_.size();
    const auto data_start = static_cast<size_t>(align_up(kHeaderSize + table));
    const size_t total = data_start + body_.size();
    out.assign(total, 0);
    uint8_t* h = out.data();
    wr_le32(h + 0x00, kMagic);
    wr_le16(h + 0x04, kFormatVersion);
    wr_le16(h + 0x06, flags_);
    std::memcpy(h + 0x08, stamp_.serial.data(), stamp_.serial.size());
    h[0x18] = static_cast<uint8_t>(stamp_.region);
    std::memcpy(h + 0x1c, stamp_.fingerprint.data(), kFingerprintSize);  // (bytes: no byte order)
    wr_le32(h + 0x30, static_cast<uint32_t>(chunks_.size()));
    wr_le32(h + 0x34, static_cast<uint32_t>(total));
    for (size_t i = 0; i < chunks_.size(); ++i) {
        const Chunk& c = chunks_[i];
        uint8_t* e = h + kHeaderSize + kEntrySize * i;
        wr_le32(e + 0x00, c.id);
        wr_le16(e + 0x04, c.version);
        wr_le32(e + 0x08, static_cast<uint32_t>(data_start + c.offset));
        wr_le32(e + 0x0c, c.size);
        wr_le32(e + 0x10, c.crc);
    }
    if (!body_.empty()) std::memcpy(h + data_start, body_.data(), body_.size());
    wr_le32(h + 0x3c, crc32(crc32(0, h, 0x3c), h + kHeaderSize, table));
    return {};
}

// orig: (host) see level_cache.h
Result Writer::write(const std::filesystem::path& path) const {
    const std::string name = path_text(path);
    std::vector<uint8_t> image;
    if (Result r = build(image); !r) return r;
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) return failure(Status::WriteFailed, name, "its folder cannot be made (" + ec.message() + ")");
    }
    if (!platform::write_file_replace(path, image.data(), image.size()))
        return failure(Status::WriteFailed, name, "cannot be written");
    return {};
}

// ---- Reading ----

// orig: (host) see level_cache.h
Reader& Reader::operator=(Reader&& o) noexcept {
    if (this != &o) {
        close();
        file_ = o.file_;
        image_ = o.image_;
        size_ = o.size_;
        name_ = std::move(o.name_);
        version_ = o.version_;
        flags_ = o.flags_;
        stamp_ = std::move(o.stamp_);
        chunks_ = std::move(o.chunks_);
        o.file_ = nullptr;
        o.image_ = {};
        o.size_ = 0;
        o.chunks_.clear();
    }
    return *this;
}

// orig: (host) see level_cache.h
void Reader::close() {
    if (file_ != nullptr) std::fclose(file_);
    file_ = nullptr;
    image_ = {};
    size_ = 0;
    version_ = flags_ = 0;
    stamp_ = Stamp{};
    chunks_.clear();
}

// orig: (host)
Result Reader::fail(Status s, const std::string& text) const { return failure(s, name_, text); }

// orig: (host) `n` bytes at `offset` (the caller has checked them against the file's size); false on a read error or a
// short file
bool Reader::fetch(uint64_t offset, void* dst, size_t n) const {
    if (n == 0) return true;
    if (offset > size_ || n > size_ - offset) return false;
    if (image_.data() != nullptr) {
        std::memcpy(dst, image_.data() + offset, n);
        return true;
    }
    if (file_ == nullptr) return false;
    auto* p = static_cast<uint8_t*>(dst);
    while (n > 0) {
        const int64_t got = platform::read_at(file_, offset, p, n);
        if (got <= 0) return false;
        p += got;
        offset += static_cast<uint64_t>(got);
        n -= static_cast<size_t>(got);
    }
    return true;
}

// orig: (host) see level_cache.h
Result Reader::open(const std::filesystem::path& path) {
    close();
    name_ = path_text(path);
    std::error_code ec;
    const auto st = std::filesystem::status(path, ec);
    if (st.type() == std::filesystem::file_type::not_found) return fail(Status::Missing, "there is no such file");
    if (ec || !std::filesystem::is_regular_file(st)) return fail(Status::Unreadable, "not a file that can be read");
    const uint64_t size = std::filesystem::file_size(path, ec);
    if (ec) return fail(Status::Unreadable, "its size cannot be read (" + ec.message() + ")");
    file_ = platform::open_file(path, "rb");
    if (file_ == nullptr) return fail(Status::Unreadable, "cannot be opened");
    size_ = size;
    Result r = parse();
    if (!r) {
        const std::string keep = name_;
        close();
        name_ = keep;
    }
    return r;
}

// orig: (host) see level_cache.h
Result Reader::open_memory(std::span<const uint8_t> image, std::string name) {
    close();
    name_ = std::move(name);
    if (image.data() == nullptr && !image.empty()) return fail(Status::BadArgument, "no bytes");
    static const uint8_t kEmpty = 0;
    image_ = image.data() != nullptr ? image : std::span<const uint8_t>(&kEmpty, 0);
    size_ = image.size();
    Result r = parse();
    if (!r) {
        const std::string keep = name_;
        close();
        name_ = keep;
    }
    return r;
}

// orig: (host) the header and the chunk table read and checked (the order of the checks: the most telling reason first)
Result Reader::parse() {
    uint8_t h[kHeaderSize] = {};
    const size_t head = static_cast<size_t>(std::min<uint64_t>(size_, kHeaderSize));
    if (size_ < 4) return fail(Status::Truncated, num(size_) + " bytes: not even a magic");
    if (!fetch(0, h, head)) return fail(Status::Unreadable, "its header cannot be read");
    if (rd_le32(h) != kMagic) return fail(Status::BadMagic, "not a level cache file (no \"R1LC\" magic)");
    if (size_ < 8) return fail(Status::Truncated, num(size_) + " bytes: not even a format version");
    const uint16_t version = rd_le16(h + 0x04);
    if (version < kFormatVersion)
        return fail(Status::OldVersion, "format version " + num(version) + ", this build reads " + num(kFormatVersion));
    if (version > kFormatVersion)
        return fail(Status::NewerVersion,
                    "format version " + num(version) + " (made by a newer build), this build reads " + num(kFormatVersion));
    if (size_ < kHeaderSize) return fail(Status::Truncated, num(size_) + " bytes, shorter than the 64-byte header");
    const uint16_t flags = rd_le16(h + 0x06);
    if ((flags & ~kKnownFlags) != 0) return fail(Status::UnknownFlags, "flags " + num(flags) + " this build does not know");
    const uint32_t count = rd_le32(h + 0x30), stated = rd_le32(h + 0x34);
    if (stated > size_)
        return fail(Status::Truncated, num(size_) + " bytes, the header says " + num(stated));
    if (stated < size_)
        return fail(Status::BadTable, num(size_) + " bytes, the header says " + num(stated) + " (bytes past its end)");
    if (count > kMaxChunks) return fail(Status::BadTable, num(count) + " chunks, more than " + num(kMaxChunks));
    const size_t table = kEntrySize * count;
    if (kHeaderSize + table > size_)
        return fail(Status::BadTable, "its chunk table (" + num(count) + " chunks) runs past the file's end");
    std::vector<uint8_t> t(table);
    if (!fetch(kHeaderSize, t.data(), table)) return fail(Status::Unreadable, "its chunk table cannot be read");
    if (crc32(crc32(0, h, 0x3c), t.data(), table) != rd_le32(h + 0x3c))
        return fail(Status::HeaderCrc, "its header or chunk table does not match their CRC");
    if (h[0x19] != 0 || h[0x1a] != 0 || h[0x1b] != 0 || rd_le32(h + 0x38) != 0)
        return fail(Status::BadTable, "reserved header bytes are not zero");

    Stamp stamp;
    size_t len = 0;
    while (len < kSerialSize && h[0x08 + len] != 0) ++len;
    for (size_t i = len; i < kSerialSize; ++i)
        if (h[0x08 + i] != 0) return fail(Status::BadTable, "the serial's padding is not zero");
    stamp.serial.assign(reinterpret_cast<const char*>(h + 0x08), len);
    stamp.region = static_cast<Region>(h[0x18]);
    std::memcpy(stamp.fingerprint.data(), h + 0x1c, kFingerprintSize);

    std::vector<Chunk> chunks(count);
    uint64_t end = align_up(kHeaderSize + table);  // (the first chunk's earliest place)
    for (size_t i = 0; i < count; ++i) {
        const uint8_t* e = t.data() + kEntrySize * i;
        Chunk& c = chunks[i];
        c.id = rd_le32(e + 0x00);
        c.version = rd_le16(e + 0x04);
        c.offset = rd_le32(e + 0x08);
        c.size = rd_le32(e + 0x0c);
        c.crc = rd_le32(e + 0x10);
        const std::string which = "chunk " + num(i) + " (" + fourcc_name(c.id) + ")";
        if (rd_le16(e + 0x06) != 0) return fail(Status::BadTable, which + ": reserved bytes are not zero");
        if (c.offset % kAlign != 0) return fail(Status::BadTable, which + ": at " + num(c.offset) + ", not a multiple of 16");
        if (c.offset < end)
            return fail(Status::BadTable, which + ": at " + num(c.offset) + ", over the table or the chunk before it");
        if (static_cast<uint64_t>(c.offset) + c.size > size_)
            return fail(Status::ChunkPastEnd, which + ": " + num(c.size) + " bytes at " + num(c.offset) + " run past the file's " +
                                                  num(size_));
        end = static_cast<uint64_t>(c.offset) + c.size;
        for (size_t k = 0; k < i; ++k)
            if (chunks[k].id == c.id) return fail(Status::BadTable, which + ": its id twice");
    }
    version_ = version;
    flags_ = flags;
    stamp_ = std::move(stamp);
    chunks_ = std::move(chunks);
    return {};
}

// orig: (host) see level_cache.h
Result Reader::check_stamp(const Stamp& expected) const {
    if (stamp_.serial != expected.serial)
        return fail(Status::OtherDisc, "made from the disc " + stamp_.serial + ", this one is " + expected.serial);
    if (stamp_.region != expected.region)
        return fail(Status::OtherDisc, "made for region " + num(static_cast<uint8_t>(stamp_.region)) + ", this disc's is " +
                                           num(static_cast<uint8_t>(expected.region)));
    if (stamp_.fingerprint != expected.fingerprint) return fail(Status::OtherSources, "made from other source files");
    return {};
}

// orig: (host) see level_cache.h
const Chunk* Reader::find(uint32_t id) const {
    for (const Chunk& c : chunks_)
        if (c.id == id) return &c;
    return nullptr;
}

// orig: (host) see level_cache.h
Result Reader::require(uint32_t id, uint16_t version, const Chunk*& out) const {
    out = find(id);
    if (out == nullptr) return fail(Status::NoChunk, "no chunk " + fourcc_name(id));
    if (out->version != version) {
        const uint16_t had = out->version;
        out = nullptr;
        return fail(Status::ChunkVersion, "chunk " + fourcc_name(id) + " is version " + num(had) + ", this build reads " + num(version));
    }
    return {};
}

// orig: (host) see level_cache.h
Result Reader::read(const Chunk& c, void* dst, size_t capacity) const {
    const std::string which = "chunk " + fourcc_name(c.id);
    if (!is_open()) return fail(Status::BadArgument, "not open");
    if (static_cast<uint64_t>(c.offset) + c.size > size_) return fail(Status::ChunkPastEnd, which + ": past the file's end");
    if (capacity < c.size)
        return fail(Status::BufferTooSmall, which + ": " + num(c.size) + " bytes, the buffer holds " + num(capacity));
    if (c.size > 0 && dst == nullptr) return fail(Status::BadArgument, which + ": no buffer");
    if (!fetch(c.offset, dst, c.size)) return fail(Status::Unreadable, which + ": cannot be read");
    if (crc32(0, dst, c.size) != c.crc) return fail(Status::ChunkCrc, which + ": its bytes do not match their CRC");
    return {};
}

// orig: (host) see level_cache.h
Result Reader::read(const Chunk& c, std::vector<uint8_t>& out) const {
    if (!is_open()) return fail(Status::BadArgument, "not open");
    if (static_cast<uint64_t>(c.offset) + c.size > size_)
        return fail(Status::ChunkPastEnd, "chunk " + fourcc_name(c.id) + ": past the file's end");
    out.resize(c.size);
    Result r = read(c, out.data(), out.size());
    if (!r) out.clear();
    return r;
}

// orig: (host) see level_cache.h
Result Reader::view(const Chunk& c, std::span<const uint8_t>& out) const {
    const std::string which = "chunk " + fourcc_name(c.id);
    out = {};
    if (image_.data() == nullptr) return fail(Status::BadArgument, which + ": view() needs a file opened from memory");
    if (static_cast<uint64_t>(c.offset) + c.size > size_) return fail(Status::ChunkPastEnd, which + ": past the file's end");
    const std::span<const uint8_t> s = image_.subspan(c.offset, c.size);
    if (crc32(0, s.data(), s.size()) != c.crc) return fail(Status::ChunkCrc, which + ": its bytes do not match their CRC");
    out = s;
    return {};
}

}  // namespace rac1::level_cache
