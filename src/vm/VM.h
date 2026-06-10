#ifndef TRYPILLIA_VM_H
#define TRYPILLIA_VM_H

#include "Chunk.h"
#include <vector>
#include <unordered_map>
#include <string>

// Результат виконання віртуальної машини
enum class InterpretResult {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
};

class VM {
private:
    Chunk* chunk;
    uint8_t* ip; // Instruction Pointer (вказівник на поточну інструкцію)
    std::vector<VMValue> stack; // Стек для зберігання тимчасових значень
    std::unordered_map<std::string, VMValue> globals;

    void resetStack();
    void push(VMValue value);
    VMValue pop();
    VMValue peek(int distance);
    
    // Головний цикл виконання інструкцій
    InterpretResult run();

public:
    VM();
    ~VM();

    InterpretResult interpret(Chunk* chunk);
};

#endif // TRYPILLIA_VM_H
