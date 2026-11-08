// See level_compose.h. Not a port: our own tool. The data file's and gameplay file's layouts are the game's (src/formats/
// level_file.h, level_gameplay.h, level_collision.h); which parts of the base level are kept follows what the game's
// level setup (src/game/ovl/level_setup.cpp, level_load.cpp) reads.
#include "level_compose.h"

#include "mrp_export.h"

#include <formats/host_meshes.h>
#include <formats/level_file.h>
#include <formats/moby_mrp.h>
#include <formats/scene_wad.h>
#include <formats/wad_encode.h>
#include <formats/world_mrp.h>
#include <runtime/kernel/mods.h>

#include <stb_image.h>  // (its implementation: static_skinned_render.cpp)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace rac1::setup {

namespace fs = std::filesystem;

namespace {

// level.txt's settings (format 1), as the docs list them
[[maybe_unused]] constexpr const char* kLevelTxtHelp =
    "name <text> / name.<fr|de|es|it> <text>   the ship's list's row and the planet page's title\n"
    "region <text> / region.<lang> <text>      the line above it\n"
    "hero <x> <y> <z> [<yaw>]                  where Ratchet starts (game units, z up; yaw in radians)\n"
    "ship <x> <y> <z> <yaw> | ship none        where the ship stands (none: no way out but the pause menu)\n"
    "death_height <z>                          below it Ratchet dies\n"
    "background <r> <g> <b>                    the sky's colour behind everything (0..255)\n"
    "fog <r> <g> <b>                           the fog's colour (0..255)\n"
    "collision <file>                          the collision mesh (default collision.obj)\n"
    "collision_type <n>                        the surface type of faces with no `usemtl type<N>` (default 31)\n"
    "meshes <file>                             the drawn meshes (default meshes.obj; optional)\n"
    "up z | up y                               the OBJ files' up axis (default z; y: Blender's default OBJ export)\n"
    "scale <s>                                 the OBJ files' units to game units (default 1)\n"
    "moby <class> <x> <y> <z> [<yaw>]          an object of the base level's class <class>, as its first placement there\n"
    "music <level> | music <loop.vag> [<intro.vag>] | music none   the level's music\n"
    "sky <level>                               the sky of the game's level <level> (default: the base level's)\n"
    "offset auto | offset <x> <y> <z>          moved by that much (auto: as far as the collision needs to be above 0)\n";

struct LevelTxt {
    float hero[4] = {0, 0, 1, 0};
    bool has_hero = false;
    bool ship = false;
    float ship_at[4] = {};
    bool has_death = false;
    float death = -100.0f;
    bool has_background = false, has_fog = false;
    int background[3] = {}, fog[3] = {};
    std::string collision = "collision.obj", meshes = "meshes.obj";
    int collision_type = 31;
    bool y_up = false;
    float scale = 1.0f;
    struct Moby {
        int cls = 0;
        float at[4] = {};  // x y z yaw
        int line = 0;
    };
    std::vector<Moby> mobys;
    int music_level = -1;               // music <level>: that level's tracks
    bool music_none = false;            // music none
    std::string music_loop, music_intro;  // music <loop.vag> [<intro.vag>]: the mod's own
    int sky_level = -1;                 // sky <level>
    bool offset_auto = false;           // offset auto
    float offset[3] = {};               // offset <x> <y> <z> (or auto's): added to every coordinate (not lights.txt's)
};

struct Log {
    std::string text;
    void say(const std::string& s) {
        text += s + "\n";
        std::cout << "[compose] " << s << "\n";
    }
};

std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));
std::string fmt(const char* f, ...) {
    char b[512];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(b, sizeof b, f, ap);
    va_end(ap);
    return b;
}

bool read_file(const fs::path& p, std::vector<uint8_t>& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

bool write_file(const fs::path& p, const std::vector<uint8_t>& b) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
    return static_cast<bool>(f);
}

int32_t rd32(const std::vector<uint8_t>& b, size_t at) {
    int32_t v = 0;
    if (at + 4 <= b.size()) std::memcpy(&v, b.data() + at, 4);
    return v;
}
void wr32(std::vector<uint8_t>& b, size_t at, int32_t v) {
    if (at + 4 > b.size()) b.resize(at + 4, 0);
    std::memcpy(b.data() + at, &v, 4);
}
void wrf(std::vector<uint8_t>& b, size_t at, float v) {
    int32_t i;
    std::memcpy(&i, &v, 4);
    wr32(b, at, i);
}
size_t align_up(size_t v, size_t a) { return (v + a - 1) / a * a; }

// ---- level.txt ----
bool read_level_txt(const fs::path& p, LevelTxt& t, std::string& err) {
    std::ifstream f(p);
    if (!f) {
        err = p.generic_string() + " cannot be read";
        return false;
    }
    std::string line;
    int n = 0;
    while (std::getline(f, line)) {
        ++n;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        const size_t h = line.find_first_not_of(" \t");
        if (h == std::string::npos || line[h] == '#') continue;
        std::istringstream in(line);
        std::string key;
        in >> key;
        auto bad = [&] {
            err = fmt("%s:%d: \"%s\" not understood", p.generic_string().c_str(), n, line.c_str());
            return false;
        };
        if (key.rfind("name", 0) == 0 || key.rfind("region", 0) == 0) continue;  // (the game's: src/game/enhance/level_slots.cpp)
        if (key == "hero") {
            if (!(in >> t.hero[0] >> t.hero[1] >> t.hero[2])) return bad();
            if (!(in >> t.hero[3])) t.hero[3] = 0;
            t.has_hero = true;
        } else if (key == "ship") {
            std::string w;
            in >> w;
            if (w == "none") {
                t.ship = false;
            } else {
                std::istringstream in2(line.substr(line.find(key) + key.size()));
                if (!(in2 >> t.ship_at[0] >> t.ship_at[1] >> t.ship_at[2] >> t.ship_at[3])) return bad();
                t.ship = true;
            }
        } else if (key == "death_height") {
            if (!(in >> t.death)) return bad();
            t.has_death = true;
        } else if (key == "background" || key == "fog") {
            int* c = key == "fog" ? t.fog : t.background;
            if (!(in >> c[0] >> c[1] >> c[2])) return bad();
            for (int i = 0; i < 3; ++i) c[i] = std::clamp(c[i], 0, 255);
            (key == "fog" ? t.has_fog : t.has_background) = true;
        } else if (key == "collision") {
            if (!(in >> t.collision)) return bad();
        } else if (key == "collision_type") {
            if (!(in >> t.collision_type) || t.collision_type < 0 || t.collision_type > 255) return bad();
        } else if (key == "meshes") {
            if (!(in >> t.meshes)) return bad();
        } else if (key == "up") {
            std::string a;
            in >> a;
            if (a != "y" && a != "z") return bad();
            t.y_up = a == "y";
        } else if (key == "scale") {
            if (!(in >> t.scale) || !(t.scale > 0)) return bad();
        } else if (key == "moby") {
            LevelTxt::Moby m;
            if (!(in >> m.cls >> m.at[0] >> m.at[1] >> m.at[2]) || m.cls < 0) return bad();
            if (!(in >> m.at[3])) m.at[3] = 0;
            m.line = n;
            t.mobys.push_back(m);
        } else if (key == "music") {
            std::string a, b;
            in >> a >> b;
            t.music_level = -1;
            t.music_none = false;
            t.music_loop.clear();
            t.music_intro.clear();
            if (a == "none") {
                t.music_none = true;
            } else if (!a.empty() && a.find_first_not_of("0123456789") == std::string::npos) {
                t.music_level = std::atoi(a.c_str());
                if (t.music_level > 18) return bad();
            } else if (!a.empty()) {
                t.music_loop = a;
                t.music_intro = b;
            } else {
                return bad();
            }
        } else if (key == "offset") {
            std::string a;
            in >> a;
            if (a == "auto") {
                t.offset_auto = true;
            } else {
                std::istringstream in2(line.substr(line.find(key) + key.size()));
                if (!(in2 >> t.offset[0] >> t.offset[1] >> t.offset[2])) return bad();
                t.offset_auto = false;
            }
        } else if (key == "sky") {
            if (!(in >> t.sky_level) || t.sky_level < 0 || t.sky_level > 18) return bad();
        } else {
            return bad();
        }
    }
    if (!t.has_hero) {
        err = p.generic_string() + ": no \"hero <x> <y> <z>\" line (where Ratchet starts)";
        return false;
    }
    return true;
}

// ---- OBJ ----
struct ObjVert {
    float p[3];
    float uv[2];
    float col[3];  // its colour (an OBJ's "v x y z r g b"; 1 1 1 without)
};
struct ObjTri {
    ObjVert v[3];
    int material = -1;  // index into Obj::materials (-1: none)
    int object = 0;     // index into Obj::objects
};
struct ObjMaterial {
    std::string name;
    float kd[3] = {0.8f, 0.8f, 0.8f};
    std::string map;  // map_Kd's file (relative to the .mtl)
    fs::path dir;
};
struct Obj {
    std::vector<ObjTri> tris;
    std::vector<ObjMaterial> materials;
    std::vector<std::string> objects = {"(default)"};
};

int obj_index(const std::string& s, size_t n) {
    if (s.empty()) return -1;
    const int i = std::atoi(s.c_str());
    if (i > 0) return i - 1;
    if (i < 0) return static_cast<int>(n) + i;
    return -1;
}

bool read_mtl(const fs::path& p, Obj& o, std::string& err) {
    std::ifstream f(p);
    if (!f) {
        err = p.generic_string() + " cannot be read";
        return false;
    }
    std::string line;
    ObjMaterial* cur = nullptr;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        std::istringstream in(line);
        std::string k;
        in >> k;
        if (k == "newmtl") {
            ObjMaterial m;
            std::getline(in >> std::ws, m.name);
            m.dir = p.parent_path();
            o.materials.push_back(m);
            cur = &o.materials.back();
        } else if (cur != nullptr && k == "Kd") {
            in >> cur->kd[0] >> cur->kd[1] >> cur->kd[2];
        } else if (cur != nullptr && k == "map_Kd") {
            std::string rest;
            std::getline(in >> std::ws, rest);
            const size_t sp = rest.find_last_of(' ');  // (options before the file's name)
            cur->map = sp == std::string::npos ? rest : rest.substr(sp + 1);
        }
    }
    return true;
}

