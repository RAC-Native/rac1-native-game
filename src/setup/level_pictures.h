// A level mod's pictures (docs/mods.md, "Level mods": level.txt's picture, banner, map_picture): made by the level composer
// (level_compose.cpp) into its build as images in the layout the game's own pictures have (the loading screen's planet and
// name, the ship's map's planet picture: tex_image_upload, src/game/ovl/level_exit.cpp), which the game shows instead of
// the base level's on the level's slot (src/game/enhance/level_slots.cpp):
//   +0 "2FIP", +4 0, +8 width, +0xc height, +0x10 0x13 (8-bit indices), +0x14 0 (a 32-bit palette), +0x18 0, +0x1c 1 (one
//   level); +0x20 the palette (256 RGBA words in the GS's CSM1 order, alpha 0..0x80), +0x420 the indices, a row after another.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace rac1::setup {

// The game's sizes: the loading screen's planet picture and name banner, the ship's map's picture (of which the map shows
// the top-left kMapShownW x kMapShownH)
constexpr int kPictureW = 128, kPictureH = 128;
constexpr int kBannerW = 256, kBannerH = 32;
constexpr int kMapW = 512, kMapH = 128, kMapShownW = 306, kMapShownH = 108;

// RGBA pixels, a row after another
struct Rgba {
    int w = 0, h = 0;
    std::vector<uint8_t> p;
};

// orig: (host) a PNG (or any picture stb_image reads) as RGBA
bool picture_read(const std::filesystem::path& file, Rgba& out, std::string& err);

// orig: (host) `in` scaled to w x h (each pixel the average of the source's under it, alpha weighted)
Rgba picture_scaled(const Rgba& in, int w, int h);

// orig: (host) `in` (w x h) as the game's 8-bit image (256 colours at most: made fewer by a median cut when it has more)
std::vector<uint8_t> picture_image(const Rgba& in);

// orig: (host) the name banner made from `text` (UTF-8, Latin-1's letters): the game's look, blue letters centred in
// kBannerW x kBannerH on nothing; false when a letter has no glyph (then '?' stands for it, `err` says which)
Rgba banner_from_text(const std::string& text, std::string& err);

// orig: (host) the ship's map's picture made from the planet picture: the planet centred on a night sky of stars in the
// part shown, kMapW x kMapH
Rgba map_from_planet(const Rgba& planet);

// orig: (host) `in` fitted into the part of the ship's map's picture that is shown (its middle, the rest black)
Rgba map_from_picture(const Rgba& in);

}  // namespace rac1::setup
