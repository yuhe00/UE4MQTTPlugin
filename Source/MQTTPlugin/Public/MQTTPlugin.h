#pragma once

#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMQTTPlugin, Log, All);

class FMQTTPlugin : public IModuleInterface
{
public:
	FMQTTPlugin();

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
