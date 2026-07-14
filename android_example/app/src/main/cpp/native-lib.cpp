#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <typeindex>
#include <vector>

#include "android_dsl_scene.h"
#include "components/control_factory.h"
#include "components/control_manager.h"
#include "components/hit_test_helper.h"
#include "components/input.h"
#include "components/stage.h"
#include "engine/astbuild_engine.h"
#include "engine/box_model_engine.h"
#include "engine/compositor.h"
#include "engine/core/parse.h"
#include "engine/document_runtime.h"
#include "engine/input_events.h"
#include "engine/layout_engine.h"
#include "engine/update_scheduler.h"
#include "engine/view_coordinator.h"
#include "engine/resource_manager.h"
#include "engine/style_engine.h"
#include "misc/ui_diagnostics.h"
#include "render/graphics_context.h"
#include "render/opengl_drawer_factory.h"
#include "render/renderer.h"
#include "render/text_measurer.h"

#define LOG_TAG "LDTAndroid"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {
namespace fs = std::filesystem;

JavaVM* g_javaVm = nullptr;
jclass g_rendererLibClass = nullptr;

struct AndroidPlatformContext {
    AAssetManager* assetManager = nullptr;
    std::string filesDir;
    std::string extractedAssetsRoot;
};

AndroidPlatformContext& platformContext() {
    static AndroidPlatformContext context;
    return context;
}

JNIEnv* getJniEnv(bool* didAttach) {
    if (didAttach) {
        *didAttach = false;
    }
    if (!g_javaVm) {
        return nullptr;
    }

    JNIEnv* env = nullptr;
    const jint status = g_javaVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_OK) {
        return env;
    }
    if (status != JNI_EDETACHED) {
        return nullptr;
    }
    if (g_javaVm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
        return nullptr;
    }
    if (didAttach) {
        *didAttach = true;
    }
    return env;
}

void releaseJniEnv(bool didAttach) {
    if (didAttach && g_javaVm) {
        g_javaVm->DetachCurrentThread();
    }
}

extern "C" void dispatchToggleFullscreen() {
    bool didAttach = false;
    JNIEnv* env = getJniEnv(&didAttach);
    if (env && g_rendererLibClass) {
        jmethodID mid = env->GetStaticMethodID(g_rendererLibClass, "toggleFullscreen", "()V");
        if (mid) {
            env->CallStaticVoidMethod(g_rendererLibClass, mid);
        }
    }
    releaseJniEnv(didAttach);
}

constexpr int kAndroidPointerActionDown = 0;
constexpr int kAndroidPointerActionUp = 1;
constexpr int kAndroidPointerActionMove = 2;
constexpr int kAndroidPointerActionCancel = 3;
constexpr int kAndroidPointerActionPointerDown = 5;
constexpr int kAndroidPointerActionPointerUp = 6;
constexpr const char* kAppVersionToken = "__APP_VERSION__";

std::string jstring_to_utf8(JNIEnv* env, jstring value) {
    if (!value) {
        return std::string();
    }
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) {
        return std::string();
    }
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

std::string getAndroidAppVersionLabel() {
    bool didAttach = false;
    JNIEnv* env = getJniEnv(&didAttach);
    if (!env || !g_rendererLibClass) {
        releaseJniEnv(didAttach);
        return std::string();
    }

    const jmethodID method = env->GetStaticMethodID(g_rendererLibClass, "getAppVersionLabel", "()Ljava/lang/String;");
    if (!method) {
        releaseJniEnv(didAttach);
        return std::string();
    }

    auto result = static_cast<jstring>(env->CallStaticObjectMethod(g_rendererLibClass, method));
    std::string version = jstring_to_utf8(env, result);
    if (result) {
        env->DeleteLocalRef(result);
    }

    releaseJniEnv(didAttach);
    return version;
}

void replaceAll(std::string& text, const std::string& token, const std::string& replacement) {
    if (token.empty()) {
        return;
    }

    size_t position = 0;
    while ((position = text.find(token, position)) != std::string::npos) {
        text.replace(position, token.size(), replacement);
        position += replacement.size();
    }
}

