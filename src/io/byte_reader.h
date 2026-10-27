#ifndef RAC1_IO_BYTE_READER_H
#define RAC1_IO_BYTE_READER_H

#include <span>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace rac1 {

class ByteReader {
public:
    explicit ByteReader(std::span<const std::byte> data) : data_(data), offset_(0) {}
    explicit ByteReader(const std::vector<std::uint8_t>& data) 
        : data_(reinterpret_cast<const std::byte*>(data.data()), data.size()), offset_(0) {}
    ByteReader(const std::byte* data, std::size_t size) : data_(data, size), offset_(0) {}

    [[nodiscard]] std::size_t tell() const noexcept {
        return offset_;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return data_.size();
    }

    [[nodiscard]] std::size_t remaining() const noexcept {
        return data_.size() - offset_;
    }

    void seek(std::size_t pos) {
        if (pos > data_.size()) {
            throw std::out_of_range("ByteReader::seek: offset " + std::to_string(pos) + 
                                    " out of range (size: " + std::to_string(data_.size()) + ")");
        }
        offset_ = pos;
    }

    void skip(std::size_t bytes) {
        if (bytes > data_.size() - offset_) {  // (not offset_ + bytes: a huge count would wrap)
            throw std::out_of_range("ByteReader::skip: skipping " + std::to_string(bytes) + 
                                    " bytes from offset " + std::to_string(offset_) + 
                                    " exceeds size " + std::to_string(data_.size()));
        }
        offset_ += bytes;
    }

    [[nodiscard]] std::span<const std::byte> slice(std::size_t len) {
        if (len > data_.size() - offset_) {
            throw std::out_of_range("ByteReader::slice: requested " + std::to_string(len) + 
                                    " bytes from offset " + std::to_string(offset_) + 
                                    " exceeds size " + std::to_string(data_.size()));
        }
        std::span<const std::byte> sub = data_.subspan(offset_, len);
        offset_ += len;
        return sub;
    }

    std::uint8_t u8() {
        if (offset_ + 1 > data_.size()) {
            throw std::out_of_range("ByteReader::u8: out of bounds read");
        }
        return static_cast<std::uint8_t>(data_[offset_++]);
    }

    std::uint16_t u16_le() {
        if (offset_ + 2 > data_.size()) {
            throw std::out_of_range("ByteReader::u16_le: out of bounds read");
        }
        std::uint16_t val = static_cast<std::uint16_t>(static_cast<std::uint8_t>(data_[offset_])) |
                            (static_cast<std::uint16_t>(static_cast<std::uint8_t>(data_[offset_ + 1])) << 8);
        offset_ += 2;
        return val;
    }

    std::uint32_t u32_le() {
        if (offset_ + 4 > data_.size()) {
            throw std::out_of_range("ByteReader::u32_le: out of bounds read");
        }
        std::uint32_t val = static_cast<std::uint32_t>(static_cast<std::uint8_t>(data_[offset_])) |
                            (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data_[offset_ + 1])) << 8) |
                            (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data_[offset_ + 2])) << 16) |
                            (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data_[offset_ + 3])) << 24);
        offset_ += 4;
        return val;
    }

    std::int32_t s32_le() {
        return static_cast<std::int32_t>(u32_le());
    }

    std::uint64_t u64_le() {
        if (offset_ + 8 > data_.size()) {
            throw std::out_of_range("ByteReader::u64_le: out of bounds read");
        }
        std::uint64_t val = static_cast<std::uint64_t>(static_cast<std::uint8_t>(data_[offset_])) |
                            (static_cast<std::uint64_t>(static_cast<std::uint8_t>(data_[offset_ + 1])) << 8) |
                            (static_cast<std::uint64_t>(static_cast<std::uint8_t>(data_[offset_ + 2])) << 16) |
                            (static_cast<std::uint64_t>(static_cast<std::uint8_t>(data_[offset_ + 3])) << 24) |
                            (static_cast<std::uint64_t>(static_cast<std::uint8_t>(data_[offset_ + 4])) << 32) |
                            (static_cast<std::uint64_t>(static_cast<std::uint8_t>(data_[offset_ + 5])) << 40) |
                            (static_cast<std::uint64_t>(static_cast<std::uint8_t>(data_[offset_ + 6])) << 48) |
                            (static_cast<std::uint64_t>(static_cast<std::uint8_t>(data_[offset_ + 7])) << 56);
        offset_ += 8;
        return val;
    }

private:
    std::span<const std::byte> data_;
    std::size_t offset_;
};

} // namespace rac1

#endif // RAC1_IO_BYTE_READER_H
