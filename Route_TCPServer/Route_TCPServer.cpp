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

unique_ptr<sql::Connection> CreateMySqlConnection()
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

		unique_ptr<sql::Statement> Stmt(Conn->createStatement());

		unique_ptr<sql::ResultSet> Result(Stmt->executeQuery("SELECT 1"));

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

// Dedicated Server 등록 또는 기존 정보 갱신
bool SaveOrUpdateServerInstance(const string& ServerName, const string& IpAddress, int Port, int CurrentPlayers, int MaxPlayers, const string& Status)
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

// Dedicated Server last_heartbeat 갱신
bool UpdateServerHeartbeat(const string& IpAddress, int Port)
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
				"UPDATE server_instances "
				"SET last_heartbeat = NOW() "
				"WHERE ip_address = ? AND port = ?"
			)
		);

		Stmt->setString(1, IpAddress);
		Stmt->setInt(2, Port);

		const int UpdateRows = Stmt->executeUpdate();

		return UpdateRows > 0;
	}
	catch (const sql::SQLException& Err)
	{
		cout << "MySQL : UpdateServerHeartbeat failed. " << Err.what() << "\n";
		return false;
	}
}

// Heartbeat 15초 초과 서버 OFFLINE 변경
bool UpdateOfflineServers()
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
				"UPDATE server_instances "
				"SET status = 'OFFLINE' "
				"WHERE status != 'OFFLINE' "
				"AND last_heartbeat < NOW() - INTERVAL 15 SECOND"
			)
		);

		const int UpdatedRows = Stmt->executeUpdate();

		if (UpdatedRows > 0)
		{
			cout << "Offline Servers Updated: " << UpdatedRows << "\n";
		}

		return true;
	}
	catch (const sql::SQLException& Err)
	{
		cout << "MySQL : UpdateOfflineServers failed. " << Err.what() << "\n";
		return false;
	}
}

// Dedicated Server 목록 조회
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

// Dedicated Server 정보 갱신
bool UpdateServerInstance(const string& IpAddress, int Port, int CurrentPlayers, int MaxPlayers, const string& Status)
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
				"UPDATE server_instances "
				"SET current_players = ?, "
				"max_players = ?, "
				"status = ?, "
				"last_heartbeat = NOW() "
				"WHERE ip_address = ? AND port = ?"
			)
		);

		Stmt->setInt(1, CurrentPlayers);
		Stmt->setInt(2, MaxPlayers);
		Stmt->setString(3, Status);
		Stmt->setString(4, IpAddress);
		Stmt->setInt(5, Port);

		const int UpdateRows = Stmt->executeUpdate();

		cout << "UpdateRows: " << UpdateRows << "\n";

		return UpdateRows > 0;
	}
	catch (const sql::SQLException& Err)
	{
		cout << "MySQL : UpdateServerInstance failed. " << Err.what() << "\n";
		return false;
	}
}

string MakeResponseMessage(bool bSuccess, const string& Message)
{
	json ResponseJson;

	ResponseJson["success"] = bSuccess;
	ResponseJson["message"] = Message;

	return ResponseJson.dump() + "\n";
}

// Winsock 초기화, TCP 요청 수신, JSON 처리, DB 작업 및 응답 전송
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
					ResponseMessage = MakeResponseMessage(false, "missing type");
				}
				else
				{
					const string MessageType = RequestJson["type"].get<string>();

					if (MessageType == "REGISTER_SERVER")
					{
						if (!RequestJson.contains("server_name") || !RequestJson.contains("ip_address") || !RequestJson.contains("port") || 
							!RequestJson.contains("current_players") || !RequestJson.contains("max_players") || !RequestJson.contains("status"))
						{
							ResponseMessage = MakeResponseMessage(false, "invalid request");
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
								ResponseMessage = MakeResponseMessage(true, "REGISTER_SERVER_OK");
							}
							else
							{
								ResponseMessage = MakeResponseMessage(false, "database error");
							}

							ResponseMessage = ResponseJson.dump() + "\n";
						}
					}
					else if (MessageType == "REQUEST_SERVER_LIST")
					{
						cout << "Request Server List" << "\n";

						UpdateOfflineServers();

						json ResponseJson = GetServerListJson();

						ResponseMessage = ResponseJson.dump() + "\n";
					}
					else if (MessageType == "UPDATE_SERVER")
					{
						cout << "Update Server Request" << "\n";

						if (!RequestJson.contains("ip_address") || !RequestJson.contains("port") || 
							!RequestJson.contains("current_players") || !RequestJson.contains("max_players") || 
							!RequestJson.contains("status"))
						{
							ResponseMessage = MakeResponseMessage(false, "invalid request");
						}
						else
						{
							const string IpAddress = RequestJson["ip_address"].get<string>();
							const int Port = RequestJson["port"].get<int>();
							const int CurrentPlayers = RequestJson["current_players"].get<int>();
							const int MaxPlayers = RequestJson["max_players"].get<int>();
							const string Status = RequestJson["status"].get<string>();

							cout << "IpAddress: " << IpAddress << "\n";
							cout << "Port: " << Port << "\n";
							cout << "Players: " << CurrentPlayers << " / " << MaxPlayers << "\n";
							cout << "Status: " << Status << "\n";

							const bool bUpdated = UpdateServerInstance(IpAddress, Port, CurrentPlayers, MaxPlayers, Status);

							json ResponseJson;

							if (bUpdated)
							{
								ResponseMessage = MakeResponseMessage(true, "UPDATE_SERVER_OK");
							}
							else
							{
								ResponseMessage = MakeResponseMessage(false, "database error");
							}

							ResponseMessage = ResponseJson.dump() + "\n";
						}
					}
					else if (MessageType == "HEARTBEAT")
					{
						cout << "Heartbeat Request" << "\n";

						if (!RequestJson.contains("ip_address") || !RequestJson.contains("port"))
						{
							ResponseMessage = MakeResponseMessage(false, "invalid request");
						}
						else
						{
							const string IpAddress = RequestJson["ip_address"].get<string>();
							const int Port = RequestJson["port"].get<int>();

							cout << "IpAddress: " << IpAddress << "\n";
							cout << "Port: " << Port << "\n";

							const bool bUpdated = UpdateServerHeartbeat(IpAddress, Port);

							json ResponseJson;

							if (bUpdated)
							{
								ResponseMessage = MakeResponseMessage(true, "HEARTBEAT_OK");
							}
							else
							{
								ResponseMessage = MakeResponseMessage(false, "server not found");
							}

							ResponseMessage = ResponseJson.dump() + "\n";
						}
					}
					else
					{
						ResponseMessage = MakeResponseMessage(false, "unknown message type");
					}
				}
			}
			catch (const json::exception& Err)
			{
				ResponseMessage = MakeResponseMessage(false, "invalid json");
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
