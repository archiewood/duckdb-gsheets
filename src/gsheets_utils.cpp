#include "gsheets_utils.hpp"
#include "duckdb/common/exception.hpp"
#include <regex>

namespace duckdb {

std::string extract_sheet_id(const std::string& input) {
    // Check if the input is already a sheet ID (no slashes)
    if (input.find('/') == std::string::npos) {
        return input;
    }

    // Regular expression to match the sheet ID in a Google Sheets URL
    if(input.find("docs.google.com/spreadsheets/d/") != std::string::npos) {
        std::regex sheet_id_regex("/d/([a-zA-Z0-9-_]+)");
        std::smatch match;

        if (std::regex_search(input, match, sheet_id_regex) && match.size() > 1) {
            return match.str(1);
        }
    }

    throw duckdb::InvalidInputException("Invalid Google Sheets URL or ID");
}

} // namespace duckdb