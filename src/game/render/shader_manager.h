#pragma once

#include <glad/glad.h>
#include <string>

namespace rac1 {

class ShaderManager {
public:
    static ShaderManager& get();

    bool init();

    // Standard shaders for different primitive types
    GLuint get_moby_shader() const { return m_moby_shader; }
    GLuint get_tfrag_shader() const { return m_tfrag_shader; }
    GLuint get_tie_shader() const { return m_tie_shader; }
    GLuint get_sky_shader() const { return m_sky_shader; }
    GLuint get_shrub_shader() const { return m_shrub_shader; }
    GLuint get_static_skinned_shader() const { return m_static_skinned_shader; }

    // the debug shading of every program (see g_render_shade)
    void set_debug_shade(int mode);

    static GLuint compile_program(const char* vertex_src, const char* fragment_src);

private:
    ShaderManager() = default;

    GLuint m_moby_shader = 0;
    GLuint m_tfrag_shader = 0;
    GLuint m_tie_shader = 0;
    GLuint m_sky_shader = 0;
    GLuint m_shrub_shader = 0;
    GLuint m_static_skinned_shader = 0;
};

// Common native vertex format for OpenGOAL-style rendering
struct NativeVertex {
    float x, y, z;
    float r, g, b, a;
    float u, v;
};

} // namespace rac1
