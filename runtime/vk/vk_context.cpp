#define VOLK_IMPLEMENTATION
#include <runtime/vk/vk_context.h>
#include <runtime/vk/vk_shaders.h>
#include <profiler/host_profiler.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <cstdlib>
#include <cstring>
#include <tuple>
#include <vector>

namespace rac1::vk {

namespace {

constexpr VkDeviceSize kRingBufferSize = 32ull * 1024ull * 1024ull;  // 32 MB per frame (a frame used about 6 at 3840x1080; more: a buffer of its own, freed with the frame)

struct SamplerKey {
    VkFilter min_f;
    VkFilter mag_f;
    VkSamplerAddressMode wrap_s;
    VkSamplerAddressMode wrap_t;
    bool compare;
    VkCompareOp compare_op;
    int aniso_x10;
    VkSamplerMipmapMode mipmap_mode;
    int max_lod_x100;
    bool operator==(const SamplerKey& o) const {
        return std::tie(min_f, mag_f, wrap_s, wrap_t, compare, compare_op, aniso_x10, mipmap_mode, max_lod_x100) ==
               std::tie(o.min_f, o.mag_f, o.wrap_s, o.wrap_t, o.compare, o.compare_op, o.aniso_x10, o.mipmap_mode, o.max_lod_x100);
    }
};

struct FrameData {
    VkCommandPool cmd_pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkSemaphore image_available = VK_NULL_HANDLE;
    VkSemaphore render_finished = VK_NULL_HANDLE;
    VkFence in_flight = VK_NULL_HANDLE;
    GpuBuffer ring_buf{};
    VkDeviceSize ring_offset = 0;
    std::vector<VkDescriptorPool> desc_pools;
    size_t active_pool_idx = 0;
    std::vector<GpuBuffer> deferred_buffers;
    std::vector<GpuImage> deferred_images;
};

struct Context {
    bool active = false;
    bool headless = false;
    bool vsync = true;
    GLFWwindow* window = nullptr;

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice phys_dev = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t queue_family = 0;
    VkQueue queue = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties mem_props{};
    VkPhysicalDeviceProperties dev_props{};
    bool has_depth_clip_control = false;
    bool has_provoking_vertex = false;

    VkFormat swap_format = VK_FORMAT_B8G8R8A8_UNORM;
    VkFormat depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swap_images;
    // the submit's "rendering finished" semaphore, one per swapchain image: the present of an image may still wait on its
    // semaphore when the frame slot comes round again, so a semaphore per frame slot could be signalled again too early
    std::vector<VkSemaphore> swap_render_finished;
    std::vector<VkImageView> swap_views;
    int swap_w = 0;
    int swap_h = 0;
    uint32_t swap_img_idx = 0;
    bool swapchain_dirty = false;
    bool swap_image_acquired = false;

    GpuImage main_color{};
    GpuImage main_depth{};
    GpuImage scaled_color{};
    GpuImage imgui_color{};  // (render_imgui's flipped copy of the frame: its own, not the render scale's scaled_color)
    GpuImage scaled_depth{};
    bool scaled_active = false;

    FrameData frames[kFramesInFlight]{};
    uint32_t frame_idx = 0;
    uint64_t frame_num = 0;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    bool in_frame = false;
    bool rendering_active = false;
    ImDrawData* imgui_draw_data = nullptr;

    VkCommandPool imm_pool = VK_NULL_HANDLE;
    VkCommandBuffer imm_cmd = VK_NULL_HANDLE;
    VkFence imm_fence = VK_NULL_HANDLE;

    VkDescriptorPool imgui_pool = VK_NULL_HANDLE;

    std::vector<RenderTarget> target_stack;
    int vp[4] = {0, 0, 1, 1};
    float vp_min_d = 0.0f;
    float vp_max_d = 1.0f;
    bool scissor_enabled = false;
    int scissor_rect[4] = {0, 0, 1, 1};

    std::vector<std::pair<SamplerKey, VkSampler>> samplers;
    GpuImage white_tex{};
    GpuImage white_tex_array{};
    GpuImage black_uint_tex{};
    GpuImage dummy_d2d{};
    GpuImage dummy_darray{};
    GpuImage dummy_dcube{};

    std::vector<void (*)()> shutdown_callbacks;

    // the profiler's GPU timestamps (prof::GpuApi): one pool, `prof_per_slot` a frame slot; a timestamp asked for while no
    // command buffer records (between a frame's present and the next one's start) is written at the next one's start
    VkQueryPool prof_pool = VK_NULL_HANDLE;
    int prof_per_slot = 0;
    double ts_period_ns = 1.0;
    uint64_t ts_mask = ~0ull;
    std::vector<uint32_t> prof_pending;
    std::vector<uint8_t> prof_deferred;  // (per query: written late, at the next frame's start)
    bool has_memory_budget = false;
    VkDeviceSize ring_peak = 0;  // (the most of a frame ring a frame used: the profiler's memory view)
    bool has_host_query_reset = false;
    bool has_float64 = false;       // (shaderFloat64: the tfrag compute's doubles; without it the tfrags are made on the CPU)
    bool has_anisotropy = false;    // (samplerAnisotropy: without it every sampler is isotropic)
    bool device_lost = false;       // (VK_ERROR_DEVICE_LOST seen: said once in the log)
    VkQueryPool timed_pool = VK_NULL_HANDLE;  // (timed_submit's two timestamps)
} g_ctx;

// The renderer's allocations, by use (the profiler's memory view)
struct AllocInfo {
    const char* use;
    bool image;
    VkDeviceSize bytes;
};
std::mutex g_alloc_mutex;
std::map<VkDeviceMemory, AllocInfo> g_allocs;
void alloc_note(VkDeviceMemory m, const char* use, bool image, VkDeviceSize bytes) {
    std::lock_guard<std::mutex> lk(g_alloc_mutex);
    g_allocs[m] = AllocInfo{use, image, bytes};
}
void alloc_forget(VkDeviceMemory m) {
    std::lock_guard<std::mutex> lk(g_alloc_mutex);
    g_allocs.erase(m);
}

VkDescriptorPool create_transient_pool() {
    VkDescriptorPoolSize sizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16384},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8192},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8192},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4096},
        {VK_DESCRIPTOR_TYPE_SAMPLER, 2048},
    };
    VkDescriptorPoolCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.maxSets = 16384;
    ci.poolSizeCount = static_cast<uint32_t>(std::size(sizes));
    ci.pPoolSizes = sizes;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    vkCreateDescriptorPool(g_ctx.device, &ci, nullptr, &pool);
    return pool;
}

void destroy_swap_semaphores() {
    for (VkSemaphore sem : g_ctx.swap_render_finished)
        if (sem != VK_NULL_HANDLE) vkDestroySemaphore(g_ctx.device, sem, nullptr);
    g_ctx.swap_render_finished.clear();
}

void destroy_swapchain() {
    destroy_swap_semaphores();
    for (VkImageView v : g_ctx.swap_views) {
        if (v != VK_NULL_HANDLE) vkDestroyImageView(g_ctx.device, v, nullptr);
    }
    g_ctx.swap_views.clear();
    g_ctx.swap_images.clear();
    if (g_ctx.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(g_ctx.device, g_ctx.swapchain, nullptr);
        g_ctx.swapchain = VK_NULL_HANDLE;
    }
}

bool recreate_swapchain(int w, int h) {
    if (g_ctx.headless || g_ctx.surface == VK_NULL_HANDLE) return true;
    vkDeviceWaitIdle(g_ctx.device);

    VkSurfaceCapabilitiesKHR caps{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_ctx.phys_dev, g_ctx.surface, &caps) != VK_SUCCESS) return false;

    uint32_t fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_ctx.phys_dev, g_ctx.surface, &fmt_count, nullptr);
    std::vector<VkSurfaceFormatKHR> fmts(fmt_count);
    if (fmt_count > 0) vkGetPhysicalDeviceSurfaceFormatsKHR(g_ctx.phys_dev, g_ctx.surface, &fmt_count, fmts.data());

    VkSurfaceFormatKHR chosen_fmt = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    if (!fmts.empty()) {
        chosen_fmt = fmts[0];
        for (const auto& f : fmts) {
            if ((f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM) &&
                f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                chosen_fmt = f;
                break;
            }
        }
    }
    g_ctx.swap_format = chosen_fmt.format;

    uint32_t pm_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_ctx.phys_dev, g_ctx.surface, &pm_count, nullptr);
    std::vector<VkPresentModeKHR> pms(pm_count);
    if (pm_count > 0) vkGetPhysicalDeviceSurfacePresentModesKHR(g_ctx.phys_dev, g_ctx.surface, &pm_count, pms.data());

    // On Wayland, FIFO blocks when the window is occluded; host_swap_interval in orig_main.cpp usesHostPacer on Wayland
    const bool wayland = glfwGetPlatform() == GLFW_PLATFORM_WAYLAND;
    const bool want_vsync = g_ctx.vsync && !wayland;
    VkPresentModeKHR chosen_pm = VK_PRESENT_MODE_FIFO_KHR;
    if (!want_vsync) {
        for (VkPresentModeKHR pm : pms) {
            if (pm == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                chosen_pm = pm;
                break;
            }
            if (pm == VK_PRESENT_MODE_MAILBOX_KHR) chosen_pm = pm;
        }
    }

    VkExtent2D extent{};
    if (caps.currentExtent.width != 0xFFFFFFFFu) {
        extent = caps.currentExtent;
    } else {
        extent.width = std::clamp(static_cast<uint32_t>(w), caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(h), caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    if (extent.width == 0 || extent.height == 0) return false;

    uint32_t img_count = std::max(caps.minImageCount + 1, 3u);
    if (caps.maxImageCount > 0 && img_count > caps.maxImageCount) img_count = caps.maxImageCount;

    VkSwapchainKHR old_swap = g_ctx.swapchain;
    VkSwapchainCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = g_ctx.surface;
    sci.minImageCount = img_count;
    sci.imageFormat = chosen_fmt.format;
    sci.imageColorSpace = chosen_fmt.colorSpace;
    sci.imageExtent = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = chosen_pm;
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = old_swap;

    VkSwapchainKHR new_swap = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(g_ctx.device, &sci, nullptr, &new_swap) != VK_SUCCESS) return false;

    for (VkImageView v : g_ctx.swap_views) {
        if (v != VK_NULL_HANDLE) vkDestroyImageView(g_ctx.device, v, nullptr);
    }
    g_ctx.swap_views.clear();
    if (old_swap != VK_NULL_HANDLE) vkDestroySwapchainKHR(g_ctx.device, old_swap, nullptr);

    g_ctx.swapchain = new_swap;
    g_ctx.swap_w = static_cast<int>(extent.width);
    g_ctx.swap_h = static_cast<int>(extent.height);
    g_ctx.swapchain_dirty = false;

    uint32_t sc_count = 0;
    vkGetSwapchainImagesKHR(g_ctx.device, g_ctx.swapchain, &sc_count, nullptr);
    g_ctx.swap_images.resize(sc_count);
    vkGetSwapchainImagesKHR(g_ctx.device, g_ctx.swapchain, &sc_count, g_ctx.swap_images.data());

    g_ctx.swap_views.resize(sc_count, VK_NULL_HANDLE);
    for (uint32_t i = 0; i < sc_count; ++i) {
        VkImageViewCreateInfo vci{};
        vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vci.image = g_ctx.swap_images[i];
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = g_ctx.swap_format;
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vci.subresourceRange.baseMipLevel = 0;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.baseArrayLayer = 0;
        vci.subresourceRange.layerCount = 1;
        vkCreateImageView(g_ctx.device, &vci, nullptr, &g_ctx.swap_views[i]);
    }
    destroy_swap_semaphores();  // (the device is idle: waited for above)
    g_ctx.swap_render_finished.assign(sc_count, VK_NULL_HANDLE);
    VkSemaphoreCreateInfo semci{};
    semci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (uint32_t i = 0; i < sc_count; ++i) vkCreateSemaphore(g_ctx.device, &semci, nullptr, &g_ctx.swap_render_finished[i]);
    return true;
}

