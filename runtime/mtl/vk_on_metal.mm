// The Vulkan renderer's code on Metal (macOS; docs/metal.md). Built on Apple in place of runtime/vk/vk_context.cpp: the same
// rac1::vk API (runtime/vk/vk_context.h), and the Vulkan commands the rendering modules record themselves (vkCmdDraw,
// vkCmdBindDescriptorSets, vkUpdateDescriptorSets...: volk's function pointers, set here), done by the Metal renderer
// (runtime/mtl/mtl_context.h). The modules' Vulkan branches run unchanged; nothing of Vulkan runs.
//
// Vulkan's handles stand for Metal's objects:
// - VkImage, VkImageView: an id<MTLTexture> (an image's view: the image itself; vkCreateImageView's: a texture view);
// - VkBuffer: an id<MTLBuffer>; VkSampler: an id<MTLSamplerState>;
// - VkShaderModule: the shader's name (known by its SPIR-V array's address, vk::shaders::k...); VkPipeline: a
//   mtl::Pipeline / mtl::ComputePipeline; VkDescriptorSet(Layout): what is written to it, bound at vkCmdBindDescriptorSets;
// - VkCommandBuffer, VkDevice and the rest: tokens (Metal records into the frame's command buffer).
// Layouts and barriers mean nothing to Metal (its resources' hazards are tracked): kept, not used.
#import <Metal/Metal.h>

#include <runtime/vk/vk_context.h>
#include <runtime/vk/vk_shaders.h>
#include <runtime/mtl/mtl_context.h>

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

