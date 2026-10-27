#include <formats/animation.h>

#include <algorithm>
#include <cstring>
#include <iostream>

namespace rac1 {

namespace {

// Reads from the class's bytes; every read says whether it fitted.
class ClassBytes {
public:
    explicit ClassBytes(std::span<const std::byte> b) : b_(b) {}

    template <class T>
    bool read(std::size_t at, T& out) const {
        if (at + sizeof(T) > b_.size()) return false;
        std::memcpy(&out, b_.data() + at, sizeof(T));
        return true;
    }
    // `out` filled from `at` (its current size); false, and `out` untouched, when it does not fit.
    template <class T>
    bool fill(std::size_t at, std::vector<T>& out) const {
        const std::size_t n = out.size() * sizeof(T);
        if (at + n > b_.size()) return false;
        if (n != 0) std::memcpy(out.data(), b_.data() + at, n);  // (an empty `out`: data() may be null)
        return true;
    }
    template <class T>
    T word(std::size_t at) const {
        T v;
        std::memcpy(&v, b_.data() + at, sizeof(T));
        return v;
    }

private:
    std::span<const std::byte> b_;
};

// The class header's fields used here (2).
constexpr std::size_t kClassHeaderBytes = 0x48;
constexpr std::size_t kJointCount = 0x08;    // u8
constexpr std::size_t kSequenceCount = 0x0c; // u8
constexpr std::size_t kShadowCount = 0x0f;   // u8
constexpr std::size_t kSkeleton = 0x14;      // s32
constexpr std::size_t kJointBinds = 0x18;    // s32
constexpr std::size_t kJointLists = 0x1c;    // s32
constexpr std::size_t kSequenceTable = 0x48; // s32 per sequence

// A sequence's 0x1c-byte head.
constexpr std::size_t kSeqHeadBytes = 0x1c;

// A table of up to 111 entries (the hero's joint count): as many as fit, else `joints`.
template <class T>
std::vector<T> joint_table(const ClassBytes& cls, std::int32_t at, std::uint8_t joints) {
    std::vector<T> rows(std::max<std::size_t>(joints, 111));
    if (!cls.fill(static_cast<std::size_t>(at), rows)) {
        rows.resize(joints);
        cls.fill(static_cast<std::size_t>(at), rows);
    }
    return rows;
}

// The joint lists: a count, the lists' offsets, each list (s16 count, s16 count, the two byte
// lists, 0xff). Empty when one does not fit.
std::vector<JointLists> read_joint_lists(const ClassBytes& cls, std::size_t at) {
    std::int32_t count = 0;
    if (!cls.read(at, count)) return {};
    std::vector<JointLists> out;
    out.reserve(count);
    for (std::int32_t i = 0; i < count; ++i) {
        std::int32_t list = 0;
        if (!cls.read(at + 4 + i * 4, list)) {
            std::cerr << "[animation] Warning: failed to read the joint lists\n";
            return {};
        }
        const std::size_t p = static_cast<std::size_t>(list);
        std::int16_t n1 = 0, n2 = 0;
        if (!cls.read(p, n1) || !cls.read(p + 2, n2)) {
            std::cerr << "[animation] Warning: failed to read the joint lists\n";
            return {};
        }
        JointLists l;
        l.first.resize(n1);
        l.second.resize(n2);
        if (!cls.fill(p + 4, l.first) || !cls.fill(p + 4 + n1, l.second)) {
            std::cerr << "[animation] Warning: failed to read the joint lists\n";
            return {};
        }
        std::uint8_t end = 0;
        if (!cls.read(p + 4 + n1 + n2, end) || end != 0xff) {
            std::cerr << "[animation] Warning: joint list " << i << " does not end with 0xff (" << int{end} << ")\n";
        }
        out.push_back(std::move(l));
    }
    return out;
}

// A plain frame: a 16-byte head (word_0 f32, word_4 u16, -, -, extra track count u16 at +0xa,
// word_c u16, position track count u16 at +0xe), then a rotation per joint, the extra tracks and
// the position tracks, 8 bytes each.
bool read_plain_frame(const ClassBytes& cls, std::size_t at, std::uint8_t joints, Keyframe& f) {
    std::uint8_t head[16];
    if (!cls.read(at, head)) return false;
    std::uint16_t extras, positions;
    std::memcpy(&f.plain.word_0, head, 4);
    std::memcpy(&f.plain.word_4, head + 4, 2);
    std::memcpy(&extras, head + 0xa, 2);
    std::memcpy(&f.plain.word_c, head + 0xc, 2);
    std::memcpy(&positions, head + 0xe, 2);
    std::size_t p = at + 16;
    f.plain.rotations.resize(joints);
    if (!cls.fill(p, f.plain.rotations)) return false;
    p += joints * 8;
    f.plain.extra_tracks.resize(extras);
    if (extras > 0) {
        if (!cls.fill(p, f.plain.extra_tracks)) return false;
        p += extras * 8;
    }
    f.plain.position_tracks.resize(positions);
    return positions == 0 || cls.fill(p, f.plain.position_tracks);
}

// A packed sequence's frame stream entry: a flag byte whose three 2-bit fields (3 meaning 0) give
// how many bytes follow it.
bool read_stream(const ClassBytes& cls, std::size_t& p, std::size_t entries, std::vector<std::uint8_t>& out) {
    for (std::size_t i = 0; i < entries; ++i) {
        std::uint8_t flags = 0;
        if (!cls.read(p++, flags)) return false;
        out.push_back(flags);
        for (int field = 0; field < 3; ++field) {
            int n = (flags >> (2 * field)) & 3;
            if (n == 3) n = 0;
            for (int k = 0; k < n; ++k) {
                std::uint8_t b = 0;
                if (!cls.read(p++, b)) return false;
                out.push_back(b);
            }
        }
    }
    return true;
}

// A packed sequence: after the triggers, a word giving the three blocks' ends (10 / 11 / 11 bits,
// + 4), the base pose (3 halfwords per joint), the extra / position track counts (bytes) and the
// tracks; each frame then holds two halfwords, the three blocks, the joint mask and the two
// streams.
bool read_packed(const ClassBytes& cls, std::size_t at, std::uint8_t joints, const std::vector<std::int32_t>& table,
                 AnimSequence& seq) {
    std::uint32_t ends = 0;
    if (!cls.read(at, ends)) return false;
    const std::uint32_t end_a = 4 + (ends & 0x3ff);
    const std::uint32_t end_b = 4 + ((ends >> 10) & 0x7ff);
    const std::uint32_t end_c = 4 + (ends >> 21);

    bool ok = true;
    seq.packed_base.rotations.resize(joints * 3);
    if (!cls.fill(at + 4, seq.packed_base.rotations)) ok = false;
    const std::size_t counts = at + 4 + joints * 6;
    std::uint8_t extras = 0, positions = 0;
    if (!cls.read(counts, extras) || !cls.read(counts + 1, positions)) {
        ok = false;
    } else {
        seq.packed_base.extra_tracks.resize(extras);
        if (extras > 0 && !cls.fill(counts + 2, seq.packed_base.extra_tracks)) ok = false;
        seq.packed_base.position_tracks.resize(positions);
        if (positions > 0 && !cls.fill(counts + 2 + extras * 8, seq.packed_base.position_tracks)) ok = false;
    }
    if (!ok) return false;

    const std::size_t mask_bytes = (joints + std::size_t{7}) / 8;
    seq.frames.reserve(table.size());
    for (std::int32_t entry : table) {
        const std::size_t f = static_cast<std::size_t>(entry & 0x0fffffff);
        Keyframe k;
        if (!cls.read(f, k.packed.word_0) || !cls.read(f + 2, k.packed.word_2)) return false;
        struct Block { std::vector<std::uint8_t>& out; std::size_t from; std::size_t size; };
        const Block blocks[] = {{k.packed.block_a, 4, end_a - 4},
                                {k.packed.block_b, end_a, end_b - end_a},
                                {k.packed.block_c, end_b, end_c - end_b},
                                {k.packed.joint_mask, end_c, mask_bytes}};
        for (const Block& b : blocks) {
            b.out.resize(b.size);
            if (b.size > 0 && !cls.fill(f + b.from, b.out)) return false;
        }
        std::size_t p = f + end_c + mask_bytes;
        if (!read_stream(cls, p, extras, k.packed.extra_stream) || !read_stream(cls, p, positions, k.packed.position_stream)) {
            return false;
        }
        seq.frames.push_back(std::move(k));
    }
    return true;
}

// The sequence at `at`; nothing when part of it does not fit.
std::optional<AnimSequence> read_sequence(const ClassBytes& cls, std::size_t at, std::uint8_t joints, int number) {
    std::uint8_t head[kSeqHeadBytes];
    if (!cls.read(at, head)) {
        std::cerr << "[animation] Warning: sequence " << number << " at " << at << " does not fit\n";
        return std::nullopt;
    }
    AnimSequence seq;
    std::memcpy(seq.bounding_sphere.data(), head, 16);
    const std::uint8_t frame_count = head[0x10];
    seq.sound_count = head[0x11];
    const std::uint8_t trigger_count = head[0x12];
    seq.byte_13 = head[0x13];
    std::uint32_t trigger_block, info;
    std::memcpy(&trigger_block, head + 0x14, 4);
    std::memcpy(&info, head + 0x18, 4);
    seq.info = static_cast<std::int32_t>(info);

    // The frame table (one offset per frame, from the class's start; any of the top 4 bits set
    // marks the sequence packed), then the triggers.
    const std::size_t table_at = at + kSeqHeadBytes;
    std::vector<std::int32_t> table(frame_count);
    if (!cls.fill(table_at, table)) {
        std::cerr << "[animation] Warning: sequence " << number << ": frame table does not fit\n";
        return std::nullopt;
    }
    seq.packed = std::any_of(table.begin(), table.end(), [](std::int32_t e) { return (e & 0xf0000000) != 0; });
    const std::size_t triggers_at = table_at + frame_count * 4;
    seq.triggers.resize(trigger_count);
    if (trigger_count > 0 && !cls.fill(triggers_at, seq.triggers)) {
        std::cerr << "[animation] Warning: sequence " << number << ": triggers do not fit\n";
    }
    const std::size_t after_triggers = triggers_at + trigger_count * 4;

    bool ok = true;
    if (!seq.packed) {
        seq.frames.reserve(frame_count);
        for (std::int32_t entry : table) {
            Keyframe k;
            if (!read_plain_frame(cls, static_cast<std::size_t>(entry & 0x0fffffff), joints, k)) {
                ok = false;
                break;
            }
            seq.frames.push_back(std::move(k));
        }
    } else {
        ok = read_packed(cls, after_triggers, joints, table, seq);
    }
    if (!ok) {
        std::cerr << "[animation] Warning: sequence " << number << ": frames do not fit\n";
        return std::nullopt;
    }
    if (trigger_block != 0) {
        TriggerBlock t;
        if (cls.read(trigger_block, t)) seq.trigger_block = t;
        else std::cerr << "[animation] Warning: sequence " << number << ": trigger block does not fit\n";
    }
    return seq;
}

} // namespace

DecodedMobyAnimationSection decode_moby_animations(std::span<const std::byte> moby_class_data) {
    DecodedMobyAnimationSection out;
    const ClassBytes cls(moby_class_data);
    if (moby_class_data.size() < kClassHeaderBytes) {
        std::cerr << "[animation] Error: moby class data too small (" << moby_class_data.size() << " bytes)\n";
        return out;
    }
    const std::uint8_t joints = cls.word<std::uint8_t>(kJointCount);
    const std::uint8_t sequences = cls.word<std::uint8_t>(kSequenceCount);
    out.joint_count = joints;
    out.sequence_count = sequences;
    out.shadow_count = cls.word<std::uint8_t>(kShadowCount);

    if (const std::int32_t at = cls.word<std::int32_t>(kSkeleton); at != 0) out.skeleton = joint_table<Matrix4>(cls, at, joints);
    if (const std::int32_t at = cls.word<std::int32_t>(kJointBinds); at != 0) {
        out.joint_binds = joint_table<JointBind>(cls, at, joints);
        if (out.joint_binds.size() > out.joint_count) out.joint_count = static_cast<std::uint8_t>(out.joint_binds.size());
    }
    if (const std::int32_t at = cls.word<std::int32_t>(kJointLists); at != 0) {
        out.joint_lists = read_joint_lists(cls, static_cast<std::size_t>(at));
    }

    if (sequences > 0) {
        out.sequences.resize(sequences);
        std::vector<std::int32_t> table(sequences);
        if (cls.fill(kSequenceTable, table)) {
            for (int s = 0; s < sequences; ++s) {
                if (table[s] != 0) out.sequences[s] = read_sequence(cls, static_cast<std::size_t>(table[s]), joints, s);
            }
        }
    }
    return out;
}

} // namespace rac1
