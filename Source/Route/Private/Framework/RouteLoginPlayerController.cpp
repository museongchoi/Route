// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/RouteLoginPlayerController.h"

#include "Framework/RouteGameInstance.h"
#include "UI/RouteLoginWidget.h"
#include "UI/RouteServerListWidget.h"
#include "UI/RouteRegisterWidget.h"

void ARouteLoginPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 로그인 UI 는 로컬 클라이언트 화면에만 필요.
	if (!IsLocalController())
	{
		return;
	}

	URouteGameInstance* RouteGameInstance = Cast<URouteGameInstance>(GetGameInstance());

	if (!RouteGameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("RouteGameInstance is null."));
		return;
	}

	RouteGameInstance->OnServerListUpdatedDelegate.AddDynamic(this, &ARouteLoginPlayerController::HandleServerListUpdated);

	ShowLoginWidget();

	bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	SetInputMode(InputMode);
}

void ARouteLoginPlayerController::ShowLoginWidget()
{
	if (RegisterWidget)
	{
		RegisterWidget->RemoveFromParent();
		RegisterWidget = nullptr;
	}

	if (!LoginWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("LoginWidgetClass is null."));
		return;
	}

	LoginWidget = CreateWidget<URouteLoginWidget>(this, LoginWidgetClass);

	if (!LoginWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create LoginWidget."));
		return;
	}

	LoginWidget->AddToViewport();
}

void ARouteLoginPlayerController::ShowRegisterWidget()
{
	if (LoginWidget)
	{
		LoginWidget->RemoveFromParent();
		LoginWidget = nullptr;
	}

	if (!RegisterWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("RegisterWidgetClass is null."));
		return;
	}

	RegisterWidget = CreateWidget<URouteRegisterWidget>(
		this,
		RegisterWidgetClass
	);

	if (!RegisterWidget)
	{
		return;
	}

	RegisterWidget->AddToViewport();
}

void ARouteLoginPlayerController::HandleServerListUpdated()
{
	if (LoginWidget)
	{
		LoginWidget->RemoveFromParent();
		LoginWidget = nullptr;
	}

	if (!ServerListWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("ServerListWidgetClass is null."));
		return;
	}

	ServerListWidget = CreateWidget<URouteServerListWidget>(this, ServerListWidgetClass);

	if (!ServerListWidget)
	{
		return;
	}

	ServerListWidget->AddToViewport();

	ServerListWidget->RefreshServerList();

	FInputModeUIOnly InputMode;
	SetInputMode(InputMode);
}


