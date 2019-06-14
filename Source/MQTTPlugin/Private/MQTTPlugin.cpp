#include "MQTTPlugin.h"

DEFINE_LOG_CATEGORY(LogMQTTPlugin);

IMPLEMENT_MODULE(FMQTTPlugin, MQTTPlugin);

FMQTTPlugin::FMQTTPlugin()
{
}

void FMQTTPlugin::StartupModule()
{
	UE_LOG(LogMQTTPlugin, Warning, TEXT("Loaded MQTT plugin module"));
}

void FMQTTPlugin::ShutdownModule()
{
	UE_LOG(LogMQTTPlugin, Warning, TEXT("Unloaded MQTT plugin module"));
}
