#include "display_list.h"
#include "engine/resource_manager.h"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <stack>

namespace ldt
{
    thread_local RenderStats* g_currentRenderStats = nullptr;

    // Move previously-inline DisplayList implementations here.
    DisplayList::DisplayList()
    {
        // 常见 UI 会产生成百上千条指令；预留可显著减少首帧/抖动分配
        commands_.reserve(256);
    }

    void DisplayList::clear() { commands_.clear(); }

    void DisplayList::reserve(size_t n) { commands_.reserve(n); }

    void DisplayList::compact()
    {
        if (commands_.empty())
            return;

        std::vector<DrawCommand> out;
        out.reserve(commands_.size());

        std::vector<size_t> clipPushIndex;
        std::vector<size_t> transformPushIndex;
        clipPushIndex.reserve(16);
        transformPushIndex.reserve(16);

        auto is_empty_sentinel = [](const DrawCommand &cmd)
        {
            return cmd.type == DrawCommandType::SubList && (!cmd.layer || cmd.layer->size() == 0);
        };

        auto emit = [&](const DrawCommand &cmd)
        {
            if (is_empty_sentinel(cmd))
            {
                return;
            }

            if (cmd.type == DrawCommandType::PushClip)
            {
                clipPushIndex.push_back(out.size());
                out.push_back(cmd);
                return;
            }
            if (cmd.type == DrawCommandType::PopClip)
            {
                if (!clipPushIndex.empty())
                {
                    size_t pushIdx = clipPushIndex.back();
                    clipPushIndex.pop_back();
                    // If PushClip was the last emitted command, this scope is empty.
                    if (pushIdx + 1 == out.size())
                    {
                        out.pop_back();
                        return;
                    }
                }
                out.push_back(cmd);
                return;
            }

            if (cmd.type == DrawCommandType::PushTransform)
            {
                transformPushIndex.push_back(out.size());
                out.push_back(cmd);
                return;
            }
            if (cmd.type == DrawCommandType::PopTransform)
            {
                if (!transformPushIndex.empty())
                {
                    size_t pushIdx = transformPushIndex.back();
                    transformPushIndex.pop_back();
                    if (pushIdx + 1 == out.size())
                    {
                        out.pop_back();
                        return;
                    }
                }
                out.push_back(cmd);
                return;
            }

            out.push_back(cmd);
        };

        for (const auto &cmd : commands_)
        {
            emit(cmd);
        }

        commands_.swap(out);
    }

    void DisplayList::addRect(const ui::Rect &bounds, const ui::Color &fill, const ui::Color &stroke, const ui::Edges& strokeWidths, float cornerRadius)
    {
        emitCommand([&](DrawCommand& cmd) {
            cmd.type = DrawCommandType::Rectangle;
            cmd.bounds = bounds; cmd.fillColor = fill; cmd.strokeColor = stroke;
            cmd.strokeWidths = strokeWidths; cmd.cornerRadius = cornerRadius;
        });
    }

    void DisplayList::addImage(const std::string &source, const ui::Rect &bounds)
    {
        emitCommand([&](DrawCommand& cmd) {
            cmd.type = DrawCommandType::Image;
            cmd.source = source; cmd.bounds = bounds; cmd.fillColor = ui::Color(1, 1, 1);
        });
    }

    void DisplayList::pushClip(const ui::Rect &bounds)
    {
        emitCommand([&](DrawCommand& cmd) {
            cmd.type = DrawCommandType::PushClip; cmd.bounds = bounds;
        });
    }

    void DisplayList::pushClip(const ui::Rect &bounds, float cornerRadius)
    {
        emitCommand([&](DrawCommand& cmd) {
            cmd.type = DrawCommandType::PushClip; cmd.bounds = bounds; cmd.cornerRadius = cornerRadius;
        });
    }

    void DisplayList::popClip()
    {
        emitCommand([&](DrawCommand& cmd) { cmd.type = DrawCommandType::PopClip; });
    }

    void DisplayList::pushTransform(float dx, float dy)
    {
        emitCommand([&](DrawCommand& cmd) {
            cmd.type = DrawCommandType::PushTransform; cmd.dx = dx; cmd.dy = dy;
        });
    }

