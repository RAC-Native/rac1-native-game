// The OpenGL renderer's multi-draws, and their single-draw path (gl_draws.h). Host code, no original counterpart.
#include <game/render/gl_draws.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace rac1 {

namespace {

// (GL 4.x entry points: the loader the runtime uses stops at 3.3; the context is 4.6)
using PfnMultiDrawElementsIndirect = void(APIENTRYP)(GLenum, GLenum, const void*, GLsizei, GLsizei);
using PfnDrawElementsInstancedBaseVertexBaseInstance = void(APIENTRYP)(GLenum, GLsizei, GLenum, const void*, GLsizei, GLint, GLuint);
PfnMultiDrawElementsIndirect p_mdi = nullptr;
PfnDrawElementsInstancedBaseVertexBaseInstance p_single = nullptr;
bool g_single = false;
bool g_inited = false;

struct Cmd {  // DrawElementsIndirectCommand
    uint32_t count, instances, first_index;
    int32_t base_vertex;
    uint32_t base_instance;
};
static_assert(sizeof(Cmd) == 20, "DrawElementsIndirectCommand");

const char* gl_str(GLenum e) {
    const GLubyte* s = glGetString(e);
    return s != nullptr ? reinterpret_cast<const char*>(s) : "?";
}

}  // namespace

// orig: (host) see gl_draws.h
void gl_draws_init() {
    if (g_inited) return;
    g_inited = true;
    p_mdi = reinterpret_cast<PfnMultiDrawElementsIndirect>(glfwGetProcAddress("glMultiDrawElementsIndirect"));
    p_single = reinterpret_cast<PfnDrawElementsInstancedBaseVertexBaseInstance>(glfwGetProcAddress("glDrawElementsInstancedBaseVertexBaseInstance"));
    const char* vendor = gl_str(GL_VENDOR);
    std::fprintf(stderr, "[gl] %s, %s, OpenGL %s\n", vendor, gl_str(GL_RENDERER), gl_str(GL_VERSION));
    // (the default uniform block's room: the world's fragment shader with the modern lighting uses some 2950 components, its
    // vertex shaders some 1600; a driver with 4096 (Intel's) may refuse a program - its link failure is in the log, [mrpw] / [mrp])
    GLint fu = 0, vu = 0;
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &fu);
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &vu);
    std::fprintf(stderr, "[gl] uniform components: fragment %d, vertex %d\n", fu, vu);
    const char* env = std::getenv("RAC1_GL_SINGLE_DRAWS");
    const char* why = "RAC1_GL_SINGLE_DRAWS";
    if (env != nullptr && *env != '\0') {
        g_single = std::atoi(env) != 0;
    } else {
#ifdef _WIN32
        // (Intel's own Windows driver: GL_VENDOR "Intel" or "Intel Corporation"; on Linux, Mesa's says "Intel" too but its
        // multi-draws are right)
        g_single = std::strncmp(vendor, "Intel", 5) == 0;
        why = "Intel's Windows driver";
#endif
    }
    if (g_single && p_single == nullptr) {
        std::fprintf(stderr, "[gl] no glDrawElementsInstancedBaseVertexBaseInstance: the multi-draws kept\n");
        g_single = false;
    }
    if (g_single) std::fprintf(stderr, "[gl] the multi-draws issued as single draws (%s; RAC1_GL_SINGLE_DRAWS=0 turns it off)\n", why);
}

// orig: (host) see gl_draws.h
bool gl_single_draws() { return g_single; }

// orig: (host) see gl_draws.h
void gl_multi_draw_elements(const void* cpu, uint32_t first, uint32_t count) {
    if (count == 0) return;
    if (!g_inited) gl_draws_init();
    if (!g_single || cpu == nullptr) {
        if (p_mdi != nullptr)
            p_mdi(GL_TRIANGLES, GL_UNSIGNED_INT, reinterpret_cast<const void*>(static_cast<uintptr_t>(first) * sizeof(Cmd)), static_cast<GLsizei>(count), 0);
        return;
    }
    const Cmd* c = static_cast<const Cmd*>(cpu) + first;
    for (uint32_t i = 0; i < count; ++i, ++c) {
        if (c->count == 0 || c->instances == 0) continue;  // (as the multi-draw: nothing drawn)
        p_single(GL_TRIANGLES, static_cast<GLsizei>(c->count), GL_UNSIGNED_INT,
                 reinterpret_cast<const void*>(static_cast<uintptr_t>(c->first_index) * 4u), static_cast<GLsizei>(c->instances), c->base_vertex,
                 c->base_instance);
    }
}

}  // namespace rac1
