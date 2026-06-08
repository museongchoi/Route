#include <iostream>
#include <string>
#include <memory>

#pragma comment(lib, "ws2_32.lib")

#include "httplib.h"
//#include <mysqlx/xdevapi.h> // sqlx 헤더
#include <mysql/jdbc.h>

using namespace std;

bool TestMySqlConnection()
{
	cout << "[MySQL] TestMySqlConnection() entered." << endl;

	try
	{
		cout << "[MySQL] Creating session..." << endl;
		/*
		mysqlcppconnx.lib / mysqlcppconnx-2-vs14.dll 사용 기준.
		X DevAPI는 일반적으로 MySQL X Protocol 포트인 33060을 사용한다.
		
		localhost : 현재 PC
		33060     : MySQL X Protocol 포트
		root      : MySQL 계정
		password  : MySQL root 비밀번호
		*/

		//mysqlx::Session session(
		//	"localhost",
		//	33060,
		//	"root",
		//	"password"
		//);

		sql::mysql::MySQL_Driver* Driver = sql::mysql::get_mysql_driver_instance();

		if (Driver == nullptr)
		{
			cout << "[MySQL] Driver is null." << endl;
			return false;
		}

		cout << "[MySQL] Creating connection..." << endl;

		unique_ptr<sql::Connection> Conn(
			Driver->connect(
				"tcp://127.0.0.1",
				"root",
				"ms12340"
			)
		);

		cout << "[MySQL] Connection created." << endl;

		Conn->close();

		cout << "[MySQL] Connection succeeded." << endl;
		return true;
	}
	catch (const sql::SQLException& Err)
	{
		cout << "[MySQL] Connection failed." << endl;
		cout << "[MySQL] Error: " << Err.what() << endl;
		cout << "[MySQL] Error Code: " << Err.getErrorCode() << endl;
		cout << "[MySQL] SQL State: " << Err.getSQLState() << endl;
		return false;
	}
	catch (const std::exception& Ex)
	{
		cout << "[MySQL] std::exception occurred." << endl;
		cout << "[MySQL] Error: " << Ex.what() << endl;
		return false;
	}
	catch (...)
	{
		cout << "[MySQL] Unknown error occurred." << endl;
		return false;
	}
}

int main()
{
	const bool bDbConnected = TestMySqlConnection();

	httplib::Server Svr;
	Svr.Get("/health", [bDbConnected](const httplib::Request& Req, httplib::Response& Res)
	{
		const string DbStatus = bDbConnected ? "connected" : "disconnected";

		const string JsonResponse = R"({"status":"ok","server":"RouteBackendServer","database_status":")" + DbStatus + R"("})";
		Res.set_content(JsonResponse, "application/json");

	});

	cout << "========================================" << "\n";
	cout << " RouteBackendServer started." << "\n";
	cout << " Listening on http://localhost:8080" << "\n";
	cout << " Health Check: http://localhost:8080/health" << "\n";
	cout << "========================================" << "\n";
	
	if (!Svr.listen("0.0.0.0", 8080))
	{
		cout << "Failed to start RouteBackendServer." << "\n";
		return 1;
	}

    return 0;
}


