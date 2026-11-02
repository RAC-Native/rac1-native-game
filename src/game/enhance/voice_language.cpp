// The voice language (see voice_language.h). Not a port: the voice sites of the ported code ask here which sector to
// play; the game's own tables are read, never written.
#include <game/enhance/voice_language.h>

#include <game/enhance/advanced.h>
#include <game/ovl/ovl_common.h>

#include <runtime/kernel/disc.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

namespace rac1 {

namespace {
constexpr uint32_t TOC = 0x00137b80u;          // the table of contents' copy (6 sectors from sector 1500)
constexpr uint32_t TOC_BYTES = 6 * 2048;
constexpr uint32_t LEVEL_HEADER = 0x0013a4e0u;  // the level's header (0x2434 bytes: +0 the level, +8 its data's sectors)
constexpr uint32_t LANGUAGE = 0x0015ed88u;      // the game's language: the text's

int32_t g_choice = -1;  // the setting

constexpr const char* NAMES[6] = {"en", "ja", "fr", "de", "es", "it"};

// orig: (none) — RAC1_VOICE_LANGUAGE: -2 unset or not a name
int32_t env_choice() {
    static const int32_t v = [] {
        const char* e = std::getenv("RAC1_VOICE_LANGUAGE");
        if (e == nullptr || *e == 0) return -2;
        const int32_t c = voice_choice_from_name(e);
        if (c == -2) std::fprintf(stderr, "RAC1_VOICE_LANGUAGE=%s: same, en, ja, fr, de, es or it\n", e);
        return c;
    }();
    return v;
}

// ---- The Japanese disc (a data disc): its table of contents and levels' headers, read once ----
struct Japanese {
    bool tried = false;
    const DataDisc* disc = nullptr;
    std::vector<uint8_t> toc;
    std::map<int32_t, std::vector<uint8_t>> headers;  // by level
};
Japanese g_jp;

// orig: (none) — the Japanese data disc, its table of contents read; null when there is none
Japanese* japanese() {
    if (!g_jp.tried) {
        g_jp.tried = true;
        const DataDisc* d = disc_data_disc("NTSC-J");
        if (d != nullptr) {
            g_jp.toc.resize(TOC_BYTES);
            int32_t magic = 0, size = 0;
            if (disc_read(d->base + 1500, 6, g_jp.toc.data())) {
                std::memcpy(&magic, g_jp.toc.data(), 4);
                std::memcpy(&size, g_jp.toc.data() + 4, 4);
            }
            if (magic == 1 && size > 8 && static_cast<uint32_t>(size) <= TOC_BYTES) {
                g_jp.disc = d;
                std::printf("[voice] the Japanese voices: the data disc %s\n", d->serial.c_str());
            } else {
                std::fprintf(stderr, "[voice] the Japanese data disc's table of contents cannot be read\n");
            }
        }
    }
    return g_jp.disc != nullptr ? &g_jp : nullptr;
}

uint32_t word(const std::vector<uint8_t>& b, uint32_t at) {
    uint32_t v = 0;
    if (at + 4 <= b.size()) std::memcpy(&v, b.data() + at, 4);
    return v;
}

// orig: (none) — the Japanese disc's level header of `level` (0x2434 bytes), or null
const std::vector<uint8_t>* japanese_header(Japanese& jp, int32_t level) {
    if (level < 0 || level > 18) return nullptr;
    auto it = jp.headers.find(level);
    if (it == jp.headers.end()) {
        std::vector<uint8_t> h(5 * 2048);
        const uint32_t lsn = word(jp.toc, 0x28c8 + static_cast<uint32_t>(level) * 8);
        int32_t id = -1, size = 0;
        if (lsn != 0 && disc_read(jp.disc->base + lsn, 5, h.data())) {
            std::memcpy(&id, h.data(), 4);
            std::memcpy(&size, h.data() + 4, 4);
        }
        if (id != level || size != 0x2434) h.clear();
        it = jp.headers.emplace(level, std::move(h)).first;
    }
    return it->second.empty() ? nullptr : &it->second;
}

// orig: (none) — a sector of the Japanese disc (`lsn`, 0: none) as the disc layer's sector, when a VAG ("VAGp") is there;
// else 0
uint32_t japanese_vag(const Japanese& jp, uint32_t lsn) {
    if (lsn == 0 || lsn >= kDataDiscWindow) return 0;
    uint8_t s[2048];
    if (!disc_read(jp.disc->base + lsn, 1, s) || std::memcmp(s, "VAGp", 4) != 0) return 0;
    return jp.disc->base + lsn;
}

// orig: (none) — the voice language differs from the text's: true and `v` the voice's code
bool voice_other(uint32_t& v) {
    v = voice_language();
    return v != lwu(LANGUAGE);
}

// orig: (none) — the voice's line `lsn` in place of the text's `text_lsn`, said once per sector (the log: which voice
// played). The voice lacks it (0): the text's. The text lacks it: none either (a line the game's own disc does not have,
// as the Japanese disc's level 15 line 7, is not added)
uint32_t said(const char* what, int32_t index, uint32_t text_lsn, uint32_t lsn) {
    if (lsn == 0 || text_lsn == 0) return text_lsn;
    static std::map<uint64_t, bool> once;
    if (once.emplace((uint64_t{lsn} << 8) ^ static_cast<uint64_t>(index), true).second)
        std::printf("[voice] %s %d: %s, sector %#x (the text's %#x)\n", what, index, NAMES[voice_language() % 6], lsn, text_lsn);
    return lsn;
}
}  // namespace

// orig: (none)
int32_t voice_choice() {
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        advanced_prefs_load();  // (the setting, from the preferences: before any page or gameplay frame, the intro's movie)
    }
    const int32_t e = env_choice();
    return e != -2 ? e : g_choice;
}

