// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/RoutePlayerController.h"

#include "Framework/RouteGameInstance.h"
#include "Framework/RoutePlayerState.h"

#include "TimerManager.h"

void ARoutePlayerController::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("RoutePlayerController BeginPlay"));

	if (!IsLocalController())
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("RoutePlayerController BeginPlay - Local Controller"));

	FTimerHandle TimerHandle;

	GetWorldTimerManager().SetTimer(
		TimerHandle,
		this,
		&ARoutePlayerController::SendNicknameToServer,
		1.0f,
		false
	);
}

void ARoutePlayerController::SendNicknameToServer()
{
	URouteGameInstance* RouteGameInstance = Cast<URouteGameInstance>(GetGameInstance());

	if (!RouteGameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("RouteGameInstance is null. SendNicknameToServer canceled."));
		return;
	}

	const FString Nickname = RouteGameInstance->GetNickname();

	if (Nickname.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Nickname is empty. SendNicknameToServer canceled."));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("SendNicknameToServer: %s"), *Nickname);

	ServerSetNickname(Nickname);
}

void ARoutePlayerController::ServerSetNickname_Implementation(const FString& NewNickname)
{
	UE_LOG(LogTemp, Warning, TEXT("ServerSetNickname called: %s"), *NewNickname);

	ARoutePlayerState* RoutePlayerState = GetPlayerState<ARoutePlayerState>();

	if (!RoutePlayerState)
	{
		UE_LOG(LogTemp, Error, TEXT("RoutePlayerState is null. SetNickname failed."));
		return;
	}

	RoutePlayerState->SetNickname(NewNickname);
}
