#include "gsheets_copy.hpp"
#include "gsheets_requests.hpp"
#include "gsheets_auth.hpp"
#include "gsheets_utils.hpp"

#include "duckdb/common/serializer/buffered_file_writer.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/main/secret/secret_manager.hpp"
#include <json.hpp>
#include <regex>
#include "gsheets_get_token.hpp"
#include <iostream>

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

        std::string token;

        if (secret.GetProvider() == "private_key") {
            // If using a private key, retrieve the private key from the secret, but convert it 
            // into a token before use. This is an extra request per Google Sheet read or copy.
            Value email_value;
            if (!kv_secret->TryGetValue("email", email_value)) {
                throw InvalidInputException("'email' not found in 'gsheet' secret");
            }
            std::string email_string = email_value.ToString();

            Value secret_value;
            if (!kv_secret->TryGetValue("secret", secret_value)) {
                throw InvalidInputException("'secret' not found in 'gsheet' secret");
            }
            std::string secret_string = std::regex_replace(secret_value.ToString(), std::regex(R"(\\n)"), "\n");
            
            json token_json = parseJson(get_token(email_string, secret_string));
            
            json failure_string = "failure";
            if (token_json["status"] == failure_string) {
                throw InvalidInputException("Conversion from private key to token failed. Check key format (-----BEGIN PRIVATE KEY-----\\n ... -----END PRIVATE KEY-----\\n) and expiration date.");
            }

            token = token_json["access_token"].get<std::string>();
        } else {
            Value token_value;
            if (!kv_secret->TryGetValue("token", token_value)) {
                throw InvalidInputException("'token' not found in 'gsheet' secret");
            }

            token = token_value.ToString();
        }
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
                Value val = col.GetValue(r);
                if (val.IsNull()) {
                    row.push_back("");
                } else {
                    row.push_back(val.ToString());
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
