#include "ui_diagnostics.h"
#include "misc/logger.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "engine/core/ast_node.h"
#include "engine/core/node_flags.h"
#include "engine/core/resolved_node.h"
#include "engine/ui_event_system.h"
#include "misc/logger.h"

namespace ldt::diagnostics {
namespace {

constexpr size_t kMaxLoggedNodes = 6;

std::string layoutUnitToString(const LayoutUnit& unit) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << unit.value;
    switch (unit.unit) {
    case Unit::Px:
        oss << "px";
        break;
    case Unit::Dp:
        oss << "dp";
        break;
    case Unit::Sp:
        oss << "sp";
        break;
    case Unit::Percent:
        oss << "%";
        break;
    case Unit::Auto:
    default:
        return "auto";
    }
    return oss.str();
}

std::string uiStateToString(UIState state) {
    if (state == UIState::None) {
        return "None";
    }

    std::string result;
    auto append = [&](UIState flag, const char* name) {
        if (!hasUIState(state, flag)) {
            return;
        }
        if (!result.empty()) {
            result += "|";
        }
        result += name;
    };

    append(UIState::Hover, "Hover");
    append(UIState::Active, "Active");
    append(UIState::Focus, "Focus");
    append(UIState::Dragging, "Dragging");
    append(UIState::Disabled, "Disabled");
    return result.empty() ? "None" : result;
}

std::string rectToString(const ui::Rect& rect) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2)
        << "(" << rect.x << "," << rect.y << "," << rect.width << "x" << rect.height << ")";
    return oss.str();
}

std::string pointToString(LogicalPointDp point) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2)
        << "(" << point.x.value << "," << point.y.value << ")";
    return oss.str();
}

std::string viewportToString(ViewportSizeDp viewportSize) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2)
        << viewportSize.width.value << "x" << viewportSize.height.value << "dp";
    return oss.str();
}

std::string safeNodeId(const ResolvedNode* node) {
    if (!node || !node->astNode || !node->astNode->hasAttribute("id")) {
        return std::string();
    }

    const Attribute* attr = node->astNode->getAttribute("id");
    if (!attr) {
        return std::string();
    }

    try {
        return attr->as<std::string>();
    } catch (...) {
        LDT_ERROR("ui_diagnostics::safeNodeId: failed to read id attribute");
        return std::string();
    }
}

std::string safeNodeUid(const ResolvedNode* node) {
    if (!node || !node->astNode) {
        return std::string();
    }

    const Attribute* uid = node->astNode->getUid();
    if (!uid) {
        return std::string();
    }

    try {
        return uid->as<std::string>();
    } catch (...) {
        LDT_ERROR("ui_diagnostics::safeNodeUid: failed to read uid attribute");
        return std::string();
    }
}

std::string describeNode(const ResolvedNode* node) {
    if (!node) {
        return "<none>";
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    if (node->astNode && !node->astNode->type.empty()) {
        oss << node->astNode->type;
    } else {
        oss << "Node";
    }

    const std::string id = safeNodeId(node);
    if (!id.empty()) {
        oss << "#" << id;
    }

    const std::string uid = safeNodeUid(node);
    if (!uid.empty()) {
        oss << "(" << uid << ")";
    }

    const ui::Rect border = node->layout.getBorderBox();
    const ui::Rect content = node->layout.getContentBox();
    oss << " state=" << uiStateToString(node->uiState)
        << " dirty=" << dirtyFlagToString(node->getDirtyFlags())
        << " decl=" << layoutUnitToString(node->finalStyle.width)
        << "x" << layoutUnitToString(node->finalStyle.height)
        << " computed=" << node->layout.computedWidth
        << "x" << node->layout.computedHeight
        << " border=" << rectToString(border)
        << " content=" << rectToString(content);
    return oss.str();
}

std::string describeNodeList(const std::vector<ResolvedNode*>& nodes) {
    if (nodes.empty()) {
        return "[]";
    }

    std::ostringstream oss;
    oss << "[";
    const size_t limit = std::min(nodes.size(), kMaxLoggedNodes);
    for (size_t index = 0; index < limit; ++index) {
        if (index > 0) {
            oss << " | ";
        }
        oss << describeNode(nodes[index]);
    }
    if (nodes.size() > limit) {
        oss << " | ... +" << (nodes.size() - limit) << " more";
    }
    oss << "]";
    return oss.str();
}

void emit(LogCategory cat, LogLevel level, const std::string& message) {
    if (!Logger::shouldLog(level)) return;
    Logger::write(cat, level, LogColor::DEFAULT, "[Diag] " + message, std::cout);
}

} // namespace

