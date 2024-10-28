#include "duckdb/common/ipc/shared_memory_manager.hpp"

namespace duckdb {
namespace imbridge {

SharedMemoryManager::SharedMemoryManager(const std::string &name, ProcessKind kind, const size_t size)
    : channel_name(name), kind(kind), size(size) {
	if (kind == ProcessKind::CLIENT || kind == ProcessKind::MANAGER) {
		segment = bi::managed_shared_memory(bi::open_or_create, channel_name.c_str(), size);
	} else {
		segment = bi::managed_shared_memory(bi::open_only, channel_name.c_str());
	}
	sem_client = segment.find_or_construct<bi::interprocess_semaphore>((channel_name + "client").c_str())(0);
	sem_server = segment.find_or_construct<bi::interprocess_semaphore>((channel_name + "server").c_str())(0);
	alive = segment.find_or_construct<bool>((channel_name + "alive").c_str())(true);
}

template <typename T>
T *SharedMemoryManager::create_shared_memory_object(const std::string &name, const size_t size) {
	return segment.construct<T>((channel_name + name).c_str())[size]();
}

template <typename T>
std::pair<T *, size_t> SharedMemoryManager::open_shared_memory_object(const std::string &name) {
	return segment.find<T>((channel_name + name).c_str());
}

template <typename T>
void SharedMemoryManager::destroy_shared_memory_object(const std::string &name) {
	if (kind != ProcessKind::CLIENT) {
		throw std::runtime_error(
		    "[Shared Memory] destroy_error! It can destroy shared memory object when process kind is CLIENT");
		return;
	}
	segment.destroy<T>((channel_name + name).c_str());
}

template <typename T>
T* SharedMemoryManager::find_or_construct_shared_memory_object(const std::string &name, T default_data){
	return segment.find_or_construct<T>((channel_name+name).c_str())(default_data);
}

template char* SharedMemoryManager::create_shared_memory_object<char>(const std::string &name, size_t size);
template std::pair<char*, size_t> SharedMemoryManager::open_shared_memory_object<char>(const std::string &name);
template void SharedMemoryManager::destroy_shared_memory_object<char>(const std::string &name);
template int* SharedMemoryManager::find_or_construct_shared_memory_object<int>(const std::string &name, int default_data);
} // namespace imbridge
} // namespace duckdb