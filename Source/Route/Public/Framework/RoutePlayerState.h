// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "RoutePlayerState.generated.h"

/**
 * 
 */
UCLASS()
class ROUTE_API ARoutePlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ARoutePlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	void SetNickname(const FString& NewNickname);
	
	FString GetNickname() const;

private:
	UFUNCTION()
	void OnRep_Nickname();

private:
	UPROPERTY(ReplicatedUsing = OnRep_Nickname, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FString Nickname;
};
