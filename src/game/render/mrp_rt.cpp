// orig: (host) Ray tracing, phases 0 and 2 (see mrp_rt.h): the acceleration structures of the level's static world, the hit's shading tables (Vulkan, RAC1_RT=1).
#include <game/render/mrp_rt.h>

#include <profiler/host_profiler.h>
#include <runtime/vk/vk_context.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

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

constexpr uint32_t kDynamicCap = 1024;  // (the dynamic partition's room: the mobys, phase 3; empty now)

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
        mb.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_COPY_BIT_KHR;
        mb.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        mb.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_COPY_BIT_KHR;
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
    // the run's numbers: frames committed, partitions / records written, updates, rebuilds
    uint64_t frames = 0, parts = 0, recs = 0, updates = 0, rebuilds = 0, kept = 0;
} g_rt;

constexpr uint32_t kRtTextures = 32;  // (the world's texture arrays the hit can read, uRtTex: 3 to 5 a level)
const int g_tlas_mode = std::clamp(env_int("RAC1_RT_TLAS", 2), 0, 2);  // (0 whole rebuild, 1 partitioned rebuild, 2 partitioned update)
const bool g_zones = env_int("RAC1_RT_ZONES", 1) != 0;

void report_run();
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
                                        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5 * n},
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
    // (the stand-in for the half image: a texel, never read)
    vk::create_image(1, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT, g_rt.stand_in);
    vk::submit_immediate([](VkCommandBuffer cb) { vk::transition_image(cb, g_rt.stand_in, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); });
    vk::register_shutdown_callback(shutdown_rt);
    return true;
}

// orig: (host) frame slot `s`'s two sets written (`what`: 1 the top level, 2 the shading's tables and textures, 4 the colours, 8 the
// half image)
void write_slot(uint32_t s, uint32_t what) {
    std::vector<VkWriteDescriptorSet> ws;
    std::vector<VkDescriptorBufferInfo> bi;
    std::vector<VkDescriptorImageInfo> ii;
    bi.reserve(12);
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
            const size_t first = ii.size();
            for (uint32_t k = 0; k < kRtTextures; ++k) {
                VkImageView v = g_rt.textures.empty() ? g_rt.stand_in.view : g_rt.textures[k < g_rt.textures.size() ? k : 0];
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
    if (g_rt.frames == 0) return;
    std::printf("[rt] level %d: %llu frames, partitions written %llu (%.2f a frame), instance records %llu (%.0f a frame); the top level kept %llu, "
                "updated %llu, rebuilt %llu\n",
                g_rt.geo.level, static_cast<unsigned long long>(g_rt.frames), static_cast<unsigned long long>(g_rt.parts),
                static_cast<double>(g_rt.parts) / static_cast<double>(g_rt.frames), static_cast<unsigned long long>(g_rt.recs),
                static_cast<double>(g_rt.recs) / static_cast<double>(g_rt.frames), static_cast<unsigned long long>(g_rt.kept),
                static_cast<unsigned long long>(g_rt.updates), static_cast<unsigned long long>(g_rt.rebuilds));
}

}  // namespace

// orig: (host) see mrp_rt.h
bool mrp_rt_active() { return vk::is_active() && vk::rt_enabled(); }

// orig: (host) see mrp_rt.h
int mrp_rt_cluster_mode() {
    static const int m = std::clamp(env_int("RAC1_RT_CLUSTERS", 1), 0, 1);
    return m;
}

// orig: (host) see mrp_rt.h
uint64_t mrp_rt_vk_set_layout() {
    if (g_rt.set_layout == VK_NULL_HANDLE && mrp_rt_active()) {
        // (0 the top level; 1 the instances' shading table, 2 the triangles', 3 the resident tie / shrub colours, 4 the tfrags'; 5 the
        // world's texture arrays; 6 the half-resolution trace; 7 the frame's constants: the fog)
        const VkShaderStageFlags st = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
        const VkDescriptorSetLayoutBinding b[8] = {{0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, st, nullptr},
                                                   {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, st, nullptr},
                                                   {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, st, nullptr},
                                                   {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, st, nullptr},
                                                   {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, st, nullptr},
                                                   {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kRtTextures, st, nullptr},
                                                   {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, st, nullptr},
                                                   {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, st, nullptr}};
        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = 8;
        ci.pBindings = b;
        vkCreateDescriptorSetLayout(vk::device(), &ci, nullptr, &g_rt.set_layout);
        vk::register_shutdown_callback(shutdown_rt);
    }
    return reinterpret_cast<uint64_t>(g_rt.set_layout);
}

// orig: (host) see mrp_rt.h
void mrp_rt_level(RtLevelGeometry&& g) {
    if (!mrp_rt_active()) return;
    report_run();
    vk::wait_idle();
    if (g_rt.accel) g_rt.accel->release();
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
    if (!g_rt.ready || !g_rt.accel) return;
    RAC1_PROF_ZONE(kRtBuild);
    vk::end_rendering();
    // the dynamic partition: rewritten every frame (empty: the mobys come in phase 3)
    g_rt.accel->partition_write(static_cast<uint32_t>(g_rt.first.size() - 1), nullptr, 0, nullptr);
    CommitReport r;
    const uint32_t slot = vk::frame_index();
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
