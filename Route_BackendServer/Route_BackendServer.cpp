#include <iostream>
#include <string>
#include <fstream>
#include <memory>

#pragma comment(lib, "ws2_32.lib")

#include "httplib.h"
#include <mysql/jdbc.h>

using namespace std;

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

		cout << "MySQL : Creating driver instance..." << "\n";

		sql::mysql::MySQL_Driver* Driver = sql::mysql::get_mysql_driver_instance();

		if (Driver == nullptr)
		{
			cout << "MySQL : Driver is null." << "\n";
			return false;
		}

		cout << "MySQL : Loading DB Password..." << "\n";
		// PW 읽어오기
		string DbPassword;

		cout << "MySQL : Creating connection..." << "\n";

		if (!LoadDbPassword(DbPassword))
		{
			return false;
		}

		unique_ptr<sql::Connection> Conn(
			Driver->connect(
				"tcp://127.0.0.1",
				"root",
				DbPassword
			)
		);

		cout << "MySQL : Connection created." << "\n";

		Conn->close();

		cout << "MySQL : Connection succeeded." << "\n";
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
	catch (...)
	{
		cout << "MySQL : Unknown error occurred." << "\n";
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

		const string JsonResponse = 
			R"({"status":"ok","server":"RouteBackendServer","database_status":")" 
			+ DbStatus 
			+ R"("})";

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


