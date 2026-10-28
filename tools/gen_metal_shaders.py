#!/usr/bin/env python3
"""The Metal renderer's shaders (docs/metal.md), made from the Vulkan renderer's GLSL (tools/gen_vk_shaders.py).

Run by the build on macOS (CMakeLists.txt), as gen_vk_shaders.py is: nothing it writes is kept in the repository. Each
GLSL shader of gen_vk_shaders.SHADERS is compiled to SPIR-V by glslc (unoptimised: the names are kept), translated to
MSL by SPIRV-Cross, and embedded as text in <out_dir>/mtl_shader_sources.inc, which the Metal device compiles when the
game starts. Each shader's MSL is also left in <out_dir>/shaders/<name>.metal, to read.

    python3 tools/gen_metal_shaders.py <out_dir> [<glslc> <spirv-cross>]

What differs from the Vulkan shaders:
- The vertex shaders flip Y and map the depth from OpenGL's -1..1 to Metal's 0..1 at their end (--flip-vert-y,
  --fixup-clipspace): the pictures are kept in OpenGL's orientation (row 0 at the bottom), as the Vulkan renderer's.
- The world's fragment shaders take their 16 texture units from an argument buffer (Metal allows 16 samplers per
  stage; the units and the lighting's maps need 22): `uTexs` is moved to descriptor set 2, the argument buffer.
- The tfrag compute shader's doubles (Metal has none) are done with 64-bit integers: the same round-toward-zero adds
  and multiplies, bit for bit (tests/metal/tfrag_double_check.mm checks them against the CPU's).

Beside the shaders it writes <out_dir>/mtl_shader_table.inc: for each shader, where each of its Vulkan resources
(descriptor set, binding) lands in Metal (a buffer, texture or sampler index), and the push constants' buffer index,
so that the Metal branches bind by the Vulkan renderer's (set, binding) numbers.
"""
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen_vk_shaders as vk  # noqa: E402

GLSLC = "glslc"
SPIRV_CROSS = "spirv-cross"

# the first vertex buffer's index (binding b at kVertexBufferBase - b); the resources' buffers must stay below it
VERTEX_BUFFER_BASE = 30
VERTEX_BUFFER_MAX = 4

WORLD_TEXS_GL = "layout(set = 0, binding = 4) uniform sampler2DArray uTexs[16];"
WORLD_TEXS_MTL = "layout(set = 2, binding = 0) uniform sampler2DArray uTexs[16];"

