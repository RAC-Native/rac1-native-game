// The host's byte order against the PS2's (little-endian): what a big-endian host (the Wii U) swaps when it reads the
// PS2's bytes, from a file or from EE memory (ee_read of a record: runtime/kernel/ee_memory.h). On a little-endian host
// every function here does nothing.
#ifndef RAC1_IO_HOST_ENDIAN_H
#define RAC1_IO_HOST_ENDIAN_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace rac1 {

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
inline constexpr bool k_host_big_endian = true;
#else
inline constexpr bool k_host_big_endian = false;
#endif

// One field of a record in the PS2's byte order put in the host's (or back: the same swap)
template <typename T>
inline void le_fix_field(T& v) {
    static_assert(std::is_trivially_copyable_v<T>);
    if constexpr (k_host_big_endian && sizeof(T) > 1) {
        unsigned char b[sizeof(T)];
        std::memcpy(b, &v, sizeof(T));
        for (std::size_t i = 0; i < sizeof(T) / 2; ++i) {
            const unsigned char t = b[i];
            b[i] = b[sizeof(T) - 1 - i];
            b[sizeof(T) - 1 - i] = t;
        }
        std::memcpy(&v, b, sizeof(T));
    }
}
template <typename T, std::size_t N>
inline void le_fix_field(T (&a)[N]) {
    for (auto& v : a) le_fix_field(v);
}
template <typename... T>
inline void le_fix_fields(T&... v) {
    (le_fix_field(v), ...);
}

// One field of a record by its offset (a packed record's fields cannot be bound to references): RAC1_LE_FIX(r, field)
template <typename F>
inline void le_fix_member(void* rec, std::size_t off) {
    if constexpr (k_host_big_endian) {
        F v;
        std::memcpy(&v, static_cast<unsigned char*>(rec) + off, sizeof(F));
        le_fix_field(v);
        std::memcpy(static_cast<unsigned char*>(rec) + off, &v, sizeof(F));
    }
}
#define RAC1_LE_FIX(r, field) \
    ::rac1::le_fix_member<decltype(r.field)>(&(r), offsetof(std::remove_reference_t<decltype(r)>, field))

// A record made of 4-byte fields alone: each of them
template <typename T>
inline void le_fix_words32(T& r) {
    static_assert(sizeof(T) % 4 == 0);
    if constexpr (k_host_big_endian) {
        unsigned char* b = reinterpret_cast<unsigned char*>(&r);
        for (std::size_t i = 0; i < sizeof(T); i += 4) {
            const unsigned char t0 = b[i], t1 = b[i + 1];
            b[i] = b[i + 3];
            b[i + 1] = b[i + 2];
            b[i + 2] = t1;
            b[i + 3] = t0;
        }
    }
}

}  // namespace rac1

#endif
