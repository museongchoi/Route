#include <iostream>
#include <string>

#include <WinSock2.h>
#include <WS2tcpip.h>

#include "json.hpp"

#pragma comment(lib, "ws2_32.lib")

using namespace std;
using json = nlohmann::json;

int main()
{
	constexpr unsigned short ServerPort = 9000;
	constexpr int BufferSize = 1024;

	// 1. Winsock 초기화
	WSADATA WsaData;

	const int StartupResult = WSAStartup(MAKEWORD(2, 2), &WsaData);

	if (StartupResult != 0)
	{
		cout << "WSAStartup failed with error: " << StartupResult << "\n";
		return 1;
	}

	// 2. 연결 요청을 받을 Listen Socket 생성
	SOCKET ListenSocket = socket(
		AF_INET,
		SOCK_STREAM,
		IPPROTO_TCP
	);

	if (ListenSocket == INVALID_SOCKET)
	{
		cout << "socket failed. Error: " << WSAGetLastError() << "\n";

		WSACleanup();
		return 1;
	}

	// 3. TCPServer 주소 설정
	sockaddr_in ServerAddress{};

	ServerAddress.sin_family = AF_INET;
	ServerAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	ServerAddress.sin_port = htons(ServerPort);

	// 4. Listen Socket 에 IP와 포트 연결
	const int BindResult = ::bind(
		ListenSocket,
		reinterpret_cast<sockaddr*>(&ServerAddress),
		sizeof(ServerAddress)
	);

	if (BindResult == SOCKET_ERROR)
	{
		cout << "bind failed. Error: " << WSAGetLastError() << "\n";

		closesocket(ListenSocket);
		WSACleanup();

		return 1;
	}

	// 5. Client 연결 요청 대기
	const int ListenResult = listen(
		ListenSocket,
		SOMAXCONN
	);

	if (ListenResult == SOCKET_ERROR)
	{
		cout << "listen failed. Error: " << WSAGetLastError() << "\n";

		closesocket(ListenSocket);
		WSACleanup();

		return 1;
	}

	cout << "Route_TCPServer started." << "\n";
	cout << "Listening on port " << ServerPort << "\n";

	// 6. Client 연결을 반복해서 수락
	while (true)
	{
		cout << "Waiting for client..." << "\n";

		SOCKET ClientSocket = accept(
			ListenSocket,
			nullptr,
			nullptr
		);

		if (ClientSocket == INVALID_SOCKET)
		{
			cout << "accept failed. Error: " << WSAGetLastError() << "\n";
			
			break;
		}

		cout << "Client connected. " << "\n";

		// 7. Client가 전송한 데이터 수신
		char ReceiveBuffer[BufferSize]{};

		const int ReceivedBytes = recv(
			ClientSocket,
			ReceiveBuffer,
			BufferSize - 1,
			0
		);

		if (ReceivedBytes > 0)
		{
			ReceiveBuffer[ReceivedBytes] = '\0';
			const string ReceivedMessage(ReceiveBuffer);

			cout << "Received: " << ReceivedMessage << "\n";

			string ResponseMessage;

			try
			{
				const json RequestJson = json::parse(ReceivedMessage);

				if (!RequestJson.contains("type"))
				{
					json ResponseJson;
					ResponseJson["success"] = false;
					ResponseJson["message"] = "missing type";

					ResponseMessage = ResponseJson.dump() + "\n";
				}
				else
				{
					const string MessageType = RequestJson["type"].get<string>();

					if (MessageType == "REGISTER_SERVER")
					{
						if (!RequestJson.contains("server_name") ||
							!RequestJson.contains("ip_address") ||
							!RequestJson.contains("port") ||
							!RequestJson.contains("current_players") ||
							!RequestJson.contains("max_players") ||
							!RequestJson.contains("status"))
						{
							json ResponseJson;
							ResponseJson["success"] = false;
							ResponseJson["message"] = "invalid request";

							ResponseMessage = ResponseJson.dump() + "\n";
						}
						else
						{
							const string ServerName = RequestJson["server_name"].get<string>();
							const string IpAddress = RequestJson["ip_address"].get<string>();
							const int Port = RequestJson["port"].get<int>();
							const int CurrentPlayers = RequestJson["current_players"].get<int>();
							const int MaxPlayers = RequestJson["max_players"].get<int>();
							const string Status = RequestJson["status"].get<string>();

							cout << "Register Server Request" << "\n";
							cout << "ServerName: " << ServerName << "\n";
							cout << "IpAddress: " << IpAddress << "\n";
							cout << "Port: " << Port << "\n";
							cout << "Players: " << CurrentPlayers << " / " << MaxPlayers << "\n";
							cout << "Status: " << Status << "\n";

							json ResponseJson;
							ResponseJson["success"] = true;
							ResponseJson["message"] = "REGISTER_SERVER_OK";

							ResponseMessage = ResponseJson.dump() + "\n";
						}
					}
					else
					{
						json ResponseJson;
						ResponseJson["success"] = false;
						ResponseJson["message"] = "unknown message type";

						ResponseMessage = ResponseJson.dump() + "\n";
					}
				}
			}
			catch (const json::exception& Err)
			{
				json ResponseJson;
				ResponseJson["success"] = false;
				ResponseJson["message"] = "invalid json";

				ResponseMessage = ResponseJson.dump() + "\n";
			}

			// 8. 처리 결과를 Client에 전송
			const int SentBytes = send(
				ClientSocket,
				ResponseMessage.c_str(),
				static_cast<int>(ResponseMessage.size()),
				0
			);

			if (SentBytes == SOCKET_ERROR)
			{
				cout << "send failed. Error: " << WSAGetLastError() << "\n";

			}
			else
			{
				cout << "Sent: " << ResponseMessage << "\n";
			}
		}
		else if (ReceivedBytes == 0)
		{
			cout << "Client disconnected without data." << "\n";
		}
		else
		{
			cout << "recv failed. Error: " << WSAGetLastError() << "\n";
		}

		// 9. 현재 Client 연결 종료
		closesocket(ClientSocket);

		cout << "Client connection closed." << "\n";
	}

	// 10. TCPServer 종료 처리
	closesocket(ListenSocket);
	WSACleanup();

	return 0;
}
