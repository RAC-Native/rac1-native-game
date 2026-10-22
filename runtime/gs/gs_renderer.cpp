#include <runtime/gs/gs_renderer.h>
#include <profiler/host_profiler.h>
#include <glad/glad.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <unordered_map>
#include <formats/level_textures.h>
#include <formats/sky_model.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/gs/static_geom_cache.h>
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

namespace rac1 {

// Vertex and Fragment Shader Sources
static const char* vertex_shader_source = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoord;

out vec4 ourColor;
out vec2 TexCoord;

uniform vec2 uScreenSize;
uniform vec4 uRow0;
uniform vec4 uRow1;
uniform vec4 uRow2;
uniform vec3 uScale;
uniform bool uNeedsProj;

void main() {
    float ndcX = 0.0;
    float ndcY = 0.0;
    float ndcZ = 0.0;

    if (uNeedsProj) {
        float cx = dot(uRow0, vec4(aPos, 1.0));
        float cy = dot(uRow1, vec4(aPos, 1.0));
        float cw = dot(uRow2, vec4(aPos, 1.0));
        if (cw <= 0.0) cw = 0.001;
        
        float ndc_x = cx / cw;
        float ndc_y = cy / cw;
        
        float x = ndc_x * uScale.x / 128.0 + 320.0;
        float y = -ndc_y * uScale.y / 128.0 + 240.0;
        float z = uScale.z / cw;
        
        ndcX = (x / uScreenSize.x) * 2.0 - 1.0;
        ndcY = 1.0 - (y / uScreenSize.y) * 2.0;
        ndcZ = 1.0 - (z / 16777216.0);
    } else {
        ndcX = (aPos.x / uScreenSize.x) * 2.0 - 1.0;
        ndcY = 1.0 - (aPos.y / uScreenSize.y) * 2.0;
        ndcZ = 1.0 - (aPos.z / 16777216.0);
    }
    
    gl_Position = vec4(ndcX, ndcY, ndcZ, 1.0);
    ourColor = aColor;
    TexCoord = aTexCoord;
}
)glsl";

static const char* fragment_shader_source = R"glsl(
#version 330 core
out vec4 FragColor;

in vec4 ourColor;
in vec2 TexCoord;

uniform sampler2D ourTexture;
uniform bool uUseTexture;
uniform bool uIsSky;

void main() {
    vec4 col = ourColor;
    if (!uIsSky) {
        col.rgb = max(col.rgb, vec3(0.6));
    }
    if (uUseTexture) {
        FragColor = texture(ourTexture, TexCoord) * col;
    } else {
        FragColor = col;
    }
}
)glsl";


GsRenderer::GsRenderer() {}

GsRenderer::~GsRenderer() {
    shutdown();
}

bool GsRenderer::init() {
    // 1. Compile Vertex Shader
    unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, nullptr);
    glCompileShader(vertex_shader);
    
    int success;
    char info_log[512];
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex_shader, 512, nullptr, info_log);
        std::cerr << "[GsRenderer] Vertex shader compilation failed: " << info_log << std::endl;
        return false;
    }

    // 2. Compile Fragment Shader
    unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, nullptr);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragment_shader, 512, nullptr, info_log);
        std::cerr << "[GsRenderer] Fragment shader compilation failed: " << info_log << std::endl;
        glDeleteShader(vertex_shader);
        return false;
    }

    // 3. Link Shader Program
    m_shader_program = glCreateProgram();
    glAttachShader(m_shader_program, vertex_shader);
    glAttachShader(m_shader_program, fragment_shader);
    glLinkProgram(m_shader_program);
    glGetProgramiv(m_shader_program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(m_shader_program, 512, nullptr, info_log);
        std::cerr << "[GsRenderer] Shader program linking failed: " << info_log << std::endl;
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        return false;
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    // 4. Create buffers
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &m_vbo, "GS renderer (legacy)");

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    // Vertex positions (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex), (void*)offsetof(GLVertex, x));
    glEnableVertexAttribArray(0);

    // Vertex colors (location 1)
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GLVertex), (void*)offsetof(GLVertex, r));
    glEnableVertexAttribArray(1);

    // Vertex texture coordinates (location 2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GLVertex), (void*)offsetof(GLVertex, u));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glUseProgram(m_shader_program);
    glUniform1i(glGetUniformLocation(m_shader_program, "ourTexture"), 0);
    glUseProgram(0);

    return true;
}