// reads an OBJ (positions converted to game units and axes)
bool read_obj(const fs::path& p, const LevelTxt& lt, bool with_materials, Obj& o, std::string& err) {
    std::ifstream f(p);
    if (!f) {
        err = p.generic_string() + " cannot be read";
        return false;
    }
    std::vector<std::array<float, 3>> vs, cols;
    std::vector<std::array<float, 2>> vts;
    std::string line;
    int material = -1, object = 0;
    int n = 0;
    while (std::getline(f, line)) {
        ++n;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        std::istringstream in(line);
        std::string k;
        in >> k;
        if (k == "v") {
            float x = 0, y = 0, z = 0;
            in >> x >> y >> z;
            if (lt.y_up) vs.push_back({x * lt.scale + lt.offset[0], -z * lt.scale + lt.offset[1], y * lt.scale + lt.offset[2]});
            else vs.push_back({x * lt.scale + lt.offset[0], y * lt.scale + lt.offset[1], z * lt.scale + lt.offset[2]});
            float r = 1, g = 1, b = 1;
            if (!(in >> r >> g >> b)) r = g = b = 1;
            cols.push_back({std::clamp(r, 0.0f, 1.0f), std::clamp(g, 0.0f, 1.0f), std::clamp(b, 0.0f, 1.0f)});
        } else if (k == "vt") {
            float u = 0, v = 0;
            in >> u >> v;
            vts.push_back({u, v});
        } else if (k == "mtllib" && with_materials) {
            std::string name;
            std::getline(in >> std::ws, name);
            if (!read_mtl(p.parent_path() / name, o, err)) return false;
        } else if (k == "usemtl") {
            std::string name;
            std::getline(in >> std::ws, name);
            material = -1;
            for (size_t i = 0; i < o.materials.size(); ++i)
                if (o.materials[i].name == name) material = static_cast<int>(i);
            if (material < 0) {  // (a name with no .mtl entry: kept by name, its default colour)
                ObjMaterial m;
                m.name = name;
                o.materials.push_back(m);
                material = static_cast<int>(o.materials.size()) - 1;
            }
        } else if (k == "o" || k == "g") {
            std::string name;
            std::getline(in >> std::ws, name);
            o.objects.push_back(name);
            object = static_cast<int>(o.objects.size()) - 1;
        } else if (k == "f") {
            std::vector<ObjVert> poly;
            std::string w;
            while (in >> w) {
                const size_t s1 = w.find('/');
                const int vi = obj_index(w.substr(0, s1), vs.size());
                int ti = -1;
                if (s1 != std::string::npos) {
                    const size_t s2 = w.find('/', s1 + 1);
                    ti = obj_index(w.substr(s1 + 1, s2 == std::string::npos ? std::string::npos : s2 - s1 - 1), vts.size());
                }
                if (vi < 0 || vi >= static_cast<int>(vs.size()) || ti >= static_cast<int>(vts.size())) {
                    err = fmt("%s:%d: a face's vertex that is not there", p.generic_string().c_str(), n);
                    return false;
                }
                ObjVert v{};
                std::memcpy(v.p, vs[static_cast<size_t>(vi)].data(), sizeof v.p);
                std::memcpy(v.col, cols[static_cast<size_t>(vi)].data(), sizeof v.col);
                if (ti >= 0) std::memcpy(v.uv, vts[static_cast<size_t>(ti)].data(), sizeof v.uv);
                poly.push_back(v);
            }
            for (size_t i = 2; i < poly.size(); ++i) {  // (a fan)
                ObjTri t;
                t.v[0] = poly[0];
                t.v[1] = poly[i - 1];
                t.v[2] = poly[i];
                t.material = material;
                t.object = object;
                o.tris.push_back(t);
            }
        }
    }
    return true;
}

// ---- the collision (level_collision.h's layout) ----
struct CTri {
    std::array<std::array<float, 3>, 3> p;
    uint8_t type = 31;
};

float dist3(const std::array<float, 3>& a, const std::array<float, 3>& b) {
    return std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
}

// triangles split until no edge is longer than `max_edge` (a cell's points lie within 32 units of its centre)
void subdivide(const CTri& t, float max_edge, std::vector<CTri>& out, int depth = 0) {
    float e = 0;
    for (int i = 0; i < 3; ++i) e = std::max(e, dist3(t.p[static_cast<size_t>(i)], t.p[static_cast<size_t>((i + 1) % 3)]));
    if (e <= max_edge || depth > 16) {
        out.push_back(t);
        return;
    }
    std::array<std::array<float, 3>, 3> m;
    for (int i = 0; i < 3; ++i)
        for (int l = 0; l < 3; ++l) m[static_cast<size_t>(i)][static_cast<size_t>(l)] = 0.5f * (t.p[static_cast<size_t>(i)][static_cast<size_t>(l)] + t.p[static_cast<size_t>((i + 1) % 3)][static_cast<size_t>(l)]);
    const CTri parts[4] = {{{t.p[0], m[0], m[2]}, t.type}, {{m[0], t.p[1], m[1]}, t.type}, {{m[2], m[1], t.p[2]}, t.type}, {{m[0], m[1], m[2]}, t.type}};
    for (const CTri& c : parts) subdivide(c, max_edge, out, depth + 1);
}

// a triangle against an axis-aligned box (separating axes: the box's, the triangle's normal, the nine edge crosses)
bool tri_box(const CTri& t, const float c[3], float h) {
    float v[3][3];
    for (int i = 0; i < 3; ++i)
        for (int l = 0; l < 3; ++l) v[i][l] = t.p[static_cast<size_t>(i)][static_cast<size_t>(l)] - c[l];
    for (int l = 0; l < 3; ++l) {
        const float lo = std::min({v[0][l], v[1][l], v[2][l]}), hi = std::max({v[0][l], v[1][l], v[2][l]});
        if (lo > h || hi < -h) return false;
    }
    float e[3][3];
    for (int i = 0; i < 3; ++i)
        for (int l = 0; l < 3; ++l) e[i][l] = v[(i + 1) % 3][l] - v[i][l];
    auto test = [&](const float a[3]) {
        const float p0 = a[0] * v[0][0] + a[1] * v[0][1] + a[2] * v[0][2];
        const float p1 = a[0] * v[1][0] + a[1] * v[1][1] + a[2] * v[1][2];
        const float p2 = a[0] * v[2][0] + a[1] * v[2][1] + a[2] * v[2][2];
        const float r = h * (std::fabs(a[0]) + std::fabs(a[1]) + std::fabs(a[2]));
        return !(std::min({p0, p1, p2}) > r || std::max({p0, p1, p2}) < -r);
    };
    const float n[3] = {e[0][1] * e[1][2] - e[0][2] * e[1][1], e[0][2] * e[1][0] - e[0][0] * e[1][2], e[0][0] * e[1][1] - e[0][1] * e[1][0]};
    if (!test(n)) return false;
    for (int i = 0; i < 3; ++i) {
        const float ax[3][3] = {{0, -e[i][2], e[i][1]}, {e[i][2], 0, -e[i][0]}, {-e[i][1], e[i][0], 0}};
        for (const auto& a : ax)
            if (!test(a)) return false;
    }
    return true;
}

struct CCell {
    std::vector<uint32_t> points;                // the packed words
    std::vector<std::array<uint8_t, 4>> faces;   // three point indices, the type
};

