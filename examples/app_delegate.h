#pragma once
#include "application.h"
#include <engine/core/parse.h>
#include <MainScene.h>
#include "components/stage.h"
#include "misc/logger.h"
class AppDelegate : public Application
{
public:
	AppDelegate(std::string title,
		int winW,
		int winH)
		:Application(title, static_cast<float>(winW), static_cast<float>(winH))
	{
	}

	~AppDelegate()
	{
	}
	virtual bool build(DocumentRuntime& runtime, ldt::Compositor& compositor, float width, float height) override
	{
		(void)compositor;
		(void)width;
		(void)height;
		ldt::Logger::setEnabled(false);
		auto mainScene = std::make_shared<ldt::ExampleMainScene>(runtime);
		if (mainScene)
		{
			ldt::Stage::getInstance().replaceScene(mainScene);
		}
		return true;
	}
};
