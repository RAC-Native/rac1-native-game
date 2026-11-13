#ifndef RAC1_GAME_OVL_DOOMLET_H
#define RAC1_GAME_OVL_DOOMLET_H
// The Glove of Doom's agents (doomlet.cpp), written by hand. Addresses are Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

int32_t doomlet_2bfec8(uint32_t p0);  // orig: 0x002bfec8
void doomlet_2beb90(uint32_t p0);  // orig: 0x002beb90
int32_t doomlet_25b440(uint32_t p0, uint32_t p1);  // orig: 0x0025b440
void doomlet_2bebc8(uint32_t p0);  // orig: 0x002bebc8
void doomlet_2bec00(uint32_t p0);  // orig: 0x002bec00
void doomlet_2bf8b0(uint32_t p0, uint32_t p1, uint32_t p2);  // orig: 0x002bf8b0
float doomlet_2bec80(uint32_t p0, uint32_t p1);  // orig: 0x002bec80
int32_t doomlet_2beab0(uint32_t p0, uint32_t p1, uint32_t p2);  // orig: 0x002beab0
void doomlet_2bec48(uint32_t p0);  // orig: 0x002bec48
void doomlet_2bf810(uint32_t p0, uint32_t p1, uint32_t p2);  // orig: 0x002bf810
int32_t doomlet_2bf9f0(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3);  // orig: 0x002bf9f0
int32_t doomlet_2bed90(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4);  // orig: 0x002bed90
float doomlet_2bf050(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, float pf12, float pf13, float pf14, float pf15, float pf16, float pf17);  // orig: 0x002bf050
int32_t doomlet_259028(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3);  // orig: 0x00259028
void doomlet_update(uint32_t m);  // orig: 0x002bffa8
void doomlet_fns_register();  // orig: (none)

}  // namespace rac1
#endif