std::string loadAssetText(const std::string& assetPath) {
    auto& platform = platformContext();
    std::string result;

    if (platform.assetManager) {
        AAsset* asset = AAssetManager_open(platform.assetManager, assetPath.c_str(), AASSET_MODE_BUFFER);
        if (asset) {
            const off_t length = AAsset_getLength(asset);
            result.assign(static_cast<size_t>(length), '\0');
            const int64_t bytesRead = AAsset_read(asset, result.data(), result.size());
            AAsset_close(asset);
            if (bytesRead < 0) {
                result.clear();
            } else {
                result.resize(static_cast<size_t>(bytesRead));
            }
        }
    }

    if (result.empty() && !platform.extractedAssetsRoot.empty()) {
        const fs::path extracted = fs::path(platform.extractedAssetsRoot) / fs::path(assetPath);
        if (fs::exists(extracted)) {
            std::ifstream stream(extracted, std::ios::binary);
            result = std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        }
    }

    if (result.empty()) {
        return std::string();
    }

    const std::string versionLabel = getAndroidAppVersionLabel();
    if (!versionLabel.empty()) {
        replaceAll(result, kAppVersionToken, versionLabel);
    }

    return result;
}

bool copyAssetTree(AAssetManager* assetManager, const std::string& assetPath, const fs::path& outputRoot) {
    if (!assetManager) {
        return false;
    }

    const char* dirPath = assetPath.empty() ? "" : assetPath.c_str();
    AAssetDir* dir = AAssetManager_openDir(assetManager, dirPath);
    if (!dir) {
        return false;
    }

    std::vector<std::string> children;
    while (const char* name = AAssetDir_getNextFileName(dir)) {
        children.emplace_back(name);
    }
    AAssetDir_close(dir);

    bool copiedAny = false;
    for (const auto& child : children) {
        const std::string childAssetPath = assetPath.empty() ? child : assetPath + "/" + child;
        AAsset* asset = AAssetManager_open(assetManager, childAssetPath.c_str(), AASSET_MODE_STREAMING);
        if (asset) {
            const off_t assetLength = AAsset_getLength(asset);
            std::vector<char> buffer(static_cast<size_t>(assetLength));
            const int64_t bytesRead = AAsset_read(asset, buffer.data(), buffer.size());
            AAsset_close(asset);
            if (bytesRead < 0) {
                continue;
            }

            const fs::path outputPath = outputRoot / fs::path(childAssetPath);
            fs::create_directories(outputPath.parent_path());
            std::ofstream stream(outputPath, std::ios::binary | std::ios::trunc);
            stream.write(buffer.data(), bytesRead);
            copiedAny = true;
            continue;
        }

        if (copyAssetTree(assetManager, childAssetPath, outputRoot)) {
            copiedAny = true;
        }
    }

    return copiedAny;
}

bool isFocusedTextInput(ldt::Scene* scene) {
    if (!scene || !scene->getUIEventSystem()) {
        return false;
    }
    auto* focusNode = scene->getUIEventSystem()->getFocusNode();
    if (!focusNode) {
        return false;
    }
    auto control = focusNode->getControl().lock();
    if (!control) {
        return false;
    }
    return control->getTypeName() == "Input";
}

bool isFocusedTextInputMultiline(ldt::Scene* scene) {
    if (!scene || !scene->getUIEventSystem()) {
        return false;
    }
    auto* focusNode = scene->getUIEventSystem()->getFocusNode();
    if (!focusNode) {
        return false;
    }
    auto control = focusNode->getControl().lock();
    if (!control) {
        return false;
    }
    auto* input = dynamic_cast<ldt::Input*>(control.get());
    return input != nullptr && input->getType() == "textarea";
}

void notifyJavaTextInputState(bool active, bool multiline) {
    bool didAttach = false;
    JNIEnv* env = getJniEnv(&didAttach);
    if (!env || !g_rendererLibClass) {
        releaseJniEnv(didAttach);
        return;
    }

    const jmethodID method = env->GetStaticMethodID(g_rendererLibClass, "syncSoftInputState", "(ZZ)V");
    if (!method) {
        releaseJniEnv(didAttach);
        return;
    }

    env->CallStaticVoidMethod(
        g_rendererLibClass,
        method,
        active ? JNI_TRUE : JNI_FALSE,
        multiline ? JNI_TRUE : JNI_FALSE);
    releaseJniEnv(didAttach);
}

