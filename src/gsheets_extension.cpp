#define DUCKDB_EXTENSION_MAIN

// TODO
// - Remove standard library functions and types and replace with DuckDB types and functions
// - Remove JSON library and use DuckDB JSON functions instead?
// - Remove OpenSSL and use DuckDB HTTPFS functions instead?
// - Fix larger read_gsheet response parsing which currently contains unescaped range characters
// - Handle types better throughout
// - OAuth flow for token management
// - Docs: how to get a token
// - Tests: Copy



#include "duckdb.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/function/table_function.hpp"


// Standard library
#include <string>


// GSheets extension
#include "gsheets_extension.hpp"
#include "gsheets_auth.hpp"
#include "gsheets_copy.hpp"
#include "gsheets_read.hpp"

// OpenSSL linked through vcpkg
#include <openssl/opensslv.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>



namespace duckdb {

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>



static void LoadInternal(DatabaseInstance &instance) {
    // Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    // Register read_gsheet table function
    TableFunction read_gsheet_function("read_gsheet", {LogicalType::VARCHAR}, ReadSheetFunction, ReadSheetBind);
    read_gsheet_function.named_parameters["header"] = LogicalType::BOOLEAN;
    read_gsheet_function.named_parameters["sheet"] = LogicalType::VARCHAR;
    ExtensionUtil::RegisterFunction(instance, read_gsheet_function);

    // Register COPY TO (FORMAT 'gsheet') function
    GSheetCopyFunction gsheet_copy_function;
    ExtensionUtil::RegisterFunction(instance, gsheet_copy_function);

    // Register Secret functions
	CreateGsheetSecretFunctions::Register(instance);
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
