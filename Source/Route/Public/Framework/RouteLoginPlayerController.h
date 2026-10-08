// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RouteLoginPlayerController.generated.h"

class URouteLoginWidget;
class URouteServerListWidget;
class URouteRegisterWidget;
/**
 * 
 */
UCLASS()
class ROUTE_API ARouteLoginPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;

// ===== Login UI =====
public:
	void ShowLoginWidget();

private:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<URouteLoginWidget> LoginWidgetClass;

	UPROPERTY()
	TObjectPtr<URouteLoginWidget> LoginWidget;

// ===== Register UI =====
public:
	void ShowRegisterWidget();

private:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<URouteRegisterWidget> RegisterWidgetClass;

	UPROPERTY()
	TObjectPtr<URouteRegisterWidget> RegisterWidget;

// ===== Server List UI =====
private:
	// Server List 갱신 완료 Delegate Callback
	UFUNCTION()
	void HandleServerListUpdated();

private:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<URouteServerListWidget> ServerListWidgetClass;

	UPROPERTY()
	TObjectPtr<URouteServerListWidget> ServerListWidget;

};
