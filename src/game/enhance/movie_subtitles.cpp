// Movie subtitles of our own (see movie_subtitles.h). Not a port: everything here is host code; it reads the game's font
// from EE RAM and writes nothing there.
#include <game/enhance/movie_subtitles.h>

#include <game/enhance/advanced.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/disc.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>

namespace rac1 {

namespace {
constexpr uint32_t LANGUAGE = 0x0015ed88u;      // the game's (text) language
constexpr uint32_t SCREEN_H = 0x0013e504u;      // the screen's height (448 NTSC)
constexpr OvConst<0x001c3150u> SMALL_FONT{};    // the small font's glyphs: u, v, y offset, advance (4 bytes a code)
constexpr OvConst<0x0016d180u> EFFECT_TEX{};    // the effect textures' table (0x10 bytes each; effect_tex0, hud.cpp)
constexpr OvConst<0x0015f420u> EFFECT_DATA{};   // their texels' and palettes' base
constexpr size_t kMaxFile = 1u << 20;           // (a subtitle file larger than this is not read)

bool g_choice = false;      // the row's choice (the preferences)
int g_env = -1;             // RAC1_MOVIE_SUBTITLES: -1 unset, 0 off, 1 on
bool g_env_read = false;

// The font, as read at a movie's open (kept for a movie whose open finds none: the last good one)
struct Font {
    bool ok = false;
    uint8_t glyph[256][4] = {};   // u, v, y offset (s8), advance (s8)
    int tw = 0, th = 0;
    std::vector<uint32_t> rgba;   // the texture, CT32 (alpha 0..0x80)
};
Font g_font;

// The open movie's cues (the text already in the game's characters)
struct Cue {
    double start = 0.0, end = 0.0;
    std::vector<uint8_t> text;
};
std::vector<Cue> g_cues;
std::vector<uint8_t> g_picture;  // the picture with the cue drawn
int g_last_cue = -2;             // (the cue drawn into g_picture, for the log)

// orig: (none) — enhancement: the environment's override, read once
int env_choice() {
    if (!g_env_read) {
        g_env_read = true;
        if (const char* e = std::getenv("RAC1_MOVIE_SUBTITLES")) {
            if (std::strcmp(e, "on") == 0 || std::strcmp(e, "1") == 0) g_env = 1;
            else if (std::strcmp(e, "off") == 0 || std::strcmp(e, "0") == 0) g_env = 0;
            else std::fprintf(stderr, "[movie subtitles] RAC1_MOVIE_SUBTITLES=%s: on or off\n", e);
        }
    }
    return g_env;
}

// orig: (none) — enhancement: the game's language code as the files' suffix
const char* language_suffix(uint32_t lang) {
    switch (lang) {
        case 0: return "en";
        case 1: return "ja";
        case 2: return "fr";
        case 3: return "de";
        case 4: return "es";
        case 5: return "it";
        default: return nullptr;
    }
}

// orig: (none) — enhancement: the folder of the files (RAC1_MOVIE_SUBTITLES_DIR, testing)
std::string folder() {
    if (const char* d = std::getenv("RAC1_MOVIE_SUBTITLES_DIR")) return d;
    return "assets/movie-subtitles";
}

// orig: (none) — enhancement: the movie file at sector `lsn` of the game's disc, by its layout (assets/disc/layout.txt:
// "first byte, size, offset in the file, sha1, path"): "row_XXXX", or "" when none
std::string movie_name(uint32_t lsn) {
    static std::map<uint32_t, std::string> names;
    static bool read = false;
    if (!read) {
        read = true;
        std::ifstream f(std::string(disc_files_dir()) + "/layout.txt");
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ls(line);
            std::string first, size, offset, sha, path;
            if (!(ls >> first >> size >> offset >> sha >> path) || offset != "0") continue;
            if (path.size() < 4 || path.compare(path.size() - 4, 4, ".pss") != 0) continue;
            const unsigned long long at = std::strtoull(first.c_str(), nullptr, 16);
            const size_t slash = path.find_last_of('/');
            names[static_cast<uint32_t>(at / 2048)] = path.substr(slash == std::string::npos ? 0 : slash + 1, path.size() - 4 - (slash == std::string::npos ? 0 : slash + 1));
        }
    }
    const auto it = names.find(lsn);
    return it == names.end() ? std::string() : it->second;
}

// orig: (none) — enhancement: the small font read from EE RAM (its glyph table and effect texture 1: PSMT8 texels, a
// CT32 palette in the GS's CSM1 order); false when what is there does not look like it (not loaded)
bool font_read(Font& f) {
    const uint32_t t = SMALL_FONT;
    const uint32_t e = EFFECT_TEX + 0x10;
    const int32_t wl = lh(e + 0xc), hl = lh(e + 0xe);
    const uint32_t base = lwu(EFFECT_DATA);
    if (wl < 6 || wl > 10 || hl < 6 || hl > 10 || base == 0 || base >= 0x02000000u) return false;
    const int tw = 1 << wl, th = 1 << hl;
    const uint32_t texels = base + lhu(e + 8) * 0x10u, pal = base + lhu(e + 0xa) * 0x10u;
    if (texels + static_cast<uint32_t>(tw * th) > 0x02000000u || pal + 0x400u > 0x02000000u) return false;
    const int a_adv = static_cast<int8_t>(lbu(t + 'A' * 4 + 3)), sp_adv = static_cast<int8_t>(lbu(t + ' ' * 4 + 3));
    if (a_adv < 3 || a_adv > 24 || sp_adv < 1 || sp_adv > 24) return false;
    for (int c = 0; c < 256; ++c)
        for (int k = 0; k < 4; ++k) f.glyph[c][k] = static_cast<uint8_t>(lbu(t + static_cast<uint32_t>(c) * 4 + static_cast<uint32_t>(k)));
    uint32_t clut[256];
    for (uint32_t i = 0; i < 256; ++i) {
        const uint32_t at = (i & 0xe7u) | ((i & 0x08u) << 1) | ((i & 0x10u) >> 1);  // (CSM1: 8..15 and 16..23 swapped in each 32)
        clut[i] = lwu(pal + at * 4);
    }
    f.tw = tw;
    f.th = th;
    f.rgba.resize(static_cast<size_t>(tw) * th);
    for (int i = 0; i < tw * th; ++i) f.rgba[static_cast<size_t>(i)] = clut[lbu(texels + static_cast<uint32_t>(i))];
    f.ok = true;
    return true;
}

// orig: (none) — enhancement: the advance of code `c` (0: the font has no glyph)
int advance(uint8_t c) { return static_cast<int8_t>(g_font.glyph[c][3]); }

// A cue's lines laid out as text_box (0x1fbf48) lays out the game's subtitles: wrapped at spaces and control codes
// within `width` pixels, balanced (a narrower width tried while the last line is shorter than a third, back to the first
// when that adds lines); [start, end] of each line.
struct Lines {
    std::vector<std::pair<int, int>> spans;
};
// orig: (none) — enhancement: as 0x001fbf48's layout (its measure pass) on a host string
Lines layout(const std::vector<uint8_t>& s, int width0) {
    const int n = static_cast<int>(s.size());
    int width = width0, retry = 0, first_lines = 0;
    Lines best;
    auto at = [&s, n](int i) -> uint8_t { return i < n ? s[static_cast<size_t>(i)] : 0; };
    for (;;) {
        Lines L;
        int last_w = 0;
        int i = 0;
        if (n != 0 && at(0) != 0) {
            for (;;) {
                const int start = i;
                int brk = i, w = 0;
                if (0 < width) {
                    int p = i;
                    do {
                        const uint8_t ch = at(p);
                        if (ch == 0x20 || ch < 0x10) brk = i;
                        if (at(p) < 2) break;
                        ++i;
                        const int a = advance(at(p));
                        if (a != 0) w += a;
                        ++p;
                    } while (w < width);
                }
                int end = brk;
                if (brk == start) end = i;
                const int e = end;
                const uint8_t ec = at(e);
                if (ec == 0x20 || ec < 0x10) end -= 1;
                L.spans.emplace_back(start, end);
                if (at(e) == 0) {
                    last_w = w;
                    break;
                }
                i = e + 1;
                if (i >= n || at(i) == 0) break;
                if (L.spans.size() > 64) break;  // (a guard: the game's frame holds 32)
            }
        }
        best = L;
        const int lines = static_cast<int>(L.spans.size());
        if (retry != 0) break;
        if (first_lines == 0) first_lines = lines;
        if (lines < 2) break;
        if (first_lines < lines) {
            width = width0;
            retry = 1;
            continue;
        }
        const int third = width / 3;
        width -= 0x10;
        if (!(last_w < third)) break;
    }
    return best;
}

// The cue drawn over the picture: an "overlay" at the game's screen (512 x height) saying, per pixel, what the GS's
// blending makes of the colour under it: out = under * k + add (the box, then the glyphs, each (Cs - Cd) As + Cd)
struct Overlay {
    int w = 512, h = 448;
    std::vector<float> k, add;  // (add: r, g, b)
    void reset(int ww, int hh) {
        w = ww;
        h = hh;
        k.assign(static_cast<size_t>(w) * h, 1.0f);
        add.assign(static_cast<size_t>(w) * h * 3, 0.0f);
    }
    // orig: (none) — enhancement: a pixel of colour (r, g, b) at alpha `a` (0..1) blended in
    void blend(int x, int y, float r, float g, float b, float a) {
        if (x < 0 || y < 0 || x >= w || y >= h || a <= 0.0f) return;
        const size_t i = static_cast<size_t>(y) * w + x;
        k[i] *= 1.0f - a;
        add[i * 3] = add[i * 3] * (1.0f - a) + r * a;
        add[i * 3 + 1] = add[i * 3 + 1] * (1.0f - a) + g * a;
        add[i * 3 + 2] = add[i * 3 + 2] * (1.0f - a) + b * a;
    }
    // orig: (none) — enhancement: a flat quad (quad_fill 0x1fa0e0): x0..x1, y0..y1 (pixel centres within)
    void quad(int y0, int y1, int x0, int x1, float r, float g, float b, float a) {
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) blend(x, y, r, g, b, a);
    }
    // orig: (none) — enhancement: the rounded box (box_rounded 0x1fad58), colour 0x040404, alpha `a` (0x80 = 1)
    void box(int y0, int y1, int x0, int x1, uint32_t a) {
        const float c = 4.0f, al = static_cast<float>(a) / 128.0f;
        quad(y0, y1, x0, x1, c, c, c, al);
        quad(y0 + 1, y1 - 1, x0 - 2, x0, c, c, c, al);
        quad(y0 + 2, y1 - 2, x0 - 3, x0 - 2, c, c, c, al);
        quad(y0 + 4, y1 - 4, x0 - 4, x0 - 3, c, c, c, al);
        quad(y0 + 1, y1 - 1, x1, x1 + 2, c, c, c, al);
        quad(y0 + 2, y1 - 2, x1 + 2, x1 + 3, c, c, c, al);
        quad(y0 + 4, y1 - 4, x1 + 3, x1 + 4, c, c, c, al);
    }
    // orig: (none) — enhancement: a 16 x 16 glyph sprite (glyph_sprite 0x1fa290) from texels (u, v), modulated by the
    // grey `grey` (0x80 = 1, the colour 0x80b0b0b0's), point sampled
    void glyph(int x, int y, int u, int v, int grey) {
        const Font& f = g_font;
        for (int j = 0; j < 16; ++j) {
            for (int i = 0; i < 16; ++i) {
                const int tu = u + i, tv = v + j;
                if (tu >= f.tw || tv >= f.th) continue;
                const uint32_t t = f.rgba[static_cast<size_t>(tv) * f.tw + tu];
                const float a = std::min(static_cast<float>(t >> 24), 128.0f) / 128.0f;
                auto ch = [grey](uint32_t c) { return std::min(static_cast<float>(c * static_cast<uint32_t>(grey) >> 7), 255.0f); };
                blend(x + i, y + j, ch(t & 0xff), ch((t >> 8) & 0xff), ch((t >> 16) & 0xff), a);
            }
        }
    }
    // orig: (none) — enhancement: a line of text (text_draw 0x1fb0f0) at (x, y): the accented letters' marks, the glyphs
    void text(int x, int y, const std::vector<uint8_t>& s, int from, int to, int grey) {
        for (int k = from; k <= to && k < static_cast<int>(s.size()); ++k) {
            const uint8_t c = s[static_cast<size_t>(k)];
            if (c < 0x10 || advance(c) == 0) continue;
            if (static_cast<uint8_t>(c + 0x80) < 0x28) {
                const uint8_t* g = g_font.glyph[static_cast<uint8_t>(c + 0x40)];
                glyph(x + static_cast<int8_t>(g[3]), y + static_cast<int8_t>(g[2]), g[0], g[1], grey);
            }
            const uint8_t* g = g_font.glyph[c];
            if (c > 0x20) glyph(x, y + static_cast<int8_t>(g[2]), g[0], g[1], grey);
            x += advance(c);
        }
    }
};
Overlay g_overlay;

