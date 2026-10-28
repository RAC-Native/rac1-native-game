// The Metal renderer's device and frame (macOS; docs/metal.md): what runtime/vk/vk_context.h is to the Vulkan renderer,
// with the same names and, where they mean the same, the same arguments. The rendering modules' Metal branches are their
// Vulkan branches with the descriptor sets, layouts and barriers gone:
// - formats, filters, address modes, blend factors, compare ops, topologies and the rest are given as Vulkan's enums
//   (Vulkan-Headers: types only, nothing of Vulkan runs), translated here;
// - a pipeline is made from the shaders' names (tools/gen_metal_shaders.py, at build time) and the same fixed
//   state as a Vulkan pipeline; resources are bound by the Vulkan shaders' (set, binding), which the shader table
//   (<build>/generated_mtl/mtl_shader_table.inc) turns into Metal's buffer / texture / sampler indices;
// - binds and state are kept and applied at the next draw or dispatch (a Metal encoder keeps nothing from the one
//   before it: the bindings outlive a render pass, as in Vulkan);
// - the pictures are kept as the Vulkan renderer keeps them, in OpenGL's orientation (row 0 the bottom): the vertex
//   shaders flip Y, so viewports, scissors and the faces' winding are given as they are to Vulkan.
//
// No Objective-C here: the handles are id<MTL...> pointers as void* (retained by the context, released by destroy_*).
#pragma once

#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

struct GLFWwindow;
struct ImDrawData;

