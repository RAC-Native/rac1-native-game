// The proof of the MRP world assets: see mrp_world_check.h. Host code (testing).
#include <game/render/mrp_world_check.h>

#include <game/ovl/world_vu1.h>
#include <runtime/vu/vu1.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace rac1::mrpw {

namespace {

uint32_t w32(const uint8_t* mem, uint32_t qw, int lane) {
    uint32_t v;
    std::memcpy(&v, mem + (((qw & 0x3ff) << 4) + 4 * static_cast<uint32_t>(lane)), 4);
    return v;
}
uint32_t bits(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
}
uint64_t mix(uint64_t h, uint64_t v) {
    h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    return h * 0x100000001b3ull;
}

// the shrub key: header, GIF tags (as stored), the converted positions (ITOF0 xyzw) and UVs (ITOF12 xyz, w as stored)
uint64_t shrub_key_from(const int32_t header[4], const std::vector<std::array<uint32_t, 4>>& tags, const std::vector<std::array<uint32_t, 4>>& pos,
                        const std::vector<std::array<uint32_t, 4>>& uv) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (int l = 0; l < 4; ++l) h = mix(h, static_cast<uint32_t>(header[l]));
    for (const auto& t : tags)
        for (uint32_t v : t) h = mix(h, v);
    for (const auto& t : pos)
        for (uint32_t v : t) h = mix(h, v);
    for (const auto& t : uv)
        for (uint32_t v : t) h = mix(h, v);
    return h;
}

}  // namespace

uint64_t shrub_key(const ShrubPacket& p) {
    std::vector<std::array<uint32_t, 4>> tags, pos, uv;
    for (int32_t t = 0; t < p.header[1]; ++t) {
        std::array<uint32_t, 4> q{};
        std::memcpy(q.data(), p.image.data() + 16 * static_cast<size_t>(1 + t), 16);
        tags.push_back(q);
    }
    for (size_t k = 0; k < p.pos.size(); ++k) {
        pos.push_back({bits(static_cast<float>(p.pos[k][0])), bits(static_cast<float>(p.pos[k][1])), bits(static_cast<float>(p.pos[k][2])), bits(static_cast<float>(p.pos[k][3]))});
        uv.push_back({bits(static_cast<float>(p.uv[k][0]) / 4096.0f), bits(static_cast<float>(p.uv[k][1]) / 4096.0f), bits(static_cast<float>(p.uv[k][2]) / 4096.0f),
                      static_cast<uint32_t>(static_cast<int32_t>(p.uv[k][3]))});
    }
    return shrub_key_from(p.header, tags, pos, uv);
}

namespace {
// the tie key: the header qword (0), the colour index list at 0xcc (as unpacked), the counts and the vertices' x, y, z lanes
// as the set-up program converted them (ITOF0; the UV qwords ITOF12)
uint64_t tie_key_from(const uint32_t header[4], uint32_t nd, uint32_t nf, const std::vector<uint32_t>& idx, const std::vector<uint32_t>& verts) {
    uint64_t h = 0x84222325cbf29ce4ull;
    for (int l = 0; l < 4; ++l) h = mix(h, header[l]);
    h = mix(h, nd);
    h = mix(h, nf);
    for (uint32_t v : idx) h = mix(h, v);
    for (uint32_t v : verts) h = mix(h, v);
    return h;
}
}  // namespace

uint64_t tie_key(const TiePacket& p) {
    uint32_t header[4];
    std::memcpy(header, p.image.data(), 16);
    std::vector<uint32_t> idx;
    for (const auto& c : p.colour_index)
        for (uint8_t b : c) idx.push_back(b);
    std::vector<uint32_t> verts;
    for (const auto& d : p.dinky)
        for (int l = 0; l < 8; ++l)
            if ((l & 3) != 3) verts.push_back(bits(static_cast<float>(d[static_cast<size_t>(l)]) * (l < 4 ? 1.0f : 1.0f / 4096.0f)));
    for (const auto& f : p.fat)
        for (int l = 0; l < 12; ++l)
            if ((l & 3) != 3) verts.push_back(bits(static_cast<float>(f[static_cast<size_t>(l)]) * (l < 8 ? 1.0f : 1.0f / 4096.0f)));
    return tie_key_from(header, p.n_dinky, p.n_fat, idx, verts);
}

