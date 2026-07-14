#include "resolved_runtime.h"

#include "container_control.h"
#include "control_manager.h"
#include "scene.h"
#include "engine/core/resolved_node.h"
#include "engine/core/resolved_tree.h"
#include <algorithm>
#include <engine/update_scheduler.h>

namespace ldt {

ResolvedNode* ResolvedRuntime::findNodeByUid(const std::string& uid) const {
    auto it = uidMap_.find(uid);
    if (it == uidMap_.end()) {
        return nullptr;
    }
    return it->second;
}

void ResolvedRuntime::registerNode(const std::string& uid, ResolvedNode* node) {
    if (uid.empty() || !node) {
        return;
    }
    uidMap_[uid] = node;
}

void ResolvedRuntime::unregisterNode(const std::string& uid) {
    if (uid.empty()) {
        return;
    }
    uidMap_.erase(uid);
}

void ResolvedRuntime::clear() {
    uidMap_.clear();
    pendingRemovalSet_.clear();
    pendingRemovalNodes_.clear();
}

void ResolvedRuntime::scheduleRemovalByUid(const std::string& uid) {
    ResolvedNode* node = findNodeByUid(uid);
    if (!node || node->isDirty(DirtyFlag::Removed)) {
        return;
    }

    node->markDirty(DirtyFlag::Removed);
    if (pendingRemovalSet_.insert(node).second) {
        pendingRemovalNodes_.push_back(node);
    }

    auto recursiveNotify = [&](ResolvedNode* current, auto& self) -> void {
        if (!current) {
            return;
        }

        auto control = current->getControl().lock();
        if (control) {
            if (auto scene = control->getScene()) {
                scene->notifyNodeRemoved(current);
            }
        } else {
            UpdateScheduler::getInstance().notifyNodeRemoved(current);
        }

        for (auto* child : current->getChildren()) {
            self(child, self);
        }
    };
    recursiveNotify(node, recursiveNotify);

    auto control = node->getControl().lock();
    if (!control) {
        return;
    }

    if (auto parent = dynamic_cast<ContainerControl*>(control->getParent())) {
        ContainerControl::InternalAccess::removeChild(parent, control);
        return;
    }

    auto scene = control->getScene();
    if (scene && scene->getControlManager()) {
        scene->getControlManager()->removeRootControl(control);
    }
}

void ResolvedRuntime::executePendingRemovals() {
    if (pendingRemovalNodes_.empty()) {
        return;
    }

    std::vector<ResolvedNode*> toRemove = std::move(pendingRemovalNodes_);
    pendingRemovalNodes_.clear();
    pendingRemovalSet_.clear();

    for (auto* node : toRemove) {
        if (!node) {
            continue;
        }

        auto* parent = node->parent;
        if (parent && parent->isDirty(DirtyFlag::Removed)) {
            unregisterNode(node->getUid());
            continue;
        }

        if (node->astNode) {
            auto parentAst = node->astNode->parent.lock();
            if (parentAst) {
                auto& siblings = parentAst->children;
                siblings.erase(std::remove(siblings.begin(), siblings.end(), node->astNode), siblings.end());
            }
        }

        auto unregisterRecursive = [&](ResolvedNode* current, auto& self) -> void {
            if (!current) {
                return;
            }
            unregisterNode(current->getUid());
            for (auto* child : current->getChildren()) {
                self(child, self);
            }
        };
        unregisterRecursive(node, unregisterRecursive);

        if (parent) {
            parent->markDirty(DirtyFlag::Layout | DirtyFlag::ChildrenLayout);
        }
        node->destory();
    }
}

} // namespace ldt