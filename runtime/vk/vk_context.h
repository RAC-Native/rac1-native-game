#pragma once

#include <volk.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

struct GLFWwindow;
struct ImDrawData;

namespace rac1::vk {

constexpr uint32_t kFramesInFlight = 2;

struct GpuBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    void* mapped = nullptr;
};

struct GpuImage {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkImageView depth_view = VK_NULL_HANDLE;  // (a sampled depth-and-stencil image: its depth alone, for sampling it)
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t layers = 1;
    uint32_t mip_levels = 1;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
};

struct RingAlloc {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceSize offset = 0;
    void* ptr = nullptr;
};

struct RenderTarget {
    VkImageView color_view = VK_NULL_HANDLE;
    VkFormat color_format = VK_FORMAT_UNDEFINED;
    VkImageView depth_view = VK_NULL_HANDLE;
    VkFormat depth_format = VK_FORMAT_UNDEFINED;
    VkImageView stencil_view = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct GraphicsPipelineDesc {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkShaderModule vs = VK_NULL_HANDLE;
    VkShaderModule fs = VK_NULL_HANDLE;
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
    VkFormat depth_format = VK_FORMAT_UNDEFINED;  // (0 = use depth_stencil_format() when has_depth_attachment is true)
    bool has_depth_attachment = true;
    bool has_stencil_attachment = true;
    std::vector<VkDynamicState> extra_dynamic_states;
};

// Lifecycle
bool init(GLFWwindow* window, bool vsync, bool headless = false);
void shutdown();
bool is_active();
void wait_idle();
void set_vsync(bool vsync);

// Frame boundaries
void begin_frame(int win_w, int win_h, float clear_r, float clear_g, float clear_b, float clear_a = 1.0f);
void render_imgui(ImDrawData* draw_data);
void end_frame();
void flush_cmd();
void submit_immediate(const std::function<void(VkCommandBuffer)>& fn);

// What the device can do (init's check: GitHub issue #113)
bool has_float64();             // shaderFloat64: the tfrag compute shader's doubles

// Core handles
VkInstance instance();
VkPhysicalDevice physical_device();
VkDevice device();
uint32_t graphics_queue_family();
VkQueue graphics_queue();
VkCommandBuffer cmd();
VkDescriptorPool imgui_descriptor_pool();
VkFormat color_format();
VkFormat swapchain_format();
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

// Dynamic rendering & state
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

// Memory & buffers
uint32_t find_memory_type(uint32_t type_bits, VkMemoryPropertyFlags props);
// the flags of a buffer the CPU reads back: host-visible, coherent and cached when the device has such memory (reads from
// uncached, write-combined memory are slow: about 3.5 ms for a 640x448 picture on an APU, far more over PCIe)
VkMemoryPropertyFlags readback_memory_flags();
bool create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, GpuBuffer& out, bool map = false);
void destroy_buffer(GpuBuffer& buf);
void destroy_buffer_now(GpuBuffer& buf);
bool upload_buffer(GpuBuffer& dst, const void* data, VkDeviceSize size, VkBufferUsageFlags usage);
RingAlloc alloc_ring(VkDeviceSize size, VkDeviceSize alignment = 256);

// Images & transfers
bool create_image(uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect,
                  GpuImage& out, uint32_t layers = 1, VkImageCreateFlags flags = 0,
                  VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D, uint32_t mip_levels = 1);
void destroy_image(GpuImage& img);
void destroy_image_now(GpuImage& img);
void transition_image(VkCommandBuffer cb, GpuImage& img, VkImageLayout new_layout);
void upload_image_2d(GpuImage& img, const void* data, size_t byte_size, int x = 0, int y = 0, int w = -1, int h = -1,
                     int row_length = 0, uint32_t layer = 0, uint32_t mip_level = 0);
void blit_image(GpuImage& src, int sx0, int sy0, int sx1, int sy1, GpuImage& dst, int dx0, int dy0, int dx1, int dy1,
                VkFilter filter = VK_FILTER_NEAREST, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                VkImageLayout dst_final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
void read_image_rgba(GpuImage& src, int w, int h, void* out_rgba);
// A same-size copy (vkCmdCopyImage: any format, the depth and the stencil too, unlike a blit), each image left in `final`
void copy_image(GpuImage& src, int sx, int sy, GpuImage& dst, int dx, int dy, int w, int h, VkImageAspectFlags aspect,
                VkImageLayout src_final, VkImageLayout dst_final);
void save_ppm(const char* path, int w, int h);

// Descriptors, samplers, pipelines & fallback textures
VkSampler get_sampler(VkFilter min_filter, VkFilter mag_filter, VkSamplerAddressMode wrap_s, VkSamplerAddressMode wrap_t,
                      bool compare = false, VkCompareOp compare_op = VK_COMPARE_OP_LESS_OR_EQUAL, float max_anisotropy = 1.0f,
                      VkSamplerMipmapMode mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST, float max_lod = 0.25f);
VkDescriptorSet alloc_descriptor_set(VkDescriptorSetLayout layout);
VkShaderModule create_shader_module(const uint32_t* code, size_t size_in_bytes);
VkPipeline create_graphics_pipeline(const GraphicsPipelineDesc& desc);
VkPipeline create_compute_pipeline(VkPipelineLayout layout, VkShaderModule cs);

const GpuImage& white_texture();
const GpuImage& white_texture_array();
const GpuImage& black_uint_texture();
const GpuImage& dummy_depth_2d();
const GpuImage& dummy_depth_array();
const GpuImage& dummy_depth_cube_array();

// Shutdown callbacks registered by rendering modules
void register_shutdown_callback(void (*fn)());

}  // namespace rac1::vk
