#include "test_case.h"
#include "golden_runner.h"
#include <iostream>
#include <string>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

static void printUsage(const char* prog) {
    std::cerr << "LDT Test Runner\n";
    std::cerr << "Usage: " << prog << " [--filter=<pattern>] [--list] [--golden-dir=<path>]\n";
    std::cerr << "\n";
    std::cerr << "  --filter=<pattern>   Run only unit tests whose name contains <pattern>\n";
    std::cerr << "  --list               List all available unit tests\n";
    std::cerr << "  --golden             Run golden tests only\n";
    std::cerr << "  --golden-dir=<path>  Golden tests directory (default: auto)\n";
    std::cerr << "  <test_name>          Run a specific unit test by exact name\n";
}

// 找出 golden/ 目录（相对于可执行文件路径或 cwd）
static std::string findGoldenDir() {
    // Try relative to executable
    std::vector<std::string> candidates = {
        "cases/golden",
        "../cases/golden",
        "../../cases/golden"
    };
    for (auto& c : candidates) {
        if (fs::exists(c)) return c;
    }
    return "";
}

int main(int argc, char* argv[]) {
    bool listOnly = false;
    bool runGolden = false;
    std::string filter;
    std::string specificTest;
    std::string goldenDir;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--list") {
            listOnly = true;
        } else if (arg == "--golden") {
            runGolden = true;
        } else if (arg.find("--golden-dir=") == 0) {
            goldenDir = arg.substr(13);
        } else if (arg.find("--filter=") == 0) {
            filter = arg.substr(9);
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else {
            specificTest = arg;
        }
    }

    // ── List mode: just print and exit ──
    if (listOnly) {
        std::cout << "Available unit tests (" << allTests().size() << "):\n";
        for (auto* t : allTests()) {
            std::cout << "  " << t->name() << "\n";
        }
        std::string dir = goldenDir.empty() ? findGoldenDir() : goldenDir;
        if (!dir.empty()) {
            auto goldenResults = GoldenTestRunner::runAll(dir);
            std::cout << "\nAvailable golden tests (" << goldenResults.size() << "):\n";
            for (auto& r : goldenResults) {
                std::cout << "  golden/" << r.name << "\n";
            }
        }
        return 0;
    }

    int totalAsserts = 0;
    int totalPassed = 0;
    int totalFailed = 0;
    auto startTime = std::chrono::steady_clock::now();

    // ============================================================
    // Phase 1: Unit tests (C++ TEST_CASE)
    // ============================================================
    if (!runGolden) {
        std::vector<TestCase*> toRun;
        if (!specificTest.empty()) {
            auto* t = findTest(specificTest);
            if (!t) {
                std::cerr << "Error: test '" << specificTest << "' not found.\n";
                return 1;
            }
            toRun.push_back(t);
        } else if (!filter.empty()) {
            toRun = filterTests(filter);
            if (toRun.empty()) {
                std::cerr << "No unit tests match filter '" << filter << "'.\n";
                return 1;
            }
        } else {
            toRun = allTests();
        }

        for (auto* test : toRun) {
            TestContext ctx;
            std::cout << "\n[" << test->name() << "]\n";
            test->run(ctx);

            totalAsserts += ctx.numAsserts;
            if (ctx.failed) {
                std::cout << "[FAIL] " << test->name()
                          << "  (" << ctx.numPassed << "/" << ctx.numAsserts << " passed)\n";
                totalFailed++;
            } else {
                std::cout << "[PASS] " << test->name()
                          << "  (" << ctx.numAsserts << " asserts)\n";
                totalPassed++;
            }
        }
    }

    // ============================================================
    // Phase 2: Golden tests (.ldt + expected.json)
    // ============================================================
    if (!specificTest.empty() && !runGolden) {
        // If a specific test name was given, skip golden
    } else {
        std::string dir = goldenDir.empty() ? findGoldenDir() : goldenDir;
        if (!dir.empty() && !listOnly) {
            auto goldenResults = GoldenTestRunner::runAll(dir);
            for (auto& r : goldenResults) {
                std::cout << "\n[golden/" << r.name << "]\n";
                if (r.failureMsg.empty()) {
                    std::cout << "[PASS] golden/" << r.name
                              << "  (" << r.numAsserts << " asserts)\n";
                    totalPassed++;
                } else {
                    std::cout << "[FAIL] golden/" << r.name
                              << "  (" << (r.numAsserts - r.numFailed) << "/"
                              << r.numAsserts << " passed)\n";
                    std::cout << r.failureMsg;
                    totalFailed++;
                }
                totalAsserts += r.numAsserts;
            }
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    // ── Summary ──
    if (!listOnly) {
        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "Results: " << (totalPassed + totalFailed) << " tests, "
                  << totalAsserts << " asserts, "
                  << duration << "ms\n";
        std::cout << "  PASS: " << totalPassed << "\n";
        std::cout << "  FAIL: " << totalFailed << "\n";
        std::cout << "========================================\n";
    }

    return totalFailed > 0 ? 1 : 0;
}
