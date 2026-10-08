// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/RouteServerInfo.h"
#include "RouteServerEntryWidget.generated.h"

class UButton;
class UTextBlock;
/**
 * 
 */
UCLASS()
class ROUTE_API URouteServerEntryWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

// ===== Server Info =====
public:
	void SetServerInfo(const FRouteServerInfo& InServerInfo);

private:
	FRouteServerInfo ServerInfo;

// ===== Server Connection =====
private:
	UFUNCTION()
	void OnConnectClicked();

// ===== Widget Components =====
private:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_ServerName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_PlayerCount;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Connect;
};
