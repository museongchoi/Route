#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <bcrypt.h>

#include <eos_sdk.h>
#include <eos_logging.h>
#include <eos_rtc_admin.h>
#include <Windows/eos_Windows.h>

#include <thread>
#include <chrono>
#include <atomic>

#include <queue>
#include <condition_variable>

#include <iostream>
#include <string>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <array>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")

#include "httplib.h"
#include "json.hpp"
#include <mysql/jdbc.h>

using namespace std;
using json = nlohmann::json;

struct EosServerConfig
{
	string ClientId;
	string ClientSecret;
	string ProductId;
	string SandboxId;
	string DeploymentId;
};

struct VoiceJoinRequest
{
	int AccountId = 0;
	string EosPuid;

	bool bProcessed = false;
	bool bSuccess = false;

	string RoomName;
	string ClientBaseUrl;
	string ParticipantToken;
	string ErrorMessage;

	mutex RequestMutex;
	condition_variable RequestCondition;
};

struct VoiceJoinCallbackContext
{
	EOS_HRTCAdmin RtcAdmin = nullptr;
	shared_ptr<VoiceJoinRequest> Request;
};

unordered_map<string, int> SessionMap;
mutex SessionMutex;

queue<shared_ptr<VoiceJoinRequest>> VoiceJoinQueue;
mutex VoiceJoinQueueMutex;


bool LoadEosConfig(EosServerConfig& Config)
{
	ifstream ConfigFile("Config/EOSSecrets.ini");

	if (!ConfigFile.is_open())
	{
		cout << "Config : Failed open Config/EOSSecrets.ini" << "\n";
		return false;
	}

	string Line;

	while (getline(ConfigFile, Line))
	{
		const size_t EqualPos = Line.find('=');

		if (EqualPos == string::npos)
		{
			continue;
		}

		const string Key = Line.substr(0, EqualPos);
		const string Value = Line.substr(EqualPos + 1);

		if (Key == "ClientId")
		{
			Config.ClientId = Value;
		}
		else if (Key == "ClientSecret")
		{
			Config.ClientSecret = Value;
		}
		else if (Key == "ProductId")
		{
			Config.ProductId = Value;
		}
		else if (Key == "SandboxId")
		{
			Config.SandboxId = Value;
		}
		else if (Key == "DeploymentId")
		{
			Config.DeploymentId = Value;
		}
	}

	if (Config.ClientId.empty() || Config.ClientSecret.empty() || Config.ProductId.empty() || Config.SandboxId.empty() || Config.DeploymentId.empty())
	{
		cout << "Config : EOS config value is missing." << "\n";
		return false;
	}

	return true;
}

