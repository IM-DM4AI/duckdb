
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
	memset(result_data, std::numeric_limits<int64_t>::min(), input.size() * sizeof(int64_t));
	for (idx_t i = 0; i < input.size(); i++) {
		result_data[i] = (int)tmp_data1[i];
	}
}

int main() {
	DuckDB db("/root/workspace/duckdb/examples/embedded-c++/imbridge_test/db/db_tpcx_ai_sf40.db");
	Connection con(db);
	con.CreateVectorizedFunction<int64_t, double, double>("udf", &udf_tmp, LogicalType::INVALID,
	                                                      FunctionKind::SCHEDULE_PREDICTION, 4096);

	string sql = R"(
explain analyze select ratio_tbl.o_customer_sk, udf(COALESCE(return_ratio,0), COALESCE(frequency,0))
from (select o_customer_sk, mean(ratio) return_ratio 
from (select o_customer_sk, return_row_price_sum/row_price_sum ratio
from (select o_order_id, o_customer_sk, SUM(row_price) row_price_sum, SUM(return_row_price) return_row_price_sum, SUM(invoice_year) invoice_year_min 
from (select o_order_id, o_customer_sk, extract('year' FROM cast(date0 as DATE)) invoice_year, quantity*price row_price, or_return_quantity*price return_row_price 
from (select o_order_id, o_customer_sk, date date0, li_product_id, price, quantity, or_return_quantity  
from (select * from lineitem left join Order_Returns 
on lineitem.li_order_id = Order_Returns.or_order_id 
and lineitem.li_product_id = Order_Returns.or_product_id
) returns_data Join Order_o on returns_data.li_order_id=Order_o.o_order_id)) 
group by o_order_id, o_customer_sk)
)
group by o_customer_sk
) ratio_tbl 
join (select o_customer_sk, mean(o_order_id) frequency
from (select o_customer_sk, invoice_year_min, count(DISTINCT o_order_id) o_order_id
from (select o_order_id, o_customer_sk, SUM(row_price) row_price_sum, SUM(return_row_price) return_row_price_sum, SUM(invoice_year) invoice_year_min 
from (select o_order_id, o_customer_sk, extract('year' FROM cast(date0 as DATE)) invoice_year, quantity*price row_price, or_return_quantity*price return_row_price 
from (select o_order_id, o_customer_sk, date date0, li_product_id, price, quantity, or_return_quantity  
from (select * from lineitem left join Order_Returns 
on lineitem.li_order_id = Order_Returns.or_order_id 
and lineitem.li_product_id = Order_Returns.or_product_id
) returns_data Join Order_o on returns_data.li_order_id=Order_o.o_order_id)) 
group by o_order_id, o_customer_sk
)
group by o_customer_sk, invoice_year_min
) group by o_customer_sk
) frequency_tbl on ratio_tbl.o_customer_sk = frequency_tbl.o_customer_sk;
)";
	int times = 5;
	double result = 0;
	double min1, max1;
	bool flag = true;
	for (int i = 0; i < times; i++) {
		auto start_time = std::chrono::high_resolution_clock::now();
		con.Query(sql);
		auto end_time = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		double t = duration / 1e6;
		printf("%d : %lf\n", i + 1, t);
		result += t;
		if (flag) {
			flag = false;
			min1 = t;
			max1 = t;
		} else {
			min1 = std::min(min1, t);
			max1 = std::max(max1, t);
		}
		con.IMLaneResetCache();
	}
	printf("min : %lf\n", min1);
	printf("max : %lf\n", max1);
	result = result - min1 - max1;
	times = times - 2;
	printf("finished execute %lf s!\n", result / (times * 1.0));
	return 0;
}