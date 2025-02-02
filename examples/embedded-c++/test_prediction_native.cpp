#include "duckdb.hpp"

#include <iostream>
#include <string>

using namespace duckdb;
using namespace imbridge;

template <typename TYPE, int NUM_INPUT>
static void udf_tmp(DataChunk &input, ExpressionState &state, Vector &result) {
	result.SetVectorType(VectorType::FLAT_VECTOR);
	auto result_data = ConstantVector::GetData<TYPE>(result);
	input.Flatten();
	auto tmp_data1 = ConstantVector::GetData<TYPE>(input.data[0]);
	auto tmp_data2 = ConstantVector::GetData<TYPE>(input.data[1]);
	memset(result_data, std::numeric_limits<TYPE>::min(), input.size() * sizeof(TYPE));
	std::cout<<input.size()<<std::endl;
	for (idx_t i = 0; i < input.size(); i++) {
		result_data[i] = 1 * tmp_data1[i] + 0 * tmp_data2[i];
	}
}

void create_data(Connection &con, int n = 10000) {
	std::stringstream ss;
	ss << "INSERT INTO data VALUES (1, 10)";
	for (int i = 2; i <= n; i++) {
		ss << ", (";
		ss << i;
		ss << ", ";
		ss << i * 10;
		ss << ")";
	}
	con.Query(ss.str());
	printf("Finish create!\n");
}

int main() {
	DuckDB db(nullptr);
	Connection con(db);
	con.Query("CREATE TABLE data (i DOUBLE, age DOUBLE)");
	create_data(con);
	con.Query("SET threads = 1");
	// con.Query("SELECT * FROM data LIMIT 10")->Print();
	con.CreateVectorizedFunction<double, double, double>("udf_vectorized_int", &udf_tmp<double, 2>, LogicalType::INVALID, FunctionKind::BATCH_PREDICTION, 0);
	clock_t start_time=clock();
	con.Query("SELECT i, udf_vectorized_int(i, age) FROM data")->Print();
	clock_t end_time=clock();
	printf("finished execute %lf s!\n",(double)(end_time - start_time) / CLOCKS_PER_SEC);
	// con.Query("SELECT i FROM data WHERE i%2==0")->Print();
	return 0;
}