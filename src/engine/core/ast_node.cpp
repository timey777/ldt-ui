#include "ast_node.h"
#include "misc/logger.h"

// Implementations moved from header
void ASTNode::setClassList(const std::vector<std::string>& classes) { classList = classes; }
const std::vector<std::string>& ASTNode::getClassList() const { return classList; }
std::shared_ptr<ASTNode> ASTNode::make(const std::string& t) { return std::make_shared<ASTNode>(t); }
ASTNode::ASTNode(const std::string& t) : type(t) {}

void ASTNode::updateFrom(const std::shared_ptr<ASTNode>& other) {
	if (!other) return;
	type = other->type;
	attrs = other->attrs;
	classList = other->classList;
	meta = other->meta;

	std::unordered_map<std::string, std::shared_ptr<ASTNode>> curByUid;
	std::vector<bool> curUsed(children.size(), false);

	for (size_t i = 0; i < children.size(); ++i) {
		const auto& c = children[i];
		if (!c) continue;
		try {
			if (auto uid = getUid()) {
				if (uid) {
					curByUid[uid->as<std::string>()] = c;
				}
			}
		}
		catch (...) {
			LDT_ERROR("ASTNode::updateFrom: failed to read uid");
		}
	}

	std::vector<std::shared_ptr<ASTNode>> newChildren;
	newChildren.reserve(other->children.size());

	for (const auto& ochild : other->children) {
		if (!ochild) {
			newChildren.push_back(nullptr);
			continue;
		}

		bool matched = false;
		try {
			if (auto ouid = ochild->getUid(); ouid && ouid->isString()) {
				std::string uidv = ouid->as<std::string>();
				auto it = curByUid.find(uidv);
				if (it != curByUid.end()) {
					auto existing = it->second;
					bool alreadyUsed = false;
					for (size_t k = 0; k < children.size(); ++k) {
						if (children[k] == existing && curUsed[k]) { alreadyUsed = true; break; }
					}
					if (!alreadyUsed) {
						existing->updateFrom(ochild);
						newChildren.push_back(existing);
						matched = true;
						for (size_t k = 0; k < children.size(); ++k) { if (children[k] == existing) { curUsed[k] = true; break; } }
					}
				}
			}
		}
		catch (...) { matched = false; }

		if (matched) continue;

		bool typeMatched = false;
		for (size_t k = 0; k < children.size(); ++k) {
			if (curUsed[k]) continue;
			if (!children[k]) continue;
			if (children[k]->type == ochild->type) {
				children[k]->updateFrom(ochild);
				newChildren.push_back(children[k]);
				curUsed[k] = true;
				typeMatched = true;
				break;
			}
		}
		if (typeMatched) continue;

		newChildren.push_back(ochild);
	}

	children = std::move(newChildren);
}

void ASTNode::addMeta(const std::string& key, std::shared_ptr<ASTNode> node) { meta[key].push_back(node); }

const std::vector<std::shared_ptr<ASTNode>>* ASTNode::getMeta(const std::string& key) const {
	auto it = meta.find(key);
	if (it != meta.end()) return &(it->second);
	return nullptr;
}

bool ASTNode::hasMeta(const std::string& key) const { return meta.find(key) != meta.end(); }

const Attribute* ASTNode::getUid() const {
	return getAttribute("__uid");
}

const Attribute* ASTNode::getAttribute(const std::string& key) const {
	auto it = attrs.find(key);
	if (it != attrs.end()) return &(it->second);
	return nullptr;
}

bool ASTNode::hasAttribute(const std::string& key) const { return attrs.find(key) != attrs.end(); }

bool ASTNode::setAttribute(const std::string& key, const Attribute& attr)
{
	attrs[key] = attr;
	return true;
}

bool ASTNode::hasClass(const std::string& cls) const {
	for (const auto& c : classList) if (c == cls) return true;
	return false;
}

void ASTNode::setLocalScopeId(std::optional<int> id)
{
	localScopeId = id;
}

const std::optional<int> ASTNode::getLocalScopeId()
{
	if (localScopeId.has_value())
	{
		return localScopeId;
	}
	auto parentPrt = parent.lock();
	if (parentPrt)
	{
		return parentPrt->getLocalScopeId();
	}
	return std::nullopt;
}
