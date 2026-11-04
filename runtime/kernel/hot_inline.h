// RAC1_HOT_INLINE: a small helper the ported and generated code calls in its inner loops (the memory accessors, the VU0 macro
// ops, the FPU rules), forced inline when the build asks for it (CMake's RAC1_INLINE_HELPERS, on by default) and left to the
// compiler (plain `inline`, the code as before) when it does not. RAC1_COLD_PATH: the rare path such a helper keeps out of line
// (a null address, a flagged op): out of line and cold with the switch on, nothing special with it off.
#ifndef RAC1_RUNTIME_KERNEL_HOT_INLINE_H
#define RAC1_RUNTIME_KERNEL_HOT_INLINE_H

#if defined(RAC1_INLINE_HELPERS) && RAC1_INLINE_HELPERS
#define RAC1_HOT_INLINE [[gnu::always_inline]] inline
#define RAC1_COLD_PATH [[gnu::noinline, gnu::cold]]
#else
#define RAC1_HOT_INLINE inline
#define RAC1_COLD_PATH
#endif

#endif