namespace rac1::mtl {

constexpr uint32_t kFramesInFlight = 2;

struct GpuBuffer {
    void* buffer = nullptr;  // id<MTLBuffer>
    size_t size = 0;
    void* mapped = nullptr;  // (shared storage: always mapped when asked for)
};

struct GpuImage {
    void* image = nullptr;       // id<MTLTexture>
    void* depth_view = nullptr;  // (a depth-and-stencil image: the texture itself samples its depth; unused)
    void* stencil_view = nullptr;  // (a depth-and-stencil image: a view of its stencil, made on demand)
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t layers = 1;
    uint32_t mip_levels = 1;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
};

struct RingAlloc {
    void* buffer = nullptr;  // id<MTLBuffer>
    size_t offset = 0;
    void* ptr = nullptr;
};

// A render pass's attachments: textures, and the array layer and mip level each draws into
struct RenderTarget {
    void* color = nullptr;
    VkFormat color_format = VK_FORMAT_UNDEFINED;
    uint32_t color_layer = 0;
    uint32_t color_level = 0;
    void* depth = nullptr;
    VkFormat depth_format = VK_FORMAT_UNDEFINED;
    uint32_t depth_layer = 0;
    void* stencil = nullptr;  // (the depth texture again when it has a stencil)
    uint32_t width = 0;
    uint32_t height = 0;
};
// a target over an image (its colour, or its depth and stencil), a layer and a level of it
RenderTarget color_target(const GpuImage& color, const GpuImage* depth = nullptr, uint32_t layer = 0, uint32_t level = 0);
RenderTarget depth_target(const GpuImage& depth, uint32_t layer = 0);

struct Shader;    // a compiled shader with its binding table (opaque)
struct Pipeline;  // a render pipeline with its depth, stencil and rasterizer state (opaque)
struct ComputePipeline;

struct GraphicsPipelineDesc {
    const char* vs = nullptr;  // the shaders' names (kMrpWorldVs...)
    const char* fs = nullptr;
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    std::vector<VkVertexInputBindingDescription> vertex_bindings;
    std::vector<VkVertexInputAttributeDescription> vertex_attribs;
    VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cull_mode = VK_CULL_MODE_NONE;
    bool depth_clamp = false;
    bool depth_bias_enable = false;
    float depth_bias_constant = 0.0f;
    float depth_bias_slope = 0.0f;
    bool depth_test = true;
    bool depth_write = true;
    VkCompareOp depth_compare = VK_COMPARE_OP_LESS_OR_EQUAL;
    bool stencil_test = false;
    VkStencilOpState stencil_front{};
    VkStencilOpState stencil_back{};
    bool blend_enable = false;
    VkBlendFactor src_color_blend = VK_BLEND_FACTOR_SRC_ALPHA;
    VkBlendFactor dst_color_blend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    VkBlendOp color_blend_op = VK_BLEND_OP_ADD;
    VkBlendFactor src_alpha_blend = VK_BLEND_FACTOR_ONE;
    VkBlendFactor dst_alpha_blend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    VkBlendOp alpha_blend_op = VK_BLEND_OP_ADD;
    VkColorComponentFlags color_write_mask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkFormat color_format = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat depth_format = VK_FORMAT_UNDEFINED;  // (0 = depth_stencil_format() when has_depth_attachment is true)
    bool has_depth_attachment = true;
    bool has_stencil_attachment = true;
    std::vector<VkDynamicState> extra_dynamic_states;  // (the stencil reference and the depth bias, when given here)
};

// Lifecycle
bool available();  // (a Metal device exists)
bool init(GLFWwindow* window, bool vsync, bool headless = false);
void shutdown();
bool is_active();
bool in_frame();
void wait_idle();
void set_vsync(bool vsync);

// Frame boundaries
void begin_frame(int win_w, int win_h, float clear_r, float clear_g, float clear_b, float clear_a = 1.0f);
void render_imgui(ImDrawData* draw_data);
void imgui_init();      // (ImGui's Metal backend, its font texture; after ImGui's context is made)
void imgui_shutdown();
void end_frame();
void flush_cmd();  // (the frame's commands so far run and are waited for; the frame goes on)
void submit_immediate(const std::function<void()>& fn);  // (fn's commands run now and are waited for)

void* device();         // id<MTLDevice>
void* command_queue();  // id<MTLCommandQueue>
VkFormat color_format();
VkFormat depth_stencil_format();
uint32_t frame_index();
uint64_t frame_number();
int main_width();
int main_height();
int current_target_width();
int current_target_height();
const RenderTarget& current_target();

// Scene render targets ("FBO 0" and ScaledScene)
GpuImage& main_color_image();
GpuImage& main_depth_image();
GpuImage& active_color_image();
GpuImage& active_depth_image();
void scaled_scene_begin(int win_w, int win_h, int bw, int bh);
void scaled_scene_end(int win_w, int win_h);
bool scaled_scene_active();

// Render passes & state
void push_target(const RenderTarget& rt, bool clear_color = false, const float rgba[4] = nullptr,
                 bool clear_depth = false, float depth_val = 1.0f, bool clear_stencil = false, uint32_t stencil_val = 0);
void pop_target();
void ensure_rendering();
void end_rendering();
void set_viewport(int x, int y, int w, int h, float min_depth = 0.0f, float max_depth = 1.0f);
void get_viewport(int out_vp[4]);
void set_scissor(bool enable, int x = 0, int y = 0, int w = 0, int h = 0);
void clear_attachments(bool color, const float rgba[4], bool depth, float depth_val = 1.0f,
                       bool stencil = false, uint32_t stencil_val = 0);

// Buffers
bool create_buffer(size_t size, GpuBuffer& out, bool map = true);
void destroy_buffer(GpuBuffer& buf);      // (when the frames that may use it are done)
void destroy_buffer_now(GpuBuffer& buf);
bool upload_buffer(GpuBuffer& dst, const void* data, size_t size);  // (made or grown as needed)
RingAlloc alloc_ring(size_t size, size_t alignment = 256);  // (this frame's, CPU-written)

// Images & transfers
bool create_image(uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect,
                  GpuImage& out, uint32_t layers = 1, VkImageCreateFlags flags = 0,
                  VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D, uint32_t mip_levels = 1);
void destroy_image(GpuImage& img);
void destroy_image_now(GpuImage& img);
void upload_image_2d(GpuImage& img, const void* data, size_t byte_size, int x = 0, int y = 0, int w = -1, int h = -1,
                     int row_length = 0, uint32_t layer = 0, uint32_t mip_level = 0);
// vkCmdBlitImage's semantics: a region to a region, scaled, either one flipped by its coordinates' order
void blit_image(GpuImage& src, int sx0, int sy0, int sx1, int sy1, GpuImage& dst, int dx0, int dy0, int dx1, int dy1,
                VkFilter filter = VK_FILTER_NEAREST, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);
void copy_image(GpuImage& src, int sx, int sy, GpuImage& dst, int dx, int dy, int w, int h,
                VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT, uint32_t src_layer = 0, uint32_t dst_layer = 0);
void generate_mipmaps(GpuImage& img);
void clear_color_image(GpuImage& img, const float rgba[4]);
void clear_depth_stencil_image(GpuImage& img, float depth, uint32_t stencil);
// a region of an image (a layer of it) into a buffer, rows tightly packed (vkCmdCopyImageToBuffer); the depth of a
// depth-and-stencil image as 32-bit floats
void copy_image_to_buffer(GpuImage& src, int x, int y, int w, int h, void* buffer, size_t offset,
                          VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT, uint32_t layer = 0);
void fill_buffer(void* buffer, size_t offset, size_t size, uint8_t value);
void copy_buffer(void* src, size_t src_offset, void* dst, size_t dst_offset, size_t size);
void read_image_rgba(GpuImage& src, int w, int h, void* out_rgba);  // (waits for the GPU)
void save_ppm(const char* path, int w, int h);

// Samplers & fallback textures
void* get_sampler(VkFilter min_filter, VkFilter mag_filter, VkSamplerAddressMode wrap_s, VkSamplerAddressMode wrap_t,
                  bool compare = false, VkCompareOp compare_op = VK_COMPARE_OP_LESS_OR_EQUAL, float max_anisotropy = 1.0f,
                  VkSamplerMipmapMode mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST, float max_lod = 0.25f);
const GpuImage& white_texture();
const GpuImage& white_texture_array();
const GpuImage& black_uint_texture();
const GpuImage& dummy_depth_2d();
const GpuImage& dummy_depth_array();
const GpuImage& dummy_depth_cube_array();

// Pipelines (made once, kept until shutdown)
Pipeline* create_graphics_pipeline(const GraphicsPipelineDesc& desc);
ComputePipeline* create_compute_pipeline(const char* cs);
void destroy_pipeline(Pipeline* p);
void destroy_compute_pipeline(ComputePipeline* p);
// the stencil of a depth-and-stencil image as a texture to sample (a view, made once, kept with the image)
void* stencil_view(GpuImage& img);
// a view of some layers (and levels) of an image, retained (released with release_object)
void* create_texture_view(const GpuImage& img, VkImageViewType type, VkImageAspectFlags aspect, uint32_t base_layer,
                          uint32_t layers, uint32_t base_level, uint32_t levels);
void release_object(void* o);  // (when the frames that may use it are done)

// Drawing: binds are kept and applied at the next draw (to the stages whose shaders use them)
void bind_pipeline(const Pipeline* p);
void bind_compute_pipeline(const ComputePipeline* p);
void push_constants(const void* data, size_t size, size_t offset = 0);
void bind_buffer(uint32_t set, uint32_t binding, void* buffer, size_t offset = 0);
void bind_ring(uint32_t set, uint32_t binding, const RingAlloc& r);
void bind_image(uint32_t set, uint32_t binding, const GpuImage& img, void* sampler);
void bind_texture(uint32_t set, uint32_t binding, void* texture, void* sampler);  // (a raw id<MTLTexture>)
// the world's 16 texture units (descriptor set 2 of its fragment shaders: an argument buffer)
void bind_texture_units(void* const* textures, void* const* samplers, uint32_t count);
void unbind_all();  // (forget the resources bound so far)
void bind_vertex_buffer(uint32_t binding, void* buffer, size_t offset);
void bind_index_buffer(void* buffer, size_t offset, VkIndexType type);
void set_depth_bias(float constant, float clamp, float slope);
void set_stencil_reference(uint32_t ref);
void set_blend_constants(const float rgba[4]);
void draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0);
void draw_indexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t first_index = 0, int32_t vertex_offset = 0,
                  uint32_t first_instance = 0);
// vkCmdDraw(Indexed)Indirect: `count` commands `stride` bytes apart (VkDraw(Indexed)IndirectCommand's layout)
void draw_indirect(void* buffer, size_t offset, uint32_t count, uint32_t stride);
void draw_indexed_indirect(void* buffer, size_t offset, uint32_t count, uint32_t stride);
void dispatch(uint32_t groups_x, uint32_t groups_y = 1, uint32_t groups_z = 1);

// Shutdown callbacks registered by rendering modules
void register_shutdown_callback(void (*fn)());

}  // namespace rac1::mtl
