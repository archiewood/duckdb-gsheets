#include "gsheets_utils.hpp"
#include "duckdb/common/exception.hpp"
#include <regex>
#include <json.hpp>
#include <iostream>

using json = nlohmann::json;
namespace duckdb {

std::string extract_spreadsheet_id(const std::string& input) {
    // Check if the input is already a sheet ID (no slashes)
    if (input.find('/') == std::string::npos) {
        return input;
    }

    // Regular expression to match the spreadsheet ID in a Google Sheets URL
    if(input.find("docs.google.com/spreadsheets/d/") != std::string::npos) {
        std::regex spreadsheet_id_regex("/d/([a-zA-Z0-9-_]+)");
        std::smatch match;

        if (std::regex_search(input, match, spreadsheet_id_regex) && match.size() > 1) {
            return match.str(1);
        }
    }

    throw duckdb::InvalidInputException("Invalid Google Sheets URL or ID");
}

std::string extract_sheet_id(const std::string& input) {
    if (input.find("docs.google.com/spreadsheets/d/") != std::string::npos && input.find("edit?gid=") != std::string::npos) {
        std::regex sheet_id_regex("gid=([0-9]+)");
        std::smatch match;
        if (std::regex_search(input, match, sheet_id_regex) && match.size() > 1) {
            return match.str(1);
        }
    }
    throw duckdb::InvalidInputException("Invalid Google Sheets URL or ID");
}

json parseJson(const std::string& json_str) {
    try {
        // Find the start of the JSON object
        size_t start = json_str.find('{');
        if (start == std::string::npos) {
            throw std::runtime_error("No JSON object found in the response");
        }

        // Find the end of the JSON object
        size_t end = json_str.rfind('}');
        if (end == std::string::npos) {
            throw std::runtime_error("No closing brace found in the JSON response");
        }

        // Extract the JSON object
        std::string clean_json = json_str.substr(start, end - start + 1);

        json j = json::parse(clean_json);
        return j;
    } catch (const json::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        std::cerr << "Raw JSON string: " << json_str << std::endl;
        throw;
    }
}

SheetData getSheetData(const json& j) {
    SheetData result;
    if (j.contains("range") && j.contains("majorDimension") && j.contains("values")) {
        result.range = j["range"].get<std::string>();
        result.majorDimension = j["majorDimension"].get<std::string>();
        result.values = j["values"].get<std::vector<std::vector<std::string>>>();
    } else if (j.contains("error")) {
        string message = j["error"]["message"].get<std::string>();
            int code = j["error"]["code"].get<int>();
            throw std::runtime_error("Google Sheets API error: " + std::to_string(code) + " - " + message);
        } else {
        std::cerr << "JSON does not contain expected fields" << std::endl;
        std::cerr << "Raw JSON string: " << j.dump() << std::endl;
        throw;
    }
    return result;
}

std::string generate_random_string(size_t length) {
    static const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, sizeof(charset) - 2);

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result.push_back(charset[distribution(generator)]);
    }
    return result;
}

} // namespace duckdb
