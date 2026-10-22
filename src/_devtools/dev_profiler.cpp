// _devtools/dev_profiler.cpp — the debug menu's Profiler tab and overlay (openly invented, ADR 0003; not a port).
// What src/profiler/host_profiler.h measured: the frame time over the last frames, the stages of a frame on the CPU and
// the GPU side by side (a stacked bar, a sortable table, the bottleneck), the threads, the memory and the GPU's memory.

#include "dev_profiler.h"

#include <platform/platform.h>
#include <profiler/host_profiler.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

namespace rac1::devtools {

namespace {
namespace prof = rac1::prof;

int g_window = 300;         // the frames the averages and the graph cover
bool g_overlay = false;
bool g_gl_auto = false;     // the GL objects scanned again every 2 s
double g_gl_scanned = -1e9;
std::vector<prof::GlOwnerRow> g_gl_rows;
std::string g_export_msg;
int g_sort_col = 1;         // the table's order (its column, descending)
bool g_sort_desc = true;

double mb(uint64_t b) { return static_cast<double>(b) / (1024.0 * 1024.0); }

ImU32 with_alpha(ImU32 c, float a) { return (c & 0x00ffffffu) | (static_cast<ImU32>(std::clamp(a, 0.0f, 1.0f) * 255.0f) << 24); }

std::string stamp_name(const char* ext) {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    char b[64] = "rac1_profile";
    if (rac1::platform::local_time(t, &tm)) std::strftime(b, sizeof b, "rac1_profile_%Y%m%d_%H%M%S", &tm);
    return std::string(b) + ext;
}

// The frame times of the last `n` frames: the period (white), the main thread's own work (blue), the GPU (orange);
// the 60 / 30 fps lines.
void frame_graph(int n, float height, bool labels) {
    const int have = prof::history_size();
    n = std::min(n, have);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const float w = std::max(100.0f, ImGui::GetContentRegionAvail().x);
    ImGui::Dummy(ImVec2(w, height));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, ImVec2(p0.x + w, p0.y + height), IM_COL32(0, 0, 0, 120));
    if (n < 2) return;
    std::vector<double> sorted;
    sorted.reserve(static_cast<size_t>(n));
    for (int k = have - n; k < have; ++k) sorted.push_back(prof::history_at(k).period);
    std::sort(sorted.begin(), sorted.end());
    const double top = std::max(20.0, sorted[static_cast<size_t>(n - 1)] * 1.1);
    auto y_of = [&](double ms) { return p0.y + height - static_cast<float>(std::min(ms, top) / top) * height; };
    for (double line : {1000.0 / 60.0, 1000.0 / 30.0}) {
        if (line > top) continue;
        dl->AddLine(ImVec2(p0.x, y_of(line)), ImVec2(p0.x + w, y_of(line)), IM_COL32(255, 255, 255, 50));
        if (labels) {
            char t[32];
            std::snprintf(t, sizeof t, "%.1f ms", line);
            dl->AddText(ImVec2(p0.x + 2, y_of(line) - 13), IM_COL32(255, 255, 255, 90), t);
        }
    }
    const float dx = w / static_cast<float>(n - 1);
    std::vector<ImVec2> pp, pw, pg;
    for (int i = 0; i < n; ++i) {
        const prof::FrameRec& r = prof::history_at(have - n + i);
        const float x = p0.x + dx * static_cast<float>(i);
        double waits = 0;
        for (int s = 0; s < prof::kStageCount; ++s)
            if (prof::stage_info(s).wait) waits += r.cpu[s];
        pp.emplace_back(x, y_of(r.period));
        pw.emplace_back(x, y_of(std::max(0.0, r.period - waits)));
        if (r.gpu_ok) pg.emplace_back(x, y_of(r.gpu_busy));
    }
    dl->AddPolyline(pg.data(), static_cast<int>(pg.size()), IM_COL32(255, 150, 40, 230), 0, 1.5f);
    dl->AddPolyline(pw.data(), static_cast<int>(pw.size()), IM_COL32(80, 170, 255, 230), 0, 1.5f);
    dl->AddPolyline(pp.data(), static_cast<int>(pp.size()), IM_COL32(240, 240, 240, 255), 0, 1.5f);
    if (labels) {
        char t[64];
        std::snprintf(t, sizeof t, "%.0f ms", top);
        dl->AddText(ImVec2(p0.x + w - 50, p0.y + 1), IM_COL32(255, 255, 255, 110), t);
    }
    if (ImGui::IsMouseHoveringRect(p0, ImVec2(p0.x + w, p0.y + height))) {
        const int i = std::clamp(static_cast<int>(std::lround((ImGui::GetIO().MousePos.x - p0.x) / dx)), 0, n - 1);
        const prof::FrameRec& r = prof::history_at(have - n + i);
        double waits = 0;
        for (int s = 0; s < prof::kStageCount; ++s)
            if (prof::stage_info(s).wait) waits += r.cpu[s];
        ImGui::SetTooltip("host frame %llu: %.2f ms (%.0f fps)\nmain thread work %.2f ms, waits %.2f\nGPU %s%.2f ms\ngame frames %d", static_cast<unsigned long long>(r.index), r.period,
                          r.period > 0 ? 1000.0 / r.period : 0.0, r.period - waits, waits, r.gpu_ok ? "" : "(not read back) ", r.gpu_busy, r.steps);
    }
}

// One stacked bar: the stages' times (`v`), scaled to `scale` ms over the width.
void stacked_bar(const char* label, const double* v, double other, double scale, float height) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine(110);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const float w = std::max(100.0f, ImGui::GetContentRegionAvail().x);
    ImGui::Dummy(ImVec2(w, height));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, ImVec2(p0.x + w, p0.y + height), IM_COL32(0, 0, 0, 100));
    if (scale <= 0) return;
    float x = p0.x;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    auto seg = [&](double ms, ImU32 col, const char* name, bool wait) {
        if (ms <= 0) return;
        const float sw = static_cast<float>(ms / scale) * w;
        const ImVec2 a(x, p0.y), b(std::min(x + sw, p0.x + w), p0.y + height);
        dl->AddRectFilled(a, b, wait ? with_alpha(col, 0.45f) : col);
        if (sw > 3) dl->AddLine(ImVec2(b.x, a.y), ImVec2(b.x, b.y), IM_COL32(0, 0, 0, 160));
        if (sw > 60) {
            char t[64];
            std::snprintf(t, sizeof t, "%.1f", ms);
            dl->AddText(ImVec2(a.x + 3, a.y + (height - ImGui::GetTextLineHeight()) * 0.5f), IM_COL32(0, 0, 0, 220), t);
        }
        if (mouse.x >= a.x && mouse.x < b.x && mouse.y >= a.y && mouse.y < b.y) ImGui::SetTooltip("%s: %.3f ms%s", name, ms, wait ? " (waiting)" : "");
        x += sw;
    };
    for (int s = 0; s < prof::kStageCount; ++s) seg(v[s], prof::stage_info(s).color, prof::stage_info(s).name, prof::stage_info(s).wait);
    seg(other, IM_COL32(60, 60, 60, 255), "(no stage)", false);
}