// 1. 설정 파일 읽기
bool LoadDbPassword(string& Password)
{
	ifstream DbConfigFile("Config/DBSecrets.ini");

	if (!DbConfigFile.is_open())
	{
		std::cout << "Config : Failed open Config/DBSecrets.ini" << "\n";
		return false;
	}

	getline(DbConfigFile, Password);

	if (Password.empty())
	{
		std::cout << "Config : PW empty" << "\n";
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
		std::cout << "MySQL : Driver is null." << "\n";
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

bool SaveOrUpdateEosUser(sql::Connection* Conn, int AccountId, const string& EosPuid)
{
	try
	{
		unique_ptr<sql::PreparedStatement> Statement(
			Conn->prepareStatement(
				"INSERT INTO account_eos "
				"(account_id, eos_puid)"
				"VALUES (?, ?) "
				"ON DUPLICATE KEY UPDATE "
				"eos_puid = VALUES(eos_puid)"
			)
		);

		Statement->setInt(1, AccountId);
		Statement->setString(2, EosPuid);

		Statement->executeUpdate();

		return true;
	}
	catch (sql::SQLException& Error)
	{
		cerr << "SaveOrUpdateEosUser : DB Error : " << Error.what() << endl;

		return false;
	}
}

// SessionToken 생성 함수
string GenerateSessionToken()
{
	array<unsigned char, 32> RandomBytes{};

	if (BCryptGenRandom(nullptr, RandomBytes.data(), static_cast<ULONG>(RandomBytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
	{
		return "";
	}

	static const char Hex[] = "0123456789abcdef";

	string Token;
	Token.reserve(RandomBytes.size() * 2);

	for (unsigned char Byte : RandomBytes)
	{
		Token.push_back(Hex[(Byte >> 4) & 0x0F]);
		Token.push_back(Hex[Byte & 0x0F]);
	}

	return Token;
}

// Session 저장/조회 함수
void SaveSession(const string& Token, int AccountId)
{
	lock_guard<mutex> Lock(SessionMutex);

	SessionMap[Token] = AccountId;
}

bool GetAccountIdFromSession(const string& Token, int& OutAccountId)
{
	lock_guard<mutex> Lock(SessionMutex);

	auto It = SessionMap.find(Token);

	if (It == SessionMap.end())
	{
		return false;
	}

	OutAccountId = It->second;

	return true;
}

bool InitializeEOS()
{
	EOS_InitializeOptions Options{};
	Options.ApiVersion = EOS_INITIALIZE_API_LATEST;
	Options.ProductName = "RouteBackendServer";
	Options.ProductVersion = "1.0";

	const EOS_EResult Result = EOS_Initialize(&Options);

	if (Result != EOS_EResult::EOS_Success)
	{
		cout << "EOS : Initialize failed. Result: " << EOS_EResult_ToString(Result) << "\n";
		return false;
	}

	cout << "EOS : Initialize succeeded." << "\n";

	return true;
}

// EOS_HPlayfrom 생성
EOS_HPlatform CreateEosPlatform(const EosServerConfig& Config)
{
	EOS_Windows_RTCOptions WindowsRtcOptions{};
	WindowsRtcOptions.ApiVersion = EOS_WINDOWS_RTCOPTIONS_API_LATEST;
	WindowsRtcOptions.XAudio29DllPath =
		"E:\\GitHub\\Route\\Route_BackendServer\\x64\\Release\\xaudio2_9redist.dll";

	// RTC 설정
	EOS_Platform_RTCOptions RtcOptions{};
	RtcOptions.ApiVersion = EOS_PLATFORM_RTCOPTIONS_API_LATEST;
	RtcOptions.PlatformSpecificOptions = &WindowsRtcOptions;

	// Platform 설정
	EOS_Platform_Options Option{};
	Option.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;

	Option.ProductId = Config.ProductId.c_str();
	Option.SandboxId = Config.SandboxId.c_str();
	Option.DeploymentId = Config.DeploymentId.c_str();

	Option.ClientCredentials.ClientId = Config.ClientId.c_str();
	Option.ClientCredentials.ClientSecret = Config.ClientSecret.c_str();

	//Trusted Server : RouteTrustedServer Client/Policy 를 서버 모드로 생성.
	Option.bIsServer = EOS_TRUE;

	// RTC 활성화
	Option.RTCOptions = &RtcOptions;

	EOS_HPlatform Platform = EOS_Platform_Create(&Option);

	if (Platform == nullptr)
	{
		cout << "EOS : Playform create failed." << "\n";
		return nullptr;
	}

	cout << "EOS : Platform created successfully." << "\n";

	return Platform;
}

void EOS_CALL EosLogCallback(const EOS_LogMessage* Message)
{
	if (Message == nullptr)
	{
		return;
	}

	cout
		<< "EOS SDK [" << Message->Category << "] "
		<< Message->Message
		<< "\n";
}

bool GetEosPuidByAccountId(int AccountId, string& OutEosPuid)
{
	try
	{
		unique_ptr<sql::Connection> Conn =
			CreateMySqlConnection();

		if (!Conn)
		{
			return false;
		}

		unique_ptr<sql::PreparedStatement> Statement(
			Conn->prepareStatement(
				"SELECT eos_puid "
				"FROM account_eos "
				"WHERE account_id = ?"
			)
		);

		Statement->setInt(1, AccountId);

		unique_ptr<sql::ResultSet> Result(
			Statement->executeQuery()
		);

		if (!Result->next())
		{
			return false;
		}

		OutEosPuid =
			Result->getString("eos_puid").asStdString();

		return !OutEosPuid.empty();
	}
	catch (const sql::SQLException& Err)
	{
		cout
			<< "MySQL : Get EOS PUID failed. "
			<< Err.what()
			<< "\n";

		return false;
	}
}

// Queue 함수
void EnqueueVoiceJoinRequest(const shared_ptr<VoiceJoinRequest>& Request)
{
	lock_guard<mutex> Lock(VoiceJoinQueueMutex);

	VoiceJoinQueue.push(Request);
}

void EOS_CALL OnQueryJoinRoomTokenComplete(const EOS_RTCAdmin_QueryJoinRoomTokenCompleteCallbackInfo* Data)
{
	unique_ptr<VoiceJoinCallbackContext> Context(static_cast<VoiceJoinCallbackContext*>(Data->ClientData));

	shared_ptr<VoiceJoinRequest> Request = Context->Request;

	bool bSuccess = false;
	string ClientBaseUrl;
	string ParticipantToken;
	string RoomName;
	string ErrorMessage;

	if (Data->ResultCode != EOS_EResult::EOS_Success)
	{
		ErrorMessage = EOS_EResult_ToString(Data->ResultCode);
	}
	else if (Data->TokenCount == 0)
	{
		ErrorMessage = "no voice token returned";
	}
	else
	{
		EOS_RTCAdmin_CopyUserTokenByIndexOptions CopyOptions{};
		CopyOptions.ApiVersion = EOS_RTCADMIN_COPYUSERTOKENBYINDEX_API_LATEST;

		CopyOptions.QueryId = Data->QueryId;
		CopyOptions.UserTokenIndex = 0;

		EOS_RTCAdmin_UserToken* UserToken = nullptr;

		const EOS_EResult CopyResult = EOS_RTCAdmin_CopyUserTokenByIndex(
			Context->RtcAdmin,
			&CopyOptions,
			&UserToken
		);

		if (CopyResult == EOS_EResult::EOS_Success && UserToken != nullptr)
		{
			ClientBaseUrl = Data->ClientBaseUrl != nullptr ? Data->ClientBaseUrl : "";

			RoomName = Data->RoomName != nullptr ? Data->RoomName : "";

			ParticipantToken = UserToken->Token != nullptr ? UserToken->Token : "";

			bSuccess = !ClientBaseUrl.empty() && !ParticipantToken.empty();

			EOS_RTCAdmin_UserToken_Release(UserToken);
		}
		else
		{
			ErrorMessage = EOS_EResult_ToString(CopyResult);
		}
	}

	{
		lock_guard<mutex> Lock(Request->RequestMutex);

		Request->bSuccess = bSuccess;
		Request->ClientBaseUrl = ClientBaseUrl;
		Request->ParticipantToken = ParticipantToken;
		Request->RoomName = RoomName;
		Request->ErrorMessage = ErrorMessage;
		Request->bProcessed = true;
	}

	Request->RequestCondition.notify_one();
}

// Main Thread 에서 Queue 처리
void ProcessVoiceJoinRequests(EOS_HRTCAdmin RtcAdmin)
{
	while (true)
	{
		shared_ptr<VoiceJoinRequest> Request;

		{
			lock_guard<mutex> Lock(VoiceJoinQueueMutex);

			if (VoiceJoinQueue.empty())
			{
				break;
			}

			Request = VoiceJoinQueue.front();
			VoiceJoinQueue.pop();
		}

		cout << "Voice : Main thread received join request. AccountId: " << Request->AccountId << "\n";

		EOS_ProductUserId ProductUserId = EOS_ProductUserId_FromString(
			Request->EosPuid.c_str()
		);

		if (!EOS_ProductUserId_IsValid(ProductUserId))
		{
			{
				lock_guard<mutex> Lock(Request->RequestMutex);
				
				Request->bSuccess = false;
				Request->ErrorMessage = "invalid EOS PUID";
				Request->bProcessed = true;
			}

			Request->RequestCondition.notify_one();
			continue;
		}

		const char* RoomName = "RouteVoiceRoom";

		EOS_ProductUserId TargetUserIds[1] =
		{
			ProductUserId
		};

		EOS_RTCAdmin_QueryJoinRoomTokenOptions Options{};

		Options.ApiVersion = EOS_RTCADMIN_QUERYJOINROOMTOKEN_API_LATEST;

		Options.LocalUserId = nullptr;
		Options.RoomName = RoomName;

		Options.TargetUserIds = TargetUserIds;
		Options.TargetUserIdsCount = 1;

		Options.TargetUserIpAddresses = nullptr;

		VoiceJoinCallbackContext* Context = new VoiceJoinCallbackContext();

		Context->RtcAdmin = RtcAdmin;
		Context->Request = Request;

		EOS_RTCAdmin_QueryJoinRoomToken(
			RtcAdmin,
			&Options,
			Context,
			OnQueryJoinRoomTokenComplete
		);
	}
}

int main()
{
	EosServerConfig EosConfig;

	if (!LoadEosConfig(EosConfig))
	{
		cout << "EOS : Failed to load Eos config." << "\n";
		return 1;
	}

	cout << "EOS : Config loaded successfully. " << "\n";

	if (!InitializeEOS())
	{
		return 1;
	}

	EOS_Logging_SetCallback(EosLogCallback);

	EOS_Logging_SetLogLevel(
		EOS_ELogCategory::EOS_LC_ALL_CATEGORIES,
		EOS_ELogLevel::EOS_LOG_VeryVerbose
	);

	EOS_HPlatform EosPlatform = CreateEosPlatform(EosConfig);

	if (EosPlatform == nullptr)
	{
		EOS_Shutdown();
		return 1;
	}

	//
	// RTC 일반 Interface 확인
	EOS_HRTC Rtc = EOS_Platform_GetRTCInterface(EosPlatform);

	cout << "EOS : RTC Interface = " << (Rtc != nullptr ? "Valid" : "Null") << "\n";

	EOS_HRTCAdmin RtcAdmin = EOS_Platform_GetRTCAdminInterface(EosPlatform);

	//
	cout << "EOS : RTC Admin Interface = " << (RtcAdmin != nullptr ? "Valid" : "Null") << "\n";

	if (RtcAdmin == nullptr)
	{
		cout << "EOS : Failed to get RTC Admin interface." << "\n";

		EOS_Platform_Release(EosPlatform);
		EOS_Shutdown();

		return 1;
	}

	cout << "EOS : RTC Admin interface acquired." << "\n";

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

	// POST /login 로그인
	Svr.Post("/login", [](const httplib::Request& Req, httplib::Response& Res)
	{
		try
		{
			// Request Body 의 JSON 파싱
			const json ReqJson = json::parse(Req.body);

			// 필수 필드 확인
			if (!ReqJson.contains("login_id") || !ReqJson.contains("password"))
			{
				Res.status = 400;
				Res.set_content(
					R"({"success":false,"message":"invalid request"})",
					"application/json"
				);
				return;
			}

			// JSON 값 추출
			const string LoginID = ReqJson["login_id"].get<string>();
			const string Password = ReqJson["password"].get<string>();

			// 빈 문자열 확인
			if (LoginID.empty() || Password.empty())
			{
				Res.status = 400;
				Res.set_content(
					R"({"success":false,"message":"invalid request"})",
					"application/json"
				);
				return;
			}
			
			// MySQL 연결
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

			// ID와 비밀번호가 모두 일치하는 계정 조회
			unique_ptr<sql::PreparedStatement> LoginStmt(
				Conn->prepareStatement(
					"SELECT account_id, nickname "
					"FROM accounts "
					"WHERE login_id = ? "
					"AND pw_hash = SHA2(?, 256)"
				)
			);

			LoginStmt->setString(1, LoginID);
			LoginStmt->setString(2, Password);

			unique_ptr<sql::ResultSet> Result(LoginStmt->executeQuery());

			// 조회 결과가 없으면 로그인 실패
			if (!Result->next())
			{
				Res.status = 401;
				Res.set_content(
					R"({"success":false,"message":"invalid credentials"})",
					"application/json"
				);
				return;
			}

			// 로그인 성공 정보 가져오기
			const int AccountId = Result->getInt("account_id");
			const string Nickname = Result->getString("nickname").asStdString();

			// /login 성송 시 SessionToken 생성

			const string SessionToken = GenerateSessionToken();

			if (SessionToken.empty())
			{
				Res.status = 500;
				Res.set_content(
					R"({"success":false,"message":"session token generation failed"})",
					"application/json"
				);
				return;
			}

			SaveSession(SessionToken, AccountId);

			// JSON 응답 생성
			json ResponseJson;
			ResponseJson["success"] = true;
			ResponseJson["account_id"] = AccountId;
			ResponseJson["nickname"] = Nickname;
			ResponseJson["session_token"] = SessionToken;

			Res.status = 200;
			Res.set_content(
				ResponseJson.dump(),
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
			cout << "MySQL : Login failed. " << Err.what() << endl;

			Res.status = 500;
			Res.set_content(
				R"({"success":false,"message":"database error"})",
				"application/json"
			);
		}
	});

	// POST /voice/register-user EOS PUID 매핑
	Svr.Post("/voice/register-user", [](const httplib::Request& Req, httplib::Response& Res)
	{
		try
		{
			// 1. Authorization Header 확인
			const string Authorization = Req.get_header_value("Authorization");
			const string Prefix = "Bearer ";

			if (Authorization.rfind(Prefix, 0) != 0)
			{
				Res.status = 401;
				Res.set_content(
					R"({"success":false,"message":"missing session token"})",
					"application/json"
				);
				return;
			}

			// 2. SessionToken 추출
			const string SessionToken = Authorization.substr(Prefix.length());

			// 3. SessionToken 으로 AccountId 조회
			int AccountId = 0;

			if (!GetAccountIdFromSession(SessionToken, AccountId))
			{
				Res.status = 401;
				Res.set_content(
					R"({"success":false,"message":"invalid session token"})",
					"application/json"
				);
				return;
			}

			// 4. Request Body JSON 파싱
			const json ReqJson = json::parse(Req.body);

			// 이제 account_id 는 클라이언트가 보내지 않음

			//if (!ReqJson.contains("account_id") || !ReqJson.contains("eos_puid"))
			if (!ReqJson.contains("eos_puid"))
			{
				Res.status = 400;
				Res.set_content(
					R"({"success":false,"message":"invalid request"})",
					"application/json"
				);
				return;
			}

			//const int AccountId = ReqJson["account_id"].get<int>();
			const string EosPuid = ReqJson["eos_puid"].get<string>();

			//if (AccountId <= 0 || EosPuid.empty())
			if (EosPuid.empty())
			{
				Res.status = 400;
				Res.set_content(
					R"({"success":false,"message":"invalid request"})",
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

			// 6. SessionToken 에서 얻은 AccountId 와 PUID 저장
			unique_ptr<sql::PreparedStatement> Statement(
				Conn->prepareStatement(
					"INSERT INTO account_eos (account_id, eos_puid) "
					"VALUES (?, ?) "
					"ON DUPLICATE KEY UPDATE "
					"eos_puid = VALUES(eos_puid)"
				)
			);

			Statement->setInt(1, AccountId);
			Statement->setString(2, EosPuid);

			Statement->executeUpdate();

			Res.status = 200;
			Res.set_content(
				R"({"success":true,"message":"EOS user registered"})",
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
			cout << "MySQL : EOS user register failed. " << Err.what() << "\n";

			Res.status = 500;
			Res.set_content(
				R"({"success":false,"message":"database error"})",
				"application/json"
			);
		}
	});

	// /voice/join
	Svr.Post("/voice/join", [](const httplib::Request& Req, httplib::Response& Res)
		{
			// 1. Authoriaztion Header 확인
			const string Authorization = Req.get_header_value("Authorization");

			const string Prefix = "Bearer ";

			if (Authorization.rfind(Prefix, 0) != 0)
			{
				Res.status = 401;
				Res.set_content(
					R"({"success":false,"message":"missing session token"})",
					"application/json"
				);
				return;
			}

			// 2. SessionToken 검증 [잘못된 Token]
			const string SessionToken = Authorization.substr(Prefix.length());

			int AccountId = 0;
			if (!GetAccountIdFromSession(SessionToken, AccountId))
			{
				Res.status = 401;
				Res.set_content(
					R"({"success":false,"message":"invalid session token"})",
					"application/json"
				);
				return;
			}

			//3. AccountId -> EOS PUID 조회 [account_eos 매핑 없는 계정]
			string EosPuid;

			if (!GetEosPuidByAccountId(AccountId, EosPuid))
			{
				Res.status = 404;
				Res.set_content(
					R"({"success":false,"message":"EOS user not registered"})",
					"application/json"
				);
				return;
			}

			cout << "Voice : Join request validated. AccountId: " << AccountId << "\n";

			// 4. Main Thread로 전달할 요청 생성
			shared_ptr<VoiceJoinRequest> VoiceRequest = make_shared<VoiceJoinRequest>();

			VoiceRequest->AccountId = AccountId;
			VoiceRequest->EosPuid = EosPuid;

			// 5. Queue에 요청 등록
			EnqueueVoiceJoinRequest(VoiceRequest);

			// 6. Main Thread의 처리 완료 대기
			unique_lock<mutex> Lock(VoiceRequest->RequestMutex);

			const bool bProcessed = VoiceRequest->RequestCondition.wait_for(
				Lock, 
				chrono::seconds(5), 
				[&]() 
				{ 
					return VoiceRequest->bProcessed;
				}
			);

			if (!bProcessed)
			{
				Res.status = 504;
				Res.set_content(
					R"({"success":false,"message":"voice request timeout"})",
					"application/json"
				);
				return;
			}

			// --
			if (!VoiceRequest->bSuccess)
			{
				Res.status = 500;

				json ResponseJson;
				ResponseJson["success"] = false;
				ResponseJson["message"] =
					VoiceRequest->ErrorMessage;

				Res.set_content(
					ResponseJson.dump(),
					"application/json"
				);

				return;
			}

			json ResponseJson;

			ResponseJson["success"] = true;
			ResponseJson["room_name"] =
				VoiceRequest->RoomName;

			ResponseJson["client_base_url"] =
				VoiceRequest->ClientBaseUrl;

			ResponseJson["participant_token"] =
				VoiceRequest->ParticipantToken;

			Res.status = 200;
			Res.set_content(
				ResponseJson.dump(),
				"application/json"
			);

			//// 7. 처리 완료
			//Res.status = 200;
			//Res.set_content(
			//	R"({"success":true,"message":"voice join request processed"})",
			//	"application/json"
			//);
		}
	);

	cout << "========================================" << "\n";
	cout << " RouteBackendServer started." << "\n";
	cout << " Listening on http://localhost:8080" << "\n";
	cout << " Health Check: http://localhost:8080/health" << "\n";
	cout << "========================================" << "\n";

	atomic<bool> bServerRunning = true;
	bool bListenSucceeded = false;

	// HTTP 서버는 별도 스레드에서 실행
	thread HttpThread([&]()
		{
			bListenSucceeded = Svr.listen("0.0.0.0", 8080);

			bServerRunning = false;
		});

	// EOS Platform을 생성한 main thread에서 Tick
	while (bServerRunning)
	{
		ProcessVoiceJoinRequests(RtcAdmin);

		EOS_Platform_Tick(EosPlatform);

		this_thread::sleep_for(chrono::milliseconds(10));
	}

	if (HttpThread.joinable())
	{
		HttpThread.join();
	}

	// EOS 종료 : 차후 종료 처리 개선 필요.
	EOS_Platform_Release(EosPlatform);
	EOS_Shutdown();

    return bListenSucceeded ? 0 : 1;
}


