#pragma once
#include "abstract_control.h"
#include "panel.h"
#include "button.h"
#include "text.h"
#include "engine/core/resolved_tree.h"
#include "engine/document_runtime.h"
#include <memory>
#include "ldt_export.h"

namespace ldt {

// 所有与控件创建、绑定和 uid 映射相关的接口已封装为 ControlFactory

// Forward declare ScrollbarControl used in factory signatures
class ScrollbarControl;

class LDT_API ControlFactory {
public:
	ControlFactory() = default;
	virtual ~ControlFactory() = default;

	// Instance management (singleton-like). 默认会有一个全局实例。
	static void setInstance(std::unique_ptr<ControlFactory> inst);
	static std::unique_ptr<ControlFactory> releaseInstance();
	static ControlFactory* getInstance();
	void SyncPropertiesFromResolvedNode(ResolvedNode* rn, std::shared_ptr<class AbstractControl> ctrl);

	std::shared_ptr<AbstractControl> CreateControlFromResolvedNode(ResolvedNode* resolvedNode);
	std::shared_ptr<AbstractControl> CreateControlTreeFromAST(ResolvedNode* resolvedNode);

	void BindControlToResolvedNode(ResolvedNode* rn, std::shared_ptr<AbstractControl> ctrl);

	ResolvedNode* FindNodeByUid(const std::string& uid);
	void RemoveResolvedNodeByUid(const std::string& uid, DocumentRuntime* ctx);
	
	// 执行延迟移除 (通常在帧末调用)
	void executePendingRemovals();

	void RegisterNodeUid(const std::string& uid, ResolvedNode* node);
	void UnregisterNodeUid(const std::string& uid);
	void ClearUidMap();
	// Create auxiliary controls that may be needed by other controls (only factory can create)
	std::shared_ptr<ldt::ScrollbarControl> CreateScrollbar(ldt::ScrollbarControl::Orientation orient);

	// Lightweight factory: create bare controls without AST/ResolvedNode.
	// Useful for debug overlays, programmatic UI, etc.
	std::shared_ptr<Text>  CreateText()  { return std::shared_ptr<Text>(new Text()); }
	std::shared_ptr<Panel> CreatePanel() { return std::shared_ptr<Panel>(new Panel()); }

protected:
	std::shared_ptr<AbstractControl> makeControl(ResolvedNode* resolvedNode);
	// 唯一保留的扩展点：插入额外控件类型创建。
	virtual std::shared_ptr<AbstractControl> doMakeControlFromResolvedNodeHook(ResolvedNode* resolvedNode);

private:
	static std::unique_ptr<ldt::ControlFactory> instance_;
};

} // namespace ldt
