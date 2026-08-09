// Fill out your copyright notice in the Description page of Project Settings.


#include "RouteGameMode.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"

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