bool build_collision(const std::vector<CTri>& in, std::vector<uint8_t>& out, Log& log, std::string& err) {
    std::vector<CTri> tris;
    for (const CTri& t : in) subdivide(t, 6.0f, tris);
    std::map<std::tuple<int, int, int>, CCell> cells;  // (z, y, x)
    size_t refs = 0;
    for (const CTri& t : tris) {
        int lo[3], hi[3];
        for (int l = 0; l < 3; ++l) {
            const float a = std::min({t.p[0][static_cast<size_t>(l)], t.p[1][static_cast<size_t>(l)], t.p[2][static_cast<size_t>(l)]});
            const float b = std::max({t.p[0][static_cast<size_t>(l)], t.p[1][static_cast<size_t>(l)], t.p[2][static_cast<size_t>(l)]});
            lo[l] = static_cast<int>(std::floor((a - 0.01f) / 4.0f));
            hi[l] = static_cast<int>(std::floor((b + 0.01f) / 4.0f));
            if (lo[l] < 0) {  // (the game finds a point's cell by truncating, so the grid's cells below 0 are never looked at)
                err = fmt("the collision reaches %c %.2f: the game's collision grid starts at 0, every coordinate must be above 0 (move the "
                          "level, its start and its ship there)", "xyz"[l], a);
                return false;
            }
        }
        for (int z = lo[2]; z <= hi[2]; ++z)
            for (int y = lo[1]; y <= hi[1]; ++y)
                for (int x = lo[0]; x <= hi[0]; ++x) {
                    const float c[3] = {x * 4.0f + 2.0f, y * 4.0f + 2.0f, z * 4.0f + 2.0f};
                    if (!tri_box(t, c, 2.01f)) continue;
                    if (std::abs(x) > 0x7ff0 || std::abs(y) > 0x7ff0 || std::abs(z) > 0x7ff0) {
                        err = "the collision reaches past the game's grid (32000 cells of 4 units)";
                        return false;
                    }
                    CCell& cell = cells[{z, y, x}];
                    std::array<uint8_t, 4> face{};
                    bool degenerate = false;
                    for (int i = 0; i < 3; ++i) {
                        const auto& p = t.p[static_cast<size_t>(i)];
                        const int qx = static_cast<int>(std::lround((p[0] - c[0]) * 16.0f));
                        const int qy = static_cast<int>(std::lround((p[1] - c[1]) * 16.0f));
                        const int qz = static_cast<int>(std::lround((p[2] - c[2]) * 64.0f));
                        if (std::abs(qx) > 511 || std::abs(qy) > 511 || std::abs(qz) > 2047) {
                            err = "a collision point past a cell's reach (internal)";
                            return false;
                        }
                        const uint32_t w = (static_cast<uint32_t>(qx) & 0x3ffu) | (static_cast<uint32_t>(qy) & 0x3ffu) << 10 |
                                           (static_cast<uint32_t>(qz) & 0xfffu) << 20;
                        auto it = std::find(cell.points.begin(), cell.points.end(), w);
                        if (it == cell.points.end()) {
                            cell.points.push_back(w);
                            it = cell.points.end() - 1;
                        }
                        face[static_cast<size_t>(i)] = static_cast<uint8_t>(it - cell.points.begin());
                    }
                    degenerate = face[0] == face[1] || face[1] == face[2] || face[0] == face[2];
                    if (degenerate) continue;
                    // the game's faces turn clockwise seen from the side they face (an OBJ's turn counter-clockwise)
                    std::swap(face[1], face[2]);
                    face[3] = t.type;
                    cell.faces.push_back(face);
                    ++refs;
                    if (cell.points.size() > 255 || cell.faces.size() > 0xffff) {
                        err = fmt("the collision has too many points in the cell at (%d, %d, %d): over 255", x * 4, y * 4, z * 4);
                        return false;
                    }
                }
    }
    if (cells.empty()) {
        err = "the collision mesh has no faces";
        return false;
    }
    // the layout: the head (the grid's offset, the hero groups' offset), the grid at 0x40: z0, zn, the z rows' offsets / 4;
    // the z rows (y0, yn, the y rows' offsets), the y rows (x0, xn, the cells' offsets << 8 | their size / 16), the cells
    // (16-byte aligned), then the hero groups' list (empty)
    std::map<int, std::map<int, std::map<int, const CCell*>>> grid;
    for (auto& [k, c] : cells) {
        if (c.faces.empty()) continue;
        grid[std::get<0>(k)][std::get<1>(k)][std::get<2>(k)] = &c;
    }
    const int z0 = grid.begin()->first, z1 = grid.rbegin()->first;
    std::vector<uint8_t> g;
    auto put16 = [&g](size_t at, uint32_t v) {
        if (at + 2 > g.size()) g.resize(at + 2, 0);
        g[at] = static_cast<uint8_t>(v);
        g[at + 1] = static_cast<uint8_t>(v >> 8);
    };
    auto put32 = [&g](size_t at, uint32_t v) {
        if (at + 4 > g.size()) g.resize(at + 4, 0);
        for (int i = 0; i < 4; ++i) g[at + static_cast<size_t>(i)] = static_cast<uint8_t>(v >> (8 * i));
    };
    const int zn = z1 - z0 + 1;
    put16(0, static_cast<uint32_t>(static_cast<int16_t>(z0)) & 0xffff);
    put16(2, static_cast<uint32_t>(zn));
    size_t at = align_up(4 + 2 * static_cast<size_t>(zn), 4);
    std::map<int, size_t> zrow_at;
    for (auto& [z, ys] : grid) {  // the z rows
        zrow_at[z] = at;
        const int y0 = ys.begin()->first, yn = ys.rbegin()->first - y0 + 1;
        if (at / 4 > 0xffff) {
            err = "the collision's grid is too large (its z rows past 256 KB)";
            return false;
        }
        put16(4 + 2 * static_cast<size_t>(z - z0), static_cast<uint32_t>(at / 4));
        put16(at, static_cast<uint32_t>(static_cast<int16_t>(y0)) & 0xffff);
        put16(at + 2, static_cast<uint32_t>(yn));
        at = align_up(at + 4 + 4 * static_cast<size_t>(yn), 4);
    }
    std::map<std::pair<int, int>, size_t> yrow_at;
    for (auto& [z, ys] : grid)  // the y rows
        for (auto& [y, xs] : ys) {
            yrow_at[{z, y}] = at;
            const int x0 = xs.begin()->first, xn = xs.rbegin()->first - x0 + 1;
            put32(zrow_at[z] + 4 + 4 * static_cast<size_t>(y - ys.begin()->first), static_cast<uint32_t>(at));
            put16(at, static_cast<uint32_t>(static_cast<int16_t>(x0)) & 0xffff);
            put16(at + 2, static_cast<uint32_t>(xn));
            at = align_up(at + 4 + 4 * static_cast<size_t>(xn), 4);
        }
    size_t faces = 0, points = 0, ncells = 0;
    for (auto& [z, ys] : grid)  // the cells
        for (auto& [y, xs] : ys)
            for (auto& [x, c] : xs) {
                at = align_up(at, 16);
                const size_t size = 4 + 4 * c->points.size() + 4 * c->faces.size();
                if (at > 0xffffff) {
                    err = "the collision is too large (past 16 MB)";
                    return false;
                }
                put32(yrow_at[{z, y}] + 4 + 4 * static_cast<size_t>(x - xs.begin()->first), static_cast<uint32_t>(at << 8 | ((size + 15) / 16)));
                put16(at, static_cast<uint32_t>(c->faces.size()));
                g.resize(std::max(g.size(), at + size), 0);
                g[at + 2] = static_cast<uint8_t>(c->points.size());
                g[at + 3] = 0;  // (no quads)
                size_t p = at + 4;
                for (uint32_t w : c->points) put32(p, w), p += 4;
                for (const auto& f : c->faces) {
                    std::memcpy(g.data() + p, f.data(), 4);
                    p += 4;
                }
                at += size;
                faces += c->faces.size();
                points += c->points.size();
                ++ncells;
            }
    at = align_up(at, 16);
    const size_t heroes = at;
    g.resize(heroes + 0x10, 0);  // (the hero groups' list: none)
    out.assign(0x40, 0);
    wr32(out, 0, 0x40);
    wr32(out, 4, static_cast<int32_t>(0x40 + heroes));
    out.insert(out.end(), g.begin(), g.end());
    log.say(fmt("collision: %zu triangles (%zu after splitting), %zu cells, %zu faces (%zu references), %zu points, %zu bytes", in.size(), tris.size(), ncells, faces,
                refs, points, out.size()));
    return true;
}

// ---- the core (the new asset data) ----
struct Core {
    std::vector<uint8_t> buf;
    struct Seg {
        size_t lo, hi, at;
    };
    std::vector<Seg> segs;
    size_t put(const uint8_t* b, size_t n, size_t a = 0x100) {
        const size_t at = align_up(buf.size(), a);
        buf.resize(at, 0);
        buf.insert(buf.end(), b, b + n);
        return at;
    }
    // the old core's bytes [lo, hi), placed at the same offset modulo 0x1000
    size_t keep(const std::vector<uint8_t>& old, size_t lo, size_t hi) {
        size_t at = align_up(buf.size(), 0x1000) + (lo & 0xfff);
        if (at < buf.size()) at += 0x1000;
        buf.resize(at, 0);
        buf.insert(buf.end(), old.begin() + static_cast<std::ptrdiff_t>(lo), old.begin() + static_cast<std::ptrdiff_t>(hi));
        segs.push_back({lo, hi, at});
        return at;
    }
    bool remap(size_t off, size_t& out) const {
        for (const Seg& s : segs)
            if ((s.lo <= off && off < s.hi) || (off == s.hi && &s == &segs.back())) {
                out = s.at + off - s.lo;
                return true;
            }
        return false;
    }
};

// ---- the host meshes ----
uint32_t csm1(uint32_t i) { return (i & ~0x18u) | ((i & 0x08u) << 1) | ((i & 0x10u) >> 1); }

uint32_t pow2_at_most(int v, uint32_t lo, uint32_t hi) {
    uint32_t p = lo;
    while (p * 2 <= static_cast<uint32_t>(std::max(v, 1)) && p * 2 <= hi) p *= 2;
    return p;
}

// a material's texture: its PNG (to at most 256 x 256, powers of two, 256 colours) or its colour
bool material_texture(const ObjMaterial& m, mrp::Texture& t, Log& log, std::string& err) {
    std::vector<uint32_t> rgba;  // RGBA8 (R lowest)
    uint32_t w = 8, h = 8;
    if (!m.map.empty()) {
        const fs::path p = m.dir / m.map;
        int iw = 0, ih = 0, ch = 0;
        unsigned char* px = stbi_load(p.string().c_str(), &iw, &ih, &ch, 4);
        if (px == nullptr) {
            err = p.generic_string() + ": not a PNG (or JPEG) that can be read";
            return false;
        }
        w = pow2_at_most(iw, 8, 256);
        h = pow2_at_most(ih, 8, 256);
        rgba.resize(static_cast<size_t>(w) * h);
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x) {
                const size_t sx = static_cast<size_t>(x) * static_cast<size_t>(iw) / w, sy = static_cast<size_t>(y) * static_cast<size_t>(ih) / h;
                std::memcpy(&rgba[static_cast<size_t>(y) * w + x], px + (sy * static_cast<size_t>(iw) + sx) * 4, 4);
            }
        stbi_image_free(px);
        if (static_cast<uint32_t>(iw) != w || static_cast<uint32_t>(ih) != h)
            log.say(fmt("  %s: %dx%d scaled to %ux%u (powers of two, at most 256)", m.map.c_str(), iw, ih, w, h));
    } else {
        uint32_t c = 0xff000000u;
        for (int l = 0; l < 3; ++l) c |= static_cast<uint32_t>(std::clamp(static_cast<int>(std::lround(m.kd[l] * 255.0f)), 0, 255)) << (8 * l);
        rgba.assign(static_cast<size_t>(w) * h, c);
    }
    // the palette: the colours themselves when there are 256 at most, else a 6 x 7 x 6 cube; alpha below half: index 255,
    // clear
    std::vector<uint32_t> pal;
    std::map<uint32_t, uint8_t> exact;
    bool fits = true;
    for (uint32_t c : rgba) {
        if ((c >> 24) < 0x80) continue;
        const uint32_t k = c | 0xff000000u;
        if (exact.count(k) == 0) {
            if (exact.size() >= 255) {
                fits = false;
                break;
            }
            const uint8_t i = static_cast<uint8_t>(exact.size());
            exact[k] = i;
        }
    }
    std::vector<uint8_t> idx(rgba.size());
    if (fits) {
        pal.assign(256, 0);
        for (auto& [c, i] : exact) pal[i] = c;
        for (size_t i = 0; i < rgba.size(); ++i) idx[i] = (rgba[i] >> 24) < 0x80 ? 255 : exact[rgba[i] | 0xff000000u];
    } else {
        pal.assign(256, 0);
        for (int r = 0; r < 6; ++r)
            for (int g = 0; g < 7; ++g)
                for (int b = 0; b < 6; ++b)
                    pal[static_cast<size_t>(r * 42 + g * 6 + b)] = 0xff000000u | static_cast<uint32_t>(b * 51) << 16 | static_cast<uint32_t>(g * 255 / 6) << 8 | static_cast<uint32_t>(r * 51);
        for (size_t i = 0; i < rgba.size(); ++i) {
            const uint32_t c = rgba[i];
            if ((c >> 24) < 0x80) {
                idx[i] = 255;
                continue;
            }
            const int r = static_cast<int>(std::lround((c & 0xff) / 51.0)), g = static_cast<int>(std::lround(((c >> 8) & 0xff) * 6 / 255.0)),
                      b = static_cast<int>(std::lround(((c >> 16) & 0xff) / 51.0));
            idx[i] = static_cast<uint8_t>(r * 42 + g * 6 + b);
        }
    }
    t.width = w;
    t.height = h;
    for (uint32_t i = 0; i < 256; ++i) {
        const uint32_t c = pal[i];
        const uint32_t a = i == 255 ? 0u : 0x80u;  // (the GS's alpha: 0x80 opaque)
        t.palette[csm1(i)] = (c & 0x00ffffffu) | a << 24;
    }
    t.mips.clear();
    t.mips.push_back(idx);
    for (uint32_t mw = w / 2, mh = h / 2, s = 2; t.mips.size() < 4 && mw >= 1 && mh >= 1; mw /= 2, mh /= 2, s *= 2) {
        std::vector<uint8_t> m2(static_cast<size_t>(mw) * mh);
        for (uint32_t y = 0; y < mh; ++y)
            for (uint32_t x = 0; x < mw; ++x) m2[static_cast<size_t>(y) * mw + x] = idx[static_cast<size_t>(y * s) * w + x * s];
        t.mips.push_back(std::move(m2));
    }
    return true;
}

