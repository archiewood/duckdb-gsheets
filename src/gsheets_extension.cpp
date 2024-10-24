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
#include "duckdb/main/config.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/tableref/table_function_ref.hpp"


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


unique_ptr<TableRef> ReadSheetReplacement(ClientContext &context, ReplacementScanInput &input,
                                            optional_ptr<ReplacementScanData> data) {
	auto table_name = ReplacementScan::GetFullPath(input);
	if (!StringUtil::StartsWith(table_name, "https://docs.google.com/spreadsheets/d/")) {
		return nullptr;
	}
	auto table_function = make_uniq<TableFunctionRef>();
	vector<unique_ptr<ParsedExpression>> children;
	children.push_back(make_uniq<ConstantExpression>(Value(table_name)));
	table_function->function = make_uniq<FunctionExpression>("read_gsheet", std::move(children));

	if (!FileSystem::HasGlob(table_name)) {
		auto &fs = FileSystem::GetFileSystem(context);
		table_function->alias = fs.ExtractBaseName(table_name);
	}

	return std::move(table_function);
}



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

    // Register replacement scan for read_gsheet
    auto &config = DBConfig::GetConfig(instance);
    config.replacement_scans.emplace_back(ReadSheetReplacement);
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
