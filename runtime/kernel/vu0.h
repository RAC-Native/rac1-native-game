#ifndef RAC1_RUNTIME_KERNEL_VU0_H
#define RAC1_RUNTIME_KERNEL_VU0_H

// The VU0 data types the ported code shares: a Q15 quaternion and a VF register's four floats. (The VU0 itself:
// runtime/vu/vu0.h; its macro mode in the ported code: src/game/ovl/vu0_macro.h.)

#include <cstddef>
#include <cstdint>

namespace rac1 {

// Q15 fixed-point quaternion structure matching the layout in skeleton update
struct Q15Quat {
    int16_t x, y, z, w;
};

// 16-byte aligned 4-float vector matching the PS2 VF register layout.
struct alignas(16) Vec4 {
    float x, y, z, w;
};

} // namespace rac1

#endif // RAC1_RUNTIME_KERNEL_VU0_H