void logInputMapping(const char* source,
                     const char* action,
                     DevicePointPx physicalPoint,
                     ContentScale scale,
                     LogicalPointDp logicalPoint,
                     const ResolvedNode* hitNode) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2)
        << source
        << " action=" << (action ? action : "<null>")
        << " physical=(" << physicalPoint.x << "," << physicalPoint.y << ")"
        << " scale=(" << scale.x.value << "," << scale.y.value << ")"
        << " logical=" << pointToString(logicalPoint)
        << " hit=" << describeNode(hitNode);
    emit(LogCategory::SYSTEM, LogLevel::TRACE, oss.str());
}

void logHitResult(const char* source,
                  LogicalPointDp point,
                  int button,
                  int action,
                  const std::vector<ResolvedNode*>& path,
                  const ResolvedNode* hitNode) {
    std::ostringstream oss;
    oss << source
        << " point=" << pointToString(point)
        << " button=" << button
        << " action=" << action
        << " pathDepth=" << path.size()
        << " hit=" << describeNode(hitNode)
        << " path=" << describeNodeList(path);
    emit(LogCategory::SYSTEM, LogLevel::TRACE, oss.str());
}

void logSceneUiState(const char* source,
                     UIEventSystem* ui,
                     const ResolvedNode* captureNode) {
    std::ostringstream oss;
    oss << source
        << " hover=" << describeNode(ui ? ui->getHoverNode() : nullptr)
        << " focus=" << describeNode(ui ? ui->getFocusNode() : nullptr)
        << " capture=" << describeNode(captureNode);
    emit(LogCategory::SYSTEM, LogLevel::TRACE, oss.str());
}

void logQueuedNodeUpdate(const char* source, const ResolvedNode* node) {
    if (!ldt::Logger::shouldLog(ldt::LogLevel::TRACE)) return;
    std::ostringstream oss;
    oss << source << " node=" << describeNode(node);
    emit(LogCategory::SYSTEM, LogLevel::TRACE, oss.str());
}

void logSchedulerDecision(const char* source,
                          const char* frameAction,
                          const std::vector<ResolvedNode*>& dirtyNodes,
                          bool hasPaintRequest,
                          bool hasRepaintRequest,
                          bool hasAstRepaintRequest) {
    std::ostringstream oss;
    oss << source
        << " action=" << (frameAction ? frameAction : "<null>")
        << " dirtyCount=" << dirtyNodes.size()
        << " flags[p=" << (hasPaintRequest ? 1 : 0)
        << ",r=" << (hasRepaintRequest ? 1 : 0)
        << ",ast=" << (hasAstRepaintRequest ? 1 : 0) << "]"
        << " dirtyNodes=" << describeNodeList(dirtyNodes);
    emit(LogCategory::SYSTEM, LogLevel::DEBUG, oss.str());
}

void logViewportEvent(const char* source, ViewportSizeDp viewportSize) {
    std::ostringstream oss;
    oss << source << " viewport=" << viewportToString(viewportSize);
    emit(LogCategory::SYSTEM, LogLevel::DEBUG, oss.str());
}

void logPartialSync(const char* source, const std::vector<ResolvedNode*>& nodes) {
    std::ostringstream oss;
    oss << source
        << " nodeCount=" << nodes.size()
        << " nodes=" << describeNodeList(nodes);
    emit(LogCategory::SYSTEM, LogLevel::DEBUG, oss.str());
}

void logNodeSnapshot(const char* source, const char* label, const ResolvedNode* node) {
    std::ostringstream oss;
    oss << source
        << " " << (label ? label : "node")
        << "=" << describeNode(node);
    emit(LogCategory::SYSTEM, LogLevel::DEBUG, oss.str());
}

} // namespace ldt::diagnostics