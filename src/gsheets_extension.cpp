#define DUCKDB_EXTENSION_MAIN

#include "gsheets_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <sstream>

// OpenSSL linked through vcpkg
#include <openssl/opensslv.h>

namespace duckdb {

inline void GsheetsScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &name_vector = args.data[0];
    UnaryExecutor::Execute<string_t, string_t>(
	    name_vector, result, args.size(),
	    [&](string_t name) {
			return StringVector::AddString(result, "Gsheets "+name.GetString()+" 🐥");;
        });
}

inline void GsheetsOpenSSLVersionScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &name_vector = args.data[0];
    UnaryExecutor::Execute<string_t, string_t>(
	    name_vector, result, args.size(),
	    [&](string_t name) {
			return StringVector::AddString(result, "Gsheets " + name.GetString() +
                                                     ", my linked OpenSSL version is " +
                                                     OPENSSL_VERSION_TEXT );;
        });
}

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

static void ReadSheetFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &sheet_id_vector = args.data[0];
    auto &token_vector = args.data[1];

    UnaryExecutor::Execute<string_t, string_t>(
        sheet_id_vector, result, args.size(),
        [&](string_t sheet_id) {
            auto token = token_vector.GetValue(0).ToString();
            std::string sheet_data = fetch_sheet_data(sheet_id.GetString(), token);
            return StringVector::AddString(result, sheet_data);
        });
}

static void LoadInternal(DatabaseInstance &instance) {
    // Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    // Register a scalar function
    auto gsheets_scalar_function = ScalarFunction("gsheets", {LogicalType::VARCHAR}, LogicalType::VARCHAR, GsheetsScalarFun);
    ExtensionUtil::RegisterFunction(instance, gsheets_scalar_function);

    // Register another scalar function
    auto gsheets_openssl_version_scalar_function = ScalarFunction("gsheets_openssl_version", {LogicalType::VARCHAR},
                                                LogicalType::VARCHAR, GsheetsOpenSSLVersionScalarFun);
    ExtensionUtil::RegisterFunction(instance, gsheets_openssl_version_scalar_function);

    // Register read_sheet function
    auto read_sheet_function = ScalarFunction("read_sheet", {LogicalType::VARCHAR, LogicalType::VARCHAR}, LogicalType::VARCHAR, ReadSheetFunction);
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