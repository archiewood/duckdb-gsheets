#define DUCKDB_EXTENSION_MAIN

#include "gsheets_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

#include <sstream>
#include "duckdb/function/table_function.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/common/types/vector.hpp"
#include <string>
#include <unordered_map>
#include <algorithm>
#include <iostream>
using namespace std;

// OpenSSL linked through vcpkg
#include <openssl/opensslv.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>

namespace duckdb {


static std::string perform_https_request(const std::string& host, const std::string& path, const std::string& token) {
    std::string response;
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        throw duckdb::IOException("Failed to create SSL context");
    }

    BIO *bio = BIO_new_ssl_connect(ctx);
    SSL *ssl;
    BIO_get_ssl(bio, &ssl);
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

    BIO_set_conn_hostname(bio, (host + ":443").c_str());

    if (BIO_do_connect(bio) <= 0) {
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        throw duckdb::IOException("Failed to connect");
    }

    std::string request = "GET " + path + " HTTP/1.1\r\n";
    request += "Host: " + host + "\r\n";
    request += "Authorization: Bearer " + token + "\r\n";
    request += "Connection: close\r\n\r\n";

    if (BIO_write(bio, request.c_str(), request.length()) <= 0) {
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        throw duckdb::IOException("Failed to write request");
    }

    char buffer[1024];
    int len;
    while ((len = BIO_read(bio, buffer, sizeof(buffer))) > 0) {
        response.append(buffer, len);
    }

    BIO_free_all(bio);
    SSL_CTX_free(ctx);

    // Extract body from response
    size_t body_start = response.find("\r\n\r\n");
    if (body_start != std::string::npos) {
        return response.substr(body_start + 4);
    }

    return response;
}

static std::string fetch_sheet_data(const std::string& sheet_id, const std::string& token) {
    std::string host = "sheets.googleapis.com";
    std::string path = "/v4/spreadsheets/" + sheet_id + "/values/Sheet1";  // Assumes "Sheet1", adjust as needed
    
    return perform_https_request(host, path, token);
}

struct ReadSheetBindData : public TableFunctionData {
    string sheet_id;
    string token;
    bool finished;
    idx_t row_index;
    string response;
    bool header;  // Add this line

    ReadSheetBindData(string sheet_id, string token, bool header) 
        : sheet_id(sheet_id), token(token), finished(false), row_index(0), header(header) {
        response = fetch_sheet_data(sheet_id, token);
    }
};

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

struct SheetData {
    std::string range;
    std::string majorDimension;
    std::vector<std::vector<std::string>> values;
};

SheetData parseJson(const std::string& json) {
    SheetData result;
    std::istringstream iss(json);
    std::string line;

    auto trim = [](std::string& s) {
        // Remove trailing whitespace and any commas
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
            return !std::isspace(ch) && ch != ',';
        }).base(), s.end());
        // Remove leading whitespace and quotes
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch) && ch != '"';
        }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
            return !std::isspace(ch) && ch != '"';
        }).base(), s.end());
    };

    while (std::getline(iss, line)) {
        trim(line);
        // Skip the {}
        if (line == "{" || line == "}") {
            continue;
        }
        if (line.find("range") != std::string::npos) {
            continue;
        } else if (line.find("majorDimension") != std::string::npos) {
            continue;
        } else if (line == "[") {
            std::vector<std::string> row;
            while (std::getline(iss, line) && line != "]") {
                trim(line);
                if (line == "[") {
                    row.clear();
                } else if (line == "]") {
                    if (!row.empty()) {
                        result.values.push_back(row);
                    }
                } else {
                    row.push_back(line);
                }
            }
        }
        
    }
    
    return result;
}

static void ReadSheetFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
    auto &bind_data = const_cast<ReadSheetBindData&>(data_p.bind_data->Cast<ReadSheetBindData>());

    if (bind_data.finished) {
        return;
    }

    SheetData sheet_data = parseJson(bind_data.response);

    idx_t row_count = 0;
    idx_t column_count = output.ColumnCount();

    // Adjust starting index based on whether we're using the header
    idx_t start_index = (bind_data.header && bind_data.row_index == 0) ? 1 : bind_data.row_index;

    for (idx_t i = start_index; i < sheet_data.values.size() && row_count < STANDARD_VECTOR_SIZE; i++) {
        const auto& row = sheet_data.values[i];
        for (idx_t col = 0; col < column_count; col++) {
            if (col < row.size()) {
                output.SetValue(col, row_count, Value(row[col]));
            } else {
                output.SetValue(col, row_count, Value(nullptr));
            }
        }
        row_count++;
    }

    bind_data.row_index += row_count;
    bind_data.finished = (bind_data.row_index >= sheet_data.values.size());

    output.SetCardinality(row_count);
}

// Update the ReadSheetBind function
static unique_ptr<FunctionData> ReadSheetBind(ClientContext &context, TableFunctionBindInput &input,
                                              vector<LogicalType> &return_types, vector<string> &names) {
    auto sheet_id = input.inputs[0].GetValue<string>();
    auto token = input.inputs[1].GetValue<string>();
    bool header = input.inputs.size() > 2 ? input.inputs[2].GetValue<bool>() : true;  // Default to true if not provided

    auto bind_data = make_uniq<ReadSheetBindData>(sheet_id, token, header);

    SheetData sheet_data = parseJson(bind_data->response);

    if (!sheet_data.values.empty()) {
        if (header) {
            for (const auto& column_name : sheet_data.values[0]) {
                names.push_back(column_name);
                return_types.push_back(LogicalType::VARCHAR);
            }
        } else {
            // If not using header, generate column names
            for (size_t i = 0; i < sheet_data.values[0].size(); i++) {
                names.push_back("column" + std::to_string(i + 1));
                return_types.push_back(LogicalType::VARCHAR);
            }
        }
    }

    return bind_data;
}

static void LoadInternal(DatabaseInstance &instance) {
    // Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();


    // Register read_sheet table function
    TableFunction read_sheet_function("read_sheet", {LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::BOOLEAN}, ReadSheetFunction, ReadSheetBind);
    read_sheet_function.named_parameters["header"] = LogicalType::BOOLEAN;  // Add this line to make it a named parameter
    ExtensionUtil::RegisterFunction(instance, read_sheet_function);
}

void GsheetsExtension::Load(DuckDB &db) {
	LoadInternal(*db.instance);
}
std::string GsheetsExtension::Name() {
	return "gsheets";
}

std::string GsheetsExtension::Version() const {
#ifdef EXT_VERSION_GSHEETS
	return EXT_VERSION_GSHEETS;
#else
	return "";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_EXTENSION_API void gsheets_init(duckdb::DatabaseInstance &db) {
    duckdb::DuckDB db_wrapper(db);
    db_wrapper.LoadExtension<duckdb::GsheetsExtension>();
}

DUCKDB_EXTENSION_API const char *gsheets_version() {
	return duckdb::DuckDB::LibraryVersion();
}
}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif
