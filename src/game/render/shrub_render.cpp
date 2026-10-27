// orig: 0x00203e00 — shrub_render_update
// orig: 0x00203b08 — register_shrub_class
#include <game/render/render_toggles.h>
#include <game/ovl/ovl_reloc.h>
#include <game/render/shrub_render.h>
#include <array>
#include <game/render/render_view.h>
#include <runtime/level/host_layout.h>
#include <runtime/level/level_loader.h>
#include <runtime/kernel/ee_memory.h>
#include <game/render/shader_manager.h>
#include <game/render/world_draw.h>
#include <runtime/gs/static_geom_cache.h>
#include <glad/glad.h>
#include <formats/level_core.h>
#include <formats/level_textures.h>
#include <formats/shrub_model.h>
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
#include <runtime/gs/gs_renderer.h>
#include <game/render/static_skinned_render.h>

namespace rac1 {

extern std::uint32_t g_core_data_vaddr;

#pragma pack(push, 1)
struct RacShrubInstance {
    std::int32_t o_class;
    float draw_distance;
    std::int32_t unused_8;
    std::int32_t unused_c;
    glm::mat4 matrix;
    float colour[3];
    std::int32_t unused_5c;
    std::int32_t dir_lights;
    std::int32_t unused_64;
    std::int32_t unused_68;
    std::int32_t unused_6c;
};
#pragma pack(pop)
static_assert(sizeof(RacShrubInstance) == 112);

struct ShrubClassInfo {
    int o_class;
    uint32_t vaddr;
};

static std::vector<DecodedShrubModel> g_shrub_classes;
static std::vector<ShrubClassInfo> g_shrub_class_infos;
static std::vector<RacShrubInstance> g_shrub_instances;
static bool g_shrub_initialized = false;


// orig: 0x00203b08
void register_shrub_class(std::int32_t o_class, std::span<const std::byte> shrub_class_data, std::span<const std::uint8_t, 16> texture_remap) {
    // Only register if not already registered
    for (const auto& info : g_shrub_class_infos) {
        if (info.o_class == o_class) return;
    }
    DecodedShrubModel model = decode_shrub_class_model(o_class, shrub_class_data, texture_remap);
    g_shrub_classes.push_back(std::move(model));
    g_shrub_class_infos.push_back({ o_class, 0 });
}

// orig: (host) the shrub instances of a gameplay blob (+0x3c: count, then 0x70 bytes each) into the host renderer
static void shrub_add_instances(const std::uint8_t* gp_ptr, std::size_t decomp_size) {
    if (decomp_size > 0x3c + 4) {
        std::int32_t shrub_instances_offset = *reinterpret_cast<const std::int32_t*>(gp_ptr + 0x3c);
        if (shrub_instances_offset > 0 && shrub_instances_offset + 16 <= static_cast<std::int32_t>(decomp_size)) {
            struct TableHeader {
                std::int32_t count_1;
                std::int32_t pad[3];
            };
            const auto* table_header = reinterpret_cast<const TableHeader*>(gp_ptr + shrub_instances_offset);
            std::int32_t count = table_header->count_1;
            
            std::size_t array_start = static_cast<std::size_t>(shrub_instances_offset) + 0x10;
            
            if (array_start + count * sizeof(RacShrubInstance) <= decomp_size) {
                for (int i = 0; i < count; ++i) {
                    const auto* inst = reinterpret_cast<const RacShrubInstance*>(gp_ptr + array_start + i * sizeof(RacShrubInstance));
                    
                    RacShrubInstance rinst;
                    rinst.o_class = inst->o_class;
                    rinst.draw_distance = inst->draw_distance;
                    rinst.unused_8 = inst->unused_8;
                    rinst.unused_c = inst->unused_c;
                    rinst.matrix = inst->matrix;
                    rinst.matrix[3][3] = 1.0f; // Ensure standard homogenous matrix
                    rinst.colour[0] = inst->colour[0];
                    rinst.colour[1] = inst->colour[1];
                    rinst.colour[2] = inst->colour[2];
                    rinst.unused_5c = inst->unused_5c;
                    rinst.dir_lights = inst->dir_lights;
                    rinst.unused_64 = inst->unused_64;
                    rinst.unused_68 = inst->unused_68;
                    rinst.unused_6c = inst->unused_6c;
                    
                    g_shrub_instances.push_back(rinst);
                }
            }
        }
    }
}

// orig: (host) the shrub classes and instances of a scene whose data is in EE memory (the boot
// module's title level), the loader's callback body for a container that is not a level WAD
void shrub_scene_load(std::span<const HostShrubClass> classes, std::uint32_t instances_blob) {
    g_shrub_classes.clear();
    g_shrub_class_infos.clear();
    g_shrub_instances.clear();
    g_shrub_initialized = false;
    for (const HostShrubClass& c : classes) {
        g_shrub_class_infos.push_back({ c.o_class, c.vaddr });
        std::span<const std::byte> class_span(reinterpret_cast<const std::byte*>(ee_ram_ptr(c.vaddr)), host_layout::EE_END - c.vaddr);
        g_shrub_classes.push_back(decode_shrub_class_model(c.o_class, class_span, c.textures));
    }
    shrub_add_instances(static_cast<const std::uint8_t*>(ee_ram_ptr(instances_blob)), host_layout::EE_END - instances_blob);
    g_shrub_initialized = !g_shrub_classes.empty();
    std::cout << "[shrub_scene_load] " << g_shrub_classes.size() << " Shrub classes, " << g_shrub_instances.size() << " Shrub instances." << std::endl;
}

// Custom level loader initialization callback
static void shrub_init(std::span<const std::byte> level_wad_data) {
    g_shrub_classes.clear();
    g_shrub_class_infos.clear();
    g_shrub_instances.clear();
    g_shrub_initialized = false;

    try {
        // Read AssetIndexHeader from 0x004d4000
        AssetIndexHeader core_header = ee_read<AssetIndexHeader>(host_layout::CORE);
        
        // 1. Recover Shrub Classes
        if (core_header.shrub_classes.count > 0 && core_header.shrub_classes.offset > 0) {
            uint32_t array_vaddr = host_layout::CORE + core_header.shrub_classes.offset;
            for (int i = 0; i < core_header.shrub_classes.count; ++i) {
                ShrubClassRecord entry = ee_read<ShrubClassRecord>(array_vaddr + i * sizeof(ShrubClassRecord));
                if (entry.data == 0) continue;
                
                uint32_t class_vaddr = g_core_data_vaddr + entry.data;
                g_shrub_class_infos.push_back({ entry.class_id, class_vaddr });
                
                const std::byte* class_ram = reinterpret_cast<const std::byte*>(ee_ram_ptr(class_vaddr));
                if (!class_ram) continue;
                
                std::span<const std::byte> class_span(class_ram, host_layout::EE_END - class_vaddr);
                DecodedShrubModel model = decode_shrub_class_model(entry.class_id, class_span, entry.texture_remap);
                g_shrub_classes.push_back(std::move(model));
            }
        }

        // 2. Recover Shrub Instances from gameplay data
        if (level_wad_data.size() >= 48) {
            std::int32_t header_size = *reinterpret_cast<const std::int32_t*>(level_wad_data.data());
            if (header_size == 48 || header_size == 128) {
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
                            shrub_add_instances(gp_ptr, decomp_size);
                        }
                    }
                }
            }
        }
        
        g_shrub_initialized = !g_shrub_classes.empty();
        std::cout << "[shrub_init] Decoded " << g_shrub_classes.size() << " Shrub classes, " 
                  << g_shrub_instances.size() << " Shrub instances." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[shrub_init] Failed to initialize Shrub classes: " << e.what() << std::endl;
        g_shrub_initialized = false;
    }
}

