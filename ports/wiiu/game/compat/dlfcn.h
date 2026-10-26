// The Wii U has no dynamic loader for host libraries: dlopen finds nothing (volk then finds no Vulkan, the audio no
// backend library), which every caller handles.
#pragma once
#define RTLD_LAZY 1
#define RTLD_NOW 2
#define RTLD_LOCAL 0
#define RTLD_GLOBAL 0x100
#ifdef __cplusplus
extern "C" {
#endif
static inline void* dlopen(const char*, int) { return 0; }
static inline void* dlsym(void*, const char*) { return 0; }
static inline int dlclose(void*) { return 0; }
static inline const char* dlerror(void) { return "no dynamic loading on the Wii U"; }
#ifdef __cplusplus
}
#endif
