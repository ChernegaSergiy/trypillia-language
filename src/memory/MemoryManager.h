#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <unordered_map>
#include <string>

class MemoryManager {
    struct Allocation {
        void* address;
        size_t size;
    };

    std::unordered_map<std::string, Allocation> allocations;

public:
    void* allocate(const std::string& name, size_t size);
    void deallocate(const std::string& name);
    ~MemoryManager();
};

#endif // MEMORY_MANAGER_H
