#include <game/render/render_toggles.h>
#include <profiler/host_profiler.h>
#include <game/ovl/ovl_reloc.h>
#include <game/render/sky_render.h>
#include <game/render/render_view.h>
#include <runtime/level/level_loader.h>
#include <runtime/kernel/ee_memory.h>
#include <formats/sky_model.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <span>
#include <cstring>
#include <bit>
#include <iostream>
#include <glad/glad.h>
#include <runtime/gs/gs_renderer.h>
#include "game/render/shader_manager.h"

namespace rac1 {

extern std::uint32_t g_core_data_vaddr;
extern void (*g_sky_init_callback)(std::uint32_t, std::uint32_t);

extern DecodedSky g_decoded_sky;
static bool g_sky_initialized = false;
static bool g_sky_gl_initialized = false;
extern bool g_parsing_sky;

struct SkyDrawCommand {
    int index_count;
    int index_offset; // byte offset in EBO
    int material_index;
    bool textured;
};

static std::vector<SkyDrawCommand> g_sky_draw_commands;
static GLuint g_sky_vao = 0;
static GLuint g_sky_vbo = 0;
static GLuint g_sky_ebo = 0;
static std::vector<GLuint> g_sky_gl_textures;

// orig: (host) decodes the sky at an EE address into the host renderer (false: not a valid sky)
static bool sky_decode_at(std::uint32_t sky_addr, const std::byte*& ram_ptr, std::size_t& size) {
    uint32_t max_size = 2 * 1024 * 1024; // 2MB max
    if (sky_addr + max_size > host_layout::EE_END) {  // the host's copy of the level lies above the game's 32 MB
        max_size = host_layout::EE_END - sky_addr;
    }

    ram_ptr = reinterpret_cast<const std::byte*>(ee_ram_ptr(sky_addr));
    if (!ram_ptr) {
        std::cerr << "[sky_init] ee_ram_ptr returned nullptr for sky_addr=0x" << std::hex << sky_addr << std::dec << std::endl;
        return false;
    }
    size = max_size;

    std::span<const std::byte> sky_span(ram_ptr, max_size);

    int level_id = ee_read<int32_t>(LevelLoader::ADDR_ACTIVE_LEVEL_ID);
    std::cout << "[sky_init] Active level ID read from ADDR_ACTIVE_LEVEL_ID: " << level_id << std::endl;

    g_decoded_sky = decode_level_sky(sky_span, level_id);
    g_sky_initialized = !g_decoded_sky.shells.empty() && g_decoded_sky.shells.size() < 32;
    if (!g_sky_initialized) {
        std::cerr << "[sky_init] decode produced invalid sky (shells=" << g_decoded_sky.shells.size()
                  << ") — sky disabled until offset is fixed." << std::endl;
        return false;
    }
    g_sky_gl_initialized = false;
    return true;
}

void sky_init(std::uint32_t sky_offset, std::uint32_t tfrag_textures_offset) {
    (void)tfrag_textures_offset;
    uint32_t sky_addr = sky_offset + g_core_data_vaddr;

    std::cout << "[sky_init] Entered sky_init with sky_offset=" << sky_offset
              << ", g_core_data_vaddr=" << g_core_data_vaddr << std::endl;
    std::cout << "[sky_init] Computed sky_addr=0x" << std::hex << sky_addr << std::dec << std::endl;

    const std::byte* ram_ptr = nullptr;
    std::size_t size = 0;
    if (!sky_decode_at(sky_addr, ram_ptr, size)) return;

    ee_write<uint32_t>(OV(0x0015f350), NOT_ADDR(0x002043b0));
    ee_write<uint32_t>(OV(0x00160d60), NOT_ADDR(0x002043b0));

    if (size >= 0x40) {
        const int32_t texture_defs = *reinterpret_cast<const int32_t*>(ram_ptr + 0x10);
        ee_write<uint32_t>(OV(0x00160510), sky_addr + texture_defs);
        ee_write<uint32_t>(OV(0x00160d50), sky_addr + texture_defs);
    }
}

// orig: (host) the sky of a scene whose data is in EE memory (the boot module's title level):
// decoded for the host's sky drawer, nothing written into the game's state
bool sky_scene_load(std::uint32_t sky_addr) {
    const std::byte* ram_ptr = nullptr;
    std::size_t size = 0;
    return sky_decode_at(sky_addr, ram_ptr, size);
}

void sky_draw() {
    if (!g_sky_initialized) {
        glClearColor(0.02f, 0.02f, 0.04f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    g_parsing_sky = true;

    if (g_decoded_sky.clear_screen) {
        glClearColor(g_decoded_sky.color_r / 255.f, g_decoded_sky.color_g / 255.f, g_decoded_sky.color_b / 255.f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    if (!g_sky_gl_initialized) {
        std::vector<NativeVertex> vertices;
        std::vector<uint32_t> indices;
        g_sky_draw_commands.clear();

        for (const auto& shell : g_decoded_sky.shells) {
            int base_vertex = vertices.size();
            for (const auto& v : shell.mesh.vertices) {
                NativeVertex nv;
                nv.x = v.x; nv.y = v.y; nv.z = v.z;
                nv.u = v.u; nv.v = v.v;
                nv.r = v.r / 255.f; nv.g = v.g / 255.f; nv.b = v.b / 255.f; nv.a = v.a / 255.f;
                vertices.push_back(nv);
            }

            for (const auto& sm : shell.mesh.submeshes) {
                SkyDrawCommand cmd;
                cmd.material_index = sm.material_index;
                cmd.textured = shell.textured && (sm.material_index != -1);
                cmd.index_count = sm.triangles.size() * 3;
                cmd.index_offset = indices.size() * sizeof(uint32_t);

                for (const auto& tri : sm.triangles) {
                    indices.push_back(base_vertex + tri[0]);
                    indices.push_back(base_vertex + tri[1]);
                    indices.push_back(base_vertex + tri[2]);
                }
                
                if (cmd.index_count > 0) {
                    g_sky_draw_commands.push_back(cmd);
                }
            }
        }

        if (g_sky_vao == 0) {
            glGenVertexArrays(1, &g_sky_vao);
            glGenBuffers(1, &g_sky_vbo);
            rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &g_sky_vbo, "Sky (legacy)");
            glGenBuffers(1, &g_sky_ebo);
            rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &g_sky_ebo, "Sky (legacy)");
        }

        glBindVertexArray(g_sky_vao);
        glBindBuffer(GL_ARRAY_BUFFER, g_sky_vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(NativeVertex), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_sky_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(NativeVertex), (void*)offsetof(NativeVertex, x));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(NativeVertex), (void*)offsetof(NativeVertex, r));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(NativeVertex), (void*)offsetof(NativeVertex, u));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
        
        // Upload textures
        for (GLuint tex_id : g_sky_gl_textures) {
            glDeleteTextures(1, &tex_id);
        }
        g_sky_gl_textures.clear();
        for (const auto& tex : g_decoded_sky.textures) {
            GLuint tex_id = 0;
            glGenTextures(1, &tex_id);
            rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &tex_id, "Sky (legacy)");
            glBindTexture(GL_TEXTURE_2D, tex_id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex.width, tex.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.rgba_data.data());
            g_sky_gl_textures.push_back(tex_id);
        }

        g_sky_gl_initialized = true;
    }

