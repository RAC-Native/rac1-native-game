#include "shader_manager.h"
#include <iostream>
#include <vector>

namespace rac1 {

ShaderManager& ShaderManager::get() {
    static ShaderManager instance;
    return instance;
}

static const char* s_generic_vs = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoords;

uniform mat4 u_ViewProj;
uniform mat4 u_Model;

out vec4 vColor;
out vec2 vTexCoords;

void main() {
    vColor = aColor;
    vTexCoords = aTexCoords;
    gl_Position = u_ViewProj * u_Model * vec4(aPos, 1.0);
}
)";

static const char* s_generic_fs = R"(
#version 330 core
in vec4 vColor;
in vec2 vTexCoords;

uniform sampler2D u_Texture;
uniform bool u_UseTexture;
uniform int u_AlphaTest;
uniform int u_DebugShade;
uniform int u_GsMode;
uniform float u_MobyAlpha;
uniform float u_AlphaRef;
uniform int u_TexBound;  // 1: the draw's texture is known uploaded (its clear texels are the texture's own)

out vec4 FragColor;

void main() {
    vec4 texColor = u_UseTexture ? texture(u_Texture, vTexCoords) : vec4(1.0);
    // Fallback: if u_UseTexture is true but the bound texture is empty
    // (= sample returns 0,0,0,0 because tex_id=0), don't multiply by zero
    // — fall back to vertex color so geometry stays visible.
    if (u_UseTexture && u_TexBound == 0 && texColor.a < 0.01 && length(texColor.rgb) < 0.01) {
        texColor = vec4(1.0);
    }
    FragColor = texColor * vColor;
    if (u_DebugShade == 1) FragColor = vec4(vColor.rgb, texColor.a * vColor.a);
    else if (u_DebugShade == 2) FragColor = vec4(texColor.rgb, texColor.a * vColor.a);
    else if (u_DebugShade == 3) FragColor = vec4(vec3(0.8), texColor.a * vColor.a);
    // u_GsMode (mobys, the GS state of the game's moby pass 0x2559c0): the moby's alpha
    // (+0x23) scales the fragment's; 1 = the pixels the alpha test passes (written with depth),
    // 2 = the ones it fails (AFAIL RGB_ONLY: colour without depth).
    if (u_GsMode != 0) {
        FragColor.a *= u_MobyAlpha;
        if (u_GsMode == 1 ? FragColor.a < u_AlphaRef : (FragColor.a >= u_AlphaRef || FragColor.a <= 0.0)) discard;
    } else if (u_AlphaTest != 0) {
        if (FragColor.a < 0.5) {
            discard;
        }
    } else {
        if (FragColor.a < 0.05) {
            discard;
        }
    }
}
)";

static const char* s_moby_vs = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in float aJointIdx;

uniform mat4 u_ViewProj;
uniform mat4 u_Model;
uniform mat4 u_Joints[128];
uniform bool u_HasJoints;

out vec4 vColor;
out vec2 vTexCoords;

void main() {
    vColor = aColor;
    vTexCoords = aTexCoords;
    
    vec4 localPos = vec4(aPos, 1.0);
    if (u_HasJoints && aJointIdx >= 0.0 && aJointIdx < 128.0) {
        localPos = u_Joints[int(aJointIdx)] * localPos;
    }
    
    gl_Position = u_ViewProj * u_Model * localPos;
}
)";

static const char* s_static_skinned_vs = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec4 aJoints;
layout (location = 4) in vec4 aWeights;
layout (location = 5) in float aMatIdx;

uniform mat4 u_ViewProj;
uniform mat4 u_Model;
uniform mat4 u_Joints[128];

out vec2 vTexCoords;
flat out int vMatIdx;

void main() {
    vTexCoords = aTexCoords;
    vMatIdx = int(aMatIdx + 0.5);
    
    int j0 = clamp(int(aJoints.x), 0, 127);
    int j1 = clamp(int(aJoints.y), 0, 127);
    int j2 = clamp(int(aJoints.z), 0, 127);
    int j3 = clamp(int(aJoints.w), 0, 127);

    mat4 skinMat = 
        aWeights.x * u_Joints[j0] +
        aWeights.y * u_Joints[j1] +
        aWeights.z * u_Joints[j2] +
        aWeights.w * u_Joints[j3];
        
    vec4 localPos = skinMat * vec4(aPos, 1.0);
    gl_Position = u_ViewProj * u_Model * localPos;
}
)";

