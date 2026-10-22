// The host's GL draw of moby class models (orig_moby_draw.cpp draws the game's mobys with it
// until the mobys go through their ported VU1 pass, ADR 0007): the class models decoded at level
// load, the skinned models exported from the ISO (setup.sh), the GL draw.
#include <game/render/render_toggles.h>
#include <profiler/host_profiler.h>
#include <game/render/moby_render.h>
#include <game/render/render_view.h>
#include <game/render/shader_manager.h>
#include <game/render/static_skinned_render.h>
#include <runtime/level/host_layout.h>
#include <runtime/level/level_loader.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/gs/gs_renderer.h>
#include <formats/moby_model.h>
#include <formats/scene_wad.h>
#include <formats/level_textures.h>
#include <formats/level_mesh.h>
#include <formats/animation.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glad/glad.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace rac1 {

static std::unordered_map<int16_t, DecodedMobyModel> g_moby_models;

struct MobyVertex {
    float x, y, z;
    float u, v;
    uint8_t r, g, b, a;
    float aJointIdx;
};

struct MobyVBOData {
    uint32_t vao = 0;
    uint32_t vbo = 0;
    uint32_t ebo = 0;
    size_t index_count = 0;
    int material_index = -1;
};

struct CachedMobyModel {
    std::vector<MobyVBOData> submeshes;
    bool initialized = false;
};

static std::unordered_map<int16_t, CachedMobyModel> g_moby_vbo_cache;