    glm::mat4 view = render_view_matrix();
    // Remove translation for skybox
    view[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

    glm::mat4 custom_proj(0.0f);
    float fov = glm::radians(60.0f);
    float aspect = g_render_aspect;
    float n = 0.1f;
    float f = 4000.0f;
    custom_proj[0][0] = render_proj_x(aspect, fov);
    custom_proj[1][1] = render_proj_y(fov);
    custom_proj[2][2] = -(f + n) / (f - n);
    custom_proj[3][2] = -(2.0f * f * n) / (f - n);
    custom_proj[2][3] = -1.0f;

    // Apply standard camera projection and rotation
    glm::mat4 viewProj = custom_proj * view;

    // Use a safe scale factor (20x) to ensure sky radius is around ~1100
    // This perfectly fits between near (0.1) and far (4000) planes.
    glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(20.0f));

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE); // Ensure inner faces are visible!
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLuint shader = ShaderManager::get().get_sky_shader();
    glUseProgram(shader);
    
    GLint vpLoc = glGetUniformLocation(shader, "u_ViewProj");
    if (vpLoc != -1) {
        glUniformMatrix4fv(vpLoc, 1, GL_FALSE, glm::value_ptr(viewProj));
    }
    
    GLint modelLoc = glGetUniformLocation(shader, "u_Model");
    if (modelLoc != -1) {
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    }
    
    GLint useTexLoc = glGetUniformLocation(shader, "u_UseTexture");
    GLint texLoc = glGetUniformLocation(shader, "u_Texture");

    glBindVertexArray(g_sky_vao);

    for (const auto& cmd : g_sky_draw_commands) {
        if (cmd.textured && cmd.material_index >= 0 && cmd.material_index < static_cast<int>(g_sky_gl_textures.size())) {
            GLuint tex_id = g_sky_gl_textures[cmd.material_index];
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex_id);
            if (texLoc != -1) glUniform1i(texLoc, 0);
            if (useTexLoc != -1) glUniform1i(useTexLoc, 1);
        } else {
            if (useTexLoc != -1) glUniform1i(useTexLoc, 0);
        }

        glDrawElements(GL_TRIANGLES, cmd.index_count, GL_UNSIGNED_INT, (void*)(intptr_t)cmd.index_offset);
    }

    glBindVertexArray(0);
    glUseProgram(0);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE); // Restore global state
    glDisable(GL_BLEND);

    g_parsing_sky = false;
}

struct SkyRenderInitializer {
    // orig: 0x00204f60
    SkyRenderInitializer() {
        g_sky_init_callback = sky_init;
    }
} g_sky_render_initializer;

} // namespace rac1