int mapAndroidActionToMouseButtonAction(int action) {
    switch (action) {
    case kAndroidPointerActionDown:
    case kAndroidPointerActionPointerDown:
        return GLFW_PRESS;
    case kAndroidPointerActionUp:
    case kAndroidPointerActionPointerUp:
    case kAndroidPointerActionCancel:
        return GLFW_RELEASE;
    default:
        return GLFW_RELEASE;
    }
}

const char* androidPointerActionName(int action) {
    switch (action) {
    case kAndroidPointerActionDown:
        return "DOWN";
    case kAndroidPointerActionUp:
        return "UP";
    case kAndroidPointerActionMove:
        return "MOVE";
    case kAndroidPointerActionCancel:
        return "CANCEL";
    case kAndroidPointerActionPointerDown:
        return "POINTER_DOWN";
    case kAndroidPointerActionPointerUp:
        return "POINTER_UP";
    default:
        return "UNKNOWN";
    }
}

std::string describeNode(ldt::ResolvedNode* node) {
    if (!node) {
        return "-";
    }

    const std::string id = node->getId();
    if (!id.empty()) {
        return id;
    }

    if (node->astNode) {
        return node->astNode->type;
    }

    return "node";
}

int mapAndroidKeyToGlfw(int keyCode) {
    switch (keyCode) {
    case 4: return GLFW_KEY_ESCAPE;
    case 61: return GLFW_KEY_TAB;
    case 62: return GLFW_KEY_SPACE;
    case 66: return GLFW_KEY_ENTER;
    case 67: return GLFW_KEY_BACKSPACE;
    case 92: return GLFW_KEY_PAGE_UP;
    case 93: return GLFW_KEY_PAGE_DOWN;
    case 112: return GLFW_KEY_DELETE;
    case 122: return GLFW_KEY_HOME;
    case 123: return GLFW_KEY_END;
    case 19: return GLFW_KEY_UP;
    case 20: return GLFW_KEY_DOWN;
    case 21: return GLFW_KEY_LEFT;
    case 22: return GLFW_KEY_RIGHT;
    case 57: return GLFW_KEY_LEFT_ALT;
    case 58: return GLFW_KEY_RIGHT_ALT;
    case 59: return GLFW_KEY_LEFT_SHIFT;
    case 60: return GLFW_KEY_RIGHT_SHIFT;
    case 113: return GLFW_KEY_LEFT_CONTROL;
    case 114: return GLFW_KEY_RIGHT_CONTROL;
    default:
        break;
    }

    if (keyCode >= 29 && keyCode <= 54) {
        return GLFW_KEY_A + (keyCode - 29);
    }
    if (keyCode >= 7 && keyCode <= 16) {
        return GLFW_KEY_0 + (keyCode - 7);
    }
    return GLFW_KEY_UNKNOWN;
}

int mapAndroidKeyActionToGlfw(int action) {
    switch (action) {
    case 0: return GLFW_PRESS;
    case 1: return GLFW_RELEASE;
    case 2: return GLFW_REPEAT;
    default: return GLFW_PRESS;
    }
}

int mapAndroidMetaStateToGlfwMods(int metaState) {
    constexpr int kMetaShiftOn = 0x1;
    constexpr int kMetaAltOn = 0x02;
    constexpr int kMetaCtrlOn = 0x1000;

    int mods = 0;
    if ((metaState & kMetaShiftOn) != 0) mods |= 0x0001;
    if ((metaState & kMetaAltOn) != 0) mods |= 0x0004;
    if ((metaState & kMetaCtrlOn) != 0) mods |= GLFW_MOD_CONTROL;
    return mods;
}

class AndroidRuntime {
public:
    void configurePlatform(AAssetManager* assetManager, std::string filesDir) {
        auto& platform = platformContext();
        platform.assetManager = assetManager;
        platform.filesDir = std::move(filesDir);
        platform.extractedAssetsRoot = (fs::path(platform.filesDir) / "ldt_assets").string();
    }