bool build_host_meshes(const Obj& o, mrpw::HostMeshFile& out, Log& log, std::string& err) {
    std::vector<int> tex_of(o.materials.size() + 1, -1);  // material (+1, 0 none) -> the file's texture
    auto texture = [&](int material) -> int {
        int& slot = tex_of[static_cast<size_t>(material + 1)];
        if (slot >= 0) return slot;
        mrp::Texture t;
        ObjMaterial none;
        none.name = "(none)";
        if (!material_texture(material >= 0 ? o.materials[static_cast<size_t>(material)] : none, t, log, err)) return -1;
        t.index = static_cast<uint32_t>(out.textures.size());
        out.textures.push_back(std::move(t));
        slot = static_cast<int>(out.textures.size()) - 1;
        return slot;
    };
    size_t total_tris = 0;
    for (size_t obj = 0; obj < o.objects.size(); ++obj) {
        std::vector<const ObjTri*> tris;
        for (const ObjTri& t : o.tris)
            if (t.object == static_cast<int>(obj)) tris.push_back(&t);
        if (tris.empty()) continue;
        std::stable_sort(tris.begin(), tris.end(), [](const ObjTri* a, const ObjTri* b) { return a->material < b->material; });
        float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
        for (const ObjTri* t : tris)
            for (const ObjVert& v : t->v)
                for (int l = 0; l < 3; ++l) lo[l] = std::min(lo[l], v.p[l]), hi[l] = std::max(hi[l], v.p[l]);
        float centre[3], reach = 0;
        for (int l = 0; l < 3; ++l) centre[l] = 0.5f * (lo[l] + hi[l]);
        for (const ObjTri* t : tris)
            for (const ObjVert& v : t->v)
                for (int l = 0; l < 3; ++l) reach = std::max(reach, std::fabs(v.p[l] - centre[l]));
        mrpw::HostMeshClass c;
        c.id = static_cast<int32_t>(out.classes.size());
        c.scale = std::max(reach * 1024.0f / 32000.0f, 1.0f / 64.0f);
        const float k = 1024.0f / c.scale;
        float r2 = 0;
        for (const ObjTri* t : tris)
            for (const ObjVert& v : t->v) {
                float d = 0;
                for (int l = 0; l < 3; ++l) d += (v.p[l] - centre[l]) * (v.p[l] - centre[l]);
                r2 = std::max(r2, d);
            }
        c.sphere[3] = std::sqrt(r2) / c.scale;
        // the vertices' colours (the OBJ's, times the material's Kd when it has a texture: a colour material's Kd is its
        // texture) in the instance's palette: at most 64 (the tie's colour table), so made coarser until they fit
        auto vcol = [&](const ObjTri* t, int i) {
            std::array<int, 3> c{};
            const ObjMaterial* m = t->material >= 0 ? &o.materials[static_cast<size_t>(t->material)] : nullptr;
            for (int l = 0; l < 3; ++l) {
                const float k = m != nullptr && !m->map.empty() ? std::clamp(m->kd[l], 0.0f, 1.0f) : 1.0f;
                c[static_cast<size_t>(l)] = static_cast<int>(std::lround(t->v[i].col[l] * k * 128.0f));
            }
            return c;
        };
        std::map<std::array<int, 3>, uint8_t> pal;
        int step = 1;
        for (const int st : {1, 2, 4, 8, 16, 32, 43}) {
            step = st;
            pal.clear();
            for (const ObjTri* t : tris)
                for (int i = 0; i < 3; ++i) {
                    std::array<int, 3> c = vcol(t, i);
                    for (int& x : c) x = std::min(128, (x + st / 2) / st * st);
                    pal.emplace(c, 0);
                }
            if (pal.size() <= 64) break;
        }
        const bool coloured = !(pal.size() == 1 && pal.begin()->first == std::array<int, 3>{128, 128, 128});
        mrpw::HostMeshInstance in;
        if (coloured) {
            for (auto& [c, k] : pal) {
                k = static_cast<uint8_t>(in.palette.size());
                in.palette.push_back({static_cast<uint8_t>(c[0]), static_cast<uint8_t>(c[1]), static_cast<uint8_t>(c[2]), 0x80});
            }
            if (step > 1) log.say(fmt("  %s: its vertex colours made coarser (steps of %d / 128) to fit 64", o.objects[obj].c_str(), step));
        }
        auto vindex = [&](const ObjTri* t, int i) -> int16_t {
            if (!coloured) return 0;
            std::array<int, 3> c = vcol(t, i);
            for (int& x : c) x = std::min(128, (x + step / 2) / step * step);
            return static_cast<int16_t>(pal[c]);
        };
        std::map<int, uint32_t> local;  // material -> local texture
        mrpw::HostMeshPacket pk;
        auto flush = [&] {
            if (pk.dinky.empty()) return;
            pk.index_in_lod = static_cast<uint8_t>(c.packets.size());
            c.packets.push_back(std::move(pk));
            pk = mrpw::HostMeshPacket{};
        };
        bool clamped = false;
        for (const ObjTri* t : tris) {
            if (local.count(t->material) == 0) {
                if (local.size() >= 16) {
                    err = "the mesh \"" + o.objects[obj] + "\" has more than 16 materials (split it into objects)";
                    return false;
                }
                const int tx = texture(t->material);
                if (tx < 0) return false;
                const uint32_t lt = static_cast<uint32_t>(local.size());
                local[t->material] = lt;
                c.textures.push_back({static_cast<uint32_t>(tx), 0, 0x44});
            }
            if (pk.dinky.size() + 3 > 240) flush();
            // the UVs moved by whole turns to start near 0 (they are s16 / 4096: within +-8)
            const float su = std::floor(std::min({t->v[0].uv[0], t->v[1].uv[0], t->v[2].uv[0]}));
            const float sv = std::floor(std::min({1.0f - t->v[0].uv[1], 1.0f - t->v[1].uv[1], 1.0f - t->v[2].uv[1]}));
            mrpw::HostMeshStrip strip;
            strip.local_texture = local[t->material];
            for (int vi = 0; vi < 3; ++vi) {
                const ObjVert& v = t->v[vi];
                std::array<int16_t, 8> d{};
                for (int l = 0; l < 3; ++l) d[static_cast<size_t>(l)] = static_cast<int16_t>(std::clamp(std::lround((v.p[l] - centre[l]) * k), -32767l, 32767l));
                d[3] = vindex(t, vi);
                const float s = (v.uv[0] - su) * 4096.0f, tt = (1.0f - v.uv[1] - sv) * 4096.0f;
                if (s > 32767.0f || tt > 32767.0f) clamped = true;
                d[4] = static_cast<int16_t>(std::clamp(std::lround(s), -32767l, 32767l));
                d[5] = static_cast<int16_t>(std::clamp(std::lround(tt), -32767l, 32767l));
                d[6] = 4096;
                strip.verts.push_back(static_cast<uint16_t>(pk.dinky.size()));
                pk.dinky.push_back(d);
            }
            pk.strips.push_back(std::move(strip));
        }
        flush();
        if (clamped) log.say("  " + o.objects[obj] + ": UVs spanning more than 8 repeats in one triangle were clamped");
        for (int r = 0; r < 3; ++r) in.rot[r][r] = 1.0f;
        for (int l = 0; l < 3; ++l) in.pos[l] = centre[l];
        in.cls = static_cast<uint32_t>(out.classes.size());
        out.instances.push_back(in);
        log.say(fmt("  mesh \"%s\": %zu triangles, %zu packets, %zu textures, %zu vertex colours, centre (%.1f %.1f %.1f), reach %.1f", o.objects[obj].c_str(),
                    tris.size(), c.packets.size(), c.textures.size(), in.palette.size(), centre[0], centre[1], centre[2], reach));
        total_tris += tris.size();
        out.classes.push_back(std::move(c));
    }
    log.say(fmt("host meshes: %zu meshes, %zu triangles, %zu textures", out.classes.size(), total_tris, out.textures.size()));
    return true;
}

