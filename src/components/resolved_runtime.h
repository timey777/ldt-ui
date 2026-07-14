#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ldt {

class ResolvedNode;

class ResolvedRuntime {
public:
    ResolvedNode* findNodeByUid(const std::string& uid) const;
    void registerNode(const std::string& uid, ResolvedNode* node);
    void unregisterNode(const std::string& uid);
    void clear();

    void scheduleRemovalByUid(const std::string& uid);
    void executePendingRemovals();

private:
    std::unordered_map<std::string, ResolvedNode*> uidMap_;
    std::unordered_set<ResolvedNode*> pendingRemovalSet_;
    std::vector<ResolvedNode*> pendingRemovalNodes_;
};

} // namespace ldt