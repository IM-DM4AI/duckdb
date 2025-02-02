//===----------------------------------------------------------------------===//
//                         DuckDB
//
// imlane/ipc/shared_memory_manager.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <string>
#include <thread>

namespace bi = boost::interprocess;

namespace duckdb {
namespace imbridge {

enum class ProcessKind : u_int8_t { CLIENT = 0, SERVER = 1, MANAGER = 2 };

class SharedMemoryManager {
public:
	SharedMemoryManager(const std::string &name, ProcessKind process_kind, const size_t size = 1024 * 1024 * 32);
	~SharedMemoryManager() {
		if (kind == ProcessKind::MANAGER) {
			bi::shared_memory_object::remove(channel_name.c_str());
		}
	}

	template <typename T>
	T *create_shared_memory_object(const std::string &name, const size_t size);

	template <typename T>
	std::pair<T *, size_t> open_shared_memory_object(const std::string &name);

	template <typename T>
	void destroy_shared_memory_object(const std::string &name);

	std::string get_channel_name() {
		return channel_name;
	}


	bi::interprocess_semaphore *sem_client;
	bi::interprocess_semaphore *sem_server;

private:
	bi::managed_shared_memory segment;
	std::string channel_name; // will be the threads id
	ProcessKind kind;         // client or server
	size_t size;              // size of the shared memory
};

} // namespace imbridge
} // namespace duckdb