// ---- the gameplay file ----
// the ship's take-off path (arrival.cpp's step 3 follows it; its last six points fade the screen out): up and ahead along
// its heading. The paths block: count, the data's offset and size, then the paths' offsets in the data; a path: its point
// count, 12 bytes, then 16-byte points (x y z, the turn the game computes)
std::vector<uint8_t> flight_path(const float ship[4]) {
    const int n = 12;
    const float dx = std::cos(ship[3]), dy = std::sin(ship[3]);
    std::vector<uint8_t> path(16, 0);
    wr32(path, 0, n);
    for (int k = 0; k < n; ++k) {
        const float ahead = 6.0f * static_cast<float>(k * k) / 4.0f + 4.0f * static_cast<float>(k);
        const size_t at = path.size();
        path.resize(at + 16, 0);
        wrf(path, at, ship[0] + dx * ahead);
        wrf(path, at + 4, ship[1] + dy * ahead);
        wrf(path, at + 8, ship[2] + 6.0f + 9.0f * static_cast<float>(k));
    }
    std::vector<uint8_t> b(0x20, 0);
    wr32(b, 0, 1);
    wr32(b, 4, 0x20);
    wr32(b, 8, static_cast<int32_t>(path.size()));
    b.insert(b.end(), path.begin(), path.end());
    return b;
}

// the base level's moby records (0x78 bytes and more each) by class: the first of each
std::map<int, std::vector<uint8_t>> moby_templates(const std::vector<uint8_t>& g, uint32_t mo, int n1) {
    std::map<int, std::vector<uint8_t>> t;
    size_t p = mo + 0x10;
    for (int i = 0; i < n1 && p + 0x78 <= g.size(); ++i) {
        const int sz = rd32(g, p);
        if (sz < 0x78 || p + static_cast<size_t>(sz) > g.size()) break;
        const int cls = rd32(g, p + 0x18);
        if (t.count(cls) == 0) t[cls].assign(g.begin() + static_cast<std::ptrdiff_t>(p), g.begin() + static_cast<std::ptrdiff_t>(p + static_cast<size_t>(sz)));
        p += static_cast<size_t>(sz);
    }
    return t;
}

// a fixup list (the pvar moby links +0x50, the pvar pointers +0x5c: 8-byte rows (pvar index, offset) up to a negative
// index): its rows, and the end row as it was
void fixup_rows(const std::vector<uint8_t>& b, std::vector<std::pair<int32_t, int32_t>>& rows, std::vector<uint8_t>& end) {
    rows.clear();
    end.assign(8, 0);
    wr32(end, 0, -1);
    for (size_t r = 0; r + 8 <= b.size(); r += 8) {
        if (rd32(b, r) < 0) {
            end.assign(b.begin() + static_cast<std::ptrdiff_t>(r), b.begin() + static_cast<std::ptrdiff_t>(r + 8));
            return;
        }
        rows.emplace_back(rd32(b, r), rd32(b, r + 4));
    }
}

std::vector<uint8_t> fixup_block(const std::vector<std::pair<int32_t, int32_t>>& rows, const std::vector<uint8_t>& end) {
    std::vector<uint8_t> b;
    for (const auto& [i, o] : rows) {
        wr32(b, b.size(), i);
        wr32(b, b.size(), o);
    }
    b.insert(b.end(), end.begin(), end.end());
    b.resize(align_up(b.size(), 0x10), 0);
    return b;
}

bool place_mobys(const std::vector<uint8_t>& g, const uint32_t G[36], uint32_t mo, int n1, int32_t hero_uid, const LevelTxt& lt, std::vector<uint8_t>& mobys,
                 std::map<uint32_t, std::vector<uint8_t>>& blocks, Log& log, std::string& err) {
    if (lt.mobys.empty()) return true;
    std::vector<uint32_t> offs;
    for (int i = 0; i < 36; ++i)
        if (G[i] != 0) offs.push_back(G[i]);
    offs.push_back(static_cast<uint32_t>(g.size()));
    std::sort(offs.begin(), offs.end());
    auto block = [&](uint32_t field) -> std::vector<uint8_t> {
        const uint32_t o = G[field / 4];
        if (o == 0) return {};
        const uint32_t e = *std::upper_bound(offs.begin(), offs.end(), o);
        return std::vector<uint8_t>(g.begin() + o, g.begin() + e);
    };
    const std::map<int, std::vector<uint8_t>> tmpl = moby_templates(g, mo, n1);
    std::vector<uint8_t> table = block(0x54), data = block(0x58), shared = block(0x4c);
    std::vector<std::pair<int32_t, int32_t>> links, ptrs;
    std::vector<uint8_t> links_end, ptrs_end;
    fixup_rows(block(0x50), links, links_end);
    fixup_rows(block(0x5c), ptrs, ptrs_end);
    // the shared pvar pointers: (data size, count, -, -), the data, then count rows (pvar index | offset << 16, the data's offset)
    std::vector<std::pair<uint32_t, uint32_t>> shared_rows;
    std::vector<uint8_t> shared_data;
    if (shared.size() >= 0x10) {
        const int32_t size = rd32(shared, 0), n = rd32(shared, 4);
        if (size >= 0 && n >= 0 && 0x10 + static_cast<size_t>(size) + 8 * static_cast<size_t>(n) <= shared.size()) {
            shared_data.assign(shared.begin() + 0x10, shared.begin() + 0x10 + size);
            for (int32_t k = 0; k < n; ++k)
                shared_rows.emplace_back(static_cast<uint32_t>(rd32(shared, 0x10 + static_cast<size_t>(size) + 8 * static_cast<size_t>(k))),
                                         static_cast<uint32_t>(rd32(shared, 0x10 + static_cast<size_t>(size) + 8 * static_cast<size_t>(k) + 4)));
        }
    }
    table.resize(align_up(table.size(), 8), 0);
    int32_t uid = 0;
    std::map<int, int> per_class;
    size_t pvar_bytes = 0;
    for (const LevelTxt::Moby& pm : lt.mobys) {
        const auto t = tmpl.find(pm.cls);
        if (t == tmpl.end()) {
            std::string have;
            for (const auto& [c, r] : tmpl)
                if (c != 0) have += (have.empty() ? "" : " ") + std::to_string(c);
            err = fmt("level.txt:%d: the base level has no object of class %d to copy (its classes placed: %s)", pm.line, pm.cls, have.c_str());
            return false;
        }
        std::vector<uint8_t> r = t->second;
        if (++uid == hero_uid) ++uid;
        wr32(r, 0xc, uid);
        for (int i = 0; i < 3; ++i) wrf(r, static_cast<size_t>(0x30 + 4 * i), pm.at[i]);
        wrf(r, 0x3c, 0.0f);
        wrf(r, 0x40, 0.0f);
        wrf(r, 0x44, pm.at[3]);
        wr32(r, 0x48, -1);  // (its moby group, the base level's (the composed level has none): none)
        wr32(r, 0x8, rd32(r, 0x8) & 4);  // (its spawn rules: not the base level's missions' (flags 1, 2, 8, 0x10); 4, gone
                                         // for good once destroyed, kept)
        const int32_t pi = rd32(r, 0x58);
        if (pi >= 0) {
            if (static_cast<size_t>(pi) * 8 + 8 > table.size()) {
                err = fmt("level.txt:%d: class %d's pvars are not in the base level's table", pm.line, pm.cls);
                return false;
            }
            const int32_t off = rd32(table, static_cast<size_t>(pi) * 8), size = rd32(table, static_cast<size_t>(pi) * 8 + 4);
            if (off < 0 || size < 0 || static_cast<size_t>(off) + static_cast<size_t>(size) > data.size()) {
                err = fmt("level.txt:%d: class %d's pvars are past the base level's pvar data", pm.line, pm.cls);
                return false;
            }
            std::vector<uint8_t> copy(data.begin() + off, data.begin() + off + size);
            for (const auto& [i, o] : links)  // (another moby named: none here)
                if (i == pi && o >= 0 && static_cast<size_t>(o) + 4 <= copy.size()) wr32(copy, static_cast<size_t>(o), -1);
            const int32_t ni = static_cast<int32_t>(table.size() / 8);
            data.resize(align_up(data.size(), 0x10), 0);
            const size_t at = data.size();
            data.insert(data.end(), copy.begin(), copy.end());
            wr32(table, table.size(), static_cast<int32_t>(at));
            wr32(table, table.size(), size);
            for (const auto& [i, o] : std::vector<std::pair<int32_t, int32_t>>(ptrs))
                if (i == pi) ptrs.emplace_back(ni, o);
            for (const auto& [w, o] : std::vector<std::pair<uint32_t, uint32_t>>(shared_rows))
                if (static_cast<int32_t>(w & 0xffff) == pi) shared_rows.emplace_back((w & 0xffff0000u) | static_cast<uint32_t>(ni), o);
            wr32(r, 0x58, ni);
            pvar_bytes += static_cast<size_t>(size);
        }
        mobys.insert(mobys.end(), r.begin(), r.end());
        ++per_class[pm.cls];
    }
    blocks[0x54] = table;
    data.resize(align_up(data.size(), 0x10), 0);
    blocks[0x58] = data;
    blocks[0x50] = fixup_block(links, links_end);
    blocks[0x5c] = fixup_block(ptrs, ptrs_end);
    if (!shared.empty()) {
        std::vector<uint8_t> sb(0x10, 0);
        wr32(sb, 0, static_cast<int32_t>(shared_data.size()));
        wr32(sb, 4, static_cast<int32_t>(shared_rows.size()));
        sb.insert(sb.end(), shared_data.begin(), shared_data.end());
        for (const auto& [w, o] : shared_rows) {
            wr32(sb, sb.size(), static_cast<int32_t>(w));
            wr32(sb, sb.size(), static_cast<int32_t>(o));
        }
        sb.resize(align_up(sb.size(), 0x10), 0);
        blocks[0x4c] = sb;
    }
    std::string what;
    for (const auto& [c, k] : per_class) what += fmt("%s%d x%d", what.empty() ? "" : ", ", c, k);
    log.say(fmt("objects: %zu placed (class x count: %s), uids 1..%d, %zu pvar bytes copied", lt.mobys.size(), what.c_str(), uid, pvar_bytes));
    return true;
}

