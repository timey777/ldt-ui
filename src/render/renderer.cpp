#include "renderer.h"
#include "drawer.h"
#include "engine/resource_manager.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <functional>

namespace ldt {

Renderer::Renderer(IDrawer* drawer) : drawer_(drawer) {
    // typical nested clip depth is small; reserving avoids growth allocations
    clipStateStack_.reserve(64);
}

Renderer::~Renderer() {
    clearImageCache();
}

void Renderer::setDrawer(IDrawer* drawer) { drawer_ = drawer; }

IDrawer* Renderer::getDrawer() const { return drawer_; }

void Renderer::render(const DisplayList& displayList) {
    if (!drawer_) return;

    const auto& commands = displayList.getCommands();

    // 简化：仅保留绘制核心逻辑——逐个命令直接渲染。
    // 移除延迟文本、子列表边界推断、复杂裁剪缓冲等逻辑以定位绘制问题。
    for (const auto& cmd : commands) {
        renderCommand(cmd);
    }
}

void Renderer::render2(const DisplayList& displayList) {
    if (!drawer_) return;

    const auto& commands = displayList.getCommands();

    auto rect_intersects = [](const ui::Rect& a, const ui::Rect& b) {
        const float ax2 = a.x + a.width;
        const float ay2 = a.y + a.height;
        const float bx2 = b.x + b.width;
        const float by2 = b.y + b.height;
        return (a.x < bx2) && (ax2 > b.x) && (a.y < by2) && (ay2 > b.y);
    };

    auto is_barrier = [](DrawCommandType t) {
        return t == DrawCommandType::PushClip || t == DrawCommandType::PopClip ||
               t == DrawCommandType::PushTransform || t == DrawCommandType::PopTransform;
    };

    auto is_deferrable_text = [](DrawCommandType t) {
        return t == DrawCommandType::Text || t == DrawCommandType::TextLayout;
    };

    std::vector<const DrawCommand*> deferred;
    deferred.reserve(64);

    auto flush_deferred = [&]() {
        for (const auto* d : deferred) {
            renderCommand(*d);
        }
        deferred.clear();
    };

    std::function<bool(const std::shared_ptr<DisplayList>&, ui::Rect*)> try_compute_sublist_bounds_simple;
    try_compute_sublist_bounds_simple = [&](const std::shared_ptr<DisplayList>& layer, ui::Rect* outBounds) -> bool {
        if (!layer || !outBounds) return false;
        const auto& cmds = layer->getCommands();
        bool hasAny = false;
        ui::Rect acc;
        for (const auto& c : cmds) {
            // If the sublist contains stateful commands, don't try to reason about it.
            if (c.type == DrawCommandType::PushClip || c.type == DrawCommandType::PopClip ||
                c.type == DrawCommandType::PushTransform || c.type == DrawCommandType::PopTransform) {
                return false;
            }
            if (c.type == DrawCommandType::SubList) {
                ui::Rect child;
                if (!try_compute_sublist_bounds_simple(c.layer, &child)) return false;
                if (!hasAny) {
                    acc = child;
                    hasAny = true;
                } else {
                    float x1 = std::min(acc.x, child.x);
                    float y1 = std::min(acc.y, child.y);
                    float x2 = std::max(acc.x + acc.width, child.x + child.width);
                    float y2 = std::max(acc.y + acc.height, child.y + child.height);
                    acc.x = x1;
                    acc.y = y1;
                    acc.width = x2 - x1;
                    acc.height = y2 - y1;
                }
                continue;
            }

            // Draw commands: use their bounds
            if (c.type == DrawCommandType::Rectangle || c.type == DrawCommandType::Text ||
                c.type == DrawCommandType::TextLayout || c.type == DrawCommandType::Image ||
                c.type == DrawCommandType::Line || c.type == DrawCommandType::Circle) {
                if (!hasAny) {
                    acc = c.bounds;
                    hasAny = true;
                } else {
                    float x1 = std::min(acc.x, c.bounds.x);
                    float y1 = std::min(acc.y, c.bounds.y);
                    float x2 = std::max(acc.x + acc.width, c.bounds.x + c.bounds.width);
                    float y2 = std::max(acc.y + acc.height, c.bounds.y + c.bounds.height);
                    acc.x = x1;
                    acc.y = y1;
                    acc.width = x2 - x1;
                    acc.height = y2 - y1;
                }
            }
        }
        if (!hasAny) return false;
        *outBounds = acc;
        return true;
    };

    auto is_text_only_clip_scope = [&](size_t startIndex, size_t* outEndIndex) -> bool {
        if (!outEndIndex) return false;
        if (startIndex >= commands.size()) return false;
        if (commands[startIndex].type != DrawCommandType::PushClip) return false;

        int depth = 0;
        for (size_t j = startIndex; j < commands.size(); ++j) {
            const auto t = commands[j].type;
            if (t == DrawCommandType::PushClip) {
                ++depth;
                continue;
            }
            if (t == DrawCommandType::PopClip) {
                --depth;
                if (depth == 0) {
                    *outEndIndex = j;
                    return true;
                }
                if (depth < 0) return false;
                continue;
            }

            if (t == DrawCommandType::Text || t == DrawCommandType::TextLayout) {
                continue;
            }

            // Any non-text draw/state inside the clip scope makes it unsafe to defer.
            return false;
        }
        return false;
    };

    for (size_t i = 0; i < commands.size(); ++i) {
        const auto& cmd = commands[i];

        if (cmd.type == DrawCommandType::PushClip) {
            size_t endIndex = 0;
            if (is_text_only_clip_scope(i, &endIndex)) {
                // Defer the entire clip scope; it only affects text, and deferring avoids
                // unnecessary clip state oscillation that can split image batches.
                for (size_t j = i; j <= endIndex; ++j) {
                    deferred.push_back(&commands[j]);
                }
                i = endIndex;
                continue;
            }
        }

        if (is_barrier(cmd.type)) {
            flush_deferred();

            renderCommand(cmd);
            continue;
        }

        if (cmd.type == DrawCommandType::SubList) {
            // Treat sublist as a draw-like command: only flush deferred text if it would overlap.
            ui::Rect subBounds;
            bool haveBounds = try_compute_sublist_bounds_simple(cmd.layer, &subBounds);
            if (haveBounds) {
                bool overlap = false;
                for (const auto* d : deferred) {
                    if (is_deferrable_text(d->type) && rect_intersects(d->bounds, subBounds)) {
                        overlap = true;
                        break;
                    }
                }
                if (overlap) flush_deferred();
            } else {
                // Unknown bounds; be conservative.
                flush_deferred();
            }
            renderCommand(cmd);
            continue;
        }

        if (is_deferrable_text(cmd.type)) {
            deferred.push_back(&cmd);
            continue;
        }

        // For draw commands (e.g. Image/Rect): if any deferred text overlaps, flush before.
        bool overlap = false;
        for (const auto* d : deferred) {
            if (is_deferrable_text(d->type) && rect_intersects(d->bounds, cmd.bounds)) {
                overlap = true;
                break;
            }
        }
        if (overlap) flush_deferred();
        renderCommand(cmd);
    }

    flush_deferred();
}

void Renderer::render(const DisplayList& displayList, const ui::Rect& target) {
    if (!drawer_) return;

    // 保存并设置裁剪到目标区域
    render::Rect prevClip;
    bool hadClip = drawer_->getClipRect(&prevClip);
    float prevClipCornerRadius = drawer_->getClipCornerRadius();

    render::Rect targetClip;
    targetClip.x = target.x;
    targetClip.y = target.y;
    targetClip.w = target.width;
    targetClip.h = target.height;
    drawer_->intersectClipRect(targetClip, nullptr);

    // 将坐标系平移到目标起点，渲染时命令可按目标内坐标绘制
    drawer_->pushTransform(target.x, target.y);

    // 复用通用的渲染逻辑
    render(displayList);

    drawer_->popTransform();

    // 恢复之前的裁剪
    if (hadClip) {
        drawer_->setClipRect(prevClip);
    } else {
        drawer_->resetClip();
    }
    drawer_->setClipCornerRadius(prevClipCornerRadius);
}


void Renderer::clearImageCache() {
    if (!drawer_) return;

    for (auto& pair : imageCache_) {
        drawer_->unloadImage(pair.second);
    }
    imageCache_.clear();
}

render::Color Renderer::convertColor(const ui::Color& color) {
    render::Color result;
    result.r = color.r;
    result.g = color.g;
    result.b = color.b;
    result.a = color.a;
    return result;
}

void Renderer::renderCommand(const DrawCommand& cmd) {
	switch (cmd.type) {
        case DrawCommandType::Rectangle:
            renderRect(cmd);
            break;

        case DrawCommandType::Text:
            renderText(cmd);
            break;

        case DrawCommandType::TextLayout:
            renderTextLayout(cmd);
            break;

        case DrawCommandType::Image:
            renderImage(cmd);
            break;

        case DrawCommandType::PushClip:
			renderPushClip(cmd);
            break;

        case DrawCommandType::PopClip:
			renderPopClip();
            break;

        case DrawCommandType::PushTransform:
			renderPushTransform(cmd);
            break;

        case DrawCommandType::PopTransform:
			renderPopTransform();
            break;

        case DrawCommandType::SubList:
            {
				if (cmd.layer) {
					render(*cmd.layer);
                }
            }
            break;

        default:
            break;
    }
}

void Renderer::renderPushTransform(const DrawCommand& cmd) {
	if (!drawer_) return;
	drawer_->pushTransform(cmd.dx, cmd.dy);
}

void Renderer::renderPopTransform() {
	if (!drawer_) return;
	drawer_->popTransform();
}

void Renderer::renderPushClip(const DrawCommand& cmd) {
	if (!drawer_) return;

    ClipState state;
    state.hasClip = drawer_->getClipRect(&state.rect);
    state.cornerRadius = drawer_->getClipCornerRadius();
    clipStateStack_.push_back(state);
    
    render::Rect newClip;
    newClip.x = cmd.bounds.x;
    newClip.y = cmd.bounds.y;
    newClip.w = cmd.bounds.width;
    newClip.h = cmd.bounds.height;
    
    drawer_->intersectClipRect(newClip, nullptr);
    drawer_->setClipCornerRadius(cmd.cornerRadius);
}

void Renderer::renderPopClip() {
	if (!drawer_ || clipStateStack_.empty()) return;

    ClipState prevState = clipStateStack_.back();
    clipStateStack_.pop_back();

    if (!prevState.hasClip) {
        drawer_->resetClip();
    } else {
        drawer_->setClipRect(prevState.rect);
    }
    drawer_->setClipCornerRadius(prevState.cornerRadius);
}

void Renderer::renderRect(const DrawCommand& cmd) {
	if (!drawer_) return;

    DrawStyle style;
    style.fillColor = convertColor(cmd.fillColor);
    style.strokeColor = convertColor(cmd.strokeColor);
    style.strokeWidths.top = cmd.strokeWidths.top;
    style.strokeWidths.right = cmd.strokeWidths.right;
    style.strokeWidths.bottom = cmd.strokeWidths.bottom;
    style.strokeWidths.left = cmd.strokeWidths.left;

	if (cmd.cornerRadius > 0) {
        drawer_->drawRoundedRect(
            cmd.bounds.x,
            cmd.bounds.y,
            cmd.bounds.width,
            cmd.bounds.height,
            cmd.cornerRadius,
            cmd.cornerRadius,
            style
        );
    } else {
        drawer_->drawRect(
            cmd.bounds.x,
            cmd.bounds.y,
            cmd.bounds.width,
            cmd.bounds.height,
            style
        );
    }
}

void Renderer::renderText(const DrawCommand& cmd) {
	if (!drawer_) return;

    Font font;
    
    // 如果字体为空或大小为0，使用默认值
    if (cmd.fontFamily.empty()) {
        font.family = ResourceManager::getInstance().getDefaultFontName();
    } else {
        font.family = cmd.fontFamily;
    }
    
    if (cmd.fontSize <= 0.0f) {
        font.size = ResourceManager::getInstance().getDefaultFontSize();
    } else {
        font.size = cmd.fontSize;
    }
    
    font.bold = cmd.bold;
    font.italic = cmd.italic;

    DrawStyle style;
    style.fillColor = convertColor(cmd.fillColor);

    // 计算 maxWidth：只有在需要换行的块级文本时才应为正数。
    float maxWidth = (cmd.bounds.width > 0.0f ? cmd.bounds.width : -1.0f);

    TextLayoutParams layout;
    layout.textAlign = cmd.textAlign;
    layout.lineHeight = cmd.lineHeight;
    layout.wrap = cmd.wrap;

    // respect wrap flag: if wrap==false, do not set maxWidth
    //if (!layout.wrap) maxWidth = -1.0f;

    drawer_->drawText(
        cmd.text,
        cmd.bounds.x,
        cmd.bounds.y,
        font,
        style,
        maxWidth,
        layout
    );
}

void Renderer::renderTextLayout(const DrawCommand& cmd) {
	if (!drawer_ || !cmd.layout) return;

    DrawStyle style;
    style.fillColor = convertColor(cmd.fillColor);

    drawer_->drawTextLayout(
        cmd.layout,
        cmd.bounds.x,
        cmd.bounds.y,
        style
    );
}

void Renderer::renderImage(const DrawCommand& cmd) {
	if (!drawer_) return;

	ImageHandle imgHandle = getOrLoadImage(cmd.source);
    if (imgHandle == 0) return;

    DrawStyle style;
    style.fillColor = convertColor(cmd.fillColor);
    drawer_->drawImage(
        imgHandle,
        cmd.bounds.x,
        cmd.bounds.y,
        cmd.bounds.width,
        cmd.bounds.height,
        style
    );
}

ImageHandle Renderer::getOrLoadImage(const std::string& path) {
    if (!drawer_ || path.empty()) return 0;

    // 检查缓存
    auto it = imageCache_.find(path);
    if (it != imageCache_.end()) {
        return it->second;
    }

    // 渲染器不再主动触发加载，只负责查询
    // 如果图片未加载，返回 0 (不绘制)
    return 0;
}

void Renderer::updateImageLoadQueue() {
    // 从 ResourceManager 获取已加载完成的图片数据
    while (auto res = ResourceManager::getInstance().popReadyImage()) {
        if (drawer_) {
            ImageHandle handle = drawer_->createImageFromMemory(res->data, res->width, res->height, res->channels);
            if (handle != 0) {
                imageCache_[res->path] = handle;
            }
        }
        // ImageRawData 析构时会自动释放 data
    }
}

} // namespace ldt