void summary_lines(const prof::Summary& m) {
    ImGui::Text("%.1f fps   frame %.2f ms  (min %.2f  max %.2f  p99 %.2f)   1%% low %.1f fps   %.2f game frames a host frame", m.fps, m.period_avg, m.period_min, m.period_max,
                m.period_p99, m.low1_fps, m.steps);
    ImGui::Text("CPU (main thread): work %.2f ms = game code %.2f + host %.2f; waits %.2f (present %.2f, cap %.2f); its CPU time %.2f ms", m.main_work, m.game_code, m.host_work,
                m.waits, m.cpu[prof::kPresent], m.cpu[prof::kPacing], m.main_cpu);
    if (m.gpu_frames > 0)
        ImGui::Text("GPU: busy %.2f ms a frame (first to last command %.2f ms)", m.gpu_busy, m.gpu_span);
    else
        ImGui::TextDisabled("GPU: no timings read back yet");
    if (m.gpu_kernel >= 0) {
        ImGui::SameLine();
        ImGui::TextDisabled("  the kernel's account: %.2f ms (%.0f %% busy)", m.gpu_kernel, prof::resources().gpu_busy_pct);
    }
    ImVec4 col(0.6f, 0.9f, 0.6f, 1.0f);
    if (m.bound == prof::Bound::Gpu) col = ImVec4(1.0f, 0.6f, 0.25f, 1.0f);
    else if (m.bound == prof::Bound::MainThread) col = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
    else if (m.bound == prof::Bound::SceneWorker || m.bound == prof::Bound::ChainWorker) col = ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
    ImGui::TextColored(col, "%s", m.bound_text.c_str());
}

