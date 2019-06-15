#pragma once

#include "CoreMinimal.h"
#include "MQTTClient.h"
#include "MQTTMessage.h"

#include "MQTTComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnConnectDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDisconnectDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSubscribeDelegate, const FString&, TopicName, EMQTTQosLevel, Qos);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnsubscribeDelegate, const FString&, TopicName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeliveryDelegate, const FMQTTMessage&, MQTTMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMessageDelegate, const FMQTTMessage&, MQTTMessage);

class FMQTTWorker : public FRunnable
{
	friend class UMQTTComponent;

	FRunnableThread* Thread;
	FThreadSafeCounter StopTaskCounter;

public:
	FMQTTWorker(const FString& IpAddress, int Port);
	~FMQTTWorker();

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	static void OnConnectionLost(void* Context, char* Cause);
	static int OnMessageArrived(void* Context, char* TopicName, int TopicLength, MQTTClient_message* Message);
	static void OnDeliveryComplete(void* Context, MQTTClient_deliveryToken DeliveryToken);

private:
	// These are threadsafe queues :)
	TQueue<TPair<FString, EMQTTQosLevel>> SubscribeRequestQueue;
	TQueue<FString> UnsubscribeRequestQueue;
	TQueue<FMQTTMessage> PublishRequestQueue;

	TQueue<FString> ConnectEventQueue;
	TQueue<FString> DisconnectEventQueue;
	TQueue<TPair<FString, EMQTTQosLevel>> SubscribeEventQueue;
	TQueue<FString> UnsubscribeEventQueue;
	TQueue<FMQTTMessage> DeliveryEventQueue;
	TQueue<FMQTTMessage> MessageEventQueue;

private:
	void* MQTTClientHandle;

	FString IpAddress;
	int Port;

	TMap<int, FMQTTMessage> PendingMessages;
};

UCLASS(Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent), Config = Game, DefaultConfig)
class UMQTTComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMQTTComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	FMQTTWorker* MQTTWorker;

public:
	UPROPERTY(Category = MQTT, EditDefaultsOnly, BlueprintReadOnly, Config)
	FString IpAddress = "127.0.0.1";

	UPROPERTY(Category = MQTT, EditDefaultsOnly, BlueprintReadOnly, Config)
	int Port = 1883;

	UPROPERTY(Category = MQTT, BlueprintAssignable)
	FOnConnectDelegate OnConnect;

	UPROPERTY(Category = MQTT, BlueprintAssignable)
	FOnDisconnectDelegate OnDisconnect;

	UPROPERTY(Category = MQTT, BlueprintAssignable)
	FOnSubscribeDelegate OnSubscribe;

	UPROPERTY(Category = MQTT, BlueprintAssignable)
	FOnUnsubscribeDelegate OnUnsubscribe;

	UPROPERTY(Category = MQTT, BlueprintAssignable)
	FOnDeliveryDelegate OnDelivery;

	UPROPERTY(Category = MQTT, BlueprintAssignable)
	FOnMessageDelegate OnMessage;

public:
	UFUNCTION(Category = MQTT, BlueprintCallable)
	void Subscribe(const FString& TopicName, EMQTTQosLevel Qos);

	UFUNCTION(Category = MQTT, BlueprintCallable)
	void Unsubscribe(const FString& TopicName);

	UFUNCTION(Category = MQTT, BlueprintCallable)
	void Publish(const FMQTTMessage& MQTTMessage);
};
