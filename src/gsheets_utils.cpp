#include "gsheets_utils.hpp"
#include "gsheets_requests.hpp"
#include "duckdb/common/exception.hpp"
#include <regex>
#include <json.hpp>
#include <iostream>
#include <sstream>

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
    if (input.find("docs.google.com/spreadsheets/d/") != std::string::npos && input.find("gid=") != std::string::npos) {
        std::regex sheet_id_regex("gid=([0-9]+)");
        std::smatch match;
        if (std::regex_search(input, match, sheet_id_regex) && match.size() > 1) {
            return match.str(1);
        }
    }
    return "0";
}

std::string get_sheet_name_from_id(const std::string& spreadsheet_id, const std::string& sheet_id, const std::string& token) {
    std::string metadata_response = get_spreadsheet_metadata(spreadsheet_id, token);
    json metadata = parseJson(metadata_response);
    for (const auto& sheet : metadata["sheets"]) {
        if (sheet["properties"]["sheetId"].get<int>() == std::stoi(sheet_id)) {
            return sheet["properties"]["title"].get<std::string>();
        }
    }
    throw duckdb::InvalidInputException("Sheet with ID %s not found", sheet_id);
}

std::string get_sheet_id_from_name(const std::string& spreadsheet_id, const std::string& sheet_name, const std::string& token) {
    std::string metadata_response = get_spreadsheet_metadata(spreadsheet_id, token);
    json metadata = parseJson(metadata_response);
    for (const auto& sheet : metadata["sheets"]) {
        if (sheet["properties"]["title"].get<std::string>() == sheet_name) {
            return sheet["properties"]["sheetId"].get<std::string>();
        }
    }
    throw duckdb::InvalidInputException("Sheet with name %s not found", sheet_name);
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

std::string url_encode(const std::string& str) {
    std::string encoded;
    for (char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else {
            std::stringstream ss;
            ss << std::hex << std::uppercase << static_cast<int>(static_cast<unsigned char>(c));
            encoded += '%' + ss.str();
        }
    }
    return encoded;
}

} // namespace duckdb