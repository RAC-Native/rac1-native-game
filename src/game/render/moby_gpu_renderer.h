// The modern GPU renderer for mobys (ADR 0009).
// orig: (host) GPU renderer consuming unpacked VU1 moby DMA chains and rendering in native depth space.
#ifndef RAC1_GAME_RENDER_MOBY_GPU_RENDERER_H
#define RAC1_GAME_RENDER_MOBY_GPU_RENDERER_H

#include <cstdint>
#include <vector>

namespace rac1 {

struct VUState;
namespace gs {
struct Viewport;
}

// orig: (host) initialize shaders and vertex buffers
void moby_gpu_init();

// orig: (host) reset per-frame vertex and draw collections
void moby_gpu_begin_frame();

// orig: (host) begin a submesh before running VU1 microcode
void moby_gpu_begin_submesh(const VUState& st, uint32_t start_pc);

// orig: (host) process a GIF packet kicked by VU1 microcode
void moby_gpu_process_kick(const VUState& st, uint32_t start_pc, uint32_t kick_addr);

// orig: (host) end current submesh after VU1 microcode finishes
void moby_gpu_end_submesh();

// orig: (host) render all collected moby submeshes with native OpenGL depth testing
void moby_gpu_draw(int win_w, int win_h, const gs::Viewport* vp);

namespace gs {
class Gs;
struct DepthMap;
}

// orig: (host) compare drawn mobys in offscreen buffers and diff GS state
void moby_gpu_diff_compare_drawn(int w, int h, const gs::Viewport* vp, const gs::Gs& gs, float fb_w, float fb_h, uint64_t frame, const gs::DepthMap* depth);

}  // namespace rac1

#endif  // RAC1_GAME_RENDER_MOBY_GPU_RENDERER_H
