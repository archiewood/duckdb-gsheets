#define DUCKDB_EXTENSION_MAIN

#include "gsheets_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

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

static void LoadInternal(DatabaseInstance &instance) {
    // Register a scalar function
    auto gsheets_scalar_function = ScalarFunction("gsheets", {LogicalType::VARCHAR}, LogicalType::VARCHAR, GsheetsScalarFun);
    ExtensionUtil::RegisterFunction(instance, gsheets_scalar_function);

    // Register another scalar function
    auto gsheets_openssl_version_scalar_function = ScalarFunction("gsheets_openssl_version", {LogicalType::VARCHAR},
                                                LogicalType::VARCHAR, GsheetsOpenSSLVersionScalarFun);
    ExtensionUtil::RegisterFunction(instance, gsheets_openssl_version_scalar_function);
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
