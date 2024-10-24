#include "gsheets_auth.hpp"
#include "gsheets_requests.hpp"
#include "gsheets_utils.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/secret/secret.hpp"
#include "duckdb/main/extension_util.hpp"
#include <fstream>

namespace duckdb
{

    std::string read_token_from_file(const std::string &file_path)
    {
        std::ifstream file(file_path);
        if (!file.is_open())
        {
            throw duckdb::IOException("Unable to open token file: " + file_path);
        }
        std::string token;
        std::getline(file, token);
        return token;
    }

    // This code is copied, with minor modifications from https://github.com/duckdb/duckdb_azure/blob/main/src/azure_secret.cpp
    static void CopySecret(const std::string &key, const CreateSecretInput &input, KeyValueSecret &result)
    {
        auto val = input.options.find(key);

        if (val != input.options.end())
        {
            result.secret_map[key] = val->second;
        }
    }

    static void RegisterCommonSecretParameters(CreateSecretFunction &function)
    {
        // Register google sheets common parameters
        function.named_parameters["token"] = LogicalType::VARCHAR;
    }

    static void RedactCommonKeys(KeyValueSecret &result)
    {
        result.redact_keys.insert("proxy_password");
    }

    // TODO: Maybe this should be a KeyValueSecret
    static unique_ptr<BaseSecret> CreateGsheetSecretFromAccessToken(ClientContext &context, CreateSecretInput &input)
    {
        auto scope = input.scope;

        auto result = make_uniq<KeyValueSecret>(scope, input.type, input.provider, input.name);

        std::string token;
        if (input.options.find("use_oauth") != input.options.end() && input.options["use_oauth"] == "true")
        {
            // Initiate OAuth flow
            token = InitiateOAuthFlow();
        }
        else
        {
            // Use the provided token
            CopySecret("token", input, *result);
        }

        result->secret_map["token"] = token;

        // Redact sensible keys
        RedactCommonKeys(*result);
        result->redact_keys.insert("token");

        return std::move(result);
    }

    static unique_ptr<BaseSecret> CreateGsheetSecretFromOAuth(ClientContext &context, CreateSecretInput &input)
    {
        auto scope = input.scope;

        auto result = make_uniq<KeyValueSecret>(scope, input.type, input.provider, input.name);

        // Initiate OAuth flow
        string token = InitiateOAuthFlow();

        result->secret_map["token"] = token;

        // Redact sensible keys
        RedactCommonKeys(*result);
        result->redact_keys.insert("token");

        return std::move(result);
    }

    void CreateGsheetSecretFunctions::Register(DatabaseInstance &instance)
    {
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

        // Register the oauth secret provider
        CreateSecretFunction oauth_function = {type, "oauth", CreateGsheetSecretFromOAuth};
        oauth_function.named_parameters["use_oauth"] = LogicalType::BOOLEAN;
        RegisterCommonSecretParameters(oauth_function);
        ExtensionUtil::RegisterFunction(instance, oauth_function);
    }

    std::string InitiateOAuthFlow()
    {
        const std::string client_id = "793766532675-2uvl6j36o8n5e80ubsqhuhrv33m2au0r.apps.googleusercontent.com";
        // This is actually not a secret in the context of a desktop app, and not required by the OAuth2 PKCE spec, but Google requires it. 
        // See https://stackoverflow.com/questions/76528208/google-oauth-2-0-authorization-code-with-pkce-requires-a-client-secret
        const std::string client_secret = "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";
        const std::string redirect_uri = "urn:ietf:wg:oauth:2.0:oob";
        const std::string auth_url = "https://accounts.google.com/o/oauth2/v2/auth";
        const std::string token_url = "https://oauth2.googleapis.com/token";

        // Generate a random state for CSRF protection
        std::string state = generate_random_string(10);

        // Generate a code verifier and code challenge for PKCE
        std::string code_verifier = generate_random_string(43);
        std::string code_challenge = code_verifier;

        std::string auth_request_url = auth_url + "?client_id=" + client_id +
                                       "&redirect_uri=" + redirect_uri +
                                       "&response_type=code" +
                                       "&scope=https://www.googleapis.com/auth/spreadsheets" +
                                       "&state=" + state +
                                       "&code_challenge=" + code_challenge +
                                       "&code_challenge_method=plain";

        // Instruct the user to visit the URL and grant permission
        std::cout << "Visit the below URL to authorize DuckDB GSheets" << std::endl << std::endl;
        std::cout << auth_request_url << std::endl << std::endl;
        std::cout << "After granting permission, you will be redirected to a page with an authorization code." << std::endl << std::endl;
        std::cout << "Please enter the authorization code: ";

        std::string auth_code;
        std::cin >> auth_code;

        // Exchange the authorization code for an access token
        std::string request_body = "client_id=" + client_id +
                                   "&client_secret=" + client_secret +
                                   "&code=" + auth_code +
                                   "&redirect_uri=" + redirect_uri +
                                   "&grant_type=authorization_code" +
                                   "&code_verifier=" + code_verifier;

        std::string response = perform_https_request("oauth2.googleapis.com", 
                                                    "/token", 
                                                    "", 
                                                    HttpMethod::POST, 
                                                    request_body, 
                                                    "application/x-www-form-urlencoded");

        json response_json = parseJson(response);

        std::cout << response_json["access_token"] << std::endl;

        return response_json["access_token"];
    }

} // namespace duckdb
