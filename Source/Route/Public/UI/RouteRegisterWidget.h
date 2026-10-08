// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RouteRegisterWidget.generated.h"

class UEditableTextBox;
class UButton;
class UTextBlock;

UCLASS()
class ROUTE_API URouteRegisterWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

// ===== Register =====
private:
	UFUNCTION()
	void OnRegisterClicked();

	// 결과 처리 함수
	UFUNCTION()
	void HandleRegisterResult(bool bSuccess, const FString& Message);

// ===== Back Navigation =====
private:
	UFUNCTION()
	void OnBackClicked();

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> EditableTextBox_ID;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> EditableTextBox_Password;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> EditableTextBox_Nickname;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Register;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Status;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Back;
};