void ensure_main_targets(int w, int h) {
    w = std::max(1, w);
    h = std::max(1, h);
    if (g_ctx.main_color.image != VK_NULL_HANDLE &&
        g_ctx.main_color.width == static_cast<uint32_t>(w) &&
        g_ctx.main_color.height == static_cast<uint32_t>(h)) {
        return;
    }
    destroy_image(g_ctx.main_color);
    destroy_image(g_ctx.main_depth);

    create_image(static_cast<uint32_t>(w), static_cast<uint32_t>(h), VK_FORMAT_R8G8B8A8_UNORM,
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                 VK_IMAGE_ASPECT_COLOR_BIT, g_ctx.main_color);
    create_image(static_cast<uint32_t>(w), static_cast<uint32_t>(h), g_ctx.depth_format,
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                 VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, g_ctx.main_depth);
}

void apply_viewport_and_scissor() {
    if (g_ctx.cmd == VK_NULL_HANDLE || g_ctx.target_stack.empty()) return;
    const RenderTarget& rt = g_ctx.target_stack.back();
    VkViewport vp{};
    vp.x = static_cast<float>(g_ctx.vp[0]);
    vp.y = static_cast<float>(g_ctx.vp[1]);
    vp.width = static_cast<float>(std::max(1, g_ctx.vp[2]));
    vp.height = static_cast<float>(std::max(1, g_ctx.vp[3]));
    vp.minDepth = g_ctx.vp_min_d;
    vp.maxDepth = g_ctx.vp_max_d;
    vkCmdSetViewport(g_ctx.cmd, 0, 1, &vp);

    VkRect2D sc{};
    if (g_ctx.scissor_enabled) {
        const int x0 = std::clamp(g_ctx.scissor_rect[0], 0, static_cast<int>(rt.width));
        const int y0 = std::clamp(g_ctx.scissor_rect[1], 0, static_cast<int>(rt.height));
        const int x1 = std::clamp(g_ctx.scissor_rect[0] + std::max(0, g_ctx.scissor_rect[2]), x0, static_cast<int>(rt.width));
        const int y1 = std::clamp(g_ctx.scissor_rect[1] + std::max(0, g_ctx.scissor_rect[3]), y0, static_cast<int>(rt.height));
        sc.offset = {x0, y0};
        sc.extent = {static_cast<uint32_t>(x1 - x0), static_cast<uint32_t>(y1 - y0)};
    } else {
        sc.offset = {0, 0};
        sc.extent = {rt.width, rt.height};
    }
    vkCmdSetScissor(g_ctx.cmd, 0, 1, &sc);
}

