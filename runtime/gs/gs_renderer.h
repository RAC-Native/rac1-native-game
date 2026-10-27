#ifndef RAC1_RUNTIME_GS_RENDERER_H
#define RAC1_RUNTIME_GS_RENDERER_H

#include <cstdint>

namespace rac1 {

// Associate a fake TBP0 with a real EE RAM pixel virtual address
void gs_bind_texture_vaddr(uint32_t tbp0, uint32_t pixel_vaddr);

// Get or upload a texture to OpenGL
unsigned int get_or_upload_texture(uint32_t tbp0);
// The uploaded textures and their bindings dropped (another level's are registered next).
void gs_texture_cache_clear();

// The GL sampler object that samples as the GS does under a CLAMP_1 value: WMS (bits 0..1) and
// WMT (bits 2..3), 0 REPEAT, 1 CLAMP (the coordinate clamped to the texture's edge texels);
// the filters those of the uploaded textures. The region modes (2, 3) are not modelled: drawn
// as CLAMP. Bind it to the texture unit for the draw, and unbind (sampler 0) after.
unsigned int gs_clamp_sampler(uint64_t clamp);

} // namespace rac1

#endif // RAC1_RUNTIME_GS_RENDERER_H