// Callback called during level load
static void moby_init_callback(std::span<const std::byte> level_wad_data) {
    g_moby_models.clear();

    if (level_wad_data.size() < sizeof(LevelFileHeader)) return;
    
    std::span<const std::byte> level_data = level_wad_data;
    if (level_wad_data.size() >= 48) {
        std::int32_t header_size = *reinterpret_cast<const std::int32_t*>(level_wad_data.data());
        if (header_size == 48) {
            std::int32_t data_sector_offset = *reinterpret_cast<const std::int32_t*>(level_wad_data.data() + 0x10);
            std::int32_t data_sector_size = *reinterpret_cast<const std::int32_t*>(level_wad_data.data() + 0x14);
            std::size_t byte_offset = static_cast<std::size_t>(data_sector_offset) * 2048;
            std::size_t byte_size = static_cast<std::size_t>(data_sector_size) * 2048;
            if (byte_offset + byte_size <= level_wad_data.size()) {
                level_data = level_wad_data.subspan(byte_offset, byte_size);
            }
        }
    }

    if (level_data.size() < sizeof(LevelFileHeader)) return;
    
    LevelFileHeader wad_header;
    std::memcpy(&wad_header, level_data.data(), sizeof(LevelFileHeader));

    std::span<const std::byte> core_index_span(
        level_data.data() + wad_header.asset_index.offset, 
        wad_header.asset_index.size
    );
    
    std::span<const std::byte> core_data_span(
        level_data.data() + wad_header.asset_data.offset, 
        wad_header.asset_data.size
    );

    std::vector<std::uint8_t> decompressed_core_data;
    if (!decompress_wad(core_data_span, decompressed_core_data)) {
        return;
    }

    if (core_index_span.size() < sizeof(AssetIndexHeader)) return;
    
    AssetIndexHeader core_header;
    std::memcpy(&core_header, core_index_span.data(), sizeof(AssetIndexHeader));

    std::vector<std::int64_t> block_bounds;
    block_bounds.push_back(core_header.tfrag_block);
    block_bounds.push_back(core_header.occlusion_block);
    block_bounds.push_back(core_header.sky_block);
    block_bounds.push_back(core_header.collision_block);
    block_bounds.push_back(core_header.texel_block);
    block_bounds.push_back(core_header.unpacked_size);

    auto get_array_range_bounds = [&](TableRef r, std::size_t entry_size) {
        if (r.count <= 0 || r.offset < 0) return;
        for (int i = 0; i < r.count; ++i) {
            std::size_t entry_offset = static_cast<std::size_t>(r.offset) + i * entry_size;
            if (entry_offset + 4 <= core_index_span.size()) {
                std::int32_t offset_in_asset_wad = *reinterpret_cast<const std::int32_t*>(core_index_span.data() + entry_offset);
                if (offset_in_asset_wad > 0) {
                    block_bounds.push_back(offset_in_asset_wad);
                }
            }
        }
    };
    get_array_range_bounds(core_header.moby_classes, sizeof(ClassRecord));
    get_array_range_bounds(core_header.tie_classes, sizeof(ClassRecord));
    get_array_range_bounds(core_header.shrub_classes, sizeof(ShrubClassRecord));

    // Register gadget asset bounds in core_index
    if (core_header.gadget_count > 0 && core_header.gadget_table > 0) {
        std::size_t array_start = static_cast<std::size_t>(core_header.gadget_table);
        std::size_t entry_size = sizeof(GadgetRecord);
        if (array_start + core_header.gadget_count * entry_size <= core_index_span.size()) {
            for (int i = 0; i < core_header.gadget_count; ++i) {
                const auto* entry = reinterpret_cast<const GadgetRecord*>(core_index_span.data() + array_start + i * entry_size);
                if (entry->data > 0) {
                    block_bounds.push_back(entry->data);
                }
            }
        }
    }
    
    std::sort(block_bounds.begin(), block_bounds.end());
    block_bounds.erase(std::unique(block_bounds.begin(), block_bounds.end()), block_bounds.end());

    auto get_block_size = [&](std::int32_t offset) -> std::size_t {
        if (offset < 0) return 0;
        auto it = std::upper_bound(block_bounds.begin(), block_bounds.end(), offset);
        if (it != block_bounds.end()) {
            return static_cast<std::size_t>(*it - offset);
        }
        return decompressed_core_data.size() - offset;
    };

    if (core_header.moby_classes.count > 0 && core_header.moby_classes.offset >= 0) {
        std::size_t array_start = static_cast<std::size_t>(core_header.moby_classes.offset);
        std::size_t entry_size = sizeof(ClassRecord);
        if (array_start + core_header.moby_classes.count * entry_size <= core_index_span.size()) {
            std::vector<ClassRecord> moby_entries(core_header.moby_classes.count);
            std::memcpy(moby_entries.data(), core_index_span.data() + array_start, core_header.moby_classes.count * entry_size);
            
            for (const auto& entry : moby_entries) {
                if (entry.data <= 0) continue;
                std::size_t asset_size = get_block_size(entry.data);
                if (entry.data + asset_size <= decompressed_core_data.size()) {
                    std::span<const std::byte> asset_span(
                        reinterpret_cast<const std::byte*>(decompressed_core_data.data() + entry.data),
                        asset_size
                    );
                    
                    try {
                        DecodedMobyModel model = decode_moby_class_model(entry.class_id, asset_span, entry.texture_remap);
                        g_moby_models[entry.class_id] = std::move(model);
                    } catch (const std::exception&) {
                        // Suppress trace errors for geometryless mobys
                    }
                }
            }
        }
    }

    // Decode and register gadgets (e.g. Class 71 Omniwrench)
    if (core_header.gadget_count > 0 && core_header.gadget_table > 0) {
        std::size_t array_start = static_cast<std::size_t>(core_header.gadget_table);
        std::size_t entry_size = sizeof(GadgetRecord);
        if (array_start + core_header.gadget_count * entry_size <= core_index_span.size()) {
            std::vector<GadgetRecord> gadget_entries(core_header.gadget_count);
            std::memcpy(gadget_entries.data(), core_index_span.data() + array_start, core_header.gadget_count * entry_size);
            
            for (const auto& entry : gadget_entries) {
                if (entry.data <= 0 || entry.packed_size <= 0) continue;
                if (entry.data + entry.packed_size <= static_cast<std::int32_t>(decompressed_core_data.size())) {
                    std::span<const std::byte> comp_span(
                        reinterpret_cast<const std::byte*>(decompressed_core_data.data() + entry.data),
                        entry.packed_size
                    );
                    std::vector<uint8_t> decomp;
                    if (decompress_wad(comp_span, decomp)) {
                        std::span<const std::byte> asset_span(
                            reinterpret_cast<const std::byte*>(decomp.data()),
                            decomp.size()
                        );
                        try {
                            // the gadget's textures: its entry in the level's moby class table (offset 0,
                            // the class streamed in; its +0x10 list is the one the game's item class load
                            // 0x245770 relocates the class with, 0x1afce0)
                            std::array<std::uint8_t, 16> textures{};
                            if (core_header.moby_classes.count > 0 && core_header.moby_classes.offset >= 0) {
                                const std::size_t mstart = static_cast<std::size_t>(core_header.moby_classes.offset);
                                for (int k = 0; k < core_header.moby_classes.count; ++k) {
                                    const std::size_t at = mstart + static_cast<std::size_t>(k) * sizeof(ClassRecord);
                                    if (at + sizeof(ClassRecord) > core_index_span.size()) break;
                                    ClassRecord me;
                                    std::memcpy(&me, core_index_span.data() + at, sizeof(me));
                                    if (me.class_id != entry.class_id) continue;
                                    for (std::size_t t = 0; t < textures.size(); ++t) textures[t] = me.texture_remap[t];
                                    break;
                                }
                            }
                            DecodedMobyModel model = decode_moby_class_model(entry.class_id, asset_span, textures);
                            g_moby_models[entry.class_id] = std::move(model);
                        } catch (const std::exception&) {
                            // Suppress trace errors for geometryless mobys
                        }
                    }
                }
            }
        }
    }
    std::cout << "[moby_init_callback] Loaded " << g_moby_models.size() << " Moby models." << std::endl;
}

