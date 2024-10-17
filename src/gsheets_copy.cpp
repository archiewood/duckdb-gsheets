#include "gsheets_copy.hpp"
#include "duckdb/common/serializer/buffered_file_writer.hpp"
#include "duckdb/common/file_system.hpp"
#include <iostream>
#include <json.hpp>

using json = nlohmann::json;

namespace duckdb
{

    GSheetCopyFunction::GSheetCopyFunction() : CopyFunction("gsheet")
    {
        copy_to_bind = GSheetWriteBind;
        copy_to_initialize_global = GSheetWriteInitializeGlobal;
        copy_to_initialize_local = GSheetWriteInitializeLocal;
        copy_to_sink = GSheetWriteSink;
    }

    struct GSheetCopyGlobalState : public GlobalFunctionData
    {
        explicit GSheetCopyGlobalState(ClientContext &context)
        {
        }

    public:
        unique_ptr<BufferedFileWriter> file_writer;
    };

    struct GSheetWriteBindData : public TableFunctionData
    {
    };

    unique_ptr<FunctionData> GSheetCopyFunction::GSheetWriteBind(ClientContext &context, CopyFunctionBindInput &input, const vector<string> &names, const vector<LogicalType> &sql_types)
    {
        return make_uniq<GSheetWriteBindData>();
    }

    unique_ptr<GlobalFunctionData> GSheetCopyFunction::GSheetWriteInitializeGlobal(ClientContext &context, FunctionData &bind_data, const string &file_path)
    {
        auto result = make_uniq<GSheetCopyGlobalState>(context);
        auto &fs = FileSystem::GetFileSystem(context);
        result->file_writer = make_uniq<BufferedFileWriter>(fs, file_path);
        return std::move(result);
    }

    unique_ptr<LocalFunctionData> GSheetCopyFunction::GSheetWriteInitializeLocal(ExecutionContext &context, FunctionData &bind_data_p)
    {
        return make_uniq<LocalFunctionData>();
    }

    void GSheetCopyFunction::GSheetWriteSink(ExecutionContext &context, FunctionData &bind_data_p, GlobalFunctionData &gstate_p, LocalFunctionData &lstate, DataChunk &input)
    {
        input.Flatten();
        // Create object ready to write to Google Sheet
        json sheet_data;
        sheet_data["range"] = "A1"; // Assuming we start from A1, adjust as needed
        sheet_data["majorDimension"] = "ROWS";
        
        vector<vector<string>> values;
        for (idx_t r = 0; r < input.size(); r++)
        {
            vector<string> row;
            for (idx_t c = 0; c < input.ColumnCount(); c++)
            {
                auto &col = input.data[c];
                switch (col.GetType().id()) {
                    case LogicalTypeId::VARCHAR:
                        row.push_back(FlatVector::GetData<string_t>(col)[r].GetString());
                        break;
                    case LogicalTypeId::INTEGER:
                        row.push_back(to_string(FlatVector::GetData<int32_t>(col)[r]));
                        break;
                    case LogicalTypeId::BIGINT:
                        row.push_back(to_string(FlatVector::GetData<int64_t>(col)[r]));
                        break;
                    case LogicalTypeId::DOUBLE:
                        row.push_back(to_string(FlatVector::GetData<double>(col)[r]));
                        break;
                    case LogicalTypeId::BOOLEAN:
                        row.push_back(FlatVector::GetData<bool>(col)[r] ? "TRUE" : "FALSE");
                        break;
                    default:
                        row.push_back("Type not implemented");
                        break;
                }
            }
            values.push_back(row);
        }
        sheet_data["values"] = values;

        std::cout << sheet_data.dump() << std::endl;
    }
} // namespace duckdb