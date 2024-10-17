#pragma once

#include <string>

namespace duckdb {

enum class HttpMethod {
        GET,
        POST,
        PUT
    };

std::string perform_https_request(const std::string& host, const std::string& path, const std::string& token, 
                                  HttpMethod method = HttpMethod::GET, const std::string& body = "");

std::string fetch_sheet_data(const std::string& sheet_id, const std::string& token, const std::string& sheet_name);
}