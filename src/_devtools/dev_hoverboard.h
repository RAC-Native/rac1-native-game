// _devtools/dev_hoverboard.h -- the debug menu's free hoverboard (openly invented, not a port): Hoverboard anywhere on the
// levels without the race's board (all but Rilgar and Kalebo III). See dev_hoverboard.cpp.
#ifndef RAC1_DEVTOOLS_DEV_HOVERBOARD_H
#define RAC1_DEVTOOLS_DEV_HOVERBOARD_H

namespace rac1::devtools {

// On the board where Ratchet stands: the hero in the board state 0x6b (nothing per frame on these levels), the board's
// animations hooked into his class, Kalebo III's board drawn on his feet. False when he cannot (lost, dying, another
// movement mode, the state refused).
bool dev_free_board_start();
// Off the board where he is, on foot (the game's standing state).
void dev_free_board_stop();
bool dev_free_board_on();
// One game frame's riding (between game frames, after the game's frame): ends by itself when the game takes the hero out
// of the board state or the level changes; in the water, below the level, off its map or after a long fall he gets off at
// the last solid ground.
void dev_free_board_frame();

}  // namespace rac1::devtools

#endif