bool build_gameplay(const std::vector<uint8_t>& g, const LevelTxt& lt, std::vector<uint8_t>& out, Log& log, std::string& err) {
    if (g.size() < 0x90) {
        err = "the base level's gameplay file is too small";
        return false;
    }
    uint32_t G[36];
    std::memcpy(G, g.data(), sizeof G);
    std::vector<uint32_t> offs;
    for (uint32_t x : G)
        if (x != 0) offs.push_back(x);
    offs.push_back(static_cast<uint32_t>(g.size()));
    std::sort(offs.begin(), offs.end());
    auto block = [&](uint32_t field) -> std::vector<uint8_t> {
        const uint32_t o = G[field / 4];
        if (o == 0) return {};
        const uint32_t e = *std::upper_bound(offs.begin(), offs.end(), o);
        return std::vector<uint8_t>(g.begin() + o, g.begin() + e);
    };
    std::map<uint32_t, std::vector<uint8_t>> blocks;
    std::map<uint32_t, bool> none;  // (written as 0: absent)
    // the settings: background +0 (three words), fog colour +0xc (three bytes in words), death height +0x28, the ship +0x2c
    // (x y z yaw; x 0: none), its take-off path +0x3c and the two level cameras its view blends between +0x40 / +0x44
    std::vector<uint8_t> st = block(0x00);
    if (st.size() < 0x48) {
        err = "the base level's settings block is too small";
        return false;
    }
    if (lt.has_background)
        for (int i = 0; i < 3; ++i) wr32(st, static_cast<size_t>(4 * i), lt.background[i]);
    if (lt.has_fog)
        for (int i = 0; i < 3; ++i) wr32(st, static_cast<size_t>(0xc + 4 * i), lt.fog[i]);
    float base_death = 0;
    std::memcpy(&base_death, st.data() + 0x28, 4);
    if (lt.has_death) wrf(st, 0x28, lt.death);
    for (int i = 0; i < 4; ++i) wrf(st, static_cast<size_t>(0x2c + 4 * i), lt.ship ? lt.ship_at[i] : 0.0f);
    wr32(st, 0x3c, 0);
    wr32(st, 0x40, -1);
    wr32(st, 0x44, -1);
    blocks[0x00] = st;
    const std::vector<uint8_t> empty(0x10, 0);
    for (uint32_t f : {0x0cu, 0x30u, 0x34u, 0x38u, 0x3cu, 0x60u, 0x64u, 0x68u, 0x6cu, 0x80u, 0x88u}) blocks[f] = empty;
    // the cameras: the base level's generic ones (the follow camera, type 0, and types 4..7 with no place), not its scripted
    // ones (their boxes are the base level's cuboids, dropped)
    {
        const std::vector<uint8_t> cb = block(0x08);
        std::vector<uint8_t> cams(0x10, 0);
        int n = 0;
        const int total = cb.size() >= 4 ? rd32(cb, 0) : 0;
        for (int k = 0; k < total && 0x30 + 0x20 * static_cast<size_t>(k) <= cb.size(); ++k) {
            const size_t c = 0x10 + 0x20 * static_cast<size_t>(k);
            const int type = rd32(cb, c);
            bool unplaced = true;
            for (size_t i = 4; i < 16; ++i) unplaced = unplaced && cb[c + i] == 0;
            if ((type == 0 || (type >= 4 && type <= 7)) && unplaced) {
                cams.insert(cams.end(), cb.begin() + static_cast<std::ptrdiff_t>(c), cb.begin() + static_cast<std::ptrdiff_t>(c + 0x20));
                ++n;
            }
        }
        wr32(cams, 0, n);
        blocks[0x08] = cams;
    }
    blocks[0x70] = lt.ship ? flight_path(lt.ship_at) : std::vector<uint8_t>(0x10, 0);  // the paths
    {
        std::vector<uint8_t> grind(0x10, 0);  // the grind lines: none
        wr32(grind, 4, 0x10);
        blocks[0x74] = grind;
    }
    // the mobys: the hero only (the base level's class 0 record), at the start
    {
        const uint32_t mo = G[0x44 / 4];
        if (mo == 0 || mo + 0x10 > g.size()) {
            err = "the base level has no mobys block";
            return false;
        }
        const int n1 = rd32(g, mo), n2 = rd32(g, mo + 4);
        size_t p = mo + 0x10;
        std::vector<uint8_t> hero;
        for (int i = 0; i < n1 && p + 0x20 <= g.size(); ++i) {
            const int sz = rd32(g, p);
            if (sz <= 0) break;
            if (rd32(g, p + 0x18) == 0) {
                hero.assign(g.begin() + static_cast<std::ptrdiff_t>(p), g.begin() + static_cast<std::ptrdiff_t>(p + static_cast<size_t>(sz)));
                break;
            }
            p += static_cast<size_t>(sz);
        }
        if (hero.size() < 0x48) {
            err = "the base level has no hero record (class 0)";
            return false;
        }
        for (int i = 0; i < 3; ++i) wrf(hero, static_cast<size_t>(0x30 + 4 * i), lt.hero[i]);
        wrf(hero, 0x3c, 0.0f);
        wrf(hero, 0x40, 0.0f);
        wrf(hero, 0x44, lt.hero[3]);
        std::vector<uint8_t> b(0x10, 0);
        wr32(b, 4, n2);
        b.insert(b.end(), hero.begin(), hero.end());
        // the placed objects (level.txt's moby lines): each a copy of its class's first placement in the base level (its
        // settings, its pvars copied into entries of their own), at the line's place; uids of our own, 1, 2, ... in the
        // lines' order (the hero's skipped); their pvar words that name another moby (the moby links) none (-1), no group
        if (!place_mobys(g, G, mo, n1, rd32(hero, 0xc), lt, b, blocks, log, err)) return false;
        wr32(b, 0, 1 + static_cast<int32_t>(lt.mobys.size()));
        blocks[0x44] = b;
    }
    blocks[0x48] = empty;  // (no moby groups: the base level's name its own records)
    none[0x8c] = true;     // (the occlusion's: none)
    for (uint32_t f = 0; f < 0x90; f += 4)
        if (blocks.count(f) == 0 && none.count(f) == 0) {
            std::vector<uint8_t> b = block(f);
            if (!b.empty()) blocks[f] = b;
        }
    // the blocks in the base file's order (the ones it lacks after them)
    std::vector<uint32_t> order;
    for (uint32_t f = 0; f < 0x90; f += 4) order.push_back(f);
    std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
        const uint32_t oa = G[a / 4] == 0 ? 0xffffffffu : G[a / 4], ob = G[b / 4] == 0 ? 0xffffffffu : G[b / 4];
        return oa < ob;
    });
    std::vector<uint8_t> ng(0xa0, 0);
    for (uint32_t f : order) {
        const auto it = blocks.find(f);
        if (it == blocks.end()) continue;
        const size_t at = align_up(ng.size(), 0x10);
        ng.resize(at, 0);
        ng.insert(ng.end(), it->second.begin(), it->second.end());
        wr32(ng, f, static_cast<int32_t>(at));
    }
    // the budget: the base level's own gameplay data fits its memory; ours is its blocks with fewer objects (a few more
    // bytes per placed object's pvars at most)
    if (lt.mobys.size() + 1 > static_cast<size_t>(rd32(g, G[0x44 / 4]))) {
        err = fmt("too many objects: %zu, the base level places %d", lt.mobys.size(), rd32(g, G[0x44 / 4]) - 1);
        return false;
    }
    if (ng.size() > g.size() + 0x10000) {
        err = fmt("the gameplay data is too large (%zu bytes, the base level's %zu)", ng.size(), g.size());
        return false;
    }
    out = wad_encode(ng);
    std::vector<uint8_t> back;
    std::string why;
    if (!wad_decode_strict(out, back, why) || back != ng) {
        err = "the gameplay file does not decode back through the game's decoder: " + why;
        return false;
    }
    out.resize(align_up(out.size(), 2048), 0);
    log.say(fmt("gameplay: %zu bytes, packed %zu; hero at (%.1f %.1f %.1f), ship %s, death height %.1f", ng.size(), out.size(), lt.hero[0], lt.hero[1], lt.hero[2],
                lt.ship ? fmt("at (%.1f %.1f %.1f)", lt.ship_at[0], lt.ship_at[1], lt.ship_at[2]).c_str() : "none", lt.has_death ? lt.death : base_death));
    return true;
}

// ---- the data file ----
// a level's data file's asset data (decompressed) and its index's first words
bool data_core(const std::vector<uint8_t>& wad, std::vector<uint8_t>& core, int32_t H[48], std::string& err) {
    FileSpan parts[11];
    if (wad.size() < sizeof parts) {
        err = "a data file too small";
        return false;
    }
    std::memcpy(parts, wad.data(), sizeof parts);
    for (const FileSpan& sp : parts)
        if (sp.offset < 0 || sp.size < 0 || static_cast<size_t>(sp.offset) + static_cast<size_t>(sp.size) > wad.size()) {
            err = "a data file damaged";
            return false;
        }
    if (static_cast<size_t>(parts[2].size) < 48 * 4) {
        err = "a data file's asset index too small";
        return false;
    }
    std::memcpy(H, wad.data() + parts[2].offset, 48 * 4);
    if (!decompress_wad(std::span<const std::byte>(reinterpret_cast<const std::byte*>(wad.data() + parts[10].offset), static_cast<size_t>(parts[10].size)), core)) {
        err = "a data file's asset data cannot be decompressed";
        return false;
    }
    return true;
}

