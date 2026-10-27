// The IOP's 2 MB of RAM on the host: what the EE puts there by SIF DMA (sceSifSetDma) and the
// host IOP modules (src: runtime/iop) read and answer from.
#ifndef RAC1_RUNTIME_IOP_IOP_RAM_H
#define RAC1_RUNTIME_IOP_IOP_RAM_H

#include <cstdint>

namespace rac1 {

constexpr uint32_t kIopRamSize = 0x200000;
uint8_t* iop_ram();  // kIopRamSize bytes, zeroed at start

}  // namespace rac1

#endif
