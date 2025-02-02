#include "imlane/ipc/shared_memory_manager.hpp"

namespace duckdb {
namespace imbridge {

SharedMemoryManager::SharedMemoryManager(const std::string &name, ProcessKind kind, const size_t size)
    : channel_name(name), kind(kind), size(size) {
	segment = bi::managed_shared_memory(bi::open_or_create, channel_name.c_str(), size);
	sem_client = segment.find_or_construct<bi::interprocess_semaphore>((channel_name + "client").c_str())(0);
	sem_server = segment.find_or_construct<bi::interprocess_semaphore>((channel_name + "server").c_str())(0);
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
	segment.destroy<T>((channel_name + name).c_str());
}

template char* SharedMemoryManager::create_shared_memory_object<char>(const std::string &name, size_t size);
template std::pair<char*, size_t> SharedMemoryManager::open_shared_memory_object<char>(const std::string &name);
template void SharedMemoryManager::destroy_shared_memory_object<char>(const std::string &name);
} // namespace imbridge
} // namespace duckdb