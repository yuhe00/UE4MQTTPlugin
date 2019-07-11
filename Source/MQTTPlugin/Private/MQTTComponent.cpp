#include "MQTTComponent.h"

#include "MQTTClient.h"
#include "MQTTPlugin.h"

FMQTTWorker::FMQTTWorker(const FString& ServerUri, const FString& Username, const FString& Password) :
	ServerUri(ServerUri),
	Username(Username),
	Password(Password)
{
	Thread = FRunnableThread::Create(this, TEXT("MQTTWorker"), 0, TPri_BelowNormal);
}

FMQTTWorker::~FMQTTWorker()
{
	delete Thread;
}

bool FMQTTWorker::Init()
{
	FGuid UniqueClientId = FGuid::NewGuid();

	int ErrorCode;
	if ((ErrorCode = MQTTClient_create(&MQTTClientHandle, TCHAR_TO_ANSI(*ServerUri), TCHAR_TO_ANSI(*UniqueClientId.ToString()),
			 MQTTCLIENT_PERSISTENCE_NONE, nullptr)) == MQTTCLIENT_SUCCESS)
	{
		// TODO make more of these configurable

		MQTTClient_connectOptions ClientConnectionOptions = MQTTClient_connectOptions_initializer;
		ClientConnectionOptions.keepAliveInterval = 30;
		ClientConnectionOptions.cleansession = 1;
		ClientConnectionOptions.reliable = 1;
		ClientConnectionOptions.will = nullptr;
		ClientConnectionOptions.username = TCHAR_TO_ANSI(*Username);
		ClientConnectionOptions.password = TCHAR_TO_ANSI(*Password);
		ClientConnectionOptions.connectTimeout = 3;
		ClientConnectionOptions.retryInterval = 0;
		ClientConnectionOptions.ssl = nullptr;	// TODO SSL support

		MQTTClient_setCallbacks(MQTTClientHandle, (void*) this, &FMQTTWorker::OnConnectionLost, &FMQTTWorker::OnMessageArrived,
			&FMQTTWorker::OnDeliveryComplete);

		// TODO websockets support

		if ((ErrorCode = MQTTClient_connect(MQTTClientHandle, &ClientConnectionOptions)) == MQTTCLIENT_SUCCESS)
		{
			UE_LOG(LogMQTTPlugin, Log, TEXT("Connected to MQTT broker: %s"), *ServerUri);
			ConnectEventQueue.Enqueue(ServerUri);
			return true;
		}
		else
		{
			UE_LOG(LogMQTTPlugin, Error, TEXT("Failed to connect to MQTT broker: %s, return code %d"), *ServerUri, ErrorCode);
		}
	}
	else
	{
		UE_LOG(LogMQTTPlugin, Error, TEXT("Failed to create MQTT client, return code %d"), ErrorCode);
	}

	return false;
}

uint32 FMQTTWorker::Run()
{
	while (StopTaskCounter.GetValue() == 0)
	{
		if (MQTTClientHandle != nullptr && MQTTClient_isConnected(MQTTClientHandle))
		{
			TPair<FString, EMQTTQosLevel> SubscribeRequest;
			while (SubscribeRequestQueue.Dequeue(SubscribeRequest))
			{
				int ErrorCode =
					MQTTClient_subscribe(MQTTClientHandle, TCHAR_TO_ANSI(*SubscribeRequest.Key), (int) SubscribeRequest.Value);
				if (ErrorCode == MQTTCLIENT_SUCCESS)
				{
					UE_LOG(LogMQTTPlugin, Log, TEXT("Subscribed to channel: %s (QOS level %d)"), *SubscribeRequest.Key,
						(int) SubscribeRequest.Value);

					SubscribeEventQueue.Enqueue(SubscribeRequest);
				}
				else
				{
					UE_LOG(LogMQTTPlugin, Error, TEXT("Failed to subscribe to channel: %s (QOS level %d)"), *SubscribeRequest.Key,
						(int) SubscribeRequest.Value);
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
				if (ErrorCode == MQTTCLIENT_SUCCESS &&
					PublishRequestMessage.Qos != EMQTTQosLevel::MQL_QOS_0)	// No delivery callback for QoS0
				{
					PendingMessages.Add(DeliveryToken, PublishRequestMessage);
				}
			}

			// Save system resources?
			// FPlatformProcess::Sleep(0.01);
		}
	}

	// Clean up when we are done

	if (MQTTClientHandle && MQTTClient_isConnected(MQTTClientHandle))
	{
		int ErrorCode;
		if ((ErrorCode = MQTTClient_disconnect(MQTTClientHandle, 3000)) == MQTTCLIENT_SUCCESS)
		{
			UE_LOG(LogMQTTPlugin, Log, TEXT("Disconnected from MQTT broker"));
			MQTTClient_destroy(&MQTTClientHandle);
			MQTTClientHandle = nullptr;
		}
		else
		{
			UE_LOG(LogMQTTPlugin, Error, TEXT("Failed to disconnect from server, return code %d"), ErrorCode);
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
		// Explicit length is only used when there are '\0'-characters embedded in string, otherwise use strlen()
		TopicLength = strlen(TopicName);
	}

	TArray<uint8> TopicNameStringBuffer((uint8*) TopicName, TopicLength);
	TopicNameStringBuffer.Add('\0');
	MQTTMessage.TopicName = FString(UTF8_TO_TCHAR(TopicNameStringBuffer.GetData()));

	TArray<uint8> PayloadStringBuffer((uint8*) Message->payload, Message->payloadlen);
	PayloadStringBuffer.Add('\0');
	MQTTMessage.Payload = FString(UTF8_TO_TCHAR(PayloadStringBuffer.GetData()));

	MQTTWorker->MessageEventQueue.Enqueue(MQTTMessage);

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

	if (bAutoActivate)
	{
		Connect();
	}
}

void UMQTTComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	Disconnect();
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

void UMQTTComponent::Connect()
{
	if (!MQTTWorker && FPlatformProcess::SupportsMultithreading())
	{
		MQTTWorker = new FMQTTWorker(ServerUri, Username, Password);
	}
	else
	{
		UE_LOG(LogMQTTPlugin, Warning, TEXT("Already connected! Not attempting to connect again..."));
	}
}

void UMQTTComponent::Disconnect()
{
	if (MQTTWorker != nullptr)
	{
		MQTTWorker->Stop();

		if (MQTTWorker->Thread)
		{
			MQTTWorker->Thread->WaitForCompletion();
		}

		delete MQTTWorker;
		MQTTWorker = nullptr;
	}
	else
	{
		UE_LOG(LogMQTTPlugin, Warning, TEXT("Not connected to MQTT..."));
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
