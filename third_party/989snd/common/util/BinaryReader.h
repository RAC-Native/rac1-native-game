#pragma once

/*!
 * @file BinaryReader.h
 * Read raw data like a stream.
 */

#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>
#include <vector>

#include "common/common_types.h"
#include "common/util/Assert.h"

// rac1 (the Wii U port, a big-endian host): the files are little-endian. A number read whole is put in the host's order;
// a record is too through its bin_le_record(T&) (found by ADL; a record of bytes alone has none and stays as read).
template <typename T>
inline void bin_le_swap(T& v) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  if constexpr (sizeof(T) > 1) {
    unsigned char b[sizeof(T)];
    memcpy(b, &v, sizeof(T));
    for (size_t i = 0; i < sizeof(T) / 2; ++i) {
      const unsigned char t = b[i];
      b[i] = b[sizeof(T) - 1 - i];
      b[sizeof(T) - 1 - i] = t;
    }
    memcpy(&v, b, sizeof(T));
  }
#else
  (void)v;
#endif
}
template <typename T, size_t N>
inline void bin_le_swap(T (&a)[N]) {
  for (T& v : a) bin_le_swap(v);
}

class BinaryReader {
 public:
  explicit BinaryReader(std::span<const uint8_t> _span) : m_span(_span) {}

  template <typename T>
  T read() {
    ASSERT(m_seek + sizeof(T) <= m_span.size());
    T obj;
    memcpy(&obj, m_span.data() + m_seek, sizeof(T));
    m_seek += sizeof(T);
    if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>) {
      bin_le_swap(obj);
    } else if constexpr (requires { bin_le_record(obj); }) {
      bin_le_record(obj);
    }
    return obj;
  }

  void ffwd(int amount) {
    m_seek += amount;
    ASSERT(m_seek <= m_span.size());
  }

  uint32_t bytes_left() const { return m_span.size() - m_seek; }
  const uint8_t* here() { return m_span.data() + m_seek; }
  BinaryReader at(u32 pos) { return BinaryReader(m_span.subspan(pos)); }
  uint32_t get_seek() const { return m_seek; }
  void set_seek(u32 seek) { m_seek = seek; }

 private:
  std::span<const u8> m_span;
  uint32_t m_seek = 0;
};
