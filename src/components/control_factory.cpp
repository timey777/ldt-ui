#include "control_factory.h"
#include "panel.h"
#include "button.h"
#include "text.h"
#include "image.h"
#include "container_control.h"
#include "engine/document_runtime.h"
#include "resolved_runtime.h"
#include <string>
#include <memory>
#include "input.h"
#include "engine/resource_manager.h"
#include "root.h"
#include <misc/stable_id.h>
#include "engine/core/ast_node.h"
#include "engine/core/resolved_node.h"
#include "combo_box/combo_box.h"
#include "radio_button.h"
#include <sstream>

namespace ldt
{
	static ResolvedRuntime g_resolvedRuntime;

	// 静态实例定义
	std::unique_ptr<ldt::ControlFactory> ControlFactory::instance_ = std::make_unique<ldt::ControlFactory>();

	// Instance management
	void ControlFactory::setInstance(std::unique_ptr<ldt::ControlFactory> inst)
	{
		instance_ = std::move(inst);
	}

	std::unique_ptr<ldt::ControlFactory> ControlFactory::releaseInstance()
	{
		return std::move(instance_);
	}

	ControlFactory *ControlFactory::getInstance()
	{
		return instance_.get();
	}

	void ControlFactory::SyncPropertiesFromResolvedNode(ResolvedNode *rn, std::shared_ptr<AbstractControl> ctrl)
	{
		if (!rn || !ctrl)
			return;
		const auto &layout = rn->layout;
		ctrl->setBounds(layout.getBorderBox());
		ctrl->setScrollSize(layout.scroll.scrollWidth, layout.scroll.scrollHeight);
		ctrl->setViewportSize(layout.viewportWidth, layout.viewportHeight);
		ctrl->setScroll(layout.scroll.offsetX, layout.scroll.offsetY);

		ctrl->setFillColor(rn->finalStyle.backgroundColor);
		ctrl->setStrokeColor(rn->finalStyle.borderColor);
		ctrl->setStrokeWidth(rn->finalStyle.borderWidth);
		// borderRadius is now UIUnit, representing pixel values
		ctrl->setCornerRadius(rn->finalStyle.borderRadius.toPx());

		const bool displayNone = (rn->layoutRules.getDisplay() == ldt::FormattingContext::None);
		ctrl->setVisible(rn->finalStyle.visible && !displayNone);

		if (!rn->finalStyle.backgroundImage.empty())
		{
			ctrl->setBackgroundImage(rn->finalStyle.backgroundImage);
			ResourceManager::getInstance().preloadImage(rn->finalStyle.backgroundImage);
		}
	}

	std::shared_ptr<AbstractControl> ControlFactory::CreateControlFromResolvedNode(ResolvedNode *resolvedNode)
	{
		if (!resolvedNode)
			return nullptr;
		auto &ast = resolvedNode->astNode;
		if (!ast)
			return nullptr;

		std::shared_ptr<AbstractControl> control = doMakeControlFromResolvedNodeHook(resolvedNode);
		if (!control)
		{
			control = makeControl(resolvedNode);
		}

		if (!control)
			return nullptr;

		if (ast->hasAttribute("id"))
			control->setId(ast->getAttribute("id")->as<std::string>());
		if (!ast->getClassList().empty())
			control->setClassList(ast->getClassList());
		auto uidAttr = ast->getUid();
		if (uidAttr && uidAttr->isString())
			control->setUid(uidAttr->as<std::string>());
		else
		{
			LDT_ERROR("no uid attr");
		}

		BindControlToResolvedNode(resolvedNode, control);
		SyncPropertiesFromResolvedNode(resolvedNode, control);

		return control;
	}

	std::shared_ptr<AbstractControl> ControlFactory::CreateControlTreeFromAST(ResolvedNode *resolvedNode)
	{
		if (!resolvedNode)
			return nullptr;
		auto ast = resolvedNode->astNode;
		if (!ast)
			return nullptr;

		auto ctrl = CreateControlFromResolvedNode(resolvedNode);
		if (!ctrl)
			return nullptr;

		if (auto container = std::dynamic_pointer_cast<ContainerControl>(ctrl))
		{
			for (const auto &childResolved : resolvedNode->getChildren())
			{
				auto childCtrl = CreateControlTreeFromAST(childResolved);
				if (childCtrl)
					container->addChild(childCtrl);
			}
		}

		return ctrl;
	}

	void ControlFactory::BindControlToResolvedNode(ResolvedNode *rn, std::shared_ptr<AbstractControl> ctrl)
	{
		if (!rn || !ctrl)
		{
			LDT_ERROR("binding fail");
			return;
		}
		if (ctrl->getUid().empty())
		{
			LDT_ERROR("binding fail no uid");
			return;
		}

		// 绑定 weak reference
		rn->setControl(ctrl);
		RegisterNodeUid(ctrl->getUid(), rn);

		// 同步滚动/viewport/scrollSize 到 Control（control 是表现层，ResolvedNode 为来源）
		try
		{
			ctrl->setScroll(rn->layout.scroll.offsetX, rn->layout.scroll.offsetY);
			ctrl->setScrollSize(rn->layout.scroll.scrollWidth, rn->layout.scroll.scrollHeight);
			ctrl->setViewportSize(rn->layout.viewportWidth, rn->layout.viewportHeight);
			ctrl->setBounds(rn->layout.getBorderBox());
		}
		catch (...)
		{
			LDT_ERROR("ControlFactory::BindControlToResolvedNode: failed to sync scroll/viewport/bounds");
		}
	}

	ResolvedNode *ControlFactory::FindNodeByUid(const std::string &uid)
	{
		return g_resolvedRuntime.findNodeByUid(uid);
	}

