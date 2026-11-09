// See level_pictures.h. Not a port: our own tool. The image layout is the game's (tex_image_upload, level_exit.cpp); the
// sizes are the disc's pictures' (the loading screen's planets 128 x 128 and names 256 x 32, the ship's map's 512 x 128).
#include "level_pictures.h"

#include <formats/level_file.h>
#include <formats/level_textures.h>
#include <formats/ratchet_executable.h>

#include <imgui.h>
#include <stb_image.h>  // (its implementation: static_skinned_render.cpp)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <exception>
#include <fstream>
#include <iterator>
#include <span>

namespace rac1::setup {

namespace {

// the palette's order in GS memory (CSM1: entries 8..15 and 16..23 of each 32 swapped)
uint32_t csm1(uint32_t i) { return (i & ~0x18u) | ((i & 0x08u) << 1) | ((i & 0x10u) >> 1); }

void put32(std::vector<uint8_t>& b, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) b[at + static_cast<size_t>(i)] = static_cast<uint8_t>(v >> (8 * i));
}

// a median cut's box: the pixels (indices into the colours) it holds
struct Box {
    std::vector<uint32_t> px;
    int axis = 0;
    int range = 0;
};

void box_measure(Box& b, const std::vector<std::array<uint8_t, 4>>& c) {
    std::array<int, 4> lo{255, 255, 255, 255}, hi{0, 0, 0, 0};
    for (uint32_t i : b.px)
        for (int k = 0; k < 4; ++k) {
            lo[static_cast<size_t>(k)] = std::min<int>(lo[static_cast<size_t>(k)], c[i][static_cast<size_t>(k)]);
            hi[static_cast<size_t>(k)] = std::max<int>(hi[static_cast<size_t>(k)], c[i][static_cast<size_t>(k)]);
        }
    b.range = -1;
    for (int k = 0; k < 4; ++k)
        if (hi[static_cast<size_t>(k)] - lo[static_cast<size_t>(k)] > b.range) {
            b.range = hi[static_cast<size_t>(k)] - lo[static_cast<size_t>(k)];
            b.axis = k;
        }
}

}  // namespace

// orig: (host) see level_pictures.h
bool picture_read(const std::filesystem::path& file, Rgba& out, std::string& err) {
    int w = 0, h = 0, n = 0;
    stbi_uc* d = stbi_load(file.string().c_str(), &w, &h, &n, 4);
    if (d == nullptr) {
        err = file.generic_string() + " cannot be read (a PNG)";
        return false;
    }
    out.w = w;
    out.h = h;
    out.p.assign(d, d + static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    stbi_image_free(d);
    return true;
}

// orig: (host) see level_pictures.h
Rgba picture_scaled(const Rgba& in, int w, int h) {
    Rgba o;
    o.w = w;
    o.h = h;
    o.p.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 0);
    if (in.w <= 0 || in.h <= 0) return o;
    const double sx = static_cast<double>(in.w) / w, sy = static_cast<double>(in.h) / h;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            // the source's area under the pixel (at least one source pixel), alpha weighted
            const double x0 = x * sx, x1 = std::max(x0 + 1e-6, (x + 1) * sx), y0 = y * sy, y1 = std::max(y0 + 1e-6, (y + 1) * sy);
            double acc[4] = {0, 0, 0, 0}, area = 0;
            for (int v = static_cast<int>(y0); v < std::min(in.h, static_cast<int>(std::ceil(y1))); ++v)
                for (int u = static_cast<int>(x0); u < std::min(in.w, static_cast<int>(std::ceil(x1))); ++u) {
                    const double wx = std::min<double>(u + 1, x1) - std::max<double>(u, x0), wy = std::min<double>(v + 1, y1) - std::max<double>(v, y0);
                    const double a = std::max(0.0, wx) * std::max(0.0, wy);
                    if (a <= 0) continue;
                    const uint8_t* s = &in.p[(static_cast<size_t>(v) * static_cast<size_t>(in.w) + static_cast<size_t>(u)) * 4];
                    const double al = s[3] / 255.0;
                    for (int k = 0; k < 3; ++k) acc[k] += s[k] * al * a;
                    acc[3] += al * a;
                    area += a;
                }
            uint8_t* d = &o.p[(static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 4];
            if (area <= 0) continue;
            for (int k = 0; k < 3; ++k) d[k] = static_cast<uint8_t>(acc[3] > 0 ? std::clamp(acc[k] / acc[3] + 0.5, 0.0, 255.0) : 0);
            d[3] = static_cast<uint8_t>(std::clamp(acc[3] / area * 255.0 + 0.5, 0.0, 255.0));
        }
    return o;
}

