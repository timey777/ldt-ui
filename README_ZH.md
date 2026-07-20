**中文** | [**English**](README.md)

# LDT — Lightweight Declarative Toolkit

[![Build LDT](https://github.com/timey777/ldt-ui/actions/workflows/build.yml/badge.svg)](https://github.com/timey777/ldt-ui/actions/workflows/build.yml)
[![Android Build](https://github.com/timey777/ldt-ui/actions/workflows/android.yml/badge.svg)](https://github.com/timey777/ldt-ui/actions/workflows/android.yml)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)

> 🚧 **早期版本** — 本项目处于早期开发阶段，API 和 DSL 语法可能发生变化。

**LDT** 是一个使用 C++17 编写的**轻量级 C++ 原生 UI 运行时**，由声明式 DSL 驱动，专为构建小型跨平台应用而设计。其简洁的 DSL 语法也非常适合 AI 辅助生成原生 UI。

![LDT UI 预览](docs/preview.gif)

---

## 设计理念

- **轻量至上**：核心库体积小、依赖少，不引入重型运行时
- **声明式 DSL**：通过类 CSS 的 `.ldt` 文件描述 UI 结构和样式，直观简洁，AI 友好
- **原生渲染**：基于 OpenGL 自绘，无系统控件依赖，跨平台表现一致
- **跨平台**：一套代码，覆盖 Windows、Linux 桌面及 Android 移动端

## 适用场景

- 需要**小体积、高性能**原生 UI 的桌面工具
- 希望由 **AI 直接生成 UI** 的项目（DSL 语法对 LLM 友好）
- 跨平台桌面 + 移动端统一 UI 方案

---

## 快速开始

### Linux

安装依赖：

```bash
sudo apt update
sudo apt install -y \
  cmake \
  ninja-build \
  build-essential \
  libx11-dev \
  libxrandr-dev \
  libxinerama-dev \
  libxcursor-dev \
  libxi-dev \
  libfreetype-dev
```

构建：

```bash
git clone https://github.com/timey777/ldt-ui.git
cd ldt-ui

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

运行：

```bash
./build/bin/examples
```

### Windows

```powershell
cmake -B build
cmake --build build --config Release
```

### Android

参见 [Android 构建指南](android_example/BUILD_ANDROID.md)。

示例代码位于 [`examples/`](examples/) 目录。

---

## 文档

| 文档 | 说明 |
|------|------|
| [LDT DSL 用户指南（中文）](docs/LDT_DSL_USER_GUIDE.md) | DSL 语法、样式、布局完整教程 |
| [LDT DSL User Guide (English)](docs/LDT_DSL_USER_GUIDE_EN.md) | Complete DSL tutorial in English |
| [LDT DSL 语法参考·AI 版（中文）](docs/LDT_DSL_AI_REFERENCE.md) | 紧凑参考，适合注入 AI 上下文 |
| [LDT DSL Reference for AI (English)](docs/LDT_DSL_AI_REFERENCE_EN.md) | Compact reference for AI context |

---

## 项目结构

```
ldt-ui/
├── src/                  # 核心库源码
│   ├── components/       # 内置控件（panel, text, button, input…）
│   ├── engine/           # 渲染引擎、窗口管理
│   ├── layout/           # 布局引擎（block / flex / grid / inline）
│   ├── render/           # OpenGL 渲染层
│   └── debugger/         # 调试工具
├── examples/             # 示例代码
├── docs/                 # 文档
├── thirdparty/           # 第三方依赖（glfw, glad, freetype）
├── ldt-tests/            # 测试用例与测试框架
├── android_example/      # Android 工程示例
└── resources/            # 字体等资源文件
```

---

## 平台支持

| 平台 | 状态 |
|------|------|
| Windows (x64) | ✅ |
| Linux (x64) | ✅ |
| Android | ✅ |
| macOS | 计划中 |
| iOS | 计划中 |

---

## 下一步计划

- 🧪 **单元测试集** — 为核心组件、布局引擎和 DSL 解析器建立完备的自动化测试
- 📦 **DSL 版本管理** — 支持不同版本的 DSL 语法，确保向前兼容，平滑演进
- 🖥️ **macOS / iOS 支持** — 扩展平台覆盖
- 🔧 **嵌入式 / IoT 适配** — 面向资源受限设备提供裁剪版本

---

## 依赖

- **GLFW** — 跨平台窗口与输入
- **OpenGL 3.3+** — 渲染后端
- **FreeType** — 字体光栅化
- **CMake 3.10+** — 构建系统

---

## 许可证

本项目采用 [Apache License 2.0](LICENSE)。

---

## 功能介绍

本节包含两个扩展能力，可直接基于引擎原生 API 使用：

### 一、动态 UI 组件 — 构建可复用的运行时组合 UI 模块

`UIComponent`（`src/components/ui_component.h`）允许将 LDT UI 逻辑封装为**可动态挂载/卸载的模块**。每个模块拥有独立的 DSL、事件绑定和生命周期，可以像"插件"一样嵌入到任意 Scene 的任意节点下。

```cpp
namespace ldt {

class UIComponent {
public:
    virtual ~UIComponent() = default;
    virtual void onAttach(Scene* scene, const std::string& parentId) = 0;
    virtual void onDetach(Scene* scene) {}
};

}
```

#### 用法示例

一个"新建会话"弹出面板组件，封装了 DSL 解析、挂载和事件绑定：

```cpp
#include "components/ui_component.h"
#include "engine/core/parse.h"
#include "engine/core/resolved_builder.h"
#include "components/resolved_mount_service.h"

class NewSessionPopComponent : public UIComponent {
public:
    void onAttach(Scene* scene, const std::string& parentId) override {
        // 避免重复创建，已存在则直接显示
        auto existing = scene->getControlManager()->findControlById("session-pop");
        if (existing) { existing->setVisible(true); return; }

        // 解析 DSL 片段
        LDTParser parser;
        auto snippet = parser.parse(R"ui(
panel:session-pop(class="v-box pop") {
    button:btnCancelComp(){text(value="Cancel")}
}
)ui");

        // 构建并挂载解析树
        auto resolved = ResolvedBuilder::BuildResolvedSubtreeFromSnippet(
            snippet, scene->getDocumentRuntime());
        ResolvedMountService::MountResolvedSubtreeUnderId(
            scene->getResolvedTreeView(), parentId, resolved,
            scene->getDocumentRuntime());

        // 绑定点击事件
        scene->onClick("#btnCancelComp", [scene](const auto&) {
            auto pop = scene->getControlManager()->findControlById("session-pop");
            if (pop) pop->setVisible(false);
        });
    }
};
```

在 Scene 中触发挂载：

```cpp
onClick("#session-add", [this](const auto&) {
    auto popComp = std::make_shared<NewSessionPopComponent>();
    popComp->onAttach(this, "view");
});
```

| 概念 | 说明 |
|------|------|
| **即用即挂** | `onAttach()` 内完成解析 → 构建 → 挂载，用完可销毁或隐藏 |
| **内部封装** | DSL 模板、事件绑定、子组件创建全部在类内部完成，不污染 Scene |
| **位置灵活** | 通过 `parentId` 参数指定挂载到任意现有 panel 下 |
| **生命周期** | `onDetach()` 可反向拆除挂载的子树（默认空实现） |

---

### 二、多视图渲染 — 在同一场景中嵌入多个独立的 LDT 预览

`PreviewViewCoordinator`（`src/engine/preview_view_coordinator.h`）允许在一个 Scene 中嵌入**多个独立**的 LDT 渲染视图。每个视图拥有自己独立的 `DocumentRuntime`、`StyleEngine`、`LayoutEngine`、`ASTBuildEngine` 和 `Compositor`，可以独立加载和刷新不同的 LDT 内容，互不干扰。

```cpp
class PreviewViewCoordinator : public ViewCoordinator {
public:
    explicit PreviewViewCoordinator(
        Compositor* compositor,
        DocumentRuntime* context,
        ViewportSizeDp initialViewportSize,
        std::atomic<bool>* pendingPresent = nullptr,
        SceneResolver sceneResolver = SceneResolver(),
        std::string previewSlotId = "preview");

    // 继承自 ViewCoordinator：
    void bind(Scene* scene);              // 绑定到宿主 Scene
    void apply(DocumentUpdatePlan plan);   // 触发预览刷新
};
```

#### 用法示例

**1.** 在宿主 Scene 的 DSL 中定义占位 panel：

```ldt
panel:root(){
  panel:slotA() {}   /* 预览 A 的位置 */
  panel:slotB() {}   /* 预览 B 的位置 */
}
```

**2.** 创建并绑定 `PreviewViewCoordinator`（**务必作为成员变量保存**，防止提前析构）：

```cpp
#include "engine/preview_view_coordinator.h"
#include "engine/core/parse.h"
#include "engine/style_engine.h"
#include "engine/layout_engine.h"
#include "engine/astbuild_engine.h"
#include "engine/box_model_engine.h"

class MyScene : public Scene {
    // 注意：必须作为成员变量，不能是局部变量！
    std::unique_ptr<DocumentRuntime> runtimeA_;
    std::unique_ptr<Compositor> compositorA_;
    std::unique_ptr<PreviewViewCoordinator> previewA_;

    void setupPreviews() {
        // --- 预览 A ---
        auto styleA   = std::make_unique<StyleEngine>();
        auto layoutA  = std::make_unique<LayoutEngine>();
        auto astBuildA = std::make_unique<ASTBuildEngine>();

        runtimeA_ = std::make_unique<DocumentRuntime>();
        runtimeA_->registerService(styleA.get());
        runtimeA_->registerService(layoutA.get());
        runtimeA_->registerService(astBuildA.get());
        runtimeA_->setTextMeasurer(getDocumentRuntime()->getTextMeasurer());
        runtimeA_->initializeAll({ typeid(StyleEngine), typeid(LayoutEngine),
                                   typeid(ASTBuildEngine), typeid(BoxModelEngine) });

        compositorA_ = std::make_unique<Compositor>();
        previewA_ = std::make_unique<PreviewViewCoordinator>(
            compositorA_.get(), runtimeA_.get(),
            ViewportSizeDp{{300},{150}},
            nullptr,
            ViewCoordinator::SceneResolver{},
            "slotA");
        previewA_->bind(this);

        // 加载 LDT 内容
        LDTParser p;
        auto astA = p.parse("button(){text(value=\"Click Me\")}").root();
        runtimeA_->notifyASTUpdated(astA);
        previewA_->apply(DocumentUpdatePlan{DocumentUpdateKind::ASTRepaint});
    }
};
```

| 概念 | 说明 |
|------|------|
| **引擎独立** | 每个预览拥有独立的 `DocumentRuntime` + 引擎套件 + `Compositor` |
| **Slot ID 绑定** | 构造函数第 6 个参数指定渲染目标 — 对应宿主 DSL 中 `panel:xxx()` 的 ID |
| **独立刷新** | 每个预览可单独调用 `apply(ASTRepaint)`，不影响其他预览 |
| **共享 TextMeasurer** | 使用 `hostRuntime->getTextMeasurer()` 传入，避免重复创建 |
| **零侵入** | 宿主 Scene 只需定义占位 panel，无需额外继承或注册 |


---

## 免责声明

当前为早期版本，功能和接口可能随时调整，暂不建议用于生产环境。欢迎试用、反馈和贡献！
