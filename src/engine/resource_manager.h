#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include <string>
#include <map>
#include <memory>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "ldt_export.h"

/**
 * @brief 字体资源信息
 */
struct FontResource {
    std::string name;           // 字体名称 (作为 key)
    std::string filePath;       // 字体文件的完整路径
    std::string fileName;       // 原始文件名
    
    FontResource() = default;
    FontResource(const std::string& n, const std::string& path, const std::string& file);
};

struct ImageRawData {
    std::string path;
    unsigned char* data = nullptr;
    int width = 0;
    int height = 0;
    int channels = 0;
    
    ~ImageRawData(); // Custom destructor to free data
};

/**
 * @brief 资源管理器类
 * 
 * 负责加载和缓存应用程序资源，特别是字体文件。
 * 默认在初始化时扫描并缓存 resources/fonts 目录下的所有字体。
 */
class LDT_API ResourceManager {
public:
    /**
     * @brief 获取 ResourceManager 的单例实例
     */
    static ResourceManager& getInstance();
    
    ~ResourceManager();

    /**
     * @brief 初始化资源管理器
     * 
     * 扫描并缓存 resources/fonts 目录下的所有字体文件
     * 
     * @return true 成功初始化，false 初始化失败
     */
    bool initialize();
    
    /**
     * @brief 根据名称获取字体资源
     * 
     * @param name 字体名称 (不包含扩展名)
     * @return 字体资源指针，如果未找到返回 nullptr
     */
    const FontResource* getFontByName(const std::string& name) const;

    // 异步加载图片
    void preloadImage(const std::string& path);
    
    // 获取已加载完成的图片数据（如果有），并从队列中移除
    // 返回 nullptr 表示该图片尚未就绪或队列为空
    std::unique_ptr<ImageRawData> popReadyImage();

    /**
     * @brief 获取所有已缓存的字体名称列表
     * 
     * @return 字体名称列表
     */
    std::vector<std::string> getAllFontNames() const;
    
    /**
     * @brief 获取已缓存的字体数量
     * 
     * @return 字体数量
     */
    size_t getFontCount() const;
    
    /**
     * @brief 手动加载单个字体文件
     * 
     * @param fontPath 字体文件的相对路径 (相对于项目根目录)
     * @param fontName 字体名称 (如果为空，则从文件名自动提取)
     * @return true 成功加载，false 加载失败
     */
    bool loadFont(const std::string& fontPath, const std::string& fontName = "");
    
    /**
     * @brief 清除所有缓存的字体资源
     */
    void clear();
    
    /**
     * @brief 打印所有已缓存的字体信息 (调试用)
     */
    void printFontCache() const;
    
    /**
     * @brief 获取默认字体名称
     * 
     * @return 默认字体名称
     */
    std::string getDefaultFontName() const;
    
    /**
     * @brief 获取默认字体大小
     * 
     * @return 默认字体大小
     */
    float getDefaultFontSize() const;
    
    /**
     * @brief 设置默认字体
     * 
     * @param fontName 字体名称
     * @param fontSize 字体大小
     */
    void setDefaultFont(const std::string& fontName, float fontSize);
    void setResourceRoot(const std::string& resourceRoot);
    std::string resolvePath(const std::string& path) const;
    std::string getResourceRoot() const;

private:
    ResourceManager();
    
    // 禁止拷贝和赋值
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    void loadThreadFunc();

    /**
     * @brief 递归扫描目录并加载字体文件
     * 
     * @param dirPath 目录路径
     */
    void scanFontsInDirectory(const std::string& dirPath);
    
    /**
     * @brief 从文件名中提取字体名称 (去除扩展名和路径)
     * 
     * @param filePath 文件路径
     * @return 字体名称
     */
    std::string extractFontName(const std::string& filePath) const;
    
    /**
     * @brief 判断文件是否为字体文件
     * 
     * @param fileName 文件名
     * @return true 是字体文件，false 不是字体文件
     */
    bool isFontFile(const std::string& fileName) const;

private:
    std::map<std::string, FontResource> fonts_;      // 字体缓存 (key: 字体名称)
    std::string resourceRoot_;
    bool initialized_;                                // 是否已初始化
    
    // 默认字体配置
    std::string defaultFontName_;                    // 默认字体名称
    float defaultFontSize_;                          // 默认字体大小

    // 异步加载相关
    std::queue<std::string> loadQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::atomic<bool> running_{true};
    std::thread loaderThread_;

    std::queue<std::unique_ptr<ImageRawData>> readyQueue_;
    std::mutex readyMutex_;
    
    std::map<std::string, bool> pendingOrLoaded_;
};

#endif // RESOURCE_MANAGER_H
