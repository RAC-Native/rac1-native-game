// The frame's packet buffers: see packet_rings.h.
#include <game/enhance/packet_rings.h>

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// orig: (host) see packet_rings.h
int32_t packet_room() { return static_cast<int32_t>(lwu(OV(0x0015f5f8)) - lwu(OV(0x001611c0))); }

// orig: (host) see packet_rings.h
uint32_t packet_joint_end() {
    const uint32_t ring = lwu(OV(0x0015f5f8));
    return ring >= 0x02000000u ? 0x04000000u : ring + lwu(OV(0x0015f578));  // (the moby pass's shadow run: its ring ends at 64 MB, mrp_lod.cpp)
}

}  // namespace rac1