// orig: (none)
void voice_choice_set(int32_t choice) { g_choice = choice >= -1 && choice < 6 ? choice : -1; }

// orig: (none)
const char* voice_choice_name(int32_t choice) { return choice >= 0 && choice < 6 ? NAMES[choice] : "same"; }

// orig: (none)
int32_t voice_choice_from_name(const std::string& name) {
    if (name == "same") return -1;
    for (int32_t i = 0; i < 6; ++i)
        if (name == NAMES[i]) return i;
    return -2;
}

// orig: (none)
bool voice_japanese_available() { return japanese() != nullptr; }

// orig: (none)
std::vector<int32_t> voice_choices() {
    std::vector<int32_t> c = {-1, 0, 2, 3, 4, 5};
    if (voice_japanese_available()) c.push_back(1);
    return c;
}

// orig: (none)
uint32_t voice_language() {
    const uint32_t text = lwu(LANGUAGE);
    const int32_t c = voice_choice();
    if (c < 0 || c > 5) return text;
    if (c == 1 && !voice_japanese_available()) return text;
    return static_cast<uint32_t>(c);
}

// orig: (none)
uint32_t voice_level_line(int32_t id, uint32_t text_lsn) {
    uint32_t v = 0;
    if (id < 0 || !voice_other(v)) return text_lsn;
    if (v != 1) return said("level line", id, text_lsn, lwu(static_cast<uint32_t>(id) * 0x250u + v * 4 + 0x0013a664u));
    Japanese* jp = japanese();
    const std::vector<uint8_t>* h = jp != nullptr ? japanese_header(*jp, lw(LEVEL_HEADER)) : nullptr;
    if (h == nullptr) return text_lsn;
    const uint32_t at = 0x184u + static_cast<uint32_t>(id) * 0x250u;
    return said("level line", id, text_lsn, at + 4 <= 0x2434u ? japanese_vag(*jp, word(*h, at)) : 0);
}

// orig: (none)
uint32_t voice_vendor_line(int32_t index, uint32_t text_lsn) {
    uint32_t v = 0;
    if (index < 0 || index >= 36 || !voice_other(v)) return text_lsn;
    const int32_t line = index / 6, slot = index % 6;
    const int32_t text_slot = std::max(lw(LANGUAGE) - 1, 0);  // (the Vendor's choice of the slot, vendor_tr)
    if (slot != text_slot) return text_lsn;
    if (v != 1) {
        const uint32_t vs = static_cast<uint32_t>(std::max(static_cast<int32_t>(v) - 1, 0));
        return said("Vendor line", index, text_lsn, lwu(TOC + 0x1a0u + (static_cast<uint32_t>(line) * 6 + vs) * 8));
    }
    Japanese* jp = japanese();
    if (jp == nullptr) return text_lsn;
    return said("Vendor line", index, text_lsn, japanese_vag(*jp, word(jp->toc, 0x1a0u + static_cast<uint32_t>(line) * 6 * 8)));
}

// orig: (none)
uint32_t voice_line_50000(int32_t index, uint32_t text_lsn) {
    uint32_t v = 0;
    if (index < 0 || !voice_other(v)) return text_lsn;
    const uint32_t at = 0xf00u + static_cast<uint32_t>(index) * 0x18u;
    if (at + 0x18 > TOC_BYTES) return text_lsn;
    if (v != 1) return said("line 50000 +", index, text_lsn, lwu(TOC + at + v * 4));
    Japanese* jp = japanese();
    return jp != nullptr ? said("line 50000 +", index, text_lsn, japanese_vag(*jp, word(jp->toc, at))) : text_lsn;
}

