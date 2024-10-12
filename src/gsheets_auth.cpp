#include "gsheets_auth.hpp"
#include "duckdb/common/exception.hpp"
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

}  // namespace duckdb