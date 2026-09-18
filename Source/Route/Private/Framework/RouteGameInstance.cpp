// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/RouteGameInstance.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"

#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void URouteGameInstance::Init()
{
	Super::Init();

	//UE_LOG(LogTemp, Log, TEXT("RouteGameInstance Init"));

	if (IsDedicatedServerInstance())
	{
		UE_LOG(LogTemp, Log, TEXT("Dedicated Server GameInstance. Skip client requests."));
		return;
	}

	RequestLogin(TEXT("test01"), TEXT("1234"));

	//RequestServerListFromTcpServer();
}

bool URouteGameInstance::TravelToFirstServer()
{
	if (CachedServerList.Num() <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("CachedServerList is empty.ClientTravel canceled."));
		return false;
	}

	const FRouteServerInfo& ServerInfo = CachedServerList[0];

	if (ServerInfo.IpAddress.IsEmpty() || ServerInfo.Port <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid server address. ClientTravel canceled."));
		return false;
	}
	
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);

	if (!PlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerController is null. ClientTravel canceled"));
		return false;
	}

	const FString ServerAddress = FString::Printf(
		TEXT("%s:%d"),
		*ServerInfo.IpAddress,
		ServerInfo.Port
	);

	UE_LOG(LogTemp, Warning, TEXT("ClientTravel to cached server: %s"), *ServerAddress);

	PlayerController->ClientTravel(ServerAddress, TRAVEL_Absolute);

	return true;
}

bool URouteGameInstance::RequestServerListFromTcpServer()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	if (!SocketSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("SocketSubsystem is null"));
		return false;
	}

	FIPv4Address TcpServerIp;

	if (!FIPv4Address::Parse(TEXT("127.0.0.1"), TcpServerIp))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid TCPServer IP."));
		return false;
	}

	TSharedRef<FInternetAddr> TcpServerAddress = SocketSubsystem->CreateInternetAddr();

	TcpServerAddress->SetIp(TcpServerIp.Value);
	TcpServerAddress->SetPort(9000);

	FSocket* Socket = SocketSubsystem->CreateSocket(
		NAME_Stream,
		TEXT("RouteServerListSocket"),
		false
	);

	if (!Socket)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateSocket failed."));
		return false;
	}

	if (!Socket->Connect(*TcpServerAddress))
	{
		UE_LOG(LogTemp, Error, TEXT("Connect to TCPServer failed."));

		Socket->Close();
		SocketSubsystem->DestroySocket(Socket);

		return false;
	}

	const FString RequestMessage = TEXT("{\"type\":\"REQUEST_SERVER_LIST\"}\n");

	FTCHARToUTF8 ConvertedMessage(*RequestMessage);

	int32 BytesSent = 0;

	const bool bSent = Socket->Send(
		reinterpret_cast<const uint8*>(ConvertedMessage.Get()),
		ConvertedMessage.Length(),
		BytesSent
	);

	if (!bSent)
	{
		UE_LOG(LogTemp, Error, TEXT("Send REQUEST_SERVER_LIST failed."));

		Socket->Close();
		SocketSubsystem->DestroySocket(Socket);

		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("REQUEST_SERVER_LIST sent. Bytes: %d"), BytesSent);

	if (Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(2)))
	{
		uint8 ReceiveBuffer[4096];
		int32 BytesRead = 0;

		if (Socket->Recv(ReceiveBuffer, sizeof(ReceiveBuffer) - 1, BytesRead))
		{
			ReceiveBuffer[BytesRead] = '\0';
			const FString Response = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(ReceiveBuffer)));

			UE_LOG(LogTemp, Log, TEXT("Server List Response: %s"), *Response);

			ParseServerListResponse(Response);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No Response from TCPServer."));
	}

	Socket->Close();
	SocketSubsystem->DestroySocket(Socket);

	return false;
}

