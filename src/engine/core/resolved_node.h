#pragma once

#include "render/display_list.h"
#include <memory>
#include <vector>
#include <unordered_set>
#include <optional>
#include <array>
#include <string>
#include "ast_node.h"
#include "node_flags.h"
#include "computed_style.h"
#include "computed_layout.h"
#include "property_provider.h"
#include <engine/capability/capability_host.h>
#include "ldt_export.h"

class DocumentRuntime;
namespace ldt
{
	class AbstractControl;
	class ResolvedTree;

	// ================= ResolvedNode =================
	class LDT_API ResolvedNode 
	{
	public:
		~ResolvedNode();
		
		ResolvedNode(ResolvedTree* tree);

		friend class ResolvedTree;

	private:
		ResolvedNode();
		// ===== 脏标记 =====
		DirtyFlag dirtyFlags = DirtyFlag::All;
		std::weak_ptr<ldt::AbstractControl> control;
		std::vector<ResolvedNode*> children;
		std::vector<ResolvedNode*> overlay_children;
		std::vector<ResolvedNode*> flow_children;


		CapabilityHost caps_;

	public:
		CapabilityHost &caps() { return caps_; }
		const CapabilityHost &caps() const { return caps_; }

		// ===== Accessor =====
		const class IPropertyProvider& props() const;

		// ===== AST & Control =====
		std::shared_ptr<ASTNode> astNode;
		std::string getId();
		std::string getUid();

		// ===== UI 状态 =====
		UIState uiState = UIState::None;
		UIState lastAppliedState = UIState::None;

		// ===== 样式 =====
		ldt::ComputedStyle finalStyle; // base + state 合并结果（实际使用）
		// compiled rules storage
		CompiledStyleRules compiledRules;

		// ===== 布局 =====
		ComputedLayout layout;
		CompiledLayoutRules layoutRules;

		// ===== 树结构 =====
		ResolvedNode *parent;
		const std::vector<ResolvedNode*>& getChildren();
		ResolvedNode* at(int i);
		const std::vector<ResolvedNode*> &getFlowChildren();
		const std::vector<ResolvedNode*> &getOverlayChildren();
		void clear();
		void destory();

		// Compiled style rules attached to this node (base + state deltas)
		const CompiledStyleRules &getCompiledStyleRules() const;
		void setCompiledStyleRules(const CompiledStyleRules &c);
		void setCompiledLayoutRules(const CompiledLayoutRules &c);

		// Recompute finalStyle based on compiledRules and current uiState
		void recomputeStyle();
		void recompileStyleAndLayout(DocumentRuntime* ctx);

		std::weak_ptr<ldt::AbstractControl> getControl();
		void setControl(std::weak_ptr<ldt::AbstractControl> control);
		// ================= 状态控制 =================
		void setUIState(UIState newState);
		void addUIState(UIState s);
		void removeUIState(UIState s);
		bool hasUIState(UIState s) const;

		// ================= Dirty 逻辑 =================
		void markDirty(DirtyFlag flags);

		// Like markDirty but does NOT cascade to children — only marks this
		// node and propagates ChildrenLayout upward. Use for resize where a
		// full-tree layout pass already handles all descendants.
		void markDirtyNoCascade(DirtyFlag flags);

		void propagateDirtyToParent();

		void clearDirty(DirtyFlag flags);
		bool isDirty(DirtyFlag flags) const;
		bool needsLayout() const;
		bool needsStyle() const;
		DirtyFlag getDirtyFlags() const;

		void addChildren(ResolvedNode* node);
	private:
		ResolvedNode* extractChild(ResolvedNode* node);
		void applyInheritedStyles();

		class IPropertyProvider* property_resolver_ = nullptr;
		DirtyFlag dirtyFlags_ = DirtyFlag::All;
		ResolvedTree* tree_ = nullptr;
	};

}