//===----------------------------------------------------------------------===//
//                         DuckDB
//
// imlane/scheduler/scheduler.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once
#include "duckdb/common/arrow/arrow_transform_util.hpp"
#include "duckdb/main/client_context.hpp"

#include <boost/interprocess/containers/deque.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <iostream>
#include <memory>
#include <thread>
#include <unordered_map>

namespace bi = boost::interprocess;

namespace duckdb {
namespace imbridge {

// use for udf server execute
const int TASK_UDF_INFER = 106;
const int TASK_DESTROY = 107;
const int TASK_RESET_CACHE = 108;

// config
const int AVALIABLE_QUEUE_MESSAGE_MAX_SIZE = 256;
const int TASK_QUEUE_MESSAGE_MAX_SIZE = 8;
const int SHARED_MEMORY_SIZE = 1024 * 1024;

const std::string PRE_FIX = "imlane_";
const std::string TASK_QUEUE_NAME = PRE_FIX + "task_queue";
const std::string AVALIABLE_QUEUE_NAME = PRE_FIX + "avaliable_queue";

const std::string START_SERVER_COMMAND = "/root/workspace/duckdb/examples/embedded-c++/imbridge/server_start.sh  ";
const int BASE_ID = 100000;

class IMLaneScheduler {
public:
	IMLaneScheduler(bool is_manager = false, int sys_core = 0, int lane_id = -100,
	                const size_t size = SHARED_MEMORY_SIZE)
	    : is_manager(is_manager), avaliable_queue(bi::open_or_create, AVALIABLE_QUEUE_NAME.c_str(),
	                                              AVALIABLE_QUEUE_MESSAGE_MAX_SIZE, sizeof(int)) {
		if (is_manager) {
			sys_cpu_core_nums = sys_core;
		} else {
			if (lane_id < 0) {
				throw std::runtime_error("[Server] lane id is not valid");
			} else {
				lane_id_str = TASK_QUEUE_NAME + std::to_string(lane_id);
				task_queue = std::make_unique<bi::message_queue>(bi::open_or_create, lane_id_str.c_str(),
				                                                 TASK_QUEUE_MESSAGE_MAX_SIZE, sizeof(int));
			}
		}
	}

	~IMLaneScheduler() {
		destroy();
	}

	void launch() {
		if (is_manager) {
			for (int i = BASE_ID; i < BASE_ID + sys_cpu_core_nums; i++) {
				std::string lane_id = std::to_string(i);
				auto tq = std::make_unique<bi::message_queue>(bi::open_or_create, (TASK_QUEUE_NAME + lane_id).c_str(),
				                                              TASK_QUEUE_MESSAGE_MAX_SIZE, sizeof(int));
				all_task_queues[i] = std::move(tq);
				std::string cmd = START_SERVER_COMMAND + lane_id;
				std::thread t([cmd]() { std::system(cmd.c_str()); });
				t.detach();
			}
			std::cout << "[Client] launch server success" << std::endl;
			// std::this_thread::sleep_for(std::chrono::seconds(7));
		}
	}

	void destroy() {
		if (is_manager) {
			for (auto &[id, tq] : all_task_queues) {
				int destroy_command = TASK_DESTROY;
				tq->send(&destroy_command, sizeof(int), 0);
			}
			all_task_queues.clear();
			bi::message_queue::remove(AVALIABLE_QUEUE_NAME.c_str());
		} else {
			bi::message_queue::remove(lane_id_str.c_str());
		}
	}

	void reset_cache() {
		if (is_manager) {
			int reset_cache = TASK_RESET_CACHE;
			for (auto &[id, tq] : all_task_queues) {
				tq->send(&reset_cache, sizeof(int), 0);
			}
		}
	}

	void push_id_to_avaliable_queue(int id) {
		avaliable_queue.send(&id, sizeof(int), 0);
	}

	void push_cmd_to_task_queue(int shm_id, int cmd) {
		all_task_queues[shm_id]->send(&cmd, sizeof(int), 0);
	}

	int get_message_from_task_queue() {
		int msg;
		bi::message_queue::size_type recvd_size;
		unsigned int priority;
		task_queue.get()->receive(&msg, sizeof(int), recvd_size, priority);
		return msg;
	}

	int get_id_from_avaliable_queue() {
		int msg;
		bi::message_queue::size_type recvd_size;
		unsigned int priority;
		avaliable_queue.receive(&msg, sizeof(int), recvd_size, priority);
		return msg;
	}

	void schedule_udf(DataChunk &data, Vector &result, const ClientProperties &options) {
		auto table = imbridge::ConvertDataChunkToArrowTable(data, options);
		int id = get_id_from_avaliable_queue();
		if (id < BASE_ID) {
			std::cout << id << std::endl;
			throw std::runtime_error("[Client] id is not valid");
		}
		// write in shared_memory
		imbridge::SharedMemoryManager shm(std::to_string(id), imbridge::ProcessKind::CLIENT);
		imbridge::WriteArrowTableToSharedMemory(table, shm, imbridge::INPUT_TABLE);
		// schedule
		push_cmd_to_task_queue(id, TASK_UDF_INFER);
		shm.sem_client->wait();
		// read from shared_memory
		auto my_table = imbridge::ReadArrowTableFromSharedMemory(shm, imbridge::OUTPUT_TABLE);
		// int cols = my_table->num_columns(), rows = my_table->num_rows();
		// std::vector<int64_t> res;
		// for (int i = 0; i < my_table->num_columns(); i++) {
		// 	std::shared_ptr<arrow::ChunkedArray> column = my_table->column(i);
		// 	for (int chunk_idx = 0; chunk_idx < column->num_chunks(); chunk_idx++) {
		// 		auto chunk = std::static_pointer_cast<arrow::Int64Array>(column->chunk(chunk_idx));
		// 		for (int j = 0; j < chunk->length(); j++) {
		// 			res.push_back(chunk->Value(j));
		// 		}
		// 	}
		// }
		imbridge::ConvertArrowTableResultToVector(my_table, result);
		shm.destroy_shared_memory_object<char>(INPUT_TABLE);
		shm.destroy_shared_memory_object<char>(OUTPUT_TABLE);
		shm.sem_server->post();
	}

private:
	int sys_cpu_core_nums = 0;
	bool is_manager;
	bi::message_queue avaliable_queue;
	// only manager use
	std::unordered_map<int, std::unique_ptr<bi::message_queue>> all_task_queues;
	// only not manager use
	std::unique_ptr<bi::message_queue> task_queue; 
	std::string lane_id_str; 
};

} // namespace imbridge
} // namespace duckdb