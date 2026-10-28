// The surface materials' GLSL (docs/materials.md), spliced at the end of RAC1_SHADOW_FS (mrp_lighting.h): the lighting's variants
// (SH_POINTS: the world; SH_SPLIT: the mobys) only. The Vulkan copy is in tools/gen_vk_shaders.py: keep the two alike.
#define RAC1_MATERIALS_FS \
    "#if defined(SH_POINTS) || defined(SH_SPLIT)\n"                                                                                                     \
    "/* the surface materials (docs/materials.md): a slot's type in uEmis[s].z, its parameters (the defaults filled in) in uMat[s].xyz */\n"            \
    "/* a cell's random value in [0, 1): integer arithmetic on the point (16ths of a unit: the callers' points are whole cells and their\n"             \
    "   offsets), exact on every GPU - the floating-point hash it replaces lost its low bits differently per driver (radeonsi's OpenGL\n"               \
    "   and RADV's Vulkan put the puddles in different places) */\n"                                                                                    \
    "float sm_hash(vec2 p) {\n"                                                                                                                         \
    "    uvec2 c = uvec2(ivec2(floor(p * 16.0)));\n"                                                                                                    \
    "    uint h = c.x * 0x8da6b343u ^ c.y * 0xd8163841u;\n"                                                                                             \
    "    h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;\n"                                                               \
    "    return float(h >> 8) * (1.0 / 16777216.0);\n"                                                                                                  \
    "}\n"                                                                                                                                               \
    "float sm_noise(vec2 p) {\n"                                                                                                                        \
    "    vec2 i = floor(p), f = fract(p);\n"                                                                                                            \
    "    vec2 u = f * f * (3.0 - 2.0 * f);\n"                                                                                                           \
    "    return mix(mix(sm_hash(i), sm_hash(i + vec2(1.0, 0.0)), u.x), mix(sm_hash(i + vec2(0.0, 1.0)), sm_hash(i + vec2(1.0, 1.0)), u.x), u.y);\n"     \
    "}\n"                                                                                                                                               \
    "int sm_type(uint s) {\n"                                                                                                                           \
    "    if (s == 0u || uShOn == 0 || uShPass != 0 || uMat[0].x == 0.0 || uEmis[s].w > 0.0) return 0;\n"                                                \
    "    return int(uEmis[s].z + 0.5);\n"                                                                                                               \
    "}\n"                                                                                                                                               \
    "/* lava, mud, quicksand: the texture pushed slowly to and fro by broad waves across the world (bounded: the texture's wrap left alone) */\n"       \
    "vec2 sh_mat_uv(vec2 st, uint s) {\n"                                                                                                               \
    "    int ty = sm_type(s);\n"                                                                                                                        \
    "    if (ty != 4 && ty != 6 && ty != 7) return st;\n"                                                                                               \
    "    float flow = ty == 4 ? 1.0 : ty == 6 ? uMat[s].y : uMat[s].x;\n"                                                                               \
    "    float t = uMat[0].y * (ty == 4 ? 1.0 : 0.3);\n"                                                                                                \
    "    vec2 p = vShWorld.xy;\n"                                                                                                                       \
    "    vec2 d = vec2(sin(p.y * 0.21 + t * 0.6) + 0.5 * sin(p.x * 0.37 - t * 0.43), cos(p.x * 0.19 + t * 0.5) + 0.5 * cos(p.y * 0.31 + t * 0.71));\n"  \
    "    return st + d * ((ty == 4 ? 0.02 : 0.012) * flow);\n"                                                                                          \
    "}\n"                                                                                                                                               \
    "/* three crossing wave trains (the height's gradient in world x, y; z is up), on the faces turned up */\n"                                         \
    "vec3 sm_waves(vec3 n, vec3 P, float amt, float t) {\n"                                                                                             \
    "    vec2 d1 = vec2(0.8, 0.6), d2 = vec2(-0.447, 0.894), d3 = vec2(0.196, -0.981);\n"                                                               \
    "    vec2 g = d1 * (0.065 * cos(dot(d1, P.xy) * 1.3 + t * 1.7));\n"                                                                                 \
    "    g += d2 * (0.063 * cos(dot(d2, P.xy) * 2.1 - t * 2.3));\n"                                                                                     \
    "    g += d3 * (0.055 * cos(dot(d3, P.xy) * 3.7 + t * 3.1));\n"                                                                                     \
    "    g += vec2(sm_noise(P.xy * 2.3 + t * 0.6) - 0.5, sm_noise(P.xy * 2.3 - t * 0.5 + 9.1) - 0.5) * 0.12;\n"                                         \
    "    return normalize(n - vec3(g, 0.0) * (amt * smoothstep(0.3, 0.7, n.z)));\n"                                                                     \
    "}\n"                                                                                                                                               \
    "/* the sky (the ambient particles' sky grid, docs/particles.md, Under a roof): 1 under the open sky, 0 under a roof - the rain's rings\n"       \
    "   only where it rains. Its window's heights, then a row: x, y its corner, the squares' size, 1 when on (no texture: 1) */\n"                      \
    "#ifdef VULKAN\n"                                                                                                                                   \
    "layout(set = 1, binding = 6) uniform sampler2D uMatSky;\n"                                                                                         \
    "#else\n"                                                                                                                                           \
    "layout(binding = 30) uniform sampler2D uMatSky;\n"                                                                                                 \
    "#endif\n"                                                                                                                                          \
    "float sm_sky(vec3 p) {\n"                                                                                                                          \
    "    ivec2 n = textureSize(uMatSky, 0);\n"                                                                                                          \
    "    if (n.y < 2) return 1.0;\n"                                                                                                                    \
    "    n.y -= 1;\n"                                                                                                                                   \
    "    vec4 w = vec4(texelFetch(uMatSky, ivec2(0, n.y), 0).r, texelFetch(uMatSky, ivec2(1, n.y), 0).r, texelFetch(uMatSky, ivec2(2, n.y), 0).r,\n"    \
    "                  texelFetch(uMatSky, ivec2(3, n.y), 0).r);\n"                                                                                     \
    "    if (w.w < 0.5 || w.z <= 0.0) return 1.0;\n"                                                                                                    \
    "    vec2 g = (p.xy - w.xy) / w.z - 0.5;\n"                                                                                                         \
    "    ivec2 i0 = ivec2(floor(g));\n"                                                                                                                 \
    "    vec2 f = g - vec2(i0);\n"                                                                                                                      \
    "    vec4 o;\n"                                                                                                                                     \
    "    for (int k = 0; k < 4; ++k) {\n"                                                                                                               \
    "        ivec2 c = i0 + ivec2(k & 1, k >> 1);\n"                                                                                                    \
    "        float h = (any(lessThan(c, ivec2(0))) || any(greaterThanEqual(c, n))) ? -1.0e6 : texelFetch(uMatSky, c, 0).r;\n"                           \
    "        o[k] = smoothstep(h - 0.25, h + 0.25, p.z + 0.75);\n"                                                                                      \
    "    }\n"                                                                                                                                           \
    "    return mix(mix(o.x, o.y, f.x), mix(o.z, o.w, f.x), f.y);\n"                                                                                    \
    "}\n"                                                                                                                                               \
    "/* rain: rings spreading from random points, two layers of cells */\n"                                                                             \
    "vec3 sm_rain(vec3 n, vec3 P, float t, float k) {\n"                                                                                                \
    "    vec2 g = vec2(0.0);\n"                                                                                                                         \
    "    for (int l = 0; l < 2; ++l) {\n"                                                                                                               \
    "        vec2 q = P.xy * 1.6 + vec2(float(l) * 0.37, float(l) * 0.71);\n"                                                                           \
    "        vec2 c = floor(q);\n"                                                                                                                      \
    "        float ph = fract(t * 0.9 + sm_hash(c + float(l) * 17.0));\n"                                                                               \
    "        vec2 v = q - (c + 0.25 + 0.5 * vec2(sm_hash(c + 3.1), sm_hash(c + 7.7)));\n"                                                               \
    "        float d = length(v);\n"                                                                                                                    \
    "        float x = (d - ph * 0.6) * 18.0;\n"                                                                                                        \
    "        g += (d > 1e-4 ? v / d : vec2(0.0)) * (cos(x * 3.0) * exp(-x * x) * (1.0 - ph) * 0.6);\n"                                                  \
    "    }\n"                                                                                                                                           \
    "    return normalize(n - vec3(g, 0.0) * (k * smoothstep(0.6, 0.9, n.z)));\n"                                                                       \
    "}\n"                                                                                                                                               \
    "/* the specular of the sun (where it is not in shadow) and of the point lights of the pixel's tile: Blinn-Phong with an exponent from a\n"         \
    "   GGX-like roughness, energy-normalised, Schlick's Fresnel */\n"                                                                                  \
    "vec3 sm_spec(vec3 N, vec3 V, float rough, vec3 F0) {\n"                                                                                            \
    "    float r = clamp(rough, 0.03, 1.0);\n"                                                                                                          \
    "    float pw = clamp(2.0 / (r * r * r * r) - 2.0, 1.0, 4096.0);\n"                                                                                 \
    "    float norm = (pw + 8.0) / 25.1327;\n"                                                                                                          \
    "    vec3 sum = vec3(0.0);\n"                                                                                                                       \
    "    if (uShKey.w >= 0.0 && uShSunK > 0.0) {\n"                                                                                                     \
    "        vec3 H = normalize(uShSun + V);\n"                                                                                                         \
    "        float ndl = max(dot(N, uShSun), 0.0), ndh = max(dot(N, H), 0.0), vdh = max(dot(V, H), 0.0);\n"                                             \
    "        float vis = clamp(1.0 - (1.0 - sh_vis) / max(uShStrength, 0.01), 0.0, 1.0);\n"                                                             \
    "        vec3 F = F0 + (1.0 - F0) * pow(1.0 - vdh, 5.0);\n"                                                                                         \
    "        sum += uShKey.rgb * (255.0 / 128.0) * (uShSunK * vis * norm * pow(ndh, pw) * ndl) * F;\n"                                                  \
    "    }\n"                                                                                                                                           \
    "    if (uPtN > 0) {\n"                                                                                                                             \
    "        vec4 cp = uPtVP * vec4(vShWorld, 1.0);\n"                                                                                                  \
    "        ivec2 ts = textureSize(uPtTiles, 0);\n"                                                                                                    \
    "        vec2 t01 = cp.w > 0.0 ? cp.xy / cp.w * 0.5 + 0.5 : vec2(0.5);\n"                                                                           \
    "        uvec4 m = texelFetch(uPtTiles, clamp(ivec2(floor(t01 * vec2(ts))), ivec2(0), ts - 1), 0);\n"                                               \
    "        while ((m.x | m.y | m.z) != 0u) {\n"                                                                                                       \
    "            int i;\n"                                                                                                                              \
    "            if (m.x != 0u) { i = findLSB(m.x); m.x &= m.x - 1u; }\n"                                                                               \
    "            else if (m.y != 0u) { i = 32 + findLSB(m.y); m.y &= m.y - 1u; }\n"                                                                     \
    "            else { i = 64 + findLSB(m.z); m.z &= m.z - 1u; }\n"                                                                                    \
    "            if (i >= uPtN) break;\n"                                                                                                               \
    "            vec3 L = uPtPos[i].xyz - vShWorld;\n"                                                                                                  \
    "            float rr = uPtPos[i].w, d2 = dot(L, L);\n"                                                                                             \
    "            if (d2 >= rr * rr || d2 <= 0.0) continue;\n"                                                                                           \
    "            float inv_d = inversesqrt(d2);\n"                                                                                                      \
    "            L *= inv_d;\n"                                                                                                                         \
    "            float ndl = dot(N, L);\n"                                                                                                              \
    "            if (ndl <= 0.0) continue;\n"                                                                                                           \
    "            float att = 1.0 - d2 * inv_d / rr;\n"                                                                                                  \
    "            vec3 H = normalize(L + V);\n"                                                                                                          \
    "            float ndh = max(dot(N, H), 0.0), vdh = max(dot(V, H), 0.0);\n"                                                                         \
    "            vec3 F = F0 + (1.0 - F0) * pow(1.0 - vdh, 5.0);\n"                                                                                     \
    "            sum += uPtCol[i].rgb * (0.5 * att * att * norm * pow(ndh, pw) * ndl) * F;\n"                                                           \
    "        }\n"                                                                                                                                       \
    "    }\n"                                                                                                                                           \
    "    return sum;\n"                                                                                                                                 \
    "}\n"                                                                                                                                               \
    "/* what a reflection sees where nothing is found: the level's sky above, its shade below, softer with the roughness */\n"                          \
    "vec3 sm_env(vec3 R, float rough) {\n"                                                                                                              \
    "    float up = clamp(R.z * (2.0 - rough) + 0.25, 0.0, 1.0);\n"                                                                                     \
    "    return uSsrSky.rgb * mix(0.3, 1.0, up * up * (3.0 - 2.0 * up));\n"                                                                             \
    "}\n"                                                                                                                                               \
    "/* High: the reflection from the last frame's picture (its depth marched along the ray in world space, reprojected with its own view;\n"           \
    "   the first step behind the picture's surface refined by halves); `miss` where the ray leaves the picture or passes behind an object */\n"        \
    "float sm_ssr_dz(vec3 q, out vec2 uv) {\n"                                                                                                          \
    "    vec4 c = uSsrVP * vec4(q, 1.0);\n"                                                                                                             \
    "    uv = c.w > 0.01 ? c.xy / c.w * 0.5 + 0.5 : vec2(-1.0);\n"                                                                                      \
    "    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) return -1e9;\n"                                                                      \
    "    float d = textureLod(uSsrDepth, uv, 0.0).r;\n"                                                                                                 \
    "    if (d >= 1.0) return -1e9;\n"                                                                                                                  \
    "    return c.w - uSsrZ.y / ((d * 2.0 - 1.0) - uSsrZ.x);  /* > 0: the ray behind the picture's surface (clip w: the depth along the view) */\n"     \
    "}\n"                                                                                                                                               \
    "/* `metal`: the hit kept only where the ray passes within some 0.2 units of the old picture's surface, faded out by twice that (the\n"            \
    "   puddles' and the water's test - 0.3 + 5 % of the distance, faded to three times it - took rays passing well behind an object as hits:\n"                    \
    "   on a polished metal the mirror smeared and jumped from frame to frame, #102 batch 1); `blur` (0 sharp, 1) spreads the picture read */\n"                  \
    "vec3 sm_ssr(vec3 P, vec3 R, vec3 miss, float blur, bool metal) {\n"                                                                                               \
    "    if (uMat[0].x < 1.5 || uMat[0].w == 0.0) return miss;\n"                                                                                          \
    "    float t0 = 0.0, t = 0.2;\n"                                                                                                                       \
    "    vec2 uv;\n"                                                                                                                                       \
    "    for (int i = 0; i < 24; ++i) {\n"                                                                                                                 \
    "        float dz = sm_ssr_dz(P + R * t, uv);\n"                                                                                                       \
    "        if (uv.x < 0.0) return miss;\n"                                                                                                               \
    "        if (dz > 0.0) {\n"                                                                                                                            \
    "            for (int k = 0; k < 5; ++k) {\n"                                                                                                          \
    "                float tm = 0.5 * (t0 + t);\n"                                                                                                         \
    "                if (sm_ssr_dz(P + R * tm, uv) > 0.0) t = tm;\n"                                                                                       \
    "                else t0 = tm;\n"                                                                                                                      \
    "            }\n"                                                                                                                                      \
    "            dz = sm_ssr_dz(P + R * t, uv);\n"                                                                                                         \
    "            if (uv.x < 0.0) return miss;\n"                                                                                                           \
    "            float th = metal ? 0.15 + 0.02 * t : 0.3 + 0.05 * t;  /* (behind an object rather than on its surface: faded out, not cut) */\n"                                                                                                            \
    "            vec2 e = smoothstep(vec2(0.0), vec2(0.08), uv) * smoothstep(vec2(0.0), vec2(0.08), 1.0 - uv);\n"                                          \
    "            vec3 col = textureLod(uSsrCol, uv, 0.0).rgb;\n"                                                                                           \
    "            if (blur > 0.0) {  /* four more taps around the hit, farther apart the rougher and the longer the ray */\n"                               \
    "                float r = blur * clamp(0.004 * t, 0.001, 0.012);\n"                                                                                     \
    "                col = (col + textureLod(uSsrCol, clamp(uv + vec2(r, 0.0), 0.0, 1.0), 0.0).rgb + textureLod(uSsrCol, clamp(uv - vec2(r, 0.0), 0.0, 1.0), 0.0).rgb\n" \
    "                       + textureLod(uSsrCol, clamp(uv + vec2(0.0, r), 0.0, 1.0), 0.0).rgb + textureLod(uSsrCol, clamp(uv - vec2(0.0, r), 0.0, 1.0), 0.0).rgb) * 0.2;\n" \
    "            }\n"                                                                                                                                      \
    "            return mix(miss, col, e.x * e.y * (1.0 - smoothstep(th, (metal ? 2.0 : 3.0) * th, dz)));\n"                                                               \
    "        }\n"                                                                                                                                          \
    "        t0 = t;\n"                                                                                                                                    \
    "        t = t * 1.25 + 0.15;\n"                                                                                                                       \
    "    }\n"                                                                                                                                              \
    "    return miss;\n"                                                                                                                                   \
    "}\n"                                                                                                                                                  \
    "/* High: lava's heat shimmer - the last frame's picture around the pixel, displaced by a rising noise */\n"                                        \
    "vec3 sm_shimmer(vec3 c, vec3 P, float t) {\n"                                                                                                      \
    "    if (uMat[0].x < 1.5 || uMat[0].w == 0.0) return c;\n"                                                                                          \
    "    vec4 q = uSsrVP * vec4(P, 1.0);\n"                                                                                                             \
    "    if (q.w <= 0.01) return c;\n"                                                                                                                  \
    "    vec2 uv = q.xy / q.w * 0.5 + 0.5;\n"                                                                                                           \
    "    vec2 o = vec2(sm_noise(P.xy * 1.7 + vec2(0.0, -t * 1.9)) - 0.5, sm_noise(P.xy * 1.3 + vec2(t * 1.3, 5.0)) - 0.5) * 0.014;\n"                   \
    "    return mix(c, textureLod(uSsrCol, clamp(uv + o, vec2(0.0), vec2(1.0)), 0.0).rgb, 0.35);\n"                                                     \
    "}\n"                                                                                                                                               \
    "/* the mobys (SH_SPLIT, #110): the modern point lights per pixel - the lights placed by hand, per class, the beams, the automatic\n" \
    "   emitters. The game's own (the list's first uEmis[0].y) light the mobys already, per vertex in the skinning's record: left out.\n" \
    "   No cube shadows (a moby casts into them: it would shadow itself); the face normal as the world's (sh_n) */\n" \
    "vec3 sh_points_moby(vec3 c) {\n" \
    "    if (uShOn == 0 || uShPass != 0 || uPtN <= 0) return c;\n" \
    "    int first = int(uEmis[0].y + 0.5);\n" \
    "    if (first >= uPtN) return c;\n" \
    "    vec3 add = vec3(0.0);\n" \
    "    vec4 cp = uPtVP * vec4(vShWorld, 1.0);\n" \
    "    ivec2 ts = textureSize(uPtTiles, 0);\n" \
    "    vec2 t01 = cp.w > 0.0 ? cp.xy / cp.w * 0.5 + 0.5 : vec2(0.5);\n" \
    "    uvec4 m = texelFetch(uPtTiles, clamp(ivec2(floor(t01 * vec2(ts))), ivec2(0), ts - 1), 0);\n" \
    "    while ((m.x | m.y | m.z) != 0u) {\n" \
    "        int i;\n" \
    "        if (m.x != 0u) { i = findLSB(m.x); m.x &= m.x - 1u; }\n" \
    "        else if (m.y != 0u) { i = 32 + findLSB(m.y); m.y &= m.y - 1u; }\n" \
    "        else { i = 64 + findLSB(m.z); m.z &= m.z - 1u; }\n" \
    "        if (i >= uPtN) break;\n" \
    "        if (i < first) continue;\n" \
    "        vec3 L = uPtPos[i].xyz - vShWorld;\n" \
    "        float r = uPtPos[i].w, d2 = dot(L, L);\n" \
    "        if (d2 >= r * r || d2 <= 0.0) continue;\n" \
    "        float inv_d = inversesqrt(d2), d = d2 * inv_d;\n" \
    "        float ndl_p = dot(sh_n, L) * inv_d;\n" \
    "        if (ndl_p <= 0.0) continue;\n" \
    "        float att = 1.0 - d / r;\n" \
    "        att = mix(att, att * att * max(L.z * inv_d, 0.0), uPtDown[i >> 2][i & 3]);\n" \
    "        add += uPtCol[i].rgb * (ndl_p * att * 0.5);\n" \
    "    }\n" \
    "    return c + add;\n" \
    "}\n" \
    "/* a material's texel: lit the texel as lit (after the emissive), orig as the original lighting shows it (the game's vertex colour,\n"             \
    "   unshadowed), tex the texel, light the lit vertex colour (x 255 / 128: 1 the texture as painted) */\n"                                           \
    "vec3 sm_material(vec3 lit, vec3 orig, vec3 tex, vec3 light, uint s, int ty) {\n"                                                                   \
    "    vec4 m = uMat[s];\n"                                                                                                                           \
    "    float t = uMat[0].y, rain = uMat[0].z;\n"                                                                                                      \
    "    vec3 P = vShWorld, N = sh_n, V = normalize(uShCam - P);\n"                                                                                     \
    "    vec3 A = clamp(tex * (255.0 / 128.0), 0.0, 1.0);\n"                                                                                            \
    "    vec3 L = clamp(light, 0.0, 1.5);\n"                                                                                                            \
    "    float ndv = max(dot(N, V), 0.0);\n"                                                                                                            \
    "    vec3 R = reflect(-V, N);\n"                                                                                                                    \
    "    if (ty == 1) {  /* metal: a roughness (0 a mirror .. 1 brushed), b strength */\n"                                                                 \
    "        float r = clamp(m.x, 0.03, 1.0), gloss = 1.0 - r;\n"                                                                                          \
    "        vec3 F0 = mix(vec3(0.04), A, 0.85);\n"                                                                                                        \
    "        vec3 Fv = F0 + (1.0 - F0) * pow(1.0 - ndv, 5.0);\n"                                                                                           \
    "        /* what it mirrors: the sky above and the ground's shade below, its horizon sharper the more polished it is (at most four\n"                  \
    "           times: the faces are flat, and a sharper horizon flipped whole faces between the sky and the ground as they turned); on\n"                 \
    "           High the last frame's picture along the reflected ray, from roughness 0.45 down (crisp only toward 0.05, blurred above: a\n"               \
    "           crisp mirror from 0.4 down showed every flaw of the old picture, #102 batch 1) - not on a moby: the old picture holds the\n"               \
    "           moby itself where it was a frame before (a spinning bolt mirrored its own last pose) */\n"                                                 \
    "        float up = clamp(R.z * mix(1.0, 4.0, gloss * gloss) + 0.5, 0.0, 1.0);\n"                                                                      \
    "        vec3 env = uSsrSky.rgb * mix(0.25, 1.15, up * up * (3.0 - 2.0 * up)) * L;\n"                                                                  \
    "#ifndef SH_MOBY\n"                                                                                                                                    \
    "        if (gloss > 0.55) env = mix(env, sm_ssr(P + N * 0.03, R, env, smoothstep(0.05, 0.45, r), true), smoothstep(0.55, 0.95, gloss));\n"            \
    "#endif\n"                                                                                                                                             \
    "        /* polished: less of the painted (diffuse) colour, more mirror; the mirror dimmed where the sun does not reach (its shadow, with\n"                \
    "           the occlusion drawn after: the metals stayed as bright in the shadow as in the sun, #102 batch 1) */\n"                                    \
    "        float sun = sh_state.w > 0.0 ? sh_state.x : 1.0;\n"                                                                                           \
    "        sun *= mix(1.0, smoothstep(-0.2, 0.2, dot(N, uShSun)), uShKey.w >= 0.0 ? uShSunK : 0.0);  /* (and on the faces turned from the sun) */\n"     \
    "        vec3 mirror = env * Fv * mix(0.35, 1.5, gloss * gloss) * mix(0.35, 1.0, sun);\n"                                                              \
    "        return lit * (1.0 - m.y * mix(0.25, 0.55, gloss)) + m.y * (sm_spec(N, V, r, F0) + mirror);\n"                                                 \
    "    }\n"                                                                                                                                              \
    "    if (ty == 2 || ty == 3) {  /* wet: a wetness, b roughness; puddle: a coverage, b the noise's scale, c wetness */\n"                            \
    "        float wet = ty == 2 ? m.x : m.z, rough = ty == 2 ? m.y : 0.4;\n"                                                                           \
    "        float Fw = 0.04 + 0.96 * pow(1.0 - ndv, 5.0);\n"                                                                                           \
    "        vec3 base = lit * (1.0 - 0.4 * wet) + wet * (sm_spec(N, V, rough, vec3(0.04)) + sm_env(R, rough) * L * (Fw * 0.8));\n"                     \
    "        if (ty == 2) return base;\n"                                                                                                               \
    "        float n = sm_noise(P.xy * m.y) * 0.65 + sm_noise(P.xy * (m.y * 2.7) + 7.3) * 0.35;\n"                                                      \
    "        float mask = smoothstep(1.0 - m.x - 0.04, 1.0 - m.x + 0.04, n) * smoothstep(0.85, 0.97, N.z);\n"                                           \
    "        if (mask <= 0.0) return base;\n"                                                                                                           \
    "        vec3 Np = sm_waves(N, P, 0.05, t);\n"                                                                                                      \
    "        if (rain > 0.0) Np = sm_rain(Np, P, t, rain * 0.6 * sm_sky(P));\n"                                                                         \
    "        vec3 Rp = reflect(-V, Np);\n"                                                                                                              \
    "        float F = mix(0.15, 0.9, pow(1.0 - max(dot(Np, V), 0.0), 4.0));\n"                                                                         \
    "        vec3 refl = sm_ssr(P + N * 0.03, Rp, sm_env(Rp, 0.05) * L, 0.0, false);\n"                                                                             \
    "        vec3 pud = lit * (0.4 * (1.0 - F)) + refl * F + sm_spec(Np, V, 0.06, vec3(0.02));\n"                                                       \
    "        return mix(base, pud, mask);\n"                                                                                                            \
    "    }\n"                                                                                                                                           \
    "    if (ty == 4) {  /* lava: a strength, b pulse, c threshold; emissive by nature, a slow pulse across the world */\n"                             \
    "        float pulse = 1.0 + m.y * sin(t * 0.8 + sm_noise(P.xy * 0.08) * 6.2832);\n"                                                                \
    "        float v = max(tex.r, max(tex.g, tex.b)), th = m.z;\n"                                                                                      \
    "        float k = th > 0.0 ? smoothstep(th - 0.04, th + 0.04, v) : 1.0;\n"                                                                         \
    "        vec3 c = mix(lit, max(lit, orig * (m.x * pulse)), k);\n"                                                                                   \
    "        return sm_shimmer(c, P, t);\n"                                                                                                             \
    "    }\n"                                                                                                                                           \
    "    if (ty == 5) {  /* water: a strength, b ripples; on top of the game's own water (its scrolling and colours kept) */\n"                         \
    "        vec3 Nw = sm_waves(N, P, m.y, t);\n"                                                                                                       \
    "        if (rain > 0.0) Nw = sm_rain(Nw, P, t, rain * 0.5 * sm_sky(P));\n"                                                                         \
    "        vec3 Rw = reflect(-V, Nw);\n"                                                                                                              \
    "        float F = 0.02 + 0.98 * pow(1.0 - max(dot(Nw, V), 0.0), 5.0);\n"                                                                           \
    "        F = mix(0.15, 1.0, F);\n"                                                                                                                  \
    "        vec3 refl = sm_ssr(P + N * 0.03, Rw, sm_env(Rw, 0.05) * L, 0.0, false);\n"                                                                             \
    "        return lit * (1.0 - 0.35 * F * m.x) + m.x * (refl * (F * 0.7) + sm_spec(Nw, V, 0.07, vec3(0.02)) * 1.5);\n"                                \
    "    }\n"                                                                                                                                           \
    "    /* mud: a wetness, b drift; quicksand: a drift, b gloss - a sheen over the slow drift (sh_mat_uv) */\n"                                        \
    "    float g = ty == 6 ? m.x : m.y;\n"                                                                                                              \
    "    float Fm = 0.04 + 0.96 * pow(1.0 - ndv, 5.0);\n"                                                                                               \
    "    return lit * (1.0 - 0.25 * g) + g * (sm_spec(N, V, ty == 6 ? 0.38 : 0.5, vec3(0.04)) * 0.8 + sm_env(R, 0.5) * L * (Fm * 0.5));\n"              \
    "}\n"                                                                                                                                               \
    "/* the material, then its glow (an emissive line's strength and threshold on the same texture: a lamp's metal body and its bulb);\n"           \
    "   lava is emissive by itself */\n"                                                                                                               \
    "vec3 sh_material(vec3 lit, vec3 orig, vec3 tex, vec3 light, uint s) {\n"                                                                           \
    "    int ty = sm_type(s);\n"                                                                                                                        \
    "    if (ty == 0) return lit;\n"                                                                                                                    \
    "    vec3 r = sm_material(lit, orig, tex, light, s, ty);\n"                                                                                         \
    "    return ty == 4 ? r : sh_glow(r, orig, tex, uEmis[s]);\n"                                                                                       \
    "}\n"                                                                                                                                               \
    "#endif\n"