static StaticSkinnedModel g_ratchet_glb;
static bool g_ratchet_glb_loaded = false;

static StaticSkinnedModel& get_ratchet_glb() {
    if (!g_ratchet_glb_loaded) {
        g_ratchet_glb_loaded = true;
        if (!load_static_skinned_model("assets/generated/ratchet_class0.glb", g_ratchet_glb)) {
            std::cerr << "Failed to load assets/generated/ratchet_class0.glb (run setup.sh)" << std::endl;
        } else {
            std::cout << "Loaded ratchet_class0.glb!" << std::endl;
        }
    }
    return g_ratchet_glb;
}

std::string g_moby_glb_dir;

// The skinned model of a level class (bind pose and joints), exported from the ISO by
// setup.sh into g_moby_glb_dir/<class>.glb; null when there is none.
// orig: (host) the class model with its skin
static std::unordered_map<int16_t, std::unique_ptr<StaticSkinnedModel>> g_class_glb_cache;
static StaticSkinnedModel* get_class_glb(int16_t cls) {
    auto& cache = g_class_glb_cache;
    auto it = cache.find(cls);
    if (it == cache.end()) {
        std::unique_ptr<StaticSkinnedModel> m;
        const std::string path = g_moby_glb_dir + "/" + std::to_string(cls) + ".glb";
        if (!g_moby_glb_dir.empty() && std::filesystem::exists(path)) {
            m = std::make_unique<StaticSkinnedModel>();
            if (!load_static_skinned_model(path, *m)) m.reset();
        }
        it = cache.emplace(cls, std::move(m)).first;
    }
    return it->second.get();
}