# The tfrag compute shader's double arithmetic as 64-bit integers: round toward zero, as dchop made the hardware's
# round-to-nearest results. Double denormals are taken as zero (the views' and the vertices' values are far from them).
SOFT_DOUBLE = r"""
const uint64_t D_SIGN = 0x8000000000000000ul;
const uint64_t D_FRAC = 0x000ffffffffffffful;
const uint64_t D_HIDDEN = 0x0010000000000000ul;
uint64_t d_pack(uint64_t sign, int e, uint64_t mant) {
    if (e <= 0) return sign;  // (underflow: zero)
    if (e >= 2047) return sign | 0x7fefffffffffffful;  // (toward zero: the largest finite)
    return sign | (uint64_t(e) << 52) | (mant & D_FRAC);
}
// a float widened to 64 bits, exact (a float denormal made normal)
uint64_t f2d(float f) {
    uint u = floatBitsToUint(f);
    uint64_t sign = uint64_t(u & 0x80000000u) << 32;
    int e = int((u >> 23) & 0xffu);
    uint m = u & 0x7fffffu;
    if (e == 0) {
        if (m == 0u) return sign;
        int lz = 23 - findMSB(m);  // (the leading zeros within the 23 bits, plus one)
        m = (m << lz) & 0x7fffffu;
        e = 1 - lz;
    } else if (e == 255) {
        return sign | 0x7ff0000000000000ul | (uint64_t(m) << 29);
    }
    return sign | (uint64_t(e - 127 + 1023) << 52) | (uint64_t(m) << 29);
}
// a >> n, the bits shifted out kept as the lowest bit (a sticky bit)
uint64_t shr_jam(uint64_t a, int n) {
    if (n <= 0) return a;
    if (n >= 63) return a != 0ul ? 1ul : 0ul;
    return (a >> n) | (((a << (64 - n)) != 0ul) ? 1ul : 0ul);
}
// a * b, rounded toward zero
uint64_t dmulz(uint64_t a, uint64_t b) {
    uint64_t sign = (a ^ b) & D_SIGN;
    int ea = int((a >> 52) & 0x7fful), eb = int((b >> 52) & 0x7fful);
    if (ea == 0 || eb == 0) return sign;
    if (ea == 2047 || eb == 2047) return sign | 0x7ff0000000000000ul;
    uint64_t ma = (a & D_FRAC) | D_HIDDEN, mb = (b & D_FRAC) | D_HIDDEN;
    // the 106-bit product from 32-bit halves
    uint64_t al = ma & 0xfffffffful, ah = ma >> 32, bl = mb & 0xfffffffful, bh = mb >> 32;
    uint64_t ll = al * bl, lh = al * bh, hl = ah * bl, hh = ah * bh;
    uint64_t mid = (ll >> 32) + (lh & 0xfffffffful) + (hl & 0xfffffffful);
    uint64_t lo = (ll & 0xfffffffful) | (mid << 32);
    uint64_t hi = hh + (lh >> 32) + (hl >> 32) + (mid >> 32);
    int e = ea + eb - 1023;
    uint64_t mant;
    if ((hi & (1ul << 41)) != 0ul) {  // (the product's bit 105)
        mant = (hi << 11) | (lo >> 53);
        e += 1;
    } else {
        mant = (hi << 12) | (lo >> 52);
    }
    return d_pack(sign, e, mant);
}
// a + b, rounded toward zero (SoftFloat's f64 add with 10 guard bits and a sticky bit)
uint64_t daddz(uint64_t a, uint64_t b) {
    if ((a & ~D_SIGN) < (b & ~D_SIGN)) { uint64_t t = a; a = b; b = t; }
    int ea = int((a >> 52) & 0x7fful), eb = int((b >> 52) & 0x7fful);
    if (eb == 0) return ea == 0 ? (a & b & D_SIGN) : a;
    if (ea == 2047) return a;
    uint64_t sign = a & D_SIGN;
    uint64_t ma = ((a & D_FRAC) | D_HIDDEN) << 10, mb = shr_jam(((b & D_FRAC) | D_HIDDEN) << 10, ea - eb);
    int e = ea;
    uint64_t m;
    if (((a ^ b) & D_SIGN) == 0ul) {
        m = ma + mb;
        if ((m & D_SIGN) != 0ul) { m = shr_jam(m, 1); e += 1; }
    } else {
        m = ma - mb;
        if (m == 0ul) return 0ul;
        while ((m & (1ul << 62)) == 0ul) { m <<= 1; e -= 1; }
    }
    return d_pack(sign, e, m >> 10);
}
float d2fz(uint64_t d) {
    uint hi = uint(d >> 32), lo = uint(d & 0xfffffffful);
    uint sign = hi & 0x80000000u;
    int e = int((hi >> 20) & 0x7ffu) - 1023 + 127;
    if (e <= 0) return uintBitsToFloat(sign);
    if (e >= 255) return uintBitsToFloat(sign | 0x7f7fffffu);
    return uintBitsToFloat(sign | uint(e) << 23 | (hi & 0xfffffu) << 3 | lo >> 29);
}
"""


