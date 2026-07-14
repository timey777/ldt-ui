#pragma once

#include <string>
#include "ldt_export.h"

namespace ldt
{
	// 平台宿主接口：承载光标、剪贴板等平台级 UI 能力
	// 由 Application 创建并持有，通过 Stage::setPlatformHost() 注入到 Stage 单例
	// 所有 Scene（包括预览 Scene）通过 Stage 转发共享同一份平台能力
	struct LDT_API IPlatformHost {
		virtual ~IPlatformHost() = default;
		virtual void setCursor(int cursorType) { (void)cursorType; }
		virtual void setClipboardString(const std::string& str) { (void)str; }
		virtual std::string getClipboardString() { return std::string(); }
	};
}