	void ControlFactory::RemoveResolvedNodeByUid(const std::string &uid, DocumentRuntime *ctx)
	{
		(void)ctx;
		g_resolvedRuntime.scheduleRemovalByUid(uid);
	}

	void ControlFactory::RegisterNodeUid(const std::string &uid, ResolvedNode *node)
	{
		g_resolvedRuntime.registerNode(uid, node);
	}

	void ControlFactory::UnregisterNodeUid(const std::string &uid)
	{
		g_resolvedRuntime.unregisterNode(uid);
	}

	void ControlFactory::ClearUidMap()
	{
		g_resolvedRuntime.clear();
	}

	void ControlFactory::executePendingRemovals()
	{
		g_resolvedRuntime.executePendingRemovals();
	}

	std::shared_ptr<ScrollbarControl> ControlFactory::CreateScrollbar(ScrollbarControl::Orientation orient)
	{
		return std::shared_ptr<ScrollbarControl>(new ScrollbarControl(orient));
	}
	std::shared_ptr<AbstractControl> ControlFactory::makeControl(ResolvedNode *resolvedNode)
	{
		auto &ast = resolvedNode->astNode;
		std::shared_ptr<AbstractControl> control = nullptr;
		if (ast->type == "Root")
		{
			control = std::shared_ptr<Root>(new Root());
		}
		else if (ast->type == "panel")
		{
			control = std::shared_ptr<Panel>(new Panel());
		}
		else if (ast->type == "button")
		{
			control = std::shared_ptr<Button>(new Button());
		}
		else if (ast->type == "text")
		{
			auto text = std::shared_ptr<Text>(new Text());
			text->setTextColor(resolvedNode->finalStyle.textColor);
			text->setFontSize(resolvedNode->finalStyle.fontSize);
			text->setLineHeight(resolvedNode->finalStyle.lineHeight);
			text->setWrap(resolvedNode->layout.wrap);
			text->setFontFamily(resolvedNode->finalStyle.fontFamily);
			text->setAlignment(resolvedNode->finalStyle.textAlign);
			text->setLayoutWidth(resolvedNode->layout.getBorderBox().width);
			text->setPadding(resolvedNode->layout.padding.left, resolvedNode->layout.padding.top,
							 resolvedNode->layout.padding.right, resolvedNode->layout.padding.bottom);
			if (ast->hasAttribute("value"))
				text->setText(ast->getAttribute("value")->as<std::string>());
			control = text;
		}
		else if (ast->type == "image")
		{
			auto image = std::shared_ptr<Image>(new Image());
			if (ast->hasAttribute("src"))
			{
				std::string src = ast->getAttribute("src")->as<std::string>();
				image->setSrc(src);
				ResourceManager::getInstance().preloadImage(src);
			}
			image->setPadding(resolvedNode->layout.padding.left, resolvedNode->layout.padding.top,
							  resolvedNode->layout.padding.right, resolvedNode->layout.padding.bottom);
			control = image;
		}
		else if (ast->type == "combobox")
		{
			auto combo = std::shared_ptr<ComboBox>(new ComboBox());
			if (ast->hasAttribute("options"))
			{
				auto attr = ast->getAttribute("options");
				if (attr && attr->isString())
				{
					std::string s = attr->as<std::string>();
					std::vector<std::string> opts;
					std::stringstream ss(s);
					std::string item;
					while (std::getline(ss, item, ','))
					{
						opts.push_back(item);
					}
					combo->setOptions(opts);
				}
			}
			combo->setTextColor(resolvedNode->finalStyle.textColor);
			combo->setFontSize(resolvedNode->finalStyle.fontSize);
			control = combo;
		}
		else if (ast->type == "radio")
		{
			auto radio = std::shared_ptr<RadioButton>(new RadioButton());
			if (ast->hasAttribute("checked"))
			{
				auto val = ast->getAttribute("checked");
				if (val && val->isBool())
					radio->setChecked(val->as<bool>());
			}
			if (ast->hasAttribute("group"))
			{
				radio->setGroup(ast->getAttribute("group")->as<std::string>());
			}
			if (ast->hasAttribute("value"))
			{
				radio->setText(ast->getAttribute("value")->as<std::string>());
			}
			control = radio;
		}
		else if (ast->type == "input")
		{
			auto input = std::shared_ptr<ldt::Input>(new ldt::Input());
			if (ast->hasAttribute("value"))
				input->setValue(ast->getAttribute("value")->as<std::string>());
			if (ast->hasAttribute("placeholder"))
				input->setPlaceholder(ast->getAttribute("placeholder")->as<std::string>());
			if (ast->hasAttribute("type"))
				input->setType(ast->getAttribute("type")->as<std::string>());
			if (ast->hasAttribute("wrap")) {
				auto wrapAttr = ast->getAttribute("wrap");
				bool w = false;
				if (wrapAttr->isBool()) w = wrapAttr->as<bool>();
				else if (wrapAttr->isString()) {
					std::string ws = wrapAttr->as<std::string>();
					w = (ws == "true" || ws == "1");
				}
				input->setWrap(w);
			}
			input->setTextColor(resolvedNode->finalStyle.textColor);
			input->setFontSize(resolvedNode->finalStyle.fontSize);
			input->setPadding(resolvedNode->layout.padding.left, resolvedNode->layout.padding.top,
							  resolvedNode->layout.padding.right, resolvedNode->layout.padding.bottom);
			control = input;
		}
		else
		{
			return nullptr;
		}
		return control;
	}

	std::shared_ptr<AbstractControl> ControlFactory::doMakeControlFromResolvedNodeHook(ResolvedNode *resolvedNode)
	{
		return nullptr;
	}

} // namespace ldt
