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

/**
 * Gets the sheet name from a spreadsheet ID and sheet ID
 * @param spreadsheet_id The spreadsheet ID
 * @param sheet_id The sheet ID
 * @param token The Google API token
 * @return The sheet name
 */
std::string get_sheet_name_from_id(const std::string& spreadsheet_id, const std::string& sheet_id, const std::string& token);

/**
 * Gets the sheet ID from a spreadsheet ID and sheet name
 * @param spreadsheet_id The spreadsheet ID
 * @param sheet_name The sheet name
 * @param token The Google API token
 * @return The sheet ID
 */
std::string get_sheet_id_from_name(const std::string& spreadsheet_id, const std::string& sheet_name, const std::string& token);

struct SheetData {
    std::string range;
    std::string majorDimension;
    std::vector<std::vector<std::string>> values;
};

SheetData getSheetData(const json& j);

/**
 * Parses a JSON string into a json object
 * @param json_str The JSON string
 * @return The parsed json object
 */
json parseJson(const std::string& json_str);

/**
 * Generates a random string of specified length using alphanumeric characters.
 * @param length The length of the random string to generate
 * @return A random string of the specified length
 */
std::string generate_random_string(size_t length);


/**
 * Encodes a string to be used in a URL
 * @param str The string to encode
 * @return The encoded string
 */
std::string url_encode(const std::string& str);

} // namespace duckdb