// orig: (host) see level_pictures.h
std::vector<uint8_t> picture_image(const Rgba& in) {
    const size_t n = static_cast<size_t>(in.w) * static_cast<size_t>(in.h);
    // the colours as the GS has them (alpha 0..0x80; colour none where nothing shows)
    std::vector<std::array<uint8_t, 4>> c(n);
    for (size_t i = 0; i < n; ++i) {
        const uint8_t* s = &in.p[i * 4];
        const uint8_t a = static_cast<uint8_t>((s[3] * 128 + 127) / 255);
        c[i] = a == 0 ? std::array<uint8_t, 4>{0, 0, 0, 0} : std::array<uint8_t, 4>{s[0], s[1], s[2], a};
    }
    // the palette: the colours as they are when 256 or fewer, else a median cut into 256 boxes (their averages)
    std::vector<std::array<uint8_t, 4>> pal;
    std::vector<uint8_t> index(n, 0);
    {
        std::vector<std::array<uint8_t, 4>> uniq(c);
        std::sort(uniq.begin(), uniq.end());
        uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
        if (uniq.size() <= 256) {
            pal = uniq;
            for (size_t i = 0; i < n; ++i) index[i] = static_cast<uint8_t>(std::lower_bound(pal.begin(), pal.end(), c[i]) - pal.begin());
        } else {
            std::vector<Box> boxes(1);
            for (size_t i = 0; i < n; ++i) boxes[0].px.push_back(static_cast<uint32_t>(i));
            box_measure(boxes[0], c);
            while (boxes.size() < 256) {
                size_t best = boxes.size();
                for (size_t b = 0; b < boxes.size(); ++b)
                    if (boxes[b].px.size() > 1 && boxes[b].range > 0 && (best == boxes.size() || boxes[b].range * std::sqrt(static_cast<double>(boxes[b].px.size())) >
                                                                                                    boxes[best].range * std::sqrt(static_cast<double>(boxes[best].px.size()))))
                        best = b;
                if (best == boxes.size()) break;
                Box& b = boxes[best];
                const int ax = b.axis;
                std::sort(b.px.begin(), b.px.end(), [&](uint32_t l, uint32_t r) { return c[l][static_cast<size_t>(ax)] < c[r][static_cast<size_t>(ax)]; });
                Box nb;
                nb.px.assign(b.px.begin() + static_cast<std::ptrdiff_t>(b.px.size() / 2), b.px.end());
                b.px.resize(b.px.size() / 2);
                box_measure(b, c);
                box_measure(nb, c);
                boxes.push_back(std::move(nb));
            }
            for (size_t b = 0; b < boxes.size(); ++b) {
                uint64_t s[4] = {0, 0, 0, 0};
                for (uint32_t i : boxes[b].px)
                    for (int k = 0; k < 4; ++k) s[k] += c[i][static_cast<size_t>(k)];
                const uint64_t m = std::max<uint64_t>(1, boxes[b].px.size());
                pal.push_back({static_cast<uint8_t>(s[0] / m), static_cast<uint8_t>(s[1] / m), static_cast<uint8_t>(s[2] / m), static_cast<uint8_t>(s[3] / m)});
                for (uint32_t i : boxes[b].px) index[i] = static_cast<uint8_t>(b);
            }
        }
    }
    std::vector<uint8_t> out(0x420 + n, 0);
    std::memcpy(out.data(), "2FIP", 4);
    put32(out, 8, static_cast<uint32_t>(in.w));
    put32(out, 0xc, static_cast<uint32_t>(in.h));
    put32(out, 0x10, 0x13);
    put32(out, 0x1c, 1);
    for (size_t k = 0; k < pal.size(); ++k)
        put32(out, 0x20 + csm1(static_cast<uint32_t>(k)) * 4,
              static_cast<uint32_t>(pal[k][0]) | static_cast<uint32_t>(pal[k][1]) << 8 | static_cast<uint32_t>(pal[k][2]) << 16 | static_cast<uint32_t>(pal[k][3]) << 24);
    std::memcpy(out.data() + 0x420, index.data(), n);
    return out;
}