    void setDslSource(std::string dslSource) {
        dsl_source_ = std::move(dslSource);
        if (scene_) {
            scene_->setSource(dsl_source_);
            scene_->reload();
            syncTextInputStateWithPlatform();
        }
    }

    void setDslSourceFromAsset(const std::string& assetPath) {
        setDslSource(loadAssetText(assetPath));
    }

    void init() {
        if (initialized_) {
            return;
        }

        registerAllParsers();
        initializeResources();

        graphics_context_ = createOpenGLGraphicsContext();
        graphics_context_->init(1, 1);
        renderer_ = std::make_unique<ldt::Renderer>(graphics_context_->getDrawer());
        compositor_ = std::make_unique<ldt::Compositor>();
        compositor_->setRenderer(renderer_.get());

        document_runtime_ = std::make_unique<DocumentRuntime>();
        style_engine_ = std::make_unique<StyleEngine>();
        layout_engine_ = std::make_unique<LayoutEngine>();
        ast_build_engine_ = std::make_unique<ASTBuildEngine>();
        box_model_engine_ = std::make_unique<BoxModelEngine>();

        document_runtime_->registerService(style_engine_.get());
        document_runtime_->registerService(layout_engine_.get());
        document_runtime_->registerService(ast_build_engine_.get());
        document_runtime_->registerService(box_model_engine_.get());
        document_runtime_->setTextMeasurer(dynamic_cast<ITextMeasurer*>(graphics_context_->getDrawer()));
        document_runtime_->initializeAll({
            typeid(StyleEngine),
            typeid(LayoutEngine),
            typeid(ASTBuildEngine),
            typeid(BoxModelEngine),
        });

        host_ = std::make_unique<ldt::ViewCoordinator>(
            compositor_.get(), document_runtime_.get(),
            ldt::ViewportSizeDp{},
            &pending_present_,
            []() -> ldt::Scene* {
                auto scene = ldt::Stage::getInstance().currentScene();
                return scene ? scene.get() : nullptr;
            });

        scene_ = std::make_shared<AndroidDslScene>(*document_runtime_);
        scene_->setSource(dsl_source_);
        ldt::Stage::getInstance().replaceScene(scene_);

        // Platform host for clipboard
        struct AndroidPlatformHost : public ldt::IPlatformHost {
            void setClipboardString(const std::string& str) override {
                bool didAttach = false;
                JNIEnv* env = getJniEnv(&didAttach);
                if (!env || !g_rendererLibClass) { releaseJniEnv(didAttach); return; }
                const jmethodID method = env->GetStaticMethodID(g_rendererLibClass, "setClipboardText", "(Ljava/lang/String;)V");
                if (!method) { releaseJniEnv(didAttach); return; }
                jstring text = env->NewStringUTF(str.c_str());
                env->CallStaticVoidMethod(g_rendererLibClass, method, text);
                env->DeleteLocalRef(text);
                releaseJniEnv(didAttach);
            }
            std::string getClipboardString() override {
                bool didAttach = false;
                JNIEnv* env = getJniEnv(&didAttach);
                if (!env || !g_rendererLibClass) { releaseJniEnv(didAttach); return std::string(); }
                const jmethodID method = env->GetStaticMethodID(g_rendererLibClass, "getClipboardText", "()Ljava/lang/String;");
                if (!method) { releaseJniEnv(didAttach); return std::string(); }
                auto result = static_cast<jstring>(env->CallStaticObjectMethod(g_rendererLibClass, method));
                std::string clipboard = jstring_to_utf8(env, result);
                if (result) { env->DeleteLocalRef(result); }
                releaseJniEnv(didAttach);
                return clipboard;
            }
        };
        static AndroidPlatformHost s_platformHost;
        ldt::Stage::getInstance().setPlatformHost(&s_platformHost);

        initialized_ = true;
        pending_present_.store(true);
        syncTextInputStateWithPlatform();
        LOGI("ldt Android runtime initialized");
    }