static void apply_static_draw_call_states(const StaticDrawCall& dc, unsigned int shader_program, int screen_height) {
    glDisable(GL_SCISSOR_TEST);

    if ((dc.prim_reg & (1 << 6)) != 0) {
        glEnable(GL_BLEND);
        int a = (dc.alpha_reg >> 0) & 3;
        int b = (dc.alpha_reg >> 2) & 3;
        int c = (dc.alpha_reg >> 4) & 3;
        int d = (dc.alpha_reg >> 6) & 3;
        int add = (dc.alpha_reg >> 8) & 3;

        if (a == 0 && b == 1 && c == 0 && d == 1) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        } else if (a == 0 && b == 2 && c == 0 && d == 1) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        } else {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        if (add == 1) {
            glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
        } else {
            glBlendEquation(GL_FUNC_ADD);
        }
    } else {
        glDisable(GL_BLEND);
    }

    bool zte = (dc.test_reg & 1) != 0;
    if (zte) {
        glEnable(GL_DEPTH_TEST);
        int ztst = (dc.test_reg >> 1) & 3;
        switch (ztst) {
            case 0: glDepthFunc(GL_NEVER); break;
            case 1: glDepthFunc(GL_ALWAYS); break;
            case 2: glDepthFunc(GL_LEQUAL); break;
            case 3: glDepthFunc(GL_EQUAL); break;
            default: glDepthFunc(GL_GEQUAL); break;
        }
        bool zmsk = (dc.zbuf_reg & (1ULL << 32)) != 0;
        glDepthMask(zmsk ? GL_FALSE : GL_TRUE);
    } else {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }

    glUniform1i(glGetUniformLocation(shader_program, "uIsSky"), 0);
    if (dc.use_texture) {
        uint32_t tbp0 = dc.tex0_reg & 0x3FFF;
        unsigned int tex_id = get_or_upload_texture(tbp0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex_id);
        glUniform1i(glGetUniformLocation(shader_program, "uUseTexture"), 1);
    } else {
        glUniform1i(glGetUniformLocation(shader_program, "uUseTexture"), 0);
    }
}

