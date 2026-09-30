// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/RouteGameInstance.h"

#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"

#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

#include "VoiceChat.h"
#include "EOSVoiceChatTypes.h"

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

	//RequestLogin(TEXT("test01"), TEXT("1234"));

	//RequestServerListFromTcpServer();

	RequestEOSLogin();
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

	// Http 요청 완료 콜백 함수 호출
	HttpRequest->OnProcessRequestComplete().BindUObject(
		this,
		&URouteGameInstance::HandleLoginResponse
	);

	UE_LOG(LogTemp, Log, TEXT("Login request sent. LoginId: %s"), *LoginId);

	HttpRequest->ProcessRequest();
}

void URouteGameInstance::SetNickname(const FString& NewNickname)
{
	Nickname = NewNickname;
}

FString URouteGameInstance::GetNickname() const
{
	return Nickname;
}

const TArray<FRouteServerInfo>& URouteGameInstance::GetCachedServerList() const
{
	return CachedServerList;
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

	bool bRequestSucceeded = false;

	if (Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(2)))
	{
		uint8 ReceiveBuffer[4096];
		int32 BytesRead = 0;

		if (Socket->Recv(ReceiveBuffer, sizeof(ReceiveBuffer) - 1, BytesRead))
		{
			ReceiveBuffer[BytesRead] = '\0';
			const FString Response = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(ReceiveBuffer)));

			UE_LOG(LogTemp, Log, TEXT("Server List Response: %s"), *Response);

			bRequestSucceeded = ParseServerListResponse(Response);

			if (bRequestSucceeded)
			{
				OnServerListUpdatedDelegate.Broadcast();
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No Response from TCPServer."));
	}

	Socket->Close();
	SocketSubsystem->DestroySocket(Socket);

	return bRequestSucceeded;
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

	return true;
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

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);

	if (!FJsonSerializer::Deserialize(Reader, ResponseJson) || !ResponseJson.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Login response JSON parse failed."));
		return;
	}

	bool bSuccess = false;

	if (!ResponseJson->TryGetBoolField(TEXT("success"), bSuccess))
	{
		UE_LOG(LogTemp, Warning, TEXT("Login response has no success field."));
		return;
	}

	// Json 의 success : 로그인 인증 성공 여부
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("Login failed."));
		OnLoginResultDelegate.Broadcast(false, TEXT("Invalid ID or password."));

		return;
	}

	ResponseJson->TryGetNumberField(TEXT("account_id"), AccountId);
	ResponseJson->TryGetStringField(TEXT("nickname"), Nickname);
	ResponseJson->TryGetStringField(TEXT("session_token"), SessionToken);

	// 추가
	TryRegisterEosUser();

	UE_LOG(LogTemp, Warning, TEXT("Login succeeded. AccountId: %d, Nickname: %s"), AccountId, *Nickname);

	OnLoginResultDelegate.Broadcast(true, TEXT("Login succeeded"));

	RequestServerListFromTcpServer();
}

void URouteGameInstance::RequestEOSLogin()
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get(TEXT("EOS"));

	if (!OnlineSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("EOS OnlineSubsystem is null."));
		return;
	}

	IOnlineIdentityPtr Identity = OnlineSubsystem->GetIdentityInterface();

	if (!Identity.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("EOS Identity Interface is invalid."));
		return;
	}

	EOSLoginCompleteDelegateHandle = Identity->AddOnLoginCompleteDelegate_Handle(0,
		FOnLoginCompleteDelegate::CreateUObject(this, &URouteGameInstance::HandleEOSLoginComplete)
	);

	FOnlineAccountCredentials Credentials;
	Credentials.Type = TEXT("Developer");
	Credentials.Id = TEXT("localhost:6666");
	Credentials.Token = TEXT("Player1");

	Identity->Login(0, Credentials);
}

FString URouteGameInstance::GetEosProductUserId() const
{
	return EosProductUserId;
}

void URouteGameInstance::HandleEOSLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get(TEXT("EOS"));

	if (!OnlineSubsystem)
	{
		return;
	}

	IOnlineIdentityPtr Identity = OnlineSubsystem->GetIdentityInterface();

	if (Identity.IsValid())
	{
		Identity->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, EOSLoginCompleteDelegateHandle);
	}

	if (!bWasSuccessful)
	{
		UE_LOG(LogTemp, Error, TEXT("EOS Login failed: %s"), *Error);
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("EOS Login succeeded. UserId: %s"), *UserId.ToString());

	const FString UniqueIdString = UserId.ToString();

	FString EpicAccountId;
	FString ProductUserId;

	if (!UniqueIdString.Split(TEXT("|"), &EpicAccountId, &ProductUserId))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to extract EOS Product User ID."));
		return;
	}

	EosProductUserId = ProductUserId;

	UE_LOG(LogTemp, Warning, TEXT("EOS Login succeeded. PUID: %s"), *EosProductUserId);

	// BackendServer에 Route Account ↔ EOS PUID 매핑
	TryRegisterEosUser();

	// EOS VoiceChat 초기화 시작
	InitializeVoiceChat();
}

