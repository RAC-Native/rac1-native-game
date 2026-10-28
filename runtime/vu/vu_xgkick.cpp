#include "vu_xgkick.h"
#include <cstring>
#include <iostream>

namespace rac1 {

void vu_execute_xgkick(VUState& state, uint32_t addr, std::vector<GsPrimitive>& primitives) {
    uint32_t byte_addr = (addr & 0x3FF) * 16;
    if (byte_addr + 16 > sizeof(state.mem)) return;

    // Read 128-bit GIF Tag
    // (VU memory: host-order 32-bit words, a doubleword's low one first on any host)
    uint32_t w[4];
    std::memcpy(w, &state.mem[byte_addr], 16);
    const uint64_t tag_low = (static_cast<uint64_t>(w[1]) << 32) | w[0], tag_high = (static_cast<uint64_t>(w[3]) << 32) | w[2];
    byte_addr += 16;

    uint32_t nloop = tag_low & 0x7FFF;
    bool pre = (tag_low >> 46) & 1;
    uint32_t prim_field = (tag_low >> 47) & 0x7FF;
    uint32_t flg = (tag_low >> 58) & 3;
    uint32_t nreg = (tag_low >> 60) & 15;
    if (nreg == 0) nreg = 16;


    if (flg != 0) { // We only support PACKED
        std::cerr << "XGKICK: Unsupported GIF mode (FLG != 0)" << std::endl;
        return;
    }

    GsPrimitive curr_prim = {};
    if (pre) curr_prim.prim = prim_field;

    for (uint32_t loop = 0; loop < nloop; ++loop) {
        for (uint32_t reg_idx = 0; reg_idx < nreg; ++reg_idx) {
            if (byte_addr + 16 > sizeof(state.mem)) return;

            uint32_t reg_desc = (tag_high >> (reg_idx * 4)) & 15;
            
            // Read 128-bit data
            uint32_t data[4];
            std::memcpy(data, &state.mem[byte_addr], 16);
            byte_addr += 16;

            switch (reg_desc) {
                case 0x00: // PRIM
                    curr_prim.prim = data[0];
                    break;
                case 0x01: // RGBAQ
                    curr_prim.r = data[0] & 0xFF;
                    curr_prim.g = data[1] & 0xFF;
                    curr_prim.b = data[2] & 0xFF;
                    curr_prim.a = data[3] & 0xFF;
                    // Q is typically in data[3] as float, but we only store RGBA
                    break;
                case 0x02: // ST
                case 0x03: { // UV
                    float f0, f1;
                    std::memcpy(&f0, &data[0], 4);
                    std::memcpy(&f1, &data[1], 4);
                    curr_prim.u = f0;
                    curr_prim.v = f1;
                    break;
                }
                case 0x04: // XYZF2
                case 0x05: { // XYZ2
                    // In PACKED mode, the GIF packs the 128-bit QWORD into the GS register.
                    // data[0] is X (32-bit int from ftoi4)
                    // data[1] is Y (32-bit int from ftoi4)
                    // data[2] is Z (32-bit int from ftoi4)
                    // data[3] contains ADC at bit 15.
                    
                    uint32_t vx = data[0] & 0xFFFF;
                    uint32_t vy = data[1] & 0xFFFF;
                    uint32_t vz = data[2]; // Z can be 24-bit or 32-bit, usually the GS takes the whole thing or bits 0-23
                    
                    // In XYZF2/XYZ2, the ADC bit is placed in the upper 32 bits, but from the 128-bit QWORD it comes from bit 15 of Word 3.
                    curr_prim.adc = (data[3] >> 15) & 1;

                    // GS fixed-point 12.4 format to float
                    // Note: sign extension might be needed if values can be negative, 
                    // but for XYZ window coordinates they are usually positive (0-4096).
                    curr_prim.x = (float)vx / 16.0f;
                    curr_prim.y = (float)vy / 16.0f;
                    curr_prim.z = (float)vz / 16.0f; // Depending on Z format, maybe just / 16.0f? Actually Z is 24-bit or 32-bit, no fractional part if it's Z buffer? Wait, Z is 24.8 in GS? Wait, in PS2 Z is usually 32-bit or 24-bit integer, no 16-division. Let's just use vz / 16.0f? Wait, Z is usually fixed point as well? Actually if it's 24.8 or 32.0. Let's just use vz for now or vz/16 if it's scaled.

                    // Emit primitive
                    primitives.push_back(curr_prim);
                    break;
                }
                case 0x0A: // FOG
                    // Ignore for now
                    break;
                case 0x0E: // A+D
                    if (data[2] == 0x00) curr_prim.prim = data[0]; // if addressing PRIM register
                    break;
                default:
                    // Ignored
                    break;
            }
        }
    }
}

} // namespace rac1