def metal_tfrag(src: str) -> str:
    """The tfrag compute shader with its doubles as 64-bit integers."""
    start = src.index("double dstep(double s)")
    end = src.index("uvec4 at(uint a)")
    src = src[:start] + SOFT_DOUBLE.lstrip("\n") + "\n" + src[end:]
    src = src.replace("#version 460\n", "#version 460\n#extension GL_ARB_gpu_shader_int64 : require\n", 1)
    src = src.replace("readonly buffer Views { double views[]; };", "readonly buffer Views { uint64_t views[]; };")
    src = src.replace("double h0 = double(hp.x), h1 = double(hp.y), h2 = double(hp.z), h3 = double(hp.w);",
                      "uint64_t h0 = f2d(hp.x), h1 = f2d(hp.y), h2 = f2d(hp.z), h3 = f2d(hp.w);")
    src = re.sub(r"\bdouble (x[0-3]) = ", r"uint64_t \1 = ", src)
    # (its triangles written last vertex first, as the Metal renderer draws a fragment shader's flat inputs: FLAT_MARK)
    tri = "else { outi[t] = c - 2u; outi[t + 1u] = c - 1u; outi[t + 2u] = c; }"
    if tri not in src:
        raise SystemExit("gen_metal_shaders: the tfrag shader's triangle write not found (gen_vk_shaders.py changed?)")
    src = src.replace(tri, "else { outi[t] = c; outi[t + 1u] = c - 2u; outi[t + 2u] = c - 1u; }")
    if re.search(r"\bdouble\b", re.sub(r"//[^\n]*", "", src)):
        raise SystemExit("gen_metal_shaders: the tfrag shader still has a double (gen_vk_shaders.py changed?)")
    return src


def metal_source(name: str, stage: str, src: str) -> str:
    if name == "kMrpTfragCs":
        return metal_tfrag(src)
    if WORLD_TEXS_GL in src:
        return src.replace(WORLD_TEXS_GL, WORLD_TEXS_MTL)
    return src


# ---- the resources of a GLSL shader: (set, binding) -> the name SPIRV-Cross gives it in MSL ----
DECL = re.compile(r"layout\s*\(([^)]*)\)\s*((?:readonly|writeonly|coherent|restrict|\s)*)\s*(uniform|buffer)\s+(\w+)(\s*\w+)?\s*(\[\s*\d*\s*\])?\s*([;{])")


def resources(src: str):
    out = []
    push = None
    for m in DECL.finditer(src):
        quals = m.group(1)
        kind_kw, t, var, brace = m.group(3), m.group(4), (m.group(5) or "").strip(), m.group(7)
        if "push_constant" in quals:
            push = t
            continue
        sm = re.search(r"set\s*=\s*(\d+)", quals)
        bm = re.search(r"binding\s*=\s*(\d+)", quals)
        if bm is None:
            continue
        s = int(sm.group(1)) if sm else 0
        b = int(bm.group(1))
        if brace == "{":  # (a block: found in MSL by its type's name)
            out.append((s, b, "block", t))
        else:  # (a sampler, texture or image: by its variable's name)
            out.append((s, b, "image" if t.startswith(("image", "uimage", "iimage")) else "sampler", var))
    return out, push


SIG_ARG = re.compile(r"(?:constant|device|const device)?\s*([\w:<>, ]+?)&?\s+(\w+)\s*\[\[(buffer|texture|sampler)\((\d+)\)\]\]")


def msl_signature(msl: str, entry: str):
    i = msl.index(f" {entry}(")
    j = msl.index("{", i)
    sig = msl[i:j]
    args = {}
    for part in re.split(r",\s*(?![^<>]*>)", sig[sig.index("(") + 1:sig.rindex(")")]):
        m = re.search(r"([\w:<>, ]+?)[&*]?\s+(\w+)\s*\[\[(buffer|texture|sampler)\((\d+)\)\]\]", part)
        if m:
            args[m.group(2)] = (m.group(3), int(m.group(4)), m.group(1).strip())
    return args


