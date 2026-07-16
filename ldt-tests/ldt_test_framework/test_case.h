#pragma once

#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include <algorithm>

// ============================================================
// TestCase — 测试用例基类，支持自动注册
// ============================================================

struct TestContext {
    bool failed = false;
    int  numAsserts = 0;
    int  numPassed  = 0;
    int  numFailed  = 0;

    void onAssert(bool ok, const char* expr, const char* file, int line) {
        numAsserts++;
        if (ok) {
            numPassed++;
        } else {
            numFailed++;
            failed = true;
            std::cerr << "  [FAIL] " << file << ":" << line << "  " << expr << "\n";
        }
    }
};

class TestCase {
public:
    virtual ~TestCase() = default;
    virtual const char* name() const = 0;
    virtual void run(TestContext& ctx) = 0;
};

// 全局注册表
inline std::vector<TestCase*>& allTests() {
    static std::vector<TestCase*> registry;
    return registry;
}

inline TestCase* findTest(const std::string& name) {
    for (auto* t : allTests()) {
        if (name == t->name()) return t;
    }
    return nullptr;
}

inline std::vector<TestCase*> filterTests(const std::string& filter) {
    std::vector<TestCase*> result;
    for (auto* t : allTests()) {
        std::string n = t->name();
        if (n.find(filter) != std::string::npos) {
            result.push_back(t);
        }
    }
    return result;
}

// 自动注册辅助：static 变量构造时注册
// 使用 __LINE__ 避免文件内冲突；匿名 namespace 避免跨文件冲突
#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)

#define TEST_CASE(test_name) \
    namespace { \
        class CONCAT(TestCase_, __LINE__) : public TestCase { \
        public: \
            CONCAT(TestCase_, __LINE__)() { allTests().push_back(this); } \
            const char* name() const override { return test_name; } \
            void run(TestContext& ctx) override; \
        }; \
        static CONCAT(TestCase_, __LINE__) CONCAT(s_testCase_, __LINE__); \
    } \
    void CONCAT(TestCase_, __LINE__)::run(TestContext& ctx)