// orig: (none) — enhancement: the cue's text drawn as cutscene_subtitle_draw (0x1f9a20) draws a scene's: the box
// (left 0x28, right 0x1d8, x 0x100, line 0x12, centred both ways) at y = height - 0x3c, moved up when its lower half would
// pass height - 0x14; the rounded box (alpha 0x60) behind, the lines over it in 0x80b0b0b0
void cue_draw(const std::vector<uint8_t>& s, int screen_h) {
    const Lines L = layout(s, 0x1b0);
    const int lines = static_cast<int>(L.spans.size());
    if (lines == 0) return;
    int wmax = 0;
    for (const auto& sp : L.spans) {
        int w = 0;
        for (int k = sp.first; k <= sp.second && k < static_cast<int>(s.size()); ++k) w += std::max(advance(s[static_cast<size_t>(k)]), 0);
        wmax = std::max(wmax, w);
    }
    const int lh = 0x12;
    const int hh = (lines * lh) >> 1, dy = hh + 5, hw = wmax >> 1;
    int cy = screen_h - 0x3c;
    if (screen_h - 0x14 < cy + dy) cy = screen_h - (hh + 0x19);
    g_overlay.box(cy - dy, cy + dy, 0x100 - (hw + 0xa), hw + 0x10a, 0x60);
    int y = cy - ((lines * lh) >> 1);
    for (const auto& sp : L.spans) {
        int w = 0;
        for (int k = sp.first; k <= sp.second && k < static_cast<int>(s.size()); ++k) w += std::max(advance(s[static_cast<size_t>(k)]), 0);
        g_overlay.text(0x100 - (w >> 1), y, s, sp.first, sp.second, 0xb0);
        y += lh;
    }
}

