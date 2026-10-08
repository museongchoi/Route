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

// ===== Nickname =====
public:
	void SetNickname(const FString& NewNickname);
	FString GetNickname() const;

private:
	// Nickname Replication Callback
	UFUNCTION()
	void OnRep_Nickname();

private:
	UPROPERTY(ReplicatedUsing = OnRep_Nickname, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FString Nickname;

// ===== Voice =====
public:
	bool IsSpeaking() const { return bIsSpeaking; }
	void SetIsSpeaking(bool bNewIsSpeaking);

protected:
	// Speaking 상태 Replication Callback
	UFUNCTION()
	void OnRep_IsSpeaking();

protected:
	UPROPERTY(ReplicatedUsing = OnRep_IsSpeaking, BlueprintReadOnly, Category = "Voice")
	bool bIsSpeaking = false;

// ===== Replication =====
protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

};
