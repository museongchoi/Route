// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RoutePlayerController.generated.h"

class UInputMappingContext;
class UInputAction;

UCLASS()
class ROUTE_API ARoutePlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;

	virtual void SetupInputComponent() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> PlayerMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> PushToTalkAction;

private:
	void StartVoiceTransmit();
	void StopVoiceTransmit();

	void SendNicknameToServer();

	UFUNCTION(Server, Reliable)
	void ServerSetNickname(const FString& NewNickname);
};
