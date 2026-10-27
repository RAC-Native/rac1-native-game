// See gs_blend.h.
#include <runtime/gs/gs_blend.h>

#include <runtime/vk/vk_context.h>

#include <glad/glad.h>

#include <algorithm>

namespace rac1::gs {

uint32_t field(uint64_t v, int lo, int n) { return static_cast<uint32_t>((v >> lo) & ((1ull << n) - 1)); }

void blend_state(const DrawState& st, bool to_rt) {
    if (vk::is_active()) {
        if (field(st.prim, 6, 1)) {
            const float fix = std::min(static_cast<float>(field(st.alpha, 32, 8)) / 128.0f, 1.0f);
            const float bc[4] = {0.0f, 0.0f, 0.0f, fix};
            vkCmdSetBlendConstants(vk::cmd(), bc);
        }
        return;
    }
    if (!field(st.prim, 6, 1)) {
        glDisable(GL_BLEND);
        return;
    }
    glEnable(GL_BLEND);
    // ALPHA: Cv = (A - B) C + D, A / B / D one of Cs (0), Cd (1), 0 (2; 3 is reserved, read as 0), C one of As (0), Ad (1), FIX (2)
    const uint32_t A = field(st.alpha, 0, 2), B = field(st.alpha, 2, 2), C = field(st.alpha, 4, 2), D = field(st.alpha, 6, 2);
    const float fix = static_cast<float>(field(st.alpha, 32, 8)) / 128.0f;
    glBlendColor(0, 0, 0, std::min(fix, 1.0f));
    const GLenum cf = C == 2 ? GL_CONSTANT_ALPHA : (C == 1 ? GL_DST_ALPHA : GL_SRC_ALPHA);
    const GLenum cf1 = C == 2 ? GL_ONE_MINUS_CONSTANT_ALPHA : (C == 1 ? GL_ONE_MINUS_DST_ALPHA : GL_ONE_MINUS_SRC_ALPHA);
    // the factor of Cs and of Cd in that sum, k0 + k1 C (k0, k1 of -1, 0, 1), as a GL factor and its sign; every ALPHA but the two
    // that give a factor 1 + C (A = D, B = 0) maps exactly (those: 1). Before, the combinations without a case of their own were
    // drawn as (Cs - Cd) C + Cd: Orxon's O2 barriers' second pass (Cd - Cs) FIX, ALPHA 0xa1, was drawn as the source - opaque black
    auto factor = [&](uint32_t who, GLenum& f) {
        const int k0 = D == who ? 1 : 0, k1 = (A == who ? 1 : 0) - (B == who ? 1 : 0);
        if (k1 == 0) {
            f = k0 != 0 ? GL_ONE : GL_ZERO;
            return 1;
        }
        if (k0 == 0) {
            f = cf;
            return k1;
        }
        f = k1 < 0 ? cf1 : GL_ONE;  // (1 - C; 1 + C, not a GL factor: 1)
        return 1;
    };
    GLenum sf = GL_ZERO, df = GL_ZERO;
    const int ss = factor(0, sf), ds = factor(1, df);
    // (the GS blends the colour; the alpha written is the source's)
    if (ss >= 0 && ds >= 0) {
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    } else if (ss < 0 && ds >= 0) {  // Cd df - Cs sf
        glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_ADD);
    } else if (ss >= 0) {  // Cs sf - Cd df
        glBlendEquationSeparate(GL_FUNC_SUBTRACT, GL_FUNC_ADD);
    } else {  // (-(Cs + Cd) C: below 0, clamped to black)
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        sf = df = GL_ZERO;
    }
    if (to_rt) {
        auto as_gs = [](GLenum f) -> GLenum {
            if (f == GL_SRC_ALPHA) return GL_SRC1_ALPHA;
            if (f == GL_ONE_MINUS_SRC_ALPHA) return GL_ONE_MINUS_SRC1_ALPHA;
            return f;
        };
        sf = as_gs(sf);
        df = as_gs(df);
    }
    glBlendFuncSeparate(sf, df, GL_ONE, GL_ZERO);
}

void vk_blend_state(const DrawState& st, bool to_rt, vk::GraphicsPipelineDesc& desc, float& out_fix) {
    out_fix = 0.0f;
    desc.src_alpha_blend = VK_BLEND_FACTOR_ONE;
    desc.dst_alpha_blend = VK_BLEND_FACTOR_ZERO;
    desc.alpha_blend_op = VK_BLEND_OP_ADD;
    if (!field(st.prim, 6, 1)) {
        desc.blend_enable = false;
        desc.src_color_blend = VK_BLEND_FACTOR_ONE;
        desc.dst_color_blend = VK_BLEND_FACTOR_ZERO;
        desc.color_blend_op = VK_BLEND_OP_ADD;
        return;
    }
    desc.blend_enable = true;
    const uint32_t A = field(st.alpha, 0, 2), B = field(st.alpha, 2, 2), C = field(st.alpha, 4, 2), D = field(st.alpha, 6, 2);
    out_fix = std::min(static_cast<float>(field(st.alpha, 32, 8)) / 128.0f, 1.0f);
    const VkBlendFactor cf = C == 2 ? VK_BLEND_FACTOR_CONSTANT_ALPHA : (C == 1 ? VK_BLEND_FACTOR_DST_ALPHA : VK_BLEND_FACTOR_SRC_ALPHA);
    const VkBlendFactor cf1 = C == 2 ? VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA : (C == 1 ? VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    auto factor = [&](uint32_t who, VkBlendFactor& f) {
        const int k0 = D == who ? 1 : 0, k1 = (A == who ? 1 : 0) - (B == who ? 1 : 0);
        if (k1 == 0) {
            f = k0 != 0 ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ZERO;
            return 1;
        }
        if (k0 == 0) {
            f = cf;
            return k1;
        }
        f = k1 < 0 ? cf1 : VK_BLEND_FACTOR_ONE;
        return 1;
    };
    VkBlendFactor sf = VK_BLEND_FACTOR_ZERO, df = VK_BLEND_FACTOR_ZERO;
    const int ss = factor(0, sf), ds = factor(1, df);
    if (ss >= 0 && ds >= 0) {
        desc.color_blend_op = VK_BLEND_OP_ADD;
    } else if (ss < 0 && ds >= 0) {
        desc.color_blend_op = VK_BLEND_OP_REVERSE_SUBTRACT;
    } else if (ss >= 0) {
        desc.color_blend_op = VK_BLEND_OP_SUBTRACT;
    } else {
        desc.color_blend_op = VK_BLEND_OP_ADD;
        sf = df = VK_BLEND_FACTOR_ZERO;
    }
    if (to_rt) {
        auto as_gs = [](VkBlendFactor f) -> VkBlendFactor {
            if (f == VK_BLEND_FACTOR_SRC_ALPHA) return VK_BLEND_FACTOR_SRC1_ALPHA;
            if (f == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
            return f;
        };
        sf = as_gs(sf);
        df = as_gs(df);
    }
    desc.src_color_blend = sf;
    desc.dst_color_blend = df;
}

}  // namespace rac1::gs