void stage_table(const prof::Summary& m) {
    const ImGuiTableFlags tf = ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("stages", 6, tf, ImVec2(0, 22.0f * 14))) return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Stage", ImGuiTableColumnFlags_WidthStretch, 3.0f);
    ImGui::TableSetupColumn("CPU ms", ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_DefaultSort, 1.0f);
    ImGui::TableSetupColumn("CPU max", ImGuiTableColumnFlags_PreferSortDescending, 1.0f);
    ImGui::TableSetupColumn("GPU ms", ImGuiTableColumnFlags_PreferSortDescending, 1.0f);
    ImGui::TableSetupColumn("GPU max", ImGuiTableColumnFlags_PreferSortDescending, 1.0f);
    ImGui::TableSetupColumn("% frame", ImGuiTableColumnFlags_PreferSortDescending, 1.0f);
    ImGui::TableHeadersRow();
    if (ImGuiTableSortSpecs* ss = ImGui::TableGetSortSpecs(); ss != nullptr && ss->SpecsCount > 0) {
        g_sort_col = ss->Specs[0].ColumnIndex;
        g_sort_desc = ss->Specs[0].SortDirection == ImGuiSortDirection_Descending;
    }
    std::vector<int> rows;
    int top_cpu = -1, top_gpu = -1;
    for (int s = 0; s < prof::kStageCount; ++s) {
        if (m.cpu_max[s] < 0.001 && m.gpu_max[s] < 0.001) continue;
        rows.push_back(s);
        if (!prof::stage_info(s).wait && (top_cpu < 0 || m.cpu[s] > m.cpu[top_cpu])) top_cpu = s;
        if (prof::stage_info(s).gpu && (top_gpu < 0 || m.gpu[s] > m.gpu[top_gpu])) top_gpu = s;
    }
    auto key = [&m](int s, int col) -> double {
        switch (col) {
            case 1: return m.cpu[s];
            case 2: return m.cpu_max[s];
            case 3: return m.gpu[s];
            case 4: return m.gpu_max[s];
            case 5: return std::max(m.cpu[s], m.gpu[s]);
            default: return -s;
        }
    };
    std::stable_sort(rows.begin(), rows.end(), [&](int a, int b) { return g_sort_desc ? key(a, g_sort_col) > key(b, g_sort_col) : key(a, g_sort_col) < key(b, g_sort_col); });
    for (int s : rows) {
        const prof::StageInfo& si = prof::stage_info(s);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        const ImVec2 c = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(c.x, c.y + 3), ImVec2(c.x + 10, c.y + 13), si.color);
        ImGui::Dummy(ImVec2(14, 0));
        ImGui::SameLine();
        if (si.wait) ImGui::TextDisabled("%s", si.name);
        else ImGui::TextUnformatted(si.name);
        const ImVec4 hot(1.0f, 0.85f, 0.3f, 1.0f);
        ImGui::TableNextColumn();
        if (s == top_cpu) ImGui::TextColored(hot, "%.3f", m.cpu[s]);
        else ImGui::Text("%.3f", m.cpu[s]);
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", m.cpu_max[s]);
        ImGui::TableNextColumn();
        if (!si.gpu) ImGui::TextDisabled("-");
        else if (s == top_gpu) ImGui::TextColored(hot, "%.3f", m.gpu[s]);
        else ImGui::Text("%.3f", m.gpu[s]);
        ImGui::TableNextColumn();
        if (si.gpu) ImGui::Text("%.3f", m.gpu_max[s]);
        else ImGui::TextDisabled("-");
        ImGui::TableNextColumn();
        ImGui::Text("%.1f", m.period_avg > 0 ? 100.0 * std::max(m.cpu[s], m.gpu[s]) / m.period_avg : 0.0);
    }
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Dummy(ImVec2(14, 0));
    ImGui::SameLine();
    ImGui::TextDisabled("(no stage)");
    ImGui::TableNextColumn();
    ImGui::Text("%.3f", m.cpu_other);
    ImGui::TableNextColumn();
    ImGui::TableNextColumn();
    ImGui::Text("%.3f", m.gpu_other);
    ImGui::EndTable();
}

