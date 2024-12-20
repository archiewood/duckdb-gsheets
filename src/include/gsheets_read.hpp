#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/main/client_context.hpp"

namespace duckdb {

struct ReadSheetBindData : public TableFunctionData {
    string spreadsheet_id;
    string token;
    bool finished;
    idx_t row_index;
    string response;
    bool header;
    string sheet_name;
    vector<LogicalType> return_types;
    vector<string> names;

    ReadSheetBindData(string spreadsheet_id, string token, bool header, string sheet_name);
};

void ReadSheetFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output);

unique_ptr<FunctionData> ReadSheetBind(ClientContext &context, TableFunctionBindInput &input,
                                       vector<LogicalType> &return_types, vector<string> &names);

} // namespace duckdb