bool build_data(const std::vector<uint8_t>& wad, const std::vector<uint8_t>& coll, const std::vector<uint8_t>* sky, std::vector<uint8_t>& out, Log& log,
                std::string& err) {
    if (wad.size() < sizeof(LevelFileHeader)) {
        err = "the base level's data file is too small";
        return false;
    }
    FileSpan parts[11];
    std::memcpy(parts, wad.data(), sizeof parts);
    for (const FileSpan& s : parts)
        if (s.offset < 0 || s.size < 0 || static_cast<size_t>(s.offset) + static_cast<size_t>(s.size) > wad.size()) {
            err = "the base level's data file is damaged";
            return false;
        }
    auto part = [&](int i) { return std::vector<uint8_t>(wad.begin() + parts[i].offset, wad.begin() + parts[i].offset + parts[i].size); };
    std::vector<uint8_t> idx = part(2);
    std::vector<uint8_t> old;
    if (!decompress_wad(std::span<const std::byte>(reinterpret_cast<const std::byte*>(wad.data() + parts[10].offset), static_cast<size_t>(parts[10].size)), old)) {
        err = "the base level's asset data cannot be decompressed";
        return false;
    }
    int32_t H[48];
    if (idx.size() < sizeof H) {
        err = "the base level's asset index is too small";
        return false;
    }
    std::memcpy(H, idx.data(), sizeof H);
    auto in_core = [&](int32_t v) { return v > 0 && static_cast<size_t>(v) <= old.size(); };
    if (!in_core(H[4]) || !in_core(H[5]) || H[5] <= H[4] || !in_core(H[24]) || !in_core(H[25]) || !in_core(H[31])) {
        err = "the base level's asset index is not as expected (sky, collision, texels, effects)";
        return false;
    }
    Core core;
    // the tfrags: none ({the entries' offset 0x40, count 0, 12.0, 0})
    std::vector<uint8_t> tf(0x40, 0);
    wr32(tf, 0, 0x40);
    wrf(tf, 8, 12.0f);
    const size_t o_tfrag = core.put(tf.data(), tf.size());
    // the sky: the base level's (up to its collision), or another level's (level.txt's sky line)
    const size_t o_sky = sky != nullptr ? core.put(sky->data(), sky->size()) : core.put(old.data() + H[4], static_cast<size_t>(H[5] - H[4]));
    // (testing: RAC1_COMPOSE_BASE_COLLISION=1 keeps the base level's collision, to tell a collision fault from another)
    static const bool base_coll = std::getenv("RAC1_COMPOSE_BASE_COLLISION") != nullptr;
    const size_t o_coll = base_coll ? core.put(old.data() + H[5], static_cast<size_t>(H[24] - H[5])) : core.put(coll.data(), coll.size());
    // the moby textures' texels, in a texel block of their own
    const int32_t nmt = H[14];
    size_t m_lo = SIZE_MAX, m_hi = 0;
    for (int32_t k = 0; k < nmt; ++k) {
        TextureRecord r;
        std::memcpy(&r, idx.data() + H[15] + 16 * k, 16);
        const size_t n = static_cast<size_t>(r.width) * static_cast<size_t>(r.height) + (r.mip_count >= 2 ? static_cast<size_t>(r.width / 2) * static_cast<size_t>(r.height / 2) : 0);
        m_lo = std::min(m_lo, static_cast<size_t>(r.texels));
        m_hi = std::max(m_hi, static_cast<size_t>(r.texels) + n);
    }
    if (nmt <= 0 || static_cast<size_t>(H[24]) + m_hi > old.size()) {
        err = "the base level's moby textures are not as expected";
        return false;
    }
    const size_t o_texel = core.put(old.data() + H[24] + m_lo, m_hi - m_lo);
    {
        const std::vector<uint8_t> pad(0x400, 0);  // (the exporter reads two mip levels whatever the count)
        core.put(pad.data(), pad.size());
    }
    // the base level's parts after its texel block: particle and effect banks, the +0x74 / +0xa0 blocks, the moby classes
    // [particle block, first tie class); the hero's sequences and the gadgets [+0x7c, end). Ties and shrubs dropped.
    int32_t tie_lo = H[31];
    for (int32_t k = 0; k < H[8]; ++k) tie_lo = std::min(tie_lo, rd32(idx, static_cast<size_t>(H[9] + 32 * k)));
    if (tie_lo < H[25]) {
        err = "the base level's tie classes are not after its effects";
        return false;
    }
    core.keep(old, static_cast<size_t>(H[25]), static_cast<size_t>(tie_lo));
    core.keep(old, static_cast<size_t>(H[31]), old.size());
    // the index: the base level's, re-pointed
    auto remap = [&](size_t field_at, const char* what) {
        const int32_t v = rd32(idx, field_at);
        if (v <= 0) return true;
        size_t to = 0;
        if (!core.remap(static_cast<size_t>(v), to)) {
            err = fmt("the base level's %s (0x%x) is in no part kept", what, v);
            return false;
        }
        wr32(idx, field_at, static_cast<int32_t>(to));
        return true;
    };
    wr32(idx, 0x08, static_cast<int32_t>(o_tfrag));
    wr32(idx, 0x0c, 0);  // no occlusion
    wr32(idx, 0x10, static_cast<int32_t>(o_sky));
    wr32(idx, 0x14, static_cast<int32_t>(o_coll));
    for (int32_t k = 0; k < H[6]; ++k)
        if (!remap(static_cast<size_t>(H[7] + 32 * k), "moby class")) return false;
    wr32(idx, 0x20, 0);  // no tie classes
    wr32(idx, 0x28, 0);  // no shrub classes
    wr32(idx, 0x30, 0);  // no tfrag textures
    wr32(idx, 0x40, 0);  // no tie textures
    wr32(idx, 0x48, 0);  // no shrub textures
    for (int32_t k = 0; k < nmt; ++k) wr32(idx, static_cast<size_t>(H[15] + 16 * k), rd32(idx, static_cast<size_t>(H[15] + 16 * k)) - static_cast<int32_t>(m_lo));
    wr32(idx, 0x60, static_cast<int32_t>(o_texel));
    for (size_t f : {0x64u, 0x68u, 0x74u, 0x7cu, 0xa0u, 0xa4u, 0xa8u})
        if (!remap(f, fmt("block +0x%zx", f).c_str())) return false;
    for (int k = 0; k < 256; ++k)
        if (!remap(static_cast<size_t>(H[30] + 4 * k), "hero sequence")) return false;
    for (int32_t k = 0; k < H[32]; ++k)
        if (!remap(static_cast<size_t>(H[33] + 16 * k), "gadget")) return false;
    std::vector<uint8_t> packed = wad_encode(core.buf);
    {
        std::vector<uint8_t> back;
        std::string why;
        if (!wad_decode_strict(packed, back, why) || back != core.buf) {
            err = "the asset data does not decode back through the game's decoder: " + why;
            return false;
        }
    }
    wr32(idx, 0x88, static_cast<int32_t>(packed.size()));
    wr32(idx, 0x8c, static_cast<int32_t>(core.buf.size()));
    // the budget: the base level's code places what follows its asset data for its own, so the level's must not be larger
    log.say(fmt("budget: asset data %.2f MB of the base level's %.2f MB (%.0f %%)", static_cast<double>(core.buf.size()) / 1048576.0,
                static_cast<double>(old.size()) / 1048576.0, 100.0 * static_cast<double>(core.buf.size()) / static_cast<double>(old.size())));
    if (core.buf.size() > old.size()) {
        err = fmt("the level is too large: its asset data (%.2f MB: the base level's classes, sky and effects, and the collision, %.2f MB) is larger "
                  "than the base level's (%.2f MB), which is what the game's memory holds there (make the collision smaller or coarser)",
                  static_cast<double>(core.buf.size()) / 1048576.0, static_cast<double>(coll.size()) / 1048576.0, static_cast<double>(old.size()) / 1048576.0);
        return false;
    }
    log.say(fmt("asset data: %zu bytes (tfrags none, sky 0x%zx, collision 0x%zx, texels 0x%zx; the base level's %zu kept), packed %zu (%.0f %%)", core.buf.size(), o_sky,
                o_coll, o_texel, old.size(), packed.size(), 100.0 * static_cast<double>(packed.size()) / static_cast<double>(core.buf.size())));
    // the data file: the base level's code, sound bank, VRAM image and HUD; the new index and asset data
    out.assign(0x80, 0);
    FileSpan np[11];
    for (int i = 0; i < 11; ++i) {
        const std::vector<uint8_t> b = i == 2 ? idx : i == 10 ? packed : part(i);
        const size_t at = align_up(out.size(), 0x40);
        out.resize(at, 0);
        out.insert(out.end(), b.begin(), b.end());
        np[i] = {static_cast<int32_t>(at), static_cast<int32_t>(b.size())};
    }
    std::memcpy(out.data(), np, sizeof np);
    out.resize(align_up(out.size(), 2048), 0);
    log.say(fmt("data file: %zu bytes (the base level's %zu)", out.size(), wad.size()));
    return true;
}

// a stamp as build.json writes it
std::string stamp_hex(uint64_t v) { return fmt("%016llx", static_cast<unsigned long long>(v)); }

}  // namespace

