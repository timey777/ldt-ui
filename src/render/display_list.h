#pragma once

#include <string>
#include "graphics_types.h"
#include <vector>
#include <memory>
#include "text_layout.h"
#include <cstdlib>
#include <cstring>
#include <unordered_set>
#include "ldt_export.h"
#include "components/uitypes.h"
#include "misc/render_stats.h"

// 前向声明
class ResourceManager;

namespace ldt
{
    // 渲染指令类型
    enum class DrawCommandType
    {
        Rectangle,
        Text,
        TextLayout,
        Image,
        Line,
        Circle,
        PushClip,
        PopClip,
        PushTransform,
        PopTransform,
        SubList // 新增：子列表（图层）
    };

    // 渲染指令（值类型）：用于避免每条指令一次堆分配
    struct LDT_API DrawCommand
    {
        DrawCommandType type = DrawCommandType::Rectangle;

        ui::Rect bounds;
        ui::Color fillColor;
        ui::Color strokeColor;
        ui::Edges strokeWidths;

        // Rectangle
        float cornerRadius = 0.0f;

        // Text
        std::string text;
        std::string fontFamily;
        float fontSize = 0.0f;
        bool bold = false;
        bool italic = false;
        std::string textAlign = "left";
        float lineHeight = 0.0f;
        bool wrap = true;

        // TextLayout
        std::shared_ptr<ITextLayout> layout;

        // Image
        std::string source;

        // Transform
        float dx = 0.0f;
        float dy = 0.0f;

        // SubList
        std::shared_ptr<class DisplayList> layer;
    };

    // 显示列表
    class LDT_API DisplayList
    {
    public:
        DisplayList();

        void clear();

        void reserve(size_t n);

        // Remove commands that have no effect (e.g. empty clip/transform scopes).
        // This is a semantics-preserving pass: it only removes Push/Pop pairs that
        // enclose no draw/state-changing commands, and removes empty SubList nodes.
        void compact();

        void addRect(const ui::Rect &bounds, const ui::Color &fill, const ui::Color &stroke = ui::Color(),
                     const ui::Edges& strokeWidths = ui::Edges(), float cornerRadius = 0.0f);

        void addText(const std::string &text,
                     const ui::Rect &bounds,
                     const ui::Color &color,
                     float fontSize = 0.0f,
                     const std::string &fontFamily = "",
                     const TextLayoutParams &layout = TextLayoutParams());

        void addTextLayout(const std::shared_ptr<ITextLayout> &layout, const ui::Rect &bounds, const ui::Color &color);

        void addImage(const std::string &source, const ui::Rect &bounds);

        void pushClip(const ui::Rect &bounds);
        void pushClip(const ui::Rect &bounds, float cornerRadius);

        void popClip();

        void pushTransform(float dx, float dy);

        void popTransform();

        void addLayer(std::shared_ptr<DisplayList> layer);

        // ---- 内部辅助 ----
        template<typename F>
        void emitCommand(F&& init) {
            DrawCommand cmd;
            init(cmd);
            commands_.push_back(std::move(cmd));
        }

        const std::vector<DrawCommand> &getCommands() const;

        size_t size() const;

        // Serialization for debugging: simple line-based text format.
        // Note: TextLayout pointers are not serialized and will be skipped.
        std::string serializeToText() const;
        static DisplayList deserializeFromText(const std::string &text);

        /**
         * @brief Recursively collect draw-command statistics into `stats`.
         *        Traverses SubList layers as well.
         */
        void collectStats(RenderStats& stats) const;

    private:
        std::vector<DrawCommand> commands_;
    };

} // namespace ldt
