//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/ipc/shared_memory_manager.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <sstream>
#include <string>
#include <thread>

namespace bi = boost::interprocess;

namespace duckdb {
namespace imbridge {

enum class ProcessKind : u_int8_t { CLIENT = 0, SERVER = 1, MANAGER = 2 };

const std::string START_SERVER_COMMAND = "/root/workspace/duckdb/examples/embedded-c++/imbridge/server_start.sh  ";

static std::string thread_id_to_string(std::thread::id id) {
	std::ostringstream ss;
	ss << id;
	return ss.str();
}

class SharedMemoryManager {
public:
	SharedMemoryManager(const std::string &name, ProcessKind process_kind, const size_t size = 1024 * 1024 * 32);
	~SharedMemoryManager() {
		if (kind == ProcessKind::MANAGER) {
			close_server();
			sem_server->post();
			sem_client->wait();
			bi::shared_memory_object::remove(channel_name.c_str());
		}
	}

	template <typename T>
	T *create_shared_memory_object(const std::string &name, const size_t size);

	template <typename T>
	std::pair<T *, size_t> open_shared_memory_object(const std::string &name);

	template <typename T>
	void destroy_shared_memory_object(const std::string &name);

	template <typename T>
	T* find_or_construct_shared_memory_object(const std::string &name, T default_data);

	std::string get_channel_name() {
		return channel_name;
	}

	bool is_alive() {
		return *alive;
	}
	void close_server() {
		if (kind != ProcessKind::MANAGER) {
			throw std::runtime_error("[Shared Memory] close_error! It can close server when process kind is MANAGER");
			return;
		}
		*alive = false;
	}

	bi::interprocess_semaphore *sem_client;
	bi::interprocess_semaphore *sem_server;

private:
	bi::managed_shared_memory segment;
	std::string channel_name; // will be the threads id
	ProcessKind kind;         // client or server
	size_t size;              // size of the shared memory
	bool *alive;
};

} // namespace imbridge
} // namespace duckdb
