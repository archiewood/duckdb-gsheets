#pragma once

#include "duckdb.hpp"

namespace duckdb {

enum class GSheetCopyFormat { AUTO = 0, BINARY = 1, TEXT = 2 };

struct GSheetCopyState {
	GSheetCopyFormat format = GSheetCopyFormat::AUTO;
	bool has_null_byte_replacement = false;
	string null_byte_replacement;

	void Initialize(ClientContext &context);
};

} // namespace duckdb