// orig: (none) — enhancement: "HH:MM:SS,mmm" (or '.') to seconds; false when it is not one
bool srt_time(const std::string& s, double& t) {
    int h = 0, m = 0, sec = 0, ms = 0;
    char sep = 0;
    int used = 0;
    if (std::sscanf(s.c_str(), "%d:%d:%d%c%d%n", &h, &m, &sec, &sep, &ms, &used) != 5 || (sep != ',' && sep != '.')) return false;
    if (h < 0 || m < 0 || m > 59 || sec < 0 || sec > 59 || ms < 0 || ms > 999) return false;
    for (size_t i = static_cast<size_t>(used); i < s.size(); ++i)
        if (s[i] != ' ' && s[i] != '\t' && s[i] != '\r') return false;
    t = h * 3600.0 + m * 60.0 + sec + ms / 1000.0;
    return true;
}

// orig: (none) — enhancement: a line without its '\r' and outer blanks
std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t a = 0;
    while (a < s.size() && (s[a] == ' ' || s[a] == '\t')) ++a;
    return s.substr(a);
}
}  // namespace

// orig: (none) — enhancement (movie_subtitles.h)
std::vector<MovieCue> movie_subtitles_parse(const std::string& srt, const std::string& name, std::vector<std::string>& log) {
    std::vector<MovieCue> out;
    std::vector<std::string> lines;
    {
        std::string text = srt;
        if (text.size() >= 3 && static_cast<uint8_t>(text[0]) == 0xef && static_cast<uint8_t>(text[1]) == 0xbb && static_cast<uint8_t>(text[2]) == 0xbf) text.erase(0, 3);
        std::istringstream in(text);
        std::string l;
        while (std::getline(in, l)) lines.push_back(trim(l));
    }
    size_t i = 0, block = 0;
    while (i < lines.size()) {
        while (i < lines.size() && lines[i].empty()) ++i;
        if (i >= lines.size()) break;
        ++block;
        const size_t first = i;
        std::vector<std::string> b;
        while (i < lines.size() && !lines[i].empty()) b.push_back(lines[i++]);
        size_t k = 0;
        if (b[0].find("-->") == std::string::npos) k = 1;  // (the cue's number, or anything else before the timing)
        char why[160];
        if (k >= b.size() || b[k].find("-->") == std::string::npos) {
            std::snprintf(why, sizeof why, "%s: cue %zu (line %zu) skipped: no timing line (HH:MM:SS,mmm --> HH:MM:SS,mmm)", name.c_str(), block, first + 1);
            log.emplace_back(why);
            continue;
        }
        const std::string& tl = b[k];
        const size_t arrow = tl.find("-->");
        std::string a = trim(tl.substr(0, arrow)), e = trim(tl.substr(arrow + 3));
        const size_t sp = e.find(' ');  // (positions after the end time, as some editors write them: ignored)
        if (sp != std::string::npos) e = e.substr(0, sp);
        MovieCue c;
        if (!srt_time(a, c.start) || !srt_time(e, c.end) || !(c.end > c.start)) {
            std::snprintf(why, sizeof why, "%s: cue %zu (line %zu) skipped: bad timing \"%.60s\"", name.c_str(), block, first + k + 1, tl.c_str());
            log.emplace_back(why);
            continue;
        }
        for (size_t j = k + 1; j < b.size(); ++j) {
            if (!c.text.empty()) c.text += '\n';
            c.text += b[j];
        }
        if (c.text.empty()) continue;  // (a cue without text: nothing to show)
        out.push_back(std::move(c));
    }
    std::stable_sort(out.begin(), out.end(), [](const MovieCue& x, const MovieCue& y) { return x.start < y.start; });
    return out;
}

