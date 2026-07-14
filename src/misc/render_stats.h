#pragma once

namespace ldt {

/**
 * @brief Per-frame render statistics for performance benchmarking.
 *
 * Populated during Scene::render() and DisplayList traversal.
 * Reset each frame by the benchmark harness.
 */
struct RenderStats {
    // ── Node traversal ──
    int totalControls = 0;   // total known controls (set externally)
    int visitedNodes = 0;    // children whose bounds were checked
    int visibleNodes = 0;    // children that passed culling
    int culledNodes = 0;     // visited - visible

    // ── DisplayList commands (recursive, including SubLists) ──
    int rectCommands = 0;
    int textCommands = 0;
    int textLayoutCommands = 0;
    int imageCommands = 0;
    int pushClipCommands = 0;
    int popClipCommands = 0;
    int subLists = 0;
    int totalCommands = 0;   // sum of all draw commands (excluding push/pop pairs)

    // ── Timing ──
    double cpuTimeUs = 0.0;  // microseconds spent in Scene::render()

    void reset() {
        *this = RenderStats{};
    }
};

/**
 * @brief Thread-local pointer to the current frame's RenderStats.
 *
 * Set by the benchmark harness before calling Scene::render().
 * The rendering pipeline (ContainerControl::renderChildren etc.)
 * will increment counters if this pointer is non-null.
 */
extern thread_local RenderStats* g_currentRenderStats;

} // namespace ldt
