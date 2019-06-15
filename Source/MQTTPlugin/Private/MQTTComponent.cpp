#include "MQTTComponent.h"

#include "MQTTClient.h"
#include "MQTTPlugin.h"

FMQTTWorker::FMQTTWorker(const FString& IpAddress, int Port) : IpAddress(IpAddress), Port(Port)
{
	Thread = FRunnableThread::Create(this, TEXT("MQTTWorker"), 0, TPri_BelowNormal);
}

FMQTTWorker::~FMQTTWorker()
{
	delete Thread;
}

bool FMQTTWorker::Init()
{
	FString ConnectString = FString::Printf(TEXT("tcp://%s:%d"), *this->IpAddress, this->Port);

	FGuid Guid = FGuid::NewGuid();
	MQTTClient_create(
		&MQTTClientHandle, TCHAR_TO_ANSI(*ConnectString), TCHAR_TO_ANSI(*Guid.ToString()), MQTTCLIENT_PERSISTENCE_NONE, nullptr);
	MQTTClient_connectOptions ClientConnectionOptions = MQTTClient_connectOptions_initializer;
	ClientConnectionOptions.keepAliveInterval = 20;
	ClientConnectionOptions.cleansession = 1;

	MQTTClient_setCallbacks(MQTTClientHandle, (void*) this, &FMQTTWorker::OnConnectionLost, &FMQTTWorker::OnMessageArrived,
		&FMQTTWorker::OnDeliveryComplete);

	int32 ReturnCode;
	if ((ReturnCode = MQTTClient_connect(MQTTClientHandle, &ClientConnectionOptions)) == MQTTCLIENT_SUCCESS)
	{
		UE_LOG(LogMQTTPlugin, Log, TEXT("Connected to MQTT broker: %s"), *ConnectString);
		ConnectEventQueue.Enqueue(ConnectString);
		return true;
	}
	else
	{
		UE_LOG(LogMQTTPlugin, Error, TEXT("Failed to connect to MQTT broker: %s, return code %d"), *ConnectString, ReturnCode);
	}

	return false;
}

uint32 FMQTTWorker::Run()
{
	FPlatformProcess::Sleep(0.03);

	if (MQTTClientHandle != nullptr && MQTTClient_isConnected(MQTTClientHandle))
	{
		while (StopTaskCounter.GetValue() == 0)
		{
			TPair<FString, EMQTTQosLevel> SubscribeRequest;
			while (SubscribeRequestQueue.Dequeue(SubscribeRequest))
			{
				int ErrorCode =
					MQTTClient_subscribe(MQTTClientHandle, TCHAR_TO_ANSI(*SubscribeRequest.Key), (int) SubscribeRequest.Value);
				if (ErrorCode == MQTTCLIENT_SUCCESS)
				{
					UE_LOG(LogMQTTPlugin, Log, TEXT("Subscribed to channel: %s (QOS level %d)"), *SubscribeRequest.Key,
						SubscribeRequest.Value);

					SubscribeEventQueue.Enqueue(SubscribeRequest);
				}
				else
				{
					UE_LOG(LogMQTTPlugin, Error, TEXT("Failed to subscribe to channel: %s (QOS level %d)"), *SubscribeRequest.Key,
						SubscribeRequest.Value);
				}
			}

			FString UnsubscribeRequestString;
			while (UnsubscribeRequestQueue.Dequeue(UnsubscribeRequestString))
			{
				int ErrorCode = MQTTClient_unsubscribe(MQTTClientHandle, TCHAR_TO_ANSI(*UnsubscribeRequestString));
				if (ErrorCode == MQTTCLIENT_SUCCESS)
				{
					UE_LOG(LogMQTTPlugin, Log, TEXT("Unsubscribed from channel: %s"), *UnsubscribeRequestString);

					UnsubscribeEventQueue.Enqueue(UnsubscribeRequestString);
				}
				else
				{
					UE_LOG(LogMQTTPlugin, Error, TEXT("Failed to unsubscribe from channel: %s"), *UnsubscribeRequestString);
				}
			}

			FMQTTMessage PublishRequestMessage;
			while (PublishRequestQueue.Dequeue(PublishRequestMessage))
			{
				MQTTClient_deliveryToken DeliveryToken;
				int ErrorCode = MQTTClient_publish(MQTTClientHandle, TCHAR_TO_ANSI(*PublishRequestMessage.TopicName),
					PublishRequestMessage.Payload.Len(), TCHAR_TO_ANSI(*PublishRequestMessage.Payload),
					(int) PublishRequestMessage.Qos, PublishRequestMessage.bRetained, &DeliveryToken);
				if (ErrorCode == MQTTCLIENT_SUCCESS)
				{
					PendingMessages.Add(DeliveryToken, PublishRequestMessage);
				}
			}

			// FPlatformProcess::Sleep(0.01);
		}

		if (MQTTClient_disconnect(MQTTClientHandle, 10000) == MQTTCLIENT_SUCCESS)
		{
			UE_LOG(LogMQTTPlugin, Log, TEXT("Disconnected from MQTT broker"));
			MQTTClient_destroy(&MQTTClientHandle);
			MQTTClientHandle = nullptr;
		}
	}

	return 0;
}

