// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Data/RouteServerInfo.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "RouteGameInstance.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoginResultDelegate, bool, bSuccess, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnServerListUpdatedDelegate);

class IVoiceChat;
class IVoiceChatUser;
struct FVoiceChatResult; 

UCLASS()
class ROUTE_API URouteGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	virtual void Init() override;

	bool TravelToFirstServer();

	void RequestLogin(const FString& LoginId, const FString& Password);

	void SetNickname(const FString& NewNickname);
	FString GetNickname() const;

	const TArray<FRouteServerInfo>& GetCachedServerList() const;

private:
	bool RequestServerListFromTcpServer();
	bool ParseServerListResponse(const FString& Response);

	void HandleLoginResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

public:
	void RequestEOSLogin();
	FString GetEosProductUserId() const;

private:
	void HandleEOSLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);

	void OnEOSLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);

	void TryRegisterEosUser();

	void RequestRegisterEosUser();

	void HandleRegisterEosUserResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

public:
	UPROPERTY(BlueprintAssignable)
	FOnLoginResultDelegate OnLoginResultDelegate;

	UPROPERTY(BlueprintAssignable)
	FOnServerListUpdatedDelegate OnServerListUpdatedDelegate;

	FDelegateHandle EOSLoginCompleteDelegateHandle;

private:
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	int32 AccountId = 0;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FString Nickname;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TArray<FRouteServerInfo> CachedServerList;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FString EosProductUserId;

private:
	bool bEosUserRegisterRequested = false;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FString SessionToken;

public:
	//const FString& GetSessionToken() const { return SessionToken; }

private:
	IVoiceChat* VoiceChat = nullptr;
	IVoiceChatUser* VoiceChatUser = nullptr;

	void InitializeVoiceChat();

	void HandleVoiceChatConnectComplete(const FVoiceChatResult& Result);

	void HandleVoiceChatLoginComplete(const FString& PlayerName, const FVoiceChatResult& Result);

	void RequestVoiceJoin();

	void HandleVoiceJoinResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	FString VoiceRoomName;
	FString VoiceClientBaseUrl;
	FString VoiceParticipantToken;

	bool bVoiceChatLoggedIn = false;  // VoiceChat 준비됨
	bool bVoiceJoinRequested = false; // 이미 Join 요청을 보냈음
	bool bEosUserRegistered = false;  // BackendServer PUID 등록됨

	void TryRequestVoiceJoin();

	// 채널 참가 성공 콜백
	void HandleVoiceChannelJoinComplete(const FString& ChannelName, const FVoiceChatResult& Result);

public:
	void StartVoiceTransmit();
	void StopVoiceTransmit();

};
