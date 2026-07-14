#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <functional>

#include "core/parse.h"
#include "core/coordinate_types.h"
#include "render/display_list.h"
#include "core/resolved_tree.h"
#include "misc/service_registry.h"
#include "ldt_export.h"

// 前向声明：文本测量接口
class ITextMeasurer;

// 前向声明
class StyleEngine;
class LayoutEngine;
class BoxModelEngine;
class ASTBuildEngine;
class UpdateScheduler;

namespace ldt {
	class ResolvedNode;
	class Scene;
}

class LDT_API DocumentRuntime
{
	protected:
		ServiceRegistry serviceRegistry;

		template <typename T>
		T* getService() const
		{
			return serviceRegistry.get<T>();
		}

		std::shared_ptr<ASTNode> m_ast;
		void setAST(std::shared_ptr<ASTNode> a);
		// 共享的 ResolvedTree
		ldt::ResolvedTree resolvedTree;
	public:
		std::shared_ptr<ASTNode> getAST();

		void registerService(AbstractEngine* service);

		template <typename T>
		bool hasService() const
		{
			return serviceRegistry.has<T>();
		}

		// 文本测量器（由 Application 创建并注入）
		ITextMeasurer* textMeasurer = nullptr;

		// 获取共享的 ResolvedTree
		ldt::ResolvedTree* getResolvedTree();
		const ldt::ResolvedTree* getResolvedTree() const;

		// UIEventSystem 已由 Scene 持有，DocumentRuntime 不再暴露该接口

		// 文本测量注入/访问（显式 setter，不使用 registerService）
		void setTextMeasurer(ITextMeasurer* m);
		ITextMeasurer* getTextMeasurer() const;

		// 便捷访问常用引擎（若已通过 registerService 注册）
		LayoutEngine* getLayoutEngine() const;
		StyleEngine* getStyleEngine() const;
		BoxModelEngine* getBoxModelEngine() const;
		ASTBuildEngine* getASTBuildEngine() const;

		// 初始化/关闭/运行管线接口（在 engine_context.cpp 中实现）
		void initializeAll(std::vector<std::type_index> pipelineOrder);
		void shutdownAll();
		// 原始管线：生成 DisplayList
		void runPipeline(ldt::DisplayList& outList, float viewportW, float viewportH);
		void runPipeline(const std::vector<std::type_index>& pipelineOrder, float viewportW, float viewportH);
		// 新增：只解析并填充 ResolvedTree，不生成 DisplayList
		void runPipelineRenderTreeOnly(ldt::ViewportSizeDp viewportSize);
		void runPipelineLayoutOnly(ldt::ViewportSizeDp viewportSize);

		// 通知所有已注册引擎 AST 已更新，并将 AST 更新请求排入 UpdateScheduler
		void notifyASTUpdated(std::shared_ptr<ASTNode> root);
};