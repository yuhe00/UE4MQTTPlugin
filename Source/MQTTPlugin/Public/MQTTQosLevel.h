#pragma once

#include "CoreMinimal.h"

#include "MQTTQosLevel.generated.h"

UENUM(BlueprintType)
enum class EMQTTQosLevel : uint8
{
	MQL_QOS_0 = 0 UMETA(DisplayName = "QoS0 (Fire and forget)"),
	MQL_QOS_1 = 1 UMETA(DisplayName = "QoS1 (At least once)"),
	MQL_QOS_2 = 2 UMETA(DisplayName = "QoS2 (Once and one only)")
};
