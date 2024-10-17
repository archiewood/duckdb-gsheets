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

    struct GSheetCopyGlobalState : public GlobalFunctionData
    {
        explicit GSheetCopyGlobalState(ClientContext &context, const string &sheet_id, const string &token, const string &sheet_name)
            : sheet_id(sheet_id), token(token), sheet_name(sheet_name)
        {
        }

    public:
        string sheet_id;
        string token;
        string sheet_name;
    };

    struct GSheetWriteBindData : public TableFunctionData
    {
    };

    unique_ptr<FunctionData> GSheetCopyFunction::GSheetWriteBind(ClientContext &context, CopyFunctionBindInput &input, const vector<string> &names, const vector<LogicalType> &sql_types)
    {
        return make_uniq<GSheetWriteBindData>();
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
        std::string sheet_id = extract_sheet_id(file_path);
        std::string sheet_name = "Sheet1"; // TODO: make this configurable

        return make_uniq<GSheetCopyGlobalState>(context, sheet_id, token, sheet_name);
    }

    unique_ptr<LocalFunctionData> GSheetCopyFunction::GSheetWriteInitializeLocal(ExecutionContext &context, FunctionData &bind_data_p)
    {
        return make_uniq<LocalFunctionData>();
    }

    void GSheetCopyFunction::GSheetWriteSink(ExecutionContext &context, FunctionData &bind_data_p, GlobalFunctionData &gstate_p, LocalFunctionData &lstate, DataChunk &input)
    {
        input.Flatten();
        auto &gstate = gstate_p.Cast<GSheetCopyGlobalState>();

        // Create object ready to write to Google Sheet
        json sheet_data;

        // TODO: make this configurable
        sheet_data["range"] = "Sheet1";
        sheet_data["majorDimension"] = "ROWS";
        
        vector<vector<string>> values;
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
                    case LogicalTypeId::BIGINT:
                        row.push_back(to_string(FlatVector::GetData<int64_t>(col)[r]));
                        break;
                    case LogicalTypeId::DOUBLE:
                        row.push_back(to_string(FlatVector::GetData<double>(col)[r]));
                        break;
                    case LogicalTypeId::BOOLEAN:
                        row.push_back(FlatVector::GetData<bool>(col)[r] ? "TRUE" : "FALSE");
                        break;
                    default:
                        row.push_back("Type not implemented");
                        break;
                }
            }
            values.push_back(row);
        }
        sheet_data["values"] = values;

        // Convert the JSON object to a string
        std::string request_body = sheet_data.dump();

        // Make the API call to write data to the Google Sheet
        std::string response = fetch_sheet_data(gstate.sheet_id, gstate.token, gstate.sheet_name, HttpMethod::PUT, request_body);

        // Check for errors in the response
        json response_json = parseJson(response);
        if (response_json.contains("error")) {
            throw duckdb::IOException("Error writing to Google Sheet: " + response_json["error"]["message"].get<std::string>());
        }
    }
} // namespace duckdb
