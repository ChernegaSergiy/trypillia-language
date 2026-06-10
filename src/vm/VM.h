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

struct CallFrame {
    std::shared_ptr<ObjFunction> function;
    uint8_t* ip;
    int stackStart;
};

class VM {
private:
    std::vector<CallFrame> frames;
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

    InterpretResult interpret(std::shared_ptr<ObjFunction> function);
};

#endif // TRYPILLIA_VM_H
