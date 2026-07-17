[**中文**](README_ZH.md) | **English**

# LDT — Lightweight Declarative Toolkit

[![Build LDT](https://github.com/timey777/ldt-ui/actions/workflows/build.yml/badge.svg)](https://github.com/timey777/ldt-ui/actions/workflows/build.yml)
[![Android Build](https://github.com/timey777/ldt-ui/actions/workflows/android.yml/badge.svg)](https://github.com/timey777/ldt-ui/actions/workflows/android.yml)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)

> 🚧 **Early Stage** — This project is in active early development. APIs and DSL syntax may change.

**LDT** is a **lightweight C++ native UI runtime** driven by a declarative DSL, designed for building small, cross-platform applications. Its concise DSL is also well-suited for AI-assisted UI generation.

---

## Design Philosophy

- **Lightweight First**: Small core binary, minimal dependencies, no heavy runtime
- **Declarative DSL**: Describe UI structure and styling via CSS-like `.ldt` files — concise, intuitive, and AI-friendly
- **Native Rendering**: OpenGL-based custom rendering ensures consistent cross-platform appearance without relying on system widgets
- **Cross-Platform**: One codebase for Windows, Linux desktop, and Android mobile

## Use Cases

- Desktop tools requiring **small footprint and high performance** native UI
- Projects where **AI generates the UI** directly (DSL syntax is LLM-friendly)
- Unified cross-platform desktop + mobile UI solutions

---

## Quick Start

### Linux

Install dependencies:

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

Build:

```bash
git clone https://github.com/timey777/ldt-ui.git
cd ldt-ui

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Run:

```bash
./build/bin/examples
```

### Windows

```powershell
cmake -B build
cmake --build build --config Release
```

### Android

See [Android Build Guide](android_example/BUILD_ANDROID.md).

Example source code is in the [`examples/`](examples/) directory.

---

## Documentation

| Document | Description |
|----------|-------------|
| [LDT DSL User Guide (中文)](docs/LDT_DSL_USER_GUIDE.md) | Complete DSL tutorial in Chinese |
| [LDT DSL User Guide (English)](docs/LDT_DSL_USER_GUIDE_EN.md) | Complete DSL tutorial in English |
| [LDT DSL Reference for AI (中文)](docs/LDT_DSL_AI_REFERENCE.md) | Compact reference for AI context (Chinese) |
| [LDT DSL Reference for AI (English)](docs/LDT_DSL_AI_REFERENCE_EN.md) | Compact reference for AI context (English) |

---

## Project Structure

```
ldt-ui/
├── src/                  # Core library source
│   ├── components/       # Built-in widgets (panel, text, button, input…)
│   ├── engine/           # Rendering engine & window management
│   ├── layout/           # Layout engine (block / flex / grid / inline)
│   ├── render/           # OpenGL rendering layer
│   └── debugger/         # Debugging utilities
├── examples/             # Example applications
├── docs/                 # Documentation
├── thirdparty/           # Third-party dependencies (glfw, glad, freetype)
├── ldt-tests/            # Test cases and test framework
├── android_example/      # Android project example
└── resources/            # Fonts and other resource files
```

---

## Platform Support

| Platform | Status |
|----------|--------|
| Windows (x64) | ✅ |
| Linux (x64) | ✅ |
| Android | ✅ |
| macOS | Planned |
| iOS | Planned |

---

## Roadmap

- 🧪 **Unit Test Suite** — Comprehensive automated tests for core components, layout engine, and DSL parser
- 📦 **DSL Versioning** — Support multiple DSL syntax versions with forward compatibility for smooth evolution
- 🖥️ **macOS / iOS Support** — Expand platform coverage
- 🔧 **Embedded / IoT Adaptation** — Provide a stripped-down build for resource-constrained devices

---

## Dependencies

- **GLFW** — Cross-platform window & input
- **OpenGL 3.3+** — Rendering backend
- **FreeType** — Font rasterization
- **CMake 3.10+** — Build system

---

## License

This project is licensed under the [Apache License 2.0](LICENSE).

---

## Disclaimer

This is an early-stage project. Features and APIs are subject to change and it is not yet recommended for production use. Feedback, testing, and contributions are welcome!
