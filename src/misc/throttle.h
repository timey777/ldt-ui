#pragma once

#include <chrono>

namespace ldt {

// =========================================================================
// Throttle — 通用"间隔内最多执行一次"限流门
//
// 用法：
//   ldt::Throttle t;                          // 默认 16ms（约 60fps）
//   if (t.tryAcquire()) { doExpensiveWork(); }
//
// tryAcquire() 只有在上一次成功获取（或构造）之后经过了最小间隔时才返回
// true，并记录当前时刻作为新的参考点。间隔设为 0 表示不限流（每次都通过）。
//
// 仅限单线程使用，内部不做同步。
// =========================================================================
class Throttle {
public:
    using Clock = std::chrono::steady_clock;
    using Duration = Clock::duration;

    // 默认间隔 16ms（约 60fps）；0 表示不限流。
    explicit Throttle(Duration minInterval = std::chrono::milliseconds(16))
        : minInterval_(minInterval) {}

    // 距上次成功获取已超过最小间隔则返回 true，并刷新参考时刻。
    bool tryAcquire() {
        const auto now = Clock::now();
        if (now - lastTime_ < minInterval_) return false;
        lastTime_ = now;
        return true;
    }

    // 把参考时刻重置为当前时刻（下一次 tryAcquire 必须等待完整间隔）。
    void reset() { lastTime_ = Clock::now(); }

    void setMinInterval(Duration interval) { minInterval_ = interval; }
    Duration minInterval() const { return minInterval_; }

private:
    Duration minInterval_;
    Clock::time_point lastTime_{};
};

} // namespace ldt