    void DisplayList::popTransform()
    {
        emitCommand([&](DrawCommand& cmd) { cmd.type = DrawCommandType::PopTransform; });
    }

    void DisplayList::addLayer(std::shared_ptr<DisplayList> layer)
    {
        emitCommand([&](DrawCommand& cmd) {
            cmd.type = DrawCommandType::SubList; cmd.layer = std::move(layer);
        });
    }

    const std::vector<DrawCommand> &DisplayList::getCommands() const { return commands_; }

    size_t DisplayList::size() const { return commands_.size(); }

    void DisplayList::collectStats(RenderStats& stats) const {
        for (const auto& cmd : commands_) {
            switch (cmd.type) {
            case DrawCommandType::Rectangle:
                stats.rectCommands++;
                stats.totalCommands++;
                break;
            case DrawCommandType::Text:
                stats.textCommands++;
                stats.totalCommands++;
                break;
            case DrawCommandType::TextLayout:
                stats.textLayoutCommands++;
                stats.totalCommands++;
                break;
            case DrawCommandType::Image:
                stats.imageCommands++;
                stats.totalCommands++;
                break;
            case DrawCommandType::PushClip:
                stats.pushClipCommands++;
                break;
            case DrawCommandType::PopClip:
                stats.popClipCommands++;
                break;
            case DrawCommandType::PushTransform:
                // not counted separately
                break;
            case DrawCommandType::PopTransform:
                break;
            case DrawCommandType::SubList:
                stats.subLists++;
                if (cmd.layer) {
                    cmd.layer->collectStats(stats);
                }
                break;
            }
        }
    }

    void DisplayList::addText(const std::string &text,
                              const ui::Rect &bounds,
                              const ui::Color &color,
                              float fontSize,
                              const std::string &fontFamily,
                              const TextLayoutParams &layout)
    {
        DrawCommand cmd;
        cmd.type = DrawCommandType::Text;
        cmd.text = text;
        cmd.bounds = bounds;
        cmd.fillColor = color;

        // 如果没有指定字体，使用 ResourceManager 的默认值
        if (fontFamily.empty())
        {
            cmd.fontFamily = ResourceManager::getInstance().getDefaultFontName();
        }
        else
        {
            cmd.fontFamily = fontFamily;
        }

        if (fontSize <= 0.0f)
        {
            cmd.fontSize = ResourceManager::getInstance().getDefaultFontSize();
        }
        else
        {
            cmd.fontSize = fontSize;
        }

        // 使用调用方传入的布局参数
        cmd.textAlign = layout.textAlign.empty() ? std::string("left") : layout.textAlign;
        cmd.lineHeight = layout.lineHeight;
        cmd.wrap = layout.wrap;

        commands_.push_back(std::move(cmd));
    }

    void DisplayList::addTextLayout(const std::shared_ptr<ITextLayout> &layout, const ui::Rect &bounds, const ui::Color &color)
    {
        emitCommand([&](DrawCommand& cmd) {
            cmd.type = DrawCommandType::TextLayout;
            cmd.layout = layout; cmd.bounds = bounds; cmd.fillColor = color;
        });
    }

} // namespace ldt

namespace ldt
{

