// Host renderer: the level's tfrag / tie / shrub drawn from the host's copy of the level (not a port).
#include <game/render/tfrag_render.h>
#include <array>
#include <game/render/render_view.h>
#include <runtime/level/host_layout.h>
#include <game/render/tie_render.h>
#include <game/render/shrub_render.h>
#include <game/render/render_toggles.h>
#include <runtime/level/level_loader.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/kernel/vu0.h>
#include <runtime/gs/static_geom_cache.h>
#include <game/render/shader_manager.h>
#include <game/render/world_draw.h>
#include <glad/glad.h>
#include <runtime/gs/gs_renderer.h>
#include <formats/level_mesh.h>
#include <game/port_stubs.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <span>
#include <cstring>
#include <bit>
#include <iostream>

namespace rac1 {

static LevelStaticGeometry g_tfrag_geom;
static bool g_tfrag_initialized = false;

// Custom level loader initialization callback
static void tfrag_init(std::span<const std::byte> level_wad_data) {
    try {
        g_tfrag_geom = decode_level_meshes(level_wad_data);
        g_tfrag_initialized = !g_tfrag_geom.tfrags.empty();
        std::cout << "[tfrag_init] Decoded " << g_tfrag_geom.tfrags.size() << " terrain fragments." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[tfrag_init] Failed to decode level meshes: " << e.what() << std::endl;
        g_tfrag_initialized = false;
    }
}

void tfrag_render_update() {
    if (!g_tfrag_initialized) {
        return;
    }
    if (!g_render_tfrag) {
        if (g_render_tie) tie_render_update();
        if (g_render_shrub) shrub_render_update();
        return;
    }

    glm::mat4 view = render_view_matrix();

    glm::mat4 custom_proj(0.0f);
    float fov = glm::radians(60.0f);
    float aspect = g_render_aspect;
    float n = 0.1f;
    float f = 4000.0f;
    custom_proj[0][0] = render_proj_x(aspect, fov);
    custom_proj[1][1] = render_proj_y(fov);
    custom_proj[2][2] = -(f + n) / (f - n);
    custom_proj[3][2] = -(2.0f * f * n) / (f - n);
    custom_proj[2][3] = -1.0f; // W = -Z_cam (standard OpenGL)

    glm::mat4 view_proj = custom_proj * view;

    // (host) the draw calls whose box is entirely outside a side plane of the view are not drawn: the
    // GPU would clip all their triangles away (the near / far planes are clamped, not culled)
    static WorldMaterials materials(host_layout::TEX_TABLE_TFRAG, 0);
    materials.begin_frame();
    const WorldFrustum frustum(view_proj);
    g_visible_tfrag_indices.clear();
    for (size_t i = 0; i < g_static_tfrag_draw_calls.size(); ++i) {
        const auto& c = g_static_tfrag_draw_calls[i];
        if (frustum.box_outside(c.aabb_min, c.aabb_max)) continue;
        g_visible_tfrag_indices.push_back(static_cast<int>(i));
        if (c.use_texture) materials.resolve(c.material_index);  // before any binding of the draw
    }

    glm::mat4 model = glm::mat4(1.0f);

    GLuint shader = ShaderManager::get().get_tfrag_shader();
    glUseProgram(shader);

    GLint vp_loc = glGetUniformLocation(shader, "u_ViewProj");
    if (vp_loc != -1) {
        glUniformMatrix4fv(vp_loc, 1, GL_FALSE, glm::value_ptr(view_proj));
    }
    GLint model_loc = glGetUniformLocation(shader, "u_Model");
    if (model_loc != -1) {
        glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(model));
    }

    // Disable culling to prevent holes
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_CLAMP);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    GLint alpha_test_loc = glGetUniformLocation(shader, "u_AlphaTest");
    GLint tex_loc = glGetUniformLocation(shader, "u_Texture");
    GLint use_tex_loc = glGetUniformLocation(shader, "u_UseTexture");
    if (tex_loc != -1) glUniform1i(tex_loc, 0);

    auto key = [&](const StaticDrawCall& dc) {
        WorldStateKey k;
        k.use_texture = dc.use_texture;
        k.blended = dc.is_blended;
        k.masked = dc.is_masked;
        if (dc.use_texture) {
            k.tex = materials.resolve(dc.material_index);
            k.sampler = gs_clamp_sampler(dc.clamp_reg);  // the texture's CLAMP_1 wrap (WMS / WMT)
        }
        return k;
    };
    auto apply = [&](const WorldStateKey& k, const WorldStateKey* prev) {
        if (!prev || k.blended != prev->blended) {
            if (k.blended) {
                glDepthMask(GL_FALSE);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            } else {
                glDepthMask(GL_TRUE);
                glDisable(GL_BLEND);
            }
        }
        if ((!prev || k.masked != prev->masked) && alpha_test_loc != -1) glUniform1i(alpha_test_loc, k.masked ? 1 : 0);
        if (k.use_texture) {
            if ((!prev || !prev->use_texture) && use_tex_loc != -1) glUniform1i(use_tex_loc, 1);
            if (!prev || !prev->use_texture || k.tex != prev->tex) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, k.tex);
            }
            glBindSampler(0, k.sampler);
        } else if ((!prev || prev->use_texture) && use_tex_loc != -1) {
            glUniform1i(use_tex_loc, 0);
        }
    };
    world_draw_runs(g_static_tfrag_draw_calls, g_visible_tfrag_indices, g_static_tfrag_geom.vao, key, apply);
    
    glBindSampler(0, 0);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    
    // Culling remains disabled globally to prevent PS2 winding order issues
    
    if (g_render_tie) tie_render_update();
    if (g_render_shrub) shrub_render_update();
}

