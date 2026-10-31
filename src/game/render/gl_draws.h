// The OpenGL renderer's multi-draws (the MRP world's and mobys' glMultiDrawElementsIndirect), and a path without them for the
// drivers that get them wrong. Host code, no original counterpart.
//
// GitHub issue #113: on an Intel UHD 620 under Windows (Intel's own OpenGL driver) the ties, shrubs and mobys were drawn now
// and then with other draws' records, or not at all, from frame to frame, while the terrain (plain draws) was always right. Taking
// the draw's index from an instanced attribute instead of gl_BaseInstance changed nothing: what is left in common is the multi-draw
// itself, its commands read by the driver from the indirect buffer. With single draws the commands go to the driver as the
// arguments of glDrawElementsInstancedBaseVertexBaseInstance, from the CPU's copy: the same draws, the same order, the same
// picture, a call each.
#ifndef RAC1_GAME_RENDER_GL_DRAWS_H
#define RAC1_GAME_RENDER_GL_DRAWS_H

#include <cstdint>

namespace rac1 {

// orig: (host) once the OpenGL context is current (the main thread): GL_VENDOR / GL_RENDERER / GL_VERSION in the log, and whether
// the multi-draws go as single draws: RAC1_GL_SINGLE_DRAWS=1 / 0 when set, else on with Intel's Windows driver
void gl_draws_init();
bool gl_single_draws();

// orig: (host) the commands first..first+count of the bound GL_DRAW_INDIRECT_BUFFER (DrawElementsIndirectCommand, 5 words each:
// count, instances, first index, base vertex, base instance), triangles with 32-bit indices of the bound element buffer.
// `cpu`: the same commands as the buffer holds them (the whole array; the single draws read theirs there)
void gl_multi_draw_elements(const void* cpu, uint32_t first, uint32_t count);

}  // namespace rac1

#endif