# ---- the provoking vertex: Vulkan's flat inputs are the primitive's LAST vertex's, Metal's its first's (and Metal has no
# switch for it). The rendering modules write a primitive's flat values on its last vertex alone (the world's CPU path: its
# texture word, its GS state), so a fragment shader with flat inputs (`flat`, and every integer input) is marked here, and
# the Metal renderer draws its triangles' indices turned (a, b, c) -> (c, a, b): the same winding, the last vertex first
# (runtime/mtl/mtl_context.mm). The tfrag compute shader writes its triangles turned so (metal_tfrag).
FLAT_MARK = "// rac1: flat inputs (the triangles drawn last vertex first)\n"
IN_FLAT = re.compile(r"\[\[user\(locn\d+\), flat\]\]|^\s*(?:u?int|u?short|u?char|bool)\d? \w+ \[\[user\(locn\d+\)", re.M)


def has_flat_inputs(name: str, msl: str) -> bool:
    m = re.search(rf"struct {name}_in\s*\{{(.*?)\n\}};", msl, re.S)
    return m is not None and IN_FLAT.search(m.group(1)) is not None


# ---- a compute shader's workgroup size: Metal takes it at the dispatch, not from the shader; SPIRV-Cross writes it into the
# MSL only when the shader reads gl_WorkGroupSize, so it is written here, from the GLSL's layout, for mtl_context.mm to read.
LOCAL_SIZE = re.compile(r"layout\s*\(([^)]*local_size_[xyz][^)]*)\)\s*in\s*;")


def local_size_mark(name: str, glsl: str) -> str:
    m = LOCAL_SIZE.search(glsl)
    if m is None:
        raise SystemExit(f"gen_metal_shaders: {name}: no layout(local_size_x ...) in the compute shader")
    size = {"x": 1, "y": 1, "z": 1}
    for axis, n in re.findall(r"local_size_([xyz])\s*=\s*(\d+)", m.group(1)):
        size[axis] = int(n)
    return f"// rac1: local size {size['x']} {size['y']} {size['z']}\n"


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"gen_metal_shaders: {' '.join(cmd)}\n{r.stdout}{r.stderr}")
    return r.stdout


def translate(name: str, stage: str, src: str, td: str):
    glsl = os.path.join(td, f"{name}.{stage}")
    spv = os.path.join(td, f"{name}.spv")
    msl_path = os.path.join(td, f"{name}.metal")
    with open(glsl, "w", encoding="utf-8") as f:
        f.write(src)
    run([GLSLC, "--target-env=vulkan1.3", f"-fshader-stage={stage}", glsl, "-o", spv])
    cmd = [SPIRV_CROSS, "--msl", "--msl-version", "30000", "--rename-entry-point", "main", name, stage]
    if stage == "vert":
        cmd += ["--flip-vert-y", "--fixup-clipspace"]
    if "set = 2, binding = 0) uniform sampler2DArray uTexs" in src:
        cmd += ["--msl-argument-buffers", "--msl-discrete-descriptor-set", "0", "--msl-discrete-descriptor-set", "1"]
    cmd += ["--output", msl_path, spv]
    run(cmd)
    with open(msl_path, encoding="utf-8") as f:
        msl = f.read()
    if stage == "frag" and has_flat_inputs(name, msl):
        msl = FLAT_MARK + msl
    if stage == "comp":
        msl = local_size_mark(name, src) + msl
    # (the source as glslc preprocessed it: the resources of the branches not taken left out)
    pre = run([GLSLC, "--target-env=vulkan1.3", f"-fshader-stage={stage}", "-E", glsl])
    return msl, pre


KIND = {"buffer": "kBuffer", "texture": "kTexture", "sampler": "kSampler"}


