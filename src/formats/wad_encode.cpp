// See wad_encode.h.
#include <formats/wad_encode.h>

#include <algorithm>
#include <cstring>

namespace rac1 {

namespace {

constexpr std::size_t kHalf = 0x2000;
constexpr int kMaxDist = 16384;
constexpr int kMaxLen = 288;

enum class St { Top, Next };

// The stream in halves: each packet is placed whole in the current half, which closes with `12 00 00`.
struct Writer {
    std::vector<std::uint8_t> body;  // the halves closed so far
    std::vector<std::uint8_t> cur;
    St state = St::Top;
    static constexpr std::size_t kRoom = kHalf - 3 - 3;  // the end code and the decoder's three-byte look past a packet

    void close_half() {
        cur.insert(cur.end(), {0x12, 0x00, 0x00});
        cur.resize(kHalf, 0);
        body.insert(body.end(), cur.begin(), cur.end());
        cur.clear();
        state = St::Top;
    }
    std::size_t space() const { return kRoom - cur.size(); }
    // a match or a no-op (valid in either state), or a literal run when the state is Top
    void packet(const std::vector<std::uint8_t>& b, St next) {
        if (b.size() > space()) close_half();
        cur.insert(cur.end(), b.begin(), b.end());
        state = next;
    }
    std::vector<std::uint8_t> finish() {
        std::vector<std::uint8_t> out = {'W', 'A', 'D'};
        const std::uint32_t total = static_cast<std::uint32_t>(0x10 + body.size() + cur.size());
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(total >> (8 * i)));
        out.resize(0x10, 0);
        out.insert(out.end(), body.begin(), body.end());
        out.insert(out.end(), cur.begin(), cur.end());
        return out;
    }
};

std::vector<std::uint8_t> run_code(std::size_t c) {
    if (c <= 18) return {static_cast<std::uint8_t>(c - 3)};
    return {0, static_cast<std::uint8_t>(c - 18)};
}

// n literals data[i, i + n) (any count), from whatever state the stream is in
void emit_literals(Writer& w, std::span<const std::uint8_t> data, std::size_t i, std::size_t n) {
    while (n > 0) {
        if (n < 4) {  // (the no-op match carries them)
            std::vector<std::uint8_t> p = {0x11, static_cast<std::uint8_t>(n), 0};
            p.insert(p.end(), data.begin() + static_cast<std::ptrdiff_t>(i), data.begin() + static_cast<std::ptrdiff_t>(i + n));
            w.packet(p, St::Next);
            return;
        }
        if (w.state == St::Next) w.packet({0x11, 0x00, 0x00}, St::Top);
        const std::size_t room = w.space() >= 2 ? w.space() - 2 : 0;
        std::size_t c = std::min<std::size_t>({n, 273, room});
        if (c < n && n - c < 4) c = n >= 4 ? n - 4 : 0;
        if (c < 4) {
            w.close_half();
            continue;
        }
        std::vector<std::uint8_t> p = run_code(c);
        p.insert(p.end(), data.begin() + static_cast<std::ptrdiff_t>(i), data.begin() + static_cast<std::ptrdiff_t>(i + c));
        w.packet(p, St::Next);
        i += c;
        n -= c;
    }
}

std::vector<std::uint8_t> match_code(int dist, int length, int k) {
    const int d = dist - 1;
    if (length >= 3 && length <= 8 && d < 2048)
        return {static_cast<std::uint8_t>(((length - 1) << 5) | ((d & 7) << 2) | k), static_cast<std::uint8_t>(d >> 3)};
    const int n = length - 2;
    const std::uint8_t t0 = static_cast<std::uint8_t>(((d & 0x3f) << 2) | k), t1 = static_cast<std::uint8_t>(d >> 6);
    if (n <= 31) return {static_cast<std::uint8_t>(0x20 | n), t0, t1};
    return {0x20, static_cast<std::uint8_t>(n - 0x1f), t0, t1};
}

struct Match {
    std::size_t pos;
    int dist, len;
};

// greedy LZ77 over data: 3-byte hash chains, `chain` candidates tried at each position
std::vector<Match> find_matches(std::span<const std::uint8_t> data, int chain) {
    const std::size_t n = data.size();
    constexpr int kBits = 16;
    std::vector<std::int32_t> head(std::size_t{1} << kBits, -1), prev(n, -1);
    auto key = [&](std::size_t p) {
        const std::uint32_t v = static_cast<std::uint32_t>(data[p]) << 16 | static_cast<std::uint32_t>(data[p + 1]) << 8 | data[p + 2];
        return (v * 2654435761u) >> (32 - kBits);
    };
    auto insert = [&](std::size_t p) {
        const std::uint32_t k = key(p);
        prev[p] = head[k];
        head[k] = static_cast<std::int32_t>(p);
    };
    std::vector<Match> out;
    std::size_t i = 0;
    while (i < n) {
        int best_len = 0, best_dist = 0;
        if (i + 3 <= n) {
            const int limit = static_cast<int>(std::min<std::size_t>(kMaxLen, n - i));
            std::int32_t cand = head[key(i)];
            for (int tries = chain; cand >= 0 && tries > 0 && static_cast<int>(i) - cand <= kMaxDist; --tries, cand = prev[static_cast<std::size_t>(cand)]) {
                const std::size_t c = static_cast<std::size_t>(cand);
                if (data[c] != data[i] || data[c + 1] != data[i + 1] || data[c + 2] != data[i + 2]) continue;
                if (best_len != 0 && data[c + static_cast<std::size_t>(best_len)] != data[i + static_cast<std::size_t>(best_len)]) continue;
                int L = 3;
                while (L < limit && data[c + static_cast<std::size_t>(L)] == data[i + static_cast<std::size_t>(L)]) ++L;
                if (L > best_len) {
                    best_len = L;
                    best_dist = static_cast<int>(i - c);
                    if (L == limit) break;
                }
            }
            insert(i);
        }
        if (best_len >= 3) {
            out.push_back({i, best_dist, best_len});
            const std::size_t end = i + static_cast<std::size_t>(best_len);
            for (std::size_t p = i + 1; p < std::min(end, n >= 2 ? n - 2 : 0); ++p) insert(p);
            i = end;
        } else {
            ++i;
        }
    }
    return out;
}

}  // namespace

