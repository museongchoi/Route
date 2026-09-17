// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/RouteGameMode.h"

#include "Framework/RoutePlayerState.h"
#include "Framework/RoutePlayerController.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

ARouteGameMode::ARouteGameMode()
{
	PlayerStateClass = ARoutePlayerState::StaticClass();
	PlayerControllerClass = ARoutePlayerController::StaticClass();
}

void ARouteGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("RouteGameMode BeginPlay"));

	//if (!IsRunningDedicatedServer())
	//{
	//	return;
	//}
	if (GetNetMode() != NM_DedicatedServer)
	{
		UE_LOG(LogTemp, Warning, TEXT("Not Dedicated Server. Skip REGISTER_SERVER."));
		return;
	}

	RegisterServerToTcpServer();
}

void ARouteGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	UE_LOG(LogTemp, Warning, TEXT("RouteGameMode PostLogin"));

	CurrentPlayers = FMath::Clamp(CurrentPlayers + 1, 0, MaxPlayers);

	UE_LOG(LogTemp, Warning, TEXT("CurrentPlayers increased: %d / %d"), CurrentPlayers, MaxPlayers);

	if (!NewPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("PostLogin NewPlayer is null"));
		UpdateServerToTcpServer();
		return;
	}

	ARoutePlayerState* RoutePlayerState = NewPlayer->GetPlayerState<ARoutePlayerState>();

	if (RoutePlayerState)
	{
		UE_LOG(LogTemp, Warning, TEXT("RoutePlayerState assigned successfully."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("RoutePlayerState cast failed."));
	}

	APlayerState* PlayerState = NewPlayer->PlayerState;

	if (PlayerState)
	{
		UE_LOG(LogTemp, Warning, TEXT("Player Connected. PlayerName: %s"), *PlayerState->GetPlayerName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Player connected. PlayerState is null."));
	}

	UpdateServerToTcpServer();
}

void ARouteGameMode::Logout(AController* ExitingPlayer)
{

	UE_LOG(LogTemp, Warning, TEXT("RouteGameMode Logout"));

	CurrentPlayers = FMath::Max(0, CurrentPlayers - 1);

	UE_LOG(LogTemp, Warning, TEXT("CurrentPlayers decreased: %d / %d"), CurrentPlayers, MaxPlayers);

	if (!ExitingPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("Logout ExitingPlayer Controller is null."));
		UpdateServerToTcpServer();

		Super::Logout(ExitingPlayer);
		return;
	}

	APlayerState* PlayerState = ExitingPlayer->PlayerState;

	if (PlayerState)
	{
		UE_LOG(LogTemp, Warning, TEXT("Player Disconnected. PlayerName: %s"), *PlayerState->GetPlayerName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Player disconnected. PlayerState is null."));
	}

	UpdateServerToTcpServer();

	Super::Logout(ExitingPlayer);
}

bool ARouteGameMode::RegisterServerToTcpServer()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("SocketSubsystem is null"));
		return false;
	}

	// 주소 변환
	FIPv4Address TcpServerIp;
	if (!FIPv4Address::Parse(TEXT("127.0.0.1"), TcpServerIp))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid TCPServer IP."));
		return false;
	}

	// 접속 주소 객체 생성
	TSharedRef<FInternetAddr> TcpServerAddress = SocketSubsystem->CreateInternetAddr();
	
	TcpServerAddress->SetIp(TcpServerIp.Value);
	TcpServerAddress->SetPort(9000);

	// 소켓 생성
	FSocket* Socket = SocketSubsystem->CreateSocket(
		NAME_Stream,
		TEXT("RouteRegisterSocket"),
		false
	);

	if (!Socket)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateSocket failed."));
		return false;
	}

	// TCP Connet
	if (!Socket->Connect(*TcpServerAddress))
	{
		UE_LOG(LogTemp, Error, TEXT("Connect to TCPServer failed."));

		Socket->Close();
		SocketSubsystem->DestroySocket(Socket);

		return false;
	}

	// 전송 문자열 생성
	const FString RegisterMessage =
		TEXT("{\"type\":\"REGISTER_SERVER\",")
		TEXT("\"server_name\":\"RouteServer01\",")
		TEXT("\"ip_address\":\"127.0.0.1\",")
		TEXT("\"port\":7777,")
		TEXT("\"current_players\":0,")
		TEXT("\"max_players\":3,")
		TEXT("\"status\":\"OPEN\"}\n");

	// 문자열을 바이트 배열로 변환
	FTCHARToUTF8 ConvertedMessage(*RegisterMessage);

	// Send 메시지 전송
	int32 BytesSent = 0;

	const bool bSent = Socket->Send(
		reinterpret_cast<const uint8*>(ConvertedMessage.Get()),
		ConvertedMessage.Length(),
		BytesSent
	);

	// 예외) Send 실패 처리
	if (!bSent)
	{
		UE_LOG(LogTemp, Error, TEXT("Send REGISTER_SERVER failed."));

		Socket->Close();
		SocketSubsystem->DestroySocket(Socket);

		return false;
	}

	// 예외) Send 성공 처리
	UE_LOG(LogTemp, Log, TEXT("REGISTER_SERVER sent. Bytes: %d"), BytesSent);
	
	// TCPServer 응답 대기
	if (Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(2)))
	{
		uint8 ReceiveBuffer[1024]{};
		int32 BytesRead = 0;

		if (Socket->Recv(ReceiveBuffer, sizeof(ReceiveBuffer) - 1, BytesRead))
		{
			ReceiveBuffer[BytesRead] = '\0';

			const FString Response = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(ReceiveBuffer)));

			UE_LOG(LogTemp, Log, TEXT("TCPServer Response: %s"), *Response);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No response from TCPServer"));
	}

	Socket->Close();
	SocketSubsystem->DestroySocket(Socket);

	return true;
}