bool WorldIndex::find_tie(const uint8_t* mem, Hit& hit) const {
    uint32_t header[4];
    for (int l = 0; l < 4; ++l) header[l] = w32(mem, 0, l);
    // the vertex counts from the control qwords (after the prims: 1 + 5 n, n = 1..4: the first that gives a plausible header)
    // (tie_key_from's hash, computed in place)
    for (uint32_t nprim = 1; nprim <= 4; ++nprim) {
        const uint32_t cq = 1 + 5 * nprim;
        const uint32_t nd = w32(mem, cq + 2, 2), nf = w32(mem, cq + 2, 3);
        if (nd > 255 || nf > 255) continue;
        uint64_t h = 0x84222325cbf29ce4ull;
        for (int l = 0; l < 4; ++l) h = mix(h, header[l]);
        h = mix(h, nd);
        h = mix(h, nf);
        // (the colour index entries: dinky 4 per qword, fat one qword each)
        const uint32_t nidx = (nd + 3) / 4 + nf;
        for (uint32_t k = 0; k < nidx; ++k)
            for (int l = 0; l < 4; ++l) h = mix(h, w32(mem, 0xcc + k, l));
        for (uint32_t q = 0; q < 2 * nd + 3 * nf; ++q)
            for (int l = 0; l < 3; ++l) h = mix(h, w32(mem, 0x32 + q, l));
        auto [a, b] = tie_.equal_range(h);
        if (a != b) {
            hit = a->second;
            return true;
        }
    }
    return false;
}

namespace {
// the tfrag key: the image's first 5 qwords (the lists' header unpack) and the lod
uint64_t tfrag_key_from(const uint8_t* q5, int lod) {
    uint64_t h = 0x9e3779b97f4a7c15ull;
    for (int i = 0; i < 20; ++i) {
        uint32_t v;
        std::memcpy(&v, q5 + 4 * i, 4);
        h = mix(h, v);
    }
    return mix(h, static_cast<uint64_t>(lod));
}
}  // namespace

bool WorldIndex::find_tfrag(const uint8_t* mem, uint32_t base, int lod, Hit& hit) const {
    uint8_t q5[80];
    for (uint32_t q = 0; q < 5; ++q) std::memcpy(q5 + 16 * q, mem + (((base + q) & 0x3ff) << 4), 16);
    auto [a, b] = tfrag_.equal_range(tfrag_key_from(q5, lod));
    for (auto it = a; it != b; ++it) {
        const TfragEntry& te = it->second.world->tfrags[static_cast<size_t>(it->second.cls)];
        const TfragImage& im = te.lods[static_cast<size_t>(lod)];
        uint32_t texaddr = 0;
        std::memcpy(&texaddr, im.image.data() + 16 * 4 + 12, 4);  // (the header's qword 4, w: the texture prims' address; rebuilt at the load, patched per frame)
        const size_t tex0 = texaddr, tex1 = tex0 + 5 * te.textures.size();
        bool same = true;
        for (size_t q = 0; q < im.written.size() && same; ++q) {
            if (im.written[q] == 0) continue;
            if (q >= tex0 && q < tex1) continue;
            if (q >= im.colour_addr && q < im.colour_addr + 2u * im.colour_count && ((q - im.colour_addr) & 1) == 0) continue;  // (the colours' qwords)
            if (q >= 5 && q <= 8) continue;  // (the view matrix)
            same = std::memcmp(im.image.data() + 16 * q, mem + (((base + q) & 0x3ff) << 4), 16) == 0;
            static const bool dbg = std::getenv("MRPW_TFDBG") != nullptr;
            if (!same && dbg) {
                uint32_t a[4], b2[4];
                std::memcpy(a, im.image.data() + 16 * q, 16);
                std::memcpy(b2, mem + (((base + q) & 0x3ff) << 4), 16);
                std::fprintf(stderr, "[tfdbg] level %d entry %d lod %d: qword %zx differs: image %08x %08x %08x %08x vu %08x %08x %08x %08x\n", it->second.world->level,
                             it->second.cls, lod, q, a[0], a[1], a[2], a[3], b2[0], b2[1], b2[2], b2[3]);
            }
        }
        if (same) {
            hit = it->second;
            return true;
        }
    }
    return false;
}

