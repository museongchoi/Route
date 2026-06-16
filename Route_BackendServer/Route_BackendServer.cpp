#include <iostream>
#include <string>
#include <fstream>
#include <memory>

#pragma comment(lib, "ws2_32.lib")

#include "httplib.h"
#include "json.hpp"
#include <mysql/jdbc.h>

using namespace std;
using json = nlohmann::json;

// 1. 설정 파일 읽기
bool LoadDbPassword(string& Password)
{
	ifstream DbConfigFile("Config/DBSecrets.ini");

	if (!DbConfigFile.is_open())
	{
		cout << "Config : Failed open Config/DBSecrets.ini" << "\n";
		return false;
	}

	getline(DbConfigFile, Password);

	if (Password.empty())
	{
		cout << "Config : PW empty" << "\n";
		return false;
	}

	return true;
}

// 2. DB 연결 생성
unique_ptr<sql::Connection> CreateMySqlConnection()
{
	// PW 읽어오기
	string DbPassword;

	if (!LoadDbPassword(DbPassword))
	{
		return nullptr;
	}

	sql::mysql::MySQL_Driver* Driver = sql::mysql::get_mysql_driver_instance();

	if (Driver == nullptr)
	{
		cout << "MySQL : Driver is null." << "\n";
		return nullptr;
	}

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

// 3. DB 연결 테스트
bool TestMySqlConnection()
{
	cout << "MySQL : TestMySqlConnection() entered." << "\n";

	try
	{
		/*
			MySQL Connector/C++ Classic JDBC API 사용 기준.

			Header  : <mysql/jdbc.h>
			Library : mysqlcppconn.lib
			DLL     : mysqlcppconn-10-vs14.dll
			Host    : tcp://127.0.0.1
			Port    : 기본 MySQL 포트 3306
		*/

		unique_ptr<sql::Connection> Conn = CreateMySqlConnection();

		if (!Conn)
		{
			cout << "MySQL : Failed to create connection." << "\n";
			return false;
		}

		cout << "MySQL : Connection created" << "\n";

		Conn->close();

		cout << "MySQL : Connection succeeded" << "\n";

		return true;

	}
	catch (const sql::SQLException& Err)
	{
		cout << "MySQL : Connection failed." << "\n";
		cout << "MySQL : Error: " << Err.what() << "\n";
		cout << "MySQL : Error Code: " << Err.getErrorCode() << "\n";
		cout << "MySQL : SQL State: " << Err.getSQLState() << "\n";
		return false;
	}
	catch (const std::exception& Ex)
	{
		cout << "MySQL : std::exception occurred." << "\n";
		cout << "MySQL : Error: " << Ex.what() << "\n";
		return false;
	}
}

int main()
{
	//json TestJson;
	//TestJson["status"] = "ok";
	//cout << TestJson.dump() << "\n";

	// DB 연결 상태 확인
	const bool bDbConnected = TestMySqlConnection();

	// HTTP 서버 생성
	httplib::Server Svr;

	// GET /health 등록
	Svr.Get("/health", [bDbConnected](const httplib::Request& Req, httplib::Response& Res)
	{
		const string DbStatus = bDbConnected ? "connected" : "disconnected";

		const string JsonResponse =
			R"({"status":"ok","server":"RouteBackendServer","database_status":")"
			+ DbStatus
			+ R"("})";

		Res.set_content(JsonResponse, "application/json");

	});

	// POST /register 등록
	Svr.Post("/register", [](const httplib::Request& Req, httplib::Response& Res)
	{
			try
			{
				// 1. Request Body 의 JSON 파싱
				json ReqJson = json::parse(Req.body);

				// 2. 필수 필드 존재 여부 확인
				if (!ReqJson.contains("login_id") || !ReqJson.contains("password") || !ReqJson.contains("nickname"))
				{
					Res.status = 400;
					Res.set_content(R"({"success":false,"message":"invalid request"})",
						"application/json"
					);
					return;
				}

				// 3. JSON 값 추출
				const string LoginId = ReqJson["login_id"].get<string>();
				const string Password = ReqJson["password"].get<string>();
				const string Nickname = ReqJson["nickname"].get<string>();

				// 4. 빈 문자열 확인
				if (LoginId.empty() || Password.empty() || Nickname.empty())
				{
					Res.status = 400;
					Res.set_content(
						R"({"success":false,"message":"empty field"})",
						"application/json"
					);
					return;
				}

				// 5. MySQL 연결
				unique_ptr<sql::Connection> Conn = CreateMySqlConnection();

				if (!Conn)
				{
					Res.status = 500;
					Res.set_content(
						R"({"success":false,"message":"database connection failed"})",
						"application/json"
					);
					return;
				}

				//unique_ptr<sql::PreparedStatement> CheckStmt(
				//	Conn->prepareStatement(
				//		"SELECT account_id FROM accounts WHERE login_id = ?"
				//	)
				//);

				//CheckStmt->setString(1, LoginId);

				//unique_ptr<sql::ResultSet> Result(
				//	CheckStmt->executeQuery()
				//);

				//if (Result->next())
				//{
				//	Res.status = 409;
				//	Res.set_content(
				//		R"({"success":false,"message":"duplicated login_id"})",
				//		"application/json"
				//	);
				//	return;
				//}

				// 6. 계정 생성
				// login_id 중복은 accounts.login_id 의 UNIQUE 제약 조건으로 검사.
				unique_ptr<sql::PreparedStatement> InsertStmt(
					Conn->prepareStatement(
						"INSERT INTO accounts (login_id, pw_hash, nickname) "
						"VALUES (?, SHA2(?, 256), ?)"
					)
				);

				InsertStmt->setString(1, LoginId);
				InsertStmt->setString(2, Password);
				InsertStmt->setString(3, Nickname);

				InsertStmt->executeUpdate();

				Res.status = 201;
				Res.set_content(
					R"({"success":true,"message":"register success"})",
					"application/json"
				);
			}
			catch (const json::exception& Err)
			{
				Res.status = 400;
				Res.set_content(
					R"({"success":false,"message":"invalid json"})",
					"application/json"
				);
			}
			catch (const sql::SQLException& Err)
			{
				// MySQL 1062 : UNIQUE 키 중복
				if (Err.getErrorCode() == 1062)
				{
					Res.status = 409;
					Res.set_content(
						R"({"success":false,"message":"duplicated login_id"})",
						"application/json"
					);
					return;
				}

				cout << "MySQL : Register failed. " << Err.what() << endl;
				cout << "MySQL : Error Code: " << Err.getErrorCode() << endl;
				cout << "MySQL : SQL State: " << Err.getSQLState() << endl;

				Res.status = 500;
				Res.set_content(
					R"({"success":false,"message":"database error"})",
					"application/json"
				);
			}
	});

	cout << "========================================" << "\n";
	cout << " RouteBackendServer started." << "\n";
	cout << " Listening on http://localhost:8080" << "\n";
	cout << " Health Check: http://localhost:8080/health" << "\n";
	cout << "========================================" << "\n";

	// 서버 실행 listen
	if (!Svr.listen("0.0.0.0", 8080))
	{
		cout << "Failed to start RouteBackendServer." << "\n";
		return 1;
	}

    return 0;
}


