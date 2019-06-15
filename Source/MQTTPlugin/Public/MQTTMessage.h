#pragma once

#include "CoreMinimal.h"
#include "MQTTQosLevel.h"

#include "MQTTMessage.generated.h"

USTRUCT(BlueprintType)
struct FMQTTMessage
{
	GENERATED_BODY()

	UPROPERTY(Category = MQTT, BlueprintReadWrite)
	FString TopicName;

	UPROPERTY(Category = MQTT, BlueprintReadWrite)
	FString Payload;

	UPROPERTY(Category = MQTT, BlueprintReadWrite)
	EMQTTQosLevel Qos;

	UPROPERTY(Category = MQTT, BlueprintReadWrite)
	bool bRetained;
};
