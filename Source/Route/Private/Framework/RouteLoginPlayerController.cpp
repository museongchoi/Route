// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/RouteLoginPlayerController.h"

#include "UI/RouteLoginWidget.h"

void ARouteLoginPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 로그인 UI 는 로컬 클라이언트 화면에만 필요.
	if (!IsLocalController())
	{
		return;
	}

	if (!LoginWidgetClass)
	{
		return;
	}

	LoginWidget = CreateWidget<URouteLoginWidget>(
		this,
		LoginWidgetClass
	);

	if (!LoginWidget)
	{
		return;
	}

	LoginWidget->AddToViewport();

	bShowMouseCursor = true;

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(LoginWidget->TakeWidget());

	SetInputMode(InputMode);
}