// orig: not ported — runtime VBO cache helper (a performance cache)
void tfrag_build_static_cache() {
    if (!g_tfrag_initialized) {
        return;
    }
    StaticMergedGeom& G = g_static_tfrag_geom;
    for (const auto& mesh : g_tfrag_geom.tfrags) {
        for (const auto& sm : mesh.submeshes) {
            bool use_texture = (sm.material_index != -1);
            bool tcc = ((sm.tex0 >> 34) & 1) != 0;
            bool abe = false;
            
            if (use_texture) {
                uint32_t packed_ptr = ee_read<uint32_t>(host_layout::TEX_TABLE_TFRAG + sm.material_index * 4);
                if (packed_ptr != 0) {
                    uint32_t pixel_addr = packed_ptr & 0x0FFFFFFF;
                    uint32_t header_addr = pixel_addr - 1024 - 12;
                    if (ee_is_valid_addr(header_addr, 12)) {
                        TextureHeader tex_hdr = ee_read<TextureHeader>(header_addr);
                        abe = (tex_hdr.pad & (1 << 1)) != 0 || (tex_hdr.pad & (1 << 6)) != 0;
                    }
                }
            }

            bool is_masked = tcc && use_texture && !abe;
            bool is_blended = tcc && use_texture && abe;

            std::vector<NativeVertex> vertices;
            std::vector<unsigned int> indices;

            auto add_vertex = [&](const DecodedVertex& v) {
                // PS2 colors map 128 to 1.0f brightness. We scale appropriately.
                NativeVertex nv;
                nv.x = v.x; nv.y = v.y; nv.z = v.z;
                
                // Add fake directional sun lighting
                // Sun direction from roughly top-right
                float sun_x = 0.5f;
                float sun_y = 0.5f;
                float sun_z = 0.707f;
                
                float dot = (v.nx * sun_x) + (v.ny * sun_y) + (v.nz * sun_z);
                float intensity = dot > 0.0f ? dot : 0.0f;
                
                // Boost ambient by adding directional sun
                float light_multiplier = 1.0f + (intensity * 1.2f);
                
                nv.r = (v.r / 128.0f) * light_multiplier;
                nv.g = (v.g / 128.0f) * light_multiplier;
                nv.b = (v.b / 128.0f) * light_multiplier;
                nv.a = 1.0f; // Force opaque vertex alpha!
                nv.u = v.u; nv.v = v.v;
                vertices.push_back(nv);
                return static_cast<unsigned int>(vertices.size() - 1);
            };

            for (const auto& tri : sm.triangles) {
                indices.push_back(add_vertex(mesh.vertices[tri[0]]));
                indices.push_back(add_vertex(mesh.vertices[tri[1]]));
                indices.push_back(add_vertex(mesh.vertices[tri[2]]));
            }

            for (const auto& quad : sm.quads) {
                unsigned int i0 = add_vertex(mesh.vertices[quad[0]]);
                unsigned int i1 = add_vertex(mesh.vertices[quad[1]]);
                unsigned int i2 = add_vertex(mesh.vertices[quad[2]]);
                unsigned int i3 = add_vertex(mesh.vertices[quad[3]]);
                // Tri 1
                indices.push_back(i0);
                indices.push_back(i1);
                indices.push_back(i2);
                // Tri 2
                indices.push_back(i0);
                indices.push_back(i2);
                indices.push_back(i3);
            }

            if (indices.empty()) continue;

            static_assert(sizeof(NativeVertex) == 9 * sizeof(float), "the merged buffers' vertex layout");
            StaticDrawCall dc;
            dc.first_index = G.append(vertices.data(), vertices.size(), indices.data(), indices.size());
            dc.index_count = static_cast<int>(indices.size());
            world_box_of(vertices.data(), vertices.size(), dc.aabb_min, dc.aabb_max);
            dc.bsphere = mesh.bsphere;
            dc.material_index = sm.material_index;
            dc.use_texture = use_texture;
            dc.is_blended = is_blended;
            dc.is_masked = is_masked;
            dc.tex0_reg = sm.tex0;
            dc.clamp_reg = sm.clamp;
            g_static_tfrag_draw_calls.push_back(dc);
        }
    }
    G.upload();
}

struct TfragRenderInitializer {
    // orig: not ported — static initializer to register tfrag callback
    TfragRenderInitializer() {
        g_tfrag_init_callback = tfrag_init;
        g_tfrag_build_static_cache_callback = tfrag_build_static_cache;
    }
} g_tfrag_render_initializer;

} // namespace rac1
