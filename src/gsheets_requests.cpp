#include "gsheets_requests.hpp"
#include "duckdb/common/exception.hpp"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
namespace duckdb
{

    std::string perform_https_request(const std::string &host, const std::string &path, const std::string &token,
                                      HttpMethod method, const std::string &body, const std::string &content_type)
    {
        std::string response;
        SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx)
        {
            throw duckdb::IOException("Failed to create SSL context");
        }

        BIO *bio = BIO_new_ssl_connect(ctx);
        SSL *ssl;
        BIO_get_ssl(bio, &ssl);
        SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

        BIO_set_conn_hostname(bio, (host + ":443").c_str());

        if (BIO_do_connect(bio) <= 0)
        {
            BIO_free_all(bio);
            SSL_CTX_free(ctx);
            throw duckdb::IOException("Failed to connect");
        }

        std::string method_str;
        switch (method)
        {
        case HttpMethod::GET:
            method_str = "GET";
            break;
        case HttpMethod::POST:
            method_str = "POST";
            break;
        case HttpMethod::PUT:
            method_str = "PUT";
            break;
        }

        std::string request = method_str + " " + path + " HTTP/1.0\r\n";
        request += "Host: " + host + "\r\n";
        request += "Authorization: Bearer " + token + "\r\n";
        request += "Connection: close\r\n";

        if (!body.empty())
        {
            request += "Content-Type: " + content_type + "\r\n";
            request += "Content-Length: " + std::to_string(body.length()) + "\r\n";
        }

        request += "\r\n";

        if (!body.empty())
        {
            request += body;
        }

        if (BIO_write(bio, request.c_str(), request.length()) <= 0)
        {
            BIO_free_all(bio);
            SSL_CTX_free(ctx);
            throw duckdb::IOException("Failed to write request");
        }

        char buffer[1024];
        int len;
        while ((len = BIO_read(bio, buffer, sizeof(buffer))) > 0)
        {
            response.append(buffer, len);
        }

        BIO_free_all(bio);
        SSL_CTX_free(ctx);

        // Extract body from response
        size_t body_start = response.find("\r\n\r\n");
        if (body_start != std::string::npos)
        {
            return response.substr(body_start + 4);
        }

        return response;
    }

    std::string fetch_sheet_data(const std::string &spreadsheet_id, const std::string &token, const std::string &sheet_name, HttpMethod method, const std::string &body)
    {
        std::string host = "sheets.googleapis.com";
        std::string path = "/v4/spreadsheets/" + spreadsheet_id + "/values/" + sheet_name;

        if (method == HttpMethod::POST) {
            path += ":append";
            path += "?valueInputOption=RAW";
        }

        return perform_https_request(host, path, token, method, body);
    }

    std::string delete_sheet_data(const std::string &spreadsheet_id, const std::string &token, const std::string &sheet_name)
    {
        std::string host = "sheets.googleapis.com";
        std::string path = "/v4/spreadsheets/" + spreadsheet_id + "/values/" + sheet_name + ":clear";

        return perform_https_request(host, path, token, HttpMethod::POST, "{}");
    }

    std::string get_spreadsheet_metadata(const std::string &spreadsheet_id, const std::string &token)
    {
        std::string host = "sheets.googleapis.com";
        std::string path = "/v4/spreadsheets/" + spreadsheet_id + "?&fields=sheets.properties";
        return perform_https_request(host, path, token, HttpMethod::GET, "");
    }
}
