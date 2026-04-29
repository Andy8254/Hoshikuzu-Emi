#pragma once
#include <string>
#include <map>

struct HttpResponse {
	long status_code;
	std::string body;
	std::string error;
};

class HttpClient {
public:
	static HttpResponse get(const std::string& url);

private:
	static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
};