void WorldIndex::add(const World* w) {
    for (size_t c = 0; c < w->tfrags.size(); ++c)
        for (int l = 0; l < 3; ++l)
            if (w->tfrags[c].lods[static_cast<size_t>(l)].image.size() >= 80)
                tfrag_.insert({tfrag_key_from(w->tfrags[c].lods[static_cast<size_t>(l)].image.data(), l), Hit{w, static_cast<int>(c), l}});
    for (size_t c = 0; c < w->ties.size(); ++c)
        for (size_t k = 0; k < w->ties[c].packets.size(); ++k) tie_.insert({tie_key(w->ties[c].packets[k]), Hit{w, static_cast<int>(c), static_cast<int>(k)}});
    for (size_t c = 0; c < w->shrubs.size(); ++c)
        for (size_t k = 0; k < w->shrubs[c].packets.size(); ++k) shrub_.insert({shrub_key(w->shrubs[c].packets[k]), Hit{w, static_cast<int>(c), static_cast<int>(k)}});
}

bool WorldIndex::find_shrub(const uint8_t* mem, uint32_t base, Hit& hit) const {
    int32_t header[4];
    for (int l = 0; l < 4; ++l) header[l] = static_cast<int32_t>(w32(mem, base, l));
    if (header[1] < 0 || header[1] > 64 || header[2] <= 0 || header[2] > 256 || header[3] <= 0) return false;
    std::vector<std::array<uint32_t, 4>> tags, pos, uv;
    for (int32_t t = 0; t < header[1]; ++t) tags.push_back({w32(mem, base + 1 + static_cast<uint32_t>(t), 0), w32(mem, base + 1 + static_cast<uint32_t>(t), 1),
                                                            w32(mem, base + 1 + static_cast<uint32_t>(t), 2), w32(mem, base + 1 + static_cast<uint32_t>(t), 3)});
    const uint32_t n = static_cast<uint32_t>(header[2]), vo = base + static_cast<uint32_t>(header[3]);
    for (uint32_t k = 0; k < n; ++k) {
        pos.push_back({w32(mem, vo + k, 0), w32(mem, vo + k, 1), w32(mem, vo + k, 2), w32(mem, vo + k, 3)});
        uv.push_back({w32(mem, vo + n + k, 0), w32(mem, vo + n + k, 1), w32(mem, vo + n + k, 2), w32(mem, vo + n + k, 3)});
    }
    auto [a, b] = shrub_.equal_range(shrub_key_from(header, tags, pos, uv));
    if (a == b) return false;
    hit = a->second;
    return true;
}

