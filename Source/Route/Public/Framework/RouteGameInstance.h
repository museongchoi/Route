// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Data/RouteServerInfo.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "RouteGameInstance.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoginResultDelegate, bool, bSuccess, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnServerListUpdatedDelegate);
/**
 * 
 */
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
	UPROPERTY(BlueprintAssignable)
	FOnLoginResultDelegate OnLoginResultDelegate;

	UPROPERTY(BlueprintAssignable)
	FOnServerListUpdatedDelegate OnServerListUpdatedDelegate;

private:
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	int32 AccountId = 0;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FString Nickname;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TArray<FRouteServerInfo> CachedServerList;
};
