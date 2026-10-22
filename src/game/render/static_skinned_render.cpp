#include "static_skinned_render.h"
#include "profiler/host_profiler.h"
#include "shader_manager.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>
#include <iostream>
#include <cstring>
#include <glm/gtc/type_ptr.hpp>

namespace rac1 {

bool load_static_skinned_model(const std::string& glb_path, StaticSkinnedModel& out) {
    tinygltf::Model gltf;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ret = loader.LoadBinaryFromFile(&gltf, &err, &warn, glb_path);
    if (!warn.empty()) std::cout << "GLTF Warning: " << warn << "\n";
    if (!err.empty()) std::cerr << "GLTF Error: " << err << "\n";
    if (!ret) return false;

    // Load images into OpenGL textures
    std::vector<GLuint> loaded_images(gltf.images.size(), 0);
    for (size_t i = 0; i < gltf.images.size(); ++i) {
        const auto& img = gltf.images[i];
        if (img.image.empty() || img.width <= 0 || img.height <= 0) continue;

        GLuint tex_id;
        glGenTextures(1, &tex_id);
        rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &tex_id, "Static skinned (legacy)");
        glBindTexture(GL_TEXTURE_2D, tex_id);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        GLenum format = GL_RGBA;
        if (img.component == 3) format = GL_RGB;
        
        glTexImage2D(GL_TEXTURE_2D, 0, format, img.width, img.height, 0, format, GL_UNSIGNED_BYTE, img.image.data());
        loaded_images[i] = tex_id;
    }

    // Map materials to out.textures array
    for (size_t i = 0; i < gltf.materials.size() && i < 16; ++i) {
        const auto& mat = gltf.materials[i];
        int tex_idx = mat.pbrMetallicRoughness.baseColorTexture.index;
        if (tex_idx >= 0 && tex_idx < static_cast<int>(gltf.textures.size())) {
            int img_idx = gltf.textures[tex_idx].source;
            if (img_idx >= 0 && img_idx < static_cast<int>(loaded_images.size())) {
                out.textures[i] = loaded_images[img_idx];
            }
        }
    }

    // Load geometry
    if (gltf.meshes.empty()) return false;
    const auto& mesh = gltf.meshes[0];

    std::vector<float> vbo_data;
    std::vector<uint32_t> ebo_data;

    auto get_buffer_ptr = [&](int accessor_idx, size_t& count, size_t& stride) -> const unsigned char* {
        if (accessor_idx < 0 || accessor_idx >= static_cast<int>(gltf.accessors.size())) return nullptr;
        const auto& acc = gltf.accessors[accessor_idx];
        const auto& bv = gltf.bufferViews[acc.bufferView];
        const auto& buf = gltf.buffers[bv.buffer];
        count = acc.count;
        stride = acc.ByteStride(bv);
        return buf.data.data() + bv.byteOffset + acc.byteOffset;
    };

    int vertex_offset = 0;

    for (const auto& prim : mesh.primitives) {
        if (prim.attributes.find("POSITION") == prim.attributes.end()) continue;
        
        size_t count = 0, stride = 0;
        const unsigned char* pos_ptr = get_buffer_ptr(prim.attributes.at("POSITION"), count, stride);
        
        size_t norm_count = 0, norm_stride = 0;
        const unsigned char* norm_ptr = nullptr;
        if (prim.attributes.count("NORMAL")) norm_ptr = get_buffer_ptr(prim.attributes.at("NORMAL"), norm_count, norm_stride);
        
        size_t uv_count = 0, uv_stride = 0;
        const unsigned char* uv_ptr = nullptr;
        if (prim.attributes.count("TEXCOORD_0")) uv_ptr = get_buffer_ptr(prim.attributes.at("TEXCOORD_0"), uv_count, uv_stride);
        
        size_t joint_count = 0, joint_stride = 0;
        const unsigned char* joint_ptr = nullptr;
        if (prim.attributes.count("JOINTS_0")) joint_ptr = get_buffer_ptr(prim.attributes.at("JOINTS_0"), joint_count, joint_stride);
        
        size_t weight_count = 0, weight_stride = 0;
        const unsigned char* weight_ptr = nullptr;
        if (prim.attributes.count("WEIGHTS_0")) weight_ptr = get_buffer_ptr(prim.attributes.at("WEIGHTS_0"), weight_count, weight_stride);

        int mat_idx = prim.material;
        float material_idx_f = static_cast<float>(mat_idx);

        for (size_t i = 0; i < count; ++i) {
            const float* p = reinterpret_cast<const float*>(pos_ptr + i * stride);
            vbo_data.push_back(p[0]); vbo_data.push_back(p[1]); vbo_data.push_back(p[2]);
            
            if (norm_ptr) {
                const float* n = reinterpret_cast<const float*>(norm_ptr + i * norm_stride);
                vbo_data.push_back(n[0]); vbo_data.push_back(n[1]); vbo_data.push_back(n[2]);
            } else {
                vbo_data.push_back(0); vbo_data.push_back(0); vbo_data.push_back(1);
            }
            
            if (uv_ptr) {
                const float* u = reinterpret_cast<const float*>(uv_ptr + i * uv_stride);
                vbo_data.push_back(u[0]); vbo_data.push_back(u[1]);
            } else {
                vbo_data.push_back(0); vbo_data.push_back(0);
            }

            if (joint_ptr) {
                const uint16_t* j = reinterpret_cast<const uint16_t*>(joint_ptr + i * joint_stride);
                vbo_data.push_back(static_cast<float>(j[0])); vbo_data.push_back(static_cast<float>(j[1]));
                vbo_data.push_back(static_cast<float>(j[2])); vbo_data.push_back(static_cast<float>(j[3]));
            } else {
                vbo_data.push_back(0); vbo_data.push_back(0); vbo_data.push_back(0); vbo_data.push_back(0);
            }

            if (weight_ptr) {
                const float* w = reinterpret_cast<const float*>(weight_ptr + i * weight_stride);
                vbo_data.push_back(w[0]); vbo_data.push_back(w[1]);
                vbo_data.push_back(w[2]); vbo_data.push_back(w[3]);
            } else {
                vbo_data.push_back(1.0f); vbo_data.push_back(0.0f); vbo_data.push_back(0.0f); vbo_data.push_back(0.0f);
            }
            
            vbo_data.push_back(material_idx_f);
        }

        size_t idx_count = 0, idx_stride = 0;
        const unsigned char* idx_ptr = get_buffer_ptr(prim.indices, idx_count, idx_stride);
        if (idx_ptr) {
            for (size_t i = 0; i < idx_count; ++i) {
                uint32_t index = 0;
                if (gltf.accessors[prim.indices].componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    index = *reinterpret_cast<const uint16_t*>(idx_ptr + i * idx_stride);
                } else if (gltf.accessors[prim.indices].componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    index = *reinterpret_cast<const uint32_t*>(idx_ptr + i * idx_stride);
                }
                ebo_data.push_back(vertex_offset + index);
            }
        }
        
        vertex_offset += count;
    }