Program program_of(const uint64_t* micro) {
    int n = 0;
    const WorldVu1Program* p = world_vu1_programs(n);
    // (host, RAC1_PERF_VU1_LOOKUP as vu1.cpp's ports) the walker asks after every MPG (the passes upload their programs several
    // times a frame) and the answer hashed up to six programs' code a byte at a time; now the last few micro images met (their
    // first `span` words, every program's length) are kept with their answer and an image met again is compared (memcmp) instead
    // of hashed: the same words, the same hashes, the same answer
    static const bool cached = [] {
        const char* e = std::getenv("RAC1_PERF_VU1_LOOKUP");
        return e == nullptr || std::atoi(e) != 0;
    }();
    auto hashed = [&] {
        for (int i = 0; i < n; ++i)
            if (vu1::code_hash(micro, p[i].n) == p[i].hash) return static_cast<Program>(i);
        return Program::None;
    };
    if (!cached) return hashed();
    uint32_t span = 0;
    for (int i = 0; i < n; ++i) span = std::max(span, p[i].n);
    struct Seen {
        std::vector<uint64_t> words;
        Program prog = Program::None;
    };
    constexpr unsigned kSeen = 8;
    thread_local Seen seen[kSeen];
    thread_local unsigned next = 0;
    for (const Seen& s : seen)
        if (s.words.size() == span && std::memcmp(s.words.data(), micro, static_cast<size_t>(span) * 8) == 0) return s.prog;
    Seen& s = seen[next++ % kSeen];
    s.words.assign(micro, micro + span);
    s.prog = hashed();
    return s.prog;
}

std::string CheckStats::summary() const {
    static const char* names[6] = {"tfrag A", "tfrag B", "tie A", "tie B", "shrub A", "shrub B"};
    std::string s = "starts:";
    for (const auto& [k, n] : cover) s += " " + k + " x" + std::to_string(n) + ";";
    s += "\n";
    char buf[400];
    for (int i = 0; i < 6; ++i) {
        const Kind& k = kind[i];
        if (k.starts == 0) continue;
        std::snprintf(buf, sizeof buf,
                      "%s: %llu starts (%llu not found), %llu kicks (%llu with another count), %llu qwords: %llu exact, %llu within tolerance, %llu differ "
                      "(tag %llu, A+D %llu, STQ %llu, RGBA %llu, XYZF %llu)%s\n",
                      names[i], static_cast<unsigned long long>(k.starts), static_cast<unsigned long long>(k.unknown), static_cast<unsigned long long>(k.kicks),
                      static_cast<unsigned long long>(k.kick_count_differ), static_cast<unsigned long long>(k.qwords), static_cast<unsigned long long>(k.exact),
                      static_cast<unsigned long long>(k.within), static_cast<unsigned long long>(k.differ), static_cast<unsigned long long>(k.part_differ[0]),
                      static_cast<unsigned long long>(k.part_differ[1]), static_cast<unsigned long long>(k.part_differ[2]), static_cast<unsigned long long>(k.part_differ[3]),
                      static_cast<unsigned long long>(k.part_differ[4]), k.clipped_skipped != 0 ? " [clipping program: compared where unclipped]" : "");
        s += buf;
        if (k.tri_ref + k.tri_flagged != 0) {
            std::snprintf(buf, sizeof buf, "    clipping program: %llu reference triangles unflagged, %llu found whole among the kicked ones; %llu flagged (clipped); %llu kicked, %llu not matched (the clipped pieces)\n",
                          static_cast<unsigned long long>(k.tri_ref), static_cast<unsigned long long>(k.tri_found), static_cast<unsigned long long>(k.tri_flagged),
                          static_cast<unsigned long long>(k.tri_kicked), static_cast<unsigned long long>(k.tri_kicked_unmatched));
            s += buf;
        }
    }
    return s;
}

