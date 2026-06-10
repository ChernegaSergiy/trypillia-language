#ifndef TRYPILLIA_VM_H
#define TRYPILLIA_VM_H

#include "Chunk.h"
#include <vector>
#include <unordered_map>
#include <string>

enum class InterpretResult {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
};

class VM {
private:
    Chunk* chunk;
    uint8_t* ip;
    std::vector<VMValue> stack;
    std::unordered_map<std::string, VMValue> globals;

    void resetStack();
    void push(VMValue value);
    VMValue pop();
    VMValue peek(int distance);
    
    InterpretResult run();

public:
    VM();
    ~VM();

    InterpretResult interpret(Chunk* chunk);
};

#endif // TRYPILLIA_VM_H