void begin_rendering_internal(const RenderTarget& rt, bool clear_color, const float rgba[4],
                              bool clear_depth, float depth_val, bool clear_stencil, uint32_t stencil_val) {
    if (g_ctx.rendering_active) end_rendering();

    VkRenderingAttachmentInfo color_att{};
    if (rt.color_view != VK_NULL_HANDLE) {
        color_att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_att.imageView = rt.color_view;
        color_att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_att.loadOp = clear_color ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        color_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        if (clear_color && rgba != nullptr) {
            color_att.clearValue.color = {{rgba[0], rgba[1], rgba[2], rgba[3]}};
        }
    }

    VkRenderingAttachmentInfo depth_att{};
    if (rt.depth_view != VK_NULL_HANDLE) {
        depth_att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_att.imageView = rt.depth_view;
        depth_att.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_att.loadOp = clear_depth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        depth_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth_att.clearValue.depthStencil = {depth_val, stencil_val};
    }

    VkRenderingAttachmentInfo stencil_att{};
    if (rt.stencil_view != VK_NULL_HANDLE) {
        stencil_att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        stencil_att.imageView = rt.stencil_view;
        stencil_att.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        stencil_att.loadOp = clear_stencil ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        stencil_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        stencil_att.clearValue.depthStencil = {depth_val, stencil_val};
    }

    VkRenderingInfo ri{};
    ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    ri.renderArea = {{0, 0}, {rt.width, rt.height}};
    ri.layerCount = 1;
    if (rt.color_view != VK_NULL_HANDLE) {
        ri.colorAttachmentCount = 1;
        ri.pColorAttachments = &color_att;
    }
    if (rt.depth_view != VK_NULL_HANDLE) ri.pDepthAttachment = &depth_att;
    if (rt.stencil_view != VK_NULL_HANDLE) ri.pStencilAttachment = &stencil_att;

    vkCmdBeginRendering(g_ctx.cmd, &ri);
    g_ctx.rendering_active = true;
    apply_viewport_and_scissor();
    const float bc[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    vkCmdSetBlendConstants(g_ctx.cmd, bc);
}

void create_fallback_textures() {
    create_image(1, 1, VK_FORMAT_R8G8B8A8_UNORM,
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                 VK_IMAGE_ASPECT_COLOR_BIT, g_ctx.white_tex);
    create_image(1, 1, VK_FORMAT_R8G8B8A8_UNORM,
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                 VK_IMAGE_ASPECT_COLOR_BIT, g_ctx.white_tex_array, 1, 0, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
    create_image(1, 1, VK_FORMAT_R32G32B32A32_UINT,
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                 VK_IMAGE_ASPECT_COLOR_BIT, g_ctx.black_uint_tex);
    create_image(4, 4, VK_FORMAT_D32_SFLOAT,
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                 VK_IMAGE_ASPECT_DEPTH_BIT, g_ctx.dummy_d2d);
    create_image(4, 4, VK_FORMAT_D32_SFLOAT,
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                 VK_IMAGE_ASPECT_DEPTH_BIT, g_ctx.dummy_darray, 3, 0, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
    create_image(4, 4, VK_FORMAT_D32_SFLOAT,
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                 VK_IMAGE_ASPECT_DEPTH_BIT, g_ctx.dummy_dcube, 6, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                 VK_IMAGE_VIEW_TYPE_CUBE_ARRAY);

    GpuBuffer stage{};
    const uint32_t white_And_zero[8] = {0xFFFFFFFFu, 0, 0, 0, 0, 0, 0, 0};
    create_buffer(sizeof(white_And_zero), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stage, true);
    std::memcpy(stage.mapped, white_And_zero, sizeof(white_And_zero));

    submit_immediate([&](VkCommandBuffer cb) {
        for (GpuImage* wimg : {&g_ctx.white_tex, &g_ctx.white_tex_array}) {
            transition_image(cb, *wimg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            VkBufferImageCopy c0{};
            c0.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            c0.imageExtent = {1, 1, 1};
            vkCmdCopyBufferToImage(cb, stage.buffer, wimg->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &c0);
            transition_image(cb, *wimg, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        transition_image(cb, g_ctx.black_uint_tex, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy c1{};
        c1.bufferOffset = 16;
        c1.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        c1.imageExtent = {1, 1, 1};
        vkCmdCopyBufferToImage(cb, stage.buffer, g_ctx.black_uint_tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &c1);
        transition_image(cb, g_ctx.black_uint_tex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        for (GpuImage* dimg : {&g_ctx.dummy_d2d, &g_ctx.dummy_darray, &g_ctx.dummy_dcube}) {
            transition_image(cb, *dimg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            VkClearDepthStencilValue dv = {1.0f, 0};
            VkImageSubresourceRange sr = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, dimg->layers};
            vkCmdClearDepthStencilImage(cb, dimg->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &dv, 1, &sr);
            transition_image(cb, *dimg, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    });
    destroy_buffer_now(stage);
}

// ---- the profiler's GPU side (prof::GpuApi) ----
bool prof_alloc(int slots, int per_slot) {
    if (g_ctx.device == VK_NULL_HANDLE || !g_ctx.has_host_query_reset || g_ctx.dev_props.limits.timestampComputeAndGraphics != VK_TRUE) return false;
    if (g_ctx.prof_pool != VK_NULL_HANDLE) return true;
    VkQueryPoolCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    qci.queryType = VK_QUERY_TYPE_TIMESTAMP;
    qci.queryCount = static_cast<uint32_t>(slots * per_slot);
    if (vkCreateQueryPool(g_ctx.device, &qci, nullptr, &g_ctx.prof_pool) != VK_SUCCESS) return false;
    vkResetQueryPool(g_ctx.device, g_ctx.prof_pool, 0, qci.queryCount);
    g_ctx.prof_per_slot = per_slot;
    g_ctx.prof_deferred.assign(qci.queryCount, 0);
    return true;
}
void prof_slot_reset(int slot) {
    if (g_ctx.prof_pool == VK_NULL_HANDLE) return;
    const uint32_t first = static_cast<uint32_t>(slot * g_ctx.prof_per_slot);
    // (a timestamp of this slot still waiting for a command buffer: dropped with the slot)
    std::erase_if(g_ctx.prof_pending, [&](uint32_t q) { return q >= first && q < first + static_cast<uint32_t>(g_ctx.prof_per_slot); });
    vkResetQueryPool(g_ctx.device, g_ctx.prof_pool, first, static_cast<uint32_t>(g_ctx.prof_per_slot));
}
void prof_write(int slot, int q) {
    if (g_ctx.prof_pool == VK_NULL_HANDLE) return;
    const uint32_t id = static_cast<uint32_t>(slot * g_ctx.prof_per_slot + q);
    const bool now = g_ctx.in_frame && g_ctx.cmd != VK_NULL_HANDLE;
    if (now) vkCmdWriteTimestamp(g_ctx.cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, g_ctx.prof_pool, id);
    else g_ctx.prof_pending.push_back(id);
    g_ctx.prof_deferred[id] = now ? 0 : 1;
}
bool prof_read(int slot, int n, uint64_t* ns) {
    if (g_ctx.prof_pool == VK_NULL_HANDLE || n <= 0) return false;
    std::vector<uint64_t> v(static_cast<size_t>(n) * 2);
    const uint32_t first = static_cast<uint32_t>(slot * g_ctx.prof_per_slot);
    // (the last one first: the others were written before it)
    uint64_t last[2] = {};
    if (vkGetQueryPoolResults(g_ctx.device, g_ctx.prof_pool, first + static_cast<uint32_t>(n - 1), 1, sizeof last, last, sizeof last,
                              VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) != VK_SUCCESS || last[1] == 0)
        return false;
    vkGetQueryPoolResults(g_ctx.device, g_ctx.prof_pool, first, static_cast<uint32_t>(n), v.size() * 8, v.data(), 16,
                          VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
    bool in_frame_seen = false;
    for (int i = 0; i < n; ++i) {
        // (one not written - its command buffer never submitted -: the one before's, an empty segment. One asked for after the
        // frame's present, written at the next frame's start: the one before's too - the GPU's idle wait for the next frame is
        // not a stage's, as with OpenGL, whose queries after the swap run when the GPU reaches them)
        const bool there = v[static_cast<size_t>(i) * 2 + 1] != 0;
        const bool late = g_ctx.prof_deferred[first + static_cast<uint32_t>(i)] != 0;
        const uint64_t t = v[static_cast<size_t>(i) * 2] & g_ctx.ts_mask;
        ns[i] = there && !(late && in_frame_seen) ? static_cast<uint64_t>(static_cast<double>(t) * g_ctx.ts_period_ns) : (i > 0 ? ns[i - 1] : 0);
        if (!late) in_frame_seen = true;
    }
    return true;
}
double prof_now_ms() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();  // (no calibrated clock: the trace's GPU line approximate)
}
void prof_resources(prof::Resources* r) {
    r->api = "Vulkan";
    r->gl_renderer = g_ctx.dev_props.deviceName;
    char drv[64];
    std::snprintf(drv, sizeof drv, "Vulkan %u.%u, driver 0x%x", VK_API_VERSION_MAJOR(g_ctx.dev_props.apiVersion), VK_API_VERSION_MINOR(g_ctx.dev_props.apiVersion),
                  g_ctx.dev_props.driverVersion);
    r->gl_vendor = drv;
    r->vk_budget = false;
    if (!g_ctx.has_memory_budget || g_ctx.phys_dev == VK_NULL_HANDLE) return;
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget{};
    budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    VkPhysicalDeviceMemoryProperties2 mp{};
    mp.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    mp.pNext = &budget;
    vkGetPhysicalDeviceMemoryProperties2(g_ctx.phys_dev, &mp);
    r->vk_heap_size = r->vk_heap_budget = r->vk_heap_usage = 0;
    for (uint32_t i = 0; i < mp.memoryProperties.memoryHeapCount; ++i) {
        if ((mp.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0) continue;
        r->vk_heap_size += mp.memoryProperties.memoryHeaps[i].size;
        r->vk_heap_budget += budget.heapBudget[i];
        r->vk_heap_usage += budget.heapUsage[i];
    }
    r->vk_budget = true;
}
void prof_scan(std::vector<prof::GlOwnerRow>* out) {
    std::map<std::string, prof::GlOwnerRow> rows;
    {
        std::lock_guard<std::mutex> lk(g_alloc_mutex);
        for (const auto& [m, a] : g_allocs) {
            prof::GlOwnerRow& r = rows[a.use];
            r.owner = a.use;
            if (a.image) {
                ++r.textures;
                r.tex_bytes += a.bytes;
            } else {
                ++r.buffers;
                r.buf_bytes += a.bytes;
            }
        }
    }
    for (auto& [k, r] : rows) {
        if (k.rfind("frame rings", 0) == 0) {
            char b[96];
            std::snprintf(b, sizeof b, "frame rings (vertices, uniforms, uploads; peak use %.1f MB a frame)", static_cast<double>(g_ctx.ring_peak) / (1024.0 * 1024.0));
            r.owner = b;
        }
        out->push_back(std::move(r));
    }
    std::sort(out->begin(), out->end(), [](const prof::GlOwnerRow& a, const prof::GlOwnerRow& b) { return a.tex_bytes + a.buf_bytes > b.tex_bytes + b.buf_bytes; });
}
const prof::GpuApi kProfApi = {"Vulkan", prof_alloc, prof_slot_reset, prof_write, prof_read, prof_now_ms, prof_resources, prof_scan};

// orig: (host) the device's features, limits and formats against what the renderer uses, each missing one said in the log
// ("[vk] missing: ..."): false when one the renderer cannot do without is missing (the game then starts with OpenGL, as
// when Vulkan is not there), and the optional ones turned off (g_ctx.has_*). GitHub issue #113: a level's first frame
// black on an Intel UHD 620, with nothing in the log to say why.
template <typename HasExt>
bool check_device(VkPhysicalDevice pd, const HasExt& has_ext) {
    VkPhysicalDeviceVulkan13Features f13{};
    f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    VkPhysicalDeviceVulkan12Features f12{};
    f12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    f12.pNext = &f13;
    VkPhysicalDeviceFeatures2 f2{};
    f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    f2.pNext = &f12;
    vkGetPhysicalDeviceFeatures2(pd, &f2);

    VkPhysicalDeviceSubgroupProperties sg{};
    sg.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
    VkPhysicalDeviceProperties2 p2{};
    p2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    p2.pNext = &sg;
    vkGetPhysicalDeviceProperties2(pd, &p2);
    const VkPhysicalDeviceLimits& L = p2.properties.limits;

    std::string missing, off;
    auto need = [&](bool ok, const char* what) {
        if (!ok) missing += std::string(missing.empty() ? "" : ", ") + what;
    };
    const VkPhysicalDeviceFeatures& f = f2.features;
    need(f13.dynamicRendering == VK_TRUE, "dynamicRendering");
    need(f13.synchronization2 == VK_TRUE, "synchronization2");
    need(f12.scalarBlockLayout == VK_TRUE, "scalarBlockLayout");
    need(f.imageCubeArray == VK_TRUE, "imageCubeArray (the point lights' shadows)");
    need(f.multiDrawIndirect == VK_TRUE, "multiDrawIndirect");
    need(f.drawIndirectFirstInstance == VK_TRUE, "drawIndirectFirstInstance");
    need(f.depthClamp == VK_TRUE, "depthClamp");
    need(f.depthBiasClamp == VK_TRUE, "depthBiasClamp");
    need(f.shaderClipDistance == VK_TRUE, "shaderClipDistance");
    need(f.dualSrcBlend == VK_TRUE, "dualSrcBlend");
    need(f.shaderSampledImageArrayDynamicIndexing == VK_TRUE, "shaderSampledImageArrayDynamicIndexing");
    // (the world's fragment shader picks its texture with subgroupBroadcastFirst)
    need((sg.supportedStages & VK_SHADER_STAGE_FRAGMENT_BIT) != 0 && (sg.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT) != 0,
         "subgroup ballot in fragment shaders");
    // (the largest push constants a pass uses: the 2D's, the world's; the occlusion's parameters are in a uniform buffer)
    char buf[160];
    std::snprintf(buf, sizeof buf, "maxPushConstantsSize %u (128 needed)", L.maxPushConstantsSize);
    need(L.maxPushConstantsSize >= 128, buf);
    std::snprintf(buf, sizeof buf, "maxPerStageDescriptorSamplers %u / SampledImages %u (24 needed)", L.maxPerStageDescriptorSamplers,
                  L.maxPerStageDescriptorSampledImages);
    need(L.maxPerStageDescriptorSamplers >= 24 && L.maxPerStageDescriptorSampledImages >= 24, buf);
    std::snprintf(buf, sizeof buf, "maxComputeWorkGroupInvocations %u (64 needed)", L.maxComputeWorkGroupInvocations);
    need(L.maxComputeWorkGroupInvocations >= 64 && L.maxComputeWorkGroupSize[0] >= 64, buf);
    need(L.maxDrawIndirectCount > 1, "maxDrawIndirectCount > 1");

    // the formats the targets are made in: colour (drawn into, blended, sampled) and depth (drawn into, sampled)
    auto fmt_ok = [&](VkFormat fmt, VkFormatFeatureFlags want) {
        VkFormatProperties fp{};
        vkGetPhysicalDeviceFormatProperties(pd, fmt, &fp);
        return (fp.optimalTilingFeatures & want) == want;
    };
    const VkFormatFeatureFlags colour = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
                                        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    const VkFormatFeatureFlags depth = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    need(fmt_ok(VK_FORMAT_R8G8B8A8_UNORM, colour), "R8G8B8A8_UNORM targets");
    need(fmt_ok(VK_FORMAT_R16G16B16A16_SFLOAT, colour), "R16G16B16A16_SFLOAT targets");
    need(fmt_ok(VK_FORMAT_R8_UNORM, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT), "R8_UNORM targets");
    need(fmt_ok(VK_FORMAT_R32_SFLOAT, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT), "R32_SFLOAT targets");
    need(fmt_ok(VK_FORMAT_D32_SFLOAT, depth), "D32_SFLOAT depth (the shadow maps)");
    need(fmt_ok(VK_FORMAT_D32_SFLOAT_S8_UINT, depth) || fmt_ok(VK_FORMAT_D24_UNORM_S8_UINT, depth), "a depth-and-stencil format");

    // optional: turned off when missing
    g_ctx.has_float64 = f.shaderFloat64 == VK_TRUE;
    if (!g_ctx.has_float64) off += "shaderFloat64 (the tfrags made on the CPU); ";
    g_ctx.has_anisotropy = f.samplerAnisotropy == VK_TRUE;
    if (!g_ctx.has_anisotropy) off += "samplerAnisotropy (no anisotropic filtering); ";
    if (!has_ext(VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME)) off += "VK_EXT_provoking_vertex; ";
    if (!has_ext(VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME)) off += "VK_EXT_depth_clip_control; ";

    std::fprintf(stderr, "[vk] %s, Vulkan %u.%u.%u, driver 0x%x (vendor 0x%04x, device 0x%04x): push constants %u bytes, subgroup %u\n",
                 p2.properties.deviceName, VK_API_VERSION_MAJOR(p2.properties.apiVersion), VK_API_VERSION_MINOR(p2.properties.apiVersion),
                 VK_API_VERSION_PATCH(p2.properties.apiVersion), p2.properties.driverVersion, p2.properties.vendorID, p2.properties.deviceID,
                 L.maxPushConstantsSize, sg.subgroupSize);
    if (!off.empty()) {
        off.resize(off.size() - 2);
        std::fprintf(stderr, "[vk] not there, turned off: %s\n", off.c_str());
    }
    if (!missing.empty()) {
        std::fprintf(stderr, "[vk] missing: %s. The Vulkan renderer needs them: OpenGL instead\n", missing.c_str());
        return false;
    }
    return true;
}

// orig: (host) a VkResult of the queue or the swapchain: VK_ERROR_DEVICE_LOST said once (the driver stopped the GPU's work:
// what follows draws nothing, the picture stays black or frozen while the game runs on)
void check_lost(VkResult r, const char* where) {
    if (r != VK_ERROR_DEVICE_LOST || g_ctx.device_lost) return;
    g_ctx.device_lost = true;
    std::fprintf(stderr, "[vk] the graphics driver stopped the GPU's work (VK_ERROR_DEVICE_LOST, at %s): nothing more can be drawn. "
                         "Restart the game; if it happens again, set Renderer to OpenGL\n", where);
}

}  // namespace

// orig: (host)
bool has_float64() { return g_ctx.has_float64; }

// orig: (host) by the PCI device ID's high byte, the families Intel's own tables (and Mesa's) give generations 7 to 9.5:
// Ivy Bridge 0x01, Haswell 0x04 / 0x0a / 0x0c / 0x0d, Bay Trail 0x0f, Broadwell 0x16, Cherry / Braswell 0x22, Skylake 0x19,
// Gemini Lake 0x31, Coffee / Whiskey Lake 0x3e, Kaby Lake 0x59, Apollo Lake 0x5a, Amber Lake 0x87, Comet Lake 0x9b
bool float64_slow() {
    const VkPhysicalDeviceProperties& p = g_ctx.dev_props;
    if (p.vendorID != 0x8086) return false;
    switch ((p.deviceID >> 8) & 0xff) {
    case 0x01: case 0x04: case 0x0a: case 0x0c: case 0x0d: case 0x0f: case 0x16: case 0x19: case 0x22: case 0x31: case 0x3e:
    case 0x59: case 0x5a: case 0x87: case 0x9b:
        return true;
    default:
        return false;
    }
}

bool init(GLFWwindow* window, bool vsync, bool headless) {
    if (g_ctx.active) return true;
    g_ctx = Context{};
    g_ctx.window = window;
    g_ctx.vsync = vsync;
    g_ctx.headless = headless;
    if (!shaders::kAvailable) {
        std::fprintf(stderr, "[vk] this build has no Vulkan shaders (no glslc / glslangValidator when it was built)\n");
        return false;
    }

    if (volkInitialize() != VK_SUCCESS) {
        std::fprintf(stderr, "[vk] volkInitialize failed (no Vulkan loader found)\n");
        return false;
    }
    if (volkGetInstanceVersion() < VK_API_VERSION_1_3) {
        std::fprintf(stderr, "[vk] Vulkan 1.3 not supported by loader\n");
        return false;
    }

    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "rac1-native";
    app.applicationVersion = VK_MAKE_VERSION(0, 4, 0);
    app.pEngineName = "rac1-native";
    app.engineVersion = VK_MAKE_VERSION(0, 4, 0);
    app.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> inst_exts;
    if (!headless && window != nullptr) {
        uint32_t glfw_ext_count = 0;
        const char** glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
        if (glfw_exts == nullptr || glfw_ext_count == 0) {
            std::fprintf(stderr, "[vk] glfwGetRequiredInstanceExtensions failed\n");
            return false;
        }
        for (uint32_t i = 0; i < glfw_ext_count; ++i) inst_exts.push_back(glfw_exts[i]);
    }

    std::vector<const char*> layers;
    if (const char* val = std::getenv("RAC1_VK_VALIDATION"); val != nullptr && std::atoi(val) != 0) {
        uint32_t lc = 0;
        vkEnumerateInstanceLayerProperties(&lc, nullptr);
        std::vector<VkLayerProperties> lps(lc);
        vkEnumerateInstanceLayerProperties(&lc, lps.data());
        for (const auto& lp : lps) {
            if (std::strcmp(lp.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                layers.push_back("VK_LAYER_KHRONOS_validation");
                break;
            }
        }
    }

    VkInstanceCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = static_cast<uint32_t>(inst_exts.size());
    ici.ppEnabledExtensionNames = inst_exts.data();
    ici.enabledLayerCount = static_cast<uint32_t>(layers.size());
    ici.ppEnabledLayerNames = layers.data();

    if (vkCreateInstance(&ici, nullptr, &g_ctx.instance) != VK_SUCCESS) {
        std::fprintf(stderr, "[vk] vkCreateInstance failed\n");
        return false;
    }
    volkLoadInstance(g_ctx.instance);

    if (!headless && window != nullptr) {
        if (glfwCreateWindowSurface(g_ctx.instance, window, nullptr, &g_ctx.surface) != VK_SUCCESS) {
            std::fprintf(stderr, "[vk] glfwCreateWindowSurface failed\n");
            shutdown();
            return false;
        }
    }

    uint32_t dev_count = 0;
    vkEnumeratePhysicalDevices(g_ctx.instance, &dev_count, nullptr);
    if (dev_count == 0) {
        std::fprintf(stderr, "[vk] no Vulkan physical devices found\n");
        shutdown();
        return false;
    }
    std::vector<VkPhysicalDevice> devs(dev_count);
    vkEnumeratePhysicalDevices(g_ctx.instance, &dev_count, devs.data());

    int best_score = -1;
    for (VkPhysicalDevice pd : devs) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(pd, &props);
        if (props.apiVersion < VK_API_VERSION_1_3) {
            std::fprintf(stderr, "[vk] %s: Vulkan %u.%u, not 1.3: left out\n", props.deviceName, VK_API_VERSION_MAJOR(props.apiVersion),
                         VK_API_VERSION_MINOR(props.apiVersion));
            continue;
        }

        uint32_t qf_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qf_count, nullptr);
        std::vector<VkQueueFamilyProperties> qfs(qf_count);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qf_count, qfs.data());

        uint32_t found_qf = UINT32_MAX;
        for (uint32_t i = 0; i < qf_count; ++i) {
            const bool gfx_comp = (qfs[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) ==
                                  (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
            if (!gfx_comp) continue;
            if (!headless && g_ctx.surface != VK_NULL_HANDLE) {
                VkBool32 present = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, g_ctx.surface, &present);
                if (!present) continue;
            }
            found_qf = i;
            break;
        }
        if (found_qf == UINT32_MAX) continue;

        int score = 1;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score = 100;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score = 50;
        if (score > best_score) {
            best_score = score;
            g_ctx.phys_dev = pd;
            g_ctx.queue_family = found_qf;
            g_ctx.dev_props = props;
        }
    }

    if (g_ctx.phys_dev == VK_NULL_HANDLE) {
        std::fprintf(stderr, "[vk] no suitable Vulkan 1.3 physical device found\n");
        shutdown();
        return false;
    }
    vkGetPhysicalDeviceMemoryProperties(g_ctx.phys_dev, &g_ctx.mem_props);

    uint32_t dext_count = 0;
    vkEnumerateDeviceExtensionProperties(g_ctx.phys_dev, nullptr, &dext_count, nullptr);
    std::vector<VkExtensionProperties> dexts(dext_count);
    vkEnumerateDeviceExtensionProperties(g_ctx.phys_dev, nullptr, &dext_count, dexts.data());
    auto has_ext = [&](const char* name) {
        for (const auto& e : dexts) {
            if (std::strcmp(e.extensionName, name) == 0) return true;
        }
        return false;
    };

    std::vector<const char*> enabled_dexts;
    if (!headless) {
        if (!has_ext(VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
            std::fprintf(stderr, "[vk] device lacks VK_KHR_swapchain\n");
            shutdown();
            return false;
        }
        enabled_dexts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    if (has_ext(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)) {
        enabled_dexts.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    }
    if (has_ext(VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME)) {
        enabled_dexts.push_back(VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME);
        g_ctx.has_depth_clip_control = true;
    }
    if (has_ext(VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME)) {
        enabled_dexts.push_back(VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME);
        g_ctx.has_provoking_vertex = true;
    }
    if (has_ext(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) {  // (the profiler's GPU memory)
        enabled_dexts.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
        g_ctx.has_memory_budget = true;
    }
    {  // (the profiler's timestamps reset from the host: Vulkan 1.2's hostQueryReset)
        VkPhysicalDeviceVulkan12Features f12{};
        f12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        VkPhysicalDeviceFeatures2 f2{};
        f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        f2.pNext = &f12;
        vkGetPhysicalDeviceFeatures2(g_ctx.phys_dev, &f2);
        g_ctx.has_host_query_reset = f12.hostQueryReset == VK_TRUE;
    }

    if (!check_device(g_ctx.phys_dev, has_ext)) {
        shutdown();
        return false;
    }

    VkPhysicalDeviceProvokingVertexFeaturesEXT prov_feat{};
    prov_feat.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT;
    prov_feat.provokingVertexLast = VK_TRUE;

    VkPhysicalDeviceDepthClipControlFeaturesEXT clip_feat{};
    clip_feat.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_CONTROL_FEATURES_EXT;
    clip_feat.depthClipControl = VK_TRUE;
    if (g_ctx.has_provoking_vertex) clip_feat.pNext = &prov_feat;

    VkPhysicalDeviceVulkan13Features v13_feat{};
    v13_feat.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    v13_feat.dynamicRendering = VK_TRUE;
    v13_feat.synchronization2 = VK_TRUE;
    if (g_ctx.has_depth_clip_control) v13_feat.pNext = &clip_feat;
    else if (g_ctx.has_provoking_vertex) v13_feat.pNext = &prov_feat;

    VkPhysicalDeviceVulkan12Features v12_feat{};
    v12_feat.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    v12_feat.pNext = &v13_feat;
    v12_feat.scalarBlockLayout = VK_TRUE;
    v12_feat.hostQueryReset = g_ctx.has_host_query_reset ? VK_TRUE : VK_FALSE;

    VkPhysicalDeviceFeatures2 feat2{};
    feat2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    feat2.pNext = &v12_feat;
    feat2.features.samplerAnisotropy = g_ctx.has_anisotropy ? VK_TRUE : VK_FALSE;
    feat2.features.imageCubeArray = VK_TRUE;
    feat2.features.multiDrawIndirect = VK_TRUE;
    feat2.features.drawIndirectFirstInstance = VK_TRUE;
    feat2.features.depthClamp = VK_TRUE;
    feat2.features.depthBiasClamp = VK_TRUE;
    feat2.features.shaderClipDistance = VK_TRUE;
    feat2.features.dualSrcBlend = VK_TRUE;
    feat2.features.shaderFloat64 = g_ctx.has_float64 ? VK_TRUE : VK_FALSE;
    feat2.features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;

    const float q_prio = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = g_ctx.queue_family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &q_prio;

    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.pNext = &feat2;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = static_cast<uint32_t>(enabled_dexts.size());
    dci.ppEnabledExtensionNames = enabled_dexts.data();

    if (const VkResult r = vkCreateDevice(g_ctx.phys_dev, &dci, nullptr, &g_ctx.device); r != VK_SUCCESS) {
        std::fprintf(stderr, "[vk] vkCreateDevice failed (VkResult %d)\n", static_cast<int>(r));
        shutdown();
        return false;
    }
    volkLoadDevice(g_ctx.device);
    vkGetDeviceQueue(g_ctx.device, g_ctx.queue_family, 0, &g_ctx.queue);

    // Choose depth-stencil format
    g_ctx.depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    for (VkFormat cand : {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}) {
        VkFormatProperties fp{};
        vkGetPhysicalDeviceFormatProperties(g_ctx.phys_dev, cand, &fp);
        if ((fp.optimalTilingFeatures & (VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) ==
            (VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
            g_ctx.depth_format = cand;
            break;
        }
    }

    // Immediate command buffer & fence
    {
        VkCommandPoolCreateInfo cpci{};
        cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpci.queueFamilyIndex = g_ctx.queue_family;
        vkCreateCommandPool(g_ctx.device, &cpci, nullptr, &g_ctx.imm_pool);

        VkCommandBufferAllocateInfo cbai{};
        cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbai.commandPool = g_ctx.imm_pool;
        cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbai.commandBufferCount = 1;
        vkAllocateCommandBuffers(g_ctx.device, &cbai, &g_ctx.imm_cmd);

        VkFenceCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vkCreateFence(g_ctx.device, &fci, nullptr, &g_ctx.imm_fence);
    }

    // Per-frame resources
    for (uint32_t i = 0; i < kFramesInFlight; ++i) {
        FrameData& f = g_ctx.frames[i];
        VkCommandPoolCreateInfo cpci{};
        cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpci.queueFamilyIndex = g_ctx.queue_family;
        vkCreateCommandPool(g_ctx.device, &cpci, nullptr, &f.cmd_pool);

        VkCommandBufferAllocateInfo cbai{};
        cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbai.commandPool = f.cmd_pool;
        cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbai.commandBufferCount = 1;
        vkAllocateCommandBuffers(g_ctx.device, &cbai, &f.cmd);

        VkSemaphoreCreateInfo sci{};
        sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        vkCreateSemaphore(g_ctx.device, &sci, nullptr, &f.image_available);
        vkCreateSemaphore(g_ctx.device, &sci, nullptr, &f.render_finished);

        VkFenceCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence(g_ctx.device, &fci, nullptr, &f.in_flight);

        create_buffer(kRingBufferSize,
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, f.ring_buf, true);
        f.desc_pools.push_back(create_transient_pool());
    }

    // ImGui descriptor pool
    {
        VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 128},
        };
        VkDescriptorPoolCreateInfo pci{};
        pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pci.maxSets = 128;
        pci.poolSizeCount = 1;
        pci.pPoolSizes = pool_sizes;
        vkCreateDescriptorPool(g_ctx.device, &pci, nullptr, &g_ctx.imgui_pool);
    }

    int init_w = 960, init_h = 720;
    if (window != nullptr) glfwGetFramebufferSize(window, &init_w, &init_h);
    init_w = std::max(1, init_w);
    init_h = std::max(1, init_h);

    if (!headless && window != nullptr) {
        recreate_swapchain(init_w, init_h);
    }
    ensure_main_targets(init_w, init_h);
    create_fallback_textures();

    g_ctx.ts_period_ns = static_cast<double>(g_ctx.dev_props.limits.timestampPeriod);
    {
        uint32_t qf_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(g_ctx.phys_dev, &qf_count, nullptr);
        std::vector<VkQueueFamilyProperties> qfs(qf_count);
        vkGetPhysicalDeviceQueueFamilyProperties(g_ctx.phys_dev, &qf_count, qfs.data());
        const uint32_t bits = g_ctx.queue_family < qf_count ? qfs[g_ctx.queue_family].timestampValidBits : 64;
        g_ctx.ts_mask = bits >= 64 ? ~0ull : ((1ull << bits) - 1);
    }
    prof::set_gpu_api(&kProfApi);
    g_ctx.active = true;
    std::printf("[vk] Vulkan 1.3 initialized on %s (%dx%d, depth=%d, clip_control=%d)\n",
                g_ctx.dev_props.deviceName, init_w, init_h, static_cast<int>(g_ctx.depth_format),
                g_ctx.has_depth_clip_control ? 1 : 0);
    return true;
}

void shutdown() {
    if (g_ctx.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(g_ctx.device);
        for (auto fn : g_ctx.shutdown_callbacks) {
            if (fn != nullptr) fn();
        }
        g_ctx.shutdown_callbacks.clear();

        for (auto& [k, s] : g_ctx.samplers) {
            if (s != VK_NULL_HANDLE) vkDestroySampler(g_ctx.device, s, nullptr);
        }
        g_ctx.samplers.clear();

        destroy_image_now(g_ctx.white_tex);
        destroy_image_now(g_ctx.white_tex_array);
        destroy_image_now(g_ctx.black_uint_tex);
        destroy_image_now(g_ctx.dummy_d2d);
        destroy_image_now(g_ctx.dummy_darray);
        destroy_image_now(g_ctx.dummy_dcube);
        destroy_image_now(g_ctx.main_color);
        destroy_image_now(g_ctx.main_depth);
        destroy_image_now(g_ctx.scaled_color);
        destroy_image_now(g_ctx.imgui_color);
        destroy_image_now(g_ctx.scaled_depth);

        destroy_swapchain();
        if (g_ctx.prof_pool != VK_NULL_HANDLE) vkDestroyQueryPool(g_ctx.device, g_ctx.prof_pool, nullptr);
        if (g_ctx.timed_pool != VK_NULL_HANDLE) vkDestroyQueryPool(g_ctx.device, g_ctx.timed_pool, nullptr);
        g_ctx.prof_pool = VK_NULL_HANDLE;
        prof::set_gpu_api(nullptr);

        for (uint32_t i = 0; i < kFramesInFlight; ++i) {
            FrameData& f = g_ctx.frames[i];
            for (GpuBuffer& b : f.deferred_buffers) destroy_buffer_now(b);
            f.deferred_buffers.clear();
            for (GpuImage& im : f.deferred_images) destroy_image_now(im);
            f.deferred_images.clear();
            destroy_buffer_now(f.ring_buf);
            for (VkDescriptorPool dp : f.desc_pools) {
                if (dp != VK_NULL_HANDLE) vkDestroyDescriptorPool(g_ctx.device, dp, nullptr);
            }
            f.desc_pools.clear();
            if (f.image_available != VK_NULL_HANDLE) vkDestroySemaphore(g_ctx.device, f.image_available, nullptr);
            if (f.render_finished != VK_NULL_HANDLE) vkDestroySemaphore(g_ctx.device, f.render_finished, nullptr);
            if (f.in_flight != VK_NULL_HANDLE) vkDestroyFence(g_ctx.device, f.in_flight, nullptr);
            if (f.cmd_pool != VK_NULL_HANDLE) vkDestroyCommandPool(g_ctx.device, f.cmd_pool, nullptr);
        }

        if (g_ctx.imgui_pool != VK_NULL_HANDLE) vkDestroyDescriptorPool(g_ctx.device, g_ctx.imgui_pool, nullptr);
        if (g_ctx.imm_fence != VK_NULL_HANDLE) vkDestroyFence(g_ctx.device, g_ctx.imm_fence, nullptr);
        if (g_ctx.imm_pool != VK_NULL_HANDLE) vkDestroyCommandPool(g_ctx.device, g_ctx.imm_pool, nullptr);

        vkDestroyDevice(g_ctx.device, nullptr);
        g_ctx.device = VK_NULL_HANDLE;
    }
    if (g_ctx.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_ctx.instance, g_ctx.surface, nullptr);
        g_ctx.surface = VK_NULL_HANDLE;
    }
    if (g_ctx.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(g_ctx.instance, nullptr);
        g_ctx.instance = VK_NULL_HANDLE;
    }
    g_ctx.active = false;
}

bool is_active() { return g_ctx.active; }

void wait_idle() {
    if (g_ctx.device != VK_NULL_HANDLE) vkDeviceWaitIdle(g_ctx.device);
}

void set_vsync(bool vsync) {
    if (g_ctx.vsync != vsync) {
        g_ctx.vsync = vsync;
        g_ctx.swapchain_dirty = true;
    }
}

void begin_frame(int win_w, int win_h, float clear_r, float clear_g, float clear_b, float clear_a) {
    if (!g_ctx.active) return;
    if (g_ctx.in_frame) end_frame();

    win_w = std::max(1, win_w);
    win_h = std::max(1, win_h);

    FrameData& f = g_ctx.frames[g_ctx.frame_idx];
    {
        // (the frame slot's fence: the GPU still on the frame before last - the driver's queue full, as an OpenGL swap that
        // blocks: the profiler's Present stage, a wait)
        RAC1_PROF_ZONE(kPresent);
        check_lost(vkWaitForFences(g_ctx.device, 1, &f.in_flight, VK_TRUE, UINT64_MAX), "the frame's fence");
    }
    g_ctx.ring_peak = std::max(g_ctx.ring_peak, f.ring_offset);

    for (GpuBuffer& b : f.deferred_buffers) destroy_buffer_now(b);
    f.deferred_buffers.clear();
    for (GpuImage& im : f.deferred_images) destroy_image_now(im);
    f.deferred_images.clear();

    f.ring_offset = 0;
    for (VkDescriptorPool dp : f.desc_pools) vkResetDescriptorPool(g_ctx.device, dp, 0);
    f.active_pool_idx = 0;

    g_ctx.swap_image_acquired = false;
    if (!g_ctx.headless && g_ctx.surface != VK_NULL_HANDLE) {
        if (g_ctx.swapchain == VK_NULL_HANDLE || g_ctx.swapchain_dirty || win_w != g_ctx.swap_w || win_h != g_ctx.swap_h) {
            recreate_swapchain(win_w, win_h);
        }
        if (g_ctx.swapchain != VK_NULL_HANDLE) {
            VkResult res = vkAcquireNextImageKHR(g_ctx.device, g_ctx.swapchain, UINT64_MAX, f.image_available,
                                                 VK_NULL_HANDLE, &g_ctx.swap_img_idx);
            if (res == VK_ERROR_OUT_OF_DATE_KHR) {
                if (recreate_swapchain(win_w, win_h)) {
                    res = vkAcquireNextImageKHR(g_ctx.device, g_ctx.swapchain, UINT64_MAX, f.image_available,
                                                VK_NULL_HANDLE, &g_ctx.swap_img_idx);
                }
            }
            check_lost(res, "vkAcquireNextImageKHR");
            if (res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR) {
                g_ctx.swap_image_acquired = true;
                if (res == VK_SUBOPTIMAL_KHR) g_ctx.swapchain_dirty = true;
            }
        }
    }

    ensure_main_targets(win_w, win_h);

    vkResetCommandPool(g_ctx.device, f.cmd_pool, 0);
    VkCommandBufferBeginInfo cbbi{};
    cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(f.cmd, &cbbi);
    for (uint32_t q : g_ctx.prof_pending) vkCmdWriteTimestamp(f.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, g_ctx.prof_pool, q);
    g_ctx.prof_pending.clear();

    g_ctx.cmd = f.cmd;
    g_ctx.in_frame = true;
    g_ctx.rendering_active = false;
    g_ctx.imgui_draw_data = nullptr;
    g_ctx.scaled_active = false;

    transition_image(g_ctx.cmd, g_ctx.main_color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    transition_image(g_ctx.cmd, g_ctx.main_depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    RenderTarget main_rt{};
    main_rt.color_view = g_ctx.main_color.view;
    main_rt.color_format = g_ctx.main_color.format;
    main_rt.depth_view = g_ctx.main_depth.view;
    main_rt.depth_format = g_ctx.main_depth.format;
    main_rt.stencil_view = g_ctx.main_depth.view;
    main_rt.width = g_ctx.main_color.width;
    main_rt.height = g_ctx.main_color.height;

    g_ctx.target_stack.clear();
    g_ctx.target_stack.push_back(main_rt);

    g_ctx.vp[0] = 0;
    g_ctx.vp[1] = 0;
    g_ctx.vp[2] = win_w;
    g_ctx.vp[3] = win_h;
    g_ctx.vp_min_d = 0.0f;
    g_ctx.vp_max_d = 1.0f;
    g_ctx.scissor_enabled = false;

    const float rgba[4] = {clear_r, clear_g, clear_b, clear_a};
    begin_rendering_internal(main_rt, true, rgba, true, 1.0f, true, 0);
}

void render_imgui(ImDrawData* draw_data) {
    if (!g_ctx.active || !g_ctx.in_frame || draw_data == nullptr) return;
    const int w = static_cast<int>(g_ctx.main_color.width);
    const int h = static_cast<int>(g_ctx.main_color.height);
    if (w <= 0 || h <= 0) return;

    end_rendering();
    if (g_ctx.imgui_color.image == VK_NULL_HANDLE ||
        g_ctx.imgui_color.width != static_cast<uint32_t>(w) ||
        g_ctx.imgui_color.height != static_cast<uint32_t>(h)) {
        destroy_image(g_ctx.imgui_color);
        create_image(static_cast<uint32_t>(w), static_cast<uint32_t>(h), VK_FORMAT_R8G8B8A8_UNORM,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT, g_ctx.imgui_color);
    }

    // Flip main_color (bottom-up) into imgui_color (top-down for ImGui), render ImGui, then flip back
    blit_image(g_ctx.main_color, 0, h, w, 0, g_ctx.imgui_color, 0, 0, w, h, VK_FILTER_NEAREST,
               VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingAttachmentInfo col_att{};
    col_att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    col_att.imageView = g_ctx.imgui_color.view;
    col_att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    col_att.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    col_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo ri{};
    ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    ri.renderArea = {{0, 0}, {static_cast<uint32_t>(w), static_cast<uint32_t>(h)}};
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &col_att;

    vkCmdBeginRendering(g_ctx.cmd, &ri);
    ImGui_ImplVulkan_RenderDrawData(draw_data, g_ctx.cmd);
    vkCmdEndRendering(g_ctx.cmd);

    blit_image(g_ctx.imgui_color, 0, h, w, 0, g_ctx.main_color, 0, 0, w, h, VK_FILTER_NEAREST,
               VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

void end_frame() {
    if (!g_ctx.active || !g_ctx.in_frame) return;
    end_rendering();

    FrameData& f = g_ctx.frames[g_ctx.frame_idx];

    if (g_ctx.swap_image_acquired && g_ctx.swapchain != VK_NULL_HANDLE &&
        g_ctx.swap_img_idx < g_ctx.swap_images.size()) {
        VkImage swap_img = g_ctx.swap_images[g_ctx.swap_img_idx];

        transition_image(g_ctx.cmd, g_ctx.main_color, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        VkImageMemoryBarrier b_to_dst{};
        b_to_dst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b_to_dst.srcAccessMask = 0;
        b_to_dst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b_to_dst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        b_to_dst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b_to_dst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b_to_dst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b_to_dst.image = swap_img;
        b_to_dst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(g_ctx.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &b_to_dst);

        // Blit Main Color (row 0 = bottom in OpenGL convention) to Swapchain (row 0 = top on monitor)
        VkImageBlit blit{};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.srcOffsets[0] = {0, static_cast<int32_t>(g_ctx.main_color.height), 0};
        blit.srcOffsets[1] = {static_cast<int32_t>(g_ctx.main_color.width), 0, 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {g_ctx.swap_w, g_ctx.swap_h, 1};
        vkCmdBlitImage(g_ctx.cmd, g_ctx.main_color.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swap_img,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);

        transition_image(g_ctx.cmd, g_ctx.main_color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkImageMemoryBarrier b_to_pres{};
        b_to_pres.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b_to_pres.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b_to_pres.dstAccessMask = 0;
        b_to_pres.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b_to_pres.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        b_to_pres.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b_to_pres.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b_to_pres.image = swap_img;
        b_to_pres.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(g_ctx.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &b_to_pres);
    }

    vkEndCommandBuffer(g_ctx.cmd);
    vkResetFences(g_ctx.device, 1, &f.in_flight);

    const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &g_ctx.cmd;
    VkSemaphore finished = f.render_finished;
    if (g_ctx.swap_img_idx < g_ctx.swap_render_finished.size() && g_ctx.swap_render_finished[g_ctx.swap_img_idx] != VK_NULL_HANDLE)
        finished = g_ctx.swap_render_finished[g_ctx.swap_img_idx];
    if (g_ctx.swap_image_acquired) {
        si.waitSemaphoreCount = 1;
        si.pWaitSemaphores = &f.image_available;
        si.pWaitDstStageMask = &wait_stage;
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores = &finished;
    }
    check_lost(vkQueueSubmit(g_ctx.queue, 1, &si, f.in_flight), "the frame's vkQueueSubmit");

    if (g_ctx.swap_image_acquired) {
        VkPresentInfoKHR pi{};
        pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores = &finished;
        pi.swapchainCount = 1;
        pi.pSwapchains = &g_ctx.swapchain;
        pi.pImageIndices = &g_ctx.swap_img_idx;
        const VkResult pres = vkQueuePresentKHR(g_ctx.queue, &pi);
        check_lost(pres, "vkQueuePresentKHR");
        if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
            g_ctx.swapchain_dirty = true;
        }
    }

    g_ctx.in_frame = false;
    g_ctx.cmd = VK_NULL_HANDLE;
    g_ctx.frame_idx = (g_ctx.frame_idx + 1) % kFramesInFlight;
    ++g_ctx.frame_num;
}

void flush_cmd() {
    if (!g_ctx.active || !g_ctx.in_frame) return;
    end_rendering();
    vkEndCommandBuffer(g_ctx.cmd);

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &g_ctx.cmd;
    check_lost(vkQueueSubmit(g_ctx.queue, 1, &si, VK_NULL_HANDLE), "vkQueueSubmit");
    vkQueueWaitIdle(g_ctx.queue);

    vkResetCommandBuffer(g_ctx.cmd, 0);
    VkCommandBufferBeginInfo cbbi{};
    cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(g_ctx.cmd, &cbbi);
}

void submit_immediate(const std::function<void(VkCommandBuffer)>& fn) {
    if (g_ctx.device == VK_NULL_HANDLE) return;
    if (g_ctx.in_frame && g_ctx.cmd != VK_NULL_HANDLE) {
        end_rendering();
        fn(g_ctx.cmd);
        flush_cmd();
        return;
    }
    vkResetCommandPool(g_ctx.device, g_ctx.imm_pool, 0);
    VkCommandBufferBeginInfo cbbi{};
    cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(g_ctx.imm_cmd, &cbbi);
    fn(g_ctx.imm_cmd);
    vkEndCommandBuffer(g_ctx.imm_cmd);

    vkResetFences(g_ctx.device, 1, &g_ctx.imm_fence);
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &g_ctx.imm_cmd;
    check_lost(vkQueueSubmit(g_ctx.queue, 1, &si, g_ctx.imm_fence), "an upload's vkQueueSubmit");
    check_lost(vkWaitForFences(g_ctx.device, 1, &g_ctx.imm_fence, VK_TRUE, UINT64_MAX), "an upload's fence");
}

// orig: (host) see vk_context.h
double timed_submit(const std::function<void(VkCommandBuffer)>& record) {
    if (g_ctx.device == VK_NULL_HANDLE) return -1.0;
    if (g_ctx.timed_pool == VK_NULL_HANDLE && g_ctx.dev_props.limits.timestampComputeAndGraphics == VK_TRUE) {
        VkQueryPoolCreateInfo qci{};
        qci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        qci.queryType = VK_QUERY_TYPE_TIMESTAMP;
        qci.queryCount = 2;
        if (vkCreateQueryPool(g_ctx.device, &qci, nullptr, &g_ctx.timed_pool) != VK_SUCCESS) g_ctx.timed_pool = VK_NULL_HANDLE;
    }
    const VkQueryPool pool = g_ctx.timed_pool;
    auto fn = [&](VkCommandBuffer cb) {
        if (pool != VK_NULL_HANDLE) {
            vkCmdResetQueryPool(cb, pool, 0, 2);
            vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pool, 0);
        }
        record(cb);
        if (pool != VK_NULL_HANDLE) vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool, 1);
    };
    if (g_ctx.in_frame && g_ctx.cmd != VK_NULL_HANDLE) {
        flush_cmd();  // (the frame's commands before: a submit of their own)
        fn(g_ctx.cmd);
        flush_cmd();
    } else {
        submit_immediate(fn);
    }
    if (pool == VK_NULL_HANDLE || g_ctx.device_lost) return -1.0;
    uint64_t ts[2] = {};
    if (vkGetQueryPoolResults(g_ctx.device, pool, 0, 2, sizeof ts, ts, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) != VK_SUCCESS)
        return -1.0;
    return static_cast<double>((ts[1] - ts[0]) & g_ctx.ts_mask) * g_ctx.ts_period_ns * 1e-6;
}

VkInstance instance() { return g_ctx.instance; }
VkPhysicalDevice physical_device() { return g_ctx.phys_dev; }
VkDevice device() { return g_ctx.device; }
uint32_t graphics_queue_family() { return g_ctx.queue_family; }
VkQueue graphics_queue() { return g_ctx.queue; }
VkCommandBuffer cmd() { return g_ctx.cmd; }
VkDescriptorPool imgui_descriptor_pool() { return g_ctx.imgui_pool; }
VkFormat color_format() { return VK_FORMAT_R8G8B8A8_UNORM; }
VkFormat swapchain_format() { return g_ctx.swap_format; }
VkFormat depth_stencil_format() { return g_ctx.depth_format; }
uint32_t frame_index() { return g_ctx.frame_idx; }
uint64_t frame_number() { return g_ctx.frame_num; }
int main_width() { return static_cast<int>(g_ctx.main_color.width); }
int main_height() { return static_cast<int>(g_ctx.main_color.height); }
int current_target_width() {
    return g_ctx.target_stack.empty() ? main_width() : static_cast<int>(g_ctx.target_stack.back().width);
}
int current_target_height() {
    return g_ctx.target_stack.empty() ? main_height() : static_cast<int>(g_ctx.target_stack.back().height);
}
const RenderTarget& current_target() {
    static const RenderTarget kEmpty{};
    return g_ctx.target_stack.empty() ? kEmpty : g_ctx.target_stack.back();
}

GpuImage& main_color_image() { return g_ctx.main_color; }
GpuImage& main_depth_image() { return g_ctx.main_depth; }
GpuImage& active_color_image() { return g_ctx.scaled_active ? g_ctx.scaled_color : g_ctx.main_color; }
GpuImage& active_depth_image() { return g_ctx.scaled_active ? g_ctx.scaled_depth : g_ctx.main_depth; }
bool scaled_scene_active() { return g_ctx.scaled_active; }

void scaled_scene_begin(int /*win_w*/, int /*win_h*/, int bw, int bh) {
    bw = std::max(1, bw);
    bh = std::max(1, bh);
    if (g_ctx.scaled_color.image == VK_NULL_HANDLE ||
        g_ctx.scaled_color.width != static_cast<uint32_t>(bw) ||
        g_ctx.scaled_color.height != static_cast<uint32_t>(bh)) {
        destroy_image(g_ctx.scaled_color);
        destroy_image(g_ctx.scaled_depth);
        create_image(static_cast<uint32_t>(bw), static_cast<uint32_t>(bh), VK_FORMAT_R8G8B8A8_UNORM,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT, g_ctx.scaled_color);
        create_image(static_cast<uint32_t>(bw), static_cast<uint32_t>(bh), g_ctx.depth_format,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, g_ctx.scaled_depth);
    }
    end_rendering();
    transition_image(g_ctx.cmd, g_ctx.scaled_color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    transition_image(g_ctx.cmd, g_ctx.scaled_depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    RenderTarget rt{};
    rt.color_view = g_ctx.scaled_color.view;
    rt.color_format = g_ctx.scaled_color.format;
    rt.depth_view = g_ctx.scaled_depth.view;
    rt.depth_format = g_ctx.scaled_depth.format;
    rt.stencil_view = g_ctx.scaled_depth.view;
    rt.width = g_ctx.scaled_color.width;
    rt.height = g_ctx.scaled_color.height;

    g_ctx.scaled_active = true;
    const float clear_col[4] = {0.02f, 0.02f, 0.04f, 1.0f};
    set_viewport(0, 0, bw, bh);
    push_target(rt, true, clear_col, true, 1.0f, true, 0);
}

void scaled_scene_end(int win_w, int win_h) {
    if (!g_ctx.scaled_active) return;
    pop_target();
    g_ctx.scaled_active = false;
    const int sw = static_cast<int>(g_ctx.scaled_color.width);
    const int sh = static_cast<int>(g_ctx.scaled_color.height);
    blit_image(g_ctx.scaled_color, 0, 0, sw, sh, g_ctx.main_color, 0, 0, win_w, win_h, VK_FILTER_LINEAR,
               VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    blit_image(g_ctx.scaled_depth, 0, 0, sw, sh, g_ctx.main_depth, 0, 0, win_w, win_h, VK_FILTER_NEAREST,
               VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
               VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    set_viewport(0, 0, win_w, win_h);
}

void push_target(const RenderTarget& rt, bool clear_color, const float rgba[4],
                 bool clear_depth, float depth_val, bool clear_stencil, uint32_t stencil_val) {
    if (g_ctx.rendering_active) end_rendering();
    g_ctx.target_stack.push_back(rt);
    if (clear_color || clear_depth || clear_stencil) {
        begin_rendering_internal(rt, clear_color, rgba, clear_depth, depth_val, clear_stencil, stencil_val);
    }
}

void pop_target() {
    if (g_ctx.rendering_active) end_rendering();
    if (g_ctx.target_stack.size() > 1) g_ctx.target_stack.pop_back();
}

void ensure_rendering() {
    if (!g_ctx.in_frame || g_ctx.target_stack.empty()) return;
    if (!g_ctx.rendering_active) {
        begin_rendering_internal(g_ctx.target_stack.back(), false, nullptr, false, 1.0f, false, 0);
    }
}

void end_rendering() {
    if (g_ctx.rendering_active && g_ctx.cmd != VK_NULL_HANDLE) {
        vkCmdEndRendering(g_ctx.cmd);
        g_ctx.rendering_active = false;
    }
}

void set_viewport(int x, int y, int w, int h, float min_depth, float max_depth) {
    g_ctx.vp[0] = x;
    g_ctx.vp[1] = y;
    g_ctx.vp[2] = std::max(1, w);
    g_ctx.vp[3] = std::max(1, h);
    g_ctx.vp_min_d = min_depth;
    g_ctx.vp_max_d = max_depth;
    if (g_ctx.rendering_active) apply_viewport_and_scissor();
}

void get_viewport(int out_vp[4]) {
    for (int i = 0; i < 4; ++i) out_vp[i] = g_ctx.vp[i];
}

void set_scissor(bool enable, int x, int y, int w, int h) {
    g_ctx.scissor_enabled = enable;
    if (enable) {
        g_ctx.scissor_rect[0] = x;
        g_ctx.scissor_rect[1] = y;
        g_ctx.scissor_rect[2] = std::max(0, w);
        g_ctx.scissor_rect[3] = std::max(0, h);
    }
    if (g_ctx.rendering_active) apply_viewport_and_scissor();
}

void clear_attachments(bool color, const float rgba[4], bool depth, float depth_val, bool stencil, uint32_t stencil_val) {
    if (!g_ctx.in_frame || g_ctx.target_stack.empty()) return;
    ensure_rendering();
    const RenderTarget& rt = g_ctx.target_stack.back();

    VkClearAttachment atts[2]{};
    uint32_t count = 0;
    if (color && rt.color_view != VK_NULL_HANDLE) {
        atts[count].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        atts[count].colorAttachment = 0;
        if (rgba != nullptr) {
            atts[count].clearValue.color = {{rgba[0], rgba[1], rgba[2], rgba[3]}};
        }
        ++count;
    }
    if ((depth || stencil) && (rt.depth_view != VK_NULL_HANDLE || rt.stencil_view != VK_NULL_HANDLE)) {
        VkImageAspectFlags asp = 0;
        if (depth && rt.depth_view != VK_NULL_HANDLE) asp |= VK_IMAGE_ASPECT_DEPTH_BIT;
        if (stencil && rt.stencil_view != VK_NULL_HANDLE) asp |= VK_IMAGE_ASPECT_STENCIL_BIT;
        if (asp != 0) {
            atts[count].aspectMask = asp;
            atts[count].clearValue.depthStencil = {depth_val, stencil_val};
            ++count;
        }
    }
    if (count == 0) return;

    VkClearRect rect{};
    rect.rect = {{0, 0}, {rt.width, rt.height}};
    rect.baseArrayLayer = 0;
    rect.layerCount = 1;
    vkCmdClearAttachments(g_ctx.cmd, count, atts, 1, &rect);
}

uint32_t find_memory_type(uint32_t type_bits, VkMemoryPropertyFlags props) {
    for (uint32_t i = 0; i < g_ctx.mem_props.memoryTypeCount; ++i) {
        if ((type_bits & (1u << i)) && (g_ctx.mem_props.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    if (props & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)  // (a cached type this resource cannot use: an uncached one, still host-visible)
        return find_memory_type(type_bits, props & ~VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    for (uint32_t i = 0; i < g_ctx.mem_props.memoryTypeCount; ++i) {
        if (type_bits & (1u << i)) return i;
    }
    return 0;
}

VkMemoryPropertyFlags readback_memory_flags() {
    constexpr VkMemoryPropertyFlags base = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    constexpr VkMemoryPropertyFlags cached = base | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    for (uint32_t i = 0; i < g_ctx.mem_props.memoryTypeCount; ++i)
        if ((g_ctx.mem_props.memoryTypes[i].propertyFlags & cached) == cached) return cached;
    return base;
}

bool create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, GpuBuffer& out, bool map) {
    destroy_buffer(out);
    if (size == 0) return false;

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(g_ctx.device, &bci, nullptr, &out.buffer) != VK_SUCCESS) return false;

    VkMemoryRequirements mr{};
    vkGetBufferMemoryRequirements(g_ctx.device, out.buffer, &mr);

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = find_memory_type(mr.memoryTypeBits, props);
    if (vkAllocateMemory(g_ctx.device, &mai, nullptr, &out.memory) != VK_SUCCESS) {
        vkDestroyBuffer(g_ctx.device, out.buffer, nullptr);
        out.buffer = VK_NULL_HANDLE;
        return false;
    }
    vkBindBufferMemory(g_ctx.device, out.buffer, out.memory, 0);
    alloc_note(out.memory, size == kRingBufferSize ? "frame rings (vertices, uniforms, uploads)"
                           : usage == VK_BUFFER_USAGE_TRANSFER_SRC_BIT || usage == VK_BUFFER_USAGE_TRANSFER_DST_BIT ? "staging, readbacks"
                           : (props & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? "buffers (meshes, tables)" : "buffers in system memory",
               false, mr.size);
    out.size = size;
    out.mapped = nullptr;
    if (map && (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
        vkMapMemory(g_ctx.device, out.memory, 0, size, 0, &out.mapped);
    }
    return true;
}

void destroy_buffer_now(GpuBuffer& buf) {
    if (g_ctx.device == VK_NULL_HANDLE) {
        buf = GpuBuffer{};
        return;
    }
    if (buf.mapped != nullptr && buf.memory != VK_NULL_HANDLE) {
        vkUnmapMemory(g_ctx.device, buf.memory);
    }
    if (buf.buffer != VK_NULL_HANDLE) vkDestroyBuffer(g_ctx.device, buf.buffer, nullptr);
    if (buf.memory != VK_NULL_HANDLE) {
        alloc_forget(buf.memory);
        vkFreeMemory(g_ctx.device, buf.memory, nullptr);
    }
    buf = GpuBuffer{};
}

// The frame slot whose fence a deferred destroy waits for. Inside a frame: the frame being recorded. Between end_frame (which
// has already moved frame_idx on) and the next begin_frame (a level loaded after the swap): the frame just submitted, whose
// slot comes back only after its fence - not frame_idx's slot, which begin_frame frees after waiting for the frame before it,
// while the frame just submitted may still use the object.
// orig: (host)
static uint32_t deferred_slot() { return g_ctx.in_frame ? g_ctx.frame_idx : (g_ctx.frame_idx + kFramesInFlight - 1) % kFramesInFlight; }
void destroy_buffer(GpuBuffer& buf) {
    if (buf.buffer == VK_NULL_HANDLE && buf.memory == VK_NULL_HANDLE) return;
    if (g_ctx.active && g_ctx.device != VK_NULL_HANDLE) {
        g_ctx.frames[deferred_slot()].deferred_buffers.push_back(buf);
        buf = GpuBuffer{};
    } else {
        destroy_buffer_now(buf);
    }
}

bool upload_buffer(GpuBuffer& dst, const void* data, VkDeviceSize size, VkBufferUsageFlags usage) {
    if (size == 0 || data == nullptr) return false;
    // Always allocate a fresh buffer and defer-free any previous buffer in flight (matching glBufferData buffer orphaning)
    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer tmp = VK_NULL_HANDLE;
    VkMemoryRequirements mr{};
    if (vkCreateBuffer(g_ctx.device, &bci, nullptr, &tmp) == VK_SUCCESS) {
        vkGetBufferMemoryRequirements(g_ctx.device, tmp, &mr);
        vkDestroyBuffer(g_ctx.device, tmp, nullptr);
    }
    VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < g_ctx.mem_props.memoryTypeCount; ++i) {
        const VkMemoryPropertyFlags want = props | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        if ((mr.memoryTypeBits & (1u << i)) && (g_ctx.mem_props.memoryTypes[i].propertyFlags & want) == want) {
            props = want;
            break;
        }
    }
    if (!create_buffer(size, usage, props, dst, true)) {
        if (!create_buffer(size, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, dst, true)) {
            return false;
        }
    }
    std::memcpy(dst.mapped, data, static_cast<size_t>(size));
    return true;
}

RingAlloc alloc_ring(VkDeviceSize size, VkDeviceSize alignment) {
    size = std::max<VkDeviceSize>(size, 16);
    // (16 at least; and the device's storage buffer offset alignment, since callers bind ring ranges as storage buffers with
    // 16: 4 on AMD, up to 64 elsewhere - both powers of two, as the spec has them)
    alignment = std::max<VkDeviceSize>({alignment, 16, g_ctx.dev_props.limits.minStorageBufferOffsetAlignment});
    FrameData& f = g_ctx.frames[g_ctx.frame_idx];
    const VkDeviceSize aligned = (f.ring_offset + alignment - 1) & ~(alignment - 1);
    if (aligned + size <= f.ring_buf.size) {
        f.ring_offset = aligned + size;
        return RingAlloc{f.ring_buf.buffer, aligned, static_cast<uint8_t*>(f.ring_buf.mapped) + aligned};
    }
    // Overflow allocation queued for deferred deletion
    GpuBuffer overflow{};
    create_buffer(size,
                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                      VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, overflow, true);
    RingAlloc res{overflow.buffer, 0, overflow.mapped};
    f.deferred_buffers.push_back(overflow);
    return res;
}

bool create_image(uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect,
                  GpuImage& out, uint32_t layers, VkImageCreateFlags flags, VkImageViewType view_type, uint32_t mip_levels) {
    destroy_image(out);
    if (w == 0 || h == 0) return false;
    mip_levels = std::max(1u, mip_levels);

    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.flags = flags;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = format;
    ici.extent = {w, h, 1};
    ici.mipLevels = mip_levels;
    ici.arrayLayers = layers;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = usage;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(g_ctx.device, &ici, nullptr, &out.image) != VK_SUCCESS) return false;

    VkMemoryRequirements mr{};
    vkGetImageMemoryRequirements(g_ctx.device, out.image, &mr);

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = find_memory_type(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(g_ctx.device, &mai, nullptr, &out.memory) != VK_SUCCESS) {
        vkDestroyImage(g_ctx.device, out.image, nullptr);
        out.image = VK_NULL_HANDLE;
        return false;
    }
    vkBindImageMemory(g_ctx.device, out.image, out.memory, 0);
    alloc_note(out.memory, (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? "depth buffers, shadow maps"
                           : (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) ? "colour targets (frame, effects)" : "textures",
               true, mr.size);

    VkImageViewCreateInfo vci{};
    vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image = out.image;
    vci.viewType = view_type;
    vci.format = format;
    // Sampled-only views of depth+stencil images must specify only the depth aspect in the view
    vci.subresourceRange.aspectMask =
        (!(usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) && (aspect & VK_IMAGE_ASPECT_DEPTH_BIT))
            ? static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT)
            : aspect;
    vci.subresourceRange.baseMipLevel = 0;
    vci.subresourceRange.levelCount = mip_levels;
    vci.subresourceRange.baseArrayLayer = 0;
    vci.subresourceRange.layerCount = layers;
    if (vkCreateImageView(g_ctx.device, &vci, nullptr, &out.view) != VK_SUCCESS) {
        destroy_image_now(out);
        return false;
    }

    if ((usage & VK_IMAGE_USAGE_SAMPLED_BIT) && (aspect & VK_IMAGE_ASPECT_DEPTH_BIT) && (aspect & VK_IMAGE_ASPECT_STENCIL_BIT) &&
        vci.subresourceRange.aspectMask != VK_IMAGE_ASPECT_DEPTH_BIT) {
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        vkCreateImageView(g_ctx.device, &vci, nullptr, &out.depth_view);
    }
    out.width = w;
    out.height = h;
    out.layers = layers;
    out.mip_levels = mip_levels;
    out.format = format;
    out.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    out.aspect = aspect;
    return true;
}

void destroy_image_now(GpuImage& img) {
    if (g_ctx.device == VK_NULL_HANDLE) {
        img = GpuImage{};
        return;
    }
    if (img.view != VK_NULL_HANDLE) vkDestroyImageView(g_ctx.device, img.view, nullptr);
    if (img.depth_view != VK_NULL_HANDLE) vkDestroyImageView(g_ctx.device, img.depth_view, nullptr);
    if (img.image != VK_NULL_HANDLE) vkDestroyImage(g_ctx.device, img.image, nullptr);
    if (img.memory != VK_NULL_HANDLE) {
        alloc_forget(img.memory);
        vkFreeMemory(g_ctx.device, img.memory, nullptr);
    }
    img = GpuImage{};
}

void destroy_image(GpuImage& img) {
    if (img.image == VK_NULL_HANDLE && img.view == VK_NULL_HANDLE && img.memory == VK_NULL_HANDLE) return;
    if (g_ctx.active && g_ctx.device != VK_NULL_HANDLE) {
        g_ctx.frames[deferred_slot()].deferred_images.push_back(img);
        img = GpuImage{};
    } else {
        destroy_image_now(img);
    }
}

void transition_image(VkCommandBuffer cb, GpuImage& img, VkImageLayout new_layout) {
    if (img.image == VK_NULL_HANDLE) return;
    if (cb == g_ctx.cmd && g_ctx.rendering_active) end_rendering();

    VkImageMemoryBarrier b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    b.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    b.oldLayout = img.layout;
    b.newLayout = new_layout;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img.image;
    b.subresourceRange.aspectMask = img.aspect;
    b.subresourceRange.baseMipLevel = 0;
    b.subresourceRange.levelCount = std::max(1u, img.mip_levels);
    b.subresourceRange.baseArrayLayer = 0;
    b.subresourceRange.layerCount = img.layers;

    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &b);
    img.layout = new_layout;
}

void upload_image_2d(GpuImage& img, const void* data, size_t byte_size, int x, int y, int w, int h, int row_length,
                     uint32_t layer, uint32_t mip_level) {
    if (img.image == VK_NULL_HANDLE || data == nullptr || byte_size == 0) return;
    const uint32_t uw = w > 0 ? static_cast<uint32_t>(w) : std::max(1u, img.width >> mip_level);
    const uint32_t uh = h > 0 ? static_cast<uint32_t>(h) : std::max(1u, img.height >> mip_level);
    if (uw == 0 || uh == 0) return;

    if (g_ctx.in_frame && g_ctx.cmd != VK_NULL_HANDLE) {
        end_rendering();
        RingAlloc stage = alloc_ring(byte_size, 16);
        std::memcpy(stage.ptr, data, byte_size);
        transition_image(g_ctx.cmd, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy copy{};
        copy.bufferOffset = stage.offset;
        copy.bufferRowLength = row_length > 0 ? static_cast<uint32_t>(row_length) : 0;
        copy.bufferImageHeight = 0;
        copy.imageSubresource = {img.aspect, mip_level, layer, 1};
        copy.imageOffset = {x, y, 0};
        copy.imageExtent = {uw, uh, 1};
        vkCmdCopyBufferToImage(g_ctx.cmd, stage.buffer, img.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        transition_image(g_ctx.cmd, img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    } else {
        GpuBuffer stage{};
        create_buffer(byte_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stage, true);
        std::memcpy(stage.mapped, data, byte_size);
        submit_immediate([&](VkCommandBuffer cb) {
            transition_image(cb, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            VkBufferImageCopy copy{};
            copy.bufferOffset = 0;
            copy.bufferRowLength = row_length > 0 ? static_cast<uint32_t>(row_length) : 0;
            copy.bufferImageHeight = 0;
            copy.imageSubresource = {img.aspect, mip_level, layer, 1};
            copy.imageOffset = {x, y, 0};
            copy.imageExtent = {uw, uh, 1};
            vkCmdCopyBufferToImage(cb, stage.buffer, img.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
            transition_image(cb, img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        });
        destroy_buffer_now(stage);
    }
}

void blit_image(GpuImage& src, int sx0, int sy0, int sx1, int sy1, GpuImage& dst, int dx0, int dy0, int dx1, int dy1,
                VkFilter filter, VkImageAspectFlags aspect, VkImageLayout dst_final_layout) {
    if (src.image == VK_NULL_HANDLE || dst.image == VK_NULL_HANDLE) return;
    auto record = [&](VkCommandBuffer cb) {
        const VkImageLayout src_old = src.layout;
        transition_image(cb, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        transition_image(cb, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkImageBlit blit{};
        blit.srcSubresource = {aspect, 0, 0, 1};
        blit.srcOffsets[0] = {sx0, sy0, 0};
        blit.srcOffsets[1] = {sx1, sy1, 1};
        blit.dstSubresource = {aspect, 0, 0, 1};
        blit.dstOffsets[0] = {dx0, dy0, 0};
        blit.dstOffsets[1] = {dx1, dy1, 1};
        vkCmdBlitImage(cb, src.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                       (aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) ? VK_FILTER_NEAREST : filter);

        transition_image(cb, src,
                         src_old == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : src_old);
        transition_image(cb, dst, dst_final_layout);
    };
    if (g_ctx.in_frame && g_ctx.cmd != VK_NULL_HANDLE) {
        end_rendering();
        record(g_ctx.cmd);
    } else {
        submit_immediate(record);
    }
}

void copy_image(GpuImage& src, int sx, int sy, GpuImage& dst, int dx, int dy, int w, int h, VkImageAspectFlags aspect,
                VkImageLayout src_final, VkImageLayout dst_final) {
    if (src.image == VK_NULL_HANDLE || dst.image == VK_NULL_HANDLE || w <= 0 || h <= 0) return;
    auto record = [&](VkCommandBuffer cb) {
        transition_image(cb, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        transition_image(cb, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkImageCopy c{};
        c.srcSubresource = {aspect, 0, 0, 1};
        c.srcOffset = {sx, sy, 0};
        c.dstSubresource = {aspect, 0, 0, 1};
        c.dstOffset = {dx, dy, 0};
        c.extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
        vkCmdCopyImage(cb, src.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &c);
        transition_image(cb, src, src_final);
        transition_image(cb, dst, dst_final);
    };
    if (g_ctx.in_frame && g_ctx.cmd != VK_NULL_HANDLE) {
        end_rendering();
        record(g_ctx.cmd);
    } else {
        submit_immediate(record);
    }
}

void read_image_rgba(GpuImage& src, int w, int h, void* out_rgba) {
    if (src.image == VK_NULL_HANDLE || w <= 0 || h <= 0 || out_rgba == nullptr) return;
    const size_t bytes = static_cast<size_t>(w) * static_cast<size_t>(h) * 4;
    GpuBuffer stage{};
    create_buffer(bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stage, true);

    auto record = [&](VkCommandBuffer cb) {
        const VkImageLayout old_layout = src.layout;
        transition_image(cb, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {std::min(static_cast<uint32_t>(w), src.width),
                            std::min(static_cast<uint32_t>(h), src.height), 1};
        vkCmdCopyImageToBuffer(cb, src.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stage.buffer, 1, &copy);
        transition_image(cb, src,
                         old_layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : old_layout);
    };

    if (g_ctx.in_frame && g_ctx.cmd != VK_NULL_HANDLE) {
        end_rendering();
        record(g_ctx.cmd);
        flush_cmd();
    } else {
        submit_immediate(record);
    }

    std::memcpy(out_rgba, stage.mapped, bytes);
    destroy_buffer_now(stage);
}

void save_ppm(const char* path, int w, int h) {
    if (w <= 0 || h <= 0) return;
    std::vector<uint8_t> px(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    read_image_rgba(g_ctx.main_color, w, h, px.data());
    FILE* f = std::fopen(path, "wb");
    if (f == nullptr) return;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; --y) {
        for (int x = 0; x < w; ++x) {
            std::fwrite(&px[(static_cast<size_t>(y) * w + x) * 4], 1, 3, f);
        }
    }
    std::fclose(f);
}

VkSampler get_sampler(VkFilter min_filter, VkFilter mag_filter, VkSamplerAddressMode wrap_s, VkSamplerAddressMode wrap_t,
                      bool compare, VkCompareOp compare_op, float max_anisotropy,
                      VkSamplerMipmapMode mipmap_mode, float max_lod) {
    const int aniso_x10 = max_anisotropy > 1.0f ? static_cast<int>(max_anisotropy * 10.0f) : 10;
    const int max_lod_x100 = static_cast<int>(max_lod * 100.0f);
    const SamplerKey key{min_filter, mag_filter, wrap_s, wrap_t, compare, compare_op, aniso_x10, mipmap_mode, max_lod_x100};
    for (const auto& [k, s] : g_ctx.samplers) {
        if (k == key) return s;
    }

    VkSamplerCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter = mag_filter;
    sci.minFilter = min_filter;
    sci.mipmapMode = mipmap_mode;
    sci.addressModeU = wrap_s;
    sci.addressModeV = wrap_t;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.anisotropyEnable = max_anisotropy > 1.0f && g_ctx.has_anisotropy ? VK_TRUE : VK_FALSE;
    sci.maxAnisotropy = std::min(max_anisotropy, g_ctx.dev_props.limits.maxSamplerAnisotropy);
    sci.compareEnable = compare ? VK_TRUE : VK_FALSE;
    sci.compareOp = compare_op;
    sci.minLod = 0.0f;
    sci.maxLod = max_lod;
    sci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    VkSampler s = VK_NULL_HANDLE;
    vkCreateSampler(g_ctx.device, &sci, nullptr, &s);
    g_ctx.samplers.emplace_back(key, s);
    return s;
}

VkDescriptorSet alloc_descriptor_set(VkDescriptorSetLayout layout) {
    FrameData& f = g_ctx.frames[g_ctx.frame_idx];
    while (f.active_pool_idx < f.desc_pools.size()) {
        VkDescriptorSetAllocateInfo dsai{};
        dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsai.descriptorPool = f.desc_pools[f.active_pool_idx];
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts = &layout;
        VkDescriptorSet set = VK_NULL_HANDLE;
        const VkResult res = vkAllocateDescriptorSets(g_ctx.device, &dsai, &set);
        if (res == VK_SUCCESS) return set;
        ++f.active_pool_idx;
    }
    f.desc_pools.push_back(create_transient_pool());
    VkDescriptorSetAllocateInfo dsai{};
    dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool = f.desc_pools.back();
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &layout;
    VkDescriptorSet set = VK_NULL_HANDLE;
    vkAllocateDescriptorSets(g_ctx.device, &dsai, &set);
    return set;
}

VkShaderModule create_shader_module(const uint32_t* code, size_t size_in_bytes) {
    VkShaderModuleCreateInfo smci{};
    smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.codeSize = size_in_bytes;
    smci.pCode = code;
    VkShaderModule mod = VK_NULL_HANDLE;
    vkCreateShaderModule(g_ctx.device, &smci, nullptr, &mod);
    return mod;
}

VkPipeline create_graphics_pipeline(const GraphicsPipelineDesc& desc) {
    VkPipelineShaderStageCreateInfo stages[2]{};
    uint32_t stage_count = 0;
    if (desc.vs != VK_NULL_HANDLE) {
        stages[stage_count].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[stage_count].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[stage_count].module = desc.vs;
        stages[stage_count].pName = "main";
        ++stage_count;
    }
    if (desc.fs != VK_NULL_HANDLE) {
        stages[stage_count].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[stage_count].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[stage_count].module = desc.fs;
        stages[stage_count].pName = "main";
        ++stage_count;
    }

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = static_cast<uint32_t>(desc.vertex_bindings.size());
    vi.pVertexBindingDescriptions = desc.vertex_bindings.data();
    vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(desc.vertex_attribs.size());
    vi.pVertexAttributeDescriptions = desc.vertex_attribs.data();

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = desc.topology;

    VkPipelineViewportDepthClipControlCreateInfoEXT clip_ctrl{};
    clip_ctrl.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT;
    clip_ctrl.negativeOneToOne = VK_TRUE;

    VkPipelineViewportStateCreateInfo vps{};
    vps.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vps.viewportCount = 1;
    vps.scissorCount = 1;
    if (g_ctx.has_depth_clip_control) {
        vps.pNext = &clip_ctrl;
    }

    VkPipelineRasterizationProvokingVertexStateCreateInfoEXT prov_state{};
    prov_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT;
    prov_state.provokingVertexMode = VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    if (g_ctx.has_provoking_vertex) rs.pNext = &prov_state;
    rs.depthClampEnable = desc.depth_clamp ? VK_TRUE : VK_FALSE;
    rs.polygonMode = desc.polygon_mode;
    rs.cullMode = desc.cull_mode;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.depthBiasEnable = desc.depth_bias_enable ? VK_TRUE : VK_FALSE;
    rs.depthBiasConstantFactor = desc.depth_bias_constant;
    rs.depthBiasSlopeFactor = desc.depth_bias_slope;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = desc.depth_test ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = desc.depth_write ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp = desc.depth_compare;
    ds.stencilTestEnable = desc.stencil_test ? VK_TRUE : VK_FALSE;
    ds.front = desc.stencil_front;
    ds.back = desc.stencil_back;

    VkFormat cfmt = desc.color_format;
    VkFormat dfmt = desc.has_depth_attachment
                        ? (desc.depth_format != VK_FORMAT_UNDEFINED ? desc.depth_format : g_ctx.depth_format)
                        : VK_FORMAT_UNDEFINED;
    VkFormat sfmt = (desc.has_depth_attachment && desc.has_stencil_attachment &&
                     (dfmt == VK_FORMAT_D32_SFLOAT_S8_UINT || dfmt == VK_FORMAT_D24_UNORM_S8_UINT))
                        ? dfmt
                        : VK_FORMAT_UNDEFINED;
    if (g_ctx.in_frame && !g_ctx.target_stack.empty()) {
        const RenderTarget& rt = g_ctx.target_stack.back();
        cfmt = (rt.color_view != VK_NULL_HANDLE) ? rt.color_format : VK_FORMAT_UNDEFINED;
        dfmt = (rt.depth_view != VK_NULL_HANDLE) ? rt.depth_format : VK_FORMAT_UNDEFINED;
        sfmt = (rt.stencil_view != VK_NULL_HANDLE) ? rt.depth_format : VK_FORMAT_UNDEFINED;
    }

    VkPipelineColorBlendAttachmentState cba{};
    cba.blendEnable = desc.blend_enable ? VK_TRUE : VK_FALSE;
    cba.srcColorBlendFactor = desc.src_color_blend;
    cba.dstColorBlendFactor = desc.dst_color_blend;
    cba.colorBlendOp = desc.color_blend_op;
    cba.srcAlphaBlendFactor = desc.src_alpha_blend;
    cba.dstAlphaBlendFactor = desc.dst_alpha_blend;
    cba.alphaBlendOp = desc.alpha_blend_op;
    cba.colorWriteMask = desc.color_write_mask;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    if (cfmt != VK_FORMAT_UNDEFINED) {
        cb.attachmentCount = 1;
        cb.pAttachments = &cba;
    }

    std::vector<VkDynamicState> dyn_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS,
    };
    for (VkDynamicState s : desc.extra_dynamic_states) dyn_states.push_back(s);

    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = static_cast<uint32_t>(dyn_states.size());
    dyn.pDynamicStates = dyn_states.data();

    VkPipelineRenderingCreateInfo prci{};
    prci.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    if (cfmt != VK_FORMAT_UNDEFINED) {
        prci.colorAttachmentCount = 1;
        prci.pColorAttachmentFormats = &cfmt;
    }
    prci.depthAttachmentFormat = dfmt;
    prci.stencilAttachmentFormat = sfmt;

    VkGraphicsPipelineCreateInfo gpci{};
    gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpci.pNext = &prci;
    gpci.stageCount = stage_count;
    gpci.pStages = stages;
    gpci.pVertexInputState = &vi;
    gpci.pInputAssemblyState = &ia;
    gpci.pViewportState = &vps;
    gpci.pRasterizationState = &rs;
    gpci.pMultisampleState = &ms;
    gpci.pDepthStencilState = &ds;
    gpci.pColorBlendState = &cb;
    gpci.pDynamicState = &dyn;
    gpci.layout = desc.layout;

    VkPipeline pipe = VK_NULL_HANDLE;
    vkCreateGraphicsPipelines(g_ctx.device, VK_NULL_HANDLE, 1, &gpci, nullptr, &pipe);
    return pipe;
}

VkPipeline create_compute_pipeline(VkPipelineLayout layout, VkShaderModule cs) {
    VkComputePipelineCreateInfo cpci{};
    cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cpci.stage.module = cs;
    cpci.stage.pName = "main";
    cpci.layout = layout;
    VkPipeline pipe = VK_NULL_HANDLE;
    vkCreateComputePipelines(g_ctx.device, VK_NULL_HANDLE, 1, &cpci, nullptr, &pipe);
    return pipe;
}

const GpuImage& white_texture() { return g_ctx.white_tex; }
const GpuImage& white_texture_array() { return g_ctx.white_tex_array; }
const GpuImage& black_uint_texture() { return g_ctx.black_uint_tex; }
const GpuImage& dummy_depth_2d() { return g_ctx.dummy_d2d; }
const GpuImage& dummy_depth_array() { return g_ctx.dummy_darray; }
const GpuImage& dummy_depth_cube_array() { return g_ctx.dummy_dcube; }

void register_shutdown_callback(void (*fn)()) {
    if (fn == nullptr) return;
    for (auto existing : g_ctx.shutdown_callbacks) {
        if (existing == fn) return;
    }
    g_ctx.shutdown_callbacks.push_back(fn);
}

}  // namespace rac1::vk
