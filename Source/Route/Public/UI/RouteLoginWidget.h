// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RouteLoginWidget.generated.h"

class UEditableTextBox;
class UButton;
class UTextBlock;

/**
 * 
 */
UCLASS()
class ROUTE_API URouteLoginWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;

// ===== Login =====
private:
	UFUNCTION()
	void OnLoginClicked();

	// Backend 로그인 결과 Delegate Callback
	UFUNCTION()
	void HandleLoginResult(bool bSuccess, const FString& Message);

// ===== Register Navigation =====
private:
	UFUNCTION()
	void OnRegisterClicked();

// ===== Widget Components =====
private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> EditableTextBox_ID;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> EditableTextBox_Password;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Login;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Register;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Status;

};