namespace {

// The game's own big font (the menus' titles: text_draw with the glyph table at Veldin's 0x1c3890 and the effect texture 3),
// read from the player's disc: Veldin's data file (its overlay's table, its effect textures). Each character `c` has 4
// bytes: its 16 x 16 cell's corner (u, v) in the texture, its drop (+2, signed) and its advance (+3; 0: no glyph).
struct GameFont {
    bool ok = false;
    int tw = 0, th = 0;
    std::vector<uint8_t> rgba;
    uint8_t table[256 * 4] = {};
};

const GameFont& game_font() {
    static GameFont f;
    static bool tried = false;
    if (tried) return f;
    tried = true;
    std::ifstream in("assets/disc/levels/level00/data.wad", std::ios::binary);
    std::vector<uint8_t> wad((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (wad.size() < sizeof(LevelFileHeader)) return f;
    LevelFileHeader h;
    std::memcpy(&h, wad.data(), sizeof h);
    if (h.overlay.offset < 0 || h.overlay.size <= 0 || static_cast<size_t>(h.overlay.offset) + static_cast<size_t>(h.overlay.size) > wad.size()) return f;
    constexpr uint32_t kTable = 0x001c3890u;
    bool table = false;
    try {
        const RatchetExecutable exe = parse_ratchet_executable(
            std::span<const std::byte>(reinterpret_cast<const std::byte*>(wad.data() + h.overlay.offset), static_cast<size_t>(h.overlay.size)));
        for (const RatchetSection& sec : exe.sections) {
            const uint32_t lo = static_cast<uint32_t>(sec.header.load_address);
            if (kTable >= lo && kTable + sizeof f.table <= lo + sec.data.size()) {
                std::memcpy(f.table, sec.data.data() + (kTable - lo), sizeof f.table);
                table = true;
            }
        }
    } catch (const std::exception&) {
        return f;
    }
    if (!table) return f;
    for (const DecodedTexture& t : decode_level_textures(std::span<const std::byte>(reinterpret_cast<const std::byte*>(wad.data()), wad.size())))
        if (t.category == "fx" && t.index == 3) {
            f.tw = t.width;
            f.th = t.height;
            f.rgba = t.rgba_data;
        }
    f.ok = !f.rgba.empty() && f.tw >= 16 && f.th >= 16;
    return f;
}

// the banner in the game's own font, in the game's blue: false when the font cannot be read, or a letter has no glyph
bool banner_game_font(const std::string& text, Rgba& o) {
    const GameFont& f = game_font();
    if (!f.ok) return false;
    std::vector<uint8_t> chars;
    for (unsigned char c : text) {
        if (c >= 0x80 || f.table[c * 4 + 3] == 0) return false;  // (Latin letters with accents: the other font)
        chars.push_back(c);
    }
    float width = 0;
    for (uint8_t c : chars) width += static_cast<int8_t>(f.table[c * 4 + 3]);
    // the cells' 16 pixels drawn at 12 (the original banners' letters' height); narrowed when too long
    float scale = 0.8f;
    if (width * scale > kBannerW - 8) scale = (kBannerW - 8) / width;
    std::vector<float> a(static_cast<size_t>(kBannerW) * kBannerH, 0.0f);
    float x = std::floor((kBannerW - width * scale) / 2.0f);
    const float top = 8.0f;
    for (uint8_t c : chars) {
        const int u = f.table[c * 4], v = f.table[c * 4 + 1];
        const float dy = static_cast<int8_t>(f.table[c * 4 + 2]) * scale;
        if (c != ' ') {
            // each banner pixel the cell's texels under it averaged (a box filter)
            const int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(top + dy));
            const int n = static_cast<int>(std::ceil(16.0f * scale)) + 1;
            for (int py = 0; py < n; ++py)
                for (int px = 0; px < n; ++px) {
                    const int bx = x0 + px, by = y0 + py;
                    if (bx < 0 || bx >= kBannerW || by < 0 || by >= kBannerH) continue;
                    float sum = 0;
                    int cnt = 0;
                    for (int sy = 0; sy < 4; ++sy)
                        for (int sx = 0; sx < 4; ++sx) {
                            const float gx = (bx - x + (sx + 0.5f) / 4.0f) / scale, gy = (by - top - dy + (sy + 0.5f) / 4.0f) / scale;
                            ++cnt;
                            if (gx < 0 || gy < 0 || gx >= 16 || gy >= 16) continue;
                            const int tx = u + static_cast<int>(gx), ty = v + static_cast<int>(gy);
                            if (tx >= f.tw || ty >= f.th) continue;
                            const uint8_t* t = &f.rgba[(static_cast<size_t>(ty) * static_cast<size_t>(f.tw) + static_cast<size_t>(tx)) * 4];
                            sum += (t[3] / 255.0f) * (std::max({t[0], t[1], t[2]}) / 255.0f);
                        }
                    float& d = a[static_cast<size_t>(by) * kBannerW + static_cast<size_t>(bx)];
                    d = std::min(1.0f, d + sum / static_cast<float>(cnt));
                }
        }
        x += static_cast<int8_t>(f.table[c * 4 + 3]) * scale;
    }
    float peak = 0;
    for (float v : a) peak = std::max(peak, v);
    if (peak <= 0) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] <= 0) continue;
        o.p[i * 4] = 29;
        o.p[i * 4 + 1] = 123;
        o.p[i * 4 + 2] = 211;
        o.p[i * 4 + 3] = static_cast<uint8_t>(std::min(1.0f, a[i] / peak) * 255.0f + 0.5f);
    }
    return true;
}

}  // namespace

