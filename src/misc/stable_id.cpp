#include "stable_id.h"
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <unordered_set>
#include "engine/core/ast_node.h"
#include "components/control_factory.h"
#include "misc/logger.h"

using namespace std;

namespace ldt {

// FNV-1a 64-bit stable hash for deterministic ids
static uint64_t fnv1a64(const std::string& s) {
    const uint64_t fnv_offset = 14695981039346656037ULL;
    const uint64_t fnv_prime = 1099511628211ULL;
    uint64_t h = fnv_offset;
    for (unsigned char c : s) {
        h ^= static_cast<uint64_t>(c);
        h *= fnv_prime;
    }
    return h;
}

static std::string u64_to_hex(uint64_t v) {
    std::ostringstream oss;
    oss << std::hex;
    // zero-pad to 16 hex digits
    oss.width(16);
    oss.fill('0');
    oss << v;
    return oss.str();
}

std::string StableId::stableIdForPath(const std::string& path) {
    uint64_t h = fnv1a64(path);
    return std::string("sid_") + u64_to_hex(h);
}

std::string StableId::getNodeSegment(const std::shared_ptr<ASTNode>& node) {
    std::string seg = node->type;
    if (auto idAttr = node->getAttribute("id")) {
        try {
            if (idAttr->isString()) {
                return seg + "[" + idAttr->as<std::string>() + "]";
            }
        } catch (...) {
            LDT_ERROR("StableId::getNodeSegment: failed to read id attribute");
        }
    }
    const char* keys[] = {"class", "title", "text", "value", "placeholder"};
    std::string keyVal;
    for (auto k : keys) {
        if (auto a = node->getAttribute(k)) {
            try {
                if (a->isString()) { keyVal = a->as<std::string>(); break; }
            } catch(...) {
                LDT_ERROR("StableId::getNodeSegment: failed to read attribute " << k);
            }
        }
    }
    if (!keyVal.empty()) {
        seg += "[" + keyVal + "]";
    }
    return seg;
}

void StableId::assignStableIdsRecursive(const std::shared_ptr<ASTNode>& node, const std::string& parentPath) {
    if (!node) return;

    std::string seg = getNodeSegment(node);
    std::string myPath = parentPath.empty() ? seg : parentPath + "/" + seg;

    // Always assign an internal unique id based on path
    {
        std::string uid = StableId::stableIdForPath(myPath);
        node->attrs["__uid"] = Attribute(uid);
    }

    // Pre-calculate segments for children to detect collisions
    std::unordered_map<std::string, int> segCounts;
    for (const auto& child : node->children) {
        if (!child) continue;
        segCounts[getNodeSegment(child)]++;
    }

    std::unordered_map<std::string, int> segIndices;
    for (const auto& child : node->children) {
        if (!child) continue;
        std::string childSeg = getNodeSegment(child);
        int count = segCounts[childSeg];
        int idx = ++segIndices[childSeg];

        std::string childParentPath = myPath;
        // If multiple siblings have the same segment (collision), append index to parent path to disambiguate.
        if (count > 1) {
            childParentPath += ":" + std::to_string(idx);
        }
        StableId::assignStableIdsRecursive(child, childParentPath);
    }
}

void StableId::assignStableIdsUnderParent(const std::shared_ptr<ASTNode>& parentAst, const std::shared_ptr<ASTNode>& node) {
    if (!node || !parentAst) return;

    std::string parentSeg = getNodeSegment(parentAst);
    std::string nodeSeg = getNodeSegment(node);
    
    // 使用父节点持有的计数器递增生成 index
    // 这样即使中间删除了 u2，新生成的也会是 u4，确保不会和现有的 u3 碰撞
    int nextIdx = ++parentAst->childTypeCounters[nodeSeg];

    std::string finalPath = parentSeg + "/" + nodeSeg;
    if (nextIdx > 1) {
        finalPath += ":" + std::to_string(nextIdx);
    }

    // 为 node 递归分配 UID
    StableId::assignStableIdsRecursive(node, finalPath);
}

} // namespace ldt