namespace rac1::vk {

namespace {

// ---- the shaders: the SPIR-V arrays' addresses to their names ----
struct ShaderName {
    const uint32_t* code;
    const char* name;
};
#define RAC1_MTL_SHADER(n, st, p, a, ...) {shaders::n, #n},
const ShaderName kShaderNames[] = {
#include <runtime/mtl/mtl_shader_table.inc>
};
#undef RAC1_MTL_SHADER

// ---- tokens for the handles Metal has no use for ----
template <typename T> T token(uintptr_t v) { return reinterpret_cast<T>(v); }
const VkCommandBuffer kCmd = token<VkCommandBuffer>(0x10);
const VkDevice kDevice = token<VkDevice>(0x20);
const VkInstance kInstance = token<VkInstance>(0x30);
const VkPhysicalDevice kPhysDev = token<VkPhysicalDevice>(0x40);
const VkQueue kQueue = token<VkQueue>(0x50);

// ---- the images: a texture's mtl::GpuImage (its format, size, layers, the stencil view made for it) ----
VkFormat vk_format(MTLPixelFormat f) {
    switch (f) {
    case MTLPixelFormatRGBA8Unorm: return VK_FORMAT_R8G8B8A8_UNORM;
    case MTLPixelFormatRGBA8Uint: return VK_FORMAT_R8G8B8A8_UINT;
    case MTLPixelFormatBGRA8Unorm: return VK_FORMAT_B8G8R8A8_UNORM;
    case MTLPixelFormatR8Unorm: return VK_FORMAT_R8_UNORM;
    case MTLPixelFormatRG8Unorm: return VK_FORMAT_R8G8_UNORM;
    case MTLPixelFormatRGBA16Float: return VK_FORMAT_R16G16B16A16_SFLOAT;
    case MTLPixelFormatRGBA16Sint: return VK_FORMAT_R16G16B16A16_SINT;
    case MTLPixelFormatRG16Sint: return VK_FORMAT_R16G16_SINT;
    case MTLPixelFormatRG16Float: return VK_FORMAT_R16G16_SFLOAT;
    case MTLPixelFormatR16Float: return VK_FORMAT_R16_SFLOAT;
    case MTLPixelFormatR32Float: return VK_FORMAT_R32_SFLOAT;
    case MTLPixelFormatR32Uint: return VK_FORMAT_R32_UINT;
    case MTLPixelFormatRG32Float: return VK_FORMAT_R32G32_SFLOAT;
    case MTLPixelFormatRG32Uint: return VK_FORMAT_R32G32_UINT;
    case MTLPixelFormatRGBA32Float: return VK_FORMAT_R32G32B32A32_SFLOAT;
    case MTLPixelFormatRGBA32Uint: return VK_FORMAT_R32G32B32A32_UINT;
    case MTLPixelFormatDepth32Float: return VK_FORMAT_D32_SFLOAT;
    case MTLPixelFormatDepth16Unorm: return VK_FORMAT_D16_UNORM;
    case MTLPixelFormatDepth32Float_Stencil8: return VK_FORMAT_D32_SFLOAT_S8_UINT;
    case MTLPixelFormatX32_Stencil8: return VK_FORMAT_S8_UINT;
    default: return VK_FORMAT_UNDEFINED;
    }
}

std::unordered_map<void*, mtl::GpuImage> g_images;  // by texture
std::set<VkPipeline> g_compute_pipes;

// The mtl::GpuImage of a texture (one not made here, or since remade at the same address: described from the texture)
mtl::GpuImage& image_of(void* tex) {
    mtl::GpuImage& m = g_images[tex];
    id<MTLTexture> t = (__bridge id<MTLTexture>)tex;
    if (t != nil && (m.image != tex || m.width != t.width || m.height != t.height || m.format != vk_format(t.pixelFormat))) {
        if (m.stencil_view != nullptr) mtl::release_object(m.stencil_view);
        m = mtl::GpuImage{};
        m.image = tex;
        m.width = static_cast<uint32_t>(t.width);
        m.height = static_cast<uint32_t>(t.height);
        m.format = vk_format(t.pixelFormat);
        m.mip_levels = static_cast<uint32_t>(t.mipmapLevelCount);
        m.layers = static_cast<uint32_t>(t.arrayLength) * (t.textureType == MTLTextureTypeCube || t.textureType == MTLTextureTypeCubeArray ? 6 : 1);
        m.view_type = t.textureType == MTLTextureType2DArray     ? VK_IMAGE_VIEW_TYPE_2D_ARRAY
                      : t.textureType == MTLTextureTypeCube      ? VK_IMAGE_VIEW_TYPE_CUBE
                      : t.textureType == MTLTextureTypeCubeArray ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
                                                                 : VK_IMAGE_VIEW_TYPE_2D;
    }
    return m;
}
mtl::GpuImage& image_of(const GpuImage& img) { return image_of(reinterpret_cast<void*>(img.image)); }

// a vk::GpuImage for a mtl::GpuImage (the Metal renderer's own: the frame's targets, the fallback textures)
void wrap(const mtl::GpuImage& m, GpuImage& out) {
    out.image = reinterpret_cast<VkImage>(m.image);
    out.view = reinterpret_cast<VkImageView>(m.image);
    out.depth_view = (m.format == VK_FORMAT_D32_SFLOAT_S8_UINT) ? out.view : VK_NULL_HANDLE;
    out.width = m.width;
    out.height = m.height;
    out.layers = m.layers;
    out.mip_levels = m.mip_levels;
    out.format = m.format;
    out.aspect = m.aspect;
}

mtl::RenderTarget mtl_target(const RenderTarget& rt) {
    mtl::RenderTarget m{};
    m.color = reinterpret_cast<void*>(rt.color_view);
    m.color_format = rt.color_view != VK_NULL_HANDLE ? image_of(m.color).format : VK_FORMAT_UNDEFINED;
    m.depth = reinterpret_cast<void*>(rt.depth_view);
    m.depth_format = rt.depth_view != VK_NULL_HANDLE ? image_of(m.depth).format : VK_FORMAT_UNDEFINED;
    m.stencil = reinterpret_cast<void*>(rt.stencil_view);
    if (m.stencil != nullptr && m.depth == nullptr) {  // (a stencil alone: the depth-and-stencil image as both)
        m.depth = m.stencil;
        m.depth_format = image_of(m.stencil).format;
    }
    m.width = rt.width;
    m.height = rt.height;
    return m;
}

// ---- descriptor sets: what is written to them ----
struct SetLayout {
    struct B {
        VkDescriptorType type;
        uint32_t count;
    };
    std::unordered_map<uint32_t, B> bindings;
};
struct Desc {
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    std::vector<void*> tex, smp;  // (an array: the world's texture units)
    void* buffer = nullptr;
    size_t offset = 0;
};
struct DescSet {
    const SetLayout* layout = nullptr;
    std::unordered_map<uint32_t, Desc> d;
};
std::vector<std::unique_ptr<DescSet>> g_sets[kFramesInFlight];  // (a frame's: freed when its slot comes round again)
uint64_t g_sets_frame[kFramesInFlight] = {~0ull, ~0ull};

std::vector<RenderTarget> g_targets;  // (the vk::RenderTargets pushed, as the Metal renderer's target stack)
GpuImage g_main_color{}, g_main_depth{}, g_active_color{}, g_active_depth{};
GpuImage g_white{}, g_white_array{}, g_black_uint{}, g_dummy_d2d{}, g_dummy_darray{}, g_dummy_dcube{};

// ==== the Vulkan commands the modules record (volk's pointers) ====
VKAPI_ATTR void VKAPI_CALL cmd_bind_pipeline(VkCommandBuffer, VkPipelineBindPoint bp, VkPipeline p) {
    if (bp == VK_PIPELINE_BIND_POINT_COMPUTE) mtl::bind_compute_pipeline(reinterpret_cast<mtl::ComputePipeline*>(p));
    else mtl::bind_pipeline(reinterpret_cast<mtl::Pipeline*>(p));
}
VKAPI_ATTR void VKAPI_CALL cmd_bind_descriptor_sets(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, uint32_t first, uint32_t count,
                                                     const VkDescriptorSet* sets, uint32_t, const uint32_t*) {
    for (uint32_t i = 0; i < count; ++i) {
        const auto* s = reinterpret_cast<const DescSet*>(sets[i]);
        if (s == nullptr) continue;
        const uint32_t set = first + i;
        for (const auto& [b, d] : s->d) {
            switch (d.type) {
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                if (d.tex.size() > 1) mtl::bind_texture_units(d.tex.data(), d.smp.data(), static_cast<uint32_t>(d.tex.size()));
                else if (!d.tex.empty()) mtl::bind_texture(set, b, d.tex[0], d.smp[0]);
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: mtl::bind_buffer(set, b, d.buffer, d.offset); break;
            default: break;
            }
        }
    }
}
VKAPI_ATTR void VKAPI_CALL cmd_push_constants(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, uint32_t offset, uint32_t size,
                                              const void* data) {
    mtl::push_constants(data, size, offset);
}
VKAPI_ATTR void VKAPI_CALL cmd_draw(VkCommandBuffer, uint32_t vc, uint32_t ic, uint32_t fv, uint32_t fi) { mtl::draw(vc, ic, fv, fi); }
VKAPI_ATTR void VKAPI_CALL cmd_draw_indexed(VkCommandBuffer, uint32_t n, uint32_t ic, uint32_t fi, int32_t vo, uint32_t finst) {
    mtl::draw_indexed(n, ic, fi, vo, finst);
}
VKAPI_ATTR void VKAPI_CALL cmd_draw_indexed_indirect(VkCommandBuffer, VkBuffer b, VkDeviceSize off, uint32_t n, uint32_t stride) {
    mtl::draw_indexed_indirect(reinterpret_cast<void*>(b), off, n, stride);
}
VKAPI_ATTR void VKAPI_CALL cmd_bind_vertex_buffers(VkCommandBuffer, uint32_t first, uint32_t n, const VkBuffer* b, const VkDeviceSize* off) {
    for (uint32_t i = 0; i < n; ++i) mtl::bind_vertex_buffer(first + i, reinterpret_cast<void*>(b[i]), off[i]);
}
VKAPI_ATTR void VKAPI_CALL cmd_bind_index_buffer(VkCommandBuffer, VkBuffer b, VkDeviceSize off, VkIndexType t) {
    mtl::bind_index_buffer(reinterpret_cast<void*>(b), off, t);
}
VKAPI_ATTR void VKAPI_CALL cmd_set_blend_constants(VkCommandBuffer, const float bc[4]) { mtl::set_blend_constants(bc); }
VKAPI_ATTR void VKAPI_CALL cmd_set_scissor(VkCommandBuffer, uint32_t, uint32_t n, const VkRect2D* r) {
    if (n > 0) mtl::set_scissor(true, r[0].offset.x, r[0].offset.y, static_cast<int>(r[0].extent.width), static_cast<int>(r[0].extent.height));
}
VKAPI_ATTR void VKAPI_CALL cmd_pipeline_barrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags, uint32_t,
                                                const VkMemoryBarrier*, uint32_t, const VkBufferMemoryBarrier*, uint32_t,
                                                const VkImageMemoryBarrier*) {}
VKAPI_ATTR void VKAPI_CALL cmd_copy_image_to_buffer(VkCommandBuffer, VkImage img, VkImageLayout, VkBuffer buf, uint32_t n,
                                                    const VkBufferImageCopy* r) {
    for (uint32_t i = 0; i < n; ++i)
        mtl::copy_image_to_buffer(image_of(reinterpret_cast<void*>(img)), r[i].imageOffset.x, r[i].imageOffset.y, static_cast<int>(r[i].imageExtent.width),
                                  static_cast<int>(r[i].imageExtent.height), reinterpret_cast<void*>(buf), r[i].bufferOffset,
                                  r[i].imageSubresource.aspectMask, r[i].imageSubresource.baseArrayLayer);
}
VKAPI_ATTR void VKAPI_CALL cmd_copy_image(VkCommandBuffer, VkImage src, VkImageLayout, VkImage dst, VkImageLayout, uint32_t n, const VkImageCopy* r) {
    for (uint32_t i = 0; i < n; ++i)
        for (uint32_t l = 0; l < std::max(1u, r[i].srcSubresource.layerCount); ++l)
            mtl::copy_image(image_of(reinterpret_cast<void*>(src)), r[i].srcOffset.x, r[i].srcOffset.y, image_of(reinterpret_cast<void*>(dst)),
                            r[i].dstOffset.x, r[i].dstOffset.y, static_cast<int>(r[i].extent.width), static_cast<int>(r[i].extent.height),
                            r[i].srcSubresource.aspectMask, r[i].srcSubresource.baseArrayLayer + l, r[i].dstSubresource.baseArrayLayer + l);
}
VKAPI_ATTR void VKAPI_CALL cmd_fill_buffer(VkCommandBuffer, VkBuffer b, VkDeviceSize off, VkDeviceSize size, uint32_t data) {
    if (data != 0 && data != 0xFFFFFFFFu) std::fprintf(stderr, "[mtl] vkCmdFillBuffer with 0x%08x: the low byte repeated\n", data);
    if (size == VK_WHOLE_SIZE) size = [(__bridge id<MTLBuffer>)reinterpret_cast<void*>(b) length] - off;
    mtl::fill_buffer(reinterpret_cast<void*>(b), off, size, static_cast<uint8_t>(data & 0xFFu));
}
VKAPI_ATTR void VKAPI_CALL cmd_copy_buffer(VkCommandBuffer, VkBuffer src, VkBuffer dst, uint32_t n, const VkBufferCopy* r) {
    for (uint32_t i = 0; i < n; ++i) mtl::copy_buffer(reinterpret_cast<void*>(src), r[i].srcOffset, reinterpret_cast<void*>(dst), r[i].dstOffset, r[i].size);
}
VKAPI_ATTR void VKAPI_CALL cmd_dispatch(VkCommandBuffer, uint32_t x, uint32_t y, uint32_t z) { mtl::dispatch(x, y, z); }
VKAPI_ATTR void VKAPI_CALL cmd_clear_color_image(VkCommandBuffer, VkImage img, VkImageLayout, const VkClearColorValue* c, uint32_t,
                                                 const VkImageSubresourceRange*) {
    mtl::clear_color_image(image_of(reinterpret_cast<void*>(img)), c->float32);
}
VKAPI_ATTR void VKAPI_CALL cmd_clear_depth_stencil_image(VkCommandBuffer, VkImage img, VkImageLayout, const VkClearDepthStencilValue* v, uint32_t,
                                                         const VkImageSubresourceRange*) {
    mtl::clear_depth_stencil_image(image_of(reinterpret_cast<void*>(img)), v->depth, v->stencil);
}

VKAPI_ATTR VkResult VKAPI_CALL create_descriptor_set_layout(VkDevice, const VkDescriptorSetLayoutCreateInfo* ci, const VkAllocationCallbacks*,
                                                            VkDescriptorSetLayout* out) {
    auto* l = new SetLayout{};
    for (uint32_t i = 0; i < ci->bindingCount; ++i)
        l->bindings[ci->pBindings[i].binding] = {ci->pBindings[i].descriptorType, ci->pBindings[i].descriptorCount};
    *out = reinterpret_cast<VkDescriptorSetLayout>(l);
    return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL destroy_descriptor_set_layout(VkDevice, VkDescriptorSetLayout l, const VkAllocationCallbacks*) {
    delete reinterpret_cast<SetLayout*>(l);
}
VKAPI_ATTR VkResult VKAPI_CALL create_pipeline_layout(VkDevice, const VkPipelineLayoutCreateInfo*, const VkAllocationCallbacks*, VkPipelineLayout* out) {
    *out = reinterpret_cast<VkPipelineLayout>(new int(0));
    return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL destroy_pipeline_layout(VkDevice, VkPipelineLayout l, const VkAllocationCallbacks*) { delete reinterpret_cast<int*>(l); }
VKAPI_ATTR void VKAPI_CALL destroy_shader_module(VkDevice, VkShaderModule, const VkAllocationCallbacks*) {}
VKAPI_ATTR void VKAPI_CALL destroy_pipeline(VkDevice, VkPipeline p, const VkAllocationCallbacks*) {
    if (p == VK_NULL_HANDLE) return;
    if (g_compute_pipes.erase(p) != 0) mtl::destroy_compute_pipeline(reinterpret_cast<mtl::ComputePipeline*>(p));
    else mtl::destroy_pipeline(reinterpret_cast<mtl::Pipeline*>(p));
}
VKAPI_ATTR void VKAPI_CALL update_descriptor_sets(VkDevice, uint32_t n, const VkWriteDescriptorSet* w, uint32_t, const VkCopyDescriptorSet*) {
    for (uint32_t i = 0; i < n; ++i) {
        auto* s = reinterpret_cast<DescSet*>(w[i].dstSet);
        if (s == nullptr) continue;
        Desc& d = s->d[w[i].dstBinding];
        d.type = w[i].descriptorType;
        if (w[i].pImageInfo != nullptr) {
            uint32_t total = w[i].dstArrayElement + w[i].descriptorCount;
            if (s->layout != nullptr) {
                const auto it = s->layout->bindings.find(w[i].dstBinding);
                if (it != s->layout->bindings.end()) total = std::max(total, it->second.count);
            }
            if (d.tex.size() < total) {
                d.tex.resize(total, nullptr);
                d.smp.resize(total, nullptr);
            }
            for (uint32_t k = 0; k < w[i].descriptorCount; ++k) {
                d.tex[w[i].dstArrayElement + k] = reinterpret_cast<void*>(w[i].pImageInfo[k].imageView);
                d.smp[w[i].dstArrayElement + k] = reinterpret_cast<void*>(w[i].pImageInfo[k].sampler);
            }
        } else if (w[i].pBufferInfo != nullptr) {
            d.buffer = reinterpret_cast<void*>(w[i].pBufferInfo[0].buffer);
            d.offset = w[i].pBufferInfo[0].offset;
        }
    }
}
VKAPI_ATTR VkResult VKAPI_CALL create_image_view(VkDevice, const VkImageViewCreateInfo* ci, const VkAllocationCallbacks*, VkImageView* out) {
    const mtl::GpuImage& m = image_of(reinterpret_cast<void*>(ci->image));
    void* v = mtl::create_texture_view(m, ci->viewType, ci->subresourceRange.aspectMask, ci->subresourceRange.baseArrayLayer,
                                       ci->subresourceRange.layerCount, ci->subresourceRange.baseMipLevel, ci->subresourceRange.levelCount);
    *out = reinterpret_cast<VkImageView>(v);
    return v != nullptr ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
}
VKAPI_ATTR void VKAPI_CALL destroy_image_view(VkDevice, VkImageView v, const VkAllocationCallbacks*) {
    if (v == VK_NULL_HANDLE) return;
    g_images.erase(reinterpret_cast<void*>(v));
    mtl::release_object(reinterpret_cast<void*>(v));
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL get_instance_proc_addr(VkInstance, const char*) { return nullptr; }

void install_commands() {
    vkCmdBindPipeline = cmd_bind_pipeline;
    vkCmdBindDescriptorSets = cmd_bind_descriptor_sets;
    vkCmdPushConstants = cmd_push_constants;
    vkCmdDraw = cmd_draw;
    vkCmdDrawIndexed = cmd_draw_indexed;
    vkCmdDrawIndexedIndirect = cmd_draw_indexed_indirect;
    vkCmdBindVertexBuffers = cmd_bind_vertex_buffers;
    vkCmdBindIndexBuffer = cmd_bind_index_buffer;
    vkCmdSetBlendConstants = cmd_set_blend_constants;
    vkCmdSetScissor = cmd_set_scissor;
    vkCmdPipelineBarrier = cmd_pipeline_barrier;
    vkCmdCopyImageToBuffer = cmd_copy_image_to_buffer;
    vkCmdCopyImage = cmd_copy_image;
    vkCmdFillBuffer = cmd_fill_buffer;
    vkCmdCopyBuffer = cmd_copy_buffer;
    vkCmdDispatch = cmd_dispatch;
    vkCmdClearColorImage = cmd_clear_color_image;
    vkCmdClearDepthStencilImage = cmd_clear_depth_stencil_image;
    vkCreateDescriptorSetLayout = create_descriptor_set_layout;
    vkDestroyDescriptorSetLayout = destroy_descriptor_set_layout;
    vkCreatePipelineLayout = create_pipeline_layout;
    vkDestroyPipelineLayout = destroy_pipeline_layout;
    vkDestroyShaderModule = destroy_shader_module;
    vkDestroyPipeline = destroy_pipeline;
    vkUpdateDescriptorSets = update_descriptor_sets;
    vkCreateImageView = create_image_view;
    vkDestroyImageView = destroy_image_view;
    vkGetInstanceProcAddr = get_instance_proc_addr;
}

void refresh_wraps() {
    wrap(mtl::main_color_image(), g_main_color);
    wrap(mtl::main_depth_image(), g_main_depth);
    wrap(mtl::active_color_image(), g_active_color);
    wrap(mtl::active_depth_image(), g_active_depth);
}

RenderTarget vk_target(const mtl::GpuImage& c, const mtl::GpuImage& d) {
    RenderTarget rt{};
    rt.color_view = reinterpret_cast<VkImageView>(c.image);
    rt.color_format = c.format;
    rt.depth_view = reinterpret_cast<VkImageView>(d.image);
    rt.depth_format = d.format;
    rt.stencil_view = rt.depth_view;
    rt.width = c.width;
    rt.height = c.height;
    return rt;
}

}  // namespace

// ==== the rac1::vk API ====

bool init(GLFWwindow* window, bool vsync, bool headless) {
    if (mtl::is_active()) return true;
    if (!mtl::init(window, vsync, headless)) return false;
    install_commands();
    wrap(mtl::white_texture(), g_white);
    wrap(mtl::white_texture_array(), g_white_array);
    wrap(mtl::black_uint_texture(), g_black_uint);
    wrap(mtl::dummy_depth_2d(), g_dummy_d2d);
    wrap(mtl::dummy_depth_array(), g_dummy_darray);
    wrap(mtl::dummy_depth_cube_array(), g_dummy_dcube);
    refresh_wraps();
    return true;
}

void shutdown() {
    mtl::shutdown();
    for (auto& f : g_sets) f.clear();
    g_images.clear();
    g_targets.clear();
}

bool is_active() { return mtl::is_active(); }
void wait_idle() { mtl::wait_idle(); }
void set_vsync(bool vsync) { mtl::set_vsync(vsync); }

void begin_frame(int win_w, int win_h, float r, float g, float b, float a) {
    mtl::begin_frame(win_w, win_h, r, g, b, a);
    const uint32_t slot = mtl::frame_index();
    if (g_sets_frame[slot] != mtl::frame_number()) {  // (the slot's frame before last done: its descriptor sets freed)
        g_sets[slot].clear();
        g_sets_frame[slot] = mtl::frame_number();
    }
    refresh_wraps();
    g_targets.clear();
    g_targets.push_back(vk_target(mtl::main_color_image(), mtl::main_depth_image()));
}

void render_imgui(ImDrawData* draw_data) { mtl::render_imgui(draw_data); }
void end_frame() { mtl::end_frame(); }
void flush_cmd() { mtl::flush_cmd(); }
void submit_immediate(const std::function<void(VkCommandBuffer)>& fn) {
    mtl::submit_immediate([&] { fn(kCmd); });
}

VkInstance instance() { return kInstance; }
VkPhysicalDevice physical_device() { return kPhysDev; }
VkDevice device() { return mtl::is_active() ? kDevice : VK_NULL_HANDLE; }
uint32_t graphics_queue_family() { return 0; }
VkQueue graphics_queue() { return kQueue; }
VkCommandBuffer cmd() { return mtl::in_frame() ? kCmd : VK_NULL_HANDLE; }
VkDescriptorPool imgui_descriptor_pool() { return VK_NULL_HANDLE; }
VkFormat color_format() { return mtl::color_format(); }
VkFormat swapchain_format() { return VK_FORMAT_B8G8R8A8_UNORM; }
VkFormat depth_stencil_format() { return mtl::depth_stencil_format(); }
uint32_t frame_index() { return mtl::frame_index(); }
uint64_t frame_number() { return mtl::frame_number(); }
int main_width() { return mtl::main_width(); }
int main_height() { return mtl::main_height(); }
int current_target_width() { return mtl::current_target_width(); }
int current_target_height() { return mtl::current_target_height(); }
const RenderTarget& current_target() {
    static const RenderTarget kEmpty{};
    return g_targets.empty() ? kEmpty : g_targets.back();
}

GpuImage& main_color_image() {
    wrap(mtl::main_color_image(), g_main_color);
    return g_main_color;
}
GpuImage& main_depth_image() {
    wrap(mtl::main_depth_image(), g_main_depth);
    return g_main_depth;
}
GpuImage& active_color_image() {
    wrap(mtl::active_color_image(), g_active_color);
    return g_active_color;
}
GpuImage& active_depth_image() {
    wrap(mtl::active_depth_image(), g_active_depth);
    return g_active_depth;
}
void scaled_scene_begin(int win_w, int win_h, int bw, int bh) {
    mtl::scaled_scene_begin(win_w, win_h, bw, bh);
    g_targets.push_back(vk_target(mtl::active_color_image(), mtl::active_depth_image()));
    refresh_wraps();
}
void scaled_scene_end(int win_w, int win_h) {
    if (mtl::scaled_scene_active() && g_targets.size() > 1) g_targets.pop_back();
    mtl::scaled_scene_end(win_w, win_h);
    refresh_wraps();
}
bool scaled_scene_active() { return mtl::scaled_scene_active(); }

void push_target(const RenderTarget& rt, bool clear_color, const float rgba[4], bool clear_depth, float depth_val, bool clear_stencil,
                 uint32_t stencil_val) {
    g_targets.push_back(rt);
    mtl::push_target(mtl_target(rt), clear_color, rgba, clear_depth, depth_val, clear_stencil, stencil_val);
}
void pop_target() {
    if (g_targets.size() > 1) g_targets.pop_back();
    mtl::pop_target();
}
void ensure_rendering() { mtl::ensure_rendering(); }
void end_rendering() { mtl::end_rendering(); }
void set_viewport(int x, int y, int w, int h, float min_depth, float max_depth) { mtl::set_viewport(x, y, w, h, min_depth, max_depth); }
void get_viewport(int out_vp[4]) { mtl::get_viewport(out_vp); }
void set_scissor(bool enable, int x, int y, int w, int h) { mtl::set_scissor(enable, x, y, w, h); }
void clear_attachments(bool color, const float rgba[4], bool depth, float depth_val, bool stencil, uint32_t stencil_val) {
    mtl::clear_attachments(color, rgba, depth, depth_val, stencil, stencil_val);
}

uint32_t find_memory_type(uint32_t, VkMemoryPropertyFlags) { return 0; }
VkMemoryPropertyFlags readback_memory_flags() {
    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
}

bool create_buffer(VkDeviceSize size, VkBufferUsageFlags, VkMemoryPropertyFlags, GpuBuffer& out, bool map) {
    destroy_buffer(out);
    mtl::GpuBuffer m{};
    if (!mtl::create_buffer(size, m, true)) return false;  // (shared memory: always mapped)
    out.buffer = reinterpret_cast<VkBuffer>(m.buffer);
    out.memory = VK_NULL_HANDLE;
    out.size = size;
    out.mapped = map ? m.mapped : nullptr;
    return true;
}
void destroy_buffer_now(GpuBuffer& buf) {
    mtl::GpuBuffer m{reinterpret_cast<void*>(buf.buffer), static_cast<size_t>(buf.size), buf.mapped};
    mtl::destroy_buffer_now(m);
    buf = GpuBuffer{};
}
void destroy_buffer(GpuBuffer& buf) {
    if (buf.buffer == VK_NULL_HANDLE) return;
    mtl::GpuBuffer m{reinterpret_cast<void*>(buf.buffer), static_cast<size_t>(buf.size), buf.mapped};
    mtl::destroy_buffer(m);
    buf = GpuBuffer{};
}
bool upload_buffer(GpuBuffer& dst, const void* data, VkDeviceSize size, VkBufferUsageFlags usage) {
    if (size == 0 || data == nullptr) return false;
    if (!create_buffer(size, usage, 0, dst, true)) return false;
    std::memcpy(dst.mapped, data, static_cast<size_t>(size));
    return true;
}
RingAlloc alloc_ring(VkDeviceSize size, VkDeviceSize alignment) {
    const mtl::RingAlloc r = mtl::alloc_ring(static_cast<size_t>(size), static_cast<size_t>(alignment));
    return RingAlloc{reinterpret_cast<VkBuffer>(r.buffer), r.offset, r.ptr};
}

bool create_image(uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, GpuImage& out, uint32_t layers,
                  VkImageCreateFlags flags, VkImageViewType view_type, uint32_t mip_levels) {
    destroy_image(out);
    mtl::GpuImage m{};
    if (!mtl::create_image(w, h, format, usage, aspect, m, layers, flags, view_type, mip_levels)) return false;
    g_images[m.image] = m;
    out.image = reinterpret_cast<VkImage>(m.image);
    out.memory = VK_NULL_HANDLE;
    out.view = reinterpret_cast<VkImageView>(m.image);
    // (a sampled depth-and-stencil image's depth: the texture itself, whose depth Metal samples)
    out.depth_view = (usage & VK_IMAGE_USAGE_SAMPLED_BIT) && (aspect & VK_IMAGE_ASPECT_DEPTH_BIT) && (aspect & VK_IMAGE_ASPECT_STENCIL_BIT)
                         ? out.view
                         : VK_NULL_HANDLE;
    out.width = w;
    out.height = h;
    out.layers = m.layers;
    out.mip_levels = m.mip_levels;
    out.format = format;
    out.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    out.aspect = aspect;
    return true;
}
void destroy_image_now(GpuImage& img) {
    if (img.image != VK_NULL_HANDLE) {
        auto it = g_images.find(reinterpret_cast<void*>(img.image));
        if (it != g_images.end()) {
            mtl::destroy_image_now(it->second);
            g_images.erase(it);
        }
    }
    img = GpuImage{};
}
void destroy_image(GpuImage& img) {
    if (img.image != VK_NULL_HANDLE) {
        auto it = g_images.find(reinterpret_cast<void*>(img.image));
        if (it != g_images.end()) {
            mtl::destroy_image(it->second);
            g_images.erase(it);
        }
    }
    img = GpuImage{};
}
void transition_image(VkCommandBuffer cb, GpuImage& img, VkImageLayout new_layout) {
    if (img.image == VK_NULL_HANDLE) return;
    if (cb == kCmd) mtl::end_rendering();  // (as Vulkan's: a barrier is recorded out of a render pass)
    img.layout = new_layout;
}
void upload_image_2d(GpuImage& img, const void* data, size_t byte_size, int x, int y, int w, int h, int row_length, uint32_t layer,
                     uint32_t mip_level) {
    if (img.image == VK_NULL_HANDLE) return;
    mtl::upload_image_2d(image_of(img), data, byte_size, x, y, w, h, row_length, layer, mip_level);
    img.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}
void blit_image(GpuImage& src, int sx0, int sy0, int sx1, int sy1, GpuImage& dst, int dx0, int dy0, int dx1, int dy1, VkFilter filter,
                VkImageAspectFlags aspect, VkImageLayout dst_final_layout) {
    if (src.image == VK_NULL_HANDLE || dst.image == VK_NULL_HANDLE) return;
    mtl::blit_image(image_of(src), sx0, sy0, sx1, sy1, image_of(dst), dx0, dy0, dx1, dy1, filter, aspect);
    dst.layout = dst_final_layout;
}
void read_image_rgba(GpuImage& src, int w, int h, void* out_rgba) {
    if (src.image == VK_NULL_HANDLE) return;
    mtl::read_image_rgba(image_of(src), w, h, out_rgba);
}
void copy_image(GpuImage& src, int sx, int sy, GpuImage& dst, int dx, int dy, int w, int h, VkImageAspectFlags aspect, VkImageLayout src_final,
                VkImageLayout dst_final) {
    if (src.image == VK_NULL_HANDLE || dst.image == VK_NULL_HANDLE) return;
    mtl::copy_image(image_of(src), sx, sy, image_of(dst), dx, dy, w, h, aspect);
    src.layout = src_final;
    dst.layout = dst_final;
}
void save_ppm(const char* path, int w, int h) { mtl::save_ppm(path, w, h); }

VkSampler get_sampler(VkFilter min_filter, VkFilter mag_filter, VkSamplerAddressMode wrap_s, VkSamplerAddressMode wrap_t, bool compare,
                      VkCompareOp compare_op, float max_anisotropy, VkSamplerMipmapMode mipmap_mode, float max_lod) {
    return reinterpret_cast<VkSampler>(
        mtl::get_sampler(min_filter, mag_filter, wrap_s, wrap_t, compare, compare_op, max_anisotropy, mipmap_mode, max_lod));
}
VkDescriptorSet alloc_descriptor_set(VkDescriptorSetLayout layout) {
    auto s = std::make_unique<DescSet>();
    s->layout = reinterpret_cast<const SetLayout*>(layout);
    VkDescriptorSet h = reinterpret_cast<VkDescriptorSet>(s.get());
    const uint32_t slot = mtl::in_frame() ? mtl::frame_index() : (mtl::frame_index() + kFramesInFlight - 1) % kFramesInFlight;
    g_sets[slot].push_back(std::move(s));
    return h;
}
VkShaderModule create_shader_module(const uint32_t* code, size_t) {
    for (const ShaderName& s : kShaderNames)
        if (s.code == code) return reinterpret_cast<VkShaderModule>(const_cast<char*>(s.name));
    std::fprintf(stderr, "[mtl] a shader module of no known shader\n");
    return VK_NULL_HANDLE;
}
VkPipeline create_graphics_pipeline(const GraphicsPipelineDesc& d) {
    mtl::GraphicsPipelineDesc m{};
    m.vs = reinterpret_cast<const char*>(d.vs);
    m.fs = reinterpret_cast<const char*>(d.fs);
    m.topology = d.topology;
    m.vertex_bindings = d.vertex_bindings;
    m.vertex_attribs = d.vertex_attribs;
    m.polygon_mode = d.polygon_mode;
    m.cull_mode = d.cull_mode;
    m.depth_clamp = d.depth_clamp;
    m.depth_bias_enable = d.depth_bias_enable;
    m.depth_bias_constant = d.depth_bias_constant;
    m.depth_bias_slope = d.depth_bias_slope;
    m.depth_test = d.depth_test;
    m.depth_write = d.depth_write;
    m.depth_compare = d.depth_compare;
    m.stencil_test = d.stencil_test;
    m.stencil_front = d.stencil_front;
    m.stencil_back = d.stencil_back;
    m.blend_enable = d.blend_enable;
    m.src_color_blend = d.src_color_blend;
    m.dst_color_blend = d.dst_color_blend;
    m.color_blend_op = d.color_blend_op;
    m.src_alpha_blend = d.src_alpha_blend;
    m.dst_alpha_blend = d.dst_alpha_blend;
    m.alpha_blend_op = d.alpha_blend_op;
    m.color_write_mask = d.color_write_mask;
    m.color_format = d.color_format;
    m.depth_format = d.depth_format;
    m.has_depth_attachment = d.has_depth_attachment;
    m.has_stencil_attachment = d.has_stencil_attachment;
    m.extra_dynamic_states = d.extra_dynamic_states;
    return reinterpret_cast<VkPipeline>(mtl::create_graphics_pipeline(m));
}
VkPipeline create_compute_pipeline(VkPipelineLayout, VkShaderModule cs) {
    VkPipeline p = reinterpret_cast<VkPipeline>(mtl::create_compute_pipeline(reinterpret_cast<const char*>(cs)));
    if (p != VK_NULL_HANDLE) g_compute_pipes.insert(p);
    return p;
}

const GpuImage& white_texture() { return g_white; }
const GpuImage& white_texture_array() { return g_white_array; }
const GpuImage& black_uint_texture() { return g_black_uint; }
const GpuImage& dummy_depth_2d() { return g_dummy_d2d; }
const GpuImage& dummy_depth_array() { return g_dummy_darray; }
const GpuImage& dummy_depth_cube_array() { return g_dummy_dcube; }

void register_shutdown_callback(void (*fn)()) { mtl::register_shutdown_callback(fn); }

}  // namespace rac1::vk