// orig: (none) — enhancement (movie_subtitles.h)
std::vector<uint8_t> movie_subtitles_to_game(const std::string& utf8, int& dropped) {
    // The small font's accented letters: codes 0x80..0xa7, four per letter (A a E e I i O o U u: the letter's glyph, the
    // I's without its dot), the accent at code + 0x40 drawn over it: acute, grave, circumflex, diaeresis in that order (as
    // read from the font in EE RAM). The other letters with a mark go as their plain letter.
    static const std::map<uint32_t, uint8_t> accents = {
        {0xc1, 0x80}, {0xc0, 0x81}, {0xc2, 0x82}, {0xc4, 0x83}, {0xe1, 0x84}, {0xe0, 0x85}, {0xe2, 0x86}, {0xe4, 0x87},
        {0xc9, 0x88}, {0xc8, 0x89}, {0xca, 0x8a}, {0xcb, 0x8b}, {0xe9, 0x8c}, {0xe8, 0x8d}, {0xea, 0x8e}, {0xeb, 0x8f},
        {0xcd, 0x90}, {0xcc, 0x91}, {0xce, 0x92}, {0xcf, 0x93}, {0xed, 0x94}, {0xec, 0x95}, {0xee, 0x96}, {0xef, 0x97},
        {0xd3, 0x98}, {0xd2, 0x99}, {0xd4, 0x9a}, {0xd6, 0x9b}, {0xf3, 0x9c}, {0xf2, 0x9d}, {0xf4, 0x9e}, {0xf6, 0x9f},
        {0xda, 0xa0}, {0xd9, 0xa1}, {0xdb, 0xa2}, {0xdc, 0xa3}, {0xfa, 0xa4}, {0xf9, 0xa5}, {0xfb, 0xa6}, {0xfc, 0xa7},
    };
    static const std::map<uint32_t, const char*> plain = {
        {0xc3, "A"}, {0xc5, "A"}, {0xe3, "a"}, {0xe5, "a"}, {0xc7, "C"}, {0xe7, "c"}, {0xd1, "N"}, {0xf1, "n"}, {0xd5, "O"},
        {0xf5, "o"}, {0xd8, "O"}, {0xf8, "o"}, {0xdd, "Y"}, {0xfd, "y"}, {0xff, "y"}, {0xdf, "ss"}, {0xc6, "AE"}, {0xe6, "ae"},
        {0x152, "OE"}, {0x153, "oe"}, {0xa1, "!"}, {0xbf, "?"}, {0xb4, "'"}, {0x60, "'"},
    };
    // ASCII the font draws as a pad button (#, $, %, @, ^) or has no glyph for (&, *, =, [, \, ], `, {, |, }, ~): left out
    auto drawable = [](uint32_t c) { return c != '#' && c != '$' && c != '%' && c != '@' && c != '^' && (!g_font.ok || advance(static_cast<uint8_t>(c)) != 0); };
    std::vector<uint8_t> out;
    size_t i = 0;
    while (i < utf8.size()) {
        const uint8_t b = static_cast<uint8_t>(utf8[i]);
        uint32_t cp = 0;
        size_t len = 1;
        if (b < 0x80) cp = b;
        else if ((b & 0xe0) == 0xc0) cp = b & 0x1fu, len = 2;
        else if ((b & 0xf0) == 0xe0) cp = b & 0x0fu, len = 3;
        else if ((b & 0xf8) == 0xf0) cp = b & 0x07u, len = 4;
        else len = 0;
        bool ok = len != 0 && i + len <= utf8.size();
        for (size_t k = 1; ok && k < len; ++k) {
            const uint8_t cb = static_cast<uint8_t>(utf8[i + k]);
            if ((cb & 0xc0) != 0x80) ok = false;
            cp = (cp << 6) | (cb & 0x3fu);
        }
        if (!ok) {  // (not UTF-8: the byte taken as Latin-1, as an editor saving in the Windows code page writes it)
            cp = b;
            len = 1;
        }
        i += len;
        if (cp == '\n') {
            out.push_back(1);
            continue;
        }
        if (cp == '\t' || cp == 0xa0) cp = ' ';
        if (cp == 0x2019 || cp == 0x2018) cp = '\'';
        if (cp == 0x201c || cp == 0x201d || cp == 0xab || cp == 0xbb) cp = '"';
        if (cp == 0x2013 || cp == 0x2014) cp = '-';
        if (cp == 0x2026) {
            out.insert(out.end(), {'.', '.', '.'});
            continue;
        }
        if (const auto p = plain.find(cp); p != plain.end()) {
            for (const char* q = p->second; *q != 0; ++q) out.push_back(static_cast<uint8_t>(*q));
            continue;
        }
        if (cp >= 0x20 && cp < 0x7f) {
            if (!drawable(cp)) {
                ++dropped;
                continue;
            }
            out.push_back(static_cast<uint8_t>(cp));
            continue;
        }
        if (const auto it = accents.find(cp); it != accents.end()) {
            out.push_back(it->second);
            continue;
        }
        ++dropped;
    }
    // the blanks a dropped character leaves: one space between words, none at a line's ends
    std::vector<uint8_t> tidy;
    for (const uint8_t c : out) {
        if (c == ' ' && (tidy.empty() || tidy.back() == ' ' || tidy.back() == 1)) continue;
        if (c == 1 && !tidy.empty() && tidy.back() == ' ') tidy.pop_back();
        tidy.push_back(c);
    }
    while (!tidy.empty() && (tidy.back() == ' ' || tidy.back() == 1)) tidy.pop_back();
    return tidy;
}