void FMQTTWorker::Stop()
{
	StopTaskCounter.Increment();
}

void FMQTTWorker::OnConnectionLost(void* Context, char* Cause)
{
	FMQTTWorker* MQTTWorker = (FMQTTWorker*) Context;
	MQTTWorker->DisconnectEventQueue.Enqueue(FString(ANSI_TO_TCHAR(Cause)));
}

int FMQTTWorker::OnMessageArrived(void* Context, char* TopicName, int TopicLength, MQTTClient_message* Message)
{
	FMQTTWorker* MQTTWorker = (FMQTTWorker*) Context;

	FMQTTMessage MQTTMessage;

	if (TopicLength == 0)
	{
		TopicLength = strlen(TopicName);
	}

	TArray<uint8> TopicNameStringBuffer((uint8*) TopicName, TopicLength);
	TopicNameStringBuffer.Add('\0');
	MQTTMessage.TopicName = FString(UTF8_TO_TCHAR(TopicNameStringBuffer.GetData()));

	TArray<uint8> PayloadStringBuffer((uint8*) Message->payload, Message->payloadlen);
	PayloadStringBuffer.Add('\0');
	MQTTMessage.Payload = FString(UTF8_TO_TCHAR(PayloadStringBuffer.GetData()));

	MQTTWorker->MessageEventQueue.Enqueue(MQTTMessage);

	MQTTClient_freeMessage(&Message);

	return 1;
}

void FMQTTWorker::OnDeliveryComplete(void* Context, MQTTClient_deliveryToken DeliveryToken)
{
	FMQTTWorker* MQTTWorker = (FMQTTWorker*) Context;

	FMQTTMessage* MQTTMessage;
	if ((MQTTMessage = MQTTWorker->PendingMessages.Find(DeliveryToken)) != nullptr)
	{
		MQTTWorker->DeliveryEventQueue.Enqueue(*MQTTMessage);
	}
}

UMQTTComponent::UMQTTComponent() : MQTTWorker(nullptr)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UMQTTComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!MQTTWorker && FPlatformProcess::SupportsMultithreading())
	{
		MQTTWorker = new FMQTTWorker(IpAddress, Port);
	}
}

void UMQTTComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (MQTTWorker != nullptr)
	{
		MQTTWorker->Stop();

		if (MQTTWorker->Thread)
		{
			MQTTWorker->Thread->WaitForCompletion();
		}

		delete MQTTWorker;
	}
}

void UMQTTComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (MQTTWorker)
	{
		FString ConnectEventString;
		while (MQTTWorker->ConnectEventQueue.Dequeue(ConnectEventString))
		{
			OnConnect.Broadcast();
		}

		FString DisconnectEventString;
		while (MQTTWorker->DisconnectEventQueue.Dequeue(DisconnectEventString))
		{
			OnDisconnect.Broadcast();
		}

		TPair<FString, EMQTTQosLevel> SubscribeEvent;
		while (MQTTWorker->SubscribeEventQueue.Dequeue(SubscribeEvent))
		{
			OnSubscribe.Broadcast(SubscribeEvent.Key, SubscribeEvent.Value);
		}

		FString UnsubscribeEventString;
		while (MQTTWorker->UnsubscribeEventQueue.Dequeue(UnsubscribeEventString))
		{
			OnUnsubscribe.Broadcast(UnsubscribeEventString);
		}

		FMQTTMessage DeliveryEventMessage;
		while (MQTTWorker->DeliveryEventQueue.Dequeue(DeliveryEventMessage))
		{
			OnDelivery.Broadcast(DeliveryEventMessage);
		}

		FMQTTMessage MessageEventMessage;
		while (MQTTWorker->MessageEventQueue.Dequeue(MessageEventMessage))
		{
			OnMessage.Broadcast(MessageEventMessage);
		}
	}
}

void UMQTTComponent::Subscribe(const FString& TopicName, EMQTTQosLevel Qos)
{
	MQTTWorker->SubscribeRequestQueue.Enqueue(TPair<FString, EMQTTQosLevel>(TopicName, Qos));
}

void UMQTTComponent::Unsubscribe(const FString& TopicName)
{
	MQTTWorker->UnsubscribeRequestQueue.Enqueue(TopicName);
}

void UMQTTComponent::Publish(const FMQTTMessage& MQTTMessage)
{
	MQTTWorker->PublishRequestQueue.Enqueue(MQTTMessage);
}