    void onSurfaceCreated() {
        const bool hadInitialized = initialized_;
        init();

        if (!hadInitialized) {
            return;
        }

        if (!graphics_context_) {
            return;
        }

        const int initWidth = framebuffer_size_.width > 0 ? framebuffer_size_.width : 1;
        const int initHeight = framebuffer_size_.height > 0 ? framebuffer_size_.height : 1;

        graphics_context_->init(initWidth, initHeight);

        if (viewport_size_.width.value > 0.0f && viewport_size_.height.value > 0.0f) {
            host_->setViewport(viewport_size_);
            graphics_context_->resize({ initWidth, initHeight }, content_scale_);
        }

        if (compositor_) {
            compositor_->clear();
        }

        if (scene_) {
            scene_->reload();
        }

        pending_present_.store(true);
        syncTextInputStateWithPlatform();
        LOGI("Android surface recreated, GL resources restored");
    }

    void onPause() {
        if (!initialized_) {
            return;
        }

        if (scene_) {
            scene_->clearPointerState();
        }

        last_text_input_active_ = false;
        last_text_input_multiline_ = false;
        notifyJavaTextInputState(false, false);
    }

    void onResume() {
        if (!initialized_) {
            return;
        }

        pending_present_.store(true);
        syncTextInputStateWithPlatform();
    }

    void clearPointerState() {
        if (!initialized_ || !scene_) {
            return;
        }

        appendAndroidDebugLog("runtime clear-pointer-state");
        scene_->clearPointerState();
        pending_present_.store(true);
    }

    void resize(int width, int height, float scaleX, float scaleY) {
        init();
        if (width <= 0 || height <= 0) {
            return;
        }

        framebuffer_size_ = { width, height };
        content_scale_ = ldt::sanitizeScale({ { scaleX }, { scaleY } });
        viewport_size_ = ldt::toViewportSizeDp(framebuffer_size_, content_scale_);
        host_->setViewport(viewport_size_);
        graphics_context_->resize(framebuffer_size_, content_scale_);
        host_->handleResize(viewport_size_);
        ensureInitialSceneSubmitted();
        syncTextInputStateWithPlatform();
        pending_present_.store(true);
    }

    void step() {
        init();
        if (viewport_size_.width.value <= 0.0f || viewport_size_.height.value <= 0.0f) {
            return;
        }

        auto currentScene = ldt::Stage::getInstance().currentScene();
        if (currentScene && currentScene->getControlManager()) {
            const bool hasAnimation = currentScene->getControlManager()->processAnimatedControls();
            if (hasAnimation) {
                UpdateScheduler::getInstance().requestPaint();
            }
        }

        auto updatePlan = UpdateScheduler::getInstance().consumePendingPlan();
        if (host_) {
            host_->apply(updatePlan);
        }

        if (currentScene && currentScene->getResolvedTree()) {
            ldt::ControlFactory::getInstance()->executePendingRemovals();
        }

        syncTextInputStateWithPlatform();

        graphics_context_->clear(1.0f, 1.0f, 1.0f, 1.0f);
        compositor_->paintAll();
        graphics_context_->present();
        pending_present_.store(false);
    }

    void onTouch(int /*pointerId*/, int action, float x, float y) {
        init();

        auto scene = ldt::Stage::getInstance().currentScene();
        if (!scene) {
            return;
        }

        const ldt::LogicalPointDp logicalPoint = ldt::toLogicalPointDp({ x, y }, content_scale_);
        ldt::HitTestHelper::HitTestResult hit;
        if (scene->getResolvedTree()) {
            hit = ldt::HitTestHelper::performHitTest(scene->getResolvedTree(), logicalPoint);
        }
        ldt::diagnostics::logInputMapping(
            "android.onTouch.pre",
            androidPointerActionName(action),
            { x, y },
            content_scale_,
            logicalPoint,
            hit.hitNode);

        if (action == kAndroidPointerActionMove) {
            scene->onMouseMove(logicalPoint);
        } else {
            scene->onMouseMove(logicalPoint);
            scene->onMouseButton(logicalPoint, 0, mapAndroidActionToMouseButtonAction(action));
        }

        ldt::diagnostics::logSceneUiState(
            "android.onTouch.post",
            scene->getUIEventSystem(),
            scene->getMouseCapture());

        syncTextInputStateWithPlatform();
        pending_present_.store(true);
    }