// orig: (none) — enhancement (movie_subtitles.h)
bool movie_subtitles_choice() { return g_choice; }
// orig: (none) — enhancement (movie_subtitles.h)
void movie_subtitles_set(bool on) { g_choice = on; }

// orig: (none) — enhancement (movie_subtitles.h)
bool movie_subtitles_on() {
    const int e = env_choice();
    if (e >= 0) return e == 1;
    return !g_advanced_disabled && g_choice;
}

// orig: (none) — enhancement (movie_subtitles.h)
void movie_subtitles_open(uint32_t lsn) {
    g_cues.clear();
    g_last_cue = -2;
    if (!movie_subtitles_on()) return;
    Font f;
    if (font_read(f)) g_font = std::move(f);
    else if (!g_font.ok) std::printf("[movie subtitles] the game's font is not loaded yet: none shown in this movie\n");
    const std::string name = movie_name(lsn);
    const uint32_t lang = lwu(LANGUAGE);
    const char* suffix = language_suffix(lang);
    if (name.empty() || suffix == nullptr) {
        std::printf("[movie subtitles] movie at sector %u: %s\n", lsn, name.empty() ? "not a movie file of the disc, no subtitles" : "no subtitles for this language");
        return;
    }
    const std::string path = folder() + "/" + name + "." + suffix + ".srt";
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::printf("[movie subtitles] %s: none (no file)\n", path.c_str());
        return;
    }
    std::string text;
    {
        std::ostringstream ss;
        ss << in.rdbuf();
        text = ss.str();
    }
    if (text.size() > kMaxFile) {
        std::printf("[movie subtitles] %s: %zu bytes, more than a subtitle file holds: not read\n", path.c_str(), text.size());
        return;
    }
    std::vector<std::string> log;
    const std::vector<MovieCue> cues = movie_subtitles_parse(text, name + "." + suffix + ".srt", log);
    for (const std::string& l : log) std::printf("[movie subtitles] %s\n", l.c_str());
    int dropped = 0;
    for (const MovieCue& c : cues) {
        Cue k;
        k.start = c.start;
        k.end = c.end;
        k.text = movie_subtitles_to_game(c.text, dropped);
        if (!k.text.empty()) g_cues.push_back(std::move(k));
    }
    std::printf("[movie subtitles] %s: %zu lines%s", path.c_str(), g_cues.size(), log.empty() ? "" : " (some skipped)");
    if (dropped > 0) std::printf(", %d characters the font does not have left out", dropped);
    std::printf("\n");
}