// orig: (host) see level_compose.h
bool compose_level_mod(const std::string& folder, bool force) {
    ModInfo m;
    m.folder = folder;
    m.dir = mods_dir() / folder;
    if (!mods_manifest_read(m)) {
        std::cerr << "[compose] " << folder << ": " << m.error << "\n";
        return false;
    }
    if (!m.level) {
        std::cerr << "[compose] " << folder << ": not a level mod (its mod.json has no \"kind\": \"level\")\n";
        return false;
    }
    const fs::path out_dir = mods_level_build_dir(folder);
    if (!force && mods_level_build_check(m).empty()) {
        std::cout << "[compose] " << folder << ": up to date\n";
        return true;
    }
    Log log;
    log.say(fmt("%s: the level \"%s\" (format %d) on level %d", folder.c_str(), m.level_id.c_str(), m.level_format, m.level_base));
    const fs::path ldir = m.dir / "level";
    LevelTxt lt;
    std::string err;
    auto fail = [&](const std::string& why) {
        std::cerr << "[compose] " << folder << ": " << why << "\n";
        return false;
    };
    if (!read_level_txt(ldir / "level.txt", lt, err)) return fail(err);
    // the offset: `offset auto` as far as the collision's lowest corner needs to be 16 units above 0 (in whole cells of 4)
    if (lt.offset_auto) {
        Obj probe;
        if (!read_obj(ldir / lt.collision, lt, false, probe, err)) return fail(err);
        float lo[3] = {1e30f, 1e30f, 1e30f};
        for (const ObjTri& t : probe.tris)
            for (const ObjVert& v : t.v)
                for (int l = 0; l < 3; ++l) lo[l] = std::min(lo[l], v.p[l]);
        for (int l = 0; l < 3; ++l) lt.offset[l] = !probe.tris.empty() && lo[l] < 16.0f ? std::ceil((16.0f - lo[l]) / 4.0f) * 4.0f : 0.0f;
    }
    if (lt.offset[0] != 0 || lt.offset[1] != 0 || lt.offset[2] != 0) {
        for (int l = 0; l < 3; ++l) {
            lt.hero[l] += lt.offset[l];
            lt.ship_at[l] += lt.offset[l];
            for (LevelTxt::Moby& mb : lt.mobys) mb.at[l] += lt.offset[l];
        }
        lt.death += lt.offset[2];
        log.say(fmt("offset: (%g %g %g) added to every coordinate%s (lights.txt's are the game's: not moved)", lt.offset[0], lt.offset[1], lt.offset[2],
                    lt.offset_auto ? " (auto)" : ""));
    }
    // the collision
    Obj co;
    if (!read_obj(ldir / lt.collision, lt, false, co, err)) return fail(err);
    std::vector<CTri> ctris;
    for (const ObjTri& t : co.tris) {
        CTri c;
        for (int i = 0; i < 3; ++i) std::memcpy(c.p[static_cast<size_t>(i)].data(), t.v[i].p, sizeof t.v[i].p);
        c.type = static_cast<uint8_t>(lt.collision_type);
        if (t.material >= 0) {
            const std::string& n = co.materials[static_cast<size_t>(t.material)].name;
            if (n.rfind("type", 0) == 0 && n.size() > 4) c.type = static_cast<uint8_t>(std::clamp(std::atoi(n.c_str() + 4), 0, 255));
        }
        ctris.push_back(c);
    }
    std::vector<uint8_t> coll;
    if (!build_collision(ctris, coll, log, err)) return fail(err);
    // the base level's files (the player's disc, as the setup extracted it)
    char base_dir[32];
    std::snprintf(base_dir, sizeof base_dir, "levels/level%02d", m.level_base);
    const fs::path bdir = fs::path("assets/disc") / base_dir;
    std::vector<uint8_t> base_wad, base_gp_file, base_gp;
    if (!read_file(bdir / "data.wad", base_wad) || !read_file(bdir / "gameplay_ntsc.bin", base_gp_file))
        return fail("the base level's files are missing (" + bdir.generic_string() + "): run the setup with your disc first");
    if (!decompress_wad(std::span<const std::byte>(reinterpret_cast<const std::byte*>(base_gp_file.data()), base_gp_file.size()), base_gp))
        return fail("the base level's gameplay file cannot be decompressed");
    // another level's sky
    std::vector<uint8_t> sky;
    if (lt.sky_level >= 0 && lt.sky_level != m.level_base) {
        std::vector<uint8_t> w, c;
        int32_t SH[48];
        const fs::path sp = fs::path("assets/disc") / fmt("levels/level%02d", lt.sky_level) / "data.wad";
        if (!read_file(sp, w) || !data_core(w, c, SH, err)) return fail(sp.generic_string() + ": its sky cannot be read (" + err + ")");
        if (SH[4] <= 0 || SH[5] <= SH[4] || static_cast<size_t>(SH[5]) > c.size()) return fail(fmt("level %d has no sky", lt.sky_level));
        sky.assign(c.begin() + SH[4], c.begin() + SH[5]);
        log.say(fmt("sky: level %d's (%zu bytes)", lt.sky_level, sky.size()));
    }
    std::vector<uint8_t> data, gameplay;
    if (!build_data(base_wad, coll, sky.empty() ? nullptr : &sky, data, log, err)) return fail(err);
    if (!build_gameplay(base_gp, lt, gameplay, log, err)) return fail(err);
    // the host meshes
    mrpw::HostMeshFile hm;
    const bool has_meshes = fs::exists(ldir / lt.meshes);
    if (has_meshes) {
        Obj mo;
        if (!read_obj(ldir / lt.meshes, lt, true, mo, err)) return fail(err);
        if (!build_host_meshes(mo, hm, log, err)) return fail(err);
    } else {
        log.say("host meshes: none (no " + lt.meshes + ")");
    }
    // written: into a fresh folder, build.json last (a build without it is not used)
    std::error_code ec;
    if (fs::is_directory(out_dir, ec))  // (its plugin's build, mod_plugins.cpp, kept)
        for (const auto& e : fs::directory_iterator(out_dir, ec))
            if (e.path().filename() != "plugin" && e.path().filename() != "plugin-build") fs::remove_all(e.path(), ec);
    fs::create_directories(out_dir, ec);
    if (!write_file(out_dir / "data.wad", data) || !write_file(out_dir / "gameplay.bin", gameplay)) return fail("cannot write into " + out_dir.generic_string());
    // the music: the build's choice (build.json's "music"), the mod's own VAG files copied into the build
    std::string music_json;
    if (lt.music_none) {
        music_json = "\"none\"";
        log.say("music: none");
    } else if (lt.music_level >= 0) {
        music_json = std::to_string(lt.music_level);
        log.say(fmt("music: level %d's", lt.music_level));
    } else if (!lt.music_loop.empty()) {
        std::vector<std::string> names;
        for (const auto& [src, dst] : {std::pair<std::string, std::string>{lt.music_intro, "music_intro.vag"}, {lt.music_loop, "music_loop.vag"}}) {
            if (src.empty()) continue;
            std::vector<uint8_t> v;
            if (!read_file(ldir / src, v)) return fail(src + " cannot be read");
            if (v.size() < 0x30 || std::memcmp(v.data(), "VAGp", 4) != 0) return fail(src + ": not a VAG (no \"VAGp\" header): see docs/mods.md, \"Level mods\"");
            const uint32_t size = static_cast<uint32_t>(v[12]) << 24 | static_cast<uint32_t>(v[13]) << 16 | static_cast<uint32_t>(v[14]) << 8 | v[15];
            const uint32_t rate = static_cast<uint32_t>(v[16]) << 24 | static_cast<uint32_t>(v[17]) << 16 | static_cast<uint32_t>(v[18]) << 8 | v[19];
            if (size == 0 || size + 0x30 > v.size() || rate < 8000 || rate > 48000)
                return fail(fmt("%s: its VAG header's size %u or rate %u do not fit the file (%zu bytes)", src.c_str(), size, rate, v.size()));
            v.resize(align_up(v.size(), 2048), 0);
            if (!write_file(out_dir / dst, v)) return fail("cannot write into " + out_dir.generic_string());
            names.push_back("\"" + dst + "\"");
            log.say(fmt("music: %s, %u Hz, %.1f s, as %s", src.c_str(), rate, static_cast<double>(size) / 16.0 * 28.0 / rate, dst.c_str()));
        }
        music_json = "[" + names[0] + (names.size() > 1 ? ", " + names[1] : std::string()) + "]";
    }
    if (!hm.classes.empty() && !mrpw::write_host_meshes((out_dir / "host_meshes.mrpc").string(), hm, err)) return fail(err);
    // the MRP world of the composed data file, the host meshes' classes and textures added
    const fs::path wdir = out_dir / "mrp" / "world" / ("level" + std::to_string(m.level_base));
    if (!export_mrp_world_wad(std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.data()), data.size()), m.level_base, wdir.string()))
        return fail("the MRP world export of the composed level failed");
    if (!hm.classes.empty()) {
        mrpw::HostMeshes back;
        if (!mrpw::read_host_meshes((out_dir / "host_meshes.mrpc").string(), back, err)) return fail(err);
        mrpw::World w;
        if (!mrpw::read_world((wdir / "world.mrpw").string(), w)) return fail("the composed world cannot be read back");
        if (!w.ties.empty()) return fail("the composed world has tie classes of its own");
        w.ties = std::move(back.classes);
        if (!mrpw::write_world((wdir / "world.mrpw").string(), w) || !mrp::write_textures((wdir / "textures_tie.mrpt").string(), back.textures))
            return fail("cannot write the host meshes into the MRP world");
    }
    log.say(fmt("MRP world: %s", wdir.generic_string().c_str()));
    {
        std::ofstream lf(out_dir / "compose.log");
        lf << log.text;
    }
    std::ofstream bj(out_dir / "build.json");
    bj << "{\n  \"composer\": " << kLevelComposerVersion << ",\n  \"format\": " << m.level_format << ",\n  \"id\": \"" << m.level_id << "\",\n  \"base\": " << m.level_base
       << (music_json.empty() ? std::string() : ",\n  \"music\": " + music_json) << ",\n  \"objects\": " << lt.mobys.size() << ",\n  \"inputs\": \"" << stamp_hex(mods_level_inputs_stamp(m.dir)) << "\"\n}\n";
    if (!bj) return fail("cannot write build.json");
    std::cout << "[compose] " << folder << ": built into " << out_dir.generic_string() << "\n";
    return true;
}

// orig: (host) see level_compose.h
bool compose_level_mods(bool force) {
    std::error_code ec;
    const fs::path root = mods_dir();
    if (!fs::is_directory(root, ec)) return true;
    std::vector<std::string> folders;
    for (const auto& e : fs::directory_iterator(root, ec)) {
        if (!e.is_directory(ec)) continue;
        const std::string f = e.path().filename().string();
        if (f.empty() || f[0] == '.' || f[0] == '_') continue;
        ModInfo m;
        m.folder = f;
        m.dir = e.path();
        if (mods_manifest_read(m) && m.level) folders.push_back(f);
    }
    std::sort(folders.begin(), folders.end());
    bool ok = true;
    for (const std::string& f : folders) ok = compose_level_mod(f, force) && ok;
    return ok;
}

}  // namespace rac1::setup
