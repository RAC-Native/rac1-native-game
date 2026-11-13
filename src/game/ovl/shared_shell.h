// The shells of Gaspar and Orxon (class 0x4ea / 1258) and the shots of Eudora and Hoven (class 0xb8 / 184): their launch and
// their flight (shared_shell.cpp: one port for each kind's two levels' copies). Phase 1 of stage 4: the levels' generated copies still run; these are proved against them by oracle
// batch 510.
#ifndef RAC1_GAME_OVL_SHARED_SHELL_H
#define RAC1_GAME_OVL_SHARED_SHELL_H

#include <cstdint>

namespace rac1 {

// a shell from `src` at `pos` with the velocity `vel` (EE vectors), its fuse `fuse` frames; the shell or 0 (L9:0x00307ba8, L10:0x002e68b8)
uint32_t shell_launch(uint32_t src, uint32_t pos, uint32_t vel, int32_t fuse);
void shell_update(uint32_t m);  // class 1258 (L9:0x00307d68, L10:0x002e6a78)
// a shot from `src` at `pos` with the velocity `vel` (EE vectors), its fuse `fuse` frames; the shot or 0 (L4:0x002aee30, L12:0x002d4150)
uint32_t shot_launch(uint32_t src, uint32_t pos, uint32_t vel, int32_t fuse);
void shot_update(uint32_t m);   // class 184 (L4:0x002af058, L12:0x002d4378)

}  // namespace rac1

#endif
