// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/RouteCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"

#include "EnhancedInputComponent.h"

// Sets default values
ARouteCharacter::ARouteCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;


	VoiceIndicatorWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("VoiceIndicatorWidget"));
	VoiceIndicatorWidget->SetupAttachment(RootComponent);

	VoiceIndicatorWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));

	VoiceIndicatorWidget->SetWidgetSpace(EWidgetSpace::Screen);
	VoiceIndicatorWidget->SetDrawSize(FVector2D(64.0f, 64.0f));

	VoiceIndicatorWidget->SetVisibility(false);
}

void ARouteCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);

	if (!EnhancedInputComponent)
	{
		return;
	}

	if (MoveAction)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ARouteCharacter::Move);
	}
}

void ARouteCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (!Controller)
	{
		return;
	}

	const FRotator ControlRotation = Controller->GetControlRotation();

	const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, MovementVector.Y);
	AddMovementInput(RightDirection, MovementVector.X);
}

void ARouteCharacter::UpdateVoiceIndicator(bool bIsSpeaking)
{
	if (!VoiceIndicatorWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("VoiceIndicatorWidget is null"));
		return;
	}

	VoiceIndicatorWidget->SetVisibility(bIsSpeaking);
}

