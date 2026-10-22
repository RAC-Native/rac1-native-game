#include <runtime/gs/static_geom_cache.h>
#include <profiler/host_profiler.h>
#include <glad/glad.h>

namespace rac1 {

bool g_use_gpu_rendering = false;

std::vector<StaticDrawCall> g_static_tfrag_draw_calls;
std::vector<StaticDrawCall> g_static_tie_draw_calls;
std::vector<StaticDrawCall> g_static_shrub_draw_calls;

StaticMergedGeom g_static_tfrag_geom;
StaticMergedGeom g_static_tie_geom;
StaticMergedGeom g_static_shrub_geom;

std::vector<int> g_visible_tfrag_indices;
std::vector<int> g_visible_tie_indices;
std::vector<int> g_visible_shrub_indices;

void (*g_tfrag_build_static_cache_callback)() = nullptr;
void (*g_tie_build_static_cache_callback)() = nullptr;
void (*g_shrub_build_static_cache_callback)() = nullptr;

unsigned int StaticMergedGeom::append(const void* vertices, std::size_t nv, const unsigned int* indices, std::size_t ni) {
    const unsigned int base = base_vertex_count();
    const unsigned int first = static_cast<unsigned int>(idx.size());
    const float* v = static_cast<const float*>(vertices);
    vtx.insert(vtx.end(), v, v + nv * 9);
    for (std::size_t i = 0; i < ni; ++i) idx.push_back(base + indices[i]);
    return first;
}

void StaticMergedGeom::upload() {
    if (vao == 0) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &vbo, "GS static geometry cache");
        glGenBuffers(1, &ebo);
        rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &ebo, "GS static geometry cache");
    }
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vtx.size() * sizeof(float)), vtx.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(idx.size() * sizeof(unsigned int)), idx.data(), GL_STATIC_DRAW);
    const GLsizei stride = 9 * sizeof(float);  // NativeVertex
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(7 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    vtx.clear();
    vtx.shrink_to_fit();
    idx.clear();
    idx.shrink_to_fit();
}

void StaticMergedGeom::release() {
    if (vao != 0) glDeleteVertexArrays(1, &vao);
    if (vbo != 0) glDeleteBuffers(1, &vbo);
    if (ebo != 0) glDeleteBuffers(1, &ebo);
    vao = vbo = ebo = 0;
    vtx.clear();
    idx.clear();
}

void static_geom_cache_init() {
    static_geom_cache_clear();

    if (g_tfrag_build_static_cache_callback) g_tfrag_build_static_cache_callback();
    if (g_tie_build_static_cache_callback) g_tie_build_static_cache_callback();
    if (g_shrub_build_static_cache_callback) g_shrub_build_static_cache_callback();
}

void static_geom_cache_clear() {
    g_static_tfrag_draw_calls.clear();
    g_static_tie_draw_calls.clear();
    g_static_shrub_draw_calls.clear();
    g_static_tfrag_geom.release();
    g_static_tie_geom.release();
    g_static_shrub_geom.release();

    g_visible_tfrag_indices.clear();
    g_visible_tie_indices.clear();
    g_visible_shrub_indices.clear();
}

} // namespace rac1