// orig: 0x00203e00
void shrub_render_update() {
    if (!g_shrub_initialized) {
        return;
    }

    glm::mat4 view = render_view_matrix();
    view[0][3] = 0.0f; view[1][3] = 0.0f; view[2][3] = 0.0f; view[3][3] = 1.0f;

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
    static WorldMaterials materials(host_layout::TEX_TABLE_SHRUB, 600);
    materials.begin_frame();
    const WorldFrustum frustum(view_proj);
    g_visible_shrub_indices.clear();
    for (size_t i = 0; i < g_static_shrub_draw_calls.size(); ++i) {
        const auto& c = g_static_shrub_draw_calls[i];
        if (frustum.box_outside(c.aabb_min, c.aabb_max)) continue;
        g_visible_shrub_indices.push_back(static_cast<int>(i));
        if (c.use_texture) materials.resolve(c.material_index);  // before any binding of the draw
    }

    if (g_static_shrub_draw_calls.empty()) {
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_DEPTH_CLAMP);
    auto shader = ShaderManager::get().get_shrub_shader();
    glUseProgram(shader);

    GLint vp_loc = glGetUniformLocation(shader, "u_ViewProj");
    glUniformMatrix4fv(vp_loc, 1, GL_FALSE, &view_proj[0][0]);

    GLint model_loc = glGetUniformLocation(shader, "u_Model");
    glm::mat4 model_identity(1.0f);
    glUniformMatrix4fv(model_loc, 1, GL_FALSE, &model_identity[0][0]);

    GLint alpha_test_loc = glGetUniformLocation(shader, "u_AlphaTest");
    GLint texLoc = glGetUniformLocation(shader, "u_Texture");
    GLint useTexLoc = glGetUniformLocation(shader, "u_UseTexture");
    GLint gsModeLoc = glGetUniformLocation(shader, "u_GsMode");
    if (texLoc != -1) glUniform1i(texLoc, 0);
    // the shrub textures are the level's own uploads: a clear texel is the texture's (no fallback)
    glUniform1i(glGetUniformLocation(shader, "u_TexBound"), 1);
    if (alpha_test_loc != -1) glUniform1i(alpha_test_loc, 0);
    if (gsModeLoc != -1) glUniform1i(gsModeLoc, 0);

    // The GS state of the shrub pass (0x286e60 writes no TEST_1 / ALPHA_1 of its own): the one the
    // frame's packets leave, the fixed packet 0x1c24c0 render_frame (0x1f85b8) REFs before the
    // pass (the frame environment 0x13cf10 holds the same: TEST_1 0x5360b, ALPHA_1 0x44): the alpha
    // test GEQUAL AREF with AFAIL RGB_ONLY, the blend (Cs - Cd) * As + Cd for the primitives with ABE.
    // orig: (host) the GS of the shrub primitives' draw
    const uint64_t test_1 = ee_read<uint64_t>(OV(0x001c24d0));
    MobyGsState gs;
    gs.active = true;
    gs.alpha = 1.0f;
    gs.additive = false;
    gs.aref = static_cast<float>((test_1 >> 4) & 0xff) / 128.0f;

    auto key = [&](const StaticDrawCall& dc) {
        WorldStateKey k;
        k.use_texture = dc.use_texture;
        if (dc.use_texture) {
            k.tex = materials.resolve(dc.material_index);
            k.sampler = gs_clamp_sampler(dc.clamp_reg);
        }
        return k;
    };
    auto apply = [&](const WorldStateKey& k, const WorldStateKey* prev) {
        if (useTexLoc != -1 && (!prev || k.use_texture != prev->use_texture)) glUniform1i(useTexLoc, k.use_texture ? 1 : 0);
        if (!k.use_texture) return;
        if (!prev || !prev->use_texture || k.tex != prev->tex) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, k.tex);
        }
        glBindSampler(0, k.sampler);
    };
    static std::vector<int> opaque_calls, blended_calls;
    opaque_calls.clear();
    blended_calls.clear();
    for (int idx : g_visible_shrub_indices) (g_static_shrub_draw_calls[idx].is_blended ? blended_calls : opaque_calls).push_back(idx);

    // the primitives without ABE: written as they come (the alpha test only gates their depth)
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    world_draw_runs(g_static_shrub_draw_calls, opaque_calls, g_static_shrub_geom.vao, key, apply);
    // the ABE primitives: blended, the pixels passing the alpha test with depth, then the failing ones
    gs_draw_passes(shader, &gs, [&] { world_draw_runs(g_static_shrub_draw_calls, blended_calls, g_static_shrub_geom.vao, key, apply); });

    glBindSampler(0, 0);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