static const char* s_static_skinned_fs = R"(
#version 330 core
in vec2 vTexCoords;
flat in int vMatIdx;

uniform sampler2D u_Textures[16];
uniform int u_DebugShade;
uniform int u_GsMode;
uniform float u_MobyAlpha;
uniform float u_AlphaRef;

out vec4 FragColor;

void main() {
    vec4 texColor = vec4(1.0);
    if (vMatIdx == 0) texColor = texture(u_Textures[0], vTexCoords);
    else if (vMatIdx == 1) texColor = texture(u_Textures[1], vTexCoords);
    else if (vMatIdx == 2) texColor = texture(u_Textures[2], vTexCoords);
    else if (vMatIdx == 3) texColor = texture(u_Textures[3], vTexCoords);
    else if (vMatIdx == 4) texColor = texture(u_Textures[4], vTexCoords);
    else if (vMatIdx == 5) texColor = texture(u_Textures[5], vTexCoords);
    else if (vMatIdx == 6) texColor = texture(u_Textures[6], vTexCoords);
    else if (vMatIdx == 7) texColor = texture(u_Textures[7], vTexCoords);
    else if (vMatIdx == 8) texColor = texture(u_Textures[8], vTexCoords);
    else if (vMatIdx == 9) texColor = texture(u_Textures[9], vTexCoords);
    else if (vMatIdx == 10) texColor = texture(u_Textures[10], vTexCoords);
    else if (vMatIdx == 11) texColor = texture(u_Textures[11], vTexCoords);
    else if (vMatIdx == 12) texColor = texture(u_Textures[12], vTexCoords);
    else if (vMatIdx == 13) texColor = texture(u_Textures[13], vTexCoords);
    else if (vMatIdx == 14) texColor = texture(u_Textures[14], vTexCoords);
    else if (vMatIdx == 15) texColor = texture(u_Textures[15], vTexCoords);
    
    FragColor = texColor;
    if (u_DebugShade == 3 || u_DebugShade == 1) FragColor.rgb = vec3(0.8);
    if (u_GsMode != 0) {  // as the generic shader's u_GsMode
        FragColor.a *= u_MobyAlpha;
        if (u_GsMode == 1 ? FragColor.a < u_AlphaRef : (FragColor.a >= u_AlphaRef || FragColor.a <= 0.0)) discard;
    } else if (FragColor.a < 0.05) discard;
}
)";

GLuint ShaderManager::compile_program(const char* vertex_src, const char* fragment_src) {
    GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vertex_src, NULL);
    glCompileShader(vertex);
    
    int success;
    char infoLog[512];
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    
    GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fragment_src, NULL);
    glCompileShader(fragment);
    
    glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragment, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    
    return program;
}

bool ShaderManager::init() {
    m_tfrag_shader = compile_program(s_generic_vs, s_generic_fs);
    m_tie_shader = compile_program(s_generic_vs, s_generic_fs);
    m_shrub_shader = compile_program(s_generic_vs, s_generic_fs);
    m_sky_shader = compile_program(s_generic_vs, s_generic_fs);
    
    m_moby_shader = compile_program(s_moby_vs, s_generic_fs);
    m_static_skinned_shader = compile_program(s_static_skinned_vs, s_static_skinned_fs);
    
    return (m_tfrag_shader != 0 && m_moby_shader != 0 && m_static_skinned_shader != 0);
}

void ShaderManager::set_debug_shade(int mode) {
    GLint prev = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev);
    for (GLuint p : {m_tfrag_shader, m_tie_shader, m_shrub_shader, m_sky_shader, m_moby_shader, m_static_skinned_shader}) {
        if (p == 0) continue;
        glUseProgram(p);
        glUniform1i(glGetUniformLocation(p, "u_DebugShade"), mode);
    }
    glUseProgram(static_cast<GLuint>(prev));
}

} // namespace rac1

