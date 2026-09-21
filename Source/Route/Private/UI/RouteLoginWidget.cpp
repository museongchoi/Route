// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/RouteLoginWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"

#include "Framework/RouteGameInstance.h"

void URouteLoginWidget::NativeConstruct()
{
	Super::NativeConstruct();

	Button_Login->OnClicked.AddDynamic(this, &URouteLoginWidget::OnLoginClicked);

	URouteGameInstance* RouteGameInstance = Cast<URouteGameInstance>(GetGameInstance());

	if (RouteGameInstance)
	{
		RouteGameInstance->OnLoginResultDelegate.AddDynamic(this, &URouteLoginWidget::HandleLoginResult);
	}
}

void URouteLoginWidget::OnLoginClicked()
{
	const FString LoginID = EditableTextBox_ID->GetText().ToString();

	const FString Password = EditableTextBox_Password->GetText().ToString();

	URouteGameInstance* RouteGameInstance = Cast<URouteGameInstance>(GetGameInstance());

	if (!RouteGameInstance)
	{
		return;
	}

	RouteGameInstance->RequestLogin(LoginID, Password);

	//UE_LOG(LogTemp, Log, TEXT("Login Button Clicked. ID: %s"), *LoginID);
}

void URouteLoginWidget::HandleLoginResult(bool bSuccess, const FString& Message)
{
	Text_Status->SetText(FText::FromString(Message));

}
