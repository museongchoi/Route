// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RouteGameMode.generated.h"

/**
 * 
 */
UCLASS()
class ROUTE_API ARouteGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARouteGameMode();
	
protected:
	virtual void BeginPlay() override;

// ===== Player Connection =====
// Client 접속 상태에 따라 TCPServer에 상태 갱신
protected:
	// 최대 인원 접속 확인 Client 접속 허용 여부 결정
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

	virtual void PostLogin(APlayerController* NewPlayer) override;
	
	virtual void Logout(AController* ExitingPlayer) override;

// ===== TCP Server =====
// DedicatedServer 정보를 TCPServer 에 등록 및 갱신, Heartbeat 전송
private:
	bool RegisterServerToTcpServer();
	bool UpdateServerToTcpServer();
	void SendHeartbeatToTcpServer();

// ===== Server State =====
private:
	// Heartbeat 주기 실행 Timer
	FTimerHandle HeartbeatTimerHandle;

	FString ServerName = TEXT("RouteServer01");
	FString ServerIpAddress = TEXT("127.0.0.1");

	int32 ServerPort = 7777;
	int32 CurrentPlayers = 0;
	int32 MaxPlayers = 3;

};
