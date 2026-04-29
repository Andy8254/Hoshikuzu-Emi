#include "core/api_fetcher.hpp"
#include <curl/curl.h>

size_t HttpClient::write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
	size_t total_size = size * nmemb;
	std::string* str = static_cast<std::string*>(userp);
	str->append(static_cast<char*>(contents), total_size);
	return total_size;
}

HttpResponse HttpClient::get(const std::string& url) {
	CURL* curl = curl_easy_init();
	HttpResponse response{};

	if (!curl) {
		response.error = "Failed to init CURL";
		return response;
	}

	std::string buffer;

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

	CURLcode res = curl_easy_perform(curl);

	if (res != CURLE_OK) {
		response.error = curl_easy_strerror(res);
	}
	else {
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
		response.body = buffer;
	}

	curl_easy_cleanup(curl);
	return response;
}