// orig: (host) see wad_encode.h
std::vector<std::uint8_t> wad_encode(std::span<const std::uint8_t> data, bool lz) {
    const std::size_t n = data.size();
    const std::vector<Match> ms = lz ? find_matches(data, 8) : std::vector<Match>{};
    Writer w;
    std::size_t cursor = 0;
    for (std::size_t idx = 0; idx < ms.size(); ++idx) {
        const Match& m = ms[idx];
        if (m.pos > cursor) emit_literals(w, data, cursor, m.pos - cursor);
        const std::size_t after = m.pos + static_cast<std::size_t>(m.len);
        const std::size_t nxt = idx + 1 < ms.size() ? ms[idx + 1].pos : n;
        const int k = nxt - after <= 3 ? static_cast<int>(nxt - after) : 0;
        std::vector<std::uint8_t> p = match_code(m.dist, m.len, k);
        p.insert(p.end(), data.begin() + static_cast<std::ptrdiff_t>(after), data.begin() + static_cast<std::ptrdiff_t>(after) + k);
        w.packet(p, k != 0 ? St::Next : St::Top);
        cursor = after + static_cast<std::size_t>(k);
    }
    if (n > cursor) emit_literals(w, data, cursor, n - cursor);
    return w.finish();
}

// orig: 0x0024d8d0 (host: a check of a stream, as the game decodes it) see wad_encode.h
bool wad_decode_strict(std::span<const std::uint8_t> blob, std::vector<std::uint8_t>& out, std::string& error) {
    out.clear();
    if (blob.size() < 0x10 || std::memcmp(blob.data(), "WAD", 3) != 0) {
        error = "bad magic";
        return false;
    }
    std::uint32_t size32 = 0;
    std::memcpy(&size32, blob.data() + 3, 4);
    std::int64_t size = size32;
    std::vector<std::uint8_t> spr(2 * kHalf, 0);
    std::size_t madr = 0x10, half = 0;
    auto dma = [&] {
        std::fill(spr.begin() + static_cast<std::ptrdiff_t>(half), spr.begin() + static_cast<std::ptrdiff_t>(half + kHalf), 0);
        if (madr < blob.size()) {
            const std::size_t k = std::min(kHalf, blob.size() - madr);
            std::memcpy(spr.data() + half, blob.data() + madr, k);
        }
        half ^= kHalf;
    };
    dma();
    madr += kHalf;
    dma();
    madr += kHalf;
    size -= 0x10;
    std::int64_t pos = 0;
    std::int64_t end = size > 0x1fff ? (std::int64_t{1} << 31) : pos + size;
    bool bad = false;
    auto rd = [&](std::int64_t p) -> std::uint8_t {
        if (p < 0 || p >= static_cast<std::int64_t>(2 * kHalf)) {
            bad = true;
            return 0;
        }
        return spr[static_cast<std::size_t>(p)];
    };
    enum class S { First, Top, Next } st = S::First;
    while (!bad) {
        if (st == S::First) {
            const int lit = static_cast<int>(rd(pos)) - 0x11;
            if (lit > 0) {
                ++pos;
                for (int i = 0; i < lit; ++i) out.push_back(rd(pos++));
                st = S::Next;
            } else {
                st = S::Top;
            }
            continue;
        }
        if (pos >= end) break;
        int v = rd(pos++);
        if (st == S::Top && v < 0x10) {
            if (v == 0) v = rd(pos++) + 0xf;
            for (int i = 0; i < v + 3; ++i) out.push_back(rd(pos++));
            st = S::Next;
            continue;
        }
        bool copy = true;
        std::int64_t dist = 0;
        int n = 0;
        if (v >= 0x40) {
            dist = 1 + ((v >> 2) & 7) + rd(pos) * 8;
            ++pos;
            n = (v >> 5) - 1;
        } else if (v >= 0x20) {
            n = v & 0x1f;
            if (n == 0) n = rd(pos++) + 0x1f;
            dist = 1 + (rd(pos) >> 2) + rd(pos + 1) * 64;
            pos += 2;
        } else if (v >= 0x10) {
            const int far = (v & 8) << 11;
            n = v & 7;
            if (n == 0) n = rd(pos++) + 7;
            const int d = (rd(pos) >> 2) + rd(pos + 1) * 64;
            pos += 2;
            if (far + d == 0) {
                if (n != 1) {  // the end of a half
                    dma();
                    size -= static_cast<std::int64_t>(kHalf);
                    madr += kHalf;
                    pos = static_cast<std::int64_t>(half);
                    end = size > 0x1fff ? (std::int64_t{1} << 31) : pos + size;
                    st = S::Top;
                    continue;
                }
                copy = false;
            } else {
                dist = far + d + 0x4000;
            }
        } else {
            error = "a literal code after literals (the game's teq) at output " + std::to_string(out.size());
            return false;
        }
        if (copy) {
            const std::int64_t from = static_cast<std::int64_t>(out.size()) - dist;
            if (from < 0) {
                error = "a match before the output's start at " + std::to_string(out.size());
                return false;
            }
            for (int i = 0; i < n + 2; ++i) out.push_back(out[static_cast<std::size_t>(from + i)]);
        }
        const int k = rd(pos - 2) & 3;
        if (k == 0) {
            st = S::Top;
            continue;
        }
        for (int i = 0; i < 3; ++i) (void)rd(pos + i);  // (the decoder reads three bytes whatever k is)
        for (int i = 0; i < k; ++i) out.push_back(rd(pos + i));
        pos += k;
        st = S::Next;
    }
    if (bad) {
        error = "a read outside the scratchpad's two halves";
        return false;
    }
    return true;
}

}  // namespace rac1
