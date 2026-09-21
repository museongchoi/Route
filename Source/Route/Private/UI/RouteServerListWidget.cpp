// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/RouteServerListWidget.h"

#include "Components/VerticalBox.h"
#include "Framework/RouteGameInstance.h"
#include "UI/RouteServerEntryWidget.h"

void URouteServerListWidget::RefreshServerList()
{
	if (!VerticalBox_ServerList)
	{
		return;
	}

	VerticalBox_ServerList->ClearChildren();

	URouteGameInstance* RouteGameInstance = Cast<URouteGameInstance>(GetGameInstance());

	if (!RouteGameInstance)
	{
		return;
	}

	if (!ServerEntryWidgetClass)
	{
		return;
	}

	const TArray<FRouteServerInfo>& ServerList = RouteGameInstance->GetCachedServerList();

	for (const FRouteServerInfo& ServerInfo : ServerList)
	{
		URouteServerEntryWidget* EntryWidget = CreateWidget<URouteServerEntryWidget>(GetOwningPlayer(), ServerEntryWidgetClass);

		if (!EntryWidget)
		{
			continue;
		}

		EntryWidget->SetServerInfo(ServerInfo);

		VerticalBox_ServerList->AddChildToVerticalBox(EntryWidget);
	}
}
