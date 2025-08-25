#include "duckdb.hpp"

#include <iostream>
#include <string>

using namespace duckdb;
using namespace prediction;

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
	DuckDB db("/home/admin0/duckdb/test.db");
	Connection con(db);
	// con.Query("CREATE TABLE data (i DOUBLE, age DOUBLE)");
	// create_data(con);
	// con.Query("CALL dbgen(sf = 1);");
	con.Query("SET threads = 1");
	// con.Query("SELECT * FROM data LIMIT 10")->Print();
	con.CreateVectorizedFunction<double, double, double>("udf_vectorized_int", &udf_tmp<double, 2>, LogicalType::INVALID, FunctionKind::PREDICTION, 4444);
	clock_t start_time=clock();
	con.Query("SELECT c_custkey, c_name, sum(l_extendedprice * (1 - l_discount)) as revenue, c_acctbal, n_name, c_address, c_phone, c_comment from customer, orders, lineitem, nation where c_custkey = o_custkey and l_orderkey = o_orderkey and o_orderdate >= DATE'1993-10-01' and o_orderdate < DATE'1993-10-01' + interval '3' month and c_nationkey = n_nationkey and udf_vectorized_int(l_orderkey, o_custkey) > 0 group by c_custkey, c_name, c_acctbal, c_phone, n_name, c_address, c_comment order by revenue desc;")->Print();
	// con.Query("SELECT i, udf_vectorized_int(i, age) FROM data")->Print();
	clock_t end_time=clock();
	printf("finished execute %lf s!\n",(double)(end_time - start_time) / CLOCKS_PER_SEC);
	// con.Query("SELECT i FROM data WHERE i%2==0")->Print();
	return 0;
}