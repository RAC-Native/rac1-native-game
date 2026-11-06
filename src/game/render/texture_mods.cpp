// See texture_mods.h.
#include <game/render/texture_mods.h>

#include <runtime/kernel/mods.h>

#include <platform/platform.h>

#include <stb_image.h>        // (its implementation: static_skinned_render.cpp, with TinyGLTF's)
#include <stb_image_write.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>

namespace rac1::texture_mods {

namespace {
std::mutex g_lock;
bool g_indexed = false;
std::unordered_map<uint64_t, std::filesystem::path> g_pngs;                 // the mods' textures by hash
std::unordered_map<uint64_t, std::shared_ptr<const Image>> g_loaded;        // decoded (null: the PNG could not be read)
std::atomic<bool> g_dump{false};
std::atomic<int> g_on{-1};  // -1: not decided yet
std::set<uint64_t> g_dumped;
std::atomic<size_t> g_replaced{0};
std::set<uint64_t> g_replaced_seen;
constexpr size_t kMaxDumps = 20000;  // (a guard: a screen that makes new pictures every frame)

// orig: (none) — enhancement: "0123456789abcdef.png" (any case) to its hash; false for other names
bool name_hash(const std::string& file, uint64_t& out) {
    if (file.size() != 20) return false;
    std::string ext = file.substr(16);
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext != ".png") return false;
    uint64_t v = 0;
    for (size_t i = 0; i < 16; ++i) {
        const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(file[i])));
        if (c >= '0' && c <= '9') v = v << 4 | static_cast<uint64_t>(c - '0');
        else if (c >= 'a' && c <= 'f') v = v << 4 | static_cast<uint64_t>(c - 'a' + 10);
        else return false;
    }
    out = v;
    return true;
}

// orig: (none) — enhancement: the mods' textures indexed (once; g_lock held)
void index_pngs() {
    if (g_indexed) return;
    g_indexed = true;
    for (const auto& [rel, path] : mods_files("textures")) {
        uint64_t h = 0;
        if (!name_hash(path.filename().string(), h)) continue;
        if (!g_pngs.emplace(h, path).second)
            std::fprintf(stderr, "[mods] texture %016llx twice: %s kept, %s ignored\n", static_cast<unsigned long long>(h), g_pngs[h].string().c_str(),
                         path.string().c_str());
    }
    if (!g_pngs.empty()) std::printf("[mods] %zu replacement textures\n", g_pngs.size());
}

// orig: (none) — enhancement: a PNG decoded to RGBA with the GS's alpha (0..255 -> 0..0x80), its mip chain made
std::shared_ptr<const Image> load(const std::filesystem::path& path) {
    std::FILE* f = platform::open_file(path, "rb");
    if (f == nullptr) return nullptr;
    int w = 0, h = 0, n = 0;
    stbi_uc* data = stbi_load_from_file(f, &w, &h, &n, 4);
    std::fclose(f);
    if (data == nullptr || w <= 0 || h <= 0 || w > 8192 || h > 8192) {
        std::fprintf(stderr, "[mods] %s: not a PNG the game reads (%s)\n", path.string().c_str(), data == nullptr ? stbi_failure_reason() : "past 8192 pixels");
        if (data != nullptr) stbi_image_free(data);
        return nullptr;
    }
    auto img = std::make_shared<Image>();
    img->w = static_cast<uint32_t>(w);
    img->h = static_cast<uint32_t>(h);
    std::vector<uint32_t> px(static_cast<size_t>(w) * h);
    for (size_t i = 0; i < px.size(); ++i) {
        const stbi_uc* p = data + i * 4;
        const uint32_t a = (static_cast<uint32_t>(p[3]) * 128u + 127u) / 255u;
        if (a < 0x80) img->opaque = false;
        px[i] = static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 | static_cast<uint32_t>(p[2]) << 16 | a << 24;
    }
    stbi_image_free(data);
    img->mips.push_back(std::move(px));
    // the mips: each a 2 x 2 box of the one above (an odd size's last row / column counted once)
    uint32_t cw = img->w, ch = img->h;
    while (cw > 1 || ch > 1) {
        const uint32_t nw = std::max(1u, cw / 2), nh = std::max(1u, ch / 2);
        const std::vector<uint32_t>& up = img->mips.back();
        std::vector<uint32_t> dn(static_cast<size_t>(nw) * nh);
        for (uint32_t y = 0; y < nh; ++y)
            for (uint32_t x = 0; x < nw; ++x) {
                uint32_t sum[4] = {0, 0, 0, 0};
                const uint32_t x0 = std::min(cw - 1, x * 2), x1 = std::min(cw - 1, x * 2 + 1), y0 = std::min(ch - 1, y * 2), y1 = std::min(ch - 1, y * 2 + 1);
                for (const uint32_t c : {up[y0 * cw + x0], up[y0 * cw + x1], up[y1 * cw + x0], up[y1 * cw + x1]})
                    for (int k = 0; k < 4; ++k) sum[k] += (c >> (8 * k)) & 0xff;
                dn[static_cast<size_t>(y) * nw + x] = (sum[0] + 2) / 4 | ((sum[1] + 2) / 4) << 8 | ((sum[2] + 2) / 4) << 16 | ((sum[3] + 2) / 4) << 24;
            }
        img->mips.push_back(std::move(dn));
        cw = nw;
        ch = nh;
    }
    return img;
}

