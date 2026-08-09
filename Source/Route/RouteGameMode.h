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

private:
	bool RegisterServerToTcpServer();
};
