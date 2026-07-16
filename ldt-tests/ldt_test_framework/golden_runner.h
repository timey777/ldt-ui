#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>

#include "test_env.h"
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

// ============================================================
// GoldenTestRunner — 扫描 golden/ 目录，执行 .ldt + expected.json
// ============================================================
struct GoldenResult {
    std::string name;
    bool passed = false;
    int numAsserts = 0;
    int numFailed = 0;
    std::string failureMsg;
};

class GoldenTestRunner {
public:
    static std::vector<GoldenResult> runAll(const std::string& rootDir) {
        std::vector<GoldenResult> results;
        if (!fs::exists(rootDir)) return results;

        for (auto& entry : fs::recursive_directory_iterator(rootDir)) {
            if (entry.path().filename() == "case.ldt") {
                auto dir = entry.path().parent_path();
                auto expectPath = dir / "expected.json";
                if (fs::exists(expectPath)) {
                    GoldenResult r;
                    r.name = fs::relative(dir, rootDir).generic_string();
                    runSingle(entry.path().string(), expectPath.string(), r);
                    results.push_back(r);
                }
            }
        }
        return results;
    }

private:
    static void runSingle(const std::string& ldtPath,
                          const std::string& jsonPath,
                          GoldenResult& result) {
        try {
            // 加载 .ldt
            std::ifstream ifs(ldtPath);
            if (!ifs) { result.failureMsg = "cannot open " + ldtPath; return; }
            std::stringstream ss; ss << ifs.rdbuf();
            std::string ldtContent = ss.str();

            // 加载 expected.json
            std::ifstream jfs(jsonPath);
            if (!jfs) { result.failureMsg = "cannot open " + jsonPath; return; }
            std::stringstream js; js << jfs.rdbuf();

            // 解析 JSON
            auto expected = json::parse(js.str());
            if (!expected.is_object()) {
                result.failureMsg = "expected.json must be an object";
                return;
            }

            // 运行引擎（完整流程：解析 + 管线 + 控件同步）
            TestEnv env;
            auto* scene = env.loadFull(ldtContent);
            if (!scene) {
                result.failureMsg = "failed to parse/process LDT";
                return;
            }
            auto* root = env.runtime.getResolvedTree()
                          ? env.runtime.getResolvedTree()->getRoot()
                          : nullptr;
            if (!root) {
                result.failureMsg = "no root node in resolved tree";
                return;
            }

            // ---- 阶段1: 检查 ResolvedNode 属性 (selectors) ----
            if (expected.contains("selectors") && expected["selectors"].is_object()) {
                for (auto& [selector, props] : expected["selectors"].items()) {
                    if (!props.is_object()) continue;

                    std::string id = selector;
                    if (!id.empty() && id[0] == '#') id = id.substr(1);

                    auto* node = env.findById(id);
                    if (!node) {
                        result.numAsserts++;
                        result.numFailed++;
                        result.failureMsg += "  selector '" + selector + "' not found\n";
                        continue;
                    }

                    if (props.contains("computedWidth"))
                        checkFloat(result, "computedWidth", node->layout.computedWidth, props["computedWidth"]);
                    if (props.contains("computedHeight"))
                        checkFloat(result, "computedHeight", node->layout.computedHeight, props["computedHeight"]);
                    if (props.contains("x"))
                        checkFloat(result, "x", node->layout.getBorderBox().x, props["x"]);
                    if (props.contains("y"))
                        checkFloat(result, "y", node->layout.getBorderBox().y, props["y"]);
                    if (props.contains("width"))
                        checkFloat(result, "width", node->layout.getBorderBox().width, props["width"]);
                    if (props.contains("height"))
                        checkFloat(result, "height", node->layout.getBorderBox().height, props["height"]);
                    if (props.contains("childCount"))
                        checkInt(result, "childCount", (int)node->getChildren().size(), props["childCount"]);
                    if (props.contains("layout.computedWidth"))
                        checkFloat(result, "layout.computedWidth", node->layout.computedWidth, props["layout.computedWidth"]);
                    if (props.contains("layout.computedHeight"))
                        checkFloat(result, "layout.computedHeight", node->layout.computedHeight, props["layout.computedHeight"]);
                }
            }

            // ---- 阶段2: 检查控件属性 (controls) ----
            if (expected.contains("controls") && expected["controls"].is_object()) {
                auto* mgr = scene->getControlManager();
                if (!mgr) {
                    result.numAsserts++;
                    result.numFailed++;
                    result.failureMsg += "  controls: no ControlManager\n";
                } else {
                    for (auto& [selector, props] : expected["controls"].items()) {
                        if (!props.is_object()) continue;

                        std::string id = selector;
                        if (!id.empty() && id[0] == '#') id = id.substr(1);

                        auto ctrl = mgr->findControlById(id);
                        if (!ctrl) {
                            result.numAsserts++;
                            result.numFailed++;
                            result.failureMsg += "  control '" + selector + "' not found\n";
                            continue;
                        }

                        if (props.contains("scrollHeight"))
                            checkFloat(result, "scrollHeight", ctrl->getScrollHeight(), props["scrollHeight"]);
                        if (props.contains("scrollWidth"))
                            checkFloat(result, "scrollWidth", ctrl->getScrollWidth(), props["scrollWidth"]);
                        if (props.contains("bounds.x"))
                            checkFloat(result, "bounds.x", ctrl->getBounds().x, props["bounds.x"]);
                        if (props.contains("bounds.y"))
                            checkFloat(result, "bounds.y", ctrl->getBounds().y, props["bounds.y"]);
                        if (props.contains("bounds.width"))
                            checkFloat(result, "bounds.width", ctrl->getBounds().width, props["bounds.width"]);
                        if (props.contains("bounds.height"))
                            checkFloat(result, "bounds.height", ctrl->getBounds().height, props["bounds.height"]);
                        if (props.contains("visible"))
                            checkBool(result, "visible", ctrl->isVisible(), props["visible"]);
                    }
                }
            }

            result.passed = (result.numFailed == 0);

        } catch (const std::exception& e) {
            result.failureMsg = std::string("exception: ") + e.what();
        }
    }

