#pragma once

#include "CoreMinimal.h"
#include "RouteServerInfo.generated.h"

USTRUCT(BlueprintType)
struct FRouteServerInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString ServerName;
	
	UPROPERTY(BlueprintReadOnly)
	FString IpAddress;

	UPROPERTY(BlueprintReadOnly)
	int32 Port = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly)
	FString Status;
};