// The Metal renderer's device and frame (runtime/mtl/mtl_context.h; docs/metal.md). Objective-C++, ARC.
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Cocoa/Cocoa.h>

#include <runtime/mtl/mtl_context.h>
#include <profiler/host_profiler.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#include <imgui.h>
#include <backends/imgui_impl_metal.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <regex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rac1::mtl {

namespace {

// ---- the shaders: their sources (runtime/mtl/shaders/, embedded at build) and binding tables ----
enum Kind : uint8_t { kBuffer, kTexture, kSampler };
struct Bind {
    uint8_t set, binding;
    Kind kind;
    uint8_t index;
};
struct ShaderInfo {
    const char* name;
    const char* stage;
    int push;    // the push constants' buffer index (-1: none)
    int argbuf;  // descriptor set 2's argument buffer index (-1: none)
    std::vector<Bind> binds;
};
#define RAC1_MTL_SHADER(n, st, p, a, ...) {#n, st, p, a, __VA_ARGS__},
const ShaderInfo kShaderInfos[] = {
#include <runtime/mtl/mtl_shader_table.inc>
};
#undef RAC1_MTL_SHADER
struct ShaderSrc {
    const char* name;
    const char* src;
};
const ShaderSrc kShaderSrcs[] = {
#include <mtl_shader_sources.inc>  // (made by cmake/embed_metal_shaders.cmake)
};

constexpr uint32_t kVertexBufferBase = 30;  // (tools/gen_metal_shaders.py: VERTEX_BUFFER_BASE)
constexpr size_t kRingBufferSize = 32ull * 1024ull * 1024ull;
constexpr size_t kRingAlign = 256;
constexpr int kSets = 4;
constexpr int kBindings = 16;

// The utility shaders: a region of a picture into another one, scaled and flipped (colour; depth and stencil)
const char* kUtilSrc = R"msl(
#include <metal_stdlib>
using namespace metal;
struct BlitParams { float2 d0; float2 d1; float2 s0; float2 s1; float2 src_size; float layer; float pad; };
struct VOut { float4 pos [[position]]; };
vertex VOut util_vs(uint vid [[vertex_id]]) {
    const float2 p[3] = {float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0)};
    VOut o; o.pos = float4(p[vid], 0.0, 1.0); return o;
}
static float2 src_coord(float2 frag, constant BlitParams& bp) {
    const float2 t = (frag - bp.d0) / (bp.d1 - bp.d0);
    return bp.s0 + t * (bp.s1 - bp.s0);
}
fragment float4 util_color_fs(VOut in [[stage_in]], constant BlitParams& bp [[buffer(0)]],
                              texture2d<float> src [[texture(0)]], sampler smp [[sampler(0)]]) {
    return src.sample(smp, src_coord(in.pos.xy, bp) / bp.src_size, level(0.0));
}
struct DsOut { float depth [[depth(any)]]; uint stencil [[stencil]]; };
fragment DsOut util_ds_fs(VOut in [[stage_in]], constant BlitParams& bp [[buffer(0)]],
                          depth2d<float> dsrc [[texture(0)]], texture2d<uint> ssrc [[texture(1)]]) {
    const float2 c = clamp(floor(src_coord(in.pos.xy, bp)), float2(0.0), bp.src_size - 1.0);
    DsOut o; o.depth = dsrc.read(uint2(c)); o.stencil = ssrc.read(uint2(c)).x; return o;
}
struct DOut { float depth [[depth(any)]]; };
fragment DOut util_d_fs(VOut in [[stage_in]], constant BlitParams& bp [[buffer(0)]], depth2d<float> dsrc [[texture(0)]]) {
    const float2 c = clamp(floor(src_coord(in.pos.xy, bp)), float2(0.0), bp.src_size - 1.0);
    DOut o; o.depth = dsrc.read(uint2(c)); return o;
}
)msl";

struct BlitParams {
    float d0[2], d1[2], s0[2], s1[2], src_size[2], layer, pad;
};

}  // namespace

struct Shader {
    const ShaderInfo* info = nullptr;
    id<MTLFunction> fn = nil;
    MTLSize local_size = MTLSizeMake(1, 1, 1);
    bool flat_inputs = false;  // (a fragment shader with flat inputs: its triangles drawn last vertex first, gen_metal_shaders.py)
};

struct Pipeline {
    GraphicsPipelineDesc desc;
    Shader* vs = nullptr;
    Shader* fs = nullptr;
    MTLVertexDescriptor* vd = nil;
    std::map<std::tuple<int, int, int>, id<MTLRenderPipelineState>> states;  // by (colour, depth, stencil) formats
    id<MTLDepthStencilState> ds = nil;
    id<MTLDepthStencilState> ds_none = nil;  // (a target without depth)
    bool dyn_bias = false;
    bool dyn_stencil_ref = false;
    MTLPrimitiveType prim = MTLPrimitiveTypeTriangle;
};

struct ComputePipeline {
    Shader* cs = nullptr;
    id<MTLComputePipelineState> state = nil;
};

namespace {

struct Slot {
    id<MTLBuffer> buffer = nil;
    size_t offset = 0;
    id<MTLTexture> texture = nil;
    id<MTLSamplerState> sampler = nil;
};

struct SamplerKey {
    VkFilter min_f, mag_f;
    VkSamplerAddressMode ws, wt;
    bool compare;
    VkCompareOp op;
    int aniso_x10;
    VkSamplerMipmapMode mm;
    int max_lod_x100;
    bool operator<(const SamplerKey& o) const {
        return std::tie(min_f, mag_f, ws, wt, compare, op, aniso_x10, mm, max_lod_x100) <
               std::tie(o.min_f, o.mag_f, o.ws, o.wt, o.compare, o.op, o.aniso_x10, o.mm, o.max_lod_x100);
    }
};

struct FrameData {
    id<MTLBuffer> ring = nil;
    size_t ring_offset = 0;
    NSMutableArray* deferred = nil;  // (objects kept until the frame's commands are done)
};

struct Context {
    bool active = false;
    bool headless = false;
    bool vsync = true;
    GLFWwindow* window = nullptr;

    id<MTLDevice> dev = nil;
    id<MTLCommandQueue> queue = nil;
    CAMetalLayer* layer = nil;
    dispatch_semaphore_t frame_sem = nullptr;
    VkFormat depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;

    GpuImage main_color{}, main_depth{}, scaled_color{}, scaled_depth{}, imgui_color{};
    bool scaled_active = false;

    FrameData frames[kFramesInFlight];
    uint32_t frame_idx = 0;
    uint64_t frame_num = 0;
    bool in_frame = false;

    id<MTLCommandBuffer> cmd = nil;
    int imm_depth = 0;  // (an immediate command buffer: out of a frame, committed and waited for when done)
    id<MTLRenderCommandEncoder> renc = nil;
    id<MTLComputeCommandEncoder> cenc = nil;
    id<MTLBlitCommandEncoder> benc = nil;

    std::vector<RenderTarget> target_stack;
    int vp[4] = {0, 0, 1, 1};
    float vp_min_d = 0.0f, vp_max_d = 1.0f;
    bool scissor_enabled = false;
    int scissor_rect[4] = {0, 0, 1, 1};
    bool scissor_empty = false;

    // the state kept between draws (Vulkan's command buffer state)
    const Pipeline* pipe = nullptr;
    const ComputePipeline* cpipe = nullptr;
    Slot slots[kSets][kBindings];
    uint8_t push[256] = {};
    size_t push_size = 0;
    id<MTLBuffer> vbufs[4] = {nil, nil, nil, nil};
    size_t voffs[4] = {0, 0, 0, 0};
    id<MTLBuffer> ibuf = nil;
    std::unordered_set<void*> gpu_written;  // (the buffers a compute shader is given: their contents the GPU's, not read here)
    size_t ioff = 0;
    MTLIndexType itype = MTLIndexTypeUInt32;
    float bias_const = 0, bias_clamp = 0, bias_slope = 0;
    uint32_t stencil_ref = 0;
    float blend_const[4] = {0, 0, 0, 0};
    std::vector<id<MTLTexture>> units_tex;
    std::vector<id<MTLSamplerState>> units_smp;
    // what the current render encoder has, so that a draw sets only what changed
    const Pipeline* enc_pipe = nullptr;
    bool enc_state_dirty = true;

    std::unordered_map<std::string, Shader*> shaders;
    std::map<SamplerKey, id<MTLSamplerState>> samplers;
    id<MTLLibrary> util_lib = nil;
    std::map<std::pair<int, int>, id<MTLRenderPipelineState>> util_pipes;  // (kind, format)
    id<MTLDepthStencilState> ds_always_write = nil;
    id<MTLDepthStencilState> ds_off = nil;

