#include "MemoryManager.h"
#include <cstdlib>

void* MemoryManager::allocate(const std::string& name, size_t size) {
    void* addr = malloc(size);
    if (addr != nullptr) {
        allocations[name] = {addr, size};
    }
    return addr;
}

void MemoryManager::deallocate(const std::string& name) {
    auto it = allocations.find(name);
    if (it != allocations.end()) {
        free(it->second.address);
        allocations.erase(it);
    }
}

MemoryManager::~MemoryManager() {
    for (auto& alloc : allocations) {
        free(alloc.second.address);
    }
}
