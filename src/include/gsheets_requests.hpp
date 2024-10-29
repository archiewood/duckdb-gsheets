#pragma once

#include <string>

namespace duckdb {

enum class HttpMethod {
        GET,
        POST,
        PUT
    };

std::string perform_https_request(const std::string& host, const std::string& path, const std::string& token, 
                                  HttpMethod method = HttpMethod::GET, const std::string& body = "", const std::string& content_type = "application/json");

std::string call_sheets_api(const std::string& spreadsheet_id, const std::string& token, const std::string& sheet_name, HttpMethod method = HttpMethod::GET, const std::string& body = "");

std::string delete_sheet_data(const std::string& spreadsheet_id, const std::string& token, const std::string& sheet_name);

std::string get_spreadsheet_metadata(const std::string& spreadsheet_id, const std::string& token);
}