bool URouteGameInstance::ParseServerListResponse(const FString& Response)
{
	CachedServerList.Empty();

	TSharedPtr<FJsonObject> RootObject;

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);

	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("ParseServerListResponse failed. Invalid Json"));
		return false;
	}

	bool bSuccess = false;

	if (!RootObject->TryGetBoolField(TEXT("success"), bSuccess) || !bSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("Server list response success is false."));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ServersArray = nullptr;

	if (!RootObject->TryGetArrayField(TEXT("servers"), ServersArray))
	{
		UE_LOG(LogTemp, Error, TEXT("Server list response has no servers array."));
		return false;
	}

	for (int32 Index = 0; Index < ServersArray->Num(); ++Index)
	{
		const TSharedPtr<FJsonValue> ServerValue = (*ServersArray)[Index];

		if (!ServerValue.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("ServerValue[%d] is invalid."), Index);
			continue;
		}

		const TSharedPtr<FJsonObject>* ServerObject = nullptr;

		if (!ServerValue->TryGetObject(ServerObject) || !ServerObject || !ServerObject->IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("ServerObject[%d] is invalid."), Index);
			continue;
		}

		FRouteServerInfo ServerInfo;

		(*ServerObject)->TryGetStringField(TEXT("server_name"), ServerInfo.ServerName);
		(*ServerObject)->TryGetStringField(TEXT("ip_address"), ServerInfo.IpAddress);
		(*ServerObject)->TryGetNumberField(TEXT("port"), ServerInfo.Port);
		(*ServerObject)->TryGetNumberField(TEXT("current_players"), ServerInfo.CurrentPlayers);
		(*ServerObject)->TryGetNumberField(TEXT("max_players"), ServerInfo.MaxPlayers);
		(*ServerObject)->TryGetStringField(TEXT("status"), ServerInfo.Status);

		CachedServerList.Add(ServerInfo);

		UE_LOG(LogTemp, Warning, TEXT("Parsed Server[%d] Name: %s"), Index, *ServerInfo.ServerName);
		UE_LOG(LogTemp, Warning, TEXT("Parsed Server[%d] Address: %s:%d"), Index, *ServerInfo.IpAddress, ServerInfo.Port);
		UE_LOG(LogTemp, Warning, TEXT("Parsed Server[%d] Players: %d / %d"), Index, ServerInfo.CurrentPlayers, ServerInfo.MaxPlayers);
		UE_LOG(LogTemp, Warning, TEXT("Parsed Server[%d] Status: %s"), Index, *ServerInfo.Status);

	}

	UE_LOG(LogTemp, Log, TEXT("Parsed Server Count: %d"), CachedServerList.Num());

	return false;
}

void URouteGameInstance::SetNickname(const FString& NewNickname)
{
	Nickname = NewNickname;
}

FString URouteGameInstance::GetNickname() const
{
	return Nickname;
}

void URouteGameInstance::RequestLogin(const FString& LoginId, const FString& Password)
{
	TSharedRef<FJsonObject> RequestJson = MakeShared<FJsonObject>();

	RequestJson->SetStringField(TEXT("login_id"), LoginId);
	RequestJson->SetStringField(TEXT("password"), Password);

	FString RequestBody;

	TSharedRef<TJsonWriter<>> Writer =
		TJsonWriterFactory<>::Create(&RequestBody);

	FJsonSerializer::Serialize(RequestJson, Writer);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->SetURL(TEXT("http://127.0.0.1:8080/login"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestBody);

	HttpRequest->OnProcessRequestComplete().BindUObject(
		this,
		&URouteGameInstance::HandleLoginResponse
	);

	UE_LOG(LogTemp, Log, TEXT("Login request sent. LoginId: %s"), *LoginId);

	HttpRequest->ProcessRequest();
}

void URouteGameInstance::HandleLoginResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Login request failed."));
		return;
	}

	const FString ResponseBody = Response->GetContentAsString();

	UE_LOG(LogTemp, Log, TEXT("Login Response: %s"), *ResponseBody);

	TSharedPtr<FJsonObject> ResponseJson;

	const TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(ResponseBody);

	if (!FJsonSerializer::Deserialize(Reader, ResponseJson) || !ResponseJson.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Login response JSON parse failed."));
		return;
	}

	bool bSuccess = false;

	if (!ResponseJson->TryGetBoolField(TEXT("success"), bSuccess) || !bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("Login failed."));
		return;
	}

	ResponseJson->TryGetNumberField(TEXT("account_id"), AccountId);
	ResponseJson->TryGetStringField(TEXT("nickname"), Nickname);

	UE_LOG(LogTemp, Warning, TEXT("Login succeeded. AccountId: %d, Nickname: %s"), AccountId, *Nickname);

	RequestServerListFromTcpServer();
}