// orig: (host) see level_pictures.h
Rgba banner_from_text(const std::string& text, std::string& err) {
    Rgba o;
    o.w = kBannerW;
    o.h = kBannerH;
    o.p.assign(static_cast<size_t>(kBannerW) * kBannerH * 4, 0);
    if (banner_game_font(text, o)) return o;
    std::fill(o.p.begin(), o.p.end(), 0);
    // the letters: Dear ImGui's own font (ProggyClean, 13 pixels: the size of the game's banners' letters), drawn a pixel
    // wider (the game's letters are bolder) in the game's blue
    ImFontAtlas atlas;
    atlas.Flags |= ImFontAtlasFlags_NoMouseCursors;
    ImFontConfig cfg;
    cfg.SizePixels = 13.0f;
    ImFont* font = atlas.AddFontDefault(&cfg);
    unsigned char* tex = nullptr;
    int tw = 0, th = 0;
    atlas.GetTexDataAsAlpha8(&tex, &tw, &th);
    if (font == nullptr || tex == nullptr) {
        err = "no font";
        return o;
    }
    std::vector<const ImFontGlyph*> glyphs;
    const char* s = text.c_str();
    const char* end = s + text.size();
    while (s < end) {
        // one UTF-8 character (a byte that is not one: as Latin-1)
        unsigned int ch = static_cast<unsigned char>(*s);
        int k = 1;
        if (ch >= 0xc0 && ch < 0xe0 && s + 1 < end) ch = (ch & 0x1f) << 6 | (static_cast<unsigned char>(s[1]) & 0x3f), k = 2;
        else if (ch >= 0xe0 && ch < 0xf0 && s + 2 < end) ch = (ch & 0x0f) << 12 | (static_cast<unsigned char>(s[1]) & 0x3f) << 6 | (static_cast<unsigned char>(s[2]) & 0x3f), k = 3;
        else if (ch >= 0xf0 && s + 3 < end) ch = 0xfffd, k = 4;
        s += k;
        const ImFontGlyph* g = font->FindGlyphNoFallback(static_cast<ImWchar>(ch));
        if (g == nullptr) {
            err = "a letter of \"" + text + "\" has no glyph: '?' instead";
            g = font->FindGlyph('?');
        }
        glyphs.push_back(g);
    }
    float width = 0;
    for (const ImFontGlyph* g : glyphs) width += g->AdvanceX + 1.0f;
    // as wide as the banner at most: narrowed (the letters' advance) when longer
    const float squeeze = width > kBannerW - 8 ? (kBannerW - 8) / width : 1.0f;
    float x = std::floor((kBannerW - width * squeeze) / 2.0f);
    const int top = 9;  // (the game's banners: the letters' tops at row 9, their baseline at 21)
    std::vector<float> cover(static_cast<size_t>(kBannerW) * kBannerH, 0.0f);
    for (const ImFontGlyph* g : glyphs) {
        // the atlas's texels of the glyph (oversampled across: ox texels a pixel), each added to the pixel under it
        const float u0 = g->U0 * tw, u1 = g->U1 * tw, v0 = g->V0 * th, v1 = g->V1 * th;
        const int gw = static_cast<int>(u1 - u0 + 0.5f), gh = static_cast<int>(v1 - v0 + 0.5f);
        const float ox = g->X1 > g->X0 ? (u1 - u0) / (g->X1 - g->X0) : 1.0f, oy = g->Y1 > g->Y0 ? (v1 - v0) / (g->Y1 - g->Y0) : 1.0f;
        for (int gy = 0; gy < gh; ++gy)
            for (int gx = 0; gx < gw; ++gx) {
                const uint8_t a = tex[static_cast<size_t>(static_cast<int>(v0 + 0.5f) + gy) * static_cast<size_t>(tw) + static_cast<size_t>(static_cast<int>(u0 + 0.5f) + gx)];
                if (a == 0) continue;
                const int px = static_cast<int>(std::floor(x + g->X0 + gx / ox)), py = top - 2 + static_cast<int>(std::floor(g->Y0 + gy / oy));
                for (int bold = 0; bold < 2; ++bold) {
                    if (px + bold < 0 || px + bold >= kBannerW || py < 0 || py >= kBannerH) continue;
                    float& c = cover[static_cast<size_t>(py) * kBannerW + static_cast<size_t>(px + bold)];
                    c = std::min(1.0f, c + a / 255.0f / (ox * oy));
                }
            }
        x += (g->AdvanceX + 1.0f) * squeeze;
    }
    for (size_t i = 0; i < cover.size(); ++i) {
        if (cover[i] <= 0) continue;
        o.p[i * 4] = 29;
        o.p[i * 4 + 1] = 123;
        o.p[i * 4 + 2] = 211;
        o.p[i * 4 + 3] = static_cast<uint8_t>(cover[i] * 255.0f + 0.5f);
    }
    return o;
}

