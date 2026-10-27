#include <runtime/iop/iop_ram.h>

#include <vector>

namespace rac1 {

uint8_t* iop_ram() {
    static std::vector<uint8_t> ram(kIopRamSize, 0);
    return ram.data();
}

}  // namespace rac1
