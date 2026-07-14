**中文** | [**English**](README.md)

# LDT — Lightweight Declarative Toolkit

> 🚧 **早期版本** — 本项目处于早期开发阶段，API 和 DSL 语法可能发生变化。

**LDT** 是一个使用 C++17 编写的**轻量级声明式 UI 框架**，专为桌面端（Windows、Linux）和移动端（Android）打造，也可以方便地与 AI 配合生成原生 UI。

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

### 构建（桌面端）

```bash
# Linux / macOS
chmod +x build.sh
./build.sh

# 构建产物位于 build/bin/ 目录
```

**Windows**：使用 CMake GUI 或 Visual Studio 打开 `CMakeLists.txt` 即可。

### 构建（Android）

参见 [Android 构建指南](android_example/BUILD_ANDROID.md)。

### 运行示例

```bash
cd build
./bin/examples
```

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

## 免责声明

当前为早期版本，功能和接口可能随时调整，暂不建议用于生产环境。欢迎试用、反馈和贡献！