// orig: (none)
uint32_t voice_line_30000(int32_t index, uint32_t text_lsn) {
    uint32_t v = 0;
    if (index < 0 || index >= 0x258 / 4 || !voice_other(v)) return text_lsn;
    const uint32_t at = 0x1ab8u + static_cast<uint32_t>(index) * 4;
    if (v != 1) return said("line 30000 +", index, text_lsn, lwu(TOC + at + v * 0x258u));
    Japanese* jp = japanese();
    return jp != nullptr ? said("line 30000 +", index, text_lsn, japanese_vag(*jp, word(jp->toc, at))) : text_lsn;
}

// orig: (none)
void voice_movie(uint32_t lsn, uint32_t bytes, int32_t& track, uint32_t& audio_lsn, uint32_t& audio_bytes) {
    audio_lsn = 0;
    audio_bytes = 0;
    uint32_t v = 0;
    if (!voice_other(v)) return;
    if (v != 1) {
        track = static_cast<int32_t>(v);
        std::printf("[voice] movie at sector %#x: track %u\n", lsn, v);
        return;
    }
    Japanese* jp = japanese();
    if (jp == nullptr) return;
    // the movie's row in the table of contents (sector, bytes), and the Japanese disc's movie on the same row
    const uint32_t size = std::min<uint32_t>(lwu(TOC + 4), TOC_BYTES);
    for (uint32_t o = 8; o + 8 <= size; o += 8) {
        if (lwu(TOC + o) != lsn || lwu(TOC + o + 4) != bytes) continue;
        const uint32_t jl = word(jp->toc, o), jb = word(jp->toc, o + 4);
        uint8_t s[2048];
        if (jl == 0 || jb < 2048 || jl >= kDataDiscWindow || !disc_read(jp->disc->base + jl, 1, s) ||
            std::memcmp(s, "\x00\x00\x01\xba", 4) != 0) {
            std::printf("[voice] movie at sector %#x (row %#x): none on the Japanese disc, the text language's sound\n", lsn, o);
            return;
        }
        audio_lsn = jp->disc->base + jl;
        audio_bytes = jb;
        track = 0;  // (the Japanese disc's movies have one track, Japanese)
        std::printf("[voice] movie at sector %#x (row %#x): the Japanese disc's sound\n", lsn, o);
        return;
    }
}

// orig: (none)
void voice_level_bank(std::vector<uint8_t>& bank) {
    uint32_t v = 0;
    if (bank.size() < 16 || !voice_other(v) || v != 1) return;
    Japanese* jp = japanese();
    if (jp == nullptr) return;
    // the level's bank as the game's disc has it (its data's +8 / +0xc: offset, size): only that bank is replaced
    const int32_t level = lw(LEVEL_HEADER);
    const uint32_t data_lsn = lwu(LEVEL_HEADER + 8);
    auto bank_of = [](uint32_t lsn, std::vector<uint8_t>& out) {
        uint8_t head[2048];
        if (lsn == 0 || !disc_read(lsn, 1, head)) return false;
        uint32_t off = 0, size = 0;
        std::memcpy(&off, head + 8, 4);
        std::memcpy(&size, head + 12, 4);
        if (size == 0 || size > (16u << 20)) return false;
        const uint32_t first = off / 2048, n = (off % 2048 + size + 2047) / 2048;
        std::vector<uint8_t> b(static_cast<size_t>(n) * 2048);
        if (!disc_read(lsn + first, n, b.data())) return false;
        out.assign(b.begin() + off % 2048, b.begin() + off % 2048 + size);
        return true;
    };
    std::vector<uint8_t> base;
    if (!bank_of(data_lsn, base) || base.size() < bank.size() || !std::equal(bank.begin(), bank.end(), base.begin())) return;
    const std::vector<uint8_t>* h = japanese_header(*jp, level);
    std::vector<uint8_t> jb;
    if (h == nullptr || !bank_of(jp->disc->base + word(*h, 8), jb) || jb.size() < 8) return;
    // its length as the IOP's loader takes it: the chunks' extent (type, count, then offset / size per chunk)
    const uint32_t chunks = word(jb, 4);
    uint32_t end = 8 + chunks * 8;
    for (uint32_t i = 0; i < chunks && i < 8; ++i) end = std::max(end, word(jb, 8 + i * 8) + word(jb, 12 + i * 8));
    if (end > jb.size() || std::memcmp(jb.data(), bank.data(), 8) != 0) return;
    jb.resize(end);
    std::printf("[voice] level %d's sound bank: the Japanese disc's (%zu bytes, the game's %zu)\n", level, jb.size(), bank.size());
    bank = std::move(jb);
}

}  // namespace rac1