// orig: (none) — enhancement (movie_subtitles.h)
void movie_subtitles_close() {
    g_cues.clear();
    g_picture.clear();
    g_picture.shrink_to_fit();
    g_last_cue = -2;
}

// orig: (none) — enhancement (movie_subtitles.h)
const std::vector<uint8_t>& movie_subtitles_picture(const std::vector<uint8_t>& rgba, int w, int h, double seconds) {
    if (g_cues.empty() || !g_font.ok || w <= 0 || h <= 0 || rgba.size() < static_cast<size_t>(w) * h * 4) return rgba;
    int cue = -1;
    for (size_t i = 0; i < g_cues.size(); ++i)
        if (seconds >= g_cues[i].start && seconds < g_cues[i].end) cue = static_cast<int>(i);  // (overlapping: the later one)
    if (cue != g_last_cue) {
        g_last_cue = cue;
        if (cue >= 0) {
            int sh = lw(SCREEN_H);
            if (sh < 256 || sh > 1024) sh = 448;
            g_overlay.reset(512, sh);
            cue_draw(g_cues[static_cast<size_t>(cue)].text, sh);
        }
    }
    if (cue < 0) return rgba;
    // the overlay (the game's 512 x height screen) scaled onto the picture, bilinear, as the frame is scaled onto the window
    g_picture = rgba;
    const Overlay& o = g_overlay;
    const float sx = static_cast<float>(o.w) / static_cast<float>(w), sy = static_cast<float>(o.h) / static_cast<float>(h);
    for (int y = 0; y < h; ++y) {
        const float gy = (static_cast<float>(y) + 0.5f) * sy - 0.5f;
        const int y0 = std::clamp(static_cast<int>(gy < 0 ? -1 : gy), 0, o.h - 1), y1 = std::min(y0 + 1, o.h - 1);
        const float fy = std::clamp(gy - static_cast<float>(y0), 0.0f, 1.0f);
        if (y0 < 0x60 && y1 < 0x60) continue;  // (nothing above the subtitle box's area)
        for (int x = 0; x < w; ++x) {
            const float gx = (static_cast<float>(x) + 0.5f) * sx - 0.5f;
            const int x0 = std::clamp(static_cast<int>(gx < 0 ? -1 : gx), 0, o.w - 1), x1 = std::min(x0 + 1, o.w - 1);
            const float fx = std::clamp(gx - static_cast<float>(x0), 0.0f, 1.0f);
            const size_t i00 = static_cast<size_t>(y0) * o.w + x0, i01 = static_cast<size_t>(y0) * o.w + x1;
            const size_t i10 = static_cast<size_t>(y1) * o.w + x0, i11 = static_cast<size_t>(y1) * o.w + x1;
            const float w00 = (1 - fx) * (1 - fy), w01 = fx * (1 - fy), w10 = (1 - fx) * fy, w11 = fx * fy;
            const float k = o.k[i00] * w00 + o.k[i01] * w01 + o.k[i10] * w10 + o.k[i11] * w11;
            if (k >= 0.999f) continue;
            uint8_t* p = &g_picture[(static_cast<size_t>(y) * w + x) * 4];
            for (int c = 0; c < 3; ++c) {
                const float add = o.add[i00 * 3 + c] * w00 + o.add[i01 * 3 + c] * w01 + o.add[i10 * 3 + c] * w10 + o.add[i11 * 3 + c] * w11;
                p[c] = static_cast<uint8_t>(std::clamp(static_cast<float>(p[c]) * k + add + 0.5f, 0.0f, 255.0f));
            }
        }
    }
    return g_picture;
}

}  // namespace rac1
