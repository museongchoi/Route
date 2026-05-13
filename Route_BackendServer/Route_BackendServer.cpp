#include <iostream>

#pragma comment(lib, "ws2_32.lib")

#include "httplib.h"

using namespace std;

int main()
{
	httplib::Server Svr;
	Svr.Get("/health", [](const httplib::Request& Req, httplib::Response& Res)
	{
		Res.set_content(
			R"({"status":"ok","server":"RouteBackendServer"})",
			"application/json"
		);
	});

	cout << "========================================" << endl;
	cout << " RouteBackendServer started." << endl;
	cout << " Listening on http://localhost:8080" << endl;
	cout << " Health Check: http://localhost:8080/health" << endl;
	cout << "========================================" << endl;
	
	if (!Svr.listen("0.0.0.0", 8080))
	{
		cout << "Failed to start RouteBackendServer." << endl;
		return 1;
	}

    return 0;
}


