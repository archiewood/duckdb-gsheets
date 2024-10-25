#pragma once

#include <string>
#include <vector>
#include <json.hpp>
#include <random>

using json = nlohmann::json;

namespace duckdb {

/**
 * Extracts the sheet ID from a Google Sheets URL or returns the input if it's already a sheet ID.
 * @param input A Google Sheets URL or sheet ID
 * @return The extracted sheet ID
 * @throws InvalidInputException if the input is neither a valid URL nor a sheet ID
 */
std::string extract_spreadsheet_id(const std::string& input);


/**
 * Extracts the sheet ID from a Google Sheets URL
 * @param input A Google Sheets URL
 * @return The extracted sheet ID
 */
std::string extract_sheet_id(const std::string& input);

struct SheetData {
    std::string range;
    std::string majorDimension;
    std::vector<std::vector<std::string>> values;
};

SheetData getSheetData(const json& j);

json parseJson(const std::string& json_str);

/**
 * Generates a random string of specified length using alphanumeric characters.
 * @param length The length of the random string to generate
 * @return A random string of the specified length
 */
std::string generate_random_string(size_t length);

} // namespace duckdb
