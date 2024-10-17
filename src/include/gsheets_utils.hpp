#pragma once

#include <string>

namespace duckdb {

/**
 * Extracts the sheet ID from a Google Sheets URL or returns the input if it's already a sheet ID.
 * @param input A Google Sheets URL or sheet ID
 * @return The extracted sheet ID
 * @throws InvalidInputException if the input is neither a valid URL nor a sheet ID
 */
std::string extract_sheet_id(const std::string& input);

} // namespace duckdb