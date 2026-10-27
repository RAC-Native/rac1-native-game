#include <runtime/gs/gs_renderer.h>
#include <profiler/host_profiler.h>
#include <glad/glad.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <formats/level_textures.h>
#include <formats/sky_model.h>
#include <runtime/kernel/ee_memory.h>
#include <game/render/texture_decode.h>

namespace rac1 {

extern std::vector<DecodedTexture> g_level_textures;
extern DecodedSky g_decoded_sky;

static std::unordered_map<uint32_t, unsigned int> g_gl_texture_cache;
static std::unordered_map<uint32_t, uint32_t> g_tbp0_to_pixel_vaddr;

void gs_bind_texture_vaddr(uint32_t tbp0, uint32_t pixel_vaddr) {
    g_tbp0_to_pixel_vaddr[tbp0] = pixel_vaddr;
}

void gs_texture_cache_clear() {
    for (auto& pair : g_gl_texture_cache) glDeleteTextures(1, &pair.second);
    g_gl_texture_cache.clear();
    g_tbp0_to_pixel_vaddr.clear();
}

// orig: (host) the GS's texture wrap (CLAMP_1 WMS / WMT) as a GL sampler
unsigned int gs_clamp_sampler(uint64_t clamp) {
    static unsigned int samplers[4] = {0, 0, 0, 0};
    const unsigned int wms = (clamp & 3) != 0 ? 1u : 0u;
    const unsigned int wmt = ((clamp >> 2) & 3) != 0 ? 1u : 0u;
    unsigned int& s = samplers[wms | wmt << 1];
    if (s == 0) {
        glGenSamplers(1, &s);
        glSamplerParameteri(s, GL_TEXTURE_WRAP_S, wms != 0 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glSamplerParameteri(s, GL_TEXTURE_WRAP_T, wmt != 0 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);  // as the uploads below
        glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    return s;
}

unsigned int get_or_upload_texture(uint32_t tbp0) {
    // 1. Check if we have a registered EE RAM pixel virtual address for this tbp0
    uint32_t pixel_vaddr = 0;
    auto map_it = g_tbp0_to_pixel_vaddr.find(tbp0);
    if (map_it != g_tbp0_to_pixel_vaddr.end()) {
        pixel_vaddr = map_it->second;
    }

    if (pixel_vaddr != 0) {
        // Check if we already have this EE RAM texture in our GL cache
        auto cache_it = g_gl_texture_cache.find(pixel_vaddr);
        if (cache_it != g_gl_texture_cache.end()) {
            return cache_it->second;
        }

        // Retrieve TextureHeader, palette, and pixels from EE RAM
        const TextureHeader* hdr = get_texture_header(pixel_vaddr);
        const uint32_t* palette = get_texture_palette(pixel_vaddr);
        if (hdr && palette) {
            uint32_t width = hdr->width;
            uint32_t height = hdr->height;
            uint32_t type = hdr->type;

            std::vector<uint8_t> rgba_data(width * height * 4);
            bool decoded = false;

            if (type == 20) { // PSMT4
                const uint8_t* src_pixels = get_texture_pixels(pixel_vaddr, (width * height) / 2);
                if (src_pixels) {
                    decode_psmt4(src_pixels, palette, width, height, rgba_data.data(), true);
                    decoded = true;
                }
            } else { // PSMT8 / default fallback
                const uint8_t* src_pixels = get_texture_pixels(pixel_vaddr, width * height);
                if (src_pixels) {
                    decode_psmt8(src_pixels, palette, width, height, rgba_data.data(), true);
                    decoded = true;
                }
            }

            if (decoded) {
                unsigned int tex_id = 0;
                glGenTextures(1, &tex_id);
                rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &tex_id, "GS renderer (legacy)");
                glBindTexture(GL_TEXTURE_2D, tex_id);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_data.data());
                glGenerateMipmap(GL_TEXTURE_2D);

                g_gl_texture_cache[pixel_vaddr] = tex_id;
                std::cout << "[GsRenderer] Decoded & Uploaded EE RAM texture (type=" << type 
                          << ", TBP0=" << tbp0 << ", pixel_vaddr=0x" << std::hex << pixel_vaddr << std::dec
                          << ", size=" << width << "x" << height 
                          << ") to GL tex_id=" << tex_id << std::endl;
                return tex_id;
            }
        }
    }

    auto it = g_gl_texture_cache.find(tbp0);
    if (it != g_gl_texture_cache.end()) {
        return it->second;
    }

    const DecodedTexture* tex = nullptr;
    std::string category;
    int idx = -1;

    if (tbp0 >= 800) {
        category = "sky";
        idx = tbp0 - 800;
    } else if (tbp0 >= 600) {
        category = "shrub";
        idx = tbp0 - 600;
    } else if (tbp0 >= 400) {
        category = "tie";
        idx = tbp0 - 400;
    } else if (tbp0 >= 200) {
        category = "moby";
        idx = tbp0 - 200;
    } else {
        category = "tfrag";
        idx = tbp0;
    }

    if (category == "sky") {
        if (idx >= 0 && idx < (int)g_decoded_sky.textures.size()) {
            tex = &g_decoded_sky.textures[idx];
        }
    } else {
        for (const auto& lt : g_level_textures) {
            if (lt.category == category && lt.index == idx) {
                tex = &lt;
                break;
            }
        }
    }

    unsigned int tex_id = 0;
    if (tex && !tex->rgba_data.empty()) {
        glGenTextures(1, &tex_id);
        rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &tex_id, "GS renderer (legacy)");
        glBindTexture(GL_TEXTURE_2D, tex_id);

        if (category == "sky") {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex->width, tex->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex->rgba_data.data());

        if (category != "sky") {
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        g_gl_texture_cache[tbp0] = tex_id;
        std::cout << "[GsRenderer] Uploaded " << category << " texture index " << idx 
                  << " (TBP0=" << tbp0 << ", size=" << tex->width << "x" << tex->height 
                  << ") to OpenGL tex_id=" << tex_id << std::endl;
    } else {
        glGenTextures(1, &tex_id);
        rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &tex_id, "GS renderer (legacy)");
        glBindTexture(GL_TEXTURE_2D, tex_id);
        uint32_t dummy_pixel = 0xffffffff; // fallback white
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &dummy_pixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        g_gl_texture_cache[tbp0] = tex_id;
        std::cerr << "[GsRenderer] WARNING: Texture not found for category=" << category 
                  << " idx=" << idx << " (TBP0=" << tbp0 << "). Using fallback white." << std::endl;
    }

    return tex_id;
}

} // namespace rac1
