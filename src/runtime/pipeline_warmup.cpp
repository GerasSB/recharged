#include "runtime/pipeline_warmup.h"
#include "runtime/pipeline_warmup_loop.h"
#include "runtime/pipeline_catalog_layout.h"

#include "platform/session_log.h"

#include <aurora/aurora.h>
#include <aurora/event.h>
#include <aurora/gfx.h>
#include <aurora/gfx.hpp>
#include <aurora/pipeline_cache.h>
#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <thread>

namespace mscharged::runtime {
namespace {
struct WarmupHost {
    detail::PipelineWarmupEvents PollEvents()
    {
        detail::PipelineWarmupEvents events;
        for (const auto* event = aurora_update(); event->type != AURORA_NONE; ++event) {
            events.closeRequested |= event->type == AURORA_EXIT;
            events.targetsChanged |= event->type == AURORA_WINDOW_RESIZED ||
                                     event->type == AURORA_DISPLAY_SCALE_CHANGED;
        }
        return events;
    }

    void Prepare() { aurora_prepare_pipelines(); }
    AuroraPipelineWarmupStatus Status() { return aurora_pipeline_warmup_status(); }

    void Draw(const AuroraPipelineWarmupStatus& status)
    {
        // Native GX has not been armed. This frame displays host startup UI
        // through the same Aurora surface that the original game will use.
        if (!aurora_begin_frame()) return;
        const auto size = ImGui::GetIO().DisplaySize;
        const float scale = std::clamp(std::min(size.x / 960.0f, size.y / 540.0f), 0.65f, 3.0f);
        auto* draw = ImGui::GetForegroundDrawList();
        auto* font = ImGui::GetFont();
        const auto text = [&](const char* value, float y, float pixels, ImU32 color) {
            const auto width = font->CalcTextSizeA(pixels, 10000.0f, 0.0f, value).x;
            draw->AddText(font, pixels, {std::round((size.x - width) * 0.5f), std::round(y)}, color, value);
        };
        draw->AddRectFilled({0, 0}, size, IM_COL32(15, 19, 27, 255));
        text("Preparing graphics", size.y * 0.5f - 58.0f * scale, 26.0f * scale,
             IM_COL32(245, 247, 250, 255));
        text("This may take longer the first time.", size.y * 0.5f - 18.0f * scale, 16.0f * scale,
             IM_COL32(170, 181, 197, 255));
        const float fraction = status.totalPipelines
            ? std::clamp(float(double(status.readyPipelines) / double(status.totalPipelines)), 0.0f, 1.0f)
            : 0.0f;
        const float width = std::min(size.x * 0.75f, 420.0f * scale);
        const float left = (size.x - width) * 0.5f;
        const float top = size.y * 0.5f + 26.0f * scale;
        draw->AddRectFilled({left, top}, {left + width, top + 6.0f * scale}, IM_COL32(45, 55, 70, 255),
                            3.0f * scale);
        if (fraction > 0.0f)
            draw->AddRectFilled({left, top}, {left + width * fraction, top + 6.0f * scale},
                                IM_COL32(91, 188, 255, 255), 3.0f * scale);
        char progress[32];
        std::snprintf(progress, sizeof progress, "%u%%", std::min(99u, unsigned(fraction * 100.0f)));
        text(progress, top + 20.0f * scale, 16.0f * scale, IM_COL32(170, 181, 197, 255));
        aurora_end_frame();
    }

    void Pause() { std::this_thread::sleep_for(std::chrono::milliseconds(16)); }
};
} // namespace

bool PrepareGraphicsForGame()
{
    platform::SessionLogLine("Startup: preparing graphics");
    const auto started = std::chrono::steady_clock::now();
    // Draw-list-only UI has no settings or cursor/input ownership. Restore the
    // context before the game's later FPS overlay chooses its own fonts.
    auto& io = ImGui::GetIO();
    struct RestoreImGui {
        ImGuiIO& io;
        const char* ini;
        ImGuiConfigFlags flags;
        ~RestoreImGui() { io.IniFilename = ini; io.ConfigFlags = flags; }
    } restore{io, io.IniFilename, io.ConfigFlags};
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    WarmupHost host;
    const auto result = detail::RunPipelineWarmup(host);
    // Retire all startup draws before original GX acquires its FIFO and EFB.
    aurora::gfx::synchronize();
    if (result == detail::PipelineWarmupResult::Cancelled) {
        platform::SessionLogLine("Startup: graphics preparation cancelled");
        return false;
    }
    if (result == detail::PipelineWarmupResult::Failed)
        throw std::runtime_error("Graphics preparation failed; see the session log for the graphics error");
    const auto status = host.Status();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    platform::SessionLogLine("Startup: graphics ready, " + std::to_string(status.readyPipelines) +
        " pipelines from " + std::to_string(status.knownConfigs) + " configurations in " +
        std::to_string(elapsed) + " ms");
    return true;
}

void LogGraphicsPipelineStats()
{
    const auto status = aurora_pipeline_warmup_status();
    platform::SessionLogLine("Graphics pipelines: " + std::to_string(status.knownConfigs) +
        " configurations, " + std::to_string(status.readyPipelines) + " ready, " +
        std::to_string(status.demandHits) + " demand hits, " +
        std::to_string(status.demandMisses) + " demand misses, " +
        std::to_string(status.demandWaitNanoseconds / 1000000) + " ms resolving misses");
}

} // namespace mscharged::runtime
