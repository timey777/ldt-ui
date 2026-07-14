#include "update_scheduler.h"
#include "engine/core/resolved_node.h"
#include <algorithm>

#include "misc/ui_diagnostics.h"

UpdateScheduler& UpdateScheduler::getInstance() {
    static UpdateScheduler instance;
    return instance;
}

void UpdateScheduler::requestUpdate(ldt::ResolvedNode* node) {
    if (!node) return;
    m_dirtyNodesSet.insert(node);
    ldt::diagnostics::logQueuedNodeUpdate("layout_scheduler.requestUpdate", node);
}

void UpdateScheduler::notifyNodeRemoved(ldt::ResolvedNode* node) {
    if (!node) return;
    m_dirtyNodesSet.erase(node);
    m_dirtyNodesVec.erase(std::remove(m_dirtyNodesVec.begin(), m_dirtyNodesVec.end(), node), m_dirtyNodesVec.end());
}

void UpdateScheduler::requestPaint() {
    m_hasPaintRequest = true;
}

void UpdateScheduler::requestRepaint() {
    m_hasRepaintRequest = true;
}

void UpdateScheduler::requestASTRepaint() {
    m_hasASTRepaintRequest = true;
}

DocumentUpdatePlan UpdateScheduler::consumePendingPlan() {
    FrameAction action = analyzeFrameAction();
    m_dirtyNodesVec.assign(m_dirtyNodesSet.begin(), m_dirtyNodesSet.end());

    DocumentUpdatePlan plan;
    switch (action) {
        case FrameAction::ASTRepaint:
            plan.kind = DocumentUpdateKind::ASTRepaint;
            break;
        case FrameAction::FullLayout:
        case FrameAction::Repaint:
            plan.kind = DocumentUpdateKind::Repaint;
            break;
        case FrameAction::PartialUpdate:
            plan.kind = DocumentUpdateKind::PartialUpdate;
            plan.dirtyNodes = m_dirtyNodesVec;
            break;
        case FrameAction::PaintOnly:
            plan.kind = DocumentUpdateKind::PaintOnly;
            break;
        case FrameAction::None:
            plan.kind = DocumentUpdateKind::None;
            break;
    }

    const char* actionName = "Unknown";
    switch (action) {
        case FrameAction::None:       actionName = "None"; break;
        case FrameAction::PaintOnly:  actionName = "PaintOnly"; break;
        case FrameAction::PartialUpdate: actionName = "PartialUpdate"; break;
        case FrameAction::Repaint:    actionName = "Repaint"; break;
        case FrameAction::FullLayout: actionName = "FullLayout"; break;
        case FrameAction::ASTRepaint: actionName = "ASTRepaint"; break;
    }
    ldt::diagnostics::logSchedulerDecision(
        "layout_scheduler.consumePendingPlan",
        actionName,
        m_dirtyNodesVec,
        m_hasPaintRequest,
        m_hasRepaintRequest,
        m_hasASTRepaintRequest);

    clear();
    return plan;
}

UpdateScheduler::FrameAction UpdateScheduler::analyzeFrameAction() {
    if (m_hasASTRepaintRequest) {
        return FrameAction::ASTRepaint;
    }

    if (!m_dirtyNodesSet.empty()) {
        bool needsFullLayout = false;
        for (auto* node : m_dirtyNodesSet) {
            if (node->isDirty(ldt::DirtyFlag::Layout)) {
                needsFullLayout = true;
                break;
            }
        }
        return needsFullLayout ? FrameAction::FullLayout : FrameAction::PartialUpdate;
    }

    if (m_hasRepaintRequest) {
        return FrameAction::Repaint;
    }
    
    return m_hasPaintRequest ? FrameAction::PaintOnly : FrameAction::None;
}

const std::vector<ldt::ResolvedNode*>& UpdateScheduler::getDirtyNodes() const {
    // This method is now internal helper, or handled directly inside executePendingTasks for PartialUpdate
    // But we keep it to satisfy header declaration if needed, though header made it private.
    // However, since it's private and only used inside this class, we can just access members directly.
    return m_dirtyNodesVec; 
}

void UpdateScheduler::clear() {
    m_dirtyNodesSet.clear();
    m_dirtyNodesVec.clear();
    m_hasPaintRequest = false;
    m_hasRepaintRequest = false;
    m_hasASTRepaintRequest = false;
}
