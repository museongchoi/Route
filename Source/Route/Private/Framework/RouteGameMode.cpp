// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/RouteGameMode.h"

#include "Framework/RoutePlayerState.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include "TimerManager.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

ARouteGameMode::ARouteGameMode()
{

}

void ARouteGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() != NM_DedicatedServer)
	{
		UE_LOG(LogTemp, Warning, TEXT("Not Dedicated Server. Skip REGISTER_SERVER."));
		return;
	}

	FParse::Value(FCommandLine::Get(), TEXT("ServerName="), ServerName);
	FParse::Value(FCommandLine::Get(), TEXT("port="), ServerPort);
	FParse::Value(FCommandLine::Get(), TEXT("MaxPlayers="), MaxPlayers);

	UE_LOG(LogTemp, Warning, TEXT("Dedicated Server 설정 완료 | Name: %s | Port: %d | MaxPlayers: %d"), *ServerName, ServerPort, MaxPlayers);

	if (!RegisterServerToTcpServer())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to register Dedicated Server."));
		return;
	}

	GetWorldTimerManager().SetTimer(
		HeartbeatTimerHandle,
		this,
		&ARouteGameMode::SendHeartbeatToTcpServer,
		5.0f,
		true
	);
}

void ARouteGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	if (CurrentPlayers >= MaxPlayers)
	{
		ErrorMessage = TEXT("Server is full.");
		UE_LOG(LogTemp, Warning, TEXT("[Client] 접속 거부 - 서버 정원 초과 | %d / %d"), CurrentPlayers, MaxPlayers);
		
		return;
	}

}

void ARouteGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (GetNetMode() != NM_DedicatedServer)
	{
		UE_LOG(LogTemp, Warning, TEXT("Not Dedicated Server. Skip PostLogin server update."));
		return;

	}

	CurrentPlayers = FMath::Clamp(CurrentPlayers + 1, 0, MaxPlayers);

	UE_LOG(LogTemp, Warning, TEXT("[DedicatedServer] Client 접속 완료 | Players: %d / %d"), CurrentPlayers, MaxPlayers);

	if (!NewPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("PostLogin NewPlayer is null"));
		UpdateServerToTcpServer();
		return;
	}

	ARoutePlayerState* RoutePlayerState = NewPlayer->GetPlayerState<ARoutePlayerState>();

	if (!RoutePlayerState)
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
	if (GetNetMode() != NM_DedicatedServer)
	{
		Super::Logout(ExitingPlayer);
		return;
	}

	CurrentPlayers = FMath::Max(0, CurrentPlayers - 1);

	UE_LOG(LogTemp, Warning, TEXT("[DedicatedServer] Client 접속 종료 | Player: %d / %d"), CurrentPlayers, MaxPlayers);

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

	// TCP Connect
	if (!Socket->Connect(*TcpServerAddress))
	{
		UE_LOG(LogTemp, Error, TEXT("Connect to TCPServer failed."));

		Socket->Close();
		SocketSubsystem->DestroySocket(Socket);

		return false;
	}

	// 전송 문자열 생성
	const FString RegisterMessage = FString::Printf(
		TEXT("{\"type\":\"REGISTER_SERVER\",")
		TEXT("\"server_name\":\"%s\",")
		TEXT("\"ip_address\":\"%s\",")
		TEXT("\"port\":%d,")
		TEXT("\"current_players\":%d,")
		TEXT("\"max_players\":%d,")
		TEXT("\"status\":\"OPEN\"}\n"),
		*ServerName,
		*ServerIpAddress,
		ServerPort,
		CurrentPlayers,
		MaxPlayers
	);

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

			UE_LOG(LogTemp, Log, TEXT("[TCPServer] Dedicated Server 등록 응답 | % s"), *Response);
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
		UE_LOG(LogTemp, Error, TEXT("Invalid TCPServer IP."));
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
		TEXT("\"ip_address\":\"%s\",")
		TEXT("\"port\":%d,")
		TEXT("\"current_players\":%d,")
		TEXT("\"max_players\":%d,")
		TEXT("\"status\":\"%s\"}\n"),
		*ServerIpAddress,
		ServerPort,
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

			UE_LOG(LogTemp, Log, TEXT("[TCPServer] 서버 상태 갱신 응답 | %s"), *Response);
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

void ARouteGameMode::SendHeartbeatToTcpServer()
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		return;
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	if (!SocketSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("Heartbeat failed. SocketSubsystem is null."));

		return;
	}

	FIPv4Address TcpServerIp;

	if (!FIPv4Address::Parse(TEXT("127.0.0.1"), TcpServerIp))
	{
		UE_LOG(LogTemp, Error, TEXT("Heartbeat failed. Invalid TCPServer IP."));

		return;
	}

	TSharedRef<FInternetAddr> TcpServerAddress = SocketSubsystem->CreateInternetAddr();

	TcpServerAddress->SetIp(TcpServerIp.Value);
	TcpServerAddress->SetPort(9000);

	FSocket* Socket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("RouteHeartbeatSocket"), false);

	if (!Socket)
	{
		UE_LOG(LogTemp, Error, TEXT("Heartbeat failed. CreateSocket failed."));

		return;
	}

	if (!Socket->Connect(*TcpServerAddress))
	{
		UE_LOG(LogTemp, Error, TEXT("Heartbeat failed. Connect to TCPServer failed."));

		Socket->Close();
		SocketSubsystem->DestroySocket(Socket);

		return;
	}

	const FString HeartbeatMessage = FString::Printf(
		TEXT("{\"type\":\"HEARTBEAT\",")
		TEXT("\"ip_address\":\"%s\",")
		TEXT("\"port\":%d}\n"),
		*ServerIpAddress,
		ServerPort
	);

	FTCHARToUTF8 ConvertedMessage(*HeartbeatMessage);

	int32 BytesSent = 0;

	const bool bSent = Socket->Send(reinterpret_cast<const uint8*>(ConvertedMessage.Get()), ConvertedMessage.Length(), BytesSent);

	if (!bSent)
	{
		UE_LOG(LogTemp, Error, TEXT("Send HEARTBEAT failed."));

		Socket->Close();
		SocketSubsystem->DestroySocket(Socket);

		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("HEARTBEAT sent. Server: %s:%d, Bytes: %d"), *ServerIpAddress, ServerPort, BytesSent);

	if (Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(2)))
	{
		uint8 ReceiveBuffer[1024]{};
		int32 BytesRead = 0;

		if (Socket->Recv(ReceiveBuffer, sizeof(ReceiveBuffer) - 1, BytesRead))
		{
			ReceiveBuffer[BytesRead] = '\0';

			const FString Response = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(ReceiveBuffer)));

			UE_LOG(LogTemp, Warning, TEXT("[Heartbeat] TCPServer 응답 | %s"), *Response);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No heartbeat response from TCPServer."));
	}

	Socket->Close();
	SocketSubsystem->DestroySocket(Socket);
}