void threads_section(const prof::Summary& m) {
    const prof::Resources& R = prof::resources();
    ImGui::Text("Process: %.0f %% of one core (%d logical CPUs: %.1f %% of the machine)", R.process_cpu_pct, R.cpus, R.process_cpu_pct / std::max(1, R.cpus));
    if (ImGui::BeginTable("workers", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Worker (per frame)");
        ImGui::TableSetupColumn("busy ms");
        ImGui::TableSetupColumn("CPU ms");
        ImGui::TableSetupColumn("% of frame");
        ImGui::TableHeadersRow();
        for (int w = 0; w < prof::kWorkerCount; ++w) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(prof::worker_name(w));
            ImGui::TableNextColumn();
            ImGui::Text("%.3f", m.worker_busy[w]);
            ImGui::TableNextColumn();
            ImGui::Text("%.3f", m.worker_cpu[w]);
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", m.period_avg > 0 ? 100.0 * m.worker_busy[w] / m.period_avg : 0.0);
        }
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("main thread");
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", m.main_work);
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", m.main_cpu);
        ImGui::TableNextColumn();
        ImGui::Text("%.1f", m.period_avg > 0 ? 100.0 * m.main_work / m.period_avg : 0.0);
        ImGui::EndTable();
    }
    ImGui::TextDisabled("Every thread of the process (CPU %% of one core, sampled twice a second):");
    if (ImGui::BeginTable("threads", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY, ImVec2(0, 22.0f * 8))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Thread", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("CPU %", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableHeadersRow();
        for (const prof::ThreadRow& t : R.threads) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (t.ours) ImGui::TextUnformatted(t.name.c_str());
            else ImGui::TextDisabled("%s", t.name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%llu", static_cast<unsigned long long>(t.tid));
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", t.cpu_pct);
        }
        ImGui::EndTable();
    }
}

