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

}