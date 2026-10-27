// orig: 0x00203338 — tie_render_update
#include <game/render/render_toggles.h>
#include <game/render/tie_render.h>
#include <array>
#include <game/render/render_view.h>
#include <runtime/level/host_layout.h>
#include <runtime/level/level_loader.h>
#include <runtime/kernel/ee_memory.h>
#include <game/render/shader_manager.h>
#include <game/render/world_draw.h>
#include <runtime/gs/gs_renderer.h>
#include <runtime/gs/static_geom_cache.h>
#include <glad/glad.h>
#include <formats/level_core.h>
#include <formats/level_textures.h>
#include <formats/tie_model.h>
#include <formats/scene_wad.h>
#include <game/port_stubs.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <span>
#include <cstring>
#include <bit>
#include <iostream>
#include <algorithm>

namespace rac1 {

extern std::uint32_t g_core_data_vaddr;

struct RacTieInstance {
    std::int32_t o_class;
    std::int32_t draw_distance;
    std::int32_t pad_8;
    std::int32_t occlusion_index;
    glm::mat4 matrix;
    std::uint8_t ambient_rgbas[0x80];
    std::int32_t directional_lights;
    std::int32_t uid;
    std::int32_t pad_58;
    std::int32_t pad_5c;
};

struct TieClassInfo {
    int o_class;
    uint32_t vaddr;
    float near_dist;
    float mid_dist;
    float far_dist;
};

static std::vector<DecodedTieModel> g_tie_classes;
static std::vector<TieClassInfo> g_tie_class_infos;
static std::vector<RacTieInstance> g_tie_instances;
static bool g_tie_initialized = false;


// orig: (host) the tie instances of a gameplay blob (+0x34: count, then 0xe0 bytes each) into the host renderer
static void tie_add_instances(const std::uint8_t* gp_ptr, std::size_t decomp_size) {
    if (decomp_size > 0x3c + 4) {
        std::int32_t tie_instances_offset = *reinterpret_cast<const std::int32_t*>(gp_ptr + 0x34);
        if (tie_instances_offset > 0 && tie_instances_offset + 16 <= static_cast<std::int32_t>(decomp_size)) {
            struct TableHeader {
                std::int32_t count_1;
                std::int32_t pad[3];
            };
            const auto* table_header = reinterpret_cast<const TableHeader*>(gp_ptr + tie_instances_offset);
            std::int32_t count = table_header->count_1;
            
            std::size_t array_start = static_cast<std::size_t>(tie_instances_offset) + 0x10;
            #pragma pack(push, 1)
            struct Vec4f_local {
                float x, y, z, w;
            };
            struct Mat4_local {
                Vec4f_local m_0, m_1, m_2, m_3;
                glm::mat4 unpack() const {
                    glm::mat4 result;
                    result[0] = glm::vec4(m_0.x, m_0.y, m_0.z, m_0.w);
                    result[1] = glm::vec4(m_1.x, m_1.y, m_1.z, m_1.w);
                    result[2] = glm::vec4(m_2.x, m_2.y, m_2.z, m_2.w);
                    result[3] = glm::vec4(m_3.x, m_3.y, m_3.z, m_3.w);
                    return result;
                }
            };
            struct DiskTieInstance {
                std::int32_t o_class;
                std::int32_t draw_distance;
                std::int32_t pad_8;
                std::int32_t occlusion_index;
                Mat4_local matrix;
                std::uint8_t ambient_rgbas[0x80];
                std::int32_t directional_lights;
                std::int32_t uid;
                std::int32_t pad_58;
                std::int32_t pad_5c;
            };
            #pragma pack(pop)
            static_assert(sizeof(DiskTieInstance) == 0xe0);
            
            if (array_start + count * sizeof(DiskTieInstance) <= decomp_size) {
                for (int i = 0; i < count; ++i) {
                    const auto* inst = reinterpret_cast<const DiskTieInstance*>(gp_ptr + array_start + i * sizeof(DiskTieInstance));
                    
                    RacTieInstance rinst;
                    rinst.o_class = inst->o_class;
                    rinst.draw_distance = inst->draw_distance;
                    rinst.pad_8 = inst->pad_8;
                    rinst.occlusion_index = inst->occlusion_index;
                    rinst.matrix = inst->matrix.unpack();
                    rinst.matrix[3][3] = 1.0f; // Ensure standard homogenous matrix
                    std::memcpy(rinst.ambient_rgbas, inst->ambient_rgbas, 0x80);
                    rinst.directional_lights = inst->directional_lights;
                    rinst.uid = inst->uid;
                    rinst.pad_58 = inst->pad_58;
                    rinst.pad_5c = inst->pad_5c;
                    
                    g_tie_instances.push_back(rinst);
                }
            }
        }
    }
}

// Custom level loader initialization callback
static void tie_init(std::span<const std::byte> level_wad_data) {
    g_tie_classes.clear();
    g_tie_class_infos.clear();
    g_tie_instances.clear();
    g_tie_initialized = false;

    try {
        // Read AssetIndexHeader from 0x004d4000
        AssetIndexHeader core_header = ee_read<AssetIndexHeader>(host_layout::CORE);
        
        // 1. Recover TIE Classes
        if (core_header.tie_classes.count > 0 && core_header.tie_classes.offset > 0) {
            uint32_t array_vaddr = host_layout::CORE + core_header.tie_classes.offset;
            for (int i = 0; i < core_header.tie_classes.count; ++i) {
                ClassRecord entry = ee_read<ClassRecord>(array_vaddr + i * sizeof(ClassRecord));
                if (entry.data == 0) continue;
                
                uint32_t class_vaddr = g_core_data_vaddr + entry.data;
                float near_dist = ee_read<float>(class_vaddr + 0x10);
                float mid_dist = ee_read<float>(class_vaddr + 0x14);
                float far_dist = ee_read<float>(class_vaddr + 0x18);
                
                g_tie_class_infos.push_back({ entry.class_id, class_vaddr, near_dist, mid_dist, far_dist });
                
                std::span<const std::byte> class_span(reinterpret_cast<const std::byte*>(ee_ram_ptr(class_vaddr)), host_layout::EE_END - class_vaddr);
                DecodedTieModel model = decode_tie_class_model(entry.class_id, class_span, entry.texture_remap);
                g_tie_classes.push_back(std::move(model));
            }
        }

        // 2. Recover TIE Instances from gameplay data
        if (level_wad_data.size() >= 48) {
            std::int32_t header_size = *reinterpret_cast<const std::int32_t*>(level_wad_data.data());
            if (header_size == 48) {
                std::int32_t gp_offset_sectors = *reinterpret_cast<const std::int32_t*>(level_wad_data.data() + 0x18);
                std::int32_t gp_size_sectors = *reinterpret_cast<const std::int32_t*>(level_wad_data.data() + 0x1c);
                if (gp_size_sectors == 0) {
                    gp_offset_sectors = *reinterpret_cast<const std::int32_t*>(level_wad_data.data() + 0x20);
                    gp_size_sectors = *reinterpret_cast<const std::int32_t*>(level_wad_data.data() + 0x24);
                }
                
                if (gp_size_sectors > 0) {
                    std::size_t gp_offset = static_cast<std::size_t>(gp_offset_sectors) * 2048;
                    std::size_t gp_size = static_cast<std::size_t>(gp_size_sectors) * 2048;
                    if (gp_offset + gp_size <= level_wad_data.size()) {
                        std::span<const std::byte> gp_compressed_span(level_wad_data.data() + gp_offset, gp_size);
                        
                        std::vector<std::uint8_t> gp_decompressed;
                        if (decompress_wad(gp_compressed_span, gp_decompressed)) {
                            const std::uint8_t* gp_ptr = gp_decompressed.data();
                            std::size_t decomp_size = gp_decompressed.size();
                            tie_add_instances(gp_ptr, decomp_size);
                        }
                    }
                }
            }
        }
        
        g_tie_initialized = !g_tie_classes.empty();
        std::cout << "[tie_init] Decoded " << g_tie_classes.size() << " TIE classes, " 
                  << g_tie_instances.size() << " TIE instances." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[tie_init] Failed to initialize TIE classes: " << e.what() << std::endl;
        g_tie_initialized = false;
    }
}

// orig: (host) the tie classes and instances of a scene whose data is in EE memory (the boot
// module's title level: classes at their own addresses, instances in its gameplay blob), the
// loader's callback body for a container that is not a level WAD
void tie_scene_load(std::span<const HostTieClass> classes, std::uint32_t instances_blob) {
    g_tie_classes.clear();
    g_tie_class_infos.clear();
    g_tie_instances.clear();
    g_tie_initialized = false;
    for (const HostTieClass& c : classes) {
        const float near_dist = ee_read<float>(c.vaddr + 0x10), mid_dist = ee_read<float>(c.vaddr + 0x14), far_dist = ee_read<float>(c.vaddr + 0x18);
        g_tie_class_infos.push_back({ c.o_class, c.vaddr, near_dist, mid_dist, far_dist });
        std::span<const std::byte> class_span(reinterpret_cast<const std::byte*>(ee_ram_ptr(c.vaddr)), host_layout::EE_END - c.vaddr);
        g_tie_classes.push_back(decode_tie_class_model(c.o_class, class_span, c.textures));
    }
    tie_add_instances(static_cast<const std::uint8_t*>(ee_ram_ptr(instances_blob)), host_layout::EE_END - instances_blob);
    g_tie_initialized = !g_tie_classes.empty();
    std::cout << "[tie_scene_load] " << g_tie_classes.size() << " TIE classes, " << g_tie_instances.size() << " TIE instances." << std::endl;
}

// orig: 0x00203730
void register_tie_class(std::int32_t o_class, std::span<const std::byte> tie_class_data, std::span<const std::uint8_t, 16> texture_remap) {
    try {
        DecodedTieModel model = decode_tie_class_model(o_class, tie_class_data, texture_remap);
        g_tie_classes.erase(
            std::remove_if(g_tie_classes.begin(), g_tie_classes.end(), 
                           [o_class](const DecodedTieModel& m) { return m.o_class == o_class; }),
            g_tie_classes.end()
        );
        g_tie_classes.push_back(std::move(model));
        g_tie_initialized = true;
    } catch (...) {
        // Safe catch
    }
}

// orig: 0x00203338
void tie_render_update() {
    if (!g_tie_initialized) {
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
    static WorldMaterials materials(host_layout::TEX_TABLE_TIE, 400);
    materials.begin_frame();
    const WorldFrustum frustum(view_proj);
    g_visible_tie_indices.clear();
    for (size_t i = 0; i < g_static_tie_draw_calls.size(); ++i) {
        const auto& c = g_static_tie_draw_calls[i];
        if (frustum.box_outside(c.aabb_min, c.aabb_max)) continue;
        g_visible_tie_indices.push_back(static_cast<int>(i));
        if (c.use_texture) materials.resolve(c.material_index);  // before any binding of the draw
    }

    if (g_static_tie_draw_calls.empty()) {
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_DEPTH_CLAMP);
    auto shader = ShaderManager::get().get_tie_shader();
    glUseProgram(shader);

    GLint vp_loc = glGetUniformLocation(shader, "u_ViewProj");
    glUniformMatrix4fv(vp_loc, 1, GL_FALSE, &view_proj[0][0]);

    GLint model_loc = glGetUniformLocation(shader, "u_Model");
    glm::mat4 model_identity(1.0f);
    glUniformMatrix4fv(model_loc, 1, GL_FALSE, &model_identity[0][0]);

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
    world_draw_runs(g_static_tie_draw_calls, g_visible_tie_indices, g_static_tie_geom.vao, key, apply);
    
    glBindSampler(0, 0);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

// orig: not directly ported — runtime-side static-geometry VBO cache helper (a performance cache)
void tie_build_static_cache() {
    if (!g_tie_initialized) {
        return;
    }

    for (const auto& inst : g_tie_instances) {
        const TieClassInfo* info = nullptr;
        for (const auto& ci : g_tie_class_infos) {
            if (ci.o_class == inst.o_class) {
                info = &ci;
                break;
            }
        }
        if (!info) continue;

        const DecodedTieModel* model = nullptr;
        for (const auto& m : g_tie_classes) {
            if (m.o_class == inst.o_class) {
                model = &m;
                break;
            }
        }
        if (!model) continue;

        glm::vec4 local_center(model->bounding_sphere[0], model->bounding_sphere[1], model->bounding_sphere[2], 1.0f);
        glm::vec4 world_center = inst.matrix * local_center;
        float radius = model->bounding_sphere[3] * model->scale;
        std::array<float, 4> world_bsphere = { world_center.x, world_center.y, world_center.z, radius };

        float max_draw_dist = info->far_dist;
        if (inst.draw_distance > 0 && inst.draw_distance < max_draw_dist) {
            max_draw_dist = inst.draw_distance;
        }

        for (const auto& sm : model->submeshes) {
            bool use_texture = (sm.material_index != -1);
            bool tcc = ((sm.tex0 >> 34) & 1) != 0;
            bool abe = false;
            bool tex_found = false;
            TextureHeader tex_hdr{};
            if (use_texture) {
                uint32_t packed_ptr = ee_read<uint32_t>(host_layout::TEX_TABLE_TIE + sm.material_index * 4);
                if (packed_ptr != 0) {
                    uint32_t pixel_addr = packed_ptr & 0x0FFFFFFF;
                    uint32_t header_addr = pixel_addr - 1024 - 12;
                    if (ee_is_valid_addr(header_addr, 12)) {
                        tex_hdr = ee_read<TextureHeader>(header_addr);
                        abe = (tex_hdr.pad & (1 << 1)) != 0 || (tex_hdr.pad & (1 << 6)) != 0;
                        tex_found = true;
                    }
                }
            }

            if (!tex_found) {
                use_texture = false;
            }

            bool is_masked = tcc && use_texture && !abe;
            bool is_blended = tcc && use_texture && abe;

            uint64_t test_val = 0;
            if (is_masked) {
                test_val = (1 << 0) | (2 << 1) | (0x80ULL << 16);
            } else {
                test_val = (1 << 0) | (1 << 1);
            }

            uint64_t zbuf_val = 0;
            if (is_blended) {
                zbuf_val = 1ULL << 32;
            }

            uint64_t alpha_val = 0;
            if (is_blended) {
                alpha_val = 0x44;
            }

            uint64_t tex0_val = 0;
            if (use_texture) {
                uint32_t w_exp = 0;
                if (tex_hdr.width > 0) w_exp = 0x1e - std::countl_zero(static_cast<uint32_t>(tex_hdr.width));
                uint32_t h_exp = 0;
                if (tex_hdr.height > 0) h_exp = 0x1e - std::countl_zero(static_cast<uint32_t>(tex_hdr.height));
                tex0_val = ((uint64_t)tcc << 34) | ((uint64_t)h_exp << 30) | ((uint64_t)w_exp << 26) | (400 + sm.material_index);
            }

            uint64_t prim_val = 3 | (use_texture ? (1 << 4) : 0) | (is_blended ? (1 << 6) : 0);

            std::vector<NativeVertex> vertices;
            std::vector<unsigned int> indices;

            auto add_vertex = [&](const DecodedVertex& v) {
                glm::vec4 local_pos(v.x, v.y, v.z, 1.0f);
                glm::vec4 world_pos = inst.matrix * local_pos;

                glm::vec3 local_norm(v.nx, v.ny, v.nz);
                glm::vec3 world_norm = glm::mat3(inst.matrix) * local_norm;

                float sun_x = 0.5f;
                float sun_y = 0.5f;
                float sun_z = 0.707f;
                float dot = (world_norm.x * sun_x) + (world_norm.y * sun_y) + (world_norm.z * sun_z);
                float intensity = dot > 0.0f ? dot : 0.0f;
                
                // Scale multiplier down to compensate for flat 255 vertex colors
                float light_multiplier = 0.7f + (intensity * 0.6f);

                NativeVertex gl_v;
                gl_v.x = world_pos.x;
                gl_v.y = world_pos.y;
                gl_v.z = world_pos.z;
                gl_v.r = (v.r / 255.0f) * light_multiplier;
                gl_v.g = (v.g / 255.0f) * light_multiplier;
                gl_v.b = (v.b / 255.0f) * light_multiplier;
                gl_v.a = 1.0f; // Force opaque vertex alpha!
                gl_v.u = v.u;
                gl_v.v = v.v;
                vertices.push_back(gl_v);
                return static_cast<unsigned int>(vertices.size() - 1);
            };

            for (const auto& tri : sm.triangles) {
                indices.push_back(add_vertex(model->vertices[tri[0]]));
                indices.push_back(add_vertex(model->vertices[tri[1]]));
                indices.push_back(add_vertex(model->vertices[tri[2]]));
            }

            if (indices.empty()) continue;

            static_assert(sizeof(NativeVertex) == 9 * sizeof(float), "the merged buffers' vertex layout");
            StaticDrawCall dc;
            dc.first_index = g_static_tie_geom.append(vertices.data(), vertices.size(), indices.data(), indices.size());
            dc.index_count = static_cast<int>(indices.size());
            world_box_of(vertices.data(), vertices.size(), dc.aabb_min, dc.aabb_max);
            dc.bsphere = world_bsphere;
            dc.max_draw_dist = max_draw_dist;
            dc.world_center = glm::vec3(world_center);
            dc.material_index = sm.material_index;
            dc.use_texture = use_texture;
            dc.is_blended = is_blended;
            dc.is_masked = is_masked;
            dc.tex0_reg = tex0_val;
            dc.clamp_reg = sm.clamp;
            dc.test_reg = test_val;
            dc.zbuf_reg = zbuf_val;
            dc.alpha_reg = alpha_val;
            dc.prim_reg = prim_val;

            g_static_tie_draw_calls.push_back(dc);
        }
    }
    g_static_tie_geom.upload();
}

struct TieRenderInitializer {
    // orig: 0x00203730
    TieRenderInitializer() {
        g_tie_init_callback = tie_init;
        g_tie_build_static_cache_callback = tie_build_static_cache;
    }
} g_tie_render_initializer;

} // namespace rac1
