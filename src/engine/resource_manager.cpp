#include "resource_manager.h"

#include "misc/filesystem.h"
#include "misc/logger.h"
#include "misc/stb_image.h"

#include <algorithm>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

ImageRawData::~ImageRawData() {
    if (data) {
        stbi_image_free(data);
        data = nullptr;
    }
}

FontResource::FontResource(const std::string& n, const std::string& path, const std::string& file)
    : name(n), filePath(path), fileName(file) {}

ResourceManager& ResourceManager::getInstance() {
    static ResourceManager instance;
    return instance;
}

ResourceManager::ResourceManager()
    : initialized_(false), defaultFontSize_(12.0f) {
    loaderThread_ = std::thread(&ResourceManager::loadThreadFunc, this);
}

ResourceManager::~ResourceManager() {
    running_ = false;
    queueCv_.notify_all();
    if (loaderThread_.joinable()) {
        loaderThread_.join();
    }
}

bool ResourceManager::initialize() {
    LDT_LOG("[ResourceManager] Initializing...");

    std::vector<std::string> fontDirectories;
    if (!resourceRoot_.empty()) {
        fontDirectories.push_back(resolvePath("resources/fonts"));
        fontDirectories.push_back(resolvePath("fonts"));
    } else {
        fontDirectories.push_back(FileSystem::getPath("resources/fonts"));
        fontDirectories.push_back(".");
    }

    for (const auto& fontsPath : fontDirectories) {
        LDT_LOG("[ResourceManager] Scanning fonts directory: " << fontsPath);
        if (!fs::exists(fontsPath) || !fs::is_directory(fontsPath)) {
            continue;
        }
        scanFontsInDirectory(fontsPath);
    }

    initialized_ = true;

    LDT_LOG("[ResourceManager] Initialization complete. Loaded " << fonts_.size() << " font(s).");
    return true;
}

void ResourceManager::preloadImage(const std::string& path) {
    if (path.empty()) return;

    std::lock_guard<std::mutex> lock(queueMutex_);
    if (pendingOrLoaded_.find(path) == pendingOrLoaded_.end()) {
        pendingOrLoaded_[path] = true;
        loadQueue_.push(path);
        queueCv_.notify_one();
    }
}

std::unique_ptr<ImageRawData> ResourceManager::popReadyImage() {
    std::lock_guard<std::mutex> lock(readyMutex_);
    if (readyQueue_.empty()) return nullptr;

    auto ptr = std::move(readyQueue_.front());
    readyQueue_.pop();
    return ptr;
}

static std::unique_ptr<ImageRawData> load_image_file(const std::string& path) {
    int w = 0;
    int h = 0;
    int orig_ch = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &orig_ch, 0);
#if _DEBUG
    if (!data) {
        data = stbi_load(FileSystem::getPath("resources/" + path).c_str(), &w, &h, &orig_ch, 0);
    }
#endif
    if (!data) {
        LDT_WARN("Failed to load image: " << path);
        return nullptr;
    }

    auto res = std::make_unique<ImageRawData>();
    res->path = path;
    res->data = data;
    res->width = w;
    res->height = h;
    res->channels = (orig_ch > 0) ? orig_ch : 4;
    return res;
}

void ResourceManager::loadThreadFunc() {
    while (running_) {
        std::string path;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait(lock, [this] { return !loadQueue_.empty() || !running_; });
            if (!running_) break;
            path = loadQueue_.front();
            loadQueue_.pop();
        }

        std::unique_ptr<ImageRawData> res = load_image_file(resolvePath(path));
        if (res) {
            res->path = path;
        }

        std::lock_guard<std::mutex> lock(readyMutex_);
        readyQueue_.push(std::move(res));
    }
}

void ResourceManager::scanFontsInDirectory(const std::string& dirPath) {
    try {
        for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            std::string filePath = entry.path().string();
            std::string fileName = entry.path().filename().string();
            if (!isFontFile(fileName)) {
                continue;
            }

            std::string fontName = extractFontName(fileName);
            std::string fontKey = fontName;
            std::transform(fontKey.begin(), fontKey.end(), fontKey.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (fonts_.find(fontKey) != fonts_.end()) {
                continue;
            }

            fonts_[fontKey] = FontResource(fontName, filePath, fileName);
        }
    } catch (const fs::filesystem_error& e) {
        LDT_ERROR("[ResourceManager] Filesystem error while scanning: " << e.what());
    }
}

const FontResource* ResourceManager::getFontByName(const std::string& name) const {
    std::string key = name;
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    auto it = fonts_.find(key);
    if (it != fonts_.end()) return &it->second;
    return nullptr;
}

