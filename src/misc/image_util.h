#pragma once

#include <string>

namespace ldt {

class ImageUtil {
public:
    /**
     * @brief 获取图片尺寸
     * 
     * @param path 图片路径
     * @param width 输出宽度
     * @param height 输出高度
     * @return true 获取成功，false 获取失败
     */
    static bool getImageSize(const std::string& path, int& width, int& height);

    /**
     * @brief 加载图片数据 (RGBA)
     * 
     * @param path 图片路径
     * @param width 输出宽度
     * @param height 输出高度
     * @param channels 输出通道数
     * @return 图片数据指针 (使用 freeImage 释放)，失败返回 nullptr
     */
    static unsigned char* loadImage(const std::string& path, int& width, int& height, int& channels);

    /**
     * @brief 释放图片数据
     * 
     * @param data 图片数据指针
     */
    static void freeImage(unsigned char* data);
};

} // namespace ldt
