// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RouteLoginPlayerController.generated.h"

class URouteLoginWidget;
class URouteServerListWidget;
/**
 * 
 */
UCLASS()
class ROUTE_API ARouteLoginPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleServerListUpdated();

private:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<URouteLoginWidget> LoginWidgetClass;

	UPROPERTY()
	TObjectPtr<URouteLoginWidget> LoginWidget;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<URouteServerListWidget> ServerListWidgetClass;

	UPROPERTY()
	TObjectPtr<URouteServerListWidget> ServerListWidget;
};
