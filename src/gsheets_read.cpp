#include "gsheets_read.hpp"
#include "gsheets_utils.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/main/secret/secret_manager.hpp"
#include "gsheets_requests.hpp"
#include <json.hpp>

namespace duckdb {

using json = nlohmann::json;

ReadSheetBindData::ReadSheetBindData(string sheet_id, string token, bool header, string sheet_name) 
    : sheet_id(sheet_id), token(token), finished(false), row_index(0), header(header), sheet_name(sheet_name) {
    response = fetch_sheet_data(sheet_id, token, sheet_name);
}

SheetData parseJson(const std::string& json_str) {
    SheetData result;
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
            std::cerr << "Raw JSON string: " << json_str << std::endl;
            throw;
        }
    } catch (const json::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        std::cerr << "Raw JSON string: " << json_str << std::endl;
        throw;
    }

    return result;
}


void ReadSheetFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
    auto &bind_data = const_cast<ReadSheetBindData&>(data_p.bind_data->Cast<ReadSheetBindData>());

    if (bind_data.finished) {
        return;
    }

    SheetData sheet_data = parseJson(bind_data.response);

    idx_t row_count = 0;
    idx_t column_count = output.ColumnCount();

    // Adjust starting index based on whether we're using the header
    idx_t start_index = bind_data.header ? bind_data.row_index + 1 : bind_data.row_index;

    // Determine column types
    vector<LogicalType> column_types(column_count, LogicalType::VARCHAR);
    if (start_index < sheet_data.values.size()) {
        const auto& first_data_row = sheet_data.values[start_index];
        for (idx_t col = 0; col < column_count && col < first_data_row.size(); col++) {
            const string& value = first_data_row[col];
            if (value == "true" || value == "false") {
                column_types[col] = LogicalType::BOOLEAN;
            } else if (value.find_first_not_of("0123456789.+-eE") == string::npos) {
                column_types[col] = LogicalType::DOUBLE;
            }
        }
    }

    for (idx_t i = start_index; i < sheet_data.values.size() && row_count < STANDARD_VECTOR_SIZE; i++) {
        const auto& row = sheet_data.values[i];
        for (idx_t col = 0; col < column_count; col++) {
            if (col < row.size()) {
                const string& value = row[col];
                switch (column_types[col].id()) {
                    case LogicalTypeId::BOOLEAN:
                        output.SetValue(col, row_count, Value::BOOLEAN(value == "true"));
                        break;
                    case LogicalTypeId::DOUBLE:
                        output.SetValue(col, row_count, Value::DOUBLE(std::stod(value)));
                        break;
                    default:
                        output.SetValue(col, row_count, Value(value));
                        break;
                }
            } else {
                output.SetValue(col, row_count, Value(nullptr));
            }
        }
        row_count++;
    }

    bind_data.row_index += row_count;
    bind_data.finished = (bind_data.row_index >= (sheet_data.values.size() - (bind_data.header ? 1 : 0)));

    output.SetCardinality(row_count);
}

unique_ptr<FunctionData> ReadSheetBind(ClientContext &context, TableFunctionBindInput &input,
                                              vector<LogicalType> &return_types, vector<string> &names) {
    auto sheet_input = input.inputs[0].GetValue<string>();
    
    // Default values
    bool header = true;
    string sheet = "Sheet1";

    // Parse named parameters
    for (auto &kv : input.named_parameters) {
        if (kv.first == "header") {
            try {
                header = kv.second.GetValue<bool>();
            } catch (const std::exception& e) {
                throw InvalidInputException("Invalid value for 'header' parameter. Expected a boolean value.");
            }
        } else if (kv.first == "sheet") {
            sheet = kv.second.GetValue<string>();
        }
    }

    // Extract the sheet ID from the input (URL or ID)
    std::string sheet_id = extract_sheet_id(sheet_input);

    // Use the SecretManager to get the token
    auto &secret_manager = SecretManager::Get(context);
    auto transaction = CatalogTransaction::GetSystemCatalogTransaction(context);
    auto secret_match = secret_manager.LookupSecret(transaction, "gsheet", "gsheet");
    
    if (!secret_match.HasMatch()) {
        throw InvalidInputException("No 'gsheet' secret found. Please create a secret with 'CREATE SECRET' first.");
    }

    auto &secret = secret_match.GetSecret();
    if (secret.GetType() != "gsheet") {
        throw InvalidInputException("Invalid secret type. Expected 'gsheet', got '%s'", secret.GetType());
    }

    const auto *kv_secret = dynamic_cast<const KeyValueSecret*>(&secret);
    if (!kv_secret) {
        throw InvalidInputException("Invalid secret format for 'gsheet' secret");
    }

    Value token_value;
    if (!kv_secret->TryGetValue("token", token_value)) {
        throw InvalidInputException("'token' not found in 'gsheet' secret");
    }

    std::string token = token_value.ToString();

    auto bind_data = make_uniq<ReadSheetBindData>(sheet_id, token, header, sheet);

    SheetData sheet_data = parseJson(bind_data->response);

    if (!sheet_data.values.empty()) {
        idx_t start_index = header ? 1 : 0;
        if (start_index < sheet_data.values.size()) {
            const auto& first_data_row = sheet_data.values[start_index];
            for (size_t i = 0; i < first_data_row.size(); i++) {
                string column_name = header ? sheet_data.values[0][i] : "column" + std::to_string(i + 1);
                names.push_back(column_name);
                
                const string& value = first_data_row[i];
                if (value == "true" || value == "false") {
                    return_types.push_back(LogicalType::BOOLEAN);
                } else if (value.find_first_not_of("0123456789.+-eE") == string::npos) {
                    return_types.push_back(LogicalType::DOUBLE);
                } else {
                    return_types.push_back(LogicalType::VARCHAR);
                }
            }
        }
    }

    return bind_data;
}

} // namespace duckdb

