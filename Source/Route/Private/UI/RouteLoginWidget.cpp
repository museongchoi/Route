// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/RouteLoginWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"

void URouteLoginWidget::NativeConstruct()
{
	Super::NativeConstruct();

	Button_Login->OnClicked.AddDynamic(this, &URouteLoginWidget::OnLoginClicked);
}

void URouteLoginWidget::OnLoginClicked()
{
	const FString LoginID = EditableTextBox_ID->GetText().ToString();

	const FString Password = EditableTextBox_Password->GetText().ToString();

	UE_LOG(LogTemp, Log, TEXT("Login Button Clicked. ID: %s"), *LoginID);
}