// orig: (host) see level_pictures.h
Rgba map_from_planet(const Rgba& planet) {
    Rgba o;
    o.w = kMapW;
    o.h = kMapH;
    o.p.assign(static_cast<size_t>(kMapW) * kMapH * 4, 0);
    // the night: dark blue, its stars from a fixed sequence (the same picture each time)
    uint32_t r = 0x2545f491u;
    for (int y = 0; y < kMapShownH; ++y)
        for (int x = 0; x < kMapShownW; ++x) {
            uint8_t* d = &o.p[(static_cast<size_t>(y) * kMapW + static_cast<size_t>(x)) * 4];
            d[0] = 4;
            d[1] = 6;
            d[2] = static_cast<uint8_t>(18 + 14 * y / kMapShownH);
            d[3] = 255;
            r ^= r << 13;
            r ^= r >> 17;
            r ^= r << 5;
            if (r % 97 == 0) {
                const uint8_t v = static_cast<uint8_t>(120 + r % 136);
                d[0] = d[1] = d[2] = v;
            }
        }
    // the planet in the middle, as high as the part shown leaves room for
    const int size = kMapShownH - 8;
    const Rgba p = picture_scaled(planet, size, size);
    const int ox = (kMapShownW - size) / 2, oy = (kMapShownH - size) / 2;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            const uint8_t* s = &p.p[(static_cast<size_t>(y) * static_cast<size_t>(size) + static_cast<size_t>(x)) * 4];
            uint8_t* d = &o.p[(static_cast<size_t>(oy + y) * kMapW + static_cast<size_t>(ox + x)) * 4];
            for (int k = 0; k < 3; ++k) d[k] = static_cast<uint8_t>((s[k] * s[3] + d[k] * (255 - s[3]) + 127) / 255);
        }
    return o;
}

// orig: (host) see level_pictures.h
Rgba map_from_picture(const Rgba& in) {
    Rgba o;
    o.w = kMapW;
    o.h = kMapH;
    o.p.assign(static_cast<size_t>(kMapW) * kMapH * 4, 0);
    if (in.w <= 0 || in.h <= 0) return o;
    // scaled to cover the part shown, its middle kept
    const double k = std::max(static_cast<double>(kMapShownW) / in.w, static_cast<double>(kMapShownH) / in.h);
    const int sw = std::max(kMapShownW, static_cast<int>(std::lround(in.w * k))), sh = std::max(kMapShownH, static_cast<int>(std::lround(in.h * k)));
    const Rgba s = picture_scaled(in, sw, sh);
    const int ox = (sw - kMapShownW) / 2, oy = (sh - kMapShownH) / 2;
    for (int y = 0; y < kMapShownH; ++y)
        for (int x = 0; x < kMapShownW; ++x) {
            const uint8_t* a = &s.p[(static_cast<size_t>(y + oy) * static_cast<size_t>(sw) + static_cast<size_t>(x + ox)) * 4];
            uint8_t* d = &o.p[(static_cast<size_t>(y) * kMapW + static_cast<size_t>(x)) * 4];
            for (int c = 0; c < 3; ++c) d[c] = static_cast<uint8_t>(a[c] * a[3] / 255);
            d[3] = 255;
        }
    return o;
}

}  // namespace rac1::setup
