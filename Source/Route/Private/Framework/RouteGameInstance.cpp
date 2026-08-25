// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/RouteGameInstance.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"

#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

void URouteGameInstance::Init()
{
	Super::Init();

	UE_LOG(LogTemp, Log, TEXT("RouteGameInstance Init"));

	if (IsRunningDedicatedServer())
	{
		UE_LOG(LogTemp, Log, TEXT("Dedicated Server. Skip REQUEST_SERVER_LIST."));
		return;
	}

	RequestServerListFromTcpServer();
}

bool URouteGameInstance::TravelToTestServer()
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);

	if (!PlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerController is null. ClientTravel canceled"));
		return false;
	}

	const FString ServerAddress = TEXT("127.0.0.1:7777");

	UE_LOG(LogTemp, Error, TEXT("ClientTravel to %s"), *ServerAddress);

	PlayerController->ClientTravel(ServerAddress, TRAVEL_Absolute);

	return true;
}

bool URouteGameInstance::RequestServerListFromTcpServer()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	if (!SocketSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("SocketSubsystem is null"));
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
		TEXT("RouteServerListSocket"),
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

	const FString RequestMessage = TEXT("{\"type\":\"REQUEST_SERVER_LIST\"}\n");

	FTCHARToUTF8 ConvertedMessage(*RequestMessage);

	int32 BytesSent = 0;

	const bool bSent = Socket->Send(
		reinterpret_cast<const uint8*>(ConvertedMessage.Get()),
		ConvertedMessage.Length(),
		BytesSent
	);

	if (!bSent)
	{
		UE_LOG(LogTemp, Error, TEXT("Send REQUEST_SERVER_LIST failed."));

		Socket->Close();
		SocketSubsystem->DestroySocket(Socket);

		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("REQUEST_SERVER_LIST sent. Bytes: %d"), BytesSent);

	if (Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(2)))
	{
		uint8 ReceiveBuffer[4096];
		int32 BytesRead = 0;

		if (Socket->Recv(ReceiveBuffer, sizeof(ReceiveBuffer) - 1, BytesRead))
		{
			ReceiveBuffer[BytesRead] = '\0';
			const FString Response = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(ReceiveBuffer)));

			UE_LOG(LogTemp, Log, TEXT("Server List Response: %s"), *Response);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No Response from TCPServer."));
	}

	Socket->Close();
	SocketSubsystem->DestroySocket(Socket);

	return false;
}