void URouteGameInstance::OnEOSLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	UE_LOG(LogTemp, Log, TEXT("EOS Login Complete | Success: %s | UserId: %s | Error: %s"), bWasSuccessful ? TEXT("true") : TEXT("false"), *UserId.ToString(), *Error);
}

void URouteGameInstance::TryRegisterEosUser()
{
	if (AccountId <= 0)
	{
		return;
	}

	if (EosProductUserId.IsEmpty())
	{
		return;
	}

	if (SessionToken.IsEmpty())
	{
		return;
	}

	if (bEosUserRegisterRequested)
	{
		return;
	}

	bEosUserRegisterRequested = true;

	RequestRegisterEosUser();
}

void URouteGameInstance::RequestRegisterEosUser()
{
	TSharedRef<FJsonObject> RequestJson = MakeShared<FJsonObject>();

	//RequestJson->SetNumberField(TEXT("account_id"), AccountId);

	RequestJson->SetStringField(TEXT("eos_puid"), EosProductUserId);

	
	FString RequestBody;

	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);

	FJsonSerializer::Serialize(RequestJson, Writer);

	UE_LOG(LogTemp, Warning, TEXT("Voice Register Request Body: %s"), *RequestBody);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->SetURL(TEXT("http://127.0.0.1:8080/voice/register-user"));

	HttpRequest->SetVerb(TEXT("POST"));

	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *SessionToken));

	HttpRequest->SetContentAsString(RequestBody);

	HttpRequest->OnProcessRequestComplete().BindUObject(
		this,
		&URouteGameInstance::HandleRegisterEosUserResponse
	);

	if (!HttpRequest->ProcessRequest())
	{
		bEosUserRegisterRequested = false;
		UE_LOG(LogTemp, Error, TEXT("EOS user register request failed to start."));
	}

}

void URouteGameInstance::HandleRegisterEosUserResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		bEosUserRegisterRequested = false;

		UE_LOG(LogTemp, Error, TEXT("EOS user register request failed."));
		return;
	}

	const FString ResponseBody = Response->GetContentAsString();

	TSharedPtr<FJsonObject> ResponseJson;

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);

	if (!FJsonSerializer::Deserialize(Reader, ResponseJson) || !ResponseJson.IsValid())
	{
		bEosUserRegisterRequested = false;

		UE_LOG(LogTemp, Error, TEXT("EOS user register response JSON parse failed."));
		return;
	}

	bool bSuccess = false;

	if (!ResponseJson->TryGetBoolField(TEXT("success"), bSuccess) || !bSuccess)
	{
		bEosUserRegisterRequested = false;

		UE_LOG(LogTemp, Error, TEXT("EOS user register failed."));
		return;
	}

	bEosUserRegistered = true;

	UE_LOG(LogTemp, Warning, TEXT("EOS user register succeeded."));

	TryRequestVoiceJoin();
}

void URouteGameInstance::InitializeVoiceChat()
{
	VoiceChat = IVoiceChat::Get();

	if (!VoiceChat)
	{
		UE_LOG(LogTemp, Error, TEXT("VoiceChat is null"));
		return;
	}

	if (!VoiceChat->Initialize())
	{
		UE_LOG(LogTemp, Error, TEXT("VoiceChat initialize failed."));
		return;
	}

	VoiceChatUser = VoiceChat->CreateUser();

	if (!VoiceChatUser)
	{
		UE_LOG(LogTemp, Error, TEXT("VoiceChatUser create failed."));
		return;
	}

	VoiceChat->Connect(FOnVoiceChatConnectCompleteDelegate::CreateUObject(
		this, 
		&URouteGameInstance::HandleVoiceChatConnectComplete
	));
}

void URouteGameInstance::HandleVoiceChatConnectComplete(const FVoiceChatResult& Result)
{
	if (!Result.IsSuccess())
	{
		UE_LOG(LogTemp, Error, TEXT("VoiceChat connect failed: %s"), *Result.ErrorDesc);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("VoiceChat connect succeeded."));

	if (!VoiceChatUser || EosProductUserId.IsEmpty())
	{
		return;
	}

	VoiceChatUser->Login(FPlatformUserId::CreateFromInternalId(0), EosProductUserId, TEXT(""),
		FOnVoiceChatLoginCompleteDelegate::CreateUObject(
			this,
			&URouteGameInstance::HandleVoiceChatLoginComplete
		)
	);

}

