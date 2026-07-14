[**中文**](README_ZH.md) | **English**

# LDT — Lightweight Declarative Toolkit

> 🚧 **Early Stage** — This project is in active early development. APIs and DSL syntax may change.

**LDT** is a **lightweight declarative UI framework** written in C++17, designed for desktop (Windows, Linux) and mobile (Android). Its concise DSL is also well-suited for AI-assisted UI generation.

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

### Build (Desktop)

```bash
# Linux / macOS
chmod +x build.sh
./build.sh

# Output binaries are in build/bin/
```

**Windows**: Open `CMakeLists.txt` with CMake GUI or Visual Studio.

### Build (Android)

See [Android Build Guide](android_example/BUILD_ANDROID.md).

### Run Examples

```bash
cd build
./bin/examples
```

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
