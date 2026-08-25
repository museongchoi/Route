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
	
protected:
	virtual void BeginPlay() override;

	virtual void PostLogin(APlayerController* NewPlayer) override;

	virtual void Logout(AController* ExitingPlayer) override;

private:
	bool RegisterServerToTcpServer();

	//bool UpdateServerToTcpServer();

private:
	//int32 CurrentPlayers = 0;
	//int32 MaxPlayers = 3;
};
