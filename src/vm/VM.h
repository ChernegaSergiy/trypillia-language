#ifndef TRYPILLIA_VM_H
#define TRYPILLIA_VM_H

#include "Chunk.h"
#include "JIT.h"
#include <string>
#include <unordered_map>
#include <vector>

enum class InterpretResult { INTERPRET_OK, INTERPRET_COMPILE_ERROR, INTERPRET_RUNTIME_ERROR };

struct CallFrame {
    ObjClosure *closure;
    uint8_t *ip;
    int stackStart;
};

class VM;
extern thread_local VM *currentVM;

class VM {
  public:
    std::vector<CallFrame> frames;
    Obj *objects = nullptr;
    size_t bytesAllocated = 0;
    size_t nextGC = 1024 * 1024;
    std::vector<VMValue> stack;
    std::unordered_map<std::string, VMValue> globals;
    ObjUpvalue *openUpvalues;

  private:
    void resetStack();
    void push(VMValue value);
    VMValue pop();
    VMValue peek(int distance);

    InterpretResult runtimeError(const std::string &message);
    InterpretResult run(int targetFrameDepth = 0);

    bool executeCall(uint8_t argCount);
    bool executePropertyGet(const std::string &name);
    bool executeIndexGet();

  public:
    ObjUpvalue *captureUpvalue(VMValue *local);
    void closeUpvalues(VMValue *last);

    void defineNative(const std::string &name, int arity, NativeFn function);
    VMValue callClosure(VMValue closureVal, int argCount, VMValue *args);

    VM();
    ~VM();

    ObjClosure *jitClosure = nullptr;
    JITCompiler jit;
    std::unordered_map<void *, JitFunc> compiledFuncs;

    VMValue instantiateClass(VMValue classVal, int argCount, VMValue *args);

    InterpretResult interpret(ObjFunction *function);
};

#endif // TRYPILLIA_VM_H