std::string ResourceManager::extractFontName(const std::string& filePath) const {
    size_t lastSlash = filePath.find_last_of("/\\");
    std::string fileName = (lastSlash != std::string::npos) ? filePath.substr(lastSlash + 1) : filePath;
    size_t lastDot = fileName.find_last_of(".");
    if (lastDot != std::string::npos) {
        return fileName.substr(0, lastDot);
    }
    return fileName;
}

bool ResourceManager::isFontFile(const std::string& fileName) const {
    std::string lowerName = fileName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const std::vector<std::string> extensions = { ".ttf", ".otf", ".ttc", ".fon", ".fnt" };
    for (const auto& ext : extensions) {
        if (lowerName.length() >= ext.length() &&
            lowerName.compare(lowerName.length() - ext.length(), ext.length(), ext) == 0) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> ResourceManager::getAllFontNames() const {
    std::vector<std::string> names;
    names.reserve(fonts_.size());
    for (const auto& pair : fonts_) {
        names.push_back(pair.first);
    }
    return names;
}

size_t ResourceManager::getFontCount() const {
    return fonts_.size();
}

bool ResourceManager::loadFont(const std::string& fontPath, const std::string& fontName) {
    const std::string fullPath = resolvePath(fontPath);
    if (!fs::exists(fullPath) || !fs::is_regular_file(fullPath)) {
        LDT_ERROR("[ResourceManager] ERROR: Font file does not exist: " << fullPath);
        return false;
    }

    const std::string fileName = fs::path(fullPath).filename().string();
    if (!isFontFile(fileName)) {
        LDT_ERROR("[ResourceManager] ERROR: Not a valid font file: " << fileName);
        return false;
    }

    const std::string name = fontName.empty() ? extractFontName(fileName) : fontName;
    std::string fontKey = name;
    std::transform(fontKey.begin(), fontKey.end(), fontKey.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (fonts_.find(fontKey) != fonts_.end()) {
        LDT_LOG("[ResourceManager] Warning: Font '" << name << "' already exists. Replacing...");
    }

    fonts_[fontKey] = FontResource(name, fullPath, fileName);
    LDT_LOG("[ResourceManager] Loaded font: " << name << " -> " << fullPath);
    return true;
}

void ResourceManager::clear() {
    fonts_.clear();
    initialized_ = false;
    LDT_LOG("[ResourceManager] Cache cleared.");
}

void ResourceManager::printFontCache() const {
    if (fonts_.empty()) {
        LDT_LOG("[ResourceManager] Font cache is empty.");
        return;
    }

    LDT_LOG("\n=== Font Cache (" << fonts_.size() << " fonts) ===");
    for (const auto& pair : fonts_) {
        const FontResource& font = pair.second;
        LDT_LOG("  - Name: " << font.name << "\n"
                  << "    File: " << font.fileName << "\n"
                  << "    Path: " << font.filePath);
    }
    LDT_LOG("===================================\n");
}

std::string ResourceManager::getDefaultFontName() const {
    return defaultFontName_;
}

float ResourceManager::getDefaultFontSize() const {
    return defaultFontSize_;
}

void ResourceManager::setDefaultFont(const std::string& fontName, float fontSize) {
    defaultFontName_ = fontName;
    defaultFontSize_ = fontSize;
    LDT_LOG("[ResourceManager] Default font set to: " << fontName << " (size: " << fontSize << ")");
}

void ResourceManager::setResourceRoot(const std::string& resourceRoot) {
    resourceRoot_ = resourceRoot;
    LDT_LOG("[ResourceManager] Resource root set to: " << resourceRoot_);
}

std::string ResourceManager::resolvePath(const std::string& path) const {
    if (path.empty()) {
        return path;
    }

    const fs::path given(path);
    if (given.is_absolute() && fs::exists(given)) {
        return given.string();
    }

    if (!resourceRoot_.empty()) {
        const std::vector<fs::path> candidates = {
            fs::path(resourceRoot_) / path,
            fs::path(resourceRoot_) / "resources" / path,
        };
        for (const auto& candidate : candidates) {
            if (fs::exists(candidate)) {
                return candidate.string();
            }
        }
    }

    const std::string rooted = FileSystem::getPath(path);
    if (fs::exists(rooted)) {
        return rooted;
    }

    const std::string resourceScoped = FileSystem::getPath("resources/" + path);
    if (fs::exists(resourceScoped)) {
        return resourceScoped;
    }

    return path;
}

std::string ResourceManager::getResourceRoot() const {
    return resourceRoot_;
}
