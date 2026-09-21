// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/RouteLoginPlayerController.h"

#include "Framework/RouteGameInstance.h"
#include "UI/RouteLoginWidget.h"
#include "UI/RouteServerListWidget.h"

void ARouteLoginPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 로그인 UI 는 로컬 클라이언트 화면에만 필요.
	if (!IsLocalController())
	{
		return;
	}

	URouteGameInstance* RouteGameInstance = Cast<URouteGameInstance>(GetGameInstance());

	if (RouteGameInstance)
	{
		RouteGameInstance->OnServerListUpdatedDelegate.AddDynamic(this, &ARouteLoginPlayerController::HandleServerListUpdated);
	}

	if (!LoginWidgetClass)
	{
		return;
	}

	LoginWidget = CreateWidget<URouteLoginWidget>(this, LoginWidgetClass);

	if (!LoginWidget)
	{
		return;
	}

	LoginWidget->AddToViewport();

	bShowMouseCursor = true;

	FInputModeUIOnly InputMode;
	SetInputMode(InputMode);
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
