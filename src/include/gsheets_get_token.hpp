#pragma once

#include <string>
#include <stdlib.h>

namespace duckdb {

    char get_base64_char(char byte);

    void base64encode(char *output, const char *input, size_t input_length) ;

    std::string get_token(const std::string& filename) ;

}