    void onKey(int keyCode, int action, int metaState, int unicodeCodePoint) {
        init();

        auto scene = ldt::Stage::getInstance().currentScene();
        if (!scene) {
            return;
        }

        scene->onKey(
            mapAndroidKeyToGlfw(keyCode),
            0,
            mapAndroidKeyActionToGlfw(action),
            mapAndroidMetaStateToGlfwMods(metaState));

        if (unicodeCodePoint > 0 && action != 1) {
            scene->onChar(static_cast<unsigned int>(unicodeCodePoint));
        }

        syncTextInputStateWithPlatform();
        pending_present_.store(true);
    }

    void onScroll(float dx, float dy) {
        init();

        auto scene = ldt::Stage::getInstance().currentScene();
        if (!scene) {
            return;
        }

        const ldt::LogicalScrollDeltaDp logicalScroll = ldt::toLogicalScrollDeltaDp({ dx, dy }, content_scale_);
        scene->onScroll(logicalScroll);
        pending_present_.store(true);
    }

    bool isTextInputActive() const {
        return isFocusedTextInput(ldt::Stage::getInstance().currentScene().get());
    }

    bool isTextInputMultiline() const {
        return isFocusedTextInputMultiline(ldt::Stage::getInstance().currentScene().get());
    }

    bool handleBackPressed() {
        init();
        auto scene = ldt::Stage::getInstance().currentScene();
        if (!isFocusedTextInput(scene.get())) {
            return false;
        }

        if (scene->getUIEventSystem()) {
            scene->getUIEventSystem()->updateFocus(nullptr);
        }
        syncTextInputStateWithPlatform();
        pending_present_.store(true);
        return true;
    }

private:
    void syncTextInputStateWithPlatform() {
        const bool active = isTextInputActive();
        const bool multiline = active && isTextInputMultiline();
        if (active == last_text_input_active_ && multiline == last_text_input_multiline_) {
            return;
        }

        last_text_input_active_ = active;
        last_text_input_multiline_ = multiline;
        notifyJavaTextInputState(active, multiline);
    }

    void ensureInitialSceneSubmitted() {
        if (!scene_) {
            return;
        }

        auto* controlManager = scene_->getControlManager();
        const bool hasControls = controlManager != nullptr && !controlManager->getControls().empty();
        const bool hasResolvedRoot = scene_->getResolvedTree() != nullptr &&
            scene_->getResolvedTree()->getRoot() != nullptr;

        if (hasControls && hasResolvedRoot) {
            return;
        }

        LOGI("Submitting initial Android scene after viewport became ready");
        scene_->reload();
    }

    static void registerAllParsers() {
        ldt::ParserRegistry::instance().registerParser("import", std::make_shared<ldt::ImportParser>());
        ldt::ParserRegistry::instance().registerParser("style", std::make_shared<ldt::StyleParser>());
        ldt::ParserRegistry::instance().registerParser("layout", std::make_shared<ldt::LayoutParser>());
        ldt::ParserRegistry::instance().registerParser("content", std::make_shared<ldt::ContentParser>());
    }

    static void initializeResources() {
        auto& resourceManager = ResourceManager::getInstance();
        auto& platform = platformContext();
        if (!platform.extractedAssetsRoot.empty()) {
            try {
                if (platform.assetManager) {
                    fs::create_directories(platform.extractedAssetsRoot);
                    copyAssetTree(platform.assetManager, "", platform.extractedAssetsRoot);
                }
            } catch (const std::exception& ex) {
                LOGE("Failed to extract Android assets during initialization: %s", ex.what());
            }
            resourceManager.setResourceRoot(platform.extractedAssetsRoot);
        }
        resourceManager.initialize();

        auto fontNames = resourceManager.getAllFontNames();
        if (fontNames.empty()) {
            const char* fallbackPaths[] = {
                "/system/fonts/Roboto-Regular.ttf",
                "/system/fonts/NotoSansCJK-Regular.ttc",
                "/system/fonts/DroidSans.ttf",
            };
            for (const char* path : fallbackPaths) {
                if (resourceManager.loadFont(path)) {
                    break;
                }
            }
            fontNames = resourceManager.getAllFontNames();
        }
        if (!fontNames.empty()) {
            resourceManager.setDefaultFont(fontNames.front(), 14.0f);
        }
    }