void GsRenderer::render(const GsState& state, int screen_width, int screen_height) {
    static int s_current_level_id = -1;
    int active_level_id = ee_read<int32_t>(0x0015ed84); // LevelLoader::ADDR_ACTIVE_LEVEL_ID
    if (active_level_id != s_current_level_id) {
        for (auto& pair : g_gl_texture_cache) {
            glDeleteTextures(1, &pair.second);
        }
        g_gl_texture_cache.clear();
        g_tbp0_to_pixel_vaddr.clear();
        s_current_level_id = active_level_id;
        std::cout << "[GsRenderer] Detected level transition to " << active_level_id << ". Cleared texture cache." << std::endl;
    }

    static int render_print_count = 0;
    if (render_print_count < 10) {
        int sky_count = 0;
        int shrub_count = 0;
        int tie_count = 0;
        int moby_count = 0;
        int tfrag_count = 0;
        int untextured_count = 0;
        int sprite_count = 0;
        for (const auto& p : state.primitives) {
            if (p.type == PrimitiveType::Sprite) {
                sprite_count++;
            }
            bool tme = (p.prim_reg & (1 << 4)) != 0;
            if (!tme) {
                untextured_count++;
            } else {
                uint32_t tbp0 = p.tex0_reg & 0x3FFF;
                if (tbp0 >= 800) sky_count++;
                else if (tbp0 >= 600) shrub_count++;
                else if (tbp0 >= 400) tie_count++;
                else if (tbp0 >= 200) moby_count++;
                else tfrag_count++;
            }
        }
        std::cout << "[GsRenderer::render] screen_width=" << screen_width 
                  << ", screen_height=" << screen_height
                  << ", primitives=" << state.primitives.size() << std::endl;
        std::cout << "  [Categories] Sky: " << sky_count
                  << ", Shrub: " << shrub_count
                  << ", Tie: " << tie_count
                  << ", Moby: " << moby_count
                  << ", Tfrag: " << tfrag_count
                  << ", Untextured: " << untextured_count
                  << ", Sprites: " << sprite_count << std::endl;
        render_print_count++;
    }

    glUseProgram(m_shader_program);
    glUniform2f(glGetUniformLocation(m_shader_program, "uScreenSize"), (float)screen_width, (float)screen_height);

    // Setup camera projection uniforms for static geometry
    float r0[4], r1[4], r2[4];
    for (int i = 0; i < 4; ++i) {
        r0[i] = ee_read<float>(0x16ccc0 + i * 4);
        r1[i] = ee_read<float>(0x16ccd0 + i * 4);
        r2[i] = ee_read<float>(0x16cce0 + i * 4);
    }
    float scale_x = ee_read<float>(0x16ccf0);
    float scale_y = ee_read<float>(0x16ccf4);
    float scale_z = ee_read<float>(0x16ccf8);

    glUniform4f(glGetUniformLocation(m_shader_program, "uRow0"), r0[0], r0[1], r0[2], r0[3]);
    glUniform4f(glGetUniformLocation(m_shader_program, "uRow1"), r1[0], r1[1], r1[2], r1[3]);
    glUniform4f(glGetUniformLocation(m_shader_program, "uRow2"), r2[0], r2[1], r2[2], r2[3]);
    glUniform3f(glGetUniformLocation(m_shader_program, "uScale"), scale_x, scale_y, scale_z);

    auto render_primitive = [&](const GsPrimitive& prim) {
        if (prim.scissor_reg != 0) {
            glEnable(GL_SCISSOR_TEST);
            int x0 = prim.scissor_reg & 0x7FF;
            int x1 = (prim.scissor_reg >> 16) & 0x7FF;
            int y0 = (prim.scissor_reg >> 32) & 0x7FF;
            int y1 = (prim.scissor_reg >> 48) & 0x7FF;
            
            int width = x1 - x0 + 1;
            int height = y1 - y0 + 1;
            glScissor(x0, screen_height - (y0 + height), width, height);
        } else {
            glDisable(GL_SCISSOR_TEST);
        }

        if ((prim.prim_reg & (1 << 6)) != 0) {
            glEnable(GL_BLEND);
            int a = (prim.alpha_reg >> 0) & 3;
            int b = (prim.alpha_reg >> 2) & 3;
            int c = (prim.alpha_reg >> 4) & 3;
            int d = (prim.alpha_reg >> 6) & 3;
            int add = (prim.alpha_reg >> 8) & 3;

            if (a == 0 && b == 1 && c == 0 && d == 1) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            } else if (a == 0 && b == 2 && c == 0 && d == 1) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            } else {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }

            if (add == 1) {
                glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
            } else {
                glBlendEquation(GL_FUNC_ADD);
            }
        } else {
            glDisable(GL_BLEND);
        }

        bool zte = (prim.test_reg & 1) != 0;
        if (zte) {
            glEnable(GL_DEPTH_TEST);
            int ztst = (prim.test_reg >> 1) & 3;
            switch (ztst) {
                case 0: glDepthFunc(GL_NEVER); break;
                case 1: glDepthFunc(GL_ALWAYS); break;
                case 2: glDepthFunc(GL_LEQUAL); break;
                case 3: glDepthFunc(GL_EQUAL); break;
                default: glDepthFunc(GL_GEQUAL); break;
            }
            bool zmsk = (prim.zbuf_reg & (1ULL << 32)) != 0;
            glDepthMask(zmsk ? GL_FALSE : GL_TRUE);
        } else {
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
        }

        glUniform1i(glGetUniformLocation(m_shader_program, "uIsSky"), prim.is_sky ? 1 : 0);
        bool tme = (prim.prim_reg & (1 << 4)) != 0;
        if (tme) {
            uint32_t tbp0 = prim.tex0_reg & 0x3FFF;
            unsigned int tex_id = get_or_upload_texture(tbp0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex_id);
            glUniform1i(glGetUniformLocation(m_shader_program, "uUseTexture"), 1);
        } else {
            glUniform1i(glGetUniformLocation(m_shader_program, "uUseTexture"), 0);
        }

        std::vector<GLVertex> gl_vertices;
        bool fst = (prim.prim_reg & (1 << 8)) != 0;

        float tex_w = 256.0f;
        float tex_h = 256.0f;
        if (tme) {
            int w_pow = (prim.tex0_reg >> 26) & 0xF;
            int h_pow = (prim.tex0_reg >> 30) & 0xF;
            tex_w = (float)(1 << w_pow);
            tex_h = (float)(1 << h_pow);
        }

        if (prim.type == PrimitiveType::Sprite) {
            if (prim.vertices.size() >= 2) {
                const auto& v0 = prim.vertices[0];
                const auto& v1 = prim.vertices[1];

                float tu0 = fst ? (v0.u / tex_w) : v0.s;
                float tv0 = fst ? (v0.v / tex_h) : v0.t;
                float tu1 = fst ? (v1.u / tex_w) : v1.s;
                float tv1 = fst ? (v1.v / tex_h) : v1.t;

                float r0 = v0.r / 255.0f;
                float g0 = v0.g / 255.0f;
                float b0 = v0.b / 255.0f;
                float a0 = v0.a / 255.0f;

                float r1 = v1.r / 255.0f;
                float g1 = v1.g / 255.0f;
                float b1 = v1.b / 255.0f;
                float a1 = v1.a / 255.0f;

                gl_vertices.push_back({v0.x, v0.y, v0.z, r0, g0, b0, a0, tu0, tv0});
                gl_vertices.push_back({v1.x, v0.y, v0.z, r1, g0, b0, a0, tu1, tv0});
                gl_vertices.push_back({v1.x, v1.y, v1.z, r1, g1, b1, a1, tu1, tv1});

                gl_vertices.push_back({v0.x, v0.y, v0.z, r0, g0, b0, a0, tu0, tv0});
                gl_vertices.push_back({v1.x, v1.y, v1.z, r1, g1, b1, a1, tu1, tv1});
                gl_vertices.push_back({v0.x, v1.y, v1.z, r0, g1, b1, a1, tu0, tv1});
            }
        } else {
            for (const auto& v : prim.vertices) {
                float r = v.r / 255.0f;
                float g = v.g / 255.0f;
                float b = v.b / 255.0f;
                float a = v.a / 255.0f;

                float tu = fst ? (v.u / tex_w) : v.s;
                float tv = fst ? (v.v / tex_h) : v.t;

                gl_vertices.push_back({v.x, v.y, v.z, r, g, b, a, tu, tv});
            }
        }

        if (gl_vertices.empty()) return;

        glBufferData(GL_ARRAY_BUFFER, gl_vertices.size() * sizeof(GLVertex), gl_vertices.data(), GL_STREAM_DRAW);

        GLenum draw_mode = GL_TRIANGLES;
        if (prim.type == PrimitiveType::Point) {
            draw_mode = GL_POINTS;
        } else if (prim.type == PrimitiveType::Line) {
            draw_mode = GL_LINES;
        } else if (prim.type == PrimitiveType::LineStrip) {
            draw_mode = GL_LINE_STRIP;
        }

        glDrawArrays(draw_mode, 0, (GLsizei)gl_vertices.size());
    };

    // --- STEP 1: Draw Sky (Needs uNeedsProj = false) ---
    // (Bypassed since we render Sky natively)
    /*
    glUniform1i(glGetUniformLocation(m_shader_program, "uNeedsProj"), 0);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    for (const auto& prim : state.primitives) {
        if (prim.is_sky) {
            render_primitive(prim);
        }
    }
    */

    // --- STEP 2: Draw Static Geometry ---
    // (Bypassed since we render Static Geometry natively)
    /*
    auto draw_static_geometry = [&](const std::vector<StaticDrawCall>& draw_calls, const std::vector<int>& visible_indices) {
        glUniform1i(glGetUniformLocation(m_shader_program, "uNeedsProj"), 1);
        for (int idx : visible_indices) {
            const auto& dc = draw_calls[idx];
            apply_static_draw_call_states(dc, m_shader_program, screen_height);
            glBindVertexArray(dc.vao);
            glDrawElements(GL_TRIANGLES, dc.index_count, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);
    };

    if (g_use_gpu_rendering) {
        draw_static_geometry(g_static_tfrag_draw_calls, g_visible_tfrag_indices);
        draw_static_geometry(g_static_tie_draw_calls, g_visible_tie_indices);
        draw_static_geometry(g_static_shrub_draw_calls, g_visible_shrub_indices);
    }
    */

    // --- STEP 3: Draw Mobys/Dynamic Geometry ---
    glUniform1i(glGetUniformLocation(m_shader_program, "uNeedsProj"), 0);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    for (const auto& prim : state.primitives) {
        if (prim.is_sky) {
            continue;
        }

        // Filter out 3D Mobys / geometry.
        // Mobys typically have tbp0 in [200, 400), shrubs in [600, 800), tie in [400, 600).
        // 2D primitives / HUD elements have tbp0 < 200 or are sprites/lines/points.
        bool tme = (prim.prim_reg & (1 << 4)) != 0;
        if (tme) {
            uint32_t tbp0 = prim.tex0_reg & 0x3FFF;
            if (tbp0 >= 200 && prim.type != PrimitiveType::Sprite) {
                continue;
            }
        }

        render_primitive(prim);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void GsRenderer::shutdown() {
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_shader_program != 0) {
        glDeleteProgram(m_shader_program);
        m_shader_program = 0;
    }
}

void gs_clear_buffers(float r, float g, float b, float a) {
    if (glad_glClear) {
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
}

} // namespace rac1
