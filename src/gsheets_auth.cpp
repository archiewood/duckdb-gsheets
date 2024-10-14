#include "gsheets_auth.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/secret/secret.hpp"
#include "duckdb/main/extension_util.hpp"
#include <fstream>

namespace duckdb {

std::string read_token_from_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw duckdb::IOException("Unable to open token file: " + file_path);
    }
    std::string token;
    std::getline(file, token);
    return token;
}

// This code is copied, with minor modifications from https://github.com/duckdb/duckdb_azure/blob/main/src/azure_secret.cpp
static void CopySecret(const std::string &key, const CreateSecretInput &input, KeyValueSecret &result) {
	auto val = input.options.find(key);

	if (val != input.options.end()) {
		result.secret_map[key] = val->second;
	}
}

static void RegisterCommonSecretParameters(CreateSecretFunction &function) {
	// Register google sheets common parameters
	function.named_parameters["token"] = LogicalType::VARCHAR;
}

static void RedactCommonKeys(KeyValueSecret &result) {
	result.redact_keys.insert("proxy_password");
}


// TODO: Maybe this should be a KeyValueSecret
static unique_ptr<BaseSecret> CreateGsheetSecretFromAccessToken(ClientContext &context, CreateSecretInput &input) {
	auto scope = input.scope;

	auto result = make_uniq<KeyValueSecret>(scope, input.type, input.provider, input.name);

	// Manage specific secret option
	CopySecret("token", input, *result);

	// Redact sensible keys
	RedactCommonKeys(*result);
	result->redact_keys.insert("token");

	return std::move(result);
}

void CreateGsheetSecretFunctions::Register(DatabaseInstance &instance) {
	string type = "gsheet";

	// Register the new type
	SecretType secret_type;
	secret_type.name = type;
	secret_type.deserializer = KeyValueSecret::Deserialize<KeyValueSecret>;
	secret_type.default_provider = "access_token";
	ExtensionUtil::RegisterSecretType(instance, secret_type);

    // Register the access_token secret provider
	CreateSecretFunction access_token_function = {type, "access_token", CreateGsheetSecretFromAccessToken};
	access_token_function.named_parameters["access_token"] = LogicalType::VARCHAR;
	RegisterCommonSecretParameters(access_token_function);
	ExtensionUtil::RegisterFunction(instance, access_token_function);

}


}  // namespace duckdb