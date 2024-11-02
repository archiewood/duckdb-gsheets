// Taken with modifications from https://gist.github.com/niuk/6365b819a86a7e0b92d82328fcf87da5
#include <stdio.h>  
#include <string.h>
#include <stdlib.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.hpp>

#include <json.hpp>
using json = nlohmann::json;
namespace duckdb
{

    char get_base64_char(char byte) {
        if (byte < 26) {
            return 'A' + byte;
        } else if (byte < 52) {
            return 'a' + byte - 26;
        } else if (byte < 62) {
            return '0' + byte - 52;
        } else if (byte == 62) {
            return '-';
        } else if (byte == 63) {
            return '_';
        } else {
            fprintf(stderr, "BAD BYTE: %02x\n", byte);
            exit(1);
            return 0;
        }
    }

    // To execute C, please define "int main()"
    void base64encode(char *output, const char *input, size_t input_length) {
        size_t input_index = 0;
        size_t output_index = 0;
        for (; input_index < input_length; ++output_index) {
            switch (output_index % 4) {
            case 0:
                output[output_index] = get_base64_char((0xfc & input[input_index]) >> 2);
                break;
            case 1:
                output[output_index] = get_base64_char(((0x03 & input[input_index]) << 4) | ((0xf0 & input[input_index + 1]) >> 4));
                ++input_index;
                break;
            case 2:
                output[output_index] = get_base64_char(((0x0f & input[input_index]) << 2) | ((0xc0 & input[input_index + 1]) >> 6));
                ++input_index;
                break;
            case 3:
                output[output_index] = get_base64_char(0x3f & input[input_index]);
                ++input_index;
                break;
            default:
                exit(1);
            }
        }

        output[output_index] = '\0';
    }

    std::string get_token(const std::string& email, const std::string& private_key) {
        const char *header = "{\"alg\":\"RS256\",\"typ\":\"JWT\"}";

        const int iat = time(NULL);
        const int exp = iat + 60 * 60 - 1;

        char claim_set[1024];

        /* Create jwt claim set */
        json jwt_claim_set;
        std::time_t t = std::time(NULL);
        jwt_claim_set["iss"] = email; /* service account email address */
        jwt_claim_set["scope"] = "https://www.googleapis.com/auth/spreadsheets" /* scope of requested access token */;
        jwt_claim_set["aud"] = "https://accounts.google.com/o/oauth2/token"; /* intended target of the assertion for an access token */
        jwt_claim_set["iat"] = std::to_string(t); /* issued time */
        // Since we get a new token on every request (and max request time is 3 minutes), 
        // set the limit to 10 minutes. (Max that Google allows is 1 hour)
        jwt_claim_set["exp"] = std::to_string(t+600); /* expire time*/

        std::copy(jwt_claim_set.dump().cbegin(), jwt_claim_set.dump().cend(), claim_set);

        char header_64[1024];
        base64encode(header_64, header, strlen(header));

        char claim_set_64[1024];
        base64encode(claim_set_64, claim_set, strlen(claim_set));

        char input[1024];
        int input_length = sprintf(input, "%s.%s", header_64, claim_set_64);

        unsigned char *digest = SHA256((const unsigned char *)input, input_length, NULL);
        char digest_str[1024];
        for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
            sprintf(digest_str + i * 2, "%02x", digest[i]);
        }
        
        digest_str[SHA256_DIGEST_LENGTH * 2] = '\0';

        // This works, but requires a file. The in-memory version is below.
        // FILE *key_file = fopen("credentials.key.pem", "r");
        // RSA *rsa = PEM_read_RSAPrivateKey(key_file, NULL, NULL, NULL);

        BIO *bufio;
        RSA *rsa;

        bufio = BIO_new_mem_buf(private_key.c_str(), -1); 
        PEM_read_bio_RSAPrivateKey(bufio, &rsa, 0, NULL);

        if (rsa != NULL) {
            unsigned char sigret[4096];
            unsigned int siglen;
            if (RSA_sign(NID_sha256, digest, SHA256_DIGEST_LENGTH, sigret, &siglen, rsa)) {
                if (RSA_verify(NID_sha256, digest, SHA256_DIGEST_LENGTH, sigret, siglen, rsa)) {
                    char signature_64[1024];
                    base64encode(signature_64, (const char *)sigret, siglen);
                    
                    char jwt[1024];
                    sprintf(jwt, "%s.%s", input, signature_64);

                    duckdb_httplib_openssl::Client cli("https://oauth2.googleapis.com");
                    duckdb_httplib_openssl::Params params;
                    params.emplace("grant_type", "urn:ietf:params:oauth:grant-type:jwt-bearer");
                    params.emplace("assertion", jwt);
                    auto result = cli.Post("/token", params);
                    return (result -> body);
                } else {
                    printf("Could not verify RSA signature.");
                }
            } else {
                unsigned long err = ERR_get_error();
                printf("RSA_sign failed: %lu, %s\n", err, ERR_error_string(err, NULL));
            }

            RSA_free(rsa);
        }
        std::string failure_message = "{\"status\":\"failed\"}";
        return failure_message;
    }
}