    GpuImage white_tex{}, white_tex_array{}, black_uint_tex{}, dummy_d2d{}, dummy_darray{}, dummy_dcube{};
    std::vector<void (*)()> shutdown_callbacks;
    size_t ring_peak = 0;
} g;

// ---- Vulkan's enums to Metal's ----
MTLPixelFormat pixel_format(VkFormat f) {
    switch (f) {
    case VK_FORMAT_R8G8B8A8_UNORM: return MTLPixelFormatRGBA8Unorm;
    case VK_FORMAT_R8G8B8A8_UINT: return MTLPixelFormatRGBA8Uint;
    case VK_FORMAT_B8G8R8A8_UNORM: return MTLPixelFormatBGRA8Unorm;
    case VK_FORMAT_R8_UNORM: return MTLPixelFormatR8Unorm;
    case VK_FORMAT_R8G8_UNORM: return MTLPixelFormatRG8Unorm;
    case VK_FORMAT_R16G16B16A16_SFLOAT: return MTLPixelFormatRGBA16Float;
    case VK_FORMAT_R16G16B16A16_SINT: return MTLPixelFormatRGBA16Sint;
    case VK_FORMAT_R16G16_SINT: return MTLPixelFormatRG16Sint;
    case VK_FORMAT_R16G16_SFLOAT: return MTLPixelFormatRG16Float;
    case VK_FORMAT_R16_SFLOAT: return MTLPixelFormatR16Float;
    case VK_FORMAT_R32_SFLOAT: return MTLPixelFormatR32Float;
    case VK_FORMAT_R32_UINT: return MTLPixelFormatR32Uint;
    case VK_FORMAT_R32G32_SFLOAT: return MTLPixelFormatRG32Float;
    case VK_FORMAT_R32G32_UINT: return MTLPixelFormatRG32Uint;
    case VK_FORMAT_R32G32B32A32_SFLOAT: return MTLPixelFormatRGBA32Float;
    case VK_FORMAT_R32G32B32A32_UINT: return MTLPixelFormatRGBA32Uint;
    case VK_FORMAT_D32_SFLOAT: return MTLPixelFormatDepth32Float;
    case VK_FORMAT_D16_UNORM: return MTLPixelFormatDepth16Unorm;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT: return MTLPixelFormatDepth32Float_Stencil8;
    default: break;
    }
    std::fprintf(stderr, "[mtl] no Metal pixel format for VkFormat %d\n", static_cast<int>(f));
    return MTLPixelFormatRGBA8Unorm;
}
size_t pixel_bytes(VkFormat f) {
    switch (f) {
    case VK_FORMAT_R8_UNORM: return 1;
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_D16_UNORM: return 2;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R32G32_UINT: return 8;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R32G32B32A32_UINT: return 16;
    default: return 4;
    }
}
bool has_stencil(VkFormat f) { return f == VK_FORMAT_D32_SFLOAT_S8_UINT || f == VK_FORMAT_D24_UNORM_S8_UINT; }
bool is_depth(VkFormat f) { return f == VK_FORMAT_D32_SFLOAT || f == VK_FORMAT_D16_UNORM || has_stencil(f); }

MTLVertexFormat vertex_format(VkFormat f) {
    switch (f) {
    case VK_FORMAT_R32_SFLOAT: return MTLVertexFormatFloat;
    case VK_FORMAT_R32G32_SFLOAT: return MTLVertexFormatFloat2;
    case VK_FORMAT_R32G32B32_SFLOAT: return MTLVertexFormatFloat3;
    case VK_FORMAT_R32G32B32A32_SFLOAT: return MTLVertexFormatFloat4;
    case VK_FORMAT_R32_UINT: return MTLVertexFormatUInt;
    case VK_FORMAT_R32G32_UINT: return MTLVertexFormatUInt2;
    case VK_FORMAT_R32G32B32A32_UINT: return MTLVertexFormatUInt4;
    case VK_FORMAT_R8G8B8A8_UNORM: return MTLVertexFormatUChar4Normalized;
    case VK_FORMAT_R8G8B8A8_UINT: return MTLVertexFormatUChar4;
    case VK_FORMAT_R16G16_SINT: return MTLVertexFormatShort2;
    case VK_FORMAT_R16G16B16A16_SINT: return MTLVertexFormatShort4;
    case VK_FORMAT_R16G16B16A16_SFLOAT: return MTLVertexFormatHalf4;
    default: break;
    }
    std::fprintf(stderr, "[mtl] no Metal vertex format for VkFormat %d\n", static_cast<int>(f));
    return MTLVertexFormatFloat4;
}
MTLCompareFunction compare_fn(VkCompareOp op) {
    switch (op) {
    case VK_COMPARE_OP_NEVER: return MTLCompareFunctionNever;
    case VK_COMPARE_OP_LESS: return MTLCompareFunctionLess;
    case VK_COMPARE_OP_EQUAL: return MTLCompareFunctionEqual;
    case VK_COMPARE_OP_LESS_OR_EQUAL: return MTLCompareFunctionLessEqual;
    case VK_COMPARE_OP_GREATER: return MTLCompareFunctionGreater;
    case VK_COMPARE_OP_NOT_EQUAL: return MTLCompareFunctionNotEqual;
    case VK_COMPARE_OP_GREATER_OR_EQUAL: return MTLCompareFunctionGreaterEqual;
    default: return MTLCompareFunctionAlways;
    }
}
MTLStencilOperation stencil_op(VkStencilOp op) {
    switch (op) {
    case VK_STENCIL_OP_ZERO: return MTLStencilOperationZero;
    case VK_STENCIL_OP_REPLACE: return MTLStencilOperationReplace;
    case VK_STENCIL_OP_INCREMENT_AND_CLAMP: return MTLStencilOperationIncrementClamp;
    case VK_STENCIL_OP_DECREMENT_AND_CLAMP: return MTLStencilOperationDecrementClamp;
    case VK_STENCIL_OP_INVERT: return MTLStencilOperationInvert;
    case VK_STENCIL_OP_INCREMENT_AND_WRAP: return MTLStencilOperationIncrementWrap;
    case VK_STENCIL_OP_DECREMENT_AND_WRAP: return MTLStencilOperationDecrementWrap;
    default: return MTLStencilOperationKeep;
    }
}
MTLBlendFactor blend_factor(VkBlendFactor f) {
    switch (f) {
    case VK_BLEND_FACTOR_ZERO: return MTLBlendFactorZero;
    case VK_BLEND_FACTOR_ONE: return MTLBlendFactorOne;
    case VK_BLEND_FACTOR_SRC_COLOR: return MTLBlendFactorSourceColor;
    case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return MTLBlendFactorOneMinusSourceColor;
    case VK_BLEND_FACTOR_DST_COLOR: return MTLBlendFactorDestinationColor;
    case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return MTLBlendFactorOneMinusDestinationColor;
    case VK_BLEND_FACTOR_SRC_ALPHA: return MTLBlendFactorSourceAlpha;
    case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: return MTLBlendFactorOneMinusSourceAlpha;
    case VK_BLEND_FACTOR_DST_ALPHA: return MTLBlendFactorDestinationAlpha;
    case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return MTLBlendFactorOneMinusDestinationAlpha;
    case VK_BLEND_FACTOR_CONSTANT_COLOR: return MTLBlendFactorBlendColor;
    case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR: return MTLBlendFactorOneMinusBlendColor;
    case VK_BLEND_FACTOR_CONSTANT_ALPHA: return MTLBlendFactorBlendAlpha;
    case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA: return MTLBlendFactorOneMinusBlendAlpha;
    case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE: return MTLBlendFactorSourceAlphaSaturated;
    case VK_BLEND_FACTOR_SRC1_COLOR: return MTLBlendFactorSource1Color;
    case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR: return MTLBlendFactorOneMinusSource1Color;
    case VK_BLEND_FACTOR_SRC1_ALPHA: return MTLBlendFactorSource1Alpha;
    case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA: return MTLBlendFactorOneMinusSource1Alpha;
    default: return MTLBlendFactorOne;
    }
}
MTLBlendOperation blend_op(VkBlendOp op) {
    switch (op) {
    case VK_BLEND_OP_SUBTRACT: return MTLBlendOperationSubtract;
    case VK_BLEND_OP_REVERSE_SUBTRACT: return MTLBlendOperationReverseSubtract;
    case VK_BLEND_OP_MIN: return MTLBlendOperationMin;
    case VK_BLEND_OP_MAX: return MTLBlendOperationMax;
    default: return MTLBlendOperationAdd;
    }
}
MTLSamplerAddressMode address_mode(VkSamplerAddressMode m) {
    switch (m) {
    case VK_SAMPLER_ADDRESS_MODE_REPEAT: return MTLSamplerAddressModeRepeat;
    case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT: return MTLSamplerAddressModeMirrorRepeat;
    case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER: return MTLSamplerAddressModeClampToBorderColor;
    default: return MTLSamplerAddressModeClampToEdge;
    }
}

// ---- objects as void* (retained) ----
void* retain(id o) { return o == nil ? nullptr : (__bridge_retained void*)o; }
void release(void*& p) {
    if (p == nullptr) return;
    id o = (__bridge_transfer id)p;
    (void)o;
    p = nullptr;
}
template <typename T> T obj(void* p) { return (__bridge T)p; }

// ---- command buffers and encoders ----
// The profiler's fence (its timestamps' encoders wait for the work before them, else the GPU runs them early, beside it):
// updated by each encoder at its end while the profiler takes timestamps (a write in this frame or the one before)
id<MTLFence> g_prof_fence = nil;
uint64_t g_prof_write_frame = 0;
bool prof_fencing() { return g_prof_fence != nil && g.frame_num <= g_prof_write_frame + 1; }
void prof_fence(id<MTLRenderCommandEncoder> e) {
    if (prof_fencing()) [e updateFence:g_prof_fence afterStages:MTLRenderStageFragment];
}

void end_encoder() {
    if (g.renc != nil) {
        prof_fence(g.renc);
        [g.renc endEncoding];
        g.renc = nil;
    }
    if (g.cenc != nil) {
        if (prof_fencing()) [g.cenc updateFence:g_prof_fence];
        [g.cenc endEncoding];
        g.cenc = nil;
    }
    if (g.benc != nil) {
        if (prof_fencing()) [g.benc updateFence:g_prof_fence];
        [g.benc endEncoding];
        g.benc = nil;
    }
}

// The command buffer to record into: the frame's, or (out of a frame) an immediate one, committed and waited for at the
// scope's end
struct CmdScope {
    bool imm = false;
    CmdScope() {
        if (g.in_frame && g.cmd != nil) return;
        if (g.imm_depth++ == 0) g.cmd = [g.queue commandBuffer];
        imm = true;
    }
    ~CmdScope() {
        if (!imm) return;
        if (--g.imm_depth == 0) {
            end_encoder();
            [g.cmd commit];
            [g.cmd waitUntilCompleted];
            g.cmd = nil;
        }
    }
};

id<MTLBlitCommandEncoder> blit_enc() {
    if (g.benc != nil) return g.benc;
    end_encoder();
    g.benc = [g.cmd blitCommandEncoder];
    return g.benc;
}
id<MTLComputeCommandEncoder> compute_enc() {
    if (g.cenc != nil) return g.cenc;
    end_encoder();
    g.cenc = [g.cmd computeCommandEncoder];
    return g.cenc;
}

void defer_release(id o) {
    if (o == nil) return;
    FrameData& f = g.frames[g.in_frame ? g.frame_idx : (g.frame_idx + kFramesInFlight - 1) % kFramesInFlight];
    [f.deferred addObject:o];
}

// ---- shaders ----
Shader* shader(const char* name) {
    if (name == nullptr) return nullptr;
    auto it = g.shaders.find(name);
    if (it != g.shaders.end()) return it->second;
    const ShaderInfo* info = nullptr;
    for (const ShaderInfo& si : kShaderInfos)
        if (std::strcmp(si.name, name) == 0) info = &si;
    const char* src = nullptr;
    for (const ShaderSrc& s : kShaderSrcs)
        if (std::strcmp(s.name, name) == 0) src = s.src;
    Shader* sh = nullptr;
    if (info != nullptr && src != nullptr) {
        MTLCompileOptions* opts = [MTLCompileOptions new];
        opts.languageVersion = MTLLanguageVersion3_0;
        // fast maths for the pixels; the vertices' (the depths compared EQUAL between passes) and the compute shaders'
        // (the tfrag's rounding, tests/metal/tfrag_double_check.mm) as precise as Vulkan's
        if (@available(macOS 15.0, *)) {
            opts.mathMode = std::strcmp(info->stage, "frag") == 0 ? MTLMathModeFast : MTLMathModeSafe;
        } else {
            opts.fastMathEnabled = std::strcmp(info->stage, "frag") == 0;
        }
        opts.preserveInvariance = YES;
        NSError* err = nil;
        id<MTLLibrary> lib = [g.dev newLibraryWithSource:[NSString stringWithUTF8String:src] options:opts error:&err];
        if (lib == nil) {
            std::fprintf(stderr, "[mtl] shader %s: %s\n", name, err ? err.localizedDescription.UTF8String : "?");
        } else {
            sh = new Shader{};
            sh->info = info;
            sh->fn = [lib newFunctionWithName:[NSString stringWithUTF8String:name]];
            sh->flat_inputs = std::strstr(src, "// rac1: flat inputs") != nullptr;
            if (std::strcmp(info->stage, "comp") == 0) {
                // (the workgroup size: tools/gen_metal_shaders.py's mark, from the GLSL's layout)
                static const std::regex re(R"(// rac1: local size (\d+) (\d+) (\d+))");
                std::cmatch m;
                if (std::regex_search(src, m, re))
                    sh->local_size = MTLSizeMake(std::stoul(m[1].str()), std::stoul(m[2].str()), std::stoul(m[3].str()));
                else
                    std::fprintf(stderr, "[mtl] shader %s: no workgroup size mark (run tools/gen_metal_shaders.py)\n", name);
            }
        }
    } else {
        std::fprintf(stderr, "[mtl] no shader %s in this build\n", name);
    }
    g.shaders[name] = sh;
    return sh;
}

id<MTLRenderPipelineState> util_pipeline(int kind, MTLPixelFormat fmt) {  // kind 0 colour, 1 depth+stencil, 2 depth
    auto& p = g.util_pipes[{kind, static_cast<int>(fmt)}];
    if (p != nil) return p;
    if (g.util_lib == nil) {
        NSError* err = nil;
        g.util_lib = [g.dev newLibraryWithSource:[NSString stringWithUTF8String:kUtilSrc] options:[MTLCompileOptions new] error:&err];
        if (g.util_lib == nil) {
            std::fprintf(stderr, "[mtl] the utility shaders: %s\n", err ? err.localizedDescription.UTF8String : "?");
            return nil;
        }
    }
    MTLRenderPipelineDescriptor* d = [MTLRenderPipelineDescriptor new];
    d.vertexFunction = [g.util_lib newFunctionWithName:@"util_vs"];
    d.fragmentFunction = [g.util_lib newFunctionWithName:kind == 0 ? @"util_color_fs" : kind == 1 ? @"util_ds_fs" : @"util_d_fs"];
    if (kind == 0) {
        d.colorAttachments[0].pixelFormat = fmt;
    } else {
        d.depthAttachmentPixelFormat = fmt;
        if (fmt == MTLPixelFormatDepth32Float_Stencil8) d.stencilAttachmentPixelFormat = fmt;
    }
    NSError* err = nil;
    p = [g.dev newRenderPipelineStateWithDescriptor:d error:&err];
    if (p == nil) std::fprintf(stderr, "[mtl] a utility pipeline: %s\n", err ? err.localizedDescription.UTF8String : "?");
    return p;
}

// ---- render passes ----
void apply_viewport_and_scissor() {
    if (g.renc == nil || g.target_stack.empty()) return;
    const RenderTarget& rt = g.target_stack.back();
    MTLViewport v;
    v.originX = g.vp[0];
    v.originY = g.vp[1];
    v.width = std::max(1, g.vp[2]);
    v.height = std::max(1, g.vp[3]);
    v.znear = g.vp_min_d;
    v.zfar = g.vp_max_d;
    [g.renc setViewport:v];
    MTLScissorRect sc;
    g.scissor_empty = false;
    if (g.scissor_enabled) {
        const int x0 = std::clamp(g.scissor_rect[0], 0, static_cast<int>(rt.width));
        const int y0 = std::clamp(g.scissor_rect[1], 0, static_cast<int>(rt.height));
        const int x1 = std::clamp(g.scissor_rect[0] + std::max(0, g.scissor_rect[2]), x0, static_cast<int>(rt.width));
        const int y1 = std::clamp(g.scissor_rect[1] + std::max(0, g.scissor_rect[3]), y0, static_cast<int>(rt.height));
        sc = {static_cast<NSUInteger>(x0), static_cast<NSUInteger>(y0), static_cast<NSUInteger>(x1 - x0), static_cast<NSUInteger>(y1 - y0)};
        if (x1 <= x0 || y1 <= y0) {  // (Metal has no empty scissor: the draws are skipped instead)
            g.scissor_empty = true;
            sc = {0, 0, 1, 1};
        }
    } else {
        sc = {0, 0, rt.width, rt.height};
    }
    [g.renc setScissorRect:sc];
}

void begin_rendering_internal(const RenderTarget& rt, bool clear_color, const float rgba[4], bool clear_depth, float depth_val,
                              bool clear_stencil, uint32_t stencil_val) {
    end_encoder();
    MTLRenderPassDescriptor* rp = [MTLRenderPassDescriptor renderPassDescriptor];
    if (rt.color != nullptr) {
        rp.colorAttachments[0].texture = obj<id<MTLTexture>>(rt.color);
        rp.colorAttachments[0].slice = rt.color_layer;
        rp.colorAttachments[0].level = rt.color_level;
        rp.colorAttachments[0].loadAction = clear_color ? MTLLoadActionClear : MTLLoadActionLoad;
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        if (clear_color && rgba != nullptr) rp.colorAttachments[0].clearColor = MTLClearColorMake(rgba[0], rgba[1], rgba[2], rgba[3]);
        else if (clear_color) rp.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 0);
    }
    if (rt.depth != nullptr) {
        rp.depthAttachment.texture = obj<id<MTLTexture>>(rt.depth);
        rp.depthAttachment.slice = rt.depth_layer;
        rp.depthAttachment.loadAction = clear_depth ? MTLLoadActionClear : MTLLoadActionLoad;
        rp.depthAttachment.storeAction = MTLStoreActionStore;
        rp.depthAttachment.clearDepth = depth_val;
    }
    if (rt.stencil != nullptr) {
        rp.stencilAttachment.texture = obj<id<MTLTexture>>(rt.stencil);
        rp.stencilAttachment.slice = rt.depth_layer;
        rp.stencilAttachment.loadAction = clear_stencil ? MTLLoadActionClear : MTLLoadActionLoad;
        rp.stencilAttachment.storeAction = MTLStoreActionStore;
        rp.stencilAttachment.clearStencil = stencil_val;
    }
    if (rt.color == nullptr && rt.depth == nullptr) {
        rp.renderTargetWidth = rt.width;
        rp.renderTargetHeight = rt.height;
        rp.defaultRasterSampleCount = 1;
    }
    g.renc = [g.cmd renderCommandEncoderWithDescriptor:rp];
    g.enc_pipe = nullptr;
    g.enc_state_dirty = true;
    apply_viewport_and_scissor();
}