def write_if_changed(path: str, text: str) -> None:
    """Writes text to path unless it is already there (the build then rebuilds nothing)."""
    if os.path.exists(path):
        with open(path, encoding="utf-8") as f:
            if f.read() == text:
                return
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def main():
    global GLSLC, SPIRV_CROSS
    if len(sys.argv) < 2:
        print("usage: gen_metal_shaders.py <out_dir> [<glslc> <spirv-cross>]", file=sys.stderr)
        sys.exit(2)
    out_dir = sys.argv[1]
    if len(sys.argv) > 3:
        GLSLC, SPIRV_CROSS = sys.argv[2], sys.argv[3]
    shaders_dir = os.path.join(out_dir, "shaders")
    os.makedirs(shaders_dir, exist_ok=True)
    sources = ["// Made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py at build time - not in the repository."]
    table = [
        "// Made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py at build time - not in the repository.",
        "// For each shader: where each Vulkan resource (set, binding) lands in Metal, and the push constants' buffer.",
        "// {set, binding, kind, index}; a combined image sampler has a kTexture and a kSampler entry.",
        "",
    ]
    with tempfile.TemporaryDirectory() as td:
        for name, stage, src in vk.SHADERS:
            msrc = metal_source(name, stage, src)
            msl, pre = translate(name, stage, msrc, td)
            res, push = resources(pre)
            args = msl_signature(msl, name)
            entries = []
            for s, b, kind, ident in res:
                if kind == "block":
                    hits = [(k, i) for a, (k, i, t) in args.items() if k == "buffer" and re.search(rf"\b{ident}\b", t)]
                    for k, i in hits[:1]:
                        entries.append((s, b, k, i))
                elif s == 2:
                    continue  # (in the argument buffer)
                else:
                    if ident in args:
                        entries.append((s, b, args[ident][0], args[ident][1]))
                    if ident + "Smplr" in args:
                        entries.append((s, b, "sampler", args[ident + "Smplr"][1]))
            push_idx = -1
            if push is not None:
                for a, (k, i, t) in args.items():
                    if k == "buffer" and re.search(rf"\b{push}\b", t):
                        push_idx = i
            argbuf = -1
            for a, (k, i, t) in args.items():
                if "spvDescriptorSetBuffer2" in t:
                    argbuf = i
            used = [i for (_, _, k, i) in entries if k == "buffer"] + ([push_idx] if push_idx >= 0 else []) + ([argbuf] if argbuf >= 0 else [])
            if used and max(used) >= VERTEX_BUFFER_BASE - VERTEX_BUFFER_MAX + 1:
                raise SystemExit(f"gen_metal_shaders: {name} uses buffer {max(used)}, which a vertex buffer may take")
            # every resource of the GLSL that is used must have been found
            found = {(s, b) for s, b, k, i in entries}
            for s, b, kind, ident in res:
                if s == 2:
                    continue
                if (s, b) not in found and re.search(rf"\b{ident}\b", msl):
                    raise SystemExit(f"gen_metal_shaders: {name}: {ident} (set {s}, binding {b}) not found in the MSL signature")
            text = f"// {name} ({stage}): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.\n" + msl
            if ")rac1src" in text:
                raise SystemExit(f"gen_metal_shaders: {name}: the raw string's delimiter is in the MSL")
            write_if_changed(os.path.join(shaders_dir, f"{name}.metal"), text)
            sources.append(f'{{"{name}", R"rac1src({text})rac1src"}},')
            ent = ", ".join(f"{{{s}, {b}, {KIND[k]}, {i}}}" for s, b, k, i in sorted(entries))
            table.append(f"RAC1_MTL_SHADER({name}, \"{stage}\", {push_idx}, {argbuf}, {{{ent}}})")
    table.append("")
    sources.append("")
    write_if_changed(os.path.join(out_dir, "mtl_shader_table.inc"), "\n".join(table))
    write_if_changed(os.path.join(out_dir, "mtl_shader_sources.inc"), "\n".join(sources))
    print(f"[metal] {len(vk.SHADERS)} shaders made with {os.path.basename(GLSLC)} and {os.path.basename(SPIRV_CROSS)}")


if __name__ == "__main__":
    main()
