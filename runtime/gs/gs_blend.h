// The GS's blending and its other per-primitive pixel state as GPU state, shared by every drawer of GS-state primitives: the MRP's
// world, mobys, effects, drop shadows and 2D (src/game/render), and the GS replica's (gs_draw.h, the faithful reference).
// ALPHA's Cv = (A - B) C + D maps to the GPU's blending (add, subtract or reverse subtract; the two combinations with a factor
// 1 + C use 1); into a render target the source alpha factors read the GS's alpha (the shader's second output, 0x80 = 1.0).
#ifndef RAC1_RUNTIME_GS_GS_BLEND_H
#define RAC1_RUNTIME_GS_GS_BLEND_H

#include <runtime/gs/gs_types.h>

#include <cstdint>

namespace rac1::vk {
struct GraphicsPipelineDesc;
}

namespace rac1::gs {

// bits [lo, lo + n) of a GS register's value
uint32_t field(uint64_t v, int lo, int n);
// OpenGL: the blending of a primitive with state `st` (PRIM ABE, ALPHA); Vulkan: its FIX as the blend constant
void blend_state(const DrawState& st, bool to_rt = false);
// Vulkan: the same into a pipeline's description, FIX in `out_fix`
void vk_blend_state(const DrawState& st, bool to_rt, vk::GraphicsPipelineDesc& desc, float& out_fix);

}  // namespace rac1::gs

#endif
