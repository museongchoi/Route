// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/RouteServerEntryWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"

void URouteServerEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	Button_Connect->OnClicked.AddDynamic(this, &URouteServerEntryWidget::OnconnectClicked);

}

void URouteServerEntryWidget::SetServerInfo(const FRouteServerInfo& InServerInfo)
{
	ServerInfo = InServerInfo;

	Text_ServerName->SetText(FText::FromString(ServerInfo.ServerName));

	const FString PlayerCountText = FString::Printf(TEXT(" % d / % d"), ServerInfo.CurrentPlayers, ServerInfo.MaxPlayers);

	Text_PlayerCount->SetText(FText::FromString(PlayerCountText));
}

void URouteServerEntryWidget::OnconnectClicked()
{
	APlayerController* PlayerController = GetOwningPlayer();

	if (!PlayerController)
	{
		return;
	}

	const FString ServerAddress = FString::Printf(TEXT("%s:%d"), *ServerInfo.IpAddress, ServerInfo.Port);

	PlayerController->ClientTravel(ServerAddress, TRAVEL_Absolute);

}
