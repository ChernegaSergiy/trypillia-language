#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <map>
#include <string>

class MemoryManager {
    struct Allocation {
        void* address;
        size_t size;
    };
    
    std::unordered_map<std::string, Allocation> allocations;
    
public:
    void* allocate(const std::string& name, size_t size) {
        void* addr = malloc(size);
        allocations[name] = {addr, size};
        return addr;
    }
    
    void deallocate(const std::string& name) {
        free(allocations[name].address);
        allocations.erase(name);
    }
    
    ~MemoryManager() {
        for (auto& alloc : allocations) {
            free(alloc.second.address);
        }
    }
};

#endif // MEMORY_MANAGER_H
