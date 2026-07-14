#pragma once

#include <memory>
#include <string>

#include "render/display_list.h"
#include <typeindex>
#include "core/parse.h"
#include "ldt_export.h"

class DocumentRuntime; // forward

// 抽象引擎基类：未来可扩展为 LayoutEngine/StyleEngine/ScriptEngine/EventEngine 等
class LDT_API AbstractEngine
{
public:
	virtual ~AbstractEngine() = default;
	virtual std::type_index type() const = 0;
	// 初始化（资源/缓存等）
	virtual void initialize();

	// 关闭/释放资源
	virtual void shutdown();

	// 设置视口尺寸 (DIP)
	virtual void setViewport(float width, float height);

	// 主要处理接口：将 AST 转换/处理为 DisplayList（或执行其它引擎工作）
	virtual void process(std::shared_ptr<ASTNode> root, ldt::DisplayList &outList, float viewportW, float viewportH) = 0;

	// 当 AST 被热重载时，框架会调用此方法（引擎可选择增量更新）
	virtual void onASTUpdated(std::shared_ptr<ASTNode> root);

	// 每帧更新（可选，用于脚本、动画等）
	virtual void update(float dt);

	// 引擎名称（调试/日志用途）
	virtual std::string name() const = 0;

	// 是否支持热重载（默认支持）
	virtual bool supportsHotReload() const { return true; }

	// 注入运行时上下文（EngineManager 创建后注入）
	virtual void setContext(DocumentRuntime *ctx);
	virtual DocumentRuntime *getContext() const;

protected:
	DocumentRuntime *context_ = nullptr;
};
