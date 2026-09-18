// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Data/RouteServerInfo.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "RouteGameInstance.generated.h"

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

private:
	bool RequestServerListFromTcpServer();

	bool ParseServerListResponse(const FString& Response);

public:
	UPROPERTY(BlueprintReadOnly)
	TArray<FRouteServerInfo> CachedServerList;

public:
	void SetNickname(const FString& NewNickname);
	FString GetNickname() const;

	void RequestLogin(const FString& LoginId, const FString& Password);

private:
	void HandleLoginResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

private:
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	int32 AccountId = 0;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FString Nickname;
};
