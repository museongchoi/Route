// Fill out your copyright notice in the Description page of Project Settings.

#include "Framework/RoutePlayerState.h"
#include "Character/RouteCharacter.h"

#include "Net/UnrealNetwork.h"

ARoutePlayerState::ARoutePlayerState()
{
	bReplicates = true;

	Nickname = TEXT("");
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

void ARoutePlayerState::SetIsSpeaking(bool bNewIsSpeaking)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bIsSpeaking == bNewIsSpeaking)
	{
		return;
	}

	bIsSpeaking = bNewIsSpeaking;

	UE_LOG(LogTemp, Warning, TEXT("Speaking State Changed. Player: %s, IsSpeaking: %s"), *GetPlayerName(), bIsSpeaking ? TEXT("true") : TEXT("false"));
}

void ARoutePlayerState::OnRep_IsSpeaking()
{
	UE_LOG(LogTemp, Warning, TEXT("OnRep_IsSpeaking. Player: %s, IsSpeaking: %s"), *GetPlayerName(), bIsSpeaking ? TEXT("true") : TEXT("false"));

	ARouteCharacter* RouteCharacter = GetPawn<ARouteCharacter>();

	if (!RouteCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("RouteCharacter is null. Voice Indicator update skipped."));
		return;
	}

	RouteCharacter->UpdateVoiceIndicator(bIsSpeaking);
}

void ARoutePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARoutePlayerState, Nickname);
	DOREPLIFETIME(ARoutePlayerState, bIsSpeaking);
}