void memory_section() {
    const prof::Resources& R = prof::resources();
    ImGui::Text("Process: resident %.0f MB (peak %.0f MB), address space %.0f MB", mb(R.rss), mb(R.rss_peak), mb(R.virt));
    ImGui::TextDisabled("The EE's RAM (32 MB) and scratchpad, the IOP's RAM: fixed, inside the resident memory");
    const bool vk = R.api == "Vulkan";
    ImGui::Text("GPU: %s (%s, %s)", R.gl_renderer.c_str(), R.gl_vendor.c_str(), R.api.c_str());
    if (R.vk_budget)
        ImGui::Text("Vulkan (VK_EXT_memory_budget): device-local heaps %.0f MB, this process uses %.0f MB of its %.0f MB budget", mb(R.vk_heap_size), mb(R.vk_heap_usage),
                    mb(R.vk_heap_budget));
    if (R.drm) {
        ImGui::Text("Kernel (%s): this process %.0f MB in VRAM, %.0f MB in GTT (system memory the GPU maps)", R.drm_driver.c_str(), mb(R.drm_vram), mb(R.drm_gtt));
        if (R.dev_vram_total > 0)
            ImGui::Text("Device: VRAM %.0f / %.0f MB used, GTT %.0f / %.0f MB (every process)", mb(R.dev_vram_used), mb(R.dev_vram_total), mb(R.dev_gtt_used), mb(R.dev_gtt_total));
        if (R.gpu_busy_pct >= 0) ImGui::Text("GPU busy with this process: %.0f %% (the kernel's account)", R.gpu_busy_pct);
    }
    if (R.nvx)
        ImGui::Text("GL_NVX_gpu_memory_info: dedicated %.0f MB, available %.0f / %.0f MB, evicted %.0f MB", mb(R.nvx_dedicated), mb(R.nvx_available), mb(R.nvx_total), mb(R.nvx_evicted));
    if (R.ati) ImGui::Text("GL_ATI_meminfo, free: textures %.0f MB, buffers %.0f MB, renderbuffers %.0f MB", mb(R.ati_tex_free), mb(R.ati_vbo_free), mb(R.ati_rb_free));
    if (!R.drm && !R.nvx && !R.ati && !R.vk_budget) ImGui::TextDisabled("(the driver gives no GPU memory figures)");
    ImGui::Separator();
    const double now = ImGui::GetTime();
    if (ImGui::Button(vk ? "Count Vulkan allocations" : "Count GL objects") || (g_gl_auto && now - g_gl_scanned > 2.0)) {
        prof::gl_scan(&g_gl_rows);
        g_gl_scanned = now;
    }
    ImGui::SameLine();
    ImGui::Checkbox("every 2 s", &g_gl_auto);
    ImGui::SameLine();
    ImGui::TextDisabled(vk ? "(the renderer's images and buffers, by use)" : "(each texture's, buffer's and renderbuffer's size, per owner; a few ms)");
    if (g_gl_rows.empty()) return;
    uint64_t tot = 0;
    for (const prof::GlOwnerRow& r : g_gl_rows) tot += r.tex_bytes + r.buf_bytes + r.rb_bytes;
    ImGui::Text("%s: %.1f MB in all (the window's own buffers not counted)", vk ? "Vulkan allocations" : "GL objects", mb(tot));
    if (ImGui::BeginTable("gl", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY, ImVec2(0, 22.0f * 10))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Owner", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("MB", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("textures", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("buffers", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("renderbuffers", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableHeadersRow();
        for (const prof::GlOwnerRow& r : g_gl_rows) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(r.owner.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", mb(r.tex_bytes + r.buf_bytes + r.rb_bytes));
            ImGui::TableNextColumn();
            if (r.textures > 0) ImGui::Text("%d: %.1f", r.textures, mb(r.tex_bytes));
            ImGui::TableNextColumn();
            if (r.buffers > 0) ImGui::Text("%d: %.1f", r.buffers, mb(r.buf_bytes));
            ImGui::TableNextColumn();
            if (r.renderbuffers > 0) ImGui::Text("%d: %.1f", r.renderbuffers, mb(r.rb_bytes));
        }
        ImGui::EndTable();
    }
}
}  // namespace

