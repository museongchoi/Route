// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RouteGameMode.generated.h"

/**
 * 
 */
UCLASS()
class ROUTE_API ARouteGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARouteGameMode();
	
protected:
	virtual void BeginPlay() override;

	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

	virtual void PostLogin(APlayerController* NewPlayer) override;

	virtual void Logout(AController* ExitingPlayer) override;

private:
	bool RegisterServerToTcpServer();
	bool UpdateServerToTcpServer();
	void SendHeartbeatToTcpServer();

private:
	FTimerHandle HeartbeatTimerHandle;

	FString ServerName = TEXT("RouteServer01");
	FString ServerIpAddress = TEXT("127.0.0.1");

	int32 ServerPort = 7777;
	int32 CurrentPlayers = 0;
	int32 MaxPlayers = 3;

};
