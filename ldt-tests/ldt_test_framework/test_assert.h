#pragma once

#include "test_case.h"

// ============================================================
// 断言宏
// ============================================================

#define EXPECT_TRUE(expr) \
    do { \
        bool _ok = static_cast<bool>(expr); \
        ctx.onAssert(_ok, #expr, __FILE__, __LINE__); \
        if (!_ok) return; \
    } while(0)

#define EXPECT_FALSE(expr) \
    do { \
        bool _ok = !static_cast<bool>(expr); \
        ctx.onAssert(_ok, "!(" #expr ")", __FILE__, __LINE__); \
        if (!_ok) return; \
    } while(0)

#define EXPECT_EQ(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        bool _ok = (_a == _b); \
        ctx.onAssert(_ok, #a " == " #b, __FILE__, __LINE__); \
        if (!_ok) { \
            std::cerr << "    lhs=" << _a << ", rhs=" << _b << "\n"; \
            return; \
        } \
    } while(0)

#define EXPECT_NE(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        bool _ok = (_a != _b); \
        ctx.onAssert(_ok, #a " != " #b, __FILE__, __LINE__); \
        if (!_ok) { \
            std::cerr << "    lhs=" << _a << ", rhs=" << _b << "\n"; \
            return; \
        } \
    } while(0)

#define EXPECT_GT(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        bool _ok = (_a > _b); \
        ctx.onAssert(_ok, #a " > " #b, __FILE__, __LINE__); \
        if (!_ok) { \
            std::cerr << "    lhs=" << _a << ", rhs=" << _b << "\n"; \
            return; \
        } \
    } while(0)

#define EXPECT_LT(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        bool _ok = (_a < _b); \
        ctx.onAssert(_ok, #a " < " #b, __FILE__, __LINE__); \
        if (!_ok) { \
            std::cerr << "    lhs=" << _a << ", rhs=" << _b << "\n"; \
            return; \
        } \
    } while(0)

#define EXPECT_GE(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        bool _ok = (_a >= _b); \
        ctx.onAssert(_ok, #a " >= " #b, __FILE__, __LINE__); \
        if (!_ok) { \
            std::cerr << "    lhs=" << _a << ", rhs=" << _b << "\n"; \
            return; \
        } \
    } while(0)

#define EXPECT_LE(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        bool _ok = (_a <= _b); \
        ctx.onAssert(_ok, #a " <= " #b, __FILE__, __LINE__); \
        if (!_ok) { \
            std::cerr << "    lhs=" << _a << ", rhs=" << _b << "\n"; \
            return; \
        } \
    } while(0)

#define EXPECT_NOT_NULL(ptr) \
    do { \
        bool _ok = (ptr) != nullptr; \
        ctx.onAssert(_ok, #ptr " != nullptr", __FILE__, __LINE__); \
        if (!_ok) return; \
    } while(0)

#define EXPECT_NULL(ptr) \
    do { \
        bool _ok = (ptr) == nullptr; \
        ctx.onAssert(_ok, #ptr " == nullptr", __FILE__, __LINE__); \
        if (!_ok) return; \
    } while(0)

#define EXPECT_FLOAT_EQ(a, b, eps) \
    do { \
        auto _a = (a); auto _b = (b); auto _e = (eps); \
        bool _ok = std::abs(_a - _b) <= _e; \
        ctx.onAssert(_ok, #a " ≈ " #b, __FILE__, __LINE__); \
        if (!_ok) { \
            std::cerr << "    lhs=" << _a << ", rhs=" << _b << ", eps=" << _e << "\n"; \
            return; \
        } \
    } while(0)
