#pragma once

#include <vector>
#include <string>
#include "render/graphics_types.h"

/**
 * @brief 文本布局接口
 * 
 * 用于将文本的排版计算（Layout）与绘制（Rendering）分离。
 * 
 * 优化点：
 * 1. 缓存排版结果：对于 Input 组件，文本内容不经常变动（相比于帧率），
 *    将排版计算结果缓存起来，避免每帧重复计算字形位置、换行等。
 * 2. 提供交互查询：Input 组件需要根据鼠标坐标获取光标位置，或根据字符索引获取光标坐标。
 *    ITextLayout 负责提供这些几何信息，保证绘制与交互逻辑的一致性。
 * 3. 渲染优化：实现类可以在内部构建用于实例化绘制（Instanced Rendering）的顶点数据或变换矩阵列表，
 *    在 drawTextLayout 时直接提交给 GPU，极大提升性能。
 */
class ITextLayout {
public:
    virtual ~ITextLayout() = default;

    /**
     * @brief 获取文本布局的总边界（相对于绘制原点 (0,0)）
     */
    virtual render::Rect getBounds() const = 0;

    /**
     * @brief 根据局部坐标 (x, y) 获取对应的字符索引
     * 
     * @param x 局部 x 坐标
     * @param y 局部 y 坐标
     * @return int 字符索引 (0 到 text.length())。
     *         如果点击在字符左半部分，返回该字符索引；右半部分则返回下一个索引。
     *         用于鼠标点击确定光标位置。
     */
    virtual int getCharIndexAt(float x, float y) const = 0;

    /**
     * @brief 获取指定字符索引处的光标位置信息
     * 
     * @param index 字符索引
     * @return Rect 光标的几何信息 (x, y, width, height)。
     *         通常 width 为 1-2 像素，height 为行高。
     */
    virtual render::Rect getCursorRect(int index) const = 0;

    /**
     * @brief 获取指定范围内的选区矩形列表
     * 
     * @param startIndex 起始索引
     * @param endIndex 结束索引
     * @return std::vector<Rect> 选区矩形列表。
     *         如果是多行文本，选区可能由多个矩形组成。
     */
    virtual std::vector<render::Rect> getSelectionRects(int startIndex, int endIndex) const = 0;

    /**
     * @brief 获取原始文本内容
     */
    virtual std::string getText() const = 0;
    
    /**
     * @brief 获取行数
     */
    virtual int getLineCount() const = 0;

    /**
     * @brief 获取某行的信息（可选，用于更高级的导航）
     */
    // virtual LineMetrics getLineMetrics(int lineIndex) const = 0;
};
