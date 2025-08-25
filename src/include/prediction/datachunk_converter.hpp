#pragma once

#include <memory>

#include "dbend/c/table_converter.hpp"

#include "duckdb/main/client_properties.hpp"
#include "duckdb/common/types/data_chunk.hpp"

#include "duckdb/common/arrow/arrow_appender.hpp"
#include "duckdb/common/arrow/arrow_converter.hpp"
#include "duckdb/common/exception/conversion_exception.hpp"
#include "duckdb/common/types/arrow_string_view_type.hpp"

namespace duckdb {
    void ConverArrowAarrayToVector(	ArrowArray *c_array, ArrowSchema *c_array_type,  Vector &res){
        idx_t size = c_array->length;
        std::string ctype(c_array_type->format);
        switch (res.GetType().id()) {
            case LogicalTypeId::BOOLEAN:
            case LogicalTypeId::TINYINT:
            case LogicalTypeId::SMALLINT:
            case LogicalTypeId::INTEGER:
            case LogicalTypeId::FLOAT:
            case LogicalTypeId::DOUBLE:
            case LogicalTypeId::UTINYINT:
            case LogicalTypeId::USMALLINT:
            case LogicalTypeId::UINTEGER:
            case LogicalTypeId::UBIGINT:
            case LogicalTypeId::BIGINT:
            case LogicalTypeId::HUGEINT:
            case LogicalTypeId::UHUGEINT:
            case LogicalTypeId::TIMESTAMP:
            case LogicalTypeId::TIMESTAMP_SEC:
            case LogicalTypeId::TIMESTAMP_MS:
            case LogicalTypeId::TIMESTAMP_NS: {
                auto data_ptr = (data_ptr_t)c_array->buffers[1];
                FlatVector::SetData(res, data_ptr);
                break;
            }
            case LogicalTypeId::VARCHAR: {
                if (ctype == "u" || ctype == "z") { // NORMAL FIXED
                    auto c_data = (char *)c_array->buffers[2];
                    auto offsets = (uint32_t *)c_array->buffers[1];
                    auto strings = FlatVector::GetData<string_t>(res);
                    for (idx_t row_idx = 0; row_idx < size; row_idx++) {
                        if (FlatVector::IsNull(res, row_idx)) {
                            continue;
                        }
                        auto cptr = c_data + offsets[row_idx];
                        auto str_len = offsets[row_idx + 1] - offsets[row_idx];
                        if (str_len > NumericLimits<uint32_t>::Maximum()) { // LCOV_EXCL_START
                            throw duckdb::ConversionException("DuckDB does not support Strings over 4GB");
                        } // LCOV_EXCL_STOP
                        strings[row_idx] = string_t(cptr, UnsafeNumericCast<uint32_t>(str_len));
                    }
                } else if (ctype == "U" || ctype == "Z") { // SUPER
                    auto c_data = (char *)c_array->buffers[2];
                    auto offsets = (uint64_t *)c_array->buffers[1];
                    auto strings = FlatVector::GetData<string_t>(res);
                    for (idx_t row_idx = 0; row_idx < size; row_idx++) {
                        if (FlatVector::IsNull(res, row_idx)) {
                            continue;
                        }
                        auto cptr = c_data + offsets[row_idx];
                        auto str_len = offsets[row_idx + 1] - offsets[row_idx];
                        if (str_len > NumericLimits<uint32_t>::Maximum()) { // LCOV_EXCL_START
                            throw duckdb::ConversionException("DuckDB does not support Strings over 4GB");
                        } // LCOV_EXCL_STOP
                        strings[row_idx] = string_t(cptr, UnsafeNumericCast<uint32_t>(str_len));
                    }
                } else if (ctype == "vu") { // VIEW
                    auto strings = FlatVector::GetData<string_t>(res);
                    auto arrow_string = (arrow_string_view_t *)c_array->buffers[1];
                    for (idx_t row_idx = 0; row_idx < size; row_idx++) {
                        if (FlatVector::IsNull(res, row_idx)) {
                            continue;
                        }
                        auto length = UnsafeNumericCast<uint32_t>(arrow_string[row_idx].Length());
                        if (arrow_string[row_idx].IsInline()) {
                            strings[row_idx] = string_t(arrow_string[row_idx].GetInlineData(), length);
                        } else {
                            auto buffer_index = UnsafeNumericCast<uint32_t>(arrow_string[row_idx].GetBufferIndex());
                            int32_t offset = arrow_string[row_idx].GetOffset();
                            D_ASSERT(c_array->n_buffers > 2 + buffer_index);
                            auto c_data = (char *)c_array->buffers[2 + buffer_index];
                            strings[row_idx] = string_t(&c_data[offset], length);
                        }
                    }
                } else {
                    throw duckdb::ConversionException("Unsupported Arrow String format: %s", c_array_type->format);
                }
    
                break;
            }
            default:
                throw NotImplementedException("Unsupported type for arrow conversion: %s", res.GetType().ToString());
        }
    }
}

namespace IMLane {

    namespace DBEnd {

        template<>
        class TableConverter<duckdb::DataChunk, duckdb::Vector> {
        public:
            std::shared_ptr<ArrowLaneTable> ConvertToExchange(duckdb::DataChunk* tbl) {
                duckdb::ClientProperties default_props;
                auto types = tbl->GetTypes();
                duckdb::vector<std::string> names;
                names.reserve(types.size());
            
                for (idx_t i = 0; i < types.size(); i++) {
                    names.push_back(duckdb::StringUtil::Format("c%d", i));
                }
            
                ArrowSchema schema;
                duckdb::ArrowConverter::ToArrowSchema(&schema, types, names, default_props);

                auto lane_table = std::shared_ptr<ArrowLaneTable>(new ArrowLaneTable(schema));

                idx_t init_capacity = tbl->size() > STANDARD_VECTOR_SIZE ? duckdb::NextPowerOfTwo(tbl->size()) : STANDARD_VECTOR_SIZE;
                duckdb::ArrowAppender appender(types, init_capacity, default_props);
                appender.Append(*tbl, 0, tbl->size(), tbl->size());
                ArrowArray array = appender.Finalize();
                lane_table->AppendChunk(array);
                return lane_table;

            }

            void ConvertFromExchange(std::shared_ptr<ArrowLaneTable> tbl, duckdb::Vector &out) {
                auto &schema = tbl->schema;
                D_ASSERT(tbl->GetNumChunks() == 1);
                duckdb::ConverArrowAarrayToVector(&tbl->chunks[0], &schema, out);
            }
        };
    }
}