RenderTarget image_target(GpuImage& color, GpuImage* depth) {
    RenderTarget rt{};
    rt.color = color.image;
    rt.color_format = color.format;
    if (depth != nullptr && depth->image != nullptr) {
        rt.depth = depth->image;
        rt.depth_format = depth->format;
        if (has_stencil(depth->format)) rt.stencil = depth->image;
    }
    rt.width = color.width;
    rt.height = color.height;
    return rt;
}

void ensure_main_targets(int w, int h) {
    w = std::max(1, w);
    h = std::max(1, h);
    if (g.main_color.image != nullptr && g.main_color.width == static_cast<uint32_t>(w) && g.main_color.height == static_cast<uint32_t>(h)) return;
    destroy_image(g.main_color);
    destroy_image(g.main_depth);
    create_image(w, h, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                 g.main_color);
    create_image(w, h, g.depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                 VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, g.main_depth);
}

id<MTLTexture> stencil_texture(GpuImage& img) {
    if (img.stencil_view == nullptr) {
        id<MTLTexture> t = obj<id<MTLTexture>>(img.image);
        img.stencil_view = retain([t newTextureViewWithPixelFormat:MTLPixelFormatX32_Stencil8]);
    }
    return obj<id<MTLTexture>>(img.stencil_view);
}

