// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RoutePlayerController.generated.h"

/**
 * 
 */
UCLASS()
class ROUTE_API ARoutePlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;

private:
	void SendNicknameToServer();

	UFUNCTION(Server, Reliable)
	void ServerSetNickname(const FString& NewNickname);
};