    bool initialized_ = false;
    ldt::ViewportSizeDp viewport_size_;
    ldt::FramebufferSizePx framebuffer_size_;
    ldt::ContentScale content_scale_;
    bool last_text_input_active_ = false;
    bool last_text_input_multiline_ = false;
    std::string dsl_source_;
    std::atomic<bool> pending_present_{false};

    std::unique_ptr<IGraphicsContext> graphics_context_;
    std::unique_ptr<ldt::Renderer> renderer_;
    std::unique_ptr<ldt::Compositor> compositor_;
    std::unique_ptr<DocumentRuntime> document_runtime_;
    std::unique_ptr<StyleEngine> style_engine_;
    std::unique_ptr<LayoutEngine> layout_engine_;
    std::unique_ptr<ASTBuildEngine> ast_build_engine_;
    std::unique_ptr<BoxModelEngine> box_model_engine_;
    std::unique_ptr<ldt::ViewCoordinator> host_;
    std::shared_ptr<AndroidDslScene> scene_;
};

AndroidRuntime& runtime() {
    static AndroidRuntime instance;
    return instance;
}

}  // namespace

std::string loadAndroidAssetText(const std::string& assetPath) {
    return loadAssetText(assetPath);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_pureopengl_RendererLib_configurePlatform(JNIEnv* env, jclass, jobject assetManager, jstring filesDir) {
    if (!g_rendererLibClass) {
        jclass localClass = env->FindClass("com/example/pureopengl/RendererLib");
        if (localClass) {
            g_rendererLibClass = static_cast<jclass>(env->NewGlobalRef(localClass));
            env->DeleteLocalRef(localClass);
        }
    }
    runtime().configurePlatform(AAssetManager_fromJava(env, assetManager), jstring_to_utf8(env, filesDir));
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_pureopengl_RendererLib_setDslSource(JNIEnv* env, jclass, jstring dsl_source) {
    runtime().setDslSource(jstring_to_utf8(env, dsl_source));
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_pureopengl_RendererLib_setDslSourceFromAsset(JNIEnv* env, jclass, jstring asset_path) {
    runtime().setDslSourceFromAsset(jstring_to_utf8(env, asset_path));
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_pureopengl_RendererLib_init(JNIEnv*, jclass) {
    runtime().init();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_pureopengl_RendererLib_onSurfaceCreated(JNIEnv*, jclass) {
    runtime().onSurfaceCreated();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_pureopengl_RendererLib_onPause(JNIEnv*, jclass) {
    runtime().onPause();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_pureopengl_RendererLib_onResume(JNIEnv*, jclass) {
    runtime().onResume();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_pureopengl_RendererLib_step(JNIEnv*, jclass) {
    runtime().step();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_pureopengl_RendererLib_resize(JNIEnv*, jclass, jint width, jint height, jfloat scale_x, jfloat scale_y) {
    runtime().resize(width, height, scale_x, scale_y);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_pureopengl_RendererLib_clearPointerState(JNIEnv*, jclass) {
    runtime().clearPointerState();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_pureopengl_RendererLib_onTouch(JNIEnv*, jclass, jint pointer_id, jint action, jfloat x, jfloat y) {
    runtime().onTouch(pointer_id, action, x, y);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_pureopengl_RendererLib_onScroll(JNIEnv*, jclass, jfloat dx, jfloat dy) {
    runtime().onScroll(dx, dy);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_pureopengl_RendererLib_onKey(JNIEnv*, jclass, jint key_code, jint action, jint meta_state, jint unicode_code_point) {
    runtime().onKey(key_code, action, meta_state, unicode_code_point);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_pureopengl_RendererLib_isTextInputActive(JNIEnv*, jclass) {
    return runtime().isTextInputActive() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_pureopengl_RendererLib_isTextInputMultiline(JNIEnv*, jclass) {
    return runtime().isTextInputMultiline() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_pureopengl_RendererLib_handleBackPressed(JNIEnv*, jclass) {
    return runtime().handleBackPressed() ? JNI_TRUE : JNI_FALSE;
}

jint JNI_OnLoad(JavaVM* vm, void*) {
    g_javaVm = vm;
    return JNI_VERSION_1_6;
}