// A region of an image into another, scaled, by the utility shaders (dst_tex: a texture of no GpuImage, the drawable)
void blit_draw(GpuImage& src, int sx0, int sy0, int sx1, int sy1, id<MTLTexture> dst_tex, VkFormat dst_format, uint32_t dst_w,
               uint32_t dst_h, int dx0, int dy0, int dx1, int dy1, VkFilter filter, VkImageAspectFlags aspect) {
    const bool depth = (aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;
    const bool stencil = depth && has_stencil(src.format) && has_stencil(dst_format) && (aspect & VK_IMAGE_ASPECT_STENCIL_BIT);
    end_encoder();
    MTLRenderPassDescriptor* rp = [MTLRenderPassDescriptor renderPassDescriptor];
    if (!depth) {
        rp.colorAttachments[0].texture = dst_tex;
        rp.colorAttachments[0].loadAction = MTLLoadActionLoad;
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
    } else {
        rp.depthAttachment.texture = dst_tex;
        rp.depthAttachment.loadAction = MTLLoadActionLoad;
        rp.depthAttachment.storeAction = MTLStoreActionStore;
        if (has_stencil(dst_format)) {
            rp.stencilAttachment.texture = dst_tex;
            rp.stencilAttachment.loadAction = MTLLoadActionLoad;
            rp.stencilAttachment.storeAction = MTLStoreActionStore;
        }
    }
    id<MTLRenderCommandEncoder> e = [g.cmd renderCommandEncoderWithDescriptor:rp];
    id<MTLRenderPipelineState> ps = util_pipeline(!depth ? 0 : stencil ? 1 : 2, [dst_tex pixelFormat]);
    if (ps != nil) {
        [e setRenderPipelineState:ps];
        const int x0 = std::clamp(std::min(dx0, dx1), 0, static_cast<int>(dst_w));
        const int x1 = std::clamp(std::max(dx0, dx1), 0, static_cast<int>(dst_w));
        const int y0 = std::clamp(std::min(dy0, dy1), 0, static_cast<int>(dst_h));
        const int y1 = std::clamp(std::max(dy0, dy1), 0, static_cast<int>(dst_h));
        if (x1 > x0 && y1 > y0) {
            [e setViewport:(MTLViewport){0, 0, static_cast<double>(dst_w), static_cast<double>(dst_h), 0, 1}];
            [e setScissorRect:(MTLScissorRect){static_cast<NSUInteger>(x0), static_cast<NSUInteger>(y0), static_cast<NSUInteger>(x1 - x0),
                                               static_cast<NSUInteger>(y1 - y0)}];
            if (depth) {
                if (g.ds_always_write == nil) {
                    MTLDepthStencilDescriptor* dd = [MTLDepthStencilDescriptor new];
                    dd.depthCompareFunction = MTLCompareFunctionAlways;
                    dd.depthWriteEnabled = YES;
                    MTLStencilDescriptor* sd = [MTLStencilDescriptor new];
                    sd.stencilCompareFunction = MTLCompareFunctionAlways;
                    sd.depthStencilPassOperation = MTLStencilOperationReplace;
                    dd.frontFaceStencil = sd;
                    dd.backFaceStencil = sd;
                    g.ds_always_write = [g.dev newDepthStencilStateWithDescriptor:dd];
                }
                [e setDepthStencilState:g.ds_always_write];
            }
            BlitParams bp{};
            bp.d0[0] = static_cast<float>(dx0);
            bp.d0[1] = static_cast<float>(dy0);
            bp.d1[0] = static_cast<float>(dx1);
            bp.d1[1] = static_cast<float>(dy1);
            bp.s0[0] = static_cast<float>(sx0);
            bp.s0[1] = static_cast<float>(sy0);
            bp.s1[0] = static_cast<float>(sx1);
            bp.s1[1] = static_cast<float>(sy1);
            bp.src_size[0] = static_cast<float>(src.width);
            bp.src_size[1] = static_cast<float>(src.height);
            [e setFragmentBytes:&bp length:sizeof bp atIndex:0];
            [e setFragmentTexture:obj<id<MTLTexture>>(src.image) atIndex:0];
            if (stencil) [e setFragmentTexture:stencil_texture(src) atIndex:1];
            if (!depth)
                [e setFragmentSamplerState:(__bridge id<MTLSamplerState>)get_sampler(filter, filter, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                                                                      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                                   atIndex:0];
            [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        }
    }
    prof_fence(e);
    [e endEncoding];
}

void create_fallback_textures() {
    create_image(1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, g.white_tex);
    create_image(1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, g.white_tex_array, 1, 0,
                 VK_IMAGE_VIEW_TYPE_2D_ARRAY);
    create_image(1, 1, VK_FORMAT_R32G32B32A32_UINT, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, g.black_uint_tex);
    create_image(4, 4, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                 VK_IMAGE_ASPECT_DEPTH_BIT, g.dummy_d2d);
    create_image(4, 4, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                 VK_IMAGE_ASPECT_DEPTH_BIT, g.dummy_darray, 3, 0, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
    create_image(4, 4, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                 VK_IMAGE_ASPECT_DEPTH_BIT, g.dummy_dcube, 6, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, VK_IMAGE_VIEW_TYPE_CUBE_ARRAY);
    const uint32_t white = 0xFFFFFFFFu;
    const uint32_t zero[4] = {0, 0, 0, 0};
    upload_image_2d(g.white_tex, &white, 4);
    upload_image_2d(g.white_tex_array, &white, 4);
    upload_image_2d(g.black_uint_tex, zero, 16);
    for (GpuImage* d : {&g.dummy_d2d, &g.dummy_darray, &g.dummy_dcube}) clear_depth_stencil_image(*d, 1.0f, 0);
}

// ---- the profiler (prof::GpuApi): the GPU's timestamps ----
// Apple's GPUs sample their timestamp counter at encoder boundaries only (MTLCounterSamplingPointAtStageBoundary), not
// between draws: a timestamp is the open encoder ended (a render pass reopens itself, loading, at its next draw) and an
// compute encoder whose start is sampled (with one thread of a kernel that does nothing: an empty encoder is skipped, its
// sample never written). A sample buffer a slot (a frame in flight), read back when the last
// command buffer that wrote in it is done. The counter's ticks to the CPU's clock by the device's CPU / GPU clock pairs. Only while the
// profiler records (its overlay, its dumps): the split render passes cost a load and a store each. A fence orders them
// (g_prof_fence): without it the GPU ran them beside the work around them, the timestamps out of order
struct ProfSlot {
    id<MTLCounterSampleBuffer> buf = nil;
    id<MTLCommandBuffer> cmd = nil;  // the last command buffer that sampled in the slot
    std::vector<int> src;            // a query's sample (its own, or an earlier one's when it could not be sampled; -1: none)
};
struct ProfState {
    std::vector<ProfSlot> slots;
    int per_slot = 0;
    MTLTimestamp cpu0 = 0, gpu0 = 0;  // the clocks' first pair (the ticks' origin and scale)
    double ns_per_tick = 1.0;
    id<MTLComputePipelineState> nop = nil;  // the timestamps' encoders' one thread
} g_prof;

void prof_calibrate() {
    MTLTimestamp cpu = 0, gpu = 0;
    [g.dev sampleTimestamps:&cpu gpuTimestamp:&gpu];
    if (gpu > g_prof.gpu0 + 1000 && cpu > g_prof.cpu0)
        g_prof.ns_per_tick = static_cast<double>(cpu - g_prof.cpu0) / static_cast<double>(gpu - g_prof.gpu0);
}

bool prof_alloc(int slots, int per_slot) {
    if (g.dev == nil || ![g.dev supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary]) return false;
    id<MTLCounterSet> ts = nil;
    for (id<MTLCounterSet> cs in g.dev.counterSets)
        if ([cs.name isEqualToString:MTLCommonCounterSetTimestamp]) ts = cs;
    if (ts == nil) return false;
    {
        NSError* err = nil;
        id<MTLLibrary> lib = [g.dev newLibraryWithSource:@"kernel void rac1_prof_nop() {}" options:nil error:&err];
        id<MTLFunction> fn = lib != nil ? [lib newFunctionWithName:@"rac1_prof_nop"] : nil;
        g_prof.nop = fn != nil ? [g.dev newComputePipelineStateWithFunction:fn error:&err] : nil;
        if (g_prof.nop == nil) return false;
    }
    g_prof_fence = [g.dev newFence];
    MTLCounterSampleBufferDescriptor* d = [MTLCounterSampleBufferDescriptor new];
    d.counterSet = ts;
    d.storageMode = MTLStorageModeShared;
    d.sampleCount = static_cast<NSUInteger>(per_slot);
    g_prof.slots.assign(static_cast<size_t>(slots), ProfSlot{});
    for (ProfSlot& sl : g_prof.slots) {
        NSError* err = nil;
        sl.buf = [g.dev newCounterSampleBufferWithDescriptor:d error:&err];
        if (sl.buf == nil) {
            std::fprintf(stderr, "[mtl] profiler: no counter sample buffer (%s): no GPU timings\n", err != nil ? err.localizedDescription.UTF8String : "?");
            g_prof.slots.clear();
            return false;
        }
        sl.src.assign(static_cast<size_t>(per_slot), -1);
    }
    g_prof.per_slot = per_slot;
    [g.dev sampleTimestamps:&g_prof.cpu0 gpuTimestamp:&g_prof.gpu0];
    return true;
}
void prof_slot_reset(int slot) {
    if (slot < 0 || slot >= static_cast<int>(g_prof.slots.size())) return;
    ProfSlot& sl = g_prof.slots[static_cast<size_t>(slot)];
    sl.cmd = nil;
    std::fill(sl.src.begin(), sl.src.end(), -1);
}
void prof_write(int slot, int q) {
    if (slot < 0 || slot >= static_cast<int>(g_prof.slots.size()) || q < 0 || q >= g_prof.per_slot) return;
    ProfSlot& sl = g_prof.slots[static_cast<size_t>(slot)];
    if (!g.in_frame || g.cmd == nil) {  // (out of a frame: the slot's timestamp before, if any)
        sl.src[static_cast<size_t>(q)] = q > 0 ? sl.src[static_cast<size_t>(q - 1)] : -1;
        return;
    }
    g_prof_write_frame = g.frame_num;
    end_encoder();
    MTLComputePassDescriptor* cp = [MTLComputePassDescriptor computePassDescriptor];
    cp.sampleBufferAttachments[0].sampleBuffer = sl.buf;
    cp.sampleBufferAttachments[0].startOfEncoderSampleIndex = static_cast<NSUInteger>(q);
    cp.sampleBufferAttachments[0].endOfEncoderSampleIndex = MTLCounterDontSample;
    id<MTLComputeCommandEncoder> e = [g.cmd computeCommandEncoderWithDescriptor:cp];
    [e waitForFence:g_prof_fence];  // (after the work before it; and the timestamps in order: each updates it too)
    [e updateFence:g_prof_fence];
    [e setComputePipelineState:g_prof.nop];
    [e dispatchThreadgroups:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
    [e endEncoding];
    sl.src[static_cast<size_t>(q)] = q;
    sl.cmd = g.cmd;
}
bool prof_read(int slot, int n, uint64_t* ns) {
    if (slot < 0 || slot >= static_cast<int>(g_prof.slots.size()) || n <= 0 || n > g_prof.per_slot) return false;
    ProfSlot& sl = g_prof.slots[static_cast<size_t>(slot)];
    if (sl.cmd == nil) {  // (no sample: a host frame that drew nothing on the GPU, its timings 0)
        std::fill(ns, ns + n, 0);
        return true;
    }
    if (sl.cmd.status != MTLCommandBufferStatusCompleted) return false;
    NSData* data = [sl.buf resolveCounterRange:NSMakeRange(0, static_cast<NSUInteger>(n))];
    if (data == nil || data.length < static_cast<NSUInteger>(n) * sizeof(MTLCounterResultTimestamp)) return false;
    const auto* r = static_cast<const MTLCounterResultTimestamp*>(data.bytes);
    prof_calibrate();
    // (a query not sampled - out of a frame, or an error value - takes the one before; those before the first sample, the first's)
    int first = -1;
    for (int i = 0; i < n; ++i) {
        const int k = sl.src[static_cast<size_t>(i)];
        const uint64_t t = k >= 0 ? r[k].timestamp : 0;
        if (k < 0 || t == MTLCounterErrorValue || t < g_prof.gpu0) {
            ns[i] = i > 0 ? ns[i - 1] : 0;
            continue;
        }
        ns[i] = static_cast<uint64_t>(static_cast<double>(t - g_prof.gpu0) * g_prof.ns_per_tick);
        if (first < 0) first = i;
    }
    if (first < 0) first = 0;
    for (int i = 0; i < first; ++i) ns[i] = ns[first];
    return true;
}
// the GPU's clock now (ms, the ticks' origin as the timestamps'): the trace's calibration against the CPU's
double prof_now_ms() {
    if (g.dev == nil || g_prof.slots.empty()) return 0.0;
    MTLTimestamp cpu = 0, gpu = 0;
    [g.dev sampleTimestamps:&cpu gpuTimestamp:&gpu];
    return gpu > g_prof.gpu0 ? static_cast<double>(gpu - g_prof.gpu0) * g_prof.ns_per_tick * 1e-6 : 0.0;
}
void prof_resources(prof::Resources* r) {
    r->api = "Metal";
    r->gl_renderer = g.dev != nil ? g.dev.name.UTF8String : "";
    r->gl_vendor = "Metal 3";
    r->vk_budget = g.dev != nil;
    if (g.dev != nil) {
        r->vk_heap_size = g.dev.recommendedMaxWorkingSetSize;
        r->vk_heap_budget = g.dev.recommendedMaxWorkingSetSize;
        r->vk_heap_usage = g.dev.currentAllocatedSize;
    }
}
void prof_scan(std::vector<prof::GlOwnerRow>*) {}
const prof::GpuApi kProfApi = {"Metal", prof_alloc, prof_slot_reset, prof_write, prof_read, prof_now_ms, prof_resources, prof_scan};

// ---- the state applied at a draw ----
id<MTLRenderPipelineState> render_state(const Pipeline* p, const RenderTarget& rt) {
    const int cf = rt.color != nullptr ? static_cast<int>(pixel_format(rt.color_format)) : 0;
    const int df = rt.depth != nullptr ? static_cast<int>(pixel_format(rt.depth_format)) : 0;
    const int sf = rt.stencil != nullptr ? static_cast<int>(pixel_format(rt.depth_format)) : 0;
    auto& st = const_cast<Pipeline*>(p)->states[{cf, df, sf}];
    if (st != nil) return st;
    const GraphicsPipelineDesc& d = p->desc;
    MTLRenderPipelineDescriptor* rd = [MTLRenderPipelineDescriptor new];
    rd.vertexFunction = p->vs != nullptr ? p->vs->fn : nil;
    rd.fragmentFunction = p->fs != nullptr ? p->fs->fn : nil;
    rd.vertexDescriptor = p->vd;
    rd.inputPrimitiveTopology = p->prim == MTLPrimitiveTypePoint  ? MTLPrimitiveTopologyClassPoint
                                : p->prim == MTLPrimitiveTypeLine ? MTLPrimitiveTopologyClassLine
                                                                  : MTLPrimitiveTopologyClassTriangle;
    if (cf != 0) {
        MTLRenderPipelineColorAttachmentDescriptor* ca = rd.colorAttachments[0];
        ca.pixelFormat = static_cast<MTLPixelFormat>(cf);
        ca.blendingEnabled = d.blend_enable;
        ca.sourceRGBBlendFactor = blend_factor(d.src_color_blend);
        ca.destinationRGBBlendFactor = blend_factor(d.dst_color_blend);
        ca.rgbBlendOperation = blend_op(d.color_blend_op);
        ca.sourceAlphaBlendFactor = blend_factor(d.src_alpha_blend);
        ca.destinationAlphaBlendFactor = blend_factor(d.dst_alpha_blend);
        ca.alphaBlendOperation = blend_op(d.alpha_blend_op);
        MTLColorWriteMask m = MTLColorWriteMaskNone;
        if (d.color_write_mask & VK_COLOR_COMPONENT_R_BIT) m |= MTLColorWriteMaskRed;
        if (d.color_write_mask & VK_COLOR_COMPONENT_G_BIT) m |= MTLColorWriteMaskGreen;
        if (d.color_write_mask & VK_COLOR_COMPONENT_B_BIT) m |= MTLColorWriteMaskBlue;
        if (d.color_write_mask & VK_COLOR_COMPONENT_A_BIT) m |= MTLColorWriteMaskAlpha;
        ca.writeMask = m;
    }
    rd.depthAttachmentPixelFormat = static_cast<MTLPixelFormat>(df);
    rd.stencilAttachmentPixelFormat = static_cast<MTLPixelFormat>(sf);
    if (cf == 0 && df == 0) {
        rd.rasterSampleCount = 1;
    }
    NSError* err = nil;
    st = [g.dev newRenderPipelineStateWithDescriptor:rd error:&err];
    if (st == nil)
        std::fprintf(stderr, "[mtl] pipeline %s/%s: %s\n", d.vs ? d.vs : "-", d.fs ? d.fs : "-",
                     err ? err.localizedDescription.UTF8String : "?");
    return st;
}

void bind_stage_resources(const Shader* sh, bool vertex) {
    if (sh == nullptr) return;
    id<MTLRenderCommandEncoder> e = g.renc;
    for (const Bind& b : sh->info->binds) {
        if (b.set >= kSets || b.binding >= kBindings) continue;
        const Slot& s = g.slots[b.set][b.binding];
        switch (b.kind) {
        case kBuffer:
            if (s.buffer == nil) continue;
            if (vertex) [e setVertexBuffer:s.buffer offset:s.offset atIndex:b.index];
            else [e setFragmentBuffer:s.buffer offset:s.offset atIndex:b.index];
            break;
        case kTexture:
            if (s.texture == nil) continue;
            if (vertex) [e setVertexTexture:s.texture atIndex:b.index];
            else [e setFragmentTexture:s.texture atIndex:b.index];
            break;
        case kSampler:
            if (s.sampler == nil) continue;
            if (vertex) [e setVertexSamplerState:s.sampler atIndex:b.index];
            else [e setFragmentSamplerState:s.sampler atIndex:b.index];
            break;
        }
    }
    if (sh->info->push >= 0 && g.push_size > 0) {
        if (vertex) [e setVertexBytes:g.push length:g.push_size atIndex:sh->info->push];
        else [e setFragmentBytes:g.push length:g.push_size atIndex:sh->info->push];
    }
    if (!vertex && sh->info->argbuf >= 0 && !g.units_tex.empty()) {
        // descriptor set 2: the texture units, an argument buffer of the textures' and the samplers' resource IDs (Metal 3)
        const size_t n = g.units_tex.size();
        RingAlloc r = alloc_ring(16 * 2 * sizeof(uint64_t));
        auto* ids = static_cast<uint64_t*>(r.ptr);
        std::memset(ids, 0, 32 * sizeof(uint64_t));
        for (size_t i = 0; i < n && i < 16; ++i) {
            if (g.units_tex[i] != nil) {
                MTLResourceID rid = g.units_tex[i].gpuResourceID;
                std::memcpy(&ids[i], &rid, 8);
                [e useResource:g.units_tex[i] usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
            }
            if (g.units_smp[i] != nil) {
                MTLResourceID rid = g.units_smp[i].gpuResourceID;
                std::memcpy(&ids[16 + i], &rid, 8);
            }
        }
        [e setFragmentBuffer:obj<id<MTLBuffer>>(r.buffer) offset:r.offset atIndex:sh->info->argbuf];
    }
}

bool prepare_draw() {
    if (g.pipe == nullptr) return false;
    ensure_rendering();
    if (g.renc == nil || g.scissor_empty) return false;
    const RenderTarget& rt = g.target_stack.back();
    const Pipeline* p = g.pipe;
    id<MTLRenderPipelineState> ps = render_state(p, rt);
    if (ps == nil) return false;
    id<MTLRenderCommandEncoder> e = g.renc;
    [e setRenderPipelineState:ps];
    if (g.enc_pipe != p || g.enc_state_dirty) {
        const GraphicsPipelineDesc& d = p->desc;
        [e setDepthStencilState:rt.depth != nullptr ? p->ds : p->ds_none];
        [e setCullMode:(d.cull_mode & VK_CULL_MODE_FRONT_AND_BACK) == VK_CULL_MODE_FRONT_AND_BACK ? MTLCullModeNone  // (no FRONT_AND_BACK)
                       : (d.cull_mode & VK_CULL_MODE_BACK_BIT)                                     ? MTLCullModeBack
                       : (d.cull_mode & VK_CULL_MODE_FRONT_BIT)                                    ? MTLCullModeFront
                                                                                                   : MTLCullModeNone];
        [e setFrontFacingWinding:MTLWindingCounterClockwise];
        [e setDepthClipMode:d.depth_clamp ? MTLDepthClipModeClamp : MTLDepthClipModeClip];
        [e setTriangleFillMode:d.polygon_mode == VK_POLYGON_MODE_LINE ? MTLTriangleFillModeLines : MTLTriangleFillModeFill];
        g.enc_pipe = p;
        g.enc_state_dirty = false;
    }
    const GraphicsPipelineDesc& d = p->desc;
    if (p->dyn_bias) [e setDepthBias:g.bias_const slopeScale:g.bias_slope clamp:g.bias_clamp];
    else if (d.depth_bias_enable) [e setDepthBias:d.depth_bias_constant slopeScale:d.depth_bias_slope clamp:0];
    else [e setDepthBias:0 slopeScale:0 clamp:0];
    if (d.stencil_test) [e setStencilReferenceValue:p->dyn_stencil_ref ? g.stencil_ref : d.stencil_front.reference];
    [e setBlendColorRed:g.blend_const[0] green:g.blend_const[1] blue:g.blend_const[2] alpha:g.blend_const[3]];
    for (uint32_t b = 0; b < 4; ++b)
        if (g.vbufs[b] != nil) [e setVertexBuffer:g.vbufs[b] offset:g.voffs[b] atIndex:kVertexBufferBase + b];
    bind_stage_resources(p->vs, true);
    bind_stage_resources(p->fs, false);
    return true;
}

}  // namespace

// ==== the API ====

RenderTarget color_target(const GpuImage& color, const GpuImage* depth, uint32_t layer, uint32_t level) {
    RenderTarget rt{};
    rt.color = color.image;
    rt.color_format = color.format;
    rt.color_layer = layer;
    rt.color_level = level;
    if (depth != nullptr && depth->image != nullptr) {
        rt.depth = depth->image;
        rt.depth_format = depth->format;
        rt.depth_layer = layer;
        if (has_stencil(depth->format)) rt.stencil = depth->image;
    }
    rt.width = std::max(1u, color.width >> level);
    rt.height = std::max(1u, color.height >> level);
    return rt;
}

RenderTarget depth_target(const GpuImage& depth, uint32_t layer) {
    RenderTarget rt{};
    rt.depth = depth.image;
    rt.depth_format = depth.format;
    rt.depth_layer = layer;
    if (has_stencil(depth.format)) rt.stencil = depth.image;
    rt.width = depth.width;
    rt.height = depth.height;
    return rt;
}

bool available() {
    @autoreleasepool {
        return MTLCreateSystemDefaultDevice() != nil;
    }
}

bool init(GLFWwindow* window, bool vsync, bool headless) {
    if (g.active) return true;
    g.window = window;
    g.vsync = vsync;
    g.headless = headless;
    g.dev = MTLCreateSystemDefaultDevice();
    if (g.dev == nil) {
        std::fprintf(stderr, "[mtl] no Metal device\n");
        return false;
    }
    if (![g.dev supportsFamily:MTLGPUFamilyMetal3]) {
        std::fprintf(stderr, "[mtl] %s does not support Metal 3\n", g.dev.name.UTF8String);
        g.dev = nil;
        return false;
    }
    g.queue = [g.dev newCommandQueue];
    g.frame_sem = dispatch_semaphore_create(kFramesInFlight);
    for (FrameData& f : g.frames) {
        f.ring = [g.dev newBufferWithLength:kRingBufferSize options:MTLResourceStorageModeShared];
        f.ring.label = @"frame ring";
        f.deferred = [NSMutableArray new];
    }
    int w = 960, h = 720;
    if (window != nullptr) glfwGetFramebufferSize(window, &w, &h);
    if (!headless && window != nullptr) {
        NSWindow* nsw = glfwGetCocoaWindow(window);
        g.layer = [CAMetalLayer layer];
        g.layer.device = g.dev;
        g.layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        g.layer.framebufferOnly = YES;
        g.layer.maximumDrawableCount = 3;
        g.layer.displaySyncEnabled = vsync;
        g.layer.contentsScale = nsw.backingScaleFactor;
        g.layer.drawableSize = CGSizeMake(std::max(1, w), std::max(1, h));
        nsw.contentView.layer = g.layer;
        nsw.contentView.wantsLayer = YES;
    }
    ensure_main_targets(w, h);
    create_fallback_textures();
    prof::set_gpu_api(&kProfApi);
    g.active = true;
    std::printf("[mtl] Metal initialized on %s (%dx%d)\n", g.dev.name.UTF8String, w, h);
    return true;
}

void shutdown() {
    if (g.dev == nil) return;
    wait_idle();
    for (auto fn : g.shutdown_callbacks)
        if (fn != nullptr) fn();
    g.shutdown_callbacks.clear();
    for (GpuImage* im : {&g.white_tex, &g.white_tex_array, &g.black_uint_tex, &g.dummy_d2d, &g.dummy_darray, &g.dummy_dcube, &g.main_color,
                         &g.main_depth, &g.scaled_color, &g.scaled_depth, &g.imgui_color})
        destroy_image_now(*im);
    for (FrameData& f : g.frames) {
        [f.deferred removeAllObjects];
        f.ring = nil;
    }
    for (auto& [k, s] : g.shaders) delete s;
    g.shaders.clear();
    g.samplers.clear();
    g.util_pipes.clear();
    g.util_lib = nil;
    prof::set_gpu_api(nullptr);
    g.layer = nil;
    g.queue = nil;
    g.dev = nil;
    g.active = false;
}

bool is_active() { return g.active; }
bool in_frame() { return g.in_frame; }

void wait_idle() {
    if (g.queue == nil) return;
    id<MTLCommandBuffer> cb = [g.queue commandBuffer];
    [cb commit];
    [cb waitUntilCompleted];
}

void set_vsync(bool vsync) {
    g.vsync = vsync;
    if (g.layer != nil) g.layer.displaySyncEnabled = vsync;
}

void begin_frame(int win_w, int win_h, float r, float gr, float b, float a) {
    if (!g.active) return;
    if (g.in_frame) end_frame();
    win_w = std::max(1, win_w);
    win_h = std::max(1, win_h);
    {
        RAC1_PROF_ZONE(kPresent);  // (the GPU still on the frame before last: as an OpenGL swap that blocks)
        dispatch_semaphore_wait(g.frame_sem, DISPATCH_TIME_FOREVER);
    }
    FrameData& f = g.frames[g.frame_idx];
    g.ring_peak = std::max(g.ring_peak, f.ring_offset);
    [f.deferred removeAllObjects];
    f.ring_offset = 0;
    ensure_main_targets(win_w, win_h);
    g.cmd = [g.queue commandBuffer];
    g.in_frame = true;
    g.scaled_active = false;
    g.target_stack.clear();
    g.target_stack.push_back(image_target(g.main_color, &g.main_depth));
    g.vp[0] = 0;
    g.vp[1] = 0;
    g.vp[2] = win_w;
    g.vp[3] = win_h;
    g.vp_min_d = 0.0f;
    g.vp_max_d = 1.0f;
    g.scissor_enabled = false;
    const float rgba[4] = {r, gr, b, a};
    begin_rendering_internal(g.target_stack.back(), true, rgba, true, 1.0f, true, 0);
}

void imgui_init() {
    if (!g.active) return;
    ImGui_ImplMetal_Init(g.dev);
    // (its device objects and font texture made now: made at the first ImGui_ImplMetal_NewFrame - in render_imgui, after
    // the frame's draw lists were made with the font texture's ID - they would replace the texture those lists name)
    ImGui_ImplMetal_CreateDeviceObjects(g.dev);
}
const char* device_name() { return g.dev != nil ? g.dev.name.UTF8String : ""; }

void imgui_shutdown() {
    if (!g.active) return;
    wait_idle();
    ImGui_ImplMetal_Shutdown();
}

void render_imgui(ImDrawData* draw_data) {
    if (!g.active || !g.in_frame || draw_data == nullptr) return;
    const int w = static_cast<int>(g.main_color.width), h = static_cast<int>(g.main_color.height);
    if (g.imgui_color.image == nullptr || g.imgui_color.width != g.main_color.width || g.imgui_color.height != g.main_color.height) {
        destroy_image(g.imgui_color);
        create_image(w, h, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                     g.imgui_color);
    }
    // the frame (bottom-up) flipped into imgui_color (top-down, as ImGui draws), ImGui drawn, and flipped back
    blit_image(g.main_color, 0, h, w, 0, g.imgui_color, 0, 0, w, h);
    end_encoder();
    MTLRenderPassDescriptor* rp = [MTLRenderPassDescriptor renderPassDescriptor];
    rp.colorAttachments[0].texture = obj<id<MTLTexture>>(g.imgui_color.image);
    rp.colorAttachments[0].loadAction = MTLLoadActionLoad;
    rp.colorAttachments[0].storeAction = MTLStoreActionStore;
    ImGui_ImplMetal_NewFrame(rp);
    id<MTLRenderCommandEncoder> e = [g.cmd renderCommandEncoderWithDescriptor:rp];
    ImGui_ImplMetal_RenderDrawData(draw_data, g.cmd, e);
    prof_fence(e);
    [e endEncoding];
    blit_image(g.imgui_color, 0, h, w, 0, g.main_color, 0, 0, w, h);
}

void end_frame() {
    if (!g.active || !g.in_frame) return;
    end_encoder();
    FrameData& f = g.frames[g.frame_idx];
    id<CAMetalDrawable> drawable = nil;
    if (g.layer != nil) {
        int fw = 0, fh = 0;
        glfwGetFramebufferSize(g.window, &fw, &fh);
        if (fw > 0 && fh > 0) {
            const CGSize ds = g.layer.drawableSize;
            if (static_cast<int>(ds.width) != fw || static_cast<int>(ds.height) != fh) g.layer.drawableSize = CGSizeMake(fw, fh);
            {
                RAC1_PROF_ZONE(kPresent);
                drawable = [g.layer nextDrawable];
            }
        }
    }
    if (drawable != nil) {
        // the frame (row 0 the bottom, as OpenGL's) to the drawable (row 0 the top)
        id<MTLTexture> t = drawable.texture;
        blit_draw(g.main_color, 0, static_cast<int>(g.main_color.height), static_cast<int>(g.main_color.width), 0, t,
                  VK_FORMAT_B8G8R8A8_UNORM, static_cast<uint32_t>(t.width), static_cast<uint32_t>(t.height), 0, 0,
                  static_cast<int>(t.width), static_cast<int>(t.height), VK_FILTER_NEAREST, VK_IMAGE_ASPECT_COLOR_BIT);
        [g.cmd presentDrawable:drawable];
    }
    dispatch_semaphore_t sem = g.frame_sem;
    [g.cmd addCompletedHandler:^(id<MTLCommandBuffer>) {
      dispatch_semaphore_signal(sem);
    }];
    [g.cmd commit];
    g.cmd = nil;
    g.in_frame = false;
    (void)f;
    g.frame_idx = (g.frame_idx + 1) % kFramesInFlight;
    ++g.frame_num;
}

void flush_cmd() {
    if (!g.active || !g.in_frame) return;
    end_encoder();
    [g.cmd commit];
    [g.cmd waitUntilCompleted];
    g.cmd = [g.queue commandBuffer];
}

void submit_immediate(const std::function<void()>& fn) {
    if (g.dev == nil) return;
    if (g.in_frame) {
        end_encoder();
        fn();
        flush_cmd();
        return;
    }
    CmdScope scope;
    fn();
}

void* device() { return (__bridge void*)g.dev; }
void* command_queue() { return (__bridge void*)g.queue; }
VkFormat color_format() { return VK_FORMAT_R8G8B8A8_UNORM; }
VkFormat depth_stencil_format() { return g.depth_format; }
uint32_t frame_index() { return g.frame_idx; }
uint64_t frame_number() { return g.frame_num; }
int main_width() { return static_cast<int>(g.main_color.width); }
int main_height() { return static_cast<int>(g.main_color.height); }
int current_target_width() { return g.target_stack.empty() ? main_width() : static_cast<int>(g.target_stack.back().width); }
int current_target_height() { return g.target_stack.empty() ? main_height() : static_cast<int>(g.target_stack.back().height); }
const RenderTarget& current_target() {
    static const RenderTarget kEmpty{};
    return g.target_stack.empty() ? kEmpty : g.target_stack.back();
}

GpuImage& main_color_image() { return g.main_color; }
GpuImage& main_depth_image() { return g.main_depth; }
GpuImage& active_color_image() { return g.scaled_active ? g.scaled_color : g.main_color; }
GpuImage& active_depth_image() { return g.scaled_active ? g.scaled_depth : g.main_depth; }
bool scaled_scene_active() { return g.scaled_active; }

void scaled_scene_begin(int, int, int bw, int bh) {
    bw = std::max(1, bw);
    bh = std::max(1, bh);
    if (g.scaled_color.image == nullptr || g.scaled_color.width != static_cast<uint32_t>(bw) || g.scaled_color.height != static_cast<uint32_t>(bh)) {
        destroy_image(g.scaled_color);
        destroy_image(g.scaled_depth);
        create_image(bw, bh, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT, g.scaled_color);
        create_image(bw, bh, g.depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, g.scaled_depth);
    }
    end_encoder();
    g.scaled_active = true;
    const float clear_col[4] = {0.02f, 0.02f, 0.04f, 1.0f};
    set_viewport(0, 0, bw, bh);
    push_target(image_target(g.scaled_color, &g.scaled_depth), true, clear_col, true, 1.0f, true, 0);
}

void scaled_scene_end(int win_w, int win_h) {
    if (!g.scaled_active) return;
    pop_target();
    g.scaled_active = false;
    const int sw = static_cast<int>(g.scaled_color.width), sh = static_cast<int>(g.scaled_color.height);
    blit_image(g.scaled_color, 0, 0, sw, sh, g.main_color, 0, 0, win_w, win_h, VK_FILTER_LINEAR);
    blit_image(g.scaled_depth, 0, 0, sw, sh, g.main_depth, 0, 0, win_w, win_h, VK_FILTER_NEAREST,
               VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    set_viewport(0, 0, win_w, win_h);
}

void push_target(const RenderTarget& rt, bool clear_color, const float rgba[4], bool clear_depth, float depth_val, bool clear_stencil,
                 uint32_t stencil_val) {
    end_encoder();
    g.target_stack.push_back(rt);
    if (clear_color || clear_depth || clear_stencil) begin_rendering_internal(rt, clear_color, rgba, clear_depth, depth_val, clear_stencil, stencil_val);
}

void pop_target() {
    end_encoder();
    if (g.target_stack.size() > 1) g.target_stack.pop_back();
}

void ensure_rendering() {
    if (!g.in_frame || g.target_stack.empty()) return;
    if (g.renc == nil) begin_rendering_internal(g.target_stack.back(), false, nullptr, false, 1.0f, false, 0);
}

void end_rendering() {
    if (g.renc != nil) end_encoder();
}

void set_viewport(int x, int y, int w, int h, float min_depth, float max_depth) {
    g.vp[0] = x;
    g.vp[1] = y;
    g.vp[2] = std::max(1, w);
    g.vp[3] = std::max(1, h);
    g.vp_min_d = min_depth;
    g.vp_max_d = max_depth;
    if (g.renc != nil) apply_viewport_and_scissor();
}

void get_viewport(int out_vp[4]) {
    for (int i = 0; i < 4; ++i) out_vp[i] = g.vp[i];
}

void set_scissor(bool enable, int x, int y, int w, int h) {
    g.scissor_enabled = enable;
    if (enable) {
        g.scissor_rect[0] = x;
        g.scissor_rect[1] = y;
        g.scissor_rect[2] = std::max(0, w);
        g.scissor_rect[3] = std::max(0, h);
    }
    if (g.renc != nil) apply_viewport_and_scissor();
}

void clear_attachments(bool color, const float rgba[4], bool depth, float depth_val, bool stencil, uint32_t stencil_val) {
    if (!g.in_frame || g.target_stack.empty()) return;
    // (Metal clears at a pass's start only: the pass begun again, clearing; the whole target, as vkCmdClearAttachments here)
    const RenderTarget& rt = g.target_stack.back();
    begin_rendering_internal(rt, color && rt.color != nullptr, rgba, depth && rt.depth != nullptr, depth_val, stencil && rt.stencil != nullptr,
                             stencil_val);
}

bool create_buffer(size_t size, GpuBuffer& out, bool map) {
    destroy_buffer(out);
    if (size == 0) return false;
    id<MTLBuffer> b = [g.dev newBufferWithLength:size options:MTLResourceStorageModeShared];
    if (b == nil) return false;
    out.buffer = retain(b);
    out.size = size;
    out.mapped = map ? b.contents : nullptr;
    return true;
}

void destroy_buffer_now(GpuBuffer& buf) {
    g.gpu_written.erase(buf.buffer);
    release(buf.buffer);
    buf = GpuBuffer{};
}

void destroy_buffer(GpuBuffer& buf) {
    if (buf.buffer == nullptr) return;
    if (g.active) defer_release((__bridge id)buf.buffer);
    destroy_buffer_now(buf);
}

bool upload_buffer(GpuBuffer& dst, const void* data, size_t size) {
    if (size == 0 || data == nullptr) return false;
    // a fresh buffer each time, the one before kept until the frames that use it are done (glBufferData's orphaning)
    if (!create_buffer(size, dst, true)) return false;
    std::memcpy(dst.mapped, data, size);
    return true;
}

RingAlloc alloc_ring(size_t size, size_t alignment) {
    size = std::max<size_t>(size, 16);
    alignment = std::max(alignment, kRingAlign);
    FrameData& f = g.frames[g.frame_idx];
    const size_t aligned = (f.ring_offset + alignment - 1) & ~(alignment - 1);
    if (aligned + size <= kRingBufferSize) {
        f.ring_offset = aligned + size;
        return RingAlloc{(__bridge void*)f.ring, aligned, static_cast<uint8_t*>(f.ring.contents) + aligned};
    }
    id<MTLBuffer> overflow = [g.dev newBufferWithLength:size options:MTLResourceStorageModeShared];
    [f.deferred addObject:overflow];
    return RingAlloc{(__bridge void*)overflow, 0, overflow.contents};
}

bool create_image(uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, GpuImage& out, uint32_t layers,
                  VkImageCreateFlags /*flags*/, VkImageViewType view_type, uint32_t mip_levels) {
    destroy_image(out);
    if (w == 0 || h == 0) return false;
    mip_levels = std::max(1u, mip_levels);
    layers = std::max(1u, layers);
    MTLTextureDescriptor* d = [MTLTextureDescriptor new];
    d.pixelFormat = pixel_format(format);
    d.width = w;
    d.height = h;
    d.mipmapLevelCount = mip_levels;
    switch (view_type) {
    case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
        d.textureType = MTLTextureType2DArray;
        d.arrayLength = layers;
        break;
    case VK_IMAGE_VIEW_TYPE_CUBE:
        d.textureType = MTLTextureTypeCube;
        break;
    case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
        d.textureType = MTLTextureTypeCubeArray;
        d.arrayLength = std::max(1u, layers / 6);
        break;
    default:
        d.textureType = layers > 1 ? MTLTextureType2DArray : MTLTextureType2D;
        if (layers > 1) d.arrayLength = layers;
        break;
    }
    MTLTextureUsage u = MTLTextureUsageShaderRead;
    if (usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) u |= MTLTextureUsageRenderTarget;
    if (usage & VK_IMAGE_USAGE_STORAGE_BIT) u |= MTLTextureUsageShaderWrite;
    if (has_stencil(format)) u |= MTLTextureUsagePixelFormatView;  // (its stencil sampled through a view)
    if (is_depth(format)) u |= MTLTextureUsageRenderTarget;  // (cleared and copied by a pass)
    d.usage = u;
    d.storageMode = MTLStorageModePrivate;
    id<MTLTexture> t = [g.dev newTextureWithDescriptor:d];
    if (t == nil) return false;
    out.image = retain(t);
    out.width = w;
    out.height = h;
    out.layers = layers;
    out.mip_levels = mip_levels;
    out.format = format;
    out.view_type = view_type;
    out.aspect = aspect;
    return true;
}

void destroy_image_now(GpuImage& img) {
    release(img.stencil_view);
    release(img.image);
    img = GpuImage{};
}

void destroy_image(GpuImage& img) {
    if (img.image == nullptr) return;
    if (g.active) {
        defer_release((__bridge id)img.image);
        if (img.stencil_view != nullptr) defer_release((__bridge id)img.stencil_view);
    }
    destroy_image_now(img);
}

void upload_image_2d(GpuImage& img, const void* data, size_t byte_size, int x, int y, int w, int h, int row_length, uint32_t layer,
                     uint32_t mip_level) {
    if (img.image == nullptr || data == nullptr || byte_size == 0) return;
    const uint32_t uw = w > 0 ? static_cast<uint32_t>(w) : std::max(1u, img.width >> mip_level);
    const uint32_t uh = h > 0 ? static_cast<uint32_t>(h) : std::max(1u, img.height >> mip_level);
    const size_t bpp = pixel_bytes(img.format);
    const size_t row = static_cast<size_t>(row_length > 0 ? row_length : static_cast<int>(uw)) * bpp;
    CmdScope scope;
    id<MTLBuffer> stage;
    size_t off = 0;
    if (g.in_frame) {
        RingAlloc r = alloc_ring(byte_size, 256);
        std::memcpy(r.ptr, data, byte_size);
        stage = obj<id<MTLBuffer>>(r.buffer);
        off = r.offset;
    } else {
        stage = [g.dev newBufferWithBytes:data length:byte_size options:MTLResourceStorageModeShared];
    }
    uint32_t slice = layer;
    if (obj<id<MTLTexture>>(img.image).textureType == MTLTextureTypeCubeArray || obj<id<MTLTexture>>(img.image).textureType == MTLTextureTypeCube) slice = layer;
    [blit_enc() copyFromBuffer:stage
                  sourceOffset:off
             sourceBytesPerRow:row
           sourceBytesPerImage:row * uh
                    sourceSize:MTLSizeMake(uw, uh, 1)
                     toTexture:obj<id<MTLTexture>>(img.image)
              destinationSlice:slice
              destinationLevel:mip_level
             destinationOrigin:MTLOriginMake(x, y, 0)];
}

void blit_image(GpuImage& src, int sx0, int sy0, int sx1, int sy1, GpuImage& dst, int dx0, int dy0, int dx1, int dy1, VkFilter filter,
                VkImageAspectFlags aspect) {
    if (src.image == nullptr || dst.image == nullptr) return;
    CmdScope scope;
    // the same size, not flipped, the same format: a copy
    if (sx1 - sx0 == dx1 - dx0 && sy1 - sy0 == dy1 - dy0 && sx1 > sx0 && sy1 > sy0 && src.format == dst.format) {
        copy_image(src, sx0, sy0, dst, dx0, dy0, sx1 - sx0, sy1 - sy0, aspect);
        return;
    }
    blit_draw(src, sx0, sy0, sx1, sy1, obj<id<MTLTexture>>(dst.image), dst.format, dst.width, dst.height, dx0, dy0, dx1, dy1, filter, aspect);
}

void copy_image(GpuImage& src, int sx, int sy, GpuImage& dst, int dx, int dy, int w, int h, VkImageAspectFlags /*aspect*/, uint32_t src_layer,
                uint32_t dst_layer) {
    if (src.image == nullptr || dst.image == nullptr || w <= 0 || h <= 0) return;
    CmdScope scope;
    [blit_enc() copyFromTexture:obj<id<MTLTexture>>(src.image)
                    sourceSlice:src_layer
                    sourceLevel:0
                   sourceOrigin:MTLOriginMake(sx, sy, 0)
                     sourceSize:MTLSizeMake(w, h, 1)
                      toTexture:obj<id<MTLTexture>>(dst.image)
               destinationSlice:dst_layer
               destinationLevel:0
              destinationOrigin:MTLOriginMake(dx, dy, 0)];
}

void generate_mipmaps(GpuImage& img) {
    if (img.image == nullptr || img.mip_levels <= 1) return;
    CmdScope scope;
    [blit_enc() generateMipmapsForTexture:obj<id<MTLTexture>>(img.image)];
}

void clear_color_image(GpuImage& img, const float rgba[4]) {
    if (img.image == nullptr) return;
    CmdScope scope;
    end_encoder();
    for (uint32_t l = 0; l < img.layers; ++l) {
        MTLRenderPassDescriptor* rp = [MTLRenderPassDescriptor renderPassDescriptor];
        rp.colorAttachments[0].texture = obj<id<MTLTexture>>(img.image);
        rp.colorAttachments[0].slice = l;
        rp.colorAttachments[0].loadAction = MTLLoadActionClear;
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        rp.colorAttachments[0].clearColor = MTLClearColorMake(rgba[0], rgba[1], rgba[2], rgba[3]);
        [[g.cmd renderCommandEncoderWithDescriptor:rp] endEncoding];
    }
}

void clear_depth_stencil_image(GpuImage& img, float depth, uint32_t stencil) {
    if (img.image == nullptr) return;
    CmdScope scope;
    end_encoder();
    for (uint32_t l = 0; l < img.layers; ++l) {
        MTLRenderPassDescriptor* rp = [MTLRenderPassDescriptor renderPassDescriptor];
        rp.depthAttachment.texture = obj<id<MTLTexture>>(img.image);
        rp.depthAttachment.slice = l;
        rp.depthAttachment.loadAction = MTLLoadActionClear;
        rp.depthAttachment.storeAction = MTLStoreActionStore;
        rp.depthAttachment.clearDepth = depth;
        if (has_stencil(img.format)) {
            rp.stencilAttachment.texture = obj<id<MTLTexture>>(img.image);
            rp.stencilAttachment.slice = l;
            rp.stencilAttachment.loadAction = MTLLoadActionClear;
            rp.stencilAttachment.storeAction = MTLStoreActionStore;
            rp.stencilAttachment.clearStencil = stencil;
        }
        [[g.cmd renderCommandEncoderWithDescriptor:rp] endEncoding];
    }
}

void copy_image_to_buffer(GpuImage& src, int x, int y, int w, int h, void* buffer, size_t offset, VkImageAspectFlags aspect, uint32_t layer) {
    if (src.image == nullptr || buffer == nullptr || w <= 0 || h <= 0) return;
    CmdScope scope;
    const bool depth = (aspect & VK_IMAGE_ASPECT_DEPTH_BIT) != 0 && is_depth(src.format);
    const size_t bpp = depth ? 4 : pixel_bytes(src.format);
    [blit_enc() copyFromTexture:obj<id<MTLTexture>>(src.image)
                       sourceSlice:layer
                       sourceLevel:0
                      sourceOrigin:MTLOriginMake(x, y, 0)
                        sourceSize:MTLSizeMake(w, h, 1)
                          toBuffer:obj<id<MTLBuffer>>(buffer)
                 destinationOffset:offset
            destinationBytesPerRow:static_cast<size_t>(w) * bpp
          destinationBytesPerImage:static_cast<size_t>(w) * h * bpp
                           options:depth && has_stencil(src.format) ? MTLBlitOptionDepthFromDepthStencil : MTLBlitOptionNone];
}

void fill_buffer(void* buffer, size_t offset, size_t size, uint8_t value) {
    if (buffer == nullptr || size == 0) return;
    CmdScope scope;
    [blit_enc() fillBuffer:obj<id<MTLBuffer>>(buffer) range:NSMakeRange(offset, size) value:value];
}

void copy_buffer(void* src, size_t src_offset, void* dst, size_t dst_offset, size_t size) {
    if (src == nullptr || dst == nullptr || size == 0) return;
    CmdScope scope;
    [blit_enc() copyFromBuffer:obj<id<MTLBuffer>>(src) sourceOffset:src_offset toBuffer:obj<id<MTLBuffer>>(dst) destinationOffset:dst_offset size:size];
}

void read_image_rgba(GpuImage& src, int w, int h, void* out_rgba) {
    if (src.image == nullptr || w <= 0 || h <= 0 || out_rgba == nullptr) return;
    w = std::min(w, static_cast<int>(src.width));
    h = std::min(h, static_cast<int>(src.height));
    const size_t bytes = static_cast<size_t>(w) * h * 4;
    id<MTLBuffer> stage = [g.dev newBufferWithLength:bytes options:MTLResourceStorageModeShared];
    {
        CmdScope scope;
        copy_image_to_buffer(src, 0, 0, w, h, (__bridge void*)stage, 0);
    }
    if (g.in_frame) flush_cmd();
    std::memcpy(out_rgba, stage.contents, bytes);
}

void save_ppm(const char* path, int w, int h) {
    if (w <= 0 || h <= 0) return;
    std::vector<uint8_t> px(static_cast<size_t>(w) * h * 4);
    read_image_rgba(g.main_color, w, h, px.data());
    FILE* f = std::fopen(path, "wb");
    if (f == nullptr) return;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; --y)
        for (int x = 0; x < w; ++x) std::fwrite(&px[(static_cast<size_t>(y) * w + x) * 4], 1, 3, f);
    std::fclose(f);
}

void* get_sampler(VkFilter min_filter, VkFilter mag_filter, VkSamplerAddressMode wrap_s, VkSamplerAddressMode wrap_t, bool compare,
                  VkCompareOp compare_op, float max_anisotropy, VkSamplerMipmapMode mipmap_mode, float max_lod) {
    const SamplerKey key{min_filter, mag_filter, wrap_s, wrap_t, compare, compare_op,
                         max_anisotropy > 1.0f ? static_cast<int>(max_anisotropy * 10.0f) : 10, mipmap_mode, static_cast<int>(max_lod * 100.0f)};
    auto& s = g.samplers[key];
    if (s != nil) return (__bridge void*)s;
    MTLSamplerDescriptor* d = [MTLSamplerDescriptor new];
    d.minFilter = min_filter == VK_FILTER_LINEAR ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
    d.magFilter = mag_filter == VK_FILTER_LINEAR ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
    // (a max LOD of 0.25: OpenGL's "no mipmaps", the base level only)
    d.mipFilter = max_lod <= 0.25f                                ? MTLSamplerMipFilterNotMipmapped
                  : mipmap_mode == VK_SAMPLER_MIPMAP_MODE_LINEAR ? MTLSamplerMipFilterLinear
                                                                 : MTLSamplerMipFilterNearest;
    d.sAddressMode = address_mode(wrap_s);
    d.tAddressMode = address_mode(wrap_t);
    d.rAddressMode = MTLSamplerAddressModeClampToEdge;
    d.borderColor = MTLSamplerBorderColorOpaqueWhite;
    d.maxAnisotropy = std::clamp(static_cast<int>(max_anisotropy), 1, 16);
    if (compare) d.compareFunction = compare_fn(compare_op);
    d.lodMinClamp = 0.0f;
    d.lodMaxClamp = max_lod;
    d.supportArgumentBuffers = YES;
    s = [g.dev newSamplerStateWithDescriptor:d];
    return (__bridge void*)s;
}

const GpuImage& white_texture() { return g.white_tex; }
const GpuImage& white_texture_array() { return g.white_tex_array; }
const GpuImage& black_uint_texture() { return g.black_uint_tex; }
const GpuImage& dummy_depth_2d() { return g.dummy_d2d; }
const GpuImage& dummy_depth_array() { return g.dummy_darray; }
const GpuImage& dummy_depth_cube_array() { return g.dummy_dcube; }

Pipeline* create_graphics_pipeline(const GraphicsPipelineDesc& desc) {
    auto* p = new Pipeline{};
    p->desc = desc;
    p->vs = shader(desc.vs);
    p->fs = shader(desc.fs);
    if (p->vs == nullptr || (desc.fs != nullptr && p->fs == nullptr)) {
        delete p;
        return nullptr;
    }
    switch (desc.topology) {
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST: p->prim = MTLPrimitiveTypePoint; break;
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST: p->prim = MTLPrimitiveTypeLine; break;
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: p->prim = MTLPrimitiveTypeLineStrip; break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: p->prim = MTLPrimitiveTypeTriangleStrip; break;
    default: p->prim = MTLPrimitiveTypeTriangle; break;
    }
    if (!desc.vertex_attribs.empty()) {
        MTLVertexDescriptor* vd = [MTLVertexDescriptor vertexDescriptor];
        for (const VkVertexInputBindingDescription& b : desc.vertex_bindings) {
            MTLVertexBufferLayoutDescriptor* l = vd.layouts[kVertexBufferBase + b.binding];
            if (b.stride == 0) {
                l.stepFunction = MTLVertexStepFunctionConstant;
                l.stepRate = 0;
                l.stride = 16;
            } else {
                l.stride = b.stride;
                l.stepFunction = b.inputRate == VK_VERTEX_INPUT_RATE_INSTANCE ? MTLVertexStepFunctionPerInstance : MTLVertexStepFunctionPerVertex;
                l.stepRate = 1;
            }
        }
        for (const VkVertexInputAttributeDescription& a : desc.vertex_attribs) {
            MTLVertexAttributeDescriptor* ad = vd.attributes[a.location];
            ad.format = vertex_format(a.format);
            ad.offset = a.offset;
            ad.bufferIndex = kVertexBufferBase + a.binding;
        }
        p->vd = vd;
    }
    for (VkDynamicState s : desc.extra_dynamic_states) {
        if (s == VK_DYNAMIC_STATE_DEPTH_BIAS) p->dyn_bias = true;
        if (s == VK_DYNAMIC_STATE_STENCIL_REFERENCE) p->dyn_stencil_ref = true;
    }
    MTLDepthStencilDescriptor* dd = [MTLDepthStencilDescriptor new];
    dd.depthCompareFunction = desc.depth_test ? compare_fn(desc.depth_compare) : MTLCompareFunctionAlways;
    dd.depthWriteEnabled = desc.depth_test && desc.depth_write;
    if (desc.stencil_test) {
        auto sd = [](const VkStencilOpState& s) {
            MTLStencilDescriptor* d = [MTLStencilDescriptor new];
            d.stencilCompareFunction = compare_fn(s.compareOp);
            d.stencilFailureOperation = stencil_op(s.failOp);
            d.depthFailureOperation = stencil_op(s.depthFailOp);
            d.depthStencilPassOperation = stencil_op(s.passOp);
            d.readMask = s.compareMask;
            d.writeMask = s.writeMask;
            return d;
        };
        dd.frontFaceStencil = sd(desc.stencil_front);
        dd.backFaceStencil = sd(desc.stencil_back);
    }
    p->ds = [g.dev newDepthStencilStateWithDescriptor:dd];
    if (g.ds_off == nil) g.ds_off = [g.dev newDepthStencilStateWithDescriptor:[MTLDepthStencilDescriptor new]];
    p->ds_none = g.ds_off;
    return p;
}

ComputePipeline* create_compute_pipeline(const char* cs) {
    Shader* sh = shader(cs);
    if (sh == nullptr || sh->fn == nil) return nullptr;
    NSError* err = nil;
    id<MTLComputePipelineState> st = [g.dev newComputePipelineStateWithFunction:sh->fn error:&err];
    if (st == nil) {
        std::fprintf(stderr, "[mtl] compute pipeline %s: %s\n", cs, err ? err.localizedDescription.UTF8String : "?");
        return nullptr;
    }
    auto* p = new ComputePipeline{};
    p->cs = sh;
    p->state = st;
    return p;
}

void destroy_pipeline(Pipeline* p) {
    if (p == nullptr) return;
    if (g.pipe == p) g.pipe = nullptr;
    if (g.enc_pipe == p) g.enc_pipe = nullptr;
    for (auto& [k, st] : p->states) defer_release(st);
    delete p;
}

void destroy_compute_pipeline(ComputePipeline* p) {
    if (p == nullptr) return;
    if (g.cpipe == p) g.cpipe = nullptr;
    defer_release(p->state);
    delete p;
}

void* stencil_view(GpuImage& img) { return img.image != nullptr && has_stencil(img.format) ? (__bridge void*)stencil_texture(img) : nullptr; }

void* create_texture_view(const GpuImage& img, VkImageViewType type, VkImageAspectFlags aspect, uint32_t base_layer, uint32_t layers,
                          uint32_t base_level, uint32_t levels) {
    if (img.image == nullptr) return nullptr;
    id<MTLTexture> t = obj<id<MTLTexture>>(img.image);
    MTLPixelFormat pf = t.pixelFormat;
    if (aspect == VK_IMAGE_ASPECT_STENCIL_BIT && has_stencil(img.format)) pf = MTLPixelFormatX32_Stencil8;
    MTLTextureType tt = MTLTextureType2D;
    switch (type) {
    case VK_IMAGE_VIEW_TYPE_2D_ARRAY: tt = MTLTextureType2DArray; break;
    case VK_IMAGE_VIEW_TYPE_CUBE: tt = MTLTextureTypeCube; break;
    case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY: tt = MTLTextureTypeCubeArray; break;
    default: break;
    }
    if (layers == VK_REMAINING_ARRAY_LAYERS) layers = img.layers - base_layer;
    if (levels == VK_REMAINING_MIP_LEVELS) levels = img.mip_levels - base_level;
    id<MTLTexture> v = [t newTextureViewWithPixelFormat:pf textureType:tt levels:NSMakeRange(base_level, levels) slices:NSMakeRange(base_layer, layers)];
    return retain(v);
}

void release_object(void* o) {
    if (o == nullptr) return;
    id obj_ = (__bridge_transfer id)o;
    if (g.active) defer_release(obj_);
}

void bind_pipeline(const Pipeline* p) { g.pipe = p; }
void bind_compute_pipeline(const ComputePipeline* p) { g.cpipe = p; }

void push_constants(const void* data, size_t size, size_t offset) {
    if (offset + size > sizeof g.push) return;
    std::memcpy(g.push + offset, data, size);
    g.push_size = std::max(g.push_size, (offset + size + 15) & ~size_t{15});
}

void bind_buffer(uint32_t set, uint32_t binding, void* buffer, size_t offset) {
    if (set >= kSets || binding >= kBindings) return;
    Slot& s = g.slots[set][binding];
    s.buffer = (__bridge id<MTLBuffer>)buffer;
    s.offset = offset;
}

void bind_ring(uint32_t set, uint32_t binding, const RingAlloc& r) { bind_buffer(set, binding, r.buffer, r.offset); }

void bind_image(uint32_t set, uint32_t binding, const GpuImage& img, void* sampler) { bind_texture(set, binding, img.image, sampler); }

void bind_texture(uint32_t set, uint32_t binding, void* texture, void* sampler) {
    if (set >= kSets || binding >= kBindings) return;
    Slot& s = g.slots[set][binding];
    s.texture = (__bridge id<MTLTexture>)texture;
    s.sampler = (__bridge id<MTLSamplerState>)sampler;
}

void bind_texture_units(void* const* textures, void* const* samplers, uint32_t count) {
    g.units_tex.assign(count, nil);
    g.units_smp.assign(count, nil);
    for (uint32_t i = 0; i < count; ++i) {
        g.units_tex[i] = (__bridge id<MTLTexture>)textures[i];
        g.units_smp[i] = (__bridge id<MTLSamplerState>)samplers[i];
    }
}

void unbind_all() {
    for (auto& set : g.slots)
        for (Slot& s : set) s = Slot{};
    g.units_tex.clear();
    g.units_smp.clear();
}

void bind_vertex_buffer(uint32_t binding, void* buffer, size_t offset) {
    if (binding >= 4) return;
    g.vbufs[binding] = (__bridge id<MTLBuffer>)buffer;
    g.voffs[binding] = offset;
}

void bind_index_buffer(void* buffer, size_t offset, VkIndexType type) {
    g.ibuf = (__bridge id<MTLBuffer>)buffer;
    g.ioff = offset;
    g.itype = type == VK_INDEX_TYPE_UINT16 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32;
}

void set_depth_bias(float constant, float clamp, float slope) {
    g.bias_const = constant;
    g.bias_clamp = clamp;
    g.bias_slope = slope;
}
void set_stencil_reference(uint32_t ref) { g.stencil_ref = ref; }
void set_blend_constants(const float rgba[4]) {
    for (int i = 0; i < 4; ++i) g.blend_const[i] = rgba[i];
}

// The provoking vertex: Vulkan's flat inputs are a triangle's last vertex's, Metal's its first's (no switch for it). A pipeline
// whose fragment shader has flat inputs (tools/gen_metal_shaders.py) draws its triangles' indices turned (a, b, c) -> (c, a, b):
// the same triangle and winding, the last vertex first. The indices are read here (the CPU's: the buffers are shared), turned
// into the frame's ring; those a compute shader writes are its own (the tfrag's: written turned) and drawn as they are.
namespace {
bool turned() { return g.pipe != nullptr && g.pipe->fs != nullptr && g.pipe->fs->flat_inputs && g.pipe->prim == MTLPrimitiveTypeTriangle; }
bool cpu_readable(id<MTLBuffer> b) { return b != nil && b.storageMode == MTLStorageModeShared && g.gpu_written.count((__bridge void*)b) == 0; }

template <typename T> void turn(const T* src, T* dst, uint32_t n) {
    uint32_t t = 0;
    for (; t + 3 <= n; t += 3) {
        const T a = src[t], b = src[t + 1], c = src[t + 2];  // (read first: src may be dst)
        dst[t] = c;
        dst[t + 1] = a;
        dst[t + 2] = b;
    }
    for (; t < n; ++t) dst[t] = src[t];
}

void draw_turned_sequence(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) {
    RingAlloc r = alloc_ring(static_cast<size_t>(vertex_count) * 4, 4);
    auto* idx = static_cast<uint32_t*>(r.ptr);
    for (uint32_t i = 0; i < vertex_count; ++i) idx[i] = first_vertex + i;
    turn(idx, idx, vertex_count);  // (in place: each triangle's three read before written)
    [g.renc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                       indexCount:vertex_count
                        indexType:MTLIndexTypeUInt32
                      indexBuffer:(__bridge id<MTLBuffer>)r.buffer
                indexBufferOffset:r.offset
                    instanceCount:instance_count
                       baseVertex:0
                     baseInstance:first_instance];
}

void draw_turned_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) {
    const size_t isz = g.itype == MTLIndexTypeUInt16 ? 2 : 4;
    const size_t at = g.ioff + static_cast<size_t>(first_index) * isz;
    if (at + index_count * isz > g.ibuf.length) return;
    RingAlloc r = alloc_ring(index_count * isz, 4);
    const auto* src = static_cast<const uint8_t*>(g.ibuf.contents) + at;
    if (isz == 2) turn(reinterpret_cast<const uint16_t*>(src), static_cast<uint16_t*>(r.ptr), index_count);
    else turn(reinterpret_cast<const uint32_t*>(src), static_cast<uint32_t*>(r.ptr), index_count);
    [g.renc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                       indexCount:index_count
                        indexType:g.itype
                      indexBuffer:(__bridge id<MTLBuffer>)r.buffer
                indexBufferOffset:r.offset
                    instanceCount:instance_count
                       baseVertex:vertex_offset
                     baseInstance:first_instance];
}
}  // namespace

void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) {
    if (vertex_count == 0 || instance_count == 0 || !prepare_draw()) return;
    if (turned()) {
        draw_turned_sequence(vertex_count, instance_count, first_vertex, first_instance);
        return;
    }
    [g.renc drawPrimitives:g.pipe->prim vertexStart:first_vertex vertexCount:vertex_count instanceCount:instance_count baseInstance:first_instance];
}

void draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) {
    if (index_count == 0 || instance_count == 0 || g.ibuf == nil || !prepare_draw()) return;
    if (turned() && cpu_readable(g.ibuf)) {
        draw_turned_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);
        return;
    }
    const size_t isz = g.itype == MTLIndexTypeUInt16 ? 2 : 4;
    [g.renc drawIndexedPrimitives:g.pipe->prim
                       indexCount:index_count
                        indexType:g.itype
                      indexBuffer:g.ibuf
                indexBufferOffset:g.ioff + first_index * isz
                    instanceCount:instance_count
                       baseVertex:vertex_offset
                     baseInstance:first_instance];
}

void draw_indirect(void* buffer, size_t offset, uint32_t count, uint32_t stride) {
    if (count == 0 || buffer == nullptr || !prepare_draw()) return;
    id<MTLBuffer> b = (__bridge id<MTLBuffer>)buffer;
    if (turned() && cpu_readable(b)) {  // (the commands the CPU's: each drawn turned)
        for (uint32_t i = 0; i < count; ++i) {
            const size_t at = offset + static_cast<size_t>(i) * stride;
            if (at + 16 > b.length) break;
            const auto* c = reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(b.contents) + at);
            if (c[0] != 0 && c[1] != 0) draw_turned_sequence(c[0], c[1], c[2], c[3]);
        }
        return;
    }
    for (uint32_t i = 0; i < count; ++i)  // (Metal has no multi-draw: a draw a command, the same layout as Vulkan's)
        [g.renc drawPrimitives:g.pipe->prim indirectBuffer:b indirectBufferOffset:offset + static_cast<size_t>(i) * stride];
}

