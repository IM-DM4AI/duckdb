
#include "duckdb.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace duckdb;
using namespace imbridge;

static void udf_tmp(DataChunk &input, ExpressionState &state, Vector &result) {
	using TYPE = double;
	result.SetVectorType(VectorType::FLAT_VECTOR);
	auto result_data = ConstantVector::GetData<TYPE>(result);
	input.Flatten();
	auto tmp_data1 = ConstantVector::GetData<TYPE>(input.data[0]);
	auto tmp_data2 = ConstantVector::GetData<TYPE>(input.data[1]);
	memset(result_data, std::numeric_limits<TYPE>::min(), input.size() * sizeof(TYPE));
	for (idx_t i = 0; i < input.size(); i++) {
		result_data[i] = 1 * tmp_data1[i] + 1 * tmp_data2[i];
	}
}

int main(int argc, char **argv) {
	if (argc < 2) {
		std::cout << "[Server] you shoule add a parameter\n";
		return 0;
	}
	std::string namesss = argv[1];
	DuckDB db("/root/workspace/duckdb/examples/embedded-c++/imbridge_test/db/db_raven.db");
	Connection con(db);
	con.CreateVectorizedFunction<double, double, double, double, double, double, double, double, double, double,
	                             double, double, double, double, double, double, double, double, double, double, double,
	                             double, double, double, double, double, double, double, double, double>(
	    "udf", &udf_tmp, LogicalType::INVALID, FunctionKind::BATCH_PREDICTION, 4096);
	con.Query("SET threads TO 1;");
	string sql1 = R"(
explain analyze SELECT udf(V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12, V13, V14, V15,
 V16, V17, V18, V19, V20, V21, V22, V23, V24, V25, V26, V27, V28, Amount)
FROM pf7_used_data;
)";
	string sql2 = R"(
explain analyze SELECT V1
FROM pf7_used_data;
)";
	double t;
	if (namesss == "udf") {
		clock_t start_time = clock();
		con.Query(sql1);
		clock_t end_time = clock();
		t = (double)(end_time - start_time) / CLOCKS_PER_SEC;
		std::cout << "udf : " << t << std::endl;
	} else {
		clock_t start_time = clock();
		con.Query(sql2);
		clock_t end_time = clock();
		t = (double)(end_time - start_time) / CLOCKS_PER_SEC;
		std::cout << "origin : " << t << std::endl;
	}
	
	return 0;
}