
#include "duckdb.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
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

void bs_process(Connection &con, int dbt, int mlt, int max_bs = 6200, int now_bs = 256, int base_bs = 768) {
	while (max_bs > now_bs) {
		std::stringstream ss;
		ss << "udf" << now_bs;
		std::string udf_name = ss.str();
		con.CreateVectorizedFunction<int64_t, double, double, double, double, int64_t, string_t, string_t, string_t,
		                             string_t, string_t, string_t, int64_t, string_t, string_t, string_t, int64_t,
		                             string_t>(udf_name, &udf_tmp, LogicalType::INVALID, FunctionKind::BATCH_PREDICTION,
		                                       now_bs);
		ss.str("");
		ss.clear();
		ss << R"(
Explain analyze SELECT Flights_S_routes_extension.airlineid, Flights_S_routes_extension.sairportid, Flights_S_routes_extension.dairportid,)"
		   << udf_name << R"((slatitude, slongitude, dlatitude, dlongitude, name1, name2, name4, acountry, active, 
                        scity, scountry, stimezone, sdst, dcity, dcountry, dtimezone, ddst) AS codeshare 
                        FROM Flights_S_routes_extension JOIN Flights_R1_airlines ON Flights_S_routes_extension.airlineid = Flights_R1_airlines.airlineid 
                        JOIN Flights_R2_sairports ON Flights_S_routes_extension.sairportid = Flights_R2_sairports.sairportid JOIN Flights_R3_dairports 
                        ON Flights_S_routes_extension.dairportid = Flights_R3_dairports.dairportid
where slatitude > 26 and dlatitude > 30 and slatitude < 40 and dlatitude < 40
)";
		int times = 5;
		double result = 0;
		double min1, max1;
		bool flag = true;
		for (int i = 0; i < times; i++) {
			auto start_time = std::chrono::high_resolution_clock::now();
			con.Query(ss.str());
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
		}
		printf("min : %lf\n", min1);
		printf("max : %lf\n", max1);
		result = result - min1 - max1;
		times = times - 2;
		printf("db threads: %d, ml threads: %d, bs: %d, finished execute %lf s!\n", dbt, mlt, now_bs,
		       result / (times * 1.0));
		now_bs += base_bs;
	}
}

int main(int argc, char **argv) {
	if (argc < 3) {
		std::cout << "[Server] you shoule add two parameter\n";
		return 0;
	}
	std::string db_threads = argv[1];
	std::string ml_threads = argv[2];
	int now_bs = argc > 3 ? std::stoi(argv[3]) : 256;
	int max_bs = argc > 4 ? std::stoi(argv[4]) : 6200;
	int base_bs = argc > 5 ? std::stoi(argv[5]) : 768;
	
	// set ml threads
	std::ofstream file("/root/workspace/duckdb/.vscode/experiment_cfg/ml_thread.cfg");
	if (file.is_open()) {
		file << ml_threads;
		file.close();
	} else {
		std::cout << "count not open ml file!" << std::endl;
		exit(0);
	}
	// std::cout << "ml threads: " << ml_threads << std::endl;
	DuckDB db("/root/workspace/duckdb/examples/embedded-c++/imbridge_test/db/db_raven_10G.db");
	Connection con(db);
	std::stringstream ss;
	// set db threads
	ss << "SET threads TO " << db_threads << ";";
	con.Query(ss.str());
	// set udf
	bs_process(con, std::stoi(db_threads), std::stoi(ml_threads), max_bs, now_bs, base_bs);
	return 0;
}