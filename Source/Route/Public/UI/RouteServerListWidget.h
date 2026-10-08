// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RouteServerListWidget.generated.h"

class UVerticalBox;
class URouteServerEntryWidget;

/**
 * 
 */
UCLASS()
class ROUTE_API URouteServerListWidget : public UUserWidget
{
	GENERATED_BODY()

// ===== Server List =====
// GameInstance의 CachedServerList를 UI Entry로 생성
public:
	void RefreshServerList();

// ===== Widget Components =====
private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> VerticalBox_ServerList;

// ===== Widget Class =====
private:
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<URouteServerEntryWidget> ServerEntryWidgetClass;

};
