// Taken with minor modifications from https://github.com/duckdb/postgres_scanner/blob/main/src/include/postgres_binary_copy.hpp

#pragma once

#include "duckdb/function/copy_function.hpp"

namespace duckdb
{

    class GSheetCopyFunction : public CopyFunction
    {
    public:
        GSheetCopyFunction();

        static unique_ptr<FunctionData> GSheetWriteBind(ClientContext &context, CopyFunctionBindInput &input, const vector<string> &names, const vector<LogicalType> &sql_types);

        static unique_ptr<GlobalFunctionData> GSheetWriteInitializeGlobal(ClientContext &context, FunctionData &bind_data, const string &file_path);

        static unique_ptr<LocalFunctionData> GSheetWriteInitializeLocal(ExecutionContext &context, FunctionData &bind_data_p);

        static void GSheetWriteSink(ExecutionContext &context, FunctionData &bind_data_p, GlobalFunctionData &gstate, LocalFunctionData &lstate, DataChunk &input);
    };

} // namespace duckdb