#include "ui_event_system.h"
#include "update_scheduler.h"
#include "components/abstract_control.h"
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>
#include "engine/core/resolved_node.h"
#include "misc/logger.h"
namespace ldt {

// Simple selector parser (#id or .class)
UIEventSystem::Selector UIEventSystem::parseSelector(const std::string& s) {
    Selector sel;
    if (s.empty()) return sel;
    if (s[0] == '#') {
        sel.type = Selector::Type::Id;
        sel.value = s.substr(1);
    } else if (s[0] == '.') {
        sel.type = Selector::Type::Class;
        sel.value = s.substr(1);
    } else {
        // default to id if no prefix
        sel.type = Selector::Type::Id;
        sel.value = s;
    }
    return sel;
}

void UIEventSystem::bind(const std::string& selector, UIEventType type, EventCallback cb) {
    Selector s = parseSelector(selector);
    Handler h{ s, type, std::move(cb) };
    if (dispatchDepth_ > 0) {
        pendingHandlers_.push_back(std::move(h));
        return;
    }
    handlers_.push_back(std::move(h));
}

static bool matchSelector(const UIEventSystem::Selector& s, const AbstractControl* c) {
    if (!c) return false;
    if (s.type == UIEventSystem::Selector::Type::Id) {
        return c->getId() == s.value;
    }
    // class
    return c->hasClass(s.value);
}

void UIEventSystem::dispatch(const UIEvent& e) {
    ++dispatchDepth_;
    for (const auto& h : handlers_) {
        if (h.type != e.type) continue;
        if (!e.target) continue;
        if (!matchSelector(h.selector, e.target)) continue;
        try {
            h.cb(e);
        } catch(...) {}
        if (e.stopPropagation) break;
    }
    --dispatchDepth_;
    if (dispatchDepth_ == 0) {
        flushPendingHandlers();
    }
}

void UIEventSystem::flushPendingHandlers() {
    if (pendingHandlers_.empty()) return;
    handlers_.insert(handlers_.end(),
        std::make_move_iterator(pendingHandlers_.begin()),
        std::make_move_iterator(pendingHandlers_.end()));
    pendingHandlers_.clear();
}

static std::vector<ResolvedNode*> buildPath(ResolvedNode* node) {
    std::vector<ResolvedNode*> path;
    while (node) {
        path.push_back(node);
        node = node->parent;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

void UIEventSystem::updateHover(ResolvedNode* newHover) {
    // 构建新旧路径
    auto newPath = buildPath(newHover);
    
    // 1. 移除旧路径中不再 hover 的节点
    for (auto* node : hoverPath_) {
        if (std::find(newPath.begin(), newPath.end(), node) == newPath.end()) {
            node->removeUIState(UIState::Hover);
            UpdateScheduler::getInstance().requestUpdate(node);

            if (auto ctrl = node->getControl().lock()) {
                ctrl->onHover(false);
            }
        }
    }

    // 2. 添加新路径中的 hover
    for (auto* node : newPath) {
        if (std::find(hoverPath_.begin(), hoverPath_.end(), node) == hoverPath_.end()) {
            // Check if node is disabled - if so, don't apply hover state
            bool disabled = false;
            // It's possible node has Disabled state already? Or check control
            if (auto ctrl = node->getControl().lock()) {
                if (!ctrl->isEnabled()) disabled = true;
            }
            // Also check node flags directly if control is missing but flags are set (unlikely)
            if (node->hasUIState(UIState::Disabled)) disabled = true;

            if (!disabled) {
                node->addUIState(UIState::Hover);
                UpdateScheduler::getInstance().requestUpdate(node);

                if (auto ctrl = node->getControl().lock()) {
                    ctrl->onHover(true);
                }
            }
        }
    }

    hoverPath_ = std::move(newPath);
}

void UIEventSystem::updateActive(ResolvedNode* newActive) {
    // 构建新旧路径
    auto newPath = buildPath(newActive);

    // 移除旧路径中不再 active 的节点
    for (auto* node : activePath_) {
        if (std::find(newPath.begin(), newPath.end(), node) == newPath.end()) {
            node->removeUIState(UIState::Active);
            UpdateScheduler::getInstance().requestUpdate(node);
            if (auto ctrl = node->getControl().lock()) {
                ctrl->onActive(false);
            }
        }
    }

    // 添加新路径中的 active
    for (auto* node : newPath) {
        if (std::find(activePath_.begin(), activePath_.end(), node) == activePath_.end()) {
            bool disabled = false;
            if (auto ctrl = node->getControl().lock()) {
                if (!ctrl->isEnabled()) disabled = true;
            }
            if (node->hasUIState(UIState::Disabled)) disabled = true;

            if (!disabled) {
                node->addUIState(UIState::Active);
                UpdateScheduler::getInstance().requestUpdate(node);
                if (auto ctrl = node->getControl().lock()) {
                    ctrl->onActive(true);
                }
            }
        }
    }

    activePath_ = std::move(newPath);
}

void UIEventSystem::updateFocus(ResolvedNode* newFocus) {
    if (focusNode_ == newFocus) return;

    if (newFocus) {
        if (auto ctrl = newFocus->getControl().lock()) {
            if (!ctrl->isEnabled()) return;
        }
        if (newFocus->hasUIState(UIState::Disabled)) return;
    }

    if (focusNode_) {
        // Remove Focus flag
        focusNode_->removeUIState(UIState::Focus);
        UpdateScheduler::getInstance().requestUpdate(focusNode_);
        if (auto ctrl = focusNode_->getControl().lock()) {
            ctrl->onBlur();
        }
    }

    focusNode_ = newFocus;

    if (focusNode_) {
        // Add Focus flag
        focusNode_->addUIState(UIState::Focus);
        UpdateScheduler::getInstance().requestUpdate(focusNode_);
        if (auto ctrl = focusNode_->getControl().lock()) {
            ctrl->onFocus();
        }
    }
}

ResolvedNode* UIEventSystem::getFocusNode() {
    return focusNode_;
}

void UIEventSystem::notifyNodeRemoved(ResolvedNode* node) {
    if (!node) return;

    if (std::find(hoverPath_.begin(), hoverPath_.end(), node) != hoverPath_.end()) {
        node->removeUIState(UIState::Hover);
        if (auto ctrl = node->getControl().lock()) {
            ctrl->onHover(false);
        }
    }

    if (std::find(activePath_.begin(), activePath_.end(), node) != activePath_.end()) {
        node->removeUIState(UIState::Active);
        if (auto ctrl = node->getControl().lock()) {
            ctrl->onActive(false);
        }
    }

    if (focusNode_ == node) {
        node->removeUIState(UIState::Focus);
        if (auto ctrl = node->getControl().lock()) {
            ctrl->onBlur();
        }
        focusNode_ = nullptr;
    }

    hoverPath_.erase(std::remove(hoverPath_.begin(), hoverPath_.end(), node), hoverPath_.end());
    activePath_.erase(std::remove(activePath_.begin(), activePath_.end(), node), activePath_.end());
}

ResolvedNode* UIEventSystem::getHoverNode() const {
    return hoverPath_.empty() ? nullptr : hoverPath_.back();
}


} // namespace ldt
