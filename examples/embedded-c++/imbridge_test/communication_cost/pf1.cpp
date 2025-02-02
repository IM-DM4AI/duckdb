
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
	con.CreateVectorizedFunction<int64_t, double, double, double, double, double, double, double, double, string_t,
	                             string_t, int64_t, bool, int64_t, int64_t, string_t, string_t, string_t, string_t,
	                             string_t, string_t, string_t, int64_t, int64_t, int64_t, int64_t, int64_t, bool, bool>(
	    "udf", &udf_tmp, LogicalType::INVALID, FunctionKind::BATCH_PREDICTION, 4096);
	con.Query("SET threads TO 1;");
	string sql1 = R"(
explain analyze SELECT udf(prop_location_score1, prop_location_score2, prop_log_historical_price, price_usd,
                           orig_destination_distance, prop_review_score, avg_bookings_usd, stdev_bookings_usd,
                           position, prop_country_id, prop_starrating, prop_brand_bool, count_clicks, count_bookings,
                           year, month, weekofyear, time, site_id, visitor_location_country_id, srch_destination_id,
                           srch_length_of_stay, srch_booking_window, srch_adults_count, srch_children_count,
                           srch_room_count, srch_saturday_night_bool, random_bool) 
FROM pf1_used_data;
)";
	string sql2 = R"(
explain analyze SELECT srch_room_count
FROM pf1_used_data;
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