// orig: (host) Ray tracing, phases 0, 2 and 3 (see mrp_rt.h): the acceleration structures of the level's static world and of the frame's mobys, the hit's
// shading tables (Vulkan: Water Reflections Medium (RT) / High (RT), RAC1_RT=1).
#include <game/render/mrp_rt.h>
#include <game/render/mrp_lighting.h>
#include <game/render/mrp_moby_renderer.h>
#include <game/render/mrp_reflect.h>

#include <profiler/host_profiler.h>
#include <runtime/vk/vk_context.h>
#include <runtime/vk/vk_shaders.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unordered_map>

namespace rac1 {

namespace {

double now_ms() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
int env_int(const char* name, int def) {
    const char* e = std::getenv(name);
    return e != nullptr && *e != '\0' ? std::atoi(e) : def;
}
VkDeviceSize align_up(VkDeviceSize v, VkDeviceSize a) { return (v + a - 1) / a * a; }

constexpr uint32_t kDynamicCap = 1024;  // (the dynamic partition's room: the mobys' instances, phase 3 - Water Reflections High (RT))
constexpr uint32_t kMobyRebuild = 60;           // (a refitted moby's structure built anew this often: an animation's refits loosen it)
constexpr uint32_t kMobyMaxTris = 1u << 20;     // (the frame's moby triangles at most; past them a moby is left out)
constexpr VkBuildAccelerationStructureFlagsKHR kMobyFlags =
    VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;

// What a level's clusters cost (the report).
struct ClusterReport {
    uint32_t built = 0, empty = 0;
    uint64_t triangles = 0, vertices = 0;
    VkDeviceSize input_bytes = 0, before = 0, after = 0, scratch_peak = 0;
    double build_gpu = 0.0, compact_gpu = 0.0, wall = 0.0;
};
// What a commit did (the report and the per-frame numbers).
struct CommitReport {
    uint32_t partitions = 0, records = 0;
    bool update = false, kept = false;
};

// The interface the clusters and partitions are made through: the KHR acceleration structures here (KhrAccel); a real cluster /
// partitioned TLAS extension (VK_NV_cluster_acceleration_structure / VK_NV_partitioned_acceleration_structure, or a future EXT / KHR
// one) would be another implementation of it - built once from the same clusters, written per partition, committed per frame.
class RtAccel {
public:
    virtual ~RtAccel() = default;
    virtual const char* name() const = 0;
    // the clusters built once (compacted): false when none could be
    virtual bool build_clusters(const std::vector<RtCluster>& clusters, ClusterReport& r) = 0;
    // the partitions' layout: partition p's instances are [first[p], first[p] + count[p]) of the records (the last one dynamic, its
    // capacity `dyn_cap`)
    virtual bool define_partitions(const std::vector<uint32_t>& first, const std::vector<uint32_t>& count, uint32_t dyn_cap) = 0;
    // partition p's instances written (its records' visibility from `visible`, one byte per instance of it)
    virtual void partition_write(uint32_t p, const RtInstance* inst, uint32_t n, const uint8_t* visible) = 0;
    // the dynamic partition's records written as they are (the mobys' instances of their own bottom-level structures, phase 3)
    virtual void dynamic_write(const VkAccelerationStructureInstanceKHR* recs, uint32_t n) = 0;
    // the frame's top level (`slot` the frame in flight) from the partitions written since that slot's last commit: `mode` 0 every
    // record rewritten and rebuilt, 1 the changed partitions rewritten and rebuilt, 2 the changed partitions rewritten and updated
    virtual bool commit(VkCommandBuffer cb, uint32_t slot, int mode, CommitReport& r) = 0;
    virtual VkAccelerationStructureKHR tlas(uint32_t slot) const = 0;
    virtual VkDeviceSize tlas_bytes() const = 0;
    virtual void release() = 0;
};

// ---- the KHR implementation -----------------------------------------------------------------------------------------------------

class KhrAccel final : public RtAccel {
public:
    ~KhrAccel() override { release(); }
    const char* name() const override { return "KHR (clusters as small BLASes, partitions as ranges of the TLAS's instances)"; }

