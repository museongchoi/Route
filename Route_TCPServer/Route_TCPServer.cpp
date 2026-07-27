#include <iostream>
#include <string>
#include <fstream>
#include <memory>

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <mysql/jdbc.h>

#include "json.hpp"

#pragma comment(lib, "ws2_32.lib")

using namespace std;
using json = nlohmann::json;

string LoadDbPassword()
{
	ifstream DbConfigFile("Config/DBSecrets.ini");

	if (!DbConfigFile.is_open())
	{
		cout << "MySQL : DBSecrets.ini open failed." << "\n";
		return "";
	}

	string DbPassword;
	getline(DbConfigFile, DbPassword);

	return DbPassword;
}

unique_ptr <sql::Connection> CreateMySqlConnection()
{
	const string DbPassword = LoadDbPassword();

	if (DbPassword.empty())
	{
		cout << "MySQL : DB password file not found. " << "\n";
		return nullptr;
	}

	sql::mysql::MySQL_Driver* Driver = sql::mysql::get_mysql_driver_instance();

	unique_ptr<sql::Connection> Conn(
		Driver->connect(
			"tcp://127.0.0.1",
			"root",
			DbPassword
		)
	);

	Conn->setSchema("RouteDB");

	return Conn;
}

bool TestMySqlConnection()
{
	cout << "MySQL : TestMySqlConnection() entered." << "\n";

	try
	{
		unique_ptr<sql::Connection> Conn = CreateMySqlConnection();

		if (!Conn)
		{
			cout << "MySQL : Connection failed." << "\n";
			return false;
		}

		cout << "MySQL : Connection created" << "\n";

		unique_ptr<sql::Statement> Stmt(
			Conn->createStatement()
		);

		unique_ptr<sql::ResultSet> Result(
			Stmt->executeQuery("SELECT 1")
		);

		if (Result->next())
		{
			cout << "MySQL : Connection succeeded" << "\n";
			return true;
		}
	}
	catch (const sql::SQLException& Err)
	{
		cout << "MySQL : Connection failed. " << Err.what() << "\n";
	}

	return false;
}

bool SaveOrUpdateServerInstance(const string& ServerName, const string& IpAddress, int Port, int CurrentPlayers, int MaxPlayers, const string& Status)
{
	{
		try
		{
			unique_ptr<sql::Connection> Conn = CreateMySqlConnection();

			if (!Conn)
			{
				return false;
			}

			unique_ptr<sql::PreparedStatement> Stmt(
				Conn->prepareStatement(
					"INSERT INTO server_instances "
					"(server_name, ip_address, port, current_players, max_players, status, last_heartbeat) "
					"VALUES (?, ?, ?, ?, ?, ?, NOW()) "
					"ON DUPLICATE KEY UPDATE "
					"server_name = VALUES(server_name),"
					"current_players = VALUES(current_players), "
					"max_players = VALUES(max_players), "
					"status = VALUES(status), "
					"last_heartbeat = NOW()"
				)
			);

			Stmt->setString(1, ServerName);
			Stmt->setString(2, IpAddress);
			Stmt->setInt(3, Port);
			Stmt->setInt(4, CurrentPlayers);
			Stmt->setInt(5, MaxPlayers);
			Stmt->setString(6, Status);

			Stmt->executeUpdate();

			return true;
		}
		catch (const sql::SQLException& Err)
		{
			cout << "MySQL : SaveOrUpdateServerInstance failed. " << Err.what() << "\n";
			return false;
		}
	}
}

json GetServerListJson()
{
	json ResponseJson;
	ResponseJson["success"] = false;

	try
	{
		unique_ptr<sql::Connection> Conn = CreateMySqlConnection();

		if (!Conn)
		{
			ResponseJson["message"] = "database connection failed";
			return ResponseJson;
		}

		unique_ptr<sql::PreparedStatement> Stmt(
			Conn->prepareStatement(
				"SELECT server_name, ip_address, port, current_players, max_players, status "
				"FROM server_instances "
				"WHERE status = 'OPEN' "
				"AND current_players < max_players"
			)
		);

		unique_ptr<sql::ResultSet> Result(
			Stmt->executeQuery()
		);

		json ServerArray = json::array();

		while (Result->next())
		{
			json ServerJson;
			ServerJson["server_name"] = Result->getString("server_name").asStdString();
			ServerJson["ip_address"] = Result->getString("ip_address").asStdString();
			ServerJson["port"] = Result->getInt("port");
			ServerJson["current_players"] = Result->getInt("current_players");
			ServerJson["max_players"] = Result->getInt("max_players");
			ServerJson["status"] = Result->getString("status").asStdString();

			ServerArray.push_back(ServerJson);
		}

		ResponseJson["success"] = true;
		ResponseJson["servers"] = ServerArray;

		return ResponseJson;
	}
	catch (const sql::SQLException& Err)
	{
		cout << "MySQL : GetServerListJson failed. " << Err.what() << "\n";

		ResponseJson["message"] = "database error";
		return ResponseJson;
	}
}

int main()
{
	TestMySqlConnection();

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

							const bool bSaved = SaveOrUpdateServerInstance(ServerName, IpAddress, Port, CurrentPlayers, MaxPlayers, Status);
							
							json ResponseJson;

							if (bSaved)
							{
								ResponseJson["success"] = true;
								ResponseJson["message"] = "REGISTER_SERVER_OK";
							}
							else
							{
								ResponseJson["success"] = false;
								ResponseJson["message"] = "database error";

							}

							ResponseMessage = ResponseJson.dump() + "\n";

						}
					}
					else if (MessageType == "REQUEST_SERVER_LIST")
					{
						cout << "Request Server List" << "\n";

						json ResponseJson = GetServerListJson();

						ResponseMessage = ResponseJson.dump() + "\n";
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
