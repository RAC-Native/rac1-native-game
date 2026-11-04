// See custom_arena_hud.h: Maktar's arena HUD in RAC2's layout and look, drawn into RAC1's HUD packets.
#include <host/custom_arena_hud.h>

#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/render/mrp_lod.h>
#include <game/render/render_view.h>
#include <runtime/kernel/disc.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>

namespace rac1::arena_hud {

namespace {

constexpr OvConst<0x001611c0u> CUR{};  // the frame packet's cursor (the HUD's packets go there)
constexpr OvConst<0x0017e4d8u> H{};    // the HUD's state: +0xc the depth its sprites are drawn at

// RAC2's numbers (its level 2 code and .lit, gp 0x1aeff0): the panels' colours (+0x2434 the fill, +0x2430 the edge), the
// texts' (+0x2368 the panels', +0x244c the victory's), the screen's scale of a shape unit (its view's 4:3 projection:
// 256 / 0.62 across and 208 / 0.4805 down, in 1/16 pixels; up the screen)
constexpr uint32_t kFill = 0x60442d00u, kEdge = 0x55f0c070u, kPanelText = 0x80f0f0f0u, kBigText = 0x90ffffffu;
constexpr uint32_t kRed = 0x806070f0u, kGreen = 0x8060f060u, kGold = 0x8060d0f0u;
constexpr float kUnitX = 256.0f / 0.62f / 16.0f, kUnitY = 208.0f / 0.4805f / 16.0f;
// the shapes (RAC2's 2D set ids): the time / kills panel (fill, edge), the round panel (fill, edge)
constexpr int kTimeFill = 0x4c, kTimeEdge = 0x4d, kRoundFill = 0x4e, kRoundEdge = 0x4f;
// where RAC2 puts them: the bottom corners, the right one mirrored (+0x2418 / +0x241c, +0x2420 / +0x2424, +0x2428)
constexpr float kLeftX = 10.0f, kRightX = 502.0f, kPanelY = 407.0f;
// RAC2's text boxes (0x2ed770's {top, bottom, left, right, anchor x, anchor y}): the number and its label in each panel
struct Box {
    int x0, x1, ax, ay;
};
constexpr Box kLeftNumber{25, 70, 40, 380}, kLeftLabel{70, 200, 130, 384};
constexpr Box kRightNumber{430, 500, 474, 380}, kRightLabel{330, 430, 380, 384};
constexpr Box kRoundNumber{420, 500, 457, 380}, kRoundLabel{320, 420, 360, 384};

struct Shape {
    float scale = 0.0f;
    std::vector<std::pair<int, int>> v;
    std::vector<std::array<int, 4>> quads;
};
struct Data {
    uint32_t gen = 0xffffffffu;
    std::map<int, Shape> shapes;
    std::map<std::string, std::string> texts;
} g_data;

// orig: (none) — hud.txt (read again when the planet's generation changes)
const Data& data() {
    if (g_data.gen == custom_level_generation()) return g_data;
    g_data = Data{};
    g_data.gen = custom_level_generation();
    std::ifstream f(custom_level_dir() + "/hud.txt");
    std::string line;
    while (f && std::getline(f, line)) {
        std::istringstream in(line);
        std::string key;
        if (!(in >> key)) continue;
        if (key == "shape") {
            std::string id;
            Shape s;
            if (!(in >> id >> s.scale)) continue;
            std::string tok;
            std::vector<int> nums;
            bool quads = false;
            while (in >> tok) {
                if (tok == ";") {
                    for (size_t k = 0; k + 1 < nums.size(); k += 2) s.v.emplace_back(nums[k], nums[k + 1]);
                    nums.clear();
                    quads = true;
                    continue;
                }
                nums.push_back(std::atoi(tok.c_str()));
            }
            if (!quads) continue;
            for (size_t k = 0; k + 3 < nums.size(); k += 4) s.quads.push_back({nums[k], nums[k + 1], nums[k + 2], nums[k + 3]});
            g_data.shapes[static_cast<int>(std::strtol(id.c_str(), nullptr, 0))] = s;
        } else if (key == "text") {
            std::string name, raw;
            in >> name;
            std::getline(in >> std::ws, raw);
            std::string t;
            for (size_t k = 0; k < raw.size(); ++k) {
                if (raw[k] == '\\' && k + 3 < raw.size() && raw[k + 1] == 'x') {
                    t.push_back(static_cast<char>(std::strtol(raw.substr(k + 2, 2).c_str(), nullptr, 16)));
                    k += 3;
                } else {
                    t.push_back(raw[k]);
                }
            }
            while (!t.empty() && t.back() == ' ') t.pop_back();
            g_data.texts[name] = t;
        }
    }
    return g_data;
}

// orig: (none) — the HUD's side bookkeeping for a HUD wider than 4:3 (as hud_draw does for its side elements)
bool wide() { return render_view_hud_wide() && mrp_lod_shadow_slot() < 0; }

// orig: (none) — a GS XYZ2 at (x, y) on the HUD screen (1/16 pixels from the screen's origin 0x13e510 / 0x13e514), the HUD's depth
uint64_t xyz(float x, float y) {
    const int32_t gx = static_cast<int32_t>(std::lround(x * 16.0f)) + lw(0x0013e510) - 8;
    const int32_t gy = static_cast<int32_t>(std::lround(y * 16.0f)) + lw(0x0013e514) - 8;
    return static_cast<uint64_t>(static_cast<uint32_t>(gx) & 0xffffu) | (static_cast<uint64_t>(static_cast<uint32_t>(gy) & 0xffffu) << 16) |
           (static_cast<uint64_t>(static_cast<uint32_t>(lw(H + 0xc))) << 32);
}

// orig: (none) — quads (each four corners, drawn as the strip 0, 1, 3, 2) in one colour: one GIF tag (REGLIST: PRIM, then
// RGBAQ and XYZ2 four times, a loop a quad) behind a DMA CNT / VIF DIRECT, as RAC1's HUD sprites are sent
void quads(const std::vector<std::array<std::pair<float, float>, 4>>& qs, uint32_t rgba) {
    if (qs.empty()) return;
    const uint32_t nloop = static_cast<uint32_t>(qs.size());
    const uint32_t dwords = nloop * 9, qwc = 1 + (dwords + 1) / 2;
    const uint32_t p = lwu(CUR);
    sw(p, 0x10000000u | qwc);
    sw(p + 4, 0);
    sw(p + 8, 0);
    sw(p + 0xc, 0x50000000u | qwc);
    const uint32_t g = p + 0x10;
    // the tag: NLOOP quads, EOP, REGLIST (FLG 1), 9 registers: PRIM, (RGBAQ, XYZ2) x 4
    sd(g, static_cast<uint64_t>(nloop) | (1ull << 15) | (1ull << 58) | (9ull << 60));
    sd(g + 8, 0x515151510ull);
    uint32_t d = g + 0x10;
    for (const auto& q : qs) {
        sd(d, 0x44);  // (a triangle strip, alpha blended)
        d += 8;
        for (int k : {0, 1, 3, 2}) {
            sd(d, static_cast<uint64_t>(rgba) | (0x3f800000ull << 32));
            sd(d + 8, xyz(q[static_cast<size_t>(k)].first, q[static_cast<size_t>(k)].second));
            d += 16;
        }
    }
    if ((dwords & 1) != 0) sd(d, 0);
    sw(CUR, p + 0x10 * (qwc + 1));
}

// orig: (none) — a flat rectangle (the menu's backing: RAC2 draws its menu on the vendor's frame, which we have not)
void rect(float x0, float y0, float x1, float y1, uint32_t rgba) {
    quads({{{{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}}}, rgba);
}

// orig: (none) — shape `id` at (x, y), mirrored across when `mirror`, one colour (RAC2's ABGR, alpha 0x80 = opaque): RAC2's
// 0x387d88 with the identity joint (each quad a triangle strip of its vertices 0, 1, 3, 2)
void shape(int id, float x, float y, bool mirror, uint32_t rgba) {
    const auto it = data().shapes.find(id);
    if (it == data().shapes.end() || it->second.quads.empty()) return;
    const Shape& s = it->second;
    const float sx = (mirror ? -1.0f : 1.0f) * s.scale * kUnitX, sy = s.scale * kUnitY;
    std::vector<std::array<std::pair<float, float>, 4>> qs;
    for (const auto& q : s.quads) {
        std::array<std::pair<float, float>, 4> c;
        for (size_t k = 0; k < 4; ++k) {
            const auto& v = s.v[static_cast<size_t>(std::clamp(q[k], 0, static_cast<int>(s.v.size()) - 1))];
            c[k] = {x + static_cast<float>(v.first) * sx, y - static_cast<float>(v.second) * sy};
        }
        qs.push_back(c);
    }
    quads(qs, rgba);
}

// orig: (none) — `t` (one line) in EE memory for the font's functions
struct EeText {
    uint32_t s = 0, n = 0;
    explicit EeText(const std::string& t) : n(static_cast<uint32_t>(t.size())) {
        s = ee_stack_alloc(n + 1);
        for (uint32_t k = 0; k < n; ++k) sb(s + k, static_cast<uint8_t>(t[k]));
        sb(s + n, 0);
    }
    ~EeText() { ee_stack_free(n + 1); }
};

enum Align { Left, Centre, Right };
// orig: (none) — a line of text at (x, y) in the small font (the labels) or the big one; its shadow under it
void line(const std::string& t, int32_t x, int32_t y, uint32_t rgba, bool big, Align a) {
    if (t.empty()) return;
    EeText e(t);
    const int32_t w = big ? text_width_big(e.s, -1) : text_width_small(e.s, -1);
    if (a == Centre) x -= w / 2;
    if (a == Right) x -= w;
    const uint64_t col = rgba, shadow = rgba & 0xff000000u;
    const uint64_t tex0 = effect_tex0(big ? 3 : 1);
    const uint32_t table = big ? OV(0x001c3890) : OV(0x001c3150);
    text_draw(x + 1, y + 1, shadow, e.s, -1, tex0, table);
    text_draw(x, y, col, e.s, -1, tex0, table);
}

// orig: (none) — text in a box of RAC2's: centred on its anchor, inside its left / right
void boxed(const std::string& t, const Box& b, bool big) {
    line(t, b.ax, b.ay - 8, kPanelText, big, Centre);
}

// orig: (none) — the lines of `t` (0x01 breaks it, as RAC2's texts do), centred at x from y, `step` apart
void lines(const std::string& t, int32_t x, int32_t y, int32_t step, uint32_t rgba, bool big) {
    size_t from = 0;
    while (from <= t.size()) {
        size_t to = t.find('\x01', from);
        if (to == std::string::npos) to = t.size();
        std::string l = t.substr(from, to - from);
        while (!l.empty() && l.front() == ' ') l.erase(l.begin());
        while (!l.empty() && l.back() == ' ') l.pop_back();
        line(l, x, y, rgba, big, Centre);
        y += step;
        from = to + 1;
    }
}

// orig: (none) — a panel at a corner with its texts, noted as a side element for a wide HUD
template <typename F>
void side(uint8_t s, F draw) {
    const bool w = wide();
    const uint32_t at = w ? lwu(CUR) : 0;
    draw();
    if (w) render_view_hud_note(at, lwu(CUR), s);
}

}  // namespace

// orig: (none)
std::string text(const char* key, const char* fallback) {
    const auto it = data().texts.find(key);
    return it != data().texts.end() ? it->second : std::string(fallback);
}

// orig: (none) — RAC2 0x3fe8b8: its time challenges (the states 3 and 5) draw both panels, the timed avoidance the time
// alone, the others the round panel (and a boss's health, not here: no bosses yet)
void fight(const Fight& f) {
    char buf[32];
    if (f.seconds_left >= 0) {
        side(1, [&] {
            shape(kTimeFill, kLeftX, kPanelY, false, kFill);
            shape(kTimeEdge, kLeftX, kPanelY, false, kEdge);
            std::snprintf(buf, sizeof buf, "%d", f.kills);
            boxed(buf, kLeftNumber, true);
            boxed(text("enemies_killed", "enemies killed"), kLeftLabel, false);
        });
        side(2, [&] {
            shape(kTimeFill, kRightX, kPanelY, true, kFill);
            shape(kTimeEdge, kRightX, kPanelY, true, kEdge);
            std::snprintf(buf, sizeof buf, "%d", f.seconds_left);
            line(buf, kRightNumber.ax, kRightNumber.ay - 8, f.seconds_left <= 5 ? kRed : kPanelText, true, Centre);
            boxed(text("time_left", "time left"), kRightLabel, false);
        });
        return;
    }
    side(2, [&] {
        shape(kRoundFill, kRightX, kPanelY, true, kFill);
        shape(kRoundEdge, kRightX, kPanelY, true, kEdge);
        std::snprintf(buf, sizeof buf, "%2d/%2d", f.wave + 1, f.waves);  // (RAC2's "%2d/%2d")
        boxed(buf, kRoundNumber, true);
        boxed(text("round", "round"), kRoundLabel, false);
    });
}

// orig: (none)
void prompt() {
    lines(text("enter", "\x12 Enter the Battle Arena"), 0x100, 0x150, 18, kBigText, true);
}

// orig: (none) — RAC2 0x3fe250's places: the name in its box (x 20..450 about 250, y 125..185), the prize at (90, 184),
// the victories at (275, 184), prev / next at (166 / 305, 208), select / exit at y 95; RAC2 draws them on its vendor's
// frame, ours on a dark box
void menu(const Menu& m) {
    rect(14, 74, 498, 250, kFill);
    rect(14, 74, 498, 76, kEdge);
    rect(14, 248, 498, 250, kEdge);
    char buf[96];
    line(text("select_challenge", "Select Your Challenge:"), 0x100, 82, kGold, true, Centre);
    std::snprintf(buf, sizeof buf, "%s   %d / %d", text("select", "select \x10").c_str(), m.index + 1, m.count);
    line(buf, 75, 104, kPanelText, false, Left);
    line(text("exit", "exit \x12"), 380, 104, kPanelText, false, Left);
    lines(m.name, 250, 137, 18, kBigText, true);
    if (m.blocked.empty()) {
        std::snprintf(buf, sizeof buf, "%s %d %s", text("prize", "Prize:").c_str(), m.prize, text("bolts", "bolts").c_str());
        line(buf, 90, 184, kGreen, false, Left);
    } else {
        line(m.blocked, 0x100, 228, kRed, false, Centre);
    }
    std::snprintf(buf, sizeof buf, "%s %d", text("victories", "Victories:").c_str(), m.victories);
    line(buf, 300, 184, kPanelText, false, Left);
    line("< " + text("prev", "prev"), 166, 208, kPanelText, false, Centre);
    line(text("next", "next") + " >", 305, 208, kPanelText, false, Centre);
}

// orig: (none)
void intro(const std::string& name) {
    lines(name, 0x100, 93, 20, kBigText, true);
}

// orig: (none) — RAC2 0x3ff6e8: "Congratulations!" (its big font, x 1.1, at y 93), the prize line (y ~135), the choice
// (y ~160); the Electrolyzer's line for the first victory of challenge 0
void won(int32_t prize, bool first_electrolyzer) {
    char buf[96];
    lines(text("congratulations", "Congratulations!"), 0x100, 93, 20, kBigText, true);
    int32_t y = 128;
    if (first_electrolyzer) {
        lines(text("electrolyzer", "You have earned an Electrolyzer"), 0x100, y, 18, kBigText, false);
        y += 18;
    }
    std::snprintf(buf, sizeof buf, "%s %d %s", text("you_have_won", "You have won").c_str(), prize, text("bolts", "bolts").c_str());
    line(buf, 0x100, y, kBigText, false, Centre);
    lines(text("exit_or_continue", "Exit the arena \x12 or continue \x01 fighting to earn bolts \x10"), 0x100, y + 28, 16, kPanelText, false);
}

// orig: (none)
void lost(const std::string& why_key, const std::string& why_words) {
    lines(text("failed", "Challenge Failed!"), 0x100, 93, 20, kBigText, true);
    const std::string why = why_key.empty() ? why_words : text(why_key.c_str(), why_words.c_str());
    lines(why, 0x100, 120, 20, kRed, true);
}

}  // namespace rac1::arena_hud