    bool build_clusters(const std::vector<RtCluster>& cl, ClusterReport& r) override {
        const double w0 = now_ms();
        VkDevice dev = vk::device();
        const size_t n = cl.size();
        blas_.assign(n, Blas{});
        // the inputs: every cluster's positions and indices in two buffers, read by their addresses
        std::vector<VkDeviceSize> voff(n), ioff(n);
        VkDeviceSize vbytes = 0, ibytes = 0;
        for (size_t i = 0; i < n; ++i) {
            voff[i] = vbytes;
            ioff[i] = ibytes;
            vbytes += cl[i].pos.size() * sizeof(float);
            ibytes += cl[i].idx.size() * sizeof(uint32_t);
        }
        if (vbytes == 0 || ibytes == 0) return false;
        r.input_bytes = vbytes + ibytes;
        const VkBufferUsageFlags in_use = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        const VkMemoryPropertyFlags hv = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        vk::GpuBuffer vb{}, ib{};
        if (!vk::create_buffer(vbytes, in_use, hv, vb, true) || !vk::create_buffer(ibytes, in_use, hv, ib, true)) {
            vk::destroy_buffer_now(vb);
            vk::destroy_buffer_now(ib);
            return false;
        }
        for (size_t i = 0; i < n; ++i) {
            std::memcpy(static_cast<uint8_t*>(vb.mapped) + voff[i], cl[i].pos.data(), cl[i].pos.size() * sizeof(float));
            std::memcpy(static_cast<uint8_t*>(ib.mapped) + ioff[i], cl[i].idx.data(), cl[i].idx.size() * sizeof(uint32_t));
        }
        const VkDeviceAddress va = vk::buffer_address(vb.buffer), ia = vk::buffer_address(ib.buffer);
        // per cluster its geometry and sizes
        std::vector<VkAccelerationStructureGeometryKHR> geo(n);
        std::vector<VkAccelerationStructureBuildGeometryInfoKHR> info(n);
        std::vector<VkAccelerationStructureBuildRangeInfoKHR> range(n);
        std::vector<VkAccelerationStructureBuildSizesInfoKHR> sizes(n);
        std::vector<VkDeviceSize> soff(n);
        VkDeviceSize sbytes = 0;
        for (size_t i = 0; i < n; ++i) {
            const uint32_t ntri = static_cast<uint32_t>(cl[i].idx.size() / 3), nv = static_cast<uint32_t>(cl[i].pos.size() / 3);
            if (ntri == 0 || nv == 0) {
                ++r.empty;
                continue;
            }
            VkAccelerationStructureGeometryKHR& g = geo[i];
            g.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            g.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            g.flags = cl[i].opaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0;  // (alpha-tested textures: its candidates looked at, sm_water_rt)
            auto& t = g.geometry.triangles;
            t.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            t.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
            t.vertexData.deviceAddress = va + voff[i];
            t.vertexStride = 12;
            t.maxVertex = nv - 1;
            t.indexType = VK_INDEX_TYPE_UINT32;
            t.indexData.deviceAddress = ia + ioff[i];
            VkAccelerationStructureBuildGeometryInfoKHR& b = info[i];
            b.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            b.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            b.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
            b.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            b.geometryCount = 1;
            b.pGeometries = &g;
            sizes[i].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
            vkGetAccelerationStructureBuildSizesKHR(dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &b, &ntri, &sizes[i]);
            range[i].primitiveCount = ntri;
            blas_[i].offset = r.before;
            blas_[i].size = sizes[i].accelerationStructureSize;
            r.before += align_up(sizes[i].accelerationStructureSize, 256);
            r.triangles += ntri;
            r.vertices += nv;
            ++r.built;
        }
        if (r.built == 0) {
            vk::destroy_buffer_now(vb);
            vk::destroy_buffer_now(ib);
            return false;
        }
        // the structures before compaction in one buffer; the builds in batches sharing a scratch buffer
        const VkBufferUsageFlags as_use = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        const VkBufferUsageFlags sc_use = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        vk::GpuBuffer big{}, scratch{};
        if (!vk::create_buffer(r.before, as_use, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, big)) {
            vk::destroy_buffer_now(vb);
            vk::destroy_buffer_now(ib);
            return false;
        }
        const VkDeviceSize salign = vk::as_scratch_alignment();
        constexpr VkDeviceSize kScratchBatch = 64ull << 20;
        std::vector<std::pair<size_t, size_t>> batches;  // [from, to) of the clusters
        {
            size_t from = 0;
            VkDeviceSize used = 0;
            for (size_t i = 0; i < n; ++i) {
                if (blas_[i].size == 0) continue;
                const VkDeviceSize need = align_up(sizes[i].buildScratchSize, salign);
                if (used + need > kScratchBatch && used > 0) {
                    batches.push_back({from, i});
                    from = i;
                    used = 0;
                }
                soff[i] = used;
                used += need;
                sbytes = std::max(sbytes, used);
            }
            batches.push_back({from, n});
        }
        r.scratch_peak = sbytes;
        if (!vk::create_buffer(std::max<VkDeviceSize>(sbytes, 256), sc_use, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratch)) {
            vk::destroy_buffer_now(big);
            vk::destroy_buffer_now(vb);
            vk::destroy_buffer_now(ib);
            return false;
        }
        const VkDeviceAddress sa = align_up(vk::buffer_address(scratch.buffer), salign);
        // (the scratch's start aligned: the buffer is made a little larger when its address is not)
        if (sa != vk::buffer_address(scratch.buffer)) {
            vk::destroy_buffer_now(scratch);
            vk::create_buffer(sbytes + salign, sc_use, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratch);
        }
        const VkDeviceAddress sbase = align_up(vk::buffer_address(scratch.buffer), salign);
        std::vector<VkAccelerationStructureKHR> tmp(n, VK_NULL_HANDLE);
        std::vector<VkAccelerationStructureKHR> listed;
        for (size_t i = 0; i < n; ++i) {
            if (blas_[i].size == 0) continue;
            VkAccelerationStructureCreateInfoKHR ci{};
            ci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            ci.buffer = big.buffer;
            ci.offset = blas_[i].offset;
            ci.size = blas_[i].size;
            ci.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            vkCreateAccelerationStructureKHR(dev, &ci, nullptr, &tmp[i]);
            info[i].dstAccelerationStructure = tmp[i];
            info[i].scratchData.deviceAddress = sbase + soff[i];
            listed.push_back(tmp[i]);
        }
        VkQueryPoolCreateInfo qci{};
        qci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        qci.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
        qci.queryCount = static_cast<uint32_t>(listed.size());
        VkQueryPool qp = VK_NULL_HANDLE;
        vkCreateQueryPool(dev, &qci, nullptr, &qp);
        r.build_gpu = vk::timed_submit([&](VkCommandBuffer cb) {
            vkCmdResetQueryPool(cb, qp, 0, qci.queryCount);
            for (const auto& [from, to] : batches) {
                std::vector<VkAccelerationStructureBuildGeometryInfoKHR> bi;
                std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> br;
                for (size_t i = from; i < to; ++i) {
                    if (blas_[i].size == 0) continue;
                    bi.push_back(info[i]);
                    br.push_back(&range[i]);
                }
                if (!bi.empty()) vkCmdBuildAccelerationStructuresKHR(cb, static_cast<uint32_t>(bi.size()), bi.data(), br.data());
                as_barrier(cb);  // (the scratch reused by the next batch; the sizes read after)
            }
            vkCmdWriteAccelerationStructuresPropertiesKHR(cb, static_cast<uint32_t>(listed.size()), listed.data(),
                                                          VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, qp, 0);
        });
        std::vector<VkDeviceSize> csize(listed.size(), 0);
        vkGetQueryPoolResults(dev, qp, 0, qci.queryCount, csize.size() * sizeof(VkDeviceSize), csize.data(), sizeof(VkDeviceSize),
                              VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        vkDestroyQueryPool(dev, qp, nullptr);
        // compacted: their own buffer, copied into it
        {
            size_t k = 0;
            for (size_t i = 0; i < n; ++i) {
                if (blas_[i].size == 0) continue;
                blas_[i].size = csize[k] > 0 ? csize[k] : blas_[i].size;
                blas_[i].offset = r.after;
                r.after += align_up(blas_[i].size, 256);
                ++k;
            }
        }
        if (!vk::create_buffer(std::max<VkDeviceSize>(r.after, 256), as_use, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, storage_)) {
            for (VkAccelerationStructureKHR a : listed) vkDestroyAccelerationStructureKHR(dev, a, nullptr);
            vk::destroy_buffer_now(big);
            vk::destroy_buffer_now(scratch);
            vk::destroy_buffer_now(vb);
            vk::destroy_buffer_now(ib);
            return false;
        }
        for (size_t i = 0; i < n; ++i) {
            if (blas_[i].size == 0) continue;
            VkAccelerationStructureCreateInfoKHR ci{};
            ci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            ci.buffer = storage_.buffer;
            ci.offset = blas_[i].offset;
            ci.size = blas_[i].size;
            ci.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            vkCreateAccelerationStructureKHR(dev, &ci, nullptr, &blas_[i].as);
        }
        r.compact_gpu = vk::timed_submit([&](VkCommandBuffer cb) {
            for (size_t i = 0; i < n; ++i) {
                if (blas_[i].as == VK_NULL_HANDLE) continue;
                VkCopyAccelerationStructureInfoKHR cp{};
                cp.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR;
                cp.src = tmp[i];
                cp.dst = blas_[i].as;
                cp.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
                vkCmdCopyAccelerationStructureKHR(cb, &cp);
            }
            as_barrier(cb);
        });
        for (VkAccelerationStructureKHR a : listed) vkDestroyAccelerationStructureKHR(dev, a, nullptr);
        vk::destroy_buffer_now(big);
        vk::destroy_buffer_now(scratch);
        vk::destroy_buffer_now(vb);
        vk::destroy_buffer_now(ib);
        for (Blas& b : blas_) {
            if (b.as == VK_NULL_HANDLE) continue;
            VkAccelerationStructureDeviceAddressInfoKHR ai{};
            ai.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
            ai.accelerationStructure = b.as;
            b.addr = vkGetAccelerationStructureDeviceAddressKHR(dev, &ai);
        }
        r.wall = now_ms() - w0;
        return true;
    }

    bool define_partitions(const std::vector<uint32_t>& first, const std::vector<uint32_t>& count, uint32_t dyn_cap) override {
        first_ = first;
        count_ = count;
        gen_.assign(first.size(), 1);
        dyn_cap_ = dyn_cap;
        uint32_t total = 0;
        for (size_t p = 0; p + 1 < first.size(); ++p) total = std::max(total, first[p] + count[p]);
        first_.back() = total;  // (the dynamic partition last)
        max_ = total + dyn_cap;
        recs_.assign(max_, VkAccelerationStructureInstanceKHR{});
        VkDevice dev = vk::device();
        // per frame in flight: the instances (host-visible), the top level, its scratch (a build's or an update's)
        VkAccelerationStructureGeometryKHR g{};
        g.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        g.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        g.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        VkAccelerationStructureBuildGeometryInfoKHR b{};
        b.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        b.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        b.flags = kTlasFlags;
        b.geometryCount = 1;
        b.pGeometries = &g;
        VkAccelerationStructureBuildSizesInfoKHR sz{};
        sz.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &b, &max_, &sz);
        tlas_size_ = sz.accelerationStructureSize;
        const VkDeviceSize salign = vk::as_scratch_alignment();
        const VkDeviceSize sbytes = std::max(sz.buildScratchSize, sz.updateScratchSize) + salign;
        for (Slot& s : slots_) {
            if (!vk::create_buffer(std::max<VkDeviceSize>(static_cast<VkDeviceSize>(max_) * sizeof(VkAccelerationStructureInstanceKHR), 64),
                                   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, s.inst, true))
                return false;
            if (!vk::create_buffer(tlas_size_, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, s.buf))
                return false;
            if (!vk::create_buffer(sbytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   s.scratch))
                return false;
            VkAccelerationStructureCreateInfoKHR ci{};
            ci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            ci.buffer = s.buf.buffer;
            ci.size = tlas_size_;
            ci.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            if (vkCreateAccelerationStructureKHR(dev, &ci, nullptr, &s.tlas) != VK_SUCCESS) return false;
            s.written.assign(first_.size(), 0);
            s.built_count = ~0u;
        }
        return true;
    }

    void partition_write(uint32_t p, const RtInstance* inst, uint32_t n, const uint8_t* visible) override {
        if (p >= first_.size()) return;
        if (p + 1 == first_.size()) {  // (the dynamic one: its room; empty and empty before: nothing changed)
            n = std::min(n, dyn_cap_);
            if (n == 0 && count_[p] == 0 && gen_[p] > 1) return;
        } else {
            n = std::min(n, count_[p]);
        }
        count_[p] = n;
        for (uint32_t k = 0; k < n; ++k) {
            const RtInstance& i = inst[k];
            VkAccelerationStructureInstanceKHR& o = recs_[first_[p] + k];
            std::memcpy(&o.transform, i.xform, sizeof i.xform);
            o.instanceCustomIndex = (first_[p] + k) & 0xffffffu;  // (its record in the instances' shading table, RtInsts)
            o.mask = visible[k] != 0 && i.cluster < blas_.size() && blas_[i.cluster].addr != 0 ? 0xffu : 0u;
            o.instanceShaderBindingTableRecordOffset = 0;
            o.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;  // (opaque: its cluster's geometry says)
            o.accelerationStructureReference = i.cluster < blas_.size() ? blas_[i.cluster].addr : 0;
        }
        ++gen_[p];
    }

    void dynamic_write(const VkAccelerationStructureInstanceKHR* recs, uint32_t n) override {
        if (first_.empty()) return;
        const uint32_t p = static_cast<uint32_t>(first_.size() - 1);
        n = std::min(n, dyn_cap_);
        if (n == 0 && count_[p] == 0 && gen_[p] > 1) return;
        count_[p] = n;
        if (n > 0) std::memcpy(recs_.data() + first_[p], recs, n * sizeof(VkAccelerationStructureInstanceKHR));
        ++gen_[p];
    }

    bool commit(VkCommandBuffer cb, uint32_t slot, int mode, CommitReport& r) override {
        Slot& s = slots_[slot % vk::kFramesInFlight];
        if (s.tlas == VK_NULL_HANDLE) return false;
        const uint32_t dyn = static_cast<uint32_t>(first_.size() - 1);
        const uint32_t count = first_[dyn] + count_[dyn];
        auto* dst = static_cast<VkAccelerationStructureInstanceKHR*>(s.inst.mapped);
        for (size_t p = 0; p < first_.size(); ++p) {
            if (mode != 0 && s.written[p] == gen_[p]) continue;
            if (count_[p] > 0) std::memcpy(dst + first_[p], recs_.data() + first_[p], count_[p] * sizeof(VkAccelerationStructureInstanceKHR));
            s.written[p] = gen_[p];
            ++r.partitions;
            r.records += count_[p];
        }
        if (mode != 0 && r.partitions == 0 && s.built_count == count) {  // (nothing changed since this slot's top level was built: kept)
            r.kept = true;
            return true;
        }
        r.update = mode == 2 && s.built_count == count;
        VkAccelerationStructureGeometryKHR g{};
        g.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        g.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        g.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        g.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        g.geometry.instances.data.deviceAddress = vk::buffer_address(s.inst.buffer);
        VkAccelerationStructureBuildGeometryInfoKHR b{};
        b.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        b.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        b.flags = kTlasFlags;
        b.mode = r.update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        b.srcAccelerationStructure = r.update ? s.tlas : VK_NULL_HANDLE;
        b.dstAccelerationStructure = s.tlas;
        b.geometryCount = 1;
        b.pGeometries = &g;
        b.scratchData.deviceAddress = align_up(vk::buffer_address(s.scratch.buffer), vk::as_scratch_alignment());
        VkAccelerationStructureBuildRangeInfoKHR rg{};
        rg.primitiveCount = count;
        const VkAccelerationStructureBuildRangeInfoKHR* prg = &rg;
        // (the slot's last reads - two frames ago - are done: begin_frame waited for its fence)
        vkCmdBuildAccelerationStructuresKHR(cb, 1, &b, &prg);
        VkMemoryBarrier2 mb{};
        mb.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        mb.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        mb.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        mb.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        mb.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        VkDependencyInfo di{};
        di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        di.memoryBarrierCount = 1;
        di.pMemoryBarriers = &mb;
        vkCmdPipelineBarrier2(cb, &di);
        s.built_count = count;
        return true;
    }