    static void checkFloat(GoldenResult& r, const std::string& field,
                           float actual, const json& expected) {
        r.numAsserts++;

        // Comparison operator: { "gt": 25 }
        if (expected.is_object()) {
            if (expected.contains("gt") && expected["gt"].is_number()) {
                float gt = expected["gt"].get<float>();
                if (!(actual > gt)) {
                    r.numFailed++;
                    r.failureMsg += "  " + field + ": expected > " + std::to_string(gt)
                                  + ", got " + std::to_string(actual) + "\n";
                }
                return;
            }
            if (expected.contains("lt") && expected["lt"].is_number()) {
                float lt = expected["lt"].get<float>();
                if (!(actual < lt)) {
                    r.numFailed++;
                    r.failureMsg += "  " + field + ": expected < " + std::to_string(lt)
                                  + ", got " + std::to_string(actual) + "\n";
                }
                return;
            }
            if (expected.contains("eq") && expected["eq"].is_number()) {
                float eq = expected["eq"].get<float>();
                float diff = std::abs(actual - eq);
                if (diff > 0.5f) {
                    r.numFailed++;
                    r.failureMsg += "  " + field + ": expected " + std::to_string(eq)
                                  + ", got " + std::to_string(actual) + "\n";
                }
                return;
            }
            return; // unknown object — skip
        }
        
        // Plain number: allow ±0.5 tolerance
        if (!expected.is_number()) {
            r.numFailed++;
            return;
        }
        float exp = expected.get<float>();
        float diff = std::abs(actual - exp);
        if (diff > 0.5f) {
            r.numFailed++;
            r.failureMsg += "  " + field + ": expected " + std::to_string(exp)
                          + ", got " + std::to_string(actual) + "\n";
        }
    }

    static void checkBool(GoldenResult& r, const std::string& field,
                          bool actual, const json& expected) {
        r.numAsserts++;
        if (expected.is_boolean()) {
            if (actual != expected.get<bool>()) {
                r.numFailed++;
                r.failureMsg += "  " + field + ": expected " + (expected.get<bool>() ? "true" : "false")
                              + ", got " + (actual ? "true" : "false") + "\n";
            }
        } else if (expected.is_number_integer()) {
            bool expBool = (expected.get<int>() != 0);
            if (actual != expBool) {
                r.numFailed++;
                r.failureMsg += "  " + field + ": expected " + (expBool ? "true" : "false")
                              + ", got " + (actual ? "true" : "false") + "\n";
            }
        }
    }

    static void checkInt(GoldenResult& r, const std::string& field,
                         int actual, const json& expected) {
        r.numAsserts++;

        if (expected.is_object()) {
            if (expected.contains("gt") && expected["gt"].is_number_integer()) {
                int gt = expected["gt"].get<int>();
                if (!(actual > gt)) {
                    r.numFailed++;
                    r.failureMsg += "  " + field + ": expected > " + std::to_string(gt)
                                  + ", got " + std::to_string(actual) + "\n";
                }
                return;
            }
            if (expected.contains("lt") && expected["lt"].is_number_integer()) {
                int lt = expected["lt"].get<int>();
                if (!(actual < lt)) {
                    r.numFailed++;
                    r.failureMsg += "  " + field + ": expected < " + std::to_string(lt)
                                  + ", got " + std::to_string(actual) + "\n";
                }
                return;
            }
            if (expected.contains("eq") && expected["eq"].is_number_integer()) {
                int eq = expected["eq"].get<int>();
                if (actual != eq) {
                    r.numFailed++;
                    r.failureMsg += "  " + field + ": expected " + std::to_string(eq)
                                  + ", got " + std::to_string(actual) + "\n";
                }
                return;
            }
            return; // unknown object — skip
        }

        if (!expected.is_number_integer()) {
            r.numFailed++;
            return;
        }
        int exp = expected.get<int>();
        if (actual != exp) {
            r.numFailed++;
            r.failureMsg += "  " + field + ": expected " + std::to_string(exp)
                          + ", got " + std::to_string(actual) + "\n";
        }
    }
};
