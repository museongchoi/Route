// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/RouteLoginWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"

#include "Framework/RouteGameInstance.h"
#include "Framework/RouteLoginPlayerController.h"

void URouteLoginWidget::NativeConstruct()
{
	Super::NativeConstruct();

	Button_Login->OnClicked.AddDynamic(this, &URouteLoginWidget::OnLoginClicked);
	Button_Register->OnClicked.AddDynamic(this, &URouteLoginWidget::OnRegisterClicked);

	URouteGameInstance* RouteGameInstance = Cast<URouteGameInstance>(GetGameInstance());

	if (!RouteGameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("RouteGameInstance is null."));
		return;
	}

	RouteGameInstance->OnLoginResultDelegate.AddDynamic(this, &URouteLoginWidget::HandleLoginResult);
}

void URouteLoginWidget::OnLoginClicked()
{
	const FString LoginID = EditableTextBox_ID->GetText().ToString();

	const FString Password = EditableTextBox_Password->GetText().ToString();

	URouteGameInstance* RouteGameInstance = Cast<URouteGameInstance>(GetGameInstance());

	if (!RouteGameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("RouteGameInstance is null. Login request canceled."));
		return;
	}

	RouteGameInstance->RequestLogin(LoginID, Password);

}

void URouteLoginWidget::HandleLoginResult(bool bSuccess, const FString& Message)
{
	Text_Status->SetText(FText::FromString(Message));

}

void URouteLoginWidget::OnRegisterClicked()
{
	ARouteLoginPlayerController* LoginPlayerController = Cast<ARouteLoginPlayerController>(GetOwningPlayer());

	if (!LoginPlayerController)
	{
		return;
	}

	LoginPlayerController->ShowRegisterWidget();
}


