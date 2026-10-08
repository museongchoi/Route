// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/RoutePlayerController.h"

#include "Framework/RouteGameInstance.h"
#include "Framework/RoutePlayerState.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"

#include "TimerManager.h"

void ARoutePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);

	bShowMouseCursor = false;

	ULocalPlayer* LocalPlayer = GetLocalPlayer();

	if (!LocalPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("LocalPlayer is null."));
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();

	if (!InputSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("EnhancedInputLocalPlayerSubsystem is null."));
		return;
	}

	if (!PlayerMappingContext)
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerMappingContext is null."));
		return;
	}

	InputSubsystem->AddMappingContext(PlayerMappingContext, 0);

	FTimerHandle TimerHandle;

	GetWorldTimerManager().SetTimer(
		TimerHandle,
		this,
		&ARoutePlayerController::SendNicknameToServer,
		1.0f,
		false
	);
}

void ARoutePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);

	if (!EnhancedInputComponent)
	{
		return;
	}

	if (PushToTalkAction)
	{
		EnhancedInputComponent->BindAction(
			PushToTalkAction,
			ETriggerEvent::Started,
			this,
			&ARoutePlayerController::StartVoiceTransmit
		);

		EnhancedInputComponent->BindAction(
			PushToTalkAction,
			ETriggerEvent::Completed,
			this,
			&ARoutePlayerController::StopVoiceTransmit
		);
	}
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

void ARoutePlayerController::StartVoiceTransmit()
{
	UE_LOG(LogTemp, Warning, TEXT("PTT Pressed - Start Voice Transmit"));

	URouteGameInstance* RouteGameInstance = Cast<URouteGameInstance>(GetGameInstance());

	if (!RouteGameInstance)
	{
		return;
	}

	RouteGameInstance->StartVoiceTransmit();

	ServerSetSpeaking(true);
}

void ARoutePlayerController::StopVoiceTransmit()
{
	UE_LOG(LogTemp, Warning, TEXT("PTT Released - Stop Voice Transmit"));

	URouteGameInstance* RouteGameInstance = Cast<URouteGameInstance>(GetGameInstance());

	if (!RouteGameInstance)
	{
		return;
	}

	RouteGameInstance->StopVoiceTransmit();

	ServerSetSpeaking(false);
}

void ARoutePlayerController::ServerSetSpeaking_Implementation(bool bNewIsSpeaking)
{
	UE_LOG(LogTemp, Warning, TEXT("ServerSetSpeaking called: %s"), bNewIsSpeaking ? TEXT("true") : TEXT("false"));

	ARoutePlayerState* RoutePlayerState = GetPlayerState<ARoutePlayerState>();

	if (!RoutePlayerState)
	{
		UE_LOG(LogTemp, Error, TEXT("RoutePlayerState is null. SetIsSpeaking failed."));

		return;
	}

	RoutePlayerState->SetIsSpeaking(bNewIsSpeaking);
}