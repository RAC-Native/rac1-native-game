// The Metal tfrag compute shader's 64-bit integer doubles (tools/gen_metal_shaders.py) against the CPU's doubles in the
// round-toward-zero mode: the products and sums the Vulkan shader works out with the GPU's doubles (dmulz, daddz), the
// widening of the vertices' floats (f2d) and the narrowing of the results (d2fz), bit for bit. A developer's test, run by
// hand on a Mac:
//   clang++ -std=c++20 -O1 -frounding-math -fobjc-arc tests/metal/tfrag_double_check.mm -framework Metal -framework Foundation -o /tmp/tfdc
//   /tmp/tfdc runtime/mtl/shaders/kMrpTfragCs.metal
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

static const char* kTest = R"(
kernel void tfdc(device const ulong* a [[buffer(0)]], device const ulong* b [[buffer(1)]], device const float* f [[buffer(2)]],
                 device ulong* out [[buffer(3)]], uint i [[thread_position_in_grid]]) {
    ulong x = a[i], y = b[i];
    float g = f[i];
    out[4 * i + 0] = dmulz(x, y);
    ulong xs = x, ys = y;  // (daddz takes its arguments by reference, as SPIRV-Cross wrote it, and may swap them)
    out[4 * i + 1] = daddz(xs, ys);
    out[4 * i + 2] = f2d(g);
    out[4 * i + 3] = as_type<uint>(d2fz(x));
}
)";

static uint64_t bits(double d) { uint64_t u; std::memcpy(&u, &d, 8); return u; }
static double dbl(uint64_t u) { double d; std::memcpy(&d, &u, 8); return d; }
static uint32_t fbits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }

// the shader's d2fz on the CPU (a truncation of the bits, the exponent clamped)
static uint32_t d2fz(uint64_t d) {
    const uint32_t hi = static_cast<uint32_t>(d >> 32), lo = static_cast<uint32_t>(d);
    const uint32_t sign = hi & 0x80000000u;
    const int e = static_cast<int>((hi >> 20) & 0x7ffu) - 1023 + 127;
    if (e <= 0) return sign;
    if (e >= 255) return sign | 0x7f7fffffu;
    return sign | static_cast<uint32_t>(e) << 23 | (hi & 0xfffffu) << 3 | lo >> 29;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: %s kMrpTfragCs.metal\n", argv[0]); return 2; }
    @autoreleasepool {
        NSString* src = [NSString stringWithContentsOfFile:@(argv[1]) encoding:NSUTF8StringEncoding error:nil];
        if (src == nil) { std::fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
        src = [src stringByAppendingString:@(kTest)];
        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        MTLCompileOptions* o = [MTLCompileOptions new];
        o.mathMode = MTLMathModeSafe;
        NSError* err = nil;
        id<MTLLibrary> lib = [dev newLibraryWithSource:src options:o error:&err];
        if (lib == nil) { std::fprintf(stderr, "%s\n", err.localizedDescription.UTF8String); return 1; }
        id<MTLComputePipelineState> ps = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"tfdc"] error:&err];

        const size_t n = 1 << 22;
        std::mt19937_64 rng(1234);
        std::vector<uint64_t> a(n), b(n);
        std::vector<float> f(n);
        auto rand_double = [&](int spread) {
            // a sign, an exponent near 1023 (spread wide or narrow: the sums' cancellations), a random mantissa
            const uint64_t r = rng();
            const int e = 1023 + static_cast<int>(rng() % (2 * spread + 1)) - spread;
            return (r & 0x8000000000000000ull) | (static_cast<uint64_t>(e) << 52) | (r & 0x000fffffffffffffull);
        };
        for (size_t i = 0; i < n; ++i) {
            const int spread = (i & 3) == 0 ? 2 : (i & 3) == 1 ? 60 : 300;
            a[i] = rand_double(spread);
            b[i] = (i % 7) == 0 ? (a[i] ^ 0x8000000000000000ull) ^ (rng() & 0xff) : rand_double(spread);  // (near-cancellation)
            if (i % 101 == 0) b[i] = 0;
            uint32_t fu = static_cast<uint32_t>(rng());
            if (i % 13 == 0) fu &= 0x807fffffu;  // (float denormals)
            if ((fu & 0x7f800000u) == 0x7f800000u) fu &= 0xbfffffffu;
            std::memcpy(&f[i], &fu, 4);
        }
        id<MTLBuffer> ba = [dev newBufferWithBytes:a.data() length:n * 8 options:MTLResourceStorageModeShared];
        id<MTLBuffer> bb = [dev newBufferWithBytes:b.data() length:n * 8 options:MTLResourceStorageModeShared];
        id<MTLBuffer> bf = [dev newBufferWithBytes:f.data() length:n * 4 options:MTLResourceStorageModeShared];
        id<MTLBuffer> bo = [dev newBufferWithLength:n * 32 options:MTLResourceStorageModeShared];
        id<MTLCommandQueue> q = [dev newCommandQueue];
        id<MTLCommandBuffer> cb = [q commandBuffer];
        id<MTLComputeCommandEncoder> ce = [cb computeCommandEncoder];
        [ce setComputePipelineState:ps];
        [ce setBuffer:ba offset:0 atIndex:0];
        [ce setBuffer:bb offset:0 atIndex:1];
        [ce setBuffer:bf offset:0 atIndex:2];
        [ce setBuffer:bo offset:0 atIndex:3];
        [ce dispatchThreads:MTLSizeMake(n, 1, 1) threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
        [ce endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
        const uint64_t* out = static_cast<const uint64_t*>(bo.contents);

        std::fesetround(FE_TOWARDZERO);
        size_t bad[4] = {0, 0, 0, 0};
        for (size_t i = 0; i < n; ++i) {
            const volatile double x = dbl(a[i]), y = dbl(b[i]);
            const uint64_t mul = bits(x * y), add = bits(x + y);
            const uint64_t wide = bits(static_cast<double>(f[i]));
            const uint64_t want[4] = {mul, add, wide, d2fz(a[i])};
            for (int k = 0; k < 4; ++k) {
                if (out[4 * i + k] != want[k]) {
                    if (bad[k]++ < 4)
                        std::printf("op %d: %016llx %016llx -> %016llx, want %016llx\n", k, (unsigned long long)a[i],
                                    (unsigned long long)b[i], (unsigned long long)out[4 * i + k], (unsigned long long)want[k]);
                }
            }
        }
        std::fesetround(FE_TONEAREST);
        std::printf("%zu cases: dmulz %zu, daddz %zu, f2d %zu, d2fz %zu different\n", n, bad[0], bad[1], bad[2], bad[3]);
        (void)fbits;
        return (bad[0] + bad[1] + bad[2] + bad[3]) == 0 ? 0 : 1;
    }
}
