#include "core/api_fetcher.hpp"

#include "core/Log.hpp"

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
		bot_log::error("http", "get_init_failed", "url=" + url);
		return response;
	}

	std::string buffer;
	bot_log::info("http", "get_start", "url=" + url);

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Hoshikuzu-Emi/1.0");
	curl_easy_setopt(curl, CURLOPT_PROXY, "");

	CURLcode res = curl_easy_perform(curl);

	if (res != CURLE_OK) {
		response.error = curl_easy_strerror(res);
		bot_log::error("http", "get_failed", "url=" + url + " error=\"" + response.error + "\"");
	}
	else {
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
		response.body = buffer;
		bot_log::info("http", "get_done", "url=" + url + " status=" + std::to_string(response.status_code) + " bytes=" + std::to_string(response.body.size()));
	}

	curl_easy_cleanup(curl);
	return response;
}

HttpResponse HttpClient::post_json(const std::string& url, const std::string& json_body) {
	CURL* curl = curl_easy_init();
	HttpResponse response{};

	if (!curl) {
		response.error = "Failed to init CURL";
		bot_log::error("http", "post_json_init_failed", "url=" + url);
		return response;
	}

	std::string buffer;
	bot_log::info("http", "post_json_start", "url=" + url + " bytes=" + std::to_string(json_body.size()));

	curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, "Accept: application/json");

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(json_body.size()));
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Hoshikuzu-Emi/1.0");
	curl_easy_setopt(curl, CURLOPT_PROXY, "");

	CURLcode res = curl_easy_perform(curl);

	if (res != CURLE_OK) {
		response.error = curl_easy_strerror(res);
		bot_log::error("http", "post_json_failed", "url=" + url + " error=\"" + response.error + "\"");
	}
	else {
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
		response.body = buffer;
		bot_log::info("http", "post_json_done", "url=" + url + " status=" + std::to_string(response.status_code) + " bytes=" + std::to_string(response.body.size()));
	}

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	return response;
}
