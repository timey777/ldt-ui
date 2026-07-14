#pragma once

#include <chrono>
#include <cstdio>
#include <cstdarg>
#include <string>

// =========================================================================
// PerfTimer — centralized, toggleable, self-describing performance timer
//
// Quick reference:
//
//   PERF_SCOPE("pipeline");                  // RAII: prints "[perf]   pipeline: 17.8 ms"
//   PERF_SCOPE_MSG("resize", "%.0fx%.0f", w, h);  // RAII with extra info
//
//   PerfWatch w("sync");
//   w.split("buildMap");                     // auto-formatted split
//   w.split("updateCtrls", "%d ctls", n);    // split with extra context
//   w.splitCounted("updateCtrls", 5000);     // split + auto avg: =10.30ms (5000 items, 2.06us avg)
//
//   PerfCounter pc;
//   for (...) { PerfCounter::Tick t(pc); doWork(); }
//   pc.report("updateControl");              // [perf] updateControl: 5000 calls, 10.32ms total, 2.06us avg
//
//   ldt::PerfLog::setEnabled(false);         // global off (zero overhead)
//   ldt::PerfLog::setMinMs(1.0);             // only report >1ms
// =========================================================================

namespace ldt {

class PerfLog {
public:
    static bool& enabled() { static bool e = true;  return e; }
    static double& minMs()  { static double m = 0.0; return m; }

    static void setEnabled(bool e) { enabled() = e; }
    static void setMinMs(double ms) { minMs()  = ms; }

    // Thread-local indent depth for PERF_SCOPE nesting
    static int& depth() { thread_local int d = 0; return d; }
};

// =========================================================================
// PerfScope — RAII scoped timer (use via PERF_SCOPE macro)
// =========================================================================
class PerfScope {
public:
    PerfScope(const char* name) : name_(name) {
        if (PerfLog::enabled()) { start_ = Clock::now(); PerfLog::depth()++; }
    }
    ~PerfScope() {
        if (!PerfLog::enabled()) { PerfLog::depth()--; return; }
        double ms = elapsed();
        int d = --PerfLog::depth();
        if (ms < PerfLog::minMs()) return;
        fprintf(stderr, "[perf]%*s%s: %.2f ms\n", d * 2 + 1, "", name_, ms);
    }

private:
    using Clock = std::chrono::steady_clock;
    double elapsed() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            Clock::now() - start_).count() / 1000.0;
    }
    const char* name_;
    Clock::time_point start_{};
};

// =========================================================================
// PerfWatch — manual multi-split timer with composable output
// =========================================================================
class PerfWatch {
public:
    PerfWatch(const char* name) : name_(name) {
        if (PerfLog::enabled()) { start_ = last_ = Clock::now(); }
    }
    ~PerfWatch() {
        if (!PerfLog::enabled()) return;
        double total = elapsedFrom(start_);
        if (total < PerfLog::minMs()) return;
        if (buf_.empty()) {
            fprintf(stderr, "[perf] %s: %.2f ms\n", name_, total);
        } else {
            fprintf(stderr, "[perf] %s: %s | total=%.2f ms\n", name_, buf_.c_str(), total);
        }
    }

    // Record a split point. Output: label=12.34ms
    void split(const char* label) {
        if (!PerfLog::enabled()) return;
        append(label, elapsedFrom(last_));
        last_ = Clock::now();
    }

    // Record a split with printf-style extra context.
    // Example:  w.split("syncCtrls", "%d updated, %d skipped", n, s);
    // Output:   syncCtrls=10.30ms (5002 updated, 0 skipped)
    void split(const char* label, const char* extraFmt, ...) {
        if (!PerfLog::enabled()) { last_ = Clock::now(); return; }
        double ms = elapsedFrom(last_);
        last_ = Clock::now();

        char extra[128];
        va_list args;
        va_start(args, extraFmt);
        vsnprintf(extra, sizeof(extra), extraFmt, args);
        va_end(args);

        append(label, ms, extra);
    }

    // Record a split, auto-appending the count and per-item average.
    // Example:  w.splitCounted("updateCtrls", 5000);
    // Output:   updateCtrls=10.30ms (5000 ctls, 2.06us avg)
    void splitCounted(const char* label, int count, const char* itemName = "items") {
        if (!PerfLog::enabled() || count <= 0) return;
        double ms = elapsedFrom(last_);
        last_ = Clock::now();
        double avgUs = (ms * 1000.0) / static_cast<double>(count);
        char extra[128];
        snprintf(extra, sizeof(extra), "%d %s, %.2fus avg", count, itemName, avgUs);
        append(label, ms, extra);
    }

    // Override the name used in the final output line.
    void setName(const char* n) { if (PerfLog::enabled()) name_ = n; }

private:
    using Clock = std::chrono::steady_clock;
    double elapsedFrom(Clock::time_point t) const {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            Clock::now() - t).count() / 1000.0;
    }

    void append(const char* label, double ms, const char* extra = nullptr) {
        char piece[256];
        if (extra) {
            snprintf(piece, sizeof(piece), "%s=%.2fms (%s)", label, ms, extra);
        } else {
            snprintf(piece, sizeof(piece), "%s=%.2fms", label, ms);
        }
        if (!buf_.empty()) buf_ += " | ";
        buf_ += piece;
    }

    const char* name_;
    Clock::time_point start_{};
    Clock::time_point last_{};
    std::string buf_;
};

} // namespace ldt

// =========================================================================
// PerfCounter — standalone call-count + total-timer
// =========================================================================
namespace ldt {

class PerfCounter {
public:
    using Clock = std::chrono::steady_clock;

    // RAII tick helper — construct before work, auto-stops on destruction.
    class Tick {
    public:
        explicit Tick(PerfCounter& c) : c_(&c) { if (PerfLog::enabled()) t0_ = Clock::now(); }
        ~Tick() {
            if (!PerfLog::enabled() || !c_) return;
            double us = static_cast<double>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    Clock::now() - t0_).count());
            c_->add(us);
        }
        Tick(const Tick&) = delete;
        Tick& operator=(const Tick&) = delete;
    private:
        PerfCounter* c_;
        Clock::time_point t0_{};
    };

    // Report accumulated stats and reset.
    // Output: [perf] name: 5000 calls, 10.32ms total, 2.06us avg
    void report(const char* name, const char* itemName = "calls") {
        if (!PerfLog::enabled() || count_ <= 0) return;
        double totalMs = totalUs_ / 1000.0;
        double avgUs = totalUs_ / static_cast<double>(count_);
        fprintf(stderr, "[perf] %s: %d %s, %.2fms total, %.2fus avg\n",
            name, count_, itemName, totalMs, avgUs);
        reset();
    }

    void reset() { count_ = 0; totalUs_ = 0.0; }
    int count() const { return count_; }

private:
    void add(double us) { count_++; totalUs_ += us; }
    int count_ = 0;
    double totalUs_ = 0.0;
};

} // namespace ldt

// ── Convenience macros ──

// RAII scoped timer.  Output indented by nesting depth.
// Usage:  { PERF_SCOPE("pipeline"); ... }
#define PERF_SCOPE(name) \
    ldt::PerfScope _perf_scope_##__LINE__(name)

// Conditional printf to stderr.  No-op when PerfLog::enabled() == false.
#define PERF_LOG(fmt, ...) \
    do { if (ldt::PerfLog::enabled()) fprintf(stderr, fmt, ##__VA_ARGS__); } while(0)
