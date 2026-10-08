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

// ===== Input =====
private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> PlayerMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> PushToTalkAction;

// ===== Nickname =====
private:
	void SendNicknameToServer();

	// Client Nickname 전달용 Server RPC
	UFUNCTION(Server, Reliable)
	void ServerSetNickname(const FString& NewNickname);

// ===== Voice =====
private:
	void StartVoiceTransmit();
	void StopVoiceTransmit();

	// Client Speaking 상태 전달용 Server RPC
	UFUNCTION(Server, Reliable)
	void ServerSetSpeaking(bool bNewIsSpeaking);

};