// orig: not ported — runtime VBO cache helper (a performance cache)
void shrub_build_static_cache() {
    if (!g_shrub_initialized) {
        return;
    }

    for (const auto& inst : g_shrub_instances) {
        const DecodedShrubModel* model = nullptr;
        for (const auto& m : g_shrub_classes) {
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

        for (const auto& sm : model->submeshes) {
            bool use_texture = (sm.material_index != -1);
            TextureHeader tex_hdr{};
            if (use_texture) {
                uint32_t packed_ptr = ee_read<uint32_t>(host_layout::TEX_TABLE_SHRUB + sm.material_index * 4);
                uint32_t header_addr = (packed_ptr & 0x0FFFFFFF) - 1024 - 12;
                if (packed_ptr != 0 && ee_is_valid_addr(header_addr, 12)) tex_hdr = ee_read<TextureHeader>(header_addr);
                else use_texture = false;
            }
            // The TEX0 the level setup gives every shrub texture (0x240e80) has TCC set (the
            // texture's alpha used, MODULATE): the primitive's GIF tag PRIM decides the blend (ABE).
            const bool tcc = true;
            const bool is_blended = use_texture && tcc && sm.has_prim && ((sm.prim >> 6) & 1) != 0;
            const bool is_masked = false;

            uint64_t test_val = 0;
            uint64_t zbuf_val = 0;

            uint64_t tex0_val = 0;
            if (use_texture) {
                uint32_t w_exp = 0;
                if (tex_hdr.width > 0) w_exp = 0x1e - std::countl_zero(static_cast<uint32_t>(tex_hdr.width));
                uint32_t h_exp = 0;
                if (tex_hdr.height > 0) h_exp = 0x1e - std::countl_zero(static_cast<uint32_t>(tex_hdr.height));
                tex0_val = ((uint64_t)tcc << 34) | ((uint64_t)h_exp << 30) | ((uint64_t)w_exp << 26) | (600 + sm.material_index);
            }

            uint64_t prim_val = sm.has_prim ? sm.prim : (3 | (use_texture ? (1 << 4) : 0));

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
            dc.first_index = g_static_shrub_geom.append(vertices.data(), vertices.size(), indices.data(), indices.size());
            dc.index_count = static_cast<int>(indices.size());
            world_box_of(vertices.data(), vertices.size(), dc.aabb_min, dc.aabb_max);
            dc.bsphere = world_bsphere;
            dc.max_draw_dist = inst.draw_distance;
            dc.world_center = glm::vec3(world_center);
            dc.material_index = sm.material_index;
            dc.use_texture = use_texture;
            dc.is_blended = is_blended;
            dc.is_masked = is_masked;
            dc.tex0_reg = tex0_val;
            dc.test_reg = test_val;
            dc.zbuf_reg = zbuf_val;
            dc.prim_reg = prim_val;
            dc.clamp_reg = sm.clamp;

            g_static_shrub_draw_calls.push_back(dc);
        }
    }
    g_static_shrub_geom.upload();
}

struct ShrubRenderInitializer {
    // orig: 0x00203b08
    ShrubRenderInitializer() {
        g_shrub_init_callback = shrub_init;
        g_shrub_build_static_cache_callback = shrub_build_static_cache;
    }
} g_shrub_render_initializer;

} // namespace rac1