void URouteGameInstance::HandleVoiceChatLoginComplete(const FString& PlayerName, const FVoiceChatResult& Result)
{
	if (!Result.IsSuccess())
	{
		UE_LOG(LogTemp, Error, TEXT("VoiceChat login failed: %s"), *Result.ErrorDesc);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("VoiceChat login succeeded."));

	bVoiceChatLoggedIn = true;
	TryRequestVoiceJoin();
}

void URouteGameInstance::RequestVoiceJoin()
{
	if (SessionToken.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Voice join canceled. SessionToken is empty."));
		return;
	}

	//EndPoint 는 Content-Type 이나 JSON Body 요구 x.
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->SetURL(TEXT("http://127.0.0.1:8080/voice/join"));

	HttpRequest->SetVerb(TEXT("POST"));

	HttpRequest->SetHeader(TEXT("Authorization"),
		FString::Printf(
			TEXT("Bearer %s"),
			*SessionToken
		)
	);

	HttpRequest->OnProcessRequestComplete().BindUObject(
		this,
		&URouteGameInstance::HandleVoiceJoinResponse
	);

	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogTemp, Error, TEXT("Voice join request failed to start."));
	}
}

void URouteGameInstance::HandleVoiceJoinResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Voice join HTTP request failed."));
		return;
	}

	if (Response->GetResponseCode() != 200)
	{
		UE_LOG(LogTemp, Error, TEXT("Voice join failed. HTTP Code : %d"), Response->GetResponseCode());
		return;
	}

	TSharedPtr<FJsonObject> ResponseJson;

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

	if (!FJsonSerializer::Deserialize(Reader, ResponseJson) || !ResponseJson.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Voice join response JSON parse failed."));
		return;
	}

	bool bSuccess = false;

	if (!ResponseJson->TryGetBoolField(TEXT("success"), bSuccess) || !bSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("Voice join response success=false."));
		return;
	}

	if (!ResponseJson->TryGetStringField(TEXT("room_name"), VoiceRoomName) ||
		!ResponseJson->TryGetStringField(TEXT("client_base_url"), VoiceClientBaseUrl) ||
		!ResponseJson->TryGetStringField(TEXT("participant_token"), VoiceParticipantToken))
	{
		UE_LOG(LogTemp, Error, TEXT("Voice join response field missing."));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Voice join credential received. Room: %s"), *VoiceRoomName);

	if (!VoiceChatUser)
	{
		UE_LOG(LogTemp, Error, TEXT("VoiceChatUser is null."));
		return;
	}

	FEOSVoiceChatChannelCredentials ChannelCredentials;

	ChannelCredentials.ClientBaseUrl = VoiceClientBaseUrl;
	ChannelCredentials.ParticipantToken = VoiceParticipantToken;

	VoiceChatUser->JoinChannel(
		VoiceRoomName,
		ChannelCredentials.ToJson(),
		EVoiceChatChannelType::NonPositional,
		FOnVoiceChatChannelJoinCompleteDelegate::CreateUObject(this, &URouteGameInstance::HandleVoiceChannelJoinComplete)
	);
}

void URouteGameInstance::TryRequestVoiceJoin()
{
	if (!bVoiceChatLoggedIn)
	{
		return;
	}

	if (SessionToken.IsEmpty())
	{
		return;
	}

	if (!bEosUserRegistered)
	{
		return;
	}

	if (bVoiceJoinRequested)
	{
		return;
	}

	bVoiceJoinRequested = true;

	RequestVoiceJoin();
}

void URouteGameInstance::HandleVoiceChannelJoinComplete(const FString& ChannelName, const FVoiceChatResult& Result)
{
	if (!Result.IsSuccess())
	{
		UE_LOG(LogTemp, Error, TEXT("Voice channel join failed. Channel: %s, Error: %s"), *ChannelName, *Result.ErrorDesc);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Voice channel join succeeded. Channel: % s"), *ChannelName);
	if (VoiceChatUser)
	{
		VoiceChatUser->TransmitToNoChannels();
	}
}

void URouteGameInstance::StartVoiceTransmit()
{
	if (!VoiceChatUser || VoiceRoomName.IsEmpty())
	{
		return;
	}

	TSet<FString> Channels;
	Channels.Add(VoiceRoomName);

	VoiceChatUser->TransmitToSpecificChannels(Channels);
}

void URouteGameInstance::StopVoiceTransmit()
{
	if (!VoiceChatUser)
	{
		return;
	}

	VoiceChatUser->TransmitToNoChannels();
}