    VkAccelerationStructureKHR tlas(uint32_t slot) const override { return slots_[slot % vk::kFramesInFlight].tlas; }
    VkDeviceSize tlas_bytes() const override { return tlas_size_; }

    void release() override {
        VkDevice dev = vk::device();
        if (dev == VK_NULL_HANDLE) return;
        for (Blas& b : blas_)
            if (b.as != VK_NULL_HANDLE) vkDestroyAccelerationStructureKHR(dev, b.as, nullptr);
        blas_.clear();
        vk::destroy_buffer_now(storage_);
        for (Slot& s : slots_) {
            if (s.tlas != VK_NULL_HANDLE) vkDestroyAccelerationStructureKHR(dev, s.tlas, nullptr);
            s.tlas = VK_NULL_HANDLE;
            vk::destroy_buffer_now(s.inst);
            vk::destroy_buffer_now(s.buf);
            vk::destroy_buffer_now(s.scratch);
        }
    }

private:
    static constexpr VkBuildAccelerationStructureFlagsKHR kTlasFlags =
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    struct Blas {
        VkAccelerationStructureKHR as = VK_NULL_HANDLE;
        VkDeviceAddress addr = 0;
        VkDeviceSize offset = 0, size = 0;
    };
    struct Slot {
        vk::GpuBuffer inst{}, buf{}, scratch{};
        VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
        std::vector<uint64_t> written;  // (per partition the generation last copied into this slot's instances)
        uint32_t built_count = ~0u;     // (the instances its top level was last built with: an update needs the same)
    };
    static void as_barrier(VkCommandBuffer cb) {
        VkMemoryBarrier2 mb{};
        mb.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        // (the compaction's copies run in the build stage: the copy stage is VK_KHR_ray_tracing_maintenance1's, not asked for)
        mb.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        mb.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        mb.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        mb.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        VkDependencyInfo di{};
        di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        di.memoryBarrierCount = 1;
        di.pMemoryBarriers = &mb;
        vkCmdPipelineBarrier2(cb, &di);
    }
    std::vector<Blas> blas_;
    vk::GpuBuffer storage_{};
    std::vector<uint32_t> first_, count_;
    std::vector<uint64_t> gen_;
    uint32_t dyn_cap_ = 0, max_ = 0;
    std::vector<VkAccelerationStructureInstanceKHR> recs_;
    std::array<Slot, vk::kFramesInFlight> slots_{};
    VkDeviceSize tlas_size_ = 0;
};

// ---- the level's state ------------------------------------------------------------------------------------------------------------

struct State {
    std::unique_ptr<RtAccel> accel;
    RtLevelGeometry geo;                    // (its instances sorted by partition)
    std::vector<uint32_t> first, count;     // per partition (the static ones, then the dynamic one)
    std::vector<uint8_t> visible;           // per instance
    std::array<uint8_t, 0x80> zone{}, zone_used{};
    bool zone_seen = false, zone_changed = false;
    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, vk::kFramesInFlight> sets{}, sets_trace{};  // (the sea's; the half-resolution trace's own pass)
    // the hit's shading (phase 2): the instances' table (first triangle, first colour, kind), the triangles' (RtTri), per frame in flight
    // the lit colours (the resident tie / shrub ones, the tfrags'), the textures' sampler, the half-resolution trace's target, a
    // stand-in for it
    vk::GpuBuffer inst_buf{}, tri_buf{};
    std::array<vk::GpuBuffer, vk::kFramesInFlight> rcol_buf{}, tcol_buf{}, frame_buf{};
    // (the frame's constants, binding 7 RtFrame: the fog's four; FOGCOL's r, g, b, whether on; the ripples' spread of the reflection - radians
    // added to the pixel's cone: RAC1_RT_CONE x 0.001; the upsample's taps' distance in half texels: RAC1_RT_BLUR x 0.1)
    float fog[12] = {};
    std::array<uint64_t, vk::kFramesInFlight> col_gen{};
    std::vector<uint32_t> rcols, tcols;
    uint64_t rcol_changes = ~0ull, tcol_changes = ~0ull, col_generation = 1;
    std::vector<VkImageView> textures;
    VkSampler sampler = VK_NULL_HANDLE;
    vk::GpuImage half{}, half_depth{}, stand_in{};
    uint64_t tri_bytes = 0;
    std::array<uint64_t, vk::kFramesInFlight> built_frame{};
    bool ready = false;
    // phase 3, the mobys (Water Reflections High (RT)): the frame's moby triangles skinned into world space (positions, 36 bytes a
    // triangle: their bottom-level structures' input) and their shading records (RtTri, binding 8); the skinning's compute pipeline;
    // per moby its bottom-level structure (kept while its pose stays, refitted while its triangle count stays, rebuilt every kMobyRebuild
    // refits or when it
    // changes); the mobys' texture arrays bound after the world's (per frame in flight: the views last written)
    vk::GpuBuffer mpos{}, mtri{}, mscratch{};
    uint64_t mtri_gen = 1;
    std::array<uint64_t, vk::kFramesInFlight> mtri_written{};
    std::array<std::vector<VkImageView>, vk::kFramesInFlight> mtex_written{};
    std::vector<VkImageView> mtex;
    VkDescriptorSetLayout mset_layout = VK_NULL_HANDLE;
    VkPipelineLayout mpipe_layout = VK_NULL_HANDLE;
    VkShaderModule mcs = VK_NULL_HANDLE;
    VkPipeline mpipe = VK_NULL_HANDLE;
    bool mtried = false;
    struct MobyBlas {
        VkAccelerationStructureKHR as = VK_NULL_HANDLE;
        vk::GpuBuffer buf{};
        VkDeviceAddress addr = 0;
        uint32_t ntri = 0, age = 0;
        uint64_t seen = 0, pose = 0;
        VkDeviceSize build_scratch = 0, update_scratch = 0;
    };
    std::unordered_map<uint64_t, MobyBlas> mblas;
    std::vector<std::pair<uint64_t, MobyBlas>> mdead;  // (freed at that frame number: the frames in flight that traced them done)
    uint64_t m_frames = 0, m_mobys = 0, m_tris = 0, m_builds = 0, m_refits = 0, m_still = 0, m_drop_tris = 0;
    uint32_t m_max_mobys = 0, m_max_tris = 0;
    int tried_level = -1;  // (the level mrp_rt_level last ran for: made, or nothing to trace)
    // the run's numbers: frames committed, partitions / records written, updates, rebuilds
    uint64_t frames = 0, parts = 0, recs = 0, updates = 0, rebuilds = 0, kept = 0;
} g_rt;
using RtState_MobyBlas = State::MobyBlas;

constexpr uint32_t kRtTextures = 64;  // (the texture arrays the hit can read, uRtTex: the world's, 3 to 9 a level, then the mobys')
const int g_tlas_mode = std::clamp(env_int("RAC1_RT_TLAS", 2), 0, 2);  // (0 whole rebuild, 1 partitioned rebuild, 2 partitioned update)
const bool g_zones = env_int("RAC1_RT_ZONES", 1) != 0;

void report_run();
void mobys_free(bool all);
void shutdown_rt() {
    if (vk::device() == VK_NULL_HANDLE) return;
    report_run();
    g_rt.frames = 0;
    if (g_rt.accel) g_rt.accel->release();
    g_rt.accel.reset();
    vk::destroy_buffer_now(g_rt.inst_buf);
    vk::destroy_buffer_now(g_rt.tri_buf);
    for (uint32_t k = 0; k < vk::kFramesInFlight; ++k) {
        vk::destroy_buffer_now(g_rt.rcol_buf[k]);
        vk::destroy_buffer_now(g_rt.tcol_buf[k]);
        vk::destroy_buffer_now(g_rt.frame_buf[k]);
    }
    vk::destroy_image_now(g_rt.half);
    vk::destroy_image_now(g_rt.half_depth);
    vk::destroy_image_now(g_rt.stand_in);
    mobys_free(true);
    vk::destroy_buffer_now(g_rt.mpos);
    vk::destroy_buffer_now(g_rt.mtri);
    vk::destroy_buffer_now(g_rt.mscratch);
    if (g_rt.mpipe != VK_NULL_HANDLE) vkDestroyPipeline(vk::device(), g_rt.mpipe, nullptr);
    if (g_rt.mcs != VK_NULL_HANDLE) vkDestroyShaderModule(vk::device(), g_rt.mcs, nullptr);
    if (g_rt.mpipe_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(vk::device(), g_rt.mpipe_layout, nullptr);
    if (g_rt.mset_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(vk::device(), g_rt.mset_layout, nullptr);
    g_rt.mpipe = VK_NULL_HANDLE;
    g_rt.mcs = VK_NULL_HANDLE;
    g_rt.mpipe_layout = VK_NULL_HANDLE;
    g_rt.mset_layout = VK_NULL_HANDLE;
    g_rt.mtried = false;
    if (g_rt.sampler != VK_NULL_HANDLE) vkDestroySampler(vk::device(), g_rt.sampler, nullptr);
    g_rt.sampler = VK_NULL_HANDLE;
    if (g_rt.pool != VK_NULL_HANDLE) vkDestroyDescriptorPool(vk::device(), g_rt.pool, nullptr);
    if (g_rt.set_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(vk::device(), g_rt.set_layout, nullptr);
    g_rt.pool = VK_NULL_HANDLE;
    g_rt.set_layout = VK_NULL_HANDLE;
    g_rt.ready = false;
}

bool ensure_sets() {
    if (g_rt.pool != VK_NULL_HANDLE) return true;
    VkDevice dev = vk::device();
    mrp_rt_vk_set_layout();
    constexpr uint32_t n = 2 * vk::kFramesInFlight;
    const VkDescriptorPoolSize ps[3] = {{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, n},
                                        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6 * n},
                                        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (kRtTextures + 1) * n}};
    VkDescriptorPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pci.maxSets = n;
    pci.poolSizeCount = 3;
    pci.pPoolSizes = ps;
    if (vkCreateDescriptorPool(dev, &pci, nullptr, &g_rt.pool) != VK_SUCCESS) return false;
    std::array<VkDescriptorSetLayout, n> ls;
    ls.fill(g_rt.set_layout);
    std::array<VkDescriptorSet, n> all{};
    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = g_rt.pool;
    ai.descriptorSetCount = n;
    ai.pSetLayouts = ls.data();
    if (vkAllocateDescriptorSets(dev, &ai, all.data()) != VK_SUCCESS) return false;
    for (uint32_t k = 0; k < vk::kFramesInFlight; ++k) {
        g_rt.sets[k] = all[k];
        g_rt.sets_trace[k] = all[vk::kFramesInFlight + k];
    }
    VkSamplerCreateInfo sci{};  // (the hit's texel: bilinear, between the levels the nearest, repeated as the GS's REPEAT)
    sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter = sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.maxLod = 16.0f;
    if (vkCreateSampler(dev, &sci, nullptr, &g_rt.sampler) != VK_SUCCESS) return false;
    // (the mobys' triangles: a stand-in until the first frame with mobys in the trace)
    if (g_rt.mtri.buffer == VK_NULL_HANDLE &&
        !vk::create_buffer(256, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, g_rt.mtri))
        return false;
    // (the stand-in for the half image: a texel, never read)
    vk::create_image(1, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT, g_rt.stand_in);
    vk::submit_immediate([](VkCommandBuffer cb) { vk::transition_image(cb, g_rt.stand_in, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); });
    vk::register_shutdown_callback(shutdown_rt);
    return true;
}

// orig: (host) frame slot `s`'s two sets written (`what`: 1 the top level, 2 the shading's tables and textures, 4 the colours, 8 the
// half image, 16 the textures alone - the world's, the mobys' after them -, 32 the mobys' triangles)
void write_slot(uint32_t s, uint32_t what) {
    std::vector<VkWriteDescriptorSet> ws;
    std::vector<VkDescriptorBufferInfo> bi;
    std::vector<VkDescriptorImageInfo> ii;
    bi.reserve(16);
    ii.reserve(2 * (kRtTextures + 1));
    VkAccelerationStructureKHR t = g_rt.accel ? g_rt.accel->tlas(s) : VK_NULL_HANDLE;
    VkWriteDescriptorSetAccelerationStructureKHR wa{};
    wa.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    wa.accelerationStructureCount = 1;
    wa.pAccelerationStructures = &t;
    for (VkDescriptorSet set : {g_rt.sets[s], g_rt.sets_trace[s]}) {
        auto buffer = [&](uint32_t binding, const vk::GpuBuffer& b) {
            bi.push_back({b.buffer, 0, VK_WHOLE_SIZE});
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = set;
            w.dstBinding = binding;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            w.pBufferInfo = &bi.back();
            ws.push_back(w);
        };
        if ((what & 1) != 0 && t != VK_NULL_HANDLE) {
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.pNext = &wa;
            w.dstSet = set;
            w.dstBinding = 0;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            ws.push_back(w);
        }
        if ((what & 2) != 0) {
            buffer(1, g_rt.inst_buf);
            buffer(2, g_rt.tri_buf);
        }
        if ((what & (2 | 32)) != 0) buffer(8, g_rt.mtri);
        if ((what & (2 | 16)) != 0) {
            const size_t first = ii.size();
            const size_t nw = g_rt.textures.size();
            for (uint32_t k = 0; k < kRtTextures; ++k) {
                VkImageView v = k < nw ? g_rt.textures[k] : k - nw < g_rt.mtex.size() ? g_rt.mtex[k - nw] : nw > 0 ? g_rt.textures[0] : g_rt.stand_in.view;
                ii.push_back({g_rt.sampler, v, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            }
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = set;
            w.dstBinding = 5;
            w.descriptorCount = kRtTextures;
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.pImageInfo = &ii[first];
            ws.push_back(w);
        }
        if ((what & 4) != 0) {
            buffer(3, g_rt.rcol_buf[s]);
            buffer(4, g_rt.tcol_buf[s]);
            buffer(7, g_rt.frame_buf[s]);
        }
        if ((what & 8) != 0) {
            const bool real = set == g_rt.sets[s] && g_rt.half.view != VK_NULL_HANDLE;
            ii.push_back({g_rt.sampler, real ? g_rt.half.view : g_rt.stand_in.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = set;
            w.dstBinding = 6;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.pImageInfo = &ii.back();
            ws.push_back(w);
        }
    }
    if (!ws.empty()) vkUpdateDescriptorSets(vk::device(), static_cast<uint32_t>(ws.size()), ws.data(), 0, nullptr);
}

// orig: (host) a host-visible storage buffer holding at least `bytes` (made again when smaller; true when it was)
bool fit_buffer(vk::GpuBuffer& b, VkDeviceSize bytes) {
    bytes = std::max<VkDeviceSize>(bytes, 16);
    if (b.buffer != VK_NULL_HANDLE && b.size >= bytes) return false;
    vk::destroy_buffer(b);
    vk::create_buffer(bytes + bytes / 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, b, true);
    return true;
}

// orig: (host) frame slot `s`'s colours copied from the kept ones when they changed since (its sets written again when a buffer grew)
void copy_colours(uint32_t s) {
    if (g_rt.col_gen[s] == g_rt.col_generation && g_rt.rcol_buf[s].buffer != VK_NULL_HANDLE) return;
    bool grew = fit_buffer(g_rt.rcol_buf[s], g_rt.rcols.size() * 4);
    grew = fit_buffer(g_rt.frame_buf[s], sizeof g_rt.fog) || grew;
    grew = fit_buffer(g_rt.tcol_buf[s], g_rt.tcols.size() * 4) || grew;
    if (g_rt.rcol_buf[s].mapped != nullptr && !g_rt.rcols.empty()) std::memcpy(g_rt.rcol_buf[s].mapped, g_rt.rcols.data(), g_rt.rcols.size() * 4);
    if (g_rt.tcol_buf[s].mapped != nullptr && !g_rt.tcols.empty()) std::memcpy(g_rt.tcol_buf[s].mapped, g_rt.tcols.data(), g_rt.tcols.size() * 4);
    if (grew) write_slot(s, 4);
    g_rt.col_gen[s] = g_rt.col_generation;
}

// orig: (host) partition p's instances' visibility from the zones (cell >= 0x80: always) and written
void write_partition(uint32_t p) {
    const uint32_t f = g_rt.first[p], n = g_rt.count[p];
    for (uint32_t k = f; k < f + n; ++k) {
        const RtInstance& i = g_rt.geo.instances[k];
        g_rt.visible[k] = !g_zones || i.zcell >= 0x80 || !g_rt.zone_seen || (i.zmask & g_rt.zone[i.zcell]) != 0 ? 1 : 0;
    }
    g_rt.accel->partition_write(p, g_rt.geo.instances.data() + f, n, g_rt.visible.data() + f);
}

void report_run() {
    if (g_rt.m_frames > 0) {
        std::printf("[rt] level %d mobys: %llu frames with mobys in the trace, %.1f mobys and %.0f triangles a frame (at most %u and %u), structures built %llu, "
                    "refitted %llu, kept (the pose unchanged) %llu; %llu triangles left out (past %u)\n",
                    g_rt.geo.level, static_cast<unsigned long long>(g_rt.m_frames), static_cast<double>(g_rt.m_mobys) / static_cast<double>(g_rt.m_frames),
                    static_cast<double>(g_rt.m_tris) / static_cast<double>(g_rt.m_frames), g_rt.m_max_mobys, g_rt.m_max_tris,
                    static_cast<unsigned long long>(g_rt.m_builds), static_cast<unsigned long long>(g_rt.m_refits), static_cast<unsigned long long>(g_rt.m_still),
                    static_cast<unsigned long long>(g_rt.m_drop_tris),
                    kMobyMaxTris);
        g_rt.m_frames = g_rt.m_mobys = g_rt.m_tris = g_rt.m_builds = g_rt.m_refits = g_rt.m_still = g_rt.m_drop_tris = 0;
        g_rt.m_max_mobys = g_rt.m_max_tris = 0;
    }
    if (g_rt.frames == 0) return;
    std::printf("[rt] level %d: %llu frames, partitions written %llu (%.2f a frame), instance records %llu (%.0f a frame); the top level kept %llu, "
                "updated %llu, rebuilt %llu\n",
                g_rt.geo.level, static_cast<unsigned long long>(g_rt.frames), static_cast<unsigned long long>(g_rt.parts),
                static_cast<double>(g_rt.parts) / static_cast<double>(g_rt.frames), static_cast<unsigned long long>(g_rt.recs),
                static_cast<double>(g_rt.recs) / static_cast<double>(g_rt.frames), static_cast<unsigned long long>(g_rt.kept),
                static_cast<unsigned long long>(g_rt.updates), static_cast<unsigned long long>(g_rt.rebuilds));
}

// ---- phase 3: the mobys (Water Reflections High (RT)) ------------------------------------------------------------------------------


// orig: (host) the mobys in the trace: Water Reflections High (RT) (g_water_reflections 4); RAC1_RT_MOBYS=1 / 0 overrides it (testing)
bool mobys_on() {
    static const int env = env_int("RAC1_RT_MOBYS", -1);
    return env >= 0 ? env != 0 : g_water_reflections >= 4;
}

// orig: (host) one moby's structure destroyed (its buffer after the frames in flight)
void moby_blas_free(RtState_MobyBlas& b) {
    if (b.as != VK_NULL_HANDLE) vkDestroyAccelerationStructureKHR(vk::device(), b.as, nullptr);
    b.as = VK_NULL_HANDLE;
    vk::destroy_buffer_now(b.buf);
}

// orig: (host) the mobys' structures freed: those dropped whose frames are done (`all`: every one, the device idle)
void mobys_free(bool all) {
    if (vk::device() == VK_NULL_HANDLE) return;
    const uint64_t now = vk::frame_number();
    for (size_t i = 0; i < g_rt.mdead.size();) {
        if (all || g_rt.mdead[i].first <= now) {
            moby_blas_free(g_rt.mdead[i].second);
            g_rt.mdead[i] = g_rt.mdead.back();
            g_rt.mdead.pop_back();
        } else {
            ++i;
        }
    }
    if (all) {
        for (auto& [k, b] : g_rt.mblas) moby_blas_free(b);
        g_rt.mblas.clear();
    }
}

// orig: (host) the skinning's compute pipeline (made once; false when it cannot be)
bool moby_pipeline() {
    if (g_rt.mtried) return g_rt.mpipe != VK_NULL_HANDLE;
    g_rt.mtried = true;
    VkDevice dev = vk::device();
    VkDescriptorSetLayoutBinding b[10]{};
    for (uint32_t i = 0; i < 10; ++i) {
        b[i].binding = i;
        b[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        b[i].descriptorCount = 1;
        b[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo lci{};
    lci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    lci.bindingCount = 10;
    lci.pBindings = b;
    if (vkCreateDescriptorSetLayout(dev, &lci, nullptr, &g_rt.mset_layout) != VK_SUCCESS) return false;
    VkPushConstantRange pcr{VK_SHADER_STAGE_COMPUTE_BIT, 0, 8};
    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &g_rt.mset_layout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(dev, &plci, nullptr, &g_rt.mpipe_layout) != VK_SUCCESS) return false;
    g_rt.mcs = vk::create_shader_module(vk::shaders::kMrpRtMobyCs, sizeof(vk::shaders::kMrpRtMobyCs));
    if (g_rt.mcs == VK_NULL_HANDLE) return false;
    g_rt.mpipe = vk::create_compute_pipeline(g_rt.mpipe_layout, g_rt.mcs);
    if (g_rt.mpipe == VK_NULL_HANDLE) std::printf("[rt] the mobys' skinning pipeline could not be made: no mobys in the trace\n");
    return g_rt.mpipe != VK_NULL_HANDLE;
}

// orig: (host) a device-local buffer holding at least `bytes` (made again, larger, when smaller; the old one freed after the frames in
// flight; true when it was made again)
bool fit_device(vk::GpuBuffer& b, VkDeviceSize bytes, VkBufferUsageFlags use) {
    bytes = std::max<VkDeviceSize>(bytes, 256);
    if (b.buffer != VK_NULL_HANDLE && b.size >= bytes) return false;
    vk::destroy_buffer(b);
    vk::create_buffer(bytes + bytes / 2, use, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, b);
    return true;
}

// orig: (host) the frame's mobys into the dynamic partition (before the top level's commit, outside a rendering pass): their draws
// skinned into world space by the compute pass, one bottom-level structure per moby built or refitted, an instance each; none when
// the mobys are not in the trace (the dynamic partition emptied)
void moby_pass(VkCommandBuffer cb, uint32_t slot) {
    static MrpMobyRtFrame mf;
    static std::vector<uint32_t> runs;
    static std::vector<VkAccelerationStructureInstanceKHR> recs;
    struct Group {
        uint64_t key;
        uint32_t first, ntri;
        uint64_t pose;  // (its parts' poses and draws: the same as the frame before's - its structure kept as it is)
    };
    static std::vector<Group> groups;
    runs.clear();
    recs.clear();
    groups.clear();
    mobys_free(false);
    if (!mobys_on() || !moby_pipeline() || !mrp_moby_rt_frame(mf)) {
        g_rt.accel->dynamic_write(nullptr, 0);
        return;
    }
    // the moby textures after the world's (their views written into this slot's sets when they changed)
    g_rt.mtex.clear();
    for (uint64_t v : mf.buckets) g_rt.mtex.push_back(reinterpret_cast<VkImageView>(v));
    const uint32_t nw = static_cast<uint32_t>(std::min<size_t>(g_rt.textures.size(), kRtTextures));
    // the draws, grouped by moby (a moby's parts follow one another; one seen again later is a moby of its own)
    uint32_t total = 0;
    std::unordered_map<uint64_t, uint32_t> seen;
    for (size_t r = 0; r + 1 <= mf.run_bucket.size(); ++r) {
        const uint32_t first_index = mf.runs[4 * r], count = mf.runs[4 * r + 1], ci = mf.runs[4 * r + 2], part = mf.runs[4 * r + 3];
        const uint32_t ntri = count / 3;
        if (ntri == 0 || part >= mf.part_moby.size()) continue;
        if (total + ntri > kMobyMaxTris) {
            g_rt.m_drop_tris += ntri;
            continue;
        }
        const uint64_t moby = mf.part_moby[part];
        if (groups.empty() || (groups.back().key & 0xffffffffull) != moby) {
            if (groups.size() >= kDynamicCap) {
                g_rt.m_drop_tris += ntri;
                continue;
            }
            const uint64_t k = moby | static_cast<uint64_t>(seen[moby]++) << 32;
            groups.push_back(Group{k, total, 0, 1469598103934665603ull});
        }
        const int32_t bk = mf.run_bucket[r];
        const uint32_t rb = bk >= 0 && nw + static_cast<uint32_t>(bk) < kRtTextures ? nw + static_cast<uint32_t>(bk) : 255u;
        runs.insert(runs.end(), {first_index, ntri, (ci & 0xffffffu) | rb << 24, total});
        groups.back().ntri += ntri;
        {
            uint64_t& h = groups.back().pose;
            const uint64_t v[3] = {part < mf.part_pose.size() ? mf.part_pose[part] : 0ull, first_index, ntri};
            for (uint64_t x : v) h = (h ^ x) * 1099511628211ull;
        }
        total += ntri;
    }
    if (total == 0) {
        g_rt.accel->dynamic_write(nullptr, 0);
        return;
    }
    // the outputs: positions (the structures' input) and the shading records (the trace's binding 8)
    const VkBufferUsageFlags pos_use = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    fit_device(g_rt.mpos, static_cast<VkDeviceSize>(total) * 36, pos_use);
    if (fit_device(g_rt.mtri, static_cast<VkDeviceSize>(total) * 48, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) ++g_rt.mtri_gen;
    if (g_rt.mpos.buffer == VK_NULL_HANDLE || g_rt.mtri.buffer == VK_NULL_HANDLE) {
        g_rt.accel->dynamic_write(nullptr, 0);
        return;
    }
    {
        uint32_t what = 0;
        if (g_rt.mtri_written[slot] != g_rt.mtri_gen) what |= 32;
        if (g_rt.mtex_written[slot] != g_rt.mtex) what |= 16;
        if (what != 0) write_slot(slot, what);
        g_rt.mtri_written[slot] = g_rt.mtri_gen;
        g_rt.mtex_written[slot] = g_rt.mtex;
    }
    // the skinning
    const vk::RingAlloc r_runs = vk::alloc_ring(runs.size() * 4, 16);
    std::memcpy(r_runs.ptr, runs.data(), runs.size() * 4);
    const vk::RingAlloc r_world = vk::alloc_ring(mf.part_world.size() * 4, 16);
    std::memcpy(r_world.ptr, mf.part_world.data(), mf.part_world.size() * 4);
    VkDescriptorSet ds = vk::alloc_descriptor_set(g_rt.mset_layout);
    if (ds == VK_NULL_HANDLE) {
        g_rt.accel->dynamic_write(nullptr, 0);
        return;
    }
    auto bi = [](const MrpMobyRtBuf& b) { return VkDescriptorBufferInfo{reinterpret_cast<VkBuffer>(b.buffer), b.offset, b.size}; };
    const VkDescriptorBufferInfo dbi[10] = {bi(mf.joints), bi(mf.parts), bi(mf.cmds), bi(mf.normals), bi(mf.verts), bi(mf.indices),
                                            {r_runs.buffer, r_runs.offset, runs.size() * 4},
                                            {r_world.buffer, r_world.offset, std::max<VkDeviceSize>(mf.part_world.size() * 4, 16)},
                                            {g_rt.mpos.buffer, 0, VK_WHOLE_SIZE},
                                            {g_rt.mtri.buffer, 0, VK_WHOLE_SIZE}};
    VkWriteDescriptorSet w[10]{};
    for (uint32_t i = 0; i < 10; ++i) {
        w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[i].dstSet = ds;
        w[i].dstBinding = i;
        w[i].descriptorCount = 1;
        w[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w[i].pBufferInfo = &dbi[i];
    }
    vkUpdateDescriptorSets(vk::device(), 10, w, 0, nullptr);
    auto barrier = [cb](VkPipelineStageFlags2 ss, VkAccessFlags2 sa, VkPipelineStageFlags2 ds2, VkAccessFlags2 da) {
        VkMemoryBarrier2 mb{};
        mb.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        mb.srcStageMask = ss;
        mb.srcAccessMask = sa;
        mb.dstStageMask = ds2;
        mb.dstAccessMask = da;
        VkDependencyInfo di{};
        di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        di.memoryBarrierCount = 1;
        di.pMemoryBarriers = &mb;
        vkCmdPipelineBarrier2(cb, &di);
    };
    // (the frame before may still trace the structures and read the records being written again: its reads first)
    barrier(VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_2_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, g_rt.mpipe);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, g_rt.mpipe_layout, 0, 1, &ds, 0, nullptr);
    const uint32_t pc[2] = {total, static_cast<uint32_t>(runs.size() / 4)};
    vkCmdPushConstants(cb, g_rt.mpipe_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof pc, pc);
    vkCmdDispatch(cb, (total + 63) / 64, 1, 1);
    barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_READ_BIT);
    // the structures: per moby built (new, its triangle count changed, or kMobyRebuild frames refitted) or refitted
    VkDevice dev = vk::device();
    const uint64_t now = vk::frame_number();
    const VkDeviceAddress pa = vk::buffer_address(g_rt.mpos.buffer);
    static std::vector<VkAccelerationStructureGeometryKHR> geo;
    static std::vector<VkAccelerationStructureBuildGeometryInfoKHR> info;
    static std::vector<VkAccelerationStructureBuildRangeInfoKHR> range;
    static std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> prange;
    static std::vector<VkDeviceSize> soff;
    geo.assign(groups.size(), VkAccelerationStructureGeometryKHR{});
    info.assign(groups.size(), VkAccelerationStructureBuildGeometryInfoKHR{});
    range.assign(groups.size(), VkAccelerationStructureBuildRangeInfoKHR{});
    soff.assign(groups.size(), 0);
    const VkDeviceSize salign = vk::as_scratch_alignment();
    VkDeviceSize sbytes = 0;
    std::vector<RtState_MobyBlas*> used(groups.size(), nullptr);
    static std::vector<uint8_t> still;
    still.assign(groups.size(), 0);
    for (size_t i = 0; i < groups.size(); ++i) {
        const Group& G = groups[i];
        VkAccelerationStructureGeometryKHR& g = geo[i];
        g.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        g.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        g.flags = 0;  // (not opaque: a candidate's texel looked at - a see-through one, alpha under the world's test, is not a hit)
        auto& t = g.geometry.triangles;
        t.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        t.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        t.vertexData.deviceAddress = pa + static_cast<VkDeviceAddress>(G.first) * 36;
        t.vertexStride = 12;
        t.maxVertex = 3 * G.ntri - 1;
        t.indexType = VK_INDEX_TYPE_NONE_KHR;
        VkAccelerationStructureBuildGeometryInfoKHR& b = info[i];
        b.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        b.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        b.flags = kMobyFlags;
        b.geometryCount = 1;
        b.pGeometries = &g;
        range[i].primitiveCount = G.ntri;
        RtState_MobyBlas& m = g_rt.mblas[G.key];
        const bool refit = m.as != VK_NULL_HANDLE && m.ntri == G.ntri && m.age < kMobyRebuild;
        still[i] = refit && m.pose == G.pose ? 1 : 0;
        m.pose = G.pose;
        if (!refit) {
            VkAccelerationStructureBuildSizesInfoKHR sz{};
            sz.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
            if (m.ntri != G.ntri || m.as == VK_NULL_HANDLE) {
                vkGetAccelerationStructureBuildSizesKHR(dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &b, &G.ntri, &sz);
                if (m.as == VK_NULL_HANDLE || m.buf.size < sz.accelerationStructureSize) {  // (a structure of its own, in its own buffer)
                    if (m.as != VK_NULL_HANDLE) g_rt.mdead.push_back({now + vk::kFramesInFlight + 1, m});
                    m = RtState_MobyBlas{};
                    if (!vk::create_buffer(sz.accelerationStructureSize,
                                           VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m.buf))
                        continue;
                    VkAccelerationStructureCreateInfoKHR ci{};
                    ci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
                    ci.buffer = m.buf.buffer;
                    ci.size = sz.accelerationStructureSize;
                    ci.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                    if (vkCreateAccelerationStructureKHR(dev, &ci, nullptr, &m.as) != VK_SUCCESS) {
                        vk::destroy_buffer_now(m.buf);
                        m = RtState_MobyBlas{};
                        continue;
                    }
                    VkAccelerationStructureDeviceAddressInfoKHR ai{};
                    ai.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
                    ai.accelerationStructure = m.as;
                    m.addr = vkGetAccelerationStructureDeviceAddressKHR(dev, &ai);
                }
                m.build_scratch = sz.buildScratchSize;
                m.update_scratch = sz.updateScratchSize;
                m.ntri = G.ntri;
            }
            m.age = 0;
        } else if (still[i] == 0) {
            ++m.age;
        }
        b.mode = refit ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        b.srcAccelerationStructure = refit ? m.as : VK_NULL_HANDLE;
        b.dstAccelerationStructure = m.as;
        m.seen = now;
        soff[i] = sbytes;
        if (still[i] == 0) sbytes += align_up(refit ? m.update_scratch : m.build_scratch, salign);
        used[i] = &m;
        if (still[i] != 0) ++g_rt.m_still;
        else if (refit) ++g_rt.m_refits;
        else ++g_rt.m_builds;
    }
    fit_device(g_rt.mscratch, sbytes + salign, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    const VkDeviceAddress sbase = align_up(vk::buffer_address(g_rt.mscratch.buffer), salign);
    static std::vector<VkAccelerationStructureBuildGeometryInfoKHR> bi_list;
    bi_list.clear();
    prange.clear();
    for (size_t i = 0; i < groups.size(); ++i) {
        if (used[i] == nullptr) continue;
        info[i].pGeometries = &geo[i];
        info[i].scratchData.deviceAddress = sbase + soff[i];
        if (!still[i]) {  // (a moby whose pose is the frame before's: its structure as it is)
            bi_list.push_back(info[i]);
            prange.push_back(&range[i]);
        }
        VkAccelerationStructureInstanceKHR o{};
        o.transform.matrix[0][0] = o.transform.matrix[1][1] = o.transform.matrix[2][2] = 1.0f;  // (the triangles in world space)
        o.instanceCustomIndex = 0x800000u | (groups[i].first & 0x7fffffu);  // (bit 23: a moby, sm_rt_surface reads rt_mtri)
        o.mask = 0xffu;
        o.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        o.accelerationStructureReference = used[i]->addr;
        recs.push_back(o);
    }
    if (!bi_list.empty()) vkCmdBuildAccelerationStructuresKHR(cb, static_cast<uint32_t>(bi_list.size()), bi_list.data(), prange.data());
    barrier(VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
            VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR);
    g_rt.accel->dynamic_write(recs.data(), static_cast<uint32_t>(recs.size()));
    // the mobys not seen for a few frames: their structures dropped
    for (auto it = g_rt.mblas.begin(); it != g_rt.mblas.end();) {
        if (it->second.seen + 4 < now) {
            g_rt.mdead.push_back({now + vk::kFramesInFlight + 1, it->second});
            it = g_rt.mblas.erase(it);
        } else {
            ++it;
        }
    }
    ++g_rt.m_frames;
    g_rt.m_mobys += recs.size();
    g_rt.m_tris += total;
    g_rt.m_max_mobys = std::max<uint32_t>(g_rt.m_max_mobys, static_cast<uint32_t>(recs.size()));
    g_rt.m_max_tris = std::max(g_rt.m_max_tris, total);
}

}  // namespace

// orig: (host) see mrp_rt.h
bool mrp_rt_supported() { return vk::is_active() && vk::rt_enabled(); }

// orig: (host) see mrp_rt.h
bool mrp_rt_active() {
    static const int env = env_int("RAC1_RT", -1);  // (testing: 1 the trace whatever the row, 0 never; unset: the row)
    if (!mrp_rt_supported()) return false;
    return env >= 0 ? env != 0 : g_water_reflections >= 3 && g_surface_detail > 0;  // (the water's material draws the reflection)
}

// orig: (host) see mrp_rt.h
bool mrp_rt_tracing() { return g_rt.ready && g_rt.accel != nullptr && mrp_rt_active(); }

// orig: (host) see mrp_rt.h
int mrp_rt_level_id() { return g_rt.tried_level; }

namespace {
// A level's (or a world section's) structures changed at the frame's start, not where the world renderer asks (its load, in the
// middle of the frame's recording): the frame being recorded may have bound this slot's set already (the seas drawn before the load,
// a pipelined frame), and the old top level destroyed, the tables' buffers freed and the sets written again under it made that
// command buffer invalid - submitted anyway, the GPU read the freed top level: an MMU fault, VK_ERROR_DEVICE_LOST, the picture frozen
// while the game went on (Daino's Arena, "La grande fuga", NVIDIA with Medium (RT)). Asked: the trace stops at once (no set
// given out, mrp_rt_vk_set), the change itself waits for mrp_rt_frame_begin.
struct RtPending {
    bool any = false, drop = false;
    RtLevelGeometry geo;
};
RtPending g_rt_pending;
void rt_level_drop_now();
void rt_level_now(RtLevelGeometry&& g);
}  // namespace

// orig: (host) see mrp_rt.h
void mrp_rt_level_drop() {
    if (g_rt.tried_level < 0 || vk::device() == VK_NULL_HANDLE) return;
    g_rt.ready = false;
    g_rt.tried_level = -1;
    g_rt_pending.any = true;
    g_rt_pending.drop = true;
    g_rt_pending.geo = RtLevelGeometry{};
}

// orig: (host) see mrp_rt.h
void mrp_rt_level(RtLevelGeometry&& g) {
    if (!mrp_rt_active()) return;
    g_rt.ready = false;
    g_rt.tried_level = g.level;  // (mrp_rt_level_id: asked, not asked again)
    g_rt_pending.any = true;
    g_rt_pending.drop = false;
    g_rt_pending.geo = std::move(g);
}

// orig: (host) see mrp_rt.h
void mrp_rt_frame_begin() {
    if (!g_rt_pending.any) return;
    RtPending p = std::move(g_rt_pending);
    g_rt_pending = RtPending{};
    if (p.drop) {
        g_rt.tried_level = 0;  // (rt_level_drop_now's guard: something to drop)
        rt_level_drop_now();
    } else {
        rt_level_now(std::move(p.geo));
    }
}

namespace {
// orig: (host) mrp_rt_level_drop, done (the frame's start)
void rt_level_drop_now() {
    if (g_rt.tried_level < 0 || vk::device() == VK_NULL_HANDLE) return;
    report_run();
    g_rt.frames = 0;
    vk::wait_idle();
    if (g_rt.accel) g_rt.accel->release();
    g_rt.accel.reset();
    mobys_free(true);
    g_rt.mtex.clear();
    for (auto& w : g_rt.mtex_written) w.clear();
    g_rt.ready = false;
    g_rt.geo = RtLevelGeometry{};
    vk::destroy_buffer_now(g_rt.inst_buf);
    vk::destroy_buffer_now(g_rt.tri_buf);
    g_rt.tried_level = -1;
}
}  // namespace

// orig: (host) see mrp_rt.h
int mrp_rt_cluster_mode() {
    static const int m = std::clamp(env_int("RAC1_RT_CLUSTERS", 1), 0, 1);
    return m;
}

// orig: (host) see mrp_rt.h
uint64_t mrp_rt_vk_set_layout() {
    if (g_rt.set_layout == VK_NULL_HANDLE && mrp_rt_supported()) {
        // (0 the top level; 1 the instances' shading table, 2 the triangles', 3 the resident tie / shrub colours, 4 the tfrags'; 5 the
        // texture arrays - the world's, the mobys' after them; 6 the half-resolution trace; 7 the frame's constants: the fog; 8 the frame's
        // moby triangles' shading, phase 3)
        const VkShaderStageFlags st = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
        const VkDescriptorSetLayoutBinding b[9] = {{0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, st, nullptr},
                                                   {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, st, nullptr},
                                                   {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, st, nullptr},
                                                   {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, st, nullptr},
                                                   {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, st, nullptr},
                                                   {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kRtTextures, st, nullptr},
                                                   {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, st, nullptr},
                                                   {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, st, nullptr},
                                                   {8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, st, nullptr}};
        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = 9;
        ci.pBindings = b;
        vkCreateDescriptorSetLayout(vk::device(), &ci, nullptr, &g_rt.set_layout);
        vk::register_shutdown_callback(shutdown_rt);
    }
    return reinterpret_cast<uint64_t>(g_rt.set_layout);
}

namespace {
// orig: (host) mrp_rt_level, done (the frame's start)
void rt_level_now(RtLevelGeometry&& g) {
    if (!mrp_rt_active()) return;
    g_rt.tried_level = g.level;
    report_run();
    vk::wait_idle();
    if (g_rt.accel) g_rt.accel->release();
    mobys_free(true);
    g_rt.mtex.clear();  // (the last level's moby textures: their views gone)
    for (auto& w : g_rt.mtex_written) w.clear();
    g_rt.accel = std::make_unique<KhrAccel>();
    g_rt.ready = false;
    g_rt.frames = g_rt.parts = g_rt.recs = g_rt.updates = g_rt.rebuilds = g_rt.kept = 0;
    g_rt.built_frame.fill(~0ull);
    g_rt.zone_seen = false;
    g_rt.geo = std::move(g);
    RtLevelGeometry& G = g_rt.geo;
    if (!ensure_sets() || G.instances.empty()) {
        std::printf("[rt] level %d: nothing to trace\n", G.level);
        return;
    }
    // the clusters (built once, compacted)
    ClusterReport cr;
    if (!g_rt.accel->build_clusters(G.clusters, cr)) {
        std::printf("[rt] level %d: the clusters could not be built\n", G.level);
        g_rt.accel.reset();
        return;
    }
    // the partitions: the instances by partition, the dynamic one after (empty)
    std::stable_sort(G.instances.begin(), G.instances.end(), [](const RtInstance& a, const RtInstance& b) { return a.partition < b.partition; });
    const uint32_t np = G.partitions + 1;
    g_rt.first.assign(np, 0);
    g_rt.count.assign(np, 0);
    for (const RtInstance& i : G.instances) ++g_rt.count[std::min<uint32_t>(i.partition, np - 2)];
    for (uint32_t p = 1; p < np; ++p) g_rt.first[p] = g_rt.first[p - 1] + g_rt.count[p - 1];
    g_rt.count[np - 1] = 0;
    g_rt.visible.assign(G.instances.size(), 1);
    if (!g_rt.accel->define_partitions(g_rt.first, g_rt.count, kDynamicCap)) {
        std::printf("[rt] level %d: the top level could not be made\n", G.level);
        g_rt.accel.reset();
        return;
    }
    for (uint32_t p = 0; p + 1 < np; ++p) write_partition(p);
    g_rt.accel->partition_write(np - 1, nullptr, 0, nullptr);
    // the hit's shading: the triangles' table (the clusters' in order), the instances' (by their records: the custom index)
    std::vector<uint32_t> tri_first(G.clusters.size(), 0);
    uint64_t ntri = 0;
    bool tri_ok = true;
    for (size_t c = 0; c < G.clusters.size(); ++c) {
        tri_first[c] = static_cast<uint32_t>(ntri);
        tri_ok = tri_ok && G.clusters[c].tri.size() == G.clusters[c].idx.size() / 3;
        ntri += G.clusters[c].tri.size();
    }
    vk::destroy_buffer_now(g_rt.tri_buf);
    vk::destroy_buffer_now(g_rt.inst_buf);
    fit_buffer(g_rt.tri_buf, ntri * sizeof(RtTri));
    fit_buffer(g_rt.inst_buf, G.instances.size() * 16);
    g_rt.tri_bytes = ntri * sizeof(RtTri);
    for (size_t c = 0; c < G.clusters.size() && g_rt.tri_buf.mapped != nullptr; ++c)
        if (!G.clusters[c].tri.empty())
            std::memcpy(static_cast<uint8_t*>(g_rt.tri_buf.mapped) + static_cast<size_t>(tri_first[c]) * sizeof(RtTri), G.clusters[c].tri.data(),
                        G.clusters[c].tri.size() * sizeof(RtTri));
    if (g_rt.inst_buf.mapped != nullptr) {
        auto* o = static_cast<uint32_t*>(g_rt.inst_buf.mapped);
        for (size_t k = 0; k < G.instances.size(); ++k) {
            const RtInstance& i = G.instances[k];
            const bool ok = i.cluster < G.clusters.size();
            o[4 * k + 0] = ok ? tri_first[i.cluster] : 0u;
            o[4 * k + 1] = i.colour;
            o[4 * k + 2] = ok ? G.clusters[i.cluster].kind : 0u;
            o[4 * k + 3] = 0;
        }
    }
    g_rt.textures.clear();
    for (uint64_t v : G.textures) g_rt.textures.push_back(reinterpret_cast<VkImageView>(v));
    if (g_rt.textures.size() > kRtTextures) std::printf("[rt] level %d: %zu texture arrays, the hit reads the first %u\n", G.level, g_rt.textures.size(), kRtTextures);
    if (!tri_ok) std::printf("[rt] level %d: a cluster without its triangles' shading\n", G.level);
    g_rt.rcols.clear();  // (the new level's colours: from its first frame's, mrp_rt_note_colours; the shader reads none past the buffers)
    g_rt.tcols.clear();
    g_rt.rcol_changes = g_rt.tcol_changes = ~0ull;
    ++g_rt.col_generation;
    g_rt.col_gen.fill(0);
    for (uint32_t s = 0; s < vk::kFramesInFlight; ++s) {
        copy_colours(s);
        write_slot(s, 1 | 2 | 4 | 8);
    }
    // the top level's cost: a whole rebuild (every record written, built) against a partition's change (one zone's records written,
    // the top level rebuilt / updated), timed on the GPU (each in a submit of its own, waited for: three of each, the best kept)
    double t_whole = 1e9, t_part = 1e9, t_upd = 1e9, c_whole = 1e9, c_part = 1e9;
    uint32_t busiest = 0;
    for (uint32_t p = 0; p + 1 < np; ++p)
        if (g_rt.count[p] > g_rt.count[busiest]) busiest = p;
    for (int k = 0; k < 3; ++k) {
        CommitReport r;
        double c0 = now_ms();
        const double tw = vk::timed_submit([&](VkCommandBuffer cb) { g_rt.accel->commit(cb, 0, 0, r); });
        c_whole = std::min(c_whole, now_ms() - c0);
        t_whole = std::min(t_whole, tw);
        write_partition(busiest);
        c0 = now_ms();
        const double tp = vk::timed_submit([&](VkCommandBuffer cb) { g_rt.accel->commit(cb, 0, 1, r); });
        c_part = std::min(c_part, now_ms() - c0);
        t_part = std::min(t_part, tp);
        write_partition(busiest);
        const double tu = vk::timed_submit([&](VkCommandBuffer cb) { g_rt.accel->commit(cb, 0, 2, r); });
        t_upd = std::min(t_upd, tu);
    }
    uint64_t ntri_inst = 0;
    for (const RtInstance& i : G.instances)
        if (i.cluster < G.clusters.size()) ntri_inst += G.clusters[i.cluster].idx.size() / 3;
    uint32_t nk[3] = {}, used_p = 0;
    for (const RtCluster& c : G.clusters) ++nk[std::min<uint8_t>(c.kind, 2)];
    for (uint32_t p = 0; p + 1 < np; ++p) used_p += g_rt.count[p] > 0 ? 1 : 0;
    std::printf("[rt] level %d (%s, back end %s): %s\n", G.level, mrp_rt_cluster_mode() == 0 ? "a cluster per packet / tfrag entry" : "clusters merged per class / zone cell",
                g_rt.accel->name(), G.source.c_str());
    std::printf("[rt] level %d clusters: %u built (%u tie, %u shrub, %u tfrag; %u empty), %llu triangles, %llu vertices, %.1f triangles a cluster; inputs %.1f MB; "
                "BLAS %.1f MB before compaction, %.1f MB after (%.0f bytes a triangle), scratch %.1f MB; GPU build %.2f ms, compaction %.2f ms; %.0f ms in all\n",
                G.level, cr.built, nk[0], nk[1], nk[2], cr.empty, static_cast<unsigned long long>(cr.triangles), static_cast<unsigned long long>(cr.vertices),
                cr.built > 0 ? static_cast<double>(cr.triangles) / cr.built : 0.0, static_cast<double>(cr.input_bytes) / 1048576.0,
                static_cast<double>(cr.before) / 1048576.0, static_cast<double>(cr.after) / 1048576.0,
                cr.triangles > 0 ? static_cast<double>(cr.after) / static_cast<double>(cr.triangles) : 0.0, static_cast<double>(cr.scratch_peak) / 1048576.0,
                cr.build_gpu, cr.compact_gpu, cr.wall);
    std::printf("[rt] level %d top level: %zu instances (%llu triangles instanced), %u partitions used of %u zone cells + unzoned + dynamic, the largest %u instances; "
                "%.2f MB a frame in flight; whole rebuild %.3f ms GPU (%.3f ms CPU with the wait), the largest partition rewritten + rebuild %.3f ms GPU (%.3f ms), "
                "+ update %.3f ms GPU\n",
                G.level, G.instances.size(), static_cast<unsigned long long>(ntri_inst), used_p, np - 2, g_rt.count[busiest],
                static_cast<double>(g_rt.accel->tlas_bytes()) / 1048576.0, t_whole, c_whole, t_part, c_part, t_upd);
    {
        uint32_t nonopq = 0;
        for (const RtCluster& c : G.clusters) nonopq += c.opaque ? 0 : 1;
        std::printf("[rt] level %d shading: %llu triangles' records (%.1f MB), %zu instances' (%.1f MB), %zu texture arrays; %u clusters alpha-tested\n", G.level,
                    static_cast<unsigned long long>(ntri), static_cast<double>(g_rt.tri_bytes) / 1048576.0, G.instances.size(),
                    static_cast<double>(G.instances.size() * 16) / 1048576.0, g_rt.textures.size(), nonopq);
    }
    std::fflush(stdout);
    g_rt.ready = true;
}
}  // namespace

// orig: (host) see mrp_rt.h
void mrp_rt_note_zones(const uint8_t* zone) {
    if (!g_rt.ready || zone == nullptr) return;
    if (!g_rt.zone_seen || std::memcmp(zone, g_rt.zone.data(), 0x80) != 0) {
        std::array<uint8_t, 0x80> before = g_rt.zone;
        const bool first = !g_rt.zone_seen;
        std::memcpy(g_rt.zone.data(), zone, 0x80);
        g_rt.zone_seen = true;
        // the partitions (zone cells) whose byte changed: their instances' visibility written again
        const uint32_t np = static_cast<uint32_t>(g_rt.first.size());
        for (uint32_t p = 0; p + 1 < np; ++p) {
            if (g_rt.count[p] == 0) continue;
            const uint8_t cell = g_rt.geo.instances[g_rt.first[p]].zcell;
            if (cell >= 0x80) continue;
            if (first || before[cell] != zone[cell]) write_partition(p);
        }
    }
}

// orig: (host) see mrp_rt.h
void mrp_rt_frame() {
    if (!g_rt.ready || !g_rt.accel || !mrp_rt_active()) return;  // (the row off an RT value: the structures kept until the next level)
    vk::end_rendering();
    const uint32_t slot = vk::frame_index();
    {  // the dynamic partition: the frame's mobys (High (RT)), else empty
        RAC1_PROF_ZONE(kRtMobys);
        moby_pass(vk::cmd(), slot % vk::kFramesInFlight);
    }
    RAC1_PROF_ZONE(kRtBuild);
    CommitReport r;
    copy_colours(slot % vk::kFramesInFlight);  // (the lit colours, when they changed: this slot's copy)
    if (g_rt.frame_buf[slot % vk::kFramesInFlight].mapped != nullptr) std::memcpy(g_rt.frame_buf[slot % vk::kFramesInFlight].mapped, g_rt.fog, sizeof g_rt.fog);
    if (!g_rt.accel->commit(vk::cmd(), slot, g_tlas_mode, r)) return;
    g_rt.built_frame[slot % vk::kFramesInFlight] = vk::frame_number();
    ++g_rt.frames;
    g_rt.parts += r.partitions;
    g_rt.recs += r.records;
    if (r.kept) ++g_rt.kept;
    else if (r.update) ++g_rt.updates;
    else ++g_rt.rebuilds;
}

// orig: (host) see mrp_rt.h
uint64_t mrp_rt_vk_set() {
    if (!g_rt.ready) return 0;
    const uint32_t slot = vk::frame_index() % vk::kFramesInFlight;
    if (g_rt.built_frame[slot] != vk::frame_number()) return 0;  // (no top level built this frame: the seas drawn without)
    return reinterpret_cast<uint64_t>(g_rt.sets[slot]);
}

// orig: (host) see mrp_rt.h
int mrp_rt_shader_mode(int pass) {
    static const int dbg = std::clamp(env_int("RAC1_RT_DEBUG", 0), 0, 5);
    static const int shade = std::clamp(env_int("RAC1_RT_SHADE", 2), 0, 2);
    return 1 | dbg << 1 | shade << 4 | std::clamp(pass, 0, 2) << 6;
}

// orig: (host) see mrp_rt.h
int mrp_rt_res() {
    static const int r = std::clamp(env_int("RAC1_RT_RES", 0), 0, 2);
    return r;
}

// orig: (host) see mrp_rt.h
uint32_t mrp_rt_shading_rate() {
    static const uint32_t r = mrp_rt_res() == 2 && vk::has_shading_rate() ? 2u : 1u;
    return r;
}

// orig: (host) see mrp_rt.h
void mrp_rt_note_colours(const std::vector<uint32_t>& resident, uint64_t resident_changes, const std::vector<uint32_t>& tfrag, uint64_t tfrag_changes) {
    if (!g_rt.ready) return;
    if (resident_changes == g_rt.rcol_changes && tfrag_changes == g_rt.tcol_changes && resident.size() == g_rt.rcols.size() && tfrag.size() == g_rt.tcols.size())
        return;
    g_rt.rcol_changes = resident_changes;
    g_rt.tcol_changes = tfrag_changes;
    g_rt.rcols = resident;
    g_rt.tcols = tfrag;
    ++g_rt.col_generation;
}

// orig: (host) see mrp_rt.h
void mrp_rt_note_fog(const float fog[4], uint32_t fogcol, bool on) {
    for (int k = 0; k < 4; ++k) g_rt.fog[k] = fog[k];
    for (int k = 0; k < 3; ++k) g_rt.fog[4 + k] = static_cast<float>((fogcol >> (8 * k)) & 0xff) / 255.0f;
    g_rt.fog[7] = on ? 1.0f : 0.0f;
    static const float spread = static_cast<float>(env_int("RAC1_RT_CONE", 20)) * 0.001f;
    g_rt.fog[8] = spread;
    static const float blur = static_cast<float>(env_int("RAC1_RT_BLUR", 10)) * 0.1f;  // (the half-resolution trace's taps, half texels apart)
    g_rt.fog[9] = blur;
}

// orig: (host) see mrp_rt.h
vk::GpuImage* mrp_rt_half_target(int w, int h, vk::GpuImage** depth) {
    if (!g_rt.ready || w <= 0 || h <= 0) return nullptr;
    const uint32_t hw = static_cast<uint32_t>(w + 1) / 2, hh = static_cast<uint32_t>(h + 1) / 2;
    if (g_rt.half.image == VK_NULL_HANDLE || g_rt.half.width != hw || g_rt.half.height != hh) {
        if (g_rt.half.image != VK_NULL_HANDLE) vk::wait_idle();
        vk::destroy_image_now(g_rt.half);
        vk::destroy_image_now(g_rt.half_depth);
        vk::create_image(hw, hh, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, g_rt.half);
        vk::create_image(hw, hh, vk::depth_stencil_format(), VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                         g_rt.half_depth);
        if (g_rt.half.image == VK_NULL_HANDLE || g_rt.half_depth.image == VK_NULL_HANDLE) return nullptr;
        vk::end_rendering();
        vk::transition_image(vk::cmd(), g_rt.half, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        for (uint32_t s = 0; s < vk::kFramesInFlight; ++s) write_slot(s, 8);
        std::printf("[rt] the half-resolution trace's target: %ux%u\n", hw, hh);
    }
    *depth = &g_rt.half_depth;
    return &g_rt.half;
}

// orig: (host) see mrp_rt.h
uint64_t mrp_rt_vk_set_trace() {
    if (mrp_rt_vk_set() == 0) return 0;
    return reinterpret_cast<uint64_t>(g_rt.sets_trace[vk::frame_index() % vk::kFramesInFlight]);
}

}  // namespace rac1
