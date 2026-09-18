// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/RouteLoginGameMode.h"

#include "Framework/RouteLoginPlayerController.h"

ARouteLoginGameMode::ARouteLoginGameMode()
{
	PlayerControllerClass = ARouteLoginPlayerController::StaticClass();
}