    out.index_count = ebo_data.size();

    glGenVertexArrays(1, &out.vao);
    glGenBuffers(1, &out.vbo);
    rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &out.vbo, "Static skinned (legacy)");
    glGenBuffers(1, &out.ebo);
    rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &out.ebo, "Static skinned (legacy)");

    glBindVertexArray(out.vao);
    
    glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
    glBufferData(GL_ARRAY_BUFFER, vbo_data.size() * sizeof(float), vbo_data.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ebo_data.size() * sizeof(uint32_t), ebo_data.data(), GL_STATIC_DRAW);

    // Layout: pos(3) + norm(3) + uv(2) + joints(4) + weights(4) + mat_idx(1) = 17 floats
    size_t stride = 17 * sizeof(float);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
    
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)(12 * sizeof(float)));
    
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride, (void*)(16 * sizeof(float)));

    glBindVertexArray(0);

    // Load skins
    if (!gltf.skins.empty()) {
        const auto& skin = gltf.skins[0];
        out.joint_count = skin.joints.size();
        
        size_t ibm_count = 0, ibm_stride = 0;
        const unsigned char* ibm_ptr = get_buffer_ptr(skin.inverseBindMatrices, ibm_count, ibm_stride);
        if (ibm_ptr) {
            out.inverse_bind_matrices.resize(out.joint_count);
            for (int i = 0; i < out.joint_count && i < static_cast<int>(ibm_count); ++i) {
                const float* m = reinterpret_cast<const float*>(ibm_ptr + i * ibm_stride);
                out.inverse_bind_matrices[i] = glm::make_mat4(m);
            }
        }
    }

    return true;
}

void render_static_skinned_model(const StaticSkinnedModel& m, const glm::mat4& model_xform,
                                 const glm::mat4& view_proj, const std::vector<glm::mat4>& joint_matrices,
                                 const MobyGsState* gs) {
    GLuint shader = ShaderManager::get().get_static_skinned_shader();
    if (!shader) {
        std::cerr << "static_skinned shader not found!\n";
        return;
    }
    glUseProgram(shader);

    glUniformMatrix4fv(glGetUniformLocation(shader, "u_ViewProj"), 1, GL_FALSE, glm::value_ptr(view_proj));
    glUniformMatrix4fv(glGetUniformLocation(shader, "u_Model"), 1, GL_FALSE, glm::value_ptr(model_xform));

    GLint joints_loc = glGetUniformLocation(shader, "u_Joints");
    if (joints_loc != -1 && !joint_matrices.empty()) {
        int count = std::min(m.joint_count, static_cast<int>(joint_matrices.size()));
        count = std::min(count, 128); // assuming uniform array size is at least 128
        // The caller passes GLOBAL joint poses; apply the GLB's own
        // inverse-bind matrices here to form the standard glTF skinning
        // matrix u_Joints[j] = global_pose[j] * inverseBind[j]. This uses the
        // GLB's bind frame (export-time scale/coordinate transform) rather
        // than a mismatched one computed elsewhere.
        std::vector<glm::mat4> skin(count);
        for (int j = 0; j < count; ++j) {
            if (j < static_cast<int>(m.inverse_bind_matrices.size())) {
                skin[j] = joint_matrices[j] * m.inverse_bind_matrices[j];
            } else {
                skin[j] = joint_matrices[j];
            }
        }
        glUniformMatrix4fv(joints_loc, count, GL_FALSE, glm::value_ptr(skin[0]));
    }

    // Bind textures using samplers array
    GLint tex_indices[16];
    for (int i = 0; i < 16; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, m.textures[i]);
        tex_indices[i] = i;
    }
    glUniform1iv(glGetUniformLocation(shader, "u_Textures"), 16, tex_indices);

    glBindVertexArray(m.vao);
    gs_draw_passes(shader, gs, [&] { glDrawElements(GL_TRIANGLES, m.index_count, GL_UNSIGNED_INT, 0); });
    glBindVertexArray(0);
}

} // namespace rac1
