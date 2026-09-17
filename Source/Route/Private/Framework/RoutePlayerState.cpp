// Fill out your copyright notice in the Description page of Project Settings.

#include "Framework/RoutePlayerState.h"

#include "Net/UnrealNetwork.h"

ARoutePlayerState::ARoutePlayerState()
{
	Nickname = TEXT("");
}

void ARoutePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARoutePlayerState, Nickname);
}

void ARoutePlayerState::SetNickname(const FString& NewNickname)
{
	Nickname = NewNickname;

	UE_LOG(LogTemp, Warning, TEXT("RoutePlayerState Nickname Set : %s"), *Nickname)
}

FString ARoutePlayerState::GetNickname() const
{
	return Nickname;
}

void ARoutePlayerState::OnRep_Nickname()
{
	UE_LOG(LogTemp, Warning, TEXT("RoutePlayerState OnRep_Nickname: %s"), *Nickname);
}