// The class models drawn so far dropped: another level's classes come next.
// orig: (host) the level's class models released
void moby_models_release() {
    for (auto& [cls, m] : g_class_glb_cache) {
        if (!m) continue;
        glDeleteVertexArrays(1, &m->vao);
        glDeleteBuffers(1, &m->vbo);
        glDeleteBuffers(1, &m->ebo);
        for (GLuint t : m->textures)
            if (t != 0) glDeleteTextures(1, &t);
    }
    g_class_glb_cache.clear();
    for (auto& [cls, c] : g_moby_vbo_cache)
        for (auto& sm : c.submeshes) {
            glDeleteVertexArrays(1, &sm.vao);
            glDeleteBuffers(1, &sm.vbo);
            glDeleteBuffers(1, &sm.ebo);
        }
    g_moby_vbo_cache.clear();
}

// Draws one moby class model with a model matrix and joint matrices (the GL draw; the legacy
// host path and the original-moby drawer share it).
// orig: (host) the GL draw of a moby model
void moby_draw_class(int16_t cls, const glm::mat4& matrix, const std::vector<glm::mat4>& joint_matrices, bool has_joints,
                     const MobyGsState* gs) {
    auto it = g_moby_models.find(cls);
    if (it == g_moby_models.end()) return;
    const auto& model = it->second;
    if (model.vertices.empty()) return;
    StaticSkinnedModel* skinned = nullptr;
    if (cls == 0) {
        skinned = &get_ratchet_glb();
    } else if (has_joints) {
        skinned = get_class_glb(cls);
    }
    if (skinned != nullptr) {
        StaticSkinnedModel& glb_model = *skinned;
        if (glb_model.vao != 0) {
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
            custom_proj[2][3] = -1.0f;

            glm::mat4 view_proj = custom_proj * view;

            render_static_skinned_model(glb_model, matrix, view_proj, joint_matrices, gs);
            return;
        }
    }


    // Initialize static VBO cache
    auto& cache = g_moby_vbo_cache[cls];
    if (!cache.initialized) {
        cache.initialized = true;

        std::vector<MobyVertex> vertices;
        vertices.reserve(model.vertices.size());
        for (const auto& v : model.vertices) {
            MobyVertex mv;
            mv.x = v.x; mv.y = v.y; mv.z = v.z;
            mv.u = v.u; mv.v = v.v;

            float sun_x = 0.5f;
            float sun_y = 0.5f;
            float sun_z = 0.707f;
            float dot = (v.nx * sun_x) + (v.ny * sun_y) + (v.nz * sun_z);
            float intensity = dot > 0.0f ? dot : 0.0f;
            
            // Scale multiplier down to compensate for flat 255 vertex colors
            float light_multiplier = 0.7f + (intensity * 0.6f);
            
            // Moby uses uint8_t, so we keep it in 0-255 range.
            // (v.r / 255.0) * light_multiplier * 255.0 = v.r * light_multiplier
            mv.r = static_cast<uint8_t>(std::clamp(v.r * light_multiplier, 0.0f, 255.0f));
            mv.g = static_cast<uint8_t>(std::clamp(v.g * light_multiplier, 0.0f, 255.0f));
            mv.b = static_cast<uint8_t>(std::clamp(v.b * light_multiplier, 0.0f, 255.0f));
            mv.a = 255;
            // Clamp joint_idx to actually-uploaded range. Vertices with
            // joint_idx >= the number of matrices uploaded would sample
            // UNINITIALIZED entries in u_Joints[128] in the shader and
            // distort that vertex. Cap at min(128, joint_count).
            int max_joint = std::min(128, has_joints ? (int)joint_matrices.size() : 128);
            int ji = (v.joint_idx >= 0 && v.joint_idx < max_joint) ? v.joint_idx : 0;
            mv.aJointIdx = static_cast<float>(ji);
            vertices.push_back(mv);
        }

        uint32_t shared_vbo = 0;
        if (!vertices.empty()) {
            glGenBuffers(1, &shared_vbo);
            rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &shared_vbo, "Mobys (legacy drawer)");
            glBindBuffer(GL_ARRAY_BUFFER, shared_vbo);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(MobyVertex), vertices.data(), GL_STATIC_DRAW);
        }

        for (const auto& sm : model.submeshes) {
            MobyVBOData vbo_data;
            vbo_data.material_index = sm.material_index;
            vbo_data.vbo = shared_vbo;

            std::vector<uint32_t> indices;
            indices.reserve(sm.triangles.size() * 3);
            for (const auto& tri : sm.triangles) {
                indices.push_back(tri[0]);
                indices.push_back(tri[1]);
                indices.push_back(tri[2]);
            }

            if (!indices.empty() && shared_vbo != 0) {
                glGenVertexArrays(1, &vbo_data.vao);
                glGenBuffers(1, &vbo_data.ebo);
                rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &vbo_data.ebo, "Mobys (legacy drawer)");

                glBindVertexArray(vbo_data.vao);
                glBindBuffer(GL_ARRAY_BUFFER, shared_vbo);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_data.ebo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MobyVertex), (void*)offsetof(MobyVertex, x));

                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(MobyVertex), (void*)offsetof(MobyVertex, r));

                glEnableVertexAttribArray(2);
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MobyVertex), (void*)offsetof(MobyVertex, u));

                glEnableVertexAttribArray(3);
                glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(MobyVertex), (void*)offsetof(MobyVertex, aJointIdx));

                glBindVertexArray(0);
                
                vbo_data.index_count = indices.size();
                cache.submeshes.push_back(vbo_data);
            }
        }
    }

    glm::mat4 view = render_view_matrix();
    // No row-2 flip — PS2 row 2 = look_back gives view_z negative for
    // objects in front, matching OpenGL standard (same as tfrag_render).

    glm::mat4 custom_proj(0.0f);
    {
        float fov = glm::radians(60.0f);
        float aspect = g_render_aspect;
        float n = 0.1f;
        float f = 4000.0f;
        custom_proj[0][0] = render_proj_x(aspect, fov);
        custom_proj[1][1] = render_proj_y(fov);
        custom_proj[2][2] = -(f + n) / (f - n);
        custom_proj[3][2] = -(2.0f * f * n) / (f - n);
        custom_proj[2][3] = -1.0f;
    }
    glm::mat4 view_proj = custom_proj * view;

    glm::vec3 cam_pos_for_cull = render_camera_position();

    uint32_t shader = ShaderManager::get().get_moby_shader();
    glUseProgram(shader);

    int loc_vp = glGetUniformLocation(shader, "u_ViewProj");
    if (loc_vp != -1) {
        glUniformMatrix4fv(loc_vp, 1, GL_FALSE, glm::value_ptr(view_proj));
    }

    int loc_model = glGetUniformLocation(shader, "u_Model");
    if (loc_model != -1) {
        glUniformMatrix4fv(loc_model, 1, GL_FALSE, glm::value_ptr(matrix));
    }

    int loc_has_joints = glGetUniformLocation(shader, "u_HasJoints");
    if (loc_has_joints != -1) {
        glUniform1i(loc_has_joints, has_joints ? 1 : 0);
    }

    int loc_joints = glGetUniformLocation(shader, "u_Joints");
    if (loc_joints != -1 && has_joints) {
        // The joint matrices are the joints' poses (the game's pose evaluation 0x254108); the
        // class's vertices are in its bind pose, so each pose goes with its joint's inverse bind
        // matrix from the class's skeleton (translation in class units, scaled by scale / 1024
        // as the poses are) - the skinning the exported GLB path does with the same matrices.
        // Without the skinned GLB (setup.sh not rerun since the class was added)
        // this path is the only one: one joint per vertex, no blending.
        static std::unordered_map<int16_t, bool> warned;
        if (!warned[cls]) {
            warned[cls] = true;
            std::fprintf(stderr, "[moby] class %d: no skinned GLB in %s, drawn with one joint per vertex (rerun setup.sh)\n", cls, g_moby_glb_dir.c_str());
        }
        const int count = std::min(128, (int)joint_matrices.size());
        std::vector<glm::mat4> skin(joint_matrices.begin(), joint_matrices.begin() + count);
        const auto& skel = model.anims.skeleton;
        for (int j = 0; j < count && j < static_cast<int>(skel.size()); ++j) {
            const auto& m = skel[j].m;
            glm::mat4 ibm(m[0][0], m[0][1], m[0][2], 0.0f, m[1][0], m[1][1], m[1][2], 0.0f, m[2][0], m[2][1], m[2][2], 0.0f,
                          m[3][0] * model.scale / 1024.0f, m[3][1] * model.scale / 1024.0f, m[3][2] * model.scale / 1024.0f, 1.0f);
            skin[j] = joint_matrices[j] * ibm;
        }
        glUniformMatrix4fv(loc_joints, count, GL_FALSE, glm::value_ptr(skin[0]));
    }

    int sm_idx = -1;
    for (const auto& sm : cache.submeshes) {
        sm_idx++;
        if (sm.material_index == -1) {
            // Submeshes without material (-1) are invisible collision/occlusion geometry in PS2 mobys
            continue;
        }
        bool use_texture = true;
        bool abe = false;

        if (use_texture) {
            uint32_t packed_ptr = ee_read<uint32_t>(host_layout::TEX_TABLE_MOBY + sm.material_index * 4);
            if (packed_ptr != 0) {
                uint32_t pixel_addr = packed_ptr & 0x0FFFFFFF;
                uint32_t header_addr = pixel_addr - 1024 - 12;
                if (ee_is_valid_addr(header_addr, 12)) {
                    TextureHeader tex_hdr = ee_read<TextureHeader>(header_addr);
                    abe = (tex_hdr.pad & (1 << 1)) != 0 || (tex_hdr.pad & (1 << 6)) != 0;
                    
                    gs_bind_texture_vaddr(200 + sm.material_index, pixel_addr);
                    unsigned int tex_id = get_or_upload_texture(200 + sm.material_index);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, tex_id);
                    GLint texLoc = glGetUniformLocation(shader, "u_Texture");
                    GLint useTexLoc = glGetUniformLocation(shader, "u_UseTexture");
                    if (texLoc != -1) glUniform1i(texLoc, 0);
                    if (useTexLoc != -1) glUniform1i(useTexLoc, 1);
                } else {
                    GLint useTexLoc = glGetUniformLocation(shader, "u_UseTexture");
                    if (useTexLoc != -1) glUniform1i(useTexLoc, 0);
                }
            } else {
                GLint useTexLoc = glGetUniformLocation(shader, "u_UseTexture");
                if (useTexLoc != -1) glUniform1i(useTexLoc, 0);
            }
        } else {
            GLint useTexLoc = glGetUniformLocation(shader, "u_UseTexture");
            if (useTexLoc != -1) glUniform1i(useTexLoc, 0);
        }
        
        glDepthMask(GL_TRUE);
        if (abe) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glDisable(GL_BLEND);
        }

        GLint alphaTestLoc = glGetUniformLocation(shader, "u_AlphaTest");
        if (alphaTestLoc != -1) {
            glUniform1i(alphaTestLoc, (use_texture && !abe) ? 1 : 0);
        }
        
        glEnable(GL_DEPTH_CLAMP);

        glBindVertexArray(sm.vao);
        glDisable(GL_CULL_FACE);
        gs_draw_passes(shader, gs, [&] { glDrawElements(GL_TRIANGLES, sm.index_count, GL_UNSIGNED_INT, 0); });
        glBindVertexArray(0);
    }

    glDisable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

struct MobyRenderInitializer {
    MobyRenderInitializer() { g_moby_init_callback = moby_init_callback; }
} g_moby_render_initializer;

} // namespace rac1