void draw_indexed_indirect(void* buffer, size_t offset, uint32_t count, uint32_t stride) {
    if (count == 0 || buffer == nullptr || g.ibuf == nil || !prepare_draw()) return;
    id<MTLBuffer> b = (__bridge id<MTLBuffer>)buffer;
    if (turned() && cpu_readable(b) && cpu_readable(g.ibuf)) {
        for (uint32_t i = 0; i < count; ++i) {
            const size_t at = offset + static_cast<size_t>(i) * stride;
            if (at + 20 > b.length) break;
            const auto* c = reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(b.contents) + at);
            if (c[0] != 0 && c[1] != 0) draw_turned_indexed(c[0], c[1], c[2], static_cast<int32_t>(c[3]), c[4]);
        }
        return;
    }
    for (uint32_t i = 0; i < count; ++i)
        [g.renc drawIndexedPrimitives:g.pipe->prim
                            indexType:g.itype
                          indexBuffer:g.ibuf
                    indexBufferOffset:g.ioff
                       indirectBuffer:b
                 indirectBufferOffset:offset + static_cast<size_t>(i) * stride];
}

void dispatch(uint32_t gx, uint32_t gy, uint32_t gz) {
    if (g.cpipe == nullptr || gx == 0 || gy == 0 || gz == 0) return;
    CmdScope scope;
    id<MTLComputeCommandEncoder> e = compute_enc();
    [e setComputePipelineState:g.cpipe->state];
    const Shader* sh = g.cpipe->cs;
    for (const Bind& b : sh->info->binds) {
        if (b.set >= kSets || b.binding >= kBindings) continue;
        const Slot& s = g.slots[b.set][b.binding];
        if (b.kind == kBuffer && s.buffer != nil) {
            [e setBuffer:s.buffer offset:s.offset atIndex:b.index];
            bool ring = false;  // (the rings: the CPU's, the compute shaders' inputs)
            for (const FrameData& f : g.frames) ring = ring || f.ring == s.buffer;
            if (!ring) g.gpu_written.insert((__bridge void*)s.buffer);
        } else if (b.kind == kTexture && s.texture != nil) [e setTexture:s.texture atIndex:b.index];
        else if (b.kind == kSampler && s.sampler != nil) [e setSamplerState:s.sampler atIndex:b.index];
    }
    if (sh->info->push >= 0 && g.push_size > 0) [e setBytes:g.push length:g.push_size atIndex:sh->info->push];
    [e dispatchThreadgroups:MTLSizeMake(gx, gy, gz) threadsPerThreadgroup:sh->local_size];
}

void register_shutdown_callback(void (*fn)()) {
    if (fn == nullptr) return;
    for (auto e : g.shutdown_callbacks)
        if (e == fn) return;
    g.shutdown_callbacks.push_back(fn);
}

}  // namespace rac1::mtl