namespace {

using Packets = std::vector<std::vector<std::array<uint32_t, 4>>>;

// one qword compared: 0 exact, 1 within tolerance, 2 differ
int compare(uint8_t part, const std::array<uint32_t, 4>& want, const std::array<uint32_t, 4>& got) {
    int lanes = 4;
    if (part == 2) lanes = 3;  // (STQ's w: not read by the GS)
    bool exact = true;
    for (int l = 0; l < lanes; ++l) exact = exact && want[static_cast<size_t>(l)] == got[static_cast<size_t>(l)];
    if (exact) return 0;
    if (part == 2) {
        for (int l = 0; l < 3; ++l) {
            float a, b;
            std::memcpy(&a, &want[static_cast<size_t>(l)], 4);
            std::memcpy(&b, &got[static_cast<size_t>(l)], 4);
            if (!(std::fabs(a - b) <= 1e-5f * std::max(std::fabs(a), std::fabs(b)) + 1e-30f)) return 2;
        }
        return 1;
    }
    if (part == 4) {
        for (int l = 0; l < 4; ++l) {
            const int64_t a = static_cast<int32_t>(want[static_cast<size_t>(l)]), b = static_cast<int32_t>(got[static_cast<size_t>(l)]);
            const int64_t tol = l == 2 ? std::max<int64_t>(64, std::llabs(a) >> 16) : 1;
            if (std::llabs(a - b) > tol) return 2;
        }
        return 1;
    }
    return 2;
}

void compare_kick(const Kicked& want, const std::vector<std::array<uint32_t, 4>>& got, CheckStats::Kind& k, CheckStats& stats, const char* what) {
    const size_t n = std::min(want.q.size(), got.size());
    if (want.q.size() != got.size()) {
        ++k.kick_count_differ;
        if (stats.notes.size() < 40) stats.notes.push_back(std::string(what) + ": kick of " + std::to_string(got.size()) + " qwords, the reference " + std::to_string(want.q.size()));
    }
    for (size_t i = 0; i < n; ++i) {
        ++k.qwords;
        const int r = compare(want.part[i], want.q[i], got[i]);
        if (r == 0) ++k.exact;
        else if (r == 1) ++k.within;
        else {
            ++k.differ;
            ++k.part_differ[want.part[i] < 5 ? want.part[i] : 0];
            if (stats.notes.size() < 40) {
                char buf[300];
                std::snprintf(buf, sizeof buf, "%s: qword %zu (part %d): want %08x %08x %08x %08x got %08x %08x %08x %08x", what, i, want.part[i], want.q[i][0], want.q[i][1], want.q[i][2],
                              want.q[i][3], got[i][0], got[i][1], got[i][2], got[i][3]);
                stats.notes.push_back(buf);
            }
        }
    }
}

// The GS's triangles of a kicked GIF packet (PACKED: STQ / RGBAQ / XYZF2 loops and A+D; PRE or a PRIM write restart the
// primitive; strip, fan, list; an XYZF2 with ADC (bit 15 of its w) closes no triangle). `clip`: per qword (or empty).
struct Tri {
    std::array<std::array<uint32_t, 4>, 3> stq{}, rgba{}, xyz{};
    bool flagged = false;
};
void gs_triangles(const std::vector<std::array<uint32_t, 4>>& pk, const std::vector<uint8_t>* clip, std::vector<Tri>& out) {
    struct V {
        std::array<uint32_t, 4> stq, rgba, xyz;
        bool flagged;
    };
    std::vector<V> queue;
    int type = 4;
    std::array<uint32_t, 4> stq{}, rgba{};
    size_t i = 0;
    while (i < pk.size()) {
        const uint64_t lo = static_cast<uint64_t>(pk[i][0]) | static_cast<uint64_t>(pk[i][1]) << 32;
        const uint64_t hi = static_cast<uint64_t>(pk[i][2]) | static_cast<uint64_t>(pk[i][3]) << 32;
        ++i;
        const uint32_t nloop = lo & 0x7fff, flg = (lo >> 58) & 3, nreg = ((lo >> 60) & 0xf) == 0 ? 16 : (lo >> 60) & 0xf;
        if (((lo >> 46) & 1) != 0) {
            type = static_cast<int>((lo >> 47) & 7);
            queue.clear();
        }
        if (flg != 0) break;
        for (uint32_t n = 0; n < nloop && i < pk.size(); ++n)
            for (uint32_t r = 0; r < nreg && i < pk.size(); ++r, ++i) {
                const uint32_t reg = static_cast<uint32_t>(hi >> (4 * r)) & 0xf;
                const auto& q = pk[i];
                if (reg == 0xe) {
                    if ((q[2] & 0xff) == 0x00) {
                        type = static_cast<int>(q[0] & 7);
                        queue.clear();
                    }
                } else if (reg == 2) {
                    stq = q;
                } else if (reg == 1) {
                    rgba = q;
                } else if (reg == 4) {
                    const bool adc = ((q[3] >> 15) & 1) != 0;
                    queue.push_back({stq, rgba, q, clip != nullptr && i < clip->size() && (*clip)[i] != 0});
                    const size_t need = (type == 3 || type == 4 || type == 5) ? 3 : 0;
                    if (need == 0) {
                        queue.clear();
                        continue;
                    }
                    if (queue.size() >= need) {
                        if (!adc) {
                            Tri t;
                            for (int k = 0; k < 3; ++k) {
                                const V& v = type == 5 && k == 0 ? queue.front() : queue[queue.size() - 3 + static_cast<size_t>(k)];
                                t.stq[static_cast<size_t>(k)] = v.stq;
                                t.rgba[static_cast<size_t>(k)] = v.rgba;
                                t.xyz[static_cast<size_t>(k)] = v.xyz;
                                t.flagged = t.flagged || v.flagged;
                            }
                            out.push_back(t);
                        }
                        if (type == 3) queue.clear();
                        else if (type == 4) queue.erase(queue.begin());
                        else queue.erase(queue.begin() + 1);
                    }
                }
            }
        if (((lo >> 15) & 1) != 0 && i >= pk.size()) break;
    }
}

bool same_vertex(const Tri& a, int ia, const Tri& b, int ib) {
    for (int l = 0; l < 3; ++l) {
        float x, y;
        std::memcpy(&x, &a.stq[static_cast<size_t>(ia)][static_cast<size_t>(l)], 4);
        std::memcpy(&y, &b.stq[static_cast<size_t>(ib)][static_cast<size_t>(l)], 4);
        if (!(std::fabs(x - y) <= 1e-4f * std::max(std::fabs(x), std::fabs(y)) + 1e-30f)) return false;
    }
    for (int l = 0; l < 4; ++l)
        if (a.rgba[static_cast<size_t>(ia)][static_cast<size_t>(l)] != b.rgba[static_cast<size_t>(ib)][static_cast<size_t>(l)]) return false;
    for (int l = 0; l < 3; ++l) {
        const int64_t u = static_cast<int32_t>(a.xyz[static_cast<size_t>(ia)][static_cast<size_t>(l)]), v = static_cast<int32_t>(b.xyz[static_cast<size_t>(ib)][static_cast<size_t>(l)]);
        const int64_t tol = l == 2 ? std::max<int64_t>(256, std::llabs(u) >> 12) : 2;
        if (std::llabs(u - v) > tol) return false;
    }
    return (a.xyz[static_cast<size_t>(ia)][3] & 0xff0) == (b.xyz[static_cast<size_t>(ib)][3] & 0xff0);
}

// The clipping programs: every reference triangle whose vertices the clip test leaves alone must be among the kicked ones
// (the program sends it whole); the others the program clips (counted).
void compare_triangles(const std::vector<Kicked>& want, const Packets& got, CheckStats::Kind& k, CheckStats& stats, const char* what) {
    std::vector<Tri> rt, gt;
    for (const auto& w : want) gs_triangles(w.q, &w.clip, rt);
    for (const auto& g : got) gs_triangles(g, nullptr, gt);
    std::vector<uint8_t> used(gt.size(), 0);
    k.tri_kicked += gt.size();
    for (const Tri& t : rt) {
        if (t.flagged) {
            ++k.tri_flagged;
            continue;
        }
        ++k.tri_ref;
        bool found = false;
        for (size_t j = 0; j < gt.size() && !found; ++j) {
            if (used[j] != 0) continue;
            if (same_vertex(t, 0, gt[j], 0) && same_vertex(t, 1, gt[j], 1) && same_vertex(t, 2, gt[j], 2)) {
                used[j] = 1;
                found = true;
            }
        }
        if (found) ++k.tri_found;
        else if (std::getenv("MRPW_TRIDBG") != nullptr && stats.notes.size() < 40) {
            // the kicked triangle nearest to it (by its first vertex's x / y)
            size_t best = gt.size();
            int64_t bd = INT64_MAX;
            for (size_t j = 0; j < gt.size(); ++j) {
                const int64_t d = std::llabs(static_cast<int64_t>(static_cast<int32_t>(gt[j].xyz[0][0])) - static_cast<int32_t>(t.xyz[0][0])) +
                                  std::llabs(static_cast<int64_t>(static_cast<int32_t>(gt[j].xyz[0][1])) - static_cast<int32_t>(t.xyz[0][1]));
                if (d < bd) {
                    bd = d;
                    best = j;
                }
            }
            char buf[400];
            if (best < gt.size()) {
                const Tri& g = gt[best];
                std::snprintf(buf, sizeof buf, "%s: want v0 stq %08x %08x %08x rgba %08x %08x %08x %08x xyzf %08x %08x %08x %08x | nearest stq %08x %08x %08x rgba %08x %08x %08x %08x xyzf %08x %08x %08x %08x",
                              what, t.stq[0][0], t.stq[0][1], t.stq[0][2], t.rgba[0][0], t.rgba[0][1], t.rgba[0][2], t.rgba[0][3], t.xyz[0][0], t.xyz[0][1], t.xyz[0][2], t.xyz[0][3],
                              g.stq[0][0], g.stq[0][1], g.stq[0][2], g.rgba[0][0], g.rgba[0][1], g.rgba[0][2], g.rgba[0][3], g.xyz[0][0], g.xyz[0][1], g.xyz[0][2], g.xyz[0][3]);
                stats.notes.push_back(buf);
            }
        } else if (stats.notes.size() < 40) {
            char buf[300];
            std::snprintf(buf, sizeof buf, "%s: a whole triangle not kicked (%08x %08x / %08x %08x / %08x %08x)", what, t.xyz[0][0], t.xyz[0][1], t.xyz[1][0], t.xyz[1][1], t.xyz[2][0],
                          t.xyz[2][1]);
            stats.notes.push_back(buf);
        }
    }
    for (uint8_t u : used) k.tri_kicked_unmatched += u == 0 ? 1 : 0;
}

}  // namespace