void tab_profiler() {
    prof::request();  // (recorded while the tab is shown)
    static bool sized = false;  // (the menu's window made room for the tables the first time)
    if (!sized) {
        sized = true;
        const ImVec2 cur = ImGui::GetWindowSize();
        ImGui::SetWindowSize(ImVec2(std::max(cur.x, 1020.0f), std::max(cur.y, 860.0f)));
    }
    if (ImGui::Checkbox("Overlay", &g_overlay)) prof::set_pinned(g_overlay);
    ImGui::SameLine();
    bool fr = prof::frozen();
    if (ImGui::Checkbox("Freeze", &fr)) prof::set_frozen(fr);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    ImGui::SliderInt("frames", &g_window, 30, prof::history_capacity());
    ImGui::SameLine();
    if (ImGui::Button("Export CSV")) {
        const std::string f = stamp_name(".csv");
        g_export_msg = prof::export_csv(f.c_str(), g_window) ? "wrote " + f : "could not write " + f;
    }
    ImGui::SameLine();
    if (prof::trace_active()) {
        ImGui::BeginDisabled();
        ImGui::Button("Tracing...");
        ImGui::EndDisabled();
    } else if (ImGui::Button("Chrome trace (300 frames)")) {
        prof::trace_start(stamp_name(".json").c_str(), 300);
    }
    if (!g_export_msg.empty()) ImGui::TextDisabled("%s", g_export_msg.c_str());
    if (const std::string ts = prof::trace_status(); !ts.empty()) ImGui::TextDisabled("Chrome trace: %s", ts.c_str());

    const prof::Summary m = prof::summarize(g_window);
    if (m.frames == 0) {
        ImGui::TextDisabled("Recording: the numbers come with the next frames.");
        return;
    }
    summary_lines(m);
    frame_graph(g_window, 120.0f, true);
    ImGui::TextColored(ImVec4(0.94f, 0.94f, 0.94f, 1), "frame");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.31f, 0.67f, 1, 1), "main thread work");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1, 0.59f, 0.16f, 1), "GPU busy");
    // the stages side by side, on one scale: the frame's period or the GPU's, the longer
    const double scale = std::max(m.period_avg, m.gpu_span);
    stacked_bar("CPU (main)", m.cpu, m.cpu_other, scale, 20.0f);
    stacked_bar("GPU", m.gpu, m.gpu_other, scale, 20.0f);
    stage_table(m);
    if (ImGui::CollapsingHeader("Threads", ImGuiTreeNodeFlags_DefaultOpen)) threads_section(m);
    if (ImGui::CollapsingHeader("Memory", ImGuiTreeNodeFlags_DefaultOpen)) memory_section();
}

bool profiler_overlay_wanted() {
    static bool env_done = false;  // (testing: RAC1_PROFILER_OVERLAY=1, the overlay on from the start, e.g. in a menu shot)
    if (!env_done) {
        env_done = true;
        if (const char* e = std::getenv("RAC1_PROFILER_OVERLAY"); e != nullptr && std::atoi(e) != 0) {
            g_overlay = true;
            prof::set_pinned(true);
        }
    }
    return g_overlay;
}

void profiler_overlay() {
    if (!profiler_overlay_wanted()) return;
    const prof::Summary m = prof::summarize(120);
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - 8, vp->WorkPos.y + 8), ImGuiCond_Always, ImVec2(1, 0));
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.6f);
    if (ImGui::Begin("profiler overlay", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
        if (m.frames == 0) {
            ImGui::TextDisabled("profiler: recording");
        } else {
            ImGui::Text("%.1f fps  %.2f ms  (max %.1f, 1%% low %.0f fps)", m.fps, m.period_avg, m.period_max, m.low1_fps);
            ImGui::Text("CPU %.2f ms (game %.2f)  GPU %.2f ms  waits %.2f", m.main_work, m.game_code, m.gpu_busy, m.waits);
            const char* b = "";
            switch (m.bound) {
                case prof::Bound::Gpu: b = "GPU-bound"; break;
                case prof::Bound::MainThread: b = "CPU-bound (main thread)"; break;
                case prof::Bound::SceneWorker: b = "scene worker-bound"; break;
                case prof::Bound::ChainWorker: b = "GS chain worker-bound"; break;
                case prof::Bound::Cap: b = "held by the cap / v-sync"; break;
                default: b = "no single bottleneck"; break;
            }
            ImGui::TextColored(ImVec4(1, 0.85f, 0.3f, 1), "%s", b);
            frame_graph(120, 50.0f, false);
            const double scale = std::max(m.period_avg, m.gpu_span);
            stacked_bar("CPU", m.cpu, m.cpu_other, scale, 10.0f);
            stacked_bar("GPU", m.gpu, m.gpu_other, scale, 10.0f);
        }
    }
    ImGui::End();
}

}  // namespace rac1::devtools