    static std::string escapeString(const std::string &s)
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s)
        {
            if (c == '\\')
                out += "\\\\";
            else if (c == '|')
                out += "\\p";
            else if (c == '\n')
                out += "\\n";
            else
                out += c;
        }
        return out;
    }

    static std::string unescapeString(const std::string &s)
    {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i)
        {
            char c = s[i];
            if (c == '\\' && i + 1 < s.size())
            {
                char n = s[i + 1];
                if (n == '\\')
                {
                    out += '\\';
                    ++i;
                }
                else if (n == 'p')
                {
                    out += '|';
                    ++i;
                }
                else if (n == 'n')
                {
                    out += '\n';
                    ++i;
                }
                else
                {
                    out += n;
                    ++i;
                }
            }
            else
            {
                out += c;
            }
        }
        return out;
    }

    static std::string floatListToStr(float a, float b, float c, float d)
    {
        std::ostringstream ss;
        ss.precision(6);
        ss << a << ',' << b << ',' << c << ',' << d;
        return ss.str();
    }

    static bool parseFloatList(const std::string &s, float &a, float &b, float &c, float &d)
    {
        std::replace(const_cast<std::string &>(s).begin(), const_cast<std::string &>(s).end(), ',', ' ');
        std::istringstream iss(s);
        if (!(iss >> a >> b >> c >> d))
            return false;
        return true;
    }

    std::string DisplayList::serializeToText() const
    {
        std::ostringstream out;
        for (const auto &cmd : getCommands())
        {
            switch (cmd.type)
            {
            case DrawCommandType::Rectangle:
                out << "Rectangle|bounds=" << floatListToStr(cmd.bounds.x, cmd.bounds.y, cmd.bounds.width, cmd.bounds.height)
                    << "|fill=" << floatListToStr(cmd.fillColor.r, cmd.fillColor.g, cmd.fillColor.b, cmd.fillColor.a)
                    << "|stroke=" << floatListToStr(cmd.strokeColor.r, cmd.strokeColor.g, cmd.strokeColor.b, cmd.strokeColor.a)
                    << "|strokeWidth=" << cmd.strokeWidths.top << "," << cmd.strokeWidths.right << "," << cmd.strokeWidths.bottom << "," << cmd.strokeWidths.left
                    << "|cornerRadius=" << cmd.cornerRadius
                    << '\n';
                break;
            case DrawCommandType::Text:
                out << "Text|bounds=" << floatListToStr(cmd.bounds.x, cmd.bounds.y, cmd.bounds.width, cmd.bounds.height)
                    << "|text=" << escapeString(cmd.text)
                    << "|fontFamily=" << escapeString(cmd.fontFamily)
                    << "|fontSize=" << cmd.fontSize
                    << "|fill=" << floatListToStr(cmd.fillColor.r, cmd.fillColor.g, cmd.fillColor.b, cmd.fillColor.a)
                    << "|textAlign=" << escapeString(cmd.textAlign)
                    << "|lineHeight=" << cmd.lineHeight
                    << "|wrap=" << (cmd.wrap ? "1" : "0")
                    << '\n';
                break;
            case DrawCommandType::TextLayout:
                out << "TextLayout|bounds=" << floatListToStr(cmd.bounds.x, cmd.bounds.y, cmd.bounds.width, cmd.bounds.height)
                    << "|note=SKIPPED" << '\n';
                break;
            case DrawCommandType::Image:
                out << "Image|bounds=" << floatListToStr(cmd.bounds.x, cmd.bounds.y, cmd.bounds.width, cmd.bounds.height)
                    << "|source=" << escapeString(cmd.source)
                    << '\n';
                break;
            case DrawCommandType::PushClip:
                out << "PushClip|bounds=" << floatListToStr(cmd.bounds.x, cmd.bounds.y, cmd.bounds.width, cmd.bounds.height);
                if (cmd.cornerRadius > 0.0f) {
                    out << "|cornerRadius=" << cmd.cornerRadius;
                }
                out << '\n';
                break;
            case DrawCommandType::PopClip:
                out << "PopClip" << '\n';
                break;
            case DrawCommandType::PushTransform:
                out << "PushTransform|dx=" << cmd.dx << "|dy=" << cmd.dy << '\n';
                break;
            case DrawCommandType::PopTransform:
                out << "PopTransform" << '\n';
                break;
            case DrawCommandType::SubList:
                out << "SubList_BEGIN" << '\n';
                if (cmd.layer)
                {
                    out << cmd.layer->serializeToText();
                }
                out << "SubList_END" << '\n';
                break;
            default:
                out << "Unknown|note=SKIPPED" << '\n';
                break;
            }
        }
        return out.str();
    }

    DisplayList DisplayList::deserializeFromText(const std::string &text)
    {
        DisplayList root;
        std::istringstream iss(text);
        std::string line;
        std::vector<DisplayList *> stack;
        stack.push_back(&root);

        while (std::getline(iss, line))
        {
            if (line.empty())
                continue;
            if (line == "SubList_BEGIN")
            {
                // create a new layer and add as SubList command to current
                auto layer = std::make_shared<DisplayList>();
                stack.back()->addLayer(layer);
                stack.push_back(layer.get());
                continue;
            }
            if (line == "SubList_END")
            {
                if (stack.size() > 1)
                    stack.pop_back();
                continue;
            }

            // split by '|'
            std::vector<std::string> parts;
            {
                std::string tmp;
                std::istringstream ls(line);
                while (std::getline(ls, tmp, '|'))
                    parts.push_back(tmp);
            }
            if (parts.empty())
                continue;
            const std::string &type = parts[0];

            auto getKV = [&](const std::string &key) -> std::string
            {
                for (size_t i = 1; i < parts.size(); ++i)
                {
                    size_t pos = parts[i].find('=');
                    if (pos != std::string::npos)
                    {
                        std::string k = parts[i].substr(0, pos);
                        if (k == key)
                            return parts[i].substr(pos + 1);
                    }
                }
                return std::string();
            };

            if (type == "Rectangle")
            {
                std::string b = getKV("bounds");
                float x, y, w, h;
                parseFloatList(b, x, y, w, h);
                std::string f = getKV("fill");
                float fr, fg, fb, fa;
                parseFloatList(f, fr, fg, fb, fa);
                std::string s = getKV("stroke");
                float sr, sg, sb, sa;
                parseFloatList(s, sr, sg, sb, sa);
                std::string sw = getKV("strokeWidth");
                float strokeW = sw.empty() ? 0.0f : std::stof(sw);
                std::string cr = getKV("cornerRadius");
                float corner = cr.empty() ? 0.0f : std::stof(cr);
                stack.back()->addRect(ui::Rect(x, y, w, h), ui::Color(fr, fg, fb, fa), ui::Color(sr, sg, sb, sa), strokeW, corner);
            }
            else if (type == "Text")
            {
                std::string b = getKV("bounds");
                float x, y, w, h;
                parseFloatList(b, x, y, w, h);
                std::string textv = unescapeString(getKV("text"));
                std::string font = unescapeString(getKV("fontFamily"));
                std::string fs = getKV("fontSize");
                float fontSize = fs.empty() ? 0.0f : std::stof(fs);
                std::string f = getKV("fill");
                float fr, fg, fb, fa;
                parseFloatList(f, fr, fg, fb, fa);
                TextLayoutParams params;
                params.textAlign = unescapeString(getKV("textAlign"));
                std::string lh = getKV("lineHeight");
                if (!lh.empty())
                    params.lineHeight = std::stof(lh);
                std::string wv = getKV("wrap");
                params.wrap = (wv == "1");
                stack.back()->addText(textv, ui::Rect(x, y, w, h), ui::Color(fr, fg, fb, fa), fontSize, font, params);
            }
            else if (type == "TextLayout")
            {
                // skip
            }
            else if (type == "Image")
            {
                std::string b = getKV("bounds");
                float x, y, w, h;
                parseFloatList(b, x, y, w, h);
                std::string src = unescapeString(getKV("source"));
                stack.back()->addImage(src, ui::Rect(x, y, w, h));
            }
            else if (type == "PushClip")
            {
                std::string b = getKV("bounds");
                float x, y, w, h;
                parseFloatList(b, x, y, w, h);
                std::string cr = getKV("cornerRadius");
                float cornerRadius = cr.empty() ? 0.0f : std::stof(cr);
                if (cornerRadius > 0.0f) {
                    stack.back()->pushClip(ui::Rect(x, y, w, h), cornerRadius);
                } else {
                    stack.back()->pushClip(ui::Rect(x, y, w, h));
                }
            }
            else if (type == "PopClip")
            {
                stack.back()->popClip();
            }
            else if (type == "PushTransform")
            {
                std::string dxs = getKV("dx");
                std::string dys = getKV("dy");
                float dx = dxs.empty() ? 0.0f : std::stof(dxs);
                float dy = dys.empty() ? 0.0f : std::stof(dys);
                stack.back()->pushTransform(dx, dy);
            }
            else if (type == "PopTransform")
            {
                stack.back()->popTransform();
            }
            else
            {
                // unknown/unsupported: ignore
            }
        }

        return root;
    }

} // namespace ldt
