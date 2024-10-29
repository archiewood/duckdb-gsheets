#include "gsheets_copy.hpp"
#include "gsheets_requests.hpp"
#include "gsheets_auth.hpp"
#include "gsheets_utils.hpp"

#include "duckdb/common/serializer/buffered_file_writer.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/main/secret/secret_manager.hpp"
#include <json.hpp>

using json = nlohmann::json;

namespace duckdb
{

    GSheetCopyFunction::GSheetCopyFunction() : CopyFunction("gsheet")
    {
        copy_to_bind = GSheetWriteBind;
        copy_to_initialize_global = GSheetWriteInitializeGlobal;
        copy_to_initialize_local = GSheetWriteInitializeLocal;
        copy_to_sink = GSheetWriteSink;
    }


    unique_ptr<FunctionData> GSheetCopyFunction::GSheetWriteBind(ClientContext &context, CopyFunctionBindInput &input, const vector<string> &names, const vector<LogicalType> &sql_types)
    {
        string file_path = input.info.file_path;

        return make_uniq<GSheetWriteBindData>(file_path, sql_types, names);
    }

    unique_ptr<GlobalFunctionData> GSheetCopyFunction::GSheetWriteInitializeGlobal(ClientContext &context, FunctionData &bind_data, const string &file_path)
    {
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
        std::string spreadsheet_id = extract_spreadsheet_id(file_path);
        std::string sheet_id = extract_sheet_id(file_path);
        std::string sheet_name = "Sheet1";

        sheet_name = get_sheet_name_from_id(spreadsheet_id, sheet_id, token);

        std::string encoded_sheet_name = url_encode(sheet_name);

        // If writing, clear out the entire sheet first.
        // Do this here in the initialization so that it only happens once
        std::string response = delete_sheet_data(spreadsheet_id, token, encoded_sheet_name);

        return make_uniq<GSheetCopyGlobalState>(context, spreadsheet_id, token, encoded_sheet_name);
    }

    unique_ptr<LocalFunctionData> GSheetCopyFunction::GSheetWriteInitializeLocal(ExecutionContext &context, FunctionData &bind_data_p)
    {
        return make_uniq<LocalFunctionData>();
    }

    void GSheetCopyFunction::GSheetWriteSink(ExecutionContext &context, FunctionData &bind_data_p, GlobalFunctionData &gstate_p, LocalFunctionData &lstate, DataChunk &input)
    {
        input.Flatten();
        auto &gstate = gstate_p.Cast<GSheetCopyGlobalState>();

        std::string sheet_id = extract_sheet_id(bind_data_p.Cast<GSheetWriteBindData>().files[0]);

        std::string sheet_name = "Sheet1";

        sheet_name = get_sheet_name_from_id(gstate.spreadsheet_id, sheet_id, gstate.token);
        std::string encoded_sheet_name = url_encode(sheet_name);
        // Create object ready to write to Google Sheet
        json sheet_data;

        sheet_data["range"] = sheet_name;
        sheet_data["majorDimension"] = "ROWS";
        
        vector<string> headers = bind_data_p.Cast<GSheetWriteBindData>().options.name_list;        

        vector<vector<string>> values;
        values.push_back(headers);

        for (idx_t r = 0; r < input.size(); r++)
        {
            vector<string> row;
            for (idx_t c = 0; c < input.ColumnCount(); c++)
            {
                auto &col = input.data[c];
                switch (col.GetType().id()) {
                    case LogicalTypeId::VARCHAR:
                        row.push_back(FlatVector::GetData<string_t>(col)[r].GetString());
                        break;
                    case LogicalTypeId::INTEGER:
                        row.push_back(to_string(FlatVector::GetData<int32_t>(col)[r]));
                        break;
                    case LogicalTypeId::TINYINT:
                        row.push_back(to_string(FlatVector::GetData<int8_t>(col)[r]));
                        break;
                    case LogicalTypeId::SMALLINT:
                        row.push_back(to_string(FlatVector::GetData<int16_t>(col)[r]));
                        break;
                    case LogicalTypeId::BIGINT:
                        row.push_back(to_string(FlatVector::GetData<int64_t>(col)[r]));
                        break;
                    case LogicalTypeId::UTINYINT:
                        row.push_back(to_string(FlatVector::GetData<uint8_t>(col)[r]));
                        break;
                    case LogicalTypeId::USMALLINT:
                        row.push_back(to_string(FlatVector::GetData<uint16_t>(col)[r]));
                        break;
                    case LogicalTypeId::UINTEGER:
                        row.push_back(to_string(FlatVector::GetData<uint32_t>(col)[r]));
                        break;
                    case LogicalTypeId::UBIGINT:
                        row.push_back(to_string(FlatVector::GetData<uint64_t>(col)[r]));
                        break;
                    case LogicalTypeId::DOUBLE:
                        row.push_back(to_string(FlatVector::GetData<double>(col)[r]));
                        break;
                    case LogicalTypeId::BOOLEAN:
                        row.push_back(FlatVector::GetData<bool>(col)[r] ? "TRUE" : "FALSE");
                        break;
                    default:
                        row.push_back("Type " + col.GetType().ToString() + " not implemented");
                        break;
                }
            }
            values.push_back(row);
        }
        sheet_data["values"] = values;

        // Convert the JSON object to a string
        std::string request_body = sheet_data.dump();

        // Make the API call to write data to the Google Sheet
        // Today, this is only append.
        std::string response = call_sheets_api(gstate.spreadsheet_id, gstate.token, encoded_sheet_name, HttpMethod::POST, request_body);

        // Check for errors in the response
        json response_json = parseJson(response);
        if (response_json.contains("error")) {
            throw duckdb::IOException("Error writing to Google Sheet: " + response_json["error"]["message"].get<std::string>());
        }
    }
} // namespace duckdb