bool ARouteGameMode::UpdateServerToTcpServer()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	if (!SocketSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("SocketSubsystem is null."));
		return false;
	}

	FIPv4Address TcpServerIp;

	if (!FIPv4Address::Parse(TEXT("127.0.0.1"), TcpServerIp))
	{
		UE_LOG(LogTemp, Error, TEXT("Invaild TCPServer IP."));
		return false;
	}

	TSharedRef<FInternetAddr> TcpServerAddress = SocketSubsystem->CreateInternetAddr();
	
	TcpServerAddress->SetIp(TcpServerIp.Value);
	TcpServerAddress->SetPort(9000);

	FSocket* Socket = SocketSubsystem->CreateSocket(
		NAME_Stream,
		TEXT("RouteUpdateServerSocket"),
		false
	);

	if (!Socket)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateSocket failed."));
		return false;
	}

	if (!Socket->Connect(*TcpServerAddress))
	{
		UE_LOG(LogTemp, Error, TEXT("Connect to TCPServer failed."));

		Socket->Close();
		SocketSubsystem->DestroySocket(Socket);

		return false;
	}

	FString ServerStatus = TEXT("OPEN");

	if (CurrentPlayers >= MaxPlayers)
	{
		ServerStatus = TEXT("FULL");
	}

	const FString UpdateMessage = FString::Printf(
		TEXT("{\"type\":\"UPDATE_SERVER\",")
		TEXT("\"ip_address\":\"127.0.0.1\",")
		TEXT("\"port\":7777,")
		TEXT("\"current_players\":%d,")
		TEXT("\"max_players\":%d,")
		TEXT("\"status\":\"%s\"}\n"),
		CurrentPlayers,
		MaxPlayers,
		*ServerStatus
	);

	FTCHARToUTF8 ConvertedMessage(*UpdateMessage);

	int32 BytesSent = 0;

	const bool bSent = Socket->Send(
		reinterpret_cast<const uint8*>(ConvertedMessage.Get()),
		ConvertedMessage.Length(),
		BytesSent
	);

	if (!bSent)
	{
		UE_LOG(LogTemp, Error, TEXT("Send UPDATE_SERVER failed."));

		Socket->Close();
		SocketSubsystem->DestroySocket(Socket);

		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("UPDATE_SERVER sent. Bytes: %d"), BytesSent);

	if (Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(2)))
	{
		uint8 ReceiveBuffer[1024]{};
		int32 BytesRead = 0;

		if (Socket->Recv(ReceiveBuffer, sizeof(ReceiveBuffer) - 1, BytesRead))
		{
			ReceiveBuffer[BytesRead] = '\0';

			const FString Response =
				FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(ReceiveBuffer)));

			UE_LOG(LogTemp, Log, TEXT("TCPServer Update Response: %s"), *Response);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No response from TCPServer."));
	}

	Socket->Close();
	SocketSubsystem->DestroySocket(Socket);

	return true;
}
