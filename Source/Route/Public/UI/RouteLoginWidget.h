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

private:
	UFUNCTION()
	void OnLoginClicked();

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> EditableTextBox_ID;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> EditableTextBox_Password;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Login;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Status;


private:
	UFUNCTION()
	void HandleLoginResult(bool bSuccess, const FString& Message);
};
