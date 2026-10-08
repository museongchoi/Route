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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRegisterResultDelegate, bool, bSuccess, const FString&, Message);
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

// ===== Account =====
// Backend 로그인 or 회원가입 요청
public:
	void RequestLogin(const FString& LoginId, const FString& Password);
	void RequestRegister(const FString& LoginId, const FString& Password, const FString& NewNickname);

	void SetNickname(const FString& NewNickname);
	FString GetNickname() const;

private:
	void HandleLoginResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandleRegisterResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

private:
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	int32 AccountId = 0;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FString Nickname;

	// Backend 인증 토큰
	// EOS PUID 등록 및 Voice Join 요청의 Bearer Token으로 사용
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FString SessionToken;

// ===== Account Delegate =====
public:
	UPROPERTY(BlueprintAssignable)
	FOnLoginResultDelegate OnLoginResultDelegate;

	UPROPERTY(BlueprintAssignable)
	FOnRegisterResultDelegate OnRegisterResultDelegate;

// ===== Server List =====
public:
	const TArray<FRouteServerInfo>& GetCachedServerList() const;
	
	bool TravelToFirstServer();

private:
	// TCPServer에 OPEN 상태 서버 목록 요청
	bool RequestServerListFromTcpServer();

	// TCPServer 응답 JSON을 CachedServerList로 변환
	bool ParseServerListResponse(const FString& Response);

private:
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TArray<FRouteServerInfo> CachedServerList;

public:
	UPROPERTY(BlueprintAssignable)
	FOnServerListUpdatedDelegate OnServerListUpdatedDelegate;

// ===== EOS Login =====
public:
	// EOS Developer Login 요청
	void RequestEOSLogin();

	FString GetEosProductUserId() const;

private:
	void HandleEOSLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);

private:
	FDelegateHandle EOSLoginCompleteDelegateHandle;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FString EosProductUserId;

	// CommandLine -EnableEOSVoice 활성화 여부
	bool bEnableEOSVoice = false;

// ===== EOS User Registration =====
private:
	// Route Account와 EOS PUID 등록 조건 확인
	void TryRegisterEosUser();

	// Backend에 EOS PUID 등록 요청
	void RequestRegisterEosUser();

	void HandleRegisterEosUserResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

private:
	// EOS PUID 등록 요청 중복 방지
	bool bEosUserRegisterRequested = false;

	// Backend EOS PUID 등록 여부
	bool bEosUserRegistered = false;  
	
// ===== Voice Chat =====
private:
	// EOS VoiceChat 초기화
	void InitializeVoiceChat();

	void HandleVoiceChatConnectComplete(const FVoiceChatResult& Result);

	void HandleVoiceChatLoginComplete(const FString& PlayerName, const FVoiceChatResult& Result);

private:
	// EOS VoiceChat 시스템 인터페이스
	IVoiceChat* VoiceChat = nullptr;

	// 로컬 플레이어 VoiceChat 인터페이스
	IVoiceChatUser* VoiceChatUser = nullptr;

	bool bVoiceChatLoggedIn = false;


// ===== Voice Room =====
private:
	// EOS User 등록 + VoiceChat Login 완료 후 Join 요청
	void TryRequestVoiceJoin();

	// Backend에 Voice Room 참가 Credential 요청
	void RequestVoiceJoin();

	void HandleVoiceJoinResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	// 채널 참가 성공 콜백
	void HandleVoiceChannelJoinComplete(const FString& ChannelName, const FVoiceChatResult& Result);

private:
	FString VoiceRoomName;
	FString VoiceClientBaseUrl;
	FString VoiceParticipantToken;

	bool bVoiceJoinRequested = false; // 이미 Join 요청을 보냈음

// ===== Voice Transmit =====
public:
	void StartVoiceTransmit();
	void StopVoiceTransmit();

public:
	//const FString& GetSessionToken() const { return SessionToken; }

};
