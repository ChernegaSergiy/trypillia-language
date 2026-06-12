#ifndef TRYPILLIA_VM_H
#define TRYPILLIA_VM_H

#include "Chunk.h"
#include "JIT.h"
#include <string>
#include <unordered_map>
#include <vector>

enum class InterpretResult { INTERPRET_OK, INTERPRET_COMPILE_ERROR, INTERPRET_RUNTIME_ERROR };

struct CallFrame {
    std::shared_ptr<ObjClosure> closure;
    uint8_t *ip;
    int stackStart;
};

class VM {
  private:
    std::vector<CallFrame> frames;
    std::vector<VMValue> stack;
    std::unordered_map<std::string, VMValue> globals_private_removed;
    std::shared_ptr<ObjUpvalue> openUpvalues;

    void resetStack();
    void push(VMValue value);
    VMValue pop();
    VMValue peek(int distance);

    std::shared_ptr<ObjUpvalue> captureUpvalue(VMValue *local);
    void closeUpvalues(VMValue *last);

    InterpretResult runtimeError(const std::string &message);
    InterpretResult run(int targetFrameDepth = 0);

  public:
    std::unordered_map<std::string, VMValue> globals;
    void defineNative(const std::string &name, int arity, NativeFn function);
    VMValue callClosure(VMValue closureVal, int argCount, VMValue *args);

    VM();
    ~VM();

    JITCompiler jit;
    std::unordered_map<void*, JitFunc> compiledFuncs;

    InterpretResult interpret(std::shared_ptr<ObjFunction> function);
};

#endif // TRYPILLIA_VM_H
