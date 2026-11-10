// (host, the Wii U) room in a vector for elements about to be appended, their cache lines zeroed first.
// orig: (host)
#ifndef RAC1_GAME_RENDER_VEC_ROOM_H
#define RAC1_GAME_RENDER_VEC_ROOM_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rac1 {

// ((2026-11-10) the Wii U) room for `add` more elements of `v`, the cache lines that start inside it zeroed first (dcbz; whole lines
// inside the storage: what lies past the elements is unused). The 750 reads a line from memory before a store to it: the frame's
// big lists (the world's draw records and draws, ~1 MB on Kalebo; the scene's packets) were read before being written. The values
// written are the same; elsewhere nothing.
template <class T>
inline void vec_room_zeroed(std::vector<T>& v, size_t add) {
#if defined(__WIIU__)
    if (v.capacity() < v.size() + add) v.reserve(std::max(v.capacity() * 2, v.size() + add));
    const uintptr_t b = reinterpret_cast<uintptr_t>(v.data() + v.size()), e = reinterpret_cast<uintptr_t>(v.data() + v.capacity());
    const uintptr_t end = b + add * sizeof(T);
    for (uintptr_t a = (b + 31u) & ~uintptr_t{31}; a < end && a + 32u <= e; a += 32u) __asm__ volatile("dcbz 0,%0" : : "r"(a) : "memory");
#else
    (void)v;
    (void)add;
#endif
}

}  // namespace rac1

#endif