// orig: (none) — enhancement: a texture written to the dump (g_lock held)
void dump(uint64_t hsh, uint32_t w, uint32_t h, const uint32_t* px, const char* kind) {
    if (g_dumped.size() >= kMaxDumps || !g_dumped.insert(hsh).second) return;
    const std::filesystem::path dir = dump_dir();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    char name[32];
    std::snprintf(name, sizeof name, "%016llx.png", static_cast<unsigned long long>(hsh));
    const std::filesystem::path path = dir / name;
    if (!std::filesystem::exists(path, ec)) {
        std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
        for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i) {
            const uint32_t c = px[i];
            rgba[i * 4 + 0] = static_cast<uint8_t>(c);
            rgba[i * 4 + 1] = static_cast<uint8_t>(c >> 8);
            rgba[i * 4 + 2] = static_cast<uint8_t>(c >> 16);
            rgba[i * 4 + 3] = static_cast<uint8_t>(std::min(255u, (c >> 24) * 255u / 128u));
        }
        std::vector<uint8_t> png;
        stbi_write_png_to_func([](void* ctx, void* data, int size) {
            auto* out = static_cast<std::vector<uint8_t>*>(ctx);
            out->insert(out->end(), static_cast<uint8_t*>(data), static_cast<uint8_t*>(data) + size);
        }, &png, static_cast<int>(w), static_cast<int>(h), 4, rgba.data(), static_cast<int>(w) * 4);
        if (png.empty() || !platform::write_file_replace(path, png.data(), png.size())) {
            std::fprintf(stderr, "[mods] cannot write %s\n", path.string().c_str());
            return;
        }
        if (std::FILE* f = platform::open_file(dir / "index.txt", "a")) {
            std::fprintf(f, "%016llx %s %ux%u\n", static_cast<unsigned long long>(hsh), kind, w, h);
            std::fclose(f);
        }
    }
    if (g_dumped.size() == kMaxDumps) std::fprintf(stderr, "[mods] %zu textures dumped: no more in this run\n", kMaxDumps);
}
}  // namespace

// orig: (none) — enhancement (texture_mods.h)
bool on() {
    int v = g_on.load(std::memory_order_acquire);
    if (v < 0) {
        std::scoped_lock lock(g_lock);
        index_pngs();
        static const bool env_dump = [] {
            const char* e = std::getenv("RAC1_TEX_DUMP");
            return e != nullptr && *e != 0 && std::strcmp(e, "0") != 0;
        }();
        if (env_dump) g_dump = true;
        v = !g_pngs.empty() ? 1 : 0;
        g_on.store(v, std::memory_order_release);
    }
    return v != 0 || g_dump.load(std::memory_order_relaxed);
}

// orig: (none) — enhancement (texture_mods.h)
uint64_t hash(uint32_t w, uint32_t h, const uint32_t* px) {
    uint64_t v = 14695981039346656037ull;
    auto mix = [&v](uint32_t x) { v = (v ^ x) * 1099511628211ull; };
    mix(w);
    mix(h);
    const size_t n = static_cast<size_t>(w) * h;
    for (size_t i = 0; i < n; ++i) mix(px[i]);
    return v;
}

// orig: (none) — enhancement (texture_mods.h)
std::shared_ptr<const Image> find(uint32_t w, uint32_t h, const uint32_t* px, const char* kind) {
    if (!on() || w == 0 || h == 0 || px == nullptr) return nullptr;
    const uint64_t hsh = hash(w, h, px);
    std::scoped_lock lock(g_lock);
    if (kind != nullptr && g_dump.load(std::memory_order_relaxed)) dump(hsh, w, h, px, kind);
    const auto it = g_pngs.find(hsh);
    if (it == g_pngs.end()) return nullptr;
    auto [l, fresh] = g_loaded.try_emplace(hsh);
    if (fresh) l->second = load(it->second);
    if (l->second != nullptr && g_replaced_seen.insert(hsh).second) {
        ++g_replaced;
        std::printf("[mods] texture %016llx (%s %ux%u) replaced: %s (%ux%u)\n", static_cast<unsigned long long>(hsh), kind != nullptr ? kind : "2d", w, h, it->second.string().c_str(),
                    l->second->w, l->second->h);
    }
    return l->second;
}

// orig: (none) — enhancement (texture_mods.h)
void dump_set(bool on_) {
    on();  // (the index first: g_on decided)
    g_dump = on_;
    if (on_) std::printf("[mods] dumping the textures into %s\n", dump_dir().string().c_str());
}

// orig: (none) — enhancement (texture_mods.h)
bool dump_on() {
    on();
    return g_dump.load(std::memory_order_relaxed);
}

// orig: (none) — enhancement (texture_mods.h)
std::filesystem::path dump_dir() {
    const char* e = std::getenv("RAC1_TEX_DUMP");
    if (e != nullptr && *e != 0 && std::strcmp(e, "0") != 0 && std::strcmp(e, "1") != 0) return e;
    return mods_dir() / "_dump" / "textures";
}

// orig: (none) — enhancement (texture_mods.h)
size_t replaced_count() { return g_replaced.load(); }

}  // namespace rac1::texture_mods
