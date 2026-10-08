// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/RouteRegisterWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"

#include "Framework/RouteLoginPlayerController.h"
#include "Framework/RouteGameInstance.h"

void URouteRegisterWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	Button_Register->OnClicked.AddDynamic(this, &URouteRegisterWidget::OnRegisterClicked);
	Button_Back->OnClicked.AddDynamic(this, &URouteRegisterWidget::OnBackClicked);

	URouteGameInstance* RouteGameInstance = Cast<URouteGameInstance>(GetGameInstance());

	if (!RouteGameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("RouteGameInstance is null."));
		return;
	}

	RouteGameInstance->OnRegisterResultDelegate.AddDynamic(this, &URouteRegisterWidget::HandleRegisterResult);

}

void URouteRegisterWidget::OnRegisterClicked()
{
	const FString LoginID = EditableTextBox_ID->GetText().ToString();
	const FString Password = EditableTextBox_Password->GetText().ToString();
	const FString Nickname = EditableTextBox_Nickname->GetText().ToString();

	if (LoginID.IsEmpty() || Password.IsEmpty() || Nickname.IsEmpty())
	{
		Text_Status->SetText(FText::FromString(TEXT("Please fill in all fields.")));
		return;
	}

	URouteGameInstance* RouteGameInstance = Cast<URouteGameInstance>(GetGameInstance());

	if (!RouteGameInstance)
	{
		return;
	}

	RouteGameInstance->RequestRegister(LoginID, Password, Nickname);
}

void URouteRegisterWidget::HandleRegisterResult(bool bSuccess, const FString& Message)
{
	Text_Status->SetText(FText::FromString(Message));

	if (!bSuccess)
	{
		return;
	}

	ARouteLoginPlayerController* LoginPlayerController = Cast<ARouteLoginPlayerController>(GetOwningPlayer());

	if (!LoginPlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("RouteLoginPlayerController is null."));
		return;
	}

	LoginPlayerController->ShowLoginWidget();
}

void URouteRegisterWidget::OnBackClicked()
{
	ARouteLoginPlayerController* LoginPlayerController = Cast<ARouteLoginPlayerController>(GetOwningPlayer());

	if (!LoginPlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("RouteLoginPlayerController is null."));
		return;
	}

	LoginPlayerController->ShowLoginWidget();
}
