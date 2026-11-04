// (Here, not in src/game: the generated code is fingerprinted by src/game's headers; the code is
// src/game/enhance/custom_arena_hud.cpp.)
// Maktar's arena HUD, as RAC2's arena controller (class 2905) draws its own (local experiment, branch
// feat/custom-levels-experiment): RAC2's layout and look, said with RAC1's HUD packets and fonts. Read as the reference:
// RAC2's level 2 code, the fight's panels 0x3fe8b8, the victory 0x3ff6e8, the menu 0x3fe250 (their places and colours are
// the numbers below); its panels are the vector shapes of RAC2's pause-menu 2D set (HUD.WAD), and its texts the level's,
// both read from the player's RAC2 disc into the planet's hud.txt by tools/custom_level/maktar.py (never committed):
//   shape <id> <scale> <vertices: x y ...> ; <quads: four vertex indices each>
//   text <key> <text>  (\xNN a control byte: 0x10 Cross, 0x12 Triangle ... as RAC1's font has them; 0x01 a line break)
// The shapes are drawn as RAC2 draws them (0x387d88: each quad a strip of four, one flat colour with its alpha), on the
// 512 x 448 HUD screen; the texts in RAC1's fonts (RAC2's font is not ours to draw yet). Without hud.txt the panels are
// left out and the texts are English of our own.
// In a HUD wider than 4:3 the panels and their texts go to the picture's sides as RAC1's side elements do.
#ifndef RAC1_HOST_CUSTOM_ARENA_HUD_H
#define RAC1_HOST_CUSTOM_ARENA_HUD_H

#include <cstdint>
#include <string>
#include <vector>

namespace rac1::arena_hud {

// orig: (none) — a text of hud.txt by key (control bytes as bytes), `fallback` when the file has none
std::string text(const char* key, const char* fallback);

// The fight's panels (RAC2 0x3fe8b8): a time challenge, the kills at the bottom left ("enemies killed") and the time at
// the bottom right ("time left"); the others, the round at the bottom right ("round", "n/m").
struct Fight {
    int wave = 0, waves = 0;  // (wave 0-based)
    int kills = 0;
    int seconds_left = -1;    // (-1: no time limit)
};
// orig: (none)
void fight(const Fight& f);

// orig: (none) — the prompt at the arena's centre (RAC2's text 0x2f3c: Triangle, "Enter the Battle Arena")
void prompt();

// The challenge menu (RAC2 0x3fe250): the challenge's name in its box, the prize and the victories under it, the buttons
struct Menu {
    std::string name;
    int index = 0, count = 0;
    int victories = 0;
    int32_t prize = 0;
    std::string blocked;  // why it cannot be played now (empty: it can)
};
// orig: (none)
void menu(const Menu& m);

// orig: (none) — a challenge starting: its name as a title
void intro(const std::string& name);
// orig: (none) — the victory (RAC2 0x3ff6e8): "Congratulations!", "You have won <prize> bolts", the choice to go or go on
void won(int32_t prize, bool first_electrolyzer);
// orig: (none) — the defeat: "Challenge Failed!" and why (a hud.txt key: time_out, been_hit, you_lose; or our words)
void lost(const std::string& why_key, const std::string& why_words);

}  // namespace rac1::arena_hud

#endif