void compare_draw(Program prog, const std::vector<Kicked>& want, const std::vector<std::vector<std::array<uint32_t, 4>>>& got, CheckStats& stats, const char* what) {
    if (prog == Program::None) return;
    CheckStats::Kind& k = stats.kind[static_cast<int>(prog)];
    ++k.starts;
    if (prog == Program::TfragB || prog == Program::TieB || prog == Program::ShrubB) {
        ++k.kicks;
        compare_triangles(want, got, k, stats, what);
        return;
    }
    if (want.size() != got.size()) {
        // (the same GS stream cut into other kicks, e.g. tie A sending a packet's texture prims in a kick of their own: compared
        // as one stream)
        ++k.kick_count_differ;
        Kicked w1;
        std::vector<std::array<uint32_t, 4>> g1;
        for (const Kicked& x : want) {
            w1.q.insert(w1.q.end(), x.q.begin(), x.q.end());
            w1.part.insert(w1.part.end(), x.part.begin(), x.part.end());
        }
        for (const auto& x : got) g1.insert(g1.end(), x.begin(), x.end());
        if (g1.size() > w1.q.size() && stats.notes.size() < 40) {  // (what the program kicked past the reference's stream)
            char buf[200];
            const auto& q = g1[w1.q.size()];
            std::snprintf(buf, sizeof buf, "%s: past the reference: %08x %08x %08x %08x (%zu more qwords)", what, q[0], q[1], q[2], q[3], g1.size() - w1.q.size());
            stats.notes.push_back(buf);
        }
        ++k.kicks;
        compare_kick(w1, g1, k, stats, what);
        return;
    }
    for (size_t i = 0; i < std::min(want.size(), got.size()); ++i) {
        ++k.kicks;
        compare_kick(want[i], got[i], k, stats, what);
    }
}

}  // namespace rac1::mrpw
